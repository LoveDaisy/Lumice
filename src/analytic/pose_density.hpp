#ifndef LUMICE_ANALYTIC_POSE_DENSITY_HPP_
#define LUMICE_ANALYTIC_POSE_DENSITY_HPP_

// Pose densities of the band sum (LI docs/band-sum-contract.md section 2.2, LI pose_density.py): a
// density relative to the Haar probability measure on SO(3), read only through the zenith
// components of the body axes, e_j = R_3j (the third row of the body -> world pose). With the c
// axis zenith theta = arccos(clip(e3)) and the roll psi = atan2(-e2, e1):
//
//   random           rho = 1
//   column, plate    rho = 2 g(theta) / I
//   parry, lowitz    rho = (2 g(theta) / I) (2 pi h(psi) / Q)
//
// g(theta) = exp(-(theta - mu)^2 / 2 sigma^2) on [0, pi], I = int_0^pi g sin theta dtheta;
// h(psi) = exp(-psi~^2 / 2 sigma_r^2), psi~ = ((psi - mu_r + pi) mod 2 pi) - pi (floored), Q = the
// integral of h over one period. I and Q are computed as LI computes them — 400-point
// Gauss-Legendre over the +-12 sigma window clipped to the domain — which the contract requires to
// 1e-12 relative, because they scale every value of a call.
//
// Column and plate (and parry and lowitz) differ only in LI's default zenith mean; this library has
// no defaults (every mean is passed), so the pair is one density and the family name is kept only so
// that a caller's intent reaches the result and the error messages.

namespace lumice::analytic {

enum class PoseFamily { kRandom, kColumn, kPlate, kParry, kLowitz };

// Degrees, as the contract and the Lumice configuration spell them. A field the family does not use
// must be exactly 0 (the C ABI's convention for unused fields, doc/analytic-api.md section 4.1).
struct PoseDensitySpec {
  PoseFamily family = PoseFamily::kRandom;
  double zenith_mean_deg = 0.0;
  double zenith_std_deg = 0.0;
  double roll_mean_deg = 0.0;
  double roll_std_deg = 0.0;
};

// Empty when `spec` is valid, else a sentence naming the offending field: the zenith mean finite in
// [0, 180] (LI's range), every used width finite and > 0, the roll mean finite, and every unused
// field 0.
const char* PoseDensityError(const PoseDensitySpec& spec);

// `nodes` Gauss-Legendre nodes and weights on [-1, 1], by Newton on P_n (no table). Exposed for the
// unit test of the quadrature; the density uses 400.
void GaussLegendre(int nodes, double* x, double* w);

constexpr int kPoseDensityQuadratureNodes = 400;
constexpr double kPoseDensityWindowSigmas = 12.0;

class PoseDensity {
 public:
  // `spec` must be valid (PoseDensityError empty).
  explicit PoseDensity(const PoseDensitySpec& spec);

  // rho at a pose whose third row is (e1, e2, e3). Gradual underflow is part of the contract: a
  // narrow density's tail is subnormal long before it is zero (the band sum counts rho > 0).
  double Evaluate(double e1, double e2, double e3) const;

  bool IsUniform() const { return family_ == PoseFamily::kRandom; }
  bool ReadsRoll() const { return family_ == PoseFamily::kParry || family_ == PoseFamily::kLowitz; }
  PoseFamily Family() const { return family_; }
  // I and Q (0 for a family that does not use them).
  double ZenithIntegral() const { return zenith_integral_; }
  double RollIntegral() const { return roll_integral_; }
  // The zenith and roll factors alone, for the rank-0 psi average.
  double ZenithFactor(double theta) const;
  double RollFactor(double psi) const;

 private:
  PoseFamily family_;
  double zenith_mean_ = 0.0;  // rad
  double zenith_std_ = 0.0;
  double roll_mean_ = 0.0;
  double roll_std_ = 0.0;
  double zenith_integral_ = 0.0;
  double roll_integral_ = 0.0;
};

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_POSE_DENSITY_HPP_
