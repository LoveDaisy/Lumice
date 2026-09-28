#include <cmath>
#include <cstddef>
#include <cstring>
#include <memory>
#include <new>
#include <vector>

#include "analytic/analytic_callback_sink.hpp"
#include "analytic/fiber_continuation.hpp"
#include "analytic/path_evaluation.hpp"
#include "analytic/path_fiber.hpp"
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

// The block behind FiberResult::storage: one allocation of 17 N - 1 doubles — poses (9 N), sun
// directions (3 N), arclength increments (N - 1), residual norms (N), tangents (3 N).
struct FiberResultStorage {
  std::unique_ptr<double[]> values;
};

LUMICE_ANALYTIC_Reason ToReason(lumice::analytic::FiberReason reason) {
  using R = lumice::analytic::FiberReason;
  switch (reason) {
    case R::kClosedLoop:
      return LUMICE_ANALYTIC_REASON_CLOSED_LOOP;
    case R::kTirBoundary:
      return LUMICE_ANALYTIC_REASON_TIR_BOUNDARY;
    case R::kBranchBoundary:
      return LUMICE_ANALYTIC_REASON_BRANCH_BOUNDARY;
    case R::kPathInfeasible:
      return LUMICE_ANALYTIC_REASON_PATH_INFEASIBLE;
    case R::kVisibilityBoundary:
      return LUMICE_ANALYTIC_REASON_VISIBILITY_BOUNDARY;
    case R::kChartBoundary:
      return LUMICE_ANALYTIC_REASON_CHART_BOUNDARY;
    case R::kRankLoss:
      return LUMICE_ANALYTIC_REASON_RANK_LOSS;
    case R::kTopologyAmbiguity:
      return LUMICE_ANALYTIC_REASON_TOPOLOGY_AMBIGUITY;
    case R::kCorrectorFailure:
      return LUMICE_ANALYTIC_REASON_CORRECTOR_FAILURE;
    case R::kLinearSolveFailure:
      return LUMICE_ANALYTIC_REASON_LINEAR_SOLVE_FAILURE;
    case R::kNonFinite:
      return LUMICE_ANALYTIC_REASON_NON_FINITE;
    case R::kStepUnderflow:
      return LUMICE_ANALYTIC_REASON_STEP_UNDERFLOW;
    case R::kInvalidNumericalInput:
      return LUMICE_ANALYTIC_REASON_INVALID_NUMERICAL_INPUT;
    case R::kStepBudget:
      return LUMICE_ANALYTIC_REASON_STEP_BUDGET;
    case R::kArclengthBudget:
      return LUMICE_ANALYTIC_REASON_ARCLENGTH_BUDGET;
    case R::kEvaluationBudget:
      return LUMICE_ANALYTIC_REASON_EVALUATION_BUDGET;
  }
  return LUMICE_ANALYTIC_REASON_UNKNOWN;
}

LUMICE_ANALYTIC_FiberStatus ToStatus(lumice::analytic::FiberStatus status) {
  using S = lumice::analytic::FiberStatus;
  switch (status) {
    case S::kClosed:
      return LUMICE_ANALYTIC_FIBER_CLOSED;
    case S::kEventTerminated:
      return LUMICE_ANALYTIC_FIBER_EVENT_TERMINATED;
    case S::kNumericalFailure:
      return LUMICE_ANALYTIC_FIBER_NUMERICAL_FAILURE;
    case S::kBudgetExhausted:
      return LUMICE_ANALYTIC_FIBER_BUDGET_EXHAUSTED;
  }
  return LUMICE_ANALYTIC_FIBER_NUMERICAL_FAILURE;
}

// The C options onto LI's full option set (doc/analytic-api.md section 4.3): zero is the default,
// negative or non-finite is invalid, and the filled-in set must pass LI's own relations.
bool ToParams(const LUMICE_ANALYTIC_ContinuationOptions* options, lumice::analytic::ContinuationParams* p) {
  *p = lumice::analytic::ContinuationParams{};
  if (options == nullptr) {
    return true;
  }
  auto real = [](double v, double* field) {
    if (!std::isfinite(v) || v < 0.0) {
      return false;
    }
    if (v > 0.0) {
      *field = v;
    }
    return true;
  };
  auto count = [](int v, int* field) {
    if (v < 0) {
      return false;
    }
    if (v > 0) {
      *field = v;
    }
    return true;
  };
  return real(options->seed_residual_tolerance, &p->residual_tolerance) &&
         real(options->step_initial, &p->initial_step) && real(options->step_min, &p->minimum_step) &&
         real(options->step_max, &p->maximum_step) && count(options->max_accepted_steps, &p->maximum_accepted_steps) &&
         count(options->closure_min_steps, &p->closure_minimum_steps) &&
         real(options->closure_pose_tolerance, &p->closure_distance) && lumice::analytic::ValidateParams(*p);
}

// A problem's own inputs (the element-level checks of TraceFiberBatch). On success `slots` holds the
// resolved face sequence.
bool ValidProblem(const lumice::analytic::FaceNormalTable& table, const LUMICE_ANALYTIC_FiberProblem& problem,
                  int* slots) {
  namespace an = lumice::analytic;
  if (problem.face_count < 2 || problem.face_count > an::kMaxFaceCount || problem.faces == nullptr) {
    return false;
  }
  if (!std::isfinite(problem.refractive_index) || problem.refractive_index <= 0.0) {
    return false;
  }
  if (!an::ValidateUnitVector(problem.incident_direction) || !an::ValidateUnitVector(problem.target_direction) ||
      !an::ValidateRotation(problem.seed_pose)) {
    return false;
  }
  if (problem.initial_tangent_sign < -1 || problem.initial_tangent_sign > 1) {
    return false;
  }
  return an::ResolveFaceSequence(table, problem.faces, problem.face_count, slots) == an::Status::kOk;
}

// Copies one trace into its result's storage block.
void FillFiberResult(const lumice::analytic::TraceResult& trace, const double incident[3],
                     LUMICE_ANALYTIC_FiberResult* out) {
  out->status = ToStatus(trace.status);
  out->reason = ToReason(trace.reason);
  const int n = trace.PoseCount();
  out->pose_count = n;
  if (n == 0) {
    return;
  }
  const size_t un = static_cast<size_t>(n);
  auto storage = std::make_unique<FiberResultStorage>();
  storage->values = std::make_unique<double[]>(17 * un - 1);
  double* poses = storage->values.get();
  double* sun = poses + 9 * un;
  double* arclength = sun + 3 * un;
  double* residuals = arclength + (un - 1);
  double* tangents = residuals + un;
  std::memcpy(poses, trace.poses.data(), 9 * un * sizeof(double));
  for (size_t i = 0; i < un; i++) {
    // u = R^T (-s): the sun direction seen from the crystal.
    const double* r = poses + 9 * i;
    for (int k = 0; k < 3; k++) {
      sun[3 * i + k] = -(r[0 * 3 + k] * incident[0] + r[1 * 3 + k] * incident[1] + r[2 * 3 + k] * incident[2]);
    }
  }
  if (un > 1) {
    std::memcpy(arclength, trace.arclength_increments.data(), (un - 1) * sizeof(double));
  }
  std::memcpy(residuals, trace.residual_norms.data(), un * sizeof(double));
  std::memcpy(tangents, trace.tangents.data(), 3 * un * sizeof(double));
  out->poses = poses;
  out->crystal_frame_sun_directions = sun;
  out->arclength_increments = un > 1 ? arclength : nullptr;
  out->residual_norms = residuals;
  out->tangents = tangents;
  out->storage = storage.release();
}

LUMICE_ANALYTIC_FiberResult* ElementAt(LUMICE_ANALYTIC_FiberResult* base, size_t stride, int i) {
  return reinterpret_cast<LUMICE_ANALYTIC_FiberResult*>(reinterpret_cast<unsigned char*>(base) +
                                                        stride * static_cast<size_t>(i));
}

void ReleaseFiberStorage(LUMICE_ANALYTIC_FiberResult* result) {
  // Reclaims the block released to the caller by FillFiberResult; destroyed at scope exit.
  std::unique_ptr<FiberResultStorage> owned(static_cast<FiberResultStorage*>(result->storage));
  ZeroAfterStructSize(result);
}

LUMICE_ANALYTIC_ErrorCode TraceFiberBatchImpl(const LUMICE_ANALYTIC_Crystal* crystal,
                                              const LUMICE_ANALYTIC_FiberProblem* problems, int count,
                                              const LUMICE_ANALYTIC_ContinuationOptions* options,
                                              LUMICE_ANALYTIC_FiberResult* out_results, size_t stride) {
  namespace an = lumice::analytic;
  if (crystal == nullptr || problems == nullptr) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  for (int i = 0; i < count; i++) {
    if (problems[i].faces == nullptr && problems[i].face_count > 0) {
      return LUMICE_ANALYTIC_ERR_NULL_ARG;
    }
  }
  an::ContinuationParams base_params;
  if (!ToParams(options, &base_params)) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  an::FaceNormalTable table;
  if (auto status = an::BuildFaceNormals(*crystal, &table); status != an::Status::kOk) {
    return ToErrorCode(status);
  }
  int slots[an::kMaxFaceCount];
  for (int i = 0; i < count; i++) {
    const LUMICE_ANALYTIC_FiberProblem& problem = problems[i];
    LUMICE_ANALYTIC_FiberResult* out = ElementAt(out_results, stride, i);
    if (!ValidProblem(table, problem, slots)) {
      out->status = LUMICE_ANALYTIC_FIBER_NUMERICAL_FAILURE;
      out->reason = LUMICE_ANALYTIC_REASON_INVALID_NUMERICAL_INPUT;
      continue;
    }
    an::ContinuationParams params = base_params;
    params.initial_tangent_sign = problem.initial_tangent_sign == -1 ? -1 : 1;
    const an::IcePathMap map(table, slots, problem.face_count, problem.refractive_index, problem.incident_direction);
    const an::TraceResult trace =
        an::TraceFiber(map, an::MakeTargetChart(problem.target_direction), problem.seed_pose, params);
    FillFiberResult(trace, problem.incident_direction, out);
  }
  return LUMICE_ANALYTIC_OK;
}

LUMICE_ANALYTIC_ErrorCode EvaluatePathImpl(const LUMICE_ANALYTIC_Crystal* crystal, const int* faces, int face_count,
                                           double refractive_index, const double incident_direction[3],
                                           const double pose[9], LUMICE_ANALYTIC_PathEvaluation* out) {
  namespace an = lumice::analytic;
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
  if (face_count < 2 || face_count > an::kMaxFaceCount) {
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
  if (out == nullptr) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  ZeroAfterStructSize(out);
  // No exception crosses the C boundary: an allocation failure is ERR_UNKNOWN, with out zero-filled
  // (the storage it would have owned was never released to it).
  try {
    const LUMICE_ANALYTIC_ErrorCode code =
        EvaluatePathImpl(crystal, faces, face_count, refractive_index, incident_direction, pose, out);
    if (code != LUMICE_ANALYTIC_OK) {
      ZeroAfterStructSize(out);
    }
    return code;
  } catch (...) {
    ZeroAfterStructSize(out);
    return LUMICE_ANALYTIC_ERR_UNKNOWN;
  }
}

void LUMICE_ANALYTIC_ReleasePathEvaluation(LUMICE_ANALYTIC_PathEvaluation* eval) {
  if (eval == nullptr || eval->struct_size < sizeof(LUMICE_ANALYTIC_PathEvaluation)) {
    return;
  }
  // Reclaims the block released to the caller by EvaluatePath; destroyed at scope exit.
  std::unique_ptr<PathEvaluationStorage> owned(static_cast<PathEvaluationStorage*>(eval->storage));
  ZeroAfterStructSize(eval);
}

LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_TraceFiberBatch(const LUMICE_ANALYTIC_Crystal* crystal,
                                                          const LUMICE_ANALYTIC_FiberProblem* problems, int count,
                                                          const LUMICE_ANALYTIC_ContinuationOptions* options,
                                                          LUMICE_ANALYTIC_FiberResult* out_results) {
  if (count < 0) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;  // the array's length is unknown: touch nothing
  }
  if (count == 0) {
    return LUMICE_ANALYTIC_OK;
  }
  if (out_results == nullptr) {
    return LUMICE_ANALYTIC_ERR_NULL_ARG;
  }
  // The stride is the caller's sizeof (doc/analytic-api.md section 8.2). Below this struct's size it
  // cannot be walked: only element 0 is known to exist.
  const size_t stride = out_results[0].struct_size;
  if (stride < sizeof(LUMICE_ANALYTIC_FiberResult)) {
    ZeroAfterStructSize(&out_results[0]);
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  bool uniform = true;
  for (int i = 0; i < count; i++) {
    LUMICE_ANALYTIC_FiberResult* out = ElementAt(out_results, stride, i);
    uniform = uniform && out->struct_size == stride;
    ZeroAfterStructSize(out);
  }
  if (!uniform) {
    return LUMICE_ANALYTIC_ERR_INVALID_VALUE;
  }
  try {
    const LUMICE_ANALYTIC_ErrorCode code = TraceFiberBatchImpl(crystal, problems, count, options, out_results, stride);
    if (code != LUMICE_ANALYTIC_OK) {
      for (int i = 0; i < count; i++) {
        ReleaseFiberStorage(ElementAt(out_results, stride, i));
      }
    }
    return code;
  } catch (...) {
    // Elements filled before the failure own their storage; give it back before zeroing.
    for (int i = 0; i < count; i++) {
      ReleaseFiberStorage(ElementAt(out_results, stride, i));
    }
    return LUMICE_ANALYTIC_ERR_UNKNOWN;
  }
}

LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_TraceFiber(const LUMICE_ANALYTIC_Crystal* crystal,
                                                     const LUMICE_ANALYTIC_FiberProblem* problem,
                                                     const LUMICE_ANALYTIC_ContinuationOptions* options,
                                                     LUMICE_ANALYTIC_FiberResult* out_result) {
  return LUMICE_ANALYTIC_TraceFiberBatch(crystal, problem, 1, options, out_result);
}

void LUMICE_ANALYTIC_ReleaseFiberResult(LUMICE_ANALYTIC_FiberResult* result) {
  if (result == nullptr || result->struct_size < sizeof(LUMICE_ANALYTIC_FiberResult)) {
    return;
  }
  ReleaseFiberStorage(result);
}

}  // extern "C"
