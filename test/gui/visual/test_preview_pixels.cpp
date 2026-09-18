// The pixel comparisons that are not part of a regeneratable reference group.
//
// One subject, decided by looking at a rendered frame and nothing else: committed references.
// Four crystal-preview scenes (one per crystal kind and shading style the modal offers) and the
// left panel as a whole. These guard the two GL paths the rest of the suite only ever asserts
// indirectly: the crystal mesh going through CrystalRenderer's FBO, and the left panel's assembled
// cards going through ImGui into the default framebuffer. A layout or shading change that no
// functional assertion is watching lands here first.
//
// The self-comparison that used to share this file — run, save, reopen, compare the reopened
// document to the live display — is a parity case now (test/gui/parity/
// test_gui_lmc_roundtrip_parity.cpp), where it is held to byte exactness rather than a PSNR.
//
// Why these are not a fifth reference group. The four registered groups
// (scripts/regen_gui_test_refs.py) exist so a deliberate visual change can be re-shot and
// re-calibrated by a driver. These five compare deterministic GL output byte-for-byte (the pixel
// ruler at tau = 0, K = 0), have no per-scene calibrated threshold to maintain, and are updated by
// copying one file. They call lumice::test::CheckAgainstReference like the groups do — same
// comparison, same stderr lines — but under a tag the driver's registry does not know, so its
// sampling never picks them up.
//
// What a user sees when these break: the crystal preview in the edit modal draws the wrong solid,
// or draws it unshaded; the left panel's cards shift or lose a control.

#include <cstdio>
#include <string>
#include <vector>

#include "gui/gui_state.hpp"
#include "test_gui_shared.hpp"

namespace {

// The ruler these scenes are held to: byte-identity (tau = 0, K = 0). They render pixel-identical
// run to run and are evaluated only on the reference machine — the CI llvmpipe leg's filter does
// not name the visual category — so there is no cross-machine noise to leave room for. The 40 dB
// PSNR floor they used to compare at is kept only for the diagnostic line; it let a 90-px
// scrollbar-thumb drift through at 53 dB elsewhere in this suite, which is why it no longer
// decides anything here (support/pixel_diff_metrics.hpp).
constexpr lumice::test::MaxCcRuler kRuler{ /*tau=*/0, /*max_cc_threshold=*/0 };
constexpr double kDeterministicThresholdDb = 40.0;

// Rebuild the crystal mesh (if changed) and render it into g_crystal_renderer's FBO so the capture
// below has something to read. Test-only: the left panel stopped driving this FBO every frame when
// the bottom preview was removed, and the modal guard mirrors the in-panel logic it replaced so an
// open edit modal's own FBO content is not overwritten.
void DriveCrystalPreviewFbo() {
  if (gui::IsEditModalOpen()) {
    return;
  }
  if (gui::g_state.layers.empty() || gui::g_state.layers[0].entries.empty()) {
    return;
  }
  const auto& cr = gui::g_state.crystals[gui::g_state.layers[0].entries[0].crystal_id];
  const int hash = gui::CrystalParamHash(cr);
  if (hash != gui::g_crystal_mesh_hash) {
    if (gui::BuildAndUploadCrystalMesh(cr, gui::kPreviewFixedSampleSeed) != 0) {
      gui::g_crystal_mesh_hash = hash;
    }
  }
  gui::g_crystal_renderer.Render(gui::g_crystal_rotation, gui::g_crystal_zoom,
                                 static_cast<gui::CrystalStyle>(gui::g_crystal_style));
}

void CrystalCaptureGuiFunc(ImGuiTestContext* /*ctx*/) {
  if (g_capture.capture_requested && !g_capture.capture_done) {
    DriveCrystalPreviewFbo();
    const int w = gui::g_crystal_renderer.Width();
    const int h = gui::g_crystal_renderer.Height();
    g_capture.pixels =
        lumice::test::ReadTexturePixels(static_cast<unsigned int>(gui::g_crystal_renderer.GetTextureId()), w, h);
    g_capture.width = w;
    g_capture.height = h;
    g_capture.capture_done = true;
  }
}

// One committed crystal-preview reference.
//
// The names carry a `crystal_preview_` prefix because the modal_layout reference group already owns
// scenes called `crystal_prism` and `crystal_pyramid`. Nothing breaks if they collide — the regen
// driver attributes PSNR lines by the `[<group>]` tag, and this suite's tag is `visual` — but
// `--filter crystal_prism` would select both suites, and a stderr log would show two lines a reader
// has to tell apart by their tag alone.
//
// `kKeepDefault` marks the field this scene does not touch, so each row states only what makes it
// different from the default modal preview — a row that set every field would make "which knob is
// this scene about" unreadable.
constexpr int kKeepDefault = -1;
struct CrystalScene {
  const char* name;      // registration name, stderr tag, and capture filename stem
  const char* ref_file;  // under test/gui/references/
  int type;              // gui::CrystalType, or kKeepDefault
  int style;             // gui::g_crystal_style (CrystalStyle index), or kKeepDefault
};
const CrystalScene kCrystalScenes[] = {
  // The default modal preview: a prism under Hidden Line. It is the scene every OTHER assertion in
  // the suite implicitly assumes is drawing something, so it is the one worth pinning first.
  { "crystal_preview_prism", "crystal_prism_default.png", kKeepDefault, kKeepDefault },
  // The other crystal kind. Its wedge geometry is generated by a different closed-form path than
  // the prism's, and nothing else in this suite looks at the result.
  { "crystal_preview_pyramid", "crystal_pyramid_default.png", static_cast<int>(gui::CrystalType::kPyramid),
    kKeepDefault },
  // The two shading styles that differ most from Hidden Line: one draws edges only, the other
  // fills faces. Between them they cover both halves of the renderer's draw path.
  { "crystal_preview_wireframe", "crystal_wireframe.png", kKeepDefault,
    static_cast<int>(gui::CrystalStyle::kWireframe) },
  { "crystal_preview_shaded", "crystal_shaded.png", kKeepDefault, static_cast<int>(gui::CrystalStyle::kShaded) },
};
constexpr int kCrystalSceneCount = sizeof(kCrystalScenes) / sizeof(kCrystalScenes[0]);

// A capture full of zeros compares as a black image, and against a mostly-dark reference that can
// clear a PSNR threshold. Every capture below therefore passes this gate first, so a readback that
// silently returned nothing fails saying so rather than passing quietly.
bool AnyNonZero(const std::vector<unsigned char>& pixels) {
  for (unsigned char v : pixels) {
    if (v != 0) {
      return true;
    }
  }
  return false;
}

}  // namespace

void RegisterPreviewPixelTests(ImGuiTestEngine* engine) {
  for (int idx = 0; idx < kCrystalSceneCount; ++idx) {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "visual", kCrystalScenes[idx].name);
    t->GuiFunc = CrystalCaptureGuiFunc;
    t->ArgVariant = idx;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      const CrystalScene& scene = kCrystalScenes[ctx->Test->ArgVariant];
      ResetTestState();
      ctx->Yield();

      if (scene.type != kKeepDefault && !gui::g_state.layers.empty() && !gui::g_state.layers[0].entries.empty()) {
        gui::g_state.crystals[gui::g_state.layers[0].entries[0].crystal_id].type =
            static_cast<gui::CrystalType>(scene.type);
        gui::g_crystal_mesh_hash = -1;  // force the rebuild the changed type needs
      }
      if (scene.style != kKeepDefault) {
        gui::g_crystal_style = scene.style;
      }
      // Three frames: the state change, the mesh rebuild it triggers, and the FBO render.
      ctx->Yield(3);

      g_capture.Reset();
      g_capture.capture_requested = true;
      ctx->Yield(2);
      IM_CHECK(g_capture.capture_done);
      IM_CHECK_EQ(static_cast<int>(g_capture.pixels.size()), g_capture.width * g_capture.height * 4);
      IM_CHECK(AnyNonZero(g_capture.pixels));

      const std::string tmp_path = GuiTestTempPath(std::string("lumice_") + scene.name + ".png").string();
      const std::string ref_path = std::string(LUMICE_TEST_REF_DIR "/") + scene.ref_file;
      auto rgb = lumice::test::StripAlpha(g_capture.pixels.data(), g_capture.width, g_capture.height);
      IM_CHECK(lumice::test::SavePng(tmp_path.c_str(), rgb.data(), g_capture.width, g_capture.height, 3));
      IM_CHECK(lumice::test::CheckAgainstReference("visual", scene.name, tmp_path, ref_path, kDeterministicThresholdDb,
                                                   g_keep_export_png, &kRuler));
    };
  }

  // The left panel as a whole, read back out of the DEFAULT framebuffer rather than an FBO — the
  // only committed reference in the repo that covers the cards, their spacing and their chrome as
  // ImGui actually lays them out.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "visual", "left_panel");
    // No GuiFunc: the capture happens in the main loop's post-RenderDrawData hook.
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      g_left_panel_capture.Reset();

      // Park the cursor off-screen. A highlighted card baked into the reference would make every
      // later no-hover run fail, which is how this reference was first captured wrong.
      ctx->MouseMoveToPos(ImVec2(-100.0f, -100.0f));
      ctx->Yield(3);

      g_left_panel_capture.requested.store(true);
      for (int i = 0; i < 10 && !g_left_panel_capture.done.load(); ++i) {
        ctx->Yield(1);
      }
      IM_CHECK(g_left_panel_capture.done.load());
      IM_CHECK(AnyNonZero(g_left_panel_capture.pixels));
      fprintf(stderr, "[visual] left_panel: captured size = %dx%d\n", g_left_panel_capture.width,
              g_left_panel_capture.height);

      const std::string tmp_path = GuiTestTempPath("lumice_left_panel.png").string();
      const std::string ref_path = std::string(LUMICE_TEST_REF_DIR "/left_panel_default.png");
      auto rgb = lumice::test::StripAlpha(g_left_panel_capture.pixels.data(), g_left_panel_capture.width,
                                          g_left_panel_capture.height);
      IM_CHECK(lumice::test::SavePng(tmp_path.c_str(), rgb.data(), g_left_panel_capture.width,
                                     g_left_panel_capture.height, 3));
      IM_CHECK(lumice::test::CheckAgainstReference("visual", "left_panel", tmp_path, ref_path,
                                                   kDeterministicThresholdDb, g_keep_export_png, &kRuler));
    };
  }

  // The save-then-reopen comparison that used to live here (`save_open_visual_consistency`, a 28 dB
  // PSNR between the live preview and its 8-bit .lmc bake) is now
  // test/gui/parity/test_gui_lmc_roundtrip_parity.cpp, at byte exactness: the .lmc texture section
  // carries the live frame's linear XYZ floats, so the reopened document renders through the same
  // shader branch on the same bytes and a threshold has nothing left to absorb.
}
