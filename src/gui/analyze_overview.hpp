#ifndef LUMICE_GUI_ANALYZE_OVERVIEW_HPP
#define LUMICE_GUI_ANALYZE_OVERVIEW_HPP

// The class overview (742.4): the adopted survey document read into a layered presentation — a
// few main conclusions organized by the routing arm the document came from, the chromatic
// dispersion and the reference profile as their own blocks, and the per-object facts behind an
// expander. Two halves, one file:
//
//   * BuildSurveyOverviewModel — PURE. One LUMICE_PathSurvey plus nothing else in; a plain
//     struct out. No ImGui, no GuiState, no recomputation (R8): every number is read back
//     through the typed readers, every label and caliber string is the engine's own text, and
//     the arm classification is a rule over the document's own availability flags. This is the
//     half the unit layer holds to rules without a frame.
//   * RenderSurveyOverviewModel — the ImGui side. Draws the model with the panel idioms;
//     expansion state lives in ImGui per-window storage, never in GuiState (R10: the overview
//     is adopted_result + the current identity, purely rendered). With a non-null readout it is
//     the INTERACTIVE host: the hoverable rows gain their addressable surfaces and this-frame's
//     highlight feedback, and every row's standing lands in the readout (the canvas consumes the
//     session copy next frame). The default null is the main window's summary projection — no
//     new items, no interaction state, byte-identical to its previous shape.
//
// Honesty rules the builder inherits from the survey face (R7): a refused or unevaluated leg
// shows the engine's named reason, an empty reading shows as no data (the probe's empty samples
// arrive as NaN — gaps, never zeros), and "the document stopped early" is stated rather than
// passed off as complete. Availability flags, never defaulted zeros.

#include <cstddef>
#include <string>
#include <vector>

#include "gui/analyze_row_hover.hpp"
#include "imgui.h"
#include "include/lumice_raypath.h"

namespace lumice::gui {

// The typed readers' texts are length-delimited, not null-terminated; the one converter the
// overview and its tests share.
std::string SurveyTextString(const LUMICE_PathText& text);

const char* SurveyStateName(int state);
const char* SurveyOutcomeName(int execution);
const char* SurveyObjectKindName(int kind);

// The routing arm a survey document answers from — the organizing principle of the main
// conclusions. Classification order matters and is the builder's rule: a family document also
// carries support/edge/domain cards, and a refused document can carry produced quantities, so
// the availability flags are read most-specific first.
enum class SurveyArm {
  kIdentity,         // every member rank-0: the source-image certificate + the weight tint
  kFamily,           // a family-restricted measure: the freezing ladder
  kRefused,          // the delta axis (or the measure shape) refused; independent facts beside it
  kFullDimensional,  // the delta machine: support, edge, bright domain, dispersion, tint
};

// One followed-onset row of the dispersion table: where this onset sits at each lambda, and how
// far it moved. Values are the engine's, in degrees.
struct SurveyFollowedOnsetView {
  std::string source;  // the onset's own source/profile names, verbatim
  std::string profile;
  double value_red_deg = 0.0;
  double value_blue_deg = 0.0;
  double displacement_deg = 0.0;
  std::string direction;
};

// The chromatic layer's diagnose half — the weight-kink and gate-edge features. A different
// kernel path from the dispersion table (the two never merge fields), so it is its own view.
struct SurveyDiagnoseView {
  bool available = false;
  std::string verdict_kind;
  std::string color;
  int visible = 0;
  bool position_available = false;
  double position_deg = 0.0;
  bool coverage_complete = false;
  std::vector<std::string> notes;
  std::string evidence_basis;
};

// The chromatic layer's dispersion half (AC2): the engine's fixed pair, the min-deviation row
// when the class has one, and the followed onsets behind it. `evidence_basis` is the engine's
// own geometry-basis string (the口径 the user can read).
struct SurveyDispersionView {
  bool available = false;
  std::string red_label;
  std::string blue_label;
  double n_red = 0.0;
  double n_blue = 0.0;
  bool min_row_available = false;
  double value_red_deg = 0.0;
  double value_blue_deg = 0.0;
  double displacement_deg = 0.0;
  std::string direction;
  std::vector<SurveyFollowedOnsetView> followed;
  std::string evidence_basis;
  // The caustic-edge certificate the basis can point at (present only on a certified edge).
  bool certificate_available = false;
  std::string certificate_formula;
  double certificate_apex_deg = 0.0;
  double certificate_cross_check_deg = 0.0;
  double certificate_tolerance_deg = 0.0;
};

// One of the inner-edge object's four readings: the value with its own evidence-level words, or
// the named absence. An unavailable reading carries its reason — never a zero stand-in, never a
// constant (R7; AC5's 措辞语法承载证据等级 lives in `caliber`).
struct SurveyInnerEdgeReading {
  bool available = false;
  double deg = 0.0;
  std::string caliber;      // the short evidence-level word the reading is stated under
  std::string unavailable;  // why, when !available (the engine's or the read's own named state)
};

// The C01 first-class inner-edge object (the random 3->5 presentation): the qualified corridor's
// named edge with its four readings — the geometric edge (the edge card's certificate), the
// profile's first occupied bin centre and highest bin centre (the built-in probe's representative
// cut), and the fixed chromatic pair's minimum-deviation row. PURE read-back, organized by
// `BuildSurveyOverviewModel` beside the conclusions it feeds.
//
// The qualification gate is the engine's own routing verdict plus the cards' own declarations —
// never a GUI-side guess: the arm must be full-dimensional (the only route the engine enters with
// the uniform random pose family — the rotation invariance the whole-circle wording rests on), and
// a CONCLUDED edge card (certified or finite) must declare coincidence with the support set (the
// productioner's own declaration; the GUI re-derives no tolerance). Any other arm or card state
// leaves `qualified` false and the display at its existing lines — a non-canonical input's
// readable angular distance keeps its own name, never upgraded to the first-class object.
struct SurveyInnerEdgeView {
  bool qualified = false;
  std::string premise_note;    // the whole-sky premise + fixed-λ caliber under the main sentence
  std::string name;            // the corridor's written faces, "3-5" — the representative the probe and the
                               // chromatic legs evaluated (the probe's own member list; the request's
                               // layer falls back when no probe ran)
  int edge_object_index = -1;  // the qualified edge card's object index (the map stroke's owner)
  double edge_deg = 0.0;       // the geometric edge — the object's main number
  bool certificate_available = false;
  bool measure_limit_available = false;
  double measure_limit = 0.0;
  double wavelength_nm = 0.0;  // the survey's fixed reference (the readings' premise; never the
                               // map's λ — AC3's fixed-reference branch)
  double bin_deg = 0.0;        // the profile's actual bin width (the finite-resolution caliber)
  bool member_kind_available = false;
  int member_kind = 0;                 // LUMICE_PATH_MEMBER_* when available — the member scope's own kind
  std::string dispersion_unavailable;  // why the pair is absent, when !dispersion_available
  SurveyInnerEdgeReading first_bin;    // first occupied bin centre on this cut
  SurveyInnerEdgeReading highest_bin;  // highest bin centre on this cut (never a row or all-sky
                                       // peak claim — the caliber says so)
  bool dispersion_available = false;
  double dispersion_red_deg = 0.0;
  double dispersion_blue_deg = 0.0;
  double dispersion_gap_deg = 0.0;
  std::string dispersion_red_label;
  std::string dispersion_blue_label;
  std::string dispersion_direction;  // the engine's raw token ("red_inner_blue_outer")
};

// The interactive host's inner-edge entry wiring (the Raypath view's class answer only; the main
// window's summary passes nothing). Plain values — the session types stay in the callers' TU, so
// this header keeps gui_state.hpp off its include chain. The renderer reads `selected` /
// `map_layer_draws` and writes `select_requested`; the map region applies the click to its
// session the same frame, and the map-side scroll request consumes at the report's own content
// top (the map region's class-answer host), not here.
struct SurveyInnerEdgeInteraction {
  bool selected = false;              // the session selection is live on this document's object
  bool map_layer_draws = true;        // the edge arc's graphics gate stands (drives the hidden-layer
                                      // note; the selection itself is kept regardless)
  bool entry_request_scroll = false;  // in/out: a map-side commit asks the entry scrolled into
                                      // view; consumed at the entry's own item (single reader)
  bool select_requested = false;      // out: the entry's click surface was activated this frame
};

// The survey's built-in solar radial probe: the class's brightness profile at the document's
// reference wavelength, as readings plus the samples themselves. `sample_values` keeps the
// engine's NaN for an empty bin — the renderer and the tests read gaps as gaps.
struct SurveyProbeView {
  bool available = false;
  int state = 0;
  std::string mode;  // a refused probe names why (stop_requested)
  double wavelength_nm = 0.0;
  double refractive_index = 0.0;
  bool azimuth_available = false;  // the profile's line azimuth in the probe's own frame — the
                                   // hover device's spatial premise (the built-in probe always
                                   // carries it: the representative wedge's choice, not a default)
  double azimuth_deg = 0.0;
  bool edge_available = false;
  double edge_deg = 0.0;
  bool peak_available = false;
  double peak_deg = 0.0;
  double peak_value = 0.0;
  bool separation_available = false;
  double separation_deg = 0.0;
  long long kept_count = 0;
  std::vector<double> sample_delta_deg;
  std::vector<double> sample_value;
};

// One rung of the freezing ladder (the family arm's structure), and the whole ladder.
struct SurveyFamilyLevelView {
  int level = 0;
  std::vector<std::string> frozen_dims;  // "zenith 90.0°±1.0°"
  std::vector<std::string> free_dims;
  int samples = 0;
  int valid_samples = 0;
  int objects_proposed = 0;
  int objects_killed = 0;
};

struct SurveyFamilyView {
  bool available = false;
  int state = 0;
  std::string refusal_mode;  // level0_refusal_mode, when the ladder never started
  std::vector<SurveyFamilyLevelView> levels;
};

// The weight tint (either arm's colour question) — the reading plus the engine's caliber string,
// which names the exact口径 and is shown to the user, not paraphrased away.
struct SurveyTintView {
  bool available = false;
  int state = 0;
  bool reading_available = false;
  double ratio = 0.0;
  std::string direction;  // the engine's own words ("blue_tinted")
  std::string caliber;
};

// The refused-support arm's independent positive fact: the declared source's own event, read on
// a lattice that never touches the refused delta axis.
struct SurveyWitnessView {
  bool available = false;
  bool value_available = false;
  double weight = 0.0;
  long long kept_count = 0;
  long long sample_count = 0;
};

// The TIR-share block (document-level, both the full-dimension and the refused arms run it).
struct SurveyTirView {
  bool available = false;
  int state = 0;
  bool value_available = false;
  double share_all_tir_red = 0.0;
  double share_all_tir_blue = 0.0;
  size_t interface_count = 0;
  std::string mode;  // a refused share names why
};

// One row of the object-details expander: the card's identity and state verbatim.
struct SurveyObjectRow {
  std::string kind_name;
  int state = 0;
  std::string state_name;
  bool position_available = false;
  double position_altitude_deg = 0.0;
  double position_azimuth_deg = 0.0;
  int family_level = -1;  // the family arm's level tag; -1 elsewhere
  double blur_deg = 0.0;
  std::string reason;  // the named refusal/unevaluated reason, verbatim
};

// The overview model: the document's facts, organized. Everything is a read-back; nothing is a
// judgment the engine did not make.
struct SurveyOverviewModel {
  // False when the document would not even read — the renderer states that and nothing else.
  bool valid = false;

  // Header facts.
  int execution = 0;
  bool early_stopped = false;
  size_t member_count = 0;
  size_t object_count = 0;
  size_t unevaluated_count = 0;
  size_t probe_count = 0;
  int budget_ms = 0;
  int evidence_state = 0;
  std::string evidence_mode;

  // Scope: the reference wavelength and where it came from, and the sun the positions read.
  bool scope_available = false;
  double wavelength_nm = 0.0;
  std::string wavelength_source;
  double refractive_index = 0.0;
  bool sun_available = false;
  double sun_altitude_deg = 0.0;
  double sun_azimuth_deg = 0.0;

  SurveyArm arm = SurveyArm::kFullDimensional;

  // The main conclusions, in display order — one to a few lines, the layer above the expanders.
  std::vector<std::string> conclusions;

  // The chromatic block's own state when neither half ran: the engine's named reason, verbatim
  // (e.g. "chromatic legs not run: the delta axis refused (...)"). Empty when a leg produced.
  int chromatic_state = 0;
  std::string chromatic_note;

  SurveyDispersionView dispersion;
  SurveyDiagnoseView diagnose;
  SurveyInnerEdgeView inner_edge;
  SurveyProbeView probe;
  SurveyFamilyView family;
  SurveyTintView tint;
  SurveyWitnessView witness;
  SurveyTirView tir;
  std::vector<SurveyObjectRow> objects;
};

// The pure half: survey document in, model out. Reads every block through the typed readers;
// never throws, never computes optics. `point_sun_premise` is the GUI's frozen judgment that
// the analyzed sun is a point source (the session's freeze capture, never the live document):
// the whole-sky inner-edge object's "light begins outside" claim rests on the point sun the
// survey algebra certifies against, so a false premise leaves `inner_edge.qualified` false and
// the display at the existing angular-distance lines — the gate has no default, every caller
// answers.
SurveyOverviewModel BuildSurveyOverviewModel(const LUMICE_PathSurvey& result, bool point_sun_premise);

// The map overlay layers' display facts (the five-group layer plan): the positional payload the
// map's reach/edge/TIR layers draw, read back through the typed readers — a second pure read of
// the same document, NOT an extension of SurveyOverviewModel (the overview's consumers are the
// text hosts; their rows and ordering do not change for the map's sake). Read once per adopted
// identity alongside the overview model, never per frame or per toggle (display switches filter
// these facts; they do not re-read them).
//
// Honesty rules, the builder's own: a refused or undelivered reader writes "not available", never
// a fabricated position (an interval list arrives only from LUMICE_PathSurveyGetSupport); the
// TIR quantity/share readers are calibers, not locators, and no coordinates ever come from them
// — a turn's position exists only where a diagnose feature carries one; a feature the engine
// names not visible, or one whose source is not an internal-reflection discriminant, is not a
// turn the map can draw.
struct SurveyOverlayFacts {
  // The reach layer: every delivered support interval, adjacency-merged (an upper that equals the
  // next lower is one continuous range — the document's own numbers, no epsilon of ours). Bounds
  // are angular distances at the survey's reference wavelength; a range spanning [0, 180] draws as
  // text, not as a circle pair.
  struct ReachInterval {
    double lower_deg = 0.0;
    double upper_deg = 0.0;
  };
  std::vector<ReachInterval> reach_intervals;  // merged, ascending; empty = no delivered support
  bool reach_full_range = false;               // the merged union covers [0, 180]

  // The sharp-edge layer: each delivered edge card's position, with the claim's own honesty
  // fields — the coincidence declaration and the certificate/measure-limit levels pass through
  // verbatim; the layer never upgrades an evidence level. `object_index` is the card's own object
  // index — which card a stroke belongs to, the selection's owner key read back for the map's
  // stationary-click test.
  struct EdgeArc {
    double position_deg = 0.0;
    bool coincident_with_support = false;  // the card's own declaration (the engine's tolerance,
                                           // not one re-derived here)
    bool certificate_available = false;
    bool measure_limit_available = false;
    double measure_limit = 0.0;
    bool concluded = false;  // certified/finite state: solid; an unevaluated-but-delivered
                             // position draws the weakened dashed form, a refused card no arc
    size_t object_index = 0;
  };
  std::vector<EdgeArc> edges;

  // The internal-reflection-turn layer: the diagnose features the document itself marks visible
  // and names as internal-reflection discriminants (source syntax `internal_<n>_tir_discriminant`).
  // The two deltas are the turn's representative angular distances at the chromatic pair's two
  // reference wavelengths — a pair of references, never a whole-circle colour claim. The source's
  // number is the REFLECTION STEP's 1-based index along the representative slot sequence — NOT a
  // crystal face number (dp_weight_kink builds margin = 2*step + 1 and dp_boundary's MarginName
  // spells margin/2, so the slot at step n is whatever face the path visits there; displaying it
  // as a face would misattribute the event). The verbatim source rides along for the text hosts.
  struct TirTurn {
    std::string source;  // the engine's own source string, verbatim
    int step = -1;       // the reflection step index parsed from internal_<n>_tir_discriminant;
                         // -1 when not parseable — never presented as a face number
    double delta_red_deg = 0.0;
    double delta_blue_deg = 0.0;
  };
  std::vector<TirTurn> tir_turns;
};

// The pure half of the map's overlay facts: survey document in, facts out. Reads the support,
// edge and diagnose-feature readers; never throws, never computes optics, never derives a
// position the readers did not deliver.
SurveyOverlayFacts BuildSurveyOverlayFacts(const LUMICE_PathSurvey& result);

// A fold group's header: the display spelling comes from the ONE label constant; the ID suffix
// keeps same-prefix groups apart inside one window (the Raypath view hosts the survey answer and
// the point answer side by side, each with its own evidence/diagnostics pair — identical visible
// labels would collide in the window's ID space and share one expansion state). No
// DefaultOpen: closed by default, expansion in the host window's storage — session-sticky,
// restart-reset (io.IniFilename is null), independent per window. Shared by all four answer
// hosts; the diagnostics half of each pair is additionally env-gated at its call site
// (env::AnalyzeDebugDetails gates the header itself, so an off env submits neither header nor
// body while the storage keeps the old expansion state).
inline bool FoldOpen(const char* label, const char* suffix_id) {
  const std::string id = std::string(label) + "##" + suffix_id;
  return ImGui::CollapsingHeader(id.c_str());
}

// The ImGui half: the model as the complete answer draws it — the default layer (the answers
// and their limits), then the evidence fold and the env-gated diagnostics fold (contract
// §16.1, both closed by default). Assumes the region's four-state shell already dispatched
// here (a completed, adopted document). This is the Raypath view's class-answer host.
//
// Non-null `row_readout` enables addressable hover surfaces and local row highlights. Only the
// profile rows return a hover standing for the caller's session copy and next-frame map device;
// verdict/Position and ratio rows keep their feedback local. Null (the default) submits the same
// rows as plain text, no extra items or interaction state — the main window's summary path
// passes nothing and renders byte-identically.
void RenderSurveyOverviewModel(const SurveyOverviewModel& model, SurveyRowHoverReadout* row_readout = nullptr,
                               SurveyInnerEdgeInteraction* inner_edge = nullptr);

// The main window's summary projection (contract §16.2): the SAME model read through the SAME
// default-layer renderer the complete answer uses — the summary is a read of it, never a second
// wording — closed with the pointer to the window hosting the complete answer. The main window
// draws no fold groups of its own; the omitted detail lives in the Raypath view.
void RenderSurveySummary(const SurveyOverviewModel& model);

}  // namespace lumice::gui

#endif  // LUMICE_GUI_ANALYZE_OVERVIEW_HPP
