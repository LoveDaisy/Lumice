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
import subprocess
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
    """A minimal Ctx for run_metal tests: a quiet machine lives behind the doubles."""
    args = argparse.Namespace(dry_run=False, cooldown_timeout=0.0, cooldown_poll=0.0,
                              threshold_override=None, pytest_timeout=10,
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
