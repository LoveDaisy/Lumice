#ifndef LUMICE_RAYPATH_SCENE_MEASURE_HPP_
#define LUMICE_RAYPATH_SCENE_MEASURE_HPP_

// Actual-scene measure assembled around the analytic diagnostic field. This is the numerical
// input contract consumed by general raypath discovery: product distributions and concrete L2
// members are retained as data, rather than collapsed into a named pose family or nominal shape.

#include <cstdint>
#include <string>
#include <vector>

#include "analytic/diagnostic_field.hpp"
#include "config/config_manager.hpp"
#include "raypath/single_path_analysis.hpp"

namespace lumice::raypath {

enum class SceneSpectrumSource {
  kScene,
  kDiagnostic,
  kLegacyReferenceEndpoints,
};

enum class SceneMemberSelection {
  // The face sequence in each requested layer is already the concrete physical-L2 member.
  kConcrete,
  // Expand each sequence only under that layer's physical P/B/D gates.
  kAllPhysical,
  // As above, then retain every layer-local physical L2 member whose stable entry-face bit is
  // set in physical_member_mask. The mask never addresses an implementation-order chain index.
  kPhysicalMask,
};

enum class SceneMeasureStatus {
  kConfirmed,
  kZeroWeight,
  kPhysicallyUnreachable,
  kNumericalIncomplete,
  kNotSupported,
};

const char* SceneMeasureStatusName(SceneMeasureStatus status);

struct SceneMeasureRequest {
  std::vector<IdType> layer_crystal_ids;
  std::vector<std::vector<int>> path_layers;
  SceneMemberSelection member_selection = SceneMemberSelection::kConcrete;
  uint64_t physical_member_mask = ~uint64_t{ 0 };

  SceneSpectrumSource spectrum_source = SceneSpectrumSource::kScene;
  std::vector<double> diagnostic_wavelengths_nm;
  std::vector<double> diagnostic_wavelength_weights;

  // Joint shape/pose samples per selected member chain and global source node. A joint sample
  // draws every layer independently, except for the sync_group correlations inside one shape.
  int sample_count = 256;
  int sun_node_count = 8;
  int illuminant_node_count = 8;
  uint32_t seed = 1;
  bool include_derivatives = false;
};

struct MeasureFactorDescriptor {
  int layer_index = -1;  // -1 means scene-global.
  std::string name;
  DistributionType distribution = DistributionType::kNoRandom;
  int latent_id = -1;
  int support_dimension = 0;
  double center = 0.0;
  double spread = 0.0;
  std::string parameterization;
  std::string measure;
  std::string normalization;
};

struct SpectrumMeasureNode {
  int node_id = 0;
  double wavelength_nm = 0.0;
  double weight = 0.0;
  double refractive_index = 0.0;
  std::string source;
};

struct SunMeasureNode {
  int node_id = 0;
  double incident_direction[3]{};
  double mass = 0.0;
};

struct ShapeScalarSample {
  std::string name;
  double value = 0.0;
  int latent_id = -1;
};

struct SceneMeasureLayerRow {
  int layer_index = 0;
  IdType crystal_id = 0;
  std::vector<int> faces;
  std::vector<ShapeScalarSample> shape;
  double pose_lon_lat_roll_rad[3]{};
  double incident_direction[3]{};
  double outgoing_direction[3]{};
  double crystal_share = 0.0;
  double continuation_mass = 0.0;
  double entry_measure = 0.0;
  double fresnel_weight = 0.0;
  SceneMeasureStatus status = SceneMeasureStatus::kNotSupported;
  std::string reason;
  analytic::DiagnosticFieldResult field;
};

struct SceneMeasureRow {
  int spectrum_node_id = 0;
  int sun_node_id = 0;
  int member_chain_index = 0;
  int sample_index = 0;
  double wavelength_nm = 0.0;
  double spectrum_weight = 0.0;
  double sun_mass = 0.0;
  double joint_sample_mass = 0.0;
  double joint_proposal_density = 0.0;
  double joint_importance_weight = 0.0;
  double global_weight = 0.0;
  double contribution = 0.0;
  SceneMeasureStatus status = SceneMeasureStatus::kNotSupported;
  std::string reason;
  std::vector<SceneMeasureLayerRow> layers;
};

struct SceneMeasureResult {
  uint32_t seed = 0;
  int requested_sample_count = 0;
  int evaluated_row_count = 0;
  int stored_row_count = 0;
  bool rows_truncated = false;
  std::string units;
  std::string normalization;
  std::vector<MeasureFactorDescriptor> factors;
  std::vector<SpectrumMeasureNode> spectrum_nodes;
  std::vector<SunMeasureNode> sun_nodes;
  std::vector<std::vector<std::vector<int>>> member_chains;
  std::vector<SceneMeasureRow> rows;
  double coarse_contribution = 0.0;
  double total_contribution = 0.0;
  double absolute_error_estimate = 0.0;
  double sampled_measure_mass = 0.0;
  SceneMeasureStatus status = SceneMeasureStatus::kNotSupported;
  std::string reason;
};

Error BuildSceneMeasure(const ConfigManager& config, const SceneMeasureRequest& request, SceneMeasureResult* out);

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_SCENE_MEASURE_HPP_
