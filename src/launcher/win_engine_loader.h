// In-process engine loader for the Windows release: picks the engine DLL this CPU can run and
// loads it from the executable's own directory before the first C API call. See
// win_engine_loader.c for the mechanism and the decisions; this header is the whole surface
// the two shells (src/main.cpp, src/gui/main.cpp) use.
//
// Only compiled into, and only meaningful for, a Windows build whose engine is a shared
// library (WIN32 AND BUILD_SHARED_LIBS — src/CMakeLists.txt and src/gui/CMakeLists.txt add
// this translation unit and define LUMICE_ENGINE_DELAY_LOADED there). A static build has no
// engine DLL to choose and never includes this header.

#ifndef LUMICE_WIN_ENGINE_LOADER_H_
#define LUMICE_WIN_ENGINE_LOADER_H_

#ifdef __cplusplus
extern "C" {
#endif

// Where a failure to load the engine is reported. The engine is not loaded yet when this
// happens, so the engine's logger does not exist; the shell has stderr and, for the GUI, a
// message box (the GUI releases its console at start-up, so stderr alone can vanish).
typedef enum LumiceEngineLoaderReport_ {
  LUMICE_ENGINE_LOADER_REPORT_STDERR = 0,      // one line on stderr (CLI)
  LUMICE_ENGINE_LOADER_REPORT_MESSAGE_BOX = 1  // stderr line + MessageBox (GUI)
} LumiceEngineLoaderReport;

// Consumes every `--isa=<tier>` token from argv (the remaining arguments are shifted down and
// *argc reduced, so the shell's own option parser never sees the token), decides which engine
// DLL this process uses — the forced tier if one was given, CPUID otherwise — and loads it from
// the directory of this executable by absolute path. Must run before any LUMICE_* call: the
// engine is delay-loaded, and the first call into it is what triggers the load.
//
// Returns 0 when the engine is loaded. Otherwise the failure has already been reported
// through `report` (which token was bad, or which file could not be loaded and why) and the
// return value is the process exit code the caller should return from main(): 1 for a bad
// `--isa=` value, 127 for an engine DLL that could not be loaded (the shell's "command not
// found", the code the retired launcher used for a missing sidecar).
int LumiceEngineLoaderInit(int* argc, char** argv, LumiceEngineLoaderReport report);

#ifdef __cplusplus
}
#endif

#endif  // LUMICE_WIN_ENGINE_LOADER_H_
