#ifndef LUMICE_ANALYTIC_SKY_BLUR_FIELD_HPP_
#define LUMICE_ANALYTIC_SKY_BLUR_FIELD_HPP_

#include <array>

#include "analytic/axis_pose_density.hpp"
#include "analytic/path_evaluation.hpp"
#include "analytic/pose_curve.hpp"

namespace lumice::analytic {

// Internal, first-order sky blur, NOT a C ABI or a finite-spread nonlinear sky distribution.
// R is body->world; Sigma_pose is rad^2 in RIGHT coordinates R exp([omega]x).
// The sky point is -outgoing. J_sky and Sigma_sky use the returned orthonormal tangent
// basis, NOT longitude/latitude. Principal widths are standard deviations in radians;
// azimuth is modulo pi from basis[0] toward basis[1], undefined for an isotropic ellipse.
// Rank loss / a zero minor width is valid data, never an inverse-singular-value failure.
//
// Adapter: non-pole covariance is the centered chart tensor, conditional on the node's
// own azimuth, with world columns z, normalized(z x e3), e3, conjugated by R^T.
// Pole assignment is instead the uniform-azimuth MARGINAL, and is discontinuous by design.
// Pin notation: v = AxisPoseVariances::sigma2_lat, mu1 = E[T], mu2 = E[T^2]
// = v + mu1^2, H = (I-z z^T)/2, P = n_hat n_hat^T; T is distance from the named pole.
// The latitude blocks obey EXACT identities (no pointwise continuity claim):
//   azimuth-average(v P) = v H at EVERY nonzero zenith;
//   ||v P-v H||_F = v/sqrt(2);
//   assigned pole = mu2 H, from AxisZenithPoleRawMoment (not the centered variance);
//   ||v H-mu2 H||_F = mu1^2/sqrt(2);
//   ||v P-mu2 H||_F = sqrt(v^2-v*mu2+mu2^2/2).
// The full centered azimuth average differs from its CENTERED closure by exactly
// sqrt(3/2)*sigma2_roll*sin(zenith)^2 (roll-axis contamination). The full-turn-roll
// coefficient is 4.0292491243; observed at 1e-3 rad: 4.0292477808e-6, at 1e-6:
// 4.0292040986e-12. These constants do NOT describe convergence to the RAW pole.
// Half-normal anchor (narrow pole-centered GaussianLegacy): mu2=s^2,
// v=(1-2/pi)*s^2, mu2/v=2.751938393884109; raw-pushed MC ratio is 1+O(s^2).
// Broad distributions still yield a local surrogate, not an accuracy guarantee for MC.
//
// Valid domain: AxisPoseDensity's declared distribution bounds and finite moments; rotations
// use ValidateRotation's tolerance. Pole means represented atan2(hypot(R02,R12),R22)
// exactly 0 or pi, never an epsilon-clamped neighborhood. Nonuniform azimuth is refused
// when hypot(R02,R12)<sqrt(double epsilon), including both poles; uniform azimuth uses
// the pointwise formula at EVERY representable non-pole, however small its zenith.
// Moment integration/undefined circular mean is refused, not replaced by an invented value.
//
// Pushforward inputs: 1..kMaxFaceCount present slots, unit normals and incident direction
// (ValidateUnitVector), finite refractive_index>0, finite symmetric PSD covariance.
// Covariance symmetry and normalized PSD eigenvalues allow 64*eps relative roundoff;
// only negative eigenvalues within that roundoff are set to zero (NOT pole clamping).
// Jacobi diagonalization: at most 32 sweeps, off-diagonal stopping at 8*eps of input scale.
// Nonrepresentable results are refused. No promise of relative precision on zero widths.
// A sky tangent is projected onto the tangent plane; directions with projected length
// <=64*eps times input length are unavailable. A curve direction is unavailable when
// ||J*omega||<=64*eps*||J||_F*||omega|| (no distinguished sky tangent at a point image).
// Isotropy is |lambda1-lambda2|<=64*eps*lambda1.
// Invalid input names are returned in error; no partial output is claimed successful.

enum class SkyBlurStatus { kOk, kInvalidInput, kUnsupportedAzimuth, kMomentUnavailable, kPathInvalid, kNonFinite };

struct PoseCovariance {
  SkyBlurStatus status = SkyBlurStatus::kInvalidInput;
  const char* error = "not evaluated";
  std::array<double, 9> sigma_pose{};
  bool pole_marginal = false;
};

PoseCovariance AxisPoseCovariance(const AxisDistribution& distribution, const double pose[9]);

struct SkyBlurField {
  SkyBlurStatus status = SkyBlurStatus::kInvalidInput;
  const char* error = "not evaluated";
  double sky[3]{};
  double basis[2][3]{};
  double j_sky[2][3]{};
  double sigma_sky[2][2]{};
  double sigma_major = 0;
  double sigma_minor = 0;
  double major_azimuth = 0;
  bool azimuth_available = false;
};

// Returns "" on valid input, otherwise the offending argument and its requirement.
const char* ValidateSkyBlurParams(const FaceNormalTable& table, const int* slots, int slot_count,
                                  double refractive_index, const double incident[3], const double pose[9],
                                  const std::array<double, 9>& sigma_pose);
SkyBlurField PushforwardSkyBlur(const FaceNormalTable& table, const int* slots, int slot_count, double refractive_index,
                                const double incident[3], const double pose[9],
                                const std::array<double, 9>& sigma_pose);

struct SkyBlurProjection {
  bool available = false;
  const char* error = "not evaluated";
  double sigma_perp = 0;
  double sigma_parallel = 0;
};

// Explicit WORLD sky tangent: callers must not pass PoseCurveNode's Euler tangent directly.
SkyBlurProjection ProjectSkyBlur(const SkyBlurField& field, const double sky_tangent[3]);
// Converts a node tangent according to PoseCurveKind: Euler for family/caustic/collapsed,
// right-so(3) for sky fibers. A stationary/point image has no distinguished sky tangent.
SkyBlurProjection ProjectSkyBlurOnCurve(const SkyBlurField& field, const PoseCurveNode& node, PoseCurveKind kind);

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_SKY_BLUR_FIELD_HPP_
