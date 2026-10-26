#ifndef LUMICE_RAYPATH_SURVEY_REPORT_HPP_
#define LUMICE_RAYPATH_SURVEY_REPORT_HPP_

// Public request surface of the path survey: the constants and request struct the C API bridge
// needs, plus the one entry point that returns the survey as its JSON form. The document model,
// its assembly and its serializer are module internals (raypath/detail/survey/). The survey is
// the L1 census face of the diagnostic pipeline: one single-layer raypath class, routed at T0
// (the identity certificate early-stops the configurational analysis for a rank-0 class).

#include <optional>
#include <string>
#include <vector>

#include "config/config_manager.hpp"
#include "raypath/domain/error.hpp"
#include "raypath/domain/input_metadata.hpp"
#include "raypath/domain/scope.hpp"
#include "raypath/survey_result.hpp"

namespace lumice::raypath::survey {
// The schema-4 serializer (schema "lumice.path-survey"), declared here since the typed-view C
// surface derives the document from a completed typed result through this header (the
// implementation stays engine-side, in this module's detail/).
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
  // draw count, at most survey::kSurveyMcEssMaxDraws (enforced in AssembleSurvey).
  int mc_ess_draws = 0;
};

// Runs the routing and serializes the document: `json_out` receives the schema-4 document.
Error AnalyzePathSurvey(const ConfigManager& config, const SurveyRequest& request, const std::string& product_version,
                        std::string* json_out);
Error AnalyzePathSurveyValue(const ConfigManager& config, const SurveyRequest& request, survey::SurveyResult* out);

}  // namespace lumice::raypath
#endif  // LUMICE_RAYPATH_SURVEY_REPORT_HPP_
