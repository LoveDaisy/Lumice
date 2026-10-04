#ifndef LUMICE_RAYPATH_H_
#define LUMICE_RAYPATH_H_

// Single-path analysis: one single-layer raypath of one crystal entry, computed synchronously and
// independently of any server.

#include "lumice_base.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============== Single-Path Analysis ===============
// Everything computed once ONE single-layer raypath of ONE crystal entry has been chosen: the
// components of the fiber of crystal poses that send the sun into a given sky point, each
// component's poses with per-pose detail, and the path's deviation over the whole sun-direction
// sphere (doc/raypath-analysis.md section 5.1.8). Synchronous and deterministic (the same scene and
// request give the same result); it runs on the calling thread, is independent of any
// LUMICE_Server, and cannot be cancelled — sample_count bounds its cost.
//
// The result is an OPAQUE handle read through its JSON form (LUMICE_SinglePathResultToJson; the
// fields are documented in doc/raypath-cli-output.md), not a C struct mirror. Deliberate: the result
// holds variable-length nested lists (components -> points -> per-face arrays) whose fields a user
// interface will keep adding to, and the engine is the one place it is serialized, so the CLI and
// the GUI cannot each grow a copy. Typed readers may be appended later without changing this
// handle. The JSON is produced once, inside LUMICE_AnalyzeSinglePath, and the handle is immutable
// afterwards: ToJson only copies it, so concurrent reads are safe and the two calls of a
// length-query-then-fetch always agree.

// The largest sample_count a request may ask for: the call cannot be cancelled, so its cost is
// bounded here instead.
#define LUMICE_SINGLE_PATH_MAX_SAMPLE_COUNT 100000000
// The largest sun_grid_lat_count a request may ask for (longitude is twice that): bounds the
// grid's memory for any caller, not only the CLI (which caps --grid lower still).
#define LUMICE_SINGLE_PATH_MAX_SUN_GRID_LAT_COUNT 1800

// Request. `struct_size` MUST be set to sizeof(LUMICE_SinglePathRequest) of the header the caller
// compiled against: fields are only ever appended, and a size smaller than this version's is
// rejected (LUMICE_ERR_INVALID_VALUE).
typedef struct LUMICE_SinglePathRequest {
  size_t struct_size;
  int crystal_id;  // a crystal entry of the scene
  // The raypath as the user wrote it, in Lumice face numbers (entry, internal reflections, exit):
  // `layer_count` scattering layers, layer i holding layer_face_counts[i] faces, all layers' faces
  // concatenated in `faces` (face_count = the sum). Only a single layer is analysed; more than one
  // is refused as multi_layer_unsupported by the analysis itself, so a shell hands over whatever it
  // parsed and does not decide that on its own.
  const int* faces;
  int face_count;
  const int* layer_face_counts;
  int layer_count;
  double target_altitude_deg;  // the sky point; azimuth measured as the sun's (`analyze --center`)
  double target_azimuth_deg;
  // <= 0 or NaN: unset — the scene's wavelength if its spectrum is exactly one discrete wavelength,
  // else 550 nm. Otherwise must lie in [350, 900].
  double wavelength_nm;
  int sample_count;        // discovery seed events, [1, LUMICE_SINGLE_PATH_MAX_SAMPLE_COUNT]; 0 = default 1000000
  int sun_grid_lat_count;  // latitude rows of the sun-direction grid (longitude twice that), [0,
                           // LUMICE_SINGLE_PATH_MAX_SUN_GRID_LAT_COUNT]; 0 = no grid
  // Warm starts: the text of an earlier LUMICE_SinglePathResultToJson output (its component seeds
  // are read back); NULL or length 0 for none. The same JSON schema version is required.
  const char* warm_json;
  size_t warm_json_len;
} LUMICE_SinglePathRequest;

typedef struct LUMICE_SinglePathResult_ LUMICE_SinglePathResult;

// Analyse. On LUMICE_OK *out is a new handle the caller owns (release with
// LUMICE_SinglePathResultDestroy). On failure *out is NULL and, when err_buf is non-NULL and
// err_size > 0, err_buf holds a NUL-terminated (possibly truncated) message of the form
// "<reason>: <detail>", where <reason> is a stable lower-case name — for a request the analysis
// refuses: unknown_crystal_id, multi_layer_unsupported, invalid_path, face_not_in_crystal,
// wavelength_out_of_range, invalid_target, invalid_argument, crystal_rejected, path_infeasible;
// for a scene that does not parse: invalid_scene; for NULL arguments: null_arg; for an internal
// failure: internal. Return codes: LUMICE_ERR_NULL_ARG (NULL scene,
// request or out; faces / layer_face_counts NULL with a non-zero count), LUMICE_ERR_INVALID_VALUE
// (struct_size too small, layer_face_counts not summing to face_count,
// or a request the analysis refuses — err_buf says which), LUMICE_ERR_INVALID_CONFIG /
// _INVALID_JSON / _MISSING_FIELD (the scene does not parse, as LUMICE_CommitScene would report it),
// LUMICE_ERR_UNKNOWN (an internal failure).
LUMICE_API LUMICE_ErrorCode LUMICE_AnalyzeSinglePath(const LUMICE_Scene* scene, const LUMICE_SinglePathRequest* request,
                                                     LUMICE_SinglePathResult** out, char* err_buf, size_t err_size);

// The result's JSON document (UTF-8, schema_version 1), with the snprintf-style buffer contract of
// LUMICE_SceneToJson: out_buf == NULL (or buf_size == 0) queries the length only; a too-small
// buffer is truncated but always NUL-terminated; *out_len (when non-NULL) is always the full
// length. LUMICE_ERR_NULL_ARG for a NULL result.
LUMICE_API LUMICE_ErrorCode LUMICE_SinglePathResultToJson(const LUMICE_SinglePathResult* result, char* out_buf,
                                                          size_t buf_size, size_t* out_len);

// Release a result. NULL is a no-op.
LUMICE_API void LUMICE_SinglePathResultDestroy(LUMICE_SinglePathResult* result);

// =============== Target-Free Path Feature Report ===============
// A separate report contract for one path without a sky target. It expands the input path under
// the selected crystal entry's physical L2 symmetry, samples the actual product measure and
// spectrum, and returns bounded actual/candidate/unfinished geometry with explicit numerical
// and source evidence. It does not add fields to the single-path schema above.

#define LUMICE_PATH_FEATURE_REPORT_MAX_SAMPLE_COUNT 1000000
#define LUMICE_PATH_FEATURE_REPORT_MAX_WAVELENGTH_COUNT 32
// Maximum permitted optical work request, including replicas, spectral refinement and source corrections.
#define LUMICE_PATH_FEATURE_REPORT_MAX_SAMPLE_EVALUATIONS 16777216

typedef struct LUMICE_PathFeatureReportRequest {
  size_t struct_size;
  int crystal_id;
  const int* faces;
  int face_count;
  const int* layer_face_counts;
  int layer_count;
  // Optional discrete wavelengths and spectral weights. wavelength_count == 0 selects the
  // scene's actual spectrum (including continuous illuminants). weights may be NULL, meaning 1 for every
  // wavelength. Every wavelength must lie in [350, 900] nm; every weight is finite and >= 0.
  const double* wavelengths_nm;
  const double* wavelength_weights;
  int wavelength_count;
  // Even outer sample count in [64, MAX_SAMPLE_COUNT]. Zero chooses a dyadic prefix <=65536
  // from the physical-member/spectrum work expansion. Explicit requests may stop partially.
  int sample_count;
  // Appended v4.52 group, read only when the whole group fits struct_size. A zero field
  // selects its documented default. Older v4.51 requests remain valid with these defaults.
  int budget_ms;                     // 15000; maximum 120000. Numerical deadline, followed by bounded serialization.
  uint64_t max_optical_evaluations;  // 4000000; maximum MAX_SAMPLE_EVALUATIONS
  uint64_t max_field_evaluations;    // 250000000; maximum 1000000000 (weighted components)
  double bandwidth_rad;              // 1 degree in radians; vMF observation, not a physical width
  double location_resolution_rad;    // .05 degree; local numerical target, not a global confidence bound
  int scene_layer_plus_one;          // 0 = first occurrence (or standalone configured crystal); otherwise layer+1
} LUMICE_PathFeatureReportRequest;

typedef struct LUMICE_PathFeatureReport_ LUMICE_PathFeatureReport;

// Synchronous with replayable sampling; deadline-limited coverage may vary. Ownership matches
// LUMICE_AnalyzeSinglePath. The
// request deliberately has no target and no warm-start field: this is a path-level report, not a
// target fiber search.
LUMICE_API LUMICE_ErrorCode LUMICE_AnalyzePathFeatureReport(const LUMICE_Scene* scene,
                                                            const LUMICE_PathFeatureReportRequest* request,
                                                            LUMICE_PathFeatureReport** out, char* err_buf,
                                                            size_t err_size);

// UTF-8 JSON with schema "lumice.path-feature-report", schema_version 2. Uses the same
// length-query/fetch and truncation contract as LUMICE_SinglePathResultToJson.
LUMICE_API LUMICE_ErrorCode LUMICE_PathFeatureReportToJson(const LUMICE_PathFeatureReport* result, char* out_buf,
                                                           size_t buf_size, size_t* out_len);

// Release a report. NULL is a no-op.
LUMICE_API void LUMICE_PathFeatureReportDestroy(LUMICE_PathFeatureReport* result);

#ifdef __cplusplus
}
#endif

#endif  // LUMICE_RAYPATH_H_
