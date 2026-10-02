#ifndef LUMICE_RAYPATH_SCENE_MEASURE_HPP_
#define LUMICE_RAYPATH_SCENE_MEASURE_HPP_

// Actual-scene measure assembled around the analytic diagnostic field. This is the numerical
// input contract consumed by general raypath discovery: product distributions and concrete L2
// members are retained as data, rather than collapsed into a named pose family or nominal shape.

#include <cstdint>
#include <functional>
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
  // Use explicit_member_chains verbatim. Each chain contains exactly one concrete face sequence
  // per requested layer; no symmetry expansion or Cartesian product is applied.
  kExplicitChains,
};

enum class SceneMeasureStatus {
  kConfirmed,
  kZeroWeight,
  kPhysicallyUnreachable,
  kNumericalIncomplete,
  kNotSupported,
};

const char* SceneMeasureStatusName(SceneMeasureStatus status);

// Describes whether a reported scalar is directly representable as a double.  Product densities
// retain their logarithm when the linear value underflows or overflows, so those two states are
// recoverable diagnostics rather than aliases for an exact zero or an unspecified null.
enum class SceneMeasureNumericStatus {
  kAvailable,
  kExactZero,
  kUnderflow,
  kOverflow,
  kInvalid,
};

const char* SceneMeasureNumericStatusName(SceneMeasureNumericStatus status);

struct SunMeasureNode {
  int node_id = 0;
  double incident_direction[3]{};
  double mass = 0.0;
};

struct SceneMeasureRequest {
  std::vector<IdType> layer_crystal_ids;
  std::vector<std::vector<int>> path_layers;
  SceneMemberSelection member_selection = SceneMemberSelection::kConcrete;
  uint64_t physical_member_mask = ~uint64_t{ 0 };
  std::vector<uint64_t> physical_member_masks;
  std::vector<std::vector<std::vector<int>>> explicit_member_chains;

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

  // Optional precomputed global-source nodes. Empty selects the product sun sampler. Non-empty
  // nodes are consumed verbatim (including their masses), which supplies a reusable integration
  // primitive for controlled source quadrature and independent counterfactual tests.
  std::vector<SunMeasureNode> source_sun_nodes;
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

enum class LatentBaseMeasure {
  kAtomCounting,
  kLebesgue,
  kUnitInterval,
  kBernoulliCounting,
};

const char* LatentBaseMeasureName(LatentBaseMeasure measure);

struct LatentMeasureSample {
  int latent_id = -1;
  int layer_index = -1;
  std::string name;
  LatentBaseMeasure base_measure = LatentBaseMeasure::kAtomCounting;
  double coordinate = 0.0;
  double proposal_density_or_mass = 1.0;
  double target_density_or_mass = 1.0;
  double mapping_jacobian = 0.0;
  std::string mapping;
  SceneMeasureStatus status = SceneMeasureStatus::kConfirmed;
};

struct ShapeScalarSample {
  std::string name;
  double value = 0.0;
  int latent_id = -1;
  int sync_group = 0;
  int leader_slot = -1;
  double raw_value = 0.0;
  bool absolute_value_fold = false;
  double mapping_jacobian = 1.0;
};

struct SceneMeasureLayerRow {
  int layer_index = 0;
  int source_sun_node_id = 0;
  int source_spectrum_node_id = 0;
  double source_wavelength_nm = 0.0;
  IdType crystal_id = 0;
  std::vector<int> faces;
  std::vector<ShapeScalarSample> shape;
  double pose_lon_lat_roll_rad[3]{};
  // Dimension of the generated pose support in SO(3). -1 means the sampled float landed on a
  // singular longitude/latitude/roll chart while a positive-width generator spans nearby poses,
  // so one local differential cannot state the support dimension without understating it.
  int pose_support_rank = 0;
  // Derivatives of the field's row-major 3x3 pose matrix with respect to the three sampled
  // longitude/latitude/roll coordinates: [coordinate][matrix element]. This is the actual SO(3)
  // tangent map, including roll, rather than two tangent vectors for the crystal axis alone.
  double pose_tangent_drotation[27]{};
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
  uint32_t replay_seed = 0;
  double wavelength_nm = 0.0;
  double spectrum_weight = 0.0;
  double sun_mass = 0.0;
  double joint_sample_mass = 0.0;
  double joint_proposal_density = 0.0;
  double joint_target_density = 0.0;
  double joint_log_proposal_density = 0.0;
  double joint_log_target_density = 0.0;
  double joint_importance_weight = 0.0;
  double global_weight = 0.0;
  double contribution = 0.0;
  SceneMeasureNumericStatus joint_proposal_density_status = SceneMeasureNumericStatus::kInvalid;
  SceneMeasureNumericStatus joint_target_density_status = SceneMeasureNumericStatus::kInvalid;
  SceneMeasureNumericStatus joint_importance_weight_status = SceneMeasureNumericStatus::kInvalid;
  SceneMeasureNumericStatus global_weight_status = SceneMeasureNumericStatus::kInvalid;
  SceneMeasureNumericStatus contribution_status = SceneMeasureNumericStatus::kInvalid;
  SceneMeasureStatus status = SceneMeasureStatus::kNotSupported;
  SceneMeasureStatus evaluation_status = SceneMeasureStatus::kNotSupported;
  std::string reason;
  std::string evaluation_reason;
  std::vector<LatentMeasureSample> latents;
  std::vector<SceneMeasureLayerRow> layers;
};

struct SceneMeasureStatusCounts {
  int confirmed = 0;
  int zero_weight = 0;
  int physically_unreachable = 0;
  int numerical_incomplete = 0;
  int not_supported = 0;
};

struct SceneMeasureResult {
  uint32_t seed = 0;
  int requested_sample_count = 0;
  int evaluated_row_count = 0;
  int stored_row_count = 0;
  bool rows_truncated = false;
  std::string stored_row_selection;
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
  double joint_sampling_error_estimate = 0.0;
  double sun_node_error_estimate = 0.0;
  double spectrum_node_error_estimate = 0.0;
  double sampled_measure_mass = 0.0;
  SceneMeasureNumericStatus coarse_contribution_status = SceneMeasureNumericStatus::kInvalid;
  SceneMeasureNumericStatus total_contribution_status = SceneMeasureNumericStatus::kInvalid;
  SceneMeasureNumericStatus absolute_error_estimate_status = SceneMeasureNumericStatus::kInvalid;
  SceneMeasureNumericStatus joint_sampling_error_estimate_status = SceneMeasureNumericStatus::kInvalid;
  SceneMeasureNumericStatus sun_node_error_estimate_status = SceneMeasureNumericStatus::kInvalid;
  SceneMeasureNumericStatus spectrum_node_error_estimate_status = SceneMeasureNumericStatus::kInvalid;
  SceneMeasureNumericStatus sampled_measure_mass_status = SceneMeasureNumericStatus::kInvalid;
  SceneMeasureStatusCounts status_counts;
  SceneMeasureStatus status = SceneMeasureStatus::kNotSupported;
  std::string reason;
};

Error BuildSceneMeasure(const ConfigManager& config, const SceneMeasureRequest& request, SceneMeasureResult* out);

using SceneMeasureRowVisitor = std::function<void(const SceneMeasureRow&)>;

// Visits every evaluated row before the bounded representative-row store is applied. The same
// seed and request replay the same sequence; consumers that need the complete numerical field use
// this overload instead of treating SceneMeasureResult::rows as the full dataset.
Error BuildSceneMeasure(const ConfigManager& config, const SceneMeasureRequest& request,
                        const SceneMeasureRowVisitor& visitor, SceneMeasureResult* out);

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_SCENE_MEASURE_HPP_
