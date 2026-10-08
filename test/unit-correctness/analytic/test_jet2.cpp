// Second-order hyper-dual numbers (src/analytic/jet2.hpp) against closed-form derivatives, against
// central differences of the double evaluation, and against a plane-restricted Jet2<2> evaluation
// — oracles that share nothing with the Jet2 rules. The fourth dual direction (the refractive-index
// slot the field layer binds to n) is checked the same way, against d/dn closed forms.
//
// symmetry_semantics: none — no face sequence is involved.

#include <gtest/gtest.h>

#include <cmath>

#include "analytic/jet2.hpp"

namespace lumice::analytic {
namespace {

using J2 = Jet2<2>;
using J3 = Jet2<3>;
using J4 = Jet2<4>;

// A composite exercising + - * / unary-, Sqrt, Sin, Cos and mixed Jet2/double arithmetic — the
// same shape as test_jet.cpp's Composite, at second order.
template <class S>
S Composite(const S& x, const S& y, const S& z) {
  const S r2 = x * x + y * y + z * z;
  const S r = Sqrt(r2);
  return (Sin(r) / r) * (2.0 - Cos(y)) - (x * z) / (1.0 + r2) + 3.0 * (-y);
}

// The deviation shape of dp_field: atan2(|c|, d) with |c| = sqrt(sum of squares) — exercises
// Atan2 through the same Sqrt the norm goes through.
template <class S>
S DeviationShape(const S& x, const S& y, const S& z) {
  return Atan2(Sqrt(y * y + z * z), x);
}

// The double evaluation of each composite, as a plain (non-overloaded) function the difference
// helpers can take by name; the elementary functions spell std:: directly so this test file does
// not need jet.hpp's double overloads.
double CompositeD(double x, double y, double z) {
  const double r = std::sqrt(x * x + y * y + z * z);
  return (std::sin(r) / r) * (2.0 - std::cos(y)) - (x * z) / (1.0 + x * x + y * y + z * z) + 3.0 * (-y);
}
double DeviationD(double x, double y, double z) {
  return std::atan2(std::sqrt(y * y + z * z), x);
}

// d^2 f / dx_k dx_j by the four-point mixed difference of the double evaluation.
template <class F>
double MixedSecond(F f, const double p[3], int k, int j, double h) {
  double pp[3] = { p[0], p[1], p[2] };
  double pm[3] = { p[0], p[1], p[2] };
  double mp[3] = { p[0], p[1], p[2] };
  double mm[3] = { p[0], p[1], p[2] };
  pp[k] += h;
  pp[j] += h;
  pm[k] += h;
  pm[j] -= h;
  mp[k] -= h;
  mp[j] += h;
  mm[k] -= h;
  mm[j] -= h;
  return (f(pp[0], pp[1], pp[2]) - f(pm[0], pm[1], pm[2]) - f(mp[0], mp[1], mp[2]) + f(mm[0], mm[1], mm[2])) /
         (4.0 * h * h);
}

template <int N>
void ExpectSymmetric(const Jet2<N>& f) {
  for (int k = 0; k < N; k++) {
    for (int j = 0; j < N; j++) {
      // Symmetric in exact arithmetic; in floating point the two triangle entries are sums of
      // the same four products in different association, so they agree to rounding, not bitwise.
      EXPECT_NEAR(f.h[k][j], f.h[j][k], 1e-14 * (1.0 + std::fabs(f.h[k][j]))) << "k=" << k << " j=" << j;
    }
  }
}

TEST(Jet2, ArithmeticMatchesClosedFormDerivatives) {
  const double x0 = 0.9;
  const double y0 = 0.4;

  const J2 x = J2::Variable(x0, 0);
  const J2 y = J2::Variable(y0, 1);

  // x*y: d2/dxdy = 1, all pure second derivatives 0.
  const J2 p = x * y;
  EXPECT_DOUBLE_EQ(p.v[0], y0);
  EXPECT_DOUBLE_EQ(p.v[1], x0);
  EXPECT_DOUBLE_EQ(p.h[0][1], 1.0);
  EXPECT_DOUBLE_EQ(p.h[1][0], 1.0);
  EXPECT_DOUBLE_EQ(p.h[0][0], 0.0);
  EXPECT_DOUBLE_EQ(p.h[1][1], 0.0);

  // x/y: linear in x (h_xx = 0), d2/dxdy = -1/y^2, d2/dy2 = 2x/y^3.
  const J2 q = x / y;
  EXPECT_DOUBLE_EQ(q.v[0], 1.0 / y0);
  EXPECT_DOUBLE_EQ(q.v[1], -x0 / (y0 * y0));
  EXPECT_DOUBLE_EQ(q.h[0][0], 0.0);
  EXPECT_DOUBLE_EQ(q.h[0][1], -1.0 / (y0 * y0));
  EXPECT_DOUBLE_EQ(q.h[1][1], 2.0 * x0 / (y0 * y0 * y0));

  // sqrt(x): d2/dx2 = -1/(4 x^(3/2)).
  const J2 s = Sqrt(x);
  EXPECT_DOUBLE_EQ(s.v[0], 0.5 / std::sqrt(x0));
  EXPECT_DOUBLE_EQ(s.h[0][0], -1.0 / (4.0 * x0 * std::sqrt(x0)));

  // sin, cos: second derivative is -f * (x')^2 along a seeded direction.
  EXPECT_DOUBLE_EQ(Sin(x).h[0][0], -std::sin(x0));
  EXPECT_DOUBLE_EQ(Cos(x).h[0][0], -std::cos(x0));

  // atan2(y, x): fy = x/r^2, fyy = -2xy/r^4, fxy = (y^2-x^2)/r^4, fxx = 2xy/r^4.
  const J2 a = Atan2(y, x);
  const double r2 = x0 * x0 + y0 * y0;
  EXPECT_DOUBLE_EQ(a.a, std::atan2(y0, x0));
  EXPECT_DOUBLE_EQ(a.v[0], -y0 / r2);
  EXPECT_DOUBLE_EQ(a.v[1], x0 / r2);
  EXPECT_DOUBLE_EQ(a.h[0][0], 2.0 * x0 * y0 / (r2 * r2));
  EXPECT_DOUBLE_EQ(a.h[1][1], -2.0 * x0 * y0 / (r2 * r2));
  EXPECT_DOUBLE_EQ(a.h[0][1], (y0 * y0 - x0 * x0) / (r2 * r2));

  // Compound-assignment chain: value and first derivatives as in test_jet.cpp, plus symmetry.
  J2 acc = x;
  acc += y;
  acc -= J2(0.25);
  acc *= y;
  acc /= x;
  const double expect = (x0 + y0 - 0.25) * y0 / x0;
  EXPECT_DOUBLE_EQ(acc.a, expect);
  EXPECT_NEAR(acc.v[0], y0 / x0 - (x0 + y0 - 0.25) * y0 / (x0 * x0), 1e-14);
  EXPECT_NEAR(acc.v[1], (x0 + 2.0 * y0 - 0.25) / x0, 1e-14);
  ExpectSymmetric(acc);
}

// The analytic-function check of the elementary rules through one composition: f = sin(x)/x has
// f' = (x cos x - sin x)/x^2 and f'' = (-x^2 sin x - 2x cos x + 2 sin x)/x^3.
TEST(Jet2, SinOverXMatchesClosedForm) {
  const double x0 = 0.7;
  const Jet2<1> x = Jet2<1>::Variable(x0, 0);
  const Jet2<1> f = Sin(x) / x;
  EXPECT_DOUBLE_EQ(f.a, std::sin(x0) / x0);
  EXPECT_DOUBLE_EQ(f.v[0], (x0 * std::cos(x0) - std::sin(x0)) / (x0 * x0));
  const double f2 = (-x0 * x0 * std::sin(x0) - 2.0 * x0 * std::cos(x0) + 2.0 * std::sin(x0)) / (x0 * x0 * x0);
  EXPECT_NEAR(f.h[0][0], f2, 1e-14 * (1.0 + std::fabs(f2)));
}

TEST(Jet2, HessianMatchesCentralDifference) {
  const double points[][3] = { { 0.3, -1.2, 0.7 }, { 1.1, 0.2, -0.4 }, { -0.6, 0.35, 0.25 } };
  for (const auto* p : points) {
    const J3 f = Composite(J3::Variable(p[0], 0), J3::Variable(p[1], 1), J3::Variable(p[2], 2));
    const J3 d = DeviationShape(J3::Variable(p[0], 0), J3::Variable(p[1], 1), J3::Variable(p[2], 2));
    ExpectSymmetric(f);
    ExpectSymmetric(d);
    const double h = 1e-4;
    for (int k = 0; k < 3; k++) {
      for (int j = 0; j < 3; j++) {
        // Mixed difference truncation O(h^2 f'''') ~ 1e-8 and cancellation ~1e-16/(4h^2) ~ 2.5e-9;
        // the Jet2 value is exact to rounding.
        const double fd_f = MixedSecond(CompositeD, p, k, j, h);
        EXPECT_NEAR(f.h[k][j], fd_f, 1e-6 * (1.0 + std::fabs(fd_f))) << "composite k=" << k << " j=" << j;
        const double fd_d = MixedSecond(DeviationD, p, k, j, h);
        EXPECT_NEAR(d.h[k][j], fd_d, 1e-6 * (1.0 + std::fabs(fd_d))) << "atan2 k=" << k << " j=" << j;
      }
    }
  }
}

// Cross-check of one Hessian block between instantiations: h[k][j] of the Jet2<3> evaluation is
// the (0,1) mixed block of the same composite restricted to the (e_k, e_j) plane and evaluated as
// a function of two variables in Jet2<2> — the directional reading of "differentiate first-order
// twice": the mixed second derivative does not depend on which of the two directions is taken
// first, and the two instantiations build it through different code paths.
TEST(Jet2, MixedBlockMatchesPlaneRestriction) {
  const double p[3] = { 0.45, -0.8, 0.35 };
  const J3 f = Composite(J3::Variable(p[0], 0), J3::Variable(p[1], 1), J3::Variable(p[2], 2));
  const J3 d = DeviationShape(J3::Variable(p[0], 0), J3::Variable(p[1], 1), J3::Variable(p[2], 2));
  for (int k = 0; k < 3; k++) {
    for (int j = 0; j < 3; j++) {
      const J2 s = J2::Variable(0.0, 0);
      const J2 t = J2::Variable(0.0, 1);
      const J2 args_f[3] = { J2(p[0]) + (k == 0 ? s : J2(0.0)) + (j == 0 ? t : J2(0.0)),
                             J2(p[1]) + (k == 1 ? s : J2(0.0)) + (j == 1 ? t : J2(0.0)),
                             J2(p[2]) + (k == 2 ? s : J2(0.0)) + (j == 2 ? t : J2(0.0)) };
      const J2 g_f = Composite(args_f[0], args_f[1], args_f[2]);
      EXPECT_DOUBLE_EQ(g_f.h[0][1], f.h[k][j]) << "composite k=" << k << " j=" << j;
      const J2 args_d[3] = { J2(p[0]) + (k == 0 ? s : J2(0.0)) + (j == 0 ? t : J2(0.0)),
                             J2(p[1]) + (k == 1 ? s : J2(0.0)) + (j == 1 ? t : J2(0.0)),
                             J2(p[2]) + (k == 2 ? s : J2(0.0)) + (j == 2 ? t : J2(0.0)) };
      const J2 g_d = DeviationShape(args_d[0], args_d[1], args_d[2]);
      EXPECT_DOUBLE_EQ(g_d.h[0][1], d.h[k][j]) << "atan2 k=" << k << " j=" << j;
    }
  }
}

// The fourth dual direction (the refractive-index slot of the field layer's Jet2<4>): the value,
// first and mixed second derivatives against d/dn closed forms, and against central differences in
// n of the double evaluation.
TEST(Jet2, FourthDirectionMatchesIndexDerivatives) {
  const double x0 = 0.6;
  const double n0 = 1.31;
  // f(x, n) = n*n*x + Sin(n)*x: df/dn = 2n x + cos(n) x, d2f/dn2 = 2x - sin(n) x, d2f/dndx = 2n + cos(n).
  const J4 x = J4::Variable(x0, 0);
  const J4 n = J4::Variable(n0, 3);
  const J4 f = n * n * x + Sin(n) * x;
  EXPECT_DOUBLE_EQ(f.v[3], 2.0 * n0 * x0 + std::cos(n0) * x0);
  EXPECT_DOUBLE_EQ(f.h[3][3], 2.0 * x0 - std::sin(n0) * x0);
  EXPECT_DOUBLE_EQ(f.h[0][3], 2.0 * n0 + std::cos(n0));
  ExpectSymmetric(f);

  // Difference oracle in the n slot on a composite that mixes all four inputs.
  const auto g = [](double x, double y, double z, double n) {
    const double r = std::sqrt(x * x + y * y + z * z);
    return (std::sin(n * r) / r) * (2.0 - std::cos(y)) - (x * z) / (1.0 + n) + 3.0 * (-y);
  };
  const double p[3] = { 0.3, 0.5, 0.8 };
  const J4 gx = J4::Variable(p[0], 0);
  const J4 gy = J4::Variable(p[1], 1);
  const J4 gz = J4::Variable(p[2], 2);
  const J4 gn = J4::Variable(n0, 3);
  const J4 r = Sqrt(gx * gx + gy * gy + gz * gz);
  const J4 fg = (Sin(gn * r) / r) * (2.0 - Cos(gy)) - (gx * gz) / (1.0 + gn) + 3.0 * (-gy);
  EXPECT_DOUBLE_EQ(fg.a, g(p[0], p[1], p[2], n0));  // not bitwise: see jet2.hpp on FMA
  const double h = 1e-6;
  const double dn = (g(p[0], p[1], p[2], n0 + h) - g(p[0], p[1], p[2], n0 - h)) / (2.0 * h);
  EXPECT_NEAR(fg.v[3], dn, 1e-8 * (1.0 + std::fabs(dn)));
  for (int k = 0; k < 4; k++) {
    double pp[4] = { p[0], p[1], p[2], n0 };
    double pm[4] = { p[0], p[1], p[2], n0 };
    const double hk = 1e-4;
    pp[3] += hk;
    pp[k] += hk;
    pm[3] += hk;
    pm[k] -= hk;
    // Mixed difference in (n, input_k); for k = 3 this doubles h, so use the pure second difference.
    if (k < 3) {
      double mp[4] = { p[0], p[1], p[2], n0 };
      double mm[4] = { p[0], p[1], p[2], n0 };
      mp[3] -= hk;
      mp[k] += hk;
      mm[3] -= hk;
      mm[k] -= hk;
      const double fd = (g(pp[0], pp[1], pp[2], pp[3]) - g(pm[0], pm[1], pm[2], pm[3]) - g(mp[0], mp[1], mp[2], mp[3]) +
                         g(mm[0], mm[1], mm[2], mm[3])) /
                        (4.0 * hk * hk);
      EXPECT_NEAR(fg.h[3][k], fd, 1e-6 * (1.0 + std::fabs(fd))) << "k=" << k;
    }
  }
}

// At a zero argument the square root's derivatives are non-finite, as in any forward-mode AD —
// the callers take a square root only where the argument is positive (jet.hpp, same contract).
TEST(Jet2, SqrtAtZeroHasNoFiniteDerivative) {
  const Jet2<1> r = Sqrt(Jet2<1>::Variable(0.0, 0));
  EXPECT_EQ(r.a, 0.0);
  EXPECT_FALSE(std::isfinite(r.v[0]));
  EXPECT_FALSE(std::isfinite(r.h[0][0]));
}

}  // namespace
}  // namespace lumice::analytic
