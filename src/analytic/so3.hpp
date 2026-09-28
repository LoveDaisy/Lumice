#ifndef LUMICE_ANALYTIC_SO3_HPP_
#define LUMICE_ANALYTIC_SO3_HPP_

// SO(3) and small dense linear algebra for fiber continuation, in right-trivialised coordinates:
// a tangent vector delta at R stands for R exp([delta]_x). Each function is a port of the function
// of the same meaning in LI's src/lumice_integral/so3.py / continuation.py / analytic.py (named in
// its comment), formula for formula, so the continuation's geometry is LI's (LI
// docs/phase1-math-contract.md section 3 and 5). Matrices are row-major double[9].
//
// Internal to liblumice_analytic: nothing here is exported, and nothing here is a second authority
// for a primitive the engine already has — the engine's own rotation code (core/math.hpp) is float
// and lives in the simulator's hot loop; this is the double-precision continuation geometry only.

#include <cmath>

#include "analytic/jet.hpp"

namespace lumice::analytic::so3 {

template <class S, class T>
S Dot3(const S a[3], const T b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

inline void Cross3(const double a[3], const double b[3], double out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

inline double Norm3(const double a[3]) {
  return std::sqrt(Dot3(a, a));
}

// out = A B (3x3, row-major); A and B may be of different scalar types.
template <class S, class TA, class TB>
void MatMul(const TA a[9], const TB b[9], S out[9]) {
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      out[i * 3 + j] = a[i * 3 + 0] * b[0 * 3 + j] + a[i * 3 + 1] * b[1 * 3 + j] + a[i * 3 + 2] * b[2 * 3 + j];
    }
  }
}

// out = A^T B.
template <class S, class TA, class TB>
void MatTMul(const TA a[9], const TB b[9], S out[9]) {
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      out[i * 3 + j] = a[0 * 3 + i] * b[0 * 3 + j] + a[1 * 3 + i] * b[1 * 3 + j] + a[2 * 3 + i] * b[2 * 3 + j];
    }
  }
}

// The skew part of a matrix as a vector, vee((M - M^T) / 2): LI's `skew_vector`.
template <class S>
void SkewVector(const S m[9], S out[3]) {
  out[0] = (m[7] - m[5]) / 2.0;
  out[1] = (m[2] - m[6]) / 2.0;
  out[2] = (m[3] - m[1]) / 2.0;
}

// Rotation vector -> rotation (so3.exp): Rodrigues with the Taylor branch below theta^2 = 1e-8,
// the same split as LI, so a Jet evaluated at delta = 0 differentiates the Taylor polynomial exactly
// as jax.jacfwd does.
template <class S>
void Exp(const S w[3], S out[9]) {
  const S theta_squared = Dot3(w, w);
  S a;
  S b;
  if (ValueOf(theta_squared) < 1e-8) {
    const S value_squared = theta_squared * theta_squared;
    a = 1.0 - theta_squared / 6.0 + value_squared / 120.0;
    b = 0.5 - theta_squared / 24.0 + value_squared / 720.0;
  } else {
    const S theta = Sqrt(theta_squared);
    a = Sin(theta) / theta;
    b = (1.0 - Cos(theta)) / theta_squared;
  }
  // generator = hat(w); generator @ generator.
  const S g[9] = { S(0.0), -w[2], w[1], w[2], S(0.0), -w[0], -w[1], w[0], S(0.0) };
  S g2[9];
  MatMul(g, g, g2);
  // I + a G + b G^2, summed in LI's order.
  for (int i = 0; i < 9; i++) {
    out[i] = (i % 4 == 0 ? 1.0 : 0.0) + a * g[i] + b * g2[i];
  }
}

// Rotation -> rotation vector on the injectivity domain (so3.log). Rotations near angle pi are
// not supported, as in LI (the axis is ill-conditioned there).
inline void Log(const double r[9], double out[3]) {
  double cosine = (r[0] + r[4] + r[8] - 1.0) / 2.0;
  cosine = cosine < -1.0 ? -1.0 : (cosine > 1.0 ? 1.0 : cosine);
  double skew[3];
  SkewVector(r, skew);
  const double sine_squared = Dot3(skew, skew);
  const double sine = sine_squared > 0.0 ? std::sqrt(sine_squared) : 0.0;
  const double theta = std::atan2(sine, cosine);
  const double theta_squared = theta * theta;
  const double scale = theta_squared < 1e-8 ? 1.0 + theta_squared / 6.0 + 7.0 * theta_squared * theta_squared / 360.0 :
                                              theta / std::sin(theta);
  for (int i = 0; i < 3; i++) {
    out[i] = scale * skew[i];
  }
}

// Geodesic angle between two rotations (so3.rotation_distance): the angle of A^T B from atan2 of
// the skew part's norm and the trace, never acos of the trace (LI contract section 3), which loses
// the angle near the identity.
inline double Distance(const double a[9], const double b[9]) {
  double rel[9];
  MatTMul(a, b, rel);
  double cosine = (rel[0] + rel[4] + rel[8] - 1.0) / 2.0;
  cosine = cosine < -1.0 ? -1.0 : (cosine > 1.0 ? 1.0 : cosine);
  double skew[3];
  SkewVector(rel, skew);
  return std::atan2(Norm3(skew), cosine);
}

// The seed-relative transverse coordinate (continuation._section_coordinate): the skew part of
// seed^T R along the seed tangent. Zero at the seed; its sign change on an accepted edge is the
// section crossing of the closure test.
template <class S>
S SectionCoordinate(const double seed[9], const S r[9], const double tangent[3]) {
  S rel[9];
  MatTMul(seed, r, rel);
  S skew[3];
  SkewVector(rel, skew);
  return tangent[0] * skew[0] + tangent[1] * skew[1] + tangent[2] * skew[2];
}

// Deterministic orthonormal basis of the tangent plane at `direction` (analytic.tangent_basis):
// the reference is the coordinate axis least aligned with it (first one on a tie), first =
// normalize(d x ref), second = d x first. `basis` holds the two columns, basis[0] and basis[1].
inline void TangentBasis(const double direction[3], double basis[2][3]) {
  const double norm = Norm3(direction);
  const double d[3] = { direction[0] / norm, direction[1] / norm, direction[2] / norm };
  int ref_axis = 0;
  for (int i = 1; i < 3; i++) {
    if (std::fabs(d[i]) < std::fabs(d[ref_axis])) {
      ref_axis = i;
    }
  }
  double ref[3] = { 0.0, 0.0, 0.0 };
  ref[ref_axis] = 1.0;
  double first[3];
  Cross3(d, ref, first);
  const double first_norm = Norm3(first);
  for (double& x : first) {
    x /= first_norm;
  }
  Cross3(d, first, basis[1]);
  for (int i = 0; i < 3; i++) {
    basis[0][i] = first[i];
  }
}

// Singular values and null vector of a 2x3 matrix with rows r0, r1 (the residual Jacobian A).
// sigma1 * sigma2 = |r0 x r1| (Lagrange's identity: no cancellation), sigma1^2 the larger root of
// s^2 - (|r0|^2 + |r1|^2) s + |r0 x r1|^2, sigma2 = product / sigma1 — so a sigma2 near the 1e-8
// rank gate keeps its relative precision, which det = |r0|^2 |r1|^2 - (r0.r1)^2 would not.
// `null_vector` = (r0 x r1) / |r0 x r1|, the unit kernel direction with a deterministic sign; left
// unset when the product is zero.
struct TwoByThreeSvd {
  double sigma1 = 0.0;
  double sigma2 = 0.0;
  double null_vector[3] = { 0.0, 0.0, 0.0 };
};

inline TwoByThreeSvd SvdTwoByThree(const double r0[3], const double r1[3]) {
  TwoByThreeSvd out;
  double cross[3];
  Cross3(r0, r1, cross);
  const double product = Norm3(cross);
  const double trace = Dot3(r0, r0) + Dot3(r1, r1);
  double disc = trace * trace - 4.0 * product * product;
  disc = disc > 0.0 ? disc : 0.0;
  out.sigma1 = std::sqrt((trace + std::sqrt(disc)) / 2.0);
  out.sigma2 = out.sigma1 > 0.0 ? product / out.sigma1 : 0.0;
  if (product > 0.0) {
    for (int i = 0; i < 3; i++) {
      out.null_vector[i] = cross[i] / product;
    }
  }
  return out;
}

// 2-norm condition number of a 3x3 matrix (numpy.linalg.cond): the ratio of its largest and
// smallest singular values, from a one-sided Jacobi (Hestenes) SVD — relative accuracy for the
// small singular value, where J^T J would square a 1e8 condition number into rounding. Infinite for
// a singular matrix, NaN when an entry is not finite.
inline double ConditionNumber(const double m[9]) {
  double c[3][3];  // c[j] = column j
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      c[j][i] = m[i * 3 + j];
      if (!std::isfinite(m[i * 3 + j])) {
        return std::nan("");
      }
    }
  }
  for (int sweep = 0; sweep < 30; sweep++) {
    bool rotated = false;
    for (int p = 0; p < 2; p++) {
      for (int q = p + 1; q < 3; q++) {
        const double alpha = Dot3(c[p], c[p]);
        const double beta = Dot3(c[q], c[q]);
        const double gamma = Dot3(c[p], c[q]);
        if (gamma == 0.0 || std::fabs(gamma) <= 1e-17 * std::sqrt(alpha * beta)) {
          continue;
        }
        rotated = true;
        const double zeta = (beta - alpha) / (2.0 * gamma);
        const double t = (zeta >= 0.0 ? 1.0 : -1.0) / (std::fabs(zeta) + std::sqrt(1.0 + zeta * zeta));
        const double cs = 1.0 / std::sqrt(1.0 + t * t);
        const double sn = cs * t;
        for (int i = 0; i < 3; i++) {
          const double cp = c[p][i];
          const double cq = c[q][i];
          c[p][i] = cs * cp - sn * cq;
          c[q][i] = sn * cp + cs * cq;
        }
      }
    }
    if (!rotated) {
      break;
    }
  }
  double smax = 0.0;
  double smin = INFINITY;
  for (const auto& col : c) {
    const double s = Norm3(col);
    smax = s > smax ? s : smax;
    smin = s < smin ? s : smin;
  }
  return smin > 0.0 ? smax / smin : INFINITY;
}

// Solves the 3x3 system M x = b by Gaussian elimination with partial pivoting (the algorithm of
// numpy.linalg.solve / LAPACK gesv). Returns false on an exactly singular pivot; x is then not set.
inline bool Solve3(const double m[9], const double b[3], double x[3]) {
  double a[3][4];
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      a[i][j] = m[i * 3 + j];
    }
    a[i][3] = b[i];
  }
  for (int k = 0; k < 3; k++) {
    int pivot = k;
    for (int i = k + 1; i < 3; i++) {
      if (std::fabs(a[i][k]) > std::fabs(a[pivot][k])) {
        pivot = i;
      }
    }
    if (a[pivot][k] == 0.0) {
      return false;
    }
    if (pivot != k) {
      for (int j = 0; j < 4; j++) {
        const double tmp = a[k][j];
        a[k][j] = a[pivot][j];
        a[pivot][j] = tmp;
      }
    }
    for (int i = k + 1; i < 3; i++) {
      const double f = a[i][k] / a[k][k];
      for (int j = k; j < 4; j++) {
        a[i][j] -= f * a[k][j];
      }
    }
  }
  for (int i = 2; i >= 0; i--) {
    double s = a[i][3];
    for (int j = i + 1; j < 3; j++) {
      s -= a[i][j] * x[j];
    }
    x[i] = s / a[i][i];
  }
  return true;
}

}  // namespace lumice::analytic::so3

#endif  // LUMICE_ANALYTIC_SO3_HPP_
