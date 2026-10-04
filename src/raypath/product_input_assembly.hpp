#ifndef RAYPATH_PRODUCT_INPUT_ASSEMBLY_H_
#define RAYPATH_PRODUCT_INPUT_ASSEMBLY_H_

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "config/light_config.hpp"
#include "core/product_sample_transform.hpp"
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

}  // namespace lumice::raypath
#endif  // RAYPATH_PRODUCT_INPUT_ASSEMBLY_H_
