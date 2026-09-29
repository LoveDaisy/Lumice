// Component discovery (src/analytic/discovery.hpp): LI docs/phase1-math-contract.md section 9.5.
// The core's clustering, representative, dedup and classification branches on analytic maps, then
// the ice-crystal pipeline on LI's own scenes (LI tests/test_discovery.py, li_rev bfbd042), each
// case naming the section 11 conformance row (C15-C21) it is the equivalent of. The ice cases pin
// LI's numbers: this library's lattice, band and gates reproduce LI's pool exactly, so the funnel
// counts are compared exactly and lengths to the fixtures' 2e-3 relative (LI
// docs/analytic-parity-fixtures.md: closed_arclength_relative).
//
// symmetry_semantics: none — every case discovers one concrete face sequence (doc/analytic-api.md
// section 3).

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "analytic/discovery.hpp"
#include "analytic/so3.hpp"

namespace lumice::analytic {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kN = 1.31;
constexpr double kArclengthRtol = 2e-3;
using Mat = std::array<double, 9>;
using Vec = std::array<double, 3>;

const Mat kIdentity = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };
const Vec kE3 = { 0.0, 0.0, 1.0 };

Mat Exp(double x, double y, double z) {
  const double w[3] = { x, y, z };
  Mat r{};
  so3::Exp(w, r.data());
  return r;
}

double Length(const TraceResult& t) {
  double sum = 0.0;
  for (double x : t.arclength_increments) {
    sum += x;
  }
  return sum;
}

double Length(const DiscoveredComponent& c) {
  return Length(c.forward) + Length(c.backward);
}

// The funnel identities of section 9.5.6, which every result must satisfy.
void ExpectFunnelIdentities(const DiscoveryOutput& out) {
  EXPECT_EQ(out.admissible_count,
            out.dedup_merged + static_cast<int>(out.components.size()) + static_cast<int>(out.incomplete.size()));
  int arcs = 0;
  for (const auto& c : out.components) {
    arcs += c.kind == ComponentKind::kArc ? 1 : 0;
  }
  EXPECT_EQ(out.arc_stitched, arcs);
  EXPECT_EQ(static_cast<int>(out.incomplete.size()), out.arc_backward_failed + out.arc_backward_closed_anomaly +
                                                         out.incomplete_unnamed_event + out.incomplete_not_converged);
  EXPECT_LE(out.raw_cluster_count, out.pool_count + out.extra_seed_count);
  EXPECT_LE(out.admissible_count, out.raw_cluster_count);
}

// ---------------------------------------------------------------------------------------------
// The core on analytic maps
// ---------------------------------------------------------------------------------------------

TEST(Discovery, ArcEventsAreExactlyTheFiveNamedBoundaries) {
  const std::set<FiberReason> arc = { FiberReason::kTirBoundary, FiberReason::kBranchBoundary,
                                      FiberReason::kPathInfeasible, FiberReason::kVisibilityBoundary,
                                      FiberReason::kChartBoundary };
  for (int r = 0; r <= static_cast<int>(FiberReason::kEvaluationBudget); r++) {
    const auto reason = static_cast<FiberReason>(r);
    EXPECT_EQ(IsArcEvent(reason), arc.count(reason) == 1) << r;
  }
}

// C20 (LI test_geodesic_cluster_centres_are_the_lowest_unassigned_index_and_membership_is_not_transitive):
// poses at 0, 0.2 and 0.4 rad about one axis with radius 0.3: 1 joins 0, 2 is within 0.3 of 1 but not
// of the centre 0, so it starts its own cluster.
TEST(Discovery, ClusterCentresAreTheLowestUnassignedIndexAndMembershipIsNotTransitive) {
  std::vector<double> poses;
  for (double angle : { 0.0, 0.2, 0.4 }) {
    const Mat r = Exp(0.0, 0.0, angle);
    poses.insert(poses.end(), r.begin(), r.end());
  }
  const auto clusters = GeodesicCluster(poses.data(), 3, 0.3);
  ASSERT_EQ(clusters.size(), 2u);
  EXPECT_EQ(clusters[0], (std::vector<int>{ 0, 1 }));
  EXPECT_EQ(clusters[1], (std::vector<int>{ 2 }));
  // The order of the pool decides the centres: 1 first takes both neighbours.
  std::vector<double> reordered(poses.begin() + 9, poses.begin() + 18);
  reordered.insert(reordered.end(), poses.begin(), poses.begin() + 9);
  reordered.insert(reordered.end(), poses.begin() + 18, poses.end());
  EXPECT_EQ(GeodesicCluster(reordered.data(), 3, 0.3).size(), 1u);
}

// C20: membership is strictly below the radius — a pose at exactly the radius is not a member.
TEST(Discovery, ClusterMembershipIsStrictlyBelowTheRadius) {
  const Mat a = Exp(0.0, 0.0, 0.0);
  const Mat b = Exp(0.1, -0.2, 0.25);
  std::vector<double> poses(a.begin(), a.end());
  poses.insert(poses.end(), b.begin(), b.end());
  const double d = so3::Distance(a.data(), b.data());
  EXPECT_EQ(GeodesicCluster(poses.data(), 2, d).size(), 2u);
  EXPECT_EQ(GeodesicCluster(poses.data(), 2, std::nextafter(d, 1.0)).size(), 1u);
}

// LI test_geodesic_cluster_separates_two_tight_clusters / _of_empty_pool_is_empty.
TEST(Discovery, ClusterSeparatesTwoTightClustersAndAnEmptyPoolHasNone) {
  std::vector<double> poses;
  const Vec centres[2] = { { 1.0, 0.2, -0.3 }, { -0.5, 1.4, 0.8 } };
  for (const Vec& c : centres) {
    for (int k = 0; k < 5; k++) {
      const double j = 0.01 * (k - 2);
      const Mat r = Exp(c[0] + j, c[1] - 0.5 * j, c[2] + 0.7 * j);
      poses.insert(poses.end(), r.begin(), r.end());
    }
  }
  const auto clusters = GeodesicCluster(poses.data(), 10, 0.3);
  ASSERT_EQ(clusters.size(), 2u);
  EXPECT_EQ(clusters[0], (std::vector<int>{ 0, 1, 2, 3, 4 }));
  EXPECT_EQ(clusters[1], (std::vector<int>{ 5, 6, 7, 8, 9 }));
  EXPECT_TRUE(GeodesicCluster(nullptr, 0, 0.3).empty());
}

// C20 (section 9.5.4 step 2): an extra seed represents its cluster; otherwise the smallest offset,
// ties to the lowest index.
TEST(Discovery, RepresentativeIsTheWarmSeedElseTheSmallestOffset) {
  const double offsets[] = { 0.3, 0.1, 0.1, 0.05 };  // band members 2..5 (extra_count = 2)
  EXPECT_EQ(Representative({ 1, 3, 5 }, 2, offsets), 1);
  EXPECT_EQ(Representative({ 0, 1, 5 }, 2, offsets), 0);
  EXPECT_EQ(Representative({ 2, 3, 4 }, 2, offsets), 3);  // 3 and 4 tie at 0.1: the lower index
  EXPECT_EQ(Representative({ 2, 3, 4, 5 }, 2, offsets), 5);
}

// LI test_distance_to_curve_is_the_minimum_over_the_sampled_poses.
TEST(Discovery, DistanceToCurveIsTheMinimumOverTheSampledPoses) {
  TraceResult curve;
  for (int k = 0; k <= 10; k++) {
    const Mat r = Exp(0.0, 0.0, 0.1 * k);
    curve.poses.insert(curve.poses.end(), r.begin(), r.end());
    curve.residual_norms.push_back(0.0);
  }
  EXPECT_NEAR(DistanceToCurve(Exp(0.0, 0.0, 0.55).data(), curve), 0.05, 1e-12);
  Mat off{};
  const Mat sample = Exp(0.0, 0.0, 0.5);
  const Mat kick = Exp(0.3, 0.0, 0.0);
  so3::MatMul(sample.data(), kick.data(), off.data());
  EXPECT_NEAR(DistanceToCurve(off.data(), curve), 0.3, 1e-9);
  EXPECT_NEAR(DistanceToCurve(curve.poses.data() + 27, curve), 0.0, 1e-12);
  EXPECT_TRUE(std::isinf(DistanceToCurve(off.data(), TraceResult{})));
}

// LI analytic.direction_map F(R) = R e3; target e3: the fiber through the identity is the rotations
// about e3, a circle of length 2 pi.
struct BodyAxisMap {
  void Domain(const double* /*r*/, DomainEvaluation* out) const { *out = DomainEvaluation{}; }
  template <class S>
  void Direction(const S r[9], S out[3]) const {
    out[0] = r[2];
    out[1] = r[5];
    out[2] = r[8];
  }
};

// LI tests/test_discovery.py _capped_circle: the body-axis circle cut at theta_min / theta_max
// (angle about e3; `cut_lo` / `cut_hi` false = uncut), with the event kinds of each cut.
struct CappedCircle {
  bool cut_lo = false;
  double theta_min = 0.0;
  bool cut_hi = false;
  double theta_max = 0.0;
  FiberReason hi_kind = FiberReason::kVisibilityBoundary;
  FiberReason lo_kind = FiberReason::kTirBoundary;

  void Domain(const double r[9], DomainEvaluation* out) const {
    *out = DomainEvaluation{};
    const double theta = std::atan2(r[3], r[0]);
    if (cut_hi) {
      out->margins[out->margin_count++] = theta_max - theta;
    }
    if (cut_lo) {
      out->margins[out->margin_count++] = theta - theta_min;
    }
    if (cut_hi && theta_max - theta < 0.0) {
      out->valid = false;
      out->has_event = true;
      out->event = hi_kind;
      out->event_margin = theta_max - theta;
    } else if (cut_lo && theta - theta_min < 0.0) {
      out->valid = false;
      out->has_event = true;
      out->event = lo_kind;
      out->event_margin = theta - theta_min;
    }
  }
  template <class S>
  void Direction(const S r[9], S out[3]) const {
    BodyAxisMap{}.Direction(r, out);
  }
};

CappedCircle Capped(bool cut_lo, double lo, bool cut_hi, double hi) {
  CappedCircle c;
  c.cut_lo = cut_lo;
  c.theta_min = lo;
  c.cut_hi = cut_hi;
  c.theta_max = hi;
  return c;
}

template <class Map>
DiscoveryOutput Classify(const Map& map, const ContinuationParams& params = ContinuationParams{}) {
  DiscoveryOutput out;
  ClassifyAndTrace(map, MakeTargetChart(kE3.data()), params, kIdentity.data(), &out);
  return out;
}

// C18 (LI test_two_named_events_stitch_into_an_arc_component).
TEST(Discovery, TwoNamedEventsStitchIntoAnArcComponent) {
  const DiscoveryOutput out = Classify(Capped(true, -1.3, true, 2.0));
  ASSERT_EQ(out.components.size(), 1u);
  EXPECT_EQ(out.arc_stitched, 1);
  const DiscoveredComponent& c = out.components[0];
  EXPECT_EQ(c.kind, ComponentKind::kArc);
  // This library's +1 runs toward +theta on this chart; the two ends are the two cuts.
  const std::multiset<FiberReason> ends = { c.forward.reason, c.backward.reason };
  EXPECT_EQ(ends, (std::multiset<FiberReason>{ FiberReason::kTirBoundary, FiberReason::kVisibilityBoundary }));
  EXPECT_EQ(c.forward.poses[0], 1.0);  // both traces start at the seed
  EXPECT_EQ(c.backward.poses[0], 1.0);
  EXPECT_NEAR(Length(c), 3.3, 0.15);  // the cut circle's 3.3 rad, short of the untraced event tails
}

// LI test_closed_loop_is_a_closed_component_without_a_backward_trace.
TEST(Discovery, ClosedLoopIsAClosedComponentWithoutABackwardTrace) {
  const DiscoveryOutput out = Classify(BodyAxisMap{});
  ASSERT_EQ(out.components.size(), 1u);
  EXPECT_EQ(out.components[0].kind, ComponentKind::kClosed);
  EXPECT_EQ(out.components[0].forward.status, FiberStatus::kClosed);
  EXPECT_EQ(out.components[0].backward.PoseCount(), 0);
  EXPECT_NEAR(Length(out.components[0]), 2.0 * kPi, 1e-6 * 2.0 * kPi);
  EXPECT_EQ(out.arc_stitched + out.dedup_merged + out.arc_backward_failed + out.arc_backward_closed_anomaly +
                out.incomplete_unnamed_event + out.incomplete_not_converged,
            0);
}

// C18 (LI test_backward_trace_that_does_not_end_on_a_named_event_leaves_the_candidate_incomplete):
// cut on one side only; the other direction runs into its arclength budget.
TEST(Discovery, BackwardTraceNotEndingOnANamedEventLeavesTheCandidateIncomplete) {
  ContinuationParams p;
  p.maximum_arclength = 1.0;
  // Cut ahead of this library's +1 direction, so the forward trace is the one that meets it.
  const DiscoveryOutput plus = Classify(Capped(false, 0.0, true, 0.5), p);
  const DiscoveryOutput minus = Classify(Capped(true, -0.5, false, 0.0), p);
  const DiscoveryOutput& out =
      plus.incomplete.empty() || plus.incomplete[0].cause != IncompleteCause::kArcBackwardFailed ? minus : plus;
  ASSERT_EQ(out.incomplete.size(), 1u);
  const IncompleteCandidate& c = out.incomplete[0];
  EXPECT_EQ(c.cause, IncompleteCause::kArcBackwardFailed);
  EXPECT_EQ(out.arc_backward_failed, 1);
  EXPECT_TRUE(IsArcEvent(c.forward.reason));
  EXPECT_EQ(c.backward.status, FiberStatus::kBudgetExhausted);
  EXPECT_EQ(c.backward.reason, FiberReason::kArclengthBudget);
  EXPECT_TRUE(out.components.empty());
}

// C18 (LI test_unnamed_event_is_incomplete_and_not_traced_backward).
TEST(Discovery, UnnamedEventIsIncompleteAndNotTracedBackward) {
  for (bool hi : { true, false }) {
    CappedCircle map = hi ? Capped(false, 0.0, true, 0.5) : Capped(true, -0.5, false, 0.0);
    map.hi_kind = map.lo_kind = FiberReason::kRankLoss;
    ContinuationParams p;
    p.maximum_arclength = 1.0;
    int calls = 0;
    DiscoveryOutput out;
    ClassifyAndTrace(
        p, kIdentity.data(),
        [&](const ContinuationParams& q) {
          calls++;
          return TraceFiber(map, MakeTargetChart(kE3.data()), kIdentity.data(), q);
        },
        &out);
    if (out.incomplete.empty() || out.incomplete[0].cause != IncompleteCause::kUnnamedEvent) {
      continue;  // this side's cut is behind the +1 direction: the forward trace ran into the budget
    }
    EXPECT_EQ(out.incomplete_unnamed_event, 1);
    EXPECT_EQ(out.incomplete[0].forward.reason, FiberReason::kRankLoss);
    EXPECT_EQ(out.incomplete[0].backward.PoseCount(), 0);
    EXPECT_EQ(calls, 1);
    return;
  }
  FAIL() << "neither cut met the forward trace";
}

// C18 (LI test_backward_trace_that_closes_is_an_anomaly_not_a_component): no real one-dimensional
// fiber does this, so the traces are canned, as LI patches trace_fiber.
TEST(Discovery, BackwardTraceThatClosesIsAnAnomalyNotAComponent) {
  const auto chart = MakeTargetChart(kE3.data());
  const TraceResult forward = TraceFiber(Capped(true, -0.5, true, 0.5), chart, kIdentity.data(), ContinuationParams{});
  const TraceResult closed = TraceFiber(BodyAxisMap{}, chart, kIdentity.data(), ContinuationParams{});
  ASSERT_TRUE(IsArcEvent(forward.reason));
  ASSERT_EQ(closed.status, FiberStatus::kClosed);
  DiscoveryOutput out;
  ClassifyAndTrace(
      ContinuationParams{}, kIdentity.data(),
      [&](const ContinuationParams& q) { return q.initial_tangent_sign == -1 ? closed : forward; }, &out);
  ASSERT_EQ(out.incomplete.size(), 1u);
  EXPECT_EQ(out.incomplete[0].cause, IncompleteCause::kArcBackwardClosedAnomaly);
  EXPECT_EQ(out.arc_backward_closed_anomaly, 1);
  EXPECT_EQ(out.incomplete[0].backward.poses, closed.poses);
  EXPECT_EQ(out.incomplete[0].forward.poses, forward.poses);
  EXPECT_TRUE(out.components.empty());
}

// C18 (LI test_budget_exhausted_forward_trace_is_incomplete_not_converged).
TEST(Discovery, BudgetExhaustedForwardTraceIsIncompleteNotConverged) {
  ContinuationParams p;
  p.maximum_accepted_steps = 5;
  const DiscoveryOutput out = Classify(BodyAxisMap{}, p);
  ASSERT_EQ(out.incomplete.size(), 1u);
  EXPECT_EQ(out.incomplete[0].cause, IncompleteCause::kNotConverged);
  EXPECT_EQ(out.incomplete_not_converged, 1);
  EXPECT_EQ(out.incomplete[0].forward.status, FiberStatus::kBudgetExhausted);
  EXPECT_EQ(out.incomplete[0].backward.PoseCount(), 0);
}

// C18 (LI test_a_reversed_caller_orientation_still_traces_the_other_way_for_the_arc).
TEST(Discovery, AReversedOrientationSwapsTheArcEnds) {
  ContinuationParams reversed;
  reversed.initial_tangent_sign = -1;
  const DiscoveryOutput plus = Classify(Capped(true, -1.3, true, 2.0));
  const DiscoveryOutput minus = Classify(Capped(true, -1.3, true, 2.0), reversed);
  ASSERT_EQ(plus.components.size(), 1u);
  ASSERT_EQ(minus.components.size(), 1u);
  EXPECT_EQ(minus.components[0].forward.reason, plus.components[0].backward.reason);
  EXPECT_EQ(minus.components[0].backward.reason, plus.components[0].forward.reason);
  EXPECT_NEAR(Length(minus.components[0]), 3.3, 0.15);
}

// C20: dedup is strictly below eta, measured to the accepted curve; a pose at exactly eta from it is
// traced as its own component.
TEST(Discovery, DedupFoldsStrictlyBelowTheThresholdToTheAcceptedCurve) {
  const auto chart = MakeTargetChart(kE3.data());
  const ContinuationParams p;
  const TraceResult loop = TraceFiber(BodyAxisMap{}, chart, kIdentity.data(), p);
  // A second fiber pose, off the first loop's samples: halfway between two of them.
  const Mat kick = Exp(0.0, 0.0, 0.5 * loop.arclength_increments[3]);
  Mat second{};
  so3::MatMul(loop.poses.data() + 27, kick.data(), second.data());
  const double gap = DistanceToCurve(second.data(), loop);
  ASSERT_GT(gap, 0.0);
  std::vector<double> pool(kIdentity.begin(), kIdentity.end());
  pool.insert(pool.end(), second.begin(), second.end());
  const double offsets[] = { 0.0, 0.0 };
  auto admit = [](const double raw[9], double seed[9]) {
    std::copy(raw, raw + 9, seed);
    return true;
  };
  // A cluster radius far below the pose gap keeps the two candidates in two clusters.
  auto run = [&](double eta) {
    DiscoveryOutput out;
    out.pool_count = 2;  // the caller's count (DiscoverOnPool sets the rest)
    DiscoverOnPool(BodyAxisMap{}, chart, p, pool.data(), 2, 0, offsets, 1e-6, eta, admit, &out);
    return out;
  };
  const DiscoveryOutput at = run(gap);
  EXPECT_EQ(at.dedup_merged, 0);
  EXPECT_EQ(at.components.size(), 2u);
  const DiscoveryOutput above = run(std::nextafter(gap, 1.0));
  EXPECT_EQ(above.dedup_merged, 1);
  EXPECT_EQ(above.components.size(), 1u);
  ExpectFunnelIdentities(at);
  ExpectFunnelIdentities(above);
}

// ---------------------------------------------------------------------------------------------
// The ice-crystal pipeline on LI's scenes
// ---------------------------------------------------------------------------------------------

// LI canonical_scene: the regular prism of height ratio 2 (Lumice height 1), n = 1.31, the
// canonical sun (incident propagation direction below).
const Vec kCanonicalIncident = { -0.9659258262890683, 0.0, -0.25881904510252074 };

LUMICE_ANALYTIC_Crystal Prism(const std::array<double, 6>& fd = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 }) {
  LUMICE_ANALYTIC_Crystal c{};
  c.kind = LUMICE_ANALYTIC_CRYSTAL_PRISM;
  c.height = 1.0;
  for (int i = 0; i < 6; i++) {
    c.face_distance[i] = fd[i];
  }
  return c;
}

// LI REFERENCE_PYRAMID: Pyramid.from_lumice(0.5, 0.5, 0.5, wedges 90 deg - the ice face angle).
LUMICE_ANALYTIC_Crystal ReferencePyramid() {
  LUMICE_ANALYTIC_Crystal c{};
  c.kind = LUMICE_ANALYTIC_CRYSTAL_PYRAMID;
  c.height = 0.5;
  for (double& d : c.face_distance) {
    d = 1.0;
  }
  c.upper_h = 0.5;
  c.lower_h = 0.5;
  c.upper_wedge_deg = 27.999371488023023;
  c.lower_wedge_deg = 27.999371488023023;
  return c;
}

// One crystal and path, built the way the C wrapper builds a call.
struct Scene {
  FaceNormalTable table;
  FacePolygonTable polygons;
  std::vector<int> slots;
  Vec incident;

  Scene(const LUMICE_ANALYTIC_Crystal& crystal, const std::vector<int>& faces, const Vec& s = kCanonicalIncident)
      : incident(s) {
    EXPECT_EQ(BuildFaceNormals(crystal, &table, &polygons), Status::kOk);
    slots.resize(faces.size());
    EXPECT_EQ(ResolveFaceSequence(table, faces.data(), static_cast<int>(faces.size()), slots.data()), Status::kOk);
  }

  DiscoveryOutput Discover(const Vec& target, int n, const std::vector<Mat>& extra = {},
                           const ContinuationParams& p = ContinuationParams{}) const {
    IceDiscovery d(table, polygons, slots.data(), static_cast<int>(slots.size()), kN, incident.data());
    std::vector<double> flat;
    for (const Mat& m : extra) {
      flat.insert(flat.end(), m.begin(), m.end());
    }
    DiscoveryOutput out = d.Discover(target.data(), n, kDefaultBandHalfWidth, flat.data(),
                                     static_cast<int>(extra.size()), kDefaultClusterRadius, p.closure_distance, p);
    ExpectFunnelIdentities(out);
    return out;
  }
};

// LI camera.linear_pixel_outgoing_direction of the canonical render, (row, column) as named.
const Vec kPixel150 = { -0.9875255807607193, -0.010382806499944452, 0.1571159593179154 };
const Vec kPixel151 = { -0.987460586147287, -0.010383253351062983, 0.15752389931532262 };

// LI tests/test_discovery.py target_at_deviation: the canonical column at `delta` from s.
Vec TargetAtDeviation(double delta_deg) {
  const double d = delta_deg * kPi / 180.0;
  const Vec& s = kCanonicalIncident;
  // up = unit(s x e3) x s, the canonical column's plane
  const double across[3] = { s[1], -s[0], 0.0 };
  const double an = std::sqrt(across[0] * across[0] + across[1] * across[1]);
  const double a[3] = { across[0] / an, across[1] / an, 0.0 };
  const double up[3] = { a[1] * s[2] - a[2] * s[1], a[2] * s[0] - a[0] * s[2], a[0] * s[1] - a[1] * s[0] };
  return { std::cos(d) * s[0] + std::sin(d) * up[0], std::cos(d) * s[1] + std::sin(d) * up[1],
           std::cos(d) * s[2] + std::sin(d) * up[2] };
}

TEST(IceDiscovery, TargetAtDeviationHasThatDeviation) {
  for (double delta : { 22.0, 60.0, 64.7434 }) {
    const Vec t = TargetAtDeviation(delta);
    EXPECT_NEAR(TargetDeviation(kCanonicalIncident.data(), t.data()) * 180.0 / kPi, delta, 1e-9);
  }
  // LI's value of the same target, printed at li_rev bfbd042.
  const Vec t60 = TargetAtDeviation(60.0);
  EXPECT_NEAR(t60[0], -0.7071067811865477, 1e-15);
  EXPECT_NEAR(t60[2], 0.7071067811865475, 1e-15);
}

// Section 9.5.2: the lattice formula, and u in the band as LI's store holds it.
TEST(IceDiscovery, LatticeIsTheAntipodalFibonacciLattice) {
  double u[3];
  LatticePoint(4, 0, u);
  const double z = 1.0 - 2.0 * 0.5 / 4.0;
  const double r = std::sqrt(1.0 - z * z);
  const double phi = kPi * (1.0 + std::sqrt(5.0)) * 0.5;
  EXPECT_DOUBLE_EQ(u[0], -r * std::cos(phi));
  EXPECT_DOUBLE_EQ(u[1], -r * std::sin(phi));
  EXPECT_DOUBLE_EQ(u[2], -z);
  // Equal-area: the mean of u over the lattice is the origin, to O(1 / n).
  double mean[3] = { 0.0, 0.0, 0.0 };
  const int n = 100000;
  for (int i = 0; i < n; i++) {
    LatticePoint(n, i, u);
    EXPECT_NEAR(so3::Norm3(u), 1.0, 1e-15);
    for (int k = 0; k < 3; k++) {
      mean[k] += u[k] / n;
    }
  }
  EXPECT_LT(so3::Norm3(mean), 1e-4);
}

// Section 9.5.3: R_i u_i = s_hat, and the candidate's outgoing direction is |D_i - delta| from d, in
// the half-plane of d.
TEST(IceDiscovery, CandidatePosesPutTheSunOnUAndTheOffsetInTheAzimuthOfTheTarget) {
  const Scene scene(Prism(), { 3, 5 });
  IceDiscovery d(scene.table, scene.polygons, scene.slots.data(), 2, kN, kCanonicalIncident.data());
  const double delta = TargetDeviation(kCanonicalIncident.data(), kPixel150.data());
  const std::vector<BandEvent> band = d.BuildBand(100000, delta, kDefaultBandHalfWidth);
  ASSERT_GT(band.size(), 10u);
  for (const BandEvent& e : band) {
    double r[9];
    CandidatePose(kCanonicalIncident.data(), kPixel150.data(), e, r);
    double ru[3];
    for (int i = 0; i < 3; i++) {
      ru[i] = r[i * 3] * e.u[0] + r[i * 3 + 1] * e.u[1] + r[i * 3 + 2] * e.u[2];
    }
    for (int i = 0; i < 3; i++) {
      EXPECT_NEAR(ru[i], -kCanonicalIncident[i], 1e-12);
    }
    double out[3];
    d.Map().Direction(r, out);
    const double offset = std::acos(std::fmin(1.0, so3::Dot3(out, kPixel150.data())));
    EXPECT_NEAR(offset, std::fabs(e.deviation - delta), 1e-7);
    EXPECT_LE(std::fabs(e.deviation - delta), kDefaultBandHalfWidth);
  }
  for (size_t k = 1; k < band.size(); k++) {
    EXPECT_TRUE(band[k - 1].deviation < band[k].deviation ||
                (band[k - 1].deviation == band[k].deviation && band[k - 1].index < band[k].index));
  }
}

// Section 9.5.3: the band is inclusive at both ends — with half-width 0 an event exactly at delta is
// still in it.
TEST(IceDiscovery, BandIsInclusiveAtBothEnds) {
  const Scene scene(Prism(), { 3, 5 });
  IceDiscovery d(scene.table, scene.polygons, scene.slots.data(), 2, kN, kCanonicalIncident.data());
  const std::vector<BandEvent> band =
      d.BuildBand(100000, TargetDeviation(kCanonicalIncident.data(), kPixel150.data()), kDefaultBandHalfWidth);
  ASSERT_FALSE(band.empty());
  for (const BandEvent& e : { band.front(), band[band.size() / 2], band.back() }) {
    const std::vector<BandEvent> point = d.BuildBand(100000, e.deviation, 0.0);
    if (point.empty()) {
      ADD_FAILURE() << "point.empty() is true";
      continue;
    }
    EXPECT_TRUE(std::any_of(point.begin(), point.end(), [&e](const BandEvent& p) { return p.index == e.index; }));
  }
}

// C15 (LI test_canonical_pixel_has_one_closed_component, N = 1e6).
TEST(IceDiscovery, C15CanonicalPixelHasOneClosedComponent) {
  const Scene scene(Prism(), { 3, 5 });
  const DiscoveryOutput out = scene.Discover(kPixel150, kDefaultDiscoverySampleCount);
  EXPECT_TRUE(out.Complete());
  EXPECT_EQ(out.pool_count, 5024);
  EXPECT_EQ(out.extra_seed_count, 0);
  EXPECT_EQ(out.raw_cluster_count, 6);
  EXPECT_EQ(out.admissible_count, 6);
  EXPECT_EQ(out.dedup_merged, 5);
  ASSERT_EQ(out.components.size(), 1u);
  const DiscoveredComponent& c = out.components[0];
  EXPECT_EQ(c.kind, ComponentKind::kClosed);
  EXPECT_EQ(c.forward.reason, FiberReason::kClosedLoop);
  EXPECT_NEAR(Length(c), 2.379121, kArclengthRtol * 2.379121);
  EXPECT_TRUE(ValidateRotation(c.seed));
}

// C15 (LI test_rows_225_and_226_are_one_continuous_branch, test_caustic_edge_pixels_are_single_short_closed_loops).
TEST(IceDiscovery, C15RowsAndCausticPixelsAreSingleClosedLoops) {
  struct Case {
    Vec target;
    int pool;
    int admissible;
    double length;
  };
  const Case cases[] = {
    { { -0.9821674448204487, -0.01041146673727333, 0.18771976905816176 }, -1, -1, 3.121867 },     // (225, 150)
    { { -0.9820893806721556, -0.010411781853580404, 0.1881277309957669 }, -1, -1, 3.130201 },     // (226, 150)
    { { -0.9919272342160596, 0.051578389094207824, 0.11584486090856672 }, 3225, 1, 0.189729 },    // (49, 0)
    { { -0.9920628535647811, 0.047876443907869756, 0.11627183965019355 }, 3186, 1, 0.165418 },    // (50, 9)
    { { -0.9927171832399313, -0.0004134453690451046, 0.12046751912072942 }, 4218, 1, 0.466117 },  // (60, 126)
  };
  const Scene scene(Prism(), { 3, 5 });
  for (const Case& c : cases) {
    SCOPED_TRACE(c.length);
    const DiscoveryOutput out = scene.Discover(c.target, kDefaultDiscoverySampleCount);
    EXPECT_TRUE(out.Complete());
    if (c.pool >= 0) {
      EXPECT_EQ(out.pool_count, c.pool);
      EXPECT_EQ(out.admissible_count, c.admissible);
    }
    if (out.components.size() != 1u) {
      ADD_FAILURE() << "out.components.size() = " << out.components.size() << ", expected " << 1u;
      continue;
    }
    EXPECT_EQ(out.components[0].kind, ComponentKind::kClosed);
    EXPECT_EQ(out.dedup_merged, out.admissible_count - 1);
    EXPECT_NEAR(Length(out.components[0]), c.length, kArclengthRtol * c.length);
  }
}

// C16 (LI test_boundary_hugging_pixels_fold_every_candidate_into_one_closed_loop): loops along the exit
// TIR boundary; every candidate folds into one closed loop, none runs out of budget.
TEST(IceDiscovery, C16BoundaryHuggingPixelsFoldEveryCandidateIntoOneClosedLoop) {
  struct Case {
    Vec target;
    int pool;
    int clusters;
    double length;
  };
  const Case cases[] = {
    { { -0.9262116241489038, -0.010358269282414756, 0.37686169021130667 }, 1961, 14, 5.408495 },  // row 700
    { { -0.9133776525444067, -0.010310238289584585, 0.4069827549404039 }, 1737, 14, 5.635867 },   // row 780
  };
  const Scene scene(Prism(), { 3, 5 });
  for (const Case& c : cases) {
    SCOPED_TRACE(c.pool);
    const DiscoveryOutput out = scene.Discover(c.target, kDefaultDiscoverySampleCount);
    EXPECT_EQ(out.pool_count, c.pool);
    EXPECT_EQ(out.raw_cluster_count, c.clusters);
    EXPECT_EQ(out.admissible_count, c.clusters);
    EXPECT_TRUE(out.Complete());
    if (out.components.size() != 1u) {
      ADD_FAILURE() << "out.components.size() = " << out.components.size() << ", expected " << 1u;
      continue;
    }
    EXPECT_EQ(out.dedup_merged, c.clusters - 1);
    EXPECT_EQ(out.components[0].kind, ComponentKind::kClosed);
    EXPECT_NEAR(Length(out.components[0]), c.length, kArclengthRtol * c.length);
  }
}

// C21 (LI test_dark_pixel_has_no_admissible_candidate_and_is_procedurally_complete): row 40 lies inside
// the minimum deviation; no event is in its band, and the empty result is complete.
TEST(IceDiscovery, C21DarkPixelHasNoCandidateAndIsProcedurallyComplete) {
  const Scene scene(Prism(), { 3, 5 });
  const DiscoveryOutput out =
      scene.Discover({ -0.9936161962973833, -0.010323239346674772, 0.11234004266027031 }, kDefaultDiscoverySampleCount);
  EXPECT_EQ(out.pool_count, 0);
  EXPECT_EQ(out.raw_cluster_count, 0);
  EXPECT_EQ(out.admissible_count, 0);
  EXPECT_TRUE(out.components.empty());
  EXPECT_TRUE(out.Complete());
}

// C17 (LI test_path_1_3_at_60_deg_is_two_distinct_components, N = 1e5).
TEST(IceDiscovery, C17Path13At60DegIsTwoDistinctArcs) {
  const Scene scene(Prism(), { 1, 3 });
  const DiscoveryOutput out = scene.Discover(TargetAtDeviation(60.0), 100000);
  EXPECT_EQ(out.pool_count, 29);
  EXPECT_EQ(out.raw_cluster_count, 3);
  EXPECT_EQ(out.admissible_count, 3);
  EXPECT_TRUE(out.Complete());
  EXPECT_EQ(out.dedup_merged, 1);
  EXPECT_EQ(out.arc_stitched, 2);
  ASSERT_EQ(out.components.size(), 2u);
  // Far apart: the closest pose of one to the other's curve (LI: 0.821 rad).
  double separation = 1e9;
  const DiscoveredComponent& a = out.components[0];
  const DiscoveredComponent& b = out.components[1];
  for (const TraceResult* t : { &a.forward, &a.backward }) {
    for (int k = 0; k < t->PoseCount(); k++) {
      const double* pose = t->poses.data() + 9 * k;
      separation =
          std::fmin(separation, std::fmin(DistanceToCurve(pose, b.forward), DistanceToCurve(pose, b.backward)));
    }
  }
  EXPECT_GT(separation, 0.8);
  EXPECT_NEAR(Length(a), 0.6146, kArclengthRtol * 0.6146);
  EXPECT_NEAR(Length(b), 0.6145, kArclengthRtol * 0.6145);
  // C18 (LI test_path_1_3_components_are_arcs_cut_by_tir_and_path_infeasibility): each arc ends on exit
  // TIR at one end and on the entry ray leaving face 1 at the other, with accepted steps on both sides.
  for (const DiscoveredComponent& c : out.components) {
    EXPECT_EQ(c.kind, ComponentKind::kArc);
    EXPECT_EQ((std::multiset<FiberReason>{ c.forward.reason, c.backward.reason }),
              (std::multiset<FiberReason>{ FiberReason::kTirBoundary, FiberReason::kPathInfeasible }));
    EXPECT_GT(c.forward.PoseCount(), 1);
    EXPECT_GT(c.backward.PoseCount(), 1);
  }
}

// C18, known limitation pinned (LI test_a_seed_within_one_initial_step_of_two_events_is_a_single_pose_arc):
// path 3-1 at 64.7434 deg, N = 1e5 — the second arc's only candidate lies within one initial step of exit
// TIR on both sides, so it is an arc of one pose and zero length.
TEST(IceDiscovery, C18ASeedWithinOneInitialStepOfTwoEventsIsASinglePoseArc) {
  const Scene scene(Prism(), { 3, 1 });
  const DiscoveryOutput out = scene.Discover(TargetAtDeviation(64.7434), 100000);
  EXPECT_TRUE(out.Complete());
  ASSERT_EQ(out.components.size(), 2u);
  EXPECT_EQ(out.arc_stitched, 2);
  const bool first_long = Length(out.components[0]) > Length(out.components[1]);
  const DiscoveredComponent& lng = out.components[first_long ? 0 : 1];
  const DiscoveredComponent& shrt = out.components[first_long ? 1 : 0];
  EXPECT_NEAR(Length(lng), 0.2703, kArclengthRtol * 0.2703);
  EXPECT_EQ(Length(shrt), 0.0);
  EXPECT_EQ(shrt.forward.PoseCount(), 1);
  EXPECT_EQ(shrt.backward.PoseCount(), 1);
  EXPECT_EQ(shrt.forward.reason, FiberReason::kTirBoundary);
  EXPECT_EQ(shrt.backward.reason, FiberReason::kTirBoundary);
  EXPECT_GT(std::fmin(DistanceToCurve(shrt.seed, lng.forward), DistanceToCurve(shrt.seed, lng.backward)), 1.0);
}

// C19 (LI test_discovery_on_a_low_symmetry_prism): the D3h prism (face distances 1 / 1.2) reaches
// discovery only through the entry-measure gate; on this fixture the fiber, the pool and the
// component are the regular prism's, while the entry measure at the seed is not.
TEST(IceDiscovery, C19LowSymmetryPrismDiffersOnlyThroughTheEntryMeasure) {
  const Scene d3h(Prism({ 1.0, 1.2, 1.0, 1.2, 1.0, 1.2 }), { 3, 5 });
  const Scene d6h(Prism(), { 3, 5 });
  const Vec target = TargetAtDeviation(32.4379);
  const DiscoveryOutput low = d3h.Discover(target, 100000);
  const DiscoveryOutput regular = d6h.Discover(target, 100000);
  EXPECT_TRUE(low.Complete());
  ASSERT_EQ(low.components.size(), 1u);
  ASSERT_EQ(regular.components.size(), 1u);
  EXPECT_EQ(low.components[0].kind, ComponentKind::kClosed);
  EXPECT_NEAR(Length(low.components[0]), 4.7119, kArclengthRtol * 4.7119);
  EXPECT_EQ(low.pool_count, regular.pool_count);
  EXPECT_NEAR(Length(regular.components[0]), Length(low.components[0]), 1e-12);
  double s_body[3];
  const double* seed = low.components[0].seed;
  for (int i = 0; i < 3; i++) {
    s_body[i] =
        seed[i] * kCanonicalIncident[0] + seed[3 + i] * kCanonicalIncident[1] + seed[6 + i] * kCanonicalIncident[2];
  }
  Corridor low_corridor(d3h.table, d3h.polygons, d3h.slots.data(), 2);
  Corridor regular_corridor(d6h.table, d6h.polygons, d6h.slots.data(), 2);
  const double a_low = low_corridor.Evaluate(s_body, kN).value;
  const double a_regular = regular_corridor.Evaluate(s_body, kN).value;
  EXPECT_GT(a_low, 0.0);
  EXPECT_GT(a_regular, 0.0);
  EXPECT_GT(std::fabs(a_low - a_regular), 1e-3 * a_regular);
}

// C19 (LI test_discovery_runs_on_another_member_of_the_class): 3-1-2-5 has no w > 0 event on the
// canonical crystal (empty pool, zero components, complete); 3-7, a member of 3-5's class, is lit.
// LI serves 3-7 by transporting 3-5's sample; this library samples every path itself.
TEST(IceDiscovery, C19AnotherMemberIsLitAndAnUnlitMemberHasAnEmptyPool) {
  const Vec canonical_target = { -0.9875255807607193, -0.010382806499944452, 0.1571159593179154 };
  const DiscoveryOutput unlit = Scene(Prism(), { 3, 1, 2, 5 }).Discover(canonical_target, 100000);
  EXPECT_EQ(unlit.pool_count, 0);
  EXPECT_TRUE(unlit.components.empty());
  EXPECT_TRUE(unlit.Complete());
  const Scene lit(Prism(), { 3, 7 });
  const DiscoveryOutput out = lit.Discover(canonical_target, kDefaultDiscoverySampleCount);
  EXPECT_GT(out.pool_count, 0);
  ASSERT_GE(out.components.size(), 1u);
  for (const DiscoveredComponent& c : out.components) {
    EXPECT_GT(Length(c), 0.0);
    double outgoing[3];
    EXPECT_TRUE(TracePathChain<double>(lit.table, lit.slots.data(), 2, kN, kCanonicalIncident.data(), c.seed, outgoing,
                                       nullptr, nullptr));
  }
}

// C19 (LI's three pyramid tests, N = 1e5): 13-15-26-28 one closed loop below its interior maximum and
// dark above it; 13-5-26-28 one arc whose ends change across its saddle level; 13-24-26 one arc cut by
// the path domain up to its boundary extremum and an empty band beyond.
TEST(IceDiscovery, C19PyramidPaths) {
  struct Case {
    std::vector<int> faces;
    double delta_deg;
    int components;  // -1: pool must be empty
    ComponentKind kind;
    double length;
    std::multiset<FiberReason> ends;
  };
  const auto pi = FiberReason::kPathInfeasible;
  const auto tir = FiberReason::kTirBoundary;
  const Case cases[] = {
    { { 13, 15, 26, 28 }, 170.0, 1, ComponentKind::kClosed, 5.7793, {} },
    { { 13, 15, 26, 28 }, 176.5, 1, ComponentKind::kClosed, 2.9192, {} },
    { { 13, 15, 26, 28 }, 177.4, 0, ComponentKind::kClosed, 0.0, {} },
    { { 13, 5, 26, 28 }, 136.0, 1, ComponentKind::kArc, 2.1012, { pi, pi } },
    { { 13, 5, 26, 28 }, 136.7, 1, ComponentKind::kArc, 2.5247, { pi, tir } },
    { { 13, 24, 26 }, 90.0, 1, ComponentKind::kArc, 2.1205, { pi, pi } },
    { { 13, 24, 26 }, 130.5, 1, ComponentKind::kArc, 0.3686, { pi, pi } },
    { { 13, 24, 26 }, 131.5, -1, ComponentKind::kArc, 0.0, {} },
  };
  for (const Case& c : cases) {
    SCOPED_TRACE(c.delta_deg);
    const DiscoveryOutput out = Scene(ReferencePyramid(), c.faces).Discover(TargetAtDeviation(c.delta_deg), 100000);
    EXPECT_TRUE(out.Complete());
    if (c.components < 0) {
      EXPECT_EQ(out.pool_count, 0);
      EXPECT_TRUE(out.components.empty());
      continue;
    }
    if (static_cast<int>(out.components.size()) != c.components) {
      ADD_FAILURE() << "static_cast<int>(out.components.size()) = " << static_cast<int>(out.components.size())
                    << ", expected " << c.components;
      continue;
    }
    if (c.components == 0) {
      EXPECT_EQ(out.admissible_count, 0);
      continue;
    }
    const DiscoveredComponent& comp = out.components[0];
    EXPECT_EQ(comp.kind, c.kind);
    EXPECT_NEAR(Length(comp), c.length, kArclengthRtol * c.length);
    if (c.kind == ComponentKind::kArc) {
      EXPECT_EQ((std::multiset<FiberReason>{ comp.forward.reason, comp.backward.reason }), c.ends);
    }
  }
}

// C20 (LI test_warm_seed_from_the_row_above_is_a_newton_start_not_a_separate_trace): the canonical
// pixel's seed as the extra seed of the pixel below it — the same band, one component, traced from the
// warm seed's cluster, never an extra trace.
TEST(IceDiscovery, C20WarmSeedIsANewtonStartNotASeparateTrace) {
  const Scene scene(Prism(), { 3, 5 });
  const DiscoveryOutput above = scene.Discover(kPixel150, kDefaultDiscoverySampleCount);
  ASSERT_EQ(above.components.size(), 1u);
  Mat warm_seed;
  std::copy(above.components[0].seed, above.components[0].seed + 9, warm_seed.begin());
  const DiscoveryOutput cold = scene.Discover(kPixel151, kDefaultDiscoverySampleCount);
  const DiscoveryOutput warm = scene.Discover(kPixel151, kDefaultDiscoverySampleCount, { warm_seed });
  EXPECT_EQ(cold.extra_seed_count, 0);
  EXPECT_EQ(warm.extra_seed_count, 1);
  EXPECT_EQ(warm.pool_count, cold.pool_count);
  ASSERT_EQ(cold.components.size(), 1u);
  ASSERT_EQ(warm.components.size(), 1u);
  EXPECT_NEAR(Length(cold.components[0]), 2.391079, kArclengthRtol * 2.391079);
  EXPECT_NEAR(Length(warm.components[0]), Length(cold.components[0]), 1e-5 * Length(cold.components[0]));
  EXPECT_LT(so3::Distance(warm.components[0].seed, warm_seed.data()), 0.05);
  EXPECT_EQ(warm.dedup_merged, warm.admissible_count - 1);
}

// C20 (LI test_warm_seed_far_from_every_fiber_neither_poisons_nor_adds_a_component).
TEST(IceDiscovery, C20WarmSeedFarFromEveryFiberNeitherPoisonsNorAddsAComponent) {
  const Scene scene(Prism(), { 3, 5 });
  const DiscoveryOutput cold = scene.Discover(kPixel150, kDefaultDiscoverySampleCount);
  const DiscoveryOutput out = scene.Discover(kPixel150, kDefaultDiscoverySampleCount, { kIdentity });
  EXPECT_EQ(out.extra_seed_count, 1);
  EXPECT_TRUE(out.Complete());
  ASSERT_EQ(out.components.size(), 1u);
  EXPECT_NEAR(Length(out.components[0]), 2.379121, kArclengthRtol * 2.379121);
  EXPECT_GE(out.raw_cluster_count, cold.raw_cluster_count);
}

// C20 / C21 (LI test_continuation_options_are_the_single_trace_policy): the call's continuation options
// govern every trace; a starving budget leaves every candidate incomplete and the result unknown.
TEST(IceDiscovery, C21ContinuationOptionsAreTheSingleTracePolicy) {
  const Scene scene(Prism(), { 3, 5 });
  ContinuationParams small;
  small.maximum_accepted_steps = 50;
  const DiscoveryOutput starved = scene.Discover(kPixel150, kDefaultDiscoverySampleCount, {}, small);
  EXPECT_TRUE(starved.components.empty());
  EXPECT_GE(starved.incomplete.size(), 1u);
  EXPECT_FALSE(starved.Complete());
  for (const IncompleteCandidate& c : starved.incomplete) {
    EXPECT_EQ(c.cause, IncompleteCause::kNotConverged);
    EXPECT_EQ(c.forward.status, FiberStatus::kBudgetExhausted);
    EXPECT_EQ(c.forward.PoseCount(), 51);
  }
}

// Section 9.5.7: densification is not monotone, so the calling pattern is to pass the sparser call's
// components as extra seeds. On path 3-5 near its boundary (LI fixture 3-5__near_boundary's target)
// a denser sample finds a component the sparser one did not; the warm call keeps every sparse
// component (a pose within the dedup distance of one of its curves).
TEST(IceDiscovery, DensificationWithWarmSeedsKeepsTheSparseComponents) {
  const Scene scene(Prism(), { 3, 5 });
  const Vec target = { -0.8810956222125053, 0.0, 0.4729381614100923 };
  const DiscoveryOutput sparse = scene.Discover(target, 100000);
  std::vector<Mat> seeds;
  for (const DiscoveredComponent& c : sparse.components) {
    Mat m;
    std::copy(c.seed, c.seed + 9, m.begin());
    seeds.push_back(m);
  }
  const DiscoveryOutput dense = scene.Discover(target, kDefaultDiscoverySampleCount, seeds);
  EXPECT_EQ(dense.extra_seed_count, static_cast<int>(seeds.size()));
  EXPECT_GE(dense.components.size(), sparse.components.size());
  for (const DiscoveredComponent& c : sparse.components) {
    bool kept = false;
    for (const DiscoveredComponent& d : dense.components) {
      kept = kept || DistanceToCurve(c.seed, d.forward) < 0.08 || DistanceToCurve(c.seed, d.backward) < 0.08;
    }
    EXPECT_TRUE(kept);
  }
}

// ---------------------------------------------------------------------------------------------
// LI parity fixtures (li_rev bfbd042, `scripts/export_analytic_parity.py`): the band carried by
// 13-15-26-28__random and 3-5-6-7__random (u, phi, D in pool order, section 9.5.8), and their expected
// counts and component seeds. This library's own band is compared with LI's point by point, and the
// pipeline is run on LI's band. The fixtures themselves are replayed by the parity suite.
// ---------------------------------------------------------------------------------------------

LUMICE_ANALYTIC_Crystal AsymmetricPyramid() {
  LUMICE_ANALYTIC_Crystal c{};
  c.kind = LUMICE_ANALYTIC_CRYSTAL_PYRAMID;
  c.height = 0.5;
  const double fd[6] = { 1.0, 1.1, 0.9, 1.0, 1.2, 0.95 };
  std::copy(fd, fd + 6, c.face_distance);
  c.upper_h = 0.25;
  c.lower_h = 0.6;
  c.upper_wedge_deg = 27.996455531220374;
  c.lower_wedge_deg = 38.5704386184827;
  return c;
}

// u[3], phi[3], D
const double kPyramidBand[][7] = {
  { 0.7328926270789373, -0.6794705411372401, 0.03447, 0.8440561482466931, -0.23010248034861516, 0.48437802091380383,
    2.484296139209129 },
  { 0.7639691168659368, -0.6107507180307531, -0.20816999999999997, 0.7459509587503128, -0.5096080625376634,
    0.42878524897213594, 2.484656079944616 },
  { 0.7687477672675774, -0.6087878361310405, -0.19596999999999998, 0.7485794645475967, -0.49690187957144843,
    0.43899579421219387, 2.484786837748502 },
  { 0.48016569635479756, -0.8604855652154267, -0.17030999999999996, 0.8928078645355761, -0.4407948251077334,
    0.09270404080550354, 2.4852099261203846 },
  { 0.6945473277472736, -0.7173177910934045, 0.05530999999999997, 0.8782437470199475, -0.2215053834665801,
    0.42381987437552365, 2.485387262860339 },
  { 0.772281875858442, -0.6347538749157551, -0.02585000000000004, 0.8017200578822696, -0.2941116898823862,
    0.5203299555702898, 2.4855164271641956 },
  { 0.5630540133874915, -0.8253587590304222, -0.04186999999999996, 0.9203136853339802, -0.34204627693404005,
    0.18980796880676518, 2.4857755957521324 },
  { 0.5054555340030649, -0.8531475843285712, -0.12905, 0.903939336186021, -0.41139242279213417, 0.11683300460398326,
    2.4861895220284453 },
  { 0.6041069588403998, -0.7968556445057057, 0.008709999999999996, 0.9208608625151568, -0.29408526137318636,
    0.2559865834978605, 2.486303244717495 },
  { 0.7822470276711223, -0.6200070473790555, -0.06067, 0.7866118356712711, -0.3372157111684124, 0.5172304941929224,
    2.486547651259982 },
  { 0.6264824250356694, -0.7788490984917598, 0.03022999999999998, 0.9162272762451662, -0.2698089272217821,
    0.2961937221748555, 2.486669375878618 },
  { 0.7829671984689639, -0.6061895984109774, -0.13963000000000003, 0.7624731841416688, -0.4343172989213737,
    0.47958641277929, 2.487015554366598 },
  { 0.475140183195498, -0.8607740157631087, -0.18250999999999995, 0.8886607269674667, -0.4499953637152844,
    0.0882399284927381, 2.4872990983996264 },
  { 0.5770564552734937, -0.8163212280268046, -0.025009999999999977, 0.9211942364878322, -0.32729843844365697,
    0.21042079472795033, 2.48733369058723 },
  { 0.5907458691691749, -0.8068165191414688, -0.00814999999999999, 0.9214216952712355, -0.3113854622435899,
    0.23242451115755489, 2.487602589315596 },
  { 0.5444741842724553, -0.8357731736307671, -0.07093000000000005, 0.9158563193158052, -0.36699117589080643,
    0.16286398984302208, 2.48787685866707 },
  { 0.6716223131223633, -0.7389573787548007, 0.05353000000000008, 0.8948975899602595, -0.23338923835943115,
    0.3803784522042788, 2.487926171887135 },
  { 0.5252572079538825, -0.8450484396722425, -0.09999000000000002, 0.9103262371059854, -0.3899950269326732,
    0.1386002200728448, 2.487929334003842 },
  { 0.7381075705949295, -0.6741453769999844, 0.0269299999999999, 0.8394424483202619, -0.23910403779884457,
    0.48802216657276287, 2.4880455644589654 },
  { 0.7676572661821781, -0.6405989896789601, -0.01831000000000005, 0.8076717678970777, -0.2870856882050967,
    0.5150224489959775, 2.4889666635731733 },
  { 0.47004717524169226, -0.8608970141354342, -0.19471000000000005, 0.8844272372643158, -0.4590451878334537,
    0.08405936897194745, 2.4890557788654357 },
  { 0.5006785809906197, -0.8540312617446855, -0.14125, 0.9001186868309856, -0.4211929987936117, 0.11127806335889295,
    2.4892503876409657 },
  { 0.6357328445667378, -0.7711202488841049, 0.03489000000000009, 0.9128102268229779, -0.26398184471684893,
    0.3115944085943661, 2.4893700053457044 },
  { 0.7140096495363597, -0.6986952900005587, 0.044890000000000096, 0.8622399428270288, -0.2275353792884766,
    0.4525151181625759, 2.4897032565136987 },
  { 0.6628205437086874, -0.7471818051436477, 0.04886999999999997, 0.900162172717694, -0.24238593618845755,
    0.3618799811349145, 2.490525332085818 },
  { 0.7432415295068422, -0.6687421451623413, 0.01939000000000002, 0.834627008038481, -0.24780797763632492,
    0.49192373765913056, 2.490773009690596 },
  { 0.5589181280431252, -0.8274581326234993, -0.05406999999999995, 0.9175565031606426, -0.3537209379436384,
    0.18158073016673953, 2.4909755096033757 },
  { 0.6448739390883406, -0.7632649606687623, 0.039549999999999974, 0.9090336040810861, -0.2575369134643109,
    0.3276166126047757, 2.4910218398881225 },
  { 0.6136411509993898, -0.7894718366731924, 0.013370000000000104, 0.9182784230631515, -0.2897148751777471,
    0.2698703926654282, 2.491162381810424 },
};
const double kPrismBand[][7] = {
  { 0.9110566097852991, -0.39970077416301963, 0.10107, 0.5246227771024156, -0.8453140226243429, -0.10107000000000001,
    2.5075151755070433 },
  { 0.9100826228970702, -0.40264630558442877, -0.09811000000000003, 0.5205600494930993, -0.8481695955242334, 0.09811,
    2.507550394059322 },
  { 0.8999426078210007, -0.4306809725636095, -0.06794999999999995, 0.4824762986353237, -0.8732693849867615,
    0.06794999999999998, 2.5076261005360383 },
  { 0.9302356420181285, -0.32353330805207703, 0.17317000000000005, 0.6371515967383788, -0.7510326183820076,
    -0.17317000000000007, 2.507648147627772 },
  { 0.8998003316428288, -0.43107231490256404, 0.06735000000000002, 0.4819531168428944, -0.8736046993150964,
    -0.06735000000000002, 2.5076734865498826 },
  { 0.9077075461933113, -0.40971219860252844, -0.09057000000000004, 0.5108725794797997, -0.8548720855400859,
    0.09057000000000007, 2.50777334089484 },
  { 0.9325407792409752, -0.3070118794975765, 0.19002999999999992, 0.6650140924039903, -0.7222498570467817,
    -0.19002999999999984, 2.5077887959328673 },
  { 0.9026368253418673, -0.42373107206901345, -0.07548999999999995, 0.4918365860563571, -0.8674088035732793, 0.07549,
    2.5078200212315935 },
  { 0.891799303316535, -0.45080802843798456, 0.038289999999999935, 0.4556649464034849, -0.889327460792204,
    -0.038289999999999956, 2.5078409271855193 },
  { 0.9052253775573925, -0.41674096861968835, -0.08303000000000005, 0.5013001954087407, -0.8612805194494756,
    0.08303000000000005, 2.5078663236252132 },
  { 0.9056947179756699, -0.41548255526673056, 0.0842099999999999, 0.5030126322928062, -0.8601668254785593,
    -0.08420999999999985, 2.5079377867708446 },
  { 0.8881739741247497, -0.4592973542134207, 0.013889999999999958, 0.44446331911929887, -0.8956893579011952,
    -0.013889999999999948, 2.508084234231248 },
  { 0.8900630802645233, -0.455090128491081, 0.026089999999999947, 0.450024060514801, -0.8926352316919656,
    -0.026089999999999954, 2.508325876680988 },
  { 0.9273586466449791, -0.33996047475080515, 0.15630999999999995, 0.6111827515628663, -0.7759013004835368,
    -0.15630999999999987, 2.5083765255763777 },
  { 0.8905689475109825, -0.4541278472293687, -0.02559, 0.45131411042308206, -0.8919981645906133, 0.025590000000000005,
    2.5087553750957006 },
  { 0.9337707748766587, -0.2709163544089657, -0.23382999999999998, 0.7389136326195371, -0.6319257667076085,
    0.23382999999999995, 2.5090019710190203 },
  { 0.9239164916419339, -0.3562751941576346, 0.13945000000000007, 0.5865840636648134, -0.7977924753684215,
    -0.13945000000000005, 2.50941401250819 },
  { 0.889548558121026, -0.45680317451261626, -0.005850000000000022, 0.44781136036573665, -0.8941089212324123,
    0.005850000000000022, 2.5094232599081674 },
  { 0.8984787731925902, -0.4355391734647278, 0.05515000000000003, 0.4760569209787378, -0.8776834768230765,
    -0.055150000000000025, 2.509767785285962 },
  { 0.9342675738343065, -0.29041458018108995, 0.20689000000000002, 0.6957270808167397, -0.6878665255107396,
    -0.20689000000000002, 2.5097799693682545 },
  { 0.8937309195087979, -0.44737841545380647, -0.03312999999999999, 0.46030994789216056, -0.8871398733973793,
    0.033129999999999986, 2.5101603861131276 },
  { 0.9199160980803831, -0.372459480202294, 0.12258999999999998, 0.5630365739600103, -0.8172891215985774,
    -0.12258999999999999, 2.5104015138080933 },
  { 0.9338917992666624, -0.26865164202452924, 0.2359500000000001, 0.744745101407419, -0.624245409618407,
    -0.2359500000000002, 2.5107715626623506 },
  { 0.9046381975716606, -0.4200527245409759, 0.07200999999999991, 0.4969279136159051, -0.8647989411240868,
    -0.07200999999999992, 2.5108939666889647 },
  { 0.8916406289838429, -0.4526993110722578, 0.006350000000000078, 0.45329806747835355, -0.8913363784343091,
    -0.006350000000000079, 2.510945404971462 },
  { 0.91536461776207, -0.3884955387765727, 0.1057300000000001, 0.5403322717274233, -0.834782728157382,
    -0.1057300000000001, 2.51109051851763 },
  { 0.8970008909897869, -0.43993715353846663, 0.042950000000000044, 0.4702443148806864, -0.8814905455093629,
    -0.04295000000000004, 2.5111477375495275 },
  { 0.9102694398746264, -0.4043657625595102, 0.08887, 0.5183292742999663, -0.8505509311638373, -0.08886999999999992,
    2.511300065796701 },
  { 0.8967906536599272, -0.4405819726318823, -0.040669999999999984, 0.46939561888448306, -0.8820508511826586,
    0.04066999999999997, 2.511395361455349 },
  { 0.8928637701326152, -0.4501277550702382, -0.013390000000000013, 0.45672689815513046, -0.8895061823290454,
    0.013390000000000008, 2.5115578934682987 },
  { 0.8935807307229433, -0.4485190912108987, 0.018550000000000066, 0.4588655726841352, -0.8883120418553722,
    -0.018550000000000066, 2.511739886357897 },
  { 0.8953678401399217, -0.4442644126454106, 0.030750000000000055, 0.4645141428241689, -0.8850316653184387,
    -0.03075000000000006, 2.5118066000270187 },
  { 0.8997473990945574, -0.43374003011317663, -0.048209999999999975, 0.4785742390405925, -0.8767225864700419,
    0.04820999999999996, 2.512465449825196 },
  { 0.9034230062164769, -0.4245581653187037, 0.05980999999999992, 0.4909317428465964, -0.8691425589427798,
    -0.059809999999999926, 2.5131488680548824 },
  { 0.9026004155073784, -0.4268541055511917, -0.055749999999999966, 0.48784942740369014, -0.8711457247107919,
    0.05574999999999996, 2.513376494222436 },
  { 0.8960779255067102, -0.4434030745490956, -0.020930000000000004, 0.46573147552067623, -0.8846785449016681,
    0.020930000000000004, 2.5135222523640683 },
  { 0.8950073794511142, -0.4460497445667347, -0.0011900000000000244, 0.4622214147428191, -0.8867637496273442,
    0.0011900000000000242, 2.5136381406443262 },
  { 0.9306849161692174, -0.32850303790847435, 0.16097000000000006, 0.6292211465265148, -0.7603745181578992,
    -0.16096999999999997, 2.5140633326901454 },
  { 0.9053489729903494, -0.4199257232003293, -0.06328999999999996, 0.4972253777914841, -0.8653099442281447,
    0.06328999999999996, 2.5141351017620375 },
  { 0.9282306988982663, -0.2531882049441406, 0.27255000000000007, 0.811079291915019, -0.5175585761308865,
    -0.27255000000000007, 2.5143091291029434 },
  { 0.9275202201328592, -0.344874686146566, 0.14410999999999996, 0.6037791345894968, -0.7840172603548718,
    -0.14410999999999993, 2.5143303374123795 },
  { 0.9093212291254277, -0.4089701863972745, 0.07667000000000002, 0.5121319834564636, -0.8554779615051163,
    -0.07667000000000003, 2.514401570516348 },
};

struct FixturePin {
  const char* name;
  LUMICE_ANALYTIC_Crystal crystal;
  std::vector<int> faces;
  Vec target;
  const double (*band)[7];
  size_t band_size;
  int clusters;
  int dedup;
  ComponentKind kind;
  std::multiset<FiberReason> ends;
  double length;
  Mat seed;
};

std::vector<FixturePin> FixturePins() {
  return {
    { "13-15-26-28__random",
      AsymmetricPyramid(),
      { 13, 15, 26, 28 },
      { 0.6092534405934367, 2.1649348980190553e-15, 0.7929755640150972 },
      kPyramidBand,
      std::size(kPyramidBand),
      5,
      4,
      ComponentKind::kClosed,
      {},
      2.73513851842093,
      { 0.4948263878325212, -0.8641167190504158, -0.09191921323729592, 0.44175626796664913, 0.34122569015183785,
        -0.82970864048326, 0.7483303051572865, 0.3699558409296249, 0.5505764525887905 } },
    { "3-5-6-7__random",
      Prism(),
      { 3, 5, 6, 7 },
      { 0.6275025327724766, -5.551115123125783e-17, 0.7786145203912691 },
      kPrismBand,
      std::size(kPrismBand),
      6,
      5,
      ComponentKind::kArc,
      { FiberReason::kTirBoundary, FiberReason::kTirBoundary },
      2.467168223892592,
      { 0.9763888040245257, -0.17498786475368247, 0.12666550660895576, -0.15685644806433433, -0.1711426278424896,
        0.9726799348376668, -0.1485293172279377, -0.9695820997337532, -0.19454972063636772 } },
  };
}

TEST(IceDiscovery, BandMatchesLiParityFixtureBandsPointByPoint) {
  for (const FixturePin& pin : FixturePins()) {
    SCOPED_TRACE(pin.name);
    const Scene scene(pin.crystal, pin.faces);
    IceDiscovery d(scene.table, scene.polygons, scene.slots.data(), static_cast<int>(scene.slots.size()), kN,
                   kCanonicalIncident.data());
    const std::vector<BandEvent> band =
        d.BuildBand(100000, TargetDeviation(kCanonicalIncident.data(), pin.target.data()), kDefaultBandHalfWidth);
    if (band.size() != pin.band_size) {
      ADD_FAILURE() << "band.size() = " << band.size() << ", expected " << pin.band_size;
      continue;
    }
    for (size_t k = 0; k < band.size(); k++) {
      for (int i = 0; i < 3; i++) {
        EXPECT_NEAR(band[k].u[i], pin.band[k][i], 1e-15) << k;
        EXPECT_NEAR(band[k].phi[i], pin.band[k][3 + i], 1e-13) << k;
      }
      EXPECT_NEAR(band[k].deviation, pin.band[k][6], 1e-13) << k;
    }
  }
}

TEST(IceDiscovery, PipelineOnLiParityFixtureBandsMatchesLiCountsAndSeeds) {
  for (const FixturePin& pin : FixturePins()) {
    SCOPED_TRACE(pin.name);
    const Scene scene(pin.crystal, pin.faces);
    IceDiscovery d(scene.table, scene.polygons, scene.slots.data(), static_cast<int>(scene.slots.size()), kN,
                   kCanonicalIncident.data());
    std::vector<BandEvent> band(pin.band_size);
    for (size_t k = 0; k < band.size(); k++) {
      band[k].index = static_cast<int>(k);
      std::copy(pin.band[k], pin.band[k] + 3, band[k].u);
      std::copy(pin.band[k] + 3, pin.band[k] + 6, band[k].phi);
      band[k].deviation = pin.band[k][6];
    }
    const ContinuationParams p;
    const DiscoveryOutput out =
        d.DiscoverOnBand(pin.target.data(), band, nullptr, 0, kDefaultClusterRadius, p.closure_distance, p);
    ExpectFunnelIdentities(out);
    EXPECT_EQ(out.pool_count, static_cast<int>(pin.band_size));
    EXPECT_EQ(out.raw_cluster_count, pin.clusters);
    EXPECT_EQ(out.admissible_count, pin.clusters);
    EXPECT_EQ(out.dedup_merged, pin.dedup);
    EXPECT_TRUE(out.Complete());
    if (out.components.size() != 1u) {
      ADD_FAILURE() << "out.components.size() = " << out.components.size() << ", expected " << 1u;
      continue;
    }
    const DiscoveredComponent& c = out.components[0];
    EXPECT_EQ(c.kind, pin.kind);
    if (pin.kind == ComponentKind::kArc) {
      EXPECT_EQ((std::multiset<FiberReason>{ c.forward.reason, c.backward.reason }), pin.ends);
    } else {
      EXPECT_NEAR(Length(c), pin.length, kArclengthRtol * pin.length);
    }
    // The same representative, corrected by the same Gauss-Newton: LI's seed to rounding.
    EXPECT_LT(so3::Distance(c.seed, pin.seed.data()), 1e-9);
  }
}

// doc/analytic-api.md section 5.3: discovery holds no static or thread_local state; concurrent calls on
// distinct outputs are bit-identical to serial ones.
TEST(IceDiscoveryConcurrency, ConcurrentDiscoveriesMatchSerialResults) {
  const Vec targets[] = { kPixel150, kPixel151, TargetAtDeviation(32.4379) };
  auto run = [&targets](int i) { return Scene(Prism(), { 3, 5 }).Discover(targets[i % 3], 100000); };
  constexpr int kThreads = 6;
  std::vector<DiscoveryOutput> serial;
  for (int i = 0; i < kThreads; i++) {
    serial.push_back(run(i));
  }
  std::vector<DiscoveryOutput> parallel(kThreads);
  std::vector<std::thread> threads;
  for (int i = 0; i < kThreads; i++) {
    threads.emplace_back([&parallel, &run, i] { parallel[i] = run(i); });
  }
  for (auto& t : threads) {
    t.join();
  }
  for (int i = 0; i < kThreads; i++) {
    if (parallel[i].components.size() != serial[i].components.size()) {
      ADD_FAILURE() << "parallel[i].components.size() = " << parallel[i].components.size() << ", expected "
                    << serial[i].components.size() << " at " << i;
      continue;
    }
    EXPECT_EQ(parallel[i].raw_cluster_count, serial[i].raw_cluster_count) << i;
    for (size_t k = 0; k < serial[i].components.size(); k++) {
      EXPECT_EQ(parallel[i].components[k].forward.poses, serial[i].components[k].forward.poses) << i;
      EXPECT_EQ(parallel[i].components[k].backward.poses, serial[i].components[k].backward.poses) << i;
    }
  }
}

}  // namespace
}  // namespace lumice::analytic
