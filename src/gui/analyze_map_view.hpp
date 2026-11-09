#ifndef LUMICE_GUI_ANALYZE_MAP_VIEW_HPP
#define LUMICE_GUI_ANALYZE_MAP_VIEW_HPP

// Frozen full-sky class-map display and sky-point intent. The observation
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

inline constexpr float kMapFullSkyRadiusDeg = 180.0f;
// Every map request uses this lattice sample count. The caliber line echoes the result's Info
// so the picture never claims a finer sample than it ran.
inline constexpr int kMapQuerySampleCount = 500000;

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

// The sun label the on-picture marker and delta readout anchor to: the adopted map's own Info echo when one is on show,
// else the adopted survey's scope, else the live document's sun (the analyzed snapshot is always the better anchor —
// the live document may have been edited since). Returns false only with nothing to read.
struct SkySunAnchor {
  double altitude_deg = 0.0;
  double azimuth_deg = 0.0;
};
bool SkySunAnchorFor(const GuiState& state, SkySunAnchor* out);

// The full-sky request for an outstanding data intent: the key supplies class and wavelength;
// sample count and budget are the constants above. Buffers move into the executor's job (the C struct holds raw
// pointers). A zero wavelength leaves the buffer empty — the engine's reference resolution.
LUMICE_PathMapRequest MakeMapRequestForKey(const MapIntentKey& key, std::vector<int>* faces, std::vector<int>* counts,
                                           std::vector<double>* wavelengths);

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
using MapSourcePreparer = bool (*)(const MapHandle&, SurveyQueryId);
void PumpMapQuery(GuiState& state, SurveyExecutor& executor, double now_s, MapSourcePreparer prepare = nullptr);
// Explicit quality actions. Retry repeats the current full-sky request.
void RetryMap(GuiState& state, SurveyExecutor& executor, double now_s);
void CancelMap(GuiState& state, SurveyExecutor& executor);

// The Raypath view window's body — formerly the depth band's map cell (755.4 moved the whole
// capability into the window in one piece). Draws the D3 split (contract §4.2): the LEFT
// region (the raster, its gestures and the query's state line, under the region's own name
// ##AnalyzeRaypathView — the tests' anchor) and the RIGHT info region (##AnalyzeRaypathInfo:
// the wavelength, the view buttons, the caliber block, the sky-point input, the legend), with
// the draggable strip between writing the session ratio
// GuiState::analyze_raypath_view_info_ratio. The raster itself is the .cpp's cached GL
// texture; the overlays, the cursor readout and the coordinate input live with their halves.
void RenderAnalyzeMapRegion(GuiState& state, const ImVec2& size);

// The map window's in-place Pose & trace button's two labels (contract §6's stable operation
// stack). Both derive from kPoseViewWindowTitle — the plain form appends the button's
// `###pose_open` addressing suffix; the narrow-column form is the constant's LAST space turned
// into the line break, the unambiguous derivation rule for the only multi-space spelling.
// Surfaced as functions so the byte-pin test reads the exact submitted strings: a wrong
// derivation or a re-spelled title cannot pass green.
const char* PoseButtonLabel();
const char* PoseButtonWrappedLabel();

// The Pose button's tooltip — the constant purpose text ("3D ray & pose. / Select class + sky
// point.") that left the always-on prose slot beside the button. Surfaced like the two labels
// above so the test layer holds the exact bytes of a text the render never draws as a visible
// item.
const char* PoseButtonTooltip();

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
