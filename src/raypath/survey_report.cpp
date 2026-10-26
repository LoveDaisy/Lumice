#include "raypath/survey_report.hpp"

#include "raypath/detail/survey/survey_assembly.hpp"

namespace lumice::raypath {

Error AnalyzePathSurvey(const ConfigManager& config, const SurveyRequest& request, const std::string& product_version,
                        std::string* json_out) {
  if (json_out == nullptr) {
    return { ErrorCode::kInvalidArgument, "null json output" };
  }
  survey::SurveyResult document;
  if (Error error = AnalyzePathSurveyValue(config, request, &document); !error.Ok()) {
    return error;
  }
  *json_out = survey::SurveyResultToJson(document, product_version.c_str());
  return {};
}

Error AnalyzePathSurveyValue(const ConfigManager& config, const SurveyRequest& request, survey::SurveyResult* out) {
  return survey::AssembleSurvey(config, request, out);
}

}  // namespace lumice::raypath
