// Save > Screenshot... now opens an options popup first (RenderScreenshotExportOptionsPopup,
// src/gui/app_panels.cpp): per overlay family a Line and a Label box, prefilled from the panel and
// greyed where the screen shows nothing, plus the display mode. What needs a real frame, and so is
// asserted here:
//   - the popup: prefilled from the panel on every open, greyed boxes that a click cannot tick,
//     and a Cancel that leaves the document and the next published frame exactly as they were;
//   - the pixels RenderScreenshot (the command's render half, app.cpp) reads back: an untouched
//     selection exports the screen, an unticked family's line or label leaves the export exactly
//     as switching it off on the panel would, and the display-mode choice takes effect.
//
// Deliberately NOT here: the field-by-field narrowing and the subtract-only clamp, asserted without
// a frame in unit-correctness/gui/test_screenshot_export_options.cpp and
// composition-correctness/gui/test_screenshot_export_label_sets.cpp. The file dialog and the PNG
// write are the Screenshot command's old tail, unchanged, and covered by functional/test_export.cpp
// through ExportPreviewPng.
//
// The popup is opened through DoExportPreviewPng, the function the menu item calls, rather than by
// clicking the menu: the menu's reachability is test_file_ops.cpp's business, and clicking the
// popup's own Export... button would block on the OS file dialog.

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "gui/app.hpp"
#include "gui/gui_state.hpp"
#include "gui/screenshot_export_options.hpp"
#include "lumice.h"
#include "test_gui_shared.hpp"

namespace {

constexpr const char* kPopup = "Screenshot Export Options";

// ---------------------------------------------------------------------------------------------
// Main-thread scaffolding: the upload and the render are GL calls, and TestFunc runs on the test
// engine's coroutine with no context bound, so both are requests the GuiFunc fulfils.
// ---------------------------------------------------------------------------------------------

struct Request {
  bool upload_requested = false;
  bool upload_done = false;
  bool render_requested = false;
  bool render_done = false;
  gui::ScreenshotExportSelection sel;
  gui::ScreenshotRender out;
};

Request g_ss;

void ScreenshotGuiFunc(ImGuiTestContext* /*ctx*/) {
  if (g_ss.upload_requested && !g_ss.upload_done) {
    InitSynthTexture();
    gui::g_preview.UploadTexture(g_synth_tex.data(), kSynthTexW, kSynthTexH);
    g_ss.upload_done = true;
  }
  if (g_ss.render_requested && !g_ss.render_done) {
    g_ss.out = gui::RenderScreenshot(g_ss.sel);
    g_ss.render_done = true;
  }
}

// Reset, upload a picture, and let the preview publish a frame of it.
bool BringUpPreview(ImGuiTestContext* ctx) {
  ResetTestState();
  g_ss = Request{};
  g_ss.upload_requested = true;
  ctx->Yield(4);
  IM_CHECK_RETV(g_ss.upload_done, false);
  IM_CHECK_RETV(gui::g_preview_vp.vp_w > 0 && gui::g_preview_vp.vp_h > 0, false);
  return true;
}

// Let the preview re-derive its params from a GuiState the case just changed.
void Settle(ImGuiTestContext* ctx) {
  ctx->Yield(3);
}

bool Render(ImGuiTestContext* ctx, const gui::ScreenshotExportSelection& sel, std::vector<unsigned char>* rgba) {
  g_ss.sel = sel;
  g_ss.render_done = false;
  g_ss.render_requested = true;
  ctx->Yield(2);
  g_ss.render_requested = false;
  IM_CHECK_RETV(g_ss.render_done, false);
  IM_CHECK_RETV(!g_ss.out.rgba.empty(), false);
  *rgba = std::move(g_ss.out.rgba);
  return true;
}

gui::ScreenshotExportSelection Prefill() {
  return gui::MakeScreenshotExportSelectionFromState(gui::g_state);
}

// Same shape as functional/test_export.cpp's: identical buffers read as a large finite number.
double Psnr(const std::vector<unsigned char>& a, const std::vector<unsigned char>& b) {
  if (a.empty() || a.size() != b.size()) {
    return -1.0;
  }
  double mse = 0.0;
  for (size_t i = 0; i < a.size(); ++i) {
    const int d = static_cast<int>(a[i]) - static_cast<int>(b[i]);
    mse += static_cast<double>(d * d);
  }
  mse /= static_cast<double>(a.size());
  if (mse == 0.0) {
    return 1e30;
  }
  return 10.0 * std::log10(255.0 * 255.0 / mse);
}

// "The same picture" and "a different picture", placed against the smallest break these cases must
// catch rather than against test_export.cpp's determinism bars: one horizon label on an otherwise
// identical render measured 57.7 dB (a build whose selection ignored the label bit read 56.1 dB
// against the panel-off render), so a 50 dB bar would pass exactly the defect it is here for.
// Every "same" pair below measured bit-identical, labels included — each render reads the one
// anchor cache the frame computed — so 70 dB leaves room only for a driver's last-bit noise.
constexpr double kSameDb = 70.0;

void SetHorizonAndGrid(bool horizon_line, bool horizon_label, bool grid_line, bool grid_label) {
  gui::g_state.show_horizon_line = horizon_line;
  gui::g_state.show_horizon_label = horizon_label;
  gui::g_state.show_grid_line = grid_line;
  gui::g_state.show_grid_label = grid_label;
}

bool OpenPopup(ImGuiTestContext* ctx) {
  gui::DoExportPreviewPng();
  ctx->Yield(2);
  IM_CHECK_RETV(ImGui::IsPopupOpen(kPopup), false);
  return true;
}

}  // namespace

void RegisterScreenshotExportOptionsTests(ImGuiTestEngine* engine) {
  // -------------------------------------------------------------------------------------------
  // The popup.
  // -------------------------------------------------------------------------------------------

  // Prefilled from the panel, and greyed exactly where the panel has the family off — line and
  // label independently, and the reference points as one family (any ring on, any name on). A click
  // on a greyed box changes nothing: the popup can only take away.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "screenshot_options", "the_popup_is_prefilled_and_greys_what_the_screen_lacks");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetHorizonAndGrid(/*horizon_line=*/true, /*horizon_label=*/false, /*grid_line=*/false, /*grid_label=*/true);
      gui::g_state.show_lens_border_line = true;
      gui::g_state.show_sun_circles_line = false;
      gui::g_state.show_sun_circles_label = false;
      for (gui::MarkerAppearance& m : gui::g_state.markers) {
        m.show = false;
        m.label = false;
      }
      gui::g_state.markers[3].label = true;
      IM_CHECK(OpenPopup(ctx));

      const gui::ScreenshotExportSelection& sel = gui::g_screenshot_export_selection;
      IM_CHECK(sel.horizon_line && !sel.horizon_label);
      IM_CHECK(!sel.grid_line && sel.grid_label);
      IM_CHECK(sel.lens_border_line);
      IM_CHECK(!sel.sun_circles_line && !sel.sun_circles_label);
      IM_CHECK(!sel.markers_line && sel.markers_label);

      IM_CHECK(!IsDisabled(ctx->ItemInfo("**/##ss_horizon_line")));
      IM_CHECK(IsDisabled(ctx->ItemInfo("**/##ss_horizon_label")));
      IM_CHECK(IsDisabled(ctx->ItemInfo("**/##ss_grid_line")));
      IM_CHECK(!IsDisabled(ctx->ItemInfo("**/##ss_grid_label")));
      IM_CHECK(!IsDisabled(ctx->ItemInfo("**/##ss_lens_border_line")));
      IM_CHECK(IsDisabled(ctx->ItemInfo("**/##ss_sun_circles_line")));
      IM_CHECK(IsDisabled(ctx->ItemInfo("**/##ss_markers_line")));
      IM_CHECK(!IsDisabled(ctx->ItemInfo("**/##ss_markers_label")));

      // A greyed box does not tick; an enabled one does un-tick.
      ctx->ItemClick("**/##ss_grid_line");
      IM_CHECK(!gui::g_screenshot_export_selection.grid_line);
      ctx->ItemClick("**/##ss_horizon_line");
      IM_CHECK(!gui::g_screenshot_export_selection.horizon_line);

      ctx->ItemClick((std::string(kPopup) + "/Cancel").c_str());
      ctx->Yield(2);
      IM_CHECK(!ImGui::IsPopupOpen(kPopup));
    };
  }

  // One-shot: un-ticking and cancelling leaves the document and the published frame as they were,
  // and the next open is prefilled from the panel as it is THEN — not from what the last popup held.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "screenshot_options", "cancel_changes_nothing_and_the_next_open_prefills_afresh");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      SetHorizonAndGrid(true, true, true, true);
      IM_CHECK(OpenPopup(ctx));
      ctx->ItemClick("**/##ss_horizon_line");
      ctx->ItemClick("**/##ss_horizon_label");
      IM_CHECK(!gui::g_screenshot_export_selection.horizon_line);
      IM_CHECK(!gui::g_screenshot_export_selection.horizon_label);
      ctx->ItemClick((std::string(kPopup) + "/Cancel").c_str());
      ctx->Yield(2);
      IM_CHECK(!ImGui::IsPopupOpen(kPopup));

      IM_CHECK(gui::g_state.show_horizon_line);
      IM_CHECK(gui::g_state.show_horizon_label);
      IM_CHECK(gui::g_state.show_grid_line);
      IM_CHECK(gui::g_state.show_grid_label);
      IM_CHECK(!gui::g_state.dirty);

      gui::g_state.show_grid_line = false;
      IM_CHECK(OpenPopup(ctx));
      IM_CHECK(gui::g_screenshot_export_selection.horizon_line);
      IM_CHECK(gui::g_screenshot_export_selection.horizon_label);
      IM_CHECK(!gui::g_screenshot_export_selection.grid_line);
      ctx->ItemClick((std::string(kPopup) + "/Cancel").c_str());
      ctx->Yield(2);
      IM_CHECK(!ImGui::IsPopupOpen(kPopup));
    };
  }

  // The display mode is a free choice between the two pictures, written to the selection only —
  // and greyed under Print by the preview control's own rule.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "screenshot_options", "the_display_mode_is_chosen_for_the_export_only");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      const std::string br =
          std::string("**/") + gui::kDisplayModeNames[LUMICE_DISPLAY_MODE_CHANNEL_BR] + "##ss_display_mode";
      IM_CHECK(OpenPopup(ctx));
      IM_CHECK_EQ(gui::g_screenshot_export_selection.display_mode, 0);
      IM_CHECK(!IsDisabled(ctx->ItemInfo(br.c_str())));
      ctx->ItemClick(br.c_str());
      IM_CHECK_EQ(gui::g_screenshot_export_selection.display_mode, LUMICE_DISPLAY_MODE_CHANNEL_BR);
      IM_CHECK_EQ(gui::g_state.renderer.display_mode, 0);
      ctx->ItemClick((std::string(kPopup) + "/Cancel").c_str());
      ctx->Yield(2);
      IM_CHECK(!ImGui::IsPopupOpen(kPopup));

      gui::g_state.renderer.tone = LUMICE_TONE_PRINT;
      IM_CHECK(OpenPopup(ctx));
      IM_CHECK(IsDisabled(ctx->ItemInfo(br.c_str())));
      ctx->ItemClick((std::string(kPopup) + "/Cancel").c_str());
      ctx->Yield(2);
      IM_CHECK(!ImGui::IsPopupOpen(kPopup));
    };
  }

  // -------------------------------------------------------------------------------------------
  // The pixels.
  // -------------------------------------------------------------------------------------------

  // Untouched, the selection exports the screen: the prefill renders what the all-in selection does
  // (whose intersection with the panel is the panel itself), with lines only and with labels.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "screenshot_options", "an_untouched_selection_exports_the_screen");
    t->GuiFunc = ScreenshotGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      IM_CHECK(BringUpPreview(ctx));
      std::vector<unsigned char> prefilled;
      std::vector<unsigned char> all_in;

      SetHorizonAndGrid(true, false, true, false);
      Settle(ctx);
      IM_CHECK(Render(ctx, Prefill(), &prefilled));
      IM_CHECK(Render(ctx, gui::ScreenshotExportSelection{}, &all_in));
      const double lines_db = Psnr(prefilled, all_in);
      IM_CHECK(lines_db >= kSameDb);

      SetHorizonAndGrid(true, true, true, true);
      Settle(ctx);
      IM_CHECK(Render(ctx, Prefill(), &prefilled));
      IM_CHECK(Render(ctx, gui::ScreenshotExportSelection{}, &all_in));
      const double labelled_db = Psnr(prefilled, all_in);
      fprintf(stderr, "[screenshot_options] untouched: lines %.2f dB, labelled %.2f dB\n", lines_db, labelled_db);
      IM_CHECK(labelled_db >= kSameDb);
    };
  }

  // Un-ticking the horizon's line and label exports exactly what switching them off on the panel
  // would — and the panel, and the frame it publishes, still have them on afterwards. Then the
  // label alone, to show the text is gated apart from the line.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "screenshot_options", "an_unticked_family_leaves_the_export_as_if_switched_off");
    t->GuiFunc = ScreenshotGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      IM_CHECK(BringUpPreview(ctx));
      std::vector<unsigned char> screen;
      std::vector<unsigned char> unticked;
      std::vector<unsigned char> label_unticked;
      std::vector<unsigned char> panel_off;
      std::vector<unsigned char> panel_label_off;

      SetHorizonAndGrid(true, true, true, false);
      Settle(ctx);
      IM_CHECK(Render(ctx, Prefill(), &screen));
      gui::ScreenshotExportSelection sel = Prefill();
      sel.horizon_line = false;
      sel.horizon_label = false;
      IM_CHECK(Render(ctx, sel, &unticked));
      sel = Prefill();
      sel.horizon_label = false;
      IM_CHECK(Render(ctx, sel, &label_unticked));

      // One-shot: nothing the export did reached the document or the frame the screen shows.
      Settle(ctx);
      IM_CHECK(gui::g_state.show_horizon_line);
      IM_CHECK(gui::g_state.show_horizon_label);
      IM_CHECK(gui::g_preview_vp.params.overlay.show_horizon);
      IM_CHECK(!gui::g_preview_vp.curve_labels.empty());

      SetHorizonAndGrid(false, false, true, false);
      Settle(ctx);
      IM_CHECK(Render(ctx, Prefill(), &panel_off));
      SetHorizonAndGrid(true, false, true, false);
      Settle(ctx);
      IM_CHECK(Render(ctx, Prefill(), &panel_label_off));

      const double off_db = Psnr(unticked, panel_off);
      const double label_off_db = Psnr(label_unticked, panel_label_off);
      const double vs_screen_db = Psnr(unticked, screen);
      const double label_vs_line_db = Psnr(label_unticked, panel_off);
      const double label_vs_screen_db = Psnr(label_unticked, screen);
      fprintf(stderr,
              "[screenshot_options] unticked vs panel-off %.2f dB, label-unticked vs panel-label-off %.2f dB, "
              "unticked vs screen %.2f dB, label-unticked vs panel-off %.2f dB, label-unticked vs screen %.2f dB\n",
              off_db, label_off_db, vs_screen_db, label_vs_line_db, label_vs_screen_db);
      IM_CHECK(off_db >= kSameDb);
      IM_CHECK(label_off_db >= kSameDb);
      // And the family was really there to lose: the screen differs, dropping only the label keeps
      // the line, and the label alone was visible ink.
      IM_CHECK(vs_screen_db < kSameDb);
      IM_CHECK(label_vs_line_db < kSameDb);
      IM_CHECK(label_vs_screen_db < kSameDb);
    };
  }

  // The display-mode choice takes effect: a B-R export of a Normal screen is the B-R screen's
  // export, and not the Normal one.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "screenshot_options", "the_display_mode_choice_takes_effect");
    t->GuiFunc = ScreenshotGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      IM_CHECK(BringUpPreview(ctx));
      std::vector<unsigned char> normal;
      std::vector<unsigned char> chosen_br;
      std::vector<unsigned char> screen_br;

      SetHorizonAndGrid(false, false, false, false);
      Settle(ctx);
      IM_CHECK(Render(ctx, Prefill(), &normal));
      gui::ScreenshotExportSelection sel = Prefill();
      sel.display_mode = LUMICE_DISPLAY_MODE_CHANNEL_BR;
      IM_CHECK(Render(ctx, sel, &chosen_br));
      IM_CHECK_EQ(gui::g_state.renderer.display_mode, 0);

      gui::g_state.renderer.display_mode = LUMICE_DISPLAY_MODE_CHANNEL_BR;
      Settle(ctx);
      IM_CHECK(Render(ctx, Prefill(), &screen_br));

      const double same_db = Psnr(chosen_br, screen_br);
      const double vs_normal_db = Psnr(chosen_br, normal);
      fprintf(stderr, "[screenshot_options] chosen B-R vs screen B-R %.2f dB, vs normal %.2f dB\n", same_db,
              vs_normal_db);
      IM_CHECK(same_db >= kSameDb);
      IM_CHECK(vs_normal_db < kSameDb);
    };
  }
}
