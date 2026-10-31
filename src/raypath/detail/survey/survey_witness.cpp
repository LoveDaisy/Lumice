#include "raypath/detail/survey/survey_witness.hpp"

#include "util/fatal.hpp"

namespace lumice::raypath::survey {

void AssembleWitness(const quantity::WitnessInput& input, SurveyObject* card) {
  // The witness hangs on a support card only; a wrong-kind card is an assembly invariant break,
  // not a silently dropped evidence block.
  if (!card->SetWitness(quantity::EvaluateWitness(input))) {
    FatalAbort("witness attached to a non-support survey card");
  }
}

void AssembleRefusedSupport(const quantity::WitnessInput& witness_input, const std::vector<std::vector<int>>& members,
                            const std::string& refusal_mode, const std::string& chromatic_reason,
                            const std::string& budget_note, RefusedSupportObjects objects_discipline, SurveyData* out) {
  SurveyObject card;
  card.SetKind(SurveyObjectKind::kSupportSet);
  card.members = members;
  card.SetState(EvidenceState::kRefused);
  card.Mode() = refusal_mode;
  if (!witness_input.context_only) {
    AssembleWitness(witness_input, &card);
  }
  // The argmax pose is a held pose: it joins the document's warm_index under its leg name (the
  // --warm contract's seed retention; a refused witness holds no pose and contributes none).
  if (card.Witness() != nullptr && card.Witness()->State() == EvidenceState::kFinite &&
      card.Witness()->Value().has_pose) {
    std::array<double, 9> seed{};
    for (int k = 0; k < 9; k++) {
      seed[static_cast<size_t>(k)] = card.Witness()->Value().pose[k];
    }
    out->warm_index.seeds.push_back({ std::move(seed), "witness_argmax" });
  }
  // The witness leg's stop, read off the leg's own refusal before the card moves: the
  // sub-block already carries the stop_requested mode; the document-level note names the leg.
  const bool witness_stopped = card.Witness() != nullptr && card.Witness()->State() == EvidenceState::kRefused &&
                               card.Witness()->Mode() == "stop_requested";
  if (objects_discipline == RefusedSupportObjects::kReplaceObjects) {
    out->objects.clear();
  }
  out->objects.push_back(std::move(card));
  out->chromatic.state = EvidenceState::kUnevaluated;
  out->chromatic.reason = chromatic_reason;
  out->weight_tint = WeightTintResult{};
  out->weight_tint->SetState(EvidenceState::kUnevaluated);
  out->weight_tint->Reason() = chromatic_reason;
  out->budget_note = budget_note;
  // The T0 note's two-arm shape, appended only on a stopped run — an unstopped document's
  // note stays byte-frozen (the corpus regression pins that).
  if (witness_stopped && witness_input.stop != nullptr) {
    out->budget_note += witness_input.stop->Why() == analytic::StopReason::kDeadline ?
                            " The witness lattice leg stopped at the query's unified deadline." :
                            " The witness lattice leg stopped at a cooperative poll (explicit cancel).";
  }
}

}  // namespace lumice::raypath::survey
