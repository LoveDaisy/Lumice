// The contour layer (src/analytic/dp_contour.hpp, scrum 660.5): the declared curve-quadrature
// rules on analytic examples (great-circle arc length, latitude-circle perimeter and its zonal
// load, trigonometric exactness and spectral decay, kink cuts), the orbit fiber-stream producer
// (bit-parity with the handwritten grid it productizes, u fidelity, the A/T split, the pole-sun
// degeneracy), the restricted family curve (the C11 plate family's pinned circle), and the
// kind-2/kind-3 chain mappings. The docking to the measure layer (scrum 661's contract) lives in
// test/unit-correctness/raypath/test_contour_contract_docking.cpp; this file owns the geometry
// side's own anchors.
//
// symmetry_semantics: none.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "analytic/dp_boundary.hpp"
#include "analytic/dp_contour.hpp"
#include "analytic/dp_focus.hpp"
#include "analytic/dp_weight_kink.hpp"
#include "analytic/path_evaluation.hpp"
#include "analytic/pose_density.hpp"

namespace lumice::analytic {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
// I_0(1), the modified Bessel value of the exponential test's closed form.
constexpr double kBesselI0One = 1.2660658777520084;

struct Tables {
  FaceNormalTable normals;
  FacePolygonTable polys;
};

// The C12 rhombic plate (LI HexPrism(a=1, h=2) at the engine scale: fd = [1.5, 1, 1, 1.5, 1, 1],
// height 1 — the construction test_fiber_quadrature.cpp verified equivalent to LI's).
Tables RhombicPlate() {
  Tables t;
  CrystalShape shape;
  shape.kind = CrystalShapeKind::kPrism;
  shape.height = 1.0;
  const double fd[6] = { 1.5, 1, 1, 1.5, 1, 1 };
  for (int i = 0; i < 6; i++) {
    shape.face_distance[i] = fd[i];
  }
  EXPECT_EQ(BuildFaceNormals(shape, &t.normals, &t.polys), Status::kOk);
  return t;
}

// The C11 plate: prism h = 0.3, all face distances 1 (raypath_feature_plate_target.json).
Tables C11Plate() {
  Tables t;
  CrystalShape shape;
  shape.kind = CrystalShapeKind::kPrism;
  shape.height = 0.3;
  for (int i = 0; i < 6; i++) {
    shape.face_distance[i] = 1.0;
  }
  EXPECT_EQ(BuildFaceNormals(shape, &t.normals, &t.polys), Status::kOk);
  return t;
}

// The canonical prism of the walk fixtures (test_dp_boundary.cpp's Prism()).
Tables CanonicalPrism() {
  Tables t;
  CrystalShape shape;
  shape.kind = CrystalShapeKind::kPrism;
  shape.height = 1.0;
  for (int i = 0; i < 6; i++) {
    shape.face_distance[i] = 1.0;
  }
  EXPECT_EQ(BuildFaceNormals(shape, &t.normals, &t.polys), Status::kOk);
  return t;
}

// The sun position vector (points AT the sun), altitude/azimuth in degrees — sky_direction.hpp's
// world convention.
void SunAt(double altitude_deg, double azimuth_deg, double s[3]) {
  const double alt = altitude_deg * kPi / 180.0;
  const double az = azimuth_deg * kPi / 180.0;
  s[0] = std::cos(alt) * std::cos(az);
  s[1] = std::cos(alt) * std::sin(az);
  s[2] = std::sin(alt);
}

PoseDensitySpec PlateSpec() {
  PoseDensitySpec spec;
  spec.family = PoseFamily::kPlate;
  spec.zenith_mean_deg = 0.0;
  spec.zenith_std_deg = 1.0;  // any positive width: the axis test is on the kind and the mean
  return spec;
}

double Dot3(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// ---- the declared rules -----------------------------------------------------------------------------------

TEST(DPContour, RuleDeclarations) {
  EXPECT_STREQ(CurveRuleName(CurveRule::kPeriodicMidpoint), "periodic-midpoint");
  EXPECT_STREQ(CurveRuleName(CurveRule::kChordTrapezoid), "chord-trapezoid");
  EXPECT_STREQ(CurveRuleName(CurveRule::kKinkSegmented), "kink-segmented");
  EXPECT_EQ(DeclaredCurveRuleOrder(CurveRule::kPeriodicMidpoint), 0);  // 0 = spectral
  EXPECT_EQ(DeclaredCurveRuleOrder(CurveRule::kChordTrapezoid), 2);
  EXPECT_EQ(DeclaredCurveRuleOrder(CurveRule::kKinkSegmented), 2);
}

TEST(DPContour, PeriodicWeightsAreUniformOnTheMidpointGrid) {
  const int n = 8;
  std::vector<double> params(n);
  for (int i = 0; i < n; i++) {
    params[i] = kTwoPi * (i + 0.5) / n;
  }
  const std::vector<double> w = SampleCurveWeights(CurveRule::kPeriodicMidpoint, params, true, {});
  ASSERT_EQ(w.size(), params.size());
  double sum = 0.0;
  for (double v : w) {
    EXPECT_DOUBLE_EQ(v, kTwoPi / n);  // the orbit producer's grid: every weight is 2 pi / N
    sum += v;
  }
  EXPECT_NEAR(sum, kTwoPi, 1e-14);
}

TEST(DPContour, PeriodicMidpointIsExactOnTrigonometricPolynomials) {
  // f = 1 + 0.3 cos(3t) - 0.2 sin(5t) + 0.15 cos(8t): integral 2 pi (only the constant survives).
  // The midpoint sum of e^{ikt} vanishes unless k = 0 (mod N), so the degree-8 term folds into
  // the constant at N = 4 and N = 8 (error exactly 2 pi * 0.15) and the rule is exact at 16.
  const auto f = [](double t) {
    return 1.0 + 0.3 * std::cos(3.0 * t) - 0.2 * std::sin(5.0 * t) + 0.15 * std::cos(8.0 * t);
  };
  const auto integrate = [&](int n) {
    std::vector<double> params(n);
    std::vector<double> values(n);
    for (int i = 0; i < n; i++) {
      params[i] = kTwoPi * (i + 0.5) / n;
      values[i] = f(params[i]);
    }
    const std::vector<double> w = SampleCurveWeights(CurveRule::kPeriodicMidpoint, params, true, {});
    double integral = 0.0;
    for (int i = 0; i < n; i++) {
      integral += w[i] * values[i];
    }
    return integral;
  };
  // At N = 4 the folded value is +1 (8 * pi/2 * (i + 1/2) = 4 pi i + 2 pi); at N = 8 it is -1
  // (8 * 2 pi / 8 * (i + 1/2) = 2 pi i + pi): the sign is part of the aliasing law.
  EXPECT_NEAR(integrate(4) - kTwoPi, 0.15 * kTwoPi, 1e-12);
  EXPECT_NEAR(integrate(8) - kTwoPi, -0.15 * kTwoPi, 1e-12);
  EXPECT_NEAR(integrate(16), kTwoPi, 1e-14);  // 8 != 0 (mod 16): exact to rounding
  EXPECT_NEAR(integrate(32), kTwoPi, 1e-14);
  // The declared error tier tracks the true error's size class: zero once the rule is exact.
  EXPECT_NEAR(RefinementErrorEstimate(integrate(16), integrate(32)), 0.0, 1e-14);
}

TEST(DPContour, PeriodicMidpointConvergesGeometricallyOnExpCos) {
  // f = exp(cos t): integral 2 pi I_0(1). The Fourier coefficients are I_k(1) ~ (1/2)^k / k!, so
  // the aliasing error falls faster than any power — the spectral claim, read as a table.
  const auto integrate = [&](int n) {
    std::vector<double> params(n);
    std::vector<double> values(n);
    for (int i = 0; i < n; i++) {
      params[i] = kTwoPi * (i + 0.5) / n;
      values[i] = std::exp(std::cos(params[i]));
    }
    const std::vector<double> w = SampleCurveWeights(CurveRule::kPeriodicMidpoint, params, true, {});
    double integral = 0.0;
    for (int i = 0; i < n; i++) {
      integral += w[i] * values[i];
    }
    return integral;
  };
  const double exact = kTwoPi * kBesselI0One;
  const double e4 = std::fabs(integrate(4) - exact);
  const double e8 = std::fabs(integrate(8) - exact);
  const double e16 = std::fabs(integrate(16) - exact);
  const double e32 = std::fabs(integrate(32) - exact);
  EXPECT_GT(e4, 1e-2);
  // The aliased tail is ~ 2 I_8(1)-shaped at N = 8 and hits the rounding floor by N = 16: each
  // doubling buys more than two extra digits — the geometric rate no fixed order can match.
  EXPECT_NEAR(e8, 1.25e-6, 0.3e-6);
  EXPECT_LT(e16, 1e-13);
  EXPECT_LT(e32, 1e-14);
  EXPECT_LT(e16, e8 / 100.0);
}

TEST(DPContour, OpenTrapezoidWeightsAndRefuseWrongShape) {
  // The open trapezoid over [0, 1]: h/2, h, ..., h/2.
  const int n = 9;  // 9 nodes, 8 cells
  std::vector<double> params(n);
  for (int i = 0; i < n; i++) {
    params[i] = static_cast<double>(i) / 8.0;
  }
  const std::vector<double> w = SampleCurveWeights(CurveRule::kChordTrapezoid, params, false, {});
  ASSERT_EQ(w.size(), params.size());
  EXPECT_DOUBLE_EQ(w.front(), 1.0 / 16.0);
  EXPECT_DOUBLE_EQ(w.back(), 1.0 / 16.0);
  for (int i = 1; i + 1 < n; i++) {
    EXPECT_DOUBLE_EQ(w[i], 1.0 / 8.0);
  }
  // A rule the function does not declare (periodic on an open curve) answers empty, not guessed.
  EXPECT_TRUE(SampleCurveWeights(CurveRule::kPeriodicMidpoint, params, false, {}).empty());
  EXPECT_TRUE(SampleCurveWeights(CurveRule::kChordTrapezoid, params, true, {}).empty());
}

// A size mismatch is a caller bug: the answer is NaN, never a plausible 0.0 (an empty curve
// still integrates to a legitimate zero).
TEST(DPContour, ChordLineIntegralSizeMismatchIsNanNotZero) {
  const std::vector<double> u = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0 };
  const std::vector<double> f = { 1.0, 1.0 };
  EXPECT_TRUE(std::isnan(ChordLineIntegral(u, { 1.0 }, false)));
  EXPECT_TRUE(std::isnan(ChordLineIntegral(u, { 1.0, 1.0, 1.0 }, true)));
  EXPECT_DOUBLE_EQ(ChordLineIntegral({}, {}, true), 0.0);
}

TEST(DPContour, ChordArcLengthOfAGreatCircleArc) {
  // Open arc of the equator from 0 to Delta: the chord sum approaches Delta from below with the
  // analytic deficit Delta^3 / (24 N^2) — O(h^2), the declared order, with its constant.
  const double delta = 1.2;
  const auto arc = [&](int n) {
    std::vector<double> u(3 * (n + 1));
    std::vector<double> f(n + 1, 1.0);
    for (int i = 0; i <= n; i++) {
      const double t = delta * i / n;
      u[3 * i] = std::cos(t);
      u[3 * i + 1] = std::sin(t);
      u[3 * i + 2] = 0.0;
    }
    return ChordLineIntegral(u, f, false);
  };
  // The chord sum of a uniform arc grid has its own closed form — pin the RULE against it at
  // rounding, then the convergence to the arc length at the declared O(h^2).
  for (int n = 16; n <= 64; n *= 2) {
    EXPECT_NEAR(arc(n), 2.0 * n * std::sin(delta / (2.0 * n)), 1e-14) << "n = " << n;
  }
  const double e16 = arc(16) - delta;  // negative: the chord sum underestimates the arc
  const double e32 = arc(32) - delta;
  const double e64 = arc(64) - delta;
  EXPECT_LT(e16, 0.0);
  EXPECT_NEAR(e16, -delta * delta * delta / (24.0 * 16.0 * 16.0), 1e-5);
  EXPECT_NEAR(e32, -delta * delta * delta / (24.0 * 32.0 * 32.0), 1e-6);
  EXPECT_NEAR(e32 * 4.0, e16, 1e-6);  // halving ratio 4: the declared O(h^2)
  EXPECT_NEAR(e64 * 4.0, e32, 1e-8);
}

TEST(DPContour, ChordPerimeterAndZonalLoadOfALatitudeCircle) {
  // Closed latitude circle at lat: perimeter 2 pi cos(lat), each chord 2 cos(lat) sin(pi / N),
  // analytic deficit 2 pi cos(lat) * pi^2 / (6 N^2). A zonal load (constant rho on the circle,
  // the AC1 half of the M2 anchor) scales it; a non-constant smooth load keeps the order.
  const double lat = 30.0 * kPi / 180.0;
  const double perimeter = 2.0 * kPi * std::cos(lat);
  const auto circle = [&](int n, const std::vector<double>& f) {
    std::vector<double> u(3 * n);
    for (int i = 0; i < n; i++) {
      const double lon = kTwoPi * i / n;
      u[3 * i] = std::cos(lat) * std::cos(lon);
      u[3 * i + 1] = std::cos(lat) * std::sin(lon);
      u[3 * i + 2] = std::sin(lat);
    }
    return ChordLineIntegral(u, f, true);
  };
  const auto constant = [&](int n) { return std::vector<double>(n, 0.7); };
  const auto wave = [&](int n) {
    std::vector<double> f(n);
    for (int i = 0; i < n; i++) {
      f[i] = 1.0 + 0.3 * std::cos(2.0 * kTwoPi * i / n);
    }
    return f;
  };
  // The closed chord polygon has its own closed form — pin the RULE against it at rounding.
  for (int n = 16; n <= 64; n *= 2) {
    EXPECT_NEAR(circle(n, constant(n)), 0.7 * 2.0 * n * std::cos(lat) * std::sin(kPi / n), 1e-14) << "n = " << n;
  }
  // The convergence to the true perimeter: the deficit is exact in closed form
  // (2 pi R - 2 N R sin(pi / N), leading term pi^3 R / (3 N^2)), and it quarters per halving.
  const double e16 = circle(16, constant(16)) - 0.7 * perimeter;
  const double e32 = circle(32, constant(32)) - 0.7 * perimeter;
  const double e64 = circle(64, constant(64)) - 0.7 * perimeter;
  const double radius = std::cos(lat);
  EXPECT_NEAR(e16, 0.7 * (2.0 * 16.0 * radius * std::sin(kPi / 16.0) - perimeter), 1e-14);
  EXPECT_NEAR(e32, 0.7 * (2.0 * 32.0 * radius * std::sin(kPi / 32.0) - perimeter), 1e-14);
  EXPECT_NEAR(e64, 0.7 * (2.0 * 64.0 * radius * std::sin(kPi / 64.0) - perimeter), 1e-14);
  EXPECT_NEAR(e16 / e32, 4.0, 0.05);  // the declared O(h^2)
  EXPECT_NEAR(e32 / e64, 4.0, 0.02);
  // The smooth non-constant load: exact value 2 pi cos(lat) (the wave averages out), same order.
  const double w16 = circle(16, wave(16)) - perimeter;
  const double w32 = circle(32, wave(32)) - perimeter;
  EXPECT_NEAR(w32 * 4.0, w16, 1e-4);
  EXPECT_LT(std::fabs(w32), 2e-2);
}

TEST(DPContour, KinkCutRestoresTheSegmentRule) {
  // Piecewise-linear integrand f = |x - a| on [0, 1]: the trapezoid is EXACT when the kink is a
  // node (linear on each cell) and off by exactly s (h - s) when the kink sits s into a cell —
  // both constants analytic, so the declared order and its degradation are pinned, not eyeballed.
  const double a = 0.37;
  const auto f = [&](double x) { return std::fabs(x - a); };
  const auto integrate = [&](int n) {  // n cells, nodes x_i = i / n
    std::vector<double> params(n + 1);
    std::vector<double> values(n + 1);
    for (int i = 0; i <= n; i++) {
      params[i] = static_cast<double>(i) / n;
      values[i] = f(params[i]);
    }
    const std::vector<double> w = SampleCurveWeights(CurveRule::kKinkSegmented, params, false, {});
    double integral = 0.0;
    for (int i = 0; i <= n; i++) {
      integral += w[i] * values[i];
    }
    return integral;
  };
  const double exact = a * a / 2.0 + (1.0 - a) * (1.0 - a) / 2.0;
  // 0.37 = 37/100 is a node at n = 100: exact to rounding.
  EXPECT_NEAR(integrate(100), exact, 1e-15);
  // n = 30: the kink sits s = 0.37 - 11/30 into cell 11; the error is exactly s (h - s).
  const double h = 1.0 / 30.0;
  const double s = a - 11.0 / 30.0;
  EXPECT_NEAR(integrate(30) - exact, s * (h - s), 1e-15);
}

TEST(DPContour, ClosedKinkRuleMatchesTheSeamConstants) {
  // f = |sin t| on the circle (integral 4, kinks at 0 and pi). The cut rule (marks at the kinks,
  // node grid) is the sum of the segment trapezoids, and its error is PURE Euler-Maclaurin seams,
  // -h^2/12 * sum [f'(segment end) - f'(segment start)] = -h^2 / 3 — predictable in advance, which
  // is the rule's claim (the kink-blind periodic rule's constant is a mix of kink-cell and seam
  // terms with no closed law: here it lands at +h^2 / 6, which happens to be smaller in
  // magnitude than the cut rule's -h^2/3 — the cut rule does not promise a smaller constant, it
  // promises the analytic one — and both laws are asserted: the cut rule at -h^2/3 exactly, the
  // blind one at +h^2/6).
  const auto node_grid = [&](int n, bool mark_kinks) {
    std::vector<double> params(n);
    std::vector<char> kink(n, 0);
    for (int i = 0; i < n; i++) {
      params[i] = kTwoPi * i / n;
      if (mark_kinks && (i == 0 || i == n / 2)) {
        kink[i] = 1;
      }
    }
    return std::make_pair(params, kink);
  };
  const auto apply = [&](const std::vector<double>& params, const std::vector<char>& kink, CurveRule rule,
                         bool closed) {
    std::vector<double> values(params.size());
    for (size_t i = 0; i < params.size(); i++) {
      values[i] = std::fabs(std::sin(params[i]));
    }
    const std::vector<double> w = SampleCurveWeights(rule, params, closed, kink);
    double integral = 0.0;
    for (size_t i = 0; i < params.size(); i++) {
      integral += w[i] * values[i];
    }
    return integral;
  };
  for (int n : { 64, 128 }) {
    const double h = kTwoPi / n;
    auto [params, marks] = node_grid(n, true);
    const double cut = apply(params, marks, CurveRule::kKinkSegmented, true);
    EXPECT_NEAR(cut - 4.0, -h * h / 3.0, 0.01 * h * h) << "n = " << n;
    // Kink-blind periodic rule, midpoint grid (kinks mid-cell): its constant is measured, not
    // predicted — and it is O(h^2) too (the spectral claim is what the kink costs).
    std::vector<double> mid(n);
    for (int i = 0; i < n; i++) {
      mid[i] = kTwoPi * (i + 0.5) / n;
    }
    // The kink-blind constant decomposes exactly: +h^2/4 per mid-cell kink (s (h - s) at
    // s = h/2, twice) minus the smooth arcs' Euler-Maclaurin h^2/3 = +h^2/6.
    const double blind = apply(mid, {}, CurveRule::kPeriodicMidpoint, true);
    EXPECT_NEAR(blind - 4.0, h * h / 6.0, 0.01 * h * h) << "n = " << n;
  }
  // Both O(h^2): the error quarters under a grid halving (they differ in constant, not order).
  {
    const int n = 64;
    auto [params, marks] = node_grid(n, true);
    const double cut64 = apply(params, marks, CurveRule::kKinkSegmented, true);
    auto [params2, marks2] = node_grid(2 * n, true);
    const double cut128 = apply(params2, marks2, CurveRule::kKinkSegmented, true);
    EXPECT_NEAR((cut64 - 4.0) / (cut128 - 4.0), 4.0, 0.05);
  }
}

// The two loud refusals of the weight rules, pinned (the r2 review's declared-but-untested
// branches): a closed kink rule with no cut does not apply, and the seam step of the closed walk
// closes the period through CircleStep's next==0 branch.
TEST(DPContour, ClosedKinkRuleWithoutMarksIsTheEmptyAnswer) {
  const std::vector<double> params = { 0.0, kTwoPi / 4.0, kTwoPi / 2.0, 3.0 * kTwoPi / 4.0 };
  // A closed kink rule needs at least one mark (the cut makes the circle a list of open arcs);
  // without one the zero-weight answer is louder than a silent wrap over an uncut kink — the
  // integral reads 0, never a guessed value.
  const std::vector<double> w = SampleCurveWeights(CurveRule::kKinkSegmented, params, true, {});
  ASSERT_EQ(w.size(), params.size());
  for (double weight : w) {
    EXPECT_DOUBLE_EQ(weight, 0.0);
  }
  // The same rule OPEN ignores the (absent) marks: the open trapezoid, weights summing to the
  // covered span.
  const std::vector<double> open = SampleCurveWeights(CurveRule::kKinkSegmented, params, false, {});
  ASSERT_EQ(open.size(), params.size());
  double sum = 0.0;
  for (double weight : open) {
    sum += weight;
  }
  EXPECT_NEAR(sum, 3.0 * kTwoPi / 4.0, 1e-12);
}

TEST(DPContour, SeamStepClosesThePeriodThroughCircleStep) {
  // One mark at the seam node: the cut walk leaves the mark, rounds the whole circle and comes
  // back through CircleStep(params, count-1, 0) — the next==0 branch, legal only there (the two
  // call sites reach it with i == count-1 by construction of (i + 1) % count; i is ignored in
  // that branch by design, the seam step reading only the ends). The weights must be the exact
  // node-cell halves summing to one period.
  const int n = 8;
  std::vector<double> params(n);
  std::vector<char> kink(n, 0);
  for (int i = 0; i < n; i++) {
    params[i] = kTwoPi * i / n;
  }
  kink[0] = 1;  // the seam node: cuts = [0], from == to
  const std::vector<double> w = SampleCurveWeights(CurveRule::kKinkSegmented, params, true, kink);
  ASSERT_EQ(w.size(), params.size());
  double sum = 0.0;
  for (int i = 0; i < n; i++) {
    EXPECT_NEAR(w[i], kTwoPi / n, 1e-12) << "node " << i;
    sum += w[i];
  }
  EXPECT_NEAR(sum, kTwoPi, 1e-12);
}

// ---- the orbit producer -----------------------------------------------------------------------------------

TEST(DPContour, OrbitStreamMatchesTheHandwrittenGrid) {
  // Bit-parity with the handwritten spin grid it productizes (661's EvaluateMember arithmetic,
  // a39 to the letter): same crystal, member, index, sun, grid — every field, bitwise.
  const Tables t = RhombicPlate();
  const std::vector<int> faces = { 1, 3, 4, 2 };
  int slots[kMaxFaceCount];
  ASSERT_EQ(ResolveFaceSequence(t.normals, faces.data(), static_cast<int>(faces.size()), slots), Status::kOk);
  double sun[3];
  SunAt(9.0, 180.0, sun);
  const int grid = 256;
  const double n = 1.307;
  const int count = static_cast<int>(faces.size());
  const OrbitFiberStream stream = MakeOrbitFiberStream(t.normals, t.polys, slots, count, PlateSpec(), sun, n, grid);
  ASSERT_EQ(stream.samples.size(), static_cast<size_t>(grid));
  EXPECT_EQ(stream.binding, FiberMeasureBinding::kFiberParameter);
  EXPECT_EQ(stream.evidence, FiberEvidenceForm::kSampledExhaustive);
  EXPECT_FALSE(stream.spin_degenerate);
  EXPECT_EQ(stream.grid, grid);

  // The handwritten path, expression for expression (test_fiber_quadrature.cpp's EvaluateMember).
  Corridor corridor(t.normals, t.polys, slots, count);
  const double incident[3] = { -sun[0], -sun[1], -sun[2] };
  std::vector<double> segments((static_cast<size_t>(count) + 1) * 3), transmittances(count);
  double hand_energy = 0.0;
  double producer_energy = 0.0;
  int hand_kept = 0;
  int producer_kept = 0;
  int classes[4] = { 0, 0, 0, 0 };  // valid | A=0,T>0 | T=0,A>0 | rest — the A/T split census
  for (int i = 0; i < grid; i++) {
    const double theta = kTwoPi * (i + 0.5) / grid;
    const double c = std::cos(theta), s = std::sin(theta);
    const double pose[9] = { c, -s, 0.0, s, c, 0.0, 0.0, 0.0, 1.0 };
    PathOutputs out;
    out.segment_directions = segments.data();
    out.interface_transmittances = transmittances.data();
    const bool valid = EvaluatePath(t.normals, slots, count, n, incident, pose, &out);
    double u[3];
    for (int r = 0; r < 3; r++) {
      u[r] = pose[r] * sun[0] + pose[3 + r] * sun[1] + pose[6 + r] * sun[2];
    }
    double s_body[3] = { -u[0], -u[1], -u[2] };
    const EntryMeasure entry = corridor.Evaluate(s_body, n);
    const double area = entry.status == EntryMeasureStatus::kOk ? entry.value : 0.0;
    const double transmission = valid ? out.fresnel_transmission : 0.0;
    const bool kept = valid && area > 0.0 && transmission > 0.0;

    const OrbitFiberPoint& point = stream.samples[i];
    EXPECT_DOUBLE_EQ(point.parameter, theta);
    EXPECT_DOUBLE_EQ(point.weight, kTwoPi / grid);
    for (int r = 0; r < 3; r++) {
      EXPECT_DOUBLE_EQ(point.u[r], u[r]) << "theta " << theta << " component " << r;
    }
    EXPECT_DOUBLE_EQ(point.area, area);
    EXPECT_DOUBLE_EQ(point.transmission, transmission);
    EXPECT_EQ(point.valid, kept);
    EXPECT_FALSE(point.jet_degenerate);
    for (int r = 0; r < 3; r++) {
      EXPECT_DOUBLE_EQ(point.outgoing[r], out.outgoing_direction[r]);
    }
    if (kept) {
      hand_energy += area * transmission * (kTwoPi / grid);
      hand_kept++;
      producer_energy += point.area * point.transmission * point.weight;
      producer_kept++;
    } else if (area == 0.0 && transmission > 0.0) {
      classes[1]++;
    } else if (transmission == 0.0 && area > 0.0) {
      classes[2]++;
    } else {
      classes[3]++;
    }
    classes[0] += kept ? 1 : 0;
  }
  EXPECT_DOUBLE_EQ(producer_energy, hand_energy);
  EXPECT_EQ(producer_kept, hand_kept);
  // u fidelity, independent of the arithmetic order: |u| = 1 and the orbit's own invariant
  // u_z = s_z (Rz leaves the pole alone), exact in this construction.
  for (const OrbitFiberPoint& point : stream.samples) {
    EXPECT_NEAR(std::fabs(Dot3(point.u, point.u) - 1.0), 0.0, 1e-15);
    EXPECT_DOUBLE_EQ(point.u[2], sun[2]);
  }
  printf("[orbit-census] valid=%d a0t+=%d t0a+=%d rest=%d\n", classes[0], classes[1], classes[2], classes[3]);
}

TEST(DPContour, OrbitStreamNaturalZeroAreaSamplesOnTheBlueMember) {
  // The A/T split's natural witness: the C12 blue member 1-3-5-2 at n = 1.317 carries samples
  // with A == 0 and T > 0 — the corridor closed (no line of that internal direction crosses
  // every polygon) while the direction-level chain stays valid. The mirrored darkness
  // (T == 0 with A > 0) does not occur on these orbits — the corridor's exit-critical gate
  // mirrors the path's exit-Snell gate, so the two die together; the certificate's
  // discrimination of the two darknesses is pinned by the synthesized control in the docking
  // test (test_contour_contract_docking.cpp), which is where a negative control belongs.
  const Tables t = RhombicPlate();
  const std::vector<int> faces = { 1, 3, 5, 2 };
  int slots[kMaxFaceCount];
  ASSERT_EQ(ResolveFaceSequence(t.normals, faces.data(), static_cast<int>(faces.size()), slots), Status::kOk);
  double sun[3];
  SunAt(9.0, 180.0, sun);
  const OrbitFiberStream stream =
      MakeOrbitFiberStream(t.normals, t.polys, slots, static_cast<int>(faces.size()), PlateSpec(), sun, 1.317, 720);
  ASSERT_EQ(stream.samples.size(), 720u);
  int zero_area_live_transmission = 0;
  int zero_transmission_positive_area = 0;
  int valid = 0;
  double area_spread_lo = 1e9, area_spread_hi = 0.0, t_spread_lo = 1e9, t_spread_hi = 0.0;
  for (const OrbitFiberPoint& p : stream.samples) {
    if (p.valid) {
      valid++;
      area_spread_lo = std::min(area_spread_lo, p.area);
      area_spread_hi = std::max(area_spread_hi, p.area);
      t_spread_lo = std::min(t_spread_lo, p.transmission);
      t_spread_hi = std::max(t_spread_hi, p.transmission);
    } else if (p.area == 0.0 && p.transmission > 0.0) {
      zero_area_live_transmission++;
    } else if (p.transmission == 0.0 && p.area > 0.0) {
      zero_transmission_positive_area++;
    }
  }
  EXPECT_GT(valid, 50);                           // the arc is lit
  EXPECT_GT(zero_area_live_transmission, 100);    // the corridor-closed darkness occurs naturally
  EXPECT_EQ(zero_transmission_positive_area, 0);  // its mirror does not (probe census, 720 grid)
  // Both fields are live on the valid arc (Fresnel modulates T, the corridor modulates A) —
  // two independent machineries, not one product reported twice.
  EXPECT_LT(area_spread_lo, area_spread_hi / 2.0);
  EXPECT_LT(t_spread_lo, t_spread_hi / 2.0);
}

TEST(DPContour, OrbitStreamPoleSunIsAllDegenerateWithANote) {
  const Tables t = RhombicPlate();
  const std::vector<int> faces = { 1, 3, 4, 2 };
  int slots[kMaxFaceCount];
  ASSERT_EQ(ResolveFaceSequence(t.normals, faces.data(), static_cast<int>(faces.size()), slots), Status::kOk);
  const double sun[3] = { 0.0, 0.0, 1.0 };  // the zenith sun: the plate orbit collapses
  const OrbitFiberStream stream =
      MakeOrbitFiberStream(t.normals, t.polys, slots, static_cast<int>(faces.size()), PlateSpec(), sun, 1.307, 32);
  ASSERT_EQ(stream.samples.size(), 32u);
  EXPECT_TRUE(stream.spin_degenerate);
  EXPECT_FALSE(stream.note.empty());  // never silent
  for (const OrbitFiberPoint& point : stream.samples) {
    EXPECT_TRUE(point.jet_degenerate);
    EXPECT_DOUBLE_EQ(point.u[0], 0.0);
    EXPECT_DOUBLE_EQ(point.u[1], 0.0);
    EXPECT_DOUBLE_EQ(point.u[2], 1.0);
  }
}

TEST(DPContour, OrbitStreamWithoutAFamilyAxisAnswersEmptyWithANote) {
  const Tables t = RhombicPlate();
  const std::vector<int> faces = { 1, 3, 4, 2 };
  int slots[kMaxFaceCount];
  ASSERT_EQ(ResolveFaceSequence(t.normals, faces.data(), static_cast<int>(faces.size()), slots), Status::kOk);
  double sun[3];
  SunAt(9.0, 180.0, sun);
  PoseDensitySpec random_spec;  // kRandom: the support is all of SO(3), no circle
  const OrbitFiberStream stream =
      MakeOrbitFiberStream(t.normals, t.polys, slots, static_cast<int>(faces.size()), random_spec, sun, 1.307, 32);
  EXPECT_TRUE(stream.samples.empty());
  EXPECT_NE(stream.note.find("no family axis"), std::string::npos);
}

// ---- the restricted family curve --------------------------------------------------------------------------

TEST(DPContour, RestrictedCircleOfTheC11PlateFamily) {
  const Tables t = C11Plate();
  const std::vector<int> faces = { 1, 4, 5, 2 };
  int slots[kMaxFaceCount];
  ASSERT_EQ(ResolveFaceSequence(t.normals, faces.data(), static_cast<int>(faces.size()), slots), Status::kOk);
  double sun[3];
  SunAt(20.0, 0.0, sun);  // C11: sun altitude 20
  const int grid = 360;
  const double wavelengths[2] = { 550.0, 650.0 };
  const double indices[2] = { 1.3110129, 1.307 };
  const RestrictedFamilyCurve curve =
      MakeRestrictedFamilyCurve(t.normals, t.polys, slots, 4, PlateSpec(), sun, 1.31, wavelengths, indices, 2, grid);
  ASSERT_EQ(curve.u.size(), static_cast<size_t>(3 * grid));
  EXPECT_TRUE(curve.closed);
  EXPECT_EQ(curve.existence, CurveExistence::kComputed);
  EXPECT_EQ(curve.routed_nonfinite, 0) << curve.note;
  // The circle's own geometry, closed-form: |u| = 1, the latitude pin u_z = sin(altitude)
  // (independent of theta — the family's u-image), unit tangents orthogonal to u.
  const double u_z = std::sin(20.0 * kPi / 180.0);
  for (int i = 0; i < grid; i++) {
    const double* u = &curve.u[3 * i];
    const double* tangent = &curve.tangent[3 * i];
    EXPECT_NEAR(Dot3(u, u), 1.0, 1e-15);
    EXPECT_NEAR(u[2], u_z, 1e-15);
    EXPECT_NEAR(Dot3(tangent, tangent), 1.0, 1e-15);
    EXPECT_NEAR(Dot3(u, tangent), 0.0, 1e-15);
    EXPECT_DOUBLE_EQ(curve.support_param[i], kTwoPi * (i + 0.5) / grid);
  }
  // The family-pinned property, both legs the corpus names: the label itself, and the level-set
  // property read off the curve (D_P constant along the whole circle).
  EXPECT_TRUE(FamilyPinned(t.normals, slots, 4, PlateSpec()));
  const auto finite_range = [&](const std::vector<double>& values) {
    double lo = values[0], hi = values[0];
    for (double v : values) {
      if (std::isfinite(v)) {
        lo = std::min(lo, v);
        hi = std::max(hi, v);
      }
    }
    return std::make_pair(lo, hi);
  };
  const auto [d_lo, d_hi] = finite_range(curve.d_p);
  printf("[c11-dp] base spread = %.3e (lo %.12f hi %.12f)\n", d_hi - d_lo, d_lo, d_hi);
  EXPECT_LE(d_hi - d_lo, 1e-9);
  // The per-wavelength columns: one row per point, the same pinned spread at each index, and
  // the base column bit-equal to an independently constructed field at the same index.
  ASSERT_EQ(curve.critical_d_p.size(), static_cast<size_t>(2 * grid));
  for (int k = 0; k < 2; k++) {
    std::vector<double> column(grid);
    for (int i = 0; i < grid; i++) {
      column[i] = curve.critical_d_p[static_cast<size_t>(i) * 2 + k];
    }
    const auto [lo, hi] = finite_range(column);
    printf("[c11-dp] lambda %d spread = %.3e\n", k, hi - lo);
    EXPECT_LE(hi - lo, 1e-9);
  }
  DeviationField base_field(t.normals, t.polys, slots, 4, 1.31);
  const bool slab = base_field.fold().degenerate;
  for (int i = 0; i < grid; i++) {
    const FieldSample sample = base_field.SampleOptical(&curve.u[3 * i]);
    double d = 0.0;
    if (RoutedDeviation(sample, slab, &d) != RoutedDeviationStatus::kOk) {
      ADD_FAILURE() << "point " << i << ": routed D_P non-finite under recompute";
      continue;
    }
    EXPECT_DOUBLE_EQ(curve.d_p[i], d);
  }
}

// ---- the chain mappings -----------------------------------------------------------------------------------

TEST(DPContour, ChainFromKinkArcsSingleArcVerbatim) {
  // 3-1-6 on the canonical prism: the closed form's kink curve is ONE arc — the kink circle
  // CLIPPED by U_P (its ends sit on the named gates, arc.closed is false — the data shape, not
  // the geometric picture of the full circle; the chain mirrors the source, it does not invent
  // closure). Every point, value and the TIR-onset mark ride along verbatim.
  const Tables t = CanonicalPrism();
  const int faces[3] = { 3, 1, 6 };
  const DeviationField field = [&] {
    int slots[kMaxFaceCount];
    EXPECT_EQ(ResolveFaceSequence(t.normals, faces, 3, slots), Status::kOk);
    return DeviationField(t.normals, t.polys, slots, 3, 1.31);
  }();
  const std::vector<KinkCurve> curves = WeightKinks(field, KinkOptions{});
  ASSERT_FALSE(curves.empty());
  ASSERT_EQ(curves[0].arcs.size(), 1u);
  const KinkArc& arc = curves[0].arcs[0];
  ASSERT_FALSE(arc.closed);  // the clipped arc: the source's own flag, mirrored
  const ChainCurve chain = ChainFromKinkArcs(curves[0]);
  EXPECT_FALSE(chain.is_gate_boundary);
  EXPECT_EQ(chain.existence, CurveExistence::kComputed);
  EXPECT_FALSE(chain.closed);
  const size_t count = arc.points.size() / 3;
  ASSERT_EQ(chain.u.size(), arc.points.size());
  ASSERT_EQ(chain.values.size(), count);
  int marks = 0;
  for (size_t i = 0; i < count; i++) {
    EXPECT_DOUBLE_EQ(chain.u[3 * i], arc.points[3 * i]);
    EXPECT_DOUBLE_EQ(chain.u[3 * i + 1], arc.points[3 * i + 1]);
    EXPECT_DOUBLE_EQ(chain.u[3 * i + 2], arc.points[3 * i + 2]);
    EXPECT_DOUBLE_EQ(chain.values[i], arc.values[i]);
    EXPECT_EQ(chain.event[i], -1);  // v1: no event words on curve points
    marks += chain.kink[i] != 0 ? 1 : 0;
  }
  EXPECT_EQ(marks, 1);  // the single arc's TIR onset
  EXPECT_EQ(chain.kink[0], 1);
  EXPECT_DOUBLE_EQ(chain.param[0], 0.0);
  for (double v : chain.param) {
    EXPECT_GE(v, 0.0);
  }
}

TEST(DPContour, ChainFromBoundaryLoopRoundtrip) {
  // 4-8-7-5 on the canonical prism (the parity corpus's loop): the chain carries every walk
  // point exactly once (shared corners deduplicated, the glued seam collapsed), a kink at every
  // corner, and the values ride along unchanged.
  const Tables t = CanonicalPrism();
  const int faces[4] = { 4, 8, 7, 5 };
  int slots[kMaxFaceCount];
  ASSERT_EQ(ResolveFaceSequence(t.normals, faces, 4, slots), Status::kOk);
  const DeviationField field(t.normals, t.polys, slots, 4, 1.31);
  BoundaryWalkRecord record;
  const WalkResult walk = WalkBoundary(field, BoundaryWalkOptions{}, &record);
  ASSERT_EQ(walk.status, WalkStatus::kOk) << walk.message;

  const ChainCurve chain = ChainFromBoundaryPieces(record, walk.status, walk.message);
  EXPECT_TRUE(chain.is_gate_boundary);
  EXPECT_EQ(chain.existence, CurveExistence::kComputed);
  EXPECT_TRUE(chain.closed);
  // Every piece's points appear in the chain in order: walk the chain once, consuming pieces.
  // The glue precondition: the last piece's end is bit-equal to the first point (WalkBoundary's
  // _replace_last_point), so the chain dropped it — every source point is in the chain exactly once.
  ASSERT_EQ(record.pieces.size(), record.corners.size());
  const BoundaryPiece& first_piece = record.pieces.front();
  const BoundaryPiece& last_piece = record.pieces.back();
  ASSERT_DOUBLE_EQ(last_piece.points[last_piece.points.size() - 3], first_piece.points[0]);
  ASSERT_DOUBLE_EQ(last_piece.points[last_piece.points.size() - 2], first_piece.points[1]);
  ASSERT_DOUBLE_EQ(last_piece.points[last_piece.points.size() - 1], first_piece.points[2]);
  size_t chain_index = 0;
  size_t total_points = 0;
  for (size_t p = 0; p < record.pieces.size(); p++) {
    const size_t point_count = record.pieces[p].points.size() / 3;
    const size_t last_consumed = (p + 1 == record.pieces.size()) ? point_count - 1 : point_count;
    for (size_t i = 0; i < last_consumed; i++) {
      if (i == 0 && p > 0 && chain_index > 0) {
        // The shared corner: the previous piece's run ended ON it (chain_index - 1), and this
        // piece's first point is the same bits — the dedup the assembly performed.
        EXPECT_DOUBLE_EQ(chain.u[3 * (chain_index - 1)], record.pieces[p].points[0]);
        EXPECT_DOUBLE_EQ(chain.u[3 * (chain_index - 1) + 1], record.pieces[p].points[1]);
        EXPECT_DOUBLE_EQ(chain.u[3 * (chain_index - 1) + 2], record.pieces[p].points[2]);
        EXPECT_DOUBLE_EQ(chain.values[chain_index - 1], record.pieces[p].values[0]);
        continue;
      }
      if (chain_index >= chain.values.size()) {
        ADD_FAILURE() << "piece " << p << " point " << i << ": chain exhausted at " << chain_index;
        continue;
      }
      EXPECT_DOUBLE_EQ(chain.u[3 * chain_index], record.pieces[p].points[3 * i]);
      EXPECT_DOUBLE_EQ(chain.u[3 * chain_index + 1], record.pieces[p].points[3 * i + 1]);
      EXPECT_DOUBLE_EQ(chain.u[3 * chain_index + 2], record.pieces[p].points[3 * i + 2]);
      EXPECT_DOUBLE_EQ(chain.values[chain_index], record.pieces[p].values[i]);
      chain_index++;
      total_points++;
    }
  }
  EXPECT_EQ(chain_index, chain.values.size());
  int marks = 0;
  for (char k : chain.kink) {
    marks += k != 0 ? 1 : 0;
  }
  EXPECT_EQ(marks, static_cast<int>(record.corners.size()));
  EXPECT_EQ(chain.kink[0], 1);  // the seam corner lives at index 0
}

TEST(DPContour, ChainStatusMappingAndTrustTheStatus) {
  // kOk -> kComputed; every refusal -> kWalkTruncated with the message; a stale record under a
  // refusal status is ignored (WalkBoundary's invariant says it cannot be filled — the mapping
  // does not read it).
  BoundaryWalkRecord empty;
  const ChainCurve truncated =
      ChainFromBoundaryPieces(empty, WalkStatus::kStepsExhausted, "MAX_WALK_STEPS without a corner");
  EXPECT_EQ(truncated.existence, CurveExistence::kWalkTruncated);
  EXPECT_NE(truncated.note.find("steps_exhausted"), std::string::npos);  // WalkStatusName's spelling
  EXPECT_NE(truncated.note.find("MAX_WALK_STEPS"), std::string::npos);
  EXPECT_TRUE(truncated.u.empty());
  EXPECT_FALSE(truncated.closed);  // closure IS the kOk certificate; a refusal never closes

  // The same refusal with a non-empty record (unconstructible from WalkBoundary): still empty.
  BoundaryWalkRecord stale;
  BoundaryPiece piece;
  piece.points = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0 };
  piece.values = { 0.1, 0.2 };
  stale.pieces.push_back(piece);
  const ChainCurve ignored = ChainFromBoundaryPieces(stale, WalkStatus::kNotClosed, "did not close");
  EXPECT_EQ(ignored.existence, CurveExistence::kWalkTruncated);
  EXPECT_TRUE(ignored.u.empty());

  KinkCurve no_arcs;
  no_arcs.status = WalkStatus::kOk;
  const ChainCurve empty_chain = ChainFromKinkArcs(no_arcs);
  EXPECT_TRUE(empty_chain.u.empty());
  EXPECT_FALSE(empty_chain.closed);  // no arcs: nothing closes
}

}  // namespace
}  // namespace lumice::analytic
