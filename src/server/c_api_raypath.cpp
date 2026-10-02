// C API bridge for lumice_raypath.h, the single-path analysis entry point: a LUMICE_Scene and a C
// request in, the module's result (src/raypath/single_path_analysis.hpp) out as an opaque handle
// holding its one JSON form (src/raypath/single_path_json.hpp). Registered at the raypath layer,
// and compiled into the lumice_raypath_obj OBJECT library rather than lumice_obj: liblumice and
// liblumice_testapi link that library, and liblumice_analytic links neither.

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "config/config_manager.hpp"
#include "include/lumice_raypath.h"
#include "raypath/path_feature_report.hpp"
#include "raypath/path_feature_report_json.hpp"
#include "raypath/single_path_analysis.hpp"
#include "raypath/single_path_json.hpp"
#include "server/c_api_engine_internal.hpp"  // lumice::capi::ToCApiErrorCode
#include "server/c_api_scene_internal.hpp"   // SceneRoot
#include "server/server.hpp"
#include "util/logger.hpp"

static_assert(LUMICE_SINGLE_PATH_MAX_SAMPLE_COUNT == lumice::raypath::kMaxSampleCount,
              "lumice_raypath.h's sample-count bound is the module's");
static_assert(LUMICE_SINGLE_PATH_MAX_SUN_GRID_LAT_COUNT == lumice::raypath::kMaxSunGridLatCount,
              "lumice_raypath.h and the single-path module must agree on the sun-grid bound");
static_assert(LUMICE_PATH_FEATURE_REPORT_MAX_SAMPLE_COUNT == lumice::raypath::kMaxFeatureReportSampleCount,
              "lumice_raypath.h and the feature-report module must agree on the sample bound");
static_assert(LUMICE_PATH_FEATURE_REPORT_MAX_WAVELENGTH_COUNT == lumice::raypath::kMaxFeatureReportWavelengthCount,
              "lumice_raypath.h and the feature-report module must agree on the wavelength bound");
static_assert(LUMICE_PATH_FEATURE_REPORT_MAX_SAMPLE_EVALUATIONS == lumice::raypath::kMaxFeatureReportSampleEvaluations,
              "lumice_raypath.h and the feature-report module must agree on the total-work bound");

struct LUMICE_SinglePathResult_ {
  std::string json;  // produced once in LUMICE_AnalyzeSinglePath, never changed afterwards
};

struct LUMICE_PathFeatureReport_ {
  std::string json;  // produced once in LUMICE_AnalyzePathFeatureReport, immutable afterwards
};

namespace {

// The v4.50 request, the smallest `struct_size` this build reads: through its last v4.50 field, so
// that a field appended later does not raise it.
constexpr size_t kRequestSizeV450 = offsetof(LUMICE_SinglePathRequest, warm_json_len) + sizeof(size_t);
constexpr size_t kFeatureReportRequestSizeV1 = offsetof(LUMICE_PathFeatureReportRequest, sample_count) + sizeof(int);

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

LUMICE_ErrorCode Analyze(const LUMICE_Scene* scene, const LUMICE_SinglePathRequest& request,
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
  if (request.warm_json != nullptr && request.warm_json_len > 0) {
    if (const rp::Error e = rp::ParseWarmSeeds(std::string(request.warm_json, request.warm_json_len), &req.warm_seeds);
        !e.Ok()) {
      return Refuse(e, err_buf, err_size);
    }
  }

  // The scene document LUMICE_CommitScene would hand the server, through the same parser.
  lumice::ConfigManager config;
  if (const lumice::Error err = lumice::ParseConfigManager(SceneRoot(scene), "LUMICE_AnalyzeSinglePath",
                                                           lumice::GetGlobalLogger(), nullptr, &config)) {
    WriteError(err_buf, err_size, "invalid_scene: " + err.message);
    return lumice::capi::ToCApiErrorCode(err.code);
  }

  rp::SinglePathResult result;
  if (const rp::Error e = rp::AnalyzeSinglePath(config, req, &result); !e.Ok()) {
    return Refuse(e, err_buf, err_size);
  }
  auto handle = std::make_unique<LUMICE_SinglePathResult_>();
  handle->json = rp::ToJson(result, LUMICE_GetVersionString());
  *out = handle.release();
  return LUMICE_OK;
}

LUMICE_ErrorCode AnalyzeReport(const LUMICE_Scene* scene, const LUMICE_PathFeatureReportRequest& request,
                               LUMICE_PathFeatureReport** out, char* err_buf, size_t err_size) {
  namespace rp = lumice::raypath;

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
  if (request.layer_count != 1 || request.face_count <= 0 || request.faces == nullptr ||
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
  req.sample_count = request.sample_count == 0 ? rp::kDefaultFeatureReportSampleCount : request.sample_count;

  lumice::ConfigManager config;
  if (const lumice::Error err = lumice::ParseConfigManager(SceneRoot(scene), "LUMICE_AnalyzePathFeatureReport",
                                                           lumice::GetGlobalLogger(), nullptr, &config)) {
    WriteError(err_buf, err_size, "invalid_scene: " + err.message);
    return lumice::capi::ToCApiErrorCode(err.code);
  }
  rp::PathFeatureReport report;
  if (const rp::Error e = rp::AnalyzePathFeatureReport(config, req, &report); !e.Ok()) {
    return Refuse(e, err_buf, err_size);
  }
  auto handle = std::make_unique<LUMICE_PathFeatureReport_>();
  handle->json = rp::PathFeatureReportToJson(report, LUMICE_GetVersionString());
  *out = handle.release();
  return LUMICE_OK;
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
    return Analyze(scene, *request, out, err_buf, err_size);
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
  if (result == nullptr) {
    return LUMICE_ERR_NULL_ARG;
  }
  if (out_len != nullptr) {
    *out_len = result->json.size();
  }
  if (out_buf != nullptr && buf_size > 0) {
    const size_t n = std::min(result->json.size(), buf_size - 1);
    std::memcpy(out_buf, result->json.data(), n);
    out_buf[n] = '\0';
  }
  return LUMICE_OK;
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
    return AnalyzeReport(scene, *request, out, err_buf, err_size);
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
  if (result == nullptr) {
    return LUMICE_ERR_NULL_ARG;
  }
  if (out_len != nullptr) {
    *out_len = result->json.size();
  }
  if (out_buf != nullptr && buf_size > 0) {
    const size_t n = std::min(result->json.size(), buf_size - 1);
    std::memcpy(out_buf, result->json.data(), n);
    out_buf[n] = '\0';
  }
  return LUMICE_OK;
}

void LUMICE_PathFeatureReportDestroy(LUMICE_PathFeatureReport* result) {
  std::unique_ptr<LUMICE_PathFeatureReport_> owned(result);
}
