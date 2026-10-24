#ifndef LUMICE_RAYPATH_RESULTS_REPORT_SCOPE_HPP_
#define LUMICE_RAYPATH_RESULTS_REPORT_SCOPE_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/scene_values.hpp"
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

struct SupportDescription {
  int pose_coordinate_count = 0;
  int pose_support_dimension = 0;     // ZYZ quotient, not the optical map rank
  int shape_parameter_dimension = 0;  // not a rank of realized polyhedra
  int source_direction_dimension = 0;
  int spectral_dimension = 0;
};
struct ReportLayerScope {
  std::optional<size_t> scene_layer;
  CrystalConfig crystal;
  std::vector<int> representative_faces;
  uint8_t symmetry_bits = 0;
};
struct ReportScope {
  std::string scene_identity;
  std::string spectrum_scope;
  uint32_t seed = 0;
  LightSourceConfig light;
  SupportDescription support;
  std::vector<std::vector<int>> physical_members;
  std::vector<ReportLayerScope> layers;
  std::vector<SpectralRow> spectrum;
  uint64_t requested_outer_samples = 0;
  int budget_ms = 0;
};


}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_RESULTS_REPORT_SCOPE_HPP_
