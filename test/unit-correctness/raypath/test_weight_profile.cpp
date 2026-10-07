// The M2 weight-profile primitive (src/raypath/detail/measure/weight_profile.hpp): rho_u read
// along a kind-1 curve and the declared trapezoid line integral, on mock curves with analytic
// densities. The LI numeric anchor is pre-declared suspended (fiber_quadrature.hpp's spec block:
// no kind-1 curve fixture exists until the geometry-layer port) — this file pins the PRIMITIVE.
//
// symmetry_semantics: none.

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "raypath/detail/measure/declared_density.hpp"
#include "raypath/detail/measure/measure_geometry_contract.hpp"
#include "raypath/detail/measure/weight_profile.hpp"

namespace lumice::raypath {
namespace {

constexpr double kPi = 3.14159265358979323846;
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

AxisDistribution MakeAxis(Distribution az, Distribution lat, Distribution roll) {
  AxisDistribution a;
  a.azimuth_dist = az;
  a.latitude_dist = lat;
  a.roll_dist = roll;
  return a;
}

CriticalSetCurve LatitudeCircleCurve(double lat_rad, int points, bool closed) {
  CriticalSetCurve curve;
  curve.existence = ExistenceState::kComputed;
  curve.u.resize(3 * static_cast<size_t>(points));
  for (int i = 0; i < points; i++) {
    const double lon = 2.0 * kPi * i / points;
    curve.u[3 * i + 0] = std::cos(lat_rad) * std::cos(lon);
    curve.u[3 * i + 1] = std::cos(lat_rad) * std::sin(lon);
    curve.u[3 * i + 2] = std::sin(lat_rad);
  }
  curve.closed = closed;
  return curve;
}

TEST(WeightProfile, UniformMeasureReadsConstantAlongALatitudeCircle) {
  // Full-sphere uniform axis: rho_u = 1/(4 pi) everywhere, so the profile of any curve is the
  // constant and the line integral is (1/(4 pi)) times the polyline's arclength. The sun must
  // be OFF the pole (a pole sun is the declared-degenerate corner and reads zero).
  const double sun[3] = { 0.0, 0.6, 0.8 };
  const UMarginal m = MakeUMarginal(MakeAxis(Uniform(0.0, 360.0), Uniform(90.0, 360.0), Uniform(0.0, 360.0)), sun);
  const double lat = 30.0 * kDeg;
  const int points = 64;
  const CriticalSetCurve curve = LatitudeCircleCurve(lat, points, true);
  const WeightProfile profile = SampleWeightProfile(m, curve);
  ASSERT_EQ(profile.points.size(), static_cast<size_t>(points));
  for (const WeightProfileSample& p : profile.points) {
    EXPECT_NEAR(p.rho_u, 1.0 / (4.0 * kPi), 1e-9);
  }
  EXPECT_EQ(profile.in_support, points);
  // Closed circle's chord-sum arclength approaches 2 pi cos(lat) from below as the polyline
  // refines; at 64 points the chord-vs-arc deficit is circumference * (chord angle)^2 / 24
  // ~ 2.2e-3 — the polyline's own geometry, not the profile's.
  const double arc = profile.points.back().s + std::sqrt(std::pow(curve.u[0] - curve.u[3 * (points - 1)], 2) +
                                                         std::pow(curve.u[1] - curve.u[3 * (points - 1) + 1], 2) +
                                                         std::pow(curve.u[2] - curve.u[3 * (points - 1) + 2], 2));
  EXPECT_NEAR(arc, 2.0 * kPi * std::cos(lat), 3e-3);
  EXPECT_NEAR(profile.total, (1.0 / (4.0 * kPi)) * 2.0 * kPi * std::cos(lat), 2e-4);
}

TEST(WeightProfile, ZonalGaussianProfileMatchesTheClosedFormIntegral) {
  // Column family under an equatorial sun at longitude 0: the zonal fast density at latitude
  // phi_u is m(sin phi_u)/(2 pi). Along the equator the profile is constant; along a meridian it
  // is the zonal density sampled in z — the trapezoid over the polyline must approach the exact
  // integral of the profile over the meridian arc, which the test computes from the same closed
  // zonal form by fine quadrature (independent rule: 2000-point trapezoid on the exact curve,
  // against the primitive's polyline trapezoid at its own resolution).
  const double sun[3] = { 1.0, 0.0, 0.0 };
  const UMarginal m = MakeUMarginal(MakeAxis(Uniform(0.0, 360.0), Gauss(60.0, 10.0), Uniform(0.0, 360.0)), sun);
  ASSERT_TRUE(m.fast_path());

  // A meridian arc from latitude -60 deg to +85 deg, at longitude 0 (the curve enters the
  // latitude band around 60 deg only there).
  const int points = 241;
  const double lat_lo = -60.0 * kDeg, lat_hi = 85.0 * kDeg;
  CriticalSetCurve curve;
  curve.existence = ExistenceState::kComputed;
  curve.u.resize(3 * static_cast<size_t>(points));
  for (int i = 0; i < points; i++) {
    const double lat = lat_lo + (lat_hi - lat_lo) * i / (points - 1);
    curve.u[3 * i + 0] = std::cos(lat);
    curve.u[3 * i + 1] = 0.0;
    curve.u[3 * i + 2] = std::sin(lat);
  }
  const WeightProfile profile = SampleWeightProfile(m, curve);
  // The Gaussian's double-precision tails keep rho_u strictly positive over the whole arc (the
  // meridian's far end sits ~3 sigma + geometry from the band centre — e^{−4.5}, not zero), so
  // the profile shows a VARIATION of many orders of magnitude, not an inside/outside split.
  EXPECT_EQ(profile.in_support, points);

  // Independent oracle: fine trapezoid of rho_u over the same arc, exact curve geometry.
  const int fine = 40001;
  double oracle = 0.0;
  auto rho_at = [&](double lat) {
    const double u[3] = { std::cos(lat), 0.0, std::sin(lat) };
    return m.DensitySolidAngle(u);
  };
  for (int i = 0; i + 1 < fine; i++) {
    const double a = lat_lo + (lat_hi - lat_lo) * i / (fine - 1);
    const double b = lat_lo + (lat_hi - lat_lo) * (i + 1) / (fine - 1);
    oracle += 0.5 * (rho_at(a) + rho_at(b)) * (b - a);
  }
  EXPECT_NEAR(profile.total, oracle, 2e-3 * std::max(oracle, 1e-9));
}

TEST(WeightProfile, EscapedAndEmptyCurvesPassthroughTheirExistence) {
  const double sun[3] = { 0.0, 0.6, 0.8 };
  const UMarginal m = MakeUMarginal(MakeAxis(Uniform(0.0, 360.0), Gauss(60.0, 10.0), Uniform(0.0, 360.0)), sun);
  CriticalSetCurve empty;
  empty.existence = ExistenceState::kComputed;  // computed EMPTY set: data, no points
  const WeightProfile p0 = SampleWeightProfile(m, empty);
  EXPECT_TRUE(p0.points.empty());
  EXPECT_EQ(p0.total, 0.0);
  EXPECT_EQ(p0.existence, ExistenceState::kComputed);

  CriticalSetCurve escaped;
  escaped.existence = ExistenceState::kEscaped;
  const WeightProfile p1 = SampleWeightProfile(m, escaped);
  EXPECT_EQ(p1.existence, ExistenceState::kEscaped);
}

TEST(WeightProfile, OrbitMeasureReadsZeroIsTheRegisteredGap) {
  // The profile-side registration (the header's read-as-zero block): rho_u is the AREA density,
  // so a curve under the plate family's kSpinOrbit measure — the contract header's canonical
  // closed kind-1 curve — reads zero along the WHOLE curve with in_support = 0, indistinguishable
  // from a curve outside the support. The pin makes the registered "unanswered" visible; the
  // along-orbit parameter reading (UMarginal::OrbitDensity) is the v2 variant, drawing on the
  // same per-kind parameter inversion registered in declared_density.hpp's gap list.
  const double sun[3] = { 0.0, 0.6, 0.8 };
  const UMarginal plate = MakeUMarginal(MakeAxis(Uniform(0.0, 360.0), NoRandom(90.0), NoRandom(0.0)), sun);
  ASSERT_EQ(plate.kind(), USupportKind::kSpinOrbit);
  const CriticalSetCurve curve = LatitudeCircleCurve(30.0 * kDeg, 32, true);
  const WeightProfile profile = SampleWeightProfile(plate, curve);
  ASSERT_EQ(profile.points.size(), static_cast<size_t>(32));
  for (const WeightProfileSample& p : profile.points) {
    EXPECT_EQ(p.rho_u, 0.0);
  }
  EXPECT_EQ(profile.in_support, 0);
  EXPECT_EQ(profile.total, 0.0);
  EXPECT_EQ(profile.existence, ExistenceState::kComputed);
  // The point kind reads the same registered zero (a point mass has no dOmega density; the
  // delta reading is the same v2 per-kind variant).
  const UMarginal point = MakeUMarginal(MakeAxis(NoRandom(30.0), NoRandom(45.0), NoRandom(60.0)), sun);
  ASSERT_EQ(point.kind(), USupportKind::kPoint);
  const WeightProfile p2 = SampleWeightProfile(point, curve);
  EXPECT_EQ(p2.in_support, 0);
  EXPECT_EQ(p2.total, 0.0);
}

}  // namespace
}  // namespace lumice::raypath
