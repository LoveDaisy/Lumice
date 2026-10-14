#ifndef LUMICE_GUI_ENTRY_DELETE_HPP
#define LUMICE_GUI_ENTRY_DELETE_HPP

// The one structural step behind the entry card's delete button, extracted so a test can walk
// the same path the button does (delete, then binding repair) without re-implementing either
// half. See the entry-op family in panels.hpp (MoveEntryWithinLayer, DuplicateEntryBelow) for
// its siblings; this one has its own header rather than joining them there because panels.hpp
// carries the panel draw declarations, whose include graph most of src/gui and the gui tests
// sit under — a function whose audience is two TUs does not need to revalidate all of them.

#include "gui/gui_state.hpp"

namespace lumice::gui {

// Remove the entry from its layer and repair every positional binding into the layer through
// NotifyEntryDeleted (edit_modals.hpp), so the binding repair is written once. The last
// remaining entry of a layer is not deletable (the layer would be left empty): a no-op
// returning false. The card's view cache (the thumbnail) is the caller's concern.
bool DeleteLayerEntry(GuiState& state, int layer_idx, int entry_idx);

}  // namespace lumice::gui

#endif  // LUMICE_GUI_ENTRY_DELETE_HPP
