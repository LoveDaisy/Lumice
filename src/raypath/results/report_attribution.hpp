#ifndef LUMICE_RAYPATH_RESULTS_REPORT_ATTRIBUTION_HPP_
#define LUMICE_RAYPATH_RESULTS_REPORT_ATTRIBUTION_HPP_

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "raypath/results/report_structure.hpp"
namespace lumice::raypath::schema3 {

struct McAttributionCounts {
  long long dp_evaluations = 0;        // SampleOptical chain evaluations for the delta images
  long long presence_ess_queries = 0;  // one per object whose presence ESS was consulted
};

struct CorroborationAnnotation {
  CorroborationState state = CorroborationState::kUnchecked;
  double presence_ess = std::nan("");
  double match_distance = std::nan("");
  double tolerance_rad = std::nan("");
  long long matched_record = -1;  // index into the evidence's features; -1 = none
  double matched_record_ess = std::nan("");
  std::string ruler;   // the declared ruler + parameter snapshot
  std::string reason;  // the table arm's reading (the contract the review pinned)
};

struct UnattributedStructure {
  size_t record_index = 0;
  std::array<double, 3> position{};  // the record's sky point of closest approach
  double delta_rad = 0.0;            // that point's deviation
  double record_ess = 0.0;
  double min_margin_rad = 0.0;
  std::string ruler;
};

struct UnattributedOutcome {
  std::vector<UnattributedStructure> structures;
  long long skipped_no_position = 0;
  long long skipped_below_ess = 0;
  McAttributionCounts counts;
  std::string error;
};

enum class NoRelatedBasis {
  kZeroSpectralSignal,       // rule A: the exact zero-signal predicate
  kCertifiedSmoothRadiance,  // rule B: the conditioned certificate
};

struct NoRelatedRuling {
  bool issued = false;
  NoRelatedBasis basis = NoRelatedBasis::kCertifiedSmoothRadiance;  // read when issued
  // The four conditions of rule B, evaluated regardless of which rule issues (the priority of
  // rule A does not blind the report to what B would have said).
  bool partition_complete_no_escape = false;
  std::string partition_failure;  // naming the first offending support row
  bool all_objects_unlit_or_none = false;
  std::string unlit_failure;  // naming the first offending object
  bool no_sufficient_ess_unattributed = false;
  std::string unattributed_failure;  // naming the count and the closest margin
  bool s4_scope_declared = false;
  std::string s4_failure;
  // Rule 3's gate (beside the four, not one of them).
  bool two_d_valid_support = false;
  // The scope declaration's own text (the coverage row's reason) when declared.
  std::string s4_scope_note;
  // Non-empty exactly when NOT issued: the "completed + empty list + absence not proven"
  // reading the consumer assembles the outcome from.
  std::string absence_not_proven_note;
};

struct McSpectralVerification {
  bool available = false;
  std::optional<double> movement_rad;  // null = the check did not run to a number
  uint64_t optical_evaluations = 0;
  uint64_t field_component_evaluations = 0;
  double seconds = 0;
};

}  // namespace lumice::raypath::schema3

#endif  // LUMICE_RAYPATH_RESULTS_REPORT_ATTRIBUTION_HPP_
