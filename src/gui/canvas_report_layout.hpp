#ifndef LUMICE_GUI_CANVAS_REPORT_LAYOUT_HPP
#define LUMICE_GUI_CANVAS_REPORT_LAYOUT_HPP

#include <algorithm>

#include "imgui.h"
#include "imgui_internal.h"

namespace lumice::gui {

// Reports take the remaining vertical slice, but never pass zero to BeginChild: zero there
// means "fill", not "absent". Keep this container rule with the allocation it protects.
inline ImVec2 RemainingReportSize() {
  return ImVec2(0.0f, std::max(1.0f, ImGui::GetContentRegionAvail().y));
}

// A floating host can keep its nominal minimum while extending beyond the viewport. Its
// unscrolled content may then fit nominally (ScrollMax == 0) but be impossible to recover on
// screen. Partition the actual visible client rectangle, rather than the invisible suffix.
// Call at the non-scrolling host's body origin, before submitting any of its regions.
inline ImVec2 VisibleContentSize() {
  const ImVec2 available = ImGui::GetContentRegionAvail();
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const ImGuiWindow* window = ImGui::GetCurrentWindowRead();
  return ImVec2(std::max(1.0f, std::min(available.x, window->InnerClipRect.Max.x - origin.x)),
                std::max(1.0f, std::min(available.y, window->InnerClipRect.Max.y - origin.y)));
}

}  // namespace lumice::gui

#endif  // LUMICE_GUI_CANVAS_REPORT_LAYOUT_HPP
