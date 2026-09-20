#include "gui/config_summary_window.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "IconsFontAwesome6.h"
#include "gui/config_summary.hpp"
#include "gui/gui_state.hpp"
#include "imgui.h"

namespace lumice::gui {

namespace {

// Fixed width (AC: "width fixed, height follows the content"), and a wide one on purpose: the page
// is two columns side by side — the settings groups on the left, the document's layers and entries
// on the right — so that a default document and a two-layer one both fit on one screen with no
// scrolling. The budget is a 1280 x 900 screen, the smallest the page is asked to fit; the width
// stops short of it so the window has a margin at either side. Height is whatever the content
// needs, up to kMaxWindowHeight and the work area; past that the content scrolls, and the
// functional test asserts that neither reference document ever gets there.
constexpr float kWindowWidth = 1200.0f;
constexpr float kMaxWindowHeight = 900.0f;
// The left column, sized to the longest settings line it carries ("Multi-scatter prob." is in
// the document column; here the widest label is "Ray allocation" and the widest value a lens
// name such as "dual_fisheye_equal_area"). The document column takes the rest.
constexpr float kSettingsColumnWidth = 380.0f;
// The label column of every single-field line. One number for both columns so labels line up
// across a group boundary; sized to the longest label the page can carry ("Multi-scatter prob.",
// "Filter row 10").
constexpr float kLabelColumnWidth = 170.0f;
constexpr float kEntryIndent = 18.0f;
// The gap between the label and value cells of neighbouring packed fields on one line, so
// "Face 3  1.000" and "Face 4  1.000" read as two cells and not as a single run of numbers.
constexpr float kPackedCellGap = 14.0f;

// The row background of the `row_index`-th line of a group, alternating as ImGuiTableFlags_RowBg
// does. Set by hand because every line is its own table (see DrawGroup), and a per-table RowBg
// would restart the alternation on each of them.
void SetLineBackground(int row_index) {
  ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                         ImGui::GetColorU32(row_index % 2 == 0 ? ImGuiCol_TableRowBg : ImGuiCol_TableRowBgAlt));
}

// One display line of a group. A single field is a fixed label column and a stretch value
// column, so a long value clips at the window edge instead of pushing the table wider than the
// window (which would defeat the fixed width and put a horizontal scrollbar in the shot). A packed
// line is a run of label/value cell pairs, each sized to its content: the run's fields are
// same-kind scalars whose values are short, and content sizing is what keeps "Face 3 1.000" and a
// randomized "Face 5 0.900 ± 0.100 uniform" on one line without reserving the widest case for
// every cell.
void DrawLine(const char* table_id, const std::vector<const ConfigSummaryField*>& line, int row_index) {
  // A run that happens to hold one field (a prism's lone Height) is drawn as a single line: the
  // packing only changes how a line with several fields is cut, not how one field looks.
  const bool packed = line.size() > 1;
  const int columns = packed ? static_cast<int>(line.size()) * 2 : 2;
  if (!ImGui::BeginTable(table_id, columns, ImGuiTableFlags_SizingFixedFit)) {
    return;
  }
  if (packed) {
    for (size_t i = 0; i < line.size(); ++i) {
      ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed);
      ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthFixed);
    }
  } else {
    ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, kLabelColumnWidth);
    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
  }
  ImGui::TableNextRow();
  SetLineBackground(row_index);
  for (size_t i = 0; i < line.size(); ++i) {
    ImGui::TableNextColumn();
    ImGui::TextDisabled("%s", line[i]->label.c_str());
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(line[i]->value.c_str());
    if (packed && i + 1 < line.size()) {
      // Padding after the value cell, before the next field's label.
      ImGui::SameLine(0.0f, kPackedCellGap);
      ImGui::Dummy(ImVec2(0.0f, 0.0f));
    }
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
  // One table per display line rather than one per group: a packed line has more columns than a
  // single-field line, and ImGui tables have no column spanning, so a group-wide table would
  // either clip every single-field value to a packed cell's width or leave the packed cells to
  // spill into empty neighbours. The line grouping itself is the page's (ConfigSummaryLines), so
  // what is drawn here is exactly what CountConfigSummaryLines counts.
  //
  // No spacing between the lines: ImGui puts ItemSpacing.y after every table, which would open a
  // gap between rows that used to be one table's rows. Zeroed around the run, restored after.
  const auto lines = ConfigSummaryLines(group);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
  int row_index = 0;
  for (const auto& line : lines) {
    const std::string table_id = "##summary_group_" + std::to_string(index) + "_line_" + std::to_string(row_index);
    DrawLine(table_id.c_str(), line, row_index);
    ++row_index;
  }
  ImGui::PopStyleVar();
  if (group.level > 0) {
    ImGui::Unindent(kEntryIndent);
  }
  ImGui::Spacing();
}

void DrawGroups(const std::vector<ConfigSummaryGroup>& groups, int* index) {
  for (const auto& group : groups) {
    DrawGroup(group, (*index)++);
  }
}

}  // namespace

void RenderConfigSummaryWindow(GuiState& state) {
  if (!state.config_summary_window_open) {
    return;
  }

  // Width pinned every frame, not FirstUseEver: AlwaysAutoResize below would otherwise grow the
  // window to its widest line, and the user is not meant to resize it either way. Height is
  // left to auto-resize between the content's own height and the smaller of the page's own
  // budget (kMaxWindowHeight) and the work area, minus a margin so the window's bottom edge never
  // sits under the status bar or off the screen. The budget is written into the constraint
  // rather than left to the work area alone so that "fits without scrolling" means the same
  // thing on every screen the window is opened on.
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float max_height =
      std::min(kMaxWindowHeight, viewport->WorkSize.y - 2.0f * ImGui::GetStyle().WindowPadding.y * 4.0f);
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

  // Two columns: settings left, document right. A two-column table with no chrome of its own —
  // no header, no borders, no row background — so the only headings on the page are the groups'
  // (Sun / Simulation / Render / Settings on the left, the layers on the right). The two cells
  // share one row, so the row's height is the taller column's, and the window's single scroll
  // judgement covers both at once.
  if (ImGui::BeginTable("##summary_columns", 2, ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("settings", ImGuiTableColumnFlags_WidthFixed, kSettingsColumnWidth);
    ImGui::TableSetupColumn("document", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextRow();
    int index = 0;
    ImGui::TableNextColumn();
    DrawGroups(summary.settings, &index);
    ImGui::TableNextColumn();
    DrawGroups(summary.document, &index);
    ImGui::EndTable();
  }

  ImGui::End();
}

}  // namespace lumice::gui
