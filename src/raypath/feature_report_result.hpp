#ifndef LUMICE_RAYPATH_FEATURE_REPORT_RESULT_HPP_
#define LUMICE_RAYPATH_FEATURE_REPORT_RESULT_HPP_

#include "raypath/domain/result_identity.hpp"
#include "raypath/results/report_attribution.hpp"
#include "raypath/results/report_enumeration.hpp"
#include "raypath/results/report_records.hpp"
#include "raypath/results/report_scope.hpp"

namespace lumice::raypath {

enum class ReportOutcome { kCompleted, kPartial, kNoRelatedFeature, kUnsupportedMulticrystal };
enum class CoverageStatus { kSupported, kNumericalIncomplete, kNotSupported };
struct ReportCoverage {
  std::string subject;
  CoverageStatus status = CoverageStatus::kNotSupported;
  std::string reason;
  std::vector<std::string> limitations;
};
struct ReportObservation {
  double bandwidth_rad = 0;
  double location_resolution_rad = 0;
};
struct ReportMcEvidence {
  CompletedDiscovery discovery;
  ReportObservation observation_options;
  schema3::McSpectralVerification spectral_verification;
  std::string observation_scope_note;
};
struct ReportBudgets {
  uint64_t requested_outer_samples = 0;
  uint64_t completed_outer_samples = 0;
  uint64_t replicate_samples = 0;
  uint32_t replicate_seed = 0;
  uint64_t max_optical_evaluations = 0;
  uint64_t optical_evaluations = 0;
  uint64_t max_field_component_evaluations = 0;
  uint64_t field_component_evaluations = 0;
  bool exhausted = false;
  uint64_t attribution_dp_evaluations = 0;
  uint64_t attribution_presence_ess_queries = 0;
};
struct ReportTiming {
  double spectral_verification = 0;
  double capture = 0;
  double assembly = 0;
  double source_events_and_edge_observations = 0;
  double field_discovery = 0;
  double enumeration = 0;
  double attribution = 0;
};

// One completed value. In particular, MC source witnesses have already been replayed;
// neither a serializer nor a typed reader retains a sampler or an input snapshot.
struct FeatureReportResult {
  ReportScope scope;
  ReportOutcome outcome = ReportOutcome::kCompleted;
  std::vector<std::vector<int>> requested_path_layers;
  schema3::Schema3DiscoveryCore core;
  std::vector<schema3::CorroborationAnnotation> annotations;
  ReportMcEvidence mc;
  schema3::UnattributedOutcome unattributed;
  schema3::McAttributionCounts corroboration_counts;
  schema3::NoRelatedRuling ruling;
  std::vector<ReportCoverage> coverage;
  ReportBudgets budgets;
  ReportTiming timing;
};

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_FEATURE_REPORT_RESULT_HPP_
