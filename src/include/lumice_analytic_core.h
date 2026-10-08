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
// Version 8 adds the u-S^2 field layer of one fixed face sequence (kind-1/2/3 critical structure,
// the delta-axis partition with its escape regimes, per-wavelength onset tables, the restricted
// family curve and the chromatic verdicts).
// Version notes, newest first (every bump says what changed, doc/analytic-api.md section 8.1):
//   8  ADDED LUMICE_ANALYTIC_WalkStatus, LUMICE_ANALYTIC_EscapeRegime, LUMICE_ANALYTIC_CriticalKind,
//      LUMICE_ANALYTIC_OnsetLocation, LUMICE_ANALYTIC_OnsetSource, LUMICE_ANALYTIC_OnsetProfile,
//      LUMICE_ANALYTIC_KinkCoverage, LUMICE_ANALYTIC_CurveExistence,
//      LUMICE_ANALYTIC_ChromaticFeatureKind, LUMICE_ANALYTIC_ChromaticColor,
//      LUMICE_ANALYTIC_ChromaticVerdictKind, LUMICE_ANALYTIC_CriticalOnset,
//      LUMICE_ANALYTIC_DeviationInterval, LUMICE_ANALYTIC_WavelengthOnsetRow,
//      LUMICE_ANALYTIC_WeightKinkArc, LUMICE_ANALYTIC_WeightKinkCurve,
//      LUMICE_ANALYTIC_ChromaticFeature, LUMICE_ANALYTIC_ChromaticThresholds,
//      LUMICE_ANALYTIC_PlateFamily, and the result structs and functions of the eight calls
//      TraceBoundaryLoop, TraceWeightKinks, ClassifyCriticalStructure, PartitionDeviationAxis,
//      TraceWavelengthCriticalTable, TraceRestrictedFamilyCurve, DiagnoseChromatic,
//      DiagnoseClassTint with their seven Release functions. Nothing existing changed.
//   7  ADDED LUMICE_ANALYTIC_DiagnosticSource, LUMICE_ANALYTIC_WeightedSkySample,
//      LUMICE_ANALYTIC_DiagnosticInterface, LUMICE_ANALYTIC_DiagnosticOptics,
//      LUMICE_ANALYTIC_SkyFieldPoint, LUMICE_ANALYTIC_SourceEventRange,
//      LUMICE_ANALYTIC_DiagnosticResult, LUMICE_ANALYTIC_EvaluateDiagnosticBatch,
//      LUMICE_ANALYTIC_TraceDiagnosticInterface, LUMICE_ANALYTIC_TraceWeightedSkyField,
//      LUMICE_ANALYTIC_ReleaseDiagnosticResult, LUMICE_ANALYTIC_CorrectDeviationBatch —
//      conditional optics and fixed-observation numerical discovery (section 4.7's struct_size
//      group rules). Nothing existing changed.
//   6  ADDED LUMICE_ANALYTIC_PoseFamily, LUMICE_ANALYTIC_PoseDensity, LUMICE_ANALYTIC_PixelTable,
//      LUMICE_ANALYTIC_BandSumProblem, LUMICE_ANALYTIC_BandPixelStatus, LUMICE_ANALYTIC_BandPixel,
//      LUMICE_ANALYTIC_PointMassMethod, LUMICE_ANALYTIC_BandSumResult, LUMICE_ANALYTIC_BandSum,
//      LUMICE_ANALYTIC_ReleaseBandSumResult — the single-path brightness map (section 4.6).
//      Nothing existing changed.
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

#include <stddef.h>
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
#define LUMICE_ANALYTIC_API_VERSION 8

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
// Version 7: conditional optics and fixed-observation numerical discovery. No Scene, symmetry,
// actual/candidate product labels, or cache handle. Inputs are fixed POD layouts for this version.
// A batch shares faces/options; bad shape/n/pose/source is per-row, malformed array/path is a call
// error. Result arrays are immutable and library-owned until ReleaseDiagnosticResult; no pointer
// survives release. Distinct outputs are re-entrant; do not release while another thread reads.
// The root result uses struct_size, including in Release. Its first group ends at storage;
// source-event fields and field-terminal status are successive optional complete groups.
// Each older complete group remains readable even when a later group does not fit. Smaller roots
// are refused and only declared bytes are cleared. Larger caller structs retain unknown bytes.
//
// Coordinates: pose is body-to-world, row-major; incident/outgoing are propagation directions.
// Weighted samples are VIEWING directions. Weights already include every physical/spectral
// coefficient exactly once. has_orbit=1 declares a NORMALIZED full uniform world-axis orbit:
// the caller must establish invariance of its measure and weight; never infer it from sparse rows.
// Rows sharing an outer statistical draw must share sample_index; token is opaque provenance.
// Tangent basis consists of two consecutive world unit vectors. Jets are per sr, per rad and
// per rad^2; their five fields are X,Y,Z,x,y. No xy data is usable unless xy_available=1.
//
// solve_status: 0 converged, 1 invalid input, 2 unavailable, 3 no support at iterate, 4 degenerate,
// 5 iteration limit, 6 budget exceeded. EvaluateDiagnosticBatch does not solve an equation (2).
// deviation_available covers the angle/correction/curvature/error group, all at the returned
// source/value. Interrupted correction returns its last complete snapshot, or partial optics
// with deviation_available=0 if none completed; path_evaluations still counts all attempted work.
// Field status: 0 converged, 1 invalid input, 2 no signal, 3 degenerate, 4 iteration limit, 5 budget.
// field_terminal_status (only when field_terminal_available=1) keeps the final solver reason
// separate from accepted geometry: the failed/censored trial, or the last accepted point on
// a normal stop. Input-copy budget exhaustion also reports status 5. Failed iterates never
// enter field[]. Other diagnostic functions leave this suffix unavailable.
// Field equation: 0 log-Y peak, 1 log-Y ridge, 2 x level, 3 y level. A root is numerical only.
// WalkField termination: 0 closed, 1 observation censored, 2 corrector failed, 3 point limit,
// 4 invalid input, 5 budget exhausted. WalkEvent termination: 0 closed, 1 area threshold, 2 geometric contact bracket,
// 3 optical gate, 4 corrector failed, 5 point limit, 6 budget exceeded, 7 invalid input.
// Event walking uses the incident S2 quotient; the caller must admit the full Haar pose chart.
// Step is .01 rad, endpoint bracket width <=1e-7 rad, discriminant tolerance 1e-10. It may retain
// source points below the product area threshold to locate the distinct raw contact bracket.
// Field walking uses vMF kappa=1/h^2, steps .5h, corrector tolerance 1e-8 rad; no physical endpoint
// or actualness is inferred from its termination. Equation 0 returns one corrected peak, not a curve. max_points and
// explicit work/deadline bound it. Same-cloud inputs are copied within each call; no hidden cold preparation or
// persistent cache. Independent prefix/replicate error and scale response belong to the consumer, not these roots.
// ---------------------------------------------------------------------------------------------
typedef struct LUMICE_ANALYTIC_DiagnosticSource {
  LUMICE_ANALYTIC_Crystal crystal;
  double pose[9];
  double incident[3];
  double refractive_index;
  uint64_t token;
} LUMICE_ANALYTIC_DiagnosticSource;
typedef struct LUMICE_ANALYTIC_WeightedSkySample {
  uint64_t sample_index;
  uint64_t source_token;
  double direction[3];
  double xyz_weight[3];
  int has_orbit;
  double orbit_axis[3];
} LUMICE_ANALYTIC_WeightedSkySample;
typedef struct LUMICE_ANALYTIC_DiagnosticInterface {
  int reached, factor_available, pose_derivative_available, index_derivative_available;
  double incidence, discriminant, factor, pose_gradient[3], index_derivative, index_error;
} LUMICE_ANALYTIC_DiagnosticInterface;
typedef struct LUMICE_ANALYTIC_DiagnosticOptics {
  LUMICE_ANALYTIC_DiagnosticSource source;
  int solve_status, input_status, path_valid, optical_failure, entry_available, geometry_evaluated;
  double outgoing[3], area, raw_area, area_threshold, interface_product;
  double direction_pose_jacobian[9], direction_index_derivative[3], direction_index_error;
  int direction_pose_available, direction_index_available, interface_count;
  const LUMICE_ANALYTIC_DiagnosticInterface* interfaces;  // interface_count immutable rows
  size_t corridor_vertex_count;
  const double* corridor_vertices;  // packed xy; one original slot/edge pair per outgoing edge
  const int* corridor_edge_sources;
  double corridor_basis[6];
  int deviation_available;
  double deviation_rad, correction_rad, objective_curvatures[2], hessian_error;
} LUMICE_ANALYTIC_DiagnosticOptics;
typedef struct LUMICE_ANALYTIC_SkyFieldPoint {
  int status;
  double direction[3], tangent_basis[6], bandwidth_rad;
  // Five covariant jets: X,Y,Z,x,y; each value,g0,g1,H00,H01,H11.
  double jets[30];
  int xy_available;
  double effective_samples_y, correction_rad, log_y_curvatures[2];
} LUMICE_ANALYTIC_SkyFieldPoint;
typedef struct LUMICE_ANALYTIC_SourceEventRange {
  int kind;  // 1 = area threshold; 2 = raw geometric contact bracket; 3 = optical validity gate
  size_t positive_index, nonpositive_index;  // indices in optical
  double source_width_rad;
} LUMICE_ANALYTIC_SourceEventRange;
typedef struct LUMICE_ANALYTIC_DiagnosticResult {
  uint32_t struct_size;
  size_t optical_count, field_count;
  const LUMICE_ANALYTIC_DiagnosticOptics* optical;
  const LUMICE_ANALYTIC_SkyFieldPoint* field;
  uint64_t path_evaluations, component_evaluations;
  int termination;
  void* storage;
  size_t curve_point_count, source_event_count;
  const LUMICE_ANALYTIC_SourceEventRange* source_events;
  // Optional complete suffix; zero availability means no field terminal status.
  int field_terminal_available, field_terminal_status;
} LUMICE_ANALYTIC_DiagnosticResult;
LUMICE_ANALYTIC_API LUMICE_ANALYTIC_ErrorCode
LUMICE_ANALYTIC_EvaluateDiagnosticBatch(const int* faces, int face_count, const LUMICE_ANALYTIC_DiagnosticSource* rows,
                                        size_t row_count, LUMICE_ANALYTIC_DiagnosticResult* out);
LUMICE_ANALYTIC_API LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_TraceDiagnosticInterface(
    const int* faces, int face_count, const LUMICE_ANALYTIC_DiagnosticSource* source, int slot, int reverse,
    int max_points, uint64_t max_evaluations, int budget_ms, LUMICE_ANALYTIC_DiagnosticResult* out);
LUMICE_ANALYTIC_API LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_TraceWeightedSkyField(
    const LUMICE_ANALYTIC_WeightedSkySample* samples, size_t count, const double seed[3], int equation, double level,
    double bandwidth_rad, int max_points, uint64_t max_evaluations, int budget_ms,
    LUMICE_ANALYTIC_DiagnosticResult* out);
LUMICE_ANALYTIC_API void LUMICE_ANALYTIC_ReleaseDiagnosticResult(LUMICE_ANALYTIC_DiagnosticResult* result);

// Returns only the processed input prefix, in order: optical_count may be smaller than row_count.
// termination: 0 = every row processed, 6 = shared evaluation/deadline budget exhausted.
// An interrupted row is retained with solve_status=6; the unprocessed tail is neither read nor
// materialized. Invalid rows within the prefix retain per-row status and do not stop later rows.
// Array/path validation precedes budget stopping, including when max_evaluations is zero.
LUMICE_ANALYTIC_API LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_CorrectDeviationBatch(
    const int* faces, int face_count, const LUMICE_ANALYTIC_DiagnosticSource* rows, size_t row_count,
    uint64_t max_evaluations, int budget_ms, LUMICE_ANALYTIC_DiagnosticResult* out);

// ---------------------------------------------------------------------------------------------
// Version 8: the u-S^2 field layer of one fixed face sequence (doc/analytic-api.md section 4.8).
// By the u-S^2 reduction (LI docs/phase2.md section 1) every geometric and optical weight of a
// path is a function of u alone — the sun direction in the crystal frame — at the identity pose.
// These calls publish that layer's objects: the deviation field's critical structure (kind-1
// critical values with their onset profiles, kind-2 the boundary dU_P walked once around, kind-3
// the weight kinks, the TIR onsets of the internal reflections), the delta-axis partition with
// its completeness certificate and its escape regimes, the per-wavelength critical table, the
// restricted family curve of an oriented density, and the two-index chromatic verdicts. The
// kernels are the C++ port of LI's dp_field / dp_boundary / dp_weight_kink / dp_partition /
// dp_focusing / dp_chromatic modules; JAX is the authority, these are derived implementations.
//
// Every call builds its field from scratch and keeps no state between calls (no handle: a handle
// freezes once published, and waits for a real consumer, doc/analytic-api.md section 9 item 15).
// Costs are lattice-scale: the walks seed from a 20000-point Fibonacci lattice, the partition
// adds a 20000-point topology lattice and (on a plural count) a chart-grid audit; each call is
// seconds at most and single-threaded. Deterministic: no random numbers anywhere on this layer
// (the plate-class tint's family sample is a seed-seeded mt19937_64 stream inside
// DiagnoseClassTint, so that call's result is fixed by its inputs including the seed; the stream
// is not LI's numpy stream — the parity caliber there is a tolerance, not bit equality).
//
// Error discipline (as everywhere on this surface): a bad crystal / face sequence / index /
// density / family / grid is a call error; every numerical outcome of a run computation — a
// boundary walk that refuses, a partition that escapes, kink arcs with failed seeds, routed
// non-finite points — is RESULT DATA in the fields below, never an error code. The two
// fail-closed mechanical invariants of the kernels hold at this surface and are pinned by the
// ABI tests: a refused boundary walk delivers an EMPTY loop (status != OK implies
// critical_point_count == 0 and corner_count == 0), and an escaped partition delivers NO
// intervals (escaped != 0 implies interval_count == 0).
//
// Open enumerations. WalkStatus and EscapeRegime are OPEN sets: the kernels refuse by regime and
// never silently merge one into another, and later versions may add values (conclusions section
// 4 item 5 requires the escapes open for the report side's fail-closed consumption). A caller
// must handle an unknown value inside a known group; each result therefore carries the value's
// stable slug string next to it (the kernels' own name tables — report-side grep compatibility).
// The number segments are this layer's own and deliberately disjoint from LUMICE_ANALYTIC_Reason's
// (0 / 100+ / 200+ / 300+). Every other enumeration below is CLOSED: LI's frozen vocabulary,
// pinned by the parity fixtures.
// ---------------------------------------------------------------------------------------------

// How a boundary walk (TraceBoundaryLoop, and the walks inside the other calls) ends. kOK is the
// closed loop — the completeness certificate itself; every other value is a named truncation or
// refusal with LI's message text. OPEN set.
typedef enum LUMICE_ANALYTIC_WalkStatus_ {
  LUMICE_ANALYTIC_WALK_OK = 0,
  LUMICE_ANALYTIC_WALK_STEPS_EXHAUSTED = 1,    // step budget without a corner or a closure
  LUMICE_ANALYTIC_WALK_START_NO_POINT = 2,     // U_P has no point on the lattice
  LUMICE_ANALYTIC_WALK_START_COVERS_ALL = 3,   // U_P covers the whole lattice: no boundary
  LUMICE_ANALYTIC_WALK_START_NO_EDGE = 4,      // no lattice point next to the boundary
  LUMICE_ANALYTIC_WALK_CORNER_NOT_SIMPLE = 5,  // a corner's outgoing margin is not unique
  LUMICE_ANALYTIC_WALK_NOT_CLOSED = 6,         // pieces without closing
  LUMICE_ANALYTIC_WALK_NOT_FINITE = 7,         // the field off the closure of U_P (fail closed)
  LUMICE_ANALYTIC_WALK_BAD_ORIENTATION = 8,
} LUMICE_ANALYTIC_WalkStatus;

// Why the partition reasoning refused (LUMICE_ANALYTIC_PartitionResult.escaped): every regime the
// simple disk / single-extremum reasoning does not cover, one value each, none ever resolved
// silently. OPEN set.
typedef enum LUMICE_ANALYTIC_EscapeRegime_ {
  LUMICE_ANALYTIC_ESCAPE_NOT_DISK_UNAUDITED = 0,
  LUMICE_ANALYTIC_ESCAPE_NOT_DISK_UNCONVERGED = 1,
  LUMICE_ANALYTIC_ESCAPE_NOT_DISK_CONFIRMED = 2,
  LUMICE_ANALYTIC_ESCAPE_NOT_DISK_CORRECTED = 3,
  LUMICE_ANALYTIC_ESCAPE_SLAB_CREASE_CONTRADICTION = 4,
  LUMICE_ANALYTIC_ESCAPE_SLAB_CREASE_NOT_CARRIED = 5,
  LUMICE_ANALYTIC_ESCAPE_SLAB_CREASE_TOUCHING = 6,
  LUMICE_ANALYTIC_ESCAPE_SLAB_CREASE_CLOSED_RIDGE = 7,
  LUMICE_ANALYTIC_ESCAPE_MULTIPLE_INTERIOR_CRITICAL_POINTS = 8,
  LUMICE_ANALYTIC_ESCAPE_LOOP_EXTREMA_NOT_ALTERNATING = 9,
  LUMICE_ANALYTIC_ESCAPE_ODD_BOUNDARY_CROSSINGS = 10,
  LUMICE_ANALYTIC_ESCAPE_INTERIOR_CRITICAL_POINT_NOT_SIMPLE = 11,
  LUMICE_ANALYTIC_ESCAPE_SUBLEVEL_NOT_REACHING_BOUNDARY = 12,
} LUMICE_ANALYTIC_EscapeRegime;

// Morse kind of a critical point. CLOSED set (LI's vocabulary).
typedef enum LUMICE_ANALYTIC_CriticalKind_ {
  LUMICE_ANALYTIC_CRITICAL_MINIMUM = 0,
  LUMICE_ANALYTIC_CRITICAL_MAXIMUM = 1,
  LUMICE_ANALYTIC_CRITICAL_SADDLE = 2,
  LUMICE_ANALYTIC_CRITICAL_DEGENERATE = 3,
} LUMICE_ANALYTIC_CriticalKind;

// Where a critical onset sits and what produced it. CLOSED set (LI's vocabulary, the parity
// fixtures pin the slugs "interior"/"boundary", "interior_minimum"/..., "finite_jump"/...).
typedef enum LUMICE_ANALYTIC_OnsetLocation_ {
  LUMICE_ANALYTIC_ONSET_INTERIOR = 0,
  LUMICE_ANALYTIC_ONSET_BOUNDARY = 1,
} LUMICE_ANALYTIC_OnsetLocation;

typedef enum LUMICE_ANALYTIC_OnsetSource_ {
  LUMICE_ANALYTIC_ONSET_INTERIOR_MINIMUM = 0,
  LUMICE_ANALYTIC_ONSET_INTERIOR_MAXIMUM = 1,
  LUMICE_ANALYTIC_ONSET_INTERIOR_SADDLE = 2,
  LUMICE_ANALYTIC_ONSET_INTERIOR_DEGENERATE = 3,
  LUMICE_ANALYTIC_ONSET_SLAB_AXIS = 4,
  LUMICE_ANALYTIC_ONSET_SLAB_CIRCLE = 5,
  LUMICE_ANALYTIC_ONSET_BOUNDARY_EXTREMUM = 6,
  LUMICE_ANALYTIC_ONSET_CORNER = 7,
} LUMICE_ANALYTIC_OnsetSource;

typedef enum LUMICE_ANALYTIC_OnsetProfile_ {
  LUMICE_ANALYTIC_PROFILE_FINITE_JUMP = 0,
  LUMICE_ANALYTIC_PROFILE_LOG_DIVERGENCE = 1,
  LUMICE_ANALYTIC_PROFILE_INVERSE_SQRT_DIVERGENCE = 2,
  LUMICE_ANALYTIC_PROFILE_CONE_POINT = 3,
  LUMICE_ANALYTIC_PROFILE_CREASE = 4,
  LUMICE_ANALYTIC_PROFILE_BOUNDARY_ONSET = 5,
  LUMICE_ANALYTIC_PROFILE_DEGENERATE = 6,
} LUMICE_ANALYTIC_OnsetProfile;

// Which way a weight-kink curve was found — the completeness declaration's first half: the
// closed-form circle is complete by construction, the marched arcs are structurally not a
// completeness claim. CLOSED set.
typedef enum LUMICE_ANALYTIC_KinkCoverage_ {
  LUMICE_ANALYTIC_KINK_CLOSED_FORM_AUTHORITY = 0,
  LUMICE_ANALYTIC_KINK_MARCHED_UNCERTIFIED = 1,
} LUMICE_ANALYTIC_KinkCoverage;

// A curve object's existence state (the measure contract's vocabulary; TraceRestrictedFamilyCurve
// emits kComputed — an empty curve with its reason in `note` when the density has no family axis
// or the sun sits at the axis pole). CLOSED set.
typedef enum LUMICE_ANALYTIC_CurveExistence_ {
  LUMICE_ANALYTIC_EXISTENCE_COMPUTED = 0,
  LUMICE_ANALYTIC_EXISTENCE_ESCAPED = 1,
  LUMICE_ANALYTIC_EXISTENCE_WALK_TRUNCATED = 2,
  LUMICE_ANALYTIC_EXISTENCE_S4_DECLARED = 3,
} LUMICE_ANALYTIC_CurveExistence;

// Chromatic vocabulary. CLOSED set (LI's, the parity fixtures pin the slugs).
typedef enum LUMICE_ANALYTIC_ChromaticFeatureKind_ {
  LUMICE_ANALYTIC_CHROMATIC_EDGE = 0,       // a weight kink C_k
  LUMICE_ANALYTIC_CHROMATIC_GATE_EDGE = 1,  // a gate of U_P that moves with n
} LUMICE_ANALYTIC_ChromaticFeatureKind;

typedef enum LUMICE_ANALYTIC_ChromaticColor_ {
  LUMICE_ANALYTIC_COLOR_BLUE = 0,
  LUMICE_ANALYTIC_COLOR_RED = 1,
  LUMICE_ANALYTIC_COLOR_WHITE = 2,
  LUMICE_ANALYTIC_COLOR_NONE = 3,
} LUMICE_ANALYTIC_ChromaticColor;

typedef enum LUMICE_ANALYTIC_ChromaticVerdictKind_ {
  LUMICE_ANALYTIC_VERDICT_EDGE = 0,
  LUMICE_ANALYTIC_VERDICT_GATE_EDGE = 1,
  LUMICE_ANALYTIC_VERDICT_TINT = 2,
  LUMICE_ANALYTIC_VERDICT_UNRESOLVED = 3,
  LUMICE_ANALYTIC_VERDICT_NONE = 4,
} LUMICE_ANALYTIC_ChromaticVerdictKind;

// One critical value of the deviation field and the profile it produces for a random orientation.
// value / measure_limit in radians; measure_limit (the limit of the level-set measure
// int dl / |grad D|) exists only for a finite_jump. gradient_norm is |grad D_P| at the point —
// infinity at an exit-TIR end of the boundary, where the gradient is unbounded.
typedef struct LUMICE_ANALYTIC_CriticalOnset_ {
  double value;
  int location;  // LUMICE_ANALYTIC_OnsetLocation
  int source;    // LUMICE_ANALYTIC_OnsetSource
  int profile;   // LUMICE_ANALYTIC_OnsetProfile
  double gradient_norm;
  int has_measure_limit;
  double measure_limit;
  int multiplicity;  // critical points merged into this record (same value, location, source,
                     // profile); the record keeps the smallest gradient_norm
} LUMICE_ANALYTIC_CriticalOnset;

// One interval of the delta axis with the constant counts of {D_P = delta} for
// lower < delta < upper: n_components = n_closed + n_open always.
typedef struct LUMICE_ANALYTIC_DeviationInterval_ {
  double lower;
  double upper;
  int n_components;
  int n_closed;
  int n_open;
} LUMICE_ANALYTIC_DeviationInterval;

// dU_P walked once around: the completeness certificate's kind-2 object. On status OK the loop
// holds its restricted critical points in WALK ORDER (the partition's alternation check is an
// order property), its corners, the walk's first sample point, and either an isolated-extremum
// loop or a plateau (a loop of constant D_P — critical_point_count 0, has_plateau 1). On any
// other status the loop is EMPTY: no half-walk is ever delivered. All arrays live in `storage`
// until LUMICE_ANALYTIC_ReleaseBoundaryLoopResult.
typedef struct LUMICE_ANALYTIC_BoundaryLoopResult_ {
  uint32_t struct_size;     // caller sets sizeof(*out) before the call (section 8.2)
  int status;               // LUMICE_ANALYTIC_WalkStatus (open)
  const char* status_name;  // the status's slug ("ok", "steps_exhausted", ...); NULL when status
                            // is a value this library does not know
  const char* message;      // the refusal's LI text; NULL when status is OK
  int critical_point_count;
  const double* critical_point_positions;     // 3N, unit u
  const double* critical_point_values;        // N, radians
  const int* critical_point_kinds;            // N, LUMICE_ANALYTIC_CriticalKind
  const unsigned char* critical_point_flags;  // N: bit 0 strict, bit 1 corner
  int corner_count;
  const double* corner_positions;  // 3N, in walk order
  const double* corner_values;     // N, radians
  int has_plateau;                 // 1 on a constant loop
  double plateau_value;            // radians; meaningful when has_plateau
  double first_point[3];           // the walk's first sample point (unit u)
  void* storage;                   // opaque; LUMICE_ANALYTIC_ReleaseBoundaryLoopResult
} LUMICE_ANALYTIC_BoundaryLoopResult;

// One arc of a weight kink: a header into the flattened point arrays of
// LUMICE_ANALYTIC_WeightKinksResult (successive complete groups, the DiagnosticResult shape).
typedef struct LUMICE_ANALYTIC_WeightKinkArc_ {
  int first_point;  // index into arc_points / arc_values
  int point_count;
  int closed;       // a whole loop inside U_P (end_gate then {-1, -1})
  int end_gate[2];  // the gate margin index stopping each end; -1 where no gate does
} LUMICE_ANALYTIC_WeightKinkArc;

// The TIR onset curve C_k of one internal reflection: one row per internal step, in step order
// (no rows for an entry-exit path). Array elements are layout-frozen (section 8.2): arc POINTS
// live in the result's flattened arrays, never in this row. spread is the width max - min of
// D_P on C_k (NaN without arcs, 0 on a single-mirror slab); note carries the curve's own
// verdict text (an onset off S^2, the first refused seed's message); failed_seeds counts the
// marched seeds whose walk refused — no failure is ever silent, and zero failures is still NOT
// a completeness certificate for a marched curve (the coverage field says which way it was
// found).
typedef struct LUMICE_ANALYTIC_WeightKinkCurve_ {
  int step;          // 1-based internal step k
  int margin;        // the step's TIR-discriminant margin index
  double index;      // the call's refractive index
  int coverage;      // LUMICE_ANALYTIC_KinkCoverage
  int has_normal;    // closed form only
  double normal[3];  // the unfolded incidence normal m_k, unit
  int failed_seeds;
  int status;  // LUMICE_ANALYTIC_WalkStatus (open): the closed form's own refusal;
               // a marched seed's refusal is counted, never set here
  const char* status_name;
  const char* note;  // "" when there is none
  double spread;
  int first_arc;  // index into the result's arc array
  int arc_count;
} LUMICE_ANALYTIC_WeightKinkCurve;

typedef struct LUMICE_ANALYTIC_WeightKinksResult_ {
  uint32_t struct_size;  // caller sets sizeof(*out) before the call (section 8.2)
  int curve_count;
  const LUMICE_ANALYTIC_WeightKinkCurve* curves;  // curve_count rows, step order
  int arc_count;
  const LUMICE_ANALYTIC_WeightKinkArc* arcs;  // arc_count headers, curve then arc order
  int arc_point_count;
  const double* arc_points;  // 3 * arc_point_count, unit u, arcs concatenated in header order
  const double* arc_values;  // arc_point_count, D_P in radians at each point
  void* storage;             // opaque; LUMICE_ANALYTIC_ReleaseWeightKinksResult
} LUMICE_ANALYTIC_WeightKinksResult;

// The kind-1 focusing label of one face sequence under one pose density: every critical value's
// onset, the sampled gradient-norm range, the dimensions the density confines and whether the
// density's sigma->0 family lies in one level set (family_pinned). `mechanism` is the slug of
// the roll-up ("point_mass", "none", "jacobian", "dimension_collapse",
// "jacobian+dimension_collapse"). A refused boundary walk is DATA: escaped 1 with the walk's
// status, and NO onsets.
typedef struct LUMICE_ANALYTIC_ClassificationResult_ {
  uint32_t struct_size;  // caller sets sizeof(*out) before the call (section 8.2)
  int halo_map_rank;     // 0 (a point mass) or 2
  int escaped;
  int escape_status;  // LUMICE_ANALYTIC_WalkStatus when escaped
  const char* escape_status_name;
  const char* escape_message;  // NULL when not escaped
  int onset_count;
  const LUMICE_ANALYTIC_CriticalOnset* onsets;  // value order
  int has_gradient_norm_range;                  // a sampled bound, not a proof
  double gradient_norm_range[2];
  int confined_dimensions;  // the pose dimensions the density confines
  int confined_width_count;
  const double* confined_widths_rad;  // confined_width_count entries
  int family_pinned;
  const char* mechanism;  // the roll-up slug; never NULL on success
  void* storage;          // opaque; LUMICE_ANALYTIC_ReleaseClassificationResult
} LUMICE_ANALYTIC_ClassificationResult;

// The delta-axis partition of D_P on U_P with the counts of every interval — the completeness
// certificate. Two refusal layers, both data and both leaving NO intervals (mechanical
// invariants, pinned at this surface): walk_status != OK — the boundary loop itself was refused,
// so there is no loop to partition (message is the walk's text); escaped != 0 — the loop was
// walked but the reasoning refused, with the regime named openly and message carrying the
// failing check's LI text. The topology evidence is the adjudicated component counts of U_P and
// its complement on the lattice, with the chart audit's verdict slug ("confirmed", "corrected",
// "unconverged"; "" when no audit ran — a disk needs none).
typedef struct LUMICE_ANALYTIC_PartitionResult_ {
  uint32_t struct_size;  // caller sets sizeof(*out) before the call (section 8.2)
  int walk_status;       // LUMICE_ANALYTIC_WalkStatus (open)
  const char* walk_status_name;
  const char* walk_message;  // NULL when the walk closed
  int escaped;
  int regime;               // LUMICE_ANALYTIC_EscapeRegime (open); meaningful when escaped
  const char* regime_name;  // the regime's slug; NULL when not escaped or unknown
  const char* message;      // the refusal's LI text; NULL when not escaped
  int interval_count;
  const LUMICE_ANALYTIC_DeviationInterval* intervals;  // ascending, adjacent (upper == next lower)
  int lattice_n;
  int domain_components;
  int complement_components;
  const char* audit_verdict;
  void* storage;  // opaque; LUMICE_ANALYTIC_ReleasePartitionResult
} LUMICE_ANALYTIC_PartitionResult;

// One onset followed across refractive indices, aligned by rank: values_deg parallels the
// call's labels (values_deg[first_value + k] at labels[k]); displacement_deg is max - min.
typedef struct LUMICE_ANALYTIC_WavelengthOnsetRow_ {
  int location;  // LUMICE_ANALYTIC_OnsetLocation
  int source;    // LUMICE_ANALYTIC_OnsetSource
  int profile;   // LUMICE_ANALYTIC_OnsetProfile
  int jacobian_focusing;
  int first_value;  // index into the result's values_deg
  double displacement_deg;
} LUMICE_ANALYTIC_WavelengthOnsetRow;

// The critical table across refractive indices. Rank pairing is accepted only when every index
// carries the same onset count and every rank the same (location, source, profile) at every
// index; a topology that changes with n is DATA (escaped 1, message names the rank), following
// it is not this layer's job. The call's labels and indices are echoed back so the result is
// self-contained.
typedef struct LUMICE_ANALYTIC_WavelengthTableResult_ {
  uint32_t struct_size;  // caller sets sizeof(*out) before the call (section 8.2)
  int escaped;
  const char* message;  // NULL when not escaped
  int label_count;
  const char* const* labels;  // label_count entries, the call's labels
  const double* indices;      // label_count entries, the call's indices
  int onset_count;
  const LUMICE_ANALYTIC_WavelengthOnsetRow* onsets;  // value order at the first index
  const double* values_deg;                          // onset_count * label_count, row-major (onset, label)
  void* storage;                                     // opaque; LUMICE_ANALYTIC_ReleaseWavelengthTableResult
} LUMICE_ANALYTIC_WavelengthTableResult;

// The restricted family curve: the circle of u about the density's family axis at the sun's
// polar angle, with D_P sampled along it at base_index and re-sampled per wavelength
// (n-continuation; the index table is the caller's). Existence is kComputed on success; a
// density without a family axis, or a sun at the axis pole, is an EMPTY curve (point_count 0)
// with the reason in `note` — a verdict, not an error. routed_nonfinite counts the points whose
// routed D_P is NaN even under the exit-Snell closure routing: the circle is sampled where the
// field does not exist, which is data, never a silent zero.
typedef struct LUMICE_ANALYTIC_RestrictedCurveResult_ {
  uint32_t struct_size;   // caller sets sizeof(*out) before the call (section 8.2)
  int closed;             // always 1 when point_count > 0 (the latitude circle is a loop)
  int existence;          // LUMICE_ANALYTIC_CurveExistence
  const char* note;       // "" when there is none
  int point_count;        // the call's grid
  const double* u;        // 3N, unit
  const double* tangent;  // 3N, unit, the theta-derivative of the circle
  const double* d_p;      // N, radians, routed, at base_index
  int wavelength_count;
  const double* wavelengths_nm;
  const double* indices;
  const double* critical_d_p;   // N * wavelength_count, row-major (point, wavelength); NaN where
                                // routed_nonfinite counts
  const double* support_param;  // N, theta in [0, 2 pi)
  int routed_nonfinite;
  void* storage;  // opaque; LUMICE_ANALYTIC_ReleaseRestrictedCurveResult
} LUMICE_ANALYTIC_RestrictedCurveResult;

// The declared chromatic thresholds that produced a verdict — a snapshot of the criterion's
// parameters, not constants of nature; paired with the verdict so a consumer reads which
// declared criterion labelled it.
typedef struct LUMICE_ANALYTIC_ChromaticThresholds_ {
  double n_red;
  double n_blue;
  double edge_min_shift_rad;
  double edge_spread_per_shift;
  double calibration_white_max_deviation;
  double tint_ratio_min;
} LUMICE_ANALYTIC_ChromaticThresholds;

// One colour source of a path: a weight kink (EDGE) or a moving gate of U_P (GATE_EDGE), with
// the two-index metrics at its line. source is the margin's name ("internal_1_tir_discriminant",
// "exit_snell_discriminant", ...). Angles in radians.
typedef struct LUMICE_ANALYTIC_ChromaticFeature_ {
  int kind;  // LUMICE_ANALYTIC_ChromaticFeatureKind
  const char* source;
  int color;  // LUMICE_ANALYTIC_ChromaticColor
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
} LUMICE_ANALYTIC_ChromaticFeature;

// A chromatic verdict: the dominant feature's kind / color / visible / position (random
// orientation), or the plate-class tint roll-up; every assessed feature, the notes (never a
// silent NONE), the threshold snapshot, and — class verdicts only — the class's members and the
// members lit at each index. For DiagnoseChromatic the class suffix is empty (member_count 0,
// has_tint 0); a rank-0 path is not a chromatic subject at all — the caller labels it
// point_mass from ClassifyCriticalStructure.
typedef struct LUMICE_ANALYTIC_ChromaticResult_ {
  uint32_t struct_size;  // caller sets sizeof(*out) before the call (section 8.2)
  int verdict_kind;      // LUMICE_ANALYTIC_ChromaticVerdictKind
  int color;             // LUMICE_ANALYTIC_ChromaticColor
  int visible;
  int has_position;
  double position;        // radians; meaningful when has_position
  int coverage_complete;  // 0 when a line the verdict rests on was not fully analysed; the notes
                          // say which
  int feature_count;
  const LUMICE_ANALYTIC_ChromaticFeature* features;
  int note_count;
  const char* const* notes;
  LUMICE_ANALYTIC_ChromaticThresholds thresholds;
  // Class verdict suffix (DiagnoseClassTint): members as flattened face sequences, the counts
  // per member in the sizes arrays; lit_* are the members lit at each index (subset indices
  // into members, ascending). Empty for DiagnoseChromatic.
  int member_count;
  const int* member_sizes;  // member_count face counts
  const int* members;       // sum(member_sizes) face numbers
  int lit_red_count;
  const int* lit_sizes_red;  // lit_red_count member indices
  const int* lit_members_red;
  int lit_blue_count;
  const int* lit_sizes_blue;
  const int* lit_members_blue;
  int has_tint;
  double energy_red;  // the tint metrics below are meaningful when has_tint
  double energy_blue;
  double ratio;  // NaN where its denominator vanishes
  double tir_fraction_red;
  double tir_fraction_blue;
  double tint_direction_dispersion;  // NaN where no pose is lit at both indices
  void* storage;                     // opaque; LUMICE_ANALYTIC_ReleaseChromaticResult
} LUMICE_ANALYTIC_ChromaticResult;

// The plate family of the class tint (DiagnoseClassTint): poses with the c axis tilted from the
// zenith by |N(0, zenith_std_deg)| in a uniform direction, uniform spin, lit by a sun at
// sun_altitude_deg. samples is the family sample size, 1..10000000 (each pose is evaluated in
// full); seed seeds the call's deterministic sample stream.
typedef struct LUMICE_ANALYTIC_PlateFamily_ {
  double sun_altitude_deg;
  double zenith_std_deg;
  int samples;
  unsigned long long seed;
} LUMICE_ANALYTIC_PlateFamily;

// Walks dU_P once around: the boundary loop of the valid domain, its corners, and the restricted
// extrema of D_P along it (kind-2). One closed walk is the completeness claim; every refusal is
// a named status with an empty loop.
//
// Call errors (out is zero-filled after struct_size, so Release is safe):
//   ERR_NULL_ARG       crystal, faces or out is NULL
//   ERR_INVALID_VALUE  out->struct_size smaller than this struct; face_count < 2 or > 64; a face
//                      number the crystal does not have; refractive_index not finite and positive;
//                      a crystal field as for EvaluatePath
//   ERR_INVALID_CONFIG crystal rejected by the engine's closed-form validity gate
//   ERR_UNKNOWN        an internal failure (out of memory); out is zero-filled
// Re-entrant: safe to call concurrently on distinct outputs; keeps no state between calls.
LUMICE_ANALYTIC_API LUMICE_ANALYTIC_ErrorCode
LUMICE_ANALYTIC_TraceBoundaryLoop(const LUMICE_ANALYTIC_Crystal* crystal, const int* faces, int face_count,
                                  double refractive_index, LUMICE_ANALYTIC_BoundaryLoopResult* out);

// Frees what TraceBoundaryLoop allocated and zeroes the struct after struct_size. NULL-safe; a
// no-op on a zero-filled struct.
LUMICE_ANALYTIC_API void LUMICE_ANALYTIC_ReleaseBoundaryLoopResult(LUMICE_ANALYTIC_BoundaryLoopResult* result);

// Traces the weight kinks: the TIR onset curve C_k of every internal reflection, closed form
// where the incidence normal passes the check, marched otherwise (kind-3). An entry-exit path
// has no internal reflection and succeeds with curve_count 0.
//
// Call errors: as TraceBoundaryLoop. Re-entrant on distinct outputs.
LUMICE_ANALYTIC_API LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_TraceWeightKinks(const LUMICE_ANALYTIC_Crystal* crystal,
                                                                               const int* faces, int face_count,
                                                                               double refractive_index,
                                                                               LUMICE_ANALYTIC_WeightKinksResult* out);

// Frees what TraceWeightKinks allocated and zeroes the struct after struct_size. NULL-safe.
LUMICE_ANALYTIC_API void LUMICE_ANALYTIC_ReleaseWeightKinksResult(LUMICE_ANALYTIC_WeightKinksResult* result);

// Classifies the critical structure of one face sequence under one pose density (kind-1): every
// critical onset with its profile, the gradient-norm range, the density's confined dimensions
// and the family_pinned label, rolled up into the mechanism slug. The density is validated like
// BandSum's (an unknown family, a missing or non-positive width, a zenith mean outside [0, 180]
// or a used-but-non-zero field is ERR_INVALID_VALUE).
//
// Call errors: as TraceBoundaryLoop, plus ERR_INVALID_VALUE for an invalid density.
// Re-entrant on distinct outputs.
LUMICE_ANALYTIC_API LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_ClassifyCriticalStructure(
    const LUMICE_ANALYTIC_Crystal* crystal, const int* faces, int face_count, double refractive_index,
    LUMICE_ANALYTIC_PoseDensity density, LUMICE_ANALYTIC_ClassificationResult* out);

// Frees what ClassifyCriticalStructure allocated and zeroes the struct after struct_size.
// NULL-safe.
LUMICE_ANALYTIC_API void LUMICE_ANALYTIC_ReleaseClassificationResult(LUMICE_ANALYTIC_ClassificationResult* result);

// Partitions the deviation axis: the intervals of [min D_P, max D_P] with the component counts
// of each level set, the completeness certificate of the field layer. A refused certificate is
// an escape — data, with the regime named openly and no intervals.
//
// Call errors: as TraceBoundaryLoop. Re-entrant on distinct outputs.
LUMICE_ANALYTIC_API LUMICE_ANALYTIC_ErrorCode
LUMICE_ANALYTIC_PartitionDeviationAxis(const LUMICE_ANALYTIC_Crystal* crystal, const int* faces, int face_count,
                                       double refractive_index, LUMICE_ANALYTIC_PartitionResult* out);

// Frees what PartitionDeviationAxis allocated and zeroes the struct after struct_size. NULL-safe.
LUMICE_ANALYTIC_API void LUMICE_ANALYTIC_ReleasePartitionResult(LUMICE_ANALYTIC_PartitionResult* result);

// Follows every onset across refractive indices, aligned by rank: the per-wavelength critical
// table (n-continuation; n(lambda) is the caller's). A topology that changes with n escapes as
// data rather than pairing unrelated onsets. `count` labels/indices; an empty set escapes
// ("indices is empty") rather than erroring — the kernel's own verdict.
//
// Call errors: as TraceBoundaryLoop, plus ERR_NULL_ARG for labels or indices NULL with
// count > 0, and ERR_INVALID_VALUE for count < 0 or a non-finite / non-positive index.
// Re-entrant on distinct outputs.
LUMICE_ANALYTIC_API LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_TraceWavelengthCriticalTable(
    const LUMICE_ANALYTIC_Crystal* crystal, const int* faces, int face_count, LUMICE_ANALYTIC_PoseDensity density,
    const char* const* labels, const double* indices, int count, LUMICE_ANALYTIC_WavelengthTableResult* out);

// Frees what TraceWavelengthCriticalTable allocated and zeroes the struct after struct_size.
// NULL-safe.
LUMICE_ANALYTIC_API void LUMICE_ANALYTIC_ReleaseWavelengthTableResult(LUMICE_ANALYTIC_WavelengthTableResult* result);

// Samples the restricted family curve of one face sequence under one pose density: the circle of
// u about the family axis at the sun's polar angle, D_P along it at base_index and per
// wavelength (kind-1, restricted). grid is the point count, >= 1; sun_hat is the unit direction
// AT the sun; the wavelength tables are parallel arrays of wavelength_count entries (0 legal:
// no chromatic rows).
//
// Call errors: as TraceBoundaryLoop, plus ERR_NULL_ARG for sun_hat NULL or a wavelength table
// NULL with wavelength_count > 0, and ERR_INVALID_VALUE for grid < 1, wavelength_count < 0, a
// non-finite / non-positive base_index or wavelength index, or a non-finite / non-unit sun_hat.
// An empty curve (no family axis, sun at the pole) is a SUCCESS with the reason in note.
// Re-entrant on distinct outputs.
LUMICE_ANALYTIC_API LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_TraceRestrictedFamilyCurve(
    const LUMICE_ANALYTIC_Crystal* crystal, const int* faces, int face_count, LUMICE_ANALYTIC_PoseDensity density,
    const double sun_hat[3], double base_index, const double* wavelengths_nm, const double* indices,
    int wavelength_count, int grid, LUMICE_ANALYTIC_RestrictedCurveResult* out);

// Frees what TraceRestrictedFamilyCurve allocated and zeroes the struct after struct_size.
// NULL-safe.
LUMICE_ANALYTIC_API void LUMICE_ANALYTIC_ReleaseRestrictedCurveResult(LUMICE_ANALYTIC_RestrictedCurveResult* result);

// The random-orientation colour verdict of one face sequence: every weight kink and every moving
// gate of U_P between n_red and n_blue as a feature, the dominant one the verdict. The class
// suffix of the result is empty. Rank-0 paths are not a chromatic subject (label them
// point_mass from ClassifyCriticalStructure).
//
// Call errors: as TraceBoundaryLoop, plus ERR_INVALID_VALUE for n_red or n_blue not finite and
// positive. Re-entrant on distinct outputs.
LUMICE_ANALYTIC_API LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_DiagnoseChromatic(const LUMICE_ANALYTIC_Crystal* crystal,
                                                                                const int* faces, int face_count,
                                                                                double n_red, double n_blue,
                                                                                LUMICE_ANALYTIC_ChromaticResult* out);

// The plate-class tint verdict: the reflection class of `faces` under a plate family, its
// members, the members lit at each index, the tint metrics over the family sample and the tint
// verdict of the blue/red ratio. The family sample is seeded (deterministic per seed; not LI's
// numpy stream — the parity caliber is a tolerance).
//
// Call errors: as DiagnoseChromatic, plus ERR_INVALID_VALUE for zenith_std_deg not finite and
// positive, sun_altitude_deg not finite, or samples outside 1..10000000. Re-entrant on distinct
// outputs.
LUMICE_ANALYTIC_API LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_DiagnoseClassTint(const LUMICE_ANALYTIC_Crystal* crystal,
                                                                                const int* faces, int face_count,
                                                                                LUMICE_ANALYTIC_PlateFamily family,
                                                                                double n_red, double n_blue,
                                                                                LUMICE_ANALYTIC_ChromaticResult* out);

// Frees what DiagnoseChromatic / DiagnoseClassTint allocated and zeroes the struct after
// struct_size. NULL-safe.
LUMICE_ANALYTIC_API void LUMICE_ANALYTIC_ReleaseChromaticResult(LUMICE_ANALYTIC_ChromaticResult* result);

#ifdef __cplusplus
}
#endif

#endif  // LUMICE_ANALYTIC_CORE_H_
