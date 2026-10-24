#include "raypath/survey_report.hpp"

#include "raypath/detail/survey/survey_assembly.hpp"
#include "raypath/detail/survey/survey_json.hpp"

namespace lumice::raypath {

Error AnalyzePathSurvey(const ConfigManager& config, const SurveyRequest& request, const std::string& product_version,
                        std::string* json_out) {
  if (json_out == nullptr) {
    return { ErrorCode::kInvalidArgument, "null json output" };
  }
  survey::SurveyResult document;
  if (Error error = survey::AssembleSurvey(config, request, &document); !error.Ok()) {
    return error;
  }
  *json_out = survey::SurveyResultToJson(document, product_version.c_str());
  return {};
}

}  // namespace lumice::raypath
