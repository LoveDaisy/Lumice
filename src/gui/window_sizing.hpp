#ifndef LUMICE_GUI_WINDOW_SIZING_HPP
#define LUMICE_GUI_WINDOW_SIZING_HPP

#include <algorithm>
#include <climits>
#include <cmath>
#include <utility>

#include "gui/gui_constants.hpp"

struct GLFWwindow;

namespace lumice::gui {

// Pure function: clamp desired window size to the usable work area.
// workarea already excludes OS bars (menubar/Dock/taskbar) per GLFW docs;
// kWindowDecorationMargin covers the remaining title-bar + border cost.
// kMinWindowWidth/Height serve as a hard floor against pathological workarea.
inline std::pair<int, int> ClampWindowSizeToWorkarea(int desired_w, int desired_h, int work_w, int work_h) {
  int max_w = std::max(kMinWindowWidth, work_w - kWindowDecorationMargin);
  int max_h = std::max(kMinWindowHeight, work_h - kWindowDecorationMargin);
  return { std::min(desired_w, max_w), std::min(desired_h, max_h) };
}

// What the main window's size limits and (if it must grow) its size become when the UI scale is
// `layout_scale`. Consumed at startup (before the window exists, with the work area of the primary
// monitor and cur_w/h = the scaled creation size) and at every scale change (with the live window
// size and the work area of the monitor it sits on) — one rule for both, see PlanWindowSizeForScale.
struct WindowSizePlan {
  int min_w = 0;  // for glfwSetWindowSizeLimits
  int min_h = 0;
  bool grow = false;  // whether to call glfwSetWindowSize at all
  int target_w = 0;   // the size to set when grow is true
  int target_h = 0;
};

// Pure function: how the window's minimum size and current size respond to a layout scale.
//
// The minimum is kMinWindowWidth/Height × layout_scale — the panels the minimum was chosen to fit
// (kLeftPanelWidth/kRightPanelWidth and the rest) grow with the scale through UiPx(), so a window
// held at the 1x minimum would clip them. But the minimum handed to GLFW is clamped to the work
// area FIRST: glfwSetWindowSizeLimits is a hard floor GLFW enforces against every later resize,
// so a floor above the work area would pin the window larger than the screen with no way to drag
// it smaller. Clamping the floor before applying it is what keeps this rule and
// ClampWindowSizeToWorkarea from contradicting each other. When the scaled minimum does not fit,
// the window is simply the work area and the layout scrolls — the honest result of a multiplier
// too large for this screen; the multiplier is not disabled on such a screen, since a preference
// that is selectable on one machine and greyed out on another is not predictable.
//
// kWindowDecorationMargin is deducted at its 1x value on purpose: it stands for the OS's own title
// bar and borders, which a user multiplier does not enlarge (the monitor-scale half of that growth
// is inside the 50 px buffer up to 150%).
//
// Reads kMinWindowWidth/Height raw rather than through UiPx(): it runs before the first
// ApplyVisualLanguage, when the outlet is still at 1.0, so the scale is an explicit parameter.
//
// grow is true only when the current size is below the new floor — a window the user has already
// dragged larger is left alone and only its floor moves. work_w/h = INT_MAX means "work area
// unknown" (headless, monitor lookup failed) and degrades to the unclamped minimum — INT_MAX minus
// the margin is above any scaled minimum — matching how edit_modals.cpp treats the same failure.
inline WindowSizePlan PlanWindowSizeForScale(float layout_scale, int cur_w, int cur_h, int work_w, int work_h) {
  WindowSizePlan plan;
  const int scaled_min_w = static_cast<int>(std::lround(static_cast<float>(kMinWindowWidth) * layout_scale));
  const int scaled_min_h = static_cast<int>(std::lround(static_cast<float>(kMinWindowHeight) * layout_scale));
  plan.min_w = std::min(scaled_min_w, work_w - kWindowDecorationMargin);
  plan.min_h = std::min(scaled_min_h, work_h - kWindowDecorationMargin);
  plan.grow = cur_w < plan.min_w || cur_h < plan.min_h;
  // The same clamp the creation size goes through, so a grown window lands where a created one
  // would (ClampWindowSizeToWorkarea keeps its own 1x hard floor against a pathological work area;
  // with work_w/h = INT_MAX it is the identity).
  const auto [target_w, target_h] =
      ClampWindowSizeToWorkarea(std::max(cur_w, plan.min_w), std::max(cur_h, plan.min_h), work_w, work_h);
  plan.target_w = target_w;
  plan.target_h = target_h;
  return plan;
}

// POD describing a monitor's workarea in virtual screen coordinates.
struct MonitorRect {
  int x;
  int y;
  int w;
  int h;
};

// Pure function: return the index of the monitor whose workarea contains the
// point (cx, cy); -1 if none. Edge convention: left/top inclusive, right/bottom
// exclusive, so adjacent monitors do not both claim the shared seam.
// Used by ApplyAspectRatio to route multi-monitor window sizing — see
// scratchpad/scrum-gui-polish-v11/task-fix-multi-monitor-aspect for rationale.
inline int SelectMonitorIndexByCenter(int cx, int cy, const MonitorRect* rects, int count) {
  for (int i = 0; i < count; i++) {
    const MonitorRect& r = rects[i];
    if (cx >= r.x && cx < r.x + r.w && cy >= r.y && cy < r.y + r.h) {
      return i;
    }
  }
  return -1;
}

// Tolerance for the "preview region matches requested aspect" check used by
// ResolveAspectFit. 5% relative deviation is the first-version threshold —
// large enough to absorb integer rounding from glfwSetWindowSize, small enough
// to flag the genuine "screen too small" case (e.g. 2:1 on 1280×720).
inline constexpr float kAspectClampTolerance = 0.05f;

// Result of fitting a requested aspect ratio onto a window-sized canvas with
// fixed panel/topbar/statusbar overhead. Returned by ResolveAspectFit.
//
// `requested_preview_ratio` is the input ratio (post-portrait-flip). Callers
// must pass the already-flipped ratio rather than re-deriving from a preset.
//
// `achieved_preview_ratio` is the ratio of the preview region after the
// final clamp; in practice this equals
//   (target_w - left_w - right_w) / (target_h - topbar_h - statusbar_h)
// using the floats *before* truncation to int target_w/target_h, so the
// comparison against `requested_preview_ratio` is not noised by the int cast.
//
// `was_clamped` is true when the relative deviation of achieved vs requested
// exceeds kAspectClampTolerance; the GUI uses this to render a warning.
struct AspectFitResult {
  int target_w = 0;
  int target_h = 0;
  float requested_preview_ratio = 0.0f;
  float achieved_preview_ratio = 0.0f;
  bool was_clamped = false;
};

// Pure function: given the current window width, a requested preview aspect
// ratio (post-portrait-flip), the active monitor workarea, and the fixed
// panel/topbar/statusbar overhead, return the size we would set the window
// to + whether the achieved preview region matches the requested ratio.
//
// Replaces the inline sizing logic that lived in app.cpp::ApplyAspectRatio
// (the "double clamp + recalc_w gate" block). The recalc_w gate previously
// failed silently when the projected width exceeded work_w on small screens
// (e.g. 2:1 on 1280×720), giving the user a window that barely changed and
// a preview region whose ratio was nowhere near 2:1 — with no UI feedback.
// This helper exposes the mismatch via was_clamped so the GUI can render
// "Screen too small — preview shows ~X:1 (export still Y:1)".
//
// Caller responsibilities:
//   * Pass `ratio > 0` (kFree / kMatchBg-without-bg paths must early-return
//     before calling this helper).
//   * Apply portrait flip (1/ratio) before passing in.
//   * Pre-resolve `work_w`, `work_h` via SelectMonitorIndexByCenter +
//     glfwGetMonitorWorkarea (kept out of this helper to preserve purity).
inline AspectFitResult ResolveAspectFit(int current_win_w, float ratio, int work_w, int work_h, float left_w,
                                        float right_w, float topbar_h, float statusbar_h) {
  AspectFitResult out{};
  out.requested_preview_ratio = ratio;

  // Mirror the legacy derivation: preview_w from current win_w minus panels;
  // target_h = preview_h + chrome; target_w starts as the unchanged win_w.
  float preview_w = std::max(1.0f, static_cast<float>(current_win_w) - left_w - right_w);
  float preview_h = preview_w / ratio;
  int target_w = current_win_w;
  auto target_h = static_cast<int>(preview_h + topbar_h + statusbar_h);

  target_w = std::clamp(target_w, kMinWindowWidth, work_w);
  target_h = std::clamp(target_h, kMinWindowHeight, work_h);

  // If height was clamped, try to recalculate width to maintain ratio.
  // The legacy gate `recalc_w <= work_w` prevented expanding past workarea;
  // we keep that gate so the caller's glfwSetWindowSize never overflows.
  // When the gate rejects, was_clamped will be set below via the achieved-
  // ratio comparison rather than relying on the gate as a signal.
  float actual_preview_h = static_cast<float>(target_h) - topbar_h - statusbar_h;
  if (actual_preview_h > 0.0f) {
    float actual_preview_w_candidate = actual_preview_h * ratio;
    int recalc_w = static_cast<int>(actual_preview_w_candidate + left_w + right_w);
    if (recalc_w >= kMinWindowWidth && recalc_w <= work_w) {
      target_w = recalc_w;
    }
  }

  // Compute achieved preview ratio from the final target_{w,h} we will hand
  // to glfwSetWindowSize. Use floats for the achieved-ratio numerator/denom
  // so cmp with requested_preview_ratio is not noised by int truncation.
  float achieved_preview_w = static_cast<float>(target_w) - left_w - right_w;
  float achieved_preview_h = static_cast<float>(target_h) - topbar_h - statusbar_h;
  if (achieved_preview_h <= 0.0f || achieved_preview_w <= 0.0f) {
    // Pathological panel widths — pretend we hit the target so we don't
    // surface a misleading warning. Caller should never hit this in practice
    // because kMinWindowWidth/Height already reserves room for chrome.
    out.target_w = target_w;
    out.target_h = target_h;
    out.achieved_preview_ratio = ratio;
    out.was_clamped = false;
    return out;
  }
  float achieved = achieved_preview_w / achieved_preview_h;
  float deviation = std::abs(achieved - ratio) / ratio;

  out.target_w = target_w;
  out.target_h = target_h;
  out.achieved_preview_ratio = achieved;
  out.was_clamped = deviation >= kAspectClampTolerance;
  return out;
}

// Return the workarea of the monitor containing the given window's center.
// Returns false when win is nullptr, GLFW cannot enumerate monitors, or the
// window center falls outside every known monitor — in all failure cases the
// caller should fall back to an unbounded constraint (e.g. FLT_MAX) rather
// than silently defaulting to a primary-monitor workarea, which would
// reintroduce the multi-monitor "primary bias" anti-pattern documented in
// scrum-gui-polish-v11/task-fix-multi-monitor-aspect.
//
// Mirrors the inline monitor-selection pattern embedded in `ApplyAspectRatio`
// (see `app.cpp`). Both sites must evolve together; prefer routing future
// monitor-lookup needs through this helper to collapse the duplicate pattern
// over time. (Function name chosen over a line-number anchor so this comment
// stays valid as `app.cpp` drifts.)
bool GetCurrentMonitorWorkArea(GLFWwindow* win, MonitorRect* out);

}  // namespace lumice::gui

#endif  // LUMICE_GUI_WINDOW_SIZING_HPP
