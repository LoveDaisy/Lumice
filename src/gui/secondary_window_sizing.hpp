#ifndef LUMICE_GUI_SECONDARY_WINDOW_SIZING_HPP
#define LUMICE_GUI_SECONDARY_WINDOW_SIZING_HPP

// What every "semi-variable" secondary window shares (doc/gui-visual-language.md §9: one axis
// pinned, the other dragged by the user between a floor and the work area): how tall such a window
// may get, and — for the ones whose height follows their content until dragged — the state machine
// that does the following. Each piece used to have exactly one caller; with two, a copy in each is
// the divergence a single owner exists to prevent.

#include <algorithm>
#include <cmath>

#include "imgui.h"
// imgui_internal.h for the height-follows-content machinery below only: ImGuiWindow::Size read back
// before Begin (did the user drag it?), and the DC cursor extents the content's own height is
// measured from after it is drawn. Both were read off the pinned vendor tag (CMakeLists.txt:
// v1.91.8-docking; imgui.cpp CalcWindowContentSizes / CalcWindowAutoFitSize are the two functions
// the measurement mirrors). This runs in the production frame — an ImGui upgrade must re-check
// those two functions.
#include "imgui_internal.h"

namespace lumice::gui {

// The tallest a secondary window may be: the smaller of the caller's own ceiling (`hard_cap`, e.g.
// Summary's 900 px page budget; FLT_MAX when there is none) and the work area less a margin, so the
// window's bottom edge never sits under the status bar or off the screen. Read every frame, since
// the work area follows the platform window.
inline float ClampedSecondaryWindowMaxHeight(float hard_cap) {
  return std::min(hard_cap, ImGui::GetMainViewport()->WorkSize.y - 2.0f * ImGui::GetStyle().WindowPadding.y * 4.0f);
}

// A semi-variable window whose height follows its content — growing and shrinking with it, as
// AlwaysAutoResize would — until the user drags it, and from then on is theirs for the session.
// AlwaysAutoResize itself cannot be used: ImGui skips the resize borders' hit test when it is set
// (imgui.cpp UpdateWindowManualResize), so a window that carries it can never be dragged. What is
// done instead is the same thing by hand, each frame the window is still following: ask for the
// height the content needed last frame, and read the answer back next frame — a height that does
// not match the request can only be the user's.
//
// One instance per window, owned by the window's own file. Summary and Edit Entry are the two users.
struct HeightFollowsContentState {
  // true while the height still follows the content; false once the user has dragged it.
  bool follows_content = true;
  // The height the last request asked for, after the same floor / ceiling clamp ImGui applies and
  // the truncation SetWindowSize performs, so that an untouched window reads back exactly this.
  // Negative when the last request left the height to ImGui's own auto-fit (see content_height),
  // whose answer is not ours to compare against.
  float requested_height = -1.0f;
  // The height the content needed the last time it was drawn: ImGui's own auto-fit arithmetic
  // (CalcWindowContentSizes + CalcWindowAutoFitSize), which is why it reads the window's DC cursor
  // extents rather than GetCursorPos — the latter carries the trailing ItemSpacing the auto-fit does
  // not, and would make the window one spacing taller than AlwaysAutoResize did. Zero until the
  // window has been drawn once (a fresh process, or a reset); a request made with no measurement
  // asks ImGui to auto-fit instead.
  float content_height = 0.0f;
};

// Call before Begin, with the same [min_h, max_h] the window's SetNextWindowSizeConstraints gets.
// Returns this frame's height request for SetNextWindowSize(..., ImGuiCond_Always): 0 is ImGui's own
// "auto-fit this frame" (no measurement yet); negative means the user owns the height and no request
// should be made at all.
//
// The drag check reads the window's Size BEFORE this frame's request: that Size is whatever the
// previous frame ended with, and a request is applied at Begin() before ImGui handles a drag, so a
// drag frame ends with the drag's size, not the request's.
inline float BeginHeightFollow(HeightFollowsContentState& s, const char* window_name, float min_h, float max_h) {
  if (s.follows_content && s.requested_height >= 0.0f) {
    if (const ImGuiWindow* w = ImGui::FindWindowByName(window_name)) {
      if (std::fabs(w->Size.y - s.requested_height) > 0.5f) {
        s.follows_content = false;
      }
    }
  }
  if (!s.follows_content) {
    return -1.0f;
  }
  if (s.content_height > 0.0f) {
    // Truncated as SetWindowSize truncates, so the read-back above compares like with like.
    s.requested_height = std::floor(std::min(std::max(s.content_height, min_h), max_h));
  } else {
    s.requested_height = -1.0f;
  }
  return std::max(s.requested_height, 0.0f);
}

// Call inside the window, after all its content: the height the window would need to show all of
// it, measured as ImGui's auto-fit would next frame — the ideal cursor extent
// (CalcWindowContentSizes' truncation included), the window padding both sides, and the title bar.
// Taken whether or not the window is currently tall enough — clipping and scrolling change what is
// drawn, never what is laid out.
inline float MeasureCurrentWindowContentHeight() {
  const ImGuiWindow* w = ImGui::GetCurrentWindowRead();
  const float content = IM_TRUNC(std::max(w->DC.CursorMaxPos.y, w->DC.IdealMaxPos.y) - w->DC.CursorStartPos.y);
  return content + 2.0f * w->WindowPadding.y + w->TitleBarHeight + w->MenuBarHeight;
}

// Call inside the window, after all its content and right before End()/EndPopup(): records this
// frame's measurement for the next frame's BeginHeightFollow. A no-op once the user owns the height.
inline void EndHeightFollow(HeightFollowsContentState& s) {
  if (!s.follows_content) {
    return;
  }
  s.content_height = MeasureCurrentWindowContentHeight();
}

// Back to "follow the content, nothing measured yet": the next request is an auto-fit.
inline void ResetHeightFollow(HeightFollowsContentState& s) {
  s = HeightFollowsContentState{};
}

}  // namespace lumice::gui

#endif  // LUMICE_GUI_SECONDARY_WINDOW_SIZING_HPP
