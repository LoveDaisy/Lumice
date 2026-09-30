#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "config/crystal_config.hpp"        // ns::PrismCrystalParam / PyramidCrystalParam (LUMICE_GetCrystalMesh)
#include "config/raypath_color_config.hpp"  // ns::kDefaultCompositeMode (single-source default)
#include "config/raypath_validation.hpp"
#include "config/render_config.hpp"
#include "core/annotation_overlay.hpp"  // annotation::ComputeAnchors (LUMICE_ComputeAnnotationAnchors)
#include "core/crystal.hpp"
#include "core/ev_anchor.hpp"
#include "core/geo3d.hpp"
#include "core/lens_proj_build.hpp"  // mask_detail::PixelToWorld + the display clips (LUMICE_UnprojectPixel)
#include "core/miller_wedge.hpp"
#include "core/scatter_accum.hpp"  // MakeCameraRotation (LUMICE_UnprojectPixel)
#include "core/trace_ops.hpp"      // ns::MakeCrystal (core single-source crystal sampler)
#if defined(__APPLE__)
#include "core/backend/metal_trace_backend.hpp"
#endif
#if defined(LUMICE_CUDA_ENABLED)
#include "core/backend/cuda_trace_backend.hpp"  // CudaDeviceAvailable() for LUMICE_IsBackendAvailable
#endif
#include "include/lumice.h"
#include "server/c_api_internal.hpp"
#include "server/raypath_histogram_consumer.hpp"  // ReduceRaypathHistogram (the analysis reads)
#include "server/server.hpp"
#include "server/version_gen.hpp"  // kLumiceProductVersion, configure_file'd from project(VERSION)
#include "util/callback_sink.hpp"
#include "util/color_space.hpp"
#include "util/logger.hpp"
#include "util/path_utils.hpp"

namespace ns = lumice;

// =============== Internal Helpers ===============
struct LUMICE_Server_ {
  std::unique_ptr<ns::Server> server_;
};

// =============== Server Lifecycle ===============
LUMICE_Server* LUMICE_CreateServer() {
  auto* s = new LUMICE_Server;
  s->server_ = std::make_unique<ns::Server>();
  return s;
}


LUMICE_Server* LUMICE_CreateServerEx(const LUMICE_ServerConfig* config) {
  auto* s = new LUMICE_Server;
  int num_workers = (config != nullptr) ? config->num_workers : 0;
  uint32_t sim_seed = (config != nullptr) ? config->sim_seed : 0;
  // C-API boundary: the public ABI stays `int`; internally we use the
  // type-safe BackendKind enum. This static_cast is the single conversion
  // point — other internal code must never cast int↔BackendKind.
  int raw_backend = (config != nullptr) ? config->preferred_backend : 0;
  auto preferred_backend = static_cast<ns::BackendKind>(raw_backend);
  s->server_ = std::make_unique<ns::Server>(num_workers, sim_seed, preferred_backend);
  return s;
}


void LUMICE_DestroyServer(LUMICE_Server* server) {
  if (!server) {
    return;
  }
  server->server_->Terminate();
  delete server;
}


// =============== Logging ===============
void LUMICE_SetLogLevel(LUMICE_Server* server, LUMICE_LogLevel level) {
  static constexpr ns::LogLevel kLevelMap[] = {
    ns::LogLevel::kTrace,    // LUMICE_LOG_TRACE
    ns::LogLevel::kDebug,    // LUMICE_LOG_DEBUG
    ns::LogLevel::kVerbose,  // LUMICE_LOG_VERBOSE
    ns::LogLevel::kInfo,     // LUMICE_LOG_INFO
    ns::LogLevel::kWarning,  // LUMICE_LOG_WARNING
    ns::LogLevel::kError,    // LUMICE_LOG_ERROR
    ns::LogLevel::kOff,      // LUMICE_LOG_OFF
  };
  if (level >= LUMICE_LOG_TRACE && level <= LUMICE_LOG_OFF) {
    auto mapped = kLevelMap[level];
    if (server) {
      server->server_->SetLogLevel(mapped);
    }
    ns::GetGlobalLogger().SetLevel(mapped);
  }
}


void LUMICE_SetLogCallback(LUMICE_LogCallback callback) {
  auto& sink = ns::GetCallbackSink();
  sink->SetCallback(callback);

  // Add callback sink to shared dist_sink on first call (idempotent check via static flag).
  // Set our custom formatter since sinks added after Logger::set_formatter don't inherit it.
  static bool registered = false;
  if (!registered) {
    sink->set_formatter(ns::CreateLumiceFormatter(ns::kLogPattern));
    ns::GetSharedSink()->add_sink(sink);
    registered = true;
  }
}


// LUMICE_MAX_CONFIG_MARKERS is spelled as a literal 6 in the header, because the id-count macro is
// defined further down the file than the renderer struct that needs it and the preprocessor reads
// top to bottom. That literal is not a second opinion about how many ids there are: the renderer's
// array is EXACTLY the id space, since duplicates are rejected. This is the line that makes adding
// a seventh id to one side a compile error rather than a list that silently cannot hold them all.
static_assert(LUMICE_MAX_CONFIG_MARKERS == LUMICE_ANNOTATION_MARKER_COUNT,
              "LUMICE_RenderParam::markers must have room for exactly the marker id space");

// Display-time color update: see doc/capi-lifecycle-architecture.md §4 / §6.4.
// It does NOT restart the simulation — accumulator, epoch, and consumers are
// untouched. Only the next acquired result frame re-composites with the new appearance.
LUMICE_ErrorCode LUMICE_SetRaypathColors(LUMICE_Server* server, const LUMICE_ColorClassDisplay* classes,
                                         int class_count, const int* z_order, int mode) {
  if (!server) {
    return LUMICE_ERR_NULL_ARG;
  }
  if (class_count < 0 || (class_count > 0 && classes == nullptr)) {
    return LUMICE_ERR_NULL_ARG;
  }
  ns::CompositeMode composite_mode = ns::CompositeMode::kDominant;
  switch (mode) {
    case LUMICE_COLOR_MODE_DOMINANT:
      composite_mode = ns::CompositeMode::kDominant;
      break;
    case LUMICE_COLOR_MODE_ADDITIVE:
      composite_mode = ns::CompositeMode::kAdditive;
      break;
    case LUMICE_COLOR_MODE_PAINTER:
      composite_mode = ns::CompositeMode::kPainter;
      break;
    default:
      return LUMICE_ERR_INVALID_VALUE;
  }
  std::vector<ns::ColorClassDisplay> internal;
  internal.reserve(static_cast<size_t>(class_count));
  for (int i = 0; i < class_count; i++) {
    ns::ColorClassDisplay d;
    d.color_[0] = classes[i].color[0];
    d.color_[1] = classes[i].color[1];
    d.color_[2] = classes[i].color[2];
    d.visible_ = classes[i].visible != 0;
    d.solo_ = classes[i].solo != 0;
    internal.push_back(d);
  }
  auto err = server->server_->SetRaypathColors(internal.data(), class_count, z_order, composite_mode);
  if (err) {
    LOG_ERROR("LUMICE_SetRaypathColors failed: {}", err.message);
    return ns::capi::ToCApiErrorCode(err.code);
  }
  return LUMICE_OK;
}


// Display-time EV multiplier for the composite path. See the
// LUMICE_SetCompositeExposure comment in include/lumice.h for the semantics
// (single scalar, mono path untouched, snapshot_dirty_ flipped so the next
// acquired result frame rebakes the composite). No ev_total validation: the GUI is the
// only in-tree caller and already clamps to [-6, 6]; server-side double-clamp
// would hide caller bugs without preventing any real hazard.
LUMICE_ErrorCode LUMICE_SetCompositeExposure(LUMICE_Server* server, float ev_total) {
  if (!server) {
    return LUMICE_ERR_NULL_ARG;
  }
  auto err = server->server_->SetCompositeExposure(ev_total);
  if (err) {
    LOG_ERROR("LUMICE_SetCompositeExposure failed: {}", err.message);
    return ns::capi::ToCApiErrorCode(err.code);
  }
  return LUMICE_OK;
}


// Display-time background colour for the composite path. See the
// LUMICE_SetCompositeBackground comment in include/lumice.h for
// the semantics (3 ADDITIVE linear floats, masked to the imaged region, mono
// path untouched, snapshot_dirty_ flipped so the next acquired result frame
// rebakes the composite). Unlike the exposure setter this one takes a pointer,
// so it needs its own null check; component values are not validated for the
// same reason ev_total is not — the caller owns the colour space conversion and
// the compositor clamps at the sRGB stage regardless.
LUMICE_ErrorCode LUMICE_SetCompositeBackground(LUMICE_Server* server, const float* background_linear) {
  if (!server || !background_linear) {
    return LUMICE_ERR_NULL_ARG;
  }
  auto err = server->server_->SetCompositeBackground(background_linear);
  if (err) {
    LOG_ERROR("LUMICE_SetCompositeBackground failed: {}", err.message);
    return ns::capi::ToCApiErrorCode(err.code);
  }
  return LUMICE_OK;
}


// Per-color-class empty-arc detector.
LUMICE_ErrorCode LUMICE_GetColorClassSignal(LUMICE_Server* server, int* out_flags, int class_count) {
  if (!server) {
    return LUMICE_ERR_NULL_ARG;
  }
  if (class_count < 0) {
    return LUMICE_ERR_INVALID_VALUE;
  }
  if (class_count > 0 && out_flags == nullptr) {
    return LUMICE_ERR_NULL_ARG;
  }
  std::vector<uint8_t> tmp(static_cast<size_t>(class_count), 0);
  auto err = server->server_->GetColorClassSignals(tmp.data(), class_count);
  if (err) {
    LOG_ERROR("LUMICE_GetColorClassSignal failed: {}", err.message);
    return ns::capi::ToCApiErrorCode(err.code);
  }
  for (int i = 0; i < class_count; i++) {
    out_flags[i] = tmp[static_cast<size_t>(i)] ? 1 : 0;
  }
  return LUMICE_OK;
}


// Single source for the "hand an already-encoded scene JSON to the core and report reuse" tail.
// LUMICE_CommitScene is its only caller today (v4.12 removed the three legacy entry points that
// shared it); it stays a separate function because the core commit call, the error log, the
// error-code mapping and the out_reused write-back are one unit that a future second producer of
// an already-encoded document must reuse verbatim rather than re-derive. Callers are responsible
// for their own NULL checks before calling. `source` only tags the error log with the
// originating entry point, so a failed commit stays attributable.
static LUMICE_ErrorCode CommitJsonToServer(LUMICE_Server* server, const nlohmann::json& root, int* out_reused,
                                           const char* source) {
  bool reused = false;
  auto err = server->server_->CommitConfig(root, &reused);
  if (err) {
    LOG_ERROR("Failed to commit configuration ({}): {}", source, err.message);
    return ns::capi::ToCApiErrorCode(err.code);
  }
  if (out_reused) {
    *out_reused = reused ? 1 : 0;
  }
  return LUMICE_OK;
}


// =============== Scene commit ===============
// Handle->core commit, and since v4.12 the only commit entry point in the API. It carries no
// bounds-check prologue on purpose: the removed struct path needed one because a caller could
// hand-fill the wide C struct with arbitrary counts/pointers, a state a Scene cannot reach —
// every Add*/Set* validated its own input at call time, so re-checking here would be dead code.
// scene->root and ConfigToJson's output are the same encoding (they share the per-section encode
// helpers), which is what lets JsonToScene hand this function a document it produced by the
// ConfigScratch route without any behavioral difference.
LUMICE_ErrorCode LUMICE_CommitScene(LUMICE_Server* server, const LUMICE_Scene* scene, int* out_reused) {
  if (!server || !scene) {
    return LUMICE_ERR_NULL_ARG;
  }
  return CommitJsonToServer(server, scene->root, out_reused, "scene");
}


// =============== Results ===============
// LUMICE_ResultFrame_ wraps a share of one published, immutable server-side snapshot.
// Lifecycle mirrors LUMICE_Scene_ (new/delete); the shared_ptr inside is what makes the
// borrow safe — the buffers a caller reads out of the frame cannot be freed or rewritten
// while it holds one.
struct LUMICE_ResultFrame_ {
  std::shared_ptr<const ns::ResultFrame> frame_;
};

LUMICE_ErrorCode LUMICE_AcquireResultFrame(LUMICE_Server* server, LUMICE_ResultFrame** out_frame) {
  if (!server || !out_frame) {
    return LUMICE_ERR_NULL_ARG;
  }
  *out_frame = new LUMICE_ResultFrame_{ server->server_->AcquireResultFrame() };
  return LUMICE_OK;
}


void LUMICE_ReleaseResultFrame(LUMICE_ResultFrame* frame) {
  delete frame;  // delete nullptr is a no-op (NULL-safe, same as LUMICE_DestroyServer)
}


LUMICE_ResultFrame* WrapResultFrameForTest(std::shared_ptr<const ns::ResultFrame> frame) {
  return new LUMICE_ResultFrame_{ std::move(frame) };
}


LUMICE_ErrorCode LUMICE_FrameGetRawXyz(const LUMICE_ResultFrame* frame, LUMICE_RawXyzResult* out, int max_count) {
  if (!frame || !out) {
    return LUMICE_ERR_NULL_ARG;
  }

  const auto& results = frame->frame_->xyz_results_;
  int count = static_cast<int>(results.size());
  if (count > max_count) {
    count = max_count;
  }

  for (int i = 0; i < count; i++) {
    out[i].renderer_id = results[i].renderer_id_;
    out[i].img_width = results[i].img_width_;
    out[i].img_height = results[i].img_height_;
    out[i].xyz_buffer = results[i].xyz_buffer_;
    out[i].snapshot_intensity = results[i].snapshot_intensity_;
    out[i].intensity_factor = results[i].intensity_factor_;
    out[i].has_valid_data = results[i].has_valid_data_ ? 1 : 0;
    out[i].snapshot_generation = results[i].snapshot_generation_;
    out[i].effective_pixels = results[i].effective_pixels_;
    out[i].emitted_energy = results[i].snapshot_emitted_energy_;
    out[i].epoch = results[i].epoch_;
    // Same value on every row — see the field's own contract in lumice.h. The one place it
    // is computed is AnchorConsumer::PrepareSnapshot; everything from there to here is a
    // copy, which is what keeps CLI and GUI on literally the same number.
    out[i].anchor_l99_sky = results[i].anchor_l99_sky_;
    out[i].axis_solid_angle = results[i].axis_solid_angle_;
  }

  // Sentinel: see doc/capi-lifecycle-architecture.md §5.2 (fix: 5287efe).
  if (count < max_count) {
    std::memset(&out[count], 0, sizeof(LUMICE_RawXyzResult));
  }

  return LUMICE_OK;
}


LUMICE_ErrorCode LUMICE_FrameGetComposite(const LUMICE_ResultFrame* frame, LUMICE_RenderResult* out, int max_count) {
  if (!frame || !out) {
    return LUMICE_ERR_NULL_ARG;
  }

  const auto& results = frame->frame_->composite_results_;
  int count = static_cast<int>(results.size());
  if (count > max_count) {
    count = max_count;
  }

  for (int i = 0; i < count; i++) {
    out[i].renderer_id = results[i].renderer_id_;
    out[i].img_width = results[i].w_;
    out[i].img_height = results[i].h_;
    out[i].img_buffer = results[i].rgb_ ? results[i].rgb_->data() : nullptr;
    // Composite path — participating-classes union P99 (auto-EV anchor).
    out[i].composite_p99_y = results[i].p99_y_;
  }

  // Sentinel: see doc/capi-lifecycle-architecture.md §5.2 (fix: 5287efe).
  if (count < max_count) {
    std::memset(&out[count], 0, sizeof(LUMICE_RenderResult));
  }

  return LUMICE_OK;
}


LUMICE_ErrorCode LUMICE_FrameGetRender(const LUMICE_ResultFrame* frame, LUMICE_RenderResult* out, int max_count) {
  if (!frame || !out) {
    return LUMICE_ERR_NULL_ARG;
  }

  const auto& results = frame->frame_->render_results_;
  int count = static_cast<int>(results.size());
  if (count > max_count) {
    count = max_count;
  }

  for (int i = 0; i < count; i++) {
    out[i].renderer_id = results[i].renderer_id_;
    out[i].img_width = results[i].img_width_;
    out[i].img_height = results[i].img_height_;
    out[i].img_buffer = results[i].img_buffer_;
    // Mono path — composite_p99_y is composite-only; leave at 0.
    out[i].composite_p99_y = 0.0f;
  }

  // Sentinel: see doc/capi-lifecycle-architecture.md §5.2 (fix: 5287efe).
  if (count < max_count) {
    std::memset(&out[count], 0, sizeof(LUMICE_RenderResult));
  }

  return LUMICE_OK;
}


LUMICE_ErrorCode LUMICE_FrameGetStats(const LUMICE_ResultFrame* frame, LUMICE_StatsResult* out) {
  if (!frame || !out) {
    return LUMICE_ERR_NULL_ARG;
  }

  const auto& stats = frame->frame_->stats_result_;
  if (stats.has_value()) {
    out->ray_seg_num = stats->ray_seg_num_;
    out->sim_ray_num = stats->sim_ray_num_;
    out->crystal_num = stats->crystal_num_;
    out->orientation_num = stats->orientation_num_;
  } else {
    std::memset(out, 0, sizeof(LUMICE_StatsResult));
  }

  return LUMICE_OK;
}


LUMICE_ErrorCode LUMICE_GetSimRayCount(LUMICE_Server* server, LUMICE_RayCount* out) {
  if (!server || !out) {
    return LUMICE_ERR_NULL_ARG;
  }
  // Cheap O(1) live ray-count read — no snapshot / no render. For progress polling that
  // needs sim_ray_num every iteration without paying the per-poll DoSnapshot/sRGB render
  // cost of materializing a result frame.
  *out = static_cast<LUMICE_RayCount>(server->server_->GetLiveSimRayCount());
  return LUMICE_OK;
}


// =============== State & Control ===============
// QueryServerState is a PROJECTION of the single-source lifecycle truth
// (GetSimLifecycle): RUNNING -> RUNNING; IDLE | COMPLETED -> IDLE. This keeps the
// historical running/idle binary intact for the existing call sites while
// GetSimLifecycle owns the authoritative completed-vs-idle distinction (I1).
LUMICE_ErrorCode LUMICE_QueryServerState(LUMICE_Server* server, LUMICE_ServerState* out) {
  if (!server || !out) {
    return LUMICE_ERR_NULL_ARG;
  }

  if (server->server_->GetSimLifecycle() == ns::SimLifecycle::kRunning) {
    *out = LUMICE_SERVER_RUNNING;
  } else {
    *out = LUMICE_SERVER_IDLE;
  }

  return LUMICE_OK;
}


LUMICE_ErrorCode LUMICE_GetSimLifecycle(LUMICE_Server* server, LUMICE_SimLifecycleResult* out) {
  if (!server || !out) {
    return LUMICE_ERR_NULL_ARG;
  }

  // -Wswitch: exhaustive over SimLifecycle, no `default:` — a new enum value
  // forces this mapping to add an explicit case.
  switch (server->server_->GetSimLifecycle()) {
    case ns::SimLifecycle::kIdle:
      out->lifecycle = LUMICE_LIFECYCLE_IDLE;
      break;
    case ns::SimLifecycle::kRunning:
      out->lifecycle = LUMICE_LIFECYCLE_RUNNING;
      break;
    case ns::SimLifecycle::kCompleted:
      out->lifecycle = LUMICE_LIFECYCLE_COMPLETED;
      break;
  }
  out->epoch = static_cast<unsigned long long>(server->server_->CommittedEpoch());
  out->session_kind = static_cast<int>(server->server_->GetSessionKind());

  return LUMICE_OK;
}


LUMICE_ErrorCode LUMICE_GetDrainStatus(LUMICE_Server* server, LUMICE_DrainResult* out) {
  if (!server || !out) {
    return LUMICE_ERR_NULL_ARG;
  }

  // Order matters: read the drained epoch BEFORE the current one. Sampled the
  // other way round, a CommitConfig landing between the two reads would pair an
  // old current_epoch with a drained_epoch the commit has already outrun, and
  // the caller's equality test would report the SUPERSEDED epoch as drained.
  // This order can only ever under-report (drained trails current), which the
  // caller's poll loop resolves on its next iteration.
  out->drained_epoch = static_cast<unsigned long long>(server->server_->DrainedEpoch());
  out->current_epoch = static_cast<unsigned long long>(server->server_->CommittedEpoch());

  return LUMICE_OK;
}


// Readback of the color-degrade counters. component_overflow_count is the
// synchronous host-side count written inside CommitConfig; the three GPU-only
// caps (symmetry-group / OR-summand / color-class) are published asynchronously
// from the worker's first batch (server ConsumeData) and read atomically here,
// so a GUI poll tick picks them up after DoRun.
LUMICE_ErrorCode LUMICE_GetColorOverflowInfo(LUMICE_Server* server, LUMICE_ColorOverflowInfo* out) {
  if (!server || !out) {
    return LUMICE_ERR_NULL_ARG;
  }
  out->component_overflow_count = static_cast<int>(server->server_->GetLastColorComponentOverflowCount());
  const lumice::ColorDegradeCounts degrade = server->server_->GetLastColorDegradeCounts();
  out->symmetry_group_overflow_count = static_cast<int>(degrade.symmetry_group_overflow);
  out->or_summand_overflow_count = static_cast<int>(degrade.or_summand_overflow);
  out->color_class_overflow_count = static_cast<int>(degrade.color_class_overflow);
  return LUMICE_OK;
}


// Readback of the "GPU route is now running on the legacy CPU path" flag.
// Same poll contract as LUMICE_GetColorOverflowInfo above — the condition is raised by
// the worker mid-run, so the GUI only ever sees it by polling.
LUMICE_ErrorCode LUMICE_GetBackendFallbackFlag(LUMICE_Server* server, int* out_fell_back) {
  if (!server || !out_fell_back) {
    return LUMICE_ERR_NULL_ARG;
  }
  *out_fell_back = server->server_->BackendFellBack() ? 1 : 0;
  return LUMICE_OK;
}


static_assert(static_cast<int>(ns::SessionKind::kRender) == LUMICE_SESSION_RENDER &&
                  static_cast<int>(ns::SessionKind::kAnalysis) == LUMICE_SESSION_ANALYSIS,
              "LUMICE_SESSION_* drifted from SessionKind; the cast in LUMICE_GetSimLifecycle relies on equality");
static_assert(static_cast<int>(ns::BackendKind::kCpu) == LUMICE_BACKEND_CPU &&
                  static_cast<int>(ns::BackendKind::kMetal) == LUMICE_BACKEND_METAL &&
                  static_cast<int>(ns::BackendKind::kCuda) == LUMICE_BACKEND_CUDA,
              "LUMICE_BACKEND_* drifted from BackendKind; the casts at this boundary rely on equality");
LUMICE_ErrorCode LUMICE_GetActiveBackend(LUMICE_Server* server, int* out_backend) {
  if (!server || !out_backend) {
    return LUMICE_ERR_NULL_ARG;
  }
  // The inverse of LUMICE_SetPreferredBackend's cast: BackendKind's values ARE the
  // LUMICE_BACKEND_* constants (backend_kind.hpp).
  *out_backend = static_cast<int>(server->server_->GetActiveBackend());
  return LUMICE_OK;
}


void LUMICE_StopServer(LUMICE_Server* server) {
  if (!server) {
    return;
  }

  server->server_->Stop();
}


LUMICE_ErrorCode LUMICE_ContinueRender(LUMICE_Server* server, int infinite, LUMICE_RayCount additional_ray_num) {
  if (!server) {
    return LUMICE_ERR_NULL_ARG;
  }
  // Same (infinite, ray_num) reading as LUMICE_SceneSetSimParams: `infinite` wins, and the
  // budget is otherwise taken as given — the server refuses a zero one.
  const size_t budget = infinite ? ns::kInfSize : static_cast<size_t>(additional_ray_num);
  auto err = server->server_->ContinueRun(budget);
  if (err) {
    LOG_ERROR("Failed to continue the render: {}", err.message);
    return ns::capi::ToCApiErrorCode(err.code);
  }
  return LUMICE_OK;
}


void LUMICE_SetPreferredBackend(LUMICE_Server* server, int backend) {
  if (!server) {
    return;
  }
  // C-API boundary cast: the public ABI keeps int; everything inside uses
  // BackendKind. Unknown int values map to a kCuda-or-beyond value that
  // CreateBackend / ResolveGpuRoute handle as "fall back to CPU".
  server->server_->SetPreferredBackend(static_cast<ns::BackendKind>(backend));
}


int LUMICE_IsBackendAvailable(int backend) {
  try {
    // -Wswitch: exhaustive over BackendKind, no `default:`. Adding a new
    // BackendKind value forces this site to add an explicit case (compile-time
    // gate against silently returning 0 for a freshly-added backend).
    auto kind = static_cast<ns::BackendKind>(backend);
    switch (kind) {
      case ns::BackendKind::kCpu:
        return 1;
      case ns::BackendKind::kMetal:
#if defined(__APPLE__)
        // Deepen the gate from device-presence to trial-compile +
        // entry-point lookup. MetalDeviceAvailable returned true on macOS 26.5 /
        // M1 Max where MSL compilation succeeded but newFunctionWithName
        // ("trace_layer_kernel") returned nil, letting the GUI "Use GPU"
        // checkbox light up and BeginSession abort on a Metal-framework nil-
        // computeFunction assertion. MetalPipelineAvailable runs the same
        // source + options EnsurePso uses and verifies all three kernel entry
        // points resolve.
        return lumice::MetalPipelineAvailable() ? 1 : 0;
#else
        return 0;
#endif
      case ns::BackendKind::kCuda:
#if defined(LUMICE_CUDA_ENABLED)
        // Mirror the Metal gate: report CUDA available only when the build has the
        // CUDA backend AND a usable NVIDIA device is present at runtime (probed via
        // the driver). Non-CUDA builds and GPU-less hosts return 0, so the GUI
        // "Use GPU" toggle stays hidden and BeginSession never routes to a missing
        // device (the CUDA analog of the Metal nil-PSO guard above).
        return lumice::CudaDeviceAvailable() ? 1 : 0;
#else
        return 0;
#endif
    }
    return 0;
  } catch (...) {
    return 0;
  }
}

int LUMICE_WillUseGpuRoute(int preferred_backend) {
  try {
    // Single source of truth: delegate to the same env-aware ResolveGpuRoute the
    // server uses to size worker_count (server.cpp). Casting an unknown int to
    // BackendKind is safe here — ResolveGpuRoute treats non-Metal/non-CUDA (or an
    // unavailable device) as the legacy CPU route (returns false), the runtime analog
    // of LUMICE_IsBackendAvailable's compile-time -Wswitch guard above.
    return ns::ResolveGpuRoute(static_cast<ns::BackendKind>(preferred_backend), ns::GetGlobalLogger()) ? 1 : 0;
  } catch (...) {
    // Fail safe to the legacy CPU route, but not silently: a swallowed device-probe
    // error here would run the benchmark's CPU dual-pass while preferred_backend is a
    // GPU, with no trace (project discipline: silent fallbacks cause undebuggable
    // per-machine drift).
    ILOG_WARN(ns::GetGlobalLogger(), "LUMICE_WillUseGpuRoute: route resolution threw; assuming legacy CPU route");
    return 0;
  }
}


// =============== Product Version ===============
// The generated header is the ONE place the version literal exists in compiled code; every
// display surface (CLI --version, GUI title, startup logs, .lmc app_version) reads it through
// this function.
const char* LUMICE_GetVersionString(void) {
  return kLumiceProductVersion;
}


// =============== Engine Build Provenance ===============
// LUMICE_ISA_LEVEL_STR is defined by lumice_apply_isa_march() in the top-level CMakeLists.txt on
// the objects of lumice_obj — this translation unit among them — by the same condition that
// applies the -march flag, so the two cannot drift apart. Real MSVC cl.exe never defines it (the
// function does not run there), hence the fallback. This is the ONE place the macro is read; the
// executables ask through this function so that what they report is the engine they loaded.
const char* LUMICE_GetEngineIsaLevel(void) {
#if defined(LUMICE_ISA_LEVEL_STR)
  return LUMICE_ISA_LEVEL_STR;
#else
  return "baseline";
#endif
}


// =============== Crystal Mesh ===============

// Reroutes preview-only geometry through the closed-form Crystal factories so
// the LUMICE_CrystalMesh output is built from the parametric face_number /
// face_present / face_vtx tables directly. Replaces the historical pipeline of
//   CreatePrismMesh/CreatePyramidMesh → FillPerFaceTopology (argmax reversal on
//   triangle normals to reconstruct face groups) → FillHexFnMap (argmax again
//   for per-tri face numbers) → triangle-adjacency dihedral edge filter.
// All three reversals are gone: face_numbers per triangle come from the
// Crystal's fn_map_ (parametric, populated from cf_geom_.face_number in
// PopulateFromCfGeom), and face_vtx_pool / face_normals come straight from
// cf_geom_. See doc/crystal-geometry-representation.md §1 for the wider
// "delete the reversal, read the constant" story.
// Fold a 64-bit sample seed into the 32-bit seed RandomNumberGenerator accepts.
// XOR-fold (both halves participate) rather than truncate: truncation would make any
// two seeds that differ only in the high 32 bits collide deterministically; XOR-fold
// reduces that to a ~2^-32 uniform collision probability. This is NOT a "distinct
// sample_seed => distinct mesh" guarantee, only a removal of the deterministic-collision
// class of false negatives.
static uint32_t FoldSampleSeed64(unsigned long long seed) {
  return static_cast<uint32_t>(seed) ^ static_cast<uint32_t>(seed >> 32);
}

LUMICE_ErrorCode LUMICE_GetCrystalMesh(const LUMICE_CrystalParam* crystal, unsigned long long sample_seed,
                                       LUMICE_CrystalMesh* out) {
  if (!crystal || !out) {
    return LUMICE_ERR_NULL_ARG;
  }
  if (crystal->type != 0 && crystal->type != 1) {
    return LUMICE_ERR_INVALID_VALUE;
  }

  // Single mapping table: translate the shape into the core crystal JSON schema, then
  // let the core from_json (already validated by ConfigToJson round-trips) build the
  // CrystalParam variant. This reuses the existing translation instead of maintaining
  // a second, parallel struct-to-struct field map.
  ns::CrystalParam param;
  try {
    const nlohmann::json shape = CrystalShapeToJson(*crystal).at("shape");
    if (crystal->type == 0) {
      param = shape.get<ns::PrismCrystalParam>();
    } else {
      param = shape.get<ns::PyramidCrystalParam>();
    }
  } catch (...) {
    return LUMICE_ERR_INVALID_CONFIG;
  }

  // Sample one concrete shape through the core single-source sampler. A LOCAL RNG
  // instance (not the process singleton) is what makes the determinism contract hold:
  // identical seed + identical param => identical MakeCrystal draw sequence, with no
  // cross-call state carried in a shared generator. For a fully NO_RANDOM param,
  // MakeCrystal never touches the RNG, so sample_seed is a no-op (contract).
  ns::RandomNumberGenerator rng(FoldSampleSeed64(sample_seed));
  const ns::Crystal crystal_obj = ns::MakeCrystal(rng, param);

  // On-demand triangulation: the Crystal no longer stores a triangle mesh
  // (entry-point sampling consumes cf_geom_ corners directly). Geometry export
  // is a cold path (GUI preview, gated by a param hash) so building the mesh
  // here — instead of eagerly in every MakeCrystal — costs nothing on the hot
  // path. A degenerate sample (validation gate rejected) yields a default-constructed
  // Crystal with face_cnt==0; BuildMeshFromCfGeom and the export loops below are all
  // safe (zero-iteration) on that, producing an empty-but-valid mesh and LUMICE_OK.
  const ns::CrystalGeom& g = crystal_obj.CfGeom();
  const ns::detail::BuiltMesh built = ns::detail::BuildMeshFromCfGeom(g);
  const ns::Mesh& mesh = built.mesh;

  auto vtx_cnt = static_cast<int>(mesh.GetVtxCnt());
  if (vtx_cnt > LUMICE_MAX_CRYSTAL_VERTICES) {
    return LUMICE_ERR_INVALID_VALUE;
  }
  out->vertex_count = vtx_cnt;
  if (vtx_cnt > 0) {
    std::memcpy(out->vertices, mesh.GetVtxPtr(0), vtx_cnt * 3 * sizeof(float));
  }

  auto tri_cnt = mesh.GetTriangleCnt();
  if (static_cast<int>(tri_cnt) > LUMICE_MAX_CRYSTAL_TRIANGLES) {
    return LUMICE_ERR_INVALID_VALUE;
  }
  out->triangle_count = static_cast<int>(tri_cnt);
  if (tri_cnt > 0) {
    std::memcpy(out->triangles, mesh.GetTrianglePtr(0), tri_cnt * 3 * sizeof(int));
  }

  // Per-triangle face_number, read straight from cf_geom_. Each triangle's
  // originating face slot (built.tri_face_slot[i]) is, by construction, a
  // present face with >= 3 corners (an absent or sub-triangle face emits no
  // triangle), so cf_geom_.face_number[slot] is always a legal fn — there is
  // no kInvalidId / -1 sentinel case to map anymore.
  for (size_t i = 0; i < tri_cnt; ++i) {
    const int slot = built.tri_face_slot[i];
    out->face_numbers[i] = g.face_number[slot];
  }

  // Per-face polygon topology: walk present slots in cf_geom_, map each face's
  // CCW (x,y,z) vertex to its index in the deduped mesh vertex pool.
  // BuildMeshFromCfGeom used the same coordinates when it built the pool, so a
  // simple linear search with the same 1e-6f tolerance is guaranteed to hit.
  const float* vtx = (vtx_cnt > 0) ? mesh.GetVtxPtr(0) : nullptr;
  constexpr float kDedupTol = 1e-6f;
  auto find_vtx_idx = [&](float x, float y, float z) -> int {
    for (int i = 0; i < vtx_cnt; ++i) {
      float dx = vtx[i * 3 + 0] - x;
      float dy = vtx[i * 3 + 1] - y;
      float dz = vtx[i * 3 + 2] - z;
      if (std::sqrt(dx * dx + dy * dy + dz * dz) < kDedupTol) {
        return i;
      }
    }
    return -1;
  };

  int fi = 0;
  int pool_offset = 0;
  // Track (v_min, v_max) edge → (slot_a, slot_b). Second slot may stay -1 for
  // boundary edges (only in degenerate geometries; well-formed prism/pyramid
  // yields a closed 2-manifold so every polygon edge is shared by exactly two
  // present slots).
  struct EdgeSlotPair {
    int slot_a;
    int slot_b;
  };
  std::map<std::pair<int, int>, EdgeSlotPair> edge_slots;
  int slot_to_fi[ns::kCrystalGeomMaxFaces];
  for (int i = 0; i < ns::kCrystalGeomMaxFaces; ++i) {
    slot_to_fi[i] = -1;
  }

  for (int slot = 0; slot < g.face_cnt; ++slot) {
    if (!g.face_present[slot]) {
      continue;
    }
    int fn = g.face_vtx_cnt[slot];
    if (fn < 3) {
      continue;
    }
    if (fi >= LUMICE_MAX_CRYSTAL_FACES) {
      break;
    }
    if (pool_offset + fn > LUMICE_MAX_CRYSTAL_FACE_VTXPOOL) {
      break;  // pool exhausted
    }

    const float* face_v = g.face_vtx + slot * ns::kCrystalGeomMaxVtxPerFace * 3;
    // Resolve pool indices for this face's CCW vertex list.
    int local_indices[ns::kCrystalGeomMaxVtxPerFace];
    for (int k = 0; k < fn; ++k) {
      int idx = find_vtx_idx(face_v[k * 3 + 0], face_v[k * 3 + 1], face_v[k * 3 + 2]);
      if (idx < 0) {
        return LUMICE_ERR_INVALID_CONFIG;  // should be impossible: BuildMeshFromCfGeom deduped these coords
      }
      local_indices[k] = idx;
      out->face_vtx_pool[pool_offset + k] = idx;
    }

    out->face_numbers_by_face[fi] = g.face_number[slot];
    out->face_vtx_offsets[fi] = pool_offset;
    out->face_vtx_counts[fi] = fn;
    // Face normal from cf_geom_ is already unit outward (populated by the
    // closed-form evaluator + AdaptClosedFormXxxToCrystalGeom).
    out->face_normals[fi * 3 + 0] = g.face_normal[slot * 3 + 0];
    out->face_normals[fi * 3 + 1] = g.face_normal[slot * 3 + 1];
    out->face_normals[fi * 3 + 2] = g.face_normal[slot * 3 + 2];
    slot_to_fi[slot] = fi;
    pool_offset += fn;
    ++fi;

    // Register polygon-boundary edges for this slot.
    for (int k = 0; k < fn; ++k) {
      int a = local_indices[k];
      int b = local_indices[(k + 1) % fn];
      auto key = std::make_pair(std::min(a, b), std::max(a, b));
      auto [it, inserted] = edge_slots.try_emplace(key, EdgeSlotPair{ slot, -1 });
      if (!inserted) {
        if (it->second.slot_b < 0 && it->second.slot_a != slot) {
          it->second.slot_b = slot;
        }
      }
    }
  }
  out->face_count = fi;

  // Emit edges as polygon boundaries (no triangle-adjacency + dihedral-angle
  // threshold — that was a numerical stand-in for "shared by two polygons of
  // different faces", which cf_geom_ tells us directly).
  int edge_cnt = 0;
  for (const auto& [edge, slots] : edge_slots) {
    if (edge_cnt >= LUMICE_MAX_CRYSTAL_EDGES) {
      break;
    }
    out->edges[edge_cnt * 2 + 0] = edge.first;
    out->edges[edge_cnt * 2 + 1] = edge.second;
    // n0 = normal of first adjacent face slot; n1 = normal of second (or same
    // as n0 for boundary edges — only possible in degenerate geometries).
    const float* n0 = g.face_normal + slots.slot_a * 3;
    const float* n1 = (slots.slot_b >= 0) ? (g.face_normal + slots.slot_b * 3) : n0;
    std::memcpy(&out->edge_face_normals[edge_cnt * 6 + 0], n0, 3 * sizeof(float));
    std::memcpy(&out->edge_face_normals[edge_cnt * 6 + 3], n1, 3 * sizeof(float));
    ++edge_cnt;
  }
  out->edge_count = edge_cnt;

  return LUMICE_OK;
}


// =============== Annotation Anchors ===============
// Bridge only: the geometry and the curve walk live in core/annotation_overlay.hpp. What is here
// is the ABI shape — validation, the enum translation, and the one heap allocation the C caller
// releases.

namespace {

// Everything LUMICE_AnnotationAnchors's pointers point into, kept in one object so a single
// Release frees the lot. Owned through the struct's opaque `storage` handle rather than through
// the individual pointers: the caller has one thing to release, and the released state is
// expressible (all pointers NULL) so a double Release is a no-op instead of a double free.
struct AnnotationStorage {
  std::vector<LUMICE_AnnotationLabel> labels;
  std::vector<LUMICE_AnnotationMarkerPoint> marker_points;
};

// A request angle list, validated and copied. Returns false with `err` set on a malformed list.
bool ReadAngleList(const float* data, int count, int cap, std::vector<float>* out, LUMICE_ErrorCode* err) {
  if (count < 0 || count > cap) {
    *err = LUMICE_ERR_INVALID_VALUE;
    return false;
  }
  if (count > 0 && data == nullptr) {
    *err = LUMICE_ERR_NULL_ARG;
    return false;
  }
  out->assign(data, data + count);
  return true;
}

// The marker id space is declared twice — as core's MarkerId enum and as the
// LUMICE_ANNOTATION_MARKER_* macros — because the C header cannot include the C++ one. This cast
// is the ONLY place the two meet, so the equality that makes it sound is asserted right here
// rather than described in a comment somewhere: reordering either side, or adding an id to one of
// them alone, becomes a compile error instead of a marker that silently resolves to the wrong
// direction. The count line is the half that catches an ADDITION (the per-id lines all still pass
// when a seventh id is appended on one side only).
static_assert(static_cast<int>(lumice::annotation::kMarkerZenith) == LUMICE_ANNOTATION_MARKER_ZENITH,
              "core MarkerId and LUMICE_ANNOTATION_MARKER_* have diverged");
static_assert(static_cast<int>(lumice::annotation::kMarkerNadir) == LUMICE_ANNOTATION_MARKER_NADIR,
              "core MarkerId and LUMICE_ANNOTATION_MARKER_* have diverged");
static_assert(static_cast<int>(lumice::annotation::kMarkerSun) == LUMICE_ANNOTATION_MARKER_SUN,
              "core MarkerId and LUMICE_ANNOTATION_MARKER_* have diverged");
static_assert(static_cast<int>(lumice::annotation::kMarkerSubsun) == LUMICE_ANNOTATION_MARKER_SUBSUN,
              "core MarkerId and LUMICE_ANNOTATION_MARKER_* have diverged");
static_assert(static_cast<int>(lumice::annotation::kMarkerAnthelion) == LUMICE_ANNOTATION_MARKER_ANTHELION,
              "core MarkerId and LUMICE_ANNOTATION_MARKER_* have diverged");
static_assert(static_cast<int>(lumice::annotation::kMarkerAntisolar) == LUMICE_ANNOTATION_MARKER_ANTISOLAR,
              "core MarkerId and LUMICE_ANNOTATION_MARKER_* have diverged");
static_assert(static_cast<int>(lumice::annotation::kMarkerCount) == LUMICE_ANNOTATION_MARKER_COUNT,
              "a marker id was added to one side of the C API boundary only");

// The label kinds cross the same boundary the same way (`dst.kind = static_cast<int>(l.kind)` in
// LUMICE_ComputeAnnotationAnchors) and had no such guard until the fifth family was added; a
// consumer that styles by kind would otherwise learn about a divergence from a mis-coloured label.
static_assert(static_cast<int>(lumice::annotation::kLabelHorizon) == LUMICE_ANNOTATION_HORIZON,
              "core LabelKind and LUMICE_ANNOTATION_* have diverged");
static_assert(static_cast<int>(lumice::annotation::kLabelElevation) == LUMICE_ANNOTATION_ELEVATION,
              "core LabelKind and LUMICE_ANNOTATION_* have diverged");
static_assert(static_cast<int>(lumice::annotation::kLabelLongitude) == LUMICE_ANNOTATION_LONGITUDE,
              "core LabelKind and LUMICE_ANNOTATION_* have diverged");
static_assert(static_cast<int>(lumice::annotation::kLabelAngularDist) == LUMICE_ANNOTATION_ANGULAR_DIST,
              "core LabelKind and LUMICE_ANNOTATION_* have diverged");
static_assert(static_cast<int>(lumice::annotation::kLabelViewDist) == LUMICE_ANNOTATION_VIEW_DIST,
              "core LabelKind and LUMICE_ANNOTATION_* have diverged");

// A request marker id list, validated and converted. Returns false with `err` set on a malformed
// list. The range check is not redundant with ResolveMarkerDir's own default branch: it is what
// turns a caller's bad id into a reported error instead of a silent fallback to the zenith.
bool ReadMarkerIdList(const int* data, int count, std::vector<lumice::annotation::MarkerId>* out,
                      LUMICE_ErrorCode* err) {
  if (count < 0 || count > LUMICE_MAX_ANNOTATION_MARKERS) {
    *err = LUMICE_ERR_INVALID_VALUE;
    return false;
  }
  if (count > 0 && data == nullptr) {
    *err = LUMICE_ERR_NULL_ARG;
    return false;
  }
  out->clear();
  out->reserve(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    if (!ns::capi::IsValidMarkerId(data[i])) {
      *err = LUMICE_ERR_INVALID_VALUE;
      return false;
    }
    out->push_back(static_cast<lumice::annotation::MarkerId>(data[i]));
  }
  return true;
}

}  // namespace


// Single owner of the LUMICE_AnnotationView -> lumice::annotation::ViewSnapshot field mapping
// (a56). Declared in c_api_internal.hpp so test/support/lumice_test_api.cpp's
// LUMICE_TEST_ComputeRenderDomainMask (the test-only door to the render-domain mask, which this
// product API no longer exports) can call it too instead of carrying a second hand-copied
// translation.
lumice::annotation::ViewSnapshot ToAnnotationViewSnapshot(const LUMICE_AnnotationView& v) {
  ns::annotation::ViewSnapshot view;
  view.width = v.width;
  view.height = v.height;
  view.lens_type = static_cast<ns::LensParam::LensType>(v.lens_type);
  view.fov_deg = v.lens_fov;
  view.lens_shift[0] = v.lens_shift[0];
  view.lens_shift[1] = v.lens_shift[1];
  view.overlap = v.overlap;
  view.az_deg = v.view_azimuth;
  view.el_deg = v.view_elevation;
  view.roll_deg = v.view_roll;
  view.visible = static_cast<ns::RenderConfig::VisibleRange>(v.visible);
  view.front = v.front != 0;
  return view;
}


LUMICE_ErrorCode LUMICE_ComputeAnnotationAnchors(const LUMICE_AnnotationRequest* request,
                                                 LUMICE_AnnotationAnchors* out) {
  if (!request || !out) {
    return LUMICE_ERR_NULL_ARG;
  }
  const LUMICE_AnnotationView& v = request->view;
  if (v.lens_type < 0 || v.lens_type > LUMICE_LENS_TYPE_GLOBE) {
    return LUMICE_ERR_INVALID_VALUE;
  }
  if (v.visible != LUMICE_VISIBLE_UPPER && v.visible != LUMICE_VISIBLE_LOWER && v.visible != LUMICE_VISIBLE_FULL) {
    return LUMICE_ERR_INVALID_VALUE;
  }

  lumice::annotation::Request req;
  req.view = ToAnnotationViewSnapshot(v);
  req.horizon = request->horizon != 0;
  req.reference_dir[0] = request->reference_dir[0];
  req.reference_dir[1] = request->reference_dir[1];
  req.reference_dir[2] = request->reference_dir[2];
  // The anchors are all this call computes, so the curve walk is always on; `labels` exists on the
  // core request for the CLI's mask-only calls.
  req.labels = true;

  LUMICE_ErrorCode err = LUMICE_OK;
  if (!ReadAngleList(request->elevation_deg, request->elevation_count, LUMICE_MAX_ANNOTATION_LINES, &req.elevation_deg,
                     &err) ||
      !ReadAngleList(request->longitude_deg, request->longitude_count, LUMICE_MAX_ANNOTATION_LINES, &req.longitude_deg,
                     &err) ||
      !ReadAngleList(request->angular_dist_deg, request->angular_dist_count, LUMICE_MAX_ANNOTATION_CIRCLES,
                     &req.angular_dist_deg, &err) ||
      !ReadAngleList(request->view_dist_deg, request->view_dist_count, LUMICE_MAX_ANNOTATION_CIRCLES,
                     &req.view_dist_deg, &err) ||
      !ReadMarkerIdList(request->marker_ids, request->marker_count, &req.markers, &err)) {
    return err;
  }

  std::unique_ptr<AnnotationStorage> storage;
  try {
    storage = std::make_unique<AnnotationStorage>();
    const lumice::annotation::Anchors anchors = lumice::annotation::ComputeAnchors(req);

    storage->labels.reserve(anchors.labels.size());
    for (const lumice::annotation::Label& l : anchors.labels) {
      LUMICE_AnnotationLabel dst{};
      dst.px = l.px;
      dst.py = l.py;
      dst.kind = static_cast<int>(l.kind);
      dst.index = l.index;
      dst.value_deg = l.value_deg;
      // Truncation cannot happen for any angle core formats (see LUMICE_ANNOTATION_LABEL_MAX), but
      // the copy is bounded anyway: a silently over-long text would otherwise be a buffer overrun
      // rather than a short label.
      const size_t n = std::min(l.text.size(), sizeof(dst.text) - 1);
      std::memcpy(dst.text, l.text.data(), n);
      dst.text[n] = '\0';
      storage->labels.push_back(dst);
    }

    storage->marker_points.reserve(anchors.markers.size());
    for (const lumice::annotation::CanvasPoint& p : anchors.markers) {
      storage->marker_points.push_back({ p.px, p.py, p.valid ? 1 : 0 });
    }
  } catch (...) {
    return LUMICE_ERR_UNKNOWN;
  }

  out->labels = storage->labels.empty() ? nullptr : storage->labels.data();
  out->label_count = static_cast<int>(storage->labels.size());
  out->marker_points = storage->marker_points.empty() ? nullptr : storage->marker_points.data();
  out->marker_count = static_cast<int>(storage->marker_points.size());
  out->storage = storage.release();
  return LUMICE_OK;
}


void LUMICE_ReleaseAnnotationAnchors(LUMICE_AnnotationAnchors* anchors) {
  if (!anchors || !anchors->storage) {
    return;  // NULL-safe, and idempotent on an already-released or zero-initialized struct
  }
  const std::unique_ptr<AnnotationStorage> owned(static_cast<AnnotationStorage*>(anchors->storage));
  anchors->storage = nullptr;
  // Leave no dangling view of freed memory behind, so a caller that keeps reading the struct after
  // Release sees "nothing here" rather than a use-after-free.
  anchors->labels = nullptr;
  anchors->label_count = 0;
  anchors->marker_points = nullptr;
  anchors->marker_count = 0;
}


// Normalize `in` into `out`. Shared by the two direction queries below so "a caller may pass an
// unnormalized sun_dir" is honoured in one place rather than in each of them.
// A zero-length vector is not an error here: it is handed to core as-is, whose two
// pole ids ignore it and whose sun-relative ones reflect it, and SunHorizonDir's own degenerate
// branch catches it — the same answer a near-pole sun gets.
static void NormalizeSunDir(const float in[3], float out[3]) {
  const float len = std::sqrt(in[0] * in[0] + in[1] * in[1] + in[2] * in[2]);
  if (len <= 0.0f) {
    out[0] = in[0];
    out[1] = in[1];
    out[2] = in[2];
    return;
  }
  out[0] = in[0] / len;
  out[1] = in[1] / len;
  out[2] = in[2] / len;
}

LUMICE_ErrorCode LUMICE_ResolveAnnotationMarkerDirection(int marker_id, const float sun_dir[3], float out_dir[3]) {
  if (!sun_dir || !out_dir) {
    return LUMICE_ERR_NULL_ARG;
  }
  // Not redundant with ResolveMarkerDir's own default branch: core answers an unknown id with the
  // zenith by design, and turning that into a REPORTED error is this boundary's job — the same
  // reasoning ReadMarkerIdList's range check carries.
  if (!ns::capi::IsValidMarkerId(marker_id)) {
    return LUMICE_ERR_INVALID_VALUE;
  }
  float unit[3];
  NormalizeSunDir(sun_dir, unit);
  lumice::annotation::ResolveMarkerDir(static_cast<lumice::annotation::MarkerId>(marker_id), unit, out_dir);
  return LUMICE_OK;
}


LUMICE_ErrorCode LUMICE_ResolveSunHorizonDirection(const float sun_dir[3], float out_dir[3]) {
  if (!sun_dir || !out_dir) {
    return LUMICE_ERR_NULL_ARG;
  }
  float unit[3];
  NormalizeSunDir(sun_dir, unit);
  lumice::annotation::SunHorizonDir(unit, out_dir);
  return LUMICE_OK;
}


// =============== Raypath Analysis Run ===============
// The C-side ROI modes and core's RaypathRoiMode are pinned to each other at the one place that
// converts between them, like the annotation marker ids above.
static_assert(static_cast<int>(ns::RaypathRoiMode::kFullSky) == LUMICE_RAYPATH_ROI_FULL_SKY,
              "LUMICE_RAYPATH_ROI_FULL_SKY drifted from RaypathRoiMode");
static_assert(static_cast<int>(ns::RaypathRoiMode::kInFrame) == LUMICE_RAYPATH_ROI_IN_FRAME,
              "LUMICE_RAYPATH_ROI_IN_FRAME drifted from RaypathRoiMode");
static_assert(static_cast<int>(ns::RaypathRoiMode::kCone) == LUMICE_RAYPATH_ROI_CONE,
              "LUMICE_RAYPATH_ROI_CONE drifted from RaypathRoiMode");
// The read-time symmetry bits are FilterConfig's, so a C caller's bit set is core's bit set.
static_assert(ns::FilterConfig::kSymP == LUMICE_RAYPATH_SYMMETRY_P, "LUMICE_RAYPATH_SYMMETRY_P drifted");
static_assert(ns::FilterConfig::kSymB == LUMICE_RAYPATH_SYMMETRY_B, "LUMICE_RAYPATH_SYMMETRY_B drifted");
static_assert(ns::FilterConfig::kSymD == LUMICE_RAYPATH_SYMMETRY_D, "LUMICE_RAYPATH_SYMMETRY_D drifted");
// The entry caps are sized to the data a run can produce (see the header): a segment is at most
// one crystal's max_hits faces, and the layer cap is the scene's scatter-layer cap by definition.
static_assert(LUMICE_MAX_RAYPATH_SEGMENT_LEN == ns::kMaxHits,
              "LUMICE_MAX_RAYPATH_SEGMENT_LEN must equal core's kMaxHits, or a legal chain gets truncated");
static_assert(LUMICE_MAX_RAYPATH_CHAIN_LAYERS == LUMICE_MAX_CONFIG_SCATTER_LAYERS, "one segment per scattering layer");

LUMICE_ErrorCode LUMICE_StartRaypathAnalysis(LUMICE_Server* server, const LUMICE_Scene* scene,
                                             const LUMICE_RaypathAnalysisRequest* request) {
  if (!server || !scene || !request) {
    return LUMICE_ERR_NULL_ARG;
  }
  ns::RaypathAnalysisRequest req;
  switch (request->roi_mode) {
    case LUMICE_RAYPATH_ROI_FULL_SKY:
      req.roi_.mode_ = ns::RaypathRoiMode::kFullSky;
      break;
    case LUMICE_RAYPATH_ROI_IN_FRAME:
      if (!ns::capi::AnnotationViewEnumsValid(request->frame_view) || request->frame_view.width <= 0 ||
          request->frame_view.height <= 0) {
        return LUMICE_ERR_INVALID_VALUE;
      }
      req.roi_.mode_ = ns::RaypathRoiMode::kInFrame;
      // The same view -> RenderConfig translation the annotation anchors use, so the frame the
      // consumer tests membership against is the frame the anchors were computed for.
      req.roi_.frame_config_ = ns::annotation::ToRenderConfig(ToAnnotationViewSnapshot(request->frame_view));
      break;
    case LUMICE_RAYPATH_ROI_CONE: {
      // Rejected here rather than degraded inside the consumer (which logs and counts nothing
      // for a bad radius, or recentres a zero vector on +z): a C caller gets a return code.
      const float* c = request->cone_center;
      const float len2 = c[0] * c[0] + c[1] * c[1] + c[2] * c[2];
      if (!(len2 > 0.0f) || !std::isfinite(len2) || !(request->cone_radius_rad > 0.0f) ||
          !std::isfinite(request->cone_radius_rad) || request->cone_ring_count < 1 ||
          request->cone_ring_count > LUMICE_MAX_RAYPATH_CONE_RINGS) {
        return LUMICE_ERR_INVALID_VALUE;
      }
      req.roi_.mode_ = ns::RaypathRoiMode::kCone;
      req.roi_.cone_center_[0] = c[0];
      req.roi_.cone_center_[1] = c[1];
      req.roi_.cone_center_[2] = c[2];
      req.roi_.cone_radius_rad_ = request->cone_radius_rad;
      req.roi_.cone_ring_count_ = request->cone_ring_count;
      break;
    }
    default:
      return LUMICE_ERR_INVALID_VALUE;
  }
  // The budget's three legal spellings; anything else is a return code, not a guess. The
  // sentinel is checked first so the boolean reading below never sees it.
  if (request->infinite == LUMICE_RAYPATH_RAY_BUDGET_SCENE_DEFAULT) {
    req.ray_num_ = std::nullopt;
  } else if (request->infinite == 0) {
    req.ray_num_ = static_cast<size_t>(request->ray_num);
  } else if (request->infinite == 1) {
    req.ray_num_ = ns::kInfSize;
  } else {
    return LUMICE_ERR_INVALID_VALUE;
  }
  // The record's capacity (v4.43): 0 is "the default", a value in range is that value, and
  // anything else — negative, or past the memory bound — is a return code, not a clamp. The
  // CLI rejects the same range on its own; this is the public boundary and does not lean on it.
  if (request->chain_capacity == 0) {
    req.chain_capacity_ = std::nullopt;
  } else if (request->chain_capacity >= 1 && request->chain_capacity <= LUMICE_MAX_RAYPATH_CHAIN_CAPACITY) {
    req.chain_capacity_ = static_cast<size_t>(request->chain_capacity);
  } else {
    return LUMICE_ERR_INVALID_VALUE;
  }
  // The same document LUMICE_CommitScene hands the server (scene->root): the two entry
  // points share one grammar and one parser, so a scene that commits analyses, and vice versa.
  const ns::Error err = server->server_->StartRaypathAnalysis(scene->root, req);
  return ns::capi::ToCApiErrorCode(err.code);
}


namespace {

// The legal read-time symmetry values are the bit sets of the three flags, and nothing else.
bool ChainIdSymmetryValid(int chain_id_symmetry) {
  constexpr int kAll = LUMICE_RAYPATH_SYMMETRY_P | LUMICE_RAYPATH_SYMMETRY_B | LUMICE_RAYPATH_SYMMETRY_D;
  return chain_id_symmetry >= 0 && chain_id_symmetry <= kAll;
}

}  // namespace


LUMICE_ErrorCode LUMICE_FrameGetRaypathAnalysisInfo(const LUMICE_ResultFrame* frame, int chain_id_symmetry,
                                                    LUMICE_RaypathAnalysisInfo* out) {
  if (!frame || !out) {
    return LUMICE_ERR_NULL_ARG;
  }
  if (!ChainIdSymmetryValid(chain_id_symmetry)) {
    return LUMICE_ERR_INVALID_VALUE;
  }
  std::memset(out, 0, sizeof(LUMICE_RaypathAnalysisInfo));
  const auto& result = frame->frame_->raypath_histogram_result_;
  if (!result.has_value()) {
    return LUMICE_OK;
  }
  out->present = 1;
  out->roi_mode = static_cast<int>(result->roi_mode_);
  // The merged row count under this symmetry — the same reduction the entry read performs
  // (and, through the frame's cache, the same instance of it), so the two agree by construction.
  const auto reduced = ns::ReducedRaypathHistogramOf(*frame->frame_, static_cast<uint8_t>(chain_id_symmetry));
  out->entry_count = static_cast<int>(reduced->entries_.size());
  out->cone_ring_count = result->cone_ring_count_;
  out->cone_radius_rad = result->cone_radius_rad_;
  out->snapshot_generation = frame->frame_->snapshot_generation_;
  // The bounded record's own account (v4.35), read off the reduction like entry_count: the
  // bucket and the truncation count are the same at every symmetry, the max row error is that
  // symmetry's (merging rows adds their errors).
  out->other_energy = reduced->other_energy_;
  out->other_count = static_cast<LUMICE_RayCount>(reduced->other_count_);
  out->truncated_chain_count = static_cast<int>(std::min<size_t>(reduced->truncated_chain_count_, INT_MAX));
  out->max_row_error = reduced->max_row_error_;
  return LUMICE_OK;
}


LUMICE_ErrorCode LUMICE_FrameGetRaypathAnalysis(const LUMICE_ResultFrame* frame, int chain_id_symmetry,
                                                LUMICE_RaypathHistogramEntry* out, int max_count) {
  if (!frame || !out) {
    return LUMICE_ERR_NULL_ARG;
  }
  if (!ChainIdSymmetryValid(chain_id_symmetry)) {
    return LUMICE_ERR_INVALID_VALUE;
  }
  const auto& result = frame->frame_->raypath_histogram_result_;
  int count = 0;
  bool truncated = false;
  if (result.has_value()) {
    // The frame holds the run's finest chains; what the caller reads is the reduction under the
    // symmetry it named, made here (raypath_histogram_consumer.hpp says how) — the frame itself
    // is never rewritten, so the next read under another symmetry starts from the same record.
    const auto reduced = ns::ReducedRaypathHistogramOf(*frame->frame_, static_cast<uint8_t>(chain_id_symmetry));
    const auto& entries = reduced->entries_;
    count = static_cast<int>(std::min<size_t>(entries.size(), static_cast<size_t>(std::max(max_count, 0))));
    for (int i = 0; i < count; i++) {
      const ns::RaypathHistogramEntry& src = entries[static_cast<size_t>(i)];
      LUMICE_RaypathHistogramEntry& dst = out[i];
      std::memset(&dst, 0, sizeof(dst));
      const size_t layers = std::min<size_t>(src.chain_.size(), LUMICE_MAX_RAYPATH_CHAIN_LAYERS);
      truncated = truncated || layers < src.chain_.size();
      for (size_t l = 0; l < layers; l++) {
        const ns::RaypathChainSegment& seg = src.chain_[l];
        dst.chain[l].crystal_id = static_cast<int>(seg.crystal_id);
        const size_t faces = std::min<size_t>(seg.segment.size(), LUMICE_MAX_RAYPATH_SEGMENT_LEN);
        truncated = truncated || faces < seg.segment.size();
        for (size_t f = 0; f < faces; f++) {
          dst.chain[l].segment[f] = static_cast<int>(seg.segment[f]);
        }
        dst.chain[l].segment_len = static_cast<int>(faces);
      }
      dst.chain_len = static_cast<int>(layers);
      // A byte copy of the server's one FormatRaypathChainDisplay() output, never re-assembled here
      // (lumice.h says why).
      const size_t n = std::min(src.display_.size(), sizeof(dst.display) - 1);
      truncated = truncated || n < src.display_.size();
      std::memcpy(dst.display, src.display_.data(), n);
      dst.display[n] = '\0';
      dst.energy = src.energy_;
      dst.count = static_cast<LUMICE_RayCount>(src.count_);
      dst.error_bound = src.error_bound_;
      const size_t rings = std::min<size_t>(src.ring_energy_.size(), LUMICE_MAX_RAYPATH_CONE_RINGS);
      // Cannot truncate: the request's ring count was capped at LUMICE_StartRaypathAnalysis.
      for (size_t r = 0; r < rings; r++) {
        dst.ring_energy[r] = src.ring_energy_[r];
      }
      dst.ring_count = static_cast<int>(rings);
    }
  }
  if (truncated) {
    // Once per read, not per entry: the caller sees a shorter chain than the run recorded, and
    // the record of that lives here rather than in a field the caller would have to know to check.
    ns::Logger logger("CAPI");
    ILOG_WARN(logger,
              "LUMICE_FrameGetRaypathAnalysis: at least one entry exceeded LUMICE_MAX_RAYPATH_CHAIN_LAYERS ({}) / "
              "LUMICE_MAX_RAYPATH_SEGMENT_LEN ({}) / LUMICE_RAYPATH_DISPLAY_MAX ({}) and was truncated",
              LUMICE_MAX_RAYPATH_CHAIN_LAYERS, LUMICE_MAX_RAYPATH_SEGMENT_LEN, LUMICE_RAYPATH_DISPLAY_MAX);
  }
  // Sentinel: written only when the array has room past the last entry (the unified contract).
  if (count < max_count) {
    std::memset(&out[count], 0, sizeof(LUMICE_RaypathHistogramEntry));
  }
  return LUMICE_OK;
}


LUMICE_ErrorCode LUMICE_UnprojectPixel(const LUMICE_AnnotationView* view, int px, int py, float out_dir[3],
                                       int* out_valid) {
  if (!view || !out_dir || !out_valid) {
    return LUMICE_ERR_NULL_ARG;
  }
  if (!ns::capi::AnnotationViewEnumsValid(*view) || view->width <= 0 || view->height <= 0) {
    return LUMICE_ERR_INVALID_VALUE;
  }
  *out_valid = 0;
  if (px < 0 || py < 0 || px >= view->width || py >= view->height) {
    return LUMICE_OK;  // not a pixel of this canvas
  }
  // The same three steps every mask and every annotation anchor of a view are built from —
  // ToRenderConfig, MakeCameraRotation, BuildProjParams — and then core's ONE per-pixel inverse.
  const ns::RenderConfig cfg = ns::annotation::ToRenderConfig(ToAnnotationViewSnapshot(*view));
  const ns::Rotation rot = ns::MakeCameraRotation(cfg);
  const float short_pix = static_cast<float>(std::min(cfg.resolution_[0], cfg.resolution_[1]));
  const lm_proj::ProjParams params = ns::BuildProjParams(cfg, rot, short_pix);
  const ns::mask_detail::MaskDir dir = ns::mask_detail::PixelToWorld(cfg, params, rot, px, py);
  if (!dir.valid) {
    return LUMICE_OK;
  }
  // Both display clips, exactly as the IN_FRAME membership test and the render-domain mask apply
  // them: a pixel the view does not show images no sky, so it is not a direction to point at.
  float forward[3];
  ns::mask_detail::CameraForward(rot, forward);
  if (!ns::mask_detail::VisibleByRange(cfg.visible_, dir.z) ||
      !ns::mask_detail::FrontVisible(cfg.front_, forward, dir.x, dir.y, dir.z)) {
    return LUMICE_OK;
  }
  out_dir[0] = dir.x;
  out_dir[1] = dir.y;
  out_dir[2] = dir.z;
  *out_valid = 1;
  return LUMICE_OK;
}

LUMICE_ErrorCode LUMICE_ProjectDirection(const LUMICE_AnnotationView* view, const float dir[3], float* out_px,
                                         float* out_py, int* out_valid) {
  if (!view || !dir || !out_px || !out_py || !out_valid) {
    return LUMICE_ERR_NULL_ARG;
  }
  if (!ns::capi::AnnotationViewEnumsValid(*view) || view->width <= 0 || view->height <= 0) {
    return LUMICE_ERR_INVALID_VALUE;
  }
  // Core's marker sampler on a caller-supplied direction: the projection, the canvas clamp and
  // the hemisphere slack are the ones LUMICE_ComputeAnnotationAnchors' marker points get.
  const ns::annotation::CanvasPoint p = ns::annotation::ProjectDirectionOnView(ToAnnotationViewSnapshot(*view), dir);
  *out_valid = p.valid ? 1 : 0;
  if (!p.valid) {
    return LUMICE_OK;
  }
  *out_px = p.px;
  *out_py = p.py;
  return LUMICE_OK;
}


// =============== Lens Type ===============
float LUMICE_MaxFov(LUMICE_LensType type) {
  return ns::MaxFov(static_cast<ns::LensParam::LensType>(type));
}

// =============== Color Conversion ===============
LUMICE_ErrorCode LUMICE_XyzToSrgbUint8(const float* xyz_in, unsigned char* out, int pixel_count,
                                       float intensity_scale) {
  if (!xyz_in || !out) {
    return LUMICE_ERR_NULL_ARG;
  }
  ns::XyzToSrgbUint8(xyz_in, out, pixel_count, intensity_scale);
  return LUMICE_OK;
}

LUMICE_ErrorCode LUMICE_XyzToSrgbUint8WithBackground(const float* xyz_in, unsigned char* out, int pixel_count,
                                                     float intensity_scale, const float* background_linear) {
  if (!xyz_in || !out || !background_linear) {
    return LUMICE_ERR_NULL_ARG;
  }
  ns::XyzToSrgbUint8(xyz_in, out, pixel_count, intensity_scale, background_linear);
  return LUMICE_OK;
}

// =============== EV Auto Anchor ===============
// Thin forwards to core/ev_anchor.hpp, the single owner of the anchor algorithm. The bare-float,
// no-NULL-check contract matches LUMICE_MaxFov and the precondition documented on the header
// declaration; it is the contract the GUI-side implementation these replaced already had.
float LUMICE_ComputeP99Y(const float* xyz_data, int img_width, int img_height, int downsample_factor) {
  return ns::ComputeP99Y(xyz_data, img_width, img_height, downsample_factor);
}

float LUMICE_ComputeEvAuto(float p99_raw_y, float snapshot_intensity, float target_white) {
  return ns::ComputeEvAuto(p99_raw_y, snapshot_intensity, target_white);
}
