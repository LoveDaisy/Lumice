#ifndef LUMICE_RAYPATH_PATH_FEATURE_REPORT_HPP_
#define LUMICE_RAYPATH_PATH_FEATURE_REPORT_HPP_

// Request, completed result and serializer for the path feature report.

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "analytic/stop.hpp"
#include "raypath/domain/error.hpp"
#include "raypath/domain/input_metadata.hpp"
#include "raypath/domain/scope.hpp"
#include "raypath/feature_report_result.hpp"
#include "raypath/query_control.hpp"

namespace lumice {
struct ConfigManager;
}

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
  std::optional<QueryDeadline> query_deadline;
};

Error AnalyzePathFeatureReportValue(const ConfigManager& config, const PathFeatureReportRequest& request,
                                    FeatureReportResult* out);

std::string PathFeatureReportV3ToJson(const FeatureReportResult& result, const char* lumice_version);
}  // namespace lumice::raypath
#endif  // LUMICE_RAYPATH_PATH_FEATURE_REPORT_HPP_
