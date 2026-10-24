#ifndef RAYPATH_DETAIL_SURVEY_JSON_HPP_
#define RAYPATH_DETAIL_SURVEY_JSON_HPP_

#include <string>

#include "raypath/survey_result.hpp"

namespace lumice::raypath::survey {

// The schema-4 document (schema "lumice.path-survey"): the one serialization of the survey
// document, engine-side only — the CLI and a future GUI read this, nobody re-serializes.
std::string SurveyResultToJson(const SurveyResult& document, const char* lumice_version);

}  // namespace lumice::raypath::survey

#endif  // RAYPATH_DETAIL_SURVEY_JSON_HPP_
