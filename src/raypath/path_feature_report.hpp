#ifndef LUMICE_RAYPATH_PATH_FEATURE_REPORT_HPP_
#define LUMICE_RAYPATH_PATH_FEATURE_REPORT_HPP_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "config/config_manager.hpp"
#include "raypath/product_feature_discovery.hpp"
#include "raypath/single_path_analysis.hpp"

namespace lumice::raypath {
constexpr int kFeatureReportSchemaVersion = 2;
constexpr int kDefaultFeatureReportSampleCount = 65536;
constexpr int kMaxFeatureReportSampleCount = 1000000;
constexpr int kMaxFeatureReportWavelengthCount = 32;
constexpr uint64_t kMaxFeatureReportSampleEvaluations = 16777216;
constexpr uint64_t kDefaultFeatureReportOpticalEvaluations = 4000000;
constexpr uint64_t kDefaultFeatureReportFieldEvaluations = 250000000;
constexpr int kDefaultFeatureReportBudgetMs = 15000;
constexpr int kMaxFeatureReportBudgetMs = 120000;
struct PathFeatureReportRequest {
  IdType crystal_id = 0;
  std::vector<std::vector<int>> path_layers;
  std::vector<double> wavelengths_nm;
  std::vector<double> wavelength_weights;
  int sample_count = 0;  // zero selects a work-aware dyadic prefix
  int budget_ms = kDefaultFeatureReportBudgetMs;
  uint64_t max_optical_evaluations = kDefaultFeatureReportOpticalEvaluations;
  uint64_t max_field_evaluations = kDefaultFeatureReportFieldEvaluations;
  double bandwidth_rad = 3.14159265358979323846 / 180;
  double location_resolution_rad = .05 * 3.14159265358979323846 / 180;
  uint8_t symmetry_bits = 7;  // physical P|B|D; zero means the concrete requested path
  std::optional<size_t> scene_layer;
  std::optional<std::chrono::steady_clock::time_point> deadline;
};

struct PathFeatureReport {
  ProductInputSnapshot snapshot;
  ProductDiscoveryOptions options;
  ProductInput representative_input;
  ProductDiscoveryResult discovery;
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
Error BuildProductPathReport(const SceneConfig& scene, const std::string& identity,
                             const std::vector<ProductLayerSelection>& selection,
                             const ProductSpectrumRequest& spectrum, uint32_t seed,
                             const ProductDiscoveryOptions& options, PathFeatureReport* out);
Error AnalyzePathFeatureReport(const ConfigManager& config, const PathFeatureReportRequest& request,
                               PathFeatureReport* out);
}  // namespace lumice::raypath
#endif  // LUMICE_RAYPATH_PATH_FEATURE_REPORT_HPP_
