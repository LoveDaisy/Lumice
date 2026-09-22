#ifndef LUMICE_GUI_TABLE_FOCUS_RING_HPP
#define LUMICE_GUI_TABLE_FOCUS_RING_HPP

#include <vector>

#include "imgui.h"

namespace lumice::gui {

// ---- Column-major Tab order over a table of input boxes ----
//
// ImGui's own Tab walks widgets in submission order, which for a property table is row by row:
// Value, Sync, Rand, Spread, next row. A user filling in such a table works down ONE column (six
// face distances in a row, then their spreads), so this helper replaces that order, for the input
// boxes of the tables that opt in, with: every Value box top to bottom, then every Spread box top
// to bottom, wrapping at either end; Shift+Tab walks the same ring backwards.
//
// Mechanism, and why it is split across two frames. ImGui resolves a Tab press as a "tabbing
// request" made at the top of the frame and scored against every item as it is submitted, and
// SetKeyboardFocusHere() files the SAME kind of request (its request additionally lands on items
// marked ImGuiItemFlags_NoTabStop — imgui_internal.h, ImGuiNavMoveFlags_FocusApi). So the ring
// decides at the start of the frame, from the membership it recorded LAST frame, which box the
// press should land on, and the row that owns that box calls SetKeyboardFocusHere(0) right before
// submitting it. Deciding from last frame's ring is what keeps this one code path for both
// directions: the target may be above or below the current box, and by the time any row is
// submitted this frame the decision is already made, so there is never a "target already drawn
// earlier this frame" case. The cost is one frame of latency, which is below what a person sees.
//
// Members opt OUT of ImGui's default walk by wrapping their widgets in
// PushItemFlag(ImGuiItemFlags_NoTabStop, true) — the two mechanisms then never race for the same
// press: the default request finds no stop inside the tables, and the ring's request is the only
// one that can land there. The consequence, accepted by design: Tab from a control OUTSIDE the
// tables does not enter them (the first box has to be clicked), which matches how every other box
// in the modal is reached anyway.
//
// Usage (per frame, per ring):
//   ring.BeginFrame();                       // before the first table
//   ... in each row, before submitting the box:
//   if (ring.Register(RingColumn::kValue, id)) ImGui::SetKeyboardFocusHere(0);
//   ring.EndFrame();                         // after the last table
// A row that is not drawn this frame (collapsed section) simply does not register, and the ring
// shrinks by itself; a Spread box registers only while it is enabled, for the same reason.
enum class RingColumn { kValue, kSpread };

class TableFocusRing {
 public:
  // Reads Tab / Shift+Tab and the active widget. If the active widget is a member of the ring
  // recorded last frame, arms the next (or previous) member as this frame's focus target. Call
  // once per frame before the first Register(); on a frame without a Tab press, or with the focus
  // anywhere outside the ring, nothing is armed and ImGui's default behaviour is untouched.
  void BeginFrame();

  // Records `id` as the next member of `column` in the ring being built this frame, and answers
  // whether it is the focus target BeginFrame armed — the caller then calls SetKeyboardFocusHere(0)
  // immediately before submitting the widget with that id. Consumed on the first true answer.
  bool Register(RingColumn column, ImGuiID id);

  // Commits the ring built this frame as the one BeginFrame reads next frame, and drops any target
  // that no row consumed (the box it named was not drawn), so a stale target never fires later.
  void EndFrame();

 private:
  std::vector<ImGuiID> committed_value_ids_;
  std::vector<ImGuiID> committed_spread_ids_;
  std::vector<ImGuiID> building_value_ids_;
  std::vector<ImGuiID> building_spread_ids_;
  ImGuiID armed_focus_id_ = 0;
};

}  // namespace lumice::gui

#endif  // LUMICE_GUI_TABLE_FOCUS_RING_HPP
