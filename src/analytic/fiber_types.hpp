#ifndef LUMICE_ANALYTIC_FIBER_TYPES_HPP_
#define LUMICE_ANALYTIC_FIBER_TYPES_HPP_


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

enum class ComponentKind { kClosed, kArc };

// The four incomplete counters of section 9.5.6, as a candidate's cause.
enum class IncompleteCause {
  kArcBackwardFailed,
  kArcBackwardClosedAnomaly,
  kUnnamedEvent,
  kNotConverged,
};


}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_FIBER_TYPES_HPP_
