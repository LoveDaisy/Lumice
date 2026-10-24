#include "raypath/detail/survey/survey_mc_ess.hpp"

namespace lumice::raypath::survey {

void AssembleMcRegionEss(const quantity::McRegionEssInput& input, SurveyData* out) {
  out->mc_region_ess = quantity::EvaluateMcRegionEss(input);
}

}  // namespace lumice::raypath::survey
