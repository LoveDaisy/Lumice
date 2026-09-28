#include <cmath>
#include <cstddef>
#include <cstring>
#include <memory>

#include "analytic/analytic_callback_sink.hpp"
#include "analytic/path_evaluation.hpp"
#include "lumice_analytic.h"
#include "util/logger.hpp"

namespace {

// This library's own sink singleton — a separate object from liblumice's GetCallbackSink(), as
// every static here is (each shared library carries its own copy of the engine).
std::shared_ptr<lumice::analytic::AnalyticCallbackSink>& GetAnalyticCallbackSink() {
  static auto sink = std::make_shared<lumice::analytic::AnalyticCallbackSink>();
  return sink;
}

// Removes the default console sink from this library's copy of GetSharedSink(). Nothing references
// this object, and that is the point: its constructor is the one place the library goes silent, and
// deleting it makes every engine warning (crystal.cpp, geo3d_closedform.cpp, ...) print to the
// host's stderr again. It runs during dynamic initialisation of the library, which completes before
// dlopen / LoadLibrary returns, so no LUMICE_ANALYTIC_* call can precede it; the linker keeps it
// because initialiser tables are GC roots under dead-code stripping. On Windows it runs inside
// DllMain under the loader lock; it only allocates and edits a sink list, which is safe there. The
// engine logs nothing at static-initialisation time, so no message can slip out before it runs.
struct SilenceDefaultConsoleSink {
  SilenceDefaultConsoleSink() { lumice::GetSharedSink()->remove_sink(lumice::GetDefaultConsoleSink()); }
};
const SilenceDefaultConsoleSink kSilenceDefaultConsoleSink;

// Zero-fills a caller-allocated result struct after its leading struct_size field, within the bytes
// the caller declared (doc/analytic-api.md section 8.2) — so a caller older than the library keeps
// the memory it did not size, and struct_size itself is never touched.
template <typename T>
void ZeroAfterStructSize(T* out) {
  const size_t declared = out->struct_size;
  const size_t known = sizeof(T);
  const size_t end = declared < known ? declared : known;
  constexpr size_t kBegin = offsetof(T, struct_size) + sizeof(out->struct_size);
  if (end > kBegin) {
    std::memset(reinterpret_cast<unsigned char*>(out) + kBegin, 0, end - kBegin);
  }
}

LUMICE_ANALYTIC_ErrorCode ToErrorCode(lumice::analytic::Status status) {
  switch (status) {
    case lumice::analytic::Status::kOk:
      return LUMICE_ANALYTIC_OK;
    case lumice::analytic::Status::kInvalidValue:
      return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
    case lumice::analytic::Status::kInvalidConfig:
      return LUMICE_ANALYTIC_ERR_INVALID_CONFIG;
  }
  return LUMICE_ANALYTIC_ERR_UNKNOWN;
}

// The block behind PathEvaluation::storage: one allocation holding the segments, then the
// transmittances.
struct PathEvaluationStorage {
  std::unique_ptr<double[]> values;
};

}  // namespace

extern "C" {

int LUMICE_ANALYTIC_GetApiVersion(void) {
  return LUMICE_ANALYTIC_API_VERSION;
}


void LUMICE_ANALYTIC_SetLogCallback(LUMICE_ANALYTIC_LogCallback callback) {
  auto& sink = GetAnalyticCallbackSink();
  sink->SetCallback(callback);

  // Attach the sink on first call. A sink added after Logger::set_formatter does not inherit the
  // logger's formatter, so it gets the engine's pattern here.
  static const bool kRegistered = [&sink] {
    sink->set_formatter(lumice::CreateLumiceFormatter(lumice::kLogPattern));
    lumice::GetSharedSink()->add_sink(sink);
    return true;
  }();
  (void)kRegistered;

  // One line through the engine's own logger, so a host can see its callback is wired up — and so
  // this library's silence is observable at its boundary: the line must reach the callback and
  // never the host's stderr (test/e2e-correctness/test_analytic_log_sink.py).
  if (callback != nullptr) {
    LOG_INFO("liblumice_analytic {}: log callback installed", LUMICE_ANALYTIC_API_VERSION);
  }
}

LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_EvaluatePath(const LUMICE_ANALYTIC_Crystal* crystal, const int* faces,
                                                       int face_count, double refractive_index,
                                                       const double incident_direction[3], const double pose[9],
                                                       LUMICE_ANALYTIC_PathEvaluation* out) {
  namespace an = lumice::analytic;
  if (out == nullptr) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  ZeroAfterStructSize(out);
  if (out->struct_size < sizeof(LUMICE_ANALYTIC_PathEvaluation)) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  if (crystal == nullptr || faces == nullptr || incident_direction == nullptr || pose == nullptr) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  if (!std::isfinite(refractive_index) || refractive_index <= 0.0 || !an::ValidateUnitVector(incident_direction) ||
      !an::ValidateRotation(pose)) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }

  an::FaceNormalTable table;
  if (auto status = an::BuildFaceNormals(*crystal, &table); status != an::Status::kOk) {
    return ToErrorCode(status);
  }
  if (face_count < 2) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  auto slots = std::make_unique<int[]>(static_cast<size_t>(face_count));
  if (auto status = an::ResolveFaceSequence(table, faces, face_count, slots.get()); status != an::Status::kOk) {
    return ToErrorCode(status);
  }

  const size_t segment_values = 3 * (static_cast<size_t>(face_count) + 1);
  auto storage = std::make_unique<PathEvaluationStorage>();
  storage->values = std::make_unique<double[]>(segment_values + static_cast<size_t>(face_count));
  an::PathOutputs outputs{};
  outputs.segment_directions = storage->values.get();
  outputs.interface_transmittances = storage->values.get() + segment_values;
  const bool valid =
      an::EvaluatePath(table, slots.get(), face_count, refractive_index, incident_direction, pose, &outputs);

  out->fresnel_transmission = outputs.fresnel_transmission;
  if (!valid) {
    return LUMICE_ANALYTIC_OK;  // zero-filled already: valid 0, no segments, no storage
  }
  out->valid = 1;
  for (int i = 0; i < 3; i++) {
    out->outgoing_direction[i] = outputs.outgoing_direction[i];
  }
  out->segment_count = face_count + 1;
  out->segment_directions = outputs.segment_directions;
  out->interface_transmittances = outputs.interface_transmittances;
  out->storage = storage.release();
  return LUMICE_ANALYTIC_OK;
}

void LUMICE_ANALYTIC_ReleasePathEvaluation(LUMICE_ANALYTIC_PathEvaluation* eval) {
  if (eval == nullptr || eval->struct_size < sizeof(LUMICE_ANALYTIC_PathEvaluation)) {
    return;
  }
  // Reclaims the block released to the caller by EvaluatePath; destroyed at scope exit.
  std::unique_ptr<PathEvaluationStorage> owned(static_cast<PathEvaluationStorage*>(eval->storage));
  ZeroAfterStructSize(eval);
}

}  // extern "C"
