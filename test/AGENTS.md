# AGENTS.md — test/

Operating rules for the test tree. Repo-wide rules live in the root `AGENTS.md`;
design reasoning, measurements and the test-organization spec live in
`doc/testing/testing-architecture.md`. This file holds what you need while
*working in* `test/`: placement gates, isolation discipline, shared-library
plumbing, and the reference-regeneration workflow.

## Where a new test goes

Chain length (how many `src/gui/` units a proposition needs) and mechanical need
(rendered frame / synthesized input) are independent axes — the full argument is
`doc/testing/testing-architecture.md` §1/§2/§5. Rule of thumb for a new
`src/gui/` test:

- needs a rendered frame or an `ImGuiTestContext` → `gui_test` (`test/gui/`)
- needs ≥2 collaborating units but no mechanics →
  `composition_correctness_test` (`test/composition-correctness/gui/`, CTest
  LABEL `composition-correctness`)
- a single unit's pure logic → `gui_unit_test` (or `unit_correctness_test` if
  header-only) — both under `test/unit-correctness/gui/`, shared CTest LABEL
  `unit-correctness`

Link boundary, not a layer boundary: the `gui` half of unit-correctness is
split on "does the case call into `file_io.cpp` / `user_defaults.cpp`?" —
`gui_unit_test` links `lumice_gui_obj` with no window, no GL context and no
ImGui test engine, so its cases run in CI on every platform that builds the
GUI. `composition_correctness_test` links `lumice_gui_obj` + `lumice_obj` (the
CMake-commented cross-layer oracle exemption) and is not split further — every
case there already needs `lumice_gui_obj`.

Gate, not a rule of thumb, for cases you add: `scripts/check_new_gui_tests.py`
(diff-scoped; CI `new-refs` job vs merge-base, pre-commit on the staged diff).
It rejects a newly registered `TestFunc = [](ImGuiTestContext* …)` when the
parameter is anonymous (including the commented-out-name form) or named but
only mentioned through `IM_UNUSED(<name>)` — both are an explicit statement, by
the compiler or by the author's hand, that the case does not drive the GUI.
There is no inline exemption: if a new case genuinely needs a frame but happens
not to touch `ctx`, make it drive `ctx` (`ctx->Yield()` is the frame pump).
What it deliberately does not reject (`TestFunc = SomeNamedFunction`, named
parameters never referenced and never `IM_UNUSED`-marked, pre-existing cases,
pure renames) and the full rule statement live in the script's docstring.

Fourth diff-scoped checker, same "the checker is the rule" discipline:
`scripts/check_loop_fatal_asserts.py`. Scope is `test/` broadly. In a scope
that executes repeatedly, a non-terminating error report (gtest `ASSERT_*`,
fatal `IM_CHECK*`, non-fatal `IM_ERRORF`) must not be followed by code that
keeps driving the same test context. Full rule statement and the four
defect-shape generalizations: the script's docstring and
testing-architecture.md §4.9.

## Platform matrix and the llvmpipe leg

- CLI, core, and unit-test flows stay cross-platform.
- `gui_test` is **built on three platforms and run on one**: the
  `Ubuntu x86_64` CI leg supplies a display with `xvfb-run` + Mesa's llvmpipe
  and runs only the `modal_layout` and `defaults_panel_layout` reference
  groups there (a **positive** filter — the binary's other ~36 categories,
  every `functional/` case and the entire `parity/` tag among them, are
  evaluated on a developer machine and nowhere else).
- Which groups may run under a software rasterizer is a measured fact, not a
  preference. Before widening the CI filter, and before dispositioning a
  visual-regression failure, follow the checklist in
  testing-architecture.md §4.6 — and read its machine caveat (the numbers were
  measured on an arm64 proxy for the amd64 runner), not just the table.
- `test/gui/parity/` has **no CI job today**: the llvmpipe leg's positive
  filter does not include it and the `E2E Slow` legs build
  `-DBUILD_GUI=OFF`, so `./scripts/test.sh {quick,full,pr}` on a machine with
  a GL context is the only place either parity fixture is ever evaluated
  (mechanism and measured cost of changing that: testing-architecture.md §7.5).
- Windows physical-desktop validation uses `scripts/win_remote_test.sh` with
  `scripts/win_test_watcher.ps1` (doc/testing/windows-remote-testing.md).
- GUI screenshot references live under `test/gui/references/`.

## Test-duration registry (runtime gate)

`test/duration_registry.json` v3 gates two kinds of quantity. `entries` keeps
the per-test rule: over 60 s in a wired job ⇒ registered `id` / `job` /
`ci_seconds` + a real reason; over 2× the registered time, a stale id, malformed
data or a `TODO` is red. `phase_budgets` limits measured wall-clock phases and
serial aggregates: missing/unknown/duplicate phase reports and over-limit sums
are red. A single-phase budget may carry a `per_miss` block — the build phase
judges `seconds ≤ overhead_seconds + max_seconds_per_miss × misses` as the
authoritative ruler (the miss count is the one factor the change controls),
with the absolute `max_seconds` kept as the disaster backstop; aggregates never
carry `per_miss`. The input does not exist at pre-commit time, so this gate is
not part of `check_policies.py`, and there is no per-test/env/inline exemption.
A new test job is not covered until it wires reports, checker and artifacts —
nothing audits that automatically. Registry rules, the 60 s / 2× calibration
and the per-miss constants: testing-architecture.md §7.7. The sole exception —
the maintainer-authorized, one-run cold-build protocol for phase budgets
containing `build` — never excuses test runtime or correctness, and a PR
whole-run/job rerun cannot consume it again; its single authority is
testing-architecture.md §7.8.

## Shared libraries the pytest layers load

The shared-lib build (`./scripts/build.sh -sj release`; `-g`/`-t` alone never
produce it) produces **three** libraries, and pytest loads the second:

- `liblumice` — the product export surface: the capability headers
  `src/include/lumice_*.h`, `LUMICE_*` only, nothing that exists for a test's
  sake.
- `liblumice_testapi` (target `lumice_testapi`, header
  `test/support/lumice_test_api.h`) — built from the **same `lumice_obj`
  objects**, so every product call behaves identically in it, plus the
  `LUMICE_TEST_*` hooks a pytest fixture needs and product headers must never
  carry. It is a superset stand-in, not a companion library:
  `-fvisibility=hidden` leaves a side library nothing to link against, so the
  hooks ship with their own copy of the engine, and a test process loads
  exactly one of the two. `test/e2e/capi_runner.py::lib_candidates` is the
  single authority on which file that is; `scripts/check_policies.py`'s
  `no-test-symbol-in-src` rule keeps the `LUMICE_TEST_` prefix out of `src/`
  so the two surfaces cannot grow back together.
- `liblumice_analytic` — the published analytic library
  (`doc/interfaces/analytic-api.md`): foundation + analytic kernel only, not
  `lumice_obj`; exports only `LUMICE_ANALYTIC_*`; never produced by a CUDA
  configure.

Each library's export list is generated from its headers by
`scripts/gen_export_list.py` (declarations carry `LUMICE_API` /
`LUMICE_TEST_API` / `LUMICE_ANALYTIC_API`; the generator refuses one without)
and audited against the built binaries by `scripts/check_export_surface.py` —
run locally by `test/e2e-correctness/test_export_symbol_scope.py` in
`e2e-slow` / `windows-shared-export`, and by `release.yml` on every packaged
engine file, both ISA builds included. The prefix boundary itself is
`check_policies.py`'s `analytic-symbol-scope` rule. Existing pytest calls to
product symbols are deliberately **not** migrated to the test header: their
subject is the product C API contract itself, and wrapping them would test the
wrapper.

`@pytest.mark.slow` means "needs this shared-lib build", not "takes long".
Bare `pytest -v` runs the fast subset — pinned by `pyproject.toml` `addopts`
(`-m "not slow"`), which matches CI's fast leg; `-m ''` on the command line
overrides back to the full set. Rationale and the incident that fixed the pin:
testing-architecture.md §5.

## E2E layout (purpose-primary)

Per `doc/testing/testing-architecture.md` §6:

- `test/e2e-correctness/` — full-stack correctness via CLI/PSNR (smoke, CLI
  behavior, raypath equivalence) + `references/*.jpg`
- `test/parity-cross-backend/backend/` — backend-equivalence oracles (Metal
  exit-seam parity, device-gen default path, cpu_backend route, Metal batch
  invariance) + C++ siblings
- `test/performance/` — throughput gates (Metal throughput)
- `test/gui/` — GUI acceptance alongside the C++ `gui_test` suite
  (`functional/`, `visual/`, `responsiveness/`, `parity/`)
- `test/regression-sentinel/` — bug-resurfacing guards, including
  `test_capi_sentinel_overflow.py` (3 configs × 12 rounds = 36 server
  lifecycles via `LUMICE_AcquireResultFrame` +
  `LUMICE_FrameGetRawXyz(max_count=1)`; guards the C API's off-by-one
  sentinel write, fix 5287efe) and `test_ms_filter_leak.py` (filter-fail rays
  do not propagate across MS layers).

Shared fixtures stay under `test/e2e/` (`base.py`, `runner.py`,
`capi_runner.py`, `image_utils.py`, `_parity_metrics.py`,
`_projection_battery.py`, `configs/`).

## gui_test isolation discipline

- **Personal defaults.** `gui_test`'s own `--user-config`/`--no-user-config`
  no-flag default is `kDisabled` (the interactive app auto-detects instead),
  so a visual-regression reference never depends on whichever
  `user_defaults.json` happens to exist on the capturing machine. Before
  adding a new reference image, confirm the isolation is actually in effect
  for the scene under test: an explicit, freshly emptied `--user-config`
  directory installed **before** `ResetTestState()`, not after — installed
  after, the capture is built from whatever personal defaults the running
  machine has saved. `gui_unit_test` gets the same baseline from a different
  place: it shares `test_main.cpp` with the non-GUI targets, which cannot
  call into `lumice_gui_obj`, so the install lives in a gtest global
  environment only that target compiles
  (`test/unit-correctness/gui/gui_unit_test_env.cpp`). Without it the
  process-wide source stays at `kAutoDetect` — the unset value,
  `src/gui/user_defaults.cpp` — which resolves to the developer's real OS
  config directory. A green run is not evidence the isolation holds: point
  `HOME` at a directory containing a `user_defaults.json` and re-run to check.
- **Window geometry.** `ResetTestState()` restores the shared GLFW window to
  the 1600x980 `kInit` basis at every case entry. A test that moves the
  window declares a `ScopedWindowBasis` guard rather than hand-writing a tail
  restore; a case entry that finds the window elsewhere prints a
  `[window-basis-leak]` line naming the last geometry mutator and fails the
  whole run's exit code (a leak is a run-level red, and it also stops a regen
  driver's full-suite run on purpose — fix the leak, then reshoot).
- **ImGui per-window state.** Every window's `ImGuiStorage` (TreeNode fold
  flags, and what production code parks there through `GetStateStorage()`,
  such as the triple typed into the wedge add row) and its scroll offset
  outlives the window being submitted, so a sibling case that unfolds a node,
  types into a storage-backed box or scrolls a list hands that state to the
  next case drawing the same window. `ResetTestState()`
  (`test/gui/test_gui_main.cpp`) clears both for **every** window, not an
  enumerated few: the writes happen inside production code a case only
  reaches by typing, so the harness cannot know which windows a case touched.
  The diagnostic that survives: a reference that only agrees with itself
  under one test order is the symptom to look for, and for every
  deterministic group `--filter <group>` in isolation must read `PSNR=inf`
  exactly as the full pool does.
- **Scratch paths.** Without `--export-dir`, `gui_test` writes to a
  **per-process** subdirectory of the platform temp directory
  (`GuiTestTempPath`, `test/gui/test_gui_shared.hpp`): the suite has no
  hardcoded `/tmp` (paths are valid on Windows), and two `gui_test` processes
  cannot collide on a fixed filename — which is what a sharded correctness
  pool would otherwise do to itself. A group's `tmp_prefix` is a **filename**
  prefix only; the directory is not part of that contract.

## GUI Test Reference Regeneration (reference groups)

The driver is `scripts/regen_gui_test_refs.py`; the group registry is its
top-level `GROUPS`, currently `capture_harness`, `lens_proj`, `modal_layout`,
`defaults_panel_layout`, `config_summary_layout`. A group names the `gui_test`
category it tags output with (also the `[<tag>]` its comparisons print and its
key in `_thresholds.json`), its scenes/modes, and the export/reference filename
prefixes. Phase A/B themselves are group-agnostic — adding a
visual-regression suite means adding a `GROUPS` entry.

Thresholds and rulers (the `mean − max(4σ, 1.0 dB)` calibration, the pixel
ruler's τ/K values per group, and why deterministic groups left PSNR for the
pixel ruler) are owned by `doc/testing/testing-architecture.md` §4.6/§4.11 —
read those before touching thresholds. What lives here is the operational
flow:

- Stochastic groups (`lens_proj` today) run the full suite `STOCHASTIC_RUNS`
  (10) times per phase; deterministic groups run it **once** (Phase A takes
  that single capture as the reference; Phase B still writes `identical_runs`
  / `maxcc_tau` / `maxcc_local_max` and records `n_calib_runs: 1`). The hard
  rule behind the default: a reshoot of a deterministic group triggered by a
  UI change is never repeated — the second run of a frame with no simulation
  and no RNG is the same bytes again, and at ~88 s per full-suite run the old
  10/10 default turned one two-group reshoot into 64 minutes of zero
  information.
- Both phases sample **full-suite** runs under `scripts/build.sh`'s
  correctness-pool invocation, never a single group in isolation — isolated
  runs measured 0.34–0.62 dB optimistic, which is how the since-retired
  `auto_ev` references once ended up flaking. The driver has no switch for
  this. Related gate: `check_policies.py`'s `gui-test-suite-args-sync` rule
  keeps the driver's `SUITE_FILTER_EXPR` equal to `scripts/build.sh`'s
  `--filter` invocation.
- Several groups in one invocation share the same runs — the cost is
  `max(n)` over the selected groups, not `sum(n)`.
- Phase A refuses to shoot a reference `origin/main` has already reshot since
  the merge base (that image would be discarded at rebase time in favour of
  main's copy): rebase first, then shoot. `--allow-stale-base` overrides; no
  `origin/main` at all skips the check with a note.
- Phase B merges per group and per scene (groups/scenes not covered keep
  their entries and `generated_at`); thresholds land under
  `groups.<key>.scenes` in `test/gui/references/_thresholds.json`. After
  Phase B, copy the `threshold` values into the group's test source — for
  `lens_proj` the `psnr_threshold` field of each `kScenes[]` row in
  `test/gui/visual/test_gui_lens_projection.cpp` (the script prints the path
  for whichever group it ran). Deterministic groups have no threshold to copy
  back: K is a cross-machine number Phase B cannot measure, so `kRuler` lives
  in the test source, and Phase B's `maxcc_tau` / `maxcc_local_max` are only
  the audit that the reference machine still reads 0.

```bash
# Full regen of EVERY registered group (Phase A + Phase B). Cost is set by the
# stochastic group: 10 + 10 full-suite runs ≈ 30 min; deterministic groups ride along.
python scripts/regen_gui_test_refs.py

# One deterministic group: 1 + 1 full-suite runs ≈ 3 min. Do NOT add --n / --n-calib here.
python scripts/regen_gui_test_refs.py --group modal_layout

# One group only (recommended — regen what you changed, leave other groups' refs alone):
python scripts/regen_gui_test_refs.py --group lens_proj

# Phase A only / Phase B only:
python scripts/regen_gui_test_refs.py --group lens_proj --phase-a-only
python scripts/regen_gui_test_refs.py --group lens_proj --phase-b-only

# Single scene (--scene requires --group; scene names are only unique within a group):
python scripts/regen_gui_test_refs.py --group lens_proj --scene overlay_ea

# Quick smoke test of the stochastic path (2 runs each phase; flags apply to every selected group):
python scripts/regen_gui_test_refs.py --group lens_proj --n 2 --n-calib 2

# Shoot even though origin/main has already reshot one of these references (default: refuse):
python scripts/regen_gui_test_refs.py --group modal_layout --allow-stale-base
```

Flags worth knowing: `--keep-export-png` makes `CheckAgainstReference` skip
its `std::remove` so the per-run export PNGs survive for the driver to
collect; `--export-dir <path>` names where every scratch file this suite
writes goes (capture PNGs, round-tripped `.lmc`, exported JSON) — the driver
passes both and collects `<group tmp_prefix><scene>.png` from the directory
it named.

Registering a new group — three constraints: the key must be unique across
groups (PSNR samples are attributed by an exact match on the `[<tag>]` prefix
in a shared full-suite stderr); the test must compare via
`lumice::test::CheckAgainstReference` so Phase B can parse its PSNR line; and
the entry must declare `deterministic` (no default on purpose — it sets the
run-count default, and the alternative, inferring it from `_thresholds.json`'s
`identical_runs`, has nothing to read for a group with no entry yet).

Not every `CheckAgainstReference` caller is in `GROUPS`. The `visual` category
(`test/gui/visual/test_preview_pixels.cpp`: `crystal_preview_prism/pyramid/
wireframe/shaded`, `left_panel`) compares at τ=0, K=0 — byte-identity, which is
why its five references are PNG and must stay PNG — and its reference
filenames do not follow the `<ref_prefix><scene>` convention every
`ReferenceGroup` assumes (see the comment above `STAGING_DIR` in the driver
for why registering it buys nothing and how to reshoot it by hand). A
theme/layout change that reaches the left panel or the crystal-preview FBO
needs that manual step in addition to the `--group` commands above; running
only the registered groups leaves `visual`'s references stale with no
automated signal pointing at it.

Per-group regen triggers and scene inventories:

- `lens_proj` covers the preview fragment shader's projection math
  (`src/gui/preview_renderer.cpp`: `linearInverse` / `fisheyeInverse` /
  `dualFisheyeInverse` / `rectangularInverse`) via an off-screen FBO export,
  independent of window size or layout. Nine scenes: five are one per
  projection branch; the other four each reuse a covered branch to cover a
  different shader stage — `overlay_ea` (the only committed pixel coverage of
  `overlayAuxLines()`), `fisheye_equal_area_120_border` /
  `dual_fisheye_equal_area_full_border` (`overlayLensBorder()`, one per
  shader branch that can draw a border), and `sky_colour_ea_180` (background
  fill and its gate; the only scene whose background is not black, so
  "painted where it should not have" and "left black" are different pixels).
  Regen trigger: any change to that projection math, to the overlay drawing,
  to the background fill or its gate, or to `export_fbo_renderer.cpp`'s
  render path.
- `modal_layout` covers the edit modal's internal control layout
  (`src/gui/edit_modals.cpp`) via an on-screen capture of the live "Edit
  Entry" window rectangle. Regen trigger: any layout change to the edit modal
  (slider/input widths, property-table columns, control ordering,
  auto-resize behavior), or a harness window size / font atlas / ImGui style
  change.
- `defaults_panel_layout` covers the Settings modal (`src/gui/defaults_panel.cpp`)
  through the same on-screen sub-region capture. Regen trigger: any layout
  change to the panel's section headers, the settings table's columns, the
  preset table's columns, or the pinned action row. Every scene here must
  install its user-config directory before `ResetTestState()` (see isolation
  above — the personal-defaults leak is worth 20.7 dB on a scene that looks
  isolated).
- `config_summary_layout` covers the read-only Summary window
  (`src/gui/config_summary_window.cpp`, content from `config_summary.cpp`).
  What the page *prints* is asserted without a frame in
  `test/unit-correctness/gui/test_config_summary_rows.cpp` and
  `test/composition-correctness/gui/test_config_summary_export_parity_chain.cpp`;
  this group only owns the layout pixels. Regen trigger: any change to the
  window's tables, indent, heading style or fixed width, or to the page's
  labels or value spelling. This group is not in the CI filter today —
  measure K (testing-architecture.md §4.6) before adding it.

**Docking coupling boundary** — relevant to any future migration of the GUI's
fixed-layout panels (and the edit modal) onto ImGui's docking branch: the
regen harness itself (readback, comparison, threshold, trigger mode) is
layout-agnostic. `lens_proj` renders through its own off-screen FBO
(`RenderExportToRgba`, caller-supplied size) and is fully docking-decoupled.
`modal_layout` reads the DEFAULT framebuffer through `g_fullframe_capture`'s
sub-region protocol using the live on-screen rectangle of the "Edit Entry"
window — once the modal becomes dockable, re-run
`python scripts/regen_gui_test_refs.py --group modal_layout` (Phase A + B); no
harness change is required.
