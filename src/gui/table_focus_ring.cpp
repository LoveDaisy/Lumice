#include "gui/table_focus_ring.hpp"

#include <algorithm>
#include <cstddef>

#include "imgui.h"
// GetActiveID: the widget currently holding the keyboard, the one fact the ring's decision is
// made from. Same reliance as panels.cpp's reload-if-active helpers.
#include "imgui_internal.h"

namespace lumice::gui {

void TableFocusRing::BeginFrame() {
  armed_focus_id_ = 0;
  building_value_ids_.clear();
  building_spread_ids_.clear();

  const ImGuiIO& io = ImGui::GetIO();
  // Ctrl+Tab / Alt+Tab are window-level chords (ImGui's own tabbing request steps aside for them
  // too, NavUpdateCreateTabbingRequest); a bare or Shift-modified Tab is the only press the ring
  // answers. Repeat on, so a held key keeps walking, as a native form does.
  if (io.KeyCtrl || io.KeyAlt || !ImGui::IsKeyPressed(ImGuiKey_Tab, /*repeat=*/true)) {
    return;
  }
  const ImGuiID active = ImGui::GetActiveID();
  if (active == 0) {
    return;
  }

  // The ring, in Tab order, is the two committed columns laid end to end: Value column top to
  // bottom, then Spread column top to bottom. The two lists are kept apart only so that a row's
  // Spread box, registered right after its Value box, sorts behind EVERY Value box.
  const std::size_t value_count = committed_value_ids_.size();
  const std::size_t total = value_count + committed_spread_ids_.size();
  if (total == 0) {
    return;
  }
  const auto at = [&](std::size_t i) -> ImGuiID {
    return i < value_count ? committed_value_ids_[i] : committed_spread_ids_[i - value_count];
  };
  std::size_t index = 0;
  for (; index < total; ++index) {
    if (at(index) == active) {
      break;
    }
  }
  if (index == total) {
    return;  // focus is outside the ring: ImGui's default Tab applies, untouched
  }
  const std::size_t next = io.KeyShift ? (index + total - 1) % total : (index + 1) % total;
  armed_focus_id_ = at(next);
}

bool TableFocusRing::Register(RingColumn column, ImGuiID id) {
  std::vector<ImGuiID>& list = column == RingColumn::kValue ? building_value_ids_ : building_spread_ids_;
  // A box registered twice in one frame means two rows submitted the same widget id, or one row
  // was submitted twice — either way the ring's order no longer means "top to bottom", and this
  // is the frame it went wrong, not the frame a Tab press happens to reveal it.
  IM_ASSERT(std::find(list.begin(), list.end(), id) == list.end() &&
            "TableFocusRing: id registered twice in one frame");
  list.push_back(id);
  if (id != 0 && id == armed_focus_id_) {
    armed_focus_id_ = 0;  // one press, one move
    return true;
  }
  return false;
}

void TableFocusRing::EndFrame() {
  committed_value_ids_.swap(building_value_ids_);
  committed_spread_ids_.swap(building_spread_ids_);
  building_value_ids_.clear();
  building_spread_ids_.clear();
  armed_focus_id_ = 0;
}

}  // namespace lumice::gui
