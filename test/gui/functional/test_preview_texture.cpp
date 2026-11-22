// The preview's GPU texture and the CPU mirror the document is written from.
//
// What this suite is for. PreviewRenderer holds the rendered sky twice: once in a GL texture, which
// is what the user sees, and once in a CPU-side byte buffer, which is what SaveLmcFile writes into
// the document. Nothing keeps those two in step except the setters that write them, and the
// failure they invite is silent in both directions — the screen keeps showing the right picture
// while the file gets an older one, or the file gets the right bytes under the wrong dimensions.
// Since 814 the suite also owns the upload failure contracts: the three upload entry points
// return an UploadResult, commit display state only on a verified success, and a texture-stage
// OOM conservatively revokes the display instead of leaving it describing storage nothing can
// vouch for. The fault cases drive those branches through the renderer's test hook — the same
// production failure path a real GL error takes, synthesized at one stage boundary at a time.
//
// Why here and not in the windowless target. Every case below has to seed the renderer through
// UploadTexture (or UploadXyzTexture), which is a glTexImage2D call and returns without doing
// anything at all when there is no texture object (preview_renderer.cpp's first guard). The
// dimension case in particular cannot be written any other way: the defect it guards is a CPU
// mirror refresh silently resizing the DISPLAY, and only a real upload can put a display size
// there to begin with.
//
// Deliberately NOT here. Which document fields survive a save/load is
// composition-correctness/gui/test_document_roundtrip_chain.cpp — that layer works on GuiState and
// never touches the texture. What a document switch is required to clear from the preview is
// test/gui/functional/test_file_ops.cpp. Whether the saved frame LOOKS like the live one is
// test/gui/visual/test_preview_pixels.cpp.
//
// What a user sees when these break: they run a simulation, save, reopen, and get the halo from
// two runs ago — or a file that will not open at all because its declared size does not match its
// payload.

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "gui/export_fbo_renderer.hpp"
#include "gui/gui_state.hpp"
#include "gui/preview_downgrade.hpp"
#include "test_gui_shared.hpp"

// Local spellings for the run-driven cases (same shape as test_run_lifecycle.cpp's).
using SimState = gui::GuiState::SimState;
using RunIntent = gui::RunIntent;

namespace {

// GL uploads have to happen on the main thread; a TestFunc runs on the test engine's coroutine.
void UploadOnMainThreadGuiFunc(ImGuiTestContext* /*ctx*/) {
  if (g_capture.capture_requested && !g_capture.capture_done) {
    gui::g_preview.UploadTexture(g_capture.pixels.data(), g_capture.width, g_capture.height);
    g_capture.capture_done = true;
  }
}

// Hand `pixels` to the renderer and return once the upload has landed.
void UploadAndWait(ImGuiTestContext* ctx, const std::vector<unsigned char>& pixels, int w, int h) {
  g_capture.Reset();
  g_capture.pixels = pixels;
  g_capture.width = w;
  g_capture.height = h;
  g_capture.capture_requested = true;
  ctx->Yield(2);
  IM_CHECK(g_capture.capture_done);
}

// The fault-injection workbench: one GuiFunc that services hook installs, hook clears, uint8
// uploads and XYZ uploads on the main thread, each behind a request/ack pair the TestFunc drives
// through Yield()s — the same shape ScreenshotCapture uses, because the same boundary applies
// (TestFunc is the coroutine, GuiFunc holds the GL context). The hook routes through
// PreviewRenderer::SetUploadFaultHookForTest, whose contract is that a forced failure takes the
// SAME production failure branch the real GL error would take.
struct FaultUploadRequest {
  std::atomic<bool> hook_requested{ false };
  std::atomic<int> hook_stage{ 0 };     // UploadResult::Stage value to force-fail
  std::atomic<int> hook_gl_error{ 0 };  // GLenum the hook synthesizes at that stage
  std::atomic<bool> hook_installed{ false };
  std::atomic<bool> hook_clear_requested{ false };
  std::atomic<bool> hook_cleared{ false };
  std::atomic<bool> upload8_requested{ false };
  std::atomic<bool> upload8_done{ false };
  std::atomic<bool> uploadxyz_requested{ false };
  std::atomic<bool> uploadxyz_done{ false };
  std::atomic<bool> uploadnstaging_requested{ false };
  std::atomic<bool> uploadnstaging_done{ false };
  std::atomic<bool> uploadhalf_requested{ false };
  std::atomic<bool> uploadhalf_done{ false };
  std::atomic<bool> uploadhalfnstaging_requested{ false };
  std::atomic<bool> uploadhalfnstaging_done{ false };
  std::atomic<bool> strip_rows_requested{ false };
  std::atomic<bool> strip_rows_done{ false };
  int strip_rows_value = 0;
  gui::UploadResult uploadhalf_result;
  gui::UploadResult uploadhalfnstaging_result;
  const std::vector<uint16_t>* half = nullptr;
  float half_scale = 1.0f;
  std::atomic<bool> bg_requested{ false };
  std::atomic<bool> bg_done{ false };
  std::atomic<bool> custom_hook_requested{ false };
  std::atomic<bool> custom_hook_installed{ false };
  std::atomic<bool> allhook_requested{ false };
  std::atomic<bool> allhook_installed{ false };
  std::atomic<bool> allhook_clear_requested{ false };
  std::atomic<bool> allhook_cleared{ false };
  std::atomic<bool> init_probe_requested{ false };
  std::atomic<bool> init_probe_done{ false };
  bool init_probe_result = false;
  bool init_probe_upload_ok = false;
  bool init_probe_has_texture = false;
  std::atomic<bool> retry_requested{ false };
  std::atomic<bool> retry_done{ false };
  // Captured INSIDE the retry request, on the main thread, before the next SyncFromPoller runs:
  // what the retry committed is observable here without a normal sync being able to re-fire the
  // mode-change branch and correct the record first.
  std::atomic<int> retry_snap_tex_mode{ -1 };
  std::atomic<int> retry_snap_as_composite{ -1 };
  std::atomic<unsigned long long> retry_snap_upload_count{ 0 };
  std::atomic<int> retry_snap_show_composite{ -1 };
  std::atomic<int> retry_snap_display_mode{ -1 };
  std::atomic<bool> dfe_requested{ false };  // dual-fisheye equal-area export (no-dialog seam)
  std::atomic<bool> dfe_done{ false };
  bool dfe_result = false;
  std::string dfe_path;
  // Captured INSIDE the export request, on the main thread, before the next SyncFromPoller can
  // run: what the target-OOM rebuild committed is observable here without a normal sync being
  // able to re-fire the mode-change branch first (same shape as retry_snap_*).
  std::atomic<int> dfe_snap_tex_mode{ -1 };
  std::atomic<int> dfe_snap_as_composite{ -1 };
  std::atomic<int> dfe_snap_notice_kind{ -1 };
  std::atomic<unsigned long long> dfe_snap_serial{ 0 };
  std::atomic<unsigned long long> dfe_snap_upload_count{ 0 };
  std::atomic<bool> equi_requested{ false };  // equirectangular export (no-dialog seam)
  std::atomic<bool> equi_done{ false };
  bool equi_result = false;
  std::string equi_path;
  std::atomic<bool> shot_requested{ false };  // RenderScreenshot, no dialog and no file
  std::atomic<bool> shot_done{ false };
  gui::ScreenshotRender shot_result;
  std::atomic<bool> storage_fault_requested{ false };  // RGBA8 target-allocation fault (main thread)
  std::atomic<bool> storage_fault_done{ false };
  int storage_fault_count = 0;
  gui::UploadResult upload8_result;
  gui::UploadResult uploadxyz_result;
  const std::vector<unsigned char>* srgb = nullptr;
  const std::vector<float>* xyz = nullptr;
  std::vector<unsigned char> bg_pixels;
  int bg_width = 0;
  int bg_height = 0;
  int width = 0;
  int height = 0;

  void Reset() {
    hook_requested = false;
    hook_stage = 0;
    hook_gl_error = 0;
    hook_installed = false;
    hook_clear_requested = false;
    hook_cleared = false;
    upload8_requested = false;
    upload8_done = false;
    uploadxyz_requested = false;
    uploadxyz_done = false;
    uploadnstaging_requested = false;
    uploadnstaging_done = false;
    uploadhalf_requested = false;
    uploadhalf_done = false;
    uploadhalfnstaging_requested = false;
    uploadhalfnstaging_done = false;
    strip_rows_requested = false;
    strip_rows_done = false;
    strip_rows_value = 0;
    bg_requested = false;
    bg_done = false;
    custom_hook_requested = false;
    custom_hook_installed = false;
    allhook_requested = false;
    allhook_installed = false;
    allhook_clear_requested = false;
    allhook_cleared = false;
    init_probe_requested = false;
    init_probe_done = false;
    init_probe_result = false;
    init_probe_upload_ok = false;
    init_probe_has_texture = false;
    retry_requested = false;
    retry_done = false;
    retry_snap_tex_mode = -1;
    retry_snap_as_composite = -1;
    retry_snap_upload_count = 0;
    retry_snap_show_composite = -1;
    retry_snap_display_mode = -1;
    dfe_requested = false;
    dfe_done = false;
    dfe_result = false;
    dfe_path.clear();
    dfe_snap_tex_mode = -1;
    dfe_snap_as_composite = -1;
    dfe_snap_notice_kind = -1;
    dfe_snap_serial = 0;
    dfe_snap_upload_count = 0;
    equi_requested = false;
    equi_done = false;
    equi_result = false;
    equi_path.clear();
    shot_requested = false;
    shot_done = false;
    shot_result = gui::ScreenshotRender{};
    storage_fault_requested = false;
    storage_fault_done = false;
    storage_fault_count = 0;
    srgb = nullptr;
    xyz = nullptr;
    bg_pixels.clear();
    bg_width = 0;
    bg_height = 0;
    width = 0;
    height = 0;
  }
};

FaultUploadRequest g_fault;

// The custom hook's factory slot: TestFunc fills it BEFORE raising custom_hook_requested, the
// GuiFunc (main thread) installs it. The function object itself EXECUTES on the main thread like
// every hook — side effects it makes on GL or on plain captured counters are main-thread facts;
// counters a TestFunc later reads are atomics for that reason only.
std::function<int(gui::UploadResult::Stage)> g_custom_hook;

// DoOpen request channel, same shape as test_file_ops' baked-preview open: the open's GL upload
// branch must run on the render thread, so the TestFunc posts a path and the GuiFunc executes it.
bool g_open_requested = false;
bool g_open_done = false;
std::string g_open_path;

// Raw GLenum spellings for the synthesized errors. Kept numeric so this TU does not need the GL
// headers: GL_OUT_OF_MEMORY (0x0505), GL_INVALID_VALUE (0x0500). GL_WAIT_FAILED (0x911D) is a
// glClientWaitSync RETURN STATUS, not a driver error flag — the fence-wait witness leans on
// exactly that difference.
constexpr int kGlOutOfMemory = 0x0505;
constexpr int kGlInvalidValue = 0x0500;
constexpr int kGlWaitFailed = 0x911D;

void FaultUploadGuiFunc(ImGuiTestContext* /*ctx*/) {
  if (g_fault.hook_requested && !g_fault.hook_installed) {
    const auto stage = static_cast<gui::UploadResult::Stage>(g_fault.hook_stage.load());
    const int err = g_fault.hook_gl_error.load();
    gui::g_preview.SetUploadFaultHookForTest(
        [stage, err](gui::UploadResult::Stage s) -> int { return s == stage ? err : 0; });
    g_fault.hook_installed = true;
  }
  if (g_fault.custom_hook_requested && !g_fault.custom_hook_installed) {
    gui::g_preview.SetUploadFaultHookForTest(g_custom_hook);
    g_fault.custom_hook_installed = true;
  }
  if (g_fault.allhook_requested && !g_fault.allhook_installed) {
    gui::PreviewRenderer::SetUploadFaultHookForAllForTest(g_custom_hook);
    g_fault.allhook_installed = true;
  }
  if (g_fault.allhook_clear_requested && !g_fault.allhook_cleared) {
    gui::PreviewRenderer::SetUploadFaultHookForAllForTest(nullptr);
    g_fault.allhook_cleared = true;
  }
  if (g_fault.init_probe_requested && !g_fault.init_probe_done) {
    // A THROWAWAY renderer: Init's partial-failure exits must leave a context the next Init can
    // build a fully working renderer in. Destroy runs on EVERY exit path while the main thread
    // still holds the context — a successful probe that skipped Destroy would leak its
    // programs/VAO/VBO/textures into the suite's shared context, the exact defect the witness
    // exists to reject.
    gui::PreviewRenderer probe;
    g_fault.init_probe_result = probe.Init();
    if (g_fault.init_probe_result) {
      static const float px[3] = { 0.25f, 0.5f, 0.75f };
      g_fault.init_probe_upload_ok = probe.UploadXyzTexture(px, 1, 1).ok;
      g_fault.init_probe_has_texture = probe.HasTexture();
    }
    probe.Destroy();
    g_fault.init_probe_done = true;
  }
  if (g_fault.hook_clear_requested && !g_fault.hook_cleared) {
    gui::g_preview.SetUploadFaultHookForTest(nullptr);
    g_fault.hook_cleared = true;
  }
  if (g_fault.retry_requested && !g_fault.retry_done) {
    gui::RetryPreviewUpload();
    g_fault.retry_snap_tex_mode = static_cast<int>(gui::g_preview.GetTextureMode());
    g_fault.retry_snap_as_composite = gui::g_state.last_uploaded_as_composite ? 1 : 0;
    g_fault.retry_snap_upload_count = gui::g_state.texture_upload_count;
    g_fault.retry_snap_show_composite = gui::g_state.show_composite_preview ? 1 : 0;
    g_fault.retry_snap_display_mode = gui::g_state.renderer.display_mode;
    g_fault.retry_done = true;
  }
  if (g_fault.dfe_requested && !g_fault.dfe_done) {
    g_fault.dfe_result = gui::ExportDualFisheyeEqualAreaPng(g_fault.dfe_path);
    // Captured before the handler returns: the next SyncFromPoller cannot have run yet, so the
    // commit snapshot below is what the export's rebuild left, not a later correction.
    g_fault.dfe_snap_tex_mode = static_cast<int>(gui::g_preview.GetTextureMode());
    g_fault.dfe_snap_as_composite = gui::g_state.last_uploaded_as_composite ? 1 : 0;
    g_fault.dfe_snap_notice_kind = gui::PreviewRecoveryNoticeKind();
    g_fault.dfe_snap_serial = gui::g_state.last_uploaded_texture_serial;
    g_fault.dfe_snap_upload_count = gui::g_state.texture_upload_count;
    g_fault.dfe_done = true;
  }
  if (g_fault.equi_requested && !g_fault.equi_done) {
    g_fault.equi_result = gui::ExportEquirectangularPng(g_fault.equi_path);
    g_fault.equi_done = true;
  }
  if (g_fault.shot_requested && !g_fault.shot_done) {
    g_fault.shot_result = gui::RenderScreenshot(gui::ScreenshotExportSelection{});
    g_fault.shot_done = true;
  }
  if (g_fault.storage_fault_requested && !g_fault.storage_fault_done) {
    gui::SetRgba8StorageFaultForTest(g_fault.storage_fault_count);
    g_fault.storage_fault_done = true;
  }
  if (g_fault.upload8_requested && !g_fault.upload8_done) {
    g_fault.upload8_result = gui::g_preview.UploadTexture(g_fault.srgb->data(), g_fault.width, g_fault.height);
    g_fault.upload8_done = true;
  }
  if (g_fault.uploadxyz_requested && !g_fault.uploadxyz_done) {
    g_fault.uploadxyz_result = gui::g_preview.UploadXyzTexture(g_fault.xyz->data(), g_fault.width, g_fault.height);
    g_fault.uploadxyz_done = true;
  }
  if (g_fault.uploadnstaging_requested && !g_fault.uploadnstaging_done) {
    g_fault.uploadxyz_result =
        gui::g_preview.UploadXyzTextureNoStaging(g_fault.xyz->data(), g_fault.width, g_fault.height);
    g_fault.uploadnstaging_done = true;
  }
  if (g_fault.uploadhalf_requested && !g_fault.uploadhalf_done) {
    g_fault.uploadhalf_result =
        gui::g_preview.UploadXyzHalfTexture(g_fault.half->data(), g_fault.width, g_fault.height, g_fault.half_scale);
    g_fault.uploadhalf_done = true;
  }
  if (g_fault.uploadhalfnstaging_requested && !g_fault.uploadhalfnstaging_done) {
    g_fault.uploadhalfnstaging_result = gui::g_preview.UploadXyzHalfTextureNoStaging(
        g_fault.half->data(), g_fault.width, g_fault.height, g_fault.half_scale);
    g_fault.uploadhalfnstaging_done = true;
  }
  if (g_fault.strip_rows_requested && !g_fault.strip_rows_done) {
    gui::PreviewRenderer::SetXyzStripRowsForTest(g_fault.strip_rows_value);
    g_fault.strip_rows_done = true;
  }
  if (g_fault.bg_requested && !g_fault.bg_done) {
    gui::g_preview.UploadBgTexture(g_fault.bg_pixels.data(), g_fault.bg_width, g_fault.bg_height);
    g_fault.bg_done = true;
  }
  if (g_open_requested && !g_open_done) {
    gui::DoOpen(g_open_path);
    g_open_done = true;
  }
  if (g_capture.capture_requested && !g_capture.capture_done) {
    gui::g_preview.UploadTexture(g_capture.pixels.data(), g_capture.width, g_capture.height);
    g_capture.capture_done = true;
  }
}

// Drive the hook install (or clear) to completion. Every fault test starts here so a hook that a
// previously-aborted case left behind can never leak into this one.
void SetFaultAndWait(ImGuiTestContext* ctx, int stage, int gl_error) {
  g_fault.Reset();
  if (stage < 0) {
    // Negative stage means "clear the hook".
    g_fault.hook_clear_requested = true;
    ctx->Yield(2);
    IM_CHECK(g_fault.hook_cleared);
  } else {
    g_fault.hook_requested = true;
    g_fault.hook_stage = stage;
    g_fault.hook_gl_error = gl_error;
    ctx->Yield(2);
    IM_CHECK(g_fault.hook_installed);
  }
}

// Install a caller-built hook — the shape for witnesses that need more than one stage's
// synthesized return value: per-candidate counters, side effects on real GL state, or
// pass-after-N-fails ladders. The hook body runs on the main thread; anything it mutates that
// the TestFunc later reads is an atomic captured through the shared pointer.
void SetCustomHookAndWait(ImGuiTestContext* ctx, std::function<int(gui::UploadResult::Stage)> hook) {
  g_fault.Reset();
  g_custom_hook = std::move(hook);
  g_fault.custom_hook_requested = true;
  ctx->Yield(2);
  IM_CHECK(g_fault.custom_hook_installed);
}

// Install (or clear) the ALL-instances upload hook through the main-thread channel — the
// degraded-export witness fails the transient full-source renderer this way. Shares the
// g_custom_hook slot with the per-instance custom hook; install one at a time.
void SetAllHookAndWait(ImGuiTestContext* ctx, std::function<int(gui::UploadResult::Stage)> hook) {
  g_fault.Reset();
  g_custom_hook = std::move(hook);
  if (g_custom_hook) {
    g_fault.allhook_requested = true;
    ctx->Yield(2);
    IM_CHECK(g_fault.allhook_installed);
  } else {
    g_fault.allhook_clear_requested = true;
    ctx->Yield(2);
    IM_CHECK(g_fault.allhook_cleared);
  }
}

// The recovery-order capture: every kTexImage hook call records the live-instance count and both
// staging-slot byte counts AT THE MOMENT THE HOOK FIRES (main thread, inside the upload path).
// A pure-observer hook returns 0 (every upload proceeds) and the witness reads the capture after
// the request completes — the done-atomic provides the happens-before, so plain reads are safe.
struct RecoveryOrderCapture {
  static constexpr int kMaxCalls = 16;
  std::atomic<int> calls{ 0 };
  std::array<std::atomic<int>, kMaxCalls> instances{};
  std::array<std::atomic<unsigned long long>, kMaxCalls> slot0{};
  std::array<std::atomic<unsigned long long>, kMaxCalls> slot1{};

  void Reset() {
    calls = 0;
    for (int i = 0; i < kMaxCalls; ++i) {
      instances[i] = -1;  // -1 = not captured (calls is the authority for which indices hold data)
      slot0[i] = 0;
      slot1[i] = 0;
    }
  }
};
RecoveryOrderCapture g_order_capture;

// Pure observer over every kTexImage in the context: records instance count and slot bytes per
// call, never forces anything. (std::array<std::atomic<...>> value-initializes each element.)
std::function<int(gui::UploadResult::Stage)> MakeOrderObserverHook() {
  g_order_capture.Reset();
  return [](gui::UploadResult::Stage s) -> int {
    if (s != gui::UploadResult::Stage::kTexImage) {
      return 0;
    }
    const int idx = g_order_capture.calls.fetch_add(1);
    if (idx < RecoveryOrderCapture::kMaxCalls) {
      g_order_capture.instances[idx] = gui::PreviewRenderer::LiveInstanceCountForTest();
      g_order_capture.slot0[idx] = gui::g_preview.StagingSlotByteCountForTest(0);
      g_order_capture.slot1[idx] = gui::g_preview.StagingSlotByteCountForTest(1);
    }
    return 0;
  };
}

// Observer + fail: the same capture, but every kTexImage is forced to `gl_error` — the
// exhausted-walk witnesses need both the per-candidate capture and a walk that never succeeds.
std::function<int(gui::UploadResult::Stage)> MakeOrderCaptureFailHook(int gl_error) {
  g_order_capture.Reset();
  return [gl_error](gui::UploadResult::Stage s) -> int {
    if (s != gui::UploadResult::Stage::kTexImage) {
      return 0;
    }
    const int idx = g_order_capture.calls.fetch_add(1);
    if (idx < RecoveryOrderCapture::kMaxCalls) {
      g_order_capture.instances[idx] = gui::PreviewRenderer::LiveInstanceCountForTest();
      g_order_capture.slot0[idx] = gui::g_preview.StagingSlotByteCountForTest(0);
      g_order_capture.slot1[idx] = gui::g_preview.StagingSlotByteCountForTest(1);
    }
    return gl_error;
  };
}

// Arm (or clear) the RGBA8 target-allocation fault through the main-thread channel — the same
// boundary every GL-adjacent seam here crosses (the counter lives behind the export renderer's
// FBO creation and is consumed on the render thread).
void SetStorageFaultAndWait(ImGuiTestContext* ctx, int remaining_failures) {
  g_fault.Reset();
  if (remaining_failures > 0) {
    g_fault.storage_fault_count = remaining_failures;
    g_fault.storage_fault_requested = true;
    ctx->Yield(2);
    IM_CHECK(g_fault.storage_fault_done);
  }
}

// Drive the no-dialog equirectangular export body on the main thread (same shape as
// ExportDualFisheyeAndWait).
bool ExportEquirectAndWait(ImGuiTestContext* ctx, const std::string& path) {
  g_fault.equi_path = path;
  g_fault.equi_result = false;
  g_fault.equi_requested = true;
  g_fault.equi_done = false;
  for (int i = 0; i < 6 && !g_fault.equi_done.load(); ++i) {
    ctx->Yield(1);
  }
  return g_fault.equi_done && g_fault.equi_result;
}

// Drive RenderScreenshot on the main thread and hand back what it produced. Bounded yield loop,
// not IM_CHECK — this helper returns a value and the check macro's failure path is a bare
// `return;` (same shape as ExportDualFisheyeAndWait).
gui::ScreenshotRender ScreenshotAndWait(ImGuiTestContext* ctx) {
  g_fault.shot_requested = true;
  g_fault.shot_done = false;
  for (int i = 0; i < 6 && !g_fault.shot_done.load(); ++i) {
    ctx->Yield(1);
  }
  if (!g_fault.shot_done) {
    return gui::ScreenshotRender{};
  }
  return g_fault.shot_result;
}

// Run Init on a THROWAWAY renderer on the main thread and report what happened (Init result and,
// when it succeeded, whether the renderer then takes a verified upload).
struct InitProbeResult {
  bool ran = false;
  bool init_ok = false;
  bool upload_ok = false;
  bool has_texture = false;
};
InitProbeResult RunInitProbe(ImGuiTestContext* ctx) {
  g_fault.init_probe_requested = true;
  g_fault.init_probe_done = false;
  ctx->Yield(2);
  InitProbeResult out;
  out.ran = g_fault.init_probe_done;
  out.init_ok = g_fault.init_probe_result;
  out.upload_ok = g_fault.init_probe_upload_ok;
  out.has_texture = g_fault.init_probe_has_texture;
  return out;
}

// Drive one explicit retry through the production entry on the main thread (the ladder and the
// uploads it performs are GL work; the TestFunc coroutine must not touch them).
void RetryAndWait(ImGuiTestContext* ctx) {
  g_fault.retry_requested = true;
  g_fault.retry_done = false;
  ctx->Yield(2);
  IM_CHECK(g_fault.retry_done);
}

// Drive the no-dialog dual-fisheye export body on the main thread and report whether it wrote
// the file. Waits by bounded yield loop rather than IM_CHECK: this helper returns a value, and
// the check macro's failure path is a bare `return;`.
bool ExportDualFisheyeAndWait(ImGuiTestContext* ctx, const std::string& path) {
  g_fault.dfe_path = path;
  g_fault.dfe_result = false;
  g_fault.dfe_requested = true;
  g_fault.dfe_done = false;
  for (int i = 0; i < 6 && !g_fault.dfe_done.load(); ++i) {
    ctx->Yield(1);
  }
  return g_fault.dfe_done && g_fault.dfe_result;
}

// Write an XYZ-baked .lmc whose texture section carries `xyz` at `w`x`h` and whose metadata
// carries `meta`. The document state written alongside is whatever the current one holds — the
// readers of this file only assert on the texture section and its meta.
bool WriteXyzLmcFile(const std::string& path, const std::vector<float>& xyz, int w, int h,
                     const gui::XyzTextureMeta& meta) {
  gui::g_preview.UpdateCpuXyzTextureData(xyz.data(), w, h, meta);
  const bool ok = gui::SaveLmcFile(path, gui::g_state, gui::g_preview, /*save_texture=*/true);
  return ok;
}

// The whole live viewport capture as one byte buffer (RGBA, top-down), for cases that compare
// WHOLE frames rather than one centre pixel (the degraded-tier τ=0 round trip).
std::vector<unsigned char> CapturePreviewFullFrame(ImGuiTestContext* ctx) {
  const int vp_w = gui::g_preview_vp.vp_w;
  const int vp_h = gui::g_preview_vp.vp_h;
  if (!gui::g_preview_vp.active || vp_w <= 0 || vp_h <= 0) {
    return {};
  }
  g_fullframe_capture.Reset();
  g_fullframe_capture.rect_x = gui::g_preview_vp.vp_x;
  g_fullframe_capture.rect_y = gui::g_preview_vp.vp_y;
  g_fullframe_capture.rect_w = vp_w;
  g_fullframe_capture.rect_h = vp_h;
  g_fullframe_capture.requested.store(true);
  for (int i = 0; i < 4 && !g_fullframe_capture.done.load(); ++i) {
    ctx->Yield(1);
  }
  if (!g_fullframe_capture.done.load()) {
    return {};
  }
  return g_fullframe_capture.pixels;
}

// A hook that fails the FIRST `fail_count` calls of `stage` (with `gl_error`) and lets every
// later call through — the shape that drives a walk to a specific candidate: the first two
// kTexImage failures kill the two full-size tiers and the walk establishes the first prefiltered
// one. The counter is main-thread-only (the hook runs there); the atomic is for the TestFunc.
std::function<int(gui::UploadResult::Stage)> FailFirstNThenPass(gui::UploadResult::Stage stage, int fail_count,
                                                                int gl_error) {
  auto counter = std::make_shared<std::atomic<int>>(0);
  return [stage, fail_count, gl_error, counter](gui::UploadResult::Stage s) -> int {
    if (s != stage) {
      return 0;
    }
    return counter->fetch_add(1) < fail_count ? gl_error : 0;
  };
}

// Fail ONLY the Nth consult of `stage` (1-based) and let every other call through. The strip
// core consults per-strip stages once per strip, so "the Nth consult" names one strip of one
// attempt — the pin the mid-attempt failure witnesses need (a failure at strip 2 of 4 must see
// strips 0 and 1 already accepted, which fail-first-N would prevent by killing the attempt at
// strip 0). The counter is main-thread-only (the hook runs there); the atomic is for the TestFunc.
std::function<int(gui::UploadResult::Stage)> FailOnlyNthConsult(gui::UploadResult::Stage stage, int n, int gl_error) {
  auto counter = std::make_shared<std::atomic<int>>(0);
  return [stage, n, gl_error, counter](gui::UploadResult::Stage s) -> int {
    if (s != stage) {
      return 0;
    }
    return counter->fetch_add(1) + 1 == n ? gl_error : 0;
  };
}

// The smallest scene that still produces a real frame, at a chosen simulation resolution index
// (0 = 512x256 all-sky) and a finite budget, for the cases that must drive SyncFromPoller
// through a real run. `millions` is dyadic so the ray count is exact.
void SeedFiniteRun(float millions) {
  gui::g_state.sim.infinite = false;
  gui::g_state.sim.ray_num_millions = millions;
  gui::g_state.sim.max_hits = 8;
  gui::g_state.renderer.sim_resolution_index = 0;
}

// The infinite shape: frames keep arriving on real time, which is what makes the
// newest-retained-frame witnesses machine-independent (a finite 0.1M run completes in a single
// frame on a fast GPU, leaving no newer frame for the second save to pick up).
void SeedInfiniteRun() {
  gui::g_state.sim.infinite = true;
  gui::g_state.sim.max_hits = 8;
  gui::g_state.renderer.sim_resolution_index = 0;
}

// Pump frames until `pred` holds or the wall-clock budget runs out (same contract as
// test_run_lifecycle.cpp's DriveUntil: --fixed-dt decouples frames from seconds, so only the
// clock bounds a hang).
template <typename Pred>
bool DriveUntil(ImGuiTestContext* ctx, Pred pred, int timeout_s) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_s);
  while (!pred()) {
    if (std::chrono::steady_clock::now() > deadline) {
      return false;
    }
    ctx->Yield();
  }
  return true;
}

// A color class whose ref matches every ray of the default document's only placement — the
// minimal raypath_color configuration that makes a real server produce composite payloads (same
// shape as the color-window suite's class factory).
gui::ColorClassRefConfig MakeColorMatchAllRef() {
  gui::ColorClassRefConfig ref;
  ref.layer_idx = 0;
  ref.crystal_pool_id = gui::g_state.layers[0].entries[0].crystal_id;
  ref.match_all = true;
  return ref;
}

gui::ColorClassConfig MakeColorMatchAllClass(float r, float g, float b, int z_order) {
  gui::ColorClassConfig cls;
  cls.color[0] = r;
  cls.color[1] = g;
  cls.color[2] = b;
  cls.visible = true;
  cls.solo = false;
  cls.z_order = z_order;
  cls.match.push_back(MakeColorMatchAllRef());
  return cls;
}

// A real server scope for the run-driven cases (same borrow-set as test_run_lifecycle.cpp's
// ScopedRunScene: the stop latch and the four epoch fields have no other owner, so the scope
// gives them back explicitly on every exit path).
struct ScopedRunServer {
  ScopedRunServer() {
    gui::JoinPendingStop();
    ResetTestState();
    gui::g_server = LUMICE_CreateServer();
    gui::ResetServerConstructionTrackers();
  }
  ~ScopedRunServer() {
    gui::g_server_poller.Stop();
    gui::JoinPendingStop();
    if (gui::g_server != nullptr) {
      LUMICE_StopServer(gui::g_server);
      LUMICE_DestroyServer(gui::g_server);
      gui::g_server = nullptr;
    }
    gui::ResetServerConstructionTrackers();
    gui::g_stop_inflight.store(false);
    gui::g_state.run_intent = RunIntent::kNone;
    gui::g_state.committed_epoch = 0;
    gui::g_state.display_epoch_floor = 0;
    gui::g_state.dirty = false;
  }
  ScopedRunServer(const ScopedRunServer&) = delete;
  ScopedRunServer& operator=(const ScopedRunServer&) = delete;
  bool ok() const { return gui::g_server != nullptr; }
};

// The XYZ float frame every fault case uploads: a coordinate pattern so two frames of the same
// size are still distinguishable by content, and small enough that the PBO double-buffer stays
// trivially exercised.
std::vector<float> XyzCoordinatePattern(int w, int h, int seed) {
  std::vector<float> tex(static_cast<size_t>(w) * h * 3);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const size_t idx = (static_cast<size_t>(y) * w + x) * 3;
      tex[idx + 0] = static_cast<float>((x * 7 + y * 3 + seed) % 251) / 251.0f;
      tex[idx + 1] = static_cast<float>((x * 11 + y * 5 + seed) % 241) / 241.0f;
      tex[idx + 2] = static_cast<float>(((x ^ y) * 13 + seed) % 239) / 239.0f;
    }
  }
  return tex;
}

// A solid RGB frame — every texel the same colour, so what the LENS does to the mapping is
// invisible and a screen capture's centre pixel names the texel directly.
std::vector<unsigned char> SolidPattern(int w, int h, unsigned char r, unsigned char g, unsigned char b) {
  std::vector<unsigned char> tex(static_cast<size_t>(w) * h * 3);
  for (size_t i = 0; i < tex.size(); i += 3) {
    tex[i + 0] = r;
    tex[i + 1] = g;
    tex[i + 2] = b;
  }
  return tex;
}

// Capture the LIVE preview viewport's pixels through the harness's full-frame hook (served by
// test_gui_main after the render pass — no per-test wiring), and return the centre pixel as
// {r,g,b}. Returns {0,0,0} with the capture failed flag set when the viewport is not active or
// the hook never completed.
struct CenterPixel {
  unsigned char r = 0;
  unsigned char g = 0;
  unsigned char b = 0;
  bool captured = false;
};

CenterPixel CapturePreviewCenterPixel(ImGuiTestContext* ctx) {
  CenterPixel out;
  const int vp_w = gui::g_preview_vp.vp_w;
  const int vp_h = gui::g_preview_vp.vp_h;
  if (!gui::g_preview_vp.active || vp_w <= 0 || vp_h <= 0) {
    return out;
  }
  g_fullframe_capture.Reset();
  g_fullframe_capture.rect_x = gui::g_preview_vp.vp_x;
  g_fullframe_capture.rect_y = gui::g_preview_vp.vp_y;
  g_fullframe_capture.rect_w = vp_w;
  g_fullframe_capture.rect_h = vp_h;
  g_fullframe_capture.requested.store(true);
  // ONE yield is the expectation: the request is consumed after this frame's render pass. The
  // loop bound is a safety net, not the mechanism (same contract as the annotation suite).
  for (int i = 0; i < 4 && !g_fullframe_capture.done.load(); ++i) {
    ctx->Yield(1);
  }
  if (!g_fullframe_capture.done.load()) {
    return out;
  }
  const int w = g_fullframe_capture.width;
  const int h = g_fullframe_capture.height;
  const size_t px = (static_cast<size_t>(h) / 2 * w + w / 2) * 4;
  if (g_fullframe_capture.pixels.size() < px + 4) {
    return out;
  }
  out.r = g_fullframe_capture.pixels[px + 0];
  out.g = g_fullframe_capture.pixels[px + 1];
  out.b = g_fullframe_capture.pixels[px + 2];
  out.captured = true;
  return out;
}

void UploadXyzAndWait(ImGuiTestContext* ctx, const std::vector<float>& xyz, int w, int h) {
  g_fault.Reset();
  g_fault.xyz = &xyz;
  g_fault.width = w;
  g_fault.height = h;
  g_fault.uploadxyz_requested = true;
  ctx->Yield(2);
  IM_CHECK(g_fault.uploadxyz_done);
}

// The half-bits twin of UploadXyzAndWait: drives UploadXyzHalfTexture (staged) on the main
// thread through the shared request channel.
void UploadXyzHalfAndWait(ImGuiTestContext* ctx, const std::vector<uint16_t>& bits, float scale, int w, int h) {
  g_fault.Reset();
  g_fault.half = &bits;
  g_fault.half_scale = scale;
  g_fault.width = w;
  g_fault.height = h;
  g_fault.uploadhalf_requested = true;
  ctx->Yield(2);
  IM_CHECK(g_fault.uploadhalf_done);
}

// Same channel, for the client-memory half entry (recovery tier 1's representation).
void UploadXyzHalfNoStagingAndWait(ImGuiTestContext* ctx, const std::vector<uint16_t>& bits, float scale, int w,
                                   int h) {
  g_fault.Reset();
  g_fault.half = &bits;
  g_fault.half_scale = scale;
  g_fault.width = w;
  g_fault.height = h;
  g_fault.uploadhalfnstaging_requested = true;
  ctx->Yield(2);
  IM_CHECK(g_fault.uploadhalfnstaging_done);
}

// Cap the strip staging's rows per strip through the main-thread channel (0 = the production
// size rule). Every case that raises the cap clears it on the way out — the seam is
// process-static, and a leaked cap would re-cut every later case's uploads.
void SetStripRowsAndWait(ImGuiTestContext* ctx, int max_rows) {
  g_fault.Reset();
  g_fault.strip_rows_value = max_rows;
  g_fault.strip_rows_requested = true;
  ctx->Yield(2);
  IM_CHECK(g_fault.strip_rows_done);
}

// The exposure premise the direct-upload pixel witnesses need: after a bare DoNew the GuiState
// carries snapshot_intensity 0, the relative-EV branch divides by it, and the shader's XYZ term
// multiplies by ZERO — every arm would compare an empty source contribution and a strip-row
// error could not turn the picture. Seeding intensity 2.0 with ev_auto 1.0 puts the applied
// intensity_scale at exactly 1.0 (2^(0+1)/2), so the ±1.0 coordinate patterns render inside the
// gamut with nothing saturating.
void SeedVisibleExposure() {
  gui::g_state.snapshot_intensity = 2.0f;
  gui::g_state.ev_auto = 1.0f;
}

// The premise check proper: the preview params the live frames render with must carry a POSITIVE
// scale, and the capture must actually carry the frame's content (a different pattern produces a
// different picture). Both arms of every pixel comparison below ride this.
void ExpectVisibleExposurePremise(ImGuiTestContext* ctx, const std::vector<float>& frame, int w, int h) {
  SeedVisibleExposure();
  UploadXyzAndWait(ctx, frame, w, h);
  IM_CHECK(g_fault.uploadxyz_result.ok);
  ctx->Yield(1);  // let the preview params recompute from the seeded state
  if (!(gui::g_preview_vp.params.exposure.intensity_scale > 0.0f)) {
    IM_ERRORF("%s",
              "premise: the rendered frame applies a non-zero exposure, or the captures below are empty of "
              "source");
  }
}

void UploadUint8ViaFaultAndWait(ImGuiTestContext* ctx, const std::vector<unsigned char>& srgb, int w, int h) {
  g_fault.Reset();
  g_fault.srgb = &srgb;
  g_fault.width = w;
  g_fault.height = h;
  g_fault.upload8_requested = true;
  ctx->Yield(2);
  IM_CHECK(g_fault.upload8_done);
}

// Yield until the open has executed or the budget is out; the flag is written by the render
// thread, so every check races a frame boundary the same way test_file_ops' helper does.
void YieldUntilOpenDone(ImGuiTestContext* ctx) {
  for (int i = 0; i < 50 && !g_open_done; ++i) {
    ctx->Yield(1);
  }
}

// A synthetic RGB frame whose value at every pixel is a function of its coordinates. The point is
// that no two pixels share a value pattern: a mirror that dropped a row, transposed the image, or
// wrote the first row everywhere would still be the right SIZE, and only per-pixel content says so.
std::vector<unsigned char> CoordinatePattern(int w, int h, int seed) {
  std::vector<unsigned char> tex(static_cast<size_t>(w) * h * 3);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const size_t idx = (static_cast<size_t>(y) * w + x) * 3;
      tex[idx + 0] = static_cast<unsigned char>((x * 7 + y * 3 + seed) % 256);
      tex[idx + 1] = static_cast<unsigned char>((x * 11 + y * 5 + seed) % 256);
      tex[idx + 2] = static_cast<unsigned char>(((x ^ y) * 13 + seed) % 256);
    }
  }
  return tex;
}

// Byte equality with a diagnostic that says how far off it was, since "not equal" on a quarter of a
// megabyte is not an answer anyone can act on. PSNR separates "one channel is swapped" (finite,
// low) from "the buffer is a different image entirely".
void ExpectSamePixels(const char* tag, const unsigned char* actual, const std::vector<unsigned char>& expected, int w,
                      int h) {
  const bool match = std::memcmp(actual, expected.data(), expected.size()) == 0;
  if (!match) {
    fprintf(stderr, "[preview_texture] %s: mismatch, PSNR = %.2f dB\n", tag,
            lumice::test::ComputePsnr(actual, expected.data(), w, h, 3));
  }
  IM_CHECK(match);
}

}  // namespace

void RegisterPreviewTextureTests(ImGuiTestEngine* engine) {
  // The mirror is a copy, not a re-read. UploadTexture stores the caller's bytes directly, and this
  // is the case that says the stored copy is the same bytes rather than, say, the GL texture read
  // back through a format conversion.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture", "an_uploaded_frame_is_mirrored_byte_for_byte");
    t->GuiFunc = UploadOnMainThreadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      constexpr int kW = 256;
      constexpr int kH = 256;
      const std::vector<unsigned char> frame = CoordinatePattern(kW, kH, /*seed=*/0);

      UploadAndWait(ctx, frame, kW, kH);

      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kW);
      IM_CHECK_EQ(gui::g_preview.GetTextureHeight(), kH);
      const unsigned char* mirror = gui::g_preview.GetTextureData();
      IM_CHECK(mirror != nullptr);
      ExpectSamePixels("upload_mirror", mirror, frame, kW, kH);
    };
  }

  // The full trip the mirror exists for: upload, save with the frame baked in, clear, load, and
  // upload what came back. The re-upload at the end is not decoration — it is what makes this
  // assert that the loaded bytes reach the RENDERER, rather than only that LoadLmcFile handed back
  // a vector. PNG is lossless, so anything short of byte equality is a defect and not a tolerance
  // question.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture", "the_saved_document_brings_the_frame_back_unchanged");
    t->GuiFunc = UploadOnMainThreadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      constexpr int kW = 256;
      constexpr int kH = 256;
      const std::vector<unsigned char> original = CoordinatePattern(kW, kH, /*seed=*/17);

      UploadAndWait(ctx, original, kW, kH);
      IM_CHECK(gui::g_preview.HasTexture());

      const std::string tmp_path = GuiTestTempPath("lumice_preview_texture_roundtrip.lmc").string();
      IM_CHECK(gui::SaveLmcFile(tmp_path, gui::g_state, gui::g_preview, /*include_texture=*/true));

      gui::DoNew();
      IM_CHECK(!gui::g_preview.HasTexture());

      gui::LmcTexture loaded;
      IM_CHECK(gui::LoadLmcFile(tmp_path, gui::g_state, loaded));
      IM_CHECK_EQ(loaded.width, kW);
      IM_CHECK_EQ(loaded.height, kH);
      // The frame went in through UploadTexture, i.e. as composited bytes, and the file must say
      // so: SaveLmcFile writes the flags from the mirror's mode, not from a constant, which is what
      // keeps a reopened pre-v4 document from being re-saved as radiance-only and re-lit twice.
      IM_CHECK(loaded.mode == gui::PreviewRenderer::TextureMode::kSrgbComposited);
      IM_CHECK(!loaded.srgb.empty());

      UploadAndWait(ctx, loaded.srgb, loaded.width, loaded.height);
      IM_CHECK(gui::g_preview.HasTexture());
      const unsigned char* mirror = gui::g_preview.GetTextureData();
      IM_CHECK(mirror != nullptr);
      ExpectSamePixels("lmc_roundtrip", mirror, original, kW, kH);

      std::remove(tmp_path.c_str());
    };
  }

  // The dimensions belong to whichever writer wrote the bytes, on the writer's OWN fields.
  // RefreshCpuTextureForSave() refreshes the CPU mirror just before a save, at whatever
  // resolution the retained snapshot happens to be — which is not always the resolution of the
  // last upload. When the mirror shared the display's width/height fields, a mirror refresh
  // resized the DISPLAY and SaveLmcFile declared sizes borrowed from wherever the last write
  // landed; since 814 the mirror carries cpu_tex_width_/height_, Save serializes those, and a
  // mirror refresh provably leaves the display size alone. The two sizes differ AND the payloads
  // differ, deliberately: a check on size alone would pass on a writer that carried the
  // dimensions across but kept the stale pixels.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_texture", "a_cpu_side_refresh_carries_its_own_dimensions_into_the_document");
    t->GuiFunc = UploadOnMainThreadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();

      // Step 1: an ordinary upload, which is what puts a size in the renderer to go stale.
      constexpr int kOldW = 256;
      constexpr int kOldH = 256;
      UploadAndWait(ctx, CoordinatePattern(kOldW, kOldH, /*seed=*/3), kOldW, kOldH);
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kOldW);
      IM_CHECK_EQ(gui::g_preview.GetTextureHeight(), kOldH);
      IM_CHECK_EQ(gui::g_preview.GetCpuTextureWidth(), kOldW);
      IM_CHECK_EQ(gui::g_preview.GetCpuTextureHeight(), kOldH);

      // Step 2: the pre-save refresh, at a different size. The display size must NOT move —
      // the mirror is the save source, and since 814 the two size facts are separate fields.
      constexpr int kNewW = 128;
      constexpr int kNewH = 128;
      const std::vector<unsigned char> refreshed = CoordinatePattern(kNewW, kNewH, /*seed=*/61);
      gui::g_preview.UpdateCpuTextureData(refreshed.data(), kNewW, kNewH);
      IM_CHECK_EQ(gui::g_preview.GetCpuTextureWidth(), kNewW);
      IM_CHECK_EQ(gui::g_preview.GetCpuTextureHeight(), kNewH);
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kOldW);
      IM_CHECK_EQ(gui::g_preview.GetTextureHeight(), kOldH);
      IM_CHECK(gui::g_preview.HasCpuTexture());
      IM_CHECK(gui::g_preview.HasTexture());

      // Step 3: what actually lands in the document — the mirror's own dimensions.
      const std::string tmp_path = GuiTestTempPath("lumice_preview_texture_dims.lmc").string();
      IM_CHECK(gui::SaveLmcFile(tmp_path, gui::g_state, gui::g_preview, /*include_texture=*/true));

      gui::GuiState loaded_state;
      gui::LmcTexture loaded;
      IM_CHECK(gui::LoadLmcFile(tmp_path, loaded_state, loaded));
      IM_CHECK_EQ(loaded.width, kNewW);
      IM_CHECK_EQ(loaded.height, kNewH);
      IM_CHECK_EQ(loaded.srgb.size(), static_cast<size_t>(kNewW) * kNewH * 3);
      ExpectSamePixels("refresh_dimensions", loaded.srgb.data(), refreshed, kNewW, kNewH);

      std::remove(tmp_path.c_str());
    };
  }

  // ---- 814 fault-witness cases ----
  //
  // Every case below forces a GL stage failure through the renderer's test hook — the SAME
  // production failure branch a real error would take — and asserts what the surrounding state
  // must look like afterwards. The contracts under witness:
  //   - a failed upload never advances the display (the pre-814 defect: tex_width_/tex_mode_
  //     committed unconditionally after a logged-only error);
  //   - OOM at a texture stage conservatively REVOKES the display (the storage is undefined), an
  //     ordinary error retains the previous frame (the command was ignored — no side effect);
  //   - the CPU mirror — the save source — outlives every display failure and never absorbs the
  //     failed frame's bytes;
  //   - a failure leaves the renderer usable: the next upload with the hook cleared succeeds.

  // OOM at the texture-define stage, no previous frame: nothing is committed and there is
  // nothing to retain.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture", "an_oom_at_texture_define_commits_nothing");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, static_cast<int>(gui::UploadResult::Stage::kTexImage), kGlOutOfMemory);
      const std::vector<float> xyz = XyzCoordinatePattern(64, 64, /*seed=*/5);
      UploadXyzAndWait(ctx, xyz, 64, 64);

      IM_CHECK(!g_fault.uploadxyz_result.ok);
      IM_CHECK(g_fault.uploadxyz_result.stage == gui::UploadResult::Stage::kTexImage);
      IM_CHECK(g_fault.uploadxyz_result.gl_error == kGlOutOfMemory);
      IM_CHECK(!g_fault.uploadxyz_result.old_frame_retained);
      IM_CHECK(!gui::g_preview.HasTexture());
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), 0);
      IM_CHECK_EQ(gui::g_preview.GetTextureHeight(), 0);

      // The renderer must stay usable after the failure.
      SetFaultAndWait(ctx, -1, 0);
      UploadXyzAndWait(ctx, xyz, 64, 64);
      IM_CHECK(g_fault.uploadxyz_result.ok);
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), 64);
      SetFaultAndWait(ctx, -1, 0);
    };
  }

  // OOM at the texture-update stage with a trusted frame already displayed: the conservative
  // revoke. The old texture's storage is undefined after an OOM (spec §2.5) — "the size query
  // would still agree" is exactly the reasoning this case exists to outlaw — so HasTexture()
  // drops, and the uint8 mirror (save source, seeded here by a good upload) keeps the FIRST
  // frame's bytes and dimensions, never the failed frame's.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_texture", "an_oom_at_texture_update_revokes_display_keeps_save_source");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, -1, 0);
      constexpr int kSide = 128;
      const std::vector<unsigned char> good = CoordinatePattern(kSide, kSide, /*seed=*/3);
      UploadAndWait(ctx, good, kSide, kSide);
      IM_CHECK(gui::g_preview.HasTexture());

      SetFaultAndWait(ctx, static_cast<int>(gui::UploadResult::Stage::kTexUpdate), kGlOutOfMemory);
      // Same size, same uint8 mode → the in-place update branch.
      const std::vector<unsigned char> failed = CoordinatePattern(kSide, kSide, /*seed=*/61);
      UploadUint8ViaFaultAndWait(ctx, failed, kSide, kSide);

      IM_CHECK(!g_fault.upload8_result.ok);
      IM_CHECK(g_fault.upload8_result.stage == gui::UploadResult::Stage::kTexUpdate);
      IM_CHECK(!g_fault.upload8_result.old_frame_retained);
      IM_CHECK(!gui::g_preview.HasTexture());
      // The save source: the last frame that was actually ACCEPTED, byte for byte.
      IM_CHECK(gui::g_preview.HasCpuTexture());
      IM_CHECK_EQ(gui::g_preview.GetCpuTextureWidth(), kSide);
      IM_CHECK_EQ(gui::g_preview.GetCpuTextureHeight(), kSide);
      ExpectSamePixels("oom_update_save_source", gui::g_preview.GetTextureData(), good, kSide, kSide);
      SetFaultAndWait(ctx, -1, 0);
    };
  }

  // An ordinary (non-OOM) error at the texture-update stage: the offending command was ignored
  // with no side effect, so the previous frame stays displayed AND stays the save source —
  // retention, the branch the OOM case must be distinguished from.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture", "an_ordinary_texture_error_keeps_the_previous_frame");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, -1, 0);
      constexpr int kSide = 128;
      const std::vector<unsigned char> good = CoordinatePattern(kSide, kSide, /*seed=*/7);
      UploadAndWait(ctx, good, kSide, kSide);
      IM_CHECK(gui::g_preview.HasTexture());

      SetFaultAndWait(ctx, static_cast<int>(gui::UploadResult::Stage::kTexUpdate), kGlInvalidValue);
      const std::vector<unsigned char> failed = CoordinatePattern(kSide, kSide, /*seed=*/8);
      UploadUint8ViaFaultAndWait(ctx, failed, kSide, kSide);

      IM_CHECK(!g_fault.upload8_result.ok);
      IM_CHECK(g_fault.upload8_result.stage == gui::UploadResult::Stage::kTexUpdate);
      IM_CHECK(g_fault.upload8_result.old_frame_retained);
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kSide);
      IM_CHECK(gui::g_preview.GetTextureMode() == gui::PreviewRenderer::TextureMode::kSrgbComposited);
      ExpectSamePixels("ordinary_error_display", gui::g_preview.GetTextureData(), good, kSide, kSide);
      SetFaultAndWait(ctx, -1, 0);
    };
  }

  // A real (or injected) OOM at the PBO-map stage: the GL's state is undefined after an OOM
  // (spec §2.5) — it is not scoped to the object the failing call was about to fill — so the
  // previously displayed frame is NOT provably safe and the display is conservatively revoked.
  // Retention would enshrine a per-stage trust boundary the spec does not draw; the save source
  // is what survives, byte for byte.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture", "a_pbo_map_oom_revokes_the_display");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, -1, 0);
      constexpr int kSide = 128;
      const std::vector<unsigned char> good = CoordinatePattern(kSide, kSide, /*seed=*/11);
      UploadAndWait(ctx, good, kSide, kSide);
      IM_CHECK(gui::g_preview.HasTexture());

      SetFaultAndWait(ctx, static_cast<int>(gui::UploadResult::Stage::kPboMap), kGlOutOfMemory);
      const std::vector<float> xyz = XyzCoordinatePattern(64, 64, /*seed=*/12);
      UploadXyzAndWait(ctx, xyz, 64, 64);

      IM_CHECK(!g_fault.uploadxyz_result.ok);
      IM_CHECK(g_fault.uploadxyz_result.stage == gui::UploadResult::Stage::kPboMap);
      IM_CHECK(!g_fault.uploadxyz_result.old_frame_retained);
      IM_CHECK(!gui::g_preview.HasTexture());
      // The save source survives the display revocation, byte for byte.
      IM_CHECK(gui::g_preview.HasCpuTexture());
      IM_CHECK_EQ(gui::g_preview.GetCpuTextureWidth(), kSide);
      ExpectSamePixels("pbo_map_oom_save_source", gui::g_preview.GetTextureData(), good, kSide, kSide);
      SetFaultAndWait(ctx, -1, 0);
    };
  }

  // A PBO-allocation OOM: same unified ruling — revoke, never register the size. The size here
  // (48x48) is deliberately one no earlier case in this suite uploads: a size some earlier
  // success already registered would skip the realloc branch entirely (the production
  // optimization this case is NOT about) and the hook would never be consulted. Observed through
  // recovery: the retry (hook cleared) reallocates and succeeds.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture", "a_pbo_allocation_failure_leaves_a_retryable_state");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, static_cast<int>(gui::UploadResult::Stage::kPboAllocate), kGlOutOfMemory);
      const std::vector<float> xyz = XyzCoordinatePattern(48, 48, /*seed=*/13);
      UploadXyzAndWait(ctx, xyz, 48, 48);

      IM_CHECK(!g_fault.uploadxyz_result.ok);
      IM_CHECK(g_fault.uploadxyz_result.stage == gui::UploadResult::Stage::kPboAllocate);
      IM_CHECK(!g_fault.uploadxyz_result.old_frame_retained);
      IM_CHECK(!gui::g_preview.HasTexture());

      SetFaultAndWait(ctx, -1, 0);
      UploadXyzAndWait(ctx, xyz, 48, 48);
      IM_CHECK(g_fault.uploadxyz_result.ok);
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), 48);
      SetFaultAndWait(ctx, -1, 0);
    };
  }

  // An ORDINARY fence-create failure (the fence came back null without memory pressure): the
  // frame itself committed fine, what failed is the guard for the NEXT write into this slot —
  // the commit stands and the slot is torn down, observed by the next upload (fresh allocation)
  // still succeeding.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_texture", "a_fence_create_failure_commits_and_tears_down_the_slot");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, static_cast<int>(gui::UploadResult::Stage::kFenceCreate), kGlInvalidValue);
      const std::vector<float> xyz = XyzCoordinatePattern(64, 64, /*seed=*/17);
      UploadXyzAndWait(ctx, xyz, 64, 64);

      IM_CHECK(g_fault.uploadxyz_result.ok);
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), 64);
      IM_CHECK(gui::g_preview.GetTextureMode() == gui::PreviewRenderer::TextureMode::kXyz);

      SetFaultAndWait(ctx, -1, 0);
      const std::vector<float> next = XyzCoordinatePattern(64, 64, /*seed=*/18);
      UploadXyzAndWait(ctx, next, 64, 64);
      IM_CHECK(g_fault.uploadxyz_result.ok);
      IM_CHECK(gui::g_preview.HasTexture());
      SetFaultAndWait(ctx, -1, 0);
    };
  }

  // A fence-create OOM: the same synthesized error one stage later flips the ruling — the GL's
  // state is undefined after an OOM, so the "frame committed, only the guard failed" story does
  // not hold and the display is revoked instead of counted as a success.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture", "a_fence_create_oom_revokes_instead_of_committing");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, static_cast<int>(gui::UploadResult::Stage::kFenceCreate), kGlOutOfMemory);
      const std::vector<float> xyz = XyzCoordinatePattern(56, 56, /*seed=*/171);
      UploadXyzAndWait(ctx, xyz, 56, 56);

      IM_CHECK(!g_fault.uploadxyz_result.ok);
      IM_CHECK(g_fault.uploadxyz_result.stage == gui::UploadResult::Stage::kFenceCreate);
      IM_CHECK(!g_fault.uploadxyz_result.old_frame_retained);
      IM_CHECK(!gui::g_preview.HasTexture());

      // Recovery: the cleared hook lets a fresh upload rebuild a trusted display.
      SetFaultAndWait(ctx, -1, 0);
      UploadXyzAndWait(ctx, xyz, 56, 56);
      IM_CHECK(g_fault.uploadxyz_result.ok);
      IM_CHECK(gui::g_preview.HasTexture());
      SetFaultAndWait(ctx, -1, 0);
    };
  }

  // The no-staging upload path (recovery tier 1): full resolution quantized into a transient CPU
  // buffer and defined from client memory. Its success commits the same display facts as the PBO
  // path; a forced texture-stage OOM on it revokes the same way (the unified ruling is not
  // PBO-specific), and its failure never registers a staging size because it never touches the
  // double buffer — the next PBO-path upload still finds its slots as the last PBO cycle left
  // them.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_texture", "a_no_staging_upload_commits_and_revokes_like_the_pbo_path");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, -1, 0);
      constexpr int kSide = 96;
      const std::vector<float> xyz = XyzCoordinatePattern(kSide, kSide, /*seed=*/31);
      // Success through the client-memory path — its own request kind on the shared GuiFunc.
      g_fault.Reset();
      g_fault.xyz = &xyz;
      g_fault.width = kSide;
      g_fault.height = kSide;
      g_fault.uploadnstaging_requested = true;
      ctx->Yield(2);
      IM_CHECK(g_fault.uploadnstaging_done);
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kSide);
      IM_CHECK(gui::g_preview.GetTextureMode() == gui::PreviewRenderer::TextureMode::kXyz);

      // Forced OOM at the client path's define: same conservative revoke as everywhere else. A
      // LARGER frame than the one above, so the tail takes the define stage — same size and mode
      // would take the in-place update stage, which is a different hook target (and its own case
      // elsewhere in this suite).
      SetFaultAndWait(ctx, static_cast<int>(gui::UploadResult::Stage::kTexImage), kGlOutOfMemory);
      constexpr int kNextSide = 128;
      const std::vector<float> next = XyzCoordinatePattern(kNextSide, kNextSide, /*seed=*/32);
      g_fault.Reset();
      g_fault.xyz = &next;
      g_fault.width = kNextSide;
      g_fault.height = kNextSide;
      g_fault.uploadnstaging_requested = true;
      ctx->Yield(2);
      IM_CHECK(g_fault.uploadnstaging_done);
      IM_CHECK(!g_fault.uploadxyz_result.ok);
      IM_CHECK(g_fault.uploadxyz_result.stage == gui::UploadResult::Stage::kTexImage);
      IM_CHECK(!gui::g_preview.HasTexture());

      // Recovery: the cleared hook lets the PBO path (tier 0) work again.
      SetFaultAndWait(ctx, -1, 0);
      UploadXyzAndWait(ctx, next, kNextSide, kNextSide);
      IM_CHECK(g_fault.uploadxyz_result.ok);
      IM_CHECK(gui::g_preview.HasTexture());
      SetFaultAndWait(ctx, -1, 0);
    };
  }

  // A storage verification that cannot witness the request refuses to display the frame — the
  // size/format query is the necessary evidence that the define landed, and without it nothing
  // may be claimed.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture", "a_storage_mismatch_refuses_to_display");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      // The hook contract says 0 = "let the real call proceed", so a synthesized mismatch carries
      // a nonzero marker: the mismatch branch forces on any nonzero and never reads the value as
      // a GL error.
      SetFaultAndWait(ctx, static_cast<int>(gui::UploadResult::Stage::kStorageMismatch), 0x0555);
      const std::vector<float> xyz = XyzCoordinatePattern(64, 64, /*seed=*/19);
      UploadXyzAndWait(ctx, xyz, 64, 64);

      IM_CHECK(!g_fault.uploadxyz_result.ok);
      IM_CHECK(g_fault.uploadxyz_result.stage == gui::UploadResult::Stage::kStorageMismatch);
      IM_CHECK(!g_fault.uploadxyz_result.old_frame_retained);
      IM_CHECK(!gui::g_preview.HasTexture());
      SetFaultAndWait(ctx, -1, 0);
    };
  }

  // The cross-document open (kOpenBaked) with the texture-define stage forced to fail. Three
  // contracts at once: the PREVIOUS document's image is revoked (an open must never leave the old
  // pixels up), the failed frame's exposure state is never committed (LoadLmcFile has already
  // replaced the whole GuiState with the new document's fresh one — snapshot_intensity is 0 — and
  // ResetFrontendState may only fill it from the file's meta on a SUCCESSFUL upload), and the
  // file's own source survives in the CPU mirror — saveable, with the preview explicitly not
  // shown. Reopening without the injection then accepts the same file normally.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_texture", "a_failed_baked_open_leaves_no_old_image_but_keeps_the_source");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, -1, 0);

      // Previous document: a displayed frame. Its exposure state is irrelevant to the witness —
      // the open REPLACES the whole GuiState before ResetFrontendState runs — so nothing is
      // planted here on purpose.
      constexpr int kSide = 128;
      UploadAndWait(ctx, CoordinatePattern(kSide, kSide, /*seed=*/21), kSide, kSide);
      IM_CHECK(gui::g_preview.HasTexture());

      // Document B: an XYZ-baked .lmc at a different size, carrying its own exposure meta.
      constexpr int kBakedW = 8;
      constexpr int kBakedH = 8;
      const std::vector<float> baked_xyz = XyzCoordinatePattern(kBakedW, kBakedH, /*seed=*/22);
      gui::XyzTextureMeta meta;
      meta.snapshot_intensity = 7.0f;
      meta.emitted_energy = 11.0f;
      meta.mono_anchor = 13.0f;
      meta.effective_pixels = 3;
      gui::g_preview.UpdateCpuXyzTextureData(baked_xyz.data(), kBakedW, kBakedH, meta);
      const std::string path = GuiTestTempPath("lumice_preview_texture_open_fail.lmc").string();
      IM_CHECK(gui::SaveLmcFile(path, gui::g_state, gui::g_preview, /*save_texture=*/true));

      SetFaultAndWait(ctx, static_cast<int>(gui::UploadResult::Stage::kTexImage), kGlOutOfMemory);
      g_open_requested = true;
      g_open_done = false;
      g_open_path = path;
      YieldUntilOpenDone(ctx);
      IM_CHECK(g_open_done);
      ctx->Yield(2);

      IM_CHECK(!gui::g_preview.HasTexture());
      IM_CHECK(gui::g_preview.HasCpuTexture());
      IM_CHECK_EQ(gui::g_preview.GetCpuTextureWidth(), kBakedW);
      IM_CHECK_EQ(gui::g_preview.GetXyzTextureMeta().snapshot_intensity, 7.0f);
      // The failed upload must not have committed the FILE's exposure into the fresh state —
      // 7.0 here would mean the pre-814 unconditional commit is back.
      IM_CHECK_EQ(gui::g_state.snapshot_intensity, 0.0f);
      SetFaultAndWait(ctx, -1, 0);

      // Recovery: the same file, opened without the injection, is accepted normally.
      g_open_requested = true;
      g_open_done = false;
      YieldUntilOpenDone(ctx);
      IM_CHECK(g_open_done);
      ctx->Yield(2);
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kBakedW);
      IM_CHECK_EQ(gui::g_state.snapshot_intensity, 7.0f);

      std::remove(path.c_str());
    };
  }

  // The no-image contract at the PIXELS, on the live screen: after a revoked display the
  // previously displayed frame must NOT appear, the blank
  // failing must not resurrect it, and recovery must. A solid red frame makes every texel name
  // itself, so the viewport's centre pixel is the whole story: red before, black while the
  // display is revoked (including with the blank's own define injected to fail — the no-sample
  // gate holds without any successful allocation), still not-red while blanked, green after a
  // verified upload re-establishes the display.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture", "a_revoked_display_never_shows_the_old_or_failed_frame");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, -1, 0);

      // A dim background photo keeps PreviewShowsPicture() true while the display is revoked —
      // that is the case the no-sample gate exists for (the panel keeps drawing, the texture
      // behind it is untrusted), and the only way the deferred preview pass keeps consuming the
      // blank attempts the revoke arms. bg_show stays false, so the photo itself never paints:
      // the centre pixel reads the SIM layer alone.
      g_fault.Reset();
      g_fault.bg_pixels = SolidPattern(32, 32, 60, 60, 60);
      g_fault.bg_width = 32;
      g_fault.bg_height = 32;
      g_fault.bg_requested = true;
      ctx->Yield(2);
      IM_CHECK(g_fault.bg_done);
      IM_CHECK(gui::g_preview.HasBackground());

      // A solid red 128x128 composited frame, displayed and visible on screen.
      constexpr int kSide = 128;
      const std::vector<unsigned char> red = SolidPattern(kSide, kSide, 240, 20, 20);
      UploadAndWait(ctx, red, kSide, kSide);
      IM_CHECK(gui::g_preview.HasTexture());
      const CenterPixel baseline = CapturePreviewCenterPixel(ctx);
      IM_CHECK(baseline.captured);
      IM_CHECK(baseline.r > 200);
      IM_CHECK(baseline.g < 60);
      IM_CHECK(baseline.b < 60);

      // OOM at the texture-define stage of an XYZ upload: the display revokes, and the screen
      // stops showing the red frame — the storage still holds it, which is exactly why the
      // no-sample gate is the mechanism under witness, not HasTexture().
      SetFaultAndWait(ctx, static_cast<int>(gui::UploadResult::Stage::kTexImage), kGlOutOfMemory);
      const std::vector<float> xyz = XyzCoordinatePattern(64, 64, /*seed=*/23);
      UploadXyzAndWait(ctx, xyz, 64, 64);
      IM_CHECK(!g_fault.uploadxyz_result.ok);
      IM_CHECK(!gui::g_preview.HasTexture());
      const CenterPixel revoked = CapturePreviewCenterPixel(ctx);
      IM_CHECK(revoked.captured);
      IM_CHECK(revoked.r < 8);
      IM_CHECK(revoked.g < 8);
      IM_CHECK(revoked.b < 8);

      // The blank redefinition ALSO fails (hook still on kTexImage): the no-sample gate keeps
      // holding — nothing red may come back while not a single allocation has succeeded.
      const CenterPixel blank_failed = CapturePreviewCenterPixel(ctx);
      IM_CHECK(blank_failed.captured);
      IM_CHECK(blank_failed.r < 8);
      IM_CHECK(blank_failed.g < 8);
      IM_CHECK(blank_failed.b < 8);
      IM_CHECK(!gui::g_preview.HasTexture());

      // Clear the hook: the pending blank now lands (a verified 1x1 define), the sim layer is
      // sampleable again — and it is BLACK, the blank's own colour, not the red frame.
      SetFaultAndWait(ctx, -1, 0);
      ctx->Yield(2);
      const CenterPixel blanked = CapturePreviewCenterPixel(ctx);
      IM_CHECK(blanked.captured);
      IM_CHECK(blanked.r < 8);
      IM_CHECK(blanked.g < 8);
      IM_CHECK(blanked.b < 8);
      IM_CHECK(!gui::g_preview.HasTexture());

      // Recovery: a verified solid-green upload re-establishes the display.
      const std::vector<unsigned char> green = SolidPattern(kSide, kSide, 20, 240, 20);
      UploadAndWait(ctx, green, kSide, kSide);
      IM_CHECK(gui::g_preview.HasTexture());
      const CenterPixel recovered = CapturePreviewCenterPixel(ctx);
      IM_CHECK(recovered.captured);
      IM_CHECK(recovered.g > 200);
      IM_CHECK(recovered.r < 60);
    };
  }

  // The composited source keeps its LABEL through a failed open: these texels already carry the
  // sky, and a mirror that said radiance-only would make the next Save write a sky-summed frame
  // under a flag that adds the sky again on reopen. Observed by re-saving the retained source
  // and reopening it: the mode must come back composited.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture", "a_failed_composited_open_keeps_the_source_label");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, -1, 0);

      // Write a composited .lmc: bytes in the mirror under kSrgbComposited (the label the v4-era
      // document carries), so the file's flags say composited.
      constexpr int kSide = 16;
      const std::vector<unsigned char> baked = SolidPattern(kSide, kSide, 30, 90, 200);
      gui::g_preview.UpdateCpuTextureData(baked.data(), kSide, kSide,
                                          gui::PreviewRenderer::TextureMode::kSrgbComposited);
      const std::string path = GuiTestTempPath("lumice_preview_texture_composited_label.lmc").string();
      IM_CHECK(gui::SaveLmcFile(path, gui::g_state, gui::g_preview, /*save_texture=*/true));

      // Open with the define stage forced to fail: preview unavailable, but the mirror keeps the
      // file's bytes under the file's label.
      SetFaultAndWait(ctx, static_cast<int>(gui::UploadResult::Stage::kTexImage), kGlOutOfMemory);
      g_open_requested = true;
      g_open_done = false;
      g_open_path = path;
      YieldUntilOpenDone(ctx);
      IM_CHECK(g_open_done);
      ctx->Yield(2);
      IM_CHECK(!gui::g_preview.HasTexture());
      IM_CHECK(gui::g_preview.HasCpuTexture());
      IM_CHECK(gui::g_preview.GetCpuTextureMode() == gui::PreviewRenderer::TextureMode::kSrgbComposited);

      // Re-save the retained source and reopen without the injection: the label must survive the
      // round trip — composited, not radiance-only (the sky must not be painted twice).
      const std::string path2 = GuiTestTempPath("lumice_preview_texture_composited_label_2.lmc").string();
      IM_CHECK(gui::SaveLmcFile(path2, gui::g_state, gui::g_preview, /*save_texture=*/true));
      SetFaultAndWait(ctx, -1, 0);
      g_open_requested = true;
      g_open_done = false;
      g_open_path = path2;
      YieldUntilOpenDone(ctx);
      IM_CHECK(g_open_done);
      ctx->Yield(2);
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK(gui::g_preview.GetTextureMode() == gui::PreviewRenderer::TextureMode::kSrgbComposited);

      std::remove(path.c_str());
      std::remove(path2.c_str());
      SetFaultAndWait(ctx, -1, 0);
    };
  }

  // The fence-wait failure branch must read and CLEAR the driver's error flag: a failed wait is
  // allowed to leave one set, and a flag left behind would leak past this candidate's boundary
  // into the NEXT candidate's error reads — which would label a successful client-memory write
  // as failed with the new frame already on the GPU (the exact pixel-vs-metadata split the
  // upload contract exists to prevent). The witness places a REAL driver flag (the hook raises
  // one through the renderer's test seam; a synthesized return value cannot produce one), then
  // proves the very next candidate — the no-staging full-size upload — commits at full size and
  // the incident reports the driver's error, not the raw wait status.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture",
                                    "a_fence_wait_failure_reads_the_real_error_and_falls_back_to_client_memory");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, -1, 0);

      // Arm both staging slots: two successful uploads leave a fence on each, so the next
      // upload's slot-0 wait has a real sync object to wait on.
      const std::vector<float> arm = XyzCoordinatePattern(32, 32, /*seed=*/31);
      UploadXyzAndWait(ctx, arm, 32, 32);
      IM_CHECK(g_fault.uploadxyz_result.ok);
      UploadXyzAndWait(ctx, arm, 32, 32);
      IM_CHECK(g_fault.uploadxyz_result.ok);

      SetCustomHookAndWait(ctx, [](gui::UploadResult::Stage s) -> int {
        if (s == gui::UploadResult::Stage::kFenceWait) {
          gui::PreviewRenderer::RaiseRealGlInvalidOperationForTest();
          return kGlWaitFailed;
        }
        return 0;
      });

      // The open's tier-0 PBO attempt waits on the armed fence and fails; the walk's next
      // candidate is the client-memory full-size upload, which must commit on the drained
      // context.
      constexpr int kW = 256;
      constexpr int kH = 128;
      const std::vector<float> baked = XyzCoordinatePattern(kW, kH, /*seed=*/32);
      gui::XyzTextureMeta meta;
      meta.snapshot_intensity = 7.0f;
      const std::string path = GuiTestTempPath("lumice_preview_texture_fence_wait.lmc").string();
      IM_CHECK(WriteXyzLmcFile(path, baked, kW, kH, meta));
      g_open_requested = true;
      g_open_done = false;
      g_open_path = path;
      YieldUntilOpenDone(ctx);
      IM_CHECK(g_open_done);
      ctx->Yield(2);

      // Full size on screen: a poisoned next candidate would have walked on to the prefiltered
      // tier and displayed 128x64 instead.
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kW);
      IM_CHECK_EQ(gui::g_preview.GetTextureHeight(), kH);
      // The incident names the fence-wait stage with the DRIVER'S error — a real GL error flag
      // the drain picked up, never the raw wait status (0x911D) and never zero. Which basic
      // error a driver reports for the rejected call is the driver's own spelling (macOS reports
      // GL_INVALID_ENUM 0x0501 here); the witness pins "a real driver error, drained", and the
      // full-size display below pins the drain itself.
      const gui::PreviewUploadIncident incident = gui::GetPreviewUploadIncident();
      IM_CHECK(incident.has_incident);
      IM_CHECK(incident.stage == static_cast<int>(gui::UploadResult::Stage::kFenceWait));
      IM_CHECK(incident.gl_error >= 0x0500 && incident.gl_error <= 0x0503);

      SetFaultAndWait(ctx, -1, 0);
      std::remove(path.c_str());
    };
  }

  // One authorized open = one attempt per recovery candidate. The counted defect had the open
  // try tier 0 outside the ladder and then walk from candidate 0 — two full-size PBO passes in
  // one cycle. The hook counts every texture-define attempt; for a 256x128 source the ladder is
  // tier 0 (PBO), tier 1 (no staging), tier 2 (2x2 prefiltered to 128x64, the floor) — exactly
  // three. The exhausted cycle's incident also stays the REAL first failure: stage, driver
  // error and request size exactly as the failing candidate reported them.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture", "one_open_walks_each_recovery_candidate_exactly_once");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, -1, 0);
      auto define_attempts = std::make_shared<std::atomic<int>>(0);
      SetCustomHookAndWait(ctx, [define_attempts](gui::UploadResult::Stage s) -> int {
        if (s == gui::UploadResult::Stage::kTexImage) {
          define_attempts->fetch_add(1);
          return kGlOutOfMemory;
        }
        return 0;
      });

      constexpr int kW = 256;
      constexpr int kH = 128;
      const std::vector<float> baked = XyzCoordinatePattern(kW, kH, /*seed=*/33);
      const std::string path = GuiTestTempPath("lumice_preview_texture_once_per_candidate.lmc").string();
      gui::XyzTextureMeta meta;
      meta.snapshot_intensity = 7.0f;
      IM_CHECK(WriteXyzLmcFile(path, baked, kW, kH, meta));
      g_open_requested = true;
      g_open_done = false;
      g_open_path = path;
      YieldUntilOpenDone(ctx);
      IM_CHECK(g_open_done);
      ctx->Yield(2);

      IM_CHECK_EQ(define_attempts->load(), 3);
      IM_CHECK(!gui::g_preview.HasTexture());
      IM_CHECK(gui::g_preview.HasCpuTexture());
      const gui::PreviewUploadIncident incident = gui::GetPreviewUploadIncident();
      IM_CHECK(incident.has_incident);
      IM_CHECK(incident.stage == static_cast<int>(gui::UploadResult::Stage::kTexImage));
      IM_CHECK_EQ(incident.gl_error, kGlOutOfMemory);
      IM_CHECK_EQ(incident.request_width, kW);
      IM_CHECK_EQ(incident.request_height, kH);
      IM_CHECK(gui::PreviewRecoveryRetryAvailable());
      IM_CHECK_EQ(gui::PreviewRecoveryNoticeKind(), 2);

      SetFaultAndWait(ctx, -1, 0);
      std::remove(path.c_str());
    };
  }

  // A failed open retries from the DOCUMENT'S OWN source, after DoOpen has returned. This is the
  // shape the borrowed-pointer defect lived in: the old retained payload wrapped the caller's
  // stack buffer, so the first retry after the open returned walked freed memory. The mirror is
  // the holder now, so the retry — first with the failure still in place (the incident must
  // still name the real texture failure, not a synthesized invalid-input), then with it cleared
  // — walks the document's own bytes to a full-size display, commits the file's exposure, and
  // leaves a document whose Save round-trips its own complete source.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture", "a_failed_open_retries_from_the_document_s_own_source");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, -1, 0);
      constexpr int kW = 256;
      constexpr int kH = 128;
      const std::vector<float> baked = XyzCoordinatePattern(kW, kH, /*seed=*/34);
      gui::XyzTextureMeta meta;
      meta.snapshot_intensity = 7.0f;
      meta.emitted_energy = 11.0f;
      meta.mono_anchor = 13.0f;
      meta.effective_pixels = 3;
      const std::string path = GuiTestTempPath("lumice_preview_texture_open_retry.lmc").string();
      IM_CHECK(WriteXyzLmcFile(path, baked, kW, kH, meta));

      SetFaultAndWait(ctx, static_cast<int>(gui::UploadResult::Stage::kTexImage), kGlOutOfMemory);
      g_open_requested = true;
      g_open_done = false;
      g_open_path = path;
      YieldUntilOpenDone(ctx);
      IM_CHECK(g_open_done);
      ctx->Yield(2);
      IM_CHECK(!gui::g_preview.HasTexture());
      IM_CHECK(gui::g_preview.HasCpuTexture());
      IM_CHECK_EQ(gui::g_preview.GetCpuTextureWidth(), kW);
      IM_CHECK_EQ(gui::g_state.snapshot_intensity, 0.0f);  // failure committed no exposure
      IM_CHECK(gui::PreviewRecoveryRetryAvailable());

      // Retry with the failure still armed: the walk runs out again, nothing is committed, and
      // the incident STILL names the real first failure of THIS cycle.
      const unsigned long long baseline = gui::g_state.texture_upload_count;
      RetryAndWait(ctx);
      IM_CHECK(!gui::g_preview.HasTexture());
      IM_CHECK_EQ(gui::g_state.texture_upload_count, baseline);
      const gui::PreviewUploadIncident failed_retry = gui::GetPreviewUploadIncident();
      IM_CHECK(failed_retry.has_incident);
      IM_CHECK(failed_retry.stage == static_cast<int>(gui::UploadResult::Stage::kTexImage));
      IM_CHECK_EQ(failed_retry.gl_error, kGlOutOfMemory);
      IM_CHECK_EQ(failed_retry.request_width, kW);
      IM_CHECK_EQ(failed_retry.request_height, kH);

      // Clear the failure: the SAME retained document source re-uploads to full size.
      SetFaultAndWait(ctx, -1, 0);
      RetryAndWait(ctx);
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kW);
      IM_CHECK_EQ(gui::g_preview.GetTextureHeight(), kH);
      IM_CHECK_EQ(gui::g_state.snapshot_intensity, 7.0f);
      IM_CHECK_EQ(gui::g_state.texture_upload_count, baseline + 1);
      IM_CHECK(!gui::PreviewRecoveryRetryAvailable());
      IM_CHECK_EQ(gui::PreviewRecoveryNoticeKind(), 0);

      // The recovered document saves its own complete source, metadata intact.
      const std::string path2 = GuiTestTempPath("lumice_preview_texture_open_retry_saved.lmc").string();
      IM_CHECK(gui::SaveLmcFile(path2, gui::g_state, gui::g_preview, /*save_texture=*/true));
      {
        gui::GuiState scratch;
        gui::LmcTexture tex;
        IM_CHECK(gui::LoadLmcFile(path2, scratch, tex));
        IM_CHECK_EQ(tex.width, kW);
        IM_CHECK_EQ(tex.height, kH);
        IM_CHECK_EQ(tex.meta.snapshot_intensity, 7.0f);
      }

      std::remove(path.c_str());
      std::remove(path2.c_str());
    };
  }

  // A uint8 document whose open fails gets the SAME observable failure exit the XYZ path has:
  // incident, exhausted (the unavailable notice), a retry that survives Dismiss, and a retry
  // that re-uploads in the document's OWN mode — radiance, then composited. The r4 gap: these
  // failures only restored the mirror and logged, leaving the panel without a notice or a way
  // back.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture",
                                    "a_failed_uint8_open_wires_the_failure_exit_and_retries_in_its_own_mode");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, -1, 0);
      constexpr int kSide = 16;

      // Radiance leg.
      const std::vector<unsigned char> rad = SolidPattern(kSide, kSide, 90, 160, 30);
      gui::g_preview.UpdateCpuTextureData(rad.data(), kSide, kSide, gui::PreviewRenderer::TextureMode::kSrgbRadiance);
      const std::string rad_path = GuiTestTempPath("lumice_preview_texture_uint8_retry_rad.lmc").string();
      IM_CHECK(gui::SaveLmcFile(rad_path, gui::g_state, gui::g_preview, /*save_texture=*/true));
      SetFaultAndWait(ctx, static_cast<int>(gui::UploadResult::Stage::kTexImage), kGlOutOfMemory);
      g_open_requested = true;
      g_open_done = false;
      g_open_path = rad_path;
      YieldUntilOpenDone(ctx);
      IM_CHECK(g_open_done);
      ctx->Yield(2);
      IM_CHECK(!gui::g_preview.HasTexture());
      IM_CHECK(gui::g_preview.HasCpuTexture());
      const gui::PreviewUploadIncident rad_incident = gui::GetPreviewUploadIncident();
      IM_CHECK(rad_incident.has_incident);
      IM_CHECK(rad_incident.stage == static_cast<int>(gui::UploadResult::Stage::kTexImage));
      IM_CHECK(gui::PreviewRecoveryRetryAvailable());
      IM_CHECK_EQ(gui::PreviewRecoveryNoticeKind(), 2);
      gui::DismissPreviewRecoveryNotice();
      IM_CHECK_EQ(gui::PreviewRecoveryNoticeKind(), 0);
      IM_CHECK(gui::PreviewRecoveryRetryAvailable());  // Dismiss hides the notice, not the exit
      SetFaultAndWait(ctx, -1, 0);
      RetryAndWait(ctx);
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK(gui::g_preview.GetTextureMode() == gui::PreviewRenderer::TextureMode::kSrgbRadiance);
      IM_CHECK_EQ(gui::PreviewRecoveryNoticeKind(), 0);

      // Composited leg: the second open resets the document wholesale, so the two legs share
      // nothing but the harness.
      const std::vector<unsigned char> comp = SolidPattern(kSide, kSide, 30, 90, 200);
      gui::g_preview.UpdateCpuTextureData(comp.data(), kSide, kSide,
                                          gui::PreviewRenderer::TextureMode::kSrgbComposited);
      const std::string comp_path = GuiTestTempPath("lumice_preview_texture_uint8_retry_comp.lmc").string();
      IM_CHECK(gui::SaveLmcFile(comp_path, gui::g_state, gui::g_preview, /*save_texture=*/true));
      SetFaultAndWait(ctx, static_cast<int>(gui::UploadResult::Stage::kTexImage), kGlOutOfMemory);
      g_open_requested = true;
      g_open_done = false;
      g_open_path = comp_path;
      YieldUntilOpenDone(ctx);
      IM_CHECK(g_open_done);
      ctx->Yield(2);
      IM_CHECK(!gui::g_preview.HasTexture());
      IM_CHECK(gui::GetPreviewUploadIncident().has_incident);
      IM_CHECK(gui::PreviewRecoveryRetryAvailable());
      IM_CHECK_EQ(gui::PreviewRecoveryNoticeKind(), 2);
      SetFaultAndWait(ctx, -1, 0);
      RetryAndWait(ctx);
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK(gui::g_preview.GetTextureMode() == gui::PreviewRenderer::TextureMode::kSrgbComposited);

      std::remove(rad_path.c_str());
      std::remove(comp_path.c_str());
    };
  }

  // A degraded open re-probes to full resolution on retry: the file source — not a live
  // payload — is what makes the retry exit exist for a document that was never simulated, and
  // the re-walk runs the real candidates (128x64 first, then the full 256x128 once the failure
  // is gone).
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture", "a_degraded_open_reprobes_to_full_resolution_on_retry");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, -1, 0);
      constexpr int kW = 256;
      constexpr int kH = 128;
      const std::vector<float> baked = XyzCoordinatePattern(kW, kH, /*seed=*/35);
      const std::string path = GuiTestTempPath("lumice_preview_texture_open_degraded.lmc").string();
      gui::XyzTextureMeta meta;
      meta.snapshot_intensity = 7.0f;
      IM_CHECK(WriteXyzLmcFile(path, baked, kW, kH, meta));

      // The first two texture-define attempts (tier 0 and tier 1, both full-size) fail; the
      // walk establishes the first prefiltered tier.
      SetCustomHookAndWait(ctx,
                           FailFirstNThenPass(gui::UploadResult::Stage::kTexImage, /*fail_count=*/2, kGlOutOfMemory));
      g_open_requested = true;
      g_open_done = false;
      g_open_path = path;
      YieldUntilOpenDone(ctx);
      IM_CHECK(g_open_done);
      ctx->Yield(2);
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kW / 2);
      IM_CHECK_EQ(gui::g_preview.GetTextureHeight(), kH / 2);
      IM_CHECK_EQ(gui::PreviewRecoveryNoticeKind(), 1);
      IM_CHECK(gui::PreviewRecoveryRetryAvailable());
      // The tier-0 attempt's full-size staging slots ended their lifecycle when the walk
      // established a client-memory tier — a reduced-tier document does not carry two dead
      // full-size PBOs until Destroy.
      IM_CHECK_EQ(gui::g_preview.StagingSlotByteCountForTest(0), 0u);
      IM_CHECK_EQ(gui::g_preview.StagingSlotByteCountForTest(1), 0u);

      SetFaultAndWait(ctx, -1, 0);
      RetryAndWait(ctx);
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kW);
      IM_CHECK_EQ(gui::g_preview.GetTextureHeight(), kH);
      IM_CHECK(!gui::PreviewRecoveryRetryAvailable());
      IM_CHECK_EQ(gui::PreviewRecoveryNoticeKind(), 0);
      // The re-established tier-0 path re-allocated its slot lazily.
      IM_CHECK(gui::g_preview.StagingSlotByteCountForTest(0) > 0);

      std::remove(path.c_str());
    };
  }

  // The one adjudication at work over a REAL run: a displayed FILE frame survives a newer
  // failed live frame as the save source AND the absolute-EV denominator. The r4 fork had the
  // extent query prefer the never-displayed pending frame (512x256) over the file frame the
  // screen actually shows (8x8) — silently moving the exposure normalization off the displayed
  // picture. Asserted through the production chain: real DoRun, real SyncFromPoller failure,
  // real save.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_texture", "the_document_s_source_survives_a_failed_newer_live_frame");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedRunServer scene;
      IM_CHECK(scene.ok());
      SetFaultAndWait(ctx, -1, 0);

      // File A: 8x8 with its own exposure meta, displayed without injection.
      constexpr int kAW = 8;
      constexpr int kAH = 8;
      const std::vector<float> baked = XyzCoordinatePattern(kAW, kAH, /*seed=*/36);
      gui::XyzTextureMeta meta;
      meta.snapshot_intensity = 5.0f;
      meta.emitted_energy = 9.0f;
      meta.mono_anchor = 2.0f;
      meta.effective_pixels = 1;
      const std::string path = GuiTestTempPath("lumice_preview_texture_source_fork.lmc").string();
      IM_CHECK(WriteXyzLmcFile(path, baked, kAW, kAH, meta));
      g_open_requested = true;
      g_open_done = false;
      g_open_path = path;
      YieldUntilOpenDone(ctx);
      IM_CHECK(g_open_done);
      ctx->Yield(2);
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kAW);

      // A real run whose every define fails with an ORDINARY error: the offending command is
      // ignored, the live frame is retained as pending, and the file frame stays displayed. (An
      // injected OOM would be the revoke path — correct production behavior, the wrong scenario
      // for this witness.)
      SetFaultAndWait(ctx, static_cast<int>(gui::UploadResult::Stage::kTexImage), kGlInvalidValue);
      SeedFiniteRun(/*millions=*/0.1f);
      gui::DoRun(/*user_initiated=*/true);
      IM_CHECK(DriveUntil(
          ctx,
          [] {
            return gui::g_state.sim_state == SimState::kDone && gui::g_state.run_intent == RunIntent::kRunCompleted;
          },
          20));
      ctx->Yield(2);
      IM_CHECK(gui::g_state.upload_latched);

      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kAW);  // the file frame stays displayed
      // The adjudication names the FILE frame for exposure and export — not the failed pending
      // one at 1024x512.
      IM_CHECK_EQ(gui::PreviewSourcePixelCount(), static_cast<long>(kAW) * kAH);
      const gui::PreviewUploadIncident incident = gui::GetPreviewUploadIncident();
      IM_CHECK(incident.has_incident);
      IM_CHECK(incident.stage == static_cast<int>(gui::UploadResult::Stage::kTexImage));
      IM_CHECK_EQ(incident.gl_error, kGlInvalidValue);
      IM_CHECK_EQ(incident.request_width, 1024);
      IM_CHECK_EQ(incident.request_height, 512);

      // Save writes the file's own complete source.
      const std::string saved = GuiTestTempPath("lumice_preview_texture_source_fork_saved.lmc").string();
      IM_CHECK(gui::SaveLmcFile(saved, gui::g_state, gui::g_preview, /*save_texture=*/true));
      {
        gui::GuiState scratch;
        gui::LmcTexture tex;
        IM_CHECK(gui::LoadLmcFile(saved, scratch, tex));
        IM_CHECK_EQ(tex.width, kAW);
        IM_CHECK_EQ(tex.height, kAH);
        IM_CHECK_EQ(tex.meta.snapshot_intensity, 5.0f);
      }

      SetFaultAndWait(ctx, -1, 0);
      std::remove(path.c_str());
      std::remove(saved.c_str());
    };
  }

  // The pending fallback stays the NEWEST retained frame across saves: a first failure, saved,
  // then a terminal frame retained while latched, then a second save — the second file must
  // carry the terminal frame's measurements, not the stale mirror copy of the first failure.
  // The r4 defect: the mirror being non-empty skipped the refresh, so the second save silently
  // rewrote the first frame.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture", "a_second_save_picks_up_the_newest_retained_frame");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedRunServer scene;
      IM_CHECK(scene.ok());
      SetFaultAndWait(ctx, -1, 0);
      SetFaultAndWait(ctx, static_cast<int>(gui::UploadResult::Stage::kTexImage), kGlInvalidValue);
      // INFINITE on purpose: the witness needs the run to keep producing newer qualifying frames
      // AFTER the first failure, on this machine's clock — a finite budget can finish inside one
      // frame on a fast GPU and leave nothing newer for the second save to pick up.
      SeedInfiniteRun();
      gui::DoRun(/*user_initiated=*/true);

      // First live frame fails everywhere and is retained.
      IM_CHECK(DriveUntil(ctx, [] { return gui::g_state.upload_latched; }, 20));
      // The production save entry (PerformSave = refresh-from-adjudication + write; the file
      // path is set so no dialog opens).
      const std::string save1 = GuiTestTempPath("lumice_preview_texture_pending_save1.lmc").string();
      gui::g_state.current_file_path = save1;
      gui::PerformSave();
      float intensity1 = 0.0f;
      {
        gui::GuiState scratch;
        gui::LmcTexture tex;
        IM_CHECK(gui::LoadLmcFile(save1, scratch, tex));
        intensity1 = tex.meta.snapshot_intensity;
        IM_CHECK_EQ(tex.width, 1024);
        IM_CHECK_EQ(tex.height, 512);
      }

      // Stop the run: the drain publishes the TERMINAL frame, which the latch-period
      // bookkeeping retains as the newest qualifying source while it stays off the GPU. (An
      // open-ended settle would burn a full core set for the whole test and blow the engine's
      // watchdog under pool load; a bounded run cannot guarantee a SECOND frame on a fast GPU —
      // this shape gets the newer-frame premise deterministically on any machine's clock.)
      gui::DoStop();
      IM_CHECK(DriveUntil(
          ctx,
          [] { return gui::g_state.sim_state == SimState::kDone && gui::g_state.run_intent == RunIntent::kStopped; },
          20));
      ctx->Yield(2);

      // The second save refreshes from the newest retained frame.
      const std::string save2 = GuiTestTempPath("lumice_preview_texture_pending_save2.lmc").string();
      gui::g_state.current_file_path = save2;
      gui::PerformSave();
      {
        gui::GuiState scratch;
        gui::LmcTexture tex;
        IM_CHECK(gui::LoadLmcFile(save2, scratch, tex));
        IM_CHECK_EQ(tex.width, 1024);
        IM_CHECK_EQ(tex.height, 512);
        IM_CHECK(tex.meta.snapshot_intensity > intensity1);
      }
      const gui::PreviewUploadIncident incident = gui::GetPreviewUploadIncident();
      IM_CHECK(incident.has_incident);
      IM_CHECK(incident.stage == static_cast<int>(gui::UploadResult::Stage::kTexImage));
      IM_CHECK_EQ(incident.gl_error, kGlInvalidValue);

      SetFaultAndWait(ctx, -1, 0);
      std::remove(save1.c_str());
      std::remove(save2.c_str());
    };
  }

  // The degraded round trip at the pixels: a live frame accepted at a prefiltered tier, saved
  // (the file keeps the FULL source), reopened onto the SAME tier, and the two viewport frames
  // compared BYTE for BYTE — the same-tier live/save/reopen consistency the plan pins at tau=0,
  // witnessed on the whole picture rather than a statistic.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture",
                                    "a_degraded_live_frame_roundtrips_tau_zero_through_save_and_reopen");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedRunServer scene;
      IM_CHECK(scene.ok());
      SetFaultAndWait(ctx, -1, 0);
      // Tier 0 and tier 1 (both full-size) fail; the walk establishes tier 2 (512x256 from
      // the run's 1024x512 frame: BuildScene sizes the canvas 2*res x res).
      SetCustomHookAndWait(ctx,
                           FailFirstNThenPass(gui::UploadResult::Stage::kTexImage, /*fail_count=*/2, kGlOutOfMemory));
      SeedFiniteRun(/*millions=*/0.1f);
      gui::DoRun(/*user_initiated=*/true);
      IM_CHECK(WaitForSimRestartAtLeast(ctx, /*baseline_upload_count=*/0, /*timeout_ms=*/20000));
      IM_CHECK(DriveUntil(
          ctx,
          [] {
            return gui::g_state.sim_state == SimState::kDone && gui::g_state.run_intent == RunIntent::kRunCompleted;
          },
          30));
      ctx->Yield(3);  // drain the run's in-flight uploads before reading the display
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), 512);
      IM_CHECK_EQ(gui::g_preview.GetTextureHeight(), 256);
      const std::vector<unsigned char> live_frame = CapturePreviewFullFrame(ctx);
      IM_CHECK(!live_frame.empty());

      // The file keeps the full-resolution source while the display stays degraded.
      const std::string path = GuiTestTempPath("lumice_preview_texture_degraded_rt.lmc").string();
      gui::g_state.current_file_path = path;
      gui::PerformSave();
      {
        gui::GuiState scratch;
        gui::LmcTexture tex;
        IM_CHECK(gui::LoadLmcFile(path, scratch, tex));
        IM_CHECK_EQ(tex.width, 1024);
        IM_CHECK_EQ(tex.height, 512);
      }

      // Re-arm the same ladder so the reopen lands on the SAME tier, then compare.
      SetCustomHookAndWait(ctx,
                           FailFirstNThenPass(gui::UploadResult::Stage::kTexImage, /*fail_count=*/2, kGlOutOfMemory));
      g_open_requested = true;
      g_open_done = false;
      g_open_path = path;
      YieldUntilOpenDone(ctx);
      IM_CHECK(g_open_done);
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), 512);
      IM_CHECK_EQ(gui::g_preview.GetTextureHeight(), 256);
      const std::vector<unsigned char> reopen_frame = CapturePreviewFullFrame(ctx);
      IM_CHECK(!reopen_frame.empty());
      IM_CHECK_EQ(live_frame.size(), reopen_frame.size());
      IM_CHECK(std::memcmp(live_frame.data(), reopen_frame.data(), live_frame.size()) == 0);

      SetFaultAndWait(ctx, -1, 0);
      std::remove(path.c_str());
    };
  }

  // The degraded all-sky export is the FULL-source render, byte for byte, whatever the display
  // tier: exporting while degraded must produce exactly the file the healthy route produces
  // from the same document — a low-resolution upsample would differ on this high-frequency
  // pattern. The export entry is the production body (no dialog), and the source extent it
  // renders at comes from the one adjudication.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_texture", "the_degraded_all_sky_export_matches_the_full_source_render");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, -1, 0);
      constexpr int kW = 256;
      constexpr int kH = 128;
      const std::vector<float> baked = XyzCoordinatePattern(kW, kH, /*seed=*/37);
      const std::string path = GuiTestTempPath("lumice_preview_texture_degraded_export.lmc").string();
      gui::XyzTextureMeta meta;
      meta.snapshot_intensity = 7.0f;
      IM_CHECK(WriteXyzLmcFile(path, baked, kW, kH, meta));

      SetCustomHookAndWait(ctx,
                           FailFirstNThenPass(gui::UploadResult::Stage::kTexImage, /*fail_count=*/2, kGlOutOfMemory));
      g_open_requested = true;
      g_open_done = false;
      g_open_path = path;
      YieldUntilOpenDone(ctx);
      IM_CHECK(g_open_done);
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kW / 2);  // degraded on screen
      IM_CHECK_EQ(gui::PreviewSourcePixelCount(), static_cast<long>(kW) * kH);

      const std::string png_a = GuiTestTempPath("lumice_preview_texture_degraded_export_a.png").string();
      std::remove(png_a.c_str());
      IM_CHECK(ExportDualFisheyeAndWait(ctx, png_a));
      std::vector<unsigned char> img_a;
      int w_a = 0;
      int h_a = 0;
      int ch_a = 0;
      IM_CHECK(lumice::test::LoadPng(png_a.c_str(), img_a, w_a, h_a, ch_a));
      IM_CHECK_EQ(w_a, kW);
      IM_CHECK_EQ(h_a, kH);

      // Recover to full and export again through the healthy route: the two files must agree
      // byte for byte.
      SetFaultAndWait(ctx, -1, 0);
      RetryAndWait(ctx);
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kW);
      const std::string png_b = GuiTestTempPath("lumice_preview_texture_degraded_export_b.png").string();
      std::remove(png_b.c_str());
      IM_CHECK(ExportDualFisheyeAndWait(ctx, png_b));
      std::vector<unsigned char> img_b;
      int w_b = 0;
      int h_b = 0;
      int ch_b = 0;
      IM_CHECK(lumice::test::LoadPng(png_b.c_str(), img_b, w_b, h_b, ch_b));
      IM_CHECK_EQ(w_b, kW);
      IM_CHECK_EQ(h_b, kH);
      IM_CHECK_EQ(img_a.size(), img_b.size());
      IM_CHECK(std::memcmp(img_a.data(), img_b.data(), img_a.size()) == 0);

      std::remove(path.c_str());
      std::remove(png_a.c_str());
      std::remove(png_b.c_str());
    };
  }

  // The retry's display-mode adjudication is the SAME one the live upload uses, channel-B-R
  // exclusion included. The r5 fork: Retry read the raw toggle, so with the diagnostic on screen
  // a failed composite frame's retry uploaded the raypath PALETTE and recorded the display mode
  // to match — while the live path was preferring the full-spectrum XYZ bytes for that very
  // display. Driven through production: a composite-producing run displays its composite; the
  // diagnostic flips on, the mode-change XYZ upload is forced to fail (the composite payload is
  // retained); the retry must re-attempt the SPECTRUM and record the XYZ shape.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_texture", "a_retry_prefers_the_spectrum_when_the_br_diagnostic_is_on_screen");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedRunServer scene;
      IM_CHECK(scene.ok());
      SetFaultAndWait(ctx, -1, 0);
      gui::g_state.raypath_color.push_back(MakeColorMatchAllClass(1.0f, 0.0f, 0.0f, /*z_order=*/0));
      gui::g_state.show_composite_preview = true;
      gui::g_state.renderer.display_mode = LUMICE_DISPLAY_MODE_NORMAL;

      SeedFiniteRun(/*millions=*/0.1f);
      gui::DoRun(/*user_initiated=*/true);
      // The composite lands first (the composite-producing fact the fork depends on), visibly in
      // the display mode record.
      IM_CHECK(WaitForSimRestartAtLeast(ctx, /*baseline_upload_count=*/0, /*timeout_ms=*/20000));
      IM_CHECK(gui::g_state.last_uploaded_as_composite);
      IM_CHECK(gui::g_preview.GetTextureMode() == gui::PreviewRenderer::TextureMode::kSrgbRadiance);

      // The diagnostic flips on; the very next sync re-uploads the same frame as XYZ. That
      // upload — and only that upload — is forced to fail, so the retained frame is the
      // composite payload.
      SetFaultAndWait(ctx, static_cast<int>(gui::UploadResult::Stage::kTexImage), kGlInvalidValue);
      gui::g_state.renderer.display_mode = LUMICE_DISPLAY_MODE_CHANNEL_BR;
      IM_CHECK(DriveUntil(ctx, [] { return gui::g_state.upload_latched; }, 20));
      IM_CHECK(DriveUntil(
          ctx,
          [] {
            return gui::g_state.sim_state == SimState::kDone && gui::g_state.run_intent == RunIntent::kRunCompleted;
          },
          20));
      ctx->Yield(2);

      // The retry re-attempts the spectrum. The commit is read INSIDE the retry request, before
      // the next SyncFromPoller runs, so no normal sync can re-fire the mode-change branch and
      // correct the record first — the commit-time fork shape this guards against.
      SetFaultAndWait(ctx, -1, 0);  // the failure is over: the retry may succeed
      const unsigned long long before_retry = gui::g_state.texture_upload_count;
      RetryAndWait(ctx);
      IM_CHECK_EQ(g_fault.retry_snap_show_composite.load(), 1);
      IM_CHECK_EQ(g_fault.retry_snap_display_mode.load(), LUMICE_DISPLAY_MODE_CHANNEL_BR);
      IM_CHECK_EQ(g_fault.retry_snap_tex_mode.load(), static_cast<int>(gui::PreviewRenderer::TextureMode::kXyz));
      IM_CHECK_EQ(g_fault.retry_snap_as_composite.load(), 0);
      IM_CHECK_EQ(g_fault.retry_snap_upload_count.load(), before_retry + 1);

      // Settle the run's tail, then the frozen terminal frame must stay silent: the committed
      // mode matches the effective preference, so no mode-change re-upload of the same serial.
      ctx->Yield(10);
      const unsigned long long settled = gui::g_state.texture_upload_count;
      ctx->Yield(5);
      IM_CHECK_EQ(gui::g_state.texture_upload_count, settled);
      IM_CHECK(gui::g_preview.GetTextureMode() == gui::PreviewRenderer::TextureMode::kXyz);
      IM_CHECK(!gui::g_state.last_uploaded_as_composite);
      IM_CHECK(gui::g_preview.GetTextureWidth() == 1024);
      IM_CHECK(gui::g_preview.GetTextureHeight() == 512);
    };
  }

  // The degraded export's out-of-memory is a SHARED-CONTEXT fact: when the transient full-source
  // renderer's upload hits a real OOM, the export fails explicitly AND the preview stops
  // sampling a texture nothing can vouch for — one authorized rebuild cycle runs against the
  // retained source, and failing that, the explicit unavailable state stands with the source
  // intact and the retry exit open.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_texture", "an_export_oom_revokes_and_rebuilds_the_preview_it_threatened");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, -1, 0);
      constexpr int kW = 256;
      constexpr int kH = 128;
      const std::vector<float> baked = XyzCoordinatePattern(kW, kH, /*seed=*/38);
      const std::string path = GuiTestTempPath("lumice_preview_texture_export_oom.lmc").string();
      gui::XyzTextureMeta meta;
      meta.snapshot_intensity = 7.0f;
      IM_CHECK(WriteXyzLmcFile(path, baked, kW, kH, meta));

      // Degraded open: the two full-size candidates fail, tier 2 displays.
      SetCustomHookAndWait(ctx,
                           FailFirstNThenPass(gui::UploadResult::Stage::kTexImage, /*fail_count=*/2, kGlOutOfMemory));
      g_open_requested = true;
      g_open_done = false;
      g_open_path = path;
      YieldUntilOpenDone(ctx);
      IM_CHECK(g_open_done);
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kW / 2);

      // Every renderer in the context now fails texture defines with a REAL-looking OOM: the
      // export's transient upload, and the rebuild cycle the revoke arms.
      SetAllHookAndWait(ctx, [](gui::UploadResult::Stage st) -> int {
        return st == gui::UploadResult::Stage::kTexImage ? kGlOutOfMemory : 0;
      });
      const std::string png = GuiTestTempPath("lumice_preview_texture_export_oom.png").string();
      std::remove(png.c_str());
      IM_CHECK(!ExportDualFisheyeAndWait(ctx, png));
      IM_CHECK(!std::filesystem::exists(png));  // no file, not even a partial one
      // The preview was revoked and the rebuild failed: the explicit no-image state, not the
      // pre-export degraded picture still being sampled.
      IM_CHECK(!gui::g_preview.HasTexture());
      IM_CHECK(gui::g_preview.HasCpuTexture());  // the source is intact
      IM_CHECK_EQ(gui::PreviewRecoveryNoticeKind(), 2);
      IM_CHECK(gui::PreviewRecoveryRetryAvailable());
      const gui::PreviewUploadIncident incident = gui::GetPreviewUploadIncident();
      IM_CHECK(incident.has_incident);
      IM_CHECK(incident.stage == static_cast<int>(gui::UploadResult::Stage::kTexImage));
      IM_CHECK_EQ(incident.gl_error, kGlOutOfMemory);
      IM_CHECK_EQ(incident.request_width, kW);
      IM_CHECK_EQ(incident.request_height, kH);

      // The failure is over: the retained source recovers the display in full.
      SetAllHookAndWait(ctx, nullptr);
      RetryAndWait(ctx);
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kW);
      IM_CHECK_EQ(gui::g_preview.GetTextureHeight(), kH);

      std::remove(path.c_str());
    };
  }

  // The screenshot's RGBA8 target was the last production consumer reading the shared FBO helper
  // through the defaulted out-param (the r4 gap): a failed target allocation left the display's
  // sampling qualification standing over unprovable storage. Now the same §2.5 ruling applies —
  // revoke, one authorized rebuild against the document's own source, cursor and mode record
  // untouched — and a healthy screenshot still renders afterwards.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_texture", "a_screenshot_target_oom_revokes_and_rebuilds_the_preview");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, -1, 0);
      constexpr int kW = 256;
      constexpr int kH = 128;
      const std::vector<float> baked = XyzCoordinatePattern(kW, kH, /*seed=*/41);
      const std::string path = GuiTestTempPath("lumice_preview_texture_shot_target_oom.lmc").string();
      gui::XyzTextureMeta meta;
      meta.snapshot_intensity = 7.0f;
      IM_CHECK(WriteXyzLmcFile(path, baked, kW, kH, meta));
      g_open_requested = true;
      g_open_done = false;
      g_open_path = path;
      YieldUntilOpenDone(ctx);
      IM_CHECK(g_open_done);
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kW);
      const int vp_w = gui::g_preview_vp.vp_w;
      const int vp_h = gui::g_preview_vp.vp_h;

      const unsigned long long serial_before = gui::g_state.last_uploaded_texture_serial;
      const unsigned long long count_before = gui::g_state.texture_upload_count;
      SetStorageFaultAndWait(ctx, 1);
      const gui::ScreenshotRender shot = ScreenshotAndWait(ctx);
      IM_CHECK(shot.rgba.empty());  // the target never came up; nothing is written anywhere
      // The rebuild stood the display back up from the document's own source.
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kW);
      IM_CHECK_EQ(gui::g_preview.GetTextureHeight(), kH);
      IM_CHECK(gui::g_preview.GetTextureMode() == gui::PreviewRenderer::TextureMode::kXyz);
      IM_CHECK(!gui::g_state.last_uploaded_as_composite);
      IM_CHECK_EQ(gui::g_state.texture_upload_count, count_before + 1);
      IM_CHECK_EQ(gui::g_state.last_uploaded_texture_serial, serial_before);  // cursor untouched
      IM_CHECK_EQ(gui::PreviewRecoveryNoticeKind(), 0);
      IM_CHECK(!gui::PreviewRecoveryRetryAvailable());
      // The r2 Minor: the TARGET's allocation failure is in the retained incident record even
      // though the rebuild succeeded — the structured entry the diagnostics read carries the
      // trigger, not a clean slate.
      const gui::PreviewUploadIncident shot_incident = gui::GetPreviewUploadIncident();
      IM_CHECK(shot_incident.has_incident);
      IM_CHECK(shot_incident.stage == static_cast<int>(gui::UploadResult::Stage::kRenderTargetAllocation));
      IM_CHECK_EQ(shot_incident.gl_error, kGlOutOfMemory);
      IM_CHECK_EQ(shot_incident.request_width, vp_w);
      IM_CHECK_EQ(shot_incident.request_height, vp_h);

      // The fault was one-shot: the screenshot's healthy path still renders the viewport.
      SetStorageFaultAndWait(ctx, 0);
      const gui::ScreenshotRender healthy = ScreenshotAndWait(ctx);
      IM_CHECK(!healthy.rgba.empty());
      IM_CHECK_EQ(healthy.w, vp_w);
      IM_CHECK_EQ(healthy.h, vp_h);

      std::remove(path.c_str());
    };
  }

  // Both all-sky exports' NORMAL-resolution branches now consume the target's real allocation
  // error (the r3 sites), driven at the target-allocation level this time: one fault per branch,
  // each revoking and rebuilding in full from the file source.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture",
                                    "an_all_sky_export_target_oom_propagates_from_both_normal_branches");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, -1, 0);
      constexpr int kW = 256;
      constexpr int kH = 128;
      const std::vector<float> baked = XyzCoordinatePattern(kW, kH, /*seed=*/43);
      const std::string path = GuiTestTempPath("lumice_preview_texture_allsky_target_oom.lmc").string();
      gui::XyzTextureMeta meta;
      meta.snapshot_intensity = 7.0f;
      IM_CHECK(WriteXyzLmcFile(path, baked, kW, kH, meta));
      g_open_requested = true;
      g_open_done = false;
      g_open_path = path;
      YieldUntilOpenDone(ctx);
      IM_CHECK(g_open_done);
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kW);

      // Dual-fisheye, display healthy: the normal branch renders g_preview at its own extent.
      unsigned long long count_before = gui::g_state.texture_upload_count;
      SetStorageFaultAndWait(ctx, 1);
      const std::string png_a = GuiTestTempPath("lumice_preview_texture_allsky_target_oom_a.png").string();
      std::remove(png_a.c_str());
      IM_CHECK(!ExportDualFisheyeAndWait(ctx, png_a));
      IM_CHECK(!std::filesystem::exists(png_a));
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kW);  // rebuilt in full from the file source
      IM_CHECK(gui::g_preview.GetTextureMode() == gui::PreviewRenderer::TextureMode::kXyz);
      IM_CHECK_EQ(gui::g_state.texture_upload_count, count_before + 1);
      IM_CHECK_EQ(gui::PreviewRecoveryNoticeKind(), 0);
      {
        const gui::PreviewUploadIncident incident = gui::GetPreviewUploadIncident();
        IM_CHECK(incident.has_incident);
        IM_CHECK(incident.stage == static_cast<int>(gui::UploadResult::Stage::kRenderTargetAllocation));
        IM_CHECK_EQ(incident.request_width, kW);
        IM_CHECK_EQ(incident.request_height, kH);
      }

      // Equirect, same story: a second fault, a second revoke and rebuild.
      count_before = gui::g_state.texture_upload_count;
      SetStorageFaultAndWait(ctx, 1);
      const std::string png_b = GuiTestTempPath("lumice_preview_texture_allsky_target_oom_b.png").string();
      std::remove(png_b.c_str());
      IM_CHECK(!ExportEquirectAndWait(ctx, png_b));
      IM_CHECK(!std::filesystem::exists(png_b));
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kW);
      IM_CHECK_EQ(gui::g_state.texture_upload_count, count_before + 1);
      IM_CHECK_EQ(gui::PreviewRecoveryNoticeKind(), 0);
      {
        // The newer trigger legitimately supersedes the older one (same shape here).
        const gui::PreviewUploadIncident incident = gui::GetPreviewUploadIncident();
        IM_CHECK(incident.has_incident);
        IM_CHECK(incident.stage == static_cast<int>(gui::UploadResult::Stage::kRenderTargetAllocation));
        IM_CHECK_EQ(incident.request_width, kW);
        IM_CHECK_EQ(incident.request_height, kH);
      }

      std::remove(path.c_str());
      std::remove(png_a.c_str());
      std::remove(png_b.c_str());
    };
  }

  // The degraded export's transient full-source renderer must be DESTROYED before the rebuild
  // cycle walks (the r4 release-timing ruling): its full-size source texture is exactly the
  // dead storage the recovery ladder must not carry. The observer hook captures the live
  // instance count at every kTexImage, so the pair (transient alive at its own upload, gone at
  // the rebuild's first upload) is read straight off the interleaving.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture",
                                    "a_degraded_export_destroys_its_transient_renderer_before_rebuilding");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, -1, 0);
      constexpr int kW = 256;
      constexpr int kH = 128;
      const std::vector<float> baked = XyzCoordinatePattern(kW, kH, /*seed=*/44);
      const std::string path = GuiTestTempPath("lumice_preview_texture_degraded_target_oom.lmc").string();
      gui::XyzTextureMeta meta;
      meta.snapshot_intensity = 7.0f;
      IM_CHECK(WriteXyzLmcFile(path, baked, kW, kH, meta));

      // Degraded open: the two full-size candidates fail, the first prefiltered tier displays.
      SetCustomHookAndWait(ctx,
                           FailFirstNThenPass(gui::UploadResult::Stage::kTexImage, /*fail_count=*/2, kGlOutOfMemory));
      g_open_requested = true;
      g_open_done = false;
      g_open_path = path;
      YieldUntilOpenDone(ctx);
      IM_CHECK(g_open_done);
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kW / 2);
      SetFaultAndWait(ctx, -1, 0);

      const int baseline = gui::PreviewRenderer::LiveInstanceCountForTest();
      const int gl_baseline = gui::PreviewRenderer::LiveGlObjectCountForTest();
      const unsigned long long count_before = gui::g_state.texture_upload_count;
      SetAllHookAndWait(ctx, MakeOrderObserverHook());
      SetStorageFaultAndWait(ctx, 1);
      const std::string png = GuiTestTempPath("lumice_preview_texture_degraded_target_oom.png").string();
      std::remove(png.c_str());
      IM_CHECK(!ExportDualFisheyeAndWait(ctx, png));
      IM_CHECK(!std::filesystem::exists(png));

      // ORDER: every kTexImage before the first baseline-counted call ran while the transient
      // was alive (its Init blank and its source upload); the first baseline-counted call is
      // the rebuild's first upload — the transient was already destroyed there.
      IM_CHECK(g_order_capture.calls >= 3);
      int rebuild_index = -1;
      int prefix_mismatch = 0;
      for (int i = 0; i < g_order_capture.calls.load(); ++i) {
        if (rebuild_index < 0) {
          if (g_order_capture.instances[i] == baseline) {
            rebuild_index = i;
          } else if (g_order_capture.instances[i] != baseline + 1) {
            ++prefix_mismatch;
          }
        }
      }
      // Asserts outside the scan loop (loop-fatal-assert rule): every capture before the first
      // baseline-counted call ran with the transient alive (baseline + 1), and the first
      // baseline-counted call is the rebuild's first upload — the transient was gone there.
      IM_CHECK(rebuild_index >= 2);
      IM_CHECK_EQ(prefix_mismatch, 0);
      // The rebuild re-established tier 0 from the file source, and the tier-0 path lazily
      // re-allocated its slot.
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kW);
      IM_CHECK(gui::g_preview.StagingSlotByteCountForTest(0) > 0);
      IM_CHECK_EQ(gui::g_state.texture_upload_count, count_before + 1);
      IM_CHECK_EQ(gui::PreviewRecoveryNoticeKind(), 0);
      // GL-object accounting, the r2-Major instrument: the transient's six objects are back
      // out of the paired counter (early Destroy), and the only net addition is the rebuild's
      // lazily re-allocated tier-0 slot.
      IM_CHECK_EQ(gui::PreviewRenderer::LiveGlObjectCountForTest(), gl_baseline + 1);
      // And the incident record carries the TARGET allocation that triggered the revoke.
      {
        const gui::PreviewUploadIncident incident = gui::GetPreviewUploadIncident();
        IM_CHECK(incident.has_incident);
        IM_CHECK(incident.stage == static_cast<int>(gui::UploadResult::Stage::kRenderTargetAllocation));
        IM_CHECK_EQ(incident.request_width, kW);
        IM_CHECK_EQ(incident.request_height, kH);
      }

      SetAllHookAndWait(ctx, nullptr);
      std::remove(path.c_str());
      std::remove(png.c_str());
    };
  }

  // An accepted composite live frame must rebuild as ITS composite (the r4 defect: the rebuild
  // walked the XYZ ladder because the payload also carries spectrum floats, leaving a full-
  // spectrum GPU picture under a composite mode record the terminal frame's sync would never
  // correct). The commit is read INSIDE the export request, before any sync can re-fire.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture",
                                    "a_composite_display_rebuilds_as_its_composite_after_export_target_oom");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedRunServer scene;
      IM_CHECK(scene.ok());
      SetFaultAndWait(ctx, -1, 0);
      gui::g_state.raypath_color.push_back(MakeColorMatchAllClass(1.0f, 0.0f, 0.0f, /*z_order=*/0));
      gui::g_state.show_composite_preview = true;
      gui::g_state.renderer.display_mode = LUMICE_DISPLAY_MODE_NORMAL;

      SeedFiniteRun(/*millions=*/0.1f);
      gui::DoRun(/*user_initiated=*/true);
      IM_CHECK(WaitForSimRestartAtLeast(ctx, /*baseline_upload_count=*/0, /*timeout_ms=*/20000));
      IM_CHECK(gui::g_state.last_uploaded_as_composite);
      IM_CHECK(gui::g_preview.GetTextureMode() == gui::PreviewRenderer::TextureMode::kSrgbRadiance);
      IM_CHECK(DriveUntil(
          ctx,
          [] {
            return gui::g_state.sim_state == SimState::kDone && gui::g_state.run_intent == RunIntent::kRunCompleted;
          },
          20));
      // Arm the fault FIRST, then settle INSIDE the armed window: the terminal frame's own
      // acceptance lags kDone by machine timing, and the baselines below must describe a
      // quiescent document or the commit assertions would race it. The fault only consumes at
      // renderbuffer storage — a poll sync never touches it.
      SetStorageFaultAndWait(ctx, 1);
      unsigned long long settled_count = 0;
      IM_CHECK(DriveUntil(
          ctx,
          [&settled_count] {
            const unsigned long long now = gui::g_state.texture_upload_count;
            if (now != settled_count && now > 0) {
              settled_count = now;
              return false;
            }
            return now > 0;
          },
          10));

      const unsigned long long serial_before = gui::g_state.last_uploaded_texture_serial;
      const unsigned long long count_before = gui::g_state.texture_upload_count;
      const int disp_w_before = gui::g_preview.GetTextureWidth();
      const int disp_h_before = gui::g_preview.GetTextureHeight();

      const std::string png = GuiTestTempPath("lumice_preview_texture_composite_target_oom.png").string();
      std::remove(png.c_str());
      IM_CHECK(!ExportDualFisheyeAndWait(ctx, png));
      IM_CHECK(!std::filesystem::exists(png));
      IM_CHECK_EQ(g_fault.dfe_snap_tex_mode.load(), static_cast<int>(gui::PreviewRenderer::TextureMode::kSrgbRadiance));
      IM_CHECK_EQ(g_fault.dfe_snap_as_composite.load(), 1);
      IM_CHECK_EQ(g_fault.dfe_snap_serial.load(), serial_before);  // the same accepted frame
      IM_CHECK_EQ(g_fault.dfe_snap_upload_count.load(), count_before + 1);
      IM_CHECK_EQ(g_fault.dfe_snap_notice_kind.load(), 0);
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), disp_w_before);
      IM_CHECK_EQ(gui::g_preview.GetTextureHeight(), disp_h_before);

      // Frozen terminal frame: the settled syncs must not re-upload — the mode record already
      // matches the effective preference, so nothing corrects or re-fires.
      ctx->Yield(10);
      const unsigned long long settled = gui::g_state.texture_upload_count;
      ctx->Yield(5);
      IM_CHECK_EQ(gui::g_state.texture_upload_count, settled);
      IM_CHECK(gui::g_preview.GetTextureMode() == gui::PreviewRenderer::TextureMode::kSrgbRadiance);
      IM_CHECK(gui::g_state.last_uploaded_as_composite);
    };
  }

  // The two baked uint8 modes rebuild in their OWN modes after an export target OOM (the r4
  // gap: the rebuild ignored them entirely — display revoked, no unavailable state, no retry
  // exit). Third leg: when the rebuild itself cannot land, the explicit unavailable state stands
  // with a real incident and the retry exit open, and the retry walks the document's own source.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_texture", "a_uint8_file_rebuilds_in_its_own_mode_after_export_target_oom");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, -1, 0);
      constexpr int kW = 256;
      constexpr int kH = 128;

      // Radiance leg.
      const std::vector<unsigned char> rad = SolidPattern(kW, kH, 200, 60, 30);
      gui::g_preview.UpdateCpuTextureData(rad.data(), kW, kH);  // defaults to radiance-only
      const std::string rad_path = GuiTestTempPath("lumice_preview_texture_uint8_rad_rebuild.lmc").string();
      IM_CHECK(gui::SaveLmcFile(rad_path, gui::g_state, gui::g_preview, /*save_texture=*/true));
      g_open_requested = true;
      g_open_done = false;
      g_open_path = rad_path;
      YieldUntilOpenDone(ctx);
      IM_CHECK(g_open_done);
      ctx->Yield(2);
      IM_CHECK(gui::g_preview.GetTextureMode() == gui::PreviewRenderer::TextureMode::kSrgbRadiance);
      unsigned long long count_before = gui::g_state.texture_upload_count;
      SetStorageFaultAndWait(ctx, 1);
      const std::string rad_png = GuiTestTempPath("lumice_preview_texture_uint8_rad_rebuild.png").string();
      std::remove(rad_png.c_str());
      IM_CHECK(!ExportDualFisheyeAndWait(ctx, rad_png));
      IM_CHECK(!std::filesystem::exists(rad_png));
      IM_CHECK_EQ(g_fault.dfe_snap_tex_mode.load(), static_cast<int>(gui::PreviewRenderer::TextureMode::kSrgbRadiance));
      IM_CHECK_EQ(g_fault.dfe_snap_as_composite.load(), 0);
      IM_CHECK_EQ(g_fault.dfe_snap_upload_count.load(), count_before + 1);
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kW);
      SetStorageFaultAndWait(ctx, 0);

      // Composited leg: the second open resets the document wholesale.
      const std::vector<unsigned char> comp = SolidPattern(kW, kH, 30, 90, 200);
      gui::g_preview.UpdateCpuTextureData(comp.data(), kW, kH, gui::PreviewRenderer::TextureMode::kSrgbComposited);
      const std::string comp_path = GuiTestTempPath("lumice_preview_texture_uint8_comp_rebuild.lmc").string();
      IM_CHECK(gui::SaveLmcFile(comp_path, gui::g_state, gui::g_preview, /*save_texture=*/true));
      g_open_requested = true;
      g_open_done = false;
      g_open_path = comp_path;
      YieldUntilOpenDone(ctx);
      IM_CHECK(g_open_done);
      ctx->Yield(2);
      IM_CHECK(gui::g_preview.GetTextureMode() == gui::PreviewRenderer::TextureMode::kSrgbComposited);
      count_before = gui::g_state.texture_upload_count;
      SetStorageFaultAndWait(ctx, 1);
      const std::string comp_png = GuiTestTempPath("lumice_preview_texture_uint8_comp_rebuild.png").string();
      std::remove(comp_png.c_str());
      IM_CHECK(!ExportDualFisheyeAndWait(ctx, comp_png));
      IM_CHECK(!std::filesystem::exists(comp_png));
      IM_CHECK_EQ(g_fault.dfe_snap_tex_mode.load(),
                  static_cast<int>(gui::PreviewRenderer::TextureMode::kSrgbComposited));
      IM_CHECK_EQ(g_fault.dfe_snap_as_composite.load(), 1);
      IM_CHECK_EQ(g_fault.dfe_snap_upload_count.load(), count_before + 1);
      IM_CHECK(gui::g_preview.HasTexture());
      SetStorageFaultAndWait(ctx, 0);

      // Failure leg: the rebuild's own upload cannot land either. Explicit unavailable, real
      // incident, retry exit open — and the retry recovers once the pressure is gone.
      g_open_requested = true;
      g_open_done = false;
      g_open_path = comp_path;
      YieldUntilOpenDone(ctx);
      IM_CHECK(g_open_done);
      ctx->Yield(2);
      IM_CHECK(gui::g_preview.HasTexture());
      SetStorageFaultAndWait(ctx, 1);
      SetAllHookAndWait(ctx, [](gui::UploadResult::Stage st) -> int {
        return st == gui::UploadResult::Stage::kTexImage ? kGlOutOfMemory : 0;
      });
      const std::string fail_png = GuiTestTempPath("lumice_preview_texture_uint8_comp_rebuild_fail.png").string();
      std::remove(fail_png.c_str());
      IM_CHECK(!ExportDualFisheyeAndWait(ctx, fail_png));
      IM_CHECK(!std::filesystem::exists(fail_png));
      IM_CHECK(!gui::g_preview.HasTexture());
      IM_CHECK_EQ(g_fault.dfe_snap_notice_kind.load(), 2);
      IM_CHECK(gui::PreviewRecoveryRetryAvailable());
      const gui::PreviewUploadIncident incident = gui::GetPreviewUploadIncident();
      IM_CHECK(incident.has_incident);
      IM_CHECK(incident.stage == static_cast<int>(gui::UploadResult::Stage::kTexImage));
      IM_CHECK_EQ(incident.gl_error, kGlOutOfMemory);
      IM_CHECK_EQ(incident.request_width, kW);
      IM_CHECK_EQ(incident.request_height, kH);

      SetStorageFaultAndWait(ctx, 0);
      SetAllHookAndWait(ctx, nullptr);
      RetryAndWait(ctx);
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK(gui::g_preview.GetTextureMode() == gui::PreviewRenderer::TextureMode::kSrgbComposited);

      std::remove(rad_path.c_str());
      std::remove(comp_path.c_str());
      std::remove(rad_png.c_str());
      std::remove(comp_png.c_str());
      std::remove(fail_png.c_str());
    };
  }

  // The ladder retires BOTH staging slots before its first client-memory candidate (the r4
  // release-timing ruling): from tier 1 on, the walk uploads from client memory, and the other
  // slot's full-size storage is part of the allocation pressure that candidate walks against.
  // The capture reads the slot bytes at each kTexImage, so the retirement moment is witnessed
  // between the tier-0 attempt and the tier-1 attempt, not at the cycle's exit.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture",
                                    "the_ladder_retires_both_staging_slots_before_its_first_client_memory_candidate");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, -1, 0);
      // The strip cap: with the production cut this 256x128 frame is ONE strip and only slot 0
      // ever allocates — the ping-pong needs multiple strips per attempt to involve both slots,
      // which is the shape this witness is about (a slot the tier walk retires must be one the
      // staged path actually held). 2-row strips make the open's tier-0 attempt a 64-strip cut.
      SetStripRowsAndWait(ctx, 2);
      constexpr int kW = 256;
      constexpr int kH = 128;
      const std::vector<float> baked = XyzCoordinatePattern(kW, kH, /*seed=*/42);
      const std::string path = GuiTestTempPath("lumice_preview_texture_slot_retirement.lmc").string();
      gui::XyzTextureMeta meta;
      meta.snapshot_intensity = 7.0f;
      IM_CHECK(WriteXyzLmcFile(path, baked, kW, kH, meta));

      // Two healthy opens = two tier-0 uploads = both staging slots hold storage (unrolled —
      // the open channel's IM_CHECKs cannot sit in a loop body, loop-fatal-assert rule).
      g_open_requested = true;
      g_open_done = false;
      g_open_path = path;
      YieldUntilOpenDone(ctx);
      IM_CHECK(g_open_done);
      ctx->Yield(2);
      g_open_requested = true;
      g_open_done = false;
      g_open_path = path;
      YieldUntilOpenDone(ctx);
      IM_CHECK(g_open_done);
      ctx->Yield(2);
      IM_CHECK(gui::g_preview.StagingSlotByteCountForTest(0) > 0);
      IM_CHECK(gui::g_preview.StagingSlotByteCountForTest(1) > 0);

      // The third open walks the ladder into a wall: every candidate's kTexImage is captured
      // and forced to fail. The walk's tier-0 attempt still has both slots; by the time the
      // FIRST client-memory candidate fires, both slots must already be retired.
      SetAllHookAndWait(ctx, MakeOrderCaptureFailHook(kGlOutOfMemory));
      g_open_requested = true;
      g_open_done = false;
      g_open_path = path;
      YieldUntilOpenDone(ctx);
      IM_CHECK(g_open_done);
      ctx->Yield(2);
      IM_CHECK(g_order_capture.calls >= 3);
      IM_CHECK(g_order_capture.slot0[0] > 0);  // the tier-0 attempt's own slot, re-armed
      IM_CHECK(g_order_capture.slot1[0] > 0);  // the OTHER slot, still held from the two uploads
      IM_CHECK_EQ(g_order_capture.slot0[1].load(), 0ull);
      IM_CHECK_EQ(g_order_capture.slot1[1].load(), 0ull);  // retired BEFORE the client-memory candidate
      IM_CHECK(!gui::g_preview.HasTexture());
      IM_CHECK_EQ(gui::PreviewRecoveryNoticeKind(), 2);
      IM_CHECK(gui::PreviewRecoveryRetryAvailable());

      SetAllHookAndWait(ctx, nullptr);
      std::remove(path.c_str());
    };
  }

  // The transient export renderer's GL storage is released on EVERY exit, verified by the
  // paired create/delete counter — not the C++ instance count (the r2 Major: scope exit alone
  // never deletes GL objects, so a successful degraded export used to leave a full-size source
  // texture behind, one set per export, and the instance counter read as if clean). Two
  // consecutive successful exports must leave the count at baseline; the ordinary-failure exit
  // must too, with the display untouched.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_texture", "a_successful_degraded_export_leaves_no_gl_objects_behind");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, -1, 0);
      constexpr int kW = 256;
      constexpr int kH = 128;
      const std::vector<float> baked = XyzCoordinatePattern(kW, kH, /*seed=*/45);
      const std::string path = GuiTestTempPath("lumice_preview_texture_export_gl_release.lmc").string();
      gui::XyzTextureMeta meta;
      meta.snapshot_intensity = 7.0f;
      IM_CHECK(WriteXyzLmcFile(path, baked, kW, kH, meta));

      // Degraded open: the two full-size candidates fail, the first prefiltered tier displays.
      SetCustomHookAndWait(ctx,
                           FailFirstNThenPass(gui::UploadResult::Stage::kTexImage, /*fail_count=*/2, kGlOutOfMemory));
      g_open_requested = true;
      g_open_done = false;
      g_open_path = path;
      YieldUntilOpenDone(ctx);
      IM_CHECK(g_open_done);
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kW / 2);
      SetFaultAndWait(ctx, -1, 0);

      const int gl_baseline = gui::PreviewRenderer::LiveGlObjectCountForTest();
      const std::string png_a = GuiTestTempPath("lumice_preview_texture_export_gl_release_a.png").string();
      std::remove(png_a.c_str());
      IM_CHECK(ExportDualFisheyeAndWait(ctx, png_a));  // the SUCCESS route, storage fault off
      IM_CHECK(std::filesystem::exists(png_a));
      IM_CHECK_EQ(gui::PreviewRenderer::LiveGlObjectCountForTest(), gl_baseline);

      // Again: no accumulation across repeated successful exports.
      const std::string png_b = GuiTestTempPath("lumice_preview_texture_export_gl_release_b.png").string();
      std::remove(png_b.c_str());
      IM_CHECK(ExportDualFisheyeAndWait(ctx, png_b));
      IM_CHECK(std::filesystem::exists(png_b));
      IM_CHECK_EQ(gui::PreviewRenderer::LiveGlObjectCountForTest(), gl_baseline);

      // The ordinary-failure exit (the transient's source upload fails with a non-OOM — armed
      // through the ALL hook: the per-instance hook cannot reach the transient): no revoke, no
      // rebuild, explicit export failure — and the transient's storage still all released.
      SetAllHookAndWait(ctx, [](gui::UploadResult::Stage st) -> int {
        return st == gui::UploadResult::Stage::kTexImage ? kGlInvalidValue : 0;
      });
      const std::string png_c = GuiTestTempPath("lumice_preview_texture_export_gl_release_c.png").string();
      std::remove(png_c.c_str());
      IM_CHECK(!ExportDualFisheyeAndWait(ctx, png_c));
      IM_CHECK(!std::filesystem::exists(png_c));
      IM_CHECK(gui::g_preview.HasTexture());  // ordinary error: the degraded picture stands
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kW / 2);
      IM_CHECK_EQ(gui::PreviewRenderer::LiveGlObjectCountForTest(), gl_baseline);
      SetAllHookAndWait(ctx, nullptr);

      std::remove(path.c_str());
      std::remove(png_a.c_str());
      std::remove(png_b.c_str());
      std::remove(png_c.c_str());
    };
  }

  // A host allocation failure inside the recovery ladder is a REPORTED stage, not a process
  // exit: the bounded candidate's buffers throw, the ladder converts the throw, the cycle
  // exhausts into the explicit unavailable state with the source intact — and a retry after the
  // machine eased up recovers in full.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture",
                                    "an_allocation_failure_in_the_ladder_lands_in_the_unavailable_state");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, -1, 0);
      constexpr int kW = 256;
      constexpr int kH = 128;
      const std::vector<float> baked = XyzCoordinatePattern(kW, kH, /*seed=*/39);
      const std::string path = GuiTestTempPath("lumice_preview_texture_alloc_fail.lmc").string();
      gui::XyzTextureMeta meta;
      meta.snapshot_intensity = 7.0f;
      IM_CHECK(WriteXyzLmcFile(path, baked, kW, kH, meta));

      // The two full-size candidates fail; the prefiltered candidate's host allocation throws.
      SetCustomHookAndWait(ctx,
                           FailFirstNThenPass(gui::UploadResult::Stage::kTexImage, /*fail_count=*/2, kGlOutOfMemory));
      lumice::gui::SetDowngradeAllocFaultForTest(/*remaining_failures=*/1);
      g_open_requested = true;
      g_open_done = false;
      g_open_path = path;
      YieldUntilOpenDone(ctx);
      IM_CHECK(g_open_done);
      ctx->Yield(2);
      lumice::gui::SetDowngradeAllocFaultForTest(/*remaining_failures=*/0);

      IM_CHECK(!gui::g_preview.HasTexture());
      IM_CHECK(gui::g_preview.HasCpuTexture());
      IM_CHECK_EQ(gui::PreviewRecoveryNoticeKind(), 2);
      IM_CHECK(gui::PreviewRecoveryRetryAvailable());
      // The incident stays the cycle's REAL first failure (the prefiltered candidate's alloc
      // failure was the LAST candidate's stage; the throw having become a reported, survived
      // failure is witnessed by the process reaching these asserts with the explicit
      // unavailable state standing).
      const gui::PreviewUploadIncident incident = gui::GetPreviewUploadIncident();
      IM_CHECK(incident.has_incident);
      IM_CHECK(incident.stage == static_cast<int>(gui::UploadResult::Stage::kTexImage));
      IM_CHECK_EQ(incident.gl_error, kGlOutOfMemory);

      // The machine eased up: the same retained source recovers the display in full, and the
      // document still saves its own complete bytes.
      SetFaultAndWait(ctx, -1, 0);
      RetryAndWait(ctx);
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kW);
      const std::string saved = GuiTestTempPath("lumice_preview_texture_alloc_fail_saved.lmc").string();
      IM_CHECK(gui::SaveLmcFile(saved, gui::g_state, gui::g_preview, /*save_texture=*/true));
      {
        gui::GuiState scratch;
        gui::LmcTexture tex;
        IM_CHECK(gui::LoadLmcFile(saved, scratch, tex));
        IM_CHECK_EQ(tex.width, kW);
        IM_CHECK_EQ(tex.height, kH);
      }

      std::remove(path.c_str());
      std::remove(saved.c_str());
    };
  }

  // A successful acceptance ends the exhausted situation it describes being over: a generation
  // whose every candidate failed, followed by a Continue re-commit (a new epoch that un-latches
  // the old one), must leave the notice gone and the retry exit closed once a real frame is
  // accepted — the r5 defect had the panel reporting "unavailable" under a full-size picture.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture", "an_accepted_frame_clears_the_exhausted_state");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedRunServer scene;
      IM_CHECK(scene.ok());
      SetFaultAndWait(ctx, -1, 0);
      SetFaultAndWait(ctx, static_cast<int>(gui::UploadResult::Stage::kTexImage), kGlInvalidValue);
      SeedFiniteRun(/*millions=*/0.1f);
      gui::DoRun(/*user_initiated=*/true);
      IM_CHECK(DriveUntil(ctx, [] { return gui::g_state.upload_latched; }, 20));
      IM_CHECK_EQ(gui::PreviewRecoveryNoticeKind(), 2);
      IM_CHECK(DriveUntil(
          ctx,
          [] {
            return gui::g_state.sim_state == SimState::kDone && gui::g_state.run_intent == RunIntent::kRunCompleted;
          },
          20));

      // Continue re-commits the same document under a new epoch; the uploads succeed now.
      SetFaultAndWait(ctx, -1, 0);
      IM_CHECK(gui::DoContinue());
      IM_CHECK(DriveUntil(
          ctx,
          [] {
            return gui::g_state.sim_state == SimState::kDone && gui::g_state.run_intent == RunIntent::kRunCompleted;
          },
          30));
      IM_CHECK(WaitForSimRestartAtLeast(ctx, /*baseline_upload_count=*/0, /*timeout_ms=*/20000));
      ctx->Yield(2);
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), 1024);
      IM_CHECK_EQ(gui::g_preview.GetTextureHeight(), 512);
      IM_CHECK_EQ(gui::PreviewRecoveryNoticeKind(), 0);
      IM_CHECK(!gui::PreviewRecoveryRetryAvailable());
    };
  }

  // Init's partial-failure exits reclaim what they created: a renderer whose first Init fails
  // mid-shader-compile leaves a context the next Init builds a fully working renderer in — the
  // transient full-source export renderer can fail init repeatedly inside a live application,
  // and every leaked shader or program would stay in a context that keeps running.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture", "a_failed_init_leaves_a_buildable_context");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, -1, 0);

      gui::PreviewRenderer::SetShaderCompileFaultForTest(/*remaining_failures=*/1);
      const InitProbeResult failed = RunInitProbe(ctx);
      IM_CHECK(failed.ran);
      IM_CHECK(!failed.init_ok);

      gui::PreviewRenderer::SetShaderCompileFaultForTest(/*remaining_failures=*/0);
      const InitProbeResult recovered = RunInitProbe(ctx);
      IM_CHECK(recovered.ran);
      IM_CHECK(recovered.init_ok);
      IM_CHECK(recovered.upload_ok);
      IM_CHECK(recovered.has_texture);
    };
  }

  // The strip staging cut delivers the SAME picture the single-shot cut does, through multiple
  // slots, with one scale per attempt. The witness is the on-screen frame (the whole live
  // viewport, byte for byte), not the mirror — the proposition is about GL texels, so the
  // comparison arms differ ONLY in how many strips the upload was cut into. The bounded-staging
  // witness rides along: with the cap at 2 rows a 16x10 frame is 5 strips, both slots carry a
  // strip-sized store, and neither can hold the whole frame.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture", "a_multi_strip_cut_lands_the_frame_the_single_cut_does");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, -1, 0);
      constexpr int kW = 16;
      constexpr int kH = 10;
      const std::vector<float> frame = XyzCoordinatePattern(kW, kH, /*seed=*/41);

      // Arm 1: the production cut of this small frame is one strip (the budget rule caps at
      // 2 MiB of rows, far above what 10 rows hold). The upload rides the exposure premise: a
      // positive intensity_scale and a pattern that actually reaches the pixels.
      SetStripRowsAndWait(ctx, 0);
      ExpectVisibleExposurePremise(ctx, frame, kW, kH);
      const float single_scale = gui::g_preview.GetXyzTextureScale();
      const std::vector<unsigned char> single_frame = CapturePreviewFullFrame(ctx);
      IM_CHECK(!single_frame.empty());

      // The witness's teeth: a DIFFERENT pattern must produce a DIFFERENT picture (a capture
      // pipeline that showed nothing would pass every equality below).
      std::vector<float> other(frame.size());
      for (size_t i = 0; i < other.size(); ++i) {
        other[i] = -frame[i];
      }
      UploadXyzAndWait(ctx, other, kW, kH);
      IM_CHECK(g_fault.uploadxyz_result.ok);
      const std::vector<unsigned char> other_frame = CapturePreviewFullFrame(ctx);
      IM_CHECK_EQ(other_frame.size(), single_frame.size());
      if (std::memcmp(other_frame.data(), single_frame.data(), other_frame.size()) == 0) {
        IM_ERRORF("%s", "witness premise broken: a different pattern rendered an identical picture");
      }

      // Arm 2: the same frame cut into 5 strips of 2 rows — 4 slot handoffs, a fence wait on
      // every strip from the third one on, and a full (2-row) tail never even reached.
      SetStripRowsAndWait(ctx, 2);
      gui::g_preview.ClearTexture();
      ctx->Yield(2);
      UploadXyzAndWait(ctx, frame, kW, kH);
      IM_CHECK(g_fault.uploadxyz_result.ok);
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kW);
      const std::vector<unsigned char> multi_frame = CapturePreviewFullFrame(ctx);
      IM_CHECK_EQ(multi_frame.size(), single_frame.size());
      if (std::memcmp(multi_frame.data(), single_frame.data(), multi_frame.size()) != 0) {
        IM_ERRORF("%s", "the strip cut changed the picture");
      }
      IM_CHECK_EQ(gui::g_preview.GetXyzTextureScale(), single_scale);

      // Directed red-state: strip 1's rows replaced by strip 0's — the wrong-row defect a broken
      // strip hand would produce — must be REJECTED by this same witness. If the equality above
      // could hold for this frame, it would hold below too; it must not.
      // The wrong-content defect shape: every strip after the first carries strip 0's rows —
      // what a stuck slot ping-pong would deliver. (A two-row displacement was tried first and
      // proved display-invariant: the default preview's sampled source region does not include
      // that band, which is itself a useful fact about screen-level witnessing — a red-state
      // must live where the view actually samples.) 8 of the 10 rows change, unambiguously.
      std::vector<float> wrong(frame);
      for (int strip = 1; strip < 5; ++strip) {
        std::copy_n(frame.begin(), 2 * kW * 3, wrong.begin() + strip * 2 * kW * 3);
      }
      {
        const size_t differing =
            static_cast<size_t>(std::mismatch(wrong.begin(), wrong.end(), frame.begin()).first - wrong.begin());
        if (differing == wrong.size()) {
          IM_ERRORF("%s", "red-state construction broken: the wrong-content frame equals the correct frame");
        }
      }
      UploadXyzAndWait(ctx, wrong, kW, kH);
      IM_CHECK(g_fault.uploadxyz_result.ok);
      const std::vector<unsigned char> wrong_frame = CapturePreviewFullFrame(ctx);
      IM_CHECK_EQ(wrong_frame.size(), multi_frame.size());
      if (std::memcmp(wrong_frame.data(), multi_frame.data(), wrong_frame.size()) == 0) {
        // Temporary red-state forensics: dump both captures for eyeballing.
        gui::WriteRgbaBufferToPng(GuiTestTempPath("redstate_multi.png").string(), gui::g_preview_vp.vp_w,
                                  gui::g_preview_vp.vp_h, multi_frame);
        gui::WriteRgbaBufferToPng(GuiTestTempPath("redstate_wrong.png").string(), gui::g_preview_vp.vp_w,
                                  gui::g_preview_vp.vp_h, wrong_frame);
        IM_ERRORF("%s", "red-state premise broken: a wrong-row frame rendered identically to the correct one");
      }

      // The slots: both ping-pong slots carry a strip-sized store, each within the budget, and
      // each holding a fraction of the frame (a slot that secretly held the whole frame would
      // fail the fraction check, not just the budget one).
      const size_t slot0 = gui::g_preview.StagingSlotByteCountForTest(0);
      const size_t slot1 = gui::g_preview.StagingSlotByteCountForTest(1);
      IM_CHECK(slot0 > 0);
      if (!(slot1 > 0)) {
        IM_ERRORF("%s", "a 5-strip attempt writes slots 0 and 1 in turn; both must be allocated");
      }
      IM_CHECK(slot0 <= gui::kXyzStagingSlotTargetBytes);
      IM_CHECK(slot1 <= gui::kXyzStagingSlotTargetBytes);
      const size_t frame_bytes = static_cast<size_t>(kW) * kH * 3 * sizeof(uint16_t);
      if (!(slot0 < frame_bytes)) {
        IM_ERRORF("%s", "a staging slot is a fraction of the frame, not a second full-size copy");
      }
      IM_CHECK(slot1 < frame_bytes);

      // A second frame of the same shape, brighter: its own scale ruling (the scale read moves),
      // and the same cut-invariance proposition at that scale. (The on-screen picture is
      // deliberately NOT required to differ from the first frame's: the relative-EV shader
      // branch and the display clamp swallow a uniform gain, so a visual inequality would
      // assert nothing — the scale field and the texel-identical comparison are the witnesses.)
      std::vector<float> brighter(frame.size());
      for (size_t i = 0; i < frame.size(); ++i) {
        brighter[i] = frame[i] * 100.0f;
      }
      SetStripRowsAndWait(ctx, 0);
      UploadXyzAndWait(ctx, brighter, kW, kH);
      IM_CHECK(g_fault.uploadxyz_result.ok);
      const float brighter_single_scale = gui::g_preview.GetXyzTextureScale();
      if (!(brighter_single_scale != single_scale)) {
        IM_ERRORF("%s", "each attempt rules its own scale");
      }
      const std::vector<unsigned char> brighter_single = CapturePreviewFullFrame(ctx);
      IM_CHECK(!brighter_single.empty());
      // The multi arm re-arms the 2-row cap: a multi-strip attempt on a frame of a DIFFERENT
      // scale (this one), reached by an update from the previous frame — the scale-change
      // witness rides the same cut the first arm exercised.
      SetStripRowsAndWait(ctx, 2);
      UploadXyzAndWait(ctx, brighter, kW, kH);
      IM_CHECK(g_fault.uploadxyz_result.ok);
      IM_CHECK_EQ(gui::g_preview.GetXyzTextureScale(), brighter_single_scale);
      const std::vector<unsigned char> brighter_multi = CapturePreviewFullFrame(ctx);
      IM_CHECK_EQ(brighter_multi.size(), brighter_single.size());
      if (std::memcmp(brighter_multi.data(), brighter_single.data(), brighter_multi.size()) != 0) {
        IM_ERRORF("%s", "the strip cut changed the brighter frame's picture");
      }
      IM_CHECK(gui::g_preview.StagingSlotByteCountForTest(0) > 0);
      if (!(gui::g_preview.StagingSlotByteCountForTest(1) > 0)) {
        IM_ERRORF("%s", "the brighter frame's multi-strip attempt wrote both slots");
      }

      SetStripRowsAndWait(ctx, 0);
      SetFaultAndWait(ctx, -1, 0);
    };
  }

  // Odd width and the tail strip: 15x9 at 2 rows per strip is 2,2,2,2,1 — a one-row tail and
  // rows that are 90 bytes (even but not 4-aligned; the unpack alignment is 2). The picture must
  // equal the single-strip cut of the same frame: neither the tail's shorter read nor the
  // alignment may skew texels (the quantization-skew class the timing probes hit).
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture", "an_odd_width_multi_strip_cut_lands_its_tail_unchanged");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, -1, 0);
      constexpr int kW = 15;
      constexpr int kH = 9;
      const std::vector<float> frame = XyzCoordinatePattern(kW, kH, /*seed=*/43);

      SetStripRowsAndWait(ctx, 0);
      ExpectVisibleExposurePremise(ctx, frame, kW, kH);
      const std::vector<unsigned char> single_frame = CapturePreviewFullFrame(ctx);
      IM_CHECK(!single_frame.empty());

      SetStripRowsAndWait(ctx, 2);
      gui::g_preview.ClearTexture();
      ctx->Yield(2);
      UploadXyzAndWait(ctx, frame, kW, kH);
      IM_CHECK(g_fault.uploadxyz_result.ok);
      const std::vector<unsigned char> multi_frame = CapturePreviewFullFrame(ctx);
      IM_CHECK_EQ(multi_frame.size(), single_frame.size());
      if (std::memcmp(multi_frame.data(), single_frame.data(), multi_frame.size()) != 0) {
        IM_ERRORF("%s", "the odd-width strip cut (tail strip included) changed the picture");
      }

      SetStripRowsAndWait(ctx, 0);
      SetFaultAndWait(ctx, -1, 0);
    };
  }

  // The frame-level acceptance boundary: a failure AFTER the attempt has written strips must not
  // claim the old frame retained — the texture is a mix of the old frame and this attempt's
  // strips, and the ruling is the conservative revoke. The retained variant (failure BEFORE any
  // write of the attempt) is the contrast arm: the old frame stands, displayable. Both arms ride
  // the same counting hook at two different consult indices of the same stage.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_texture", "a_strip_failure_after_partial_writes_revokes_not_retains");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, -1, 0);
      SetStripRowsAndWait(ctx, 2);
      constexpr int kW = 8;
      constexpr int kH = 8;  // 4 strips of 2 rows
      const std::vector<float> first = XyzCoordinatePattern(kW, kH, /*seed=*/47);
      UploadXyzAndWait(ctx, first, kW, kH);
      IM_CHECK(g_fault.uploadxyz_result.ok);
      const unsigned long long first_width = static_cast<unsigned long long>(gui::g_preview.GetTextureWidth());

      // Arm 1 — ordinary kTexUpdate error at strip 2 (the THIRD consult; strips 0 and 1 of this
      // attempt are already in the texture), so the old frame is provably NOT intact. Revoke.
      SetCustomHookAndWait(ctx, FailOnlyNthConsult(gui::UploadResult::Stage::kTexUpdate, /*n=*/3, kGlInvalidValue));
      const std::vector<float> second = XyzCoordinatePattern(kW, kH, /*seed=*/48);
      UploadXyzAndWait(ctx, second, kW, kH);
      SetFaultAndWait(ctx, -1, 0);
      IM_CHECK(!g_fault.uploadxyz_result.ok);
      IM_CHECK(g_fault.uploadxyz_result.stage == gui::UploadResult::Stage::kTexUpdate);
      IM_CHECK(g_fault.uploadxyz_result.gl_error == kGlInvalidValue);
      if (!(!g_fault.uploadxyz_result.old_frame_retained)) {
        IM_ERRORF("%s", "two strips of the new frame are in the texture; the old frame cannot be called retained");
      }
      if (!(!gui::g_preview.HasTexture())) {
        IM_ERRORF("%s", "the mixed candidate was revoked, not displayed");
      }

      // Arm 2 — fence-wait failure at strip 2 (the third wait consult; the first two waits guard
      // the PREVIOUS accepted attempt's last two strips and pass, the third guards THIS attempt's
      // own strip 0 with strips 0 and 1 already written): the same shape one stage earlier.
      UploadXyzAndWait(ctx, first, kW, kH);
      IM_CHECK(g_fault.uploadxyz_result.ok);
      SetCustomHookAndWait(ctx, FailOnlyNthConsult(gui::UploadResult::Stage::kFenceWait, /*n=*/3, kGlWaitFailed));
      UploadXyzAndWait(ctx, second, kW, kH);
      SetFaultAndWait(ctx, -1, 0);
      IM_CHECK(!g_fault.uploadxyz_result.ok);
      IM_CHECK(g_fault.uploadxyz_result.stage == gui::UploadResult::Stage::kFenceWait);
      IM_CHECK(!g_fault.uploadxyz_result.old_frame_retained);
      IM_CHECK(!gui::g_preview.HasTexture());

      // Contrast arm — the SAME fence-wait failure at the FIRST consult (strip 0): this attempt
      // has written nothing, and the waited fence guarded the previous accepted frame, which
      // stands.
      UploadXyzAndWait(ctx, first, kW, kH);
      IM_CHECK(g_fault.uploadxyz_result.ok);
      SetCustomHookAndWait(ctx, FailOnlyNthConsult(gui::UploadResult::Stage::kFenceWait, /*n=*/1, kGlWaitFailed));
      UploadXyzAndWait(ctx, second, kW, kH);
      SetFaultAndWait(ctx, -1, 0);
      IM_CHECK(!g_fault.uploadxyz_result.ok);
      IM_CHECK(g_fault.uploadxyz_result.stage == gui::UploadResult::Stage::kFenceWait);
      if (!(g_fault.uploadxyz_result.old_frame_retained)) {
        IM_ERRORF("%s", "strip 0 had not written when the wait failed; the accepted frame stands");
      }
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), first_width);

      SetStripRowsAndWait(ctx, 0);
      SetFaultAndWait(ctx, -1, 0);
    };
  }

  // The half-source upload: the held bits ARE the GPU texture, bit for bit — no re-quantize, no
  // float plane. The witness arms all render the same pattern through the three routes (float
  // staged, half staged, half client-memory) and require one picture; the CPU-mirror arm saves
  // the held bits back out and requires the file to carry exactly them.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture", "a_half_source_upload_is_the_bits_it_was_handed");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, -1, 0);
      SetStripRowsAndWait(ctx, 2);  // every arm multi-strip: the representation, not the cut, is under test
      constexpr int kW = 12;
      constexpr int kH = 8;
      const std::vector<float> frame = XyzCoordinatePattern(kW, kH, /*seed=*/53);
      const size_t components = frame.size();
      const float scale = gui::ComputeXyzHalfScale(frame.data(), components);
      std::vector<uint16_t> bits(components);
      gui::QuantizeXyzToHalf(frame.data(), components, scale, bits.data());

      // Arm 1 — the float path (quantize inside the upload), under the exposure premise: the
      // half arms' equality below witnesses CONTENT only if a positive scale puts the pattern on
      // screen.
      gui::g_preview.ClearTexture();
      ctx->Yield(2);
      ExpectVisibleExposurePremise(ctx, frame, kW, kH);
      const std::vector<unsigned char> float_frame = CapturePreviewFullFrame(ctx);
      IM_CHECK(!float_frame.empty());

      // Arm 2 — the staged half path: same bits handed in directly.
      gui::g_preview.ClearTexture();
      ctx->Yield(2);
      UploadXyzHalfAndWait(ctx, bits, scale, kW, kH);
      IM_CHECK(g_fault.uploadhalf_result.ok);
      if (!((gui::g_preview.GetXyzTextureScale()) == (scale))) {
        IM_ERRORF("%s", "the held scale is the display scale, unchanged");
      }
      const std::vector<unsigned char> half_frame = CapturePreviewFullFrame(ctx);
      IM_CHECK_EQ(half_frame.size(), float_frame.size());
      if (std::memcmp(half_frame.data(), float_frame.data(), half_frame.size()) != 0) {
        IM_ERRORF("%s", "the half upload sampled different GPU texels than the float one");
      }

      // Arm 3 — the client-memory half path (recovery tier 1's representation): no slots, no
      // scratch, the strips read straight from the held bits.
      gui::g_preview.ClearTexture();
      ctx->Yield(2);
      UploadXyzHalfNoStagingAndWait(ctx, bits, scale, kW, kH);
      IM_CHECK(g_fault.uploadhalfnstaging_result.ok);
      const std::vector<unsigned char> half_client_frame = CapturePreviewFullFrame(ctx);
      IM_CHECK_EQ(half_client_frame.size(), float_frame.size());
      IM_CHECK(std::memcmp(half_client_frame.data(), float_frame.data(), half_client_frame.size()) == 0);

      // The CPU-mirror arm: the half mirror holds the bits as they came, and Save writes them
      // back out unchanged — the round trip a v6 reopen-then-save rides.
      gui::XyzTextureMeta meta;
      meta.snapshot_intensity = 3.0f;
      gui::g_preview.UpdateCpuXyzHalfTextureData(bits.data(), kW, kH, scale, meta);
      IM_CHECK(gui::g_preview.CpuXyzStorageIsHalf());
      IM_CHECK_EQ(gui::g_preview.GetXyzHalfTextureScale(), scale);
      if (!(gui::g_preview.GetXyzTextureData() == nullptr)) {
        IM_ERRORF("%s", "the float mirror is cleared, not kept alongside");
      }
      const std::string path = GuiTestTempPath("lumice_preview_texture_half_mirror.lmc").string();
      IM_CHECK(gui::SaveLmcFile(path, gui::g_state, gui::g_preview, /*save_texture=*/true));
      {
        gui::GuiState scratch;
        gui::LmcTexture tex;
        IM_CHECK(gui::LoadLmcFile(path, scratch, tex));
        IM_CHECK(tex.XyzStorageIsHalf());
        if (!((tex.xyz_half) == (bits))) {
          IM_ERRORF("%s", "the saved file holds exactly the bits the mirror held");
        }
        IM_CHECK_EQ(tex.xyz_half_scale, scale);
        IM_CHECK(tex.xyz.empty());
        IM_CHECK_EQ(tex.meta.snapshot_intensity, 3.0f);
      }
      std::remove(path.c_str());

      SetStripRowsAndWait(ctx, 0);
      SetFaultAndWait(ctx, -1, 0);
    };
  }

  // The file→accepted-live boundary: a live frame's ACCEPTANCE retires the open's file source —
  // the fact AND the mirror storage (a v6 document's retired half mirror is real memory) —
  // while a FAILED live acceptance must leave it intact, because it is still the save/retry
  // authority. Witnessed at the capacity level: a cleared-but-resident mirror passes the
  // null-pointer queries and would hide the leak.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture", "an_accepted_live_frame_retires_the_file_source");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      // ScopedRunServer resets the state itself; a SECOND ResetTestState here would run DoNew
      // against the live server and leave the subsequent Run unable to commit.
      ScopedRunServer scene;
      IM_CHECK(scene.ok());
      SetFaultAndWait(ctx, -1, 0);
      constexpr int kFW = 8;
      constexpr int kFH = 8;
      const std::vector<float> baked = XyzCoordinatePattern(kFW, kFH, /*seed=*/61);
      gui::XyzTextureMeta meta;
      meta.snapshot_intensity = 2.0f;
      const std::string path = GuiTestTempPath("lumice_preview_texture_file_source_retire.lmc").string();
      IM_CHECK(WriteXyzLmcFile(path, baked, kFW, kFH, meta));
      g_open_requested = true;
      g_open_done = false;
      g_open_path = path;
      YieldUntilOpenDone(ctx);
      IM_CHECK(g_open_done);
      ctx->Yield(2);
      IM_CHECK(gui::g_preview.HasTexture());
      if (!(gui::g_preview.CpuMirrorCapacityBytesForTest() > 0)) {
        IM_ERRORF("%s", "premise: the open mirrored the file bits");
      }

      // Arm 1 — a real run whose live frame is ACCEPTED retires the file source's storage, and
      // the live frame becomes the save source.
      SeedFiniteRun(/*millions=*/0.1f);
      gui::DoRun(/*user_initiated=*/true);
      IM_CHECK(DriveUntil(
          ctx,
          [] {
            return gui::g_state.sim_state == SimState::kDone && gui::g_state.run_intent == RunIntent::kRunCompleted;
          },
          20));
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), 1024);
      if (gui::g_preview.CpuMirrorCapacityBytesForTest() != 0u) {
        IM_ERRORF("%s", "the accepted live frame retired the file source's mirror storage");
      }
      const std::string saved = GuiTestTempPath("lumice_preview_texture_file_source_retire_saved.lmc").string();
      // The PRODUCTION save chain (PerformSave → RefreshCpuTextureForSave → SaveLmcFile): after
      // the retirement the mirror is empty until this refresh repopulates it from the accepted
      // payload — saving through the serializer alone would skip exactly the step the production
      // user takes.
      gui::g_state.current_file_path = saved;
      gui::PerformSave();
      {
        gui::GuiState scratch;
        gui::LmcTexture tex;
        IM_CHECK(gui::LoadLmcFile(saved, scratch, tex));
        if (tex.width != 1024) {
          IM_ERRORF("%s", "the save source is the accepted live frame, not the opened file");
        }
      }

      // Arm 2 — a FAILED live acceptance leaves the file source intact: its bytes are still the
      // save/retry authority. Re-opening the file is the document switch (its non-Revert reset
      // clears the latch, the pending source and the file fact), without a state reset that
      // would fight the live server.
      SetFaultAndWait(ctx, -1, 0);
      g_open_requested = true;
      g_open_done = false;
      g_open_path = path;
      YieldUntilOpenDone(ctx);
      IM_CHECK(g_open_done);
      ctx->Yield(2);
      IM_CHECK(gui::g_preview.GetTextureWidth() == kFW);
      IM_CHECK(gui::g_preview.CpuMirrorCapacityBytesForTest() > 0);
      SetFaultAndWait(ctx, static_cast<int>(gui::UploadResult::Stage::kTexImage), kGlInvalidValue);
      SeedFiniteRun(/*millions=*/0.1f);
      gui::DoRun(/*user_initiated=*/true);
      IM_CHECK(DriveUntil(
          ctx,
          [] {
            return gui::g_state.sim_state == SimState::kDone && gui::g_state.run_intent == RunIntent::kRunCompleted;
          },
          20));
      ctx->Yield(2);
      IM_CHECK(gui::g_state.upload_latched);
      if (!(gui::g_preview.CpuMirrorCapacityBytesForTest() > 0)) {
        IM_ERRORF("%s", "a failed live acceptance keeps the file source's bytes");
      }
      if (gui::PreviewSourcePixelCount() != static_cast<long>(kFW) * kFH) {
        IM_ERRORF("%s", "the resolver still names the file frame");
      }

      SetFaultAndWait(ctx, -1, 0);
      std::remove(path.c_str());
      std::remove(saved.c_str());
    };
  }
}
