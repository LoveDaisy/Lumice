#ifndef LUMICE_ANALYTIC_JET2_HPP_
#define LUMICE_ANALYTIC_JET2_HPP_

// Second-order forward-mode hyper-dual numbers: a value, its gradient and its full Hessian with
// respect to N inputs, all in one expression sweep (jet.hpp is the first-order tier). Consumers:
// the u-S^2 field layer (dp_field.hpp) differentiates D_P and the domain margins in u (three dual
// directions) and in the refractive index n (the fourth, LI docs/chromatic-module-c.md section 4
// "extends to d/dn by one more dual direction"), so Jet2<4> is the field layer's scalar type.
//
// Division of labour with jet.hpp, deliberate and permanent: Jet<N> carries no Hessian and is the
// module A hot path (the continuation's Jacobians); Jet2<N> is the field layer's. Two tiers of one
// discipline, not two implementations of one semantics — an expression is written once against a
// scalar-type template and instantiated at the tier its consumer needs. Do not merge them and do
// not instantiate one where the other is meant: Jet2 costs N^2 doubles per value, which the hot
// path must not pay, and Jet silently drops every second-order term, which the field layer must
// not get. Only the operations the consumers use are defined; an expression that needs another one
// fails to compile rather than silently dropping a derivative term.
//
// The second-order algebra is LI's implementation of the same numbers, checked there against
// JAX value/gradient/Hessian to <= 1e-11 relative error (LI docs/chromatic-module-c.md section
// 4 names the template), transcribed step for step. If a rule here disagrees with LI's the LI
// one wins; do not re-derive by hand. The rules, for reference:
//   product  (a,v,H)(b,w,G): value ab, gradient aw+bv, Hessian aG + bH + vw^T + wv^T
//   quotient c = a/b:        v_c = (v - c w)/b, H_c = (H - c G - v_c w^T - w v_c^T)/b
//   univariate h(a):         gradient h' v, Hessian h' H + h'' vv^T  (compose_scalar below)
//   atan2(y, x):             the bivariate second-order chain rule, spelled out at its site
//
// The value part of every operation is the double operation itself. It is not promised to be
// bit-identical to the double instantiation of the same code: the compiler may fuse a double
// `a * b + c` into one FMA, which it cannot do across the separate Jet2 operators (jet.hpp, same
// disclaimer).

#include <cmath>

namespace lumice::analytic {

template <int N>
struct Jet2 {
  double a = 0.0;    // value
  double v[N]{};     // d(value)/d(input_k)
  double h[N][N]{};  // d^2(value)/d(input_k) d(input_j); symmetric by algebra, asserted in tests

  Jet2() = default;
  // A constant: zero gradient and Hessian. Implicit, so double literals mix into Jet2 expressions.
  Jet2(double value) : a(value) {}  // NOLINT(google-explicit-constructor)

  // The k-th input variable at `value`: unit gradient along k.
  static Jet2 Variable(double value, int k) {
    Jet2 j(value);
    j.v[k] = 1.0;
    return j;
  }

  Jet2& operator+=(const Jet2& o) {
    a += o.a;
    for (int k = 0; k < N; k++) {
      v[k] += o.v[k];
      for (int j = 0; j < N; j++) {
        h[k][j] += o.h[k][j];
      }
    }
    return *this;
  }
  Jet2& operator-=(const Jet2& o) {
    a -= o.a;
    for (int k = 0; k < N; k++) {
      v[k] -= o.v[k];
      for (int j = 0; j < N; j++) {
        h[k][j] -= o.h[k][j];
      }
    }
    return *this;
  }
  Jet2& operator*=(const Jet2& o) { return *this = *this * o; }
  Jet2& operator/=(const Jet2& o) { return *this = *this / o; }

  friend Jet2 operator-(const Jet2& x) {
    Jet2 r;
    r.a = -x.a;
    for (int k = 0; k < N; k++) {
      r.v[k] = -x.v[k];
      for (int j = 0; j < N; j++) {
        r.h[k][j] = -x.h[k][j];
      }
    }
    return r;
  }
  friend Jet2 operator+(Jet2 x, const Jet2& y) { return x += y; }
  friend Jet2 operator-(Jet2 x, const Jet2& y) { return x -= y; }
  friend Jet2 operator*(const Jet2& x, const Jet2& y) {
    Jet2 r;
    r.a = x.a * y.a;
    for (int k = 0; k < N; k++) {
      r.v[k] = x.v[k] * y.a + x.a * y.v[k];
    }
    for (int k = 0; k < N; k++) {
      for (int j = 0; j < N; j++) {
        r.h[k][j] = x.h[k][j] * y.a + x.a * y.h[k][j] + x.v[k] * y.v[j] + x.v[j] * y.v[k];
      }
    }
    return r;
  }
  friend Jet2 operator/(const Jet2& x, const Jet2& y) {
    Jet2 r;
    r.a = x.a / y.a;
    for (int k = 0; k < N; k++) {
      r.v[k] = (x.v[k] - r.a * y.v[k]) / y.a;
    }
    for (int k = 0; k < N; k++) {
      for (int j = 0; j < N; j++) {
        r.h[k][j] = (x.h[k][j] - r.a * y.h[k][j] - r.v[k] * y.v[j] - r.v[j] * y.v[k]) / y.a;
      }
    }
    return r;
  }
};

// Value of a scalar for branching and for gates: the one place templated code looks at a number
// without its derivatives (a branch is piecewise-constant, so it has no derivative to lose). The
// double overload lives in jet.hpp (this header never redefines it: a TU that includes both —
// every field-layer TU does — would hold two definitions of one function).
template <int N>
double ValueOf(const Jet2<N>& x) {
  return x.a;
}

// Univariate composition h(a) from h, h', h'' at a's value: the order-2 chain rule every
// elementary function below is built on.
template <int N>
Jet2<N> ComposeScalar(const Jet2<N>& a, double h_value, double h_prime, double h_second) {
  Jet2<N> r;
  r.a = h_value;
  for (int k = 0; k < N; k++) {
    r.v[k] = h_prime * a.v[k];
  }
  for (int k = 0; k < N; k++) {
    for (int j = 0; j < N; j++) {
      r.h[k][j] = h_prime * a.h[k][j] + h_second * a.v[k] * a.v[j];
    }
  }
  return r;
}

// One name for both scalar types, so templated code calls Sqrt(x) whatever x is (jet.hpp's
// discipline). d sqrt(x) at x == 0 is infinite, as in any forward-mode AD.
template <int N>
Jet2<N> Sqrt(const Jet2<N>& x) {
  const double s = std::sqrt(x.a);
  return ComposeScalar(x, s, 0.5 / s, -0.25 / (x.a * s));
}

template <int N>
Jet2<N> Sin(const Jet2<N>& x) {
  return ComposeScalar(x, std::sin(x.a), std::cos(x.a), -std::sin(x.a));
}

template <int N>
Jet2<N> Cos(const Jet2<N>& x) {
  return ComposeScalar(x, std::cos(x.a), -std::sin(x.a), -std::cos(x.a));
}

// atan2(y, x), the bivariate order-2 chain rule: with f = atan2, the gradient is f_y dy + f_x dx
// and the Hessian adds f_yy dy dy + f_xy (dy dx + dx dy) + f_xx dx dx on top of the jets' own
// second-order parts. D_P's atan2 form (dp_field) sits on this rule; its |y| argument crossing
// zero at D in {0, pi} goes through Sqrt above, which is non-finite there in LI's JAX
// differentiation too (jnp.linalg.norm at a zero vector) — value exact, derivative not.
template <int N>
Jet2<N> Atan2(const Jet2<N>& y, const Jet2<N>& x) {
  Jet2<N> r;
  r.a = std::atan2(y.a, x.a);
  const double r2 = x.a * x.a + y.a * y.a;
  const double fy = x.a / r2;
  const double fx = -y.a / r2;
  const double fyy = -2.0 * x.a * y.a / (r2 * r2);
  const double fxx = 2.0 * x.a * y.a / (r2 * r2);
  const double fxy = (y.a * y.a - x.a * x.a) / (r2 * r2);
  for (int k = 0; k < N; k++) {
    r.v[k] = fy * y.v[k] + fx * x.v[k];
  }
  for (int k = 0; k < N; k++) {
    for (int j = 0; j < N; j++) {
      r.h[k][j] = fy * y.h[k][j] + fx * x.h[k][j] + fyy * y.v[k] * y.v[j] + fxy * (y.v[k] * x.v[j] + y.v[j] * x.v[k]) +
                  fxx * x.v[k] * x.v[j];
    }
  }
  return r;
}

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_JET2_HPP_
