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

// Request. Set struct_size to the complete extent of the header the caller compiled against.
// Workspace queries accept published complete extents and larger future tails, rejecting
// intermediate sizes; legacy Analyze preserves its original prefix-group reading behavior.
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
// kinds unevaluated with the named reason. The result owns a typed immutable value; schema-4
// JSON ("lumice.path-survey", doc/raypath/raypath-cli-output.md) is derived on demand.
// Synchronous and independent of any LUMICE_Server. The new workspace query accepts a cancellation
// token and one absolute deadline; legacy Analyze keeps its existing budget semantics. The opt-in
// MC region ESS leg (see mc_ess_draws below) is seeded random and bounded by budget_ms.

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

// =============== Typed query workspace ===============
// One workspace captures a parsed Scene when created. The caller may destroy the Scene after
// creation. Queries on the same workspace must not overlap; distinct workspaces may run in
// parallel. Returned results own their data and outlive both Scene and workspace.
// NULL Destroy is a no-op. A live handle must be destroyed exactly once, after all reads and
// borrowed-pointer uses finish; Destroy must not overlap a read.

typedef struct LUMICE_PathWorkspace_ LUMICE_PathWorkspace;
typedef struct LUMICE_PathWarmData_ LUMICE_PathWarmData;
typedef struct LUMICE_PathCancelToken_ LUMICE_PathCancelToken;

typedef struct LUMICE_PathQueryOptions {
  size_t struct_size;
  int deadline_ms;  // 0 = no deadline; positive = one absolute query deadline, at most 120000
  const LUMICE_PathCancelToken* cancel;
  const LUMICE_PathWarmData* warm;  // only point and probe; NULL means workspace reuse or inline
  int include_context;              // point only: 0 = skip context, 1 = include; other values invalid
} LUMICE_PathQueryOptions;

// A query request must use a published complete struct_size extent or a larger future tail.
// A new point/probe query refuses nonempty request.warm_json when options.warm is present;
// warm_json alone is refused: import it explicitly with LUMICE_PathWarmDataFromJson.
// Query request buffers need only remain valid until the synchronous call returns. The options
// may be NULL for unbounded, uncancelled queries with no external warm data (point context on).
LUMICE_API LUMICE_ErrorCode LUMICE_PathWorkspaceCreate(const LUMICE_Scene* scene, LUMICE_PathWorkspace** out,
                                                       char* err_buf, size_t err_size);
LUMICE_API void LUMICE_PathWorkspaceDestroy(LUMICE_PathWorkspace* workspace);
LUMICE_API LUMICE_ErrorCode LUMICE_PathCancelTokenCreate(LUMICE_PathCancelToken** out);
LUMICE_API void LUMICE_PathCancelTokenRequest(LUMICE_PathCancelToken* token);
LUMICE_API void LUMICE_PathCancelTokenDestroy(LUMICE_PathCancelToken* token);
LUMICE_API LUMICE_ErrorCode LUMICE_PathWarmDataFromSurvey(const LUMICE_PathSurvey* result, LUMICE_PathWarmData** out);
LUMICE_API LUMICE_ErrorCode LUMICE_PathWarmDataFromPoint(const LUMICE_SinglePathResult* result,
                                                         LUMICE_PathWarmData** out);
LUMICE_API LUMICE_ErrorCode LUMICE_PathWarmDataFromJson(const char* utf8, size_t length, LUMICE_PathWarmData** out,
                                                        char* err_buf, size_t err_size);
LUMICE_API void LUMICE_PathWarmDataDestroy(LUMICE_PathWarmData* warm);
LUMICE_API LUMICE_ErrorCode LUMICE_PathWorkspaceQueryPoint(LUMICE_PathWorkspace* workspace,
                                                           const LUMICE_SinglePathRequest* request,
                                                           const LUMICE_PathQueryOptions* options,
                                                           LUMICE_SinglePathResult** out, char* err_buf,
                                                           size_t err_size);
LUMICE_API LUMICE_ErrorCode LUMICE_PathWorkspaceQueryProbe(LUMICE_PathWorkspace* workspace,
                                                           const LUMICE_PathProbeRequest* request,
                                                           const LUMICE_PathQueryOptions* options,
                                                           LUMICE_PathProbe** out, char* err_buf, size_t err_size);
LUMICE_API LUMICE_ErrorCode LUMICE_PathWorkspaceQuerySurvey(LUMICE_PathWorkspace* workspace,
                                                            const LUMICE_PathSurveyRequest* request,
                                                            const LUMICE_PathQueryOptions* options,
                                                            LUMICE_PathSurvey** out, char* err_buf, size_t err_size);
LUMICE_API LUMICE_ErrorCode LUMICE_PathWorkspaceQueryReport(LUMICE_PathWorkspace* workspace,
                                                            const LUMICE_PathFeatureReportRequest* request,
                                                            const LUMICE_PathQueryOptions* options,
                                                            LUMICE_PathFeatureReport** out, char* err_buf,
                                                            size_t err_size);

// =============== Typed result readers ===============
// Every returned pointer is borrowed from the immutable result and remains valid until Destroy.
// Strings are UTF-8 and NUL-terminated; length excludes NUL. Read-only calls may overlap one
// another, but Destroy must wait for all reads and borrowed uses. The caller sets struct_size to
// sizeof the struct it compiled against. A released complete extent or larger future tail is
// accepted; unknown tail bytes are not written. Invalid size never writes even the first byte.
// On other failures, known output fields are zeroed, preserving caller struct_size.

typedef struct LUMICE_PathText {
  const char* data;
  size_t length;
} LUMICE_PathText;

typedef enum LUMICE_PathReadStatus {
  LUMICE_PATH_READ_OK = 0,
  LUMICE_PATH_READ_NULL_ARG = 1,
  LUMICE_PATH_READ_INVALID_SIZE = 2,
  LUMICE_PATH_READ_INVALID_INDEX = 3,
  LUMICE_PATH_READ_INVALID_KIND = 4,
  LUMICE_PATH_READ_NOT_AVAILABLE = 5
} LUMICE_PathReadStatus;

#define LUMICE_PATH_STATE_UNKNOWN 0
#define LUMICE_PATH_STATE_CERTIFIED 1
#define LUMICE_PATH_STATE_FINITE 2
#define LUMICE_PATH_STATE_REFUSED 3
#define LUMICE_PATH_STATE_UNEVALUATED 4
#define LUMICE_PATH_OUTCOME_UNKNOWN 0
#define LUMICE_PATH_OUTCOME_COMPLETED 1
#define LUMICE_PATH_OUTCOME_CANCELLED 2
#define LUMICE_PATH_OUTCOME_DEADLINE_EXCEEDED 3
#define LUMICE_PATH_POINT_MASS 1
#define LUMICE_PATH_POINT_DISCOVERED 2
#define LUMICE_PATH_PROBE_WARM_REUSED 1
#define LUMICE_PATH_PROBE_RECOMPUTED 2
#define LUMICE_PATH_DARK_INSIDE_POSITIVE 1
#define LUMICE_PATH_DARK_INSIDE_ZERO 2
#define LUMICE_PATH_DARK_OUTSIDE 3
#define LUMICE_PATH_DARK_UNKNOWN 4
#define LUMICE_PATH_ANCHOR_SUPPORT_ENDPOINT 1
#define LUMICE_PATH_ANCHOR_EDGE_POSITION 2
#define LUMICE_PATH_SURVEY_UNKNOWN 0
#define LUMICE_PATH_SURVEY_SOURCE_IMAGE 1
#define LUMICE_PATH_SURVEY_SUPPORT_SET 2
#define LUMICE_PATH_SURVEY_CAUSTIC_EDGE 3
#define LUMICE_PATH_SURVEY_BRIGHT_DOMAIN 4
#define LUMICE_PATH_SURVEY_FAMILY_COLLAPSE 5
#define LUMICE_PATH_SURVEY_REGULAR_IMAGE 6
#define LUMICE_PATH_REPORT_COMPLETED 1
#define LUMICE_PATH_REPORT_PARTIAL 2
#define LUMICE_PATH_REPORT_NO_RELATED_FEATURE 3
#define LUMICE_PATH_REPORT_UNSUPPORTED_MULTICRYSTAL 4
#define LUMICE_PATH_EXISTENCE_COMPUTED 1
#define LUMICE_PATH_EXISTENCE_ESCAPED 2
#define LUMICE_PATH_EXISTENCE_WALK_TRUNCATED 3
#define LUMICE_PATH_EXISTENCE_DECLARED 4
#define LUMICE_PATH_VISIBILITY_CERTIFIED 1
#define LUMICE_PATH_VISIBILITY_PARTIAL 2
#define LUMICE_PATH_VISIBILITY_UNLIT 3
#define LUMICE_PATH_VISIBILITY_UNPROVEN 4
#define LUMICE_PATH_MC_ACTUAL 1
#define LUMICE_PATH_MC_CANDIDATE 2
#define LUMICE_PATH_MC_UNFINISHED 3

// Semantic int fields use the named domain below. Zero is unrecognized/unavailable unless a
// domain explicitly starts at zero; raw zero-based domains use -1 for an unrecognized value.
#define LUMICE_PATH_TAG_UNKNOWN 0
#define LUMICE_PATH_RAW_TAG_UNKNOWN -1
#define LUMICE_PATH_UNIT_PER_SR 1
#define LUMICE_PATH_UNIT_DIMENSIONLESS 2
#define LUMICE_PATH_UNIT_AREA 3
#define LUMICE_PATH_UNIT_MASS 4
#define LUMICE_PATH_UNIT_PER_RAD_CUBED 5
#define LUMICE_PATH_UNIT_PER_RAD_SQUARED 6
#define LUMICE_PATH_AGGREGATION_REPRESENTATIVE 1
#define LUMICE_PATH_AGGREGATION_PHYSICAL_CLASS 2
#define LUMICE_PATH_AGGREGATION_MEMBER 3
#define LUMICE_PATH_MEASURE_ACTUAL_DECLARED 1
#define LUMICE_PATH_MEASURE_IDEAL_FROZEN 2
#define LUMICE_PATH_REFERENCE_HAAR 1
#define LUMICE_PATH_REFERENCE_ANGLE_LEBESGUE 2
#define LUMICE_PATH_REFERENCE_ORBIT_PARAMETER 3
#define LUMICE_PATH_REFERENCE_ACTUAL_PRODUCT 4
#define LUMICE_PATH_REFERENCE_NONE 5
#define LUMICE_PATH_DENSITY_POSE 1
#define LUMICE_PATH_DENSITY_NONE 2
#define LUMICE_PATH_DENSITY_DECLARED_POSE 3
#define LUMICE_PATH_DENSITY_ACTUAL_SAMPLING_WEIGHTS 4
#define LUMICE_PATH_SPECTRUM_REFERENCE_WAVELENGTH 1
#define LUMICE_PATH_SPECTRUM_FIXED_PAIR 2
#define LUMICE_PATH_SPECTRUM_DISCRETE 3
#define LUMICE_PATH_SPECTRUM_QUADRATURE 4
#define LUMICE_PATH_SOURCE_SOLAR_CENTER 1
#define LUMICE_PATH_SOURCE_SOLAR_DISK 2
#define LUMICE_PATH_GEOMETRY_NOMINAL 1
#define LUMICE_PATH_GEOMETRY_REALIZED 2
#define LUMICE_PATH_GEOMETRY_DECLARED_POPULATION 3
#define LUMICE_PATH_MEMBER_LITERAL 1
#define LUMICE_PATH_MEMBER_PHYSICAL_L2 2
#define LUMICE_PATH_LOCATION_STANDALONE 1
#define LUMICE_PATH_LOCATION_SCENE_LAYER 2
#define LUMICE_PATH_CONTEXT_WARM 1
#define LUMICE_PATH_CONTEXT_INLINE 2
#define LUMICE_PATH_RAW_SPECTRUM_DISCRETE 1
#define LUMICE_PATH_RAW_SPECTRUM_ILLUMINANT 2
#define LUMICE_PATH_CRYSTAL_PRISM 1
#define LUMICE_PATH_CRYSTAL_PYRAMID 2
#define LUMICE_PATH_DISTRIBUTION_FIXED 1
#define LUMICE_PATH_DISTRIBUTION_UNIFORM 2
#define LUMICE_PATH_DISTRIBUTION_GAUSSIAN 3
#define LUMICE_PATH_DISTRIBUTION_ZIGZAG 4
#define LUMICE_PATH_DISTRIBUTION_LAPLACIAN 5
#define LUMICE_PATH_DISTRIBUTION_LEGACY_GAUSSIAN 6
#define LUMICE_PATH_ILLUMINANT_D50 1
#define LUMICE_PATH_ILLUMINANT_D55 2
#define LUMICE_PATH_ILLUMINANT_D65 3
#define LUMICE_PATH_ILLUMINANT_D75 4
#define LUMICE_PATH_ILLUMINANT_A 5
#define LUMICE_PATH_ILLUMINANT_E 6
#define LUMICE_PATH_SPECTRUM_ORIGIN_DISCRETE_SUM 1
#define LUMICE_PATH_SPECTRUM_ORIGIN_SAMPLED 2
#define LUMICE_PATH_SPECTRUM_ORIGIN_QUADRATURE 3
#define LUMICE_PATH_FIELD_EQUATION_LOG_Y_PEAK 0
#define LUMICE_PATH_FIELD_EQUATION_LOG_Y_RIDGE 1
#define LUMICE_PATH_FIELD_EQUATION_CHROMATICITY_X 2
#define LUMICE_PATH_FIELD_EQUATION_CHROMATICITY_Y 3
#define LUMICE_PATH_FIELD_SOLVE_CONVERGED 1
#define LUMICE_PATH_FIELD_SOLVE_INVALID_INPUT 2
#define LUMICE_PATH_FIELD_SOLVE_NO_SIGNAL 3
#define LUMICE_PATH_FIELD_SOLVE_DEGENERATE 4
#define LUMICE_PATH_FIELD_SOLVE_ITERATION_LIMIT 5
#define LUMICE_PATH_FIELD_SOLVE_BUDGET_EXCEEDED 6
#define LUMICE_PATH_FIELD_WALK_CLOSED 1
#define LUMICE_PATH_FIELD_WALK_OBSERVATION_CENSORED 2
#define LUMICE_PATH_FIELD_WALK_CORRECTOR_FAILED 3
#define LUMICE_PATH_FIELD_WALK_POINT_LIMIT 4
#define LUMICE_PATH_FIELD_WALK_INVALID_INPUT 5
#define LUMICE_PATH_DIAGNOSTIC_POINT 1
#define LUMICE_PATH_DIAGNOSTIC_POLYLINE 2
#define LUMICE_PATH_DIAGNOSTIC_ATOM 3
#define LUMICE_PATH_DIAGNOSTIC_SOURCE_RANGE 4
#define LUMICE_PATH_DIAGNOSTIC_BAND 5
#define LUMICE_PATH_COVERAGE_BASE_INPUT 1
#define LUMICE_PATH_COVERAGE_BOUNDED_DISCOVERY 2
#define LUMICE_PATH_COVERAGE_DECLARED_SKIP 3
#define LUMICE_PATH_COVERAGE_SUPPORTED 1
#define LUMICE_PATH_COVERAGE_NUMERICAL_INCOMPLETE 2
#define LUMICE_PATH_COVERAGE_NOT_SUPPORTED 3
#define LUMICE_PATH_INPUT_OK 1
#define LUMICE_PATH_INPUT_INVALID_VALUE 2
#define LUMICE_PATH_INPUT_INVALID_CONFIG 3
#define LUMICE_PATH_CHAIN_NONE 1
#define LUMICE_PATH_CHAIN_PATH_INFEASIBLE 2
#define LUMICE_PATH_CHAIN_TIR_BOUNDARY 3
#define LUMICE_PATH_CHAIN_NON_FINITE 4
#define LUMICE_PATH_ENTRY_OK 1
#define LUMICE_PATH_ENTRY_BACKFACE 2
#define LUMICE_PATH_ENTRY_EXIT_CRITICAL_ANGLE 3
#define LUMICE_PATH_ENTRY_CORRIDOR_EMPTY 4
#define LUMICE_PATH_INTERFACE_SOLVE_CONVERGED 1
#define LUMICE_PATH_INTERFACE_SOLVE_INVALID_INPUT 2
#define LUMICE_PATH_INTERFACE_SOLVE_UNAVAILABLE 3
#define LUMICE_PATH_INTERFACE_SOLVE_NO_SUPPORT 4
#define LUMICE_PATH_INTERFACE_SOLVE_DEGENERATE 5
#define LUMICE_PATH_INTERFACE_SOLVE_ITERATION_LIMIT 6
#define LUMICE_PATH_INTERFACE_SOLVE_BUDGET_EXCEEDED 7
#define LUMICE_PATH_INTERFACE_WALK_CLOSED 1
#define LUMICE_PATH_INTERFACE_WALK_AREA_THRESHOLD 2
#define LUMICE_PATH_INTERFACE_WALK_GEOMETRIC_CONTACT 3
#define LUMICE_PATH_INTERFACE_WALK_OPTICAL_GATE 4
#define LUMICE_PATH_INTERFACE_WALK_CORRECTOR_FAILED 5
#define LUMICE_PATH_INTERFACE_WALK_POINT_LIMIT 6
#define LUMICE_PATH_INTERFACE_WALK_BUDGET_EXCEEDED 7
#define LUMICE_PATH_INTERFACE_WALK_INVALID_INPUT 8
#define LUMICE_PATH_SUPPORT_COMPLETE 1
#define LUMICE_PATH_SUPPORT_INCOMPLETE 2
#define LUMICE_PATH_SUPPORT_UNKNOWN 3
#define LUMICE_PATH_ESCAPE_SLAB_CREASE 1
#define LUMICE_PATH_WALK_OK 1
#define LUMICE_PATH_WALK_STEPS_EXHAUSTED 2
#define LUMICE_PATH_WALK_START_NO_POINT 3
#define LUMICE_PATH_WALK_START_COVERS_ALL 4
#define LUMICE_PATH_WALK_START_NO_EDGE 5
#define LUMICE_PATH_WALK_CORNER_NOT_SIMPLE 6
#define LUMICE_PATH_WALK_NOT_CLOSED 7
#define LUMICE_PATH_WALK_NOT_FINITE 8
#define LUMICE_PATH_WALK_BAD_ORIENTATION 9
#define LUMICE_PATH_WALK_STOP_REQUESTED 10
#define LUMICE_PATH_ORBIT_SOLID_ANGLE 1
#define LUMICE_PATH_ORBIT_FIBER_PARAMETER 2
#define LUMICE_PATH_ORBIT_STRUCTURAL 1
#define LUMICE_PATH_ORBIT_SAMPLED_EXHAUSTIVE 2
#define LUMICE_PATH_ORBIT_SAMPLED_PARTIAL 3
#define LUMICE_PATH_ONSET_INTERIOR 1
#define LUMICE_PATH_ONSET_BOUNDARY 2
#define LUMICE_PATH_ONSET_INTERIOR_MINIMUM 1
#define LUMICE_PATH_ONSET_INTERIOR_MAXIMUM 2
#define LUMICE_PATH_ONSET_INTERIOR_SADDLE 3
#define LUMICE_PATH_ONSET_INTERIOR_DEGENERATE 4
#define LUMICE_PATH_ONSET_SLAB_AXIS 5
#define LUMICE_PATH_ONSET_SLAB_CIRCLE 6
#define LUMICE_PATH_ONSET_BOUNDARY_EXTREMUM 7
#define LUMICE_PATH_ONSET_CORNER 8
#define LUMICE_PATH_ONSET_PROFILE_FINITE_JUMP 1
#define LUMICE_PATH_ONSET_PROFILE_LOG_DIVERGENCE 2
#define LUMICE_PATH_ONSET_PROFILE_INVERSE_SQRT_DIVERGENCE 3
#define LUMICE_PATH_ONSET_PROFILE_CONE_POINT 4
#define LUMICE_PATH_ONSET_PROFILE_CREASE 5
#define LUMICE_PATH_ONSET_PROFILE_BOUNDARY_ONSET 6
#define LUMICE_PATH_ONSET_PROFILE_DEGENERATE 7
#define LUMICE_PATH_CORROBORATION_OBSERVED 1
#define LUMICE_PATH_CORROBORATION_CONSISTENT 2
#define LUMICE_PATH_CORROBORATION_NOT_OBSERVED_INSUFFICIENT_ESS 3
#define LUMICE_PATH_CORROBORATION_NOT_OBSERVED_SUFFICIENT_ESS 4
#define LUMICE_PATH_CORROBORATION_UNCHECKED 5
#define LUMICE_PATH_ABSENCE_ZERO_SPECTRAL_SIGNAL 1
#define LUMICE_PATH_ABSENCE_CERTIFIED_SMOOTH_RADIANCE 2
#define LUMICE_PATH_BUCKET_ACTUAL 1
#define LUMICE_PATH_BUCKET_CANDIDATE 2
#define LUMICE_PATH_BUCKET_UNFINISHED 3
#define LUMICE_PATH_MC_UNKNOWN 0

typedef enum LUMICE_PathQuantityKind {
  LUMICE_PATH_QUANTITY_BAND_SUM = 1,
  LUMICE_PATH_QUANTITY_TINT_RATIO = 2,
  LUMICE_PATH_QUANTITY_TIR_SHARE = 3,
  LUMICE_PATH_QUANTITY_MC_ESS = 4,
  LUMICE_PATH_QUANTITY_WITNESS_WEIGHT = 5,
  LUMICE_PATH_QUANTITY_ENTRY_AREA = 6,
  LUMICE_PATH_QUANTITY_TRANSMISSION = 7,
  LUMICE_PATH_QUANTITY_POSE_DENSITY = 8,
  LUMICE_PATH_QUANTITY_NORMAL_JACOBIAN = 9,
  LUMICE_PATH_QUANTITY_COAREA_FACTOR = 10,
  LUMICE_PATH_QUANTITY_FACTOR_PRODUCT = 11,
  LUMICE_PATH_QUANTITY_FIBER_INTEGRAL = 12,
  LUMICE_PATH_QUANTITY_ORBIT_INTEGRAL = 13
} LUMICE_PathQuantityKind;

typedef enum LUMICE_PathQuantityMethod {
  LUMICE_PATH_METHOD_BAND_SUM_LATTICE = 1,
  LUMICE_PATH_METHOD_POINT_MASS_PSI_AVERAGE = 2,
  LUMICE_PATH_METHOD_POINT_MASS_LATTICE_MEAN = 3,
  LUMICE_PATH_METHOD_CLASS_LATTICE_POWER_SUM = 4,
  LUMICE_PATH_METHOD_SINGLE_POSE_DIRECT = 5,
  LUMICE_PATH_METHOD_POINT_MASS_BAND_SUM = 6,
  LUMICE_PATH_METHOD_DIAGNOSTIC_TIR_LATTICE = 7,
  LUMICE_PATH_METHOD_KISH_OUTER_DRAW = 8,
  LUMICE_PATH_METHOD_LATTICE_ARGMAX_DIAGNOSTIC = 9,
  LUMICE_PATH_METHOD_ENTRY_MEASURE = 10,
  LUMICE_PATH_METHOD_INTERFACE_PRODUCT = 11,
  LUMICE_PATH_METHOD_DECLARED_POSE_DENSITY = 12,
  LUMICE_PATH_METHOD_NORMAL_JACOBIAN = 13,
  LUMICE_PATH_METHOD_REGULARIZED_COAREA = 14,
  LUMICE_PATH_METHOD_FACTOR_PRODUCT = 15,
  LUMICE_PATH_METHOD_FIBER_ARCLENGTH = 16,
  LUMICE_PATH_METHOD_ORBIT_MIDPOINT = 17
} LUMICE_PathQuantityMethod;

typedef struct LUMICE_PathQuantitySpec {
  size_t struct_size;
  int kind;         // LUMICE_PATH_QUANTITY_*
  int method;       // LUMICE_PATH_METHOD_*
  int unit;         // LUMICE_PATH_UNIT_*
  int aggregation;  // LUMICE_PATH_AGGREGATION_*
  int measure;      // LUMICE_PATH_MEASURE_*
  int reference;    // LUMICE_PATH_REFERENCE_*
  int density;      // LUMICE_PATH_DENSITY_*
  int spectrum;     // LUMICE_PATH_SPECTRUM_*
  int source;       // LUMICE_PATH_SOURCE_*
  int shape;        // LUMICE_PATH_GEOMETRY_*
  int wavelength_available;
  double wavelength_nm;
  int sun_altitude_available;
  double sun_altitude_deg;
  int sun_azimuth_available;
  double sun_azimuth_deg;
  const double* wavelengths_nm;
  size_t wavelength_count;
  double regularization_epsilon;
  int scope_available;
  int scope_member_kind;  // LUMICE_PATH_MEMBER_*
  int scope_location;     // LUMICE_PATH_LOCATION_*
  size_t scope_layer_index;
  size_t scope_member_count;
  int scope_geometry;   // LUMICE_PATH_GEOMETRY_*
  int scope_measure;    // LUMICE_PATH_MEASURE_*
  int scope_reference;  // LUMICE_PATH_REFERENCE_*
} LUMICE_PathQuantitySpec;

LUMICE_API LUMICE_PathReadStatus LUMICE_PathProbeGetQuantity(const LUMICE_PathProbe* result, size_t profile,
                                                             LUMICE_PathQuantitySpec* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetProbeQuantity(const LUMICE_PathSurvey* result, size_t probe,
                                                                   LUMICE_PathQuantitySpec* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetObjectQuantity(const LUMICE_PathSurvey* result, size_t object,
                                                                    LUMICE_PathQuantitySpec* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetTintQuantity(const LUMICE_PathSurvey* result,
                                                                  LUMICE_PathQuantitySpec* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetTirQuantity(const LUMICE_PathSurvey* result,
                                                                 LUMICE_PathQuantitySpec* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetEssQuantity(const LUMICE_PathSurvey* result,
                                                                 LUMICE_PathQuantitySpec* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetWitnessQuantity(const LUMICE_PathSurvey* result, size_t object,
                                                                     LUMICE_PathQuantitySpec* out);

// A single four-arm evidence contract across all survey capabilities. For DOCUMENT/CHROMATIC/
// FAMILY/DISPERSION/DIAGNOSE/TINT/TIR/ESS, index must be 0. MEMBER/OBJECT/PROBE/WITNESS
// use their respective collection index. Kind selects only this fixed evidence type, not a
// reflected property or an alternate business result.
#define LUMICE_PATH_EVIDENCE_DOCUMENT 1
#define LUMICE_PATH_EVIDENCE_CHROMATIC 2
#define LUMICE_PATH_EVIDENCE_FAMILY 3
#define LUMICE_PATH_EVIDENCE_MEMBER 4
#define LUMICE_PATH_EVIDENCE_OBJECT 5
#define LUMICE_PATH_EVIDENCE_DISPERSION 6
#define LUMICE_PATH_EVIDENCE_DIAGNOSE 7
#define LUMICE_PATH_EVIDENCE_TINT 8
#define LUMICE_PATH_EVIDENCE_TIR 9
#define LUMICE_PATH_EVIDENCE_ESS 10
#define LUMICE_PATH_EVIDENCE_PROBE 11
#define LUMICE_PATH_EVIDENCE_WITNESS 12

typedef struct LUMICE_PathSurveyEvidence {
  size_t struct_size;
  int state;  // LUMICE_PATH_STATE_*
  LUMICE_PathText method;
  LUMICE_PathText mode;
  LUMICE_PathText reason;
  int coverage_available;
  int members_evaluated;
  int64_t sample_count;
  LUMICE_PathText geometry_basis;
  double wavelength_nm;
} LUMICE_PathSurveyEvidence;

LUMICE_API LUMICE_PathReadStatus LUMICE_PathProbeGetEvidence(const LUMICE_PathProbe* result, size_t profile,
                                                             LUMICE_PathSurveyEvidence* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetEvidence(const LUMICE_PathSurvey* result, int kind, size_t index,
                                                              LUMICE_PathSurveyEvidence* out);

// Fixed root metadata and the counts that bound all indexed readers below.
typedef struct LUMICE_PathPointInfo {
  size_t struct_size;
  int schema_version;
  int outcome;    // LUMICE_PATH_POINT_*
  int execution;  // LUMICE_PATH_OUTCOME_*
  int crystal_id;
  int analytic_api_version;
  LUMICE_PathText crystal_kind;
  const int* faces;
  size_t face_count;
  size_t shape_count;
  int shape_is_nominal;
  double upper_wedge_deg;
  double lower_wedge_deg;
  double sun_altitude_deg;
  double sun_azimuth_deg;
  double sun_diameter_deg;
  double incident_direction[3];
  double target_altitude_deg;
  double target_azimuth_deg;
  double target_direction[3];
  double target_deviation_deg;
  double wavelength_nm;
  LUMICE_PathText wavelength_source;
  double refractive_index;
  int sample_count;
  double band_half_width_rad;
  double cluster_radius_rad;
  double distance_threshold_rad;
  int warm_seed_count;
  int budget_ms;
  int interrupted;
  size_t component_count;
  size_t incomplete_count;
  int grid_lat_count;
  int grid_lon_count;
} LUMICE_PathPointInfo;

typedef struct LUMICE_PathPointComponent {
  size_t struct_size;
  LUMICE_PathText kind;
  double seed[9];
  LUMICE_PathText forward_status;
  LUMICE_PathText forward_reason;
  int forward_pose_count;
  LUMICE_PathText backward_status;
  LUMICE_PathText backward_reason;
  int backward_pose_count;
  size_t point_count;
  int seed_index;
  const double* arclength_increments;
  size_t arclength_count;
  int integral_available;
  double fiber_integral;
  int available_points;
  int total_points;
} LUMICE_PathPointComponent;

typedef struct LUMICE_PathPointSample {
  size_t struct_size;
  double pose[9];
  double zenith_deg;
  double azimuth_deg;
  double roll_deg;
  int angles_degenerate;
  double sun_in_crystal[3];
  double outgoing_direction[3];
  double residual_norm;
  int valid;
  double total_transmission;
  double entry_measure;
  double rho;
  double entry_measure_a2;
  int jacobian_available;
  double jacobian;
  double singular_values[2];
  double k_carea;
  double factor_product;
  const double* segment_directions;
  size_t segment_direction_count;
  const double* interface_transmittances;
  size_t interface_count;
} LUMICE_PathPointSample;

typedef struct LUMICE_PathSunGrid {
  size_t struct_size;
  int lat_count;
  int lon_count;
  const double* deviation_rad;
  const uint8_t* valid;
  const double* entry_measure;
  size_t cell_count;
} LUMICE_PathSunGrid;

// Probe profiles are indexed by wavelength; an unavailable claim has no samples to index.
typedef struct LUMICE_PathProbeInfo {
  size_t struct_size;
  int execution;  // LUMICE_PATH_OUTCOME_*
  int crystal_id;
  int analytic_api_version;
  LUMICE_PathText crystal_kind;
  const int* faces;
  size_t face_count;
  size_t shape_count;
  int shape_is_nominal;
  double upper_wedge_deg;
  double lower_wedge_deg;
  double sun_altitude_deg;
  double sun_azimuth_deg;
  double incident_direction[3];
  double at_altitude_deg;
  double at_azimuth_deg;
  double at_direction[3];
  double at_deviation_deg;
  double at_azimuth_about_sun_deg;
  int budget_ms;
  int interrupted;
  size_t wavelength_count;
  size_t profile_count;
  int red_edge_width_available;
  double red_edge_width_deg;
  LUMICE_PathText red_edge_rule;
} LUMICE_PathProbeInfo;

typedef struct LUMICE_PathProbeProfile {
  size_t struct_size;
  int along;  // LUMICE_PATH_PROBE_ALONG_* (0..3); -1 if unrecognized
  int state;  // LUMICE_PATH_STATE_*
  LUMICE_PathText reason;
  double wavelength_nm;
  double refractive_index;
  double delta_min_deg;
  double delta_max_deg;
  double azimuth_deg;
  double window_half_width_deg;
  double bin_deg;
  double wedge_half_width_deg;
  int anchor_available;
  int anchor_basis;  // LUMICE_PATH_ANCHOR_* when anchor_available
  double anchor_deg;
  double anchor_wavelength_nm;
  LUMICE_PathText semantics;
  LUMICE_PathText lattice;
  int deterministic;
  int member_count;
  int sample_count;
  int source_available;
  int source;  // LUMICE_PATH_PROBE_WARM_REUSED / LUMICE_PATH_PROBE_RECOMPUTED when available
  int members_evaluated;
  const int* representative;
  size_t representative_count;
  size_t sample_row_count;
  size_t dark_region_count;
  int edge_available;
  double edge_deg;
  int peak_available;
  double peak_deg;
  double peak_value;
  int separation_available;
  double separation_deg;
} LUMICE_PathProbeProfile;

typedef struct LUMICE_PathProbeDarkRegion {
  size_t struct_size;
  int kind;  // LUMICE_PATH_DARK_*
  double lo_deg;
  double hi_deg;
  int bin_count;
  LUMICE_PathText reason;
} LUMICE_PathProbeDarkRegion;

LUMICE_API LUMICE_PathReadStatus LUMICE_PathProbeGetDarkRegion(const LUMICE_PathProbe* result, size_t profile,
                                                               size_t index, LUMICE_PathProbeDarkRegion* out);

typedef struct LUMICE_PathProbeSample {
  size_t struct_size;
  double delta_deg;
  int azimuth_available;
  double azimuth_deg;
  double value;
  int k;
  double k_eff;
} LUMICE_PathProbeSample;

// Survey object kind is the variant tag. Numeric payloads are readable only under their kind
// and successful evidence state; absent values use availability flags, never defaulted zero.
typedef struct LUMICE_PathSurveyInfo {
  size_t struct_size;
  int execution;  // LUMICE_PATH_OUTCOME_*
  int schema_version;
  int64_t crystal_id;
  unsigned symmetry_bits;
  double wavelength_nm;
  int budget_ms;
  int early_stopped;
  int evidence_state;  // LUMICE_PATH_STATE_*
  size_t member_count;
  size_t object_count;
  size_t unevaluated_count;
  size_t probe_count;
  size_t warm_seed_count;
  int family_available;
  int tint_available;
  int tir_available;
  int ess_available;
} LUMICE_PathSurveyInfo;

typedef struct LUMICE_PathSurveyObject {
  size_t struct_size;
  int kind;   // LUMICE_PATH_SURVEY_*
  int state;  // LUMICE_PATH_STATE_*
  LUMICE_PathText method;
  LUMICE_PathText mode;
  LUMICE_PathText reason;
  size_t member_count;
  int position_available;
  double position_altitude_deg;
  double position_azimuth_deg;
  int band_available;
  int family_level;
  double blur_deg;
  double scale_deg;
  size_t support_interval_count;
  int witness_available;
} LUMICE_PathSurveyObject;

typedef struct LUMICE_PathFamilyInfo {
  size_t struct_size;
  int state;         // LUMICE_PATH_STATE_*         // LUMICE_PATH_STATE_*
  int measure_kind;  // LUMICE_PATH_MEASURE_*
  LUMICE_PathText level0_refusal_mode;
  size_t ladder_count;
} LUMICE_PathFamilyInfo;

typedef struct LUMICE_PathFamilyLevel {
  size_t struct_size;
  int level;
  size_t frozen_dim_count;
  size_t free_dim_count;
  int rank_asked;
  int generic_rank;
  int grid;
  double grid_spacing_deg[2];
  int samples;
  int valid_samples;
  int rank_refused_members;
  int objects_proposed;
  int objects_killed;
  size_t killed_count;
} LUMICE_PathFamilyLevel;

typedef struct LUMICE_PathTint {
  size_t struct_size;
  int state;  // LUMICE_PATH_STATE_*
  int reading_available;
  int class_power;
  int reading_unit;  // LUMICE_PATH_UNIT_* when reading_available
  LUMICE_PathText red_label;
  LUMICE_PathText blue_label;
  double n_red;
  double n_blue;
  LUMICE_PathText method;
  LUMICE_PathText direction;
  LUMICE_PathText lattice;
  int deterministic;
  const int* representative;
  size_t representative_count;
  int member_count;
  double ratio;
  double red;
  double blue;
  double red_error;
  double blue_error;
  int64_t sample_count;
  int64_t kept_count_red;
  int64_t kept_count_blue;
  LUMICE_PathText caliber;
  LUMICE_PathText mode;
} LUMICE_PathTint;

typedef struct LUMICE_PathTirShare {
  size_t struct_size;
  int state;  // LUMICE_PATH_STATE_*
  int value_available;
  double n_red;
  double n_blue;
  int64_t sample_count;
  int64_t kept_count_red;
  int64_t kept_count_blue;
  LUMICE_PathText lattice;
  int deterministic;
  const int* representative;
  size_t representative_count;
  int member_count;
  LUMICE_PathText caliber;
  double total_weight_red;
  double total_weight_blue;
  double all_tir_weight_red;
  double all_tir_weight_blue;
  double share_all_tir_red;
  double share_all_tir_blue;
  int64_t chart_degenerate_red;
  int64_t chart_degenerate_blue;
  size_t interface_count;
  LUMICE_PathText mode;
} LUMICE_PathTirShare;

typedef struct LUMICE_PathTirInterface {
  size_t struct_size;
  int face;
  double share_red;
  double share_blue;
  double tir_weight_red;
  double tir_weight_blue;
} LUMICE_PathTirInterface;

typedef struct LUMICE_PathMcEss {
  size_t struct_size;
  int state;  // LUMICE_PATH_STATE_*
  int value_available;
  int64_t seed;
  int64_t draws;
  double wavelength_nm;
  double band_deg;
  int deterministic;
  size_t band_count;
  LUMICE_PathText mode;
} LUMICE_PathMcEss;

typedef struct LUMICE_PathMcEssBand {
  size_t struct_size;
  double delta_min_deg;
  double delta_max_deg;
  double ess;
  int64_t draw_count;
  double sum_weight;
  double sum_squared_weight;
} LUMICE_PathMcEssBand;

typedef struct LUMICE_PathWarmSeed {
  size_t struct_size;
  double pose[9];
  LUMICE_PathText source;
} LUMICE_PathWarmSeed;

// Report object and MC records have independent tables; the record index in an annotation is
// local to that result, never a persistent ID. The absence/ruling state is separate from execution.
typedef struct LUMICE_PathReportInfo {
  size_t struct_size;
  int execution;  // LUMICE_PATH_OUTCOME_*
  int outcome;    // LUMICE_PATH_REPORT_*
  size_t object_count;
  size_t record_count;
  size_t source_count;
  size_t coverage_count;
  size_t declared_skip_count;
  size_t produced_kind_count;
  size_t support_member_count;
  size_t support_family_count;
  size_t orbit_count;
  size_t unattributed_count;
  uint64_t requested_outer_samples;
  uint64_t completed_outer_samples;
  uint64_t optical_evaluations;
  int budget_ms;
  int absence_issued;
  size_t requested_layer_count;
} LUMICE_PathReportInfo;

typedef struct LUMICE_PathReportObject {
  size_t struct_size;
  LUMICE_PathText kind;
  const int* member;
  size_t member_face_count;
  int slot;
  LUMICE_PathText phi_class_note;
  LUMICE_PathText escape_regime_slug;
  int bucket;      // LUMICE_PATH_BUCKET_*
  int existence;   // LUMICE_PATH_EXISTENCE_*
  int visibility;  // LUMICE_PATH_VISIBILITY_*
  int chromatic_assessed;
  int corroboration;  // LUMICE_PATH_CORROBORATION_*
  int sky_position_available;
  double sky_position[3];
  const double* u;
  size_t u_count;
  double walk_s;
  int counterfactual_available;
  double counterfactual_with_slot;
  double counterfactual_without_slot;
} LUMICE_PathReportObject;

typedef struct LUMICE_PathReportVisibility {
  size_t struct_size;
  int state;  // LUMICE_PATH_VISIBILITY_*
  LUMICE_PathText state_name;
  int lit_fraction_available;
  double lit_fraction;
  LUMICE_PathText evidence_form;
  int jets_ok;
  int saw_zero_area;
  int saw_zero_transmission;
  LUMICE_PathText reason;
} LUMICE_PathReportVisibility;

typedef struct LUMICE_PathReportChromatic {
  size_t struct_size;
  int assessed;
  LUMICE_PathText verdict_kind;
  LUMICE_PathText color;
  int visible;
  int position_available;
  double position_rad;
  size_t face_count;
  const int* faces;
  size_t feature_count;
  size_t note_count;
  int tint_available;
  double tint_energy_red;
  double tint_energy_blue;
  double tint_ratio;
  double tint_tir_fraction_red;
  double tint_tir_fraction_blue;
  double tint_direction_dispersion;
  int coverage_complete;
  double n_red;
  double n_blue;
  double threshold_n_red;
  double threshold_n_blue;
  double edge_min_shift_rad;
  double edge_spread_per_shift;
  double calibration_white_max_deviation;
  double tint_ratio_min;
} LUMICE_PathReportChromatic;

typedef struct LUMICE_PathReportChromaticFeature {
  size_t struct_size;
  LUMICE_PathText kind;
  LUMICE_PathText source;
  LUMICE_PathText color;
  double positive_fraction;
  double delta_red;
  double delta_blue;
  double shift;
  double spread;
  double direction_dispersion;
  double contrast;
  double weight;
  double lit_fraction;
  int visible;
} LUMICE_PathReportChromaticFeature;

LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetVisibility(const LUMICE_PathFeatureReport* result, size_t object,
                                                                LUMICE_PathReportVisibility* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetChromatic(const LUMICE_PathFeatureReport* result, size_t object,
                                                               LUMICE_PathReportChromatic* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetChromaticFeature(const LUMICE_PathFeatureReport* result,
                                                                      size_t object, size_t index,
                                                                      LUMICE_PathReportChromaticFeature* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetChromaticNote(const LUMICE_PathFeatureReport* result,
                                                                   size_t object, size_t index, LUMICE_PathText* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetProducedKind(const LUMICE_PathFeatureReport* result, size_t index,
                                                                  LUMICE_PathText* out);

typedef struct LUMICE_PathReportRecord {
  size_t struct_size;
  int evidence;  // LUMICE_PATH_MC_*
  int geometry;  // LUMICE_PATH_DIAGNOSTIC_*
  LUMICE_PathText kind;
  LUMICE_PathText reason;
  size_t sky_point_count;
  size_t field_point_count;
  size_t interface_curve_count;
  size_t source_parameter_count;
  int source_token_available;
  uint64_t source_token;
  int connected_record_available;
  size_t connected_record;
  double level;
  double bandwidth_rad;
  int replicate_movement_available;
  double replicate_movement_rad;
  int scale_movement_available;
  double scale_movement_rad;
  double minimum_effective_samples;
  int equation;  // LUMICE_PATH_FIELD_EQUATION_* (0..3); -1 if unrecognized
  int contributor_fraction_available;
  double contributor_fraction_of_estimated_y;
} LUMICE_PathReportRecord;

typedef struct LUMICE_PathReportCoverage {
  size_t struct_size;
  int role;    // LUMICE_PATH_COVERAGE_BASE_INPUT / BOUNDED_DISCOVERY / DECLARED_SKIP
  int status;  // LUMICE_PATH_COVERAGE_SUPPORTED / NUMERICAL_INCOMPLETE / NOT_SUPPORTED
  LUMICE_PathText subject;
  LUMICE_PathText reason;
  size_t limitation_count;
} LUMICE_PathReportCoverage;

typedef struct LUMICE_PathReportDeclaredSkip {
  size_t struct_size;
  LUMICE_PathText kind;
  LUMICE_PathText reason;
} LUMICE_PathReportDeclaredSkip;

typedef struct LUMICE_PathShapeScalar {
  size_t struct_size;
  LUMICE_PathText name;
  double value;
  LUMICE_PathText distribution;
  double spread;
} LUMICE_PathShapeScalar;

typedef struct LUMICE_PathPointMass {
  size_t struct_size;
  double direction[3];
  double altitude_deg;
  double azimuth_deg;
  double target_separation_deg;
  int orbit_available;
  LUMICE_PathText orbit_reason;
  LUMICE_PathText measure_support;
  int orbit_sample_count;
  double orbit_integral;
  int orbit_kept_count;
  size_t orbit_point_count;
} LUMICE_PathPointMass;

typedef struct LUMICE_PathOrbitPoint {
  size_t struct_size;
  double theta_rad;
  double rho_theta;
  double entry_measure_a2;
  double total_transmission;
  int valid;
} LUMICE_PathOrbitPoint;

LUMICE_API LUMICE_PathReadStatus LUMICE_PathPointGetOrbitPoint(const LUMICE_SinglePathResult* result, size_t index,
                                                               LUMICE_PathOrbitPoint* out);

typedef struct LUMICE_PathPointDiscovery {
  size_t struct_size;
  int complete;
  int pool_count;
  int extra_seed_count;
  int raw_cluster_count;
  int admissible_count;
  int dedup_merged;
  int arc_stitched;
  int arc_backward_failed;
  int arc_backward_closed_anomaly;
  int incomplete_unnamed_event;
  int incomplete_not_converged;
  LUMICE_PathText completeness_note;
} LUMICE_PathPointDiscovery;

typedef struct LUMICE_PathPointReach {
  size_t struct_size;
  int target_in_range;
  int probe_lat_count;
  double deviation_min_rad;
  double deviation_max_rad;
  double target_deviation_rad;
  double tolerance_rad;
} LUMICE_PathPointReach;

typedef struct LUMICE_PathMeasureRegistration {
  size_t struct_size;
  LUMICE_PathText support_kind;
  int fast_path;
  int registered_gap;
  LUMICE_PathText note;
} LUMICE_PathMeasureRegistration;

typedef struct LUMICE_PathIncomplete {
  size_t struct_size;
  LUMICE_PathText cause;
  double seed[9];
  LUMICE_PathText forward_status;
  LUMICE_PathText forward_reason;
  int forward_pose_count;
  LUMICE_PathText backward_status;
  LUMICE_PathText backward_reason;
  int backward_pose_count;
} LUMICE_PathIncomplete;

typedef struct LUMICE_PathContext {
  size_t struct_size;
  int source;  // LUMICE_PATH_CONTEXT_*
  int warm_index_absent;
  double reference_wavelength_nm;
  int object_state;
  LUMICE_PathText object_reason;
  int object_kind_available;
  int object_kind;
  int target_inside_support;
  const int* object_members;
  size_t object_member_count;
  double object_distance_deg;
  int edge_state;
  LUMICE_PathText edge_reason;
  double edge_distance_deg;
  int edge_basis;  // LUMICE_PATH_ANCHOR_* if edge is available
  int band_sum_state;
  LUMICE_PathText band_sum_reason;
  double band_sum_value;
  double band_sum_delta_deg;
  double band_sum_bin_deg;
  LUMICE_PathText band_sum_caliber;
} LUMICE_PathContext;

typedef struct LUMICE_PathProbeWavelength {
  size_t struct_size;
  double nm;
  LUMICE_PathText source;
  double refractive_index;
} LUMICE_PathProbeWavelength;

LUMICE_API LUMICE_PathReadStatus LUMICE_PathPointGetShape(const LUMICE_SinglePathResult* result, size_t index,
                                                          LUMICE_PathShapeScalar* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathProbeGetShape(const LUMICE_PathProbe* result, size_t index,
                                                          LUMICE_PathShapeScalar* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathPointGetMass(const LUMICE_SinglePathResult* result,
                                                         LUMICE_PathPointMass* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathPointGetDiscovery(const LUMICE_SinglePathResult* result,
                                                              LUMICE_PathPointDiscovery* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathPointGetReach(const LUMICE_SinglePathResult* result,
                                                          LUMICE_PathPointReach* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathPointGetMeasureRegistration(const LUMICE_SinglePathResult* result,
                                                                        LUMICE_PathMeasureRegistration* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathPointGetIncomplete(const LUMICE_SinglePathResult* result, size_t index,
                                                               LUMICE_PathIncomplete* out);
// include_context=0 leaves this block unavailable; ToJson still reports "not_requested" fields.
LUMICE_API LUMICE_PathReadStatus LUMICE_PathPointGetContext(const LUMICE_SinglePathResult* result,
                                                            LUMICE_PathContext* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathProbeGetContext(const LUMICE_PathProbe* result, LUMICE_PathContext* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathProbeGetWavelength(const LUMICE_PathProbe* result, size_t index,
                                                               LUMICE_PathProbeWavelength* out);

typedef struct LUMICE_PathChromatic {
  size_t struct_size;
  int state;  // LUMICE_PATH_STATE_*
  LUMICE_PathText method;
  LUMICE_PathText mode;
  LUMICE_PathText reason;
  int dispersion_available;
  int diagnose_available;
} LUMICE_PathChromatic;

typedef struct LUMICE_PathDispersion {
  size_t struct_size;
  LUMICE_PathText red_label;
  LUMICE_PathText blue_label;
  double n_red;
  double n_blue;
  int min_row_available;
  double value_red_deg;
  double value_blue_deg;
  double displacement_deg;
  LUMICE_PathText direction;
  size_t followed_count;
  int evidence_state;  // LUMICE_PATH_STATE_*
} LUMICE_PathDispersion;

typedef struct LUMICE_PathFollowedOnset {
  size_t struct_size;
  LUMICE_PathText source;
  LUMICE_PathText profile;
  const double* values_deg;
  size_t value_count;
  double displacement_deg;
  LUMICE_PathText direction;
} LUMICE_PathFollowedOnset;

typedef struct LUMICE_PathDiagnose {
  size_t struct_size;
  LUMICE_PathText verdict_kind;
  LUMICE_PathText color;
  int visible;
  int position_available;
  double position_deg;
  int coverage_complete;
  double n_red;
  double n_blue;
  size_t feature_count;
  size_t note_count;
  int evidence_state;  // LUMICE_PATH_STATE_*
} LUMICE_PathDiagnose;

typedef struct LUMICE_PathDiagnoseFeature {
  size_t struct_size;
  LUMICE_PathText kind;
  LUMICE_PathText source;
  LUMICE_PathText color;
  int visible;
  double delta_red_deg;
  double delta_blue_deg;
  double shift_deg;
} LUMICE_PathDiagnoseFeature;

typedef struct LUMICE_PathSurveyProbe {
  size_t struct_size;
  int state;  // LUMICE_PATH_STATE_*
  double wavelength_nm;
  double delta_min_deg;
  double delta_max_deg;
  double bin_deg;
  double wedge_half_width_deg;
  int azimuth_available;
  double azimuth_deg;
  double refractive_index;
  LUMICE_PathText lattice;
  int deterministic;
  int member_count;
  int64_t sample_count;
  const int* representative;
  size_t representative_count;
  size_t sample_row_count;
  int edge_available;
  int edge_bin;
  double edge_deg;
  int peak_available;
  double peak_deg;
  double peak_value;
  int separation_available;
  double separation_deg;
  int64_t kept_count;
  LUMICE_PathText reason;
} LUMICE_PathSurveyProbe;

typedef struct LUMICE_PathWitness {
  size_t struct_size;
  int state;  // LUMICE_PATH_STATE_*
  int value_available;
  int64_t kept_count;
  int64_t sample_count;
  LUMICE_PathText lattice;
  int deterministic;
  double deviation_deg;
  double weight;
  double entry;
  double interface_product;
  LUMICE_PathText source_note;
  LUMICE_PathText mode;
} LUMICE_PathWitness;

typedef struct LUMICE_PathReportRuling {
  size_t struct_size;
  int issued;
  int basis_available;
  int basis;  // LUMICE_PATH_ABSENCE_* when issued
  int partition_complete_no_escape;
  LUMICE_PathText partition_failure;
  int all_objects_unlit_or_none;
  LUMICE_PathText unlit_failure;
  int no_sufficient_ess_unattributed;
  LUMICE_PathText unattributed_failure;
  int s4_scope_declared;
  LUMICE_PathText s4_failure;
  int two_d_valid_support;
  LUMICE_PathText s4_scope_note;
  LUMICE_PathText absence_not_proven_note;
} LUMICE_PathReportRuling;

typedef struct LUMICE_PathSurveyScope {
  size_t struct_size;
  LUMICE_PathText scene_identity;
  int scene_layer_available;
  int64_t scene_layer;
  double wavelength_nm;
  int request_wavelength_given;
  double request_wavelength_nm;
  int member_kind;    // LUMICE_PATH_MEMBER_*
  int geometry_kind;  // LUMICE_PATH_GEOMETRY_*
  LUMICE_PathText wavelength_source;
  double refractive_index;
  int sun_altitude_available;
  double sun_altitude_deg;
  int sun_azimuth_available;
  double sun_azimuth_deg;
  size_t request_layer_count;
  size_t routing_member_count;
  LUMICE_PathText warm_reuse_scope;
  LUMICE_PathText warm_produced_for;
  LUMICE_PathText warm_config_fingerprint;
  LUMICE_PathText budget_note;
} LUMICE_PathSurveyScope;

typedef struct LUMICE_PathFaceSequence {
  size_t struct_size;
  const int* faces;
  size_t face_count;
} LUMICE_PathFaceSequence;

typedef struct LUMICE_PathSurveyMemberVerdict {
  size_t struct_size;
  const int* member;
  size_t face_count;
  int rank_zero;
  LUMICE_PathText mechanism;
  LUMICE_PathText mechanism_unavailable;
  int evidence_state;  // LUMICE_PATH_STATE_*
} LUMICE_PathSurveyMemberVerdict;

typedef struct LUMICE_PathSurveyUnevaluated {
  size_t struct_size;
  LUMICE_PathText reason;
  size_t kind_count;
} LUMICE_PathSurveyUnevaluated;

typedef struct LUMICE_PathFamilyDimension {
  size_t struct_size;
  LUMICE_PathText kind;
  double center_deg;
  double width_deg;
} LUMICE_PathFamilyDimension;

typedef struct LUMICE_PathFamilyFrozenDim {
  size_t struct_size;
  LUMICE_PathText dim;
  double center_deg;
  double width_deg;
} LUMICE_PathFamilyFrozenDim;

typedef struct LUMICE_PathFamilyKilled {
  size_t struct_size;
  const int* member;
  size_t member_face_count;
  int kind;  // LUMICE_PATH_SURVEY_* family-image kind
  int position_available;
  double position_altitude_deg;
  double position_azimuth_deg;
  int blur_measured;
  double blur_deg;
  double scale_deg;
  int clamped;
  LUMICE_PathText reason;
  size_t verdict_count;
} LUMICE_PathFamilyKilled;

typedef struct LUMICE_PathSurveyInterval {
  size_t struct_size;
  double lower_deg;
  double upper_deg;
  int n_components;
  int n_closed;
  int n_open;
} LUMICE_PathSurveyInterval;

typedef struct LUMICE_PathBandVerdict {
  size_t struct_size;
  LUMICE_PathText dimension;
  LUMICE_PathText verdict;
} LUMICE_PathBandVerdict;

typedef struct LUMICE_PathFamilyOrigin {
  size_t struct_size;
  int sample_count;
  int crossing_count;
  size_t frozen_verdict_count;
  int band_clamped;
} LUMICE_PathFamilyOrigin;

typedef struct LUMICE_PathSurveySupport {
  size_t struct_size;
  unsigned symmetry_bits;
  int member_count;
  size_t interval_count;
  size_t endpoint_count;
  size_t density_tail_count;
} LUMICE_PathSurveySupport;

typedef struct LUMICE_PathSurveyEndpoint {
  size_t struct_size;
  double delta_deg;
  LUMICE_PathText source;
  LUMICE_PathText profile;
  int multiplicity;
} LUMICE_PathSurveyEndpoint;

typedef struct LUMICE_PathSurveyTail {
  size_t struct_size;
  LUMICE_PathText dimension;
  double lower_deg;
  double upper_deg;
} LUMICE_PathSurveyTail;

typedef struct LUMICE_PathSurveyEdge {
  size_t struct_size;
  double position_deg;
  LUMICE_PathText profile;
  int measure_limit_available;
  double measure_limit;
  int coincident_with_support;
  int certificate_available;
  LUMICE_PathText formula;
  double apex_angle_deg;
  double cross_check_deg;
  double tolerance_deg;
} LUMICE_PathSurveyEdge;

typedef struct LUMICE_PathSurveyBright {
  size_t struct_size;
  double domain_lower_deg;
  double domain_upper_deg;
  int peak_available;
  double peak_delta_deg;
  double peak_value;
  int edge_reading_available;
  double edge_delta_deg;
  int64_t kept_count;
  int probe_index_available;
  size_t probe_index;
} LUMICE_PathSurveyBright;

typedef struct LUMICE_PathSurveySourceCertificate {
  size_t struct_size;
  double fold_tolerance;
  double wedge_tolerance_deg;
} LUMICE_PathSurveySourceCertificate;

LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetObjectMember(const LUMICE_PathSurvey* result, size_t object,
                                                                  size_t index, LUMICE_PathFaceSequence* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetObjectBandVerdict(const LUMICE_PathSurvey* result, size_t object,
                                                                       size_t index, LUMICE_PathBandVerdict* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetFamilyOrigin(const LUMICE_PathSurvey* result, size_t object,
                                                                  LUMICE_PathFamilyOrigin* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetSupport(const LUMICE_PathSurvey* result, size_t object,
                                                             LUMICE_PathSurveySupport* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetSupportEndpoint(const LUMICE_PathSurvey* result, size_t object,
                                                                     size_t index, LUMICE_PathSurveyEndpoint* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetSupportTail(const LUMICE_PathSurvey* result, size_t object,
                                                                 size_t index, LUMICE_PathSurveyTail* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetEdge(const LUMICE_PathSurvey* result, size_t object,
                                                          LUMICE_PathSurveyEdge* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetBright(const LUMICE_PathSurvey* result, size_t object,
                                                            LUMICE_PathSurveyBright* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetSourceCertificate(const LUMICE_PathSurvey* result, size_t object,
                                                                       LUMICE_PathSurveySourceCertificate* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetFamilyKilledVerdict(const LUMICE_PathSurvey* result, size_t level,
                                                                         size_t killed, size_t index,
                                                                         LUMICE_PathBandVerdict* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetFamilyFreeDim(const LUMICE_PathSurvey* result, size_t level,
                                                                   size_t index, LUMICE_PathText* out);

LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetScope(const LUMICE_PathSurvey* result,
                                                           LUMICE_PathSurveyScope* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetClaimMember(const LUMICE_PathSurvey* result, int kind,
                                                                 size_t owner, size_t member,
                                                                 LUMICE_PathFaceSequence* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetRequestLayer(const LUMICE_PathSurvey* result, size_t index,
                                                                  LUMICE_PathFaceSequence* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetMember(const LUMICE_PathSurvey* result, size_t index,
                                                            LUMICE_PathFaceSequence* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetMemberVerdict(const LUMICE_PathSurvey* result, size_t index,
                                                                   LUMICE_PathSurveyMemberVerdict* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetUnevaluated(const LUMICE_PathSurvey* result, size_t index,
                                                                 LUMICE_PathSurveyUnevaluated* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetUnevaluatedKind(const LUMICE_PathSurvey* result, size_t object,
                                                                     size_t index, int* out_kind);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetFamilyDimension(const LUMICE_PathSurvey* result, size_t index,
                                                                     LUMICE_PathFamilyDimension* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetFamilyFrozenDim(const LUMICE_PathSurvey* result, size_t level,
                                                                     size_t index, LUMICE_PathFamilyFrozenDim* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetFamilyKilled(const LUMICE_PathSurvey* result, size_t level,
                                                                  size_t index, LUMICE_PathFamilyKilled* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetSupportInterval(const LUMICE_PathSurvey* result, size_t object,
                                                                     size_t index, LUMICE_PathSurveyInterval* out);

typedef struct LUMICE_PathReportScope {
  size_t struct_size;
  LUMICE_PathText scene_identity;
  LUMICE_PathText spectrum_scope;
  uint32_t seed;
  int member_kind;            // LUMICE_PATH_MEMBER_*
  int location;               // LUMICE_PATH_LOCATION_*
  size_t layer_index;         // only when location == LUMICE_PATH_LOCATION_SCENE_LAYER
  int raw_spectrum_kind;      // LUMICE_PATH_RAW_SPECTRUM_*
  int raw_illuminant;         // LUMICE_PATH_ILLUMINANT_* when raw_spectrum_kind is ILLUMINANT
  size_t raw_spectrum_count;  // when raw_spectrum_kind is DISCRETE
  double sun_altitude_deg;
  double sun_azimuth_deg;
  double sun_diameter_deg;
  int pose_coordinate_count;
  int pose_support_dimension;
  int shape_parameter_dimension;
  int source_direction_dimension;
  int spectral_dimension;
  size_t layer_count;
  size_t member_count;
  size_t spectrum_row_count;
} LUMICE_PathReportScope;

typedef struct LUMICE_PathReportLayer {
  size_t struct_size;
  int scene_layer_available;
  size_t scene_layer;
  int64_t crystal_id;
  int crystal_kind;  // LUMICE_PATH_CRYSTAL_*
  unsigned symmetry_bits;
  const int* representative_faces;
  size_t representative_face_count;
  size_t shape_scalar_count;  // ten fixed slots; inapplicable slots return NOT_AVAILABLE
  int wedge_available;
  double upper_wedge_deg;
  double lower_wedge_deg;
} LUMICE_PathReportLayer;

typedef struct LUMICE_PathReportAxis {
  size_t struct_size;
  int distribution;  // LUMICE_PATH_DISTRIBUTION_*
  double center;
  double spread;
} LUMICE_PathReportAxis;

typedef struct LUMICE_PathReportShapeScalar {
  size_t struct_size;
  LUMICE_PathText name;
  int distribution;  // LUMICE_PATH_DISTRIBUTION_*
  double center;
  double spread;
  int sync_group;
} LUMICE_PathReportShapeScalar;

typedef struct LUMICE_PathRawSpectrumRow {
  size_t struct_size;
  double wavelength_nm;
  double weight;
} LUMICE_PathRawSpectrumRow;

LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetLayer(const LUMICE_PathFeatureReport* result, size_t index,
                                                           LUMICE_PathReportLayer* out);
// Axis index: 0 azimuth, 1 latitude, 2 roll (the as-configured distribution coordinates).
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetLayerAxis(const LUMICE_PathFeatureReport* result, size_t layer,
                                                               size_t axis, LUMICE_PathReportAxis* out);
// Scalar slot 0..9 follows the engine's stable shape-slot numbering.
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetLayerShape(const LUMICE_PathFeatureReport* result, size_t layer,
                                                                size_t slot, LUMICE_PathReportShapeScalar* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetRawSpectrumRow(const LUMICE_PathFeatureReport* result,
                                                                    size_t index, LUMICE_PathRawSpectrumRow* out);

typedef struct LUMICE_PathSpectralRow {
  size_t struct_size;
  double wavelength_nm;
  double source_weight;
  double measure_mass;
  double refractive_index;
  double xyz_basis[3];
  double coefficient[3];
  int origin;  // LUMICE_PATH_SPECTRUM_ORIGIN_*
  LUMICE_PathText provenance;
} LUMICE_PathSpectralRow;

typedef struct LUMICE_PathReportSupportMember {
  size_t struct_size;
  const int* member;
  size_t face_count;
  LUMICE_PathText regime_slug;
  LUMICE_PathText message;
  int coverage;  // LUMICE_PATH_SUPPORT_*
  int escape_regime_available;
  int escape_regime;  // LUMICE_PATH_ESCAPE_* when available
  int walk_status;    // LUMICE_PATH_WALK_*
  int walk_closed;
  size_t interval_count;
  size_t onset_count;
  size_t constant_curve_count;
} LUMICE_PathReportSupportMember;

typedef struct LUMICE_PathReportInterval {
  size_t struct_size;
  double lower_rad;
  double upper_rad;
  int n_components;
  int n_closed;
  int n_open;
} LUMICE_PathReportInterval;

typedef struct LUMICE_PathReportBudgets {
  size_t struct_size;
  uint64_t requested_outer_samples;
  uint64_t completed_outer_samples;
  uint64_t replicate_samples;
  uint32_t replicate_seed;
  uint64_t max_optical_evaluations;
  uint64_t optical_evaluations;
  uint64_t max_field_component_evaluations;
  uint64_t field_component_evaluations;
  int exhausted;
  uint64_t attribution_dp_evaluations;
  uint64_t attribution_presence_ess_queries;
  uint64_t enumeration_max_optical_evaluations;
  int64_t enumeration_sampling_evaluations;
  int64_t enumeration_hang_cap_ms;
  int enumeration_truncated;
  LUMICE_PathText enumeration_truncation_note;
} LUMICE_PathReportBudgets;

typedef struct LUMICE_PathReportAnnotation {
  size_t struct_size;
  int state;  // LUMICE_PATH_CORROBORATION_*
  int presence_ess_available;
  double presence_ess;
  int match_distance_available;
  double match_distance;
  int tolerance_available;
  double tolerance_rad;
  int matched_record_available;
  size_t matched_record;
  int matched_record_ess_available;
  double matched_record_ess;
  LUMICE_PathText ruler;
  LUMICE_PathText reason;
} LUMICE_PathReportAnnotation;

typedef struct LUMICE_PathUnattributed {
  size_t struct_size;
  size_t record_index;
  double position[3];
  double delta_rad;
  double record_ess;
  int min_margin_available;
  double min_margin_rad;
  LUMICE_PathText ruler;
} LUMICE_PathUnattributed;

typedef struct LUMICE_PathUnattributedSummary {
  size_t struct_size;
  size_t structure_count;
  int64_t skipped_no_position;
  int64_t skipped_below_ess;
  int64_t attribution_dp_evaluations;
  int64_t attribution_presence_ess_queries;
  LUMICE_PathText error;
} LUMICE_PathUnattributedSummary;

LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetUnattributedSummary(const LUMICE_PathFeatureReport* result,
                                                                         LUMICE_PathUnattributedSummary* out);

typedef struct LUMICE_PathReportSource {
  size_t struct_size;
  uint64_t sample_index;
  size_t member_index;
  size_t spectral_row;
} LUMICE_PathReportSource;

typedef struct LUMICE_PathReportSkyPoint {
  size_t struct_size;
  double direction[3];
} LUMICE_PathReportSkyPoint;

typedef struct LUMICE_PathReportSourceParameter {
  size_t struct_size;
  LUMICE_PathText name;
  double value;
} LUMICE_PathReportSourceParameter;

typedef struct LUMICE_PathReportMcEvidence {
  size_t struct_size;
  uint64_t event_path_evaluations;
  uint64_t replicate_path_evaluations;
  uint64_t replicate_samples;
  uint64_t field_component_evaluations;
  int budget_exhausted;
  uint64_t completed_samples;
  uint64_t optical_evaluations;
  size_t component_count;
  size_t positive_source_count;
  size_t unfinished_count;
  size_t limitation_count;
  double bandwidth_rad;
  double location_resolution_rad;
  LUMICE_PathText observation_scope_note;
  int spectral_available;
  int spectral_movement_available;
  double spectral_movement_rad;
  uint64_t spectral_optical_evaluations;
  uint64_t spectral_field_evaluations;
  double spectral_seconds;
} LUMICE_PathReportMcEvidence;

typedef struct LUMICE_PathMcComponent {
  size_t struct_size;
  uint64_t sample_index;
  double direction[3];
  double xyz_weight[3];
  int orbit_axis_available;
  double orbit_axis[3];
  uint64_t source_token;
} LUMICE_PathMcComponent;

typedef struct LUMICE_PathPositiveSource {
  size_t struct_size;
  uint64_t token;
  uint64_t sample_index;
  size_t member_index;
  size_t spectral_row;
  int coordinates_available;
  int crystal_kind;  // LUMICE_PATH_CRYSTAL_*
  double height;
  double face_distance[6];
  double upper_h;
  double lower_h;
  double upper_wedge_deg;
  double lower_wedge_deg;
  double pose[9];
  double incident[3];
  double refractive_index;
} LUMICE_PathPositiveSource;

typedef struct LUMICE_PathRecordAdjuncts {
  size_t struct_size;
  int band_available;
  size_t band_lower_count;
  size_t band_upper_count;
  int boundary_source_available;
  int boundary_value_available;
  int interface_event_available;
  int paired_interface_available;
  int source_event_available;
  int deviation_minimum_available;
  int orbit_axis_available;
  double orbit_axis[3];
  double orbit_begin_rad;
  double orbit_end_rad;
  double observation_contrast_error;
  double atom_xyz_mass[3];
  double transverse_contrast;
  double prefix_movement_rad;
  int scale_status;  // LUMICE_PATH_FIELD_SOLVE_*
  int walk_stop;     // LUMICE_PATH_FIELD_WALK_*
  int internal_slot;
} LUMICE_PathRecordAdjuncts;

typedef struct LUMICE_PathFieldPoint {
  size_t struct_size;
  int status;  // LUMICE_PATH_FIELD_SOLVE_*
  double direction[3];
  double basis[6];
  double bandwidth_rad;
  double effective_samples_y;
  uint64_t outer_sample_count;
  int chromaticity_available;
  double normal[2];
  double log_y_curvatures[2];
  double correction_rad;
  double travelled_rad;
  int iterations;
} LUMICE_PathFieldPoint;

typedef struct LUMICE_PathFieldJet {
  size_t struct_size;
  double value;
  double gradient[2];
  double hessian[3];
} LUMICE_PathFieldJet;

typedef struct LUMICE_PathFieldBand {
  size_t struct_size;
  int status;  // LUMICE_PATH_FIELD_SOLVE_*
  double levels[2];
  size_t boundary_count[2];
} LUMICE_PathFieldBand;

typedef struct LUMICE_PathInterfacePoint {
  size_t struct_size;
  int status;  // LUMICE_PATH_INTERFACE_SOLVE_*
  double travelled_rad;
  uint64_t path_evaluations;
  size_t accepted_pose_count;
} LUMICE_PathInterfacePoint;

typedef struct LUMICE_PathInterfaceCurve {
  size_t struct_size;
  int stop;  // LUMICE_PATH_INTERFACE_WALK_*
  uint64_t path_evaluations;
  size_t point_count;
  size_t event_count;
} LUMICE_PathInterfaceCurve;

typedef struct LUMICE_PathEventBracket {
  size_t struct_size;
  int kind;  // LUMICE_PATH_INTERFACE_WALK_*
  double source_width_rad;
  int positive_status;     // LUMICE_PATH_INTERFACE_SOLVE_*
  int nonpositive_status;  // LUMICE_PATH_INTERFACE_SOLVE_*
} LUMICE_PathEventBracket;

typedef struct LUMICE_PathDeviationMinimum {
  size_t struct_size;
  int status;  // LUMICE_PATH_INTERFACE_SOLVE_*
  int deviation_available;
  double deviation_rad;
  double objective_curvatures[2];
  double hessian_error;
  double correction_rad;
  uint64_t path_evaluations;
} LUMICE_PathDeviationMinimum;

typedef struct LUMICE_PathPairedInterface {
  size_t struct_size;
  int chromaticity_available;
  int complete;
  double actual_xyz[3];
  double without_slot_xyz[3];
  double xy_difference[2];
} LUMICE_PathPairedInterface;

typedef struct LUMICE_PathCorridor {
  size_t struct_size;
  int geometry_evaluated;
  double raw_area;
  double area_threshold;
  double projection_basis[6];
  size_t vertex_count;
  size_t edge_source_count;
} LUMICE_PathCorridor;

typedef struct LUMICE_PathCorridorVertex {
  size_t struct_size;
  double xy[2];
} LUMICE_PathCorridorVertex;

typedef struct LUMICE_PathCorridorEdge {
  size_t struct_size;
  int path_index;
  int edge_index;
} LUMICE_PathCorridorEdge;

typedef struct LUMICE_PathAcceptedPose {
  size_t struct_size;
  double pose[9];
} LUMICE_PathAcceptedPose;

// These fixed selectors address only the shared DiagnosticInputRow/OutputRow value, not arbitrary
// record properties. curve selects an interface curve; point selects its point or event.
#define LUMICE_PATH_DIAGNOSTIC_BOUNDARY 1
#define LUMICE_PATH_DIAGNOSTIC_INTERFACE_EVENT 2
#define LUMICE_PATH_DIAGNOSTIC_CURVE_POINT 3
#define LUMICE_PATH_DIAGNOSTIC_CURVE_EVENT_POSITIVE 4
#define LUMICE_PATH_DIAGNOSTIC_CURVE_EVENT_NONPOSITIVE 5
#define LUMICE_PATH_DIAGNOSTIC_SOURCE_EVENT_POSITIVE 6
#define LUMICE_PATH_DIAGNOSTIC_SOURCE_EVENT_NONPOSITIVE 7
#define LUMICE_PATH_DIAGNOSTIC_DEVIATION_MINIMUM 8

LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetFieldBand(const LUMICE_PathFeatureReport* result, size_t record,
                                                               LUMICE_PathFieldBand* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetBandPoint(const LUMICE_PathFeatureReport* result, size_t record,
                                                               size_t boundary, size_t index,
                                                               LUMICE_PathFieldPoint* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetBandJet(const LUMICE_PathFeatureReport* result, size_t record,
                                                             size_t boundary, size_t point, int channel,
                                                             LUMICE_PathFieldJet* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetInterfaceEvent(const LUMICE_PathFeatureReport* result,
                                                                    size_t record, LUMICE_PathInterfacePoint* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetInterfaceCurve(const LUMICE_PathFeatureReport* result,
                                                                    size_t record, size_t curve,
                                                                    LUMICE_PathInterfaceCurve* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetCurvePoint(const LUMICE_PathFeatureReport* result, size_t record,
                                                                size_t curve, size_t point,
                                                                LUMICE_PathInterfacePoint* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetCurveEvent(const LUMICE_PathFeatureReport* result, size_t record,
                                                                size_t curve, size_t event,
                                                                LUMICE_PathEventBracket* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetSourceEvent(const LUMICE_PathFeatureReport* result, size_t record,
                                                                 LUMICE_PathEventBracket* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetDeviationMinimum(const LUMICE_PathFeatureReport* result,
                                                                      size_t record, LUMICE_PathDeviationMinimum* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetPairedInterface(const LUMICE_PathFeatureReport* result,
                                                                     size_t record, LUMICE_PathPairedInterface* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetAcceptedPose(const LUMICE_PathFeatureReport* result, size_t record,
                                                                  int site, size_t curve, size_t point, size_t index,
                                                                  LUMICE_PathAcceptedPose* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetCorridor(const LUMICE_PathFeatureReport* result, size_t record,
                                                              int site, size_t curve, size_t point,
                                                              LUMICE_PathCorridor* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetCorridorVertex(const LUMICE_PathFeatureReport* result,
                                                                    size_t record, int site, size_t curve, size_t point,
                                                                    size_t index, LUMICE_PathCorridorVertex* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetCorridorEdge(const LUMICE_PathFeatureReport* result, size_t record,
                                                                  int site, size_t curve, size_t point, size_t index,
                                                                  LUMICE_PathCorridorEdge* out);

typedef struct LUMICE_PathDiagnosticSource {
  size_t struct_size;
  int crystal_kind;  // LUMICE_PATH_CRYSTAL_*
  double height;
  double face_distance[6];
  double upper_h;
  double lower_h;
  double upper_wedge_deg;
  double lower_wedge_deg;
  double pose[9];
  double incident[3];
  double refractive_index;
  uint64_t source_token;
} LUMICE_PathDiagnosticSource;

typedef struct LUMICE_PathDiagnosticValue {
  size_t struct_size;
  uint64_t source_token;
  int input_status;  // LUMICE_PATH_INPUT_*
  uint64_t path_evaluations;
  int optical_failure;  // LUMICE_PATH_CHAIN_*
  int path_valid;
  double outgoing[3];
  int direction_jacobian_available;
  double direction_pose_jacobian[9];
  int direction_index_available;
  double direction_index_derivative[3];
  double direction_index_error;
  double interface_product;
  int entry_available;
  int entry_status;  // LUMICE_PATH_ENTRY_* when entry_available
  double entry_measure;
  double entry_area_perp_internal;
  size_t interface_count;
  size_t corridor_vertex_count;
  size_t corridor_edge_count;
} LUMICE_PathDiagnosticValue;

typedef struct LUMICE_PathDiagnosticInterface {
  size_t struct_size;
  int reached;
  double incidence;
  double discriminant;
  int pose_gradient_available;
  double discriminant_pose_gradient[3];
  int index_derivative_available;
  double discriminant_index_derivative;
  double index_derivative_error;
  int factor_available;
  double factor;
} LUMICE_PathDiagnosticInterface;

LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetDiagnosticSource(const LUMICE_PathFeatureReport* result,
                                                                      size_t record, int site, size_t curve,
                                                                      size_t point, LUMICE_PathDiagnosticSource* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetDiagnosticValue(const LUMICE_PathFeatureReport* result,
                                                                     size_t record, int site, size_t curve,
                                                                     size_t point, LUMICE_PathDiagnosticValue* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetDiagnosticInterface(const LUMICE_PathFeatureReport* result,
                                                                         size_t record, int site, size_t curve,
                                                                         size_t point, size_t index,
                                                                         LUMICE_PathDiagnosticInterface* out);

LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetRecordAdjuncts(const LUMICE_PathFeatureReport* result,
                                                                    size_t record, LUMICE_PathRecordAdjuncts* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetFieldPoint(const LUMICE_PathFeatureReport* result, size_t record,
                                                                size_t index, LUMICE_PathFieldPoint* out);
// channel 0..2 = XYZ, 3..4 = chromaticity xy; unavailable xy requires field chromaticity_available.
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetFieldJet(const LUMICE_PathFeatureReport* result, size_t record,
                                                              size_t point, int channel, LUMICE_PathFieldJet* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetBoundarySource(const LUMICE_PathFeatureReport* result,
                                                                    size_t record, LUMICE_PathDiagnosticSource* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetBoundaryValue(const LUMICE_PathFeatureReport* result,
                                                                   size_t record, LUMICE_PathDiagnosticValue* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetBoundaryInterface(const LUMICE_PathFeatureReport* result,
                                                                       size_t record, size_t index,
                                                                       LUMICE_PathDiagnosticInterface* out);

LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetMcUnfinished(const LUMICE_PathFeatureReport* result, size_t index,
                                                                  LUMICE_PathText* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetMcLimitation(const LUMICE_PathFeatureReport* result, size_t index,
                                                                  LUMICE_PathText* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetMcEvidence(const LUMICE_PathFeatureReport* result,
                                                                LUMICE_PathReportMcEvidence* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetMcComponent(const LUMICE_PathFeatureReport* result, size_t index,
                                                                 LUMICE_PathMcComponent* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetPositiveSource(const LUMICE_PathFeatureReport* result,
                                                                    size_t index, LUMICE_PathPositiveSource* out);

// The requested layers remain readable when an unsupported multi-crystal report has no scope.
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetRequestedLayer(const LUMICE_PathFeatureReport* result,
                                                                    size_t index, LUMICE_PathFaceSequence* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetScope(const LUMICE_PathFeatureReport* result,
                                                           LUMICE_PathReportScope* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetMember(const LUMICE_PathFeatureReport* result, size_t index,
                                                            LUMICE_PathFaceSequence* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetSpectralRow(const LUMICE_PathFeatureReport* result, size_t index,
                                                                 LUMICE_PathSpectralRow* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetSupportMember(const LUMICE_PathFeatureReport* result, size_t index,
                                                                   LUMICE_PathReportSupportMember* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetSupportInterval(const LUMICE_PathFeatureReport* result,
                                                                     size_t member, size_t index,
                                                                     LUMICE_PathReportInterval* out);
typedef struct LUMICE_PathReportOrbit {
  size_t struct_size;
  int binding;   // LUMICE_PATH_ORBIT_SOLID_ANGLE / FIBER_PARAMETER
  int evidence;  // LUMICE_PATH_ORBIT_STRUCTURAL / SAMPLED_EXHAUSTIVE / SAMPLED_PARTIAL
  int grid;
  int spin_degenerate;
  LUMICE_PathText note;
  LUMICE_PathText source_name;
  size_t sample_count;
} LUMICE_PathReportOrbit;

typedef struct LUMICE_PathReportOrbitSample {
  size_t struct_size;
  double u[3];
  double outgoing[3];
  double area;
  double transmission;
  int valid;
  int jet_degenerate;
  double parameter;
  double weight;
} LUMICE_PathReportOrbitSample;

LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetOrbit(const LUMICE_PathFeatureReport* result, size_t index,
                                                           LUMICE_PathReportOrbit* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetOrbitSample(const LUMICE_PathFeatureReport* result, size_t orbit,
                                                                 size_t index, LUMICE_PathReportOrbitSample* out);

typedef struct LUMICE_PathReportOnset {
  size_t struct_size;
  double value;
  int location;  // LUMICE_PATH_ONSET_INTERIOR / BOUNDARY
  int source;    // LUMICE_PATH_ONSET_* source vocabulary
  int profile;   // LUMICE_PATH_ONSET_PROFILE_*
  double gradient_norm;
  int measure_limit_available;
  double measure_limit;
  int multiplicity;
} LUMICE_PathReportOnset;

typedef struct LUMICE_PathReportCurve {
  size_t struct_size;
  double d_p;
  int weight_step;
  size_t wavelength_count;
} LUMICE_PathReportCurve;

typedef struct LUMICE_PathReportCurveWavelength {
  size_t struct_size;
  double wavelength_nm;
  int critical_available;
  double critical_d_p;
} LUMICE_PathReportCurveWavelength;

typedef struct LUMICE_PathReportSupportFamily {
  size_t struct_size;
  int shared;
  LUMICE_PathText phi_class_note;
  size_t interval_count;
  size_t member_count;
} LUMICE_PathReportSupportFamily;

LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetSupportOnset(const LUMICE_PathFeatureReport* result, size_t member,
                                                                  size_t index, LUMICE_PathReportOnset* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetConstantCurve(const LUMICE_PathFeatureReport* result,
                                                                   size_t member, size_t index,
                                                                   LUMICE_PathReportCurve* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetCurveWavelength(const LUMICE_PathFeatureReport* result,
                                                                     size_t member, size_t curve, size_t index,
                                                                     LUMICE_PathReportCurveWavelength* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetSupportFamily(const LUMICE_PathFeatureReport* result, size_t index,
                                                                   LUMICE_PathReportSupportFamily* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetFamilyInterval(const LUMICE_PathFeatureReport* result,
                                                                    size_t family, size_t index,
                                                                    LUMICE_PathReportInterval* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetFamilyMember(const LUMICE_PathFeatureReport* result, size_t family,
                                                                  size_t index, LUMICE_PathFaceSequence* out);

LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetBudgets(const LUMICE_PathFeatureReport* result,
                                                             LUMICE_PathReportBudgets* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetAnnotation(const LUMICE_PathFeatureReport* result, size_t object,
                                                                LUMICE_PathReportAnnotation* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetUnattributed(const LUMICE_PathFeatureReport* result, size_t index,
                                                                  LUMICE_PathUnattributed* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetSource(const LUMICE_PathFeatureReport* result, size_t index,
                                                            LUMICE_PathReportSource* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetSkyPoint(const LUMICE_PathFeatureReport* result, size_t record,
                                                              size_t index, LUMICE_PathReportSkyPoint* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetSourceParameter(const LUMICE_PathFeatureReport* result,
                                                                     size_t record, size_t index,
                                                                     LUMICE_PathReportSourceParameter* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetCoverageLimitation(const LUMICE_PathFeatureReport* result,
                                                                        size_t coverage, size_t index,
                                                                        LUMICE_PathText* out);

LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetChromatic(const LUMICE_PathSurvey* result,
                                                               LUMICE_PathChromatic* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetDispersion(const LUMICE_PathSurvey* result,
                                                                LUMICE_PathDispersion* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetFollowedOnset(const LUMICE_PathSurvey* result, size_t index,
                                                                   LUMICE_PathFollowedOnset* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetDiagnose(const LUMICE_PathSurvey* result,
                                                              LUMICE_PathDiagnose* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetDiagnoseNote(const LUMICE_PathSurvey* result, size_t index,
                                                                  LUMICE_PathText* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetDiagnoseFeature(const LUMICE_PathSurvey* result, size_t index,
                                                                     LUMICE_PathDiagnoseFeature* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetProbe(const LUMICE_PathSurvey* result, size_t index,
                                                           LUMICE_PathSurveyProbe* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetProbeSample(const LUMICE_PathSurvey* result, size_t probe,
                                                                 size_t index, LUMICE_PathProbeSample* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetWitness(const LUMICE_PathSurvey* result, size_t object,
                                                             LUMICE_PathWitness* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetRuling(const LUMICE_PathFeatureReport* result,
                                                            LUMICE_PathReportRuling* out);

LUMICE_API LUMICE_PathReadStatus LUMICE_PathPointGetInfo(const LUMICE_SinglePathResult* result,
                                                         LUMICE_PathPointInfo* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathPointGetComponent(const LUMICE_SinglePathResult* result, size_t index,
                                                              LUMICE_PathPointComponent* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathPointGetSample(const LUMICE_SinglePathResult* result, size_t component,
                                                           size_t index, LUMICE_PathPointSample* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathPointGetSunGrid(const LUMICE_SinglePathResult* result,
                                                            LUMICE_PathSunGrid* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathProbeGetInfo(const LUMICE_PathProbe* result, LUMICE_PathProbeInfo* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathProbeGetProfile(const LUMICE_PathProbe* result, size_t index,
                                                            LUMICE_PathProbeProfile* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathProbeGetSample(const LUMICE_PathProbe* result, size_t profile, size_t index,
                                                           LUMICE_PathProbeSample* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetInfo(const LUMICE_PathSurvey* result, LUMICE_PathSurveyInfo* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetObject(const LUMICE_PathSurvey* result, size_t index,
                                                            LUMICE_PathSurveyObject* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetFamily(const LUMICE_PathSurvey* result,
                                                            LUMICE_PathFamilyInfo* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetFamilyLevel(const LUMICE_PathSurvey* result, size_t index,
                                                                 LUMICE_PathFamilyLevel* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetTint(const LUMICE_PathSurvey* result, LUMICE_PathTint* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetTirShare(const LUMICE_PathSurvey* result,
                                                              LUMICE_PathTirShare* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetTirInterface(const LUMICE_PathSurvey* result, size_t index,
                                                                  LUMICE_PathTirInterface* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetMcEss(const LUMICE_PathSurvey* result, LUMICE_PathMcEss* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetMcEssBand(const LUMICE_PathSurvey* result, size_t index,
                                                               LUMICE_PathMcEssBand* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathSurveyGetWarmSeed(const LUMICE_PathSurvey* result, size_t index,
                                                              LUMICE_PathWarmSeed* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetInfo(const LUMICE_PathFeatureReport* result,
                                                          LUMICE_PathReportInfo* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetObject(const LUMICE_PathFeatureReport* result, size_t index,
                                                            LUMICE_PathReportObject* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetRecord(const LUMICE_PathFeatureReport* result, size_t index,
                                                            LUMICE_PathReportRecord* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetCoverage(const LUMICE_PathFeatureReport* result, size_t index,
                                                              LUMICE_PathReportCoverage* out);
LUMICE_API LUMICE_PathReadStatus LUMICE_PathReportGetDeclaredSkip(const LUMICE_PathFeatureReport* result, size_t index,
                                                                  LUMICE_PathReportDeclaredSkip* out);

#ifdef __cplusplus
}
#endif

#endif  // LUMICE_RAYPATH_H_
