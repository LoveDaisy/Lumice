#ifndef LUMICE_RAYPATH_PATH_FEATURE_REPORT_HPP_
#define LUMICE_RAYPATH_PATH_FEATURE_REPORT_HPP_

// Target-free diagnostics for one concrete single-layer raypath. Unlike
// single_path_analysis.hpp this module does not solve a fiber through a requested sky point. It
// expands the path only under the crystal entry's physical L2 symmetry, evaluates every concrete
// member at every requested wavelength, and reports the small set of positioned features whose
// mechanisms have an implemented, fixture-backed detector.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "config/config_manager.hpp"
#include "raypath/scene_measure.hpp"
#include "raypath/single_path_analysis.hpp"

namespace lumice::raypath {

constexpr int kFeatureReportSchemaVersion = 3;
constexpr int kDefaultFeatureReportSampleCount = 8192;
constexpr int kMaxFeatureReportSampleCount = 1000000;
constexpr int kMaxFeatureReportWavelengthCount = 32;
constexpr uint64_t kMaxFeatureReportSampleEvaluations = 16777216;

struct PathFeatureReportRequest {
  IdType crystal_id = 0;
  std::vector<IdType> layer_crystal_ids;
  std::vector<std::vector<int>> path_layers;
  std::vector<double> wavelengths_nm;
  std::vector<double> wavelength_weights;
  int sample_count = kDefaultFeatureReportSampleCount;
  SceneMemberSelection member_selection = SceneMemberSelection::kAllPhysical;
  uint64_t physical_member_mask = ~uint64_t{ 0 };
  std::vector<uint64_t> physical_member_masks;
  std::vector<std::vector<std::vector<int>>> explicit_member_chains;
  SceneSpectrumSource scene_spectrum_source = SceneSpectrumSource::kScene;
  int scene_measure_sample_count = 64;
  int sun_node_count = 8;
  int illuminant_node_count = 8;
  uint32_t seed = 1;
};

enum class CoverageStatus {
  kSupported,
  kNotSupported,
  kNotDetectedAtResolution,
  kNumericalIncomplete,
  kPhysicallyUnreachable,
};

const char* CoverageStatusName(CoverageStatus status);

struct CoverageItem {
  std::string subject;
  CoverageStatus status = CoverageStatus::kNotSupported;
  std::string reason;
};

struct ReportWavelength {
  double wavelength_nm = 0.0;
  double weight = 0.0;
  double refractive_index = 0.0;
};

struct BrightnessEstimate {
  CoverageStatus status = CoverageStatus::kNotSupported;
  int coarse_sample_count = 0;
  int fine_sample_count = 0;
  int fine_valid_count = 0;
  int fine_positive_count = 0;
  double coarse_mean_at = 0.0;
  double fine_mean_at = 0.0;
  double absolute_difference = 0.0;
  double weighted_mean_at = 0.0;
  std::string measure;
  std::string reason;
  bool has_fixed_direction = false;
  double fixed_direction[3]{};
  double direction_residual_max = 0.0;
};

struct MemberWavelengthReport {
  ReportWavelength wavelength;
  BrightnessEstimate brightness;
};

struct PhysicalMemberReport {
  std::vector<int> faces;
  std::vector<MemberWavelengthReport> wavelengths;
};

struct FeaturePosition {
  double wavelength_nm = 0.0;
  double refractive_index = 0.0;
  std::optional<double> deviation_deg;
  std::optional<double> altitude_deg;
  std::optional<double> azimuth_deg;
  std::optional<double> relative_solar_azimuth_deg;
  std::optional<double> spherical_separation_deg;
};

struct FeatureMetric {
  std::string name;
  double value = 0.0;
};

struct PathFeature {
  std::string id;
  std::string kind;
  std::string evidence_status;
  std::string mechanism;
  std::string location;
  std::string interpretation;
  std::optional<bool> visible;
  std::vector<FeaturePosition> positions;
  std::vector<FeatureMetric> metrics;
};

struct FeatureReportMetadata {
  int schema_version = kFeatureReportSchemaVersion;
  int analytic_api_version = 0;
  IdType crystal_id = 0;
  std::vector<IdType> layer_crystal_ids;
  std::string crystal_kind;
  std::vector<NominalShapeScalar> shape;
  bool shape_is_nominal = false;
  std::vector<int> requested_faces;
  std::vector<std::vector<int>> requested_path_layers;
  double sun_altitude_deg = 0.0;
  double sun_azimuth_deg = 0.0;
  double incident_direction[3]{};
  std::string orientation_measure;
  int sample_count = 0;
};

struct PathFeatureReport {
  FeatureReportMetadata meta;
  SceneMeasureResult scene_measure;
  std::vector<ReportWavelength> wavelengths;
  std::vector<PhysicalMemberReport> members;
  std::vector<PathFeature> features;
  std::vector<CoverageItem> coverage;
  std::vector<std::string> limitations;
};

Error AnalyzePathFeatureReport(const ConfigManager& config, const PathFeatureReportRequest& request,
                               PathFeatureReport* out);

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_PATH_FEATURE_REPORT_HPP_
