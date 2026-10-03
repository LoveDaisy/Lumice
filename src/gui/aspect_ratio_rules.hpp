#ifndef LUMICE_GUI_ASPECT_RATIO_RULES_HPP
#define LUMICE_GUI_ASPECT_RATIO_RULES_HPP

// Which aspect-ratio controls apply, as a function of the preset (and, for one of them, whether a
// background image is loaded).
//
// Both rules used to be inline expressions inside the Display group's draw code, where the only
// way to check "does 2:1 offer Portrait?" was to drive the combo in a live frame. They are pure
// functions of their arguments — no GuiState, no globals, no ImGui — so a test can enumerate all
// kAspectPresetCount presets against them instead.
//
// Extracted verbatim: each body is the expression that stood at its call site.

#include "gui/gui_state.hpp"

namespace lumice::gui {

// "Match Background" is the one preset that needs an actual image behind it; every other option
// in the combo is always selectable. Greyed rather than hidden so the option's existence stays
// discoverable before a background is loaded.
inline bool AspectPresetOptionDisabled(AspectPreset option, bool has_background) {
  return option == AspectPreset::kMatchBg && !has_background;
}

// The Portrait/Landscape flip needs a preset with two distinguishable orientations. Free has no
// fixed ratio to flip, 1:1 is its own transpose, and Match Background takes its orientation from
// the image.
inline bool AspectFlipDisabled(AspectPreset preset) {
  return preset == AspectPreset::kFree || preset == AspectPreset::k1x1 || preset == AspectPreset::kMatchBg;
}

// Apply the saved orientation only to presets whose UI offers that choice. Older documents can
// carry aspect_portrait=true while using Match Background; treating that stale bit as active would
// transpose the image-derived ratio during a scale rebuild even though the control is disabled.
inline float ApplyAspectOrientation(AspectPreset preset, bool portrait, float ratio) {
  return portrait && ratio > 0.0f && !AspectFlipDisabled(preset) ? 1.0f / ratio : ratio;
}

}  // namespace lumice::gui

#endif  // LUMICE_GUI_ASPECT_RATIO_RULES_HPP
