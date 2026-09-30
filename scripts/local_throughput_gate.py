#!/usr/bin/env python3
"""Run the precise dual-renderer throughput gates on the reference machines, unattended.

Why this exists. The 0.85 dual-vs-single throughput gate
(``test/e2e/_multi_renderer_throughput.py``) was calibrated on the reference
machines' noise (Metal CoV 6-10%, CUDA 10-17%). A GitHub-hosted runner is slower
and noisier than that, so on CI the same number reads red with nothing wrong.
The precise gate therefore runs here, on a schedule (``launchd``, installed by
``scripts/install_throughput_gate.sh``), and CI keeps only a loose disaster
floor. Where it runs, how often, where results land and how to install it:
``doc/performance-testing.md``, "Precise throughput gate (local schedule)".

What one run does, per leg, in a fixed order:

    lock -> resolve the commit (origin/main tip) ->
      metal: local idle pre-check -> build -> cool-down -> idle re-check -> pytest
      cuda : probe remote -> remote idle pre-check -> sync + build (on remote disk)
             -> cool-down -> idle re-check -> pytest (on remote disk) -> collect
    -> append one record per leg to results.jsonl -> notify -> staleness check

Only the re-check after the build decides whether the gate is measured; the
pre-check exists so a busy machine is not made busier by a build nobody uses.

Each leg ends in exactly one of four states. ``pass`` and ``fail`` are real
measurements; ``skipped`` (busy, unreachable, locked) and ``error`` (build
failure, no test executed, lost remote run) are not, and are kept apart from
``fail`` so an infrastructure problem is never read as a throughput regression.
Only ``pass``/``fail`` count as "the gate really ran" for the staleness alarm.

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

LEGS = ("metal", "cuda")

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
# Matched as a substring of the full command line: the drive runners and pytest
# run under a generic interpreter name, so their executable name says nothing.
BUSY_ARG_MARKERS = (
    "task_drive.py", "chore.py", "scrum_drive.py", "explore_drive.py", "pytest",
)
# The Windows host of the WSL remote, read through WSL interop.
HOST_BUSY_EXECUTABLES = frozenset({
    "lumice", "lumicegui", "gui_test", "cl", "link", "nvcc", "cicc", "ptxas",
    "ninja", "cmake", "msbuild", "nsys", "ncu",
})
HOST_BUSY_ARG_MARKERS = ("pytest", "win_bench", "bench_throughput")

DEFAULT_LOAD_PER_CORE = 0.25      # 1-min loadavg / physical cores
DEFAULT_HOST_CPU_PERCENT = 25.0   # Windows host instantaneous CPU %
DEFAULT_GPU_UTIL_PERCENT = 10.0   # nvidia-smi utilization.gpu %
DEFAULT_STALE_DAYS = 3.0
KEEP_RUN_DIRS = 30

ISSUE_TAG = "[throughput-gate]"
TEST_NAMESPACE_TAG = "[TEST]"
ISSUE_KINDS = ("fail", "error", "stale")

_RATIO_RE = re.compile(r"dual/single_a=([0-9.]+) dual/single_b=([0-9.]+)")
_LEGACY_RE = re.compile(r"/legacy=([0-9.]+)x")
_GATE_RE = re.compile(r"gate >= ([0-9.]+)")

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


def busy_processes(procs: Sequence[Proc], self_pid: int) -> List[str]:
    skip = own_tree(procs, self_pid)
    out = []
    for p in procs:
        if p.pid in skip:
            continue
        if p.exe in BUSY_EXECUTABLES or any(m in p.args for m in BUSY_ARG_MARKERS):
            out.append(f"{p.pid} {p.exe}: {p.args[:120]}")
    return out


def host_busy_processes(names_and_args: Sequence[Tuple[str, str]]) -> List[str]:
    out = []
    for name, args in names_and_args:
        base = name.lower()
        if base.endswith(".exe"):
            base = base[:-4]
        if base in HOST_BUSY_EXECUTABLES or any(m in (args or "") for m in HOST_BUSY_ARG_MARKERS):
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
                         open_kinds: set, body: str) -> List[Action]:
    """The single decision table: (this leg's outcome x open issues) -> actions.

    status   | open issue of that kind    | action
    ---------+----------------------------+-----------------------------------
    fail     | fail open                  | comment (numbers only)
    fail     | fail not open              | create fail
    error    | error open                 | none
    error    | error not open             | create error
    pass     | (non-synthetic)            | close fail / error / stale if open
    pass     | (synthetic)                | none — a doctored threshold proves nothing
    fail     | (non-synthetic)            | also close stale if open (it really ran)
    skipped  | -                          | none
    stale    | stale or error open        | none (an open error already explains it)
    stale    | neither open               | create stale
    """
    actions: List[Action] = []
    if status == "fail":
        if "fail" in open_kinds:
            actions.append(Action("comment", "fail", body))
        else:
            actions.append(Action("create", "fail", body))
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


# =============================================================================
# IO: readings
# =============================================================================


def _run(cmd: Sequence[str], **kw) -> subprocess.CompletedProcess:
    return subprocess.run(list(cmd), capture_output=True, text=True, **kw)


def _physical_cores() -> int:
    if platform.system() == "Darwin":
        return int(_run(["sysctl", "-n", "hw.physicalcpu"]).stdout.strip())
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
        return json.loads(p.read_text())
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
        self.fd = open(self.path, "w")
        try:
            fcntl.flock(self.fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            self.fd.close()
            self.fd = None
            return False
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


def run_logged(ctx: Ctx, cmd: Sequence[str], log_path: Path, cwd: Optional[Path] = None,
               env: Optional[Dict[str, str]] = None, timeout: Optional[float] = None,
               stdin_text: Optional[str] = None) -> int:
    ctx.log(f"$ {' '.join(shlex.quote(c) for c in cmd)}  (> {log_path.name})")
    if ctx.args.dry_run:
        return 0
    with open(log_path, "a") as out:
        try:
            p = subprocess.run(list(cmd), cwd=cwd, env=env, stdout=out, stderr=subprocess.STDOUT,
                               timeout=timeout, text=True, input=stdin_text)
        except subprocess.TimeoutExpired:
            out.write(f"\n[gate] TIMEOUT after {timeout}s\n")
            return 124
    return p.returncode


def wait_idle(ctx: Ctx, read: Callable[[], Readings], label: str) -> Tuple[bool, Readings, List[str]]:
    """Poll until idle or the cool-down budget runs out. Returns the LAST reading."""
    deadline = time.monotonic() + ctx.args.cooldown_timeout
    while True:
        r = read()
        ok, reasons = idle_verdict(r, ctx.thresholds)
        if ok or ctx.args.dry_run or time.monotonic() >= deadline:
            ctx.log(f"{label}: {'idle' if ok else 'busy: ' + ' | '.join(reasons)}")
            return ok, r, reasons
        time.sleep(ctx.args.cooldown_poll)


# ---- source --------------------------------------------------------------


def git(repo: Path, *a: str) -> subprocess.CompletedProcess:
    return _run(["git", "-C", str(repo), *a], env=clean_env({"GIT_LFS_SKIP_SMUDGE": "1"}))


def prepare_checkout(ctx: Ctx) -> str:
    """Fetch the gate's own clone and check out the commit under test; return its SHA."""
    repo = Path(ctx.args.work_dir).expanduser() / "repo"
    if not (repo / ".git").is_dir():
        ctx.log(f"cloning {ctx.args.repo_url} into {repo}")
        repo.parent.mkdir(parents=True, exist_ok=True)
        p = _run(["git", "clone", "--no-checkout", ctx.args.repo_url, str(repo)],
                 env=clean_env({"GIT_LFS_SKIP_SMUDGE": "1"}))
        if p.returncode != 0:
            raise RuntimeError(f"git clone failed: {p.stderr[-500:]}")
    p = git(repo, "fetch", "--quiet", "origin", "main")
    if p.returncode != 0:
        raise RuntimeError(f"git fetch failed: {p.stderr[-500:]}")
    target = ctx.args.sha or "origin/main"
    p = git(repo, "rev-parse", "--verify", target + "^{commit}")
    if p.returncode != 0:
        raise RuntimeError(f"cannot resolve {target}: {p.stderr[-300:]}")
    sha = p.stdout.strip()
    p = git(repo, "checkout", "--quiet", "--detach", "--force", sha)
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


def run_metal(ctx: Ctx, sha: str) -> dict:
    rec: dict = {}
    th = ctx.thresholds
    read = lambda: collect_readings(with_wsl_host=False)  # noqa: E731
    ok, r, reasons = wait_idle_once(ctx, read, "metal pre-check")
    rec["idle_pre"] = r.to_json()
    if not ok:
        return {**rec, "status": "skipped", "reason": "busy (pre-check): " + " | ".join(reasons)}
    repo = local_repo(ctx)
    t0 = time.monotonic()
    rc = run_logged(ctx, ["./scripts/build.sh", "-j", "release"], ctx.run_dir / "metal-build.log",
                    cwd=repo, env=clean_env({}))
    rec["build_seconds"] = round(time.monotonic() - t0, 1)
    if rc != 0:
        return {**rec, "status": "error", "reason": f"build failed (exit {rc}), see metal-build.log"}
    ok, r, reasons = wait_idle(ctx, read, "metal re-check")
    rec["idle_post"] = r.to_json()
    if not ok:
        return {**rec, "status": "skipped", "reason": "busy (post-build re-check): " + " | ".join(reasons)}
    junit = ctx.run_dir / "metal-junit.xml"
    pyenv = {"LUMICE_BIN": str(repo / "build" / "cmake_install" / "static" / "Lumice")}
    if ctx.synthetic:
        write_override_plugin(ctx.run_dir)
        pyenv["PYTHONPATH"] = str(ctx.run_dir)
    t0 = time.monotonic()
    rc = run_logged(ctx, pytest_argv(sys.executable, "metal", str(junit), str(ctx.run_dir),
                                     ctx.args.threshold_override),
                    ctx.run_dir / "metal-pytest.log", cwd=repo, env=clean_env(pyenv),
                    timeout=ctx.args.pytest_timeout)
    rec["pytest_seconds"] = round(time.monotonic() - t0, 1)
    rec["idle_after_pytest"] = read().to_json()
    if ctx.args.dry_run:
        return {**rec, "status": "skipped", "reason": "dry-run"}
    junit_text = junit.read_text() if junit.is_file() else None
    status, reason = classify_pytest(None if rc == 124 else rc, junit_text, GATE_TESTS["metal"][1])
    log_text = (ctx.run_dir / "metal-pytest.log").read_text(errors="replace")
    return {**rec, "status": status, "reason": reason, "pytest_exit": rc, **extract_ratios(log_text)}


def wait_idle_once(ctx: Ctx, read: Callable[[], Readings], label: str):
    r = read()
    ok, reasons = idle_verdict(r, ctx.thresholds)
    ctx.log(f"{label}: {'idle' if ok else 'busy: ' + ' | '.join(reasons)}")
    return ok, r, reasons


# ---- cuda leg (remote) ------------------------------------------------------


def ssh_argv(ctx: Ctx, *remote: str) -> List[str]:
    return ["ssh", "-o", "BatchMode=yes", "-o", f"ConnectTimeout={ctx.args.ssh_connect_timeout}",
            ctx.args.remote, *remote]


def remote_readings(ctx: Ctx) -> Readings:
    """Run this very file's collect_readings on the remote via `python3 -`."""
    try:
        p = subprocess.run(ssh_argv(ctx, "python3", "-", "--collect-readings", "--wsl-host"),
                           input=Path(__file__).read_text(), capture_output=True, text=True, timeout=180)
        if p.returncode != 0:
            raise RuntimeError(f"exit {p.returncode}: {p.stderr[-300:]}")
        return Readings.from_json(json.loads(p.stdout.strip().splitlines()[-1]))
    except (RuntimeError, subprocess.TimeoutExpired, ValueError, IndexError) as e:
        return Readings(load1=float("nan"), physical_cores=1, errors=[f"remote readings failed: {e}"])


def remote_phase_script(ctx: Ctx, phase: str, sha: str, rdir: str) -> str:
    """A self-contained bash script that runs on the remote and leaves its result on remote disk."""
    base = ctx.args.remote_dir
    common = f"""set -u
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
        body = f"""
for v in $(env | sed -n 's/^\\(LUMICE_[A-Za-z0-9_]*\\)=.*/\\1/p'); do unset "$v"; done
LUMICE_HAS_CUDA=1 LUMICE_BIN="$REPO/build/cmake_install/static/Lumice" {plugin_env}\\
  timeout {int(ctx.args.pytest_timeout)} {argv_s} > "$OUT/pytest.log" 2>&1
echo $? > "$OUT/pytest.exit"
touch "$OUT/pytest.done"
"""
    return common + body


def remote_start_phase(ctx: Ctx, phase: str, sha: str, rdir: str) -> bool:
    script = remote_phase_script(ctx, phase, sha, rdir)
    out = f"{ctx.args.remote_dir}/results/{rdir}"
    remote_cmd = (f"mkdir -p {out} && cat > {out}/{phase}.sh && "
                  f"setsid nohup bash {out}/{phase}.sh > {out}/{phase}.nohup 2>&1 < /dev/null &")
    rc = run_logged(ctx, ssh_argv(ctx, remote_cmd), ctx.run_dir / "cuda-ssh.log",
                    timeout=120, stdin_text=script)
    return rc == 0


def remote_wait_phase(ctx: Ctx, phase: str, rdir: str, budget_s: float) -> Optional[int]:
    """Poll the remote done-marker with short, independent ssh sessions; a dropped
    session costs one poll, not the run. Returns the phase's exit code or None."""
    out = f"{ctx.args.remote_dir}/results/{rdir}"
    deadline = time.monotonic() + budget_s
    while True:
        if ctx.args.dry_run:
            return 0
        try:
            p = _run(ssh_argv(ctx, f"test -f {out}/{phase}.done && cat {out}/{phase}.exit"), timeout=60)
        except subprocess.TimeoutExpired:
            p = None
        if p is not None and p.returncode == 0 and p.stdout.strip():
            try:
                return int(p.stdout.strip())
            except ValueError:
                return None
        if time.monotonic() >= deadline:
            return None
        time.sleep(ctx.args.remote_poll)


def remote_fetch(ctx: Ctx, rdir: str) -> None:
    dst = ctx.run_dir / "remote"
    dst.mkdir(exist_ok=True)
    run_logged(ctx, ["rsync", "-az", f"{ctx.args.remote}:{ctx.args.remote_dir}/results/{rdir}/", f"{dst}/"],
               ctx.run_dir / "cuda-ssh.log", timeout=300)


def remote_sync_source(ctx: Ctx, sha: str) -> Optional[str]:
    """Push the commit into a persistent repo on the remote (only changed files get new
    mtimes, so the remote build stays incremental), and top up its CPM source cache."""
    rrepo = f"{ctx.args.remote_dir}/repo"
    rc = run_logged(ctx, ssh_argv(ctx, f"mkdir -p {rrepo} && cd {rrepo} && "
                                  f"(test -d .git || git init -q) && git config receive.denyCurrentBranch ignore"),
                    ctx.run_dir / "cuda-ssh.log", timeout=120)
    if rc != 0:
        return f"remote repo init failed (exit {rc})"
    rc = run_logged(ctx, ["git", "-C", str(local_repo(ctx)), "-c", "core.hooksPath=/dev/null", "push",
                          "--quiet", "--force", f"{ctx.args.remote}:{rrepo}", f"{sha}:refs/heads/gate-target"],
                    ctx.run_dir / "cuda-ssh.log", env=clean_env({"GIT_LFS_SKIP_PUSH": "1"}), timeout=1800)
    if rc != 0:
        return f"git push to remote failed (exit {rc})"
    cpm = Path(ctx.args.cpm_cache).expanduser()
    if cpm.is_dir():
        rc = run_logged(ctx, ["rsync", "-az", f"{cpm}/", f"{ctx.args.remote}:.cache/lumice-cpm/"],
                        ctx.run_dir / "cuda-ssh.log", timeout=3600)
        if rc != 0:
            return f"CPM cache rsync failed (exit {rc})"
    return None


def run_cuda(ctx: Ctx, sha: str, resume_rdir: Optional[str] = None) -> dict:
    rec: dict = {"remote": ctx.args.remote}
    if not ctx.args.dry_run:
        p = _run(ssh_argv(ctx, "true"), timeout=ctx.args.ssh_connect_timeout + 15)
        if p.returncode != 0:
            return {**rec, "status": "skipped", "reason": f"unreachable (ssh exit {p.returncode})"}
    rdir = resume_rdir or ctx.run_id
    rec["remote_run_dir"] = rdir
    if resume_rdir is None:
        ok, r, reasons = wait_idle_once(ctx, lambda: remote_readings(ctx), "cuda pre-check")
        rec["idle_pre"] = r.to_json()
        if not ok:
            return {**rec, "status": "skipped", "reason": "busy (pre-check): " + " | ".join(reasons)}
        err = remote_sync_source(ctx, sha)
        if err:
            return {**rec, "status": "error", "reason": err}
        t0 = time.monotonic()
        if not remote_start_phase(ctx, "build", sha, rdir):
            return {**rec, "status": "error", "reason": "could not start remote build"}
        brc = remote_wait_phase(ctx, "build", rdir, ctx.args.remote_build_timeout)
        rec["build_seconds"] = round(time.monotonic() - t0, 1)
        if brc != 0:
            remote_fetch(ctx, rdir)
            return {**rec, "status": "error",
                    "reason": f"remote build failed (exit {brc}), see remote/build.log"}
        ok, r, reasons = wait_idle(ctx, lambda: remote_readings(ctx), "cuda re-check")
        rec["idle_post"] = r.to_json()
        if not ok:
            return {**rec, "status": "skipped", "reason": "busy (post-build re-check): " + " | ".join(reasons)}
        if ctx.synthetic and not ctx.args.dry_run:
            out = f"{ctx.args.remote_dir}/results/{rdir}"
            run_logged(ctx, ssh_argv(ctx, f"cat > {out}/{OVERRIDE_PLUGIN_NAME}.py"),
                       ctx.run_dir / "cuda-ssh.log", timeout=60, stdin_text=OVERRIDE_PLUGIN_SOURCE)
        if not remote_start_phase(ctx, "pytest", sha, rdir):
            return {**rec, "status": "error", "reason": "could not start remote pytest"}
    t0 = time.monotonic()
    prc = remote_wait_phase(ctx, "pytest", rdir, ctx.args.pytest_timeout + 600)
    rec["pytest_wait_seconds"] = round(time.monotonic() - t0, 1)
    rec["idle_after_pytest"] = remote_readings(ctx).to_json()
    if ctx.args.dry_run:
        return {**rec, "status": "skipped", "reason": "dry-run"}
    remote_fetch(ctx, rdir)
    got = ctx.run_dir / "remote"
    junit = got / "junit.xml"
    exit_file = got / "pytest.exit"
    rc = int(exit_file.read_text().strip()) if exit_file.is_file() else prc
    if rc is None:
        return {**rec, "status": "error", "reason": "remote pytest result never appeared (lost run)"}
    status, reason = classify_pytest(None if rc == 124 else rc,
                                     junit.read_text() if junit.is_file() else None,
                                     GATE_TESTS["cuda"][1])
    log = got / "pytest.log"
    ratios = extract_ratios(log.read_text(errors="replace")) if log.is_file() else {}
    return {**rec, "status": status, "reason": reason, "pytest_exit": rc, **ratios}


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


def issue_body(leg: str, rec: dict, stale_since: Optional[str]) -> str:
    lines = [
        f"Leg: `{leg}` — role: {'Metal reference machine' if leg == 'metal' else 'CUDA reference machine (Linux)'}",
        f"Commit: `{rec.get('sha', '?')}`",
        f"Run: `{rec.get('run_id')}`" + ("  (synthetic: threshold overridden)" if rec.get("synthetic") else ""),
        f"Status: **{rec['status']}**" + (f" — {rec['reason']}" if rec.get("reason") else ""),
    ]
    if "ratio_a" in rec:
        lines.append(f"dual/single_a = {rec['ratio_a']:.3f}, dual/single_b = {rec['ratio_b']:.3f}"
                     f" (gate >= {rec.get('threshold', '?')}); dual/legacy = {rec.get('vs_legacy', '?')}x")
    if stale_since:
        lines.append(f"Last real measurement (pass or fail): {stale_since}")
    lines.append("")
    lines.append("Posted by `scripts/local_throughput_gate.py`; how to read it: "
                 "`doc/performance-testing.md`, \"Precise throughput gate (local schedule)\".")
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
    for a in decide_notifications(rec["status"], bool(rec.get("synthetic")), stale, opened, body):
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


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0],
                                formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    p.add_argument("--leg", choices=("metal", "cuda", "all"), default="all")
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
    ctx = Ctx(args, run_id, run_dir, log,
              IdleThresholds(args.load_per_core, args.host_cpu_percent, args.gpu_util_percent),
              synthetic=args.threshold_override is not None)
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
    try:
        if args.dry_run and not (local_repo(ctx) / ".git").is_dir():
            log("dry-run: gate clone absent; would clone and check out origin/main")
        else:
            sha = prepare_checkout(ctx)
    except RuntimeError as e:
        source_error = str(e)
    log(f"commit under test: {sha}")

    worst = 0
    for leg in legs:
        t0 = time.monotonic()
        if source_error:
            rec = {"status": "error", "reason": f"source: {source_error}"}
        elif leg == "metal":
            if platform.system() != "Darwin":
                rec = {"status": "error", "reason": "metal leg needs macOS"}
            else:
                rec = run_metal(ctx, sha)
        else:
            rec = run_cuda(ctx, sha, resume_rdir=args.collect_remote)
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
        save_state(state_dir, state)
    prune_run_dirs(runs_dir, KEEP_RUN_DIRS)
    return worst


if __name__ == "__main__":
    sys.exit(main())
