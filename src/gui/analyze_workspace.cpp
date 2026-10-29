#include "gui/analyze_workspace.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>

#include "gui/analysis_panel.hpp"
#include "gui/app.hpp"
#include "gui/gui_constants.hpp"
#include "gui/mono_exposure_scale.hpp"
#include "gui/panels.hpp"
#include "gui/semantic_colors.hpp"
#include "gui/sim_state_rules.hpp"
#include "gui/theme.hpp"
#include "imgui.h"
#include "util/color_space.hpp"

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

// The left column's compact crystal list: one row per entry across the scattering layers. Two
// click semantics, the cards' own: with a pick-link armed, a valid row completes the share
// (CompletePickLinkAtRow) and a self/already-shared row is disabled (PickLinkRowStateFor, the
// cards' one predicate); without one, the row opens the SAME entry editor the cards open
// (RequestEntryCardEdit -> ProcessEditRequest, the cards' own path). No thumbnails and no
// per-row delete here — those are the cards' affordances, still in the Render workspace; this
// list is the Analyze workspace's way into the same editors. The rows render inline in the
// panel's one scrolling column: a child that fills the remaining height would push the Scene
// controls (rendered after this) out of view.
void RenderCrystalList(GuiState& state) {
  ImGui::SeparatorText("Crystals");
  for (int layer_idx = 0; layer_idx < static_cast<int>(state.layers.size()); ++layer_idx) {
    const Layer& layer = state.layers[layer_idx];
    for (int entry_idx = 0; entry_idx < static_cast<int>(layer.entries.size()); ++entry_idx) {
      const EntryCard& entry = layer.entries[entry_idx];
      const CrystalConfig& crystal = state.crystals[entry.crystal_id];
      char row[192];
      // The row's identity is the entry's position in the display numbering the pick-mode hint
      // uses (1-based; spelled with a comma — a slash would read as a range) plus the crystal's
      // name as the editor will show it.
      std::snprintf(row, sizeof(row), "Layer %d, entry %d: %s##crys_%d_%d", DisplayLayerNumber(layer_idx),
                    DisplayEntryNumber(entry_idx), crystal.name.empty() ? "Crystal" : crystal.name.c_str(), layer_idx,
                    entry_idx);
      const bool pick_target = state.pick_link_source.has_value();
      const bool disabled =
          pick_target && PickLinkRowStateFor(state, layer_idx, entry_idx) == PickLinkRowState::kDisabledSelfOrShared;
      ImGui::BeginDisabled(disabled);
      const bool clicked = ImGui::Selectable(row);
      ImGui::EndDisabled();
      if (clicked && !disabled) {
        if (pick_target) {
          CompletePickLinkAtRow(state, layer_idx, entry_idx);
        } else {
          RequestEntryCardEdit(layer_idx, entry_idx);
        }
      }
    }
  }
  if (ImGui::SmallButton("+ Layer")) {
    AddScatteringLayer(state);
  }
}

}  // namespace

// The scene panorama: the committed texture — always dual equal-area full-sky,
// gui-state-governance.md §9.1 — through the existing reprojection family at Fisheye Equal Area
// / 180°, hemisphere per the view state. The params are composed fresh each frame from the same
// supply functions the Render preview's come from (ComputeMonoExposure, SrgbToLinearRgb, the
// kDualFisheyeOverlap source constants) with only the view overridden; the photo overlay and the
// auxiliary lines stay off — Render-workspace affordances the panorama does not inherit. The
// picture itself is drawn by the deferred pass (RenderPreviewFrameAndBlit into the region this
// publishes to g_panorama_vp); this side draws the state around it: the hemisphere switch, the
// empty state, and the preview's own two-branch notice (AnalysisPictureNotice — the same
// predicate, never a second freshness judgment).
void RenderScenePanorama(GLFWwindow* window, float window_height) {
  g_panorama_vp.active = false;  // level-triggered, like the preview viewport's own publication

  ImGui::SeparatorText("Scene panorama");
  int upper = g_state.workspace.panorama_upper ? 0 : 1;
  ImGui::RadioButton("Upper##panorama_hemisphere", &upper, 0);
  ImGui::SameLine();
  ImGui::RadioButton("Lower##panorama_hemisphere", &upper, 1);
  g_state.workspace.panorama_upper = upper == 0;

  if (!g_preview.HasTexture()) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped(
        "No rendered image for this document yet — press Run once in the Render workspace; the "
        "panorama shows the traced sky here.");
    ImGui::PopStyleColor();
    return;
  }

  // The picture region: transparent, because the deferred pass blits the reprojected texture
  // under ImGui exactly as it does for the Render preview panel.
  const ImVec2 size = ImGui::GetContentRegionAvail();
  ImGui::BeginChild("##AnalyzePanorama", size, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoBackground);
  const ImVec2 region_min = ImGui::GetWindowPos();
  ImGui::EndChild();

  int fb_w = 0;
  int fb_h = 0;
  glfwGetFramebufferSize(window, &fb_w, &fb_h);
  const float dpi_x = static_cast<float>(fb_w) / ImGui::GetIO().DisplaySize.x;
  const float dpi_y = static_cast<float>(fb_h) / ImGui::GetIO().DisplaySize.y;
  g_panorama_vp.active = true;
  g_panorama_vp.vp_x = static_cast<int>(region_min.x * dpi_x);
  // OpenGL Y is bottom-up (the preview panel's own comment): the region's bottom edge measured
  // from the window's bottom.
  g_panorama_vp.vp_y = static_cast<int>((ImGui::GetIO().DisplaySize.y - (region_min.y + size.y)) * dpi_y);
  g_panorama_vp.vp_w = static_cast<int>(size.x * dpi_x);
  g_panorama_vp.vp_h = static_cast<int>(size.y * dpi_y);
  g_panorama_vp.dpi_scale_x = dpi_x;
  g_panorama_vp.dpi_scale_y = dpi_y;

  PreviewParams& pp = g_panorama_vp.params;
  auto& rc = g_state.renderer;
  pp.source.max_abs_dz = kDualFisheyeOverlap;
  pp.source.r_scale = 1.0f / std::sqrt(1.0f + kDualFisheyeOverlap);
  lumice::SrgbToLinearRgb(rc.background, pp.background_color_linear);
  lumice::SrgbToLinearRgb(rc.paper, pp.paper_color_linear);
  pp.tone = rc.tone;
  pp.display_mode = rc.display_mode;
  // The same mono exposure the preview computes per frame, so the EV controls act on the
  // panorama on the very next frame too — one supply, two views.
  lumice::gui::MonoExposureInput mono_ev_in;
  mono_ev_in.exposure_offset = rc.exposure_offset;
  mono_ev_in.ev_auto = g_state.ev_auto;
  mono_ev_in.snapshot_intensity = g_state.snapshot_intensity;
  mono_ev_in.snapshot_emitted_energy = g_state.snapshot_emitted_energy;
  mono_ev_in.total_pixels = g_preview.GetTextureWidth() * g_preview.GetTextureHeight();
  const lumice::gui::MonoExposure mono_ev = lumice::gui::ComputeMonoExposure(rc.ev_mode, mono_ev_in);
  pp.exposure.intensity_factor = mono_ev.intensity_factor;
  pp.exposure.intensity_scale = mono_ev.intensity_scale;
  // The up- (or down-) looking equal-area fisheye: the same framing PanoramaAnnotationView gives
  // the pick, so the picture and the click agree by construction. visible=Full — the fisheye's
  // own behind-camera bound is the clip; the hemisphere is the camera pitch.
  pp.view_proj = ViewProjection{ kLensTypeFisheyeEqualArea,
                                 180.0f,
                                 g_state.workspace.panorama_upper ? 90.0f : -90.0f,
                                 0.0f,
                                 0.0f,
                                 kVisibleFull,
                                 false };

  // ---- The cone ROI's gestures, over this region and this view ----
  // The same gesture family the preview panel hosts (marker drag > armed pick), through
  // PanoramaAnnotationView — the view the picture under the cursor was drawn with. No camera
  // branch: the panorama's surface has no orbit, so the arbiter runs with its two analysis arms
  // only, and the angular-distance picker cannot reach here — that pick lives in the Render
  // workspace's panels.
  auto& a = g_state.analysis;
  if (a.roi_mode == LUMICE_RAYPATH_ROI_CONE) {
    const LUMICE_AnnotationView view = PanoramaAnnotationView(g_state, g_panorama_vp.vp_w, g_panorama_vp.vp_h);
    // A centre when there is none — level-triggered, the preview panel's own rule, so the mode
    // gets its default as soon as the picture region exists.
    if (!a.cone_center_valid) {
      EnsureDefaultConeCenter(g_state, view, g_panorama_vp.vp_w, g_panorama_vp.vp_h);
    }
    const ImGuiIO& io = ImGui::GetIO();
    const bool region_hovered =
        ImGui::IsMouseHoveringRect(region_min, ImVec2(region_min.x + size.x, region_min.y + size.y));
    bool marker_hover = false;
    if (region_hovered) {
      if (const std::optional<CanvasPixel> marker = ProjectConeCenterMarker(g_state, view); marker.has_value()) {
        float mx = 0.0f;
        float my = 0.0f;
        CanvasPixelToPreviewPoint(marker->px, marker->py, g_panorama_vp.dpi_scale_x, g_panorama_vp.dpi_scale_y, &mx,
                                  &my);
        const float dx = io.MousePos.x - region_min.x - mx;
        const float dy = io.MousePos.y - region_min.y - my;
        const float hit_r = UiPx(kAnalysisConeMarkerHitRadiusPt);
        marker_hover = dx * dx + dy * dy <= hit_r * hit_r;
      }
    }
    const ConeInputOwner owner = ArbitrateConeInput(marker_hover || a.cone_marker_dragging, a.pick_armed);
    if (owner == ConeInputOwner::kMarkerDrag) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      if (region_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        a.cone_marker_dragging = true;
        a.pick_armed = false;
      }
      if (a.cone_marker_dragging && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if (const std::optional<CanvasPixel> px = PreviewPointToCanvasPixel(
                io.MousePos.x - region_min.x, io.MousePos.y - region_min.y, g_panorama_vp.dpi_scale_x,
                g_panorama_vp.dpi_scale_y, g_panorama_vp.vp_w, g_panorama_vp.vp_h);
            px.has_value() && PanoramaPickHitsSky(g_state, view, px->px, px->py)) {
          DragAnalysisConeCenter(g_state, view, px->px, px->py);
        }
      }
      if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        a.cone_marker_dragging = false;
      }
    }
    if (owner == ConeInputOwner::kPickClick && region_hovered) {
      // The armed crosshair, drawn on this window's list like the ring below.
      ImDrawList* dl = ImGui::GetWindowDrawList();
      const ImU32 colour = ImGui::ColorConvertFloat4ToU32(AccentColor());
      const ImVec2 m = io.MousePos;
      const float arm = UiPx(kAnalysisPickCrosshairArmPt);
      dl->AddLine(ImVec2(m.x - arm, m.y), ImVec2(m.x + arm, m.y), colour, 1.5f);
      dl->AddLine(ImVec2(m.x, m.y - arm), ImVec2(m.x, m.y + arm), colour, 1.5f);
      if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (const std::optional<CanvasPixel> px = PreviewPointToCanvasPixel(
                io.MousePos.x - region_min.x, io.MousePos.y - region_min.y, g_panorama_vp.dpi_scale_x,
                g_panorama_vp.dpi_scale_y, g_panorama_vp.vp_w, g_panorama_vp.vp_h);
            px.has_value() && PanoramaPickHitsSky(g_state, view, px->px, px->py)) {
          PickAnalysisConeCenter(g_state, view, px->px, px->py);
        }
      }
    }
    DrawPanoramaRoiRing(g_state, view, region_min, g_panorama_vp.dpi_scale_x, g_panorama_vp.dpi_scale_y);
  }
  // Esc disarms an armed pick — the preview panel's rule, hosted here while the workspace owns
  // the picture an armed pick aims at.
  if (a.pick_armed && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    a.pick_armed = false;
  }

  // The preview's own two-branch notice, below the picture: kNone (nothing ever rendered) cannot
  // reach here (the texture gate above), so this is the kModified branch — the picture is of the
  // configuration before the edit.
  if (const char* notice = AnalysisPictureNotice(g_state.run_intent, g_state.sim_state)) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped("%s", notice);
    ImGui::PopStyleColor();
  }
}

void RenderAnalyzeWorkspace(GLFWwindow* window, float window_width, float window_height) {
  // Level-triggered publication, the same statement RenderPreviewPanel makes every frame: the
  // frame that does not draw the preview publishes neither the viewport nor the curve labels, so
  // every consumer reads "no picture on screen" (DoAnalyze's IN_FRAME canvas falls back to the
  // document's own resolution, the deferred blit skips, the export gates stay closed).
  g_preview_vp.active = false;
  g_preview_vp.curve_labels.clear();

  // Pick-mode Esc cancel — read before any ImGui::Begin so the key event is not consumed by
  // inner widgets first (RenderLeftPanel's own rule; an armed pick is session state, not a
  // Render-panel property, and the mode switch must not orphan it).
  const bool pick_active_at_entry = g_state.pick_link_source.has_value();
  if (pick_active_at_entry && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    g_state.pick_link_source.reset();
  }
  // Blank-area cancel: if the pick survived the frame's rows (no row consumed the click), a
  // left click anywhere this frame cancels it — the cards' host behavior, mirrored.
  std::optional<GuiState::EntryRef> pick_source_at_entry =
      pick_active_at_entry ? g_state.pick_link_source : std::nullopt;

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
    // Pick completion reopens the editor on the pick's source — the cards' host post-pass.
    ReopenEditorOnPickCompletion(g_state, pick_source_at_entry);
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
    RenderScenePanorama(window, window_height);
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

void RenderWorkspacePanels(GLFWwindow* window, float window_width, float window_height) {
  if (g_state.workspace.mode == GuiState::WorkspaceMode::kAnalyze) {
    RenderAnalyzeWorkspace(window, window_width, window_height);
  } else {
    RenderLeftPanel(window_height);
    RenderRightPanel(window, window_width, window_height);
    RenderPreviewPanel(window, window_width, window_height);
  }
}

}  // namespace lumice::gui
