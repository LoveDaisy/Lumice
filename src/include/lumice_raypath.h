#ifndef LUMICE_RAYPATH_H_
#define LUMICE_RAYPATH_H_

#include <stdint.h>

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
// the selected crystal entry's physical L2 symmetry, evaluates concrete members at discrete
// wavelengths, and returns positioned, fixture-backed features plus explicit coverage and
// limitations. It does not add fields to the single-path schema above.

#define LUMICE_PATH_FEATURE_REPORT_MAX_SAMPLE_COUNT 1000000
#define LUMICE_PATH_FEATURE_REPORT_MAX_WAVELENGTH_COUNT 32
// The report evaluates every admitted physical-L2 member at every wavelength twice: at the
// requested resolution and at half that resolution. This bounds their combined synchronous work.
#define LUMICE_PATH_FEATURE_REPORT_MAX_SAMPLE_EVALUATIONS 16777216

typedef enum LUMICE_PathFeatureMemberSelection {
  LUMICE_PATH_FEATURE_MEMBERS_ALL_PHYSICAL = 0,
  LUMICE_PATH_FEATURE_MEMBERS_CONCRETE = 1,
  LUMICE_PATH_FEATURE_MEMBERS_PHYSICAL_MASK = 2,
} LUMICE_PathFeatureMemberSelection;

typedef enum LUMICE_PathFeatureSpectrumSource {
  LUMICE_PATH_FEATURE_SPECTRUM_SCENE = 0,
  LUMICE_PATH_FEATURE_SPECTRUM_DIAGNOSTIC = 1,
  LUMICE_PATH_FEATURE_SPECTRUM_LEGACY_REFERENCE = 2,
} LUMICE_PathFeatureSpectrumSource;

typedef struct LUMICE_PathFeatureReportRequest {
  // Set to sizeof of the caller's struct. Layout v1 (through sample_count) remains accepted;
  // fields appended after it are read only when struct_size reaches their published v2 extent.
  size_t struct_size;
  int crystal_id;
  const int* faces;
  int face_count;
  const int* layer_face_counts;
  int layer_count;
  // Optional discrete wavelengths and spectral weights. wavelength_count == 0 selects the
  // report's documented red/blue diagnostic endpoints. weights may be NULL, meaning 1 for every
  // wavelength. Every wavelength must lie in [350, 900] nm; every weight is finite and >= 0.
  const double* wavelengths_nm;
  const double* wavelength_weights;
  int wavelength_count;
  // Even integer in [64, LUMICE_PATH_FEATURE_REPORT_MAX_SAMPLE_COUNT]; 0 selects 8192. The report
  // records both this fine resolution and its half-resolution estimate. The combined work is at
  // most LUMICE_PATH_FEATURE_REPORT_MAX_SAMPLE_EVALUATIONS: physical-L2 members × wavelengths ×
  // (sample_count + sample_count / 2).
  int sample_count;

  // v2 append-only scene-measure fields. One crystal id per path layer; NULL/count 0 retains the
  // legacy one-layer crystal_id field. Multi-layer requests require a full array.
  const int* layer_crystal_ids;
  int layer_crystal_id_count;
  int member_selection;  // LUMICE_PathFeatureMemberSelection; zero = all physical members.
  uint64_t physical_member_mask;
  // wavelength_count > 0 always selects the explicit diagnostic nodes above. With no explicit
  // nodes, this selects the actual scene spectrum by default; LEGACY_REFERENCE is the named
  // compatibility mode for the historical red/blue pair.
  int spectrum_source;  // LUMICE_PathFeatureSpectrumSource
  // Joint scene-measure samples (shape + pose across all layers); 0 = 64. Independent of the
  // legacy fixture-detector sample_count above.
  int scene_measure_sample_count;
  int sun_node_count;         // 0 = 8 for a finite disc; ignored for a zero-diameter sun.
  int illuminant_node_count;  // 0 = 8 midpoint-stratified nodes over [380, 780).
  uint32_t seed;              // 0 is a valid deterministic seed.
} LUMICE_PathFeatureReportRequest;

typedef struct LUMICE_PathFeatureReport_ LUMICE_PathFeatureReport;

// Synchronous and deterministic. Error and ownership rules match LUMICE_AnalyzeSinglePath. The
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
