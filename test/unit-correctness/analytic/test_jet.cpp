// Forward-mode dual numbers (src/analytic/jet.hpp) against closed-form derivatives and against a
// central difference of the double evaluation — two oracles that share nothing with the Jet rules.
//
// symmetry_semantics: none — no face sequence is involved.

#include <gtest/gtest.h>

#include <cmath>

#include "analytic/jet.hpp"

namespace lumice::analytic {
namespace {

using J3 = Jet<3>;

// A composite exercising every operation the continuation uses: + - * / unary-, Sqrt, Sin, Cos,
// and mixed Jet/double arithmetic.
template <class S>
S Composite(const S& x, const S& y, const S& z) {
  const S r2 = x * x + y * y + z * z;
  const S r = Sqrt(r2);
  return (Sin(r) / r) * (2.0 - Cos(y)) - (x * z) / (1.0 + r2) + 3.0 * (-y);
}

TEST(Jet, ArithmeticMatchesClosedFormDerivatives) {
  const double x0 = 0.3;
  const double y0 = -1.2;
  const J3 x = J3::Variable(x0, 0);
  const J3 y = J3::Variable(y0, 1);
  const J3 c(2.5);

  const J3 p = x * y;
  EXPECT_DOUBLE_EQ(p.a, x0 * y0);
  EXPECT_DOUBLE_EQ(p.v[0], y0);
  EXPECT_DOUBLE_EQ(p.v[1], x0);
  EXPECT_EQ(p.v[2], 0.0);

  const J3 q = x / y;
  EXPECT_DOUBLE_EQ(q.v[0], 1.0 / y0);
  EXPECT_DOUBLE_EQ(q.v[1], -x0 / (y0 * y0));

  const J3 s = x - c * y + 1.0;
  EXPECT_DOUBLE_EQ(s.v[0], 1.0);
  EXPECT_DOUBLE_EQ(s.v[1], -2.5);

  const J3 t = 1.0 / x;
  EXPECT_DOUBLE_EQ(t.v[0], -1.0 / (x0 * x0));

  const J3 u = Sqrt(x);
  EXPECT_DOUBLE_EQ(u.v[0], 0.5 / std::sqrt(x0));
  EXPECT_DOUBLE_EQ(Sin(y).v[1], std::cos(y0));
  EXPECT_DOUBLE_EQ(Cos(y).v[1], -std::sin(y0));

  J3 acc = x;
  acc += y;
  acc -= c;
  acc *= y;
  acc /= x;
  const double expect = (x0 + y0 - 2.5) * y0 / x0;
  EXPECT_DOUBLE_EQ(acc.a, expect);
  // d/dx [(x + y - c) y / x] = y/x - (x + y - c) y / x^2
  EXPECT_NEAR(acc.v[0], y0 / x0 - (x0 + y0 - 2.5) * y0 / (x0 * x0), 1e-14);
  // d/dy = (x + 2y - c) / x
  EXPECT_NEAR(acc.v[1], (x0 + 2.0 * y0 - 2.5) / x0, 1e-14);
}

TEST(Jet, CompositeGradientMatchesCentralDifference) {
  const double points[][3] = { { 0.3, -1.2, 0.7 }, { 1.5, 0.2, -0.4 }, { -0.05, 0.01, 0.02 } };
  for (const auto& p : points) {
    const J3 f = Composite(J3::Variable(p[0], 0), J3::Variable(p[1], 1), J3::Variable(p[2], 2));
    EXPECT_DOUBLE_EQ(f.a, Composite(p[0], p[1], p[2]));  // not bitwise: see jet.hpp on FMA
    for (int k = 0; k < 3; k++) {
      const double h = 1e-5;
      double plus[3] = { p[0], p[1], p[2] };
      double minus[3] = { p[0], p[1], p[2] };
      plus[k] += h;
      minus[k] -= h;
      const double fd = (Composite(plus[0], plus[1], plus[2]) - Composite(minus[0], minus[1], minus[2])) / (2.0 * h);
      // Central difference error is O(h^2 f''') ~ 1e-10 here; the Jet is exact to rounding.
      EXPECT_NEAR(f.v[k], fd, 1e-9 * (1.0 + std::fabs(fd))) << "k=" << k;
    }
  }
}

// At a zero argument the square root's derivative is infinite: forward mode reports it (inf, or
// NaN where a zero tangent multiplies it) rather than a plausible finite number. Callers never
// take a square root there — the domain gate rejects a non-positive discriminant first.
TEST(Jet, SqrtAtZeroHasNoFiniteDerivative) {
  const J3 r = Sqrt(J3::Variable(0.0, 0));
  EXPECT_EQ(r.a, 0.0);
  EXPECT_TRUE(std::isinf(r.v[0]));
  EXPECT_FALSE(std::isfinite(r.v[1]));
}

}  // namespace
}  // namespace lumice::analytic
