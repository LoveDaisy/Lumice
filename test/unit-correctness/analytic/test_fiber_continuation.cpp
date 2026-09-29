// The fiber continuation core (src/analytic/fiber_continuation.hpp) on analytic maps: LI's
// conformance matrix (LI docs/phase1-math-contract.md section 11) restated case by case. Each test
// names the LI C-number it stands for and the LI test it restates; the maps and options are LI's,
// the expectations are LI's where they are about geometry or classification. Diagnostics-only
// expectations (step diagnostics, terminal payload) have no counterpart: v0 returns no diagnostics.
// The ice-crystal adapter's cases are in test_path_fiber.cpp.
//
// symmetry_semantics: none — no face sequence is involved.

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <vector>

#include "analytic/fiber_continuation.hpp"
#include "analytic/jet.hpp"
#include "analytic/so3.hpp"

namespace lumice::analytic {
namespace {

constexpr double kPi = 3.14159265358979323846;
using Mat = std::array<double, 9>;

// ---------------------------------------------------------------------------------------------
// Analytic maps (LI analytic.py and test_reference_core_conformance.py)
// ---------------------------------------------------------------------------------------------

struct AlwaysValid {
  void Domain(const double* /*r*/, DomainEvaluation* out) const { *out = DomainEvaluation{}; }
};

// LI analytic.direction_map: F(R) = R e3. Its fibers are circles of length 2 pi.
struct BodyAxisMap : AlwaysValid {
  template <class S>
  void Direction(const S r[9], S out[3]) const {
    out[0] = r[2];
    out[1] = r[5];
    out[2] = r[8];
  }
};

// Constant map: rank 0 everywhere (LI test_rank_deficient_direction_map_has_typed_event_and_diagnostics).
struct ConstantMap : AlwaysValid {
  double value[3] = { 0.0, 0.0, 1.0 };
  template <class S>
  void Direction(const S* /*r*/, S out[3]) const {
    for (int i = 0; i < 3; i++) {
      out[i] = S(value[i]);
    }
  }
};

// LI _conjugation_map: F(R) = normalize((cos phi, sin(phi) u . a, 1)), a = e3. Its fiber through
// Rot(u0, pi/2) with u0 at cone angle beta from a has length 4 pi sin(pi/4) sin(beta).
struct ConjugationMap : AlwaysValid {
  template <class S>
  void Direction(const S r[9], S out[3]) const {
    const S cos_angle = (r[0] + r[4] + r[8] - 1.0) / 2.0;
    const S sin_axis_z = (r[3] - r[1]) / 2.0;
    const S lifted[3] = { cos_angle, sin_axis_z, S(1.0) };
    const S norm = Sqrt(lifted[0] * lifted[0] + lifted[1] * lifted[1] + lifted[2] * lifted[2]);
    for (int i = 0; i < 3; i++) {
      out[i] = lifted[i] / norm;
    }
  }
};

// A direction map that is never finite.
struct NanMap : AlwaysValid {
  template <class S>
  void Direction(const S* /*r*/, S out[3]) const {
    for (int i = 0; i < 3; i++) {
      out[i] = S(std::nan(""));
    }
  }
};

constexpr double kConjugationAngle = kPi / 2.0;

double ConjugationConeAngle(double loop_length) {
  return std::asin(loop_length / (4.0 * kPi * std::sin(kConjugationAngle / 2.0)));
}

Mat Exp(double x, double y, double z) {
  const double w[3] = { x, y, z };
  Mat r{};
  so3::Exp(w, r.data());
  return r;
}

const Mat kIdentity = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
const double kE3[3] = { 0.0, 0.0, 1.0 };

double Length(const TraceResult& r) {
  double s = 0.0;
  for (double a : r.arclength_increments) {
    s += a;
  }
  return s;
}

double MaxResidual(const TraceResult& r) {
  double m = 0.0;
  for (double v : r.residual_norms) {
    m = std::fmax(m, v);
  }
  return m;
}

void ExpectShapes(const TraceResult& r) {
  const size_t n = r.residual_norms.size();
  EXPECT_EQ(r.poses.size(), 9 * n);
  EXPECT_EQ(r.tangents.size(), 3 * n);
  EXPECT_EQ(r.arclength_increments.size(), n == 0 ? 0 : n - 1);
  // The per-pose diagnostics are aligned with the poses (N = 0 included: all empty).
  EXPECT_EQ(r.branch_margins.size(), static_cast<size_t>(r.branch_margin_count) * n);
  EXPECT_EQ(r.jacobian_available.size(), n);
  EXPECT_EQ(r.normal_jacobian.size(), n);
  EXPECT_EQ(r.singular_values.size(), 2 * n);
  for (double a : r.arclength_increments) {
    EXPECT_GE(a, 0.0);
  }
}

// Seed-relative section value at pose i of a trace.
double SectionAt(const TraceResult& r, int i) {
  return so3::SectionCoordinate(r.poses.data(), r.poses.data() + 9 * i, r.tangents.data());
}

// LI _assert_one_traversal_crossings: over the open loop the section coordinate changes sign once
// (at the antipode, seed distance pi); the closing pose sits on the section.
void ExpectOneTraversal(const TraceResult& r) {
  const int n = r.PoseCount();
  int sign_changes = 0;
  int at = -1;
  for (int i = 1; i + 2 < n; i++) {
    if (SectionAt(r, i) * SectionAt(r, i + 1) < 0.0) {
      sign_changes++;
      at = i + 1;
    }
  }
  EXPECT_EQ(sign_changes, 1);
  if (at >= 0) {
    EXPECT_GE(so3::Distance(r.poses.data(), r.poses.data() + 9 * at), kPi - 0.2);
  }
  EXPECT_LE(std::fabs(SectionAt(r, n - 1)), 1e-12);
}

// ---------------------------------------------------------------------------------------------
// C01 — rank two, singular values (1, 1), J_perp = 1 at the identity
// LI: test_regular_state_reports_analytic_singular_values_and_tangent
// ---------------------------------------------------------------------------------------------

TEST(FiberContinuation, C01AnalyticJacobianHasUnitSingularValues) {
  const TargetChart chart = MakeTargetChart(kE3);
  double a[2][3];
  fiber_detail::LocalResidualJacobian(BodyAxisMap{}, chart, kIdentity.data(), a);
  const auto svd = so3::SvdTwoByThree(a[0], a[1]);
  EXPECT_NEAR(svd.sigma1, 1.0, 1e-14);
  EXPECT_NEAR(svd.sigma2, 1.0, 1e-14);
  EXPECT_NEAR(svd.sigma1 * svd.sigma2, 1.0, 1e-14);

  const auto state =
      fiber_detail::EvaluateRegularState(BodyAxisMap{}, chart, ContinuationParams{}, kIdentity.data(), nullptr);
  ASSERT_TRUE(state.accepted);
  EXPECT_NEAR(so3::Norm3(state.tangent), 1.0, 1e-14);
  // The fiber of R e3 through I is rotation about e3; +1 is (row 0 x row 1) of A, here +e3.
  EXPECT_NEAR(state.tangent[2], 1.0, 1e-14);
}

// The recorded normal Jacobian is taken in the chart at the pose's OWN direction (LI contract
// section 5.4), not the traced target's. Off the fiber the two differ: at R = exp(0.3 e1), R e3 is
// 0.3 rad from the target e3, and projecting its unit-singular-value derivative onto e3's tangent
// plane shortens one singular value to cos(0.3), while the own chart keeps (1, 1).
TEST(FiberContinuation, NormalJacobianUsesThePosesOwnDirection) {
  const TargetChart chart = MakeTargetChart(kE3);
  const Mat pose = Exp(0.3, 0.0, 0.0);
  double a[2][3];
  fiber_detail::LocalResidualJacobian(BodyAxisMap{}, chart, pose.data(), a);
  const auto target_svd = so3::SvdTwoByThree(a[0], a[1]);
  EXPECT_NEAR(target_svd.sigma1 * target_svd.sigma2, std::cos(0.3), 1e-14);

  const NormalJacobian own = fiber_detail::NormalJacobianAt(BodyAxisMap{}, pose.data());
  ASSERT_TRUE(own.available);
  EXPECT_NEAR(own.singular_values[0], 1.0, 1e-14);
  EXPECT_NEAR(own.singular_values[1], 1.0, 1e-14);
  EXPECT_NEAR(own.value, 1.0, 1e-14);

  // EvaluateRegularState records the same numbers from its one forward-mode evaluation.
  const auto state =
      fiber_detail::EvaluateRegularState(BodyAxisMap{}, chart, ContinuationParams{}, pose.data(), nullptr);
  ASSERT_TRUE(state.accepted);
  EXPECT_EQ(state.normal.value, own.value);
  EXPECT_EQ(state.normal.singular_values[0], own.singular_values[0]);
  EXPECT_EQ(state.normal.singular_values[1], own.singular_values[1]);
}

// A rank-0 map still has a normal Jacobian (J_perp = 0, available): availability says whether A
// could be formed, not whether the pose is regular. A non-finite direction cannot form it: NaN, not
// a plausible 0 or 1.
TEST(FiberContinuation, NormalJacobianIsUnavailableOnlyWhenItCannotBeFormed) {
  const NormalJacobian rank0 = fiber_detail::NormalJacobianAt(ConstantMap{}, kIdentity.data());
  EXPECT_TRUE(rank0.available);
  EXPECT_EQ(rank0.value, 0.0);

  const NormalJacobian none = fiber_detail::NormalJacobianAt(NanMap{}, kIdentity.data());
  EXPECT_FALSE(none.available);
  EXPECT_TRUE(std::isnan(none.value));
  EXPECT_TRUE(std::isnan(none.singular_values[0]));
  EXPECT_TRUE(std::isnan(none.singular_values[1]));
}

// LI test_tangent_orientation_is_continuous.
TEST(FiberContinuation, TangentOrientationFollowsThePreviousTangent) {
  const TargetChart chart = MakeTargetChart(kE3);
  const ContinuationParams p;
  const auto first = fiber_detail::EvaluateRegularState(BodyAxisMap{}, chart, p, kIdentity.data(), nullptr);
  const double minus[3] = { -first.tangent[0], -first.tangent[1], -first.tangent[2] };
  const Mat second_pose = Exp(0.1 * first.tangent[0], 0.1 * first.tangent[1], 0.1 * first.tangent[2]);
  const auto along = fiber_detail::EvaluateRegularState(BodyAxisMap{}, chart, p, second_pose.data(), first.tangent);
  const auto against = fiber_detail::EvaluateRegularState(BodyAxisMap{}, chart, p, second_pose.data(), minus);
  EXPECT_GE(so3::Dot3(along.tangent, first.tangent), 0.0);
  EXPECT_LE(so3::Dot3(against.tangent, first.tangent), 0.0);
}

// ---------------------------------------------------------------------------------------------
// C02 — the analytic circle closes with length 2 pi
// LI: test_analytic_circle_truth_closure_and_haar_normalization
// ---------------------------------------------------------------------------------------------

TEST(FiberContinuation, C02AnalyticCircleClosesAtTwoPi) {
  const TargetChart chart = MakeTargetChart(kE3);
  for (double step : { 0.04, 0.08, 0.12 }) {
    ContinuationParams p;
    p.initial_step = step;
    const TraceResult r = TraceFiber(BodyAxisMap{}, chart, kIdentity.data(), p);
    SCOPED_TRACE(step);
    ExpectShapes(r);
    EXPECT_EQ(r.status, FiberStatus::kClosed);
    EXPECT_EQ(r.reason, FiberReason::kClosedLoop);
    EXPECT_LE(MaxResidual(r), 1e-11);
    EXPECT_NEAR(Length(r), 2.0 * kPi, 1e-12);
    // Haar -> sphere density: (2 pi) / (8 pi^2) = 1 / (4 pi).
    EXPECT_NEAR(Length(r) / (8.0 * kPi * kPi), 1.0 / (4.0 * kPi), 2e-14);
    const int n = r.PoseCount();
    EXPECT_LE(so3::Distance(r.poses.data(), r.poses.data() + 9 * (n - 1)), 1e-12);
    for (int i = 0; i + 1 < n; i++) {
      EXPECT_GT(so3::Dot3(&r.tangents[3 * i], &r.tangents[3 * (i + 1)]), 0.999999999999);
    }
    // Per-pose diagnostics: J_perp = 1, singular values (1, 1) everywhere on the circle (C01's closed
    // form), and every row — the closure-replaced last one included — is a fresh evaluation's.
    EXPECT_EQ(r.branch_margin_count, 0);
    for (int i = 0; i < n; i++) {
      EXPECT_EQ(r.jacobian_available[i], 1);
      EXPECT_NEAR(r.normal_jacobian[i], 1.0, 1e-13);
      EXPECT_NEAR(r.singular_values[2 * i], 1.0, 1e-13);
      EXPECT_NEAR(r.singular_values[2 * i + 1], 1.0, 1e-13);
      const NormalJacobian fresh = fiber_detail::NormalJacobianAt(BodyAxisMap{}, &r.poses[9 * i]);
      EXPECT_EQ(r.normal_jacobian[i], fresh.value) << i;
    }
  }
}

// ---------------------------------------------------------------------------------------------
// C03 — invariance under an orthogonal change of the target basis
// LI: test_analytic_circle_is_invariant_under_orthogonal_target_basis
// ---------------------------------------------------------------------------------------------

double SetDistance(const TraceResult& a, const TraceResult& b) {
  auto one_way = [](const TraceResult& x, const TraceResult& y) {
    double worst = 0.0;
    for (int i = 0; i < x.PoseCount(); i++) {
      double best = 1e300;
      for (int j = 0; j < y.PoseCount(); j++) {
        best = std::fmin(best, so3::Distance(&x.poses[9 * i], &y.poses[9 * j]));
      }
      worst = std::fmax(worst, best);
    }
    return worst;
  };
  return std::fmax(one_way(a, b), one_way(b, a));
}

TargetChart Rebased(const TargetChart& chart, const double q[2][2]) {
  // basis' = basis Q (columns mixed by Q).
  TargetChart c = chart;
  for (int j = 0; j < 2; j++) {
    for (int i = 0; i < 3; i++) {
      c.basis[j][i] = chart.basis[0][i] * q[0][j] + chart.basis[1][i] * q[1][j];
    }
  }
  return c;
}

TEST(FiberContinuation, C03AnalyticCircleIsInvariantUnderTargetBasis) {
  const TargetChart chart = MakeTargetChart(kE3);
  const ContinuationParams p;
  const TraceResult reference = TraceFiber(BodyAxisMap{}, chart, kIdentity.data(), p);
  const double rotation[2][2] = { { 0.0, -1.0 }, { 1.0, 0.0 } };
  const double reflection[2][2] = { { 1.0, 0.0 }, { 0.0, -1.0 } };
  for (const auto* q : { rotation, reflection }) {
    const TraceResult changed = TraceFiber(BodyAxisMap{}, Rebased(chart, q), kIdentity.data(), p);
    EXPECT_EQ(changed.status, reference.status);
    EXPECT_EQ(changed.reason, reference.reason);
    // The tangent agrees in sign too: the orientation convention is chart-invariant.
    EXPECT_GE(so3::Dot3(changed.tangents.data(), reference.tangents.data()), 1.0 - 1e-14);
    EXPECT_LE(SetDistance(changed, reference), 1e-12);
    EXPECT_NEAR(Length(changed), Length(reference), 2e-12);
  }
}

// ---------------------------------------------------------------------------------------------
// C04 — the antipode of the target is rejected by the chart gate
// LI: test_antipode_algebraic_root_is_rejected_by_chart_gate, test_public_rank_loss_and_antipode_events_...
// ---------------------------------------------------------------------------------------------

TEST(FiberContinuation, C04AntipodeIsAChartBoundaryNotARoot) {
  ConstantMap antipode;
  antipode.value[2] = -1.0;  // F = -e3: the projected residual basis^T (F - d) is exactly zero
  const TraceResult r = TraceFiber(antipode, MakeTargetChart(kE3), kIdentity.data(), ContinuationParams{});
  EXPECT_EQ(r.status, FiberStatus::kEventTerminated);
  EXPECT_EQ(r.reason, FiberReason::kChartBoundary);
  EXPECT_EQ(r.PoseCount(), 0);
  ExpectShapes(r);
}

// ---------------------------------------------------------------------------------------------
// C07 — a rank-deficient map ends in rank_loss with no regular samples
// LI: test_rank_deficient_direction_map_has_typed_event_and_diagnostics
// ---------------------------------------------------------------------------------------------

TEST(FiberContinuation, C07RankDeficientMapIsRankLoss) {
  const auto state = fiber_detail::EvaluateRegularState(ConstantMap{}, MakeTargetChart(kE3), ContinuationParams{},
                                                        kIdentity.data(), nullptr);
  EXPECT_FALSE(state.accepted);
  EXPECT_EQ(state.reason, FiberReason::kRankLoss);
  const TraceResult r = TraceFiber(ConstantMap{}, MakeTargetChart(kE3), kIdentity.data(), ContinuationParams{});
  EXPECT_EQ(r.status, FiberStatus::kEventTerminated);
  EXPECT_EQ(r.reason, FiberReason::kRankLoss);
  EXPECT_EQ(r.PoseCount(), 0);
}

// A map of rank one (depends on one rotation angle only): still rank_loss, through the sigma2 gate
// rather than a zero Jacobian.
struct RankOneMap : AlwaysValid {
  template <class S>
  void Direction(const S r[9], S out[3]) const {
    // (sin t, 0, cos t) with t the rotation's first-row x component: one scalar of the pose.
    const S t = r[0] - 1.0;
    out[0] = Sin(t);
    out[1] = S(0.0);
    out[2] = Cos(t);
  }
};

TEST(FiberContinuation, C07RankOneMapIsRankLoss) {
  const Mat seed = Exp(0.3, 0.2, 0.1);
  double target[3];
  RankOneMap{}.Direction(seed.data(), target);
  const TraceResult r = TraceFiber(RankOneMap{}, MakeTargetChart(target), seed.data(), ContinuationParams{});
  EXPECT_EQ(r.reason, FiberReason::kRankLoss);
  EXPECT_EQ(r.PoseCount(), 0);
}

// ---------------------------------------------------------------------------------------------
// C08 — a known domain event is reported before any unsafe evaluation
// LI: test_known_event_precedes_unsafe_direction_evaluation, test_domain_event_is_only_believed_inside_...
// ---------------------------------------------------------------------------------------------

// Domain boundary at rotation angle theta_max about e3 (LI's `cap`): margin = theta_max - angle.
// Beyond it the direction map returns NaN, so the only way to report the boundary as a boundary is
// to have asked the domain first.
struct CappedBodyAxisMap {
  double theta_max = 0.1;
  void Domain(const double r[9], DomainEvaluation* out) const {
    *out = DomainEvaluation{};
    const double margin = theta_max - std::atan2(r[3], r[0]);
    out->margin_count = 1;
    out->margins[0] = margin;
    if (margin < 0.0) {
      out->valid = false;
      out->has_event = true;
      out->event = FiberReason::kTirBoundary;
      out->event_margin = margin;
    }
  }
  template <class S>
  void Direction(const S r[9], S out[3]) const {
    const bool unsafe = theta_max - std::atan2(ValueOf(r[3]), ValueOf(r[0])) < 0.0;
    const double poison = unsafe ? std::nan("") : 0.0;
    out[0] = r[2] + poison;
    out[1] = r[5] + poison;
    out[2] = r[8] + poison;
  }
};

TEST(FiberContinuation, C08DomainEventPrecedesTheUnsafeDirection) {
  const TraceResult r = TraceFiber(CappedBodyAxisMap{}, MakeTargetChart(kE3), kIdentity.data(), ContinuationParams{});
  EXPECT_EQ(r.status, FiberStatus::kEventTerminated);
  EXPECT_EQ(r.reason, FiberReason::kTirBoundary);  // not non_finite, not corrector_failure
  EXPECT_GT(r.PoseCount(), 1);
  // Every accepted pose is inside the domain, and the event-approach rule has brought the last
  // one close to the boundary.
  const int n = r.PoseCount();
  for (int i = 0; i < n; i++) {
    EXPECT_GT(0.1 - std::atan2(r.poses[9 * i + 3], r.poses[9 * i]), 0.0) << i;
  }
  // Without the rule the last pose of this trace stops ~0.01 short (steps 0.04, 0.05, then a
  // crossing trial); with it the step halves toward the boundary down to minimum_step.
  const double last_margin = 0.1 - std::atan2(r.poses[9 * (n - 1) + 3], r.poses[9 * (n - 1)]);
  EXPECT_LT(last_margin, 1e-3);
}

// The recorded margins are the map's Domain() margins at each pose, row by row.
TEST(FiberContinuation, BranchMarginsAreTheDomainMarginsOfEachPose) {
  const CappedBodyAxisMap map;
  const TraceResult r = TraceFiber(map, MakeTargetChart(kE3), kIdentity.data(), ContinuationParams{});
  ExpectShapes(r);
  ASSERT_GE(r.PoseCount(), 2);
  ASSERT_EQ(r.branch_margin_count, 1);
  for (int i = 0; i < r.PoseCount(); i++) {
    DomainEvaluation d;
    map.Domain(&r.poses[9 * i], &d);
    EXPECT_EQ(r.branch_margins[i], d.margins[0]) << i;
    EXPECT_GT(r.branch_margins[i], 0.0) << i;
  }
}

TEST(FiberContinuation, C08DomainEventIsOnlyBelievedInsideTheTrustRegion) {
  const CappedBodyAxisMap map;
  const TargetChart chart = MakeTargetChart(kE3);
  const ContinuationParams p;
  // A predictor 0.4 rad past the cap: invalid domain with zero correction but an advance beyond
  // maximum_advance = 0.2 — the corrector running away, not an event.
  const Mat runaway = Exp(0.0, 0.0, 0.5);
  auto o = fiber_detail::CorrectTrial(map, chart, p, kIdentity.data(), runaway.data(), kE3, 1000);
  EXPECT_FALSE(o.accepted);
  EXPECT_EQ(o.reason, FiberReason::kCorrectorFailure);
  EXPECT_NEAR(o.advance, 0.5, 1e-9);
  // The same cap crossed inside the trust region is the event.
  const Mat near = Exp(0.0, 0.0, 0.15);
  o = fiber_detail::CorrectTrial(map, chart, p, kIdentity.data(), near.data(), kE3, 1000);
  EXPECT_FALSE(o.accepted);
  EXPECT_EQ(o.reason, FiberReason::kTirBoundary);
}

// ---------------------------------------------------------------------------------------------
// C10 — non-finite evaluator output never appears closed
// LI: test_public_nonfinite_evaluator_output_never_appears_closed
// ---------------------------------------------------------------------------------------------

struct NonFiniteAfterSeedMap : AlwaysValid {
  template <class S>
  void Direction(const S r[9], S out[3]) const {
    const double poison = ValueOf(r[3]) > 0.0 ? std::nan("") : 0.0;  // LI: where(R[1,0] > 0, nan, 0)
    out[0] = r[2] + poison;
    out[1] = r[5] + poison;
    out[2] = r[8] + poison;
  }
};

TEST(FiberContinuation, C10NonFiniteOutputNeverAppearsClosed) {
  ContinuationParams p;
  p.maximum_retries = 2;
  const TraceResult r = TraceFiber(NonFiniteAfterSeedMap{}, MakeTargetChart(kE3), kIdentity.data(), p);
  EXPECT_EQ(r.status, FiberStatus::kNumericalFailure);
  EXPECT_EQ(r.reason, FiberReason::kNonFinite);
  EXPECT_EQ(r.PoseCount(), 1);  // only the seed
}

// ---------------------------------------------------------------------------------------------
// C11 — step underflow and each budget are distinct, and budgets keep the partial trace
// LI: test_public_corrector_nonconvergence_and_step_underflow_are_distinct,
//     test_public_budget_termination_retains_partial_geometry
// ---------------------------------------------------------------------------------------------

TEST(FiberContinuation, C11CorrectorFailureAndStepUnderflowAreDistinct) {
  const TargetChart chart = MakeTargetChart(kE3);
  ContinuationParams corrector;
  corrector.maximum_advance = 0.01;
  corrector.maximum_retries = 0;
  const TraceResult c = TraceFiber(BodyAxisMap{}, chart, kIdentity.data(), corrector);
  EXPECT_EQ(c.status, FiberStatus::kNumericalFailure);
  EXPECT_EQ(c.reason, FiberReason::kCorrectorFailure);

  ContinuationParams underflow;
  underflow.initial_step = 0.04;
  underflow.minimum_step = 0.03;
  underflow.maximum_step = 0.04;
  underflow.maximum_advance = 0.01;
  const TraceResult u = TraceFiber(BodyAxisMap{}, chart, kIdentity.data(), underflow);
  EXPECT_EQ(u.status, FiberStatus::kNumericalFailure);
  EXPECT_EQ(u.reason, FiberReason::kStepUnderflow);
}

TEST(FiberContinuation, C11BudgetsAreDistinctAndKeepThePartialTrace) {
  const TargetChart chart = MakeTargetChart(kE3);
  struct Case {
    ContinuationParams p;
    FiberReason reason;
  };
  std::vector<Case> cases(3);
  cases[0].p.maximum_accepted_steps = 1;
  cases[0].reason = FiberReason::kStepBudget;
  cases[1].p.maximum_arclength = 0.01;
  cases[1].reason = FiberReason::kArclengthBudget;
  cases[2].p.maximum_evaluations = 1;
  cases[2].reason = FiberReason::kEvaluationBudget;
  for (const auto& c : cases) {
    const TraceResult r = TraceFiber(BodyAxisMap{}, chart, kIdentity.data(), c.p);
    EXPECT_EQ(r.status, FiberStatus::kBudgetExhausted);
    EXPECT_EQ(r.reason, c.reason);
    EXPECT_GE(r.PoseCount(), 1);
    ExpectShapes(r);
  }
}

// LI test_large_initial_step_is_rejected_then_recovers_by_shrinking: a rejected trial is not
// terminal; the step shrinks and the trace still closes.
TEST(FiberContinuation, RejectedTrialShrinksTheStepAndRecovers) {
  ContinuationParams p;
  p.initial_step = 0.6;
  p.maximum_step = 0.6;
  p.maximum_advance = 0.2;
  const TraceResult r = TraceFiber(BodyAxisMap{}, MakeTargetChart(kE3), kIdentity.data(), p);
  EXPECT_EQ(r.status, FiberStatus::kClosed);
  EXPECT_NEAR(Length(r), 2.0 * kPi, 1e-12);
}

// ---------------------------------------------------------------------------------------------
// C12 — an incompatible tangent cannot pass the final closure correction
// LI: test_incompatible_tangent_cannot_pass_final_closure_correction
// ---------------------------------------------------------------------------------------------

TEST(FiberContinuation, C12IncompatibleTangentIsNotClosure) {
  const TargetChart chart = MakeTargetChart(kE3);
  const ContinuationParams p;
  const auto initial = fiber_detail::EvaluateRegularState(BodyAxisMap{}, chart, p, kIdentity.data(), nullptr);
  const double reversed[3] = { -initial.tangent[0], -initial.tangent[1], -initial.tangent[2] };
  const auto o = fiber_detail::CorrectClosure(BodyAxisMap{}, chart, p, kIdentity.data(), kIdentity.data(),
                                              initial.tangent, reversed, 1000);
  EXPECT_FALSE(o.accepted);
  EXPECT_EQ(o.reason, FiberReason::kTopologyAmbiguity);
  EXPECT_LT(o.tangent_dot, p.closure_tangent_dot);
}

// ---------------------------------------------------------------------------------------------
// C06 subset — first-traversal closure (LI contract section 6.4, the step-aware trigger)
// LI: test_analytic_circle_closes_on_the_first_traversal_with_a_large_step,
//     test_analytic_circle_with_a_growing_step_closes_on_the_first_traversal,
//     test_analytic_conjugation_loops_shorter_than_pi_close_on_the_first_traversal,
//     test_closure_crossing_pose_bisects_the_section_zero_on_the_edge / _stays_on_an_off_fiber_edge
// ---------------------------------------------------------------------------------------------

TEST(FiberContinuation, C06CircleClosesOnTheFirstTraversalWithALargeStep) {
  ContinuationParams large;
  large.initial_step = 0.2;
  large.maximum_step = 0.2;
  ContinuationParams growing;
  growing.maximum_step = 0.2;
  for (const auto& p : { large, growing }) {
    const TraceResult r = TraceFiber(BodyAxisMap{}, MakeTargetChart(kE3), kIdentity.data(), p);
    EXPECT_EQ(r.status, FiberStatus::kClosed);
    EXPECT_LE(MaxResidual(r), 1e-11);
    EXPECT_NEAR(Length(r), 2.0 * kPi, 1e-12);
    ExpectOneTraversal(r);
  }
}

TEST(FiberContinuation, C06ConjugationLoopsShorterThanPiCloseOnceAtSecondOrder) {
  const double steps[] = { 0.01, 0.02, 0.04, 0.2 };
  for (double length : { 1.0, 2.0 }) {
    const double beta = ConjugationConeAngle(length);
    const Mat seed = Exp(kConjugationAngle * std::sin(beta), 0.0, kConjugationAngle * std::cos(beta));  // Rot(u0, pi/2)
    double target[3];
    ConjugationMap{}.Direction(seed.data(), target);
    std::vector<double> errors;
    for (double step : steps) {
      ContinuationParams p;
      p.initial_step = step;
      p.maximum_step = step;
      const TraceResult r = TraceFiber(ConjugationMap{}, MakeTargetChart(target), seed.data(), p);
      SCOPED_TRACE(testing::Message() << "L=" << length << " h=" << step);
      EXPECT_EQ(r.status, FiberStatus::kClosed);
      EXPECT_LE(MaxResidual(r), 1e-11);
      EXPECT_LE(so3::Distance(seed.data(), &r.poses[9 * (r.PoseCount() - 1)]), 1e-12);
      // The chord polygon under-estimates the metric length by O(h^2).
      const double traced = Length(r);
      EXPECT_GT(length - traced, 0.0);
      EXPECT_LE(length - traced, 1e-2 * length);
      errors.push_back(length - traced);
    }
    EXPECT_LE(errors[0], 2e-4 * length);
    for (int i = 1; i < 3; i++) {
      const double ratio = errors[i] / errors[i - 1];
      EXPECT_GE(ratio, 3.0) << i;
      EXPECT_LE(ratio, 5.0) << i;
    }
  }
}

TEST(FiberContinuation, ClosureCrossingPoseBisectsTheSectionZeroOnTheEdge) {
  // On the circle exp(theta e3): the section coordinate is sin(theta); the edge from -0.1 to 0.3
  // crosses at the seed itself, although its end lies 0.3 > closure_distance away.
  const Mat previous = Exp(0.0, 0.0, -0.1);
  const Mat current = Exp(0.0, 0.0, 0.3);
  const double pv = so3::SectionCoordinate(kIdentity.data(), previous.data(), kE3);
  double crossing[9];
  fiber_detail::ClosureCrossingPose(kIdentity.data(), kE3, previous.data(), pv, current.data(), crossing);
  EXPECT_LE(std::fabs(so3::SectionCoordinate(kIdentity.data(), crossing, kE3)), 1e-15);
  EXPECT_GE(so3::SectionCoordinate(kIdentity.data(), crossing, kE3), 0.0);  // on the end's side
  EXPECT_LE(so3::Distance(kIdentity.data(), crossing), 1e-15);

  // A generic edge off any fiber: the zero lies on its geodesic, between its ends.
  const Mat seed = Exp(0.3, -0.2, 0.1);
  const double t[3] = { 0.6, 0.0, 0.8 };
  Mat prev2{};
  Mat cur2{};
  const Mat a = Exp(-0.07, 0.04, -0.09);
  const Mat b = Exp(0.05, 0.03, 0.08);
  so3::MatMul(seed.data(), a.data(), prev2.data());
  so3::MatMul(seed.data(), b.data(), cur2.data());
  const double pv2 = so3::SectionCoordinate(seed.data(), prev2.data(), t);
  ASSERT_LT(pv2, 0.0);
  ASSERT_GT(so3::SectionCoordinate(seed.data(), cur2.data(), t), 0.0);
  fiber_detail::ClosureCrossingPose(seed.data(), t, prev2.data(), pv2, cur2.data(), crossing);
  EXPECT_LE(std::fabs(so3::SectionCoordinate(seed.data(), crossing, t)), 1e-15);
  EXPECT_NEAR(so3::Distance(prev2.data(), crossing) + so3::Distance(crossing, cur2.data()),
              so3::Distance(prev2.data(), cur2.data()), 1e-14);
}

// LI test_initial_tangent_sign_reverses_sample_order_without_changing_geometry.
TEST(FiberContinuation, InitialTangentSignReversesTheOrderOnly) {
  const TargetChart chart = MakeTargetChart(kE3);
  ContinuationParams reverse_p;
  reverse_p.initial_tangent_sign = -1;
  const TraceResult forward = TraceFiber(BodyAxisMap{}, chart, kIdentity.data(), ContinuationParams{});
  const TraceResult reverse = TraceFiber(BodyAxisMap{}, chart, kIdentity.data(), reverse_p);
  EXPECT_EQ(reverse.status, FiberStatus::kClosed);
  for (int i = 0; i < 3; i++) {
    EXPECT_NEAR(reverse.tangents[i], -forward.tangents[i], 1e-15);
  }
  EXPECT_NEAR(Length(reverse), Length(forward), 1e-12);
  // The second reverse sample sits at -theta where the forward one sits at +theta.
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      EXPECT_NEAR(reverse.poses[9 + i * 3 + j], forward.poses[9 + j * 3 + i], 1e-12);
    }
  }
}

// ---------------------------------------------------------------------------------------------
// Step control (LI _adapt_accepted_step / arclength_to_event unit tests)
// LI: test_arclength_to_event_is_the_rate_estimate_behind_the_step_limit,
//     test_event_step_limit_only_restrains_margins_that_are_shrinking
// ---------------------------------------------------------------------------------------------

DomainEvaluation Margins(std::initializer_list<double> values) {
  DomainEvaluation d;
  for (double v : values) {
    d.margins[d.margin_count++] = v;
  }
  return d;
}

TEST(FiberContinuation, ArclengthToEventIsTheLinearRateEstimate) {
  EXPECT_NEAR(fiber_detail::ArclengthToEvent(Margins({ 0.01, 0.5 }), Margins({ 0.03, 0.4 }), 0.1, 1e300),
              0.01 * 0.1 / 0.02, 1e-15);
  EXPECT_TRUE(std::isinf(fiber_detail::ArclengthToEvent(Margins({ 0.5 }), Margins({ 0.4 }), 0.1, 1e300)));
  EXPECT_EQ(fiber_detail::ArclengthToEvent(Margins({ 0.01 }), Margins({}), 0.1, 1e300), 0.0);
  EXPECT_EQ(fiber_detail::ArclengthToEvent(Margins({ 0.01 }), Margins({ 0.03 }), 0.0, 1e300), 0.0);
  // Above the threshold a margin imposes nothing, shrinking or not.
  EXPECT_TRUE(std::isinf(fiber_detail::ArclengthToEvent(Margins({ 0.05 }), Margins({ 0.2 }), 0.1, 0.02)));
  // A receding margin below the threshold imposes nothing either.
  EXPECT_TRUE(std::isinf(fiber_detail::ArclengthToEvent(Margins({ 0.01 }), Margins({ 0.005 }), 0.1, 0.02)));
}

TEST(FiberContinuation, StepAdaptationShrinksIntoAnApproachingEvent) {
  const ContinuationParams p;
  fiber_detail::CorrectorOutcome o;
  o.accepted = true;
  o.iterations = 1;
  o.residual_norm = 0.0;
  o.correction_norm = 0.0;
  o.tangent_dot = 1.0;
  o.advance = 0.04;
  o.state.condition = 1.0;
  // Clear of events: easy -> grows.
  o.state.domain = Margins({ 0.5 });
  EXPECT_NEAR(fiber_detail::AdaptAcceptedStep(0.04, o, p, Margins({ 0.6 })), 0.05, 1e-15);
  // Shrinking toward zero at 0.01 per 0.04 rad from 0.015: 0.06 rad left, so the limit is 0.03 < 0.04
  // — not clear of the event, hence "difficult": the step halves to 0.02, under the limit.
  o.state.domain = Margins({ 0.015 });
  EXPECT_NEAR(fiber_detail::AdaptAcceptedStep(0.04, o, p, Margins({ 0.025 })), 0.02, 1e-15);
  // Receding below the threshold: no bound, grows.
  EXPECT_NEAR(fiber_detail::AdaptAcceptedStep(0.04, o, p, Margins({ 0.010 })), 0.05, 1e-15);
}

TEST(FiberContinuation, ParamsValidationFollowsLi) {
  EXPECT_TRUE(ValidateParams(ContinuationParams{}));
  ContinuationParams p;
  p.initial_tangent_sign = 0;
  EXPECT_FALSE(ValidateParams(p));
  p = ContinuationParams{};
  p.minimum_step = 0.05;  // > initial
  EXPECT_FALSE(ValidateParams(p));
  p = ContinuationParams{};
  p.maximum_step = std::nan("");
  EXPECT_FALSE(ValidateParams(p));
  p = ContinuationParams{};
  p.growth_factor = 1.0;
  EXPECT_FALSE(ValidateParams(p));
  p = ContinuationParams{};
  p.closure_minimum_steps = 0;
  EXPECT_FALSE(ValidateParams(p));
}

}  // namespace
}  // namespace lumice::analytic
