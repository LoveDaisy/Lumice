[中文版](developer-guide_zh.md)

# Developer Guide

This guide is the practical entry point for contributors. Architecture decisions live in the topic documents; repository-wide engineering rules live in `AGENTS.md`; branch, pull-request, and release policy lives in `CONTRIBUTING.md`.

## 1. Prerequisites

Lumice is a C++17 CMake project built with Ninja. A normal development machine needs:

- Apple Clang 14+, GCC 11+, or MSVC 2022;
- CMake 3.14+;
- Ninja 1.10+;
- Python 3.9+ (used during every build to embed assets and by E2E tooling);
- for GUI builds on Linux, OpenGL, X11, and GTK 3 development packages.

Dependencies are fetched by CPM.cmake and cached per machine under `$HOME/.cache/lumice-cpm` by default. `CPM_SOURCE_CACHE` overrides that location. Build and install trees remain per worktree and per library flavor.

## 2. Build commands and output trees

Use the repository script:

```bash
./scripts/build.sh -j release       # static CLI/library
./scripts/build.sh -gj release      # add the GUI
./scripts/build.sh -tj release      # build tests and run CTest correctness labels
./scripts/build.sh -gtj release     # add gui_test; requires a display
./scripts/build.sh -bj release      # add benchmarks
./scripts/build.sh -sj release      # shared libraries
```

Options compose:

- `-t`, `-g`, and `-b` choose what is built;
- `-s` alone changes the library flavor from static to shared;
- `-k` removes the selected flavor's CMake build/install trees but leaves compiler output and the machine CPM cache;
- `-x` removes all repository-local `build/` output, but still leaves the machine CPM cache.

Paths:

| Purpose | Path |
|---|---|
| CMake build tree | `build/cmake_build/<static|shared>/` |
| Install tree | `build/cmake_install/<static|shared>/` |
| Compiler output | `build/<BUILD_TYPE>/<static|shared>/{bin,lib}/` |
| CLI | `build/cmake_install/static/Lumice` |
| GUI | `build/cmake_install/static/LumiceGUI` after a `-g` build |

For the complete current option text, run `./scripts/build.sh -h` rather than copying flags from this page into another specification.

## 3. Source organization and boundaries

The engine layer order is defined once in `cmake/lumice_layers.cmake`:

```text
foundation → analytic → scene → view → sim → render → engine → raypath → capi
```

Every engine file under `src/` belongs to that manifest. A file may include its own or a lower layer. `scripts/check_policies.py` rejects unowned files, missing manifest entries, and new upward dependencies.

Major directories:

| Directory | Responsibility |
|---|---|
| `src/config/` | JSON parsing and scene/config data |
| `src/core/` | geometry, optics, simulator, buffers, projection, backends |
| `src/server/` | session orchestration, consumers, results, C bridges |
| `src/raypath/` | raypath reports and single-path analysis |
| `src/analytic/` | the published analytic capability |
| `src/include/` | public C capability headers |
| `src/gui/` | interactive application shell |
| `src/util/` | infrastructure and pure shared helpers |
| `test/` | C++ and Python verification layers |

### Public API boundary

There is no umbrella header. Include the capability that declares what you use:

```text
lumice_base.h  lumice_scene.h  lumice_render.h
lumice_editor.h  lumice_engine.h  lumice_raypath.h
```

The GUI may include these engine capability headers, but it may not include `core/`, `config/`, or the analytic-family public headers. A helper shared by GUI and engine belongs in `src/util/` only when it is pure, stateless, and carries no simulation/configuration semantics.

`src/raypath/detail/` is internal to the raypath module and its tests. The engine-facing raypath headers are the documented public module surface.

## 4. Style and diagnostics

Follow `.clang-format` and `.clang-tidy`.

Naming:

- types: `CamelCase`;
- functions: `CamelCase`;
- variables: `lower_case`;
- private members: `lower_case_`;
- constants: `kCamelCase`;
- namespace: `lumice`.

Prefer `const`, `constexpr`, smart pointers, `nullptr`, and `override`. Do not use raw `new`/`delete` or `using namespace`.

Run formatting before committing:

```bash
./scripts/format.sh
./scripts/format.sh --check
```

### Logging

Engine and server diagnostics use `src/util/logger.hpp`:

```cpp
ILOG_INFO(logger_, "Committed epoch {}", epoch);
LOG_WARNING("Configuration uses a deprecated field");
```

GUI diagnostics use `GUI_LOG_*` from `src/gui/gui_logger.hpp`. Do not add `printf`, `fprintf`, `std::cout`, or `std::cerr` under `src/`; raw output bypasses the configured sinks and may disappear in the Windows GUI. The only named exceptions are the CLI product output in `src/main.cpp`, the pre-abort owner in `src/util/fatal.hpp`, and the pre-engine Windows launcher.

CLI product output uses stdout; diagnostic logging uses stderr.

## 5. Adding or changing code

Before editing a subsystem, read the relevant design document from `doc/README.md`. In particular:

- geometry or predicates: `geometry/crystal-geometry-representation.md` and `geometry/numerical-robustness.md`;
- GPU paths: `backends/seam-design.md` plus the backend lifecycle documents;
- exposure/display: `rendering/ev-pipeline-architecture.md`;
- GUI state: `gui/gui-state-governance.md`;
- raypath analysis: `raypath/raypath-analysis.md` and `raypath/raypath-analysis-panel.md`;
- tests: `testing/testing-architecture.md`.

For a new engine source file:

1. choose the lowest layer that owns its semantics;
2. add it to `cmake/lumice_layers.cmake` in the same change;
3. keep includes within the allowed direction;
4. expose functionality through an existing capability, or make an explicit API design decision before adding a new surface;
5. add the smallest test whose oracle matches the behavior.

For a public C API change:

1. place the declaration in the capability header that owns the operation;
2. use that header's export macro;
3. implement it in the matching `src/server/c_api_<capability>.cpp` bridge;
4. update the API version and compatibility record when required;
5. verify the generated export surface and external-consumer contract.

Do not copy one semantic implementation into the GUI, CLI, and engine. Put the authority at the lowest valid shared layer and have each shell consume it.

### Environment variables

Before adding `std::getenv`, apply `env-var-policy.md`. User-facing behavior belongs in CLI/config/API, not in a silent machine-local variable. All runtime `LUMICE_*` reads are centralized in `src/util/env_knobs.cpp` and must be registered and logged.

## 6. Testing

Build tests first, then use the scope that matches the decision:

```bash
LUMICE_SKIP_GUI_TESTS=1 ./scripts/build.sh -gtj release
./scripts/test.sh quick   # verify the change just made
./scripts/test.sh full    # broader local sweep, including real-timing GUI pool
./scripts/test.sh pr      # pre-PR shape, including shared-lib slow E2E
```

Direct Python entry points:

```bash
pytest -v                 # fast subset, matching CI
pytest -v -m ''           # full E2E set
pytest -v -m slow         # slow tests; requires ./scripts/build.sh -sj release
```

The command's exit code is the pass/fail verdict. Do not pipe a test command through `tail` without `pipefail`, and do not infer success from a stale binary after a failed build.

### Where a new test belongs

Choose by verification purpose first:

| Proposition | Location / target |
|---|---|
| one pure unit | `test/unit-correctness/` |
| two or more collaborating units, no live frame/input | `test/composition-correctness/` |
| backend equivalence | `test/parity-cross-backend/` |
| closed-form or analytic oracle | `test/golden-analytic/` |
| CLI/full-stack behavior | `test/e2e-correctness/` |
| throughput gate | `test/performance/` |
| rendered frame or synthesized GUI input | `test/gui/` / `gui_test` |
| a specific escaped defect | `test/regression-sentinel/` |

For `src/gui/` specifically:

- one unit's pure logic: `gui_unit_test` (or `unit_correctness_test` for header-only code);
- collaborating GUI units without a frame: `composition_correctness_test`;
- a rendered frame or `ImGuiTestContext`: `gui_test`.

Do not update visual references by copying one failed output over the expected image. Use the registered regeneration workflow and its full-suite sampling rules from `testing/testing-architecture.md`.

## 7. Policy checks

Run the whole-tree policy check when changing structure or boundaries:

```bash
python3 scripts/check_policies.py
```

Before committing, the hook runs formatting plus the diff-scoped gates. They can also be invoked directly:

```bash
python3 scripts/check_new_refs.py --staged
python3 scripts/check_new_gui_tests.py --staged
python3 scripts/check_loop_fatal_asserts.py --staged
```

Install the repository hook once with `./scripts/install-hooks.sh`. Do not bypass a failing hook; fix the underlying issue.

## 8. Debugging and profiling

Build a debug flavor with `./scripts/build.sh -gj debug`. The executable is installed under `build/cmake_install/static/`; compiler output remains under `build/Debug/static/`.

Useful first steps:

- run the CLI with `-v` or `-d`; logs are on stderr;
- for intermittent failures, map the complete data flow and instrument silent-return paths before changing behavior;
- for parity failures, compare both routes at successive internal stages and locate the first divergence;
- for GPU changes, use the documented reference-machine protocol rather than extrapolating from a host without that backend;
- use ASan/UBSan, LLDB/GDB, Instruments, or `perf` according to platform and failure shape.

Performance claims require a rebuilt binary, a correctness sanity gate, an idle reference machine, and the baseline specified by `testing/performance-testing.md`.

## 9. Documentation and contribution

Generate Doxygen locally with:

```bash
doxygen .doxygen-config
```

Output lands under `doc/api/html/` and is not a substitute for the stable capability documentation.

Before opening a pull request:

1. run the relevant focused checks and `./scripts/test.sh pr` when the change reaches the simulator, C API, or shared-library path;
2. inspect the complete diff and staged set;
3. keep permanent prose free of ignored working-note references;
4. follow `CONTRIBUTING.md` for branch, PR, and release procedure.

## Related documentation

- [System architecture](architecture.md)
- [Configuration reference](configuration.md)
- [Testing architecture](testing/testing-architecture.md)
- [C API reference](interfaces/c_api.md)
- [Environment-variable policy](env-var-policy.md)
- [Contributing guide](../CONTRIBUTING.md)
