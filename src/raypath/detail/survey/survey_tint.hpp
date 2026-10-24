#ifndef RAYPATH_DETAIL_SURVEY_TINT_H_
#define RAYPATH_DETAIL_SURVEY_TINT_H_

#include "raypath/detail/quantity/tint.hpp"
#include "raypath/survey_result.hpp"

namespace lumice::raypath::survey {
void AssembleWeightTint(const quantity::TintInput& input, SurveyData* out);
void AssembleClassTint(const quantity::ClassTintInput& input, SurveyData* out);
void AssembleSinglePoseTint(const quantity::SinglePoseTintInput& input, SurveyData* out);
}  // namespace lumice::raypath::survey
#endif
