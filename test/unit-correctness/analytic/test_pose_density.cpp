// Pose densities of the band sum (src/analytic/pose_density.hpp, LI docs/band-sum-contract.md
// section 2.2): the quadrature, the normalisations against LI's exported values and an independent
// quadrature, the Haar mean, the roll fold, the zenith clip, the parameter rules and the subnormal
// tail the band sum's K_rho_pos counts.
//
// symmetry_semantics: none — a pose density describes the pose ensemble, not a face sequence.

#include <gtest/gtest.h>

#include <cfloat>
#include <cmath>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "analytic/pose_density.hpp"
#include "support/portable_random.hpp"

namespace lumice::analytic {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;

PoseDensitySpec Spec(PoseFamily family, double zm, double zs, double rm = 0.0, double rs = 0.0) {
  PoseDensitySpec s;
  s.family = family;
  s.zenith_mean_deg = zm;
  s.zenith_std_deg = zs;
  s.roll_mean_deg = rm;
  s.roll_std_deg = rs;
  return s;
}

// Composite Simpson, the independent reference for I and Q.
template <class F>
double Simpson(double a, double b, int intervals, const F& f) {
  const double h = (b - a) / intervals;
  double sum = f(a) + f(b);
  for (int i = 1; i < intervals; i++) {
    sum += (i % 2 == 1 ? 4.0 : 2.0) * f(a + i * h);
  }
  return sum * h / 3.0;
}

TEST(PoseDensity, GaussLegendreIsExactOnPolynomials) {
  std::vector<double> x(kPoseDensityQuadratureNodes);
  std::vector<double> w(kPoseDensityQuadratureNodes);
  GaussLegendre(kPoseDensityQuadratureNodes, x.data(), w.data());
  double weight_sum = 0.0;
  for (int i = 0; i < kPoseDensityQuadratureNodes; i++) {
    weight_sum += w[i];
    if (i > 0) {
      EXPECT_LT(x[i - 1], x[i]);
    }
  }
  EXPECT_NEAR(weight_sum, 2.0, 1e-13);
  // Degrees up to 2n - 1 are integrated exactly; x^k over [-1, 1] is 2 / (k + 1) for even k.
  for (int k : { 2, 50, 400, 798 }) {
    double sum = 0.0;
    for (int i = 0; i < kPoseDensityQuadratureNodes; i++) {
      sum += w[i] * std::pow(x[i], k);
    }
    EXPECT_NEAR(sum * (k + 1) / 2.0, 1.0, 1e-12) << "x^" << k;
  }
}

// LI's exported I and Q (the band_sum fixtures' normalization_informative, li_rev fa8dadd).
TEST(PoseDensity, NormalisationsMatchLiToTheContractPrecision) {
  struct Case {
    PoseDensitySpec spec;
    double zenith;
    double roll;
  };
  const Case cases[] = {
    { Spec(PoseFamily::kPlate, 0.0, 1.0), 0.0003045864910802201, 0.0 },
    { Spec(PoseFamily::kPlate, 0.0, 0.5), 7.615242181419644e-05, 0.0 },
    { Spec(PoseFamily::kParry, 90.0, 1.0, 0.0, 1.0), 0.04374225368227967, 0.04374891651589675 },
  };
  for (const Case& c : cases) {
    if (std::string(PoseDensityError(c.spec)) != "") {
      ADD_FAILURE() << PoseDensityError(c.spec);
      continue;
    }
    const PoseDensity d(c.spec);
    const double ze = std::fabs(d.ZenithIntegral() / c.zenith - 1.0);
    std::cout << "[pose-density] I relative error " << ze << '\n';
    EXPECT_LE(ze, 1e-12);
    if (c.roll > 0.0) {
      const double re = std::fabs(d.RollIntegral() / c.roll - 1.0);
      std::cout << "[pose-density] Q relative error " << re << '\n';
      EXPECT_LE(re, 1e-12);
    }
  }
}

TEST(PoseDensity, NormalisationsMatchAnIndependentQuadrature) {
  for (double zm : { 0.0, 30.0, 90.0, 178.0 }) {
    for (double zs : { 0.5, 5.0, 60.0, 1e4 }) {
      const PoseDensity d(Spec(PoseFamily::kColumn, zm, zs));
      const double mu = zm * kDeg;
      const double sigma = zs * kDeg;
      // Over the whole domain: independent of the +-12 sigma window.
      const double lo = std::fmax(0.0, mu - 40.0 * sigma);
      const double hi = std::fmin(kPi, mu + 40.0 * sigma);
      const double ref = Simpson(lo, hi, 200000, [&](double t) {
        return std::exp(-(t - mu) * (t - mu) / (2.0 * sigma * sigma)) * std::sin(t);
      });
      EXPECT_NEAR(d.ZenithIntegral() / ref, 1.0, 1e-12) << zm << " " << zs;
    }
  }
  // sigma -> infinity: g -> 1, I -> 2.
  EXPECT_NEAR(PoseDensity(Spec(PoseFamily::kColumn, 90.0, 1e9)).ZenithIntegral(), 2.0, 1e-12);
  for (double rs : { 0.3, 1.0, 20.0, 1e3 }) {
    const PoseDensity d(Spec(PoseFamily::kLowitz, 0.0, 1.0, 170.0, rs));
    const double sigma = rs * kDeg;
    const double ref = Simpson(-kPi, kPi, 200000, [&](double t) { return std::exp(-t * t / (2.0 * sigma * sigma)); });
    EXPECT_NEAR(d.RollIntegral() / ref, 1.0, 1e-12) << rs;
  }
  // sigma_r -> infinity: h -> 1, Q -> 2 pi.
  EXPECT_NEAR(PoseDensity(Spec(PoseFamily::kParry, 90.0, 1.0, 0.0, 1e9)).RollIntegral(), 2.0 * kPi, 1e-11);
}

// E_Haar[rho] = 1, on uniform rotations (normalised Gaussian quaternions). Widths wide enough that
// 2e5 samples resolve the mean; the bound is five standard errors of the sample itself.
TEST(PoseDensity, HaarMeanIsOne) {
  const PoseDensitySpec specs[] = {
    Spec(PoseFamily::kColumn, 90.0, 20.0),
    Spec(PoseFamily::kPlate, 10.0, 30.0),
    Spec(PoseFamily::kParry, 80.0, 25.0, 30.0, 40.0),
    Spec(PoseFamily::kLowitz, 20.0, 30.0, -150.0, 50.0),
  };
  for (const PoseDensitySpec& spec : specs) {
    const PoseDensity d(spec);
    std::mt19937_64 rng(20260930);
    constexpr int kSamples = 200000;
    double sum = 0.0;
    double sum2 = 0.0;
    for (int i = 0; i < kSamples; i++) {
      double q[4];
      double n2 = 0.0;
      for (double& c : q) {
        c = test::PortableGaussianDouble(rng);  // same poses on every standard library
        n2 += c * c;
      }
      const double inv = 1.0 / std::sqrt(n2);
      const double w = q[0] * inv, x = q[1] * inv, y = q[2] * inv, z = q[3] * inv;
      // Third row of the rotation matrix of (w, x, y, z).
      const double e1 = 2.0 * (x * z - w * y);
      const double e2 = 2.0 * (y * z + w * x);
      const double e3 = 1.0 - 2.0 * (x * x + y * y);
      const double rho = d.Evaluate(e1, e2, e3);
      sum += rho;
      sum2 += rho * rho;
    }
    const double mean = sum / kSamples;
    const double se = std::sqrt((sum2 / kSamples - mean * mean) / kSamples);
    std::cout << "[pose-density] Haar mean " << mean << " +- " << se << '\n';
    EXPECT_LE(std::fabs(mean - 1.0), 5.0 * se);
  }
}

TEST(PoseDensity, RollFoldsOntoOnePeriodAboutTheMean) {
  const PoseDensity d(Spec(PoseFamily::kParry, 90.0, 1.0, 170.0, 10.0));
  // psi = -175 deg is 15 deg past the mean across +-pi.
  const double expected =
      2.0 * kPi * std::exp(-(15.0 * kDeg) * (15.0 * kDeg) / (2.0 * (10.0 * kDeg) * (10.0 * kDeg))) / d.RollIntegral();
  EXPECT_NEAR(d.RollFactor(-175.0 * kDeg) / expected, 1.0, 1e-13);
  EXPECT_NEAR(d.RollFactor(185.0 * kDeg) / expected, 1.0, 1e-13);
  EXPECT_NEAR(d.RollFactor(155.0 * kDeg) / expected, 1.0, 1e-13);
  // Evaluate reads psi = atan2(-e2, e1): e1 = cos(-175 deg), e2 = -sin(-175 deg).
  const double psi = -175.0 * kDeg;
  EXPECT_NEAR(d.Evaluate(std::cos(psi), -std::sin(psi), 0.0) / (d.ZenithFactor(kPi / 2.0) * expected), 1.0, 1e-12);
}

TEST(PoseDensity, ZenithComponentIsClipped) {
  const PoseDensity d(Spec(PoseFamily::kPlate, 0.0, 1.0));
  EXPECT_EQ(d.Evaluate(0.0, 0.0, 1.0 + 1e-15), d.ZenithFactor(0.0));
  EXPECT_EQ(d.Evaluate(0.0, 0.0, -1.0 - 1e-15), d.ZenithFactor(kPi));
  EXPECT_EQ(PoseDensity(Spec(PoseFamily::kRandom, 0.0, 0.0)).Evaluate(0.3, 0.4, 0.5), 1.0);
}

// A narrow density's tail is subnormal before it is zero (gradual underflow, contract section 4.5).
TEST(PoseDensity, TailIsSubnormalNotZero) {
  const PoseDensity d(Spec(PoseFamily::kPlate, 0.0, 0.5));
  const double sigma = 0.5 * kDeg;
  const double theta = sigma * std::sqrt(2.0 * 740.0);  // g = exp(-740)
  const double rho = d.Evaluate(0.0, 0.0, std::cos(theta));
  EXPECT_GT(rho, 0.0);
  EXPECT_LT(rho, DBL_MIN);
}

TEST(PoseDensity, ParameterRules) {
  EXPECT_STREQ(PoseDensityError(Spec(PoseFamily::kRandom, 0.0, 0.0)), "");
  EXPECT_STRNE(PoseDensityError(Spec(PoseFamily::kRandom, 90.0, 0.0)), "");
  EXPECT_STRNE(PoseDensityError(Spec(PoseFamily::kRandom, 0.0, 1.0)), "");
  EXPECT_STREQ(PoseDensityError(Spec(PoseFamily::kColumn, 90.0, 1.0)), "");
  EXPECT_STRNE(PoseDensityError(Spec(PoseFamily::kColumn, 90.0, 0.0)), "");  // width required
  EXPECT_STRNE(PoseDensityError(Spec(PoseFamily::kColumn, 90.0, -1.0)), "");
  EXPECT_STRNE(PoseDensityError(Spec(PoseFamily::kColumn, 90.0, 1.0, 0.0, 1.0)), "");  // roll width on column
  EXPECT_STRNE(PoseDensityError(Spec(PoseFamily::kPlate, 0.0, 1.0, 5.0, 0.0)), "");    // roll mean on plate
  EXPECT_STRNE(PoseDensityError(Spec(PoseFamily::kPlate, 190.0, 1.0)), "");
  EXPECT_STRNE(PoseDensityError(Spec(PoseFamily::kPlate, -1.0, 1.0)), "");
  EXPECT_STRNE(PoseDensityError(Spec(PoseFamily::kPlate, NAN, 1.0)), "");
  EXPECT_STRNE(PoseDensityError(Spec(PoseFamily::kPlate, 0.0, INFINITY)), "");
  EXPECT_STREQ(PoseDensityError(Spec(PoseFamily::kParry, 90.0, 1.0, 0.0, 1.0)), "");
  EXPECT_STREQ(PoseDensityError(Spec(PoseFamily::kLowitz, 0.0, 1.0, -30.0, 1.0)), "");
  EXPECT_STRNE(PoseDensityError(Spec(PoseFamily::kParry, 90.0, 1.0, 0.0, 0.0)), "");  // roll width required
  EXPECT_STRNE(PoseDensityError(Spec(PoseFamily::kParry, 90.0, 1.0, NAN, 1.0)), "");
  EXPECT_STRNE(PoseDensityError(Spec(static_cast<PoseFamily>(99), 0.0, 1.0)), "");
}

}  // namespace
}  // namespace lumice::analytic
