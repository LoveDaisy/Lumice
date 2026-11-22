#include "gui/app.hpp"

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>  // NOLINT(misc-include-cleaner) — used by logger.hpp macros
#include <stb_image.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "gui/analysis_panel.hpp"
#include "gui/aspect_ratio_rules.hpp"
#include "gui/edit_modals.hpp"
#include "gui/export_fbo_renderer.hpp"
#include "gui/file_io.hpp"
#include "gui/gui_logger.hpp"
#include "gui/gui_state_reconcile.hpp"
#include "gui/mono_exposure_scale.hpp"
#include "gui/preview_downgrade.hpp"
#include "gui/theme.hpp"
#include "gui/user_defaults.hpp"
#include "gui/window_sizing.hpp"
#include "util/color_space.hpp"
#include "util/path_utils.hpp"
#include "util/result_frame.hpp"

namespace lumice::gui {

using SimState = GuiState::SimState;

// Global state definitions
GuiState g_state;
PreviewRenderer g_preview;
CrystalRenderer g_crystal_renderer;
ThumbnailCache g_thumbnail_cache;
LUMICE_Server* g_server = nullptr;
ServerPoller g_server_poller;
// The construction-time properties the live g_server was actually built with — the pair
// MaybeReconstructServerForConstructionProperties compares the requested values against. The zero
// values are what LUMICE_CreateServer() constructs (preferred_backend=CPU, num_workers=0); every
// server this file builds goes through ConstructServerForState, which writes both from the config
// it actually handed to the constructor. Any code that creates g_server directly (bypassing that
// function — the test harnesses) must put them back with ResetServerConstructionTrackers().
bool g_server_is_gpu = false;
int g_server_worker_count = 0;

float g_ui_scale_multiplier = kFactoryUiScaleMultiplier;
bool g_ui_scale_dirty = false;

void SetUiScaleMultiplierImmediate(float multiplier) {
  if (multiplier == g_ui_scale_multiplier) {
    return;
  }
  g_ui_scale_multiplier = multiplier;
  g_ui_scale_dirty = true;
}
PreviewViewport g_preview_vp;
BgColorPickState g_bg_pick;

// Async Stop plumbing (blueprint §5/§8, 1.6). DoStop offloads the blocking teardown sequence
// (`poller.Stop() + LUMICE_StopServer`) onto this future so the UI thread returns immediately and
// paints the optimistic "Stopping…" state. g_stop_inflight is the coarse completion latch read by
// SyncFromPoller to advance the intent to kStopped. Both live at file scope (single GUI thread
// owns the future; the atomic crosses the one background thread).
std::atomic<bool> g_stop_inflight{ false };

// task-color-degrade-gui-surfacing: epoch of the committed config for which the
// async GPU color-degrade modal has already been surfaced. The three GPU device
// caps (symmetry-group / OR-summand / color-class) fire on the worker's first
// batch — too late for DoRun's synchronous component check — so SyncFromPoller
// polls LUMICE_GetColorOverflowInfo each frame and warns once per committed
// epoch (a new Run mints a new epoch and re-arms, mirroring the DoRun modal's
// re-warn-on-Run policy). Sentinel ~0ull = nothing warned yet.
unsigned long long g_color_degrade_warned_epoch = ~0ull;

// Latch for the "GPU backend fell back to the CPU mid-run" modal. Unlike
// the color-degrade latch above, this condition has no committed-epoch key to dedup on
// — it survives CommitConfig within one run and clears only when the next Start()
// re-resolves the backend — so it is a plain rising-edge latch (see
// BackendFallbackWarningEdge). File scope for the same reason as its neighbour: the
// single GUI thread owns it, and no test drives SyncFromPoller.
bool g_backend_fallback_warned = false;

bool BackendFallbackWarningEdge(bool fell_back, bool& warned_latch) {
  if (!fell_back) {
    warned_latch = false;  // re-arm: the next fallback gets its own warning
    return false;
  }
  if (warned_latch) {
    return false;  // same fallback, already warned
  }
  warned_latch = true;
  return true;
}

namespace {
std::future<void> g_stop_future;
// The startup calibration run, once CalibrateQualityThreshold has handed it to a background thread
// (see RunCalibrationInBackground). Same single-owner shape as g_stop_future: valid() while the
// thread may still be using g_server, released by JoinPendingCalibration.
std::future<void> g_calibration_future;
}  // namespace

// Block until any in-flight async Stop has finished (poller + server drained), then release the
// future. Idempotent / cheap when no stop is pending. MUST be called before every path that
// destroys or reconstructs g_server / g_server_poller, so the background thread can never touch a
// freed server (R1 use-after-free guard): shutdown (main.cpp), DoRun top, and
// MaybeReconstructServerForConstructionProperties.
void JoinPendingStop() {
  if (g_stop_future.valid()) {
    g_stop_future.wait();
    g_stop_future = {};
  }
}

// The calibration twin of JoinPendingStop, with one more class of caller. Like the Stop, the
// calibration thread holds g_server, so every path that destroys or reconstructs the server joins
// it first (R1): shutdown (main.cpp), MaybeReconstructServerForConstructionProperties. Unlike the
// Stop, the calibration is also a RUN — the server is RUNNING the default document — while the
// poller has never been started: g_server_poller is the only publisher of what the preview shows,
// and the calibration never touches it, which is what keeps the warm-up frames off the screen.
// That holds only while nobody wakes the poller on the server mid-run, so every path that starts
// or wakes the poller must not observe a still-running calibration either. Two different
// mechanisms enforce that, split by whether blocking is acceptable: DoRun, DoStop and DoAnalyze
// are user-initiated commands where a bounded wait is expected, so they join here directly before
// touching the server. The two display-time refresh paths (PushDisplayState, the composite-EV
// push) are on the render path and must never block it for calibration's up-to-2s worst case
// (code review round 1, Major #3), so they use the non-blocking CalibrationPending() probe below
// instead and skip their whole push (not just the wake) while it reports true — they never call
// this function. A wake or a server write that slipped past either guard would have the poller
// observe the calibration's RUNNING server, or race its unsynchronized CommitConfig, respectively.
void JoinPendingCalibration() {
  if (g_calibration_future.valid()) {
    g_calibration_future.wait();
    g_calibration_future = {};
  }
}

// Non-blocking: true only while the background task is actually still running, not merely
// "launched and not yet joined". A finished-but-unjoined future means the server is already IDLE
// again — waking the poller then is harmless, there is nothing left to race — so this deliberately
// reports false in that case rather than reusing JoinPendingCalibration's coarser valid() check.
// Display-time refresh paths (which must never block the render thread waiting on join) use this
// to skip a wake while calibration is genuinely in flight instead of joining it (code review
// round 1, Major #3).
bool CalibrationPending() {
  return g_calibration_future.valid() &&
         g_calibration_future.wait_for(std::chrono::seconds(0)) != std::future_status::ready;
}

namespace {
WindowResizeEvents g_window_resize_events;
}

bool g_show_unsaved_popup = false;
PendingAction g_pending_action = PendingAction::kNone;

// task-cleanup-hardening AC4: Save-modified prompt state.
bool g_show_save_modified_popup = false;
PendingSaveKind g_pending_save_kind = PendingSaveKind::kNone;

bool g_show_export_overwrite_confirm_popup = false;
bool g_show_screenshot_export_options_popup = false;
ScreenshotExportSelection g_screenshot_export_selection;
std::filesystem::path g_pending_export_json_path;
std::string g_pending_export_json_content;

float ComputeGridStep(float fov) {
  // FOV here is full-angle in degrees (matches RenderConfig::fov / ViewProjection::fov).
  // Aim: ~4–6 grid lines across the visible FOV; round to a "nice" step.
  if (fov >= 120.0f)
    return 30.0f;
  if (fov >= 60.0f)
    return 20.0f;
  if (fov >= 30.0f)
    return 10.0f;
  if (fov >= 15.0f)
    return 5.0f;
  if (fov >= 6.0f)
    return 2.0f;
  if (fov >= 2.0f)
    return 1.0f;
  return 0.5f;
}

namespace {

// The shared body of the two expansions below: the angles g*step for g over an index range, with
// the g == 0 curve optionally left out. Written once because the two families differ only in that
// range and that flag — a second hand-rolled loop is a second place for the range to drift.
std::vector<float> GridAnglesOverIndexRange(float step, int index_lo, int index_hi, bool skip_zero) {
  std::vector<float> out;
  if (index_hi < index_lo) {
    return out;
  }
  out.reserve(static_cast<size_t>(index_hi - index_lo + 1));
  for (int g = index_lo; g <= index_hi; ++g) {
    if (skip_zero && g == 0) {
      continue;
    }
    out.push_back(static_cast<float>(g) * step);
  }
  return out;
}

}  // namespace

std::vector<float> ComputeGridElevationAngles(float step) {
  // ±80°, 0° excluded: the horizon owns that curve and carries its own colour.
  const int g_max = static_cast<int>(std::round(80.0f / step));
  return GridAnglesOverIndexRange(step, -g_max, g_max, /*skip_zero=*/true);
}

std::vector<float> ComputeGridLongitudeAngles(float step) {
  // (-180°, 180°]: starting one index above -g_max is what keeps the anti-meridian from being
  // emitted twice, once as -180° and once as +180°.
  const int g_max = static_cast<int>(std::round(180.0f / step));
  return GridAnglesOverIndexRange(step, -g_max + 1, g_max, /*skip_zero=*/false);
}

std::shared_ptr<ImGuiLogSink> g_imgui_log_sink;
std::shared_ptr<spdlog::sinks::basic_file_sink_mt> g_file_log_sink;
std::string g_log_file_path;

void GlfwErrorCallback(int error, const char* description) {
  GUI_LOG_ERROR("GLFW Error {}: {}", error, description);
}

namespace {

constexpr float kCollapsedStripWidth = 20.0f;  // Must match kCollapseBtnSize in app_panels.cpp.

float RequestedAspectRatio(AspectPreset preset, bool portrait, float override_ratio) {
  float ratio = 0.0f;
  if (preset == AspectPreset::kMatchBg) {
    if (!g_preview.HasBackground()) {
      return 0.0f;
    }
    ratio = override_ratio > 0.0f ? override_ratio : g_preview.GetBgAspect();
  } else {
    ratio = GetAspectRatio(preset);
  }
  return ApplyAspectOrientation(preset, portrait, ratio);
}

void UpdateAspectClampFromActualSize(int window_w, int window_h) {
  const float ratio = RequestedAspectRatio(g_state.aspect_preset, g_state.aspect_portrait, 0.0f);
  if (ratio <= 0.0f) {
    g_state.aspect_clamp = {};
    return;
  }
  const float left_w = UiPx(g_state.left_panel_collapsed ? kCollapsedStripWidth : kLeftPanelWidth);
  const float right_w = UiPx(g_state.right_panel_collapsed ? kCollapsedStripWidth : kRightPanelWidth);
  const AspectFitResult actual =
      MeasureAspectFit(window_w, window_h, ratio, left_w, right_w, UiPx(kTopBarHeight), UiPx(kStatusBarHeight));
  g_state.aspect_clamp.was_clamped = actual.was_clamped;
  g_state.aspect_clamp.requested_preview_ratio = actual.requested_preview_ratio;
  g_state.aspect_clamp.achieved_preview_ratio = actual.achieved_preview_ratio;
}

void SetProgrammaticWindowSize(GLFWwindow* window, int width, int height) {
  int actual_w = 0;
  int actual_h = 0;
  glfwGetWindowSize(window, &actual_w, &actual_h);
  if (actual_w == width && actual_h == height) {
    return;
  }
  BeginProgrammaticWindowResize(/*resize_expected=*/true);
  glfwSetWindowSize(window, width, height);
  EndProgrammaticWindowResize();
}

void ClampLiveWindowPosition(GLFWwindow* window, const WindowGeometryConstraints& constraints) {
  int pos_x = 0;
  int pos_y = 0;
  int win_w = 0;
  int win_h = 0;
  glfwGetWindowPos(window, &pos_x, &pos_y);
  glfwGetWindowSize(window, &win_w, &win_h);
  const WindowPosition clamped = ClampWindowPositionToWorkarea(pos_x, pos_y, win_w, win_h, constraints);
  if (clamped.x != pos_x || clamped.y != pos_y) {
    glfwSetWindowPos(window, clamped.x, clamped.y);
  }
}

}  // namespace

void WindowSizeCallback(GLFWwindow* /*window*/, int width, int height) {
  if (g_window_resize_events.RecordResize(width, height)) {
    UpdateAspectClampFromActualSize(width, height);
  }
}

void NotifyWindowContentScaleChanged() {
  g_window_resize_events.RecordContentScaleChange();
  g_ui_scale_dirty = true;
}

unsigned int WindowContentScaleRevision() {
  return g_window_resize_events.ContentScaleRevision();
}

void ResetWindowResizeEvents() {
  g_window_resize_events = {};
}

void BeginProgrammaticWindowResize(bool resize_expected) {
  g_window_resize_events.BeginRequest(resize_expected);
}

void EndProgrammaticWindowResize() {
  g_window_resize_events.EndRequest();
}

void FinishWindowEventPoll() {
  if (!g_window_resize_events.FinishEventPoll()) {
    return;
  }
  if (g_state.aspect_preset != AspectPreset::kFree) {
    g_state.aspect_preset = AspectPreset::kFree;
  }
  // Manual resize takes the user out of any preset, so the "screen too small"
  // warning is no longer meaningful — clear it so the GUI does not display a
  // stale clamp banner against a Free preset.
  g_state.aspect_clamp = {};
}

void ApplyAspectRatio(GLFWwindow* window, AspectPreset preset, bool portrait, float override_ratio) {
  const float ratio = RequestedAspectRatio(preset, portrait, override_ratio);
  if (ratio <= 0.0f) {
    // kFree (or any other ratio-less preset) reaches here — no preview-region
    // ratio to honor, so any prior clamp warning is no longer relevant.
    g_state.aspect_clamp = {};
    return;
  }

  int win_w = 0;
  int win_h = 0;
  glfwGetWindowSize(window, &win_w, &win_h);
  float left_w = UiPx(g_state.left_panel_collapsed ? kCollapsedStripWidth : kLeftPanelWidth);
  float right_w = UiPx(g_state.right_panel_collapsed ? kCollapsedStripWidth : kRightPanelWidth);
  const WindowGeometryConstraints constraints = GetCurrentWindowGeometryConstraints(window, CurrentUiScale());
  AspectFitResult fit =
      ResolveAspectFit(win_w, ratio, constraints, left_w, right_w, UiPx(kTopBarHeight), UiPx(kStatusBarHeight));
  int target_w = fit.target_w;
  int target_h = fit.target_h;

  SetProgrammaticWindowSize(window, target_w, target_h);
  ClampLiveWindowPosition(window, constraints);

  // GLFW documents the request as subject to window-manager limits. Read back what exists now and
  // let a delayed X11/Wayland callback update it again later; state never claims the request was
  // achieved merely because it was sent.
  int actual_w = 0;
  int actual_h = 0;
  glfwGetWindowSize(window, &actual_w, &actual_h);
  const AspectFitResult actual =
      MeasureAspectFit(actual_w, actual_h, ratio, left_w, right_w, UiPx(kTopBarHeight), UiPx(kStatusBarHeight));
  g_state.aspect_clamp.was_clamped = actual.was_clamped;
  g_state.aspect_clamp.requested_preview_ratio = actual.requested_preview_ratio;
  g_state.aspect_clamp.achieved_preview_ratio = actual.achieved_preview_ratio;
}

void ApplyWindowGeometryForScale(GLFWwindow* window, float layout_scale) {
  int cur_w = 0;
  int cur_h = 0;
  glfwGetWindowSize(window, &cur_w, &cur_h);
  const WindowGeometryConstraints constraints = GetCurrentWindowGeometryConstraints(window, layout_scale);
  const WindowSizePlan plan = PlanWindowSizeForScale(cur_w, cur_h, constraints);
  // Updating limits can itself synchronously resize Win32 windows, before SetWindowSize runs.
  BeginProgrammaticWindowResize(plan.resize);
  glfwSetWindowSizeLimits(window, plan.min_w, plan.min_h, GLFW_DONT_CARE, GLFW_DONT_CARE);
  if (plan.resize) {
    glfwSetWindowSize(window, plan.target_w, plan.target_h);
  }
  EndProgrammaticWindowResize();
  ClampLiveWindowPosition(window, constraints);
}

// The poller payload behind the texture currently on screen, retained by SyncFromPoller at the
// moment it uploads and released when the document changes (ResetFrontendState). It is what
// Save serializes, and holding it — rather than acquiring a fresh result frame at Save time — is
// what makes "the file holds the pixels that were on screen" true by construction: a fresh
// acquire runs DoSnapshot, which may publish a newer accumulation than the one the last poll
// uploaded, and the two would then differ by sampling noise. The payload's frame keeps its pixel
// storage alive for as long as the payload is held (server_poller.hpp), across further
// snapshots and across the server being stopped or destroyed, so this is a share of the frame's
// lifetime, not a borrow.
static std::shared_ptr<const TexturePayload> g_last_uploaded_payload;

// The most recent frame whose upload FAILED, held as the recovery ladder's retry source (814
// §3.2/§3.3). Its jobs, in the plan's own words: explicit retry uses "the retained newest
// qualifying source — including the terminal frame the backend already finished and the poller
// self-paused on, without re-running the simulation"; and a document whose first display failed
// still has a saveable complete source with clear provenance (Save reads the mirror first, then
// falls back to this — never the other way round, so a trusted displayed frame is never replaced
// by a newer never-shown one). Document-bound: cleared by ResetFrontendState on every non-Revert
// reason, and superseded the moment any upload succeeds (the displayed payload is then the
// authoritative source and this holder releases its frame).
static std::shared_ptr<const TexturePayload> g_pending_upload_source;
// The retained source's own poller serial, kept beside it: the retry's success commit must
// advance the exact-once cursor to THIS frame's serial (the incident record's serial is the
// FIRST failure's, which lags once later frames update the retained source).
static unsigned long long g_pending_upload_serial = 0;

// The recovery ladder's runtime state (814 §3.2/§3.4). Machine-resource memory, deliberately
// app-side rather than GuiState: it is runtime pause/strategy state like the latch, not document
// content, and its reset points are the ones this file already owns.
struct PreviewRecoveryState {
  // The tier established for ONGOING frames of the current generation: 0 = full + PBO (the
  // normal path), 1 = full texture with client-memory upload (no staging), >= 2 = prefiltered at
  // 2^(tier-1) both axes, still client-memory. A successful walk sets it, and following frames of
  // the same generation take ONE attempt at the established tier — never re-probing the bigger
  // candidates every frame, which on a constrained machine is exactly the repeated giant
  // allocation the latch exists to prevent. A new authorization (new run epoch, document switch,
  // explicit retry) walks the ladder afresh from candidate 0.
  int xyz_tier = 0;
  // The last authorized cycle reached the end of the ladder with nothing displayed. Auto-uploads
  // stay latched (the latch governs that); this flag is what the panel notice reports, and only
  // an explicit retry or a new authorization clears the situation it describes.
  bool exhausted = false;
  // One-time dismissible notice (814 AC3): dismissal only hides the notice — it never triggers a
  // retry, never fakes recovery, and a NEW failure re-shows it (notice_dismissed resets on every
  // incident).
  bool notice_dismissed = false;
  // The minimal incident record (814 §3.4), kept for the hardware-diagnostics export to read.
  // Every failed attempt updates it; a later success updates the display facts but never erases
  // the record — "后续成功不抹去首次失败因果链".
  bool has_incident = false;
  int stage = 0;  // UploadResult::Stage of the first failure of the last cycle
  int gl_error = 0;
  int request_width = 0;
  int request_height = 0;
  int tier_tried = 0;  // which ladder candidate was being attempted
  unsigned long long texture_serial = 0;
};
static PreviewRecoveryState g_recovery;

// ONE authorized XYZ upload cycle's outcome: the tier that displayed (>= 0), or -1 when every
// attempted candidate failed — plus the cycle's REAL first failure, the incident's causal head
// (stage, GL error and request size exactly as the failing candidate reported them). Callers log
// and report THIS result instead of synthesizing a placeholder: the r4 defect was a retry that
// fabricated stage=kInvalidInput 0x0 on top of the walk's concrete record, so the hardware
// report named an input problem where a texture OOM had happened. (Defined here because the
// open path calls the runner before its definition below.)
struct XyzUploadCycle {
  int displayed_tier = -1;
  UploadResult first_failure;
};
static XyzUploadCycle RunXyzUploadCycle(const float* xyz, int width, int height, unsigned long long texture_serial,
                                        bool probe_full_ladder);
// The incident record's single writer (defined beside the upload machinery); the open's uint8
// branches reach it through this declaration.
static void RecordUploadIncident(const UploadResult& result, int tier_tried, unsigned long long texture_serial);
// The effective composite preference (defined beside the mode adjudication below); the
// shared-context rebuild reads it from its earlier definition site.
static bool EffectiveShowCompositePreview();

// The FILE-side complete-source fact (814 §3.1/§3.3): a document opened from a .lmc carries its
// full source in the renderer's CPU mirror — the open writes those bytes whether or not the GPU
// accepted the display upload — and this struct is the metadata that travels with them: what the
// bytes mean (extent, mode) and, for XYZ, the exposure measurements the file stored. The mirror
// is the pixel holder; this struct never owns pixels. That is deliberate: an owning second copy
// of a full-size frame is exactly the resident backup the plan forbids on the constrained
// machines this task serves, and the mirror already exists. It also retires the r4 defect where
// the failed open wrapped the CALLER'S stack buffer in a payload whose views nobody owned: the
// file's retry source is now the document's own mirror, never a borrowed pointer.
//
// Lifetime contract: valid for the current document only — ResetFrontendState clears it on every
// non-Revert reason (Revert is a config restore of the same document). While it is valid, the
// only mirror writers left are the open itself and RefreshCpuTextureForSave writing from a
// payload or the pending source — content of THIS document, never another frame's pixels — so a
// retry walking against the mirror cannot observe a mid-document mutation of someone else's
// bytes, and nothing here can outlive the ClearTexture that pairs with the fact reset.
struct FilePreviewSource {
  bool valid = false;
  int width = 0;
  int height = 0;
  PreviewRenderer::TextureMode mode = PreviewRenderer::TextureMode::kSrgbComposited;
  // XYZ only (zero for the two baked uint8 modes: their exposure lives in the bytes).
  float snapshot_intensity = 0;
  float emitted_energy = 0;
  float mono_anchor = 0;
  int effective_pixels = 0;
};
static FilePreviewSource g_file_source;

// The ONE adjudication of "what is this document's complete source" (814 §3.1; review r4: save,
// the extent/exposure queries and the degraded-export route each re-derived it, with opposite
// priorities — which is how a failed newer frame ended up deciding the absolute-EV denominator
// of an older DISPLAYED file frame). One resolver, one priority:
//
//   1. the accepted live payload — the frame the screen was built from;
//   2. the file source — the document's own bytes in the CPU mirror (an opened document);
//   3. the pending source — the newest frame that never displayed (first-failure fallback).
//
// Consumers: RefreshCpuTextureForSave (what Save writes), PreviewSourceExtent/PixelCount (the
// absolute-EV denominator and the degraded-export size), RenderExportFromFullSource (the pixels
// a full-size export renders), and the retry target (which frame a recovery cycle walks).
struct ResolvedPreviewSource {
  enum class Role {
    kNone,                // nothing holds this document's source
    kAcceptedLive,        // payload of the displayed frame
    kFileMirror,          // the open's own bytes in the CPU mirror
    kPendingUndisplayed,  // retained frame that never displayed
  };
  Role role = Role::kNone;
  std::shared_ptr<const TexturePayload> payload;  // the live roles; empty for kFileMirror
  // kFileMirror: a view into the renderer's CPU mirror, valid on the main thread for this
  // document's lifetime — no consumer retains it across a mirror write. A live payload carries
  // xyz_pixels in BOTH display shapes (mono and composite) plus rgb_pixels when composite.
  const float* xyz_pixels = nullptr;
  const unsigned char* rgb_pixels = nullptr;
  int width = 0;
  int height = 0;
  // Exposure measurements (meaningful for XYZ sources; zeros for the baked uint8 modes).
  float snapshot_intensity = 0;
  float emitted_energy = 0;
  float mono_anchor = 0;
  int effective_pixels = 0;
  unsigned long long serial = 0;  // live: the frame's poller serial; file: 0 (a file has none)
};

static ResolvedPreviewSource ResolvePreviewSource() {
  ResolvedPreviewSource out;
  // Priority 1: the accepted live payload. A payload whose pixel views are empty cannot name a
  // source (the acceptance-leg retained copy may carry none) — fall through, never trust it.
  const auto fill_from_payload = [&out](const TexturePayload& p, ResolvedPreviewSource::Role role,
                                        unsigned long long serial) {
    out.role = role;
    out.width = p.width;
    out.height = p.height;
    out.xyz_pixels = p.xyz_buffer;
    out.rgb_pixels = p.rgb_buffer;
    out.snapshot_intensity = p.snapshot_intensity;
    out.emitted_energy = p.emitted_energy;
    out.mono_anchor = p.axis_solid_angle * p.anchor_l99_sky;
    out.effective_pixels = p.effective_pixels;
    out.serial = serial;
  };
  if (g_last_uploaded_payload != nullptr &&
      (g_last_uploaded_payload->xyz_buffer != nullptr || g_last_uploaded_payload->rgb_buffer != nullptr) &&
      g_last_uploaded_payload->width > 0 && g_last_uploaded_payload->height > 0) {
    fill_from_payload(*g_last_uploaded_payload, ResolvedPreviewSource::Role::kAcceptedLive,
                      g_state.last_uploaded_texture_serial);
    out.payload = g_last_uploaded_payload;
    return out;
  }
  // Priority 2: the document's own file bytes. The facts and the mirror were written together by
  // the open; if the mirror view is gone anyway the state is inconsistent and resolves as
  // nothing rather than as a half-named source.
  if (g_file_source.valid) {
    const float* xyz =
        g_file_source.mode == PreviewRenderer::TextureMode::kXyz ? g_preview.GetXyzTextureData() : nullptr;
    const unsigned char* rgb =
        g_file_source.mode != PreviewRenderer::TextureMode::kXyz ? g_preview.GetTextureData() : nullptr;
    if (xyz != nullptr || rgb != nullptr) {
      out.role = ResolvedPreviewSource::Role::kFileMirror;
      out.width = g_file_source.width;
      out.height = g_file_source.height;
      out.xyz_pixels = xyz;
      out.rgb_pixels = rgb;
      out.snapshot_intensity = g_file_source.snapshot_intensity;
      out.emitted_energy = g_file_source.emitted_energy;
      out.mono_anchor = g_file_source.mono_anchor;
      out.effective_pixels = g_file_source.effective_pixels;
      return out;
    }
  }
  // Priority 3: the newest frame that never displayed.
  if (g_pending_upload_source != nullptr &&
      (g_pending_upload_source->xyz_buffer != nullptr || g_pending_upload_source->rgb_buffer != nullptr) &&
      g_pending_upload_source->width > 0 && g_pending_upload_source->height > 0) {
    fill_from_payload(*g_pending_upload_source, ResolvedPreviewSource::Role::kPendingUndisplayed,
                      g_pending_upload_serial);
    out.payload = g_pending_upload_source;
    return out;
  }
  return out;  // kNone
}

// Refresh the renderer's CPU-side mirror with the linear XYZ energy behind the picture on screen,
// so SaveLmcFile writes it out. No exposure, no sky, no vignetting: all three are settings the
// document's JSON carries and the preview shader applies at display time, for a reopened document
// through the same kTexModeXyz branch as for a live one — which is exactly what lets the EV
// slider, the exposure mode, the background and print mode keep working on the reopened picture,
// and what removes the bake-then-clip step whose clipping used to blur a hot spot the live view
// showed sharp (bilinear on clipped bytes vs bilinear on unclipped energy). The exposure
// measurements travel alongside (XyzTextureMeta) so ComputeMonoExposure has its denominators back
// on reopen; they are read off the retained payload, i.e. off the frame the pixels belong to,
// through the same expression SyncFromPoller used to fill GuiState from it.
//
// Unconditional, with no visibility gate: this buffer is not a view of the sky, it is the
// fixed full-sky dual-fisheye SOURCE the preview re-projects. The gate the shader applies
// (result.w >= 0.5 && pixel_visible) is a screen-space property of the current lens / fov /
// orientation, which this buffer has none of — every legal world direction lands inside one of
// its two discs, and the packing's corner padding is never sampled back out.
//
// Mono XYZ always, even while the composite (raypath-colour) preview is showing: the payload
// carries xyz_buffer in both cases, and the composite colouring is a display-time rule the
// document's JSON re-derives. Same scope the sRGB bake this replaced had.
static void RefreshCpuTextureForSave() {
  if (g_state.sim_state == SimState::kIdle) {
    return;  // No simulation data to refresh; the mirror keeps whatever Open last put there
  }
  // One adjudication for every source consumer (see ResolvePreviewSource): a trusted DISPLAYED
  // frame wins over a newer never-shown one, and the CURRENT document's own file source wins
  // over a failed live frame — a reopened document's mirror already holds its file's bytes, so a
  // live failure's retained retry source must not overwrite what is on screen and saveable right
  // now. The mirror being non-empty is NOT what keeps the file bytes in place; the RESOLVER's
  // file role is. That is the r4 distinction: a mirror written by an earlier fallback save (a
  // first-failure frame) must not outrank the newest retained source, and a mirror holding file
  // bytes must not lose them to one either.
  const ResolvedPreviewSource src = ResolvePreviewSource();
  if (src.role != ResolvedPreviewSource::Role::kAcceptedLive &&
      src.role != ResolvedPreviewSource::Role::kPendingUndisplayed) {
    // kNone: nothing holds a source to refresh from. kFileMirror: the mirror already holds
    // exactly the bytes Save must write — the open wrote them — nothing to refresh.
    return;
  }
  const TexturePayload& payload = *src.payload;
  if (payload.xyz_buffer == nullptr || payload.width <= 0 || payload.height <= 0) {
    return;
  }
  if (src.role == ResolvedPreviewSource::Role::kPendingUndisplayed) {
    GUI_LOG_INFO(
        "[GUI] Save: the preview never displayed this document's frame (upload failed); saving the "
        "retained source, not a displayed picture");
  }
  XyzTextureMeta meta;
  meta.snapshot_intensity = payload.snapshot_intensity;
  meta.emitted_energy = payload.emitted_energy;
  meta.mono_anchor = payload.axis_solid_angle * payload.anchor_l99_sky;
  meta.effective_pixels = payload.effective_pixels;
  g_preview.UpdateCpuXyzTextureData(payload.xyz_buffer, payload.width, payload.height, meta);
}

// task-cleanup-hardening AC4: PerformSave / PerformSaveAs are the actual
// serialization body (existing DoSave/DoSaveAs pre-353.5). DoSave / DoSaveAs
// now front them with a sim_state == kModified check that opens
// RenderSaveModifiedPopup; PerformSave / PerformSaveAs are called directly
// only by that popup's "Save anyway" branch and by DoSave/DoSaveAs' own
// non-kModified fallthrough below (code-review-01 M1: RenderUnsavedPopup no
// longer bypasses the gate — it now calls DoSave()/DoSaveAs()).
void PerformSave() {
  if (g_state.current_file_path.empty()) {
    g_state.current_file_path = ShowSaveDialog();
    if (g_state.current_file_path.empty()) {
      return;
    }
  }
  RefreshCpuTextureForSave();
  if (SaveLmcFile(g_state.current_file_path, g_state, g_preview, g_state.save_texture)) {
    g_state.dirty = false;
    GUI_LOG_INFO("[GUI] DoSave: {}", PathToU8(g_state.current_file_path));
  }
}

void PerformSaveAs() {
  auto path = ShowSaveDialog();
  if (!path.empty()) {
    g_state.current_file_path = path;
    RefreshCpuTextureForSave();
    if (SaveLmcFile(path, g_state, g_preview, g_state.save_texture)) {
      g_state.dirty = false;
      GUI_LOG_INFO("[GUI] DoSaveAs: {}", PathToU8(path));
    }
  }
}

void DoSave() {
  // Gate on kModified: the last committed run does not reflect the current
  // config, so the on-screen preview being about to serialize is a snapshot
  // of a stale config. Rather than silently freeze it and clear Modified,
  // prompt: Run first / Save anyway / Cancel.
  if (g_state.sim_state == GuiState::SimState::kModified) {
    g_show_save_modified_popup = true;
    g_pending_save_kind = PendingSaveKind::kSave;
    return;
  }
  PerformSave();
}

void DoSaveAs() {
  if (g_state.sim_state == GuiState::SimState::kModified) {
    g_show_save_modified_popup = true;
    g_pending_save_kind = PendingSaveKind::kSaveAs;
    return;
  }
  PerformSaveAs();
}

// Build PreviewParams for export: copy current preview viewport params, but override
// intensity_* fields to sample the GUI EV slider live (the viewport copy carries whatever the
// last rendered frame used, which is a frame behind a just-moved slider). The exposure itself
// comes from mono_exposure_scale.hpp, so an export lands at the brightness the preview shows
// under either mode.
static PreviewParams BuildExportParams() {
  PreviewParams params = g_preview_vp.params;
  lumice::gui::MonoExposureInput ev_in;
  ev_in.exposure_offset = g_state.renderer.exposure_offset;
  ev_in.ev_auto = g_state.ev_auto;
  ev_in.snapshot_intensity = g_state.snapshot_intensity;
  ev_in.snapshot_emitted_energy = g_state.snapshot_emitted_energy;
  ev_in.total_pixels = PreviewSourcePixelCount();
  const lumice::gui::MonoExposure ev = lumice::gui::ComputeMonoExposure(g_state.renderer.ev_mode, ev_in);
  params.exposure.intensity_factor = ev.intensity_factor;
  params.exposure.intensity_scale = ev.intensity_scale;
  return params;
}

void DoExportPreviewPng() {
  g_screenshot_export_selection = MakeScreenshotExportSelectionFromState(g_state);
  g_show_screenshot_export_options_popup = true;
}

// The all-sky exports' degraded-display route (plan 814 §3.3): when the on-screen texture is a
// reduced recovery tier, the export must still be the ORIGINAL-size picture built from the FULL
// source — upsampling the reduced texture would ship a blurred file labelled at full resolution.
// The full source renders through a transient renderer (its own texture, quad and programs,
// destroyed at return): a one-off allocation at export time, never a persistent second
// full-size plane, and a failure of any stage inside it is EXPLICIT — empty return, error log, no
// file written — never a truncated or pseudo-full-resolution delivery. Returns empty both when
// the display is not degraded (caller uses the normal path) and when the degraded route failed
// (callers distinguish via IsDisplayDegradedAgainstSource).
static bool IsDisplayDegradedAgainstSource() {
  const int disp_w = g_preview.GetTextureWidth();
  const int disp_h = g_preview.GetTextureHeight();
  return disp_w > 0 && disp_h > 0 && static_cast<long>(disp_w) * disp_h < PreviewSourcePixelCount();
}

// One authorized recovery cycle against the resolver's source, run after a SHARED-CONTEXT OOM
// revoked the preview's sampling资格 (see RenderExportFromFullSource). Either a smaller trusted
// tier is re-established, or the explicit exhausted/no-image state stands — never continued
// sampling of a texture nothing can vouch for. Bounded: one cycle, the same machinery a live
// failure uses.
//
// The rebuilt display is the display that was ACCEPTED, not whatever the full source could
// provide (the r4 defect): an accepted composite live frame re-uploads its composite bake — a
// single plain re-upload, the ladder's linear-radiance low-pass does not apply to it — while a
// document's own file re-uploads in its own TextureMode, and only an XYZ source walks the
// ladder. The mode verdict is ONE adjudication for the upload selection and the success commit,
// the same contract SyncFromPoller and Retry run under; a rebuild that silently switched the
// display to XYZ under a composite record would show a picture the next sync's serial/mode
// gates would never correct (the terminal frame's serial did not change).
static void RebuildPreviewAfterContextOom() {
  const ResolvedPreviewSource src = ResolvePreviewSource();
  if (src.role == ResolvedPreviewSource::Role::kNone || src.width <= 0 || src.height <= 0) {
    return;  // nothing holds a rebuildable source; the no-image state stands
  }
  bool use_composite = false;
  switch (src.role) {
    case ResolvedPreviewSource::Role::kAcceptedLive:
    case ResolvedPreviewSource::Role::kPendingUndisplayed:
      use_composite = src.payload != nullptr &&
                      ShouldUseCompositeUpload(src.payload->is_composite, EffectiveShowCompositePreview());
      break;
    case ResolvedPreviewSource::Role::kFileMirror:
      use_composite = g_file_source.mode == PreviewRenderer::TextureMode::kSrgbComposited;
      break;
    case ResolvedPreviewSource::Role::kNone:
      break;
  }
  UploadResult upload_result;
  int tier = -1;
  if (src.role == ResolvedPreviewSource::Role::kFileMirror) {
    // The document's own bytes re-upload in their own mode (the r4 gap: a uint8 file used to
    // fall out of the rebuild with the display revoked and no exhausted flag, no incident and
    // no retry exit — a healthy tier-0 document stranded without a picture and without a way
    // back). The pixels come from the CPU mirror, where the open wrote them.
    switch (g_file_source.mode) {
      case PreviewRenderer::TextureMode::kXyz: {
        const XyzUploadCycle cycle =
            RunXyzUploadCycle(src.xyz_pixels, src.width, src.height, /*texture_serial=*/0, /*probe_full_ladder=*/true);
        tier = cycle.displayed_tier;
        upload_result = cycle.first_failure;
        upload_result.ok = tier >= 0;
        break;
      }
      case PreviewRenderer::TextureMode::kSrgbRadiance:
        upload_result = g_preview.UploadRadianceTexture(src.rgb_pixels, src.width, src.height);
        if (!upload_result.ok) {
          RecordUploadIncident(upload_result, /*tier_tried=*/0, /*texture_serial=*/0);
        }
        break;
      case PreviewRenderer::TextureMode::kSrgbComposited:
        upload_result = g_preview.UploadTexture(src.rgb_pixels, src.width, src.height);
        if (!upload_result.ok) {
          RecordUploadIncident(upload_result, /*tier_tried=*/0, /*texture_serial=*/0);
        }
        break;
    }
  } else if (use_composite && src.rgb_pixels != nullptr) {
    // An accepted composite keeps its own display semantics: one plain re-upload of the bake,
    // success or the same incident any other single-shot failure lands.
    upload_result = g_preview.UploadRadianceTexture(src.rgb_pixels, src.width, src.height);
    if (!upload_result.ok) {
      RecordUploadIncident(upload_result, /*tier_tried=*/0, src.serial);
    }
  } else if (src.xyz_pixels != nullptr) {
    // One authorized XYZ walk; the cycle records its own real first failure.
    const XyzUploadCycle cycle =
        RunXyzUploadCycle(src.xyz_pixels, src.width, src.height, src.serial, /*probe_full_ladder=*/true);
    tier = cycle.displayed_tier;
    upload_result = cycle.first_failure;
    upload_result.ok = tier >= 0;
  } else {
    return;  // nothing uploadable in the resolved source's shape; the no-image state stands
  }
  if (!upload_result.ok) {
    // The rebuild ran out: the explicit unavailable state stands, and the generation latch
    // SyncFromPoller arms goes up too — a live run's next frames must not self-authorize
    // another walk at the poll rate.
    g_recovery.exhausted = true;
    g_state.upload_latched = true;
    g_state.upload_latch_epoch = g_state.committed_epoch;
    GUI_LOG_ERROR(
        "[GUI] Export: the preview could not be rebuilt after the export's out-of-memory (stage={} "
        "gl_error=0x{:04X} request={}x{}) — preview unavailable, source retained, auto-upload latched",
        UploadStageName(upload_result.stage), upload_result.gl_error, upload_result.request_width,
        upload_result.request_height);
    return;
  }
  // Success. The accepted source stays accepted — the exact-once cursor is untouched, because
  // the display describes the same frame it described before the revoke (moving the cursor
  // would un-dedup the poller's stream). The display-mode record, the counters and the recovery
  // state describe the picture re-accepted above; the tier memory follows the same rule as
  // Retry (a plain single-shot re-upload lands at 0, a ladder walk at the tier it established).
  g_state.last_uploaded_as_composite = use_composite;
  g_recovery.xyz_tier = tier >= 0 ? tier : 0;
  g_recovery.exhausted = false;
  g_state.texture_upload_count++;
  g_state.upload_latched = false;
  if (src.role == ResolvedPreviewSource::Role::kFileMirror && !use_composite) {
    // The file's XYZ exposure facts: the same commit a Retry of this source makes. A failed
    // open never committed them, so without this the rebuilt display could be lit off the
    // previous document's stale state.
    g_state.snapshot_intensity = g_file_source.snapshot_intensity;
    g_state.snapshot_emitted_energy = g_file_source.emitted_energy;
    g_state.effective_pixels = g_file_source.effective_pixels;
    g_state.p99_raw_y = g_file_source.mono_anchor;
    g_state.ev_auto = LUMICE_ComputeEvAuto(g_state.p99_raw_y, g_state.snapshot_intensity, g_state.target_white);
  }
  GUI_LOG_INFO("[GUI] Export: the preview was rebuilt ({}x{}, {}) after the export's out-of-memory", src.width,
               src.height, use_composite ? "composite" : (tier >= 1 ? "xyz, recovery tier" : "xyz"));
}

// The export TARGET's own allocation failure enters the retained incident record (the r2 Minor:
// a first-candidate rebuild that succeeded used to erase the fact that a target allocation had
// OOM'd at all — the log carried it, but the structured record the diagnostics read saw no
// incident, or an older unrelated one). Recorded by the same app owner that observed the
// shared-context OOM, BEFORE the rebuild cycle runs, so a successful rebuild cannot mask the
// trigger; a later rebuild failure legitimately overwrites it with the newer real cause.
static void RecordExportTargetOomIncident(int target_w, int target_h) {
  UploadResult r;
  r.stage = UploadResult::Stage::kRenderTargetAllocation;
  r.gl_error = 0x0505;
  r.request_width = target_w;
  r.request_height = target_h;
  RecordUploadIncident(r, /*tier_tried=*/0, g_state.last_uploaded_texture_serial);
}

static std::vector<unsigned char> RenderExportFromFullSource(const PreviewParams& params, int out_w, int out_h) {
  // The resolver names the same source a Save of this document would write (one adjudication,
  // review r4); the export reads its pixels directly instead of round-tripping them through the
  // CPU mirror.
  const ResolvedPreviewSource src = ResolvePreviewSource();
  if (src.xyz_pixels == nullptr || src.width <= 0 || src.height <= 0) {
    // A degraded display over a source with no linear floats (the two baked uint8 modes) has
    // nothing to rebuild from. Explicit refusal, not a guess: upscaling the bake would be the
    // exact pseudo-full-resolution delivery the plan forbids.
    GUI_LOG_ERROR(
        "[GUI] Export: the display is degraded but the document's source carries no linear XYZ; "
        "refusing to upscale a baked source");
    return {};
  }
  // The transient renderer lives in its own scope so that a shared-context OOM can end the
  // OBJECT — not just its GL names — before the recovery cycle walks (the r4 release-timing
  // ruling: the retired resource leaves before the next strategy allocates, and the rebuild
  // must not run while the failed export's renderer still exists to hold it).
  //
  // GL cleanup is UNCONDITIONAL over every exit past construction (the r2 Major: the destructor
  // maintains only the test counters — scope exit alone never deletes the GL objects, so a
  // successful degraded export used to leave the full-size source texture and its programs
  // behind, one set per export). The OOM branches destroy EARLY — the rebuild must not walk
  // against the transient's storage — and the guard's second pass there is a guarded no-op.
  bool shared_context_oom = false;
  std::vector<unsigned char> rgba;
  {
    PreviewRenderer export_renderer;
    struct ScopedGlCleanup {
      PreviewRenderer* renderer;
      ~ScopedGlCleanup() { renderer->Destroy(); }
    } scoped_cleanup{ &export_renderer };
    if (!export_renderer.Init()) {
      // Init's own partial-failure cleanup has already freed what it built; the guard's
      // Destroy is a guarded no-op on the remainder.
      GUI_LOG_ERROR("[GUI] Export: the transient full-source renderer failed to initialize");
      return {};
    }
    const UploadResult upload = export_renderer.UploadXyzTextureNoStaging(src.xyz_pixels, src.width, src.height);
    if (!upload.ok) {
      GUI_LOG_ERROR(
          "[GUI] Export: uploading the full source failed (stage={} gl_error=0x{:04X} request={}x{}) — no "
          "file written",
          UploadStageName(upload.stage), upload.gl_error, upload.request_width, upload.request_height);
      if (upload.stage == UploadResult::Stage::kTexImage && upload.gl_error == 0x0505) {
        // A REAL out-of-memory is a SHARED-CONTEXT fact, not the transient renderer's private
        // one (GL spec §2.5, the ruling this file's own upload path already applies): the
        // transient object's conservative revoke answers its own storage, not whether the
        // preview's texture is still provable. Stop sampling the unprovable display, then
        // rebuild one authorized cycle from the retained source — a smaller trusted tier or
        // the explicit no-image state. The transient's storage is released first: the early
        // Destroy below runs before the scope exit that precedes the rebuild.
        export_renderer.Destroy();
        RecordUploadIncident(upload, /*tier_tried=*/0, g_state.last_uploaded_texture_serial);
        shared_context_oom = true;
      }
    } else {
      int render_gl_error = 0;
      rgba = RenderExportToRgba(export_renderer, params, out_w, out_h, /*curve_labels=*/{}, /*dpi_scale_x=*/1.0f,
                                /*dpi_scale_y=*/1.0f, &render_gl_error);
      if (rgba.empty()) {
        GUI_LOG_ERROR(
            "[GUI] Export: rendering the full source failed (target {}x{} gl_error=0x{:04X}) — no "
            "file written",
            out_w, out_h, render_gl_error);
        if (render_gl_error == 0x0505) {
          // The export TARGET's allocation (a full-size RGBA8 renderbuffer) hit a real
          // out-of-memory in the shared context — the same §2.5 ruling as the source-upload
          // path above: the preview stops being sampled until a verified rebuild stands.
          // Same order: the early Destroy runs before the scope exit that precedes the
          // rebuild.
          export_renderer.Destroy();
          RecordExportTargetOomIncident(out_w, out_h);
          shared_context_oom = true;
        }
      }
    }
  }  // the guard's unconditional Destroy — and the destructor — run here, before the revoke
  if (shared_context_oom) {
    g_preview.RevokeDisplayForContextOom();
    RebuildPreviewAfterContextOom();
    return {};
  }
  return rgba;
}

ScreenshotFrameFacts CurrentScreenshotFrameFacts() {
  ScreenshotFrameFacts facts;
  facts.has_background = g_preview.HasBackground();
  facts.payload_is_composite = g_last_uploaded_payload != nullptr && g_last_uploaded_payload->is_composite;
  return facts;
}

ScreenshotRender RenderScreenshot(const ScreenshotExportSelection& sel) {
  PreviewParams params = BuildExportParams();
  ApplyScreenshotExportSelectionToParams(sel, g_state, CurrentScreenshotFrameFacts(), params);

  int w = g_preview_vp.vp_w;
  int h = g_preview_vp.vp_h;
  // The FBO is the live viewport's DEVICE pixel size, so the exported PNG is the full-resolution
  // twin of what is on screen (on a Retina display, twice the width of the preview area — that is
  // correct and deliberate, not a bug to normalize away).
  //
  // The anchors, though, are built at the viewport's LOGICAL POINT size and the DPI is handed to
  // RenderExportToRgba separately, because that is the space a draw list works in and the space
  // the on-screen preview publishes its own sets in (app_panels.cpp). Building them at w/h and
  // leaving the scale at 1 is self-consistent for POSITIONS and silently halves the TEXT, which is
  // the defect this alignment removes: the two paths now name the same rectangle in the same
  // units, so there is no conversion left for one of them to do differently.
  //
  // WHICH lines and labels appear is the Overlay panel's per-family switches intersected with the
  // export options the user just confirmed (screenshot_export_options.hpp). That selection is a
  // second gate, and an earlier one — "Include Overlay in Screenshot" — was removed for
  // disagreeing with the panel: it defaulted off, did not persist, and gated only the text. This
  // one is prefilled from the panel on every open, lives for this one export, gates lines and
  // labels separately, and can only take away what the screen shows, so untouched it IS the
  // screen and it has no state of its own to drift.
  const float dpi_x = g_preview_vp.dpi_scale_x;
  const float dpi_y = g_preview_vp.dpi_scale_y;
  const float label_w = DeviceToLogical(w, dpi_x);
  const float label_h = DeviceToLogical(h, dpi_y);
  const std::vector<CurveLabelSet> curve_labels =
      BuildScreenshotExportLabelSets(PreviewAnnotationAnchors(), g_state, sel, label_w, label_h);
  ScreenshotRender out;
  out.w = w;
  out.h = h;
  out.has_labels = !curve_labels.empty();
  int render_gl_error = 0;
  out.rgba = RenderExportToRgba(g_preview, params, w, h, curve_labels, dpi_x, dpi_y, &render_gl_error);
  if (out.rgba.empty() && render_gl_error == 0x0505) {
    // The screenshot's own RGBA8 target allocation hit a real out-of-memory in the shared
    // context: the same §2.5 ruling the two all-sky exports and the degraded route already
    // apply (the r4 gap — the screenshot was the one production consumer still reading the
    // helper through the defaulted out-param, so a failed target allocation left the preview's
    // display qualification standing over unprovable storage). The FBO target is already
    // released inside RenderExportToRgba; stop sampling and run one authorized rebuild cycle.
    RecordExportTargetOomIncident(w, h);
    g_preview.RevokeDisplayForContextOom();
    RebuildPreviewAfterContextOom();
  }
  return out;
}

void PerformScreenshotExport(const ScreenshotExportSelection& sel) {
  auto path = ShowExportPngDialog();
  if (path.empty()) {
    return;
  }
  const ScreenshotRender shot = RenderScreenshot(sel);
  if (shot.rgba.empty()) {
    GUI_LOG_ERROR("[GUI] Export screenshot failed: RenderExportToRgba returned empty (vp={}x{})", shot.w, shot.h);
    return;
  }

  if (!WriteRgbaBufferToPng(path, shot.w, shot.h, shot.rgba)) {
    GUI_LOG_ERROR("[GUI] Export screenshot failed: PNG write error path={}", PathToU8(path));
    return;
  }
  GUI_LOG_INFO("[GUI] Export screenshot{}: {}", shot.has_labels ? " (overlay)" : "", PathToU8(path));
}

// The no-dialog body of the dual-fisheye all-sky export (gui tests drive this directly; the
// native save dialog cannot be driven from a test — same split as ExportPreviewPng). Returns
// whether the file was written.
bool ExportDualFisheyeEqualAreaPng(const std::filesystem::path& path) {
  if (path.empty()) {
    return false;
  }

  int w = g_preview.GetTextureWidth();
  int h = g_preview.GetTextureHeight();
  if (w <= 0 || h <= 0) {
    GUI_LOG_ERROR("[GUI] Export dual fisheye: no texture loaded");
    return false;
  }

  PreviewParams params = BuildExportParams();
  ConfigureDualFisheyeExportParams(params);

  // A degraded display would only upsample: the export renders from the FULL source at the
  // original size through the transient renderer, or fails explicitly (plan 814 §3.3). The PNG
  // must be written at the size actually rendered — the source extent in the degraded branch,
  // the display extent otherwise; writing the display size over a full-source render truncates.
  std::vector<unsigned char> rgba;
  int out_w = w;
  int out_h = h;
  if (IsDisplayDegradedAgainstSource()) {
    PreviewSourceExtent(&out_w, &out_h);
    rgba = RenderExportFromFullSource(params, out_w, out_h);
    if (rgba.empty()) {
      return false;  // the failure is already logged; no file may be written
    }
  } else {
    int render_gl_error = 0;
    rgba = RenderExportToRgba(g_preview, params, out_w, out_h, /*curve_labels=*/{}, /*dpi_scale_x=*/1.0f,
                              /*dpi_scale_y=*/1.0f, &render_gl_error);
    if (rgba.empty()) {
      GUI_LOG_ERROR("[GUI] Export dual fisheye: RenderExportToRgba empty (size={}x{} gl_error=0x{:04X})", out_w, out_h,
                    render_gl_error);
      if (render_gl_error == 0x0505) {
        // The export target's allocation OOM'd in the shared context: the §2.5 ruling applies
        // to the whole context, the preview included, however healthy the source upload looked.
        RecordExportTargetOomIncident(out_w, out_h);
        g_preview.RevokeDisplayForContextOom();
        RebuildPreviewAfterContextOom();
      }
      return false;
    }
  }
  if (!WriteRgbaBufferToPng(path, out_w, out_h, rgba)) {
    GUI_LOG_ERROR("[GUI] Export dual fisheye: PNG write failed path={}", PathToU8(path));
    return false;
  }
  GUI_LOG_INFO("[GUI] Export dual fisheye equal-area: {}", PathToU8(path));
  return true;
}

void DoExportDualFisheyeEqualAreaPng() {
  ExportDualFisheyeEqualAreaPng(ShowExportDualFisheyeEqualAreaDialog());
}

// The no-dialog body of the equirectangular all-sky export (same split as the dual-fisheye
// above). Returns whether the file was written.
bool ExportEquirectangularPng(const std::filesystem::path& path) {
  if (path.empty()) {
    return false;
  }

  int tex_w = g_preview.GetTextureWidth();
  int tex_h = g_preview.GetTextureHeight();
  if (tex_w <= 0 || tex_h <= 0) {
    GUI_LOG_ERROR("[GUI] Export equirect: no texture loaded");
    return false;
  }
  // Preserve 155.4 约定：short_res = min(tex_w/2, tex_h) for strict 2:1 output.
  int short_res = std::min(tex_w / 2, tex_h);
  if (short_res <= 0) {
    GUI_LOG_ERROR("[GUI] Export equirect: texture too small (tex={}x{})", tex_w, tex_h);
    return false;
  }
  int dst_w = 2 * short_res;
  int dst_h = short_res;

  PreviewParams params = BuildExportParams();
  ConfigureEquirectExportParams(params);

  // A degraded display would only upsample (plan 814 §3.3): the export renders from the FULL
  // source at its own 2:1 size, or fails explicitly. The PNG is written at the size actually
  // rendered.
  std::vector<unsigned char> rgba;
  if (IsDisplayDegradedAgainstSource()) {
    int src_w = 0;
    int src_h = 0;
    PreviewSourceExtent(&src_w, &src_h);
    const int src_short = std::min(src_w / 2, src_h);
    if (src_short <= 0) {
      GUI_LOG_ERROR("[GUI] Export equirect: degraded source has no valid 2:1 extent ({}x{})", src_w, src_h);
      return false;
    }
    rgba = RenderExportFromFullSource(params, 2 * src_short, src_short);
    if (rgba.empty()) {
      return false;  // the failure is already logged; no file may be written
    }
    dst_w = 2 * src_short;
    dst_h = src_short;
  } else {
    int render_gl_error = 0;
    rgba = RenderExportToRgba(g_preview, params, dst_w, dst_h, /*curve_labels=*/{}, /*dpi_scale_x=*/1.0f,
                              /*dpi_scale_y=*/1.0f, &render_gl_error);
    if (rgba.empty()) {
      GUI_LOG_ERROR("[GUI] Export equirect: RenderExportToRgba empty (size={}x{} gl_error=0x{:04X})", dst_w, dst_h,
                    render_gl_error);
      if (render_gl_error == 0x0505) {
        // Same shared-context ruling as the dual-fisheye branch above.
        RecordExportTargetOomIncident(dst_w, dst_h);
        g_preview.RevokeDisplayForContextOom();
        RebuildPreviewAfterContextOom();
      }
      return false;
    }
  }
  if (!WriteRgbaBufferToPng(path, dst_w, dst_h, rgba)) {
    GUI_LOG_ERROR("[GUI] Export equirect: PNG write failed path={}", PathToU8(path));
    return false;
  }
  GUI_LOG_INFO("[GUI] Export equirectangular: {}", PathToU8(path));
  return true;
}

void DoExportEquirectangularPng() {
  ExportEquirectangularPng(ShowExportEquirectangularDialog());
}

void DoExportConfigJson() {
  auto path = ShowExportJsonDialog();
  if (path.empty()) {
    return;
  }
  // Build-or-reject is a pure, unit-tested function (BuildExportJsonOrWarn): it refuses to
  // produce a config when a filter overflows the ABI bounds — otherwise the JSON twin would
  // emit a semantically-opposite match-all stand-in and we would silently write a wrong
  // config (code-review-02/03 Major). This wrapper only owns the dialog path + user surface.
  std::string json_str;
  std::string warning;
  if (!BuildExportJsonOrWarn(g_state, &json_str, &warning)) {
    GUI_LOG_WARNING("[GUI] Export config JSON aborted: {}", warning);
    SetGuiWarning(warning);
    return;
  }
  // No "this config will fall back to the CPU on the GPU route" notice here, on
  // purpose. The exported document carries two renderers (preview + export
  // projection), and a GPU session now serves N renderers at once, so the
  // renderer count is no longer a fallback trigger. What could still make a GPU
  // route refuse a config is Simulator's CanUseBackend gate: more renderers than
  // the backend's MaxRenderers() (4, the same count the C API rejects at parse
  // time, LUMICE_MAX_CONFIG_RENDERERS — so never reached), or a renderer the
  // backend's IsCompatible() declines (Metal's and CUDA's both accept every lens
  // type and view unconditionally, like the base default — so never reached
  // either). A
  // notice for an unreachable path is a notice nobody can ever see; if a backend
  // starts declining renderers by lens type or view, this is where to add it,
  // reading the same fields BuildExportJsonOrWarn just serialized.
  RequestConfigJsonExport(path, json_str);
}

void RequestConfigJsonExport(const std::filesystem::path& path, const std::string& json_str) {
  if (path.empty()) {
    return;
  }
  if (ConfigJsonExportNeedsOverwriteConfirm(path)) {
    // Hold everything and ask. Nothing is written on this branch — the whole point is that the
    // document already on disk survives until the user says otherwise.
    g_pending_export_json_path = path;
    g_pending_export_json_content = json_str;
    g_show_export_overwrite_confirm_popup = true;
    return;
  }
  if (!ExportConfigJson(path, json_str)) {
    GUI_LOG_ERROR("[GUI] Export config JSON failed: {}", PathToU8(path));
    return;
  }
  GUI_LOG_INFO("[GUI] Export config JSON: {}", PathToU8(path));
}

void ConfirmPendingConfigJsonExport() {
  if (g_pending_export_json_path.empty()) {
    return;
  }
  const std::filesystem::path path = std::move(g_pending_export_json_path);
  const std::string json_str = std::move(g_pending_export_json_content);
  g_pending_export_json_path.clear();
  g_pending_export_json_content.clear();
  if (!ExportConfigJson(path, json_str)) {
    GUI_LOG_ERROR("[GUI] Export config JSON failed: {}", PathToU8(path));
    return;
  }
  GUI_LOG_INFO("[GUI] Export config JSON (overwrote existing file): {}", PathToU8(path));
}

void CancelPendingConfigJsonExport() {
  g_pending_export_json_path.clear();
  g_pending_export_json_content.clear();
}

bool BgPhotoOnScreen(const GuiState& state) {
  return BgPhotoInFrame(g_preview.HasBackground(), state.bg_show, state.renderer);
}

// The background photo lives in two places that must agree: the GL texture the shader samples and
// the CPU byte copy the eyedropper samples. Clearing one without the other leaves the picker
// reading an image that is no longer on screen, so the two clears are spelled once, here, and
// every site that drops the background calls this instead of PreviewRenderer::ClearBackground.
static void ClearBackgroundImage(GuiState& state) {
  g_preview.ClearBackground();
  state.bg_pixels.clear();
  // Up to ~50 MB; a cleared-but-reserved vector still holds it. shrink_to_fit is a non-binding
  // request — the standard permits an implementation to ignore it — so this is a best effort at
  // handing the memory back, not a guarantee to cite as the feature's hard ceiling.
  state.bg_pixels.shrink_to_fit();
  state.bg_pixel_w = 0;
  state.bg_pixel_h = 0;
}

// Helper: load image from path, downsample if needed, upload to bg texture, and keep a CPU
// copy of the uploaded bytes in `state` for the eyedropper to sample. Returns true on success.
static bool LoadAndUploadBgImage(GuiState& state, const std::filesystem::path& path) {
  int w = 0;
  int h = 0;
  int channels = 0;
  auto u8path = PathToU8(path);
  unsigned char* raw = stbi_load(u8path.c_str(), &w, &h, &channels, 3);  // Force 3 channels (RGB)
  if (!raw) {
    GUI_LOG_WARNING("Failed to load background image: {}", u8path);
    return false;
  }

  // Copy to vector, then free stbi allocation
  size_t byte_count = static_cast<size_t>(w) * h * 3;
  std::vector<unsigned char> data(raw, raw + byte_count);
  stbi_image_free(raw);

  // Box downsample while max dimension > 4096
  while (std::max(w, h) > 4096) {
    int new_w = w / 2;
    int new_h = h / 2;
    for (int y = 0; y < new_h; y++) {
      for (int x = 0; x < new_w; x++) {
        for (int c = 0; c < 3; c++) {
          int sum = data[(y * 2 * w + x * 2) * 3 + c] + data[(y * 2 * w + x * 2 + 1) * 3 + c] +
                    data[((y * 2 + 1) * w + x * 2) * 3 + c] + data[((y * 2 + 1) * w + x * 2 + 1) * 3 + c];
          data[(y * new_w + x) * 3 + c] = static_cast<unsigned char>(sum / 4);
        }
      }
    }
    w = new_w;
    h = new_h;
    data.resize(static_cast<size_t>(w) * h * 3);
  }

  g_preview.UploadBgTexture(data.data(), w, h);
  // AFTER the upload, never before: UploadBgTexture's first line returns early on a null pointer,
  // and a moved-from vector's data() is exactly that — the texture would silently stay empty.
  state.bg_pixels = std::move(data);
  state.bg_pixel_w = w;
  state.bg_pixel_h = h;
  return true;
}

// Shared background-restore + degrade step. See app.hpp for the contract; the three call
// sites (`.lmc` open / `.json` import / New) all run it right after ResetFrontendState, which
// has already cleared the background.
void LoadBackgroundWithDegrade(GuiState& state) {
  if (state.bg_path.empty()) {
    return;
  }
  if (LoadAndUploadBgImage(state, state.bg_path)) {
    // bg_show and bg_alpha already restored from deserialization / personal defaults.
    return;
  }
  // Degradation: bg image not found — clear show, reset kMatchBg->kFree. Warn rather than
  // fail: a personal default pointing at a since-deleted image must not block startup.
  GUI_LOG_WARNING("[GUI] Background image '{}' could not be loaded; showing the preview without it",
                  PathToU8(state.bg_path));
  state.bg_show = false;
  if (state.aspect_preset == AspectPreset::kMatchBg) {
    state.aspect_preset = AspectPreset::kFree;
  }
}

void ResetFrontendState(GuiState& state, FrontendResetReason reason, const LmcTexture* baked) {
  // reason/payload consistency: baked payload is present iff and only if kOpenBaked.
  // A caller mismatch is a programming error — assert here so it fails fast in debug builds
  // instead of silently doing the wrong thing (UB deref if kOpenBaked && baked==nullptr, or
  // silently ignoring a payload for the other reasons).
  assert((reason == FrontendResetReason::kOpenBaked) == (baked != nullptr));

  // Every reason invalidates the thumbnail cache: New / Open replace the entire layer/entry
  // structure; Revert restores it from snapshot — same reason to refresh thumbnails either way.
  g_thumbnail_cache.OnLayerStructureChanged();

  // And every reason closes an open Edit Entry modal, for the same reason one level up: it binds
  // its entry by position into the pool these reasons replace or restore, and holds copies of that
  // entry's crystal and filter. An uncommitted edit in those copies would otherwise be committed
  // into whatever the new document has at the same position. Same shape: unconditional, no
  // reason-grouped block.
  CloseEditModalOnDocumentReset();
  // And every reason drops the axis chips' per-crystal preset memory: it is keyed by pool crystal
  // id, and each of these reasons replaces or restores the pool, so what a stale entry would
  // remember belongs to a crystal that is no longer at that id. Unconditional on purpose — not
  // folded into any reason-grouped block below, so it cannot inherit another state's rule.
  ClearAxisPresetMemory();
  // The named camera target is session intent, not document state. Every successful document reset
  // drops both the target and the lock so a previous document cannot move the newly loaded camera
  // on the next preview frame. Revert also clears it: the view itself is preserved, but no old
  // follow intent may wake back up after the scene fields are restored.
  state.look_at = LookAtSession{};
  // And the Summary window, unconditionally for the same reason: it is a picture of the document
  // these reasons replace or restore, so an open one would carry over showing the next document
  // under the previous one's title. Deliberately NOT the analysis window's per-reason rule below
  // (which keeps that window open across kRevert): nothing in this window is worth keeping open,
  // and one rule that covers every reason is the shape the edit modal's close above settled on.
  state.config_summary_window_open = false;
  // And an armed Angular Distance pick, by the same reasoning as the Summary window: the pick
  // would otherwise survive into the next document, with the next click adding a ring to a list
  // the user never aimed at.
  state.angular_dist_picker.armed = false;

  // Previous document's recovery leftovers are cleared BEFORE this function decides what the new
  // document shows: the kOpenBaked branch below legitimately ESTABLISHES recovery state (its own
  // ladder tier, its own retained source on failure), and a cleanup that ran after the switch
  // would erase exactly what the switch just built. The poller-side fence and the payload
  // retention belong to the previous document's picture in every non-Revert reason.
  if (reason != FrontendResetReason::kRevert) {
    g_server_poller.InvalidateStagedTexture();
    g_last_uploaded_payload.reset();
    // A document switch re-authorizes the upload cycle (814): a latch recorded by the previous
    // document's failed upload belongs to that document's generation, not to this one. The retry
    // source belongs to the old document too, and the ladder's established tier restarts — one
    // walk per document is the bounded shape, and a tier the previous document needed says
    // nothing about what the next one will.
    state.upload_latched = false;
    g_pending_upload_source.reset();
    g_pending_upload_serial = 0;
    g_file_source = FilePreviewSource{};  // the old document's source fact leaves with the document
    g_recovery.xyz_tier = 0;
    g_recovery.exhausted = false;
    g_recovery.notice_dismissed = false;
    g_recovery.has_incident = false;
  }

  // Preview texture / background — as-built subset per reason. `.lmc` variants both call
  // ClearBackground (post-branch shared line in the pre-refactor DoOpen); the DoOpen(.lmc)
  // handler then does bg_path restore separately (data recovery, not reset).
  switch (reason) {
    case FrontendResetReason::kNewDocument:
    case FrontendResetReason::kOpenLmcBlank:
    case FrontendResetReason::kOpenJson:
      g_preview.ClearTexture();
      ClearBackgroundImage(state);
      break;
    case FrontendResetReason::kOpenBaked:
      // Which upload entry point is decided by the file, not by this owner: a v>=5 .lmc carries
      // the unexposed linear XYZ energy a live run uploads, a v4 one carries radiance-only bytes
      // the shader still owes the vignetting and the sky, a pre-v4 one has the sky already summed
      // in and can only be shown as it is.
      //
      // A document switch revokes the PREVIOUS document's display first (814 AC2: a cross-document
      // open must never leave the old pixels on screen, and an upload that fails mid-flight must
      // not be able to mix them into the new document's frame). ClearTexture also clears the CPU
      // mirror, so each branch below re-establishes the save source from the file's own bytes —
      // which stay saveable even when the display upload fails (814 §3.3: a document whose first
      // display failed keeps its complete source, with the preview explicitly not shown).
      g_preview.ClearTexture();
      switch (baked->mode) {
        case PreviewRenderer::TextureMode::kXyz: {
          // The open runs the SAME upload cycle a live upload and a Retry run: ONE authorized
          // walk from candidate 0 (the r4-counted defect here was a tier-0 attempt outside the
          // ladder followed by a walk from candidate 0 — two full-size PBO passes in one cycle,
          // and a first-failure record that could not see the pre-ladder attempt).
          const XyzUploadCycle cycle =
              RunXyzUploadCycle(baked->xyz.data(), baked->width, baked->height, /*texture_serial=*/0,
                                /*probe_full_ladder=*/true);
          // The mirror + the source fact: the file's own bytes and their metadata, whether or
          // not the GPU accepted the display upload (UpdateCpuXyzTextureData copies). THIS — not
          // a payload wrapped around the caller's stack buffer, whose floats die when DoOpen
          // returns — is the retry and save-source fact from here on.
          g_preview.UpdateCpuXyzTextureData(baked->xyz.data(), baked->width, baked->height, baked->meta);
          g_file_source.valid = true;
          g_file_source.width = baked->width;
          g_file_source.height = baked->height;
          g_file_source.mode = PreviewRenderer::TextureMode::kXyz;
          g_file_source.snapshot_intensity = baked->meta.snapshot_intensity;
          g_file_source.emitted_energy = baked->meta.emitted_energy;
          g_file_source.mono_anchor = baked->meta.mono_anchor;
          g_file_source.effective_pixels = baked->meta.effective_pixels;
          if (cycle.displayed_tier >= 0) {
            if (cycle.displayed_tier > 0) {
              // The tier memory belongs to this document's session: a live frame arriving later
              // starts from what the open established instead of re-probing the full ladder.
              g_recovery.xyz_tier = cycle.displayed_tier;
              GUI_LOG_INFO("[GUI] Open: recovery ladder established tier {} ({}x{})", cycle.displayed_tier,
                           baked->width, baked->height);
            }
            // The exposure state SyncFromPoller would have filled from a live frame, filled from
            // the file instead — the same four numbers, from the same frame the pixels came from,
            // so the per-frame ComputeMonoExposure in the Display panel lights this texture
            // exactly as it lit the live one. Without them intensity_scale is 0 and the reopened
            // document is black. ev_auto is recomputed rather than stored: it is a pure function
            // of the anchor, the intensity and target_white, and target_white is not persisted.
            // Committed only on a successful upload (814 AC1): exposure belongs to the frame that
            // was accepted.
            state.snapshot_intensity = baked->meta.snapshot_intensity;
            state.snapshot_emitted_energy = baked->meta.emitted_energy;
            state.effective_pixels = baked->meta.effective_pixels;
            state.p99_raw_y = baked->meta.mono_anchor;
            state.ev_auto = LUMICE_ComputeEvAuto(state.p99_raw_y, state.snapshot_intensity, state.target_white);
          } else {
            // The whole ladder ran out: the cycle's own first-failure record IS the incident (the
            // real stage, GL error and request size — nothing synthesized on top), the cycle
            // reports exhausted so the notice and the retry predicate open the recovery exit,
            // and the mirror + source facts above are what an explicit Retry walks against,
            // without re-opening the file.
            g_recovery.exhausted = true;
            GUI_LOG_ERROR(
                "[GUI] Open: baked XYZ texture upload failed (stage={} gl_error=0x{:04X} request={}x{}) — "
                "preview unavailable; the document's source is retained and still saves",
                UploadStageName(cycle.first_failure.stage), cycle.first_failure.gl_error, baked->width, baked->height);
          }
          break;
        }
        case PreviewRenderer::TextureMode::kSrgbRadiance: {
          const UploadResult upload_result =
              g_preview.UploadRadianceTexture(baked->srgb.data(), baked->width, baked->height);
          g_file_source.valid = true;
          g_file_source.width = baked->width;
          g_file_source.height = baked->height;
          g_file_source.mode = PreviewRenderer::TextureMode::kSrgbRadiance;
          // The 8-bit upload paths write the mirror themselves on success; on failure the mirror
          // is empty (ClearTexture above) and the file's bytes are restored here so the source
          // survives without the display — and the failure is OBSERVABLE: the incident record
          // and the exhausted flag open the notice and the Retry exit (the r4 gap: a uint8 open
          // failure used to restore the mirror and only log, leaving the panel silent).
          if (!upload_result.ok) {
            g_preview.UpdateCpuTextureData(baked->srgb.data(), baked->width, baked->height);
            RecordUploadIncident(upload_result, /*tier_tried=*/0, /*texture_serial=*/0);
            g_recovery.exhausted = true;
            GUI_LOG_ERROR(
                "[GUI] Open: baked radiance texture upload failed (stage={} gl_error=0x{:04X} "
                "request={}x{}) — preview unavailable; the document's source is retained and still saves",
                UploadStageName(upload_result.stage), upload_result.gl_error, baked->width, baked->height);
          }
          break;
        }
        case PreviewRenderer::TextureMode::kSrgbComposited: {
          const UploadResult upload_result = g_preview.UploadTexture(baked->srgb.data(), baked->width, baked->height);
          g_file_source.valid = true;
          g_file_source.width = baked->width;
          g_file_source.height = baked->height;
          g_file_source.mode = PreviewRenderer::TextureMode::kSrgbComposited;
          if (!upload_result.ok) {
            // The label must name what the bytes ARE: these texels already carry the sky, and a
            // mirror that said radiance-only would make the next Save write a sky-summed frame
            // under a flag that adds the sky again on reopen. UpdateCpuTextureData defaults to
            // radiance because its usual producers bake radiance, so the composited caller says
            // so explicitly. Failure is observable for the same reason as the radiance branch.
            g_preview.UpdateCpuTextureData(baked->srgb.data(), baked->width, baked->height,
                                           PreviewRenderer::TextureMode::kSrgbComposited);
            RecordUploadIncident(upload_result, /*tier_tried=*/0, /*texture_serial=*/0);
            g_recovery.exhausted = true;
            GUI_LOG_ERROR(
                "[GUI] Open: baked composited texture upload failed (stage={} gl_error=0x{:04X} "
                "request={}x{}) — preview unavailable; the document's source is retained and still saves",
                UploadStageName(upload_result.stage), upload_result.gl_error, baked->width, baked->height);
          }
          break;
        }
      }
      ClearBackgroundImage(state);
      break;
    case FrontendResetReason::kRevert:
      // Revert preserves current preview — it is a config restore, not a document switch.
      break;
  }

  // Poller-side staged composite fence: document-switch reasons must discard any in-flight
  // snapshot from the previous scene (task-351 class regression). Revert keeps current staged.
  // The payload Save reads goes with it: it belonged to the previous document's picture, and
  // whatever this document shows next either came from a file (mirrored above, no payload) or
  // will come from a fresh upload that retains its own.
  // The analysis tool's state is about the scene that was on screen: its picked centre, its
  // selection and its list mean nothing for another document. The window stays open — which
  // panels are open is not document state. Revert keeps all of it: the scene is the same one.
  if (reason != FrontendResetReason::kRevert) {
    const bool window_open = state.analysis.window_open;
    state.analysis = GuiState::RaypathAnalysisSession{};
    state.analysis.window_open = window_open;
    // The list's search text has this struct's lifetime but is not a member of it (an ImGui type
    // cannot enter gui_state.hpp), so the whole-struct reset above does not reach it.
    ClearAnalysisSearchFilter();
    state.analysis_result = GuiState::AnalysisResultView{};
    state.analysis_run_in_progress = false;
    // And the poller's carried copy, or the cleared view would adopt it back next frame.
    g_server_poller.InvalidateAnalysisResult();
  }

  // crystal_mesh_hash reset — as-built: only DoNew clears it (Open branches don't touch it;
  // edit_modals writes -1 as a distinct "force re-upload" signal, not a "no mesh" reset).
  if (reason == FrontendResetReason::kNewDocument) {
    g_crystal_mesh_hash = 0;
  }

  // Effects-baseline invalidation — as-built: only DoRevert (color-migration §4 M6, plan §1
  // 偏离 C 修复). Forces the next reconciler tick to re-push the restored display payload so
  // the server catches up to Revert without further user interaction.
  if (reason == FrontendResetReason::kRevert) {
    state.InvalidateEffectsBaselines();
  }

  // Trackball reset for the modal-preview view — document-switch reasons only. Revert keeps
  // current trackball pose (config restore, not scene switch).
  if (reason != FrontendResetReason::kRevert) {
    if (!state.layers.empty() && !state.layers[0].entries.empty()) {
      ResetCrystalViewToCrystal(state.crystals[state.layers[0].entries[0].crystal_id]);
    }
  }
}

// The GUI edits uniform shape distributions only. If a document carried non-uniform families
// (gauss/laplacian/...) they were loaded as uniform (see ParseShapeDist). This is a deliberate
// capability downgrade, surfaced once via the shared import-warning popup — from BOTH load
// branches, which is why the sentence lives here instead of being spelled out twice.
constexpr const char* kShapeDistDowngradeNotice =
    "Some crystal shape distributions used non-uniform types (e.g. gauss, laplacian). The GUI "
    "edits uniform distributions only, so they were loaded as uniform. Edit the config file / "
    "CLI directly to keep other distribution types.";

std::string TextureLoadNotice(const LmcTexture& texture) {
  if (!texture.HasPixels() || texture.mode == PreviewRenderer::TextureMode::kXyz) {
    return {};
  }
  if (texture.mode == PreviewRenderer::TextureMode::kSrgbComposited) {
    return "This cached preview is still viewable, but its sky colour and exposure are baked in.\n"
           "Channel B-R cannot accurately exclude that sky. Original XYZ data is missing,\n"
           "so EV and Print cannot be fully applied to this preview.\n"
           "Run to regenerate the data, then Save to update the file. Saving alone cannot recover it.";
  }
  return "This cached halo-only preview is still viewable; Channel B-R excludes the sky colour.\n"
         "Exposure is baked in and original XYZ data is missing, so EV and Print cannot be\n"
         "fully applied to this preview. Run to regenerate the data, then Save to update\n"
         "the file. Saving alone cannot recover it.";
}

void DoOpen() {
  DoOpen(ShowOpenDialog());
}

void DoOpen(const std::filesystem::path& path) {
  if (path.empty()) {
    return;
  }

  // Check extension to determine file format
  bool is_json = path.extension() == ".json";

  if (is_json) {
    // Import CLI JSON config
    std::ifstream in(path);
    if (!in.is_open()) {
      return;
    }
    std::string json_str((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    GuiState new_state = MakeNewDocumentState();
    // Discarded, not reported. DeserializeFromJson opens with `state = GuiState{}`, so this
    // path keeps NOTHING of the personal defaults — warning about a degraded default the user is
    // not being given would be a warning about nothing. Draining the channel anyway matters:
    // left in place, this read's counts would surface on the next New and describe THAT load.
    TakeUserDefaultsDowngradeCount();
    TakeUserDefaultsDowngradeNotices();
    // Same pre-load drain the .lmc branch does below, for the same reason: MakeNewDocumentState
    // just ran the user's personal defaults through this very deserializer, so a hand-edited
    // defaults file can leave a count behind that belongs to no load at all. Without it, the
    // post-load take further down would report that count as this document's downgrade.
    TakeShapeDistDowngradeCount();
    if (DeserializeFromJson(json_str, new_state)) {
      // Data restore + command-semantic fields (path/dirty/run_intent stay in handler per
      // plan §2 — they are command intent, not frontend reset).
      g_state = new_state;
      g_state.current_file_path.clear();  // Don't set .json path as save target
      g_state.dirty = true;               // Unsaved new project
      // Intent: no server run behind this import (→ kIdle via ReconcileSimState).
      g_state.run_intent = RunIntent::kNone;
      // Frontend reset delegated to the single owner.
      ResetFrontendState(g_state, FrontendResetReason::kOpenJson);
      LoadBackgroundWithDegrade(g_state);
      GUI_LOG_INFO("[GUI] DoOpen (JSON import): {}", PathToU8(path));

      // Same notice the .lmc branch raises below, and it belongs here even more than there: a core
      // JSON is exactly the document that carries the distribution families the GUI cannot edit
      // (gauss/laplacian/...), and until this call the downgrade happened with nothing said. The
      // wording is shared with that branch — one downgrade, one sentence, wherever it is read from.
      if (TakeShapeDistDowngradeCount() > 0) {
        AppendLoadNotice(kShapeDistDowngradeNotice);
      }
    }
    return;
  }

  // Load .lmc binary file
  LmcTexture tex;
  // Discard any stale counts so only this load's downgrades are counted. Both channels need the
  // drain, and the filter one needs it for a reason the shape one does not have: MakeNewDocumentState
  // runs the user's personal defaults through the same deserializer, so a hand-edited defaults file
  // can leave a count behind that belongs to no load at all.
  TakeShapeDistDowngradeCount();
  TakeFilterNoPredicateDowngradeCount();
  TakeInvalidSummandRowCount();
  if (LoadLmcFile(path, g_state, tex)) {
    // Data restore + command-semantic fields (path/dirty/run_intent stay in handler).
    g_state.current_file_path = path;
    g_state.dirty = false;
    if (tex.HasPixels()) {
      // Intent: a baked static result (→ kDone via ReconcileSimState, no server run).
      g_state.run_intent = RunIntent::kLoaded;
      ResetFrontendState(g_state, FrontendResetReason::kOpenBaked, &tex);
    } else {
      // Intent: no result to show (→ kIdle). Mirrors DoNew() / JSON-import
      // semantics: "no preview data = clear screen, wait for user to Run".
      g_state.run_intent = RunIntent::kNone;
      ResetFrontendState(g_state, FrontendResetReason::kOpenLmcBlank);
    }
    AppendLoadNotice(TextureLoadNotice(tex));
    GUI_LOG_INFO("[GUI] DoOpen: {}", PathToU8(path));

    // Background image restore from saved path — DATA recovery (not a frontend reset). Uses
    // the deserialized alpha, not a hard-coded value. Runs after ResetFrontendState which
    // already ClearBackground()'d, so a missing bg_path leaves the preview blank (as before).
    LoadBackgroundWithDegrade(g_state);

    // Notify: same downgrade, same sentence as the JSON-import branch above (kShapeDistDowngradeNotice).
    if (TakeShapeDistDowngradeCount() > 0) {
      AppendLoadNotice(kShapeDistDowngradeNotice);
    }

    // Notify: a filter object in the file described no rule a reader could use (an empty summands
    // array, a legacy form with no raypath text, a filter type the GUI no longer has). The entry
    // was loaded without a filter rather than with one that matches everything — under filter_out
    // the latter hides every ray, and a black picture says nothing about why. Same one-time popup
    // as the shape-distribution notice above; the two append rather than replace each other.
    if (TakeFilterNoPredicateDowngradeCount() > 0) {
      AppendLoadNotice(
          "Some filters in this file described no rule (no ray paths and no entry/exit faces). They "
          "were loaded as no filter at all, so the entries they belonged to now let every ray "
          "through. Re-add the rule in the entry's Filter tab if the file was meant to carry one.");
    }

    // Notify: a filter row in the file was not a valid filter expression — an empty step in a face
    // path ("3--5", "-3-5", "3-5-"), a face number no crystal has, a malformed entry:/exit:/len:
    // factor. The row was dropped. It used to be read by the tolerant parser instead, which skips
    // what it cannot make sense of, so "3--5" loaded as the path 3-5: a path that looks entirely
    // reasonable and is not the one the file states. Same one-time popup as the two notices above.
    if (TakeInvalidSummandRowCount() > 0) {
      AppendLoadNotice(
          "Some filter rows in this file were not valid filter expressions (for example a face path "
          "with an empty step, like \"3--5\"). Those rows were dropped rather than guessed at, so "
          "the filters they belonged to now match less than the file described. Re-enter the "
          "affected rows in the entry's Filter tab.");
    }
  }
}

void SurfaceUserDefaultsDowngrades() {
  const int count = TakeUserDefaultsDowngradeCount();
  const std::vector<std::string> notices = TakeUserDefaultsDowngradeNotices();
  if (count == 0 && notices.empty()) {
    return;  // the normal case — no "your defaults loaded fine" noise on every New
  }

  // The notices carry the detail (which key, which values); the count is what catches a
  // degradation that produced no sentence — an unreadable or malformed file, where naming a key
  // is impossible but staying silent would leave the user's saved defaults quietly not applying.
  std::string msg = "Some of your personal defaults could not be used as saved:";
  for (const auto& notice : notices) {
    msg += "\n  - " + notice;
  }
  if (notices.empty()) {
    msg += "\n  - your personal defaults file could not be read; this document uses the built-in values.";
  }
  msg += "\nCheck the Settings values and save the intended personal defaults again.";
  AppendLoadNotice(msg);
}

void DoNew() {
  // Data reset (default state), then delegate every frontend reset (preview clear + poller
  // fence + mesh-hash zero + trackball) to the single owner.
  g_state = MakeNewDocumentState();
  SurfaceUserDefaultsDowngrades();
  ResetFrontendState(g_state, FrontendResetReason::kNewDocument);
  LoadBackgroundWithDegrade(g_state);
  GUI_LOG_INFO("[GUI] DoNew");
}

bool DoLoadBackground(GLFWwindow* window) {
  auto path = ShowOpenImageDialog();
  if (path.empty()) {
    return false;
  }

  if (!LoadAndUploadBgImage(g_state, path)) {
    return false;
  }

  g_state.bg_path = path;
  g_state.bg_show = true;
  g_state.bg_alpha = 0.5f;  // Start at 50% to immediately show overlay effect
  // A new image starts from the identity transform: the pan/zoom the user dialed in for the
  // PREVIOUS photo is meaningless for this one (it was fitted to that photo's crop), and leaving
  // it in place would open the new image already off-frame with no visible cause. Same timing as
  // bg_alpha above. DoClearBackground deliberately does not reset these, matching bg_alpha's
  // existing asymmetry there rather than inventing a second rule.
  g_state.bg_offset_x = 0.0f;
  g_state.bg_offset_y = 0.0f;
  g_state.bg_scale = 1.0f;
  g_state.aspect_preset = AspectPreset::kMatchBg;
  ApplyAspectRatio(window, AspectPreset::kMatchBg, false, g_preview.GetBgAspect());
  return true;
}

void DoClearBackground() {
  ClearBackgroundImage(g_state);
  g_state.bg_path.clear();
  g_state.bg_show = false;
  if (g_state.aspect_preset == AspectPreset::kMatchBg) {
    g_state.aspect_preset = AspectPreset::kFree;
  }
}

// Run a short simulation with the current default config to measure platform throughput,
// then set the quality gate threshold to max(kMinRaysFloor, measured_rate * 10%).
// Called once at startup, before the main loop, and returns as soon as the run has been handed to
// a background thread (typically the run completes in < 200ms; JoinPendingCalibration waits for
// it).
//
// This is ALSO the startup warm-up, not only a measurement — the name predates that second role.
// The run goes through the same LUMICE_CommitScene → CommitConfig → Start → Simulator::Run →
// TraceBackend::BeginSession chain a user's Run takes, on the server the user will actually run
// on (main.cpp constructs it for the document's backend before calling here), so every
// once-per-process and once-per-server cost — the GPU context, the consumer layout for the default
// document, the D65 table, the first large readback's page faults — is paid here rather than on
// the user's first Run. Skipping the calibration (--skip-calibration) therefore also skips the
// warm-up: the first real Run then pays the cold-start cost this removes.
//
// Split in two on purpose: the scene is built HERE, on the calling (main) thread, because g_state
// is not thread-safe and the main loop starts writing it the moment this returns; the background
// thread then touches nothing but the built scene and g_server through the C API.
void CalibrateQualityThreshold() {
  if (!g_server) {
    return;
  }
  constexpr int kCalibrationRays = 100000;

  // Use current default state to build a calibration scene
  ScenePtr scene = BuildScene(g_state, SceneIntent::kSimCommit);
  if (!scene) {
    // Unlike DoRun (which pops SetGuiWarning), calibration degrades silently to the default
    // threshold: it does not touch the user's edited/committed config, so a log line is
    // sufficient and a modal would be noise. Intentional asymmetry, not an oversight.
    GUI_LOG_WARNING("[Calibration] filter exceeds ABI limits, using default threshold");
    return;
  }
  // Override just the two sim scalars for the short calibration run; every Set* is idempotent
  // (last write wins), so re-setting the group is the documented way to amend a built scene.
  LUMICE_SceneSetSimParams(scene.get(), /*infinite=*/0, kCalibrationRays, g_state.sim.max_hits, /*geom_clock=*/0);
  RunCalibrationInBackground(std::move(scene));
}

void RunCalibrationInBackground(ScenePtr scene) {
  if (!g_server || !scene) {
    return;
  }
  // Single-owner future, as g_stop_future is: a second dispatch while one is in flight would drop
  // the first future's handle (std::async's destructor blocks) — join it instead.
  JoinPendingCalibration();
  // The scene is moved into the task; `srv` is captured by value so that the thread never re-reads
  // the global — the joins at every reassignment site are what make the captured pointer stay
  // valid for the thread's whole life.
  g_calibration_future = std::async(std::launch::async, [srv = g_server, scene = std::move(scene)] {
    constexpr double kCalibrationFraction = 0.4;  // Accept frames >= 40% of typical ray count per window

    auto t0 = std::chrono::steady_clock::now();
    auto err = LUMICE_CommitScene(srv, scene.get(), nullptr);
    if (err != LUMICE_OK) {
      GUI_LOG_WARNING("[Calibration] CommitConfig failed ({}), using default threshold", static_cast<int>(err));
      return;
    }
    auto t_commit = std::chrono::steady_clock::now();
    GUI_LOG_DEBUG("[Calibration] CommitConfig took {:.1f}ms",
                  std::chrono::duration<double, std::milli>(t_commit - t0).count());

    // Wait for simulation to complete (server returns to IDLE)
    constexpr int kMaxWaitMs = 2000;
    int waited_ms = 0;
    LUMICE_ServerState final_state = LUMICE_SERVER_RUNNING;
    while (waited_ms < kMaxWaitMs) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      waited_ms += 10;
      LUMICE_QueryServerState(srv, &final_state);
      if (final_state == LUMICE_SERVER_IDLE) {
        break;
      }
    }

    auto t1 = std::chrono::steady_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // Whatever the outcome below, the server is left IDLE for the user's first actual Run. Done
    // before the early returns rather than after the threshold: a timed-out run (kMaxWaitMs) that
    // returned early would otherwise leave the server RUNNING the calibration scene behind the
    // user's back.
    struct StopOnExit {
      LUMICE_Server* server;
      ~StopOnExit() { LUMICE_StopServer(server); }
    } stop_on_exit{ srv };

    // Read stats to get actual rays simulated
    LUMICE_StatsResult stats{};
    {
      LUMICE_ResultFrame* raw_frame = nullptr;
      if (LUMICE_AcquireResultFrame(srv, &raw_frame) != LUMICE_OK) {
        GUI_LOG_WARNING("[Calibration] could not acquire a result frame, using default threshold");
        return;
      }
      lumice::ResultFramePtr frame(raw_frame);
      LUMICE_FrameGetStats(frame.get(), &stats);
    }
    GUI_LOG_DEBUG("[Calibration] waited {}ms, state={}, sim_ray_num={}", waited_ms,
                  final_state == LUMICE_SERVER_IDLE ? "IDLE" : "RUNNING", stats.sim_ray_num);

    if (stats.sim_ray_num == 0 || elapsed_ms <= 0) {
      GUI_LOG_WARNING("[Calibration] no data produced in {:.0f}ms, using default threshold", elapsed_ms);
      return;
    }

    // Compute: how many rays would be produced in the calibration window?
    // Uses kCalibrationWindowMs (fixed) instead of kCommitIntervalMs so that changing
    // the commit interval doesn't accidentally tighten/loosen the quality gate.
    double rays_per_window = static_cast<double>(stats.sim_ray_num) * kCalibrationWindowMs / elapsed_ms;
    auto threshold = std::max(kMinRaysFloor, static_cast<unsigned long long>(rays_per_window * kCalibrationFraction));

    g_server_poller.SetCalibratedThreshold(threshold);
    GUI_LOG_INFO("[Calibration] done in {:.0f}ms: {} rays, {:.0f} rays/window({}ms), threshold={}", elapsed_ms,
                 stats.sim_ray_num, rays_per_window, kCalibrationWindowMs, threshold);
  });
}


// Resolve which GPU backend to request on this host: Metal on Apple, CUDA on
// NVIDIA. Both are single-engine GPU routes (vs the CPU N-worker topology). Probes
// the C API at runtime, so a host with neither resolves to CPU (the toggle that
// drives this is hidden when no GPU is available — see panels.cpp).
int ResolveGpuBackend() {
  if (LUMICE_IsBackendAvailable(LUMICE_BACKEND_METAL)) {
    return LUMICE_BACKEND_METAL;
  }
  if (LUMICE_IsBackendAvailable(LUMICE_BACKEND_CUDA)) {
    return LUMICE_BACKEND_CUDA;
  }
  return LUMICE_BACKEND_CPU;
}

LUMICE_ServerConfig ServerConfigForState(const GuiState& state) {
  LUMICE_ServerConfig cfg{};
  // The user's personal default (Settings §app), 0 = the automatic count (server.cpp's per-platform
  // core count, capped). On the GPU route
  // the render engine is one worker whatever this says; the value sizes that server's standing CPU
  // analysis pool instead.
  cfg.num_workers = state.worker_count;
  cfg.sim_seed = 0;  // 0 = random — matches LUMICE_CreateServer() startup default
  // A host whose GPU probe fails resolves to CPU here, so a personal default of use_gpu_backend
  // saved on a machine that later lost its GPU still constructs a working server — there is no
  // failing construction to handle (LUMICE_CreateServerEx never returns null). A GPU that probes
  // fine but fails at BeginSession falls back to the CPU path mid-run and raises the fallback
  // flag, exactly as it would on a user's Run.
  cfg.preferred_backend = state.use_gpu_backend ? ResolveGpuBackend() : LUMICE_BACKEND_CPU;
  return cfg;
}

void ConstructServerForState(const GuiState& state) {
  const LUMICE_ServerConfig cfg = ServerConfigForState(state);
  g_server = LUMICE_CreateServerEx(&cfg);
  g_server_is_gpu = state.use_gpu_backend;
  // Read back from the config that was actually handed to the constructor, not from the state.
  // The two are the same expression today; assigning the request would make this tracker agree
  // with the request even if ServerConfigForState stopped passing it on, which is precisely the
  // defect the tracker is here to make observable.
  g_server_worker_count = cfg.num_workers;
}

// Reconstruct the server so its orchestration *topology* matches the requested
// CONSTRUCTION-TIME properties. Two of them today, and the name says the category
// rather than listing them, so a third does not force a third rename:
//   - backend — CPU is N-worker queue-per-Simulator; the GPU route (Metal/CUDA) is a
//     single engine with one large dispatch. It cannot be flipped on a live
//     server (SetPreferredBackend on an N-worker server would run GPU kernels inside
//     the 12-worker structure, the 0.58× anti-pattern this scrum removed).
//   - worker count — ServerImpl builds its Simulator vector in its constructor
//     (server.cpp), so the number of workers is fixed for the life of a server.
// Changing either therefore tears the server down and rebuilds it.
//
// The accumulated image is intentionally discarded: CPU and GPU are
// statistically-equivalent-but-not-identical sample streams, so mixing them in one
// accumulation buffer is physically unsound — a clean reset is the correct
// semantics, and a fresh re-converge is what a backend A/B comparison wants.
//
// Returns true iff the server was reconstructed; the caller must then force a full
// consumer rebuild + fresh poller Start (the new server has no consumers).
// See app.hpp for why this is a function and who has to call it.
void ResetServerConstructionTrackers() {
  g_server_is_gpu = false;
  g_server_worker_count = 0;
}

bool ServerMatchesConstructionProperties(const GuiState& state) {
  return state.use_gpu_backend == g_server_is_gpu && state.worker_count == g_server_worker_count;
}

bool MaybeReconstructServerForConstructionProperties() {
  if (ServerMatchesConstructionProperties(g_state)) {
    return false;  // construction-time properties unchanged — keep the live server
  }
  const bool want_gpu = g_state.use_gpu_backend;
  const int want_workers = g_state.worker_count;
  GUI_LOG_INFO("[GUI] Reconstructing server (backend {} -> {}, workers {} -> {})", g_server_is_gpu ? "GPU" : "CPU",
               want_gpu ? "GPU" : "CPU", g_server_worker_count, want_workers);
  JoinPendingStop();         // R1: a background stop may still hold this server — drain it before destroy
  JoinPendingCalibration();  // R1: so may the startup calibration run
  g_server_poller.Stop();    // synchronous: worker confirmed no longer touching the old server
  LUMICE_DestroyServer(g_server);

  ConstructServerForState(g_state);

  // Re-apply per-server settings that died with the old instance (cf. main.cpp startup).
  LUMICE_SetLogLevel(g_server, static_cast<LUMICE_LogLevel>(g_state.core_log_level));

  // The fresh server restarts its epoch authority at 0. Reset the GUI's display generation so the
  // new backend's low-epoch frames are not fenced out by the old server's carried-over
  // display_epoch_floor (else the preview freezes on the previous backend's texture). See
  // GuiState::ResetDisplayGenerationForBackendSwap. The epoch is re-read from the new server by
  // DoRun's post-commit LUMICE_GetSimLifecycle.
  g_state.ResetDisplayGenerationForBackendSwap();
  return true;
}

// The one emitter of the document a run is given, for BOTH kinds of run. DoRun and DoAnalyze
// call this and nothing else to turn g_state into the LUMICE_Scene they submit, so the scene an
// analysis reports on is byte-for-byte the scene a Run of the same document would render — a
// second BuildScene call site would be a second place for the two to drift apart, and nothing
// downstream could tell (doc/gui/gui-state-governance.md §9: one encoder per intent). The overflow
// report is part of the emitter for the same reason: a document too large for the ABI is refused
// the same way, with the same message, whichever button asked.
ScenePtr BuildCommitSceneOrWarn(const GuiState& state, bool user_initiated) {
  FilterOverflowInfo overflow;
  ColorClassOverflowInfo color_overflow;
  ScenePtr scene = BuildScene(state, SceneIntent::kSimCommit, &overflow, &color_overflow);
  if (scene) {
    return scene;
  }
  // color_overflow.class_index >= 0 iff BuildScene failed on the color-class walk
  // (which runs strictly after the filter walk), rather than a physical-filter overflow.
  // Reusing the filter-overflow wording/limit for a color overflow would misattribute the
  // resource type and print the wrong cap number to the user.
  std::string warning_msg;
  std::string log_locator;
  if (color_overflow.class_index >= 0) {
    log_locator = FormatColorOverflowLocator(color_overflow);
    // User-visible prompt (the Log panel is collapsed by default): the edit did NOT apply
    // and the previous configuration was kept, so the user knows why nothing changed.
    warning_msg = "This raypath color configuration exceeds its limits (" + log_locator +
                  ").\nThe previous configuration was kept. Simplify the color configuration and try again.";
  } else {
    // Locator ("filter \"NAME\", Layer L / Entry E", or "Layer L / Entry E" when unnamed)
    // identifying which filter reference tripped the ABI bounds, captured inside
    // BuildScene. Built by FormatOverflowLocator so the format is unit-testable
    // (test/composition-correctness/gui/test_filter_reconstruct_chain.cpp) rather than only
    // exercised through on-screen GUI.
    log_locator = FormatOverflowLocator(overflow);
    warning_msg = "This filter has too many OR segments / values to apply (limit " +
                  std::to_string(LUMICE_MAX_CONFIG_CLAUSES) + "; " + log_locator +
                  ").\nThe previous configuration was kept. Simplify the filter and try again.";
  }
  // Log-panel dedup: gate the Log write on the SAME state SetGuiWarning uses for modal dedup
  // (in-flight message equals the new one). Sharing the key intentionally couples the two
  // channels — a wording change (e.g. new Layer/Entry after the user switches which filter
  // triggers the overflow) counts as a fresh event and re-logs. A separate static holding the
  // last logged message would be a second dedup key that could disagree with the modal's.
  if (PeekGuiWarning() != warning_msg) {
    GUI_LOG_WARNING("[GUI] BuildCommitSceneOrWarn: {} exceeds ABI limits ({}); keeping the previous configuration.",
                    color_overflow.class_index >= 0 ? "raypath color configuration" : "filter", log_locator);
  }
  // A user-clicked Run (or Analyze) always opens the warning modal, even when the previous
  // OK dismissed it and the overflow condition is unchanged. SetGuiWarning's identity-dedup
  // would otherwise silently swallow the second click. The auto-commit (70ms) path sets
  // user_initiated=false so a stuck-overflow slider drag does not reopen the modal every
  // tick and freeze the UI (app_panels.cpp's warning modal records why "OK to clear" alone
  // is wrong).
  if (user_initiated) {
    ClearGuiWarning();
  }
  SetGuiWarning(warning_msg);
  return nullptr;
}

bool DoRun(bool user_initiated) {
  if (!g_server) {
    return true;
  }
  // R1: a previous Stop may still be draining on the background thread. It calls poller.Stop() +
  // LUMICE_StopServer on this same server; joining first makes the DoRun commit/rebuild below
  // race-free (and the reused-consumer / poller-pointer reasoning valid).
  JoinPendingStop();
  // The startup calibration may still be running the default document on this server. A Run that
  // arrives before it is done queues behind it (bounded by the calibration's own 2 s wait cap) —
  // an accepted trade against cancelling a run that may already be inside a GPU kernel — and the
  // poller Start below must not happen while it is running (see JoinPendingCalibration).
  JoinPendingCalibration();

  // task-metal-gui-commit-backpressure §3 design point 3/6: epoch-keyed backpressure gate.
  // Rationale: kCommitIntervalMs=70ms is faster than Metal's first-batch consume latency
  // (~150ms cold, ~50-80ms after the O2 PSO cache) — without a gate, continuous slider drag
  // restarts the sim before StatsConsumer processes a single batch, starving the preview.
  // Design:
  //   - Gate only fires on the auto-commit path (`user_initiated == false`), and only when a Run
  //     is actively RUNNING AND has not yet consumed any rays.
  //     (LiveSimRays counts root rays consumed by StatsConsumer, reset in CommitConfig on both
  //     rebuild and reuse branches, so `> 0` is equivalent to "first batch landed".)
  //   - Why an explicit Run is exempt: what this gate defends against is a MACHINE cadence —
  //     kCommitIntervalMs re-submits every 70ms, continuously and without gaps, for as long as a
  //     slider is being dragged, which is what outruns the first-batch consume. A click is a
  //     discrete human event and does not have that shape, so exempting it cannot reopen the
  //     starvation this gate was built for. The reverse cost is real and is the reason the
  //     exemption exists at all: gating an explicit Run drops the whole commit silently, so a
  //     user who read the overflow modal, dismissed it and deliberately pressed Run again would
  //     get nothing back — no re-run, no modal, no log line. Not gating it costs nothing today,
  //     because the two user-initiated call sites (the top-bar Run button and the Save-Modified
  //     popup's "Run first") are both drawn only while no Run is in flight, so back-to-back
  //     explicit Runs into the RUNNING-but-unconsumed window are not reachable through the UI at
  //     all; the exemption makes that already-true property hold by construction instead of by
  //     the button-disabling convention alone. See DoRun's declaration in app.hpp for the
  //     obligation this places on any NEW user-initiated caller.
  //   - Timer is keyed by lifecycle.epoch, not a raw wall-clock static. Epoch changes → timer
  //     resets, so a stale timer from a previous gui_test scenario cannot leak into a new one.
  //     Contract (task-364 review): this relies on lifecycle.epoch being a per-server monotonic
  //     generation counter that never reuses 0 for a live-but-unconsumed Run. It is minted by the
  //     server on each Start/CommitConfig (see ServerImpl epoch increment); the failure mode if
  //     that ever regressed to "new server restarts epoch at 0, colliding with the initial
  //     gate_epoch==0" is fail-OPEN (gate opens early, worst case reverts to the pre-task-364
  //     0-rays symptom in a narrow window) — never a deadlock. The `!gate_since.has_value()`
  //     guard below also forces a fresh timer whenever the timer was cleared, covering the
  //     epoch==0-collision edge without depending on the counter's absolute value.
  //   - kQualityGateTimeoutMs (500ms) is a defensive ceiling reused from server_poller.cpp:
  //     if a Run somehow never lands a batch (hung backend), the gate opens after 500ms with
  //     a WARNING rather than blocking commits forever. See plan R2.
  //   - Return false keeps g_state.dirty=true in main.cpp so the next 70ms tick retries with
  //     g_state's latest values ("continuously reflect the latest edit" — plan §3 dp 3).
  static uint64_t gate_epoch = 0;
  static std::optional<std::chrono::steady_clock::time_point> gate_since;
  LUMICE_SimLifecycleResult lc0{};
  LUMICE_GetSimLifecycle(g_server, &lc0);
  if (!user_initiated && lc0.lifecycle == LUMICE_LIFECYCLE_RUNNING) {
    LUMICE_RayCount live_rays = 0;
    LUMICE_GetSimRayCount(g_server, &live_rays);
    if (live_rays == 0) {
      auto now = std::chrono::steady_clock::now();
      if (gate_epoch != lc0.epoch || !gate_since.has_value()) {
        gate_epoch = lc0.epoch;
        gate_since = now;
      }
      auto blocked_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - *gate_since).count();
      if (blocked_ms < gui::kQualityGateTimeoutMs) {
        return false;
      }
      GUI_LOG_WARNING("[GUI] DoRun: backpressure gate timeout ({}ms, epoch={}) — forcing commit with 0 rays landed",
                      blocked_ms, lc0.epoch);
    }
  }

  auto run_start = std::chrono::steady_clock::now();

  // A change to a construction-time property (backend, worker count) reconstructs the server.
  // A reconstructed server has no consumers, so force the full-rebuild path below.
  // No-op (returns false) when neither property has moved since the live server was built.
  bool backend_reconstructed = MaybeReconstructServerForConstructionProperties();

  // Pre-check: will CommitConfig rebuild consumers (destroying old buffers)?
  // Only renderer layout changes (resolution/lens/view/visible/filter) trigger rebuild.
  // High-frequency slider changes (crystal/sun/scattering) always reuse consumers.
  // If reuse is expected, skip Poller Stop — the poller's raw pointers remain valid.
  // NeedsRebuild comparison: RenderConfig::id was removed during the renderer copy-model
  // migration; since `id` was always 1 pre-migration, operator== behavior is strictly looser
  // and cannot produce new false-positive reuse paths.
  // Renderer comparison uses RenderConfigResimFields::Matches (gui_state.hpp) — the same
  // projection gui_state_reconcile.cpp::DiffAgainstCommitBaseline compares against, so this
  // predicate and the reconciler's resim/dirty verdict stay in lock-step (single source of
  // truth; see T2 plan §3 design 1).
  // Excludes all T-view fields (lens_type / fov / elevation / azimuth / roll / visible / front /
  // exposure_offset) because they are client-side reprojection only — the sim renders a fixed
  // full-sky dual-fisheye and the GUI reprojects it (see gui_state.hpp comment). Dragging the
  // view / switching the lens / toggling the hemisphere clip must not cause a poller Stop here.
  // `background` is excluded for the same reason (it changes nothing the simulation computes),
  // and is Revert-tracked separately — see gui_state.hpp's comment block.
  // The last term is not a document diff at all but a session-level fact read straight off the
  // server: CommitConfig never reuses the consumers of an analysis session (server.cpp,
  // `was_analysis`), and the only authority on "was the session an analysis" is the server's own
  // mode — `lc0` was sampled at the top of this function, BEFORE the commit below flips it back
  // to RENDER, which is exactly the moment CommitConfig's exchange reads it. Reading it here is
  // what keeps this predicate a superset of the server's decision (see the mismatch check after
  // the commit) without a shadow flag of our own — `g_state.analysis.started` is an intent for
  // the panel, not a copy of the server's judgement, and must not be used for this.
  bool expect_rebuild = backend_reconstructed || !g_state.last_committed_state.has_value() ||
                        !g_state.last_committed_state->renderer_resim.Matches(g_state.renderer) ||
                        lc0.session_kind == LUMICE_SESSION_ANALYSIS;

  // Single handle commit path (327.4 / 399.5): all filter types — including multi-segment
  // raypath / multi-value EE (expanded to N simple + 1 complex) — go through LUMICE_Scene, so
  // the out_reused signal is always available (no more JSON path that forced full rebuild and
  // tore live-preview buffers on every EE/multi-segment slider drag).
  //
  // Build the scene BEFORE stopping the poller: if a filter exceeds the ABI bounds
  // (clause / term / filter capacity), keep the previously committed state (graceful
  // degradation) — poller untouched, buffer not torn — rather than commit a truncated config.
  // The build and the overflow report are BuildCommitSceneOrWarn's, shared with DoAnalyze.
  ScenePtr scene = BuildCommitSceneOrWarn(g_state, user_initiated);
  if (!scene) {
    return true;
  }

  if (expect_rebuild) {
    g_server_poller.Stop();  // Must pause before consumer destruction
  }

  // Backend preference is now a construction-time property of the server
  // (see MaybeReconstructServerForConstructionProperties) — no per-DoRun SetPreferredBackend push.

  int reused = 0;
  LUMICE_ErrorCode err = LUMICE_CommitScene(g_server, scene.get(), &reused);
  if (err == LUMICE_OK) {
    // task-gui-feedback-affordances Step 7 (AC1): the ABI check above passed
    // (filter / raypath tracing / geometry all fit within their caps), but
    // the CORE may still have dropped color-classification predicates that
    // overflowed the 64-bit component budget (BuildColorGateTable → kNoBit).
    // Surface that as a modal so the user sees "coloring degraded" rather
    // than staring at a partially-uncolored render with no explanation. The
    // Log panel is collapsed by default, so a log-only warning is invisible
    // to most users — this is the fix for the "core degrades silently" trap
    // that was called out in this task's issue.md §A.
    //
    // Two mutually-exclusive branches feed SetGuiWarning inside DoRun:
    //  1) BuildScene failure (above) → commit was REJECTED, previous
    //     config kept. That branch already gated on `user_initiated` to force
    //     a refire on explicit user Runs (Step 2 AC3).
    //  2) This one → commit SUCCEEDED, coloring degrades gracefully. Uses a
    //     different message string (SetGuiWarning's identity-dedup means two
    //     different messages cannot swallow each other). Applies the same
    //     `user_initiated` refire policy so a user who dismisses the modal
    //     and re-Runs with the same still-oversized color config sees the
    //     modal again.
    LUMICE_ColorOverflowInfo color_over{};
    LUMICE_GetColorOverflowInfo(g_server, &color_over);
    // Only component_overflow_count is meaningful HERE: it is set synchronously in
    // CommitConfig (host-side predicate budget), so it is already populated at this
    // post-commit point. The three GPU-only async caps (symmetry-group / OR-summand
    // / color-class) are 0 at DoRun time — CommitConfig just reset them and they are
    // re-populated only on the worker's first batch — so they are surfaced separately
    // by the poll tick in SyncFromPoller. task-color-degrade-gui-surfacing.
    if (color_over.component_overflow_count > 0) {
      std::string degrade_msg =
          "This raypath color configuration exceeds its predicate/symmetry-group budget (" +
          std::to_string(color_over.component_overflow_count) + " predicate(s) and/or " +
          std::to_string(color_over.symmetry_group_overflow_count) +
          " symmetry-group(s) dropped).\n"
          "Affected rays/crystals will render with missing or incomplete color. The underlying "
          "filtering/geometry is unaffected — only color assignment degrades.\n"
          "Simplify the color configuration (fewer distinct predicates per class / fewer symmetry variants) to fix.";
      // Same user_initiated-gated-clear discipline as the BuildScene-failure branch
      // above (AC3): only force-reopen on an explicit user Run. Do NOT unconditionally
      // ClearGuiWarning() before this check (code-review-01 Critical 1) — that would zero
      // g_gui_warning_current ahead of the comparison below, so SetGuiWarning's
      // identity-dedup always sees an empty prior message and reopens the modal on every
      // auto-commit tick (user_initiated=false) even when the SAME overflow persists,
      // reproducing the "70ms slider drag freezes the UI" regression this task's Step 2
      // fixed for the sibling branch.
      if (user_initiated) {
        ClearGuiWarning();
      }
      SetGuiWarning(degrade_msg);
    } else {
      // No color overflow: clear any prior over-bounds warning (the ABI-reject warning from
      // the branch above, or a previous color-degrade warning that has now resolved) so a
      // later re-occurrence of either warns again.
      ClearGuiWarning();
    }
    g_state.last_committed_state = GuiState::ConfigSnapshot::From(g_state);
    // task-color-migration §4 M6 (repush discipline): a fresh commit invalidates every edge-
    // trigger baseline so the next frame's reconciler unconditionally re-pushes the full
    // display-state payload (incl. z_order — the field the commit channel cannot carry, see
    // §3 D2). This is the single fix for the "Run 后 z_order 不生效" bug (AC2, plan §1 偏离 B').
    // Single entry point for all edge-trigger baselines — future third baseline picks up its
    // reset here (a12: 统一原理优先于分类打补丁).
    g_state.InvalidateEffectsBaselines();
    // A new generation invalidates the retained retry source too (plan 814 §2: a new run's frames
    // must not be retried against — or mixed with — the previous generation's failed frame). The
    // ladder's established tier resets with it: the new run walks afresh, one bounded cycle, and
    // whatever tier it needs it re-establishes there.
    g_pending_upload_source.reset();
    g_pending_upload_serial = 0;
    g_recovery.xyz_tier = 0;
    g_recovery.exhausted = false;
    // Intent + epoch readback (I1): the synchronous commit above has already minted (or reused)
    // the lifecycle epoch; read it back so ReconcileSimState keys the display on THIS generation.
    // A filter change always rebuilds (epoch++), so the new epoch strictly exceeds any
    // display_epoch_floor MarkStructHardDirty raised — the floor lifts itself with no explicit unlock.
    LUMICE_SimLifecycleResult lc{};
    LUMICE_GetSimLifecycle(g_server, &lc);
    g_state.committed_epoch = lc.epoch;
    g_state.run_intent = RunIntent::kRunning;
    // The server is a render session again (a commit withdraws the analysis session, server.cpp
    // CommitConfig), so the analysis intent is over; the result on show is deliberately kept — the
    // user is most often re-running the scene they just edited FROM that list.
    g_state.analysis.started = false;
    g_state.stats_ray_seg_num = 0;
    g_state.stats_sim_ray_num = 0;
    g_state.stats_crystal_num = 0;
    g_state.stats_orientation_num = 0;
    // Safety check: if GUI predicted reuse but server rebuilt consumers, the poller was not
    // stopped and may hold dangling pointers. `expect_rebuild` is meant to be a superset of the
    // server's `can_reuse` (server.cpp CommitConfig), which is the conjunction of four terms;
    // how each one is covered, term by term, so this claim can be checked rather than believed:
    //   1. `!consumers_.empty()` — consumers are empty only on a server that has never committed:
    //      `backend_reconstructed` covers a server rebuilt this call, and
    //      `!last_committed_state.has_value()` covers this process's first commit.
    //   2. `!was_analysis` — `lc0.session_kind == LUMICE_SESSION_ANALYSIS` above, the server's own
    //      mode read back before the commit flips it (this used to be the missing term: every
    //      Run after an Analyze landed in this branch, deterministically).
    //   3. per-renderer layout (`NeedsRebuild(RenderConfig)`: resolution / lens / lens_shift / view
    //      / visible / front / overlap) — the kSimCommit scene submits ONE renderer whose every
    //      layout field but the resolution is a constant (file_io.cpp BuildScene: fixed dual
    //      equal-area full-sky texture, the view settings are shader uniforms), so
    //      `renderer_resim.Matches` on `sim_resolution_index` is the whole of the diff.
    //   4. `!class_table_changed` (a raypath-color class gained/lost or changed its
    //      combine/member_bits) — NOT covered: nothing here looks at the color config's structure,
    //      so a structural raypath-color edit committed without a resolution change can still
    //      land in this branch. Known and left alone; this fallback is what carries it.
    // So the branch is reachable through term 4 only. If it fires for another reason, a term
    // has been added to `can_reuse` without an entry above.
    if (!expect_rebuild && !reused) {
      GUI_LOG_WARNING(
          "[GUI] DoRun: predict/actual mismatch! GUI predicted reuse but server rebuilt. "
          "Stopping poller to prevent dangling pointer.");
      g_server_poller.Stop();
    }
    if (expect_rebuild || !reused) {
      g_server_poller.Start(g_server);  // Rebuild: new consumers, reset server pointer
    }
    // Unconditionally ensure poller is running — covers all edge cases:
    // DoStop→DoRun (poller was paused), PollOnce self-pause (finite rays done), etc.
    // If already kRunning, this is a no-op (zero overhead). WakeForRestart (not
    // WakeForRefresh): a fresh commit is a real re-arm, so valid=false publish is
    // desired (consumers must ignore any stale terminal snapshot until the worker
    // republishes for the new epoch).
    g_server_poller.WakeForRestart(g_server);
    // No staged-texture invalidate here: the epoch floor (raised by MarkStructHardDirty) already
    // fences the old generation's payloads, and the freshly committed epoch (read back above)
    // lifts that fence. Crystal-scrub reuse (no filter change) MUST keep carry-forward, so we
    // deliberately do not drop the staged texture.
    auto run_end = std::chrono::steady_clock::now();
    GUI_LOG_INFO("[GUI] DoRun: config committed ({:.1f}ms)",
                 std::chrono::duration<double, std::milli>(run_end - run_start).count());
  } else {
    GUI_LOG_WARNING("[GUI] CommitConfig FAILED with error code {}", static_cast<int>(err));
    // Restore poller if it was stopped for rebuild but CommitConfig failed.
    // task-color-migration Minor 4: mechanical rename from the former EnsureRunning
    // to WakeForRestart. Semantics of this failure-recovery branch are unchanged (the
    // former name's behavior — publish valid=false on the wake edge — is preserved by
    // WakeForRestart). A deeper review of this branch's behavior is out of scope for
    // this task (§7 risk 5); if further changes are needed, they will be independently
    // scoped.
    if (expect_rebuild) {
      g_server_poller.WakeForRestart(g_server);
    }
  }
  // Both err==LUMICE_OK and err!=LUMICE_OK paths issued LUMICE_CommitScene;
  // "committed" here means "gate did not defer this attempt", not "commit succeeded".
  // Callers use this to gate throttle-side accounting (see main.cpp / test_gui_perf.cpp).
  return true;
}

bool DocumentContinuable(const GuiState& state) {
  return MatchesCommitExceptRayBudget(state) && ServerMatchesConstructionProperties(state);
}

bool DoContinue() {
  if (!g_server) {
    return false;
  }
  // Same two joins DoRun opens with, for the same reasons: a Stop may still be draining on the
  // background thread (it is the usual way here — Stop, then Continue), and the startup
  // calibration may still hold the server.
  JoinPendingStop();
  JoinPendingCalibration();
  const bool infinite = g_state.sim.infinite;
  const LUMICE_ErrorCode err =
      LUMICE_ContinueRender(g_server, infinite ? 1 : 0, infinite ? 0 : SimRayCount(g_state.sim));
  if (err != LUMICE_OK) {
    // The button is gated on WhyCannotContinue, so a refusal is the surprise it is: logged, and
    // nothing on the GUI side moves.
    GUI_LOG_WARNING("[GUI] DoContinue: LUMICE_ContinueRender failed with error code {}", static_cast<int>(err));
    return false;
  }
  // The budget the server is now tracing is the panel's. Recording it in the Revert baseline keeps
  // the resim diff honest: the ray budget was the one field the gate let differ, and after this it
  // no longer does. Nothing else in the baseline moves — the scene it describes is unchanged.
  if (g_state.last_committed_state) {
    g_state.last_committed_state->sim.ray_num_millions = g_state.sim.ray_num_millions;
    g_state.last_committed_state->sim.infinite = g_state.sim.infinite;
  }
  // From here on a continuation is a run like any other to the display side: a new epoch read
  // back (the server advanced it), the kRunning intent, and the poller woken for the new epoch.
  // What it deliberately does NOT do, unlike DoRun: zero the stats readouts, or raise the display
  // epoch floor — the picture and the counts on screen stay until this run's first frame carries
  // larger ones, because they are a prefix of what it will show.
  LUMICE_SimLifecycleResult lc{};
  LUMICE_GetSimLifecycle(g_server, &lc);
  g_state.committed_epoch = lc.epoch;
  g_state.run_intent = RunIntent::kRunning;
  // The poller is paused after a Stop and self-paused after a completion; the consumers were not
  // rebuilt, so its pointers are still valid and a wake is all it needs (DoRun's reuse path).
  g_server_poller.WakeForRestart(g_server);
  GUI_LOG_INFO("[GUI] DoContinue: continuing the render (epoch {}, {} more rays)", lc.epoch,
               infinite ? std::string("unbounded") : std::to_string(SimRayCount(g_state.sim)));
  return true;
}

void DoStop() {
  if (!g_server)
    return;
  // Idempotent: a stop is already draining on the background thread (kStopping intent-latched).
  // Re-entry (double-click, or a UI path that slipped past the disabled button) is a no-op — the
  // future is single-owner and must not be overwritten while pending.
  if (g_state.run_intent == RunIntent::kStopping) {
    return;
  }
  // The startup calibration may still be running on this server (see JoinPendingCalibration):
  // it holds g_server and, on the way out, calls LUMICE_StopServer itself (StopOnExit in
  // RunCalibrationInBackground). Without this join, that call and the async Stop lambda's own
  // LUMICE_StopServer(srv) below could run concurrently on the same server pointer. Same
  // precedent as DoRun's top-of-function join; the queued wait is bounded by the calibration's
  // own 2 s cap and only ever engaged in the narrow startup window before it finishes.
  JoinPendingCalibration();
  // Optimistic async Stop (blueprint §5/§8): set the intent synchronously so the UI paints
  // "Stopping…" THIS frame, then offload the EXISTING blocking teardown sequence onto a background
  // thread. The backend cannot interrupt the in-flight batch, so LUMICE_StopServer blocks until it
  // drains (server.cpp active_workers_==0); doing it off the UI thread keeps the toolbar live.
  // This writes the INTENT only — sim_state stays single-owner (ReconcileSimState in SyncFromPoller).
  g_state.run_intent = RunIntent::kStopping;
  // A Stop ends whichever run is in flight. For an analysis the intent is withdrawn here, at the
  // command, rather than waited for from the poller: Stop() pauses the poller, so the last
  // observation it published (RUNNING) would otherwise stand until the next wake and keep the
  // in-progress flag up over a run that is over. The result adopted so far stays on show.
  g_state.analysis.started = false;
  g_stop_inflight.store(true);
  g_stop_future = std::async(std::launch::async, [srv = g_server] {
    g_server_poller.Stop();  // fast: wait out one readback poll (reuses the validated stop order)
    LUMICE_StopServer(srv);  // slow: blocks on the in-flight batch drain — now off the UI thread
    g_stop_inflight.store(false);
  });
  GUI_LOG_INFO("[GUI] DoStop: stopping (async)");
}

bool DoAnalyze() {
  if (!g_server) {
    return false;
  }
  // Same reason DoRun joins first: a Stop may still be draining on the background thread, and the
  // server call below must not race it; and the startup calibration may still be running.
  JoinPendingStop();
  JoinPendingCalibration();
  // The document on the panels, through the same emitter DoRun uses (its comment says why it
  // must be the same one). An analysis is always a deliberate click, so a document too large for
  // the ABI reopens the warning the way an explicit Run does, and nothing is analysed — not a
  // truncated document, and not the previous one.
  ScenePtr scene = BuildCommitSceneOrWarn(g_state, /*user_initiated=*/true);
  if (!scene) {
    return false;
  }
  // The IN_FRAME canvas is the preview's own when there is one — the frame on screen is what
  // "in frame" means. Without a preview the frame is still the document's own view, sized at
  // the resolution the document would render at: PreviewAnnotationView reads only the state
  // and the canvas size, so this is the same frame a preview of that size would show. (The
  // other two modes never read the canvas.)
  const int doc_res = kSimResolutions[g_state.renderer.sim_resolution_index];
  const int canvas_w = g_preview_vp.active ? g_preview_vp.vp_w : doc_res;
  const int canvas_h = g_preview_vp.active ? g_preview_vp.vp_h : doc_res;
  const LUMICE_RaypathAnalysisRequest req = BuildAnalysisRequest(g_state, canvas_w, canvas_h);
  const LUMICE_ErrorCode err = LUMICE_StartRaypathAnalysis(g_server, scene.get(), &req);
  if (err != LUMICE_OK) {
    GUI_LOG_WARNING("[GUI] DoAnalyze: LUMICE_StartRaypathAnalysis failed with error code {} (roi_mode={})",
                    static_cast<int>(err), req.roi_mode);
    return false;
  }
  // Intent + an empty view: the previous result must not sit under this run's list, and the
  // held generation stays as it was — the run's first snapshot carries a larger one (the
  // counter only grows), so the first result is adopted without any reset of the cursor.
  g_state.analysis.started = true;
  g_state.analysis.selected_entry.reset();
  // What this run was asked about, for the "centre has moved" hint: the request's own copy, so
  // it is the value the server received and not a re-read of the session field.
  std::copy(req.cone_center, req.cone_center + 3, g_state.analysis.analyzed_cone_center_dir);
  g_state.analysis_result = GuiState::AnalysisResultView{};
  // The scene this run describes: the same g_state BuildCommitSceneOrWarn just read, nothing
  // having written it in between. The panel compares the live document against it each frame.
  g_state.analysis_result.analyzed_scene = GuiState::AnalysisSceneIdentity::From(g_state);
  // The poller's snapshot still carries the previous result forward, and with the view just
  // cleared (held generation 0) the next SyncFromPoller would adopt it as new. Drop it there too.
  g_server_poller.InvalidateAnalysisResult();
  // Same wake as a fresh commit: the poller publishes valid=false across the edge, so the first
  // observation SyncFromPoller derives the in-progress flag from is this run's, not the last
  // render's COMPLETED.
  g_server_poller.WakeForRestart(g_server);
  GUI_LOG_INFO("[GUI] DoAnalyze: analysis run started (roi_mode={})", req.roi_mode);
  return true;
}

void DoRevert() {
  if (g_state.last_committed_state) {
    const auto& snapshot = *g_state.last_committed_state;
    // Restore configuration fields atomically, then fire GUI side effects.
    // Order rationale: ApplyTo() is pure field assignment. Runtime state (run_intent,
    // committed_epoch, poller counters, etc.) is intentionally preserved by ApplyTo.
    snapshot.ApplyTo(g_state);
    // Frontend reset delegated to the single owner (kRevert subset = OnLayerStructureChanged +
    // InvalidateEffectsBaselines only; preview/staged/trackball preserved because Revert is
    // config restore, not a document switch). InvalidateEffectsBaselines is task-color-migration
    // §4 M6 (repush discipline): revert restores display fields but the server still holds the
    // post-edit display state — invalidating the display-push baseline forces the next frame's
    // reconciler to re-push the restored display payload so the server catches up to Revert.
    // Fixes the "Revert 不重推颜色 display 态" bug (plan §1 偏离 C).
    ResetFrontendState(g_state, FrontendResetReason::kRevert);
    // Revert restores config == committed, so it is no longer dirty. This is load-bearing under
    // the single-owner reconcile: with the old direct sim_state=kDone write gone, a leftover
    // dirty=true would make ReconcileSimState re-derive kDone+dirty → kModified every frame
    // (R5 — the revert would appear not to take). Clearing dirty settles it back to kDone.
    g_state.dirty = false;
  }
}

// Terminal display state a drained Stop resolves to (single flip point). Blueprint §8 originally
// abstracted this as "Idle", but that was reconsidered to kDone (2026-07-03, doc §8): the GUI's Save guards are
// keyed on kIdle (RefreshCpuTextureForSave early-return + Save-menu `has_server`), so Stop→Idle
// would regress save-after-stop, and kIdle would be overloaded with "never-run" vs
// "stopped-with-result". kDone == current terminal behavior, so the Save guards need no migration.
// A future flip back to kIdle is this one line + the Save-guard migration.
constexpr GuiState::SimState kStopEndState = GuiState::SimState::kDone;

// Single-owner sim_state reconcile (I2). Pure — see app.hpp for the contract. The §1.3 truth
// table: intent picks a base state; a fresh COMPLETED observation is the only thing that pulls a
// kRunning intent to kDone; a dirty edit on a kDone result surfaces as kModified.
GuiState::SimState ReconcileSimState(RunIntent intent, uint64_t committed_epoch, const PreviewSnapshot* snap,
                                     bool dirty) {
  using S = GuiState::SimState;
  // "Fresh" = the observation belongs to THIS committed generation (I1). A stale epoch (< committed,
  // e.g. an old snapshot or a transient poll after a re-commit bumped the epoch) is discarded so a
  // prior generation's COMPLETED never masquerades as this generation's completion. epoch > committed
  // cannot happen (the GUI reads the epoch back synchronously on commit).
  const bool fresh = snap != nullptr && snap->valid && snap->epoch == committed_epoch;
  S base;
  switch (intent) {
    case RunIntent::kNone:
      base = S::kIdle;  // fresh session / JSON import / .lmc without a baked texture
      break;
    case RunIntent::kLoaded:
      base = S::kDone;  // static loaded result (no server run backing it)
      break;
    case RunIntent::kStopping:
      base = S::kStopping;  // optimistic: async Stop is draining the in-flight batch (pure intent —
                            // does NOT depend on a fresh observation, and is not demoted by dirty)
      break;
    case RunIntent::kStopped:
      base = kStopEndState;  // drained terminal state; single flip point (see kStopEndState above)
      break;
    case RunIntent::kRunCompleted:
      // task-color-migration §3 D4: intent-latched terminal state for a natural (finite) run
      // completion. Once SyncFromPoller has advanced the intent to kRunCompleted (having observed
      // a fresh COMPLETED snapshot at the committed epoch — see the Mealy edge below), the base
      // stays kDone regardless of subsequent snapshot volatility. Structurally immune to a
      // transient valid=false observation pulling the completed run back into kSimulating — the
      // second layer of the AC1 activity-bug defense (belt-and-suspenders with WakeForRefresh).
      base = S::kDone;
      break;
    case RunIntent::kRunning:
      base = (fresh && snap->lifecycle == LUMICE_LIFECYCLE_COMPLETED) ? S::kDone : S::kSimulating;
      break;
  }
  // A dirty edit layered on a completed result is kModified (old MarkDirty kDone→kModified rule).
  // kIdle/kSimulating are never demoted by dirty.
  return (base == S::kDone && dirty) ? S::kModified : base;
}

// Record one failed attempt into the minimal incident record and arm the notice. A NEW failure
// always re-shows the notice (the dismissal is one-time per incident, not forever).
static void RecordUploadIncident(const UploadResult& result, int tier_tried, unsigned long long texture_serial) {
  if (!g_recovery.has_incident) {
    g_recovery.notice_dismissed = false;
  }
  g_recovery.has_incident = true;
  g_recovery.stage = static_cast<int>(result.stage);
  g_recovery.gl_error = result.gl_error;
  g_recovery.request_width = result.request_width;
  g_recovery.request_height = result.request_height;
  g_recovery.tier_tried = tier_tried;
  g_recovery.texture_serial = texture_serial;
}

// Execute ONE ladder candidate against a payload: tier 0 = the normal PBO path, tier 1 = full
// resolution from client memory (no staging), tier >= 2 = prefiltered 2^(tier-1) per axis from
// the ORIGINAL floats, still client memory. Each candidate is attempted at most once per cycle;
// the prefilter cost is paid only on the recovery path, never on a healthy full-size upload.
static UploadResult UploadXyzAtTier(const float* xyz, int width, int height, int tier) {
  if (tier <= 0) {
    return g_preview.UploadXyzTexture(xyz, width, height);
  }
  if (tier == 1) {
    return g_preview.UploadXyzTextureNoStaging(xyz, width, height);
  }
  const int steps = tier - 1;
  int dst_w = 0;
  int dst_h = 0;
  if (!DowngradedSize(width, height, steps, &dst_w, &dst_h)) {
    // Past the ladder's end: nothing to try (the caller's cycle should have stopped already).
    UploadResult r;
    r.stage = UploadResult::Stage::kInvalidInput;
    r.request_width = width;
    r.request_height = height;
    return r;
  }
  // The bounded candidate (see DowngradeXyzPlaneBounded): canonical-half input, one-pass box
  // average, and a working set of the destination plane plus ONE source row — no full-size
  // canonical plane, no full-size half plane, no quarter scratch. A machine too tight for even
  // that fails the allocation, and the failure is REPORTED like any other stage: an
  // std::bad_alloc escaping the recovery state machine would terminate the process exactly when
  // the latch-and-unavailable state is what the plan asks for.
  try {
    std::vector<float> downgraded;
    int applied_w = 0;
    int applied_h = 0;
    if (!DowngradeXyzPlaneBounded(xyz, width, height, steps, &applied_w, &applied_h, downgraded)) {
      UploadResult r;
      r.stage = UploadResult::Stage::kInvalidInput;
      r.request_width = width;
      r.request_height = height;
      return r;
    }
    return g_preview.UploadXyzTextureNoStaging(downgraded.data(), applied_w, applied_h);
  } catch (const std::bad_alloc&) {
    GUI_LOG_ERROR(
        "[GUI] Ladder: host allocation failed for a {}x{} recovery candidate — reported as a "
        "failed stage",
        width, height);
    UploadResult r;
    r.stage = UploadResult::Stage::kCpuAllocation;
    r.request_width = width;
    r.request_height = height;
    return r;
  }
}

// Run ONE authorized recovery cycle: candidate 0 upward, stopping at the first success, each
// candidate attempted at most once. `probe_full_ladder` false runs ONE attempt at the
// established tier only — the steady-state shape for ongoing frames of a generation whose tier
// is already known (a constrained machine must not re-probe giant candidates at the poll rate).
// Finite by construction: the ladder ends at the smallest dual-disc frame the projection can
// still carry (preview_downgrade.hpp), and each candidate is tried once. The FIRST failure of
// the cycle is recorded into the incident record here, and nowhere else — one writer per cycle
// (plan 814 §3.4: the first failure chain survives later successes; a NEW cycle's first failure
// legitimately supersedes the previous cycle's, and successes never erase the record). The
// XyzUploadCycle it fills is declared beside the other source-adjudication state above, where
// the open path first calls this.
static XyzUploadCycle RunXyzUploadCycle(const float* xyz, int width, int height, unsigned long long texture_serial,
                                        bool probe_full_ladder) {
  XyzUploadCycle out;
  bool first_failure_recorded = false;
  for (int tier = probe_full_ladder ? 0 : g_recovery.xyz_tier;; ++tier) {
    // Stepping off the PBO path retires BOTH staging slots now, not at the cycle's exit (the r4
    // release-timing ruling: a retired resource's storage leaves before the next strategy
    // allocates). From this candidate on the walk uploads from client memory, and the other
    // slot's full-size storage — 192 MiB a slot at 8192x4096 — is part of the allocation
    // pressure this candidate walks against: a walk that would fit once the slots are retired
    // must not fail, or drop a further tier, while dead storage is still held. Idempotent at an
    // established tier >= 1 (already released when the walk established it); the tier-0 path
    // re-allocates its slot lazily if a later cycle re-establishes it.
    if (tier == 1) {
      g_preview.ReleaseStagingSlots();
    }
    const UploadResult r = UploadXyzAtTier(xyz, width, height, tier);
    if (r.ok) {
      out.displayed_tier = tier;
      break;
    }
    if (!first_failure_recorded) {
      RecordUploadIncident(r, tier, texture_serial);
      out.first_failure = r;
      first_failure_recorded = true;
    }
    if (!probe_full_ladder) {
      break;  // the established tier's single attempt is the whole cycle
    }
    // Termination: is there a NEXT candidate? DowngradedSize with steps = tier+1 asks exactly
    // that (and must get REAL out-pointers — the null form is its "arguments invalid" refusal,
    // not "no candidate"; a null call here made every reduced candidate unreachable and ended
    // each walk after the two full-size tiers).
    int next_w = 0;
    int next_h = 0;
    // The NEXT candidate is tier+1, whose step count is (tier+1)-1 = tier — querying tier+1
    // STEPS here skipped one legal candidate and the floor-sized tier was never attempted.
    if (tier > 0 && !DowngradedSize(width, height, tier, &next_w, &next_h)) {
      break;
    }
  }
  // The in-loop retirement above releases the slots on every walk that reaches tier 1; this
  // exit still owns the shape that never gets there — an established-tier-0 attempt (or a
  // tier-0-terminated walk) that failed hard leaves the slots' fence/buffer lifecycle ended
  // here rather than carried into the unavailable state. The tier-0 path re-allocates lazily.
  if (out.displayed_tier != 0) {
    g_preview.ReleaseStagingSlots();
  }
  return out;
}

// Display upload gate (I1/I6) — pure predicate, see app.hpp. Uploads a payload only when it is a
// fresh (unseen serial), non-empty frame (intensity>0 or a valid zero-ray terminal frame) whose
// epoch clears display_epoch_floor. The floor (raised by MarkStructHardDirty) fences the old
// generation; a null / cold-start payload is skipped so GL keeps the last frame (no black flicker).
bool ShouldUploadPayload(const PreviewSnapshot& snap, unsigned long long last_uploaded_texture_serial,
                         uint64_t display_epoch_floor) {
  const auto& payload = snap.payload;
  return payload != nullptr && snap.texture_serial != last_uploaded_texture_serial &&
         (payload->snapshot_intensity > 0 || payload->texture_ray_count > 0) &&
         payload->payload_epoch > display_epoch_floor;
}

// 814 upload failure latch — see app.hpp. Pure predicate, pinned for the same reason the other
// gates are: the decision boundary is testable headlessly, and SyncFromPoller reads it instead of
// re-deriving it inline.
bool ShouldBlockUploadForLatch(bool upload_latched, unsigned long long latch_epoch,
                               unsigned long long committed_epoch) {
  return upload_latched && latch_epoch == committed_epoch;
}

// Stats-apply gate — see app.hpp. Deliberately reads snap.stats_epoch and NOT snap.epoch: the
// bundle epoch is re-stamped on every poll, so it says nothing about which generation produced
// the stats travelling with it.
bool ShouldApplyStats(const PreviewSnapshot& snap, uint64_t committed_epoch) {
  return snap.stats_epoch == committed_epoch && snap.stats_sim_ray_num > 0;
}

// task-345.4 effective composite/xyz choice — see app.hpp. Pure boolean AND: server must have
// produced a composite (raypath_color active) AND the user preference must be "show composite".
// When raypath_color is empty the poller leaves is_composite=false → this returns false → the
// upload branch falls back to the XYZ float path (AC4 zero-regression).
bool ShouldUseCompositeUpload(bool payload_is_composite, bool show_composite_preview) {
  return payload_is_composite && show_composite_preview;
}

// The effective composite preference: the user's toggle MINUS the channel-B-R exclusion — the
// diagnostic reads B-R off the NORMAL (full-spectrum) picture, so while it is on screen the
// composite preference is read as off (the toggle itself is untouched and applies again the
// moment the display mode changes back). ONE adjudication for SyncFromPoller's live upload,
// Retry's target mode and both success commits: the r5 finding had Retry recompute the
// preference without the exclusion, so a B-R-mode retry of a failed composite frame uploaded
// the palette instead of the spectrum and recorded the display mode to match.
static bool EffectiveShowCompositePreview() {
  return g_state.show_composite_preview && !IsChannelBrDisplay(g_state.renderer);
}

// task-345.4 fire-branch gate — see app.hpp. Combines the standard serial-dedup gate and the
// mode-change OR-branch that keeps display-time preference flips responsive without a new poll.
bool ShouldFireCompositeUpload(const PreviewSnapshot& snap, unsigned long long last_uploaded_texture_serial,
                               uint64_t display_epoch_floor, bool show_composite_preview,
                               bool last_uploaded_as_composite) {
  const bool payload_available = snap.payload != nullptr;
  const bool effective_composite =
      payload_available && ShouldUseCompositeUpload(snap.payload->is_composite, show_composite_preview);
  const bool mode_changed = payload_available && effective_composite != last_uploaded_as_composite;
  return ShouldUploadPayload(snap, last_uploaded_texture_serial, display_epoch_floor) || mode_changed;
}

// task-348.3 (⑦) — see app.hpp. Pure predicate: no globals, no GL. Kept trivial on purpose
// so the "only fire on false→true transition" time-guard lives at the call site and is
// obvious in the diff (RenderTopBar), while this function pins the decision boundary that
// gui_test can assert without an ImGui/GL context.
bool ShouldDefaultEnableColorsOnOpen(bool raypath_color_empty) {
  return raypath_color_empty;
}

// task-gui-feedback-affordances Step 1 (AC2) — see app.hpp. Pure predicate,
// intentionally trivial: the tint decision boundary is `raypath_color` is
// non-empty, and pinning it in a testable function prevents the top-bar
// rendering call site from drifting away from the same read.
bool ShouldTintColorsButton(bool raypath_color_empty) {
  return !raypath_color_empty;
}

namespace {

// Magnitude notation shared with the status bar's "Total rays" segment (app_panels.cpp): a count is
// shown as "5.4 x10^6". Keeping one notation across the whole row matters because the sampling
// readout sits directly beside "Total rays" — two notations side by side read as two different
// kinds of quantity. Below 1e3 the scaled form would be sillier than the plain integer ("1 per
// 0.0 x10^3 rays"), so small ratios print bare.
std::string FormatMagnitude(double v) {
  char buf[64];
  if (v >= 1e9) {
    snprintf(buf, sizeof(buf), "%.1f x10^9", v / 1e9);
  } else if (v >= 1e6) {
    snprintf(buf, sizeof(buf), "%.1f x10^6", v / 1e6);
  } else if (v >= 1e3) {
    snprintf(buf, sizeof(buf), "%.1f x10^3", v / 1e3);
  } else {
    snprintf(buf, sizeof(buf), "%.0f", v);
  }
  return buf;
}

// Thousands-grouped raw count for the tooltip ("5,419,520"). Hand-rolled rather than via
// std::locale: the grouping character is locale-dependent, and a reference screenshot taken on one
// machine must not depend on the locale of the machine that runs the test.
std::string FormatGrouped(LUMICE_RayCount n) {
  std::string digits = std::to_string(n);
  std::string out;
  int count = 0;
  for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
    if (count > 0 && count % 3 == 0) {
      out.push_back(',');
    }
    out.push_back(*it);
    ++count;
  }
  std::reverse(out.begin(), out.end());
  return out;
}

// Placeholder for both "nothing traced yet" and "no draws recorded". One shared spelling on
// purpose: from the user's side the two are the same statement — this run has no density to report.
constexpr const char* kSamplingNotAvailable = "n/a";

// Single implementation behind both spellings — see app.hpp. `with_rays_word` only ever adds or
// omits a trailing literal; every number is computed once, here.
std::string FormatDensityImpl(LUMICE_RayCount draws, LUMICE_RayCount rays, bool with_rays_word) {
  if (rays == 0 || draws == 0) {
    return kSamplingNotAvailable;
  }
  if (draws >= rays) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f/ray", static_cast<double>(draws) / static_cast<double>(rays));
    return buf;
  }
  std::string out = "1 per " + FormatMagnitude(static_cast<double>(rays) / static_cast<double>(draws));
  if (with_rays_word) {
    out += " rays";
  }
  return out;
}

}  // namespace

// See app.hpp for the full branch contract.
std::string FormatSamplingDensity(LUMICE_RayCount draws, LUMICE_RayCount rays) {
  return FormatDensityImpl(draws, rays, true);
}

std::string FormatSamplingDensityCompact(LUMICE_RayCount draws, LUMICE_RayCount rays) {
  return FormatDensityImpl(draws, rays, false);
}

std::string FormatSamplingSegment(LUMICE_RayCount crystals, LUMICE_RayCount orientations, LUMICE_RayCount rays) {
  // No "Sampling:" prefix: the two labels below already say what the numbers are, the tooltip's
  // first line spells it out in full, and at 1024 px (the enforced minimum window width) the row
  // has no spare space for a word that carries no information the labels do not.
  // U+00B7 MIDDLE DOT, spelled as UTF-8 bytes so the source file stays plain ASCII.
  return "| shape " + FormatSamplingDensityCompact(crystals, rays) + " \xC2\xB7 orient " +
         FormatSamplingDensityCompact(orientations, rays);
}

std::string FormatSamplingTooltip(LUMICE_RayCount crystals, LUMICE_RayCount orientations, LUMICE_RayCount rays) {
  // Raw counts alongside the densities: the density answers "is this dimension sampled?", the raw
  // count is what a user quotes in a bug report, and neither substitutes for the other.
  std::string out = "Sampling density -- this run\n";
  out += "  Rays traced         " + FormatGrouped(rays) + "\n";
  out += "  Shape draws         " + FormatGrouped(crystals) + "   " + FormatSamplingDensity(crystals, rays) + "\n";
  out += "  Orientation draws   " + FormatGrouped(orientations) + "   " + FormatSamplingDensity(orientations, rays) +
         "\n\n";
  out += kSamplingCrossBackendNote;
  return out;
}

// task-348.3 (⑤/⑥) shared writer — see app.hpp. Two-line function on purpose: the whole
// point is that both write sites go through the same left-value assignment so future
// code cannot introduce a third variant that reads/writes the wrong field.
void ToggleCompositePreview(GuiState& state) {
  state.show_composite_preview = !state.show_composite_preview;
}

void SyncFromPoller() {
  // Atomically load the whole immutable snapshot (invariant I5). One lock-free atomic_load; the
  // consumer never observes a half-updated field combination.
  auto snap = g_server_poller.LoadSnapshot();

  // NOTE (scrum-gui-state-reconcile T0, M6): the ApplyGuiEffects(ReconcileGuiEffects(...)) call
  // that used to live here was moved to the end of the main-loop frame (main.cpp, after all
  // Render*() calls). Rationale: SyncFromPoller runs at the TOP of each frame (main.cpp:278),
  // BEFORE widget rendering — so a frame-top reconcile could only observe field edits from the
  // PREVIOUS frame, introducing a one-frame delay vs. the legacy DIRTY_IF wrappers which set
  // dirty synchronously in the widget call site (code-review Round 3 Major #1, CONFIRMED).
  // Placing the reconcile at frame tail restores same-frame semantics: edit in frame N → tail
  // reconcile of frame N sets dirty → frame N+1's SyncFromPoller-driven ReconcileSimState below
  // observes it exactly like the legacy path did. ReconcileSimState stays at frame top because
  // it depends on the freshly-loaded poller snapshot.

  // Level-triggered single-owner reconcile (I2/I3): unconditionally derive sim_state every frame
  // from (intent, committed_epoch, observation, dirty). No early-return on !=kSimulating — the
  // durable COMPLETED snapshot self-heals to kDone on any future frame even after the poller
  // self-pauses, and loaded/idle states also need per-frame derivation. This is the ONLY sim_state
  // writer in production.
  const auto prev_state = g_state.sim_state;
  g_state.sim_state = ReconcileSimState(g_state.run_intent, g_state.committed_epoch, snap.get(), g_state.dirty);
  if (prev_state != SimState::kDone && g_state.sim_state == SimState::kDone) {
    GUI_LOG_INFO("[GUI] Simulation done");
  }

  // The analysis run's display state, derived the same way and beside it: intent + observation,
  // every frame, one writer. A render commit does not bump the lifecycle epoch for an analysis
  // (server.cpp StartRaypathAnalysis: same scene, same epoch), so nothing above changes while one
  // runs — the picture is still the render's, and sim_state says so.
  {
    const bool prev_in_progress = g_state.analysis_run_in_progress;
    g_state.analysis_run_in_progress = DeriveAnalysisInProgress(g_state.analysis.started, snap.get());
    if (snap && snap->valid) {
      g_state.server_session_is_analysis = snap->session_kind == LUMICE_SESSION_ANALYSIS;
    }
    if (prev_in_progress && !g_state.analysis_run_in_progress) {
      GUI_LOG_INFO("[GUI] Raypath analysis finished");
    }
    // The result: adopted when its generation is new, ignored otherwise (a carry-forward of the
    // result already on show is what every poll after the last snapshot carries).
    // The poller's payload names the result; its entries are read here, on this thread, under
    // the panel's symmetry — in the same frame as the adoption, so the list is never a frame late.
    if (snap && AdoptAnalysisPayloadIfNew(g_state, snap->analysis)) {
      RefreshAnalysisEntries(g_state, g_server);
      GUI_LOG_VERBOSE("[GUI] SyncFromPoller: analysis result adopted ({} entries, gen={})",
                      g_state.analysis_result.payload->entries.size(),
                      g_state.analysis_result.payload->snapshot_generation);
    }
  }

  // Mealy next-intent advance for the async Stop (blueprint §5/§8): once the background stop thread
  // has drained (g_stop_inflight cleared, i.e. LUMICE_StopServer returned), promote the optimistic
  // kStopping intent to the terminal kStopped. This writes the INTENT (a reconcile INPUT) only —
  // sim_state is still written in exactly one place above (I2 preserved). Placed AFTER the reconcile
  // so the terminal kStopEndState paints on the NEXT frame's reconcile.
  if (g_state.run_intent == RunIntent::kStopping && !g_stop_inflight.load()) {
    g_state.run_intent = RunIntent::kStopped;
  }

  // Mealy next-intent advance for a natural run completion (task-color-migration §3 D4 /
  // doc/gui/gui-state-governance.md §4 支柱 2). Symmetric to the kStopping→kStopped promote above:
  // the first time we observe a fresh COMPLETED snapshot at the committed epoch under a kRunning
  // intent, latch the intent to kRunCompleted so subsequent frames stay kDone even if a later
  // snapshot briefly observes valid=false. Same "writes INTENT only, reconcile is single-owner
  // over sim_state" contract as the Stopping→Stopped promote. Placed AFTER the reconcile so the
  // latched terminal state paints on the NEXT frame's reconcile.
  //
  // DoRun re-arms this by unconditionally writing run_intent=kRunning on a successful commit
  // (see app.cpp:855), so any subsequent Run naturally invalidates the prior latch.
  if (g_state.run_intent == RunIntent::kRunning && snap && snap->valid && snap->epoch == g_state.committed_epoch &&
      snap->lifecycle == LUMICE_LIFECYCLE_COMPLETED) {
    g_state.run_intent = RunIntent::kRunCompleted;
  }

  // task-color-degrade-gui-surfacing: poll the ASYNC GPU color-degrade tally and
  // surface a modal once per committed epoch. The three GPU device caps
  // (symmetry-group / OR-summand / color-class) fire on the worker's first batch,
  // AFTER DoRun's synchronous component check, so the poll tick is the only place
  // they can be observed. Epoch-keyed dedup keeps it from re-firing every frame
  // while letting a new Run (new epoch) re-warn — mirrors the DoRun modal policy.
  // CPU backend has no such caps and always reports zeros, so this never fires on
  // CPU. Runs before the snapshot-validity early-return below because the tally
  // comes from the C API, not the poller snapshot.
  {
    LUMICE_ColorOverflowInfo co{};
    if (LUMICE_GetColorOverflowInfo(g_server, &co) == LUMICE_OK &&
        (co.symmetry_group_overflow_count > 0 || co.or_summand_overflow_count > 0 ||
         co.color_class_overflow_count > 0) &&
        g_color_degrade_warned_epoch != g_state.committed_epoch) {
      g_color_degrade_warned_epoch = g_state.committed_epoch;
      std::string msg =
          "Raypath coloring is degraded on the GPU backend — this configuration exceeds a device "
          "capacity and some color assignment was dropped:\n";
      if (co.symmetry_group_overflow_count > 0) {
        msg += "  - " + std::to_string(co.symmetry_group_overflow_count) +
               " symmetry group(s) dropped (too many distinct P/B/D variants on one crystal slot).\n";
      }
      if (co.or_summand_overflow_count > 0) {
        msg += "  - " + std::to_string(co.or_summand_overflow_count) +
               " OR-summand(s) dropped (too many predicates in one color group).\n";
      }
      if (co.color_class_overflow_count > 0) {
        msg += "  - " + std::to_string(co.color_class_overflow_count) +
               " color class(es) dropped (more than the device color-class limit).\n";
      }
      msg +=
          "Affected rays render with missing or incomplete color. The filtering / geometry / raypath tracing is "
          "UNAFFECTED — only color assignment degrades. Simplify the color configuration to fix.";
      SetGuiWarning(msg);
    }
  }

  // Poll the "GPU backend stopped partway through this run" flag. Without
  // this the fallback surfaces only as a core WARN in the log, while the user watches a
  // run that suddenly got slow and, for the first seconds, strongly miscoloured (the
  // batches queued at GPU grain carry one wavelength each on the CPU path). Rising edge
  // only — see BackendFallbackWarningEdge. Placed with the color-degrade poll above,
  // before the snapshot early-return, because the flag comes from the C API rather than
  // the poller snapshot.
  {
    int fell_back = 0;
    if (LUMICE_GetBackendFallbackFlag(g_server, &fell_back) == LUMICE_OK &&
        BackendFallbackWarningEdge(fell_back != 0, g_backend_fallback_warned)) {
      SetGuiWarning(
          "GPU acceleration stopped working partway through this run — tracing has fallen back to the CPU. "
          "Rendering is slower from here on, and the frame may look unusually saturated for the first seconds "
          "while the CPU path works through batches that were already queued at the GPU's size.\n"
          "The geometry and the physics are UNAFFECTED; only speed and the early preview are. Stop and Run "
          "again to retry the GPU backend.");
    }
  }

  if (!snap || !snap->valid) {
    return;  // No observation yet (or a valid=false restart reset). State already reconciled.
  }

  // Stats: apply only when the STATS THEMSELVES were produced under the committed generation (I1),
  // so a stale carry-forward can't backfill the status bar with a prior run's counts. Keyed on the
  // stats' own stamp rather than the bundle epoch — see ShouldApplyStats in app.hpp for why the
  // bundle epoch cannot answer this.
  if (ShouldApplyStats(*snap, g_state.committed_epoch)) {
    g_state.stats_ray_seg_num = snap->stats_ray_seg_num;
    g_state.stats_sim_ray_num = snap->stats_sim_ray_num;
    g_state.stats_crystal_num = snap->stats_crystal_num;
    g_state.stats_orientation_num = snap->stats_orientation_num;
  }

  // Upload XYZ float texture (GL call — must be on main thread). Gate is the pure
  // ShouldFireCompositeUpload predicate (epoch-keyed serial dedup OR mode-change OR-branch — see
  // app.hpp comment). Single source of truth shared with the regression test so the exact production
  // fire-decision is pinned headlessly. AC4 zero-regression is preserved by boolean algebra: no
  // raypath_color ⇒ is_composite==false ⇒ effective_composite==false ⇒ mode_changed==false ⇒ path
  // is bit-identical to pre-345.4.
  const auto& payload = snap->payload;
  // The channel-B-R diagnostic reads B - R off the NORMAL picture; a composite's hues are a
  // per-class palette the user chose, whose B - R would measure the palette, not the light. So while
  // the diagnostic is on screen the preference is read as "full spectrum" — the preference itself is
  // untouched and applies again the moment the mode goes back to Normal. Done here, not by the
  // server, because the GUI never commits display_mode (it is display-time, and the kSimCommit arm
  // of BuildScene leaves it at normal), so the server keeps producing the composite regardless. The
  // CLI's twin is DoSnapshot's composite skip. Fed into BOTH predicates below, which is what turns a
  // mode flip into an immediate re-upload through their mode-change OR-branch.
  const bool show_composite = EffectiveShowCompositePreview();
  const bool effective_composite =
      payload != nullptr && ShouldUseCompositeUpload(payload->is_composite, show_composite);

  // Upload failure latch (814): while a failure is latched for the CURRENT generation, the
  // upload branch stays silent — a constrained machine must not re-attempt a giant allocation at
  // the poll rate, and a newly minted serial is not a new authorization. A new Run mints
  // committed_epoch+1, which is what un-latches (the recorded epoch no longer matches); a
  // document switch clears the latch explicitly in ResetFrontendState.
  const bool upload_latched =
      ShouldBlockUploadForLatch(g_state.upload_latched, g_state.upload_latch_epoch, g_state.committed_epoch);
  // While latched, the fire gate below never runs — but the retained RETRY source must still
  // track the newest qualifying frame (plan 814 §3.2: explicit retry uses "the retained newest
  // qualifying source", which for a run that kept producing means the terminal frame, not the
  // first mid-run failure). Pure bookkeeping: hold the newer payload, no GL call, no state
  // commit.
  if (upload_latched && payload != nullptr && snap->texture_serial != g_pending_upload_serial &&
      snap->texture_serial != g_state.last_uploaded_texture_serial) {
    g_pending_upload_source = payload;
    g_pending_upload_serial = snap->texture_serial;
  }
  if (!upload_latched &&
      ShouldFireCompositeUpload(*snap, g_state.last_uploaded_texture_serial, g_state.display_epoch_floor,
                                show_composite, g_state.last_uploaded_as_composite)) {
    GUI_LOG_VERBOSE("[GUI] SyncFromPoller: upload tex_rays={}, intensity={:.6f}, eff_pixels={}, factor={:.6f}",
                    payload->texture_ray_count, payload->snapshot_intensity, payload->effective_pixels,
                    payload->intensity_factor);
    // task-342.4 Step 3 + task-345.4: choose sRGB uint8 vs XYZ float upload path. Server produces
    // composite iff raypath_color is active; user can toggle back to full-spectrum via the status
    // bar (`show_composite_preview`). Auto-EV / stats / serial-dedup still consume xyz-derived
    // fields below — those are populated in both branches (plan §3 keypoint 3).
    // Both buffers are views into the result frame the payload holds, so `payload` staying alive
    // for this whole block is what keeps them readable. The XYZ path goes through the recovery
    // ladder (plan 814 §3.2): at the established tier directly — one attempt per frame, no
    // re-probing of bigger candidates — or, at tier 0, through one full walk from the normal PBO
    // path down to the prefiltered floor. The composite (uint8) path keeps its plain upload: its
    // colour semantics are baked display-side and the ladder's linear-radiance low-pass does not
    // apply to it; a composite failure reports through the same incident record and latches the
    // same way.
    UploadResult upload_result;
    if (effective_composite) {
      upload_result = g_preview.UploadRadianceTexture(payload->rgb_buffer, payload->width, payload->height);
      if (!upload_result.ok) {
        RecordUploadIncident(upload_result, /*tier_tried=*/0, snap->texture_serial);
        // The composite has no ladder (its uint8 bake has no linear low-pass), so this one
        // failure IS the exhausted cycle: the retained source + the exhausted flag open the
        // notice and the Retry exit (whose composite branch re-uploads this same payload).
        g_recovery.exhausted = true;
      }
    } else {
      // One authorized XYZ cycle (see RunXyzUploadCycle): at the established tier only when one
      // is set — one attempt per frame, never re-probing bigger candidates at the poll rate —
      // otherwise one full walk from candidate 0. The cycle's REAL first failure is what the
      // failure block below logs and what the incident record carries; nothing is synthesized.
      const XyzUploadCycle cycle =
          RunXyzUploadCycle(payload->xyz_buffer, payload->width, payload->height, snap->texture_serial,
                            /*probe_full_ladder=*/g_recovery.xyz_tier <= 0);
      if (cycle.displayed_tier >= 0) {
        upload_result.ok = true;
        if (g_recovery.xyz_tier <= 0 && cycle.displayed_tier > 0) {
          g_recovery.xyz_tier = cycle.displayed_tier;
          GUI_LOG_INFO("[GUI] SyncFromPoller: recovery ladder established tier {} ({}x{})", cycle.displayed_tier,
                       payload->width >> (cycle.displayed_tier - 1), payload->height >> (cycle.displayed_tier - 1));
        }
      } else {
        // Every attempted candidate failed: the cycle is exhausted, the latch below pauses
        // auto-uploads, and the panel notice says the preview is unavailable until an explicit
        // retry or a new run.
        g_recovery.exhausted = true;
        upload_result = cycle.first_failure;
        upload_result.ok = false;
      }
    }
    if (!upload_result.ok) {
      // The upload failed: nothing about the DISPLAY may advance. The payload retention, the
      // serial cursor, the exposure state and the counters below all describe the frame that was
      // ACCEPTED — committing them here would repeat the pre-814 defect where a logged-only
      // failure still recorded a frame as shown. The failed frame's own source is retained as the
      // retry ladder's input (explicit retry consumes it; Save falls back to it with clear
      // provenance), and the latch pauses auto-uploads for this generation.
      g_pending_upload_source = payload;
      g_pending_upload_serial = snap->texture_serial;
      g_state.upload_latched = true;
      g_state.upload_latch_epoch = g_state.committed_epoch;
      GUI_LOG_ERROR(
          "[GUI] SyncFromPoller: texture upload failed (stage={} gl_error=0x{:04X} request={}x{}) — "
          "display state not advanced; auto-upload paused for this generation (serial {})",
          UploadStageName(upload_result.stage), upload_result.gl_error, upload_result.request_width,
          upload_result.request_height, snap->texture_serial);
    } else {
      // Retained for Save: the picture now on screen is this payload's, and the file must hold
      // exactly it. See g_last_uploaded_payload. A failed frame held for retry is superseded: the
      // displayed payload is the authoritative source now.
      g_pending_upload_source.reset();
      g_last_uploaded_payload = payload;
      g_state.last_uploaded_as_composite = effective_composite;
      g_state.snapshot_intensity = payload->snapshot_intensity;
      g_state.snapshot_emitted_energy = payload->emitted_energy;
      g_state.effective_pixels = payload->effective_pixels;
      g_state.texture_upload_count++;
      g_state.last_uploaded_texture_serial = snap->texture_serial;  // record exact-once cursor
      g_state.upload_latched = false;
      // And the recovery state: an ACCEPTED frame is the situation the exhausted flag describes
      // being over. A DoContinue re-commit (new epoch) can un-latch an exhausted generation and
      // its very next upload succeeds — without this clear, the panel would keep reporting the
      // preview unavailable under a full-size picture that just got accepted.
      g_recovery.exhausted = false;
      // Auto-EV. The anchor is the SERVER's sky radiance (LUMICE_RawXyzResult::anchor_l99_sky),
      // the same scalar RenderConsumer::ExposureScale divides by, so the preview and a CLI render
      // of this document are exposed against one number rather than two that were argued to agree
      // (see doc/rendering/adaptive-brightness.md and doc/rendering/ev-pipeline-architecture.md).
      //
      // Only the FIRST argument changed. snapshot_intensity stays the real value, because it is
      // not a leftover: ComputeEvAuto puts it in the numerator, ComputeMonoExposure's kRelative
      // branch divides the result by it again, and the two cancel exactly — leaving
      // intensity_scale = 2^exposure_offset * target_linear / anchor_l99_sky, which is the CLI's
      // expression divided by Omega_axis, which is what the shader's relative-illumination factor
      // supplies per pixel. Passing 1 here instead would break that cancellation and leave a
      // 1/snapshot_intensity gain on the preview.
      //
      // The anchor is a RADIANCE and this texture holds a radiance integrated over a texel, so it
      // is converted into the texture's own units before it can anchor anything: one multiply by
      // the source renderer's on-axis solid angle, which the server publishes for exactly this.
      // The CLI divides by ITS view's solid angle at the same point (RenderConsumer::ExposureScale)
      // — same bridge, opposite direction, because one holds a texture and the other a render.
      // Omitting it is worth 15 stops on the all-sky texture, and ComputeEvAuto's +/-6 clamp turns
      // that into a picture that merely looks a few stops dark rather than into an obvious black.
      //
      // The composite path keeps its own anchor and its own field (payload->p99_y); this branch is
      // the mono one, and the two do not converge — see server_poller.cpp. That field is already a
      // P99 over the composite lanes of this same buffer, so it needs no conversion.
      g_state.p99_raw_y = payload->is_composite ? payload->p99_y : payload->axis_solid_angle * payload->anchor_l99_sky;
      g_state.ev_auto = LUMICE_ComputeEvAuto(g_state.p99_raw_y, g_state.snapshot_intensity, g_state.target_white);
      GUI_LOG_VERBOSE("[GUI] SyncFromPoller: anchor={:.6f} (composite={}), ev_auto={:.3f}", g_state.p99_raw_y,
                      payload->is_composite, g_state.ev_auto);
    }
  }
}

// ---- Preview recovery surface (see app.hpp) ----

int PreviewRecoveryNoticeKind() {
  if (g_recovery.notice_dismissed) {
    return 0;
  }
  if (g_recovery.exhausted) {
    return 2;
  }
  return g_recovery.xyz_tier > 0 ? 1 : 0;
}

const char* PreviewRecoveryNoticeText() {
  switch (PreviewRecoveryNoticeKind()) {
    case 1:
      return "Preview recovered at a reduced resolution (GPU memory pressure). The saved document keeps the "
             "full-resolution source.";
    case 2:
      return "Preview unavailable: the GPU could not fit this frame at any recovery size. Retry re-attempts from "
             "full resolution; the retained source still saves.";
    default:
      return "";
  }
}

void DismissPreviewRecoveryNotice() {
  g_recovery.notice_dismissed = true;
}

bool PreviewRecoveryRetryAvailable() {
  // A retry target exists when an exhausted cycle left a retained frame (the newest one that
  // never displayed), OR the document holds an accepted source — the displayed live payload or
  // its own file bytes — while the display is degraded-but-working (tier > 0, the re-probe) or
  // exhausted (the re-walk). The file source closing both shapes is the r4 fix: a degraded or
  // failed OPEN used to leave no retry exit at all, because availability was phrased in live
  // payloads and an open never creates one.
  if (g_pending_upload_source != nullptr) {
    return true;
  }
  if (!g_file_source.valid && g_last_uploaded_payload == nullptr) {
    return false;
  }
  return g_recovery.exhausted || g_recovery.xyz_tier > 0;
}

void RetryPreviewUpload() {
  // The retry target, in priority order: the retained newest-qualifying frame of a failed cycle
  // (the exhausted path — for a run that kept producing this is the terminal frame), or the
  // ACCEPTED source resolved by the same adjudication Save and the extent queries use — the
  // frame a degraded tier is displaying smaller than it is, whose re-walk from candidate 0 is
  // how the user probes for restored full resolution without a new run. Availability
  // (PreviewRecoveryRetryAvailable) and this resolution are ONE contract, so the panel cannot
  // offer a retry the target resolver cannot name and vice versa. A file target re-uploads in
  // its own mode (r4: the file source could not re-attempt at all); its pixels come from the
  // CPU mirror, where the open wrote them.
  std::shared_ptr<const TexturePayload> target = g_pending_upload_source;
  unsigned long long target_serial = g_pending_upload_serial;
  bool target_is_file = false;
  bool from_accepted_live = false;
  if (target == nullptr) {
    if (!(g_recovery.exhausted || g_recovery.xyz_tier > 0)) {
      // Nothing retained, nothing degraded or exhausted: a new Run is the way back, and
      // pretending otherwise would fabricate an attempt against a frame nobody holds.
      GUI_LOG_INFO("[GUI] Preview retry: no retained source to re-attempt; run the simulation again");
      return;
    }
    const ResolvedPreviewSource src = ResolvePreviewSource();
    if (src.role == ResolvedPreviewSource::Role::kAcceptedLive) {
      target = src.payload;
      target_serial = src.serial;
      from_accepted_live = true;
    } else if (src.role == ResolvedPreviewSource::Role::kFileMirror) {
      target_is_file = true;
    } else {
      GUI_LOG_INFO("[GUI] Preview retry: no retained source to re-attempt; run the simulation again");
      return;
    }
  }

  // Re-authorize ONE cycle, from candidate 0: the machine's pressure may have eased, and the
  // plan's contract is that an explicit retry walks the ladder afresh against the retained
  // source — the newest qualifying frame, which a self-paused terminal poll keeps readable.
  g_state.upload_latched = false;
  g_recovery.exhausted = false;
  g_recovery.notice_dismissed = false;
  // The target's display mode, adjudicated ONCE (a review round caught the fork): the upload
  // selection below AND
  // both success-commit branches consume this one verdict. Re-reading the raw preference at
  // commit time re-forked the mode under the channel-B-R diagnostic — the retry uploads XYZ
  // while recording the display as composite, and the next sync's mode-change branch then
  // re-uploads the very frame the retry just accepted.
  const bool use_composite = target_is_file ?
                                 g_file_source.mode == PreviewRenderer::TextureMode::kSrgbComposited :
                                 ShouldUseCompositeUpload(target->is_composite, EffectiveShowCompositePreview());
  UploadResult upload_result;
  int tier = -1;
  if (target_is_file) {
    switch (g_file_source.mode) {
      case PreviewRenderer::TextureMode::kXyz: {
        const XyzUploadCycle cycle = RunXyzUploadCycle(g_preview.GetXyzTextureData(), g_file_source.width,
                                                       g_file_source.height, /*texture_serial=*/0,
                                                       /*probe_full_ladder=*/true);
        tier = cycle.displayed_tier;
        upload_result = cycle.first_failure;
        upload_result.ok = tier >= 0;
        break;
      }
      case PreviewRenderer::TextureMode::kSrgbRadiance:
        upload_result =
            g_preview.UploadRadianceTexture(g_preview.GetTextureData(), g_file_source.width, g_file_source.height);
        if (!upload_result.ok) {
          RecordUploadIncident(upload_result, /*tier_tried=*/0, /*texture_serial=*/0);
        }
        break;
      case PreviewRenderer::TextureMode::kSrgbComposited:
        upload_result = g_preview.UploadTexture(g_preview.GetTextureData(), g_file_source.width, g_file_source.height);
        if (!upload_result.ok) {
          RecordUploadIncident(upload_result, /*tier_tried=*/0, /*texture_serial=*/0);
        }
        break;
    }
  } else {
    const TexturePayload& payload = *target;
    if (use_composite) {
      // A composite keeps its own display semantics: the composite bake is uint8 and the
      // ladder's linear-radiance low-pass does not apply to it — one plain re-upload, success or
      // the same latch as any other failure.
      upload_result = g_preview.UploadRadianceTexture(payload.rgb_buffer, payload.width, payload.height);
      if (!upload_result.ok) {
        RecordUploadIncident(upload_result, /*tier_tried=*/0, target_serial);
      }
    } else if (payload.xyz_buffer != nullptr) {
      // The cycle records its own first failure — the retry block below must NOT record again,
      // or it would overwrite the concrete stage/GL error/request size with a placeholder (the
      // r4 defect that turned a real texture OOM into a fabricated invalid-input report). The
      // cycle's real result is what the failure log carries, same as SyncFromPoller's.
      const XyzUploadCycle cycle = RunXyzUploadCycle(payload.xyz_buffer, payload.width, payload.height, target_serial,
                                                     /*probe_full_ladder=*/true);
      tier = cycle.displayed_tier;
      upload_result = cycle.first_failure;
      upload_result.ok = tier >= 0;
    } else {
      // A source with no usable pixels (the acceptance-leg retained copy can carry none in
      // shapes the ladder cannot walk): treat as an exhausted retry, recorded for what it is.
      upload_result.ok = false;
      upload_result.stage = UploadResult::Stage::kInvalidInput;
      upload_result.request_width = payload.width;
      upload_result.request_height = payload.height;
      RecordUploadIncident(upload_result, /*tier_tried=*/0, target_serial);
    }
  }
  if (!upload_result.ok) {
    // The cycle ran out again: re-latch against the CURRENT generation, exactly as SyncFromPoller
    // does — without this, the next ordinary poll would silently self-authorize yet another
    // cycle, and the latch's one-cycle-per-authorization contract would be dead code.
    g_recovery.exhausted = true;
    g_state.upload_latched = true;
    g_state.upload_latch_epoch = g_state.committed_epoch;
    GUI_LOG_ERROR(
        "[GUI] Preview retry: every recovery candidate failed again (stage={} gl_error=0x{:04X} "
        "request={}x{}) — preview stays unavailable, auto-upload re-latched",
        UploadStageName(upload_result.stage), upload_result.gl_error, upload_result.request_width,
        upload_result.request_height);
    return;
  }
  // Success. The display commit mirrors SyncFromPoller's exactly: the accepted payload pointer,
  // the exact-once cursor, the display-mode record, the exposure state and the counters all
  // describe the picture now on screen. For the degraded-reprobe shape the target WAS the
  // accepted payload — the commit re-asserts it at the tier the walk re-established.
  if (target_is_file) {
    // A file has no poller frame behind it: no payload moves, and the exact-once cursor stays
    // untouched (writing 0 there would un-dedup the poller's first frame). The exposure state,
    // though, is committed from the file's own facts for XYZ — the same numbers the open
    // committed on its successful upload, so the document lights the same after a recovery as
    // it would have without one. The baked uint8 modes carry their exposure in the bytes; only
    // the display facts change for them.
    if (g_file_source.mode == PreviewRenderer::TextureMode::kXyz) {
      g_state.snapshot_intensity = g_file_source.snapshot_intensity;
      g_state.snapshot_emitted_energy = g_file_source.emitted_energy;
      g_state.effective_pixels = g_file_source.effective_pixels;
      g_state.p99_raw_y = g_file_source.mono_anchor;
      g_state.ev_auto = LUMICE_ComputeEvAuto(g_state.p99_raw_y, g_state.snapshot_intensity, g_state.target_white);
    }
    g_state.last_uploaded_as_composite = use_composite;
  } else if (from_accepted_live) {
    // The accepted pointer stays the accepted pointer; drop the retained copy if one existed.
    g_pending_upload_source.reset();
    g_pending_upload_serial = 0;
    const TexturePayload& payload = *target;
    g_state.last_uploaded_as_composite = use_composite;
    g_state.snapshot_intensity = payload.snapshot_intensity;
    g_state.snapshot_emitted_energy = payload.emitted_energy;
    g_state.effective_pixels = payload.effective_pixels;
    g_state.p99_raw_y = payload.is_composite ? payload.p99_y : payload.axis_solid_angle * payload.anchor_l99_sky;
    g_state.ev_auto = LUMICE_ComputeEvAuto(g_state.p99_raw_y, g_state.snapshot_intensity, g_state.target_white);
  } else {
    // The retained frame is now the accepted one: move the hold, don't copy it.
    const unsigned long long retried_serial = g_pending_upload_serial;
    g_last_uploaded_payload = g_pending_upload_source;
    g_pending_upload_source.reset();
    g_pending_upload_serial = 0;
    g_state.last_uploaded_texture_serial = retried_serial;
    const TexturePayload& payload = *g_last_uploaded_payload;
    g_state.last_uploaded_as_composite = use_composite;
    g_state.snapshot_intensity = payload.snapshot_intensity;
    g_state.snapshot_emitted_energy = payload.emitted_energy;
    g_state.effective_pixels = payload.effective_pixels;
    g_state.p99_raw_y = payload.is_composite ? payload.p99_y : payload.axis_solid_angle * payload.anchor_l99_sky;
    g_state.ev_auto = LUMICE_ComputeEvAuto(g_state.p99_raw_y, g_state.snapshot_intensity, g_state.target_white);
  }
  g_recovery.xyz_tier = tier >= 0 ? tier : 0;
  g_state.texture_upload_count++;
  g_state.upload_latched = false;
  GUI_LOG_INFO("[GUI] Preview retry: recovery succeeded ({}x{}, {})",
               target_is_file ? g_file_source.width : target->width,
               target_is_file ? g_file_source.height : target->height,
               target_is_file ?
                   (g_file_source.mode == PreviewRenderer::TextureMode::kXyz ? "file xyz ladder" : "file re-upload") :
                   "retained source");
}

PreviewUploadIncident GetPreviewUploadIncident() {
  PreviewUploadIncident out;
  out.has_incident = g_recovery.has_incident;
  out.stage = g_recovery.stage;
  out.gl_error = g_recovery.gl_error;
  out.request_width = g_recovery.request_width;
  out.request_height = g_recovery.request_height;
  out.tier_tried = g_recovery.tier_tried;
  out.texture_serial = g_recovery.texture_serial;
  return out;
}

long PreviewSourcePixelCount() {
  int w = 0;
  int h = 0;
  PreviewSourceExtent(&w, &h);
  return static_cast<long>(w) * h;
}

void PreviewSourceExtent(int* out_w, int* out_h) {
  if (out_w == nullptr || out_h == nullptr) {
    return;
  }
  *out_w = 0;
  *out_h = 0;
  // The ONE source adjudication (see ResolvePreviewSource) — the same answer Save, the exposure
  // queries and the degraded-export route get, in the same priority order. The r4 fork had this
  // query prefer a never-displayed failed frame over the file source it sat on top of, which
  // silently moved the absolute-EV denominator and the degraded-export size off the displayed
  // frame.
  const ResolvedPreviewSource src = ResolvePreviewSource();
  if (src.width > 0 && src.height > 0) {
    *out_w = src.width;
    *out_h = src.height;
    return;
  }
  if (g_preview.HasCpuTexture()) {
    *out_w = g_preview.GetCpuTextureWidth();
    *out_h = g_preview.GetCpuTextureHeight();
    return;
  }
  *out_w = g_preview.GetTextureWidth();
  *out_h = g_preview.GetTextureHeight();
}

void CheckUnsavedAndDo(PendingAction action) {
  if (g_state.dirty) {
    g_pending_action = action;
    g_show_unsaved_popup = true;
  } else {
    switch (action) {
      case PendingAction::kNew:
        DoNew();
        break;
      case PendingAction::kOpen:
        DoOpen();
        break;
      default:
        break;
    }
  }
}

}  // namespace lumice::gui
