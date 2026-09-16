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
# Shared builds link the CLI against @rpath/liblumice.dylib. Give the installed
# CLI an rpath into the sibling lib/ dir (where the dylib is installed, see the
# root CMakeLists) so build/cmake_install/shared/Lumice is self-contained and
# runnable without rebuilding static — fixes "dyld: no LC_RPATH's found" when
# switching from a static to a shared build. Static builds archive lumice into the CLI, so
# there is no @rpath dependency and this rpath is simply not applied.
if(BUILD_SHARED_LIBS)
  if(APPLE)
    set_target_properties(Lumice PROPERTIES INSTALL_RPATH "@loader_path/lib")
  else()
    set_target_properties(Lumice PROPERTIES INSTALL_RPATH "$ORIGIN/lib")
  endif()
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

# linux-x64 release: the package carries two builds of every entry point (`<name>.baseline` +
# `<name>.x86-64-v4`, produced by two configures at different LUMICE_ISA_LEVEL) and this
# launcher installed under the user-facing name picks one by CPUID at run time
# (src/launcher/isa_launcher.c has the full story). It deliberately gets none of the engine:
# no lumice_obj, no -march flag — it has to run on exactly the CPUs the faster sidecar cannot.
# Built on every x86_64 Linux configure so a plain CI build compiles it, but only
# release.yml's merge step installs it under the entry-point names; a local
# build/cmake_install/<flavor>/Lumice is still the real binary.
# Compiler floor: `__builtin_cpu_supports("x86-64-v4")` needs GCC 12+ or Clang 18+ (it is a
# compile error below that, not a silent misdetection).
# Windows has no launcher any more: its release ships one shell per entry point and two
# engine DLLs, and the shell loads one in-process (lumice_wire_engine_delay_load above).
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
  add_executable(LumiceIsaLauncher "${PROJ_SRC_DIR}/launcher/isa_launcher.c")
  target_compile_options(LumiceIsaLauncher PRIVATE -Wall -Wextra)
endif()
if(TARGET LumiceIsaLauncher)
  install(TARGETS LumiceIsaLauncher
      DESTINATION "${CMAKE_INSTALL_PREFIX}")
endif()
