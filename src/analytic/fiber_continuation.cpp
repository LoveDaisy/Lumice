#include "analytic/fiber_continuation.hpp"

#include <algorithm>
#include <cmath>

namespace lumice::analytic {

FiberStatus StatusOf(FiberReason reason) {
  if (reason == FiberReason::kClosedLoop) {
    return FiberStatus::kClosed;
  }
  if (IsEventReason(reason)) {
    return FiberStatus::kEventTerminated;
  }
  if (reason == FiberReason::kStepBudget || reason == FiberReason::kArclengthBudget ||
      reason == FiberReason::kEvaluationBudget) {
    return FiberStatus::kBudgetExhausted;
  }
  return FiberStatus::kNumericalFailure;
}

bool IsEventReason(FiberReason reason) {
  switch (reason) {
    case FiberReason::kTirBoundary:
    case FiberReason::kBranchBoundary:
    case FiberReason::kPathInfeasible:
    case FiberReason::kVisibilityBoundary:
    case FiberReason::kChartBoundary:
    case FiberReason::kRankLoss:
    case FiberReason::kTopologyAmbiguity:
      return true;
    default:
      return false;
  }
}

bool ValidateParams(const ContinuationParams& p) {
  // Each test is written so that a NaN fails it, as LI's `not (...)` comparisons do.
  if (p.initial_tangent_sign != 1 && p.initial_tangent_sign != -1) {
    return false;
  }
  if (!(0.0 < p.minimum_step && p.minimum_step <= p.initial_step && p.initial_step <= p.maximum_step)) {
    return false;
  }
  if (!(0.0 < p.shrink_factor && p.shrink_factor < 1.0 && 1.0 < p.growth_factor)) {
    return false;
  }
  if (p.maximum_retries < 0 || p.corrector_maximum_iterations < 1) {
    return false;
  }
  if (p.maximum_accepted_steps < 1 || p.maximum_evaluations < 1) {
    return false;
  }
  if (!(p.maximum_arclength > 0.0)) {
    return false;
  }
  if (p.closure_minimum_steps < 1 || !(p.closure_minimum_arclength >= 0.0)) {
    return false;
  }
  if (!(p.corrector_phase_tolerance > 0.0) || !(p.corrector_update_tolerance > 0.0)) {
    return false;
  }
  if (!(-1.0 <= p.minimum_tangent_dot && p.minimum_tangent_dot <= 1.0)) {
    return false;
  }
  if (!(-1.0 <= p.closure_tangent_dot && p.closure_tangent_dot <= 1.0)) {
    return false;
  }
  return true;
}

TargetChart MakeTargetChart(const double target_direction[3]) {
  TargetChart chart;
  for (int i = 0; i < 3; i++) {
    chart.direction[i] = target_direction[i];
  }
  so3::TangentBasis(target_direction, chart.basis);
  chart.minimum_dot = 0.0;
  return chart;
}

namespace fiber_detail {

SmoothOutput GateSmoothOutput(const ContinuationParams& p, const TargetChart& chart, const double direction[3],
                              const double residual[2]) {
  SmoothOutput out;
  for (int i = 0; i < 3; i++) {
    if (!std::isfinite(direction[i])) {
      out.reason = FiberReason::kNonFinite;
      return out;
    }
  }
  const double norm = so3::Norm3(direction);
  if (!(std::fabs(norm - 1.0) <= p.unit_tolerance)) {
    out.reason = FiberReason::kInvalidNumericalInput;
    return out;
  }
  const double chart_dot = so3::Dot3(direction, chart.direction);
  if (chart_dot <= chart.minimum_dot) {
    out.reason = FiberReason::kChartBoundary;
    return out;
  }
  out.residual_norm = std::sqrt(residual[0] * residual[0] + residual[1] * residual[1]);
  if (!std::isfinite(out.residual_norm)) {
    out.reason = FiberReason::kNonFinite;
    return out;
  }
  out.accepted = true;
  return out;
}

void NormalizeDomain(DomainEvaluation* d) {
  if (!d->valid && !d->has_event) {
    d->has_event = true;
    d->event = FiberReason::kPathInfeasible;
    d->event_margin = d->margin_count > 0 ? *std::min_element(d->margins, d->margins + d->margin_count) : kNaN;
  }
}

double ArclengthToEvent(const DomainEvaluation& margins, const DomainEvaluation& previous, double advance,
                        double threshold) {
  double distance = kInf;
  for (int i = 0; i < margins.margin_count; i++) {
    const double margin = margins.margins[i];
    if (margin > threshold) {
      continue;
    }
    if (i >= previous.margin_count || advance <= 0.0) {
      return 0.0;
    }
    const double drop = previous.margins[i] - margin;
    if (drop <= 0.0) {
      continue;
    }
    distance = std::fmin(distance, margin * advance / drop);
  }
  return distance;
}

double AdaptAcceptedStep(double step, const CorrectorOutcome& outcome, const ContinuationParams& p,
                         const DomainEvaluation& previous) {
  const State& state = outcome.state;
  const double event_limit =
      kEventApproachStepFraction * ArclengthToEvent(state.domain, previous, outcome.advance, p.event_slowdown_margin);
  const bool clear_of_event = event_limit >= step;
  const double residual_ratio = outcome.residual_norm / p.residual_tolerance;
  const bool easy = outcome.iterations <= 2 && residual_ratio <= 0.1 && outcome.correction_norm <= 0.1 * step &&
                    outcome.tangent_dot >= 0.98 && state.condition <= 0.1 * p.condition_limit && clear_of_event;
  const bool difficult = outcome.iterations >= std::max(3, p.corrector_maximum_iterations / 2) ||
                         residual_ratio >= 0.5 || outcome.correction_norm >= 0.5 * step || outcome.tangent_dot < 0.95 ||
                         !clear_of_event;
  if (easy) {
    step *= p.growth_factor;
  } else if (difficult) {
    step *= p.shrink_factor;
  }
  step = std::fmin(step, event_limit);
  return std::fmin(p.maximum_step, std::fmax(p.minimum_step, step));
}

int CrossingDirection(double previous, double current) {
  if (previous < 0.0 && 0.0 <= current) {
    return 1;
  }
  if (previous > 0.0 && 0.0 >= current) {
    return -1;
  }
  return 0;
}

void ClosureCrossingPose(const double seed[9], const double tangent[3], const double previous[9], double previous_value,
                         const double current[9], double out[9]) {
  double relative[9];
  so3::MatTMul(previous, current, relative);
  double generator[3];
  so3::Log(relative, generator);
  const bool low_positive = previous_value > 0.0;
  double low = 0.0;
  double high = 1.0;
  for (int i = 0; i < kClosureCrossingBisectionSteps; i++) {
    const double middle = 0.5 * (low + high);
    const double w[3] = { middle * generator[0], middle * generator[1], middle * generator[2] };
    double candidate[9];
    ApplyCorrection(previous, w, candidate);
    const double value = so3::SectionCoordinate(seed, candidate, tangent);
    if ((value > 0.0) == low_positive && value != 0.0) {
      low = middle;
    } else {
      high = middle;
    }
  }
  const double w[3] = { high * generator[0], high * generator[1], high * generator[2] };
  ApplyCorrection(previous, w, out);
}

}  // namespace fiber_detail

}  // namespace lumice::analytic
