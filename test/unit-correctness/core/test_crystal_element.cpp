// The element algebra's three views (core/crystal.hpp CrystalElement), each held to a ruler
// restated in this file rather than to any production helper:
//   * the LABEL view against the element's own matrix acting on restated face normals — the
//     permutation and the matrix cannot drift apart because both sides of that comparison are
//     independent constructions;
//   * the MATRIX view against group identities (involutions, generator products, determinants);
//   * the POSE view against the closed-form angle maps spelled here, at matrix level — the form
//     the plan requires for the pole cases (the angle maps degenerate only as a chart; the
//     matrix identity must hold at the poles too);
//   * the SKY view against the sun's vertical plane restated (proper elements fix every sky
//     direction; improper ones fix exactly the sun's vertical plane).
// The pose convention restated here is the engine's: row-major body -> world,
// R = Rz(azimuth - 180) Ry(-zenith) Rz(roll) (single_path_analysis.hpp's contract).

#include <gtest/gtest.h>

#include <cmath>
#include <set>
#include <vector>

#include "core/crystal.hpp"

namespace lumice {
namespace {

constexpr double kPiD = 3.14159265358979323846;
constexpr double kDegToRadD = kPiD / 180.0;

// ---- independent 3x3 construction (never the production helpers under test) -----------------

void MatMul(const double a[9], const double b[9], double out[9]) {
  double result[9];
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      result[i * 3 + j] = a[i * 3 + 0] * b[0 * 3 + j] + a[i * 3 + 1] * b[1 * 3 + j] + a[i * 3 + 2] * b[2 * 3 + j];
    }
  }
  for (int k = 0; k < 9; ++k) {
    out[k] = result[k];
  }
}

void MatVec(const double m[9], const double v[3], double out[3]) {
  for (int i = 0; i < 3; ++i) {
    out[i] = m[i * 3 + 0] * v[0] + m[i * 3 + 1] * v[1] + m[i * 3 + 2] * v[2];
  }
}

double Determinant(const double m[9]) {
  return m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6]) + m[2] * (m[3] * m[7] - m[4] * m[6]);
}

void Transpose(const double a[9], double out[9]) {
  out[0] = a[0];
  out[1] = a[3];
  out[2] = a[6];
  out[3] = a[1];
  out[4] = a[4];
  out[5] = a[7];
  out[6] = a[2];
  out[7] = a[5];
  out[8] = a[8];
}

double RzMatrix[6][9];  // Rz(k*60 deg), k = 0..5, built once below

void BuildRzTable() {
  static const bool built = [] {
    for (int k = 0; k < 6; ++k) {
      const double a = k * 60.0 * kDegToRadD;
      const double c = std::cos(a);
      const double s = std::sin(a);
      RzMatrix[k][0] = c;
      RzMatrix[k][1] = -s;
      RzMatrix[k][2] = 0.0;
      RzMatrix[k][3] = s;
      RzMatrix[k][4] = c;
      RzMatrix[k][5] = 0.0;
      RzMatrix[k][6] = 0.0;
      RzMatrix[k][7] = 0.0;
      RzMatrix[k][8] = 1.0;
    }
    return true;
  }();
  (void)built;
}

// The engine's pose convention, restated: row-major body -> world, degrees.
void EulerPoseDeg(double az_deg, double zen_deg, double roll_deg, double out[9]) {
  BuildRzTable();
  const double zen = zen_deg * kDegToRadD;
  const double ry[9] = { std::cos(zen), 0.0, -std::sin(zen), 0.0, 1.0, 0.0, std::sin(zen), 0.0, std::cos(zen) };
  // Rz(az - 180) via the table: shift by 180 deg = three 60-deg steps.
  const double rz_az[9] = { std::cos((az_deg - 180.0) * kDegToRadD),
                            -std::sin((az_deg - 180.0) * kDegToRadD),
                            0.0,
                            std::sin((az_deg - 180.0) * kDegToRadD),
                            std::cos((az_deg - 180.0) * kDegToRadD),
                            0.0,
                            0.0,
                            0.0,
                            1.0 };
  const double rz_roll[9] = { std::cos(roll_deg * kDegToRadD),
                              -std::sin(roll_deg * kDegToRadD),
                              0.0,
                              std::sin(roll_deg * kDegToRadD),
                              std::cos(roll_deg * kDegToRadD),
                              0.0,
                              0.0,
                              0.0,
                              1.0 };
  double left[9];
  MatMul(rz_az, ry, left);
  MatMul(left, rz_roll, out);
}

// The world vertical mirror whose plane contains the sun at `azimuth_deg`, restated.
void SunVerticalMirrorDeg(double azimuth_deg, double out[9]) {
  const double a = 2.0 * azimuth_deg * kDegToRadD;
  const double c = std::cos(a);
  const double s = std::sin(a);
  out[0] = c;
  out[1] = s;
  out[2] = 0.0;
  out[3] = s;
  out[4] = -c;
  out[5] = 0.0;
  out[6] = 0.0;
  out[7] = 0.0;
  out[8] = 1.0;
}

// Restated face normals from the numbering convention (face 3+i at azimuth i*60 degrees, basal
// at ±z, cone faces tilted `wedge` degrees off the horizontal in their ring's direction).
void FaceNormalDeg(int fn, double wedge_deg, double out[3]) {
  if (fn == 1) {
    out[0] = 0.0;
    out[1] = 0.0;
    out[2] = 1.0;
    return;
  }
  if (fn == 2) {
    out[0] = 0.0;
    out[1] = 0.0;
    out[2] = -1.0;
    return;
  }
  const int i = (fn % 10) - 3;
  const double az = i * 60.0 * kDegToRadD;
  if (fn < 10) {
    out[0] = std::cos(az);
    out[1] = std::sin(az);
    out[2] = 0.0;
    return;
  }
  const double w = wedge_deg * kDegToRadD;
  const double up = fn < 20 ? 1.0 : -1.0;
  out[0] = std::cos(w) * std::cos(az);
  out[1] = std::cos(w) * std::sin(az);
  out[2] = up * std::sin(w);
}

void ExpectMatricesClose(const double actual[9], const double expected[9], double tol) {
  for (int k = 0; k < 9; ++k) {
    if (std::fabs(actual[k] - expected[k]) > tol) {
      ADD_FAILURE() << "matrix entry " << k << ": " << actual[k] << " vs " << expected[k];
    }
  }
}

CrystalElement MakeElement(CrystalElement::Family family, int index) {
  return { family, index };
}

const std::vector<int>& LegalFaces() {
  static const std::vector<int> faces = [] {
    std::vector<int> f{ 1, 2 };
    for (int i = 3; i <= 8; ++i) {
      f.push_back(i);
    }
    for (int i = 13; i <= 18; ++i) {
      f.push_back(i);
    }
    for (int i = 23; i <= 28; ++i) {
      f.push_back(i);
    }
    return f;
  }();
  return faces;
}

// ---- the label view -------------------------------------------------------------------------

TEST(CrystalElement, LabelViewMatchesTheMatrixOnEveryFace) {
  constexpr double kWedge = 60.0;  // symmetric cones, so the cone rings are exact B partners
  const auto elements = AllCrystalElements();
  ASSERT_EQ(elements.size(), 24u);
  for (const CrystalElement& element : elements) {
    double g[9];
    element.BodyMatrix(g);
    for (int face : LegalFaces()) {
      double normal[3];
      FaceNormalDeg(face, kWedge, normal);
      double imaged[3];
      MatVec(g, normal, imaged);
      const IdType transported = element.ApplyToFace(static_cast<IdType>(face));
      double expected[3];
      FaceNormalDeg(static_cast<int>(transported), kWedge, expected);
      for (int k = 0; k < 3; ++k) {
        if (std::fabs(imaged[k] - expected[k]) > 1e-9) {
          ADD_FAILURE() << "component " << k << ": family " << static_cast<int>(element.family) << " index "
                        << element.index << " face " << face << ": " << imaged[k] << " vs " << expected[k];
        }
      }
    }
  }
}

TEST(CrystalElement, ApplyToFaceRejectsIllegalFacesByAborting) {
  // The precondition's guard: a face outside the legal universe must not silently pass through
  // (a silently-passed face would poison every orbit built on top). Death-test form because the
  // guard is an unrecoverable invariant, the engine's FatalAbort.
  EXPECT_DEATH(
      {
        CrystalElement e{};
        e.ApplyToFace(9);
      },
      "");
}

// ---- the matrix view ------------------------------------------------------------------------

TEST(CrystalElement, InvolutionsAndProductsCloseTheGroup) {
  BuildRzTable();
  const auto elements = AllCrystalElements();
  ASSERT_EQ(elements.size(), 24u);
  std::set<std::pair<int, int>> seen;
  for (const CrystalElement& element : elements) {
    auto key = std::make_pair(static_cast<int>(element.family), element.index);
    EXPECT_TRUE(seen.insert(key).second) << "duplicate element family " << key.first << " index " << key.second;
    double g[9];
    element.BodyMatrix(g);
    const double det = Determinant(g);
    if (element.IsProper()) {
      EXPECT_NEAR(det, 1.0, 1e-12) << "proper element family " << static_cast<int>(element.family);
    } else {
      EXPECT_NEAR(det, -1.0, 1e-12) << "improper element family " << static_cast<int>(element.family);
    }
    double gt[9];
    Transpose(g, gt);
    double gt_g[9];
    MatMul(gt, g, gt_g);
    double identity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
    ExpectMatricesClose(gt_g, identity, 1e-12);  // orthogonal
  }
  // Involutions: H_a, sigma_a, S_3 (the inversion), and every two-step composition squares to
  // the identity in the label view too.
  for (int a = 0; a < 6; ++a) {
    for (const CrystalElement::Family family :
         { CrystalElement::Family::kHorizontalHalfTurn, CrystalElement::Family::kVerticalMirror }) {
      double g[9];
      MakeElement(family, a).BodyMatrix(g);
      double gg[9];
      MatMul(g, g, gg);
      double identity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
      ExpectMatricesClose(gg, identity, 1e-12);
    }
    for (int face : LegalFaces()) {
      const IdType once =
          MakeElement(CrystalElement::Family::kHorizontalHalfTurn, a).ApplyToFace(static_cast<IdType>(face));
      EXPECT_EQ(MakeElement(CrystalElement::Family::kHorizontalHalfTurn, a).ApplyToFace(once),
                static_cast<IdType>(face));
    }
  }
  // P_k · P_l = P_{k+l}, in matrices and in labels.
  for (int k = 0; k < 6; ++k) {
    for (int l = 0; l < 6; ++l) {
      double pk[9];
      double pl[9];
      MakeElement(CrystalElement::Family::kCAxisRotation, k).BodyMatrix(pk);
      MakeElement(CrystalElement::Family::kCAxisRotation, l).BodyMatrix(pl);
      double product[9];
      MatMul(pk, pl, product);
      ExpectMatricesClose(product, RzMatrix[(k + l) % 6], 1e-12);
    }
  }
  // H_a = B · sigma_a as matrices (B is the roto-mirror with index 0).
  double b[9];
  double sigma[9];
  MakeElement(CrystalElement::Family::kRotoMirror, 0).BodyMatrix(b);
  MakeElement(CrystalElement::Family::kVerticalMirror, 3).BodyMatrix(sigma);
  double bs[9];
  MatMul(b, sigma, bs);
  double h3[9];
  MakeElement(CrystalElement::Family::kHorizontalHalfTurn, 3).BodyMatrix(h3);
  ExpectMatricesClose(bs, h3, 1e-12);
  // S_k = S_0 · P_k as matrices.
  for (int k = 0; k < 6; ++k) {
    double s0[9];
    double pk[9];
    MakeElement(CrystalElement::Family::kRotoMirror, 0).BodyMatrix(s0);
    MakeElement(CrystalElement::Family::kCAxisRotation, k).BodyMatrix(pk);
    double product[9];
    MatMul(s0, pk, product);
    double sk[9];
    MakeElement(CrystalElement::Family::kRotoMirror, k).BodyMatrix(sk);
    ExpectMatricesClose(product, sk, 1e-12);
  }
}

TEST(CrystalElement, InventoryIsTheTwentyFourElements) {
  const auto elements = AllCrystalElements();
  ASSERT_EQ(elements.size(), 24u);
  int counts[5] = { 0, 0, 0, 0, 0 };
  for (const CrystalElement& element : elements) {
    ++counts[static_cast<int>(element.family)];
    EXPECT_GE(element.index, 0);
    EXPECT_LE(element.index, 5);
  }
  EXPECT_EQ(counts[static_cast<int>(CrystalElement::Family::kIdentity)], 1);
  EXPECT_EQ(counts[static_cast<int>(CrystalElement::Family::kCAxisRotation)], 5);
  EXPECT_EQ(counts[static_cast<int>(CrystalElement::Family::kHorizontalHalfTurn)], 6);
  EXPECT_EQ(counts[static_cast<int>(CrystalElement::Family::kVerticalMirror)], 6);
  EXPECT_EQ(counts[static_cast<int>(CrystalElement::Family::kRotoMirror)], 6);
}

// ---- the pose view --------------------------------------------------------------------------

// The closed-form angle maps (mod 360 degrees), per family — the derivation's statements at
// chart level. The test compares the mapped triple's MATRIX to CompensatedPose's output, which
// is the form the plan requires: the identity must hold at the poles too, where the chart is
// degenerate but the group action is not.
void ExpectedCompensatedAngles(const CrystalElement& e, double az, double zen, double roll, double sun_az,
                               double* out_az, double* out_zen, double* out_roll) {
  using Family = CrystalElement::Family;
  *out_zen = zen;
  *out_az = az;
  *out_roll = roll;
  switch (e.family) {
    case Family::kIdentity:
      break;
    case Family::kCAxisRotation:
      *out_roll = roll - 60.0 * e.index;
      break;
    case Family::kHorizontalHalfTurn:
      *out_az = az + 180.0;
      *out_zen = 180.0 - zen;
      *out_roll = -roll - 60.0 * e.index;
      break;
    case Family::kVerticalMirror:
      *out_az = 2.0 * sun_az - az;
      *out_roll = -roll - 60.0 * e.index;
      break;
    case Family::kRotoMirror:
      *out_az = 2.0 * sun_az - az + 180.0;
      *out_zen = 180.0 - zen;
      *out_roll = roll - 60.0 * e.index;
      break;
  }
}

TEST(CrystalElement, PoseCompensationMatchesTheClosedFormAngleMaps) {
  struct PoseDeg {
    double az;
    double zen;
    double roll;
  };
  // Generic poses plus both zenith poles (the chart-degenerate cases the matrix form owns).
  const PoseDeg poses[] = {
    { 37.3, 64.9, -21.7 }, { 0.0, 90.0, 0.0 }, { -152.5, 0.0, 31.4 }, { 200.5, 180.0, -90.0 }, { 12.0, 89.43, -0.37 },
  };
  const double sun_azimuths[] = { 0.0, 30.0, 123.4 };
  for (const CrystalElement& element : AllCrystalElements()) {
    double g[9];
    element.BodyMatrix(g);
    double gt[9];
    Transpose(g, gt);
    for (const PoseDeg& pose : poses) {
      double r[9];
      EulerPoseDeg(pose.az, pose.zen, pose.roll, r);
      for (double sun_az : sun_azimuths) {
        double compensated[9];
        CompensatedPose(element, r, sun_az * kDegToRadD, compensated);
        // Matrix-level ground truth, built independently: R' = M·R·gᵀ (M = I for proper).
        double target[9];
        if (element.IsProper()) {
          MatMul(r, gt, target);
        } else {
          double m[9];
          SunVerticalMirrorDeg(sun_az, m);
          double mr[9];
          MatMul(m, r, mr);
          MatMul(mr, gt, target);
        }
        ExpectMatricesClose(compensated, target, 1e-12);
        // The closed-form angle maps, at matrix level (holds at the poles too).
        double az_p = 0.0;
        double zen_p = 0.0;
        double roll_p = 0.0;
        ExpectedCompensatedAngles(element, pose.az, pose.zen, pose.roll, sun_az, &az_p, &zen_p, &roll_p);
        double mapped[9];
        EulerPoseDeg(az_p, zen_p, roll_p, mapped);
        ExpectMatricesClose(compensated, mapped, 1e-12);
      }
    }
  }
}

// ---- the sky view ---------------------------------------------------------------------------

void SkyDirectionDeg(double altitude_deg, double azimuth_deg, double out[3]) {
  const double alt = altitude_deg * kDegToRadD;
  const double az = azimuth_deg * kDegToRadD;
  out[0] = std::cos(alt) * std::cos(az);
  out[1] = std::cos(alt) * std::sin(az);
  out[2] = std::sin(alt);
}

TEST(CrystalElement, ProperElementsFixEverySkyDirection) {
  const double dirs[][2] = { { 20.0, 52.0 },   { 20.0, -52.0 }, { 36.0, 120.0 },
                             { 36.0, -120.0 }, { 0.0, 0.0 },    { -35.0, 217.0 } };
  for (const CrystalElement& element : AllCrystalElements()) {
    if (!element.IsProper()) {
      continue;
    }
    for (const auto& dir : dirs) {
      double d[3];
      SkyDirectionDeg(dir[0], dir[1], d);
      EXPECT_TRUE(ElementFixesSkyDirection(element, d, 0.0)) << "family " << static_cast<int>(element.family);
    }
  }
}

TEST(CrystalElement, ImproperElementsFixExactlyTheSunVerticalPlane) {
  const double sun_az = 0.0;  // the Parry/C12 discussions' sun azimuth
  // Fixed: the sun's azimuth, its antipode (the exact, negative, and wrapped spellings), and the
  // two poles at ±90 altitude (a pole lies in every vertical plane).
  const double fixed_dirs[][2] = { { 36.0, 0.0 },  { 36.0, 180.0 }, { 36.0, -180.0 }, { 20.0, 360.0 },
                                   { 90.0, 17.0 }, { -90.0, 71.0 }, { 0.0, 0.0 },     { 20.0, 0.0 } };
  // Moved: the C12 sides (±120), the Parry sides (±52), a quarter turn, and a near-pole
  // direction that is NOT the pole (altitude 89.999 keeps cos(zenith) ~ 3e-5, far above the band).
  const double moved_dirs[][2] = { { 36.0, 120.0 }, { 36.0, -120.0 }, { 20.0, 52.0 },   { 20.0, -52.0 },
                                   { 10.0, 90.0 },  { 0.0, 17.0 },    { 89.999, 90.0 }, { 30.0, -71.0 } };
  for (const CrystalElement& element : AllCrystalElements()) {
    if (element.IsProper()) {
      continue;
    }
    for (const auto& dir : fixed_dirs) {
      double d[3];
      SkyDirectionDeg(dir[0], dir[1], d);
      EXPECT_TRUE(ElementFixesSkyDirection(element, d, sun_az))
          << "family " << static_cast<int>(element.family) << " index " << element.index << " dir " << dir[0] << "/"
          << dir[1];
    }
    for (const auto& dir : moved_dirs) {
      double d[3];
      SkyDirectionDeg(dir[0], dir[1], d);
      EXPECT_FALSE(ElementFixesSkyDirection(element, d, sun_az))
          << "family " << static_cast<int>(element.family) << " index " << element.index << " dir " << dir[0] << "/"
          << dir[1];
    }
  }
  // A non-zero sun azimuth rotates the plane with it (radians at the boundary, degrees here).
  const double sun_az2 = 30.0;
  double in_plane[3];
  SkyDirectionDeg(36.0, 30.0 + 180.0, in_plane);
  double out_of_plane[3];
  SkyDirectionDeg(36.0, 30.0 + 120.0, out_of_plane);
  for (const CrystalElement& element : AllCrystalElements()) {
    if (element.IsProper()) {
      continue;
    }
    EXPECT_TRUE(ElementFixesSkyDirection(element, in_plane, sun_az2 * kDegToRadD));
    EXPECT_FALSE(ElementFixesSkyDirection(element, out_of_plane, sun_az2 * kDegToRadD));
  }
}

}  // namespace
}  // namespace lumice
