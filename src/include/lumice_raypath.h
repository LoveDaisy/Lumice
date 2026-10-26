#ifndef LUMICE_RAYPATH_H_
#define LUMICE_RAYPATH_H_

// Single-path analysis: one single-layer raypath of one crystal entry, computed synchronously and
// independently of any server.

#include <stdint.h>

#include "lumice_base.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============== Single-Path Analysis ===============
// Everything computed once ONE single-layer raypath of ONE crystal entry has been chosen: the
// components of the fiber of crystal poses that send the sun into a given sky point, each
// component's poses with per-pose detail, and the path's deviation over the whole sun-direction
// sphere (doc/raypath/raypath-analysis.md section 5.1.8). Synchronous and deterministic (the same scene and
// request give the same result); it runs on the calling thread, is independent of any
// LUMICE_Server, and cannot be cancelled — sample_count bounds its cost.
//
// The opaque result owns a completed, immutable typed value. ToJson derives an optional UTF-8
// representation on first request and caches it for the result's lifetime; later calls copy the
// same bytes. Concurrent reads and length-query-then-fetch return identical data. The result is
// independent of its Scene after Analyze returns; Destroy must not overlap a read.

// The largest sample_count a request may ask for: the call cannot be cancelled, so its cost is
// bounded here instead.
#define LUMICE_SINGLE_PATH_MAX_SAMPLE_COUNT 100000000
// The largest sun_grid_lat_count a request may ask for (longitude is twice that): bounds the
// grid's memory for any caller, not only the CLI (which caps --grid lower still).
#define LUMICE_SINGLE_PATH_MAX_SUN_GRID_LAT_COUNT 1800
// The budget_ms a 0 deadline_ms request resolves to, and the largest one it may carry (a shell's
// validate-early mirror of the engine's own constants, like the survey's pair).
#define LUMICE_SINGLE_PATH_DEFAULT_BUDGET_MS 15000
#define LUMICE_SINGLE_PATH_MAX_BUDGET_MS 120000

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
  // are read back), or of a `Lumice raypath survey` schema-4 document (the --warm index contract).
  // NULL or length 0 for none. The same JSON schema version is required.
  const char* warm_json;
  size_t warm_json_len;
  // Appended group, read only when the whole group fits struct_size (older requests keep their
  // exact previous behaviour: no budget, no interruption). A 0 deadline_ms selects the documented
  // default; the check runs at the computation's loop boundaries, so it bounds, never interrupts
  // mid-number. The stopped call serializes what it has and reports interrupted in the document's
  // meta — an honest early exit, not a failure.
  int deadline_ms;                  // 0 = LUMICE_SINGLE_PATH_DEFAULT_BUDGET_MS; at most
                                    // LUMICE_SINGLE_PATH_MAX_BUDGET_MS
  const volatile int* interrupted;  // the caller's one-way stop flag: 0 runs, non-zero stops at the
                                    // next loop boundary (a signal handler's sig_atomic_t; the
                                    // engine only reads it, never writes). NULL = no flag.
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
  int symmetry_bits_plus_one;        // 0 = P|B|D default; otherwise (P=1, B=2, D=4 bits)+1; 1 selects a concrete path
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

// UTF-8 JSON with schema "lumice.path-feature-report", schema_version 3. Uses the same
// length-query/fetch and truncation contract as LUMICE_SinglePathResultToJson.
LUMICE_API LUMICE_ErrorCode LUMICE_PathFeatureReportToJson(const LUMICE_PathFeatureReport* result, char* out_buf,
                                                           size_t buf_size, size_t* out_len);

// Release a report. NULL is a no-op.
LUMICE_API void LUMICE_PathFeatureReportDestroy(LUMICE_PathFeatureReport* result);

// =============== Path Survey (L1 census) ===============
// A separate contract for the survey face of the diagnostic pipeline: one single-layer raypath
// class of one crystal entry, routed at the algebra layer (T0). A class whose every member
// carries the identity certificate (the fold matrix is I and the wedge is zero — the kernel's
// rank-0 test) collapses to exactly one source_image object at the sun and the configurational
// analysis stops there; any other member verdict ships the T0 facts and leaves the T1/T2 object
// kinds unevaluated with the named reason. The result is the schema-4 document
// ("lumice.path-survey", doc/raypath/raypath-cli-output.md), serialized engine-side once — the
// CLI and the GUI read the same bytes. Synchronous and independent of any LUMICE_Server; it
// runs on the calling thread and cannot be cancelled. The routing/probe/chromatic legs are
// deterministic and enforce no budget yet (the v1 routing leg is millisecond work); one
// document-level leg is the exception: the opt-in MC region ESS leg (see mc_ess_draws below)
// is seeded random, reproducible for a given seed, and bounded by budget_ms's deadline.

// The largest budget_ms a survey request may carry, mirroring the report request's bound. The
// v1 leg does not consume it; the constant exists so a shell can validate before the engine does.
#define LUMICE_PATH_SURVEY_MAX_BUDGET_MS 120000

// The largest mc_ess_draws a survey request may carry (the engine refuses more). The opt-in MC
// region ESS leg consumes the budget_ms deadline, not a per-draw budget.
#define LUMICE_PATH_SURVEY_MAX_MC_ESS_DRAWS 200000

// The budget_ms a 0 budget_ms request resolves to, as a shell-facing constant (the engine's own
// default, mirrored here for the same validate-early purpose).
#define LUMICE_PATH_SURVEY_DEFAULT_BUDGET_MS 15000

typedef struct LUMICE_PathSurveyRequest {
  size_t struct_size;
  int crystal_id;  // a crystal entry of the scene
  // The raypath as the user wrote it, in Lumice face numbers, with the same layer encoding as
  // LUMICE_SinglePathRequest. Exactly one layer is routed; more than one is refused as
  // multi_layer_unsupported.
  const int* faces;
  int face_count;
  const int* layer_face_counts;
  int layer_count;
  // <= 0 or NaN: unset — the scene's wavelength if its spectrum is exactly one discrete
  // wavelength, else 550 nm. Otherwise must lie in [350, 900].
  double wavelength_nm;
  // 0 selects the 15000 ms default; at most LUMICE_PATH_SURVEY_MAX_BUDGET_MS. Recorded in the
  // document's budget block; not enforced by the v1 routing leg.
  int budget_ms;
  // 0 = the physical P|B|D class (the default); 1 = the literal requested sequence — the report
  // request's symmetry_bits_plus_one encoding.
  int symmetry_bits_plus_one;
  // Appended v4.55, read only when struct_size covers it: 0 (the default) = the opt-in
  // MC region ESS leg does not run; positive = its requested draw count, at most
  // LUMICE_PATH_SURVEY_MAX_MC_ESS_DRAWS (refused as invalid_argument beyond, or when negative).
  // The leg appears in the document only where its arms run it (the full-dimensional and
  // refused-support routes).
  int mc_ess_draws;
} LUMICE_PathSurveyRequest;

typedef struct LUMICE_PathSurvey_ LUMICE_PathSurvey;

// Analyse. On LUMICE_OK *out owns an immutable typed result; ToJson derives its stable JSON
// representation on demand. On failure *out is
// NULL and err_buf holds "<reason>: <detail>" as LUMICE_AnalyzeSinglePath spells it; the request
// refusals are the report's: unknown_crystal_id, invalid_path, multi_layer_unsupported,
// face_not_in_crystal, wavelength_out_of_range, plus invalid_argument (symmetry bits or budget
// out of range), crystal_rejected (the nominal shape builds no crystal), null_arg and internal
// with the same meanings. Return codes match LUMICE_AnalyzeSinglePath's.
LUMICE_API LUMICE_ErrorCode LUMICE_AnalyzePathSurvey(const LUMICE_Scene* scene, const LUMICE_PathSurveyRequest* request,
                                                     LUMICE_PathSurvey** out, char* err_buf, size_t err_size);

// The result's JSON document (UTF-8, schema "lumice.path-survey", schema_version 4), with the
// snprintf-style buffer contract of LUMICE_SinglePathResultToJson.
LUMICE_API LUMICE_ErrorCode LUMICE_PathSurveyToJson(const LUMICE_PathSurvey* result, char* out_buf, size_t buf_size,
                                                    size_t* out_len);

// Release a survey result. NULL is a no-op.
LUMICE_API void LUMICE_PathSurveyDestroy(LUMICE_PathSurvey* result);

// =============== Path Probe (the L2 draw-a-line query) ===============
// A separate contract for the probe face of the diagnostic pipeline: the canonical probe library's
// profile at a user-chosen location, in one of four `along` forms (routing.md's probe subdivision).
// The canonical probe IS a slice of the L1 synchronous reference-lambda band-sum map — the same
// implementation the survey's built-in radial probe runs (one implementation, two triggers), never
// a second evaluation path. The chromatic lambda grid lives on the line only: each requested
// wavelength is one band-sum on the same layout, never a multi-wavelength map.
//
// The four forms are two line geometries:
//   radial / edge-normal — a delta-axis line at the at point's azimuth about the sun (radial spans
//     the whole axis; edge-normal windows the same geometry at the context's nearest edge — the
//     closest of the support intervals' endpoints and the certified edge position to the at point's
//     deviation; a context with neither is the named refusal no_edge_in_context);
//   arc / tangential — a constant-delta arc through the at point, swept over azimuth (the context
//     carries only delta-axis facts, so the arc form declares its constant-delta reading in its
//     spec; the tangential form is the same geometry spelled as the transverse crossing).
//
// Synchronous and deterministic (the band-sum is lattice-deterministic; a probe consumes no fiber
// seeds); it runs on the calling thread and cannot be cancelled except at the per-lambda
// boundaries: a stopped call serializes the profiles it has and reports interrupted in the
// document's meta — an honest early exit, not a failure. The --warm contract of the point query
// applies unchanged: a schema-4 survey document passes the same consistency gates (crystal id,
// path layers, config fingerprint, the family measure echo), and the radial form at a matching
// lambda/sun scope reuses the document's radial samples bit for bit (zero band-sum calls; the
// profile names its provenance warm_reused / recomputed). Other forms always recompute.

// The budget_ms a 0 budget_ms request resolves to, and the largest one it may carry, as a
// shell-facing constant (the engine's own defaults, mirrored like the survey's pair).
#define LUMICE_PATH_PROBE_DEFAULT_BUDGET_MS 15000
#define LUMICE_PATH_PROBE_MAX_BUDGET_MS 120000
// The largest wavelength count a request may carry (the online lambda grid on one line).
#define LUMICE_PATH_PROBE_MAX_WAVELENGTH_COUNT 32

// The along forms. radial: the full delta axis at the at point's azimuth (the value is
// azimuth-free under the uniform density — declared in the document, and the warm reuse
// criterion does not compare the azimuth).
#define LUMICE_PATH_PROBE_ALONG_RADIAL 0
#define LUMICE_PATH_PROBE_ALONG_EDGE_NORMAL 1
#define LUMICE_PATH_PROBE_ALONG_ARC 2
#define LUMICE_PATH_PROBE_ALONG_TANGENTIAL 3

// Request. `struct_size` MUST be set to sizeof(LUMICE_PathProbeRequest) of the header the caller
// compiled against: fields are only ever appended, and a size smaller than this version's is
// rejected (LUMICE_ERR_INVALID_VALUE). A new struct: the budget fields are first-class from the
// first version (no legacy size baggage).
typedef struct LUMICE_PathProbeRequest {
  size_t struct_size;
  int crystal_id;  // a crystal entry of the scene
  // The raypath in Lumice face numbers, with the same layer encoding as
  // LUMICE_SinglePathRequest. Exactly one layer is probed; more than one is refused as
  // multi_layer_unsupported.
  const int* faces;
  int face_count;
  const int* layer_face_counts;
  int layer_count;
  int along;  // a LUMICE_PATH_PROBE_ALONG_* value
  // The user's sky point (altitude in [-90, 90], azimuth measured as the sun's — the
  // `analyze --center` convention). The axis forms read its azimuth about the sun; the arc forms
  // read its deviation. Required for every form.
  double at_altitude_deg;
  double at_azimuth_deg;
  // The online lambda grid: NULL or wavelength_count 0 selects the scene's wavelength if its
  // spectrum is exactly one discrete wavelength, else 550 nm — one profile. Non-empty: one
  // profile per entry, in order, each with its own refractive index; every entry must lie in
  // [350, 900]; at most LUMICE_PATH_PROBE_MAX_WAVELENGTH_COUNT.
  const double* wavelengths_nm;
  int wavelength_count;
  // The band-sum lattice n (the same ruler the survey's probe reads). 0 = the survey probe's
  // 2000000; otherwise [1, LUMICE_SINGLE_PATH_MAX_SAMPLE_COUNT].
  int sample_count;
  // The warm document's text, as LUMICE_AnalyzeSinglePath spells it (a schema-1 seed sheet or a
  // schema-4 survey document). NULL or length 0 for none. A probe consumes no seeds from it.
  const char* warm_json;
  size_t warm_json_len;
  // 0 = LUMICE_PATH_PROBE_DEFAULT_BUDGET_MS; at most LUMICE_PATH_PROBE_MAX_BUDGET_MS. Checked at
  // the per-lambda boundaries: a stopped call serializes the profiles it has and reports
  // interrupted in the document's meta.
  int budget_ms;
  const volatile int* interrupted;  // the caller's one-way stop flag (the engine only reads it,
                                    // never writes). NULL = no flag.
} LUMICE_PathProbeRequest;

typedef struct LUMICE_PathProbe_ LUMICE_PathProbe;

// Analyse. On LUMICE_OK *out owns an immutable typed result; ToJson derives its stable JSON
// representation on demand. On failure *out is
// NULL and err_buf holds "<reason>: <detail>" as LUMICE_AnalyzeSinglePath spells it; the request
// refusals are the point query's: unknown_crystal_id, invalid_path, multi_layer_unsupported,
// face_not_in_crystal, wavelength_out_of_range, invalid_target (the at point), invalid_argument
// (along, sample count, wavelength count or budget out of range), crystal_rejected,
// path_infeasible (no pose realises the path), plus null_arg and internal with the same
// meanings. Refusals the DOCUMENT carries rather than the error: no_edge_in_context,
// degenerate_arc_at_pole, measure_not_expressible, and inline_survey_refused (an edge-normal form
// whose inline context assembly refused — the context blocks carry the same named reason); the
// profile's evidence state reads refused with the mode. Return codes match
// LUMICE_AnalyzeSinglePath's.
LUMICE_API LUMICE_ErrorCode LUMICE_AnalyzePathProbe(const LUMICE_Scene* scene, const LUMICE_PathProbeRequest* request,
                                                    LUMICE_PathProbe** out, char* err_buf, size_t err_size);

// The result's JSON document (UTF-8, schema "lumice.path-probe", schema_version 5), with the
// snprintf-style buffer contract of LUMICE_SinglePathResultToJson.
LUMICE_API LUMICE_ErrorCode LUMICE_PathProbeToJson(const LUMICE_PathProbe* result, char* out_buf, size_t buf_size,
                                                   size_t* out_len);

// Release a probe result. NULL is a no-op.
LUMICE_API void LUMICE_PathProbeDestroy(LUMICE_PathProbe* result);

#ifdef __cplusplus
}
#endif

#endif  // LUMICE_RAYPATH_H_
