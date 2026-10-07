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

}  // namespace
}  // namespace lumice::analytic
