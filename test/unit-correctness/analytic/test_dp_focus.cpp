// The kind-1 focusing layer (src/analytic/dp_focus.hpp, LI focusing.py ported for scrum 660.4)
// against the pinned anchors: the closed form of C01's inner edge (2 asin(n sin 30 deg) - 60 deg),
// the family_pinned truth table and its sigma -> 0 circle evidence (AC2), the slab onset table of
// the beta crystal's blade, the ch10 saddle / TIR-corner cross-layer anchors, and the refusal
// paths (a walk with no lattice point, an onset count that changes across n). LI's own bounds are
// the assertion constants where it has them (the spread circles' 1e-9 deg); measured JAX values
// sit in comments as context, not as bounds — cross-ISA discipline keeps them out of EXPECTs.
//
// symmetry_semantics: none for the onset table; the family_pinned block states its density in
// PoseDensitySpec fields, no reduction involved.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "analytic/dp_focus.hpp"
#include "analytic/dp_partition.hpp"
#include "analytic/dp_weight_kink.hpp"
#include "analytic/path_evaluation.hpp"
#include "analytic/reflection_group.hpp"

namespace lumice::analytic {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kN131 = 1.31;                // LI's canonical index (the anchor tables' n)
constexpr double kN550 = 1.3110129170742788;  // LI n(550), full precision (fixture-caliber literal)

double Deg(double rad) {
  return rad * 180.0 / kPi;
}

LUMICE_ANALYTIC_Crystal Prism() {
  LUMICE_ANALYTIC_Crystal c{};
  c.kind = LUMICE_ANALYTIC_CRYSTAL_PRISM;
  c.height = 1.0;
  for (int i = 0; i < 6; i++) {
    c.face_distance[i] = 1.0;
  }
  return c;
}

// The beta crystal of the 52.x / 659 fixtures: fd = [2, 1, 1, 2, 1, 1], h = 3 (faces 3 / 6 absent).
LUMICE_ANALYTIC_Crystal Beta() {
  LUMICE_ANALYTIC_Crystal c{};
  c.kind = LUMICE_ANALYTIC_CRYSTAL_PRISM;
  c.height = 3.0;
  const double fd[6] = { 2.0, 1.0, 1.0, 2.0, 1.0, 1.0 };
  for (int i = 0; i < 6; i++) {
    c.face_distance[i] = fd[i];
  }
  return c;
}

// The ch10 d1 terrain crystal: a regular prism at plate h/a = 0.2 (Liljequist's own caliber).
LUMICE_ANALYTIC_Crystal PlateD1() {
  LUMICE_ANALYTIC_Crystal c{};
  c.kind = LUMICE_ANALYTIC_CRYSTAL_PRISM;
  c.height = 0.1;
  for (int i = 0; i < 6; i++) {
    c.face_distance[i] = 1.0;
  }
  return c;
}

struct Fixture {
  FaceNormalTable normals;
  FacePolygonTable polygons;

  explicit Fixture(const LUMICE_ANALYTIC_Crystal& crystal) {
    EXPECT_EQ(BuildFaceNormals(crystal, &normals, &polygons), Status::kOk);
  }

  void Resolve(const int* faces, int count, int* slots) const {
    ASSERT_EQ(ResolveFaceSequence(normals, faces, count, slots), Status::kOk);
  }
};

PoseDensitySpec Random() {
  PoseDensitySpec spec;
  spec.family = PoseFamily::kRandom;
  return spec;
}

// LI build_pose_density("plate"/"column"/"parry"/"lowitz", ...) with the test widths above.
PoseDensitySpec Density(PoseFamily family, double zenith_mean_deg, double zenith_std_deg, double roll_mean_deg = 0.0,
                        double roll_std_deg = 0.0) {
  PoseDensitySpec spec;
  spec.family = family;
  spec.zenith_mean_deg = zenith_mean_deg;
  spec.zenith_std_deg = zenith_std_deg;
  spec.roll_mean_deg = family == PoseFamily::kParry || family == PoseFamily::kLowitz ? roll_mean_deg : 0.0;
  spec.roll_std_deg = family == PoseFamily::kParry || family == PoseFamily::kLowitz ? roll_std_deg : 0.0;
  return spec;
}

double Angle(const double a[3], const double b[3]) {
  const double cross[3] = { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
  const double dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  return std::atan2(std::sqrt(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]), dot);
}

// The index of the onset with `source`, -1 when absent.
int IndexOfSource(const std::vector<CriticalOnset>& onsets, OnsetSource source) {
  for (size_t k = 0; k < onsets.size(); k++) {
    if (onsets[k].source == source) {
      return static_cast<int>(k);
    }
  }
  return -1;
}

// The spread (max - min, deg) of D_P over the points of `circle` that are inside U_P; the count
// of valid points rides along so an empty U_P cannot pass silently.
double CircleSpread(const DeviationField& field, const std::vector<double>& circle, int* valid_count) {
  double smallest = 1e9;
  double largest = -1e9;
  *valid_count = 0;
  for (size_t k = 0; k + 2 < circle.size(); k += 3) {
    const double* u = &circle[k];
    if (!InsideUp(field, u)) {
      continue;
    }
    const double d = Deg(field.Sample(u).d_value);
    smallest = std::min(smallest, d);
    largest = std::max(largest, d);
    (*valid_count)++;
  }
  return largest - smallest;
}

// 721 azimuths on the latitude circle at sin(z) = z of body z (a plate family's u runs these).
std::vector<double> LatitudeCircle(double z) {
  const double r = std::sqrt(std::max(0.0, 1.0 - z * z));
  std::vector<double> circle;
  for (int i = 0; i < 721; i++) {
    const double t = 2.0 * kPi * static_cast<double>(i) / 721.0;
    circle.push_back(r * std::cos(t));
    circle.push_back(r * std::sin(t));
    circle.push_back(z);
  }
  return circle;
}

// 721 azimuths on the circle `eta` off body x (a Parry-at-roll-0 family's u runs these).
std::vector<double> CircleAboutBodyX(double eta) {
  const double c = std::cos(eta);
  const double s = std::sin(eta);
  std::vector<double> circle;
  for (int i = 0; i < 721; i++) {
    const double t = 2.0 * kPi * static_cast<double>(i) / 721.0;
    circle.push_back(c);
    circle.push_back(s * std::cos(t));
    circle.push_back(s * std::sin(t));
  }
  return circle;
}

// ---------------------------------------------------------------------------------------------
// ConfinedDimensions / FamilyAxis (Step 1)
// ---------------------------------------------------------------------------------------------

// LI test_confined_dimensions: random none, a zenith Gaussian one, a roll-locked density two; the
// widths are the spec's own, in radians.
TEST(DPFocus, ConfinedDimensionsPerFamily) {
  const ConfinedDimensions random = ConfinedDimensionsOf(Random());
  EXPECT_EQ(random.dimensions, 0);
  EXPECT_TRUE(random.widths_rad.empty());

  const ConfinedDimensions column = ConfinedDimensionsOf(Density(PoseFamily::kColumn, 90.0, 0.5));
  EXPECT_EQ(column.dimensions, 1);
  ASSERT_EQ(column.widths_rad.size(), 1u);
  EXPECT_NEAR(column.widths_rad[0], 0.5 * kPi / 180.0, 1e-15);

  const ConfinedDimensions parry = ConfinedDimensionsOf(Density(PoseFamily::kParry, 90.0, 0.5, 0.0, 1.0));
  EXPECT_EQ(parry.dimensions, 2);
  ASSERT_EQ(parry.widths_rad.size(), 2u);
  EXPECT_NEAR(parry.widths_rad[0], 0.5 * kPi / 180.0, 1e-15);
  EXPECT_NEAR(parry.widths_rad[1], 1.0 * kPi / 180.0, 1e-15);
}

// LI _family_axis: a zenith Gaussian at a pole pins e3 (either pole, whatever the family name),
// off a pole nothing; a roll-locked density pins the body image of the zenith — body x at
// Parry's (90, 0), e3 for Lowitz at the pole.
TEST(DPFocus, FamilyAxisPerDensity) {
  double axis[3];
  EXPECT_TRUE(FamilyAxis(Density(PoseFamily::kPlate, 0.0, 0.5), axis));
  EXPECT_NEAR(axis[0], 0.0, 0.0);
  EXPECT_NEAR(axis[1], 0.0, 0.0);
  EXPECT_NEAR(axis[2], 1.0, 0.0);
  EXPECT_TRUE(FamilyAxis(Density(PoseFamily::kPlate, 180.0, 0.5), axis));
  EXPECT_NEAR(axis[2], 1.0, 0.0);  // the other pole is the same family
  EXPECT_FALSE(FamilyAxis(Density(PoseFamily::kColumn, 20.0, 0.5), axis));
  EXPECT_FALSE(FamilyAxis(Random(), axis));

  EXPECT_TRUE(FamilyAxis(Density(PoseFamily::kParry, 90.0, 0.5, 0.0, 1.0), axis));
  EXPECT_NEAR(axis[0], 1.0, 1e-15);
  EXPECT_NEAR(axis[1], 0.0, 1e-15);
  EXPECT_NEAR(axis[2], 0.0, 1e-15);
  EXPECT_TRUE(FamilyAxis(Density(PoseFamily::kLowitz, 0.0, 0.5, 0.0, 1.0), axis));
  EXPECT_NEAR(axis[2], 1.0, 1e-15);
  // A Parry family with a non-zero roll mean tilts the axis off body x by the roll.
  EXPECT_TRUE(FamilyAxis(Density(PoseFamily::kParry, 90.0, 0.5, 30.0, 1.0), axis));
  EXPECT_NEAR(axis[0], std::cos(30.0 * kPi / 180.0), 1e-15);
  EXPECT_NEAR(axis[1], -std::sin(30.0 * kPi / 180.0), 1e-15);
}

// ---------------------------------------------------------------------------------------------
// FamilyPinned truth table and circle evidence (Step 1, AC2)
// ---------------------------------------------------------------------------------------------

// LI test_family_pinned_truth_table: {3-6-4-8, 1-2-1, 1-3-4-2} pin under plate and Lowitz, not
// under column / Parry / random; {3-5, 1-3} pin under nothing; the other pole still pins; a mean
// off the pole does not; rank 0 has no field to pin.
TEST(DPFocus, FamilyPinnedTruthTable) {
  const Fixture f(Prism());
  const int pinned_3648[4] = { 3, 6, 4, 8 };
  const int pinned_121[3] = { 1, 2, 1 };
  const int pinned_1342[4] = { 1, 3, 4, 2 };
  const int ring_35[2] = { 3, 5 };
  const int arc_13[2] = { 1, 3 };
  const int rank0_36[2] = { 3, 6 };
  struct Case {
    const int* faces;
    int count;
  };
  for (const Case& c : std::vector<Case>{ { pinned_3648, 4 }, { pinned_121, 3 }, { pinned_1342, 4 } }) {
    int slots[kMaxFaceCount];
    f.Resolve(c.faces, c.count, slots);
    EXPECT_TRUE(FamilyPinned(f.normals, slots, c.count, Density(PoseFamily::kPlate, 0.0, 0.5)));
    EXPECT_TRUE(FamilyPinned(f.normals, slots, c.count, Density(PoseFamily::kLowitz, 0.0, 0.5, 0.0, 1.0)));
    EXPECT_FALSE(FamilyPinned(f.normals, slots, c.count, Density(PoseFamily::kColumn, 90.0, 0.5)));
    EXPECT_FALSE(FamilyPinned(f.normals, slots, c.count, Density(PoseFamily::kParry, 90.0, 0.5, 0.0, 1.0)));
    EXPECT_FALSE(FamilyPinned(f.normals, slots, c.count, Random()));
  }
  for (const Case& c : std::vector<Case>{ { ring_35, 2 }, { arc_13, 2 } }) {
    int slots[kMaxFaceCount];
    f.Resolve(c.faces, c.count, slots);
    EXPECT_FALSE(FamilyPinned(f.normals, slots, c.count, Density(PoseFamily::kPlate, 0.0, 0.5)));
    EXPECT_FALSE(FamilyPinned(f.normals, slots, c.count, Density(PoseFamily::kParry, 90.0, 0.5, 0.0, 1.0)));
  }
  // The c axis at the other pole is the same family; off the pole there is no circle about c.
  {
    int slots[kMaxFaceCount];
    f.Resolve(pinned_3648, 4, slots);
    EXPECT_TRUE(FamilyPinned(f.normals, slots, 4, Density(PoseFamily::kPlate, 180.0, 0.5)));
    EXPECT_FALSE(FamilyPinned(f.normals, slots, 4, Density(PoseFamily::kPlate, 20.0, 0.5)));
  }
  // Rank 0 (3-6: M = I, wedge 0) commutes with everything but has no field: never pinned.
  {
    int slots[kMaxFaceCount];
    f.Resolve(rank0_36, 2, slots);
    EXPECT_FALSE(FamilyPinned(f.normals, slots, 2, Density(PoseFamily::kPlate, 0.0, 0.5)));
  }
}

// LI test_family_pinned_covers_parry (corpus C13): the Parry family pins the S_x-fold wedge-0
// paths — 1-6-2 (D = 2h, the subsun) and its roll-180 label-swap partner 1-3-2 — while 1-4-2
// (the mirror of a tilted face) and 3-5 (wedge 60 deg, the guard's counterexample) stay out.
TEST(DPFocus, FamilyPinnedCoversParry) {
  const Fixture f(Prism());
  const int subsun[3] = { 1, 6, 2 };
  const int swapped[3] = { 1, 3, 2 };
  const int tilted[3] = { 1, 4, 2 };
  const int ring[2] = { 3, 5 };
  const PoseDensitySpec parry = Density(PoseFamily::kParry, 90.0, 0.5, 0.0, 1.0);
  struct Case {
    const int* faces;
  };
  for (const Case& c : std::vector<Case>{ { subsun }, { swapped }, { tilted } }) {
    int slots[3];
    f.Resolve(c.faces, 3, slots);
    const bool pinned = FamilyPinned(f.normals, slots, 3, parry);
    if (c.faces == tilted) {
      EXPECT_FALSE(pinned);
    } else {
      EXPECT_TRUE(pinned);
    }
  }
  int ring_slots[2];
  f.Resolve(ring, 2, ring_slots);
  EXPECT_FALSE(FamilyPinned(f.normals, ring_slots, 2, parry));
}

// AC2's spread evidence, plate side (LI test_family_pinned_against_d_p_on_latitude_circles): a
// pinned path's D_P is constant on every latitude circle about body z a plate family's u runs
// (JAX measures <= 5.7e-14 deg; the bound is LI's own 1e-9), the 3-5 control varies by degrees
// (JAX measures >= 19.3; the bound is the issue's 7.2 anchor). At least two circles with 50
// valid points each, so an empty U_P cannot pass.
TEST(DPFocus, PinnedFamilyDConstantOnLatitudeCircles) {
  const Fixture f(Prism());
  const int pinned[4] = { 3, 6, 4, 8 };
  const int ring[2] = { 3, 5 };
  int slots[4];
  const double elevations[6] = { 5.0, 25.0, 60.0, -5.0, -25.0, -60.0 };
  struct Case {
    const int* faces;
    int count;
    bool is_pinned;
  };
  for (const Case& c : std::vector<Case>{ { pinned, 4, true }, { ring, 2, false } }) {
    f.Resolve(c.faces, c.count, slots);
    const DeviationField field(f.normals, f.polygons, slots, c.count, kN131);
    int circles = 0;
    double worst_pinned = 0.0;
    double smallest_control = 1e9;
    for (double elevation : elevations) {
      int valid = 0;
      const double spread = CircleSpread(field, LatitudeCircle(std::sin(elevation * kPi / 180.0)), &valid);
      if (valid < 50) {
        continue;
      }
      circles++;
      worst_pinned = std::max(worst_pinned, c.is_pinned ? spread : 0.0);
      smallest_control = std::min(smallest_control, c.is_pinned ? 1e9 : spread);
    }
    if (circles < 2) {
      ADD_FAILURE() << "fewer than two latitude circles of " << c.count << " faces hold 50 valid points";
      continue;
    }
    if (c.is_pinned) {
      EXPECT_LE(worst_pinned, 1e-9);
    } else {
      EXPECT_GE(smallest_control, 7.2);
    }
  }
}

// AC2's spread evidence, Parry side (LI test_family_pinned_against_d_p_on_circles_about_body_x):
// the subsun path's D = 2 arcsin |u . x| depends on u . x alone, constant on every circle about
// body x (JAX <= 4.3e-14 deg, the plan's anchor; the bound is LI's 1e-9).
TEST(DPFocus, SubsunDConstantOnCirclesAboutBodyX) {
  const Fixture f(Prism());
  const int subsun[3] = { 1, 6, 2 };
  int slots[3];
  f.Resolve(subsun, 3, slots);
  const DeviationField field(f.normals, f.polygons, slots, 3, kN131);
  int circles = 0;
  double worst = 0.0;
  for (double off : { 5.0, 25.0, 60.0, 85.0 }) {
    int valid = 0;
    const double spread = CircleSpread(field, CircleAboutBodyX(off * kPi / 180.0), &valid);
    if (valid < 50) {
      continue;
    }
    circles++;
    worst = std::max(worst, spread);
  }
  ASSERT_GE(circles, 2);
  EXPECT_LE(worst, 1e-9);
}

// ---------------------------------------------------------------------------------------------
// Interior Newton + onset collection (Step 2)
// ---------------------------------------------------------------------------------------------

// The interior minimum of 3-5 is the C01 closed form 2 asin(n sin 30 deg) - 60 deg — 21.839300 deg
// at n = 1.31, 21.916127 at n(550) — with the measure limit 2 pi / sqrt(det H) and gradient norm
// at the Newton tolerance (1e-10 scale, asserted loosely as a class).
TEST(DPFocus, InteriorMinimumOfThreeFiveIsTheClosedForm) {
  const Fixture f(Prism());
  const int faces[2] = { 3, 5 };
  int slots[2];
  f.Resolve(faces, 2, slots);
  const DeviationField field(f.normals, f.polygons, slots, 2, kN131);
  const std::vector<InteriorCriticalPoint> points = InteriorCriticalPointsOf(field, InteriorNewtonOptions());
  ASSERT_EQ(points.size(), 1u);
  EXPECT_EQ(points[0].kind, CriticalKind::kMinimum);
  const double closed_form = Deg(2.0 * std::asin(kN131 * 0.5)) - 60.0;
  EXPECT_NEAR(Deg(points[0].value), closed_form, 1e-8);
  EXPECT_LT(points[0].gradient_norm, 1e-9);
  // Both eigenvalues positive (a minimum), of the order LI measures (+0.34 / +0.96 corrected).
  EXPECT_GT(points[0].hessian_eigenvalues[0], 0.0);
  EXPECT_GT(points[0].hessian_eigenvalues[1], 0.0);
  EXPECT_NEAR(points[0].hessian_eigenvalues[0], 0.34, 0.1);
  EXPECT_NEAR(points[0].hessian_eigenvalues[1], 0.96, 0.1);
}

// The onset table of 3-5 at n = 1.31 (the LI-measured anchor): the interior minimum, the two
// boundary extrema (4-fold mirror merge and 2-fold), the corner. Boundary values agree with the
// walk's own loop data — same-source assertion: the onset carries the loop's value verbatim.
TEST(DPFocus, OnsetTableOfThreeFive) {
  const Fixture f(Prism());
  const int faces[2] = { 3, 5 };
  int slots[2];
  f.Resolve(faces, 2, slots);
  const DeviationField field(f.normals, f.polygons, slots, 2, kN131);
  const WalkResult walk = WalkBoundary(field, BoundaryWalkOptions());
  ASSERT_EQ(walk.status, WalkStatus::kOk);
  const std::vector<CriticalOnset> onsets = FieldOnsets(field, nullptr, walk.loop);
  ASSERT_EQ(onsets.size(), 4u);
  EXPECT_EQ(onsets[0].source, OnsetSource::kInteriorMinimum);
  EXPECT_EQ(onsets[0].profile, OnsetProfile::kFiniteJump);
  EXPECT_TRUE(onsets[0].has_measure_limit);
  EXPECT_NEAR(Deg(onsets[0].value), Deg(2.0 * std::asin(kN131 * 0.5)) - 60.0, 1e-8);
  EXPECT_EQ(onsets[1].source, OnsetSource::kBoundaryExtremum);
  EXPECT_EQ(onsets[1].multiplicity, 4);
  EXPECT_NEAR(Deg(onsets[1].value), 42.990858891, 1e-6);
  EXPECT_EQ(onsets[2].source, OnsetSource::kBoundaryExtremum);
  EXPECT_EQ(onsets[2].multiplicity, 2);
  EXPECT_NEAR(Deg(onsets[2].value), 43.465157696, 1e-6);
  EXPECT_EQ(onsets[3].source, OnsetSource::kCorner);
  EXPECT_EQ(onsets[3].multiplicity, 2);
  EXPECT_NEAR(Deg(onsets[3].value), 50.062619367, 1e-6);
  // Same-source: every boundary onset's value equals its loop datum to the last float.
  for (const BoundaryCriticalPoint& point : walk.loop.critical_points) {
    if (point.corner) {
      continue;
    }
    bool found = false;
    for (const CriticalOnset& onset : onsets) {
      if (onset.source == OnsetSource::kBoundaryExtremum && std::fabs(onset.value - point.value) <= kExtremumAtol) {
        found = true;
      }
    }
    EXPECT_TRUE(found) << "boundary extremum " << Deg(point.value) << " deg missing from the onset table";
  }
}

// The slab onsets of the beta crystal's 4-8-7-5 (the C05 cell, the fixture's n(550) caliber):
// both axis cone points at 0 (boundary, merged to multiplicity 2), the blade as a degenerate
// boundary extremum at exactly 120 deg, the rotation circle as an interior inverse-square-root
// divergence at the same value — the focusing label of the blade is jacobian. U_P is direction
// space: the corner values are the prism's own at this index (the fixture table's rows).
TEST(DPFocus, SlabOnsetsOfBetaFourEightSevenFive) {
  const Fixture f(Beta());
  const int faces[4] = { 4, 8, 7, 5 };
  int slots[4];
  f.Resolve(faces, 4, slots);
  const DeviationField field(f.normals, f.polygons, slots, 4, kN550);
  ASSERT_TRUE(field.fold().degenerate);
  const DegenerateFoldSet fold_set = BuildDegenerateFoldSet(field, kFoldCircleSamples);
  const WalkResult walk = WalkBoundary(field, BoundaryWalkOptions());
  ASSERT_EQ(walk.status, WalkStatus::kOk);
  const std::vector<CriticalOnset> onsets = FieldOnsets(field, &fold_set, walk.loop);
  ASSERT_EQ(onsets.size(), 5u);
  EXPECT_EQ(onsets[0].source, OnsetSource::kSlabAxis);
  EXPECT_EQ(onsets[0].profile, OnsetProfile::kConePoint);
  EXPECT_EQ(onsets[0].multiplicity, 2);
  EXPECT_EQ(onsets[0].location, OnsetLocation::kBoundary);
  EXPECT_NEAR(Deg(onsets[0].value), 0.0, 1e-12);
  EXPECT_EQ(onsets[1].source, OnsetSource::kBoundaryExtremum);  // the axis minimum, not the blade
  EXPECT_EQ(onsets[1].profile, OnsetProfile::kBoundaryOnset);
  EXPECT_NEAR(Deg(onsets[1].value), 0.0, 1e-12);
  // The blade: the boundary extremum AT 120 deg (the table's fourth row), degenerate — |grad D|
  // vanishes there — carrying multiplicity 2.
  int blade = -1;
  for (size_t k = 0; k < onsets.size(); k++) {
    if (onsets[k].source == OnsetSource::kBoundaryExtremum && std::fabs(Deg(onsets[k].value) - 120.0) < 1e-6) {
      blade = static_cast<int>(k);
    }
  }
  ASSERT_GE(blade, 0);
  EXPECT_EQ(onsets[blade].profile, OnsetProfile::kDegenerate);
  EXPECT_EQ(onsets[blade].multiplicity, 2);
  EXPECT_NEAR(Deg(onsets[blade].value), 120.0, 1e-8);
  const int circle = IndexOfSource(onsets, OnsetSource::kSlabCircle);
  ASSERT_GE(circle, 0);
  EXPECT_EQ(onsets[circle].profile, OnsetProfile::kInverseSqrtDivergence);
  EXPECT_EQ(onsets[circle].location, OnsetLocation::kInterior);
  EXPECT_NEAR(Deg(onsets[circle].value), 120.0, 1e-12);
  EXPECT_NEAR(onsets[circle].gradient_norm, 0.0, 0.0);
  const int corner = IndexOfSource(onsets, OnsetSource::kCorner);
  ASSERT_GE(corner, 0);
  EXPECT_EQ(onsets[corner].multiplicity, 4);
  EXPECT_NEAR(Deg(onsets[corner].value), 50.161741671, 1e-6);
}

// The mirror slab 3-1-6: the crease circle enters as the slab_circle onset at value 0 (a kink, no
// focusing), one axis cone point at 180, and |grad D| = 2 on all of U_P (the gradient range).
TEST(DPFocus, MirrorSlabCreaseOnset) {
  const Fixture f(Prism());
  const int faces[3] = { 3, 1, 6 };
  int slots[3];
  f.Resolve(faces, 3, slots);
  const DeviationField field(f.normals, f.polygons, slots, 3, kN131);
  ASSERT_TRUE(field.fold().degenerate);
  const DegenerateFoldSet fold_set = BuildDegenerateFoldSet(field, kFoldCircleSamples);
  const WalkResult walk = WalkBoundary(field, BoundaryWalkOptions());
  ASSERT_EQ(walk.status, WalkStatus::kOk);
  const std::vector<CriticalOnset> onsets = FieldOnsets(field, &fold_set, walk.loop);
  ASSERT_EQ(onsets.size(), 4u);
  const int crease = IndexOfSource(onsets, OnsetSource::kSlabCircle);
  ASSERT_GE(crease, 0);
  EXPECT_EQ(onsets[crease].profile, OnsetProfile::kCrease);
  EXPECT_EQ(onsets[crease].location, OnsetLocation::kBoundary);
  EXPECT_NEAR(Deg(onsets[crease].value), 0.0, 1e-12);
  EXPECT_NEAR(onsets[crease].gradient_norm, 2.0, 1e-12);
  const int axis = IndexOfSource(onsets, OnsetSource::kSlabAxis);
  ASSERT_GE(axis, 0);
  EXPECT_NEAR(Deg(onsets[axis].value), 180.0, 1e-12);
  double range[2];
  ASSERT_TRUE(GradientNormRange(field, 2000, range));
  EXPECT_NEAR(range[0], 2.0, 1e-12);
  EXPECT_NEAR(range[1], 2.0, 1e-12);
}

// ---------------------------------------------------------------------------------------------
// Classify + WavelengthCriticalTable (Step 3)
// ---------------------------------------------------------------------------------------------

// Rank 0 (3-6: no field) classifies as point_mass with no onsets, no gradient range and no
// family_pinned, without running any walk; the mechanism vocabulary joins with "+".
TEST(DPFocus, RankZeroIsPointMass) {
  const Fixture f(Prism());
  const int faces[2] = { 3, 6 };
  int slots[2];
  f.Resolve(faces, 2, slots);
  const FocusingClassification label =
      Classify(f.normals, f.polygons, slots, 2, Density(PoseFamily::kPlate, 0.0, 0.5), kN131);
  EXPECT_EQ(label.halo_map_rank, 0);
  EXPECT_EQ(label.Mechanism(), "point_mass");
  EXPECT_TRUE(label.onsets.empty());
  EXPECT_FALSE(label.has_gradient_norm_range);
  EXPECT_FALSE(label.family_pinned);
  EXPECT_FALSE(label.escaped);

  const int subsun[3] = { 1, 6, 2 };
  int subsun_slots[3];
  f.Resolve(subsun, 3, subsun_slots);
  const FocusingClassification pinned =
      Classify(f.normals, f.polygons, subsun_slots, 3, Density(PoseFamily::kParry, 90.0, 0.5, 0.0, 1.0), kN131);
  EXPECT_TRUE(pinned.family_pinned);
  // A mirror fold's only slab onset is the crease (not a Jacobian profile): the Parry subsun is a
  // dimension collapse alone. The rotation-fold plate case carries both (LI's truth table's
  // "jacobian+dimension_collapse" row).
  EXPECT_EQ(pinned.Mechanism(), "dimension_collapse");

  const int rotation_pinned[4] = { 3, 6, 4, 8 };
  int rotation_slots[4];
  f.Resolve(rotation_pinned, 4, rotation_slots);
  EXPECT_EQ(
      Classify(f.normals, f.polygons, rotation_slots, 4, Density(PoseFamily::kPlate, 0.0, 0.5), kN131).Mechanism(),
      "jacobian+dimension_collapse");

  const int ring[2] = { 3, 5 };
  int ring_slots[2];
  f.Resolve(ring, 2, ring_slots);
  const FocusingClassification collapse =
      Classify(f.normals, f.polygons, ring_slots, 2, Density(PoseFamily::kPlate, 0.0, 0.5), kN131);
  EXPECT_EQ(collapse.Mechanism(), "dimension_collapse");
  EXPECT_FALSE(collapse.family_pinned);
  EXPECT_EQ(Classify(f.normals, f.polygons, ring_slots, 2, Random(), kN131).Mechanism(), "none");
}

// AC1's control row: the wavelength table's interior minimum equals the closed form at every
// index, and its displacement is the closed forms' difference; the 3-5 displacement anchor is
// 0.580962 deg across LI's n(450) / n(550) / n(650).
TEST(DPFocus, WavelengthTableInteriorMinimumFollowsTheClosedForm) {
  const Fixture f(Prism());
  const int faces[2] = { 3, 5 };
  int slots[2];
  f.Resolve(faces, 2, slots);
  const std::vector<std::string> labels = { "450nm", "550nm", "650nm" };
  const std::vector<double> indices = { 1.3156924365566363, 1.3110129170742788, 1.308038822937431 };
  const WavelengthCriticalTable table =
      WavelengthCriticalTableOf(f.normals, f.polygons, slots, 2, Random(), labels, indices);
  ASSERT_FALSE(table.escaped) << table.message;
  ASSERT_EQ(table.onsets.size(), 4u);
  EXPECT_EQ(table.labels, labels);
  const WavelengthOnsetShift& minimum = table.onsets[0];
  EXPECT_EQ(minimum.source, OnsetSource::kInteriorMinimum);
  double displacement = 0.0;
  double closed[3];
  for (int k = 0; k < 3; k++) {
    closed[k] = Deg(2.0 * std::asin(indices[k] * 0.5)) - 60.0;
    EXPECT_NEAR(minimum.values_deg[k], closed[k], 1e-8) << labels[k];
    displacement = std::max(displacement, std::fabs(closed[k] - closed[0]));
  }
  EXPECT_NEAR(minimum.displacement_deg, displacement, 1e-8);
  EXPECT_NEAR(minimum.displacement_deg, 0.580962, 1e-6);  // the issue's anchor
  EXPECT_NEAR(minimum.values_deg[1], 21.916127, 1e-6);    // C01 at n(550)
}

// The refusal paths. 3-1-4-6-2 at n = 1.34 has no lattice point in U_P (LI's walk refuses the
// same way): the classification escapes with empty onsets (an ignored escape fails downstream as
// no onsets, not as stale ones). The wavelength table of 1-3-5 across n = 1.30 / 1.34 escapes on
// the count mismatch instead of pairing unrelated onsets.
TEST(DPFocus, RefusalsAreData) {
  const Fixture f(Prism());
  {
    const int faces[5] = { 3, 1, 4, 6, 2 };
    int slots[5];
    f.Resolve(faces, 5, slots);
    const FocusingClassification label = Classify(f.normals, f.polygons, slots, 5, Random(), 1.34);
    EXPECT_TRUE(label.escaped);
    EXPECT_TRUE(label.onsets.empty());
    EXPECT_NE(label.escape_message.find("start_no_point"), std::string::npos)
        << "the walk's own refusal slug reaches the message: " << label.escape_message;
  }
  {
    const int faces[3] = { 1, 3, 5 };
    int slots[3];
    f.Resolve(faces, 3, slots);
    const WavelengthCriticalTable table =
        WavelengthCriticalTableOf(f.normals, f.polygons, slots, 3, Random(), { "lo", "hi" }, { 1.30, 1.34 });
    EXPECT_TRUE(table.escaped);
    EXPECT_TRUE(table.onsets.empty());
    EXPECT_NE(table.message.find("onset counts differ"), std::string::npos) << table.message;
  }
  {
    const int faces[2] = { 3, 5 };
    int slots[2];
    f.Resolve(faces, 2, slots);
    const WavelengthCriticalTable empty = WavelengthCriticalTableOf(f.normals, f.polygons, slots, 2, Random(), {}, {});
    EXPECT_TRUE(empty.escaped);
    EXPECT_EQ(empty.message, "indices is empty");
  }
}

// The ch10 cross-layer anchors on the d1 terrain crystal (LI ch10_verdicts, the liljequist
// verdict): 3-5-6-7's three critical values are [50.06262, 141.83930, 163.46516] deg — the middle
// one the loop maximum on a grazing internal-reflection piece, the "saddle of the smooth
// extension" — and the internal TIR onset of 3-5-6-7-3 puts its constrained maximum at 153.0697
// deg, which the 660.3 kink curve reproduces to its sampling resolution (~8e-4 deg below).
TEST(DPFocus, Ch10AnchorsOnTheD1Crystal) {
  const Fixture f(PlateD1());
  {
    const int faces[4] = { 3, 5, 6, 7 };
    int slots[4];
    f.Resolve(faces, 4, slots);
    const FocusingClassification label = Classify(f.normals, f.polygons, slots, 4, Random(), kN131);
    ASSERT_FALSE(label.escaped) << label.escape_message;
    ASSERT_EQ(label.onsets.size(), 3u);
    EXPECT_NEAR(Deg(label.onsets[0].value), 50.06262, 1e-4);
    EXPECT_NEAR(Deg(label.onsets[1].value), 141.83930, 1e-4);
    EXPECT_NEAR(Deg(label.onsets[2].value), 163.46516, 1e-4);
  }
  {
    const int faces[5] = { 3, 5, 6, 7, 3 };
    int slots[5];
    f.Resolve(faces, 5, slots);
    const DeviationField field(f.normals, f.polygons, slots, 5, kN131);
    const std::vector<KinkCurve> kinks = WeightKinks(field, KinkOptions());
    ASSERT_FALSE(kinks.empty());
    double maximum = -1e9;
    for (const KinkCurve& curve : kinks) {
      for (const KinkArc& arc : curve.arcs) {
        for (double value : arc.values) {
          maximum = std::max(maximum, Deg(value));
        }
      }
    }
    EXPECT_NEAR(maximum, 153.0697, 2e-3);  // the kink sampling sits ~8e-4 deg under the converged solve
  }
}

}  // namespace
}  // namespace lumice::analytic
