#include "raypath/detail/survey/survey_tint.hpp"

namespace lumice::raypath::survey {
namespace {
void MountTint(WeightTintResult tint, int member_count, const std::string& geometry_basis, SurveyData* out) {
  out->weight_tint = std::move(tint);
  if (out->weight_tint->State() == EvidenceState::kRefused) {
    out->chromatic.state = EvidenceState::kRefused;
    out->chromatic.mode = out->weight_tint->Mode();
  } else {
    out->chromatic.state = EvidenceState::kFinite;
    out->chromatic.coverage.members_evaluated = member_count;
    out->chromatic.coverage.wavelength_nm = kChromaticBlueNm;
    out->chromatic.coverage.geometry_basis = geometry_basis;
  }
}
}  // namespace

void AssembleWeightTint(const quantity::TintInput& input, SurveyData* out) {
  MountTint(quantity::EvaluateWeightTint(input), input.member_count,
            "the tint leg ran on the class representative at the fixed " + ChromaticPairProseRedFirst() + " pair", out);
}

void AssembleClassTint(const quantity::ClassTintInput& input, SurveyData* out) {
  out->weight_tint = quantity::EvaluateClassTint(input);
}

void AssembleSinglePoseTint(const quantity::SinglePoseTintInput& input, SurveyData* out) {
  MountTint(
      quantity::EvaluateSinglePoseTint(input), input.member_count,
      "the 0D arm's only chromatic leg is the single-pose weight at the " + ChromaticPairProseRedFirst() + " pair",
      out);
}

}  // namespace lumice::raypath::survey
