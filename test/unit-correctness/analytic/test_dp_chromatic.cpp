// The chromatic layer (src/analytic/dp_chromatic.hpp, LI chromatic.py ported for scrum 660.4)
// against the fixture-caliber anchors: the dark hole's blue rim (3-1-6), the C02 criterion-layer
// split (3-1-5: its kink edge is blue and visible while its exit gate is red and not visible),
// the declared threshold snapshot, and the rhombic-plate tints of the three class cells. The
// statistical tolerances (5e-2) are the fixture caliber: the family sampler's stream is this
// implementation's own, and the assertion bounds LI's split-half sigma by more than a factor of
// fifteen — stream equality is not a target (the header's seed note).
//
// symmetry_semantics: L1 for the class cells — the members are the PBD label orbit, which on the
// rhombic plate deliberately includes paths the crystal cannot realise (the impossible-literal
// cell's point).

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

#include "analytic/dp_chromatic.hpp"
#include "analytic/path_evaluation.hpp"

namespace lumice::analytic {
namespace {

constexpr double kPi = 3.14159265358979323846;

double Deg(double rad) {
  return rad * 180.0 / kPi;
}

// The chromatic fixtures' crystal: PRISM_H1 = prism_crystal(0.5), the h/a = 1 column.
LUMICE_ANALYTIC_Crystal PrismH1() {
  LUMICE_ANALYTIC_Crystal c{};
  c.kind = LUMICE_ANALYTIC_CRYSTAL_PRISM;
  c.height = 0.5;
  for (int i = 0; i < 6; i++) {
    c.face_distance[i] = 1.0;
  }
  return c;
}

// The rhombic plate of the class cells: fd = [1.5, 1, 1, 1.5, 1, 1] at h = 0.5.
LUMICE_ANALYTIC_Crystal RhombicPlate() {
  LUMICE_ANALYTIC_Crystal c{};
  c.kind = LUMICE_ANALYTIC_CRYSTAL_PRISM;
  c.height = 0.5;
  const double fd[6] = { 1.5, 1.0, 1.0, 1.5, 1.0, 1.0 };
  for (int i = 0; i < 6; i++) {
    c.face_distance[i] = fd[i];
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

const ChromaticFeature* FeatureOf(const ChromaticVerdict& verdict, ChromaticFeatureKind kind,
                                  const std::string& source) {
  for (const ChromaticFeature& feature : verdict.features) {
    if (feature.kind == kind && feature.source == source) {
      return &feature;
    }
  }
  return nullptr;
}

// ---------------------------------------------------------------------------------------------
// The declared thresholds (AC3: the parameters travel with the output)
// ---------------------------------------------------------------------------------------------

TEST(DPChromatic, ThresholdSnapshotCarriesTheConstants) {
  const ChromaticThresholds thresholds = ChromaticThresholdsSnapshot();
  EXPECT_DOUBLE_EQ(thresholds.n_red, kNRed);
  EXPECT_DOUBLE_EQ(thresholds.n_blue, kNBlue);
  EXPECT_DOUBLE_EQ(thresholds.edge_min_shift_rad, kEdgeMinShiftRad);
  EXPECT_DOUBLE_EQ(thresholds.edge_spread_per_shift, kEdgeSpreadPerShift);
  EXPECT_DOUBLE_EQ(thresholds.calibration_white_max_deviation, kCalibrationWhiteMaxDeviation);
  EXPECT_DOUBLE_EQ(thresholds.tint_ratio_min, kTintRatioMin);
  // The declared values themselves: the solar-disc floor and the twice-the-calibration tint band.
  EXPECT_NEAR(thresholds.edge_min_shift_rad, 0.5 * kPi / 180.0, 0.0);
  EXPECT_NEAR(thresholds.tint_ratio_min, 1.10, 1e-12);
}

// ---------------------------------------------------------------------------------------------
// Random orientation (Step 4)
// ---------------------------------------------------------------------------------------------

// The dark hole's rim (the fixture's 3-1-6 cell): one edge feature, blue, visible, sitting on
// D = 2 asin sqrt(n_blue^2 - 1) = 117.996 deg with shift 3.354 deg and spread 0 (the whole onset
// circle is one level set of the mirror slab), no gates moving, coverage complete.
TEST(DPChromatic, DarkHoleRimOfThreeOneSix) {
  const Fixture f(PrismH1());
  const int faces[3] = { 3, 1, 6 };
  int slots[3];
  f.Resolve(faces, 3, slots);
  const ChromaticVerdict verdict = Diagnose(f.normals, f.polygons, slots, 3);
  EXPECT_EQ(verdict.kind, ChromaticVerdictKind::kEdge);
  EXPECT_EQ(verdict.color, ChromaticColor::kBlue);
  EXPECT_TRUE(verdict.visible);
  EXPECT_TRUE(verdict.coverage_complete);
  EXPECT_TRUE(verdict.notes.empty());
  ASSERT_EQ(verdict.features.size(), 1u);
  const ChromaticFeature& edge = verdict.features[0];
  EXPECT_EQ(edge.kind, ChromaticFeatureKind::kEdge);
  EXPECT_EQ(edge.source, "internal_1_tir_discriminant");
  EXPECT_EQ(edge.color, ChromaticColor::kBlue);
  EXPECT_TRUE(edge.visible);
  EXPECT_DOUBLE_EQ(edge.positive_fraction, 1.0);
  const double closed_form = 2.0 * std::asin(std::sqrt(kNBlue * kNBlue - 1.0));
  EXPECT_NEAR(edge.delta_blue, closed_form, 2e-3);
  EXPECT_NEAR(Deg(edge.shift), 3.354, 2e-3);  // the median D difference, rad tolerance in deg
  EXPECT_LE(edge.spread, 1e-9);               // sigma = 0: one level set
  EXPECT_NEAR(edge.direction_dispersion, 0.0, 1e-12);
  EXPECT_TRUE(verdict.has_position);
  EXPECT_NEAR(verdict.position, closed_form, 2e-3);
}

// The C02 criterion layer (the fixture's 3-1-5 cell): the kink edge is blue and visible (shift
// 7.325 deg over spread 4.878 deg) while the exit gate is red and NOT visible (spread 108.65 deg
// against |shift| 0.33 deg — a slope corner, its colour spread). The issue's expected "red gate"
// is exactly this divergence, recorded in LI chromatic-module-c.md's Evidence.
TEST(DPChromatic, ThreeOneFiveKinkVisibleGateNot) {
  const Fixture f(PrismH1());
  const int faces[3] = { 3, 1, 5 };
  int slots[3];
  f.Resolve(faces, 3, slots);
  const ChromaticVerdict verdict = Diagnose(f.normals, f.polygons, slots, 3);
  EXPECT_EQ(verdict.kind, ChromaticVerdictKind::kEdge);
  EXPECT_EQ(verdict.color, ChromaticColor::kBlue);
  EXPECT_TRUE(verdict.visible);
  ASSERT_EQ(verdict.features.size(), 2u);
  const ChromaticFeature* edge = FeatureOf(verdict, ChromaticFeatureKind::kEdge, "internal_1_tir_discriminant");
  ASSERT_NE(edge, nullptr);
  EXPECT_EQ(edge->color, ChromaticColor::kBlue);
  EXPECT_TRUE(edge->visible);
  EXPECT_NEAR(Deg(edge->shift), 7.325, 0.05);
  EXPECT_NEAR(Deg(edge->spread), 4.878, 0.05);
  const ChromaticFeature* gate = FeatureOf(verdict, ChromaticFeatureKind::kGateEdge, "exit_snell_discriminant");
  ASSERT_NE(gate, nullptr);
  EXPECT_EQ(gate->color, ChromaticColor::kRed);
  EXPECT_FALSE(gate->visible);
  EXPECT_NEAR(Deg(gate->shift), -0.327, 0.02);
  EXPECT_NEAR(Deg(gate->spread), 108.65, 0.2);
}

// A one-sided line without assessed features is kUnresolved, never a silent kNone (LI's own test
// of the roll-up; no small real path flips its kink onset across any index pair on the prism, so
// the shape is asserted where LI asserts it).
TEST(DPChromatic, OneSidedLineIsUnresolvedNotNone) {
  const std::vector<int> faces = { 3, 1, 5 };
  const ChromaticVerdict unresolved =
      VerdictOf(faces, {}, { "internal_1_tir_discriminant: weight kink at n = 1.317 only (not assessed)" }, 1.307,
                1.317, true, true);
  EXPECT_EQ(unresolved.kind, ChromaticVerdictKind::kUnresolved);
  EXPECT_EQ(unresolved.color, ChromaticColor::kNone);
  EXPECT_FALSE(unresolved.visible);
  EXPECT_FALSE(unresolved.has_position);
  // Without the one-sided note the same empty feature set is a plain kNone (LI's second assert).
  const ChromaticVerdict none = VerdictOf(faces, {}, {}, 1.307, 1.317, false, true);
  EXPECT_EQ(none.kind, ChromaticVerdictKind::kNone);
}

// ---------------------------------------------------------------------------------------------
// The class verdicts (Step 5)
// ---------------------------------------------------------------------------------------------

// The 120-deg parhelion tint on the rhombic plate (the fixture's 1-3-5-2 cell): tint blue at
// ratio 1.492 with four of the 24 members lit at both indices, direction dispersion at the float
// zero (a slab-like class), tir fractions near the fixture's 0.44 / 0.54.
TEST(DPChromatic, ParhelionTintBlueOnRhombicPlate) {
  const Fixture f(RhombicPlate());
  const int representative[4] = { 1, 3, 5, 2 };
  const PlateFamilySpec family;  // sun 9 deg, sigma 1 deg, 1e5 poses, seed 3
  const ClassVerdict verdict = DiagnoseClass(f.normals, f.polygons, representative, 4, family);
  ASSERT_EQ(verdict.members.size(), 24u);
  EXPECT_EQ(verdict.verdict.kind, ChromaticVerdictKind::kTint);
  EXPECT_EQ(verdict.verdict.color, ChromaticColor::kBlue);
  EXPECT_TRUE(verdict.verdict.visible);
  EXPECT_TRUE(verdict.verdict.has_tint);
  ASSERT_EQ(verdict.lit_members_red.size(), 4u);
  ASSERT_EQ(verdict.lit_members_blue.size(), 4u);
  EXPECT_NEAR(verdict.verdict.tint.ratio, 1.492, 5e-2);
  EXPECT_NEAR(verdict.verdict.tint.tir_fraction_red, 0.439, 1e-2 + 5e-2 * 0.439);
  EXPECT_NEAR(verdict.verdict.tint.tir_fraction_blue, 0.537, 1e-2 + 5e-2 * 0.537);
  EXPECT_LE(verdict.verdict.tint.direction_dispersion, 1e-9);
  EXPECT_TRUE(verdict.verdict.notes.empty());

  // The self-convergence probe (the fixture caliber's own justification): halving the sample
  // moves the ratio by far less than the statistical tolerance, so the 5e-2 band above measures
  // the criterion, not this implementation's sampling noise.
  PlateFamilySpec half = family;
  half.samples = family.samples / 2;
  const ClassVerdict half_verdict = DiagnoseClass(f.normals, f.polygons, representative, 4, half);
  EXPECT_LT(std::fabs(half_verdict.verdict.tint.ratio - verdict.verdict.tint.ratio), 5e-2);
}

// The white control (the fixture's 1-3-4-2 cell): a class whose internal reflections stay partial
// across the index pair stays within the calibration band — ratio 0.965, verdict none / white,
// not visible.
TEST(DPChromatic, WhiteControlTintOnRhombicPlate) {
  const Fixture f(RhombicPlate());
  const int representative[4] = { 1, 3, 4, 2 };
  const PlateFamilySpec family;
  const ClassVerdict verdict = DiagnoseClass(f.normals, f.polygons, representative, 4, family);
  ASSERT_EQ(verdict.members.size(), 24u);
  EXPECT_EQ(verdict.verdict.kind, ChromaticVerdictKind::kNone);
  EXPECT_EQ(verdict.verdict.color, ChromaticColor::kWhite);
  EXPECT_FALSE(verdict.verdict.visible);
  EXPECT_NEAR(verdict.verdict.tint.ratio, 0.965, 5e-2);
}

// The impossible-literal class (the fixture's 3-5-6-8 cell): the literal representative is
// geometrically impossible on this plate — unlit on the whole sample — and four other members of
// its PBD orbit carry the class to a white verdict at ratio 1.029. The label orbit does the
// carrying, not the literal sequence. A side-only sequence has twelve images (the top-bottom
// swap fixes prism sides, so D6 alone enumerates the orbit).
TEST(DPChromatic, ImpossibleLiteralClassCarriedByOrbit) {
  const Fixture f(RhombicPlate());
  const int representative[4] = { 3, 5, 6, 8 };
  const PlateFamilySpec family;
  const ClassVerdict verdict = DiagnoseClass(f.normals, f.polygons, representative, 4, family);
  ASSERT_EQ(verdict.members.size(), 12u);
  EXPECT_EQ(verdict.verdict.kind, ChromaticVerdictKind::kNone);
  EXPECT_EQ(verdict.verdict.color, ChromaticColor::kWhite);
  EXPECT_FALSE(verdict.verdict.visible);
  EXPECT_NEAR(verdict.verdict.tint.ratio, 1.029, 5e-2);
  ASSERT_EQ(verdict.lit_members_red.size(), 4u);
  ASSERT_EQ(verdict.lit_members_blue.size(), 4u);
  // The literal member is in the orbit (labels) but in neither lit list.
  const std::vector<int> literal = { 3, 5, 6, 8 };
  for (const std::vector<int>& lit : verdict.lit_members_red) {
    EXPECT_NE(lit, literal);
  }
}

}  // namespace
}  // namespace lumice::analytic
