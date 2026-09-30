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
    assert json.loads(line) == {"id": "t.py::slow", "job": JOB, "ci_seconds": 35, "reason": "TODO"}


def test_exactly_at_threshold_is_green():
    errors, _ = run({"t.py::edge": T}, [])
    assert errors == []


def test_registered_for_another_job_does_not_cover_this_job():
    errors, _ = run({"t.py::slow": 45}, [entry("t.py::slow", 50, job="e2e-test")])
    assert any("t.py::slow" in e and "not registered" in e for e in errors)


# --- rule 2: registered but slowed down -------------------------------------

def test_registered_within_slowdown_factor_is_green():
    errors, _ = run({"t.py::slow": 2 * 40}, [entry("t.py::slow", 40)])
    assert errors == []


def test_registered_beyond_slowdown_factor_is_red():
    errors, _ = run({"t.py::slow": 2 * 40 + 0.5}, [entry("t.py::slow", 40)])
    assert len(errors) == 1 and "more than 2x" in errors[0]


def test_registered_but_now_fast_is_a_notice_not_red():
    errors, notices = run({"t.py::slow": 3.0, "t.py::b": 50}, [entry("t.py::slow", 40), entry("t.py::b", 50)])
    assert errors == []
    assert any("t.py::slow" in n for n in notices)


# --- rule 3: stale entries -------------------------------------------------

def test_registered_but_absent_is_red_and_names_sibling_parametrizations():
    durations = {"t.py::f[new]": 45, "t.py::keep": 50}
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
        {"entries": []},
        {"version": 2, "entries": []},
        {"version": 1, "entries": [{"id": "a", "job": JOB, "ci_seconds": 40}]},
        {"version": 1, "entries": [{**entry("a", 40), "note": "x"}]},
        {"version": 1, "entries": [entry("a", "40")]},
        {"version": 1, "entries": [entry("a", 0)]},
        {"version": 1, "entries": [entry("a", 40), entry("a", 45)]},
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
    registry = write_registry(tmp_path, {"version": 1})
    assert ctd.main(["--job", JOB, "--report", str(report), "--registry", str(registry)]) == 1
    assert "::error::" in capsys.readouterr().out


# --- report merging and the CLI ---------------------------------------------

def test_two_reports_of_one_job_are_merged(tmp_path, capsys):
    phase1 = tmp_path / "p1.json"
    phase2 = tmp_path / "p2.json"
    phase1.write_text(json.dumps({"a.py::t": 40, "::<fixture:f>": 3}), encoding="utf-8")
    phase2.write_text(json.dumps({"perf.py::t": 90, "::<fixture:f>": 5}), encoding="utf-8")
    assert ctd.load_reports([phase1, phase2]) == {"a.py::t": 40, "perf.py::t": 90, "::<fixture:f>": 5}
    registry = write_registry(tmp_path, {"version": 1, "entries": [entry("a.py::t", 40)]})
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
