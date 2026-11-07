#ifndef LUMICE_GUI_ANALYZE_MAP_VIEW_HPP
#define LUMICE_GUI_ANALYZE_MAP_VIEW_HPP

// Frozen class-map display, immutable quality targets and sky-point intent. The observation
// camera is exclusively GuiState::renderer; no disk camera or settle-triggered query exists.
// Raw result frames/columns remain engine-owned under their immutable handles.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "gui/analyze_survey.hpp"
#include "include/lumice_raypath.h"

struct ImDrawList;
struct ImVec2;

namespace lumice::gui {

struct GuiState;  // gui/gui_state.hpp — the pump and the renderer take it; this header stays off
                  // its include chain so GuiState can hold the types below.

// ---- The view's own constants (spelled before the session types that default to them) --------------

inline constexpr float kMapMinRadiusDeg = 1.5f;
inline constexpr float kMapFullSkyRadiusDeg = 180.0f;
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

// ---- View -> request derivation --------------------------------------------------------------------

struct ViewProjection;

// An immutable calculation target, NOT an observation camera. Captured only by an explicit
// quality action; subsequent camera/viewport changes cannot mutate it. The cap conservatively
// contains the effective canvas (the globe's near root only).
struct MapCaptureTarget {
  double centre_pos[3] = { 0.0, 0.0, 1.0 };
  double radius_deg = 180.0;
};
MapCaptureTarget CaptureMapTarget(const ViewProjection& view, int width_px, int height_px);

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
  bool subsky_target = false;
  double actual_delta_span_deg = 180.0;
  double centre_altitude_deg = 0.0;
  double centre_azimuth_deg = 0.0;
  double delta_span_deg = 0.0;
  double azimuth_span_deg = 0.0;
  int rows = 0;
  int cols = 0;
};
MapWindowSpec MapWindowSpecForTarget(const MapCaptureTarget& target, double sun_altitude_deg, double sun_azimuth_deg);

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
MapFitResult FitMapViewFromPixels(const LUMICE_PathMapPixels& pixels, const LUMICE_PathMapInfo& info,
                                  size_t* temporary_bytes = nullptr);

// The display normalization: the robust maximum of the map's own finite positive values (99th
// percentile — the halo's peak must not grey out its whole ring system). valid=false when the
// map carries no positive value (an honest all-zero texture displays as the finite zero it is,
// annotated, not stretched).
struct MapDisplayNorm {
  bool valid = false;
  double vmax = 0.0;
};
MapDisplayNorm MapDisplayNormFor(const LUMICE_PathMapPixels& pixels, size_t* temporary_bytes = nullptr);

// One cell's display colour, RGBA8. `t` is the normalized value (value / norm.vmax); statuses
// map as the honesty rules demand: OK cells ramp dark-to-light, POINT_MASS gets the fixed
// mass-marker colour, SINGULAR (and any out-of-normalization cell) gets alpha 0 — the picture
// shows data, never a written zero where nothing was computed.
std::array<double, 4> MapCellColor(double value, uint8_t status, const MapDisplayNorm& norm);
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

// ---- The overlay presentation model (pure; the renderer draws what these name) ----------------------

struct SurveyObjectRow;  // gui/analyze_overview.hpp — the grouping reads its rows by pointer

// One survey object marker's look: a shape per kind (the engine's six-kind vocabulary plus the
// fallback), a colour per kind, and solid-vs-outline by the claim's state — a certified or
// finite conclusion draws solid, the refused/unevaluated/unknown states draw outline only (the
// marker never presents an unevaluated row as if it were an answered one).
enum class OverlayMarkerShape { kCircle, kSquare, kTriangle, kDiamond, kTriangleDown, kFallbackDiamond };
struct OverlayMarkerStyle {
  OverlayMarkerShape shape = OverlayMarkerShape::kFallbackDiamond;
  unsigned int colour = 0;  // ImU32 (RGBA8); spelled unsigned to keep this header off imgui.h
  bool filled = false;
};
OverlayMarkerStyle OverlayMarkerStyleFor(const std::string& kind_name, int state);

// One tooltip's grouped entries: the objects whose marker rect the pointer is inside, grouped
// by the row's OBSERVATION key — the row's whole content (kind, state, position, reason AND the
// evidence fields family_level/blur_deg), because "identical" is a claim about the sources.
// Identical rows fold into one entry with a count. A display grouping of the model's rows:
// nothing is dropped (every hit appears, folded or lone), no physical claim changes, and rows
// differing in ANY carried field stay separate entries. First-seen order is kept (the model's
// own order). The grouping is pure presentation; the unit layer pins it on observed row shapes
// plus synthetic same-key and evidence-differing rows.
struct OverlayHitGroup {
  const SurveyObjectRow* representative = nullptr;
  int count = 0;
};
void GroupOverlayHits(const std::vector<const SurveyObjectRow*>& hits, std::vector<OverlayHitGroup>* out);

// What the frame actually drew — the legend's one input (a legend describes the drawn picture,
// so a category absent from this frame draws no entry). The overlay/anchor draws fill it; the
// info region's legend reads it, same frame.
struct OverlayLegendState {
  bool anchor_sun = false;
  bool anchor_zenith = false;
  bool anchor_horizon = false;
  std::vector<std::string> object_kinds;  // unique, in first-draw order
  bool dispersion_red = false;
  bool dispersion_blue = false;
  bool dispersion_partial = false;  // any drawn circle left the view disk (the arc-in-view wording)
  bool sky_point = false;
};

// Last actually drawn overlay legend, not a second camera/availability model.
OverlayLegendState LastMapOverlayLegend();
struct MapObservationTiming {
  double began_seconds = 0.0;
  double cpu_finished_seconds = 0.0;
  int width = 0;
  int height = 0;
};
MapObservationTiming LastMapObservationTiming();

// ---- The pump and the renderer (GuiState-dependent, defined in the .cpp) ---------------------------

// The map face's frame pump — the survey pump's sibling (analysis_panel.hpp PumpSurveyQuery):
// drains the map mailbox through generation/serial/data identity gates. Only a new data
// identity establishes an all-sky intent; observation never issues a request.
// Writes only state.analysis's map fields and the executor; no widget, no document field.
// The GL frame owner supplies preparation explicitly; frame-free consumers leave it unset.
// Preparation succeeds before a finite slot changes, keeping raw readers and images attached.
using MapSourcePreparer = bool (*)(const MapHandle&, SurveyQueryId, bool);
void PumpMapQuery(GuiState& state, SurveyExecutor& executor, double now_s, MapSourcePreparer prepare = nullptr);
// Explicit quality actions. Refine captures the target once; Retry reuses that exact request.
bool RefineMap(GuiState& state, SurveyExecutor& executor, const MapCaptureTarget& target, double now_s);
void RetryMap(GuiState& state, SurveyExecutor& executor, double now_s);
void CancelMap(GuiState& state, SurveyExecutor& executor);
bool MapSourcesCompatible(const LUMICE_PathMapInfo& reference, const LUMICE_PathMapInfo& detail);

// The Raypath view window's body — formerly the depth band's map cell (755.4 moved the whole
// capability into the window in one piece). Draws the D3 split (contract §4.2): the LEFT
// region (the raster, its gestures and the query's state line, under the region's own name
// ##AnalyzeRaypathView — the tests' anchor) and the RIGHT info region (##AnalyzeRaypathInfo:
// the wavelength, the view buttons, the caliber block, the sky-point input, the legend), with
// the draggable strip between writing the session ratio
// GuiState::analyze_raypath_view_info_ratio. The raster itself is the .cpp's cached GL
// texture; the overlays, the cursor readout and the coordinate input live with their halves.
void RenderAnalyzeMapRegion(GuiState& state, const ImVec2& size);

// The Raypath view floating window (doc/gui/analyze-interaction-contract.md §6): the non-modal
// container the share list's class rows open. Committed from BOTH frame loops next to the
// Colors/Summary windows, in every workspace mode — early-return while
// `state.analyze_raypath_view_open` is false, so a closed window costs one branch. The `bool*`
// passed to ImGui::Begin is the flag itself: the title-bar X writes it back directly, and the
// next class-row click reopens. In-flight queries are not touched by opening or closing (the
// T-session contract, §8). In the Render workspace the window's frames run the shared query
// pumps and clear the map wait-feedback fields (analyze_workspace.hpp PumpAnalyzeQueries) —
// it is the only analysis face rendering there. The body is RenderAnalyzeMapRegion over the
// window's content area.
void RenderAnalyzeRaypathViewWindow(GuiState& state);

// Releases the region's GL texture (the app's teardown, next to the executor's join).
bool InitMapView();
void ShutdownMapView();

}  // namespace lumice::gui

#endif  // LUMICE_GUI_ANALYZE_MAP_VIEW_HPP
