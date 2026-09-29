#ifndef LUMICE_ANALYTIC_H_
#define LUMICE_ANALYTIC_H_

// The published analytic interface of Lumice (liblumice_analytic). Design: doc/analytic-api.md.
//
// Independent of lumice.h: it includes nothing from it and shares none of its types. The library
// is built from the same engine objects as liblumice, but exports exactly the LUMICE_ANALYTIC_*
// functions declared here — its export list is generated from this header by
// scripts/gen_export_list.py (root CMakeLists.txt, lumice_apply_export_list). A process loads one
// of liblumice / liblumice_testapi / liblumice_analytic, never two: each carries its own copy of
// the engine and its statics (doc/analytic-api.md section 2.6).
//
// 0.x is experimental: the interface may change between versions (doc/analytic-api.md section 8).
//
// Conventions: frames, face numbers and the pose chain are those of doc/coordinate-convention.md;
// this header passes them, it does not define them. Symmetry: every function takes one concrete
// face sequence and performs no symmetry reduction (doc/analytic-api.md section 3).
//
// Version notes, newest first (every bump says what changed, doc/analytic-api.md section 8.1):
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
// incompatible change, and in 0.x on every addition too. Independent of lumice.h's LUMICE_API_VERSION.
#define LUMICE_ANALYTIC_API_VERSION 5

// Library version at run time; compare with LUMICE_ANALYTIC_API_VERSION to detect a
// header/library mismatch. Also the minimal function the build, export and load chain is proven
// with before any module exists.
LUMICE_ANALYTIC_API int LUMICE_ANALYTIC_GetApiVersion(void);

// Return codes of the computation functions. The names shared with lumice.h's LUMICE_ErrorCode mean
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

// Logging: the library writes nothing by default — no console, no file — until the host installs a
// callback, which then receives the engine's diagnostics, including crystal-construction warnings
// (doc/analytic-api.md section 6). This differs from lumice.h's LUMICE_SetLogCallback, which only
// adds a destination next to a console sink the host cannot remove.
//
// The level values match spdlog's six levels, which is what the engine's messages carry; a
// LOG_WARNING in the engine arrives as LUMICE_ANALYTIC_LOG_WARNING.
typedef enum LUMICE_ANALYTIC_LogLevel_ {
  LUMICE_ANALYTIC_LOG_TRACE = 0,
  LUMICE_ANALYTIC_LOG_DEBUG,
  LUMICE_ANALYTIC_LOG_VERBOSE,
  LUMICE_ANALYTIC_LOG_INFO,
  LUMICE_ANALYTIC_LOG_WARNING,
  LUMICE_ANALYTIC_LOG_ERROR,
} LUMICE_ANALYTIC_LogLevel;

// `message` is the formatted line without a trailing newline. Both strings are valid only for the
// duration of the call. The callback may be invoked from any thread that calls into this library.
typedef void (*LUMICE_ANALYTIC_LogCallback)(LUMICE_ANALYTIC_LogLevel level, const char* logger_name,
                                            const char* message);

// Installs `callback` as the one receiver of the library's diagnostics, replacing any previous one.
// A non-NULL callback logs one LUMICE_ANALYTIC_LOG_INFO line, "log callback installed", through
// itself so the host can see the wiring work; this is not a one-time event — every call with a
// non-NULL callback logs it again, including a call that merely replaces an already-installed one.
// NULL stops forwarding; the library is then silent again. An initialisation call: make it before
// any computation, from one thread (doc/analytic-api.md section 5.3). Do not call this function
// again from inside `callback` itself — the sink holds a non-recursive mutex across the callback
// invocation, and a reentrant call would deadlock (the same known limitation as lumice.h's
// LUMICE_SetLogCallback).
LUMICE_ANALYTIC_API void LUMICE_ANALYTIC_SetLogCallback(LUMICE_ANALYTIC_LogCallback callback);

#ifdef __cplusplus
}
#endif

#endif  // LUMICE_ANALYTIC_H_
