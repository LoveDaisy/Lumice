#include "analytic/path_fiber.hpp"

#include <cmath>
#include <limits>

namespace lumice::analytic {

OpticalMarginVerdict ClassifyChainMargins(bool chain_ok, const ChainDomain& domain, int slot_count) {
  OpticalMarginVerdict out;
  out.margin_count = domain.margin_count;
  for (int i = 0; i < domain.margin_count; i++) {
    out.margins[i] = domain.margins[i];
  }
  if (!chain_ok) {
    out.valid = false;
    out.failing_index = domain.margin_count > 0 ? domain.margin_count - 1 : -1;
    out.min_effective_margin = domain.failure_margin;
    for (int i = 0; i < domain.margin_count; i++) {
      if (!(domain.margins[i] > 0.0)) {
        out.failing_index = i;
        out.min_effective_margin = domain.margins[i];
        break;
      }
    }
    out.failing_is_snell = (domain.failure == ChainFailure::kTirBoundary) ||
                           (out.failing_index >= 0 && IsSnellMarginIndex(out.failing_index, slot_count));
    return out;
  }

  double min_eff = std::numeric_limits<double>::infinity();
  int min_idx = -1;
  bool min_is_snell = false;
  for (int i = 0; i < domain.margin_count; i++) {
    const bool is_snell = IsSnellMarginIndex(i, slot_count);
    const double eff = is_snell ? (domain.margins[i] - kSnellEventTolerance) : domain.margins[i];
    if (!std::isfinite(eff)) {
      out.valid = false;
      out.failing_index = i;
      out.failing_is_snell = is_snell;
      out.min_effective_margin = eff;
      return out;
    }
    if (eff < min_eff) {
      min_eff = eff;
      min_idx = i;
      min_is_snell = is_snell;
    }
  }
  out.min_effective_margin = min_eff;
  if (min_eff <= 0.0) {
    out.valid = false;
    out.failing_index = min_idx;
    out.failing_is_snell = min_is_snell;
  } else {
    out.valid = true;
    out.failing_index = -1;
    out.failing_is_snell = false;
  }
  return out;
}

void IcePathMap::Domain(const double r[9], DomainEvaluation* out) const {
  *out = DomainEvaluation{};
  ChainDomain chain;
  chain.margins = out->margins;
  double outgoing[3];
  const bool chain_ok =
      TracePathChain<double>(*table_, slots_, slot_count_, refractive_index_, incident_, r, outgoing, nullptr, &chain);
  out->margin_count = chain.margin_count;
  if (!chain_ok) {
    out->valid = false;
    out->has_event = true;
    out->event_margin = chain.failure_margin;
    switch (chain.failure) {
      case ChainFailure::kTirBoundary:
        out->event = FiberReason::kTirBoundary;
        break;
      case ChainFailure::kNonFinite:
        out->event = FiberReason::kNonFinite;
        break;
      case ChainFailure::kPathInfeasible:
      case ChainFailure::kNone:  // not reached: an invalid chain names its failure
        out->event = FiberReason::kPathInfeasible;
        break;
    }
    return;
  }
  const OpticalMarginVerdict verdict = ClassifyChainMargins(true, chain, slot_count_);
  out->valid = verdict.valid;
  if (!verdict.valid && verdict.failing_is_snell) {
    out->has_event = true;
    out->event = FiberReason::kTirBoundary;
    out->event_margin = verdict.margins[verdict.failing_index];
  }
}

PathDiagnostics EvaluatePathDiagnostics(const FaceNormalTable& table, const int* slots, int slot_count,
                                        double refractive_index, const double incident_direction[3],
                                        const double pose[9]) {
  PathDiagnostics out;
  ChainDomain chain;
  chain.margins = out.margins;
  double outgoing[3];
  out.valid = TracePathChain<double>(table, slots, slot_count, refractive_index, incident_direction, pose, outgoing,
                                     nullptr, &chain);
  out.margin_count = chain.margin_count;
  if (!out.valid) {
    // The chain records a gate pair (entry, exit) before testing it, so the failed gate is the first
    // recorded margin that is not > 0, not necessarily the last one recorded.
    for (int i = 0; i < out.margin_count; i++) {
      if (!(out.margins[i] > 0.0)) {
        out.failed_gate = i;
        break;
      }
    }
    return out;
  }
  const IcePathMap map(table, slots, slot_count, refractive_index, incident_direction);
  out.jacobian = fiber_detail::NormalJacobianAt(map, pose);
  return out;
}

}  // namespace lumice::analytic
