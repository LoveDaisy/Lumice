// SO(3) and small linear algebra of the fiber continuation (src/analytic/so3.hpp), against
// closed forms, an independent quaternion construction, and central differences.
//
// symmetry_semantics: none — no face sequence is involved.

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <random>

#include "analytic/jet.hpp"
#include "analytic/so3.hpp"
#include "support/portable_random.hpp"

namespace lumice::analytic {
namespace {

using Mat = std::array<double, 9>;

// Rotation by `angle` about unit `axis` from the unit quaternion (cos a/2, sin a/2 axis): shares
// no formula with the Rodrigues form under test.
Mat QuaternionRotation(const double axis[3], double angle) {
  const double w = std::cos(angle / 2.0);
  const double s = std::sin(angle / 2.0);
  const double x = s * axis[0], y = s * axis[1], z = s * axis[2];
  return { 1 - 2 * (y * y + z * z), 2 * (x * y - w * z),     2 * (x * z + w * y),
           2 * (x * y + w * z),     1 - 2 * (x * x + z * z), 2 * (y * z - w * x),
           2 * (x * z - w * y),     2 * (y * z + w * x),     1 - 2 * (x * x + y * y) };
}

Mat ExpOf(double wx, double wy, double wz) {
  const double w[3] = { wx, wy, wz };
  Mat r{};
  so3::Exp(w, r.data());
  return r;
}

double MaxAbsDiff(const Mat& a, const Mat& b) {
  double m = 0.0;
  for (int i = 0; i < 9; i++) {
    m = std::fmax(m, std::fabs(a[i] - b[i]));
  }
  return m;
}

TEST(So3, ExpMatchesTheQuaternionRotationOnBothBranches) {
  const double axis[3] = { 2.0 / 7.0, -3.0 / 7.0, 6.0 / 7.0 };
  // 1e-5 is inside the Taylor branch (theta^2 < 1e-8), the rest in the regular one.
  for (double angle : { 0.0, 1e-9, 1e-5, 0.3, 1.7, 3.0 }) {
    const Mat r = ExpOf(angle * axis[0], angle * axis[1], angle * axis[2]);
    EXPECT_LT(MaxAbsDiff(r, QuaternionRotation(axis, angle)), 1e-15) << angle;
  }
}

TEST(So3, LogInvertsExpOnTheInjectivityDomain) {
  std::mt19937_64 rng(3);
  for (double angle : { 1e-12, 1e-6, 1e-4, 0.2, 1.0, 2.5, 3.0 }) {
    for (int t = 0; t < 20; t++) {
      double axis[3] = { test::PortableGaussianDouble(rng), test::PortableGaussianDouble(rng),
                         test::PortableGaussianDouble(rng) };
      const double m = std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
      double w[3];
      for (int i = 0; i < 3; i++) {
        axis[i] /= m;
        w[i] = angle * axis[i];
      }
      Mat r{};
      so3::Exp(w, r.data());
      double back[3];
      so3::Log(r.data(), back);
      for (int i = 0; i < 3; i++) {
        // Near pi the axis is ill-conditioned (not supported, as in LI); 3.0 still inverts to 1e-13.
        EXPECT_NEAR(back[i], w[i], angle > 2.0 ? 1e-13 : 1e-15) << angle;
      }
    }
  }
}

TEST(So3, DistanceKeepsItsPrecisionNearTheIdentity) {
  const Mat identity = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
  for (double angle : { 1e-12, 1e-9, 1e-6, 0.5, 2.0 }) {
    const Mat r = ExpOf(0.0, angle * 0.6, angle * 0.8);
    // acos((tr - 1) / 2) returns 0 for angles below ~1e-8; the atan2 form keeps them.
    EXPECT_NEAR(so3::Distance(identity.data(), r.data()), angle, 1e-15 * (1.0 + angle)) << angle;
    // Left-invariant: d(QA, QB) = d(A, B).
    const Mat q = ExpOf(0.4, -0.2, 1.1);
    Mat qi{};
    Mat qr{};
    so3::MatMul(q.data(), identity.data(), qi.data());
    so3::MatMul(q.data(), r.data(), qr.data());
    EXPECT_NEAR(so3::Distance(qi.data(), qr.data()), angle, 2e-15) << angle;
  }
}

TEST(So3, SectionCoordinateIsTheSineAlongTheSeedTangent) {
  const Mat seed = ExpOf(0.3, -0.2, 0.1);
  const double t[3] = { 0.6, 0.0, 0.8 };
  EXPECT_EQ(so3::SectionCoordinate(seed.data(), seed.data(), t), 0.0);
  for (double angle : { -1.0, -0.01, 0.2, 2.9 }) {
    const Mat step = ExpOf(angle * t[0], angle * t[1], angle * t[2]);
    Mat r{};
    so3::MatMul(seed.data(), step.data(), r.data());
    // seed^T R = exp(angle [t]); its skew part is sin(angle) t, and t is a unit vector.
    EXPECT_NEAR(so3::SectionCoordinate(seed.data(), r.data(), t), std::sin(angle), 1e-15) << angle;
  }
}

TEST(So3, TangentBasisIsOrthonormalAndDeterministic) {
  const double dirs[][3] = { { 0, 0, 1 }, { 0.36, -0.48, -0.8 }, { 1, 1, 1 }, { -0.2, 0.9, 0.1 } };
  for (const auto& d0 : dirs) {
    const double m = std::sqrt(d0[0] * d0[0] + d0[1] * d0[1] + d0[2] * d0[2]);
    const double d[3] = { d0[0] / m, d0[1] / m, d0[2] / m };
    double b[2][3];
    so3::TangentBasis(d, b);
    EXPECT_NEAR(so3::Dot3(b[0], b[0]), 1.0, 1e-15);
    EXPECT_NEAR(so3::Dot3(b[1], b[1]), 1.0, 1e-15);
    EXPECT_NEAR(so3::Dot3(b[0], b[1]), 0.0, 1e-15);
    EXPECT_NEAR(so3::Dot3(b[0], d), 0.0, 1e-15);
    EXPECT_NEAR(so3::Dot3(b[1], d), 0.0, 1e-15);
    // (b0, b1, d) is right-handed: b1 = d x b0.
    double c[3];
    so3::Cross3(b[0], b[1], c);
    EXPECT_NEAR(so3::Dot3(c, d), 1.0, 1e-15);
  }
  // e3: the least-aligned axis is e1 (first of the tie e1/e2), so b0 = e3 x e1 = e2, b1 = e3 x e2 = -e1.
  const double e3[3] = { 0, 0, 1 };
  double b[2][3];
  so3::TangentBasis(e3, b);
  EXPECT_EQ(b[0][1], 1.0);
  EXPECT_EQ(b[1][0], -1.0);
}

// A = U diag(s1, s2) V^T with U a plane rotation and V a random rotation (rows 0..1 of V^T span the
// row space); its kernel is V's third column.
TEST(So3, TwoByThreeSingularValuesKeepRelativePrecisionNearTheRankGate) {
  const Mat v = ExpOf(0.7, -1.1, 0.4);
  const double c = std::cos(0.9), s = std::sin(0.9);
  for (double s2 : { 0.5, 1e-4, 1e-8, 1e-10 }) {
    const double s1 = 1.3;
    double rows[2][3];
    for (int j = 0; j < 3; j++) {
      // V^T row k = V column k.
      const double v0 = v[j * 3 + 0], v1 = v[j * 3 + 1];
      rows[0][j] = c * s1 * v0 - s * s2 * v1;
      rows[1][j] = s * s1 * v0 + c * s2 * v1;
    }
    const auto svd = so3::SvdTwoByThree(rows[0], rows[1]);
    EXPECT_NEAR(svd.sigma1, s1, 1e-15);
    // Rounding in A itself perturbs sigma2 by ~eps * s1, whatever the algorithm.
    EXPECT_NEAR(svd.sigma2, s2, 1e-15) << s2;
    const double kernel[3] = { v[2], v[5], v[8] };
    EXPECT_NEAR(std::fabs(so3::Dot3(svd.null_vector, kernel)), 1.0, 1e-12) << s2;
  }
  // Equal and nearly equal singular values: the characteristic polynomial's discriminant cancels
  // there, and a square root of it would move both values by ~1e-8.
  for (double s2 : { 1.3, 1.3 * (1.0 - 1e-9), 1.3 * (1.0 - 1e-6) }) {
    const double s1 = 1.3;
    double rows[2][3];
    for (int j = 0; j < 3; j++) {
      const double v0 = v[j * 3 + 0], v1 = v[j * 3 + 1];
      rows[0][j] = c * s1 * v0 - s * s2 * v1;
      rows[1][j] = s * s1 * v0 + c * s2 * v1;
    }
    const auto svd = so3::SvdTwoByThree(rows[0], rows[1]);
    EXPECT_NEAR(svd.sigma1, s1, 1e-15) << s2;
    EXPECT_NEAR(svd.sigma2, s2, 1e-15) << s2;
  }
  const double zero[3] = { 0, 0, 0 };
  const auto rank0 = so3::SvdTwoByThree(zero, zero);
  EXPECT_EQ(rank0.sigma1, 0.0);
  EXPECT_EQ(rank0.sigma2, 0.0);
}

TEST(So3, ConditionNumberIsTheTwoNormRatio) {
  const Mat u = ExpOf(0.3, 0.2, -0.9);
  const Mat v = ExpOf(-1.2, 0.5, 0.25);
  for (double smin : { 1.0, 1e-4, 1e-8, 1e-9 }) {
    const double sv[3] = { 2.0, 0.7, smin };
    double m[9];
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        m[i * 3 + j] = 0.0;
        for (int k = 0; k < 3; k++) {
          m[i * 3 + j] += u[i * 3 + k] * sv[k] * v[j * 3 + k];
        }
      }
    }
    const double expected = 2.0 / std::fmin(0.7, smin);
    EXPECT_NEAR(so3::ConditionNumber(m) / expected, 1.0, 1e-6) << smin;
  }
  const double singular[9] = { 1, 2, 3, 2, 4, 6, 0, 1, 1 };
  EXPECT_TRUE(std::isinf(so3::ConditionNumber(singular)) || so3::ConditionNumber(singular) > 1e15);
  double with_nan[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
  with_nan[4] = std::nan("");
  EXPECT_TRUE(std::isnan(so3::ConditionNumber(with_nan)));
}

TEST(So3, Solve3RecoversAKnownSolution) {
  const double m[9] = { 0.0, 2.0, 1.0, 1.0, -1.0, 3.0, 4.0, 0.5, -2.0 };  // zero leading pivot
  const double x_true[3] = { 0.3, -1.7, 2.2 };
  double b[3];
  for (int i = 0; i < 3; i++) {
    b[i] = m[i * 3] * x_true[0] + m[i * 3 + 1] * x_true[1] + m[i * 3 + 2] * x_true[2];
  }
  double x[3];
  ASSERT_TRUE(so3::Solve3(m, b, x));
  for (int i = 0; i < 3; i++) {
    EXPECT_NEAR(x[i], x_true[i], 1e-14);
  }
  const double singular[9] = { 1, 2, 3, 2, 4, 6, 0, 0, 0 };
  EXPECT_FALSE(so3::Solve3(singular, b, x));
}

// d/d(delta) of base * exp(delta) through Jet<3>, at delta = 0 (Taylor branch) and away from it
// (regular branch), against a central difference of the double Exp — the derivative the bordered
// Newton system uses.
TEST(So3, JetExpDerivativeMatchesCentralDifference) {
  const Mat base = ExpOf(0.5, -0.3, 0.9);
  for (const auto& d0 : { std::array<double, 3>{ 0, 0, 0 }, std::array<double, 3>{ 0.02, -0.05, 0.01 },
                          std::array<double, 3>{ 0.4, 0.3, -0.6 } }) {
    Jet<3> w[3];
    for (int k = 0; k < 3; k++) {
      w[k] = Jet<3>::Variable(d0[k], k);
    }
    Jet<3> e[9];
    so3::Exp(w, e);
    Jet<3> r[9];
    so3::MatMul(base.data(), e, r);
    for (int k = 0; k < 3; k++) {
      const double h = 1e-6;
      auto rot = [&](double sign) {
        Mat ex = ExpOf(d0[0] + (k == 0 ? sign * h : 0.0), d0[1] + (k == 1 ? sign * h : 0.0),
                       d0[2] + (k == 2 ? sign * h : 0.0));
        Mat out{};
        so3::MatMul(base.data(), ex.data(), out.data());
        return out;
      };
      const Mat plus = rot(1.0);
      const Mat minus = rot(-1.0);
      for (int i = 0; i < 9; i++) {
        EXPECT_NEAR(r[i].v[k], (plus[i] - minus[i]) / (2.0 * h), 1e-9) << "entry " << i << " k " << k;
      }
    }
  }
}

}  // namespace
}  // namespace lumice::analytic
