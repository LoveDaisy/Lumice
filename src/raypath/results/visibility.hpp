#ifndef LUMICE_RAYPATH_RESULTS_VISIBILITY_HPP_
#define LUMICE_RAYPATH_RESULTS_VISIBILITY_HPP_

#include "raypath/domain/measure_geometry.hpp"
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

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_RESULTS_VISIBILITY_HPP_
