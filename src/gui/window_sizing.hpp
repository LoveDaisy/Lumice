#ifndef LUMICE_GUI_WINDOW_SIZING_HPP
#define LUMICE_GUI_WINDOW_SIZING_HPP

#include <algorithm>
#include <climits>
#include <cmath>
#include <vector>

#include "gui/gui_constants.hpp"

struct GLFWwindow;

namespace lumice::gui {

// GLFW can report a resize inside SetWindowSize or during a later event poll. Correlate delayed
// events with concrete requests through the next event poll, and decide whether an unmatched event
// was manual only after all content-scale callbacks in that poll have arrived (Win32 reports size
// before DPI). Unconfirmed requests expire at that boundary: rejected requests must not exempt a
// later user resize. An event arriving after settlement is treated as a new external resize.
class WindowResizeEvents {
 public:
  void BeginRequest(int width, int height) {
    requests_.push_back({ width, height, width, height, false });
    issuing_ = true;
    saw_synchronous_callback_ = false;
  }

  void EndRequest(int actual_width, int actual_height) {
    auto& request = requests_.back();
    request.actual_w = actual_width;
    request.actual_h = actual_height;
    request.completed =
        saw_synchronous_callback_ || (actual_width == request.target_w && actual_height == request.target_h);
    issuing_ = false;
  }

  bool RecordResize(int width, int height) {
    if (issuing_) {
      saw_synchronous_callback_ = true;
      return true;
    }
    for (auto& request : requests_) {
      if ((width == request.target_w && height == request.target_h) ||
          (request.completed && width == request.actual_w && height == request.actual_h)) {
        request.completed = true;
        return true;
      }
    }
    unmatched_resize_ = true;
    return false;
  }

  void RecordContentScaleChange() {
    content_scale_changed_ = true;
    ++content_scale_revision_;
  }

  unsigned int ContentScaleRevision() const { return content_scale_revision_; }

  bool FinishEventPoll() {
    const bool manual_resize = unmatched_resize_ && !content_scale_changed_;
    // Success, rejection and silence all settle here. The old readback of a rejected request is
    // never evidence of a programmatic event, and no historical target survives into another poll.
    requests_.clear();
    unmatched_resize_ = false;
    content_scale_changed_ = false;
    return manual_resize;
  }

 private:
  struct Request {
    int target_w;
    int target_h;
    int actual_w;
    int actual_h;
    bool completed;
  };
  std::vector<Request> requests_;
  bool issuing_ = false;
  bool saw_synchronous_callback_ = false;
  bool unmatched_resize_ = false;
  bool content_scale_changed_ = false;
  unsigned int content_scale_revision_ = 0;
};

// A monitor work area is expressed in GLFW screen coordinates. Window sizes below are content
// sizes in the same coordinate system; framebuffer/device pixels never enter this module.
struct MonitorRect {
  int x = 0;
  int y = 0;
  int w = INT_MAX;
  int h = INT_MAX;
};

// The non-client frame around GLFW's content area, reported by glfwGetWindowFrameSize. The frame
// is owned by the OS and therefore does not scale with the user's UI multiplier.
struct WindowFrameInsets {
  int left = 0;
  int top = 0;
  int right = 0;
  int bottom = 0;
};

// One snapshot of every size constraint used by startup, scale rebuild and aspect fitting.
// min/max are GLFW content sizes; workarea/frame retain the inputs needed to clamp position.
struct WindowGeometryConstraints {
  MonitorRect workarea{};
  WindowFrameInsets frame{};
  bool workarea_known = false;
  int min_w = 0;
  int min_h = 0;
  int max_w = INT_MAX;
  int max_h = INT_MAX;
};

inline int ContentExtentInsideWorkarea(int work_extent, int leading_frame, int trailing_frame) {
  if (work_extent == INT_MAX) {
    return INT_MAX;
  }
  const long long content_extent =
      static_cast<long long>(work_extent) - std::max(0, leading_frame) - std::max(0, trailing_frame);
  return static_cast<int>(std::max(1LL, content_extent));
}

inline WindowFrameInsets EstimatedWindowFrameInsets() {
  // Before a window exists GLFW cannot report its real frame. Preserve the historical conservative
  // total while routing startup through the same constraint builder as the live window.
  return { 0, 0, kWindowDecorationMargin, kWindowDecorationMargin };
}

inline WindowGeometryConstraints MakeWindowGeometryConstraints(float layout_scale, MonitorRect workarea,
                                                               WindowFrameInsets frame) {
  WindowGeometryConstraints out;
  out.workarea = workarea;
  out.frame = frame;
  out.workarea_known = workarea.w != INT_MAX && workarea.h != INT_MAX;
  out.max_w = ContentExtentInsideWorkarea(workarea.w, frame.left, frame.right);
  out.max_h = ContentExtentInsideWorkarea(workarea.h, frame.top, frame.bottom);
  const int scaled_min_w = static_cast<int>(std::lround(static_cast<float>(kMinWindowWidth) * layout_scale));
  const int scaled_min_h = static_cast<int>(std::lround(static_cast<float>(kMinWindowHeight) * layout_scale));
  out.min_w = std::min(scaled_min_w, out.max_w);
  out.min_h = std::min(scaled_min_h, out.max_h);
  return out;
}

// What the main window's size limits and (if it must grow) its size become when the UI scale is
// `layout_scale`. Consumed at startup (before the window exists, with the work area of the primary
// monitor and cur_w/h = the scaled creation size) and at every scale change (with the live window
// size and the work area of the monitor it sits on) — one rule for both, see PlanWindowSizeForScale.
struct WindowSizePlan {
  int min_w = 0;  // for glfwSetWindowSizeLimits
  int min_h = 0;
  bool resize = false;  // whether to call glfwSetWindowSize at all
  int target_w = 0;     // the size to set when resize is true
  int target_h = 0;
};

inline WindowSizePlan PlanWindowSizeForScale(int cur_w, int cur_h, const WindowGeometryConstraints& constraints) {
  WindowSizePlan plan;
  plan.min_w = constraints.min_w;
  plan.min_h = constraints.min_h;
  plan.target_w = std::clamp(cur_w, constraints.min_w, constraints.max_w);
  plan.target_h = std::clamp(cur_h, constraints.min_h, constraints.max_h);
  plan.resize = plan.target_w != cur_w || plan.target_h != cur_h;
  return plan;
}

// Pure function: how the window's minimum size and current size respond to a layout scale.
//
// The minimum is kMinWindowWidth/Height × layout_scale — the panels the minimum was chosen to fit
// (kLeftPanelWidth/kRightPanelWidth and the rest) grow with the scale through UiPx(), so a window
// held at the 1x minimum would clip them. But the minimum handed to GLFW is clamped to the work
// area FIRST: glfwSetWindowSizeLimits is a hard floor GLFW enforces against every later resize,
// so a floor above the work area would pin the window larger than the screen with no way to drag
// it smaller. Clamping the floor before applying it is what keeps this rule and the work-area
// maximum from contradicting each other. When the scaled minimum does not fit, the window is
// simply the work area and the layout scrolls — the honest result of a multiplier
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
// resize is true when the current size falls outside the shared min/max interval. A window the user
// has already dragged larger is left alone unless it no longer fits the current monitor. work_w/h
// = INT_MAX means "work area unknown" (headless, monitor lookup failed) and degrades to the
// unclamped minimum, matching how edit_modals.cpp treats the same failure.
inline WindowSizePlan PlanWindowSizeForScale(float layout_scale, int cur_w, int cur_h, int work_w, int work_h) {
  return PlanWindowSizeForScale(
      cur_w, cur_h,
      MakeWindowGeometryConstraints(layout_scale, { 0, 0, work_w, work_h }, EstimatedWindowFrameInsets()));
}

// Pure function: return the index of the monitor whose workarea contains the
// point (cx, cy); -1 if none. Edge convention: left/top inclusive, right/bottom
// exclusive, so adjacent monitors do not both claim the shared seam.
// Used by ApplyAspectRatio to route multi-monitor window sizing — see
// scratchpad/scrum-gui-polish-v11/task-fix-multi-monitor-aspect for rationale.
inline int SelectMonitorIndexByCenter(int cx, int cy, const MonitorRect* rects, int count) {
  for (int i = 0; i < count; i++) {
    const MonitorRect& r = rects[i];
    const long long right = static_cast<long long>(r.x) + r.w;
    const long long bottom = static_cast<long long>(r.y) + r.h;
    if (cx >= r.x && static_cast<long long>(cx) < right && cy >= r.y && static_cast<long long>(cy) < bottom) {
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
// using the final integer window dimensions. That makes the small rounding error visible and lets
// the same function measure a GLFW readback rather than only a planned request.
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

inline AspectFitResult MeasureAspectFit(int window_w, int window_h, float ratio, float left_w, float right_w,
                                        float topbar_h, float statusbar_h) {
  AspectFitResult out{};
  out.target_w = window_w;
  out.target_h = window_h;
  out.requested_preview_ratio = ratio;
  const float preview_w = static_cast<float>(window_w) - left_w - right_w;
  const float preview_h = static_cast<float>(window_h) - topbar_h - statusbar_h;
  if (ratio <= 0.0f || preview_w <= 0.0f || preview_h <= 0.0f) {
    out.achieved_preview_ratio = ratio;
    return out;
  }
  out.achieved_preview_ratio = preview_w / preview_h;
  out.was_clamped = std::abs(out.achieved_preview_ratio - ratio) / ratio >= kAspectClampTolerance;
  return out;
}

inline int RoundWindowExtent(double value, int min_value, int max_value) {
  const double bounded = std::clamp(value, static_cast<double>(min_value), static_cast<double>(max_value));
  return static_cast<int>(std::llround(bounded));
}

inline AspectFitResult ResolveAspectFit(int current_win_w, float ratio, const WindowGeometryConstraints& constraints,
                                        float left_w, float right_w, float topbar_h, float statusbar_h) {
  const double chrome_w = static_cast<double>(left_w + right_w);
  const double chrome_h = static_cast<double>(topbar_h + statusbar_h);
  const double min_preview_w = std::max(1.0, static_cast<double>(constraints.min_w) - chrome_w);
  const double min_preview_h = std::max(1.0, static_cast<double>(constraints.min_h) - chrome_h);
  const double max_preview_w = std::max(min_preview_w, static_cast<double>(constraints.max_w) - chrome_w);
  const double max_preview_h = std::max(min_preview_h, static_cast<double>(constraints.max_h) - chrome_h);
  const double preferred_preview_w =
      std::clamp(static_cast<double>(current_win_w) - chrome_w, min_preview_w, max_preview_w);

  // A ratio-preserving window exists when one preview width can satisfy both width and height
  // intervals. Keep the current preview width when possible; otherwise move only as far as the
  // shared floor/workarea constraints require.
  const double ratio_min_width = static_cast<double>(ratio) * min_preview_h;
  const double ratio_max_width = static_cast<double>(ratio) * max_preview_h;
  const double feasible_min_width = std::max(min_preview_w, ratio_min_width);
  const double feasible_max_width = std::min(max_preview_w, ratio_max_width);

  double preview_w = 0.0;
  double preview_h = 0.0;
  if (feasible_min_width <= feasible_max_width) {
    preview_w = std::clamp(preferred_preview_w, feasible_min_width, feasible_max_width);
    preview_h = preview_w / ratio;
  } else if (ratio > max_preview_w / min_preview_h) {
    // The requested ratio is wider than this constraint box can represent.
    preview_w = max_preview_w;
    preview_h = min_preview_h;
  } else {
    // The requested ratio is taller than this constraint box can represent.
    preview_w = min_preview_w;
    preview_h = max_preview_h;
  }

  const int target_w = RoundWindowExtent(preview_w + chrome_w, constraints.min_w, constraints.max_w);
  const int target_h = RoundWindowExtent(preview_h + chrome_h, constraints.min_h, constraints.max_h);
  return MeasureAspectFit(target_w, target_h, ratio, left_w, right_w, topbar_h, statusbar_h);
}

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
// Compatibility overload for 1x callers. Runtime code supplies a WindowGeometryConstraints
// snapshot so the scaled floor and live decorations participate in the same solution.
inline AspectFitResult ResolveAspectFit(int current_win_w, float ratio, int work_w, int work_h, float left_w,
                                        float right_w, float topbar_h, float statusbar_h) {
  return ResolveAspectFit(
      current_win_w, ratio,
      MakeWindowGeometryConstraints(/*layout_scale=*/1.0f, { 0, 0, work_w, work_h }, EstimatedWindowFrameInsets()),
      left_w, right_w, topbar_h, statusbar_h);
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

// Capture the live counterpart of MakeWindowGeometryConstraints: current-monitor workarea,
// actual window decorations and the scaled content floor. Unknown monitor degrades to unbounded
// max extents while retaining the actual frame for later position work.
WindowGeometryConstraints GetCurrentWindowGeometryConstraints(GLFWwindow* win, float layout_scale);

struct WindowPosition {
  int x = 0;
  int y = 0;
};

inline WindowPosition ClampWindowPositionToWorkarea(int x, int y, int window_w, int window_h,
                                                    const WindowGeometryConstraints& constraints) {
  if (!constraints.workarea_known) {
    return { x, y };
  }
  const int min_x = constraints.workarea.x + constraints.frame.left;
  const int min_y = constraints.workarea.y + constraints.frame.top;
  const int max_x = constraints.workarea.x + constraints.workarea.w - window_w - constraints.frame.right;
  const int max_y = constraints.workarea.y + constraints.workarea.h - window_h - constraints.frame.bottom;
  return { std::clamp(x, std::min(min_x, max_x), std::max(min_x, max_x)),
           std::clamp(y, std::min(min_y, max_y), std::max(min_y, max_y)) };
}

}  // namespace lumice::gui

#endif  // LUMICE_GUI_WINDOW_SIZING_HPP
