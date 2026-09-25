#include "gui/screenshot_export_options.hpp"

#include <utility>

#include "gui/app.hpp"

namespace lumice::gui {

ScreenshotExportSelection MakeScreenshotExportSelectionFromState(const GuiState& state) {
  ScreenshotExportSelection sel;
  sel.horizon_line = state.show_horizon_line;
  sel.horizon_label = state.show_horizon_label;
  sel.grid_line = state.show_grid_line;
  sel.grid_label = state.show_grid_label;
  sel.sun_circles_line = state.show_sun_circles_line;
  sel.sun_circles_label = state.show_sun_circles_label;
  sel.view_dist_line = state.show_view_dist_line;
  sel.view_dist_label = state.show_view_dist_label;
  sel.lens_border_line = state.show_lens_border_line;
  sel.markers_line = AnyMarkerShown(state);
  sel.markers_label = AnyMarkerLabelShown(state);
  sel.display_mode = state.renderer.display_mode;
  return sel;
}

bool ScreenshotDisplayModeRenderable(const GuiState& state, const ScreenshotFrameFacts& facts, int mode) {
  if (mode < 0 || mode >= kDisplayModeCount) {
    return false;
  }
  // The upload rule SyncFromPoller applies, evaluated for `mode` instead of the document's mode,
  // against the upload it actually made.
  RenderConfig renderer = state.renderer;
  renderer.display_mode = mode;
  const bool needs_composite = ShouldUseCompositeUpload(facts.payload_is_composite,
                                                        state.show_composite_preview && !IsChannelBrDisplay(renderer));
  return needs_composite == state.last_uploaded_as_composite;
}

void ApplyScreenshotExportSelectionToParams(const ScreenshotExportSelection& sel, const GuiState& state,
                                            const ScreenshotFrameFacts& facts, PreviewParams& params) {
  // Each line: off unless the panel AND the selection have it. Only ever clears — a family `params`
  // already had off (the panel's switch, or a caller's own override) stays off.
  if (!(sel.horizon_line && state.show_horizon_line)) {
    params.overlay.show_horizon = false;
  }
  if (!(sel.grid_line && state.show_grid_line)) {
    params.overlay.show_grid = false;
  }
  if (!(sel.sun_circles_line && state.show_sun_circles_line)) {
    params.overlay.show_sun_circles = false;
  }
  if (!(sel.view_dist_line && state.show_view_dist_line)) {
    params.overlay.show_view_dist = false;
  }
  if (!(sel.lens_border_line && state.show_lens_border_line)) {
    params.overlay.show_lens_border = false;
  }
  // The rings have no enable flag — the sentinel position is the "do not draw" (OverlayDecoration).
  // Kept: every slot stays as the preview wrote it, i.e. each marker's own switch still applies.
  if (!(sel.markers_line && AnyMarkerShown(state))) {
    params.overlay.marker_screen_pos = MakeAllSentinelMarkerPositions();
  }

  // The display mode, when it changes, carries the background photo with it: the channel-B-R
  // diagnostic hides the photo, and a Normal export of a B-R screen brings it back — read through
  // the same rule the preview frame uses.
  if (sel.display_mode != params.display_mode && ScreenshotDisplayModeRenderable(state, facts, sel.display_mode)) {
    RenderConfig renderer = state.renderer;
    renderer.display_mode = sel.display_mode;
    params.display_mode = sel.display_mode;
    params.bg.enabled = BgPhotoInFrame(facts.has_background, state.bg_show, renderer);
  }
}

std::vector<CurveLabelSet> BuildScreenshotExportLabelSets(const AnnotationAnchors& cache, const GuiState& state,
                                                          const ScreenshotExportSelection& sel, float vp_w,
                                                          float vp_h) {
  std::vector<CurveLabelSet> sets;
  if (sel.horizon_label && state.show_horizon_label) {
    sets.push_back(BuildHorizonLabelSet(cache, state, vp_w, vp_h));
  }
  if (sel.sun_circles_label && state.show_sun_circles_label) {
    sets.push_back(BuildSunCirclesLabelSet(cache, state, vp_w, vp_h));
  }
  if (sel.view_dist_label && state.show_view_dist_label) {
    sets.push_back(BuildViewDistLabelSet(cache, state, vp_w, vp_h));
  }
  if (sel.grid_label && state.show_grid_label) {
    sets.push_back(BuildGridLabelSet(cache, state, vp_w, vp_h));
  }
  // Each marker carries its own label switch, which BuildMarkerLabelSets reads; the selection only
  // decides whether the family is in at all.
  if (sel.markers_label) {
    for (CurveLabelSet& set : BuildMarkerLabelSets(cache, state, vp_w, vp_h)) {
      sets.push_back(std::move(set));
    }
  }
  return sets;
}

}  // namespace lumice::gui
