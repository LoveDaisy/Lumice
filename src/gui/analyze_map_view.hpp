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

// The rank-0 point-mass semantic colour (the map's own mass cells and the adopted point query's
// point-image marker — one rank-0 semantics, one ink, so the picture never carries two hues for
// one fact). Same discipline as the pair above: one value, two consumers (the draw and the
// test), a compile-time pin against the original literal.
inline constexpr unsigned int kPointMassColour = (255u << 24) | (74u << 16) | (184u << 8) | 250u;

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
  // The scope relation: 1 while the adopted map's member set (the request's true P/B/D bits)
  // differs from the survey overlay's (the selected row's literal representative) — exactly the
  // partial tick sets. Only a real difference draws the in-place scope note; the bits ride the
  // input so the note names what THIS map evaluated, never the current tick state of a late frame.
  uint8_t map_symmetry_bits = 7;
  bool survey_scope_differs = false;
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

// The grouped hover presentation, independent of marker projection and hit collection. The
// noun names what the count line counts ("object" for the plain rows; a caustic pile's hover
// says "representative record" — the same count the pile's ×N badge shows).
void RenderSurveyObjectTooltip(const std::vector<const SurveyObjectRow*>& hits, const char* record_noun);

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
  // The adopted point query's rank-0 image (the point mass): true when the image is inked this
  // frame — by the dedicated marker, or by the adopted map's own POINT_MASS cell when the two
  // documents match (the reuse branch draws no second symbol).
  bool point_image = false;
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
  bool family_curves = false;          // any family fold arc / track submitted ink
  int family_curve_count = 0;          // how many curves submitted (one entry counts them)
  // The reference calibers the drawn arcs quote, recorded with the bits so the legend keys can
  // name them: the support/edge bounds read at the survey document's own scope wavelength (a
  // FIXED reference, whatever the map's λ slider shows), the turn pair at the chromatic pair's
  // two wavelengths. Empty when the document carries no caliber — the entry then says unknown.
  std::string arc_lambda_text;
  std::string tir_lambda_text;
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
// The points group's per-kind sub-gates (AC4): the positioned image kinds keep independent
// visibility inside the group — regular and collapsed images are separate names that stay
// separately switchable, whatever happens to overlap on screen. The positioned caustic rows are
// NOT in this group: they are the sixth layer group's own payload (the member below). Any other
// positioned kind follows the parent gate alone.
struct SurveyPointKindGates {
  bool source_image = true;
  bool regular_image = true;
  bool family_collapse = true;
};
struct SurveyLayerGates {
  SurveyLayerGate reach;   // support-range bounds
  SurveyLayerGate edge;    // sharp-edge positions (the deviation-axis reference arcs)
  SurveyLayerGate curves;  // the family routing's ordered fold arcs / regular tracks (the
                           // drawable critical arcs; a card carrying a curve hides its
                           // aggregate representative marker while this group is on)
  SurveyLayerGate points;  // positioned object markers (labels gate the markers' short
                           // standing labels; the hover tooltip is the group's detail channel)
  SurveyPointKindGates points_kinds;
  SurveyLayerGate caustic;    // the positioned caustic representatives — a sixth group, NOT a
                              // points sub-gate: the pile's declutter switch must not ride the
                              // Point images parent (its own graphics/labels pair; hiding the
                              // images keeps the representatives on view and the reverse)
  SurveyLayerGate chromatic;  // the red-blue group: the dispersion pair and the verdict anchor
  SurveyLayerGate tir;        // internal-reflection turn references
};

// The ONE points-row visibility rule (the draw loop, the hit collection and the legend read it
// through this single predicate — no second kind table): the positioned caustic rows through
// their OWN group's gate (a points sub-gate would let the Point images switch bury the caustic
// layer), and the points group's rows through the parent gate plus the three positioned image
// kinds' sub-gates. Surfaced so the unit layer holds the mapping exhaustively and the real
// family documents' rows at the composition tier.
struct SurveyObjectRow;
bool PointMarkerVisible(const SurveyObjectRow& row, const SurveyLayerGates& gates);

// A frame-projected screen point of one positioned caustic row — the clustering input's plain
// shape (this header stays off imgui.h).
struct OverlayScreenPoint {
  float x = 0.0f;
  float y = 0.0f;
};
// The caustic pile's ONE clustering rule (the display declutter's AC2): complete linkage on the
// frame's screen points — a cluster is a set every pair of whose members lies within `radius`
// points of each other, so a chain of mutually distant representatives can never fold into one
// spanning cluster. Greedy in input order (the model's own row order), deterministic for a
// given input, radius, camera and scale. Display-only: the caller passes already-projected
// visible points and draws one marker per returned group, anchored at the group's first (seed)
// point — a real representative's own position, never an average, so no new sky point is
// invented and the seam's two visible copies stay two markers.
void GroupOverlayScreenClusters(const std::vector<OverlayScreenPoint>& points, float radius,
                                std::vector<std::vector<size_t>>* clusters);

// The cluster's one evidence rule (AC2's "aggregation must not upgrade evidence"): a pile may
// draw solid only when EVERY member's claim concluded — certified or finite; any refused or
// unevaluated source keeps the whole pile on the conservative outline, and the count never
// reads as confidence. Empty input is vacuously true by the all-of's own semantics, and the
// clustering above never produces an empty group; the unit layer pins both the mixed-evidence
// refusals and the concluded pile.
bool OverlayClusterAllConcluded(const std::vector<int>& states);

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
  // The fixed reference calibers the arcs quote (the caller reads them off the cached model):
  // the survey document's own scope wavelength for the reach/edge bounds, and the chromatic
  // pair's wavelength words for the turn pairs. Unavailable scopes leave the legend's caliber
  // wording at "unknown" rather than borrowing the map's λ.
  float reference_lambda_nm = 0.0f;
  bool reference_lambda_available = false;
  std::string tir_lambda_text;
  // The selected inner-edge object (817.2): the qualified edge card's object index, or -1 for
  // none. The owned stroke draws its selected feedback (thicker ink, the object's readout with
  // the light-outside wording and the outward hint tick) — the answer's selection and the map's
  // ink are one state read by two consumers.
  int selected_edge_object_index = -1;
  // The frame's submitted-stroke click surfaces (the stationary-click selection's input; null =
  // no collection). A stroke records only the segments the walk actually submitted this frame —
  // post-clip, screen points — so a part of the circle no line covered cannot be picked.
  std::vector<struct ReferenceArcStroke>* stroke_sink = nullptr;
};

// One submitted reference-arc stroke's click surface: whose stroke it is, and the segments the
// walk actually drew this frame as flat screen-point quadruples (x0, y0, x1, y1, ...). Plain
// floats — this header stays off imgui.h. Frame-transient display state: filled by the draw
// pass, read by the same frame's click handling, never stored. `carries_edge_identity` is the
// coincidence association (the edge→support-bound pairing the producer's own declaration
// determines, built before any gate): the shared stroke carries the object while EITHER owner's
// gate shows it — hiding an owner changes visibility, never the object identity, and no numeric
// comparison ever re-derives the pairing. `owned_by_edge` is the display ownership the legend
// and labels consume, which DOES follow the gates.
struct ReferenceArcStroke {
  bool carries_edge_identity = false;
  bool owned_by_edge = false;
  bool owned_by_reach = false;
  size_t edge_object_index = 0;
  std::vector<float> segments;
};

// The stationary-click selection's distance test: squared point-to-segment distance in points,
// compared against a squared tolerance. Pure — the unit layer drives the geometry directly.
float PointSegmentDistanceSq(float px, float py, float ax, float ay, float bx, float by);
bool ArcStrokeHit(const ReferenceArcStroke& stroke, float px, float py, float tolerance_sq);

// The selected stroke's light-side tick, computed in SKY space: the anchor is the circle's own
// topmost in-disk sample (the walk's parameterization and continuity rules), and the tip is the
// SAME circle azimuth at a larger deviation — a step outward on the sphere, which is the light
// side by the support's own geometry — projected through the shared projection chain. Surfaced
// whole for the unit layer's direction regression: a screen-space sun-to-anchor direction is
// wrong exactly where the wording matters (under the dual-fisheye family the projected circle
// is not centred on the projected sun, and the tick would point into the dark side). `eps_deg`
// is the step that read (the draw scales the tip, not the direction); `drawn` false when no
// step carried — the words carry the side then, never a wrong geometry.
struct LightSideTick {
  bool drawn = false;
  double anchor_x = 0.0;
  double anchor_y = 0.0;
  double tip_x = 0.0;
  double tip_y = 0.0;
  double eps_deg = 0.0;
};
LightSideTick LightSideTickFor(const ViewProjection* camera, int width_px, int height_px, float width_pt,
                               float height_pt, float pic_min_x, float pic_min_y, float pic_px, const SkySunAnchor& sun,
                               double delta_deg);

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

// The inner-edge selection's liveness (817.2): the stored selection names the survey document
// and intent it was made under, and is live only while that exact adopted identity stands — a
// class switch, a new generation or a lost document reads false (the binding, not a timestamp).
// Selection changes never touch a query; this predicate is the only way anything reads it.
bool InnerEdgeSelectionActive(const GuiState& state);
// Commits the selection against the CURRENT adopted survey identity (the caller has checked the
// object exists): display-session writes only — no query, no sky point, no camera. The one
// writer of the selection fields.
void SelectInnerEdgeObject(GuiState& state);

// ---- The family curve selection (task arc-recovery-engine) ------------------------------------------

// WHAT a curve hit was: the pick carries the event's own identity, never a bare point index —
// a chain point reads the line's nearest endpoint, a point-image run reads the collapsed
// marker's segment, a cusp candidate reads the tick's event. The kinds' witnesses render from
// the same adopted document's facts (no re-read, no recompute).
enum class FamilyCurvePickKind { kPoint, kPointImage, kCusp };
struct FamilyCurvePick {
  int curve_index = -1;
  FamilyCurvePickKind kind = FamilyCurvePickKind::kPoint;
  int point_index = -1;    // kPoint: the order position the line's nearer end sits at
  int segment_index = -1;  // kPointImage: the collapsed run
  int cusp_index = -1;     // kCusp: the located candidate
  float dist_sq = 0.0f;    // squared points, the pick's own tolerance scale
};

// The selection's liveness: the same adopted-identity binding as the inner-edge selection (the
// document handle + the intent id; a class switch, a new generation or a lost document reads
// false; a window close/open and a camera change keep it). Selection changes never touch a
// query; this predicate is the only way anything reads it.
bool FamilyCurveSelectionActive(const GuiState& state);
// Commits the selection against the CURRENT adopted survey identity: the curve, the picked
// event's identity, and every curve that shared the click (the overlapping-source cycle the
// readout's switcher walks). Display-session writes only — no query, no sky point, no camera.
// The one writer of the selection fields.
void SelectFamilyCurve(GuiState& state, const FamilyCurvePick& pick, std::vector<int> sources);
// Steps the overlapping-source cycle (the readout's ‹ › buttons): slot wraps within the
// committed sources; a selection without a cycle is untouched. Display-session write only.
void CycleFamilyCurveSource(GuiState& state, int step);
// Drops the selection (the readout's Clear). Display-session write only.
void ClearFamilyCurveSelection(GuiState& state);

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
