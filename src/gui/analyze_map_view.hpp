#ifndef LUMICE_GUI_ANALYZE_MAP_VIEW_HPP
#define LUMICE_GUI_ANALYZE_MAP_VIEW_HPP

// Frozen full-sky class-map display and sky-point intent. The observation
// camera is exclusively GuiState::renderer; no disk camera or settle-triggered query exists.
// Raw result frames/columns remain engine-owned under their immutable handles.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "gui/analyze_row_hover.hpp"
#include "gui/analyze_survey.hpp"
#include "include/lumice_raypath.h"

struct ImDrawList;
struct ImVec2;

namespace lumice::gui {

struct GuiState;  // gui/gui_state.hpp — the pump and the renderer take it; this header stays off
                  // its include chain so GuiState can hold the types below.

// ---- The view's own constants (spelled before the session types that default to them) --------------

// Every map request uses this lattice sample count. The caliber line echoes the result's Info
// so the picture never claims a finer sample than it ran.
inline constexpr int kMapQuerySampleCount = 500000;

// The chromatic layer's two semantic colours, the dispersion pair's own values promoted to names
// (contract §16.1's A2 pilot): the dispersion circles and the verdict anchor share this palette —
// one semantics, one authority — so retuning a value here is a TWO-consumer change by definition,
// never a one-side restyle. Spelled as the shift expression IM_COL32 spells (alpha<<24 | blue<<16
// | green<<8 | red) so the RGBA tuple stays readable and this header stays off imgui.h;
// analyze_map_view.cpp pins each against its original literal at compile time.
inline constexpr unsigned int kChromaticColourRed = (235u << 24) | (92u << 16) | (108u << 8) | 242u;
inline constexpr unsigned int kChromaticColourBlue = (235u << 24) | (240u << 16) | (145u << 8) | 96u;

// The readout label's backing ink (the dark patch every iso-deviation readout sits on): promoted
// beside the chromatic pair because the unit layer reads the patch BY THIS COLOUR — the backing
// is unique in the map frame, so its vertex box is the label's patch rect, and the clamp's tests
// hold it to the canvas. Same discipline as the pair above: one value, both consumers (the draw
// and the test), a compile-time pin against the original literal.
inline constexpr unsigned int kReadoutBackingInk = (170u << 24) | (20u << 16) | (17u << 8) | 16u;

// The reference profile's semantic colour (the hover device's point/arc on the map): a rose
// distinct from every existing semantic hue on the map (the chromatic pair's red/blue, the sun's
// gold, the object kinds' five, the sky point's white) — the profile's readings are brightness
// facts, not chromatic ones, so they take their own name. One value, two consumers (the draw and
// the test), pinned like the pair above.
inline constexpr unsigned int kProfileReadingColour = (255u << 24) | (182u << 16) | (114u << 8) | 244u;

// The survey reference arcs' ink (the reach bounds, the sharp edges and the internal-reflection
// turns all draw in it): the anchors' neutral grey at conversational alpha — a fixed-λ reference
// structure, deliberately NOT a chromatic hue (the red-blue group's ink is the chromatic pair
// alone, so the group's negative control cannot be defeated by an arc), and deliberately ONE
// ink for all three layers: the layers differ by OWNER (legend names, label prefixes), never by
// hue, so toggling a group never repaints a shared stroke and no arc can borrow another layer's
// look. Same discipline as the pair above: one value, two consumers (the draw and the test), a
// compile-time pin against the original literal.
inline constexpr unsigned int kSurveyArcInk = (215u << 24) | (215u << 16) | (205u << 8) | 200u;

// ---- Session-facing types -------------------------------------------------------------------------

// The user's sky-point intent (AC3): WHERE in the sky they are asking about, independent of any
// class, ROI or share-list state. `dir` is the light-travel direction of the point (the engine's
// at-point convention — the same units cone_center_dir carries), so a 742.6 point query can fill
// its target field verbatim; the map pick writes the negated projection result, the coordinate
// input converts (altitude, azimuth) through the engine's own label rule. `from_map_pick` is the
// expression source only — the tooltip's provenance word reads it; it gates nothing. The
// marker's draw gate is `valid` alone: every writer of a valid intent (pick, drag, the Set sky
// point button) draws the same ink.
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

// ---- Map display normalization ---------------------------------------------------------------------

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
// One read-back per information tier (the disposition ledger): the DEFAULT lines answer "what
// am I looking at" — the short quantity name with its unit, the scene-vs-nominal limit when
// the answer is not the nominal shape, and the truncation / finite-zero / rank-0-mass facts;
// the PHYSICS lines carry the full quantity definition and the sun premise — the evidence
// group's rows; the DIAGNOSTICS lines are the env-gated group's — the engine's own geometry
// basis verbatim plus the grid and execution numbers: reference cell, view window, lattice,
// members, kept events, and the source-fixed-scale note. The λ is in no line: the λ row's
// same-control echo is the reading's one owner.
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
std::vector<std::string> MapCaliberDefaultLines(const MapCaliberInput& in);
std::vector<std::string> MapCaliberPhysicsLines(const MapCaliberInput& in);
std::vector<std::string> MapCaliberDiagnosticsLines(const MapCaliberInput& in);

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

// The grouped hover presentation, independent of marker projection and hit collection.
void RenderSurveyObjectTooltip(const std::vector<const SurveyObjectRow*>& hits);

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
  bool dispersion_partial = false;         // any drawn circle left the view disk (the arc-in-view wording)
  bool verdict_anchor = false;             // the verdict anchor drew (solid or dashed) — one bit: the
                                           // legend entry names the category, the dashed form's honesty
                                           // wording rides the Legend explanations group
  unsigned int verdict_anchor_colour = 0;  // the drawn anchor's palette entry (kChromaticColour*)
                                           // — the legend icon's own colour, recorded with the bit
                                           // so the icon reads what actually drew
  bool verdict_anchor_dashed = false;      // the drawn form's weakening — the icon carries it too
  bool sky_point = false;
  // The five layer groups' arcs, what the frame actually submitted (a legend describes the drawn
  // picture — a group whose gate is off, or whose payloads left the view, draws no entry).
  bool reach_bounds = false;           // any support-range bound arc, or the full-range text label, drew
  int reach_bound_count = 0;           // how many bounds submitted (inner/outer/gap, or 1 for the text)
  bool reach_full_range_text = false;  // the reach drew its full-range TEXT, not bound arcs — the
                                       // legend entry names the form it carries
  bool edge_arcs = false;              // any sharp-edge arc submitted (a shared stroke counts for the edge too)
  bool edge_shared = false;            // the drawn edge arc is one stroke shared with a support bound
  bool tir_turns = false;              // any internal-reflection turn pair drew
  int tir_turn_count = 0;              // how many turns drew (one entry per turn pair)
};

// Last actually drawn overlay legend, not a second camera/availability model.
OverlayLegendState LastMapOverlayLegend();

// Same-frame readouts for the dispersion pair and the verdict. All three keep their full text;
// layout owns only display positions, never the curve's physical anchor or projection.
struct MapReadoutLabel {
  float anchor_x = 0.0f;
  float anchor_y = 0.0f;
  unsigned int colour = 0;
  std::string text;
};
void DrawMapReadoutLabels(ImDrawList* dl, const std::vector<MapReadoutLabel>& labels, float x, float y, float width,
                          float height);

// The production sky-point ink, shared by every provenance path after projection succeeds.
void DrawSkyPointIndicator(ImDrawList* dl, float cx, float cy);

// ---- The verdict anchor ---------------------------------------------------------------------------

struct SurveyDiagnoseView;  // gui/analyze_overview.hpp — the anchor's input facts, by reference

// The anchor draw's caller-side geometry and switches. Every field is a plain value so the unit
// layer can drive every honesty leg on a synthetic diagnose: the input struct is the model's own
// field shape (SurveyDiagnoseView), the camera reaches the projection by pointer (the header
// stays off ViewProjection's include chain), and the canvas rectangle is the region's own.
// The projection quintet (camera/width_px/height_px/width_pt/height_pt) mirrors MapCanvasProjection
// field-for-field — DrawVerdictAnchor rebuilds the projection from these, and a new projection
// field must be added to BOTH sides or the anchor's rebuilt view silently defaults while the
// dispersion circles move (the geometry-parameter twin of the palette's static_assert pin).
struct VerdictAnchorInput {
  const ViewProjection* camera = nullptr;  // the region's frame projection (incomplete here)
  int width_px = 1;                        // the projection's pixel sizes (the frame's own)
  int height_px = 1;
  float width_pt = 1.0f;  // the canvas rectangle's point sizes
  float height_pt = 1.0f;
  float pic_min_x = 0.0f;  // the canvas rectangle's top-left, screen coords
  float pic_min_y = 0.0f;
  float pic_px = 1.0f;                             // the canvas width in points (the walk's disk)
  SkySunAnchor sun;                                // the sun the delta quotes (SkySunAnchorFor's read)
  bool overlays = true;                            // the sole analysis-ink gate
  bool label_wanted = true;                        // the readout label submits only when the labels
                                                   // toggle is on; the arc/legend bit draws either way
                                                   // (labels annotate the ink, they do not gate it)
  OverlayLegendState* legend = nullptr;            // the frame's legend accounting; null = no legend
  std::vector<MapReadoutLabel>* labels = nullptr;  // production collects before laying out; null draws alone
};

// The diagnose block's spatial conclusion drawn as an iso-deviation circle about the sun — the
// dispersion curve's own geometry at the verdict's delta, the vocabulary's circle device. The
// honesty gate runs in contract §16.1.3's short-circuit order: a diagnosis the class
// answer does not render rows for (unavailable), a verdict outside the closed trigger kind set
// (the edge family only — the kinds that carry a position, dp_chromatic's VerdictOf), a verdict
// with no spatial payload, and a verdict that names itself not visible draw NOTHING (not drawing
// is the honest state, the text layer's limit lines and the legend's drawn-only rule carry it);
// a position-carrying verdict with incomplete coverage draws the DASHED form (the same circle,
// weakened — the "limited computation" honesty in the visual grammar), solid otherwise. The
// colour is the shared semantic palette (kChromaticColour*): blue verdict blue, red verdict red,
// no palette entry → no anchor. The readout label and the legend bit follow the dispersion
// circle's precedents (topmost drawn point, backing rect, "arc in view"; a legend describes the
// drawn picture — nothing drawn, no entry). Surfaced whole (gate + geometry) for the unit
// layer's draw-list assertions; the map region's overlay path calls it with the adopted survey's
// cache model under the sole Overlays gate.
void DrawVerdictAnchor(ImDrawList* dl, const SurveyDiagnoseView& diagnose, const VerdictAnchorInput& in);
struct MapObservationTiming {
  double began_seconds = 0.0;
  double cpu_finished_seconds = 0.0;
  int width = 0;
  int height = 0;
};
MapObservationTiming LastMapObservationTiming();

// ---- The survey reference arcs (the layer plan's reach / edge / turn groups) ------------------------

struct SurveyOverlayFacts;  // gui/analyze_overview.hpp — the arcs' payload facts, by reference

// The five layer groups' session switches, read from the map window's storage each frame (the
// T-session display preferences; the master Overlays gate is applied by the caller and lives in
// the same storage). One group, one pair — the Render habit's graphics/labels split at the
// group's granularity: the labels box shows the group's readout labels without touching its ink,
// and the graphics box removes ink and labels together (nothing to annotate). Defaults all on:
// a group with no payload draws nothing anyway, so the defaults never fabricate ink.
struct SurveyLayerGate {
  bool graphics = true;
  bool labels = true;
};
struct SurveyLayerGates {
  SurveyLayerGate reach;      // support-range bounds
  SurveyLayerGate edge;       // sharp-edge positions
  SurveyLayerGate points;     // positioned object markers (their text channel is the hover tooltip)
  SurveyLayerGate chromatic;  // the red-blue group: the dispersion pair and the verdict anchor
  SurveyLayerGate tir;        // internal-reflection turn references
};

// The reference arcs' caller-side geometry and gates — the same mirror discipline as
// VerdictAnchorInput's projection quintet (a new MapCanvasProjection field must be added to BOTH
// sides or the rebuilt view silently defaults).
struct ReferenceArcInput {
  const ViewProjection* camera = nullptr;
  int width_px = 1;
  int height_px = 1;
  float width_pt = 1.0f;
  float height_pt = 1.0f;
  float pic_min_x = 0.0f;
  float pic_min_y = 0.0f;
  float pic_px = 1.0f;
  SkySunAnchor sun;
  SurveyLayerGate reach;                 // the group gates (the master Overlays gate is the caller's, outside
  SurveyLayerGate edge;                  // this pass); a group's labels bit shows its readout labels without
  SurveyLayerGate tir;                   // touching its ink
  OverlayLegendState* legend = nullptr;  // the frame's legend accounting; null = no legend
  std::vector<MapReadoutLabel>* labels_sink = nullptr;  // production collects into the frame's set
};

// The reach/edge/turn arcs, drawn in the ONE shared neutral ink (kSurveyArcInk). The shared
// stroke (the layer plan's AC2): an edge whose card declares coincidence with support takes its
// NEARER interval endpoint's bound as the same geometry — with both gates on, one walk submits
// the stroke and both owners' names stand in the legend; each gate alone keeps the line; only
// both off (or no payloads) submit nothing. A non-coincident edge always walks its own circle —
// coincidental on-screen overlap merges nothing. Honesty: bounds at the degenerate deltas (0 —
// the sun's own anchor — and 180) submit no walk; the full-range reach draws its text statement,
// not a circle pair; an unconcluded edge draws the dashed form. Labels follow the same topmost
// anchor and "arc in view" precedents as the dispersion pair.
void DrawReferenceArcs(ImDrawList* dl, const SurveyOverlayFacts& facts, const ReferenceArcInput& in);

// ---- The reference profile's hover device (contract §16.1.3's extension) ----------------------------

struct SurveyProbeView;  // gui/analyze_overview.hpp — the probe's readings, by reference

// The profile device's caller-side geometry: the same mirror discipline as VerdictAnchorInput's
// projection quintet (a new MapCanvasProjection field must be added to BOTH sides or the rebuilt
// view silently defaults), with the SURVEY document's own sun echo — the probe frame's premise is
// the survey's observation, not the map's anchor fallback.
struct ProfileHoverInput {
  const ViewProjection* camera = nullptr;
  int width_px = 1;
  int height_px = 1;
  float width_pt = 1.0f;
  float height_pt = 1.0f;
  float pic_min_x = 0.0f;
  float pic_min_y = 0.0f;
  float pic_px = 1.0f;
  SkySunAnchor sun;
  SurveyProfileHover hover = SurveyProfileHover::kNone;
};

// The hovered profile row's reading drawn on the map (the vocabulary's point/interval device):
// the point sits at the named reading's deviation on the radial line the probe sampled — the
// probe's own azimuth in its own frame (engine-side `MakeProbeFrame` from the incident direction,
// whose basis coincides with the map walk's (u, v) by construction, with the azimuth mapping
// φ = π − az; see the cpp's derivation comment) — NOT a whole-circle visibility or global-peak
// claim, and the separation row draws the radial INTERVAL between the two readings, never an
// annulus. Honesty gates, in short-circuit order: no row standing, a refused probe, an unknown
// probe azimuth, or the named readings unavailable draw NOTHING (the row's own words carry the
// absence); an endpoint outside the view leaves the device undrawn rather than bridging across
// the projection break. No legend entry: the device is hover-transient, the driving row is the
// explanation (the contract's sync records the exemption). Surfaced whole for the unit layer's
// draw-list assertions; the map's overlay path calls it with the cache model and the session's
// profile standing.
void DrawProfileHoverDevice(ImDrawList* dl, const SurveyProbeView& probe, const ProfileHoverInput& in);

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
// the wavelength, the caliber block, the sky-point input, the legend), with
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
