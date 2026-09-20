#include "gui/config_summary_window.hpp"

#include <string>

#include "IconsFontAwesome6.h"
#include "gui/config_summary.hpp"
#include "gui/gui_state.hpp"
#include "imgui.h"

namespace lumice::gui {

namespace {

// Fixed width (AC: "width fixed, height follows the content"). Wide enough for the longest
// value a document commonly carries — an entry's axis line ("Gauss · Mean 90 · Std 1") next to
// its label at the indent an entry sits at — without wrapping, so a screenshot's lines stay
// lines. Height is whatever the content needs, up to the work area; past that the content
// scrolls.
constexpr float kWindowWidth = 560.0f;
// The label column of every table. One number for both sections so labels line up across a
// group boundary; sized to the longest label the page can carry ("Multi-scatter prob.",
// "Filter row 10").
constexpr float kLabelColumnWidth = 170.0f;
constexpr float kEntryIndent = 18.0f;

void DrawFields(const char* table_id, const ConfigSummaryGroup& group) {
  // SizingFixedFit + one stretch column: the label column is a fixed width and the value takes
  // the rest, so a long value clips at the window edge instead of pushing the table wider than
  // the window (which would defeat the fixed width and put a horizontal scrollbar in the shot).
  if (!ImGui::BeginTable(table_id, 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
    return;
  }
  ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, kLabelColumnWidth);
  ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
  for (const auto& field : group.fields) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextDisabled("%s", field.label.c_str());
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(field.value.c_str());
  }
  ImGui::EndTable();
}

void DrawGroup(const ConfigSummaryGroup& group, int index) {
  if (group.level > 0) {
    ImGui::Indent(kEntryIndent);
  }
  // A level-0 title is a section heading (SeparatorText: a rule with the title in it, the
  // theme's own idiom for a heading); an entry title is a plain line under its layer's heading.
  if (group.level == 0) {
    ImGui::SeparatorText(group.title.c_str());
  } else {
    ImGui::TextUnformatted(group.title.c_str());
  }
  DrawFields((std::string("##summary_group_") + std::to_string(index)).c_str(), group);
  if (group.level > 0) {
    ImGui::Unindent(kEntryIndent);
  }
  ImGui::Spacing();
}

}  // namespace

void RenderConfigSummaryWindow(GuiState& state) {
  if (!state.config_summary_window_open) {
    return;
  }

  // Width pinned every frame, not FirstUseEver: AlwaysAutoResize below would otherwise grow the
  // window to its widest line, and the user is not meant to resize it either way. Height is
  // left to auto-resize between the content's own height and the work area, minus a margin so
  // the window's bottom edge never sits under the status bar or off the screen.
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float max_height = viewport->WorkSize.y - 2.0f * ImGui::GetStyle().WindowPadding.y * 4.0f;
  ImGui::SetNextWindowSizeConstraints(ImVec2(kWindowWidth, 0.0f), ImVec2(kWindowWidth, max_height));
  ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
  if (!ImGui::Begin(ICON_FA_FILE_LINES " Summary###ConfigSummary", &state.config_summary_window_open,
                    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize |
                        ImGuiWindowFlags_NoResize)) {
    ImGui::End();
    return;
  }

  // Rebuilt every frame from the live document: the page has no state of its own to fall out of
  // date, which is also why nothing here needs resetting when the document changes.
  const ConfigSummary summary = BuildConfigSummary(state);

  ImGui::TextDisabled("Lumice");
  ImGui::SameLine();
  ImGui::TextUnformatted(summary.version.c_str());

  int index = 0;
  for (const auto& group : summary.settings) {
    DrawGroup(group, index++);
  }
  for (const auto& group : summary.document) {
    DrawGroup(group, index++);
  }

  ImGui::End();
}

}  // namespace lumice::gui
