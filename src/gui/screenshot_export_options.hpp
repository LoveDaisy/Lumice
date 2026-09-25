#ifndef LUMICE_GUI_SCREENSHOT_EXPORT_OPTIONS_HPP
#define LUMICE_GUI_SCREENSHOT_EXPORT_OPTIONS_HPP

// What one Screenshot export leaves out of the frame the preview is showing.
//
// The Screenshot export used to have a second gate once before — "Include Overlay in Screenshot" —
// and it was removed because it could not agree with the Overlay panel: it defaulted off, did not
// persist, and gated only the text while the lines went out regardless. This one is shaped so that
// none of those three can happen:
//
//   - PREFILLED from the panel every time it is opened (MakeScreenshotExportSelectionFromState), so
//     the default is exactly the picture on screen and there is no second default to drift;
//   - ONE-SHOT: the selection lives only until the export it was opened for, and nothing here writes
//     back into GuiState, so there is no second persisted state to disagree with the first;
//   - SUBTRACTIVE, per family, for the line AND the label separately: every overlay field is the
//     intersection of the panel switch and the selection (ApplyScreenshotExportSelectionToParams /
//     BuildScreenshotExportLabelSets), so a family the screen does not show cannot be exported even
//     if a caller sets its selection bit — the clamp is here, not in the dialog's greying.
//
// The display mode is the one exception to "subtractive": it picks how the picture is drawn, not a
// layer drawn over it, so the export may take either mode — but only one the texture now on screen
// can actually draw (ScreenshotDisplayModeRenderable).
//
// Pure: no ImGui, no GL, no globals. Whatever the globals would supply is passed in
// (ScreenshotFrameFacts), which is what lets the windowless test targets call all of it.

#include <vector>

#include "gui/annotation_anchors.hpp"
#include "gui/gui_state.hpp"
#include "gui/overlay_labels.hpp"
#include "gui/preview_renderer.hpp"

namespace lumice::gui {

// One bit per thing the export may leave out. The defaults (everything in) are only what a
// default-constructed value holds; a real selection is always made by
// MakeScreenshotExportSelectionFromState.
struct ScreenshotExportSelection {
  bool horizon_line = true;
  bool horizon_label = true;
  bool grid_line = true;
  bool grid_label = true;
  bool sun_circles_line = true;
  bool sun_circles_label = true;
  bool view_dist_line = true;
  bool view_dist_label = true;
  bool lens_border_line = true;  // the lens border has no label
  // The six reference points as one family: clearing a bit drops every point's ring (or name);
  // leaving it set keeps each point exactly as its own switch in GuiState::markers says.
  bool markers_line = true;
  bool markers_label = true;
  int display_mode = 0;  // same spelling as RenderConfig::display_mode
};

// The two facts about the frame on screen that GuiState does not hold: whether a background photo
// is loaded, and whether the payload behind the uploaded texture carries a raypath-colour
// composite. The caller reads them off its globals (app.cpp); tests state them.
struct ScreenshotFrameFacts {
  bool has_background = false;
  bool payload_is_composite = false;
};

// The selection that reproduces the screen: every bit equal to the panel's switch for it, the
// display mode equal to the current one.
ScreenshotExportSelection MakeScreenshotExportSelectionFromState(const GuiState& state);

// Can the texture now on screen be drawn in `mode`? The texture was uploaded as the composite or
// as full spectrum according to the CURRENT mode (the channel-B-R diagnostic reads the full
// spectrum even while the composite preview is selected — SyncFromPoller), so a mode that would
// need the other upload cannot be exported without changing what the screen shows. False only
// when a composite payload is being previewed; every other state renders both modes.
bool ScreenshotDisplayModeRenderable(const GuiState& state, const ScreenshotFrameFacts& facts, int mode);

// Narrow `params` (a copy of the preview's params, as BuildExportParams returns it) to the
// selection: each overlay line family is kept only where the panel AND the selection both have it,
// and the display mode — with the background photo, whose visibility depends on it — follows the
// selection when the texture can render it and stays as it is otherwise. Never turns anything on
// that `params` had off.
void ApplyScreenshotExportSelectionToParams(const ScreenshotExportSelection& sel, const GuiState& state,
                                            const ScreenshotFrameFacts& facts, PreviewParams& params);

// The label sets the export draws: the same builders, in the same order, the preview's own label
// block uses (app_panels.cpp), each family gated on the panel switch AND the selection. `vp_w` /
// `vp_h` are the target's logical-point size, as for the builders themselves.
std::vector<CurveLabelSet> BuildScreenshotExportLabelSets(const AnnotationAnchors& cache, const GuiState& state,
                                                          const ScreenshotExportSelection& sel, float vp_w, float vp_h);

}  // namespace lumice::gui

#endif  // LUMICE_GUI_SCREENSHOT_EXPORT_OPTIONS_HPP
