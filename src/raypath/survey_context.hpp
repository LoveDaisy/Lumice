#ifndef LUMICE_RAYPATH_SURVEY_CONTEXT_HPP_
#define LUMICE_RAYPATH_SURVEY_CONTEXT_HPP_

#include <string>
#include <vector>

namespace lumice::raypath {

// One field of the survey context, in the four-state vocabulary (the evidence layer's; schema1
// spells the states as strings). `state` is always serialized; the value fields exist exactly
// when the state says something was read.
struct SurveyContextField {
  std::string state;   // "certified" | "finite" | "unevaluated"
  std::string reason;  // the unevaluated cause, named (never empty when unevaluated)
};

// The survey context block (schema1's top-level append): where the target sits in the survey's
// structure. "Survey is the query's index" made concrete — with warm, the producing document's
// facts; without, the same facts from a context-only (T0–T2) assembly run here. Every field
// carries its own state; a band-sum reading only ever appears under the scope gate's pass.
struct SurveyContextBlock {
  std::string source;                    // "warm" | "inline"
  double reference_wavelength_nm = 0.0;  // probe context only: first requested wavelength
  // True: a --warm schema-4 document was given but carried no (complete) warm_index — the
  // documented degradation (no structural reuse, no seed consumption, inline context).
  bool warm_index_absent = false;
  // The target's deviation vs the delta-axis support: inside an interval, or the nearest card.
  SurveyContextField object;
  bool target_inside_support = false;
  std::string object_kind;           // the owning/nearest card's kind (e.g. "support_set")
  std::vector<int> object_members;   // the card's members (empty when the document carried none)
  double object_distance_deg = 0.0;  // 0 when inside
  // The angular distance to the nearest edge (a support endpoint or the certified edge position).
  SurveyContextField edge;
  double edge_distance_deg = 0.0;
  std::string edge_basis;  // "support_interval_endpoint" | "caustic_edge_position"
  // The band-sum reading at the nearest probe sample (warm documents only; the scope gate's
  // exact-equality comparison on lambda and sun decides, with the two named failure causes).
  SurveyContextField band_sum;
  double band_sum_value = 0.0;
  double band_sum_delta_deg = 0.0;  // the nearest sample's own deviation
  double band_sum_bin_deg = 0.0;
  std::string band_sum_caliber;  // both sides' lambda/sun identity, for the reading's declaration
};


}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_SURVEY_CONTEXT_HPP_
