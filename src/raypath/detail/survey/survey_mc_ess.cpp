#include "raypath/detail/survey/survey_mc_ess.hpp"

namespace lumice::raypath::survey {

void AssembleMcRegionEss(const McRegionEssInput& input, SurveyResult* out) {
  out->mc_region_ess = EvaluateMcRegionEss(input);
}

}  // namespace lumice::raypath::survey
