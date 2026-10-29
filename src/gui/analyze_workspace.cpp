#include "gui/analyze_workspace.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "gui/app.hpp"
#include "gui/gui_constants.hpp"
#include "gui/panels.hpp"
#include "gui/semantic_colors.hpp"
#include "gui/theme.hpp"
#include "imgui.h"

namespace lumice::gui {
namespace {

// One not-wired region of the workspace (the plan's R7): a named child with the region's title
// and an explicit state line — where the real thing will come from, never demo data, never fake
// numbers. The region shows that it is not wired, which is the honest state.
void PlaceholderRegion(const char* name, const char* title, const char* note, const ImVec2& size) {
  ImGui::BeginChild(name, size, ImGuiChildFlags_Borders);
  ImGui::TextUnformatted(title);
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
  ImGui::TextWrapped("%s", note);
  ImGui::PopStyleColor();
  ImGui::EndChild();
}

// The middle area's horizontal divider: a full-width grab strip with the resize-NS cursor, the
// classic ImGui splitter shape (invisible button + hover/active line drawn over it), writing
// GuiState::AnalyzeWorkspaceSession::middle_rows_ratio. Level-triggered like every other drag:
// only a frame with a drag in flight writes the ratio.
void MiddleRowsSplitter(const char* id, float width, float* ratio) {
  const float height = UiPx(8.0f);
  ImGui::InvisibleButton(id, ImVec2(width, height));
  const bool hovered = ImGui::IsItemHovered();
  const bool active = ImGui::IsItemActive();
  if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    // The splitter sits between the rows, so its centre is the ratio's pivot: move the mouse,
    // move the pivot. Parent-local y via GetCursorStartPos — the item's own start is the strip.
    const float pivot_y = ImGui::GetItemRectMin().y + height * 0.5f;
    const float window_top = ImGui::GetWindowPos().y;
    const float window_h = ImGui::GetWindowHeight();
    *ratio = (pivot_y + ImGui::GetIO().MouseDelta.y - window_top) / window_h;
    *ratio = std::clamp(*ratio, 0.15f, 0.85f);
  }
  if (hovered || active) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
  }
  // The line: dim normally, themed while grabbed — the only affordance the strip has.
  const ImU32 line_col = ImGui::GetColorU32(active  ? ImGuiCol_SeparatorActive :
                                            hovered ? ImGuiCol_SeparatorHovered :
                                                      ImGuiCol_Separator);
  ImGui::GetWindowDrawList()->AddLine(ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMin().y + height * 0.5f),
                                      ImVec2(ImGui::GetItemRectMax().x, ImGui::GetItemRectMin().y + height * 0.5f),
                                      line_col, 1.0f);
}

// The left column's compact crystal list: one row per entry across the scattering layers, each
// opening the SAME entry editor the cards open (RequestEntryCardEdit -> ProcessEditRequest, the
// cards' own path). No thumbnails and no per-row delete here — those are the cards' affordances,
// still in the Render workspace; this list is the Analyze workspace's way into the same editors.
// The rows render inline in the panel's one scrolling column: a child that fills the remaining
// height would push the Scene controls (rendered after this) out of a NoScrollbar panel.
void RenderCrystalList(GuiState& state) {
  ImGui::SeparatorText("Crystals");
  for (int layer_idx = 0; layer_idx < static_cast<int>(state.layers.size()); ++layer_idx) {
    const Layer& layer = state.layers[layer_idx];
    for (int entry_idx = 0; entry_idx < static_cast<int>(layer.entries.size()); ++entry_idx) {
      const EntryCard& entry = layer.entries[entry_idx];
      const CrystalConfig& crystal = state.crystals[entry.crystal_id];
      char row[160];
      // The row's identity is the entry's position in the display numbering the pick-mode hint
      // uses (1-based; spelled with a comma — a slash would read as a range) plus the crystal's
      // name as the editor will show it.
      std::snprintf(row, sizeof(row), "Layer %d, entry %d: %s##crys_%d_%d", DisplayLayerNumber(layer_idx),
                    DisplayEntryNumber(entry_idx), crystal.name.empty() ? "Crystal" : crystal.name.c_str(), layer_idx,
                    entry_idx);
      if (ImGui::Selectable(row)) {
        RequestEntryCardEdit(layer_idx, entry_idx);
      }
    }
  }
  if (ImGui::SmallButton("+ Layer")) {
    AddScatteringLayer(state);
  }
}

}  // namespace

void RenderWorkspacePanels(GLFWwindow* window, float window_width, float window_height) {
  if (g_state.workspace.mode == GuiState::WorkspaceMode::kAnalyze) {
    RenderAnalyzeWorkspace(window, window_width, window_height);
  } else {
    RenderLeftPanel(window_height);
    RenderRightPanel(window, window_width, window_height);
    RenderPreviewPanel(window, window_width, window_height);
  }
}

void RenderAnalyzeWorkspace(GLFWwindow* /*window*/, float window_width, float window_height) {
  // Level-triggered publication, the same statement RenderPreviewPanel makes every frame: the
  // frame that does not draw the preview publishes neither the viewport nor the curve labels, so
  // every consumer reads "no picture on screen" (DoAnalyze's IN_FRAME canvas falls back to the
  // document's own resolution, the deferred blit skips, the export gates stay closed).
  g_preview_vp.active = false;
  g_preview_vp.curve_labels.clear();

  const float panel_height = window_height - UiPx(kTopBarHeight) - UiPx(kStatusBarHeight);
  const float top = UiPx(kTopBarHeight);
  const float left_w = UiPx(kAnalyzeLeftPanelWidth);
  const float right_w = UiPx(kAnalyzeRightPanelWidth);
  const float mid_w = std::max(0.0f, window_width - left_w - right_w);

  // ---- Left column: crystal list + Sun + Simulation (the Render right panel's Scene controls,
  // the very same function). One scrolling column, like the prototype stacks them — a long
  // crystal list scrolls the Scene group below it rather than hiding it. ----
  SetNextPanelGeometry(0.0f, top, left_w, panel_height);
  ImGui::Begin("##AnalyzeLeftPanel", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
  {
    // An armed pick-link keeps its hint bar and its Esc cancel in this column too — the pick is
    // armed session state, not a Render-panel property, and a mode switch must not orphan it.
    if (g_state.pick_link_source.has_value()) {
      const auto& src = *g_state.pick_link_source;
      ImGui::PushStyleColor(ImGuiCol_Text, WarningTextColor());
      ImGui::TextWrapped("Pick mode: click an entry to share crystal/filter from Layer %d / Entry %d (Esc to cancel)",
                         DisplayLayerNumber(src.layer_idx), DisplayEntryNumber(src.entry_idx));
      ImGui::PopStyleColor();
      ImGui::Separator();
    }
    RenderCrystalList(g_state);
    ProcessEditRequest(g_state);
    ImGui::SeparatorText("Scene");
    RenderSceneControls(g_state);
  }
  ImGui::End();

  // ---- Middle: two columns x two rows and the divider between the rows ----
  SetNextPanelGeometry(left_w, top, mid_w, panel_height);
  ImGui::Begin("##AnalyzeMiddle", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus);
  {
    const float col_w = (std::max(0.0f, ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x)) * 0.5f;
    float upper_h = panel_height * g_state.workspace.middle_rows_ratio;
    float lower_h = std::max(0.0f, panel_height - upper_h - UiPx(8.0f));

    // Upper row: scene panorama | raypath share list.
    PlaceholderRegion("##AnalyzePanorama", "Scene panorama",
                      "Not wired up yet. Will show the committed scene as a single-disk equal-area view "
                      "(upper/lower hemisphere) and take the cone ROI.",
                      ImVec2(col_w, upper_h));
    ImGui::SameLine();
    PlaceholderRegion("##AnalyzeShareList", "Raypath share list",
                      "Lives in the Raypath Analysis window during the migration; this region takes it over.",
                      ImVec2(0, upper_h));

    MiddleRowsSplitter("##AnalyzeRowsSplitter", ImGui::GetContentRegionAvail().x, &g_state.workspace.middle_rows_ratio);

    // Lower row: raypath view | raypath 3D.
    PlaceholderRegion("##AnalyzeRaypathView", "Raypath view",
                      "Not wired up yet. Will show one raypath class's sky picture with pan, zoom and the sky-point "
                      "pick.",
                      ImVec2(col_w, lower_h));
    ImGui::SameLine();
    PlaceholderRegion("##AnalyzeRaypath3D", "Raypath 3D",
                      "Not wired up yet. Will show the selected path's crystal and its 3D geometry.",
                      ImVec2(0, lower_h));
  }
  ImGui::End();

  // ---- Right column: sun-direction globe above the selection details ----
  SetNextPanelGeometry(left_w + mid_w, top, right_w, panel_height);
  ImGui::Begin("##AnalyzeRightPanel", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);
  {
    const float globe_h = panel_height * 0.4f;
    PlaceholderRegion("##AnalyzeSunGlobe", "Sun direction globe",
                      "Not wired up yet. Will show the sun direction in the selected crystal's frame.",
                      ImVec2(0, globe_h));
    ImGui::Spacing();
    PlaceholderRegion("##AnalyzeDetails", "Selection details",
                      "Select a class in the Raypath Analysis window; the class overview takes this region.",
                      ImVec2(0, 0));
  }
  ImGui::End();
}

}  // namespace lumice::gui
