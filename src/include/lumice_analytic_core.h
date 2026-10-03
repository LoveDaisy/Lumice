#ifndef LUMICE_ANALYTIC_CORE_H_
#define LUMICE_ANALYTIC_CORE_H_

// The analytic capability of Lumice: its types and computation functions. Design:
// doc/analytic-api.md. A consumer of the published library includes lumice_analytic.h, which
// includes this header and adds the two library-management functions (version, log callback).
//
// The split is the packaging boundary. The functions declared HERE are exported by every library
// that hosts the capability — liblumice_analytic, and the engine libraries liblumice and
// liblumice_testapi — while the two in lumice_analytic.h are exported by liblumice_analytic alone,
// because they manage that library's own logging and version. Which headers each library exports
// is declared once, in cmake/export_surfaces.cmake; the export lists are generated from them by
// scripts/gen_export_list.py (root CMakeLists.txt, lumice_apply_export_list).
//
// Independent of the engine's lumice_*.h headers: it includes nothing from them and shares none of their types. A
// process loads one of liblumice / liblumice_testapi / liblumice_analytic, never two: each carries its own copy of the
// engine and its statics (doc/analytic-api.md section 2.6).
//
// 0.x is experimental: the interface may change between versions (doc/analytic-api.md section 8).
//
// Conventions: frames, face numbers and the pose chain are those of doc/coordinate-convention.md;
// this header passes them, it does not define them. Symmetry: every function takes one concrete
// face sequence and performs no symmetry reduction (doc/analytic-api.md section 3).
//
// Version notes, newest first (every bump says what changed, doc/analytic-api.md section 8.1):
//   10 ADDED feature-support version 2: dynamic coordinate extents, an append-only
//      accumulates_measure flag, and explicit local cell-axis topology. Version 1 keeps its
//      published 16-coordinate limit and layouts. Nothing else changed.
//   9  ADDED the general support-driven feature-discovery structs, re-evaluation callback,
//      LUMICE_ANALYTIC_DiscoverFeatures and LUMICE_ANALYTIC_ReleaseFeatureDiscoveryResult.
//      Nothing existing changed.
//   8  ADDED one-face external-reflection semantics to EvaluateDiagnosticFieldBatch and appended
//      LUMICE_ANALYTIC_DIAGNOSTIC_EXTERNAL_REFLECTION to DiagnosticInterfaceKind. The frozen
//      version 7 DiagnosticFieldResult layout and every older computation contract are unchanged.
//   7  ADDED LUMICE_ANALYTIC_DiagnosticFieldRow, LUMICE_ANALYTIC_DiagnosticFieldResult and their
//      variable interface/margin records, LUMICE_ANALYTIC_EvaluateDiagnosticFieldBatch, and
//      LUMICE_ANALYTIC_ReleaseDiagnosticFieldResult. Nothing existing changed.
//   6  ADDED LUMICE_ANALYTIC_BandSumProblem, LUMICE_ANALYTIC_BandSumResult,
//      LUMICE_ANALYTIC_BandSum, and LUMICE_ANALYTIC_ReleaseBandSumResult. Nothing existing changed.
//   5  APPENDED to LUMICE_ANALYTIC_FiberResult, under the struct_size rule: branch_margin_count,
//      branch_margin_names, branch_margins, jacobian_available, normal_jacobian, singular_values —
//      the per-pose diagnostics of LI docs/analytic-parity-fixtures.md section 3.2. A caller
//      compiled against version 4 (struct_size = the version 4 sizeof) is accepted and served
//      exactly as before; version 4 rejected any struct_size below its own sizeof, so no such
//      caller could have passed less. The nested results of DiscoverComponents carry the fields.
//   4  ADDED LUMICE_ANALYTIC_DiscoveryProblem, LUMICE_ANALYTIC_DiscoveryOptions,
//      LUMICE_ANALYTIC_Completeness, LUMICE_ANALYTIC_ComponentKind, LUMICE_ANALYTIC_IncompleteCause,
//      LUMICE_ANALYTIC_DiscoveredComponent, LUMICE_ANALYTIC_IncompleteCandidate,
//      LUMICE_ANALYTIC_DiscoveryResult, LUMICE_ANALYTIC_DiscoverComponents,
//      LUMICE_ANALYTIC_ReleaseDiscoveryResult — component discovery (seed search). Nothing existing
//      changed.
//   3  ADDED LUMICE_ANALYTIC_FiberProblem, LUMICE_ANALYTIC_ContinuationOptions,
//      LUMICE_ANALYTIC_FiberStatus, LUMICE_ANALYTIC_Reason, LUMICE_ANALYTIC_FiberResult,
//      LUMICE_ANALYTIC_TraceFiber, LUMICE_ANALYTIC_TraceFiberBatch, LUMICE_ANALYTIC_ReleaseFiberResult —
//      fiber continuation from a seed. Against the section 4.5 draft, FiberProblem carries
//      initial_tangent_sign. EvaluatePath now also rejects face_count > 64 (ERR_INVALID_VALUE) and
//      returns ERR_UNKNOWN instead of letting an allocation failure escape.
//   2  ADDED LUMICE_ANALYTIC_ErrorCode, LUMICE_ANALYTIC_Crystal, LUMICE_ANALYTIC_PathEvaluation,
//      LUMICE_ANALYTIC_EvaluatePath, LUMICE_ANALYTIC_ReleasePathEvaluation — the first computation.
//   1  LUMICE_ANALYTIC_GetApiVersion and LUMICE_ANALYTIC_SetLogCallback only.

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Symbol visibility. Which functions a shared library exports is decided at link time by that
// library's export list, never by this macro; the macro only has to make a declaration eligible
// for it. On GCC/Clang that means default visibility — the engine objects are compiled with
// -fvisibility=hidden, and a hidden symbol cannot be exported by any list. On Windows the .def
// file does the exporting, so the only thing left for a header to say is the consumer-side
// dllimport, and only when the consumer really links the DLL: LUMICE_ANALYTIC_SHARED_DEFINE is an
// INTERFACE definition of the lumice_analytic target, so a target that compiles against these
// declarations without linking the DLL sees an empty macro instead of an unresolvable __imp_.
#if defined(_WIN32)
#if defined(LUMICE_ANALYTIC_SHARED_DEFINE)
#define LUMICE_ANALYTIC_API __declspec(dllimport)
#else
#define LUMICE_ANALYTIC_API
#endif
#else
#define LUMICE_ANALYTIC_API __attribute__((visibility("default")))
#endif

// Interface version, a single integer (doc/analytic-api.md section 8.2): bumped on every
// incompatible change, and in 0.x on every addition too. Independent of lumice_base.h's LUMICE_API_VERSION.
#define LUMICE_ANALYTIC_API_VERSION 11

// Return codes of the computation functions. The names shared with lumice_base.h's LUMICE_ErrorCode mean
// the same thing there; the type is this header's own (doc/analytic-api.md section 5.2). A numerical
// outcome of a computation (an invalid path at a pose) is result data, never an error code.
typedef enum LUMICE_ANALYTIC_ErrorCode_ {
  LUMICE_ANALYTIC_OK = 0,
  LUMICE_ANALYTIC_ERR_NULL_ARG,
  LUMICE_ANALYTIC_ERR_INVALID_VALUE,   // non-finite / non-unit / out-of-range argument
  LUMICE_ANALYTIC_ERR_INVALID_CONFIG,  // crystal fails the engine's closed-form validity gate
  LUMICE_ANALYTIC_ERR_UNKNOWN,
} LUMICE_ANALYTIC_ErrorCode;

// ---------------------------------------------------------------------------------------------
// Crystal: one deterministic closed-form crystal, the scalars of one sampled Lumice instance
// (doc/analytic-api.md section 4.1). Field semantics are doc/configuration.md's prism / pyramid
// shape fields; heights fold to their absolute value as the simulator folds them. Scale-free.
//
// Every field must be finite and representable as float (the engine's closed-form factory takes
// float). Fields not used by `kind` must be zero: a prism with a non-zero upper_h, lower_h or wedge
// is LUMICE_ANALYTIC_ERR_INVALID_VALUE, so a caller who fills the wrong fields hears about it. A
// crystal the engine's closed-form validity gate rejects (the engine would build an empty crystal
// and log a warning, which reaches the log callback) is LUMICE_ANALYTIC_ERR_INVALID_CONFIG.
// Which faces exist is decided by the engine's closed-form geometry in float, exactly as for a
// simulated crystal; the face normals and the optics are evaluated in double.
// Known limitation: on deliberately constructed degenerate inputs the closed-form pyramid can yield
// an open surface that still passes the gate (doc/analytic-api.md section 4.1). Degradation of a
// crystal (a dropped face, a cone collapsed to its apex) is visible only as an absent face number,
// not as a separate result field.
// ---------------------------------------------------------------------------------------------
typedef enum LUMICE_ANALYTIC_CrystalKind_ {
  LUMICE_ANALYTIC_CRYSTAL_PRISM = 0,
  LUMICE_ANALYTIC_CRYSTAL_PYRAMID = 1,
} LUMICE_ANALYTIC_CrystalKind;

typedef struct LUMICE_ANALYTIC_Crystal_ {
  int kind;                 // LUMICE_ANALYTIC_CrystalKind
  double height;            // prism: height; pyramid: prism-band height (prism_h)
  double face_distance[6];  // ratio of the regular apothem; may be negative
  double upper_h;           // pyramid only
  double lower_h;           // pyramid only
  double upper_wedge_deg;   // pyramid only; final angle in degrees, convert Miller indices first
  double lower_wedge_deg;   // pyramid only
} LUMICE_ANALYTIC_Crystal;

// ---------------------------------------------------------------------------------------------
// Single-pose path evaluation (doc/analytic-api.md section 4.3).
// ---------------------------------------------------------------------------------------------
typedef struct LUMICE_ANALYTIC_PathEvaluation_ {
  uint32_t struct_size;                    // caller sets sizeof(*out) before the call (section 8.2)
  int valid;                               // 1 iff the path is realisable at this pose (see below)
  double outgoing_direction[3];            // world, propagation crystal -> observer
  double fresnel_transmission;             // T_entry * prod(R_k) * T_exit, R_k = 1 under TIR
  int segment_count;                       // face_count + 1 (incident ... outgoing)
  const double* segment_directions;        // segment_count * 3, body-frame propagation directions
  const double* interface_transmittances;  // face_count: T at entry/exit, R at internal faces
  void* storage;                           // opaque; LUMICE_ANALYTIC_ReleasePathEvaluation
} LUMICE_ANALYTIC_PathEvaluation;

// Evaluates one concrete face sequence `faces[0..face_count)` (Lumice face numbers: entry, internal
// reflections, exit) through `crystal` held at `pose` (row-major rotation, body -> world), lit by
// `incident_direction` (world-frame unit propagation direction, sun -> crystal), with the caller's
// `refractive_index`.
//
// `valid` is direction-level: the entry incidence cosine and Snell discriminant, each internal
// face's incidence cosine (the ray reaches it from inside) and the exit incidence cosine and Snell
// discriminant are all positive. A partial internal reflection keeps the path valid and lowers
// `fresnel_transmission`; a total one contributes R = 1. Whether a ray at this pose actually meets
// these faces' finite polygons is not checked. When `valid` is 0 the call still succeeds: the
// directions and transmittances are zero, segment_count is 0 and both pointers are NULL.
// Unpolarised s/p average per interface, the rule of optics.cpp HitSurface.
//
// Call errors (out is zero-filled after struct_size, so Release is safe):
//   ERR_NULL_ARG       crystal, faces, incident_direction, pose or out is NULL
//   ERR_INVALID_VALUE  out->struct_size smaller than this struct; face_count < 2 or > 64 (the
//                      simulator's bound on hits per crystal); a face number the
//                      crystal does not have (unknown, or absent from this crystal's shape);
//                      refractive_index not finite and positive; incident_direction not finite or
//                      its length off 1 by more than 1e-10; pose not finite, R^T R off I by more
//                      than 1e-10 in any entry, or det(R) <= 0; a crystal field as described above
//   ERR_INVALID_CONFIG crystal rejected by the engine's closed-form validity gate
//   ERR_UNKNOWN        an internal failure (out of memory); out is zero-filled
// Every bad input here is a call error, because the call evaluates one pose. TraceFiberBatch draws
// the line differently: an input that belongs to one problem of a batch is that element's result,
// not the call's (see there).
// Re-entrant: safe to call concurrently on distinct outputs; keeps no state between calls.
LUMICE_ANALYTIC_API LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_EvaluatePath(
    const LUMICE_ANALYTIC_Crystal* crystal, const int* faces, int face_count, double refractive_index,
    const double incident_direction[3], const double pose[9], LUMICE_ANALYTIC_PathEvaluation* out);

// Frees what EvaluatePath allocated and zeroes the struct after struct_size. NULL-safe; a no-op on a
// zero-filled struct.
LUMICE_ANALYTIC_API void LUMICE_ANALYTIC_ReleasePathEvaluation(LUMICE_ANALYTIC_PathEvaluation* eval);

// ---------------------------------------------------------------------------------------------
// General diagnostic field over one concrete geometry/path (doc/analytic-api.md section 4.8).
// Geometry and `faces` are shared by a batch; each row supplies its own optical and pose state.
// ---------------------------------------------------------------------------------------------
typedef struct LUMICE_ANALYTIC_DiagnosticFieldRow_ {
  double refractive_index;       // finite, > 0
  double incident_direction[3];  // world unit propagation direction, sun -> crystal
  double pose[9];                // row-major active rotation, body -> world
} LUMICE_ANALYTIC_DiagnosticFieldRow;

typedef enum LUMICE_ANALYTIC_DiagnosticPathStatus_ {
  LUMICE_ANALYTIC_DIAGNOSTIC_PATH_OK = 0,
  LUMICE_ANALYTIC_DIAGNOSTIC_PATH_INFEASIBLE = 1,
  LUMICE_ANALYTIC_DIAGNOSTIC_PATH_REFRACTION_CRITICAL = 2,
  LUMICE_ANALYTIC_DIAGNOSTIC_PATH_NON_FINITE = 3,
} LUMICE_ANALYTIC_DiagnosticPathStatus;

typedef enum LUMICE_ANALYTIC_DiagnosticEntryStatus_ {
  LUMICE_ANALYTIC_DIAGNOSTIC_ENTRY_NOT_EVALUATED = 0,
  LUMICE_ANALYTIC_DIAGNOSTIC_ENTRY_OK = 1,
  LUMICE_ANALYTIC_DIAGNOSTIC_ENTRY_BACKFACE = 2,
  LUMICE_ANALYTIC_DIAGNOSTIC_ENTRY_EXIT_CRITICAL = 3,
  LUMICE_ANALYTIC_DIAGNOSTIC_ENTRY_CORRIDOR_EMPTY = 4,
} LUMICE_ANALYTIC_DiagnosticEntryStatus;

typedef enum LUMICE_ANALYTIC_DiagnosticInterfaceKind_ {
  LUMICE_ANALYTIC_DIAGNOSTIC_ENTRY_TRANSMISSION = 0,
  LUMICE_ANALYTIC_DIAGNOSTIC_INTERNAL_REFLECTION = 1,
  LUMICE_ANALYTIC_DIAGNOSTIC_EXIT_TRANSMISSION = 2,
  LUMICE_ANALYTIC_DIAGNOSTIC_EXTERNAL_REFLECTION = 3,
} LUMICE_ANALYTIC_DiagnosticInterfaceKind;

typedef struct LUMICE_ANALYTIC_DiagnosticInterface_ {
  int interface_index;  // position in faces, 0..face_count-1
  int face_number;
  int kind;  // LUMICE_ANALYTIC_DiagnosticInterfaceKind
  double coefficient;
  int pose_derivative_available;
  int index_derivative_available;
  double pose_gradient[3];  // body-axis gradient in the right-trivialised pose chart
  double index_derivative;
} LUMICE_ANALYTIC_DiagnosticInterface;

typedef struct LUMICE_ANALYTIC_DiagnosticMargin_ {
  const char* name;  // NUL-terminated, owned by the result
  int interface_index;
  double value;
  int pose_derivative_available;
  int index_derivative_available;
  double pose_gradient[3];
  double index_derivative;
} LUMICE_ANALYTIC_DiagnosticMargin;

typedef struct LUMICE_ANALYTIC_DiagnosticFieldResult_ {
  // Caller sets sizeof(*out); also the batch stride. The version 7 minimum layout runs through
  // `storage`. Later fields may be appended after it under doc/analytic-api.md section 8.2.
  uint32_t struct_size;
  int row_error;     // LUMICE_ANALYTIC_ErrorCode; bad rows do not fail the batch
  int path_status;   // LUMICE_ANALYTIC_DiagnosticPathStatus
  int entry_status;  // LUMICE_ANALYTIC_DiagnosticEntryStatus
  double outgoing_direction[3];
  double entry_measure;  // crystal length unit squared; 0 unless entry_status is ENTRY_OK
  double fresnel_weight;
  int interface_count;
  const LUMICE_ANALYTIC_DiagnosticInterface* interfaces;
  int domain_margin_count;
  const LUMICE_ANALYTIC_DiagnosticMargin* domain_margins;
  int tir_margin_count;
  const LUMICE_ANALYTIC_DiagnosticMargin* tir_margins;
  // A derivative is available only when both finite-difference scales keep the applicable path,
  // TIR and finite-support branch stable and their Richardson local-error estimate converges.
  // See doc/analytic-api.md section 4.8 for steps, tolerances and tensor layout.
  int direction_pose_jacobian_available;
  int direction_pose_hessian_available;
  int direction_index_derivative_available;
  int entry_pose_gradient_available;
  int entry_index_derivative_available;
  double direction_pose_jacobian[9];     // [world component][body delta axis]
  double direction_pose_hessian[27];     // [world component][body axis 0][body axis 1]
  double direction_index_derivative[3];  // world components
  double entry_pose_gradient[3];         // body delta axes
  double entry_index_derivative;
  void* storage;  // opaque; LUMICE_ANALYTIC_ReleaseDiagnosticFieldResult
} LUMICE_ANALYTIC_DiagnosticFieldResult;

// Evaluates `rows[0..count)` for one deterministic crystal and concrete face sequence. A one-face
// sequence is the external reflection at that finite face: entry_measure is its projected area,
// fresnel_weight is air-to-crystal reflectance, and the sole interface has kind
// DIAGNOSTIC_EXTERNAL_REFLECTION. Sequences of 2..64 faces retain the transmitted/internal-
// reflection/transmitted meaning of EvaluatePath. The call
// constructs one internal field and reuses its geometry/path preparation across all rows; there is
// no persistent handle or global cache. Direction, pose and refractive-index validation is per row:
// a bad row has row_error = ERR_INVALID_VALUE and otherwise zero fields while later rows continue.
// A path/entry numerical outcome is result data with row_error = OK.
//
// Call errors (every walkable output is zero-filled after struct_size, so Release is safe):
//   ERR_NULL_ARG       crystal, faces, rows or out_results is NULL when count > 0
//   ERR_INVALID_VALUE  count < 0 (touches nothing); first struct_size smaller than the complete
//                      version 7 layout through storage; non-uniform result struct_size;
//                      face_count outside 1..64; an absent face; a crystal field as for EvaluatePath
//   ERR_INVALID_CONFIG crystal rejected by the engine's closed-form validity gate
//   ERR_UNKNOWN        an internal failure; any completed row storage is reclaimed
// count == 0 succeeds and touches no pointer. The first result's struct_size is the byte stride and
// every element must repeat it. Result arrays and names are valid until that row is released.
// Re-entrant: concurrent calls with distinct outputs own independent mutable scratch.
LUMICE_ANALYTIC_API LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_EvaluateDiagnosticFieldBatch(
    const LUMICE_ANALYTIC_Crystal* crystal, const int* faces, int face_count,
    const LUMICE_ANALYTIC_DiagnosticFieldRow* rows, int count, LUMICE_ANALYTIC_DiagnosticFieldResult* out_results);

// Frees one row's variable records and zeroes it after struct_size. NULL-safe, safe on a zero-filled
// row, and idempotent because the first release clears storage.
LUMICE_ANALYTIC_API void LUMICE_ANALYTIC_ReleaseDiagnosticFieldResult(LUMICE_ANALYTIC_DiagnosticFieldResult* result);

// ---------------------------------------------------------------------------------------------
// Fiber continuation from a seed (doc/analytic-api.md section 4.3): the component of
// { R : outgoing_direction(R) = target_direction } reachable from seed_pose, traced by LI's
// predictor-corrector reference solver (LI docs/phase1-math-contract.md sections 5-10). One call
// traces one component from one seed; it never claims there is no other.
// ---------------------------------------------------------------------------------------------
typedef struct LUMICE_ANALYTIC_FiberProblem_ {
  const int* faces;              // concrete Lumice face numbers: entry, internal reflections, exit
  int face_count;                // 2..64
  double refractive_index;       // finite, > 0
  double incident_direction[3];  // world unit vector, propagation sun -> crystal (LI's s)
  double target_direction[3];    // world unit vector, propagation crystal -> observer (LI's d)
  double seed_pose[9];           // row-major rotation, body -> world; required
  // +1 (or 0) traces in the library's seed orientation, -1 in the opposite one: the same component,
  // samples in reverse order. An arc that ends on an event at both ends needs both traces from the
  // same seed (LI contract section 8). The +1 orientation is deterministic and independent of the
  // target basis, but it is not LI's, whose +1 is its LAPACK build's SVD sign.
  int initial_tangent_sign;
} LUMICE_ANALYTIC_FiberProblem;

// NULL => every field at its reference default. A zero field also means "default". Defaults are LI
// docs/phase1-math-contract.md section 10.1's reference-continuation-v1 values (that section is also
// their convergence evidence); the solver's other options are fixed at their section 10.1 values.
// A negative or non-finite field is ERR_INVALID_VALUE, and so is a combination LI's
// ContinuationOptions rejects once the defaults are filled in (step_min <= step_initial <= step_max).
typedef struct LUMICE_ANALYTIC_ContinuationOptions_ {
  double seed_residual_tolerance;  // LI residual_tolerance, 1e-11: the seed gate, corrector root and
                                   // acceptance all use it; residual = |basis^T (F - d)|
  double step_initial;             // LI initial_step, 0.04 rad
  double step_min;                 // LI minimum_step, 1e-5 rad
  double step_max;                 // LI maximum_step, 0.12 rad
  int max_accepted_steps;          // LI maximum_accepted_steps, 4000
  int closure_min_steps;           // LI closure_minimum_steps, 3
  double closure_pose_tolerance;   // LI closure_distance, 0.08 rad (SO(3) angle to the seed)
} LUMICE_ANALYTIC_ContinuationOptions;

// Closed set: exactly one of these, as in LI's phase1-math-contract.md section 9.4.
typedef enum LUMICE_ANALYTIC_FiberStatus_ {
  LUMICE_ANALYTIC_FIBER_CLOSED = 0,
  LUMICE_ANALYTIC_FIBER_EVENT_TERMINATED = 1,
  LUMICE_ANALYTIC_FIBER_NUMERICAL_FAILURE = 2,
  LUMICE_ANALYTIC_FIBER_BUDGET_EXHAUSTED = 3,
} LUMICE_ANALYTIC_FiberStatus;

// OPEN set: values are grouped by status (0 / 100+ / 200+ / 300+); later versions may add values,
// so a caller must handle an unknown value inside a known status — including
// INVALID_NUMERICAL_INPUT, which is what an element-level bad input reports today. Precedence: a
// known domain event outranks the numerical symptom it caused (LI section 9.4).
typedef enum LUMICE_ANALYTIC_Reason_ {
  LUMICE_ANALYTIC_REASON_UNKNOWN = -1,
  LUMICE_ANALYTIC_REASON_CLOSED_LOOP = 0,
  LUMICE_ANALYTIC_REASON_TIR_BOUNDARY = 100,
  LUMICE_ANALYTIC_REASON_BRANCH_BOUNDARY = 101,
  LUMICE_ANALYTIC_REASON_PATH_INFEASIBLE = 102,
  LUMICE_ANALYTIC_REASON_VISIBILITY_BOUNDARY = 103,
  LUMICE_ANALYTIC_REASON_CHART_BOUNDARY = 104,
  LUMICE_ANALYTIC_REASON_RANK_LOSS = 105,
  LUMICE_ANALYTIC_REASON_TOPOLOGY_AMBIGUITY = 106,
  LUMICE_ANALYTIC_REASON_CORRECTOR_FAILURE = 200,
  LUMICE_ANALYTIC_REASON_LINEAR_SOLVE_FAILURE = 201,
  LUMICE_ANALYTIC_REASON_NON_FINITE = 202,
  LUMICE_ANALYTIC_REASON_STEP_UNDERFLOW = 203,
  LUMICE_ANALYTIC_REASON_INVALID_NUMERICAL_INPUT = 204,
  LUMICE_ANALYTIC_REASON_STEP_BUDGET = 300,
  LUMICE_ANALYTIC_REASON_ARCLENGTH_BUDGET = 301,
  LUMICE_ANALYTIC_REASON_EVALUATION_BUDGET = 302,
} LUMICE_ANALYTIC_Reason;

// One traced component. N = pose_count accepted samples, the seed first. On a closed loop the last
// sample is the closure-corrected pose back at the seed (it replaces the last step's end, not an
// extra sample), so a closed result holds the seed twice, first and last. N = 0 when the seed itself
// is rejected (not on the path's domain, not a regular root, the target's antipode); N = 1 when the
// first step already ends the trace. Every array of length 0 is NULL; every other one points into
// `storage` and is valid until LUMICE_ANALYTIC_ReleaseFiberResult (for a nested result of
// DiscoverComponents, until LUMICE_ANALYTIC_ReleaseDiscoveryResult).
//
// Per-pose diagnostics (version 5), aligned with `poses`; their meaning is the one EvaluatePath's
// fields have at that pose in LI docs/analytic-parity-fixtures.md section 3.1:
//   branch_margin_count  k = face_count + 2; 0 when N is 0.
//   branch_margin_names  k NUL-terminated names, the validity margins in the order the path meets
//                        them: entry_incidence_cosine, entry_snell_discriminant,
//                        internal_<j>_incidence_cosine (j = 1 .. face_count - 2),
//                        exit_incidence_cosine, exit_snell_discriminant. The spelling is LI's.
//   branch_margins       N * k, row-major (pose, margin). Incidence cosines of the ray with the face
//                        normal and Snell discriminants 1 - n_rel^2 (1 - cos^2); dimensionless, all
//                        > 0 at an accepted pose, and the smallest says which boundary is nearest.
//   jacobian_available   N; 1 where the normal Jacobian exists. Every accepted pose is on the smooth
//                        branch, so 0 would mean it could not be formed (a non-finite derivative).
//   normal_jacobian      N, J_perp = sigma1 * sigma2: the 2 x 3 derivative of the outgoing direction
//                        under right-trivialised pose rotations, projected on the tangent plane at
//                        the pose's OWN outgoing direction (not the target, which a sample misses by
//                        its residual). Dimensionless, basis-invariant. NaN where unavailable — never
//                        a plausible 0 or 1.
//   singular_values      N * 2, (sigma1, sigma2), sigma1 >= sigma2 >= 0, of the same matrix. NaN
//                        where unavailable.
// These six are written only when struct_size covers all of them. A struct_size that ends inside
// them gets none: its bytes there are zero-filled like every byte after struct_size's field, and
// zero / NULL means "not provided" (section 8.2). A caller compiled against version 4 keeps the
// version 4 behaviour.
typedef struct LUMICE_ANALYTIC_FiberResult_ {
  uint32_t struct_size;                        // caller sets sizeof(*out_result) (section 8.2)
  int status;                                  // LUMICE_ANALYTIC_FiberStatus
  int reason;                                  // LUMICE_ANALYTIC_Reason (open set)
  int pose_count;                              // N accepted samples
  const double* poses;                         // N * 9, row-major, body -> world
  const double* crystal_frame_sun_directions;  // N * 3, u = R^T (-incident_direction)
  const double* arclength_increments;          // N - 1, SO(3) geodesic angle between samples, radians
  const double* residual_norms;                // N, |basis^T (outgoing - target)| in the target chart
  const double* tangents;                      // N * 3, unit, body frame (right-trivialised), in order
  void* storage;                               // opaque; LUMICE_ANALYTIC_ReleaseFiberResult
  // Version 5 (see above).
  int branch_margin_count;                 // k
  const char* const* branch_margin_names;  // k names
  const double* branch_margins;            // N * k
  const int* jacobian_available;           // N
  const double* normal_jacobian;           // N
  const double* singular_values;           // N * 2
} LUMICE_ANALYTIC_FiberResult;

// TraceFiberBatch with count = 1 (the same code path): *out_result as that batch's one element.
LUMICE_ANALYTIC_API LUMICE_ANALYTIC_ErrorCode
LUMICE_ANALYTIC_TraceFiber(const LUMICE_ANALYTIC_Crystal* crystal, const LUMICE_ANALYTIC_FiberProblem* problem,
                           const LUMICE_ANALYTIC_ContinuationOptions* options, LUMICE_ANALYTIC_FiberResult* out_result);

// One crystal, one options block, `count` independent problems (typically one path swept over many
// target directions). out_results: caller-allocated array of `count`; its stride is
// out_results[0].struct_size, which every element must carry (section 8.2), and which may be this
// struct's sizeof or that of an earlier version (the version 4 layout is the smallest accepted).
// Each element is filled and released independently. The library starts no threads.
//
// What belongs to one problem is that element's result, never the call's return code: a problem
// with face_count outside 2..64, a face number the crystal does not have, a non-finite or non-unit
// direction, a seed that is not a rotation, refractive_index not finite and positive, or
// initial_tangent_sign not in {-1, 0, 1} gets status NUMERICAL_FAILURE, reason
// INVALID_NUMERICAL_INPUT and pose_count 0, and the other elements are traced as usual. (EvaluatePath
// reports the same inputs as ERR_INVALID_VALUE: one pose, one call.) What happens along a fiber —
// TIR, rank loss, a budget — is likewise status/reason.
//
// Call errors:
//   ERR_INVALID_VALUE  count < 0 — returns at once without touching out_results, whose length is
//                      unknown; out_results[0].struct_size smaller than the version 4 layout — only element
//                      0 is zero-filled, as the stride is unusable; elements disagreeing on
//                      struct_size; an invalid options block; a crystal field as for EvaluatePath
//   ERR_NULL_ARG       crystal, problems or out_results NULL (count > 0); a problem with faces NULL
//                      and face_count > 0
//   ERR_INVALID_CONFIG crystal rejected by the engine's closed-form validity gate
//   ERR_UNKNOWN        an internal failure (out of memory); every element released and zero-filled
// On every call error but the two noted above, all `count` elements are zero-filled, so Release is
// safe on each. `count == 0` is a legal empty batch: a no-op success that touches nothing.
// Re-entrant: safe to call concurrently on distinct outputs; keeps no state between calls.
LUMICE_ANALYTIC_API LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_TraceFiberBatch(
    const LUMICE_ANALYTIC_Crystal* crystal, const LUMICE_ANALYTIC_FiberProblem* problems, int count,
    const LUMICE_ANALYTIC_ContinuationOptions* options, LUMICE_ANALYTIC_FiberResult* out_results);

// Frees what TraceFiber / TraceFiberBatch allocated for one result and zeroes it after struct_size.
// NULL-safe; a no-op on a zero-filled struct.
LUMICE_ANALYTIC_API void LUMICE_ANALYTIC_ReleaseFiberResult(LUMICE_ANALYTIC_FiberResult* result);

// ---------------------------------------------------------------------------------------------
// Component discovery / seed search (doc/analytic-api.md section 4.3): for one path and one target
// direction, the seeds of the fiber { R : outgoing_direction(R) = target_direction }, each distinct
// component traced once (TraceFiber's solver) and classified. LI docs/phase1-math-contract.md
// section 9.5, strategy reference-discovery-v1; the step order and gates are that section's.
//
// Deterministic: the sample is the antipodal Fibonacci lattice of `sample_count` points of the
// sun direction in the crystal frame, generated inside the call — no random numbers, so the result
// is fixed by the inputs.
//
// NOT a completeness certificate. `completeness` is procedural: COMPLETE means only that every
// admissible candidate of this sample closed or became an arc, never that every component of the
// fiber was found. A denser sample is NOT guaranteed to find a superset: it can lose a component a
// sparser one found (LI section 9.5.7). To densify, pass the sparser call's component seeds as
// `extra_seeds` of the denser one.
// ---------------------------------------------------------------------------------------------
typedef struct LUMICE_ANALYTIC_DiscoveryProblem_ {
  const int* faces;              // concrete Lumice face numbers: entry, internal reflections, exit
  int face_count;                // 2..64
  double refractive_index;       // finite, > 0
  double incident_direction[3];  // world unit vector, propagation sun -> crystal (LI's s)
  double target_direction[3];    // world unit vector (LI's d); its angle to incident_direction must
                                 // lie strictly between 0 and pi
  // Warm starts, typically the component seeds of a sparser call or a neighbouring target:
  // extra_seed_count row-major rotations (body -> world), 9 doubles each. Each is only the
  // Gauss-Newton start of its cluster; it is never traced on its own and never counts toward
  // completeness. May be NULL when extra_seed_count is 0.
  const double* extra_seeds;
  int extra_seed_count;
} LUMICE_ANALYTIC_DiscoveryProblem;

// NULL => every field at its reference default (LI section 9.5.9). A zero field also means
// "default"; a negative or non-finite one is ERR_INVALID_VALUE.
// Upper bound on DiscoveryOptions.sample_count (1e8, about 3 s of single-threaded sampling at the
// measured 28 ms per 1e6): the call cannot be cancelled, so a larger request is ERR_INVALID_VALUE.
#define LUMICE_ANALYTIC_MAX_DISCOVERY_SAMPLE_COUNT 100000000

typedef struct LUMICE_ANALYTIC_DiscoveryOptions_ {
  int sample_count;           // lattice points N; default 1000000; at most LUMICE_ANALYTIC_MAX_DISCOVERY_SAMPLE_COUNT
  double band_half_width;     // rad; candidates are sample events whose deviation is within this
                              // of the target's; default 0.2 deg
  double cluster_radius;      // rad, SO(3) geodesic; default 0.3
  double distance_threshold;  // rad, SO(3) geodesic: a corrected candidate closer than this to an
                              // accepted component's curve is that component; default the
                              // continuation's closure_pose_tolerance (0.08)
} LUMICE_ANALYTIC_DiscoveryOptions;

typedef enum LUMICE_ANALYTIC_Completeness_ {
  LUMICE_ANALYTIC_COMPLETENESS_COMPLETE = 0,  // no incomplete candidate (procedural, see above)
  LUMICE_ANALYTIC_COMPLETENESS_UNKNOWN = 1,   // at least one incomplete candidate
} LUMICE_ANALYTIC_Completeness;

typedef enum LUMICE_ANALYTIC_ComponentKind_ {
  LUMICE_ANALYTIC_COMPONENT_CLOSED = 0,  // one closed trace
  LUMICE_ANALYTIC_COMPONENT_ARC = 1,     // two traces from the seed, each ending on a boundary event
} LUMICE_ANALYTIC_ComponentKind;

// Why a traced candidate is neither a component nor a duplicate of one (LI section 9.5.5).
typedef enum LUMICE_ANALYTIC_IncompleteCause_ {
  LUMICE_ANALYTIC_INCOMPLETE_ARC_BACKWARD_FAILED = 0,          // forward on a boundary event, backward not
  LUMICE_ANALYTIC_INCOMPLETE_ARC_BACKWARD_CLOSED_ANOMALY = 1,  // forward on a boundary event, backward closed
  LUMICE_ANALYTIC_INCOMPLETE_UNNAMED_EVENT = 2,                // forward on an event that ends no arc
  LUMICE_ANALYTIC_INCOMPLETE_NOT_CONVERGED = 3,                // forward failed numerically or ran out of budget
} LUMICE_ANALYTIC_IncompleteCause;

// A component. `forward` is the trace from `seed` in the library's +1 orientation (TraceFiber's).
// For an arc, `backward` is the trace from the same seed in the opposite orientation; the arc runs
// from backward's last pose through the seed to forward's last pose, and either trace may hold only
// the seed. A boundary event ends each trace: TIR, BRANCH, PATH_INFEASIBLE, VISIBILITY or CHART
// BOUNDARY. For a closed component `backward` is NULL.
typedef struct LUMICE_ANALYTIC_DiscoveredComponent_ {
  int kind;                                     // LUMICE_ANALYTIC_ComponentKind
  double seed[9];                               // corrected seed, row-major, body -> world
  const LUMICE_ANALYTIC_FiberResult* forward;   // never NULL
  const LUMICE_ANALYTIC_FiberResult* backward;  // arc only; NULL for a closed component
} LUMICE_ANALYTIC_DiscoveredComponent;

typedef struct LUMICE_ANALYTIC_IncompleteCandidate_ {
  int cause;                                    // LUMICE_ANALYTIC_IncompleteCause
  double seed[9];                               // corrected seed, row-major, body -> world
  const LUMICE_ANALYTIC_FiberResult* forward;   // never NULL
  const LUMICE_ANALYTIC_FiberResult* backward;  // NULL unless cause is one of the ARC_BACKWARD_* values
} LUMICE_ANALYTIC_IncompleteCandidate;

// Everything reached through `components` and `incomplete`, including the FiberResults they point
// to, lives in `storage` and is freed by LUMICE_ANALYTIC_ReleaseDiscoveryResult alone. The nested
// FiberResults are views: their own `storage` is NULL, and they must not be passed to
// LUMICE_ANALYTIC_ReleaseFiberResult. Their struct_size is the library's sizeof.
typedef struct LUMICE_ANALYTIC_DiscoveryResult_ {
  uint32_t struct_size;  // caller sets sizeof(*out_result) (section 8.2)
  int completeness;      // LUMICE_ANALYTIC_Completeness
  int component_count;
  const LUMICE_ANALYTIC_DiscoveredComponent* components;  // trace order; NULL when component_count is 0
  int incomplete_count;
  const LUMICE_ANALYTIC_IncompleteCandidate* incomplete;  // trace order; NULL when incomplete_count is 0
  // The funnel (LI section 9.5.6): sample events in the band (w = A T > 0), extra seeds, clusters of
  // the pool, admissible representatives. admissible_count = dedup_merged + component_count +
  // incomplete_count.
  int pool_count;
  int extra_seed_count;
  int raw_cluster_count;
  int admissible_count;
  // The six classification counters of LI section 9.5.6. arc_stitched counts the arc components; the
  // last four sum to incomplete_count.
  int dedup_merged;
  int arc_stitched;
  int arc_backward_failed;
  int arc_backward_closed_anomaly;
  int incomplete_unnamed_event;
  int incomplete_not_converged;
  void* storage;  // opaque; LUMICE_ANALYTIC_ReleaseDiscoveryResult
} LUMICE_ANALYTIC_DiscoveryResult;

// Discovers the components of `problem`'s fiber on `crystal`. `options` NULL = reference defaults;
// `continuation` (NULL = defaults) is the one policy every trace of the call runs under — there is
// no separate discovery budget, and a starving budget shows up as NOT_CONVERGED candidates. The
// candidates must also pass the finite crystal's entry-measure gate: some ray entering the entry
// face at that pose must meet every later face of the path inside its polygon.
//
// A target with no candidate is a success: COMPLETE, zero components (a dark sky point).
//
// Call errors (out_result is zero-filled after struct_size, so Release is safe):
//   ERR_NULL_ARG       crystal, problem or out_result NULL; faces NULL with face_count > 0;
//                      extra_seeds NULL with extra_seed_count > 0
//   ERR_INVALID_VALUE  out_result->struct_size smaller than this struct; face_count outside 2..64, a
//                      face number the crystal does not have; refractive_index not finite and
//                      positive; a direction not finite or its length off 1 by more than 1e-10; the
//                      target at 0 or pi from the incident direction; extra_seed_count < 0, an extra
//                      seed that is not a rotation (as EvaluatePath checks a pose); an invalid options
//                      or continuation block; a crystal field as for EvaluatePath
//   ERR_INVALID_CONFIG crystal rejected by the engine's closed-form validity gate
//   ERR_UNKNOWN        an internal failure (out of memory)
// Re-entrant: safe to call concurrently on distinct outputs; keeps no state between calls.
LUMICE_ANALYTIC_API LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_DiscoverComponents(
    const LUMICE_ANALYTIC_Crystal* crystal, const LUMICE_ANALYTIC_DiscoveryProblem* problem,
    const LUMICE_ANALYTIC_DiscoveryOptions* options, const LUMICE_ANALYTIC_ContinuationOptions* continuation,
    LUMICE_ANALYTIC_DiscoveryResult* out_result);

// Frees what DiscoverComponents allocated, nested traces included, and zeroes the struct after
// struct_size. NULL-safe; a no-op on a zero-filled struct.
LUMICE_ANALYTIC_API void LUMICE_ANALYTIC_ReleaseDiscoveryResult(LUMICE_ANALYTIC_DiscoveryResult* result);

// ---------------------------------------------------------------------------------------------
// Band sum: the single-path brightness map (doc/analytic-api.md section 4.6; LI
// docs/band-sum-contract.md). For one concrete face sequence, one refractive index and one pose
// density, the value of each pixel of a caller-given table: the power per steradian the path sends
// toward that pixel from one crystal of the ensemble, per unit incident irradiance, for a crystal
// of hexagon edge 1 (contract section 6). The sample is DiscoverComponents' lattice: `sample_count`
// antipodal Fibonacci points of the sun direction in the crystal frame, kept where the path is
// valid and w = A T > 0; each pixel sums the kept events whose deviation lies in its corner band,
// posed at the pixel's azimuth and weighted by the density there. Deterministic: no random numbers.
//
// A rank-0 path (its reflections compose to the identity and entry and exit faces are parallel, so
// every pose sends the light straight on) is not a band but a point mass `point_mass` in the sun's
// own direction: the first pixel in table order containing the incident direction carries
// point_mass / solid_angle, every other pixel 0.
//
// v1 is single-threaded, single-wavelength and holds every kept event of the sample at once (about
// 64 bytes each), which is why sample_count has its own, lower bound.
// ---------------------------------------------------------------------------------------------

// The pose densities of contract section 2.2: a density on SO(3) relative to Haar measure that
// reads a pose only through the c axis' zenith theta and its roll psi. Degrees. random: no
// parameter (every field 0). column / plate: zenith_mean_deg in [0, 180] and zenith_std_deg > 0, a
// Gaussian in theta taken as a sphere density (normalised with sin theta, as the engine samples its
// gauss zenith); the roll fields must be 0. parry / lowitz: that zenith times a Gaussian in the
// roll, roll_mean_deg finite and roll_std_deg > 0. Column and plate (parry and lowitz) differ only
// by LI's default mean, which the C ABI does not apply: the mean is always the caller's.
typedef enum LUMICE_ANALYTIC_PoseFamily_ {
  LUMICE_ANALYTIC_POSE_RANDOM = 0,
  LUMICE_ANALYTIC_POSE_COLUMN = 1,
  LUMICE_ANALYTIC_POSE_PLATE = 2,
  LUMICE_ANALYTIC_POSE_PARRY = 3,
  LUMICE_ANALYTIC_POSE_LOWITZ = 4,
} LUMICE_ANALYTIC_PoseFamily;

typedef struct LUMICE_ANALYTIC_PoseDensity_ {
  int family;  // LUMICE_ANALYTIC_PoseFamily
  double zenith_mean_deg;
  double zenith_std_deg;
  double roll_mean_deg;
  double roll_std_deg;
} LUMICE_ANALYTIC_PoseDensity;

// The pixel table (contract section 2.3). A pixel is the spherical quadrilateral with great-circle
// edges through its four corners, given in cyclic order (either orientation). Every direction is a
// world unit propagation direction — the sky point a pixel shows is its negative. Results come back
// in table order, so the caller's own pixel labels stay the caller's.
typedef struct LUMICE_ANALYTIC_PixelTable_ {
  int pixel_count;            // >= 0
  const double* centre;       // pixel_count x 3: fixes the pixel's azimuth about the sun and its deviation
  const double* corners;      // pixel_count x 4 x 3
  const double* solid_angle;  // pixel_count, steradians, finite and > 0; read by the point mass only
} LUMICE_ANALYTIC_PixelTable;

// Upper bound on BandSumProblem.sample_count (1e7): every kept event is held and sorted at once, and
// the call cannot be cancelled, so a larger request is ERR_INVALID_VALUE. Lower than
// LUMICE_ANALYTIC_MAX_DISCOVERY_SAMPLE_COUNT, which streams its sample.
#define LUMICE_ANALYTIC_MAX_BAND_SUM_SAMPLE_COUNT 10000000

typedef struct LUMICE_ANALYTIC_BandSumProblem_ {
  const int* faces;              // concrete Lumice face numbers: entry, internal reflections, exit
  int face_count;                // 2..64
  double refractive_index;       // finite, > 0
  double incident_direction[3];  // world unit vector, propagation sun -> crystal (LI's s)
  int sample_count;              // lattice points N, 1..LUMICE_ANALYTIC_MAX_BAND_SUM_SAMPLE_COUNT; no default
  LUMICE_ANALYTIC_PoseDensity pose_density;
  LUMICE_ANALYTIC_PixelTable pixels;
} LUMICE_ANALYTIC_BandSumProblem;

typedef enum LUMICE_ANALYTIC_BandPixelStatus_ {
  LUMICE_ANALYTIC_BAND_PIXEL_OK = 0,
  LUMICE_ANALYTIC_BAND_PIXEL_SINGULAR = 1,    // the pixel contains the incident direction or its
                                              // antipode, where the band sum has no value
  LUMICE_ANALYTIC_BAND_PIXEL_POINT_MASS = 2,  // rank 0: the pixel carrying the point mass
} LUMICE_ANALYTIC_BandPixelStatus;

// One pixel (contract section 6). Band fields: delta is the centre's deviation from the incident
// direction, [delta_lo, delta_hi) the band of the corners' deviations (radians); k the kept events
// in the band, k_rho_pos those whose weighted contribution is > 0 (subnormals count: a thread with
// flush-to-zero set gets smaller counts), k_eff Kish's effective sample size (S^2 / sum c^2, 0 for an
// empty band). A SINGULAR pixel has value and k_eff NaN and zero counts. In a rank-0 result every
// pixel has delta, delta_lo, delta_hi and k_eff NaN and zero counts, and value 0 except the
// POINT_MASS pixel.
typedef struct LUMICE_ANALYTIC_BandPixel_ {
  int status;  // LUMICE_ANALYTIC_BandPixelStatus
  double value;
  double delta;
  double delta_lo;
  double delta_hi;
  int k;
  int k_rho_pos;
  double k_eff;
} LUMICE_ANALYTIC_BandPixel;

// How a rank-0 mass was computed: under the random density the lattice mean of w (exact on the
// sample); under any other the library's deterministic average over the twist about the sun of
// each kept event, integrated to 1e-6 relative (point_mass_error is the change of its last
// refinement). LI computes the latter by Monte Carlo; the two agree only statistically.
typedef enum LUMICE_ANALYTIC_PointMassMethod_ {
  LUMICE_ANALYTIC_POINT_MASS_LATTICE_MEAN = 0,
  LUMICE_ANALYTIC_POINT_MASS_TWIST_AVERAGE = 1,
} LUMICE_ANALYTIC_PointMassMethod;

typedef struct LUMICE_ANALYTIC_BandSumResult_ {
  uint32_t struct_size;  // caller sets sizeof(*out_result) (section 8.2)
  int rank_zero;         // 1: the point-mass fields below apply and no pixel has band fields
  int kept_count;        // kept events of the sample (w > 0), either rank
  int pixel_count;
  const LUMICE_ANALYTIC_BandPixel* pixels;  // table order; NULL when pixel_count is 0
  // Rank 0 only (0 otherwise). point_mass_pixel is the index of the POINT_MASS pixel, or -1 when no
  // pixel contains the incident direction (the mass is then reported but lands nowhere).
  double point_mass;
  double point_mass_error;
  int point_mass_method;  // LUMICE_ANALYTIC_PointMassMethod
  int point_mass_pixel;
  void* storage;  // opaque; LUMICE_ANALYTIC_ReleaseBandSumResult
} LUMICE_ANALYTIC_BandSumResult;

// The band sum of `problem` on `crystal`. A path no pose realises is a success with kept_count 0
// and every value 0; so are an empty band, a singular pixel and a rank-0 sun outside every pixel.
//
// Call errors (out_result is zero-filled after struct_size, so Release is safe):
//   ERR_NULL_ARG       crystal, problem or out_result NULL; faces NULL with face_count > 0; a pixel
//                      array NULL with pixel_count > 0
//   ERR_INVALID_VALUE  out_result->struct_size smaller than this struct; face_count outside 2..64, a
//                      face number the crystal does not have; refractive_index not finite and
//                      positive; the incident direction, a pixel centre or a corner not finite or its
//                      length off 1 by more than 1e-10; sample_count outside
//                      1..LUMICE_ANALYTIC_MAX_BAND_SUM_SAMPLE_COUNT; pixel_count < 0; a solid angle
//                      not finite and positive; a pose density with an unknown family, a missing or
//                      non-positive width, a zenith mean outside [0, 180] or a field its family does
//                      not use left non-zero; a crystal field as for EvaluatePath
//   ERR_INVALID_CONFIG crystal rejected by the engine's closed-form validity gate
//   ERR_UNKNOWN        an internal failure (out of memory)
// Re-entrant: safe to call concurrently on distinct outputs; keeps no state between calls (the
// sample is rebuilt on every call).
LUMICE_ANALYTIC_API LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_BandSum(const LUMICE_ANALYTIC_Crystal* crystal,
                                                                      const LUMICE_ANALYTIC_BandSumProblem* problem,
                                                                      LUMICE_ANALYTIC_BandSumResult* out_result);

// Frees what BandSum allocated and zeroes the struct after struct_size. NULL-safe; a no-op on a
// zero-filled struct.
LUMICE_ANALYTIC_API void LUMICE_ANALYTIC_ReleaseBandSumResult(LUMICE_ANALYTIC_BandSumResult* result);

// ---------------------------------------------------------------------------------------------
// General feature discovery over a caller-supplied finite support. Unlike DiscoverComponents,
// this operation has no target direction and no crystal-family vocabulary. The caller enumerates
// its actual measure support, topology, weights and named constraint margins; the library owns all
// classification and equal-area sky aggregation. Directions are world propagation directions
// (crystal -> observer), so their displayed sky points are their negatives.
// ---------------------------------------------------------------------------------------------
#define LUMICE_ANALYTIC_FEATURE_SUPPORT_VERSION_V1 1
#define LUMICE_ANALYTIC_FEATURE_SUPPORT_VERSION_V2 2
#define LUMICE_ANALYTIC_FEATURE_SUPPORT_VERSION 3
// Version 1's frozen cap. Versions 2 and 3 use checked dynamic buffers and have no dimension-only cap.
#define LUMICE_ANALYTIC_MAX_FEATURE_COORDINATE_DIMENSION 16

typedef enum LUMICE_ANALYTIC_FeatureEvidenceStatus_ {
  LUMICE_ANALYTIC_FEATURE_CONFIRMED = 0,
  LUMICE_ANALYTIC_FEATURE_CANDIDATE = 1,
  LUMICE_ANALYTIC_FEATURE_NOT_DETECTED_AT_RESOLUTION = 2,
  LUMICE_ANALYTIC_FEATURE_NUMERICAL_INCOMPLETE = 3,
  LUMICE_ANALYTIC_FEATURE_PHYSICALLY_UNREACHABLE = 4,
  LUMICE_ANALYTIC_FEATURE_NOT_SUPPORTED = 5,
} LUMICE_ANALYTIC_FeatureEvidenceStatus;

typedef enum LUMICE_ANALYTIC_FeatureMechanism_ {
  LUMICE_ANALYTIC_FEATURE_INTERIOR_RANK_LOSS = 0,
  LUMICE_ANALYTIC_FEATURE_SUPPORT_BOUNDARY = 1,
  LUMICE_ANALYTIC_FEATURE_SUPPORT_CORNER = 2,
  LUMICE_ANALYTIC_FEATURE_OPTICAL_KINK = 3,
  LUMICE_ANALYTIC_FEATURE_FILTER_BOUNDARY = 4,
  LUMICE_ANALYTIC_FEATURE_WEIGHT_KINK = 5,
  LUMICE_ANALYTIC_FEATURE_MEASURE_ATOM = 6,
  LUMICE_ANALYTIC_FEATURE_STRICT_CONFINEMENT = 7,
  LUMICE_ANALYTIC_FEATURE_FINITE_WIDTH_CONCENTRATION = 8,
  LUMICE_ANALYTIC_FEATURE_BRIGHTNESS_MAXIMUM = 9,
  LUMICE_ANALYTIC_FEATURE_BRIGHTNESS_RIDGE = 10,
} LUMICE_ANALYTIC_FeatureMechanism;

typedef enum LUMICE_ANALYTIC_SupportMeasureKind_ {
  LUMICE_ANALYTIC_SUPPORT_ATOM = 0,
  LUMICE_ANALYTIC_SUPPORT_CONTINUOUS = 1,
} LUMICE_ANALYTIC_SupportMeasureKind;

typedef enum LUMICE_ANALYTIC_MappingEvidenceKind_ {
  LUMICE_ANALYTIC_MAPPING_EVIDENCE_NONE = 0,
  LUMICE_ANALYTIC_MAPPING_EVIDENCE_EXACT_IMAGE_DIMENSION_UPPER_BOUND = 1,
} LUMICE_ANALYTIC_MappingEvidenceKind;

typedef enum LUMICE_ANALYTIC_ConstraintKind_ {
  LUMICE_ANALYTIC_CONSTRAINT_DOMAIN = 0,
  LUMICE_ANALYTIC_CONSTRAINT_ENTRY = 1,
  LUMICE_ANALYTIC_CONSTRAINT_TIR = 2,
  LUMICE_ANALYTIC_CONSTRAINT_FILTER = 3,
  LUMICE_ANALYTIC_CONSTRAINT_WEIGHT = 4,
} LUMICE_ANALYTIC_ConstraintKind;

typedef struct LUMICE_ANALYTIC_FeatureProvenance_ {
  int member_index;
  int layer_index;
  int interface_index;
  int spectrum_node_id;
  int source_node_id;
  int sample_index;
} LUMICE_ANALYTIC_FeatureProvenance;

typedef struct LUMICE_ANALYTIC_SupportConstraint_ {
  uint32_t struct_size;
  const char* name;  // borrowed NUL-terminated string
  int kind;          // LUMICE_ANALYTIC_ConstraintKind
  int layer_index;
  int interface_index;
  double value;
  int numerically_available;
  int gradient_available;
  const double* gradient;  // coordinate_dimension entries when available
} LUMICE_ANALYTIC_SupportConstraint;

typedef struct LUMICE_ANALYTIC_FeatureSupportSample_ {
  uint32_t struct_size;
  uint64_t sample_id;
  LUMICE_ANALYTIC_FeatureProvenance provenance;
  int measure_kind;  // LUMICE_ANALYTIC_SupportMeasureKind
  int support_dimension;
  int finite_width;
  const double* coordinates;      // batch coordinate_dimension entries
  const int* active_coordinates;  // support_dimension unique indices
  double direction[3];
  double weight;
  int direction_jacobian_available;
  const double* direction_jacobian;                    // row-major 3 x coordinate_dimension, or NULL
  const uint8_t* direction_jacobian_column_available;  // coordinate_dimension entries with Jacobian
  double direction_jacobian_error;
  double direction_jacobian_resolution;
  int constraint_count;
  uint32_t constraint_stride;  // sizeof(LUMICE_ANALYTIC_SupportConstraint)
  const LUMICE_ANALYTIC_SupportConstraint* constraints;
  int numerically_available;
  int accumulates_measure;  // ADDED version 10; 0 for local probes, 1 for input-measure rows
  // ADDED version 11. This certifies the complete continuous support cell; it is not inferred
  // from finite probes. NONE uses zero-filled trailing fields; EXACT requires mapping_error_bound == 0 and a
  // bound in [0,min(2,support_dimension)].
  int mapping_evidence_kind;  // LUMICE_ANALYTIC_MappingEvidenceKind
  int image_dimension_upper_bound;
  double mapping_error_bound;
} LUMICE_ANALYTIC_FeatureSupportSample;

typedef struct LUMICE_ANALYTIC_FeatureSupportEdge_ {
  int first;
  int second;
  double parameter_distance;
} LUMICE_ANALYTIC_FeatureSupportEdge;

typedef struct LUMICE_ANALYTIC_FeatureSupportCellAxis_ {
  int cell_id;
  int coordinate_index;
  int lower;
  int center;
  int upper;
  double parameter_span;
} LUMICE_ANALYTIC_FeatureSupportCellAxis;

typedef struct LUMICE_ANALYTIC_FeatureSupportBatch_ {
  uint32_t struct_size;
  uint32_t version;  // LUMICE_ANALYTIC_FEATURE_SUPPORT_VERSION
  int coordinate_dimension;
  uint64_t visited_row_count;
  int complete_visit;
  int materialization_complete;
  int sample_count;
  uint32_t sample_stride;  // caller sample layout stride; version 1 accepts its frozen v9 prefix
  const LUMICE_ANALYTIC_FeatureSupportSample* samples;
  int edge_count;
  const LUMICE_ANALYTIC_FeatureSupportEdge* edges;
  // ADDED version 10; required by feature-support version 2, absent from version 1's frozen prefix.
  int cell_axis_count;
  const LUMICE_ANALYTIC_FeatureSupportCellAxis* cell_axes;
} LUMICE_ANALYTIC_FeatureSupportBatch;

typedef struct LUMICE_ANALYTIC_FeatureReevaluationRequest_ {
  LUMICE_ANALYTIC_FeatureProvenance provenance;
  int coordinate_dimension;
  const double* coordinates;
} LUMICE_ANALYTIC_FeatureReevaluationRequest;

// The library initializes out_sample->struct_size and calls synchronously. The callback returns 1
// only after filling a complete sample for the requested coordinates and provenance. All pointers
// it writes are borrowed only until the callback returns; the library copies them before returning
// control. A 0 return is a local numerical-unavailability outcome and does not fail the call.
typedef int (*LUMICE_ANALYTIC_FeatureReevaluateFn)(const LUMICE_ANALYTIC_FeatureReevaluationRequest* request,
                                                   LUMICE_ANALYTIC_FeatureSupportSample* out_sample, void* user_data);

typedef struct LUMICE_ANALYTIC_FeatureDiscoveryOptions_ {
  uint32_t struct_size;
  double margin_tolerance;         // 0 selects the default
  double rank_relative_tolerance;  // 0 selects the default
  double sky_merge_tolerance;      // 0 selects the default
  int maximum_refinement_steps;    // 0 selects the default
  int sky_z_bins;                  // 0 selects 8; otherwise even and >= 4
  int sky_azimuth_bins;            // 0 selects 16; otherwise even and >= 8
} LUMICE_ANALYTIC_FeatureDiscoveryOptions;

typedef struct LUMICE_ANALYTIC_FeatureCandidate_ {
  int mechanism;  // LUMICE_ANALYTIC_FeatureMechanism
  int status;     // LUMICE_ANALYTIC_FeatureEvidenceStatus
  LUMICE_ANALYTIC_FeatureProvenance provenance;
  double direction[3];
  int support_dimension;
  int mapping_rank;
  double singular_values[2];
  double weighted_mass;
  int has_weight_sides;
  double weight_sides[2];
  double residual;
  double resolution;
  int active_constraint_count;
  const char* const* active_constraints;  // owned by result
  const char* reason;                     // owned by result
} LUMICE_ANALYTIC_FeatureCandidate;

typedef struct LUMICE_ANALYTIC_FeatureMechanismRecord_ {
  int mechanism;  // LUMICE_ANALYTIC_FeatureMechanism
  int status;     // LUMICE_ANALYTIC_FeatureEvidenceStatus
  int candidate_count;
  const char* reason;  // owned by result
} LUMICE_ANALYTIC_FeatureMechanismRecord;

typedef struct LUMICE_ANALYTIC_SkyFieldNode_ {
  double direction[3];
  double value;
  double normalized_value;
  double gradient_norm;
  double hessian_eigenvalues[2];
  double error;
  double resolution;
  int sample_count;
  int status;  // LUMICE_ANALYTIC_FeatureEvidenceStatus
} LUMICE_ANALYTIC_SkyFieldNode;

typedef struct LUMICE_ANALYTIC_FeatureDiscoveryResult_ {
  uint32_t struct_size;  // caller sets sizeof(*out_result)
  uint64_t visited_row_count;
  int evaluated_sample_count;
  int complete_visit;
  int materialization_complete;
  int candidate_count;
  const LUMICE_ANALYTIC_FeatureCandidate* candidates;
  int mechanism_count;
  const LUMICE_ANALYTIC_FeatureMechanismRecord* mechanisms;
  int sky_field_count;
  const LUMICE_ANALYTIC_SkyFieldNode* sky_field;
  void* storage;  // opaque; LUMICE_ANALYTIC_ReleaseFeatureDiscoveryResult
} LUMICE_ANALYTIC_FeatureDiscoveryResult;

// Input rows and nested constraints are walked with their declared strides. Counts must be
// non-negative, every stride must cover the selected support version's layout, and every required
// pointer must be non-NULL. A semantically malformed support is ERR_INVALID_VALUE rather than a discovery
// status. The callback and user_data are borrowed synchronously; NULL disables refinement.
// Re-entrant: concurrent calls with distinct outputs own independent storage and callback state.
LUMICE_ANALYTIC_API LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_DiscoverFeatures(
    const LUMICE_ANALYTIC_FeatureSupportBatch* batch, const LUMICE_ANALYTIC_FeatureDiscoveryOptions* options,
    LUMICE_ANALYTIC_FeatureReevaluateFn reevaluate, void* user_data,
    LUMICE_ANALYTIC_FeatureDiscoveryResult* out_result);

// Frees all candidate strings/arrays, mechanism records and sky nodes, then zeroes the result after
// struct_size. NULL-safe and idempotent on a zero-filled result.
LUMICE_ANALYTIC_API void LUMICE_ANALYTIC_ReleaseFeatureDiscoveryResult(LUMICE_ANALYTIC_FeatureDiscoveryResult* result);


#ifdef __cplusplus
}
#endif

#endif  // LUMICE_ANALYTIC_CORE_H_
