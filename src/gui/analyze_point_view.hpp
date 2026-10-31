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

// The identity/echo lines (AC1): the result's OWN Info read back — target labels + deviation,
// member faces, crystal id, wavelength + its source, refractive index, sample tier and budget
// echo. The engine's echo, not a GUI restatement of the request.
std::vector<std::string> PointCaliberLines(const LUMICE_PathPointInfo& info);

// The rank-0 mass arm's lines (AC5): direction labels, target separation, and the orbit factors'
// own availability words (support kind, kept/total, integral) — presented WITHOUT a fiber, never
// forced into the discovered arm's widgets.
std::vector<std::string> PointMassLines(const LUMICE_PathPointMass& mass);

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

// The polyline the 3D view draws, straight from the 742.2 readers: entry leg (incident direction,
// drawn from outside), the contacts' internal legs, the exit leg. The incident/exit segments'
// display length is THIS file's presentation convention (the result carries physical quantities
// only): `extension` times the contacts' own spread. Segment kinds index into the points array:
// 0 = entry, 1 = internal, 2 = exit.
struct RayTracePolyline {
  std::vector<float> points;   // xyz triples, world frame
  std::vector<int> seg_kinds;  // one per segment, the kinds above
};

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

// ---- The pumps and renderers (GuiState-dependent, defined in the .cpp) ------------------------------

// The point face's frame pump — the map pump's sibling: drains the executor's point mailbox
// through the adoption gate, then keeps the intent level-triggered against the live selection +
// sky point + wavelength. A key change DROPS the adopted result (a newer sky point's answer must
// not linger under the new target — AC5) and issues the fresh intent. Writes only
// state.analysis's point fields, the point view's adoption reset, and the executor.
void PumpPointQuery(GuiState& state, SurveyExecutor& executor);

// The ray face's frame pump: drains the ray mailbox, then keeps the ray intent level-triggered
// against the CURRENT point adoption + component + sample. No point adoption (or the mass arm)
// means no ray intent — the session is dropped, the 3D view shows its honest state.
void PumpRayTrace(GuiState& state, SurveyExecutor& executor);

// The right column's point-level block — the placeholder's replacement under the class overview:
// the four visible states, the echo lines, the mass arm, the component list (selectable), the
// current sample's factor lines and interface transmittances, the cancel button (latch, the
// survey's shape).
void RenderPointDetailsBlock(GuiState& state, const SelectedClassIdentity& identity);

// The sun-direction globe region — the ##AnalyzeSunGlobe placeholder's replacement: the u-sphere
// disk (deviation backdrop when the document carries the grid, the selected component's fiber
// polyline, the current sample marker), the weight bar, the slider and the play/stop row.
void RenderAnalyzeSunGlobeRegion(GuiState& state, const ImVec2& size);

// The raypath 3D region — the ##AnalyzeRaypath3D placeholder's replacement: the crystal's
// true-scale wireframe (nominal shape — the analysis geometry's own口径) + the current sample's
// trace polyline, the world/crystal frame switch, the orbit camera, and the named
// computing/refused/empty states.
void RenderAnalyzeRay3DRegion(GuiState& state, const ImVec2& size);

// The crystal config the frozen scene binding was built from, by the snapshot's own pool slot —
// the 3D region's mesh source: the geometry the trace ran against, so a post-Analyze edit of the
// live document cannot rebuild the mesh into a shape the polyline no longer lies inside (AC4's
// one physical state). Null when nothing was analyzed or the slot is out of the snapshot's
// range — the caller renders the honest note instead of a wrong mesh.
const CrystalConfig* FrozenCrystalForPool(const GuiState& state, int pool);

}  // namespace lumice::gui

#endif  // LUMICE_GUI_ANALYZE_POINT_VIEW_HPP
