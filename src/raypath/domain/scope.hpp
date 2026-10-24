#ifndef LUMICE_RAYPATH_DOMAIN_SCOPE_HPP_
#define LUMICE_RAYPATH_DOMAIN_SCOPE_HPP_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "core/def.hpp"

namespace lumice::raypath {

constexpr uint8_t kDefaultSymmetryBits = 7;

// Boundary selection. An absent layer selects the first allocation, or standalone.
struct PathSelection {
  IdType crystal_id = 0;
  std::vector<std::vector<int>> path_layers;
  uint8_t symmetry_bits = kDefaultSymmetryBits;
  std::optional<size_t> scene_layer;
};
enum class SelectionLocation { kStandalone, kSceneLayer };
struct ScopeLocation {
  SelectionLocation location = SelectionLocation::kSceneLayer;
  size_t layer_index = 0;
};
enum class MemberKind { kLiteral, kPhysicalL2 };
struct MemberScope : ScopeLocation {
  MemberKind member_kind = MemberKind::kLiteral;
  std::vector<std::vector<int>> members;
};

enum class GeometryKind { kNominal, kRealized };
struct GeometryScope {
  GeometryKind kind = GeometryKind::kNominal;
};
// Presence is independent of a physical zero. These are source-center coordinates,
// never the actual incident ray drawn inside a finite solar cap.
struct ObservationScope {
  std::optional<double> wavelength_nm;
  std::optional<double> sun_altitude_deg;
  std::optional<double> sun_azimuth_deg;
};

enum class MeasureKind { kActualDeclared, kIdealFrozen };
enum class ReferenceMeasure { kHaar };
struct MeasureScope {
  MeasureKind kind = MeasureKind::kActualDeclared;
  ReferenceMeasure reference = ReferenceMeasure::kHaar;
};
enum class QuantityKind { kBandSum };
enum class QuantityUnit { kPerSteradian };
enum class MemberAggregation { kRepresentative, kPhysicalClass };
struct QuantitySpec {
  QuantityKind kind = QuantityKind::kBandSum;
  QuantityUnit unit = QuantityUnit::kPerSteradian;
  MemberAggregation aggregation = MemberAggregation::kRepresentative;
  MeasureKind measure = MeasureKind::kActualDeclared;
};
}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_DOMAIN_SCOPE_HPP_
