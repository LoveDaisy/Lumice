#ifndef LUMICE_TEST_SUPPORT_ANALYTIC_DIAGNOSTIC_ROUNDOFF_HPP_
#define LUMICE_TEST_SUPPORT_ANALYTIC_DIAGNOSTIC_ROUNDOFF_HPP_

#include <cmath>
#include <limits>

#include "analytic/fiber_continuation.hpp"

namespace lumice::analytic::test_roundoff {

// Error-budget arithmetic, NOT an independent optics oracle. Exact input doubles are propagated
// through the production direction map. This keeps Snell sqrt/division amplification in the ruler
// instead of fitting a fixed epsilon to observed stored/fresh differences. See analytic-api.md's
// per-pose diagnostic precision contract. The witnesses use normal finite IEEE double arithmetic;
// this is not an interval library for overflow, subnormals or arbitrary transcendental maps.
constexpr double kEps = std::numeric_limits<double>::epsilon();
constexpr double kInflation = 1.0 + 16.0 * kEps;
constexpr double kInfinity = std::numeric_limits<double>::infinity();

struct Rounded {
  double value = 0.0;
  double error = 0.0;

  Rounded() = default;
  Rounded(double v) : value(v) {}  // NOLINT(google-explicit-constructor)
  Rounded(double v, double e) : value(v), error(e) {}

  friend Rounded operator-(Rounded x) { return { -x.value, x.error }; }
  friend Rounded operator+(Rounded x, Rounded y) {
    const double v = x.value + y.value;
    return { v, kInflation * (x.error + y.error + kEps * std::fabs(v)) };
  }
  friend Rounded operator-(Rounded x, Rounded y) { return x + (-y); }
  friend Rounded operator*(Rounded x, Rounded y) {
    const double v = x.value * y.value;
    return { v, kInflation * (std::fabs(x.value) * y.error + std::fabs(y.value) * x.error + x.error * y.error +
                              kEps * std::fabs(v)) };
  }
  friend Rounded operator/(Rounded x, Rounded y) {
    const double v = x.value / y.value;
    const double denominator = std::fabs(y.value) - y.error;
    return { v, denominator > 0.0 ?
                    kInflation * ((x.error + std::fabs(v) * y.error) / denominator + kEps * std::fabs(v)) :
                    kInfinity };
  }
};

inline Rounded Sqrt(Rounded x) {
  const double v = std::sqrt(x.value);
  if (x.value == 0.0 && x.error == 0.0) {
    return { v, 0.0 };
  }
  return { v, x.value > x.error ? kInflation * (x.error / (v + std::sqrt(x.value - x.error)) + kEps * v) : kInfinity };
}

inline void Cross(const Rounded a[3], const Rounded b[3], Rounded out[3]) {
  for (int k = 0; k < 3; k++) {
    out[k] = a[(k + 1) % 3] * b[(k + 2) % 3] - a[(k + 2) % 3] * b[(k + 1) % 3];
  }
}

// First-order arithmetic only. The physics stays in the same templated TracePathChain; copying
// Jet's chain rule here supplies error radii, never the expected direction/Jacobian values.
struct RoundoffJet {
  Rounded a;
  Rounded v[3];

  RoundoffJet() = default;
  RoundoffJet(double value) : a(value) {}  // NOLINT(google-explicit-constructor)
  static RoundoffJet Variable(double value, int k) {
    RoundoffJet r(value);
    r.v[k] = 1.0;
    return r;
  }
  friend RoundoffJet operator-(RoundoffJet x) {
    x.a = -x.a;
    for (auto& v : x.v) {
      v = -v;
    }
    return x;
  }
  friend RoundoffJet operator+(RoundoffJet x, const RoundoffJet& y) {
    x.a = x.a + y.a;
    for (int k = 0; k < 3; k++) {
      x.v[k] = x.v[k] + y.v[k];
    }
    return x;
  }
  friend RoundoffJet operator-(RoundoffJet x, const RoundoffJet& y) { return x + (-y); }
  friend RoundoffJet operator*(const RoundoffJet& x, const RoundoffJet& y) {
    RoundoffJet r;
    r.a = x.a * y.a;
    for (int k = 0; k < 3; k++) {
      r.v[k] = x.v[k] * y.a + x.a * y.v[k];
    }
    return r;
  }
  friend RoundoffJet operator/(const RoundoffJet& x, const RoundoffJet& y) {
    RoundoffJet r;
    r.a = x.a / y.a;
    for (int k = 0; k < 3; k++) {
      r.v[k] = (x.v[k] - r.a * y.v[k]) / y.a;
    }
    return r;
  }
  RoundoffJet& operator-=(const RoundoffJet& y) { return *this = *this - y; }
};

inline double ValueOf(const RoundoffJet& x) {
  return x.a.value;
}
inline RoundoffJet Sqrt(const RoundoffJet& x) {
  RoundoffJet r;
  r.a = Sqrt(x.a);
  const Rounded derivative = Rounded(0.5) / r.a;
  for (int k = 0; k < 3; k++) {
    r.v[k] = x.v[k] * derivative;
  }
  return r;
}
// Exp at delta=0 uses its Taylor branch, never these transcendental branches. Return an unusable
// radius if that premise changes instead of silently making a libm-accuracy assumption.
inline RoundoffJet Sin(const RoundoffJet&) {
  RoundoffJet r;
  r.a = { 0.0, kInfinity };
  return r;
}
inline RoundoffJet Cos(const RoundoffJet&) {
  return Sin(RoundoffJet{});
}

// A bound on the absolute difference of EACH singular value in two independent evaluations at
// this pose. Weyl contributes the Frobenius input radius; gamma64*M bounds the 2x3 SVD's local
// cross/norm and Gram/hypot/sqrt/division roundoff. The factor two covers both evaluations. It is
// conditional on finite radii and resolution of the rank gap, checked by the caller.
template <class Map>
double SingularValueAllowance(const Map& map, const double pose[9]) {
  RoundoffJet delta[3];
  for (int k = 0; k < 3; k++) {
    delta[k] = RoundoffJet::Variable(0.0, k);
  }
  RoundoffJet exponential[9];
  RoundoffJet rotation[9];
  RoundoffJet direction[3];
  so3::Exp(delta, exponential);
  so3::MatMul(pose, exponential, rotation);
  map.Direction(rotation, direction);

  Rounded d[3] = { direction[0].a, direction[1].a, direction[2].a };
  const Rounded norm = Sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  for (auto& x : d) {
    x = x / norm;
  }
  // Any exact orthonormal basis has the same singular values, including at a reference-axis tie.
  int axis = 0;
  for (int k = 1; k < 3; k++) {
    if (std::fabs(d[k].value) < std::fabs(d[axis].value)) {
      axis = k;
    }
  }
  Rounded ref[3];
  ref[axis] = 1.0;
  Rounded b0[3];
  Rounded b1[3];
  Cross(d, ref, b0);
  const Rounded basis_norm = Sqrt(b0[0] * b0[0] + b0[1] * b0[1] + b0[2] * b0[2]);
  for (auto& x : b0) {
    x = x / basis_norm;
  }
  Cross(d, b0, b1);
  double error_squared = 0.0;
  double scale_squared = 0.0;
  for (const auto* b : { b0, b1 }) {
    for (int k = 0; k < 3; k++) {
      const Rounded a = b[0] * direction[0].v[k] + b[1] * direction[1].v[k] + b[2] * direction[2].v[k];
      error_squared += a.error * a.error;
      scale_squared += a.value * a.value;
    }
  }
  constexpr double kGamma64 = 64.0 * kEps / (1.0 - 64.0 * kEps);
  return kInflation * 2.0 * (std::sqrt(error_squared) + kGamma64 * std::sqrt(scale_squared));
}

}  // namespace lumice::analytic::test_roundoff

#endif  // LUMICE_TEST_SUPPORT_ANALYTIC_DIAGNOSTIC_ROUNDOFF_HPP_
