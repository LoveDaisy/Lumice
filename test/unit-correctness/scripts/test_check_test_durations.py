"""Regression net for `scripts/check_test_durations.py` and its pytest plugin.

Every rule of the gate fails in the quiet direction if it breaks: an unregistered
slow test passes, a stale entry stays, a registry typo is silently read as "no
entries". So each rule below is pinned by a red case next to its green one.

The plugin tests run a tiny pytest session in a subprocess, because what matters
is what the plugin writes from a real session (setup included, shared fixtures
split out, skipped tests present), not what its functions return in isolation.
"""
from __future__ import annotations

import json
import os
import subprocess
import sys
import time
from pathlib import Path

import pytest

SCRIPTS = Path(__file__).resolve().parents[3] / "scripts"
sys.path.insert(0, str(SCRIPTS))

import check_test_durations as ctd  # noqa: E402

JOB = "E2E Slow (Ubuntu x86_64)"
T = ctd.THRESHOLD_S


def entry(test_id: str, seconds: float, job: str = JOB, reason: str = "guards X; cannot be faster because Y"):
    return {"id": test_id, "job": job, "ci_seconds": seconds, "reason": reason}


def run(durations: dict[str, float], entries: list[dict], job: str = JOB):
    return ctd.check(job, durations, entries)


# --- rule 1: unregistered over threshold ------------------------------------

def test_unregistered_over_threshold_is_red_and_prints_a_pasteable_skeleton():
    errors, _ = run({"t.py::slow": T + 0.1, "t.py::fast": 1.0}, [entry("t.py::other", 40)])
    assert len(errors) == 2  # the slow one + t.py::other being absent from the report
    slow = next(e for e in errors if "t.py::slow" in e)
    line = slow.splitlines()[-1].strip()
    assert json.loads(line) == {"id": "t.py::slow", "job": JOB, "ci_seconds": ctd.round_up(T + 0.1), "reason": "TODO"}
    assert ctd.round_up(T + 0.1) > T


def test_exactly_at_threshold_is_green():
    errors, _ = run({"t.py::edge": T}, [])
    assert errors == []


def test_registered_for_another_job_does_not_cover_this_job():
    errors, _ = run({"t.py::slow": T + 15}, [entry("t.py::slow", T + 20, job="e2e-test")])
    assert any("t.py::slow" in e and "not registered" in e for e in errors)


# --- rule 2: registered but slowed down -------------------------------------

def test_registered_within_slowdown_factor_is_green():
    errors, _ = run({"t.py::slow": 2 * 40}, [entry("t.py::slow", 40)])
    assert errors == []


def test_registered_beyond_slowdown_factor_is_red():
    errors, _ = run({"t.py::slow": 2 * 40 + 0.5}, [entry("t.py::slow", 40)])
    assert len(errors) == 1 and "more than 2x" in errors[0]


def test_registered_but_now_much_faster_is_a_notice_not_red():
    errors, notices = run({"t.py::slow": 19.9, "t.py::b": 50}, [entry("t.py::slow", 40), entry("t.py::b", 50)])
    assert errors == []
    assert [n for n in notices if "t.py::" in n] == [n for n in notices if "t.py::slow" in n] != []


def test_registered_under_threshold_but_near_its_entry_says_nothing():
    errors, notices = run({"t.py::a": 25.0}, [entry("t.py::a", 30)])
    assert errors == [] and notices == []


# --- rule 3: stale entries -------------------------------------------------

def test_registered_but_absent_is_red_and_names_sibling_parametrizations():
    durations = {"t.py::f[new]": T + 15, "t.py::keep": 50}
    errors, _ = run(durations, [entry("t.py::f[old]", 45), entry("t.py::keep", 50)])
    assert len(errors) == 2
    stale = next(e for e in errors if "t.py::f[old]" in e)
    assert "did not run" in stale and "t.py::f[new]" in stale


def test_skipped_test_reads_zero_and_is_not_stale():
    errors, notices = run({"t.py::metal_only": 0.0}, [entry("t.py::metal_only", 60)])
    assert errors == []
    assert notices  # under threshold -> notice only


def test_job_name_mismatch_gets_one_dedicated_message():
    entries = [entry("t.py::a", 40, job="E2E Slow (Ubuntu)"), entry("t.py::b", 40, job="E2E Slow (Ubuntu)")]
    errors, _ = run({"t.py::a": 41, "t.py::b": 41}, entries, job="E2E Slow (Ubuntu)")
    assert errors == []
    errors, _ = run({"x.py::c": 1}, entries, job="E2E Slow (Ubuntu)")
    assert len(errors) == 1 and "--job" in errors[0]


# --- rule 4: reasons and schema ---------------------------------------------

@pytest.mark.parametrize("reason", ["", "   ", "TODO"])
def test_missing_or_placeholder_reason_is_red(reason):
    errors, _ = run({"t.py::slow": 40}, [entry("t.py::slow", 40, reason=reason)])
    assert len(errors) == 1 and "no reason" in errors[0]


def write_registry(tmp_path: Path, payload) -> Path:
    path = tmp_path / "registry.json"
    path.write_text(json.dumps(payload), encoding="utf-8")
    return path


@pytest.mark.parametrize(
    "payload",
    [
        {"entries": [], "phase_budgets": []},
        {"version": 1, "entries": [], "phase_budgets": []},
        {"version": 2, "entries": [{"id": "a", "job": JOB, "ci_seconds": 40}], "phase_budgets": []},
        {"version": 2, "entries": [{**entry("a", 40), "note": "x"}], "phase_budgets": []},
        {"version": 2, "entries": [entry("a", "40")], "phase_budgets": []},
        {"version": 2, "entries": [entry("a", 0)], "phase_budgets": []},
        {"version": 2, "entries": [entry("a", 40), entry("a", 45)], "phase_budgets": []},
    ],
    ids=["no-version", "wrong-version", "missing-field", "unknown-field", "string-seconds", "zero-seconds",
         "duplicate"],
)
def test_malformed_registry_is_rejected(tmp_path, payload):
    with pytest.raises(ctd.RegistryError):
        ctd.load_registry(write_registry(tmp_path, payload))


def test_main_is_red_on_malformed_registry(tmp_path, capsys):
    report = tmp_path / "r.json"
    report.write_text("{}", encoding="utf-8")
    registry = write_registry(tmp_path, {"version": 2})
    assert ctd.main(["--job", JOB, "--report", str(report), "--registry", str(registry)]) == 1
    assert "::error::" in capsys.readouterr().out


# --- report merging and the CLI ---------------------------------------------

def test_two_reports_of_one_job_are_merged(tmp_path, capsys):
    phase1 = tmp_path / "p1.json"
    phase2 = tmp_path / "p2.json"
    phase1.write_text(json.dumps({"a.py::t": 40, "::<fixture:f>": 3}), encoding="utf-8")
    phase2.write_text(json.dumps({"perf.py::t": 90, "::<fixture:f>": 5}), encoding="utf-8")
    assert ctd.load_reports([phase1, phase2]) == {"a.py::t": 40, "perf.py::t": 90, "::<fixture:f>": 5}
    registry = write_registry(tmp_path, {"version": 2, "entries": [entry("a.py::t", 40)], "phase_budgets": []})
    argv = ["--job", JOB, "--report", str(phase1), "--report", str(phase2), "--registry", str(registry)]
    assert ctd.main(argv) == 1
    out = capsys.readouterr().out
    assert "perf.py::t" in out and "a.py::t took" not in out


def test_check_runs_well_under_a_second_on_a_suite_sized_input():
    durations = {f"test/x/test_{i}.py::test_case[{j}]": float(j) for i in range(300) for j in range(10)}
    entries = [entry(f"test/x/test_{i}.py::test_case[9]", 10) for i in range(300)]
    start = time.perf_counter()
    run(durations, entries)
    assert time.perf_counter() - start < 1.0


# --- cumulative phase budgets -------------------------------------------------

def phase_budget(phase: str, limit: float, members=None, job: str = JOB, reason: str = "bounds the PR path"):
    result = {"job": job, "phase": phase, "max_seconds": limit, "reason": reason}
    if members is not None:
        result["members"] = members
    return result


def test_phase_budget_catches_many_individually_cheap_tests():
    durations = {f"t.py::case_{i}": 10.0 for i in range(50)}
    test_errors, _ = run(durations, [])
    phase_errors, _ = ctd.check_phases(JOB, {"tests": 501.0}, [phase_budget("tests", 500)])
    assert test_errors == []
    assert len(phase_errors) == 1 and "over its 500s limit" in phase_errors[0]


def test_phase_boundary_and_aggregate_are_checked_without_worker_second_summing():
    budgets = [
        phase_budget("build", 300),
        phase_budget("ctest", 200),
        phase_budget("critical-path", 449, members=["build", "ctest"]),
    ]
    errors, notices = ctd.check_phases(JOB, {"build": 250, "ctest": 200}, budgets)
    assert len(errors) == 1 and "critical-path" in errors[0]
    assert any("ctest" in notice and "200.0s / 200s" in notice for notice in notices)


def test_missing_unknown_and_duplicate_phase_reports_are_red(tmp_path):
    budgets = [phase_budget("tests", 60)]
    errors, _ = ctd.check_phases(JOB, {}, budgets)
    assert len(errors) == 1 and "missing reports" in errors[0]
    errors, _ = ctd.check_phases(JOB, {"typo": 1}, budgets)
    assert any("no registered phase budget" in error for error in errors)

    first = tmp_path / "one.json"
    second = tmp_path / "two.json"
    payload = {"phase": "tests", "seconds": 1.0, "exit_code": 0}
    first.write_text(json.dumps(payload), encoding="utf-8")
    second.write_text(json.dumps(payload), encoding="utf-8")
    with pytest.raises(ValueError, match="duplicate phase"):
        ctd.load_phase_reports([first, second])


def test_phase_report_and_budget_schema_reject_invalid_values(tmp_path):
    registry = {
        "version": 2,
        "entries": [],
        "phase_budgets": [phase_budget("aggregate", 10, members=["tests", "tests"])],
    }
    with pytest.raises(ctd.RegistryError, match="duplicates"):
        ctd.load_registry(write_registry(tmp_path, registry))

    report = tmp_path / "phase.json"
    report.write_text(json.dumps({"phase": "tests", "seconds": float("nan"), "exit_code": 0}), encoding="utf-8")
    with pytest.raises(ValueError, match="seconds"):
        ctd.load_phase_reports([report])


# --- the plugin, from a real pytest session ---------------------------------

PROBE = '''
import time, pytest

@pytest.fixture(scope="module")
def shared():
    time.sleep(0.4)

@pytest.mark.parametrize("i", range(3))
def test_uses_shared(shared, i):
    pass

@pytest.fixture
def per_test():
    time.sleep(0.3)

def test_heavy_setup(per_test):
    pass

def test_skipped():
    pytest.skip("not on this runner")
'''


def run_probe(tmp_path: Path, *extra: str) -> subprocess.CompletedProcess:
    (tmp_path / "test_probe.py").write_text(PROBE, encoding="utf-8")
    env = {**os.environ, "PYTHONPATH": str(SCRIPTS)}
    return subprocess.run(
        [sys.executable, "-m", "pytest", "-q", "-p", "no:cacheprovider", "-p", "duration_report_plugin",
         *extra, "test_probe.py"],
        cwd=tmp_path, env=env, capture_output=True, text=True, check=False,
    )


def test_plugin_writes_setup_inclusive_totals_with_shared_fixtures_split_out(tmp_path):
    result = run_probe(tmp_path, "--duration-report=out.json")
    assert result.returncode == 0, result.stdout + result.stderr
    data = json.loads((tmp_path / "out.json").read_text(encoding="utf-8"))
    assert set(data) == {
        "test_probe.py::test_uses_shared[0]",
        "test_probe.py::test_uses_shared[1]",
        "test_probe.py::test_uses_shared[2]",
        "test_probe.py::test_heavy_setup",
        "test_probe.py::test_skipped",
        "test_probe.py::<fixture:shared>",
    }
    assert data["test_probe.py::<fixture:shared>"] >= 0.4
    assert all(data[f"test_probe.py::test_uses_shared[{i}]"] < 0.2 for i in range(3))
    assert data["test_probe.py::test_heavy_setup"] >= 0.3  # function-scoped setup stays on the test
    assert data["test_probe.py::test_skipped"] < 0.2


def test_plugin_is_inert_without_the_option(tmp_path):
    result = run_probe(tmp_path)
    assert result.returncode == 0, result.stdout + result.stderr
    assert sorted(p.name for p in tmp_path.iterdir() if p.suffix == ".json") == []


def test_plugin_under_xdist_writes_one_complete_file(tmp_path):
    pytest.importorskip("xdist")
    result = run_probe(tmp_path, "--duration-report=out.json", "-n", "2")
    assert result.returncode == 0, result.stdout + result.stderr
    data = json.loads((tmp_path / "out.json").read_text(encoding="utf-8"))
    assert len([k for k in data if "<fixture:" not in k]) == 5
    assert data["test_probe.py::<fixture:shared>"] >= 0.4
    assert all(data[f"test_probe.py::test_uses_shared[{i}]"] < 0.2 for i in range(3))
