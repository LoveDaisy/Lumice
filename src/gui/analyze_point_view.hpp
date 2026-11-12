#ifndef LUMICE_GUI_ANALYZE_POINT_VIEW_HPP
#define LUMICE_GUI_ANALYZE_POINT_VIEW_HPP

// The Analyze workspace's point inspector (742.6): the three faces that consume ONE adopted point
// document — the sun-direction globe (the selected component's fiber projected on the crystal-frame
// u sphere, the current sample marker, the sun grid's deviation backdrop), the raypath 3D view
// (the crystal's true-scale mesh + the current sample's representative-ray polyline, world /
// crystal frame switch), and the right column's point-level detail block (identity echo, component
// list, per-sample factors). The session/query machinery lives beside its siblings in
// analyze_survey.hpp (PointQuerySession / RayTraceSession, the kPoint / kRay faces); this header
// owns the display state and the pure rules over the adopted documents.
//
// Conventions inherited from the map TU (analyze_map_view.hpp):
//   * Sky directions are TRAVEL vectors; (altitude, azimuth) labels are derived at the request
//     builder through the one named conversion the map's coordinate input uses (DirToAltAz,
//     util/sky_direction.hpp) — no second conversion lives here.
//   * Pure rules are frame-free and ImGui-free; the pumps and renderers are the only
//     GuiState-dependent half. The u-globe's projection speaks normalized disk coordinates,
//     [0,1]x[0,1] over the disk's bounding square, (0,0) the top-left, y down — the map's own
//     disk convention.
//
// Honesty rules this file holds itself to (the issue's ACs, spelled where they bind):
//   * The fiber polyline is the SELECTED COMPONENT's valid samples, in arclength order, broken at
//     invalid samples — separate components are never joined into one curve (AC2).
//   * The slider and playback index the component's VALID samples only; playback writes the
//     sample index and nothing else — the sky point intent is untouched by construction (AC3).
//   * A trace polyline is drawn only while its key (point adoption + component + sample) IS the
//     current sample; anything else displays the named computing / refused state, never the
//     previous pose's polyline (AC4). The polyline itself is the 742.2 recovery's contacts and
//     legs — no GUI-side intersection or segment-direction synthesis (AC4).
//   * Unavailable quantities (jacobian, integral) display as named unavailable states — never a
//     zero, never a normalization that hides the gap (AC3).

#include <string>
#include <vector>

#include "gui/analysis_panel.hpp"
#include "gui/analyze_survey.hpp"
#include "imgui.h"
#include "include/lumice_editor.h"  // LUMICE_CrystalMesh — the contact-facing lookup's mesh type
#include "include/lumice_raypath.h"

struct ImDrawList;
struct ImVec2;

namespace lumice::gui {

struct GuiState;  // gui/gui_state.hpp — the pumps and renderers take it; this header stays off
                  // its include chain so GuiState can hold the types below.
struct CrystalConfig;

// ---- Request-side constants (the M0 tier; the reasoning lives next to the executor's deadline) ----

// Every point request samples the discovery at the engine's default tier: the M0 probe measured
// the wall clock FLAT across sample counts (1k .. 1M) — the seed search is not the cost — so the
// default buys the best seeds for free. The caliber line echoes the count from the result's Info.
inline constexpr int kPointQuerySampleCount = 1000000;
// The sun grid rows carried for the deviation backdrop (45x90 cells). Also measured cost-flat
// (grid 0 vs 90: same wall clock); 45 rows is ample for a backdrop a few hundred px across.
inline constexpr int kPointSunGridLatCount = 45;

// ---- Display state ----------------------------------------------------------------------------------
// The display cursor (PointViewState) and the playback tick live beside the sessions they read,
// in analyze_survey.hpp — GuiState holds the view by value and this header sits above that
// include chain.

// ---- Request derivation -----------------------------------------------------------------------------

// The engine request for an outstanding point intent (the map request builder's sibling): the
// key's selection fills the literal-sequence class fields (R1 — no second representation of the
// selection exists), the sky point fills the target labels through DirToAltAz (the ONE named
// conversion), λ and the M0 tier fill the rest. Buffers move into the executor's job. A zero
// wavelength is carried as 0.0 — the engine's reference resolution (the map's shared knob).
LUMICE_SinglePathRequest MakePointRequestForKey(const PointIntentKey& key, std::vector<int>* faces,
                                                std::vector<int>* counts);

// The representative-ray request for one adopted point sample (the R5 shape): the selection's
// faces, the sample's pose VERBATIM (742.2's recovery binds to the request's member/pose pair),
// and the session's λ (0/NULL = the engine's reference rule). Buffers move into the executor's
// job; `wavelengths` stays empty for the reference resolution.
LUMICE_RepresentativeRayRequest MakeRayRequestForSample(const SelectedClassIdentity& identity, const double pose[9],
                                                        double wavelength_nm, std::vector<int>* faces,
                                                        std::vector<int>* counts, std::vector<double>* wavelengths);

// ---- Reading the adopted point document (pure over the C readers) -----------------------------------

// One component row of the component list: the reader's own words, nothing recomputed.
struct PointComponentRow {
  std::string kind;          // the component kind name (e.g. "closed", "arc")
  std::string forward_text;  // "<status>" or "<status> (<reason>)" — the trace end's own words
  std::string backward_text;
  int point_count = 0;
  int available_points = 0;  // the integral's completeness pair, verbatim
  int total_points = 0;
  bool integral_available = false;
  double fiber_integral = 0.0;  // meaningful iff integral_available
};

// Reads the adopted document's components into display rows. Returns the component count
// (0 for a mass outcome — the caller renders the mass arm instead).
int PointComponentRows(const LUMICE_SinglePathResult& result, std::vector<PointComponentRow>* rows);

// The selected component's valid-sample bookkeeping: how many valid samples it carries, and the
// STORED row index of the `valid_position`-th valid one (the slider's mapping; -1 when out of
// range). The samples are read in the component's own arclength order — no reordering here.
int PointValidSampleCount(const LUMICE_SinglePathResult& result, int component);
int PointValidSampleIndexAt(const LUMICE_SinglePathResult& result, int component, int valid_position);

// The same map in ONE pass — out[v] is the stored row index of the v-th valid sample — for the
// render paths that used to pay a full scan per sample per frame. The document is immutable
// behind the adoption gate, so a per-frame build is consistent; anything hotter caches it above.
std::vector<int> CollectValidSampleIndices(const LUMICE_SinglePathResult& result, int component);

// The per-sample factor lines (R4's vocabulary): the current sample's orientation angles, rho,
// A_P and A/a^2, T (total), J with its named-unavailable state, K = 1/(J+eps), the product
// rho*A/a^2*T*K, and the per-interface transmittances labelled as conditional-on-this-path —
// the engine's names and units, never an additive percentage, never a fabricated number.
std::vector<std::string> PointFactorLines(const LUMICE_PathPointSample& sample);

// The two §16.1 ladder halves of the factor readouts (contract §16.4's factors region): the
// head lines are the default layer's factor conclusions (pose, rho, A_P/A_a^2, T, J/K/product
// or their named-unavailable forms); the interface lines are T's optical premises — the
// per-interface transmittances, conditional on this path — the Interface transmission fold's
// rows. The production callers are the factors region's two arms (default layer + fold); the
// concatenated spelling below survives as the partition's single-source anchor and the tests'
// consumer (unit-correctness pins the per-state spellings, the functional chain asserts the
// engine-facing read) — the halves and the concatenation cannot drift apart because the
// concatenation IS their composition.
std::vector<std::string> PointFactorHeadLines(const LUMICE_PathPointSample& sample);
std::vector<std::string> PointFactorInterfaceLines(const LUMICE_PathPointSample& sample);

// The identity/echo lines (AC1): the result's OWN Info read back — target labels + deviation,
// member faces, crystal id, wavelength + its source, refractive index, sample tier and budget
// echo. The engine's echo, not a GUI restatement of the request.
std::vector<std::string> PointCaliberLines(const LUMICE_PathPointInfo& info);

// The rank-0 mass arm's lines (AC5): direction labels, target separation, and the orbit factors'
// own availability words (support kind, kept/total, integral) — presented WITHOUT a fiber, never
// forced into the discovered arm's widgets.
std::vector<std::string> PointMassLines(const LUMICE_PathPointMass& mass);

// ---- The contribution curve (the Pose & trace window's D5 lower region, 756) ------------------------
// The multi-factor curve along ONE component's real SO(3) arclength — rho, A/a^2, T, K and their
// product read verbatim from the adopted document's per-pose payload (the audit's verdict:
// `doc/raypath/fiber-arclength-payload-audit.md`; drawing needs no engine change). The
// consumption contract this model implements (audit §5): the axis is built from the component's
// own increments BEFORE any filtering (a point keeps its original index and x whatever its
// availability), availability is decided by the gate flags ONLY (never a value comparison —
// the flag-clear cell shows the BeginRead zero, the valid-fail cell shows storage's NaN copied
// through), and the angle-Lebesgue caliber is declared, not inferred: on plate-family scenes
// rho — and with it the product and the integral — read identically zero (a legal declared
// zero, not absence), while the other arms stay informative per their own flags.

// One arm's honest state at one point: a finite positive value (drawn on the log axis), an
// exact zero the engine itself produced (a plate's declared-measure rho, a legal zero-area
// endpoint — marked on the dedicated zero row, never mapped onto log), or unavailable (the
// gate flag clear, or the value is not drawable: non-finite, negative) — a gap, never a zero.
enum class PointCurveValueKind { kUnavailable, kExactZero, kPositive };

// One arm's classification: `gated` carries the arm's own availability predicate (rho: always
// produced; A/a^2 and T: `valid`; K: `jacobian_available`; product: both — audit §3's matrix).
// Behind a true gate the value decides positive/zero; a non-finite or negative value is
// physically impossible here and names unavailable instead of drawing.
PointCurveValueKind PointCurveClassify(double value, bool gated);

// One stored point of the curve: its axis position plus the five arms, each with its own
// three-state classification, and the point's own gate flags (the renderer's gap/cursor
// bookkeeping).
struct PointCurvePoint {
  double x_rad = 0.0;  // SO(3) arclength from the component's own zero (audit §5.1's construction)
  double rho = 0.0;
  double area = 0.0;          // A/a^2
  double transmission = 0.0;  // T (total)
  double k_carea = 0.0;       // K = 1/(J+eps)
  double product = 0.0;       // rho*A/a^2*T*K
  PointCurveValueKind rho_kind = PointCurveValueKind::kUnavailable;
  PointCurveValueKind area_kind = PointCurveValueKind::kUnavailable;
  PointCurveValueKind transmission_kind = PointCurveValueKind::kUnavailable;
  PointCurveValueKind k_kind = PointCurveValueKind::kUnavailable;
  PointCurveValueKind product_kind = PointCurveValueKind::kUnavailable;
  bool valid = false;
  bool jacobian_available = false;
};

// The selected component's contribution curve, built per frame from the adopted document
// through the C readers (the document is immutable behind the adoption gate — the same
// per-frame-build rule the valid-index map follows). `ok` false: outside the discovered arm
// or the component did not read — the caller renders the honest state, never a fabricated axis.
struct PointContributionCurve {
  bool ok = false;
  // The component's identity and completeness, its own words verbatim (the integral echo is
  // the engine's left-endpoint rule's answer — the GUI re-displays it, never recomputes).
  std::string kind;
  int seed_index = -1;
  int available_points = 0;
  int total_points = 0;
  bool integral_available = false;
  double fiber_integral = 0.0;
  // The stored points, in arclength order; size == the component's point_count when ok.
  std::vector<PointCurvePoint> points;
  // The v-th valid sample's stored index (the cursor's domain — the same walk the slider and
  // the globe marker follow, so one selection owns every view).
  std::vector<int> valid_positions;
};

bool BuildPointContributionCurve(const LUMICE_SinglePathResult& result, int component, PointContributionCurve* out);

// The stored index of the v-th valid sample over the curve's own walk (-1 out of range).
int PointCurveStoredAt(const PointContributionCurve& curve, int valid_position);
// The arclength of the v-th valid sample (0.0 out of range).
double PointCurveXAt(const PointContributionCurve& curve, int valid_position);
// The nearest VALID sample's position to `x_rad` — the canvas click/drag's inverse mapping.
// Invalid samples are unreachable (the slider's domain); ties pick the lower position.
int PointCurveValidPositionNearX(const PointContributionCurve& curve, double x_rad);

inline constexpr ImU32 kPointCurveProductColor = IM_COL32(250, 250, 250, 255);
inline constexpr ImU32 kPointCurvePlotBorderColor = IM_COL32(90, 94, 102, 120);

// The shared axis pools only kPositive values across all five arms. The nearest-rank P1
// lower bound and the relative noise floor keep tiny tails from consuming the log axis.
inline constexpr double kCurveAxisLowerQuantile = 0.01;
inline constexpr double kCurveAxisNoiseRatio = 1e-15;
// A band within one decade has no extreme tail to trim; preserve its original endpoints.
inline constexpr double kCurveAxisNarrowRatio = 10.0;
struct PointCurveAxisRange {
  bool valid = false;
  double lo = 0.0;
  double hi = 0.0;
};
PointCurveAxisRange PointCurveAxisFit(const PointContributionCurve& curve);
inline constexpr double kCurveAxisZoomFactor = 1.12;
// Reconcile the view window with the current fit, then zoom around anchor (0 = bottom, 1 = top).
// The window never extends beyond the fit; spans stop at 0.25 decade or the full fit span.
PointCurveAxisRange PointCurveVisibleRange(const PointCurveAxisRange& fit, PointCurveZoom* zoom, double wheel = 0.0,
                                           double anchor = 0.5);

// Counts stored points, not (arm, point) pairs: multiple hidden arms at one point count once.
// The extrema cover only hidden positive values; zeros and unavailable cells never count.
struct PointCurveOutsideCount {
  int points = 0;
  double min = 0.0;
  double max = 0.0;
  bool above = false;
};
PointCurveOutsideCount PointCurveCountOutside(const PointContributionCurve& curve, double lo, double hi);
std::string PointCurveOutsideLabel(const PointCurveOutsideCount& outside);

// One log-axis tick: a decade boundary (major) or its 2x/5x sub-multiple (minor).
struct PointCurveLogTick {
  double value = 0.0;
  bool major = false;
};
// Decades and sub ticks spanning [lo, hi]: majors at every power of ten inside, minors at the
// 2 and 5 multiples. A band degenerate to lo == hi widens to one decade, so a single value
// still draws inside a range rather than on an edge; nothing positive to span answers empty.
std::vector<PointCurveLogTick> PointCurveLogTicks(double lo, double hi);

// The exponent stride between LABELLED decade majors so labels cannot overlap: a real fiber
// can span hundreds of decades (live: 1e-213 .. 1.6), and labelling every decade on a ~200 px
// plot draws hundreds of overlapping texts. Returns 1 when every decade can carry a label;
// above that, only every Nth exponent is labelled and the rest stay unlabelled.
int PointCurveLogLabelStep(double lo, double hi, float plot_h_px, float line_h_px);

// One run of consecutive stored indices whose arm value is a usable positive: half-open
// [begin, end) over the stored points. Gaps (unavailable or exact-zero points) break runs and
// are never bridged; a run of length 1 is an isolated positive contribution the renderer must
// draw as a point marker — never dropped (AC3/AC6: discrete data stays visible).
struct PointCurveRun {
  int begin = 0;
  int end = 0;
};
// Optional values restrict runs to the inclusive visible window; outside values break runs.
std::vector<PointCurveRun> PointCurvePositiveRuns(const PointCurveValueKind* kinds, size_t count,
                                                  const double* values = nullptr, double lo = 0.0, double hi = 0.0);

// ---- The u-globe (crystal-frame sun-direction sphere) -----------------------------------------------

// The disk projection of the crystal-frame sun-direction sphere: Lambert azimuthal equal-area
// about a centre fixed PER COMPONENT (the first valid sample's u — deterministic from the
// document, stable while the slider and playback move), the map's rho = 2 sin(theta/2) family
// normalized so the disk's rim is the ANTIPODE (the whole sphere maps into the disk). Coordinates
// are the map's: [0,1]^2 over the bounding square, (0,0) top-left, y down.
struct SunGlobeProjector {
  double centre[3] = { 0.0, 0.0, 1.0 };
  double up[3] = { 0.0, 1.0, 0.0 };
  double right[3] = { 1.0, 0.0, 0.0 };
};

// The component-fixed projector, read from the adopted document. ok=false when the component
// carries no valid sample (no fibre to anchor on) — the caller renders the honest empty state.
bool MakeSunGlobeProjector(const LUMICE_SinglePathResult& result, int component, SunGlobeProjector* out);

// The crystal-frame direction `u` -> normalized disk coordinates. Always answers (the whole
// sphere is in bounds; the antipode collapses to the rim).
void SunGlobeProject(const SunGlobeProjector& projector, const double u[3], double* out_x, double* out_y);

// One sun grid cell's backdrop colour, RGBA8, from the grid's own deviation (t = delta/pi into
// the map's dark-to-light ramp family); cells the grid marks invalid get alpha 0 — the globe
// shows data, never a painted zero (the map's rule).
uint32_t SunGridCellColorRgba(double deviation_rad, uint8_t valid);

// ---- The representative-ray display -----------------------------------------------------------------

// Canvas and legend share the segment palette; frame tests inspect the submitted draw primitives.
inline constexpr ImU32 kRayTraceContactColor = IM_COL32(240, 240, 240, 190);       // hollow contact markers
inline constexpr ImU32 kRayTraceContactSolidColor = IM_COL32(240, 240, 240, 255);  // front-facing contact markers
inline constexpr ImU32 kRayTraceLabelColor = IM_COL32(210, 210, 214, 255);         // per-segment ratio labels
inline constexpr ImU32 kRayTraceSegmentColors[] = { IM_COL32(90, 160, 255, 255), IM_COL32(255, 165, 60, 255),
                                                    IM_COL32(90, 220, 120, 255) };

// The width ends of the cumulative-Fresnel visual mapping, in UI layout units (UiPx at the use
// site; 2026-11-12 feedback #8's calibration). The mapping is w_min + (w_max - w_min) * sqrt(E)
// over the positive energies — strictly monotone, low values stay readable. The width is ONLY
// visual scaling; the per-segment numbers (when shown) carry the exact ratio. A segment past an
// unusable interface factor draws at the fixed neutral width — no energy claim.
inline constexpr float kRayTraceWidthMinUi = 0.75f;
inline constexpr float kRayTraceWidthMaxUi = 3.5f;
inline constexpr float kRayTraceWidthNeutralUi = 2.0f;

// One segment's cumulative Fresnel relative energy: the running product of the interface factors
// already passed (the entry leg carries 1.0 — nothing absorbed yet; the exit leg the full-chain
// product). `valid` false from the first unusable factor onward — the prefix stays honest, the
// suffix carries no energy claim. An exact zero is a VALID energy (the engine produced it): no
// ink, the contact markers stay, the label reads 0%.
struct RayTraceSegmentEnergy {
  float value = 1.0f;
  bool valid = true;
};

// The polyline the 3D view draws, straight from the 742.2 readers: entry leg (incident direction,
// drawn from outside), the contacts' internal legs, the exit leg. The incident/exit segments'
// display length is THIS file's presentation convention (the result carries physical quantities
// only): `extension` times the contacts' own spread. Segment kinds index into the points array:
// 0 = entry, 1 = internal, 2 = exit. seg_energy is parallel to seg_kinds.
struct RayTracePolyline {
  std::vector<float> points;                      // xyz triples, world frame
  std::vector<int> seg_kinds;                     // one per segment, the kinds above
  std::vector<RayTraceSegmentEnergy> seg_energy;  // one per segment, the energies above
};

// The cumulative relative energies for the N+1 segments over the N interface factors (E[0] = 1,
// then E[k+1] = E[k] * q[k] — every interface consumed exactly once, the last segment including
// the exit factor). A factor is unusable when non-finite or outside the rounding band
// [-1e-6, 1+1e-6]: in-band overshoot clamps to [0,1] (a recorded rounding tolerance, not a
// physics rewrite), and everything from the first unusable factor on is marked unavailable.
// Pure; the gui_test's hand anchors live in the unit test.
std::vector<RayTraceSegmentEnergy> RayTraceCumulativeEnergies(const double* factors, size_t count);

// The visual line width (live UI scale) for a cumulative relative energy: the fixed sqrt
// monotone mapping over the positive range; E <= 0 answers 0 — the renderer draws no ink for a
// zero-energy segment. Not a physical scale: the legend says so.
float RayTraceEnergyWidth(float energy);

// The per-segment ratio label's text: the exact cumulative value as a percentage with enough
// precision for tiny non-zero values (never a rounded-to-zero non-zero), "0%" for an exact zero,
// "n/a" past an unusable factor.
std::string RayTraceEnergyLabelText(const RayTraceSegmentEnergy& energy);

// One contact's outward face normal, read from the mesh's per-face table by face NUMBER (the
// raypath filter numbering; never an array index — the table is scanned). `known` false for a
// negative number (kInvalidId), a number the table does not carry, an unpopulated table, or a
// missing/degenerate normal — the renderer's hollow fallback ("facing unknown" in the legend).
struct RayTraceContactFaceNormal {
  bool known = false;
  float normal_body[3] = { 0.0f, 0.0f, 0.0f };
};
RayTraceContactFaceNormal RayTraceContactFaceNormalFor(const LUMICE_CrystalMesh& mesh, int face_number);

// The facing sign rule: dot(display-frame normal, eye - contact) over the scene metric —
// positive faces the camera (solid marker), zero or negative faces away or is tangent (hollow).
// Pure; the caller supplies the real camera's eye position.
bool RayTraceContactFacesEye(const float normal_display[3], const float contact_scene[3], const float eye_scene[3]);

// Builds the display polyline from the handle's readers. ok=false on a refusal arm (the callers
// render the named refusal instead — GetContacts/GetLegs answer NOT_AVAILABLE there).
bool BuildRayTracePolyline(const LUMICE_RepresentativeRay& ray, float extension, RayTracePolyline* out);

// World -> crystal frame (the pose is the row-major body->world rotation the metadata echoes):
// the display transform behind the frame switch (AC4's one physical state, two views). Directions
// transform by the same rotation — the polyline's points carry the geometry, its segment kinds
// are frame-independent.
void RayTracePointToCrystalFrame(const double pose[9], float point[3]);

// The trace display's five states (AC4's honesty set): nothing to show (no adopted point, or the
// mass arm), the named computing state, the drawn polyline, the named refusal (inside the
// handle), and the engine's request-level error text.
enum class RayTraceDisplayState { kHidden, kComputing, kDrawn, kRefused, kError };
// `current_key` is the sample the display wants RIGHT NOW; the state is computed against the
// session's own key — a stale pose's polyline is never drawn (the gate).
RayTraceDisplayState RayTraceDisplayStateFor(const RayTraceSession& trace, const RayTraceKey& current_key);

// ---- The raypath 3D orbit camera ---------------------------------------------------------------------

// The 3D region's orbit basis in the display frame, z-up (the crystal's c-axis
// display convention): `yaw_deg` sweeps the horizontal plane from +x toward +y, `pitch_deg`
// rises from that plane toward +z (clamped to ±88°, short of the pole). `fwd` is the view
// direction, from the viewer INTO the scene — the eye and the visible near face sit on the
// −fwd side, and with `up = right × fwd` the basis satisfies right × up = −fwd. `right`/`up`
// are the screen axes, `right` a function of the yaw alone — exactly (sin yaw, −cos yaw, 0).
// The drag signs are the caller's grab mapping (doc/geometry/coordinate-convention.md §9.8
// has the three-camera table). Exposed — not a TU-local helper — so the gui_test pin reads
// the SAME basis the renderer draws with (the convention's single authority; the test never
// re-derives it).
void OrbitBasis(float yaw_deg, float pitch_deg, double fwd[3], double right[3], double up[3]);

// The last submitted GL canvas frame, invalidated on every region submission before any early
// return. Tests independently project the frozen C-API mesh using these exact render parameters.
struct Ray3DFrameInfo {
  float mvp[16] = {};
  ImVec2 canvas_min;
  ImVec2 canvas_size;
  float centre[3] = {};
  float fit = 1.0f;
  double pose[9] = {};
  int pool = -1;
  int frame_count = -1;
  uintptr_t texture_id = 0;
  bool crystal_frame = false;
  float zoom = 0.0f;  // the frame's effective zoom — ComputeDist/ComputeNearPlane's input, so
                      // the frame tests can place the real camera (eye = -dist * fwd) and the
                      // real near plane without a second distance formula
  bool valid = false;
};
const Ray3DFrameInfo& LastRay3DFrameInfo();

// ---- The pumps and renderers (GuiState-dependent, defined in the .cpp) ------------------------------

// The point face's frame pump — the map pump's sibling: drains the executor's point mailbox
// through the adoption gate, then keeps the intent level-triggered against the live selection +
// sky point + wavelength. A key change DROPS the adopted result (a newer sky point's answer must
// not linger under the new target — AC5) and issues the fresh intent. `now_s` is the frame
// clock, stamped onto each new intent as its wait-display epoch (the survey pump's rule). Writes
// only state.analysis's point fields, the point view's adoption reset, and the executor.
void PumpPointQuery(GuiState& state, SurveyExecutor& executor, double now_s);

// The ray face's frame pump: drains the ray mailbox, then keeps the ray intent level-triggered
// against the CURRENT point adoption + component + sample. No point adoption (or the mass arm)
// means no ray intent — the session is dropped, the 3D view shows its honest state.
void PumpRayTrace(GuiState& state, SurveyExecutor& executor);

// The main window's point summary (contract §16.2's summary projection): the four visible
// states with their cancel actions, then — completed — the L1/L2 reads of the document: the
// partial limit, the target line, the rank-0 answer, the component count with its availability
// limits and the no-valid-sample fact. The complete answer lives in the Raypath view.
void RenderPointDetailsBlock(GuiState& state, const SelectedClassIdentity& identity);

// The Raypath view's sky-point answer (contract §16.2's complete-answer host): the adopted
// document read in full — target, λ, the rank-0 mass arm, the component evidence rows, the
// reach and completeness conclusions — sorted into the same default layer plus fold groups the
// survey answer uses (its own `##pt_answer` suffixed pair, the same pair on every arm). An
// adopted document draws its content, including across a re-query's in-flight window; the
// four-state shell and its cancel actions stay in the main window's summary.
void RenderPointAnswerBlock(GuiState& state);

// The point session's one-line state word — the short status RenderPointStatus draws, and the
// main window's Selection header hint reuses (the mapping has one owner, two readers).
// *partial, when non-null, is set iff the word is the partial completion's: the status row
// paints that word in the warning color, and the completed read happens inside so the word and
// its flag cannot disagree. *completed_info, when non-null AND the state is completed, receives
// the same Info read the word was derived from — the caller's classification facts
// (outcome / component_count) come from that one read instead of a second engine pass.
const char* PointStatusLabelFor(const PointQuerySession& point, bool* partial = nullptr,
                                LUMICE_PathPointInfo* completed_info = nullptr);

// The sun-direction globe region — the ##AnalyzeSunGlobe placeholder's replacement: the u-sphere
// disk (deviation backdrop when the document carries the grid, the selected component's fiber
// polyline, the current sample marker), the weight bar, the slider and the play/stop row.
void RenderAnalyzeSunGlobeRegion(GuiState& state, const ImVec2& size);

// The raypath 3D region — the ##AnalyzeRaypath3D placeholder's replacement: the crystal's
// true-scale crystal rendered with the shared perspective/x-ray renderer + the current sample's
// trace polyline, the world/crystal frame switch, the orbit camera, and the named
// computing/refused/empty states.
void RenderAnalyzeRay3DRegion(GuiState& state, const ImVec2& size);

// The Pose & trace window (analyze-interaction-contract.md §7, 755.5): the free-floating
// non-modal window that gathers the sky point's three faces — the sun-direction globe, the
// raypath 3D view and the current sample's factor readouts — around ONE adopted point document.
// Opened by the named entry buttons only — the main window's Selection header and the Raypath
// view window's in-place one (§16.3; closed by its own title bar X / DoNew's whole-state
// reset); the sky-point and wavelength writers never open it. The open flag
// and the two split ratios are T-session display state (GuiState::analyze_pose_view_*). The
// frame pump is NOT this window's concern — it runs at RenderWorkspacePanels' entry on every
// GUI frame; this function keeps only the window skeleton (size recipe, minimum constraint)
// and the fiber wait-feedback field's clear on the Render-workspace frames it is the sole
// render site for. Rendering is a no-op while the flag is false.
void RenderAnalyzePoseViewWindow(GuiState& state);

// The crystal config the frozen scene binding was built from, by the snapshot's own pool slot —
// the 3D region's mesh source: the geometry the trace ran against, so a post-Analyze edit of the
// live document cannot rebuild the mesh into a shape the polyline no longer lies inside (AC4's
// one physical state). Null when nothing was analyzed or the slot is out of the snapshot's
// range — the caller renders the honest note instead of a wrong mesh.
const CrystalConfig* FrozenCrystalForPool(const GuiState& state, int pool);

}  // namespace lumice::gui

#endif  // LUMICE_GUI_ANALYZE_POINT_VIEW_HPP
