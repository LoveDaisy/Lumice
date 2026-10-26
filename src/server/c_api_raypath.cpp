// C API bridge for lumice_raypath.h. Opaque handles own completed raypath values; JSON is
// derived from those values and never feeds the query or a typed reader.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "analytic/dp_chromatic.hpp"
#include "config/config_manager.hpp"
#include "config/crystal_config.hpp"
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

static_assert(LUMICE_PATH_QUANTITY_BAND_SUM == static_cast<int>(lumice::raypath::QuantityKind::kBandSum) + 1);
static_assert(LUMICE_PATH_QUANTITY_TINT_RATIO == static_cast<int>(lumice::raypath::QuantityKind::kTintRatio) + 1);
static_assert(LUMICE_PATH_QUANTITY_TIR_SHARE == static_cast<int>(lumice::raypath::QuantityKind::kTirShare) + 1);
static_assert(LUMICE_PATH_QUANTITY_MC_ESS == static_cast<int>(lumice::raypath::QuantityKind::kMcEss) + 1);
static_assert(LUMICE_PATH_QUANTITY_WITNESS_WEIGHT ==
              static_cast<int>(lumice::raypath::QuantityKind::kWitnessWeight) + 1);
static_assert(LUMICE_PATH_QUANTITY_ENTRY_AREA == static_cast<int>(lumice::raypath::QuantityKind::kEntryArea) + 1);
static_assert(LUMICE_PATH_QUANTITY_TRANSMISSION == static_cast<int>(lumice::raypath::QuantityKind::kTransmission) + 1);
static_assert(LUMICE_PATH_QUANTITY_POSE_DENSITY == static_cast<int>(lumice::raypath::QuantityKind::kPoseDensity) + 1);
static_assert(LUMICE_PATH_QUANTITY_NORMAL_JACOBIAN ==
              static_cast<int>(lumice::raypath::QuantityKind::kNormalJacobian) + 1);
static_assert(LUMICE_PATH_QUANTITY_COAREA_FACTOR == static_cast<int>(lumice::raypath::QuantityKind::kCoareaFactor) + 1);
static_assert(LUMICE_PATH_QUANTITY_FACTOR_PRODUCT ==
              static_cast<int>(lumice::raypath::QuantityKind::kFactorProduct) + 1);
static_assert(LUMICE_PATH_QUANTITY_FIBER_INTEGRAL ==
              static_cast<int>(lumice::raypath::QuantityKind::kFiberIntegral) + 1);
static_assert(LUMICE_PATH_QUANTITY_ORBIT_INTEGRAL ==
              static_cast<int>(lumice::raypath::QuantityKind::kOrbitIntegral) + 1);
static_assert(LUMICE_PATH_METHOD_BAND_SUM_LATTICE ==
              static_cast<int>(lumice::raypath::QuantityMethod::kBandSumLattice) + 1);
static_assert(LUMICE_PATH_METHOD_POINT_MASS_PSI_AVERAGE ==
              static_cast<int>(lumice::raypath::QuantityMethod::kPointMassPsiAverage) + 1);
static_assert(LUMICE_PATH_METHOD_POINT_MASS_LATTICE_MEAN ==
              static_cast<int>(lumice::raypath::QuantityMethod::kPointMassLatticeMean) + 1);
static_assert(LUMICE_PATH_METHOD_CLASS_LATTICE_POWER_SUM ==
              static_cast<int>(lumice::raypath::QuantityMethod::kClassLatticePowerSum) + 1);
static_assert(LUMICE_PATH_METHOD_SINGLE_POSE_DIRECT ==
              static_cast<int>(lumice::raypath::QuantityMethod::kSinglePoseDirect) + 1);
static_assert(LUMICE_PATH_METHOD_POINT_MASS_BAND_SUM ==
              static_cast<int>(lumice::raypath::QuantityMethod::kPointMassBandSum) + 1);
static_assert(LUMICE_PATH_METHOD_DIAGNOSTIC_TIR_LATTICE ==
              static_cast<int>(lumice::raypath::QuantityMethod::kDiagnosticTirLattice) + 1);
static_assert(LUMICE_PATH_METHOD_KISH_OUTER_DRAW ==
              static_cast<int>(lumice::raypath::QuantityMethod::kKishOuterDraw) + 1);
static_assert(LUMICE_PATH_METHOD_LATTICE_ARGMAX_DIAGNOSTIC ==
              static_cast<int>(lumice::raypath::QuantityMethod::kLatticeArgmaxDiagnostic) + 1);
static_assert(LUMICE_PATH_METHOD_ENTRY_MEASURE == static_cast<int>(lumice::raypath::QuantityMethod::kEntryMeasure) + 1);
static_assert(LUMICE_PATH_METHOD_INTERFACE_PRODUCT ==
              static_cast<int>(lumice::raypath::QuantityMethod::kInterfaceProduct) + 1);
static_assert(LUMICE_PATH_METHOD_DECLARED_POSE_DENSITY ==
              static_cast<int>(lumice::raypath::QuantityMethod::kDeclaredPoseDensity) + 1);
static_assert(LUMICE_PATH_METHOD_NORMAL_JACOBIAN ==
              static_cast<int>(lumice::raypath::QuantityMethod::kNormalJacobian) + 1);
static_assert(LUMICE_PATH_METHOD_REGULARIZED_COAREA ==
              static_cast<int>(lumice::raypath::QuantityMethod::kRegularizedCoarea) + 1);
static_assert(LUMICE_PATH_METHOD_FACTOR_PRODUCT ==
              static_cast<int>(lumice::raypath::QuantityMethod::kFactorProduct) + 1);
static_assert(LUMICE_PATH_METHOD_FIBER_ARCLENGTH ==
              static_cast<int>(lumice::raypath::QuantityMethod::kFiberArclength) + 1);
static_assert(LUMICE_PATH_METHOD_ORBIT_MIDPOINT ==
              static_cast<int>(lumice::raypath::QuantityMethod::kOrbitMidpoint) + 1);

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

namespace {

template <typename T>
LUMICE_PathReadStatus BeginRead(const void* result, T* out) {
  if (out == nullptr) {
    return LUMICE_PATH_READ_NULL_ARG;
  }
  const size_t caller_size = out->struct_size;
  if (caller_size < sizeof(T)) {
    return LUMICE_PATH_READ_INVALID_SIZE;
  }
  T cleared{};
  cleared.struct_size = caller_size;
  *out = cleared;
  return result == nullptr ? LUMICE_PATH_READ_NULL_ARG : LUMICE_PATH_READ_OK;
}

LUMICE_PathText Text(const std::string& value) {
  return { value.c_str(), value.size() };
}
LUMICE_PathText Text(const char* value) {
  return { value, std::strlen(value) };
}

int ExecutionTag(lumice::raypath::ExecutionOutcome outcome) {
  using E = lumice::raypath::ExecutionOutcome;
  switch (outcome) {
    case E::kCompleted:
      return LUMICE_PATH_OUTCOME_COMPLETED;
    case E::kCancelled:
      return LUMICE_PATH_OUTCOME_CANCELLED;
    case E::kDeadlineExceeded:
      return LUMICE_PATH_OUTCOME_DEADLINE_EXCEEDED;
  }
  return 0;
}

int EvidenceTag(lumice::raypath::survey::EvidenceState state) {
  using S = lumice::raypath::survey::EvidenceState;
  switch (state) {
    case S::kCertified:
      return LUMICE_PATH_STATE_CERTIFIED;
    case S::kFinite:
      return LUMICE_PATH_STATE_FINITE;
    case S::kRefused:
      return LUMICE_PATH_STATE_REFUSED;
    case S::kUnevaluated:
      return LUMICE_PATH_STATE_UNEVALUATED;
  }
  return 0;
}

int SurveyKindTag(lumice::raypath::survey::SurveyObjectKind kind) {
  using K = lumice::raypath::survey::SurveyObjectKind;
  switch (kind) {
    case K::kSourceImage:
      return LUMICE_PATH_SURVEY_SOURCE_IMAGE;
    case K::kSupportSet:
      return LUMICE_PATH_SURVEY_SUPPORT_SET;
    case K::kCausticEdge:
      return LUMICE_PATH_SURVEY_CAUSTIC_EDGE;
    case K::kBrightDomain:
      return LUMICE_PATH_SURVEY_BRIGHT_DOMAIN;
    case K::kFamilyCollapsePoint:
      return LUMICE_PATH_SURVEY_FAMILY_COLLAPSE;
    case K::kRegularImage:
      return LUMICE_PATH_SURVEY_REGULAR_IMAGE;
  }
  return 0;
}

void FillEvidence(const lumice::raypath::survey::SurveyEvidence& evidence, LUMICE_PathSurveyEvidence* out) {
  out->state = EvidenceTag(evidence.state);
  switch (evidence.state) {
    case lumice::raypath::survey::EvidenceState::kCertified:
      out->method = Text(evidence.method);
      break;
    case lumice::raypath::survey::EvidenceState::kFinite:
      out->coverage_available = 1;
      out->members_evaluated = evidence.coverage.members_evaluated;
      out->sample_count = evidence.coverage.sample_count;
      out->geometry_basis = Text(evidence.coverage.geometry_basis);
      out->wavelength_nm = evidence.coverage.wavelength_nm;
      break;
    case lumice::raypath::survey::EvidenceState::kRefused:
      out->mode = Text(evidence.mode);
      break;
    case lumice::raypath::survey::EvidenceState::kUnevaluated:
      out->reason = Text(evidence.reason);
      break;
  }
}

template <typename Claimed>
void FillClaimEvidence(const Claimed& claim, LUMICE_PathSurveyEvidence* out) {
  out->state = EvidenceTag(claim.State());
  switch (claim.State()) {
    case lumice::raypath::survey::EvidenceState::kCertified:
      out->method = Text(claim.Method());
      break;
    case lumice::raypath::survey::EvidenceState::kFinite: {
      const auto& coverage = claim.Coverage();
      out->coverage_available = 1;
      out->members_evaluated = coverage.members_evaluated;
      out->sample_count = coverage.sample_count;
      out->geometry_basis = Text(coverage.geometry_basis);
      out->wavelength_nm = coverage.wavelength_nm;
      break;
    }
    case lumice::raypath::survey::EvidenceState::kRefused:
      out->mode = Text(claim.Mode());
      break;
    case lumice::raypath::survey::EvidenceState::kUnevaluated:
      out->reason = Text(claim.Reason());
      break;
  }
}

int FamilyKindTag(lumice::raypath::structure::FamilyImageKind kind) {
  using K = lumice::raypath::structure::FamilyImageKind;
  switch (kind) {
    case K::kCollapsePoint:
      return LUMICE_PATH_SURVEY_FAMILY_COLLAPSE;
    case K::kCausticEdge:
      return LUMICE_PATH_SURVEY_CAUSTIC_EDGE;
    case K::kRegularImage:
      return LUMICE_PATH_SURVEY_REGULAR_IMAGE;
    case K::kBrightDomain:
      return LUMICE_PATH_SURVEY_BRIGHT_DOMAIN;
  }
  return 0;
}

}  // namespace

namespace {

void FillShape(const lumice::raypath::NominalShapeScalar& shape, LUMICE_PathShapeScalar* out) {
  out->name = Text(shape.name);
  out->value = shape.value;
  out->distribution = Text(lumice::raypath::DistributionName(shape.distribution));
  out->spread = shape.spread;
}

void FillContext(const lumice::raypath::SurveyContextBlock& context, LUMICE_PathContext* out) {
  out->source = context.source == lumice::raypath::ContextSource::kWarm ? 1 : 2;
  out->warm_index_absent = context.warm_index_absent;
  out->reference_wavelength_nm = context.reference_wavelength_nm;
  out->object_state = EvidenceTag(context.object.state);
  if (out->object_state == LUMICE_PATH_STATE_REFUSED || out->object_state == LUMICE_PATH_STATE_UNEVALUATED) {
    out->object_reason = Text(context.object.reason);
  }
  out->object_kind_available = context.object_kind.has_value();
  if (context.object_kind) {
    out->object_kind = SurveyKindTag(*context.object_kind);
  }
  out->target_inside_support = context.target_inside_support;
  out->object_members = context.object_members.data();
  out->object_member_count = context.object_members.size();
  if (out->object_state == LUMICE_PATH_STATE_FINITE || out->object_state == LUMICE_PATH_STATE_CERTIFIED) {
    out->object_distance_deg = context.object_distance_deg;
  }
  out->edge_state = EvidenceTag(context.edge.state);
  if (out->edge_state == LUMICE_PATH_STATE_REFUSED || out->edge_state == LUMICE_PATH_STATE_UNEVALUATED) {
    out->edge_reason = Text(context.edge.reason);
  }
  if (out->edge_state == LUMICE_PATH_STATE_FINITE || out->edge_state == LUMICE_PATH_STATE_CERTIFIED) {
    out->edge_distance_deg = context.edge_distance_deg;
    out->edge_basis = context.edge_basis == lumice::raypath::ContextEdgeBasis::kSupportIntervalEndpoint ? 1 : 2;
  }
  out->band_sum_state = EvidenceTag(context.band_sum.state);
  if (out->band_sum_state == LUMICE_PATH_STATE_REFUSED || out->band_sum_state == LUMICE_PATH_STATE_UNEVALUATED) {
    out->band_sum_reason = Text(context.band_sum.reason);
  }
  if (out->band_sum_state == LUMICE_PATH_STATE_FINITE || out->band_sum_state == LUMICE_PATH_STATE_CERTIFIED) {
    out->band_sum_value = context.band_sum_value;
    out->band_sum_delta_deg = context.band_sum_delta_deg;
    out->band_sum_bin_deg = context.band_sum_bin_deg;
    out->band_sum_caliber = Text(context.band_sum_caliber);
  }
}

void FillQuantity(const lumice::raypath::QuantitySpec& spec, const lumice::raypath::ClaimScope* scope,
                  LUMICE_PathQuantitySpec* out) {
  out->kind = static_cast<int>(spec.kind) + 1;
  out->method = static_cast<int>(spec.method) + 1;
  out->unit = static_cast<int>(spec.unit) + 1;
  out->aggregation = static_cast<int>(spec.aggregation) + 1;
  out->measure = static_cast<int>(spec.measure) + 1;
  out->reference = static_cast<int>(spec.reference) + 1;
  out->density = static_cast<int>(spec.density) + 1;
  out->spectrum = static_cast<int>(spec.spectrum) + 1;
  out->source = static_cast<int>(spec.source) + 1;
  out->shape = static_cast<int>(spec.shape) + 1;
  out->wavelength_available = spec.observation.wavelength_nm.has_value();
  if (spec.observation.wavelength_nm)
    out->wavelength_nm = *spec.observation.wavelength_nm;
  out->sun_altitude_available = spec.observation.sun_altitude_deg.has_value();
  if (spec.observation.sun_altitude_deg)
    out->sun_altitude_deg = *spec.observation.sun_altitude_deg;
  out->sun_azimuth_available = spec.observation.sun_azimuth_deg.has_value();
  if (spec.observation.sun_azimuth_deg)
    out->sun_azimuth_deg = *spec.observation.sun_azimuth_deg;
  out->wavelengths_nm = spec.wavelengths_nm.data();
  out->wavelength_count = spec.wavelengths_nm.size();
  out->regularization_epsilon = spec.regularization_epsilon;
  if (scope) {
    out->scope_available = 1;
    out->scope_member_kind = static_cast<int>(scope->members.member_kind) + 1;
    out->scope_location = static_cast<int>(scope->members.location) + 1;
    if (scope->members.location == lumice::raypath::SelectionLocation::kSceneLayer) {
      out->scope_layer_index = scope->members.layer_index;
    }
    out->scope_member_count = scope->members.members.size();
    out->scope_geometry = static_cast<int>(scope->geometry.kind) + 1;
    out->scope_measure = static_cast<int>(scope->measure.kind) + 1;
    out->scope_reference = static_cast<int>(scope->measure.reference) + 1;
  }
}

}  // namespace

LUMICE_PathReadStatus LUMICE_PathProbeGetQuantity(const LUMICE_PathProbe* result, size_t profile,
                                                  LUMICE_PathQuantitySpec* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& profiles = result->result.Data().profiles;
  if (profile >= profiles.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  FillQuantity(profiles[profile].quantity, &profiles[profile].Scope(), out);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetProbeQuantity(const LUMICE_PathSurvey* result, size_t probe,
                                                        LUMICE_PathQuantitySpec* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& probes = result->result.Data().probes;
  if (probe >= probes.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  FillQuantity(probes[probe].spec.quantity, &probes[probe].Scope(), out);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetObjectQuantity(const LUMICE_PathSurvey* result, size_t object,
                                                         LUMICE_PathQuantitySpec* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& objects = result->result.Data().objects;
  if (object >= objects.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  const auto& scope = std::visit([](const auto& arm) -> const lumice::raypath::ClaimScope& { return arm.Scope(); },
                                 objects[object].Declaration());
  if (!scope.quantity)
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  FillQuantity(*scope.quantity, &scope, out);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetTintQuantity(const LUMICE_PathSurvey* result, LUMICE_PathQuantitySpec* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& tint = result->result.Data().weight_tint;
  if (!tint || !tint->Scope().quantity)
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  FillQuantity(*tint->Scope().quantity, &tint->Scope(), out);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetTirQuantity(const LUMICE_PathSurvey* result, LUMICE_PathQuantitySpec* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& tir = result->result.Data().tir_share;
  if (!tir || !tir->Scope().quantity)
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  FillQuantity(*tir->Scope().quantity, &tir->Scope(), out);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetEssQuantity(const LUMICE_PathSurvey* result, LUMICE_PathQuantitySpec* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& ess = result->result.Data().mc_region_ess;
  if (!ess || !ess->Scope().quantity)
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  FillQuantity(*ess->Scope().quantity, &ess->Scope(), out);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetWitnessQuantity(const LUMICE_PathSurvey* result, size_t object,
                                                          LUMICE_PathQuantitySpec* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& objects = result->result.Data().objects;
  if (object >= objects.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  if (objects[object].Kind() != lumice::raypath::survey::SurveyObjectKind::kSupportSet) {
    return LUMICE_PATH_READ_INVALID_KIND;
  }
  const auto* witness = objects[object].Witness();
  if (!witness || !witness->Scope().quantity)
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  FillQuantity(*witness->Scope().quantity, &witness->Scope(), out);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathPointGetShape(const LUMICE_SinglePathResult* result, size_t index,
                                               LUMICE_PathShapeScalar* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& shapes = result->result.Data().meta.shape;
  if (index >= shapes.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  FillShape(shapes[index], out);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathProbeGetShape(const LUMICE_PathProbe* result, size_t index,
                                               LUMICE_PathShapeScalar* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& shapes = result->result.Data().meta.shape;
  if (index >= shapes.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  FillShape(shapes[index], out);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathPointGetMass(const LUMICE_SinglePathResult* result, LUMICE_PathPointMass* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& data = result->result.Data();
  if (data.OutcomeValue() != lumice::raypath::Outcome::kPointMass) {
    return LUMICE_PATH_READ_INVALID_KIND;
  }
  const auto& mass = data.Mass();
  std::copy_n(mass.direction, 3, out->direction);
  out->altitude_deg = mass.altitude_deg;
  out->azimuth_deg = mass.azimuth_deg;
  out->target_separation_deg = mass.target_separation_deg;
  const auto& orbit = mass.orbit_factors;
  out->orbit_available = orbit.available;
  out->orbit_reason = Text(orbit.reason);
  out->measure_support = Text(orbit.measure_support);
  if (orbit.available) {
    out->orbit_sample_count = orbit.sample_count;
    out->orbit_integral = orbit.integral;
    out->orbit_kept_count = orbit.kept_count;
    out->orbit_point_count = orbit.points.size();
  }
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathPointGetOrbitPoint(const LUMICE_SinglePathResult* result, size_t index,
                                                    LUMICE_PathOrbitPoint* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& data = result->result.Data();
  if (data.OutcomeValue() != lumice::raypath::Outcome::kPointMass) {
    return LUMICE_PATH_READ_INVALID_KIND;
  }
  const auto& orbit = data.Mass().orbit_factors;
  if (!orbit.available) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  if (index >= orbit.points.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& point = orbit.points[index];
  out->theta_rad = point.theta_rad;
  out->rho_theta = point.rho_theta;
  out->entry_measure_a2 = point.entry_measure_a2;
  out->total_transmission = point.total_transmission;
  out->valid = point.valid;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathPointGetDiscovery(const LUMICE_SinglePathResult* result,
                                                   LUMICE_PathPointDiscovery* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& data = result->result.Data();
  if (data.OutcomeValue() != lumice::raypath::Outcome::kDiscovered) {
    return LUMICE_PATH_READ_INVALID_KIND;
  }
  const auto& d = data.Discovered().discovery;
  out->complete = d.complete;
  out->pool_count = d.pool_count;
  out->extra_seed_count = d.extra_seed_count;
  out->raw_cluster_count = d.raw_cluster_count;
  out->admissible_count = d.admissible_count;
  out->dedup_merged = d.dedup_merged;
  out->arc_stitched = d.arc_stitched;
  out->arc_backward_failed = d.arc_backward_failed;
  out->arc_backward_closed_anomaly = d.arc_backward_closed_anomaly;
  out->incomplete_unnamed_event = d.incomplete_unnamed_event;
  out->incomplete_not_converged = d.incomplete_not_converged;
  out->completeness_note = Text(lumice::raypath::kCompletenessNote);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathPointGetReach(const LUMICE_SinglePathResult* result, LUMICE_PathPointReach* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& data = result->result.Data();
  if (data.OutcomeValue() != lumice::raypath::Outcome::kDiscovered) {
    return LUMICE_PATH_READ_INVALID_KIND;
  }
  const auto& reach = data.Discovered().reach;
  out->target_in_range = reach.target_in_range;
  out->probe_lat_count = reach.probe_lat_count;
  out->deviation_min_rad = reach.deviation_min_rad;
  out->deviation_max_rad = reach.deviation_max_rad;
  out->target_deviation_rad = reach.target_deviation_rad;
  out->tolerance_rad = reach.tolerance_rad;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathPointGetMeasureRegistration(const LUMICE_SinglePathResult* result,
                                                             LUMICE_PathMeasureRegistration* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& data = result->result.Data();
  if (data.OutcomeValue() != lumice::raypath::Outcome::kDiscovered) {
    return LUMICE_PATH_READ_INVALID_KIND;
  }
  const auto& registration = data.Discovered().measure_registration;
  out->support_kind = Text(registration.support_kind);
  out->fast_path = registration.fast_path;
  out->registered_gap = registration.registered_gap;
  out->note = Text(registration.note);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathPointGetIncomplete(const LUMICE_SinglePathResult* result, size_t index,
                                                    LUMICE_PathIncomplete* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& data = result->result.Data();
  if (data.OutcomeValue() != lumice::raypath::Outcome::kDiscovered) {
    return LUMICE_PATH_READ_INVALID_KIND;
  }
  const auto& rows = data.Discovered().incomplete;
  if (index >= rows.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& row = rows[index];
  out->cause = Text(lumice::raypath::IncompleteCauseName(row.cause));
  std::copy_n(row.seed, 9, out->seed);
  out->forward_status = Text(lumice::raypath::TraceStatusName(row.forward.status));
  out->forward_reason = Text(lumice::raypath::TraceReasonName(row.forward.reason));
  out->forward_pose_count = row.forward.pose_count;
  if (row.backward.pose_count > 0) {
    out->backward_status = Text(lumice::raypath::TraceStatusName(row.backward.status));
    out->backward_reason = Text(lumice::raypath::TraceReasonName(row.backward.reason));
    out->backward_pose_count = row.backward.pose_count;
  }
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathPointGetContext(const LUMICE_SinglePathResult* result, LUMICE_PathContext* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  FillContext(result->result.Data().survey_context, out);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathProbeGetContext(const LUMICE_PathProbe* result, LUMICE_PathContext* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  FillContext(result->result.Data().context, out);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathProbeGetWavelength(const LUMICE_PathProbe* result, size_t index,
                                                    LUMICE_PathProbeWavelength* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& wavelengths = result->result.Data().meta.wavelengths;
  if (index >= wavelengths.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  out->nm = wavelengths[index].nm;
  out->source = Text(lumice::raypath::WavelengthSourceName(wavelengths[index].source));
  out->refractive_index = wavelengths[index].refractive_index;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathPointGetInfo(const LUMICE_SinglePathResult* result, LUMICE_PathPointInfo* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& data = result->result.Data();
  const auto& m = data.meta;
  out->schema_version = m.schema_version;
  out->outcome = data.OutcomeValue() == lumice::raypath::Outcome::kPointMass ? LUMICE_PATH_POINT_MASS :
                                                                               LUMICE_PATH_POINT_DISCOVERED;
  out->execution = ExecutionTag(m.execution);
  out->crystal_id = static_cast<int>(m.crystal_id);
  out->analytic_api_version = m.analytic_api_version;
  out->crystal_kind = Text(m.crystal_kind);
  out->faces = m.faces.data();
  out->face_count = m.faces.size();
  out->shape_count = m.shape.size();
  out->shape_is_nominal = m.shape_is_nominal;
  out->upper_wedge_deg = m.upper_wedge_deg;
  out->lower_wedge_deg = m.lower_wedge_deg;
  out->sun_altitude_deg = m.sun_altitude_deg;
  out->sun_azimuth_deg = m.sun_azimuth_deg;
  out->sun_diameter_deg = m.sun_diameter_deg;
  std::copy_n(m.incident_direction, 3, out->incident_direction);
  out->target_altitude_deg = m.target_altitude_deg;
  out->target_azimuth_deg = m.target_azimuth_deg;
  std::copy_n(m.target_direction, 3, out->target_direction);
  out->target_deviation_deg = m.target_deviation_deg;
  out->wavelength_nm = m.wavelength_nm;
  out->wavelength_source = Text(lumice::raypath::WavelengthSourceName(m.wavelength_source));
  out->refractive_index = m.refractive_index;
  out->sample_count = m.sample_count;
  out->band_half_width_rad = m.band_half_width_rad;
  out->cluster_radius_rad = m.cluster_radius_rad;
  out->distance_threshold_rad = m.distance_threshold_rad;
  out->warm_seed_count = m.warm_seed_count;
  out->budget_ms = m.budget_ms;
  out->interrupted = m.interrupted;
  if (out->outcome == LUMICE_PATH_POINT_DISCOVERED) {
    out->component_count = data.Discovered().components.size();
    out->incomplete_count = data.Discovered().incomplete.size();
  }
  out->grid_lat_count = data.sun_grid.lat_count;
  out->grid_lon_count = data.sun_grid.lon_count;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathPointGetComponent(const LUMICE_SinglePathResult* result, size_t index,
                                                   LUMICE_PathPointComponent* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& data = result->result.Data();
  if (data.OutcomeValue() != lumice::raypath::Outcome::kDiscovered) {
    return LUMICE_PATH_READ_INVALID_KIND;
  }
  const auto& components = data.Discovered().components;
  if (index >= components.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& c = components[index];
  out->kind = Text(lumice::raypath::ComponentKindName(c.kind));
  std::copy_n(c.seed, 9, out->seed);
  out->forward_status = Text(lumice::raypath::TraceStatusName(c.forward.status));
  out->forward_reason = Text(lumice::raypath::TraceReasonName(c.forward.reason));
  out->forward_pose_count = c.forward.pose_count;
  if (c.backward.pose_count > 0) {
    out->backward_status = Text(lumice::raypath::TraceStatusName(c.backward.status));
    out->backward_reason = Text(lumice::raypath::TraceReasonName(c.backward.reason));
    out->backward_pose_count = c.backward.pose_count;
  }
  out->point_count = c.points.size();
  out->seed_index = c.seed_index;
  out->arclength_increments = c.arclength_increments.data();
  out->arclength_count = c.arclength_increments.size();
  out->integral_available = c.factors.integral_available;
  if (c.factors.integral_available) {
    out->fiber_integral = c.factors.fiber_integral;
  }
  out->available_points = c.factors.available_points;
  out->total_points = c.factors.total_points;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathPointGetSample(const LUMICE_SinglePathResult* result, size_t component, size_t index,
                                                LUMICE_PathPointSample* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& data = result->result.Data();
  if (data.OutcomeValue() != lumice::raypath::Outcome::kDiscovered) {
    return LUMICE_PATH_READ_INVALID_KIND;
  }
  const auto& components = data.Discovered().components;
  if (component >= components.size() || index >= components[component].points.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& p = components[component].points[index];
  std::copy_n(p.pose, 9, out->pose);
  out->zenith_deg = p.angles.zenith_deg;
  out->azimuth_deg = p.angles.azimuth_deg;
  out->roll_deg = p.angles.roll_deg;
  out->angles_degenerate = p.angles.degenerate;
  std::copy_n(p.sun_in_crystal, 3, out->sun_in_crystal);
  std::copy_n(p.outgoing_direction, 3, out->outgoing_direction);
  out->residual_norm = p.residual_norm;
  out->valid = p.valid;
  out->total_transmission = p.total_transmission;
  out->entry_measure = p.entry_measure;
  out->rho = p.rho;
  out->entry_measure_a2 = p.entry_measure_a2;
  out->jacobian_available = p.jacobian_available;
  if (p.jacobian_available) {
    out->jacobian = p.jacobian;
    std::copy_n(p.singular_values, 2, out->singular_values);
    out->k_carea = p.k_carea;
    out->factor_product = p.factor_product;
  }
  out->segment_directions = p.segment_directions.data();
  out->segment_direction_count = p.segment_directions.size();
  out->interface_transmittances = p.interface_transmittances.data();
  out->interface_count = p.interface_transmittances.size();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathPointGetSunGrid(const LUMICE_SinglePathResult* result, LUMICE_PathSunGrid* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& grid = result->result.Data().sun_grid;
  out->lat_count = grid.lat_count;
  out->lon_count = grid.lon_count;
  out->deviation_rad = grid.deviation_rad.data();
  out->valid = grid.valid.data();
  out->entry_measure = grid.entry_measure.data();
  out->cell_count = grid.deviation_rad.size();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathProbeGetInfo(const LUMICE_PathProbe* result, LUMICE_PathProbeInfo* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& data = result->result.Data();
  const auto& m = data.meta;
  out->execution = ExecutionTag(m.execution);
  out->crystal_id = static_cast<int>(m.crystal_id);
  out->analytic_api_version = m.analytic_api_version;
  out->crystal_kind = Text(m.crystal_kind);
  out->faces = m.faces.data();
  out->face_count = m.faces.size();
  out->shape_count = m.shape.size();
  out->shape_is_nominal = m.shape_is_nominal;
  out->upper_wedge_deg = m.upper_wedge_deg;
  out->lower_wedge_deg = m.lower_wedge_deg;
  out->sun_altitude_deg = m.sun_altitude_deg;
  out->sun_azimuth_deg = m.sun_azimuth_deg;
  std::copy_n(m.incident_direction, 3, out->incident_direction);
  out->at_altitude_deg = m.at_altitude_deg;
  out->at_azimuth_deg = m.at_azimuth_deg;
  std::copy_n(m.at_direction, 3, out->at_direction);
  out->at_deviation_deg = m.at_deviation_deg;
  out->at_azimuth_about_sun_deg = m.at_azimuth_about_sun_deg;
  out->budget_ms = m.budget_ms;
  out->interrupted = m.interrupted;
  out->wavelength_count = m.wavelengths.size();
  out->profile_count = data.profiles.size();
  out->red_edge_width_available = data.has_red_edge_width;
  if (data.has_red_edge_width) {
    out->red_edge_width_deg = data.red_edge_width_deg;
    out->red_edge_rule = Text(lumice::raypath::kRedEdgeWidthRule);
  }
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathProbeGetProfile(const LUMICE_PathProbe* result, size_t index,
                                                 LUMICE_PathProbeProfile* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& profiles = result->result.Data().profiles;
  if (index >= profiles.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& profile = profiles[index];
  out->along = static_cast<int>(profile.along);
  out->state = EvidenceTag(profile.State());
  if (const auto* refused = std::get_if<lumice::raypath::Refused>(&profile.Claim())) {
    out->reason = Text(refused->reason);
  } else if (const auto* unevaluated = std::get_if<lumice::raypath::Unevaluated>(&profile.Claim())) {
    out->reason = Text(unevaluated->reason);
  }
  out->wavelength_nm = profile.wavelength_nm;
  out->refractive_index = profile.refractive_index;
  out->delta_min_deg = profile.delta_min_deg;
  out->delta_max_deg = profile.delta_max_deg;
  out->azimuth_deg = profile.azimuth_deg;
  out->window_half_width_deg = profile.window_half_width_deg;
  out->bin_deg = profile.bin_deg;
  out->wedge_half_width_deg = profile.wedge_half_width_deg;
  out->anchor_available = profile.anchor_basis.has_value();
  if (profile.anchor_basis) {
    out->anchor_basis = *profile.anchor_basis == lumice::raypath::ContextEdgeBasis::kSupportIntervalEndpoint ?
                            LUMICE_PATH_ANCHOR_SUPPORT_ENDPOINT :
                            LUMICE_PATH_ANCHOR_EDGE_POSITION;
    out->anchor_deg = profile.anchor_deg;
    out->anchor_wavelength_nm = profile.anchor_wavelength_nm;
  }
  out->semantics = Text(profile.semantics);
  out->lattice = Text(profile.lattice);
  out->deterministic = profile.deterministic;
  out->member_count = profile.member_count;
  out->sample_count = profile.sample_count;
  out->representative = profile.representative.data();
  out->representative_count = profile.representative.size();
  const auto* reading = lumice::raypath::ClaimValue(profile.Claim());
  if (reading != nullptr) {
    out->source_available = 1;
    out->source = reading->source == lumice::raypath::ProbeProfileSource::kWarmReused ? LUMICE_PATH_PROBE_WARM_REUSED :
                                                                                        LUMICE_PATH_PROBE_RECOMPUTED;
    out->members_evaluated = reading->members_evaluated;
    out->sample_row_count = reading->samples.size();
    out->dark_region_count = reading->dark_regions.size();
    out->edge_available = reading->readings.has_edge;
    if (out->edge_available) {
      out->edge_deg = reading->readings.edge_deg;
    }
    out->peak_available = reading->readings.has_peak;
    if (out->peak_available) {
      out->peak_deg = reading->readings.peak_deg;
      out->peak_value = reading->readings.peak_value;
      out->separation_available = reading->readings.has_edge;
      if (out->separation_available) {
        out->separation_deg = reading->readings.separation_deg;
      }
    }
  }
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathProbeGetSample(const LUMICE_PathProbe* result, size_t profile, size_t index,
                                                LUMICE_PathProbeSample* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& profiles = result->result.Data().profiles;
  if (profile >= profiles.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto* reading = lumice::raypath::ClaimValue(profiles[profile].Claim());
  if (reading == nullptr) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  if (index >= reading->samples.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& sample = reading->samples[index];
  out->delta_deg = sample.delta_deg;
  out->azimuth_available = 1;
  out->azimuth_deg = sample.azimuth_deg;
  out->value = sample.value;
  out->k = sample.k;
  out->k_eff = sample.k_eff;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathProbeGetDarkRegion(const LUMICE_PathProbe* result, size_t profile, size_t index,
                                                    LUMICE_PathProbeDarkRegion* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& profiles = result->result.Data().profiles;
  if (profile >= profiles.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto* reading = lumice::raypath::ClaimValue(profiles[profile].Claim());
  if (!reading) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  if (index >= reading->dark_regions.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& region = reading->dark_regions[index];
  using K = lumice::raypath::ProbeDarkKind;
  switch (region.kind) {
    case K::kInsideSupportPositive:
      out->kind = LUMICE_PATH_DARK_INSIDE_POSITIVE;
      break;
    case K::kInsideSupportZero:
      out->kind = LUMICE_PATH_DARK_INSIDE_ZERO;
      break;
    case K::kOutsideSupport:
      out->kind = LUMICE_PATH_DARK_OUTSIDE;
      break;
    case K::kUnknown:
      out->kind = LUMICE_PATH_DARK_UNKNOWN;
      break;
    default:
      return LUMICE_PATH_READ_INVALID_KIND;
  }
  out->lo_deg = region.lo_deg;
  out->hi_deg = region.hi_deg;
  out->bin_count = region.bin_count;
  out->reason = Text(region.reason);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetInfo(const LUMICE_PathSurvey* result, LUMICE_PathSurveyInfo* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& d = result->result.Data();
  out->execution = ExecutionTag(d.execution);
  out->schema_version = d.schema_version;
  out->crystal_id = d.request.crystal_id;
  out->symmetry_bits = d.request.symmetry_bits;
  out->wavelength_nm = d.wavelength_nm;
  out->budget_ms = d.budget_ms;
  out->early_stopped = d.early_stopped;
  out->evidence_state = EvidenceTag(d.evidence.state);
  out->member_count = d.members.size();
  out->object_count = d.objects.size();
  out->unevaluated_count = d.unevaluated.size();
  out->probe_count = d.probes.size();
  out->warm_seed_count = d.warm_index.seeds.size();
  out->family_available = d.family_routing.has_value();
  out->tint_available = d.weight_tint.has_value();
  out->tir_available = d.tir_share.has_value();
  out->ess_available = d.mc_region_ess.has_value();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathProbeGetEvidence(const LUMICE_PathProbe* result, size_t profile,
                                                  LUMICE_PathSurveyEvidence* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& profiles = result->result.Data().profiles;
  if (profile >= profiles.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  const auto& claim = profiles[profile].Claim();
  out->state = EvidenceTag(profiles[profile].State());
  if (const auto* arm = std::get_if<lumice::raypath::Certified<lumice::raypath::ProbeFiniteReading>>(&claim)) {
    out->method = Text(arm->method);
  } else if (const auto* arm = std::get_if<lumice::raypath::Finite<lumice::raypath::ProbeFiniteReading>>(&claim)) {
    out->coverage_available = 1;
    out->members_evaluated = arm->coverage.members_evaluated;
    out->sample_count = arm->coverage.sample_count;
    out->geometry_basis = Text(arm->coverage.geometry_basis);
    out->wavelength_nm = arm->coverage.wavelength_nm;
  } else if (const auto* arm = std::get_if<lumice::raypath::Refused>(&claim)) {
    out->mode = Text(arm->reason);
  } else {
    out->reason = Text(std::get<lumice::raypath::Unevaluated>(claim).reason);
  }
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetEvidence(const LUMICE_PathSurvey* result, int kind, size_t index,
                                                   LUMICE_PathSurveyEvidence* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& d = result->result.Data();
  switch (kind) {
    case LUMICE_PATH_EVIDENCE_DOCUMENT:
      if (index != 0)
        return LUMICE_PATH_READ_INVALID_INDEX;
      FillEvidence(d.evidence, out);
      return LUMICE_PATH_READ_OK;
    case LUMICE_PATH_EVIDENCE_CHROMATIC:
      if (index != 0)
        return LUMICE_PATH_READ_INVALID_INDEX;
      FillEvidence(d.chromatic, out);
      return LUMICE_PATH_READ_OK;
    case LUMICE_PATH_EVIDENCE_FAMILY:
      if (index != 0)
        return LUMICE_PATH_READ_INVALID_INDEX;
      if (!d.family_routing)
        return LUMICE_PATH_READ_NOT_AVAILABLE;
      FillEvidence(d.family_routing->evidence, out);
      return LUMICE_PATH_READ_OK;
    case LUMICE_PATH_EVIDENCE_MEMBER:
      if (index >= d.member_verdicts.size())
        return LUMICE_PATH_READ_INVALID_INDEX;
      FillEvidence(d.member_verdicts[index].evidence, out);
      return LUMICE_PATH_READ_OK;
    case LUMICE_PATH_EVIDENCE_OBJECT:
      if (index >= d.objects.size())
        return LUMICE_PATH_READ_INVALID_INDEX;
      FillClaimEvidence(d.objects[index], out);
      return LUMICE_PATH_READ_OK;
    case LUMICE_PATH_EVIDENCE_DISPERSION:
      if (index != 0)
        return LUMICE_PATH_READ_INVALID_INDEX;
      if (!d.positional_dispersion)
        return LUMICE_PATH_READ_NOT_AVAILABLE;
      FillEvidence(d.positional_dispersion->evidence, out);
      return LUMICE_PATH_READ_OK;
    case LUMICE_PATH_EVIDENCE_DIAGNOSE:
      if (index != 0)
        return LUMICE_PATH_READ_INVALID_INDEX;
      if (!d.diagnose)
        return LUMICE_PATH_READ_NOT_AVAILABLE;
      FillEvidence(d.diagnose->evidence, out);
      return LUMICE_PATH_READ_OK;
    case LUMICE_PATH_EVIDENCE_TINT:
      if (index != 0)
        return LUMICE_PATH_READ_INVALID_INDEX;
      if (!d.weight_tint)
        return LUMICE_PATH_READ_NOT_AVAILABLE;
      FillClaimEvidence(*d.weight_tint, out);
      return LUMICE_PATH_READ_OK;
    case LUMICE_PATH_EVIDENCE_TIR:
      if (index != 0)
        return LUMICE_PATH_READ_INVALID_INDEX;
      if (!d.tir_share)
        return LUMICE_PATH_READ_NOT_AVAILABLE;
      FillClaimEvidence(*d.tir_share, out);
      return LUMICE_PATH_READ_OK;
    case LUMICE_PATH_EVIDENCE_ESS:
      if (index != 0)
        return LUMICE_PATH_READ_INVALID_INDEX;
      if (!d.mc_region_ess)
        return LUMICE_PATH_READ_NOT_AVAILABLE;
      FillClaimEvidence(*d.mc_region_ess, out);
      return LUMICE_PATH_READ_OK;
    case LUMICE_PATH_EVIDENCE_PROBE:
      if (index >= d.probes.size())
        return LUMICE_PATH_READ_INVALID_INDEX;
      FillClaimEvidence(d.probes[index], out);
      return LUMICE_PATH_READ_OK;
    case LUMICE_PATH_EVIDENCE_WITNESS:
      if (index >= d.objects.size())
        return LUMICE_PATH_READ_INVALID_INDEX;
      if (d.objects[index].Kind() != lumice::raypath::survey::SurveyObjectKind::kSupportSet)
        return LUMICE_PATH_READ_INVALID_KIND;
      if (!d.objects[index].Witness())
        return LUMICE_PATH_READ_NOT_AVAILABLE;
      FillClaimEvidence(*d.objects[index].Witness(), out);
      return LUMICE_PATH_READ_OK;
    default:
      return LUMICE_PATH_READ_INVALID_KIND;
  }
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetObject(const LUMICE_PathSurvey* result, size_t index,
                                                 LUMICE_PathSurveyObject* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& objects = result->result.Data().objects;
  if (index >= objects.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& object = objects[index];
  out->kind = SurveyKindTag(object.Kind());
  out->state = EvidenceTag(object.State());
  if (out->state == LUMICE_PATH_STATE_CERTIFIED) {
    out->method = Text(object.Method());
  } else if (out->state == LUMICE_PATH_STATE_REFUSED) {
    out->mode = Text(object.Mode());
  } else if (out->state == LUMICE_PATH_STATE_UNEVALUATED) {
    out->reason = Text(object.Reason());
  }
  out->member_count = object.members.size();
  out->position_available = object.has_position;
  if (object.has_position) {
    out->position_altitude_deg = object.position_altitude_deg;
    out->position_azimuth_deg = object.position_azimuth_deg;
  }
  out->band_available = object.band.has_value();
  if (object.band) {
    out->family_level = object.band->level;
    out->blur_deg = object.band->blur_deg;
    out->scale_deg = object.band->scale_deg;
  }
  if (const auto* support = object.Support()) {
    out->support_interval_count = support->intervals.size();
  }
  out->witness_available = object.Witness() != nullptr;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetFamily(const LUMICE_PathSurvey* result, LUMICE_PathFamilyInfo* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& family = result->result.Data().family_routing;
  if (!family) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  out->state = EvidenceTag(family->evidence.state);
  out->measure_kind = static_cast<int>(family->measure.kind) + 1;
  if (family->level0_refusal_mode) {
    out->level0_refusal_mode = Text(*family->level0_refusal_mode);
  }
  out->ladder_count = family->ladder.size();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetFamilyLevel(const LUMICE_PathSurvey* result, size_t index,
                                                      LUMICE_PathFamilyLevel* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& family = result->result.Data().family_routing;
  if (!family) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  if (index >= family->ladder.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& level = family->ladder[index];
  out->level = level.level;
  out->frozen_dim_count = level.frozen_dims.size();
  out->free_dim_count = level.free_dims.size();
  out->rank_asked = level.rank_asked;
  if (level.rank_asked) {
    out->generic_rank = level.generic_rank;
  }
  out->grid = level.grid;
  std::copy_n(level.grid_spacing_deg, 2, out->grid_spacing_deg);
  out->samples = level.samples;
  out->valid_samples = level.valid_samples;
  out->rank_refused_members = level.rank_refused_members;
  out->objects_proposed = level.objects_proposed;
  out->objects_killed = level.objects_killed;
  out->killed_count = level.killed.size();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetTint(const LUMICE_PathSurvey* result, LUMICE_PathTint* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& tint = result->result.Data().weight_tint;
  if (!tint) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  out->state = EvidenceTag(tint->State());
  if (out->state == LUMICE_PATH_STATE_REFUSED) {
    out->mode = Text(tint->Mode());
  }
  const auto* value = tint->ValueOrNull();
  if (value == nullptr) {
    return LUMICE_PATH_READ_OK;
  }
  out->reading_available = 1;
  out->class_power = value->IsClassPower();
  out->reading_unit = static_cast<int>(value->reading_unit) + 1;
  out->red_label = Text(value->red_label);
  out->blue_label = Text(value->blue_label);
  out->n_red = value->n_red;
  out->n_blue = value->n_blue;
  out->method = Text(lumice::raypath::TintMethodName(value->Method()));
  out->direction = Text(lumice::raypath::TintDirectionName(value->direction));
  out->lattice = Text(value->lattice);
  out->deterministic = value->deterministic;
  out->representative = value->representative.data();
  out->representative_count = value->representative.size();
  out->member_count = value->member_count;
  out->ratio = value->ratio;
  if (value->IsClassPower()) {
    out->red = value->Power().red;
    out->blue = value->Power().blue;
  } else {
    out->red = value->Mass().red;
    out->blue = value->Mass().blue;
    out->red_error = value->Mass().red_error;
    out->blue_error = value->Mass().blue_error;
  }
  out->sample_count = value->sample_count;
  out->kept_count_red = value->kept_count_red;
  out->kept_count_blue = value->kept_count_blue;
  out->caliber = Text(value->caliber);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetTirShare(const LUMICE_PathSurvey* result, LUMICE_PathTirShare* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& tir = result->result.Data().tir_share;
  if (!tir) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  out->state = EvidenceTag(tir->State());
  if (out->state == LUMICE_PATH_STATE_REFUSED) {
    out->mode = Text(tir->Mode());
  }
  const auto* value = tir->ValueOrNull();
  if (value == nullptr) {
    return LUMICE_PATH_READ_OK;
  }
  out->value_available = 1;
  out->n_red = value->n_red;
  out->n_blue = value->n_blue;
  out->sample_count = value->sample_count;
  out->kept_count_red = value->kept_count_red;
  out->kept_count_blue = value->kept_count_blue;
  out->lattice = Text(value->lattice);
  out->deterministic = value->deterministic;
  out->representative = value->representative.data();
  out->representative_count = value->representative.size();
  out->member_count = value->member_count;
  out->caliber = Text(value->caliber);
  out->total_weight_red = value->total_weight_red;
  out->total_weight_blue = value->total_weight_blue;
  out->all_tir_weight_red = value->all_tir_weight_red;
  out->all_tir_weight_blue = value->all_tir_weight_blue;
  out->share_all_tir_red = value->share_all_tir_red;
  out->share_all_tir_blue = value->share_all_tir_blue;
  out->chart_degenerate_red = value->chart_degenerate_red;
  out->chart_degenerate_blue = value->chart_degenerate_blue;
  out->interface_count = value->interfaces.size();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetTirInterface(const LUMICE_PathSurvey* result, size_t index,
                                                       LUMICE_PathTirInterface* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& tir = result->result.Data().tir_share;
  if (!tir || !tir->ValueOrNull()) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  const auto& interfaces = tir->ValueOrNull()->interfaces;
  if (index >= interfaces.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& row = interfaces[index];
  out->face = row.face;
  out->share_red = row.share_red;
  out->share_blue = row.share_blue;
  out->tir_weight_red = row.tir_weight_red;
  out->tir_weight_blue = row.tir_weight_blue;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetMcEss(const LUMICE_PathSurvey* result, LUMICE_PathMcEss* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& ess = result->result.Data().mc_region_ess;
  if (!ess) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  out->state = EvidenceTag(ess->State());
  if (out->state == LUMICE_PATH_STATE_REFUSED) {
    out->mode = Text(ess->Mode());
  }
  const auto* value = ess->ValueOrNull();
  if (!value) {
    return LUMICE_PATH_READ_OK;
  }
  out->value_available = 1;
  out->seed = value->seed;
  out->draws = value->draws;
  out->wavelength_nm = value->lambda_nm;
  out->band_deg = value->band_deg;
  out->deterministic = value->deterministic;
  out->band_count = value->bands.size();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetMcEssBand(const LUMICE_PathSurvey* result, size_t index,
                                                    LUMICE_PathMcEssBand* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& ess = result->result.Data().mc_region_ess;
  if (!ess || !ess->ValueOrNull()) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  const auto& bands = ess->ValueOrNull()->bands;
  if (index >= bands.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& b = bands[index];
  out->delta_min_deg = b.delta_min_deg;
  out->delta_max_deg = b.delta_max_deg;
  out->ess = b.ess;
  out->draw_count = b.draw_count;
  out->sum_weight = b.sum_weight;
  out->sum_squared_weight = b.sum_squared_weight;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetWarmSeed(const LUMICE_PathSurvey* result, size_t index,
                                                   LUMICE_PathWarmSeed* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& seeds = result->result.Data().warm_index.seeds;
  if (index >= seeds.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  std::copy(seeds[index].pose.begin(), seeds[index].pose.end(), out->pose);
  out->source = Text(seeds[index].source);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetObjectMember(const LUMICE_PathSurvey* result, size_t object, size_t index,
                                                       LUMICE_PathFaceSequence* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& objects = result->result.Data().objects;
  if (object >= objects.size() || index >= objects[object].members.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& faces = objects[object].members[index];
  out->faces = faces.data();
  out->face_count = faces.size();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetObjectBandVerdict(const LUMICE_PathSurvey* result, size_t object,
                                                            size_t index, LUMICE_PathBandVerdict* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& objects = result->result.Data().objects;
  if (object >= objects.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  if (!objects[object].IsFamilyKind()) {
    return LUMICE_PATH_READ_INVALID_KIND;
  }
  const auto& band = objects[object].band;
  if (!band) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  if (index >= band->frozen.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  out->dimension = Text(band->frozen[index].dim);
  out->verdict = Text(band->frozen[index].verdict);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetFamilyOrigin(const LUMICE_PathSurvey* result, size_t object,
                                                       LUMICE_PathFamilyOrigin* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& objects = result->result.Data().objects;
  if (object >= objects.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  if (!objects[object].IsFamilyKind()) {
    return LUMICE_PATH_READ_INVALID_KIND;
  }
  const auto& origin = objects[object].family_origin;
  if (!origin) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  out->sample_count = origin->sample_count;
  out->crossing_count = origin->crossing_count;
  out->frozen_verdict_count = objects[object].band ? objects[object].band->frozen.size() : 0;
  out->band_clamped = objects[object].band && objects[object].band->clamped;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetSupport(const LUMICE_PathSurvey* result, size_t object,
                                                  LUMICE_PathSurveySupport* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& objects = result->result.Data().objects;
  if (object >= objects.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  if (objects[object].Kind() != lumice::raypath::survey::SurveyObjectKind::kSupportSet) {
    return LUMICE_PATH_READ_INVALID_KIND;
  }
  const auto* value = objects[object].Support();
  if (!value) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  out->symmetry_bits = value->symmetry_bits;
  out->member_count = value->member_count;
  out->interval_count = value->intervals.size();
  out->endpoint_count = value->endpoints.size();
  out->density_tail_count = value->density_tails.size();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetSupportEndpoint(const LUMICE_PathSurvey* result, size_t object, size_t index,
                                                          LUMICE_PathSurveyEndpoint* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& objects = result->result.Data().objects;
  if (object >= objects.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  if (objects[object].Kind() != lumice::raypath::survey::SurveyObjectKind::kSupportSet) {
    return LUMICE_PATH_READ_INVALID_KIND;
  }
  const auto* value = objects[object].Support();
  if (!value) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  if (index >= value->endpoints.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& row = value->endpoints[index];
  out->delta_deg = row.delta_deg;
  out->source = Text(row.source);
  out->profile = Text(row.profile);
  out->multiplicity = row.multiplicity;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetSupportTail(const LUMICE_PathSurvey* result, size_t object, size_t index,
                                                      LUMICE_PathSurveyTail* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& objects = result->result.Data().objects;
  if (object >= objects.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  if (objects[object].Kind() != lumice::raypath::survey::SurveyObjectKind::kSupportSet) {
    return LUMICE_PATH_READ_INVALID_KIND;
  }
  const auto* value = objects[object].Support();
  if (!value) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  if (index >= value->density_tails.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& row = value->density_tails[index];
  out->dimension = Text(row.dim);
  out->lower_deg = row.lower_deg;
  out->upper_deg = row.upper_deg;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetEdge(const LUMICE_PathSurvey* result, size_t object,
                                               LUMICE_PathSurveyEdge* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& objects = result->result.Data().objects;
  if (object >= objects.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  if (objects[object].Kind() != lumice::raypath::survey::SurveyObjectKind::kCausticEdge) {
    return LUMICE_PATH_READ_INVALID_KIND;
  }
  const auto* value = objects[object].Edge();
  if (!value) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  out->position_deg = value->position_deg;
  out->profile = Text(value->profile);
  out->measure_limit_available = value->has_measure_limit;
  if (value->has_measure_limit) {
    out->measure_limit = value->measure_limit;
  }
  out->coincident_with_support = value->coincident_with_support;
  out->certificate_available = value->certificate.has_value();
  if (value->certificate) {
    out->formula = Text(value->certificate->formula);
    out->apex_angle_deg = value->certificate->apex_angle_deg;
    out->cross_check_deg = value->certificate->cross_check_deg;
    out->tolerance_deg = value->certificate->tolerance_deg;
  }
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetBright(const LUMICE_PathSurvey* result, size_t object,
                                                 LUMICE_PathSurveyBright* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& objects = result->result.Data().objects;
  if (object >= objects.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  if (objects[object].Kind() != lumice::raypath::survey::SurveyObjectKind::kBrightDomain) {
    return LUMICE_PATH_READ_INVALID_KIND;
  }
  const auto* value = objects[object].Bright();
  if (!value) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  out->domain_lower_deg = value->domain_lower_deg;
  out->domain_upper_deg = value->domain_upper_deg;
  out->peak_available = value->has_peak;
  if (value->has_peak) {
    out->peak_delta_deg = value->peak_delta_deg;
    out->peak_value = value->peak_value;
  }
  out->edge_reading_available = value->has_edge_reading;
  if (value->has_edge_reading) {
    out->edge_delta_deg = value->edge_delta_deg;
  }
  out->kept_count = value->kept_count;
  out->probe_index_available = value->probe_index >= 0;
  if (out->probe_index_available) {
    out->probe_index = static_cast<size_t>(value->probe_index);
  }
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetSourceCertificate(const LUMICE_PathSurvey* result, size_t object,
                                                            LUMICE_PathSurveySourceCertificate* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& objects = result->result.Data().objects;
  if (object >= objects.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  if (objects[object].Kind() != lumice::raypath::survey::SurveyObjectKind::kSourceImage) {
    return LUMICE_PATH_READ_INVALID_KIND;
  }
  const auto* value = objects[object].Certificate();
  if (!value) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  out->fold_tolerance = value->fold_tolerance;
  out->wedge_tolerance_deg = value->wedge_tolerance_deg;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetFamilyKilledVerdict(const LUMICE_PathSurvey* result, size_t level,
                                                              size_t killed, size_t index,
                                                              LUMICE_PathBandVerdict* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& family = result->result.Data().family_routing;
  if (!family) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  if (level >= family->ladder.size() || killed >= family->ladder[level].killed.size() ||
      index >= family->ladder[level].killed[killed].verdicts.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& verdict = family->ladder[level].killed[killed].verdicts[index];
  out->dimension = Text(verdict.dim);
  out->verdict = Text(verdict.verdict);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetFamilyFreeDim(const LUMICE_PathSurvey* result, size_t level, size_t index,
                                                        LUMICE_PathText* out) {
  if (!out) {
    return LUMICE_PATH_READ_NULL_ARG;
  }
  *out = {};
  if (!result) {
    return LUMICE_PATH_READ_NULL_ARG;
  }
  const auto& family = result->result.Data().family_routing;
  if (!family) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  if (level >= family->ladder.size() || index >= family->ladder[level].free_dims.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto dim = family->ladder[level].free_dims[index];
  if (dim != lumice::analytic::FamilyDimension::kZenith && dim != lumice::analytic::FamilyDimension::kAzimuth &&
      dim != lumice::analytic::FamilyDimension::kRoll) {
    return LUMICE_PATH_READ_INVALID_KIND;
  }
  *out = Text(lumice::raypath::structure::DimName(dim));
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetScope(const LUMICE_PathSurvey* result, LUMICE_PathSurveyScope* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& d = result->result.Data();
  out->scene_identity = Text(d.scene_identity);
  out->scene_layer_available = d.scene_layer.has_value();
  if (d.scene_layer) {
    out->scene_layer = *d.scene_layer;
  }
  out->wavelength_nm = d.wavelength_nm;
  out->request_wavelength_given = d.request.wavelength_given;
  if (d.request.wavelength_given) {
    out->request_wavelength_nm = d.request.wavelength_nm;
  }
  out->member_kind = d.quantity_context ? static_cast<int>(d.quantity_context->members.member_kind) + 1 :
                                          (d.request.symmetry_bits == 0 ? 1 : 2);
  out->geometry_kind = static_cast<int>(d.geometry.kind) + 1;
  out->wavelength_source = Text(d.wavelength_source);
  out->refractive_index = d.refractive_index;
  out->sun_altitude_available = d.sun_altitude_deg.has_value();
  if (d.sun_altitude_deg) {
    out->sun_altitude_deg = *d.sun_altitude_deg;
  }
  out->sun_azimuth_available = d.sun_azimuth_deg.has_value();
  if (d.sun_azimuth_deg) {
    out->sun_azimuth_deg = *d.sun_azimuth_deg;
  }
  out->request_layer_count = d.request.path_layers.size();
  out->routing_member_count = d.member_verdicts.size();
  out->warm_reuse_scope = Text(d.warm_index.reuse_scope);
  out->warm_produced_for = Text(d.warm_index.produced_for);
  out->warm_config_fingerprint = Text(d.warm_index.config_fingerprint);
  out->budget_note = Text(d.budget_note);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetClaimMember(const LUMICE_PathSurvey* result, int kind, size_t owner,
                                                      size_t member, LUMICE_PathFaceSequence* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& data = result->result.Data();
  const lumice::raypath::ClaimScope* scope = nullptr;
  switch (kind) {
    case LUMICE_PATH_EVIDENCE_OBJECT:
      if (owner >= data.objects.size())
        return LUMICE_PATH_READ_INVALID_INDEX;
      scope = &std::visit([](const auto& arm) -> const lumice::raypath::ClaimScope& { return arm.Scope(); },
                          data.objects[owner].Declaration());
      break;
    case LUMICE_PATH_EVIDENCE_PROBE:
      if (owner >= data.probes.size())
        return LUMICE_PATH_READ_INVALID_INDEX;
      scope = &data.probes[owner].Scope();
      break;
    case LUMICE_PATH_EVIDENCE_WITNESS:
      if (owner >= data.objects.size())
        return LUMICE_PATH_READ_INVALID_INDEX;
      if (data.objects[owner].Kind() != lumice::raypath::survey::SurveyObjectKind::kSupportSet)
        return LUMICE_PATH_READ_INVALID_KIND;
      if (!data.objects[owner].Witness())
        return LUMICE_PATH_READ_NOT_AVAILABLE;
      scope = &data.objects[owner].Witness()->Scope();
      break;
    case LUMICE_PATH_EVIDENCE_TINT:
      if (owner != 0)
        return LUMICE_PATH_READ_INVALID_INDEX;
      if (!data.weight_tint)
        return LUMICE_PATH_READ_NOT_AVAILABLE;
      scope = &data.weight_tint->Scope();
      break;
    case LUMICE_PATH_EVIDENCE_TIR:
      if (owner != 0)
        return LUMICE_PATH_READ_INVALID_INDEX;
      if (!data.tir_share)
        return LUMICE_PATH_READ_NOT_AVAILABLE;
      scope = &data.tir_share->Scope();
      break;
    case LUMICE_PATH_EVIDENCE_ESS:
      if (owner != 0)
        return LUMICE_PATH_READ_INVALID_INDEX;
      if (!data.mc_region_ess)
        return LUMICE_PATH_READ_NOT_AVAILABLE;
      scope = &data.mc_region_ess->Scope();
      break;
    default:
      return LUMICE_PATH_READ_INVALID_KIND;
  }
  if (member >= scope->members.members.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  const auto& faces = scope->members.members[member];
  out->faces = faces.data();
  out->face_count = faces.size();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetRequestLayer(const LUMICE_PathSurvey* result, size_t index,
                                                       LUMICE_PathFaceSequence* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().request.path_layers;
  if (index >= rows.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  out->faces = rows[index].data();
  out->face_count = rows[index].size();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetMember(const LUMICE_PathSurvey* result, size_t index,
                                                 LUMICE_PathFaceSequence* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().members;
  if (index >= rows.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  out->faces = rows[index].data();
  out->face_count = rows[index].size();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetMemberVerdict(const LUMICE_PathSurvey* result, size_t index,
                                                        LUMICE_PathSurveyMemberVerdict* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().member_verdicts;
  if (index >= rows.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& row = rows[index];
  out->member = row.member.data();
  out->face_count = row.member.size();
  out->rank_zero = row.rank_zero;
  out->mechanism = Text(row.mechanism);
  out->mechanism_unavailable = Text(row.mechanism_unavailable);
  out->evidence_state = EvidenceTag(row.evidence.state);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetUnevaluated(const LUMICE_PathSurvey* result, size_t index,
                                                      LUMICE_PathSurveyUnevaluated* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().unevaluated;
  if (index >= rows.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  out->reason = Text(rows[index].reason);
  out->kind_count = rows[index].kinds.size();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetUnevaluatedKind(const LUMICE_PathSurvey* result, size_t object, size_t index,
                                                          int* out_kind) {
  if (out_kind == nullptr) {
    return LUMICE_PATH_READ_NULL_ARG;
  }
  *out_kind = 0;
  if (result == nullptr) {
    return LUMICE_PATH_READ_NULL_ARG;
  }
  const auto& rows = result->result.Data().unevaluated;
  if (object >= rows.size() || index >= rows[object].kinds.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  *out_kind = SurveyKindTag(rows[object].kinds[index]);
  return *out_kind == 0 ? LUMICE_PATH_READ_INVALID_KIND : LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetFamilyDimension(const LUMICE_PathSurvey* result, size_t index,
                                                          LUMICE_PathFamilyDimension* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& family = result->result.Data().family_routing;
  if (!family) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  const auto& measure = family->measure;
  if (index > 2) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& dim = index == 0 ? measure.zenith : index == 1 ? measure.azimuth : measure.roll;
  if (dim.kind != lumice::raypath::survey::FamilyDimKind::kFixed &&
      dim.kind != lumice::raypath::survey::FamilyDimKind::kGauss &&
      dim.kind != lumice::raypath::survey::FamilyDimKind::kUniformFull) {
    return LUMICE_PATH_READ_INVALID_KIND;
  }
  out->kind = Text(lumice::raypath::survey::FamilyDimKindName(dim.kind));
  out->center_deg = dim.center_deg;
  out->width_deg = dim.width_deg;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetFamilyFrozenDim(const LUMICE_PathSurvey* result, size_t level, size_t index,
                                                          LUMICE_PathFamilyFrozenDim* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& family = result->result.Data().family_routing;
  if (!family) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  if (level >= family->ladder.size() || index >= family->ladder[level].frozen_dims.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& dim = family->ladder[level].frozen_dims[index];
  if (dim.dim != lumice::analytic::FamilyDimension::kZenith && dim.dim != lumice::analytic::FamilyDimension::kAzimuth &&
      dim.dim != lumice::analytic::FamilyDimension::kRoll) {
    return LUMICE_PATH_READ_INVALID_KIND;
  }
  out->dim = Text(lumice::raypath::structure::DimName(dim.dim));
  out->center_deg = dim.center_deg;
  out->width_deg = dim.width_deg;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetFamilyKilled(const LUMICE_PathSurvey* result, size_t level, size_t index,
                                                       LUMICE_PathFamilyKilled* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& family = result->result.Data().family_routing;
  if (!family) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  if (level >= family->ladder.size() || index >= family->ladder[level].killed.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& row = family->ladder[level].killed[index];
  out->member = row.member.data();
  out->member_face_count = row.member.size();
  out->kind = FamilyKindTag(row.kind);
  if (out->kind == 0) {
    return LUMICE_PATH_READ_INVALID_KIND;
  }
  out->position_available = row.has_position;
  if (row.has_position) {
    out->position_altitude_deg = row.position_altitude_deg;
    out->position_azimuth_deg = row.position_azimuth_deg;
  }
  out->blur_measured = row.blur_measured;
  if (row.blur_measured) {
    out->blur_deg = row.blur_deg;
    out->scale_deg = row.scale_deg;
    out->clamped = row.clamped;
  }
  out->reason = Text(row.reason);
  out->verdict_count = row.verdicts.size();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetSupportInterval(const LUMICE_PathSurvey* result, size_t object, size_t index,
                                                          LUMICE_PathSurveyInterval* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& objects = result->result.Data().objects;
  if (object >= objects.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  if (objects[object].Kind() != lumice::raypath::survey::SurveyObjectKind::kSupportSet) {
    return LUMICE_PATH_READ_INVALID_KIND;
  }
  const auto* support = objects[object].Support();
  if (!support) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  if (index >= support->intervals.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& row = support->intervals[index];
  out->lower_deg = row.lower_deg;
  out->upper_deg = row.upper_deg;
  out->n_components = row.n_components;
  out->n_closed = row.n_closed;
  out->n_open = row.n_open;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetChromatic(const LUMICE_PathSurvey* result, LUMICE_PathChromatic* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& d = result->result.Data();
  out->state = EvidenceTag(d.chromatic.state);
  if (out->state == LUMICE_PATH_STATE_CERTIFIED) {
    out->method = Text(d.chromatic.method);
  } else if (out->state == LUMICE_PATH_STATE_REFUSED) {
    out->mode = Text(d.chromatic.mode);
  } else if (out->state == LUMICE_PATH_STATE_UNEVALUATED) {
    out->reason = Text(d.chromatic.reason);
  }
  out->dispersion_available = d.positional_dispersion.has_value();
  out->diagnose_available = d.diagnose.has_value();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetDispersion(const LUMICE_PathSurvey* result, LUMICE_PathDispersion* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& dispersion = result->result.Data().positional_dispersion;
  if (!dispersion) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  out->red_label = Text(dispersion->red_label);
  out->blue_label = Text(dispersion->blue_label);
  out->n_red = dispersion->n_red;
  out->n_blue = dispersion->n_blue;
  out->min_row_available = dispersion->has_min_deviation_row;
  if (dispersion->has_min_deviation_row) {
    out->value_red_deg = dispersion->value_red_deg;
    out->value_blue_deg = dispersion->value_blue_deg;
    out->displacement_deg = dispersion->displacement_deg;
    out->direction = Text(dispersion->direction);
  }
  out->followed_count = dispersion->followed_onsets.size();
  out->evidence_state = EvidenceTag(dispersion->evidence.state);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetFollowedOnset(const LUMICE_PathSurvey* result, size_t index,
                                                        LUMICE_PathFollowedOnset* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& dispersion = result->result.Data().positional_dispersion;
  if (!dispersion) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  if (index >= dispersion->followed_onsets.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& onset = dispersion->followed_onsets[index];
  out->source = Text(onset.source);
  out->profile = Text(onset.profile);
  out->values_deg = onset.values_deg.data();
  out->value_count = onset.values_deg.size();
  out->displacement_deg = onset.displacement_deg;
  out->direction = Text(onset.direction);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetDiagnose(const LUMICE_PathSurvey* result, LUMICE_PathDiagnose* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& diagnose = result->result.Data().diagnose;
  if (!diagnose) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  out->verdict_kind = Text(diagnose->verdict_kind);
  out->color = Text(diagnose->color);
  out->visible = diagnose->visible;
  out->position_available = diagnose->has_position;
  if (diagnose->has_position) {
    out->position_deg = diagnose->position_deg;
  }
  out->coverage_complete = diagnose->coverage_complete;
  out->n_red = diagnose->n_red;
  out->n_blue = diagnose->n_blue;
  out->feature_count = diagnose->features.size();
  out->note_count = diagnose->notes.size();
  out->evidence_state = EvidenceTag(diagnose->evidence.state);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetDiagnoseNote(const LUMICE_PathSurvey* result, size_t index,
                                                       LUMICE_PathText* out) {
  if (!out)
    return LUMICE_PATH_READ_NULL_ARG;
  *out = {};
  if (!result)
    return LUMICE_PATH_READ_NULL_ARG;
  const auto& diagnose = result->result.Data().diagnose;
  if (!diagnose)
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  if (index >= diagnose->notes.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  *out = Text(diagnose->notes[index]);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetDiagnoseFeature(const LUMICE_PathSurvey* result, size_t index,
                                                          LUMICE_PathDiagnoseFeature* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& diagnose = result->result.Data().diagnose;
  if (!diagnose) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  if (index >= diagnose->features.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& f = diagnose->features[index];
  out->kind = Text(f.kind);
  out->source = Text(f.source);
  out->color = Text(f.color);
  out->visible = f.visible;
  out->delta_red_deg = f.delta_red_deg;
  out->delta_blue_deg = f.delta_blue_deg;
  out->shift_deg = f.shift_deg;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetProbe(const LUMICE_PathSurvey* result, size_t index,
                                                LUMICE_PathSurveyProbe* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& probes = result->result.Data().probes;
  if (index >= probes.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& probe = probes[index];
  out->state = EvidenceTag(probe.State());
  out->wavelength_nm = probe.spec.wavelength_nm;
  out->delta_min_deg = probe.spec.delta_min_deg;
  out->delta_max_deg = probe.spec.delta_max_deg;
  out->bin_deg = probe.spec.bin_deg;
  out->wedge_half_width_deg = probe.spec.wedge_half_width_deg;
  out->azimuth_available = probe.spec.azimuth_deg.has_value();
  if (probe.spec.azimuth_deg)
    out->azimuth_deg = *probe.spec.azimuth_deg;
  out->refractive_index = probe.spec.refractive_index;
  out->lattice = Text(probe.spec.lattice);
  out->deterministic = probe.spec.deterministic;
  out->member_count = probe.spec.member_count;
  out->sample_count = probe.spec.sample_count;
  out->representative = probe.spec.representative.data();
  out->representative_count = probe.spec.representative.size();
  if (out->state == LUMICE_PATH_STATE_REFUSED) {
    out->reason = Text(probe.Mode());
  } else if (out->state == LUMICE_PATH_STATE_UNEVALUATED) {
    out->reason = Text(probe.Reason());
  }
  if (const auto* value = probe.ValueOrNull()) {
    out->sample_row_count = value->samples.size();
    out->edge_available = value->has_edge;
    if (value->has_edge) {
      out->edge_bin = value->edge_bin;
      out->edge_deg = value->edge_delta_deg;
    }
    out->peak_available = value->has_peak;
    if (value->has_peak) {
      out->peak_deg = value->peak_delta_deg;
      out->peak_value = value->peak_value;
    }
    out->separation_available = value->has_edge && value->has_peak;
    if (out->separation_available)
      out->separation_deg = value->separation_deg;
    out->kept_count = value->kept_count;
  }
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetProbeSample(const LUMICE_PathSurvey* result, size_t probe, size_t index,
                                                      LUMICE_PathProbeSample* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& probes = result->result.Data().probes;
  if (probe >= probes.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto* reading = probes[probe].ValueOrNull();
  if (!reading) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  if (index >= reading->samples.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& sample = reading->samples[index];
  out->delta_deg = sample.delta_deg;
  out->value = sample.value;
  out->k = sample.k;
  out->k_eff = sample.k_eff;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathSurveyGetWitness(const LUMICE_PathSurvey* result, size_t object,
                                                  LUMICE_PathWitness* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& objects = result->result.Data().objects;
  if (object >= objects.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  if (objects[object].Kind() != lumice::raypath::survey::SurveyObjectKind::kSupportSet) {
    return LUMICE_PATH_READ_INVALID_KIND;
  }
  const auto* witness = objects[object].Witness();
  if (!witness) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  out->state = EvidenceTag(witness->State());
  if (out->state == LUMICE_PATH_STATE_REFUSED) {
    out->mode = Text(witness->Mode());
  }
  if (const auto* value = witness->ValueOrNull()) {
    out->value_available = 1;
    out->kept_count = value->kept_count;
    out->sample_count = value->sample_count;
    out->lattice = Text(value->lattice);
    out->deterministic = value->deterministic;
    out->deviation_deg = value->deviation_deg;
    out->weight = value->weight;
    out->entry = value->entry;
    out->interface_product = value->interface_product;
    out->source_note = Text(value->source_note);
  }
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetRecordAdjuncts(const LUMICE_PathFeatureReport* result, size_t record,
                                                         LUMICE_PathRecordAdjuncts* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().mc.discovery.features;
  if (record >= rows.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& row = rows[record];
  out->band_available = row.band.has_value();
  if (row.band) {
    out->band_lower_count = row.band->boundaries[0].size();
    out->band_upper_count = row.band->boundaries[1].size();
  }
  out->boundary_source_available = row.boundary_source.has_value();
  out->boundary_value_available = row.boundary_value.has_value();
  out->interface_event_available = row.interface_event.has_value();
  out->paired_interface_available = row.paired_interface.has_value();
  out->source_event_available = row.source_event.has_value();
  out->deviation_minimum_available = row.deviation_minimum.has_value();
  out->orbit_axis_available = row.orbit_axis.has_value();
  if (row.orbit_axis) {
    std::copy(row.orbit_axis->begin(), row.orbit_axis->end(), out->orbit_axis);
    out->orbit_begin_rad = row.orbit_begin_rad;
    out->orbit_end_rad = row.orbit_end_rad;
  }
  std::copy(row.atom_xyz_mass.begin(), row.atom_xyz_mass.end(), out->atom_xyz_mass);
  out->observation_contrast_error = row.observation_contrast_error;
  out->transverse_contrast = row.transverse_contrast;
  out->prefix_movement_rad = row.prefix_movement_rad;
  out->scale_status = static_cast<int>(row.scale_status) + 1;
  out->walk_stop = static_cast<int>(row.walk_stop) + 1;
  out->internal_slot = row.internal_slot;
  return LUMICE_PATH_READ_OK;
}

namespace {

void FillFieldPoint(const lumice::analytic::FieldStationaryPoint& point, LUMICE_PathFieldPoint* out) {
  out->status = static_cast<int>(point.status) + 1;
  std::copy(point.query.direction.begin(), point.query.direction.end(), out->direction);
  for (int i = 0; i < 2; ++i) {
    std::copy(point.query.basis[i].begin(), point.query.basis[i].end(), out->basis + 3 * i);
  }
  out->bandwidth_rad = point.query.bandwidth_rad;
  out->effective_samples_y = point.field.effective_samples_y;
  out->outer_sample_count = point.field.outer_sample_count;
  out->chromaticity_available = point.field.chromaticity_available;
  std::copy(point.normal.begin(), point.normal.end(), out->normal);
  std::copy(point.log_y_curvatures.begin(), point.log_y_curvatures.end(), out->log_y_curvatures);
  out->correction_rad = point.correction_rad;
  out->travelled_rad = point.travelled_rad;
  out->iterations = point.iterations;
}

LUMICE_PathReadStatus FillFieldJet(const lumice::analytic::SphericalFieldValue& field, int channel,
                                   LUMICE_PathFieldJet* out) {
  if (channel < 0 || channel > 4) {
    return LUMICE_PATH_READ_INVALID_KIND;
  }
  if (channel >= 3 && !field.chromaticity_available) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  const auto& jet = channel < 3 ? field.xyz[static_cast<size_t>(channel)] : field.xy[static_cast<size_t>(channel - 3)];
  out->value = jet.value;
  std::copy(jet.gradient.begin(), jet.gradient.end(), out->gradient);
  std::copy(jet.hessian.begin(), jet.hessian.end(), out->hessian);
  return LUMICE_PATH_READ_OK;
}

}  // namespace

LUMICE_PathReadStatus LUMICE_PathReportGetFieldPoint(const LUMICE_PathFeatureReport* result, size_t record,
                                                     size_t index, LUMICE_PathFieldPoint* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().mc.discovery.features;
  if (record >= rows.size() || index >= rows[record].field_points.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  FillFieldPoint(rows[record].field_points[index], out);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetFieldJet(const LUMICE_PathFeatureReport* result, size_t record, size_t point,
                                                   int channel, LUMICE_PathFieldJet* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().mc.discovery.features;
  if (record >= rows.size() || point >= rows[record].field_points.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  return FillFieldJet(rows[record].field_points[point].field, channel, out);
}

LUMICE_PathReadStatus LUMICE_PathReportGetFieldBand(const LUMICE_PathFeatureReport* result, size_t record,
                                                    LUMICE_PathFieldBand* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& rows = result->result.Data().mc.discovery.features;
  if (record >= rows.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  if (!rows[record].band)
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  const auto& band = *rows[record].band;
  out->status = static_cast<int>(band.status) + 1;
  std::copy(band.levels.begin(), band.levels.end(), out->levels);
  for (size_t i = 0; i < 2; ++i)
    out->boundary_count[i] = band.boundaries[i].size();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetBandPoint(const LUMICE_PathFeatureReport* result, size_t record,
                                                    size_t boundary, size_t index, LUMICE_PathFieldPoint* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& rows = result->result.Data().mc.discovery.features;
  if (record >= rows.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  if (boundary >= 2)
    return LUMICE_PATH_READ_INVALID_INDEX;
  if (!rows[record].band)
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  const auto& points = rows[record].band->boundaries[boundary];
  if (index >= points.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  FillFieldPoint(points[index], out);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetBandJet(const LUMICE_PathFeatureReport* result, size_t record,
                                                  size_t boundary, size_t point, int channel,
                                                  LUMICE_PathFieldJet* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& rows = result->result.Data().mc.discovery.features;
  if (record >= rows.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  if (boundary >= 2)
    return LUMICE_PATH_READ_INVALID_INDEX;
  if (!rows[record].band)
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  const auto& points = rows[record].band->boundaries[boundary];
  if (point >= points.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  return FillFieldJet(points[point].field, channel, out);
}

namespace {

void FillDiagnosticSource(const lumice::analytic::DiagnosticInputRow& source, LUMICE_PathDiagnosticSource* out) {
  out->crystal_kind = static_cast<int>(source.crystal.kind) + 1;
  out->height = source.crystal.height;
  std::copy_n(source.crystal.face_distance, 6, out->face_distance);
  out->upper_h = source.crystal.upper_h;
  out->lower_h = source.crystal.lower_h;
  out->upper_wedge_deg = source.crystal.upper_wedge_deg;
  out->lower_wedge_deg = source.crystal.lower_wedge_deg;
  std::copy(source.pose.begin(), source.pose.end(), out->pose);
  std::copy(source.incident.begin(), source.incident.end(), out->incident);
  out->refractive_index = source.refractive_index;
  out->source_token = source.source_token;
}

void FillDiagnosticValue(const lumice::analytic::DiagnosticOutputRow& value, LUMICE_PathDiagnosticValue* out) {
  out->source_token = value.source_token;
  out->input_status = static_cast<int>(value.input_status) + 1;
  out->path_evaluations = value.path_evaluations;
  out->optical_failure = static_cast<int>(value.optical_failure) + 1;
  out->path_valid = value.path_valid;
  if (value.path_valid)
    std::copy(value.outgoing.begin(), value.outgoing.end(), out->outgoing);
  out->direction_jacobian_available = value.direction_jacobian_available;
  if (value.direction_jacobian_available) {
    for (int i = 0; i < 3; ++i) {
      std::copy(value.direction_pose_jacobian[i].begin(), value.direction_pose_jacobian[i].end(),
                out->direction_pose_jacobian + 3 * i);
    }
  }
  out->direction_index_available = value.direction_index_available;
  if (value.direction_index_available) {
    std::copy(value.direction_index_derivative.begin(), value.direction_index_derivative.end(),
              out->direction_index_derivative);
    out->direction_index_error = value.direction_index_error;
  }
  out->interface_product = value.interface_product;
  out->entry_available = value.entry_available;
  if (value.entry_available) {
    out->entry_status = static_cast<int>(value.entry.status) + 1;
    if (value.entry.status == lumice::analytic::EntryMeasureStatus::kOk) {
      out->entry_measure = value.entry.value;
      out->entry_area_perp_internal = value.entry.area_perp_internal;
    }
  }
  out->interface_count = value.interfaces.size();
  out->corridor_vertex_count = value.corridor.vertices.size();
  out->corridor_edge_count = value.corridor.edge_sources.size();
}

void FillDiagnosticInterface(const lumice::analytic::DiagnosticInterface& row, LUMICE_PathDiagnosticInterface* out) {
  out->reached = row.reached;
  if (row.reached) {
    out->incidence = row.incidence;
    out->discriminant = row.discriminant;
  }
  out->pose_gradient_available = row.pose_gradient_available;
  if (row.pose_gradient_available) {
    std::copy(row.discriminant_pose_gradient.begin(), row.discriminant_pose_gradient.end(),
              out->discriminant_pose_gradient);
  }
  out->index_derivative_available = row.index_derivative_available;
  if (row.index_derivative_available) {
    out->discriminant_index_derivative = row.discriminant_index_derivative;
    out->index_derivative_error = row.index_derivative_error;
  }
  out->factor_available = row.factor_available;
  if (row.factor_available)
    out->factor = row.factor;
}

void FillInterfacePoint(const lumice::analytic::InterfaceStationaryPoint& point, LUMICE_PathInterfacePoint* out) {
  out->status = static_cast<int>(point.status) + 1;
  out->travelled_rad = point.travelled_rad;
  out->path_evaluations = point.path_evaluations;
  out->accepted_pose_count = point.accepted_poses.size();
}

void FillEventBracket(const lumice::analytic::InterfaceEventBracket& event, LUMICE_PathEventBracket* out) {
  out->kind = static_cast<int>(event.kind) + 1;
  out->source_width_rad = event.source_width_rad;
  out->positive_status = static_cast<int>(event.positive.status) + 1;
  out->nonpositive_status = static_cast<int>(event.nonpositive.status) + 1;
}

struct DiagnosticSelection {
  LUMICE_PathReadStatus status = LUMICE_PATH_READ_OK;
  const lumice::analytic::DiagnosticInputRow* source = nullptr;
  const lumice::analytic::DiagnosticOutputRow* value = nullptr;
  const lumice::analytic::InterfaceStationaryPoint* station = nullptr;
};

DiagnosticSelection SelectDiagnostic(const LUMICE_PathFeatureReport* result, size_t record, int site, size_t curve,
                                     size_t point) {
  const auto& rows = result->result.Data().mc.discovery.features;
  if (record >= rows.size())
    return { LUMICE_PATH_READ_INVALID_INDEX };
  const auto& row = rows[record];
  if (site < LUMICE_PATH_DIAGNOSTIC_BOUNDARY || site > LUMICE_PATH_DIAGNOSTIC_DEVIATION_MINIMUM)
    return { LUMICE_PATH_READ_INVALID_KIND };
  if (site == LUMICE_PATH_DIAGNOSTIC_CURVE_POINT || site == LUMICE_PATH_DIAGNOSTIC_CURVE_EVENT_POSITIVE ||
      site == LUMICE_PATH_DIAGNOSTIC_CURVE_EVENT_NONPOSITIVE) {
    if (curve >= row.interface_curves.size())
      return { LUMICE_PATH_READ_INVALID_INDEX };
    const auto& path = row.interface_curves[curve];
    if (site == LUMICE_PATH_DIAGNOSTIC_CURVE_POINT) {
      if (point >= path.points.size())
        return { LUMICE_PATH_READ_INVALID_INDEX };
      const auto& station = path.points[point];
      return { LUMICE_PATH_READ_OK, &station.source, &station.value, &station };
    }
    if (point >= path.events.size())
      return { LUMICE_PATH_READ_INVALID_INDEX };
    const auto& station = site == LUMICE_PATH_DIAGNOSTIC_CURVE_EVENT_POSITIVE ? path.events[point].positive :
                                                                                path.events[point].nonpositive;
    return { LUMICE_PATH_READ_OK, &station.source, &station.value, &station };
  }
  if (curve != 0 || point != 0)
    return { LUMICE_PATH_READ_INVALID_INDEX };
  if (site == LUMICE_PATH_DIAGNOSTIC_BOUNDARY) {
    if (!row.boundary_source || !row.boundary_value)
      return { LUMICE_PATH_READ_NOT_AVAILABLE };
    return { LUMICE_PATH_READ_OK, &*row.boundary_source, &*row.boundary_value };
  }
  if (site == LUMICE_PATH_DIAGNOSTIC_INTERFACE_EVENT) {
    if (!row.interface_event)
      return { LUMICE_PATH_READ_NOT_AVAILABLE };
    const auto& station = *row.interface_event;
    return { LUMICE_PATH_READ_OK, &station.source, &station.value, &station };
  }
  if (site == LUMICE_PATH_DIAGNOSTIC_SOURCE_EVENT_POSITIVE || site == LUMICE_PATH_DIAGNOSTIC_SOURCE_EVENT_NONPOSITIVE) {
    if (!row.source_event)
      return { LUMICE_PATH_READ_NOT_AVAILABLE };
    const auto& station = site == LUMICE_PATH_DIAGNOSTIC_SOURCE_EVENT_POSITIVE ? row.source_event->positive :
                                                                                 row.source_event->nonpositive;
    return { LUMICE_PATH_READ_OK, &station.source, &station.value, &station };
  }
  if (!row.deviation_minimum)
    return { LUMICE_PATH_READ_NOT_AVAILABLE };
  return { LUMICE_PATH_READ_OK, &row.deviation_minimum->source, &row.deviation_minimum->value };
}

}  // namespace

LUMICE_PathReadStatus LUMICE_PathReportGetInterfaceEvent(const LUMICE_PathFeatureReport* result, size_t record,
                                                         LUMICE_PathInterfacePoint* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& rows = result->result.Data().mc.discovery.features;
  if (record >= rows.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  if (!rows[record].interface_event)
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  FillInterfacePoint(*rows[record].interface_event, out);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetInterfaceCurve(const LUMICE_PathFeatureReport* result, size_t record,
                                                         size_t curve, LUMICE_PathInterfaceCurve* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& rows = result->result.Data().mc.discovery.features;
  if (record >= rows.size() || curve >= rows[record].interface_curves.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  const auto& path = rows[record].interface_curves[curve];
  out->stop = static_cast<int>(path.stop) + 1;
  out->path_evaluations = path.path_evaluations;
  out->point_count = path.points.size();
  out->event_count = path.events.size();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetCurvePoint(const LUMICE_PathFeatureReport* result, size_t record,
                                                     size_t curve, size_t point, LUMICE_PathInterfacePoint* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& rows = result->result.Data().mc.discovery.features;
  if (record >= rows.size() || curve >= rows[record].interface_curves.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  const auto& points = rows[record].interface_curves[curve].points;
  if (point >= points.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  FillInterfacePoint(points[point], out);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetCurveEvent(const LUMICE_PathFeatureReport* result, size_t record,
                                                     size_t curve, size_t event, LUMICE_PathEventBracket* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& rows = result->result.Data().mc.discovery.features;
  if (record >= rows.size() || curve >= rows[record].interface_curves.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  const auto& events = rows[record].interface_curves[curve].events;
  if (event >= events.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  FillEventBracket(events[event], out);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetSourceEvent(const LUMICE_PathFeatureReport* result, size_t record,
                                                      LUMICE_PathEventBracket* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& rows = result->result.Data().mc.discovery.features;
  if (record >= rows.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  if (!rows[record].source_event)
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  FillEventBracket(*rows[record].source_event, out);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetDeviationMinimum(const LUMICE_PathFeatureReport* result, size_t record,
                                                           LUMICE_PathDeviationMinimum* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& rows = result->result.Data().mc.discovery.features;
  if (record >= rows.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  if (!rows[record].deviation_minimum)
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  const auto& point = *rows[record].deviation_minimum;
  out->status = static_cast<int>(point.status) + 1;
  out->deviation_available = point.deviation_available;
  if (point.deviation_available) {
    out->deviation_rad = point.deviation_rad;
    std::copy(point.objective_curvatures.begin(), point.objective_curvatures.end(), out->objective_curvatures);
    out->hessian_error = point.hessian_error;
    out->correction_rad = point.correction_rad;
  }
  out->path_evaluations = point.path_evaluations;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetPairedInterface(const LUMICE_PathFeatureReport* result, size_t record,
                                                          LUMICE_PathPairedInterface* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& rows = result->result.Data().mc.discovery.features;
  if (record >= rows.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  if (!rows[record].paired_interface)
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  const auto& paired = *rows[record].paired_interface;
  out->chromaticity_available = paired.chromaticity_available;
  out->complete = paired.complete;
  std::copy(paired.actual_xyz.begin(), paired.actual_xyz.end(), out->actual_xyz);
  std::copy(paired.without_slot_xyz.begin(), paired.without_slot_xyz.end(), out->without_slot_xyz);
  if (paired.chromaticity_available)
    std::copy(paired.xy_difference.begin(), paired.xy_difference.end(), out->xy_difference);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetDiagnosticSource(const LUMICE_PathFeatureReport* result, size_t record,
                                                           int site, size_t curve, size_t point,
                                                           LUMICE_PathDiagnosticSource* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto selection = SelectDiagnostic(result, record, site, curve, point);
  if (selection.status != LUMICE_PATH_READ_OK)
    return selection.status;
  FillDiagnosticSource(*selection.source, out);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetDiagnosticValue(const LUMICE_PathFeatureReport* result, size_t record,
                                                          int site, size_t curve, size_t point,
                                                          LUMICE_PathDiagnosticValue* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto selection = SelectDiagnostic(result, record, site, curve, point);
  if (selection.status != LUMICE_PATH_READ_OK)
    return selection.status;
  FillDiagnosticValue(*selection.value, out);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetDiagnosticInterface(const LUMICE_PathFeatureReport* result, size_t record,
                                                              int site, size_t curve, size_t point, size_t index,
                                                              LUMICE_PathDiagnosticInterface* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto selection = SelectDiagnostic(result, record, site, curve, point);
  if (selection.status != LUMICE_PATH_READ_OK)
    return selection.status;
  if (index >= selection.value->interfaces.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  FillDiagnosticInterface(selection.value->interfaces[index], out);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetAcceptedPose(const LUMICE_PathFeatureReport* result, size_t record, int site,
                                                       size_t curve, size_t point, size_t index,
                                                       LUMICE_PathAcceptedPose* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto selection = SelectDiagnostic(result, record, site, curve, point);
  if (selection.status != LUMICE_PATH_READ_OK)
    return selection.status;
  if (!selection.station)
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  if (index >= selection.station->accepted_poses.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  std::copy(selection.station->accepted_poses[index].begin(), selection.station->accepted_poses[index].end(),
            out->pose);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetCorridor(const LUMICE_PathFeatureReport* result, size_t record, int site,
                                                   size_t curve, size_t point, LUMICE_PathCorridor* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto selection = SelectDiagnostic(result, record, site, curve, point);
  if (selection.status != LUMICE_PATH_READ_OK)
    return selection.status;
  const auto& corridor = selection.value->corridor;
  out->geometry_evaluated = corridor.geometry_evaluated;
  if (corridor.geometry_evaluated) {
    out->raw_area = corridor.raw_area;
    out->area_threshold = corridor.area_threshold;
    for (int i = 0; i < 2; ++i)
      std::copy(corridor.projection_basis[i].begin(), corridor.projection_basis[i].end(),
                out->projection_basis + 3 * i);
  }
  out->vertex_count = corridor.vertices.size();
  out->edge_source_count = corridor.edge_sources.size();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetCorridorVertex(const LUMICE_PathFeatureReport* result, size_t record,
                                                         int site, size_t curve, size_t point, size_t index,
                                                         LUMICE_PathCorridorVertex* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto selection = SelectDiagnostic(result, record, site, curve, point);
  if (selection.status != LUMICE_PATH_READ_OK)
    return selection.status;
  const auto& vertices = selection.value->corridor.vertices;
  if (index >= vertices.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  std::copy(vertices[index].begin(), vertices[index].end(), out->xy);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetCorridorEdge(const LUMICE_PathFeatureReport* result, size_t record, int site,
                                                       size_t curve, size_t point, size_t index,
                                                       LUMICE_PathCorridorEdge* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto selection = SelectDiagnostic(result, record, site, curve, point);
  if (selection.status != LUMICE_PATH_READ_OK)
    return selection.status;
  const auto& edges = selection.value->corridor.edge_sources;
  if (index >= edges.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  out->path_index = edges[index].path_index;
  out->edge_index = edges[index].edge_index;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetBoundarySource(const LUMICE_PathFeatureReport* result, size_t record,
                                                         LUMICE_PathDiagnosticSource* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().mc.discovery.features;
  if (record >= rows.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& source = rows[record].boundary_source;
  if (!source) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  FillDiagnosticSource(*source, out);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetBoundaryValue(const LUMICE_PathFeatureReport* result, size_t record,
                                                        LUMICE_PathDiagnosticValue* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().mc.discovery.features;
  if (record >= rows.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& value = rows[record].boundary_value;
  if (!value) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  FillDiagnosticValue(*value, out);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetBoundaryInterface(const LUMICE_PathFeatureReport* result, size_t record,
                                                            size_t index, LUMICE_PathDiagnosticInterface* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().mc.discovery.features;
  if (record >= rows.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& value = rows[record].boundary_value;
  if (!value) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  if (index >= value->interfaces.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& row = value->interfaces[index];
  FillDiagnosticInterface(row, out);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetMcUnfinished(const LUMICE_PathFeatureReport* result, size_t index,
                                                       LUMICE_PathText* out) {
  if (!out) {
    return LUMICE_PATH_READ_NULL_ARG;
  }
  *out = {};
  if (!result) {
    return LUMICE_PATH_READ_NULL_ARG;
  }
  const auto& rows = result->result.Data().mc.discovery.unfinished;
  if (index >= rows.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  *out = Text(rows[index]);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetMcLimitation(const LUMICE_PathFeatureReport* result, size_t index,
                                                       LUMICE_PathText* out) {
  if (!out) {
    return LUMICE_PATH_READ_NULL_ARG;
  }
  *out = {};
  if (!result) {
    return LUMICE_PATH_READ_NULL_ARG;
  }
  const auto& rows = result->result.Data().mc.discovery.limitations;
  if (index >= rows.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  *out = Text(rows[index]);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetMcEvidence(const LUMICE_PathFeatureReport* result,
                                                     LUMICE_PathReportMcEvidence* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& mc = result->result.Data().mc;
  const auto& d = mc.discovery;
  out->event_path_evaluations = d.event_path_evaluations;
  out->replicate_path_evaluations = d.replicate_path_evaluations;
  out->replicate_samples = d.replicate_samples;
  out->field_component_evaluations = d.field_component_evaluations;
  out->budget_exhausted = d.budget_exhausted;
  out->completed_samples = d.measure.completed_samples;
  out->optical_evaluations = d.measure.optical_evaluations;
  out->component_count = d.measure.components.size();
  out->positive_source_count = d.positive_sources.size();
  out->unfinished_count = d.unfinished.size();
  out->limitation_count = d.limitations.size();
  out->bandwidth_rad = mc.observation_options.bandwidth_rad;
  out->location_resolution_rad = mc.observation_options.location_resolution_rad;
  out->observation_scope_note = Text(mc.observation_scope_note);
  const auto& spectral = mc.spectral_verification;
  out->spectral_available = spectral.available;
  out->spectral_movement_available = spectral.movement_rad.has_value();
  if (spectral.movement_rad) {
    out->spectral_movement_rad = *spectral.movement_rad;
  }
  out->spectral_optical_evaluations = spectral.optical_evaluations;
  out->spectral_field_evaluations = spectral.field_component_evaluations;
  out->spectral_seconds = spectral.seconds;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetMcComponent(const LUMICE_PathFeatureReport* result, size_t index,
                                                      LUMICE_PathMcComponent* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().mc.discovery.measure.components;
  if (index >= rows.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& row = rows[index];
  out->sample_index = row.sample_index;
  std::copy(row.direction.begin(), row.direction.end(), out->direction);
  std::copy(row.xyz_weight.begin(), row.xyz_weight.end(), out->xyz_weight);
  out->orbit_axis_available = row.uniform_orbit_axis.has_value();
  if (row.uniform_orbit_axis) {
    std::copy(row.uniform_orbit_axis->begin(), row.uniform_orbit_axis->end(), out->orbit_axis);
  }
  out->source_token = row.source_token;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetPositiveSource(const LUMICE_PathFeatureReport* result, size_t index,
                                                         LUMICE_PathPositiveSource* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().mc.discovery.positive_sources;
  if (index >= rows.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& row = rows[index];
  out->token = row.token;
  out->sample_index = row.source.sample_index;
  out->member_index = row.source.member_index;
  out->spectral_row = row.source.spectral_row;
  out->coordinates_available = row.coordinates.has_value();
  if (row.coordinates) {
    const auto& crystal = row.coordinates->crystal;
    out->crystal_kind = static_cast<int>(crystal.kind) + 1;
    out->height = crystal.height;
    std::copy_n(crystal.face_distance, 6, out->face_distance);
    out->upper_h = crystal.upper_h;
    out->lower_h = crystal.lower_h;
    out->upper_wedge_deg = crystal.upper_wedge_deg;
    out->lower_wedge_deg = crystal.lower_wedge_deg;
    std::copy(row.coordinates->pose.begin(), row.coordinates->pose.end(), out->pose);
    std::copy(row.coordinates->incident.begin(), row.coordinates->incident.end(), out->incident);
    out->refractive_index = row.coordinates->refractive_index;
  }
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetRequestedLayer(const LUMICE_PathFeatureReport* result, size_t index,
                                                         LUMICE_PathFaceSequence* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& layers = result->result.Data().requested_path_layers;
  if (index >= layers.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  out->faces = layers[index].data();
  out->face_count = layers[index].size();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetScope(const LUMICE_PathFeatureReport* result, LUMICE_PathReportScope* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  if (result->result.Data().outcome == lumice::raypath::ReportOutcome::kUnsupportedMulticrystal)
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  const auto& scope = result->result.Data().scope;
  out->scene_identity = Text(scope.scene_identity);
  out->spectrum_scope = Text(scope.spectrum_scope);
  out->seed = scope.seed;
  out->member_kind = static_cast<int>(scope.member_kind) + 1;
  out->location = static_cast<int>(scope.location.location) + 1;
  if (scope.location.location == lumice::raypath::SelectionLocation::kSceneLayer) {
    out->layer_index = scope.location.layer_index;
  }
  if (const auto* discrete = std::get_if<std::vector<lumice::WlParam>>(&scope.light.spectrum_)) {
    out->raw_spectrum_kind = 1;
    out->raw_spectrum_count = discrete->size();
  } else {
    out->raw_spectrum_kind = 2;
    out->raw_illuminant = static_cast<int>(std::get<lumice::IlluminantType>(scope.light.spectrum_)) + 1;
  }
  out->sun_altitude_deg = scope.light.param_.altitude_;
  out->sun_azimuth_deg = scope.light.param_.azimuth_;
  out->sun_diameter_deg = scope.light.param_.diameter_;
  out->pose_coordinate_count = scope.support.pose_coordinate_count;
  out->pose_support_dimension = scope.support.pose_support_dimension;
  out->shape_parameter_dimension = scope.support.shape_parameter_dimension;
  out->source_direction_dimension = scope.support.source_direction_dimension;
  out->spectral_dimension = scope.support.spectral_dimension;
  out->layer_count = scope.layers.size();
  out->member_count = scope.physical_members.size();
  out->spectrum_row_count = scope.spectrum.size();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetLayer(const LUMICE_PathFeatureReport* result, size_t index,
                                                LUMICE_PathReportLayer* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& layers = result->result.Data().scope.layers;
  if (index >= layers.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  const auto& layer = layers[index];
  out->scene_layer_available = layer.scene_layer.has_value();
  if (layer.scene_layer)
    out->scene_layer = *layer.scene_layer;
  out->crystal_id = layer.crystal.id_;
  out->crystal_kind = std::holds_alternative<lumice::PrismCrystalParam>(layer.crystal.param_) ? 1 : 2;
  out->symmetry_bits = layer.symmetry_bits;
  out->representative_faces = layer.representative_faces.data();
  out->representative_face_count = layer.representative_faces.size();
  out->shape_scalar_count = lumice::kShapeScalarCount;
  if (const auto* pyramid = std::get_if<lumice::PyramidCrystalParam>(&layer.crystal.param_)) {
    out->wedge_available = 1;
    out->upper_wedge_deg = pyramid->wedge_angle_u_;
    out->lower_wedge_deg = pyramid->wedge_angle_l_;
  }
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetLayerAxis(const LUMICE_PathFeatureReport* result, size_t layer, size_t axis,
                                                    LUMICE_PathReportAxis* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& layers = result->result.Data().scope.layers;
  if (layer >= layers.size() || axis > 2)
    return LUMICE_PATH_READ_INVALID_INDEX;
  const auto& orientation = layers[layer].crystal.axis_;
  const auto& value = axis == 0 ? orientation.azimuth_dist :
                      axis == 1 ? orientation.latitude_dist :
                                  orientation.roll_dist;
  out->distribution = static_cast<int>(value.type) + 1;
  out->center = value.center;
  out->spread = value.spread;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetLayerShape(const LUMICE_PathFeatureReport* result, size_t layer, size_t slot,
                                                     LUMICE_PathReportShapeScalar* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& layers = result->result.Data().scope.layers;
  if (layer >= layers.size() || slot >= lumice::kShapeScalarCount)
    return LUMICE_PATH_READ_INVALID_INDEX;
  const auto kind = std::holds_alternative<lumice::PrismCrystalParam>(layers[layer].crystal.param_) ?
                        lumice::CrystalKind::kPrism :
                        lumice::CrystalKind::kPyramid;
  if (!lumice::IsShapeScalarApplicable(kind, static_cast<int>(slot)))
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  out->name = Text(lumice::ShapeScalarSyncKeyName(kind, static_cast<int>(slot)));
  std::visit(
      [&](const auto& original) {
        auto shape = original;
        const auto slots = lumice::GetShapeScalarSlots(shape);
        const auto* dist = slots[slot];
        out->distribution = static_cast<int>(dist->type) + 1;
        out->center = dist->center;
        out->spread = dist->spread;
        out->sync_group = shape.sync_group_[slot];
      },
      layers[layer].crystal.param_);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetRawSpectrumRow(const LUMICE_PathFeatureReport* result, size_t index,
                                                         LUMICE_PathRawSpectrumRow* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto* spectrum = std::get_if<std::vector<lumice::WlParam>>(&result->result.Data().scope.light.spectrum_);
  if (!spectrum)
    return LUMICE_PATH_READ_INVALID_KIND;
  if (index >= spectrum->size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  out->wavelength_nm = (*spectrum)[index].wl_;
  out->weight = (*spectrum)[index].weight_;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetMember(const LUMICE_PathFeatureReport* result, size_t index,
                                                 LUMICE_PathFaceSequence* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().scope.physical_members;
  if (index >= rows.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  out->faces = rows[index].data();
  out->face_count = rows[index].size();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetSpectralRow(const LUMICE_PathFeatureReport* result, size_t index,
                                                      LUMICE_PathSpectralRow* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().scope.spectrum;
  if (index >= rows.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& row = rows[index];
  out->wavelength_nm = row.wavelength_nm;
  out->source_weight = row.source_weight;
  out->measure_mass = row.measure_mass;
  out->refractive_index = row.refractive_index;
  std::copy(row.xyz_basis.begin(), row.xyz_basis.end(), out->xyz_basis);
  std::copy(row.coefficient.begin(), row.coefficient.end(), out->coefficient);
  out->origin = static_cast<int>(row.origin) + 1;
  out->provenance = Text(row.provenance);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetSupportMember(const LUMICE_PathFeatureReport* result, size_t index,
                                                        LUMICE_PathReportSupportMember* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().core.support.members;
  if (index >= rows.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& row = rows[index];
  out->member = row.member.data();
  out->face_count = row.member.size();
  out->regime_slug = Text(row.axis.regime_slug);
  out->message = Text(row.axis.message);
  out->coverage = static_cast<int>(row.axis.context.coverage) + 1;
  out->escape_regime_available =
      row.axis.context.coverage == lumice::raypath::PartitionContext::Coverage::kIncomplete &&
      row.axis.context.escape_regime != lumice::raypath::EscapeRegime::kUnset;
  if (out->escape_regime_available) {
    out->escape_regime = static_cast<int>(row.axis.context.escape_regime) + 1;
  }
  out->walk_status = static_cast<int>(row.axis.walk_status) + 1;
  out->walk_closed = row.axis.walk_closed;
  out->interval_count = row.axis.intervals.size();
  out->onset_count = row.endpoint_onsets.size();
  out->constant_curve_count = row.constant_curves.size();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetSupportInterval(const LUMICE_PathFeatureReport* result, size_t member,
                                                          size_t index, LUMICE_PathReportInterval* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().core.support.members;
  if (member >= rows.size() || index >= rows[member].axis.intervals.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& interval = rows[member].axis.intervals[index];
  out->lower_rad = interval.lower;
  out->upper_rad = interval.upper;
  out->n_components = interval.n_components;
  out->n_closed = interval.n_closed;
  out->n_open = interval.n_open;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetOrbit(const LUMICE_PathFeatureReport* result, size_t index,
                                                LUMICE_PathReportOrbit* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& rows = result->result.Data().core.orbits;
  if (index >= rows.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  const auto& orbit = rows[index];
  out->binding = static_cast<int>(orbit.binding) + 1;
  out->evidence = static_cast<int>(orbit.evidence) + 1;
  out->grid = orbit.grid;
  out->spin_degenerate = orbit.spin_degenerate;
  out->note = Text(orbit.note);
  out->source_name = Text(orbit.source_name);
  out->sample_count = orbit.samples.size();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetOrbitSample(const LUMICE_PathFeatureReport* result, size_t orbit,
                                                      size_t index, LUMICE_PathReportOrbitSample* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& rows = result->result.Data().core.orbits;
  if (orbit >= rows.size() || index >= rows[orbit].samples.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  const auto& sample = rows[orbit].samples[index];
  std::copy_n(sample.u, 3, out->u);
  std::copy_n(sample.outgoing, 3, out->outgoing);
  out->area = sample.area;
  out->transmission = sample.transmission;
  out->valid = sample.valid;
  out->jet_degenerate = sample.jet_degenerate;
  out->parameter = sample.parameter;
  out->weight = sample.weight;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetSupportOnset(const LUMICE_PathFeatureReport* result, size_t member,
                                                       size_t index, LUMICE_PathReportOnset* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().core.support.members;
  if (member >= rows.size() || index >= rows[member].endpoint_onsets.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& onset = rows[member].endpoint_onsets[index];
  out->value = onset.value;
  out->location = static_cast<int>(onset.location) + 1;
  out->source = static_cast<int>(onset.source) + 1;
  out->profile = static_cast<int>(onset.profile) + 1;
  out->gradient_norm = onset.gradient_norm;
  out->measure_limit_available = onset.has_measure_limit;
  if (onset.has_measure_limit) {
    out->measure_limit = onset.measure_limit;
  }
  out->multiplicity = onset.multiplicity;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetConstantCurve(const LUMICE_PathFeatureReport* result, size_t member,
                                                        size_t index, LUMICE_PathReportCurve* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().core.support.members;
  if (member >= rows.size() || index >= rows[member].constant_curves.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& curve = rows[member].constant_curves[index];
  out->d_p = curve.d_p;
  out->weight_step = curve.weight_step;
  out->wavelength_count = curve.wavelengths_nm.size();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetCurveWavelength(const LUMICE_PathFeatureReport* result, size_t member,
                                                          size_t curve, size_t index,
                                                          LUMICE_PathReportCurveWavelength* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().core.support.members;
  if (member >= rows.size() || curve >= rows[member].constant_curves.size() ||
      index >= rows[member].constant_curves[curve].wavelengths_nm.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& row = rows[member].constant_curves[curve];
  if (index >= row.critical_d_p.size()) {
    return LUMICE_PATH_READ_NOT_AVAILABLE;
  }
  out->wavelength_nm = row.wavelengths_nm[index];
  const double critical = row.critical_d_p[index];
  out->critical_available = std::isfinite(critical);
  if (out->critical_available) {
    out->critical_d_p = critical;
  }
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetSupportFamily(const LUMICE_PathFeatureReport* result, size_t index,
                                                        LUMICE_PathReportSupportFamily* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().core.support.families;
  if (index >= rows.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& family = rows[index];
  out->shared = family.shared;
  out->phi_class_note = Text(family.phi_class_note);
  out->interval_count = family.intervals.size();
  out->member_count = family.members.size();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetFamilyInterval(const LUMICE_PathFeatureReport* result, size_t family,
                                                         size_t index, LUMICE_PathReportInterval* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().core.support.families;
  if (family >= rows.size() || index >= rows[family].intervals.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& interval = rows[family].intervals[index];
  out->lower_rad = interval.lower;
  out->upper_rad = interval.upper;
  out->n_components = interval.n_components;
  out->n_closed = interval.n_closed;
  out->n_open = interval.n_open;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetFamilyMember(const LUMICE_PathFeatureReport* result, size_t family,
                                                       size_t index, LUMICE_PathFaceSequence* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().core.support.families;
  if (family >= rows.size() || index >= rows[family].members.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& member = rows[family].members[index];
  out->faces = member.data();
  out->face_count = member.size();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetBudgets(const LUMICE_PathFeatureReport* result,
                                                  LUMICE_PathReportBudgets* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& d = result->result.Data();
  const auto& b = d.budgets;
  out->requested_outer_samples = b.requested_outer_samples;
  out->completed_outer_samples = b.completed_outer_samples;
  out->replicate_samples = b.replicate_samples;
  out->replicate_seed = b.replicate_seed;
  out->max_optical_evaluations = b.max_optical_evaluations;
  out->optical_evaluations = b.optical_evaluations;
  out->max_field_component_evaluations = b.max_field_component_evaluations;
  out->field_component_evaluations = b.field_component_evaluations;
  out->exhausted = b.exhausted;
  out->attribution_dp_evaluations = b.attribution_dp_evaluations;
  out->attribution_presence_ess_queries = b.attribution_presence_ess_queries;
  out->enumeration_max_optical_evaluations = d.core.budget.max_optical_evaluations;
  out->enumeration_sampling_evaluations = d.core.budget.sampling_evaluations;
  out->enumeration_hang_cap_ms = d.core.budget.hang_cap_ms;
  out->enumeration_truncated = d.core.budget.truncated;
  out->enumeration_truncation_note = Text(d.core.budget.truncation_note);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetAnnotation(const LUMICE_PathFeatureReport* result, size_t object,
                                                     LUMICE_PathReportAnnotation* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().annotations;
  if (object >= rows.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& row = rows[object];
  out->state = static_cast<int>(row.state) + 1;
  out->presence_ess_available = std::isfinite(row.presence_ess);
  if (out->presence_ess_available) {
    out->presence_ess = row.presence_ess;
  }
  out->match_distance_available = std::isfinite(row.match_distance);
  if (out->match_distance_available) {
    out->match_distance = row.match_distance;
  }
  out->tolerance_available = std::isfinite(row.tolerance_rad);
  if (out->tolerance_available) {
    out->tolerance_rad = row.tolerance_rad;
  }
  out->matched_record_available = row.matched_record >= 0;
  if (out->matched_record_available) {
    out->matched_record = static_cast<size_t>(row.matched_record);
  }
  out->matched_record_ess_available = std::isfinite(row.matched_record_ess);
  if (out->matched_record_ess_available) {
    out->matched_record_ess = row.matched_record_ess;
  }
  out->ruler = Text(row.ruler);
  out->reason = Text(row.reason);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetUnattributed(const LUMICE_PathFeatureReport* result, size_t index,
                                                       LUMICE_PathUnattributed* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().unattributed.structures;
  if (index >= rows.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& row = rows[index];
  out->record_index = row.record_index;
  std::copy(row.position.begin(), row.position.end(), out->position);
  out->delta_rad = row.delta_rad;
  out->record_ess = row.record_ess;
  out->min_margin_available = std::isfinite(row.min_margin_rad);
  if (out->min_margin_available) {
    out->min_margin_rad = row.min_margin_rad;
  }
  out->ruler = Text(row.ruler);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetUnattributedSummary(const LUMICE_PathFeatureReport* result,
                                                              LUMICE_PathUnattributedSummary* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& data = result->result.Data().unattributed;
  out->structure_count = data.structures.size();
  out->skipped_no_position = data.skipped_no_position;
  out->skipped_below_ess = data.skipped_below_ess;
  out->attribution_dp_evaluations = data.counts.dp_evaluations;
  out->attribution_presence_ess_queries = data.counts.presence_ess_queries;
  out->error = Text(data.error);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetSource(const LUMICE_PathFeatureReport* result, size_t index,
                                                 LUMICE_PathReportSource* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().mc.discovery.measure.sources;
  if (index >= rows.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  out->sample_index = rows[index].sample_index;
  out->member_index = rows[index].member_index;
  out->spectral_row = rows[index].spectral_row;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetSkyPoint(const LUMICE_PathFeatureReport* result, size_t record, size_t index,
                                                   LUMICE_PathReportSkyPoint* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().mc.discovery.features;
  if (record >= rows.size() || index >= rows[record].sky_points.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  std::copy(rows[record].sky_points[index].begin(), rows[record].sky_points[index].end(), out->direction);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetSourceParameter(const LUMICE_PathFeatureReport* result, size_t record,
                                                          size_t index, LUMICE_PathReportSourceParameter* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().mc.discovery.features;
  if (record >= rows.size() || index >= rows[record].source_parameters.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& parameter = rows[record].source_parameters[index];
  out->name = Text(parameter.first);
  out->value = parameter.second;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetCoverageLimitation(const LUMICE_PathFeatureReport* result, size_t coverage,
                                                             size_t index, LUMICE_PathText* out) {
  if (out == nullptr) {
    return LUMICE_PATH_READ_NULL_ARG;
  }
  *out = {};
  if (result == nullptr) {
    return LUMICE_PATH_READ_NULL_ARG;
  }
  const auto& rows = result->result.Data().coverage;
  if (coverage >= rows.size() || index >= rows[coverage].limitations.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  *out = Text(rows[coverage].limitations[index]);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetRuling(const LUMICE_PathFeatureReport* result, LUMICE_PathReportRuling* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& ruling = result->result.Data().ruling;
  out->issued = ruling.issued;
  out->basis_available = ruling.issued;
  if (ruling.issued) {
    out->basis = static_cast<int>(ruling.basis) + 1;
  }
  out->partition_complete_no_escape = ruling.partition_complete_no_escape;
  out->partition_failure = Text(ruling.partition_failure);
  out->all_objects_unlit_or_none = ruling.all_objects_unlit_or_none;
  out->unlit_failure = Text(ruling.unlit_failure);
  out->no_sufficient_ess_unattributed = ruling.no_sufficient_ess_unattributed;
  out->unattributed_failure = Text(ruling.unattributed_failure);
  out->s4_scope_declared = ruling.s4_scope_declared;
  out->s4_failure = Text(ruling.s4_failure);
  out->two_d_valid_support = ruling.two_d_valid_support;
  out->s4_scope_note = Text(ruling.s4_scope_note);
  out->absence_not_proven_note = Text(ruling.absence_not_proven_note);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetInfo(const LUMICE_PathFeatureReport* result, LUMICE_PathReportInfo* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& d = result->result.Data();
  out->execution = ExecutionTag(d.execution);
  out->outcome = static_cast<int>(d.outcome);
  out->object_count = d.core.objects.size();
  out->record_count = d.mc.discovery.features.size();
  out->source_count = d.mc.discovery.measure.sources.size();
  out->coverage_count = d.coverage.size();
  out->declared_skip_count = d.core.coverage.declared_not_produced.size();
  out->produced_kind_count = d.core.coverage.produced.size();
  out->support_member_count = d.core.support.members.size();
  out->support_family_count = d.core.support.families.size();
  out->orbit_count = d.core.orbits.size();
  out->unattributed_count = d.unattributed.structures.size();
  out->requested_outer_samples = d.budgets.requested_outer_samples;
  out->completed_outer_samples = d.budgets.completed_outer_samples;
  out->optical_evaluations = d.budgets.optical_evaluations;
  out->budget_ms = d.scope.budget_ms;
  out->absence_issued = d.ruling.issued;
  out->requested_layer_count = d.requested_path_layers.size();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetObject(const LUMICE_PathFeatureReport* result, size_t index,
                                                 LUMICE_PathReportObject* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& objects = result->result.Data().core.objects;
  if (index >= objects.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& object = objects[index];
  out->kind = Text(lumice::raypath::schema3::ObjectKindName(object.kind));
  out->member = object.member.data();
  out->member_face_count = object.member.size();
  out->slot = object.slot;
  out->phi_class_note = Text(object.phi_class_note);
  if (object.existence == lumice::raypath::ExistenceState::kEscaped) {
    out->escape_regime_slug = Text(object.escape_regime_slug);
  }
  out->bucket = static_cast<int>(lumice::raypath::schema3::BucketOf(object)) + 1;
  out->existence = static_cast<int>(object.existence) + 1;
  out->visibility = static_cast<int>(object.visibility.state) + 1;
  out->chromatic_assessed = object.ChromaticAssessed();
  out->corroboration = static_cast<int>(object.corroboration) + 1;
  out->sky_position_available = object.has_sky_position;
  if (object.has_sky_position) {
    std::copy_n(object.sky_position, 3, out->sky_position);
  }
  out->u = object.u.data();
  out->u_count = object.u.size();
  out->walk_s = object.walk_s;
  out->counterfactual_available = object.counterfactual.available;
  if (object.counterfactual.available) {
    out->counterfactual_with_slot = object.counterfactual.with_slot;
    out->counterfactual_without_slot = object.counterfactual.without_slot;
  }
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetVisibility(const LUMICE_PathFeatureReport* result, size_t object,
                                                     LUMICE_PathReportVisibility* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& rows = result->result.Data().core.objects;
  if (object >= rows.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  const auto& visibility = rows[object].visibility;
  out->state = static_cast<int>(visibility.state) + 1;
  out->state_name = Text(lumice::raypath::VisibilityStateName(visibility.state));
  out->lit_fraction_available = std::isfinite(visibility.lit_fraction);
  if (out->lit_fraction_available)
    out->lit_fraction = visibility.lit_fraction;
  out->evidence_form = Text(lumice::raypath::EvidenceFormName(visibility.evidence));
  out->jets_ok = visibility.jets_ok;
  out->saw_zero_area = visibility.saw_zero_area;
  out->saw_zero_transmission = visibility.saw_zero_transmission;
  out->reason = Text(visibility.reason);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetChromatic(const LUMICE_PathFeatureReport* result, size_t object,
                                                    LUMICE_PathReportChromatic* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& rows = result->result.Data().core.objects;
  if (object >= rows.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  const auto& row = rows[object];
  const auto& assessment = row.Chromatic();
  const auto& verdict = assessment.verdict;
  const auto& thresholds = assessment.thresholds;
  out->assessed = row.ChromaticAssessed();
  out->verdict_kind = Text(lumice::analytic::ChromaticVerdictKindName(verdict.kind));
  out->color = Text(lumice::analytic::ChromaticColorName(verdict.color));
  out->visible = verdict.visible;
  out->position_available = verdict.has_position;
  if (verdict.has_position)
    out->position_rad = verdict.position;
  out->faces = verdict.faces.data();
  out->face_count = verdict.faces.size();
  out->feature_count = verdict.features.size();
  out->note_count = verdict.notes.size();
  out->tint_available = verdict.has_tint;
  if (verdict.has_tint) {
    out->tint_energy_red = verdict.tint.energy_red;
    out->tint_energy_blue = verdict.tint.energy_blue;
    out->tint_ratio = verdict.tint.ratio;
    out->tint_tir_fraction_red = verdict.tint.tir_fraction_red;
    out->tint_tir_fraction_blue = verdict.tint.tir_fraction_blue;
    out->tint_direction_dispersion = verdict.tint.direction_dispersion;
  }
  out->coverage_complete = verdict.coverage_complete;
  out->n_red = verdict.n_red;
  out->n_blue = verdict.n_blue;
  out->threshold_n_red = thresholds.n_red;
  out->threshold_n_blue = thresholds.n_blue;
  out->edge_min_shift_rad = thresholds.edge_min_shift_rad;
  out->edge_spread_per_shift = thresholds.edge_spread_per_shift;
  out->calibration_white_max_deviation = thresholds.calibration_white_max_deviation;
  out->tint_ratio_min = thresholds.tint_ratio_min;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetChromaticFeature(const LUMICE_PathFeatureReport* result, size_t object,
                                                           size_t index, LUMICE_PathReportChromaticFeature* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK)
    return status;
  const auto& rows = result->result.Data().core.objects;
  if (object >= rows.size() || index >= rows[object].Chromatic().verdict.features.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  const auto& feature = rows[object].Chromatic().verdict.features[index];
  out->kind = Text(lumice::analytic::ChromaticFeatureKindName(feature.kind));
  out->source = Text(feature.source);
  out->color = Text(lumice::analytic::ChromaticColorName(feature.color));
  out->positive_fraction = feature.positive_fraction;
  out->delta_red = feature.delta_red;
  out->delta_blue = feature.delta_blue;
  out->shift = feature.shift;
  out->spread = feature.spread;
  out->direction_dispersion = feature.direction_dispersion;
  out->contrast = feature.contrast;
  out->weight = feature.weight;
  out->lit_fraction = feature.lit_fraction;
  out->visible = feature.visible;
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetChromaticNote(const LUMICE_PathFeatureReport* result, size_t object,
                                                        size_t index, LUMICE_PathText* out) {
  if (!out)
    return LUMICE_PATH_READ_NULL_ARG;
  *out = {};
  if (!result)
    return LUMICE_PATH_READ_NULL_ARG;
  const auto& rows = result->result.Data().core.objects;
  if (object >= rows.size() || index >= rows[object].Chromatic().verdict.notes.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  *out = Text(rows[object].Chromatic().verdict.notes[index]);
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetProducedKind(const LUMICE_PathFeatureReport* result, size_t index,
                                                       LUMICE_PathText* out) {
  if (!out)
    return LUMICE_PATH_READ_NULL_ARG;
  *out = {};
  if (!result)
    return LUMICE_PATH_READ_NULL_ARG;
  const auto& rows = result->result.Data().core.coverage.produced;
  if (index >= rows.size())
    return LUMICE_PATH_READ_INVALID_INDEX;
  *out = Text(lumice::raypath::schema3::ObjectKindName(rows[index]));
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetRecord(const LUMICE_PathFeatureReport* result, size_t index,
                                                 LUMICE_PathReportRecord* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& records = result->result.Data().mc.discovery.features;
  if (index >= records.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& record = records[index];
  out->evidence = static_cast<int>(record.evidence) + 1;
  out->geometry = static_cast<int>(record.geometry) + 1;
  out->kind = Text(record.kind);
  out->reason = Text(record.reason);
  out->sky_point_count = record.sky_points.size();
  out->field_point_count = record.field_points.size();
  out->interface_curve_count = record.interface_curves.size();
  out->source_parameter_count = record.source_parameters.size();
  out->source_token_available = record.source_token.has_value();
  if (record.source_token) {
    out->source_token = *record.source_token;
  }
  out->connected_record_available = record.source_connected_feature.has_value();
  if (record.source_connected_feature) {
    out->connected_record = *record.source_connected_feature;
  }
  out->level = record.level;
  out->bandwidth_rad = record.bandwidth_rad;
  out->replicate_movement_available = record.replicate_movement_rad.has_value();
  if (record.replicate_movement_rad) {
    out->replicate_movement_rad = *record.replicate_movement_rad;
  }
  out->scale_movement_available = record.scale_movement_rad.has_value();
  if (record.scale_movement_rad) {
    out->scale_movement_rad = *record.scale_movement_rad;
  }
  out->minimum_effective_samples = record.minimum_effective_samples;
  out->equation = static_cast<int>(record.equation);
  out->contributor_fraction_available = record.contributor_fraction_of_estimated_y.has_value();
  if (record.contributor_fraction_of_estimated_y) {
    out->contributor_fraction_of_estimated_y = *record.contributor_fraction_of_estimated_y;
  }
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetCoverage(const LUMICE_PathFeatureReport* result, size_t index,
                                                   LUMICE_PathReportCoverage* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().coverage;
  if (index >= rows.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  const auto& row = rows[index];
  out->role = static_cast<int>(row.role) + 1;
  out->status = static_cast<int>(row.status) + 1;
  out->subject = Text(row.subject);
  out->reason = Text(row.reason);
  out->limitation_count = row.limitations.size();
  return LUMICE_PATH_READ_OK;
}

LUMICE_PathReadStatus LUMICE_PathReportGetDeclaredSkip(const LUMICE_PathFeatureReport* result, size_t index,
                                                       LUMICE_PathReportDeclaredSkip* out) {
  if (const auto status = BeginRead(result, out); status != LUMICE_PATH_READ_OK) {
    return status;
  }
  const auto& rows = result->result.Data().core.coverage.declared_not_produced;
  if (index >= rows.size()) {
    return LUMICE_PATH_READ_INVALID_INDEX;
  }
  out->kind = Text(lumice::raypath::schema3::ObjectKindName(rows[index].kind));
  out->reason = Text(rows[index].reason);
  return LUMICE_PATH_READ_OK;
}
