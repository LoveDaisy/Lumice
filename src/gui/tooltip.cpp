#include "gui/tooltip.hpp"

#include <cstdarg>
#include <cstdio>

#include "gui/theme.hpp"
#include "imgui_internal.h"

namespace lumice::gui {

namespace {

// The one formatting / wrap-width / override body behind both text entry points.
void WrappedTooltipBody(const char* fmt, va_list args) {
  // Match SetTooltipV, including hiding a previous tooltip submitted on the same frame.
  if (!ImGui::BeginTooltipEx(ImGuiTooltipFlags_OverridePrevious, ImGuiWindowFlags_None)) {
    return;
  }
  constexpr float kTextWidth = 400.0f;
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + UiPx(kTextWidth));
  ImGui::TextV(fmt, args);
  ImGui::PopTextWrapPos();
  ImGui::EndTooltip();
}

// The one icon body behind both placement entry points.
void DrawInfoIcon(const char* id_suffix, const char* fmt, va_list args, bool join_previous) {
  char label[96];
  std::snprintf(label, sizeof(label), ICON_FA_CIRCLE_INFO "##%s", id_suffix);
  if (join_previous) {
    ImGui::SameLine();
  }
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
  ImGui::SmallButton(label);
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(4);
  if (ImGui::IsItemHovered()) {
    SetWrappedTooltipV(fmt, args);
  }
}

}  // namespace

void SetWrappedTooltipV(const char* fmt, va_list args) {
  WrappedTooltipBody(fmt, args);
}

void SetWrappedTooltip(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  WrappedTooltipBody(fmt, args);
  va_end(args);
}

void InfoIcon(const char* id_suffix, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  DrawInfoIcon(id_suffix, fmt, args, /*join_previous=*/true);
  va_end(args);
}

void InfoIconLine(const char* id_suffix, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  DrawInfoIcon(id_suffix, fmt, args, /*join_previous=*/false);
  va_end(args);
}

float InfoIconAdvance() {
  return ImGui::CalcTextSize(ICON_FA_CIRCLE_INFO).x + ImGui::GetStyle().FramePadding.x * 2.0f +
         ImGui::GetStyle().ItemSpacing.x;
}

}  // namespace lumice::gui
