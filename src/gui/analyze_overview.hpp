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
//     is adopted_result + the current identity, purely rendered).
//
// Honesty rules the builder inherits from the survey face (R7): a refused or unevaluated leg
// shows the engine's named reason, an empty reading shows as no data (the probe's empty samples
// arrive as NaN — gaps, never zeros), and "the document stopped early" is stated rather than
// passed off as complete. Availability flags, never defaulted zeros.

#include <cstddef>
#include <string>
#include <vector>

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

// The survey's built-in solar radial probe: the class's brightness profile at the document's
// reference wavelength, as readings plus the samples themselves. `sample_values` keeps the
// engine's NaN for an empty bin — the renderer and the tests read gaps as gaps.
struct SurveyProbeView {
  bool available = false;
  int state = 0;
  std::string mode;  // a refused probe names why (stop_requested)
  double wavelength_nm = 0.0;
  double refractive_index = 0.0;
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
  SurveyProbeView probe;
  SurveyFamilyView family;
  SurveyTintView tint;
  SurveyWitnessView witness;
  SurveyTirView tir;
  std::vector<SurveyObjectRow> objects;
};

// The pure half: survey document in, model out. Reads every block through the typed readers;
// never throws, never computes optics.
SurveyOverviewModel BuildSurveyOverviewModel(const LUMICE_PathSurvey& result);

// The ImGui half: the model as the details region draws it. Assumes the region's four-state
// shell already dispatched here (a completed, adopted document).
void RenderSurveyOverviewModel(const SurveyOverviewModel& model);

}  // namespace lumice::gui

#endif  // LUMICE_GUI_ANALYZE_OVERVIEW_HPP
