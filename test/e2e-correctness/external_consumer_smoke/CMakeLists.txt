# A consumer of liblumice_analytic that sees only an install tree: configured on its own, never
# add_subdirectory'd into this repository's build, with the install prefix on CMAKE_PREFIX_PATH.
# Driven by test/e2e-correctness/test_external_consumer_smoke.py, which also asserts that no
# include path of this project resolves into the source tree.
cmake_minimum_required(VERSION 3.14)
project(LumiceAnalyticExternalConsumerSmoke C)

# ExactVersion while at 0.x: this must equal the installed LUMICE_ANALYTIC_API_VERSION
# (doc/analytic-api.md sections 8.3 / 8.4).
find_package(LumiceAnalytic 11 REQUIRED)

add_executable(external_consumer_smoke main.c)
target_link_libraries(external_consumer_smoke PRIVATE Lumice::lumice_analytic)

# Windows has no rpath: the DLL must sit next to the executable to be found at run time
# (doc/analytic-api.md section 8.4).
if(WIN32)
  add_custom_command(TARGET external_consumer_smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:Lumice::lumice_analytic>" "$<TARGET_FILE_DIR:external_consumer_smoke>")
endif()
