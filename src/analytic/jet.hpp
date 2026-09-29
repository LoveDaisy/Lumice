#ifndef LUMICE_ANALYTIC_JET_HPP_
#define LUMICE_ANALYTIC_JET_HPP_

// First-order forward-mode dual numbers: a value and its gradient with respect to N inputs. The
// continuation's Jacobians (the 2x3 residual Jacobian and the 3x3 bordered Newton system) are
// computed by evaluating the same templated code once with Jet<3> in place of double, so there is
// no second, hand-derived copy of any derivative (doc/raypath-analysis.md section 5.1.6: no AD
// framework). Only the operations that code uses are defined; an expression that needs another one
// fails to compile rather than silently dropping a derivative term.
//
// The value part of every operation is the double operation itself. It is not promised to be
// bit-identical to the double instantiation of the same code: the compiler may fuse a double
// `a * b + c` into one FMA, which it cannot do across the separate Jet operators.

#include <cmath>

namespace lumice::analytic {

template <int N>
struct Jet {
  double a = 0.0;  // value
  double v[N]{};   // d(value)/d(input_k)

  Jet() = default;
  // A constant: zero gradient. Implicit, so double literals mix into Jet expressions.
  Jet(double value) : a(value) {}  // NOLINT(google-explicit-constructor)

  // The k-th input variable at `value`: unit gradient along k.
  static Jet Variable(double value, int k) {
    Jet j(value);
    j.v[k] = 1.0;
    return j;
  }

  Jet& operator+=(const Jet& o) {
    a += o.a;
    for (int k = 0; k < N; k++) {
      v[k] += o.v[k];
    }
    return *this;
  }
  Jet& operator-=(const Jet& o) {
    a -= o.a;
    for (int k = 0; k < N; k++) {
      v[k] -= o.v[k];
    }
    return *this;
  }
  Jet& operator*=(const Jet& o) { return *this = *this * o; }
  Jet& operator/=(const Jet& o) { return *this = *this / o; }

  friend Jet operator-(const Jet& x) {
    Jet r;
    r.a = -x.a;
    for (int k = 0; k < N; k++) {
      r.v[k] = -x.v[k];
    }
    return r;
  }
  friend Jet operator+(Jet x, const Jet& y) { return x += y; }
  friend Jet operator-(Jet x, const Jet& y) { return x -= y; }
  friend Jet operator*(const Jet& x, const Jet& y) {
    Jet r;
    r.a = x.a * y.a;
    for (int k = 0; k < N; k++) {
      r.v[k] = x.v[k] * y.a + x.a * y.v[k];
    }
    return r;
  }
  friend Jet operator/(const Jet& x, const Jet& y) {
    Jet r;
    r.a = x.a / y.a;
    for (int k = 0; k < N; k++) {
      r.v[k] = (x.v[k] - r.a * y.v[k]) / y.a;
    }
    return r;
  }
};

// Value of a scalar for branching and for gates: the one place templated code looks at a number
// without its derivative (a branch is piecewise-constant, so it has no derivative to lose).
inline double ValueOf(double x) {
  return x;
}
template <int N>
double ValueOf(const Jet<N>& x) {
  return x.a;
}

// Sqrt / Sin / Cos: one name for both scalar types, so templated code calls Sqrt(x) whatever x is.
inline double Sqrt(double x) {
  return std::sqrt(x);
}
inline double Sin(double x) {
  return std::sin(x);
}
inline double Cos(double x) {
  return std::cos(x);
}

// d sqrt(x) = dx / (2 sqrt(x)); at x == 0 the gradient is inf/NaN, as in any forward-mode AD. The
// callers take a square root only where the domain gate has already made the argument positive.
template <int N>
Jet<N> Sqrt(const Jet<N>& x) {
  Jet<N> r;
  r.a = std::sqrt(x.a);
  const double d = 0.5 / r.a;
  for (int k = 0; k < N; k++) {
    r.v[k] = x.v[k] * d;
  }
  return r;
}

template <int N>
Jet<N> Sin(const Jet<N>& x) {
  Jet<N> r;
  r.a = std::sin(x.a);
  const double d = std::cos(x.a);
  for (int k = 0; k < N; k++) {
    r.v[k] = x.v[k] * d;
  }
  return r;
}

template <int N>
Jet<N> Cos(const Jet<N>& x) {
  Jet<N> r;
  r.a = std::cos(x.a);
  const double d = -std::sin(x.a);
  for (int k = 0; k < N; k++) {
    r.v[k] = x.v[k] * d;
  }
  return r;
}

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_JET_HPP_
