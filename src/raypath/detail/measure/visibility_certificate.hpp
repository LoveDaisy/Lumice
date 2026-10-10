#ifndef LUMICE_RAYPATH_DETAIL_VISIBILITY_CERTIFICATE_HPP_
#define LUMICE_RAYPATH_DETAIL_VISIBILITY_CERTIFICATE_HPP_

// SPECIFICATION — the measure side of the schema3 visibility certificate (design vocabulary:
// certified | partial | unlit | unproven, with lit_fraction). The certificate is a finite
// statement about a continuum, so its inputs carry an EXPLICIT evidence form (pinned in
// measure_geometry_contract.hpp) and the verdict records which form was decisive — the criteria
// below are frozen with the mock tests; integration may not reinterpret them.
//
//   certified   the evidence form is kStructural or kSampledExhaustive, every in-support sample
//               has mu > 0, A > 0, T > 0, a valid chain, and non-degenerate jets. (Pointwise
//               mu·A·T > 0 lifted to the whole object by the declared coverage of the evidence.)
//   partial     lit and dark both occur among the in-support samples (0 < lit_fraction < 1):
//               some covered measure is lit, some is not. A mixed observation, not a coverage
//               claim — it holds under any evidence form, including kSampledPartial.
//               lit_fraction = (sum of weights of lit in-support samples) / (sum of weights of all
//               in-support samples).
//   unlit       the C09 "contour present, no passage" verdict, precisely: a kind-1 curve is
//               present (computed, non-empty), the declared measure is positive SOMEWHERE on the
//               curve's support neighbourhood (the object is not outside the crystal ensemble's
//               declared orientations — mu positive at some curve point), and EVERY in-support
//               fiber candidate is not lit (A·T <= 0, or an invalid chain) under a kStructural or
//               kSampledExhaustive stream.
//               Under kSampledPartial this combination is NOT unlit — it routes to unproven (a
//               finite spot sample cannot certify "no passage anywhere").
//   unproven    anything the evidence cannot decide: partition coverage incomplete (an escape
//               regime), the kind-1 curve walk truncated or escaped, a sampled-partial stream
//               that would need universal quantification, or no in-support sample at all. The
//   `reason` field names the blocking condition (stable strings, listed in the .cpp).
//
// The certificate never invents thresholds: partial vs certified is the lit_fraction 0/1 boundary
// plus the evidence form, nothing numeric to tune. A·T = 0 boundary cases are discriminated, not
// merged: A == 0 with T > 0 is an empty corridor (the gate closed), T == 0 with A > 0 is a
// transmission gate (TIR at entry/exit) — the result carries both flags so the report can name
// the reason, and the mock tests pin each branch separately.

#include "raypath/detail/measure/declared_density.hpp"
#include "raypath/detail/measure/measure_geometry_contract.hpp"

namespace lumice::raypath {

enum class VisibilityState { kCertified, kPartial, kUnlit, kUnproven };

const char* VisibilityStateName(VisibilityState state);
const std::vector<VisibilityState>& RegisteredVisibilityStates();

struct VisibilityCertificate {
  VisibilityState state = VisibilityState::kUnproven;
  // Measure-weighted lit fraction over the in-support samples: NaN when there is no in-support
  // sample; the fail-closed early exits (degenerate / escape / truncated / kind-1 not computed)
  // keep the 0.0 default instead.
  double lit_fraction = 0.0;
  // Which evidence form the verdict rests on (the stream's form; mirrors it for the report).
  FiberSampleStream::EvidenceForm evidence = FiberSampleStream::EvidenceForm::kSampledPartial;
  // True when every in-support sample's jets were non-degenerate. Part of the certified
  // conjunction (the plan's "mu·A·T > 0 + non-degenerate jets"): a degenerate jet among the
  // in-support samples downgrades certified to unproven with reason "degenerate_jets" — the
  // producer could not certify the local normal Jacobian, so the pointwise statement is not
  // lifted to the whole object. jets_ok stays false with any state, so the report can name it.
  bool jets_ok = true;
  // Discriminated A/T boundary flags over the in-support samples (the unlit reasons).
  bool saw_zero_area = false;          // some in-support sample has A == 0, T > 0
  bool saw_zero_transmission = false;  // some in-support sample has T == 0, A > 0
  // Stable machine string naming the blocking condition for kUnproven ("" for the others).
  const char* reason = "";
};

// `kind1` may be null (the caller has no critical-set curve — then unlit is unreachable and the
// verdict comes from the stream alone); `partition` may be null (treated as kUnknown coverage).
// Routing, in order: a degenerate measure is unproven; a kIncomplete partition (a real escape)
// is unproven fail-closed — an escaped slice cannot vouch anything; a kind-1 curve whose
// existence is not kComputed is unproven (walk_truncated / escaped / s4_declared each with its
// own stable reason, and — the enum being open — ANY other value with reason
// "kind1_existence_unknown": the routing is negative-form fail-closed, an existence value this
// code has never seen never reads as computed). From there the stream
// decides: no in-support sample at all is unproven; all-lit under kStructural or
// kSampledExhaustive evidence is CERTIFIED (a per-point statement — a kUnknown or absent
// partition does not block it: the object's bucket placement consumes existence and partition
// separately, this certificate owns only visibility), unless a degenerate jet was seen among the
// in-support samples — then unproven with reason "degenerate_jets" (the certified conjunction
// keeps its jets leg); mixed lit/dark is partial; all-dark with a computed non-empty kind-1 curve
// under exhaustive evidence is unlit (C09) when the declared measure is positive somewhere on the
// curve — a curve lying entirely in the measure's zero set routes unproven
// ("kind1_curve_measure_zero": the dark in-support samples say nothing about an unreachable
// contour); everything else is unproven with a named reason.
VisibilityCertificate CertifyVisibility(const UMarginal& measure, const FiberSampleStream& stream,
                                        const CriticalSetCurve* kind1, const PartitionContext* partition,
                                        double angular_tol_rad);

// G8's ruling (661's registered question, answered in 666.1): a kind-1 object whose critical set
// has SEVERAL connected components certifies PER COMPONENT and the object layer aggregates the
// union — adding a function, not changing the single-curve semantics. Each component's verdict
// is exactly CertifyVisibility on that curve alone. The aggregate, strongest first:
//   unlit     any component unlit — SOUND on one component alone: unlit's three legs (a present
//             curve, the measure positive on it, every in-support candidate dark) are grounded
//             by that component plus the shared stream; no other component's state enters them;
//   unproven  else any component unproven (an undecided component keeps the object undecided);
//   partial   else any component partial (a mixed observation, not a coverage claim);
//   certified else all components certified.
// The decisive component's certificate is carried wholesale (its lit_fraction, saw flags and
// reason are that component's; the report's per-component rows carry their own).
VisibilityCertificate CertifyVisibilityPerComponent(const UMarginal& measure, const FiberSampleStream& stream,
                                                    const std::vector<const CriticalSetCurve*>& components,
                                                    const PartitionContext* partition, double angular_tol_rad);

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_DETAIL_VISIBILITY_CERTIFICATE_HPP_
