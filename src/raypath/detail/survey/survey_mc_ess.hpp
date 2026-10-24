#ifndef RAYPATH_DETAIL_SURVEY_MC_ESS_H_
#define RAYPATH_DETAIL_SURVEY_MC_ESS_H_

#include "raypath/detail/quantity/mc_ess.hpp"
#include "raypath/survey_result.hpp"

namespace lumice::raypath::survey {
void AssembleMcRegionEss(const McRegionEssInput& input, SurveyData* out);
}

#endif
