#ifndef RAYPATH_DETAIL_PATH_FEATURE_REPORT_H_
#define RAYPATH_DETAIL_PATH_FEATURE_REPORT_H_

#include <cstdint>
#include <string>
#include <vector>

#include "config/config_manager.hpp"
#include "raypath/detail/feature_discovery.hpp"
#include "raypath/detail/input_assembly.hpp"
#include "raypath/path_feature_report.hpp"
#include "raypath/single_path_analysis.hpp"

namespace lumice::raypath {

struct PathFeatureReport {
  InputSnapshot snapshot;
  DiscoveryOptions options;
  AssembledInput representative_input;
  DiscoveryResult discovery;
  uint32_t seed = 0;
  bool no_related_signal = false;
  bool standalone_crystal = false;
  bool unsupported_multicrystal = false;
  std::vector<std::vector<int>> requested_path_layers;
  uint64_t requested_outer_samples = 0;
  int budget_ms = 0;
  double capture_seconds = 0;
  std::string spectrum_scope;
  std::optional<double> spectral_movement_rad;
  uint64_t spectral_optical_evaluations = 0;
  uint64_t spectral_field_evaluations = 0;
  double spectral_seconds = 0;
};

// Request-level assembly: validates the request, resolves the scene entry and
// drives BuildPathFeatureReport. The public AnalyzePathFeatureReport wraps
// this plus serialization; module tests drive this directly to inspect the
// report struct.
Error AssemblePathFeatureReport(const ConfigManager& config, const PathFeatureReportRequest& request,
                                PathFeatureReport* out);
Error BuildPathFeatureReport(const SceneConfig& scene, const std::string& identity,
                             const std::vector<LayerSelection>& selection, const SpectrumRequest& spectrum,
                             uint32_t seed, const DiscoveryOptions& options, PathFeatureReport* out);
}  // namespace lumice::raypath
#endif  // RAYPATH_DETAIL_PATH_FEATURE_REPORT_H_
