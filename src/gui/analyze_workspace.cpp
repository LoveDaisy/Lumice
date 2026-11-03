#include "gui/analyze_workspace.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>

#include "IconsFontAwesome6.h"
#include "gui/analysis_panel.hpp"
#include "gui/analyze_map_view.hpp"
#include "gui/analyze_overview.hpp"
#include "gui/analyze_point_view.hpp"
#include "gui/analyze_workspace.hpp"
#include "gui/app.hpp"
#include "gui/destructive_style.hpp"
#include "gui/gui_constants.hpp"
#include "gui/mono_exposure_scale.hpp"
#include "gui/panels.hpp"
#include "gui/panorama_viewport.hpp"
#include "gui/semantic_colors.hpp"
#include "gui/sim_state_rules.hpp"
#include "gui/theme.hpp"
#include "imgui.h"
#include "util/color_space.hpp"
#include "util/raypath_analysis_display.hpp"  // RoiModeLabel — the diagnostics summary's region line
#include "util/sky_direction.hpp"             // DirToAltAz — the diagnostics summary's centre line
#include "util/thousands_format.hpp"          // FormatThousands — the diagnostics run-status line

namespace lumice::gui {
namespace {

// The diagnostics region's idle state (analyze-interaction-contract.md §9.1 — a first-class
// design, not an empty placeholder): the entry guidance (the list's own empty-state line, plus
// the next step — no picture yet points at the Render workspace's Run, a picture points at the
// region radios and the list's Analyze), the analysis's own run status while one is in flight
// (the same state machine and Stop the list's controls drive — two faces of one truth, the
// contract's cancel entry for this region), and the observation summary (the request as it
// stands, for the "edit the scene, compare again" pass). Everything is a display read of fields
// another region already renders or the session already holds: no new query, no new state owner.
void RenderAnalysisIdleDiagnostics(GuiState& state) {
  const auto& a = state.analysis;
  ImGui::TextUnformatted("Diagnostics");
  // ---- The entry guidance: where an analysis starts from, as the state actually is. ----
  if (state.analysis_result.payload == nullptr) {
    // The list's own empty-state line (RenderResultList's, one source for the same fact).
    ImGui::TextDisabled("No result yet. Choose a region and press Analyze.");
  }
  // The same branch logic as the panorama's empty state: without a picture of the document the
  // first step is the Render workspace's Run; with one, the radios and the list's Analyze are
  // the path. The controls themselves stay where they are in this slice (the region radios
  // above the picture, the Analyze button in the list) — the guidance names the real places.
  // Each branch is its own named child (the pick banner's reason: a text item has no id a test
  // can address; a window does), so a case can pin which guidance is on screen.
  if (!HasEverShownPicture(state.run_intent)) {
    ImGui::BeginChild("##AnalyzeIdleNoPictureGuide", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AutoResizeY);
    ImGui::TextWrapped(
        "No rendered image for this document yet — press Run once in the Render "
        "workspace; an analysis needs a picture of the document to aim at.");
    ImGui::EndChild();
  } else {
    ImGui::BeginChild("##AnalyzeIdlePictureGuide", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AutoResizeY);
    ImGui::TextWrapped("Choose a region above the picture, then press Analyze in the list above.");
    ImGui::EndChild();
  }
  if (state.analysis_result.payload != nullptr) {
    ImGui::TextWrapped("Select a class in the list above; the class overview takes this region.");
  }
  // ---- The run status: the analysis in flight, with its cancel entry. ----
  if (state.analysis_run_in_progress) {
    LUMICE_RayCount rays = 0;
    if (g_server != nullptr) {
      LUMICE_GetSimRayCount(g_server, &rays);
    }
    ImGui::TextColored(AccentColor(), "Analyzing (%s)... %s rays", RoiModeLabel(a.roi_mode),
                       FormatThousands(static_cast<std::uint64_t>(rays)).c_str());
    PushDestructiveStyle();
    if (ImGui::SmallButton(ICON_FA_STOP " Stop")) {
      DoStop();
    }
    PopDestructiveStyle();
  }
  // ---- The observation summary: the request as it stands. A named child like the guides —
  // its presence is what a case pins (the bullets themselves are plain text). ----
  ImGui::BeginChild("##AnalyzeIdleSummary", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AutoResizeY);
  ImGui::Separator();
  ImGui::BulletText("Region: %s", RoiModeLabel(a.roi_mode));
  if (a.roi_mode == LUMICE_RAYPATH_ROI_CONE) {
    if (a.cone_center_valid) {
      float alt = 0.0f;
      float az = 0.0f;
      DirToAltAz(a.cone_center_dir, &alt, &az);
      ImGui::BulletText("Cone centre: altitude %.1f deg, azimuth %.1f deg", alt, az);
    } else {
      ImGui::BulletText("Cone centre: not set");
    }
  }
  if (a.infinite) {
    ImGui::BulletText("Analysis budget: infinite rays");
  } else {
    ImGui::BulletText("Analysis budget: %.1fM rays", a.ray_num_millions);
  }
  if (state.sim_state == GuiState::SimState::kModified) {
    ImGui::PushStyleColor(ImGuiCol_Text, WarningTextColor());
    ImGui::BulletText("Document: modified — the picture predates the edit.");
    ImGui::PopStyleColor();
  } else {
    ImGui::BulletText("Document: up to date");
  }
  ImGui::EndChild();
}

// The main view column's horizontal divider: a full-width grab strip with the resize-NS cursor,
// the classic ImGui splitter shape (invisible button + hover/active line drawn over it), writing
// GuiState::AnalyzeWorkspaceSession::middle_rows_ratio. Level-triggered like every other drag:
// only a frame with a drag in flight writes the ratio. The strip lives in its own thin window,
// so the ratio's reference frame — the main view column's top and height — is passed in;
// measuring against the strip's own window would make the ratio a function of the last mouse
// delta alone.
void MiddleRowsSplitter(const char* id, float width, float* ratio, float area_top, float area_h) {
  const float height = UiPx(kAnalyzeSplitterSize);
  ImGui::InvisibleButton(id, ImVec2(width, height));
  const bool hovered = ImGui::IsItemHovered();
  const bool active = ImGui::IsItemActive();
  const ImVec2 mouse_delta = ImGui::GetIO().MouseDelta;
  if (active && (mouse_delta.x != 0.0f || mouse_delta.y != 0.0f) && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    // The splitter sits between the rows, so its centre is the ratio's pivot: move the mouse,
    // move the pivot. Screen-space y against the main view column's frame. The zero-delta gate
    // is not decoration: with the button held on a stationary pointer, IsMouseDragging stays
    // true and a pivot-plus-zero write would read the strip's CURRENT position as the ratio —
    // a half-strip-width step per frame, the strip crawling away from the resting hand.
    const float pivot_y = ImGui::GetItemRectMin().y + height * 0.5f;
    *ratio = (pivot_y + mouse_delta.y - area_top) / area_h;
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

// D1 (main view | right column), the row splitter's vertical counterpart — the same recipe
// (invisible button + hover/active line + the one affordance line + level-triggered write),
// with the resize-EW cursor, measuring against the split's reference frame (the span right of
// the fixed left column) and clamping the write to the pixel floors (gui_constants.hpp): a drag
// past a floor pins the ratio at that floor, at this window width — the layout re-derives the
// pixels every frame, the stored ratio stays the split's identity.
void RightColumnSplitter(const char* id, float height, float* ratio, float area_left, float area_w) {
  const float width = UiPx(kAnalyzeSplitterSize);
  ImGui::InvisibleButton(id, ImVec2(width, height));
  const bool hovered = ImGui::IsItemHovered();
  const bool active = ImGui::IsItemActive();
  const ImVec2 mouse_delta = ImGui::GetIO().MouseDelta;
  if (active && (mouse_delta.x != 0.0f || mouse_delta.y != 0.0f) && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    // The strip's right edge is the split: the right column is what remains of the span to the
    // strip's right, so the ratio reads off the strip's trailing edge — dragging the hand right
    // narrows the right column. Screen-space x against the span's right edge. (The zero-delta
    // gate: a held-still pointer must not write the strip's own position back as the ratio —
    // the same crawl the row splitter's gate blocks.)
    const float strip_right = ImGui::GetItemRectMax().x + mouse_delta.x;
    *ratio = (area_left + area_w - strip_right) / area_w;
    const float right_floor = UiPx(kAnalyzeRightColMinWidth);
    const float main_floor = UiPx(kAnalyzeMainViewMinWidth);
    if (area_w >= right_floor + main_floor + width) {
      *ratio = std::clamp(*ratio, right_floor / area_w, (area_w - width - main_floor) / area_w);
    } else {
      // Both floors cannot fit: the contract's ③ split is the only reachable one, so the ratio
      // pins to it while the window is this narrow.
      *ratio = right_floor / (right_floor + main_floor);
    }
  }
  if (hovered || active) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
  }
  const ImU32 line_col = ImGui::GetColorU32(active  ? ImGuiCol_SeparatorActive :
                                            hovered ? ImGuiCol_SeparatorHovered :
                                                      ImGuiCol_Separator);
  ImGui::GetWindowDrawList()->AddLine(ImVec2(ImGui::GetItemRectMin().x + width * 0.5f, ImGui::GetItemRectMin().y),
                                      ImVec2(ImGui::GetItemRectMin().x + width * 0.5f, ImGui::GetItemRectMax().y),
                                      line_col, 1.0f);
}

// D2 (share list | diagnostics within the right column), the row splitter's shape with the same
// pixel-floor clamp: the list keeps its fixed head plus the table's header row, one data row and
// the button row; the diagnostics region keeps its title and one state line.
void RightRowsSplitter(const char* id, float width, float* ratio, float area_top, float area_h) {
  const float height = UiPx(kAnalyzeSplitterSize);
  ImGui::InvisibleButton(id, ImVec2(width, height));
  const bool hovered = ImGui::IsItemHovered();
  const bool active = ImGui::IsItemActive();
  const ImVec2 mouse_delta = ImGui::GetIO().MouseDelta;
  if (active && (mouse_delta.x != 0.0f || mouse_delta.y != 0.0f) && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    // (Zero-delta gate: the row splitter's crawl block, same reason.)
    const float pivot_y = ImGui::GetItemRectMin().y + height * 0.5f;
    *ratio = (pivot_y + mouse_delta.y - area_top) / area_h;
    const float list_floor = UiPx(kAnalyzeShareListMinHeight);
    const float details_floor = UiPx(kAnalyzeDetailsMinHeight);
    if (area_h >= list_floor + details_floor + height) {
      *ratio = std::clamp(*ratio, list_floor / area_h, (area_h - height - details_floor) / area_h);
    } else {
      *ratio = list_floor / (list_floor + details_floor);
    }
  }
  if (hovered || active) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
  }
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
  // The publication is cleared once per frame at the dispatch point; this function publishes
  // by assignment below.

  ImGui::SeparatorText("Scene panorama");
  // The ROI controls sit ABOVE the picture: Point mode's gestures aim at the picture below them.
  RenderPanoramaRoiControls(g_state);
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
  // The publish quantities, packed for the one conversion in panorama_viewport.hpp. Under
  // ViewportsEnable the child rect is OS-screen absolute — GetWindowPos() carries the main
  // viewport's origin, the MainVpPos idiom in app_panels.cpp being the same fact used in the
  // other direction — while the GL viewport the deferred blit fills counts main-window-local
  // pixels. The origin travels with the rect so the conversion can localize it; the gesture
  // block below keeps comparing region_min against io.MousePos, both absolute, and must not.
  lumice::gui::PanoramaSurface surface;
  surface.display_w = ImGui::GetIO().DisplaySize.x;
  surface.display_h = ImGui::GetIO().DisplaySize.y;
  surface.fb_w = fb_w;
  surface.fb_h = fb_h;
  lumice::gui::PanoramaScreenRect screen_rect;
  screen_rect.x = region_min.x;
  screen_rect.y = region_min.y;
  screen_rect.w = size.x;
  screen_rect.h = size.y;
  const ImGuiViewport* main_vp = ImGui::GetMainViewport();
  lumice::gui::PanoramaScreenPoint main_vp_origin;
  main_vp_origin.x = main_vp->Pos.x;
  main_vp_origin.y = main_vp->Pos.y;
  const lumice::gui::PanoramaGlViewport vp =
      lumice::gui::PanoramaChildRectToGlViewport(screen_rect, main_vp_origin, surface);
  g_panorama_vp.active = true;
  g_panorama_vp.vp_x = vp.vp_x;
  g_panorama_vp.vp_y = vp.vp_y;
  g_panorama_vp.vp_w = vp.vp_w;
  g_panorama_vp.vp_h = vp.vp_h;
  g_panorama_vp.dpi_scale_x = vp.dpi_scale_x;
  g_panorama_vp.dpi_scale_y = vp.dpi_scale_y;

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

// ---- The class overview (742.4) ---------------------------------------------------------------------

// The overview's four visible states (R7): waiting, cancelled, the engine's named refusal, and
// the completed document read through the layered overview model (analyze_overview.hpp — the
// document's own facts organized by routing arm; nothing computed here, R8). Called only with a
// selection on show, which — after this frame's pump — means the intent is valid, so
// kNoSelection cannot appear here.
void RenderSurveyOverview(const SelectedClassIdentity& identity) {
  SurveyQuerySession& survey = g_state.analysis.survey;
  switch (SurveyOverviewStateFor(survey)) {
    case SurveyOverviewState::kNoSelection:
      break;  // unreachable with a selection on show — the pump reconciled before any render
    case SurveyOverviewState::kWaiting: {
      // The wait line is the pure function's output (unit-held bytes), and the same string is
      // reported to the render-feedback field: what the test reads is what was drawn (AC4).
      // The poll snapshot rides along: the stage clause appears only while the SURVEY face
      // itself is computing, named from the engine's own ledger (742's wait grammar + 751).
      const std::string wait_line =
          SurveyWaitLineFor(survey, ImGui::GetTime(), g_survey_executor.InflightPollSnapshot());
      g_state.analysis.wait_feedback_texts.survey_wait_line = wait_line;
      ImGui::TextUnformatted(wait_line.c_str());
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
      ImGui::TextWrapped("An engine-typed summary of this class is computed off the interface thread.");
      ImGui::PopStyleColor();
      ImGui::Spacing();
      if (ImGui::SmallButton("Cancel survey")) {
        // R6's cancel row: request the cancel and withdraw the intent — no re-issue while this
        // selection stands (that is what SurveyOverviewStateFor turns into kCancelled next
        // frame). A result that already returned is dropped by the gate: the intent it would
        // have to answer no longer exists.
        g_survey_executor.CancelInFlight(QueryFace::kSurvey);
        survey.cancelled_for_key = true;
        survey.adopted_result.reset();
        survey.adopted_error.clear();
      }
      break;
    }
    case SurveyOverviewState::kCancelled:
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
      ImGui::TextWrapped(
          "Survey cancelled for this selection. Selecting another class — or re-Analyze and select again — "
          "asks anew.");
      ImGui::PopStyleColor();
      break;
    case SurveyOverviewState::kRefused:
      ImGui::TextUnformatted("Survey refused:");
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
      ImGui::TextWrapped("%s", survey.adopted_error.c_str());
      ImGui::PopStyleColor();
      break;
    case SurveyOverviewState::kCompleted:
      RenderSurveyOverviewModel(BuildSurveyOverviewModel(*survey.adopted_result));
      break;
  }
  // The M1-0 disposition's honesty label: an intermediate P/B/D read position has NO survey
  // class, so a literal-sequence answer describes the selected row's own representative member,
  // never a class summary. The all-three and all-unchecked positions are exact and unlabelled.
  // Selection-side, not document-side — the document cannot know which row asked for it.
  if (survey.adopted_result != nullptr && identity.symmetry_bits_plus_one == 1) {
    const uint8_t bits = AnalysisSymmetryBits(g_state);
    if (bits != 0 && bits != 7) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
      ImGui::TextWrapped(
          "Literal-sequence scope: the checked P/B/D reduction has no survey class, so these are the selected "
          "row's own representative member, not a class summary.");
      ImGui::PopStyleColor();
    }
  }
}

void RenderAnalyzeWorkspace(GLFWwindow* window, float window_width, float window_height) {
  // The background queries' frame pumps (742.3 R6, the map face added by 742.5): drain finished
  // outcomes through the adoption gates, then reconcile each intent against its live inputs —
  // before any region reads the sessions, so the overview and the map on screen are always this
  // frame's state. Render mode does not pump: in-flight queries keep running (R6 — switching
  // away neither cancels nor cleans) and their outcomes wait in the single slots for the return.
  PumpSurveyQuery(g_state, g_survey_executor, ImGui::GetTime());
  PumpMapQuery(g_state, g_survey_executor, ImGui::GetTime());
  // The point face's pumps (742.6), in dependency order: the point query first, then the ray
  // trace it feeds (the ray intent reads the adopted point document and the display cursor).
  PumpPointQuery(g_state, g_survey_executor, ImGui::GetTime());
  PumpRayTrace(g_state, g_survey_executor);

  // The wait displays' render-feedback fields are THIS frame's outputs: cleared at entry, the
  // render sites are the only writers (a site stores its text iff it drew it this frame), so a
  // field's content is exactly the wait text the user saw (analyze_survey.hpp
  // WaitFeedbackTexts). The status bar's segment field is the status bar's own to clear — that
  // render runs in every frame of either workspace.
  {
    WaitFeedbackTexts& feedback = g_state.analysis.wait_feedback_texts;
    feedback.survey_wait_line.clear();
    feedback.map_wait_line.clear();
    feedback.map_refining_note.clear();
    feedback.point_wait_line_fiber.clear();
    feedback.point_wait_line_details.clear();
    feedback.share_list_busy_dots.clear();
  }

  // The viewport publications are cleared once at the dispatch point (RenderWorkspacePanels);
  // this region publishes by assignment below.

  // Pick-mode Esc cancel — read before any ImGui::Begin so the key event is not consumed by
  // inner widgets first (RenderLeftPanel's own rule; an armed pick is session state, not a
  // Render-panel property, and the mode switch must not orphan it).
  const bool pick_active_at_entry = g_state.pick_link_source.has_value();
  if (pick_active_at_entry && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    g_state.pick_link_source.reset();
  }
  std::optional<GuiState::EntryRef> pick_source_at_entry =
      pick_active_at_entry ? g_state.pick_link_source : std::nullopt;

  const float panel_height = window_height - UiPx(kTopBarHeight) - UiPx(kStatusBarHeight);
  const float top = UiPx(kTopBarHeight);
  const float left_w = UiPx(kAnalyzeLeftPanelWidth);

  // ---- The skeleton's geometry, once (analyze-interaction-contract.md §4.2): every window below
  // consumes these results and none re-derives its own. The stored ratios are the splits'
  // identity; the pixel floors clamp the derivation, never the stored value, so a window brought
  // back to its size brings the user's split back with it. Clamp order, D1 here and D2 the same
  // on its own axis: ① the right column stops at its content floor and the main view absorbs the
  // rest; ② the main view stops at its soft floor and both pin; ③ — only when both floors cannot
  // fit — the floors share the span proportionally and the inner scroll regions take over. ----
  const float strip = UiPx(kAnalyzeSplitterSize);
  const float style_spacing = ImGui::GetStyle().ItemSpacing.x;
  const float avail_w = std::max(0.0f, window_width - left_w);
  const float right_floor = UiPx(kAnalyzeRightColMinWidth);
  const float main_floor = UiPx(kAnalyzeMainViewMinWidth);
  float right_w = g_state.workspace.right_column_ratio * avail_w;
  if (avail_w >= right_floor + main_floor + strip) {
    right_w = std::clamp(right_w, right_floor, avail_w - strip - main_floor);
  } else {
    right_w = std::max(0.0f, (avail_w - strip) * (right_floor / (right_floor + main_floor)));
  }
  const float main_x = left_w;
  const float main_w = std::max(0.0f, avail_w - strip - right_w);
  const float d1_x = main_x + main_w;
  const float right_x = d1_x + strip;

  const float list_floor = UiPx(kAnalyzeShareListMinHeight);
  const float details_floor = UiPx(kAnalyzeDetailsMinHeight);
  float list_h = g_state.workspace.right_list_ratio * panel_height;
  if (panel_height >= list_floor + details_floor + strip) {
    list_h = std::clamp(list_h, list_floor, panel_height - strip - details_floor);
  } else {
    list_h = std::max(0.0f, (panel_height - strip) * (list_floor / (list_floor + details_floor)));
  }
  const float details_y = top + list_h + strip;
  const float details_h = std::max(0.0f, panel_height - strip - list_h);

  // The main view column's own row split — the panorama above, the depth band below; the splitter
  // keeps the field inside [0.15, 0.85] so both rows stay real.
  const float upper_h = panel_height * g_state.workspace.middle_rows_ratio;
  const float band_y = top + upper_h + strip;
  const float band_h = std::max(0.0f, panel_height - upper_h - strip);

  // The depth band's form — the transition's responsive two-shape rule: side-by-side when the
  // band can give each of the three regions its content floor (widths), stacked — each region the
  // full band width at its floor height, the band scrolling — when it cannot. The one-bit latch
  // is the hysteresis a resize at the boundary needs; it is a per-frame derivation's memory (a
  // per-process layout fact, not session state: each frame re-derives the form from the band's
  // width, the bit only chooses which side of the hysteresis band applies).
  static bool band_side_by_side = false;
  {
    const float cells_floor =
        UiPx(kAnalyzeDepthMapMinSize + kAnalyzeDepth3DMinSize + kAnalyzeDepthGlobeMinSize) + style_spacing * 2.0f;
    if (!band_side_by_side && main_w >= cells_floor + UiPx(kAnalyzeDepthBandFormHysteresisPt)) {
      band_side_by_side = true;
    } else if (band_side_by_side && main_w < cells_floor) {
      band_side_by_side = false;
    }
  }

  // ---- Left column: crystal list + Sun + Simulation (the Render right panel's Scene controls,
  // the very same function). One scrolling column, like the prototype stacks them. ----
  SetNextPanelGeometry(0.0f, top, left_w, panel_height);
  ImGui::Begin("##AnalyzeLeftPanel", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
  {
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
    ReopenEditorOnPickCompletion(g_state, pick_source_at_entry);
    ImGui::SeparatorText("Scene");
    RenderSceneControls(g_state);
  }
  ImGui::End();

  // ---- Main view: the committed picture, as large as the column's upper row gives it. The
  // panorama's mechanics are unchanged — NoBackground (the deferred pass blits the reprojected
  // sky into this window's rect BEFORE the ImGui draw data renders; the chrome window's own
  // background fill would paint straight over it, the same flag ##PreviewPanel carries), the
  // delayed blit, the g_panorama_vp publication — only the rectangle grows. ----
  SetNextPanelGeometry(main_x, top, main_w, upper_h);
  ImGui::Begin("##AnalyzePanorama", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);
  RenderScenePanorama(window, window_height);
  ImGui::End();

  // The row divider: a thin strip across the main view column, dragging the panorama/band ratio
  // (its line is the only affordance — dim normally, themed on hover/grab).
  SetNextPanelGeometry(main_x, top + upper_h, main_w, strip);
  // Zero padding AND zero minimum size: the strip is exactly as thick as its grab bar. With the
  // inherited (8,6) padding the bar rode below the strip's own rectangle (a free-floating line,
  // and a press on the bar's centre landing in the lower row's hover territory); with the
  // style's WindowMinSize (32pt, which ImGui applies to every top-level window regardless of a
  // SetNextWindowSize) the strip came out 32pt thick and its opaque background covered the first
  // rows of both regions beside it — the cut-off header row — while stealing their hover. No
  // scrollbar: an 8pt strip with nothing to scroll has no business offering one.
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0.0f, 0.0f));
  ImGui::Begin("##AnalyzeRowsDivider", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);
  MiddleRowsSplitter("##AnalyzeRowsSplitter", main_w, &g_state.workspace.middle_rows_ratio, top, panel_height);
  ImGui::End();
  ImGui::PopStyleVar(2);

  // ---- The depth band: the map / 3D / globe regions' transition host, until their windows take
  // them (755.4/.5) and the band retires with this split. The region functions own their content
  // (each opens its own child); only the geometry they are handed differs by form. Side-by-side:
  // map and globe at their content floors, the 3D view absorbing what is left. Stacked: each the
  // full band width at its floor height — the band itself is the one scrolling region then, so
  // the scrollbars exist only in the form that needs them. ----
  SetNextPanelGeometry(main_x, band_y, main_w, band_h);
  ImGui::Begin("##AnalyzeLowerRow", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
                   (band_side_by_side ? (ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse) : 0));
  {
    const float band_inner_w = ImGui::GetContentRegionAvail().x;
    if (band_side_by_side) {
      const float cell_h = ImGui::GetContentRegionAvail().y;
      const float map_w = UiPx(kAnalyzeDepthMapMinSize);
      const float globe_w = UiPx(kAnalyzeDepthGlobeMinSize);
      const float three_d_w =
          std::max(UiPx(kAnalyzeDepth3DMinSize), band_inner_w - map_w - globe_w - style_spacing * 2.0f);
      RenderAnalyzeMapRegion(g_state, ImVec2(map_w, cell_h));
      ImGui::SameLine();
      RenderAnalyzeRay3DRegion(g_state, ImVec2(three_d_w, cell_h));
      ImGui::SameLine();
      RenderAnalyzeSunGlobeRegion(g_state, ImVec2(globe_w, cell_h));
    } else {
      RenderAnalyzeMapRegion(g_state, ImVec2(band_inner_w, UiPx(kAnalyzeDepthMapMinSize)));
      ImGui::Spacing();
      RenderAnalyzeRay3DRegion(g_state, ImVec2(band_inner_w, UiPx(kAnalyzeDepth3DMinSize)));
      ImGui::Spacing();
      RenderAnalyzeSunGlobeRegion(g_state, ImVec2(band_inner_w, UiPx(kAnalyzeDepthGlobeMinSize)));
    }
  }
  ImGui::End();

  // D1: the vertical strip between the main view column and the right column, dragging the
  // column ratio. The same zero-padding / zero-minimum recipe as the row divider above.
  SetNextPanelGeometry(d1_x, top, strip, panel_height);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0.0f, 0.0f));
  ImGui::Begin("##AnalyzeColumnDivider", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);
  RightColumnSplitter("##AnalyzeColumnSplitter", panel_height, &g_state.workspace.right_column_ratio, main_x, avail_w);
  ImGui::End();
  ImGui::PopStyleVar(2);

  // ---- Right column, upper: the share list. The window itself never scrolls (the same flag
  // pair it carried in the middle area): its one scrolling region is the result table's own inner
  // window, so the controls above it are a fixed header and stay reachable at every height. ----
  SetNextPanelGeometry(right_x, top, right_w, list_h);
  ImGui::Begin("##AnalyzeShareList", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);
  RenderAnalysisPanelBody(g_state, g_server);
  ImGui::End();

  // D2: the right column's horizontal strip, dragging the list/diagnostics ratio.
  SetNextPanelGeometry(right_x, top + list_h, right_w, strip);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0.0f, 0.0f));
  ImGui::Begin("##AnalyzeRightRowsDivider", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);
  RightRowsSplitter("##AnalyzeRightRowsSplitter", right_w, &g_state.workspace.right_list_ratio, top, panel_height);
  ImGui::End();
  ImGui::PopStyleVar(2);

  // ---- Right column, lower: the diagnostics region — the selection's identity facts, the
  // survey's four visible states and the point block when a class is selected; the idle state's
  // entry guidance, run status and observation summary when none is (§9.1). The window itself
  // never scrolls: the query content is its one scrolling region, the identity facts its fixed
  // head — the same fixed-header/inner-scroll convention the share list carries. ----
  SetNextPanelGeometry(right_x, details_y, right_w, details_h);
  ImGui::Begin("##AnalyzeDetails", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);
  if (const std::optional<gui::SelectedClassIdentity> identity = gui::SelectedClassIdentityFor(g_state);
      identity.has_value()) {
    // R5/R7 (742.3 M3-1): the selection's identity facts — the row's own text, the snapshot's
    // crystal, the layer/face shape — and, for a multi-layer class, the survey face's documented
    // answer, named. Below them, the survey query's four visible states (R7): the
    // engine-answered digest when one was adopted, the named refusal where the engine says
    // no, waiting and cancelled spelled as such. The point-level block (742.6) follows as the
    // second level of the two-level information contract (D6).
    ImGui::TextUnformatted("Selection details");
    ImGui::BulletText("%s", g_state.analysis.selected_entry->c_str());
    ImGui::BulletText("snapshot crystal %d · %d layer(s) · %d faces", identity->core_crystal_id,
                      static_cast<int>(identity->layer_face_counts.size()), static_cast<int>(identity->faces.size()));
    if (identity->layer_face_counts.size() > 1) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
      ImGui::TextWrapped("The typed survey refuses multi-layer classes: multi_layer_unsupported.");
      ImGui::PopStyleColor();
    }
    ImGui::BeginChild("##AnalyzeDetailsScroll", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    ImGui::Separator();
    RenderSurveyOverview(*identity);
    RenderPointDetailsBlock(g_state, *identity);
    ImGui::EndChild();
  } else {
    RenderAnalysisIdleDiagnostics(g_state);
  }
  ImGui::End();
}

void RenderWorkspacePanels(GLFWwindow* window, float window_width, float window_height) {
  // The frame's viewport publications, cleared once at the ONE dispatch point. Both halves of
  // each publication are level-triggered (PreviewViewport in app.hpp), so the frame that does
  // not draw a region is the frame that publishes neither its viewport nor its curve labels —
  // stated here, once, instead of as a copy of "clear the other mode's publication" in every
  // region entry. That scatter is what let a stale panorama `true` blit over Render frames
  // once already (M2-4's six red cases).
  g_preview_vp.active = false;
  g_preview_vp.curve_labels.clear();
  g_panorama_vp.active = false;

  if (g_state.workspace.mode == GuiState::WorkspaceMode::kAnalyze) {
    RenderAnalyzeWorkspace(window, window_width, window_height);
  } else {
    RenderLeftPanel(window_height);
    RenderRightPanel(window, window_width, window_height);
    RenderPreviewPanel(window, window_width, window_height);
  }
}

}  // namespace lumice::gui
