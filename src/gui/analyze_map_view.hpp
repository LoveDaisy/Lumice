#ifndef LUMICE_GUI_ANALYZE_MAP_VIEW_HPP
#define LUMICE_GUI_ANALYZE_MAP_VIEW_HPP

// The Analyze workspace's raypath reference-map view (742.5): the single-disk equal-area picture
// of the adopted class map, its pan/zoom/Fit gestures, the sky-point intent and the survey
// payload overlays. Two halves, the overview's shape:
//
//   * The pure half below: the view state and its math — the disk projection (the prototype's
//     Lambert-azimuthal contract, any centre), the gestures (pan, anchored zoom, Fit, full sky),
//     the view->window-request derivation, the map-driven Fit, the display normalization and the
//     cell colour ramp. Every function is frame-free and holds itself to rules without ImGui.
//   * The pump and the ImGui half (bottom of this header, GuiState forward-declared):
//     PumpMapQuery — the survey pump's sibling over the map face's key (selection + view +
//     wavelength) with the settle gate — and RenderAnalyzeMapRegion, the workspace region.
//
// Two conventions this file refuses to blur (the M1 probe pinned both ends; see the session
// struct comments in analyze_survey.hpp):
//   * POSITION vs TRAVEL. MapViewState::centre_pos and every projection/pick here speak
//     POSITION (the vector pointing AT the sky point — what a picture projects). Engine fields
//     (the map request's centre, the sky-point intent's dir) speak TRAVEL (light's propagation =
//     the negated position, the §12 display rule). The conversions are one-liners and named:
//     the request builder negates; the pick writes intent.dir = -pos.
//   * The map's own (delta, azimuth) grid lives in the probe frame the result's Info echoes; this
//     file derives no second frame — it reads view_incident/e1/e2 and converts through the
//     header's formula only.

#include <cstdint>
#include <vector>

#include "gui/analyze_survey.hpp"
#include "include/lumice_raypath.h"

struct ImDrawList;
struct ImVec2;

namespace lumice::gui {

struct GuiState;  // gui/gui_state.hpp — the pump and the renderer take it; this header stays off
                  // its include chain so GuiState can hold the types below.

// ---- The view's own constants (spelled before the session types that default to them) --------------

// The settle window after the last view change before the pump issues (plan §3.2-4: gestures do
// not fire; the settle does — a wheel burst is one request, not one per notch).
inline constexpr double kMapSettleSeconds = 0.3;
// The zoom floor (the prototype's) and the full-sky radius.
inline constexpr float kMapMinRadiusDeg = 1.5f;
inline constexpr float kMapFullSkyRadiusDeg = 180.0f;
// The wheel's zoom factor per notch (the prototype's).
inline constexpr float kMapZoomWheelFactor = 1.2f;
// Every map request samples at this lattice n (the M1 ruling): the interaction tier, sub-second
// at both the full-sky and the window shapes on the 742.1 envelope. The caliber line echoes the
// count from the result's Info — the picture never claims a finer sample than it ran.
inline constexpr int kMapQuerySampleCount = 500000;
// The refinement grid targets this many cells across the disk's diameter (before the pixel-cap
// rescale); the pixel bound is the engine's LUMICE_PATH_MAP_MAX_PIXELS.
inline constexpr int kMapTargetCellsAcross = 220;

// ---- Session-facing types -------------------------------------------------------------------------

// The user's sky-point intent (AC3): WHERE in the sky they are asking about, independent of any
// class, ROI or share-list state. `dir` is the light-travel direction of the point (the engine's
// at-point convention — the same units cone_center_dir carries), so a 742.6 point query can fill
// its target field verbatim; the map pick writes the negated projection result, the coordinate
// input converts (altitude, azimuth) through the engine's own label rule. `from_map_pick` is the
// expression source, kept because the two paths are visible states (a pick shows a marker on the
// map; a typed coordinate does not depend on any map existing).
struct SkyPointIntent {
  bool valid = false;
  float dir[3] = { 0.0f, 0.0f, 0.0f };
  bool from_map_pick = false;
};

// The map display's view state: which disk of sky the region shows. Session tier (the tool's own
// bookkeeping), next to the map query session it drives. The centre is a POSITION vector —
// display space; the radius is the disk's angular half-size in degrees, full sky at
// kMapFullSkyRadiusDeg. gesture_active / last_view_change_s are the settle gate's inputs: a
// gesture frame changes the view but the request fires only after the gesture ends and the view
// has stood still for kMapSettleSeconds. auto_fit_done is the once-per-selection auto-Fit latch
// (the default view chain: the first full-sky map fits the picture once, which fires the first
// refinement request).
struct MapViewState {
  float centre_pos[3] = { 0.0f, 0.0f, 1.0f };
  float radius_deg = kMapFullSkyRadiusDeg;
  bool gesture_active = false;
  bool point_dragging = false;  // the press grabbed the sky-point marker, not the view
  double last_view_change_s = 0.0;
  bool auto_fit_done = false;
};

// ---- Pure view math -------------------------------------------------------------------------------

bool MapViewSettled(double now_s, const MapViewState& view);

// The disk projection: Lambert azimuthal equal-area about the view centre, the prototype's
// makeView contract — screen up is the world zenith projected orthogonal to the centre (the
// (−1,0,0) fallback when the centre is the zenith), screen right the cross product, radius
// rho = 2 sin(theta/2) scaled so the disk's rim is the view radius. Coordinates below are
// normalized: [0,1]x[0,1] over the disk's bounding square, (0,0) the top-left, y down.
struct MapDiskProjector {
  double centre[3];
  double up[3];
  double right[3];
  double radius_rad;  // the view's angular half-size, radians
};

MapDiskProjector MakeMapDiskProjector(const float centre_pos[3], float radius_deg);

// The sky POSITION `pos` -> normalized disk coordinates. False when it falls outside the view
// disk (angle from the centre past the view radius).
bool MapDiskProject(const MapDiskProjector& projector, const double pos[3], double* out_x, double* out_y);

// Normalized disk coordinates -> the sky POSITION under them. False outside the disk (the
// bounding square's corners).
bool MapDiskUnproject(const MapDiskProjector& projector, double x, double y, double out_pos[3]);

// Pan: the sky point under the grab follows the cursor (the prototype's drag rule) — the centre
// moves by the NEGATED delta, in normalized disk coordinates. A pan that would carry the centre
// off the globe clamps at the antipode of the drag direction's end (the unproject miss writes
// nothing and the view stands).
void MapPanViewByDiskCoords(MapViewState* view, double dx, double dy);

// Zoom: the radius scales by `factor` (clamped to [kMapMinRadiusDeg, kMapFullSkyRadiusDeg]) and
// the centre slerps toward `cursor_pos` (the sky position under the cursor) by exactly the
// fraction that keeps that point fixed on screen — the prototype's anchored zoom. A cursor at
// the centre moves nothing but the radius.
void MapZoomViewAtPoint(MapViewState* view, const double cursor_pos[3], float factor);

// Marks a view change for the settle gate (gesture bookkeeping: the caller stamps its own clock).
void MapViewChanged(MapViewState* view, double now_s, bool gesture_active);

// ---- View -> request derivation --------------------------------------------------------------------

// The sun label the request derivation (and the on-picture sun marker and the delta readout)
// anchor to: the adopted map's own Info echo when one is on show, else the adopted survey's
// scope, else the live document's sun (the analyzed snapshot is always the better anchor —
// the live document may have been edited since). Returns false only with nothing to read.
struct SkySunAnchor {
  double altitude_deg = 0.0;
  double azimuth_deg = 0.0;
};
bool SkySunAnchorFor(const GuiState& state, SkySunAnchor* out);

// The window request a view names: the disk's bounding rectangle in the map's (delta,
// azimuth-about-sun) frame, centred on the view centre, plus the grid sized to
// kMapTargetCellsAcross with the azimuth columns matched to the sky spacing at the centre's
// deviation (the grid is a parameterization, not an isotropic raster). `sun_altitude_deg`/
// `sun_azimuth_deg` are the analyzed scene's sun label (the anchor the map frame measures
// deviation against) — SkySunAnchorFor gathers them from the freshest adopted document. A
// full-sky view takes the engine's named all-sky view and the grid zero (the engine's default).
struct MapWindowSpec {
  bool full_sky = true;
  double centre_altitude_deg = 0.0;
  double centre_azimuth_deg = 0.0;
  double delta_span_deg = 0.0;
  double azimuth_span_deg = 0.0;
  int rows = 0;
  int cols = 0;
};
MapWindowSpec MapWindowSpecForView(const MapViewState& view, double sun_altitude_deg, double sun_azimuth_deg);

// The engine request for an outstanding intent (the survey request builder's sibling): the
// key's selection fills the class fields, the view fills the window (against `sun` — the
// bounding rectangle needs the anchor the deviation axis measures from), the sample tier and
// budget are the constants above. Buffers move into the executor's job (the C struct holds raw
// pointers). A zero wavelength leaves the buffer empty — the engine's reference resolution.
LUMICE_PathMapRequest MakeMapRequestForKey(const MapIntentKey& key, const SkySunAnchor& sun, std::vector<int>* faces,
                                           std::vector<int>* counts, std::vector<double>* wavelengths);

// ---- Map-driven Fit and display normalization ------------------------------------------------------

// The view the adopted map's own cells ask for (the prototype's fitRaypath rule, the plan's
// default): the value-weighted centroid of the positive cells in POSITION space, and a radius
// covering 95% of the value mass about it, times the prototype's 1.4 headroom, clamped to
// [3, 180] degrees. ok=false when no positive cell exists (a rank-0 map's texture is an honest
// zero — the mass arm is a marker, not a Fit target) — the caller keeps the current view.
struct MapFitResult {
  bool ok = false;
  float centre_pos[3] = { 0.0f, 0.0f, 1.0f };
  float radius_deg = kMapFullSkyRadiusDeg;
};
MapFitResult FitMapViewFromPixels(const LUMICE_PathMapPixels& pixels, const LUMICE_PathMapInfo& info);

// The display normalization: the robust maximum of the map's own finite positive values (99th
// percentile — the halo's peak must not grey out its whole ring system). valid=false when the
// map carries no positive value (an honest all-zero texture displays as the finite zero it is,
// annotated, not stretched).
struct MapDisplayNorm {
  bool valid = false;
  double vmax = 0.0;
};
MapDisplayNorm MapDisplayNormFor(const LUMICE_PathMapPixels& pixels);

// One cell's display colour, RGBA8. `t` is the normalized value (value / norm.vmax); statuses
// map as the honesty rules demand: OK cells ramp dark-to-light, POINT_MASS gets the fixed
// mass-marker colour, SINGULAR (and any out-of-normalization cell) gets alpha 0 — the picture
// shows data, never a written zero where nothing was computed.
uint32_t MapCellColorRgba(double value, uint8_t status, const MapDisplayNorm& norm);

// ---- The caliber composer (pure; the unit layer holds it to the readback) --------------------------

// The map region's caliber block: every fact the reader calls handed in, the lines spelled here.
// The renderer fills the input from the adopted result's own readers; the unit layer pins each
// line's fields to the input structs — the engine's words, never a hard-coded claim (AC1).
inline constexpr size_t kMapCaliberMaxMethods = 8;
struct MapCaliberInput {
  LUMICE_PathMapInfo info{};
  bool have_evidence = false;
  LUMICE_PathSurveyEvidence evidence{};
  bool have_quantity = false;
  LUMICE_PathQuantitySpec quantity{};
  bool have_mass = false;
  LUMICE_PathMapPointMass mass{};
  const char* methods[kMapCaliberMaxMethods] = {};  // the mass arm's deduplicated method names
  bool completed = false;                           // the standing intent answered (gates the finite-zero line)
};
std::vector<std::string> MapCaliberLines(const MapCaliberInput& in);

// ---- The pump and the renderer (GuiState-dependent, defined in the .cpp) ---------------------------

// The map face's frame pump — the survey pump's sibling (analysis_panel.hpp PumpSurveyQuery):
// drains the executor's map mailbox through the adoption gate, then keeps the intent
// level-triggered against the live selection + view + wavelength, gated by the settle rule.
// Writes only state.analysis's map fields and the executor; no widget, no document field.
void PumpMapQuery(GuiState& state, SurveyExecutor& executor, double now_s);

// The workspace's raypath-view region — the placeholder's replacement in
// analyze_workspace.cpp's lower row. Draws the state around the picture (the raster itself is
// the .cpp's cached GL texture), hosts the gestures, the overlays, the coordinate input and the
// caliber line.
void RenderAnalyzeMapRegion(GuiState& state, const ImVec2& size);

// Releases the region's GL texture (the app's teardown, next to the executor's join).
void ShutdownMapView();

// The pick the unit layer drives without a frame: the sky position under a disk coordinate, or
// nullopt outside the disk. The renderer's click path goes through this one function — the
// same conversion the tests pin.
bool MapPickPositionAtDiskCoords(const MapViewState& view, double x, double y, double out_pos[3]);

}  // namespace lumice::gui

#endif  // LUMICE_GUI_ANALYZE_MAP_VIEW_HPP
