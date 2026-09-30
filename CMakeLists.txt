cmake_minimum_required(VERSION 3.14)

# macOS: minimum deployment target (Ventura).
# Must be set before project() so the compiler detection picks it up.
if(APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version")
endif()

# CMP0091 decides whether CMAKE_MSVC_RUNTIME_LIBRARY (set in the MSVC block further
# down, with the reasoning for the /MT choice) means anything at all. It has to be
# decided HERE, before project(): project() enables the C/CXX languages, and that is
# when CMake either bakes a runtime-library flag into the CMAKE_<LANG>_FLAGS_<CONFIG>
# defaults (OLD) or leaves them alone and honors the abstraction instead (NEW).
# Setting the policy after project() is measurably too late — the flags are already
# baked and the whole block silently does nothing.
#   - if(POLICY ...) guard: below CMake 3.15 this policy does not exist, and
#     cmake_policy(SET) on an unknown id is a hard configure error, not a no-op. The
#     silent-/MD case that leaves is caught by the MSVC block's check further down.
#   - CMAKE_POLICY_DEFAULT_CMP0091 covers scopes created later. A function records the
#     policy stack in effect where it was defined, and cmake/CPM.cmake declares its own
#     (lower) cmake_minimum_required at the top, so CPMAddPackage and the
#     add_subdirectory it performs would otherwise run with CMP0091 unset and the
#     third-party dependencies would keep the dynamic CRT while this project uses the
#     static one. Both must precede include(cmake/CPM.cmake) and every add_subdirectory.
# This is deliberately not gated on MSVC: neither line exists as a variable before
# project(), and both are inert on every non-MSVC toolchain.
if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
  set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
endif()

project(Lumice VERSION 4.7.1)

set(CMAKE_VERBOSE_MAKEFILE OFF)

# ==================================================================================================
# Basic
set(CMAKE_CXX_STANDARD 17)            # C++17...
set(CMAKE_CXX_STANDARD_REQUIRED ON)   #...is required...
set(CMAKE_CXX_EXTENSIONS OFF)         #...without compiler extensions like gnu++11
set(CMAKE_C_STANDARD 11)              # C11...

# ISA tier a Release build is compiled for (compilers that honor -march: GCC, Clang and
# clang-cl; real MSVC cl.exe has no equivalent flag wired up and is always the baseline). One
# four-valued variable, not a boolean plus a second knob: the Linux and Windows releases each
# ship TWO builds of the engine and pick one at start-up (Windows: the shell's delay-load hook
# by CPUID; Linux: glibc's hwcaps subdirectory lookup, see src/CMakeLists.txt), and "which tier
# is this?" must have exactly one answer per configure.
#   baseline   no -march flag: the x86-64-v1 floor every x86_64 CPU runs; what CI tests
#   x86-64-v3  -march=x86-64-v3 (AVX2+FMA): the second Windows release variant (clang-cl —
#              LLVM takes its whole ~2.26x at v3, where GCC takes none of it; see
#              doc/performance-testing.md)
#   x86-64-v4  -march=x86-64-v4 (AVX-512): the second Linux release variant
#   native     -march=native: local default, a developer machine builds for itself
set(LUMICE_ISA_LEVEL "native" CACHE STRING
    "ISA tier for Release builds: baseline | x86-64-v3 | x86-64-v4 | native (GCC/Clang/clang-cl only)")
set_property(CACHE LUMICE_ISA_LEVEL PROPERTY STRINGS baseline x86-64-v3 x86-64-v4 native)
if(NOT LUMICE_ISA_LEVEL MATCHES "^(baseline|x86-64-v3|x86-64-v4|native)$")
  message(FATAL_ERROR "LUMICE_ISA_LEVEL=\"${LUMICE_ISA_LEVEL}\" is not one of: baseline, x86-64-v3, x86-64-v4, native")
endif()
if(LUMICE_ISA_LEVEL MATCHES "^x86-64-v[34]$" AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
  # Fail here, in words, rather than a few hundred lines into the build with a compiler
  # error about an unknown -march value on an arm64 host.
  message(FATAL_ERROR "LUMICE_ISA_LEVEL=${LUMICE_ISA_LEVEL} is an x86_64 tier; CMAKE_SYSTEM_PROCESSOR is "
                      "\"${CMAKE_SYSTEM_PROCESSOR}\"")
endif()

# clang-cl: CMake reports MSVC=TRUE for it (it drives the MSVC ABI, CRT and /-style flags), yet
# it is a Clang frontend that honors -march. That one fact is why the Windows release's
# x86-64-v3 variant exists at all (the ISA-tier STATUS message and lumice_apply_isa_march()
# below both branch on it), so it is computed once, here, and nowhere else. Not a cache
# entry: it is derived from the chosen compiler every configure and must never go stale.
set(LUMICE_CLANG_CL FALSE)
if(MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
   AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
  set(LUMICE_CLANG_CL TRUE)
endif()

# CUDA backend (off by default; turn on for dev49 docker CUDA 11.6+ environments).
# scrum-cuda-backend-mvp subtask 3: enables CudaTraceBackend + lumice_trace.cu.
# Mac/Windows host builds without nvcc must stay zero-regress when this is OFF.
option(LUMICE_CUDA_ENABLED "Enable CUDA backend (requires nvcc / CUDAToolkit)" OFF)

# scrum-306.1 (explore-async-evidence, H7): embed Metal shader sources + line
# tables into the metallib so Xcode/Instruments GPU shader profiler maps occupancy
# / per-line ALU cost back to lumice_trace.metal. No runtime perf impact (only a
# slightly larger embedded metallib blob); keep OFF for release/CI. Turn ON for
# the Xcode profiling build (build/xcode) used to measure Metal kernel occupancy.
option(LUMICE_METAL_SHADER_PROFILE "Embed Metal shader source + line tables for GPU shader profiler" OFF)

# A non-tag build must be visibly distinguishable from a tagged release: OFF (the default)
# appends "-dev" to LUMICE_GetVersionString()'s return value, and release.yml is the only caller
# that sets this ON. No git-sha injection -- one string, one knob, and the suffix decision is made
# here at configure time so no run-time code has to ask "release or dev?" a second time.
option(LUMICE_RELEASE_BUILD "Building a tagged release artifact (omit the -dev version suffix)" OFF)

if(MSVC)
  add_definitions(-DNOMINMAX)
  set(CMAKE_DEBUG_POSTFIX "d")
endif()

# Make the local-build-vs-shipped-binary ISA gap visible at configure time. Local
# scripts/build.sh does not pass LUMICE_ISA_LEVEL and therefore takes the "native" default,
# while every ci.yml job passes -DLUMICE_ISA_LEVEL=baseline explicitly and release.yml builds
# the Linux x64 package once at baseline and once at x86-64-v4. That default is deliberate
# (a developer machine should build for itself and run fast); what it must not do is stay
# invisible at the point where someone reads a throughput number off this build and
# compares it with another platform. See doc/performance-testing.md, section "Local builds
# are not the shipped binary".
# Deliberately conditional wording ("would apply"): this message runs once at configure
# time, whereas the -march gate below adds $<CONFIG:Release> and is only resolved
# per-configuration at generate/build time, so an assertive phrasing would be wrong for a
# Debug build with a non-baseline tier selected.
if(MSVC AND NOT LUMICE_CLANG_CL)
  message(STATUS "LUMICE_ISA_LEVEL: not applicable to MSVC cl.exe (no equivalent flag is "
                 "wired up); this build is the release ISA baseline.")
elseif(LUMICE_ISA_LEVEL STREQUAL "native")
  message(STATUS "LUMICE_ISA_LEVEL=native (local default): a Release build would apply "
                 "-march=native, so its binary is NOT one release ships. Fine for local "
                 "iteration; take any cross-platform or externally quoted throughput number "
                 "with -DLUMICE_ISA_LEVEL=baseline instead. See doc/performance-testing.md.")
elseif(LUMICE_ISA_LEVEL STREQUAL "x86-64-v3")
  message(STATUS "LUMICE_ISA_LEVEL=x86-64-v3: a Release build would apply -march=x86-64-v3 "
                 "(AVX2+FMA); this is the Windows release's second engine DLL (clang-cl) and "
                 "is only loaded on CPUs the shell's CPUID check selects it for.")
elseif(LUMICE_ISA_LEVEL STREQUAL "x86-64-v4")
  message(STATUS "LUMICE_ISA_LEVEL=x86-64-v4: a Release build would apply -march=x86-64-v4 "
                 "(AVX-512); this is the Linux release's second engine .so and is only "
                 "loaded on CPUs glibc's hwcaps lookup selects it for.")
else()
  message(STATUS "LUMICE_ISA_LEVEL=baseline: this build matches the release ISA baseline.")
endif()
if(LUMICE_CLANG_CL)
  message(STATUS "clang-cl detected (MSVC=TRUE, Clang frontend): the -march tier above applies "
                 "through lumice_apply_isa_march(); the MSVC-only flag set does not.")
endif()

# Persist the compiler id into CMakeCache.txt for test tooling that has to answer "was
# -march=native actually compiled into this binary?" from a source independent of the
# C++ macro path it is checking. CMake itself keeps CMAKE_CXX_COMPILER_ID only in
# CMakeFiles/<cmake-version>/CMakeCXXCompiler.cmake, whose per-version directory name is
# an internal layout detail; this snapshot is a plain cache entry instead.
# MUST stay outside any if(MSVC)/if(NOT MSVC) branch: the value is equally needed on
# MSVC (where it reads "MSVC"), and burying it in one branch would silently downgrade
# that cross-check to a skip on the platforms it did not run on.
set(LUMICE_COMPILER_ID_SNAPSHOT "${CMAKE_CXX_COMPILER_ID}" CACHE STRING
    "Snapshot of CMAKE_CXX_COMPILER_ID at configure time (read by test tooling)." FORCE)
mark_as_advanced(LUMICE_COMPILER_ID_SNAPSHOT)

# MinGW: statically link GCC runtime (libgcc, libstdc++, libwinpthread) so the
# distributed .exe is self-contained and doesn't require MinGW DLLs on the target machine.
if(MINGW)
  add_link_options(-static)
endif()

# MSVC: the executables this repo distributes link the static CRT (/MT) so they run on
# a machine with no VC++ Redistributable installed. The shared-library flavor keeps the
# dynamic CRT (/MD), and that split is the whole point rather than an oversight:
#
#   - The release workflow publishes only the CLI and GUI executables. For those, a
#     private CRT is exactly what is wanted, and the dynamic CRT's worst failure mode
#     (several modules each holding their own CRT state) cannot arise in a single
#     self-contained process.
#   - A DLL is the opposite case: it is loaded into someone else's process, and a static
#     CRT gives it a *private copy of the CRT state* — its own heap, and its own
#     environment block. The environment half is not theoretical here. It was measured:
#     with /MT, a LUMICE_* variable the host process sets after loading the library is
#     invisible to getenv() inside it, because the library's CRT snapshotted the
#     environment when it initialized. The whole CUDA parity suite, which selects the
#     backend exactly that way, silently ran on the CPU path instead and failed 11/11;
#     the same suite passes with /MD. Sharing the host's CRT is the correct behavior for
#     a library, so this is the answer, not a workaround.
#
# So: the trigger for revisiting is a change to what gets *published*, not a CMake or
# compiler version bump.
#
# The setting below only means something if CMP0091 is NEW; that is arranged before
# project() at the top of this file, where the reasoning lives. When CMP0091 is not in
# effect, CMake's own default is the dynamic CRT, which is why only the static-CRT
# branch has to check that the abstraction actually works.
#
# That check reads the state which actually decides the outcome rather than the policy's
# nominal value: under CMP0091 NEW, CMake puts no runtime-library flag in the per-config
# flag defaults and adds it from the abstraction instead. A runtime flag sitting in those
# defaults therefore means the abstraction is being ignored and this build links the
# dynamic CRT no matter what the line below says — whether because the CMake in use
# predates 3.15, because someone moved the policy call below project() (that regression
# is not hypothetical), or because the flags were overridden on the command line. Asking
# cmake_policy(GET) instead would not catch any of those: it reports NEW happily while
# the baked flags win.
#
# When you touch this, verify the *artifact*, not the intent: check that the generated
# build.ninja compile FLAGS carry the static-CRT flag (dependencies included, not just
# this project's own sources) and that dumpbin /dependents on the produced .exe lists no
# MSVCP140.dll / VCRUNTIME140.dll / VCRUNTIME140_1.dll. This block was inert for a long
# time precisely because it was only ever read, never measured. If some target ever needs
# a per-target MSVC_RUNTIME_LIBRARY override, add it in one named block per build unit
# (one here after all CPMAddPackage calls, one in test/CMakeLists.txt) instead of
# scattering set_target_properties calls, so a reader can still answer "who decides this
# target's CRT" from a single place.
#
# LUMICE_MSVC_STATIC_CRT is the single answer to "is this build using the static CRT",
# for anything downstream that has to agree with it (see test/CMakeLists.txt). It is
# deliberately the *measured* answer, not the intended one.
# Both languages are checked, not just CXX. CMP0091 bakes into CMAKE_<LANG>_FLAGS_<CONFIG>
# per language, so a regression that reaches only one of them — a future CMake version
# difference, or someone overriding CMAKE_C_FLAGS_RELEASE — would otherwise slip past a
# CXX-only check, and the C-only dependencies (glad) would quietly link the other CRT.
#
# Two things about reading this check's result. It is only trustworthy on a FRESH cache:
# these are CACHE variables, so once a configure has baked a runtime flag into them, a
# reconfigure in the same build directory reuses the stale value and the check reports on
# that rather than on what this file now does — deleting the build tree (or at least
# CMakeCache.txt) is part of verifying it. And when it does find a baked flag, it reads
# WHICH one: the usual failure bakes the dynamic flag, but an explicit
# -DCMAKE_CXX_FLAGS_RELEASE=/MT bakes the static one, and in that case the build really is
# static even though the abstraction is being bypassed. Reporting OFF there would push
# gtest to the opposite CRT and produce the very LNK1319 mismatch this variable exists to
# prevent, so the answer follows the measurement in both directions.
set(LUMICE_MSVC_STATIC_CRT OFF)
if(MSVC AND NOT BUILD_SHARED_LIBS)
  string(REGEX MATCHALL "[-/]M[DT]"
         LUMICE_BAKED_CRT_FLAGS
         "${CMAKE_CXX_FLAGS_RELEASE} ${CMAKE_CXX_FLAGS_DEBUG} ${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${CMAKE_CXX_FLAGS_MINSIZEREL} ${CMAKE_C_FLAGS_RELEASE} ${CMAKE_C_FLAGS_DEBUG} ${CMAKE_C_FLAGS_RELWITHDEBINFO} ${CMAKE_C_FLAGS_MINSIZEREL}")
  if(LUMICE_BAKED_CRT_FLAGS)
    # Static only if EVERY baked flag is the static one; a mix means different translation
    # units disagree, which is not something to report as either answer confidently, so it
    # takes the conservative one.
    set(LUMICE_MSVC_STATIC_CRT ON)
    set(LUMICE_BAKED_CRT_SAW_STATIC OFF)
    set(LUMICE_BAKED_CRT_SAW_DYNAMIC OFF)
    foreach(LUMICE_BAKED_CRT_FLAG IN LISTS LUMICE_BAKED_CRT_FLAGS)
      if(NOT LUMICE_BAKED_CRT_FLAG MATCHES "MT$")
        set(LUMICE_MSVC_STATIC_CRT OFF)
        set(LUMICE_BAKED_CRT_SAW_DYNAMIC ON)
      else()
        set(LUMICE_BAKED_CRT_SAW_STATIC ON)
      endif()
    endforeach()
    # string(CONCAT), not set(), so the result is ONE string: it is expanded inside a quoted
    # message() argument below, where a multi-element list would render its separators.
    if(LUMICE_MSVC_STATIC_CRT)
      string(CONCAT LUMICE_BAKED_CRT_DESCRIPTION
             "the STATIC CRT, through those flags rather than "
             "through CMAKE_MSVC_RUNTIME_LIBRARY")
    elseif(LUMICE_BAKED_CRT_SAW_STATIC AND LUMICE_BAKED_CRT_SAW_DYNAMIC)
      # Worth its own wording: this is not "the build fell back to /MD", it is compilation
      # units disagreeing with each other, which surfaces at link time as LNK1319/LNK2038.
      # Sending the reader looking for "why did it all become dynamic" would be the wrong
      # search.
      string(CONCAT LUMICE_BAKED_CRT_DESCRIPTION
             "a MIX of static and dynamic CRT flags across compilation units, which will "
             "fail at link time (LNK1319/LNK2038) rather than produce a working binary")
    else()
      string(CONCAT LUMICE_BAKED_CRT_DESCRIPTION
             "the DYNAMIC CRT and depend on the VC++ Redistributable")
    endif()
    set(LUMICE_BAKED_CRT_FLAGS_SEEN ${LUMICE_BAKED_CRT_FLAGS})
    list(REMOVE_DUPLICATES LUMICE_BAKED_CRT_FLAGS_SEEN)
    string(REPLACE ";" " " LUMICE_BAKED_CRT_FLAGS_SEEN "${LUMICE_BAKED_CRT_FLAGS_SEEN}")
    message(WARNING "MSVC runtime library selection is not in effect: a runtime flag "
                    "(${LUMICE_BAKED_CRT_FLAGS_SEEN}) is baked into the per-config compiler flags, "
                    "so CMAKE_MSVC_RUNTIME_LIBRARY is ignored and this build will link "
                    "${LUMICE_BAKED_CRT_DESCRIPTION}" " (CMake <3.15, a CMP0091 call moved "
                    "after project(), or overridden CMAKE_C_FLAGS_<CONFIG> / "
                    "CMAKE_CXX_FLAGS_<CONFIG>).")
  else()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(LUMICE_MSVC_STATIC_CRT ON)
  endif()
elseif(MSVC AND BUILD_SHARED_LIBS)
  # The shared flavor is on the DYNAMIC CRT, and this line is what says so. Before it
  # existed the flavor landed on the same value through CMake's own default — the static
  # branch above simply did not run and LUMICE_MSVC_STATIC_CRT stayed at its initial OFF —
  # which is a behaviour nobody had written down. The Windows release now ships this
  # flavor (a baseline cl.exe shell loading a clang-cl x86-64-v3 engine DLL, see
  # .github/workflows/release.yml), and a shell and an engine DLL built by two compilers
  # must agree on the CRT to share heap allocations across the boundary; "both happened to
  # default to /MD" is the wrong kind of agreement to build a release on. Setting the value
  # explicitly changes nothing about what is compiled; it makes the choice readable and
  # keeps a future edit to the static branch from silently moving this one. The same
  # baked-flag caveat as the static branch applies: an explicit CMAKE_<LANG>_FLAGS_<CONFIG>
  # override takes precedence over this variable, and this file does not re-detect it here.
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT result OUTPUT output)

# IPO is on for every static Release build and, of the shared ones, for clang-cl and for GCC
# on Linux — the two toolchains that build a release's shared engine. The shared flavor
# originally went without IPO everywhere because GCC's LTO once emitted R_X86_64_TPOFF32
# relocations for spdlog's thread_local variables, which a shared object cannot carry. Both
# releases now ship the shared flavor, and going without costs the same thing on each:
#   - Windows reference box, release bench config: the x86-64-v3 engine DLL built without
#     -flto=thin ran the single-worker pass at 0.77 M rays/s against 1.13 M for the static
#     exe (multi-worker at parity); the same DLL with thin-LTO ran 1.13 M.
#   - Linux reference box (GCC 13.3), same config: the x86-64-v4 engine .so without LTO ran
#     the single-worker pass at 1.50x the static baseline exe, below the 1.9x the release
#     is held to and that the static v4 exe used to deliver; the same .so with -flto=auto
#     ran 2.04x. The multi-worker pass was 1.12x / 1.13x, the memory-bound ceiling the
#     static split measured before, unchanged by LTO.
# In both cases the whole gap is LTO, none of it the library boundary. GCC 13's LTO links
# the shared engine cleanly (only DTPOFF64 relocations remain, which are fine in a shared
# object), so the TPOFF32 exclusion no longer holds for the toolchain a release uses; the
# CI shared leg builds with the same GCC and would surface a return of it as a link error.
# clang-cl's LTO objects worked with WINDOWS_EXPORT_ALL_SYMBOLS (its .def generator read
# them), so that tree gets IPO too. Real cl.exe did NOT: its /GL objects hold no symbol
# table CMake's __create_def can read, and the DLL linked "successfully" with an EMPTY export
# table — a baseline engine with zero exports, measured, not a hypothesis — so the cl.exe
# baseline tree was left without IPO until the export surface was explicit rather than
# harvested. It now is (lumice_apply_export_list below: a .def generated from the surface
# headers, no symbol harvesting), so that reason no longer holds; turning IPO on for the cl.exe shared
# tree is still a separate, measured change and is not made here. Apple Clang shared builds
# stay excluded: no release ships them, and they have not been measured either way.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(_lumice_gcc_linux TRUE)
else()
  set(_lumice_gcc_linux FALSE)
endif()
if(result AND "${CMAKE_BUILD_TYPE}" STREQUAL "Release"
   AND (NOT BUILD_SHARED_LIBS OR LUMICE_CLANG_CL OR _lumice_gcc_linux))
  message(STATUS "IPO supported. Enable inter-procedural optimization.")
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

if(BUILD_SHARED_LIBS)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# Directories
# The build flavor is a pure function of BUILD_SHARED_LIBS, never a knob of its
# own. It is a path segment because these output directories are anchored on
# CMAKE_SOURCE_DIR, not on the -B tree: without it, a static and a shared build
# write the same paths and overwrite each other, so alternating between the two
# means a full rebuild every time (and a stale-artifact hazard in between).
# With it, the two flavors coexist and each stays warm.
if(BUILD_SHARED_LIBS)
  set(LUMICE_BUILD_FLAVOR "shared")
else()
  set(LUMICE_BUILD_FLAVOR "static")
endif()
set(BUILD_DIR "${CMAKE_SOURCE_DIR}/build")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${BUILD_DIR}/${CMAKE_BUILD_TYPE}/${LUMICE_BUILD_FLAVOR}/lib/${ARCH}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${BUILD_DIR}/${CMAKE_BUILD_TYPE}/${LUMICE_BUILD_FLAVOR}/lib/${ARCH}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BUILD_DIR}/${CMAKE_BUILD_TYPE}/${LUMICE_BUILD_FLAVOR}/bin/${ARCH}")

set(PROJ_SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(PROJ_TEST_DIR "${CMAKE_SOURCE_DIR}/test")
set(PROJ_ROOT "${CMAKE_SOURCE_DIR}")


# ==================================================================================================
# Find modules
find_package(Threads REQUIRED)  # GoogleTest needs it
if(BUILD_GUI AND NOT WIN32)
  find_package(OpenGL REQUIRED)
endif()

# ==================================================================================================
# CPM.cmake — lightweight dependency manager (wrapper around FetchContent)
# Cache CPM downloads outside cmake_build so clean builds (-k) don't re-download, and — since the
# default below is a machine-level directory rather than a per-source-tree one — so that a fresh
# clone or a new `git worktree` reuses whatever this machine already downloaded instead of
# re-fetching every dependency source. Three-tier fallback; the last tier is the old repo-local
# behaviour, so configure can never fail just because no home directory is discoverable.
if(NOT DEFINED CPM_SOURCE_CACHE AND NOT DEFINED ENV{CPM_SOURCE_CACHE})
  if(DEFINED ENV{HOME})
    set(_lumice_cpm_cache_default "$ENV{HOME}/.cache/lumice-cpm")
  elseif(DEFINED ENV{USERPROFILE})  # Windows shells that do not set HOME
    set(_lumice_cpm_cache_default "$ENV{USERPROFILE}/.cache/lumice-cpm")
  else()
    set(_lumice_cpm_cache_default "${CMAKE_SOURCE_DIR}/build/cpm_cache")
  endif()
  set(CPM_SOURCE_CACHE "${_lumice_cpm_cache_default}"
      CACHE PATH "Directory to cache CPM dependency sources (machine-level and shared across \
worktrees/clones by default; override with the CPM_SOURCE_CACHE env var, as CI does)")
  unset(_lumice_cpm_cache_default)
endif()
include(cmake/CPM.cmake)

# nlohmann/json: download only headers (~255KB) instead of full source (~6.7MB)
CPMAddPackage(
  NAME nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.10.5/include.zip
  URL_HASH SHA256=b94997df68856753b72f0d7a3703b7d484d4745c567f3584ef97c96c25a5798e
)
if(nlohmann_json_ADDED)
  add_library(nlohmann_json INTERFACE)
  add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
  target_include_directories(nlohmann_json INTERFACE ${nlohmann_json_SOURCE_DIR}/include)
endif()

# spdlog: header-only mode (skip building the static library)
CPMAddPackage(
  NAME spdlog
  URL https://github.com/gabime/spdlog/archive/refs/tags/v1.15.0.tar.gz
  URL_HASH SHA256=9962648c9b4f1a7bbc76fd8d9172555bad1871fdb14ff4f842ef87949682caa5
  OPTIONS "SPDLOG_BUILD_SHARED OFF" "SPDLOG_ENABLE_PCH OFF" "SPDLOG_BUILD_EXAMPLE OFF"
)
if(spdlog_ADDED)
  set_target_properties(spdlog PROPERTIES EXCLUDE_FROM_ALL YES)
  # scrum-309: back-port the `#if FMT_USE_FLOAT128` guard on fmt's float128
  # formatter so the bundled fmt compiles under nvcc on Windows. fmt v11 declares
  # `formatter<detail::float128>` unguarded; when FMT_USE_FLOAT128=0 (no
  # __float128) float128 aliases void, and nvcc's device (EDG) frontend on
  # Windows eagerly instantiates formatter<void> → "reference to void is not
  # allowed". Patch the cached source in place (idempotent; no-op once guarded,
  # harmless on Linux where the specialization stays enabled). Done inline with
  # CPM's ${spdlog_SOURCE_DIR} rather than a PATCH_COMMAND so the working
  # directory is unambiguous.
  set(_fmt_fh "${spdlog_SOURCE_DIR}/include/spdlog/fmt/bundled/format.h")
  if(EXISTS "${_fmt_fh}")
    file(READ "${_fmt_fh}" _fmt_c)
    string(FIND "${_fmt_c}" "#if FMT_USE_FLOAT128\ntemplate <typename Char>\nstruct formatter<detail::float128" _fmt_guarded)
    if(_fmt_guarded EQUAL -1)
      string(REPLACE
        "template <typename Char>\nstruct formatter<detail::float128, Char>\n    : detail::native_formatter<detail::float128, Char,\n                               detail::type::float_type> {};"
        "#if FMT_USE_FLOAT128\ntemplate <typename Char>\nstruct formatter<detail::float128, Char>\n    : detail::native_formatter<detail::float128, Char,\n                               detail::type::float_type> {};\n#endif"
        _fmt_c "${_fmt_c}")
      file(WRITE "${_fmt_fh}" "${_fmt_c}")
      message(STATUS "scrum-309: guarded bundled fmt formatter<float128> for nvcc/Windows")
    endif()
  endif()
  if(MSVC AND LUMICE_CUDA_ENABLED)
    # scrum-309 (Windows CUDA build): spdlog's CMake adds bare MSVC flags
    # `/utf-8` and `/Zc:__cplusplus` as INTERFACE compile options on
    # spdlog_header_only WITHOUT the `$<NOT:$<COMPILE_LANGUAGE:CUDA>>` guard it
    # uses for /wd4251. When a CUDA (.cu) TU links spdlog, these `/`-prefixed
    # flags reach nvcc, which on Windows misreads them as input file paths and
    # dies ("nvcc fatal: A single input file is required for a non-link phase
    # when an output file is specified"). Re-scope the interface options to the
    # CXX language so they never reach nvcc; CMake still supplies the MSVC
    # runtime to the CUDA host compiler via its own -Xcompiler handling.
    get_target_property(_spdlog_io spdlog_header_only INTERFACE_COMPILE_OPTIONS)
    if(_spdlog_io)
      set(_spdlog_io_cxx "")
      foreach(_opt IN LISTS _spdlog_io)
        list(APPEND _spdlog_io_cxx "$<$<COMPILE_LANGUAGE:CXX>:${_opt}>")
      endforeach()
      set_target_properties(spdlog_header_only PROPERTIES
        INTERFACE_COMPILE_OPTIONS "${_spdlog_io_cxx}")
    endif()
  endif()
endif()

# tl-expected: header-only expected<T,E> for C++17
CPMAddPackage(
  NAME tl-expected
  URL https://github.com/TartanLlama/expected/archive/refs/tags/v1.1.0.tar.gz
  URL_HASH SHA256=1db357f46dd2b24447156aaf970c4c40a793ef12a8a9c2ad9e096d9801368df6
  OPTIONS "EXPECTED_BUILD_TESTS OFF"
)

# stb: header-only image writing (only stb_image_write.h used)
CPMAddPackage(
  NAME stb
  URL https://github.com/nothings/stb/archive/f1c79c02822848a9bed4315b12c8c8f3761e1296.tar.gz
  URL_HASH SHA256=a39b0f2d96d9b64171ac1fd73295b7c5866f904e653b379da886d050a6900db9
  DOWNLOAD_ONLY YES
)

# Roboto Medium — the font core rasterizes annotation labels with (src/core/annotation_font.cpp,
# via stb_truetype above). Fetched as a SINGLE FILE rather than through the imgui tree the GUI
# embeds the identical bytes from, and OUTSIDE the BUILD_GUI block below, because lumice_obj
# (core + CLI) must compile in a CLI-only build where ${imgui_SOURCE_DIR} does not exist at all.
# Same tag, same path, same SHA256 as the GUI's copy: both drawers rasterize one typeface, so a
# future GUI/CLI text comparison has one fewer way to differ. DOWNLOAD_NO_EXTRACT because a .ttf
# is not an archive.
CPMAddPackage(
  NAME roboto_font
  URL https://raw.githubusercontent.com/ocornut/imgui/v1.91.8-docking/misc/fonts/Roboto-Medium.ttf
  URL_HASH SHA256=8559132c89ad51d8a2ba5b171887a44a7ba93776e205f553573de228e64b45f8
  DOWNLOAD_ONLY YES
  DOWNLOAD_NO_EXTRACT YES
)

# GUI dependencies (only when BUILD_GUI is ON)
if(BUILD_GUI)
  # The GUI's vendored libraries (GLFW, nfd, glad) are linked STATICALLY into LumiceGUI in
  # every flavor. BUILD_SHARED_LIBS is a global switch that every add_subdirectory'd dependency
  # honours too, and under the shared flavor it made all three come out as DLLs — glfw3.dll,
  # nfd.dll, glad_gl.dll — that LumiceGUI.exe imports implicitly at process start. Nothing
  # installs or packages them, so the executable failed to start with STATUS_DLL_NOT_FOUND
  # (0xC0000135) the first time the Windows release was built as the shared flavor, while the
  # workflow's own merge check stayed green: it looks for the files it expects, not for the
  # ones the executable does. The shared flavor exists for exactly one library, the engine;
  # the switch is therefore shadowed OFF for this block only, as a normal variable over the
  # cache entry, and unshadowed again right after so the engine target below still sees the
  # configure's value. (GLFW has its own GLFW_LIBRARY_TYPE knob, nfd has none, and glad's
  # glad_add_library takes STATIC — one mechanism for all three rather than three spellings.)
  set(BUILD_SHARED_LIBS OFF)

  # GLFW: windowing and input
  CPMAddPackage(
    NAME glfw
    GITHUB_REPOSITORY glfw/glfw
    GIT_TAG 3.4
    OPTIONS "GLFW_BUILD_DOCS OFF" "GLFW_BUILD_EXAMPLES OFF" "GLFW_BUILD_TESTS OFF"
  )

  # Dear ImGui: immediate-mode GUI (docking branch, no CMake support — download only)
  CPMAddPackage(
    NAME imgui
    GITHUB_REPOSITORY ocornut/imgui
    GIT_TAG v1.91.8-docking
    DOWNLOAD_ONLY YES
  )

  # nativefiledialog-extended: native OS file dialogs
  CPMAddPackage(
    NAME nfd
    GITHUB_REPOSITORY btzy/nativefiledialog-extended
    GIT_TAG v1.2.1
  )

  # Dear ImGui Test Engine (no CMake support — download only)
  CPMAddPackage(
    NAME imgui_test_engine
    GITHUB_REPOSITORY ocornut/imgui_test_engine
    GIT_TAG v1.91.8
    DOWNLOAD_ONLY YES
  )

  # IconFontCppHeaders: ICON_FA_* macros for FontAwesome 6 (header-only).
  # Pinned to a specific commit SHA (rather than `main`) for reproducible builds.
  CPMAddPackage(
    NAME IconFontCppHeaders
    GITHUB_REPOSITORY juliettef/IconFontCppHeaders
    GIT_TAG 3ee7f3d295ae773c0046db8d7b89b886eb2526de
    DOWNLOAD_ONLY YES
  )

  # Font Awesome 6 Free — TTF asset is consumed at configure time by
  # src/gui/CMakeLists.txt (file(READ HEX)) and embedded into the GUI binary.
  CPMAddPackage(
    NAME font_awesome
    GITHUB_REPOSITORY FortAwesome/Font-Awesome
    GIT_TAG 6.7.2
    DOWNLOAD_ONLY YES
  )

  # GLAD2: OpenGL loader for Windows (macOS/Linux use native GL headers)
  # GLAD2 v2.0.8 has its CMakeLists.txt in cmake/ subdirectory (root is a Python package).
  # SOURCE_SUBDIR tells CPM to run add_subdirectory on cmake/ instead of root.
  if(WIN32)
    CPMAddPackage(
      NAME glad
      GITHUB_REPOSITORY Dav1dde/glad
      GIT_TAG v2.0.8
      SOURCE_SUBDIR cmake
    )
    glad_add_library(glad_gl REPRODUCIBLE API gl:core=3.3)
  endif()

  # Unshadow: the normal variable set above goes away and the cache entry (the configure's
  # -DBUILD_SHARED_LIBS) is visible again for everything after this block.
  unset(BUILD_SHARED_LIBS)
endif()

# ==================================================================================================
# Add some definitions
if(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Linux")
  add_compile_definitions(OS_LINUX)
elseif(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Darwin")
  add_compile_definitions(OS_MAC)
elseif(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows")
  add_compile_definitions(OS_WIN)
endif()

# ==================================================================================================
# Set submodules
set(lumice_include
  "${PROJ_SRC_DIR}")

set(lumice_config_src
  "${PROJ_SRC_DIR}/config/color_class_table.cpp"
  "${PROJ_SRC_DIR}/config/color_gate_table.cpp"
  "${PROJ_SRC_DIR}/config/component_table.cpp"
  "${PROJ_SRC_DIR}/config/config_manager.cpp"
  "${PROJ_SRC_DIR}/config/crystal_config.cpp"
  "${PROJ_SRC_DIR}/config/filter_config.cpp"
  "${PROJ_SRC_DIR}/config/light_config.cpp"
  "${PROJ_SRC_DIR}/config/proj_config.cpp"
  "${PROJ_SRC_DIR}/config/raypath_color_config.cpp"
  "${PROJ_SRC_DIR}/config/raypath_validation.cpp"
  "${PROJ_SRC_DIR}/config/render_config.cpp"
  "${PROJ_SRC_DIR}/config/sim_data.cpp")

set(lumice_core_src
  "${PROJ_SRC_DIR}/core/annotation_font.cpp"
  "${PROJ_SRC_DIR}/core/annotation_overlay.cpp"
  "${PROJ_SRC_DIR}/core/backend/cpu_trace_backend.cpp"
  "${PROJ_SRC_DIR}/core/chain_id_table.cpp"
  "${PROJ_SRC_DIR}/core/crystal.cpp"
  "${PROJ_SRC_DIR}/core/crystal_param.cpp"
  "${PROJ_SRC_DIR}/core/device_filter_desc.cpp"
  "${PROJ_SRC_DIR}/core/filter_spec.cpp"
  "${PROJ_SRC_DIR}/core/geo3d.cpp"
  "${PROJ_SRC_DIR}/core/geo3d_closedform.cpp"
  "${PROJ_SRC_DIR}/core/lat_lut.cpp"
  "${PROJ_SRC_DIR}/core/math.cpp"
  "${PROJ_SRC_DIR}/core/miller_wedge.cpp"
  "${PROJ_SRC_DIR}/core/optics.cpp"
  "${PROJ_SRC_DIR}/core/parallel_rows.cpp"
  "${PROJ_SRC_DIR}/core/projection.cpp"
  "${PROJ_SRC_DIR}/core/raypath.cpp"
  "${PROJ_SRC_DIR}/core/simulator.cpp"
  "${PROJ_SRC_DIR}/core/worker_projection.cpp")

# Metal backend (Apple only). task-metal-build-time-metallib (#283): the GPU
# kernel single source of truth is `src/core/metal/lumice_trace.metal`. CMake
# precompiles it to a .metallib at build time and embeds the bytes + a C-string
# fallback into the binary via scripts/embed_metal_artifacts.py. The runtime
# loads the embedded metallib via newLibraryWithData, bypassing macOS 26.5's
# broken MSL source frontend; newLibraryWithSource is retained as fallback
# (disabled when LUMICE_DISABLE_METAL_SOURCE_COMPILE=1, which simulates 26.5
# for the AC2 regression test).
if(APPLE)
  list(APPEND lumice_core_src
    "${PROJ_SRC_DIR}/core/metal_filter_match_src.mm"
    "${PROJ_SRC_DIR}/core/backend/metal_trace_backend.mm")
  set_source_files_properties(
    "${PROJ_SRC_DIR}/core/backend/metal_trace_backend.mm"
    "${PROJ_SRC_DIR}/core/metal_filter_match_src.mm"
    PROPERTIES COMPILE_FLAGS "-fobjc-arc")

  # --- Build-time MSL → metallib → embedded C headers --------------------------
  # Tools (probed inside APPLE guard so non-Apple builds don't require Python3).
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  find_program(XCRUN_EXECUTABLE xcrun REQUIRED)

  set(LUMICE_METAL_SRC      "${PROJ_SRC_DIR}/core/metal/lumice_trace.metal")
  set(LUMICE_METAL_GEN_DIR  "${CMAKE_BINARY_DIR}/generated")
  set(LUMICE_METAL_AIR      "${LUMICE_METAL_GEN_DIR}/lumice_trace.air")
  set(LUMICE_METAL_METALLIB "${LUMICE_METAL_GEN_DIR}/lumice_trace.metallib")
  set(LUMICE_METAL_BYTES_H  "${LUMICE_METAL_GEN_DIR}/lumice_trace_metallib_embed.h")
  set(LUMICE_METAL_SRC_H    "${LUMICE_METAL_GEN_DIR}/lumice_trace_src_embed.h")
  set(LUMICE_EMBED_SCRIPT   "${CMAKE_SOURCE_DIR}/scripts/embed_metal_artifacts.py")

  # Compile flags MUST stay byte-identical to EnsurePso's runtime
  # MTLCompileOptions (MSL 3.0 + fno-fast-math/MTLMathModeSafe) so the offline
  # metallib produces the same numerical result as the runtime source path.
  # `-arch air64` prevents fat-AIR output (xcrun metallib rejects fat input).
  # Shared single-source math headers consumed by both MSL and host C++.
  # Listed in DEPENDS so any change re-triggers metallib compile + embed.
  set(LUMICE_METAL_SHARED_HDRS
      "${PROJ_SRC_DIR}/core/shared/lm_shims.h"
      "${PROJ_SRC_DIR}/core/shared/projection_shared.h"
      "${PROJ_SRC_DIR}/core/shared/optics_shared.h"
      "${PROJ_SRC_DIR}/core/shared/traversal_shared.h"
      "${PROJ_SRC_DIR}/core/shared/pcg_shared.h"
      "${PROJ_SRC_DIR}/core/shared/accum_shared.h")

  # scrum-306.1 H7: optional shader debug info for the GPU profiler (off by default).
  set(LUMICE_METAL_DEBUG_FLAGS "")
  if(LUMICE_METAL_SHADER_PROFILE)
    set(LUMICE_METAL_DEBUG_FLAGS -frecord-sources -gline-tables-only)
    message(STATUS "[metal] LUMICE_METAL_SHADER_PROFILE=ON → embedding shader sources/line-tables for GPU profiler")
  endif()

  add_custom_command(
    OUTPUT  "${LUMICE_METAL_AIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${LUMICE_METAL_GEN_DIR}"
    COMMAND ${XCRUN_EXECUTABLE} metal
            -arch air64
            -std=metal3.0
            -fno-fast-math
            -mmacosx-version-min=13.0
            ${LUMICE_METAL_DEBUG_FLAGS}
            -c "${LUMICE_METAL_SRC}"
            -o "${LUMICE_METAL_AIR}"
    DEPENDS "${LUMICE_METAL_SRC}" ${LUMICE_METAL_SHARED_HDRS}
    COMMENT "Compiling MSL → AIR (lumice_trace.metal → lumice_trace.air)"
    VERBATIM)

  add_custom_command(
    OUTPUT  "${LUMICE_METAL_METALLIB}"
    COMMAND ${XCRUN_EXECUTABLE} metallib
            "${LUMICE_METAL_AIR}"
            -o "${LUMICE_METAL_METALLIB}"
    DEPENDS "${LUMICE_METAL_AIR}"
    COMMENT "Linking metallib (lumice_trace.air → lumice_trace.metallib)"
    VERBATIM)

  add_custom_command(
    OUTPUT  "${LUMICE_METAL_BYTES_H}" "${LUMICE_METAL_SRC_H}"
    COMMAND ${Python3_EXECUTABLE} "${LUMICE_EMBED_SCRIPT}"
            --metallib   "${LUMICE_METAL_METALLIB}"
            --metal-src  "${LUMICE_METAL_SRC}"
            --out-dir    "${LUMICE_METAL_GEN_DIR}"
    DEPENDS "${LUMICE_METAL_METALLIB}" "${LUMICE_METAL_SRC}" "${LUMICE_EMBED_SCRIPT}"
            ${LUMICE_METAL_SHARED_HDRS}
    COMMENT "Embedding metallib + source into C headers"
    VERBATIM)

  add_custom_target(lumice_metal_shaders
    DEPENDS "${LUMICE_METAL_BYTES_H}" "${LUMICE_METAL_SRC_H}")
endif()

# CUDA backend (off by default). scrum-cuda-backend-mvp subtask 3:
# - nvcc 11.6 (dev49 docker image) is the lowest supported toolchain;
#   CUDA 12.x toolkits also work (still support compute_61).
# - PTX floor = compute_61 (Pascal, 2016+); driver JITs to native sm on any
#   GPU >= sm_61, including future cards.
# - Multi-arch fatbin (scrum-310.2): on top of the compute_61 PTX floor, emit
#   native SASS for the common modern cards (Turing through Blackwell, i.e.
#   RTX 20/30/40/50) so they skip first-launch JIT and the CUDA reference box
#   bench runs native (not PTX-JIT'd) for fidelity.
# - Mac/Windows host builds without nvcc keep BUILD_GUI/perf paths intact when
#   LUMICE_CUDA_ENABLED is OFF (no enable_language(CUDA) at all).
if(LUMICE_CUDA_ENABLED)
  enable_language(CUDA)
  find_package(CUDAToolkit REQUIRED)
  set(CMAKE_CUDA_STANDARD 17)
  set(CMAKE_CUDA_STANDARD_REQUIRED ON)
  if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
    # PTX floor `61-virtual` emits compute_61 PTX only (no SASS); the driver
    # JITs it to native sm on any GPU >= sm_61, including future cards. Owner
    # decision 2026-06-25 (296.1): PTX floor widens forward-compat to
    # Pascal/Turing/Ampere/Ada/Hopper/future; the older `86-virtual` default
    # was an explore #294 dev49 probe artifact that silently excluded
    # Turing/Pascal.
    set(_lumice_cuda_archs "61-virtual")
    # Opportunistic native SASS for common cards: skips first-launch JIT and
    # gives bench fidelity. Each real arch is gated on the toolkit version that
    # introduced that sm, so older toolchains (e.g. an 11.6 / 11.7 docker
    # image) still build — they just emit fewer `-real` archs and fall back to
    # PTX JIT. Distribution/CI builds use a modern toolkit (CI: 12.9, the last
    # 12.x minor) and get the full fatbin. The PTX floor guarantees universal
    # compat regardless.
    # Toolkit >= 12.8 prints a deprecation warning for compute_61 (the PTX
    # floor). That is the expected, accepted cost of keeping the Pascal floor
    # — not a defect: do not silence it with -Wno-deprecated-gpu-targets and
    # do not drop 61-virtual over it. It is not promoted to an error by any
    # flag in this tree (-Werror=switch is CXX-only; there is no /WX).
    # CUDA 13 removes compute_61 outright, which is why the CUDA major stays
    # pinned at 12 in ci.yml / release.yml.
    if(CUDAToolkit_VERSION VERSION_GREATER_EQUAL "10.0")
      list(APPEND _lumice_cuda_archs "75-real")  # Turing (RTX 20 / GTX 16)
    endif()
    if(CUDAToolkit_VERSION VERSION_GREATER_EQUAL "11.1")
      list(APPEND _lumice_cuda_archs "86-real")  # Ampere (RTX 30)
    endif()
    if(CUDAToolkit_VERSION VERSION_GREATER_EQUAL "11.8")
      list(APPEND _lumice_cuda_archs "89-real")  # Ada Lovelace (RTX 40)
    endif()
    if(CUDAToolkit_VERSION VERSION_GREATER_EQUAL "12.8")
      list(APPEND _lumice_cuda_archs "120-real")  # Blackwell (RTX 50)
    endif()
    set(CMAKE_CUDA_ARCHITECTURES "${_lumice_cuda_archs}" CACHE STRING "CUDA archs")
  endif()
  list(APPEND lumice_core_src
    "${PROJ_SRC_DIR}/core/backend/cuda_trace_backend.cu")
  message(STATUS "CUDA backend enabled (arch=${CMAKE_CUDA_ARCHITECTURES})")
endif()

set(lumice_server_src
  "${PROJ_SRC_DIR}/server/anchor_consumer.cpp"
  "${PROJ_SRC_DIR}/server/c_api.cpp"
  "${PROJ_SRC_DIR}/server/component_compositor.cpp"
  "${PROJ_SRC_DIR}/server/render.cpp"
  "${PROJ_SRC_DIR}/server/server.cpp"
  "${PROJ_SRC_DIR}/server/stats.cpp"
  "${PROJ_SRC_DIR}/server/raypath_histogram_consumer.cpp")

set(lumice_util_src
  "${PROJ_SRC_DIR}/util/color_space.cpp"
  "${PROJ_SRC_DIR}/util/cpu_info.cpp"
  "${PROJ_SRC_DIR}/util/env_knobs.cpp"
  "${PROJ_SRC_DIR}/util/illuminant.cpp"
  "${PROJ_SRC_DIR}/util/threading_pool.cpp")

# --- Build-time TTF -> embedded byte array (core's label font) ----------------------------------
# The same scripts/embed_binary.py the GUI drives for its two fonts, pointed at core's namespace:
# lumice_obj is built with BUILD_GUI off too, where lumice::gui has no other member. The bytes end
# up in the binary, so a CLI render draws text with no font file present on the machine.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(LUMICE_LABEL_FONT_TTF "${roboto_font_SOURCE_DIR}/Roboto-Medium.ttf")
set(LUMICE_LABEL_FONT_EMBED_CPP "${CMAKE_BINARY_DIR}/generated/label_font_embed.cpp")
if(NOT EXISTS "${LUMICE_LABEL_FONT_TTF}")
  message(FATAL_ERROR "[core] Label font not found at ${LUMICE_LABEL_FONT_TTF}. "
                      "Check the roboto_font CPM dependency above.")
endif()
add_custom_command(
    OUTPUT "${LUMICE_LABEL_FONT_EMBED_CPP}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/generated"
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_SOURCE_DIR}/scripts/embed_binary.py"
            "${LUMICE_LABEL_FONT_TTF}"
            "${LUMICE_LABEL_FONT_EMBED_CPP}"
            "kLabelFont"
            "lumice::annotation"
    DEPENDS "${LUMICE_LABEL_FONT_TTF}" "${CMAKE_SOURCE_DIR}/scripts/embed_binary.py"
    COMMENT "[core] Embedding Roboto-Medium.ttf -> label_font_embed.cpp"
    VERBATIM)

# --- Product version string ------------------------------------------------------------------------
# project(VERSION) above is the single source (scripts/version.py reads and writes it); this bakes
# it -- plus the "-dev" suffix unless LUMICE_RELEASE_BUILD -- into a build-tree-only header whose
# one consumer is src/server/c_api.cpp's LUMICE_GetVersionString(). Plain configure_file: it is a
# string substitution, so unlike the font embed above it needs no custom command or Python.
if(LUMICE_RELEASE_BUILD)
  set(LUMICE_PRODUCT_VERSION "${PROJECT_VERSION}")
else()
  set(LUMICE_PRODUCT_VERSION "${PROJECT_VERSION}-dev")
endif()
configure_file(
  "${PROJ_SRC_DIR}/server/version_gen.hpp.in"
  "${CMAKE_BINARY_DIR}/generated/server/version_gen.hpp"
  @ONLY)

set(lumice_src
  ${lumice_config_src} ${lumice_core_src}
  ${lumice_server_src} ${lumice_util_src}
  "${LUMICE_LABEL_FONT_EMBED_CPP}")

# --- Layer manifest: the foundation layer is its own OBJECT library ---------------------------
# cmake/lumice_layers.cmake is the one statement of which layer owns each src/ file; the
# layer-inversion rule in scripts/check_policies.py enforces it on every #include. Its foundation
# list is the set of engine sources that include nothing above foundation, so it is exactly what a
# library can be built from without the rest of the engine: lumice_foundation_obj below compiles
# those sources and lumice_obj compiles the others. liblumice_analytic links foundation and the
# analytic kernel only — the engine does not reach it structurally, rather than being linked in
# and stripped again. The source lists above stay the one list of what the engine compiles; the
# foundation files are subtracted from it here, never written a second time.
include("${CMAKE_SOURCE_DIR}/cmake/lumice_layers.cmake")
set(lumice_foundation_src "")
foreach(_rel IN LISTS LUMICE_LAYER_foundation_FILES)
  set(_abs "${PROJ_SRC_DIR}/${_rel}")
  if(NOT EXISTS "${_abs}")
    message(FATAL_ERROR "[layers] cmake/lumice_layers.cmake lists ${_rel}, which does not exist.")
  endif()
  get_filename_component(_ext "${_rel}" LAST_EXT)
  if(NOT _ext MATCHES "^\\.(cpp|hpp|h)$")
    # An .mm or .cu entry would be compiled by nothing: lumice_foundation_obj takes .cpp only.
    message(FATAL_ERROR "[layers] foundation may hold only .cpp/.hpp/.h sources; ${_rel} is not one.")
  endif()
  if(_ext STREQUAL ".cpp")
    list(FIND lumice_src "${_abs}" _idx)
    if(_idx EQUAL -1)
      message(FATAL_ERROR "[layers] foundation lists ${_rel}, but no engine source list above "
                          "compiles it; add it there (the manifest assigns a layer, it does not "
                          "add a source).")
    endif()
    list(APPEND lumice_foundation_src "${_abs}")
  endif()
endforeach()
list(REMOVE_ITEM lumice_src ${lumice_foundation_src})

# ==================================================================================================
# OBJECT library — compile sources once, reuse .o files across targets (library, test, bench).
# test/bench link lumice_obj directly so all symbols are accessible regardless of
# -fvisibility=hidden (visibility only affects shared-library export, not executable linking).
add_library(lumice_obj OBJECT ${lumice_src})
# Usage requirements only: an OBJECT library linking another OBJECT library gets its include
# directories, definitions and dependencies, never its objects. Every final target that links
# lumice_obj therefore links lumice_foundation_obj by name as well (the same rule the kernel and
# the single-path module below already follow) — once, and only there, so each binary holds one
# copy of the foundation objects and of the logger's statics among them.
target_link_libraries(lumice_obj PUBLIC lumice_foundation_obj)
target_include_directories(lumice_obj
  PUBLIC
    $<BUILD_INTERFACE:${PROJ_SRC_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${lumice_include})
target_link_libraries(lumice_obj PUBLIC nlohmann_json::nlohmann_json spdlog::spdlog_header_only tl::expected Threads::Threads)
# stb_truetype.h, for core/annotation_font.cpp. PRIVATE: the rasterizer is an implementation
# detail of the label drawing, and no public header names an stb type.
target_include_directories(lumice_obj PRIVATE "${stb_SOURCE_DIR}")
# generated/server/version_gen.hpp (configure_file above). PRIVATE and outside the if(APPLE) block
# below: every platform's c_api.cpp includes it, and no public header does.
target_include_directories(lumice_obj PRIVATE "${CMAKE_BINARY_DIR}/generated")
if(APPLE)
  target_link_libraries(lumice_obj PUBLIC "-framework Metal" "-framework Foundation")
  # task-metal-build-time-metallib (#283): metal_trace_backend.mm #includes the
  # generated metallib + source headers; depend on the custom target so the
  # headers exist before the .mm compiles, and add the generated dir to the
  # include path. PRIVATE — these headers are not part of the public API.
  add_dependencies(lumice_obj lumice_metal_shaders)
  target_include_directories(lumice_obj PRIVATE "${LUMICE_METAL_GEN_DIR}")
endif()

if(LUMICE_CUDA_ENABLED)
  target_link_libraries(lumice_obj PUBLIC CUDA::cudart)
  target_compile_definitions(lumice_obj PUBLIC LUMICE_CUDA_ENABLED=1)
  if(MSVC)
    # Windows CUDA build (scrum-309).
    # spdlog/fmt require `/utf-8` (Unicode static_assert) and `/Zc:__cplusplus`
    # (correct __cplusplus so fmt picks the right code path; without it the
    # float128 native_formatter instantiates against `void`). The spdlog block
    # above strips these from the CUDA TU because nvcc misreads the bare `/`
    # flags as input file paths; re-supply them to the host compiler via
    # -Xcompiler (nvcc forwards them to cl) so the CUDA host phase matches the
    # plain-CXX build.
    target_compile_options(lumice_obj PRIVATE
      $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=/utf-8,/Zc:__cplusplus>)
    # fmt (bundled in spdlog) trips two MSVC×nvcc-only issues during the CUDA
    # TU's device (EDG) frontend pass, which `/utf-8` (a cl flag) cannot reach:
    #   FMT_UNICODE: EDG reads source under the system code page (936) on this
    #     host, so fmt's "Unicode support requires /utf-8" static_assert fires.
    #   FMT_USE_FLOAT128: nvcc emulates gcc on the host and defines
    #     __SIZEOF_FLOAT128__, so fmt enables its float128 formatter, but the
    #     MSVC backend has no __float128 (float128 aliases void) →
    #     "reference to void is not allowed".
    # Both features are unused on Windows (logs are ASCII; no float128); disable
    # them for the CUDA TU. FMT_USE_FLOAT128 is already 0 under plain cl, so
    # only FMT_UNICODE differs from the CXX TUs (acceptable: ASCII-only logs).
    target_compile_definitions(lumice_obj PRIVATE
      $<$<COMPILE_LANGUAGE:CUDA>:FMT_UNICODE=0 FMT_USE_FLOAT128=0>)
  endif()
endif()

# Single source of truth for "this compile actually applies a -march flag", shared by the
# GCC/Clang path (the if(NOT MSVC) block below) and the clang-cl path (the if(LUMICE_CLANG_CL)
# block after it). One function rather than two inline copies: every future ISA-tier change
# would otherwise be a two-site edit, and the site that gets missed is the one whose `isa`
# key then quietly reports the wrong tier on one platform — the same failure shape as the
# probe that found clang-cl at all, where every arm reported "baseline" because the macro was
# gated on if(MSVC). Both the compile-option gate and the LUMICE_ISA_LEVEL_STR macro that
# reports the tier in --benchmark output reference the one _active_cond expression, so the
# flag and the number's provenance cannot independently drift out of sync. (Its inner
# $<CONFIG:Release> is redundant with the outer $<$<CONFIG:Release>:...> wrapper; that
# redundancy is the price of the two gates being literally the same expression.)
# Scoped to `target`'s own compile: the -march flag and the macro are both PRIVATE (each
# object file is compiled for one tier, and the macro is read only inside the engine — see
# the comment at that call).
function(lumice_apply_isa_march target)
  if(LUMICE_ISA_LEVEL STREQUAL "baseline")
    set(_march_flag "")
  else()
    set(_march_flag "-march=${LUMICE_ISA_LEVEL}")
  endif()
  set(_active_cond "$<AND:$<CONFIG:Release>,$<BOOL:${_march_flag}>>")
  target_compile_options(${target} PRIVATE
    $<$<CONFIG:Release>:$<${_active_cond}:${_march_flag}>>)
  # PRIVATE on purpose: the macro is read in exactly one place, LUMICE_GetEngineIsaLevel()
  # in src/server/c_api.cpp (an object of lumice_obj), and the executables ask through that
  # function. It used to be PUBLIC so src/main.cpp could read it directly, and that was the
  # defect: on Windows the engine is a DLL chosen by CPUID at start-up while the executable
  # is compiled once for the baseline tier, so a macro reaching main.cpp named the shell's
  # configure rather than the engine that ran — a v3 engine reported "baseline". Keeping it
  # off the usage requirements is what makes that mistake a compile error rather than a
  # wrong number. The value is the tier name when the -march flag above is actually applied
  # and "baseline" otherwise (Debug, MinSizeRel, or the baseline tier itself), so what the
  # engine reports is what it was compiled with, never what was merely asked for. No
  # counterpart for real MSVC cl.exe: neither block below runs there, the macro is never
  # defined, and c_api.cpp's #else falls back to "baseline".
  target_compile_definitions(${target} PRIVATE
    "LUMICE_ISA_LEVEL_STR=\"$<IF:${_active_cond},${LUMICE_ISA_LEVEL},baseline>\"")
endfunction()

# The engine's compile discipline, one definition for every OBJECT library whose objects end up
# in the engine libraries: lumice_obj, and the analytic kernel and single-path module below, which
# live outside lumice_obj only so that each shared library links exactly the objects it needs.
# A second hand-written copy would let a module go into liblumice without -fvisibility=hidden or
# without its ISA tier's -march, and nothing would notice.
function(lumice_apply_engine_compile_options target)
  if(NOT MSVC)
    # -Werror=switch is a GCC/Clang host flag; nvcc parses -Werror itself (does
    # NOT forward it) and rejects "switch" as an invalid value. Scope it to CXX so
    # the BackendKind exhaustiveness gate still applies to host TUs while the CUDA
    # (.cu) TU compiles cleanly. The CXX TUs already cover every BackendKind switch.
    target_compile_options(${target} PRIVATE
      -Wall -Wextra $<$<COMPILE_LANGUAGE:CXX>:-Werror=switch> -fexceptions -frtti)
    target_compile_definitions(${target} PRIVATE __STDC_FORMAT_MACROS SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
    # -fvisibility=hidden (Release/MinSizeRel): every engine symbol is hidden except the
    # declarations the public headers mark LUMICE_API / LUMICE_TEST_API / LUMICE_ANALYTIC_API.
    # That marks what a shared library MAY export; what each one DOES export is its own export
    # list (lumice_apply_export_list below), because the same objects go into three libraries
    # with three different surfaces. Debug leaves everything visible for debugging; the export
    # lists still narrow each shared library to its surface there.
    target_compile_options(${target} PRIVATE
      $<$<CONFIG:Debug>:-O0 -g -ggdb>
      $<$<CONFIG:Release>:-ffunction-sections -fdata-sections -fvisibility=hidden>
      $<$<CONFIG:MinSizeRel>:-ffunction-sections -fdata-sections -fvisibility=hidden>)
    lumice_apply_isa_march(${target})
  endif()

  if(LUMICE_CLANG_CL)
    # clang-cl reports MSVC=TRUE but IS a Clang frontend that honors -march: measured on the
    # Windows reference box, -march=x86-64-v3 is ~2.26x real MSVC's release build at W=1, where
    # cl.exe's own /arch flags move nothing (doc/performance-testing.md). Deliberately NOT folded
    # into the if(NOT MSVC) block above: that block also turns on -Wall/-Werror=switch,
    # SPDLOG_ACTIVE_LEVEL=TRACE and -fvisibility=hidden, none of which the Windows release needs
    # or has been validated under clang-cl. The march-flag mapping itself is the shared
    # lumice_apply_isa_march() above, not a second copy here. Nothing else: clang-cl otherwise
    # takes the MSVC platform defaults (/EHsc, /GR, the static CRT chosen in the MSVC block),
    # which is what makes it a drop-in for the .cu host side that still goes through cl.exe.
    lumice_apply_isa_march(${target})
  endif()
endfunction()

lumice_apply_engine_compile_options(lumice_obj)

# The foundation layer (see the manifest block above): math, geometry, optics, the crystal and
# its shape parameters, the logger and the other util/ pieces. The same public include
# directories and dependencies as lumice_obj — nothing the rest of the engine adds privately
# (stb, the generated headers, Metal, CUDA) is needed by a file the manifest calls foundation,
# because the layer gate keeps every such file from including anything above foundation.
add_library(lumice_foundation_obj OBJECT ${lumice_foundation_src})
target_include_directories(lumice_foundation_obj
  PUBLIC
    $<BUILD_INTERFACE:${PROJ_SRC_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${lumice_include})
target_link_libraries(lumice_foundation_obj
  PUBLIC nlohmann_json::nlohmann_json spdlog::spdlog_header_only tl::expected Threads::Threads)
lumice_apply_engine_compile_options(lumice_foundation_obj)

# The analytic kernel (src/analytic/ minus liblumice_analytic's management TU): the evaluator, the
# fiber continuation, the entry measure, component discovery, and their C ABI. Its own OBJECT library because several
# targets link it and lumice_obj must not carry it: liblumice_analytic, the engine libraries (which
# export its C ABI and also call it behind the single-path module below), and
# unit_correctness_test. One list of its sources, here, rather than one per consumer.
set(lumice_analytic_kernel_src
  "${PROJ_SRC_DIR}/analytic/path_evaluation.cpp"
  "${PROJ_SRC_DIR}/analytic/fiber_continuation.cpp"
  "${PROJ_SRC_DIR}/analytic/path_fiber.cpp"
  "${PROJ_SRC_DIR}/analytic/entry_measure.cpp"
  "${PROJ_SRC_DIR}/analytic/discovery.cpp"
  "${PROJ_SRC_DIR}/analytic/path_rank.cpp"
  "${PROJ_SRC_DIR}/analytic/pose_density.cpp"
  "${PROJ_SRC_DIR}/analytic/band_sum.cpp"
  # The capability's C ABI (lumice_analytic_core.h). No static object with a side effect, so it
  # may go wherever the kernel goes; the library-management TU (analytic_lib.cpp) may not.
  "${PROJ_SRC_DIR}/analytic/analytic_capi.cpp")
add_library(lumice_analytic_kernel OBJECT ${lumice_analytic_kernel_src})
# Foundation only: the kernel is the analytic layer, which sits directly on foundation
# (cmake/lumice_layers.cmake), and liblumice_analytic links nothing else of the engine.
target_link_libraries(lumice_analytic_kernel PUBLIC lumice_foundation_obj)
lumice_apply_engine_compile_options(lumice_analytic_kernel)

# The single-path analysis module (src/raypath/: scene config -> the kernel above -> fibers,
# per-pose detail, the sun-direction grid, its JSON form) and its lumice.h entry point. Linked into
# liblumice and liblumice_testapi — the shells reach it only through lumice.h — and NOT into
# liblumice_analytic, whose surface is the kernel alone; that is why it sits outside lumice_obj.
add_library(lumice_raypath_obj OBJECT
  "${PROJ_SRC_DIR}/raypath/scene_to_analytic.cpp"
  "${PROJ_SRC_DIR}/raypath/single_path_analysis.cpp"
  "${PROJ_SRC_DIR}/raypath/single_path_json.cpp"
  "${PROJ_SRC_DIR}/server/c_api_single_path.cpp")
target_link_libraries(lumice_raypath_obj PUBLIC lumice_analytic_kernel lumice_obj)
lumice_apply_engine_compile_options(lumice_raypath_obj)

# The one way a shared library here decides what it exports: a list generated from the headers
# that define its surface by scripts/gen_export_list.py (see its docstring), handed to the linker
# in the platform's own form — a GNU version script, an ld64 -exported_symbols_list, or a Windows
# .def. Per library, not per object: lumice, lumice_testapi and lumice_analytic are all linked
# from lumice_obj's objects, so an export decision compiled into those objects (a dllexport, or
# visibility alone) would give all three the union of their surfaces. The list is a whitelist on
# every platform, so it also replaces WINDOWS_EXPORT_ALL_SYMBOLS, which exported every engine
# symbol from the DLLs.
#
# LINK_DEPENDS makes an edited header relink the library on Linux/macOS (a link option naming a
# file is not a build-graph edge by itself); on Windows the .def is a target source, which is.
# The generator rewrites its output only when the content changes, and Ninja restats custom
# command outputs, so an edit that does not change the function set relinks nothing.
#
# Which headers each library's list is generated from is not decided at the call sites below but
# in cmake/export_surfaces.cmake, one list per library, which the export test and the policy
# checker read too. The arguments are those repo-relative paths.
include("${CMAKE_SOURCE_DIR}/cmake/export_surfaces.cmake")
function(lumice_apply_export_list target)
  set(_headers "")
  foreach(_rel IN LISTS ARGN)
    set(_abs "${CMAKE_SOURCE_DIR}/${_rel}")
    if(NOT EXISTS "${_abs}")
      message(FATAL_ERROR "[export surfaces] ${target}: cmake/export_surfaces.cmake names ${_rel}, which does not exist.")
    endif()
    list(APPEND _headers "${_abs}")
  endforeach()
  if(WIN32)
    set(_format def)
    set(_ext def)
  elseif(APPLE)
    set(_format darwin)
    set(_ext exp)
  else()
    set(_format gnu)
    set(_ext map)
  endif()
  set(_script "${CMAKE_SOURCE_DIR}/scripts/gen_export_list.py")
  set(_out "${CMAKE_BINARY_DIR}/export_lists/${target}.${_ext}")
  add_custom_command(
    OUTPUT "${_out}"
    COMMAND ${Python3_EXECUTABLE} "${_script}" --format ${_format} --output "${_out}" ${_headers}
    DEPENDS ${_headers} "${_script}" "${CMAKE_SOURCE_DIR}/scripts/check_policies.py"
    COMMENT "Generating export list for ${target}"
    VERBATIM)
  if(WIN32)
    target_sources(${target} PRIVATE "${_out}")
  else()
    add_custom_target(${target}_export_list DEPENDS "${_out}")
    add_dependencies(${target} ${target}_export_list)
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${_out}")
    if(APPLE)
      target_link_options(${target} PRIVATE "LINKER:-exported_symbols_list,${_out}")
    else()
      target_link_options(${target} PRIVATE "LINKER:--version-script=${_out}")
    endif()
  endif()
endfunction()

# Library target — static or shared per BUILD_SHARED_LIBS; reuses lumice_obj objects.
add_library(lumice)
# The single-path module and the kernel under it are linked by name, both of them: an OBJECT
# library's objects go only into a target that links it directly, never transitively.
target_link_libraries(lumice PUBLIC lumice_obj PRIVATE lumice_foundation_obj lumice_raypath_obj lumice_analytic_kernel)
if(NOT MSVC)
  target_link_options(lumice PUBLIC
    $<$<CONFIG:MinSizeRel>:$<$<CXX_COMPILER_ID:AppleClang,Clang>:-dead_strip>>
    $<$<CONFIG:MinSizeRel>:$<$<CXX_COMPILER_ID:GNU>:--gc-sections>>)
endif()
if(BUILD_SHARED_LIBS)
  # Exactly the functions its surface headers declare (the lumice_*.h capability headers and the
  # analytic capability, cmake/export_surfaces.cmake), on every platform.
  lumice_apply_export_list(lumice ${LUMICE_ENGINE_SURFACE_HEADERS})
endif()
if(WIN32 AND BUILD_SHARED_LIBS)
  # Consumers that link the engine DLL (the two shells, gui_test) get LUMICE_API's dllimport;
  # lumice_obj itself and the targets linking its objects directly do not (see lumice_base.h's
  # LUMICE_API comment). INTERFACE, so the decision follows the link graph, not a global switch.
  target_compile_definitions(lumice INTERFACE LUMICE_SHARED_DEFINE)
  # The same for the analytic capability the engine DLL also exports (lumice_analytic_core.h's
  # LUMICE_ANALYTIC_API): a shell calling it through this DLL needs the dllimport. The objects
  # that define those functions (lumice_analytic_kernel) link no DLL target and so never see it.
  target_compile_definitions(lumice INTERFACE LUMICE_ANALYTIC_SHARED_DEFINE)
  # On Windows the shared engine is an INTERNAL component of the release, not a library
  # anyone links: the zip carries one shell per entry point (`Lumice.exe`, `LumiceGUI.exe`)
  # and two builds of this DLL, one per ISA tier, and the shell picks one by CPUID at start
  # (src/launcher/win_engine_loader.c). The tier is therefore part of the FILE NAME — the two
  # DLLs sit side by side in one directory and must not collide — and the name says "engine"
  # so a reader of the zip does not take it for a library to program against; there is no
  # bare lumice.dll on Windows any more. The shell's /DELAYLOAD directive in src/CMakeLists.txt
  # and src/gui/CMakeLists.txt reads the name back through $<TARGET_FILE_NAME:lumice> rather
  # than spelling it a second time.
  set_target_properties(lumice PROPERTIES OUTPUT_NAME "lumice-engine.${LUMICE_ISA_LEVEL}")
endif()

# Shared builds: install the dylib next to the CLI (build/cmake_install/shared/lib)
# so the install tree is self-contained. The CLI's INSTALL_RPATH (@loader_path/lib,
# see src/CMakeLists.txt) resolves @rpath/liblumice.dylib there. Static builds
# archive lumice into the CLI, so there is nothing to install here.
# Windows has no rpath: the shell resolves the engine DLL from its OWN directory (absolute
# path, never PATH or the working directory), so the DLL is installed beside the executables
# at the prefix root, not under lib/ — the same place release.yml's merge step copies it from.
# The import library is not product content and stays under lib/.
if(BUILD_SHARED_LIBS)
  if(WIN32)
    install(TARGETS lumice
        RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}"
        ARCHIVE DESTINATION lib)
  else()
    install(TARGETS lumice LIBRARY DESTINATION lib)
  endif()
endif()

# Test-only export surface: the same lumice_obj objects liblumice is made of, plus the handful of
# LUMICE_TEST_* hooks pytest needs and lumice.h must never carry (a test-only entry point in the
# product ABI is the shape the owner rejected). It is a SUPERSET STAND-IN for liblumice, not a
# companion: -fvisibility=hidden leaves nothing for a side library to link against, so the hooks
# must ship with their own copy of the engine, and the ctypes harness loads this one library
# instead of liblumice — two copies in one process would be two engines and two sets of statics.
# Gated on BUILD_SHARED_LIBS alone (NOT BUILD_TEST) so CI's e2e-slow leg, configured with
# -DBUILD_TEST=OFF -DBUILD_GUI=OFF -DBUILD_SHARED_LIBS=ON, still produces it; the static flavor
# has no shared library at all, so the target simply does not exist there. The hooks' sources
# live under test/, and scripts/check_policies.py's no-test-symbol-in-src rule keeps the
# LUMICE_TEST_ prefix out of src/ so the two surfaces cannot grow back together.
if(BUILD_SHARED_LIBS)
  add_library(lumice_testapi SHARED
    "${PROJ_TEST_DIR}/support/lumice_test_api.cpp")
  # PRIVATE: nothing links against this target — it is only ever dlopen'ed by ctypes.
  target_link_libraries(lumice_testapi PRIVATE lumice_obj lumice_foundation_obj lumice_raypath_obj lumice_analytic_kernel)
  if(NOT MSVC)
    # Same compile discipline as lumice_obj: hidden unless marked (LUMICE_TEST_API here).
    target_compile_options(lumice_testapi PRIVATE
      $<$<CONFIG:Release>:-fvisibility=hidden>
      $<$<CONFIG:MinSizeRel>:-fvisibility=hidden>)
  endif()
  # liblumice's surface plus the LUMICE_TEST_* hooks — the superset stand-in's whole surface.
  lumice_apply_export_list(lumice_testapi ${LUMICE_TESTAPI_SURFACE_HEADERS})
  install(TARGETS lumice_testapi LIBRARY DESTINATION lib)
endif()

# The published analytic library (doc/analytic-api.md): the engine's foundation objects and the
# analytic kernel — not lumice_obj, which it no longer links (section 2.1) — its own header
# (src/include/lumice_analytic.h over lumice_analytic_core.h), its own prefix, and an export list
# naming only the LUMICE_ANALYTIC_* functions — so, unlike lumice_testapi, it does NOT re-export
# lumice.h. Its own code lives in src/analytic/, outside lumice_obj. The kernel, with the
# capability's C ABI (analytic_capi.cpp), is also linked into the other two libraries, which
# export that capability too (cmake/export_surfaces.cmake). The library's management TU
# analytic_lib.cpp — GetApiVersion, SetLogCallback, and the load-time object that removes the
# console sink — is a direct source of this target and nothing else: linked into the engine it
# would silence the engine's console, which no export table shows.
# Three rules from doc/analytic-api.md section 2:
#   - only from a build without CUDA (2.3): the published artifact must load on a machine without
#     the CUDA runtime. foundation links no CUDA target, so this is no longer what keeps cudart
#     out; the target still does not exist in a CUDA configure.
#   - dead-code stripping (2.2) is for size only: with lumice_obj gone from the link there is no
#     engine left for it to remove, and the closure is checked by lumice_analytic_link_probe below,
#     which links the same objects without stripping. The objects are compiled with
#     -ffunction-sections -fdata-sections; MinSizeRel matches what `lumice` does there.
#   - the ISA tier is a property of the configure it is produced from (2.4): publish it from
#     -DLUMICE_ISA_LEVEL=baseline, never from the local default `native`. Not enforced here.
# Installed with a CMake package config, so an outside project finds it with
# find_package(LumiceAnalytic <version>) and links Lumice::lumice_analytic (doc/analytic-api.md
# section 8.4). Tests in this tree still load it from the build tree.
if(BUILD_SHARED_LIBS AND NOT LUMICE_CUDA_ENABLED)
  add_library(lumice_analytic SHARED
    "${PROJ_SRC_DIR}/analytic/analytic_lib.cpp")
  target_link_libraries(lumice_analytic PRIVATE lumice_analytic_kernel lumice_foundation_obj)
  if(NOT MSVC)
    target_compile_options(lumice_analytic PRIVATE
      $<$<CONFIG:Release>:-fvisibility=hidden>
      $<$<CONFIG:MinSizeRel>:-fvisibility=hidden>)
    # By linker, not by compiler: Clang on Linux drives GNU ld / lld, which want --gc-sections.
    if(APPLE)
      set(_analytic_strip "-Wl,-dead_strip")
    else()
      set(_analytic_strip "-Wl,--gc-sections")
    endif()
    target_link_options(lumice_analytic PRIVATE
      $<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>:${_analytic_strip}>)
  else()
    # link.exe and lld-link strip unreferenced functions under /OPT:REF — their default unless
    # /DEBUG is passed, which a RelWithDebInfo-style flag change would do; spelled out so the
    # stripping does not depend on the rest of the link line.
    target_link_options(lumice_analytic PRIVATE
      $<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>:/OPT:REF>)
  endif()
  lumice_apply_export_list(lumice_analytic ${LUMICE_ANALYTIC_SURFACE_HEADERS})

  # Link probe: an executable built from exactly liblumice_analytic's objects (foundation, the
  # kernel with the capability's C ABI, and the management TU), with no dead-code stripping. The library itself cannot serve:
  # a GNU/Linux shared library may leave symbols undefined, and its --gc-sections would drop an
  # unreferenced function before its references were ever resolved. An executable has neither
  # escape, so if any of these objects reaches a symbol of a layer above foundation/analytic
  # (cmake/lumice_layers.cmake), this target fails to link — in every build that produces the
  # library, CI's e2e-slow legs included. Not installed; built only to be linked.
  add_executable(lumice_analytic_link_probe
    "${PROJ_TEST_DIR}/support/analytic_link_probe.cpp"
    "${PROJ_SRC_DIR}/analytic/analytic_lib.cpp")
  target_link_libraries(lumice_analytic_link_probe PRIVATE lumice_foundation_obj lumice_analytic_kernel)
  lumice_apply_engine_compile_options(lumice_analytic_link_probe)
  if(WIN32)
    target_compile_definitions(lumice_analytic INTERFACE LUMICE_ANALYTIC_SHARED_DEFINE)
  endif()

  # The target's own include interface. The engine's include directories reach it only through
  # the PRIVATE links to the object libraries, which a consumer never sees; install(EXPORT) needs a path that
  # is relative to the install prefix, not this checkout's absolute source path.
  target_include_directories(lumice_analytic PUBLIC
    $<BUILD_INTERFACE:${PROJ_SRC_DIR}/include>
    $<INSTALL_INTERFACE:include>)

  # The package version IS the header's LUMICE_ANALYTIC_API_VERSION, read from the header rather
  # than written a second time here: one counter, so the version a consumer's find_package
  # accepts cannot drift from the one LUMICE_ANALYTIC_GetApiVersion() reports.
  file(STRINGS "${PROJ_SRC_DIR}/include/lumice_analytic_core.h" _analytic_version_line
    REGEX "^#define LUMICE_ANALYTIC_API_VERSION [0-9]+$")
  string(REGEX MATCH "[0-9]+$" LUMICE_ANALYTIC_API_VERSION "${_analytic_version_line}")
  if(NOT LUMICE_ANALYTIC_API_VERSION)
    message(FATAL_ERROR "Could not parse LUMICE_ANALYTIC_API_VERSION from lumice_analytic_core.h")
  endif()

  # Standard bin/ + lib/ layout — deliberately not `lumice`'s DLL-at-the-prefix-root one, which
  # serves the release shell loading its engine from its own directory. This is a library for
  # other projects' builds (doc/analytic-api.md section 8.7 fixes these relative paths as a
  # contract for non-CMake consumers). Every rule below is in the `analytic` component, so
  # `cmake --install <build> --component analytic` installs exactly this package — from a tree
  # that built only some targets (CI's windows-shared-export), or into a separate artifact.
  install(TARGETS lumice_analytic EXPORT LumiceAnalyticTargets
    RUNTIME DESTINATION bin COMPONENT analytic
    LIBRARY DESTINATION lib COMPONENT analytic
    ARCHIVE DESTINATION lib COMPONENT analytic)
  # These two headers only (lumice_analytic.h includes the core one), never the whole
  # src/include/: lumice.h is not published, and a consumer must not be able to compile both
  # (doc/analytic-api.md section 2.6).
  install(FILES
    "${PROJ_SRC_DIR}/include/lumice_analytic.h"
    "${PROJ_SRC_DIR}/include/lumice_analytic_core.h"
    DESTINATION include COMPONENT analytic)

  # ExactVersion: in 0.x no two interface versions are promised compatible (section 8.3), so a
  # request for any version other than the installed one is refused at configure time. Moving to
  # SameMajorVersion is part of the 1.0 graduation (section 8.6), not before.
  include(CMakePackageConfigHelpers)
  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/LumiceAnalyticConfigVersion.cmake"
    VERSION "${LUMICE_ANALYTIC_API_VERSION}"
    COMPATIBILITY ExactVersion)
  configure_package_config_file(
    "${CMAKE_SOURCE_DIR}/cmake/LumiceAnalyticConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/LumiceAnalyticConfig.cmake"
    INSTALL_DESTINATION lib/cmake/LumiceAnalytic)
  install(EXPORT LumiceAnalyticTargets
    FILE LumiceAnalyticTargets.cmake
    NAMESPACE Lumice::
    DESTINATION lib/cmake/LumiceAnalytic
    COMPONENT analytic)
  install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/LumiceAnalyticConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/LumiceAnalyticConfigVersion.cmake"
    DESTINATION lib/cmake/LumiceAnalytic
    COMPONENT analytic)
endif()


# ==================================================================================================
# Add subdirectory
add_subdirectory(src)

if(BUILD_TEST)
  enable_testing()
  add_subdirectory(test)
endif()

if(BUILD_BENCH)
  add_subdirectory(bench)
endif()

if(BUILD_GUI)
  add_subdirectory(src/gui)
endif()

if(BUILD_GUI AND BUILD_TEST)
  add_subdirectory(test/gui)
endif()
