// lumice.h's single-path analysis entry point: a LUMICE_Scene and a C request in, the module's
// result (src/raypath/single_path_analysis.hpp) out as an opaque handle holding its one JSON form
// (src/raypath/single_path_json.hpp). A translation unit of its own, outside c_api.cpp, because it
// belongs to the lumice_raypath_obj OBJECT library: liblumice and liblumice_testapi link that
// library, liblumice_analytic links lumice_obj without it, and c_api.cpp is part of lumice_obj.

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "config/config_manager.hpp"
#include "include/lumice.h"
#include "raypath/single_path_analysis.hpp"
#include "raypath/single_path_json.hpp"
#include "server/c_api_internal.hpp"
#include "server/server.hpp"
#include "util/logger.hpp"

static_assert(LUMICE_SINGLE_PATH_MAX_SAMPLE_COUNT == lumice::raypath::kMaxSampleCount,
              "lumice.h's sample-count bound is the module's");

struct LUMICE_SinglePathResult_ {
  std::string json;  // produced once in LUMICE_AnalyzeSinglePath, never changed afterwards
};

namespace {

// The v4.50 request, the smallest `struct_size` this build reads: through its last v4.50 field, so
// that a field appended later does not raise it.
constexpr size_t kRequestSizeV450 = offsetof(LUMICE_SinglePathRequest, warm_json_len) + sizeof(size_t);

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
    return MapErrorCode(err.code);
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
