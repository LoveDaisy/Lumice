// In-process engine loader for the Windows release.
//
// The Windows x64 release ships each entry point ONCE (`Lumice.exe`, `LumiceGUI.exe`, both
// compiled by MSVC cl.exe for the x86-64 baseline) next to TWO builds of the engine DLL —
// `lumice-engine.baseline.dll` (cl.exe, runs on any x86_64 CPU) and
// `lumice-engine.x86-64-v3.dll` (clang-cl, AVX2+FMA, ~2.3x faster where the CPU has it). The
// executables reach the engine through delay-load (/DELAYLOAD in src/CMakeLists.txt and
// src/gui/CMakeLists.txt): the linker binds them to the baseline DLL's name, and this file's
// hook substitutes, at the first call into the engine, the module it has already loaded — the
// tier CPUID says this machine can run, from an ABSOLUTE PATH under the executable's own
// directory. That is the whole job: detect, resolve, load, hand over.
//
// This replaced a launcher process (formerly src/launcher/isa_launcher_win.c) that shipped two
// complete executables per entry point and picked one with CreateProcess. Everything that
// launcher had to get right because it was a separate process — quoting argv back into a
// command line, forwarding the child's exit code, swallowing its own copy of Ctrl-C, detaching
// from the console so the GUI's window closed — is not carried here; it does not exist. There
// is one process, and it is the one doing the work. What IS carried over verbatim is the CPUID
// detection (DetectTier and its table below): it covers the whole x86-64-v3 psABI level plus the
// XGETBV check that the OS saves YMM state, and was validated on the Windows reference box with
// `bcdedit /set xsavedisable 1` as the negative control.
//
// Four decisions, each a measured fact rather than a preference:
//
// - The DLL is loaded by absolute path with LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
//   LOAD_LIBRARY_SEARCH_DEFAULT_DIRS, never by name. The trust boundary of the package is the
//   installation directory; PATH and the working directory are not part of it, and a same-named
//   DLL placed in either must not be picked up. (The flags also let the engine's own imports —
//   cudart64_*.dll next to it — resolve from that same directory.)
//
// - The delay-load notification hook NEVER returns NULL from dliNotePreLoadLibrary. Measured on
//   the reference box: a NULL from that notification does not reach the failure hook, it makes
//   the delay-load helper fall back to a plain LoadLibrary(<linked name>) — a PATH/working-
//   directory search of exactly the kind the point above forbids, and one that, when it
//   succeeds, silently loads the wrong tier. So the hook either hands over the module or
//   reports and ends the process itself.
//
// - The engine is loaded EAGERLY from main() (LumiceEngineLoaderInit), not lazily at the first
//   C API call. Lazy loading would put the "engine DLL missing" failure at whichever call
//   happens first — inside the GUI's frame loop, after FreeConsole() — where the only outlet is
//   the process dying. Loading it first thing turns that into an ordinary early exit with a
//   message, and the hook's own failure path becomes a second line of defence rather than the
//   first.
//
// - This file is plain C with a `.c` suffix and writes to stderr directly, on purpose. It runs
//   before the engine exists, so the engine's logger (ILOG_*) is not available to it; and the
//   repository's no-bare-print gate (scripts/check_policies.py) scans C++/CUDA/Metal suffixes
//   only, so a `.c` file is outside its scope by construction rather than by exemption — the
//   same footing the retired launcher stood on. A `.c` file that did link the engine would be
//   wrong for the same reason this one is right; do not reuse the suffix as a loophole.
//
// Two compile-time inputs come from CMake. LUMICE_ENGINE_LINKED_DLL is the file name the
// executable was linked against ($<TARGET_FILE_NAME:lumice>, e.g. "lumice-engine.baseline.dll"
// in the release); the hook only ever substitutes for THAT name, so any other delay-loaded DLL
// keeps the helper's default handling. The two release tier names are constants here: they are
// the file names release.yml's merge step produces, and the pair is the product's contract.

#if !defined(_WIN32) || !defined(_M_X64)
#error "win_engine_loader.c is Windows x64 only: it reads GetModuleFileName and dispatches on x86 CPUID"
#endif
#if !defined(LUMICE_ENGINE_LINKED_DLL)
#error \
    "win_engine_loader.c needs LUMICE_ENGINE_LINKED_DLL (the DLL name the executable delay-loads); see src/CMakeLists.txt"
#endif

// clang-format off: windows.h must come first — delayimp.h uses the Win32 types without
// including them, and the formatter's include ordering would put it after delayimp.h.
#include <windows.h>

#include <delayimp.h>
#include <intrin.h>
#include <stdio.h>
#include <string.h>

#include "launcher/win_engine_loader.h"
// clang-format on

static const char kIsaPrefix[] = "--isa=";
static const char kBaseline[] = "baseline";
static const char kV3[] = "x86-64-v3";
static const char kDllPrefix[] = "lumice-engine.";
static const char kDllSuffix[] = ".dll";
static const char kLinkedDll[] = LUMICE_ENGINE_LINKED_DLL;

// Process-wide state, written once by LumiceEngineLoaderInit and read by the hooks. The hooks
// run on whichever thread first calls into the engine; that is after Init returned on the main
// thread, so no synchronisation is needed for the reads.
static char g_self_name[MAX_PATH] = "Lumice";
static char g_engine_path[MAX_PATH] = "";
static HMODULE g_engine = NULL;
static LumiceEngineLoaderReport g_report = LUMICE_ENGINE_LOADER_REPORT_STDERR;

static void Report(const char* line) {
  fprintf(stderr, "%s: %s\n", g_self_name, line);
  fflush(stderr);
  if (g_report == LUMICE_ENGINE_LOADER_REPORT_MESSAGE_BOX) {
    MessageBoxA(NULL, line, g_self_name, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
  }
}

static void ReportLastError(const char* what, const char* target, DWORD err) {
  char msg[512];
  DWORD n =
      FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, 0, msg, sizeof(msg), NULL);
  if (n == 0) {
    snprintf(msg, sizeof(msg), "error %lu", (unsigned long)err);
  } else {
    while (n > 0 && (msg[n - 1] == '\n' || msg[n - 1] == '\r' || msg[n - 1] == ' ')) {
      msg[--n] = '\0';
    }
  }
  char line[MAX_PATH + 640];
  snprintf(line, sizeof(line), "%s %s: %s", what, target, msg);
  Report(line);
}

static void Usage(void) {
  fprintf(stderr,
          "  %s%s | %s%s   force one engine tier (the token is consumed, not forwarded)\n"
          "  without the token, the tier is chosen by CPUID; the engine is loaded from the\n"
          "  directory of this executable as %s<tier>%s\n",
          kIsaPrefix, kBaseline, kIsaPrefix, kV3, kDllPrefix, kDllSuffix);
}

// One CPUID feature bit: which leaf/subleaf to query, which register (0=EAX..3=EDX) and bit.
typedef struct {
  int leaf;
  int subleaf;
  int reg;
  int bit;
} CpuidBit;

static int HasBit(const CpuidBit* b) {
  int info[4];
  __cpuidex(info, b->leaf, b->subleaf);
  return (info[b->reg] >> b->bit) & 1;
}

// Detection covers the whole x86-64-v3 psABI level (every v2/v3 feature bit plus the XGETBV
// check that the OS saves YMM state), not just the four bits an AVX2 binary visibly needs:
// `-march=x86-64-v3` lets the compiler use all of them. Order matters in one place: OSXSAVE
// (CPUID.1:ECX[27]) must be confirmed before `_xgetbv` is executed — with CR4.OSXSAVE clear (a
// machine booted with `bcdedit /set xsavedisable 1`) XGETBV is #UD, and this code exists
// precisely to not crash on the CPU/OS states it is protecting against.
static const char* DetectTier(void) {
  // Leaf availability first: CPUID.0:EAX is the highest basic leaf, and leaf 7 (AVX2/BMI) is
  // only meaningful if it is reported; the extended leaf 0x80000001 likewise.
  int info[4];
  __cpuid(info, 0);
  if ((unsigned)info[0] < 7u) {
    return kBaseline;
  }
  __cpuid(info, (int)0x80000000);
  if ((unsigned)info[0] < 0x80000001u) {
    return kBaseline;
  }

  // x86-64-v2: CMPXCHG16B, LAHF-SAHF, POPCNT, SSE3, SSE4.1, SSE4.2, SSSE3.
  // x86-64-v3: AVX, AVX2, BMI1, BMI2, F16C, FMA, LZCNT, MOVBE, plus OS-enabled YMM state.
  static const CpuidBit kV3Bits[] = {
    { 1, 0, 2, 0 },                // SSE3
    { 1, 0, 2, 9 },                // SSSE3
    { 1, 0, 2, 12 },               // FMA
    { 1, 0, 2, 13 },               // CMPXCHG16B
    { 1, 0, 2, 19 },               // SSE4.1
    { 1, 0, 2, 20 },               // SSE4.2
    { 1, 0, 2, 22 },               // MOVBE
    { 1, 0, 2, 23 },               // POPCNT
    { 1, 0, 2, 27 },               // OSXSAVE — must be checked before _xgetbv below (XGETBV is #UD otherwise)
    { 1, 0, 2, 28 },               // AVX
    { 1, 0, 2, 29 },               // F16C
    { 7, 0, 1, 3 },                // BMI1
    { 7, 0, 1, 5 },                // AVX2
    { 7, 0, 1, 8 },                // BMI2
    { (int)0x80000001, 0, 2, 0 },  // LAHF-SAHF
    { (int)0x80000001, 0, 2, 5 },  // LZCNT (ABM)
  };
  for (size_t i = 0; i < sizeof(kV3Bits) / sizeof(kV3Bits[0]); ++i) {
    if (!HasBit(&kV3Bits[i])) {
      return kBaseline;
    }
  }
  // Every bit above is set, OSXSAVE included, so XGETBV is legal here. XCR0[1] = SSE state,
  // XCR0[2] = AVX (YMM upper) state: the OS has to save both across context switches, or a
  // -march=x86-64-v3 binary corrupts its own registers.
  unsigned long long xcr0 = _xgetbv(0);
  if ((xcr0 & 0x6ull) != 0x6ull) {
    return kBaseline;
  }
  return kV3;
}

// The file name of the engine DLL for `tier`, as release.yml's merge step names it.
static int TierDllName(const char* tier, char* out, size_t cap) {
  int len = snprintf(out, cap, "%s%s%s", kDllPrefix, tier, kDllSuffix);
  return len > 0 && (size_t)len < cap;
}

// Is the DLL this executable was linked against one of the two the release ships? In the
// release it always is (the shell comes from the baseline configure). In a local
// BUILD_SHARED_LIBS build it is not: the local default tier is `native`, the configure
// produced exactly one engine DLL, named for that tier, and CPUID has nothing to choose
// between — so such a build loads its own DLL and the tier table above does not apply. This
// is decided from the link-time name alone, with no probing of the file system: a release
// shell whose chosen tier is missing must fail (see LumiceEngineLoaderInit), not quietly load
// the other one.
static int LinkedDllIsAReleaseTier(void) {
  char name[MAX_PATH];
  if (TierDllName(kBaseline, name, sizeof(name)) && _stricmp(name, kLinkedDll) == 0) {
    return 1;
  }
  if (TierDllName(kV3, name, sizeof(name)) && _stricmp(name, kLinkedDll) == 0) {
    return 1;
  }
  return 0;
}

// Loads `path` (absolute) and records it as the engine. Returns 0 on success, otherwise the
// exit code after having reported the failure.
static int LoadEngineAt(const char* path) {
  HMODULE h = LoadLibraryExA(path, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
  if (!h) {
    // 127 is the shell's "command not found" code, and a missing engine DLL (a partially
    // unpacked zip) is the same failure — the code the retired launcher used for a missing
    // sidecar.
    ReportLastError("cannot load the engine DLL", path, GetLastError());
    return 127;
  }
  g_engine = h;
  return 0;
}

int LumiceEngineLoaderInit(int* argc, char** argv, LumiceEngineLoaderReport report) {
  g_report = report;

  // Own path first: the directory is where the engine DLLs are, the file name is what error
  // lines and the message box are titled with.
  char self_path[MAX_PATH];
  DWORD n = GetModuleFileNameA(NULL, self_path, (DWORD)sizeof(self_path));
  if (n == 0 || n >= sizeof(self_path)) {
    Report("cannot resolve own executable path");
    return 127;
  }
  char* slash = strrchr(self_path, '\\');
  size_t dir_len = slash ? (size_t)(slash - self_path) + 1 : 0;  // keeps the trailing '\'
  snprintf(g_self_name, sizeof(g_self_name), "%s", slash ? slash + 1 : self_path);

  // Consume the override token; every other argument keeps its relative order.
  const char* forced = NULL;
  int out = 1;
  for (int i = 1; i < *argc; ++i) {
    if (strncmp(argv[i], kIsaPrefix, sizeof(kIsaPrefix) - 1) != 0) {
      argv[out++] = argv[i];
      continue;
    }
    const char* value = argv[i] + sizeof(kIsaPrefix) - 1;
    if (forced) {
      char line[128];
      snprintf(line, sizeof(line), "%s given more than once", kIsaPrefix);
      Report(line);
      Usage();
      return 1;
    }
    if (strcmp(value, kBaseline) == 0) {
      forced = kBaseline;
    } else if (strcmp(value, kV3) == 0) {
      forced = kV3;
    } else {
      char line[256];
      snprintf(line, sizeof(line), "unknown ISA tier \"%s\"", value);
      Report(line);
      Usage();
      return 1;
    }
  }
  argv[out] = NULL;
  *argc = out;

  // Which file: the forced tier if given; otherwise this executable's own DLL when it is not
  // a release shell (see LinkedDllIsAReleaseTier); otherwise the tier CPUID picks.
  char dll_name[MAX_PATH];
  if (forced) {
    if (!TierDllName(forced, dll_name, sizeof(dll_name))) {
      Report("engine DLL name too long");
      return 127;
    }
  } else if (!LinkedDllIsAReleaseTier()) {
    snprintf(dll_name, sizeof(dll_name), "%s", kLinkedDll);
  } else if (!TierDllName(DetectTier(), dll_name, sizeof(dll_name))) {
    Report("engine DLL name too long");
    return 127;
  }
  int len = snprintf(g_engine_path, sizeof(g_engine_path), "%.*s%s", (int)dir_len, self_path, dll_name);
  if (len < 0 || (size_t)len >= sizeof(g_engine_path)) {
    Report("engine DLL path too long");
    return 127;
  }
  return LoadEngineAt(g_engine_path);
}

// Delay-load notification hook. dliNotePreLoadLibrary for the linked engine name hands the
// helper the module Init loaded (or loads it now, if a caller reached the engine before Init —
// a programming error in the shell, but not one to answer with a PATH search). Every other
// notification, and every other DLL, gets the helper's default handling (return NULL).
static FARPROC WINAPI DelayLoadNotify(unsigned dliNotify, PDelayLoadInfo pdli) {
  if (dliNotify != dliNotePreLoadLibrary || !pdli || !pdli->szDll || _stricmp(pdli->szDll, kLinkedDll) != 0) {
    return NULL;
  }
  if (!g_engine) {
    if (g_engine_path[0] == '\0') {
      Report("the engine was called before LumiceEngineLoaderInit ran");
      ExitProcess(127);
    }
    if (LoadEngineAt(g_engine_path) != 0) {
      ExitProcess(127);
    }
  }
  return (FARPROC)g_engine;
}

// Delay-load failure hook. dliFailLoadLib cannot follow a non-NULL module from the notify
// hook and is kept as the second line of defence the file header describes; dliFailGetProc
// is real — an engine DLL from a different build that lacks a symbol this shell imports — and
// is named as such rather than left to the helper's unhandled-exception dialog.
static FARPROC WINAPI DelayLoadFailure(unsigned dliNotify, PDelayLoadInfo pdli) {
  if (dliNotify == dliFailLoadLib) {
    ReportLastError("cannot load the engine DLL", pdli && pdli->szDll ? pdli->szDll : kLinkedDll,
                    pdli ? pdli->dwLastError : ERROR_MOD_NOT_FOUND);
    ExitProcess(127);
  }
  if (dliNotify == dliFailGetProc) {
    char line[MAX_PATH + 256];
    const char* proc = (pdli && pdli->dlp.fImportByName) ? pdli->dlp.szProcName : "(ordinal)";
    snprintf(line, sizeof(line), "the engine DLL %s does not export %s (a mismatched build?)",
             g_engine_path[0] ? g_engine_path : kLinkedDll, proc);
    Report(line);
    ExitProcess(127);
  }
  return NULL;
}

// delayimp.h declares both pointers `const` (Visual Studio 2015 and later); defining them here
// overrides the NULL defaults in delayimp.lib. Both cl.exe and clang-cl accept this form in C.
const PfnDliHook __pfnDliNotifyHook2 = DelayLoadNotify;
const PfnDliHook __pfnDliFailureHook2 = DelayLoadFailure;
