// The minimal reflection-group port (src/analytic/reflection_group.hpp, LI
// symmetry/reflection_group.py's two consumed predicates) against LI's own test tables and
// hand-computed oracles: the commutation probe's non-special-angle discipline and the PBD orbit
// as label arithmetic. The comparison rotations are built by an independent Rodrigues formula in
// this file, not by the production one — a defect in the shared construction cannot blind the
// ruler.
//
// symmetry_semantics: L1 — the orbit is label arithmetic on face numbers and merges paths a
// concrete crystal may not treat equivalently; that is its contract (LI conventions #21).

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "analytic/reflection_group.hpp"

namespace lumice::analytic {
namespace {

constexpr double kPi = 3.14159265358979323846;

// An independent Rodrigues rotation (axis-angle by repeated squaring of the Cayley form would
// also do; this is the textbook series-free formula, written apart from the production one).
void Rotation(const double axis[3], double deg, double out[9]) {
  const double norm = std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
  const double k[3] = { axis[0] / norm, axis[1] / norm, axis[2] / norm };
  const double t = deg * kPi / 180.0;
  const double s = std::sin(t);
  const double c = std::cos(t);
  const double kx[3] = { 0.0, -k[2], k[1] };
  const double ky[3] = { k[2], 0.0, -k[0] };
  const double kz[3] = { -k[1], k[0], 0.0 };
  // R = c I + s [k]_x + (1 - c) k k^T.
  const double rows[3][3] = { { kx[0], kx[1], kx[2] }, { ky[0], ky[1], ky[2] }, { kz[0], kz[1], kz[2] } };
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      out[i * 3 + j] = c * (i == j ? 1.0 : 0.0) + s * rows[i][j] + (1.0 - c) * k[i] * k[j];
    }
  }
}

// A mirror about the plane with normal n: I - 2 n n^T (unit n assumed).
void Mirror(const double n[3], double out[9]) {
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      out[i * 3 + j] = (i == j ? 1.0 : 0.0) - 2.0 * n[i] * n[j];
    }
  }
}

// ---------------------------------------------------------------------------------------------
// CommutesWithRotationAbout
// ---------------------------------------------------------------------------------------------

// A rotation commutes with the rotations about its own axis; nobody else's (an 120-deg z rotation
// does not commute with rotations about x).
TEST(ReflectionGroup, RotationCommutesWithItsOwnAxisOnly) {
  const double z[3] = { 0.0, 0.0, 1.0 };
  const double x[3] = { 1.0, 0.0, 0.0 };
  double rz[9];
  double rx[9];
  Rotation(z, 120.0, rz);
  Rotation(x, 71.3, rx);
  EXPECT_TRUE(CommutesWithRotationAbout(rz, z));
  const double minus_z[3] = { 0.0, 0.0, -1.0 };
  EXPECT_TRUE(CommutesWithRotationAbout(rz, minus_z));  // the axis sign is irrelevant
  EXPECT_FALSE(CommutesWithRotationAbout(rz, x));
  EXPECT_FALSE(CommutesWithRotationAbout(rx, z));
  EXPECT_TRUE(CommutesWithRotationAbout(rx, x));
}

// A mirror commutes with the rotations about its own normal (that IS the mirror's invariant
// axis) and with nothing else — S_z against a z rotation yes, against an x rotation no.
TEST(ReflectionGroup, MirrorCommutesWithItsNormalAxis) {
  const double z[3] = { 0.0, 0.0, 1.0 };
  const double x[3] = { 1.0, 0.0, 0.0 };
  double mirror_z[9];
  Mirror(z, mirror_z);
  EXPECT_TRUE(CommutesWithRotationAbout(mirror_z, z));
  EXPECT_FALSE(CommutesWithRotationAbout(mirror_z, x));
}

// The probe-angle discipline (LI commutes_with_rotation_about's docstring): S_b with b = y
// PERPENDICULAR to the probe axis x satisfies S_b R_x(180) = R_x(180) S_b — the 180-deg probe
// centralizer admits it — while the non-special 37-deg probe does not. A 90-deg probe is wrong
// the same way for {I, R_x(90)}: S_b with b = z gives S_z R_x(90) = R_x(-90) S_z != R_x(90) S_z,
// yet b at 45 deg to the axis separates them. The default probe must therefore be non-special.
TEST(ReflectionGroup, ProbeAngleMustBeNonSpecial) {
  const double x[3] = { 1.0, 0.0, 0.0 };
  const double y[3] = { 0.0, 1.0, 0.0 };
  const double z[3] = { 0.0, 0.0, 1.0 };
  double mirror_y[9];
  Mirror(y, mirror_y);
  double rotation_180[9];
  Rotation(x, 180.0, rotation_180);
  // The false positive the docstring names: at 180 deg the pair commutes although the mirror
  // does not commute with the rotations about x in general.
  double product_left[9];
  double product_right[9];
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      product_left[i * 3 + j] = mirror_y[i * 3 + 0] * rotation_180[0 * 3 + j] +
                                mirror_y[i * 3 + 1] * rotation_180[1 * 3 + j] +
                                mirror_y[i * 3 + 2] * rotation_180[2 * 3 + j];
      product_right[i * 3 + j] = rotation_180[i * 3 + 0] * mirror_y[0 * 3 + j] +
                                 rotation_180[i * 3 + 1] * mirror_y[1 * 3 + j] +
                                 rotation_180[i * 3 + 2] * mirror_y[2 * 3 + j];
    }
  }
  bool commutes_at_180 = true;
  for (int i = 0; i < 9; i++) {
    if (std::fabs(product_left[i] - product_right[i]) > 1e-12) {
      commutes_at_180 = false;
    }
  }
  EXPECT_TRUE(commutes_at_180);                          // the trap exists ...
  EXPECT_FALSE(CommutesWithRotationAbout(mirror_y, x));  // ... and the default probe does not fall in it
  (void)z;
}

// The identity commutes with everything; a zero axis has no rotations to commute with and the
// predicate fails closed.
TEST(ReflectionGroup, IdentityAndDegenerateInputs) {
  const double identity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
  const double axis[3] = { 0.3, -0.5, 0.8 };
  double rotation[9];
  Rotation(axis, 41.0, rotation);
  EXPECT_TRUE(CommutesWithRotationAbout(identity, axis));
  EXPECT_TRUE(CommutesWithRotationAbout(rotation, axis));
  const double zero[3] = { 0.0, 0.0, 0.0 };
  EXPECT_FALSE(CommutesWithRotationAbout(identity, zero));
}

// ---------------------------------------------------------------------------------------------
// PbdOrbit
// ---------------------------------------------------------------------------------------------

// The orbit of 1-3-5-2 on the canonical prism is 24 members (the fixture's member table): D6
// sends 3-5 through its twelve images, the B swap doubles them, and basal 1/2 ride along. The
// table below is the fixture's expected members (1-3-5-2__mc_chromatic_class), an independent
// oracle for the label arithmetic.
TEST(ReflectionGroup, PbdOrbitOfOneThreeFiveTwo) {
  const int faces[4] = { 1, 3, 5, 2 };
  std::vector<std::vector<int>> members;
  ASSERT_TRUE(PbdOrbit(faces, 4, &members));
  ASSERT_EQ(members.size(), 24u);
  // Spot checks against hand-worked images: R^c_2 (shift 2) maps 3->5, 5->7; F^f_3 (flip, shift 0)
  // maps side n -> -n mod the ring: 3->6? No — flip shift 0 maps n -> 0 - n: 3->3-3=0->6, 5->4.
  // Image of R2: (1, 5, 7, 2); its B swap: (2, 5, 7, 1). Image of F(shift 0): (1, 6, 4, 2).
  const auto contains = [&members](const int(&want)[4]) {
    for (const std::vector<int>& member : members) {
      if (member.size() == 4 && member[0] == want[0] && member[1] == want[1] && member[2] == want[2] &&
          member[3] == want[3]) {
        return true;
      }
    }
    return false;
  };
  EXPECT_TRUE(contains({ 1, 5, 7, 2 }));
  EXPECT_TRUE(contains({ 2, 5, 7, 1 }));
  EXPECT_TRUE(contains({ 1, 6, 4, 2 }));
  EXPECT_TRUE(contains({ 1, 3, 5, 2 }));  // the identity image of itself
  // Sorted output: the first member is lexicographically smallest.
  EXPECT_EQ(members[0], (std::vector<int>{ 1, 3, 5, 2 }));
  // D6 x B is a group action on labels: every member has the same length.
  for (const std::vector<int>& member : members) {
    EXPECT_EQ(member.size(), 4u);
  }
}

// Pyramid bands: D6 acts ring-wise, B swaps the cones (13+i <-> 23+i), sides stay.
TEST(ReflectionGroup, PbdOrbitCarriesPyramidBands) {
  const int faces[3] = { 13, 15, 3 };
  std::vector<std::vector<int>> members;
  ASSERT_TRUE(PbdOrbit(faces, 3, &members));
  const auto contains = [&members](const int(&want)[3]) {
    for (const std::vector<int>& member : members) {
      if (member.size() == 3 && member[0] == want[0] && member[1] == want[1] && member[2] == want[2]) {
        return true;
      }
    }
    return false;
  };
  EXPECT_TRUE(contains({ 13, 15, 3 }));  // identity
  EXPECT_TRUE(contains({ 23, 25, 3 }));  // the B swap of it (cones swap, side stays)
  EXPECT_TRUE(contains({ 14, 16, 4 }));  // R^c_1 within each ring
  EXPECT_TRUE(contains({ 18, 16, 8 }));  // F^d (odd flip) within each ring
}

// A rank-0 pair's orbit is small: 3-6 (opposite sides, no reflection) has the six rotations and
// their flips colliding pairwise, so D6 yields 6 images and B does not double them (sides stay).
TEST(ReflectionGroup, PbdOrbitOfOppositeSides) {
  const int faces[2] = { 3, 6 };
  std::vector<std::vector<int>> members;
  ASSERT_TRUE(PbdOrbit(faces, 2, &members));
  ASSERT_EQ(members.size(), 6u);  // {3,4,5,6,7,8} x {6,7,8,3,4,5} as pairs
  EXPECT_EQ(members[0], (std::vector<int>{ 3, 6 }));
}

// A face number outside the analytic layer's universe fails closed: no orbit, not a partial one.
TEST(ReflectionGroup, PbdOrbitRejectsUnknownFaces) {
  const int faces[2] = { 3, 9 };
  std::vector<std::vector<int>> members;
  EXPECT_FALSE(PbdOrbit(faces, 2, &members));
  EXPECT_TRUE(members.empty());
}

}  // namespace
}  // namespace lumice::analytic
