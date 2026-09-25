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

#include <cstring>
#include <string>

#include "IconsFontAwesome6.h"
#include "gui/gui_constants.hpp"
#include "gui/log_sink.hpp"  // ImGuiLogSink — the panel's second gate
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
  "##TopBar/" ICON_FA_ROUTE " Analysis",
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
}
