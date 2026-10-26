#include "raypath/survey_report.hpp"

#include "raypath/detail/survey/survey_assembly.hpp"

namespace lumice::raypath {

Error AnalyzePathSurveyValue(const ConfigManager& config, const SurveyRequest& request, survey::SurveyResult* out) {
  return survey::AssembleSurvey(config, request, out);
}

}  // namespace lumice::raypath
