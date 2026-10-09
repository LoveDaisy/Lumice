#ifndef RAYPATH_DETAIL_INPUT_ASSEMBLY_H_
#define RAYPATH_DETAIL_INPUT_ASSEMBLY_H_

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "config/light_config.hpp"
#include "core/sample_transform.hpp"
#include "core/shape_sample.hpp"
#include "raypath/detail/physical_member_scope.hpp"
#include "raypath/single_path_analysis.hpp"

namespace lumice::raypath {

enum class SpectrumOrigin { kDiscreteSum, kSampled, kQuadrature };
struct SpectralRow {
  float wavelength_nm = 0;
  double source_weight = 0;
  double measure_mass = 0;
  double refractive_index = 0;
  std::array<double, 3> xyz_basis{};
  std::array<double, 3> coefficient{};
  SpectrumOrigin origin = SpectrumOrigin::kDiscreteSum;
  std::string provenance;
};
struct WavelengthSample {
  float wavelength_nm;
  // Required for a discrete spectrum, absent for a continuous one. Weight is
  // read from that source slot, never supplied/charged again by the caller.
  std::optional<size_t> discrete_slot;
  std::string provenance;  // e.g. host batch or device pool slot
};
struct SpectrumQuadratureNode {
  float wavelength_nm;
  double probability_mass;  // relative to Uniform[380,780), NOT nm
};
// Full-band probability measure: masses must sum to one within 64 double epsilons.
// Partial-band integrals require a different, explicitly declared domain.
struct SpectrumQuadrature {
  std::vector<SpectrumQuadratureNode> nodes;
  std::string rule;
  size_t evaluation_budget = 0;
  // Caller-estimated error, if known. No finite budget promises all-SPD accuracy.
  std::optional<double> estimated_error;
};
struct AssembledSpectrum {
  std::vector<SpectralRow> rows;
  std::optional<SpectrumQuadrature> quadrature;
};
Error AssembleDiscreteSpectrum(const LightSourceConfig& light, AssembledSpectrum* out);
Error AssembleSampledSpectrum(const LightSourceConfig& light, const WavelengthSample& sample, AssembledSpectrum* out);
Error AssembleSpectrumQuadrature(const LightSourceConfig& light, const SpectrumQuadrature& quadrature,
                                 AssembledSpectrum* out);

struct SourceSample {
  SphericalCapDraw draw;
  std::string provenance;
};
struct AssembledSource {
  SunParam domain{};
  std::array<double, 3> center_direction{};
  // Exact single-precision sample direction, kept separately from the normalized double
  // direction supplied to the analytic kernel (whose inputs must be unit).
  std::array<float, 3> sampled_direction{};
  std::array<double, 3> incident_direction{};
  SourceSample sample;
};
Error AssembleSource(const SunParam& sun, const SourceSample& sample, AssembledSource* out);

// Capture while the selected list and scene still refer to the same identity.
// Only selected entries are copied; callers may destroy/edit the source scene.
struct LayerSelection {
  size_t layer_index;
  IdType crystal_id;
  std::vector<int> representative;
  uint8_t symmetry_bits;
};
struct InputSnapshot {
  std::string scene_identity;
  LightSourceConfig light;
  std::vector<PhysicalMemberRequest> layers;
};
struct SupportDescription {
  int pose_coordinate_count = 0;
  int pose_support_dimension = 0;     // ZYZ quotient, not the optical map rank
  int shape_parameter_dimension = 0;  // not a rank of realized polyhedra
  int source_direction_dimension = 0;
  int spectral_dimension = 0;
};
SupportDescription DescribeSupport(const InputSnapshot& snapshot);

Error CaptureInput(const SceneConfig& scene, const std::string& scene_identity,
                   const std::vector<LayerSelection>& selection, InputSnapshot* out);

struct FullSphereAxisDraw {
  float latitude_uniform;
  float longitude_uniform;
  DistributionLatentDraw roll;
};
struct DistributedAxisDraw {
  // The distribution draw is used only for fixed/degenerate and legacy paths.
  // LUT paths require the two explicit CDF/flip uniforms instead.
  std::variant<DistributionLatentDraw, LatitudeLutDraw> latitude;
  DistributionLatentDraw azimuth;
  DistributionLatentDraw roll;
};
using AxisDraw = std::variant<FullSphereAxisDraw, DistributedAxisDraw>;
struct SampleIdentity {
  std::string scene_identity;
  size_t layer_index = 0;
  IdType crystal_id = 0;
};
struct LayerSample {
  SampleIdentity identity;
  AxisDraw axis;
  ShapeLeaderValues shape;
  std::string provenance;
};
struct DiscreteSpectrumSum {};
using SpectrumRequest = std::variant<DiscreteSpectrumSum, WavelengthSample, SpectrumQuadrature>;

struct AssembledLayer {
  PhysicalMemberScope scope;
  LayerSample sample;
  ShapeSample shape_sample;
  analytic::CrystalShape shape;
  analytic::Status geometry_status = analytic::Status::kInvalidConfig;
  analytic::FaceNormalTable normals;
  analytic::FacePolygonTable polygons;
  float surface_area = 0;
  std::array<float, 3> angles{};
  std::array<float, 9> sample_pose{};
  std::array<double, 9> analytic_pose{};
};
struct AssembledInput {
  std::string scene_identity;
  AssembledSource source;
  AssembledSpectrum spectrum;
  std::vector<AssembledLayer> layers;
};
Error AssembleInput(const InputSnapshot& snapshot, const std::vector<LayerSample>& samples, const SourceSample& source,
                    const SpectrumRequest& spectrum, AssembledInput* out);

// Uniform world spin is valid conditional on THIS realized incident ray only
// for a declared Haar pose independent of shape/source. A cap does not invalidate
// it, but replacing each cap ray by the solar center would change the measure.
// The single-crystal restriction is intentional; it is not a chain reduction.
std::optional<std::array<double, 3>> SingleCrystalIncidentOrbit(const AssembledInput& input);

// The exact zero-spectral-signal predicate (the v2 report's `no_related_signal`, the rule-A
// basis of the schema3 no-related-feature ruling): the snapshot's light is a discrete WlParam
// list AND every assembled spectral row's XYZ coefficient is exactly zero. The one authority —
// the v2 assembler and the schema3 ruling both read this (the schema3 module consumes the
// boolean; the predicate lives with the input types it reads). An exact zero is a CONFIG
// statement, never a claim based on empty sampling.
bool ZeroSpectralSignal(const InputSnapshot& snapshot, const AssembledInput& representative);

enum class ContributionStatus { kPositive, kZeroSupport, kInvalidOptics, kMissingFace, kRejectedShape };
struct LayerEvaluation {
  ContributionStatus status = ContributionStatus::kRejectedShape;
  std::array<double, 3> incident{};
  std::array<double, 3> outgoing{};
  std::vector<double> interface_transmittances;
  double entry_area = 0;
  double entry_weight = 0;  // 2*A/S under the raypath surface-area normalization
  // Math product (not a product-line name): all interface Fresnel transmittances multiplied
  // together; assigned from analytic::PathOutputs::fresnel_transmission.
  double interface_product = 0;
};
struct ChainEvaluation {
  uint64_t optical_evaluations = 0;
  std::vector<LayerEvaluation> layers;
  double optical_weight = 0;
  std::array<double, 3> xyz{};
};
// Evaluates ONE conditional chain, not a scene integral. Layer allocation,
// continuation probabilities and filters remain the caller's scene policy.
// Every layer consumes the preceding layer's real outgoing direction and the
// SAME spectral row. Spectrum is charged once, after the optical product.
Error EvaluateChain(const AssembledInput& input, const std::vector<size_t>& members, size_t spectral_row,
                    ChainEvaluation* out);
// Start with one zero index per layer. Advances an odometer without allocating
// a Cartesian product; false means exhausted (or a malformed cursor).
bool NextMemberChain(const AssembledInput& input, std::vector<size_t>* members);

}  // namespace lumice::raypath
#endif  // RAYPATH_DETAIL_INPUT_ASSEMBLY_H_
