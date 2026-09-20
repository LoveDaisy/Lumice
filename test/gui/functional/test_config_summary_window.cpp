// The Summary window (src/gui/config_summary_window.cpp) as the user reaches it: the top-bar
// button opens and closes it, its own X closes it, every document reset closes it, and what it
// draws is the page config_summary.cpp built — one table per group, one row per field — so the
// rendered row count is the page's own count and not a second enumeration.
//
// The page's CONTENT is asserted without a frame elsewhere (test_config_summary_rows.cpp for the
// tier rule, test_config_summary_export_parity_chain.cpp for value parity with the export); this
// file only holds what needs the window on screen.

#include <string>

#include "IconsFontAwesome6.h"
#include "gui/app.hpp"
#include "gui/config_summary.hpp"
#include "gui/config_summary_window.hpp"
#include "test_gui_shared.hpp"

namespace {

constexpr const char* kButtonRef = "##TopBar/" ICON_FA_FILE_LINES " Summary";
constexpr const char* kWindowRef = "###ConfigSummary";

// The window as ImGui drew it this frame, or null when it is not being submitted.
ImGuiWindow* SummaryWindow(ImGuiTestContext* ctx) {
  ImGuiWindow* win = ctx->GetWindowByRef(kWindowRef);
  return (win != nullptr && win->WasActive) ? win : nullptr;
}

// Rows across every group table the window drew this frame. ImGuiTable::CurrentRow is the index
// of the last row submitted, which survives EndTable, so rows = CurrentRow + 1 per table.
int RenderedRowCount(ImGuiTestContext* ctx, int group_count) {
  ImGuiWindow* win = SummaryWindow(ctx);
  if (win == nullptr) {
    return -1;
  }
  int rows = 0;
  for (int i = 0; i < group_count; ++i) {
    const std::string id = "##summary_group_" + std::to_string(i);
    ImGuiTable* table = ImGui::TableFindByID(win->GetID(id.c_str()));
    if (table == nullptr) {
      return -1;
    }
    rows += table->CurrentRow + 1;
  }
  return rows;
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
    // What is drawn is the page: as many group tables as the page has groups, as many rows in
    // them as the page has fields, and the count moves with the document.
    ImGuiTest* t = IM_REGISTER_TEST(engine, "config_summary", "rendered_rows_are_the_pages_fields");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->ItemClick(kButtonRef);
      ctx->Yield(3);
      IM_CHECK(SummaryWindow(ctx) != nullptr);

      const gui::ConfigSummary default_page = gui::BuildConfigSummary(gui::g_state);
      const int default_groups = static_cast<int>(default_page.settings.size() + default_page.document.size());
      const int default_fields = gui::CountConfigSummaryFields(default_page);
      IM_CHECK_GT(default_fields, 0);
      IM_CHECK_EQ(RenderedRowCount(ctx, default_groups), default_fields);
      // One table more than the groups must NOT exist: the window draws exactly the page.
      IM_CHECK(ImGui::TableFindByID(SummaryWindow(ctx)->GetID(
                   ("##summary_group_" + std::to_string(default_groups)).c_str())) == nullptr);

      // The window is fixed-width and reads the live document: a bigger document adds rows in
      // place, without the window growing sideways.
      const float width_before = SummaryWindow(ctx)->Size.x;
      SeedTwoLayerDocument();
      ctx->Yield(3);
      const gui::ConfigSummary page = gui::BuildConfigSummary(gui::g_state);
      const int groups = static_cast<int>(page.settings.size() + page.document.size());
      IM_CHECK_GT(groups, default_groups);
      IM_CHECK_EQ(RenderedRowCount(ctx, groups), gui::CountConfigSummaryFields(page));
      IM_CHECK_EQ(SummaryWindow(ctx)->Size.x, width_before);
      // Height follows the content up to the work area, and the content scrolls past that.
      const ImGuiWindow* win = SummaryWindow(ctx);
      IM_CHECK_LE(win->Size.y, ImGui::GetMainViewport()->WorkSize.y);
      IM_CHECK(win->ScrollMax.y > 0.0f || win->ContentSize.y <= win->Size.y);

      ctx->ItemClick(kButtonRef);
      ctx->Yield(2);
    };
  }
}
