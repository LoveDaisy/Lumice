// The declared pose measure (src/raypath/detail/measure/declared_density.hpp): the per-type slot
// laws, the folded latitude law against the production LUT's own target, the joint pose law, and
// the u-marginal — closed-form values against INDEPENDENT oracles (hand-derived formulas in this
// file, BuildLatLut's cdf table, analytic::PoseDensity on the overlap family), never against the
// implementation's own intermediates.
//
// Tolerances are physical/numerical, per quantity:
//   - closed-form vs closed-form (double arithmetic): 1e-12 relative.
//   - closed-form vs the LUT's float32 cdf table (4096-bin histogram + 257-node resample): the
//     LUT's own quadrature/interpolation error, pinned at 2e-3 absolute after measurement
//     (scrum 649's 1e-8-across-ISA lesson: the tolerance names what the cheaper side can hold).
//   - zonal vs general path: the general path's 512-panel roll midpoint, pinned per case.
//
// symmetry_semantics: none — measures, not face paths.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "analytic/pose_density.hpp"
#include "core/lat_lut.hpp"
#include "raypath/detail/measure/declared_density.hpp"

namespace lumice::raypath {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kHalfPi = kPi * 0.5;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kDeg = kPi / 180.0;

Distribution NoRandom(double v) {
  return { DistributionType::kNoRandom, static_cast<float>(v), 0.0f };
}
Distribution Uniform(double center, double range) {
  return { DistributionType::kUniform, static_cast<float>(center), static_cast<float>(range) };
}
Distribution Gauss(double mean, double std) {
  return { DistributionType::kGaussian, static_cast<float>(mean), static_cast<float>(std) };
}
Distribution Laplace(double location, double scale) {
  return { DistributionType::kLaplacian, static_cast<float>(location), static_cast<float>(scale) };
}
Distribution Zigzag(double tilt, double amplitude) {
  return { DistributionType::kZigzag, static_cast<float>(tilt), static_cast<float>(amplitude) };
}
Distribution Legacy(double mean, double std) {
  return { DistributionType::kGaussianLegacy, static_cast<float>(mean), static_cast<float>(std) };
}

AxisDistribution Axis(Distribution az, Distribution lat, Distribution roll) {
  AxisDistribution a;
  a.azimuth_dist = az;
  a.latitude_dist = lat;
  a.roll_dist = roll;
  return a;
}

// Composite Gauss-Legendre in the test (independent of the implementation's helper: nodes from
// analytic::GaussLegendre — the shared, LI-parity-checked node source — composition here).
double Integrate(double (*f)(double, void*), void* ctx, double a, double b, int panels = 64, int order = 16) {
  std::vector<double> x(static_cast<size_t>(order)), w(static_cast<size_t>(order));
  analytic::GaussLegendre(order, x.data(), w.data());
  const double h = (b - a) / panels;
  double total = 0.0;
  for (int p = 0; p < panels; p++) {
    const double lo = a + p * h;
    for (int i = 0; i < order; i++) {
      total += w[i] * f(lo + (x[i] + 1.0) * 0.5 * h, ctx);
    }
  }
  return total * 0.5 * h;
}

struct SlotCtx {
  Distribution slot;
};
double SlotF(double v, void* ctx) {
  return SlotDensityValue(static_cast<SlotCtx*>(ctx)->slot, v);
}

// ===========================================================================
// Table 1: slot-level laws.
// ===========================================================================

TEST(DeclaredDensity, SlotUniformIsABox) {
  const Distribution u = Uniform(30.0, 40.0);  // [10 deg, 50 deg]
  EXPECT_NEAR(SlotDensityValue(u, 30.0 * kDeg), 1.0 / (40.0 * kDeg), 1e-15);
  EXPECT_EQ(SlotDensityValue(u, 9.9 * kDeg), 0.0);
  EXPECT_EQ(SlotDensityValue(u, 50.1 * kDeg), 0.0);
  SlotCtx ctx{ u };
  // Integrate the box itself: a panel straddling a jump is where composite GL loses its order,
  // and there is nothing to learn from integrating across the support edges.
  EXPECT_NEAR(Integrate(SlotF, &ctx, 10.0 * kDeg, 50.0 * kDeg), 1.0, 1e-13);
}

TEST(DeclaredDensity, SlotGaussianValueAndNormalization) {
  const Distribution g = Gauss(10.0, 5.0);
  const double sigma = 5.0 * kDeg;
  EXPECT_NEAR(SlotDensityValue(g, 10.0 * kDeg), 1.0 / (sigma * std::sqrt(kTwoPi)), 1e-12);
  EXPECT_NEAR(SlotDensityValue(g, 10.0 * kDeg + sigma), std::exp(-0.5) / (sigma * std::sqrt(kTwoPi)), 1e-12);
  SlotCtx ctx{ g };
  EXPECT_NEAR(Integrate(SlotF, &ctx, -kPi, kPi), 1.0, 1e-12);
}

TEST(DeclaredDensity, SlotLaplacianSymmetryAndNormalization) {
  const Distribution l = Laplace(-7.0, 12.0);
  const double b = 12.0 * kDeg;
  EXPECT_NEAR(SlotDensityValue(l, -7.0 * kDeg), 1.0 / (2.0 * b), 1e-12);
  EXPECT_NEAR(SlotDensityValue(l, -7.0 * kDeg + b), SlotDensityValue(l, -7.0 * kDeg - b), 1e-15);
  SlotCtx ctx{ l };
  // Split at the |x - mu| kink (-7 deg) and cover the exponential tails (the +-pi cut would
  // drop e^{-(180 - 7)/12} ~ 5e-7 of the mass — the domain truncation, not the law).
  EXPECT_NEAR(Integrate(SlotF, &ctx, -7.0 * kDeg - 40.0 * b, -7.0 * kDeg) +
                  Integrate(SlotF, &ctx, -7.0 * kDeg, -7.0 * kDeg + 40.0 * b),
              1.0, 1e-12);
}

TEST(DeclaredDensity, SlotZigzagIsTheArcsineWhenTiltDominates) {
  // B > A > 0: y = |A sin t + B| = A sin t + B, so the law is the arcsine on [B - A, B + A]
  // (independent re-derivation: two t-preimages of y per period, each 1/(2 pi A |cos t|)).
  const double a = 0.3, b = 0.5;  // radians
  for (double y : { 0.25, 0.4, 0.6, 0.75 }) {
    const double s = (y - b) / a;
    const double expected = 2.0 / (kTwoPi * a * std::sqrt(1.0 - s * s));
    EXPECT_NEAR(ZigzagDensityValue(a, b, y), expected, 1e-12) << "y = " << y;
  }
  EXPECT_EQ(ZigzagDensityValue(a, b, 0.15), 0.0);  // below B - A
  EXPECT_EQ(ZigzagDensityValue(a, b, 0.85), 0.0);  // above B + A
  EXPECT_EQ(ZigzagDensityValue(a, b, 0.0), 0.0);   // the fold boundary itself
}

TEST(DeclaredDensity, SlotZigzagNormalizesAlsoWhenTheFoldIsActive) {
  // |B| < A: the support is [0, A + B] and four preimage branches meet below A - |B|.
  const Distribution z = Zigzag(20.0, 50.0);  // A = 50 deg, B = 20 deg, support [0, 70] deg
  SlotCtx ctx{ z };
  // The integrand has an integrable 1/sqrt divergence at 0 (B < A) and a branch change at
  // y = A - B = 30 deg; splitting both, the unsubstituted end panel still costs ~2.5e-3
  // (measured), which is the quadrature's, not the law's.
  EXPECT_NEAR(Integrate(SlotF, &ctx, 1e-9, 30.0 * kDeg, 64, 16) +
                  Integrate(SlotF, &ctx, 30.0 * kDeg, 70.0 * kDeg + 1e-9, 64, 16),
              1.0, 3e-3);
}

TEST(DeclaredDensity, SlotDegenerateValuesFollowTheSamplerConstant) {
  // BuildDistributionDrawPlan answers kConstant at spread == 0 and TransformDistribution
  // evaluates the law's constant: |center| for zigzag, center for the rest (sample_transform.cpp).
  double at = 0.0;
  EXPECT_TRUE(SlotIsDirac(NoRandom(37.0), &at));
  EXPECT_NEAR(at, 37.0 * kDeg, 1e-15);
  EXPECT_TRUE(SlotIsDirac(Uniform(30.0, 0.0f), &at));
  EXPECT_NEAR(at, 30.0 * kDeg, 1e-15);
  EXPECT_TRUE(SlotIsDirac(Gauss(30.0, 0.0f), &at));
  EXPECT_NEAR(at, 30.0 * kDeg, 1e-15);
  EXPECT_TRUE(SlotIsDirac(Zigzag(-25.0, 0.0f), &at));
  EXPECT_NEAR(at, 25.0 * kDeg, 1e-15);  // |-25|, the rectified fold's constant
  EXPECT_FALSE(SlotIsDirac(Gauss(0.0, 1.0f), &at));
}

// ===========================================================================
// Table 2: the folded latitude law.
// ===========================================================================

TEST(DeclaredDensity, LatitudeFoldedGaussianMatchesHandDerivedFold) {
  // mu = 30 deg, sigma = 20 deg: the fold is ACTIVE (the proposal reaches past the pole and the
  // equator-mirrored branch). Independent re-derivation of p_fold at phi = 60 deg:
  // branch A: p(60 deg); branch B: p(pi - 60 deg = 120 deg); farther 2 pi k shifts are e^{-...}.
  const LatitudeDensity law = MakeLatitudeDensity(Gauss(30.0, 20.0));
  ASSERT_EQ(law.kind(), LatitudeLawKind::kFoldedArea);
  const double phi = 60.0 * kDeg;
  const double mu = 30.0 * kDeg, sigma = 20.0 * kDeg;
  auto gauss_at = [&](double x) {
    return std::exp(-0.5 * (x - mu) * (x - mu) / (sigma * sigma)) / (sigma * std::sqrt(kTwoPi));
  };
  const double p_fold_expected = gauss_at(phi) + gauss_at(180.0 * kDeg - phi) + gauss_at(phi - kTwoPi) +
                                 gauss_at(180.0 * kDeg - phi + kTwoPi) + gauss_at(phi + kTwoPi) +
                                 gauss_at(180.0 * kDeg - phi - kTwoPi);
  const double z_expected = [&]() {
    // Z by the test's own composite GL over p_fold * cos (the same k-sum, evaluated pointwise).
    std::vector<double> x(16), w(16);
    analytic::GaussLegendre(16, x.data(), w.data());
    double total = 0.0;
    const int panels = 64;
    const double h = kPi / panels;
    for (int p = 0; p < panels; p++) {
      const double lo = -kHalfPi + p * h;
      for (int i = 0; i < 16; i++) {
        const double f = lo + (x[i] + 1.0) * 0.5 * h;
        const double pfold = gauss_at(f) + gauss_at(kPi - f) + gauss_at(f - kTwoPi) + gauss_at(kPi - f + kTwoPi) +
                             gauss_at(f + kTwoPi) + gauss_at(kPi - f - kTwoPi);
        total += w[i] * pfold * std::cos(f);
      }
    }
    return total * 0.5 * h;
  }();
  EXPECT_NEAR(law.norm(), z_expected, 1e-12 * std::max(1.0, z_expected));
  EXPECT_NEAR(law.Evaluate(phi), p_fold_expected * std::cos(phi) / z_expected, 1e-11);
  EXPECT_NEAR(law.TotalMass(), 1.0, 1e-12);  // the law is normalized on [-pi/2, pi/2]
}

TEST(DeclaredDensity, LatitudeLegacyHasNoAreaJacobian) {
  // kGaussianLegacy: rho_phi = p_fold, no cos factor, integrates to 1 by the fold itself.
  const LatitudeDensity law = MakeLatitudeDensity(Legacy(30.0, 20.0));
  ASSERT_EQ(law.kind(), LatitudeLawKind::kFoldedNoArea);
  const double mu = 30.0 * kDeg, sigma = 20.0 * kDeg;
  auto gauss_at = [&](double x) {
    return std::exp(-0.5 * (x - mu) * (x - mu) / (sigma * sigma)) / (sigma * std::sqrt(kTwoPi));
  };
  const double phi = 60.0 * kDeg;
  const double p_fold_expected = gauss_at(phi) + gauss_at(180.0 * kDeg - phi);
  EXPECT_NEAR(law.Evaluate(phi), p_fold_expected, 1e-11);
  EXPECT_NEAR(law.TotalMass(), 1.0, 1e-12);
}

TEST(DeclaredDensity, LatitudeDegenerateKindsAreSplitUnfoldedVsFolded) {
  const LatitudeDensity unfolded = MakeLatitudeDensity(NoRandom(200.0));
  ASSERT_EQ(unfolded.kind(), LatitudeLawKind::kDiracUnfolded);
  EXPECT_NEAR(unfolded.point(), 200.0 * kDeg, 1e-15);  // taken raw, NO fold (random.cpp:172)
  const LatitudeDensity folded = MakeLatitudeDensity(Gauss(200.0, 0.0f));
  ASSERT_EQ(folded.kind(), LatitudeLawKind::kDiracFolded);
  // normalize_latitude(200 deg) = -20 deg with a flip (math.cpp:341): theta_z = 90 - 200 = -110
  // -> +250 (mod 360) -> > 180 -> reflect to 110 -> phi = -20 deg.
  EXPECT_NEAR(folded.point(), -20.0 * kDeg, 1e-12);
}

TEST(DeclaredDensity, LatitudeUniformFullSphereIdentity) {
  // The LUT-family closed form of uniform [90 +- 180] equals the full-sphere branch's law
  // cos(phi)/2 (the two sampler branches declare the same continuum measure):
  const LatitudeDensity law = MakeLatitudeDensity(Uniform(90.0, 360.0));
  ASSERT_EQ(law.kind(), LatitudeLawKind::kFoldedArea);
  for (double phi : { -80.0 * kDeg, -30.0 * kDeg, 0.0, 45.0 * kDeg, 89.0 * kDeg }) {
    EXPECT_NEAR(law.Evaluate(phi), std::cos(phi) * 0.5, 1e-12) << "phi = " << phi;
  }
}

TEST(DeclaredDensity, LatitudeClosedFormCdfMatchesTheProductionLut) {
  // The closed-form CDF vs BuildLatLut's own cdf table (the LUT target IS this law's continuum
  // limit; the tolerance is the LUT's float32/4096-bin/257-node resample error, measured then
  // pinned — 2e-3 absolute on the CDF scale).
  struct Case {
    Distribution slot;
    const char* name;
    double tol;
  };
  const Case cases[] = {
    { Gauss(30.0, 10.0), "gauss-30-10", 1e-3 },
    // The latitude band [60, 120] deg folds to [60, 90] with a HARD support edge; the LUT's own
    // 4096-bin histogram smears that edge (measured cdf deviation up to 3.5e-3 there — the
    // LUT's quadrature error, not a law mismatch; smooth configs hold 1e-3).
    { Uniform(90.0, 60.0), "uniform-90-60", 6e-3 },
    { Laplace(80.0, 15.0), "laplace-80-15", 1e-3 },
    // The zigzag law carries integrable 1/sqrt spikes (its rectified-sine folds); the LUT's
    // 65536-point U-quadrature concentrates its error there — measured cdf deviation 0.037.
    { Zigzag(30.0, 20.0), "zigzag-30-20", 5e-2 },
  };
  for (const Case& c : cases) {
    const LatitudeDensity law = MakeLatitudeDensity(c.slot);
    if (law.kind() != LatitudeLawKind::kFoldedArea) {
      ADD_FAILURE() << c.name << ": expected the area-measure fold law";
      continue;
    }
    const LatLut lut = BuildLatLut(c.slot);
    for (int pass = 0; pass < 2; pass++) {
      // Sample the node table at every 16th node (stride keeps the loop cheap; interior + ends).
      for (int n = (pass == 0 ? 0 : 8); n < static_cast<int>(LatLut::kNodes); n += 16) {
        const double theta_z = lut.theta[n];
        // F_LUT(theta_z) should equal integral of rho_phi over phi in [pi/2 - theta_z, pi/2].
        // SPIKE-AWARE: the zigzag law's fold-image 1/sqrt spikes (its arcsine support edges)
        // make a plain GL integral overshoot arbitrarily, so the window is split at the spike
        // phi-values (re-derived here independently: the support edges of |A sin t + B| and
        // their branch-B images) and the sqrt-substitution is applied at the window's spike
        // ends. The LUT side carries its own U-quadrature error at the same spikes (the tol).
        struct LawCtx {
          const LatitudeDensity* law;
        } ctx{ &law };
        auto f = [](double phi, void* p) { return static_cast<LawCtx*>(p)->law->Evaluate(phi); };
        const double lo_int = kHalfPi - theta_z;
        std::vector<double> sp;
        if (c.slot.type == DistributionType::kZigzag) {
          const double a_z = static_cast<double>(c.slot.spread) * kDeg;
          const double b_z = std::fabs(static_cast<double>(c.slot.center)) * kDeg;
          sp.push_back(std::max(0.0, b_z - a_z));
          sp.push_back(b_z + a_z);
          sp.push_back(kPi - (b_z + a_z));
          sp.push_back(kPi - std::max(0.0, b_z - a_z));
        }
        std::sort(sp.begin(), sp.end());
        std::vector<double> edges{ lo_int };
        for (double e : sp) {
          if (e > lo_int && e < kHalfPi) {
            edges.push_back(e);
          }
        }
        edges.push_back(kHalfPi);
        double expected = 0.0;
        for (size_t e = 0; e + 1 < edges.size(); e++) {
          const double a = edges[e], b = edges[e + 1];
          if (!(b > a)) {
            continue;
          }
          const bool spike_a = std::any_of(sp.begin(), sp.end(), [a](double v) { return std::fabs(v - a) < 1e-12; });
          const bool spike_b = std::any_of(sp.begin(), sp.end(), [b](double v) { return std::fabs(v - b) < 1e-12; });
          const double mid = 0.5 * (a + b);
          const int ns = 512;
          if (spike_a) {
            const double smax = std::sqrt(mid - a), ds = smax / ns;
            for (int j = 0; j < ns; j++) {
              const double sq = (j + 0.5) * ds;
              expected += f(a + sq * sq, &ctx) * 2.0 * sq * ds;
            }
          } else {
            expected += Integrate(f, &ctx, a, mid, 8, 24);
          }
          if (spike_b) {
            const double smax = std::sqrt(b - mid), ds = smax / ns;
            for (int j = 0; j < ns; j++) {
              const double sq = (j + 0.5) * ds;
              expected += f(b - sq * sq, &ctx) * 2.0 * sq * ds;
            }
          } else {
            expected += Integrate(f, &ctx, mid, b, 8, 24);
          }
        }
        EXPECT_NEAR(lut.cdf[n], expected, c.tol) << c.name << " node " << n << " theta_z " << theta_z;
      }
    }
  }
}

// ===========================================================================
// Table 3: the joint pose law.
// ===========================================================================

TEST(DeclaredDensity, JointPoseDensityReducesToTheProductWhenTheFoldIsNegligible) {
  // mu = 30 deg, sigma = 2 deg: branch B sits 33 sigma away, so f(phi) ~ 0 in the support and
  // the joint is the plain product of the slot laws at the composed angles.
  const AxisDistribution axis = Axis(Uniform(0.0, 360.0), Gauss(60.0, 2.0), Uniform(0.0, 360.0));
  const LatitudeDensity lat = MakeLatitudeDensity(axis.latitude_dist);
  const double lambda = 0.7, phi = 1.02, roll = 2.9;
  const double expected =
      lat.Evaluate(phi) * SlotDensityValue(axis.azimuth_dist, lambda) * SlotDensityValue(axis.roll_dist, roll);
  EXPECT_NEAR(DeclaredPoseDensity(axis, lambda, phi, roll), expected, 1e-12 * expected);
}

TEST(DeclaredDensity, JointPoseDensityCarriesTheFlipMixture) {
  // mu = 80 deg, sigma = 25 deg: branch B carries real mass. The mixture identity to pin: the
  // density at (lambda_out, roll_out) plus the density at the flipped partner (lambda_out + pi,
  // roll_out + pi) integrates against the phi-marginal consistently — checked as the torus
  // normalization: the joint integrates to 1 over lambda_out in [0, 2 pi], phi, roll_out in
  // [0, 2 pi].
  const AxisDistribution axis = Axis(Uniform(0.0, 360.0), Gauss(80.0, 25.0), Uniform(0.0, 360.0));
  const LatitudeDensity lat = MakeLatitudeDensity(axis.latitude_dist);  // built once, not per pose
  struct Ctx {
    const AxisDistribution* axis;
    const LatitudeDensity* lat;
  } ctx{ &axis, &lat };
  auto f = [](double phi, void* p) {
    const Ctx* c = static_cast<const Ctx*>(p);
    // Inner: 2D midpoint rule over the (lambda_out, roll_out) torus. MIDPOINTS, not nodes: a
    // node grid lands exactly on the uniform box's open seam (lambda_out = pi), where the law
    // is legitimately zero (a measure-zero puncture) and the coarse rule would read (23/24)^2.
    const int m = 24;
    const double h = kTwoPi / m;
    double sum = 0.0;
    for (int i = 0; i < m; i++) {
      for (int j = 0; j < m; j++) {
        sum += DeclaredPoseDensity(*c->lat, *c->axis, (i + 0.5) * h, phi, (j + 0.5) * h);
      }
    }
    return sum * h * h;
  };
  EXPECT_NEAR(Integrate(f, &ctx, -kHalfPi + 1e-6, kHalfPi - 1e-6, 32, 16), 1.0, 5e-3);
}

// ===========================================================================
// Table 4: the u-marginal.
// ===========================================================================

TEST(DeclaredDensity, USupportClassificationAndKinds) {
  // An OFF-pole sun: the sun at a pole is itself a declared-degenerate corner (the azimuth
  // degree of freedom collapses), pinned separately below.
  const double sun[3] = { 0.0, 0.6, 0.8 };
  EXPECT_EQ(MakeUMarginal(Axis(NoRandom(30.0), NoRandom(45.0), NoRandom(60.0)), sun).kind(), USupportKind::kPoint);
  // The plate family of the C12 config: fixed zenith 0 (latitude 90), uniform azimuth, fixed roll.
  const AxisDistribution plate = Axis(Uniform(0.0, 360.0), NoRandom(90.0), NoRandom(0.0));
  const UMarginal plate_m = MakeUMarginal(plate, sun);
  EXPECT_EQ(plate_m.kind(), USupportKind::kSpinOrbit);
  EXPECT_FALSE(plate_m.fast_path());
  // Column family: Gaussian zenith, uniform azimuth and roll — the fast family.
  const UMarginal column_m = MakeUMarginal(Axis(Uniform(0.0, 360.0), Gauss(60.0, 5.0), Uniform(0.0, 360.0)), sun);
  EXPECT_EQ(column_m.kind(), USupportKind::kArea);
  EXPECT_TRUE(column_m.fast_path());
  // Roll-only and latitude-only spreads.
  const UMarginal roll_m = MakeUMarginal(Axis(NoRandom(0.0), NoRandom(45.0), Uniform(0.0, 360.0)), sun);
  EXPECT_EQ(roll_m.kind(), USupportKind::kRollOrbit);
  const UMarginal lat_m = MakeUMarginal(Axis(NoRandom(30.0), Gauss(60.0, 5.0), NoRandom(0.0)), sun);
  EXPECT_EQ(lat_m.kind(), USupportKind::kLatitudeOrbit);
  // Sun at a pole: the declared degenerate corner (unless every slot is Dirac).
  const double pole_sun[3] = { 1e-13, 0.0, 1.0 };
  EXPECT_EQ(MakeUMarginal(Axis(Uniform(0.0, 360.0), Gauss(60.0, 5.0), Uniform(0.0, 360.0)), pole_sun).kind(),
            USupportKind::kDegenerateSunGeometry);
  EXPECT_EQ(MakeUMarginal(Axis(NoRandom(30.0), NoRandom(45.0), NoRandom(60.0)), pole_sun).kind(), USupportKind::kPoint);
  // Axis-Dirac at the pole with BOTH azimuth and roll spread: they act about the same body
  // axis, the support is one circle whose density is their convolution — degenerate, unanswered.
  EXPECT_EQ(MakeUMarginal(Axis(Uniform(0.0, 360.0), NoRandom(90.0), Uniform(0.0, 360.0)), sun).kind(),
            USupportKind::kDegenerateAxisGeometry);
  // The same axis with roll FIXED is the healthy plate spin orbit (the C12 family).
  EXPECT_EQ(MakeUMarginal(Axis(Uniform(0.0, 360.0), NoRandom(90.0), NoRandom(0.0)), sun).kind(),
            USupportKind::kSpinOrbit);
}

TEST(DeclaredDensity, DiracAzimuthAreaFamilyIsTheRegisteredReadAsZeroGap) {
  // The registered v1 gap (the header's list, review round 2): azimuth FIXED with latitude AND
  // roll spread classifies kArea, the fast predicates do not hold, and the general path zeroes
  // every preimage term — DensitySolidAngle reads 0 and MuPositive reads false everywhere. The
  // pin makes the gap VISIBLE (a classified kind answering zero), so an integration-time
  // consumer meets a registered "unanswered", not an undocumented "dark".
  const double sun[3] = { 0.0, 0.6, 0.8 };
  const AxisDistribution axis = Axis(NoRandom(30.0), Gauss(60.0, 5.0), Uniform(0.0, 360.0));
  const UMarginal m = MakeUMarginal(axis, sun);
  ASSERT_EQ(m.kind(), USupportKind::kArea);
  ASSERT_FALSE(m.fast_path());
  const double probes[3][3] = { { 0.0, 0.6, 0.8 }, { 0.3, -0.5, 0.81 }, { -0.7, 0.1, 0.7 } };
  for (const auto& u : probes) {
    EXPECT_EQ(m.DensitySolidAngle(u), 0.0);
    EXPECT_EQ(m.DensitySolidAngleGeneral(u), 0.0);
    EXPECT_FALSE(m.MuPositive(u, 1e-6));
  }
}

TEST(DeclaredDensity, UPointIsTheComposedRotationOfTheSun) {
  // u = Rz(-roll) Ry(pi/2 - phi) Rz(pi - az) s_hat, built here by explicit matrix products
  // (independent construction; the implementation's SpinOrbitPoint factors share the claim).
  const double az = 30.0 * kDeg, phi = 45.0 * kDeg, roll = 60.0 * kDeg;
  const double alt = 20.0 * kDeg, azim = 50.0 * kDeg;
  const double s[3] = { std::cos(alt) * std::cos(azim), std::cos(alt) * std::sin(azim), std::sin(alt) };
  double expected[3] = { s[0], s[1], s[2] };
  auto rz = [&](double a) {
    const double c = std::cos(a), sn = std::sin(a);
    const double x = expected[0] * c - expected[1] * sn;
    const double y = expected[0] * sn + expected[1] * c;
    expected[0] = x;
    expected[1] = y;
  };
  auto ry = [&](double a) {
    const double c = std::cos(a), sn = std::sin(a);
    const double x = expected[0] * c + expected[2] * sn;
    const double z = -expected[0] * sn + expected[2] * c;
    expected[0] = x;
    expected[2] = z;
  };
  rz(kPi - az);
  ry(kHalfPi - phi);
  rz(-roll);
  const UMarginal m = MakeUMarginal(Axis(NoRandom(az / kDeg), NoRandom(phi / kDeg), NoRandom(roll / kDeg)), s);
  ASSERT_EQ(m.kind(), USupportKind::kPoint);
  double u[3];
  m.Point(u);
  for (int i = 0; i < 3; i++) {
    EXPECT_NEAR(u[i], expected[i], 1e-12);
  }
}

TEST(DeclaredDensity, SpinOrbitEmbeddingDensityAndCombinationJacobian) {
  // The plate family (the C12 fiber) under an equatorial sun: u(theta) = Rz(pi - theta) s_hat,
  // the density w.r.t. dtheta is the azimuth law, and the ARC-length density is
  // rho_theta / |du/dtheta| = 1/(2 pi cos sigma) — the "1/(2 pi |cos phi_u|)" latitude-circle
  // form of the design vocabulary, with |du/dtheta| MEASURED by finite difference (the
  // combination Jacobian is verified, not assumed).
  const double alt = 9.0 * kDeg;
  const double s[3] = { std::cos(alt), 0.0, std::sin(alt) };
  const UMarginal m = MakeUMarginal(Axis(Uniform(0.0, 360.0), NoRandom(90.0), NoRandom(0.0)), s);
  ASSERT_EQ(m.kind(), USupportKind::kSpinOrbit);

  double u0[3], u1[3];
  m.SpinOrbitPoint(0.0, u0);
  m.SpinOrbitPoint(0.5 * kPi, u1);
  EXPECT_NEAR(u0[0], -std::cos(alt), 1e-12);  // Rz(pi) s_hat
  EXPECT_NEAR(u0[2], std::sin(alt), 1e-12);
  EXPECT_NEAR(u1[0], 0.0, 1e-12);
  EXPECT_NEAR(u1[1], std::cos(alt), 1e-12);  // Rz(pi/2) s_hat
  EXPECT_NEAR(u1[2], std::sin(alt), 1e-12);

  EXPECT_NEAR(m.SpinOrbitThetaDensity(1.234), 1.0 / kTwoPi, 1e-15);

  // Measured |du/dtheta| against the analytic cos(sigma), and the arc density derived from it.
  const double h = 1e-4;
  double ua[3], ub[3];
  m.SpinOrbitPoint(1.0 - h, ua);
  m.SpinOrbitPoint(1.0 + h, ub);
  const double speed = std::sqrt((ub[0] - ua[0]) * (ub[0] - ua[0]) + (ub[1] - ua[1]) * (ub[1] - ua[1]) +
                                 (ub[2] - ua[2]) * (ub[2] - ua[2])) /
                       (2.0 * h);
  EXPECT_NEAR(speed, std::cos(alt), 1e-6);
  EXPECT_NEAR(m.SpinOrbitThetaDensity(1.0) / speed, 1.0 / (kTwoPi * std::cos(alt)), 1e-6);
}

TEST(DeclaredDensity, ZonalFullSphereIsUniform) {
  const double sun[3] = { 0.3, -0.4, std::sqrt(1.0 - 0.09 - 0.16) };  // any off-pole sun
  const UMarginal m = MakeUMarginal(Axis(Uniform(0.0, 360.0), Uniform(90.0, 360.0), Uniform(0.0, 360.0)), sun);
  ASSERT_EQ(m.kind(), USupportKind::kArea);
  ASSERT_TRUE(m.fast_path());
  for (double z : { -0.9, -0.4, 0.0, 0.6, 0.95 }) {
    const double u[3] = { std::sqrt(1.0 - z * z), 0.0, z };
    EXPECT_NEAR(m.DensitySolidAngle(u), 1.0 / (4.0 * kPi), 1e-9) << "z = " << z;
  }
}

TEST(DeclaredDensity, ZonalArcsineRingForAFixedZenithFamily) {
  // Dirac latitude at phi_0 with uniform azimuth and roll: the u-marginal is zonal with
  // m(c) the arcsine law on (-cos(phi_0 + sigma), cos(phi_0 - sigma)) — closed form, and the
  // Chebyshev machinery must reproduce it (the ring the certificate's plate family reads).
  const double phi0 = 70.0 * kDeg, alt = 30.0 * kDeg;
  const double s[3] = { std::cos(alt), 0.0, std::sin(alt) };
  // The latitude slot stores latitude: phi_0 = 70 deg means zenith 20 deg.
  const UMarginal m = MakeUMarginal(Axis(Uniform(0.0, 360.0), NoRandom(phi0 / kDeg), Uniform(0.0, 360.0)), s);
  ASSERT_EQ(m.kind(), USupportKind::kArea);
  ASSERT_TRUE(m.fast_path());
  const double c_lo = -std::cos(phi0 + alt), c_hi = std::cos(phi0 - alt);
  EXPECT_GT(c_hi, c_lo);
  for (double frac : { 0.15, 0.4, 0.6, 0.85 }) {
    const double c = c_lo + frac * (c_hi - c_lo);
    const double expected = 1.0 / (kPi * std::sqrt((c - c_lo) * (c_hi - c)));
    const double u[3] = { std::sqrt(1.0 - c * c), 0.0, c };
    // rho_u = m(c)/(2 pi) w.r.t. dOmega.
    EXPECT_NEAR(m.DensitySolidAngle(u), expected / kTwoPi, 2e-3 * expected) << "c = " << c;
  }
  // Off the ring: zero (outside the arcsine interval the density vanishes). The probe is chosen
  // PROVABLY outside — 0.95 > c_hi = cos(phi0 - alt) = cos(40 deg) ~ 0.766 — and the relation is
  // asserted before use: a conditional negative once executed nothing here (an in-range 0.5
  // probe made the branch dead while reading as coverage).
  ASSERT_GT(0.95, c_hi);
  const double c_out = 0.95;
  const double out_u[3] = { std::sqrt(1.0 - c_out * c_out), 0.0, c_out };
  EXPECT_EQ(m.DensitySolidAngle(out_u), 0.0);
}

double ZonalMassCtxF(double z, void* ctx);

namespace {
struct ZonalCtx {
  const UMarginal* m;
};
}  // namespace

double ZonalMassCtxF(double z, void* ctx) {
  const ZonalCtx* c = static_cast<ZonalCtx*>(ctx);
  const double u[3] = { std::sqrt(std::max(1.0 - z * z, 0.0)), 0.0, z };
  return c->m->DensitySolidAngle(u) * kTwoPi;  // zonal: integral over dOmega = 2 pi * f(z) dz
}

TEST(DeclaredDensity, ZonalDensityNormalizesOverTheUSphere) {
  // The u-S^2 support is the normalization domain (the plan's pinned reading): integral of
  // rho_u over dOmega = 1, checked through the zonal form 2 pi * integral f(z) dz. The
  // Gaussian-column integrand is smooth in z away from its soft tails; the arcsine ring case
  // re-uses the closed form's own interval.
  const double sun[3] = { 0.6, 0.0, 0.8 };
  {
    const UMarginal m = MakeUMarginal(Axis(Uniform(0.0, 360.0), Gauss(60.0, 8.0), Uniform(0.0, 360.0)), sun);
    ZonalCtx ctx{ &m };
    EXPECT_NEAR(Integrate(ZonalMassCtxF, &ctx, -1.0 + 1e-9, 1.0 - 1e-9, 96, 24), 1.0, 2e-4);
  }
  {
    // Legacy latitude (no area Jacobian) under the fast family: still a probability measure.
    const UMarginal m = MakeUMarginal(Axis(Uniform(0.0, 360.0), Legacy(60.0, 8.0), Uniform(0.0, 360.0)), sun);
    ZonalCtx ctx{ &m };
    EXPECT_NEAR(Integrate(ZonalMassCtxF, &ctx, -1.0 + 1e-9, 1.0 - 1e-9, 96, 24), 1.0, 2e-4);
  }
}

TEST(DeclaredDensity, FastAndGeneralPathsAgreeOnTheFastFamily) {
  // The equivalence claim of Table 4: on a config where both apply, the zonal fast path and the
  // preimage general path agree to the general path's 512-panel roll-midpoint accuracy.
  const double sun[3] = { 0.6, 0.0, 0.8 };
  const UMarginal m = MakeUMarginal(Axis(Uniform(0.0, 360.0), Gauss(60.0, 10.0), Uniform(0.0, 360.0)), sun);
  ASSERT_TRUE(m.fast_path());
  for (double z : { 0.1, 0.35, 0.6, 0.85 }) {
    const double lon = 0.9;  // the fast value is zonal; the general value must not see the longitude
    const double u[3] = { std::sqrt(1.0 - z * z) * std::cos(lon), std::sqrt(1.0 - z * z) * std::sin(lon), z };
    const double fast = m.DensitySolidAngle(u);
    const double general = m.DensitySolidAngleGeneral(u);
    if (!(fast > 0.0)) {
      ADD_FAILURE() << "z = " << z << ": the fast path must be positive in the support";
      continue;
    }
    // The general path integrates the fold onsets with the sqrt-substitution; its residual
    // against the exact zonal answer is quadrature-level (measured below 1e-4 relative here).
    EXPECT_NEAR(general, fast, 1e-3 * fast) << "z = " << z;
  }
}

TEST(DeclaredDensity, GeneralPathHandlesDiracRollAndSpreadAzimuth) {
  // az uniform, latitude Gaussian, roll FIXED: a legal kArea config the fast predicates refuse
  // (roll is not full-turn uniform). The general path answers with the single roll slice; the
  // oracle is a direct (lambda, phi) grid pushforward through the same map formula the spec
  // states — an independent numeric marginal (cap-mass / cap-area over a deterministic grid).
  const double alt = 25.0 * kDeg;
  const double s[3] = { std::cos(alt), 0.0, std::sin(alt) };
  const AxisDistribution axis = Axis(Uniform(0.0, 360.0), Gauss(60.0, 12.0), NoRandom(0.0));
  const UMarginal m = MakeUMarginal(axis, s);
  ASSERT_EQ(m.kind(), USupportKind::kArea);
  ASSERT_FALSE(m.fast_path());

  // Probe z = 0.2: the Gaussian axis phi ~ N(60 deg, 12 deg) with the sun at 25 deg puts a
  // healthy density here (O(0.06); deeper probes fall toward the Gaussian tail where the cap
  // oracle cannot resolve).
  const double u0[3] = { std::sqrt(1.0 - 0.04), 0.0, 0.2 };
  const double analytic = m.DensitySolidAngle(u0);
  ASSERT_GT(analytic, 0.0);

  // Deterministic grid oracle: lambda x phi midpoints, mass in a small cap around u0 divided by
  // the cap's solid angle. Roll is Dirac at 0; the map is u(lambda, phi) per the spec table.
  const LatitudeDensity lat = MakeLatitudeDensity(axis.latitude_dist);
  const int n_lam = 1440, n_phi = 720;
  const double cap = 3.0 * kDeg;
  double mass = 0.0;
  for (int i = 0; i < n_lam; i++) {
    const double lam = (i + 0.5) * kTwoPi / n_lam;
    for (int j = 0; j < n_phi; j++) {
      const double phi = -kHalfPi + (j + 0.5) * kPi / n_phi;
      double v[3] = { s[0], s[1], s[2] };
      // u = Rz(0) Ry(pi/2 - phi) Rz(pi - lambda) s_hat — build by rotating s_hat.
      const double w_lon = std::atan2(s[1], s[0]) + kPi - lam;
      double w[3] = { std::cos(alt) * std::cos(w_lon), std::cos(alt) * std::sin(w_lon), std::sin(alt) };
      const double th = kHalfPi - phi;
      const double cw = std::cos(th), sw = std::sin(th);
      v[0] = w[0] * cw + w[2] * sw;
      v[1] = w[1];
      v[2] = -w[0] * sw + w[2] * cw;
      const double dot = v[0] * u0[0] + v[1] * u0[1] + v[2] * u0[2];
      if (dot > std::cos(cap)) {
        mass += lat.Evaluate(phi) * (1.0 / kTwoPi) * (kTwoPi / n_lam) * (kPi / n_phi);
      }
    }
  }
  const double cap_area = kTwoPi * (1.0 - std::cos(cap));
  // The grid oracle smears the cap boundary: 5% relative agreement is the honest bar for a
  // 3-degree cap at this grid density (boundary cells dominate the error), and it is enough to
  // catch a wrong Jacobian or a wrong preimage sign — either is an O(1) error.
  EXPECT_NEAR(analytic, mass / cap_area, 0.05 * mass / cap_area);
}

TEST(DeclaredDensity, MuPositiveAcrossSupportKinds) {
  const double alt = 9.0 * kDeg;
  const double s[3] = { std::cos(alt), 0.0, std::sin(alt) };

  // Plate family: on-orbit points are positive, off-orbit latitudes are not.
  const UMarginal plate = MakeUMarginal(Axis(Uniform(0.0, 360.0), NoRandom(90.0), NoRandom(0.0)), s);
  double on[3], off[3];
  plate.SpinOrbitPoint(1.7, on);
  off[0] = std::cos(60.0 * kDeg) * on[0] / std::cos(alt);
  off[1] = std::cos(60.0 * kDeg) * on[1] / std::cos(alt);
  off[2] = std::sin(60.0 * kDeg);
  EXPECT_TRUE(plate.MuPositive(on, 1e-6));
  EXPECT_FALSE(plate.MuPositive(off, 1e-6));

  // Area support: the latitude band [60, 120] deg folds to phi in [60, 90]; with the sun at
  // latitude 9 the u_z range is cos(angle(axis, sun)) over axis phi in [60, 90] and all
  // longitudes: angle in [51, 111] deg, u_z in [-0.36, 0.63]. Probe inside and outside THAT.
  const UMarginal area = MakeUMarginal(Axis(Uniform(0.0, 360.0), Uniform(90.0, 60.0), Uniform(0.0, 360.0)), s);
  const double z_in = 0.3;  // angle ~ 72.5 deg: reachable
  const double inside[3] = { std::sqrt(1.0 - z_in * z_in), 0.0, z_in };
  const double z_out = 0.9;  // angle ~ 26 deg: not reachable from any axis in the band
  const double outside[3] = { std::sqrt(1.0 - z_out * z_out), 0.0, z_out };
  EXPECT_TRUE(area.MuPositive(inside, 1e-9));
  EXPECT_FALSE(area.MuPositive(outside, 1e-9));

  // Point support: the single pose Rz(0)Ry(0)Rz(pi - 0) gives u = Rz(pi) s_hat (the composed
  // azimuth's -pi shift is part of the map), NOT the sun position itself.
  const UMarginal point = MakeUMarginal(Axis(NoRandom(0.0), NoRandom(90.0), NoRandom(0.0)), s);
  double u_point[3];
  point.Point(u_point);
  const double expected_point[3] = { -s[0], -s[1], s[2] };  // Rz(pi) s_hat
  for (int i = 0; i < 3; i++) {
    EXPECT_NEAR(u_point[i], expected_point[i], 1e-12);
  }
  EXPECT_TRUE(point.MuPositive(expected_point, 1e-6));
  EXPECT_FALSE(point.MuPositive(s, 1e-6));
}

TEST(DeclaredDensity, OverlapFamilyCrossTableAgainstPoseDensity) {
  // The deliberate double implementation's agreement condition (declared_density.hpp's relation
  // block): zenith Gaussian + full-turn uniform azimuth and roll, the +-12 sigma window strictly
  // inside (0, pi) so LI's clipped g equals the unclipped one and the fold branches are ~e^{-500}.
  // Conversion: rho_Haar = 8 pi^2 * rho_angle / cos(phi) at the same composed angles.
  const double zenith_mean = 30.0, zenith_std = 2.0;
  analytic::PoseDensitySpec spec;
  spec.family = analytic::PoseFamily::kPlate;  // label only; the mean is carried explicitly
  spec.zenith_mean_deg = zenith_mean;
  spec.zenith_std_deg = zenith_std;
  ASSERT_STREQ(analytic::PoseDensityError(spec), "");
  const analytic::PoseDensity li(spec);

  const AxisDistribution axis = Axis(Uniform(0.0, 360.0), Gauss(90.0 - zenith_mean, zenith_std), Uniform(0.0, 360.0));
  for (double phi : { 50.0 * kDeg, 60.0 * kDeg, 61.0 * kDeg }) {
    for (double lam : { 0.3, 2.2, 4.9 }) {
      const double roll = 1.1;
      const double mine = DeclaredPoseDensity(axis, lam, phi, roll);
      // The pose's third row (world z of the body axes): e = (cos phi cos lam, cos phi sin lam,
      // sin phi) — theta = arccos(e3) = pi/2 - phi is the zenith angle LI reads.
      const double e1 = std::cos(phi) * std::cos(lam), e2 = std::cos(phi) * std::sin(lam);
      const double e3 = std::sin(phi);
      const double li_haar = li.Evaluate(e1, e2, e3);
      const double converted = mine * 8.0 * kPi * kPi / std::cos(phi);
      EXPECT_NEAR(converted, li_haar, 1e-9 * li_haar) << "phi = " << phi << " lam = " << lam;
    }
  }
}

TEST(DeclaredDensity, SpinOrbitAndRollOrbitDensitiesAreWrappedOnTheCircle) {
  // The orbit parameters are circle quantities (u(theta) = u(theta + 2 pi)), so the density
  // entries read the WRAPPED slot law — the same authority MuPositive's membership leg uses. A
  // seam-crossing support (azimuth Uniform(180 +- 20): support [160, 200] deg) has its second
  // half land in (-180, -160] under a producer's atan2 parameter convention; the unwrapped slot
  // value reads ZERO there and a kFiberParameter quadrature would silently drop those samples
  // (round 3's Major: the one authority reading the unwrapped law in a wrapped codebase).
  const double alt = 9.0 * kDeg;
  const double s[3] = { std::cos(alt), 0.0, std::sin(alt) };
  const UMarginal spin = MakeUMarginal(Axis(Uniform(180.0, 40.0), NoRandom(90.0), NoRandom(0.0)), s);
  ASSERT_EQ(spin.kind(), USupportKind::kSpinOrbit);
  const double expected = 1.0 / (40.0 * kDeg);
  const double in_support = 190.0 * kDeg;   // inside [160, 200]
  const double seam_image = -170.0 * kDeg;  // the SAME circle point, atan2 convention
  const double off = 100.0 * kDeg;          // outside the support
  double u_seam[3], u_support[3];
  spin.SpinOrbitPoint(seam_image, u_seam);
  spin.SpinOrbitPoint(in_support, u_support);
  for (int i = 0; i < 3; i++) {
    EXPECT_NEAR(u_seam[i], u_support[i], 1e-12);  // the parameters name one circle point
  }
  EXPECT_NEAR(spin.SpinOrbitThetaDensity(in_support), expected, 1e-12);
  EXPECT_NEAR(spin.SpinOrbitThetaDensity(seam_image), expected, 1e-12);  // wrapped: positive
  EXPECT_EQ(spin.SpinOrbitThetaDensity(in_support + kTwoPi), expected);  // periodic
  EXPECT_EQ(spin.SpinOrbitThetaDensity(off), 0.0);

  // The roll orbit's parameter is a circle quantity too: same seam behavior through
  // OrbitDensity (which also writes the same orbit point for both spellings).
  const UMarginal roll = MakeUMarginal(Axis(NoRandom(0.0), NoRandom(45.0), Uniform(180.0, 40.0)), s);
  ASSERT_EQ(roll.kind(), USupportKind::kRollOrbit);
  double u0[3], u1[3];
  EXPECT_NEAR(roll.OrbitDensity(in_support, u0), expected, 1e-12);
  EXPECT_NEAR(roll.OrbitDensity(seam_image, u1), expected, 1e-12);
  for (int i = 0; i < 3; i++) {
    EXPECT_NEAR(u0[i], u1[i], 1e-12);
  }
  // The latitude orbit's parameter is NOT a circle quantity: the folded law is read raw and is
  // zero outside [-pi/2, pi/2] — the asymmetry is the law, not an oversight.
  const UMarginal lat = MakeUMarginal(Axis(NoRandom(0.0), Gauss(60.0, 5.0), NoRandom(0.0)), s);
  ASSERT_EQ(lat.kind(), USupportKind::kLatitudeOrbit);
  EXPECT_GT(lat.OrbitDensity(60.0 * kDeg, nullptr), 0.0);
  EXPECT_EQ(lat.OrbitDensity(60.0 * kDeg + kTwoPi, nullptr), 0.0);
}

TEST(DeclaredDensity, DensityEntriesGuardTheirSupportKind) {
  // Guards mirror DensitySolidAngle's: a density entry read on a foreign support kind answers
  // 0 (the caller checks kind()), never a plausible-looking number that is the density of
  // nothing — SlotDensityValue of a spread slot on an area measure was exactly that.
  const double sun[3] = { 0.0, 0.6, 0.8 };
  const UMarginal area = MakeUMarginal(Axis(Uniform(0.0, 360.0), Gauss(60.0, 5.0), Uniform(0.0, 360.0)), sun);
  ASSERT_EQ(area.kind(), USupportKind::kArea);
  EXPECT_EQ(area.SpinOrbitThetaDensity(1.0), 0.0);
  EXPECT_EQ(area.OrbitDensity(1.0, nullptr), 0.0);
  const UMarginal point = MakeUMarginal(Axis(NoRandom(30.0), NoRandom(45.0), NoRandom(60.0)), sun);
  ASSERT_EQ(point.kind(), USupportKind::kPoint);
  EXPECT_EQ(point.SpinOrbitThetaDensity(1.0), 0.0);
  EXPECT_EQ(point.OrbitDensity(1.0, nullptr), 0.0);
}

TEST(DeclaredDensity, DiracLatitudeGeneralPathIsTheRegisteredReadAsZeroGap) {
  // The second registered read-as-zero family (the header's v1 list; pinned visible like the
  // Dirac-azimuth one above): latitude FIXED with azimuth AND roll spread classifies kArea
  // with the fast predicates false, and the general path does not solve the Dirac-latitude
  // preimage — DensitySolidAngle reads 0 and MuPositive reads false everywhere. Zero is
  // "unanswered": integration must treat this family as unanswerable until the v2 preimage
  // solve (the quadrature/profile registrations point here).
  const double sun[3] = { 0.0, 0.6, 0.8 };
  const AxisDistribution axis = Axis(Gauss(30.0, 10.0), NoRandom(60.0), Uniform(0.0, 360.0));
  const UMarginal m = MakeUMarginal(axis, sun);
  ASSERT_EQ(m.kind(), USupportKind::kArea);
  ASSERT_FALSE(m.fast_path());
  const double probes[3][3] = { { 0.0, 0.6, 0.8 }, { 0.3, -0.5, 0.81 }, { -0.7, 0.1, 0.7 } };
  for (const auto& u : probes) {
    EXPECT_EQ(m.DensitySolidAngle(u), 0.0);
    EXPECT_EQ(m.DensitySolidAngleGeneral(u), 0.0);
    EXPECT_FALSE(m.MuPositive(u, 1e-6));
  }
}

TEST(DeclaredDensity, OrbitKindMuPositiveDistanceOnlyIsTheRegisteredV1Approximation) {
  // The registered v1 gap made visible (the header's list): kRollOrbit / kLatitudeOrbit answer
  // membership by DISTANCE-TO-ORBIT only — a point on the orbit circle OUTSIDE the law's
  // parameter support reads positive, because the parameter density is not inverted. Pinned so
  // integration meets a declared approximation, not an accident; the inversion is the v2 fill.
  const double sun[3] = { 0.0, 0.6, 0.8 };
  // Roll law Uniform(90, 180) (support [0, 180) deg): the circle point at r = 270 deg is on
  // the orbit but outside the parameter support (its wrapped density is 0) — distance-to-orbit
  // still answers true.
  const UMarginal roll = MakeUMarginal(Axis(NoRandom(0.0), NoRandom(45.0), Uniform(90.0, 180.0)), sun);
  ASSERT_EQ(roll.kind(), USupportKind::kRollOrbit);
  double on_circle_outside[3], on_circle_inside[3];
  roll.OrbitDensity(270.0 * kDeg, on_circle_outside);
  roll.OrbitDensity(30.0 * kDeg, on_circle_inside);
  EXPECT_EQ(roll.OrbitDensity(270.0 * kDeg, nullptr), 0.0);  // the density honestly reads 0 there
  EXPECT_TRUE(roll.MuPositive(on_circle_inside, 1e-9));
  EXPECT_TRUE(roll.MuPositive(on_circle_outside, 1e-9));  // THE registered approximation
  const double pole[3] = { 0.0, 0.0, 1.0 };               // a genuinely off-circle point
  EXPECT_FALSE(roll.MuPositive(pole, 1e-9));
  // Same shape for the latitude orbit: phi = 200 deg is outside the folded law's [-90, 90].
  const UMarginal lat = MakeUMarginal(Axis(NoRandom(0.0), Gauss(60.0, 5.0), NoRandom(0.0)), sun);
  ASSERT_EQ(lat.kind(), USupportKind::kLatitudeOrbit);
  double lat_outside[3];
  lat.OrbitDensity(200.0 * kDeg, lat_outside);
  EXPECT_EQ(lat.OrbitDensity(200.0 * kDeg, nullptr), 0.0);
  EXPECT_TRUE(lat.MuPositive(lat_outside, 1e-9));
}

TEST(DeclaredDensity, TotalMassIsHonestForEveryFoldedLaw) {
  // The accessor shares the Z integral's fold-aware split: the spike laws (kZigzag) and the
  // kinked fold-active laws (kLaplacian's branch-B kink image) integrate to 1, and so does an
  // extreme support whose fold image only lands through the DERIVED k window (|center| + spread
  // beyond a full turn — the case a hardcoded k in {-1,0,1} silently dropped: the image at
  // +30 deg of the 750 deg support edge).
  EXPECT_NEAR(MakeLatitudeDensity(Zigzag(30.0, 20.0)).TotalMass(), 1.0, 1e-3);
  EXPECT_NEAR(MakeLatitudeDensity(Zigzag(650.0, 100.0)).TotalMass(), 1.0, 3e-3);
  EXPECT_NEAR(MakeLatitudeDensity(Laplace(120.0, 30.0)).TotalMass(), 1.0, 1e-3);
  EXPECT_NEAR(MakeLatitudeDensity(Gauss(30.0, 20.0)).TotalMass(), 1.0, 1e-12);
  EXPECT_NEAR(MakeLatitudeDensity(Legacy(30.0, 20.0)).TotalMass(), 1.0, 1e-12);
  EXPECT_NEAR(MakeFullSphereLatitude().TotalMass(), 1.0, 1e-12);
}

TEST(DeclaredDensity, ZigzagLatitudeFastPathNormalizesAndAgreesWithTheGeneralPath) {
  // The fast path's fold-image spike split (round 3's Major: the unsplit rule underreads the
  // zigzag family at the arcsine edges while the header claimed exactness): the zonal integral
  // must normalize over the u-sphere, and the general (preimage) path — an independent
  // authority — must agree at its own roll-quadrature floor.
  const double sun[3] = { 0.6, 0.0, 0.8 };
  const UMarginal m = MakeUMarginal(Axis(Uniform(0.0, 360.0), Zigzag(30.0, 20.0), Uniform(0.0, 360.0)), sun);
  ASSERT_EQ(m.kind(), USupportKind::kArea);
  ASSERT_TRUE(m.fast_path());
  ZonalCtx ctx{ &m };
  EXPECT_NEAR(Integrate(ZonalMassCtxF, &ctx, -1.0 + 1e-9, 1.0 - 1e-9, 96, 24), 1.0, 2e-3);
  for (double z : { 0.15, 0.45 }) {
    const double lon = 0.9;
    const double u[3] = { std::sqrt(1.0 - z * z) * std::cos(lon), std::sqrt(1.0 - z * z) * std::sin(lon), z };
    const double fast = m.DensitySolidAngle(u);
    if (!(fast > 0.0)) {
      ADD_FAILURE() << "z = " << z << ": the fast path must be positive in the support";
      continue;
    }
    const double general = m.DensitySolidAngleGeneral(u);
    // Pinned at the MEASURED pair floor on this family (4.2% at z = 0.45): the general path's
    // roll midpoint undershoots the rho_phi fold-image spikes its panels cross — the same
    // physical spike the fast path now splits at its panel edges. Direction and size are the
    // registered degraded accuracy of DensitySolidAngleGeneral on spiky-latitude families
    // (the fast path is the production answer here; the general path is the equivalence check).
    EXPECT_NEAR(general, fast, 5e-2 * fast) << "z = " << z;
  }
}


}  // namespace
}  // namespace lumice::raypath
