#ifndef LUMICE_ANALYTIC_FIBER_CONTINUATION_HPP_
#define LUMICE_ANALYTIC_FIBER_CONTINUATION_HPP_

// Predictor-corrector continuation along one fiber X_(P,d) = { R : F_P(R) = d } of SO(3), from one
// seed. A port of LI's reference solver (src/lumice_integral/continuation.py at the revision named
// in doc/analytic-api.md section 4.3), function for function; each function below names its LI
// counterpart. The contract is LI docs/phase1-math-contract.md sections 5-10; the reference defaults
// are its section 10.1 and live in ContinuationParams.
//
// The core does not know optics. It is a template over a Map with
//   void Domain(const double r[9], DomainEvaluation* out) const;       // validity + typed event
//   template <class S> void Direction(const S r[9], S out[3]) const;   // S = double or Jet<3>
// — LI's domain_and_event_evaluator and direction_evaluator. The ice-crystal path is one such map
// (path_fiber.hpp); the unit tests drive the same core with analytic maps (LI's F(R) = R e3 and the
// constructed maps of its conformance suite). Derivatives are forward-mode: the direction map is
// evaluated once more with Jet<3> poses (jet.hpp), through the same exp, so the bordered Newton
// system is LI's jax.jacfwd of the same function, without a hand-derived second copy.
//
// Frames and units: poses are row-major rotations, body -> world; tangents and corrections are
// right-trivialised (R exp([delta]_x)); arclength is the SO(3) geodesic angle in radians.

#include <cmath>
#include <limits>
#include <vector>

#include "analytic/jet.hpp"
#include "analytic/so3.hpp"

namespace lumice::analytic {

// LI FiberStatus: closed.
enum class FiberStatus {
  kClosed,
  kEventTerminated,
  kNumericalFailure,
  kBudgetExhausted,
};

// LI TerminationReason, grouped by the status it maps to (StatusOf).
enum class FiberReason {
  kClosedLoop,
  kTirBoundary,
  kBranchBoundary,
  kPathInfeasible,
  kVisibilityBoundary,
  kChartBoundary,
  kRankLoss,
  kTopologyAmbiguity,
  kCorrectorFailure,
  kLinearSolveFailure,
  kNonFinite,
  kStepUnderflow,
  kInvalidNumericalInput,
  kStepBudget,
  kArclengthBudget,
  kEvaluationBudget,
};

// continuation._status_for_reason.
FiberStatus StatusOf(FiberReason reason);
// continuation._EVENT_REASONS: the reasons that end a trace as a boundary rather than a failure.
bool IsEventReason(FiberReason reason);

// LI ContinuationOptions, every field, with LI docs/phase1-math-contract.md section 10.1's reference
// defaults (the reference-continuation-v1 strategy; its convergence evidence is that section's
// text and the conformance tests it names). The C ABI exposes seven of these
// (doc/analytic-api.md section 4.3); the rest stay at these values there.
struct ContinuationParams {
  // Precision and root.
  double unit_tolerance = 1e-10;
  double residual_tolerance = 1e-11;
  double relative_residual_tolerance = 0.0;
  // Regularity.
  double singular_value_tolerance = 1e-8;
  double condition_limit = 1e8;
  // Step controller.
  double initial_step = 0.04;
  double minimum_step = 1e-5;
  double maximum_step = 0.12;
  double shrink_factor = 0.5;
  double growth_factor = 1.25;
  int maximum_retries = 8;
  // Corrector and trust gates.
  int corrector_maximum_iterations = 10;
  double corrector_phase_tolerance = 1e-12;
  double corrector_update_tolerance = 1e-12;
  double maximum_correction = 0.2;
  double maximum_advance = 0.2;
  double minimum_tangent_dot = 0.8;
  // Event approach.
  double event_slowdown_margin = 0.02;
  // Work bounds. One evaluation is one gated pose or corrector iterate, value and derivative work
  // together (LI's evaluation unit).
  int maximum_accepted_steps = 4000;
  int maximum_evaluations = 100000;
  double maximum_arclength = 20.0;
  // Closure.
  int closure_minimum_steps = 3;
  double closure_minimum_arclength = 0.0;
  double closure_distance = 0.08;
  double closure_tangent_dot = 0.8;
  double closure_section_tolerance = 1e-11;
  int closure_maximum_iterations = 10;
  // Seed orientation (LI contract section 5.4): -1 reverses the seed tangent and so the sample
  // order. +1 is this implementation's deterministic convention (EvaluateRegularState: A's first row
  // x second row, times the chart basis's handedness, so it does not depend on the basis), not LI's,
  // whose +1 is whatever LAPACK's SVD returns and may differ between LAPACK builds.
  int initial_tangent_sign = 1;
};

// LI code constants, not options (contract section 10.1 names them).
constexpr double kEventApproachStepFraction = 0.5;       // _EVENT_APPROACH_STEP_FRACTION
constexpr double kClosureArclengthStepMultiplier = 2.0;  // _CLOSURE_ARCLENGTH_STEP_MULTIPLIER
constexpr int kClosureCrossingBisectionSteps = 52;       // _CLOSURE_CROSSING_BISECTION_STEPS

// ContinuationOptions.__post_init__: the relations between fields LI enforces. NaN fails.
bool ValidateParams(const ContinuationParams& p);

// The target-local chart (LI TargetChart): the target direction d, an orthonormal basis of the
// tangent plane at d (the residual is r = basis^T (F - d)), and the neighbourhood F . d >
// minimum_dot, which excludes the antipode, where the projected residual also vanishes.
struct TargetChart {
  double direction[3] = { 0.0, 0.0, 1.0 };
  double basis[2][3] = { { 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 } };
  double minimum_dot = 0.0;
};

// The chart LI's path_problem builds: analytic.tangent_basis of the target, minimum_dot 0.
TargetChart MakeTargetChart(const double target_direction[3]);

// Upper bound on the margins one Domain() call reports. The ice-crystal map reports face_count + 2
// (path_fiber.hpp holds it to this bound).
constexpr int kMaxDomainMargins = 66;

// LI DomainEvaluation. `margins` are event margins: positive on the smooth branch, zero where it
// ends; index i names the same margin at every pose of one map. An invalid domain carries an event
// (`event`, `event_margin`); a map that sets valid = false without one gets path_infeasible
// (continuation._evaluate_domain).
struct DomainEvaluation {
  bool valid = true;
  int margin_count = 0;
  double margins[kMaxDomainMargins]{};
  bool has_event = false;
  FiberReason event = FiberReason::kPathInfeasible;
  double event_margin = 0.0;
};

// One trace's samples: N accepted poses (seed first; on closure the last is the corrected closing
// pose), N residual norms |basis^T (F - d)|, N unit tangents, N - 1 arclength increments. N = 0 when
// the seed itself is rejected.
struct TraceResult {
  FiberStatus status = FiberStatus::kNumericalFailure;
  FiberReason reason = FiberReason::kInvalidNumericalInput;
  std::vector<double> poses;
  std::vector<double> residual_norms;
  std::vector<double> tangents;
  std::vector<double> arclength_increments;
  int PoseCount() const { return static_cast<int>(residual_norms.size()); }
};

namespace fiber_detail {

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// continuation._StateEvaluation, the fields continuation reads.
struct State {
  bool accepted = false;
  double rotation[9]{};
  double residual_norm = kInf;
  bool has_tangent = false;
  double tangent[3]{};
  double condition = kInf;  // sigma1 / sigma2 of the residual Jacobian
  DomainEvaluation domain;
  FiberReason reason = FiberReason::kNonFinite;
};

// continuation._SmoothOutputEvaluation.
struct SmoothOutput {
  bool accepted = false;
  double residual_norm = kInf;
  FiberReason reason = FiberReason::kNonFinite;
};

// continuation._CorrectorOutcome, the fields continuation reads.
struct CorrectorOutcome {
  bool accepted = false;
  bool has_state = false;
  State state;
  int iterations = 0;
  double residual_norm = kInf;
  double correction_norm = 0.0;
  double advance = 0.0;
  double tangent_dot = kNaN;
  FiberReason reason = FiberReason::kCorrectorFailure;
  int evaluations = 0;
};

// continuation._gate_smooth_output: finiteness, unit norm, chart neighbourhood, residual
// finiteness, in that order — the one definition shared by the regular state and both correctors.
SmoothOutput GateSmoothOutput(const ContinuationParams& p, const TargetChart& chart, const double direction[3],
                              const double residual[2]);

// continuation._evaluate_domain's normalisation of a map's report.
void NormalizeDomain(DomainEvaluation* d);

// continuation.arclength_to_event: the linear-rate arclength from an accepted state to the nearest
// event among margins at or below `threshold` that decreased over the last edge; inf if none is
// decreasing, 0 when one has no measurable rate.
double ArclengthToEvent(const DomainEvaluation& margins, const DomainEvaluation& previous, double advance,
                        double threshold);

// continuation._adapt_accepted_step.
double AdaptAcceptedStep(double step, const CorrectorOutcome& outcome, const ContinuationParams& p,
                         const DomainEvaluation& previous);

// continuation._crossing_direction.
int CrossingDirection(double previous, double current);

// continuation._closure_crossing_pose: bisects the section's zero on the accepted edge's geodesic
// previous exp(t log(previous^T current)), t in [0, 1], and returns the bracket end on the end's side.
void ClosureCrossingPose(const double seed[9], const double tangent[3], const double previous[9], double previous_value,
                         const double current[9], double out[9]);

// residual = basis^T (direction - target).
template <class S>
void ChartResidual(const TargetChart& chart, const S direction[3], S residual[2]) {
  for (int i = 0; i < 2; i++) {
    residual[i] = chart.basis[i][0] * (direction[0] - chart.direction[0]) +
                  chart.basis[i][1] * (direction[1] - chart.direction[1]) +
                  chart.basis[i][2] * (direction[2] - chart.direction[2]);
  }
}

// base exp([delta]_x) with delta a Jet<3> seeded at `delta0` (unit gradient per component).
inline void JetPose(const double base[9], const double delta0[3], Jet<3> out[9]) {
  Jet<3> delta[3];
  for (int k = 0; k < 3; k++) {
    delta[k] = Jet<3>::Variable(delta0[k], k);
  }
  Jet<3> e[9];
  so3::Exp(delta, e);
  so3::MatMul(base, e, out);
}

inline void ApplyCorrection(const double base[9], const double delta[3], double out[9]) {
  double e[9];
  so3::Exp(delta, e);
  so3::MatMul(base, e, out);
}

// continuation._local_residual_jacobian_kernel: A = d r(R exp(delta)) / d delta at delta = 0.
template <class Map>
void LocalResidualJacobian(const Map& map, const TargetChart& chart, const double r[9], double a[2][3]) {
  const double zero[3] = { 0.0, 0.0, 0.0 };
  Jet<3> pose[9];
  JetPose(r, zero, pose);
  Jet<3> direction[3];
  map.Direction(pose, direction);
  Jet<3> residual[2];
  ChartResidual(chart, direction, residual);
  for (int i = 0; i < 2; i++) {
    for (int k = 0; k < 3; k++) {
      a[i][k] = residual[i].v[k];
    }
  }
}

// continuation._evaluate_regular_state: one budgeted pose unit — domain, smooth output and its
// gates, the residual Jacobian, the rank / condition gate and the oriented unit tangent.
template <class Map>
State EvaluateRegularState(const Map& map, const TargetChart& chart, const ContinuationParams& p,
                           const double rotation[9], const double* previous_tangent) {
  State s;
  for (int i = 0; i < 9; i++) {
    s.rotation[i] = rotation[i];
  }
  map.Domain(rotation, &s.domain);
  NormalizeDomain(&s.domain);
  if (!s.domain.valid) {
    s.reason = s.domain.event;
    return s;
  }
  double direction[3];
  map.Direction(rotation, direction);
  double residual[2];
  ChartResidual(chart, direction, residual);
  const SmoothOutput smooth = GateSmoothOutput(p, chart, direction, residual);
  s.residual_norm = smooth.residual_norm;
  if (!smooth.accepted) {
    s.reason = smooth.reason;
    return s;
  }
  double a[2][3];
  LocalResidualJacobian(map, chart, rotation, a);
  for (const auto& row : a) {
    for (double v : row) {
      if (!std::isfinite(v)) {
        s.reason = FiberReason::kNonFinite;
        return s;
      }
    }
  }
  const so3::TwoByThreeSvd svd = so3::SvdTwoByThree(a[0], a[1]);
  const int rank =
      (svd.sigma1 >= p.singular_value_tolerance ? 1 : 0) + (svd.sigma2 >= p.singular_value_tolerance ? 1 : 0);
  s.condition = svd.sigma2 > 0.0 ? svd.sigma1 / svd.sigma2 : kInf;
  if (rank < 2 || s.condition > p.condition_limit) {
    s.reason = FiberReason::kRankLoss;
    return s;
  }
  // The seed orientation convention (initial_tangent_sign = +1): the kernel direction r0 x r1 of
  // A times the handedness of the chart basis, (b0 x b1) . d. A change of basis by Q in O(2) scales
  // both by det(Q), so the product — and the traversal order — does not depend on the chart's basis
  // (LI contract section 11, C03: the order may reverse only with the seed orientation).
  double handedness[3];
  so3::Cross3(chart.basis[0], chart.basis[1], handedness);
  const double sign = so3::Dot3(handedness, chart.direction) < 0.0 ? -1.0 : 1.0;
  double t[3] = { sign * svd.null_vector[0], sign * svd.null_vector[1], sign * svd.null_vector[2] };
  if (previous_tangent != nullptr && so3::Dot3(t, previous_tangent) < 0.0) {
    for (double& x : t) {
      x = -x;
    }
  }
  const double norm = so3::Norm3(t);
  for (int i = 0; i < 3; i++) {
    s.tangent[i] = t[i] / norm;
  }
  s.has_tangent = true;
  s.accepted = true;
  return s;
}

// continuation._NewtonStep.
struct NewtonStep {
  double direction[3];
  double residual[2];
  double value[3];
  double jacobian[9];
  double condition;
  double update[3];
  double update_norm;
  double next_delta[3];
};

// continuation._bordered_newton_step: the system [r(base exp(delta)); border] and its Jacobian in
// delta, in one forward-mode pass at the current delta; condition number and Newton update. No gate
// is applied here. `border(candidate, delta)` returns the third equation as a Jet<3>.
template <class Map, class Border>
NewtonStep BorderedNewtonStep(const Map& map, const TargetChart& chart, const double base[9], const double delta[3],
                              const Border& border) {
  NewtonStep n{};
  Jet<3> delta_jet[3];
  for (int k = 0; k < 3; k++) {
    delta_jet[k] = Jet<3>::Variable(delta[k], k);
  }
  Jet<3> e[9];
  so3::Exp(delta_jet, e);
  Jet<3> candidate[9];
  so3::MatMul(base, e, candidate);
  Jet<3> direction[3];
  map.Direction(candidate, direction);
  Jet<3> residual[2];
  ChartResidual(chart, direction, residual);
  const Jet<3> b = border(candidate, delta_jet);
  const Jet<3>* rows[3] = { &residual[0], &residual[1], &b };
  for (int i = 0; i < 3; i++) {
    n.value[i] = rows[i]->a;
    for (int k = 0; k < 3; k++) {
      n.jacobian[i * 3 + k] = rows[i]->v[k];
    }
  }
  for (int i = 0; i < 3; i++) {
    n.direction[i] = direction[i].a;
  }
  n.residual[0] = residual[0].a;
  n.residual[1] = residual[1].a;
  n.condition = so3::ConditionNumber(n.jacobian);
  const double rhs[3] = { -n.value[0], -n.value[1], -n.value[2] };
  if (!so3::Solve3(n.jacobian, rhs, n.update)) {
    // numpy.linalg.solve inside the compiled kernel yields a non-finite update on a singular
    // system; the host classifies it (ClassifyNewtonSolve).
    n.update[0] = n.update[1] = n.update[2] = kNaN;
  }
  n.update_norm = so3::Norm3(n.update);
  for (int k = 0; k < 3; k++) {
    n.next_delta[k] = delta[k] + n.update[k];
  }
  return n;
}

// continuation._classify_newton_solve: non-finite Jacobian, non-finite condition, condition limit,
// non-finite update, in that order. true = a defect (linear_solve_failure).
inline bool NewtonSolveDefect(const ContinuationParams& p, const NewtonStep& n) {
  for (double v : n.jacobian) {
    if (!std::isfinite(v)) {
      return true;
    }
  }
  if (!std::isfinite(n.condition) || n.condition > p.condition_limit) {
    return true;
  }
  for (double v : n.update) {
    if (!std::isfinite(v)) {
      return true;
    }
  }
  return false;
}

inline double Norm3(const double v[3]) {
  return so3::Norm3(v);
}

// continuation._correct_trial: bounded bordered Newton around one predictor, phase condition
// tangent . delta = 0. Each iteration reserves one evaluation unit before its gated value and AD
// work; `maximum_evaluations` is what is left of the trace's budget.
template <class Map>
CorrectorOutcome CorrectTrial(const Map& map, const TargetChart& chart, const ContinuationParams& p,
                              const double current[9], const double predicted[9], const double phase_tangent[3],
                              int maximum_evaluations) {
  CorrectorOutcome o;
  double delta[3] = { 0.0, 0.0, 0.0 };
  double last_residual = kInf;
  double last_update = kInf;
  int evaluations = 0;
  double candidate[9];

  auto finish = [&](int iteration, FiberReason reason) {
    o.iterations = iteration;
    o.correction_norm = Norm3(delta);
    o.advance = so3::Distance(current, candidate);
    o.reason = reason;
    o.evaluations = evaluations;
    return o;
  };
  auto budget_exhausted = [&](int iteration) {
    o.residual_norm = last_residual;
    return finish(iteration, FiberReason::kEvaluationBudget);
  };
  const auto border = [&phase_tangent](const Jet<3>* /*candidate*/, const Jet<3>* correction) {
    return phase_tangent[0] * correction[0] + phase_tangent[1] * correction[1] + phase_tangent[2] * correction[2];
  };

  for (int iteration = 0; iteration <= p.corrector_maximum_iterations; iteration++) {
    ApplyCorrection(predicted, delta, candidate);
    if (evaluations >= maximum_evaluations) {
      return budget_exhausted(iteration);
    }
    DomainEvaluation domain;
    map.Domain(candidate, &domain);
    NormalizeDomain(&domain);
    evaluations++;
    if (!domain.valid) {
      o.residual_norm = last_residual;
      const double correction_norm = Norm3(delta);
      const double advance = so3::Distance(current, candidate);
      // A Newton iterate outside the trust region acceptance itself requires is the corrector
      // running away, not the fiber leaving the domain (LI contract section 8, last paragraph).
      if (correction_norm > p.maximum_correction || advance > p.maximum_advance) {
        return finish(iteration, FiberReason::kCorrectorFailure);
      }
      return finish(iteration, domain.event);
    }
    const NewtonStep newton = BorderedNewtonStep(map, chart, predicted, delta, border);
    const SmoothOutput smooth = GateSmoothOutput(p, chart, newton.direction, newton.residual);
    if (!smooth.accepted) {
      o.residual_norm = smooth.residual_norm;
      return finish(iteration, smooth.reason);
    }
    last_residual = std::sqrt(newton.value[0] * newton.value[0] + newton.value[1] * newton.value[1]);
    const double phase_norm = std::fabs(newton.value[2]);
    if (!std::isfinite(newton.value[0]) || !std::isfinite(newton.value[1]) || !std::isfinite(newton.value[2])) {
      o.residual_norm = last_residual;
      return finish(iteration, FiberReason::kNonFinite);
    }
    if (last_residual <= p.residual_tolerance && phase_norm <= p.corrector_phase_tolerance &&
        (std::isfinite(last_update) ? last_update : 0.0) <= p.corrector_update_tolerance) {
      if (evaluations >= maximum_evaluations) {
        return budget_exhausted(iteration);
      }
      o.state = EvaluateRegularState(map, chart, p, candidate, phase_tangent);
      o.has_state = true;
      evaluations++;
      o.residual_norm = o.state.residual_norm;
      o.tangent_dot = o.state.has_tangent ? so3::Dot3(phase_tangent, o.state.tangent) : kNaN;
      if (!o.state.accepted) {
        return finish(iteration, o.state.reason);
      }
      const double correction_norm = Norm3(delta);
      const double advance = so3::Distance(current, candidate);
      if (correction_norm > p.maximum_correction || advance > p.maximum_advance ||
          o.tangent_dot < p.minimum_tangent_dot) {
        return finish(iteration, FiberReason::kCorrectorFailure);
      }
      o.accepted = true;
      return finish(iteration, FiberReason::kCorrectorFailure);  // reason unused when accepted
    }
    if (iteration == p.corrector_maximum_iterations) {
      break;
    }
    if (evaluations >= maximum_evaluations) {
      return budget_exhausted(iteration);
    }
    if (NewtonSolveDefect(p, newton)) {
      o.residual_norm = last_residual;
      return finish(iteration, FiberReason::kLinearSolveFailure);
    }
    last_update = newton.update_norm;
    for (int k = 0; k < 3; k++) {
      delta[k] = newton.next_delta[k];
    }
  }
  ApplyCorrection(predicted, delta, candidate);
  o.residual_norm = last_residual;
  return finish(p.corrector_maximum_iterations, FiberReason::kCorrectorFailure);
}

// continuation._correct_closure: bordered Newton from the crossing pose with the seed section
// coordinate as border; accepted only when the regular state passes and the correction, advance,
// tangent agreement with the seed tangent and seed distance all pass (else topology_ambiguity).
template <class Map>
CorrectorOutcome CorrectClosure(const Map& map, const TargetChart& chart, const ContinuationParams& p,
                                const double current[9], const double seed[9], const double initial_tangent[3],
                                const double current_tangent[3], int maximum_evaluations) {
  CorrectorOutcome o;
  double delta[3] = { 0.0, 0.0, 0.0 };
  double last_residual = kInf;
  double last_update = kInf;
  int evaluations = 0;
  double candidate[9];

  auto finish = [&](int iteration, FiberReason reason) {
    o.iterations = iteration;
    o.correction_norm = Norm3(delta);
    o.advance = so3::Distance(current, candidate);
    o.reason = reason;
    o.evaluations = evaluations;
    return o;
  };
  auto budget_exhausted = [&](int iteration) {
    o.residual_norm = last_residual;
    return finish(iteration, FiberReason::kEvaluationBudget);
  };
  const auto border = [seed, initial_tangent](const Jet<3>* candidate_pose, const Jet<3>* /*correction*/) {
    return so3::SectionCoordinate(seed, candidate_pose, initial_tangent);
  };

  for (int iteration = 0; iteration <= p.closure_maximum_iterations; iteration++) {
    ApplyCorrection(current, delta, candidate);
    if (evaluations >= maximum_evaluations) {
      return budget_exhausted(iteration);
    }
    DomainEvaluation domain;
    map.Domain(candidate, &domain);
    NormalizeDomain(&domain);
    evaluations++;
    if (!domain.valid) {
      o.residual_norm = last_residual;
      return finish(iteration, domain.event);
    }
    const NewtonStep newton = BorderedNewtonStep(map, chart, current, delta, border);
    const SmoothOutput smooth = GateSmoothOutput(p, chart, newton.direction, newton.residual);
    if (!smooth.accepted) {
      o.residual_norm = smooth.residual_norm;
      return finish(iteration, smooth.reason);
    }
    last_residual = std::sqrt(newton.value[0] * newton.value[0] + newton.value[1] * newton.value[1]);
    const double section_norm = std::fabs(newton.value[2]);
    if (!std::isfinite(newton.value[0]) || !std::isfinite(newton.value[1]) || !std::isfinite(newton.value[2])) {
      o.residual_norm = last_residual;
      return finish(iteration, FiberReason::kNonFinite);
    }
    if (last_residual <= p.residual_tolerance && section_norm <= p.closure_section_tolerance &&
        (std::isfinite(last_update) ? last_update : 0.0) <= p.corrector_update_tolerance) {
      if (evaluations >= maximum_evaluations) {
        return budget_exhausted(iteration);
      }
      o.state = EvaluateRegularState(map, chart, p, candidate, current_tangent);
      o.has_state = true;
      evaluations++;
      o.residual_norm = o.state.residual_norm;
      o.tangent_dot = o.state.has_tangent ? so3::Dot3(initial_tangent, o.state.tangent) : kNaN;
      if (!o.state.accepted) {
        return finish(iteration, o.state.reason);
      }
      const double correction_norm = Norm3(delta);
      const double advance = so3::Distance(current, candidate);
      o.accepted = correction_norm <= p.maximum_correction && advance <= p.maximum_advance &&
                   o.tangent_dot >= p.closure_tangent_dot && so3::Distance(seed, candidate) <= p.closure_distance;
      return finish(iteration, o.accepted ? FiberReason::kClosedLoop : FiberReason::kTopologyAmbiguity);
    }
    if (iteration == p.closure_maximum_iterations) {
      break;
    }
    if (evaluations >= maximum_evaluations) {
      return budget_exhausted(iteration);
    }
    if (NewtonSolveDefect(p, newton)) {
      o.residual_norm = last_residual;
      return finish(iteration, FiberReason::kLinearSolveFailure);
    }
    last_update = newton.update_norm;
    for (int k = 0; k < 3; k++) {
      delta[k] = newton.next_delta[k];
    }
  }
  ApplyCorrection(current, delta, candidate);
  o.residual_norm = last_residual;
  return finish(p.closure_maximum_iterations, FiberReason::kCorrectorFailure);
}

inline void AppendState(const State& s, TraceResult* r) {
  r->poses.insert(r->poses.end(), s.rotation, s.rotation + 9);
  r->residual_norms.push_back(s.residual_norm);
  r->tangents.insert(r->tangents.end(), s.tangent, s.tangent + 3);
}

inline void ReplaceLastState(const State& s, TraceResult* r) {
  const size_t n = r->residual_norms.size() - 1;
  for (int i = 0; i < 9; i++) {
    r->poses[9 * n + i] = s.rotation[i];
  }
  r->residual_norms[n] = s.residual_norm;
  for (int i = 0; i < 3; i++) {
    r->tangents[3 * n + i] = s.tangent[i];
  }
}

inline void Finish(FiberReason reason, TraceResult* r) {
  r->reason = reason;
  r->status = StatusOf(reason);
}

}  // namespace fiber_detail

// continuation.trace_fiber: the regular component reachable from `seed`, never claiming it is the
// only one. `seed` is assumed to be a rotation (the caller validates it, as LI's FiberProblem does)
// and `p` to pass ValidateParams.
template <class Map>
TraceResult TraceFiber(const Map& map, const TargetChart& chart, const double seed[9], const ContinuationParams& p) {
  using fiber_detail::AppendState;
  using fiber_detail::CorrectorOutcome;
  using fiber_detail::Finish;
  using fiber_detail::State;
  TraceResult result;

  State initial = fiber_detail::EvaluateRegularState(map, chart, p, seed, nullptr);
  int evaluations = 1;
  if (!initial.accepted) {
    Finish(initial.reason, &result);
    return result;
  }
  if (p.initial_tangent_sign == -1) {
    for (double& x : initial.tangent) {
      x = -x;
    }
  }
  if (initial.residual_norm > p.residual_tolerance + p.relative_residual_tolerance) {
    Finish(FiberReason::kInvalidNumericalInput, &result);
    return result;
  }

  AppendState(initial, &result);
  // Margins of the last two accepted states, for the event-approach step limit.
  DomainEvaluation previous_margins = initial.domain;
  DomainEvaluation current_margins = initial.domain;
  double step = p.initial_step;
  const double closure_minimum_arclength =
      std::fmax(p.closure_minimum_arclength, kClosureArclengthStepMultiplier * p.initial_step);
  double total_arclength = 0.0;
  double previous_section = 0.0;
  State current_state = initial;
  State before_current;  // the accepted state before current_state (closing edge start)

  while (true) {
    const int accepted_steps = result.PoseCount() - 1;
    if (accepted_steps >= p.maximum_accepted_steps) {
      Finish(FiberReason::kStepBudget, &result);
      return result;
    }
    if (evaluations >= p.maximum_evaluations) {
      Finish(FiberReason::kEvaluationBudget, &result);
      return result;
    }

    int trial_index = 0;
    while (true) {
      double predicted[9];
      {
        const double w[3] = { step * current_state.tangent[0], step * current_state.tangent[1],
                              step * current_state.tangent[2] };
        fiber_detail::ApplyCorrection(current_state.rotation, w, predicted);
      }
      const CorrectorOutcome outcome = fiber_detail::CorrectTrial(
          map, chart, p, current_state.rotation, predicted, current_state.tangent, p.maximum_evaluations - evaluations);
      evaluations += outcome.evaluations;

      if (!outcome.accepted && outcome.reason == FiberReason::kEvaluationBudget) {
        Finish(FiberReason::kEvaluationBudget, &result);
        return result;
      }
      if (!outcome.accepted) {
        if (IsEventReason(outcome.reason)) {
          Finish(outcome.reason, &result);
          return result;
        }
        if (trial_index >= p.maximum_retries) {
          Finish(outcome.reason, &result);
          return result;
        }
        const double reduced_step = step * p.shrink_factor;
        if (reduced_step < p.minimum_step) {
          Finish(outcome.reason == FiberReason::kNonFinite ? FiberReason::kNonFinite : FiberReason::kStepUnderflow,
                 &result);
          return result;
        }
        step = reduced_step;
        trial_index++;
        continue;
      }

      if (evaluations > p.maximum_evaluations) {
        Finish(FiberReason::kEvaluationBudget, &result);
        return result;
      }
      if (total_arclength + outcome.advance > p.maximum_arclength) {
        Finish(FiberReason::kArclengthBudget, &result);
        return result;
      }

      AppendState(outcome.state, &result);
      result.arclength_increments.push_back(outcome.advance);
      previous_margins = current_margins;
      current_margins = outcome.state.domain;
      total_arclength += outcome.advance;
      before_current = current_state;
      current_state = outcome.state;

      const double section = so3::SectionCoordinate(seed, outcome.state.rotation, initial.tangent);
      const int crossing_direction = fiber_detail::CrossingDirection(previous_section, section);
      const bool crossed = previous_section != 0.0 && crossing_direction != 0;
      // A crossing edge is measured at its bisected section zero, not at its end (LI contract
      // section 6.4 item 2).
      double crossing_pose[9];
      if (crossed) {
        fiber_detail::ClosureCrossingPose(seed, initial.tangent, before_current.rotation, previous_section,
                                          outcome.state.rotation, crossing_pose);
      } else {
        for (int i = 0; i < 9; i++) {
          crossing_pose[i] = outcome.state.rotation[i];
        }
      }
      const double seed_distance = so3::Distance(seed, crossing_pose);
      const double tangent_dot = so3::Dot3(initial.tangent, outcome.state.tangent);
      const bool extent_gate =
          result.PoseCount() - 1 >= p.closure_minimum_steps && total_arclength >= closure_minimum_arclength;
      if (extent_gate && crossed && seed_distance <= p.closure_distance && tangent_dot >= p.closure_tangent_dot) {
        const CorrectorOutcome closure =
            fiber_detail::CorrectClosure(map, chart, p, crossing_pose, seed, initial.tangent, outcome.state.tangent,
                                         p.maximum_evaluations - evaluations);
        evaluations += closure.evaluations;
        if (!closure.accepted && closure.reason == FiberReason::kEvaluationBudget) {
          Finish(FiberReason::kEvaluationBudget, &result);
          return result;
        }
        if (closure.accepted) {
          const double closing_advance = so3::Distance(before_current.rotation, closure.state.rotation);
          const double closing_arclength = total_arclength + closing_advance - outcome.advance;
          if (evaluations > p.maximum_evaluations) {
            Finish(FiberReason::kEvaluationBudget, &result);
            return result;
          }
          if (closing_arclength > p.maximum_arclength) {
            Finish(FiberReason::kArclengthBudget, &result);
            return result;
          }
          fiber_detail::ReplaceLastState(closure.state, &result);
          result.arclength_increments.back() = closing_advance;
          Finish(FiberReason::kClosedLoop, &result);
          return result;
        }
        if (IsEventReason(closure.reason)) {
          Finish(closure.reason, &result);
          return result;
        }
      }

      previous_section = section;
      step = fiber_detail::AdaptAcceptedStep(step, outcome, p, previous_margins);
      break;
    }
  }
}

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_FIBER_CONTINUATION_HPP_
