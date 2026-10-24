#ifndef RAYPATH_DETAIL_SURVEY_TINT_H_
#define RAYPATH_DETAIL_SURVEY_TINT_H_

#include "raypath/detail/quantity/tint.hpp"
#include "raypath/survey_result.hpp"

namespace lumice::raypath::survey {
void AssembleWeightTint(const TintInput& input, SurveyResult* out);
void AssembleClassTint(const ClassTintInput& input, SurveyResult* out);
void AssembleSinglePoseTint(const SinglePoseTintInput& input, SurveyResult* out);
}  // namespace lumice::raypath::survey
#endif
