// The U_P partition's topology half (src/analytic/dp_partition.hpp, LI certificate's
// domain_topology / chart_audit of task 52.7): the escape-regime name table, the audit verdict
// roll-up, the chart mask's 4-connected counts on synthetic masks, and the thin-neck integration
// anchor — 3-5-6-7 at n = 1.31 whose lattice count of 2 is an artefact the chart audit corrects
// to a disk. Every threshold here is an assertion constant (cross-ISA discipline); the
// implementation carries no tolerance.
//
// symmetry_semantics: none — no symmetry reduction is involved.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "analytic/dp_partition.hpp"
#include "analytic/path_evaluation.hpp"

namespace lumice::analytic {
namespace {

constexpr double kN131 = 1.31;  // LI's default ice index (the 52.7 fixtures' n)
constexpr double kPi = 3.14159265358979323846;

LUMICE_ANALYTIC_Crystal Prism() {
  LUMICE_ANALYTIC_Crystal c{};
  c.kind = LUMICE_ANALYTIC_CRYSTAL_PRISM;
  c.height = 1.0;
  for (int i = 0; i < 6; i++) {
    c.face_distance[i] = 1.0;
  }
  return c;
}

// The beta crystal of the 52.x fixtures: fd = [2, 1, 1, 2, 1, 1], h = 3.
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

struct Fixture {
  FaceNormalTable normals;
  FacePolygonTable polygons;

  explicit Fixture(const LUMICE_ANALYTIC_Crystal& crystal) {
    EXPECT_EQ(BuildFaceNormals(crystal, &normals, &polygons), Status::kOk);
  }

  DeviationField Field(const int* faces, int count, double n) const {
    int slots[kMaxFaceCount];
    EXPECT_EQ(ResolveFaceSequence(normals, faces, count, slots), Status::kOk);
    return DeviationField(normals, polygons, slots, count, n);
  }
};

// ---------------------------------------------------------------------------------------------
// The name tables
// ---------------------------------------------------------------------------------------------

// AC1's ground stone: every regime has a name and no two share one (the report side and the
// tests match on these slugs; a merge here would be a silent regime collapse).
TEST(DPPartition, EscapeRegimeNamesAreStableAndDistinct) {
  const EscapeRegime all[] = {
    EscapeRegime::kNotDiskUnaudited,
    EscapeRegime::kNotDiskUnconverged,
    EscapeRegime::kNotDiskConfirmed,
    EscapeRegime::kNotDiskCorrected,
    EscapeRegime::kSlabCreaseContradiction,
    EscapeRegime::kSlabCreaseNotCarried,
    EscapeRegime::kSlabCreaseTouching,
    EscapeRegime::kSlabCreaseClosedRidge,
    EscapeRegime::kMultipleInteriorCriticalPoints,
    EscapeRegime::kLoopExtremaNotAlternating,
    EscapeRegime::kOddBoundaryCrossings,
    EscapeRegime::kInteriorCriticalPointNotSimple,
    EscapeRegime::kSublevelNotReachingBoundary,
  };
  std::vector<std::string> names;
  for (EscapeRegime regime : all) {
    const char* name = EscapeRegimeName(regime);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "unknown_escape_regime");
    names.emplace_back(name);
  }
  std::vector<std::string> sorted = names;
  std::sort(sorted.begin(), sorted.end());
  EXPECT_EQ(std::adjacent_find(sorted.begin(), sorted.end()), sorted.end()) << "names are pairwise distinct";
  EXPECT_STREQ(AuditVerdictName(AuditVerdict::kConfirmed), "confirmed");
  EXPECT_STREQ(AuditVerdictName(AuditVerdict::kCorrected), "corrected");
  EXPECT_STREQ(AuditVerdictName(AuditVerdict::kUnconverged), "unconverged");
}

// LI test_audit_verdict_three_states_and_rollup, verbatim: grids agreeing against the lattice
// correct, agreeing with it confirm, disagreeing establish nothing — and the roll-up takes the
// worst of the two counts' statuses.
TEST(DPPartition, AuditVerdictRollupThreeStatesAndWorstFirst) {
  const struct {
    int domain[2];
    int complement[2];
    int lattice_domain;
    int lattice_complement;
    AuditVerdict want;
  } rows[] = {
    { { 1, 1 }, { 1, 1 }, 2, 1, AuditVerdict::kCorrected },   { { 2, 2 }, { 1, 1 }, 2, 1, AuditVerdict::kConfirmed },
    { { 1, 2 }, { 1, 1 }, 2, 1, AuditVerdict::kUnconverged }, { { -1, 1 }, { 1, 1 }, 2, 1, AuditVerdict::kUnconverged },
    { { 1, 1 }, { 2, 2 }, 2, 1, AuditVerdict::kCorrected },   { { 2, 2 }, { 1, 2 }, 2, 1, AuditVerdict::kUnconverged },
  };
  for (const auto& row : rows) {
    EXPECT_EQ(AuditVerdictRollup(row.domain, row.complement, 2, row.lattice_domain, row.lattice_complement), row.want)
        << AuditVerdictName(row.want);
  }
}

// ---------------------------------------------------------------------------------------------
// The chart mask counts on synthetic masks (LI test_chart_counts_*, no field involved)
// ---------------------------------------------------------------------------------------------

// The chart's index coordinates of an 81-point side (LI _chart_geometry): i, j in [-40, 40], the
// off-chart mask i^2 + j^2 >= 40^2.
struct ChartGeometry {
  static constexpr int kGrid = 81;
  static constexpr int kM = 40;
  std::vector<unsigned char> off_chart;

  ChartGeometry() : off_chart(kGrid * kGrid, 0) {
    for (int i = -kM; i <= kM; i++) {
      for (int j = -kM; j <= kM; j++) {
        off_chart[Node(i, j)] = (i * i + j * j >= kM * kM) ? 1 : 0;
      }
    }
  }

  static int Node(int i, int j) { return (i + kM) * kGrid + (j + kM); }

  std::vector<unsigned char> Mask(bool(mask_fn)(int, int)) const {
    std::vector<unsigned char> mask(kGrid * kGrid, 0);
    for (int i = -kM; i <= kM; i++) {
      for (int j = -kM; j <= kM; j++) {
        mask[Node(i, j)] = mask_fn(i, j) ? 1 : 0;
      }
    }
    return mask;
  }
};

namespace chart_masks {

bool TwoIslands(int i, int j) {
  const int a = i + 22, b = i - 22;
  return (a * a + j * j <= 100) || (b * b + j * j <= 100);
}

bool IslandsJoinedByCorridor(int i, int j) {
  return TwoIslands(i, j) || (j == 0 && std::abs(i) <= 13);
}

bool HoledDisc(int i, int j) {
  return i * i + j * j < ChartGeometry::kM * ChartGeometry::kM && i * i + j * j > 9;
}

bool RimBand(int i, int j) {
  return i * i + j * j < ChartGeometry::kM * ChartGeometry::kM && i < 20;
}

}  // namespace chart_masks

// A neck the coarse view splits and the corridor view joins; a genuine two-island mask.
TEST(DPPartition, ChartCountsDumbbellAndTwoIslands) {
  const ChartGeometry g;
  int domain = 0, complement = 0;
  ASSERT_TRUE(ChartComponentCounts(g.Mask(chart_masks::TwoIslands).data(), g.off_chart.data(), ChartGeometry::kGrid,
                                   &domain, &complement));
  EXPECT_EQ(domain, 2);
  EXPECT_EQ(complement, 1);
  ASSERT_TRUE(ChartComponentCounts(g.Mask(chart_masks::IslandsJoinedByCorridor).data(), g.off_chart.data(),
                                   ChartGeometry::kGrid, &domain, &complement));
  EXPECT_EQ(domain, 1);
  EXPECT_EQ(complement, 1);
}

// An interior invalid hole is a second complement island; an invalid band reaching the rim merges
// into the one far-hemisphere component.
TEST(DPPartition, ChartCountsComplementIslandsAndRimMerge) {
  const ChartGeometry g;
  int domain = 0, complement = 0;
  ASSERT_TRUE(ChartComponentCounts(g.Mask(chart_masks::HoledDisc).data(), g.off_chart.data(), ChartGeometry::kGrid,
                                   &domain, &complement));
  EXPECT_EQ(domain, 1);
  EXPECT_EQ(complement, 2);
  ASSERT_TRUE(ChartComponentCounts(g.Mask(chart_masks::RimBand).data(), g.off_chart.data(), ChartGeometry::kGrid,
                                   &domain, &complement));
  EXPECT_EQ(domain, 1);
  EXPECT_EQ(complement, 1);
}

// A valid node on the pushed rim breaches the chart premise: no counts, not counts merged
// through it (fail closed — the -1 sentinel of a breached grid).
TEST(DPPartition, ChartCountsValidOnTheRimVoidsTheCounts) {
  const ChartGeometry g;
  std::vector<unsigned char> breached = g.Mask(chart_masks::TwoIslands);
  breached[ChartGeometry::Node(-40, -40)] = 1;  // a grid corner is off-chart
  EXPECT_EQ(g.off_chart[ChartGeometry::Node(-40, -40)], 1);
  int domain = 0, complement = 0;
  EXPECT_FALSE(ChartComponentCounts(breached.data(), g.off_chart.data(), ChartGeometry::kGrid, &domain, &complement));
}

// ---------------------------------------------------------------------------------------------
// The thin-neck integration anchor (task 52.7): 3-5-6-7 at n = 1.31
// ---------------------------------------------------------------------------------------------

// The lattice says 2 components (a neck < 1e-3 rad against its 0.016 rad spacing — the artefact),
// the audit's grids (801, 1601) both say one, and the adjudicated topology is the disk the
// partition needs. The lattice count of 2 is itself a pinned measurement (non-monotone in the
// density on LI: 50000 = 2, 100000 = 1, 200000 = 2): if a platform flips the 20000 count to 1
// the trigger gate disarms and this test's audit assertions cannot hold — that is the recorded
// risk-1 degradation, to be answered by pinning another density or downgrading to the structural
// properties, never by re-tuning the implementation.
TEST(DPPartition, ThinNeckTopologyIsAuditedIntoADisk) {
  const Fixture f(Prism());
  const int faces[4] = { 3, 5, 6, 7 };
  const DeviationField field = f.Field(faces, 4, kN131);

  const DomainTopology topology = DomainTopologyOf(field, kTopologyLatticeN, nullptr, 0);
  ASSERT_TRUE(topology.has_grid_audit) << "a plural lattice count pays the audit";
  const ChartAudit& audit = topology.grid_audit;
  EXPECT_EQ(audit.lattice_domain_count, 2) << "the 52.7 artefact, as LI measured it";
  EXPECT_EQ(audit.lattice_complement_count, 1);
  EXPECT_EQ(audit.grids, (std::vector<int>{ 801, 1601 }));
  EXPECT_EQ(audit.domain_counts, (std::vector<int>{ 1, 1 }));
  EXPECT_EQ(audit.complement_counts, (std::vector<int>{ 1, 1 }));
  EXPECT_EQ(audit.verdict, AuditVerdict::kCorrected);
  EXPECT_EQ(topology.domain_components, 1) << "the adjudicated counts replace the lattice's";
  EXPECT_EQ(topology.complement_components, 1);
  EXPECT_TRUE(topology.IsDisk());
}

// The empty ladder is the rollback switch: the audit stays off, the plural lattice count keeps
// its value, the topology is not a disk — the pre-52.7 semantics (the red-state half of AC2's
// 52.7 probe: without the audit the partition of this path has no disk to stand on).
TEST(DPPartition, EmptyLadderIsTheRollbackSwitch) {
  const Fixture f(Prism());
  const int faces[4] = { 3, 5, 6, 7 };
  const DeviationField field = f.Field(faces, 4, kN131);

  const int ladder[1] = { 0 };
  const DomainTopology topology = DomainTopologyOf(field, kTopologyLatticeN, ladder, 0);
  EXPECT_FALSE(topology.has_grid_audit);
  EXPECT_EQ(topology.domain_components, 2) << "the lattice artefact, unaudited";
  EXPECT_EQ(topology.complement_components, 1);
  EXPECT_FALSE(topology.IsDisk());
}

// A healthy path pays no audit: a singular lattice count never triggers it (grid_audit absent,
// no chart cost).
TEST(DPPartition, HealthyPathsPayNoAudit) {
  const Fixture f(Prism());
  const int faces[2] = { 3, 5 };
  const DeviationField field = f.Field(faces, 2, kN131);
  const DomainTopology topology = DomainTopologyOf(field, kTopologyLatticeN, nullptr, 0);
  EXPECT_FALSE(topology.has_grid_audit);
  EXPECT_TRUE(topology.IsDisk());
  EXPECT_EQ(topology.domain_components, 1);
  EXPECT_EQ(topology.complement_components, 1);
}

// ---------------------------------------------------------------------------------------------
// The degenerate fold set and its circular runs (task 52.8's evidence half)
// ---------------------------------------------------------------------------------------------

// LI _circular_runs' semantics, table-driven: a run crossing index 0 is ONE run (the wrap-merge
// lesson of 52.8's review — a naive linear pass splits it and miscounts both halves).
TEST(DPPartition, CircularRunsMergeTheWrappingRun) {
  const struct {
    const char* what;
    std::vector<unsigned char> mask;
    std::vector<std::pair<int, int>> want;
  } rows[] = {
    { "two interior runs", { 0, 0, 1, 1, 0, 1, 0, 0 }, { { 2, 2 }, { 5, 1 } } },
    { "a run across index 0", { 1, 1, 0, 0, 1, 1, 1, 0 }, { { 0, 2 }, { 4, 3 } } },
    { "one run wrapping the end to the start", { 1, 0, 0, 1 }, { { 3, 2 } } },
    { "all true", { 1, 1, 1, 1 }, { { 0, 4 } } },
    { "all false", { 0, 0, 0, 0 }, {} },
  };
  for (const auto& row : rows) {
    const std::vector<std::pair<int, int>> runs = CircularRuns(row.mask.data(), static_cast<int>(row.mask.size()));
    if (runs.size() != row.want.size()) {
      ADD_FAILURE() << row.what << ": got " << runs.size() << " run(s), want " << row.want.size();
      continue;
    }
    for (size_t k = 0; k < runs.size(); k++) {
      EXPECT_EQ(runs[k], row.want[k]) << row.what << " run " << k;
    }
  }
}

// The one positive fixture of the cluster evidence: the beta crystal's 4-8-7-5, whose crease (the
// c-axis great circle, D_P = 120 deg along it) crosses U_P's interior as one arc — 22.764% of
// its sampling — without closing inside (no closed ridge) or hugging dU_P (no touching arc), and
// both axis points sit ON dU_P (so the slab interior set is empty). Every number is LI's
// (dp-slab-partition-completion's probe, the task-dir anchor dump).
TEST(DPPartition, BetaFoldSetHoldsOneInteriorCreaseArc) {
  const Fixture f(Beta());
  const int faces[4] = { 4, 8, 7, 5 };
  const DeviationField field = f.Field(faces, 4, 1.3110129);
  const DegenerateFoldSet fold_set = BuildDegenerateFoldSet(field, kFoldCircleSamples);
  ASSERT_TRUE(fold_set.has_axis);
  EXPECT_GT(std::fabs(fold_set.axis[2]), 1.0 - 1e-12) << "the fold axis is the c-axis";
  EXPECT_NEAR(fold_set.circle_interior_fraction, 0.227639, 2e-4);
  EXPECT_EQ(fold_set.crease_interior_arcs, 1);
  EXPECT_FALSE(fold_set.crease_closed_ridge);
  EXPECT_FALSE(fold_set.crease_touching_arc);
  ASSERT_EQ(fold_set.axis_point_count, 2);
  EXPECT_EQ(fold_set.axis_points[0].location, DomainLocation::kBoundary);
  EXPECT_EQ(fold_set.axis_points[1].location, DomainLocation::kBoundary);
  const std::vector<InteriorCriticalPoint> interior = SlabInteriorCriticalPoints(field, fold_set);
  EXPECT_TRUE(interior.empty()) << "both axis points are on dU_P: no interior member";
}

// The grazing fixtures: a crease that only touches U_P holds no interior arc (fraction exactly 0)
// — 3-1-6's crease is a grazing boundary, 3-5-6-7-3's entry circle is coincident with the crease
// (the touching-arc evidence) and +n_3 is an interior axis point at D = pi.
TEST(DPPartition, GrazingCreaseHoldsNoInteriorArc) {
  const Fixture f(Prism());
  {
    const int faces[3] = { 3, 1, 6 };
    const DeviationField field = f.Field(faces, 3, kN131);
    const DegenerateFoldSet fold_set = BuildDegenerateFoldSet(field, kFoldCircleSamples);
    EXPECT_EQ(fold_set.circle_interior_fraction, 0.0);
    EXPECT_EQ(fold_set.crease_interior_arcs, 0);
  }
  {
    const int faces[5] = { 3, 5, 6, 7, 3 };
    const DeviationField field = f.Field(faces, 5, kN131);
    const DegenerateFoldSet fold_set = BuildDegenerateFoldSet(field, kFoldCircleSamples);
    EXPECT_EQ(fold_set.circle_interior_fraction, 0.0);
    EXPECT_EQ(fold_set.crease_interior_arcs, 0);
    EXPECT_TRUE(fold_set.crease_touching_arc) << "the coincident entry circle hugs dU_P all round";
    ASSERT_EQ(fold_set.axis_point_count, 2);
    EXPECT_EQ(fold_set.axis_points[0].location, DomainLocation::kInterior);
    EXPECT_EQ(fold_set.axis_points[1].location, DomainLocation::kExterior);
    const std::vector<InteriorCriticalPoint> interior = SlabInteriorCriticalPoints(field, fold_set);
    ASSERT_EQ(interior.size(), 1u);
    EXPECT_EQ(interior[0].kind, CriticalKind::kDegenerate);
    EXPECT_NEAR(interior[0].value, kPi, 1e-12) << "D_P = pi at the mirror axis point";
  }
}

// ---------------------------------------------------------------------------------------------
// The slab-crease three-check gate (task 52.8's four fail-closed forms + the holding case)
// ---------------------------------------------------------------------------------------------

namespace {

// A loop whose critical points carry `value` as a strict maximum (the blade evidence the gate
// wants), plus one minimum so the data is not vacuous.
BoundaryLoopData LoopWithStrictMaximum(double value) {
  BoundaryLoopData loop;
  BoundaryCriticalPoint maximum;
  maximum.value = value;
  maximum.kind = CriticalKind::kMaximum;
  maximum.strict = true;
  loop.critical_points.push_back(maximum);
  BoundaryCriticalPoint minimum;
  minimum.value = value - 1.0;
  minimum.kind = CriticalKind::kMinimum;
  minimum.strict = true;
  loop.critical_points.push_back(minimum);
  return loop;
}

}  // namespace

// The four fail-closed forms, each with its own regime and its LI message text (report-side grep
// compatibility), plus the holding case on the real beta evidence: the gates pass when the arc
// evidence, the strict blade carriage and the two non-degeneracy checks all hold.
TEST(DPPartition, SlabCreaseGatesFourFormsAndHoldingCase) {
  const Fixture f(Beta());
  const int faces[4] = { 4, 8, 7, 5 };
  const DeviationField field = f.Field(faces, 4, 1.3110129);
  const DegenerateFoldSet real = BuildDegenerateFoldSet(field, kFoldCircleSamples);
  double basis[2][3];
  TangentBasis(real.axis, basis);
  const double blade = field.DSlab(basis[0]);  // 120.00000000000001 deg, LI's own value

  // The holding case: the real evidence and a loop carrying the blade as a strict maximum.
  EscapeRegime regime = EscapeRegime::kNotDiskUnaudited;
  std::string message;
  EXPECT_TRUE(SlabCreaseGates(field, real, LoopWithStrictMaximum(blade), &regime, &message));

  // Form 0: a fraction claiming interior arcs the crease sampling does not hold.
  DegenerateFoldSet patched = real;
  patched.crease_interior_arcs = 0;
  EXPECT_FALSE(SlabCreaseGates(field, patched, LoopWithStrictMaximum(blade), &regime, &message));
  EXPECT_EQ(regime, EscapeRegime::kSlabCreaseContradiction);
  EXPECT_NE(message.find("holds no interior arc"), std::string::npos);

  // Form (a): no strict local maximum at the blade value (a foreign loop, maxima far away).
  BoundaryLoopData foreign = LoopWithStrictMaximum(blade - 0.2);
  EXPECT_FALSE(SlabCreaseGates(field, real, foreign, &regime, &message));
  EXPECT_EQ(regime, EscapeRegime::kSlabCreaseNotCarried);
  EXPECT_NE(message.find("not carried by the boundary walk as a strict local maximum"), std::string::npos);

  // Form (a) again, the plateau trap: a NON-strict maximum at the blade does not carry it.
  BoundaryLoopData plateau = LoopWithStrictMaximum(blade);
  plateau.critical_points[0].strict = false;
  EXPECT_FALSE(SlabCreaseGates(field, real, plateau, &regime, &message));
  EXPECT_EQ(regime, EscapeRegime::kSlabCreaseNotCarried);

  // Form 2a: the crease hugging dU_P over an arc (a tangency or coincidence, not a crossing).
  patched = real;
  patched.crease_touching_arc = true;
  EXPECT_FALSE(SlabCreaseGates(field, patched, LoopWithStrictMaximum(blade), &regime, &message));
  EXPECT_EQ(regime, EscapeRegime::kSlabCreaseTouching);
  EXPECT_NE(message.find("non-transversal contact"), std::string::npos);

  // Form 2b: an interior crease arc that never reaches dU_P (a closed ridge).
  patched = real;
  patched.crease_closed_ridge = true;
  EXPECT_FALSE(SlabCreaseGates(field, patched, LoopWithStrictMaximum(blade), &regime, &message));
  EXPECT_EQ(regime, EscapeRegime::kSlabCreaseClosedRidge);
  EXPECT_NE(message.find("closed ridge"), std::string::npos);

  // The four regimes are four names: no form is merged into another (AC1's 52.8 side).
  EXPECT_STRNE(EscapeRegimeName(EscapeRegime::kSlabCreaseContradiction),
               EscapeRegimeName(EscapeRegime::kSlabCreaseNotCarried));
  EXPECT_STRNE(EscapeRegimeName(EscapeRegime::kSlabCreaseTouching),
               EscapeRegimeName(EscapeRegime::kSlabCreaseClosedRidge));
}


// The anchors' loop data, generated at full precision from LI's one-shot dump (JAX
// authority; the generator lives in the task directory). Positions included for fidelity — the
// partition reads values, kinds, strictness, corners and the plateau; the ring probes read the
// touching positions.
BoundaryLoopData BetaLoop() {
  BoundaryLoopData loop;
  loop.critical_points.push_back(
      BoundaryCriticalPoint{ { 0.9452800254848724, 0.32626013151980265, 1.1942756488834577e-08 },
                             2.0943951023931957,
                             CriticalKind::kMaximum,
                             true,
                             false });
  loop.critical_points.push_back(BoundaryCriticalPoint{ { 6.922886122576375e-16, -2.8422296280593433e-16, -1.0 },
                                                        1.2962016223347998e-15,
                                                        CriticalKind::kMinimum,
                                                        true,
                                                        false });
  loop.critical_points.push_back(
      BoundaryCriticalPoint{ { -0.1900904506042355, 0.9817665815198023, -1.5517542870223015e-08 },
                             2.0943951023931957,
                             CriticalKind::kMaximum,
                             true,
                             false });
  loop.critical_points.push_back(BoundaryCriticalPoint{ { -9.582873898943746e-16, 5.532674825165411e-16, 1.0 },
                                                        1.9165747797887496e-15,
                                                        CriticalKind::kMinimum,
                                                        true,
                                                        false });
  loop.corners.push_back(
      LoopCorner{ { 0.4238970464530301, -0.24473707387834423, -0.87201780869307 }, 0.8754875215347143 });
  loop.corners.push_back(
      LoopCorner{ { -0.4238970464530303, 0.24473707387834437, -0.8720178086930699 }, 0.8754875215347147 });
  loop.corners.push_back(
      LoopCorner{ { -0.4238970464530303, 0.24473707387834434, 0.8720178086930699 }, 0.8754875215347147 });
  loop.corners.push_back(
      LoopCorner{ { 0.42389704645303006, -0.2447370738783442, 0.87201780869307 }, 0.8754875215347142 });
  return loop;
}
BoundaryLoopData Loop3145() {
  BoundaryLoopData loop;
  loop.critical_points.push_back(BoundaryCriticalPoint{
      { 0.0, -0.48947414775668857, -0.8720178086930699 }, 2.6470956323339543, CriticalKind::kMaximum, true, true });
  loop.critical_points.push_back(BoundaryCriticalPoint{
      { 0.7551895748806372, -0.65550645, 0.0 }, 0.38250855233379194, CriticalKind::kMinimum, true, true });
  loop.corners.push_back(LoopCorner{ { 0.0, -0.48947414775668857, -0.8720178086930699 }, 2.6470956323339543 });
  loop.corners.push_back(LoopCorner{ { 0.7551895748806372, -0.65550645, 0.0 }, 0.38250855233379194 });
  loop.corners.push_back(LoopCorner{ { 0.0, -1.0, 0.0 }, 2.0943951023931957 });
  return loop;
}
BoundaryLoopData Loop3415() {
  BoundaryLoopData loop;
  loop.critical_points.push_back(BoundaryCriticalPoint{
      { 0.0, -0.48947414775668857, -0.8720178086930699 }, 2.6470956323339543, CriticalKind::kMaximum, true, true });
  loop.critical_points.push_back(BoundaryCriticalPoint{
      { 0.7551895748806372, -0.65550645, 0.0 }, 0.38250855233379194, CriticalKind::kMinimum, true, true });
  loop.corners.push_back(LoopCorner{ { 0.0, -0.48947414775668857, -0.8720178086930699 }, 2.6470956323339543 });
  loop.corners.push_back(LoopCorner{ { 0.7551895748806372, -0.65550645, 0.0 }, 0.38250855233379194 });
  loop.corners.push_back(LoopCorner{ { 0.0, -1.0, 0.0 }, 2.0943951023931957 });
  return loop;
}
BoundaryLoopData Loop3567N131() {
  BoundaryLoopData loop;
  loop.critical_points.push_back(BoundaryCriticalPoint{
      { 0.0, -0.48856934001224467, 0.8725250712730264 }, 0.8737575536009662, CriticalKind::kMinimum, true, true });
  loop.critical_points.push_back(BoundaryCriticalPoint{
      { 0.755628877161269, -0.655, 7.140961095069564e-09 }, 2.475562792149235, CriticalKind::kMaximum, true, false });
  loop.critical_points.push_back(BoundaryCriticalPoint{
      { 0.0, -0.48856934001224445, -0.8725250712730265 }, 0.8737575468630231, CriticalKind::kMinimum, true, true });
  loop.critical_points.push_back(
      BoundaryCriticalPoint{ { 0.9725117017385273, -0.23285401001836709, -3.32888361163401e-09 },
                             2.8530052140926117,
                             CriticalKind::kMaximum,
                             true,
                             false });
  loop.corners.push_back(LoopCorner{ { 0.0, -0.48856934001224445, -0.8725250712730265 }, 0.8737575468630231 });
  loop.corners.push_back(LoopCorner{ { 0.0, -0.48856934001224467, 0.8725250712730264 }, 0.8737575536009662 });
  return loop;
}
BoundaryLoopData Loop35673() {
  BoundaryLoopData loop;
  loop.critical_points.push_back(BoundaryCriticalPoint{
      { 0.0, -0.4885693400122444, 0.8725250712730266 }, 2.9837338786239642e-16, CriticalKind::kMinimum, false, true });
  loop.critical_points.push_back(
      BoundaryCriticalPoint{ { 0.755628877161269, -0.6549999999999999, 1.7369718536425154e-08 },
                             1.713227412637156,
                             CriticalKind::kMaximum,
                             false,
                             false });
  loop.critical_points.push_back(BoundaryCriticalPoint{ { 0.0, -0.48856934001224445, -0.8725250712730265 },
                                                        2.9624124749404127e-16,
                                                        CriticalKind::kMinimum,
                                                        false,
                                                        true });
  loop.critical_points.push_back(BoundaryCriticalPoint{ { 0.7556288771612689, 0.655, -2.185707643261136e-09 },
                                                        1.7132274126371552,
                                                        CriticalKind::kMaximum,
                                                        false,
                                                        false });
  loop.corners.push_back(LoopCorner{ { 0.0, -0.48856934001224445, -0.8725250712730265 }, 2.9624124749404127e-16 });
  loop.corners.push_back(LoopCorner{ { 0.0, 0.48856934001224445, -0.8725250712730265 }, 2.9624124749404127e-16 });
  loop.corners.push_back(LoopCorner{ { 0.0, 0.48856934001224445, 0.8725250712730266 }, 2.957884427700476e-16 });
  loop.corners.push_back(LoopCorner{ { 0.0, -0.4885693400122444, 0.8725250712730266 }, 2.9837338786239642e-16 });
  return loop;
}
BoundaryLoopData Loop121() {
  BoundaryLoopData loop;
  loop.has_plateau = true;
  loop.plateau_value = 2.682805732374007e-17;
  return loop;
}
constexpr double kLoop121FirstPoint[3] = { 0.5933607928732527, 0.8049366245120326, 0.0 };


// ---------------------------------------------------------------------------------------------
// The partition end to end on the anchors (AC3): loop data hand-built from LI's dump at full
// precision, topology / fold set / slab interior computed live. The break values therefore land
// on the dumped ones essentially exactly; tolerances are 1e-9 deg (a rounding cushion, not a
// semantic margin).
// ---------------------------------------------------------------------------------------------

double Deg2(double rad) {
  return rad * 180.0 / kPi;
}

void ExpectInterval(const DeviationInterval& interval, double lower_deg, double upper_deg, int n_components,
                    int n_closed, int n_open) {
  EXPECT_NEAR(Deg2(interval.lower), lower_deg, 1e-9) << "lower";
  EXPECT_NEAR(Deg2(interval.upper), upper_deg, 1e-9) << "upper";
  EXPECT_EQ(interval.n_components, n_components);
  EXPECT_EQ(interval.n_closed, n_closed);
  EXPECT_EQ(interval.n_open, n_open);
  EXPECT_EQ(interval.n_components, interval.n_closed + interval.n_open);
}

// The beta crystal's 4-8-7-5: the slab crease crosses U_P and partitions (52.8's positive case).
// The breaks are the axis minima (D = 0), the four corners' closure value 50.161740 deg and the
// blade 120.00000000000001 deg; every interval holds two open arcs; C05's display support
// [0.54, 120] deg is bracketed.
TEST(DPPartition, BetaSlabCreasePartition) {
  const Fixture f(Beta());
  const int faces[4] = { 4, 8, 7, 5 };
  const DeviationField field = f.Field(faces, 4, 1.3110129);
  const DomainTopology topology = DomainTopologyOf(field, kTopologyLatticeN, nullptr, 0);
  ASSERT_TRUE(topology.IsDisk());
  const DegenerateFoldSet fold_set = BuildDegenerateFoldSet(field, kFoldCircleSamples);
  const std::vector<InteriorCriticalPoint> interior = SlabInteriorCriticalPoints(field, fold_set);

  const PartitionResult result = IntervalPartition(field, interior, &fold_set, BetaLoop(), topology);
  ASSERT_FALSE(result.escaped) << result.message;
  ASSERT_EQ(result.intervals.size(), 2u);
  ExpectInterval(result.intervals[0], 7.426688235779429e-14, 50.161740000307894, 2, 0, 2);
  ExpectInterval(result.intervals[1], 50.161740000307894, 120.00000000000001, 2, 0, 2);

  // The blade: the upper break is d_slab of a tangent basis vector of the fold axis (1e-9 deg).
  double basis[2][3];
  TangentBasis(fold_set.axis, basis);
  EXPECT_NEAR(Deg2(result.intervals[1].upper), Deg2(field.DSlab(basis[0])), 1e-9) << "the blade value";
  // C05 anchor: the display support [0.54, 120] deg lies inside the partition's range.
  EXPECT_LE(Deg2(result.intervals[0].lower), 0.54);
  EXPECT_GT(Deg2(result.intervals.back().upper), 120.0 - 1e-9);
}

// 3-1-4-5 and 3-4-1-5: degenerate folds whose crease only grazes U_P (fraction 0, gates skipped),
// the 120.00000000000001 deg break is a CORNER value, and each interval holds one open arc.
TEST(DPPartition, PrismSlabCornerPartition) {
  const Fixture f(Prism());
  const int paths[2][4] = { { 3, 1, 4, 5 }, { 3, 4, 1, 5 } };
  for (int path = 0; path < 2; path++) {
    const DeviationField field = f.Field(paths[path], 4, 1.3110129);
    const DomainTopology topology = DomainTopologyOf(field, kTopologyLatticeN, nullptr, 0);
    const DegenerateFoldSet fold_set = BuildDegenerateFoldSet(field, kFoldCircleSamples);
    const std::vector<InteriorCriticalPoint> interior;  // both axis points are exterior (the dump)
    const BoundaryLoopData loop = path == 0 ? Loop3145() : Loop3415();
    const PartitionResult result = IntervalPartition(field, interior, &fold_set, loop, topology);
    if (!topology.IsDisk() || fold_set.circle_interior_fraction != 0.0 || result.escaped ||
        result.intervals.size() != 2u) {
      ADD_FAILURE() << "path " << path << ": disk, grazing crease, partitioned into two (" << result.message << ")";
      continue;
    }
    ExpectInterval(result.intervals[0], 21.916125676385253, 120.00000000000001, 1, 0, 1);
    ExpectInterval(result.intervals[1], 120.00000000000001, 151.66740770024947, 1, 0, 1);
  }
}

// The thin neck's flip, end to end (52.7's red/green pair): with the audit the adjudicated disk
// carries the A60-10 partition; with the empty ladder the same call escapes not_disk_unaudited —
// the pre-52.7 behaviour, and the red-state half of AC2's 52.7 probe (without the audit this
// path has no disk to stand on).
TEST(DPPartition, ThinNeckFlipPartitionEndToEnd) {
  const Fixture f(Prism());
  const int faces[4] = { 3, 5, 6, 7 };
  const DeviationField field = f.Field(faces, 4, kN131);
  const std::vector<InteriorCriticalPoint> interior;  // the A60-10 members have none (the dump)
  const int empty_ladder[1] = { 0 };

  const DomainTopology audited = DomainTopologyOf(field, kTopologyLatticeN, nullptr, 0);
  const PartitionResult partitioned = IntervalPartition(field, interior, nullptr, Loop3567N131(), audited);
  ASSERT_FALSE(partitioned.escaped) << partitioned.message;
  ASSERT_EQ(partitioned.intervals.size(), 2u);
  ExpectInterval(partitioned.intervals[0], 50.06261975295546, 141.83929990977302, 2, 0, 2);
  ExpectInterval(partitioned.intervals[1], 141.83929990977302, 163.4651576963245, 1, 0, 1);

  const DomainTopology rolled_back = DomainTopologyOf(field, kTopologyLatticeN, empty_ladder, 0);
  const PartitionResult escaped = IntervalPartition(field, interior, nullptr, Loop3567N131(), rolled_back);
  ASSERT_TRUE(escaped.escaped);
  EXPECT_EQ(escaped.regime, EscapeRegime::kNotDiskUnaudited);
  EXPECT_NE(escaped.message.find("not a disk on the 20000-point lattice: 2 component(s), complement 1"),
            std::string::npos);
  EXPECT_TRUE(escaped.intervals.empty()) << "the mechanical invariant: an escape carries no intervals";
}

// 3-5-6-7-3's interior degenerate point: the ring probe decides it is a maximum, the closed loop
// exists between the loop maximum 98.16070 deg and the interior D = pi, and the first interval
// holds two open arcs around the four zero corners.
TEST(DPPartition, ClosedLoopAroundTheDegenerateMaximum) {
  const Fixture f(Prism());
  const int faces[5] = { 3, 5, 6, 7, 3 };
  const DeviationField field = f.Field(faces, 5, kN131);
  const DomainTopology topology = DomainTopologyOf(field, kTopologyLatticeN, nullptr, 0);
  ASSERT_TRUE(topology.IsDisk());
  const DegenerateFoldSet fold_set = BuildDegenerateFoldSet(field, kFoldCircleSamples);
  const std::vector<InteriorCriticalPoint> interior = SlabInteriorCriticalPoints(field, fold_set);
  ASSERT_EQ(interior.size(), 1u);

  const PartitionResult result = IntervalPartition(field, interior, &fold_set, Loop35673(), topology);
  ASSERT_FALSE(result.escaped) << result.message;
  ASSERT_EQ(result.intervals.size(), 2u);
  ExpectInterval(result.intervals[0], 1.6947429399470616e-14, 98.16070009022695, 2, 0, 2);
  ExpectInterval(result.intervals[1], 98.16070009022695, 179.99999999999997, 1, 1, 0);
  EXPECT_NEAR(interior[0].value, kPi, 1e-12);
}

// 1-2-1's plateau loop: no critical point, no corner, the constant value 2.7e-17 rad is the
// record; the ring probe at the walk's first point decides the interior axis point is a maximum
// and the single interval holds its closed loop.
TEST(DPPartition, PlateauLoopPartition) {
  const Fixture f(Prism());
  const int faces[3] = { 1, 2, 1 };
  const DeviationField field = f.Field(faces, 3, kN131);
  const DomainTopology topology = DomainTopologyOf(field, kTopologyLatticeN, nullptr, 0);
  ASSERT_TRUE(topology.IsDisk());
  const DegenerateFoldSet fold_set = BuildDegenerateFoldSet(field, kFoldCircleSamples);
  const std::vector<InteriorCriticalPoint> interior = SlabInteriorCriticalPoints(field, fold_set);
  ASSERT_EQ(interior.size(), 1u);
  BoundaryLoopData loop = Loop121();
  for (int i = 0; i < 3; i++) {
    loop.first_point[i] = kLoop121FirstPoint[i];
  }

  const PartitionResult result = IntervalPartition(field, interior, &fold_set, loop, topology);
  ASSERT_FALSE(result.escaped) << result.message;
  ASSERT_EQ(result.intervals.size(), 1u);
  ExpectInterval(result.intervals[0], 1.5371344571853445e-15, 180.0, 1, 1, 0);
}

// The disk escapes' audit branches (the two AC1 gap regimes of the plan review among them): a
// confirmed plural count and a corrected-but-still-not-disk count escape with their own regimes,
// and an unconverged audit establishes nothing. Synthetic topologies on the real 3-5 field.
TEST(DPPartition, DiskEscapeAuditBranches) {
  const Fixture f(Prism());
  const int faces[2] = { 3, 5 };
  const DeviationField field = f.Field(faces, 2, kN131);
  const DomainTopology disk = DomainTopologyOf(field, kTopologyLatticeN, nullptr, 0);
  ASSERT_TRUE(disk.IsDisk());

  const auto with_audit = [&](int domain, int complement, std::vector<int> domain_counts,
                              std::vector<int> complement_counts, AuditVerdict verdict) {
    DomainTopology topology;
    topology.lattice_n = 20000;
    topology.domain_components = domain;
    topology.complement_components = complement;
    topology.has_grid_audit = true;
    topology.grid_audit.grids = { 801, 1601 };
    topology.grid_audit.domain_counts = std::move(domain_counts);
    topology.grid_audit.complement_counts = std::move(complement_counts);
    topology.grid_audit.lattice_domain_count = 2;
    topology.grid_audit.lattice_complement_count = 1;
    topology.grid_audit.verdict = verdict;
    return topology;
  };
  BoundaryLoopData loop;
  BoundaryCriticalPoint minimum;
  minimum.value = 0.1;
  minimum.kind = CriticalKind::kMinimum;
  BoundaryCriticalPoint maximum;
  maximum.value = 1.0;
  maximum.kind = CriticalKind::kMaximum;
  loop.critical_points = { minimum, maximum };
  const std::vector<InteriorCriticalPoint> interior;

  const struct {
    DomainTopology topology;
    EscapeRegime want;
    const char* fragment;
  } rows[] = {
    { with_audit(2, 1, { 2, 2 }, { 1, 1 }, AuditVerdict::kConfirmed), EscapeRegime::kNotDiskConfirmed,
      "lattice 20000 and chart grids (801, 1601) agree" },
    { with_audit(1, 2, { 1, 1 }, { 2, 2 }, AuditVerdict::kCorrected), EscapeRegime::kNotDiskCorrected,
      "correcting the lattice 20000 counts 2/1" },
    { with_audit(2, 1, { 1, 2 }, { 1, 1 }, AuditVerdict::kUnconverged), EscapeRegime::kNotDiskUnconverged,
      "counts are not resolution-converged, the topology is not established" },
  };
  for (const auto& row : rows) {
    const PartitionResult result = IntervalPartition(field, interior, nullptr, loop, row.topology);
    if (!result.escaped) {
      ADD_FAILURE() << AuditVerdictName(row.topology.grid_audit.verdict) << ": expected an escape";
      continue;
    }
    EXPECT_EQ(result.regime, row.want) << result.message;
    EXPECT_NE(result.message.find(row.fragment), std::string::npos) << result.message;
    EXPECT_TRUE(result.intervals.empty()) << "the mechanical invariant: an escape carries no intervals";
  }
}

// The remaining escape regimes on synthetic data (AC1's full enumeration closed): two interior
// points, a non-alternating loop, an odd crossing count, a saddle, and an interior minimum above
// the loop minimum (its sublevel component cannot be the first to reach dU_P there).
TEST(DPPartition, SyntheticEscapeBranches) {
  const Fixture f(Prism());
  const int faces[2] = { 3, 5 };
  const DeviationField field = f.Field(faces, 2, kN131);
  const DomainTopology topology = DomainTopologyOf(field, kTopologyLatticeN, nullptr, 0);
  ASSERT_TRUE(topology.IsDisk());

  const auto critical = [](double value, CriticalKind kind, bool strict = true) {
    BoundaryCriticalPoint point;
    point.value = value;
    point.kind = kind;
    point.strict = strict;
    return point;
  };
  const auto interior_point = [](double value, CriticalKind kind) {
    InteriorCriticalPoint point;
    point.position[0] = 0.0;
    point.position[1] = 0.0;
    point.position[2] = 1.0;
    point.value = value;
    point.kind = kind;
    return point;
  };

  const struct {
    const char* what;
    std::vector<InteriorCriticalPoint> interior;
    BoundaryLoopData loop;
    EscapeRegime want;
    const char* fragment;
  } rows[] = {
    { "two interior points",
      { interior_point(0.1, CriticalKind::kMinimum), interior_point(1.0, CriticalKind::kSaddle) },
      { { critical(0.1, CriticalKind::kMinimum), critical(1.0, CriticalKind::kMaximum) }, {}, {}, false, 0.0 },
      EscapeRegime::kMultipleInteriorCriticalPoints,
      "2 interior critical points: minimum, saddle" },
    { "a saddle",
      { interior_point(1.0, CriticalKind::kSaddle) },
      { { critical(0.1, CriticalKind::kMinimum), critical(1.0, CriticalKind::kMaximum) }, {}, {}, false, 0.0 },
      EscapeRegime::kInteriorCriticalPointNotSimple,
      "interior critical point of kind 'saddle'" },
    { "not alternating",
      {},
      { { critical(0.1, CriticalKind::kMinimum), critical(0.5, CriticalKind::kMinimum),
          critical(1.0, CriticalKind::kMaximum) },
        {},
        {},
        false,
        0.0 },
      EscapeRegime::kLoopExtremaNotAlternating,
      "loop extrema do not alternate" },
    { "a minimum above the loop minimum",
      { interior_point(0.5, CriticalKind::kMinimum) },
      { { critical(0.1, CriticalKind::kMinimum), critical(1.0, CriticalKind::kMaximum) }, {}, {}, false, 0.0 },
      EscapeRegime::kSublevelNotReachingBoundary,
      "is not shown to reach dU_P first" },
  };
  for (const auto& row : rows) {
    const PartitionResult result = IntervalPartition(field, row.interior, nullptr, row.loop, topology);
    if (!result.escaped) {
      ADD_FAILURE() << row.what << ": expected an escape";
      continue;
    }
    EXPECT_EQ(result.regime, row.want) << row.what << ": " << result.message;
    EXPECT_NE(result.message.find(row.fragment), std::string::npos) << row.what << ": " << result.message;
    EXPECT_TRUE(result.intervals.empty()) << row.what << ": an escape carries no intervals";
  }
}

// The slab-crease gate's four forms THROUGH the partition (AC2's 52.8 red-state row: without the
// gate a crease through U_P would be partitioned as if ordinary — these four say what is refused
// instead).
TEST(DPPartition, SlabCreaseEscapesThroughThePartition) {
  const Fixture f(Beta());
  const int faces[4] = { 4, 8, 7, 5 };
  const DeviationField field = f.Field(faces, 4, 1.3110129);
  const DomainTopology topology = DomainTopologyOf(field, kTopologyLatticeN, nullptr, 0);
  ASSERT_TRUE(topology.IsDisk());
  const DegenerateFoldSet real = BuildDegenerateFoldSet(field, kFoldCircleSamples);
  const std::vector<InteriorCriticalPoint> interior;

  double basis[2][3];
  TangentBasis(real.axis, basis);
  const double blade = field.DSlab(basis[0]);
  BoundaryLoopData carrying = LoopWithStrictMaximum(blade);

  const struct {
    const char* what;
    DegenerateFoldSet fold_set;
    BoundaryLoopData loop;
    EscapeRegime want;
  } rows[] = {
    { "contradiction",
      [&] {
        DegenerateFoldSet patched = real;
        patched.crease_interior_arcs = 0;
        return patched;
      }(),
      carrying, EscapeRegime::kSlabCreaseContradiction },
    { "not carried", real, LoopWithStrictMaximum(blade - 0.2), EscapeRegime::kSlabCreaseNotCarried },
    { "touching",
      [&] {
        DegenerateFoldSet patched = real;
        patched.crease_touching_arc = true;
        return patched;
      }(),
      carrying, EscapeRegime::kSlabCreaseTouching },
    { "closed ridge",
      [&] {
        DegenerateFoldSet patched = real;
        patched.crease_closed_ridge = true;
        return patched;
      }(),
      carrying, EscapeRegime::kSlabCreaseClosedRidge },
  };
  for (const auto& row : rows) {
    const PartitionResult result = IntervalPartition(field, interior, &row.fold_set, row.loop, topology);
    if (!result.escaped) {
      ADD_FAILURE() << row.what << ": expected an escape";
      continue;
    }
    EXPECT_EQ(result.regime, row.want) << row.what << ": " << result.message;
    EXPECT_TRUE(result.intervals.empty()) << row.what << ": an escape carries no intervals";
  }
}

// AC1 closed: twelve of the thirteen regimes are reachable, each through its own entry point
// above — kNotDiskUnaudited (ThinNeckFlipPartitionEndToEnd), kNotDiskUnconverged /
// kNotDiskConfirmed / kNotDiskCorrected (DiskEscapeAuditBranches), the four slab-crease forms
// (SlabCreaseEscapesThroughThePartition), and the four reasoning branches
// (SyntheticEscapeBranches). The thirteenth, kOddBoundaryCrossings, is PROVABLY unreachable
// through the partition's input: the crossing count is the number of sign changes of
// (v_j - delta) around the closed extrema sequence, which is even for any values whatsoever —
// the guard is a tripwire against a defective walk feeding inconsistent extrema, kept because LI
// keeps it (LI's own suite has no reachability test for it either, consistent with the parity
// argument). Documented-unreachable, not silently dropped; the SUMMARY reconciles AC1 against
// this finding.
TEST(DPPartition, AllEscapeRegimesAccountedFor) {
  EXPECT_EQ(static_cast<int>(EscapeRegime::kSublevelNotReachingBoundary) -
                static_cast<int>(EscapeRegime::kNotDiskUnaudited) + 1,
            13);
  // The twelve reachable regimes are reached by named tests above; grep-stable slugs all distinct
  // (EscapeRegimeNamesAreStableAndDistinct).
}

}  // namespace

}  // namespace lumice::analytic
