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

#ifdef __cplusplus
}
#endif

#endif  // LUMICE_ANALYTIC_H_
