// C API bridge for lumice_raypath.h. Opaque handles own completed raypath values; JSON is
// derived from those values and never feeds the query or a typed reader.

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "config/config_manager.hpp"
#include "include/lumice_raypath.h"
#include "raypath/detail/survey/survey_mc_ess.hpp"
#include "raypath/path_feature_report.hpp"
#include "raypath/probe_analysis.hpp"
#include "raypath/probe_json.hpp"
#include "raypath/single_path_analysis.hpp"
#include "raypath/single_path_json.hpp"
#include "raypath/survey_report.hpp"
#include "raypath/workspace.hpp"
#include "server/c_api_engine_internal.hpp"  // lumice::capi::ToCApiErrorCode
#include "server/c_api_scene_internal.hpp"   // SceneRoot
#include "server/server.hpp"
#include "util/logger.hpp"

static_assert(LUMICE_SINGLE_PATH_MAX_SAMPLE_COUNT == lumice::raypath::kMaxSampleCount,
              "lumice_raypath.h's sample-count bound is the module's");
static_assert(LUMICE_SINGLE_PATH_MAX_SUN_GRID_LAT_COUNT == lumice::raypath::kMaxSunGridLatCount,
              "lumice_raypath.h and the single-path module must agree on the sun-grid bound");
static_assert(LUMICE_SINGLE_PATH_DEFAULT_BUDGET_MS == lumice::raypath::kDefaultPointBudgetMs,
              "lumice_raypath.h and the single-path module must agree on the default budget");
static_assert(LUMICE_SINGLE_PATH_MAX_BUDGET_MS == lumice::raypath::kMaxPointBudgetMs,
              "lumice_raypath.h and the single-path module must agree on the budget bound");
static_assert(LUMICE_PATH_FEATURE_REPORT_MAX_SAMPLE_COUNT == lumice::raypath::kMaxFeatureReportSampleCount,
              "lumice_raypath.h and the feature-report module must agree on the sample bound");
static_assert(LUMICE_PATH_FEATURE_REPORT_MAX_WAVELENGTH_COUNT == lumice::raypath::kMaxFeatureReportWavelengthCount,
              "lumice_raypath.h and the feature-report module must agree on the wavelength bound");
static_assert(LUMICE_PATH_FEATURE_REPORT_MAX_SAMPLE_EVALUATIONS == lumice::raypath::kMaxFeatureReportSampleEvaluations,
              "lumice_raypath.h and the feature-report module must agree on the total-work bound");
static_assert(LUMICE_PATH_SURVEY_MAX_BUDGET_MS == lumice::raypath::kMaxSurveyBudgetMs,
              "lumice_raypath.h and the survey module must agree on the budget bound");
static_assert(LUMICE_PATH_SURVEY_DEFAULT_BUDGET_MS == lumice::raypath::kDefaultSurveyBudgetMs,
              "lumice_raypath.h and the survey module must agree on the default budget");
static_assert(LUMICE_PATH_SURVEY_MAX_MC_ESS_DRAWS == lumice::raypath::quantity::kSurveyMcEssMaxDraws,
              "lumice_raypath.h and the survey module must agree on the mc-ess draw bound");
static_assert(LUMICE_PATH_PROBE_DEFAULT_BUDGET_MS == lumice::raypath::kDefaultProbeBudgetMs,
              "lumice_raypath.h and the probe module must agree on the default budget");
static_assert(LUMICE_PATH_PROBE_MAX_BUDGET_MS == lumice::raypath::kMaxProbeBudgetMs,
              "lumice_raypath.h and the probe module must agree on the budget bound");
static_assert(LUMICE_PATH_PROBE_MAX_WAVELENGTH_COUNT == lumice::raypath::kMaxProbeWavelengthCount,
              "lumice_raypath.h and the probe module must agree on the wavelength bound");

// Derived JSON is cached under a mutex; all getters read the frozen value instead.
struct LUMICE_SinglePathResult_ {
  lumice::raypath::PointResult result;
  mutable std::mutex derived_mutex;
  mutable std::string derived_json;
};

struct LUMICE_PathFeatureReport_ {
  lumice::raypath::FeatureReportResult result;
  mutable std::mutex derived_mutex;
  mutable std::string derived_json;
};

struct LUMICE_PathSurvey_ {
  lumice::raypath::survey::SurveyResult result;
  mutable std::mutex derived_mutex;
  mutable std::string derived_json;
};

struct LUMICE_PathProbe_ {
  lumice::raypath::ProbeResult result;
  mutable std::mutex derived_mutex;
  mutable std::string derived_json;
};

struct LUMICE_PathWorkspace_ {
  std::unique_ptr<lumice::raypath::AnalysisWorkspace> workspace;
};

struct LUMICE_PathWarmData_ {
  lumice::raypath::WarmCandidate candidate;
};

struct LUMICE_PathCancelToken_ {
  lumice::analytic::StopToken token;
};

namespace {

template <typename Handle, typename Serialize>
LUMICE_ErrorCode CopyDerivedJson(const Handle* handle, Serialize serialize, char* out_buf, size_t buf_size,
                                 size_t* out_len) {
  if (handle == nullptr) {
    return LUMICE_ERR_NULL_ARG;
  }
  try {
    std::lock_guard<std::mutex> lock(handle->derived_mutex);
    if (handle->derived_json.empty()) {
      handle->derived_json = serialize(handle->result);
    }
    const std::string& json = handle->derived_json;
    if (out_len != nullptr) {
      *out_len = json.size();
    }
    if (out_buf != nullptr && buf_size > 0) {
      const size_t n = std::min(json.size(), buf_size - 1);
      std::memcpy(out_buf, json.data(), n);
      out_buf[n] = '\0';
    }
    return LUMICE_OK;
  } catch (const std::exception& e) {
    LOG_ERROR("raypath ToJson: {}", e.what());
  } catch (...) {
    LOG_ERROR("raypath ToJson: unknown exception");
  }
  if (out_len != nullptr) {
    *out_len = 0;
  }
  if (out_buf != nullptr && buf_size > 0) {
    out_buf[0] = '\0';
  }
  return LUMICE_ERR_UNKNOWN;
}

// The v4.50 request, the smallest `struct_size` this build reads: through its last v4.50 field, so
// that a field appended later does not raise it. The v4.52 append adds the budget/stop group,
// read only when the whole group fits.
constexpr size_t kRequestSizeV450 = offsetof(LUMICE_SinglePathRequest, warm_json_len) + sizeof(size_t);
constexpr size_t kRequestSizeV452 = offsetof(LUMICE_SinglePathRequest, interrupted) + sizeof(void*);
constexpr size_t kFeatureReportRequestSizeV1 = offsetof(LUMICE_PathFeatureReportRequest, sample_count) + sizeof(int);
constexpr size_t kSurveyRequestSizeV1 = offsetof(LUMICE_PathSurveyRequest, symmetry_bits_plus_one) + sizeof(int);

void WriteError(char* err_buf, size_t err_size, const std::string& message) {
  if (err_buf == nullptr || err_size == 0) {
    return;
  }
  const size_t n = std::min(message.size(), err_size - 1);
  std::memcpy(err_buf, message.data(), n);
  err_buf[n] = '\0';
}

LUMICE_ErrorCode Refuse(const lumice::raypath::Error& e, char* err_buf, size_t err_size) {
  WriteError(err_buf, err_size, std::string(lumice::raypath::ErrorCodeName(e.code)) + ": " + e.message);
  return LUMICE_ERR_INVALID_VALUE;
}

LUMICE_ErrorCode ValidateQueryOptions(const LUMICE_PathQueryOptions* options, bool accepts_warm, char* err_buf,
                                      size_t err_size) {
  if (options == nullptr) {
    return LUMICE_OK;
  }
  if (options->struct_size < sizeof(LUMICE_PathQueryOptions) || options->deadline_ms < 0 ||
      options->deadline_ms > LUMICE_SINGLE_PATH_MAX_BUDGET_MS || (!accepts_warm && options->warm != nullptr) ||
      (options->include_context != 0 && options->include_context != 1)) {
    WriteError(err_buf, err_size, "invalid_argument: invalid query options");
    return LUMICE_ERR_INVALID_VALUE;
  }
  return LUMICE_OK;
}

LUMICE_ErrorCode ValidateWarm(const char* text, size_t length, const LUMICE_PathQueryOptions* options, char* err_buf,
                              size_t err_size) {
  if (text != nullptr && length > 0) {
    WriteError(err_buf, err_size,
               options != nullptr && options->warm != nullptr ?
                   "invalid_argument: both typed warm and warm_json were supplied" :
                   "invalid_argument: import warm_json with LUMICE_PathWarmDataFromJson");
    return LUMICE_ERR_INVALID_VALUE;
  }
  return LUMICE_OK;
}

LUMICE_ErrorCode Analyze(const LUMICE_Scene* scene, const LUMICE_SinglePathRequest& request,
                         lumice::raypath::AnalysisWorkspace* workspace, const LUMICE_PathQueryOptions* options,
                         LUMICE_SinglePathResult** out, char* err_buf, size_t err_size) {
  namespace rp = lumice::raypath;

  rp::SinglePathRequest req;
  req.crystal_id = static_cast<lumice::IdType>(request.crystal_id);
  if (request.crystal_id < 0 || request.crystal_id != static_cast<int>(req.crystal_id)) {
    return Refuse(
        { rp::ErrorCode::kUnknownCrystalId, "no crystal entry with id " + std::to_string(request.crystal_id) }, err_buf,
        err_size);
  }
  if (request.face_count < 0 || request.layer_count < 0) {
    return Refuse({ rp::ErrorCode::kInvalidPath, "negative face or layer count" }, err_buf, err_size);
  }
  int consumed = 0;
  for (int i = 0; i < request.layer_count; i++) {
    const int n = request.layer_face_counts[i];
    if (n < 0 || n > request.face_count - consumed) {
      return Refuse({ rp::ErrorCode::kInvalidPath, "layer face counts do not add up to face_count" }, err_buf,
                    err_size);
    }
    req.path_layers.emplace_back(request.faces + consumed, request.faces + consumed + n);
    consumed += n;
  }
  if (consumed != request.face_count) {
    return Refuse({ rp::ErrorCode::kInvalidPath, "layer face counts do not add up to face_count" }, err_buf, err_size);
  }
  req.target_altitude_deg = request.target_altitude_deg;
  req.target_azimuth_deg = request.target_azimuth_deg;
  if (request.wavelength_nm > 0.0) {  // false for NaN too: unset
    req.wavelength_nm = request.wavelength_nm;
  }
  req.sample_count = request.sample_count == 0 ? rp::kDefaultSampleCount : request.sample_count;
  req.sun_grid_lat_count = request.sun_grid_lat_count;
  if (workspace != nullptr) {
    req.budget_ms = options != nullptr ? options->deadline_ms : 0;
    if (options != nullptr && options->warm != nullptr) {
      req.warm_candidate = options->warm->candidate;
    }
  } else if (request.struct_size >= kRequestSizeV452) {
    if (request.deadline_ms < 0) {  // a negative is not the default: refuse as the contract is
      // written, not silently remap it (the CLI never sends one; a direct caller might)
      return Refuse({ rp::ErrorCode::kInvalidArgument,
                      "deadline_ms must be 0 (the default) or at most " + std::to_string(rp::kMaxPointBudgetMs) },
                    err_buf, err_size);
    }
    req.budget_ms = request.deadline_ms > 0 ? request.deadline_ms : rp::kDefaultPointBudgetMs;
    if (req.budget_ms > rp::kMaxPointBudgetMs) {
      return Refuse({ rp::ErrorCode::kInvalidArgument,
                      "deadline_ms must be 0 (the default) or at most " + std::to_string(rp::kMaxPointBudgetMs) },
                    err_buf, err_size);
    }
    req.interrupted = request.interrupted;
  }

  // The scene document LUMICE_CommitScene would hand the server, through the same parser. Parsed
  // before the warm text so the schema-4 gates can compare it.
  lumice::ConfigManager config;
  if (workspace == nullptr) {
    if (const lumice::Error err = lumice::ParseConfigManager(SceneRoot(scene), "LUMICE_AnalyzeSinglePath",
                                                             lumice::GetGlobalLogger(), nullptr, &config)) {
      WriteError(err_buf, err_size, "invalid_scene: " + err.message);
      return lumice::capi::ToCApiErrorCode(err.code);
    }
  }

  if (workspace == nullptr && request.warm_json != nullptr && request.warm_json_len > 0) {
    req.warm_candidate.emplace();
    req.warm_candidate->decode_error =
        rp::DecodeWarmCandidate(std::string(request.warm_json, request.warm_json_len), &*req.warm_candidate);
  }

  rp::PointResult result;
  const rp::Error error =
      workspace != nullptr ?
          workspace->QueryPoint(req, options == nullptr || options->include_context != 0, &result,
                                options != nullptr && options->cancel != nullptr ? &options->cancel->token : nullptr) :
          rp::AnalyzePathPointValue(config, req, true, &result);
  if (!error.Ok()) {
    return Refuse(error, err_buf, err_size);
  }
  auto handle = std::make_unique<LUMICE_SinglePathResult_>();
  handle->result = std::move(result);
  *out = handle.release();
  return LUMICE_OK;
}

LUMICE_ErrorCode AnalyzeReport(const LUMICE_Scene* scene, const LUMICE_PathFeatureReportRequest& request,
                               lumice::raypath::AnalysisWorkspace* workspace, const LUMICE_PathQueryOptions* options,
                               LUMICE_PathFeatureReport** out, char* err_buf, size_t err_size) {
  namespace rp = lumice::raypath;
  const auto request_begin = std::chrono::steady_clock::now();

  rp::PathFeatureReportRequest req;
  req.crystal_id = static_cast<lumice::IdType>(request.crystal_id);
  if (request.crystal_id < 0 || request.crystal_id != static_cast<int>(req.crystal_id)) {
    return Refuse(
        { rp::ErrorCode::kUnknownCrystalId, "no crystal entry with id " + std::to_string(request.crystal_id) }, err_buf,
        err_size);
  }
  if (request.face_count < 0 || request.layer_count < 0 || request.wavelength_count < 0) {
    return Refuse({ rp::ErrorCode::kInvalidArgument, "negative face, layer or wavelength count" }, err_buf, err_size);
  }
  if (request.layer_count <= 0 || request.face_count <= 0 || request.faces == nullptr ||
      request.layer_face_counts == nullptr) {
    return Refuse({ rp::ErrorCode::kInvalidPath,
                    "a path feature report requires one non-empty layer with a non-null face sequence" },
                  err_buf, err_size);
  }
  if (request.wavelength_count > rp::kMaxFeatureReportWavelengthCount) {
    return Refuse({ rp::ErrorCode::kInvalidArgument, "at most " + std::to_string(rp::kMaxFeatureReportWavelengthCount) +
                                                         " wavelengths may be requested" },
                  err_buf, err_size);
  }
  int consumed = 0;
  for (int i = 0; i < request.layer_count; i++) {
    const int count = request.layer_face_counts[i];
    if (count < 0 || count > request.face_count - consumed) {
      return Refuse({ rp::ErrorCode::kInvalidPath, "layer face counts do not add up to face_count" }, err_buf,
                    err_size);
    }
    req.path_layers.emplace_back(request.faces + consumed, request.faces + consumed + count);
    consumed += count;
  }
  if (consumed != request.face_count) {
    return Refuse({ rp::ErrorCode::kInvalidPath, "layer face counts do not add up to face_count" }, err_buf, err_size);
  }
  if (request.wavelength_count > 0) {
    req.wavelengths_nm.assign(request.wavelengths_nm, request.wavelengths_nm + request.wavelength_count);
  }
  if (request.wavelength_weights != nullptr) {
    req.wavelength_weights.assign(request.wavelength_weights, request.wavelength_weights + request.wavelength_count);
  }
  req.sample_count = request.sample_count;
  if (request.struct_size >= sizeof(LUMICE_PathFeatureReportRequest)) {
    if (request.budget_ms != 0) {
      req.budget_ms = request.budget_ms;
    }
    if (request.max_optical_evaluations != 0) {
      req.max_optical_evaluations = request.max_optical_evaluations;
    }
    if (request.max_field_evaluations != 0) {
      req.max_field_evaluations = request.max_field_evaluations;
    }
    if (request.bandwidth_rad != 0) {
      req.bandwidth_rad = request.bandwidth_rad;
    }
    if (request.location_resolution_rad != 0) {
      req.location_resolution_rad = request.location_resolution_rad;
    }
    if (request.symmetry_bits_plus_one < 0 || request.symmetry_bits_plus_one > 8) {
      return Refuse({ rp::ErrorCode::kInvalidArgument, "invalid physical symmetry bits" }, err_buf, err_size);
    }
    if (request.symmetry_bits_plus_one > 0) {
      req.symmetry_bits = static_cast<uint8_t>(request.symmetry_bits_plus_one - 1);
    }
    if (request.scene_layer_plus_one < 0) {
      return Refuse({ rp::ErrorCode::kInvalidArgument, "negative scene layer selection" }, err_buf, err_size);
    }
    if (request.scene_layer_plus_one > 0) {
      req.scene_layer = static_cast<size_t>(request.scene_layer_plus_one - 1);
    }
  }

  lumice::ConfigManager config;
  if (workspace == nullptr) {
    if (const lumice::Error err = lumice::ParseConfigManager(SceneRoot(scene), "LUMICE_AnalyzePathFeatureReport",
                                                             lumice::GetGlobalLogger(), nullptr, &config)) {
      WriteError(err_buf, err_size, "invalid_scene: " + err.message);
      return lumice::capi::ToCApiErrorCode(err.code);
    }
    req.deadline = request_begin + std::chrono::milliseconds(req.budget_ms);
  } else {
    req.budget_ms = options != nullptr ? options->deadline_ms : 0;
  }
  rp::FeatureReportResult result;
  const rp::Error error =
      workspace != nullptr ?
          workspace->QueryReport(req, &result,
                                 options != nullptr && options->cancel != nullptr ? &options->cancel->token : nullptr) :
          rp::AnalyzePathFeatureReportValue(config, req, &result);
  if (!error.Ok()) {
    return Refuse(error, err_buf, err_size);
  }
  auto handle = std::make_unique<LUMICE_PathFeatureReport_>();
  handle->result = std::move(result);
  *out = handle.release();
  return LUMICE_OK;
}

LUMICE_ErrorCode AnalyzeSurvey(const LUMICE_Scene* scene, const LUMICE_PathSurveyRequest& request, int mc_ess_draws,
                               lumice::raypath::AnalysisWorkspace* workspace, const LUMICE_PathQueryOptions* options,
                               LUMICE_PathSurvey** out, char* err_buf, size_t err_size) {
  namespace rp = lumice::raypath;

  rp::SurveyRequest req;
  req.crystal_id = static_cast<lumice::IdType>(request.crystal_id);
  if (request.crystal_id < 0 || request.crystal_id != static_cast<int>(req.crystal_id)) {
    return Refuse(
        { rp::ErrorCode::kUnknownCrystalId, "no crystal entry with id " + std::to_string(request.crystal_id) }, err_buf,
        err_size);
  }
  if (request.face_count < 0 || request.layer_count < 0) {
    return Refuse({ rp::ErrorCode::kInvalidPath, "negative face or layer count" }, err_buf, err_size);
  }
  int consumed = 0;
  for (int i = 0; i < request.layer_count; i++) {
    const int n = request.layer_face_counts[i];
    if (n < 0 || n > request.face_count - consumed) {
      return Refuse({ rp::ErrorCode::kInvalidPath, "layer face counts do not add up to face_count" }, err_buf,
                    err_size);
    }
    req.path_layers.emplace_back(request.faces + consumed, request.faces + consumed + n);
    consumed += n;
  }
  if (consumed != request.face_count) {
    return Refuse({ rp::ErrorCode::kInvalidPath, "layer face counts do not add up to face_count" }, err_buf, err_size);
  }
  if (request.wavelength_nm > 0.0) {  // false for NaN too: unset
    req.wavelength_nm = request.wavelength_nm;
  }
  req.budget_ms = workspace != nullptr ? (options != nullptr ? options->deadline_ms : 0) :
                                         (request.budget_ms != 0 ? request.budget_ms : req.budget_ms);
  if (request.symmetry_bits_plus_one < 0 || request.symmetry_bits_plus_one > 1) {
    return Refuse({ rp::ErrorCode::kInvalidArgument, "invalid physical symmetry bits" }, err_buf, err_size);
  }
  req.symmetry_bits = request.symmetry_bits_plus_one == 0 ? 7 : 0;
  // The v4.54-and-below callers' structs stop at symmetry_bits_plus_one; the field was
  // extracted by the struct_size gate in the public entry.
  req.mc_ess_draws = mc_ess_draws;

  // The scene document LUMICE_CommitScene would hand the server, through the same parser.
  lumice::ConfigManager config;
  if (workspace == nullptr) {
    if (const lumice::Error err = lumice::ParseConfigManager(SceneRoot(scene), "LUMICE_AnalyzePathSurvey",
                                                             lumice::GetGlobalLogger(), nullptr, &config)) {
      WriteError(err_buf, err_size, "invalid_scene: " + err.message);
      return lumice::capi::ToCApiErrorCode(err.code);
    }
  }
  rp::survey::SurveyResult result;
  const rp::Error error =
      workspace != nullptr ?
          workspace->QuerySurvey(req, &result,
                                 options != nullptr && options->cancel != nullptr ? &options->cancel->token : nullptr) :
          rp::AnalyzePathSurveyValue(config, req, &result);
  if (!error.Ok()) {
    return Refuse(error, err_buf, err_size);
  }
  auto handle = std::make_unique<LUMICE_PathSurvey_>();
  handle->result = std::move(result);
  *out = handle.release();
  return LUMICE_OK;
}

LUMICE_ErrorCode AnalyzeProbe(const LUMICE_Scene* scene, const LUMICE_PathProbeRequest& request,
                              lumice::raypath::AnalysisWorkspace* workspace, const LUMICE_PathQueryOptions* options,
                              LUMICE_PathProbe** out, char* err_buf, size_t err_size) {
  namespace rp = lumice::raypath;

  rp::ProbeRequest req;
  req.crystal_id = static_cast<lumice::IdType>(request.crystal_id);
  if (request.crystal_id < 0 || request.crystal_id != static_cast<int>(req.crystal_id)) {
    return Refuse(
        { rp::ErrorCode::kUnknownCrystalId, "no crystal entry with id " + std::to_string(request.crystal_id) }, err_buf,
        err_size);
  }
  if (request.face_count < 0 || request.layer_count < 0) {
    return Refuse({ rp::ErrorCode::kInvalidPath, "negative face or layer count" }, err_buf, err_size);
  }
  int consumed = 0;
  for (int i = 0; i < request.layer_count; i++) {
    const int n = request.layer_face_counts[i];
    if (n < 0 || n > request.face_count - consumed) {
      return Refuse({ rp::ErrorCode::kInvalidPath, "layer face counts do not add up to face_count" }, err_buf,
                    err_size);
    }
    req.path_layers.emplace_back(request.faces + consumed, request.faces + consumed + n);
    consumed += n;
  }
  if (consumed != request.face_count) {
    return Refuse({ rp::ErrorCode::kInvalidPath, "layer face counts do not add up to face_count" }, err_buf, err_size);
  }
  switch (request.along) {
    case LUMICE_PATH_PROBE_ALONG_RADIAL:
      req.along = rp::ProbeAlong::kRadial;
      break;
    case LUMICE_PATH_PROBE_ALONG_EDGE_NORMAL:
      req.along = rp::ProbeAlong::kEdgeNormal;
      break;
    case LUMICE_PATH_PROBE_ALONG_ARC:
      req.along = rp::ProbeAlong::kArc;
      break;
    case LUMICE_PATH_PROBE_ALONG_TANGENTIAL:
      req.along = rp::ProbeAlong::kTangential;
      break;
    default:
      return Refuse({ rp::ErrorCode::kInvalidArgument, "unknown along form" }, err_buf, err_size);
  }
  req.at_altitude_deg = request.at_altitude_deg;
  req.at_azimuth_deg = request.at_azimuth_deg;
  if (request.wavelength_count < 0) {
    return Refuse({ rp::ErrorCode::kInvalidArgument, "negative wavelength count" }, err_buf, err_size);
  }
  if (request.wavelength_count > rp::kMaxProbeWavelengthCount) {
    return Refuse({ rp::ErrorCode::kInvalidArgument,
                    "at most " + std::to_string(rp::kMaxProbeWavelengthCount) + " wavelengths may be requested" },
                  err_buf, err_size);
  }
  if (request.wavelength_count > 0) {
    req.wavelengths_nm.assign(request.wavelengths_nm, request.wavelengths_nm + request.wavelength_count);
  }
  req.sample_count = request.sample_count;
  if (workspace == nullptr && request.budget_ms < 0) {
    return Refuse({ rp::ErrorCode::kInvalidArgument,
                    "budget_ms must be 0 (the default) or at most " + std::to_string(rp::kMaxProbeBudgetMs) },
                  err_buf, err_size);
  }
  req.budget_ms = workspace != nullptr ? (options != nullptr ? options->deadline_ms : 0) :
                                         (request.budget_ms > 0 ? request.budget_ms : rp::kDefaultProbeBudgetMs);
  if (workspace == nullptr && req.budget_ms > rp::kMaxProbeBudgetMs) {
    return Refuse({ rp::ErrorCode::kInvalidArgument,
                    "budget_ms must be 0 (the default) or at most " + std::to_string(rp::kMaxProbeBudgetMs) },
                  err_buf, err_size);
  }
  req.interrupted = request.interrupted;

  // The scene document LUMICE_CommitScene would hand the server, through the same parser.
  lumice::ConfigManager config;
  if (workspace == nullptr) {
    if (const lumice::Error err = lumice::ParseConfigManager(SceneRoot(scene), "LUMICE_AnalyzePathProbe",
                                                             lumice::GetGlobalLogger(), nullptr, &config)) {
      WriteError(err_buf, err_size, "invalid_scene: " + err.message);
      return lumice::capi::ToCApiErrorCode(err.code);
    }
  }

  if (workspace == nullptr && request.warm_json != nullptr && request.warm_json_len > 0) {
    req.warm_candidate.emplace();
    req.warm_candidate->decode_error =
        rp::DecodeWarmCandidate(std::string(request.warm_json, request.warm_json_len), &*req.warm_candidate);
  } else if (workspace != nullptr && options != nullptr && options->warm != nullptr) {
    req.warm_candidate = options->warm->candidate;
  }

  rp::ProbeResult result;
  const rp::Error error =
      workspace != nullptr ?
          workspace->QueryProbe(req, &result,
                                options != nullptr && options->cancel != nullptr ? &options->cancel->token : nullptr) :
          rp::AnalyzePathProbe(config, req, &result);
  if (!error.Ok()) {
    return Refuse(error, err_buf, err_size);
  }
  auto handle = std::make_unique<LUMICE_PathProbe_>();
  handle->result = std::move(result);
  *out = handle.release();
  return LUMICE_OK;
}

template <typename Request, typename Result, typename Query>
LUMICE_ErrorCode RunWorkspaceQuery(LUMICE_PathWorkspace* workspace, const Request* request,
                                   const LUMICE_PathQueryOptions* options, Result** out, char* err_buf, size_t err_size,
                                   std::initializer_list<size_t> extents, bool accepts_warm, Query query) {
  if (out != nullptr) {
    *out = nullptr;
  }
  if (err_buf != nullptr && err_size > 0) {
    err_buf[0] = '\0';
  }
  if (workspace == nullptr || request == nullptr || out == nullptr) {
    WriteError(err_buf, err_size, "null_arg: workspace, request and out must not be NULL");
    return LUMICE_ERR_NULL_ARG;
  }
  const size_t current = *std::max_element(extents.begin(), extents.end());
  if (request->struct_size < current &&
      std::find(extents.begin(), extents.end(), request->struct_size) == extents.end()) {
    WriteError(err_buf, err_size, "invalid_argument: request struct_size is not a published complete extent");
    return LUMICE_ERR_INVALID_VALUE;
  }
  if ((request->faces == nullptr && request->face_count > 0) ||
      (request->layer_face_counts == nullptr && request->layer_count > 0)) {
    WriteError(err_buf, err_size, "null_arg: path arrays must not be NULL with non-zero counts");
    return LUMICE_ERR_NULL_ARG;
  }
  if (const auto error = ValidateQueryOptions(options, accepts_warm, err_buf, err_size); error != LUMICE_OK) {
    return error;
  }
  try {
    return query();
  } catch (const std::exception& e) {
    LOG_ERROR("LUMICE_PathWorkspaceQuery: {}", e.what());
    WriteError(err_buf, err_size, std::string("internal: ") + e.what());
  } catch (...) {
    LOG_ERROR("LUMICE_PathWorkspaceQuery: unknown exception");
    WriteError(err_buf, err_size, "internal: unknown exception");
  }
  return LUMICE_ERR_UNKNOWN;
}

}  // namespace

LUMICE_ErrorCode LUMICE_AnalyzeSinglePath(const LUMICE_Scene* scene, const LUMICE_SinglePathRequest* request,
                                          LUMICE_SinglePathResult** out, char* err_buf, size_t err_size) {
  if (out != nullptr) {
    *out = nullptr;
  }
  if (err_buf != nullptr && err_size > 0) {
    err_buf[0] = '\0';
  }
  if (scene == nullptr || request == nullptr || out == nullptr) {
    WriteError(err_buf, err_size, "null_arg: scene, request and out must not be NULL");
    return LUMICE_ERR_NULL_ARG;
  }
  if (request->struct_size < kRequestSizeV450) {
    WriteError(err_buf, err_size,
               "invalid_argument: struct_size " + std::to_string(request->struct_size) +
                   " is smaller than this version's LUMICE_SinglePathRequest (" + std::to_string(kRequestSizeV450) +
                   "); set it to sizeof(LUMICE_SinglePathRequest)");
    return LUMICE_ERR_INVALID_VALUE;
  }
  if ((request->faces == nullptr && request->face_count > 0) ||
      (request->layer_face_counts == nullptr && request->layer_count > 0)) {
    WriteError(err_buf, err_size, "null_arg: faces / layer_face_counts must not be NULL with a non-zero count");
    return LUMICE_ERR_NULL_ARG;
  }
  // No exception crosses the C ABI.
  try {
    return Analyze(scene, *request, nullptr, nullptr, out, err_buf, err_size);
  } catch (const std::exception& e) {
    LOG_ERROR("LUMICE_AnalyzeSinglePath: {}", e.what());
    WriteError(err_buf, err_size, std::string("internal: ") + e.what());
  } catch (...) {
    LOG_ERROR("LUMICE_AnalyzeSinglePath: unknown exception");
    WriteError(err_buf, err_size, "internal: unknown exception");
  }
  return LUMICE_ERR_UNKNOWN;
}

LUMICE_ErrorCode LUMICE_SinglePathResultToJson(const LUMICE_SinglePathResult* result, char* out_buf, size_t buf_size,
                                               size_t* out_len) {
  return CopyDerivedJson(
      result, [](const auto& value) { return lumice::raypath::ToJson(value, LUMICE_GetVersionString()); }, out_buf,
      buf_size, out_len);
}

void LUMICE_SinglePathResultDestroy(LUMICE_SinglePathResult* result) {
  std::unique_ptr<LUMICE_SinglePathResult_> owned(result);
}

LUMICE_ErrorCode LUMICE_AnalyzePathFeatureReport(const LUMICE_Scene* scene,
                                                 const LUMICE_PathFeatureReportRequest* request,
                                                 LUMICE_PathFeatureReport** out, char* err_buf, size_t err_size) {
  if (out != nullptr) {
    *out = nullptr;
  }
  if (err_buf != nullptr && err_size > 0) {
    err_buf[0] = '\0';
  }
  if (scene == nullptr || request == nullptr || out == nullptr) {
    WriteError(err_buf, err_size, "null_arg: scene, request and out must not be NULL");
    return LUMICE_ERR_NULL_ARG;
  }
  if (request->struct_size < kFeatureReportRequestSizeV1) {
    WriteError(err_buf, err_size,
               "invalid_argument: struct_size " + std::to_string(request->struct_size) +
                   " is smaller than this version's LUMICE_PathFeatureReportRequest (" +
                   std::to_string(kFeatureReportRequestSizeV1) +
                   "); set it to sizeof(LUMICE_PathFeatureReportRequest)");
    return LUMICE_ERR_INVALID_VALUE;
  }
  if ((request->faces == nullptr && request->face_count > 0) ||
      (request->layer_face_counts == nullptr && request->layer_count > 0) ||
      (request->wavelengths_nm == nullptr && request->wavelength_count > 0)) {
    WriteError(err_buf, err_size,
               "null_arg: faces, layer_face_counts and wavelengths_nm must not be NULL with a non-zero count");
    return LUMICE_ERR_NULL_ARG;
  }
  try {
    return AnalyzeReport(scene, *request, nullptr, nullptr, out, err_buf, err_size);
  } catch (const std::exception& e) {
    LOG_ERROR("LUMICE_AnalyzePathFeatureReport: {}", e.what());
    WriteError(err_buf, err_size, std::string("internal: ") + e.what());
  } catch (...) {
    LOG_ERROR("LUMICE_AnalyzePathFeatureReport: unknown exception");
    WriteError(err_buf, err_size, "internal: unknown exception");
  }
  return LUMICE_ERR_UNKNOWN;
}

LUMICE_ErrorCode LUMICE_PathFeatureReportToJson(const LUMICE_PathFeatureReport* result, char* out_buf, size_t buf_size,
                                                size_t* out_len) {
  return CopyDerivedJson(
      result,
      [](const auto& value) { return lumice::raypath::PathFeatureReportV3ToJson(value, LUMICE_GetVersionString()); },
      out_buf, buf_size, out_len);
}

void LUMICE_PathFeatureReportDestroy(LUMICE_PathFeatureReport* result) {
  std::unique_ptr<LUMICE_PathFeatureReport_> owned(result);
}

LUMICE_ErrorCode LUMICE_AnalyzePathSurvey(const LUMICE_Scene* scene, const LUMICE_PathSurveyRequest* request,
                                          LUMICE_PathSurvey** out, char* err_buf, size_t err_size) {
  if (out != nullptr) {
    *out = nullptr;
  }
  if (err_buf != nullptr && err_size > 0) {
    err_buf[0] = '\0';
  }
  if (scene == nullptr || request == nullptr || out == nullptr) {
    WriteError(err_buf, err_size, "null_arg: scene, request and out must not be NULL");
    return LUMICE_ERR_NULL_ARG;
  }
  if (request->struct_size < kSurveyRequestSizeV1) {
    WriteError(err_buf, err_size,
               "invalid_argument: struct_size " + std::to_string(request->struct_size) +
                   " is smaller than this version's LUMICE_PathSurveyRequest (" + std::to_string(kSurveyRequestSizeV1) +
                   "); set it to sizeof(LUMICE_PathSurveyRequest)");
    return LUMICE_ERR_INVALID_VALUE;
  }
  if ((request->faces == nullptr && request->face_count > 0) ||
      (request->layer_face_counts == nullptr && request->layer_count > 0)) {
    WriteError(err_buf, err_size, "null_arg: faces / layer_face_counts must not be NULL with a non-zero count");
    return LUMICE_ERR_NULL_ARG;
  }
  try {
    // The appended mc_ess_draws reads only when the caller's struct actually carries it — a
    // v4.54-or-older struct_size means the field does not exist there, and the zero default
    // (the leg does not run) is the caller's intent.
    const int mc_ess_draws = request->struct_size >= sizeof(LUMICE_PathSurveyRequest) ? request->mc_ess_draws : 0;
    return AnalyzeSurvey(scene, *request, mc_ess_draws, nullptr, nullptr, out, err_buf, err_size);
  } catch (const std::exception& e) {
    LOG_ERROR("LUMICE_AnalyzePathSurvey: {}", e.what());
    WriteError(err_buf, err_size, std::string("internal: ") + e.what());
  } catch (...) {
    LOG_ERROR("LUMICE_AnalyzePathSurvey: unknown exception");
    WriteError(err_buf, err_size, "internal: unknown exception");
  }
  return LUMICE_ERR_UNKNOWN;
}

LUMICE_ErrorCode LUMICE_PathSurveyToJson(const LUMICE_PathSurvey* result, char* out_buf, size_t buf_size,
                                         size_t* out_len) {
  return CopyDerivedJson(
      result,
      [](const auto& value) { return lumice::raypath::survey::SurveyResultToJson(value, LUMICE_GetVersionString()); },
      out_buf, buf_size, out_len);
}

void LUMICE_PathSurveyDestroy(LUMICE_PathSurvey* result) {
  std::unique_ptr<LUMICE_PathSurvey_> owned(result);
}

LUMICE_ErrorCode LUMICE_AnalyzePathProbe(const LUMICE_Scene* scene, const LUMICE_PathProbeRequest* request,
                                         LUMICE_PathProbe** out, char* err_buf, size_t err_size) {
  if (out != nullptr) {
    *out = nullptr;
  }
  if (err_buf != nullptr && err_size > 0) {
    err_buf[0] = '\0';
  }
  if (scene == nullptr || request == nullptr || out == nullptr) {
    WriteError(err_buf, err_size, "null_arg: scene, request and out must not be NULL");
    return LUMICE_ERR_NULL_ARG;
  }
  if (request->struct_size < sizeof(LUMICE_PathProbeRequest)) {
    WriteError(err_buf, err_size,
               "invalid_argument: struct_size " + std::to_string(request->struct_size) +
                   " is smaller than this version's LUMICE_PathProbeRequest (" +
                   std::to_string(sizeof(LUMICE_PathProbeRequest)) + "); set it to sizeof(LUMICE_PathProbeRequest)");
    return LUMICE_ERR_INVALID_VALUE;
  }
  if ((request->faces == nullptr && request->face_count > 0) ||
      (request->layer_face_counts == nullptr && request->layer_count > 0) ||
      (request->wavelengths_nm == nullptr && request->wavelength_count > 0)) {
    WriteError(err_buf, err_size,
               "null_arg: faces, layer_face_counts and wavelengths_nm must not be NULL with a non-zero count");
    return LUMICE_ERR_NULL_ARG;
  }
  try {
    return AnalyzeProbe(scene, *request, nullptr, nullptr, out, err_buf, err_size);
  } catch (const std::exception& e) {
    LOG_ERROR("LUMICE_AnalyzePathProbe: {}", e.what());
    WriteError(err_buf, err_size, std::string("internal: ") + e.what());
  } catch (...) {
    LOG_ERROR("LUMICE_AnalyzePathProbe: unknown exception");
    WriteError(err_buf, err_size, "internal: unknown exception");
  }
  return LUMICE_ERR_UNKNOWN;
}

LUMICE_ErrorCode LUMICE_PathProbeToJson(const LUMICE_PathProbe* result, char* out_buf, size_t buf_size,
                                        size_t* out_len) {
  return CopyDerivedJson(
      result, [](const auto& value) { return lumice::raypath::ProbeToJson(value, LUMICE_GetVersionString()); }, out_buf,
      buf_size, out_len);
}

void LUMICE_PathProbeDestroy(LUMICE_PathProbe* result) {
  std::unique_ptr<LUMICE_PathProbe_> owned(result);
}

LUMICE_ErrorCode LUMICE_PathWorkspaceCreate(const LUMICE_Scene* scene, LUMICE_PathWorkspace** out, char* err_buf,
                                            size_t err_size) {
  if (out != nullptr) {
    *out = nullptr;
  }
  if (scene == nullptr || out == nullptr) {
    WriteError(err_buf, err_size, "null_arg: scene and out must not be NULL");
    return LUMICE_ERR_NULL_ARG;
  }
  try {
    lumice::ConfigManager config;
    if (const lumice::Error err = lumice::ParseConfigManager(SceneRoot(scene), "LUMICE_PathWorkspaceCreate",
                                                             lumice::GetGlobalLogger(), nullptr, &config)) {
      WriteError(err_buf, err_size, "invalid_scene: " + err.message);
      return lumice::capi::ToCApiErrorCode(err.code);
    }
    auto handle = std::make_unique<LUMICE_PathWorkspace_>();
    if (const auto error = lumice::raypath::AnalysisWorkspace::Create(config, &handle->workspace); !error.Ok()) {
      return Refuse(error, err_buf, err_size);
    }
    *out = handle.release();
    return LUMICE_OK;
  } catch (const std::exception& e) {
    LOG_ERROR("LUMICE_PathWorkspaceCreate: {}", e.what());
    WriteError(err_buf, err_size, std::string("internal: ") + e.what());
    return LUMICE_ERR_UNKNOWN;
  } catch (...) {
    LOG_ERROR("LUMICE_PathWorkspaceCreate: unknown exception");
    WriteError(err_buf, err_size, "internal: unknown exception");
    return LUMICE_ERR_UNKNOWN;
  }
}

void LUMICE_PathWorkspaceDestroy(LUMICE_PathWorkspace* workspace) {
  std::unique_ptr<LUMICE_PathWorkspace_> owned(workspace);
}

LUMICE_ErrorCode LUMICE_PathCancelTokenCreate(LUMICE_PathCancelToken** out) {
  if (out == nullptr) {
    return LUMICE_ERR_NULL_ARG;
  }
  *out = nullptr;
  try {
    *out = std::make_unique<LUMICE_PathCancelToken_>().release();
    return LUMICE_OK;
  } catch (...) {
    return LUMICE_ERR_UNKNOWN;
  }
}

void LUMICE_PathCancelTokenRequest(LUMICE_PathCancelToken* token) {
  if (token != nullptr) {
    token->token.RequestCancel();
  }
}

void LUMICE_PathCancelTokenDestroy(LUMICE_PathCancelToken* token) {
  std::unique_ptr<LUMICE_PathCancelToken_> owned(token);
}

LUMICE_ErrorCode LUMICE_PathWarmDataFromSurvey(const LUMICE_PathSurvey* result, LUMICE_PathWarmData** out) {
  if (out != nullptr) {
    *out = nullptr;
  }
  if (result == nullptr || out == nullptr) {
    return LUMICE_ERR_NULL_ARG;
  }
  try {
    auto handle = std::make_unique<LUMICE_PathWarmData_>();
    handle->candidate = lumice::raypath::HarvestSurveyResult(result->result);
    *out = handle.release();
    return LUMICE_OK;
  } catch (...) {
    return LUMICE_ERR_UNKNOWN;
  }
}

LUMICE_ErrorCode LUMICE_PathWarmDataFromPoint(const LUMICE_SinglePathResult* result, LUMICE_PathWarmData** out) {
  if (out != nullptr) {
    *out = nullptr;
  }
  if (result == nullptr || out == nullptr) {
    return LUMICE_ERR_NULL_ARG;
  }
  try {
    auto handle = std::make_unique<LUMICE_PathWarmData_>();
    const auto& data = result->result.Data();
    if (data.OutcomeValue() == lumice::raypath::Outcome::kDiscovered) {
      for (const auto& component : data.Discovered().components) {
        handle->candidate.seeds.insert(handle->candidate.seeds.end(), std::begin(component.seed),
                                       std::end(component.seed));
      }
      for (const auto& incomplete : data.Discovered().incomplete) {
        handle->candidate.seeds.insert(handle->candidate.seeds.end(), std::begin(incomplete.seed),
                                       std::end(incomplete.seed));
      }
    }
    *out = handle.release();
    return LUMICE_OK;
  } catch (...) {
    return LUMICE_ERR_UNKNOWN;
  }
}

LUMICE_ErrorCode LUMICE_PathWarmDataFromJson(const char* utf8, size_t length, LUMICE_PathWarmData** out, char* err_buf,
                                             size_t err_size) {
  if (out != nullptr) {
    *out = nullptr;
  }
  if (utf8 == nullptr || out == nullptr) {
    WriteError(err_buf, err_size, "null_arg: document and out must not be NULL");
    return LUMICE_ERR_NULL_ARG;
  }
  try {
    auto handle = std::make_unique<LUMICE_PathWarmData_>();
    if (const auto error = lumice::raypath::DecodeWarmCandidate(std::string(utf8, length), &handle->candidate);
        !error.Ok()) {
      return Refuse(error, err_buf, err_size);
    }
    *out = handle.release();
    return LUMICE_OK;
  } catch (const std::exception& e) {
    LOG_ERROR("LUMICE_PathWarmDataFromJson: {}", e.what());
    WriteError(err_buf, err_size, std::string("internal: ") + e.what());
    return LUMICE_ERR_UNKNOWN;
  } catch (...) {
    LOG_ERROR("LUMICE_PathWarmDataFromJson: unknown exception");
    WriteError(err_buf, err_size, "internal: unknown exception");
    return LUMICE_ERR_UNKNOWN;
  }
}

void LUMICE_PathWarmDataDestroy(LUMICE_PathWarmData* warm) {
  std::unique_ptr<LUMICE_PathWarmData_> owned(warm);
}

LUMICE_ErrorCode LUMICE_PathWorkspaceQueryPoint(LUMICE_PathWorkspace* workspace,
                                                const LUMICE_SinglePathRequest* request,
                                                const LUMICE_PathQueryOptions* options, LUMICE_SinglePathResult** out,
                                                char* err_buf, size_t err_size) {
  return RunWorkspaceQuery(
      workspace, request, options, out, err_buf, err_size, { kRequestSizeV450, kRequestSizeV452 }, true, [&] {
        if (const auto error = ValidateWarm(request->warm_json, request->warm_json_len, options, err_buf, err_size);
            error != LUMICE_OK) {
          return error;
        }
        return Analyze(nullptr, *request, workspace->workspace.get(), options, out, err_buf, err_size);
      });
}

LUMICE_ErrorCode LUMICE_PathWorkspaceQueryProbe(LUMICE_PathWorkspace* workspace, const LUMICE_PathProbeRequest* request,
                                                const LUMICE_PathQueryOptions* options, LUMICE_PathProbe** out,
                                                char* err_buf, size_t err_size) {
  return RunWorkspaceQuery(
      workspace, request, options, out, err_buf, err_size, { sizeof(LUMICE_PathProbeRequest) }, true, [&] {
        if ((request->wavelengths_nm == nullptr && request->wavelength_count > 0)) {
          WriteError(err_buf, err_size, "null_arg: wavelengths_nm is NULL with non-zero count");
          return LUMICE_ERR_NULL_ARG;
        }
        if (const auto error = ValidateWarm(request->warm_json, request->warm_json_len, options, err_buf, err_size);
            error != LUMICE_OK) {
          return error;
        }
        return AnalyzeProbe(nullptr, *request, workspace->workspace.get(), options, out, err_buf, err_size);
      });
}

LUMICE_ErrorCode LUMICE_PathWorkspaceQuerySurvey(LUMICE_PathWorkspace* workspace,
                                                 const LUMICE_PathSurveyRequest* request,
                                                 const LUMICE_PathQueryOptions* options, LUMICE_PathSurvey** out,
                                                 char* err_buf, size_t err_size) {
  return RunWorkspaceQuery(
      workspace, request, options, out, err_buf, err_size, { kSurveyRequestSizeV1, sizeof(LUMICE_PathSurveyRequest) },
      false, [&] {
        const int draws = request->struct_size >= sizeof(LUMICE_PathSurveyRequest) ? request->mc_ess_draws : 0;
        return AnalyzeSurvey(nullptr, *request, draws, workspace->workspace.get(), options, out, err_buf, err_size);
      });
}

LUMICE_ErrorCode LUMICE_PathWorkspaceQueryReport(LUMICE_PathWorkspace* workspace,
                                                 const LUMICE_PathFeatureReportRequest* request,
                                                 const LUMICE_PathQueryOptions* options, LUMICE_PathFeatureReport** out,
                                                 char* err_buf, size_t err_size) {
  return RunWorkspaceQuery(workspace, request, options, out, err_buf, err_size,
                           { kFeatureReportRequestSizeV1, sizeof(LUMICE_PathFeatureReportRequest) }, false, [&] {
                             if (request->wavelengths_nm == nullptr && request->wavelength_count > 0) {
                               WriteError(err_buf, err_size, "null_arg: wavelengths_nm is NULL with non-zero count");
                               return LUMICE_ERR_NULL_ARG;
                             }
                             return AnalyzeReport(nullptr, *request, workspace->workspace.get(), options, out, err_buf,
                                                  err_size);
                           });
}
