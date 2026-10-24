#ifndef LUMICE_ANALYTIC_SUPPORT_TYPES_HPP_
#define LUMICE_ANALYTIC_SUPPORT_TYPES_HPP_

#include <string>
#include <vector>
namespace lumice::analytic {

enum class WalkStatus {
  kOk,
  kStepsExhausted,   // RuntimeError: MAX_WALK_STEPS without a corner or a closure
  kStartNoPoint,     // ValueError: U_P has no point on the lattice
  kStartCoversAll,   // ValueError: U_P covers the whole lattice — no boundary
  kStartNoEdge,      // ValueError: no lattice point next to the boundary — also the
                     // start-bisection shell fail-close (the message text distinguishes)
  kCornerNotSimple,  // RuntimeError: a corner's outgoing margin is not unique
  kNotClosed,        // RuntimeError: 1000 pieces without closing
  kNotFinite,        // RuntimeError: D_P off the closure of U_P (fail closed)
  kBadOrientation,   // ValueError: orientation not in {1, -1}
};

struct DeviationInterval {
  double lower = 0.0;
  double upper = 0.0;
  int n_components = 0;
  int n_closed = 0;
  int n_open = 0;
};

enum class OnsetLocation { kInterior, kBoundary };

enum class OnsetSource {
  kInteriorMinimum,
  kInteriorMaximum,
  kInteriorSaddle,
  kInteriorDegenerate,
  kSlabAxis,
  kSlabCircle,
  kBoundaryExtremum,
  kCorner,
};

enum class OnsetProfile {
  kFiniteJump,
  kLogDivergence,
  kInverseSqrtDivergence,
  kConePoint,
  kCrease,
  kBoundaryOnset,
  kDegenerate,
};

bool ProfileIsJacobianFocusing(OnsetProfile profile);

struct CriticalOnset {
  double value = 0.0;
  OnsetLocation location = OnsetLocation::kInterior;
  OnsetSource source = OnsetSource::kInteriorMinimum;
  OnsetProfile profile = OnsetProfile::kFiniteJump;
  double gradient_norm = 0.0;
  bool has_measure_limit = false;
  double measure_limit = 0.0;
  int multiplicity = 1;

  bool JacobianFocusing() const { return ProfileIsJacobianFocusing(profile); }
};

enum class FiberMeasureBinding { kSolidAngle, kFiberParameter };

enum class FiberEvidenceForm { kStructural, kSampledExhaustive, kSampledPartial };

enum class CurveExistence { kComputed, kEscaped, kWalkTruncated, kS4Declared };

struct OrbitFiberPoint {
  double u[3] = { 0.0, 0.0, 0.0 };         // R^T s_hat: the TRUE body-frame sun direction (never a placeholder)
  double outgoing[3] = { 0.0, 0.0, 0.0 };  // world frame, propagation direction
  double area = 0.0;                       // A: the corridor entry measure (0 unless the corridor opens)
  double transmission = 0.0;               // T: the path power with every internal reflectance
  bool valid = false;                      // path valid AND A > 0 AND T > 0 (the kept convention)
  bool jet_degenerate = false;             // orbit-regularity: the sun at the family-axis pole
  double parameter = 0.0;                  // theta
  double weight = 0.0;                     // 2 pi / grid
};

struct OrbitFiberStream {
  std::vector<OrbitFiberPoint> samples;
  // The producer's routing declaration (the mirrored vocabulary; see the enum block above):
  // the weights are dtheta elements of the orbit's own parameterization, and the stream covers
  // that orbit to its declared resolution (`grid`), which is the kSampledExhaustive claim.
  FiberMeasureBinding binding = FiberMeasureBinding::kFiberParameter;
  FiberEvidenceForm evidence = FiberEvidenceForm::kSampledExhaustive;
  int grid = 0;
  // The sun sits at the family-axis pole: the orbit circle collapses to a point, every sample is
  // marked jet_degenerate and `note` says so (never silent). A spec with no family axis
  // (FamilyAxis false — random orientation, or a column off the pole) returns an EMPTY stream
  // with the reason in `note`: the sigma->0 support is not one circle, so there is no orbit to
  // sample. Both are producer verdicts, not errors.
  bool spin_degenerate = false;
  std::string note;
  const char* source_name = "orbit-spin-grid";
};

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_SUPPORT_TYPES_HPP_
