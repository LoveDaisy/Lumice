#ifndef LUMICE_GUI_PREVIEW_DISPLAY_MODE_CONTROL_HPP
#define LUMICE_GUI_PREVIEW_DISPLAY_MODE_CONTROL_HPP

// Where the display-mode segmented control sits in the preview's top-right corner, as a function
// of the content region's width and the segments' text sizes.
//
// The draw code needs the control's rectangle BEFORE it submits anything — the hover test that
// picks the control's opacity, and the one that keeps the viewport's own gestures off it, both
// run first — so the geometry cannot be read back from ImGui after the fact. It is a pure
// function of its arguments — no GuiState, no globals, no ImGui — so a test can check it over
// widths and segment counts without a frame. The caller measures the text (ImGui::CalcTextSize)
// and passes pixels in; every returned rectangle is relative to the content region's top-left.

#include <vector>

namespace lumice::gui {

struct ControlRect {
  float x0 = 0.0f;
  float y0 = 0.0f;
  float x1 = 0.0f;
  float y1 = 0.0f;
};

struct SegmentedControlLayout {
  ControlRect frame;                  // the union of the segments
  std::vector<ControlRect> segments;  // left to right, abutting, one per text width passed in
};

// Right-anchored `margin` in from the region's right edge and `margin` down from its top. Each
// segment is its text plus `pad_x` on either side, and all share one height, `text_h` plus `pad_y`
// above and below. A region narrower than the control does not push it past the left edge: it
// anchors at x = 0 and overflows to the right, where the window clips it — the labels stay
// readable from their first letter rather than from their last.
inline SegmentedControlLayout LayoutSegmentedControl(float region_w, float margin, float pad_x, float pad_y,
                                                     float text_h, const std::vector<float>& text_w) {
  SegmentedControlLayout out;
  float total_w = 0.0f;
  for (const float w : text_w) {
    total_w += w + 2.0f * pad_x;
  }
  const float h = text_h + 2.0f * pad_y;
  float x = region_w - margin - total_w;
  if (x < 0.0f) {
    x = 0.0f;
  }
  out.frame = ControlRect{ x, margin, x + total_w, margin + h };
  out.segments.reserve(text_w.size());
  for (const float w : text_w) {
    const float seg_w = w + 2.0f * pad_x;
    out.segments.push_back(ControlRect{ x, margin, x + seg_w, margin + h });
    x += seg_w;
  }
  return out;
}

}  // namespace lumice::gui

#endif  // LUMICE_GUI_PREVIEW_DISPLAY_MODE_CONTROL_HPP
