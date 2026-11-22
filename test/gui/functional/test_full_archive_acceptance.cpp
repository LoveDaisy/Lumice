// The 814 acceptance leg's full-size-archive display witness (plan §6.1/§6.3: "大存档只在必须
// 验证实际尺寸的本机/Windows 手工或专用验收腿跑").
//
// What this file is. One opt-in gui_test case that opens a REAL large .lmc (the 8192x4096
// archive behind the 4096 investigation) through the production open chain — LoadLmcFile,
// ResetFrontendState(kOpenBaked), the same upload-and-commit path a user's File>Open takes — and
// witnesses what the plan's AC6 needs: the source size reaches the display metadata intact, the
// upload succeeds on this machine's real GL, and a screenshot of the rendered preview lands on
// disk for eyeballing. Nothing here runs unless the caller points it at an archive:
//
//     gui_test --filter full_archive_acceptance   (with LUMICE_TEST_OPEN_ARCHIVE set)
//
// The path comes from an environment variable because the archive is a 114 MB working-set file
// that lives outside the repo on whatever acceptance machine runs this leg; test infrastructure
// may read env (the env-var policy exempts test/build infra), and the case simply registers as
// nothing when the variable is absent, so the suite's normal runs are untouched. There is
// deliberately no reference-image comparison here: the archive's pixel truth was established by
// the cross-machine investigation; this leg's job is "loads, displays at full size, no failure
// record", which is a state check plus a screenshot, not a tolerance question.

#include <cstdio>
#include <cstdlib>
#include <string>

#include "gui/gui_state.hpp"
#include "test_gui_shared.hpp"

namespace {

std::string ArchivePathFromEnv() {
  const char* p = std::getenv("LUMICE_TEST_OPEN_ARCHIVE");
  return (p != nullptr && *p != '\0') ? std::string(p) : std::string();
}

// Drive one GuiFunc frame that opens the archive on the render thread (the GL upload branch of
// the open must run there — the same shape test_file_ops' baked-preview open uses).
bool g_accept_open_requested = false;
bool g_accept_open_done = false;
std::string g_accept_open_path;

void AcceptanceOpenGuiFunc(ImGuiTestContext* /*ctx*/) {
  if (g_accept_open_requested && !g_accept_open_done) {
    gui::DoOpen(g_accept_open_path);
    g_accept_open_done = true;
  }
}

}  // namespace

void RegisterFullArchiveAcceptanceTests(ImGuiTestEngine* engine) {
  if (ArchivePathFromEnv().empty()) {
    return;  // No archive pointed at: this leg is opt-in, the suite stays untouched.
  }
  ImGuiTest* t = IM_REGISTER_TEST(engine, "full_archive_acceptance", "the_original_8192_archive_loads_and_displays");
  t->GuiFunc = AcceptanceOpenGuiFunc;
  t->TestFunc = [](ImGuiTestContext* ctx) {
    ResetTestState();
    // Clear any injection residue before the open (no uploads are in flight at this point, so a
    // direct assignment from the coroutine side is race-free): this leg runs the real path.
    gui::g_preview.SetUploadFaultHookForTest(nullptr);

    const std::string path = ArchivePathFromEnv();
    g_accept_open_requested = true;
    g_accept_open_done = false;
    g_accept_open_path = path;
    for (int i = 0; i < 120 && !g_accept_open_done; ++i) {
      ctx->Yield(1);  // a 114 MB decode takes frames; the bound is a safety net
    }
    IM_CHECK(g_accept_open_done);
    ctx->Yield(4);  // let the deferred preview pass render the opened frame

    // The plan's AC6 witnesses, in order: the display accepted the full-size frame (the
    // conservative revoke would have zeroed these on any upload failure); the CPU mirror holds
    // the same size (the save source's own facts); and the exposure state was committed — the
    // commit only happens on a verified upload, so these three together say "loaded and
    // displayed", not "loaded".
    IM_CHECK(gui::g_preview.HasTexture());
    IM_CHECK_EQ(gui::g_preview.GetTextureWidth(), 8192);
    IM_CHECK_EQ(gui::g_preview.GetTextureHeight(), 4096);
    IM_CHECK(gui::g_preview.HasCpuTexture());
    IM_CHECK_EQ(gui::g_preview.GetCpuTextureWidth(), 8192);
    IM_CHECK_EQ(gui::g_preview.GetCpuTextureHeight(), 4096);
    IM_CHECK(gui::g_preview.GetTextureMode() == gui::PreviewRenderer::TextureMode::kXyz);
    IM_CHECK(gui::g_state.snapshot_intensity > 0.0f);

    // The screenshot: the live preview viewport's pixels, saved next to the log for eyeballing.
    // Size tolerance ±2 px absorbs the viewport fitting the window basis of whatever session
    // runs this; the DISPLAY metadata above is the exact-size witness, this is the picture.
    const int vp_w = gui::g_preview_vp.vp_w;
    const int vp_h = gui::g_preview_vp.vp_h;
    if (gui::g_preview_vp.active && vp_w > 0 && vp_h > 0) {
      g_fullframe_capture.Reset();
      g_fullframe_capture.rect_x = gui::g_preview_vp.vp_x;
      g_fullframe_capture.rect_y = gui::g_preview_vp.vp_y;
      g_fullframe_capture.rect_w = vp_w;
      g_fullframe_capture.rect_h = vp_h;
      g_fullframe_capture.requested.store(true);
      for (int i = 0; i < 4 && !g_fullframe_capture.done.load(); ++i) {
        ctx->Yield(1);
      }
      if (g_fullframe_capture.done.load()) {
        const std::string png = GuiTestTempPath("lumice_814_full_archive_display.png").string();
        IM_CHECK(gui::WriteRgbaBufferToPng(png, g_fullframe_capture.width, g_fullframe_capture.height,
                                           g_fullframe_capture.pixels));
        // The screenshot path is the acceptance artifact; fprintf(stderr) is this suite's
        // diagnostic convention (test binaries are their own harness, outside the no-bare-print
        // rule's scope), and the path is what the operator collects.
        fprintf(stderr, "[814-acceptance] archive display screenshot: %s (%dx%d)\n", png.c_str(),
                g_fullframe_capture.width, g_fullframe_capture.height);
      }
    }
    // And the negative-space witness: no incident recorded (a failure anywhere in the open's
    // upload would have armed the incident record the recovery surface keeps).
    IM_CHECK(!gui::GetPreviewUploadIncident().has_incident);
  };
}
