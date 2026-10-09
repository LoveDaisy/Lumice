#include "raypath/detail/schema3/no_related_feature.hpp"

#include <algorithm>
#include <cmath>

#include "raypath/detail/measure/visibility_certificate.hpp"

namespace lumice::raypath::schema3 {

const char* NoRelatedBasisName(NoRelatedBasis basis) {
  switch (basis) {
    case NoRelatedBasis::kZeroSpectralSignal:
      return "zero_spectral_signal";
    case NoRelatedBasis::kCertifiedSmoothRadiance:
      return "certified_smooth_radiance";
  }
  return "certified_smooth_radiance";
}

namespace {

// The absence-not-proven reading, fixed text: a refusal to issue is the consumer's cue to
// report completed-with-an-empty-list WITHOUT an absence claim.
std::string AbsenceNote() {
  return "no basis declares the absence: the document reports completed with an empty feature "
         "list, and an empty list does not prove absence — the structural certificate and the "
         "MC standing did not jointly license the smooth-radiance claim";
}

}  // namespace

NoRelatedRuling DeriveNoRelatedFeature(const Schema3DiscoveryCore& core, const UnattributedOutcome& unattributed,
                                       USupportKind support_kind, bool zero_spectral_signal) {
  NoRelatedRuling ruling;

  // ---- condition 1: partition 完备无 escape ------------------------------------------
  ruling.partition_complete_no_escape = !core.support.members.empty();
  for (size_t i = 0; i < core.support.members.size(); i++) {
    if (core.support.members[i].axis.context.coverage != PartitionContext::Coverage::kComplete) {
      ruling.partition_complete_no_escape = false;
      ruling.partition_failure = "support row " + std::to_string(i) + " (member size " +
                                 std::to_string(core.support.members[i].member.size()) +
                                 ") did not complete: coverage " +
                                 std::to_string(static_cast<int>(core.support.members[i].axis.context.coverage)) +
                                 (core.support.members[i].axis.regime_slug.empty() ?
                                      "" :
                                      ", escape slug " + core.support.members[i].axis.regime_slug);
      break;
    }
  }
  if (core.support.members.empty()) {
    ruling.partition_failure = "no support rows: nothing certifies the partition";
  }

  // ---- condition 2: 全对象 unlit-or-none ----------------------------------------------
  ruling.all_objects_unlit_or_none = true;  // the empty object set reads true (plan A6)
  for (size_t i = 0; i < core.objects.size(); i++) {
    if (core.objects[i].visibility.state != VisibilityState::kUnlit) {
      ruling.all_objects_unlit_or_none = false;
      ruling.unlit_failure = "object " + std::to_string(i) + " carries visibility state " +
                             VisibilityStateName(core.objects[i].visibility.state) +
                             " — a default certificate is unproven and breaks the arm (fail closed)";
      break;
    }
  }

  // ---- condition 3: 无 sufficient-ESS unattributed ------------------------------------
  ruling.no_sufficient_ess_unattributed = unattributed.structures.empty();
  if (!unattributed.structures.empty()) {
    double widest = 0.0;
    for (const UnattributedStructure& structure : unattributed.structures) {
      widest = std::max(widest, structure.min_margin_rad);
    }
    ruling.unattributed_failure = "the forward pass emitted " + std::to_string(unattributed.structures.size()) +
                                  " unattributed structure(s) at standing (widest margin " + std::to_string(widest) +
                                  " rad) — the MC contradicts the claim";
  }

  // ---- condition 4: S4 范围声明 --------------------------------------------------------
  for (size_t i = 0; i < core.coverage.declared_not_produced.size(); i++) {
    if (core.coverage.declared_not_produced[i] == ObjectKind::kS4BranchBoundary) {
      ruling.s4_scope_declared = true;
      if (i < core.coverage.declared_reasons.size()) {
        ruling.s4_scope_note = core.coverage.declared_reasons[i];
      }
      break;
    }
  }
  if (!ruling.s4_scope_declared) {
    ruling.s4_failure =
        "the coverage does not declare the S4 branch boundary as "
        "declared-not-produced — the scope statement is missing";
  }

  // ---- rule 3's gate: 2D valid support --------------------------------------------------
  ruling.two_d_valid_support = support_kind == USupportKind::kArea;

  // ---- the rules ------------------------------------------------------------------------
  if (zero_spectral_signal) {
    // Rule A outranks B: issued on the zero-signal basis whatever B's conditions say.
    ruling.issued = true;
    ruling.basis = NoRelatedBasis::kZeroSpectralSignal;
    return ruling;
  }
  if (ruling.partition_complete_no_escape && ruling.all_objects_unlit_or_none &&
      ruling.no_sufficient_ess_unattributed && ruling.s4_scope_declared && ruling.two_d_valid_support) {
    ruling.issued = true;
    ruling.basis = NoRelatedBasis::kCertifiedSmoothRadiance;
    return ruling;
  }
  ruling.absence_not_proven_note = AbsenceNote();
  return ruling;
}

}  // namespace lumice::raypath::schema3
