// The element algebra's OPTICAL views on the production single-path pieces (analytic kernel
// only — no discovery, no sampling statistics): the deviation grid's orbit identity
// D_{gP}(g·u) = D_P(u) over the 24×48 Parry grid with the members 4-2 and 8-1 (the half-turn
// H_0 pair), and the world-ray compensation witnesses for H_0 (proper: the sibling member
// realizes the SAME world ray at the SAME sky point) and for σ_0 (improper: the sibling realizes
// the MIRRORED world ray). The compensated poses in the witnesses are built here by exact
// sign-flip composition — the witness tests the physics identity, not the CompensatedPose
// helper; the helper's matrix contract is pinned in test/unit-correctness/core.
//
// Exactness: the prism's face-normal table lives on ±1, ±1/2, ±√3/2 (the closed-form constants),
// so the H_0 action (x, y, z) → (x, −y, −z) and the σ_0 action (x, y, z) → (x, −y, z) flip
// exact signs; every dot product in the chain flips term by term with the summation order
// unchanged, which is why the grid identity and these witnesses assert bit equality (the
// elements with a 30-degree phase — H_a, σ_a for a ≠ 0 — carry √3/2 into their matrices and are
// held to the 1e-12 kinematic band by the core matrix tests instead).

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <random>
#include <vector>

#include "analytic/entry_measure.hpp"
#include "analytic/path_chain.hpp"
#include "analytic/path_evaluation.hpp"
#include "core/crystal.hpp"
#include "core/optics.hpp"
#include "support/portable_random.hpp"

namespace lumice {
namespace {

constexpr double kPiD = 3.14159265358979323846;
constexpr double kDegToRadD = kPiD / 180.0;

LUMICE_ANALYTIC_Crystal RegularPrism() {
  LUMICE_ANALYTIC_Crystal c{};
  c.kind = LUMICE_ANALYTIC_CRYSTAL_PRISM;
  c.height = 1.0;
  for (double& d : c.face_distance) {
    d = 1.0;
  }
  return c;
}

// The sun direction AT elevation/azimuth; the propagation incident direction is its negation.
void SunDirectionDeg(double altitude_deg, double azimuth_deg, double out[3]) {
  const double alt = altitude_deg * kDegToRadD;
  const double az = azimuth_deg * kDegToRadD;
  out[0] = std::cos(alt) * std::cos(az);
  out[1] = std::cos(alt) * std::sin(az);
  out[2] = std::sin(alt);
}

// The deviation grid, at the point query's own cell-centre layout (SunSphereGrid's stated
// convention): latitude (elevation) rows from −90, longitude columns from −180, u the body-frame
// sun direction, D via TracePathChain at the identity pose (the one definition the grid reads).
struct GridProbe {
  analytic::FaceNormalTable table;
  std::vector<int> slots;
  double refractive_index = 0.0;
  int lat_count = 0;
  int lon_count = 0;
  std::vector<double> deviation;
  std::vector<uint8_t> valid;
};

GridProbe ProbeGrid(const analytic::FaceNormalTable& table, const std::vector<int>& face_sequence, double n,
                    int lat_count) {
  GridProbe probe;
  probe.table = table;
  probe.slots.resize(face_sequence.size());
  EXPECT_EQ(analytic::ResolveFaceSequence(table, face_sequence.data(), static_cast<int>(face_sequence.size()),
                                          probe.slots.data()),
            analytic::Status::kOk);
  probe.refractive_index = n;
  probe.lat_count = lat_count;
  probe.lon_count = 2 * lat_count;
  const int cells = probe.lat_count * probe.lon_count;
  probe.deviation.assign(cells, std::numeric_limits<double>::quiet_NaN());
  probe.valid.assign(cells, 0);
  constexpr double kIdentity[9] = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };
  for (int i = 0; i < probe.lat_count; ++i) {
    for (int j = 0; j < probe.lon_count; ++j) {
      const double lat = (-90.0 + (i + 0.5) * 180.0 / probe.lat_count) * kDegToRadD;
      const double lon = (-180.0 + (j + 0.5) * 360.0 / probe.lon_count) * kDegToRadD;
      const double u[3] = { std::cos(lat) * std::cos(lon), std::cos(lat) * std::sin(lon), std::sin(lat) };
      const double incident[3] = { -u[0], -u[1], -u[2] };
      double outgoing[3];
      if (!analytic::TracePathChain<double>(table, probe.slots.data(), static_cast<int>(probe.slots.size()), n,
                                            incident, kIdentity, outgoing, nullptr, nullptr)) {
        continue;
      }
      const double dot = outgoing[0] * incident[0] + outgoing[1] * incident[1] + outgoing[2] * incident[2];
      const int cell = i * probe.lon_count + j;
      probe.valid[cell] = 1;
      probe.deviation[cell] = std::acos(std::clamp(dot, -1.0, 1.0));
    }
  }
  return probe;
}

TEST(ElementOpticalViews, DeviationGridOrbitIdentityIsBitExact) {
  analytic::FaceNormalTable table;
  analytic::FacePolygonTable polygons;
  ASSERT_EQ(analytic::BuildFaceNormals(RegularPrism(), &table, &polygons), analytic::Status::kOk);
  const double n = IceRefractiveIndex::Get(550.0f);
  // The Parry −52° pair: 4-2 and its H_0 image 8-1 (prism 4 ↔ 8 under the half-turn's prism
  // reflection, basal 2 ↔ 1 under its basal swap).
  GridProbe four_two = ProbeGrid(table, { 4, 2 }, n, 24);
  GridProbe eight_one = ProbeGrid(table, { 8, 1 }, n, 24);
  ASSERT_EQ(four_two.lat_count, eight_one.lat_count);
  // The valid domain is non-trivial: at this resolution the 4-2 path's valid cells are a finite,
  // non-empty set (the讨论 evidence's measured count at this grid and index; the value pins the
  // domain so an optics change that silently empties the grid cannot read as a vacuous pass).
  int valid_count = 0;
  for (uint8_t v : four_two.valid) {
    valid_count += v;
  }
  EXPECT_GT(valid_count, 0);
  EXPECT_EQ(valid_count, 96);
  // The orbit identity, aligned by the H_0 action on the direction: (lat, lon) → (−lat, −lon),
  // which at this layout is the cell index map (i, j) → (lat_count−1−i, lon_count−1−j). Masks
  // equal, every valid deviation bit-equal — the members are one physical family and the grid
  // arithmetic mirrors term by term.
  int compared = 0;
  int mismatched_unaligned = 0;
  for (int i = 0; i < four_two.lat_count; ++i) {
    for (int j = 0; j < four_two.lon_count; ++j) {
      const int cell = i * four_two.lon_count + j;
      const int image = (four_two.lat_count - 1 - i) * four_two.lon_count + (four_two.lon_count - 1 - j);
      if (four_two.valid[cell] != eight_one.valid[image]) {
        ADD_FAILURE() << "validity mask mismatch at cell (" << i << ", " << j << ")";
        continue;
      }
      if (!four_two.valid[cell]) {
        continue;  // both invalid: the stored deviation is NaN, nothing to compare
      }
      EXPECT_EQ(four_two.deviation[cell], eight_one.deviation[image])
          << "deviation mismatch at cell (" << i << ", " << j << ")";
      ++compared;
      // Negative control (detection power): the UNALIGNED pairing must disagree somewhere, or
      // this oracle could not tell the alignment from its absence.
      if (four_two.deviation[cell] != eight_one.deviation[cell]) {
        ++mismatched_unaligned;
      }
    }
  }
  EXPECT_GT(compared, 0);
  EXPECT_GT(mismatched_unaligned, 0);
}

// A uniform random rotation (row-major) from a unit quaternion — the same portable construction
// test_path_evaluation.cpp uses, so the pose search is deterministic on every standard library.
std::array<double, 9> RandomRotation(std::mt19937_64& rng) {
  double q[4] = { test::PortableGaussianDouble(rng), test::PortableGaussianDouble(rng),
                  test::PortableGaussianDouble(rng), test::PortableGaussianDouble(rng) };
  const double m = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
  for (double& v : q) {
    v /= m;
  }
  const double w = q[0], x = q[1], y = q[2], z = q[3];
  return { 1 - 2 * (y * y + z * z), 2 * (x * y - w * z),     2 * (x * z + w * y),
           2 * (x * y + w * z),     1 - 2 * (x * x + z * z), 2 * (y * z - w * x),
           2 * (x * z - w * y),     2 * (y * z + w * x),     1 - 2 * (x * x + y * y) };
}

// The first pose (fixed-seed search) at which the face sequence's chain is valid: the valid-pose
// set of one member is a thin manifold, so a fixed spread misses it — the search is the existing
// FindPose pattern, not a sampled statistic (the pose feeds an exact identity below).
bool FindWitnessPose(const analytic::FaceNormalTable& table, const std::vector<int>& slots, double n,
                     const double incident[3], double pose[9]) {
  std::mt19937_64 rng(20261123);
  for (int t = 0; t < 50000; ++t) {
    auto candidate = RandomRotation(rng);
    double outgoing[3];
    if (analytic::TracePathChain<double>(table, slots.data(), static_cast<int>(slots.size()), n, incident,
                                         candidate.data(), outgoing, nullptr, nullptr)) {
      for (int k = 0; k < 9; ++k) {
        pose[k] = candidate[k];
      }
      return true;
    }
  }
  return false;
}

TEST(ElementOpticalViews, HorizontalHalfTurnWitnessIsBitIdentical) {
  analytic::FaceNormalTable table;
  analytic::FacePolygonTable polygons;
  ASSERT_EQ(analytic::BuildFaceNormals(RegularPrism(), &table, &polygons), analytic::Status::kOk);
  const double n = IceRefractiveIndex::Get(550.0f);
  // Sun at elevation 20, azimuth 0 (the Parry discussions' sun); the incident light travels
  // opposite the sun direction. The pose family is the Parry column's (zenith near 90).
  double sun[3];
  SunDirectionDeg(20.0, 0.0, sun);
  const double incident[3] = { -sun[0], -sun[1], -sun[2] };

  std::vector<int> slots_four_two(2);
  std::vector<int> slots_eight_one(2);
  const int faces_four_two[2] = { 4, 2 };
  const int faces_eight_one[2] = { 8, 1 };
  ASSERT_EQ(analytic::ResolveFaceSequence(table, faces_four_two, 2, slots_four_two.data()), analytic::Status::kOk);
  ASSERT_EQ(analytic::ResolveFaceSequence(table, faces_eight_one, 2, slots_eight_one.data()), analytic::Status::kOk);

  double pose[9];
  ASSERT_TRUE(FindWitnessPose(table, slots_four_two, n, incident, pose)) << "no valid 4-2 pose in the spread";
  // The compensated pose for H_0, built exactly: R·Rx(π) negates the pose's y and z columns
  // (Rx(π) = diag(1, −1, −1) right-multiplied), the exact sign-flip composition.
  double turned[9];
  for (int row = 0; row < 3; ++row) {
    turned[row * 3 + 0] = pose[row * 3 + 0];
    turned[row * 3 + 1] = -pose[row * 3 + 1];
    turned[row * 3 + 2] = -pose[row * 3 + 2];
  }
  double out_a[3];
  double out_b[3];
  const bool valid_a =
      analytic::TracePathChain<double>(table, slots_four_two.data(), 2, n, incident, pose, out_a, nullptr, nullptr);
  const bool valid_b =
      analytic::TracePathChain<double>(table, slots_eight_one.data(), 2, n, incident, turned, out_b, nullptr, nullptr);
  // The sibling must be valid exactly where the original is: the world geometry does not change.
  ASSERT_EQ(valid_a, valid_b);
  ASSERT_TRUE(valid_a);
  for (int k = 0; k < 3; ++k) {
    EXPECT_EQ(out_a[k], out_b[k]) << "outgoing component " << k;
  }
  // The same witness on the full PathOutputs (per-face interface factors ride along bit-equal).
  double segments_a[9];
  double interfaces_a[2];
  double segments_b[9];
  double interfaces_b[2];
  analytic::PathOutputs outputs_a{ {}, 0.0, segments_a, interfaces_a };
  analytic::PathOutputs outputs_b{ {}, 0.0, segments_b, interfaces_b };
  ASSERT_TRUE(analytic::EvaluatePath(table, slots_four_two.data(), 2, n, incident, pose, &outputs_a));
  ASSERT_TRUE(analytic::EvaluatePath(table, slots_eight_one.data(), 2, n, incident, turned, &outputs_b));
  EXPECT_EQ(outputs_a.fresnel_transmission, outputs_b.fresnel_transmission);
  for (int k = 0; k < 2; ++k) {
    EXPECT_EQ(interfaces_a[k], interfaces_b[k]) << "interface factor " << k;
  }
}

TEST(ElementOpticalViews, VerticalMirrorWitnessMirrorsTheWorldRay) {
  analytic::FaceNormalTable table;
  analytic::FacePolygonTable polygons;
  ASSERT_EQ(analytic::BuildFaceNormals(RegularPrism(), &table, &polygons), analytic::Status::kOk);
  const double n = IceRefractiveIndex::Get(550.0f);
  // The incident light must lie in the mirror plane for the mirror to fix it: sun azimuth 0.
  double sun[3];
  SunDirectionDeg(20.0, 0.0, sun);
  const double incident[3] = { -sun[0], -sun[1], -sun[2] };

  std::vector<int> slots_four_one(2);
  std::vector<int> slots_eight_one(2);
  const int faces_four_one[2] = { 4, 1 };
  const int faces_eight_one[2] = { 8, 1 };
  ASSERT_EQ(analytic::ResolveFaceSequence(table, faces_four_one, 2, slots_four_one.data()), analytic::Status::kOk);
  ASSERT_EQ(analytic::ResolveFaceSequence(table, faces_eight_one, 2, slots_eight_one.data()), analytic::Status::kOk);

  double pose[9];
  ASSERT_TRUE(FindWitnessPose(table, slots_four_one, n, incident, pose)) << "no valid 4-1 pose in the spread";
  // The compensated pose for σ_0, built exactly: M·R·σ_0 with M = σ_0 = F_y — negate the pose's
  // y row, then its y column (each an exact sign-flip composition).
  double mirrored[9];
  for (int col = 0; col < 3; ++col) {
    mirrored[0 * 3 + col] = pose[0 * 3 + col];
    mirrored[1 * 3 + col] = -pose[1 * 3 + col];
    mirrored[2 * 3 + col] = pose[2 * 3 + col];
  }
  for (int row = 0; row < 3; ++row) {
    mirrored[row * 3 + 1] = -mirrored[row * 3 + 1];
  }
  double out_a[3];
  double out_b[3];
  const bool valid_a =
      analytic::TracePathChain<double>(table, slots_four_one.data(), 2, n, incident, pose, out_a, nullptr, nullptr);
  const bool valid_b = analytic::TracePathChain<double>(table, slots_eight_one.data(), 2, n, incident, mirrored, out_b,
                                                        nullptr, nullptr);
  ASSERT_EQ(valid_a, valid_b);
  ASSERT_TRUE(valid_a);
  // The sibling's world ray is the mirror image: only the y component flips.
  EXPECT_EQ(out_a[0], out_b[0]);
  EXPECT_EQ(out_a[1], -out_b[1]);
  EXPECT_EQ(out_a[2], out_b[2]);
}

}  // namespace
}  // namespace lumice
