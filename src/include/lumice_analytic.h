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

// Interface version, a single integer bumped on every incompatible change (doc/analytic-api.md
// section 8). Independent of lumice.h's LUMICE_API_VERSION.
#define LUMICE_ANALYTIC_API_VERSION 2

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
//   ERR_INVALID_VALUE  out->struct_size smaller than this struct; face_count < 2; a face number the
//                      crystal does not have (unknown, or absent from this crystal's shape);
//                      refractive_index not finite and positive; incident_direction not finite or
//                      its length off 1 by more than 1e-10; pose not finite, R^T R off I by more
//                      than 1e-10 in any entry, or det(R) <= 0; a crystal field as described above
//   ERR_INVALID_CONFIG crystal rejected by the engine's closed-form validity gate
// Re-entrant: safe to call concurrently on distinct outputs; keeps no state between calls.
LUMICE_ANALYTIC_API LUMICE_ANALYTIC_ErrorCode LUMICE_ANALYTIC_EvaluatePath(
    const LUMICE_ANALYTIC_Crystal* crystal, const int* faces, int face_count, double refractive_index,
    const double incident_direction[3], const double pose[9], LUMICE_ANALYTIC_PathEvaluation* out);

// Frees what EvaluatePath allocated and zeroes the struct after struct_size. NULL-safe; a no-op on a
// zero-filled struct.
LUMICE_ANALYTIC_API void LUMICE_ANALYTIC_ReleasePathEvaluation(LUMICE_ANALYTIC_PathEvaluation* eval);

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
