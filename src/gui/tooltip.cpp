#include "gui/tooltip.hpp"

#include <cstdarg>
#include <cstdio>

#include "IconsFontAwesome6.h"  // ICON_FA_CIRCLE_INFO: the glyph lives here, not in the header
#include "gui/theme.hpp"
#include "imgui_internal.h"

namespace lumice::gui {

namespace {

// The one formatting / wrap-width / override body behind both text entry points. The va_list
// form stays TU-local: there is no external forwarding consumer yet, so the formatting authority
// is not part of the public contract (promote it only when one appears).
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
  // Icon glyph + "##" + suffix + NUL. 96 bytes cap the suffix at ~90 chars; a truncated label
  // would silently collide two items' ids, so truncation asserts instead. Debug-period guard
  // only: under NDEBUG the truncation stays silent, as before.
  char label[96];
  const int needed = std::snprintf(label, sizeof(label), ICON_FA_CIRCLE_INFO "##%s", id_suffix);
  IM_ASSERT(needed > 0 && needed < static_cast<int>(sizeof(label)));
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
  // AllowWhenDisabled: the icon has no activation semantics and no click effect, so hover works
  // uniformly whether or not a parent BeginDisabled scope surrounds it — callers never need to
  // reason about scope ownership for the hover to work.
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    WrappedTooltipBody(fmt, args);
  }
}

}  // namespace

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
