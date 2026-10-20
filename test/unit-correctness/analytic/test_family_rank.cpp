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
#include <cmath>
#include <string>
#include <vector>

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


}  // namespace
}  // namespace lumice::analytic
