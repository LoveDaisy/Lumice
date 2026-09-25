#ifndef LUMICE_GUI_ANGULAR_DIST_PICKER_HPP
#define LUMICE_GUI_ANGULAR_DIST_PICKER_HPP

// The Angular Distance section's picker: arm it from a family's angle editor, move the cursor over
// the preview, and a ring about that family's centre follows the cursor; a left click adds the
// ring's radius to the family's list.
//
// The preview ring is NOT drawn by anything in this file. It is the candidate radius handed to the
// shader as one more level of the family's own list for that frame (InjectAngularDistPickPreview),
// so it is rasterized by the same per-fragment level-set, about the same centre uniform, at the
// same line width as the ring the click then adds — "the ring you see is the ring you get" is a
// property of there being one drawing path, not of two being kept in step.
//
// The radius is measured the way the shader measures a fragment's distance: the pixel inverted
// through the shader's own inverse projection (PixelToWorldDir, overlay_labels.hpp — the CPU
// mirror of the GLSL, formula for formula), then acos(dot) against the centre the shader uses. That
// is what puts the ring through the cursor pixel even at a lens's rim.
//
// Whether the pixel is sky at all is a separate question with a separate authority:
// LUMICE_UnprojectPixel, which alone knows `visible` and `front` (the mirror knows only the lens's
// projection domain). A pixel is pickable only when both say it is on the sky — the C API for "is
// this on the picture", the mirror for "which direction, as the shader will draw it". The two
// implementations of the lens inverse are held together by the six parity gates listed at
// PixelToWorldDir; on the rare rim pixel where their domains disagree, the pixel is simply not
// pickable.

#include <optional>
#include <string>
#include <vector>

#include "gui/gui_state.hpp"
#include "gui/preview_renderer.hpp"
#include "include/lumice.h"

namespace lumice::gui {

// The world direction the family's rings are centred on, as the preview shader takes it:
//   kSun   u_reference_dir, filled from GuiSunWorldDir(sun altitude) (annotation_anchors.hpp)
//   kView  view_axis = -u_view_matrix[2], the camera's forward, from BuildViewMatrix on the
//          preview's own ViewProjection (so a lens that ignores roll sees roll 0 here too)
void AngularDistFamilyCenterDir(AngularDistFamily family, const ViewProjection& view_proj, float sun_altitude_deg,
                                float out[3]);

// The radius, in degrees, of the family's ring through canvas pixel (px, py) — the ring's centre
// is AngularDistFamilyCenterDir — or nullopt when that pixel is not pickable sky (off the canvas,
// outside the lens's image, on the side `visible` / `front` hides). The canvas is view.width x
// view.height, top-left origin, y down: the space PreviewPointToCanvasPixel produces and
// LUMICE_UnprojectPixel reads. `view` must describe the same picture as `view_proj`
// (PreviewAnnotationView and BuildPreviewViewProjFromRenderer of the same state).
std::optional<float> AngularDistAtCanvasPixel(AngularDistFamily family, const LUMICE_AnnotationView& view,
                                              const ViewProjection& view_proj, float sun_altitude_deg, int px, int py);

// Arm the picker for `family`. Also switches that family's LINE on — a persistent write, not a
// display override: a user picking a radius on a ring family means to see that family, and would
// otherwise click a ring into a list whose rings are hidden. Cancelling the pick does not switch
// the line back off; the switch was set by the user's entering the mode, which a cancel does not
// undo. The Label switch is left alone. Disarms the analysis window's point pick (one
// click-taking mode at a time, GuiState::AngularDistPicker); the eyedropper's flag lives outside
// GuiState and is the caller's to clear.
void ArmAngularDistPicker(GuiState& state, AngularDistFamily family);

// The family's list in `state`.
std::vector<float>& AngularDistFamilyAngles(GuiState& state, AngularDistFamily family);

// Add a picked radius to `angles` under the rules the editor's "+" button applies: clamped into
// ClampAngularDistCircleAngle's band, not added again when AngularDistCircleAlreadyPresent says
// the list holds it, not added at all past AngularDistCirclesAtLimit; the list stays sorted.
// Returns whether the list changed.
bool CommitAngularDistPick(std::vector<float>& angles, float angle_deg);

// Put the candidate ring on this frame's picture: `angle_deg` appended to the family's levels in
// `overlay` (the frame's own copy — the document's list is untouched) and the family's line forced
// on for this frame. Called only with a pickable radius; no call, no ring.
void InjectAngularDistPickPreview(AngularDistFamily family, float angle_deg, OverlayDecoration& overlay);

// The readout drawn beside the cursor: the radius in the format the editor's list prints
// its entries in ("%.1f°"), so the number read at the click is the number the list then shows.
std::string FormatAngularDistPickReadout(float angle_deg);

}  // namespace lumice::gui

#endif  // LUMICE_GUI_ANGULAR_DIST_PICKER_HPP
