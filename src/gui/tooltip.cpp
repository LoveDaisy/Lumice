#include "gui/tooltip.hpp"

#include <cstdarg>

#include "gui/theme.hpp"
#include "imgui_internal.h"

namespace lumice::gui {

void SetWrappedTooltip(const char* fmt, ...) {
  // Match SetTooltipV, including hiding a previous tooltip submitted on the same frame.
  if (!ImGui::BeginTooltipEx(ImGuiTooltipFlags_OverridePrevious, ImGuiWindowFlags_None)) {
    return;
  }
  constexpr float kTextWidth = 400.0f;
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + UiPx(kTextWidth));
  va_list args;
  va_start(args, fmt);
  ImGui::TextV(fmt, args);
  va_end(args);
  ImGui::PopTextWrapPos();
  ImGui::EndTooltip();
}

}  // namespace lumice::gui
