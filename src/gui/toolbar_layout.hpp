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
