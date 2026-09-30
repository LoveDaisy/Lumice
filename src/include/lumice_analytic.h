#ifndef LUMICE_ANALYTIC_H_
#define LUMICE_ANALYTIC_H_

// The published analytic interface of Lumice (liblumice_analytic). Design: doc/analytic-api.md.
//
// The whole interface: the capability (types and computation functions, lumice_analytic_core.h,
// which also holds LUMICE_ANALYTIC_API_VERSION and the version notes) plus the two functions that
// manage liblumice_analytic itself, declared below. Those two are exported by liblumice_analytic
// only; the engine libraries export the capability without them, under their own lumice.h logging
// and version (cmake/export_surfaces.cmake declares which headers each library exports).

#include "lumice_analytic_core.h"

#ifdef __cplusplus
extern "C" {
#endif

// Library version at run time; compare with LUMICE_ANALYTIC_API_VERSION to detect a
// header/library mismatch. Also the minimal function the build, export and load chain is proven
// with before any module exists.
LUMICE_ANALYTIC_API int LUMICE_ANALYTIC_GetApiVersion(void);

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
