#ifndef RAYPATH_DETAIL_SURVEY_TIR_SHARE_H_
#define RAYPATH_DETAIL_SURVEY_TIR_SHARE_H_

#include "raypath/detail/quantity/tir_share.hpp"
#include "raypath/survey_result.hpp"

namespace lumice::raypath::survey {
void AssembleTirShare(const quantity::TirShareInput& input, SurveyData* out);
}

#endif
