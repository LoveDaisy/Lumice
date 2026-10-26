# AGENTS.md

## Project Overview

Lumice is a C++17 ice halo ray-tracing simulator. It reproduces halo patterns by tracing light through ice crystals and supports CLI, GUI, unit tests, E2E tests, and performance-oriented workflows.

Core conventions: namespace `lumice`; source layout `.hpp`/`.cpp`; build CMake + Ninja; dependencies CPM.cmake; public API boundary = the engine's capability headers `src/include/lumice_{base,scene,render,editor,engine,raypath}.h` (one per capability, no umbrella header).

## Common Commands

```bash
# Build
./scripts/build.sh -j release
./scripts/build.sh -tj release
./scripts/build.sh -gtj release
./scripts/build.sh -k release

# Run
./build/cmake_install/static/Lumice -f examples/config_example.json
./build/cmake_install/static/Lumice -f config.json -v

# Tests — scripts/test.sh is the recommended entry; see the test-scope table under "Testing and Platform Notes".
LUMICE_SKIP_GUI_TESTS=1 ./scripts/build.sh -gtj release   # headless: build gui_test, skip running it
./scripts/test.sh quick                   # after -gtj above: ctest + fast e2e + gui_test correctness pool
./scripts/test.sh full                    # quick + gui_test real-timing pool
./scripts/test.sh pr                      # full + shared-lib freshness + slow e2e (before opening a PR)
pytest -v                                 # fast e2e only, direct — pinned by pyproject.toml addopts (matches CI)
pytest -v -m ''                           # full e2e set — the addopts pin's escape hatch
pytest -v -m slow                         # slow e2e only (needs shared lib: ./scripts/build.sh -sj release)

# Format
./scripts/format.sh
./scripts/format.sh --check          # read-only: exit 1 if anything needs formatting (used by the pre-commit hook; CI runs the format-check job)

# Engineering-policy gate (env knobs, GUI API boundary, using-namespace, layer inversion)
python3 scripts/check_policies.py    # whole-tree: what state is the repo in?
python3 scripts/check_new_refs.py --staged        # diff-scoped: what did you just write?
python3 scripts/check_new_refs.py --range A..B    # same, over a commit range (what CI runs)
./scripts/install-hooks.sh           # install the non-interactive pre-commit hook (one-time)
```

Build trees and install trees are per-flavor: `-s` (shared) and no `-s` (static) never share a
directory, so both stay warm and switching between them does not force a rebuild. `-t`/`-g`/`-b`
choose *what* to build (tests / GUI app / benchmarks) and compose with either flavor — only `-s`
switches the flavor itself, and it defaults to static when omitted, so `-t`/`-g` alone never produce
a shared library. Release artifacts install to `build/cmake_install/<flavor>/`; the CMake build tree
is `build/cmake_build/<flavor>/` and compiler output lands in `build/<BUILD_TYPE>/<flavor>/{bin,lib}/`.

Dependency **sources**, by contrast, are cached per-*machine*, not per-source-tree: CPM downloads
land in `$HOME/.cache/lumice-cpm` by default (`$USERPROFILE/.cache/lumice-cpm` on a Windows shell
that sets no `HOME`; and if neither variable exists, the old repo-local `build/cpm_cache`).
That is what makes a fresh clone or a new `git worktree` cheap — it reuses whatever this machine
has already downloaded instead of re-fetching ~300MB of the same 13 packages. `CPM_SOURCE_CACHE`
still overrides it unconditionally, which is how CI keeps its own per-workspace cache
(`.github/workflows/{ci,release}.yml`) unaffected. Two consequences worth knowing: `rm -rf build/`,
and `./scripts/build.sh -x` with it, no longer clears the dependency sources — delete the cache
directory by hand when you genuinely want them re-downloaded, and note that doing so bills every
other worktree on the machine; and the compiler-output and build trees above stay deliberately
per-tree — only the immutable, content-addressed dependency sources are shared, which is why
concurrent configures are safe (CPM takes a `file(LOCK)` per package directory before downloading
into it).

## Code Structure

- `src/config/`: configuration parsing and simulation config data
- `src/core/`: math, optics, simulator, filters, buffers, ray paths
- `src/raypath/`: the raypath analysis module — module surface rules live in `src/raypath/AGENTS.md`
- `src/gui/`: GUI app, panels, preview, file IO, poller
- `src/server/`: server-side render, consumer, stats, C API bridge
- `src/util/`: logger, threading, queue, arguments, color data
- `src/analytic/`: the published analytic library `liblumice_analytic` (design in
  `doc/interfaces/analytic-api.md`); which headers each shared library exports is declared once
  in `cmake/export_surfaces.cmake`.
- `src/include/`: the C API, one header per capability — `lumice_{base,scene,render,editor,engine,raypath}.h`
  (no umbrella header) plus `lumice_analytic{,_core}.h`, bridged by `src/server/c_api_<capability>.cpp`.
  The one non-capability leaf, `lumice_api_version.h`, solely owns `LUMICE_API_VERSION` and is included
  only by the few TUs that textually use the macro — a value change would invalidate the whole
  includer closure's compile cache.
- `test/`: unit tests, GUI tests, and E2E tests (operating rules: `test/AGENTS.md`)

Layers: `cmake/lumice_layers.cmake` assigns every file under `src/` (the shell — `gui/`, `launcher/`,
`main.cpp` — aside) to one of nine layers, lowest first: foundation, analytic, scene, view, sim,
render, engine, raypath, capi. It is the single statement of the layering, read by both sides: CMake
builds `lumice_foundation_obj` from its foundation list (`liblumice_analytic` links foundation and
the analytic kernel only, `doc/interfaces/analytic-api.md` §2.1), and `scripts/check_policies.py`'s
`layer-inversion` rule rejects any `#include` that points at a higher layer, any `src/` file the
manifest does not own, and any manifest entry naming no file — so a new source file goes into the
manifest in the same change. The existing inversions are listed in `cmake/lumice_layer_allowlist.txt`,
which only shrinks — fixing an inversion means deleting its line, and an entry whose edge is gone
fails the check.

## Style and Engineering Rules

- Follow the repository `.clang-format` and `.clang-tidy` rules.
- Naming: types and functions `CamelCase`; variables `lower_case`; private members `lower_case_`; constants `kCamelCase`.
- Prefer `const`, `constexpr`, smart pointers, `nullptr`, and `override`.
- Do not use raw `new` / `delete`.
- Do not use `using namespace`.
- Keep code comments in English.
- Logging: all diagnostic output goes through the logger — `ILOG_*` (core/server, `src/util/logger.hpp`)
  or `GUI_LOG_*` (GUI, `src/gui/gui_logger.hpp`). A raw `printf` / `fprintf` / `std::cout` / `std::cerr`
  bypasses the unified sinks, the level filter and the format, so it is invisible exactly where a user
  looks: the GUI log panel, the file sink they enabled, and — on Windows, where the GUI calls
  `FreeConsole()` unless a diagnostic flag was passed — anywhere at all. Two `src/` files are exempt —
  named owners, not per-line escape hatches: `src/main.cpp`, whose stdout **is** the CLI's product
  output (two of its lines are parsed contracts — the `[BENCHMARK]` JSON and `ColorClassSignal:`, so
  a log prefix would break the e2e suite); and `src/util/fatal.hpp`, the single owner of the pre-abort
  trap, where unbuffered stderr is the whole point (a per-message-unflushed file sink can lose the
  line precisely when it matters). For an unrecoverable invariant call `lumice::FatalAbort(...)`
  instead of hand-rolling print-then-`abort()`.
  `src/launcher/` sits outside the rule by construction rather than by name: it holds only the Windows
  release shell's CPUID probe and delay-load wiring (the Linux side needs no such file — glibc-hwcaps
  picks between the two engine `.so` files), is compiled into the CLI/GUI shell targets rather than
  `lumice_obj`, and runs before the engine DLL is even loaded — there is no logger yet to bypass; its
  few error lines go to stderr for the same reason. The checker never sees it — its scan covers
  C++/CUDA/Metal suffixes, not `.c` — consistent with what the rule guards; a `.c` file that *does*
  link the engine would still be wrong.
  Enforced by the `no-bare-print` rule in `scripts/check_policies.py`, which also scans `.cu`/`.metal`
  so a GPU backend cannot reopen the side channel. `test/` is deliberately out of scope: test binaries
  are their own harness with no app logger to bypass, and some of their output is a parsed contract
  (the regen driver reads gui_test's `PSNR=` line). Device-side `printf` in a CUDA kernel stays a
  legitimate debugging tool — the gate stops it landing, not being added while working.
- Public API boundary: `src/gui/` code must only access core/config functionality
  through the C API. Direct `#include` of `core/` or `config/` headers from `src/gui/` is
  prohibited, and of the public headers under `src/include/` it may include only the engine's
  capability headers, `src/include/lumice_*.h` (the engine export surface of
  `cmake/export_surfaces.cmake` minus the analytic family — so not `lumice_analytic*.h`). Each file
  includes the capability headers declaring what it uses, not a gathered set. Enforced by the
  `gui-api-boundary` rule in `scripts/check_policies.py`.
  `src/util/` is deliberately outside that prohibition, and the exemption has a shape:
  what may live there and be included from both sides is a **pure, stateless helper that
  carries no simulation or configuration semantics** — `bit_utils.hpp`, `color_space.hpp`,
  `label_viewport_clamp.hpp`, `annotation_line_width.hpp`. The gate exists so the GUI cannot reach
  into the engine's data model and pin its layout by including it; forcing an exempt helper through
  the C API would write the rule twice, once per side — the divergence the boundary is there to
  prevent. So: a rule BOTH renderers must obey, expressible without a core or config type, belongs
  in `src/util/`. Anything that needs to see a `RenderConfig`, a `SimData` or a crystal does not,
  and still goes through the C API.
- Raypath typed dependency boundary: `scripts/check_policies.py`'s `raypath-typed-boundary`
  checks public value and query-facade include closures (quoted and angle-bracket in-tree),
  the bridge's direct `detail/` includes, and codec-to-query header dependencies. Its
  narrow scope and runtime proof limits are in `doc/raypath/typed-results.md` (Dependency
  boundary); module ownership lives in `src/raypath/AGENTS.md`.
- Environment variables: before adding any new `std::getenv`, apply the decision gate in
  `doc/env-var-policy.md`. User-facing behavior switches must go through CLI/config/API,
  not env vars (env causes silent per-machine drift). Env is fine for dev/experiment knobs
  (centralized + logged on startup) and test/build infra. All `LUMICE_*` env reads live in
  `src/util/env_knobs.cpp` and are enforced by `scripts/check_policies.py` (CI `policy` job +
  local pre-commit hook, installed via `./scripts/install-hooks.sh`).
  The **write** side has its own single owner, on the test side only: a test that has to set or
  clear a variable calls `lumice::test::SetEnvVar` / `UnsetEnvVar` from `test/support/env_var.hpp` —
  the one place in the tree holding the `_WIN32` branch (`_putenv_s` vs POSIX `setenv`/`unsetenv`;
  MSVC declares neither). This is not enforced by a
  checker, deliberately: a bare `::setenv` is a *compile error* on MSVC, not a silent failure, which
  is the line the other four gates are drawn on (the CI `cuda-compile` jobs back it up — the breaking
  call sites hid behind platform `#if`s). `src/` never writes env vars at all.
- Working-note references: prose in a tracked file must not cite `scratchpad/` artifacts —
  task ids (`task-<name>`, `explore-<name>`, `scrum-<name>`, `chore-<name>`), per-task note
  filenames (`plan.md`, `progress.md`, `SUMMARY.md`), `code-review-0N`, `round-N`, or
  `scratchpad/` paths. That tree is git-ignored and archived per task, so the reference dangles;
  state the mechanism itself instead. Permanent anchors are fine and should stay: PR numbers, commit
  hashes, `file:line`, code symbol names. Applies to any long-lived prose — production comments, test
  docstrings, CI config comments, `doc/` alike. Enforced by `scripts/check_new_refs.py`, a **second,
  diff-scoped entry point** next to `check_policies.py`: it reads only the lines a change *adds* (CI
  `new-refs` job vs merge-base; pre-commit on the staged diff), and only natural-language regions —
  comments, docstrings, Markdown, never string literals. No inline exemption, by design: if prose
  genuinely needs such a reference, the rule is wrong and the rule should change. Exempt files are
  limited by one criterion — the file's *subject* is this working-notes system itself (this file,
  `CLAUDE.md`, the root `.gitignore`); citing the system is not grounds for exemption, it is the thing
  being checked.
  **The checker is the rule, not an approximation of it.** If `check_new_refs.py` passes, the change
  is compliant — full stop. Do not flag a reference in review that the checker accepts, and do not
  appeal to the rule's "spirit": the checker deliberately misses some citation forms (the trade-offs
  are argued at each pattern in the script) and those misses are accepted. Rationale: the harm is measured
  *low* — 41% of tracked files carried such references for years with no observed cost — so
  an unbounded review-time judgement costs more than it buys. Tightening is legitimate only by changing
  a pattern in the script, never by case-by-case escalation in review.

## Testing and Platform Notes

- CLI, core, and unit-test flows should remain cross-platform.
- Operating rules for the test tree live in **`test/AGENTS.md`**: the gui_test platform matrix (built
  on three platforms, run only on the llvmpipe leg), the three-way placement split for new `src/gui/`
  tests, the gui_test isolation discipline, the shared libraries pytest loads, the E2E layout, and the
  reference-regeneration workflow. Which groups tolerate a software rasterizer is measured — before
  widening the CI filter or dispositioning a visual-regression failure, follow the checklist in
  `doc/testing/testing-architecture.md` §4.6.
- Three more diff-scoped checkers beside `check_new_refs.py` (same "checker is the rule" discipline;
  pre-commit hook on the staged diff, CI `new-refs` job vs merge-base): `scripts/check_new_gui_tests.py`
  rejects a newly registered gui_test case that does not use its `ImGuiTestContext`;
  `scripts/check_loop_fatal_asserts.py` rejects a fatal assert (or non-fatal report) followed by code
  that keeps driving the same test context in a repeatedly-executing scope of `test/`;
  `scripts/check_new_test_tus.py` rejects a PR adding more than five test TUs unless the same diff
  appends declaration entries to the append-only ledger `test/tu_cost_ledger.json` (its hub-closure
  half is notice-only, on the `build` job's judging leg). Full rule statements: the scripts'
  docstrings, testing-architecture.md §4.9, and §7.9.
- **Test-duration registry — a runtime gate, not a fifth diff-scoped checker.** `test/duration_registry.json`
  is the single authority for two kinds of quantity — per-test time and phase budgets — and its v4
  shape adds the build phase's adaptive per-miss ruler beside the absolute backstop, and the ctest
  phase's machine-speed normalization (`machine_factor`: the budget scales by the median
  measured/calibrated ratio of a 7-id ruler subset of the per-test report). The runtime
  input does not exist at pre-commit time, so the gate is not part of `check_policies.py`; there is no
  per-test/env/inline exemption. The sole exception is the maintainer-authorized, one-run cold-build
  protocol for phase budgets containing `build`; it never excuses test runtime or correctness,
  and a PR whole-run/job
  rerun cannot consume it again. Operating rules: `test/AGENTS.md`; full rules, the 60 s / 2×
  calibration and the per-miss constants: `doc/testing/testing-architecture.md` §7.7. The cold-build protocol's
  single authority is §7.8.
- E2E split essentials: `./scripts/test.sh <quick|full|pr|inner>` is the recommended entry point for a
  local run (one call, a single per-layer summary ending in one `RESULT:` line). `inner` runs a
  diff-derived subset of these layers during iteration and is covered by the test-scope table
  below; `quick`/`full`/`pr` are the closing scopes and are unchanged. Bare `pytest
  -v` runs the fast subset directly — pinned by `pyproject.toml`'s `addopts` (`-m "not slow"`), so
  it is structurally true and matches CI's fast leg; `pytest -v -m ''` is the escape hatch back to
  the full set, and any marker CI
  never uses must be passed explicitly. `@pytest.mark.slow` tests require the shared-lib build
  (`./scripts/build.sh -sj release`) — run them before opening a PR that touches the simulator
  core, query filter, or C API surface. That build produces **three** shared libraries, pytest loads
  the second, and the plumbing is in `test/AGENTS.md`.
- **Test-scope table** — which command fits a given situation:

  | Situation | Run |
  |---|---|
  | Iterating on a change — fast feedback scoped to what the diff touches | `./scripts/test.sh inner` |
  | Verify the change you just made (the closing gate) | `./scripts/test.sh quick` |
  | Right after a static rebuild, before moving on | `./scripts/test.sh quick` |
  | Broader regression sweep — unsure how far a change reaches | `./scripts/test.sh full` |
  | Before opening a PR — reproduce the CI e2e-slow shape locally | `./scripts/test.sh pr` |
  | Confirm one specific gate/test actually fails on a deliberately-broken state (not a general sweep) | run that one check directly against the broken state, then again after the fix: `ctest -R <label>`, `pytest <file>::<test> -m ''`, or `gui_test --filter <name>` |

  `inner` derives its layers from the current diff (`merge-base main..worktree`, or
  `--files <paths>`): changed `src/` files go through the configure-time layer map (generated
  from `cmake/lumice_layers.cmake` — the one manifest, no second path→layer table), and the
  mapped faces pick the test layers (a raypath edit runs the raypath/analytic gtest suites
  through a direct binary call; a docs-only diff runs no build at all), plus the four
  diff-scoped checkers and `format.sh --check` on every run. It is deliberately coarse and
  prints its derivation chain — **iteration = inner, closing = quick**: whatever a face misses
  is caught by the quick gate before anything merges, so an inner `RESULT: PASS` never closes
  a task. `inner` builds with plain `cmake --build`, never `build.sh` — build.sh passes
  `-DBUILD_TEST` back unconditionally, so a flagless call would reconfigure the hot `-gtj`
  tree and tear its test targets out. Keep `-gtj` standing for the task's duration and neither
  `inner` nor the closing `quick` pays a flavor-switch rebuild.

  What each row **costs** in measured seconds, what each scope catches that the cheaper one
  structurally cannot, the budget rule naming who answers for the spend, and where each test runs
  beyond these commands (PR layer, `main` only, the reference machines' daily schedule, release, a
  developer machine; no nightly) are in `doc/testing/testing-architecture.md` §7 — deliberately not
  duplicated here: two copies of a measurement drift apart. Moving a test out of the PR layer means
  giving it one of those places first.

- **Judgment discipline**: a command's exit code is the only thing that says pass/fail, and two common habits obscure it in different ways. A pipe (`cmd | tail`) leaves `$?` reporting `tail`'s exit code, not `cmd`'s, unless the shell has `set -o pipefail` — so a failing command piped through one still reads as success. A redirect (`cmd > log 2>&1`) does not touch `$?` at all, but it does move the PASS/FAIL text out of sight, so a finished run still needs a second call (`grep`/`cat` on the log) just to learn the outcome it already produced. Read `$?` straight off the foregrounded command, or use `./scripts/test.sh`, which already does this for every layer.
- Performance diagnostics and workflows are documented in `doc/testing/performance-testing.md`.

## Logging and Troubleshooting

- `VERBOSE` is a project-defined log level between `DEBUG` and `INFO`.
- When debugging intermittent issues, first map the full data flow, then audit silent-return paths and add observability before changing behavior.
- If a known-good branch or prior fix exists, diff it first before starting fresh analysis.
- Do not split one decision across multiple threads or modules when a single owner can make it.

## Collaboration Constraints

- `config.json`, `test.json`, `scratchpad/`, remote test output files, and most generated artifacts are intentionally git-ignored.
- Do not use `git add -f` to force-track ignored files. If a file is ignored and you are unsure, stop and ask first.
- Reference images under `test/e2e-correctness/references/*.jpg` and `test/gui/references/*.jpg` are explicitly unignored and may be tracked normally.
- **Release**: a release is a chore — not a task, and not a commit made directly on `main` — worked
  in its own worktree like any other change, and merged through a PR. Its `CHANGELOG.md` section is
  written in that chore, in one batch, and the input to "is the section complete" is the mechanical
  PR enumeration under `CHANGELOG.md`'s "Sourcing" rule (merge commits, first-parent non-merges,
  `LUMICE_API_VERSION` diff), never memory of what was merged: every PR in that output gets a
  recorded disposition, entry or one-sentence reason for none. `scripts/version.py set` validates
  the hand-written section rather than cutting one from an accumulator, so there is no
  `[Unreleased]` section to keep current between releases. The tag is the owner's to create, on
  the merge commit. Steps, in order, are in `CONTRIBUTING.md` "Release Process" — that is the one
  copy; do not restate them here.
- CI (`.github/workflows/ci.yml`) triggers on `pull_request` and on `push` **filtered to `main`**.
  Every job therefore runs exactly once per commit: once while the change is a PR, once again when
  it lands on `main`. The filter is what makes that true — without it a push to a PR branch fires
  both events and every unguarded job runs twice. The consequence when adding a job: on a PR branch
  there is no `push` event at all, so a guard of `github.event_name == 'push'` means "`main` only"
  (which `benchmark-summary` genuinely wants, since it writes the gh-pages benchmark history) and
  is a silent loss of PR coverage anywhere it was meant to mean "always".
  The `on:` block's own comment in `ci.yml` is the authority on this — it is the thing that has to
  be right for the workflow to behave, and it carries the details this summary leaves out (what the
  duplication cost, and why tag pushes no longer run this workflow). Change the triggers and that
  comment is what you edit; this paragraph is a summary that has to be re-checked against it.
- **Where a change lives, and from where it is made** — two criteria, not advice. Both attach to
  an *action* rather than to a judgement, because the incidents that produced them were committed
  by an agent that knew the rule and never asked itself whether it applied:
  1. **Before the first edit to `src/` or `test/`, a `scratchpad/` task directory for that change
     must already exist.** No directory → create one first (`scratchpad/tasks.md` is the ledger);
     do not write code first. The trigger is the edit itself, not "does this feel like a task" —
     the moment the rule hangs on that judgement, one wrong judgement switches the whole rule off.
     This is also the reason the retrieval discipline under "Knowledge Base & Working Discipline"
     below works at all: a change with no task directory leaves no notes for the next session to
     retrieve.
  2. **A task is worked on a branch in its own linked git worktree; the main worktree stays clean.
     N=1 is not an exception.** A single task gets a worktree too, because the main worktree is
     shared by every task on the machine and "nothing else is running right now" is exactly the
     judgement (1) refuses to rely on — when this rule was written, three tasks were live in
     three worktrees at once. Shape: `git worktree add ../ice-halo-wt-<n> -b feat/<name> main`,
     then `ln -s "<main-repo-abs-path>/scratchpad" ../ice-halo-wt-<n>/scratchpad` (that symlink
     is already covered by `info/exclude`, which lives in the common dir and so applies to every
     worktree; do not add it again). Worktrees are cheap here on purpose — dependency sources are
     cached per machine, see "Build trees..." above. One consequence worth knowing:
     `.git/hooks/pre-commit` is shared across worktrees (hooks live in the common dir) but does
     `cd "$(git rev-parse --show-toplevel)"`, which inside a worktree resolves to the worktree
     root — so a commit there runs the worktree's own copy of the four checkers, and a branch that
     edits a hook script does not see its own edit take effect until it is merged.
  Criterion 2 is machine-enforced at the point every write converges on, whatever tool produced
  it: `git commit`. Step 0 of `scripts/hooks/pre-commit` runs
  `scripts/hooks/worktree-guard.py pre-commit`, which refuses the commit when the staged set
  touches `src/` or `test/` and the toplevel is the *main* worktree (git's own structural test:
  `rev-parse --git-common-dir` resolves to `<toplevel>/.git` only there). The commit is the gate
  for a reason that was learned the expensive way: the first version of this guard was a Claude
  Code `PreToolUse` hook on `Edit|Write` alone, and it was correct — and it would not have
  caught the incident that produced it, because that edit was written through Bash
  (`python3 - <<'PY'`), which a tool-call hook never sees. The same is true of shell
  redirection, `sed -i`, `tee`, a heredoc; widening the hook to `Bash` would mean parsing
  command lines for writes, a heuristic that both misses and false-alarms. The commit does not
  care how the file was written, and the failure shape being closed ("edit a lot, then squeeze
  it into one big commit") lands on it by construction. Its honest boundary: a change that is
  never committed is invisible to it — and also never reaches the repository, which is a
  different blast radius (a dirty main worktree, not a polluted history) and the residue this
  design accepts. The escape is git's own explicit `git commit --no-verify`, not a switch of
  ours — no env var, no sentinel file — so an owner-authorized one-off stays visible in the
  shell history. The `PreToolUse` hook is kept as a second line, not the first: same script,
  `claude-pretooluse` mode, enabled per machine by a git-ignored `.claude/settings.local.json`
  in the **main** checkout only (a linked worktree needs none — everything is allowed there),
  and it fires at the first keystroke rather than at the end. Its settings command tests that
  the script exists before running it and degrades *loudly* otherwise (one stderr line, edit
  proceeds) — the first enablement switched it on while the script still lived on an unmerged
  branch, and a bare `python3 <missing path>` returns exit 2, which Claude Code treats as a
  blocking error, so every `Edit`/`Write` in the main worktree was refused, not just `src/` and
  `test/`. The canonical settings content is a constant in the script (`print-settings` writes
  it; the reasoning the JSON file cannot carry is in the module docstring), and because a
  git-ignored file is invisible to every diff, `check-settings` compares a checkout's copy
  against it field for field (structural equality on the parsed JSON, so a narrowed matcher
  reads as stale and reformatting does not) — `./scripts/install-hooks.sh` runs that last, so the one install
  step after a clone or a merge also reports whether the Claude side is current, absent, or
  stale. The main-worktree predicate lives in that script and nowhere else. Criterion 1 (the task directory) is not machine-enforced; it is the criterion the commit
  gate makes cheap to honour, because by the time a worktree exists the task directory is the
  natural place to have named it.

## Documentation Index (`doc/`)

Valuable design/architecture docs live in `doc/` (tracked); most have a `_zh`/`.zh.md` Chinese sibling. Each entry below is the trigger map — the doc itself is the sole content authority, this index carries no summaries. Subsystem operating rules live next to the code: `test/AGENTS.md` (test tree) and `src/raypath/AGENTS.md` (raypath module surface).

- **Overview / guides**: `README.md`, `architecture.md`, `developer-guide.md`, `gui-guide.md`, `configuration.md`
- **Core / rendering architecture**: `accumulator-consumer-architecture.md`, `raypath-rayseg-architecture.md`, `coordinate-convention.md`, `crystal-orientation-sampling.md`, `ev-pipeline-architecture.md`, `adaptive-brightness.md`, `filter-architecture.md`
  - `ev-pipeline-architecture.md` §2.4–2.8/§7 + `adaptive-brightness.md` §2–3 — the two exposure anchors (`ev_mode`: relative default / absolute), the §2.8 dual-side common-anchor law, both migration laws, photometry rules. Read before touching exposure normalization / `ev_mode` / `ExposureScale()` / `emitted_energy` / whether metering sees `visible` / GUI preview brightness / `anchor_l99_sky` or `axis_solid_angle` consumers.
  - `raypath-symmetry.md` — filter/colour-ref P/B/D is label equivalence, the analysis list's is physical equivalence (`SymmetrySemantics`); the physical P gate is a per-element retained-rotation set (roll periodicity + the pole-fold 180° subgroup), not a boolean. Read before filter symmetry / analysis-list grouping / missing-face warnings / Exclude generation / anything reading `SymmetryGating`.
  - `overlay-label-placement.md` — curve-centric overlay text-label placement. Read before label-placement work.
  - `numerical-robustness.md` — geometric numerical-stability conventions (7 rules). Read before adding or **modifying** any geometric predicate.
  - `crystal-geometry-representation.md` — crystal geometry generation + in-memory representation (closed-form planes + face-existence mask; as-built). Read before redesigning geometry (predicate-only changes: `numerical-robustness.md`).
  - `near-pole-area-measure-sampling.md` — near-pole orientation sampling blueprint (area-measure importance sampling). Read before near-pole sampling / Rayleigh path / `ComputeJacobianEnvelope` / GPU device gen sampler changes.
- **GUI**:
  - `gui-preview-lifecycle-architecture.md` — worker × display clock decoupling, invariants I1–I7. Read before preview lifecycle / poller-server handoff / completion determination.
  - `gui-state-governance.md` — GUI state governance (field→tier classifier, scene-encoding projections, user defaults, view-side copy resync). Read before GUI state transitions / colour display channel / document-switch resets / simulation-lifecycle display coupling / user defaults / export-config field filling / Analysis-list lifetime / any view-side copy, modal buffer, position reference or pick mode.
  - `gui-visual-language.md` — visual language (typography / palette / rhythm) + the window-sizing four-tier table. Read before GUI appearance / panel typography / control shape / any window's resize behavior.
  - `gui-layout-architecture.md` — panel re-organization blueprint whose **direction was rejected by beta users — not a todo**; the §0 diagnosis and §5 overturned-forms list survive the rejection. Read before any re-paneling or panel-level feature proposal.
  - `gui-custom-spectrum-and-raypath-color.md` — custom discrete spectrum (landed) + per-raypath colour marking blueprint + §4.8 composite redesign. Read before spectrum / raypath-colour work / `component_compositor.cpp`.
- **Raypath analysis**:
  - `raypath-analysis.md` — the feature roadmap: recognition delivered, stripping shelved by owner decision (**not a todo**); workspace layout; repo positioning. Read before adding any raypath-analysis feature or re-raising "toggle an arc live".
  - `raypath-analysis-panel.md` — the delivered panel's mechanism record (dedicated-stat-run form, chain-id plumbing, ROI, the Exclude loop). Read before panel work / chain ids / histogram consumer / the analysis C API.
  - `feature-diagnostic-discovery-research.md` — generic feature-discovery research history + product-landing record. Read before designing feature discovery / attribution / their shared interfaces; research history is not the current API's acceptance.
  - `cases/raypath-analysis-cases.md` — the census of canonical test scenes (append-only numbering). Take test scenes from here, not ad hoc.
  - `raypath-cli-output.md` — the `Lumice raypath` subcommand's JSON field reference (target schema1 / report schema3). Read before output fields / subcommand options / that C entry.
  - `raypath-schema3-layered-design.md` — the schema3 layered-design record. Read before schema3 report semantics / geometry-layer enums / MC evidence / `src/analytic/` module-C surface.
  - `typed-results.md` — typed result ownership, shared structure/quantity services, identity-bound references and the field migration ledger. Read before modifying result types / claim payloads / typed completion / result-to-JSON assembly.
  - `gap1-family-routing.md` — the measure-to-family routing (label-free routing, the fixed/narrow/wide concentration trichotomy, the freezing ladder with the per-object survival filter, the Lumice-born restricted-family rank primitive `src/analytic/family_rank.hpp`). Read before family routing / ladder levels / freeze decisions / restricted-family rank or fold semantics / widening the routing's v1 refusal domain.
- **print mode**: `print-mode-subtractive-ink.md` — the single design authority for the print display mode. Read before print-mode field chain / operator / colour-channel exclusivity / headroom warnings.
- **C API**: `c_api.md`; `capi-lifecycle-architecture.md` §9 — result lifetime and ownership invariants (read before result lifetime / `DoSnapshot` publishing / C API borrow contracts); `api-layering-and-product-lines.md` (read before shipping dynamic libraries / new product lines / adding or moving C API functions across headers / re-raising a repo split); `analytic-api.md` (read before new library targets / export lists / log takeover / packaging / the first parsing module).
- **GPU / Metal route** (read these before touching the GPU path):
  - `seam-design.md` — the `TraceBackend` host/device seam blueprint; §5 = single-engine three-clock target architecture; §3.6 = why GPU must not mirror the CPU pipeline; the header carries the implementation discipline (reuse verified implementations, blueprint is the final judge).
  - `gpu-route-history.md` — systematic retrospective of the GPU migration (#250→268) + leftover-item ledger; §9 = single-engine arc high-point.
  - `trace-backend-frame-lifecycle.md` — Metal frame lifecycle, multi-MS transit, parity harness methodology.
  - `gpu-single-engine-implementation.md` — §0 as-built handover notes (read first when taking over the GPU route); §10 XYZ device-plane drain-window precision (read before `AccumXyzToPixel` / `FoldDeviceXyzBatch` / fold cadence / `LUMICE_XYZ_DRAIN_BATCHES` semantics); §8.1 CPU-side standard-light wavelengths (read before CPU wavelength sampling).
- **Perf / testing**: `performance-testing.md`, `windows-remote-testing.md`, `xyz-stats-tool.md`
  - `geometry-randomization-value-and-measurement.md` — the two-axis framework (variance vs bias) + bias-axis measurement methodology. Read before designing randomization quality/throughput gates.
  - `geometry-randomization-perf.md` — the randomization cost model + frontier map. Read before optimizing the randomization path (pair with the value doc).
  - `machines.md` — role → machine binding, the single source of truth; when machines change, change only this file.
  - `gpu-remote-cuda-build-testing.md` — the CUDA build + parity/correctness protocol (headless). Read before touching `cuda_trace_backend.*` / the three-backend shared headers / SimData / simulator.
  - `testing-architecture.md` — the authoritative test-organization spec; §7 = the test-scope contract, §4.11 = the comparison-ruler master table. Read before adding or reorganizing any test, claiming a change shortens CI, or picking a ruler for a new image comparison.
- **Engineering policy**: `env-var-policy.md` — the env-var usage policy and the decision gate to answer before adding any new `getenv`.
- Example config: `examples/config_example.json`

## Knowledge Base & Working Discipline

This project carries a deliberate accumulated memory. Its value depends on it being
**retrieved**, not just stored. The recurring failure mode is starting each session like a
newcomer and re-deriving decisions the owner already settled. Avoid it:
(The other half — every change to `src/` or `test/` has a task directory before the first edit,
and is made in a linked worktree, not the main one — lives under "Collaboration Constraints".)

- **Retrieve before re-deriving.** Before starting work on a continuing/recurring topic, FIRST grep
  the scratchpad backlog and ledger for it, read the relevant prior explore/scrum summaries and the
  `doc/` index above, then **explicitly continue prior threads** rather than starting fresh; connect
  new evidence that conflicts with a prior conclusion.
- **Don't re-measure what's already de-risked.** Before running ANY benchmark or experiment on a
  recurring topic, grep the relevant prior experiment log for that exact measurement — harvest the
  number, do NOT re-run it. Read at row level, not summary level: the rows are what stop a re-run.
  A design explore's job is the unresolved frontier; if your "finding" restates a prior note, stop.
- **The scratchpad system is the source of truth for in-flight reasoning** — the task ledger, the
  backlog, per-effort notes, extracted lessons. Git-ignored working memory; its absence from git is
  not absence of knowledge.
- **Promote durable design docs out of `scratchpad/` into `doc/`**, adding them to the index above: left in git-ignored notes they get lost.
- **Think at the architecture level before decomposing into tasks** — resist acting on the first promising small direction before that question is reasoned through.
- **A completeness claim carries the same burden of proof as adding code.** "Covers all N with zero
  omissions", "orthogonal", "closed" read as achievements and pass review unchallenged, while their
  opposites have to be argued for — so completeness wins every conflict, including against the task's
  own declared budget. A coverage assertion over an enumerated surface must say why each member is
  worth covering; where a per-member value measure exists or is cheap (escape-defect density,
  call-site count, blast radius, user reachability), an equal-weight partition needs a stated reason —
  "it is the whole set" is not one. Measured instance (PR #261; the four
  supporting measurements are inlined here because the PR description does not carry them):
  a test-suite rebuild pre-committed to −30% de-commented test lines against a 21,336-line
  baseline pinned by two independent counters (52/52 files, zero diff) landed at −8.7%,
  and the coverage backfill demanded by "zero omissions" accounted for 1,000–1,500 lines,
  roughly a third of the miss — while escape-defect density over the same files, measured
  before the partition was drawn, spanned 8× (47.2 vs 6.2 defects per kloc); the equal-weight
  split discarded that measurement. **Not** a mandate to cut —
  whether a leaner suite would have let more defects escape is untestable; what is required is the
  justification, not the reduction.
