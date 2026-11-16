"""Regression net for the pure-logic half of `scripts/local_throughput_gate.py`.

The gate runs unattended, so every rule here fails in a quiet direction if it
breaks: a skipped test read as a pass, an infrastructure error read as a
throughput regression, a second issue for the same failure, a skip that never
turns into a staleness alarm, or the run calling its own build "busy". Each
rule is pinned by a red case next to its green one.
"""
from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import subprocess
import sys
import time
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


# --- busy-skip constructor (the retry loop's trigger) -------------------------

def test_busy_skip_constructs_the_exact_legacy_shape():
    """Reason text byte-identical to the pre-retry shape (the public issue
    categories parse it), plus the explicit marker the retry loop keys on."""
    r = g.busy_skip("pre-check", ["load 9.00/8 cores = 1.125 >= 0.5", "busy processes: 123 ninja: ninja -C build"])
    assert r == {"status": "skipped",
                 "reason": "busy (pre-check): load 9.00/8 cores = 1.125 >= 0.5"
                           " | busy processes: 123 ninja: ninja -C build",
                 "env_rejected": True}
    r2 = g.busy_skip("post-build re-check", ["GPU util 40% >= 10%"])
    assert r2["reason"] == "busy (post-build re-check): GPU util 40% >= 10%"
    assert r2["env_rejected"] is True and r2["status"] == "skipped"


def test_is_env_busy_skip_reads_only_the_marker():
    assert g.is_env_busy_skip(g.busy_skip("pre-check", ["x"]))
    # the legacy shape without the marker (a hand-built record) is not a trigger
    assert not g.is_env_busy_skip({"status": "skipped", "reason": "busy (pre-check): x"})
    # cuda's unreachable and the run-level locked skip carry no marker
    assert not g.is_env_busy_skip({"status": "skipped", "reason": "unreachable (ssh exit 255)"})
    assert not g.is_env_busy_skip({"status": "skipped", "reason": "locked"})
    assert not g.is_env_busy_skip({"status": "fail", "reason": "functional red: some_case"})
    assert not g.is_env_busy_skip({})


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
    ("pass", False, False, {"sigill"}, [("close", "sigill")]),        # sigill joins the recovery sweep
    ("pass", True, False, {"fail"}, []),                              # synthetic pass closes nothing
    ("skipped", False, False, {"fail"}, []),
    ("skipped", False, True, set(), [("create", "stale")]),
    ("skipped", False, True, {"stale"}, []),
    ("skipped", False, True, {"error"}, []),                          # the open error explains it
    ("inconclusive", False, False, set(), []),                        # environment had no verdict: no issue
    ("inconclusive", False, False, {"fail"}, []),                     # nor a comment on an open fail
    ("inconclusive", False, True, set(), [("create", "stale")]),      # but no early return: stale row still fires
    ("inconclusive", False, True, {"stale"}, []),
])
def test_decide_notifications(status, synthetic, stale, opened, expected):
    assert ops(g.decide_notifications(status, synthetic, stale, opened, "b")) == expected


@pytest.mark.parametrize("opened,expected", [
    (set(), [("create", "sigill")]),
    ({"sigill"}, [("comment", "sigill")]),
    ({"fail"}, [("create", "sigill")]),    # a sigill sample is not a fail-thread comment
    ({"stale"}, [("create", "sigill"), ("close", "stale")]),
])
def test_decide_notifications_sigill_fail_uses_the_sigill_thread(opened, expected):
    assert ops(g.decide_notifications("fail", False, "stale" in opened, opened, "b",
                                      sigill=True)) == expected


def test_only_real_measurements_refresh_the_staleness_clock():
    assert g.refreshes_last_real_run("pass", False)
    assert g.refreshes_last_real_run("fail", False)
    assert not g.refreshes_last_real_run("fail", True)
    assert not g.refreshes_last_real_run("skipped", False)
    assert not g.refreshes_last_real_run("error", False)
    assert not g.refreshes_last_real_run("inconclusive", False)   # ran, but measured the environment


def test_stale_boundary():
    t0 = dt.datetime(2026, 10, 1, tzinfo=dt.timezone.utc)
    assert not g.is_stale(t0, t0 + dt.timedelta(days=2, hours=23), 3)
    assert g.is_stale(t0, t0 + dt.timedelta(days=3), 3)


def test_issue_titles_are_distinct_per_namespace():
    assert g.issue_title("cuda", "fail", "real") == "[throughput-gate] cuda fail"
    assert g.issue_title("cuda", "fail", "test") == "[TEST][throughput-gate] cuda fail"


# --- gui real-timing pool classifier ------------------------------------------
# Evidence shapes are pinned to real gui_test output: failing checks log as
# `[NNNN] Error <file>:<line> '<expression>'` into the case's junit <failure>
# text; the frozen-map value line is Info-level and precedes the assertions.

BUDGET_EXPR = "p95 <= 33.4 && max_latency <= 100.0 && max_observe <= 8.0"
FROZEN_MAP = "maximum_frozen_map_input_to_present"
TIMING_LINE = ("[0166] [frozen-map-timing] frames=124 p95=29.0829 ms max=30.2127 ms "
               "observation-upper=10.013 ms")


def gui_xml(*cases):
    """(name, status, failure_text) triples -> the engine exporter's junit shape.
    Failure text is XML-escaped the way the exporter's ImStrXmlEscape does."""
    def esc(s):
        return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    rows = []
    for name, status, ftext in cases:
        inner = f"<failure type=\"error\">{esc(ftext)}</failure>" if status == "Error" \
            else "<skipped message=\"Skipped\" />"
        rows.append(f'<testcase name="{name}" classname="perf_test" status="{status}" '
                    f'time="1.0">{inner}</testcase>')
    return ('<?xml version="1.0"?><testsuites><testsuite name="tests" tests="%d">'
            '%s</testsuite></testsuites>' % (len(cases), "".join(rows)))


def run_pool(rc, xml, log, **kw):
    return g.classify_gui_pool(rc, xml, log, **kw)


def test_gui_pool_pass_and_timing_decoupled_from_the_verdict():
    log = f"[0000] Test: 'perf_test' '{FROZEN_MAP}'..\n{TIMING_LINE}\n[0170] Success.\n"
    v = run_pool(0, gui_xml((FROZEN_MAP, "Success", "")), log)
    assert v.status == "pass" and v.reason == ""
    assert v.timing == {"timing_frames": 124, "timing_p95_ms": 29.0829,
                        "timing_max_ms": 30.2127, "timing_observation_max_ms": 10.013}
    # No value line -> no fields, nothing fabricated.
    v2 = run_pool(0, gui_xml(("some_case", "Success", "")), "[0000] Test: 'c' 'some_case'..\n[0001] Success.\n")
    assert v2.status == "pass" and v2.timing is None


def test_gui_pool_functional_red_names_the_case():
    log = ("[0000] Test: 'raypath_analysis' 'refused_class_blockades_nothing'..\n"
           "[0689] Error test_raypath_analysis_panel.cpp:6073 "
           "'report_text.find(\"Sky point answer\") != std::string::npos'\n"
           "[0689] refused_class_blockades_nothing test failed.\n")
    xml = gui_xml(("refused_class_blockades_nothing", "Error",
                   "[0689] Error test_raypath_analysis_panel.cpp:6073 "
                   "'report_text.find(\"Sky point answer\") != std::string::npos'"))
    v = run_pool(1, xml, log)
    assert v.status == "fail"
    assert "functional red" in v.reason and "refused_class_blockades_nothing" in v.reason
    assert v.failing_case == "refused_class_blockades_nothing"
    assert v.sigill is None


def test_gui_pool_budget_red_is_inconclusive_and_issues_nothing():
    """The frozen-map ms budget red on a personal machine is an environment
    signal: inconclusive, and (pinned in the notification table) no issue."""
    ftext = f"{TIMING_LINE}\n[0166] OK test_frozen_map_view.cpp:1307 'latencies.size() >= 120'\n" \
            f"[0166] Error test_frozen_map_view.cpp:1308 '{BUDGET_EXPR}'\n" \
            "[0170] Error test_frozen_map_view.cpp:1358 'MaximumTimingWitness(ctx)'"
    v = run_pool(1, gui_xml((FROZEN_MAP, "Error", ftext)), f"marker\n{TIMING_LINE}\n")
    assert v.status == "inconclusive"
    assert "budget red" in v.reason and FROZEN_MAP in v.reason
    assert v.timing is not None          # values still recorded for the trend
    assert v.sigill is None and v.failing_case is None


def test_gui_pool_structural_red_is_functional_not_budget():
    """frames >= 120 on the same case is a functional contract, not the budget:
    a pool-wide exemption of the case by name is exactly what must not happen."""
    ftext = (f"{TIMING_LINE}\n[0166] Error test_frozen_map_view.cpp:1307 'latencies.size() >= 120'\n"
             "[0170] Error test_frozen_map_view.cpp:1358 'MaximumTimingWitness(ctx)'")
    v = run_pool(1, gui_xml((FROZEN_MAP, "Error", ftext)), TIMING_LINE)
    assert v.status == "fail" and v.failing_case == FROZEN_MAP


def test_gui_pool_structural_and_budget_together_stay_functional():
    ftext = (f"[0166] Error test_frozen_map_view.cpp:1307 'latencies.size() >= 120'\n"
             f"[0166] Error test_frozen_map_view.cpp:1308 '{BUDGET_EXPR}'\n"
             "[0170] Error test_frozen_map_view.cpp:1358 'MaximumTimingWitness(ctx)'")
    v = run_pool(1, gui_xml((FROZEN_MAP, "Error", ftext)), "")
    assert v.status == "fail"


def test_gui_pool_unattributable_failure_is_functional_by_default():
    """The adoption breach returns false without an IM_CHECK of its own: only
    the case's outer wrapper fires. Fail-safe default: functional red."""
    ftext = "[0170] Error test_frozen_map_view.cpp:1358 'MaximumTimingWitness(ctx)'"
    v = run_pool(1, gui_xml((FROZEN_MAP, "Error", ftext)), "")
    assert v.status == "fail" and v.failing_case == FROZEN_MAP


def test_gui_pool_timeout_telemetry_is_inconclusive():
    for marker in ("[drive-gate] TIMEOUT after 15.0s (budget 15s, 3 polls, avg 5000 ms/poll)",
                   "[Watchdog] Running time for 'some_case' is >60 seconds, aborting.",
                   "[Watchdog] Emergency process exit as the test didn't return."):
        ftext = f"[0001] Info {marker}\n[0842] Error test_raypath_analysis_panel.cpp:16687 'SomeHelper(ctx)'"
        v = run_pool(1, gui_xml(("some_case", "Error", ftext)), f"marker\n{marker}\n")
        assert v.status == "inconclusive" and "timeout" in v.reason, marker


def test_gui_pool_watchdog_warning_does_not_demote_a_functional_red():
    """The engine logs '>N seconds, may be excessive.' routinely before a slow
    case fails a real assertion (measured in the real pool log); the warning is
    not an abort and must leave the failure a functional red."""
    ftext = ("[0001] [Watchdog] Running time for 'some_case' is >30 seconds, may be excessive.\n"
             "[0842] Error test_raypath_analysis_panel.cpp:16687 'SomeHelper(ctx)'")
    v = run_pool(1, gui_xml(("some_case", "Error", ftext)), "")
    assert v.status == "fail" and v.failing_case == "some_case"
    assert "timeout" not in v.reason


def test_gui_pool_budget_fragment_in_another_case_stays_functional():
    """The ms-budget exemption names its case: the same expression fragment
    failing in any other case is a functional red, not an environment verdict."""
    ftext = "[0010] Error other.cpp:10 'p95 <= 33.4 && max_latency <= 100.0 && max_observe <= 8.0'"
    v = run_pool(1, gui_xml(("some_other_case", "Error", ftext)), "")
    assert v.status == "fail" and v.failing_case == "some_other_case"


def test_gui_pool_functional_red_has_priority_over_timeout():
    """A real defect signal is not downgraded by an unrelated environment red."""
    xml = gui_xml(
        ("good_case", "Error", "[0010] Error a.cpp:10 'real_contract_broken'"),
        ("hang_case", "Error", "[drive-gate] TIMEOUT after 15.0s (budget 15s)\n"
                               "[0842] Error b.cpp:20 'SomeHelper(ctx)'"))
    v = run_pool(1, xml, "")
    assert v.status == "fail"
    assert "good_case" in v.reason and "also timeout: hang_case" in v.reason
    assert v.failing_case == "good_case"


def test_gui_pool_sigill_records_the_sample_event():
    log = ("[0000] Test: 'shell_chrome' 'workspace_panorama_publishes_after_a_render'..\n"
           "[0044] Success.\n<truncated by the crash>")
    load = {"load_snapshot": {"load1": 0.4, "physical_cores": 12}, "sampled_at": "2026-11-16T00:00:00+00:00"}
    for rc in (-4, 132):
        v = run_pool(rc, None, log)
        assert v.status == "fail" and f"exit {rc}" in v.reason
        # the event exists even without a load snapshot; the IO layer fills those two fields
        assert v.sigill["exit_code"] == rc
        assert v.sigill["last_case_marker"] == "workspace_panorama_publishes_after_a_render"
        assert "load_snapshot" not in v.sigill and "sampled_at" not in v.sigill
    v = run_pool(-4, None, log, sigill_load=load)
    assert v.sigill["exit_code"] == -4
    assert v.sigill["last_case_marker"] == "workspace_panorama_publishes_after_a_render"
    assert v.sigill["known_signature_boundary"] is True
    assert v.sigill["log_tail"].endswith("<truncated by the crash>")
    assert v.sigill["load_snapshot"] == load["load_snapshot"]      # field completeness
    assert v.sigill["sampled_at"] == load["sampled_at"]
    assert v.timing is None               # a dead process leaves no timing line behind


def test_gui_pool_sigill_unknown_boundary_is_a_new_sample():
    log = "[0000] Test: 'perf_test' 'maximum_frozen_map_input_to_present'..\n[0166] [frozen-map-timing] ..."
    v = run_pool(-4, None, log)
    assert v.sigill["last_case_marker"] == FROZEN_MAP
    assert v.sigill["known_signature_boundary"] is False


def test_gui_pool_silent_green_and_infrastructure_shapes():
    ok_xml = gui_xml(("only_case", "Success", ""))
    assert run_pool(0, ok_xml, "m\n[0001] Success.\n").status == "pass"
    assert run_pool(1, ok_xml, "").status == "error"                      # exit contradicts junit
    red_xml = gui_xml(("only_case", "Error", "[0001] Error a.cpp:1 'x'"))
    assert run_pool(0, red_xml, "").status == "error"
    assert run_pool(1, gui_xml(("a", "Unknown", ""), ("b", "Unknown", "")), "").status == "error"
    assert run_pool(1, None, "").status == "error"
    assert run_pool(1, "<not xml", "").status == "error"
    assert run_pool(None, ok_xml, "").status == "error"
    assert run_pool(2, red_xml, "").status == "error"
    assert run_pool(124, red_xml, "").status == "inconclusive"            # gate-level timeout


def test_gui_pool_tty_span_fallback_when_failure_text_is_empty():
    """No junit failure text (defensive): the case's last TTY span attributes it,
    the last attempt winning over the re-run's earlier one."""
    log = ("[0000] Test: 'c' 'some_case'..\n[0010] Error a.cpp:1 'contract_x_broken'\n"
           "[0000] Test: 'c' 'other_case'..\n[0001] Success.\n")
    v = run_pool(1, gui_xml(("some_case", "Error", "")), log)
    assert v.status == "fail" and v.failing_case == "some_case"


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


# --- subprocess capture boundary (decode robustness) ---------------------------
# The gate runs unattended at 03:30; one stray non-UTF-8 byte in any child's
# output (another process's args in a `ps` row) must not raise and lose the run.
# Both tests drive the real _run entry with a real child, not a mocked parameter.

def test_run_replaces_invalid_utf8_bytes_from_a_real_subprocess():
    child = ("import sys; sys.stdout.buffer.write(b'ok \\xffbad\\n'); "
             "sys.stderr.buffer.write(b'err \\xfetail\\n'); sys.exit(3)")
    p = g._run([sys.executable, "-c", child])
    assert p.returncode == 3
    assert p.stdout == "ok " + "�" + "bad\n"
    assert p.stderr == "err " + "�" + "tail\n"


def test_run_keeps_valid_text_and_return_code_untouched():
    p = g._run([sys.executable, "-c", "print('plain')"])
    assert p.returncode == 0
    assert p.stdout == "plain\n" and p.stderr == ""


# --- remote readings capture (cuda leg) ----------------------------------------

def remote_ctx():
    # ssh_argv reads ctx.args.remote / ctx.args.ssh_connect_timeout — a bare Ctx shape.
    return argparse.Namespace(args=argparse.Namespace(remote="remote-host", ssh_connect_timeout=5))


def test_remote_readings_reuses_the_common_capture_entry(monkeypatch):
    """The cuda leg's readings ride the same tolerant capture boundary as every
    other child: one entry, one decode rule — no second decoder beside it."""
    captured = {}

    def fake_run(cmd, **kw):
        captured["cmd"], captured["kw"] = cmd, kw
        payload = json.dumps(g.Readings(load1=0.1, physical_cores=8).to_json())
        return subprocess.CompletedProcess(cmd, 0, stdout=payload + "\n", stderr="")

    monkeypatch.setattr(g, "_run", fake_run)
    r = g.remote_readings(remote_ctx())
    assert (r.load1, r.physical_cores) == (0.1, 8)
    assert captured["cmd"][0] == "ssh" and "remote-host" in captured["cmd"]
    assert "--collect-readings" in captured["cmd"]
    assert captured["kw"]["timeout"] == 180
    # the gate still feeds itself to the remote interpreter on stdin
    assert captured["kw"]["input"] == Path(g.__file__).read_text()


def test_remote_readings_failures_degrade_to_an_error_reading(monkeypatch):
    """Non-zero ssh exit and unparseable output keep the old contract: an error
    reading (busy by verdict), not a raised exception."""
    monkeypatch.setattr(g, "_run",
                        lambda c, **kw: subprocess.CompletedProcess(c, 255, stdout="", stderr="no route"))
    r = g.remote_readings(remote_ctx())
    assert r.errors and "remote readings failed: exit 255" in r.errors[0]

    monkeypatch.setattr(g, "_run",
                        lambda c, **kw: subprocess.CompletedProcess(c, 0, stdout="not json\n", stderr=""))
    r = g.remote_readings(remote_ctx())
    assert r.errors and "remote readings failed" in r.errors[0]


# --- metal build-tools pre-check -----------------------------------------------
# A missing build tool is a deterministic configuration error: it must surface as
# an error before anything expensive (readings, build, cool-down, pytest), with a
# public category an issue reader can diagnose from — tool names stay local.

def test_missing_build_tools_reason_names_each_missing_tool_in_fixed_order():
    assert g.missing_build_tools_reason({"cmake": "/opt/hb/bin/cmake", "ninja": "/opt/hb/bin/ninja"}) is None
    assert g.missing_build_tools_reason({"cmake": None, "ninja": "/opt/hb/bin/ninja"}) == \
        "build tools missing from PATH: cmake"
    assert g.missing_build_tools_reason({"cmake": "/opt/hb/bin/cmake", "ninja": None}) == \
        "build tools missing from PATH: ninja"
    assert g.missing_build_tools_reason({"cmake": None, "ninja": None}) == \
        "build tools missing from PATH: cmake ninja"


def test_metal_build_tools_found_resolves_from_path(monkeypatch):
    """The IO seam queries every required tool through shutil.which on the
    inherited PATH — the same PATH the build will use."""
    monkeypatch.setattr(g.shutil, "which", lambda t: {"cmake": "/opt/hb/bin/cmake"}.get(t))
    assert g.metal_build_tools_found() == {"cmake": "/opt/hb/bin/cmake", "ninja": None}


def make_ctx(tmp_path):
    """A minimal Ctx for leg tests: a quiet machine lives behind the doubles.
    Carries the full argparse surface the three legs may read (retry params
    included) so any leg can run under it; no deadline is set, so the
    deadline guards stay inert."""
    args = argparse.Namespace(dry_run=False, cooldown_timeout=0.0, cooldown_poll=0.0,
                              threshold_override=None, pytest_timeout=10,
                              idle_retry_backoff_seconds=3600.0, idle_retry_max_backoffs=2,
                              remote="remote-host", remote_dir="gate", ssh_connect_timeout=5,
                              remote_build_timeout=99.0, remote_poll=0.0,
                              cpm_cache=str(tmp_path / "cpm"),
                              work_dir=str(tmp_path / "work"))
    return g.Ctx(args=args, run_id="t", run_dir=tmp_path, log=lambda msg: None,
                 thresholds=g.IdleThresholds(), synthetic=False)


def test_metal_fails_fast_before_any_expensive_stage_when_tools_missing(tmp_path, monkeypatch):
    ctx = make_ctx(tmp_path)
    calls = []
    monkeypatch.setattr(g, "metal_build_tools_found",
                        lambda: {"cmake": None, "ninja": "/opt/hb/bin/ninja"})
    monkeypatch.setattr(g, "collect_readings", lambda with_wsl_host: calls.append("readings"))
    monkeypatch.setattr(g, "run_logged", lambda *a, **kw: calls.append(tuple(a)))
    rec = g.run_metal(ctx, "sha")
    assert rec["status"] == "error"
    assert rec["reason"] == "build tools missing from PATH: cmake"
    assert calls == []                       # no readings, no build, no pytest
    assert "idle_pre" not in rec             # nothing was measured, nothing is faked
    assert not (tmp_path / "metal-build.log").exists()


def test_metal_with_tools_present_reaches_build_and_pytest_boundaries(tmp_path, monkeypatch):
    """Tools present: the leg runs its real sequence to both expensive boundaries,
    where the doubles stop it — no build or test workload is ever executed."""
    ctx = make_ctx(tmp_path)
    calls = []
    monkeypatch.setattr(g, "metal_build_tools_found",
                        lambda: {"cmake": "/opt/hb/bin/cmake", "ninja": "/opt/hb/bin/ninja"})
    monkeypatch.setattr(g, "collect_readings",
                        lambda with_wsl_host: g.Readings(load1=0.0, physical_cores=8))

    def fake_run_logged(ctx_, cmd, log_path, **kw):
        calls.append(list(cmd))
        Path(log_path).touch()               # run_logged always creates the log file
        return 0

    monkeypatch.setattr(g, "run_logged", fake_run_logged)
    rec = g.run_metal(ctx, "sha")
    build_calls = [c for c in calls if c[0] == "./scripts/build.sh"]
    pytest_calls = [c for c in calls if c[0] != "./scripts/build.sh"]
    assert len(build_calls) == 1 and build_calls[0][1:3] == ["-j", "release"]
    assert len(pytest_calls) == 1 and pytest_calls[0][1:3] == ["-m", "pytest"]
    assert any(a.startswith("--junitxml=") and str(tmp_path) in a for a in pytest_calls[0])
    assert "idle_pre" in rec and "idle_post" in rec and "idle_after_pytest" in rec
    # the doubles stop at the boundaries, so this is no measurement — and never a pass
    assert rec["status"] == "error" and "no-junit" in rec["reason"]
    assert (tmp_path / "metal-build.log").exists() and (tmp_path / "metal-pytest.log").exists()


def test_missing_tools_error_publishes_its_category_but_not_the_details():
    rec = {"status": "error", "run_id": "r", "sha": "abc",
           "reason": "build tools missing from PATH: cmake"}
    assert g.public_reason(rec["reason"]) == "build tools missing from PATH"
    body = g.issue_body("metal", rec, None)
    assert "build tools missing from PATH" in body
    # which tool is missing stays in the local record; the public body names no tool or path
    assert "cmake" not in body and "/Users/" not in body


# --- gui real-timing leg public face -------------------------------------------

@pytest.mark.parametrize("reason,category", [
    ("functional red: some_case", "functional red"),
    ("timeout: some_case ([drive-gate] TIMEOUT)", "timeout"),
    ("budget red: maximum_frozen_map_input_to_present", "budget red"),
    ("sigill: exit -4 (sample event)", "sigill"),
    ("gate-timeout (budget exhausted)", "gate-timeout"),
    ("gui-test junit mismatch (exit 0, junit reports errors)", "gui-test junit mismatch"),
    ("gui-real-timing leg needs macOS", "gui-real-timing leg needs macOS"),
])
def test_gui_reason_categories_are_public(reason, category):
    assert g.public_reason(reason) == category


def test_gui_issue_body_names_role_case_and_sigill_but_no_evidence_text():
    rec = {"status": "fail", "run_id": "r", "sha": "abc",
           "reason": "functional red: some_case",
           "failing_case": "some_case",
           "sigill_event": {"exit_code": -4, "last_case_marker": "some_case",
                            "log_tail": "crash /Users/someone/secret/x", "load_snapshot": {"load1": 9.9}}}
    body = g.issue_body("gui-real-timing", rec, None)
    assert "GUI real-timing pool" in body
    assert "Failing case: `some_case`" in body
    assert "exit `-4`" in body and "last case marker: `some_case`" in body
    # evidence free text and the load snapshot stay in the local record
    assert "/Users/" not in body and "load1" not in body and "log_tail" not in body


def test_gui_issue_body_is_quiet_when_the_record_has_no_case():
    rec = {"status": "pass", "run_id": "r", "sha": "abc", "reason": ""}
    body = g.issue_body("gui-real-timing", rec, None)
    assert "Failing case" not in body and "SIGILL sample" not in body
    assert "GUI real-timing pool" in body


# --- gui leg runner boundaries ---------------------------------------------------

def test_gui_leg_skips_fast_when_build_tools_missing(tmp_path, monkeypatch):
    """Same fast-fail contract as the metal leg: a deterministic configuration
    error precedes readings, build and drive; nothing is measured or faked."""
    ctx = make_ctx(tmp_path)
    calls = []
    monkeypatch.setattr(g, "metal_build_tools_found", lambda: {"cmake": None, "ninja": None})
    monkeypatch.setattr(g, "collect_readings", lambda with_wsl_host: calls.append("readings"))
    monkeypatch.setattr(g, "run_logged", lambda *a, **kw: calls.append(tuple(a)))
    rec = g.run_gui_realtiming(ctx, "sha")
    assert rec["status"] == "error" and calls == [] and "idle_pre" not in rec


def test_gui_leg_sigill_samples_once_and_reuses_the_reading(tmp_path, monkeypatch):
    """The crash-moment load snapshot IS the first post-exit reading: one
    collect_readings call serves idle_after_gui and the sigill event both —
    no second sampling pass after the classification decision."""
    ctx = make_ctx(tmp_path)
    calls = []
    monkeypatch.setattr(g, "metal_build_tools_found",
                        lambda: {"cmake": "/opt/hb/bin/cmake", "ninja": "/opt/hb/bin/ninja"})

    def fake_readings(with_wsl_host):
        calls.append("read")
        return g.Readings(load1=0.7, physical_cores=12)

    monkeypatch.setattr(g, "collect_readings", fake_readings)

    def fake_run_logged(ctx_, cmd, log_path, **kw):
        Path(log_path).touch()
        return -4 if str(cmd[0]).endswith("gui_test") else 0

    monkeypatch.setattr(g, "run_logged", fake_run_logged)
    rec = g.run_gui_realtiming(ctx, "sha")
    assert calls.count("read") == 3          # pre-check, re-check, ONE post-exit reading
    assert rec["status"] == "fail" and rec["gui_exit"] == -4
    assert rec["sigill_event"]["exit_code"] == -4
    assert rec["sigill_event"]["load_snapshot"] == rec["idle_after_gui"]
    assert rec["sigill_event"]["load_snapshot"]["load1"] == 0.7
    assert "sampled_at" in rec["sigill_event"]


def test_gui_leg_reaches_configure_build_and_drive_boundaries(tmp_path, monkeypatch):
    """Tools present: the leg runs configure -> build --target gui_test -> drive
    with the real-timing filter and junit export, to the boundaries where the
    doubles stop it — no build or test workload is ever executed."""
    ctx = make_ctx(tmp_path)
    calls = []
    monkeypatch.setattr(g, "metal_build_tools_found",
                        lambda: {"cmake": "/opt/hb/bin/cmake", "ninja": "/opt/hb/bin/ninja"})
    monkeypatch.setattr(g, "collect_readings",
                        lambda with_wsl_host: g.Readings(load1=0.0, physical_cores=8))

    def fake_run_logged(ctx_, cmd, log_path, **kw):
        calls.append(list(cmd))
        Path(log_path).touch()
        return 0

    monkeypatch.setattr(g, "run_logged", fake_run_logged)
    rec = g.run_gui_realtiming(ctx, "sha")
    conf = [c for c in calls if c[:2] == ["cmake", "-S"]]
    builds = [c for c in calls if c[:2] == ["cmake", "--build"]]
    drives = [c for c in calls if c[0].endswith("gui_test")]
    assert len(conf) == 1 and "-DBUILD_TEST=ON" in conf[0] and "-DBUILD_GUI=ON" in conf[0]
    assert len(builds) == 1 and "--target" in builds[0] and "gui_test" in builds[0]
    assert builds[0][2].endswith("build/cmake_build/static-gate-gui")   # this leg's own tree, not build.sh's
    assert len(drives) == 1
    assert "--filter" in drives[0] and g.GUI_REALTIMING_FILTER in drives[0]
    assert "--export-junit" in drives[0] and "--no-user-config" in drives[0]
    assert "idle_pre" in rec and "idle_post" in rec and "idle_after_gui" in rec
    # the doubles stop at the boundaries: no junit -> no measurement, never a pass
    assert rec["status"] == "error" and "no-junit" in rec["reason"]
    assert (tmp_path / "gui-configure.log").exists() and (tmp_path / "gui-build.log").exists() \
        and (tmp_path / "gui-test.log").exists()


# --- ubsan leg -------------------------------------------------------------------
# The UB-regression leg: a UBSan engine variant drives the two probe tests that
# encode the fixed UB families (the projection float->int cast, the
# CrystalParamHash signed overflow). The matrix pins the recoverable-diagnostic
# shape (UBSan prints and exits 0 — exit 0 with a diagnostic is THE fail) and
# the silent-green shape (exit 0 without the expected cases' [ RUN ] markers),
# the instrumentation reference check's two symbol-table shapes, and the
# runner's boundaries (configure flags, build targets, probe env and filter).

UBSAN_UNIT_CASE = "ProjectionCastBounds.OutOfIntRangeLinearHitStaysDefinedAndOffCanvas"
UBSAN_COMP_CASES = ("EditModalChain.TheCrystalParamHashMovesOnEveryEditAndOnNothingElse",
                    "SceneCommitChain.EveryGroupableSlotChangesThePreviewParamHash")
UBSAN_DIAG = ("src/core/shared/projection_shared.h:368:17: runtime error: "
              "value 6.4e+10 is outside the range of representable values of type 'int'")


def ubsan_log(*cases, diagnostic=""):
    lines = [f"[ RUN      ] {c}" for c in cases] + [f"[       OK ] {c}" for c in cases]
    if diagnostic:
        lines.insert(1, diagnostic)
    return "\n".join(lines) + "\n"


def instrumented_nm(cmd, **kw):
    return subprocess.CompletedProcess(cmd, 0,
                                       stdout="                 U ___ubsan_handle_add_overflow\n",
                                       stderr="")


def test_classify_ubsan_probe_matrix():
    all_ran = ubsan_log(UBSAN_UNIT_CASE)
    cases = (UBSAN_UNIT_CASE,)
    # green: exit 0, every expected case ran, zero diagnostics
    assert g.classify_ubsan_probe(0, all_ran, cases) == ("pass", "")
    # THE red: a recoverable diagnostic — UBSan printed and continued, exit 0
    status, reason = g.classify_ubsan_probe(0, all_ran + UBSAN_DIAG + "\n", cases)
    assert status == "fail" and reason.startswith("sanitizer diagnostics")
    # silent green: exit 0 but the expected case never ran
    status, reason = g.classify_ubsan_probe(0, "[ RUN      ] Some.Other\n", cases)
    assert status == "error" and reason.startswith("probe not executed")
    assert UBSAN_UNIT_CASE in reason
    # a diagnostic outranks the missing execution: the runtime ran, fail
    status, _ = g.classify_ubsan_probe(0, "[ RUN      ] Some.Other\n" + UBSAN_DIAG + "\n", cases)
    assert status == "fail"
    # exit 1: assertion red (the probes encode fixed behavior)
    assert g.classify_ubsan_probe(1, all_ran, cases)[0] == "fail"
    assert g.classify_ubsan_probe(1, all_ran + UBSAN_DIAG + "\n", cases)[0] == "fail"
    # signal death under instrumentation is a defect signal, not an environment state
    status, reason = g.classify_ubsan_probe(-4, all_ran, cases)
    assert status == "fail" and reason.startswith("probe crashed")
    # infrastructure shapes: nothing ran to a verdict
    assert g.classify_ubsan_probe(124, all_ran, cases) == ("error", "probe-timeout (budget exhausted)")
    assert g.classify_ubsan_probe(None, all_ran, cases) == ("error", "probe-did-not-finish")
    status, reason = g.classify_ubsan_probe(2, all_ran, cases)
    assert status == "error" and reason.startswith("probe-exit-")


def test_ubsan_diagnostic_stats_counts_and_locates():
    log = ("before\n"
           "src/core/shared/projection_shared.h:368:17: runtime error: first one\n"
           "  #0 0x108a stack frame line\n"
           "src/gui/crystal_preview.cpp:65:12: runtime error: second one\n"
           "src/gui/crystal_preview.cpp:78:12: runtime error: third one\n")
    count, first, location = g.ubsan_diagnostic_stats(log)
    assert count == 3
    assert location == "src/core/shared/projection_shared.h:368"
    assert first.startswith("src/core/shared/projection_shared.h:368")
    # fail-safe: a SUMMARY without per-line diagnostics still counts one
    count, first, location = g.ubsan_diagnostic_stats("SUMMARY: UndefinedBehaviorSanitizer\n")
    assert count == 1 and "SUMMARY" in first and location == ""
    assert g.ubsan_diagnostic_stats("nothing here\n") == (0, "", "")


def test_fold_ubsan_probe_results_fail_first():
    unit_fail = {"status": "fail", "reason": "sanitizer diagnostics: 1 diagnostic(s); first: a.h:12"}
    comp_error = {"status": "error", "reason": "probe not executed (X)"}
    comp_pass = {"status": "pass", "reason": ""}
    status, reason = g.fold_ubsan_probe_results({"unit_correctness_test": unit_fail,
                                                 "composition_correctness_test": comp_error})
    assert status == "fail" and reason.startswith("sanitizer diagnostics")
    assert "; also composition_correctness_test: probe not executed (X)" in reason
    status, reason = g.fold_ubsan_probe_results({"unit_correctness_test": comp_pass,
                                                 "composition_correctness_test": comp_pass})
    assert status == "pass" and reason == ""
    status, reason = g.fold_ubsan_probe_results({"unit_correctness_test": comp_error,
                                                 "composition_correctness_test": comp_error})
    assert status == "error" and reason.startswith("probe not executed")
    # a fail on the second probe leads with its own reason
    status, reason = g.fold_ubsan_probe_results({"unit_correctness_test": comp_pass,
                                                 "composition_correctness_test": unit_fail})
    assert status == "fail" and reason.startswith("sanitizer diagnostics")


def test_ubsan_probe_filters_pin_the_expected_cases():
    assert list(g.UBSAN_PROBE_FILTERS) == ["unit_correctness_test", "composition_correctness_test"]
    for spec in g.UBSAN_PROBE_FILTERS.values():
        assert list(spec["filter"].split(":")) == list(spec["expected"])
    assert g.UBSAN_PROBE_FILTERS["unit_correctness_test"]["filter"] == UBSAN_UNIT_CASE
    assert list(g.UBSAN_PROBE_FILTERS["composition_correctness_test"]["expected"]) \
        == list(UBSAN_COMP_CASES)


def test_ubsan_instrumentation_reference_accepts_both_shapes():
    # macOS: the runtime is linked dynamically, the handler is an UNDEFINED
    # import (nm's Mach-O spelling has one extra leading underscore)
    mach_o = "                 U ___ubsan_handle_add_overflow\n0000000000000000 T _main\n"
    assert not g.ubsan_instrumentation_missing(mach_o)
    # Linux/static: the handler is a defined symbol
    elf = "0000000000000000 T __ubsan_handle_add_overflow\n0000000000000010 W __ubsan_handle_out_of_bounds\n"
    assert not g.ubsan_instrumentation_missing(elf)
    # a plain binary carries no handler reference at all
    assert g.ubsan_instrumentation_missing("0000000000000000 T _main\n")
    assert g.ubsan_instrumentation_missing("")


def test_ubsan_leg_skips_fast_when_build_tools_missing(tmp_path, monkeypatch):
    ctx = make_ctx(tmp_path)
    calls = []
    monkeypatch.setattr(g, "metal_build_tools_found", lambda: {"cmake": None, "ninja": None})
    monkeypatch.setattr(g, "collect_readings", lambda with_wsl_host: calls.append("readings"))
    monkeypatch.setattr(g, "run_logged", lambda *a, **kw: calls.append(tuple(a)))
    rec = g.run_ubsan(ctx, "sha")
    assert rec["status"] == "error" and calls == [] and "idle_pre" not in rec


def test_ubsan_leg_reaches_configure_build_and_probe_boundaries(tmp_path, monkeypatch):
    """Tools present: the leg runs configure -> build both probe targets ->
    instrumentation reference check -> drive both probes with their filters,
    to the boundaries where the doubles stop it."""
    ctx = make_ctx(tmp_path)
    calls, envs = [], []
    metal_tools(monkeypatch)
    monkeypatch.setattr(g, "collect_readings", lambda with_wsl_host: idle_r())
    monkeypatch.setattr(g, "_run", instrumented_nm)

    def fake_run_logged(ctx_, cmd, log_path, **kw):
        calls.append(list(cmd))
        envs.append(kw.get("env") or {})
        exe = Path(cmd[0]).name
        if exe in g.UBSAN_PROBE_FILTERS:
            Path(log_path).write_text(ubsan_log(*g.UBSAN_PROBE_FILTERS[exe]["expected"]))
        else:
            Path(log_path).touch()
        return 0

    monkeypatch.setattr(g, "run_logged", fake_run_logged)
    rec = g.run_ubsan(ctx, "sha")
    conf = [c for c in calls if c[:2] == ["cmake", "-S"]]
    builds = [c for c in calls if c[:2] == ["cmake", "--build"]]
    probes = [c for c in calls if Path(c[0]).name in g.UBSAN_PROBE_FILTERS]
    assert len(conf) == 1
    assert "-DLUMICE_IPO=OFF" in conf[0] and "-DBUILD_GUI=ON" in conf[0]
    assert any(a.startswith("-DCMAKE_C_FLAGS=") and "-fsanitize=undefined" in a
               and "-gline-tables-only" in a for a in conf[0])
    assert any(a.startswith("-DCMAKE_CXX_FLAGS=") and "-fsanitize=undefined" in a for a in conf[0])
    assert any(a.startswith("-DCMAKE_EXE_LINKER_FLAGS=") and "-fsanitize=undefined" in a
               for a in conf[0])
    assert len(builds) == 1 and builds[0][2].endswith("build/cmake_build/static-gate-ubsan")
    assert "--target" in builds[0] and "unit_correctness_test" in builds[0] \
        and "composition_correctness_test" in builds[0]
    assert len(probes) == 2
    probe_envs = [e for e, c in zip(envs, calls) if Path(c[0]).name in g.UBSAN_PROBE_FILTERS]
    assert all(e.get("UBSAN_OPTIONS") == "print_stacktrace=1" for e in probe_envs)
    assert all(any(a.startswith("--gtest_filter=") for a in p) for p in probes)
    assert "idle_pre" in rec and "idle_post" in rec
    assert rec["status"] == "pass" and rec["reason"] == ""
    assert rec["probe_results"]["unit_correctness_test"]["status"] == "pass"
    assert rec["probe_results"]["unit_correctness_test"]["diagnostics"] == 0
    assert rec["probe_results"]["composition_correctness_test"]["exit"] == 0
    assert (tmp_path / "ubsan-configure.log").exists() and (tmp_path / "ubsan-build.log").exists()


def test_ubsan_leg_build_failure_is_an_error(tmp_path, monkeypatch):
    ctx = make_ctx(tmp_path)
    metal_tools(monkeypatch)
    monkeypatch.setattr(g, "collect_readings", lambda with_wsl_host: idle_r())

    def fake_run_logged(ctx_, cmd, log_path, **kw):
        Path(log_path).touch()
        return 1 if cmd[:2] == ["cmake", "--build"] else 0

    monkeypatch.setattr(g, "run_logged", fake_run_logged)
    rec = g.run_ubsan(ctx, "sha")
    assert rec["status"] == "error"
    assert rec["reason"].startswith("ubsan build failed")
    assert g.public_reason(rec["reason"]) == "ubsan build failed"
    assert "probe_results" not in rec


def test_ubsan_leg_instrumentation_missing_is_an_error(tmp_path, monkeypatch):
    """The backstop against a silently un-instrumented binary: no __ubsan_handle
    reference in the symbol table, or a binary nm cannot read at all, is an
    error before any probe runs."""
    ctx = make_ctx(tmp_path)
    metal_tools(monkeypatch)
    monkeypatch.setattr(g, "collect_readings", lambda with_wsl_host: idle_r())
    monkeypatch.setattr(g, "run_logged", touch_zero_run_logged)

    def plain_nm(cmd, **kw):
        return subprocess.CompletedProcess(cmd, 0, stdout="0000000000000000 T _main\n", stderr="")

    monkeypatch.setattr(g, "_run", plain_nm)
    rec = g.run_ubsan(ctx, "sha")
    assert rec["status"] == "error"
    assert rec["reason"].startswith("instrumentation missing (")
    assert g.public_reason(rec["reason"]) == "instrumentation missing"
    assert "probe_results" not in rec

    def broken_nm(cmd, **kw):
        return subprocess.CompletedProcess(cmd, 1, stdout="", stderr="no such file")

    monkeypatch.setattr(g, "_run", broken_nm)
    rec = g.run_ubsan(ctx, "sha")
    assert rec["status"] == "error" and rec["reason"].startswith("instrumentation missing")


def test_ubsan_leg_marks_both_busy_windows_for_retry(tmp_path, monkeypatch):
    ctx = make_ctx(tmp_path)
    metal_tools(monkeypatch)
    monkeypatch.setattr(g, "collect_readings", lambda with_wsl_host: busy_r())
    rec = g.run_ubsan(ctx, "sha")
    assert rec["env_rejected"] is True and rec["reason"] == BUSY_REASON
    seq = iter([idle_r(), busy_r()])
    monkeypatch.setattr(g, "collect_readings", lambda with_wsl_host: next(seq))
    monkeypatch.setattr(g, "run_logged", touch_zero_run_logged)
    monkeypatch.setattr(g, "_run", instrumented_nm)
    rec = g.run_ubsan(ctx, "sha")
    assert rec["env_rejected"] is True and rec["reason"] == RECHECK_BUSY_REASON


def test_ubsan_leg_dry_run_skips_before_classification(tmp_path, monkeypatch):
    """Dry-run mirrors the stage list (configure, build, the nm check logged,
    both probes) but classifies nothing and runs no real subprocess."""
    ctx = make_ctx(tmp_path)
    ctx.args.dry_run = True
    metal_tools(monkeypatch)
    monkeypatch.setattr(g, "collect_readings", lambda with_wsl_host: idle_r())
    nm_cmds = []

    def recording_nm(cmd, **kw):
        nm_cmds.append(cmd)
        return subprocess.CompletedProcess(cmd, 0,
                                           stdout="                 U ___ubsan_handle_add_overflow\n",
                                           stderr="")

    monkeypatch.setattr(g, "_run", recording_nm)
    monkeypatch.setattr(g, "run_logged", touch_zero_run_logged)
    rec = g.run_ubsan(ctx, "sha")
    assert rec["status"] == "skipped" and rec["reason"] == "dry-run"
    assert "probe_results" not in rec
    assert "idle_pre" in rec and "idle_post" in rec
    assert nm_cmds == []              # the nm check runs nothing in dry-run


def test_ubsan_leg_strips_the_repo_prefix_from_the_diagnostic_location(tmp_path, monkeypatch):
    """UBSan prints the compile-time path (the clone's absolute location); the
    record's location is repo-relative so the issue body names a repo symbol,
    never a local path. The full line stays in the local record unchanged."""
    ctx = make_ctx(tmp_path)
    metal_tools(monkeypatch)
    monkeypatch.setattr(g, "collect_readings", lambda with_wsl_host: idle_r())
    monkeypatch.setattr(g, "_run", instrumented_nm)
    clone = Path(ctx.args.work_dir) / "repo"

    def fake_run_logged(ctx_, cmd, log_path, **kw):
        exe = Path(cmd[0]).name
        if exe == "unit_correctness_test":
            Path(log_path).write_text(
                ubsan_log(UBSAN_UNIT_CASE, diagnostic=f"{clone}/src/core/shared/projection_shared.h:382:17: "
                                                          "runtime error: value 6.4e+10 outside int range\n"))
        elif exe in g.UBSAN_PROBE_FILTERS:
            Path(log_path).write_text(ubsan_log(*g.UBSAN_PROBE_FILTERS[exe]["expected"]))
        else:
            Path(log_path).touch()
        return 0

    monkeypatch.setattr(g, "run_logged", fake_run_logged)
    rec = g.run_ubsan(ctx, "sha")
    pr = rec["probe_results"]["unit_correctness_test"]
    assert pr["first_diagnostic_location"] == "src/core/shared/projection_shared.h:382"
    assert "/private/var" not in pr["first_diagnostic_location"]
    assert pr["first_diagnostic_line"].startswith(str(clone))   # full text stays local
    body = g.issue_body("ubsan", rec, None)
    assert "src/core/shared/projection_shared.h:382" in body
    assert "/private/var" not in body


@pytest.mark.parametrize("reason,category", [
    ("sanitizer diagnostics: 2 diagnostic(s); first: a.h:12", "sanitizer diagnostics"),
    ("instrumentation missing (unit_correctness_test: no __ubsan_handle reference)",
     "instrumentation missing"),
    ("probe not executed (ProjectionCastBounds.X)", "probe not executed"),
    ("ubsan build failed (exit 2), see ubsan-build.log", "ubsan build failed"),
    ("probe crashed (exit -4)", "probe crashed"),
    ("probe assertion failed (exit 1)", "probe assertion failed"),
    ("probe-timeout (budget exhausted)", "probe-timeout"),
    ("probe-did-not-finish", "probe-did-not-finish"),
    ("probe-exit-2", "probe-exit"),
    ("ubsan leg needs macOS", "ubsan leg needs macOS"),
])
def test_ubsan_reason_categories_are_public(reason, category):
    assert g.public_reason(reason) == category


def test_ubsan_issue_body_names_probes_and_locations_but_no_diagnostic_text():
    rec = {"status": "fail", "run_id": "r", "sha": "abc",
           "reason": "sanitizer diagnostics: 2 diagnostic(s); first: projection_shared.h:368",
           "probe_results": {
               "unit_correctness_test": {
                   "status": "fail",
                   "reason": "sanitizer diagnostics: 2 diagnostic(s); first: projection_shared.h:368",
                   "exit": 0, "diagnostics": 2,
                   "first_diagnostic_line": "src/core/shared/projection_shared.h:368:17: "
                                            "runtime error: value 6.4e+10 outside int range "
                                            "/Users/zhang/secret",
                   "first_diagnostic_location": "src/core/shared/projection_shared.h:368"},
               "composition_correctness_test": {
                   "status": "pass", "reason": "", "exit": 0, "diagnostics": 0,
                   "first_diagnostic_line": "", "first_diagnostic_location": ""}}}
    body = g.issue_body("ubsan", rec, None)
    assert "UBSan probe leg" in body
    assert "Probe `unit_correctness_test`: **fail**" in body
    assert "Probe `composition_correctness_test`: **pass**" in body
    assert "`src/core/shared/projection_shared.h:368`" in body
    # the diagnostic's free text and any local path stay in the local record
    assert "/Users/" not in body and "runtime error:" not in body


def test_ubsan_issue_titles_are_distinct_per_namespace():
    titles = {g.issue_title(leg, kind, ns)
              for leg in ("metal", "gui-real-timing", "ubsan", "cuda")
              for kind in g.ISSUE_KINDS
              for ns in ("real", "test")}
    assert len(titles) == len(g.ISSUE_KINDS) * 4 * 2
    assert "[throughput-gate] ubsan fail" in titles
    assert "[TEST][throughput-gate] ubsan fail" in titles


def test_ubsan_leg_registration_and_sequence():
    assert g.LEGS == ("metal", "gui-real-timing", "ubsan", "cuda")
    assert g.parse_args(["--leg", "ubsan"]).leg == "ubsan"
    assert g.parse_args([]).leg == "all"
    assert g.LEG_ROLES["ubsan"]


# --- leg-level retry loop (busy -> backoff -> retry; the deadline wall) ---------
# The loop runs on injectable clock/sleep seams, so no test here ever really
# sleeps. The idle windows keep their real implementation: the window-timing
# tests below drive the actual wait_idle loop with tiny real budgets.

BUSY_REASON = "busy (pre-check): load 9.00/8 cores = 1.125 >= 0.5"
RECHECK_BUSY_REASON = "busy (post-build re-check): load 9.00/8 cores = 1.125 >= 0.5"


def busy_r():
    return g.Readings(load1=9.0, physical_cores=8)


def idle_r():
    return g.Readings(load1=0.0, physical_cores=8)


def metal_tools(monkeypatch):
    monkeypatch.setattr(g, "metal_build_tools_found",
                        lambda: {"cmake": "/opt/hb/bin/cmake", "ninja": "/opt/hb/bin/ninja"})


def touch_zero_run_logged(ctx_, cmd, log_path, **kw):
    Path(log_path).touch()               # run_logged always creates the log file
    return 0


def at(hour, minute, second=0):
    """A local wall-clock time on today's date — the same date
    resolve_nightly_deadline picks, so test clocks and the code compare like with like."""
    return dt.datetime.combine(dt.date.today(), dt.time(hour, minute, second))


class FakeClock:
    """The clock/sleep seam the retry loop and the deadline wall run on.
    sleep() advances the clock and records every nonzero sleep."""

    def __init__(self, start):
        self.now = start
        self.slept = []

    def clock(self):
        return self.now

    def sleep(self, seconds):
        if seconds:
            self.slept.append(seconds)
            self.now += dt.timedelta(seconds=seconds)

    def advance(self, seconds):
        self.now += dt.timedelta(seconds=seconds)


def retry_ctx(tmp_path, clock, deadline=None, max_backoffs=2, backoff_seconds=3600.0,
              cooldown_timeout=0.0, cooldown_poll=0.0, remote_poll=0.0,
              remote_build_timeout=99.0):
    """A Ctx for retry-loop tests: fake clock/sleep seams, deadline opt-in,
    zero-length idle windows (the window's own timing has its own tests)."""
    args = argparse.Namespace(dry_run=False, cooldown_timeout=cooldown_timeout,
                              cooldown_poll=cooldown_poll,
                              threshold_override=None, pytest_timeout=10,
                              idle_retry_backoff_seconds=backoff_seconds,
                              idle_retry_max_backoffs=max_backoffs,
                              remote="remote-host", remote_dir="gate", ssh_connect_timeout=5,
                              remote_build_timeout=remote_build_timeout, remote_poll=remote_poll,
                              cpm_cache=str(tmp_path / "cpm"), work_dir=str(tmp_path / "work"))
    return g.Ctx(args=args, run_id="t", run_dir=tmp_path, log=lambda msg: None,
                 thresholds=g.IdleThresholds(), synthetic=False,
                 nightly_deadline=deadline, clock=clock.clock, sleep=clock.sleep)


def test_retry_budget_exhaustion_payload(tmp_path, monkeypatch):
    """AC1/AC3: busy through every window -> terminal skip with the full spent
    budget, the configured backoffs actually slept, and the rejected attempts
    in history (the terminal attempt itself is the record, not a history row)."""
    clock = FakeClock(at(3, 30))
    ctx = retry_ctx(tmp_path, clock)
    metal_tools(monkeypatch)
    monkeypatch.setattr(g, "collect_readings", lambda with_wsl_host: busy_r())
    rec = g.run_leg_with_retry(ctx, "metal", g.run_metal, "sha")
    assert rec["status"] == "skipped"
    assert rec["reason"] == BUSY_REASON
    assert rec["env_rejected"] is True
    ir = rec["idle_retry"]
    assert ir["attempts"] == 3 and ir["backoffs"] == 2
    assert ir["backoff_seconds"] == 3600.0 and ir["terminal"] == "max_backoffs"
    assert clock.slept == [3600.0, 3600.0]
    assert [h["attempt"] for h in ir["history"]] == [1, 2]
    assert all(h["reason"].startswith("busy (pre-check):") for h in ir["history"])
    # staleness semantics untouched: the terminal state is still a skip
    assert not g.refreshes_last_real_run(rec["status"], False)


def test_zero_backoff_budget_exhausts_on_the_first_busy_attempt(tmp_path, monkeypatch):
    """max_backoffs=0: one busy attempt IS the budget — attempts=1, backoffs=0,
    terminal=max_backoffs, empty history; the controller forced the end even
    though nothing was slept."""
    ctx = retry_ctx(tmp_path, FakeClock(at(3, 30)), max_backoffs=0)
    metal_tools(monkeypatch)
    monkeypatch.setattr(g, "collect_readings", lambda with_wsl_host: busy_r())
    rec = g.run_leg_with_retry(ctx, "metal", g.run_metal, "sha")
    assert rec["idle_retry"] == {"attempts": 1, "backoffs": 0, "backoff_seconds": 3600.0,
                                 "terminal": "max_backoffs", "history": []}


def test_recovery_within_budget_carries_history_without_terminal(tmp_path, monkeypatch):
    """busy twice, then the machine frees up: the third attempt measures. The
    pass record carries the retry history ('busy at 03:30, measured at 04:31'
    stays visible) but no terminal — its status already says what happened."""
    ctx = retry_ctx(tmp_path, FakeClock(at(3, 30)))
    metal_tools(monkeypatch)
    (ctx.run_dir / "metal-junit.xml").write_text(junit("passed"))
    seq = iter([busy_r(), busy_r(), idle_r(), idle_r(), idle_r()])
    monkeypatch.setattr(g, "collect_readings", lambda with_wsl_host: next(seq))
    monkeypatch.setattr(g, "run_logged", touch_zero_run_logged)
    rec = g.run_leg_with_retry(ctx, "metal", g.run_metal, "sha")
    assert rec["status"] == "pass"
    ir = rec["idle_retry"]
    assert ir["attempts"] == 3 and ir["backoffs"] == 2
    assert "terminal" not in ir
    assert [h["attempt"] for h in ir["history"]] == [1, 2]


def test_busy_then_fail_keeps_the_verdict_and_the_history(tmp_path, monkeypatch):
    """One backoff, then a functional red: the fail verdict stands (a red is a
    measurement, not an environment state) and the history rides along, still
    without a terminal."""
    ctx = retry_ctx(tmp_path, FakeClock(at(3, 30)))
    metal_tools(monkeypatch)
    (ctx.run_dir / "metal-junit.xml").write_text(junit("failure", message="dual-renderer throughput 0.80x"))
    seq = iter([busy_r(), idle_r(), idle_r(), idle_r()])
    monkeypatch.setattr(g, "collect_readings", lambda with_wsl_host: next(seq))

    def pytest_exit_one(ctx_, cmd, log_path, **kw):
        Path(log_path).touch()
        return 0 if cmd[0] == "./scripts/build.sh" else 1   # build ok, gate red

    monkeypatch.setattr(g, "run_logged", pytest_exit_one)
    rec = g.run_leg_with_retry(ctx, "metal", g.run_metal, "sha")
    assert rec["status"] == "fail"
    ir = rec["idle_retry"]
    assert ir["attempts"] == 2 and ir["backoffs"] == 1 and "terminal" not in ir


@pytest.mark.parametrize("attempt", [
    {"status": "fail", "reason": "functional red: some_case"},
    {"status": "error", "reason": "build failed (exit 1), see metal-build.log"},
    {"status": "inconclusive", "reason": "budget red: maximum_frozen_map_input_to_present"},
    {"status": "skipped", "reason": "unreachable (ssh exit 255)"},
])
def test_only_environment_busy_triggers_a_retry(attempt, tmp_path):
    """AC2: a red is a measurement, not an environment state — none of the four
    shapes retries, and a first-attempt return carries no idle_retry at all."""
    calls = []

    def runner(ctx, sha):
        calls.append(1)
        return dict(attempt)

    out = g.run_leg_with_retry(retry_ctx(tmp_path, FakeClock(at(3, 30))), "metal", runner, "sha")
    assert len(calls) == 1 and out == attempt
    assert "idle_retry" not in out


def test_history_entries_carry_per_attempt_snapshots(tmp_path, monkeypatch):
    """AC3 payload shape: a re-check busy attempt keeps pre+post readings, a
    pre-check busy attempt only pre; every entry carries its index, end time
    and duration."""
    ctx = retry_ctx(tmp_path, FakeClock(at(3, 30)))
    metal_tools(monkeypatch)
    # attempt 1: pre idle, build, re-check busy; attempts 2-3: pre-check busy
    seq = iter([idle_r(), busy_r(), busy_r(), busy_r()])
    monkeypatch.setattr(g, "collect_readings", lambda with_wsl_host: next(seq))
    monkeypatch.setattr(g, "run_logged", touch_zero_run_logged)
    rec = g.run_leg_with_retry(ctx, "metal", g.run_metal, "sha")
    ir = rec["idle_retry"]
    assert ir["attempts"] == 3 and ir["backoffs"] == 2 and ir["terminal"] == "max_backoffs"
    h1, h2 = ir["history"]
    assert h1["attempt"] == 1 and h1["reason"].startswith("busy (post-build re-check):")
    assert "idle_pre" in h1 and "idle_post" in h1
    assert h2["attempt"] == 2 and h2["reason"].startswith("busy (pre-check):")
    assert "idle_pre" in h2 and "idle_post" not in h2
    for h in (h1, h2):
        assert {"attempt", "ended_at", "seconds", "reason"} <= set(h)


def test_window_recovery_continues_the_attempt_without_a_backoff(tmp_path, monkeypatch):
    """The real wait_idle window on a tiny real budget: busy readings inside
    the window do not consume the attempt — the machine freeing up mid-window
    goes straight on to build and measure, with zero backoff calls."""
    clock = FakeClock(at(3, 30))
    ctx = retry_ctx(tmp_path, clock, cooldown_timeout=5.0, cooldown_poll=0.0)
    metal_tools(monkeypatch)
    (ctx.run_dir / "metal-junit.xml").write_text(junit("passed"))
    seq = iter([busy_r(), busy_r(), idle_r(), idle_r(), idle_r()])
    monkeypatch.setattr(g, "collect_readings", lambda with_wsl_host: next(seq))
    monkeypatch.setattr(g, "run_logged", touch_zero_run_logged)
    rec = g.run_leg_with_retry(ctx, "metal", g.run_metal, "sha")
    assert rec["status"] == "pass"
    assert "idle_retry" not in rec      # zero backoffs -> no payload at all
    assert clock.slept == []


def test_window_exhaustion_costs_one_configured_backoff(tmp_path, monkeypatch):
    """Busy through the whole window: the attempt ends busy only when the
    window budget is out, and the backoff that follows is the configured one."""
    clock = FakeClock(at(3, 30))
    ctx = retry_ctx(tmp_path, clock, cooldown_timeout=0.1, cooldown_poll=0.0, max_backoffs=1)
    metal_tools(monkeypatch)
    monkeypatch.setattr(g, "collect_readings", lambda with_wsl_host: busy_r())
    rec = g.run_leg_with_retry(ctx, "metal", g.run_metal, "sha")
    assert rec["env_rejected"] is True and rec["status"] == "skipped"
    assert clock.slept[0] == 3600.0
    assert rec["idle_retry"]["attempts"] == 2 and rec["idle_retry"]["terminal"] == "max_backoffs"


def test_metal_leg_marks_both_busy_windows_for_retry(tmp_path, monkeypatch):
    ctx = make_ctx(tmp_path)
    metal_tools(monkeypatch)
    monkeypatch.setattr(g, "collect_readings", lambda with_wsl_host: busy_r())
    rec = g.run_metal(ctx, "sha")
    assert rec["env_rejected"] is True and rec["reason"] == BUSY_REASON
    seq = iter([idle_r(), busy_r()])
    monkeypatch.setattr(g, "collect_readings", lambda with_wsl_host: next(seq))
    monkeypatch.setattr(g, "run_logged", touch_zero_run_logged)
    rec = g.run_metal(ctx, "sha")
    assert rec["env_rejected"] is True and rec["reason"] == RECHECK_BUSY_REASON


def test_gui_leg_marks_both_busy_windows_for_retry(tmp_path, monkeypatch):
    ctx = make_ctx(tmp_path)
    metal_tools(monkeypatch)
    monkeypatch.setattr(g, "collect_readings", lambda with_wsl_host: busy_r())
    rec = g.run_gui_realtiming(ctx, "sha")
    assert rec["env_rejected"] is True and rec["reason"] == BUSY_REASON
    seq = iter([idle_r(), busy_r()])
    monkeypatch.setattr(g, "collect_readings", lambda with_wsl_host: next(seq))
    monkeypatch.setattr(g, "run_logged", touch_zero_run_logged)
    rec = g.run_gui_realtiming(ctx, "sha")
    assert rec["env_rejected"] is True and rec["reason"] == RECHECK_BUSY_REASON


def test_cuda_leg_marks_both_busy_windows_for_retry(tmp_path, monkeypatch):
    ctx = make_ctx(tmp_path)
    monkeypatch.setattr(g, "_run",
                        lambda c, **kw: subprocess.CompletedProcess(c, 0, stdout="", stderr=""))
    monkeypatch.setattr(g, "remote_readings", lambda ctx_, *a: busy_r())
    rec = g.run_cuda(ctx, "sha")
    assert rec["env_rejected"] is True and rec["reason"] == BUSY_REASON
    seq = iter([idle_r(), busy_r()])
    monkeypatch.setattr(g, "remote_readings", lambda ctx_, *a: next(seq))
    monkeypatch.setattr(g, "run_logged", touch_zero_run_logged)
    monkeypatch.setattr(g, "remote_wait_phase", lambda ctx_, phase, rdir, budget, **kw: 0)
    rec = g.run_cuda(ctx, "sha")
    assert rec["env_rejected"] is True and rec["reason"] == RECHECK_BUSY_REASON


def test_cuda_unreachable_is_not_marked_for_retry(tmp_path, monkeypatch):
    """The red side of the marker rule: cuda's unreachable skip carries no
    env_rejected — a dead remote is not an environment the wall can wait out."""
    ctx = make_ctx(tmp_path)
    monkeypatch.setattr(g, "_run",
                        lambda c, **kw: subprocess.CompletedProcess(c, 255, stdout="", stderr="no route"))
    rec = g.run_cuda(ctx, "sha")
    assert rec["reason"] == "unreachable (ssh exit 255)"
    assert "env_rejected" not in rec


def test_backoff_sleep_ends_at_the_deadline_not_a_duration_later(tmp_path, monkeypatch):
    """D1: a backoff in progress across 06:30 ends AT the wall — the clock
    shows 06:30, not 07:20 — and the leg ends deadline-blocked with the busy
    record byte-identical and no further attempt."""
    clock = FakeClock(at(6, 20))
    ctx = retry_ctx(tmp_path, clock, deadline=at(6, 30))
    metal_tools(monkeypatch)
    monkeypatch.setattr(g, "collect_readings", lambda with_wsl_host: busy_r())
    rec = g.run_leg_with_retry(ctx, "metal", g.run_metal, "sha")
    assert clock.now == at(6, 30) and clock.slept == [600.0]
    assert rec["reason"] == BUSY_REASON
    assert rec["env_rejected"] is True
    ir = rec["idle_retry"]
    assert ir["terminal"] == "deadline" and ir["attempts"] == 1 and ir["backoffs"] == 1
    assert ir["history"] == []          # attempt 1 IS the terminal record


def test_in_flight_attempt_is_voided_at_a_stage_boundary(tmp_path, monkeypatch):
    """D2: attempt past pre-check (idle), the build runs across the wall -> the
    stage guard voids it: deadline skip, no measurement fields, the voided
    attempt in history with the readings it had actually taken."""
    clock = FakeClock(at(6, 25))
    ctx = retry_ctx(tmp_path, clock, deadline=at(6, 30))
    metal_tools(monkeypatch)
    monkeypatch.setattr(g, "collect_readings", lambda with_wsl_host: idle_r())

    def fake_run_logged(ctx_, cmd, log_path, **kw):
        Path(log_path).touch()
        clock.advance(600)              # the build runs ten minutes, across 06:30
        return 0

    monkeypatch.setattr(g, "run_logged", fake_run_logged)
    rec = g.run_leg_with_retry(ctx, "metal", g.run_metal, "sha")
    assert rec["status"] == "skipped"
    assert rec["reason"].startswith("nightly deadline 06:30 reached at ")
    assert "voided before measurement" in rec["reason"]
    assert "env_rejected" not in rec
    ir = rec["idle_retry"]
    assert ir["attempts"] == 1 and ir["backoffs"] == 0 and ir["terminal"] == "deadline"
    (entry,) = ir["history"]
    assert entry["attempt"] == 1 and entry["voided"] is True
    assert "idle_pre" in entry and "idle_post" not in entry
    assert "reason" not in entry        # never busy -> no busy snapshot is faked
    assert "idle_pre" not in rec and "build_seconds" not in rec


def test_long_phase_stops_at_the_deadline_and_never_becomes_a_timeout_error(tmp_path, monkeypatch):
    """D2b: the build is sent out under the remaining time as its timeout, is
    killed at the wall (rc 124) — and the leg's outcome is a deadline skip,
    not 'build failed (exit 124)'."""
    clock = FakeClock(at(6, 25))
    ctx = retry_ctx(tmp_path, clock, deadline=at(6, 30))
    metal_tools(monkeypatch)
    monkeypatch.setattr(g, "collect_readings", lambda with_wsl_host: idle_r())
    seen = {}

    def fake_run_logged(ctx_, cmd, log_path, **kw):
        seen["timeout"] = kw.get("timeout")
        Path(log_path).touch()
        clock.advance(600)              # overruns the wall, killed by the timeout
        return 124

    monkeypatch.setattr(g, "run_logged", fake_run_logged)
    rec = g.run_leg_with_retry(ctx, "metal", g.run_metal, "sha")
    assert seen["timeout"] == 300.0     # 06:30 minus 06:25: the remaining time, not None
    assert rec["status"] == "skipped" and "deadline" in rec["reason"]
    assert "build failed" not in rec["reason"]


# --- main-level wiring: per-leg budgets, the wall across legs, applicability ----

def main_env(tmp_path, monkeypatch):
    """Common main() doubles: macOS platform, no open issues, checkout faked,
    and — the belt this file earned the hard way — notifications recorded
    instead of applied, so no main-level test can ever reach gh. (2026-11-17:
    the source-error test drove the real notification path with a doubled
    query face only; every run filed fresh ``error`` issues on the real repo.)"""
    monkeypatch.setattr(g.platform, "system", lambda: "Darwin")
    monkeypatch.setattr(g, "open_issues", lambda ctx, leg, namespace: {})
    monkeypatch.setattr(g, "prepare_checkout", lambda ctx: "fakesha")
    monkeypatch.setattr(g, "apply_action",
                        lambda ctx, leg, namespace, action:
                        ctx.log(f"notify (test double): {action.op} {action.kind} for {leg}"))
    return tmp_path / "state"


_REAL_GATE_CTX = g.Ctx   # captured at import: inject_clock must never chain through itself


def inject_clock(monkeypatch, clock):
    """Route every Ctx main() builds through the fake clock's seams. Always
    delegates to the class captured at import, so a second injection in the
    same test replaces the first instead of wrapping it."""
    monkeypatch.setattr(g, "Ctx",
                        lambda *a, **kw: _REAL_GATE_CTX(*a, clock=clock.clock, sleep=clock.sleep, **kw))


def busy_fab(calls, name):
    def fab(ctx, sha):
        calls.append(name)
        return {**g.busy_skip("pre-check", ["load 9.00/8 cores = 1.125 >= 0.5"]),
                "idle_pre": busy_r().to_json()}
    return fab


def install_busy_legs(monkeypatch, calls):
    monkeypatch.setattr(g, "run_metal", busy_fab(calls, "metal"))
    monkeypatch.setattr(g, "run_gui_realtiming", busy_fab(calls, "gui-real-timing"))
    monkeypatch.setattr(g, "run_ubsan", busy_fab(calls, "ubsan"))
    monkeypatch.setattr(g, "run_cuda", busy_fab(calls, "cuda"))


def read_records(state_dir):
    return [json.loads(line) for line in (state_dir / "results.jsonl").read_text().splitlines()]


def test_main_wiring_gives_each_leg_its_own_budget(tmp_path, monkeypatch):
    """AC4: the retry loop wraps all four dispatch branches; dry-run forces
    the backoff to zero so the loop runs to its budget without sleeping, and
    the four legs each spend their own budget serially."""
    state_dir = main_env(tmp_path, monkeypatch)
    calls = []
    install_busy_legs(monkeypatch, calls)
    backoff_calls = []
    real_backoff = g.backoff_sleep

    def counting_backoff(ctx_, seconds):
        backoff_calls.append(seconds)
        return real_backoff(ctx_, seconds)

    monkeypatch.setattr(g, "backoff_sleep", counting_backoff)
    rc = g.main(["--leg", "all", "--state-dir", str(state_dir),
                 "--work-dir", str(tmp_path / "work"), "--dry-run"])
    assert rc == 0
    recs = read_records(state_dir)
    assert [r["leg"] for r in recs] == ["metal", "gui-real-timing", "ubsan", "cuda"]
    for r in recs:
        assert r["dry_run"] is True
        assert r["idle_retry"]["attempts"] == 3
        assert r["idle_retry"]["terminal"] == "max_backoffs"
    assert len(calls) == 12             # 3 attempts per leg: budgets are per-leg
    assert backoff_calls == [0.0] * 8   # dry-run: the loop runs, nothing sleeps


def test_deadline_wall_cuts_the_serial_budget_sum(tmp_path, monkeypatch):
    """Four fully busy legs: per-leg budgets sum to 8 h of backoffs
    (03:30 -> 11:30); the wall ends the run at 06:30 — metal exhausts its own
    budget before the wall, the gui leg's backoff is cut by it, ubsan and cuda
    never start (AC-D1/D3, combined with the per-leg budget shape)."""
    state_dir = main_env(tmp_path, monkeypatch)
    clock = FakeClock(at(3, 30))
    inject_clock(monkeypatch, clock)
    calls = []
    install_busy_legs(monkeypatch, calls)
    rc = g.main(["--leg", "all", "--state-dir", str(state_dir),
                 "--work-dir", str(tmp_path / "work"), "--trigger", "launchd"])
    assert rc == 0
    metal, gui, ubsan, cuda = read_records(state_dir)
    assert metal["idle_retry"]["terminal"] == "max_backoffs"
    assert metal["idle_retry"]["attempts"] == 3 and metal["idle_retry"]["backoffs"] == 2
    assert metal["reason"] == BUSY_REASON
    assert gui["idle_retry"]["terminal"] == "deadline"
    assert gui["idle_retry"]["attempts"] == 1 and gui["idle_retry"]["backoffs"] == 1
    assert gui["reason"] == BUSY_REASON and gui["env_rejected"] is True
    assert cuda["reason"] == "nightly deadline 06:30 reached before this leg started"
    assert ubsan["reason"] == "nightly deadline 06:30 reached before this leg started"
    assert "idle_retry" not in cuda and "idle_retry" not in ubsan
    assert calls.count("cuda") == 0 and calls.count("ubsan") == 0
    assert clock.now == at(6, 30)       # the wall, not the 11:30 budget sum


def test_late_nightly_run_skips_every_leg_without_attempts(tmp_path, monkeypatch):
    """AC-D2: a launchd run that started past the wall ends immediately —
    every leg one deadline skip, zero attempts, no idle_retry payload."""
    state_dir = main_env(tmp_path, monkeypatch)
    inject_clock(monkeypatch, FakeClock(at(6, 31)))
    calls = []
    install_busy_legs(monkeypatch, calls)
    rc = g.main(["--leg", "all", "--state-dir", str(state_dir),
                 "--work-dir", str(tmp_path / "work"), "--trigger", "launchd"])
    assert rc == 0
    recs = read_records(state_dir)
    assert [r["leg"] for r in recs] == ["metal", "gui-real-timing", "ubsan", "cuda"]
    for r in recs:
        assert r["status"] == "skipped"
        assert r["reason"] == "nightly deadline 06:30 reached before this leg started"
        assert "idle_retry" not in r and "env_rejected" not in r
    assert calls == []                  # the wall precedes every attempt


def test_manual_runs_are_not_bounded_by_the_nightly_deadline(tmp_path, monkeypatch):
    """AC-D2's negative control: a manual run started past the wall runs its
    full retry budget — the wall governs launchd runs only, never the
    operator's explicit daytime run."""
    state_dir = main_env(tmp_path, monkeypatch)
    inject_clock(monkeypatch, FakeClock(at(6, 31)))
    calls = []
    install_busy_legs(monkeypatch, calls)
    rc = g.main(["--leg", "all", "--state-dir", str(state_dir),
                 "--work-dir", str(tmp_path / "work"), "--trigger", "manual"])
    assert rc == 0
    recs = read_records(state_dir)
    for r in recs:
        assert r["idle_retry"]["terminal"] == "max_backoffs"
        assert r["idle_retry"]["attempts"] == 3
        assert "deadline" not in r["reason"]
    assert len(calls) == 12


def test_far_future_deadline_is_inert(tmp_path, monkeypatch):
    """D5: a wall far outside the run's span leaves the records identical to a
    run with no wall at all (projected to the fields a deadline could touch;
    the trigger field is the one legitimate difference)."""
    main_env(tmp_path, monkeypatch)

    def run(state_dir, trigger, deadline):
        inject_clock(monkeypatch, FakeClock(at(3, 30)))
        calls = []
        install_busy_legs(monkeypatch, calls)
        argv = ["--leg", "all", "--state-dir", str(state_dir),
                "--work-dir", str(tmp_path / "work"), "--trigger", trigger]
        if deadline:
            argv += ["--nightly-deadline", deadline]
        assert g.main(argv) == 0
        return read_records(state_dir)

    bounded = run(tmp_path / "s1", "launchd", "23:59")
    unbounded = run(tmp_path / "s2", "manual", None)

    def project(recs):
        return [{k: r[k] for k in ("leg", "status", "reason", "idle_retry")} for r in recs]

    assert project(bounded) == project(unbounded)
    assert all(r["idle_retry"]["terminal"] == "max_backoffs" for r in bounded)


def test_retry_parameters_carry_the_owner_pinned_defaults():
    a = g.parse_args([])
    assert a.idle_retry_backoff_seconds == 3600.0
    assert a.idle_retry_max_backoffs == 2
    assert a.nightly_deadline == dt.time(6, 30)
    with pytest.raises(SystemExit):
        g.parse_args(["--nightly-deadline", "6:30pm"])


# --- work actually stops, waits end at the wall ---------------------------------
# The deadline's promise is not "the record says skip" but "the machine stops":
# these probes drive the real execution paths (subprocess tree, remote poll
# loop) — a mocked run_logged/wait would only prove the return value, not that
# the work stopped or the wait ended.

def test_run_logged_timeout_stops_the_whole_work_tree(tmp_path):
    """A real two-level process tree: the timed-out phase's grandchild is dead
    too — its 2s of work never completes in the background after the 124."""
    marker = tmp_path / "gc-done.txt"
    gc_pid_file = tmp_path / "gc-pid.txt"
    grandchild = f"import time; time.sleep(30); open({str(marker)!r}, 'w').write('done')"
    child = (f"import subprocess, time; "
             f"p = subprocess.Popen([{sys.executable!r}, '-c', {grandchild!r}]); "
             f"open({str(gc_pid_file)!r}, 'w').write(str(p.pid)); time.sleep(30)")
    rc = g.run_logged(make_ctx(tmp_path), [sys.executable, "-c", child],
                      tmp_path / "phase.log", timeout=0.5)
    assert rc == 124
    gc_pid = int(gc_pid_file.read_text())
    assert wait_until(lambda: _pid_gone(gc_pid), 10.0)   # the timeout ended the whole tree
    assert not marker.exists()     # a dead grandchild cannot write; its 30s lifetime
    # makes any racing completion structurally impossible, on any runner speed


def test_remote_wait_phase_ends_at_the_wall_without_oversleeping(tmp_path, monkeypatch):
    """The wall governs the real poll loop: no poll past it, the poll sleep is
    cut to it (10s left, 30s poll -> one ssh, wake exactly at the wall), and
    each ssh is bounded by the remaining wall time, not the fixed 60s."""
    clock = FakeClock(at(6, 29, 50))
    ctx = retry_ctx(tmp_path, clock, deadline=at(6, 30), remote_poll=30.0)
    ssh_timeouts = []

    def fake_run(cmd, **kw):
        ssh_timeouts.append(kw.get("timeout"))
        return subprocess.CompletedProcess(cmd, 1, stdout="", stderr="")   # never done

    monkeypatch.setattr(g, "_run", fake_run)
    rc = g.remote_wait_phase(ctx, "build", "t", budget_s=3600.0, wall=at(6, 30))
    assert rc is None
    assert clock.now == at(6, 30)          # ended AT the wall, not a poll later
    assert len(ssh_timeouts) == 1          # no poll issued past the wall
    assert ssh_timeouts[0] == pytest.approx(10.0, abs=0.01)   # ssh bounded by the remaining wall
    assert clock.slept == [10.0]


def test_remote_wait_phase_still_reads_one_final_state_on_plain_budget(tmp_path, monkeypatch):
    """Without a wall, the budget stays soft exactly as before: the poll runs
    first, the budget is checked after, and the ssh keeps its full timeout — a
    phase completing inside the last window is still read. Real subprocess
    probe: the markers land mid-sleep, the poll after reads them (a budget-
    capped ssh timeout would die at zero seconds and lose them)."""
    out_dir = tmp_path / "results" / "t"
    out_dir.mkdir(parents=True)
    args = argparse.Namespace(dry_run=False, cooldown_timeout=0.0, cooldown_poll=0.0,
                              threshold_override=None, pytest_timeout=10,
                              idle_retry_backoff_seconds=3600.0, idle_retry_max_backoffs=2,
                              remote="remote-host", remote_dir=str(tmp_path),
                              ssh_connect_timeout=5, remote_build_timeout=99.0,
                              remote_poll=0.2, cpm_cache=str(tmp_path / "cpm"),
                              work_dir=str(tmp_path / "work"))
    ctx = g.Ctx(args=args, run_id="t", run_dir=tmp_path, log=lambda msg: None,
                thresholds=g.IdleThresholds(), synthetic=False)   # real clock, real sleep
    # the markers land 25ms in — inside the 50ms budget, during the 200ms poll
    subprocess.Popen(["bash", "-c",
                      f"(sleep 0.025; echo 0 > {out_dir}/build.exit; touch {out_dir}/build.done) &"])

    def local_probe(cmd, **kw):
        return subprocess.run(["bash", "-c", cmd[-1]], capture_output=True, text=True,
                              timeout=kw.get("timeout"))

    monkeypatch.setattr(g, "_run", local_probe)
    rc = g.remote_wait_phase(ctx, "build", "t", budget_s=0.05, wall=None)
    assert rc == 0          # the completion was read, not lost to a zero-second timeout


def test_remote_phase_script_records_its_pid_for_the_stop_interface(tmp_path):
    ctx = make_ctx(tmp_path)
    script = g.remote_phase_script(ctx, "build", "sha", "r")
    assert "echo $$" in script and "results/r/build.pid" in script
    script2 = g.remote_phase_script(ctx, "pytest", "sha", "r")
    assert "results/r/pytest.pid" in script2
    # GNU timeout makes its own process group: the worker group's id is
    # recorded too, so the stop interface can reach the measurement work.
    assert "$OUT/pytest-worker.pid" in script2
    assert "timeout 10.000" in script2     # the configured budget, at full precision


def cuda_remote_doubles(monkeypatch, run_logged_cmds, marker_state, readings):
    """Real run_cuda IO: fake the ssh capture layer (_run) with per-directory
    done-marker state, capture every run_logged command, idle readings."""
    monkeypatch.setattr(g, "remote_readings", lambda ctx_, *a: next(readings))

    def fake_run_logged(ctx_, cmd, log_path, **kw):
        run_logged_cmds.append(" ".join(str(c) for c in cmd))
        Path(log_path).touch()
        return 0

    monkeypatch.setattr(g, "run_logged", fake_run_logged)

    def fake_run(cmd, **kw):
        if cmd[-1] == "true":
            return subprocess.CompletedProcess(cmd, 0, stdout="", stderr="")
        query = cmd[-1]
        for rdir, exit_code in marker_state.items():
            if f"results/{rdir}/build.done" in query:
                return subprocess.CompletedProcess(cmd, 0, stdout=f"{exit_code}\n", stderr="")
        return subprocess.CompletedProcess(cmd, 1, stdout="", stderr="")

    monkeypatch.setattr(g, "_run", fake_run)


def test_retried_cuda_attempt_gets_its_own_remote_dir(tmp_path, monkeypatch):
    """A retried cuda attempt starts its remote phases in a fresh results
    directory: the first attempt's successful build.done can never be read as
    the second attempt's build result, and an unfinished second build does not
    open the measurement phase."""
    clock = FakeClock(at(3, 30))
    ctx = retry_ctx(tmp_path, clock, remote_build_timeout=0.05)
    run_logged_cmds = []
    marker_state = {ctx.run_id: 0}         # attempt 1: build succeeds in ITS directory
    readings = iter([idle_r(), busy_r(),   # attempt 1: pre idle, re-check busy
                     idle_r(), idle_r(), idle_r(), idle_r()])
    cuda_remote_doubles(monkeypatch, run_logged_cmds, marker_state, readings)
    rec = g.run_leg_with_retry(ctx, "cuda", g.run_cuda, "sha")
    assert rec["status"] == "error"        # the retried build is unfinished -> error, never pass
    assert "remote build failed" in rec["reason"]
    assert rec["idle_retry"]["attempts"] == 2
    assert any(f"results/{ctx.run_id}-a2/build.sh" in c for c in run_logged_cmds)
    assert not any(f"-a2" in c and "pytest.sh" in c for c in run_logged_cmds)


def test_retried_cuda_attempt_does_not_inherit_a_pass_from_a_failed_build(tmp_path, monkeypatch):
    """Second build FAILS (exit 2 in its own directory): the leg ends error —
    the first attempt's successful marker must not launder it into a pass."""
    clock = FakeClock(at(3, 30))
    ctx = retry_ctx(tmp_path, clock)
    run_logged_cmds = []
    marker_state = {ctx.run_id: 0, f"{ctx.run_id}-a2": 2}
    readings = iter([idle_r(), busy_r(),   # attempt 1: pre idle, re-check busy
                     idle_r(), idle_r(), idle_r(), idle_r()])
    cuda_remote_doubles(monkeypatch, run_logged_cmds, marker_state, readings)
    rec = g.run_leg_with_retry(ctx, "cuda", g.run_cuda, "sha")
    assert rec["status"] == "error"
    assert "remote build failed (exit 2)" in rec["reason"]
    assert not any(f"-a2" in c and "pytest.sh" in c for c in run_logged_cmds)


def test_cuda_deadline_reaches_the_remote_stop_interface(tmp_path, monkeypatch):
    """D2b's cuda half: the wall's void reaches the remote stop interface — the
    kill command names the phase's recorded pid file — the wait itself ended at
    the wall with no poll past it, and the leg lands a deadline skip, not an
    error."""
    clock = FakeClock(at(6, 29, 50))
    ctx = retry_ctx(tmp_path, clock, deadline=at(6, 30), remote_poll=30.0)
    run_logged_cmds = []
    marker_state = {}
    ssh_timeouts = []
    readings = iter([idle_r(), idle_r()])

    def fake_run_logged(ctx_, cmd, log_path, **kw):
        run_logged_cmds.append(" ".join(str(c) for c in cmd))
        Path(log_path).touch()
        return 0

    monkeypatch.setattr(g, "run_logged", fake_run_logged)
    monkeypatch.setattr(g, "remote_readings", lambda ctx_, *a: next(readings))

    def fake_run(cmd, **kw):
        if cmd[-1] == "true":
            ssh_timeouts.append(kw.get("timeout"))
            return subprocess.CompletedProcess(cmd, 0, stdout="", stderr="")
        ssh_timeouts.append(kw.get("timeout"))
        return subprocess.CompletedProcess(cmd, 1, stdout="", stderr="")   # build never done

    monkeypatch.setattr(g, "_run", fake_run)
    rec = g.run_leg_with_retry(ctx, "cuda", g.run_cuda, "sha")
    assert clock.now == at(6, 30)          # the wait ended at the wall
    kill_cmds = [c for c in run_logged_cmds if "kill -- -" in c]
    assert len(kill_cmds) == 1 and f"results/{ctx.run_id}/build.pid" in kill_cmds[0]
    assert ssh_timeouts[-1] == pytest.approx(10.0, abs=0.01)  # last ssh bounded by the wall
    assert rec["status"] == "skipped" and "deadline" in rec["reason"]
    assert rec["idle_retry"]["terminal"] == "deadline"
    assert not any("pytest.sh" in c for c in run_logged_cmds)  # never reached the measurement phase


def test_late_nightly_run_skips_source_prep_and_reports_deadline_skips(tmp_path, monkeypatch):
    """Past the wall, the run never touches the network for source prep, and a
    failing source can no longer outrank the wall: four zero-attempt deadline
    skips, rc 0."""
    state_dir = main_env(tmp_path, monkeypatch)

    def forbidden_checkout(ctx):
        raise AssertionError("prepare_checkout must not run past the wall")

    monkeypatch.setattr(g, "prepare_checkout", forbidden_checkout)
    inject_clock(monkeypatch, FakeClock(at(6, 31)))
    calls = []
    install_busy_legs(monkeypatch, calls)
    rc = g.main(["--leg", "all", "--state-dir", str(state_dir),
                 "--work-dir", str(tmp_path / "work"), "--trigger", "launchd"])
    assert rc == 0
    recs = read_records(state_dir)
    assert [r["reason"] for r in recs] == ["nightly deadline 06:30 reached before this leg started"] * 4
    assert calls == []


def test_source_error_still_stands_without_a_deadline(tmp_path, monkeypatch):
    """The red side: with no wall, a failing source prep still produces the
    run's per-leg source errors — the wall's precedence is wall-scoped."""
    state_dir = main_env(tmp_path, monkeypatch)

    def broken_checkout(ctx):
        raise RuntimeError("git fetch failed: network down")

    monkeypatch.setattr(g, "prepare_checkout", broken_checkout)
    calls = []
    install_busy_legs(monkeypatch, calls)
    rc = g.main(["--leg", "all", "--state-dir", str(state_dir),
                 "--work-dir", str(tmp_path / "work"), "--trigger", "manual"])
    assert rc == 1
    recs = read_records(state_dir)
    assert all(r["status"] == "error"
               and r["reason"] == "source: RuntimeError: git fetch failed: network down"
               for r in recs)
    assert calls == []


# --- the wall reaches the cuda entry, and the stop is verified -------------------

def test_entry_probe_is_bounded_by_the_wall_and_voids_past_it(tmp_path, monkeypatch):
    """The connectivity probe is the cuda attempt's first blocking call: it
    takes the remaining wall time as its timeout, and a probe cut by the wall
    is a voided attempt with history — never a plain unreachable skip."""
    clock = FakeClock(at(6, 29, 59))
    ctx = retry_ctx(tmp_path, clock, deadline=at(6, 30))
    seen = {}

    def fake_run(cmd, **kw):
        seen["timeout"] = kw.get("timeout")
        clock.advance(10)                   # the ssh hung until the wall cut it
        raise subprocess.TimeoutExpired(cmd, 10)

    monkeypatch.setattr(g, "_run", fake_run)
    rec = g.run_leg_with_retry(ctx, "cuda", g.run_cuda, "sha")
    assert seen["timeout"] == pytest.approx(1.0, abs=0.01)   # bounded by the wall, not 20
    assert rec["status"] == "skipped" and "deadline" in rec["reason"]
    assert "unreachable" not in rec["reason"]
    ir = rec["idle_retry"]
    assert ir["terminal"] == "deadline" and ir["attempts"] == 1
    assert "reason" not in ir["history"][0]  # no readings yet: nothing is faked


def test_start_receipt_lost_at_the_wall_still_cleans_up(tmp_path, monkeypatch):
    """A lost start receipt does not mean the phase never started: when the
    wall cut the start ssh, the stop interface still runs against the attempt
    directory before the void — the request may have landed."""
    clock = FakeClock(at(6, 29, 55))
    ctx = retry_ctx(tmp_path, clock, deadline=at(6, 30))
    run_logged_cmds = []
    readings = iter([idle_r(), idle_r()])

    def fake_run_logged(ctx_, cmd, log_path, **kw):
        joined = " ".join(str(c) for c in cmd)
        run_logged_cmds.append(joined)
        Path(log_path).touch()
        if "build.sh" in joined:
            clock.advance(10)               # the start ssh hangs until the wall cuts it
            return 124
        return 0                            # the sync steps are quick and succeed

    monkeypatch.setattr(g, "run_logged", fake_run_logged)
    monkeypatch.setattr(g, "_run",
                        lambda c, **kw: subprocess.CompletedProcess(c, 0, stdout="", stderr=""))
    monkeypatch.setattr(g, "remote_readings", lambda ctx_, *a: next(readings))
    rec = g.run_leg_with_retry(ctx, "cuda", g.run_cuda, "sha")
    stop_cmds = [c for c in run_logged_cmds if "kill -- -" in c]
    assert stop_cmds and f"results/{ctx.run_id}/build.pid" in stop_cmds[0]
    assert rec["status"] == "skipped" and "deadline" in rec["reason"]
    assert "could not start" not in rec["reason"]


def route_stop_locally(monkeypatch, ctx, cwd):
    """Run remote_stop_phase's ssh text through a local bash — the same command,
    the real kill semantics, no network."""
    def local_bash(ctx_, cmd, log_path, **kw):
        with open(log_path, "a") as out:
            p = subprocess.run(["bash", "-c", cmd[-1]], cwd=str(cwd), stdout=out,
                               stderr=subprocess.STDOUT, timeout=kw.get("timeout"))
        return p.returncode

    monkeypatch.setattr(g, "run_logged", local_bash)


def wait_until(predicate, timeout_s, interval=0.05):
    """Bounded poll for a condition. CI runners run an order of magnitude
    slower than a dev Mac, so an assertion here must never depend on a fixed
    sleep racing a worker's real lifetime — poll until the mechanism's
    observable effect lands, or fail on the deadline."""
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        if predicate():
            return True
        time.sleep(interval)
    return predicate()


def _pid_gone(pid):
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return True
    return False


IGNORING_WORKER = ("import signal, time; signal.signal(signal.SIGTERM, signal.SIG_IGN); "
                   "time.sleep({sleep}); open({marker!r}, 'w').write('done')")


def test_stop_kills_the_recorded_worker_group_and_verifies(tmp_path, monkeypatch):
    """The real generated pytest script runs in a local sandbox: a SIGTERM-
    ignoring worker under real GNU timeout, in its own recorded group, is dead
    after the graded stop and its work never completes — "stopped" means the
    work ended, not that a signal was sent."""
    home = tmp_path / "home"
    (home / "Codes" / "gate" / "repo").mkdir(parents=True)
    (home / "lumice-env.sh").write_text("")
    marker = tmp_path / "worker-done.txt"
    monkeypatch.setattr(g, "pytest_argv",
                        lambda *a, **k: [sys.executable, "-c", IGNORING_WORKER.format(
                            sleep=30, marker=str(marker))])
    ctx = make_ctx(tmp_path)
    ctx.args.remote_dir = "Codes/gate"
    script = g.remote_phase_script(ctx, "pytest", "sha", "t")
    out_dir = home / "Codes" / "gate" / "results" / "t"
    out_dir.mkdir(parents=True)
    script_file = out_dir / "pytest.sh"
    script_file.write_text(script)
    # The reviewed sandbox shape: the script is a pytest child and its killed
    # session leader stays an unreaped zombie. The stop's verification is
    # zombie-aware, so that reads as terminated work on both platforms.
    subprocess.Popen(["bash", str(script_file)], start_new_session=True, cwd=str(home),
                     env={"HOME": str(home), "PATH": os.environ["PATH"]})
    route_stop_locally(monkeypatch, ctx, home)
    worker_pid_file = out_dir / "pytest-worker.pid"
    assert wait_until(worker_pid_file.exists, 15.0)
    assert g.remote_stop_phase(ctx, "pytest", "t") is True
    worker_pid = int(worker_pid_file.read_text())
    assert wait_until(lambda: _pid_gone(worker_pid), 10.0)   # the stop ended the work
    assert not marker.exists()     # a dead worker cannot write; its 30s lifetime makes
    # any racing completion structurally impossible, on any runner speed


def test_stop_cannot_reach_a_worker_group_it_was_never_given(tmp_path, monkeypatch):
    """The red side of the worker-group record: a timeout-spawned worker in an
    unrecorded group survives a stop that only knows the entry shell's — the
    stop reports success over what it can see while the work completes."""
    work = tmp_path / "results" / "t"
    work.mkdir(parents=True)
    marker = tmp_path / "red-worker-done.txt"
    worker = IGNORING_WORKER.format(sleep=3, marker=str(marker))
    prog = (f'echo $$ > {work}/pytest.pid; '
            f'timeout 30 {sys.executable} -c {worker!r} > /dev/null 2>&1')
    # Same reviewed Popen shape as the stop-kills sandbox above; the stop's
    # zombie-aware verification reads the unreaped session leader as ended work.
    subprocess.Popen(["bash", "-c", prog], start_new_session=True,
                     cwd=str(tmp_path), env={"HOME": str(tmp_path), "PATH": os.environ["PATH"]})
    ctx = make_ctx(tmp_path)
    ctx.args.remote_dir = str(tmp_path)
    route_stop_locally(monkeypatch, ctx, tmp_path)
    pid_file = work / "pytest.pid"
    assert wait_until(pid_file.exists, 15.0)
    assert g.remote_stop_phase(ctx, "pytest", "t") is True   # the stop saw only the shell
    assert wait_until(marker.exists, 30.0)
    # the work completed: it was out of the stop's reach (deadline = the worker's
    # own timeout budget; a slow runner only delays the same observable outcome)


def test_stop_reports_unverifiable_state_instead_of_success(tmp_path, monkeypatch):
    """No pid file and no done marker: the phase's state cannot be confirmed —
    non-zero exit and False, never a silent success; a done marker is the one
    clean nothing-to-stop."""
    ctx = make_ctx(tmp_path)
    ctx.args.remote_dir = str(tmp_path)
    route_stop_locally(monkeypatch, ctx, tmp_path)
    assert g.remote_stop_phase(ctx, "pytest", "empty") is False
    (tmp_path / "results" / "done-case").mkdir(parents=True)
    (tmp_path / "results" / "done-case" / "pytest.done").write_text("")
    assert g.remote_stop_phase(ctx, "pytest", "done-case") is True


def test_stop_does_not_count_another_phase_pid_as_its_own(tmp_path, monkeypatch):
    """A finished build's pid file is not evidence about the pytest phase: a
    stop for pytest in a directory holding only build.pid reports unverifiable
    (exit 4) — never a borrowed 'stopped' while the pytest phase may still
    start."""
    (tmp_path / "results" / "t").mkdir(parents=True)
    (tmp_path / "results" / "t" / "build.pid").write_text("99999999")
    (tmp_path / "results" / "t" / "build.done").write_text("")
    ctx = make_ctx(tmp_path)
    ctx.args.remote_dir = str(tmp_path)
    route_stop_locally(monkeypatch, ctx, tmp_path)
    assert g.remote_stop_phase(ctx, "pytest", "t") is False


# --- terminal adjudication ownership: the wrapper, not each return branch --------

def test_collect_remote_probe_keeps_its_plain_budget_past_the_wall(tmp_path, monkeypatch):
    """The recovery path has no wall — not even a passed one: its connectivity
    probe keeps the plain connect budget when the run's clock is already past
    the nightly deadline, so an existing remote result can still be collected."""
    clock = FakeClock(at(7, 0))
    ctx = retry_ctx(tmp_path, clock, deadline=at(6, 30))
    seen = {}

    def fake_run(cmd, **kw):
        seen["timeout"] = kw.get("timeout")
        raise subprocess.TimeoutExpired(cmd, 20)

    monkeypatch.setattr(g, "_run", fake_run)
    rec = g.run_cuda(ctx, "sha", resume_rdir="r")
    assert seen["timeout"] == ctx.args.ssh_connect_timeout + 15   # plain budget, not wall-cut
    assert rec["status"] == "skipped" and rec["reason"] == "unreachable (TimeoutExpired)"
    assert "idle_retry" not in rec and "deadline" not in rec["reason"]


def test_fresh_probe_past_the_wall_is_voided_not_unreachable(tmp_path, monkeypatch):
    """The fresh attempt's probe past the wall: bounded to ~zero by the wall and
    voided by the stage guard — never a plain unreachable skip. (Driven without
    the wrapper here: an attempt that never starts would be caught by the
    wrapper's zero-attempt gate before the probe; this is the in-leg adjudication.)"""
    clock = FakeClock(at(7, 0))
    ctx = retry_ctx(tmp_path, clock, deadline=at(6, 30))
    seen = {}

    def fake_run(cmd, **kw):
        seen["timeout"] = kw.get("timeout")
        raise subprocess.TimeoutExpired(cmd, 0)

    monkeypatch.setattr(g, "_run", fake_run)
    with pytest.raises(g.DeadlineVoid):
        g.run_cuda(ctx, "sha")
    assert seen["timeout"] == pytest.approx(0.0, abs=0.01)


def test_build_failed_fetch_crossing_the_wall_is_voided(tmp_path, monkeypatch):
    """A build that failed before the wall but whose diagnostic fetch crossed
    it is a voided attempt — the wrapper owns the terminal adjudication, so no
    error record can be minted past the deadline."""
    clock = FakeClock(at(6, 29, 59))
    ctx = retry_ctx(tmp_path, clock, deadline=at(6, 30))
    marker_state = {ctx.run_id: 2}          # the build failed in its own directory
    readings = iter([idle_r(), idle_r()])
    run_logged_cmds = []

    def fake_run_logged(ctx_, cmd, log_path, **kw):
        joined = " ".join(str(c) for c in cmd)
        run_logged_cmds.append(joined)
        Path(log_path).touch()
        if "rsync" in joined:
            clock.advance(10)               # the diagnostic fetch eats the last second
        return 0

    monkeypatch.setattr(g, "run_logged", fake_run_logged)
    monkeypatch.setattr(g, "remote_readings", lambda ctx_, *a: next(readings))

    def fake_run(cmd, **kw):
        if cmd[-1] == "true":
            return subprocess.CompletedProcess(cmd, 0, stdout="", stderr="")
        for rdir, exit_code in marker_state.items():
            if f"results/{rdir}/build.done" in cmd[-1]:
                return subprocess.CompletedProcess(cmd, 0, stdout=f"{exit_code}\n", stderr="")
        return subprocess.CompletedProcess(cmd, 1, stdout="", stderr="")

    monkeypatch.setattr(g, "_run", fake_run)
    rec = g.run_leg_with_retry(ctx, "cuda", g.run_cuda, "sha")
    assert rec["status"] == "skipped" and "deadline" in rec["reason"]
    assert "remote build failed" not in rec["reason"]
    ir = rec["idle_retry"]
    assert ir["terminal"] == "deadline" and ir["attempts"] == 1
    entry = ir["history"][0]
    assert entry["voided"] is True and "idle_pre" in entry


def test_build_error_before_the_wall_still_stands(tmp_path, monkeypatch):
    """The red side: a build failure whose diagnostics are collected before the
    wall keeps its error record — the void is wall-scoped, not error-scoped."""
    clock = FakeClock(at(6, 29, 59))
    ctx = retry_ctx(tmp_path, clock, deadline=at(6, 30))
    marker_state = {ctx.run_id: 2}
    readings = iter([idle_r(), idle_r()])

    def fake_run_logged(ctx_, cmd, log_path, **kw):
        Path(log_path).touch()
        return 0                            # the fetch is quick and stays inside the wall

    monkeypatch.setattr(g, "run_logged", fake_run_logged)
    monkeypatch.setattr(g, "remote_readings", lambda ctx_, *a: next(readings))

    def fake_run(cmd, **kw):
        if cmd[-1] == "true":
            return subprocess.CompletedProcess(cmd, 0, stdout="", stderr="")
        for rdir, exit_code in marker_state.items():
            if f"results/{rdir}/build.done" in cmd[-1]:
                return subprocess.CompletedProcess(cmd, 0, stdout=f"{exit_code}\n", stderr="")
        return subprocess.CompletedProcess(cmd, 1, stdout="", stderr="")

    monkeypatch.setattr(g, "_run", fake_run)
    rec = g.run_leg_with_retry(ctx, "cuda", g.run_cuda, "sha")
    assert rec["status"] == "error" and "remote build failed (exit 2)" in rec["reason"]
    assert "idle_retry" not in rec


# --- the remote pytest budget keeps the wall's fractional precision --------------

def test_remote_pytest_budget_keeps_fractional_wall_precision(tmp_path):
    """The remaining wall time reaches the remote worker at full precision: an
    int() truncation would kill it before the wall and let the 124 read as a
    plain test error; a sub-millisecond remainder never becomes the
    timeout-disabling exact 0."""
    ctx = make_ctx(tmp_path)
    ctx.args.pytest_timeout = 10.0
    clock = FakeClock(at(6, 29, 57))
    ctx.nightly_deadline = clock.now + dt.timedelta(seconds=2.8)
    ctx.clock, ctx.sleep = clock.clock, clock.sleep
    assert "timeout 2.800" in g.remote_phase_script(ctx, "pytest", "sha", "r")
    ctx2 = make_ctx(tmp_path)
    ctx2.args.pytest_timeout = 1.0        # the worker's own budget is the smaller one
    assert "timeout 1.000" in g.remote_phase_script(ctx2, "pytest", "sha", "r")
    clock3 = FakeClock(at(7, 0))
    ctx3 = make_ctx(tmp_path)
    ctx3.nightly_deadline = at(6, 30)
    ctx3.clock, ctx3.sleep = clock3.clock, clock3.sleep
    assert "timeout 0.001" in g.remote_phase_script(ctx3, "pytest", "sha", "r")
    assert "timeout 0 " not in g.remote_phase_script(ctx3, "pytest", "sha", "r")


def test_remote_worker_killed_at_the_wall_is_voided_not_error(tmp_path, monkeypatch):
    """The fractional budget kills the worker exactly at the wall: the wait
    ends there, the stop runs, and the attempt is voided with history — the
    124 never becomes 'pytest-did-not-finish'."""
    clock = FakeClock(at(6, 29, 57))
    deadline = clock.now + dt.timedelta(seconds=2.8)
    ctx = retry_ctx(tmp_path, clock, deadline=deadline, remote_poll=1.0)
    readings = iter([idle_r(), idle_r()])
    run_logged_cmds = []

    def fake_run_logged(ctx_, cmd, log_path, **kw):
        run_logged_cmds.append(" ".join(str(c) for c in cmd))
        Path(log_path).touch()
        return 0

    monkeypatch.setattr(g, "run_logged", fake_run_logged)
    monkeypatch.setattr(g, "remote_readings", lambda ctx_, *a: next(readings))

    def fake_run(cmd, **kw):
        if cmd[-1] == "true":
            return subprocess.CompletedProcess(cmd, 0, stdout="", stderr="")
        if "build.done" in cmd[-1]:
            return subprocess.CompletedProcess(cmd, 0, stdout="0\n", stderr="")
        if "pytest.done" in cmd[-1]:
            # the worker dies exactly at the wall (the fractional budget), not before
            if clock.now >= deadline:
                return subprocess.CompletedProcess(cmd, 0, stdout="124\n", stderr="")
            return subprocess.CompletedProcess(cmd, 1, stdout="", stderr="")
        return subprocess.CompletedProcess(cmd, 1, stdout="", stderr="")

    monkeypatch.setattr(g, "_run", fake_run)
    rec = g.run_leg_with_retry(ctx, "cuda", g.run_cuda, "sha")
    assert clock.now >= deadline            # the wait ended at the wall
    assert rec["status"] == "skipped" and "deadline" in rec["reason"]
    assert "did-not-finish" not in rec["reason"] and "pytest_exit" not in rec
    assert rec["idle_retry"]["terminal"] == "deadline"
    assert any("kill -- -" in c for c in run_logged_cmds)   # the stop ran


def test_remote_pytest_own_budget_expiry_still_classifies_error(tmp_path, monkeypatch):
    """The red control: when the worker's own budget expires first (the wall is
    far), the plain timeout error stands — the void is wall-scoped."""
    clock = FakeClock(at(3, 30))
    ctx = retry_ctx(tmp_path, clock, deadline=at(6, 30))
    ctx.args.pytest_timeout = 1.0
    readings = iter([idle_r(), idle_r(), idle_r(), idle_r()])

    def fake_run_logged(ctx_, cmd, log_path, **kw):
        Path(log_path).touch()
        return 0

    monkeypatch.setattr(g, "run_logged", fake_run_logged)
    monkeypatch.setattr(g, "remote_readings", lambda ctx_, *a: next(readings))
    pytest_done_fired = [False]

    def fake_run(cmd, **kw):
        if cmd[-1] == "true":
            return subprocess.CompletedProcess(cmd, 0, stdout="", stderr="")
        if "build.done" in cmd[-1]:
            return subprocess.CompletedProcess(cmd, 0, stdout="0\n", stderr="")
        if "pytest.done" in cmd[-1]:
            if not pytest_done_fired[0]:
                pytest_done_fired[0] = True
                clock.advance(1.5)          # the worker dies of its own 1s budget
            return subprocess.CompletedProcess(cmd, 0, stdout="124\n", stderr="")
        return subprocess.CompletedProcess(cmd, 1, stdout="", stderr="")

    monkeypatch.setattr(g, "_run", fake_run)
    rec = g.run_leg_with_retry(ctx, "cuda", g.run_cuda, "sha")
    assert clock.now < at(6, 30)            # the wall never came near
    assert rec["status"] == "error" and "pytest-did-not-finish" in rec["reason"]
    assert "idle_retry" not in rec
