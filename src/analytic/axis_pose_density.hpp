#ifndef LUMICE_ANALYTIC_AXIS_POSE_DENSITY_HPP_
#define LUMICE_ANALYTIC_AXIS_POSE_DENSITY_HPP_

#include <array>
#include <cstdint>
#include <vector>

#include "core/distribution.hpp"

namespace lumice::analytic {

// The normalized CONTINUUM target of AxisDistribution, not the realized inverse LUT. The
// 257-node LUT has a maximum 180/256 degree spacing, piecewise-constant density and endpoint
// atoms. Sampler comparisons therefore use widths >=3 degrees and a 5% binned tolerance;
// continuum comparisons may use widths >=0.1 degrees. Laplace proposal clamp atoms (~5e-31
// each in ProposalLatFromU) are outside this continuum approximation contract.
//
// Input parameters are degrees. Valid domain: known types, finite |center| <=360, finite spread;
// kNoRandom ignores spread; every other spread is in [0.001,360]. A pinned latitude must be in
// [-90,90] (production does not fold that branch). Zero stochastic spread is rejected, not
// silently treated as a continuous density. Invalid construction throws std::invalid_argument.
//
// The production chart is Rz(az-pi) Ry(latitude-pi/2) Rz(roll), body->world, row-major. The full
// matrix is required when azimuth is nonuniform: its THIRD ROW DOES NOT CONTAIN AZIMUTH.
// EvaluateThirdRow/LogDensityThirdRow reject that case with kAzimuthRequired. Full-turn uniform
// factors never read their angle. The caller supplies a rotation accurate to 64*double epsilon
// in orthogonality/determinant; invalid matrices are flagged, not projected onto SO(3).
//
// For free latitude, let q0(phi)=sum q(phi+2k*pi), q1(phi)=sum q(pi-phi+2k*pi). Each leaf is
// (2*qj/Z)*(2*pi*A(az-j*pi))*(2*pi*B(roll-j*pi)), Z=E[cos(fold(L(U)))]. Thus density is
// relative to Haar PROBABILITY, cos(phi) dphi da dr/(8*pi^2), not angle Lebesgue measure.
// Legacy instead has latitude factor 2*qj/cos(phi), without area weighting. The factors are
// LEAF factors, not independent unconditional marginals; summing the two products is essential.
// A pinned coordinate is a support constraint, with factor 1 on support, 0 off support. The
// reference on each support sheet is the normalized product of FREE Haar coordinate marginals
// (cos(phi)dphi/2 and da/(2*pi), dr/(2*pi)); folded pinned angles can occupy two shifted sheets.
// Pinned comparisons allow 64*epsilon radians for chart inversion roundoff, not physical spread.
//
// At exact poles (sin(zenith)<=64*epsilon) the chart is undefined even for a uniform law. If
// either angle is read and sin(zenith)<sqrt(epsilon), kNearPole warns of eps/sin conditioning;
// legacy's inverse-cos latitude factor carries the same warning even with uniform angles.
// Zigzag is the rectified arcsine |center+spread*sin(2*pi*U)| on EVERY axis. Within
// sqrt(epsilon)*max(1,|center_rad|,spread_rad) of a raw cusp, kCusp warns of ill conditioning;
// at a cusp log density is +infinity, never clamped. Precision is on log density; exp may
// gradually underflow. Non-finite/out-of-domain chart inputs are invalid, not clipped.
//
// Normalization and moments use adaptive 16/32-point Gauss-Legendre in native U, Gaussian
// Phi^-1(U) included, split at folds/absolute-value corners/circular seams. The local absolute
// error target is 16*epsilon*max(1,(1+|center_rad|)/spread_rad) times each native interval's
// max(panel probability, absolute quadrature mass), accounting for centered-moment subtraction.
// Recursion is bounded at depth 48 and 4096 subdivisions per native interval; exhaustion reports
// kQuadratureAccuracy. Breakpoints below 1e-40 native probability are omitted to avoid subnormal
// panels; the tail itself is still integrated. Gaussian/wrapped image sums drop only terms below exp(-40)
// relative to the nearest image (well below log-domain roundoff); Laplace images sum analytically.
//
// Variances are rad^2. Latitude uses centered moments about fold(center), or fold(|center|)
// for the rectified zigzag proposal. Angular variances
// are E[wrap(X-mu_circ)^2], mu_circ=arg E[exp(iX)], INCLUDING the pole-flip mixture. A full-turn
// uniform has pi^2/3. Otherwise a circular resultant <=64*epsilon is undefined: the corresponding
// variance is NaN with kCircularMeanUndefined (no invented mean). Moments use the same area
// weighting/normalizer as the density; GaussianLegacy is unweighted.
//
// Windows are unions of marginal superlevel intervals (>exp(-72)*essential peak), on latitude
// [-pi/2,pi/2], angles [-pi,pi]. Pinned windows are points. Zigzag and legacy latitude have
// unbounded peaks and return kUnboundedPeak with NO intervals. Finite-peak windows use a mesh
// <=min(spread/8,domain/256), bracketed peak maximization and 64-epsilon root accuracy; numerical
// uncertainty remains visible in flags. Seam-straddling or folded laws can have multiple intervals.

enum class AxisDensityFlag : uint32_t {
  kNone = 0,
  kInvalidPose = 1u << 0,
  kUndefinedPole = 1u << 1,
  kNearPole = 1u << 2,
  kCusp = 1u << 3,
  kAzimuthRequired = 1u << 4,
  kQuadratureAccuracy = 1u << 5,
  kCircularMeanUndefined = 1u << 6,
  kUnboundedPeak = 1u << 7,
};

struct AxisDensityValue {
  double value = 0.0;
  uint32_t flags = 0;
  bool Has(AxisDensityFlag flag) const { return (flags & static_cast<uint32_t>(flag)) != 0; }
};

// Narrow interface consumed by sky-blur calculations. No distribution or quadrature state.
struct AxisPoseVariances {
  double sigma2_az = 0.0;
  double sigma2_lat = 0.0;
  double sigma2_roll = 0.0;
};

struct AxisDensityWindow {
  std::vector<std::array<double, 2>> intervals;
  uint32_t flags = 0;
};

const char* AxisPoseDensityError(const AxisDistribution& distribution);

class AxisPoseDensity {
 public:
  explicit AxisPoseDensity(const AxisDistribution& distribution);

  AxisDensityValue LogDensity(const std::array<double, 9>& rotation) const;
  AxisDensityValue Evaluate(const std::array<double, 9>& rotation) const;
  AxisDensityValue LogDensityThirdRow(double e1, double e2, double e3) const;
  AxisDensityValue EvaluateThirdRow(double e1, double e2, double e3) const;
  // Canonical chart face, radians: latitude in [-pi/2,pi/2]; finite angles when read.
  AxisDensityValue LogDensityAngles(double azimuth, double latitude, double roll) const;
  AxisDensityValue EvaluateAngles(double azimuth, double latitude, double roll) const;

  AxisDensityValue LatitudeLogFactor(double latitude, bool flipped = false) const;
  AxisDensityValue AzimuthLogFactor(double azimuth, bool flipped = false) const;
  AxisDensityValue RollLogFactor(double roll, bool flipped = false) const;
  AxisDensityValue LatitudeFactor(double latitude, bool flipped = false) const;
  AxisDensityValue AzimuthFactor(double azimuth, bool flipped = false) const;
  AxisDensityValue RollFactor(double roll, bool flipped = false) const;

  bool ReadsAzimuth() const;
  bool ReadsRoll() const;
  double LatitudeNormalizer() const { return normalization_; }
  double FlipProbability() const { return flip_probability_; }
  AxisPoseVariances Variances() const { return variances_; }
  uint32_t MomentFlags() const { return moment_flags_; }
  AxisDensityWindow LatitudeWindow() const;
  AxisDensityWindow AzimuthWindow() const;
  AxisDensityWindow RollWindow() const;

 private:
  AxisDensityValue AngularLogFactor(const Distribution& distribution, double angle, bool flipped) const;
  AxisDensityWindow Window(int axis) const;
  AxisDistribution distribution_;
  double normalization_ = 1.0;
  double flip_probability_ = 0.0;
  AxisPoseVariances variances_;
  uint32_t moment_flags_ = 0;
};

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_AXIS_POSE_DENSITY_HPP_
