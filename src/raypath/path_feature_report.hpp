#ifndef LUMICE_RAYPATH_PATH_FEATURE_REPORT_HPP_
#define LUMICE_RAYPATH_PATH_FEATURE_REPORT_HPP_

// Public request surface of the feature report: the constants and request
// struct the C API bridge needs, plus the one entry point that returns the
// report as its JSON form. The report struct itself, its assembly and its
// serializer are module internals (raypath/detail/).

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "analytic/stop.hpp"
#include "config/config_manager.hpp"
#include "raypath/domain/error.hpp"
#include "raypath/domain/input_metadata.hpp"
#include "raypath/domain/scope.hpp"
#include "raypath/feature_report_result.hpp"

namespace lumice::raypath {
constexpr int kFeatureReportSchemaVersion = 3;
constexpr int kDefaultFeatureReportSampleCount = 65536;
constexpr int kMaxFeatureReportSampleCount = 1000000;
constexpr int kMaxFeatureReportWavelengthCount = 32;
constexpr uint64_t kMaxFeatureReportSampleEvaluations = 16777216;
constexpr uint64_t kDefaultFeatureReportOpticalEvaluations = 4000000;
constexpr uint64_t kDefaultFeatureReportFieldEvaluations = 250000000;
constexpr int kDefaultFeatureReportBudgetMs = 15000;
constexpr int kMaxFeatureReportBudgetMs = 120000;
struct PathFeatureReportRequest : PathSelection {
  std::vector<double> wavelengths_nm;
  std::vector<double> wavelength_weights;
  int sample_count = 0;  // zero selects a work-aware dyadic prefix
  int budget_ms = kDefaultFeatureReportBudgetMs;
  uint64_t max_optical_evaluations = kDefaultFeatureReportOpticalEvaluations;
  uint64_t max_field_evaluations = kDefaultFeatureReportFieldEvaluations;
  double bandwidth_rad = 3.14159265358979323846 / 180;
  double location_resolution_rad = .05 * 3.14159265358979323846 / 180;
  std::optional<std::chrono::steady_clock::time_point> deadline;
  // The query's one-way cancel token (the new contract's cancel core). Every stop the report
  // installs folds it in; the C surface's legacy flag entry keeps its own documented path.
  const analytic::StopToken* cancel = nullptr;
};

Error AnalyzePathFeatureReportValue(const ConfigManager& config, const PathFeatureReportRequest& request,
                                    FeatureReportResult* out);

// The v3 report serializer (schema "lumice.path-feature-report"), declared here since the
// typed-view C surface derives the document from a completed typed result through this header;
// the implementation stays engine-side, in this module's detail/.
std::string PathFeatureReportV3ToJson(const FeatureReportResult& result, const char* lumice_version);

// Runs the analysis and serializes the report: `json_out` receives the same
// bytes PathFeatureReportToJson would produce for the assembled report.
Error AnalyzePathFeatureReport(const ConfigManager& config, const PathFeatureReportRequest& request,
                               const std::string& product_version, std::string* json_out);
}  // namespace lumice::raypath
#endif  // LUMICE_RAYPATH_PATH_FEATURE_REPORT_HPP_
