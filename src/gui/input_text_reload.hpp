#ifndef LUMICE_GUI_INPUT_TEXT_RELOAD_HPP
#define LUMICE_GUI_INPUT_TEXT_RELOAD_HPP

#include "imgui.h"

namespace lumice::gui {

// ---- Reload-if-active: an InputText under a value that changed by other means than typing ----
//
// ImGui's InputText keeps a private copy of the text it is editing for as long as it is the active
// widget, and while it is, that copy has priority over the buffer the caller passes in: on every
// frame the two differ, the widget writes its copy BACK over the buffer (imgui_widgets.cpp,
// InputTextEx: "as soon as the input box is active, the in-widget value gets priority over any
// underlying modification of the input buffer"). InputFloat is an InputText over a per-frame
// formatting of the float, so the same holds for it. A value the edit modal pulls in from the pool
// underneath an active box is therefore undone one frame later — silently, and for good, since the
// pull's baseline has already moved on — unless the widget is told to reload from the buffer first
// (ImGuiInputTextState::ReloadUserBufAndSelectAll, ImGui's own remedy for #2890).
//
// Call these right before submitting the widget, in the same ID scope it is submitted in, on the
// frame the value underneath it changed. A no-op unless `id` is the active widget, so calling them
// on a frame nothing changed costs a comparison. ReloadSliderInputIfActive derives the id of the
// input box SliderWithInput renders from `label` — the "##<label>_input" rule lives in one place
// (FormatSliderInputId in panels.cpp) so the two cannot disagree on which box that is. Both are
// defined in panels.cpp; the `reload_active_inputs` parameters of RenderShapeDistTableRow /
// RenderAxisDist (panels.hpp) are what routes the modal's pull verdict to them.
void ReloadInputTextIfActive(ImGuiID id);
void ReloadSliderInputIfActive(const char* label);

}  // namespace lumice::gui

#endif  // LUMICE_GUI_INPUT_TEXT_RELOAD_HPP
