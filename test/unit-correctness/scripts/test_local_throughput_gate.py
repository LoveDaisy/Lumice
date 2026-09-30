"""Regression net for the pure-logic half of `scripts/local_throughput_gate.py`.

The gate runs unattended, so every rule here fails in a quiet direction if it
breaks: a skipped test read as a pass, an infrastructure error read as a
throughput regression, a second issue for the same failure, a skip that never
turns into a staleness alarm, or the run calling its own build "busy". Each
rule is pinned by a red case next to its green one.
"""
from __future__ import annotations

import datetime as dt
import sys
from pathlib import Path

import pytest

SCRIPTS = Path(__file__).resolve().parents[3] / "scripts"
sys.path.insert(0, str(SCRIPTS))

import local_throughput_gate as g  # noqa: E402

TEST = "test_metal_dual_renderer_throughput"


def junit(outcome: str, name: str = TEST, message: str = "boom") -> str:
    inner = "" if outcome == "passed" else f'<{outcome} message="{message}"/>'
    return (f'<?xml version="1.0"?><testsuites><testsuite name="pytest">'
            f'<testcase classname="x" name="{name}">{inner}</testcase></testsuite></testsuites>')


# --- pytest result -> state ---------------------------------------------------

@pytest.mark.parametrize("exit_code,xml,expected", [
    (0, junit("passed"), "pass"),
    (1, junit("failure", message="dual-renderer throughput 0.80x"), "fail"),
    # exit 0 with the target skipped: the silent-green shape (missing CUDA env, binary, marker)
    (0, junit("skipped"), "error"),
    # exit 0 but the target is not in the report at all
    (0, junit("passed", name="some_other_test"), "error"),
    # a raised exception (timeout, missing binary) is <error>, not a gate verdict
    (1, junit("error"), "error"),
    (2, junit("failure"), "error"),     # interrupted
    (5, None, "error"),                 # no tests collected, no junit
    (0, None, "error"),
    (None, junit("passed"), "error"),   # killed by our timeout
    (0, "<not xml", "error"),
])
def test_classify_pytest(exit_code, xml, expected):
    status, reason = g.classify_pytest(exit_code, xml, TEST)
    assert status == expected, reason
    if expected != "pass":
        assert reason


# --- idle predicate ---------------------------------------------------------

TH = g.IdleThresholds(load_per_core=0.5, host_cpu_percent=25, gpu_util_percent=10)


def test_idle_load_boundary():
    assert g.idle_verdict(g.Readings(load1=5.99, physical_cores=12), TH)[0]
    assert not g.idle_verdict(g.Readings(load1=6.0, physical_cores=12), TH)[0]


@pytest.mark.parametrize("field,value", [
    ("busy", ["123 ninja: ninja -C build"]),
    ("host_busy", ["Lumice.exe: Lumice.exe benchmark"]),
    ("host_cpu_percent", 25.0),
    ("gpu_util_percent", 10.0),
    ("errors", ["remote readings failed"]),
])
def test_each_busy_signal_alone_makes_it_busy(field, value):
    r = g.Readings(load1=0.0, physical_cores=16)
    assert g.idle_verdict(r, TH)[0]
    setattr(r, field, value)
    ok, reasons = g.idle_verdict(r, TH)
    assert not ok and reasons


def test_unreadable_remote_is_busy_not_idle():
    r = g.Readings(load1=float("nan"), physical_cores=1, errors=["ssh died"])
    assert not g.idle_verdict(r, TH)[0]


def P(pid, ppid, exe, args=None):
    return g.Proc(pid, ppid, exe, args or exe)


def test_own_build_and_ancestors_are_not_busy_but_siblings_are():
    procs = [
        P(1, 0, "launchd"),
        P(10, 1, "python", "python scrum_drive.py"),      # our driver: ancestor
        P(20, 10, "python", "python local_throughput_gate.py"),  # self
        P(30, 20, "ninja", "ninja -C build"),              # our build
        P(31, 30, "cc1plus"),                              # our compiler
        P(40, 10, "python", "python task_drive.py"),       # sibling under the same driver
        P(50, 1, "ninja", "ninja -C other"),               # unrelated build
    ]
    busy = g.busy_processes(procs, self_pid=20)
    pids = {int(b.split()[0]) for b in busy}
    assert pids == {40, 50}


@pytest.mark.parametrize("exe,args,busy", [
    ("python3.11", "python3.11 -m pytest test/performance", True),
    ("python", "/usr/bin/python /x/drive/runners/task_drive.py --task t", True),
    ("pytest", "pytest -q", True),
    # a shell whose command TEXT mentions pytest is not running it (measured false positive)
    ("zsh", "/bin/zsh -c python -m pytest --version && echo", False),
    ("python", "python some_server.py --flag pytest", False),
])
def test_python_workload_is_read_from_argv_of_python_processes_only(exe, args, busy):
    assert bool(g.busy_processes([P(99, 1, exe, args)], self_pid=5)) is busy


def test_host_names_match_exactly_not_by_substring():
    busy = g.host_busy_processes([("atieclxx.exe", ""), ("cl.exe", "cl /c x.cpp"),
                                  ("python.exe", "python -m pytest test"), ("python.exe", "python idle.py")])
    assert [b.split(":")[0] for b in busy] == ["cl.exe", "python.exe"]


# --- notification decision table --------------------------------------------

def ops(actions):
    return [(a.op, a.kind) for a in actions]


@pytest.mark.parametrize("status,synthetic,stale,opened,expected", [
    ("fail", False, False, set(), [("create", "fail")]),
    ("fail", False, False, {"fail"}, [("comment", "fail")]),          # no second issue
    ("fail", False, False, {"stale"}, [("create", "fail"), ("close", "stale")]),
    ("fail", True, False, {"stale"}, [("create", "fail")]),           # synthetic: stale stays
    ("error", False, False, set(), [("create", "error")]),
    ("error", False, False, {"error"}, []),
    ("pass", False, False, {"fail", "error", "stale"},
     [("close", "fail"), ("close", "error"), ("close", "stale")]),
    ("pass", True, False, {"fail"}, []),                              # synthetic pass closes nothing
    ("skipped", False, False, {"fail"}, []),
    ("skipped", False, True, set(), [("create", "stale")]),
    ("skipped", False, True, {"stale"}, []),
    ("skipped", False, True, {"error"}, []),                          # the open error explains it
])
def test_decide_notifications(status, synthetic, stale, opened, expected):
    assert ops(g.decide_notifications(status, synthetic, stale, opened, "b")) == expected


def test_only_real_measurements_refresh_the_staleness_clock():
    assert g.refreshes_last_real_run("pass", False)
    assert g.refreshes_last_real_run("fail", False)
    assert not g.refreshes_last_real_run("fail", True)
    assert not g.refreshes_last_real_run("skipped", False)
    assert not g.refreshes_last_real_run("error", False)


def test_stale_boundary():
    t0 = dt.datetime(2026, 10, 1, tzinfo=dt.timezone.utc)
    assert not g.is_stale(t0, t0 + dt.timedelta(days=2, hours=23), 3)
    assert g.is_stale(t0, t0 + dt.timedelta(days=3), 3)


def test_issue_titles_are_distinct_per_namespace():
    assert g.issue_title("cuda", "fail", "real") == "[throughput-gate] cuda fail"
    assert g.issue_title("cuda", "fail", "test") == "[TEST][throughput-gate] cuda fail"


# --- misc -----------------------------------------------------------------

def test_extract_ratios_reads_the_tests_own_summary_line():
    log = ("x.py [metal-dual-throughput] dual: median=27.85M rays/s CoV=0.104 min=21.7M max=30.4M n=21\n"
           "[metal-dual-throughput] single_a: median=29.30M rays/s CoV=0.113 min=21.3M max=32.2M n=21\n"
           "[metal-dual-throughput] single_b: median=30.67M rays/s CoV=0.105 min=22.8M max=31.8M n=21\n"
           "[metal-dual-throughput] dual/single_a=0.950 dual/single_b=0.908 (gate >= 0.85); "
           "legacy dual median=6.70M rays/s, dual metal/legacy=4.16x (sanity >= 2.0)\n")
    assert g.extract_ratios(log) == {
        "ratio_a": 0.95, "ratio_b": 0.908, "vs_legacy": 4.16, "threshold": 0.85,
        "median_mrps_dual": 27.85, "median_mrps_single_a": 29.30, "median_mrps_single_b": 30.67,
        "median_mrps_legacy": 6.70,
    }
    assert g.extract_ratios("nothing here") == {}


def test_threshold_override_cannot_touch_real_issues():
    with pytest.raises(SystemExit):
        g.parse_args(["--threshold-override", "99"])
    a = g.parse_args(["--threshold-override", "99", "--issue-namespace", "test"])
    assert a.threshold_override == 99


def test_override_plugin_rebinds_the_shared_threshold(tmp_path, monkeypatch):
    """The plugin text is the only thing standing between a drill and a real gate."""
    (tmp_path / f"{g.OVERRIDE_PLUGIN_NAME}.py").write_text(g.OVERRIDE_PLUGIN_SOURCE)
    monkeypatch.syspath_prepend(str(tmp_path))
    import importlib

    plugin = importlib.import_module(g.OVERRIDE_PLUGIN_NAME)
    gate = importlib.import_module("test.e2e._multi_renderer_throughput")
    before = gate.T_DUAL_VS_SINGLE

    class Cfg:
        def getoption(self, name):
            return 99.0

    try:
        plugin.pytest_configure(Cfg())
        assert gate.T_DUAL_VS_SINGLE == 99.0
    finally:
        gate.T_DUAL_VS_SINGLE = before


# --- public issue text / unattended-run robustness ---------------------------------

def test_issue_body_carries_no_free_text_from_the_reason():
    rec = {"status": "skipped", "run_id": "r", "sha": "abc",
           "reason": "busy (pre-check): busy processes: 123 python: /Users/someone/work/x.py"}
    body = g.issue_body("metal", rec, None)
    assert "/Users/" not in body and "busy" in body


def test_public_reason_unknown_text_is_not_published():
    assert g.public_reason("git clone failed: fatal: /home/u/secret") == "unspecified"


def test_corrupt_state_is_moved_aside_not_fatal(tmp_path):
    now = dt.datetime(2026, 10, 1, tzinfo=dt.timezone.utc)
    (tmp_path / "state.json").write_text("{not json")
    st = g.load_state(tmp_path, now)
    assert st["legs"] == {} and st["pending_notifications"] == []
    assert any(p.name.startswith("state.json.corrupt-") for p in tmp_path.iterdir())
