#include "raypath/detail/survey/survey_witness.hpp"

namespace lumice::raypath::survey {

void AssembleWitness(const quantity::WitnessInput& input, SurveyObject* card) {
  card->SetWitness(quantity::EvaluateWitness(input));
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
    out->warm_index.seeds.push_back(std::move(seed));
    out->warm_index.seed_sources.emplace_back("witness_argmax");
  }
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
}

}  // namespace lumice::raypath::survey
