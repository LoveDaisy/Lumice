# ==================================================================================================
message(STATUS "[src] CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

# ==================================================================================================
# Add targets

# cpu_info.cpp is compiled directly into the executable: main.cpp calls the
# internal lumice::PhysicalCoreCount(), which carries hidden visibility and is
# therefore not exported from the shared `lumice` library (BUILD_SHARED_LIBS).
add_executable(Lumice
    "${PROJ_SRC_DIR}/main.cpp"
    "${PROJ_SRC_DIR}/util/cpu_info.cpp")
target_include_directories(Lumice PRIVATE
    ${stb_SOURCE_DIR})
target_link_libraries(Lumice PRIVATE
    lumice)
# Shared builds (Linux/macOS) link a shell against the engine library by its SONAME
# (liblumice.so / @rpath/liblumice.dylib) and resolve it at start-up from the sibling lib/ dir
# the root CMakeLists installs it to, so build/cmake_install/shared/ is self-contained and
# runnable without rebuilding static — fixes "dyld: no LC_RPATH's found" when switching from a
# static to a shared build. Static builds archive lumice into the shell, so there is no rpath
# dependency and none of this is applied. Both shells (this one and LumiceGUI in
# gui/CMakeLists.txt) need the same wiring, hence a function, on the model of
# lumice_wire_engine_delay_load below; Windows has no rpath and is the other function's job.
#
# On Linux x86_64 this rpath is also the whole of the release's ISA dispatch. The tarball
# carries the engine twice under ONE file name — lib/liblumice.so (baseline) and
# lib/glibc-hwcaps/x86-64-v4/liblumice.so (AVX-512) — and glibc >= 2.33 probes the
# glibc-hwcaps/<level>/ subdirectory of every rpath entry before the entry itself, picking the
# v4 copy on a CPU that has it and the baseline copy elsewhere (older glibc never looks in the
# subdirectory and lands on baseline: correct, slower). No code of ours takes part: the shell
# is compiled once at baseline, release.yml's merge step lays the two engine builds out in
# those two directories, and the dynamic loader does the choosing. The two copies must keep
# the same file name for that lookup to work, which is why the Linux engine is NOT renamed
# per tier the way the Windows DLLs are (those sit side by side in one directory and must
# not collide). The override / negative control is glibc's own
# GLIBC_TUNABLES=glibc.cpu.hwcaps=-AVX512F (it masks feature bits, from which the level is
# derived; the level name is not a token it accepts), not a knob of this repository.
function(lumice_wire_engine_rpath target)
  if(APPLE)
    # A bundle's executable lives at <prefix>/<name>.app/Contents/MacOS/<name>; the CLI at
    # <prefix>/<name>. lib/ is beside the prefix root in both cases.
    get_target_property(_is_bundle ${target} MACOSX_BUNDLE)
    if(_is_bundle)
      set_target_properties(${target} PROPERTIES INSTALL_RPATH "@loader_path/../../../lib")
    else()
      set_target_properties(${target} PROPERTIES INSTALL_RPATH "@loader_path/lib")
    endif()
  else()
    set_target_properties(${target} PROPERTIES INSTALL_RPATH "$ORIGIN/lib")
    # Load-path safety, separate from the hwcaps layout above: the linker's default is
    # DT_RUNPATH, which the loader consults AFTER LD_LIBRARY_PATH, so a same-named
    # liblumice.so on LD_LIBRARY_PATH would shadow the packaged engine. --disable-new-dtags
    # emits DT_RPATH instead, which is consulted BEFORE LD_LIBRARY_PATH. The hwcaps
    # subdirectory probe applies to RPATH entries exactly as to RUNPATH ones, so the dispatch
    # does not depend on this line; it can be removed on its own if the safety trade is ever
    # reversed.
    target_link_options(${target} PRIVATE "-Wl,--disable-new-dtags")
  endif()
endfunction()
if(BUILD_SHARED_LIBS AND NOT WIN32)
  lumice_wire_engine_rpath(Lumice)
endif()
# Windows shared builds delay-load the engine DLL and choose WHICH one at start-up
# (src/launcher/win_engine_loader.c has the full story). Three things wire that in, and both
# shells (this one and LumiceGUI in gui/CMakeLists.txt) need the same three: the loader TU, the
# /DELAYLOAD directive naming the DLL the executable is linked against (read back from the
# `lumice` target so the name is spelled in one place, the OUTPUT_NAME in the root
# CMakeLists.txt), and delayimp.lib, the delay-load helper the directive relies on. The loader
# TU is told the same name (LUMICE_ENGINE_LINKED_DLL) because its hook only substitutes for
# that DLL and no other; LUMICE_ENGINE_DELAY_LOADED is what main.cpp gates its call on.
# The `.c` suffix is deliberate — see the file header.
function(lumice_wire_engine_delay_load target)
  target_sources(${target} PRIVATE "${PROJ_SRC_DIR}/launcher/win_engine_loader.c")
  target_compile_definitions(${target} PRIVATE
    LUMICE_ENGINE_DELAY_LOADED=1
    "LUMICE_ENGINE_LINKED_DLL=\"$<TARGET_FILE_NAME:lumice>\"")
  target_link_options(${target} PRIVATE "/DELAYLOAD:$<TARGET_FILE_NAME:lumice>")
  target_link_libraries(${target} PRIVATE delayimp)
endfunction()
if(WIN32 AND BUILD_SHARED_LIBS)
  lumice_wire_engine_delay_load(Lumice)
endif()
install(TARGETS Lumice
    DESTINATION "${CMAKE_INSTALL_PREFIX}")
