#ifndef LUMICE_RAYPATH_RESULTS_QUANTITY_HPP_
#define LUMICE_RAYPATH_RESULTS_QUANTITY_HPP_

#include <string>
#include <variant>
#include <vector>

#include "core/scene_values.hpp"
#include "raypath/domain/claim.hpp"
#include "raypath/domain/scope.hpp"

namespace lumice::raypath {

namespace quantity {
inline constexpr long long kSurveyMcEssMaxDraws = 200000;
}

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

struct WitnessValues {
  QuantitySpec quantity;
  long long kept_count = 0;
  long long sample_count = 0;
  std::string lattice = "antipodal_fibonacci";  // the sampler's own lattice kind (discovery.hpp)
  bool deterministic = true;                    // no RNG anywhere in the sample
  double deviation_deg = 0.0;                   // the event's own deviation (the discovery's D)
  double weight = 0.0;                          // the event's w = A*T (the lattice's unit)
  double entry = 0.0;                           // the batch row's entry measure (its own read of A)
  double interface_product = 0.0;               // the batch row's interface product
  std::string source_note;                      // the declared source, spelled inside the document
  // The argmax event's candidate pose, retained for the document's warm_index (the seed the
  // query faces reuse). Not serialized on the card itself — it travels in the warm_index block
  // under the leg name "witness_argmax".
  bool has_pose = false;
  double pose[9] = {};
};

class WitnessResult : public ClaimedValue<WitnessValues> {};

// The chromatic layer's fixed wavelength pair — the single authority every chromatic-site
// spelling derives from (both legs' labels and indices, the coverage wavelengths, the prose).
// Both legs (the positional-dispersion table and the weight tint) consume this pair, so it
// cannot drift between them; the serialized labels and n values echo exactly these constants.
inline constexpr double kChromaticRedNm = 650.0;
inline constexpr double kChromaticBlueNm = 450.0;
inline constexpr const char* kChromaticRedLabel = "650nm";
inline constexpr const char* kChromaticBlueLabel = "450nm";

// The pair in prose, derived (never a site's own literal). Two orders are live in the document
// and preserved byte-for-byte: the budget note spells the pair blue-first, the coverage basis
// strings red-first (matching the labels vector's red-first order).
inline std::string ChromaticPairProseBlueFirst() {
  return std::to_string(static_cast<long long>(kChromaticBlueNm)) + "/" +
         std::to_string(static_cast<long long>(kChromaticRedNm));
}

inline std::string ChromaticPairProseRedFirst() {
  return std::to_string(static_cast<long long>(kChromaticRedNm)) + "/" +
         std::to_string(static_cast<long long>(kChromaticBlueNm));
}

// The chromatic layer's weight-tint leg: the direction the transmission WEIGHT tilts across
// the fixed 650/450 index pair (the positional-dispersion leg's sibling: a different mechanism
// — the same position's weight changes with lambda, driven by near-TIR Fresnel growth — and a
// separate sub-block, never merged into the dispersion fields). The ratio is m(450nm)/m(650nm),
// the point-mass band sum's masses at the sun direction, unweighted by SPD or CMF: the
// lambda-pair transmission-weight caliber, which is a different quantity from a disk-integrated
// D65 B/R and must not be compared against one.
//
// The direction vocabulary is the sign of the ratio itself: blue-tinted is B/R > 1, red-tinted
// is B/R < 1. No neutral band lives in the model — a band is an acceptance notion, and each
// consumer pins its own separation threshold with first-run provenance (the thresholds are the
// consumers', not the schema's; the caliber rule is documented in doc/raypath/raypath-cli-output.md
// section 9.6). `kNeutral` is the degenerate exact-1.0 point.
enum class TintDirection { kBlueTinted, kRedTinted, kNeutral };

// The full-coverage switch below: a new vocabulary value fails
// -Wswitch at the switch itself; the trailing return only satisfies control-reaches-end for
// today's fully-covered enum.
inline const char* TintDirectionName(TintDirection direction) {
  switch (direction) {
    case TintDirection::kBlueTinted:
      return "blue_tinted";
    case TintDirection::kRedTinted:
      return "red_tinted";
    case TintDirection::kNeutral:
      return "neutral";
  }
  return "neutral";
}

// The point-mass method names (band_sum.hpp's PointMassMethod) as the document spells them —
// the survey-side single authority over the slug, so a kernel-side spelling change is one edit.
// The switch carries no default: a new kernel PointMassMethod value fails -Wswitch at the
// switch itself; the trailing return only satisfies control-reaches-end for today's
// fully-covered enum. The class power-sum path is survey-side, not a kernel method: its slug
// is the constant below, and the two families never share a field shape (see WeightTintResult).
constexpr const char* kClassTintMethodLatticePowerSum = "lattice_power_sum";
inline constexpr const char* kTintMethodSinglePoseDirect = "single_pose_direct";

// One weight-tint evaluation. The two shapes this struct carries share the ratio/question but
// not the reading, and the field sets never merge:
// - the point-mass shape (the identity arm; `class_power_shape` false): `m_*` are the
//   per-lambda masses and their last-doubling quadrature changes (provenance, never asserted
//   against); `method` names the kernel's PointMassMethod. `ratio` is m_blue / m_red.
// - the class power-sum shape (the T1 arm; `class_power_shape` true): `energy_*` are the
//   per-sample average powers of the declared lattice's kept events (sum of w = A*T over the
//   lattice, divided by the lattice size); `method` is kClassTintMethodLatticePowerSum; the
//   `m_*` family stays zero and does not serialize (a mass is not this shape's quantity).
// Either way the tint runs on the class representative, with `member_count` declaring the
// class size it stands for (the probe's relabeled-equivalence discipline), and `ratio` is the
// blue reading over the red one.
enum class TintMassProtocol { kPsiAverage, kLatticeMean, kSinglePoseDirect };
struct TintMassReading {
  double red = 0;
  double blue = 0;
  double red_error = 0;
  double blue_error = 0;
  TintMassProtocol protocol = TintMassProtocol::kLatticeMean;
};
struct TintClassPowerReading {
  double red = 0;
  double blue = 0;
};

struct WeightTintValues {
  QuantitySpec quantity;
  QuantityUnit reading_unit = QuantityUnit::kMass;
  std::string red_label = kChromaticRedLabel;
  std::string blue_label = kChromaticBlueLabel;
  double n_red = 0.0;
  double n_blue = 0.0;
  long long sample_count = 0;
  std::string lattice = "antipodal_fibonacci";  // the sampler's own lattice kind (band_sum.hpp)
  bool deterministic = true;                    // no RNG anywhere in the sample
  std::vector<int> representative;
  int member_count = 0;
  double ratio = 0.0;
  TintDirection direction = TintDirection::kNeutral;
  long long kept_count_red = 0;
  long long kept_count_blue = 0;
  std::string caliber = std::string("m(") + kChromaticBlueLabel + ")/m(" + kChromaticRedLabel +
                        "): point-mass band-sum transmission weights at the sun direction; fixed " +
                        ChromaticPairProseRedFirst() +
                        " index pair, unweighted by SPD or CMF — not a disk-integrated D65 B/R";
  std::variant<TintMassReading, TintClassPowerReading> reading;
  bool IsClassPower() const { return std::holds_alternative<TintClassPowerReading>(reading); }
  TintMassReading& Mass() { return std::get<TintMassReading>(reading); }
  const TintMassReading& Mass() const { return std::get<TintMassReading>(reading); }
  TintClassPowerReading& Power() { return std::get<TintClassPowerReading>(reading); }
  const TintClassPowerReading& Power() const { return std::get<TintClassPowerReading>(reading); }
  QuantityMethod Method() const {
    if (IsClassPower())
      return QuantityMethod::kClassLatticePowerSum;
    switch (Mass().protocol) {
      case TintMassProtocol::kPsiAverage:
        return QuantityMethod::kPointMassPsiAverage;
      case TintMassProtocol::kLatticeMean:
        return QuantityMethod::kPointMassLatticeMean;
      case TintMassProtocol::kSinglePoseDirect:
        return QuantityMethod::kSinglePoseDirect;
    }
    return QuantityMethod::kPointMassBandSum;
  }
};

inline const char* TintMethodName(QuantityMethod method) {
  switch (method) {
    case QuantityMethod::kPointMassPsiAverage:
      return "psi_average";
    case QuantityMethod::kPointMassLatticeMean:
      return "lattice_mean";
    case QuantityMethod::kClassLatticePowerSum:
      return kClassTintMethodLatticePowerSum;
    case QuantityMethod::kSinglePoseDirect:
      return kTintMethodSinglePoseDirect;
    default:
      return "point_mass_band_sum";
  }
}

class WeightTintResult : public ClaimedValue<WeightTintValues> {};

// One internal interface's row of the TIR share leg: the face's literal number (the
// representative's own, since slots resolve 1:1) and the per-lambda TIR weight shares.
struct TirShareInterface {
  int face = 0;
  double share_red = 0.0;
  double share_blue = 0.0;
  double tir_weight_red = 0.0;
  double tir_weight_blue = 0.0;
};

// The TIR share leg (the schema-4 append; the exception evidence leg that decomposes the class
// tint's per-sample average power by internal-reflection mechanism): per lambda, the kept
// events' weight share spent on all-internal-interfaces-TIR poses, plus the per-interface
// decomposition. Declared antipodal Fibonacci lattice, class representative, the fixed
// 450/650 pair — the same caliber the class tint reads, a different quantity (a [0,1] share,
// never a cross-lambda ratio). Present only on a document whose arm ran the leg (the T1 arm
// and the refused-support arm); a path with no internal interface refuses the leg
// (no_internal_interfaces — the question is meaningless for that path shape), never zero.
struct TirShareValues {
  QuantitySpec quantity;
  double total_weight_red = 0.0;
  double total_weight_blue = 0.0;
  double all_tir_weight_red = 0.0;
  double all_tir_weight_blue = 0.0;
  long long chart_degenerate_red = 0;
  long long chart_degenerate_blue = 0;
  double n_red = 0.0;
  double n_blue = 0.0;
  std::vector<TirShareInterface> interfaces;
  double share_all_tir_red = 0.0;
  double share_all_tir_blue = 0.0;
  long long sample_count = 0;
  std::string lattice = "antipodal_fibonacci";
  bool deterministic = true;  // no RNG anywhere in the sample
  std::vector<int> representative;
  int member_count = 0;
  long long kept_count_red = 0;
  long long kept_count_blue = 0;
  std::string caliber;
};

class TirShareResult : public ClaimedValue<TirShareValues> {};

// One delta band's row of the MC region ESS leg: the Kish effective sample size of the band's
// Y-channel draw weights (rows grouped by outer draw — one draw is one sampling act even when
// it contributed several member rows), the contributing draw count and the weight sum.
struct McRegionEssBand {
  double delta_min_deg = 0.0;
  double delta_max_deg = 0.0;
  double ess = 0.0;
  long long draw_count = 0;
  double sum_weight = 0.0;
  double sum_squared_weight = 0.0;
};

// The opt-in MC region ESS leg (the schema-4 append): the MC corroboration sampling's effective
// sample size per solar-radial delta band for THIS raypath class — a sampling-quality
// measurement, never a corroboration verdict (no agrees/silent/conflicts word may appear here;
// the MC annotation vocabulary stays on the schema-3 side). Seeded and prefix-replay
// reproducible, so `deterministic` is false by construction: the field exists to state the
// leg's own shape against the deterministic legs' true — a deterministic quadrature carries no
// ESS semantics and never appears on one. Present only when the caller requested the leg.
struct McRegionEssValues {
  QuantitySpec quantity;
  long long seed = 0;
  long long draws = 0;
  double lambda_nm = 0.0;
  double band_deg = 0.0;
  std::vector<McRegionEssBand> bands;
  bool deterministic = false;  // seeded random; a given seed reproduces the sample
};

class McRegionEssResult : public ClaimedValue<McRegionEssValues> {};

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
