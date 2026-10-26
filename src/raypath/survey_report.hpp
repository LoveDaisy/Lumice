#ifndef LUMICE_RAYPATH_SURVEY_REPORT_HPP_
#define LUMICE_RAYPATH_SURVEY_REPORT_HPP_

// Request and completed-result encoding for the path survey.

#include <optional>
#include <string>
#include <vector>

#include "raypath/domain/error.hpp"
#include "raypath/domain/input_metadata.hpp"
#include "raypath/domain/scope.hpp"
#include "raypath/query_control.hpp"
#include "raypath/survey_result.hpp"

namespace lumice {
struct ConfigManager;
}

namespace lumice::raypath::survey {
// The schema-4 serializer projects a completed result.
std::string SurveyResultToJson(const SurveyResult& document, const char* lumice_version);
}  // namespace lumice::raypath::survey

namespace lumice::raypath {
// The survey document family is its own schema (schema 4); it shares nothing field-wise with the
// target-fiber schema 1 or the feature-report schema 3 documents.
constexpr int kSurveySchemaVersion = 4;
constexpr int kDefaultSurveyBudgetMs = 15000;
constexpr int kMaxSurveyBudgetMs = 120000;

struct SurveyRequest : PathSelection {
  // Unset: the scene's wavelength if its spectrum is exactly one discrete wavelength, else
  // kDefaultWavelengthNm — the same resolution the target-fiber request spells.
  std::optional<double> wavelength_nm;
  int budget_ms = kDefaultSurveyBudgetMs;
  // 0 (the default) = the opt-in MC region ESS leg does not run; positive = its requested
  // draw count, at most quantity::kSurveyMcEssMaxDraws (enforced in AssembleSurvey).
  int mc_ess_draws = 0;
  std::optional<QueryDeadline> query_deadline;
};

Error AnalyzePathSurveyValue(const ConfigManager& config, const SurveyRequest& request, survey::SurveyResult* out);

}  // namespace lumice::raypath
#endif  // LUMICE_RAYPATH_SURVEY_REPORT_HPP_
