// The window's chrome: the top bar's shape, the collapse strips, and where the log panel sits in
// the stack.
//
// What this suite is for. These are the parts of the shell that have no state of their own — they
// are about where things are and what is on top of what, which is a property of a rendered frame
// and of nothing else. Two of them are load-bearing in a way that is easy to miss: a top-bar slot
// whose width changes with its label makes every control to its right jump while a simulation
// starts and stops, and a floating window that cannot come forward is a log panel the user cannot
// read once they have clicked anything else.
//
// Deliberately NOT here, with where each lives instead. What the Run slot DOES is
// functional/test_run_lifecycle.cpp — which states in so many words that the slot's width across
// its three labels is a separate proposition belonging to the top bar's chrome, i.e. here. What the
// log panel CONTAINS is functional/test_log_panel.cpp. Whether the Colors button opens the Colors
// window is functional/test_color_window.cpp.
//
// What a user sees when these break: a toolbar that shifts sideways the moment they press Run, a
// collapsed panel they cannot get back, or a log panel that vanishes behind the side panels as soon
// as they touch anything.

#include <chrono>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>

#include "IconsFontAwesome6.h"
#include "gui/edit_modals.hpp"
#include "gui/file_io.hpp"  // DeserializeFromJson — the panorama case loads a scene like the GUI does
#include "gui/gui_constants.hpp"
#include "gui/log_sink.hpp"     // ImGuiLogSink — the panel's second gate
#include "test_screenshot.hpp"  // SavePng / StripAlpha — the archived skeleton capture
// imgui_internal.h is normally an anti-pattern. Z-order has no public reading: it is the ORDER of
// ImGuiContext::Windows, and the two rules relied on here (BringWindowToDisplayFront splices to the
// back; a window created with NoBringToFrontOnFocus is pushed to the front, i.e. the bottom) are
// documented in the convention block at the top of src/gui/app_panels.cpp. An ImGui upgrade that
// changes either must update both.
#include "imgui_internal.h"
#include "test_gui_shared.hpp"

namespace {

// The top bar's run slot, under each of its three labels.
const char* const kRunBtn = "##TopBar/" ICON_FA_PLAY " Run";
const char* const kStopBtn = "##TopBar/" ICON_FA_STOP " Stop";
const char* const kStoppingBtn = "##TopBar/" ICON_FA_STOP " Stopping...";
const char* const kContinueBtn = "##TopBar/" ICON_FA_FORWARD " Continue";
const char* const kSettingsBtn = "##TopBar/" ICON_FA_GEAR " Settings";
// The workspace switch's two labels — the slot shows the DESTINATION, so which one exists this
// frame depends on the mode (from Render it reads Analyze; from Analyze it reads Render).
const char* const kWorkspaceToAnalyzeBtn = "##TopBar/" ICON_FA_ROUTE " Analyze";
const char* const kWorkspaceToRenderBtn = "##TopBar/Render";
const char* const kRightToggleBtn = "##TopBar/" ICON_FA_CHEVRON_RIGHT "##right_panel_toggle";
const char* const kRevertBtn = "##TopBar/Revert";

// Every button on the top bar, in the order it is drawn. Revert is included on purpose while
// hidden: it is submitted at alpha 0 so the bar does not shift when it appears, and its hidden
// rectangle is the one that holds that space.
const char* const kTopBarButtons[] = {
  "##TopBar/" ICON_FA_CHEVRON_LEFT "##left_panel_toggle",
  kRunBtn,
  kContinueBtn,
  "##TopBar/New",
  "##TopBar/Open",
  "##TopBar/Save",
  "##TopBar/" ICON_FA_PALETTE " Colors",
  "##TopBar/" ICON_FA_ROUTE " Analyze",
  "##TopBar/" ICON_FA_FILE_LINES " Summary",
  kSettingsBtn,
  kRevertBtn,
  kRightToggleBtn,
};
constexpr int kTopBarButtonCount = static_cast<int>(sizeof(kTopBarButtons) / sizeof(kTopBarButtons[0]));

// Index of a window in ImGui's submission-order list, or -1. Later index means visually higher.
int WindowStackIndex(const char* name) {
  ImGuiContext* g = ImGui::GetCurrentContext();
  for (int i = 0; i < g->Windows.Size; ++i) {
    if (std::strcmp(g->Windows[i]->Name, name) == 0) {
      return i;
    }
  }
  return -1;
}

}  // namespace

void RegisterShellChromeTests(ImGuiTestEngine* engine) {
  // P1 / P13. The collapse toggle and the strip that brings the panel back, as one round trip —
  // half of it is not a feature: a panel that collapses and cannot be restored is a panel the user
  // has lost.
  //
  // The strip's button is drawn with an explicit screen position rather than by the layout, so the
  // click below is placed from the same geometry the drawing code uses. That is deliberate: the
  // button is an OverlayButton gated on `!io.WantCaptureMouse`, and the point of clicking it by
  // POSITION is that the gate is evaluated for a real pointer at a real place. Its occluded half —
  // the same gate correctly swallowing a click under a floating window — is the entry card's, in
  // functional/test_entry_management.cpp.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "shell_chrome", "collapsing_the_left_panel_hides_it_and_the_strip_brings_it_back");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      IM_CHECK(!gui::g_state.left_panel_collapsed);
      IM_CHECK(ctx->GetWindowByRef("##LeftPanel") != nullptr);

      // The toggle's label carries the chevron that points the way it will move, so the path
      // depends on the current state — expanded here.
      ctx->ItemClick("##TopBar/" ICON_FA_CHEVRON_LEFT "##left_panel_toggle");
      ctx->Yield(3);
      IM_CHECK(gui::g_state.left_panel_collapsed);
      // The window is not merely empty — RenderLeftPanel returns before Begin, so it stops being
      // submitted at all and the preview gains the space.
      ImGuiWindow* left = ctx->GetWindowByRef("##LeftPanel");
      IM_CHECK(left == nullptr || !left->WasActive);

      // The strip spans the space between the top bar and the status bar, with a square button
      // centred vertically in it (RenderCollapsedStrip, src/gui/app_panels.cpp). kCollapseBtnSize is
      // file-local there and is mirrored here rather than exported for one test.
      const ImGuiViewport* vp = ImGui::GetMainViewport();
      constexpr float kCollapseBtnSize = 20.0f;
      const float strip_h = vp->Size.y - gui::kTopBarHeight - gui::kStatusBarHeight;
      const float btn_y = gui::kTopBarHeight + (strip_h - kCollapseBtnSize) * 0.5f;
      ctx->MouseMoveToPos(ImVec2(vp->Pos.x + kCollapseBtnSize * 0.5f, vp->Pos.y + btn_y + kCollapseBtnSize * 0.5f));
      ctx->MouseClick(0);
      ctx->Yield(3);

      IM_CHECK(!gui::g_state.left_panel_collapsed);
      IM_CHECK(ctx->GetWindowByRef("##LeftPanel") != nullptr);
    };
  }

  // P3. The run slot holds three different labels over a run's lifetime, and the button is sized so
  // that all three occupy the same rectangle — otherwise every control to its right slides sideways
  // twice per run, which reads as the toolbar flinching.
  //
  // The three states are reached the way the product reaches them (a run intent plus the in-flight
  // stop latch), not by writing the label: a width that only holds when a test poses the button
  // would hold in no real run.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "shell_chrome", "the_run_slot_keeps_one_rectangle_across_its_three_labels");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      struct Slot {
        const char* name;
        gui::RunIntent intent;
        bool stop_inflight;
        const char* button;
      };
      const Slot kSlots[] = {
        { "idle", gui::RunIntent::kNone, false, kRunBtn },
        { "simulating", gui::RunIntent::kRunning, false, kStopBtn },
        { "stop draining", gui::RunIntent::kStopping, true, kStoppingBtn },
      };

      ResetTestState();
      ctx->Yield(3);

      float width = -1.0f;
      float right_neighbour_x = -1.0f;
      for (const Slot& s : kSlots) {
        gui::g_state.run_intent = s.intent;
        gui::g_state.dirty = false;
        gui::g_stop_inflight.store(s.stop_inflight);
        ctx->Yield(3);

        const ImGuiTestItemInfo btn = ctx->ItemInfo(s.button, ImGuiTestOpFlags_NoError);
        if (btn.ID == 0) {
          IM_ERRORF("%s: the run slot is not showing %s", s.name, s.button);
          continue;
        }
        // The neighbour is what the user actually notices moving: New, the first control to the
        // right of the execution group (Continue sits between, and is measured with the slot).
        const ImGuiTestItemInfo neighbour = ctx->ItemInfo("##TopBar/New", ImGuiTestOpFlags_NoError);
        if (width < 0.0f) {
          width = btn.RectFull.GetWidth();
          right_neighbour_x = neighbour.RectFull.Min.x;
          continue;
        }
        if (btn.RectFull.GetWidth() != width) {
          IM_ERRORF("%s: the slot is %.1f px wide, the first state measured %.1f", s.name,
                    static_cast<double>(btn.RectFull.GetWidth()), static_cast<double>(width));
        }
        if (neighbour.RectFull.Min.x != right_neighbour_x) {
          IM_ERRORF("%s: the control to the slot's right moved to x=%.1f from %.1f", s.name,
                    static_cast<double>(neighbour.RectFull.Min.x), static_cast<double>(right_neighbour_x));
        }

        if (ctx->IsError()) {
          break;
        }
      }
      IM_CHECK_GT(width, 0.0f);  // a run of three misses would leave this unset

      gui::g_state.run_intent = gui::RunIntent::kNone;
      gui::g_stop_inflight.store(false);
      ctx->Yield(2);
    };
  }

  // P56. The six background panels carry NoBringToFrontOnFocus so a stray click cannot raise one
  // over a modal; the log panel deliberately does NOT, because it is a floating window the user is
  // meant to be able to bring forward and read. That asymmetry is the proposition — and it is
  // invisible in any state, since both windows are "open" either way.
  //
  // The panel is gated twice in this harness (a CLI flag and the presence of a sink), and neither
  // gate is reset by ResetTestState, so both are restored by a guard rather than at the end of the
  // body — an assertion failure part-way through would otherwise leak the log panel into every case
  // that runs after this one.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "shell_chrome", "the_log_panel_can_come_forward_over_the_side_panels");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();

      struct LogPanelGateGuard {
        bool prev_enable;
        std::shared_ptr<gui::ImGuiLogSink> prev_sink;
        LogPanelGateGuard() : prev_enable(g_enable_log_panel), prev_sink(gui::g_imgui_log_sink) {
          g_enable_log_panel = true;
          if (!gui::g_imgui_log_sink) {
            gui::g_imgui_log_sink = std::make_shared<gui::ImGuiLogSink>();
          }
        }
        ~LogPanelGateGuard() {
          gui::g_imgui_log_sink = prev_sink;
          g_enable_log_panel = prev_enable;
          gui::g_state.log_panel_open = false;
        }
      } gate_guard;

      gui::g_state.log_panel_open = true;
      ctx->Yield(4);

      // Try to raise a background panel. Without its flag this would splice ##LeftPanel to the back
      // of the list, i.e. above the log panel.
      ctx->WindowFocus("##LeftPanel");
      ctx->Yield(2);

      const int log_idx = WindowStackIndex("##LogPanel");
      const int left_idx = WindowStackIndex("##LeftPanel");
      IM_CHECK_GE(log_idx, 0);
      IM_CHECK_GE(left_idx, 0);
      IM_CHECK_GT(log_idx, left_idx);
    };
  }
  // P3b. The top bar's buttons are measured, not left to their labels. Every button in the bar has
  // one frame height (Revert used to be a SmallButton, a frame-padding shorter than its neighbours),
  // and each group shares one width: Continue takes the run slot's width, so the execution group is
  // two equal buttons whatever the slot currently says, and New / Open / Save are three equal ones.
  //
  // Measured on the rendered items rather than recomputed from CalcTextSize, because the claim is
  // about what is drawn: a width computed and then not passed to the button is the defect this
  // would otherwise miss.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "shell_chrome", "top_bar_buttons_share_one_height_and_one_width_per_group");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(3);

      constexpr int kCount = kTopBarButtonCount;
      ImGuiTestItemInfo infos[kCount];
      for (int i = 0; i < kCount; ++i) {
        infos[i] = ctx->ItemInfo(kTopBarButtons[i], ImGuiTestOpFlags_NoError);
      }

      const float height = infos[1].RectFull.GetHeight();
      IM_CHECK_GT(height, 0.0f);
      for (int i = 0; i < kCount; ++i) {
        if (infos[i].ID == 0) {
          IM_ERRORF("the top bar has no %s", kTopBarButtons[i]);
        } else if (infos[i].RectFull.GetHeight() != height) {
          IM_ERRORF("%s is %.1f px tall, the run slot is %.1f", kTopBarButtons[i],
                    static_cast<double>(infos[i].RectFull.GetHeight()), static_cast<double>(height));
        }
      }

      // Indices into kTopBarButtons: the execution group is {1, 2}, the file group {3, 4, 5}.
      IM_CHECK_EQ(infos[2].RectFull.GetWidth(), infos[1].RectFull.GetWidth());
      IM_CHECK_EQ(infos[4].RectFull.GetWidth(), infos[3].RectFull.GetWidth());
      IM_CHECK_EQ(infos[5].RectFull.GetWidth(), infos[3].RectFull.GetWidth());
    };
  }

  // P3d. The ⚠ + Revert pair appears when the document is modified and disappears when it is not,
  // and no button on the bar moves either way — the pair is hidden by alpha, never by omission, so
  // its rectangle is held in both states. A pair drawn only when modified would slide the right
  // toggle (or, beside Continue where it used to sit, every group to its right) each time an edit
  // lands or a run completes, which is the toolbar flinching under the user's cursor.
  //
  // Both states are reached the way the product reaches them: a completed result plus the dirty
  // flag, which the frame-top reconcile turns into kModified, and the same result clean (kDone).
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "shell_chrome", "toggling_modified_moves_no_top_bar_button");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      gui::g_state.run_intent = gui::RunIntent::kLoaded;
      gui::g_state.dirty = false;
      ctx->Yield(3);
      IM_CHECK_EQ(static_cast<int>(gui::g_state.sim_state), static_cast<int>(gui::GuiState::SimState::kDone));

      ImGuiTestItemInfo clean[kTopBarButtonCount];
      for (int i = 0; i < kTopBarButtonCount; ++i) {
        clean[i] = ctx->ItemInfo(kTopBarButtons[i], ImGuiTestOpFlags_NoError);
      }

      gui::g_state.dirty = true;
      ctx->Yield(3);
      const bool reached_modified = gui::g_state.sim_state == gui::GuiState::SimState::kModified;
      ImGuiTestItemInfo modified[kTopBarButtonCount];
      for (int i = 0; i < kTopBarButtonCount; ++i) {
        modified[i] = ctx->ItemInfo(kTopBarButtons[i], ImGuiTestOpFlags_NoError);
      }

      gui::g_state.dirty = false;
      gui::g_state.run_intent = gui::RunIntent::kNone;
      ctx->Yield(2);

      IM_CHECK(reached_modified);
      for (int i = 0; i < kTopBarButtonCount; ++i) {
        if (clean[i].ID == 0 || modified[i].ID == 0) {
          IM_ERRORF("%s is missing from the top bar (clean: %s, modified: %s)", kTopBarButtons[i],
                    clean[i].ID == 0 ? "absent" : "present", modified[i].ID == 0 ? "absent" : "present");
        } else if (clean[i].RectFull.Min.x != modified[i].RectFull.Min.x ||
                   clean[i].RectFull.Max.x != modified[i].RectFull.Max.x) {
          IM_ERRORF("%s spans x=[%.1f, %.1f] unmodified but [%.1f, %.1f] modified", kTopBarButtons[i],
                    static_cast<double>(clean[i].RectFull.Min.x), static_cast<double>(clean[i].RectFull.Max.x),
                    static_cast<double>(modified[i].RectFull.Min.x), static_cast<double>(modified[i].RectFull.Max.x));
        }
      }
    };
  }

  // P3c. At the narrowest window the app allows, the whole bar fits: every group from the left
  // toggle through Settings, then the ⚠ + Revert pair and the right toggle at the right edge, with
  // at least one item gap between Settings and the pair. The bar has no scrollbar and no overflow
  // handling — a bar that did not fit would draw Revert over Settings, or push it off the window.
  //
  // Measured at its widest real content: with a colour class present the Colored / Full Spectrum
  // checkbox joins the feature group (showing "Full Spectrum", the longer label), and with no run
  // yet no class has matched anything, so the empty-composite warning pip is drawn beside it too.
  // The left part is read off the rendered frame (the right edge of Settings), and so is the right
  // end from Revert through the right toggle, which at any wider window sit flush together. The ⚠
  // glyph is text, not an item with a rectangle, so its width is the one thing taken from
  // CalcTextSize; the window floor comes from a constant.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "shell_chrome", "the_top_bar_fits_at_the_minimum_window_width");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      gui::ColorClassConfig cls;
      cls.visible = true;
      gui::g_state.raypath_color.push_back(cls);
      ctx->Yield(4);

      const ImGuiTestItemInfo checkbox =
          ctx->ItemInfo("##TopBar/Full Spectrum##CompositePreviewToggle", ImGuiTestOpFlags_NoError);
      const ImGuiTestItemInfo settings = ctx->ItemInfo(kSettingsBtn, ImGuiTestOpFlags_NoError);
      const ImGuiTestItemInfo revert = ctx->ItemInfo(kRevertBtn, ImGuiTestOpFlags_NoError);
      const ImGuiTestItemInfo right_toggle = ctx->ItemInfo(kRightToggleBtn, ImGuiTestOpFlags_NoError);
      ImGuiWindow* bar = ctx->GetWindowByRef("##TopBar");

      gui::g_state.raypath_color.clear();
      ctx->Yield(2);

      IM_CHECK(bar != nullptr);
      IM_CHECK_NE(checkbox.ID, 0u);  // the widest configuration really was the one measured
      IM_CHECK_NE(settings.ID, 0u);
      IM_CHECK_NE(revert.ID, 0u);
      IM_CHECK_NE(right_toggle.ID, 0u);
      IM_CHECK_GT(settings.RectFull.Min.x, checkbox.RectFull.Max.x);
      IM_CHECK_GT(revert.RectFull.Min.x, settings.RectFull.Max.x);

      const ImGuiStyle& style = ImGui::GetStyle();
      // Revert and the right toggle are flush (one item gap) at this window width, so the span from
      // Revert's left edge to the toggle's right edge is what the right end needs at any width.
      IM_CHECK_EQ(right_toggle.RectFull.Min.x - revert.RectFull.Max.x, style.ItemSpacing.x);
      const float warning_glyph_w = ImGui::CalcTextSize(ICON_FA_CIRCLE_EXCLAMATION).x;
      const float right_end_w =
          warning_glyph_w + style.ItemSpacing.x + (right_toggle.RectFull.Max.x - revert.RectFull.Min.x);
      const float required =
          (settings.RectFull.Max.x - bar->Pos.x) + style.ItemSpacing.x + right_end_w + style.WindowPadding.x;
      const float floor_width = static_cast<float>(gui::kMinWindowWidth);
      ctx->LogInfo("required=%.1f min_window=%.1f headroom=%.1f", static_cast<double>(required),
                   static_cast<double>(floor_width), static_cast<double>(floor_width - required));
      IM_CHECK_LE(required, floor_width);
    };
  }

  // ---- The Analyze workspace (the switch's other half, analyze_workspace.cpp) ----

  // Switching workspaces swaps the frame's layout and touches nothing else: the Analyze branch
  // replaces the three Render-mode panels with the workspace's windows and switches back; the
  // document neither dirties nor changes its simulation state (the switch is session view state),
  // and the frame with no preview publishes no preview viewport (level-triggered, so DoAnalyze's
  // IN_FRAME canvas falls back to the document resolution and the deferred blit skips).
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "shell_chrome", "workspace_switch_swaps_layout_and_stays_clean");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(3);
      IM_CHECK_EQ(gui::g_state.workspace.mode, gui::GuiState::WorkspaceMode::kRender);
      ImGuiWindow* left = ctx->GetWindowByRef("##LeftPanel");
      IM_CHECK(left != nullptr && left->Active);

      const bool dirty_before = gui::g_state.dirty;
      const auto sim_state_before = gui::g_state.sim_state;
      ctx->ItemClick(kWorkspaceToAnalyzeBtn);
      ctx->Yield(3);

      IM_CHECK_EQ(gui::g_state.workspace.mode, gui::GuiState::WorkspaceMode::kAnalyze);
      IM_CHECK(left != nullptr && !left->Active);  // not submitted this frame
      ImGuiWindow* middle = ctx->GetWindowByRef("##AnalyzeMiddle");
      IM_CHECK(middle != nullptr && middle->Active);
      IM_CHECK_EQ(gui::g_preview_vp.active, false);
      IM_CHECK_EQ(gui::g_state.dirty, dirty_before);
      IM_CHECK_EQ(static_cast<int>(gui::g_state.sim_state), static_cast<int>(sim_state_before));

      ctx->ItemClick(kWorkspaceToRenderBtn);
      ctx->Yield(3);
      IM_CHECK_EQ(gui::g_state.workspace.mode, gui::GuiState::WorkspaceMode::kRender);
      IM_CHECK(left != nullptr && left->Active);
      IM_CHECK(middle != nullptr && !middle->Active);
    };
  }

  // The Analyze left column hosts the scene editors through the Render workspace's own functions:
  // one row per entry (click opens the SAME entry editor modal a card opens), the shared +Layer
  // primitive, and the Sun/Simulation controls (RenderSceneControls). A row of the compact list
  // and the cards are two hosts for one editing semantics — this case pins that the host swap
  // keeps the editor reachable and the structure primitives live.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "shell_chrome", "workspace_left_column_hosts_the_scene_editors");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      IM_CHECK_EQ(static_cast<int>(gui::g_state.layers.size()), 1);
      IM_CHECK_EQ(static_cast<int>(gui::g_state.layers[0].entries.size()), 1);

      ctx->ItemClick(kWorkspaceToAnalyzeBtn);
      ctx->Yield(3);

      // The default document's one entry, as a row (** = any host window).
      const ImGuiTestItemInfo row = ctx->ItemInfo("**/Layer 1, entry 1: Crystal##crys_0_0", ImGuiTestOpFlags_NoError);
      IM_CHECK_NE(row.ID, 0u);
      // The Sun controls are the Render right panel's own widgets, now hosted in this column —
      // SliderWithInput names its slider "##<label>_slider". The slider stays interactive at the
      // 300px column (its rect has real interactive width; PrepareSliderLayout's 40px floor is
      // the readability bound, and the measured width is recorded for the risk-9 ledger).
      const ImGuiTestItemInfo altitude = ctx->ItemInfo("**/##Altitude_slider", ImGuiTestOpFlags_NoError);
      IM_CHECK_NE(altitude.ID, 0u);
      ctx->LogInfo("analyze left column @%.0f px: Altitude slider w=%.1f",
                   static_cast<double>(gui::kAnalyzeLeftPanelWidth), static_cast<double>(altitude.RectFull.GetWidth()));
      IM_CHECK_GT(altitude.RectFull.GetWidth(), 40.0f);

      // A row click opens the entry editor — the modal the cards open, not a second editor.
      ctx->ItemClick("**/Layer 1, entry 1: Crystal##crys_0_0");
      ctx->Yield(4);
      IM_CHECK_EQ(gui::IsEditModalOpen(), true);
      ctx->WindowClose("Edit Entry");
      ctx->Yield(4);
      IM_CHECK_EQ(gui::IsEditModalOpen(), false);

      // +Layer is the shared primitive: the new layer's seed entry appears as a row of the list.
      ctx->ItemClick("**/+ Layer");
      ctx->Yield(3);
      IM_CHECK_EQ(static_cast<int>(gui::g_state.layers.size()), 2);
      const ImGuiTestItemInfo seeded =
          ctx->ItemInfo("**/Layer 2, entry 1: Crystal##crys_1_0", ImGuiTestOpFlags_NoError);
      IM_CHECK_NE(seeded.ID, 0u);
    };
  }

  // The middle area's divider: dragging it moves the row ratio (session memory) and the clamp
  // holds at both ends — the four-column skeleton's "上下比例可调" contract.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "shell_chrome", "workspace_splitter_drag_moves_the_ratio");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->ItemClick(kWorkspaceToAnalyzeBtn);
      ctx->Yield(3);
      const float before = gui::g_state.workspace.middle_rows_ratio;
      const ImGuiTestItemInfo splitter = ctx->ItemInfo("**/##AnalyzeRowsSplitter", ImGuiTestOpFlags_NoError);
      IM_CHECK_NE(splitter.ID, 0u);
      const ImVec2 grab((splitter.RectFull.Min.x + splitter.RectFull.Max.x) * 0.5f,
                        (splitter.RectFull.Min.y + splitter.RectFull.Max.y) * 0.5f);
      ctx->MouseMoveToPos(grab);
      ctx->Yield(2);
      ctx->MouseDown(0);
      ctx->Yield(2);
      ctx->MouseMoveToPos(ImVec2(grab.x, grab.y + 120.0f));
      ctx->Yield(4);
      ctx->MouseUp(0);
      ctx->Yield(2);
      IM_CHECK_GT(gui::g_state.workspace.middle_rows_ratio, before);
      IM_CHECK_LE(gui::g_state.workspace.middle_rows_ratio, 0.85f);

      // Past the top edge the clamp pins at 0.15 — the ratio never leaves its band. The first
      // drag moved the splitter (the ratio re-laid-out the rows), so re-query where it sits now.
      const ImGuiTestItemInfo splitter2 = ctx->ItemInfo("**/##AnalyzeRowsSplitter", ImGuiTestOpFlags_NoError);
      IM_CHECK_NE(splitter2.ID, 0u);
      const ImVec2 grab2((splitter2.RectFull.Min.x + splitter2.RectFull.Max.x) * 0.5f,
                         (splitter2.RectFull.Min.y + splitter2.RectFull.Max.y) * 0.5f);
      ctx->MouseMoveToPos(grab2);
      ctx->Yield(2);
      ctx->MouseDown(0);
      ctx->Yield(2);
      ctx->MouseMoveToPos(ImVec2(grab2.x, grab2.y - 2000.0f));
      ctx->Yield(4);
      ctx->MouseUp(0);
      ctx->Yield(2);
      // The ratio stays inside its band and moved the drag's way. (The harness's pointer cannot
      // leave the OS window, so the drag cannot push the pivot all the way to the 0.15 pin — the
      // band, not the exact pin, is the product contract a pointer-driven test can reach.)
      IM_CHECK_GE(gui::g_state.workspace.middle_rows_ratio, 0.15f);
      IM_CHECK_LT(gui::g_state.workspace.middle_rows_ratio, 0.66f);
    };
  }

  // Pick-link in the Analyze workspace: the list rows complete the share with the cards' own
  // predicate (one PickLinkRowStateFor, two hosts), and the completion reopens the editor on the
  // pick's source — the Render flow's shape, not a second semantics.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "shell_chrome", "workspace_pick_link_completes_from_list_rows");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->ItemClick(kWorkspaceToAnalyzeBtn);
      ctx->Yield(3);
      // A second entry on its own pool slot, the share's target.
      ctx->ItemClick("**/+ Layer");
      ctx->Yield(3);
      IM_CHECK_EQ(static_cast<int>(gui::g_state.layers.size()), 2);

      // Open the editor from the list, change a visible field, arm the pick from the modal.
      ctx->ItemClick("**/Layer 1, entry 1: Crystal##crys_0_0");
      ctx->Yield(4);
      IM_CHECK_EQ(gui::IsEditModalOpen(), true);
      const int source_crystal_before = gui::g_state.layers[0].entries[0].crystal_id;
      const float orig_h = gui::g_state.crystals[source_crystal_before].height.center;
      ctx->ItemInputValue("**/##Height##modal_cr_input", orig_h + 2.0f);
      ctx->Yield(2);
      ctx->ItemClick("**/Link to...##share");
      ctx->Yield(4);
      IM_CHECK(gui::g_state.pick_link_source.has_value());
      IM_CHECK_EQ(gui::g_state.pick_link_source->layer_idx, 0);
      IM_CHECK_EQ(gui::IsEditModalOpen(), false);

      // Completing on the second layer's row rebinds the ARMED entry to the clicked row's ids
      // ("A adopts B": the armed source adopts the model's crystal/filter) and reopens the
      // editor on the source entry, with the armed edit committed on the way out — onto the
      // crystal the source referenced before the rebind.
      const int model_crystal = gui::g_state.layers[1].entries[0].crystal_id;
      ctx->ItemClick("**/Layer 2, entry 1: Crystal##crys_1_0");
      ctx->Yield(4);
      IM_CHECK(!gui::g_state.pick_link_source.has_value());
      IM_CHECK_EQ(gui::g_state.layers[0].entries[0].crystal_id, model_crystal);
      IM_CHECK_EQ(gui::IsEditModalOpen(), true);
      IM_CHECK_EQ(gui::GetEditModalTarget().layer_idx, 0);
      IM_CHECK_EQ(gui::g_state.crystals[source_crystal_before].height, orig_h + 2.0f);
      ctx->WindowClose("Edit Entry");
      ctx->Yield(2);

      // Esc cancels a pick armed in this workspace, as it does beside the cards.
      ctx->ItemClick("**/Layer 1, entry 1: Crystal##crys_0_0");
      ctx->Yield(4);
      ctx->ItemClick("**/Link to...##share");
      ctx->Yield(4);
      IM_CHECK(gui::g_state.pick_link_source.has_value());
      ctx->KeyPress(ImGuiKey_Escape);
      ctx->Yield(3);
      IM_CHECK(!gui::g_state.pick_link_source.has_value());
    };
  }

  // Document switch inside the Analyze workspace behaves as in Render: the open editor closes
  // with the frontend reset (one ResetFrontendState, both modes), the fresh document's single
  // entry shows up in the list. DoNew resets the workspace selection along with the whole
  // GuiState (every session field's clean-slate rule — the analysis window closes the same way),
  // so the fresh document starts in Render and the case re-enters Analyze to check the list.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "shell_chrome", "workspace_document_switch_closes_the_editor");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->ItemClick(kWorkspaceToAnalyzeBtn);
      ctx->Yield(3);
      ctx->ItemClick("**/+ Layer");
      ctx->Yield(3);
      ctx->ItemClick("**/Layer 1, entry 1: Crystal##crys_0_0");
      ctx->Yield(4);
      IM_CHECK_EQ(gui::IsEditModalOpen(), true);

      gui::DoNew();
      ctx->Yield(4);
      IM_CHECK_EQ(gui::IsEditModalOpen(), false);
      IM_CHECK_EQ(static_cast<int>(gui::g_state.layers.size()), 1);
      IM_CHECK_EQ(gui::g_state.workspace.mode, gui::GuiState::WorkspaceMode::kRender);
      ctx->ItemClick(kWorkspaceToAnalyzeBtn);
      ctx->Yield(3);
      const ImGuiTestItemInfo fresh = ctx->ItemInfo("**/Layer 1, entry 1: Crystal##crys_0_0", ImGuiTestOpFlags_NoError);
      IM_CHECK_NE(fresh.ID, 0u);
    };
  }

  // The skeleton's core controls stay reachable at the 1280x800 acceptance size (the plan's R9):
  // every interactive element of the Analyze shell submits with a real rect at the smaller
  // window, and a fullframe capture of this exact state is archived for the record. The window
  // basis audit restores the shared window at the next ResetTestState, so the resize is
  // pool-safe.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "shell_chrome", "workspace_skeleton_reachable_at_1280x800");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->ItemClick(kWorkspaceToAnalyzeBtn);
      ctx->Yield(4);
      // Archive the default-size (1600x980) skeleton first, then repeat at the 1280x800
      // acceptance size.
      ctx->MouseMoveToPos(ImVec2(-100.0f, -100.0f));
      ctx->Yield(2);
      auto capture_to = [&](const char* filename) {
        g_fullframe_capture.Reset();
        g_fullframe_capture.requested.store(true);
        for (int i = 0; i < 10 && !g_fullframe_capture.done.load(); ++i) {
          ctx->Yield(1);
        }
        IM_CHECK(g_fullframe_capture.done.load());
        if (g_fullframe_capture.done.load()) {
          const std::string path = GuiTestTempPath(filename).string();
          auto rgb = lumice::test::StripAlpha(g_fullframe_capture.pixels.data(), g_fullframe_capture.width,
                                              g_fullframe_capture.height);
          IM_CHECK(lumice::test::SavePng(path.c_str(), rgb.data(), g_fullframe_capture.width,
                                         g_fullframe_capture.height, 3));
          ctx->LogInfo("archived: %s (%dx%d)", path.c_str(), g_fullframe_capture.width, g_fullframe_capture.height);
        }
      };
      capture_to("workspace_skeleton_1600x980.png");

      g_window_size_test.width = 1280;
      g_window_size_test.height = 800;
      g_window_size_test.requested.store(true);
      for (int i = 0; i < 20 && !g_window_size_test.done.load(); ++i) {
        ctx->Yield(1);
      }
      IM_CHECK(g_window_size_test.done.load());
      ctx->Yield(3);

      const char* kReachable[] = {
        kWorkspaceToRenderBtn,                     // the way back (the slot now reads Render)
        "**/Layer 1, entry 1: Crystal##crys_0_0",  // the list row
        "**/+ Layer",                              // the shared add primitive
        "**/##Altitude_slider",                    // the Sun editor
        "**/##AnalyzeRowsSplitter",                // the divider
      };
      for (const char* ref : kReachable) {
        const ImGuiTestItemInfo info = ctx->ItemInfo(ref, ImGuiTestOpFlags_NoError);
        if (info.ID == 0u) {
          IM_ERRORF("not reachable at 1280x800: %s", ref);
          if (ctx->IsError()) {
            break;
          }
          continue;
        }
        if (!(info.RectFull.GetWidth() > 1.0f && info.RectFull.GetHeight() > 1.0f)) {
          IM_ERRORF("zero-area at 1280x800: %s", ref);
          if (ctx->IsError()) {
            break;
          }
        }
      }
      // The five not-wired regions are child WINDOWS holding plain text — no items for the item
      // registry. Their reachability is "submitted this frame", checked over the window list
      // (child names embed the hash suffix; the substring is the stable part).
      // The panorama is the sixth region and deliberately absent here: without a render it
      // shows the empty state and publishes no picture region (its own case pins the render
      // side).
      const char* kRegions[] = {
        "##AnalyzeShareList", "##AnalyzeRaypathView", "##AnalyzeRaypath3D", "##AnalyzeSunGlobe", "##AnalyzeDetails",
      };
      int regions_found = 0;
      for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
        if (w == nullptr || !w->Active) {
          continue;
        }
        for (const char* region : kRegions) {
          if (std::strstr(w->Name, region) != nullptr) {
            regions_found++;
          }
        }
      }
      IM_CHECK_EQ(regions_found, 5);
      IM_CHECK_EQ(gui::g_panorama_vp.active, false);

      // Archive the exact state this case verified (the pointer is already parked off-frame).
      capture_to("workspace_skeleton_1280x800.png");

      // Restore the shared window's basis HERE: a resize the next case's entry audit has to
      // undo counts as a window-basis leak and fails the run's exit code by design.
      g_window_size_test.width = gui::kInitWindowWidth;
      g_window_size_test.height = gui::kInitWindowHeight;
      g_window_size_test.requested.store(true);
      for (int i = 0; i < 20 && !g_window_size_test.done.load(); ++i) {
        ctx->Yield(1);
      }
      IM_CHECK(g_window_size_test.done.load());
      ctx->Yield(2);
    };
  }

  // The panorama is the committed texture's second view: after a real render, the Analyze
  // workspace publishes the panorama's region to the deferred pass (g_panorama_vp), the
  // hemisphere switch flips the view's session state and the published uniform with it; without
  // a render the empty state publishes nothing (the skeleton case pins that side).
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "shell_chrome", "workspace_panorama_publishes_after_a_render");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      // The case before this one resized the shared window (the 1280 acceptance check);
      // ResetTestState queued the basis restore, which lands as an async OS resize a few frames
      // from now. Wait it out BEFORE the run: a resize that lands after DoRun's commit writes a
      // view field and dirties the document, and kModified then masks the run's completion.
      for (int i = 0; i < 40 && ImGui::GetIO().DisplaySize.x != static_cast<float>(gui::kInitWindowWidth); ++i) {
        ctx->Yield(1);
      }
      ctx->Yield(3);
      // A server of our own, torn down before the case returns (the sim-smoke cases' rule: the
      // harness's shared server pointer must not outlive the case that made it).
      gui::g_server = LUMICE_CreateServer();
      LUMICE_SetLogLevel(gui::g_server, static_cast<LUMICE_LogLevel>(g_core_log_level));

      // The smallest render this suite does: a tiny halo scene, lowest resolution, a sliver of a
      // budget. The panorama's contract is about the published region, not picture quality.
      {
        constexpr const char* kScene = R"({
          "crystal": [{"id": 1, "type": "prism", "shape": {"height": 1.2},
                       "axis": {"zenith": {"type": "uniform", "mean": 90, "std": 360},
                                "azimuth": {"type": "uniform", "mean": 0, "std": 360}}}],
          "filter": [],
          "scene": {"light_source": {"type": "sun", "altitude": 20.0, "spectrum": "D65"},
                    "ray_num": 100000, "max_hits": 7,
                    "scattering": [{"prob": 0.0, "entries": [{"crystal": 1, "proportion": 10}]}]},
          "render": [{"id": 1, "lens": {"type": "linear", "fov": 60},
                      "resolution": [64, 64], "view": {"elevation": 42}}]
        })";
        std::istringstream in(kScene);
        std::string json_str((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        IM_CHECK(gui::DeserializeFromJson(json_str, gui::g_state));
      }
      gui::g_state.renderer.sim_resolution_index = 0;
      gui::g_state.sim.infinite = false;
      // The registry's slider domain floors the budget at kRayNumMinMillions (0.1M) — a smaller
      // value would be clamped back by the Render right panel's slider every frame and dirty the
      // document over the run, masking kDone with kModified.
      gui::g_state.sim.ray_num_millions = 0.1f;

      gui::DoRun(/*user_initiated=*/true);
      const auto wait_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
      while (gui::g_state.sim_state != gui::GuiState::SimState::kDone &&
             std::chrono::steady_clock::now() < wait_deadline) {
        ctx->Yield(1);
      }
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)gui::GuiState::SimState::kDone);
      IM_CHECK(gui::g_preview.HasTexture());

      // Entering Analyze now publishes the picture region (the empty case before the run
      // published nothing — ResetTestState's fresh document guarantees that ordering).
      IM_CHECK_EQ(gui::g_state.workspace.mode, gui::GuiState::WorkspaceMode::kRender);
      ctx->ItemClick(kWorkspaceToAnalyzeBtn);
      ctx->Yield(4);
      IM_CHECK_EQ(gui::g_panorama_vp.active, true);
      IM_CHECK_GT(gui::g_panorama_vp.vp_w, 0);
      IM_CHECK_GT(gui::g_panorama_vp.vp_h, 0);
      // The view is the panorama's own: the equal-area fisheye pitched straight up (the upper
      // hemisphere), the document's lens untouched.
      IM_CHECK_EQ(gui::g_panorama_vp.params.view_proj.lens_type, gui::kLensTypeFisheyeEqualArea);
      IM_CHECK_EQ(gui::g_panorama_vp.params.view_proj.fov, 180.0f);
      IM_CHECK_EQ(gui::g_panorama_vp.params.view_proj.elevation, 90.0f);
      IM_CHECK_EQ(gui::g_panorama_vp.params.view_proj.visible, gui::kVisibleFull);
      IM_CHECK_EQ(gui::g_state.renderer.lens_type, gui::kLensTypeLinear);

      // The hemisphere switch is a view state: it flips the session bit and the uniform the
      // deferred pass reads, and never touches the document.
      ctx->ItemClick("**/Lower##panorama_hemisphere");
      ctx->Yield(3);
      IM_CHECK_EQ(gui::g_state.workspace.panorama_upper, false);
      IM_CHECK_EQ(gui::g_panorama_vp.params.view_proj.elevation, -90.0f);
      ctx->ItemClick("**/Upper##panorama_hemisphere");
      ctx->Yield(3);
      IM_CHECK_EQ(gui::g_state.workspace.panorama_upper, true);

      // ---- The cone pick over the panorama ----
      // Arm CONE + the pick directly (this case is about the panorama's half of the flow), and
      // click inside the picture disk at half its radius.
      gui::g_state.analysis.roi_mode = LUMICE_RAYPATH_ROI_CONE;
      gui::g_state.analysis.pick_armed = true;
      ctx->Yield(2);
      // The picture region, by name, over the window list (a child window, no items).
      ImGuiWindow* pano = nullptr;
      for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
        if (w != nullptr && w->Active && std::strstr(w->Name, "##AnalyzePanorama") != nullptr) {
          pano = w;
        }
      }
      IM_CHECK(pano != nullptr);
      const ImVec2 disk_centre((pano->Pos.x + pano->Size.x * 0.5f), (pano->Pos.y + pano->Size.y * 0.5f));
      const float disk_radius = std::min(pano->Size.x, pano->Size.y) * 0.5f;
      ctx->MouseMoveToPos(ImVec2(disk_centre.x + disk_radius * 0.5f, disk_centre.y));
      ctx->Yield(2);
      ctx->MouseClick(0);
      ctx->Yield(3);
      IM_CHECK(gui::g_state.analysis.cone_center_valid);
      IM_CHECK(!gui::g_state.analysis.pick_armed);
      // The closed-form anchor: a click at half the disk's radius sits at
      // theta = 2*asin(r_norm * sin(45 deg)) from the zenith — derived from the equal-area rim
      // at theta = fov/2, NOT from the inverse under test.
      const float cz = gui::g_state.analysis.cone_center_dir[2];
      const float theta_from_zenith = std::acos(std::clamp(-cz, -1.0f, 1.0f)) * 180.0f / 3.14159265358979323846f;
      const float expected_theta = 2.0f * std::asin(0.5f * 0.70710678118654752440f) * 180.0f / 3.14159265358979323846f;
      ctx->LogInfo("picked theta=%.2f expected=%.2f", static_cast<double>(theta_from_zenith),
                   static_cast<double>(expected_theta));
      IM_CHECK(std::abs(theta_from_zenith - expected_theta) < 2.0f);
      IM_CHECK_LT(gui::g_state.analysis.cone_center_dir[2], 0.0f);  // upper-sky travel direction

      LUMICE_StopServer(gui::g_server);
      LUMICE_DestroyServer(gui::g_server);
      gui::g_server = nullptr;
      gui::g_state.run_intent = gui::RunIntent::kNone;
    };
  }
}
