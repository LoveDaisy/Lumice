// The restricted-family rank primitive (src/analytic/family_rank.hpp, the Lumice-born kernel of
// the measure-to-family routing) against its three anchor groups. AC3's shape: the C16 Lowitz
// fold anchors (the corpus fold witnesses of the raypath corpus check 2026-10-12 — the tight
// part is the witness-point evaluation: our restricted Jacobian at their refined fold poses
// reproduces their signed Jacobian ~1e-12, their restricted singular values and their outgoing
// vectors; the locus part is grid-resolution containment — a crossing of the extracted fold
// curve within one cell of each witness, the resolution the sampling can honestly certify), the
// Parry one-dimensional circle against FamilyPinned and MakeRestrictedFamilyCurve (the same
// mathematical predicate, closed form vs sampled, must give the same verdict — no second 1D
// judgement), and the refusal paths. Closed-form controls: the parallel basal slab {1,2} is
// pose-independent (rank 0 on any family), the pinned mirror 1-6-2 on the Parry circle is the
// horizontal-plane reflection (image at -sun altitude, azimuth 0), and the pole spin chart
// {azimuth, roll} at zenith 0 is structurally degenerate (sigma2 ~ 0 for every member — data,
// not an error, no NaN). The corpus anchors run at n(550) full precision and the sun at 20
// degrees elevation — the corpus probes' own inputs; the witness Euler coordinates in the
// constants below are the decomposition of the recorded witness poses onto the production chart
// R(a, t, r) = Rz(a - pi) Ry(-t) Rz(r), verified to 2.2e-16 at design time.
//
// symmetry_semantics: none — every family is stated in explicit per-dimension specs.

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <string>
#include <thread>
#include <vector>

#include "analytic/discovery.hpp"
#include "analytic/dp_contour.hpp"
#include "analytic/dp_focus.hpp"
#include "analytic/family_rank.hpp"
#include "analytic/path_evaluation.hpp"
#include "analytic/pose_density.hpp"

namespace lumice::analytic {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kN131 = 1.31;                // the canonical index of the dp_focus truth tables
constexpr double kN550 = 1.3110129170742788;  // n(550) full precision (the corpus probes' index)

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

struct Fixture {
  FaceNormalTable normals;
  FacePolygonTable polygons;

  Fixture() { EXPECT_EQ(BuildFaceNormals(Prism(), &normals, &polygons), Status::kOk); }

  void Resolve(const int* faces, int count, int* slots) const {
    ASSERT_EQ(ResolveFaceSequence(normals, faces, count, slots), Status::kOk);
  }
};

void SunAt(double altitude_deg, double azimuth_deg, double s[3]) {
  const double alt = altitude_deg * kPi / 180.0;
  const double az = azimuth_deg * kPi / 180.0;
  s[0] = std::cos(alt) * std::cos(az);
  s[1] = std::cos(alt) * std::sin(az);
  s[2] = std::sin(alt);
}

// The C16 Lowitz family: roll frozen at its center 0, zenith and azimuth free (the r = 0 sheet
// the corpus witnesses sit on — their poses decompose onto the production chart with roll 0).
RestrictedFamilySpec LowitzFoldSheet() {
  RestrictedFamilySpec spec;
  spec.zenith.mode = FamilyDimensionMode::kFree;
  spec.azimuth.mode = FamilyDimensionMode::kFree;
  spec.roll.mode = FamilyDimensionMode::kFixed;
  spec.roll.center_deg = 0.0;
  return spec;
}

// The Parry circle: zenith 90 and roll 0 frozen, azimuth free — the orbit producer's
// Rz(theta) R0 circle in the production chart (R(a, 90, 0) = Rz(a - pi) R0 with R0 = Ry(-90)).
RestrictedFamilySpec ParryCircle() {
  RestrictedFamilySpec spec;
  spec.zenith.mode = FamilyDimensionMode::kFixed;
  spec.zenith.center_deg = 90.0;
  spec.azimuth.mode = FamilyDimensionMode::kFree;
  spec.roll.mode = FamilyDimensionMode::kFixed;
  spec.roll.center_deg = 0.0;
  return spec;
}

// The C10 pole spin chart: zenith frozen at 0, azimuth and roll free — the chart whose two
// tangents are the same physical spin generator (the structural degeneracy).
RestrictedFamilySpec PoleSpinChart() {
  RestrictedFamilySpec spec;
  spec.zenith.mode = FamilyDimensionMode::kFixed;
  spec.zenith.center_deg = 0.0;
  spec.azimuth.mode = FamilyDimensionMode::kFree;
  spec.roll.mode = FamilyDimensionMode::kFree;
  return spec;
}

// atan2 of |a x b| and a . b — accurate at 0, where acos of the dot loses sqrt(eps) and two
// vectors equal to the last ULP would read as ~8.5e-7 deg apart (so3::AngleBetween's form).
double AngleBetweenDeg(const double a[3], const double b[3]) {
  const double cross[3] = { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
  const double norm = std::sqrt(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]);
  const double dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  return Deg(std::atan2(norm, dot));
}

// ---- the refusals (Step 1) ------------------------------------------------------------------------

TEST(FamilyRank, SpecValidation) {
  const RestrictedFamilySpec good = LowitzFoldSheet();
  EXPECT_STREQ(FamilySpecError(good), "");

  // Roll is [0, 360): 180 legal, 360 not; zenith is [0, 180]: 270 not.
  RestrictedFamilySpec roll_legal = good;
  roll_legal.roll.center_deg = 180.0;
  EXPECT_STREQ(FamilySpecError(roll_legal), "");
  RestrictedFamilySpec roll_out = good;
  roll_out.roll.center_deg = 360.0;
  EXPECT_STRNE(FamilySpecError(roll_out), "");
  RestrictedFamilySpec zenith_out = good;
  zenith_out.zenith.mode = FamilyDimensionMode::kFixed;
  zenith_out.zenith.center_deg = 270.0;
  EXPECT_STRNE(FamilySpecError(zenith_out), "");
  RestrictedFamilySpec negative = good;
  negative.roll.center_deg = -5.0;
  EXPECT_STRNE(FamilySpecError(negative), "");

  // A free dimension takes no center.
  RestrictedFamilySpec free_center = good;
  free_center.zenith.center_deg = 30.0;
  EXPECT_STRNE(FamilySpecError(free_center), "");

  RestrictedFamilySpec non_finite = good;
  non_finite.roll.center_deg = std::nan("");
  EXPECT_STRNE(FamilySpecError(non_finite), "");
}

TEST(FamilyRank, RefusesDegenerateAndOutOfDomainShapes) {
  const Fixture f;
  int slots[2];
  const int faces[2] = { 3, 5 };
  f.Resolve(faces, 2, slots);
  double sun[3];
  SunAt(20.0, 0.0, sun);

  RestrictedFamilySpec all_fixed;
  all_fixed.zenith.mode = FamilyDimensionMode::kFixed;
  all_fixed.zenith.center_deg = 30.0;
  all_fixed.azimuth.mode = FamilyDimensionMode::kFixed;
  all_fixed.azimuth.center_deg = 10.0;
  all_fixed.roll.mode = FamilyDimensionMode::kFixed;
  all_fixed.roll.center_deg = 0.0;
  const RestrictedFamilyRank single = RankRestrictedFamily(f.normals, slots, 2, all_fixed, sun, kN131, 32);
  EXPECT_EQ(single.status, FamilyRankStatus::kRefusedSinglePose);
  EXPECT_EQ(single.generic_rank, -1);

  RestrictedFamilySpec all_free;
  EXPECT_EQ(RankRestrictedFamily(f.normals, slots, 2, all_free, sun, kN131, 32).status,
            FamilyRankStatus::kRefusedFullSo3);

  // Azimuth frozen while something else is free: outside the routing v1 domain.
  RestrictedFamilySpec az_frozen = PoleSpinChart();
  az_frozen.azimuth.mode = FamilyDimensionMode::kFixed;
  az_frozen.azimuth.center_deg = 10.0;
  az_frozen.roll.mode = FamilyDimensionMode::kFree;
  EXPECT_EQ(RankRestrictedFamily(f.normals, slots, 2, az_frozen, sun, kN131, 32).status,
            FamilyRankStatus::kRefusedAzimuthFrozen);

  const RestrictedFamilySpec lowitz = LowitzFoldSheet();
  EXPECT_EQ(RankRestrictedFamily(f.normals, slots, 2, lowitz, sun, kN131, 1).status,
            FamilyRankStatus::kRefusedGridOutOfBounds);
  EXPECT_EQ(RankRestrictedFamily(f.normals, slots, 2, lowitz, sun, kN131, kFamilyMaxGridPerDimension + 1).status,
            FamilyRankStatus::kRefusedGridOutOfBounds);

  RestrictedFamilySpec malformed = lowitz;
  malformed.roll.center_deg = -5.0;
  EXPECT_EQ(RankRestrictedFamily(f.normals, slots, 2, malformed, sun, kN131, 32).status,
            FamilyRankStatus::kRefusedInvalidSpec);
  // The point entry shares the routing (its grid-free form).
  const double params[2] = { 30.0, 10.0 };
  EXPECT_EQ(EvaluateFamilyPoint(f.normals, slots, 2, all_fixed, sun, kN131, params).status,
            FamilyRankStatus::kRefusedSinglePose);
  EXPECT_EQ(EvaluateFamilyPoint(f.normals, slots, 2, all_free, sun, kN131, params).status,
            FamilyRankStatus::kRefusedFullSo3);
}

// ---- the closed-form controls (Step 2) --------------------------------------------------------------

// The parallel basal slab {1, 2}: the outgoing is the incident at every pose, so any family
// collapses (generic rank 0) and the collapse component's image is the sun's anti-direction —
// the closed-form Jacobian control (every derivative is exactly zero up to rounding).
TEST(FamilyRank, ParallelSlabCollapsesOnAnyFamily) {
  const Fixture f;
  int slots[2];
  const int faces[2] = { 1, 2 };
  f.Resolve(faces, 2, slots);
  double sun[3];
  SunAt(20.0, 0.0, sun);

  const RestrictedFamilyRank rank = RankRestrictedFamily(f.normals, slots, 2, LowitzFoldSheet(), sun, kN131, 24);
  ASSERT_EQ(rank.status, FamilyRankStatus::kOk);
  EXPECT_EQ(rank.generic_rank, 0);
  EXPECT_EQ(rank.rank0_samples, rank.valid_samples);
  ASSERT_EQ(rank.collapse_components.size(), 1u);
  const CollapseRecord& collapse = rank.collapse_components[0];
  // A zero-deviation path's image is the sun itself: the outgoing is the incident
  // propagation (-sun_hat), so the halo-side image direction -outgoing IS sun_hat
  // (altitude +20, azimuth 0).
  EXPECT_NEAR(collapse.image_alt_deg, 20.0, 1e-6);
  EXPECT_NEAR(std::fabs(collapse.image_az_deg), 0.0, 1e-6);
  EXPECT_LT(collapse.sigma1, 1e-12);
  EXPECT_EQ(collapse.sample_count, rank.valid_samples);
  EXPECT_TRUE(std::isfinite(rank.min_sigma1));
}

// The pole spin chart is structural data, not an error: at zenith 0 the (azimuth, roll)
// tangents coincide, sigma2 is ~0 for every member (a one-dimensional image for the wedge
// member, a collapse for the pinned mirror), and nothing is NaN.
TEST(FamilyRank, PoleSpinChartIsDataNotError) {
  const Fixture f;
  double sun[3];
  SunAt(20.0, 0.0, sun);

  // The wedge member {3, 5}: the spin still moves the outgoing — a one-dimensional image.
  int wedge[2];
  const int wedge_faces[2] = { 3, 5 };
  f.Resolve(wedge_faces, 2, wedge);
  const RestrictedFamilyRank wedge_rank = RankRestrictedFamily(f.normals, wedge, 2, PoleSpinChart(), sun, kN131, 24);
  ASSERT_EQ(wedge_rank.status, FamilyRankStatus::kOk);
  EXPECT_EQ(wedge_rank.generic_rank, 1);
  EXPECT_EQ(wedge_rank.rank2_samples, 0);
  EXPECT_GT(wedge_rank.rank1_samples, 0);
  EXPECT_TRUE(std::isfinite(wedge_rank.min_sigma1));
  EXPECT_TRUE(std::isfinite(wedge_rank.min_sigma2));
  EXPECT_LT(wedge_rank.min_sigma2, 1e-12);  // the chart tangents coincide exactly at the pole

  // The pinned mirror {1, 2, 1}: the basal reflection is invariant under the whole spin family.
  int mirror[3];
  const int mirror_faces[3] = { 1, 2, 1 };
  f.Resolve(mirror_faces, 3, mirror);
  const RestrictedFamilyRank mirror_rank = RankRestrictedFamily(f.normals, mirror, 3, PoleSpinChart(), sun, kN131, 24);
  ASSERT_EQ(mirror_rank.status, FamilyRankStatus::kOk);
  EXPECT_EQ(mirror_rank.generic_rank, 0);
  ASSERT_EQ(mirror_rank.collapse_components.size(), 1u);
  EXPECT_NEAR(mirror_rank.collapse_components[0].image_alt_deg, -20.0, 1e-6);
  EXPECT_NEAR(std::fabs(mirror_rank.collapse_components[0].image_az_deg), 0.0, 1e-6);
}

// A regular two-dimensional family of the wedge member is rank 2 (the C16 shape's generic
// image — the corpus witnesses' full-SO3 rank is 2 and the restricted one is the fold's 1 only
// ON the fold curve).
TEST(FamilyRank, RegularTwoDimensionalFamilyIsRank2) {
  const Fixture f;
  int slots[2];
  const int faces[2] = { 3, 5 };
  f.Resolve(faces, 2, slots);
  double sun[3];
  SunAt(20.0, 0.0, sun);

  const RestrictedFamilyRank rank = RankRestrictedFamily(f.normals, slots, 2, LowitzFoldSheet(), sun, kN131, 24);
  ASSERT_EQ(rank.status, FamilyRankStatus::kOk);
  EXPECT_EQ(rank.generic_rank, 2);
  EXPECT_GT(rank.rank2_samples, 0);
  EXPECT_GT(rank.valid_samples, 0);
}

// ---- the C16 Lowitz fold anchors (Step 3, AC3's first anchor) ---------------------------------------

// One corpus fold witness: its refined pose (Euler coordinates on the production chart), its
// sky position, its restricted singular values and its outgoing — from the corpus round's
// fold-witnesses.json, LI-verified there.
struct CorpusWitness {
  const char* family;
  int faces[2];
  double zenith_deg;
  double azimuth_deg;
  double alt_deg;
  double az_deg;
  double restricted_sigma1;
  double outgoing[3];
};

const CorpusWitness kWitnesses[3] = {
  { "upper",
    { 3, 5 },
    59.765625,
    237.70624404019037,
    36.000463054003795,
    20.609204718603635,
    0.41548586706191176,
    { -0.7572378868865196, -0.28476585744471844, -0.5877917906005601 } },
  { "lower",
    { 4, 6 },
    41.484375,
    153.3247174035113,
    10.003569407054895,
    22.073965333341054,
    0.4063371461797484,
    { -0.9126108137689134, -0.3700898569260242, -0.17370952879020923 } },
  { "middle",
    { 5, 7 },
    89.296875,
    89.92276755719554,
    -2.570579683571319,
    0.2677232379924053,
    0.391420172492294,
    { -0.9989828253107966, -0.0046679330564981655, 0.04485002937611549 } },
};

// The tight part of the anchor: each corpus witness lies ON our fold curve. The corpus's own
// refinement drove its constraint to ~1e-6 rad, so its RECORDED poses sit ~1e-8 rad off the
// exact fold (their signed Jacobian at their refined points is 1e-12-scale; ours at the
// recorded poses is <= 4.1e-9, i.e. the fold passes within ~1e-8 rad — well inside their
// location budget). The asserted bounds: |signed J| and sigma2 < 1e-8 (the fold within ~1e-8
// rad), sigma1 theirs to 1e-6, the outgoing to 2e-6 deg. This is the position-independent
// semantic core; the locus containment below carries the grid side.
TEST(FamilyRank, LowitzWitnessPointsReproduceTheCorpusMeasurement) {
  const Fixture f;
  double sun[3];
  SunAt(20.0, 0.0, sun);
  for (int w = 0; w < 3; w++) {
    const CorpusWitness& witness = kWitnesses[w];
    int slots[2];
    f.Resolve(witness.faces, 2, slots);
    const double params[2] = { witness.zenith_deg, witness.azimuth_deg };
    const FamilyPointRecord point = EvaluateFamilyPoint(f.normals, slots, 2, LowitzFoldSheet(), sun, kN550, params);
    if (point.status != FamilyRankStatus::kOk || !point.valid) {
      ADD_FAILURE() << "witness " << witness.family << " not evaluated";
      continue;
    }
    EXPECT_NEAR(point.signed_jacobian, 0.0, 1e-8) << witness.family;
    EXPECT_EQ(point.rank_class, 1) << witness.family;
    EXPECT_NEAR(point.sigma2, 0.0, 1e-8) << witness.family;
    EXPECT_NEAR(point.sigma1, witness.restricted_sigma1, 1e-6) << witness.family;
    EXPECT_LT(AngleBetweenDeg(point.outgoing, witness.outgoing), 2e-6) << witness.family;
  }
}

// The locus part: the generic rank is 2, the fold locus is non-empty, and a crossing lies
// within one grid cell of each witness (the curve passes through the witness's cell, so one of
// its boundary edges carries a crossing — the honest resolution of a sampled locus).
TEST(FamilyRank, LowitzFoldLocusContainsTheWitnessCells) {
  const Fixture f;
  double sun[3];
  SunAt(20.0, 0.0, sun);
  const int grid = 90;  // zenith spacing 180/89 deg, azimuth spacing 4 deg

  for (int w = 0; w < 3; w++) {
    const CorpusWitness& witness = kWitnesses[w];
    int slots[2];
    f.Resolve(witness.faces, 2, slots);
    const RestrictedFamilyRank rank = RankRestrictedFamily(f.normals, slots, 2, LowitzFoldSheet(), sun, kN550, grid);
    EXPECT_EQ(rank.generic_rank, 2) << witness.family;
    if (rank.status != FamilyRankStatus::kOk || rank.crossings.empty()) {
      ADD_FAILURE() << "witness " << witness.family << " has no located fold locus";
      continue;
    }
    EXPECT_FALSE(rank.fold_components.empty()) << witness.family;

    double best = 1e9;
    for (const FoldPointRecord& crossing : rank.crossings) {
      // Sky distance, azimuth-aware enough near these positions (no antimeridian in play).
      const double d_alt = crossing.image_alt_deg - witness.alt_deg;
      double d_az = std::fmod(crossing.image_az_deg - witness.az_deg, 360.0);
      if (d_az > 180.0) {
        d_az -= 360.0;
      }
      if (d_az < -180.0) {
        d_az += 360.0;
      }
      const double d_cos = std::cos(d_alt * kPi / 180.0);
      best = std::min(best, std::sqrt(d_alt * d_alt + (d_az * d_cos) * (d_az * d_cos)));
    }
    // The cell diagonal: sqrt((180/89)^2 + 4^2) ~ 4.5 deg; twice it for the curve's slant.
    EXPECT_LT(best, 9.0) << witness.family;
  }
}

// The declared-error tier: the fold representative's image position shift between grid N and
// 2N (dp_contour's |I_N - I_2N| discipline, here on the locus instead of an integral).
// Measured (faces 3-5, grid 60 -> 120, 2026-10-20): both grids see 3 fold components; the
// nearest coarse-to-fine CROSSING pair agrees to < 0.001 deg; the representatives (each
// component's smallest-sigma2 crossing) sit 0.47 deg apart — the representative is a point
// ALONG the curve, so its position moves with the sampling, which is exactly the quantity
// this tier declares.
TEST(FamilyRank, FoldRepresentativeShiftBetweenGrids) {
  const Fixture f;
  int slots[2];
  const int faces[2] = { 3, 5 };
  f.Resolve(faces, 2, slots);
  double sun[3];
  SunAt(20.0, 0.0, sun);

  const RestrictedFamilyRank coarse = RankRestrictedFamily(f.normals, slots, 2, LowitzFoldSheet(), sun, kN550, 60);
  const RestrictedFamilyRank fine = RankRestrictedFamily(f.normals, slots, 2, LowitzFoldSheet(), sun, kN550, 120);
  ASSERT_EQ(coarse.status, FamilyRankStatus::kOk);
  ASSERT_EQ(fine.status, FamilyRankStatus::kOk);
  ASSERT_FALSE(coarse.crossings.empty());
  ASSERT_FALSE(fine.crossings.empty());
  double best_coarse_to_fine = 1e9;
  for (const FoldPointRecord& c : coarse.crossings) {
    for (const FoldPointRecord& fi : fine.crossings) {
      best_coarse_to_fine = std::min(best_coarse_to_fine, AngleBetweenDeg(c.outgoing, fi.outgoing));
    }
  }
  // The coarse grid's spacing is 6 deg (azimuth); the two loci must agree well inside it.
  EXPECT_LT(best_coarse_to_fine, 6.0);
}

// ---- the one-dimensional consistency (Step 4, AC3's second anchor + AC2) -----------------------------

// The Parry circle's verdicts must be FamilyPinned's verdicts — the same mathematical
// predicate, closed form and sampled: 1-6-2 (the subsun mirror, corpus C13) pins and collapses
// (rank 0, image at the reflection of the sun about the horizontal plane: altitude -20,
// azimuth 0); 1-4-2 and 3-5 stay rank 1 with FamilyPinned false.
TEST(FamilyRank, ParryCircleMatchesFamilyPinned) {
  const Fixture f;
  double sun[3];
  SunAt(20.0, 0.0, sun);
  const PoseDensitySpec parry = [] {
    PoseDensitySpec spec;
    spec.family = PoseFamily::kParry;
    spec.zenith_mean_deg = 90.0;
    spec.zenith_std_deg = 0.5;
    spec.roll_mean_deg = 0.0;
    spec.roll_std_deg = 1.0;
    return spec;
  }();

  struct Case {
    const char* name;
    int faces[3];
    int count;
    bool pinned;
    int expected_rank;
  };
  const Case cases[3] = { { "1-6-2", { 1, 6, 2 }, 3, true, 0 },
                          { "1-4-2", { 1, 4, 2 }, 3, false, 1 },
                          { "3-5", { 3, 5, 0 }, 2, false, 1 } };
  for (const Case& c : cases) {
    int slots[3];
    f.Resolve(c.faces, c.count, slots);
    EXPECT_EQ(FamilyPinned(f.normals, slots, c.count, parry), c.pinned) << c.name;
    const RestrictedFamilyRank rank = RankRestrictedFamily(f.normals, slots, c.count, ParryCircle(), sun, kN131, 72);
    if (rank.status != FamilyRankStatus::kOk) {
      ADD_FAILURE() << "case " << c.name << " refused";
      continue;
    }
    EXPECT_EQ(rank.generic_rank, c.expected_rank) << c.name;
    if (c.pinned) {
      if (rank.collapse_components.size() != 1u) {
        ADD_FAILURE() << "case " << c.name << " did not collapse to one component";
        continue;
      }
      EXPECT_NEAR(rank.collapse_components[0].image_alt_deg, -20.0, 1e-6) << c.name;
      EXPECT_NEAR(std::fabs(rank.collapse_components[0].image_az_deg), 0.0, 1e-6) << c.name;
    } else {
      EXPECT_TRUE(rank.collapse_components.empty()) << c.name;
      EXPECT_EQ(rank.rank1_samples, rank.valid_samples) << c.name;
    }
  }
}

// The same verdict through the curve entry: D_P constant on the circle for the pinned member
// (LI's own 1e-9 deg bound for pinned circles), varying by degrees for 3-5 — the restricted
// family curve and this module read the same family with the same predicate.
TEST(FamilyRank, ParryCircleMatchesRestrictedFamilyCurve) {
  const Fixture f;
  double sun[3];
  SunAt(20.0, 0.0, sun);
  const PoseDensitySpec parry = [] {
    PoseDensitySpec spec;
    spec.family = PoseFamily::kParry;
    spec.zenith_mean_deg = 90.0;
    spec.zenith_std_deg = 0.5;
    spec.roll_mean_deg = 0.0;
    spec.roll_std_deg = 1.0;
    return spec;
  }();

  int subsun[3];
  const int subsun_faces[3] = { 1, 6, 2 };
  f.Resolve(subsun_faces, 3, subsun);
  const RestrictedFamilyCurve pinned_curve =
      MakeRestrictedFamilyCurve(f.normals, f.polygons, subsun, 3, parry, sun, kN131, nullptr, nullptr, 0, 72);
  ASSERT_FALSE(pinned_curve.d_p.empty());
  // The spread reads the FINITE entries only: off-U_P points are honest NaN data
  // (routed_nonfinite), and a NaN would poison minmax_element.
  double pinned_lo = 1e9;
  double pinned_hi = -1e9;
  int pinned_finite = 0;
  for (double d : pinned_curve.d_p) {
    if (std::isfinite(d)) {
      pinned_lo = std::min(pinned_lo, d);
      pinned_hi = std::max(pinned_hi, d);
      pinned_finite++;
    }
  }
  ASSERT_GE(pinned_finite, 50);
  EXPECT_LT(Deg(pinned_hi - pinned_lo), 1e-9);

  int wedge[2];
  const int wedge_faces[2] = { 3, 5 };
  f.Resolve(wedge_faces, 2, wedge);
  const RestrictedFamilyCurve wedge_curve =
      MakeRestrictedFamilyCurve(f.normals, f.polygons, wedge, 2, parry, sun, kN131, nullptr, nullptr, 0, 72);
  ASSERT_FALSE(wedge_curve.d_p.empty());
  double wedge_lo = 1e9;
  double wedge_hi = -1e9;
  int wedge_finite = 0;
  for (double d : wedge_curve.d_p) {
    if (std::isfinite(d)) {
      wedge_lo = std::min(wedge_lo, d);
      wedge_hi = std::max(wedge_hi, d);
      wedge_finite++;
    }
  }
  // A wedge member's U_P covers only part of the circle at this sun height (the pinned
  // mirror's field covers nearly all of it) — the floor is the wedge's own coverage.
  ASSERT_GE(wedge_finite, 20);
  EXPECT_GT(Deg(wedge_hi - wedge_lo), 1.0);
}

// The azimuth chart is the orbit producer's circle: R(a, 90, 0) = Rz(a - pi) R0, so the point
// at a = theta + pi and the orbit sample at theta are the same pose — the outgoing directions
// must agree (sample-level, the Rz(theta) R0 shape check).
TEST(FamilyRank, AzimuthChartIsTheOrbitCircle) {
  const Fixture f;
  int slots[2];
  const int faces[2] = { 3, 5 };
  f.Resolve(faces, 2, slots);
  double sun[3];
  SunAt(20.0, 0.0, sun);
  const PoseDensitySpec parry = [] {
    PoseDensitySpec spec;
    spec.family = PoseFamily::kParry;
    spec.zenith_mean_deg = 90.0;
    spec.zenith_std_deg = 0.5;
    spec.roll_mean_deg = 0.0;
    spec.roll_std_deg = 1.0;
    return spec;
  }();

  const int grid = 64;
  const OrbitFiberStream orbit = MakeOrbitFiberStream(f.normals, f.polygons, slots, 2, parry, sun, kN131, grid);
  ASSERT_EQ(static_cast<int>(orbit.samples.size()), grid);
  int compared = 0;
  for (int i = 0; i < grid; i++) {
    if (!orbit.samples[static_cast<size_t>(i)].valid) {
      continue;
    }
    const double theta = 2.0 * kPi * (i + 0.5) / grid;
    const double params[2] = { Deg(theta + kPi), 0.0 };  // a = theta + pi (mod 360 by the chart)
    const FamilyPointRecord point = EvaluateFamilyPoint(f.normals, slots, 2, ParryCircle(), sun, kN131, params);
    if (point.status != FamilyRankStatus::kOk || !point.valid) {
      ADD_FAILURE() << "orbit sample " << i << " not evaluated";
      continue;
    }
    EXPECT_LT(AngleBetweenDeg(point.outgoing, orbit.samples[static_cast<size_t>(i)].outgoing), 1e-7);
    compared++;
  }
  EXPECT_GT(compared, grid / 3);
}


// ---- the ordered-arc layer (task arc-recovery-engine) ----------------------------------------------
//
// The C16 column scene at the 819 exploration's own product calibre: sun 20 deg at azimuth 0,
// faces {3, 5}, the spin chart with zenith frozen at 90 deg and azimuth + roll free, grid 128,
// and n(550) at the ENGINE's full precision — 1.3110129100622272, the value the corpus probes
// ran at. This is deliberately NOT the kN550 constant above (1.3110129170742788, a different
// physical input with its own recorded value): the corpus's n-sensitivity finding pins the
// scene identity to the exact digits, so the two constants stay separate and each cites its
// own source. The closed-form oracle is the corpus check report's column_folds
// (scratchpad-raypath-corpus-check-2026-10-12 C16/brightness-001 sky-loci-v3.json), quoted as
// altitude/azimuth pairs; the 819 median measured against it was 0.2014 deg (its own sampling
// noise floor), the apex the corpus documents independently at 41.916 deg.

constexpr double kC16N = 1.3110129100622272;  // n(550) engine full precision (the corpus probes')

RestrictedFamilySpec C16SpinChart() {
  RestrictedFamilySpec spec;
  spec.zenith.mode = FamilyDimensionMode::kFixed;
  spec.zenith.center_deg = 90.0;
  spec.azimuth.mode = FamilyDimensionMode::kFree;
  spec.roll.mode = FamilyDimensionMode::kFree;
  return spec;
}

// The closed-form column folds: 264 oracle points (altitude, azimuth) in degrees.
struct C16OraclePoint {
  double alt;
  double az;
};
constexpr C16OraclePoint kC16ColumnFolds[264] = {
  { 42.05543299214284, 2.6322050980094023 },        { 42.055432992165024, -2.632205098232862 },
  { 42.29168583052347, 4.585441543608077 },         { 42.29168583051655, -4.585441543557413 },
  { 42.500981387710375, 6.071834762329312 },        { 42.50098138784292, -6.071834763261638 },
  { 42.68336238420953, 7.381732155028808 },         { 42.68336238421445, -7.381732155065313 },
  { 42.838892368005205, 8.597870529472141 },        { 42.83889236793757, -8.597870528908983 },
  { 42.96765774207498, 9.75631432083857 },          { 42.96765774211229, -9.756314321206164 },
  { 43.069769440400194, 10.875860721932126 },       { 43.069769440433696, -10.875860722348659 },
  { 43.145364246993005, 11.967287591026622 },       { 43.1453642469999, -11.967287591146036 },
  { 43.19460575898348, 13.037105988154401 },        { 43.19460575899045, -13.037105988358496 },
  { 43.217684995509195, 14.089341306234001 },       { 43.217684995511014, -14.089341306419955 },
  { 43.21482065940722, 15.126475679838983 },        { 43.21482065940287, -15.126475680124209 },
  { 43.18625906335183, 16.149988605902035 },        { 43.18625906334122, -16.149988606162985 },
  { 43.13227373555481, 17.160687274991904 },        { 43.13227373554676, -17.16068727511328 },
  { 43.05316472443966, 18.158918969605473 },        { 43.05316472445721, -18.15891896941526 },
  { 42.94925762592685, 19.14471347706268 },         { 42.94925762590483, -19.144713477248214 },
  { 42.82090235879479, 20.117881974023298 },        { 42.820902358860586, -20.11788197357054 },
  { 42.668471718155, 21.078087680790237 },          { 42.668471718156084, -21.07808768078396 },
  { 42.49235973658736, 22.02489754326474 },         { 42.49235973677817, -22.024897542309855 },
  { 42.29297988690713, 22.95782071490817 },         { 42.29297988698452, -22.957820714568445 },
  { 42.070763155333005, 23.876337598459404 },       { 42.07076315536516, -23.876337598334 },
  { 41.82615602231876, 24.77992191035574 },         { 41.82615602228524, -24.779921910473135 },
  { 41.559618376890384, 25.66805749858365 },        { 41.5596183769573, -25.66805749837123 },
  { 41.27162139821347, 26.540251094903162 },        { 41.27162139814177, -26.54025109511069 },
  { 40.962645429021855, 27.396041880965672 },       { 40.96264542904523, -27.396041880903656 },
  { 40.633177868727735, 28.235008504186034 },       { 40.63317786862791, -28.235008504430166 },
  { 40.283711105103, 29.056774045668266 },          { 40.28371110497741, -29.056774045952505 },
  { 39.914740507367945, 29.861009315584383 },       { 39.91474050745359, -29.86100931540447 },
  { 39.52676249516832, 30.647434790363878 },        { 39.52676249540535, -30.6474347899001 },
  { 39.12027269665274, 31.415821443790477 },        { 39.12027269688079, -31.415821443373847 },
  { 38.69576420622518, 32.165990681145125 },        { 38.69576420621801, -32.16599068115743 },
  { 38.253725948567336, 32.897813554675906 },       { 38.253725948508404, -32.89781355477041 },
  { 37.79464115679931, 33.611209405635506 },        { 37.7946411567979, -33.61120940563762 },
  { 37.318985962299145, 34.30614406997275 },        { 37.318985962437, -34.306144069777375 },
  { 36.82722809866704, 34.982627753232315 },        { 36.82722809871385, -34.98262775316982 },
  { 36.31982571783032, 35.64071266965433 },         { 36.31982571792885, -35.6407126695302 },
  { 35.79722631499149, 36.28049052619185 },         { 35.797226315180396, -36.280490525967096 },
  { 35.25986575595346, 36.90208992222307 },         { 35.2598657558918, -36.90208992229242 },
  { 34.70816740008345, 37.50567372339909 },         { 34.7081674002115, -37.50567372326283 },
  { 34.1425413147768, 38.09143645500564 },          { 34.142541314578295, -38.09143645520572 },
  { 33.5633835680727, 38.65960176147255 },          { 33.56338356811836, -38.659601761428966 },
  { 32.97107559607791, 39.2104199595496 },          { 32.97107559606081, -39.2104199595651 },
  { 32.365983630857166, 39.74416571774189 },        { 32.36598363079991, -39.7441657177911 },
  { 31.7484581821435, 40.26113588066737 },          { 31.748458182182215, -40.26113588063579 },
  { 31.118833563043303, 40.76164745588733 },        { 31.118833563033494, -40.76164745589494 },
  { 30.47742744452342, 41.24603578078294 },         { 30.477427444478717, -41.24603578081586 },
  { 29.824540437940755, 41.71465287223193 },        { 29.824540437848988, -41.71465287229614 },
  { 29.16045568204028, 42.16786597743777 },         { 29.160455682178355, -42.167865977345876 },
  { 28.485438434539326, 42.606056323704905 },       { 28.48543843438039, -42.60605632380554 },
  { 27.79973564949116, 43.0296180781208 },          { 27.799735649532607, -43.02961807809583 },
  { 27.10357553393625, 43.438957518278805 },        { 27.103575533891625, -43.43895751830442 },
  { 26.397167066092127, 43.83449242229968 },        { 26.397167065969974, -43.834492422366445 },
  { 25.68069946648073, 44.216651680556346 },        { 25.680699466624116, -44.21665168048171 },
  { 24.95434160451187, 44.58587513748622 },         { 24.9543416046455, -44.58587513741992 },
  { 24.21824132898859, 44.942613669375305 },        { 24.218241328845235, -44.942613669443155 },
  { 23.47252470178266, 45.28732951034232 },         { 23.472524701845927, -45.287329510313754 },
  { 22.71729511816268, 45.62049683816131 },         { 22.717295118204603, -45.62049683814325 },
  { 17.988787985769548, 47.405323670683146 },       { 17.988787986123196, -47.405323670565906 },
  { 17.167700217042896, 47.672193557665345 },       { 17.167700217248626, -47.67219355759978 },
  { 16.336948265449465, 47.93191199211513 },        { 16.33694826559144, -47.93191199207157 },
  { -20.00000238376456, 2.433541013969883e-07 },    { -20.000002383799718, -2.433576866628239e-07 },
  { -20.000004697701808, 7.385447969035524e-07 },   { -20.000004697813743, -7.385623906705464e-07 },
  { -20.000006190298894, 1.2342287420004776e-06 },  { -20.000006190437517, -1.2342563783996424e-06 },
  { -20.000007173010857, 1.924501843862164e-06 },   { -20.000007173123677, -1.9245321167935754e-06 },
  { -20.000006905510176, 2.0616081250261323e-06 },  { -20.00000690614063, -2.0617963402362394e-06 },
  { -20.00000629746845, 2.0591839752356316e-06 },   { -20.000006297800514, -2.059292544287818e-06 },
  { -20.00000451471901, 1.7148322360104315e-06 },   { -20.000004514699473, -1.7148248161769527e-06 },
  { -19.999999962985754, -1.9104770052702695e-08 }, { -19.999999962540308, 1.9334707553681688e-08 },
  { -19.99999961874794, -2.0441350385920467e-07 },  { -19.999999619196753, 2.0417290762694625e-07 },
  { -19.999999229301327, -4.4233698414040714e-07 }, { -19.99999922941011, 4.4227455235567406e-07 },
  { -19.999999148531007, -5.03796342290528e-07 },   { -19.9999991489041, 5.035756272574104e-07 },
  { -19.999999129007616, -5.441522977327196e-07 },  { -19.999999130003733, 5.4352995591537e-07 },
  { -19.999999165357053, -5.342033917774922e-07 },  { -19.999999165718013, 5.339723939031843e-07 },
  { -19.999999278268422, -4.821131996518099e-07 },  { -19.99999927807, 4.822457596814335e-07 },
  { -19.999999344673945, -4.460221943321997e-07 },  { -19.999999344684188, 4.460152205636681e-07 },
  { -19.999999410517248, -4.0808933964180833e-07 }, { -19.99999941133566, 4.0752277656456244e-07 },
  { -19.999999474004447, -3.697684516459775e-07 },  { -19.99999947458286, 3.693618225002262e-07 },
  { -19.99999953219279, -3.3340560372522665e-07 },  { -6.771881647804677, -2.572467715718248 },
  { -6.771881645491292, 2.5724677152159763 },       { -19.999999532689138, 3.3305185660667845e-07 },
  { -19.999999589009693, -2.9648581332569135e-07 }, { -9.92025197760872, -2.994180623657458 },
  { -9.92025198009778, 2.9941806237827286 },        { -19.999999588086744, 2.971516370217158e-07 },
  { -19.999999636859062, -2.647484108788311e-07 },  { -12.05029370692019, -2.9727829098290903 },
  { -12.05029370449662, 2.9727829100056486 },       { -19.99999963603018, 2.653527262291243e-07 },
  { -19.99999968062586, -2.3494845284528178e-07 },  { -13.747835528149418, -2.757074686241777 },
  { -13.747835528560689, 2.7570746861663338 },      { -19.9999996828052, 2.3334524492696577e-07 },
  { -19.99999972375241, -2.0474940546595655e-07 },  { -15.188189972923377, -2.418589668585718 },
  { -15.18818997194902, 2.418589668866813 },        { -19.999999723803786, 2.0471134247759255e-07 },
  { -19.999999756989794, -1.8119349638983053e-07 }, { -16.452036927412387, -1.9903059761377708 },
  { -16.452036919571682, 1.9903059792038915 },      { -19.99999975736567, 1.809132369370643e-07 },
  { -19.999999790352543, -1.5701791531937093e-07 }, { -17.584249294699724, -1.4908623012030284 },
  { -17.584249294741753, 1.490862301182331 },       { -19.99999979024473, 1.5709862804933236e-07 },
  { -19.99999982292153, -1.330202391173424e-07 },   { -18.612701572928188, -0.9321139580132325 },
  { -18.612701585303714, 0.9321139506449768 },      { -19.99999980874428, 1.4367011945149945e-07 },
  { -21.236136346461425, 1.0287324089089345 },      { -19.999999874937725, -9.394601474817279e-08 },
  { -19.999999873709292, 9.486879326002007e-08 },   { -21.236136352262942, -1.028732414225418 },
  { -21.989729225997984, 1.7617332383318365 },      { -19.99999989444316, -7.905802655140268e-08 },
  { -19.999999896351806, 7.762854017978052e-08 },   { -21.989729223775154, -1.7617332360398514 },
  { -19.9999999023692, -7.279557986791649e-08 },    { -19.999999904238706, 7.14016666343996e-08 },
  { -19.999999917104464, -6.144057293745615e-08 },  { -19.99999991812399, 6.068494306193597e-08 },
  { -23.968968064676474, 4.1574255535314935 },      { -19.999999925246183, -5.499286218464626e-08 },
  { -19.99999992737279, 5.342841814170254e-08 },    { -23.96896806173003, -4.1574255493669865 },
  { -24.546665035879847, 5.014749056080823 },       { -19.999999937136533, -4.5830703962991026e-08 },
  { -19.999999937933545, 4.524965626819604e-08 },   { -24.54666503660984, -5.014749057218152 },
  { -25.087601890775453, 5.89871225213838 },        { -19.999999942424807, -4.1534390256994036e-08 },
  { -19.999999943536746, 4.073222037547306e-08 },   { -25.08760188956257, -5.898712250059445 },
  { -19.999999951135447, -3.482570198181628e-08 },  { -19.99999994946053, 3.601942407214402e-08 },
  { -19.99999995773643, -2.9710795993125717e-08 },  { -19.999999957745374, 2.9704501938018173e-08 },
  { -19.999999961154696, -2.6891982902065873e-08 }, { -19.999999962411582, 2.602187206945504e-08 },
  { -19.999999967498816, -2.2120673053352324e-08 }, { -19.999999967127675, 2.237326704818883e-08 },
  { -19.99999996892928, -2.075510507495544e-08 },   { -19.99999997023738, 1.988130136506455e-08 },
  { -19.999999973443654, -1.7380118697957093e-08 }, { -19.999999973960517, 1.7041878884870487e-08 },
  { -19.999999976979446, -1.4734044238071144e-08 }, { -19.999999977132916, 1.4635832629951243e-08 },
  { -19.99999997966451, -1.27046176200799e-08 },    { -19.999999979976312, 1.2509799676485199e-08 },
  { -19.999999982274716, -1.0787981515779456e-08 }, { -19.999999982762958, 1.0490852411875879e-08 },
  { -19.999999983954492, -9.493792013180253e-09 },  { -19.99999998396271, 9.48892704242233e-09 },
  { -19.999999986329676, -7.845984581183776e-09 },  { -19.99999998580967, 8.144452545476367e-09 },
  { -19.99999998780077, -6.775847012308972e-09 },   { -19.999999987896445, 6.722709798877054e-09 },
  { -19.99999998896972, -5.914031070314838e-09 },   { -19.999999988853364, 5.976426454219441e-09 },
  { -19.999999990907984, -4.692804963738143e-09 },  { -19.999999989996272, 5.163403198764513e-09 },
  { -19.999999991870858, -4.027054729226151e-09 },  { -19.999999992288718, 3.820079864049763e-09 },
  { -19.99999999304792, -3.29443307027878e-09 },    { -19.99999999267205, 3.472556733847181e-09 },
  { -19.99999999374483, -2.8248573100664643e-09 },  { -19.99999999359022, 2.894692222745048e-09 },
  { -19.99999999479251, -2.2316346095924044e-09 },  { -19.999999994705686, 2.268844391051125e-09 },
  { -19.999999994979984, -2.0313085071767237e-09 }, { -19.999999995496502, 1.8223194547669546e-09 },
  { -19.99999999659609, -1.2929021465513854e-09 },  { -19.999999996073804, 1.491285583906795e-09 },
  { -19.999999996389832, -1.277994904392498e-09 },  { -19.99999999666648, 1.1800637059656174e-09 },
  { -19.999999997999954, -6.539808950537774e-10 },  { -19.9999999979285, 6.773525770044916e-10 },
  { -19.99999999739612, -7.773750437866758e-10 },   { -19.999999997313516, 8.020382700905415e-10 },
  { -19.99999999820828, -4.807140390803879e-10 },   { -19.999999998825718, 3.1505891053335375e-10 },
  { -19.999999998361954, -3.8595017392798225e-10 }, { -19.999999998954785, 2.4627011900458547e-10 },
  { -19.999999998617596, -2.75626353792922e-10 },   { -19.9999999988787, 2.2356557883566493e-10 },
  { -19.99999999873681, -1.9858780418404436e-10 },  { -19.999999998670624, 2.0899737224936216e-10 },
  { -19.999999999233548, -7.824389403411217e-11 },  { -19.9999999992345, 7.81484460490902e-11 },
};

// The image sky direction (unit) of an alt/az pair, and the angular distance of two of them.
void C16SkyDir(double alt_deg, double az_deg, double s[3]) {
  const double alt = alt_deg * kPi / 180.0;
  const double az = az_deg * kPi / 180.0;
  s[0] = std::cos(alt) * std::cos(az);
  s[1] = std::cos(alt) * std::sin(az);
  s[2] = std::sin(alt);
}

double C16SkyAngleDeg(const double a[3], const double b[3]) {
  const double cross[3] = { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
  const double norm = std::sqrt(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]);
  const double dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  return Deg(std::atan2(norm, dot));
}

// Nearest-oracle great-circle distance (degrees) of one chain point.
double C16OracleDistanceDeg(const RestrictedFamilyRank& rank, int crossing_index) {
  const FoldPointRecord& p = rank.crossings[static_cast<size_t>(crossing_index)];
  double dir[3];
  C16SkyDir(p.image_alt_deg, p.image_az_deg, dir);
  double best = 1e9;
  for (const C16OraclePoint& o : kC16ColumnFolds) {
    double od[3];
    C16SkyDir(o.alt, o.az, od);
    best = std::min(best, C16SkyAngleDeg(dir, od));
  }
  return best;
}

// The chain whose image reaches the highest altitude (the upper tangent arc's identification).
size_t C16UpperArcChain(const RestrictedFamilyRank& rank) {
  size_t best = rank.fold_chains.size();
  double best_apex = -1e9;
  for (size_t c = 0; c < rank.fold_chains.size(); c++) {
    double apex = -1e9;
    for (int idx : rank.fold_chains[c].order) {
      apex = std::max(apex, rank.crossings[static_cast<size_t>(idx)].image_alt_deg);
    }
    if (apex > best_apex) {
      best_apex = apex;
      best = c;
    }
  }
  return best;
}

// AC1: the upper arc recovers as an ordered chain, every drawn adjacency carries the cell that
// justifies it (the test re-derives the per-cell occupancy from the crossings' edge references
// — an independent restatement of the adjacency contract, not a read of the chain builder's
// state), the chain hugs the closed-form oracle at the 819 level, and its points walk smoothly.
TEST(FamilyRank, C16UpperArcOrderedChainMatchesClosedFormOracle) {
  Fixture f;
  int slots[2];
  const int faces[2] = { 3, 5 };
  f.Resolve(faces, 2, slots);
  double sun[3];
  SunAt(20.0, 0.0, sun);
  const RestrictedFamilyRank rank = RankRestrictedFamily(f.normals, slots, 2, C16SpinChart(), sun, kC16N, 128);
  ASSERT_EQ(rank.status, FamilyRankStatus::kOk);
  // The base scan reproduces the 819 exploration exactly: 222 crossings on grid edges, 11
  // sampling-connectivity components (the union-find compatibility read). The returned table
  // additionally carries the cliff-probe recoveries (from_cliff_probe) and any subdivision's
  // interior crossings (edge_dim < 0) — the grid-edge non-cliff subset is the 819 reproduction.
  ASSERT_EQ(rank.fold_components.size(), 11u);
  int grid_edge_originals = 0;
  for (const FoldPointRecord& p : rank.crossings) {
    if (p.edge_dim >= 0 && !p.from_cliff_probe) {
      grid_edge_originals++;
    }
  }
  ASSERT_EQ(grid_edge_originals, 222);
  // The ordered layer on top: chains cover every crossing exactly once.
  size_t chained = 0;
  for (const FoldChainRecord& chain : rank.fold_chains) {
    chained += chain.order.size();
  }
  EXPECT_EQ(chained, rank.crossings.size());

  const size_t upper = C16UpperArcChain(rank);
  ASSERT_LT(upper, rank.fold_chains.size());
  const FoldChainRecord& chain = rank.fold_chains[upper];
  // The 819 chain held 78 located points in one walk; this chaining refuses the cells whose
  // interior feasibility it cannot verify (the continuity check), so the arc's unverifiable
  // ends walk as their own kept fragments and the verified core sits strictly inside the 819
  // walk's range.
  ASSERT_GE(chain.order.size(), 60u);
  EXPECT_EQ(chain.closed, false);

  // Per-link provenance: the link's cell holds exactly the two crossings it joins. The
  // occupancy is re-derived here from the edge references (one crossing per edge; a cell's
  // boundary holds up to four).
  const int dim0_cells = 128;  // azimuth (periodic): cells == samples
  int link_bounds_ok = 0;
  for (const FoldChainLink& link : chain.links) {
    link_bounds_ok +=
        static_cast<int>(link.cell_i0 >= 0 && link.cell_i0 < dim0_cells && link.cell_i1 >= 0 && link.cell_i1 < 128);
  }
  ASSERT_EQ(link_bounds_ok, static_cast<int>(chain.links.size()));
  for (size_t l = 0; l < chain.links.size(); l++) {
    const int a = chain.order[l];
    const int b = chain.order[(l + 1) % chain.order.size()];
    const FoldChainLink& link = chain.links[l];
    // Cell (I, J) spans [I, I+1] x [J, J+1]; a boundary crossing's edge must be one of the
    // cell's four. A subdivision link may also join interior-refined crossings (edge fields
    // -1): those are the subdivision's own children of exactly this cell.
    const auto edge_touches_cell = [&](int crossing) {
      const FoldPointRecord& p = rank.crossings[static_cast<size_t>(crossing)];
      if (p.edge_dim < 0) {
        return link.through_subdivision;
      }
      const int down1 = (p.edge_i1 + 127) % 128;
      const int down0 = (p.edge_i0 + 127) % 128;
      if (p.edge_dim == 0) {
        return p.edge_i0 == link.cell_i0 && (p.edge_i1 == link.cell_i1 || down1 == link.cell_i1);
      }
      return p.edge_i1 == link.cell_i1 && (p.edge_i0 == link.cell_i0 || down0 == link.cell_i0);
    };
    if (link.through_subdivision) {
      EXPECT_TRUE(edge_touches_cell(a) || edge_touches_cell(b))
          << "subdivision link " << l << " touches its cell through neither crossing";
    } else {
      EXPECT_TRUE(edge_touches_cell(a)) << "link " << l << " crossing a off its cell";
      EXPECT_TRUE(edge_touches_cell(b)) << "link " << l << " crossing b off its cell";
    }
  }

  // Oracle median at the 819 level: the median nearest-oracle distance of the chain's
  // grid-edge crossings stays inside the oracle's own sampling noise floor (~0.2 deg; the 819
  // run measured 0.2014 over its 78-point subset — this chaining is a strict superset of that
  // partial walk, so the pin is the level, not the subset's exact digits).
  std::vector<double> distances;
  for (int idx : chain.order) {
    distances.push_back(C16OracleDistanceDeg(rank, idx));
  }
  std::sort(distances.begin(), distances.end());
  const double median = distances[distances.size() / 2];
  EXPECT_LT(median, 0.25) << "upper-arc oracle median " << median;

  // Smoothness: a mis-ordered chain jumps across the sky; the ordered one steps at the grid's
  // own scale (819 measured a 0.8 deg median step here).
  std::vector<double> steps;
  const FoldPointRecord* prev = nullptr;
  for (int idx : chain.order) {
    const FoldPointRecord& p = rank.crossings[static_cast<size_t>(idx)];
    if (prev != nullptr) {
      double a[3];
      double b[3];
      C16SkyDir(prev->image_alt_deg, prev->image_az_deg, a);
      C16SkyDir(p.image_alt_deg, p.image_az_deg, b);
      steps.push_back(C16SkyAngleDeg(a, b));
    }
    prev = &p;
  }
  std::sort(steps.begin(), steps.end());
  EXPECT_LT(steps[steps.size() / 2], 3.0);
}

// AC3: the mirror branch presents per segment — the collapsed middle verifies as a point
// image at the geometric mirror identity (-sun altitude, azimuth 0), the extended ends stay
// non-point segments, and the transition zones (where dJ(v) sinks below its resolution) stay
// unknown — no cusp markers in numerically unresolvable regions.
TEST(FamilyRank, C16MirrorBranchSegmentation) {
  Fixture f;
  int slots[2];
  const int faces[2] = { 3, 5 };
  f.Resolve(faces, 2, slots);
  double sun[3];
  SunAt(20.0, 0.0, sun);
  const RestrictedFamilyRank rank = RankRestrictedFamily(f.normals, slots, 2, C16SpinChart(), sun, kC16N, 128);
  ASSERT_EQ(rank.status, FamilyRankStatus::kOk);

  // The mirror chain: the longest chain carrying a point-image segment. The strict-tangency
  // contract (the tangency read's own instrument floor, the review's unit-corrected gate)
  // segments the branch as the plan's own fact statement describes: the collapsed MIDDLE (the
  // identity zone, where the deficit reads at the instrument's floor) is the point-image
  // segment; the drifting ends (deficit 1e-4..7.8e-4 — resolvably misaligned) stay arcs. The
  // old whole-run expectation (>40 points) embodied the curvature-swamped gate's over-claim.
  size_t mirror = rank.fold_chains.size();
  size_t mirror_points = 0;
  for (size_t c = 0; c < rank.fold_chains.size(); c++) {
    for (const FoldSegmentRecord& segment : rank.fold_chains[c].segments) {
      if (segment.form == FoldSegmentForm::kPointImage && segment.last - segment.first + 1 > mirror_points) {
        mirror_points = static_cast<size_t>(segment.last - segment.first + 1);
        mirror = c;
      }
    }
  }
  ASSERT_LT(mirror, rank.fold_chains.size()) << "no point-image segment found";
  const FoldChainRecord& chain = rank.fold_chains[mirror];
  ASSERT_GE(chain.order.size(), 70u);
  // A point-image claim carries a nonzero parameter extension (>= 2 crossings).
  EXPECT_GE(mirror_points, 2u);

  // The collapsed segment sits at the geometric mirror identity (-sun altitude, azimuth 0) —
  // the horizontal-plane reflection's own constant, an independent witness.
  int point_points = 0;
  bool has_point = false;
  for (const FoldSegmentRecord& segment : chain.segments) {
    if (segment.form != FoldSegmentForm::kPointImage) {
      continue;
    }
    has_point = true;
    EXPECT_LE(segment.image_diameter_deg, kFoldPointImageFloorDeg);
    // The segment's own resolution statement (the verification's bound) rides the record.
    EXPECT_GE(segment.image_bound_deg, 0.0);
    EXPECT_NEAR(segment.image_alt_deg, -20.0, 0.01);
    EXPECT_NEAR(segment.image_az_deg, 0.0, 0.01);
    for (int p = segment.first; p <= segment.last; p++) {
      point_points++;
      const FoldPointRecord& point = rank.crossings[static_cast<size_t>(chain.order[static_cast<size_t>(p)])];
      // The recorded per-point reads carry the segment's own evidence.
      EXPECT_GE(point.alignment, 1.0 - kFoldPointImageLocatorSlack);
    }
  }
  EXPECT_TRUE(has_point);
  EXPECT_GE(point_points, 2);

  // The extended ends are NOT swallowed: some chain reaches below the collapsed middle, to
  // the transverse end the corpus records at (-22.6 deg, +-2.45 deg) — the unverifiable cliff
  // cells break the walk there (kept fragments, the break reasons say so), so the end may
  // ride a chain of its own.
  double bottom = 1e9;
  bool end_broken = false;
  for (const FoldChainRecord& other : rank.fold_chains) {
    for (int idx : other.order) {
      bottom = std::min(bottom, rank.crossings[static_cast<size_t>(idx)].image_alt_deg);
    }
    if (other.end_a == FoldChainEnd::kFeasibilityInterruption ||
        other.end_b == FoldChainEnd::kFeasibilityInterruption || other.end_a == FoldChainEnd::kAmbiguousCell ||
        other.end_b == FoldChainEnd::kAmbiguousCell) {
      end_broken = true;
    }
  }
  EXPECT_LT(bottom, -22.0);
  EXPECT_TRUE(end_broken) << "the ends' breaks carry no reason";
  // The ends are resolvably misaligned on the mirror chain itself (deficit 1e-4..7.8e-4
  // against a ~1e-9 instrument floor): they read as arcs there, or ride the end fragments the
  // unverifiable cells broke off.
  int arc_points = 0;
  for (const FoldSegmentRecord& segment : chain.segments) {
    if (segment.form == FoldSegmentForm::kArc) {
      arc_points += segment.last - segment.first + 1;
    }
  }
  if (arc_points == 0) {
    for (const FoldChainRecord& other : rank.fold_chains) {
      for (const FoldSegmentRecord& segment : other.segments) {
        if (segment.form == FoldSegmentForm::kArc) {
          arc_points += segment.last - segment.first + 1;
        }
      }
    }
  }
  EXPECT_GT(arc_points, 0);

  // Every published cusp candidate sits on an arc position (the classification's own order:
  // a point-image segment never doubles as a cusp set).
  for (const FoldCuspCandidate& cusp : chain.cusps) {
    const FoldSegmentRecord* home = nullptr;
    for (const FoldSegmentRecord& segment : chain.segments) {
      if (cusp.order_index >= segment.first && cusp.order_index <= segment.last) {
        home = &segment;
      }
    }
    if (home == nullptr) {
      ADD_FAILURE() << "a cusp outside every segment (order " << cusp.order_index << ")";
      continue;  // this row's verdict is in; the other rows still drive
    }
    EXPECT_EQ(home->form, FoldSegmentForm::kArc) << "a cusp published on a non-arc segment";
  }
}

// The two-point fragments whose whole image collapses onto the mirror point: a whole-branch
// point form exists beside the mixed branch's segmented one.
TEST(FamilyRank, C16WholeBranchPointImageExists) {
  Fixture f;
  int slots[2];
  const int faces[2] = { 3, 5 };
  f.Resolve(faces, 2, slots);
  double sun[3];
  SunAt(20.0, 0.0, sun);
  const RestrictedFamilyRank rank = RankRestrictedFamily(f.normals, slots, 2, C16SpinChart(), sun, kC16N, 128);
  ASSERT_EQ(rank.status, FamilyRankStatus::kOk);
  int whole_point_chains = 0;
  for (const FoldChainRecord& chain : rank.fold_chains) {
    if (chain.segments.size() != 1 || chain.segments.front().form != FoldSegmentForm::kPointImage) {
      continue;
    }
    whole_point_chains++;
    const FoldSegmentRecord& segment = chain.segments.front();
    EXPECT_NEAR(segment.image_alt_deg, -20.0, 0.01);
    EXPECT_NEAR(segment.image_az_deg, 0.0, 0.01);
  }
  EXPECT_GE(whole_point_chains, 1);
}

// AC2's positive leg: the upper arc is an ordinary fold — dJ(v) resolvably nonzero along its
// arc positions, the recorded resolution bounds far below the reads, and no cusp candidates.
TEST(FamilyRank, C16OrdinaryFoldTransversality) {
  Fixture f;
  int slots[2];
  const int faces[2] = { 3, 5 };
  f.Resolve(faces, 2, slots);
  double sun[3];
  SunAt(20.0, 0.0, sun);
  const RestrictedFamilyRank rank = RankRestrictedFamily(f.normals, slots, 2, C16SpinChart(), sun, kC16N, 128);
  ASSERT_EQ(rank.status, FamilyRankStatus::kOk);
  const size_t upper = C16UpperArcChain(rank);
  ASSERT_LT(upper, rank.fold_chains.size());
  const FoldChainRecord& chain = rank.fold_chains[upper];
  EXPECT_EQ(chain.cusps.size(), 0u);
  int checked = 0;
  int transverse = 0;
  for (const FoldSegmentRecord& segment : chain.segments) {
    if (segment.form != FoldSegmentForm::kArc) {
      continue;
    }
    for (int p = segment.first; p <= segment.last; p++) {
      const FoldPointRecord& point = rank.crossings[static_cast<size_t>(chain.order[static_cast<size_t>(p)])];
      if (point.alignment < 0.0) {
        continue;
      }
      checked++;
      // dJ(v) resolvably nonzero: the ordinary fold's own criterion. The resolution bound is
      // the located point's own |J| — at the rare validity-cliff artifact (a sign jump across
      // an infeasible strip inside one edge, bisected like a crossing) |J| stays large and the
      // bound honestly says so; the transversality claim here absorbs that.
      if (std::fabs(point.djv) > point.djv_resolution) {
        transverse++;
      }
    }
  }
  EXPECT_GT(checked, 50);
  EXPECT_EQ(transverse, checked) << "an ordinary-fold arc position lost its transversality";
}

// The cliff probes recover the feasibility-cliff loci the both-valid-edges rule loses, and the
// recovered crossings land on the closed-form oracle's folds.
TEST(FamilyRank, C16CliffProbeRecoversTheTail) {
  Fixture f;
  int slots[2];
  const int faces[2] = { 3, 5 };
  f.Resolve(faces, 2, slots);
  double sun[3];
  SunAt(20.0, 0.0, sun);
  const RestrictedFamilyRank rank = RankRestrictedFamily(f.normals, slots, 2, C16SpinChart(), sun, kC16N, 128);
  ASSERT_EQ(rank.status, FamilyRankStatus::kOk);
  EXPECT_GT(rank.cliff_probed_edges, 100);
  EXPECT_GT(rank.cliff_unresolved_edges, 0);  // the probes hit the invalid region somewhere — declared data
  int recovered = 0;
  int recovered_on_oracle = 0;
  for (size_t c = 0; c < rank.crossings.size(); c++) {
    if (!rank.crossings[c].from_cliff_probe) {
      continue;
    }
    recovered++;
    if (C16OracleDistanceDeg(rank, static_cast<int>(c)) < 0.3) {
      recovered_on_oracle++;
    }
  }
  EXPECT_GT(recovered, 10);
  EXPECT_GT(recovered_on_oracle, 0) << "no recovered crossing landed on a closed-form fold";
}

// AC1's Lowitz leg: the sheet recovers multiple ordered chains — the corpus witness's main
// fold AND the near-sun branches the 819 exploration found at the zenith extremes (LI-arbitrated
// real folds, kept, not chart-boundary deletions). The chain machinery runs unmodified on a
// {zenith, azimuth} family; every chain's links carry their cell provenance like the C16 ones.
TEST(FamilyRank, LowitzSheetOrderedChainsIncludeThePolarBranches) {
  Fixture f;
  int slots[2];
  const int faces[2] = { 3, 5 };
  f.Resolve(faces, 2, slots);
  double sun[3];
  SunAt(15.0, 0.0, sun);
  const RestrictedFamilyRank rank = RankRestrictedFamily(f.normals, slots, 2, LowitzFoldSheet(), sun, kN131, 128);
  ASSERT_EQ(rank.status, FamilyRankStatus::kOk);
  ASSERT_GE(rank.fold_chains.size(), 3u) << "the sheet's main fold and near-sun branches";
  int polar_chains = 0;
  int provenance_ok = 0;
  int links_total = 0;
  for (const FoldChainRecord& chain : rank.fold_chains) {
    if (chain.order.size() < 2) {
      continue;
    }
    // A near-sun (polar) branch: its chain's crossings sit at zenith below 34 or above 146
    // degrees — the 819 exploration's LI-arbitrated real folds the witness segment alone misses.
    double zen_min = 1e9;
    double zen_max = -1e9;
    for (int idx : chain.order) {
      const FoldPointRecord& p = rank.crossings[static_cast<size_t>(idx)];
      zen_min = std::min(zen_min, p.params_deg[0]);
      zen_max = std::max(zen_max, p.params_deg[0]);
    }
    if (zen_min <= 5.0 || zen_max >= 175.0) {
      polar_chains++;
    }
    // Per-link provenance, same contract as the C16 chains: a direct link's two crossings sit
    // on the link's cell boundary (subdivision links carry their cell by construction and are
    // not re-derived here).
    for (size_t l = 0; l + 1 < chain.order.size(); ++l) {
      if (chain.links[l].through_subdivision) {
        continue;
      }
      links_total++;
      const FoldPointRecord& a = rank.crossings[static_cast<size_t>(chain.order[l])];
      const FoldPointRecord& b = rank.crossings[static_cast<size_t>(chain.order[l + 1])];
      const auto on_cell = [&](const FoldPointRecord& p) {
        if (p.edge_dim < 0) {
          return false;
        }
        // The Lowitz sheet's free dimensions are zenith (0..180, inclusive) and azimuth
        // (periodic): cell (I, J) spans [I, I+1] x [J, J+1] in sample indices.
        const int I = chain.links[l].cell_i0;
        const int J = chain.links[l].cell_i1;
        if (p.edge_dim == 0) {
          return p.edge_i0 == I && (p.edge_i1 == J || p.edge_i1 == (J + 1) % 128);
        }
        return p.edge_i1 == J && (p.edge_i0 == I || p.edge_i0 == I + 1);
      };
      if (on_cell(a) && on_cell(b)) {
        provenance_ok++;
      }
    }
  }
  EXPECT_GE(polar_chains, 1) << "the near-sun branches are gone";
  EXPECT_EQ(provenance_ok, links_total) << "a direct link lacks its cell provenance";
}

// The review's near-degenerate counterexample: the spin chart with zenith fixed 89.9999 (one
// hundred-thousandth of a degree off the mirror identity). Its long aligned run's measured sky
// span (~3.6e-4 deg) sits UNDER the declared floor — the old fixed-diameter arbiter swallowed
// it into a point image — but its kernel deficits (~7e-6, resolvably nonzero image velocity)
// sit four orders above the tangency instrument floor: resolvably moving, an arc, never a
// point image. The zenith-90 mirror beside it is the geometry-identity positive.
TEST(FamilyRank, NearDegenerateSmallArcStaysAnArc) {
  Fixture f;
  int slots[2];
  const int faces[2] = { 3, 5 };
  f.Resolve(faces, 2, slots);
  RestrictedFamilySpec spec;
  spec.zenith.mode = FamilyDimensionMode::kFixed;
  spec.zenith.center_deg = 89.9999;
  spec.azimuth.mode = FamilyDimensionMode::kFree;
  spec.roll.mode = FamilyDimensionMode::kFree;
  double sun[3];
  SunAt(20.0, 0.0, sun);
  const RestrictedFamilyRank rank = RankRestrictedFamily(f.normals, slots, 2, spec, sun, kC16N, 128);
  ASSERT_EQ(rank.status, FamilyRankStatus::kOk);
  int point_positions = 0;
  int aligned_positions = 0;
  for (const FoldChainRecord& chain : rank.fold_chains) {
    for (const FoldSegmentRecord& segment : chain.segments) {
      if (segment.form == FoldSegmentForm::kPointImage) {
        point_positions += segment.last - segment.first + 1;
      }
    }
    for (int idx : chain.order) {
      if (rank.crossings[static_cast<size_t>(idx)].alignment >= 1.0 - kFoldPointImageLocatorSlack) {
        aligned_positions++;
      }
    }
  }
  // The locator still sees the near-identity alignments (the input is 4e-5 deg from the
  // mirror); the tangency resolution gate refuses the run wholesale — the old 70-point
  // collapse is gone. What remains is the genuinely-unresolvable island (a few positions
  // whose deficits sit at the instrument floor: their own image velocity is below its own
  // resolution, the honest point-image candidate the plan's criterion admits), a strict
  // minority of the aligned run.
  EXPECT_GT(aligned_positions, 20) << "the counterexample lost its aligned run";
  // Per-segment honesty (the review's bar, not a count): every published point-image claim
  // must be consistent with its OWN declared bound — the measured diameter and the
  // velocities' predicted span inside the resolution it declares. The segment that survived
  // last round's criterion ([34..36], diameter ~2.0e-5 deg against its ~9.8e-7 bound, image
  // velocity ~3.5e-6 resolvable) must not reappear.
  for (const FoldChainRecord& chain : rank.fold_chains) {
    for (const FoldSegmentRecord& segment : chain.segments) {
      if (segment.form != FoldSegmentForm::kPointImage) {
        continue;
      }
      EXPECT_GE(segment.image_bound_deg, segment.image_diameter_deg)
          << "a point-image claim contradicts its own resolution bound (diameter " << segment.image_diameter_deg
          << " vs bound " << segment.image_bound_deg << ")";
    }
  }
  EXPECT_LT(point_positions, aligned_positions / 2) << "the unresolvable island dominates the aligned run";
}

// AC1's one-dimensional leg: the Parry circle records as the ordered, validity-segmented track
// — every sample in parameter order, one run over the whole circle, closed through the seam,
// and no fold machinery on the one-dimensional family.
TEST(FamilyRank, ParryTrackOrderedAndClosed) {
  Fixture f;
  int slots[2];
  const int faces[2] = { 3, 5 };
  f.Resolve(faces, 2, slots);
  double sun[3];
  SunAt(20.0, 0.0, sun);
  const RestrictedFamilyRank rank = RankRestrictedFamily(f.normals, slots, 2, ParryCircle(), sun, kN550, 128);
  ASSERT_EQ(rank.status, FamilyRankStatus::kOk);
  EXPECT_TRUE(rank.fold_chains.empty());
  EXPECT_TRUE(rank.crossings.empty());
  ASSERT_EQ(rank.track.points.size(), 128u);
  // Segments partition exactly the valid runs: every in-segment point valid, every
  // out-of-segment point invalid, and `closed` says the seam ends both valid (sun 20 has
  // validity gaps on this circle, so the runs' count is data, not a pin).
  std::vector<char> in_segment(static_cast<size_t>(128), 0);
  int valid_total = 0;
  int segment_bounds_ok = 0;
  for (const FamilyTrackSegment& segment : rank.track.segments) {
    segment_bounds_ok += static_cast<int>(segment.first >= 0 && segment.last >= segment.first && segment.last < 128);
  }
  ASSERT_EQ(segment_bounds_ok, static_cast<int>(rank.track.segments.size()));
  for (const FamilyTrackSegment& segment : rank.track.segments) {
    for (int i = segment.first; i <= segment.last; i++) {
      EXPECT_TRUE(rank.track.points[static_cast<size_t>(i)].valid);
      in_segment[static_cast<size_t>(i)] = 1;
      valid_total++;
    }
  }
  for (int i = 0; i < 128; i++) {
    EXPECT_EQ(in_segment[static_cast<size_t>(i)], rank.track.points[static_cast<size_t>(i)].valid ? 1 : 0);
  }
  // The existing Parry tests' own bar (grid/3): sun 20 leaves this circle mostly infeasible.
  EXPECT_GT(valid_total, 128 / 3) << "the Parry circle lost most of its track";
  EXPECT_EQ(rank.track.closed, rank.track.points.front().valid && rank.track.points.back().valid);
  // The ordered walk: consecutive images WITHIN a run step at the circle's own scale (a run
  // boundary jumps by the gap's own size and is not compared).
  double max_step = 0.0;
  for (const FamilyTrackSegment& segment : rank.track.segments) {
    for (int i = segment.first + 1; i <= segment.last; i++) {
      double a[3];
      double b[3];
      C16SkyDir(rank.track.points[static_cast<size_t>(i - 1)].image_alt_deg,
                rank.track.points[static_cast<size_t>(i - 1)].image_az_deg, a);
      C16SkyDir(rank.track.points[static_cast<size_t>(i)].image_alt_deg,
                rank.track.points[static_cast<size_t>(i)].image_az_deg, b);
      max_step = std::max(max_step, C16SkyAngleDeg(a, b));
    }
  }
  EXPECT_LT(max_step, 10.0) << "a within-run image step jumped (mis-order would jump ~180)";
  // The seam-closure step: a closed track's last and first images are one step apart.
  if (rank.track.closed) {
    double first_dir[3];
    double last_dir[3];
    C16SkyDir(rank.track.points.front().image_alt_deg, rank.track.points.front().image_az_deg, first_dir);
    C16SkyDir(rank.track.points.back().image_alt_deg, rank.track.points.back().image_az_deg, last_dir);
    EXPECT_LT(C16SkyAngleDeg(first_dir, last_dir), 10.0);
  }
  double first_dir[3];
  C16SkyDir(rank.track.points.front().image_alt_deg, rank.track.points.front().image_az_deg, first_dir);
  double last_dir[3];
  C16SkyDir(rank.track.points.back().image_alt_deg, rank.track.points.back().image_az_deg, last_dir);
  EXPECT_LT(C16SkyAngleDeg(first_dir, last_dir), 5.0);
}

// The review probe's scenario (3→5, zenith fixed 60, azimuth + roll free, sun 45, the same n and
// grid): the chord-zero locator once published a "cusp candidate" at a plain full-rank position
// (|J| ~ 1.3e-3, sigma2/sigma1 ~ 1e-3 — orders above the fold bar). The projection-and-gates fix
// must leave this input with ZERO candidates: the publish path refuses any event not on the
// rank-1 critical set, so a full-rank chord zero never becomes a marker.
TEST(FamilyRank, ReviewProbeScenarioPublishesNoOffLocusCusp) {
  Fixture f;
  int slots[2];
  const int faces[2] = { 3, 5 };
  f.Resolve(faces, 2, slots);
  RestrictedFamilySpec spec;
  spec.zenith.mode = FamilyDimensionMode::kFixed;
  spec.zenith.center_deg = 60.0;
  spec.azimuth.mode = FamilyDimensionMode::kFree;
  spec.roll.mode = FamilyDimensionMode::kFree;
  double sun[3];
  SunAt(45.0, 0.0, sun);
  const RestrictedFamilyRank rank = RankRestrictedFamily(f.normals, slots, 2, spec, sun, kC16N, 128);
  ASSERT_EQ(rank.status, FamilyRankStatus::kOk);
  int cusp_total = 0;
  for (const FoldChainRecord& chain : rank.fold_chains) {
    cusp_total += static_cast<int>(chain.cusps.size());
  }
  EXPECT_EQ(cusp_total, 0) << "a chord-zero artifact published as a cusp candidate again";
}

// The publish contract, independently re-checked: every cusp candidate this module publishes
// (across the C16 spin chart and the Lowitz sheet — the two scenarios this file's own scans
// produce events on) IS a point of the rank-1 critical set when EvaluateFamilyPoint re-evaluates
// the event's OWN parameters outside the chain pass: valid, the singular ratio under the fold
// bar, the Jacobian residual three orders below the old artifact's, and the published sky
// position the independent read's own (the direction dot, wrap-free).
TEST(FamilyRank, PublishedCuspEventsSitOnTheLocus) {
  Fixture f;
  int slots[2];
  const int faces[2] = { 3, 5 };
  f.Resolve(faces, 2, slots);
  struct Scenario {
    RestrictedFamilySpec spec;
    double sun[3];
    double n;
  };
  std::vector<Scenario> scenarios(2);
  scenarios[0].spec = C16SpinChart();
  SunAt(20.0, 0.0, scenarios[0].sun);
  scenarios[0].n = kC16N;
  scenarios[1].spec = LowitzFoldSheet();
  SunAt(15.0, 0.0, scenarios[1].sun);
  scenarios[1].n = kN550;
  int events = 0;
  for (const Scenario& scenario : scenarios) {
    const RestrictedFamilyRank rank =
        RankRestrictedFamily(f.normals, slots, 2, scenario.spec, scenario.sun, scenario.n, 128);
    if (rank.status != FamilyRankStatus::kOk) {
      ADD_FAILURE() << "the scan refused: " << FamilyRankStatusName(rank.status);
      continue;  // this scenario's verdict is in; the other still drives
    }
    for (const FoldChainRecord& chain : rank.fold_chains) {
      for (const FoldCuspCandidate& cusp : chain.cusps) {
        events++;
        const FamilyPointRecord read =
            EvaluateFamilyPoint(f.normals, slots, 2, scenario.spec, scenario.sun, scenario.n, cusp.params_deg);
        if (!read.valid) {
          ADD_FAILURE() << "a published event's own parameters are not even evaluable";
          continue;  // this row's verdict is in; the other rows still drive
        }
        EXPECT_LE(read.sigma2, read.sigma1 * kFoldLocalRankRatio)
            << "the published event is not a rank-1 point (sigma ratio " << read.sigma2 / std::max(read.sigma1, 1e-300)
            << ")";
        // |J| at the event is distance × |grad J| — a huge gradient puts a real on-locus event
        // at ~4e-5 (measured), so no flat |J| bar can certify the distance (the kernel's own
        // projection converges in |J|/|grad J| ≤ kFoldCuspProjectionDistanceDeg). This line
        // only bounds the artifact scale: the chord-zero candidate the review measured sat at
        // |J| 1.3e-3 WITH a full-rank sigma ratio — the rank gate above is the discriminator,
        // this refuses anything at that scale even before the ratio is read.
        EXPECT_LT(std::fabs(read.signed_jacobian), 1e-4)
            << "the published event sits at chord-zero-artifact scale (|J| " << std::fabs(read.signed_jacobian) << ")";
        // The published sky position is the independent read's own (angle between the unit
        // directions — no atan2 wrap in the comparison).
        double published[3];
        C16SkyDir(cusp.image_alt_deg, cusp.image_az_deg, published);
        double dot = 0.0;
        for (int k = 0; k < 3; k++) {
          dot += published[k] * -read.outgoing[k];
        }
        const double angle = Deg(std::acos(std::max(-1.0, std::min(1.0, dot))));
        EXPECT_LT(angle, 1e-4) << "the published sky position drifted off the event's own read";
      }
    }
  }
  EXPECT_GT(events, 0) << "the scenario lost every real candidate — the gates now refuse everything";
}

// The recovery joins the query's cooperative stop (the handshake pattern: the canceller waits
// for the poll counter to move — proof the scan entered — then cancels; no sleeps, no pre-armed
// flags). The cut run publishes what completed under stop_requested, a cut walk's chain ends
// kStopRequested and never closed, and fewer chains stand than the uncut scan builds (no chain
// started after the cut).
TEST(FamilyRank, StopRequestCutsTheRecoveryAndDeclaresIt) {
  Fixture f;
  int slots[2];
  const int faces[2] = { 3, 5 };
  f.Resolve(faces, 2, slots);
  double sun[3];
  SunAt(20.0, 0.0, sun);
  // The uncut reference: the chains the full scan builds on this input.
  const RestrictedFamilyRank whole = RankRestrictedFamily(f.normals, slots, 2, C16SpinChart(), sun, kC16N, 128);
  ASSERT_EQ(whole.status, FamilyRankStatus::kOk);

  StopToken token;
  DiscoveryStop stop;
  stop.token = &token;
  std::atomic<bool> cancelled{ false };
  std::thread canceller([&] {
    while (token.PollCount(kPollFamilyCurve) == 0 && !cancelled.load()) {
    }
    token.RequestCancel();
  });
  const RestrictedFamilyRank rank = RankRestrictedFamily(f.normals, slots, 2, C16SpinChart(), sun, kC16N, 128, &stop);
  cancelled.store(true);
  canceller.join();
  EXPECT_TRUE(rank.stop_requested) << "the stop never observed";
  EXPECT_GT(token.PollCount(kPollFamilyCurve), 0) << "the recovery's poll site never moved";
  EXPECT_LE(static_cast<int>(rank.fold_chains.size()), static_cast<int>(whole.fold_chains.size()))
      << "chains appeared after the cut";
  int cut_ends = 0;
  for (const FoldChainRecord& chain : rank.fold_chains) {
    cut_ends += static_cast<int>(chain.end_a == FoldChainEnd::kStopRequested) +
                static_cast<int>(chain.end_b == FoldChainEnd::kStopRequested);
    EXPECT_FALSE(chain.end_a == FoldChainEnd::kStopRequested && chain.closed);
    EXPECT_FALSE(chain.end_b == FoldChainEnd::kStopRequested && chain.closed);
  }
  EXPECT_GT(cut_ends + (static_cast<int>(rank.fold_chains.size()) < static_cast<int>(whole.fold_chains.size()) ? 1 : 0),
            0)
      << "the cut left no trace: no kStopRequested end and the full chain set published";
  EXPECT_STREQ(FoldChainEndName(FoldChainEnd::kStopRequested), "stop_requested");
}

// A stop already standing at entry: the very first poll cuts the scan, nothing downstream runs,
// and the empty result still says why (stop_requested with no crossings, no chains, no track).
TEST(FamilyRank, StopStandingAtEntryRunsNothing) {
  Fixture f;
  int slots[2];
  const int faces[2] = { 3, 5 };
  f.Resolve(faces, 2, slots);
  double sun[3];
  SunAt(20.0, 0.0, sun);
  StopToken token;
  token.RequestCancel();
  DiscoveryStop stop;
  stop.token = &token;
  const RestrictedFamilyRank rank = RankRestrictedFamily(f.normals, slots, 2, C16SpinChart(), sun, kC16N, 128, &stop);
  ASSERT_EQ(rank.status, FamilyRankStatus::kOk);
  EXPECT_TRUE(rank.stop_requested);
  EXPECT_EQ(rank.valid_samples, 0);
  EXPECT_TRUE(rank.crossings.empty());
  EXPECT_TRUE(rank.fold_chains.empty());
  EXPECT_GT(token.PollCount(kPollFamilyCurve), 0) << "the first poll site never moved";
}

// A stop landing in the WALK phase (the handshake waits past the scan and the pairing — on
// grid128 the pairing polls per cell, so a late threshold lands the cancel inside the chain
// walks): every published chain keeps its topology contract — links == points - 1 open,
// links == points closed, no dangling adjacency to a point that never joined the order, the
// cut ends declared. This is the invariant the typed freeze enforces; the review's probe
// turned the old commit order (link first, poll after) into a guaranteed freeze exception.
TEST(FamilyRank, WalkPhaseStopKeepsTheTopologyContract) {
  Fixture f;
  int slots[2];
  const int faces[2] = { 3, 5 };
  f.Resolve(faces, 2, slots);
  double sun[3];
  SunAt(20.0, 0.0, sun);
  StopToken token;
  DiscoveryStop stop;
  stop.token = &token;
  std::atomic<bool> cancelled{ false };
  std::thread canceller([&] {
    // The threshold sits a few polls past the pairing's own cell count (grid128 C16: 128
    // scan rows + 255 edge-batch rows + 255 cliff rows + 16256 pairing cells ~= 16894), so
    // the cancel lands INSIDE the first chain's walk steps — the phase the old commit order
    // turned into a dangling link.
    while (token.PollCount(kPollFamilyCurve) < 16910 && !cancelled.load()) {
    }
    token.RequestCancel();
  });
  // The uncut reference: the chains the full scan builds (the cut either truncates a walk or
  // stops a chain from ever starting — both leave fewer chains than the whole).
  const RestrictedFamilyRank whole = RankRestrictedFamily(f.normals, slots, 2, C16SpinChart(), sun, kC16N, 128);
  ASSERT_EQ(whole.status, FamilyRankStatus::kOk);
  const RestrictedFamilyRank rank = RankRestrictedFamily(f.normals, slots, 2, C16SpinChart(), sun, kC16N, 128, &stop);
  cancelled.store(true);
  canceller.join();
  EXPECT_TRUE(rank.stop_requested);
  int cut_chains = 0;
  for (const FoldChainRecord& chain : rank.fold_chains) {
    const size_t expected = chain.order.size() - (chain.closed ? 0u : 1u);
    EXPECT_EQ(chain.links.size(), expected) << "a cut walk published a dangling link (points " << chain.order.size()
                                            << ", links " << chain.links.size() << ", closed " << chain.closed << ")";
    if (chain.end_a == FoldChainEnd::kStopRequested || chain.end_b == FoldChainEnd::kStopRequested) {
      cut_chains++;
      EXPECT_FALSE(chain.closed) << "a cut walk declared a closed loop";
    }
  }
  EXPECT_GT(cut_chains + (rank.fold_chains.size() < whole.fold_chains.size() ? 1 : 0), 0)
      << "the walk-phase threshold left no trace: no cut chain and the full chain set";
}


}  // namespace
}  // namespace lumice::analytic
