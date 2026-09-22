#ifndef LUMICE_GUI_SECONDARY_WINDOW_SIZING_HPP
#define LUMICE_GUI_SECONDARY_WINDOW_SIZING_HPP

// The one piece of arithmetic every "semi-variable" secondary window shares (doc/gui-visual-language.md
// §9: one axis pinned, the other dragged by the user between a floor and the work area): how tall
// such a window may get. Settings and Summary each used to be the only caller of this line; with
// two, a copy in each is the divergence a single owner exists to prevent.

#include <algorithm>

#include "imgui.h"

namespace lumice::gui {

// The tallest a secondary window may be: the smaller of the caller's own ceiling (`hard_cap`, e.g.
// Summary's 900 px page budget; FLT_MAX when there is none) and the work area less a margin, so the
// window's bottom edge never sits under the status bar or off the screen. Read every frame, since
// the work area follows the platform window.
inline float ClampedSecondaryWindowMaxHeight(float hard_cap) {
  return std::min(hard_cap, ImGui::GetMainViewport()->WorkSize.y - 2.0f * ImGui::GetStyle().WindowPadding.y * 4.0f);
}

}  // namespace lumice::gui

#endif  // LUMICE_GUI_SECONDARY_WINDOW_SIZING_HPP
