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
        {"version": 4, "entries": [{"id": "a", "job": JOB, "ci_seconds": 40}], "phase_budgets": []},
        {"version": 4, "entries": [{**entry("a", 40), "note": "x"}], "phase_budgets": []},
        {"version": 4, "entries": [entry("a", "40")], "phase_budgets": []},
        {"version": 4, "entries": [entry("a", 0)], "phase_budgets": []},
        {"version": 4, "entries": [entry("a", 40), entry("a", 45)], "phase_budgets": []},
    ],
    ids=["no-version", "wrong-version", "missing-field", "unknown-field", "string-seconds", "zero-seconds",
         "duplicate"],
)
def test_malformed_registry_is_rejected(tmp_path, payload):
    with pytest.raises(ctd.RegistryError):
        ctd.load_registry(write_registry(tmp_path, payload))


def test_v2_registry_is_rejected_after_the_per_miss_schema_change(tmp_path):
    # A complete, well-formed v2 registry — the shape every job ran on before the
    # per-miss budget — must fail loudly rather than be read with its per_miss
    # entries silently ignored.
    payload = {"version": 2, "entries": [], "phase_budgets": [phase_budget("build", 420)]}
    with pytest.raises(ctd.RegistryError, match="version 2 is not 4"):
        ctd.load_registry(write_registry(tmp_path, payload))


def test_v3_registry_is_rejected_after_the_machine_factor_schema_change(tmp_path):
    # Same migration discipline, one version later: a complete v3 registry —
    # the shape every ctest budget ran on before machine normalization — must
    # fail loudly rather than be read with its per-test semantics silently
    # reinterpreted.
    payload = {"version": 3, "entries": [], "phase_budgets": [phase_budget("ctest", 480)]}
    with pytest.raises(ctd.RegistryError, match="version 3 is not 4"):
        ctd.load_registry(write_registry(tmp_path, payload))


def test_main_is_red_on_malformed_registry(tmp_path, capsys):
    report = tmp_path / "r.json"
    report.write_text("{}", encoding="utf-8")
    registry = write_registry(tmp_path, {"version": 4})
    assert ctd.main(["--job", JOB, "--report", str(report), "--registry", str(registry)]) == 1
    assert "::error::" in capsys.readouterr().out


# --- report merging and the CLI ---------------------------------------------

def test_two_reports_of_one_job_are_merged(tmp_path, capsys):
    phase1 = tmp_path / "p1.json"
    phase2 = tmp_path / "p2.json"
    phase1.write_text(json.dumps({"a.py::t": 40, "::<fixture:f>": 3}), encoding="utf-8")
    phase2.write_text(json.dumps({"perf.py::t": 90, "::<fixture:f>": 5}), encoding="utf-8")
    assert ctd.load_reports([phase1, phase2]) == {"a.py::t": 40, "perf.py::t": 90, "::<fixture:f>": 5}
    registry = write_registry(tmp_path, {"version": 4, "entries": [entry("a.py::t", 40)], "phase_budgets": []})
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
    phase_errors, _, _ = ctd.check_phases(JOB, {"tests": 501.0}, [phase_budget("tests", 500)])
    assert test_errors == []
    assert len(phase_errors) == 1 and "over its 500s limit" in phase_errors[0]


def test_phase_boundary_and_aggregate_are_checked_without_worker_second_summing():
    budgets = [
        phase_budget("build", 300),
        phase_budget("ctest", 200),
        phase_budget("critical-path", 449, members=["build", "ctest"]),
    ]
    errors, notices, warnings = ctd.check_phases(JOB, {"build": 250, "ctest": 200}, budgets)
    assert len(errors) == 1 and "critical-path" in errors[0]
    assert warnings == []
    assert any("ctest" in notice and "200.0s / 200s" in notice for notice in notices)


def test_cold_build_exception_accepts_only_build_budgets_and_preserves_raw_cost():
    budgets = [
        phase_budget("build", 420),
        phase_budget("ctest", 480),
        phase_budget("build+ctest", 720, members=["build", "ctest"]),
    ]
    phases = {"build": 477.375, "ctest": 398.109}
    errors, _, warnings = ctd.check_phases(JOB, phases, budgets)
    assert len(errors) == 2
    assert warnings == []

    errors, notices, warnings = ctd.check_phases(JOB, phases, budgets, allow_cold_build=True)
    assert errors == []
    assert len(warnings) == 2
    assert all("accepted with cold-build exception" in warning for warning in warnings)
    assert any("477.4s" in warning and "420s limit" in warning for warning in warnings)
    assert any("875.5s" in warning and "720s limit" in warning for warning in warnings)
    assert any("cold-build exception is enabled" in notice for notice in notices)


def test_cold_build_exception_does_not_accept_test_or_non_build_aggregate_overruns():
    budgets = [
        phase_budget("build", 420),
        phase_budget("ctest", 480),
        phase_budget("build+ctest", 720, members=["build", "ctest"]),
        phase_budget("test-total", 450, members=["ctest"]),
    ]
    errors, _, warnings = ctd.check_phases(
        JOB, {"build": 477.375, "ctest": 500.0}, budgets, allow_cold_build=True
    )
    assert any("phase budget 'ctest'" in error for error in errors)
    assert any("phase budget 'test-total'" in error for error in errors)
    assert not any("phase budget 'build'" in error for error in errors)
    assert {warning.split(" for job", 1)[0] for warning in warnings} == {
        "phase budget 'build'",
        "phase budget 'build+ctest'",
    }


def test_nonzero_phase_exit_is_red_before_any_budget_exception(tmp_path):
    report = tmp_path / "phase.json"
    report.write_text(json.dumps({"phase": "build", "seconds": 477.375, "exit_code": 2}), encoding="utf-8")
    with pytest.raises(ValueError, match="phase 'build' exited with code 2"):
        ctd.load_phase_reports([report])


def test_cli_writes_the_same_cold_build_exception_to_log_and_summary(tmp_path, capsys):
    report = tmp_path / "build.json"
    report.write_text(json.dumps({"phase": "build", "seconds": 477.375, "exit_code": 0}), encoding="utf-8")
    registry = write_registry(
        tmp_path,
        {"version": 4, "entries": [], "phase_budgets": [phase_budget("build", 420)]},
    )
    summary = tmp_path / "summary.md"
    summary.write_text("audit evidence\n", encoding="utf-8")
    argv = [
        "--job", JOB,
        "--phase-report", str(report),
        "--registry", str(registry),
        "--allow-cold-build",
        "--summary", str(summary),
    ]
    assert ctd.main(argv) == 0
    output = capsys.readouterr().out
    summary_text = summary.read_text(encoding="utf-8")
    expected = "took 477.4s (build), over its 420s limit; accepted with cold-build exception"
    assert expected in output
    assert expected in summary_text
    assert summary_text.startswith("audit evidence\n")
    assert "Cold-build exception: `enabled`" in summary_text
    assert "accepted with cold-build exception" in output.splitlines()[-1]


def test_missing_unknown_and_duplicate_phase_reports_are_red(tmp_path):
    budgets = [phase_budget("tests", 60)]
    errors, _, _ = ctd.check_phases(JOB, {}, budgets)
    assert len(errors) == 1 and "missing reports" in errors[0]
    errors, _, _ = ctd.check_phases(JOB, {"typo": 1}, budgets)
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
        "version": 4,
        "entries": [],
        "phase_budgets": [phase_budget("aggregate", 10, members=["tests", "tests"])],
    }
    with pytest.raises(ctd.RegistryError, match="duplicates"):
        ctd.load_registry(write_registry(tmp_path, registry))

    report = tmp_path / "phase.json"
    report.write_text(json.dumps({"phase": "tests", "seconds": float("nan"), "exit_code": 0}), encoding="utf-8")
    with pytest.raises(ValueError, match="seconds"):
        ctd.load_phase_reports([report])


# --- per-miss build budgets (registry v3) -------------------------------------
#
# The build phase's two conditions: the adaptive per-miss limit is the
# authoritative ruler, the absolute max_seconds only the disaster backstop.
# Every rule below is pinned against the five real reds of the 420s era.

PER_MISS_BUDGET = {
    "job": JOB,
    "phase": "build",
    "max_seconds": 1800,
    "per_miss": {"overhead_seconds": 30, "max_seconds_per_miss": 5.0},
    "reason": "bounds the build phase per compile unit of change",
}

# (seconds, misses) recovered from each run's Ubuntu x86_64 job log:
# `measure_ci_phase: build took <s>s` and the ccache statistics' `Misses: n / m`.
# All five had zero test failures — every one was the ruler, not the tests.
RED_RUNS_OF_THE_420S_ERA = [
    (436.608, 156),  # run 38124925035, PR #483 (GUI closure)
    (421.150, 163),  # run 38254590460
    (552.887, 162),  # run 38320486746 (main push after #484)
    (602.677, 208),  # run 39061707245 (raypath/analytic closure)
    (490.760, 121),  # run 39158418638, PR #513 (GUI closure, worst s/miss 4.06)
]


def per_miss_phases(seconds: float) -> dict[str, float]:
    return {"build": seconds}


def run_per_miss(seconds: float, misses: int | None, budget=None, allow_cold_build=False):
    reports = None if misses is None else {"build": {"tool": "ccache", "misses": misses}}
    return ctd.check_phases(
        JOB, per_miss_phases(seconds), [budget or PER_MISS_BUDGET],
        allow_cold_build=allow_cold_build, cache_reports=reports,
    )


def test_ac3_the_five_red_runs_replay_green_under_both_conditions():
    for seconds, misses in RED_RUNS_OF_THE_420S_ERA:
        errors, notices, warnings = run_per_miss(seconds, misses)
        assert errors == [], (seconds, misses)
        assert warnings == []
        assert any(f"{seconds:.1f}s / 1800s, {misses} misses" in notice for notice in notices)


def test_per_miss_regression_is_red_and_names_the_dominant_term():
    # 700s over 121 misses = 5.79 s/miss, past K=5.0 while inside the outer bound.
    errors, _, _ = run_per_miss(700.0, 121)
    assert len(errors) == 1
    assert "5.79 s/miss > 5" in errors[0]
    assert "compile-cost regression" in errors[0]


def test_broad_closure_violates_only_the_outer_bound_and_says_the_ratio_is_healthy():
    # 2000s over 500 misses = 4.00 s/miss — per-miss healthy, absolute too big.
    errors, _, _ = run_per_miss(2000.0, 500)
    assert len(errors) == 1
    assert "4.00 s/miss healthy" in errors[0]
    assert "broad closure" in errors[0]
    assert "over its 1800s outer bound" in errors[0]


def test_zero_miss_warm_build_is_green_but_a_slow_zero_miss_build_is_red():
    errors, _, _ = run_per_miss(21.0, 0)
    assert errors == []
    errors, _, _ = run_per_miss(400.0, 0)
    assert len(errors) == 1
    assert "with 0 misses" in errors[0]
    assert "adaptive limit: 30s overhead + 0 misses = 30s" in errors[0]


def test_both_conditions_violated_reported_once_with_both_facts():
    # 1900s over 121 misses: 15.70 s/miss past K and past the 1800s outer bound.
    errors, _, _ = run_per_miss(1900.0, 121)
    assert len(errors) == 1
    assert "15.70 s/miss > 5" in errors[0]
    assert "and over its 1800s outer bound" in errors[0]


def test_missing_cache_report_fails_closed_even_when_within_every_limit():
    errors, _, _ = run_per_miss(21.0, None)
    assert len(errors) == 1
    assert "fails closed" in errors[0]
    # A missing measurement is not a slow build: the cold-build exception does
    # not accept it either.
    errors, _, warnings = run_per_miss(21.0, None, allow_cold_build=True)
    assert len(errors) == 1 and "fails closed" in errors[0] and warnings == []


def test_cold_build_exemption_covers_both_per_miss_conditions():
    # A full-tree cold build (500 misses, near the 524-request signature) at
    # healthy ratios but far over both conditions — one warning carrying both
    # facts, zero errors.
    errors, _, warnings = run_per_miss(3000.0, 500, allow_cold_build=True)
    assert errors == []
    assert len(warnings) == 1
    assert "6.00 s/miss > 5" in warnings[0]
    assert "and over its 1800s outer bound" in warnings[0]
    assert "accepted with cold-build exception" in warnings[0]

    # Outer bound only: 2000s over 500 misses is 4.00 s/miss — per-miss healthy.
    errors, _, warnings = run_per_miss(2000.0, 500, allow_cold_build=True)
    assert errors == []
    assert len(warnings) == 1
    assert "4.00 s/miss healthy" in warnings[0]


# --- all four legs judge per-miss (registry values, real artifacts) ------------
#
# The per-leg budgets mirror test/duration_registry.json (each value's
# derivation lives in that file's reasons). The broad-closure pairs are the
# real build.json + cache-stats.json artifacts of run 39197233020 (PR #516) —
# the run that red three legs under the legacy absolute bounds with 0 test
# failures, and the reason the outer bounds were re-anchored.

LEG_BUDGETS = {
    "Ubuntu x86_64": {"overhead_seconds": 30, "max_seconds_per_miss": 5.0, "max_seconds": 1800},
    "Ubuntu ARM64": {"overhead_seconds": 15, "max_seconds_per_miss": 5.0, "max_seconds": 1020},
    "macOS ARM64": {"overhead_seconds": 20, "max_seconds_per_miss": 5.0, "max_seconds": 1200},
    "Windows MSVC x86_64": {"overhead_seconds": 105, "max_seconds_per_miss": 5.0, "max_seconds": 2640},
}

BROAD_CLOSURE_RUN_39197233020 = [
    ("Ubuntu x86_64", 722.509, 375, "ccache"),
    ("Ubuntu ARM64", 386.112, 223, "ccache"),
    ("macOS ARM64", 475.760, 382, "ccache"),
    ("Windows MSVC x86_64", 1027.282, 376, "sccache"),
]


def leg_budget(job: str) -> dict:
    constants = LEG_BUDGETS[job]
    return {
        "job": job,
        "phase": "build",
        "max_seconds": constants["max_seconds"],
        "per_miss": {
            "overhead_seconds": constants["overhead_seconds"],
            "max_seconds_per_miss": constants["max_seconds_per_miss"],
        },
        "reason": "bounds the build phase per compile unit of change",
    }


def run_leg(job: str, seconds: float, misses: int | None, allow_cold_build=False):
    reports = None if misses is None else {"build": {"tool": "ccache", "misses": misses}}
    return ctd.check_phases(
        job, {"build": seconds}, [leg_budget(job)],
        allow_cold_build=allow_cold_build, cache_reports=reports,
    )


def test_ac4_broad_closure_run_replays_green_on_all_four_legs():
    for job, seconds, misses, tool in BROAD_CLOSURE_RUN_39197233020:
        reports = {"build": {"tool": tool, "misses": misses}}
        errors, notices, warnings = ctd.check_phases(
            job, {"build": seconds}, [leg_budget(job)], cache_reports=reports
        )
        assert errors == [] and warnings == [], (job, errors)
        ratio = seconds / misses
        expected = (
            f"{seconds:.1f}s / {LEG_BUDGETS[job]['max_seconds']}s, "
            f"{misses} misses ({ratio:.2f} s/miss)"
        )
        assert any(expected in notice for notice in notices), (job, notices)


def test_new_leg_per_miss_regression_is_red_and_names_the_dominant_term():
    # One red probe per new leg: past K while inside that leg's outer bound.
    probes = [
        ("Ubuntu ARM64", 700.0, 100),           # 7.00 s/miss, inside 1020
        ("macOS ARM64", 700.0, 100),            # 7.00 s/miss, inside 1200
        ("Windows MSVC x86_64", 2000.0, 300),   # 6.67 s/miss, inside 2640
    ]
    for job, seconds, misses in probes:
        errors, _, _ = run_leg(job, seconds, misses)
        assert len(errors) == 1, (job, errors)
        assert "compile-cost regression" in errors[0], (job, errors[0])
        assert f"{seconds / misses:.2f} s/miss > 5" in errors[0], (job, errors[0])


def test_new_leg_missing_cache_report_fails_closed():
    for job in ("Ubuntu ARM64", "macOS ARM64", "Windows MSVC x86_64"):
        errors, _, _ = run_leg(job, 10.0, None)
        assert len(errors) == 1 and "fails closed" in errors[0], (job, errors)


def test_new_leg_cold_build_exemption_covers_both_conditions():
    # A full-tree cold build on a new leg: one warning carrying both facts,
    # zero errors — the §7.8 mechanism is leg-agnostic.
    errors, _, warnings = run_leg("Windows MSVC x86_64", 3000.0, 500, allow_cold_build=True)
    assert errors == []
    assert len(warnings) == 1
    assert "6.00 s/miss > 5" in warnings[0]
    assert "and over its 2640s outer bound" in warnings[0]

    errors, _, warnings = run_leg("Ubuntu ARM64", 1200.0, 200, allow_cold_build=True)
    assert errors == []
    assert len(warnings) == 1
    assert "6.00 s/miss > 5" in warnings[0]
    assert "and over its 1020s outer bound" in warnings[0]


def test_windows_warm_zero_miss_build_is_judged_against_its_own_overhead():
    # The Windows leg's sccache-warm builds cost ~7x the ccache legs'
    # (73.8-74.8s on runs 39194647515/39187542721); the leg-local overhead is
    # what keeps a 0-miss warm run green there while a genuinely slow build
    # still trips.
    errors, _, _ = run_leg("Windows MSVC x86_64", 90.0, 0)
    assert errors == []
    errors, _, _ = run_leg("Windows MSVC x86_64", 150.0, 0)
    assert len(errors) == 1
    assert "adaptive limit: 105s overhead + 0 misses = 105s" in errors[0]


def test_red_probe_at_k3_turns_the_regressed_runs_red_and_holds_the_blind_spot():
    # The plan's red-state probe, pinned as a test. At K=3.0 the two runs whose
    # closure genuinely sits above 3.0 s/miss turn red while the raypath-class
    # run stays green — the declared single-K cross-class blind spot (2.90
    # s/miss would need a +72% regression to reach 30 + 3x208 = 654s). The
    # raypath run staying green here is the expected reading, not a stale
    # fixture.
    budget = {**PER_MISS_BUDGET, "per_miss": {"overhead_seconds": 30, "max_seconds_per_miss": 3.0}}
    expected = {(436.608, 156): False, (421.150, 163): False, (552.887, 162): True,
                (602.677, 208): False, (490.760, 121): True}
    for (seconds, misses), should_be_red in expected.items():
        errors, _, _ = run_per_miss(seconds, misses, budget=budget)
        assert bool(errors) == should_be_red, (seconds, misses, errors)


def test_notice_reports_misses_and_measured_ratio():
    _, notices, _ = run_per_miss(490.760, 121)
    assert any("121 misses (4.06 s/miss)" in notice for notice in notices)
    _, notices, _ = run_per_miss(21.0, 0)
    assert any(", 0 misses" in notice for notice in notices)


def test_cache_reports_without_a_per_miss_budget_are_never_consumed():
    budget = phase_budget("build", 420)
    errors, _, _ = ctd.check_phases(
        JOB, {"build": 100.0}, [budget], cache_reports={"build": {"tool": "ccache", "misses": 999}}
    )
    assert errors == []


@pytest.mark.parametrize(
    "payload",
    [
        {"overhead_seconds": 30},
        {"max_seconds_per_miss": 5.0},
        {"overhead_seconds": 30, "max_seconds_per_miss": 5.0, "note": "x"},
        {"overhead_seconds": -1, "max_seconds_per_miss": 5.0},
        {"overhead_seconds": 30, "max_seconds_per_miss": 0},
        {"overhead_seconds": 30, "max_seconds_per_miss": -2.0},
    ],
    ids=["missing-k", "missing-overhead", "extra-field", "negative-overhead", "zero-k", "negative-k"],
)
def test_per_miss_schema_rejects_malformed_blocks(tmp_path, payload):
    registry = {
        "version": 4,
        "entries": [],
        "phase_budgets": [{**PER_MISS_BUDGET, "per_miss": payload}],
    }
    with pytest.raises(ctd.RegistryError):
        ctd.load_registry(write_registry(tmp_path, registry))


def test_per_miss_never_sits_on_an_aggregate(tmp_path):
    # members + per_miss together is not an allowed field set: the schema
    # rejects the combination at the fields check, before any per_miss
    # validation runs.
    registry = {
        "version": 4,
        "entries": [],
        "phase_budgets": [
            {**phase_budget("build+ctest", 1080, members=["build", "ctest"]),
             "per_miss": {"overhead_seconds": 30, "max_seconds_per_miss": 5.0}},
        ],
    }
    with pytest.raises(ctd.RegistryError, match="at most one of 'members', 'per_miss' or 'machine_factor'"):
        ctd.load_registry(write_registry(tmp_path, registry))


@pytest.mark.parametrize(
    "payload",
    [
        {"phase": "build", "tool": "ccache"},
        {"phase": "build", "tool": "ccache", "misses": 5, "note": "x"},
        {"phase": "", "tool": "ccache", "misses": 5},
        {"phase": "build", "tool": "baz", "misses": 5},
        {"phase": "build", "tool": "ccache", "misses": -1},
        {"phase": "build", "tool": "ccache", "misses": 1.5},
        {"phase": "build", "tool": "ccache", "misses": True},
    ],
    ids=["missing-misses", "extra-field", "empty-phase", "unknown-tool", "negative", "float", "bool"],
)
def test_cache_report_schema_rejects_malformed_reports(tmp_path, payload):
    report = tmp_path / "cache.json"
    report.write_text(json.dumps(payload), encoding="utf-8")
    with pytest.raises(ValueError):
        ctd.load_cache_reports([report])


def test_duplicate_cache_reports_for_one_phase_are_red(tmp_path):
    first = tmp_path / "one.json"
    second = tmp_path / "two.json"
    payload = {"phase": "build", "tool": "ccache", "misses": 5}
    first.write_text(json.dumps(payload), encoding="utf-8")
    second.write_text(json.dumps(payload), encoding="utf-8")
    with pytest.raises(ValueError, match="duplicate cache report"):
        ctd.load_cache_reports([first, second])


def test_main_consumes_the_cache_report_end_to_end(tmp_path, capsys):
    phase = tmp_path / "build.json"
    phase.write_text(json.dumps({"phase": "build", "seconds": 490.76, "exit_code": 0}), encoding="utf-8")
    cache = tmp_path / "cache-stats.json"
    cache.write_text(json.dumps({"phase": "build", "tool": "ccache", "misses": 121}), encoding="utf-8")
    registry = write_registry(tmp_path, {"version": 4, "entries": [], "phase_budgets": [PER_MISS_BUDGET]})
    argv = [
        "--job", JOB,
        "--phase-report", str(phase),
        "--cache-report", str(cache),
        "--registry", str(registry),
    ]
    assert ctd.main(argv) == 0
    assert "121 misses (4.06 s/miss)" in capsys.readouterr().out

    # Red path through the same wiring: a per-miss regression turns the job red.
    cache.write_text(json.dumps({"phase": "build", "tool": "ccache", "misses": 121}), encoding="utf-8")
    phase.write_text(json.dumps({"phase": "build", "seconds": 700.0, "exit_code": 0}), encoding="utf-8")
    assert ctd.main(argv) == 1
    assert "compile-cost regression" in capsys.readouterr().out


# --- machine-factor ctest budgets (registry v4) --------------------------------
#
# The ctest phase's machine-speed normalization: the ruler subset of the
# per-test report measures how much slower this run's host is than the
# calibration, and the phase budget scales by it. Mirror of the per-miss
# section: the authoritative ruler is the factor, absolute max_seconds survives
# only through the clamp band's upper edge (a raw factor past the band clamps
# with a loud warning and the budget arithmetic judges from there).

MACHINE_BUDGET = {
    "job": JOB,
    "phase": "ctest",
    "max_seconds": 480,
    "machine_factor": {
        "ruler_seconds": {
            "LumiceUnitCorrectnessTest::Schema3.AssemblyChainReportsTheLadder": 3.0,
            "LumiceUnitCorrectnessTest::Schema3.FoldKeepsEveryRayAccounted": 5.0,
            "LumiceUnitCorrectnessTest::Schema3.RoutingKeySurvivesTheRoll": 8.0,
        },
        "clamp_band": [0.8, 2.0],
    },
    "reason": "normalizes the ctest budget by measured machine speed",
}


def ruler_durations(factor: float, skip: str | None = None, zero: str | None = None) -> dict[str, float]:
    out = {tid: cal * factor for tid, cal in MACHINE_BUDGET["machine_factor"]["ruler_seconds"].items()}
    if skip is not None:
        del out[skip]
    if zero is not None:
        out[zero] = 0.0
    return out


def run_machine(seconds: float, factor: float = 1.0, durations=None, budget=None):
    if durations is None:
        durations = ruler_durations(factor)
    return ctd.check_phases(JOB, {"ctest": seconds}, [budget or MACHINE_BUDGET], durations=durations)


def test_machine_normalized_budget_replays_a_tier_shifted_run_green():
    # The incident shape: the same suite measures 1.25x on the slower host tier.
    # 506.8s red at the absolute 480; the ruler reads the tier and the budget
    # scales to 601.4s.
    errors, notices, warnings = run_machine(506.8, 1.253)
    assert errors == [] and warnings == []
    assert any(
        "506.8s / 601.4s (machine factor 1.253 from 3 rulers)" in notice for notice in notices
    ), notices


def test_real_regression_stays_red_when_the_ruler_says_the_machine_is_not_slower():
    # The other incident shape (the branch that genuinely added ~100s of test
    # work): the ruler reads factor 1.0, so normalization must not absorb it.
    errors, _, _ = run_machine(506.8, 1.0)
    assert len(errors) == 1
    assert "machine-normalized limit 480s x factor 1.000 = 480.0s" in errors[0]


def test_exactly_at_the_machine_normalized_limit_is_green():
    errors, _, _ = run_machine(480.0 * 1.253, 1.253)
    assert errors == []


def test_missing_per_test_report_fails_closed():
    errors, _, _ = ctd.check_phases(JOB, {"ctest": 100.0}, [MACHINE_BUDGET], durations=None)
    assert len(errors) == 1
    assert "fails closed" in errors[0] and "--report" in errors[0]


def test_missing_ruler_id_fails_closed_and_names_it():
    durations = ruler_durations(1.0, skip="LumiceUnitCorrectnessTest::Schema3.FoldKeepsEveryRayAccounted")
    errors, _, _ = run_machine(100.0, durations=durations)
    assert len(errors) == 1
    assert "fails closed" in errors[0]
    assert "Schema3.FoldKeepsEveryRayAccounted" in errors[0]
    assert "§7.7 anti-ratchet" in errors[0]
    # A missing measurement is not a slow phase: the cold-build exception does
    # not accept it either (ctest never carries `build`, but pin the rule).
    errors, _, warnings = ctd.check_phases(
        JOB, {"ctest": 100.0}, [MACHINE_BUDGET], durations=durations, allow_cold_build=True
    )
    assert len(errors) == 1 and warnings == []


def test_zero_ruler_reading_fails_closed():
    # A skipped ruler test reads 0 and cannot measure the machine — the exact
    # way a stale calibration would hide, so it must not read as "fast".
    durations = ruler_durations(1.0, zero="LumiceUnitCorrectnessTest::Schema3.RoutingKeySurvivesTheRoll")
    errors, _, _ = run_machine(100.0, durations=durations)
    assert len(errors) == 1
    assert "broken (zero or non-finite) readings" in errors[0]
    assert "Schema3.RoutingKeySurvivesTheRoll" in errors[0]


def test_band_exceeded_clamps_with_a_warning_and_does_not_stay_silent():
    # Raw factor 2.5 clamps to the 2.0 upper edge: the budget judges from the
    # clamped value (900s < 960s green, with a loud warning), and a phase over
    # even the clamped budget still goes red.
    errors, notices, warnings = run_machine(900.0, 2.5)
    assert errors == []
    assert len(warnings) == 1
    assert "calibration band exceeded (raw factor 2.500, clamped to 2.000)" in warnings[0]
    assert "§7.7 anti-ratchet" in warnings[0]
    assert any("900.0s / 960.0s (machine factor 2.000 from 3 rulers)" in n for n in notices)

    errors, _, warnings = run_machine(1000.0, 2.5)
    assert len(errors) == 1
    assert "machine-normalized limit 480s x factor 2.000 = 960.0s" in errors[0]
    assert len(warnings) == 1  # the clamped run stays loudly warned either way


def test_below_the_band_clamps_up_and_warns_the_same_way():
    # A host faster than the calibration era clamps at the lower edge: budgets
    # do not shrink below 0.8x, and the reader is told the calibration is stale.
    errors, notices, warnings = run_machine(350.0, 0.5)
    assert errors == []
    assert len(warnings) == 1
    assert "raw factor 0.500, clamped to 0.800" in warnings[0]
    assert any("350.0s / 384.0s (machine factor 0.800 from 3 rulers)" in n for n in notices)

    errors, _, _ = run_machine(400.0, 0.5)
    assert len(errors) == 1
    assert "machine-normalized limit 480s x factor 0.800 = 384.0s" in errors[0]


def test_compute_machine_factor_matches_hand_computation():
    ruler = {"a": 2.0, "b": 4.0, "c": 5.0, "d": 10.0, "e": 20.0}
    durations = {"a": 2.5, "b": 4.5, "c": 7.5, "d": 12.5, "e": 25.0}
    # ratios: 1.25, 1.125, 1.5, 1.25, 1.25 -> median 1.25
    factor, raw = ctd.compute_machine_factor(ruler, durations, (0.8, 2.0))
    assert raw == 1.25 and factor == 1.25
    # computed once, consumed everywhere: check_phases' notice carries the same
    # number this function returns.
    budget = {
        **MACHINE_BUDGET,
        "machine_factor": {**MACHINE_BUDGET["machine_factor"], "ruler_seconds": ruler},
    }
    _, notices, _ = ctd.check_phases(JOB, {"ctest": 500.0}, [budget], durations=durations)
    assert any("machine factor 1.250 from 5 rulers" in n for n in notices)


@pytest.mark.parametrize(
    "payload",
    [
        {"ruler_seconds": {"a": 1.0, "b": 2.0}, "clamp_band": [0.8, 2.0]},
        {"ruler_seconds": {"a": 1.0, "b": 2.0, "c": 3.0}, "clamp_band": [2.0, 0.8]},
        {"ruler_seconds": {"a": 1.0, "b": 2.0, "c": 3.0}, "clamp_band": [0.0, 2.0]},
        {"ruler_seconds": {"a": 1.0, "b": 2.0, "c": 3.0}, "clamp_band": [0.8, 2.0, 4.0]},
        {"ruler_seconds": {"a": 1.0, "b": 2.0, "c": 0}, "clamp_band": [0.8, 2.0]},
        {"ruler_seconds": {"a": 1.0, "b": 2.0, "c": -3.0}, "clamp_band": [0.8, 2.0]},
        {"ruler_seconds": {"a": 1.0, "b": 2.0, "c": 3.0}, "clamp_band": [0.8, 2.0], "note": "x"},
        {"ruler_seconds": {"": 1.0, "b": 2.0, "c": 3.0}, "clamp_band": [0.8, 2.0]},
    ],
    ids=[
        "two-rulers", "inverted-band", "zero-lo", "three-element-band", "zero-calibration",
        "negative-calibration", "extra-field", "empty-ruler-id",
    ],
)
def test_machine_factor_schema_rejects_malformed_blocks(tmp_path, payload):
    registry = {
        "version": 4,
        "entries": [],
        "phase_budgets": [{**MACHINE_BUDGET, "machine_factor": payload}],
    }
    with pytest.raises(ctd.RegistryError):
        ctd.load_registry(write_registry(tmp_path, registry))


@pytest.mark.parametrize(
    "budget",
    [
        # an aggregate: normalization would silently absorb what the owner's
        # wall clock must show
        {**phase_budget("build+ctest", 2160, members=["build", "ctest"]),
         "machine_factor": MACHINE_BUDGET["machine_factor"]},
        # the build phase already has its own adaptive ruler (per_miss)
        {**PER_MISS_BUDGET, "machine_factor": MACHINE_BUDGET["machine_factor"]},
        # an e2e phase has no ctest semantics to normalize
        {**phase_budget("e2e", 600), "machine_factor": MACHINE_BUDGET["machine_factor"]},
    ],
    ids=["aggregate", "build-with-per-miss", "e2e-phase"],
)
def test_machine_factor_never_mounts_outside_the_ctest_phase(tmp_path, budget):
    registry = {"version": 4, "entries": [], "phase_budgets": [budget]}
    with pytest.raises(ctd.RegistryError):
        ctd.load_registry(write_registry(tmp_path, registry))


def test_machine_factor_and_per_miss_are_mutually_exclusive(tmp_path):
    budget = {
        **MACHINE_BUDGET,
        "phase": "build",
        "per_miss": {"overhead_seconds": 30, "max_seconds_per_miss": 5.0},
    }
    registry = {"version": 4, "entries": [], "phase_budgets": [budget]}
    with pytest.raises(ctd.RegistryError):
        ctd.load_registry(write_registry(tmp_path, registry))


def test_main_consumes_the_per_test_report_end_to_end_for_the_factor(tmp_path, capsys):
    report = tmp_path / "ctest-durations.json"
    report.write_text(json.dumps(ruler_durations(1.253)), encoding="utf-8")
    phase = tmp_path / "ctest.json"
    phase.write_text(json.dumps({"phase": "ctest", "seconds": 506.8, "exit_code": 0}), encoding="utf-8")
    registry = write_registry(tmp_path, {"version": 4, "entries": [], "phase_budgets": [MACHINE_BUDGET]})
    argv = [
        "--job", JOB,
        "--report", str(report),
        "--phase-report", str(phase),
        "--registry", str(registry),
    ]
    assert ctd.main(argv) == 0
    out = capsys.readouterr().out
    assert "machine factor 1.253 from 3 rulers" in out

    # Red path through the same wiring: drop the ruler rows and the check
    # fails closed on the same input shape.
    report.write_text(json.dumps({"unrelated.py::t": 1.0}), encoding="utf-8")
    assert ctd.main(argv) == 1
    assert "fails closed" in capsys.readouterr().out

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


KILL_PROBE = '''
import time

import pytest


@pytest.mark.parametrize("i", range(4))
def test_slow(i):
    time.sleep(2)
'''


def test_plugin_leaves_a_parseable_report_behind_when_the_session_is_killed(tmp_path):
    # The disposal rule for an over-budget step reads the per-test durations
    # first; a report written only at session finish is gone exactly when the
    # step was killed for being over budget. The plugin rewrites the file after
    # every report, so a SIGKILL mid-session must leave the finished tests'
    # entries behind, in a complete (atomically replaced) file.
    (tmp_path / "test_kill_probe.py").write_text(KILL_PROBE, encoding="utf-8")
    env = {**os.environ, "PYTHONPATH": str(SCRIPTS)}
    proc = subprocess.Popen(
        [sys.executable, "-m", "pytest", "-q", "-p", "no:cacheprovider", "-p", "duration_report_plugin",
         "--duration-report=out.json", "test_kill_probe.py"],
        cwd=tmp_path, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
    report = tmp_path / "out.json"
    deadline = time.monotonic() + 60
    while not report.exists() and time.monotonic() < deadline:
        time.sleep(0.1)
    assert report.exists(), "the plugin wrote nothing before the first test finished"
    assert proc.poll() is None, "the probe session finished before it could be killed mid-run"
    proc.kill()
    proc.wait(timeout=30)
    data = json.loads(report.read_text(encoding="utf-8"))
    assert "test_kill_probe.py::test_slow[0]" in data
    assert "test_kill_probe.py::test_slow[3]" not in data
