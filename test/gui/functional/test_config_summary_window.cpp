// The Summary window (src/gui/config_summary_window.cpp) as the user reaches it: the top-bar
// button opens and closes it, its own X closes it, every document reset closes it, and what it
// draws is the page config_summary.cpp built — one table per display LINE (a field, or a packed
// run of same-kind fields), so the rendered line count is the page's own count
// (CountConfigSummaryLines) and not a second enumeration.
//
// The page's CONTENT is asserted without a frame elsewhere (test_config_summary_rows.cpp for the
// tier rule and the line packing, test_config_summary_export_parity_chain.cpp for value parity
// with the export and the "only what the panel enables" gate); this file only holds what needs
// the window on screen: the size budget (both reference documents fit 1280 x 900 with no
// scrolling) and that a live change of the document — the tone switch — moves the rows in place.

#include <string>

#include "IconsFontAwesome6.h"
#include "gui/app.hpp"
#include "gui/config_summary.hpp"
#include "gui/config_summary_window.hpp"
#include "include/lumice.h"
#include "test_gui_shared.hpp"

namespace {

constexpr const char* kButtonRef = "##TopBar/" ICON_FA_FILE_LINES " Summary";
constexpr const char* kWindowRef = "###ConfigSummary";

// The window as ImGui drew it this frame, or null when it is not being submitted.
ImGuiWindow* SummaryWindow(ImGuiTestContext* ctx) {
  ImGuiWindow* win = ctx->GetWindowByRef(kWindowRef);
  return (win != nullptr && win->WasActive) ? win : nullptr;
}

// The line tables the window drew this frame: "##summary_group_<g>_line_<l>" for group g's l-th
// line, counted while they exist. Each holds exactly one row (ImGuiTable::CurrentRow, the index of
// the last row submitted, survives EndTable and must read 0), so the number of tables IS the
// rendered line count. Returns -1 on a table with more than one row, which would mean the window
// stopped drawing one line per table, or when the two-column table they sit in is not drawn.
//
// The line tables are cells of the window's two-column table ("##summary_columns"), and a table
// pushes its own id over the window's while its body is drawn (imgui_tables.cpp BeginTableEx:
// PushOverrideID), so a line table's id is seeded by the column table's id, not the window's.
int RenderedLineCount(ImGuiTestContext* ctx, int group_count) {
  ImGuiWindow* win = SummaryWindow(ctx);
  if (win == nullptr) {
    return -1;
  }
  ImGuiTable* columns = ImGui::TableFindByID(win->GetID("##summary_columns"));
  if (columns == nullptr) {
    return -1;
  }
  // Only tables drawn in the frame just ended count: ImGui keeps a table's record in its pool for
  // a while after it stops being submitted, so a bigger document drawn by an earlier case (or
  // earlier in this one) leaves line tables behind that a bare TableFindByID still returns.
  const int this_frame = ImGui::GetFrameCount();
  int lines = 0;
  for (int g = 0; g < group_count; ++g) {
    for (int l = 0;; ++l) {
      const std::string id = "##summary_group_" + std::to_string(g) + "_line_" + std::to_string(l);
      ImGuiTable* table = ImGui::TableFindByID(ImGui::GetIDWithSeed(id.c_str(), nullptr, columns->ID));
      if (table == nullptr || table->LastFrameActive < this_frame - 1) {
        break;
      }
      if (table->CurrentRow != 0) {
        return -1;
      }
      ++lines;
    }
  }
  return lines;
}

// The one-screen budget the page is designed to: the window is no wider than 1280, no taller than
// 900, and nothing in it is scrolled away.
bool FitsOneScreen(const ImGuiWindow* win, const char* stage) {
  if (win->Size.x > 1280.0f || win->Size.y > 900.0f) {
    IM_ERRORF("%s: window is %.0f x %.0f, outside the 1280 x 900 budget", stage, win->Size.x, win->Size.y);
    return false;
  }
  if (win->ScrollMax.y > 0.0f) {
    IM_ERRORF("%s: the content scrolls (ScrollMax.y = %.0f, window %.0f tall, content %.0f)", stage, win->ScrollMax.y,
              win->Size.y, win->ContentSize.y);
    return false;
  }
  return true;
}

bool HasRow(const gui::ConfigSummary& page, const char* title, const char* label) {
  for (const auto& g : page.settings) {
    if (g.title != title) {
      continue;
    }
    for (const auto& f : g.fields) {
      if (f.label == label) {
        return true;
      }
    }
  }
  return false;
}

// A document with two layers and three entries so the row count is not the default document's.
void SeedTwoLayerDocument() {
  gui::g_state.crystals.assign(2, gui::CrystalConfig{});
  gui::g_state.crystals[1].type = gui::CrystalType::kPyramid;
  gui::FilterConfig f;
  f.name = "cza";
  f.SetRaypath(gui::RaypathParams{ "3-5-1" });
  gui::g_state.filters.assign(1, f);
  gui::Layer first;
  first.probability = 0.5f;
  gui::EntryCard a;
  a.crystal_id = 0;
  a.filter_id = 0;
  gui::EntryCard b;
  b.crystal_id = 1;
  first.entries = { a, b };
  gui::Layer second;
  gui::EntryCard c;
  c.crystal_id = 0;
  second.entries = { c };
  gui::g_state.layers = { first, second };
}

}  // namespace

void RegisterConfigSummaryWindowTests(ImGuiTestEngine* engine) {
  {
    // The button toggles: open, then closed again by the same button; and the X closes it too.
    ImGuiTest* t = IM_REGISTER_TEST(engine, "config_summary", "top_bar_button_opens_and_closes_the_window");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      IM_CHECK(ctx->ItemExists(kButtonRef));
      IM_CHECK(!gui::g_state.config_summary_window_open);
      IM_CHECK(SummaryWindow(ctx) == nullptr);

      ctx->ItemClick(kButtonRef);
      ctx->Yield(3);
      IM_CHECK(gui::g_state.config_summary_window_open);
      IM_CHECK(SummaryWindow(ctx) != nullptr);

      ctx->ItemClick(kButtonRef);
      ctx->Yield(3);
      IM_CHECK(!gui::g_state.config_summary_window_open);
      IM_CHECK(SummaryWindow(ctx) == nullptr);

      // Reopen and close through the window's own close button, which writes the same flag.
      ctx->ItemClick(kButtonRef);
      ctx->Yield(3);
      IM_CHECK(gui::g_state.config_summary_window_open);
      ctx->WindowClose(kWindowRef);
      ctx->Yield(3);
      IM_CHECK(!gui::g_state.config_summary_window_open);
      IM_CHECK(SummaryWindow(ctx) == nullptr);
    };
  }

  {
    // Every document reset closes it, the Revert reason included — the rule is unconditional
    // (app.cpp ResetFrontendState), unlike the analysis window's, so each reason is driven and
    // read. kOpenBaked needs a decoded texture the harness does not have; the other four are the
    // reasons a user reaches through New / Open / Import / Revert.
    ImGuiTest* t = IM_REGISTER_TEST(engine, "config_summary", "every_document_reset_closes_the_window");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      const gui::FrontendResetReason reasons[] = {
        gui::FrontendResetReason::kNewDocument,
        gui::FrontendResetReason::kOpenLmcBlank,
        gui::FrontendResetReason::kOpenJson,
        gui::FrontendResetReason::kRevert,
      };
      for (const auto reason : reasons) {
        ctx->ItemClick(kButtonRef);
        ctx->Yield(3);
        if (!gui::g_state.config_summary_window_open || SummaryWindow(ctx) == nullptr) {
          IM_ERRORF("reason %d: the window did not open before the reset", static_cast<int>(reason));
          return;
        }
        // Between frames, as the app's own New / Open / Revert handlers run it.
        gui::ResetFrontendState(gui::g_state, reason);
        ctx->Yield(3);
        if (gui::g_state.config_summary_window_open || SummaryWindow(ctx) != nullptr) {
          IM_ERRORF("reason %d: the window survived the document reset", static_cast<int>(reason));
          return;
        }
      }
      // And the top-bar New, the way the user does it: the same close, through the real handler.
      ctx->ItemClick(kButtonRef);
      ctx->Yield(3);
      IM_CHECK(gui::g_state.config_summary_window_open);
      gui::DoNew();
      ctx->Yield(3);
      IM_CHECK(!gui::g_state.config_summary_window_open);
      IM_CHECK(SummaryWindow(ctx) == nullptr);
    };
  }

  {
    // What is drawn is the page: as many line tables as the page has lines (packed runs counted
    // as the lines they cut into), the count moves with the document, and both reference
    // documents fit the one-screen budget with nothing scrolled away.
    ImGuiTest* t = IM_REGISTER_TEST(engine, "config_summary", "rendered_lines_are_the_pages_lines_and_fit_one_screen");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->ItemClick(kButtonRef);
      ctx->Yield(3);
      IM_CHECK(SummaryWindow(ctx) != nullptr);

      const gui::ConfigSummary default_page = gui::BuildConfigSummary(gui::g_state);
      const int default_groups = static_cast<int>(default_page.settings.size() + default_page.document.size());
      const int default_lines = gui::CountConfigSummaryLines(default_page);
      IM_CHECK_GT(default_lines, 0);
      // Packing is real on this page too: the default entry's faces and axis share lines.
      IM_CHECK_LT(default_lines, gui::CountConfigSummaryFields(default_page));
      IM_CHECK_EQ(RenderedLineCount(ctx, default_groups), default_lines);
      // One group more than the page has must NOT exist: the window draws exactly the page.
      {
        ImGuiTable* columns = ImGui::TableFindByID(SummaryWindow(ctx)->GetID("##summary_columns"));
        IM_CHECK(columns != nullptr);
        const std::string beyond = "##summary_group_" + std::to_string(default_groups) + "_line_0";
        IM_CHECK(ImGui::TableFindByID(ImGui::GetIDWithSeed(beyond.c_str(), nullptr, columns->ID)) == nullptr);
      }
      IM_CHECK(FitsOneScreen(SummaryWindow(ctx), "default document"));

      // The window is fixed-width and reads the live document: a bigger document adds lines in
      // place, without the window growing sideways — and still inside the budget.
      const float width_before = SummaryWindow(ctx)->Size.x;
      SeedTwoLayerDocument();
      ctx->Yield(3);
      const gui::ConfigSummary page = gui::BuildConfigSummary(gui::g_state);
      const int groups = static_cast<int>(page.settings.size() + page.document.size());
      IM_CHECK_GT(groups, default_groups);
      IM_CHECK_EQ(RenderedLineCount(ctx, groups), gui::CountConfigSummaryLines(page));
      IM_CHECK_EQ(SummaryWindow(ctx)->Size.x, width_before);
      IM_CHECK(FitsOneScreen(SummaryWindow(ctx), "two-layer document"));

      ctx->ItemClick(kButtonRef);
      ctx->Yield(2);
    };
  }

  {
    // The page follows the panel's own gate live: switching Mode to Print takes the Sky Color row
    // off the page and puts Paper Color on it (the panel's swatch does the same swap), and the
    // window redraws with the new line set on the next frame. Driven through the state rather
    // than the combo so the case is about the page, not about finding a combo; the swatch swap
    // itself is test_view_display_controls.cpp's.
    ImGuiTest* t = IM_REGISTER_TEST(engine, "config_summary", "print_mode_swaps_sky_color_for_paper_color");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->ItemClick(kButtonRef);
      ctx->Yield(3);
      IM_CHECK(SummaryWindow(ctx) != nullptr);

      gui::ConfigSummary page = gui::BuildConfigSummary(gui::g_state);
      IM_CHECK(HasRow(page, "Render", "Sky Color"));
      IM_CHECK(!HasRow(page, "Render", "Paper Color"));
      const int groups_before = static_cast<int>(page.settings.size() + page.document.size());
      const int lines_screen = RenderedLineCount(ctx, groups_before);
      IM_CHECK_EQ(lines_screen, gui::CountConfigSummaryLines(page));

      gui::g_state.renderer.tone = LUMICE_TONE_PRINT;
      ctx->Yield(3);
      page = gui::BuildConfigSummary(gui::g_state);
      IM_CHECK(!HasRow(page, "Render", "Sky Color"));
      IM_CHECK(HasRow(page, "Render", "Paper Color"));
      // One row swapped for one row: the line count is unchanged, and it is what is on screen.
      IM_CHECK_EQ(RenderedLineCount(ctx, static_cast<int>(page.settings.size() + page.document.size())), lines_screen);

      gui::g_state.renderer.tone = LUMICE_TONE_SCREEN;
      ctx->Yield(3);
      page = gui::BuildConfigSummary(gui::g_state);
      IM_CHECK(HasRow(page, "Render", "Sky Color"));
      IM_CHECK(!HasRow(page, "Render", "Paper Color"));

      // And the popup-only heading is on screen as its own group, after the three panel groups.
      IM_CHECK(HasRow(page, gui::kSettingsPopupOnlyGroupTitle, "Ray allocation"));
      IM_CHECK(!page.settings.empty());
      IM_CHECK_STR_EQ(page.settings.back().title.c_str(), gui::kSettingsPopupOnlyGroupTitle);

      ctx->ItemClick(kButtonRef);
      ctx->Yield(2);
    };
  }
}
