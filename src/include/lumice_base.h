#ifndef LUMICE_BASE_H_
#define LUMICE_BASE_H_

// The foundation every other capability header builds on: symbol visibility (LUMICE_API), the
// opaque handle types, error codes, the ray-count type, logging and the product version. Every
// other lumice_*.h includes it; it includes none of them.

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Symbol visibility. Which functions a shared library exports is decided at link time by that
// library's export list, generated from its surface headers by scripts/gen_export_list.py (root
// CMakeLists.txt, lumice_apply_export_list; the headers are listed in cmake/export_surfaces.cmake)
// — liblumice exports exactly the functions declared in the lumice_*.h capability headers and the
// analytic capability, and liblumice_testapi / liblumice_analytic, built from the same objects,
// export their own lists. LUMICE_API does not export anything; it only makes a declaration eligible:
//   - GCC/Clang: default visibility. The engine objects are compiled with -fvisibility=hidden in
//     Release, and a hidden symbol cannot be exported by any list. The generator refuses a
//     declaration without the marker, so one cannot silently drop out of the library.
//   - Windows: the .def file does the exporting (no __declspec(dllexport) anywhere), so what is
//     left is the consumer-side dllimport, and only for a consumer that really links the engine
//     DLL. LUMICE_SHARED_DEFINE is an INTERFACE definition of the shared `lumice` target, so the
//     shells linking it see dllimport, while lumice_obj itself and every test/bench target that
//     links lumice_obj's objects directly see an empty macro — a dllimport there would ask the
//     linker for an __imp_ symbol no static link provides.
#if defined(_WIN32)
#if defined(LUMICE_SHARED_DEFINE)
#define LUMICE_API __declspec(dllimport)
#else
#define LUMICE_API
#endif
#else
#define LUMICE_API __attribute__((visibility("default")))
#endif

// =============== Constants ===============
// The ABI version (LUMICE_API_VERSION) and its change history live in lumice_api_version.h — a
// leaf header this file deliberately does NOT include: a macro value change invalidates the
// compile-cache entry of every TU in the includer's closure, so keeping the define out of this
// wide closure makes a bump recompile only the TUs that textually use the macro.
#define LUMICE_MAX_RENDER_RESULTS 16
#define LUMICE_MAX_STATS_RESULTS 1

// =============== Opaque Types ===============
typedef struct LUMICE_Server_ LUMICE_Server;
// Opaque, incrementally-built scene container. See the "Scene (opaque handle)" section of
// lumice_scene.h for its lifecycle + Add*/Set* build API. Declared here alongside the other opaque
// handle types; the build functions live after the leaf POD structs they consume.
typedef struct LUMICE_Scene_ LUMICE_Scene;
// Opaque, immutable, reference-counted snapshot of ALL results (mono render, composite,
// raw XYZ, stats) as of one server-side snapshot. Acquiring one gives the caller a real
// share of the data's lifetime: every buffer reachable through it stays valid until the
// caller releases the frame, no matter what the server does meanwhile. See the "Results"
// section of lumice_engine.h for the acquire/release + read functions.
typedef struct LUMICE_ResultFrame_ LUMICE_ResultFrame;

// =============== Error Codes ===============
typedef enum LUMICE_ErrorCode_ {
  LUMICE_OK = 0,
  LUMICE_ERR_NULL_ARG,
  LUMICE_ERR_INVALID_JSON,
  LUMICE_ERR_INVALID_CONFIG,
  LUMICE_ERR_MISSING_FIELD,
  LUMICE_ERR_INVALID_VALUE,
  LUMICE_ERR_FILE_NOT_FOUND,
  LUMICE_ERR_SERVER,
  LUMICE_ERR_UNKNOWN,
} LUMICE_ErrorCode;

// =============== Log Levels ===============
typedef enum LUMICE_LogLevel_ {
  LUMICE_LOG_TRACE,
  LUMICE_LOG_DEBUG,
  LUMICE_LOG_VERBOSE,
  LUMICE_LOG_INFO,
  LUMICE_LOG_WARNING,
  LUMICE_LOG_ERROR,
  LUMICE_LOG_OFF,
} LUMICE_LogLevel;

// =============== Ray Count Type ===============
// 64-bit count type for ray / ray-segment / crystal totals. Must stay >= 64-bit:
// `unsigned long` is only 32-bit on Windows (LLP64), which silently truncated
// totals above 2^32 (the status-bar ray-count rollover reported by Windows users).
// Used for every field that carries an actual ray-count value across the C API.
typedef unsigned long long LUMICE_RayCount;
#if defined(__cplusplus)
static_assert(sizeof(LUMICE_RayCount) >= 8, "LUMICE_RayCount must be 64-bit (Windows unsigned long is 32-bit)");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(LUMICE_RayCount) >= 8, "LUMICE_RayCount must be 64-bit (Windows unsigned long is 32-bit)");
#endif

// =============== Logging ===============
// Sets `server`'s loggers and the engine-wide (global) logger to `level`. `server` may be NULL
// (v4.50): then only the engine-wide level is set — for a caller that uses no server at all, such
// as one that only calls LUMICE_AnalyzeSinglePath.
LUMICE_API void LUMICE_SetLogLevel(LUMICE_Server* server, LUMICE_LogLevel level);

// Log callback: receives all Core log messages. Called from Core logging threads.
// Parameters: level, logger name (e.g. "Server", "Simulator"), pre-formatted message.
// The callback must be thread-safe.
typedef void (*LUMICE_LogCallback)(LUMICE_LogLevel level, const char* logger_name, const char* message);

// Register a log callback. Pass NULL to disable. Must be called BEFORE LUMICE_CreateServer()
// for full coverage, but can also be called later (subsequent log messages will be forwarded).
LUMICE_API void LUMICE_SetLogCallback(LUMICE_LogCallback callback);

// =============== Product Version ===============
// The product version string: "X.Y.Z" for a tagged release build (LUMICE_RELEASE_BUILD=ON) or
// "X.Y.Z-dev" otherwise. Single source: project(VERSION) in the top-level CMakeLists.txt,
// generated into a build-tree-only header by configure_file and read back here — the CLI's
// `--version`, the GUI window title, both startup log lines and the .lmc `app_version` field all
// call this instead of carrying their own copy. Never NULL; static storage, do not free.
LUMICE_API const char* LUMICE_GetVersionString(void);

// =============== Engine Build Provenance ===============
// The ISA tier this engine was compiled for: "baseline", "x86-64-v3", "x86-64-v4" or "native".
// Never NULL; static storage, do not free. The value is the tier name only when the matching
// -march flag was actually applied to the engine's objects, and "baseline" otherwise — so a Debug
// or MinSizeRel build, and every build by real MSVC cl.exe (which has no equivalent flag),
// answers "baseline" no matter what was asked of the configure. It is the engine's own answer:
// in a build where the engine is a shared library, it is the tier of the library that this
// process actually loaded, which the executable's own compile cannot know. The CLI's
// `[BENCHMARK]` JSON `isa` key is this string.
LUMICE_API const char* LUMICE_GetEngineIsaLevel(void);

#ifdef __cplusplus
}
#endif

#endif  // LUMICE_BASE_H_
