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

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "gui/gui_state.hpp"
#include "test_gui_shared.hpp"

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
  gui::UploadResult upload8_result;
  gui::UploadResult uploadxyz_result;
  const std::vector<unsigned char>* srgb = nullptr;
  const std::vector<float>* xyz = nullptr;
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
    srgb = nullptr;
    xyz = nullptr;
    width = 0;
    height = 0;
  }
};

FaultUploadRequest g_fault;

// DoOpen request channel, same shape as test_file_ops' baked-preview open: the open's GL upload
// branch must run on the render thread, so the TestFunc posts a path and the GuiFunc executes it.
bool g_open_requested = false;
bool g_open_done = false;
std::string g_open_path;

// Raw GLenum spellings for the synthesized errors. Kept numeric so this TU does not need the GL
// headers: GL_OUT_OF_MEMORY (0x0505), GL_INVALID_VALUE (0x0500).
constexpr int kGlOutOfMemory = 0x0505;
constexpr int kGlInvalidValue = 0x0500;

void FaultUploadGuiFunc(ImGuiTestContext* /*ctx*/) {
  if (g_fault.hook_requested && !g_fault.hook_installed) {
    const auto stage = static_cast<gui::UploadResult::Stage>(g_fault.hook_stage.load());
    const int err = g_fault.hook_gl_error.load();
    gui::g_preview.SetUploadFaultHookForTest(
        [stage, err](gui::UploadResult::Stage s) -> int { return s == stage ? err : 0; });
    g_fault.hook_installed = true;
  }
  if (g_fault.hook_clear_requested && !g_fault.hook_cleared) {
    gui::g_preview.SetUploadFaultHookForTest(nullptr);
    g_fault.hook_cleared = true;
  }
  if (g_fault.upload8_requested && !g_fault.upload8_done) {
    g_fault.upload8_result = gui::g_preview.UploadTexture(g_fault.srgb->data(), g_fault.width, g_fault.height);
    g_fault.upload8_done = true;
  }
  if (g_fault.uploadxyz_requested && !g_fault.uploadxyz_done) {
    g_fault.uploadxyz_result = gui::g_preview.UploadXyzTexture(g_fault.xyz->data(), g_fault.width, g_fault.height);
    g_fault.uploadxyz_done = true;
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

void UploadXyzAndWait(ImGuiTestContext* ctx, const std::vector<float>& xyz, int w, int h) {
  g_fault.Reset();
  g_fault.xyz = &xyz;
  g_fault.width = w;
  g_fault.height = h;
  g_fault.uploadxyz_requested = true;
  ctx->Yield(2);
  IM_CHECK(g_fault.uploadxyz_done);
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

  // A PBO-map failure on the XYZ path happens before the texture is touched: the previously
  // displayed uint8 frame is provably untouched — retained, not revoked.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_texture", "a_pbo_map_failure_keeps_the_old_frame");
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
      IM_CHECK(g_fault.uploadxyz_result.old_frame_retained);
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), kSide);
      IM_CHECK(gui::g_preview.GetTextureMode() == gui::PreviewRenderer::TextureMode::kSrgbComposited);
      SetFaultAndWait(ctx, -1, 0);
    };
  }

  // A PBO-allocation failure must not leave the slot claiming a size it does not hold — the
  // pre-814 code registered pbo_byte_count_ before the allocation result was known. The size here
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
      IM_CHECK(g_fault.uploadxyz_result.old_frame_retained);
      IM_CHECK(!gui::g_preview.HasTexture());

      SetFaultAndWait(ctx, -1, 0);
      UploadXyzAndWait(ctx, xyz, 48, 48);
      IM_CHECK(g_fault.uploadxyz_result.ok);
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), 48);
      SetFaultAndWait(ctx, -1, 0);
    };
  }

  // A fence-create failure happens after the frame committed: the upload SUCCEEDS, and the
  // unguarded-reuse hazard is answered by tearing the slot down — observed by the next upload
  // (fresh allocation) still succeeding.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_texture", "a_fence_create_failure_commits_and_tears_down_the_slot");
    t->GuiFunc = FaultUploadGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetFaultAndWait(ctx, static_cast<int>(gui::UploadResult::Stage::kFenceCreate), kGlOutOfMemory);
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
}
