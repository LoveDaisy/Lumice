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

#include <string>
#include <vector>

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

// ---- The combined orientation combo (Display group) ----
//
// The combo absorbed the Portrait/Landscape flip button: one entry per reachable (preset,
// orientation) pair, so picking an entry writes the full state. Everything the combo shows or
// highlights derives from the three functions below — the draw loop keeps no second table of
// entries, and the unit tests enumerate the same functions, so a preset added later shows up as
// a test failure rather than as a stale list. has_background is deliberately NOT an input: the
// entry list is invariant, and Match Background stays listed (greyed, not hidden) without an
// image so the option's existence stays discoverable — see AspectPresetOptionDisabled.

// One line of the combined combo: a preset plus the orientation that entry applies.
struct AspectComboEntry {
  AspectPreset preset;
  bool portrait;
};

// The orientation an entry stands for, as the stored portrait bit reads for matching purposes:
// presets without two orientations always match their base entry, so a stale aspect_portrait bit
// on them can light neither a Portrait entry nor a Portrait label. Same predicate the preview
// label below consumes — the combo keeps ONE stale-bit judgment, matching the render side's
// ApplyAspectOrientation (which additionally gates on a non-degenerate ratio, a concern the UI
// side does not have).
inline bool AspectOrientationEffective(AspectPreset preset, bool portrait) {
  return !AspectFlipDisabled(preset) && portrait;
}

// Display label of an entry: the preset's name, suffixed " Portrait" for the orientation entries
// of presets that offer one. A flip-disabled preset ignores the portrait bit entirely — the UI
// twin of ApplyAspectOrientation's guard.
inline std::string AspectEntryLabel(AspectPreset preset, bool portrait) {
  std::string label = kAspectPresetNames[static_cast<int>(preset)];
  if (AspectOrientationEffective(preset, portrait)) {
    label += " Portrait";
  }
  return label;
}

// The combo's entries in enum order, each flip-disabled preset contributing just its base entry:
// 11 lines for the current seven presets. Rebuilt per frame — eleven small structs, same cost
// class as the label list it replaced.
inline std::vector<AspectComboEntry> AspectComboEntries() {
  std::vector<AspectComboEntry> entries;
  entries.reserve(static_cast<size_t>(kAspectPresetCount) * 2);
  for (int i = 0; i < kAspectPresetCount; ++i) {
    const auto preset = static_cast<AspectPreset>(i);
    entries.push_back({ preset, false });
    if (!AspectFlipDisabled(preset)) {
      entries.push_back({ preset, true });
    }
  }
  return entries;
}

}  // namespace lumice::gui

#endif  // LUMICE_GUI_ASPECT_RATIO_RULES_HPP
