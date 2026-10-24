#ifndef LUMICE_RAYPATH_RESULTS_QUANTITY_HPP_
#define LUMICE_RAYPATH_RESULTS_QUANTITY_HPP_

#include "core/scene_values.hpp"
#include "raypath/domain/claim.hpp"
#include "raypath/domain/scope.hpp"

namespace lumice::raypath {

// The actual inputs, owned by the completed result. Geometry/probability metadata is never
// reconstructed from a display label or a member count. A quantity selects its projection
// through its spec; this is not a universal cache key.
struct QuantityContext {
  MemberScope members;
  CrystalConfig crystal;
  LightSourceConfig light;
  ObservationScope observation;
};

inline QuantitySpec DescribeQuantity(QuantityKind kind, QuantityUnit unit, QuantityMethod method,
                                     ReferenceMeasure reference, DensityWeighting density) {
  QuantitySpec spec;
  spec.kind = kind;
  spec.unit = unit;
  spec.method = method;
  spec.reference = reference;
  spec.density = density;
  return spec;
}

// The sole projection of a quantity's declared semantics onto its claim.
inline void BindQuantityScope(ClaimScope* scope, const QuantitySpec& spec) {
  scope->quantity = spec;
  scope->geometry.kind = spec.shape;
  scope->observation = spec.observation;
  scope->measure = { spec.measure, spec.reference };
}

template <class Quantity>
void DeclareQuantity(Quantity* result, QuantitySpec spec, std::vector<std::vector<int>> members,
                     std::optional<double> wavelength_nm) {
  if (spec.wavelengths_nm.empty() && wavelength_nm)
    spec.wavelengths_nm = { *wavelength_nm };
  spec.observation.wavelength_nm = wavelength_nm;
  ClaimScope scope;
  BindQuantityScope(&scope, spec);
  scope.members.location = SelectionLocation::kStandalone;
  scope.members.members = std::move(members);
  result->BindScope(scope);
  if (auto* value = result->ValueOrNull())
    value->quantity = std::move(spec);
}

template <class Quantity>
Quantity CompleteQuantity(Quantity result) {
  if (auto* value = result.ValueOrNull()) {
    ClaimScope scope = result.Scope();
    BindQuantityScope(&scope, value->quantity);
    result.BindScope(scope);
  }
  return result;
}

struct PointFactorQuantities {
  QuantitySpec entry = DescribeQuantity(QuantityKind::kEntryArea, QuantityUnit::kArea, QuantityMethod::kEntryMeasure,
                                        ReferenceMeasure::kNone, DensityWeighting::kNone);
  QuantitySpec normalized_entry =
      DescribeQuantity(QuantityKind::kEntryArea, QuantityUnit::kDimensionless, QuantityMethod::kEntryMeasure,
                       ReferenceMeasure::kNone, DensityWeighting::kNone);
  QuantitySpec transmission =
      DescribeQuantity(QuantityKind::kTransmission, QuantityUnit::kDimensionless, QuantityMethod::kInterfaceProduct,
                       ReferenceMeasure::kNone, DensityWeighting::kNone);
  QuantitySpec density =
      DescribeQuantity(QuantityKind::kPoseDensity, QuantityUnit::kPerRadianCubed, QuantityMethod::kDeclaredPoseDensity,
                       ReferenceMeasure::kAngleLebesgue, DensityWeighting::kDeclaredPoseDensity);
  QuantitySpec jacobian =
      DescribeQuantity(QuantityKind::kNormalJacobian, QuantityUnit::kDimensionless, QuantityMethod::kNormalJacobian,
                       ReferenceMeasure::kNone, DensityWeighting::kNone);
  QuantitySpec coarea =
      DescribeQuantity(QuantityKind::kCoareaFactor, QuantityUnit::kDimensionless, QuantityMethod::kRegularizedCoarea,
                       ReferenceMeasure::kNone, DensityWeighting::kNone);
  QuantitySpec product =
      DescribeQuantity(QuantityKind::kFactorProduct, QuantityUnit::kPerRadianCubed, QuantityMethod::kFactorProduct,
                       ReferenceMeasure::kAngleLebesgue, DensityWeighting::kDeclaredPoseDensity);
  QuantitySpec fiber_integral =
      DescribeQuantity(QuantityKind::kFiberIntegral, QuantityUnit::kPerRadianSquared, QuantityMethod::kFiberArclength,
                       ReferenceMeasure::kAngleLebesgue, DensityWeighting::kDeclaredPoseDensity);
  QuantitySpec orbit_integral =
      DescribeQuantity(QuantityKind::kOrbitIntegral, QuantityUnit::kMass, QuantityMethod::kOrbitMidpoint,
                       ReferenceMeasure::kOrbitParameter, DensityWeighting::kDeclaredPoseDensity);
};

}  // namespace lumice::raypath
#endif  // LUMICE_RAYPATH_RESULTS_QUANTITY_HPP_
