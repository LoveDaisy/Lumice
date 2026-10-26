#ifndef LUMICE_RAYPATH_DETAIL_FEATURE_DISCOVERY_HPP_
#define LUMICE_RAYPATH_DETAIL_FEATURE_DISCOVERY_HPP_

#include "analytic/diagnostic_batch.hpp"
#include "raypath/detail/diagnostic_sampler.hpp"
#include "raypath/results/report_records.hpp"

namespace lumice::raypath {


struct DiscoveryOptions {
  SamplingBudget sampling;
  double bandwidth_rad = 0;
  double location_resolution_rad = 0;
  uint64_t max_field_evaluations = 0;
  int max_seeds = 0;
  int max_curve_points = 0;
  int max_interface_candidates = 0;
  int max_deviation_candidates = 0;
  int max_source_curve_points = 256;
  int max_source_boundaries = 8;
};
struct DiscoveryResult : DiscoveryRecords {
  DiagnosticMeasure measure;
};
// Explicit internal budget/scale contract, not yet a public default policy.
// No path/shape dispatcher and no target coordinate or feature name as input.
Error DiscoverFeatures(const DiagnosticSampler& sampler, const DiscoveryOptions& options, DiscoveryResult* out);

}  // namespace lumice::raypath
#endif  // LUMICE_RAYPATH_DETAIL_FEATURE_DISCOVERY_HPP_
