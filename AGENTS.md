# AGENTS.md

## Project Overview

Lumice is a C++17 ice halo ray-tracing simulator. It reproduces halo patterns by tracing light through ice crystals and supports CLI, GUI, unit tests, E2E tests, and performance-oriented workflows.

Core conventions:

- namespace: `lumice`
- public API boundary: the engine's capability headers `src/include/lumice_{base,scene,render,editor,engine,raypath}.h`
  (one per capability; there is no umbrella header)
- source layout: `.hpp` headers and `.cpp` implementations
- build system: CMake + Ninja
- dependency management: CPM.cmake

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

# Tests — scripts/test.sh is the recommended entry point; see the test-scope
# table under "Testing and Platform Notes" for which one fits a given situation.
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

# Engineering-policy gate (env-knob centralization/registration, GUI API boundary, using-namespace,
# layer inversion)
python3 scripts/check_policies.py    # whole-tree: what state is the repo in?
python3 scripts/check_new_refs.py --staged        # diff-scoped: what did you just write?
python3 scripts/check_new_refs.py --range A..B    # same, over a commit range (what CI runs)
./scripts/install-hooks.sh           # install the non-interactive pre-commit hook (one-time)
```

Build trees and install trees are per-flavor: `-s` (shared) and no `-s` (static) never share a
directory, so both stay warm and switching between them does not force a rebuild. `-t`/`-g`/`-b`
choose *what* to build (tests / GUI app / benchmarks) and compose with either flavor — only `-s`
switches the flavor itself, and it defaults to static when omitted, so `-t`/`-g` alone never
produce a shared library. Release artifacts are installed to `build/cmake_install/<flavor>/`; the
CMake build tree is `build/cmake_build/<flavor>/` and compiler output lands in
`build/<BUILD_TYPE>/<flavor>/{bin,lib}/`.

Dependency **sources**, by contrast, are cached per-*machine*, not per-source-tree: CPM downloads
land in `$HOME/.cache/lumice-cpm` by default (`$USERPROFILE/.cache/lumice-cpm` on a Windows shell
that sets no `HOME`; and if neither variable exists, the old repo-local `build/cpm_cache`, so a
configure can never fail for want of a home directory). That is what makes a fresh clone or a new
`git worktree` cheap — it reuses whatever this machine has already downloaded instead of re-fetching
~300MB of the same 13 packages, which is the cost that was actually being paid: four trees on one
machine each held their own ~300MB copy. `CPM_SOURCE_CACHE` still overrides it unconditionally —
the default is only computed when neither the CMake variable nor the environment variable is set —
which is how CI keeps its own per-workspace cache (`.github/workflows/{ci,release}.yml`) unaffected.
Two consequences worth knowing. `rm -rf build/`, and `./scripts/build.sh -x` with it, no longer
clears the dependency sources, because they are no longer under `build/`; delete the cache directory
by hand when you genuinely want them re-downloaded, and note that doing so bills every other
worktree on the machine for the re-download, not just yours. And the compiler-output and build trees
above stay deliberately per-tree — only the immutable, content-addressed dependency sources are
shared, which is why concurrent configures are safe (CPM takes a `file(LOCK)` per package directory
before downloading into it).

## Code Structure

- `src/config/`: configuration parsing and simulation config data
- `src/core/`: math, optics, simulator, filters, buffers, ray paths
- `src/raypath/`: the raypath analysis module. Its surface to the engine is the three public
  headers `path_feature_report.hpp` / `single_path_analysis.hpp` / `single_path_json.hpp` (the
  bridge `src/server/c_api_raypath.cpp` compiles exactly these); everything under
  `src/raypath/detail/` is internal implementation, includable only by the module itself and
  its own tests (`doc/raypath/raypath-cli-output.md` §8).
- `src/gui/`: GUI app, panels, preview, file IO, poller
- `src/server/`: server-side render, consumer, stats, C API bridge
- `src/util/`: logger, threading, queue, arguments, color data
- `src/analytic/`: implementation of the published analytic library `liblumice_analytic`
  (header `src/include/lumice_analytic.h` over `lumice_analytic_core.h`, prefix `LUMICE_ANALYTIC_`;
  design in `doc/interfaces/analytic-api.md`). The engine libraries export the capability too: which headers
  each shared library exports is declared once in `cmake/export_surfaces.cmake`.
- `src/include/`: the C API, one header per capability — `lumice_{base,scene,render,editor,engine,raypath}.h`
  (the engine surface; no umbrella header) plus `lumice_analytic{,_core}.h` (the analytic capability). The C
  bridges are `src/server/c_api_<capability>.cpp`, one per header. One non-capability leaf header also
  lives here: `lumice_api_version.h` solely owns `LUMICE_API_VERSION` and its change history, and
  no header includes it — a macro value change would invalidate the whole includer closure's compile
  cache, so only the few TUs that textually use the macro include it.
- `test/`: unit tests, GUI tests, and E2E tests

Layers: `cmake/lumice_layers.cmake` assigns every file under `src/` (the shell — `gui/`,
`launcher/`, `main.cpp` — aside) to one of nine layers, lowest first: foundation, analytic, scene,
view, sim, render, engine, raypath, capi. It is the single statement of the layering, read by both
sides: CMake builds `lumice_foundation_obj` from its foundation list (`liblumice_analytic` links
foundation and the analytic kernel only, `doc/interfaces/analytic-api.md` §2.1), and
`scripts/check_policies.py`'s `layer-inversion` rule rejects any `#include` that points at a
higher layer, any `src/` file the manifest does not own, and any manifest entry naming no file. A
new source file therefore goes into the manifest in the same change. The existing inversions are
listed in `cmake/lumice_layer_allowlist.txt`, which only shrinks — fixing an inversion means
deleting its line, and an entry whose edge is gone fails the check.

## Style and Engineering Rules

- Follow the repository `.clang-format` and `.clang-tidy` rules.
- Naming:
  - types: `CamelCase`
  - functions: `CamelCase`
  - variables: `lower_case`
  - private members: `lower_case_`
  - constants: `kCamelCase`
- Prefer `const`, `constexpr`, smart pointers, `nullptr`, and `override`.
- Do not use raw `new` / `delete`.
- Do not use `using namespace`.
- Keep code comments in English.
- Logging: all diagnostic output goes through the logger — `ILOG_*` (core/server, `src/util/logger.hpp`)
  or `GUI_LOG_*` (GUI, `src/gui/gui_logger.hpp`). A raw `printf` / `fprintf` / `std::cout` / `std::cerr`
  bypasses the unified sinks, the level filter and the format, so it is invisible exactly where a user
  looks: the GUI log panel, the file sink they enabled, and — on Windows, where the GUI calls
  `FreeConsole()` unless a diagnostic flag was passed — anywhere at all. Two files under `src/` are
  exempt, and both are exemptions with a name and an owner rather than a per-line escape hatch:
  `src/main.cpp`, whose stdout **is** the CLI's product output (two of its lines are parsed contracts —
  the `[BENCHMARK]` JSON and `ColorClassSignal:`, so a log prefix would break the e2e suite); and
  `src/util/fatal.hpp`, the single owner of the pre-abort trap, where unbuffered stderr is the whole
  point — a per-message-unflushed file sink can lose the line precisely when it matters. For an
  unrecoverable invariant call `lumice::FatalAbort(...)` instead of hand-rolling print-then-`abort()`.
  A third place sits outside the rule by construction rather than by name: `src/launcher/`, now
  holding only `win_engine_loader.{c,h}` (the Windows release shell's CPUID probe and delay-load
  wiring for its two internal engine DLLs; the Linux side needed no such file — the dynamic
  linker's own glibc-hwcaps mechanism picks between the two engine `.so` files with zero
  application code, so there is nothing here for it to own). It is compiled straight into the CLI
  and GUI shell targets, not into `lumice_obj`, and runs *before* the engine DLL is even loaded, so
  `ILOG_*` genuinely does not exist yet for it to bypass; its few error lines go to stderr because
  there is no other sink at that point in the process's life. The checker never sees it — its scan
  covers C++/CUDA/Metal suffixes, not `.c` — which is consistent with what the rule guards (the
  engine's unified sinks), not a loophole to reuse: a `.c` file that *does* link the engine would
  still be wrong. Enforced by the `no-bare-print` rule
  in `scripts/check_policies.py`, which also scans `.cu` / `.metal` so a GPU backend cannot reopen
  the side channel. `test/` is deliberately out of scope: test binaries
  are their own harness with no app logger to bypass, and some of their output is a parsed contract
  (the regen driver reads gui_test's `PSNR=` line). Device-side `printf` in a CUDA kernel stays a
  legitimate debugging tool — the gate does not stop you adding one while working, only from landing it.
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
  `label_viewport_clamp.hpp`, `annotation_line_width.hpp`. The gate exists so the GUI cannot reach into the engine's
  data model and pin its layout by including it; a geometry function with no state does
  not offer that reach, and forcing one through the C API would only mean the rule gets
  written twice, once per side, which is the divergence the boundary is there to prevent.
  So: a rule BOTH renderers must obey, expressible without a core or config type, belongs
  in `src/util/`. Anything that needs to see a `RenderConfig`, a `SimData` or a crystal
  does not, and still goes through the C API.
- Environment variables: before adding any new `std::getenv`, apply the decision gate in
  `doc/env-var-policy.md`. User-facing behavior switches must go through CLI/config/API,
  not env vars (env causes silent per-machine drift). Env is fine for dev/experiment knobs
  (centralized + logged on startup) and test/build infra. All `LUMICE_*` env reads live in
  `src/util/env_knobs.cpp` and are enforced by `scripts/check_policies.py` (CI `policy` job +
  local pre-commit hook, installed via `./scripts/install-hooks.sh`).
  The **write** side has its own single owner, on the test side only: a test that has to set or
  clear a variable calls `lumice::test::SetEnvVar` / `UnsetEnvVar` from `test/support/env_var.hpp`,
  which is the one place in the tree holding the `_WIN32` branch (`_putenv_s` vs POSIX
  `setenv`/`unsetenv` — MSVC declares neither of the latter two). This is not enforced by a
  checker, deliberately: a bare `::setenv` is a *compile error* on MSVC, not a silent failure,
  which is the line the other four gates are drawn on. What it is not, is a compile error you
  will see — the two CUDA sources that first hit it sat behind `#if LUMICE_CUDA_ENABLED` and the
  Metal ones behind `#if defined(__APPLE__)`, so the breakage was invisible for three weeks until
  a Windows host built with both CUDA and `BUILD_TEST` on. `src/` never writes env vars at all.
- Working-note references: prose in a tracked file must not cite `scratchpad/` artifacts —
  task ids (`task-<name>`, `explore-<name>`, `scrum-<name>`, `chore-<name>`), per-task note
  filenames (`plan.md`, `progress.md`, `SUMMARY.md`), `code-review-0N`, `round-N`, or
  `scratchpad/` paths. That tree is git-ignored and archived per task, so the reference
  dangles for anyone reading later; state the mechanism itself instead. Permanent anchors
  are fine and should stay: PR numbers, commit hashes, `file:line`, code symbol names.
  This applies to any long-lived prose read apart from the context it was written in —
  production comments, test docstrings, CI config comments, and `doc/` alike.
  Enforced by `scripts/check_new_refs.py`, a **second, diff-scoped entry point** next to
  `check_policies.py` — not a check inside it. It reads only the lines a change *adds*
  (CI `new-refs` job on PRs vs merge-base; pre-commit hook on the staged diff), so the
  large body of pre-existing references never blocks a commit. Only natural-language
  regions are read: comments, Python docstrings, and Markdown — never string literals.
  There is no inline exemption, by design: if some prose genuinely needs such a reference,
  the rule is wrong and the rule should change. Files exempt from it are limited by one
  criterion — the file's *subject* is this working-notes system itself, i.e. it must name
  the system to do its job (this file, `CLAUDE.md`, and the root `.gitignore`). Citing the
  system is not grounds for exemption; that is the thing being checked.
  **The checker is the rule, not an approximation of it.** If `check_new_refs.py` passes, the
  change is compliant — full stop. Do not flag a reference in review that the checker accepts,
  and do not appeal to the rule's "spirit" beyond what it matches: the checker deliberately
  misses some citation forms (the trade-offs are argued at each pattern in the script), and
  those misses are accepted, not oversights awaiting discovery. Rationale: this rule guards
  against a harm measured to be *low* — 41% of tracked files carried such references for years
  with no observed cost — so an unbounded review-time judgement about it is worth less than it
  costs. Tightening is legitimate only by changing a pattern in the script (and accepting the
  false positives that buys), never by case-by-case escalation in review.

## Testing and Platform Notes

- CLI, core, and unit-test flows should remain cross-platform.
- Operating rules for the test tree live in **`test/AGENTS.md`**: the gui_test three-platform
  matrix (built on three platforms, run only on the `Ubuntu x86_64` llvmpipe leg), the three-way
  placement split for new `src/gui/` tests (`gui_test` / `composition_correctness_test` /
  `gui_unit_test`), the gui_test isolation discipline (user-config / window basis /
  ImGuiStorage), the shared libraries the pytest layers load, the E2E layout, and the full GUI
  reference-regeneration workflow. Which groups may run under a software rasterizer is a
  measured fact, not a preference — before widening the CI filter or dispositioning a
  visual-regression failure, follow the checklist in `doc/testing/testing-architecture.md` §4.6.
- Two more diff-scoped checkers beside `check_new_refs.py` (all in the pre-commit hook and the
  CI `new-refs` job; same "checker is the rule" discipline): `scripts/check_new_gui_tests.py`
  rejects a newly registered gui_test case that does not use its `ImGuiTestContext`, and
  `scripts/check_loop_fatal_asserts.py` rejects a fatal assert (or non-fatal report) followed by
  code that keeps driving the same test context in a repeatedly-executing scope of `test/`.
  Full rule statements: the scripts' docstrings and testing-architecture.md §4.9.
- **Test-duration registry — a runtime gate, not a fifth diff-scoped checker.**
  `test/duration_registry.json` v2 is the single authority for two quantities: per-test time
  (over 60 s in a wired job ⇒ registered `id`, `job`, `ci_seconds` and a real reason; >2× its
  registered time, a stale id, malformed data or `TODO` is red) and phase budgets
  (missing/unknown/duplicate phase reports and an over-limit sum are red). The runtime input
  does not exist at pre-commit time, so the gate is not part of `check_policies.py`. There is no
  per-test/env/inline exemption. The sole exception is the maintainer-authorized, one-run
  cold-build protocol for phase budgets containing `build`; it never excuses test runtime or correctness,
  and a PR whole-run/job
  rerun cannot consume it again. Full registry rules, the 60 s / 2× calibration and phase
  boundaries: `doc/testing/testing-architecture.md` §7.7. The cold-build protocol's
  single authority is §7.8.
- E2E split essentials: `./scripts/test.sh <quick|full|pr>` is the recommended entry point for a
  local run (one call, a single per-layer summary ending in one `RESULT:` line). Bare `pytest
  -v` runs the fast subset directly — pinned by `pyproject.toml`'s `addopts` (`-m "not slow"`),
  so it is structurally true rather than a convention a caller has to remember, and it matches
  CI's fast leg; `pytest -v -m ''` is the escape hatch back to the full set, and any marker CI
  never uses must be passed explicitly. `@pytest.mark.slow` tests require the shared-lib build
  (`./scripts/build.sh -sj release`) — run them locally before opening a PR that touches the
  simulator core, query filter, or C API surface. That build produces **three** shared libraries
  and pytest loads the second; the plumbing is described in `test/AGENTS.md`.
- **Test-scope table** — which command fits a given situation:

  | Situation | Run |
  |---|---|
  | Verify the change you just made | `./scripts/test.sh quick` |
  | Right after a static rebuild, before moving on | `./scripts/test.sh quick` |
  | Broader regression sweep — unsure how far a change reaches | `./scripts/test.sh full` |
  | Before opening a PR — reproduce the CI e2e-slow shape locally | `./scripts/test.sh pr` |
  | Confirm one specific gate/test actually fails on a deliberately-broken state (not a general sweep) | run that one check directly against the broken state, then again after the fix: `ctest -R <label>`, `pytest <file>::<test> -m ''`, or `gui_test --filter <name>` |

  What each row **costs** in measured seconds, what each scope catches that the cheaper one is
  structurally unable to report, the budget rule naming who has to answer for the spend, and
  where each test runs beyond these commands (the PR layer, `main` only, the reference machines'
  daily schedule, release, a developer machine; there is no nightly) are in
  `doc/testing/testing-architecture.md` §7 — deliberately not duplicated here: two copies of a
  measurement drift apart. Moving a test out of the PR layer means giving it one of those places
  first.

- **Judgment discipline**: a command's exit code is the only thing that says pass/fail, and two
  common habits obscure it in different ways. A pipe (`cmd | tail`) leaves `$?` reporting
  `tail`'s exit code, not `cmd`'s, unless the shell has `set -o pipefail` — so a failing command
  piped through one still reads as success. A redirect (`cmd > log 2>&1`) does not touch `$?` at
  all, but it does move the PASS/FAIL text out of sight, so a finished run still needs a second
  call (`grep`/`cat` on the log) just to learn the outcome it already produced. Read `$?`
  straight off the foregrounded command, or use `./scripts/test.sh`, which already does this for
  every layer.
- Windows physical-desktop validation uses `scripts/win_remote_test.sh` together with `scripts/win_test_watcher.ps1`.
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

Valuable design/architecture docs live in `doc/` (tracked); most have a `_zh`/`.zh.md` Chinese
sibling. Each entry below is the trigger map — the doc itself is the sole content authority, this
index carries no summaries. Subsystem operating rules live next to the code: `test/AGENTS.md`
(test tree) and `src/raypath/AGENTS.md` (raypath module surface).

- **Overview / guides**: `README.md`, `architecture.md`, `developer-guide.md`, `gui-guide.md`, `configuration.md`
- **Core / rendering architecture**: `accumulator-consumer-architecture.md`, `raypath-rayseg-architecture.md`, `coordinate-convention.md`, `crystal-orientation-sampling.md`, `ev-pipeline-architecture.md`, `adaptive-brightness.md`, `filter-architecture.md`
  - `ev-pipeline-architecture.md` §2.4–2.8/§7 + `adaptive-brightness.md` §2–3 — the two exposure anchors (`ev_mode`: relative default / absolute), the §2.8 dual-side common-anchor law, both migration laws, photometry rules. Read before touching exposure normalization / `ev_mode` / `ExposureScale()` / `emitted_energy` / whether metering sees `visible` / GUI preview brightness / `anchor_l99_sky` or `axis_solid_angle` consumers.
  - `raypath-symmetry.md` — filter/colour-ref P/B/D is label equivalence, the analysis list's is physical equivalence (`SymmetrySemantics`). Read before filter symmetry / analysis-list grouping / missing-face warnings / Exclude generation.
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
- **print mode**: `print-mode-subtractive-ink.md` — the single design authority for the print display mode (subtractive ink). Read before print-mode field chain / operator / colour-channel exclusivity / headroom warnings.
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
**retrieved**, not just stored (信息价值 = 内在价值 × 被检索到的概率). The recurring
failure mode is starting each session like a newcomer and re-deriving decisions the
owner already settled. Avoid it:
(The other half of this discipline — every change to `src/` or `test/` has a task directory
before the first edit, and is made in a linked worktree, not the main one — is a criterion, not
a habit, and lives under "Collaboration Constraints" above.)

- **Retrieve before re-deriving.** Before starting work on a continuing/recurring topic
  (GPU/Metal perf, GUI perf, parity, batch/throughput, architecture decisions), FIRST:
  1. `grep` `scratchpad/backlog.md` for the topic — it holds owner concerns, start
     conditions, and dependencies (e.g. concern #2 commit↔batch decoupling).
  2. Read the relevant prior `scratchpad/explore-*/{SUMMARY,insights}.md` and
     `scratchpad/scrum-*/SUMMARY.md` — they hold verified conclusions and rejected
     directions. Check the `doc/` index above.
  3. **Explicitly continue prior threads** ("this refines concern #X") rather than
     starting fresh. If new evidence conflicts with a prior conclusion, connect them.
- **Don't re-measure what's already de-risked.** Before running ANY benchmark or
  experiment on a recurring topic, `grep` the relevant prior `experiments.md` for that
  exact measurement — if it's there, harvest the number, do NOT re-run it. Reading a
  SUMMARY is not enough: it gives conclusions, but the failure mode is re-running the
  experiment, so check `experiments.md` at row level. A *design* explore's job is to
  de-risk the unresolved frontier (the blueprint's open §-items), not to re-confirm the
  premise the blueprint already rests on. Self-check: if your "finding" restates a
  sentence already in some prior insights/experiments, you are re-deriving — stop.
- **The scratchpad system is the source of truth for in-flight reasoning.**
  `scratchpad/tasks.md` (the task ledger), `scratchpad/backlog.md` (deferred work +
  owner concerns), `scratchpad/explore-*/` & `scratchpad/scrum-*/` (per-effort
  hypotheses/experiments/insights/SUMMARY), `scratchpad/learnings/` (extracted lessons).
  These are git-ignored working memory — do not treat their absence from git as absence
  of knowledge.
- **Promote durable design docs out of `scratchpad/` into `doc/`.** scratchpad is
  git-ignored, so design docs left there are undiscoverable and get lost (this is how
  the `seam-design.md` blueprint sat unbuilt). When an explore/scrum produces a durable
  design or decision record, copy it to `doc/` and add it to the index above.
- **Think at the architecture level before decomposing into tasks.** The task/scrum
  machinery rewards fast decomposition and immediate action; resist acting on the first
  promising small direction before the architecture-level question is reasoned through.
- **A completeness claim carries the same burden of proof as adding code.** "Covers all
  20 panels with zero omissions", "orthogonal", "closed", "every proposition enumerated"
  read as achievements and pass review unchallenged, while their opposites ("this class is
  not worth covering", "50 cases is enough here") have to be argued for. That asymmetry is
  the default state, not a choice anyone makes, so completeness wins every conflict without
  a single person advocating for it — including conflicts against the task's own declared
  budget. Measured instance, PR #261: the pre-committed target was −30% de-commented test
  lines against a baseline of 21,336 pinned by two independent counters
  (`scripts/count_gui_test_lines.py` and `cloc --by-file`, 52/52 files zero diff). It
  landed at −8.7%, and the coverage backfill demanded by "20 panels, zero omissions"
  accounts for 1,000–1,500 lines, roughly a third of the miss — the work shape was spending
  against the metric the same task had committed to. Escape-defect density over those same
  files had already been measured before the partition was drawn and spans 8× (47.2 vs 6.2
  defects per kloc); the equal-weight-per-panel split discarded that measurement.
  So: when a plan, a scope statement, or a review comment asserts coverage of an enumerated
  surface, it must say why each member is worth covering. Where a per-member value measure
  exists or is cheap to obtain (escape-defect density, call-site count, blast radius, user
  reachability), an equal-weight partition needs a stated reason — "it is the whole set" is
  not one. This puts the burden of proof on the side that adds and the side that persists,
  which is where it belongs, and which completeness normally escapes. It is **not** a
  mandate to cut: whether a leaner suite would have let more defects escape is a
  counterfactual and untestable. What is required is the justification, not the reduction.
