#!/usr/bin/env python3
"""Run the precise dual-renderer throughput gates on the reference machines, unattended.

Why this exists. The 0.85 dual-vs-single throughput gate
(``test/e2e/_multi_renderer_throughput.py``) was calibrated on the reference
machines' noise (Metal CoV 6-10%, CUDA 10-17%). A GitHub-hosted runner is slower
and noisier than that, so on CI the same number reads red with nothing wrong.
The precise gate therefore runs here, on a schedule (``launchd``, installed by
``scripts/install_throughput_gate.sh``), and CI keeps only a loose disaster
floor. Where it runs, how often, where results land and how to install it:
``doc/testing/performance-testing.md``, "Precise throughput gate (local schedule)".

What one run does, per leg, in a fixed order (one attempt of the retry loop
described below):

    lock -> resolve the commit (origin/main tip) ->
      metal: idle pre-check (600 s window) -> build -> cool-down -> idle re-check -> pytest
      gui-real-timing: idle pre-check (600 s window) -> build gui_test (own tree) -> cool-down
             -> idle re-check -> gui_test (real-timing pool, junit export)
      cuda : probe remote -> remote idle pre-check (600 s window) -> sync + build (on remote disk)
             -> cool-down -> idle re-check -> pytest (on remote disk) -> collect
    -> append one record per leg to results.jsonl -> notify -> staleness check

Only the re-check after the build decides whether the gate is measured; the
pre-check exists so a busy machine is not made busier by a build nobody uses.

Each leg ends in exactly one of four states. ``pass`` and ``fail`` are real
measurements; ``skipped`` (busy, unreachable, locked) and ``error`` (build
failure, no test executed, lost remote run) are not, and are kept apart from
``fail`` so an infrastructure problem is never read as a throughput regression.
Only ``pass``/``fail`` count as "the gate really ran" for the staleness alarm.
The gui-real-timing leg adds a fifth state, ``inconclusive``: the pool ran but
the environment had no verdict (a gate timeout, a DriveUntil/watchdog
environment timeout, or the frozen-map ms-budget reading red on a personal
machine) — it is a real measurement of the environment, issues nothing of its
own, and still does not refresh the staleness clock.

A busy attempt is not the end: each leg runs inside a structured retry loop.
One attempt is one full leg execution (both idle windows belong to it); a
busy attempt — only an ``idle_verdict`` rejection: a functional red, a build
failure or an ``inconclusive`` pool is a measurement, not an environment
state — is followed by a backoff (``--idle-retry-backoff-seconds``, 1 h) and
a full re-run, at most ``--idle-retry-max-backoffs`` times (2), each leg
counting its own budget and never re-running a leg that measured. When the
budget runs out, the leg's single ``skipped`` record carries an ``idle_retry``
payload (attempt count, backoffs, per-attempt snapshots) and the staleness
alarm covers the blind night as before; a leg that backed off before
eventually measuring carries the same history on its final record. When the
retry controller itself ends the loop — budget exhausted, or the nightly
deadline (``--nightly-deadline``, 06:30 local, launchd runs only) reached —
the payload also names the cause in ``terminal`` (``"max_backoffs"`` /
``"deadline"``); a leg that returned normally carries no ``terminal``, its
status already says what happened. The deadline is a hard wall for launchd
runs: every wait face (idle windows, remote phase polls, backoff sleeps) ends
at the wall instead of running out its configured budget, no attempt starts
past it, and an attempt still in flight is voided — its long phases run under
the remaining time as their timeout and their work is actually stopped (local
phases run in their own process group and are killed whole; the cuda leg's
detached remote phases record their pid and are killed by group, a failed
stop logged, never swallowed), nothing they did is recorded, and the leg's
outcome is a ``skipped`` line, never a timeout ``error``. Each cuda attempt
gets its own remote results directory, so a retried attempt can never read a
previous attempt's done markers. A leg the wall
reached before it started falls a single ``skipped`` line with no
``idle_retry`` payload, and a nightly run that launched after the wall skips
its source prep and ends the same way for every leg: the wall binds launchd
runs whenever they started. Manual runs are unbounded.

The judgement itself is not re-implemented here: the script runs the two
existing pytest files and classifies their exit code + junit XML. The ratios it
records are copied out of the test's own printed summary line for humans.

The file is split into a pure-logic section (no network, no processes; unit
tested by ``test/unit-correctness/scripts/test_local_throughput_gate.py``) and an
IO section below it.
"""

from __future__ import annotations

import argparse
import datetime as dt
import fcntl
import json
import os
import platform
import re
import shlex
import shutil
import signal
import subprocess
import sys
import time
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Dict, List, Optional, Sequence, Tuple

# =============================================================================
# Constants
# =============================================================================

LEGS = ("metal", "gui-real-timing", "cuda")

# The gui-real-timing leg drives gui_test directly with the real-timing pool's
# filter. Byte-for-byte copy of the literal on scripts/build.sh's real-timing
# invocation (the one WITHOUT --fixed-dt); check_policies.py's
# gui-test-suite-args-sync keeps the two in step. The first term "perf_test" is
# a category match (the engine's filter matches name OR category), which is how
# maximum_frozen_map_input_to_present and the other perf_test cases join the pool.
GUI_REALTIMING_FILTER = (
    "perf_test,revert_repushes_server_display_state,zorder_priority_persists_across_rerun,"
    "gpu_color_class_overflow,run_after_analysis_renders_gpu,survey_overview_shows_dispersion_and_profile,"
    "verdict_anchor_hover_and_toggle_on_the_map,map_region_shows_the_class_map_and_takes_the_pick,"
    "point_inspector_shows_the_point_fiber_and_the_path,refined_payload_sample_follows_curve_globe_and_trace,"
    "analyze_full_chain_one_session,internal_reflection_trace_from_the_real_consumer,"
    "refused_class_names_the_refusal_and_keeps_the_witness,refused_class_blockades_nothing,"
    "narrow_column_clipped_rows_keep_their_entities,analyze_result_carries_across_scale_tiers,"
    "profile_rows_highlight_and_draw_on_the_map"
)

# The delay contract's three-ms-budget assertion lives inside one IM_CHECK
# expression (test/gui/functional/test_frozen_map_view.cpp, the
# maximum_frozen_map_input_to_present case); the engine logs a failing check as
# `[NNNN] Error <file>:<line> '<expression>'`, so this fragment — the documented
# contract value — identifies a *budget* red. A recalibrated budget in the test
# must update this constant the same way a renamed pool member must update
# GUI_REALTIMING_FILTER. The structural sibling (frames >= 120) is NOT listed:
# any failing check that is neither the budget expression nor the case's outer
# wrapper is a functional red, the fail-safe default.
GUI_BUDGET_ASSERT_FRAGMENT = "max_observe <= 8.0"
# The case wraps all of its assertions in one IM_CHECK, so that outer expression
# fires for every failure mode and identifies nothing by itself.
GUI_TIMING_WRAPPER_EXPR = "MaximumTimingWitness(ctx)"
# The budget exemption names its case: the fragment above only grants the
# environment verdict on the frozen-map delay contract, never pool-wide.
GUI_FROZEN_TIMING_CASE = "maximum_frozen_map_input_to_present"
# Wall-clock telemetry of an environment timeout, logged into the failing case's
# own evidence. Only the engine's actual aborts count — the watchdog kill reads
# "..., aborting." and the emergency exit line (imgui_te_engine.cpp UpdateWatchdog);
# the ">N seconds, may be excessive." warning (LogWarning) fires routinely on a
# slow case that then fails a real assertion, and must not demote it.
GUI_TIMEOUT_MARKERS = ("[drive-gate] TIMEOUT", "seconds, aborting.",
                       "[Watchdog] Emergency process exit")
# A signal death reaches this script as -N from subprocess.run (SIGILL=4) or as
# 128+N when a shell renders it. The known full-suite signature's boundary case
# (test/AGENTS.md) is structurally unreachable from the filtered pool, so any
# marker here is a new sample; the label below only records that fact.
SIGILL_EXITS = (-4, 132)
GUI_SIGILL_KNOWN_MARKER = "workspace_panorama_publishes_after_a_render"

# The metal leg builds with scripts/build.sh, which needs these on PATH. Resolved
# before anything expensive — even before the idle pre-check — because a missing
# tool is a deterministic configuration error, not a busy-machine signal.
METAL_BUILD_TOOLS = ("cmake", "ninja")

# The gate each leg runs: (pytest file, test function name). The threshold and
# the statistic live in test/e2e/_multi_renderer_throughput.py, not here.
GATE_TESTS: Dict[str, Tuple[str, str]] = {
    "metal": (
        "test/performance/test_metal_multi_renderer_throughput.py",
        "test_metal_dual_renderer_throughput",
    ),
    "cuda": (
        "test/performance/test_cuda_throughput.py",
        "test_cuda_dual_renderer_throughput",
    ),
}

# Processes that make a throughput sample untrustworthy. Matched against the
# EXACT executable name (comm basename) — a substring match on "cl" once hit an
# unrelated driver process on the Windows host.
BUSY_EXECUTABLES = frozenset({
    "Lumice", "LumiceGUI", "gui_test", "unit_correctness_test", "gui_unit_test",
    "composition_correctness_test", "lumice_bench",
    "ninja", "make", "cmake", "ctest",
    "cc1plus", "cc1", "clang", "clang++", "c++", "g++", "gcc", "ld", "lld",
    "nvcc", "cicc", "ptxas", "fatbinary", "nvlink",
})
# The drive runners and pytest run under a generic interpreter name, so they are
# recognised from the ARGV of a python process: a script whose basename is one of
# these, or `-m pytest`. Only python processes are read this way — a shell whose
# command TEXT merely mentions pytest (an agent's `zsh -c "..."`) once made the
# gate call itself busy.
BUSY_PY_SCRIPTS = frozenset({
    "task_drive.py", "chore.py", "scrum_drive.py", "explore_drive.py", "pytest", "py.test",
})
# The Windows host of the WSL remote, read through WSL interop.
HOST_BUSY_EXECUTABLES = frozenset({
    "lumice", "lumicegui", "gui_test", "cl", "link", "nvcc", "cicc", "ptxas",
    "ninja", "cmake", "msbuild", "nsys", "ncu",
})
HOST_BUSY_ARG_MARKERS = ("win_bench", "bench_throughput")

DEFAULT_LOAD_PER_CORE = 0.5       # 1-min loadavg / physical cores; desktop baseline 0.24-0.36
DEFAULT_HOST_CPU_PERCENT = 25.0   # Windows host instantaneous CPU %
DEFAULT_GPU_UTIL_PERCENT = 10.0   # nvidia-smi utilization.gpu %
DEFAULT_STALE_DAYS = 3.0
KEEP_RUN_DIRS = 30

ISSUE_TAG = "[throughput-gate]"
TEST_NAMESPACE_TAG = "[TEST]"
ISSUE_KINDS = ("fail", "error", "stale", "sigill")

_RATIO_RE = re.compile(r"dual/single_a=([0-9.]+) dual/single_b=([0-9.]+)")
_LEGACY_RE = re.compile(r"/legacy=([0-9.]+)x")
_GATE_RE = re.compile(r"gate >= ([0-9.]+)")
_MEDIAN_RE = re.compile(r"\] (dual|single_a|single_b): median=([0-9.]+)M rays/s")
_LEGACY_MEDIAN_RE = re.compile(r"legacy dual median=([0-9.]+)M rays/s")

# A pytest plugin written into the run directory when --threshold-override is
# given. It cannot live in the checkout under test: that commit predates this
# script. It rebinds the shared module's constant before the test calls it.
OVERRIDE_PLUGIN_NAME = "throughput_gate_override"
OVERRIDE_PLUGIN_SOURCE = '''\
"""Written by scripts/local_throughput_gate.py for a synthetic (test) run only."""


def pytest_addoption(parser):
    parser.addoption("--dual-gate-threshold", type=float, default=None)


def pytest_configure(config):
    value = config.getoption("--dual-gate-threshold")
    if value is not None:
        import test.e2e._multi_renderer_throughput as gate

        gate.T_DUAL_VS_SINGLE = value
'''

# =============================================================================
# Pure logic (unit tested; no network, no subprocess, no clock)
# =============================================================================


@dataclass(frozen=True)
class Proc:
    pid: int
    ppid: int
    exe: str    # executable basename
    args: str   # full command line


@dataclass
class Readings:
    """One idle reading of one machine (plus, for the WSL remote, its host)."""
    load1: float
    physical_cores: int
    busy: List[str] = field(default_factory=list)          # "<pid> <exe>: <args head>"
    host_cpu_percent: Optional[float] = None
    host_busy: List[str] = field(default_factory=list)
    gpu_util_percent: Optional[float] = None
    errors: List[str] = field(default_factory=list)

    def to_json(self) -> dict:
        return dict(self.__dict__)

    @staticmethod
    def from_json(d: dict) -> "Readings":
        return Readings(**d)


@dataclass(frozen=True)
class IdleThresholds:
    load_per_core: float = DEFAULT_LOAD_PER_CORE
    host_cpu_percent: float = DEFAULT_HOST_CPU_PERCENT
    gpu_util_percent: float = DEFAULT_GPU_UTIL_PERCENT


def own_tree(procs: Sequence[Proc], self_pid: int) -> set:
    """Ancestors and descendants of ``self_pid``: never a reason to call ourselves busy.

    Ancestors include whatever drove this run (a shell, launchd, an agent's drive
    runner); descendants are this run's own build and pytest.
    """
    by_pid = {p.pid: p for p in procs}
    mine = {self_pid}
    changed = True
    while changed:
        changed = False
        for p in procs:
            if p.ppid in mine and p.pid not in mine:
                mine.add(p.pid)
                changed = True
    # Ancestors only — never their other children: a sibling under the same
    # shell or drive runner is exactly the concurrent load this check exists for.
    excluded = set(mine)
    pid = self_pid
    while pid in by_pid and by_pid[pid].ppid > 0 and by_pid[pid].ppid not in excluded:
        pid = by_pid[pid].ppid
        excluded.add(pid)
    return excluded


def runs_python_workload(exe: str, args: str) -> bool:
    """A python process running pytest or a drive runner (see BUSY_PY_SCRIPTS)."""
    base = exe.lower()
    if base.endswith(".exe"):
        base = base[:-4]
    if base in ("pytest", "py.test"):
        return True
    if not base.startswith("python"):
        return False
    tokens = args.split()[1:]
    for i, t in enumerate(tokens):
        if t == "-m":
            return i + 1 < len(tokens) and tokens[i + 1] in ("pytest", "py.test")
        if t.startswith("-"):
            continue
        # The first positional argument is the script (`python /venv/bin/pytest` is
        # how a pytest entry point shows up); later ones are its own arguments.
        return os.path.basename(t.replace("\\", "/")) in BUSY_PY_SCRIPTS
    return False


def busy_processes(procs: Sequence[Proc], self_pid: int) -> List[str]:
    skip = own_tree(procs, self_pid)
    out = []
    for p in procs:
        if p.pid in skip:
            continue
        if p.exe in BUSY_EXECUTABLES or runs_python_workload(p.exe, p.args):
            out.append(f"{p.pid} {p.exe}: {p.args[:120]}")
    return out


def host_busy_processes(names_and_args: Sequence[Tuple[str, str]]) -> List[str]:
    out = []
    for name, args in names_and_args:
        base = name.lower()
        if base.endswith(".exe"):
            base = base[:-4]
        if (base in HOST_BUSY_EXECUTABLES or runs_python_workload(name, args or "")
                or any(m in (args or "") for m in HOST_BUSY_ARG_MARKERS)):
            out.append(f"{name}: {(args or '')[:120]}")
    return out


def idle_verdict(r: Readings, th: IdleThresholds) -> Tuple[bool, List[str]]:
    """(idle?, reasons it is not). A reading that could not be taken counts as busy."""
    reasons = []
    per_core = r.load1 / max(r.physical_cores, 1)
    if per_core >= th.load_per_core:
        reasons.append(f"load {r.load1:.2f}/{r.physical_cores} cores = {per_core:.3f} >= {th.load_per_core}")
    if r.busy:
        reasons.append(f"busy processes: {'; '.join(r.busy[:5])}")
    if r.host_cpu_percent is not None and r.host_cpu_percent >= th.host_cpu_percent:
        reasons.append(f"host CPU {r.host_cpu_percent:.0f}% >= {th.host_cpu_percent:.0f}%")
    if r.host_busy:
        reasons.append(f"host busy processes: {'; '.join(r.host_busy[:5])}")
    if r.gpu_util_percent is not None and r.gpu_util_percent >= th.gpu_util_percent:
        reasons.append(f"GPU util {r.gpu_util_percent:.0f}% >= {th.gpu_util_percent:.0f}%")
    if r.errors:
        reasons.append(f"reading errors: {'; '.join(r.errors)}")
    return (not reasons, reasons)


@dataclass(frozen=True)
class JunitCase:
    name: str
    outcome: str   # "passed" | "failure" | "error" | "skipped"
    message: str = ""


def parse_junit(xml_text: str) -> List[JunitCase]:
    root = ET.fromstring(xml_text)
    cases = []
    for tc in root.iter("testcase"):
        outcome, message = "passed", ""
        for tag in ("failure", "error", "skipped"):
            el = tc.find(tag)
            if el is not None:
                outcome, message = tag, (el.get("message") or "")
                break
        cases.append(JunitCase(tc.get("name", ""), outcome, message))
    return cases


def classify_pytest(exit_code: Optional[int], junit_xml: Optional[str], test_name: str) -> Tuple[str, str]:
    """Map a pytest run of one gate test to (status, reason); status in pass/fail/error.

    pass  = exit 0 AND the target test ran and passed (exit 0 with the test
            skipped or not collected is the silent-green shape: missing
            binary, CUDA env not set, marker not selected).
    fail  = exit 1 AND the target test has a junit <failure> — an assertion in
            the test body, i.e. the gate (ratio, legacy sanity floor, on-device
            routing guard) said no.
    error = everything else: interrupted, collection error, no junit, a raised
            exception (<error>, e.g. a timeout or a missing binary), exit 2/3/4/5.
    """
    if exit_code is None:
        return "error", "pytest-did-not-finish"
    if junit_xml is None:
        return "error", f"no-junit (pytest exit {exit_code})"
    try:
        cases = [c for c in parse_junit(junit_xml) if c.name == test_name]
    except ET.ParseError as e:
        return "error", f"junit-unparseable: {e}"
    if not cases:
        return "error", f"no-test-executed (pytest exit {exit_code}, {test_name} not collected)"
    case = cases[0]
    if exit_code == 0:
        if case.outcome == "passed":
            return "pass", ""
        return "error", f"no-test-executed ({test_name} {case.outcome}: {case.message[:200]})"
    if exit_code == 1 and case.outcome == "failure":
        return "fail", case.message[:300]
    return "error", f"pytest-exit-{exit_code} ({case.outcome}: {case.message[:200]})"


# ---- gui real-timing pool classifier ----------------------------------------
# Evidence formats pinned against real gui_test output (junit exporter
# imgui_te_exporters.cpp writes the on-error log into the <failure> text
# element; ConfigLogToTTY echoes the same lines to stderr): case start marker
# `[NNNN] Test: '<category>' '<name>'..`, check lines
# `[NNNN] OK|Error <file>:<line> '<expression>'`, the frozen-map value line
# `[NNNN] [frozen-map-timing] frames=N p95=X ms max=Y ms observation-upper=Z ms`.


@dataclass(frozen=True)
class GuiCase:
    name: str
    status: str        # junit status attribute: "Success" | "Error" | "Unknown"
    failure_text: str  # <failure> element text (the case's on-error log), "" when none


def parse_gui_junit(xml_text: str) -> List[GuiCase]:
    """Per-case name/status/failure-text. The engine's exporter lists every
    registered test; unexecuted ones carry status="Unknown" + <skipped/>."""
    root = ET.fromstring(xml_text)
    cases = []
    for tc in root.iter("testcase"):
        failure = tc.find("failure")
        cases.append(GuiCase(tc.get("name", ""), tc.get("status", ""),
                             (failure.text or "") if failure is not None else ""))
    return cases


_FROZEN_TIMING_RE = re.compile(
    r"\[frozen-map-timing\] frames=(\d+) p95=([0-9.eE+-]+) ms max=([0-9.eE+-]+) ms "
    r"observation-upper=([0-9.eE+-]+) ms")
_CASE_MARKER_RE = re.compile(r"Test: '([^']*)' '([^']*)'")
_CHECK_ERROR_RE = re.compile(r"\[\d+\] Error \S+ '([^']*)'")


def extract_frozen_timing(log_text: str) -> Optional[dict]:
    """The maximum_frozen_map_input_to_present value line, whenever the run
    produced one — pass or red — as trend fields independent of the verdict.
    The last match wins: a failed case is re-run once and the final attempt's
    numbers are the ones that decided."""
    m = None
    for m in _FROZEN_TIMING_RE.finditer(log_text):
        pass
    if m is None:
        return None
    return {"timing_frames": int(m.group(1)), "timing_p95_ms": float(m.group(2)),
            "timing_max_ms": float(m.group(3)), "timing_observation_max_ms": float(m.group(4))}


def _gui_case_span(log_text: str, case: str) -> str:
    """Fallback evidence for a failed case when its junit failure text is
    missing: the text from the case marker's LAST occurrence to the next one
    (a failed case is re-run once; the last attempt decides)."""
    starts = [m.end() for m in _CASE_MARKER_RE.finditer(log_text)
              if m.group(2) == case]
    if not starts:
        return ""
    begin = starts[-1]
    nxt = [m.start() for m in _CASE_MARKER_RE.finditer(log_text)
           if m.start() > begin]
    return log_text[begin:nxt[0] if nxt else len(log_text)]


def _attribute_gui_case(case: GuiCase, log_text: str) -> Tuple[str, str]:
    """One failed case -> ("timeout" | "budget" | "functional", detail).

    Primary evidence is the case's junit failure text (the engine exports the
    on-error log per case, so retries and sibling cases cannot bleed in); the
    TTY span is the fallback. Timeout markers decide first (a DriveUntil/watchdog
    abort is the failure), then the failing-check expressions: the budget
    expression alone (besides the case's outer wrapper, which fires for every
    mode) is a budget red; anything else — including nothing recognizable, e.g.
    the wrapper-only shape of an adoption breach — is a functional red, the
    fail-safe default.
    """
    evidence = case.failure_text or _gui_case_span(log_text, case.name)
    for marker in GUI_TIMEOUT_MARKERS:
        if marker in evidence:
            return "timeout", marker
    errors = [m.group(1) for m in _CHECK_ERROR_RE.finditer(evidence)]
    others = [e for e in errors
              if GUI_BUDGET_ASSERT_FRAGMENT not in e and e != GUI_TIMING_WRAPPER_EXPR]
    if others:
        return "functional", others[0][:200]
    if (case.name == GUI_FROZEN_TIMING_CASE
            and any(GUI_BUDGET_ASSERT_FRAGMENT in e for e in errors)):
        return "budget", GUI_BUDGET_ASSERT_FRAGMENT
    return "functional", (case.failure_text.strip().splitlines() or ["<no evidence>"])[-1][:200]


@dataclass(frozen=True)
class GuiPoolVerdict:
    status: str                    # pass / fail / error / inconclusive
    reason: str
    sigill: Optional[dict] = None  # event payload when the run died by signal
    timing: Optional[dict] = None  # extract_frozen_timing's trend fields
    failing_case: Optional[str] = None


def classify_gui_pool(exit_code: Optional[int], junit_xml: Optional[str], log_text: str,
                      sigill_load: Optional[dict] = None) -> GuiPoolVerdict:
    """Map one gui_test run of the real-timing pool to a verdict.

    pass         exit 0, >=1 case executed, no case in error.
    fail         a functional red (any failing check that is not attributable to
                 the environment), or a signal death (SIGILL sample event: the
                 per-sample record the first-action protocol asks for — exit
                 code, last case marker, log tail, and the crash-moment load
                 snapshot the IO layer passes in via sigill_load).
    inconclusive the run measured but the environment had no verdict: a gate
                 timeout (exit 124), or every failure attributed to a
                 DriveUntil/watchdog timeout or the frozen-map ms-budget.
    error        infrastructure: no/unparseable junit, nothing executed, exit
                 code contradicting the junit (the silent-green shapes), or an
                 unexpected exit code.

    SIGILL is checked before everything: a signal death leaves no junit, and it
    is a sample to record, not a verdict to classify.
    """
    timing = extract_frozen_timing(log_text)
    if exit_code is not None and exit_code in SIGILL_EXITS:
        markers = list(_CASE_MARKER_RE.finditer(log_text))
        event = {
            "exit_code": exit_code,
            "last_case_marker": markers[-1].group(2) if markers else "<none>",
            "log_tail": log_text[-2000:],
            "known_signature_boundary":
                bool(markers) and markers[-1].group(2) == GUI_SIGILL_KNOWN_MARKER,
        }
        if sigill_load is not None:
            event.update(sigill_load)
        return GuiPoolVerdict("fail", f"sigill: exit {exit_code} (sample event)", sigill=event, timing=timing)
    if exit_code == 124:
        return GuiPoolVerdict("inconclusive", "gate-timeout (budget exhausted)", timing=timing)
    if exit_code is None:
        return GuiPoolVerdict("error", "gui-test-did-not-finish", timing=timing)
    if junit_xml is None:
        return GuiPoolVerdict("error", f"no-junit (gui_test exit {exit_code})", timing=timing)
    try:
        cases = parse_gui_junit(junit_xml)
    except ET.ParseError as e:
        return GuiPoolVerdict("error", f"junit-unparseable: {e}", timing=timing)
    executed = [c for c in cases if c.status != "Unknown"]
    failed = [c for c in executed if c.status == "Error"]
    if not executed:
        return GuiPoolVerdict("error", f"no-test-executed (gui_test exit {exit_code})", timing=timing)
    if exit_code == 0:
        if failed:
            return GuiPoolVerdict("error", "gui-test junit mismatch (exit 0, junit reports errors)",
                                  timing=timing)
        return GuiPoolVerdict("pass", "", timing=timing)
    if not failed:
        return GuiPoolVerdict("error", f"gui-test junit mismatch (exit {exit_code}, junit all-pass)",
                              timing=timing)
    if exit_code != 1:
        return GuiPoolVerdict("error", f"gui-test-exit-{exit_code} "
                              f"({failed[0].name}: {failed[0].failure_text[:200]})", timing=timing)
    attributed = [(c,) + _attribute_gui_case(c, log_text) for c in failed]
    functional = [a for a in attributed if a[1] == "functional"]
    if functional:
        # A real defect signal is not downgraded by unrelated environment reds
        # in the same run: they are recorded into the reason, the status stays
        # fail.
        env = [a for a in attributed if a[1] != "functional"]
        env_note = ("; also " + "; ".join(f"{a[1]}: {a[0].name}" for a in env)) if env else ""
        return GuiPoolVerdict("fail", "functional red: " + ", ".join(a[0].name for a in functional) + env_note,
                              timing=timing, failing_case=functional[0][0].name)
    timeouts = [a for a in attributed if a[1] == "timeout"]
    budgets = [a for a in attributed if a[1] == "budget"]
    parts = ([f"timeout: {a[0].name} ({a[2]})" for a in timeouts]
             + [f"budget red: {a[0].name}" for a in budgets])
    return GuiPoolVerdict("inconclusive", "; ".join(parts), timing=timing)


def extract_ratios(log_text: str) -> dict:
    out = {}
    m = None
    for m in _RATIO_RE.finditer(log_text):
        pass
    if m is not None:
        out["ratio_a"] = float(m.group(1))
        out["ratio_b"] = float(m.group(2))
    m = None
    for m in _LEGACY_RE.finditer(log_text):
        pass
    if m is not None:
        out["vs_legacy"] = float(m.group(1))
    m = None
    for m in _GATE_RE.finditer(log_text):
        pass
    if m is not None:
        out["threshold"] = float(m.group(1))
    # Absolute rates, recorded so a run under different conditions (throttled
    # QoS, a thermal state, a slower build) is visible even when the ratio passes.
    for m in _MEDIAN_RE.finditer(log_text):
        out[f"median_mrps_{m.group(1)}"] = float(m.group(2))
    for m in _LEGACY_MEDIAN_RE.finditer(log_text):
        out["median_mrps_legacy"] = float(m.group(1))
    return out


def issue_title(leg: str, kind: str, namespace: str) -> str:
    prefix = TEST_NAMESPACE_TAG if namespace == "test" else ""
    return f"{prefix}{ISSUE_TAG} {leg} {kind}"


def is_stale(last_real_run: dt.datetime, now: dt.datetime, stale_days: float) -> bool:
    return (now - last_real_run) >= dt.timedelta(days=stale_days)


@dataclass(frozen=True)
class Action:
    op: str      # "create" | "comment" | "close"
    kind: str    # "fail" | "error" | "stale"
    body: str


def decide_notifications(status: str, synthetic: bool, stale: bool,
                         open_kinds: set, body: str, sigill: bool = False) -> List[Action]:
    """The single decision table: (this leg's outcome x open issues) -> actions.

    status      | open issue of that kind    | action
    ------------+----------------------------+-----------------------------------
    fail+sigill | sigill open                | comment (sample record)
    fail+sigill | sigill not open            | create sigill
    fail        | fail open                  | comment (numbers only)
    fail        | fail not open              | create fail
    error       | error open                 | none
    error       | error not open             | create error
    pass        | (non-synthetic)            | close fail / error / stale / sigill if open
    pass        | (synthetic)                | none — a doctored threshold proves nothing
    fail        | (non-synthetic)            | also close stale if open (it really ran)
    skipped     | -                          | none
    inconclusive| -                          | none of its own — but never returns
                |                            | early, so the stale row below still fires
    stale       | stale or error open        | none (an open error already explains it)
    stale       | neither open               | create stale

    ``inconclusive`` (the gui-real-timing leg's ran-but-environment-had-no-verdict
    state) issues nothing of its own: an environment timeout or a machine-load
    budget red is not a defect signal. It still does not refresh the staleness
    clock, so a pool that hangs every night ages into the stale row below —
    the freshness alarm is the separate channel that keeps covering it.
    """
    actions: List[Action] = []
    if status == "fail":
        kind = "sigill" if sigill else "fail"
        if kind in open_kinds:
            actions.append(Action("comment", kind, body))
        else:
            actions.append(Action("create", kind, body))
        if not synthetic and "stale" in open_kinds:
            actions.append(Action("close", "stale", "The gate measured again (fail)."))
    elif status == "error":
        if "error" not in open_kinds:
            actions.append(Action("create", "error", body))
    elif status == "pass" and not synthetic:
        for kind in ISSUE_KINDS:
            if kind in open_kinds:
                actions.append(Action("close", kind, "Recovered: the gate passed.\n\n" + body))
    if stale and "stale" not in open_kinds and "error" not in open_kinds:
        actions.append(Action("create", "stale", body))
    return actions


def refreshes_last_real_run(status: str, synthetic: bool) -> bool:
    return status in ("pass", "fail") and not synthetic


def missing_build_tools_reason(found: Dict[str, Optional[str]]) -> Optional[str]:
    """The metal leg's tool pre-check verdict: None, or the error reason.

    ``found`` maps each required tool to its resolved path (or None). The reason
    names the missing tools in METAL_BUILD_TOOLS order and nothing else — the
    public issue shows only the category prefix; the tool names stay in the
    local record.
    """
    missing = [t for t in METAL_BUILD_TOOLS if not found.get(t)]
    if missing:
        return "build tools missing from PATH: " + " ".join(missing)
    return None


def busy_skip(phase: str, reasons: List[str]) -> dict:
    """The one constructor for every leg's busy-skip return point (both idle
    windows of all three legs). It carries the explicit ``env_rejected`` marker
    the leg-level retry loop keys on — matching the reason's wording would rot
    silently with any rewording — while the reason text itself stays
    byte-identical to the shape the public issue categories already parse.
    ``phase`` is the stage name the legacy reasons already carry.
    """
    return {"status": "skipped",
            "reason": f"busy ({phase}): " + " | ".join(reasons),
            "env_rejected": True}


def is_env_busy_skip(rec: dict) -> bool:
    """The retry loop's only trigger: the leg ended busy because the environment
    said so. Reads the constructor's marker, never the reason text; cuda's
    ``unreachable`` and the run-level ``locked`` skips carry no marker and are
    terminal states, not environment rejections."""
    return bool(rec.get("env_rejected"))


# =============================================================================
# IO: readings
# =============================================================================


GIT_TIMEOUT_S = 600
GIT_CLONE_TIMEOUT_S = 3600


def _run(cmd: Sequence[str], **kw) -> subprocess.CompletedProcess:
    # The capture boundary decodes with replacement: one stray non-UTF-8 byte in a
    # child's output (another process's args in a `ps` row) degrades to U+FFFD
    # instead of raising UnicodeDecodeError and losing the whole unattended run.
    return subprocess.run(list(cmd), capture_output=True, text=True, errors="replace", **kw)


def _physical_cores() -> int:
    if platform.system() == "Darwin":
        try:
            return int(_run(["sysctl", "-n", "hw.physicalcpu"]).stdout.strip())
        except (ValueError, OSError):
            return os.cpu_count() or 1
    pairs = set()
    phys = core = None
    with open("/proc/cpuinfo") as f:
        for line in f:
            if line.startswith("physical id"):
                phys = line.split(":")[1].strip()
            elif line.startswith("core id"):
                core = line.split(":")[1].strip()
            elif not line.strip():
                if core is not None:
                    pairs.add((phys, core))
                phys = core = None
    return len(pairs) or (os.cpu_count() or 1)


def _process_table() -> List[Proc]:
    out = _run(["ps", "-axo", "pid=,ppid=,comm=,args="]).stdout
    procs = []
    for line in out.splitlines():
        parts = line.split(None, 3)
        if len(parts) < 3:
            continue
        pid, ppid, comm = int(parts[0]), int(parts[1]), parts[2]
        args = parts[3] if len(parts) > 3 else comm
        procs.append(Proc(pid, ppid, os.path.basename(comm), args))
    return procs


_HOST_PS = (
    "$c=(Get-CimInstance Win32_Processor | Measure-Object -Property LoadPercentage -Average).Average;"
    "Write-Output ('CPU ' + $c);"
    "Get-CimInstance Win32_Process | ForEach-Object { Write-Output ('P ' + $_.Name + \"`t\" + $_.CommandLine) }"
)


def collect_readings(with_wsl_host: bool) -> Readings:
    """Read this machine (and, on WSL, its Windows host and GPU). Runs locally or via ssh."""
    r = Readings(load1=os.getloadavg()[0], physical_cores=_physical_cores())
    r.busy = busy_processes(_process_table(), os.getpid())
    if with_wsl_host:
        ps = "/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe"
        try:
            p = _run([ps, "-NoProfile", "-NonInteractive", "-Command", _HOST_PS], timeout=60)
            entries = []
            for line in p.stdout.splitlines():
                line = line.rstrip("\r")
                if line.startswith("CPU "):
                    r.host_cpu_percent = float(line[4:] or "nan")
                elif line.startswith("P "):
                    name, _, args = line[2:].partition("\t")
                    entries.append((name, args))
            if r.host_cpu_percent is None:
                r.errors.append(f"host CPU unreadable (powershell exit {p.returncode})")
            r.host_busy = host_busy_processes(entries)
        except (OSError, subprocess.TimeoutExpired, ValueError) as e:
            r.errors.append(f"host interop failed: {e}")
        try:
            p = _run(["/usr/lib/wsl/lib/nvidia-smi", "--query-gpu=utilization.gpu",
                      "--format=csv,noheader,nounits"], timeout=30)
            r.gpu_util_percent = max(float(x) for x in p.stdout.split())
        except (OSError, subprocess.TimeoutExpired, ValueError) as e:
            r.errors.append(f"nvidia-smi failed: {e}")
    return r


# =============================================================================
# IO: state, records, lock
# =============================================================================


def utcnow() -> dt.datetime:
    return dt.datetime.now(dt.timezone.utc).replace(microsecond=0)


def iso(t: dt.datetime) -> str:
    return t.isoformat()


def load_state(state_dir: Path, now: dt.datetime) -> dict:
    p = state_dir / "state.json"
    if p.is_file():
        try:
            st = json.loads(p.read_text())
            if isinstance(st, dict) and "created" in st and "legs" in st:
                st.setdefault("pending_notifications", [])
                return st
        except (ValueError, OSError):
            pass
        # Unreadable state must not kill an unattended run: keep it for inspection, start over.
        p.replace(state_dir / f"state.json.corrupt-{now.strftime('%Y%m%dT%H%M%SZ')}")
    # First run: start the staleness clock now, not at the epoch.
    return {"created": iso(now), "legs": {}, "pending_notifications": []}


def save_state(state_dir: Path, state: dict) -> None:
    tmp = state_dir / "state.json.tmp"
    tmp.write_text(json.dumps(state, indent=2, sort_keys=True))
    tmp.replace(state_dir / "state.json")


def last_real_run(state: dict, leg: str) -> dt.datetime:
    v = state["legs"].get(leg, {}).get("last_real_run") or state["created"]
    return dt.datetime.fromisoformat(v)


def append_record(state_dir: Path, record: dict) -> None:
    with open(state_dir / "results.jsonl", "a") as f:
        f.write(json.dumps(record, sort_keys=True) + "\n")


def prune_run_dirs(runs_dir: Path, keep: int) -> None:
    dirs = sorted(d for d in runs_dir.iterdir() if d.is_dir())
    for d in dirs[:-keep] if len(dirs) > keep else []:
        shutil.rmtree(d, ignore_errors=True)


class Lock:
    def __init__(self, path: Path):
        self.path = path
        self.fd = None

    def acquire(self) -> bool:
        self.fd = open(self.path, "a+")
        try:
            fcntl.flock(self.fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            self.fd.close()
            self.fd = None
            return False
        self.fd.seek(0)
        self.fd.truncate()
        self.fd.write(str(os.getpid()))
        self.fd.flush()
        return True


# =============================================================================
# IO: runner
# =============================================================================


class Log:
    def __init__(self, path: Path, dry_run: bool):
        self.path = path
        self.dry_run = dry_run

    def __call__(self, msg: str) -> None:
        line = f"[{dt.datetime.now().strftime('%H:%M:%S')}] {msg}"
        print(line, flush=True)
        with open(self.path, "a") as f:
            f.write(line + "\n")


def clean_env(extra: Dict[str, str]) -> Dict[str, str]:
    """The caller's env minus every LUMICE_* knob, plus exactly what the gate sets."""
    env = {k: v for k, v in os.environ.items() if not k.startswith("LUMICE_")}
    env.update(extra)
    return env


@dataclass
class Ctx:
    args: argparse.Namespace
    run_id: str
    run_dir: Path
    log: Log
    thresholds: IdleThresholds
    synthetic: bool
    # Retry/deadline seams. ``nightly_deadline`` is the run's wall as local
    # wall-clock time (None when it does not apply — manual runs, decided once
    # per run in main); ``clock``/``sleep`` are injectable so the retry and
    # deadline paths are unit-tested without real sleeps.
    nightly_deadline: Optional[dt.datetime] = None
    clock: Callable[[], dt.datetime] = field(default_factory=lambda: dt.datetime.now)
    sleep: Callable[[float], None] = field(default_factory=lambda: time.sleep)
    # Which retry attempt is executing (1-based). The cuda leg derives its
    # remote results directory from it: one directory per attempt, so a
    # retried attempt can never read the previous attempt's done markers.
    attempt_no: int = 1


def _kill_work_tree(p: subprocess.Popen) -> None:
    """Stop a timed-out phase's whole work tree. A phase runs in its own POSIX
    process group: build.sh spawns ninja and compilers, pytest spawns render
    processes — killing only the entry process would leave the work running
    past the wall (or past the budget) while the record already says it ended."""
    if os.name == "posix":
        try:
            os.killpg(os.getpgid(p.pid), signal.SIGKILL)
        except (ProcessLookupError, PermissionError, OSError):
            pass
    else:
        p.kill()
    try:
        p.wait(timeout=10)
    except (subprocess.TimeoutExpired, OSError):
        pass


def run_logged(ctx: Ctx, cmd: Sequence[str], log_path: Path, cwd: Optional[Path] = None,
               env: Optional[Dict[str, str]] = None, timeout: Optional[float] = None,
               stdin_text: Optional[str] = None) -> int:
    ctx.log(f"$ {' '.join(shlex.quote(c) for c in cmd)}  (> {log_path.name})")
    if ctx.args.dry_run:
        return 0
    with open(log_path, "a") as out:
        # POSIX: the phase gets its own process group, so a timeout stops the
        # whole tree it spawned (see _kill_work_tree).
        kw = {"start_new_session": True} if os.name == "posix" else {}
        p = subprocess.Popen(list(cmd), cwd=cwd, env=env, stdout=out, stderr=subprocess.STDOUT,
                             stdin=subprocess.PIPE if stdin_text is not None else None,
                             text=True, **kw)
        if stdin_text is not None:
            p.stdin.write(stdin_text)
            p.stdin.close()
        try:
            return p.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            _kill_work_tree(p)
            out.write(f"\n[gate] TIMEOUT after {timeout}s\n")
            return 124


def wait_idle(ctx: Ctx, read: Callable[[], Readings], label: str) -> Tuple[bool, Readings, List[str]]:
    """Poll until idle or the cool-down budget runs out. Returns the LAST reading.

    The window budget itself is unchanged; when the run has a nightly deadline,
    the wait also ends at the wall — the poll sleep never crosses it, and
    waking at the wall returns the reading already in hand instead of sampling
    again. A wait interrupted by the wall is not the attempt's own busy
    verdict; the leg's stage guard voids the attempt."""
    deadline = time.monotonic() + ctx.args.cooldown_timeout
    while True:
        r = read()
        ok, reasons = idle_verdict(r, ctx.thresholds)
        if ok or ctx.args.dry_run or time.monotonic() >= deadline:
            ctx.log(f"{label}: {'idle' if ok else 'busy: ' + ' | '.join(reasons)}")
            return ok, r, reasons
        if deadline_reached(ctx):
            ctx.log(f"{label}: nightly deadline reached; wait ends")
            return ok, r, reasons
        sleep_before(ctx, ctx.args.cooldown_poll, ctx.nightly_deadline)
        if deadline_reached(ctx):
            ctx.log(f"{label}: nightly deadline reached; wait ends")
            return ok, r, reasons


# ---- nightly deadline wall (M0.2 ruling: launchd runs end at local 06:30) ----


def sleep_before(ctx: Ctx, seconds: float, wall: Optional[dt.datetime]) -> None:
    """One wait sleep, never across ``wall`` (None = no wall applies to this
    wait: sleep as configured). The wait must be able to end at the wall, not
    wake a configured interval past it."""
    if wall is None:
        ctx.sleep(seconds)
        return
    until = (wall - ctx.clock()).total_seconds()
    ctx.sleep(max(0.0, min(seconds, until)))


def deadline_reached(ctx: Ctx) -> bool:
    dl = ctx.nightly_deadline
    return dl is not None and ctx.clock() >= dl


def timeout_before_deadline(ctx: Ctx, configured: Optional[float]) -> Optional[float]:
    """A long phase's subprocess timeout, capped by the nightly deadline: the
    phase must stop at the wall, not at its own (possibly longer) budget.
    ``configured`` may be None (no timeout of its own); None also comes back
    when no deadline applies, leaving the phase exactly as it was."""
    dl = ctx.nightly_deadline
    if dl is None:
        return configured
    remaining = (dl - ctx.clock()).total_seconds()
    if configured is None:
        return max(0.0, remaining)
    return max(0.0, min(configured, remaining))


class DeadlineVoid(Exception):
    """An in-flight attempt cut by the nightly deadline at a stage boundary.
    Carries the readings the attempt already took; they become the voided
    attempt's history entry — never a measurement (abort is not fail)."""

    def __init__(self, readings: dict):
        super().__init__("nightly deadline reached mid-attempt")
        self.readings = readings


def guard_deadline(ctx: Ctx, rec: dict, wall: Optional[dt.datetime]) -> None:
    """Void the in-flight attempt at a stage boundary once the caller's wall has
    been reached (``wall`` is the deadline that applies to this call — None for
    the recovery path, which no wall governs): nothing measured after the
    deadline enters the record, and the attempt never becomes a timeout error
    on the way out."""
    if wall is None or ctx.clock() < wall:
        return
    raise DeadlineVoid({k: rec[k] for k in ("idle_pre", "idle_post") if k in rec})


# ---- source --------------------------------------------------------------


def git(repo: Path, *a: str, timeout: float = GIT_TIMEOUT_S) -> subprocess.CompletedProcess:
    return _run(["git", "-C", str(repo), *a], env=clean_env({"GIT_LFS_SKIP_SMUDGE": "1"}), timeout=timeout)


def prepare_checkout(ctx: Ctx) -> str:
    """Fetch the gate's own clone and check out the commit under test; return its SHA.

    Every blocking step is bounded by the nightly deadline's remaining time: a
    run that started live but runs its source prep into the wall stops waiting
    for the network there (the legs then fall deadline skips — a source error
    never outranks the wall)."""
    repo = Path(ctx.args.work_dir).expanduser() / "repo"
    if not (repo / ".git").is_dir():
        ctx.log(f"cloning {ctx.args.repo_url} into {repo}")
        repo.parent.mkdir(parents=True, exist_ok=True)
        p = _run(["git", "clone", "--no-checkout", ctx.args.repo_url, str(repo)],
                 env=clean_env({"GIT_LFS_SKIP_SMUDGE": "1"}),
                 timeout=timeout_before_deadline(ctx, GIT_CLONE_TIMEOUT_S))
        if p.returncode != 0:
            raise RuntimeError(f"git clone failed: {p.stderr[-500:]}")
    p = git(repo, "fetch", "--quiet", "origin", "main",
            timeout=timeout_before_deadline(ctx, GIT_TIMEOUT_S))
    if p.returncode != 0:
        raise RuntimeError(f"git fetch failed: {p.stderr[-500:]}")
    target = ctx.args.sha or "origin/main"
    p = git(repo, "rev-parse", "--verify", target + "^{commit}",
            timeout=timeout_before_deadline(ctx, GIT_TIMEOUT_S))
    if p.returncode != 0:
        raise RuntimeError(f"cannot resolve {target}: {p.stderr[-300:]}")
    sha = p.stdout.strip()
    p = git(repo, "checkout", "--quiet", "--detach", "--force", sha,
            timeout=timeout_before_deadline(ctx, GIT_TIMEOUT_S))
    if p.returncode != 0:
        raise RuntimeError(f"git checkout {sha} failed: {p.stderr[-500:]}")
    return sha


def local_repo(ctx: Ctx) -> Path:
    return Path(ctx.args.work_dir).expanduser() / "repo"


# ---- pytest --------------------------------------------------------------


def pytest_argv(python: str, leg: str, junit: str, plugin_dir: Optional[str],
                threshold_override: Optional[float]) -> List[str]:
    path, _ = GATE_TESTS[leg]
    argv = [python, "-m", "pytest", path, "-m", "slow", "-s", "-p", "no:cacheprovider",
            f"--junitxml={junit}"]
    if threshold_override is not None:
        argv += ["-p", OVERRIDE_PLUGIN_NAME, f"--dual-gate-threshold={threshold_override}"]
    return argv


def write_override_plugin(d: Path) -> None:
    (d / f"{OVERRIDE_PLUGIN_NAME}.py").write_text(OVERRIDE_PLUGIN_SOURCE)


# ---- metal leg -------------------------------------------------------------


def metal_build_tools_found() -> Dict[str, Optional[str]]:
    """Resolve the metal build tools from this process's PATH — the same PATH the
    build inherits (`clean_env` keeps it). IO seam: tests inject the result."""
    return {t: shutil.which(t) for t in METAL_BUILD_TOOLS}


def run_metal(ctx: Ctx, sha: str) -> dict:
    rec: dict = {}
    reason = missing_build_tools_reason(metal_build_tools_found())
    if reason:
        # A missing build tool is a deterministic configuration error, reported
        # before anything expensive — even before the idle pre-check — so this
        # record has no idle readings: none were taken, none are faked.
        return {"status": "error", "reason": reason}
    th = ctx.thresholds
    read = lambda: collect_readings(with_wsl_host=False)  # noqa: E731
    ok, r, reasons = wait_idle(ctx, read, "metal pre-check")
    rec["idle_pre"] = r.to_json()
    guard_deadline(ctx, rec, ctx.nightly_deadline)
    if not ok:
        return {**rec, **busy_skip("pre-check", reasons)}
    repo = local_repo(ctx)
    t0 = time.monotonic()
    rc = run_logged(ctx, ["./scripts/build.sh", "-j", "release"], ctx.run_dir / "metal-build.log",
                    cwd=repo, env=clean_env({}), timeout=timeout_before_deadline(ctx, None))
    rec["build_seconds"] = round(time.monotonic() - t0, 1)
    guard_deadline(ctx, rec, ctx.nightly_deadline)
    if rc != 0:
        return {**rec, "status": "error", "reason": f"build failed (exit {rc}), see metal-build.log"}
    ok, r, reasons = wait_idle(ctx, read, "metal re-check")
    rec["idle_post"] = r.to_json()
    guard_deadline(ctx, rec, ctx.nightly_deadline)
    if not ok:
        return {**rec, **busy_skip("post-build re-check", reasons)}
    junit = ctx.run_dir / "metal-junit.xml"
    pyenv = {"LUMICE_BIN": str(repo / "build" / "cmake_install" / "static" / "Lumice")}
    if ctx.synthetic:
        write_override_plugin(ctx.run_dir)
        pyenv["PYTHONPATH"] = str(ctx.run_dir)
    t0 = time.monotonic()
    rc = run_logged(ctx, pytest_argv(sys.executable, "metal", str(junit), str(ctx.run_dir),
                                     ctx.args.threshold_override),
                    ctx.run_dir / "metal-pytest.log", cwd=repo, env=clean_env(pyenv),
                    timeout=timeout_before_deadline(ctx, ctx.args.pytest_timeout))
    rec["pytest_seconds"] = round(time.monotonic() - t0, 1)
    guard_deadline(ctx, rec, ctx.nightly_deadline)
    rec["idle_after_pytest"] = read().to_json()
    if ctx.args.dry_run:
        return {**rec, "status": "skipped", "reason": "dry-run"}
    junit_text = junit.read_text() if junit.is_file() else None
    status, reason = classify_pytest(None if rc == 124 else rc, junit_text, GATE_TESTS["metal"][1])
    log_text = (ctx.run_dir / "metal-pytest.log").read_text(errors="replace")
    return {**rec, "status": status, "reason": reason, "pytest_exit": rc, **extract_ratios(log_text)}


# ---- gui real-timing leg (local) --------------------------------------------
# Drives gui_test's real-timing pool — the functional contracts whose only home
# is this pool (they have no fixed-dt sibling and no CI execution point). The
# build is this leg's own tree in the gate clone: build.sh is off limits here
# because -gt/-gtj RUNS the gui pools itself, and the leg must own the gui_test
# invocation it classifies. The tree name keeps it apart from the metal leg's
# build.sh tree; the two trees share only the source-anchored output directory
# (build/<type>/<flavor>), whose file names do not collide (Lumice vs gui_test)
# and whose libs are never read across legs — the legs run strictly in sequence.
GUI_BUILD_TREE = "build/cmake_build/static-gate-gui"


def gui_test_binary(repo: Path) -> Path:
    # The output directory is anchored on CMAKE_SOURCE_DIR (CMakeLists.txt), so
    # the binary lands here regardless of the -B tree.
    return repo / "build" / "release" / "static" / "bin" / "gui_test"


def run_gui_realtiming(ctx: Ctx, sha: str) -> dict:
    rec: dict = {}
    reason = missing_build_tools_reason(metal_build_tools_found())
    if reason:
        return {"status": "error", "reason": reason}
    read = lambda: collect_readings(with_wsl_host=False)  # noqa: E731
    ok, r, reasons = wait_idle(ctx, read, "gui-real-timing pre-check")
    rec["idle_pre"] = r.to_json()
    guard_deadline(ctx, rec, ctx.nightly_deadline)
    if not ok:
        return {**rec, **busy_skip("pre-check", reasons)}
    repo = local_repo(ctx)
    build_dir = repo / GUI_BUILD_TREE
    t0 = time.monotonic()
    rc = run_logged(ctx, ["cmake", "-S", str(repo), "-B", str(build_dir), "-G", "Ninja",
                          "-DCMAKE_BUILD_TYPE=Release", "-DBUILD_TEST=ON", "-DBUILD_GUI=ON",
                          "-DBUILD_BENCH=OFF", "-DBUILD_SHARED_LIBS=OFF"],
                    ctx.run_dir / "gui-configure.log", cwd=repo, env=clean_env({}),
                    timeout=timeout_before_deadline(ctx, None))
    if rc == 0:
        rc = run_logged(ctx, ["cmake", "--build", str(build_dir), "--target", "gui_test"],
                        ctx.run_dir / "gui-build.log", cwd=repo, env=clean_env({}),
                        timeout=timeout_before_deadline(ctx, None))
    rec["build_seconds"] = round(time.monotonic() - t0, 1)
    guard_deadline(ctx, rec, ctx.nightly_deadline)
    if rc != 0:
        return {**rec, "status": "error", "reason": f"build failed (exit {rc}), see gui-build.log"}
    ok, r, reasons = wait_idle(ctx, read, "gui re-check")
    rec["idle_post"] = r.to_json()
    guard_deadline(ctx, rec, ctx.nightly_deadline)
    if not ok:
        return {**rec, **busy_skip("post-build re-check", reasons)}
    junit = ctx.run_dir / "gui-junit.xml"
    t0 = time.monotonic()
    rc = run_logged(ctx, [str(gui_test_binary(repo)), "--filter", GUI_REALTIMING_FILTER,
                          "--export-junit", str(junit), "--no-user-config"],
                    ctx.run_dir / "gui-test.log", cwd=repo, env=clean_env({}),
                    timeout=timeout_before_deadline(ctx, ctx.args.pytest_timeout))
    rec["gui_seconds"] = round(time.monotonic() - t0, 1)
    guard_deadline(ctx, rec, ctx.nightly_deadline)
    # One reading, taken the moment the child returned and timestamped as
    # sampling begins: it doubles as the post-run idle record and — on a signal
    # death — as the crash-moment load snapshot, the first sample after the
    # exit, before anything else can change the machine.
    sampled_at = iso(utcnow())
    r_after = read()
    rec["idle_after_gui"] = r_after.to_json()
    if ctx.args.dry_run:
        return {**rec, "status": "skipped", "reason": "dry-run"}
    sigill_load = None
    if rc in SIGILL_EXITS:
        sigill_load = {"load_snapshot": r_after.to_json(), "sampled_at": sampled_at}
    junit_text = junit.read_text() if junit.is_file() else None
    log_text = (ctx.run_dir / "gui-test.log").read_text(errors="replace")
    v = classify_gui_pool(rc, junit_text, log_text, sigill_load=sigill_load)
    out = {**rec, "status": v.status, "reason": v.reason, "gui_exit": rc}
    if v.sigill is not None:
        out["sigill_event"] = v.sigill
    if v.timing is not None:
        out.update(v.timing)
    if v.failing_case is not None:
        out["failing_case"] = v.failing_case
    return out


# ---- cuda leg (remote) ------------------------------------------------------


def ssh_argv(ctx: Ctx, *remote: str) -> List[str]:
    return ["ssh", "-o", "BatchMode=yes", "-o", f"ConnectTimeout={ctx.args.ssh_connect_timeout}",
            "-o", "ServerAliveInterval=15", "-o", "ServerAliveCountMax=3", ctx.args.remote, *remote]


def remote_readings(ctx: Ctx, wall: Optional[dt.datetime] = None) -> Readings:
    """Run this very file's collect_readings on the remote via `python3 -`.
    ``wall`` bounds the read when one applies (fresh attempts); the recovery
    path keeps the plain timeout."""
    timeout = 180.0 if wall is None else max(0.0, min(180.0, (wall - ctx.clock()).total_seconds()))
    try:
        # Through the common _run entry, so remote output follows the same
        # replace-decode rule as every other captured text.
        p = _run(ssh_argv(ctx, "python3", "-", "--collect-readings", "--wsl-host"),
                 input=Path(__file__).read_text(), timeout=timeout)
        if p.returncode != 0:
            raise RuntimeError(f"exit {p.returncode}: {p.stderr[-300:]}")
        return Readings.from_json(json.loads(p.stdout.strip().splitlines()[-1]))
    except (RuntimeError, subprocess.TimeoutExpired, ValueError, IndexError) as e:
        return Readings(load1=float("nan"), physical_cores=1, errors=[f"remote readings failed: {e}"])


def remote_phase_script(ctx: Ctx, phase: str, sha: str, rdir: str) -> str:
    """A self-contained bash script that runs on the remote and leaves its result on remote disk.

    The script records its own pid (the session leader that `setsid nohup`
    created): that file is the wall's stop interface — the whole phase process
    group can be killed by it when an attempt is voided. The pytest phase's own
    timeout is bounded by the deadline's remaining time at start."""
    base = ctx.args.remote_dir
    common = f"""set -u
mkdir -p "$HOME/{base}/results/{rdir}"
echo $$ > "$HOME/{base}/results/{rdir}/{phase}.pid"
trap 'rc=$?; [ -f "$HOME/{base}/results/{rdir}/{phase}.done" ] || {{ echo "$rc" > "$HOME/{base}/results/{rdir}/{phase}.exit"; touch "$HOME/{base}/results/{rdir}/{phase}.done"; }}' EXIT
source "$HOME/lumice-env.sh" >/dev/null
REPO="$HOME/{base}/repo"
OUT="$HOME/{base}/results/{rdir}"
cd "$REPO" || {{ echo 97 > "$OUT/{phase}.exit"; touch "$OUT/{phase}.done"; exit 0; }}
"""
    if phase == "build":
        body = f"""
GIT_LFS_SKIP_SMUDGE=1 git -c filter.lfs.process= -c filter.lfs.smudge= -c filter.lfs.required=false \\
  checkout --quiet --detach --force {sha} > "$OUT/build.log" 2>&1
rc=$?
if [ $rc -eq 0 ]; then
  cmake -S "$REPO" -B build/cmake_build/static -G Ninja -DCMAKE_BUILD_TYPE=Release \\
    -DBUILD_TEST=OFF -DBUILD_GUI=OFF -DBUILD_BENCH=OFF -DBUILD_SHARED_LIBS=OFF \\
    -DFETCHCONTENT_FULLY_DISCONNECTED=ON -DLUMICE_CUDA_ENABLED=ON \\
    -DCMAKE_CUDA_COMPILER="$CUDACXX" -DCMAKE_CUDA_ARCHITECTURES="$LUMICE_CUDA_ARCHS" \\
    -DCMAKE_INSTALL_PREFIX="$REPO/build/cmake_install/static" >> "$OUT/build.log" 2>&1 \\
  && cmake --build build/cmake_build/static -j "$(nproc)" >> "$OUT/build.log" 2>&1 \\
  && cmake --build build/cmake_build/static --target install >> "$OUT/build.log" 2>&1
  rc=$?
fi
echo $rc > "$OUT/build.exit"
touch "$OUT/build.done"
"""
    else:
        plugin_env = f'PYTHONPATH="$OUT" ' if ctx.synthetic else ""
        argv = pytest_argv("python", "cuda", "$OUT/junit.xml", None, ctx.args.threshold_override)
        # $OUT must expand on the remote: quote everything except that variable.
        argv_s = " ".join(a if "$OUT" in a else shlex.quote(a) for a in argv)
        argv_s = argv_s.replace("--junitxml=$OUT/junit.xml", '--junitxml="$OUT/junit.xml"')
        remote_budget = int(timeout_before_deadline(ctx, ctx.args.pytest_timeout))
        body = f"""
for v in $(env | sed -n 's/^\\(LUMICE_[A-Za-z0-9_]*\\)=.*/\\1/p'); do unset "$v"; done
LUMICE_HAS_CUDA=1 LUMICE_BIN="$REPO/build/cmake_install/static/Lumice" {plugin_env}\\
  timeout {remote_budget} {argv_s} > "$OUT/pytest.log" 2>&1
echo $? > "$OUT/pytest.exit"
touch "$OUT/pytest.done"
"""
    return common + body


def remote_start_phase(ctx: Ctx, phase: str, sha: str, rdir: str) -> bool:
    script = remote_phase_script(ctx, phase, sha, rdir)
    out = f"{ctx.args.remote_dir}/results/{rdir}"
    # The braces matter: a bare trailing `&` backgrounds the whole && list, and a
    # background job's stdin is /dev/null — `cat` then writes an empty script.
    remote_cmd = (f"mkdir -p {out} && cat > {out}/{phase}.sh && "
                  f"{{ setsid nohup bash {out}/{phase}.sh > {out}/{phase}.nohup 2>&1 < /dev/null & }}")
    rc = run_logged(ctx, ssh_argv(ctx, remote_cmd), ctx.run_dir / "cuda-ssh.log",
                    timeout=timeout_before_deadline(ctx, 120), stdin_text=script)
    return rc == 0


def remote_wait_phase(ctx: Ctx, phase: str, rdir: str, budget_s: float,
                      wall: Optional[dt.datetime] = None) -> Optional[int]:
    """Poll the remote done-marker with short, independent ssh sessions; a dropped
    session costs one poll, not the run. Returns the phase's exit code or None.

    ``wall`` (a fresh attempt's nightly deadline) governs this wait for real: no
    poll is issued past it, the poll sleep never crosses it, and each ssh is
    bounded by the nearer of the two budgets — so the wait ends at the wall
    instead of sleeping a configured interval past it. The budget check stays
    after the poll: a phase that completed inside the last window is still read
    (the pre-existing behavior, kept for the recovery path's plain budget)."""
    out = f"{ctx.args.remote_dir}/results/{rdir}"
    budget_deadline = time.monotonic() + budget_s
    while True:
        if ctx.args.dry_run:
            return 0
        if wall is not None and ctx.clock() >= wall:
            ctx.log(f"remote {phase}: nightly deadline reached; wait ends")
            return None
        ssh_timeout = 60.0
        if wall is not None:
            ssh_timeout = min(ssh_timeout, max(0.0, (wall - ctx.clock()).total_seconds()))
        ssh_timeout = min(ssh_timeout, max(0.0, budget_deadline - time.monotonic()))
        try:
            p = _run(ssh_argv(ctx, f"test -f {out}/{phase}.done && cat {out}/{phase}.exit"),
                     timeout=ssh_timeout)
        except subprocess.TimeoutExpired:
            p = None
        if p is not None and p.returncode == 0 and p.stdout.strip():
            try:
                return int(p.stdout.strip())
            except ValueError:
                return None
        if time.monotonic() >= budget_deadline:
            return None
        if wall is not None and ctx.clock() >= wall:
            ctx.log(f"remote {phase}: nightly deadline reached; wait ends")
            return None
        sleep_before(ctx, ctx.args.remote_poll, wall)


def remote_stop_phase(ctx: Ctx, phase: str, rdir: str) -> bool:
    """Kill a detached remote phase's whole process group via the pid file its
    script wrote — the wall's stop interface for the cuda leg: an attempt
    voided by the deadline must not keep building or measuring on the remote.
    A phase that already completed needs no stop; a stop that could not be
    issued is logged and stays visible, never swallowed."""
    out = f"{ctx.args.remote_dir}/results/{rdir}"
    cmd = (f"if [ -f {out}/{phase}.done ]; then :; "
           f"elif [ -f {out}/{phase}.pid ]; then kill -- -\"$(cat {out}/{phase}.pid)\"; fi")
    rc = run_logged(ctx, ssh_argv(ctx, cmd), ctx.run_dir / "cuda-ssh.log", timeout=60)
    ok = rc == 0
    if not ok:
        ctx.log(f"remote {phase} stop failed (exit {rc}); the phase may still be running")
    else:
        ctx.log(f"remote {phase} stopped (pid file kill), or already done")
    return ok


def remote_fetch(ctx: Ctx, rdir: str, wall: Optional[dt.datetime] = None) -> None:
    dst = ctx.run_dir / "remote"
    dst.mkdir(exist_ok=True)
    timeout = 300.0 if wall is None else max(0.0, min(300.0, (wall - ctx.clock()).total_seconds()))
    run_logged(ctx, ["rsync", "-az", f"{ctx.args.remote}:{ctx.args.remote_dir}/results/{rdir}/", f"{dst}/"],
               ctx.run_dir / "cuda-ssh.log", timeout=timeout)


def remote_sync_source(ctx: Ctx, sha: str) -> Optional[str]:
    """Push the commit into a persistent repo on the remote (only changed files get new
    mtimes, so the remote build stays incremental), and top up its CPM source cache."""
    rrepo = f"{ctx.args.remote_dir}/repo"
    rc = run_logged(ctx, ssh_argv(ctx, f"mkdir -p {rrepo} && cd {rrepo} && "
                                  f"(test -d .git || git init -q) && git config receive.denyCurrentBranch ignore"),
                    ctx.run_dir / "cuda-ssh.log", timeout=timeout_before_deadline(ctx, 120))
    if rc != 0:
        return f"remote repo init failed (exit {rc})"
    rc = run_logged(ctx, ["git", "-C", str(local_repo(ctx)), "-c", "core.hooksPath=/dev/null", "push",
                          "--quiet", "--force", f"{ctx.args.remote}:{rrepo}", f"{sha}:refs/heads/gate-target"],
                    ctx.run_dir / "cuda-ssh.log", env=clean_env({"GIT_LFS_SKIP_PUSH": "1"}),
                    timeout=timeout_before_deadline(ctx, 1800))
    if rc != 0:
        return f"git push to remote failed (exit {rc})"
    cpm = Path(ctx.args.cpm_cache).expanduser()
    if cpm.is_dir():
        rc = run_logged(ctx, ["rsync", "-az", f"{cpm}/", f"{ctx.args.remote}:.cache/lumice-cpm/"],
                        ctx.run_dir / "cuda-ssh.log", timeout=timeout_before_deadline(ctx, 3600))
        if rc != 0:
            return f"CPM cache rsync failed (exit {rc})"
    return None


def run_cuda(ctx: Ctx, sha: str, resume_rdir: Optional[str] = None) -> dict:
    rec: dict = {"remote": ctx.args.remote}
    if not ctx.args.dry_run:
        try:
            p = _run(ssh_argv(ctx, "true"), timeout=ctx.args.ssh_connect_timeout + 15)
        except (subprocess.TimeoutExpired, OSError) as e:
            return {**rec, "status": "skipped", "reason": f"unreachable ({type(e).__name__})"}
        if p.returncode != 0:
            return {**rec, "status": "skipped", "reason": f"unreachable (ssh exit {p.returncode})"}
    # One remote results directory per attempt: a retried attempt starts its
    # phases in a fresh directory and can never read a previous attempt's done
    # markers. The recovery path keeps the recorded directory it re-attaches to.
    rdir = resume_rdir or (ctx.run_id if ctx.attempt_no == 1
                           else f"{ctx.run_id}-a{ctx.attempt_no}")
    rec["remote_run_dir"] = rdir
    # The wall governs fresh attempts; the --collect-remote recovery path has no
    # attempt structure, so no deadline checkpoint covers it.
    wall = ctx.nightly_deadline if resume_rdir is None else None
    if resume_rdir is None:
        ok, r, reasons = wait_idle(ctx, lambda: remote_readings(ctx, wall), "cuda pre-check")
        rec["idle_pre"] = r.to_json()
        guard_deadline(ctx, rec, wall)
        if not ok:
            return {**rec, **busy_skip("pre-check", reasons)}
        err = remote_sync_source(ctx, sha)
        guard_deadline(ctx, rec, wall)
        if err:
            return {**rec, "status": "error", "reason": err}
        t0 = time.monotonic()
        if not remote_start_phase(ctx, "build", sha, rdir):
            guard_deadline(ctx, rec, wall)   # a start killed by the wall is a void, not an error
            return {**rec, "status": "error", "reason": "could not start remote build"}
        brc = remote_wait_phase(ctx, "build", rdir,
                                timeout_before_deadline(ctx, ctx.args.remote_build_timeout),
                                wall=wall)
        rec["build_seconds"] = round(time.monotonic() - t0, 1)
        if wall is not None and ctx.clock() >= wall:
            remote_stop_phase(ctx, "build", rdir)
        guard_deadline(ctx, rec, wall)
        if brc != 0:
            remote_fetch(ctx, rdir, wall=wall)
            return {**rec, "status": "error",
                    "reason": f"remote build failed (exit {brc}), see remote/build.log"}
        ok, r, reasons = wait_idle(ctx, lambda: remote_readings(ctx, wall), "cuda re-check")
        rec["idle_post"] = r.to_json()
        guard_deadline(ctx, rec, wall)
        if not ok:
            return {**rec, **busy_skip("post-build re-check", reasons)}
        if ctx.synthetic and not ctx.args.dry_run:
            out = f"{ctx.args.remote_dir}/results/{rdir}"
            run_logged(ctx, ssh_argv(ctx, f"cat > {out}/{OVERRIDE_PLUGIN_NAME}.py"),
                       ctx.run_dir / "cuda-ssh.log", timeout=60, stdin_text=OVERRIDE_PLUGIN_SOURCE)
        if not remote_start_phase(ctx, "pytest", sha, rdir):
            guard_deadline(ctx, rec, wall)
            return {**rec, "status": "error", "reason": "could not start remote pytest"}
    t0 = time.monotonic()
    wait_budget = ctx.args.pytest_timeout + 600
    if resume_rdir is None:
        wait_budget = timeout_before_deadline(ctx, wait_budget)
    prc = remote_wait_phase(ctx, "pytest", rdir, wait_budget, wall=wall)
    rec["pytest_wait_seconds"] = round(time.monotonic() - t0, 1)
    if wall is not None and ctx.clock() >= wall:
        remote_stop_phase(ctx, "pytest", rdir)
    guard_deadline(ctx, rec, wall)
    rec["idle_after_pytest"] = remote_readings(ctx, wall).to_json()
    if ctx.args.dry_run:
        return {**rec, "status": "skipped", "reason": "dry-run"}
    remote_fetch(ctx, rdir, wall=wall)
    guard_deadline(ctx, rec, wall)
    got = ctx.run_dir / "remote"
    junit = got / "junit.xml"
    exit_file = got / "pytest.exit"
    try:
        rc = int(exit_file.read_text().strip()) if exit_file.is_file() else prc
    except ValueError:
        rc = None
    if rc is None:
        return {**rec, "status": "error", "reason": "remote pytest result never appeared (lost run)"}
    status, reason = classify_pytest(None if rc == 124 else rc,
                                     junit.read_text() if junit.is_file() else None,
                                     GATE_TESTS["cuda"][1])
    log = got / "pytest.log"
    ratios = extract_ratios(log.read_text(errors="replace")) if log.is_file() else {}
    return {**rec, "status": status, "reason": reason, "pytest_exit": rc, **ratios}


# ---- leg-level retry loop (only idle_verdict rejections retry) ---------------


def backoff_sleep(ctx: Ctx, seconds: float) -> None:
    """Sleep one backoff off, never across the nightly deadline: the wait ends
    at the wall, not a configured duration later (the loop-top deadline gate
    then decides the leg's end)."""
    dl = ctx.nightly_deadline
    remaining = seconds
    while remaining > 0:
        now = ctx.clock()
        if dl is not None and now >= dl:
            return
        if dl is None:
            ctx.sleep(remaining)
            return
        until_dl = (dl - now).total_seconds()
        if until_dl <= 0:
            return
        chunk = min(remaining, until_dl)
        ctx.sleep(chunk)
        remaining -= chunk


def idle_retry_payload(ctx: Ctx, attempts: int, backoffs: int, history: List[dict],
                       terminal: Optional[str]) -> dict:
    """The record's ``idle_retry`` payload: the budget actually spent, one
    history entry per attempt that did not become the final record, and — only
    when the retry controller itself forced this end — the cause (budget
    exhaustion or the wall). A leg that returned normally carries no
    ``terminal``: its status already says what happened."""
    payload = {"attempts": attempts, "backoffs": backoffs,
               "backoff_seconds": ctx.args.idle_retry_backoff_seconds, "history": history}
    if terminal is not None:
        payload["terminal"] = terminal
    return payload


def run_leg_with_retry(ctx: Ctx, leg: str, runner: Callable[[Ctx, str], dict], sha: str) -> dict:
    """Wrap one leg's runner in the structured idle-retry loop.

    One attempt = one full leg execution (both idle windows belong to it). Only
    an ``is_env_busy_skip`` attempt costs the attempt and is followed by a
    backoff; fail / error / inconclusive / unreachable / locked are terminal
    states on their first appearance, and the leg's own exceptions propagate
    unchanged (one error record, no retry budget spent). The loop ends with the
    controller's cause in ``idle_retry.terminal`` when the backoff budget is
    exhausted or the nightly deadline is reached; a leg whose first attempt
    never ran (the wall passed before its turn) falls a single skip line with
    no ``idle_retry`` payload — zero attempts is not a retry history.
    """
    max_backoffs = ctx.args.idle_retry_max_backoffs
    attempts = 0
    backoffs = 0
    history: List[dict] = []
    last_busy: Optional[dict] = None
    while True:
        if attempts == 0 and deadline_reached(ctx):
            ctx.log(f"{leg}: nightly deadline reached before this leg started")
            return {"status": "skipped",
                    "reason": f"nightly deadline {ctx.nightly_deadline:%H:%M} reached "
                              f"before this leg started"}
        ctx.log(f"{leg}: attempt {attempts + 1}")
        ctx.attempt_no = attempts + 1
        t0 = ctx.clock()
        attempts += 1
        try:
            rec = runner(ctx, sha)
        except DeadlineVoid as e:
            ended = ctx.clock()
            history.append({"attempt": attempts, "ended_at": iso(ended),
                            "seconds": round((ended - t0).total_seconds(), 1), "voided": True,
                            **e.readings})
            ctx.log(f"{leg}: attempt {attempts} voided by the nightly deadline")
            return {"status": "skipped",
                    "reason": f"nightly deadline {ctx.nightly_deadline:%H:%M} reached at "
                              f"{iso(ended)}; in-flight attempt {attempts} voided before measurement",
                    "idle_retry": idle_retry_payload(ctx, attempts, backoffs, history, "deadline")}
        ended = ctx.clock()
        if not is_env_busy_skip(rec):
            if backoffs > 0:
                rec["idle_retry"] = idle_retry_payload(ctx, attempts, backoffs, history, None)
            return rec
        history.append({"attempt": attempts, "ended_at": iso(ended),
                        "seconds": round((ended - t0).total_seconds(), 1), "reason": rec["reason"],
                        **{k: rec[k] for k in ("idle_pre", "idle_post") if k in rec}})
        last_busy = rec
        if backoffs >= max_backoffs:
            ctx.log(f"{leg}: idle-retry budget exhausted after {attempts} attempts")
            return {**rec, "idle_retry": idle_retry_payload(ctx, attempts, backoffs,
                                                            history[:-1], "max_backoffs")}
        backoffs += 1
        ctx.log(f"{leg}: busy; backoff {backoffs}/{max_backoffs} "
                f"({ctx.args.idle_retry_backoff_seconds:.0f}s)")
        backoff_sleep(ctx, ctx.args.idle_retry_backoff_seconds)
        if deadline_reached(ctx):
            ctx.log(f"{leg}: nightly deadline reached; no further attempt")
            return {**rec, "idle_retry": idle_retry_payload(ctx, attempts, backoffs,
                                                            history[:-1], "deadline")}


# ---- notifications --------------------------------------------------------


def gh(ctx: Ctx, *a: str) -> subprocess.CompletedProcess:
    return _run(["gh", *a, "--repo", ctx.args.gh_repo], timeout=60)


def open_issues(ctx: Ctx, leg: str, namespace: str) -> Dict[str, int]:
    """kind -> open issue number, found by exact title (the fingerprint)."""
    p = gh(ctx, "issue", "list", "--state", "open", "--search", f"{ISSUE_TAG} in:title",
           "--json", "number,title", "--limit", "100")
    if p.returncode != 0:
        raise RuntimeError(f"gh issue list failed: {p.stderr.strip()[-300:]}")
    by_title = {i["title"]: i["number"] for i in json.loads(p.stdout)}
    return {k: by_title[issue_title(leg, k, namespace)] for k in ISSUE_KINDS
            if issue_title(leg, k, namespace) in by_title}


def apply_action(ctx: Ctx, leg: str, namespace: str, action: Action) -> None:
    """Idempotent against a fresh query, so a replayed pending action cannot duplicate an issue."""
    opened = open_issues(ctx, leg, namespace)
    title = issue_title(leg, action.kind, namespace)
    if action.op == "create":
        if action.kind in opened:
            return
        p = gh(ctx, "issue", "create", "--title", title, "--body", action.body)
    elif action.op == "comment":
        if action.kind not in opened:
            return
        p = gh(ctx, "issue", "comment", str(opened[action.kind]), "--body", action.body)
    else:
        if action.kind not in opened:
            return
        p = gh(ctx, "issue", "close", str(opened[action.kind]), "--comment", action.body)
    if p.returncode != 0:
        raise RuntimeError(f"gh issue {action.op} failed: {p.stderr.strip()[-300:]}")
    ctx.log(f"notify: {action.op} {title!r}")


PUBLIC_REASON_CATEGORIES = (
    "busy", "unreachable", "dry-run", "locked", "no-test-executed", "no-junit", "junit-unparseable",
    "pytest-did-not-finish", "pytest-exit", "build failed", "build tools missing from PATH",
    "remote build failed", "remote pytest", "could not start", "source", "internal",
    "metal leg needs macOS", "gui-real-timing leg needs macOS",
    # gui-real-timing leg verdict categories: the case names and evidence stay in
    # the local record; the public issue shows only which class of red it was.
    "functional red", "timeout", "budget red", "sigill", "gate-timeout", "gui-test junit mismatch",
    "gui-test-did-not-finish", "gui-test-exit")


def public_reason(reason: str) -> str:
    """Category of a reason, safe for a public issue: the free text after it (process
    command lines, local paths, git stderr) stays in the local jsonl."""
    for c in PUBLIC_REASON_CATEGORIES:
        if reason.startswith(c):
            return c
    return "unspecified"


LEG_ROLES = {
    "metal": "Metal reference machine",
    "cuda": "CUDA reference machine (Linux)",
    "gui-real-timing": "GUI real-timing pool, on the Metal reference machine",
}


def issue_body(leg: str, rec: dict, stale_since: Optional[str]) -> str:
    lines = [
        f"Leg: `{leg}` — role: {LEG_ROLES.get(leg, 'unlisted role')}",
        f"Commit: `{rec.get('sha', '?')}`",
        f"Run: `{rec.get('run_id')}`" + ("  (synthetic: threshold overridden)" if rec.get("synthetic") else ""),
        f"Status: **{rec['status']}**" + (f" — {public_reason(rec['reason'])}" if rec.get("reason") else ""),
    ]
    if "ratio_a" in rec:
        lines.append(f"dual/single_a = {rec['ratio_a']:.3f}, dual/single_b = {rec['ratio_b']:.3f}"
                     f" (gate >= {rec.get('threshold', '?')}); dual/legacy = {rec.get('vs_legacy', '?')}x")
    if leg == "gui-real-timing":
        # Identifiers only — a case name and the SIGILL exit/marker are repo
        # symbols a reader can grep; evidence free text (log tails, load
        # snapshots) stays in the local record.
        if rec.get("failing_case"):
            lines.append(f"Failing case: `{rec['failing_case']}`")
        event = rec.get("sigill_event")
        if event:
            lines.append(f"SIGILL sample: exit `{event.get('exit_code', '?')}`, "
                         f"last case marker: `{event.get('last_case_marker', '?')}`")
    if stale_since:
        lines.append(f"Last real measurement (pass or fail): {stale_since}")
    lines.append("")
    lines.append("Posted by `scripts/local_throughput_gate.py`; how to read it: "
                 "`doc/testing/performance-testing.md`, \"Precise throughput gate (local schedule)\".")
    return "\n".join(lines)


def notify(ctx: Ctx, state: dict, leg: str, rec: dict, stale: bool) -> None:
    namespace = ctx.args.issue_namespace
    try:
        opened = set(open_issues(ctx, leg, namespace))
    except (RuntimeError, OSError, subprocess.TimeoutExpired, ValueError) as e:
        ctx.log(f"notify: cannot query issues ({e}); deciding as if none are open is unsafe — queueing")
        state["pending_notifications"].append({"leg": leg, "namespace": namespace, "deferred_record": rec,
                                               "stale": stale, "error": str(e)})
        rec["notify_failed"] = str(e)
        return
    body = issue_body(leg, rec, rec.get("last_real_run") if stale else None)
    for a in decide_notifications(rec["status"], bool(rec.get("synthetic")), stale, opened, body,
                                  sigill=bool(rec.get("sigill_event"))):
        if ctx.args.dry_run:
            ctx.log(f"notify (dry-run): {a.op} {issue_title(leg, a.kind, namespace)!r}")
            continue
        try:
            apply_action(ctx, leg, namespace, a)
        except (RuntimeError, OSError, subprocess.TimeoutExpired, ValueError) as e:
            ctx.log(f"notify failed, queued for next run: {e}")
            state["pending_notifications"].append({"leg": leg, "namespace": namespace,
                                                   "action": a.__dict__, "error": str(e)})
            rec["notify_failed"] = str(e)


def replay_pending(ctx: Ctx, state: dict) -> None:
    pending, state["pending_notifications"] = state["pending_notifications"], []
    for item in pending:
        try:
            if "action" in item:
                if not ctx.args.dry_run:
                    apply_action(ctx, item["leg"], item["namespace"], Action(**item["action"]))
            else:
                notify(ctx, state, item["leg"], item["deferred_record"], item["stale"])
        except (RuntimeError, OSError, subprocess.TimeoutExpired, ValueError) as e:
            item["error"] = str(e)
            state["pending_notifications"].append(item)


# =============================================================================
# main
# =============================================================================


def parse_hhmm(s: str) -> dt.time:
    return dt.datetime.strptime(s, "%H:%M").time()


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0],
                                formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    p.add_argument("--leg", choices=("metal", "gui-real-timing", "cuda", "all"), default="all")
    p.add_argument("--remote", default="home-wsl", help="ssh alias of the CUDA reference machine (Linux role)")
    p.add_argument("--remote-dir", default="Codes/lumice-throughput-gate",
                   help="remote directory, relative to the remote $HOME")
    p.add_argument("--repo-url", default="git@github.com:LoveDaisy/Lumice.git")
    p.add_argument("--gh-repo", default="LoveDaisy/Lumice")
    p.add_argument("--sha", default=None, help="commit to measure (default: origin/main tip)")
    p.add_argument("--work-dir", default="~/.cache/lumice-throughput-gate",
                   help="holds the gate's own clone; never a developer worktree")
    p.add_argument("--state-dir", default="~/.local/state/lumice-throughput-gate",
                   help="results.jsonl, state.json and runs/<run_id>/")
    p.add_argument("--cpm-cache", default="~/.cache/lumice-cpm",
                   help="local CPM source cache, rsynced to the remote before its build")
    p.add_argument("--load-per-core", type=float, default=DEFAULT_LOAD_PER_CORE)
    p.add_argument("--host-cpu-percent", type=float, default=DEFAULT_HOST_CPU_PERCENT)
    p.add_argument("--gpu-util-percent", type=float, default=DEFAULT_GPU_UTIL_PERCENT)
    p.add_argument("--cooldown-timeout", type=float, default=600.0)
    p.add_argument("--cooldown-poll", type=float, default=30.0)
    p.add_argument("--remote-poll", type=float, default=30.0)
    p.add_argument("--remote-build-timeout", type=float, default=3 * 3600.0)
    p.add_argument("--pytest-timeout", type=float, default=3600.0)
    p.add_argument("--ssh-connect-timeout", type=int, default=5)
    p.add_argument("--idle-retry-backoff-seconds", type=float, default=3600.0,
                   help="busy attempt: sleep this long before the next attempt")
    p.add_argument("--idle-retry-max-backoffs", type=int, default=2,
                   help="max backoff sleeps per leg (attempts = max-backoffs + 1)")
    p.add_argument("--nightly-deadline", type=parse_hhmm, default=dt.time(6, 30),
                   help="local-time wall bounding launchd runs: no attempt starts or continues "
                        "past it; manual runs are unbounded")
    p.add_argument("--stale-days", type=float, default=DEFAULT_STALE_DAYS)
    p.add_argument("--issue-namespace", choices=("real", "test"), default="real",
                   help="'test' prefixes issue titles with [TEST] so a drill never touches real issues")
    p.add_argument("--threshold-override", type=float, default=None,
                   help="TEST ONLY: replace the 0.85 gate; the run is recorded as synthetic, does not "
                        "count as a real measurement, and requires --issue-namespace test")
    p.add_argument("--now", default=None, help="TEST ONLY: ISO time used for the staleness check")
    p.add_argument("--collect-remote", default=None, metavar="RUN_ID",
                   help="re-attach to a remote cuda run by id and collect its result (no new build)")
    p.add_argument("--trigger", choices=("manual", "launchd"), default="manual",
                   help="recorded only; the launchd plist passes 'launchd'")
    p.add_argument("--dry-run", action="store_true", help="print actions; build/run/notify nothing")
    p.add_argument("--collect-readings", action="store_true", help=argparse.SUPPRESS)
    p.add_argument("--wsl-host", action="store_true", help=argparse.SUPPRESS)
    a = p.parse_args(argv)
    if a.threshold_override is not None and a.issue_namespace != "test":
        p.error("--threshold-override requires --issue-namespace test")
    return a


def resolve_nightly_deadline(args: argparse.Namespace, log: Log) -> Optional[dt.datetime]:
    """The run's nightly deadline as today's local wall-clock time, or None when
    it does not apply. Applicability is decided once per run from the run's
    trigger (a launchd run is bounded whenever it started — a run that began
    past the wall is ended by the legs' zero-attempt skips; a manual run is the
    operator's explicit choice and unbounded) and logged to gate.log."""
    if args.trigger != "launchd":
        log(f"nightly deadline not applicable (trigger {args.trigger})")
        return None
    dl = dt.datetime.combine(dt.datetime.now().date(), args.nightly_deadline)
    log(f"nightly deadline {dl:%H:%M} applies (trigger launchd)")
    return dl


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    if args.collect_readings:
        print(json.dumps(collect_readings(with_wsl_host=args.wsl_host).to_json()))
        return 0

    state_dir = Path(args.state_dir).expanduser()
    runs_dir = state_dir / "runs"
    runs_dir.mkdir(parents=True, exist_ok=True)
    now = utcnow()
    run_id = now.strftime("%Y%m%dT%H%M%SZ") + f"-{os.getpid()}"
    run_dir = runs_dir / run_id
    run_dir.mkdir()
    log = Log(run_dir / "gate.log", args.dry_run)
    if args.dry_run:
        # Dry-run only forces the backoff to zero (the loop still runs, nothing
        # sleeps); it does not change the deadline's applicability, which the
        # trigger decides.
        args.idle_retry_backoff_seconds = 0.0
    nightly_deadline = resolve_nightly_deadline(args, log)
    ctx = Ctx(args, run_id, run_dir, log,
              IdleThresholds(args.load_per_core, args.host_cpu_percent, args.gpu_util_percent),
              synthetic=args.threshold_override is not None,
              nightly_deadline=nightly_deadline)
    run_dead_at_start = deadline_reached(ctx)
    if run_dead_at_start:
        # A nightly run that launched past the wall ends before it starts: skip
        # the source prep it can no longer use and let every leg fall its
        # zero-attempt deadline skip. A source error never outranks the wall.
        log("nightly deadline already passed; no attempt will start, "
            "every leg falls a deadline skip")
    legs = LEGS if args.leg == "all" else (args.leg,)
    if args.collect_remote:
        legs = ("cuda",)
    base = {"run_id": run_id, "time": iso(now), "synthetic": ctx.synthetic,
            "namespace": args.issue_namespace, "dry_run": args.dry_run,
            "idle_thresholds": ctx.thresholds.__dict__, "trigger": args.trigger}
    if args.threshold_override is not None:
        base["threshold_override"] = args.threshold_override

    lock = Lock(state_dir / "lock")
    if not lock.acquire():
        for leg in legs:
            append_record(state_dir, {**base, "leg": leg, "status": "skipped", "reason": "locked"})
        log("another gate run holds the lock; skipped")
        return 0

    state = load_state(state_dir, now)
    replay_pending(ctx, state)
    log(f"run {run_id}: legs={','.join(legs)} synthetic={ctx.synthetic} namespace={args.issue_namespace}")

    sha = args.sha or "?"
    source_error = None
    if not run_dead_at_start:
        try:
            if args.collect_remote:
                # Recovery only fetches an earlier remote run's results; that run measured a
                # commit this checkout may no longer be at, so do not stamp the record with it.
                sha = args.sha or "unknown (collect-remote)"
            elif args.dry_run and not (local_repo(ctx) / ".git").is_dir():
                log("dry-run: gate clone absent; would clone and check out origin/main")
            else:
                sha = prepare_checkout(ctx)
        except (RuntimeError, subprocess.TimeoutExpired, OSError) as e:
            source_error = f"{type(e).__name__}: {e}"
    log(f"commit under test: {sha}")

    worst = 0
    for leg in legs:
        t0 = time.monotonic()
        try:
            if source_error and not deadline_reached(ctx):
                # A source failure is the leg's story only while the run could
                # still attempt something; past the wall, the zero-attempt
                # deadline skip outranks it (M0.2: the wall forces terminal).
                rec = {"status": "error", "reason": f"source: {source_error}"}
            elif leg == "metal":
                if platform.system() != "Darwin":
                    rec = {"status": "error", "reason": "metal leg needs macOS"}
                else:
                    rec = run_leg_with_retry(ctx, leg, run_metal, sha)
            elif leg == "gui-real-timing":
                if platform.system() != "Darwin":
                    rec = {"status": "error", "reason": "gui-real-timing leg needs macOS"}
                else:
                    rec = run_leg_with_retry(ctx, leg, run_gui_realtiming, sha)
            elif args.collect_remote:
                # Recovery only re-attaches to an earlier remote run: no busy
                # branch, no attempt structure — the retry loop and the deadline
                # checkpoints do not cover it.
                rec = run_cuda(ctx, sha, resume_rdir=args.collect_remote)
            else:
                rec = run_leg_with_retry(ctx, leg, run_cuda, sha)
        except Exception as e:  # noqa: BLE001 - one leg must not take the other, or the state, down with it
            log(f"{leg}: unexpected {type(e).__name__}: {e}")
            rec = {"status": "error", "reason": f"internal: {type(e).__name__}: {e}"}
        rec = {**base, "leg": leg, "sha": sha, **rec, "seconds": round(time.monotonic() - t0, 1)}
        if refreshes_last_real_run(rec["status"], ctx.synthetic) and not args.dry_run:
            state["legs"].setdefault(leg, {})["last_real_run"] = iso(utcnow())
        stale_now = dt.datetime.fromisoformat(args.now) if args.now else utcnow()
        lrr = last_real_run(state, leg)
        rec["last_real_run"] = iso(lrr)
        stale = is_stale(lrr, stale_now, args.stale_days)
        rec["stale"] = stale
        log(f"{leg}: {rec['status']}" + (f" — {rec['reason']}" if rec.get("reason") else "")
            + (f"  ratios {rec.get('ratio_a')}/{rec.get('ratio_b')}" if "ratio_a" in rec else ""))
        notify(ctx, state, leg, rec, stale)
        append_record(state_dir, rec)
        if rec["status"] in ("fail", "error"):
            worst = 1
        if not args.dry_run:
            save_state(state_dir, state)  # per leg: a later leg's failure cannot lose this one's clock
    prune_run_dirs(runs_dir, KEEP_RUN_DIRS)
    return worst


if __name__ == "__main__":
    sys.exit(main())
