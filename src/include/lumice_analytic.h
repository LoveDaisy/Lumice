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
#define LUMICE_ANALYTIC_API_VERSION 1

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
// Installing a non-NULL callback logs one LUMICE_ANALYTIC_LOG_INFO line, "log callback installed", so
// the host can see the wiring work. NULL stops forwarding; the library is then silent again. An initialisation call:
// make it before any computation, from one thread (doc/analytic-api.md section 5.3).
LUMICE_ANALYTIC_API void LUMICE_ANALYTIC_SetLogCallback(LUMICE_ANALYTIC_LogCallback callback);

#ifdef __cplusplus
}
#endif

#endif  // LUMICE_ANALYTIC_H_
