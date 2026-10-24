#include "raypath/detail/survey/survey_witness.hpp"

namespace lumice::raypath::survey {

void AssembleWitness(const WitnessInput& input, SurveyObject* card) {
  card->witness = EvaluateWitness(input);
}

void AssembleRefusedSupport(const WitnessInput& witness_input, const std::vector<std::vector<int>>& members,
                            const std::string& refusal_mode, const std::string& chromatic_reason,
                            const std::string& budget_note, RefusedSupportObjects objects_discipline,
                            SurveyResult* out) {
  SurveyObject card;
  card.kind = SurveyObjectKind::kSupportSet;
  card.members = members;
  card.evidence.state = EvidenceState::kRefused;
  card.evidence.mode = refusal_mode;
  if (!witness_input.context_only) {
    AssembleWitness(witness_input, &card);
  }
  // The argmax pose is a held pose: it joins the document's warm_index under its leg name (the
  // --warm contract's seed retention; a refused witness holds no pose and contributes none).
  if (card.witness.has_value() && card.witness->state == EvidenceState::kFinite && card.witness->has_pose) {
    std::array<double, 9> seed{};
    for (int k = 0; k < 9; k++) {
      seed[static_cast<size_t>(k)] = card.witness->pose[k];
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
  out->weight_tint = SurveyWeightTint{};
  out->weight_tint->evidence.state = EvidenceState::kUnevaluated;
  out->weight_tint->evidence.reason = chromatic_reason;
  out->budget_note = budget_note;
}

}  // namespace lumice::raypath::survey
