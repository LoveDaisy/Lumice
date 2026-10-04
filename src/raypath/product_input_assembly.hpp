#ifndef RAYPATH_PRODUCT_INPUT_ASSEMBLY_H_
#define RAYPATH_PRODUCT_INPUT_ASSEMBLY_H_

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "config/light_config.hpp"
#include "core/product_sample_transform.hpp"
#include "core/shape_sample.hpp"
#include "raypath/physical_member_scope.hpp"
#include "raypath/single_path_analysis.hpp"

namespace lumice::raypath {

enum class SpectrumOrigin { kDiscreteSum, kProductSample, kQuadrature };
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
struct ProductWavelengthSample {
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
Error AssembleSampledSpectrum(const LightSourceConfig& light, const ProductWavelengthSample& sample,
                              AssembledSpectrum* out);
Error AssembleSpectrumQuadrature(const LightSourceConfig& light, const SpectrumQuadrature& quadrature,
                                 AssembledSpectrum* out);

struct ProductSourceSample {
  SphericalCapDraw draw;
  std::string provenance;
};
struct AssembledSource {
  SunParam domain{};
  std::array<double, 3> center_direction{};
  // Exact product float direction, kept separately from the normalized double
  // direction supplied to the analytic kernel (whose inputs must be unit).
  std::array<float, 3> product_direction{};
  std::array<double, 3> incident_direction{};
  ProductSourceSample sample;
};
Error AssembleSource(const SunParam& sun, const ProductSourceSample& sample, AssembledSource* out);

// Capture while the selected list and scene still refer to the same identity.
// Only selected entries are copied; callers may destroy/edit the source scene.
struct ProductLayerSelection {
  size_t layer_index;
  IdType crystal_id;
  std::vector<int> representative;
  uint8_t symmetry_bits;
};
struct ProductInputSnapshot {
  std::string scene_identity;
  LightSourceConfig light;
  std::vector<PhysicalMemberRequest> layers;
};
Error CaptureProductInput(const SceneConfig& scene, const std::string& scene_identity,
                          const std::vector<ProductLayerSelection>& selection, ProductInputSnapshot* out);

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
using ProductAxisDraw = std::variant<FullSphereAxisDraw, DistributedAxisDraw>;
struct ProductLayerSample {
  ProductAxisDraw axis;
  ShapeLeaderValues shape;
  std::string provenance;
};
struct DiscreteSpectrumSum {};
using ProductSpectrumRequest = std::variant<DiscreteSpectrumSum, ProductWavelengthSample, SpectrumQuadrature>;

struct AssembledProductLayer {
  PhysicalMemberScope scope;
  ProductLayerSample sample;
  ShapeSample shape_sample;
  analytic::CrystalShape shape;
  analytic::Status geometry_status = analytic::Status::kInvalidConfig;
  analytic::FaceNormalTable normals;
  analytic::FacePolygonTable polygons;
  float surface_area = 0;
  std::array<float, 3> angles{};
  std::array<float, 9> product_pose{};
  std::array<double, 9> analytic_pose{};
};
struct ProductInput {
  std::string scene_identity;
  AssembledSource source;
  AssembledSpectrum spectrum;
  std::vector<AssembledProductLayer> layers;
};
Error AssembleProductInput(const ProductInputSnapshot& snapshot, const std::vector<ProductLayerSample>& samples,
                           const ProductSourceSample& source, const ProductSpectrumRequest& spectrum,
                           ProductInput* out);

enum class ProductContributionStatus { kPositive, kZeroSupport, kInvalidOptics, kMissingFace, kRejectedShape };
struct ProductLayerEvaluation {
  ProductContributionStatus status = ProductContributionStatus::kRejectedShape;
  std::array<double, 3> incident{};
  std::array<double, 3> outgoing{};
  std::vector<double> interface_transmittances;
  double entry_area = 0;
  double entry_weight = 0;  // 2*A/S under the product's surface-area normalization
  double interface_product = 0;
};
struct ProductChainEvaluation {
  std::vector<ProductLayerEvaluation> layers;
  double optical_weight = 0;
  std::array<double, 3> xyz{};
};
// Evaluates ONE conditional chain, not a scene integral. Layer allocation,
// continuation probabilities and filters remain the caller's scene policy.
// Every layer consumes the preceding layer's real outgoing direction and the
// SAME spectral row. Spectrum is charged once, after the optical product.
Error EvaluateProductChain(const ProductInput& input, const std::vector<size_t>& members, size_t spectral_row,
                           ProductChainEvaluation* out);
// Start with one zero index per layer. Advances an odometer without allocating
// a Cartesian product; false means exhausted (or a malformed cursor).
bool NextProductMemberChain(const ProductInput& input, std::vector<size_t>* members);

}  // namespace lumice::raypath
#endif  // RAYPATH_PRODUCT_INPUT_ASSEMBLY_H_
