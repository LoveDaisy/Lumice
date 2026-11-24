// The Edit modal's Filter tab: the sum-of-products row editor and the controls beside it.
//
// What this suite is for: the row editor is a buffer, not the document. Rows are authored into a
// scratch list, validated per row, and only materialised into the entry's FilterConfig when OK
// commits them — with Cancel, Remove Filter and Immediate mode each taking a different route
// through that seam. Falsifying any of it needs a real modal on screen and real typing: the
// buffers are file-scope statics inside edit_modals.cpp with no seam to write them from a test,
// which is exactly the point (a test that could set them directly would not be testing the
// editor). What the grammar means once committed is settled elsewhere and deliberately not
// re-asserted here — ValidateSummandText in unit-correctness/gui/test_filter_sop_grammar.cpp, and
// the reconstruction and clause arithmetic in
// composition-correctness/gui/test_filter_reconstruct_chain.cpp.
//
// What a user sees when these break: OK stays grey with no way to tell which row is at fault, or
// worse goes live on a row that cannot commit; deleting one OR row silently edits a different
// one; Remove Filter appears to work and the filter comes back on the next save; a forgotten
// blank row turns a filter into a match-all, which reads as "my filter does nothing" under
// filter_in and as an all-black render under filter_out.
//
// Five propositions from the catalogue are NOT covered here and the reason is mechanical, the
// same one recorded for the Colors window: the per-row validation hints, the static syntax hint,
// the live SoP preview and the "Clauses: N / M" line are all drawn with TextColored /
// TextWrapped / TextDisabled / TextUnformatted, which reach ItemAdd() with id == 0 and are never
// registered with the test engine; and the row's validation tint is a style colour, not an item
// flag, so ItemInfo cannot see it either. Their user-visible consequence IS covered, by the OK
// gate below — that gate is driven by the same per-row validation the hints report, so a
// validation regression surfaces here even though the text does not. The clause count's own
// arithmetic is covered a layer down.

#include <string>
#include <vector>

#include "IconsFontAwesome6.h"
#include "gui/app.hpp"
#include "gui/edit_modal_rules.hpp"  // kMaxSummandRows — the cap the add-row button enforces
#include "gui/edit_modals.hpp"       // IsCurrentModalDApplicable
#include "gui/file_io.hpp"           // SaveLmcFile / LoadLmcFile
#include "gui/gui_state.hpp"
#include "gui/raypath_segments.hpp"  // ParseSummandText / FormatSopExpansionPreview — the preview's own spelling
#include "test_gui_shared.hpp"

namespace {

const char* const kOk = "**/" ICON_FA_CHECK " OK##edit_modal";
const char* const kCancel = "**/" ICON_FA_XMARK " Cancel##edit_modal";
const char* const kAddRow = "**/+ Add OR row##summand_add";

// Open the entry's Edit modal on its Filter tab. Four frames because the tab's SetSelected only
// takes effect on the frame after the popup opens.
void OpenFilterModal(ImGuiTestContext* ctx) {
  OpenCardEditor(ctx, 0, kFilterTabRef);
  ctx->Yield(4);
}

bool OkIsDisabled(ImGuiTestContext* ctx) {
  return IsDisabled(ctx->ItemInfo(kOk));
}

// Give the entry a committed filter to edit, so a case can start from "there is something here"
// rather than from the blank editor.
void SeedRaypathFilter(const char* text) {
  gui::FilterConfig f;
  f.SetRaypath(gui::RaypathParams{ text });
  gui::SetFilter(gui::g_state, gui::g_state.layers[0].entries[0], f);
}

const gui::FilterConfig* CommittedFilter() {
  const auto& entry = gui::g_state.layers[0].entries[0];
  return entry.filter_id.has_value() ? &gui::g_state.filters[static_cast<size_t>(*entry.filter_id)] : nullptr;
}

// Type `text` into the row InputText at `ref` and leave the box focused: no Enter at the end.
// ItemInputValue would end with KeyCharsReplaceEnter, and under the keyboard flow an Enter inside
// a row now inserts a blank row below it — the fixture's edits must end without one, so what a
// case seeded is exactly what it later asserts.
void EditRowText(ImGuiTestContext* ctx, const char* ref, const char* text) {
  ctx->ItemInput(ref);
  ctx->KeyCharsReplace(text);
  ctx->Yield(2);
}

// Type `text` into row `uid`, adding a row first when the caller is past row 0.
void AuthorRow(ImGuiTestContext* ctx, int uid, const char* text) {
  if (uid > 0) {
    ctx->ItemClick(kAddRow);
    ctx->Yield(2);
  }
  EditRowText(ctx, ("**/##row_text_" + std::to_string(uid)).c_str(), text);
}

// The editor's live row texts, blank rows included — the only row-level observation surface (the
// row buffers are edit_modals.cpp file-scope statics with no write seam, by design). While a row's
// InputText is active its buffer is the edit buffer, so this reads what a real keyboard typed.
std::vector<std::string> RowTexts() {
  return gui::GetEditModalBuffers().filter_rows;
}

}  // namespace

void RegisterFilterEditorTests(ImGuiTestEngine* engine) {
  // ---------------------------------------------------------------------------------------------
  // The OK gate. This is the whole validation story's one addressable surface.
  // ---------------------------------------------------------------------------------------------

  // A row that cannot commit must stop the commit, and stop it in a recoverable way. Both halves
  // are needed: a gate that never releases is as broken as one that never closes, and the second
  // half is what proves the gate is reading the live buffer rather than latching once.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "ok_is_gated_by_a_row_that_cannot_commit");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      IM_CHECK(!OkIsDisabled(ctx));  // a lone blank row is match-all, not an error

      EditRowText(ctx, "**/##row_text_0", "3-");  // incomplete: a path with nothing after the dash
      ctx->Yield(2);
      IM_CHECK(OkIsDisabled(ctx));

      EditRowText(ctx, "**/##row_text_0", "3-5");
      ctx->Yield(2);
      IM_CHECK(!OkIsDisabled(ctx));  // released again, so the gate re-reads every frame

      ctx->ItemClick(kOk);
      ctx->Yield(2);
      const auto* f = CommittedFilter();
      IM_CHECK(f != nullptr);
      IM_CHECK_STR_EQ(f->param[0].text.c_str(), "3-5");
    };
  }

  // A blank row beside a real one must not block either — blank means "no constraint from this
  // row", and treating it as an error would make the add-row button a trap.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "a_blank_row_beside_a_real_one_does_not_gate_ok");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "3-5");
      ctx->ItemClick(kAddRow);
      ctx->Yield(2);
      IM_CHECK(ctx->ItemExists("**/##row_text_1"));
      IM_CHECK(!OkIsDisabled(ctx));
      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // The clause-count line is a diagnostic, not a gate — deliberately so, and the distinction is
  // only observable on OK's flag since the line itself is unaddressable text. An over-cap row must
  // leave OK live: the ABI limit is enforced at commit, and turning the preview into a second
  // enforcement point would strand the user with a disabled button and an unreadable reason.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "an_over_cap_clause_count_warns_without_gating_ok");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);

      // Four factors of nine alternatives each: 6561 clauses, well past LUMICE_MAX_CONFIG_CLAUSES.
      EditRowText(ctx, "**/##row_text_0",
                  "1;2;3;4;5;6;7;8;3-4 & 1;2;3;4;5;6;7;8;3-4 & 1;2;3;4;5;6;7;8;3-4 & 1;2;3;4;5;6;7;8;3-4");
      ctx->Yield(3);
      IM_CHECK(!OkIsDisabled(ctx));
      // And the modal survived rendering that preview — the over-cap branch pushes a style colour
      // it must also pop, and an unbalanced pair takes the whole popup down with it.
      IM_CHECK(ctx->ItemExists(kCancel));

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // ---------------------------------------------------------------------------------------------
  // The row list.
  // ---------------------------------------------------------------------------------------------

  // The editor keeps at least one row, but it keeps it by refilling, not by greying the last
  // delete button: deleting the last row lands on the same one-blank-row state as clearing its
  // text, and commits the same way (no filter). A greyed button used to stand in for that
  // invariant and gave the user nothing to explain why. Rebuilt from a committed filter first so
  // the rows deleted are real ones, and the refilled row carries a fresh uid — a stale uid would
  // mean a survivor was recycled rather than a blank appended.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "deleting_down_to_empty_refills_a_blank_row");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "1-2");
      AuthorRow(ctx, 1, "3-4");
      AuthorRow(ctx, 2, "5-6");
      ctx->ItemClick(kOk);
      ctx->Yield(2);
      const auto* seeded = CommittedFilter();
      IM_CHECK(seeded != nullptr);
      IM_CHECK_EQ(seeded->param.size(), static_cast<size_t>(3));

      OpenFilterModal(ctx);  // rows 0..2 rebuilt from the committed filter
      ctx->ItemClick("**/" ICON_FA_XMARK "##row_delete_2");
      ctx->Yield(2);
      ctx->ItemClick("**/" ICON_FA_XMARK "##row_delete_1");
      ctx->Yield(2);
      IM_CHECK(ctx->ItemExists("**/##row_text_0"));
      IM_CHECK(!ctx->ItemExists("**/##row_text_1"));
      IM_CHECK(!IsDisabled(ctx->ItemInfo("**/" ICON_FA_XMARK "##row_delete_0")));  // the last row's x is live

      ctx->ItemClick("**/" ICON_FA_XMARK "##row_delete_0");
      ctx->Yield(2);
      IM_CHECK(!ctx->ItemExists("**/##row_text_0"));
      IM_CHECK(ctx->ItemExists("**/##row_text_3"));  // exactly one row back, and a new one
      IM_CHECK(!ctx->ItemExists("**/##row_text_4"));
      IM_CHECK(!IsDisabled(ctx->ItemInfo("**/" ICON_FA_XMARK "##row_delete_3")));
      IM_CHECK(!OkIsDisabled(ctx));  // a lone blank row is match-all, not an error

      ctx->ItemClick(kOk);
      ctx->Yield(2);
      IM_CHECK(CommittedFilter() == nullptr);  // same result as blanking the last row's text
    };
  }

  // Row widgets are keyed by a per-row uid rather than by position, which is what stops a
  // middle-row delete from re-colliding the ImGui id stack and pouring one row's buffer into
  // another's widget. Authoring distinct text into each row is what makes that observable: if the
  // ids shifted, the survivors' text would follow the wrong widget.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "deleting_a_middle_row_leaves_the_survivors_alone");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "1-2");
      AuthorRow(ctx, 1, "3-4");
      AuthorRow(ctx, 2, "5-6");

      ctx->ItemClick("**/" ICON_FA_XMARK "##row_delete_1");  // the middle one
      ctx->Yield(2);
      IM_CHECK(ctx->ItemExists("**/##row_text_0"));
      IM_CHECK(!ctx->ItemExists("**/##row_text_1"));
      IM_CHECK(ctx->ItemExists("**/##row_text_2"));  // uid 2 did NOT slide down into slot 1

      ctx->ItemClick(kOk);
      ctx->Yield(2);
      const auto* f = CommittedFilter();
      IM_CHECK(f != nullptr);
      IM_CHECK_EQ(f->param.size(), static_cast<size_t>(2));
      IM_CHECK_STR_EQ(f->param[0].text.c_str(), "1-2");
      IM_CHECK_STR_EQ(f->param[1].text.c_str(), "5-6");
    };
  }

  // The soft cap was raised from 16 to 256, and the button must stay live across the old boundary
  // — that boundary is where a stale constant would still be hiding.
  //
  // Honest scope: this walks to 20 rows, not to the 256-row cap, so it falsifies "the old limit is
  // still in force" but not "the new limit is enforced". AtSummandRowCap is a pure predicate on
  // the row count and is the single owner of both answers; clicking 256 times to see the button
  // grey out would buy the second half at roughly ten times the runtime of every other case here.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "add_row_stays_live_past_the_old_sixteen_row_limit");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      IM_CHECK(gui::kMaxSummandRows > 20u);  // the walk below must stay below the real cap

      std::string disabled_at;
      for (int i = 1; i < 20; ++i) {
        if (IsDisabled(ctx->ItemInfo(kAddRow))) {
          disabled_at += " " + std::to_string(i);
        }
        ctx->ItemClick(kAddRow);
        ctx->Yield(2);
      }
      IM_CHECK_STR_EQ(disabled_at.c_str(), "");
      IM_CHECK(ctx->ItemExists("**/##row_text_16"));  // the 17th row exists at all
      IM_CHECK(ctx->ItemExists("**/##row_text_19"));
      IM_CHECK(!IsDisabled(ctx->ItemInfo(kAddRow)));

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // ---------------------------------------------------------------------------------------------
  // The keyboard flow: Enter inserts a blank row below and focuses it, Esc cancels — deleting the
  // row when the cancel leaves it empty — and navigation only navigates. Every case here drives a
  // real click/keystroke per step (no direct fixture writes), because the propositions are about
  // where real input lands, not about the row list's shape alone.
  // ---------------------------------------------------------------------------------------------

  // Enter from a middle row: the blank goes between the two existing rows with their texts and
  // order intact, and the following keystrokes land in the inserted row WITHOUT any click — the
  // focus request is served, not just armed.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor",
                                    "enter_from_a_middle_row_inserts_below_and_typing_lands_in_the_new_row");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "1-2");
      AuthorRow(ctx, 1, "3-4");

      ctx->ItemInput("**/##row_text_0");  // real focus on the first row
      ctx->Yield(2);
      ctx->KeyPress(ImGuiKey_Enter);
      ctx->Yield(4);

      auto rows = RowTexts();
      IM_CHECK_EQ(rows.size(), static_cast<size_t>(3));
      IM_CHECK_STR_EQ(rows[0].c_str(), "1-2");
      IM_CHECK_STR_EQ(rows[1].c_str(), "");
      IM_CHECK_STR_EQ(rows[2].c_str(), "3-4");
      IM_CHECK_EQ(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_2").ID);  // uid 2: inserted, not shifted

      ctx->KeyChars("9-9");  // no click first: the keystrokes themselves locate the new row
      ctx->Yield(3);
      rows = RowTexts();
      IM_CHECK_STR_EQ(rows[1].c_str(), "9-9");
      IM_CHECK_STR_EQ(rows[0].c_str(), "1-2");  // the source row did not absorb them
      IM_CHECK_STR_EQ(rows[2].c_str(), "3-4");

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // Enter below the last row, and Enter again on the blank it created: a blank row still inserts
  // (one rule, no special case), each creation stacking below its source.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "filter_editor", "enter_below_the_last_row_and_below_a_blank_row_keeps_creating");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "5-6");
      ctx->ItemInput("**/##row_text_0");
      ctx->KeyPress(ImGuiKey_Enter);
      ctx->Yield(4);
      ctx->KeyPress(ImGuiKey_Enter);  // from the focused blank row
      ctx->Yield(4);

      auto rows = RowTexts();
      IM_CHECK_EQ(rows.size(), static_cast<size_t>(3));
      IM_CHECK_STR_EQ(rows[0].c_str(), "5-6");
      IM_CHECK_STR_EQ(rows[1].c_str(), "");
      IM_CHECK_STR_EQ(rows[2].c_str(), "");
      IM_CHECK_EQ(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_2").ID);
      ctx->KeyChars("7-8");
      ctx->Yield(3);
      IM_CHECK_STR_EQ(RowTexts()[2].c_str(), "7-8");

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // Esc on the created blank cancels the creation: the row is gone and focus is back on the Enter
  // source, proven by typing through the keyboard alone afterwards.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "filter_editor", "esc_on_a_new_blank_row_deletes_it_and_focus_returns_to_the_source");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "1-2");
      ctx->ItemInput("**/##row_text_0");
      ctx->KeyCharsReplace("3-4");
      ctx->KeyPress(ImGuiKey_Enter);
      ctx->Yield(4);
      IM_CHECK_EQ(RowTexts().size(), static_cast<size_t>(2));  // created blank present

      ctx->KeyPress(ImGuiKey_Escape);
      ctx->Yield(4);
      auto rows = RowTexts();
      IM_CHECK_EQ(rows.size(), static_cast<size_t>(1));
      IM_CHECK_STR_EQ(rows[0].c_str(), "3-4");
      IM_CHECK_EQ(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_0").ID);

      ctx->KeyCharsReplace("9-9");
      ctx->Yield(3);
      IM_CHECK_STR_EQ(RowTexts()[0].c_str(), "9-9");

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // Nested creations unwind one level per Esc: blank-from-row, blank-from-blank, then two Escs
  // walk back through the sources instead of emptying the list at once.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "nested_creates_unwind_one_esc_at_a_time");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      ctx->ItemInput("**/##row_text_0");
      ctx->KeyCharsReplace("1-2");
      ctx->KeyPress(ImGuiKey_Enter);
      ctx->Yield(4);
      ctx->KeyPress(ImGuiKey_Enter);
      ctx->Yield(4);
      IM_CHECK_EQ(RowTexts().size(), static_cast<size_t>(3));

      ctx->KeyPress(ImGuiKey_Escape);  // deletes the blank-from-blank, back to the first blank
      ctx->Yield(4);
      IM_CHECK_EQ(RowTexts().size(), static_cast<size_t>(2));
      IM_CHECK_EQ(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_1").ID);

      ctx->KeyPress(ImGuiKey_Escape);  // deletes the first blank, back to the authored row
      ctx->Yield(4);
      IM_CHECK_EQ(RowTexts().size(), static_cast<size_t>(1));
      IM_CHECK_EQ(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_0").ID);

      ctx->KeyCharsReplace("7-7");
      ctx->Yield(3);
      IM_CHECK_STR_EQ(RowTexts()[0].c_str(), "7-7");

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // Esc on a row that is not empty after the cancel keeps the row and restores its pre-edit text —
  // the plain ImGui revert, which the deletion rule must not override. Covering both a junk edit
  // and a cleared-to-empty edit: only the POST-cancel text decides.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "esc_on_a_nonempty_row_reverts_and_keeps_it");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "3-5");
      // The revert restores the buffer as it was at focus-gain, so the case must deactivate first
      // and re-focus WITH "3-5" already in the row — the red run of this file caught the first
      // version reverting to "" because the fixture's typing had never re-activated the box.
      ctx->KeyPress(ImGuiKey_Tab);
      ctx->Yield(2);
      ctx->ItemInput("**/##row_text_0");
      ctx->KeyCharsReplace("zzz");
      ctx->KeyPress(ImGuiKey_Escape);
      ctx->Yield(4);
      auto rows = RowTexts();
      IM_CHECK_EQ(rows.size(), static_cast<size_t>(1));
      IM_CHECK_STR_EQ(rows[0].c_str(), "3-5");

      ctx->ItemInput("**/##row_text_0");
      ctx->KeyCharsReplace("");        // cleared while editing...
      ctx->KeyPress(ImGuiKey_Escape);  // ...but the cancel restores, so the row survives
      ctx->Yield(4);
      rows = RowTexts();
      IM_CHECK_EQ(rows.size(), static_cast<size_t>(1));
      IM_CHECK_STR_EQ(rows[0].c_str(), "3-5");

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // Esc on the editor's own initial blank row deletes it; the ≥1-row invariant refills one blank
  // with a FRESH uid (not a recycled id), focus lands on the refill, and it is directly editable.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "filter_editor", "esc_on_the_only_blank_row_refills_it_and_it_stays_editable");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);  // opens with exactly one blank row, uid 0
      ctx->ItemInput("**/##row_text_0");
      ctx->KeyPress(ImGuiKey_Escape);
      ctx->Yield(4);

      IM_CHECK(!ctx->ItemExists("**/##row_text_0"));
      IM_CHECK(ctx->ItemExists("**/##row_text_1"));
      IM_CHECK_EQ(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_1").ID);

      ctx->KeyCharsReplace("3-5");
      ctx->Yield(3);
      IM_CHECK_STR_EQ(RowTexts()[0].c_str(), "3-5");

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // At the soft cap Enter must be a no-op — the same predicate the Add button reads — while at
  // cap−1 it still inserts. Seeded through the document model: no clicking to the cap.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "enter_at_the_row_cap_inserts_nothing");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      {
        gui::FilterConfig f;
        const gui::SummandText row{ "1-2", gui::ParseSummandText("1-2") };
        f.param.assign(gui::kMaxSummandRows - 1, row);
        gui::SetFilter(gui::g_state, gui::g_state.layers[0].entries[0], f);
      }
      ctx->Yield();
      OpenFilterModal(ctx);
      IM_CHECK_EQ(RowTexts().size(), static_cast<size_t>(gui::kMaxSummandRows - 1));

      ctx->ItemInput("**/##row_text_0");
      ctx->KeyPress(ImGuiKey_Enter);
      ctx->Yield(4);
      IM_CHECK_EQ(RowTexts().size(), static_cast<size_t>(gui::kMaxSummandRows));  // cap−1 → cap works
      IM_CHECK_STR_EQ(RowTexts()[1].c_str(), "");                                 // ...and it was the Enter

      ctx->KeyPress(ImGuiKey_Enter);  // now at cap, from the focused inserted row
      ctx->Yield(4);
      IM_CHECK_EQ(RowTexts().size(), static_cast<size_t>(gui::kMaxSummandRows));
      IM_CHECK(!ctx->ItemExists(("**/##row_text_" + std::to_string(gui::kMaxSummandRows)).c_str()));
      // The Add button's own grey-at-cap face is not asserted here: with 256 rows the live preview
      // pushes that button to the middle of ~10000px of content, so it sits outside the pane's
      // clip at either scroll end and is invisible to the test engine (measured: pane scrollmax
      // 9735, button unregistered at scroll bottom). The predicate it reads (AtSummandRowCap at
      // cap-1/cap/cap+1) is unit-owned in test_gui_widget_rules.cpp, and the Enter side of the cap
      // is what this case exists to witness.

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // Focus loss never deletes: Tab across the new blank keeps it, Shift+Tab comes back and the row
  // is editable by keys alone, and clicking another control keeps it too. (A window-level
  // deactivation is not drivable in-process; the retention has no focus-reason-specific path to
  // miss — see the Enter/Esc handler's shape.)
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "filter_editor", "focus_loss_keeps_the_new_blank_row_and_nav_returns_by_tab");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "1-2");
      ctx->ItemInput("**/##row_text_0");
      ctx->KeyPress(ImGuiKey_Enter);
      ctx->Yield(4);
      IM_CHECK(ctx->ItemExists("**/##row_text_1"));

      ctx->KeyPress(ImGuiKey_Tab);
      ctx->Yield(4);
      IM_CHECK(ctx->ItemExists("**/##row_text_1"));  // navigating away does not delete
      IM_CHECK_NE(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_1").ID);

      ctx->KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
      ctx->Yield(4);
      IM_CHECK_EQ(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_1").ID);
      ctx->KeyChars("5-6");
      ctx->Yield(3);
      IM_CHECK_STR_EQ(RowTexts()[1].c_str(), "5-6");

      ctx->ItemClick("**/Filter In##filter_action");
      ctx->Yield(3);
      IM_CHECK(ctx->ItemExists("**/##row_text_1"));
      IM_CHECK_STR_EQ(RowTexts()[1].c_str(), "5-6");

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // A parked focus request is session-local: switching tabs parks it for good, closing and
  // reopening must not let it (or any zombie focus) land on the rebuilt rows.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "filter_editor", "a_parked_focus_request_does_not_outlive_its_tab_or_modal_session");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      ctx->ItemInput("**/##row_text_0");
      ctx->KeyCharsReplace("1-2");
      ctx->KeyPress(ImGuiKey_Enter);
      ctx->Yield(4);
      IM_CHECK(ctx->ItemExists("**/##row_text_1"));

      ctx->ItemClick(kCrystalTabRef);
      ctx->Yield(4);
      ctx->ItemClick(kFilterTabRef);
      ctx->Yield(4);
      IM_CHECK(ctx->ItemExists("**/##row_text_1"));                            // the blank survived the trip
      IM_CHECK_NE(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_1").ID);  // ...without the focus

      ctx->ItemClick(kCancel);
      ctx->Yield(4);
      OpenFilterModal(ctx);
      IM_CHECK_NE(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_0").ID);  // rebuild stole nothing
      ctx->ItemInput("**/##row_text_0");
      ctx->KeyCharsReplace("2-4");
      ctx->Yield(3);
      IM_CHECK_STR_EQ(RowTexts()[0].c_str(), "2-4");

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // Remove Filter rebuilds the row list AND rewinds the uid counter — the exact shape that could
  // turn a stale focus request into a stolen focus. The reset must leave nothing armed.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "filter_editor", "remove_filter_resets_the_row_session_without_leaking_focus");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      SeedRaypathFilter("3-5");
      ctx->Yield();
      OpenFilterModal(ctx);
      ctx->ItemInput("**/##row_text_0");
      ctx->KeyPress(ImGuiKey_Enter);
      ctx->Yield(4);
      IM_CHECK(ctx->ItemExists("**/##row_text_1"));

      ctx->ItemClick("**/Remove Filter##filter");
      ctx->Yield(4);
      IM_CHECK(ctx->ItemExists("**/##row_text_0"));  // rebuilt from the counter rewind
      IM_CHECK(!ctx->ItemExists("**/##row_text_1"));
      IM_CHECK_EQ(ImGui::GetActiveID(), (ImGuiID)0);  // nothing grabbed the fresh row

      ctx->ItemInput("**/##row_text_0");
      ctx->KeyCharsReplace("1-1");
      ctx->Yield(3);
      IM_CHECK_STR_EQ(RowTexts()[0].c_str(), "1-1");

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // The commit edge of the flow: a keyboard-created blank row survives real focus loss and is then
  // dropped by the same blank-stripping commit as any other — the filter commits as written.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor",
                                    "keyboard_created_blank_rows_are_dropped_on_ok_after_surviving_focus_loss");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "3-5");
      ctx->ItemInput("**/##row_text_0");
      ctx->KeyPress(ImGuiKey_Enter);
      ctx->Yield(4);
      IM_CHECK(ctx->ItemExists("**/##row_text_1"));

      ctx->KeyPress(ImGuiKey_Tab);  // focus loss first: the blank must live to reach OK
      ctx->Yield(4);
      IM_CHECK(ctx->ItemExists("**/##row_text_1"));

      ctx->ItemClick(kOk);
      ctx->Yield(2);
      const auto* f = CommittedFilter();
      IM_CHECK(f != nullptr);
      IM_CHECK_EQ(f->param.size(), static_cast<size_t>(1));
      IM_CHECK_STR_EQ(f->param[0].text.c_str(), "3-5");
    };
  }

  // The handoff must not eat keystrokes: the first character typed right after Enter — arriving on
  // the frame the focus request is still crossing between boxes — lands in the inserted row, whole.
  // No settling yields before the typing: this case exists to fail on the transition-frame drop.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "filter_editor", "the_first_keystroke_after_enter_lands_in_the_inserted_row");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "1-2");
      AuthorRow(ctx, 1, "3-4");
      ctx->ItemInput("**/##row_text_0");
      ctx->Yield(2);
      // Frame-exact choreography: the Enter is consumed on the frame the row is inserted, and the
      // two keystrokes are queued for the handoff frame and the activation frame — the two frames
      // a loose "press Enter, wait, then type" sequence would have sailed over.
      ctx->KeySetEx(ImGuiKey_Enter, true, 0.0f);
      ctx->Yield();  // the Enter is consumed here: the row is inserted, the focus request armed
      ctx->KeySetEx(ImGuiKey_Enter, false, 0.0f);  // release: a held Enter auto-repeats and keeps inserting
      ctx->KeyChars("9");                          // frame after the insert: the handoff frame itself
      ctx->KeyChars("8");                          // the activation frame
      ctx->Yield(2);
      ctx->KeyChars("7");  // normal typing AFTER the handoff settled: must append, not replace
      ctx->Yield(3);

      auto rows = RowTexts();
      IM_CHECK_EQ(rows.size(), static_cast<size_t>(3));
      IM_CHECK_STR_EQ(rows[1].c_str(), "987");  // all three keystrokes: none eaten, none replaced
      IM_CHECK_STR_EQ(rows[0].c_str(), "1-2");  // the source row did not absorb them
      IM_CHECK_STR_EQ(rows[2].c_str(), "3-4");
      IM_CHECK_EQ(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_2").ID);

      // And the undo baseline stays clean: the parked keystrokes were an edit of the new row, not
      // its original content — a cancel restores empty and deletes the creation.
      ctx->KeyPress(ImGuiKey_Escape);
      ctx->Yield(4);
      rows = RowTexts();
      IM_CHECK_EQ(rows.size(), static_cast<size_t>(2));
      IM_CHECK_STR_EQ(rows[0].c_str(), "1-2");
      IM_CHECK_STR_EQ(rows[1].c_str(), "3-4");
      IM_CHECK_EQ(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_0").ID);  // back to the source

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // The identity witness: with the new row ACTUALLY activated first, the cancel hands focus back
  // to the non-empty source — and an Esc landing during THAT refocus handoff must not read the
  // crossing as a delete authorization. The source row keeps its uid and its text; no blank is
  // refilled behind it. This is the case a pending-presence delete used to fail: it erased the
  // "1-2" row and refilled a blank, losing the user's expression.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "esc_during_the_refocus_handoff_keeps_a_nonempty_source");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "1-2");
      ctx->ItemInput("**/##row_text_0");
      ctx->Yield(2);
      ctx->KeySetEx(ImGuiKey_Enter, true, 0.0f);
      ctx->Yield();  // frame N: the blank is inserted, the focus request armed
      ctx->KeySetEx(ImGuiKey_Enter, false, 0.0f);
      ctx->Yield(2);
      IM_CHECK_EQ(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_1").ID);  // premise: the new row took the editor

      ctx->KeySetEx(ImGuiKey_Escape, true, 0.0f);  // frame M: the activated blank is cancelled
      ctx->Yield();
      ctx->KeySetEx(ImGuiKey_Escape, false, 0.0f);
      ctx->KeySetEx(ImGuiKey_Escape, true, 0.0f);  // frame M+1: the refocus handoff crossing itself
      ctx->Yield();
      ctx->KeySetEx(ImGuiKey_Escape, false, 0.0f);
      ctx->Yield(4);

      auto rows = RowTexts();
      IM_CHECK_EQ(rows.size(), static_cast<size_t>(1));
      IM_CHECK(ctx->ItemExists("**/##row_text_0"));  // the SOURCE row: same uid, not a refill
      IM_CHECK(!ctx->ItemExists("**/##row_text_1"));
      IM_CHECK(!ctx->ItemExists("**/##row_text_2"));  // no blank refilled behind the kept row
      IM_CHECK_STR_EQ(rows[0].c_str(), "1-2");        // the text survived the second Esc

      ctx->ItemInput("**/##row_text_0");  // and the kept row is still directly editable
      ctx->KeyCharsReplace("1-9");
      ctx->Yield(3);
      IM_CHECK_STR_EQ(RowTexts()[0].c_str(), "1-9");

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // The two-end-cancel witness: Esc landing on the ACTIVATION frame — after the focus request was
  // submitted and while the toolkit activation is in flight — must cancel the creation AND that
  // in-flight activation, so the deleted row never becomes the active editor and the source
  // refocus survives it. Typing straight afterwards (no click) proves the keyboard actually
  // returned to the source; a dead-row active id used to eat the refocus here.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "filter_editor", "esc_on_the_activation_frame_returns_keyboard_to_the_source");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "1-2");
      AuthorRow(ctx, 1, "3-4");
      ctx->ItemInput("**/##row_text_0");
      ctx->Yield(2);
      ctx->KeySetEx(ImGuiKey_Enter, true, 0.0f);
      ctx->Yield();  // frame N: the blank is inserted, the focus request armed
      ctx->KeySetEx(ImGuiKey_Enter, false, 0.0f);
      ctx->KeyChars("9");  // frame N+1: the crossing frame — accepted into the crossing, undelivered
      ctx->KeySetEx(ImGuiKey_Escape, true, 0.0f);  // frame N+2: the ACTIVATION frame — cancel + revoke in flight
      ctx->Yield();
      ctx->KeySetEx(ImGuiKey_Escape, false, 0.0f);
      ctx->Yield(4);

      auto rows = RowTexts();
      IM_CHECK_EQ(rows.size(), static_cast<size_t>(2));  // the creation is gone, neighbours intact
      IM_CHECK_STR_EQ(rows[0].c_str(), "1-2");           // "9" was cancelled input: dropped, not delivered
      IM_CHECK_STR_EQ(rows[1].c_str(), "3-4");
      IM_CHECK(!ctx->ItemExists("**/##row_text_2"));
      IM_CHECK_EQ(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_0").ID);  // refocus survived the dead row

      ctx->KeyChars("7");  // no click: the keyboard must already be back on the source
      ctx->Yield(3);
      IM_CHECK_STR_EQ(RowTexts()[0].c_str(), "1-27");
      IM_CHECK_EQ(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_0").ID);

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // The frame AFTER the activation is part of the handoff too: the activation frame itself skips
  // key processing (a just-activated editor registers no shortcuts), so an Esc landing on the
  // next frame must still find the transaction owning the keyboard — revert to the pre-crossing
  // baseline, delete the empty creation, hand the keyboard back. Without that coverage the cancel
  // dies in the navigation cancel (box cleared without a revert, text kept, focus lost).
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "filter_editor", "esc_on_the_frame_after_activation_still_cancels_the_creation");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "1-2");
      AuthorRow(ctx, 1, "3-4");
      ctx->ItemInput("**/##row_text_0");
      ctx->Yield(2);
      ctx->KeySetEx(ImGuiKey_Enter, true, 0.0f);
      ctx->Yield();  // frame N: the blank is inserted, the focus request armed
      ctx->KeySetEx(ImGuiKey_Enter, false, 0.0f);
      ctx->KeyChars("9");                          // frame N+1: the crossing frame
      ctx->KeyChars("8");                          // frame N+2: the activation frame
      ctx->KeySetEx(ImGuiKey_Escape, true, 0.0f);  // frame N+3: the frame after activation
      ctx->Yield();
      ctx->KeySetEx(ImGuiKey_Escape, false, 0.0f);
      ctx->Yield(4);

      auto rows = RowTexts();
      IM_CHECK_EQ(rows.size(), static_cast<size_t>(2));
      IM_CHECK_STR_EQ(rows[0].c_str(), "1-2");  // "98" was cancelled input: reverted, not kept
      IM_CHECK_STR_EQ(rows[1].c_str(), "3-4");
      IM_CHECK(!ctx->ItemExists("**/##row_text_2"));
      IM_CHECK_EQ(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_0").ID);  // keyboard handed back

      ctx->KeyChars("7");  // no click: the source is the editor again
      ctx->Yield(3);
      IM_CHECK_STR_EQ(RowTexts()[0].c_str(), "1-27");

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // A click that lands AFTER the crossing's submission, within the same frame, must keep its
  // effect: the request the crossing just submitted is still unapplied, and the next frame's
  // activation would otherwise ClearActiveID the clicked control and swallow the press. The
  // clicked control here is the refilled row's own "x" button — its rect is stable across the
  // refill (the fresh uid lands on the identical geometry), and it renders after the row's text
  // box, which is exactly the submit-then-click order the ownership check exists for.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "filter_editor", "a_same_frame_click_after_the_submission_keeps_the_user_s_choice");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);  // the initial blank row, uid 0
      ctx->ItemInput("**/##row_text_0");
      ctx->Yield(2);
      ctx->MouseMoveToPos(ctx->ItemInfo("**/" ICON_FA_XMARK "##row_delete_0").RectFull.GetCenter());
      ctx->Yield(2);  // hover the row's "x": the refill will reuse this exact rect for the new uid
      ctx->KeySetEx(ImGuiKey_Escape, true, 0.0f);
      ctx->Yield();  // frame M: the blank is cancelled, the refill lands on the same rect, refocus arms
      ctx->KeySetEx(ImGuiKey_Escape, false, 0.0f);
      ctx->MouseDown(0);  // frame M+1: the crossing frame — the click lands after the submission
      ctx->MouseUp(0);    // frame M+2: release fires the button
      ctx->Yield(4);

      IM_CHECK(!ctx->ItemExists("**/##row_text_1"));  // the click was honored: the refilled row is gone
      IM_CHECK(ctx->ItemExists("**/##row_text_2"));   // the ≥1-row invariant refilled one blank
      IM_CHECK_EQ(ImGui::GetActiveID(), (ImGuiID)0);  // and nothing stole the keyboard

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // Negative control for event attribution (the shape a frame-level probe caught): tab away and
  // press Enter on the very next frame — the box reports its deactivation across that window, but
  // the Enter belongs to whatever navigation focused, not to the row just left. Nothing may be
  // inserted, and the row keeps its text.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "enter_right_after_tabbing_away_does_not_insert");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "1-2");
      ctx->ItemInput("**/##row_text_0");
      ctx->Yield(2);
      ctx->KeySetEx(ImGuiKey_Tab, true, 0.0f);
      ctx->Yield();  // the tabbing request goes out
      ctx->KeySetEx(ImGuiKey_Tab, false, 0.0f);
      ctx->KeySetEx(ImGuiKey_Enter, true, 0.0f);  // adjacent frame, as a fast typist lands them
      ctx->Yield();
      ctx->KeySetEx(ImGuiKey_Enter, false, 0.0f);
      ctx->Yield(4);

      auto rows = RowTexts();
      IM_CHECK_EQ(rows.size(), static_cast<size_t>(1));  // the left row spawned nothing
      IM_CHECK_STR_EQ(rows[0].c_str(), "1-2");
      IM_CHECK_NE(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_0").ID);  // no focus steal either

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // The same adjacency for Esc against an empty row, where row COUNT cannot see the difference:
  // tab away from the lone blank row and press Esc next frame — a misfire would delete that row
  // and refill a blank under a FRESH uid, leaving one row again. The row's identity is the
  // observable: uid 0 must survive untouched, nothing refilled, nothing refocused.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "filter_editor", "esc_right_after_tabbing_away_does_not_delete_the_blank_row");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);  // the lone blank row, uid 0
      ctx->ItemInput("**/##row_text_0");
      ctx->Yield(2);
      ctx->KeySetEx(ImGuiKey_Tab, true, 0.0f);
      ctx->Yield();  // the tabbing request goes out
      ctx->KeySetEx(ImGuiKey_Tab, false, 0.0f);
      ctx->KeySetEx(ImGuiKey_Escape, true, 0.0f);  // adjacent frame, as a fast hand lands them
      ctx->Yield();
      ctx->KeySetEx(ImGuiKey_Escape, false, 0.0f);
      ctx->Yield(4);

      IM_CHECK(ctx->ItemExists("**/##row_text_0"));   // uid 0: the SAME row, not a refilled one
      IM_CHECK(!ctx->ItemExists("**/##row_text_1"));  // no refill: nothing was deleted
      IM_CHECK_STR_EQ(RowTexts()[0].c_str(), "");
      IM_CHECK_NE(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_0").ID);  // no focus steal

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // The handoff itself must carry the cancel: press Enter, then Esc on the very next frame —
  // before the inserted row ever became an editor. Nothing consumes a key there unless the flow
  // routes it to the row it is undoing; a dropped cancel would leave the creation alive and hand
  // it the focus anyway.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "esc_during_the_enter_handoff_cancels_the_creation");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "1-2");
      ctx->ItemInput("**/##row_text_0");
      ctx->Yield(2);
      ctx->KeySetEx(ImGuiKey_Enter, true, 0.0f);
      ctx->Yield();  // the row is inserted, the focus request armed, the new box NOT yet active
      ctx->KeySetEx(ImGuiKey_Enter, false, 0.0f);
      ctx->KeySetEx(ImGuiKey_Escape, true, 0.0f);  // the handoff frame: cancel before any focus
      ctx->Yield();
      ctx->KeySetEx(ImGuiKey_Escape, false, 0.0f);
      ctx->KeyChars("7");  // the refocus handoff frame itself
      ctx->KeyChars("7");  // the refocus activation frame
      ctx->Yield(4);

      auto rows = RowTexts();
      IM_CHECK_EQ(rows.size(), static_cast<size_t>(1));  // the creation is gone
      IM_CHECK_STR_EQ(rows[0].c_str(), "1-277");         // both keystrokes landed in the refocused source
      IM_CHECK(!ctx->ItemExists("**/##row_text_1"));
      IM_CHECK_EQ(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_0").ID);  // back on the source

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // Keystrokes must read exactly the same whether they arrive during the refocus handoff or after
  // it settles. BOTH legs let the new row actually take the activation first — the state the
  // normal cancel path runs in, where the global InputTextState belongs to the new row when the
  // source is re-armed — then cancel, then differ ONLY in when the same keystrokes arrive. No
  // extra click or ItemInput anywhere: the settled leg is the one a select-all-on-fresh-activation
  // used to eat ("99" over "1-2" instead of an append).
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor",
                                    "parked_and_settled_keystrokes_read_identically_on_the_refocused_row");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      // Leg A: the keystrokes land inside the refocus handoff frames.
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "1-2");
      ctx->ItemInput("**/##row_text_0");
      ctx->Yield(2);
      ctx->KeySetEx(ImGuiKey_Enter, true, 0.0f);
      ctx->Yield();
      ctx->KeySetEx(ImGuiKey_Enter, false, 0.0f);
      ctx->Yield(2);
      IM_CHECK_EQ(ImGui::GetActiveID(),
                  ctx->ItemInfo("**/##row_text_1").ID);  // premise: the new row really took the editor
      ctx->KeySetEx(ImGuiKey_Escape, true, 0.0f);        // cancel the activated creation...
      ctx->Yield();                                      // ...the source row is the refocus target
      ctx->KeySetEx(ImGuiKey_Escape, false, 0.0f);
      ctx->KeyChars("99");  // land in the refocus handoff frames themselves
      ctx->Yield(4);
      auto rows = RowTexts();
      IM_CHECK_EQ(rows.size(), static_cast<size_t>(1));
      IM_CHECK_STR_EQ(rows[0].c_str(), "1-299");

      // Leg B: the same keystrokes arrive only after the refocus has fully settled.
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "1-2");
      ctx->ItemInput("**/##row_text_0");
      ctx->Yield(2);
      ctx->KeySetEx(ImGuiKey_Enter, true, 0.0f);
      ctx->Yield();
      ctx->KeySetEx(ImGuiKey_Enter, false, 0.0f);
      ctx->Yield(2);
      IM_CHECK_EQ(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_1").ID);  // premise: same activation
      ctx->KeySetEx(ImGuiKey_Escape, true, 0.0f);
      ctx->Yield();
      ctx->KeySetEx(ImGuiKey_Escape, false, 0.0f);
      ctx->Yield(4);
      IM_CHECK_EQ(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_0").ID);  // premise: refocus settled
      ctx->KeyChars("99");
      ctx->Yield(4);
      rows = RowTexts();
      IM_CHECK_EQ(rows.size(), static_cast<size_t>(1));
      IM_CHECK_STR_EQ(rows[0].c_str(), "1-299");  // identical to leg A

      // And both timings undo to the same pre-crossing baseline: one Esc restores "1-2".
      ctx->KeyPress(ImGuiKey_Escape);
      ctx->Yield(4);
      rows = RowTexts();
      IM_CHECK_EQ(rows.size(), static_cast<size_t>(1));
      IM_CHECK_STR_EQ(rows[0].c_str(), "1-2");

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // A parked focus request yields to a control the user chose meanwhile: clicking another real
  // input box while the handoff is in flight must not have the row grab the focus on a later
  // frame, must not delete the row, and must not drop the keystrokes the crossing had already
  // accepted — they land in the row exactly once, while anything typed after the click belongs to
  // the clicked control alone.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "a_focus_request_yields_to_a_control_the_user_chose");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "1-2");
      AuthorRow(ctx, 1, "3-4");
      ctx->ItemInput("**/##row_text_0");
      ctx->Yield(2);
      ctx->KeySetEx(ImGuiKey_Enter, true, 0.0f);
      ctx->Yield();
      ctx->KeySetEx(ImGuiKey_Enter, false, 0.0f);
      ctx->KeyChars("9");                 // accepted into the crossing before the click
      ctx->ItemClick("**/##row_text_0");  // the user chose the row above, mid-handoff
      ctx->Yield(4);
      ctx->KeyCharsReplace("7");  // typed after the click settled: the clicked box's edit
      ctx->Yield(3);

      auto rows = RowTexts();
      IM_CHECK_EQ(rows.size(), static_cast<size_t>(3));
      IM_CHECK_STR_EQ(rows[0].c_str(), "7");  // the clicked control got the late keystrokes
      IM_CHECK_STR_EQ(rows[1].c_str(), "9");  // the accepted keystroke landed in the crossing's row, exactly once
      IM_CHECK_STR_EQ(rows[2].c_str(), "3-4");
      IM_CHECK(ctx->ItemExists("**/##row_text_2"));                            // the creation itself survives
      IM_CHECK_NE(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_2").ID);  // ...without the focus

      // Second flow: the click lands on the row that renders AFTER the handoff target, on the
      // activation frame itself. The programmatic activation must not outrun it (an activation
      // blocks same-frame clicks on later widgets): the clicked row takes the editor, the
      // crossing's row keeps its accepted keystroke.
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "1-2");
      AuthorRow(ctx, 1, "3-4");
      ctx->ItemInput("**/##row_text_0");
      ctx->Yield(2);
      ctx->MouseMoveToPos(ctx->ItemInfo("**/##row_text_1").RectFull.GetCenter());  // hover the later row
      ctx->Yield(2);
      ctx->KeySetEx(ImGuiKey_Enter, true, 0.0f);
      ctx->Yield();  // frame N: the blank is inserted, the focus request armed
      ctx->KeySetEx(ImGuiKey_Enter, false, 0.0f);
      ctx->KeyChars("9");  // frame N+1: the crossing frame
      // Click the row below the insertion while the handoff is still in flight (or freshly
      // settled — ItemClick's own hover-then-click spans a few frames, and the outcome must be
      // the same either way): the crossing's row keeps its accepted keystroke exactly once and
      // never ends up with the user's post-click typing; the clicked row takes the editor.
      ctx->ItemClick("**/##row_text_1");
      ctx->Yield(4);
      ctx->KeyCharsReplace("7");
      ctx->Yield(3);

      rows = RowTexts();
      IM_CHECK_EQ(rows.size(), static_cast<size_t>(3));
      IM_CHECK_STR_EQ(rows[1].c_str(), "9");  // the accepted keystroke stays in the crossing's row
      IM_CHECK_STR_EQ(rows[2].c_str(), "7");  // the clicked later row got the typing
      IM_CHECK_EQ(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_1").ID);  // the click decided, not the request

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // The click half of the attribution boundary, in the adjacency the harness can drive (a click
  // and a keydown in the SAME frame has no deterministic primitive — MouseClick's internal yields
  // always land the key one frame apart): click away, then Esc next frame. The left row is not
  // the owner entering its call anymore, so the key belongs to nobody — the row's identity is the
  // observable, exactly as in the tab adjacency.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "esc_right_after_clicking_away_does_not_delete");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      ctx->ItemInput("**/##row_text_0");
      ctx->Yield(2);
      ctx->ItemClick("**/Filter In##filter_action");  // the click-away deactivation
      ctx->KeySetEx(ImGuiKey_Escape, true, 0.0f);     // adjacent frame
      ctx->Yield();
      ctx->KeySetEx(ImGuiKey_Escape, false, 0.0f);
      ctx->Yield(4);

      IM_CHECK(ctx->ItemExists("**/##row_text_0"));   // uid 0: the SAME row, kept
      IM_CHECK(!ctx->ItemExists("**/##row_text_1"));  // no refill
      IM_CHECK_STR_EQ(RowTexts()[0].c_str(), "");
      IM_CHECK_NE(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_0").ID);

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // An Enter arriving while the creation handoff is still crossing must not be dropped by the
  // activation frame's key-skip: it accepts the crossing's parked text into the target row and
  // creates the next blank below it — the same outcome as typing after the handoff had settled —
  // without the target ever taking the editor (that would swallow the next handoff whole).
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "filter_editor", "enter_during_the_handoff_accepts_the_parked_text_and_creates_below");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "1-2");
      AuthorRow(ctx, 1, "3-4");
      ctx->ItemInput("**/##row_text_0");
      ctx->Yield(2);
      ctx->KeySetEx(ImGuiKey_Enter, true, 0.0f);
      ctx->Yield();  // frame N: blank uid2 inserted, request armed
      ctx->KeySetEx(ImGuiKey_Enter, false, 0.0f);
      ctx->KeyChars("9");                         // frame N+1: the crossing frame
      ctx->KeySetEx(ImGuiKey_Enter, true, 0.0f);  // frame N+2: the ACTIVATION frame — the key is intercepted
      ctx->Yield();
      ctx->KeySetEx(ImGuiKey_Enter, false, 0.0f);
      ctx->Yield(4);

      auto rows = RowTexts();
      IM_CHECK_EQ(rows.size(), static_cast<size_t>(4));
      IM_CHECK_STR_EQ(rows[0].c_str(), "1-2");
      IM_CHECK_STR_EQ(rows[1].c_str(), "9");  // the parked text was accepted into the crossing's row
      IM_CHECK_STR_EQ(rows[2].c_str(), "");   // the new blank sits right below it
      IM_CHECK_STR_EQ(rows[3].c_str(), "3-4");
      IM_CHECK_EQ(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_3").ID);  // the NEW handoff, not the old target

      ctx->KeyChars("5");  // and the new blank is the editor: typing lands there
      ctx->Yield(3);
      IM_CHECK_STR_EQ(RowTexts()[2].c_str(), "5");

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // Navigation during the handoff must behave like navigation after it: the same keystrokes
  // accepted during the crossing then a Tab, versus typed after settling then a Tab, leave the
  // row with the same text, move focus on without letting the handoff steal it back, and nothing
  // re-flows into the row afterwards. Two legs, one expected outcome.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "navigation_during_the_handoff_keeps_the_accepted_chars");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      // Leg A: "9" is accepted into the crossing, then the user Tabs mid-handoff.
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "1-2");
      ctx->ItemInput("**/##row_text_0");
      ctx->Yield(2);
      ctx->KeySetEx(ImGuiKey_Enter, true, 0.0f);
      ctx->Yield();  // frame N: blank uid1 inserted, request armed
      ctx->KeySetEx(ImGuiKey_Enter, false, 0.0f);
      ctx->KeySetEx(ImGuiKey_Tab, true, 0.0f);  // queued with the keystroke below
      ctx->KeyChars("9");  // frame N+1: the crossing frame — "9" accepted, Tab lands the same frame
      ctx->KeySetEx(ImGuiKey_Tab, false, 0.0f);
      ctx->Yield(4);
      auto rows = RowTexts();
      IM_CHECK_EQ(rows.size(), static_cast<size_t>(2));
      IM_CHECK_STR_EQ(rows[1].c_str(), "9");  // the accepted keystroke stayed in the row
      IM_CHECK_NE(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_1").ID);  // no late steal: the Tab decided
      ctx->KeyChars("5");  // typing after the navigation must not re-flow into the row
      ctx->Yield(3);
      IM_CHECK_STR_EQ(RowTexts()[1].c_str(), "9");

      // The navigation DESTINATION is part of the proposition: the Tab must continue from the
      // blank row (the transaction's target), not from wherever NavId happened to be on that
      // frame — on the first crossing frame that is still the source. Enter on the focused
      // delete button makes the destination observable: the BLANK is deleted, the source survives.
      ctx->KeyPress(ImGuiKey_Enter);
      ctx->Yield(4);
      IM_CHECK(!ctx->ItemExists("**/##row_text_1"));  // the blank took the Tab and the Enter
      IM_CHECK_STR_EQ(RowTexts()[0].c_str(), "1-2");  // the source row was NOT the one removed

      // Leg B: the same keystrokes typed after the handoff settled, then the same Tab.
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "1-2");
      ctx->ItemInput("**/##row_text_0");
      ctx->Yield(2);
      ctx->KeySetEx(ImGuiKey_Enter, true, 0.0f);
      ctx->Yield();
      ctx->KeySetEx(ImGuiKey_Enter, false, 0.0f);
      ctx->Yield(4);
      IM_CHECK_EQ(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_1").ID);  // premise: settled editor
      ctx->KeyChars("9");
      ctx->Yield(3);
      ctx->KeySetEx(ImGuiKey_Tab, true, 0.0f);
      ctx->Yield();
      ctx->KeySetEx(ImGuiKey_Tab, false, 0.0f);
      ctx->Yield(4);
      rows = RowTexts();
      IM_CHECK_EQ(rows.size(), static_cast<size_t>(2));
      IM_CHECK_STR_EQ(rows[1].c_str(), "9");  // identical to leg A
      IM_CHECK_NE(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_1").ID);
      ctx->KeyChars("5");
      ctx->Yield(3);
      IM_CHECK_STR_EQ(RowTexts()[1].c_str(), "9");

      // Same destination probe as leg A: settled Tab continues from the blank row.
      ctx->KeyPress(ImGuiKey_Enter);
      ctx->Yield(4);
      IM_CHECK(!ctx->ItemExists("**/##row_text_1"));
      IM_CHECK_STR_EQ(RowTexts()[0].c_str(), "1-2");

      // Leg C: the Tab lands on the ACTIVATION frame itself — the crossing has just submitted its
      // request and the navigation state points at the target. The user's Tab must not turn into
      // a re-activation of that target: no focus steal, the accepted text stays, later typing
      // does not reflow into the row.
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "1-2");
      ctx->ItemInput("**/##row_text_0");
      ctx->Yield(2);
      ctx->KeySetEx(ImGuiKey_Enter, true, 0.0f);
      ctx->Yield();  // frame N: blank uid1 inserted, request armed
      ctx->KeySetEx(ImGuiKey_Enter, false, 0.0f);
      ctx->KeyChars("9");                       // frame N+1: the crossing frame
      ctx->KeySetEx(ImGuiKey_Tab, true, 0.0f);  // frame N+2: the activation frame
      ctx->Yield();
      ctx->KeySetEx(ImGuiKey_Tab, false, 0.0f);
      ctx->Yield(4);
      rows = RowTexts();
      IM_CHECK_EQ(rows.size(), static_cast<size_t>(2));
      IM_CHECK_STR_EQ(rows[1].c_str(), "9");  // the accepted keystroke stayed in the row
      IM_CHECK_NE(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_1").ID);  // not re-activated by the old request
      ctx->KeyChars("5");
      ctx->Yield(3);
      IM_CHECK_STR_EQ(RowTexts()[1].c_str(), "9");  // nothing reflows into the row

      // Same destination probe: the activation-frame Tab continues from the blank row too.
      ctx->KeyPress(ImGuiKey_Enter);
      ctx->Yield(4);
      IM_CHECK(!ctx->ItemExists("**/##row_text_1"));
      IM_CHECK_STR_EQ(RowTexts()[0].c_str(), "1-2");

      // Leg D: the source was focused by the KEYBOARD (a Tab there and back), which leaves the
      // nav cursor visible — nav's own tabbing request on the crossing frame is then directional
      // (+1), not the init form, while its anchor is still the source row. The re-issue must
      // fire on the anchor relationship alone: the destination is the blank's button, not the
      // source's, and the Enter below deletes the BLANK.
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "1-2");
      ctx->ItemInput("**/##row_text_0");
      ctx->Yield(2);
      ctx->KeyPress(ImGuiKey_Tab);
      ctx->Yield(2);
      ctx->KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
      ctx->Yield(2);
      IM_CHECK_EQ(ImGui::GetActiveID(),
                  ctx->ItemInfo("**/##row_text_0").ID);  // premise: keyboard focus back on the source
      ctx->KeyPress(ImGuiKey_Enter);
      ctx->Yield();
      ctx->KeyChars("9");                       // the crossing frame
      ctx->KeySetEx(ImGuiKey_Tab, true, 0.0f);  // the Tab, same stage of the handoff
      ctx->Yield();
      ctx->KeySetEx(ImGuiKey_Tab, false, 0.0f);
      ctx->Yield(4);
      IM_CHECK_STR_EQ(RowTexts()[1].c_str(), "9");  // the accepted keystroke stayed in the blank
      ctx->KeyPress(ImGuiKey_Enter);
      ctx->Yield(4);
      IM_CHECK(!ctx->ItemExists("**/##row_text_1"));  // the blank took the Tab and the Enter
      IM_CHECK_STR_EQ(RowTexts()[0].c_str(), "1-2");  // the source survived

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // The same crossing-tab contract in the EXPANDED layout. The Tab guard's only call used to live
  // in the Compact pane child, so under Expanded a Tab during the handoff was overridden by the
  // row's own auto-focus: the row stayed the editor and typing after the navigation flowed back
  // into it. Two legs — the Tab on the first crossing frame and on the activation frame — assert
  // what the Compact witness asserts: the accepted keystroke stays in the row, the navigation
  // continues from the blank (the destination probe: a later Enter deletes the BLANK, the source
  // survives), and typing afterwards does not reflow into the row.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "the_crossing_tab_holds_in_the_expanded_layout");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      // Leg A: Tab on the first crossing frame.
      ResetTestState();
      gui::g_state.modal_layout_compact = false;  // Expanded: all sections at once, no tab bar
      ctx->Yield(2);
      OpenCardEditor(ctx, 0, kCrystalTabRef);  // opens the modal; the Filter rows are live in both layouts
      ctx->Yield(4);
      IM_CHECK_EQ(gui::g_state.modal_layout_compact, false);  // premise: the Expanded layout is up
      IM_CHECK(ctx->ItemExists("**/##row_text_0"));           // premise: the row list renders in it
      AuthorRow(ctx, 0, "1-2");
      ctx->ItemInput("**/##row_text_0");
      ctx->Yield(2);
      ctx->KeySetEx(ImGuiKey_Enter, true, 0.0f);
      ctx->Yield();  // frame N: blank uid1 inserted, request armed
      ctx->KeySetEx(ImGuiKey_Enter, false, 0.0f);
      ctx->KeySetEx(ImGuiKey_Tab, true, 0.0f);  // queued with the keystroke below
      ctx->KeyChars("9");  // frame N+1: the crossing frame — "9" accepted, the Tab must end the crossing
      ctx->KeySetEx(ImGuiKey_Tab, false, 0.0f);
      ctx->Yield(4);
      auto rows = RowTexts();
      IM_CHECK_EQ(rows.size(), static_cast<size_t>(2));
      IM_CHECK_STR_EQ(rows[1].c_str(), "9");  // the accepted keystroke stayed in the row
      IM_CHECK_NE(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_1").ID);  // no late steal: the Tab decided
      ctx->KeyChars("5");  // typing after the navigation must not re-flow into the row
      ctx->Yield(3);
      IM_CHECK_STR_EQ(RowTexts()[1].c_str(), "9");

      // The destination probe: Enter acts on the row the Tab actually landed on — the blank's own
      // delete button — so the BLANK is deleted and the source survives.
      ctx->KeyPress(ImGuiKey_Enter);
      ctx->Yield(4);
      IM_CHECK(!ctx->ItemExists("**/##row_text_1"));  // the blank took the Tab and the Enter
      IM_CHECK_STR_EQ(RowTexts()[0].c_str(), "1-2");  // the source row was NOT the one removed

      // Leg B: the Tab lands on the ACTIVATION frame — the crossing has just submitted its request
      // and the navigation state points at the target. The user's Tab must not turn into a
      // re-activation of that target: no focus steal, the accepted text stays, later typing does
      // not reflow.
      ResetTestState();
      gui::g_state.modal_layout_compact = false;
      ctx->Yield(2);
      OpenCardEditor(ctx, 0, kCrystalTabRef);
      ctx->Yield(4);
      IM_CHECK_EQ(gui::g_state.modal_layout_compact, false);
      IM_CHECK(ctx->ItemExists("**/##row_text_0"));
      AuthorRow(ctx, 0, "1-2");
      ctx->ItemInput("**/##row_text_0");
      ctx->Yield(2);
      ctx->KeySetEx(ImGuiKey_Enter, true, 0.0f);
      ctx->Yield();  // frame N: blank uid1 inserted, request armed
      ctx->KeySetEx(ImGuiKey_Enter, false, 0.0f);
      ctx->KeyChars("9");                       // frame N+1: the crossing frame
      ctx->KeySetEx(ImGuiKey_Tab, true, 0.0f);  // frame N+2: the activation frame
      ctx->Yield();
      ctx->KeySetEx(ImGuiKey_Tab, false, 0.0f);
      ctx->Yield(4);
      rows = RowTexts();
      IM_CHECK_EQ(rows.size(), static_cast<size_t>(2));
      IM_CHECK_STR_EQ(rows[1].c_str(), "9");  // the accepted keystroke stayed in the row
      IM_CHECK_NE(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_1").ID);  // not re-activated by the old request
      ctx->KeyChars("5");
      ctx->Yield(3);
      IM_CHECK_STR_EQ(RowTexts()[1].c_str(), "9");  // nothing reflows into the row

      // Same destination probe: the activation-frame Tab continues from the blank row too.
      ctx->KeyPress(ImGuiKey_Enter);
      ctx->Yield(4);
      IM_CHECK(!ctx->ItemExists("**/##row_text_1"));
      IM_CHECK_STR_EQ(RowTexts()[0].c_str(), "1-2");

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // Shift+Tab on the refocus handoff's activation frame must reach BACKWARD through the full
  // candidate set — the same destination the stable state's Shift+Tab reaches. The request
  // adjustments a crossing makes may not discard the candidates already scored ahead of the list,
  // or the backward navigation wraps around to a control AFTER the rows instead. Two legs, one
  // expected destination: the activation-frame leg and the settled leg must agree on it (the
  // nearest focusable ahead of the row list), and neither may leave an editor active.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor",
                                    "shift_tab_on_the_refocus_activation_reaches_backward_to_the_tab_header");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      // Leg A: Shift+Tab lands on the refocus handoff's activation frame.
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "1-2");
      ctx->ItemInput("**/##row_text_0");
      ctx->Yield(2);
      ctx->KeySetEx(ImGuiKey_Enter, true, 0.0f);
      ctx->Yield();  // frame N: the blank is inserted
      ctx->KeySetEx(ImGuiKey_Enter, false, 0.0f);
      ctx->Yield(2);
      IM_CHECK_EQ(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_1").ID);  // premise: the blank took the editor
      ctx->KeySetEx(ImGuiKey_Escape, true, 0.0f);  // frame M: cancel the blank — the refocus arms
      ctx->Yield();
      ctx->KeySetEx(ImGuiKey_Escape, false, 0.0f);
      ctx->Yield();                                              // frame M+1: the refocus crossing
      ctx->KeySetEx(ImGuiKey_Tab | ImGuiMod_Shift, true, 0.0f);  // frame M+2: the activation frame
      ctx->Yield();
      ctx->KeySetEx(ImGuiKey_Tab | ImGuiMod_Shift, false, 0.0f);
      ctx->Yield(4);
      const ImGuiID nav_after_activation = ImGui::GetCurrentContext()->NavId;
      IM_CHECK_NE(nav_after_activation, (ImGuiID)0);  // backward navigation happened
      IM_CHECK_EQ(ImGui::GetActiveID(), (ImGuiID)0);  // no editor grabbed anywhere
      IM_CHECK_STR_EQ(RowTexts()[0].c_str(), "1-2");  // the modal is untouched by the detour

      // Leg B: the same Shift+Tab from the settled editor.
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "1-2");
      ctx->ItemInput("**/##row_text_0");
      ctx->Yield(2);
      ctx->KeySetEx(ImGuiKey_Enter, true, 0.0f);
      ctx->Yield();
      ctx->KeySetEx(ImGuiKey_Enter, false, 0.0f);
      ctx->Yield(2);
      IM_CHECK_EQ(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_1").ID);
      ctx->KeySetEx(ImGuiKey_Escape, true, 0.0f);
      ctx->Yield();
      ctx->KeySetEx(ImGuiKey_Escape, false, 0.0f);
      ctx->Yield(4);                                                           // the refocus has fully settled
      IM_CHECK_EQ(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_0").ID);  // premise: settled editor
      ctx->KeySetEx(ImGuiKey_Tab | ImGuiMod_Shift, true, 0.0f);
      ctx->Yield();
      ctx->KeySetEx(ImGuiKey_Tab | ImGuiMod_Shift, false, 0.0f);
      ctx->Yield(4);
      const ImGuiID nav_after_settled = ImGui::GetCurrentContext()->NavId;

      // The activation-frame backward navigation landed exactly where the settled one does.
      IM_CHECK_EQ(nav_after_activation, nav_after_settled);
      IM_CHECK_NE(nav_after_settled, ctx->ItemInfo("**/##row_text_0").ID);  // it left the row list
      IM_CHECK_EQ(ImGui::GetActiveID(), (ImGuiID)0);

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // The row-end policy must never reach past its own transaction: clicking the source row right
  // after the handoff's settle puts the cursor where the user clicked (the row's start), and the
  // deferred caret fixup the settle armed for the blank must die with the blank's ownership —
  // otherwise it fires inside the source's next call and shoves the caret to the end. Plain
  // KeyChars afterwards (no refocus, no replace helper) makes the insertion point the observable.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "filter_editor", "a_click_on_the_source_row_keeps_the_caret_where_it_was_put");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "1-2");
      ctx->ItemInput("**/##row_text_0");
      ctx->Yield(2);
      ctx->MouseMoveToPos(ImVec2(ctx->ItemInfo("**/##row_text_0").RectFull.Min.x + 6.0f,
                                 ctx->ItemInfo("**/##row_text_0").RectFull.GetCenter().y));
      ctx->Yield(2);  // hover the row's start: the click will put the caret at position 0
      ctx->KeySetEx(ImGuiKey_Enter, true, 0.0f);
      ctx->Yield();  // frame N: the blank is inserted, the focus request armed
      ctx->KeySetEx(ImGuiKey_Enter, false, 0.0f);
      ctx->KeyChars("9");  // frame N+1: the crossing frame
      ctx->Yield(1);       // frame N+2: the activation frame (settle: the caret fixup is armed)
      ctx->MouseDown(0);   // frame N+3: the click on the source row — the user re-picks the editor
      ctx->MouseUp(0);     // frame N+4: release
      ctx->Yield(2);
      ctx->KeyChars("7");  // plain typing: lands at the clicked caret, not at a policy-forced end
      ctx->Yield(3);

      auto rows = RowTexts();
      IM_CHECK_EQ(rows.size(), static_cast<size_t>(2));
      IM_CHECK_STR_EQ(rows[0].c_str(), "71-2");  // inserted at the clicked caret (position 0)
      IM_CHECK_STR_EQ(rows[1].c_str(), "9");     // the blank kept its accepted keystroke

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // Shift+Tab on the FIRST refocus frame — while NavId still points at the row that was just
  // deleted — must anchor on the transaction's target, not scan from a dangling id (which finds
  // nothing and wraps past the list, landing on controls behind it where a later Enter eats the
  // expression). Backward from the target reaches the Filter tab header; Enter there changes
  // nothing. Same observable shape as the two-leg case, one frame earlier.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "filter_editor", "shift_tab_on_the_first_refocus_frame_wraps_neither_nor_wipes");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "1-2");
      ctx->ItemInput("**/##row_text_0");
      ctx->Yield(2);
      ctx->KeySetEx(ImGuiKey_Enter, true, 0.0f);
      ctx->Yield();  // frame N: the blank is inserted
      ctx->KeySetEx(ImGuiKey_Enter, false, 0.0f);
      ctx->Yield(2);
      IM_CHECK_EQ(ImGui::GetActiveID(), ctx->ItemInfo("**/##row_text_1").ID);  // premise: the blank took the editor
      ctx->KeySetEx(ImGuiKey_Escape, true, 0.0f);  // frame M: the blank is cancelled, the refocus arms
      ctx->Yield();
      ctx->KeySetEx(ImGuiKey_Escape, false, 0.0f);
      ctx->KeySetEx(ImGuiKey_Tab | ImGuiMod_Shift, true, 0.0f);  // frame M+1: the FIRST refocus frame
      ctx->Yield();
      ctx->KeySetEx(ImGuiKey_Tab | ImGuiMod_Shift, false, 0.0f);
      ctx->Yield(4);

      IM_CHECK_EQ(ImGui::GetCurrentContext()->NavId, ctx->ItemInfo("**/###filter_tab").ID);  // backward from the target
      IM_CHECK_EQ(ImGui::GetActiveID(), (ImGuiID)0);  // no editor grabbed anywhere

      ctx->KeyPress(ImGuiKey_Enter);  // whatever holds nav focus takes the Enter, not a row
      ctx->Yield(4);
      IM_CHECK_STR_EQ(RowTexts()[0].c_str(), "1-2");  // the expression survived the detour

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // Clicking the crossing's own row right after the settle must neither drop the keystrokes the
  // transaction already accepted nor force the caret anywhere: the deferred text sync completes
  // (the row keeps "9"), the click decides the caret (the row's start), and plain typing inserts
  // there — "79", not "7" (the sync lost) and not "1-29"-style policy append (the policy gone).
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor",
                                    "a_click_on_the_crossing_row_itself_keeps_the_accepted_text_and_the_caret");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "1-2");
      ctx->ItemInput("**/##row_text_0");
      ctx->Yield(2);
      ctx->KeySetEx(ImGuiKey_Enter, true, 0.0f);
      ctx->Yield();  // frame N: the blank is inserted, the focus request armed
      ctx->KeySetEx(ImGuiKey_Enter, false, 0.0f);
      ctx->KeyChars("9");  // frame N+1: the crossing frame
      // The blank's position is only known post-insert; teleport instantly and let the hover
      // register on the activation frame, so the click below is a real preceded click.
      ctx->MouseTeleportToPos(ImVec2(ctx->ItemInfo("**/##row_text_1").RectFull.Min.x + 6.0f,
                                     ctx->ItemInfo("**/##row_text_1").RectFull.GetCenter().y),
                              ImGuiTestOpFlags_NoYield);
      ctx->Yield(1);      // frame N+2: the activation frame (settle: "9" delivered, sync deferred)
      ctx->MouseDown(0);  // frame N+3: the click on the crossing's own row start
      ctx->MouseUp(0);    // frame N+4: release
      ctx->Yield(2);
      ctx->KeyChars("7");  // plain typing at the clicked caret
      ctx->Yield(3);

      auto rows = RowTexts();
      IM_CHECK_EQ(rows.size(), static_cast<size_t>(2));
      IM_CHECK_STR_EQ(rows[1].c_str(), "79");  // the accepted text kept, the typing at the clicked caret
      IM_CHECK_STR_EQ(rows[0].c_str(), "1-2");

      // And the undo baseline of the clicked row is still its pre-handoff emptiness: one Esc
      // reverts everything typed since, the cancel leaves it empty, and the row is deleted.
      ctx->KeyPress(ImGuiKey_Escape);
      ctx->Yield(4);
      rows = RowTexts();
      IM_CHECK_EQ(rows.size(), static_cast<size_t>(1));
      IM_CHECK_STR_EQ(rows[0].c_str(), "1-2");

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }

  // ---------------------------------------------------------------------------------------------
  // Committing what was authored.
  // ---------------------------------------------------------------------------------------------

  // Each row is one OR alternative and its text is stored verbatim — the editor does not
  // normalise, reorder or merge. Three rows of deliberately different grammar shapes, so a
  // regression that only survives the simplest one shows up.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "rows_commit_verbatim_one_summand_each");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "3-5");
      AuthorRow(ctx, 1, "entry:2 & exit:4");
      AuthorRow(ctx, 2, "3-5 & entry:2");
      IM_CHECK(!OkIsDisabled(ctx));
      ctx->ItemClick(kOk);
      ctx->Yield(2);

      const auto* f = CommittedFilter();
      IM_CHECK(f != nullptr);
      IM_CHECK_EQ(f->param.size(), static_cast<size_t>(3));
      IM_CHECK_STR_EQ(f->param[0].text.c_str(), "3-5");
      IM_CHECK_STR_EQ(f->param[1].text.c_str(), "entry:2 & exit:4");
      IM_CHECK_STR_EQ(f->param[2].text.c_str(), "3-5 & entry:2");
    };
  }

  // A ';' inside one row is an alternative within that row's token, not a row separator. The
  // distinction is invisible until commit: fanning it out here would change the on-disk row count
  // and, on the next open, hand the user back a different editor state than they typed.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "a_semicolon_row_commits_as_a_single_row");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      EditRowText(ctx, "**/##row_text_0", "1-3;3-5");
      ctx->Yield(2);
      IM_CHECK(!OkIsDisabled(ctx));
      ctx->ItemClick(kOk);
      ctx->Yield(2);

      const auto* f = CommittedFilter();
      IM_CHECK(f != nullptr);
      IM_CHECK_EQ(f->param.size(), static_cast<size_t>(1));
      IM_CHECK_STR_EQ(f->param[0].text.c_str(), "1-3;3-5");
      IM_CHECK_EQ(f->param[0].factors.size(), static_cast<size_t>(1));
    };
  }

  // Regression sentinel. A forgotten blank row used to materialise as a factor-less clause, which
  // the expansion turns into match-all: OR(3-5, everything). Under filter_in that is a filter that
  // silently does nothing; under filter_out it is an all-black render. Blank rows are stripped
  // before materialising, and if none survive there is no filter at all rather than a match-all one.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "a_blank_row_is_dropped_not_lowered_to_match_all");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "3-5");
      ctx->ItemClick(kAddRow);
      ctx->Yield(2);
      ctx->ItemClick(kOk);  // row 1 left blank on purpose
      ctx->Yield(2);

      const auto* f = CommittedFilter();
      IM_CHECK(f != nullptr);
      IM_CHECK_EQ(f->param.size(), static_cast<size_t>(1));
      IM_CHECK_STR_EQ(f->param[0].text.c_str(), "3-5");

      // And the degenerate case: blank out the only real row too. All rows blank means no filter,
      // not a filter that matches everything.
      OpenFilterModal(ctx);
      EditRowText(ctx, "**/##row_text_0", "");
      ctx->Yield(2);
      ctx->ItemClick(kOk);
      ctx->Yield(2);
      IM_CHECK(CommittedFilter() == nullptr);
    };
  }

  // Reopening has to rebuild the row list from what was committed, or the second edit starts from
  // a stale buffer and silently discards whatever the first one wrote.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "reopening_rebuilds_the_rows_from_the_committed_filter");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "3-5");
      AuthorRow(ctx, 1, "1-2");
      ctx->ItemClick(kOk);
      ctx->Yield(2);
      IM_CHECK(CommittedFilter() != nullptr);

      OpenFilterModal(ctx);
      IM_CHECK(ctx->ItemExists("**/##row_text_0"));
      IM_CHECK(ctx->ItemExists("**/##row_text_1"));
      IM_CHECK(!ctx->ItemExists("**/##row_text_2"));  // two rows back, not three or one

      // Edit the second row and commit again: the rebuilt buffer is the one being edited.
      EditRowText(ctx, "**/##row_text_1", "4-6");
      ctx->Yield(2);
      ctx->ItemClick(kOk);
      ctx->Yield(2);
      const auto* f = CommittedFilter();
      IM_CHECK(f != nullptr);
      IM_CHECK_EQ(f->param.size(), static_cast<size_t>(2));
      IM_CHECK_STR_EQ(f->param[0].text.c_str(), "3-5");
      IM_CHECK_STR_EQ(f->param[1].text.c_str(), "4-6");
    };
  }

  // What the editor produced has to survive the document it goes into. The authoring half is why
  // this lives here rather than beside the other round-trip chains: only a real modal and real
  // typing produce the SoP being saved, and a serialisation that silently normalised the rows
  // would hand the user back a different editor on the next open.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "authored_rows_survive_a_save_and_reload");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "3-5");
      AuthorRow(ctx, 1, "entry:2 & exit:4");
      AuthorRow(ctx, 2, "3-5 & entry:2");
      ctx->ItemClick(kOk);
      ctx->Yield(2);
      const auto* committed = CommittedFilter();
      IM_CHECK(committed != nullptr);
      const gui::FilterConfig original = *committed;

      const std::string tmp_path = GuiTestTempPath("lumice_filter_editor_roundtrip.lmc").string();
      IM_CHECK(gui::SaveLmcFile(tmp_path, gui::g_state, gui::g_preview, /*save_texture=*/false));

      gui::DoNew();
      gui::LmcTexture tex;
      IM_CHECK(gui::LoadLmcFile(tmp_path, gui::g_state, tex));

      const auto* reloaded = CommittedFilter();
      IM_CHECK(reloaded != nullptr);
      IM_CHECK(*reloaded == original);
      IM_CHECK_EQ(reloaded->param.size(), static_cast<size_t>(3));
      IM_CHECK_STR_EQ(reloaded->param[2].text.c_str(), "3-5 & entry:2");
      std::remove(tmp_path.c_str());
    };
  }

  // ---------------------------------------------------------------------------------------------
  // The controls beside the row list.
  // ---------------------------------------------------------------------------------------------

  // Two radios over one integer, so exclusivity is structural and not worth asserting — what is
  // worth asserting is that each radio writes ITS OWN value. Two call sites that both wrote 0
  // would look correct on the default and leave Filter Out permanently inert, which is a real
  // shape of copy-paste bug and the only one a frame can catch here.
  //
  // Asserted through the committed action rather than the widget's checked state: ImGui's
  // RadioButton never reports ImGuiItemStatusFlags_Checked to the test engine (unlike Checkbox),
  // so ctx->ItemIsChecked() cannot read one — the same registration gap that hides this modal's
  // hint text.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "each_action_radio_commits_its_own_value");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      OpenFilterModal(ctx);
      IM_CHECK(ctx->ItemExists("**/Filter In##filter_action"));
      IM_CHECK(ctx->ItemExists("**/Filter Out##filter_action"));

      ctx->ItemClick("**/Filter Out##filter_action");
      ctx->Yield(2);
      EditRowText(ctx, "**/##row_text_0", "3-1-5");
      ctx->Yield(2);
      ctx->ItemClick(kOk);
      ctx->Yield(2);
      const auto* out = CommittedFilter();
      IM_CHECK(out != nullptr);
      IM_CHECK_EQ(out->action, 1);

      // And back: the In radio is not a no-op that merely leaves the default in place.
      OpenFilterModal(ctx);
      ctx->ItemClick("**/Filter In##filter_action");
      ctx->Yield(2);
      ctx->ItemClick(kOk);
      ctx->Yield(2);
      const auto* in = CommittedFilter();
      IM_CHECK(in != nullptr);
      IM_CHECK_EQ(in->action, 0);
    };
  }

  // D only means something for axis configurations that have the symmetry it names. When they do
  // not, the checkbox stays live but grows an explanation beside it — a hover target that exists
  // only in the inapplicable case, which is what makes the rule observable at all (the text it
  // carries is a tooltip, and tooltips are not addressable).
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "d_is_explained_when_the_axis_does_not_support_it");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      struct AxisCase {
        const char* label;
        gui::AxisDistType az_type;
        float az_std;
        float roll_mean;
        bool expect_applicable;
      };
      // Uniform-360 azimuth with a roll on the 30-degree lattice is the applicable case; break
      // either half and it stops being applicable. Both breakages are asserted because they are
      // two independent conditions in one predicate.
      const AxisCase kCases[] = {
        { "uniform_az_roll_0", gui::AxisDistType::kUniform, 360.0f, 0.0f, true },
        { "roll_off_lattice", gui::AxisDistType::kUniform, 360.0f, 15.0f, false },
        { "az_not_uniform", gui::AxisDistType::kGauss, 30.0f, 0.0f, false },
      };

      // Collected, then asserted once: a fatal assert in the loop would leave the later axis
      // shapes unevaluated, and each row here is an independent condition.
      std::string wrong_predicate;
      std::string wrong_icon;
      for (const auto& c : kCases) {
        ResetTestState();
        ctx->Yield(2);
        auto& crystal = gui::g_state.crystals[gui::g_state.layers[0].entries[0].crystal_id];
        crystal.azimuth = { c.az_type, 0.0f, c.az_std };
        crystal.roll = { gui::AxisDistType::kUniform, c.roll_mean, 360.0f };
        ctx->Yield();
        OpenFilterModal(ctx);

        if (gui::IsCurrentModalDApplicable() != c.expect_applicable) {
          wrong_predicate += std::string(" ") + c.label;
        }
        // The user-visible half: the info icon appears exactly when D does not apply.
        const bool icon = ctx->ItemExists("**/" ICON_FA_CIRCLE_INFO "##d_tooltip_icon_filter_modal");
        if (icon == c.expect_applicable) {
          wrong_icon += std::string(" ") + c.label;
        }
        // D itself stays editable either way — it is explained, not taken away.
        if (IsDisabled(ctx->ItemInfo("**/D##filter_modal"))) {
          wrong_icon += std::string(" ") + c.label + ":disabled";
        }
        ctx->ItemClick(kCancel);
        ctx->Yield(2);
      }
      IM_CHECK_STR_EQ(wrong_predicate.c_str(), "");
      IM_CHECK_STR_EQ(wrong_icon.c_str(), "");
    };
  }

  // The shape half of the same rule: P, B and D act only as far as the crystal's own shape allows,
  // and a checkbox whose element this crystal lacks is explained the same way. A three-fold prism
  // (face distances alternating 1 / 1.2) keeps B, keeps P only for 120-degree turns, and loses D
  // when its roll selects an odd mirror; a regular prism shows none of the three hints.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "symmetry_hints_follow_the_crystal_shape");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      struct ShapeCase {
        const char* label;
        float alt_distance;  // face_distance of faces 1, 3, 5 (faces 0, 2, 4 stay 1)
        float roll_mean;
        bool expect_p_icon;
        bool expect_d_icon;
      };
      const ShapeCase kCases[] = {
        { "regular", 1.0f, 0.0f, false, false },
        { "three_fold_even_mirror", 1.2f, 0.0f, true, false },
        { "three_fold_odd_mirror", 1.2f, 30.0f, true, true },
      };
      std::string wrong;
      for (const auto& c : kCases) {
        ResetTestState();
        ctx->Yield(2);
        auto& crystal = gui::g_state.crystals[gui::g_state.layers[0].entries[0].crystal_id];
        crystal.type = gui::CrystalType::kPrism;
        for (int i = 1; i < 6; i += 2) {
          crystal.face_distance[i] = c.alt_distance;
        }
        crystal.azimuth = { gui::AxisDistType::kUniform, 0.0f, 360.0f };
        crystal.roll = { gui::AxisDistType::kUniform, c.roll_mean, 360.0f };
        ctx->Yield();
        OpenFilterModal(ctx);
        const bool p_icon = ctx->ItemExists("**/" ICON_FA_CIRCLE_INFO "##p_tooltip_icon_filter_modal");
        const bool b_icon = ctx->ItemExists("**/" ICON_FA_CIRCLE_INFO "##b_tooltip_icon_filter_modal");
        const bool d_icon = ctx->ItemExists("**/" ICON_FA_CIRCLE_INFO "##d_tooltip_icon_filter_modal");
        if (p_icon != c.expect_p_icon) {
          wrong += std::string(" ") + c.label + ":p";
        }
        if (b_icon) {
          wrong += std::string(" ") + c.label + ":b";
        }
        if (d_icon != c.expect_d_icon) {
          wrong += std::string(" ") + c.label + ":d";
        }
        if (gui::IsCurrentModalDApplicable() == c.expect_d_icon) {
          wrong += std::string(" ") + c.label + ":predicate";
        }
        ctx->ItemClick(kCancel);
        ctx->Yield(2);
      }
      IM_CHECK_STR_EQ(wrong.c_str(), "");
    };
  }

  // The orientation half for P and B: P needs a free roll, B a zenith symmetric about 90 degrees
  // (with a uniform azimuth). A Parry axis (roll locked, zenith 90) loses P and keeps B; a plate
  // axis (zenith 0, roll free) keeps P and loses B; a random axis keeps both. The crystal is a
  // regular prism, so every icon here is the axis's doing.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "filter_editor", "p_and_b_are_explained_when_the_axis_does_not_support_them");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      struct AxisCase {
        const char* label;
        gui::AxisDist zenith;
        gui::AxisDist roll;
        bool expect_p_icon;
        bool expect_b_icon;
      };
      const AxisCase kCases[] = {
        { "random",
          { gui::AxisDistType::kUniform, 0.0f, 360.0f },
          { gui::AxisDistType::kUniform, 0.0f, 360.0f },
          false,
          false },
        { "parry", { gui::AxisDistType::kGauss, 90.0f, 1.0f }, { gui::AxisDistType::kGauss, 0.0f, 1.0f }, true, false },
        { "plate",
          { gui::AxisDistType::kGauss, 0.0f, 1.0f },
          { gui::AxisDistType::kUniform, 0.0f, 360.0f },
          false,
          true },
      };
      std::string wrong;
      for (const auto& c : kCases) {
        ResetTestState();
        ctx->Yield(2);
        auto& crystal = gui::g_state.crystals[gui::g_state.layers[0].entries[0].crystal_id];
        crystal.type = gui::CrystalType::kPrism;
        for (auto& d : crystal.face_distance) {
          d = 1.0f;
        }
        crystal.zenith = c.zenith;
        crystal.azimuth = { gui::AxisDistType::kUniform, 0.0f, 360.0f };
        crystal.roll = c.roll;
        ctx->Yield();
        OpenFilterModal(ctx);
        const bool p_icon = ctx->ItemExists("**/" ICON_FA_CIRCLE_INFO "##p_tooltip_icon_filter_modal");
        const bool b_icon = ctx->ItemExists("**/" ICON_FA_CIRCLE_INFO "##b_tooltip_icon_filter_modal");
        if (p_icon != c.expect_p_icon) {
          wrong += std::string(" ") + c.label + ":p";
        }
        if (b_icon != c.expect_b_icon) {
          wrong += std::string(" ") + c.label + ":b";
        }
        ctx->ItemClick(kCancel);
        ctx->Yield(2);
      }
      IM_CHECK_STR_EQ(wrong.c_str(), "");
    };
  }

  // ---------------------------------------------------------------------------------------------
  // Remove Filter — a session-level intent, not a buffer edit.
  // ---------------------------------------------------------------------------------------------

  // Remove arms an intent that OK honours regardless of what the rows say afterwards. Retyping
  // after Remove used to re-create the filter, and the current single direction is deliberate:
  // aborting a Remove is Cancel's job. Typing into the row after arming is how this case tells the
  // two apart.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "remove_filter_wins_over_whatever_the_rows_say");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      SeedRaypathFilter("3-1-5");
      ctx->Yield();
      IM_CHECK(CommittedFilter() != nullptr);
      OpenFilterModal(ctx);

      // Always enabled: it is an intent flag, not something derived from row emptiness.
      IM_CHECK(!IsDisabled(ctx->ItemInfo("**/Remove Filter##filter")));
      ctx->ItemClick("**/Remove Filter##filter");
      ctx->Yield(2);
      // The row stays editable, and OK stays live — the intent bypasses row validation entirely.
      IM_CHECK(!IsDisabled(ctx->ItemInfo("**/##row_text_0")));
      IM_CHECK(!OkIsDisabled(ctx));

      EditRowText(ctx, "**/##row_text_0", "1-2");  // typing must NOT resurrect the filter
      ctx->Yield(2);
      ctx->ItemClick(kOk);
      ctx->Yield(2);
      IM_CHECK(CommittedFilter() == nullptr);
    };
  }

  // Cancel discards the intent along with the buffers, restoring the committed filter whole —
  // symmetry flags included, since those live beside the rows and are just as easy to drop.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "remove_then_cancel_puts_the_filter_back");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      gui::FilterConfig f;
      f.SetRaypath(gui::RaypathParams{ "3-1-5" });
      f.sym_p = true;
      f.sym_b = false;
      f.sym_d = true;
      gui::SetFilter(gui::g_state, gui::g_state.layers[0].entries[0], f);
      ctx->Yield();

      OpenFilterModal(ctx);
      ctx->ItemClick("**/Remove Filter##filter");
      ctx->Yield(2);
      ctx->ItemClick(kCancel);
      ctx->Yield(4);

      const auto* restored = CommittedFilter();
      IM_CHECK(restored != nullptr);
      IM_CHECK_STR_EQ(restored->RaypathText().c_str(), "3-1-5");
      IM_CHECK(restored->sym_p);
      IM_CHECK(!restored->sym_b);
      IM_CHECK(restored->sym_d);
    };
  }

  // Immediate mode has no OK to press, so the same click has to reach the document on the next
  // frame. The second assertion is the one that matters: applying the removal without routing it
  // through the structural lane would leave the display generation floor behind, and the preview
  // would keep showing rays the filter no longer admits.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "immediate_mode_applies_remove_on_the_next_frame");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      SeedRaypathFilter("3-1-5");
      gui::g_state.committed_epoch = 5;
      gui::g_state.display_epoch_floor = 0;
      // Baseline seeded with the filter present, so removing it is the diff the reconciler sees.
      gui::g_state.last_committed_state = gui::GuiState::ConfigSnapshot::From(gui::g_state);
      ctx->Yield();
      IM_CHECK_EQ(gui::g_state.display_epoch_floor, 0u);  // guard: the floor has not moved yet

      OpenFilterModal(ctx);
      ctx->ItemClick("**/Immediate##edit_modal");
      ctx->Yield(6);
      ctx->ItemClick("**/###filter_tab");
      ctx->Yield(2);
      ctx->ItemClick("**/Remove Filter##filter");
      ctx->Yield(2);

      IM_CHECK(CommittedFilter() == nullptr);
      IM_CHECK_EQ(gui::g_state.display_epoch_floor, gui::g_state.committed_epoch);

      ctx->ItemClick("**/Close##edit_modal");
      ctx->Yield(2);
      // Courtesy reset, not a guard: any fatal IM_CHECK above skips this line and leaves the flag
      // set. That is safe because ResetTestState() pins modal_immediate_mode back to false
      // (test_gui_main.cpp:142), so the next case starts from the documented default either way.
      // Left in so a reader of THIS case does not have to know that.
      gui::g_state.modal_immediate_mode = false;
    };
  }

  // The same delete-to-empty under Immediate mode, where there is no OK to press: the frame that
  // refills the blank row is the frame that pushes the buffers back, so the document must read
  // "no filter" at once — the same thing clearing the row's text would have committed.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "filter_editor", "immediate_mode_deleting_the_last_row_commits_as_no_filter");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      SeedRaypathFilter("3-1-5");
      ctx->Yield();
      IM_CHECK(CommittedFilter() != nullptr);

      OpenFilterModal(ctx);
      ctx->ItemClick("**/Immediate##edit_modal");
      ctx->Yield(6);
      ctx->ItemClick("**/###filter_tab");
      ctx->Yield(2);
      IM_CHECK(!IsDisabled(ctx->ItemInfo("**/" ICON_FA_XMARK "##row_delete_0")));  // the only row, and live
      ctx->ItemClick("**/" ICON_FA_XMARK "##row_delete_0");
      ctx->Yield(2);

      IM_CHECK(CommittedFilter() == nullptr);
      IM_CHECK(!ctx->ItemExists("**/##row_text_0"));
      IM_CHECK(ctx->ItemExists("**/##row_text_1"));  // the refilled blank row
      IM_CHECK(!IsDisabled(ctx->ItemInfo("**/" ICON_FA_XMARK "##row_delete_1")));

      ctx->ItemClick("**/Close##edit_modal");
      ctx->Yield(2);
      // Courtesy reset, as in the case above: ResetTestState() pins the flag back regardless.
      gui::g_state.modal_immediate_mode = false;
    };
  }

  // ---------------------------------------------------------------------------------------------
  // Copying the preview (592.3).
  // ---------------------------------------------------------------------------------------------

  // The live preview is plain text with no item of its own (see the file comment), so the copy
  // menu hangs on an invisible button laid over its rect, "##filter_preview_copy_target" — which
  // is also what makes the preview reachable from here at all. A right-click on it and "Copy"
  // puts the preview's text on the clipboard: the same string the commit path formats, spelled
  // here from the same two helpers on the row's text rather than as a literal. The clipboard is
  // the process-local stand-in test_gui_main.cpp installs.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "filter_editor", "the_preview_copies_its_text_on_right_click");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      IM_CHECK_STR_EQ(ImGui::GetClipboardText(), "");
      OpenFilterModal(ctx);
      AuthorRow(ctx, 0, "3-5 & entry:2");
      AuthorRow(ctx, 1, "1-2");
      IM_CHECK(ctx->ItemExists("**/##filter_preview_copy_target"));

      ctx->ItemClick("**/##filter_preview_copy_target", ImGuiMouseButton_Right);
      ctx->Yield(2);
      ctx->SetRef("//$FOCUSED");
      ctx->ItemClick("**/Copy");
      ctx->Yield(2);
      ctx->SetRef("");

      gui::SumOfProducts sop;
      for (const char* text : { "3-5 & entry:2", "1-2" }) {
        sop.push_back(gui::SummandText{ text, gui::ParseSummandText(text) });
      }
      const std::string want = gui::FormatSopExpansionPreview(sop);
      IM_CHECK(want.find("OR of 2 row(s):") == 0);
      IM_CHECK_STR_EQ(ImGui::GetClipboardText(), want.c_str());

      ctx->ItemClick(kCancel);
      ctx->Yield(2);
    };
  }
}
