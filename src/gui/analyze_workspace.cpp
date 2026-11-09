#include "gui/analyze_workspace.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>

#include "IconsFontAwesome6.h"
#include "gui/analysis_panel.hpp"
#include "gui/analyze_map_renderer.hpp"
#include "gui/analyze_map_view.hpp"
#include "gui/analyze_overview.hpp"
#include "gui/analyze_point_view.hpp"
#include "gui/analyze_region_wheel.hpp"
#include "gui/analyze_workspace.hpp"
#include "gui/app.hpp"
#include "gui/destructive_style.hpp"
#include "gui/gui_constants.hpp"
#include "gui/panels.hpp"
#include "gui/panorama_viewport.hpp"
#include "gui/semantic_colors.hpp"
#include "gui/sim_state_rules.hpp"
#include "gui/theme.hpp"
#include "gui/view_look_at.hpp"  // UpdateLockedLookAt — the camera reconciliation the picture is built from
#include "imgui.h"
#include "util/raypath_analysis_display.hpp"  // RoiModeLabel — the diagnostics summary's region line
#include "util/sky_direction.hpp"             // DirToAltAz — the diagnostics summary's centre line
#include "util/thousands_format.hpp"          // FormatThousands — the diagnostics run-status line

namespace lumice::gui {
namespace {

// The diagnostics region's idle state (analyze-interaction-contract.md §9.1 — a first-class
// design, not an empty placeholder): the entry guidance (the list's own empty-state line, plus
// the next step — no picture yet points at the top bar's Run, a picture points at the
// main view's operation bar: region there, Analyze there), the analysis's own run status while
// one is in flight (the same state machine and Stop the bar's controls drive — two faces of one
// truth, the contract's cancel entry for this region), and the observation summary (the request
// as it stands, for the "edit the scene, compare again" pass). Everything is a display read of
// fields another region already renders or the session already holds: no new query, no new
// state owner.
void RenderAnalysisIdleDiagnostics(GuiState& state) {
  const auto& a = state.analysis;
  // (The "Diagnostics" title is the window's fixed head, drawn by the caller — this function is
  // the scrolling region's content.)
  // The region's text wraps, never clips: ImGui's BulletText/Text/TextDisabled size a single
  // line and clip at the region's right edge — exactly where a sentence gets interesting at the
  // right column's 260 pt floor (analyze_overview.cpp's conclusions learned this first). The
  // bullet+wrapped pair is BulletText's wrapping shape; DisabledWrapped here is the color pair
  // by hand (ImGui has no DisabledTextWrapped).
  const auto disabled_wrapped = [](const char* fmt, auto... args) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped(fmt, args...);
    ImGui::PopStyleColor();
  };
  const auto disabled_bullet_wrapped = [](const char* fmt, auto... args) {
    ImGui::Bullet();
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped(fmt, args...);
    ImGui::PopStyleColor();
  };
  // ---- The entry guidance: where an analysis starts from, as the state actually is. ----
  if (state.analysis_result.payload == nullptr) {
    // The list's own empty-state line (RenderResultList's, one source for the same fact).
    disabled_wrapped("No result yet. Choose a region and press Analyze.");
  }
  // The same branch logic as the panorama's empty state: without a picture of the document the
  // first step is the top bar's Run; with one, the main view's operation bar is the
  // path — the guidance names the real places (the bar below the picture, both region and
  // Analyze button on it).
  // Each branch is its own named child (the pick banner's reason: a text item has no id a test
  // can address; a window does), so a case can pin which guidance is on screen.
  if (!HasEverShownPicture(state.run_intent)) {
    ImGui::BeginChild("##AnalyzeIdleNoPictureGuide", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AutoResizeY);
    ImGui::TextWrapped(
        "No rendered image for this document yet — press Run in the top bar; "
        "an analysis needs a picture of the document to aim at.");
    ImGui::EndChild();
  } else {
    ImGui::BeginChild("##AnalyzeIdlePictureGuide", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AutoResizeY);
    ImGui::TextWrapped("Choose a region on the bar below the picture, then press Analyze there.");
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
    ImGui::PushStyleColor(ImGuiCol_Text, AccentColor());
    ImGui::TextWrapped("Analyzing (%s)... %s rays", RoiModeLabel(a.roi_mode),
                       FormatThousands(static_cast<std::uint64_t>(rays)).c_str());
    ImGui::PopStyleColor();
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
  disabled_bullet_wrapped("Region: %s", RoiModeLabel(a.roi_mode));
  if (a.roi_mode == LUMICE_RAYPATH_ROI_CONE) {
    if (a.cone_center_valid) {
      float alt = 0.0f;
      float az = 0.0f;
      DirToAltAz(a.cone_center_dir, &alt, &az);
      disabled_bullet_wrapped("Cone centre: altitude %.1f deg, azimuth %.1f deg", alt, az);
    } else {
      disabled_bullet_wrapped("Cone centre: not set");
    }
  }
  if (a.infinite) {
    disabled_bullet_wrapped("Analysis budget: infinite rays");
  } else {
    disabled_bullet_wrapped("Analysis budget: %.1fM rays", a.ray_num_millions);
  }
  if (state.sim_state == GuiState::SimState::kModified) {
    ImGui::PushStyleColor(ImGuiCol_Text, WarningTextColor());
    ImGui::TextWrapped("Document: modified — the picture predates the edit.");
    ImGui::PopStyleColor();
  } else {
    disabled_bullet_wrapped("Document: up to date");
  }
  ImGui::EndChild();
}

// D1 (main view | right column), the row splitter's vertical counterpart — the same recipe
// (invisible button + hover/active line + the one affordance line + delta-only write), with the
// resize-EW cursor. Dragging the hand right narrows the right column: the ratio (the right
// column's share) decreases by the travel. The pixel-floor clamp (gui_constants.hpp) pins the
// write at the floors of this window width — the layout re-derives the pixels every frame, the
// stored ratio stays the split's identity. `area_w` is a size, not a position: sizes survive the
// viewport-origin translation the splitter must not read (see MiddleRowsSplitter).
void RightColumnSplitter(const char* id, float height, float* ratio, float area_w) {
  const float width = UiPx(kAnalyzeSplitterSize);
  ImGui::InvisibleButton(id, ImVec2(width, height));
  const bool hovered = ImGui::IsItemHovered();
  const bool active = ImGui::IsItemActive();
  const float mouse_dx = ImGui::GetIO().MouseDelta.x;
  if (active && mouse_dx != 0.0f && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    // Delta from the displayed boundary (see MiddleRowsSplitter): at a window whose layout is
    // clamped away from the stored ratio, a raw write makes the first stretch of the drag dead
    // while the strip stands still. The clamp bounds only exist in the normal tier — when the
    // two floors cannot both fit, lower > upper and std::clamp's precondition is violated
    // (libc++ hardening aborts) — so the tiers branch BEFORE any clamp: the compressed tier
    // writes its fixed split directly, the normal tier computes raw inside its ordered bounds.
    const float right_floor = UiPx(kAnalyzeRightColMinWidth);
    const float main_floor = UiPx(kAnalyzeMainViewMinWidth);
    if (area_w >= right_floor + main_floor + width) {
      const float lo = right_floor / area_w;
      const float hi = (area_w - width - main_floor) / area_w;
      *ratio = std::clamp(std::clamp(*ratio, lo, hi) - mouse_dx / area_w, lo, hi);
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
// delta-only write and the pixel-floor clamp: the list keeps its fixed head plus the table's
// header row, one data row and the button row; the diagnostics region keeps its title and a
// visible first line of its scrolling region.
void RightRowsSplitter(const char* id, float width, float* ratio, float area_h) {
  const float height = UiPx(kAnalyzeSplitterSize);
  ImGui::InvisibleButton(id, ImVec2(width, height));
  const bool hovered = ImGui::IsItemHovered();
  const bool active = ImGui::IsItemActive();
  const float mouse_dy = ImGui::GetIO().MouseDelta.y;
  if (active && mouse_dy != 0.0f && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    // Delta from the displayed boundary (see MiddleRowsSplitter); tiers branch before any
    // clamp — the compressed tier's bounds are inverted and std::clamp's precondition would
    // be violated (see RightColumnSplitter).
    const float list_floor = UiPx(kAnalyzeShareListMinHeight);
    const float details_floor = UiPx(kAnalyzeDetailsMinHeight);
    if (area_h >= list_floor + details_floor + height) {
      const float lo = list_floor / area_h;
      const float hi = (area_h - height - details_floor) / area_h;
      *ratio = std::clamp(std::clamp(*ratio, lo, hi) + mouse_dy / area_h, lo, hi);
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

// The picture region proper — the child the deferred pass blits into and the one hosting its
// gestures (the cone ROI's two analysis arms, the camera orbit as the third, the Angular
// Distance pick), at the height the caller's operation-bar reserve leaves it. Split out of
// RenderScenePanorama so the empty state and the bar bracket it without re-indenting the
// gesture block.
void RenderAnalyzeMainPicture(GLFWwindow* window, float picture_h) {
  // A non-positive height is the caller's bar-takeover state — never submitted, because a
  // zero-height BeginChild would fill what is left instead of taking none.
  if (picture_h <= 0.0f) {
    return;
  }
  // The picture region: transparent, because the deferred pass blits the reprojected texture
  // under ImGui exactly as it does for the Render preview panel. Everything the region hosts —
  // the display-mode control, the publication, the shared assembly, the interaction button and
  // every gesture — lives inside this child, the way the preview panel's own window hosts them.
  const ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x, picture_h);
  ImGui::BeginChild("##AnalyzePanoramaPicture", size, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoBackground);
  const ImVec2 region_min = ImGui::GetWindowPos();
  if (!g_preview.HasTexture()) {
    ImGui::TextWrapped(
        "No rendered image for this document yet — press Run in the top bar; "
        "the main view shows the traced sky here.");
    ImGui::EndChild();
    return;
  }

  // Submitted ahead of `##panorama_interact` on purpose — RenderDisplayModeControl's own
  // comment carries the press-order rule.
  const bool over_display_mode_control = RenderDisplayModeControl();

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
  // The picture itself: the shared assembly — the real BuildPreviewViewProjFromRenderer view,
  // the same mono exposure and composite push, overlays, markers, labels and photo state the
  // preview publishes. Assembled BEFORE the gestures so a drag frame's picture is the pre-drag
  // frame's — the order the preview panel runs in (the drag lands on the next frame's picture,
  // and every face behaves the same way).
  PreviewPictureTarget target;
  target.canvas_w_pt = size.x;
  target.canvas_h_pt = size.y;
  target.vp_w_px = g_panorama_vp.vp_w;
  target.vp_h_px = g_panorama_vp.vp_h;
  AssemblePreviewPictureParams(g_state, g_panorama_vp.params, g_panorama_vp.curve_labels, target,
                               g_panorama_vp.anchors);

  ImGui::InvisibleButton("##panorama_interact", ImGui::GetContentRegionAvail());

  // ---- The picture region's gestures ----
  const ImGuiIO& io = ImGui::GetIO();
  const bool interact_hovered = ImGui::IsItemHovered() && !over_display_mode_control;
  const bool interact_active = ImGui::IsItemActive();
  const bool region_hovered =
      ImGui::IsMouseHoveringRect(region_min, ImVec2(region_min.x + size.x, region_min.y + size.y));
  auto& a = g_state.analysis;
  // The camera half is disabled for lenses in kFullSkyLensTypes (dual fisheye 4-6, rectangular
  // 7, dual orthographic 9): their shader path skips the view matrix so view angles + FOV have
  // no visual effect — the same gate the preview panel's camera arm carries.
  const bool full_sky = LensIsFullSky(g_state.renderer.lens_type);
  // An armed Angular Distance pick — or the swallow latch a completed pick still holds — owns
  // the press outright, the Render host's gestures_locked rule carried to its second host: no
  // camera or ROI arm may start while it lasts, and a marker/camera latch taken before the
  // arming keeps the press it already owns. Without this, one press would be consumed twice —
  // the camera orbiting through the very click that set a circle's angle.
  const bool angular_pick_owns =
      g_state.angular_dist_picker.armed || g_state.angular_dist_picker.swallow_drag_until_release;
  const bool explicit_pick_owns = angular_pick_owns || g_bg_pick.active || g_bg_pick.swallow_drag_until_release;

  if (a.roi_mode == LUMICE_RAYPATH_ROI_CONE) {
    // The annotation view the picture was drawn with — the pick family's one view (both
    // picture faces publish the same shared assembly, so PreviewAnnotationView IS the picture's
    // view here as it is in the preview).
    const LUMICE_AnnotationView view = PreviewAnnotationView(g_state, g_panorama_vp.vp_w, g_panorama_vp.vp_h);
    // A centre when there is none — or a re-place of the DEFAULT one during the placement's own
    // bounded convergence (the operation bar's height settles a frame or two after the mode
    // switch that grows it; kAnalyzeConeCenterConvergenceFrames bounds the window). After the
    // window, the direction is the region's: no layout, view or observation change re-derives
    // it — the invariant is the placement lifecycle's, not an enumeration of the entries that
    // might otherwise move it. An acceptance (DoAnalyze) or a camera press freezes ahead of the
    // window's expiry as well; a user-picked or user-dragged centre is never re-placed by
    // anything but the user.
    const auto& analysis = g_state.analysis;
    const int frames_since_placement = analysis.cone_center_default_frame >= 0 ?
                                           ImGui::GetFrameCount() - analysis.cone_center_default_frame :
                                           kAnalyzeConeCenterConvergenceFrames + 1;
    const bool re_place_window_open =
        frames_since_placement <= kAnalyzeConeCenterConvergenceFrames && analysis.cone_center_is_default;
    const bool needs_default = !analysis.cone_center_valid ||
                               (re_place_window_open && (analysis.cone_center_default_vp_w != g_panorama_vp.vp_w ||
                                                         analysis.cone_center_default_vp_h != g_panorama_vp.vp_h));
    if (needs_default) {
      EnsureDefaultConeCenter(g_state, view, g_panorama_vp.vp_w, g_panorama_vp.vp_h);
    }
    bool marker_hover = false;
    if (interact_hovered) {
      // The hover/press surface for the ROI arms is the interaction button's — occlusion-aware
      // and excluding the display-mode control submitted above it — not the region rectangle:
      // a press on a control is the control's, never a pick or a marker grab underneath it.
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
    if ((marker_hover ||
         g_state.workspace.panorama_press_owner == GuiState::AnalyzeWorkspaceSession::PanoramaPressOwner::kMarker) &&
        !angular_pick_owns) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      ImGui::SetTooltip("Drag the marker to move the centre.\n%s+click on the picture to set it directly.",
                        kBgModifierName);
    }
    DrawPanoramaRoiRing(g_state, view, region_min, g_panorama_vp.dpi_scale_x, g_panorama_vp.dpi_scale_y);

    // Only the mouse-down chooses an arm. Neither a modifier change nor crossing the marker
    // can reclassify a held press. Even a direct-point miss owns its press until release.
    if (interact_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        g_state.workspace.panorama_press_owner == GuiState::AnalyzeWorkspaceSession::PanoramaPressOwner::kNone) {
      using Owner = GuiState::AnalyzeWorkspaceSession::PanoramaPressOwner;
      auto& owner = g_state.workspace.panorama_press_owner;
      owner = explicit_pick_owns ? Owner::kExplicitPick :
              io.KeyAlt          ? Owner::kDirectPoint :
              marker_hover       ? Owner::kMarker :
                                   Owner::kCamera;
      if (owner == Owner::kCamera) {
        a.cone_center_is_default = false;
      }
      if (owner == Owner::kDirectPoint) {
        if (const auto px = PreviewPointToCanvasPixel(io.MousePos.x - region_min.x, io.MousePos.y - region_min.y,
                                                      g_panorama_vp.dpi_scale_x, g_panorama_vp.dpi_scale_y,
                                                      g_panorama_vp.vp_w, g_panorama_vp.vp_h)) {
          PickAnalysisConeCenter(g_state, view, px->px, px->py);
        }
      }
    }
    using Owner = GuiState::AnalyzeWorkspaceSession::PanoramaPressOwner;
    if (g_state.workspace.panorama_press_owner == Owner::kMarker && interact_active && region_hovered &&
        ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      if (const auto px = PreviewPointToCanvasPixel(io.MousePos.x - region_min.x, io.MousePos.y - region_min.y,
                                                    g_panorama_vp.dpi_scale_x, g_panorama_vp.dpi_scale_y,
                                                    g_panorama_vp.vp_w, g_panorama_vp.vp_h)) {
        DragAnalysisConeCenter(g_state, view, px->px, px->py);
      }
    }
  } else if (interact_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    using Owner = GuiState::AnalyzeWorkspaceSession::PanoramaPressOwner;
    if (g_state.workspace.panorama_press_owner == Owner::kNone) {
      g_state.workspace.panorama_press_owner = explicit_pick_owns ? Owner::kExplicitPick : Owner::kCamera;
      a.cone_center_is_default = false;
    }
  }

  using Owner = GuiState::AnalyzeWorkspaceSession::PanoramaPressOwner;
  const Owner owner = g_state.workspace.panorama_press_owner;
  const bool wheel_standing = AnalyzeRegionWheelStanding();
  AnalyzeRegionWheelClaim(interact_hovered, MainPictureWheelOwnerId());
  if (wheel_standing && interact_hovered && !angular_pick_owns && !g_bg_pick.active &&
      !g_bg_pick.swallow_drag_until_release && (owner == Owner::kNone || owner == Owner::kCamera) &&
      io.MouseWheel != 0.0f) {
    ApplyCameraWheelZoom(g_state, io.MouseWheel);
    a.cone_center_is_default = false;
  }
  if (owner == Owner::kCamera && !full_sky && interact_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    ApplyCameraOrbitDrag(g_state, io.MouseDelta, g_panorama_vp.vp_w, g_panorama_vp.vp_h);
  }

  // A later arming cannot steal a camera/marker/direct press. With no press, the explicit
  // picker still previews its readout; after mouse-down it alone can consume that press.
  if (g_state.angular_dist_picker.armed && !g_bg_pick.active && interact_hovered &&
      (owner == Owner::kExplicitPick || (owner == Owner::kNone && !ImGui::IsMouseDown(ImGuiMouseButton_Left)))) {
    ApplyAngularDistPickGesture(
        g_state, g_panorama_vp.params, ImVec2(io.MousePos.x - region_min.x, io.MousePos.y - region_min.y),
        g_panorama_vp.dpi_scale_x, g_panorama_vp.dpi_scale_y, g_panorama_vp.vp_w, g_panorama_vp.vp_h);
  }

  ImGui::EndChild();
}

// The main view: the committed texture through the REAL renderer view — the same lens, FOV,
// pose, exposure, overlays, markers, labels, display mode and background-photo state the Render
// preview shows (one display-state source; analyze-interaction-contract.md §5.4), published
// through the shared picture assembly (AssemblePreviewPictureParams). The picture itself is
// drawn by the deferred pass (RenderPreviewFrameAndBlit into the region this publishes to
// g_panorama_vp); this side draws the state around it and hosts the picture region's gestures
// (RenderAnalyzeMainPicture). Below the picture, the operation bar — the analysis request's
// controls next to what they aim at (contract §2.2).
void RenderScenePanorama(GLFWwindow* window) {
  // The publication is cleared once per frame at the dispatch point; this function publishes
  // by assignment below.

  // Camera reconciliation and the renderer's view invariants, before this frame's picture is
  // assembled — the same order the preview panel runs in: the left column's scene and
  // observation edits above rendered first, so the same frame's picture is built from the
  // reconciled view, and a Look At lock or a Sun edit lands on the frame it happens.
  UpdateLockedLookAt(g_state);
  ClampRendererViewInvariants(g_state.renderer);

  const ImVec2 avail = ImGui::GetContentRegionAvail();
  const float gap = ImGui::GetStyle().ItemSpacing.y;
  const float bar_h = AnalyzeOperationBarHeight(avail.x);
  const float status_h = ImGui::GetTextLineHeight() * 2.0f + gap;
  const float picture_h = std::max(1.0f, avail.y - bar_h - status_h - gap * 2.0f);
  RenderAnalyzeMainPicture(window, picture_h);
  ImGui::BeginChild("##AnalyzePictureStatus", ImVec2(0.0f, status_h));
  RenderAnalyzeStatus(g_state, g_server);
  ImGui::EndChild();
  // Only genuinely compressed geometry scrolls controls. The normal two templates render flat,
  // preserving the controls' existing IDs; query state never enters this decision.
  if (AnalyzeOperationBarNeedsScroll(avail.x) || picture_h + status_h + bar_h + gap * 2.0f > avail.y) {
    ImGui::BeginChild("##AnalyzeOpBarScroll", ImVec2(0.0f, std::max(1.0f, bar_h)));
    RenderAnalyzeOperationBar(g_state, g_server);
    ImGui::EndChild();
  } else {
    RenderAnalyzeOperationBar(g_state, g_server);
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
      ImGui::TextWrapped("%s", wait_line.c_str());
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

void PumpAnalyzeQueries(GuiState& state, SurveyExecutor& executor, double now_s, bool prepare_map_sources) {
  // The point face's pumps run in dependency order: the point query first, then the ray trace
  // it feeds (the ray intent reads the adopted point document and the display cursor).
  PumpSurveyQuery(state, executor, now_s);
  PumpMapQuery(state, executor, now_s, prepare_map_sources ? PrepareFrozenMapSource : nullptr);
  PumpPointQuery(state, executor, now_s);
  PumpRayTrace(state, executor);
}

void RenderAnalyzeWorkspace(GLFWwindow* window, float window_width, float window_height) {
  // The background queries' frame pumps run at RenderWorkspacePanels' entry since 755.5 — every
  // GUI frame drains the mailboxes, applies the adoption gates and keeps the intents
  // level-triggered, before any region reads the sessions (this entry and the two analysis
  // windows all render after that call, so the overview, the map and the point faces on screen
  // are always this frame's state). In-flight queries keep running across a workspace switch
  // (R6 — switching away neither cancels nor cleans) and their outcomes adopt through the gate
  // no matter which window is open — the pump no longer depends on any window drawing.

  // The wait displays' render-feedback fields are THIS frame's outputs: cleared at entry, the
  // render sites are the only writers (a site stores its text iff it drew it this frame), so a
  // field's content is exactly the wait text the user saw (analyze_survey.hpp
  // WaitFeedbackTexts). The map fields' render site is the Raypath view window now — on the
  // Render-workspace frames where this entry does not run, the window clears its own two, and
  // the Pose & trace window clears the fiber field the same way; the workspace entry still
  // clears them here because on an Analyze frame where either window is closed nothing else
  // will. The status bar's segment field is the status bar's own to clear — that render runs in
  // every frame of either workspace.
  {
    WaitFeedbackTexts& feedback = g_state.analysis.wait_feedback_texts;
    feedback.survey_wait_line.clear();
    feedback.map_wait_line.clear();
    feedback.map_superseding_note.clear();
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
  // The Angular Distance pick is hosted over both workspaces' pictures; this side's two exits
  // are Esc and the committed texture going away under it.
  if (g_state.angular_dist_picker.armed && (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || !g_preview.HasTexture())) {
    g_state.angular_dist_picker.armed = false;
  }
  if (g_state.angular_dist_picker.swallow_drag_until_release && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    g_state.angular_dist_picker.swallow_drag_until_release = false;
  }

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
    // Scene editing, in place (analyze-interaction-contract.md §3): the Sun group expanded —
    // the same controls Render's right panel hosts — and Simulation behind a DEFAULT-COLLAPSED
    // fold, the render budget staying an in-place entry without flattening the column (its
    // edits still ride the explicit Run contract: kModified, no automatic re-run).
    RenderSunControls(g_state);
    if (ImGui::CollapsingHeader("Simulation")) {
      RenderSimulationControls(g_state);
    }
    // The observation groups — the Render right panel's View / Display / Overlay bodies through
    // the same functions on the same fields (§3's display tier shares one source with Render).
    // Lens and overlays are the everyday groups — expanded; brightness/colour folds. The folds
    // are ImGui window storage, so a fold's open state survives within the session for free.
    if (ImGui::CollapsingHeader("View", ImGuiTreeNodeFlags_DefaultOpen)) {
      RenderViewControlsGroup(g_state);
    }
    if (ImGui::CollapsingHeader("Display")) {
      RenderDisplayTimeControlsGroup(g_state, /*host_bg_pick=*/false);
    }
    if (ImGui::CollapsingHeader("Overlay", ImGuiTreeNodeFlags_DefaultOpen)) {
      RenderOverlayControlsGroup(g_state);
    }
  }
  ImGui::End();

  // ---- Main view: the committed picture, filling the whole main column (755.5: the depth
  // band retired — its two regions are the Pose & trace window's now, and the panorama/band
  // row split retired with it). The panorama's mechanics are unchanged — NoBackground (the
  // deferred pass blits the reprojected sky into this window's rect BEFORE the ImGui draw data
  // renders; the chrome window's own background fill would paint straight over it, the same
  // flag ##PreviewPanel carries), the delayed blit, the g_panorama_vp publication — only the
  // rectangle is the full column now. ----
  SetNextPanelGeometry(main_x, top, main_w, panel_height);
  ImGui::Begin("##AnalyzePanorama", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus);
  RenderScenePanorama(window);
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
  RightColumnSplitter("##AnalyzeColumnSplitter", ImGui::GetContentRegionAvail().y,
                      &g_state.workspace.right_column_ratio, avail_w);
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
  // (Grab spans the strip's real inner width — the same floored-size reason as the row divider.)
  RightRowsSplitter("##AnalyzeRightRowsSplitter", ImGui::GetContentRegionAvail().x, &g_state.workspace.right_list_ratio,
                    panel_height);
  ImGui::End();
  ImGui::PopStyleVar(2);

  // ---- Right column, lower: the diagnostics region — the selection's identity facts, the
  // survey's four visible states and the point block when a class is selected; the idle state's
  // entry guidance, run status and observation summary when none is (§9.1). The window itself
  // never scrolls: the query content is its one scrolling region, the identity facts its fixed
  // head — the window's own head is exactly one line (the title), the region's whole D2-floor
  // budget promise: everything else — identity facts, query states, the idle guidance / run
  // status / observation summary — grows inside the ONE scrolling region, so the smallest
  // permitted diagnostics height still shows the title and a visible first line, and the wheel
  // reaches the rest. ----
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
    // second level of the two-level information contract (D6). The identity facts scroll WITH
    // the query content: at the D2 floor a multi-row fixed head would push the query region's
    // own start below the clip, and the facts are information, not controls — nothing here
    // needs to stay under the hand.
    ImGui::TextUnformatted("Selection details");
    ImGui::BeginChild("##AnalyzeDetailsScroll", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    // Identity facts wrap like the region's other text (BulletText is a single clipped line).
    ImGui::Bullet();
    ImGui::TextWrapped("%s", g_state.analysis.selected_entry->c_str());
    ImGui::Bullet();
    ImGui::TextWrapped("snapshot crystal %d · %d layer(s) · %d faces", identity->core_crystal_id,
                       static_cast<int>(identity->layer_face_counts.size()), static_cast<int>(identity->faces.size()));
    if (identity->layer_face_counts.size() > 1) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
      ImGui::TextWrapped("The typed survey refuses multi-layer classes: multi_layer_unsupported.");
      ImGui::PopStyleColor();
    }
    ImGui::Separator();
    RenderSurveyOverview(*identity);
    RenderPointDetailsBlock(g_state, *identity);
    ImGui::EndChild();
  } else {
    ImGui::TextUnformatted("Diagnostics");
    ImGui::BeginChild("##AnalyzeDetailsScroll", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    RenderAnalysisIdleDiagnostics(g_state);
    ImGui::EndChild();
  }
  ImGui::End();
}

void RenderWorkspacePanels(GLFWwindow* window, float window_width, float window_height) {
  // The analysis queries' frame pump, hoisted to the ONE entry every GUI frame passes through
  // (755.5): both workspaces' frames — and the frames where no analysis face renders at all
  // (Render mode with both analysis windows closed) — drain the four mailboxes, apply the
  // adoption gates and keep the intents level-triggered. An in-flight query's completion is
  // therefore adopted regardless of which window is on screen (the AC the two window-hosted
  // pump calls could not carry: a face-less frame never reached a pump, so a query kept running
  // but its answer sat unadopted until a face rendered again — the hidden-wait hang). The pump
  // is per-frame idempotent (drain clears its slot; a key match re-asks nothing), so running it
  // here instead of in the consumers changes no consumer-visible behavior; the wait-feedback
  // fields stay owned by their render sites (cleared by the site that can draw the field, not
  // by the pump). Cost: a drain-and-compare pass over four sessions on every frame — the same
  // order of work the Analyze frames have always paid, and empty on the Render frames where no
  // intent is outstanding.
  // Snapshot before any face claims the key: native dispatch already ran in NewFrame.
  AnalyzeRegionWheelStanding();
  using Owner = GuiState::AnalyzeWorkspaceSession::PanoramaPressOwner;
  auto& press_owner = g_state.workspace.panorama_press_owner;
  if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    press_owner = Owner::kNone;
  } else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && press_owner != Owner::kNone) {
    press_owner = Owner::kCancelled;
  }
  auto& map_owner = g_state.workspace.map_press_owner;
  if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    map_owner = Owner::kNone;
  } else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && map_owner != Owner::kNone) {
    map_owner = Owner::kCancelled;
  }
  PumpAnalyzeQueries(g_state, g_survey_executor, ImGui::GetTime(), /*prepare_map_sources=*/true);

  // The frame's viewport publications, cleared once at the ONE dispatch point. Both halves of
  // each publication are level-triggered (PreviewViewport in app.hpp), so the frame that does
  // not draw a region is the frame that publishes neither its viewport nor its curve labels —
  // stated here, once, instead of as a copy of "clear the other mode's publication" in every
  // region entry. That scatter is what let a stale panorama `true` blit over Render frames
  // once already (M2-4's six red cases).
  g_preview_vp.active = false;
  g_preview_vp.curve_labels.clear();
  g_panorama_vp.active = false;
  g_panorama_vp.curve_labels.clear();

  if (g_state.workspace.mode == GuiState::WorkspaceMode::kAnalyze) {
    RenderAnalyzeWorkspace(window, window_width, window_height);
  } else {
    RenderLeftPanel(window_height);
    RenderRightPanel(window, window_width, window_height);
    RenderPreviewPanel(window, window_width, window_height);
  }
  if (!g_panorama_vp.active && press_owner != Owner::kNone) {
    press_owner = Owner::kCancelled;
  }
}

}  // namespace lumice::gui
