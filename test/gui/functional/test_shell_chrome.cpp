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
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>

#include "IconsFontAwesome6.h"
#include "gui/analyze_map_view.hpp"  // kMapFullSkyRadiusDeg — the Full-sky reset's expected value
#include "gui/edit_modals.hpp"
#include "gui/file_io.hpp"  // DeserializeFromJson — the panorama case loads a scene like the GUI does
#include "gui/gui_constants.hpp"
#include "gui/log_sink.hpp"        // ImGuiLogSink — the panel's second gate
#include "gui/theme.hpp"           // UiPx — the rendered gap reads the rule through the one outlet
#include "gui/toolbar_layout.hpp"  // TopBarGroupGapPt — the fit pin reads the rule's own function
#include "gui/user_defaults.hpp"   // kFactoryUiScaleMultiplier — the scale leg restores through it
#include "test_screenshot.hpp"     // SavePng / StripAlpha — the archived skeleton capture
#include "util/sky_direction.hpp"  // kDeg2Rad — the sky-point intent's direction convention
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
// The workspace switch's two cells — a segmented pair, both drawn every frame. The tint on the
// current mode's cell is the only mode-dependent part: its click is a no-op, the other's switches.
const char* const kWorkspaceToAnalyzeBtn = "##TopBar/Analyze";
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
  "##TopBar/" ICON_FA_FILE,
  "##TopBar/" ICON_FA_FOLDER_OPEN,
  "##TopBar/" ICON_FA_FLOPPY_DISK,
  "##TopBar/" ICON_FA_PALETTE " Colors",
  "##TopBar/" ICON_FA_FILE_LINES,
  kWorkspaceToRenderBtn,
  kWorkspaceToAnalyzeBtn,
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

// Set the shared window's size and wait for it to land — the splitters case's local shape,
// gathered once for the basis legs below.
void ApplyWindowBasis(ImGuiTestContext* ctx, int width, int height) {
  g_window_size_test.apply_aspect = false;
  g_window_size_test.width = width;
  g_window_size_test.height = height;
  g_window_size_test.done.store(false);
  g_window_size_test.requested.store(true);
  for (int i = 0; i < 20 && !g_window_size_test.done.load(); ++i) {
    ctx->Yield(1);
  }
  IM_CHECK(g_window_size_test.done.load());
  ctx->Yield(3);
}

// A live region child window of the workspace (##AnalyzeRaypathView / ##AnalyzeRaypath3D /
// ##AnalyzeSunGlobe): the child name carries a hash suffix, the fragment is the stable part.
ImGuiWindow* AnalyzeRegionWindow(const char* fragment) {
  for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
    if (w != nullptr && w->Active && std::strstr(w->Name, fragment) != nullptr) {
      return w;
    }
  }
  return nullptr;
}

// The containment evidence form: the item's FULL rectangle inside the host window's inner clip
// rect, four edges. RectClipped comparisons and RectFull area checks do not pass for containment
// — a half-clipped control satisfies both while being unreachable.
void AssertRectContained(ImGuiTestContext* ctx, const char* ref, ImGuiWindow* host) {
  const ImGuiTestItemInfo info = ctx->ItemInfo(ref, ImGuiTestOpFlags_NoError);
  if (info.ID == 0u) {
    IM_ERRORF("not found for containment in %s: %s", host->Name, ref);
    return;
  }
  IM_CHECK_GE(info.RectFull.Min.x, host->InnerClipRect.Min.x - 0.5f);
  IM_CHECK_LE(info.RectFull.Max.x, host->InnerClipRect.Max.x + 0.5f);
  IM_CHECK_GE(info.RectFull.Min.y, host->InnerClipRect.Min.y - 0.5f);
  IM_CHECK_LE(info.RectFull.Max.y, host->InnerClipRect.Max.y + 0.5f);
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
        const ImGuiTestItemInfo neighbour = ctx->ItemInfo("##TopBar/" ICON_FA_FILE, ImGuiTestOpFlags_NoError);
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
  // CalcTextSize.
  //
  // Measured at the REAL minimum, not analytically at the test basis: the shared window is
  // resized to kMinWindowWidth through the main-thread handshake (GLFW is main-thread-only; the
  // request waits for the reply and the resize is ScopedWindowBasis-guarded, so a red below never
  // leaks the basis), and the frame's own item gaps are the adaptive rule's minimum — the same
  // gaps the fit budget must absorb. The wide end is pinned right after: back at the test basis
  // (= kTopBarGroupGapAnchorWidthPt), the same boundary measures the maximum. The boundary read
  // is Analyze-cell → Settings, the one separator whose two neighbours are always drawn (every
  // other boundary has a conditional member on one side).
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "shell_chrome", "the_top_bar_fits_at_the_minimum_window_width");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      gui::ColorClassConfig cls;
      cls.visible = true;
      gui::g_state.raypath_color.push_back(cls);
      ctx->Yield(4);

      // The pure function, pinned directly: both ends clamp, the middle never dips.
      IM_CHECK_EQ(gui::TopBarGroupGapPt(static_cast<float>(gui::kMinWindowWidth)), gui::kTopBarGroupGapMinPt);
      IM_CHECK_EQ(gui::TopBarGroupGapPt(gui::kTopBarGroupGapAnchorWidthPt), gui::kTopBarGroupGapMaxPt);
      IM_CHECK_EQ(gui::TopBarGroupGapPt(gui::kTopBarGroupGapAnchorWidthPt * 4.0f), gui::kTopBarGroupGapMaxPt);
      float prev_gap = gui::TopBarGroupGapPt(static_cast<float>(gui::kMinWindowWidth));
      int monotonicity_violations = 0;
      for (int w = gui::kMinWindowWidth + 64; w <= static_cast<int>(gui::kTopBarGroupGapAnchorWidthPt); w += 64) {
        const float gap = gui::TopBarGroupGapPt(static_cast<float>(w));
        monotonicity_violations += (gap < prev_gap) ? 1 : 0;
        prev_gap = gap;
      }
      IM_CHECK_EQ(monotonicity_violations, 0);

      const float floor_width = static_cast<float>(gui::kMinWindowWidth);
      const ScopedWindowBasis window_basis(ctx);
      g_window_size_test.apply_aspect = false;
      g_window_size_test.width = gui::kMinWindowWidth;
      g_window_size_test.height = gui::kMinWindowHeight;
      g_window_size_test.done.store(false);
      g_window_size_test.requested.store(true);
      for (int i = 0; i < 20 && !g_window_size_test.done.load(); ++i) {
        ctx->Yield(1);
      }
      IM_CHECK(g_window_size_test.done.load());
      ctx->Yield(3);

      const ImGuiTestItemInfo checkbox =
          ctx->ItemInfo("##TopBar/Full Spectrum##CompositePreviewToggle", ImGuiTestOpFlags_NoError);
      const ImGuiTestItemInfo analyze_cell = ctx->ItemInfo(kWorkspaceToAnalyzeBtn, ImGuiTestOpFlags_NoError);
      const ImGuiTestItemInfo settings = ctx->ItemInfo(kSettingsBtn, ImGuiTestOpFlags_NoError);
      const ImGuiTestItemInfo revert = ctx->ItemInfo(kRevertBtn, ImGuiTestOpFlags_NoError);
      const ImGuiTestItemInfo right_toggle = ctx->ItemInfo(kRightToggleBtn, ImGuiTestOpFlags_NoError);
      ImGuiWindow* bar = ctx->GetWindowByRef("##TopBar");

      IM_CHECK(bar != nullptr);
      IM_CHECK_NE(checkbox.ID, 0u);  // the widest configuration really was the one measured
      IM_CHECK_NE(analyze_cell.ID, 0u);
      IM_CHECK_NE(settings.ID, 0u);
      IM_CHECK_NE(revert.ID, 0u);
      IM_CHECK_NE(right_toggle.ID, 0u);
      IM_CHECK_GT(settings.RectFull.Min.x, analyze_cell.RectFull.Max.x);
      IM_CHECK_GT(revert.RectFull.Min.x, settings.RectFull.Max.x);

      const ImGuiStyle& style = ImGui::GetStyle();
      // The frame at kMinWindowWidth renders the rule's minimum, exactly (the pushed ItemSpacing
      // reproduces the target through exact float ops — same-source equality per the assertion
      // discipline: renderer and test read the one pure function).
      const float min_gap = gui::UiPx(gui::TopBarGroupGapPt(floor_width));
      IM_CHECK_EQ(settings.RectFull.Min.x - analyze_cell.RectFull.Max.x, min_gap);
      // Revert and the right toggle are flush (one item gap) at this window width, so the span from
      // Revert's left edge to the toggle's right edge is what the right end needs at any width.
      IM_CHECK_EQ(right_toggle.RectFull.Min.x - revert.RectFull.Max.x, style.ItemSpacing.x);
      const float warning_glyph_w = ImGui::CalcTextSize(ICON_FA_CIRCLE_EXCLAMATION).x;
      const float right_end_w =
          warning_glyph_w + style.ItemSpacing.x + (right_toggle.RectFull.Max.x - revert.RectFull.Min.x);
      const float required =
          (settings.RectFull.Max.x - bar->Pos.x) + style.ItemSpacing.x + right_end_w + style.WindowPadding.x;
      ctx->LogInfo("required=%.1f min_window=%.1f headroom=%.1f", static_cast<double>(required),
                   static_cast<double>(floor_width), static_cast<double>(floor_width - required));
      IM_CHECK_LE(required, floor_width);

      // The wide end: back at the test basis (== the anchor), the same boundary reads the maximum.
      g_window_size_test.apply_aspect = false;
      g_window_size_test.width = gui::kInitWindowWidth;
      g_window_size_test.height = gui::kInitWindowHeight;
      g_window_size_test.done.store(false);
      g_window_size_test.requested.store(true);
      for (int i = 0; i < 20 && !g_window_size_test.done.load(); ++i) {
        ctx->Yield(1);
      }
      IM_CHECK(g_window_size_test.done.load());
      ctx->Yield(3);

      const ImGuiTestItemInfo analyze_cell_wide = ctx->ItemInfo(kWorkspaceToAnalyzeBtn, ImGuiTestOpFlags_NoError);
      const ImGuiTestItemInfo settings_wide = ctx->ItemInfo(kSettingsBtn, ImGuiTestOpFlags_NoError);
      IM_CHECK_NE(analyze_cell_wide.ID, 0u);
      IM_CHECK_NE(settings_wide.ID, 0u);
      IM_CHECK_EQ(settings_wide.RectFull.Min.x - analyze_cell_wide.RectFull.Max.x,
                  gui::UiPx(gui::TopBarGroupGapPt(static_cast<float>(gui::kInitWindowWidth))));
      IM_CHECK_EQ(gui::UiPx(gui::TopBarGroupGapPt(static_cast<float>(gui::kInitWindowWidth))),
                  gui::UiPx(gui::kTopBarGroupGapMaxPt));
    };
  }

  // ---- The Analyze workspace (the switch's other half, analyze_workspace.cpp) ----

  // Switching workspaces swaps the frame's layout and touches nothing else: the Analyze branch
  // replaces the three Render-mode panels with the workspace's windows and switches back; the
  // document neither dirties nor changes its simulation state (the switch is session view state),
  // and the frame with no preview publishes no preview viewport (level-triggered, so DoAnalyze's
  // IN_FRAME canvas falls back to the document resolution and the deferred blit skips).
  //
  // The segmented shape is pinned here too, because the no-shift claim rests on it: both cells
  // exist in both modes, one quantized width for both, flush together — so the pair's rectangle
  // is the same rectangle whatever the mode, and the first group to its right holds its position
  // byte for byte across the toggle. The Analyze→Render half also pins the armed-pick clear: the
  // armed cone pick aims at the panorama, which the Render mode takes off screen, and no click
  // may stay armed for a mode that does not read it (the ROI radios' rule; the clear lives in the
  // Render cell's click branch — deleting it turns this case red).
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "shell_chrome", "workspace_switch_swaps_layout_and_stays_clean");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(3);
      IM_CHECK_EQ(gui::g_state.workspace.mode, gui::GuiState::WorkspaceMode::kRender);
      ImGuiWindow* left = ctx->GetWindowByRef("##LeftPanel");
      IM_CHECK(left != nullptr && left->Active);

      const ImGuiTestItemInfo render_cell = ctx->ItemInfo(kWorkspaceToRenderBtn, ImGuiTestOpFlags_NoError);
      const ImGuiTestItemInfo analyze_cell = ctx->ItemInfo(kWorkspaceToAnalyzeBtn, ImGuiTestOpFlags_NoError);
      const ImGuiTestItemInfo settings_before = ctx->ItemInfo(kSettingsBtn, ImGuiTestOpFlags_NoError);
      IM_CHECK_NE(render_cell.ID, 0u);
      IM_CHECK_NE(analyze_cell.ID, 0u);
      IM_CHECK_NE(settings_before.ID, 0u);
      // Flush (SameLine(0, 0): the cells touch) and one width (ToolbarGroupButtonWidth): the
      // pair reads — and is laid out — as one control of constant size.
      IM_CHECK_EQ(render_cell.RectFull.Max.x, analyze_cell.RectFull.Min.x);
      IM_CHECK_EQ(render_cell.RectFull.GetWidth(), analyze_cell.RectFull.GetWidth());

      const bool dirty_before = gui::g_state.dirty;
      const auto sim_state_before = gui::g_state.sim_state;
      ctx->ItemClick(kWorkspaceToAnalyzeBtn);
      ctx->Yield(3);

      IM_CHECK_EQ(gui::g_state.workspace.mode, gui::GuiState::WorkspaceMode::kAnalyze);
      IM_CHECK(left != nullptr && !left->Active);  // not submitted this frame
      // The middle area is four top-level chrome windows since the migration; the share list is
      // the one this switch's contract reads through.
      ImGuiWindow* middle = ctx->GetWindowByRef("##AnalyzeShareList");
      IM_CHECK(middle != nullptr && middle->Active);
      IM_CHECK_EQ(gui::g_preview_vp.active, false);
      IM_CHECK_EQ(gui::g_state.dirty, dirty_before);
      IM_CHECK_EQ(static_cast<int>(gui::g_state.sim_state), static_cast<int>(sim_state_before));

      // The switch is pure view state — nothing on the bar moved: the Settings group, the first
      // thing to the pair's right, holds its rectangle exactly across the toggle.
      const ImGuiTestItemInfo settings_after = ctx->ItemInfo(kSettingsBtn, ImGuiTestOpFlags_NoError);
      IM_CHECK_NE(settings_after.ID, 0u);
      IM_CHECK_EQ(settings_after.RectFull.Min.x, settings_before.RectFull.Min.x);
      IM_CHECK_EQ(settings_after.RectFull.Max.x, settings_before.RectFull.Max.x);

      gui::g_state.analysis.pick_armed = true;
      gui::g_state.analysis.cone_marker_dragging = true;
      ctx->ItemClick(kWorkspaceToRenderBtn);
      ctx->Yield(3);
      IM_CHECK_EQ(gui::g_state.workspace.mode, gui::GuiState::WorkspaceMode::kRender);
      IM_CHECK(left != nullptr && left->Active);
      IM_CHECK(middle != nullptr && !middle->Active);
      IM_CHECK_EQ(gui::g_state.analysis.pick_armed, false);
      IM_CHECK_EQ(gui::g_state.analysis.cone_marker_dragging, false);
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

  // The two new splitters (D1: main view | right column; D2: list | diagnostics) each write only
  // their own session ratio — the independence the contract names — and each pins at its pixel
  // floors: dragged past a floor the ratio stops there and the derived width sits on it, a
  // window below the floors' sum clamps by the contract's proportional rung, and bringing the
  // window back restores the split the stored ratio describes (the ratio, not a pixel width, is
  // what the session keeps). The harness's pointer cannot leave the OS window, so the
  // past-the-floor drags end at the window's edge — which is past both floors at every basis
  // this window reaches.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "shell_chrome", "workspace_splitters_drag_independently_and_pin_at_their_floors");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      const ScopedWindowBasis window_basis(ctx);
      ctx->ItemClick(kWorkspaceToAnalyzeBtn);
      ctx->Yield(3);
      const float d1_0 = gui::g_state.workspace.right_column_ratio;
      const float d2_0 = gui::g_state.workspace.right_list_ratio;
      const float rows_0 = gui::g_state.workspace.middle_rows_ratio;

      const auto drag = [&](const char* ref, float dx, float dy) {
        const ImGuiTestItemInfo grab = ctx->ItemInfo(ref, ImGuiTestOpFlags_NoError);
        IM_CHECK(grab.ID != 0);
        const ImVec2 c = grab.RectFull.GetCenter();
        ctx->MouseMoveToPos(c);
        ctx->Yield(2);
        ctx->MouseDown(0);
        ctx->Yield(2);
        ctx->MouseMoveToPos(ImVec2(c.x + dx, c.y + dy));
        ctx->Yield(3);
        ctx->MouseUp(0);
        ctx->Yield(2);
      };

      // D1 left: the right column widens; the other two splits are bit-identical.
      drag("**/##AnalyzeColumnSplitter", -200.0f, 0.0f);
      const float d1_1 = gui::g_state.workspace.right_column_ratio;
      IM_CHECK_GT(d1_1, d1_0);
      IM_CHECK_EQ(gui::g_state.workspace.right_list_ratio, d2_0);
      IM_CHECK_EQ(gui::g_state.workspace.middle_rows_ratio, rows_0);

      // The ratio, not a pixel width: at the minimum window the column follows the stored ratio
      // (up from its floor? down — narrower window, same share), and back at the basis it
      // returns to the width the ratio describes.
      auto set_basis = [&](int w, int h) {
        g_window_size_test.apply_aspect = false;
        g_window_size_test.width = w;
        g_window_size_test.height = h;
        g_window_size_test.done.store(false);
        g_window_size_test.requested.store(true);
        for (int i = 0; i < 20 && !g_window_size_test.done.load(); ++i) {
          ctx->Yield(1);
        }
        IM_CHECK(g_window_size_test.done.load());
        ctx->Yield(3);
      };
      set_basis(gui::kMinWindowWidth, gui::kMinWindowHeight);
      ImGuiWindow* share = ctx->GetWindowByRef("##AnalyzeShareList");
      IM_CHECK(share != nullptr);
      const float narrow_w = share->Size.x;
      const float narrow_avail = static_cast<float>(gui::kMinWindowWidth) - gui::UiPx(gui::kAnalyzeLeftPanelWidth);
      ctx->LogInfo("[splitters] d1 %.4f -> narrow w %.1f (floor %.0f, basis w would be %.1f)",
                   static_cast<double>(d1_1), static_cast<double>(narrow_w),
                   static_cast<double>(gui::UiPx(gui::kAnalyzeRightColMinWidth)),
                   static_cast<double>(d1_1 * narrow_avail));
      IM_CHECK_FLOAT_NEAR(narrow_w, d1_1 * narrow_avail, 1.0f);
      set_basis(gui::kInitWindowWidth, gui::kInitWindowHeight);
      share = ctx->GetWindowByRef("##AnalyzeShareList");
      IM_CHECK(share != nullptr);
      IM_CHECK_FLOAT_NEAR(share->Size.x,
                          d1_1 * (static_cast<float>(gui::kInitWindowWidth) - gui::UiPx(gui::kAnalyzeLeftPanelWidth)),
                          1.0f);
      IM_CHECK_EQ(gui::g_state.workspace.right_column_ratio, d1_1);  // no drag, no write

      // D1 right, past the window edge: the ratio pins at the content floor and the column sits
      // on it — at the basis and again at the minimum window.
      drag("**/##AnalyzeColumnSplitter", 2000.0f, 0.0f);
      const float d1_pinned = gui::g_state.workspace.right_column_ratio;
      const float floor_w = gui::UiPx(gui::kAnalyzeRightColMinWidth);
      ctx->LogInfo("[splitters] d1 pinned %.4f (floor ratio %.4f)", static_cast<double>(d1_pinned),
                   static_cast<double>(
                       floor_w / (static_cast<float>(gui::kInitWindowWidth) - gui::UiPx(gui::kAnalyzeLeftPanelWidth))));
      share = ctx->GetWindowByRef("##AnalyzeShareList");
      IM_CHECK(share != nullptr);
      IM_CHECK_EQ(share->Size.x, floor_w);
      set_basis(gui::kMinWindowWidth, gui::kMinWindowHeight);
      share = ctx->GetWindowByRef("##AnalyzeShareList");
      IM_CHECK(share != nullptr);
      IM_CHECK_EQ(share->Size.x, floor_w);
      // From the clamped layout the hand moves the boundary immediately: the delta accumulates
      // from the DISPLAYED boundary (the stored ratio sits below the floor here), so a 30px drag
      // widens the column by ~30 instead of crawling through the clamp band while the strip
      // stands still.
      drag("**/##AnalyzeColumnSplitter", -30.0f, 0.0f);
      IM_CHECK_FLOAT_NEAR(share->Size.x, floor_w + 30.0f, 3.0f);
      gui::g_state.workspace.right_column_ratio = d1_pinned;  // the drag wrote it; pin again
      ctx->Yield(2);

      // The dead zone's other half, multi-frame small steps, D1's: from this clamped layout a
      // press arms the drag with an 8px move (ImGui's own drag-lock threshold, ~6px, is the
      // arm cost every splitter pays — not ours), then three 2px movements inside the same
      // press move the boundary by six more — every frame's delta lands, none is absorbed (the
      // single big displacement above proved no dead zone; this half proves the per-frame
      // accumulation).
      {
        ImGuiWindow* share = ctx->GetWindowByRef("##AnalyzeShareList");
        IM_CHECK(share != nullptr);
        const float before_w = share->Size.x;
        const ImGuiTestItemInfo grab = ctx->ItemInfo("**/##AnalyzeColumnSplitter", ImGuiTestOpFlags_NoError);
        IM_CHECK(grab.ID != 0);
        const ImVec2 c = grab.RectFull.GetCenter();
        ctx->MouseMoveToPos(c);
        ctx->Yield(2);
        ctx->MouseDown(0);
        ctx->Yield(2);
        ctx->MouseMoveToPos(ImVec2(c.x - 8.0f, c.y));  // arm: past the drag-lock threshold
        ctx->Yield(2);
        for (int i = 1; i <= 3; ++i) {
          ctx->MouseMoveToPos(ImVec2(c.x - 8.0f - 2.0f * static_cast<float>(i), c.y));
          ctx->Yield(2);
        }
        ctx->MouseUp(0);
        ctx->Yield(2);
        ctx->LogInfo("[splitters] D1 arm+three 2px steps: %.1f -> %.1f (expected %.1f)", static_cast<double>(before_w),
                     static_cast<double>(share->Size.x), static_cast<double>(before_w + 14.0f));
        IM_CHECK_FLOAT_NEAR(share->Size.x, before_w + 14.0f, 3.0f);
        gui::g_state.workspace.right_column_ratio = d1_pinned;  // restore the pinned state
        ctx->Yield(2);
      }
      set_basis(gui::kInitWindowWidth, gui::kInitWindowHeight);

      // D2 down: the diagnostics region shrinks; D1 stays exactly where the pin left it, the
      // row split untouched. Up past its floor: the list's own floor pins instead.
      drag("**/##AnalyzeRightRowsSplitter", 0.0f, 120.0f);
      IM_CHECK_GT(gui::g_state.workspace.right_list_ratio, d2_0);
      IM_CHECK_EQ(gui::g_state.workspace.right_column_ratio, d1_pinned);
      IM_CHECK_EQ(gui::g_state.workspace.middle_rows_ratio, rows_0);
      drag("**/##AnalyzeRightRowsSplitter", 0.0f, -2000.0f);
      ctx->LogInfo("[splitters] d2 pinned %.4f", static_cast<double>(gui::g_state.workspace.right_list_ratio));
      ImGuiWindow* details = ctx->GetWindowByRef("##AnalyzeDetails");
      IM_CHECK(details != nullptr);
      IM_CHECK_GE(details->Size.y,
                  912.0f - gui::UiPx(gui::kAnalyzeSplitterSize) - gui::UiPx(gui::kAnalyzeShareListMinHeight) - 1.0f);

      // D2's half of the same proof, from a layout clamped at its OTHER pin: the stored ratio
      // asks for more than the diagnostics floor leaves, the display sits on the clamp, and an
      // armed press's three 2px upward steps move the list by six — no dead zone, no per-frame
      // absorption.
      gui::g_state.workspace.right_list_ratio = 0.95f;
      ctx->Yield(3);
      {
        ImGuiWindow* share = ctx->GetWindowByRef("##AnalyzeShareList");
        IM_CHECK(share != nullptr);
        const float before_h = share->Size.y;
        const ImGuiTestItemInfo grab = ctx->ItemInfo("**/##AnalyzeRightRowsSplitter", ImGuiTestOpFlags_NoError);
        IM_CHECK(grab.ID != 0);
        const ImVec2 c = grab.RectFull.GetCenter();
        ctx->MouseMoveToPos(c);
        ctx->Yield(2);
        ctx->MouseDown(0);
        ctx->Yield(2);
        ctx->MouseMoveToPos(ImVec2(c.x, c.y - 8.0f));  // arm: past the drag-lock threshold
        ctx->Yield(2);
        for (int i = 1; i <= 3; ++i) {
          ctx->MouseMoveToPos(ImVec2(c.x, c.y - 8.0f - 2.0f * static_cast<float>(i)));
          ctx->Yield(2);
        }
        ctx->MouseUp(0);
        ctx->Yield(2);
        ctx->LogInfo("[splitters] D2 arm+three 2px steps: %.1f -> %.1f (expected %.1f)", static_cast<double>(before_h),
                     static_cast<double>(share->Size.y), static_cast<double>(before_h - 14.0f));
        IM_CHECK_FLOAT_NEAR(share->Size.y, before_h - 14.0f, 3.0f);
      }
      gui::g_state.workspace.right_list_ratio = d2_0;
      ctx->Yield(2);

      // The write is delta-only, so the boundary moves EXACTLY by the hand's travel: a slow
      // multi-frame drag of ten 1px steps moves the list's height by ten, a drag on the other
      // axis moves nothing (the per-axis gate), and a held-still press drifts nothing (the
      // zero-delta gate) — displacement assertions, not direction-and-clamp ones.
      {
        ImGuiWindow* share = ctx->GetWindowByRef("##AnalyzeShareList");
        IM_CHECK(share != nullptr);
        const float list_h_start = share->Size.y;
        const ImGuiTestItemInfo grab = ctx->ItemInfo("**/##AnalyzeRightRowsSplitter", ImGuiTestOpFlags_NoError);
        IM_CHECK(grab.ID != 0);
        const ImVec2 c = grab.RectFull.GetCenter();
        ctx->MouseMoveToPos(c);
        ctx->Yield(2);
        ctx->MouseDown(0);
        ctx->Yield(2);
        for (int i = 1; i <= 10; ++i) {  // purely horizontal: the list height must not move
          ctx->MouseMoveToPos(ImVec2(c.x + static_cast<float>(i), c.y));
          ctx->Yield(1);
        }
        IM_CHECK_EQ(share->Size.y, list_h_start);
        for (int i = 1; i <= 10; ++i) {  // slow vertical: the height tracks the travel 1:1
          ctx->MouseMoveToPos(ImVec2(c.x + 10.0f, c.y + static_cast<float>(i)));
          ctx->Yield(1);
        }
        ctx->LogInfo("[splitters] slow drag: list %.1f -> %.1f (expected %.1f)", static_cast<double>(list_h_start),
                     static_cast<double>(share->Size.y), static_cast<double>(list_h_start + 10.0f));
        IM_CHECK_FLOAT_NEAR(share->Size.y, list_h_start + 10.0f, 2.0f);
        const float after_move = share->Size.y;
        ctx->Yield(5);  // held still: no drift (equal to the value the movement ended on)
        IM_CHECK_EQ(share->Size.y, after_move);
        ctx->MouseUp(0);
        ctx->Yield(2);
      }

      // The band's height rung: at the row ratio's far clamp the band is below the fixed-stack
      // floor, and the side-by-side form — whose children do not scroll — must give way to the
      // stacked one, whose own scroll is the reachability there.
      gui::g_state.workspace.middle_rows_ratio = 0.85f;
      ctx->Yield(3);
      ImGuiWindow* band = ctx->GetWindowByRef("//##AnalyzeLowerRow");
      IM_CHECK(band != nullptr);
      ctx->LogInfo("[splitters] band at 0.85 rows: h %.0f, ScrollMax %.0f", static_cast<double>(band->Size.y),
                   static_cast<double>(band->ScrollMax.y));
      IM_CHECK_GT(band->ScrollMax.y, 0.0f);
      gui::g_state.workspace.middle_rows_ratio = rows_0;
      ctx->Yield(2);

      // Just above the rung the side-by-side form holds, and the map cell's worst honest stack —
      // title, view-button row, the no-selection state text, the input row — fits: the sky-point
      // row's Set button stays inside the cell's clip at a legal split position. The ratio is the
      // panorama's (upper) share, so the rung's edge sits near 0.70: band = panel*(1-r) - strip.
      gui::g_state.workspace.middle_rows_ratio = 0.70f;  // band ≈ 912*0.30 - 8 = 266, the rung's edge
      ctx->Yield(3);
      band = ctx->GetWindowByRef("//##AnalyzeLowerRow");
      IM_CHECK(band != nullptr);
      IM_CHECK_FLOAT_NEAR(band->Size.y, 912.0f * 0.30f - gui::UiPx(gui::kAnalyzeSplitterSize), 2.0f);
      IM_CHECK_EQ(band->ScrollMax.y, 0.0f);  // side-by-side: the band itself does not scroll
      {
        ImGuiWindow* map = nullptr;
        for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
          if (w != nullptr && w->Active && w->ParentWindow == band &&
              std::strstr(w->Name, "##AnalyzeRaypathView") != nullptr) {
            map = w;
          }
        }
        IM_CHECK(map != nullptr);
        const ImGuiTestItemInfo set_btn = ctx->ItemInfo("**/Set sky point###sky_set", ImGuiTestOpFlags_NoError);
        IM_CHECK(set_btn.ID != 0);
        // The X5 containment form (this diff's own contract): the button's FULL rectangle
        // inside the cell's effective clip, not a clipped-height proxy.
        IM_CHECK_GE(set_btn.RectFull.Min.x, map->InnerClipRect.Min.x - 0.5f);
        IM_CHECK_LE(set_btn.RectFull.Max.x, map->InnerClipRect.Max.x + 0.5f);
        IM_CHECK_GE(set_btn.RectFull.Min.y, map->InnerClipRect.Min.y - 0.5f);
        IM_CHECK_LE(set_btn.RectFull.Max.y, map->InnerClipRect.Max.y + 0.5f);
        ctx->LogInfo("[splitters] band %.0f, map clip bottom %.0f, set btn bottom %.0f",
                     static_cast<double>(band->Size.y), static_cast<double>(map->InnerClipRect.Max.y),
                     static_cast<double>(set_btn.RectFull.Max.y));
      }
      gui::g_state.workspace.middle_rows_ratio = rows_0;
      ctx->Yield(2);
    };
  }

  // The hysteresis band's budget: the form's latch and the cells'
  // allocation must spend the same width. Entered side-by-side wide, the window then shrinks to
  // the middle of the hysteresis band (main_w 900: the planned inner width 884 sits between the
  // cells' floor 876 and the entry threshold 900) — the latch holds side-by-side, and the three
  // cells' floors must still fit the inner width the allocation actually spends. Judged on the
  // outer width, the two paddings leaked in: the last cell's frame sat ~16 px past the host's
  // clip.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "shell_chrome", "workspace_band_cells_stay_inside_the_host_in_the_hysteresis_band");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      const ScopedWindowBasis window_basis(ctx);
      ctx->ItemClick(kWorkspaceToAnalyzeBtn);
      ctx->Yield(3);
      const auto set_basis = [&](int w, int h) {
        g_window_size_test.apply_aspect = false;
        g_window_size_test.width = w;
        g_window_size_test.height = h;
        g_window_size_test.done.store(false);
        g_window_size_test.requested.store(true);
        for (int i = 0; i < 20 && !g_window_size_test.done.load(); ++i) {
          ctx->Yield(1);
        }
        IM_CHECK(g_window_size_test.done.load());
        ctx->Yield(3);
      };
      const auto band_cell = [&](const char* fragment) {
        for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
          if (w != nullptr && w->Active && w->ParentWindow == ctx->GetWindowByRef("//##AnalyzeLowerRow") &&
              std::strstr(w->Name, fragment) != nullptr) {
            return w;
          }
        }
        return static_cast<ImGuiWindow*>(nullptr);
      };
      const float d1_saved = gui::g_state.workspace.right_column_ratio;
      const float rows_saved = gui::g_state.workspace.middle_rows_ratio;
      // At the default basis: main_w = 1300 - 8 - 292 = 1000, planned inner 984 >= 900 — the
      // side-by-side form enters (or holds, if an earlier case in the pool left it on; the
      // assertions below are form-state facts either way).
      gui::g_state.workspace.right_column_ratio = 292.0f / 1300.0f;
      gui::g_state.workspace.middle_rows_ratio = 0.35f;  // band ~585, well past the 250 rung
      ctx->Yield(3);
      {
        ImGuiWindow* band = ctx->GetWindowByRef("//##AnalyzeLowerRow");
        IM_CHECK(band != nullptr);
        IM_CHECK_EQ(band->ScrollMax.y, 0.0f);  // side-by-side: the band does not scroll
        ImGuiWindow* map_w = band_cell("##AnalyzeRaypathView");
        ImGuiWindow* globe_w = band_cell("##AnalyzeSunGlobe");
        IM_CHECK(map_w != nullptr && globe_w != nullptr);
        IM_CHECK_FLOAT_NEAR(map_w->Size.x, gui::UiPx(gui::kAnalyzeDepthMapMinSize), 1.5f);
        IM_CHECK_FLOAT_NEAR(globe_w->Size.x, gui::UiPx(gui::kAnalyzeDepthGlobeMinSize), 1.5f);
        // Into the hysteresis band: main_w 900, planned inner 884 — above the cells' floor
        // (876), below the entry threshold (900): the latch keeps side-by-side, and the floors
        // must fit the inner width exactly (3D absorbs the remainder: 884 - 370 - 250 - 16).
        set_basis(1471, gui::kInitWindowHeight);
        band = ctx->GetWindowByRef("//##AnalyzeLowerRow");
        IM_CHECK(band != nullptr);
        ctx->LogInfo("[band hysteresis] band %.0fx%.0f, ScrollMax %.0f", static_cast<double>(band->Size.x),
                     static_cast<double>(band->Size.y), static_cast<double>(band->ScrollMax.y));
        IM_CHECK_EQ(band->ScrollMax.y, 0.0f);  // the latch held: still side-by-side
        map_w = band_cell("##AnalyzeRaypathView");
        ImGuiWindow* three_d_w = band_cell("##AnalyzeRaypath3D");
        globe_w = band_cell("##AnalyzeSunGlobe");
        IM_CHECK(map_w != nullptr && three_d_w != nullptr && globe_w != nullptr);
        IM_CHECK_FLOAT_NEAR(map_w->Size.x, gui::UiPx(gui::kAnalyzeDepthMapMinSize), 1.5f);
        IM_CHECK_FLOAT_NEAR(globe_w->Size.x, gui::UiPx(gui::kAnalyzeDepthGlobeMinSize), 1.5f);
        IM_CHECK_FLOAT_NEAR(three_d_w->Size.x,
                            band->Size.x - 2.0f * ImGui::GetStyle().WindowPadding.x -
                                gui::UiPx(gui::kAnalyzeDepthMapMinSize + gui::kAnalyzeDepthGlobeMinSize) -
                                ImGui::GetStyle().ItemSpacing.x * 2.0f,
                            2.0f);
        IM_CHECK_GE(three_d_w->Size.x, gui::UiPx(gui::kAnalyzeDepth3DMinSize) - 1.5f);
        // The catch itself: every cell fully inside the band's effective clip — the last cell's
        // frame (the globe's) was the one that sat past the host before the budgets unified.
        // Non-fatal per row: one escaping cell must not skip the other rows' evidence.
        for (ImGuiWindow* w : { map_w, three_d_w, globe_w }) {
          const ImRect cell_rect(w->Pos, ImVec2(w->Pos.x + w->Size.x, w->Pos.y + w->Size.y));
          if (cell_rect.Min.x >= band->InnerClipRect.Min.x - 0.5f &&
              cell_rect.Max.x <= band->InnerClipRect.Max.x + 0.5f) {
            continue;
          }
          IM_ERRORF("cell '%s' escapes the band's clip: [%.0f..%.0f] vs clip [%.0f..%.0f]", w->Name,
                    static_cast<double>(cell_rect.Min.x), static_cast<double>(cell_rect.Max.x),
                    static_cast<double>(band->InnerClipRect.Min.x), static_cast<double>(band->InnerClipRect.Max.x));
        }
        ctx->LogInfo("[band hysteresis] cells [%.0f..%.0f][%.0f..%.0f][%.0f..%.0f] vs clip [..%.0f]",
                     static_cast<double>(map_w->Pos.x), static_cast<double>(map_w->Pos.x + map_w->Size.x),
                     static_cast<double>(three_d_w->Pos.x), static_cast<double>(three_d_w->Pos.x + three_d_w->Size.x),
                     static_cast<double>(globe_w->Pos.x), static_cast<double>(globe_w->Pos.x + globe_w->Size.x),
                     static_cast<double>(band->InnerClipRect.Max.x));
      }
      set_basis(gui::kInitWindowWidth, gui::kInitWindowHeight);
      gui::g_state.workspace.right_column_ratio = d1_saved;
      gui::g_state.workspace.middle_rows_ratio = rows_saved;
      ctx->Yield(2);
    };
  }

  // The diagnostics region's idle states (the interaction contract §9.1): before any result the
  // region guides — with no picture of the document yet it points at the Render workspace's Run,
  // with one it points at the region radios and the list's Analyze — and while an analysis runs
  // it names the run and keeps the cancel entry reachable. The branches are named windows (the
  // pick banner's testability shape), and every state is reached the way the product reaches it
  // — the session's own flags, never a text write. The fourth state (a first result turning the
  // region into the identity block, ratios untouched) is pinned by the panel suite's whole-flow
  // case.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "shell_chrome", "workspace_diagnostics_idle_states_follow_the_analysis_state");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->ItemClick(kWorkspaceToAnalyzeBtn);
      ctx->Yield(3);
      const auto window_present = [](const char* fragment) {
        for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
          if (w != nullptr && w->Active && std::strstr(w->Name, fragment) != nullptr) {
            return true;
          }
        }
        return false;
      };

      // A fresh document: no picture, no result — the no-picture guidance, the summary; the
      // query scroll region exists in both branches (the region's one scrolling viewport), the
      // identity branch does not.
      IM_CHECK(window_present("##AnalyzeIdleNoPictureGuide"));
      IM_CHECK(!window_present("##AnalyzeIdlePictureGuide"));
      IM_CHECK(window_present("##AnalyzeIdleSummary"));
      IM_CHECK(window_present("##AnalyzeDetailsScroll"));

      // A picture seen (the same flag the Analyze button reads): the guidance switches.
      gui::g_state.run_intent = gui::RunIntent::kLoaded;
      ctx->Yield(3);
      IM_CHECK(window_present("##AnalyzeIdlePictureGuide"));
      IM_CHECK(!window_present("##AnalyzeIdleNoPictureGuide"));

      // An analysis in flight: the region names the run and hosts the cancel entry. The state is
      // reached the way the product reaches it — a RUNNING backend plus the analysis's started
      // flag. The derive (DeriveAnalysisInProgress) reads the poller's snapshot, so a completed
      // snapshot left by an earlier case would honestly say idle: a real run is the only
      // construction that survives the pool. (The run is stopped and the server torn down below,
      // the panorama case's dance.)
      gui::g_server = LUMICE_CreateServer();
      LUMICE_SetLogLevel(gui::g_server, static_cast<LUMICE_LogLevel>(g_core_log_level));
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
      gui::g_state.sim.ray_num_millions = 0.1f;
      gui::DoRun(/*user_initiated=*/true);
      gui::g_state.analysis.started = true;
      const auto run_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
      while (gui::g_state.sim_state != gui::GuiState::SimState::kSimulating &&
             std::chrono::steady_clock::now() < run_deadline) {
        ctx->Yield(1);
      }
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)gui::GuiState::SimState::kSimulating);
      // DeserializeFromJson replaced the whole GuiState — the workspace went back to Render
      // (the five-rule ledger's deserialize row). Re-enter Analyze: the diagnostics region is
      // this case's subject, and the picture the run is producing is on the main view.
      ctx->ItemClick(kWorkspaceToAnalyzeBtn);
      ctx->Yield(3);
      // Scoped to the diagnostics window: with the run in flight the list panel draws its own
      // Stop beside the budget controls, and a bare "**/" would match that one first.
      ctx->SetRef("//##AnalyzeDetails");
      const ImGuiTestItemInfo stop = ctx->ItemInfo("**/" ICON_FA_STOP " Stop", ImGuiTestOpFlags_NoError);
      IM_CHECK_NE(stop.ID, 0u);
      IM_CHECK_GT(stop.RectFull.GetHeight(), 0.0f);

      // The cancel entry at the D2 floor: the diagnostics region dragged to its smallest
      // permitted height (the list takes the rest), where the running block sits below the fold
      // of the region's one scrolling viewport. The wheel reaches it: scroll the viewport down
      // and the Stop resolves a rect clipped INSIDE the window's visible area — reachable, not
      // merely submitted — and the click lands on it (DoStop is the same entry the list drives).
      {
        const float details_floor_ratio =
            (912.0f - gui::UiPx(gui::kAnalyzeSplitterSize) - gui::UiPx(gui::kAnalyzeDetailsMinHeight)) / 912.0f;
        gui::g_state.workspace.right_list_ratio = details_floor_ratio;
        ctx->Yield(3);
        ImGuiWindow* details_win = nullptr;
        for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
          if (w != nullptr && w->Active && w->ParentWindow == nullptr &&
              std::strcmp(w->Name, "##AnalyzeDetails") == 0) {
            details_win = w;
          }
        }
        IM_CHECK(details_win != nullptr);
        ImGuiWindow* scroll = nullptr;
        for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
          if (w != nullptr && w->Active && w->ParentWindow == details_win &&
              std::strstr(w->Name, "##AnalyzeDetailsScroll") != nullptr) {
            scroll = w;
          }
        }
        IM_CHECK(scroll != nullptr);
        ctx->LogInfo("[idle floor] scroll viewport %.0f, ScrollMax %.0f", scroll->InnerRect.GetHeight(),
                     scroll->ScrollMax.y);
        // Step the viewport down toward the Stop (below the guidance block, above the summary):
        // a stepwise scroll, like the list-row finder — the click below is the reachability
        // evidence, so the view must actually carry the button.
        bool stop_in_view = false;
        for (int step = 0; step < 10 && !stop_in_view; ++step) {
          scroll->Scroll.y = std::min(scroll->ScrollMax.y, scroll->Scroll.y + scroll->InnerRect.GetHeight() * 0.5f);
          ctx->Yield(2);
          const ImGuiTestItemInfo probe = ctx->ItemInfo("**/" ICON_FA_STOP " Stop", ImGuiTestOpFlags_NoError);
          stop_in_view = probe.ID != 0 && probe.RectFull.Min.y >= details_win->InnerClipRect.Min.y - 0.5f &&
                         probe.RectFull.Max.y <= details_win->InnerClipRect.Max.y + 0.5f &&
                         probe.RectFull.Min.x >= details_win->InnerClipRect.Min.x - 0.5f &&
                         probe.RectFull.Max.x <= details_win->InnerClipRect.Max.x + 0.5f;
        }
        IM_CHECK(stop_in_view);
        const ImGuiTestItemInfo stop_floor = ctx->ItemInfo("**/" ICON_FA_STOP " Stop", ImGuiTestOpFlags_NoError);
        IM_CHECK_NE(stop_floor.ID, 0u);
        IM_CHECK_GE(stop_floor.RectFull.Min.y, details_win->InnerClipRect.Min.y - 0.5f);
        IM_CHECK_LE(stop_floor.RectFull.Max.y, details_win->InnerClipRect.Max.y + 0.5f);
        ctx->ItemClick("**/" ICON_FA_STOP " Stop");
        ctx->Yield(2);
        gui::g_state.workspace.right_list_ratio = 0.45f;
      }

      // Teardown: stop the run, wait it out (the panorama case's dance — the shared server must
      // not outlive the case that made it, and a stopping intent must not leak to later cases).
      gui::DoStop();
      const auto stop_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
      while (gui::g_state.sim_state != gui::GuiState::SimState::kDone &&
             gui::g_state.sim_state != gui::GuiState::SimState::kIdle &&
             std::chrono::steady_clock::now() < stop_deadline) {
        ctx->Yield(1);
      }
      LUMICE_StopServer(gui::g_server);
      LUMICE_DestroyServer(gui::g_server);
      gui::g_server = nullptr;
      gui::g_state.analysis.started = false;
      gui::g_state.run_intent = gui::RunIntent::kNone;
      ctx->Yield(2);
    };
  }

  // At a non-default UI scale the skeleton's controls keep their reachability — the contract's
  // render-level calibration duty, exercised at 150%: the fixed left column, the splitters, the
  // list header and the diagnostics region all submit with real rects. The scale rebuilds the
  // product at the frame boundary (the settings panel's own path), so the yields below ride the
  // rebuild, and the dial is restored through the same path so no later case inherits the style.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "shell_chrome", "workspace_skeleton_reachable_at_150_percent_scale");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      gui::g_ui_scale_multiplier = 1.5f;
      gui::g_ui_scale_dirty = true;
      ctx->Yield(5);
      ctx->ItemClick(kWorkspaceToAnalyzeBtn);
      ctx->Yield(4);

      const char* kReachable[] = {
        kWorkspaceToRenderBtn,        "**/Layer 1, entry 1: Crystal##crys_0_0",
        "**/##Altitude_slider",       "**/##AnalyzeRowsSplitter",
        "**/##AnalyzeColumnSplitter", "**/##AnalyzeRightRowsSplitter",
        "**/###analysis_search",
      };
      for (const char* ref : kReachable) {
        const ImGuiTestItemInfo info = ctx->ItemInfo(ref, ImGuiTestOpFlags_NoError);
        if (info.ID == 0u) {
          IM_ERRORF("not reachable at 150%% scale: %s", ref);
          if (ctx->IsError()) {
            break;
          }
          continue;
        }
        if (!(info.RectFull.GetWidth() > 1.0f && info.RectFull.GetHeight() > 1.0f)) {
          IM_ERRORF("zero-area at 150%% scale: %s", ref);
          if (ctx->IsError()) {
            break;
          }
        }
      }

      // The evidence form the rewritten contract demands: containment of the tier's key
      // controls in the regions they live in, then the real operations — the search input
      // typed, D1 dragged (its write is delta-only, so the drag's own landing is the evidence).
      {
        ImGuiWindow* share = ctx->GetWindowByRef("##AnalyzeShareList");
        IM_CHECK(share != nullptr);
        AssertRectContained(ctx, "**/###analysis_search", share);
        AssertRectContained(ctx, "**/" ICON_FA_PLAY " Analyze", share);
        ImGuiWindow* panorama = ctx->GetWindowByRef("//##AnalyzePanorama");
        IM_CHECK(panorama != nullptr);
        AssertRectContained(ctx, "**/Whole sky", panorama);
        AssertRectContained(ctx, "**/In frame", panorama);
        AssertRectContained(ctx, "**/Point", panorama);
        ctx->SetRef("//##AnalyzeShareList");
        ctx->ItemInputValue("**/###analysis_search", "3");
        ctx->ItemInputValue("**/###analysis_search", "");
        ctx->SetRef("");
        const float d1_before = gui::g_state.workspace.right_column_ratio;
        const ImGuiTestItemInfo grab = ctx->ItemInfo("**/##AnalyzeColumnSplitter", ImGuiTestOpFlags_NoError);
        IM_CHECK(grab.ID != 0);
        const ImVec2 c = grab.RectFull.GetCenter();
        ctx->MouseMoveToPos(c);
        ctx->Yield(2);
        ctx->MouseDown(0);
        ctx->Yield(2);
        ctx->MouseMoveToPos(ImVec2(c.x - 40.0f, c.y));
        ctx->Yield(3);
        ctx->MouseUp(0);
        ctx->Yield(2);
        ctx->LogInfo("[150 percent] D1 drag %.4f -> %.4f", static_cast<double>(d1_before),
                     static_cast<double>(gui::g_state.workspace.right_column_ratio));
        IM_CHECK_GT(gui::g_state.workspace.right_column_ratio, d1_before);
        gui::g_state.workspace.right_column_ratio = d1_before;  // the drag wrote it; restore
        ctx->Yield(2);
      }

      gui::g_ui_scale_multiplier = gui::kFactoryUiScaleMultiplier;
      gui::g_ui_scale_dirty = true;
      ctx->Yield(5);
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
        "**/##AnalyzeRowsSplitter",                // the panorama/band divider
        "**/##AnalyzeColumnSplitter",              // D1: main view | right column
        "**/##AnalyzeRightRowsSplitter",           // D2: list | diagnostics
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
      for (const char* region : kRegions) {
        // At least one live window per fragment, not a total count: the details window hosts a
        // named scroll child whose name carries the same fragment, so raw matches overcount.
        for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
          if (w == nullptr || !w->Active) {
            continue;
          }
          if (std::strstr(w->Name, region) != nullptr) {
            regions_found++;
            break;
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

  // The contract's ③ split — the window too narrow for both columns' floors — at 1x, reached by
  // a plain 900x640 window (default assumption 2: the compressed tier does not need a scale leg
  // to reach it). The columns share the span proportionally, the band takes its stacked form
  // with its own scroll as the reachability, and the tier's key operations are driven for real
  // in the geometry that actually results.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "shell_chrome", "workspace_operates_at_the_900x640_proportional_split");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      const ScopedWindowBasis window_basis(ctx);
      ApplyWindowBasis(ctx, 900, 640);
      ctx->ItemClick(kWorkspaceToAnalyzeBtn);
      ctx->Yield(3);

      // The proportional rung engaged: both floors cannot fit the span right of the left column.
      const float avail_w = ImGui::GetIO().DisplaySize.x - gui::UiPx(gui::kAnalyzeLeftPanelWidth);
      IM_CHECK_LT(avail_w, gui::UiPx(gui::kAnalyzeRightColMinWidth) + gui::UiPx(gui::kAnalyzeMainViewMinWidth) +
                               gui::UiPx(gui::kAnalyzeSplitterSize));
      ImGuiWindow* share = ctx->GetWindowByRef("##AnalyzeShareList");
      IM_CHECK(share != nullptr);
      const float expected_right =
          (avail_w - gui::UiPx(gui::kAnalyzeSplitterSize)) *
          (gui::UiPx(gui::kAnalyzeRightColMinWidth) /
           (gui::UiPx(gui::kAnalyzeRightColMinWidth) + gui::UiPx(gui::kAnalyzeMainViewMinWidth)));
      ctx->LogInfo("[900x640] share %.1f (proportional %.1f)", static_cast<double>(share->Size.x),
                   static_cast<double>(expected_right));
      IM_CHECK_FLOAT_NEAR(share->Size.x, expected_right, 1.0f);

      // The band: stacked — each region the full band width at its floor height, the band
      // scrolling (the reachability at this height).
      ImGuiWindow* band = ctx->GetWindowByRef("//##AnalyzeLowerRow");
      IM_CHECK(band != nullptr);
      ImGuiWindow* map = AnalyzeRegionWindow("##AnalyzeRaypathView");
      ImGuiWindow* r3d = AnalyzeRegionWindow("##AnalyzeRaypath3D");
      ImGuiWindow* globe = AnalyzeRegionWindow("##AnalyzeSunGlobe");
      IM_CHECK(map != nullptr);
      IM_CHECK(r3d != nullptr);
      IM_CHECK(globe != nullptr);
      ctx->LogInfo("[900x640] band %.0f scrollmax %.0f; map %.0fx%.0f 3d %.0fx%.0f globe %.0fx%.0f",
                   static_cast<double>(band->Size.y), static_cast<double>(band->ScrollMax.y),
                   static_cast<double>(map->Size.x), static_cast<double>(map->Size.y), static_cast<double>(r3d->Size.x),
                   static_cast<double>(r3d->Size.y), static_cast<double>(globe->Size.x),
                   static_cast<double>(globe->Size.y));
      IM_CHECK_GT(band->ScrollMax.y, 0.0f);
      // The stacked form's defining fact: each region the FULL band content width — equal to its
      // siblings, and the band's inner width minus the scrollbar the stacked form's own scroll
      // shows. (The exact width is ImGui's content computation; the shape is the contract.)
      IM_CHECK_FLOAT_NEAR(map->Size.x, r3d->Size.x, 1.0f);
      IM_CHECK_FLOAT_NEAR(map->Size.x, globe->Size.x, 1.0f);
      IM_CHECK_LE(map->Size.x, band->Size.x - 2.0f * ImGui::GetStyle().WindowPadding.x + 1.0f);
      IM_CHECK_FLOAT_NEAR(map->Size.y, gui::UiPx(gui::kAnalyzeDepthMapMinSize), 2.0f);
      IM_CHECK_FLOAT_NEAR(r3d->Size.y, gui::UiPx(gui::kAnalyzeDepth3DMinSize), 2.0f);
      IM_CHECK_FLOAT_NEAR(globe->Size.y, gui::UiPx(gui::kAnalyzeDepthGlobeMinSize), 2.0f);

      // The map cell (the stacked band's head, at scroll 0): type a sky point, press Set.
      band->Scroll.y = 0.0f;
      ctx->Yield(2);
      ctx->SetRef("//##AnalyzeLowerRow");
      ctx->ItemInputValue("**/alt°##sky", 30.0f);
      ctx->ItemInputValue("**/az°##sky", 45.0f);
      ctx->ItemClick("**/Set sky point###sky_set");
      ctx->SetRef("");
      ctx->Yield(2);
      IM_CHECK(gui::g_state.analysis.sky_point.valid);
      IM_CHECK_FLOAT_NEAR(gui::g_state.analysis.sky_point.dir[2], -std::sin(30.0f * lumice::kDeg2Rad), 1e-3f);

      // The globe (the stacked band's tail): scroll it in, flip its toggle for real.
      band->Scroll.y = band->ScrollMax.y;
      ctx->Yield(2);
      AssertRectContained(ctx, "**/Deviation grid###globe_dev_grid", globe);
      const bool grid_before = gui::g_state.analysis.point_view.show_deviation_grid;
      ctx->ItemClick("**/Deviation grid###globe_dev_grid");
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.analysis.point_view.show_deviation_grid, !grid_before);

      // The right column at this window: the search box takes real input, the (disabled,
      // serverless) Analyze button sits contained, and the diagnostics floor keeps a visible
      // first line of its scrolling region with the wheel reaching the rest.
      const float d2_saved = gui::g_state.workspace.right_list_ratio;
      const float panel_h =
          ImGui::GetIO().DisplaySize.y - gui::UiPx(gui::kTopBarHeight) - gui::UiPx(gui::kStatusBarHeight);
      gui::g_state.workspace.right_list_ratio =
          (panel_h - gui::UiPx(gui::kAnalyzeSplitterSize) - gui::UiPx(gui::kAnalyzeDetailsMinHeight)) / panel_h;
      ctx->Yield(3);
      ctx->SetRef("//##AnalyzeShareList");
      ctx->ItemInputValue("**/###analysis_search", "3-5");
      ctx->ItemInputValue("**/###analysis_search", "");
      AssertRectContained(ctx, "**/" ICON_FA_PLAY " Analyze", share);
      ctx->SetRef("");
      {
        ImGuiWindow* scroll = AnalyzeRegionWindow("##AnalyzeDetailsScroll");
        IM_CHECK(scroll != nullptr);
        ctx->LogInfo("[900x640] details scroll child %.0f, scrollmax %.0f", static_cast<double>(scroll->Size.y),
                     static_cast<double>(scroll->ScrollMax.y));
        IM_CHECK_GT(scroll->ScrollMax.y, 0.0f);
      }
      gui::g_state.workspace.right_list_ratio = d2_saved;
      ctx->Yield(2);
      // Inline basis restore (the 1280x800 leg's rule): the next case's entry audit must have
      // nothing to undo.
      ApplyWindowBasis(ctx, gui::kInitWindowWidth, gui::kInitWindowHeight);
      ctx->Yield(2);
    };
  }

  // The 2x scale leg — the workspace-clamp matrix cell: at twice the UI scale the Analyze
  // workspace's own floors exceed what the shared window's span can give, the ③ proportional
  // rung engages, and the tier's key operations must hold in the geometry that results. The
  // 150% leg keeps the ordinary-split shape; this one is the clamp, and its D1 write is the ③
  // branch (both floors pinned, the ratio held at the proportional split while the drag lands).
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "shell_chrome", "workspace_operates_at_200_percent_scale_clamped");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      gui::g_ui_scale_multiplier = 2.0f;
      gui::g_ui_scale_dirty = true;
      // The rebuild and the window re-derivation land over several frames (pool-context timing
      // is wider than a fixed yield count covers): wait for the scale to settle through the one
      // outlet before asserting anything about the geometry it produces.
      for (int i = 0; i < 60 && !(gui::UiPx(gui::kAnalyzeLeftPanelWidth) > 2.0f * gui::kAnalyzeLeftPanelWidth - 0.5f &&
                                  gui::UiPx(gui::kAnalyzeLeftPanelWidth) < 2.0f * gui::kAnalyzeLeftPanelWidth + 0.5f);
           ++i) {
        ctx->Yield(1);
      }
      IM_CHECK_FLOAT_NEAR(gui::UiPx(gui::kAnalyzeLeftPanelWidth), 2.0f * gui::kAnalyzeLeftPanelWidth, 0.5f);
      // On the harness the scale rebuild itself re-plans the window (the size follows the
      // monitor the harness reports), and the resulting span still fits both floors — the
      // clamp does not engage by the scale alone. The plan's sanctioned substitute (risk 1):
      // an explicit window size at the clamped-equivalent geometry — the same scale, a span
      // too narrow for the doubled floors. Set once the scale has settled, so the re-plan
      // cannot override it.
      ApplyWindowBasis(ctx, 1800, 900);
      ctx->ItemClick(kWorkspaceToAnalyzeBtn);
      ctx->Yield(4);

      // The clamp engaged: at 2x the two columns' floors alone exceed the span.
      const float avail_w = ImGui::GetIO().DisplaySize.x - gui::UiPx(gui::kAnalyzeLeftPanelWidth);
      ctx->LogInfo(
          "[2x clamp] span right of the left column: %.0f (floors %.0f)", static_cast<double>(avail_w),
          static_cast<double>(gui::UiPx(gui::kAnalyzeRightColMinWidth) + gui::UiPx(gui::kAnalyzeMainViewMinWidth)));
      IM_CHECK_LT(avail_w, gui::UiPx(gui::kAnalyzeRightColMinWidth) + gui::UiPx(gui::kAnalyzeMainViewMinWidth) +
                               gui::UiPx(gui::kAnalyzeSplitterSize));

      // The panorama's fixed rows — the ROI radios — wrap under the judgment and each sits
      // contained; the Whole-sky radio's real click is the operability half (the one radio not
      // gated on a picture, so it is clickable in this no-run-yet state).
      ImGuiWindow* panorama = ctx->GetWindowByRef("//##AnalyzePanorama");
      IM_CHECK(panorama != nullptr);
      AssertRectContained(ctx, "**/Whole sky", panorama);
      AssertRectContained(ctx, "**/In frame", panorama);
      AssertRectContained(ctx, "**/Point", panorama);
      gui::g_state.analysis.roi_mode = LUMICE_RAYPATH_ROI_CONE;
      ctx->SetRef("//##AnalyzePanorama");
      ctx->ItemClick("**/Whole sky");
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.analysis.roi_mode, LUMICE_RAYPATH_ROI_FULL_SKY);
      ctx->SetRef("");

      // The map cell at the band scroll positions its two ends need, in order: the header
      // (scroll 0 — the view buttons' containment and Full sky's real click), then the cell's
      // bottom slice (the sky row — the cell is far taller than the 2x band, one scroll shows
      // one slice).
      ImGuiWindow* band = ctx->GetWindowByRef("//##AnalyzeLowerRow");
      IM_CHECK(band != nullptr);
      ImGuiWindow* map = AnalyzeRegionWindow("##AnalyzeRaypathView");
      IM_CHECK(map != nullptr);
      AssertRectContained(ctx, "**/Fit##map", map);
      AssertRectContained(ctx, "**/Full sky##map", map);
      gui::g_state.analysis.map_view.radius_deg = 12.0f;
      ctx->ItemClick("**/Full sky##map");
      ctx->Yield(2);
      IM_CHECK_FLOAT_NEAR(gui::g_state.analysis.map_view.radius_deg, gui::kMapFullSkyRadiusDeg, 0.5f);
      band->Scroll.y = map->Size.y - (band->Size.y - 2.0f * ImGui::GetStyle().WindowPadding.y);
      ctx->Yield(2);
      ctx->SetRef("//##AnalyzeLowerRow");
      ctx->ItemInputValue("**/alt°##sky", 15.0f);
      ctx->ItemClick("**/Set sky point###sky_set");
      ctx->SetRef("");
      ctx->Yield(2);
      IM_CHECK(gui::g_state.analysis.sky_point.valid);

      // The globe at the band scroll position that shows its cell's top (the cell is taller
      // than the 2x-clamped band, so the scroll bottom shows only the cell's lower part); the
      // search box typed; the (disabled) Analyze contained.
      ImGuiWindow* globe = AnalyzeRegionWindow("##AnalyzeSunGlobe");
      IM_CHECK(globe != nullptr);
      ImGuiWindow* r3d_2x = AnalyzeRegionWindow("##AnalyzeRaypath3D");
      IM_CHECK(r3d_2x != nullptr);
      band->Scroll.y = map->Size.y + r3d_2x->Size.y + 2.0f * ImGui::GetStyle().ItemSpacing.y;
      ctx->Yield(2);
      AssertRectContained(ctx, "**/Deviation grid###globe_dev_grid", globe);
      const bool grid_before = gui::g_state.analysis.point_view.show_deviation_grid;
      ctx->ItemClick("**/Deviation grid###globe_dev_grid");
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.analysis.point_view.show_deviation_grid, !grid_before);
      ImGuiWindow* share = ctx->GetWindowByRef("##AnalyzeShareList");
      IM_CHECK(share != nullptr);
      AssertRectContained(ctx, "**/###analysis_search", share);
      AssertRectContained(ctx, "**/" ICON_FA_PLAY " Analyze", share);
      ctx->SetRef("//##AnalyzeShareList");
      ctx->ItemInputValue("**/###analysis_search", "3");
      ctx->ItemInputValue("**/###analysis_search", "");
      ctx->SetRef("");

      // The ③ write branch: a real drag lands, and the ratio holds at the proportional pin —
      // the only split the clamp allows.
      const float pinned = gui::UiPx(gui::kAnalyzeRightColMinWidth) /
                           (gui::UiPx(gui::kAnalyzeRightColMinWidth) + gui::UiPx(gui::kAnalyzeMainViewMinWidth));
      ctx->LogInfo("[2x clamp] D1 ratio %.4f (pin %.4f)",
                   static_cast<double>(gui::g_state.workspace.right_column_ratio), static_cast<double>(pinned));
      const ImGuiTestItemInfo grab = ctx->ItemInfo("**/##AnalyzeColumnSplitter", ImGuiTestOpFlags_NoError);
      IM_CHECK(grab.ID != 0);
      const ImVec2 c = grab.RectFull.GetCenter();
      ctx->MouseMoveToPos(c);
      ctx->Yield(2);
      ctx->MouseDown(0);
      ctx->Yield(2);
      ctx->MouseMoveToPos(ImVec2(c.x - 40.0f, c.y));
      ctx->Yield(3);
      ctx->MouseUp(0);
      ctx->Yield(2);
      IM_CHECK_FLOAT_NEAR(gui::g_state.workspace.right_column_ratio, pinned, 0.01f);

      gui::g_ui_scale_multiplier = gui::kFactoryUiScaleMultiplier;
      gui::g_ui_scale_dirty = true;
      for (int i = 0; i < 60 && !(gui::UiPx(gui::kAnalyzeLeftPanelWidth) > gui::kAnalyzeLeftPanelWidth - 0.5f &&
                                  gui::UiPx(gui::kAnalyzeLeftPanelWidth) < gui::kAnalyzeLeftPanelWidth + 0.5f);
           ++i) {
        ctx->Yield(1);
      }
      IM_CHECK_FLOAT_NEAR(gui::UiPx(gui::kAnalyzeLeftPanelWidth), gui::kAnalyzeLeftPanelWidth, 0.5f);
      // Inline basis restore (the 1280x800 leg's rule): the scale rebuild re-plans the window,
      // so the explicit size lands AFTER the settle — done here, waited on, so the next case's
      // entry audit has nothing to undo.
      ApplyWindowBasis(ctx, gui::kInitWindowWidth, gui::kInitWindowHeight);
      ctx->Yield(2);
    };
  }

  // The row ratio's break points (the 0.82–0.85 band the adjudication named as the former
  // contract's fracture) and D2's two floors: the stack-END controls take real operations in
  // the geometry each ratio actually produces — never "the cell reached its floor" as a proxy
  // for content reachability.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "shell_chrome", "workspace_row_ratio_extremes_keep_stack_ends_operable");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->ItemClick(kWorkspaceToAnalyzeBtn);
      ctx->Yield(3);
      const float rows_saved = gui::g_state.workspace.middle_rows_ratio;
      const float list_saved = gui::g_state.workspace.right_list_ratio;
      ImGuiWindow* band = nullptr;
      ImGuiWindow* map = nullptr;
      ImGuiWindow* r3d = nullptr;
      ImGuiWindow* globe = nullptr;
      {
        band = ctx->GetWindowByRef("//##AnalyzeLowerRow");
        IM_CHECK(band != nullptr);
        map = AnalyzeRegionWindow("##AnalyzeRaypathView");
        r3d = AnalyzeRegionWindow("##AnalyzeRaypath3D");
        globe = AnalyzeRegionWindow("##AnalyzeSunGlobe");
        IM_CHECK(map != nullptr);
        IM_CHECK(r3d != nullptr);
        IM_CHECK(globe != nullptr);
      }

      // 0.85 — the far clamp: the band is far below its rung, the stacked form's scroll is the
      // reachability. The band scroll positions a region's CELL; the scroll bottom shows the
      // globe's lower portion (the cell is taller than the band), so the toggle — at the cell's
      // top — needs the band scrolled to the globe's own content offset. The globe's remaining
      // stack is its region's own internal scroll (the designed degradation).
      gui::g_state.workspace.middle_rows_ratio = 0.85f;
      ctx->Yield(3);
      band = ctx->GetWindowByRef("//##AnalyzeLowerRow");
      IM_CHECK(band != nullptr);
      ctx->LogInfo("[0.85] band %.0f scrollmax %.0f", static_cast<double>(band->Size.y),
                   static_cast<double>(band->ScrollMax.y));
      IM_CHECK_GT(band->ScrollMax.y, 0.0f);
      band->Scroll.y = map->Size.y + r3d->Size.y + 2.0f * ImGui::GetStyle().ItemSpacing.y;
      ctx->Yield(2);
      globe = AnalyzeRegionWindow("##AnalyzeSunGlobe");
      IM_CHECK(globe != nullptr);
      AssertRectContained(ctx, "**/Deviation grid###globe_dev_grid", globe);
      const bool grid_before = gui::g_state.analysis.point_view.show_deviation_grid;
      ctx->ItemClick("**/Deviation grid###globe_dev_grid");
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.analysis.point_view.show_deviation_grid, !grid_before);
      // The 3D cell keeps its floor height in the stacked form (its canvas yields; the cell
      // does not).
      IM_CHECK_FLOAT_NEAR(r3d->Size.y, gui::UiPx(gui::kAnalyzeDepth3DMinSize), 2.0f);

      // 0.82 — just inside the clamp: the map's sky-point row (the cell's tail) takes a real
      // Set. The cell (370) is taller than the band (~156), so the band scroll shows the cell's
      // bottom slice — the sky row. The tail's measured height sits the row above the cell's
      // bottom, so the scroll lands on an estimate and then walks UP until the button's full
      // rectangle is inside the band's clip (the canvas budget changed where the row sits).
      gui::g_state.workspace.middle_rows_ratio = 0.82f;
      ctx->Yield(3);
      band = ctx->GetWindowByRef("//##AnalyzeLowerRow");
      IM_CHECK(band != nullptr);
      band->Scroll.y = map->Size.y - (band->Size.y - 2.0f * ImGui::GetStyle().WindowPadding.y);
      ctx->Yield(2);
      bool set_in_slice = false;
      for (int step = 0; step < 10 && !set_in_slice; ++step) {
        const ImGuiTestItemInfo probe = ctx->ItemInfo("**/Set sky point###sky_set", ImGuiTestOpFlags_NoError);
        set_in_slice = probe.ID != 0 && probe.RectFull.Min.y >= band->InnerClipRect.Min.y - 0.5f &&
                       probe.RectFull.Max.y <= band->InnerClipRect.Max.y + 0.5f;
        if (!set_in_slice) {
          band->Scroll.y = std::max(0.0f, band->Scroll.y - 12.0f);
          ctx->Yield(2);
        }
      }
      IM_CHECK(set_in_slice);
      AssertRectContained(ctx, "**/Set sky point###sky_set", map);
      ctx->SetRef("//##AnalyzeLowerRow");
      ctx->ItemInputValue("**/alt°##sky", 12.0f);
      ctx->ItemClick("**/Set sky point###sky_set");
      ctx->SetRef("");
      ctx->Yield(2);
      IM_CHECK(gui::g_state.analysis.sky_point.valid);
      gui::g_state.workspace.middle_rows_ratio = rows_saved;
      ctx->Yield(2);

      // D2's two floors. Diagnostics floor: the idle stack scrolls (the Stop click's
      // operability is the idle-states case's carrier — this cell pins the geometry and the
      // wheel reach). List floor: the search box contains and takes input.
      const float panel_h =
          ImGui::GetIO().DisplaySize.y - gui::UiPx(gui::kTopBarHeight) - gui::UiPx(gui::kStatusBarHeight);
      gui::g_state.workspace.right_list_ratio =
          (panel_h - gui::UiPx(gui::kAnalyzeSplitterSize) - gui::UiPx(gui::kAnalyzeDetailsMinHeight)) / panel_h;
      ctx->Yield(3);
      {
        ImGuiWindow* details = ctx->GetWindowByRef("##AnalyzeDetails");
        IM_CHECK(details != nullptr);
        IM_CHECK_FLOAT_NEAR(details->Size.y, gui::UiPx(gui::kAnalyzeDetailsMinHeight), 1.0f);
        ImGuiWindow* scroll = AnalyzeRegionWindow("##AnalyzeDetailsScroll");
        IM_CHECK(scroll != nullptr);
        IM_CHECK_GT(scroll->ScrollMax.y, 0.0f);
      }
      gui::g_state.workspace.right_list_ratio = gui::UiPx(gui::kAnalyzeShareListMinHeight) / panel_h;
      ctx->Yield(3);
      {
        ImGuiWindow* share = ctx->GetWindowByRef("##AnalyzeShareList");
        IM_CHECK(share != nullptr);
        IM_CHECK_FLOAT_NEAR(share->Size.y, gui::UiPx(gui::kAnalyzeShareListMinHeight), 1.0f);
        AssertRectContained(ctx, "**/###analysis_search", share);
        ctx->SetRef("//##AnalyzeShareList");
        ctx->ItemInputValue("**/###analysis_search", "3");
        ctx->ItemInputValue("**/###analysis_search", "");
        ctx->SetRef("");
      }
      gui::g_state.workspace.right_list_ratio = list_saved;
      ctx->Yield(2);
    };
  }

  // The λ reference branch and the view buttons at the side-by-side cell's own width: the band
  // arms side-by-side BY CONSTRUCTION here — the right column pinned at its floor leaves the
  // main view ~1032 px (planned inner 1016, past the unified entry threshold of 900), where the
  // default split's ~548 px main view now honestly derives stacked. The explicit width also
  // frees the leg from inheriting whatever latch state an earlier pool case happened to leave.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "shell_chrome", "workspace_map_lambda_branch_and_view_buttons_at_cell_floor");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->ItemClick(kWorkspaceToAnalyzeBtn);
      ctx->Yield(3);
      const float d1_saved = gui::g_state.workspace.right_column_ratio;
      gui::g_state.workspace.right_column_ratio =
          static_cast<float>(gui::UiPx(gui::kAnalyzeRightColMinWidth)) /
          static_cast<float>(gui::kInitWindowWidth - gui::UiPx(gui::kAnalyzeLeftPanelWidth));
      ctx->Yield(3);
      ImGuiWindow* map = AnalyzeRegionWindow("##AnalyzeRaypathView");
      IM_CHECK(map != nullptr);
      IM_CHECK_FLOAT_NEAR(map->Size.x, gui::UiPx(gui::kAnalyzeDepthMapMinSize), 2.0f);
      gui::g_state.analysis.map_wavelength_nm = 550.0;
      ctx->Yield(2);
      AssertRectContained(ctx, "**/reference##map_lambda", map);
      AssertRectContained(ctx, "**/Fit##map", map);
      AssertRectContained(ctx, "**/Full sky##map", map);
      ctx->ItemClick("**/reference##map_lambda");
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.analysis.map_wavelength_nm, 0.0);
      gui::g_state.analysis.map_view.radius_deg = 12.0f;
      ctx->ItemClick("**/Full sky##map");
      ctx->Yield(2);
      IM_CHECK_FLOAT_NEAR(gui::g_state.analysis.map_view.radius_deg, gui::kMapFullSkyRadiusDeg, 0.5f);
      gui::g_state.workspace.right_column_ratio = d1_saved;
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
      // The view is the renderer's own: the document's lens and pose displayed as-is — the
      // continuity contract (what Render shows, Analyze shows).
      IM_CHECK_EQ(gui::g_panorama_vp.params.view_proj.lens_type, gui::g_state.renderer.lens_type);
      IM_CHECK_EQ(gui::g_state.renderer.lens_type, gui::kLensTypeLinear);
      IM_CHECK_FLOAT_NEAR(gui::g_panorama_vp.params.view_proj.fov, gui::g_state.renderer.fov, 1e-4f);
      IM_CHECK_FLOAT_NEAR(gui::g_panorama_vp.params.view_proj.elevation, gui::g_state.renderer.elevation, 1e-4f);

      // The picture region, by name, over the window list (the region child; the picture's own
      // controls — the display-mode control and the interaction button — live inside it).
      ImGuiWindow* pano = nullptr;
      for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
        if (w != nullptr && w->Active && std::strstr(w->Name, "##AnalyzePanorama") != nullptr) {
          pano = w;
        }
      }
      IM_CHECK(pano != nullptr);

      // The camera drag is a view state: a real drag on the picture orbits the view angles and
      // re-projects the next frame's picture, and touches nothing in the document.
      {
        const float az_before = gui::g_state.renderer.azimuth;
        const float el_before = gui::g_state.renderer.elevation;
        const ImVec2 drag_from(pano->Pos.x + pano->Size.x * 0.30f, pano->Pos.y + pano->Size.y * 0.60f);
        ctx->MouseMoveToPos(drag_from);
        ctx->Yield(2);
        ctx->MouseDown(0);
        ctx->Yield(2);
        ctx->MouseMoveToPos(ImVec2(drag_from.x + 120.0f, drag_from.y - 90.0f));
        ctx->Yield(3);
        ctx->MouseUp(0);
        ctx->Yield(3);
        IM_CHECK_NE(gui::g_state.renderer.azimuth, az_before);
        IM_CHECK_NE(gui::g_state.renderer.elevation, el_before);
      }

      // ---- The cone pick over the picture ----
      // Arm CONE + the pick directly (this case is about the main view's half of the flow), and
      // click inside the picture at half its radius. The view is set to the equal-area fisheye
      // pitched at the zenith first, so the closed-form anchor below is exact — the free camera
      // carrying the same view the fixed panorama used to bake in.
      gui::g_state.renderer.lens_type = gui::kLensTypeFisheyeEqualArea;
      gui::g_state.renderer.fov = 180.0f;
      gui::g_state.renderer.elevation = 90.0f;
      gui::g_state.renderer.visible = gui::kVisibleFull;
      gui::g_state.analysis.roi_mode = LUMICE_RAYPATH_ROI_CONE;
      gui::g_state.analysis.pick_armed = true;
      ctx->Yield(2);
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
