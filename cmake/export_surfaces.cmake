# Export surfaces: which headers each shared library exports, declared once. A library exports
# exactly the functions these headers declare — the root CMakeLists.txt include()s this file and
# hands each list to lumice_apply_export_list(), which generates the library's export list from
# them (scripts/gen_export_list.py). test/e2e-correctness/test_export_symbol_scope.py reads the
# same lists to know what each built binary must export, and scripts/check_policies.py's
# analytic-symbol-scope rule reads the analytic list as the analytic header family. Paths are
# relative to the repository root.
#
# The engine libraries host the analytic capability (lumice_analytic_core.h) next to lumice.h;
# lumice_analytic.h's two library-management functions (its version and log callback) stay with
# liblumice_analytic alone, because in the engine the logging and the version are lumice.h's.
#
# Restricted syntax, so that CMake and the Python parser (check_policies.parse_export_surfaces)
# cannot read it differently: `#` comments, blank lines, `set(LUMICE_<LIBRARY>_SURFACE_HEADERS` on
# its own line, one bare path per line, and `)` on its own line. The one addition to the layer
# manifest's syntax: a line that is exactly `${LUMICE_<LIBRARY>_SURFACE_HEADERS}` names a list
# declared above it and stands for its elements, as it does in CMake — so a superset is written as
# one, not as a second copy that could drift. The parser rejects anything else rather than
# skipping it.

# liblumice (on Windows lumice-engine.<tier>.dll, one per ISA tier).
set(LUMICE_ENGINE_SURFACE_HEADERS
  src/include/lumice.h
  src/include/lumice_analytic_core.h
)

# liblumice_testapi: the engine's whole surface plus the LUMICE_TEST_* hooks (a superset stand-in
# for liblumice, root CMakeLists.txt).
set(LUMICE_TESTAPI_SURFACE_HEADERS
  ${LUMICE_ENGINE_SURFACE_HEADERS}
  test/support/lumice_test_api.h
)

# liblumice_analytic: the published interface, lumice_analytic.h over lumice_analytic_core.h.
set(LUMICE_ANALYTIC_SURFACE_HEADERS
  src/include/lumice_analytic_core.h
  src/include/lumice_analytic.h
)
