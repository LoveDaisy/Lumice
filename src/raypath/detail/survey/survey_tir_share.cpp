#include "raypath/detail/survey/survey_tir_share.hpp"

namespace lumice::raypath::survey {

void AssembleTirShare(const quantity::TirShareInput& input, SurveyData* out) {
  out->tir_share = quantity::EvaluateTirShare(input);
}

}  // namespace lumice::raypath::survey
