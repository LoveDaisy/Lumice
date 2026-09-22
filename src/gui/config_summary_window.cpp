#include "gui/config_summary_window.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "IconsFontAwesome6.h"
#include "gui/config_summary.hpp"
#include "gui/copyable_text.hpp"
#include "gui/gui_state.hpp"
#include "gui/secondary_window_sizing.hpp"
#include "gui/theme.hpp"
#include "imgui.h"
// imgui_internal.h for the height-follows-content machinery in RenderConfigSummaryWindow only:
// ImGuiWindow::Size read back before Begin (did the user drag it?), and the DC cursor extents the
// content's own height is measured from after it is drawn. Both were read off the pinned vendor
// tag (CMakeLists.txt: v1.91.8-docking; imgui.cpp CalcWindowContentSizes / CalcWindowAutoFitSize
// are the two functions the measurement mirrors). Unlike the same fields' use in gui_test, this
// runs in the production frame — an ImGui upgrade must re-check those two functions.
#include "imgui_internal.h"

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
// The shortest the user may drag it: room for the version line and a heading or two. A UX floor
// against a window collapsed to a sliver, not a layout constant anything else derives from.
constexpr float kMinSummaryHeight = 150.0f;
constexpr const char* kWindowName = ICON_FA_FILE_LINES " Summary###ConfigSummary";

// The window's height follows its content — the page grows and shrinks with the document, as
// AlwaysAutoResize did — until the user drags it, and from then on it is theirs for the session
// (doc/gui-visual-language.md §9, "semi-variable"). The flag itself cannot be used: ImGui skips the
// resize borders' hit test when it is set (imgui.cpp UpdateWindowManualResize), so a window that
// carries it can never be dragged. What is done instead is the same thing by hand, each frame the
// window is still following: ask for the height the content needed last frame, and read the
// answer back next frame — a height that does not match the request can only be the user's.
//
// true while the height still follows the content; false once the user has dragged it.
bool g_height_follows_content = true;
// The height the last request asked for, after the same floor / ceiling clamp ImGui applies and
// the truncation SetWindowSize performs, so that an untouched window reads back exactly this.
// Negative when the last request left the height to ImGui's own auto-fit (see g_content_height),
// whose answer is not ours to compare against.
float g_requested_height = -1.0f;
// The height the content needed the last time it was drawn: ImGui's own auto-fit arithmetic
// (CalcWindowContentSizes + CalcWindowAutoFitSize), which is why it reads the window's DC cursor
// extents rather than GetCursorPos — the latter carries the trailing ItemSpacing the auto-fit does
// not, and would make the window one spacing taller than AlwaysAutoResize did. Zero until the page
// has been drawn once (a fresh process, or a reset); a request made with no measurement asks ImGui
// to auto-fit instead, which for a brand-new window is also what hides it for its first frame
// while it is measured, exactly as AlwaysAutoResize used to.
float g_content_height = 0.0f;
// The left column, sized to the longest settings line it carries (the widest label is "Ray
// allocation" and the widest value a lens name such as "dual_fisheye_equal_area"). The document
// column takes the rest — 800 px less the table padding, which the 13-column Shape table fits at
// its content width on both reference documents (measured when the tables were introduced).
constexpr float kSettingsColumnWidth = 380.0f;
// The label column of every settings line. One number for every group so labels line up across
// a group boundary; sized to the longest label the page can carry ("Ray allocation").
constexpr float kLabelColumnWidth = 170.0f;

// The row background of the `row_index`-th line of a group, alternating as ImGuiTableFlags_RowBg
// does. Set by hand because every line is its own table (see DrawGroup), and a per-table RowBg
// would restart the alternation on each of them.
void SetLineBackground(int row_index) {
  ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                         ImGui::GetColorU32(row_index % 2 == 0 ? ImGuiCol_TableRowBg : ImGuiCol_TableRowBgAlt));
}

// One settings line: a fixed label column and a stretch value column, so a long value clips at
// the window edge instead of pushing the table wider than the window (which would defeat the
// fixed width and put a horizontal scrollbar in the shot).
void DrawLine(const char* table_id, const ConfigSummaryField& field, int row_index) {
  if (!ImGui::BeginTable(table_id, 2, ImGuiTableFlags_SizingFixedFit)) {
    return;
  }
  ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, UiPx(kLabelColumnWidth));
  ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableNextRow();
  SetLineBackground(row_index);
  ImGui::TableNextColumn();
  ImGui::TextDisabled("%s", field.label.c_str());
  ImGui::TableNextColumn();
  ImGui::TextUnformatted(field.value.c_str());
  ImGui::EndTable();
}

// A settings group: its heading (SeparatorText: a rule with the title in it, the theme's own
// idiom for a heading), then one line per field.
//
// One table per line rather than one per group, kept from when a line could hold a packed run of
// several fields and the two shapes could not share a table: every line is a two-column table
// now, and the per-line tables stay because the functional test counts them by id
// ("##summary_group_<g>_line_<f>") and the references were shot with them — a single table per
// group would draw the same pixels with different ids, which is a reshoot for no visible change.
//
// No spacing between the lines: ImGui puts ItemSpacing.y after every table, which would open a
// gap between rows that used to be one table's rows. Zeroed around the run, restored after.
void DrawGroup(const ConfigSummaryGroup& group, int index) {
  ImGui::SeparatorText(group.title.c_str());
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
  for (size_t f = 0; f < group.fields.size(); ++f) {
    const std::string table_id = "##summary_group_" + std::to_string(index) + "_line_" + std::to_string(f);
    DrawLine(table_id.c_str(), group.fields[f], static_cast<int>(f));
  }
  ImGui::PopStyleVar();
  ImGui::Spacing();
}

// A document table: the header row once, a row per entry, every column sized to its content. The
// two columns that can carry a long text — the crystal's identity triple and the filter summary —
// stretch and clip at the window edge; the rest are numbers and a letter, sized to fit. (That is
// why the words and the numbers are in different tables: with the thirteen number columns beside
// them, the two stretch columns were left 90 px between them.) An empty cell is left empty: the
// column does not apply to that row, and "-" would read as a value.
void DrawDocumentTable(const char* table_id, const ConfigSummaryTable& table) {
  if (table.rows.empty()) {
    return;
  }
  constexpr ImGuiTableFlags kFlags =
      ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV;
  if (!ImGui::BeginTable(table_id, static_cast<int>(table.columns.size()), kFlags)) {
    return;
  }
  for (const auto& column : table.columns) {
    const bool stretch = column == "Crystal" || column == "Filter";
    ImGui::TableSetupColumn(column.c_str(),
                            stretch ? ImGuiTableColumnFlags_WidthStretch : ImGuiTableColumnFlags_WidthFixed);
  }
  ImGui::TableHeadersRow();
  for (const auto& row : table.rows) {
    ImGui::TableNextRow();
    for (const auto& cell : row.cells) {
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(cell.c_str());
    }
  }
  ImGui::EndTable();
}

// A layer: its heading, then its two tables, the Crystals table first. The ids carry the layer's
// index so the functional test can find each table and count its rows.
void DrawDocumentLayer(const ConfigSummaryLayer& layer, int layer_idx) {
  ImGui::SeparatorText(layer.heading.c_str());
  const std::string crystals_id = "##summary_crystals_" + std::to_string(layer_idx);
  const std::string shape_id = "##summary_shape_" + std::to_string(layer_idx);
  DrawDocumentTable(crystals_id.c_str(), layer.crystals);
  DrawDocumentTable(shape_id.c_str(), layer.shape);
  ImGui::Spacing();
}

}  // namespace

void RenderConfigSummaryWindow(GuiState& state) {
  if (!state.config_summary_window_open) {
    return;
  }

  // Did the user take the height over? Read BEFORE this frame's request: the window's Size is
  // whatever the previous frame ended with, and a request is applied at Begin() before ImGui
  // handles a drag, so a drag frame ends with the drag's size, not the request's.
  if (g_height_follows_content && g_requested_height >= 0.0f) {
    if (const ImGuiWindow* w = ImGui::FindWindowByName(kWindowName)) {
      if (std::fabs(w->Size.y - g_requested_height) > 0.5f) {
        g_height_follows_content = false;
      }
    }
  }

  // Width pinned every frame through the constraint (min == max), and the user is not meant to
  // change it. Height runs between kMinSummaryHeight and the smaller of the page's own budget
  // (kMaxWindowHeight) and the work area, minus a margin so the window's bottom edge never sits
  // under the status bar or off the screen. The budget is written into the constraint rather than
  // left to the work area alone so that "fits without scrolling" means the same thing on every
  // screen the window is opened on. Within that range the height is the content's until the user
  // drags it (g_height_follows_content), then the user's.
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float max_height = ClampedSecondaryWindowMaxHeight(UiPx(kMaxWindowHeight));
  ImGui::SetNextWindowSizeConstraints(ImVec2(UiPx(kWindowWidth), UiPx(kMinSummaryHeight)),
                                      ImVec2(UiPx(kWindowWidth), max_height));
  ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
  if (g_height_follows_content) {
    if (g_content_height > 0.0f) {
      // Truncated as SetWindowSize truncates, so the read-back above compares like with like.
      g_requested_height = std::floor(std::min(std::max(g_content_height, UiPx(kMinSummaryHeight)), max_height));
    } else {
      // No measurement yet: a height of 0 is ImGui's own "auto-fit this frame" request.
      g_requested_height = -1.0f;
    }
    ImGui::SetNextWindowSize(ImVec2(UiPx(kWindowWidth), std::max(g_requested_height, 0.0f)), ImGuiCond_Always);
  }
  if (!ImGui::Begin(kWindowName, &state.config_summary_window_open,
                    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking)) {
    ImGui::End();
    return;
  }

  // Rebuilt every frame from the live document: the page has no state of its own to fall out of
  // date, which is also why nothing here needs resetting when the document changes.
  const ConfigSummary summary = BuildConfigSummary(state);

  ImGui::TextDisabled("Lumice");
  ImGui::SameLine();
  ImGui::TextUnformatted(summary.version.c_str());
  // "Copy as text" at the right end of the version line — a small button (no vertical frame
  // padding), so the line keeps its text height and the page stays inside the same one-screen
  // budget it fit before the button existed. The text it copies is the page model, not the
  // pixels (FormatConfigSummaryAsText).
  constexpr const char* kCopyLabel = ICON_FA_COPY " Copy as text";
  const float copy_width = ImGui::CalcTextSize(kCopyLabel).x + 2.0f * ImGui::GetStyle().FramePadding.x;
  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - copy_width);
  CopyAllSmallButton(kCopyLabel, [&summary] { return FormatConfigSummaryAsText(summary); });

  // Two columns: settings left, document right. A two-column table with no chrome of its own —
  // no header, no borders, no row background — so the only headings on the page are the groups'
  // (Sun / Simulation / Render / Settings on the left, the layers on the right). The two cells
  // share one row, so the row's height is the taller column's, and the window's single scroll
  // judgement covers both at once.
  if (ImGui::BeginTable("##summary_columns", 2, ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("settings", ImGuiTableColumnFlags_WidthFixed, UiPx(kSettingsColumnWidth));
    ImGui::TableSetupColumn("document", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    for (size_t g = 0; g < summary.settings.size(); ++g) {
      DrawGroup(summary.settings[g], static_cast<int>(g));
    }
    ImGui::TableNextColumn();
    for (size_t l = 0; l < summary.document.size(); ++l) {
      DrawDocumentLayer(summary.document[l], static_cast<int>(l));
    }
    // The letters the distribution cells use, spelled out once for the whole page.
    if (!summary.document.empty()) {
      ImGui::TextDisabled("%s", DistributionLegend().c_str());
    }
    ImGui::EndTable();
  }

  // What the content needed this frame, measured as ImGui's auto-fit would next frame: the ideal
  // cursor extent (CalcWindowContentSizes' truncation included), the window padding both sides,
  // and the title bar. Taken whether or not the window is currently tall enough — clipping and
  // scrolling change what is drawn, never what is laid out.
  if (g_height_follows_content) {
    const ImGuiWindow* w = ImGui::GetCurrentWindowRead();
    const float content = IM_TRUNC(std::max(w->DC.CursorMaxPos.y, w->DC.IdealMaxPos.y) - w->DC.CursorStartPos.y);
    g_content_height = content + 2.0f * w->WindowPadding.y + w->TitleBarHeight + w->MenuBarHeight;
  }

  ImGui::End();
}

void ResetConfigSummaryWindowTestState() {
  g_height_follows_content = true;
  g_requested_height = -1.0f;
  g_content_height = 0.0f;
}

}  // namespace lumice::gui
