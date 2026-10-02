// C API bridge for lumice_raypath.h, the single-path analysis entry point: a LUMICE_Scene and a C
// request in, the module's result (src/raypath/single_path_analysis.hpp) out as an opaque handle
// holding its one JSON form (src/raypath/single_path_json.hpp). Registered at the raypath layer,
// and compiled into the lumice_raypath_obj OBJECT library rather than lumice_obj: liblumice and
// liblumice_testapi link that library, and liblumice_analytic links neither.

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <exception>
#include <limits>
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
struct PathFeatureReportRequestV451 {
  size_t struct_size;
  int crystal_id;
  const int* faces;
  int face_count;
  const int* layer_face_counts;
  int layer_count;
  const double* wavelengths_nm;
  const double* wavelength_weights;
  int wavelength_count;
  int sample_count;
};
constexpr size_t kFeatureReportRequestSizeV1 = sizeof(PathFeatureReportRequestV451);
constexpr size_t kFeatureReportRequestSizeV2 = offsetof(LUMICE_PathFeatureReportRequest, seed) + sizeof(uint32_t);
constexpr size_t kFeatureReportRequestSizeV3 =
    offsetof(LUMICE_PathFeatureReportRequest, explicit_member_chain_count) + sizeof(int);
static_assert(kFeatureReportRequestSizeV1 == offsetof(LUMICE_PathFeatureReportRequest, layer_crystal_ids),
              "the frozen v4.51 request extent includes its trailing ABI padding");

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
  const bool has_v2 = request.struct_size >= kFeatureReportRequestSizeV2;
  const bool has_v3 = request.struct_size >= kFeatureReportRequestSizeV3;

  rp::PathFeatureReportRequest req;
  req.schema_version = has_v2 ? rp::kFeatureReportSchemaVersion : 1;
  req.crystal_id = static_cast<lumice::IdType>(request.crystal_id);
  const bool has_layer_crystal_ids = has_v2 && request.layer_crystal_id_count > 0;
  if (!has_layer_crystal_ids && (request.crystal_id < 0 || request.crystal_id != static_cast<int>(req.crystal_id))) {
    return Refuse(
        { rp::ErrorCode::kUnknownCrystalId, "no crystal entry with id " + std::to_string(request.crystal_id) }, err_buf,
        err_size);
  }
  if (request.face_count < 0 || request.layer_count < 0 || request.wavelength_count < 0 ||
      (has_v2 && (request.layer_crystal_id_count < 0 || request.scene_measure_sample_count < 0 ||
                  request.sun_node_count < 0 || request.illuminant_node_count < 0)) ||
      (has_v3 && (request.physical_member_mask_count < 0 || request.explicit_member_face_count < 0 ||
                  request.explicit_member_layer_face_count < 0 || request.explicit_member_chain_count < 0))) {
    return Refuse({ rp::ErrorCode::kInvalidArgument, "negative face, layer or wavelength count" }, err_buf, err_size);
  }
  if (request.layer_count <= 0 || request.face_count <= 0 || request.faces == nullptr ||
      request.layer_face_counts == nullptr) {
    return Refuse({ rp::ErrorCode::kInvalidPath, "a path feature report requires at least one non-empty layer" },
                  err_buf, err_size);
  }
  if (has_v3 && request.physical_member_mask_count > 0 && request.physical_member_masks == nullptr) {
    WriteError(err_buf, err_size, "physical_member_masks is null with a positive count");
    return LUMICE_ERR_NULL_ARG;
  }
  if (has_v3 && request.explicit_member_chain_count > 0 &&
      (request.explicit_member_faces == nullptr || request.explicit_member_layer_face_counts == nullptr)) {
    WriteError(err_buf, err_size, "explicit member chain arrays are null with a positive count");
    return LUMICE_ERR_NULL_ARG;
  }
  if (request.wavelength_count > rp::kMaxFeatureReportWavelengthCount) {
    return Refuse({ rp::ErrorCode::kInvalidArgument, "at most " + std::to_string(rp::kMaxFeatureReportWavelengthCount) +
                                                         " wavelengths may be requested" },
                  err_buf, err_size);
  }
  int consumed = 0;
  for (int i = 0; i < request.layer_count; i++) {
    const int count = request.layer_face_counts[i];
    if (count <= 0 || count > request.face_count - consumed) {
      return Refuse({ rp::ErrorCode::kInvalidPath, "layer face counts do not add up to face_count" }, err_buf,
                    err_size);
    }
    req.path_layers.emplace_back(request.faces + consumed, request.faces + consumed + count);
    consumed += count;
  }
  if (consumed != request.face_count) {
    return Refuse({ rp::ErrorCode::kInvalidPath, "layer face counts do not add up to face_count" }, err_buf, err_size);
  }
  if (has_v2 && request.layer_crystal_id_count > 0) {
    if (request.layer_crystal_id_count != request.layer_count) {
      return Refuse({ rp::ErrorCode::kInvalidPath, "layer_crystal_id_count must equal layer_count" }, err_buf,
                    err_size);
    }
    req.layer_crystal_ids.reserve(static_cast<size_t>(request.layer_crystal_id_count));
    for (int i = 0; i < request.layer_crystal_id_count; i++) {
      const int id = request.layer_crystal_ids[i];
      const lumice::IdType converted = static_cast<lumice::IdType>(id);
      if (id < 0 || id != static_cast<int>(converted)) {
        return Refuse({ rp::ErrorCode::kUnknownCrystalId, "invalid layer crystal id " + std::to_string(id) }, err_buf,
                      err_size);
      }
      req.layer_crystal_ids.push_back(converted);
    }
  } else if (request.layer_count == 1) {
    req.layer_crystal_ids = { req.crystal_id };
  } else {
    return Refuse({ rp::ErrorCode::kInvalidPath, "multi-layer requests require layer_crystal_ids" }, err_buf, err_size);
  }
  if (request.wavelength_count > 0) {
    req.wavelengths_nm.assign(request.wavelengths_nm, request.wavelengths_nm + request.wavelength_count);
  }
  if (request.wavelength_weights != nullptr) {
    req.wavelength_weights.assign(request.wavelength_weights, request.wavelength_weights + request.wavelength_count);
  }
  req.sample_count = request.sample_count == 0 ? rp::kDefaultFeatureReportSampleCount : request.sample_count;
  if (has_v2) {
    switch (request.member_selection) {
      case LUMICE_PATH_FEATURE_MEMBERS_ALL_PHYSICAL:
        req.member_selection = rp::SceneMemberSelection::kAllPhysical;
        break;
      case LUMICE_PATH_FEATURE_MEMBERS_CONCRETE:
        req.member_selection = rp::SceneMemberSelection::kConcrete;
        break;
      case LUMICE_PATH_FEATURE_MEMBERS_PHYSICAL_MASK:
        req.member_selection = rp::SceneMemberSelection::kPhysicalMask;
        break;
      case LUMICE_PATH_FEATURE_MEMBERS_EXPLICIT_CHAINS:
        if (!has_v3) {
          return Refuse({ rp::ErrorCode::kInvalidArgument, "explicit member chains require the v3 request extent" },
                        err_buf, err_size);
        }
        req.member_selection = rp::SceneMemberSelection::kExplicitChains;
        break;
      default:
        return Refuse({ rp::ErrorCode::kInvalidArgument, "unknown member_selection" }, err_buf, err_size);
    }
    req.physical_member_mask = request.physical_member_mask;
    if (has_v3 && request.physical_member_mask_count > 0) {
      if (request.physical_member_mask_count != request.layer_count) {
        return Refuse({ rp::ErrorCode::kInvalidArgument, "physical_member_mask_count must equal layer_count" }, err_buf,
                      err_size);
      }
      req.physical_member_masks.assign(request.physical_member_masks,
                                       request.physical_member_masks + request.physical_member_mask_count);
    }
    if (has_v3 && request.explicit_member_chain_count > 0) {
      const int64_t expected_layer_counts_wide =
          static_cast<int64_t>(request.explicit_member_chain_count) * request.layer_count;
      if (expected_layer_counts_wide > std::numeric_limits<int>::max()) {
        return Refuse({ rp::ErrorCode::kInvalidArgument, "explicit member chain dimensions overflow" }, err_buf,
                      err_size);
      }
      const int expected_layer_counts = static_cast<int>(expected_layer_counts_wide);
      if (request.explicit_member_layer_face_count != expected_layer_counts) {
        return Refuse(
            { rp::ErrorCode::kInvalidPath, "explicit_member_layer_face_count must equal chain_count * layer_count" },
            err_buf, err_size);
      }
      int face_offset = 0;
      req.explicit_member_chains.reserve(static_cast<size_t>(request.explicit_member_chain_count));
      for (int chain_index = 0; chain_index < request.explicit_member_chain_count; chain_index++) {
        std::vector<std::vector<int>> chain;
        chain.reserve(static_cast<size_t>(request.layer_count));
        for (int layer_index = 0; layer_index < request.layer_count; layer_index++) {
          const int count = request.explicit_member_layer_face_counts[chain_index * request.layer_count + layer_index];
          if (count <= 0 || count > request.explicit_member_face_count - face_offset) {
            return Refuse({ rp::ErrorCode::kInvalidPath,
                            "explicit member layer face counts do not add up to explicit_member_face_count" },
                          err_buf, err_size);
          }
          chain.emplace_back(request.explicit_member_faces + face_offset,
                             request.explicit_member_faces + face_offset + count);
          face_offset += count;
        }
        req.explicit_member_chains.push_back(std::move(chain));
      }
      if (face_offset != request.explicit_member_face_count) {
        return Refuse({ rp::ErrorCode::kInvalidPath,
                        "explicit member layer face counts do not add up to explicit_member_face_count" },
                      err_buf, err_size);
      }
    }
    switch (request.spectrum_source) {
      case LUMICE_PATH_FEATURE_SPECTRUM_SCENE:
        req.scene_spectrum_source = rp::SceneSpectrumSource::kScene;
        break;
      case LUMICE_PATH_FEATURE_SPECTRUM_DIAGNOSTIC:
        req.scene_spectrum_source = rp::SceneSpectrumSource::kDiagnostic;
        break;
      case LUMICE_PATH_FEATURE_SPECTRUM_LEGACY_REFERENCE:
        req.scene_spectrum_source = rp::SceneSpectrumSource::kLegacyReferenceEndpoints;
        break;
      default:
        return Refuse({ rp::ErrorCode::kInvalidArgument, "unknown spectrum_source" }, err_buf, err_size);
    }
    req.scene_measure_sample_count = request.scene_measure_sample_count == 0 ? 64 : request.scene_measure_sample_count;
    req.sun_node_count = request.sun_node_count == 0 ? 8 : request.sun_node_count;
    req.illuminant_node_count = request.illuminant_node_count == 0 ? 8 : request.illuminant_node_count;
    req.seed = request.seed;
  } else {
    req.member_selection = rp::SceneMemberSelection::kAllPhysical;
    req.scene_spectrum_source = request.wavelength_count == 0 ? rp::SceneSpectrumSource::kLegacyReferenceEndpoints :
                                                                rp::SceneSpectrumSource::kDiagnostic;
  }

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
  const bool is_v1 = request->struct_size == kFeatureReportRequestSizeV1;
  const bool is_v2 = request->struct_size == kFeatureReportRequestSizeV2;
  const bool has_v3_or_future_tail = request->struct_size >= kFeatureReportRequestSizeV3;
  if (!is_v1 && !is_v2 && !has_v3_or_future_tail) {
    WriteError(err_buf, err_size,
               "invalid_argument: struct_size must be the complete v1 or v2 request extent, or at least the "
               "complete v3 extent");
    return LUMICE_ERR_INVALID_VALUE;
  }
  if ((request->faces == nullptr && request->face_count > 0) ||
      (request->layer_face_counts == nullptr && request->layer_count > 0) ||
      (request->wavelengths_nm == nullptr && request->wavelength_count > 0) ||
      (request->struct_size >= kFeatureReportRequestSizeV2 && request->layer_crystal_ids == nullptr &&
       request->layer_crystal_id_count > 0) ||
      (request->struct_size >= kFeatureReportRequestSizeV3 && request->physical_member_masks == nullptr &&
       request->physical_member_mask_count > 0) ||
      (request->struct_size >= kFeatureReportRequestSizeV3 && request->explicit_member_faces == nullptr &&
       request->explicit_member_face_count > 0) ||
      (request->struct_size >= kFeatureReportRequestSizeV3 && request->explicit_member_layer_face_counts == nullptr &&
       request->explicit_member_layer_face_count > 0)) {
    WriteError(err_buf, err_size, "null_arg: counted request arrays must not be NULL with a non-zero count");
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
