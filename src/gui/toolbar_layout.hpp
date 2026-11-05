#ifndef LUMICE_GUI_TOOLBAR_LAYOUT_HPP
#define LUMICE_GUI_TOOLBAR_LAYOUT_HPP

#include <algorithm>

#include "gui/gui_constants.hpp"  // kMinWindowWidth — the narrow clamp of the interpolation

namespace lumice::gui {

// The top bar's group-gap rule (doc/gui/gui-visual-language.md §4.8): the gap at every group
// boundary widens as the window widens, from the minimum at the narrowest window the app allows
// to a maximum it reaches at the anchor width and never grows past. The spare width above the
// anchor goes to the bar's trailing whitespace — the ⚠ + Revert corner and the right toggle stay
// right-anchored — and the buttons themselves never stretch.
//
// Each boundary's gap is laid out UNEVENLY around its vertical rule: the left side — the larger
// breathing room before the rule — carries kTopBarGroupGapLeftShare of the gap's spare width,
// the right side the remainder, so the next group reads as hugging the rule instead of floating
// midway (owner feedback, 2026-11-04: the old even split left a large blank on the rule's left
// and little or nothing on its right). The share is an engineering value, calibrated by eye
// against full-frame captures; changing it moves neither the budget nor any total-gap fact —
// the two sides' sum is pinned to the spare width by construction (the right side is computed
// as the remainder), and the renderer reads the split through the two functions below exactly
// as the tests do.
//
// Pure and scale-free, single source for the renderer and the tests: the input is the window
// width in 1x design points (the caller divides out CurrentUiScale()), the output is the gap in
// the same basis (the caller lays it out through UiPx()). The fit constraint lives at the narrow
// end — the minimum IS the gap the bar renders at kMinWindowWidth, where
// shell_chrome/the_top_bar_fits_at_the_minimum_window_width measures the whole bar — so the
// adaptive rule never competes with the floor.
//
// The three constants are engineering values, calibrated where the plan's width ledger expected
// them (the ledger's per-boundary +8 pt over the base rhythm 2 × ItemSpacing(8) + the 1 px rule):
// min 25 pt at kMinWindowWidth, max min + 8 at kInitWindowWidth — the anchor doubles as the test
// basis, so the wide end is pinned at a width the harness actually renders. Measured headroom at
// the floor lives in the fit test's LogInfo line.
inline constexpr float kTopBarGroupGapMinPt = 25.0f;
inline constexpr float kTopBarGroupGapMaxPt = 33.0f;
inline constexpr float kTopBarGroupGapAnchorWidthPt = 1600.0f;

// The left side's share of a boundary's spare width (the part above the base rhythm). 0.75 was
// the first screenshot-calibrated value: at the floor's spare of 8 px it splits 6/2 — the rule
// reads as opening onto the wider left blank and letting the next group sit close — without
// squeezing the right side under the base rhythm. Recalibrate here and against the captures, not
// by re-deriving layout: the value only moves pixels inside a fixed total.
inline constexpr float kTopBarGroupGapLeftShare = 0.75f;

// The two sides' share of a boundary's spare width, in the same basis as the input (px). A spare
// of zero or less means the target total sits at or below the base rhythm: both sides stay at
// base and both return 0 (the never-narrower guard — a miscalibrated target cannot shrink the
// bar's rhythm below the gap the theme's ItemSpacing.x already provides).
inline float TopBarGroupGapLeftExtraPx(float spare_px) {
  return spare_px > 0.0f ? spare_px * kTopBarGroupGapLeftShare : 0.0f;
}
inline float TopBarGroupGapRightExtraPx(float spare_px) {
  return spare_px > 0.0f ? spare_px - TopBarGroupGapLeftExtraPx(spare_px) : 0.0f;
}

// The group gap for a window of the given design-basis width: linear interpolation between the
// two constants, clamped at both ends (below kMinWindowWidth reads as the minimum, at or above
// the anchor as the maximum — the function rises monotonically in between and is exactly the
// maximum for every wider window).
inline float TopBarGroupGapPt(float window_width_pt) {
  const float t = std::clamp((window_width_pt - static_cast<float>(kMinWindowWidth)) /
                                 (kTopBarGroupGapAnchorWidthPt - static_cast<float>(kMinWindowWidth)),
                             0.0f, 1.0f);
  return kTopBarGroupGapMinPt + t * (kTopBarGroupGapMaxPt - kTopBarGroupGapMinPt);
}

}  // namespace lumice::gui

#endif  // LUMICE_GUI_TOOLBAR_LAYOUT_HPP
