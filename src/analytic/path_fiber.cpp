#include "analytic/path_fiber.hpp"

namespace lumice::analytic {

void IcePathMap::Domain(const double r[9], DomainEvaluation* out) const {
  *out = DomainEvaluation{};
  ChainDomain chain;
  chain.margins = out->margins;
  double outgoing[3];
  out->valid =
      TracePathChain<double>(*table_, slots_, slot_count_, refractive_index_, incident_, r, outgoing, nullptr, &chain);
  out->margin_count = chain.margin_count;
  if (!out->valid) {
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
  // A valid chain recorded every validity margin: the entry Snell discriminant is the second, the
  // exit one the last. The smaller one (the entry on a tie, as LI's min over its name list) is the
  // event margin.
  const double entry_snell = out->margins[1];
  const double exit_snell = out->margins[out->margin_count - 1];
  const double snell = exit_snell < entry_snell ? exit_snell : entry_snell;
  if (snell <= kSnellEventTolerance) {
    out->valid = false;
    out->has_event = true;
    out->event = FiberReason::kTirBoundary;
    out->event_margin = snell;
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
