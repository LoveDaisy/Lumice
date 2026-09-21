// Summary window pixel regression — a disk-reference baseline for the read-only Summary page
// (src/gui/config_summary_window.cpp): its two columns (settings left, document right), the
// label/value lines and group headings on the left, the per-layer heading, the two document
// tables (their header rows, column widths and empty cells) and the legend on the right, the
// fixed width and the height the content settles at.
//
// Why this exists beside the functional cases: those assert state and counts (the window opens,
// it draws N rows), and the content tests assert the words; none of them reads a committed image,
// so a label column that shrank, a heading that lost its rule, or an indent that vanished could
// not turn a test red — and a page whose whole job is to be screenshotted is judged on exactly
// those pixels.
//
// Capture path: the DEFAULT framebuffer through g_fullframe_capture's sub-region protocol, using
// the live ImGui window rectangle of "###ConfigSummary" — the same protocol modal_layout and
// defaults_panel_layout use, with the same consequence: the references are tied to the harness
// window size, the font atlas and the ImGui style, and any of those moving is a legitimate reason
// to re-run scripts/regen_gui_test_refs.py --group config_summary_layout.
//
// Category "config_summary_layout" doubles as the "[config_summary_layout]" tag
// CheckAgainstReference prints, which is how scripts/regen_gui_test_refs.py attributes samples to
// this group in a shared full-suite stderr. It must stay unique across groups.

#include <cmath>
#include <cstdio>
#include <string>

#include "gui/app.hpp"
#include "gui/gui_state.hpp"
#include "test_gui_shared.hpp"

namespace {

struct SummaryLayoutScene {
  const char* name;
  // Whether the scene seeds the two-layer document (a pyramid, a named raypath filter, an
  // excluded entry) or shoots the default document as New leaves it.
  bool two_layers;
};

// Both scenes are deterministic — no simulation, no RNG, no animation: the page is text laid out
// from the document, so on the reference machine they compare pixel-identical and the ruler is
// the differing-pixel one (support/pixel_diff_metrics.hpp), not a PSNR statistic.
//
// tau = 16 for the same reason the other two on-screen groups use it: it describes the
// llvmpipe-vs-Metal flat-fill quantisation noise, not the scene. K = 40 is inherited from
// defaults_panel_layout, the group this one most resembles (tables of text, no crystal preview
// and so no anti-aliased edge for a blob to survive tau=16 on) — this group does not run on the
// CI llvmpipe leg today, so K has not been measured against that residue for these scenes and
// should be when the group is added to the filter (doc/testing-architecture.md §4.6).
constexpr lumice::test::MaxCcRuler kRuler{ /*tau=*/16, /*max_cc_threshold=*/40 };
// The PSNR floor the old ruler applied; printed on the diagnostic line, not enforced.
static constexpr double kDeterministicThresholdDb = 40.0;

// clang-format off
static const SummaryLayoutScene kScenes[] = {
  {"default_document", false},
  {"two_layers",       true},
};
// clang-format on
static constexpr int kSceneCount = sizeof(kScenes) / sizeof(kScenes[0]);

// Where the window is parked before the capture (see modal_layout's note on why a position is
// pinned at all: ImGui remembers a window's position for the process, so where it sits would
// otherwise depend on which earlier case opened it). Top-left with a margin; the fixed 1200 px
// width and the 900 px height budget (config_summary_window.cpp) fit inside 1600x980 from here.
constexpr float kParkX = 20.0f;
constexpr float kParkY = 20.0f;

void SeedTwoLayerDocument() {
  gui::g_state.crystals.assign(2, gui::CrystalConfig{});
  gui::g_state.crystals[0].name = "plate";
  gui::g_state.crystals[0].zenith = gui::AxisDist{ gui::AxisDistType::kGauss, 0.0f, 1.0f };
  gui::g_state.crystals[1].type = gui::CrystalType::kPyramid;
  gui::g_state.crystals[1].face_distance[2] = gui::ShapeDist{ gui::ShapeDistType::kUniform, 0.9f, 0.1f };
  gui::FilterConfig f;
  f.name = "cza";
  f.SetRaypath(gui::RaypathParams{ "3-5-1" });
  gui::g_state.filters.assign(1, f);
  gui::Layer first;
  first.probability = 0.5f;
  gui::EntryCard a;
  a.crystal_id = 0;
  a.filter_id = 0;
  a.proportion = 60.0f;
  gui::EntryCard b;
  b.crystal_id = 1;
  b.proportion = 40.0f;
  first.entries = { a, b };
  gui::Layer second;
  gui::EntryCard c;
  c.crystal_id = 0;
  c.enabled = false;
  second.entries = { c };
  gui::g_state.layers = { first, second };
  gui::g_state.renderer.lens_type = gui::kLensTypeFisheyeEqualArea;
  gui::g_state.renderer.fov = 120.0f;
  gui::g_state.sun.altitude = 25.0f;
}

}  // namespace

void RegisterConfigSummaryLayoutTests(ImGuiTestEngine* engine) {
  for (int idx = 0; idx < kSceneCount; idx++) {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "config_summary_layout", kScenes[idx].name);
    t->ArgVariant = idx;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      const auto& scene = kScenes[ctx->Test->ArgVariant];
      ResetTestState();
      g_fullframe_capture.Reset();

      if (scene.two_layers) {
        SeedTwoLayerDocument();
      }
      gui::g_state.config_summary_window_open = true;
      ctx->Yield(4);

      ImGuiWindow* win = ctx->GetWindowByRef("###ConfigSummary");
      IM_CHECK(win != nullptr);
      IM_CHECK(win->WasActive);

      ctx->WindowMove("###ConfigSummary", ImVec2(kParkX, kParkY));
      ctx->Yield(2);

      // Park the mouse off-window: a hovered row bakes its highlight into the reference. Must come
      // after WindowMove, which drives the mouse to the title bar to drag it.
      ctx->MouseMoveToPos(ImVec2(-100.0f, -100.0f));
      ctx->Yield(4);

      const ImGuiIO& io = ImGui::GetIO();
      const float win_w = io.DisplaySize.x;
      const float win_h = io.DisplaySize.y;
      const float sx = io.DisplayFramebufferScale.x;
      const float sy = io.DisplayFramebufferScale.y;
      IM_CHECK_GT(win_w, 0.0f);
      IM_CHECK_GT(win_h, 0.0f);
      const int fb_w = static_cast<int>(std::lround(win_w * sx));
      const int fb_h = static_cast<int>(std::lround(win_h * sy));

      const ImVec2 vp_pos = ImGui::GetMainViewport()->Pos;
      const float lx = win->Pos.x - vp_pos.x;
      const float ly = win->Pos.y - vp_pos.y;
      fprintf(stderr, "[config_summary_layout] %s: fb=%dx%d win=%.0fx%.0f window pos=(%.1f,%.1f) size=(%.1f,%.1f)\n",
              scene.name, fb_w, fb_h, win_w, win_h, lx, ly, win->Size.x, win->Size.y);
      IM_CHECK_EQ(lx, kParkX);
      IM_CHECK_EQ(ly, kParkY);
      // The one-screen budget, on the very documents the references are shot from: no wider
      // than 1280, no taller than 900, and nothing scrolled out of the capture. A reference shot
      // of a scrolled window would be a picture of part of the page passing as the page.
      IM_CHECK_LE(win->Size.x, 1280.0f);
      IM_CHECK_LE(win->Size.y, 900.0f);
      IM_CHECK_EQ(win->ScrollMax.y, 0.0f);

      int rx = static_cast<int>(std::lround(lx * sx));
      int ry = static_cast<int>(std::lround((win_h - (ly + win->Size.y)) * sy));
      int rw = static_cast<int>(std::lround(win->Size.x * sx));
      int rh = static_cast<int>(std::lround(win->Size.y * sy));
      // Not clipped, as a machine check: a window whose bottom ran off the framebuffer would be
      // captured as a shorter, internally consistent image and pass forever against an equally
      // truncated reference.
      IM_CHECK_GE(rx, 0);
      IM_CHECK_GE(ry, 0);
      IM_CHECK_LE(rx + rw, fb_w);
      IM_CHECK_LE(ry + rh, fb_h);

      g_fullframe_capture.rect_x = rx;
      g_fullframe_capture.rect_y = ry;
      g_fullframe_capture.rect_w = rw;
      g_fullframe_capture.rect_h = rh;
      g_fullframe_capture.requested.store(true);
      for (int i = 0; i < 10 && !g_fullframe_capture.done.load(); ++i) {
        ctx->Yield(1);
      }
      IM_CHECK(g_fullframe_capture.done.load());
      IM_CHECK_EQ(g_fullframe_capture.width, rw);
      IM_CHECK_EQ(g_fullframe_capture.height, rh);

      bool has_nonzero = false;
      for (size_t i = 0; i < g_fullframe_capture.pixels.size() && !has_nonzero; ++i) {
        if (g_fullframe_capture.pixels[i] != 0) {
          has_nonzero = true;
        }
      }
      IM_CHECK(has_nonzero);

      // Tmp filename must match ReferenceGroup.tmp_prefix in scripts/regen_gui_test_refs.py.
      const std::string tmp_path =
          GuiTestTempPath(std::string("lumice_config_summary_") + scene.name + ".png").string();
      const std::string ref_path = std::string(LUMICE_TEST_REF_DIR) + "/config_summary_" + scene.name + ".png";
      auto rgb = lumice::test::StripAlpha(g_fullframe_capture.pixels.data(), g_fullframe_capture.width,
                                          g_fullframe_capture.height);
      IM_CHECK(lumice::test::SavePng(tmp_path.c_str(), rgb.data(), g_fullframe_capture.width,
                                     g_fullframe_capture.height, 3));

      gui::g_state.config_summary_window_open = false;
      ctx->Yield(2);

      IM_CHECK(lumice::test::CheckAgainstReference("config_summary_layout", scene.name, tmp_path, ref_path,
                                                   kDeterministicThresholdDb, g_keep_export_png, &kRuler));
    };
  }
}
