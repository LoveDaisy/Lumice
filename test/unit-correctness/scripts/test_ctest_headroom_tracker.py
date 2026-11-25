"""Regression net for `scripts/ctest_headroom_tracker.py` and its workflow wiring.

The tracker is observation, not judgment, and its failure modes are quiet in
exactly the wrong direction: a misparsed clamp reading fakes extra headroom
(a clamp inflates C by up to ~9% when read from the clamped factor), a
mispaired pair of low samples fires a signal nobody asked for, a gap that
reads as health launders a broken leg into a clean series, and a publish
that overwrites gh-pages outside `headroom/` clobbers benchmark history.
So every rule below is pinned next to its counter-case, mostly offline:
parsing and the streak engine run on the real captured log lines, and the
end-to-end harvest runs against a fake `gh` binary and a throwaway git
remote — no network, no real GitHub write.
"""
from __future__ import annotations

import base64
import json
import os
import re
import subprocess
import sys
import urllib.parse
from pathlib import Path

import pytest
import yaml

SCRIPTS = Path(__file__).resolve().parents[3] / "scripts"
sys.path.insert(0, str(SCRIPTS))

import ctest_headroom_tracker as tracker  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parents[3]
WORKFLOW_PATH = REPO_ROOT / ".github" / "workflows" / "ci.yml"

# --- fixtures: the three captured real log lines (old v4 formats, both lines) ----
#
# Sources (public job logs, read 2026-11-24):
#   main green   run 45099209377 / job 137445890915 (macOS ARM64)
#   release clamp run 45146282519 / job 137596258809 (Windows MSVC x86_64) — the
#     warning is printed AFTER the notice; raw factor 0.726 clamped to 0.800
#   release red  run 45309154293 / job 138160253218 (Windows MSVC x86_64)
# The leading timestamps are captured log text: the parser strips them and no
# assertion reads them, so they are inert against the wall clock.

MAIN_GREEN_MACOS = (
    "2026-11-23T02:30:37.2006990Z ##[notice]phase budget 'ctest' for job 'macOS ARM64': "
    "537.7s / 569.8s (machine factor 1.018 from 7 rulers)"
)
RELEASE_CLAMP_NOTICE = (
    "2026-11-23T09:37:31.7304670Z ##[notice]phase budget 'ctest' for job 'Windows MSVC x86_64': "
    "346.8s / 384.0s (machine factor 0.800 from 7 rulers)"
)
RELEASE_CLAMP_WARNING = (
    "2026-11-23T09:37:31.7308196Z ##[warning]phase budget 'ctest' for job 'Windows MSVC x86_64': "
    "calibration band exceeded (raw factor 0.726, clamped to 0.800); recalibrate the ruler "
    "per the §7.7 anti-ratchet rule"
)
RELEASE_RED = (
    "2026-11-24T02:23:57.1617308Z ##[error]phase budget 'ctest' for job 'Windows MSVC x86_64' "
    "took 565.3s (ctest), over its machine-normalized limit 480s x factor 1.174 = 563.3s"
)

MACOS_JOB = "macOS ARM64"
WINDOWS_JOB = "Windows MSVC x86_64"


def registry_payload(budgets: list[dict]) -> str:
    return json.dumps({"version": 4, "entries": [], "phase_budgets": budgets})


def machine_budget(job: str, max_seconds: int, rulers=3) -> dict:
    return {
        "job": job,
        "phase": "ctest",
        "max_seconds": max_seconds,
        "machine_factor": {
            "ruler_seconds": {f"Case.{i}": float(2 * (i + 1)) for i in range(rulers)},
            "clamp_band": [0.8, 2.0],
        },
        "reason": "r",
    }


def contents_payload(text: str, blob_sha: str) -> str:
    return json.dumps(
        {
            "encoding": "base64",
            "content": base64.b64encode(text.encode("utf-8")).decode("ascii"),
            "sha": blob_sha,
        }
    )


# --- parsing -------------------------------------------------------------------


def test_real_green_line_parses_with_timestamp_and_level_stripped():
    parsed = tracker.parse_checker_lines(MAIN_GREEN_MACOS)
    assert len(parsed) == 1
    entry = parsed[0]
    assert entry["kind"] == "green"
    assert entry["job"] == MACOS_JOB and entry["phase"] == "ctest"
    assert entry["seconds"] == "537.7" and entry["factor"] == "1.018" and entry["rulers"] == "7"


def test_double_colon_spelling_is_tolerated_like_the_runner_rendered_one():
    alt = "::notice::phase budget 'ctest' for job 'x': 10.0s / 11.0s (machine factor 1.000 from 7 rulers)"
    parsed = tracker.parse_checker_lines(alt)
    assert [entry["kind"] for entry in parsed] == ["green"]


def test_unrelated_lines_never_parse_as_readings():
    noise = "\n".join(
        [
            "##[notice]Deprecation notice: something else",
            "##[error]Process completed with exit code 1.",
            "2026-11-23T00:00:00.0Z ##[notice]phase budget 'build' for job 'x': 10.0s / 20s",
            "phase budget 'ctest' for job 'y': 10.0s / 11.0s (machine factor 1.0 from 7 rulers)",  # no level prefix
        ]
    )
    assert tracker.parse_checker_lines(noise) == []


def test_clamp_warning_after_green_notice_overrides_with_the_raw_factor():
    log = "\n".join([RELEASE_CLAMP_NOTICE, RELEASE_CLAMP_WARNING, "##[notice]unrelated"])
    reading = tracker.extract_reading(tracker.parse_checker_lines(log), WINDOWS_JOB, "ctest")
    assert reading["seconds"] == 346.8
    assert reading["gate_factor"] == 0.8
    assert reading["raw_factor"] == 0.726  # the clamp must not fake the machine-relative reading
    assert reading["log_lines"] == [RELEASE_CLAMP_NOTICE.split("Z ", 1)[1], RELEASE_CLAMP_WARNING.split("Z ", 1)[1]]


def test_red_line_yields_its_own_reading():
    reading = tracker.extract_reading(tracker.parse_checker_lines(RELEASE_RED), WINDOWS_JOB, "ctest")
    assert reading["seconds"] == 565.3
    assert reading["raw_factor"] == 1.174  # no warning: gate factor == raw, at log precision
    assert reading["rulers"] == 0  # the red format carries no ruler count


def test_ambiguous_logs_refuse_rather_than_pick_one():
    two_greens = "\n".join([MAIN_GREEN_MACOS, MAIN_GREEN_MACOS.replace("537.7", "538.8")])
    with pytest.raises(tracker.ReadingError, match="ambiguous-multiple-readings"):
        tracker.extract_reading(tracker.parse_checker_lines(two_greens), MACOS_JOB, "ctest")
    with pytest.raises(tracker.ReadingError, match="no-machine-factor-reading"):
        tracker.extract_reading(tracker.parse_checker_lines("##[notice]nothing here"), MACOS_JOB, "ctest")


# --- headroom arithmetic ---------------------------------------------------------


def test_literal_worked_example_uses_raw_factor_and_budget_denominator():
    # T=350, raw factor 0.5, B=480 -> C=700, headroom -45.83%; the clamped 0.8
    # would fake C=437.5 / +8.85%.
    implied, headroom = tracker.implied_and_headroom(350.0, 0.5, 480.0)
    assert implied == pytest.approx(700.0)
    assert headroom == pytest.approx(-45.8333, abs=1e-3)
    lo, hi = tracker.rounding_interval_pct(350.0, 0.5, 480.0)
    assert lo < -45.83 < hi
    assert tracker.classify_headroom((lo, hi), 6.0) == "low"


def test_clearance_denominator_is_one_minus_c_over_b_not_b_over_c():
    # The same pair reads ~8.66% in the B/C-1 denomination — the two must not be
    # confused (the 480/560 raise used the other one and overstated by ~1 point).
    implied, headroom = tracker.implied_and_headroom(511.5, 1.0, 560.0)
    other = 100.0 * (560.0 / implied - 1.0)
    assert headroom == pytest.approx(-8.66, abs=0.05) or headroom == pytest.approx(8.66, abs=0.05)
    assert abs(other - headroom) > 0.5  # genuinely different numbers


def test_threshold_classification_boundaries():
    assert tracker.classify_headroom((6.0, 6.0), 6.0) == "healthy"  # equal is not low
    assert tracker.classify_headroom((5.9, 6.1), 6.0) == "uncertain"  # rounding straddles
    assert tracker.classify_headroom((5.99, 5.999), 6.0) == "low"
    assert tracker.classify_headroom((-0.4, -0.3), 6.0) == "low"  # negative headroom is low


def test_real_boundary_sample_classifies_low():
    # The main macOS reading this task exists to watch: C=528.2 vs B=560.
    lo, hi = tracker.rounding_interval_pct(537.7, 1.018, 560.0)
    assert tracker.classify_headroom((lo, hi), 6.0) == "low"


# --- registry snapshot ------------------------------------------------------------


def test_snapshot_extracts_machine_budgets_and_ignores_others():
    text = registry_payload([machine_budget(MACOS_JOB, 560), {"job": "e2e-test", "phase": "e2e", "max_seconds": 600, "reason": "r"}])
    budgets = tracker.parse_registry_snapshot(text)
    assert set(budgets) == {(MACOS_JOB, "ctest")}
    assert budgets[(MACOS_JOB, "ctest")]["budget_seconds"] == 560
    assert budgets[(MACOS_JOB, "ctest")]["rulers"] == 3


def test_segment_changes_when_the_budget_or_rulers_change_not_when_prose_does():
    base = tracker.parse_registry_snapshot(registry_payload([machine_budget(MACOS_JOB, 560)]))
    same_prose = registry_payload([{**machine_budget(MACOS_JOB, 560), "reason": "different prose"}])
    assert tracker.parse_registry_snapshot(same_prose)[(MACOS_JOB, "ctest")]["segment"] == base[(MACOS_JOB, "ctest")]["segment"]
    moved_budget = tracker.parse_registry_snapshot(registry_payload([machine_budget(MACOS_JOB, 520)]))
    assert moved_budget[(MACOS_JOB, "ctest")]["segment"] != base[(MACOS_JOB, "ctest")]["segment"]
    moved_ruler = machine_budget(MACOS_JOB, 560)
    moved_ruler["machine_factor"]["ruler_seconds"]["Case.0"] = 2.5
    assert tracker.parse_registry_snapshot(registry_payload([moved_ruler]))[(MACOS_JOB, "ctest")]["segment"] != base[(MACOS_JOB, "ctest")]["segment"]


def test_non_v4_snapshot_is_rejected_not_guessed():
    with pytest.raises(tracker.HarvestError, match="unsupported-registry-version"):
        tracker.parse_registry_snapshot('{"version": 3, "entries": [], "phase_budgets": []}')


# --- streak engine ------------------------------------------------------------------

CFG = tracker.config_identity(6.0, 2)
OTHER_CFG = tracker.config_identity(4.0, 2)


def make_sample(run_id, seconds, raw, budget, *, line="main", job=MACOS_JOB, created, segment="seg-a", config=CFG):
    run = {
        "id": run_id,
        "run_number": run_id,
        "head_branch": "main" if line == "main" else "release/4.7.7",
        "html_url": f"https://example.test/run/{run_id}",
        "head_sha": f"sha-{run_id}",
        "created_at": created,
    }
    return tracker.sample_record(
        run=run, line=line, job=job, phase="ctest",
        budget_entry={"segment": segment, "budget_seconds": budget, "rulers": 7},
        reading={"seconds": seconds, "gate_factor": raw, "raw_factor": raw, "rulers": 7, "log_lines": ["x"]},
        registry_blob_sha="blob", registry_sha256="reg", config_id=config,
    )


def make_gap(run_id, *, line="main", job=MACOS_JOB, created, config=CFG,
             reason="reading: no-machine-factor-reading", phase="ctest"):
    run = {
        "id": run_id, "run_number": run_id,
        "head_branch": "main" if line == "main" else "release/4.7.7",
        "html_url": f"https://example.test/run/{run_id}",
        "head_sha": f"sha-{run_id}", "created_at": created,
    }
    return tracker.gap_record(run=run, line=line, job=job, phase=phase, reason=reason, config_id=config)


def walk(records, threshold=6.0, streak=2, config=CFG):
    grouped = tracker.group_series(records)
    key = next(k for k in grouped if k[0] == "main" and k[2] == MACOS_JOB)
    return tracker.walk_series(grouped[key], config, threshold, streak)


def test_two_consecutive_lows_fire_exactly_one_deterministic_event():
    records = [
        make_sample(1, 560.0, 1.0, 560.0, created="2026-11-20T00:00:00Z"),  # h=0.00 low
        make_sample(2, 580.0, 1.0, 560.0, created="2026-11-21T00:00:00Z"),  # h=-3.57 low
    ]
    first = walk(records)
    assert first["streak"] == 2 and len(first["fired"]) == 1
    event = first["fired"][0]
    assert event["first_run"]["id"] == 1 and event["latest_run"]["id"] == 2
    again = walk(records)
    assert again["fired"][0]["event_id"] == event["event_id"]  # recompute is idempotent


def test_single_low_records_but_does_not_fire():
    result = walk([make_sample(1, 560.0, 1.0, 560.0, created="2026-11-20T00:00:00Z")])
    assert result["streak"] == 1 and result["fired"] == [] and result["active"] is None


def test_low_healthy_low_never_fires():
    records = [
        make_sample(1, 560.0, 1.0, 560.0, created="2026-11-20T00:00:00Z"),
        make_sample(2, 480.0, 1.0, 560.0, created="2026-11-21T00:00:00Z"),  # h=14.29 healthy
        make_sample(3, 560.0, 1.0, 560.0, created="2026-11-22T00:00:00Z"),
    ]
    result = walk(records)
    assert result["fired"] == [] and result["streak"] == 1


def test_low_gap_low_does_not_pair_across_the_gap():
    records = [
        make_sample(1, 560.0, 1.0, 560.0, created="2026-11-20T00:00:00Z"),
        make_gap(2, created="2026-11-21T00:00:00Z"),
        make_sample(3, 560.0, 1.0, 560.0, created="2026-11-22T00:00:00Z"),
    ]
    result = walk(records)
    assert result["fired"] == [] and result["streak"] == 1


def test_uncertain_breaks_continuity_instead_of_guessing():
    # T=564, f=1.0, B=600 reads exactly 6.00% before rounding; the displayed
    # operands' interval straddles the threshold: neither a low nor a health
    # witness.
    uncertain = make_sample(2, 564.0, 1.0, 600.0, created="2026-11-21T00:00:00Z")
    assert tracker.annotate_status(uncertain, 6.0) == "uncertain"
    records = [
        make_sample(1, 560.0, 1.0, 560.0, created="2026-11-20T00:00:00Z"),
        uncertain,
        make_sample(3, 560.0, 1.0, 560.0, created="2026-11-22T00:00:00Z"),
    ]
    result = walk(records)
    assert result["fired"] == [] and result["streak"] == 1


def test_recovery_clears_the_signal_and_a_new_pair_fires_a_new_event():
    records = [
        make_sample(1, 560.0, 1.0, 560.0, created="2026-11-20T00:00:00Z"),
        make_sample(2, 580.0, 1.0, 560.0, created="2026-11-21T00:00:00Z"),  # fires
        make_sample(3, 480.0, 1.0, 560.0, created="2026-11-22T00:00:00Z"),  # recovery
        make_sample(4, 570.0, 1.0, 560.0, created="2026-11-23T00:00:00Z"),  # low again
        make_sample(5, 585.0, 1.0, 560.0, created="2026-11-24T00:00:00Z"),  # fires anew
    ]
    result = walk(records)
    assert len(result["fired"]) == 2
    assert result["fired"][0]["event_id"] != result["fired"][1]["event_id"]
    assert result["active"]["event_id"] == result["fired"][1]["event_id"]


def test_active_signal_survives_a_gap_but_not_a_recovery():
    with_gap = [
        make_sample(1, 560.0, 1.0, 560.0, created="2026-11-20T00:00:00Z"),
        make_sample(2, 580.0, 1.0, 560.0, created="2026-11-21T00:00:00Z"),
        make_gap(3, created="2026-11-22T00:00:00Z"),
    ]
    result = walk(with_gap)
    assert result["active"] is not None  # a gap is not a recovery
    with_recovery = with_gap + [make_sample(4, 400.0, 1.0, 560.0, created="2026-11-23T00:00:00Z")]
    assert walk(with_recovery)["active"] is None


def test_calibration_segment_change_breaks_the_streak():
    records = [
        make_sample(1, 560.0, 1.0, 560.0, created="2026-11-20T00:00:00Z", segment="seg-old"),
        make_sample(2, 580.0, 1.0, 520.0, created="2026-11-21T00:00:00Z", segment="seg-new"),
    ]
    result = walk(records)
    assert result["fired"] == [] and result["streak"] == 1  # a re-anchor never pairs with the old anchor


def test_threshold_config_change_puts_history_in_a_past_era():
    # Era semantics with ledger-fenced boundaries: a real ledger contains a
    # config-change marker at every observed change; the walk evaluates only
    # the suffix after the LAST marker. While the old config is current, its
    # own era fires legitimately (recompute re-derives the same identity).
    # After the change, earlier eras evaluate to nothing — and a return to
    # the old threshold value does NOT resurrect them: the return writes its
    # own marker, and the era after it is still empty.
    pair_old = [
        make_sample(1, 560.0, 1.0, 560.0, created="2026-11-20T00:00:00Z", config=CFG),
        make_sample(2, 580.0, 1.0, 560.0, created="2026-11-21T00:00:00Z", config=CFG),
    ]
    old_era = walk(pair_old, config=CFG)
    assert len(old_era["fired"]) == 1 and old_era["fired"][0]["config_id"] == CFG

    change_up = tracker.config_change_marker(CFG, OTHER_CFG)
    change_up["run_created_at"] = "2026-11-21T12:00:00Z"
    records = pair_old + [change_up, make_sample(3, 585.0, 1.0, 560.0, created="2026-11-22T00:00:00Z", config=OTHER_CFG)]
    new_era = walk(records, config=OTHER_CFG)
    assert new_era["fired"] == [] and new_era["active"] is None
    assert new_era["streak"] == 1  # the era's own first sample, no inherited count

    # Threshold returns to the old VALUE: without the marker fence the hash
    # would resurrect the cleared era; with it, the post-return era is empty.
    change_back = tracker.config_change_marker(OTHER_CFG, CFG)
    change_back["run_created_at"] = "2026-11-22T12:00:00Z"
    resurrected = walk(records + [change_back], config=CFG)
    assert resurrected["fired"] == [] and resurrected["active"] is None and resurrected["streak"] == 0

    # New evidence under the returned config builds its own streak and fires
    # under the same config identity but a distinct first_run identity.
    more = records + [change_back] + [
        make_sample(4, 590.0, 1.0, 560.0, created="2026-11-23T00:00:00Z", config=CFG),
        make_sample(5, 595.0, 1.0, 560.0, created="2026-11-24T00:00:00Z", config=CFG),
    ]
    after = walk(more, config=CFG)
    assert len(after["fired"]) == 1 and after["fired"][0]["config_id"] == CFG
    assert after["fired"][0]["first_run"]["id"] == 4
    assert after["fired"][0]["event_id"] != old_era["fired"][0]["event_id"]


def test_lines_and_platforms_do_not_share_a_series():
    records = [
        make_sample(1, 560.0, 1.0, 560.0, created="2026-11-20T00:00:00Z"),
        make_sample(2, 580.0, 1.0, 560.0, created="2026-11-21T00:00:00Z", line="release", job=WINDOWS_JOB),
    ]
    grouped = tracker.group_series(records)
    assert set(grouped) == {
        ("main", "main", MACOS_JOB, "ctest"),
        ("release", "release/4.7.7", WINDOWS_JOB, "ctest"),
    }
    assert all(result["fired"] == [] for result in [
        tracker.walk_series(entries, CFG, 6.0, 2) for entries in grouped.values()
    ])


def test_rerun_sample_never_enters_the_ledger_as_a_second_attempt():
    # Attempt discipline lives in the record key: attempt is pinned to 1, so a
    # rerun's rows cannot be appended under a second key.
    rec = make_sample(1, 560.0, 1.0, 560.0, created="2026-11-20T00:00:00Z")
    assert rec["key"] == "1:1:macOS ARM64:ctest" and rec["attempt"] == 1


def test_line_wide_gap_barriers_a_series_discovered_in_the_same_or_later_batch():
    # A cancelled run recorded as a line-wide gap (before the branch's series
    # were known) must fence the series anyway — including when the series'
    # first numeric samples arrive in the very same batch on either side.
    cancelled = make_gap(2, created="2026-11-21T00:00:00Z", job="", reason="run-cancelled")
    records = [
        make_sample(1, 560.0, 1.0, 560.0, created="2026-11-20T00:00:00Z"),
        cancelled,
        make_sample(3, 580.0, 1.0, 560.0, created="2026-11-22T00:00:00Z"),
    ]
    result = walk(records)
    assert result["fired"] == [] and result["streak"] == 1
    # The same fencing for the registry-unreadable class, projected onto the
    # series from a line-wide gap recorded outside any known series.
    between = [
        make_sample(10, 560.0, 1.0, 560.0, created="2026-11-20T00:00:00Z"),
        make_gap(9, created="2026-11-20T12:00:00Z", job="", reason="registry-unreadable: ..."),
        make_sample(11, 580.0, 1.0, 560.0, created="2026-11-21T00:00:00Z"),
    ]
    assert walk(between)["fired"] == [] and walk(between)["streak"] == 1
    # Two lows adjacent AFTER a barrier pair with each other — the barrier
    # breaks continuity at its own position, it does not fence the future.
    after = [
        make_gap(9, created="2026-11-19T00:00:00Z", job="", reason="run-cancelled"),
        make_sample(10, 560.0, 1.0, 560.0, created="2026-11-20T00:00:00Z"),
        make_sample(11, 580.0, 1.0, 560.0, created="2026-11-21T00:00:00Z"),
    ]
    assert len(walk(after)["fired"]) == 1


def test_retryable_gaps_upgrade_to_samples_and_samples_never_downgrade():
    retryable = make_gap(1, created="2026-11-20T00:00:00Z", reason="api-budget-exhausted")
    sample = make_sample(1, 560.0, 1.0, 560.0, created="2026-11-20T00:00:00Z")
    merged = tracker.merge_records([retryable], [sample])
    assert len(merged) == 1 and merged[0]["type"] == "sample"  # upgrade
    merged = tracker.merge_records([sample], [retryable])
    assert len(merged) == 1 and merged[0]["type"] == "sample"  # never a downgrade
    terminal = make_gap(1, created="2026-11-20T00:00:00Z", reason="reading: no-machine-factor-reading")
    merged = tracker.merge_records([terminal], [retryable])
    assert merged[0]["reason"].startswith("reading:")  # terminal outranks retryable
    assert tracker.is_retryable_gap(retryable)
    assert not tracker.is_retryable_gap(terminal)
    assert tracker.is_retryable_gap(
        make_gap(2, created="2026-11-20T00:00:00Z", reason="log-unreadable: forbidden — check token permissions (x)")
    )
    assert not tracker.is_retryable_gap(
        make_gap(3, created="2026-11-20T00:00:00Z", reason="log-unreadable: not found or expired (404)")
    )


def test_line_wide_retryable_gaps_are_retired_once_the_run_is_collected():
    # A cold-start collection failure records `run:1::`; the successful
    # collection writes `run:1:job:phase`. The key-difference merge cannot
    # supersede across them, so the sweep must retire the stale line-wide row
    # once real per-series evidence covers the run — otherwise the stale
    # fence would sit next to the evidence forever.
    line_wide_retryable = make_gap(1, created="2026-11-20T00:00:00Z", job="", reason="registry-unreadable: ...")
    per_series = [
        make_sample(1, 560.0, 1.0, 560.0, created="2026-11-20T00:00:00Z"),
        make_gap(1, created="2026-11-20T00:00:00Z", job=WINDOWS_JOB),  # terminal gap, series known
    ]
    swept = tracker.drop_superseded_line_wide_gaps([line_wide_retryable] + per_series)
    assert line_wide_retryable not in swept and len(swept) == 2
    # A terminal line-wide row survives: it fences series that may not exist yet.
    line_wide_terminal = make_gap(2, created="2026-11-21T00:00:00Z", job="", reason="run-cancelled")
    assert tracker.drop_superseded_line_wide_gaps([line_wide_terminal]) == [line_wide_terminal]
    # A run whose per-series records are still retryable keeps its line-wide row.
    incomplete = [
        make_gap(3, created="2026-11-22T00:00:00Z", job="", reason="run-in-progress"),
        make_gap(3, created="2026-11-22T00:00:00Z", job=MACOS_JOB, reason="api-budget-exhausted"),
    ]
    assert tracker.drop_superseded_line_wide_gaps(incomplete) == incomplete


def test_state_merge_takes_the_max_cursor_and_drops_legacy_pending(tmp_path):
    ours = tmp_path / "ours"
    theirs = tmp_path / "theirs"
    ours.mkdir()
    theirs.mkdir()
    write_jsonl = tracker.write_jsonl
    write_jsonl(ours / "samples.jsonl", [])
    write_jsonl(theirs / "samples.jsonl", [])
    (ours / "state.json").write_text(json.dumps({
        "config": {"config_id": "cfg-x"}, "cursor": {"run_id": 1, "created_at": "2026-11-20T00:00:00Z"},
    }))
    # A legacy `pending` key from the removed re-resolution subsystem is
    # dropped, not unioned: those runs' fences live in the samples ledger.
    (theirs / "state.json").write_text(json.dumps({
        "config": {"config_id": "cfg-x"}, "cursor": {"run_id": 3, "created_at": "2026-11-22T00:00:00Z"},
        "pending": [{"run_id": 2, "created_at": "2026-11-21T00:00:00Z", "run_number": 2, "branch": "main"}],
    }))
    tracker.merge_into_state_dir(ours, theirs)
    state = json.loads((ours / "state.json").read_text())
    assert state["cursor"]["run_id"] == 3  # the newer cursor wins regardless of side
    assert "pending" not in state


# --- fake gh plumbing for the end-to-end harvest -------------------------------------

FAKE_GH = r'''#!/usr/bin/env python3
import os, re, sys, time, urllib.parse
root = os.environ["HEADROOM_FAKE_GH_DIR"]
args = sys.argv[1:]
assert args[:1] == ["api"], args
path = args[1]
base, _, query = path.partition("?")
params = urllib.parse.parse_qs(query)
keep = []
for key in sorted(params):
    if key in ("created", "per_page", "event"):
        continue
    keep.append(key + "=" + params[key][0])
name = base.replace("/", "_")
if keep:
    name += "@" + "&".join(keep)
name = re.sub(r"[^A-Za-z0-9_.@=-]", "_", name)
log = os.environ.get("HEADROOM_FAKE_GH_LOG")
if log:
    with open(log, "a") as fh:
        fh.write(path + "\n")
delay = os.environ.get("HEADROOM_FAKE_GH_DELAY")
if delay:
    time.sleep(float(delay))
target = os.path.join(root, name + ".txt")
if os.path.exists(target + ".forbidden"):
    sys.stderr.write("gh: forbidden (HTTP 403)\n")
    sys.exit(1)
if not os.path.exists(target):
    sys.stderr.write("gh: Not Found (HTTP 404)\n")
    sys.exit(1)
sys.stdout.write(open(target).read())
'''


def norm_path(path: str) -> str:
    base, _, query = path.partition("?")
    params = urllib.parse.parse_qs(query)
    keep = []
    for key in sorted(params):
        if key in ("created", "per_page", "event"):
            continue
        keep.append(f"{key}={params[key][0]}")
    name = base.replace("/", "_")
    if keep:
        name += "@" + "&".join(keep)
    return re.sub(r"[^A-Za-z0-9_.@=-]", "_", name)


def make_run(run_id, number, branch, created, sha, conclusion="success", attempt=1):
    return {
        "id": run_id,
        "run_number": number,
        "run_attempt": attempt,
        "head_branch": branch,
        "head_sha": sha,
        "created_at": created,
        "status": "completed" if conclusion else "in_progress",
        "conclusion": conclusion,
        "html_url": f"https://github.com/LoveDaisy/Lumice/actions/runs/{run_id}",
    }


def serve(root: Path, path: str, content: str) -> None:
    (root / (norm_path(path) + ".txt")).write_text(content)


def setup_fake_gh(
    tmp_path: Path, runs: list[dict], jobs_and_logs: dict, registries: dict,
    release_branches="auto",
) -> tuple[Path, dict]:
    """Install the fake gh binary and the canned responses.

    jobs_and_logs: {run_id: {"jobs": {job_name: (job_id, conclusion)}, "logs": {job_id: text}}}
    registries: {sha: registry_text}
    release_branches: "auto" derives the extant release/* set from the runs;
      a list is served verbatim (empty = every release branch retired);
      None does not serve the endpoint at all (discovery fails with a 404).
    """
    root = tmp_path / "fakegh"
    root.mkdir()
    bin_path = tmp_path / "fake-gh-bin"
    bin_path.write_text(FAKE_GH)
    bin_path.chmod(0o755)
    env = {
        "HEADROOM_FAKE_GH_DIR": str(root),
        "HEADROOM_FAKE_GH_LOG": str(tmp_path / "requests.log"),
    }
    if release_branches == "auto":
        release_branches = sorted(
            {r["head_branch"] for r in runs if (r.get("head_branch") or "").startswith("release/")}
        )
    if release_branches is not None:
        serve(root, "repos/O/R/branches?per_page=100&page=1",
              json.dumps([{"name": name} for name in release_branches]))
    serve(root, "repos/O/R/actions/workflows/ci.yml/runs?event=push&per_page=100&page=1",
          json.dumps({"total_count": len(runs), "workflow_runs": runs}))
    for run_id, spec in jobs_and_logs.items():
        jobs_payload = {
            "total_count": len(spec["jobs"]),
            "jobs": [
                {"id": job_id, "name": name, "status": "completed", "conclusion": conclusion, "html_url": ""}
                for name, (job_id, conclusion) in spec["jobs"].items()
            ],
        }
        serve(root, f"repos/O/R/actions/runs/{run_id}/attempts/1/jobs", json.dumps(jobs_payload))
        for job_id, text in spec["logs"].items():
            serve(root, f"repos/O/R/actions/jobs/{job_id}/logs", text)
    for sha, text in registries.items():
        serve(root, f"repos/O/R/contents/test/duration_registry.json?ref={sha}",
              contents_payload(text, f"blob-{sha[:8]}"))
    return bin_path, env


# The synthetic wall clock handed to every harvest subprocess (see
# HARVEST_BOOTSTRAP below): fixed forever, never read from the machine
# running the test. It sits strictly between run 881 (2026-11-20) and run
# 882 (2026-11-25) so the production config-change marker fences the era
# exactly where the threshold-change scenario needs it, and after every
# fixture run date in the no-new-run scenarios (the newest is run 103,
# 2026-11-22). Fixture run dates and this moment form a closed ordering;
# no assertion compares any of them to the real clock.
SYNTHETIC_NOW = "2026-11-23T12:00:00Z"

# The bootstrap that runs the REAL production entry point inside the
# harvest subprocess, with exactly one test-side patch: `utc_now_iso` is
# pinned to SYNTHETIC_NOW, so the calendar input that fences config-change
# eras (and the audit-only harvested_at / fired_at stamps) is synthetic
# instead of the executing machine's clock. Every call site resolves that
# function through module globals at call time, so the patch intercepts
# all of them; argument parsing, the harvest pipeline, stdout/stderr and
# the exit code propagate exactly as when the script is run directly.
HARVEST_BOOTSTRAP = (
    "import sys\n"
    f"sys.path.insert(0, {str(SCRIPTS)!r})\n"
    "import ctest_headroom_tracker as tracker\n"
    f"tracker.utc_now_iso = lambda: {SYNTHETIC_NOW!r}\n"
    "sys.exit(tracker.main())\n"
)


def run_harvest(tmp_path: Path, bin_path: Path, env: dict, *, extra_args=(), publish_remote=None, publish=False):
    state_dir = tmp_path / "state"
    repo_root = tmp_path / "repo-root"
    repo_root.mkdir(exist_ok=True)
    subprocess.run(["git", "init", "-q", str(repo_root)], check=True)
    args = [
        sys.executable, "-c", HARVEST_BOOTSTRAP,
        "harvest",
        "--repo", "O/R",
        "--state-dir", str(state_dir),
        "--repo-root", str(repo_root),
        "--gh-bin", str(bin_path),
        "--window-days", "30",
        "--emit-annotation",
        *extra_args,
    ]
    if publish:
        args += ["--publish", "--publish-remote", publish_remote]
    process = subprocess.run(
        args, env={**os.environ, **env}, capture_output=True, text=True, timeout=120,
    )
    return process, state_dir


MAIN_REGISTRY = registry_payload([machine_budget(MACOS_JOB, 560), machine_budget(WINDOWS_JOB, 480)])
RELEASE_REGISTRY = registry_payload([machine_budget(WINDOWS_JOB, 480)])


def basic_scenario(tmp_path: Path):
    """Two main runs (macOS low twice) + one release red (Windows, low once)."""
    runs = [
        make_run(103, 2154, "release/4.7.7", "2026-11-22T00:00:00Z", "sha-rel1", conclusion="failure"),
        make_run(102, 2156, "main", "2026-11-21T00:00:00Z", "sha-main2"),
        make_run(101, 2155, "main", "2026-11-20T00:00:00Z", "sha-main1"),
    ]
    jobs_and_logs = {
        101: {"jobs": {MACOS_JOB: (9001, "success"), WINDOWS_JOB: (9002, "success")},
              "logs": {9001: MAIN_GREEN_MACOS, 9002: "log without checker lines"}},
        102: {"jobs": {MACOS_JOB: (9011, "success"), WINDOWS_JOB: (9012, "success")},
              "logs": {9011: MAIN_GREEN_MACOS.replace("537.7", "580.0").replace("1.018", "1.000"),
                       9012: "log without checker lines"}},
        103: {"jobs": {WINDOWS_JOB: (9021, "failure")}, "logs": {9021: RELEASE_RED}},
    }
    registries = {"sha-main1": MAIN_REGISTRY, "sha-main2": MAIN_REGISTRY, "sha-rel1": RELEASE_REGISTRY}
    return setup_fake_gh(tmp_path, runs, jobs_and_logs, registries)


def test_end_to_end_harvest_fires_once_annotates_and_persists(tmp_path):
    bin_path, env = basic_scenario(tmp_path)
    process, state_dir = run_harvest(tmp_path, bin_path, env)
    assert process.returncode == 0, process.stdout + process.stderr

    samples = [json.loads(line) for line in (state_dir / "samples.jsonl").read_text().splitlines() if line]
    by_key = {s["key"]: s for s in samples}
    assert set(by_key) == {
        "101:1:macOS ARM64:ctest", "102:1:macOS ARM64:ctest", "103:1:Windows MSVC x86_64:ctest",
        "101:1:Windows MSVC x86_64:ctest",  # honest gap: its log has no Windows reading
        "102:1:Windows MSVC x86_64:ctest",
    }
    assert by_key["101:1:macOS ARM64:ctest"]["type"] == "sample"
    assert by_key["101:1:macOS ARM64:ctest"]["headroom_pct"] == pytest.approx(5.68, abs=0.01)
    assert by_key["102:1:Windows MSVC x86_64:ctest"]["type"] == "gap"
    assert "no-machine-factor-reading" in by_key["102:1:Windows MSVC x86_64:ctest"]["reason"]
    release = by_key["103:1:Windows MSVC x86_64:ctest"]
    assert release["type"] == "sample" and release["raw_factor"] == 1.174
    assert release["implied_c"] == pytest.approx(481.5, abs=0.1)

    events = [json.loads(line) for line in (state_dir / "events.jsonl").read_text().splitlines() if line]
    assert len(events) == 1
    assert events[0]["line"] == "main" and events[0]["job"] == MACOS_JOB
    assert events[0]["first_run"]["id"] == 101 and events[0]["latest_run"]["id"] == 102

    notices = [line for line in process.stdout.splitlines() if line.startswith("::notice::")]
    assert len(notices) == 1 and "[ctest-headroom] main/macOS ARM64/ctest" in notices[0]
    assert "::warning::" not in process.stdout
    assert "[2154]" in process.stdout  # the release run number is traceable in the summary
    assert "NOT PERSISTED" not in process.stdout

    cursor = json.loads((state_dir / "state.json").read_text())["cursor"]
    assert cursor["run_id"] == 103 and cursor["created_at"] == "2026-11-22T00:00:00Z"


def test_second_harvest_is_idempotent_and_keeps_the_active_signal(tmp_path):
    bin_path, env = basic_scenario(tmp_path)
    first, state_dir = run_harvest(tmp_path, bin_path, env)
    assert first.returncode == 0
    events_before = (state_dir / "events.jsonl").read_text()

    second, state_dir = run_harvest(tmp_path, bin_path, env)
    assert second.returncode == 0
    assert "0 run(s) processed" in second.stdout  # overlap only, no re-fetch
    events_after = (state_dir / "events.jsonl").read_text()
    assert events_after == events_before  # no duplicate event
    notices = [line for line in second.stdout.splitlines() if line.startswith("::notice::")]
    assert len(notices) == 1  # the active signal persists across harvests


def test_backlog_continues_across_harvests_without_losing_the_streak(tmp_path):
    bin_path, env = basic_scenario(tmp_path)
    first, state_dir = run_harvest(tmp_path, bin_path, env, extra_args=("--max-runs", "1"))
    assert first.returncode == 0
    assert "run budget (1) reached" in first.stdout
    assert "Backlog" in first.stdout
    samples = [json.loads(line) for line in (state_dir / "samples.jsonl").read_text().splitlines() if line]
    keys = {record["key"] for record in samples}
    assert "101:1:macOS ARM64:ctest" in keys
    # The two backlog runs carry continuity fences until collected.
    assert {r["key"] for r in samples if r.get("reason") == "run-unprocessed"} == {
        "102:1::", "103:1::",
    }
    events = (state_dir / "events.jsonl").read_text().strip()
    assert events == ""  # one low + fenced holes never fires

    second, state_dir = run_harvest(tmp_path, bin_path, env)
    assert second.returncode == 0
    samples = [json.loads(line) for line in (state_dir / "samples.jsonl").read_text().splitlines() if line]
    assert not [r for r in samples if r.get("reason") == "run-unprocessed"]  # fences retired once collected
    events = [json.loads(line) for line in (state_dir / "events.jsonl").read_text().splitlines() if line]
    assert len(events) == 1 and events[0]["first_run"]["id"] == 101  # recompute spans the backlog boundary


def test_gap_between_lows_blocks_the_trigger_end_to_end(tmp_path):
    runs = [
        make_run(201, 1, "main", "2026-11-20T00:00:00Z", "sha-a"),
        make_run(202, 2, "main", "2026-11-21T00:00:00Z", "sha-b"),
        make_run(203, 3, "main", "2026-11-22T00:00:00Z", "sha-c"),
    ]
    low_log = MAIN_GREEN_MACOS.replace("537.7", "600.0").replace("1.018", "1.000")  # h=-7.14 low
    jobs_and_logs = {
        201: {"jobs": {MACOS_JOB: (3001, "success")}, "logs": {3001: low_log}},
        202: {"jobs": {MACOS_JOB: (3002, "success")}, "logs": {3002: "##[notice]nothing useful"}},
        203: {"jobs": {MACOS_JOB: (3003, "success")}, "logs": {3003: low_log}},
    }
    registries = {"sha-a": MAIN_REGISTRY, "sha-b": MAIN_REGISTRY, "sha-c": MAIN_REGISTRY}
    bin_path, env = setup_fake_gh(tmp_path, runs, jobs_and_logs, registries)
    process, state_dir = run_harvest(tmp_path, bin_path, env)
    assert process.returncode == 0
    events = (state_dir / "events.jsonl").read_text().strip()
    assert events == ""  # low / gap / low never pairs
    assert "::notice::" not in process.stdout


def test_attempt_one_red_survives_a_green_rerun_and_attempt_two_is_never_fetched(tmp_path):
    runs = [make_run(301, 1, "main", "2026-11-20T00:00:00Z", "sha-a", conclusion="failure", attempt=2)]
    red_log = RELEASE_RED.replace(WINDOWS_JOB, MACOS_JOB)  # the attempt-1 leg went red
    green_log = MAIN_GREEN_MACOS
    bin_path, env = setup_fake_gh(
        tmp_path, runs,
        {301: {"jobs": {MACOS_JOB: (4001, "failure")}, "logs": {4001: red_log}}},
        {"sha-a": MAIN_REGISTRY},
    )
    # Serve a greener attempt 2; the tracker must never ask for it.
    serve(tmp_path / "fakegh", "repos/O/R/actions/runs/301/attempts/2/jobs",
          json.dumps({"total_count": 1, "jobs": [{"id": 4002, "name": MACOS_JOB, "status": "completed",
                                                  "conclusion": "success", "html_url": ""}]}))
    serve(tmp_path / "fakegh", "repos/O/R/actions/jobs/4002/logs", green_log)
    process, state_dir = run_harvest(tmp_path, bin_path, env)
    assert process.returncode == 0
    samples = {json.loads(line)["key"]: json.loads(line)
               for line in (state_dir / "samples.jsonl").read_text().splitlines() if line}
    red_sample = samples["301:1:macOS ARM64:ctest"]
    assert red_sample["type"] == "sample"
    assert red_sample["phase_seconds"] == 565.3  # attempt 1's red evidence, not the rerun's green
    assert samples["301:1:Windows MSVC x86_64:ctest"]["reason"] == "job-absent"
    requested = (tmp_path / "requests.log").read_text()
    assert "attempts/2" not in requested


def test_cancelled_run_is_a_named_gap_with_no_log_fetch(tmp_path):
    runs = [make_run(401, 1, "main", "2026-11-20T00:00:00Z", "sha-a", conclusion="cancelled")]
    bin_path, env = setup_fake_gh(
        tmp_path, runs, {401: {"jobs": {MACOS_JOB: (5001, "cancelled")}, "logs": {}}},
        {"sha-a": MAIN_REGISTRY},
    )
    process, state_dir = run_harvest(tmp_path, bin_path, env)
    assert process.returncode == 0
    samples = [json.loads(line) for line in (state_dir / "samples.jsonl").read_text().splitlines() if line]
    assert samples[0]["type"] == "gap" and samples[0]["reason"] == "run-cancelled"
    assert "actions/jobs" not in (tmp_path / "requests.log").read_text()


def test_expired_log_and_missing_registry_snapshot_are_honest_gaps(tmp_path):
    runs = [
        make_run(501, 1, "main", "2026-11-20T00:00:00Z", "sha-gone"),
        make_run(502, 2, "main", "2026-11-21T00:00:00Z", "sha-alive"),
    ]
    bin_path, env = setup_fake_gh(
        tmp_path, runs,
        {
            501: {"jobs": {MACOS_JOB: (6001, "success")}, "logs": {}},  # registry snapshot 404s
            502: {"jobs": {MACOS_JOB: (6002, "success")}, "logs": {}},  # job log 404s (expired)
        },
        {"sha-alive": MAIN_REGISTRY},  # sha-gone has no snapshot either
    )
    process, state_dir = run_harvest(tmp_path, bin_path, env)
    assert process.returncode == 0
    samples = {json.loads(line)["key"]: json.loads(line)
               for line in (state_dir / "samples.jsonl").read_text().splitlines() if line}
    assert samples["501:1::"]["type"] == "gap"
    assert "registry-unreadable" in samples["501:1::"]["reason"]
    assert samples["502:1:macOS ARM64:ctest"]["type"] == "gap"
    assert "log-unreadable" in samples["502:1:macOS ARM64:ctest"]["reason"]
    assert "expired" in samples["502:1:macOS ARM64:ctest"]["reason"]
    # Neither gap may read as health: no sample, no streak, no signal.
    assert (state_dir / "events.jsonl").read_text().strip() == ""


def test_out_of_population_branches_are_counted_not_harvested(tmp_path):
    runs = [
        make_run(601, 1, "feat/some-branch", "2026-11-20T00:00:00Z", "sha-x"),
        make_run(602, 2, "main", "2026-11-21T00:00:00Z", "sha-a"),
    ]
    bin_path, env = setup_fake_gh(
        tmp_path, runs,
        {602: {"jobs": {MACOS_JOB: (7001, "success")}, "logs": {7001: MAIN_GREEN_MACOS}}},
        {"sha-a": MAIN_REGISTRY},
    )
    process, state_dir = run_harvest(tmp_path, bin_path, env)
    assert process.returncode == 0
    assert "1 out-of-population run(s) ignored" in process.stdout
    keys = (state_dir / "samples.jsonl").read_text()
    assert "601" not in keys and "602:1:macOS ARM64:ctest" in keys


def test_current_run_is_harvested_before_it_completes(tmp_path):
    runs = [make_run(701, 1, "main", "2026-11-20T00:00:00Z", "sha-a", conclusion=None)]
    bin_path, env = setup_fake_gh(
        tmp_path, runs,
        {701: {"jobs": {MACOS_JOB: (8001, "success")}, "logs": {8001: MAIN_GREEN_MACOS}}},
        {"sha-a": MAIN_REGISTRY},
    )
    process, state_dir = run_harvest(tmp_path, bin_path, env, extra_args=("--current-run", "701"))
    assert process.returncode == 0
    samples = {json.loads(line)["key"]: json.loads(line)
               for line in (state_dir / "samples.jsonl").read_text().splitlines() if line}
    assert samples["701:1:macOS ARM64:ctest"]["type"] == "sample"
    assert samples["701:1:macOS ARM64:ctest"]["run_id"] == 701
    # The Windows leg of the registry has no job in this synthetic run: an honest gap.
    assert samples["701:1:Windows MSVC x86_64:ctest"]["reason"] == "job-absent"


def test_new_release_branch_gets_its_own_line_and_budget(tmp_path):
    runs = [
        make_run(801, 1, "release/4.7.7", "2026-11-20T00:00:00Z", "sha-old"),
        make_run(802, 2, "release/4.7.8", "2026-11-21T00:00:00Z", "sha-new"),
    ]
    clamp_log = "\n".join([RELEASE_CLAMP_NOTICE, RELEASE_CLAMP_WARNING])
    red_log = RELEASE_RED
    bin_path, env = setup_fake_gh(
        tmp_path, runs,
        {
            801: {"jobs": {WINDOWS_JOB: (9101, "success")}, "logs": {9101: clamp_log}},
            802: {"jobs": {WINDOWS_JOB: (9102, "failure")}, "logs": {9102: red_log}},
        },
        {"sha-old": RELEASE_REGISTRY, "sha-new": RELEASE_REGISTRY},
    )
    process, state_dir = run_harvest(tmp_path, bin_path, env)
    assert process.returncode == 0
    samples = {json.loads(line)["run_id"]: json.loads(line)
               for line in (state_dir / "samples.jsonl").read_text().splitlines() if line}
    clamp = samples[801]
    assert clamp["line"] == "release" and clamp["branch"] == "release/4.7.7"
    assert clamp["raw_factor"] == 0.726  # warning-after-notice association worked
    assert clamp["implied_c"] == pytest.approx(477.7, abs=0.1)  # NOT the clamped 433.5
    assert samples[802]["line"] == "release" and samples[802]["branch"] == "release/4.7.8"
    # Different budget eras stay distinct series records; the release line never
    # inherits a main-line budget.
    assert clamp["budget_seconds"] == 480 and samples[802]["budget_seconds"] == 480


def test_two_release_branches_with_the_same_registry_never_pair(tmp_path):
    # A new release line inherits the old one's budget, rulers and clamp — the
    # segment hash is identical. Series identity must still be per branch, or
    # one low sample on each line pairs into a cross-branch trigger, and one
    # line's healthy reading resets the other's trend.
    low_log = RELEASE_RED  # h = -0.32% low vs the shared B=480 registry
    healthy_log = RELEASE_CLAMP_NOTICE  # h ~ 27% healthy vs B=480
    runs = [
        make_run(851, 1, "release/4.7.7", "2026-11-20T00:00:00Z", "sha-r1"),
        make_run(852, 2, "release/4.7.8", "2026-11-21T00:00:00Z", "sha-r2"),
        make_run(853, 3, "release/4.7.7", "2026-11-22T00:00:00Z", "sha-r3"),
        make_run(854, 4, "release/4.7.8", "2026-11-23T00:00:00Z", "sha-r4"),
    ]
    bin_path, env = setup_fake_gh(
        tmp_path, runs,
        {
            851: {"jobs": {WINDOWS_JOB: (9501, "success")}, "logs": {9501: low_log}},
            852: {"jobs": {WINDOWS_JOB: (9502, "success")}, "logs": {9502: low_log}},
            853: {"jobs": {WINDOWS_JOB: (9503, "success")}, "logs": {9503: low_log}},
            854: {"jobs": {WINDOWS_JOB: (9504, "success")}, "logs": {9504: healthy_log}},
        },
        {"sha-r1": RELEASE_REGISTRY, "sha-r2": RELEASE_REGISTRY,
         "sha-r3": RELEASE_REGISTRY, "sha-r4": RELEASE_REGISTRY},
    )
    process, state_dir = run_harvest(tmp_path, bin_path, env)
    assert process.returncode == 0
    events = [json.loads(line) for line in (state_dir / "events.jsonl").read_text().splitlines() if line]
    assert len(events) == 1
    assert events[0]["branch"] == "release/4.7.7"  # 4.7.8's low never pairs into 4.7.7's streak
    assert events[0]["first_run"]["id"] == 851 and events[0]["latest_run"]["id"] == 853
    # 4.7.8's healthy reading must not clear 4.7.7's active signal.
    assert "::notice::" in process.stdout
    assert "release:release/4.7.8" in process.stdout  # the second series is reported under its own branch


def test_deleted_release_branch_stops_signaling_but_keeps_its_history(tmp_path):
    # release/4.7.7 fires (two lows) while extant; the branch is then deleted
    # and a later main push is harvested: the retired line's history stays in
    # the ledger, but it must not keep annotating as ACTIVE — and the summary
    # must say "retired", never silently treat the suppression as health.
    low_rel = RELEASE_RED  # h=-0.32% low vs B=480
    runs1 = [
        make_run(1001, 1, "release/4.7.7", "2026-11-20T00:00:00Z", "sha-r1", conclusion="failure"),
        make_run(1002, 2, "release/4.7.7", "2026-11-21T00:00:00Z", "sha-r2", conclusion="failure"),
    ]
    bin_path, env = setup_fake_gh(
        tmp_path, runs1,
        {
            1001: {"jobs": {WINDOWS_JOB: (9101, "failure")}, "logs": {9101: low_rel}},
            1002: {"jobs": {WINDOWS_JOB: (9102, "failure")}, "logs": {9102: low_rel}},
        },
        {"sha-r1": RELEASE_REGISTRY, "sha-r2": RELEASE_REGISTRY},
    )
    first, state_dir = run_harvest(tmp_path, bin_path, env)
    assert first.returncode == 0
    assert len([l for l in first.stdout.splitlines() if l.startswith("::notice::")]) == 1

    # The branch is deleted; a later main push arrives with a healthy reading.
    runs2 = runs1 + [make_run(1003, 3, "main", "2026-11-23T00:00:00Z", "sha-m1")]
    serve(tmp_path / "fakegh", "repos/O/R/actions/workflows/ci.yml/runs?event=push&per_page=100&page=1",
          json.dumps({"total_count": len(runs2), "workflow_runs": runs2}))
    serve(tmp_path / "fakegh", "repos/O/R/branches?per_page=100&page=1", json.dumps([]))  # deleted
    serve(tmp_path / "fakegh", "repos/O/R/actions/runs/1003", json.dumps(runs2[-1]))
    serve(tmp_path / "fakegh", "repos/O/R/actions/runs/1003/attempts/1/jobs",
          json.dumps({"total_count": 1, "jobs": [{"id": 9103, "name": MACOS_JOB, "status": "completed",
                                                  "conclusion": "success", "html_url": ""}]}))
    serve(tmp_path / "fakegh", "repos/O/R/actions/jobs/9103/logs", MAIN_GREEN_MACOS)
    serve(tmp_path / "fakegh", "repos/O/R/contents/test/duration_registry.json?ref=sha-m1",
          contents_payload(MAIN_REGISTRY, "blob-m1"))
    second, state_dir = run_harvest(tmp_path, bin_path, env)
    assert second.returncode == 0
    assert len([l for l in second.stdout.splitlines() if l.startswith("::notice::")]) == 0  # retired is silent
    assert "retired" in second.stdout  # the summary names the suppression explicitly
    events = [json.loads(line) for line in (state_dir / "events.jsonl").read_text().splitlines() if line]
    assert len(events) == 1 and events[0]["branch"] == "release/4.7.7"  # history preserved, never re-fired
    samples = {json.loads(line)["key"]
               for line in (state_dir / "samples.jsonl").read_text().splitlines() if line}
    assert "1001:1:Windows MSVC x86_64:ctest" in samples  # retired samples stay in the ledger


def test_backfill_window_skips_a_deleted_branchs_runs_entirely(tmp_path):
    # Cold start, full window: two low runs on a release branch that no
    # longer exists must not enter the population. The old prefix-only rule
    # would harvest them and pair their lows into a trigger for a line the
    # owner has already retired.
    low_rel = RELEASE_RED
    runs = [
        make_run(1021, 1, "release/4.7.6", "2026-11-20T00:00:00Z", "sha-d1", conclusion="failure"),
        make_run(1022, 2, "release/4.7.6", "2026-11-21T00:00:00Z", "sha-d2", conclusion="failure"),
        make_run(1023, 3, "main", "2026-11-22T00:00:00Z", "sha-m2"),
    ]
    bin_path, env = setup_fake_gh(
        tmp_path, runs,
        {
            1021: {"jobs": {WINDOWS_JOB: (9101, "failure")}, "logs": {9101: low_rel}},
            1022: {"jobs": {WINDOWS_JOB: (9102, "failure")}, "logs": {9102: low_rel}},
            1023: {"jobs": {MACOS_JOB: (9103, "success")}, "logs": {9103: MAIN_GREEN_MACOS}},
        },
        {"sha-d1": RELEASE_REGISTRY, "sha-d2": RELEASE_REGISTRY, "sha-m2": MAIN_REGISTRY},
        release_branches=[],  # the only release line was retired before the window
    )
    process, state_dir = run_harvest(tmp_path, bin_path, env)
    assert process.returncode == 0
    assert "2 out-of-population run(s) ignored" in process.stdout
    keys = (state_dir / "samples.jsonl").read_text()
    assert "1021" not in keys and "1022" not in keys  # retired runs are not harvested at all
    assert "1023:1:macOS ARM64:ctest" in keys
    assert (state_dir / "events.jsonl").read_text().strip() == ""  # retired lows never pair


def test_branch_discovery_failure_defers_release_signals_and_recovery_fires(tmp_path):
    # A failed branch read is NOT "all retired": release evidence is still
    # collected (fail-open), but release-line signals are deferred
    # (fail-closed) with a named warning while main stays unaffected. When
    # discovery recovers, the deferred signal fires — evidence first
    # collected under unknown coverage is not lost.
    low_rel = RELEASE_RED
    low_main = MAIN_GREEN_MACOS.replace("537.7", "600.0").replace("1.018", "1.000")  # h=-7.14
    runs = [
        make_run(1031, 1, "release/4.7.7", "2026-11-20T00:00:00Z", "sha-r1", conclusion="failure"),
        make_run(1032, 2, "release/4.7.7", "2026-11-21T00:00:00Z", "sha-r2", conclusion="failure"),
        make_run(1033, 3, "main", "2026-11-20T06:00:00Z", "sha-a1"),
        make_run(1034, 4, "main", "2026-11-21T06:00:00Z", "sha-a2"),
    ]
    bin_path, env = setup_fake_gh(
        tmp_path, runs,
        {
            1031: {"jobs": {WINDOWS_JOB: (9101, "failure")}, "logs": {9101: low_rel}},
            1032: {"jobs": {WINDOWS_JOB: (9102, "failure")}, "logs": {9102: low_rel}},
            1033: {"jobs": {MACOS_JOB: (9103, "success")}, "logs": {9103: low_main}},
            1034: {"jobs": {MACOS_JOB: (9104, "success")}, "logs": {9104: low_main}},
        },
        {"sha-r1": RELEASE_REGISTRY, "sha-r2": RELEASE_REGISTRY,
         "sha-a1": MAIN_REGISTRY, "sha-a2": MAIN_REGISTRY},
        release_branches=None,  # the branches endpoint is unreadable this harvest
    )
    first, state_dir = run_harvest(tmp_path, bin_path, env)
    assert first.returncode == 0
    assert "release branch discovery failed" in first.stdout  # the named warning
    assert "deferred" in first.stdout  # the summary says deferred, not healthy
    samples = {json.loads(line)["key"]
               for line in (state_dir / "samples.jsonl").read_text().splitlines() if line}
    assert "1031:1:Windows MSVC x86_64:ctest" in samples  # fail-open: evidence still collected
    notices = [l for l in first.stdout.splitlines() if l.startswith("::notice::")]
    assert len(notices) == 1 and "main/macOS ARM64/ctest" in notices[0]  # main unaffected
    events = [json.loads(line) for line in (state_dir / "events.jsonl").read_text().splitlines() if line]
    assert {event["line"] for event in events} == {"main"}  # release deferred, nothing fired

    # Discovery recovers: the deferred release signal fires on the next walk.
    serve(tmp_path / "fakegh", "repos/O/R/branches?per_page=100&page=1",
          json.dumps([{"name": "release/4.7.7"}]))
    second, state_dir = run_harvest(tmp_path, bin_path, env)
    assert second.returncode == 0
    notices = [l for l in second.stdout.splitlines() if l.startswith("::notice::")]
    assert len(notices) == 2  # main active + the deferred release signal
    assert any("release:release/4.7.7" in line for line in notices)
    events = [json.loads(line) for line in (state_dir / "events.jsonl").read_text().splitlines() if line]
    assert {event["line"] for event in events} == {"main", "release"}


def test_in_progress_run_between_lows_fences_and_upgrades_on_completion(tmp_path):
    # A run discovered while still in progress must appear in the timeline the
    # moment it is skipped — a fenceless hole would leave the lows around it
    # adjacent and pair them. When it completes, the fence upgrades into the
    # run's real evidence in the same ledger positions.
    runs = [
        make_run(891, 1, "main", "2026-11-20T00:00:00Z", "sha-a"),
        make_run(892, 2, "main", "2026-11-21T00:00:00Z", "sha-b", conclusion=None),
        make_run(893, 3, "main", "2026-11-22T00:00:00Z", "sha-c"),
    ]
    low_log = MAIN_GREEN_MACOS.replace("537.7", "600.0").replace("1.018", "1.000")  # h=-7.14
    bin_path, env = setup_fake_gh(
        tmp_path, runs,
        {
            891: {"jobs": {MACOS_JOB: (9901, "success")}, "logs": {9901: low_log}},
            893: {"jobs": {MACOS_JOB: (9903, "success")}, "logs": {9903: low_log}},
        },
        {"sha-a": MAIN_REGISTRY, "sha-c": MAIN_REGISTRY},
    )
    first, state_dir = run_harvest(tmp_path, bin_path, env)
    assert first.returncode == 0
    samples = {json.loads(line)["key"]: json.loads(line)
               for line in (state_dir / "samples.jsonl").read_text().splitlines() if line}
    assert samples["892:1::"]["reason"] == "run-in-progress"  # the fence is in the ledger
    assert (state_dir / "events.jsonl").read_text().strip() == ""  # low / in-progress / low never pairs
    state = json.loads((state_dir / "state.json").read_text())
    assert "pending" not in state  # no identity is carried across harvests

    # The run completes: the listing says so on the next harvest; the real
    # evidence replaces the stale fence via the sweep.
    runs_second = [
        make_run(891, 1, "main", "2026-11-20T00:00:00Z", "sha-a"),
        make_run(892, 2, "main", "2026-11-21T00:00:00Z", "sha-b"),
        make_run(893, 3, "main", "2026-11-22T00:00:00Z", "sha-c"),
    ]
    serve(tmp_path / "fakegh", "repos/O/R/actions/workflows/ci.yml/runs?event=push&per_page=100&page=1",
          json.dumps({"total_count": len(runs_second), "workflow_runs": runs_second}))
    serve(tmp_path / "fakegh", "repos/O/R/actions/runs/892/attempts/1/jobs",
          json.dumps({"total_count": 1, "jobs": [{"id": 9902, "name": MACOS_JOB, "status": "completed",
                                                  "conclusion": "success", "html_url": ""}]}))
    serve(tmp_path / "fakegh", "repos/O/R/actions/jobs/9902/logs", low_log)
    serve(tmp_path / "fakegh", "repos/O/R/contents/test/duration_registry.json?ref=sha-b",
          contents_payload(MAIN_REGISTRY, "blob-b"))
    second, state_dir = run_harvest(tmp_path, bin_path, env)
    assert second.returncode == 0
    samples = {json.loads(line)["key"]: json.loads(line)
               for line in (state_dir / "samples.jsonl").read_text().splitlines() if line}
    assert samples["892:1:macOS ARM64:ctest"]["type"] == "sample"
    assert "892:1::" not in samples  # the stale fence is retired once the run is collected
    state = json.loads((state_dir / "state.json").read_text())
    assert "pending" not in state
    events = [json.loads(line) for line in (state_dir / "events.jsonl").read_text().splitlines() if line]
    assert len(events) == 1 and events[0]["first_run"]["id"] == 891  # three consecutive lows now pair


def test_repeated_config_transition_keeps_every_fence():
    # A transition may legitimately recur (6->7->6->7): each occurrence is
    # its own fence. The transition-pair key of earlier rounds let the
    # upgrade-only merge swallow the newest fence into the first one, and the
    # era boundary silently fell back to the previous return.
    m1 = tracker.config_change_marker(CFG, OTHER_CFG)
    m1["run_created_at"] = "2026-11-20T12:00:00Z"
    m2 = tracker.config_change_marker(OTHER_CFG, CFG)
    m2["run_created_at"] = "2026-11-21T12:00:00Z"
    m3 = tracker.config_change_marker(CFG, OTHER_CFG)
    m3["run_created_at"] = "2026-11-22T12:00:00Z"
    merged = tracker.merge_records([m1], [m3])
    assert {m["key"] for m in merged} == {m1["key"], m3["key"]}  # no swallow
    history = [
        make_sample(1, 560.0, 1.0, 560.0, created="2026-11-20T00:00:00Z", config=CFG),
        make_sample(2, 580.0, 1.0, 560.0, created="2026-11-20T06:00:00Z", config=CFG),
        m1,
        make_sample(3, 480.0, 1.0, 560.0, created="2026-11-21T00:00:00Z", config=OTHER_CFG),  # healthy at 6, low at 4?
        m2,
        make_sample(4, 560.0, 1.0, 560.0, created="2026-11-22T00:00:00Z", config=CFG),
        make_sample(5, 580.0, 1.0, 560.0, created="2026-11-22T06:00:00Z", config=CFG),
        m3,
        make_sample(6, 590.0, 1.0, 560.0, created="2026-11-23T00:00:00Z", config=OTHER_CFG),
    ]
    # Current config OTHER (the second 6->7): the era starts after m3, so only
    # sample 6 evaluates — one low, no fire, and samples 4-5 (an OLD cfg-6 era
    # misread under the new threshold) never pair.
    grouped = tracker.group_series(history)
    key = next(k for k in grouped if k[0] == "main" and k[2] == MACOS_JOB)
    result = tracker.walk_series(grouped[key], tracker.config_identity(4.0, 2), 4.0, 2)
    assert result["streak"] == 1 and result["fired"] == [] and result["active"] is None


def test_gh_timeout_is_a_named_warning_with_a_summary_not_a_traceback(tmp_path):
    bin_path, env = basic_scenario(tmp_path)
    env = {**env, "HEADROOM_FAKE_GH_DELAY": "2"}
    process, state_dir = run_harvest(tmp_path, bin_path, env, extra_args=("--api-timeout-sec", "0.5"))
    assert process.returncode == 0
    assert "gh-timeout" in process.stdout
    assert "::warning::[ctest-headroom] run listing failed" in process.stdout
    assert "Traceback" not in process.stdout and "Traceback" not in process.stderr
    assert "Ctest headroom" in (state_dir / "summary.md").read_text()


def test_crash_leaves_a_named_warning_and_a_summary(tmp_path):
    bin_path, env = basic_scenario(tmp_path)
    # Poison the listing payload: a JSONDecodeError escapes harvest_command
    # and must be caught by the top-level guard, not printed as a traceback.
    serve(tmp_path / "fakegh", "repos/O/R/actions/workflows/ci.yml/runs?event=push&per_page=100&page=1", "{not json")
    process, state_dir = run_harvest(tmp_path, bin_path, env)
    assert process.returncode == 2
    assert "::warning::[ctest-headroom] harvest crashed: JSONDecodeError" in process.stdout
    assert "HARVEST CRASHED" in (state_dir / "summary.md").read_text()


def test_legacy_pending_entries_become_permanent_abandoned_gaps(tmp_path):
    # A state written by the removed re-resolution subsystem pins runs the
    # listing can no longer see (they sit behind the cursor). The discharge
    # is a PERMANENT line-wide gap: the hole is recorded, never backfilled,
    # and no identity is queried by id — ever.
    bin_path, env = setup_fake_gh(tmp_path, [], {}, {})  # empty window: both entries are behind the cursor
    state_dir = tmp_path / "state"
    state_dir.mkdir()
    (state_dir / "samples.jsonl").write_text("")
    (state_dir / "events.jsonl").write_text("")
    (state_dir / "state.json").write_text(json.dumps({
        "schema": 1, "config": None,
        "cursor": {"run_id": 950, "created_at": "2026-11-21T00:00:00Z", "run_number": 950},
        "pending": [
            {"run_id": 911, "created_at": "2026-11-20T00:00:00Z", "run_number": 1, "branch": "main"},
            {"run_id": 913, "created_at": "2026-11-19T00:00:00Z", "run_number": 2, "branch": "release/4.7.7"},
        ],
    }))
    process, state_dir = run_harvest(tmp_path, bin_path, env)
    assert process.returncode == 0
    assert "legacy pinned runs discharged as permanent run-abandoned gaps: 2" in process.stdout
    requested = (tmp_path / "requests.log").read_text()
    assert not re.search(r"/actions/runs/\d+$", requested)  # no by-id identity query, ever
    assert "contents/test/duration_registry.json" not in requested  # nothing signed on an unresolvable run
    samples = [json.loads(line) for line in (state_dir / "samples.jsonl").read_text().splitlines() if line]
    abandoned = {record["key"]: record for record in samples if record.get("reason") == "run-abandoned"}
    assert set(abandoned) == {"911:1::", "913:1::"}
    assert all(record["type"] == "gap" and record["job"] == "" for record in abandoned.values())
    assert all(not tracker.is_retryable_gap(record) for record in abandoned.values())  # permanent
    assert {record["branch"] for record in abandoned.values()} == {"main", "release/4.7.7"}
    state = json.loads((state_dir / "state.json").read_text())
    assert "pending" not in state  # the queue is gone for good

    # Idempotent: the second harvest re-derives the same ledger and fires nothing.
    events_before = (state_dir / "events.jsonl").read_text()
    second, state_dir = run_harvest(tmp_path, bin_path, env)
    assert second.returncode == 0
    assert "run-abandoned gaps" not in second.stdout  # discharge happened once
    samples_after = [json.loads(line) for line in (state_dir / "samples.jsonl").read_text().splitlines() if line]
    assert {record["key"] for record in samples_after} == {record["key"] for record in samples}
    assert (state_dir / "events.jsonl").read_text() == events_before


def test_legacy_pending_entry_still_listed_is_collected_not_abandoned(tmp_path):
    # A pinned run the window listing still presents has a resolvable identity
    # this round: it is collected normally — the listing is the only identity
    # source, and no abandoned gap is written for it.
    run = make_run(931, 1, "main", "2026-11-20T00:00:00Z", "sha-a")
    bin_path, env = setup_fake_gh(
        tmp_path, [run],
        {931: {"jobs": {MACOS_JOB: (9931, "success")}, "logs": {9931: MAIN_GREEN_MACOS}}},
        {"sha-a": MAIN_REGISTRY},
    )
    state_dir = tmp_path / "state"
    state_dir.mkdir()
    (state_dir / "samples.jsonl").write_text("")
    (state_dir / "events.jsonl").write_text("")
    (state_dir / "state.json").write_text(json.dumps({
        "schema": 1, "config": None,
        "cursor": {"run_id": 930, "created_at": "2026-11-19T00:00:00Z", "run_number": 0},
        "pending": [{"run_id": 931, "created_at": "2026-11-20T00:00:00Z", "run_number": 1, "branch": "main"}],
    }))
    process, state_dir = run_harvest(tmp_path, bin_path, env)
    assert process.returncode == 0
    samples = [json.loads(line) for line in (state_dir / "samples.jsonl").read_text().splitlines() if line]
    assert any(record["key"] == "931:1:macOS ARM64:ctest" and record["type"] == "sample" for record in samples)
    assert not [record for record in samples if record.get("reason") == "run-abandoned"]
    state = json.loads((state_dir / "state.json").read_text())
    assert "pending" not in state


def test_legacy_removed_reason_rows_are_dropped_at_the_read_edge():
    # The removed re-resolution subsystem's own rows (`run-pending` backlog
    # fences, `run-not-found` identity records) are mechanism residue, not
    # evidence: dropped wherever a ledger is read. The current vocabulary —
    # `run-unprocessed` fences and discharged `run-abandoned` gaps — survives.
    keep = [
        make_sample(1, 560.0, 1.0, 560.0, created="2026-11-20T00:00:00Z"),
        make_gap(2, created="2026-11-21T00:00:00Z", job="", reason="run-unprocessed"),
        make_gap(3, created="2026-11-22T00:00:00Z", job="", reason="run-abandoned"),
        make_gap(4, created="2026-11-23T00:00:00Z", job="", reason="run-in-progress"),
    ]
    drop = [
        make_gap(5, created="2026-11-19T00:00:00Z", job="", reason="run-pending"),
        make_gap(6, created="2026-11-19T06:00:00Z", job="", reason="run-not-found"),
    ]
    kept = tracker.drop_legacy_removed_reason_rows(keep + drop)
    assert {record["key"] for record in kept} == {record["key"] for record in keep}
    assert tracker.drop_legacy_removed_reason_rows([]) == []


def test_state_merge_clears_legacy_removed_reason_rows_from_both_sides(tmp_path):
    ours = tmp_path / "ours"
    theirs = tmp_path / "theirs"
    ours.mkdir()
    theirs.mkdir()
    tracker.write_jsonl(ours / "samples.jsonl", [
        make_gap(5, created="2026-11-19T00:00:00Z", job="", reason="run-pending"),
    ])
    tracker.write_jsonl(theirs / "samples.jsonl", [
        make_gap(6, created="2026-11-19T06:00:00Z", job="", reason="run-not-found"),
    ])
    (ours / "state.json").write_text(json.dumps({
        "config": {"config_id": "cfg-x"}, "cursor": {"run_id": 1, "created_at": "2026-11-20T00:00:00Z"},
    }))
    (theirs / "state.json").write_text(json.dumps({
        "config": {"config_id": "cfg-x"}, "cursor": {"run_id": 3, "created_at": "2026-11-22T00:00:00Z"},
    }))
    tracker.merge_into_state_dir(ours, theirs)
    samples = [json.loads(line) for line in (ours / "samples.jsonl").read_text().splitlines() if line]
    assert samples == []  # both sides' residue rows are gone from the union
    state = json.loads((ours / "state.json").read_text())
    assert state["cursor"]["run_id"] == 3


def test_state_merge_legacy_row_cannot_erase_a_live_fence_on_the_same_key(tmp_path):
    # A published ledger written by pre-shrink code carries `run-pending`
    # residue on a key our side fences with a live `run-in-progress` gap.
    # Dropping the residue AFTER merge_records let it win the same-key
    # contest first (non-retryable gap, precedence 1, beats a retryable
    # fence, precedence 0) and then deleted the winner — the fence itself —
    # so low/gap/low recomputed as low/low and fired a pseudo event. The
    # residue is not evidence: it is dropped from each side BEFORE the
    # adjudication and never competes.
    ours = tmp_path / "ours"
    theirs = tmp_path / "theirs"
    ours.mkdir()
    theirs.mkdir()
    tracker.write_jsonl(ours / "samples.jsonl", [
        make_sample(1, 560.0, 1.0, 560.0, created="2026-11-20T00:00:00Z"),  # low
        make_gap(2, created="2026-11-21T00:00:00Z", job="", phase="", reason="run-in-progress"),
        make_sample(3, 560.0, 1.0, 560.0, created="2026-11-22T00:00:00Z"),  # low
    ])
    tracker.write_jsonl(theirs / "samples.jsonl", [
        make_gap(2, created="2026-11-21T00:00:00Z", job="", phase="", reason="run-pending"),  # same key
    ])
    (ours / "state.json").write_text(json.dumps({
        "config": {"config_id": CFG}, "cursor": {"run_id": 3, "created_at": "2026-11-22T00:00:00Z"},
    }))
    (theirs / "state.json").write_text(json.dumps({
        "config": {"config_id": CFG}, "cursor": {"run_id": 1, "created_at": "2026-11-20T00:00:00Z"},
    }))
    tracker.merge_into_state_dir(ours, theirs)
    merged = [json.loads(line) for line in (ours / "samples.jsonl").read_text().splitlines() if line]
    assert not [record for record in merged if record.get("reason") in ("run-pending", "run-not-found")]
    assert [record for record in merged if record["run_id"] == 2 and record.get("reason") == "run-in-progress"]
    # the consequence the defect existed for: the fence still breaks the pairing
    result = walk(merged)
    assert result["fired"] == [] and result["streak"] == 1


def test_legacy_run_pending_fence_row_is_cleared_and_the_run_recollected(tmp_path):
    # A ledger written by the pre-shrink code carries the removed subsystem's
    # budget-backlog fence: reason run-pending, job "", line-wide key. After
    # the shrink that reason is no longer retryable, so the completeness
    # check read it as "collection complete" and the run the window listing
    # still presented was skipped forever. The repair clears the row at the
    # read edge: the identity returns to ordinary listing-driven collection
    # (registry evidence actually lands), the stale row never comes back,
    # and no by-id query or abandoned discharge is involved.
    run = make_run(931, 1, "main", "2026-11-20T00:00:00Z", "sha-a")
    bin_path, env = setup_fake_gh(
        tmp_path, [run],
        {931: {"jobs": {MACOS_JOB: (9931, "success")}, "logs": {9931: MAIN_GREEN_MACOS}}},
        {"sha-a": MAIN_REGISTRY},
    )
    state_dir = tmp_path / "state"
    state_dir.mkdir()
    fence = make_gap(931, created="2026-11-20T00:00:00Z", job="", reason="run-pending")
    (state_dir / "samples.jsonl").write_text(json.dumps(fence) + "\n")
    (state_dir / "events.jsonl").write_text("")
    (state_dir / "state.json").write_text(json.dumps({
        "schema": 1, "config": None,
        "cursor": {"run_id": 930, "created_at": "2026-11-19T00:00:00Z", "run_number": 0},
        "pending": [{"run_id": 931, "created_at": "2026-11-20T00:00:00Z", "run_number": 1, "branch": "main"}],
    }))
    process, state_dir = run_harvest(tmp_path, bin_path, env)
    assert process.returncode == 0
    requested = (tmp_path / "requests.log").read_text()
    assert "contents/test/duration_registry.json" in requested  # collected for real
    assert not re.search(r"/actions/runs/\d+$", requested)  # listing identity only — no by-id query
    samples = [json.loads(line) for line in (state_dir / "samples.jsonl").read_text().splitlines() if line]
    assert any(record["key"] == "931:1:macOS ARM64:ctest" and record["type"] == "sample" for record in samples)
    assert not [record for record in samples if record.get("reason") in ("run-pending", "run-not-found")]
    assert not [record for record in samples if record.get("reason") == "run-abandoned"]  # listed → not discharged
    state = json.loads((state_dir / "state.json").read_text())
    assert "pending" not in state

    # Idempotent: the cleared row stays cleared on the next harvest.
    second, state_dir = run_harvest(tmp_path, bin_path, env)
    assert second.returncode == 0
    samples_after = [json.loads(line) for line in (state_dir / "samples.jsonl").read_text().splitlines() if line]
    assert not [record for record in samples_after if record.get("reason") in ("run-pending", "run-not-found")]
    assert any(record["key"] == "931:1:macOS ARM64:ctest" and record["type"] == "sample" for record in samples_after)


def test_abandoned_gap_breaks_pairing_and_is_never_evidence():
    # The discharged run's gap row fences the timeline at its position — two
    # lows separated by it never pair — and the row itself is never a reading:
    # a terminal discharge can only lose signal, never invent it, and a later
    # retryable fence cannot downgrade it back to retryable.
    records = [
        make_sample(1, 600.0, 1.0, 560.0, created="2026-11-20T00:00:00Z"),  # low
        make_gap(2, created="2026-11-21T00:00:00Z", job="", reason="run-abandoned"),
        make_sample(3, 600.0, 1.0, 560.0, created="2026-11-22T00:00:00Z"),  # low
    ]
    result = walk(records)
    assert result["fired"] == [] and result["streak"] == 1  # barrier, not evidence
    abandoned = make_gap(9, created="2026-11-19T00:00:00Z", job="", reason="run-abandoned")
    assert not tracker.is_retryable_gap(abandoned)
    fence = make_gap(9, created="2026-11-19T00:00:00Z", job="", reason="run-unprocessed")
    merged = tracker.merge_records([abandoned], [fence])
    assert merged[0]["reason"] == "run-abandoned"  # permanence survives the merge


def test_partial_published_state_fails_loudly(tmp_path):
    bare = tmp_path / "partial.git"
    subprocess.run(["git", "init", "-q", "--bare", str(bare)], check=True)
    subprocess.run(["git", "-C", str(bare), "config", "core.hooksPath", str(bare / "hooks")], check=True)
    work = tmp_path / "partial-seed"
    subprocess.run(["git", "init", "-q", str(work)], check=True)
    subprocess.run(["git", "-C", str(work), "config", "user.email", "s@test"], check=True)
    subprocess.run(["git", "-C", str(work), "config", "user.name", "s"], check=True)
    (work / "headroom").mkdir()
    (work / "headroom" / "events.jsonl").write_text("")  # samples.jsonl MISSING: partial authority
    subprocess.run(["git", "-C", str(work), "add", "-A"], check=True)
    subprocess.run(["git", "-C", str(work), "commit", "-q", "-m", "partial"], check=True)
    subprocess.run(["git", "-C", str(work), "branch", "-M", "main"], check=True)
    subprocess.run(["git", "-C", str(work), "push", "-q", str(bare), "main:gh-pages"], check=True)

    repo_root = tmp_path / "repo-root"
    repo_root.mkdir()
    subprocess.run(["git", "init", "-q", str(repo_root)], check=True)
    state_dir = tmp_path / "state"
    state_dir.mkdir()
    tracker.write_jsonl(state_dir / "samples.jsonl", [])
    tracker.write_jsonl(state_dir / "events.jsonl", [])
    (state_dir / "state.json").write_text('{"schema": 1, "config": null, "cursor": null}')
    (state_dir / "summary.md").write_text("local")
    ok, detail = tracker.publish_state(repo_root, str(bare), "gh-pages", state_dir)
    assert not ok
    assert "published-state-corrupt" in detail  # never treated as a fresh initialization


def test_gh_timeout_gaps_are_retryable():
    # A timeout is transient: the same attempt-1 evidence may read fine next
    # harvest, so the gap must classify retryable — before the fix a timeout
    # fossilized into a permanent terminal hole.
    assert tracker.is_retryable_gap(
        make_gap(1, created="2026-11-20T00:00:00Z", reason="gh-timeout: repos/x/y/logs: no output within 120s")
    )
    assert tracker.is_retryable_gap(
        make_gap(2, created="2026-11-20T00:00:00Z", reason="log-unreadable: gh-timeout: repos/x/y/logs")
    )


def test_new_series_lows_cannot_pair_across_a_dead_run(tmp_path):
    # Known series at harvest start: macOS only (from harvest 1). The batch
    # then contains a dead registry run BETWEEN two lows of a NEW Windows leg
    # introduced by the same batch: the fence must cover the new series too,
    # not only the series known when the harvest began.
    runs1 = [make_run(921, 1, "main", "2026-11-20T00:00:00Z", "sha-r1")]
    mac_registry = registry_payload([machine_budget(MACOS_JOB, 560)])
    both_registry = registry_payload([machine_budget(MACOS_JOB, 560), machine_budget(WINDOWS_JOB, 480)])
    low_mac = MAIN_GREEN_MACOS.replace("537.7", "600.0").replace("1.018", "1.000")
    low_win = RELEASE_RED
    bin_path, env = setup_fake_gh(
        tmp_path, runs1,
        {921: {"jobs": {MACOS_JOB: (9921, "success")}, "logs": {9921: low_mac}}},
        {"sha-r1": mac_registry},
    )
    first, state_dir = run_harvest(tmp_path, bin_path, env)
    assert first.returncode == 0

    runs = runs1 + [
        make_run(922, 2, "main", "2026-11-21T00:00:00Z", "sha-r2"),
        make_run(923, 3, "main", "2026-11-22T00:00:00Z", "sha-gone"),
        make_run(924, 4, "main", "2026-11-23T00:00:00Z", "sha-r2b"),
    ]
    for run in runs:
        serve(tmp_path / "fakegh", f"repos/O/R/actions/runs/{run['id']}", json.dumps(run))
    serve(tmp_path / "fakegh", "repos/O/R/actions/workflows/ci.yml/runs?event=push&per_page=100&page=1",
          json.dumps({"total_count": len(runs), "workflow_runs": runs}))
    serve(tmp_path / "fakegh", "repos/O/R/actions/runs/922/attempts/1/jobs",
          json.dumps({"total_count": 1, "jobs": [{"id": 9922, "name": WINDOWS_JOB, "status": "completed",
                                                  "conclusion": "success", "html_url": ""}]}))
    serve(tmp_path / "fakegh", "repos/O/R/actions/jobs/9922/logs", low_win)
    # sha-gone: no registry snapshot (the dead run between the two Windows lows)
    serve(tmp_path / "fakegh", "repos/O/R/actions/runs/924/attempts/1/jobs",
          json.dumps({"total_count": 1, "jobs": [{"id": 9924, "name": WINDOWS_JOB, "status": "completed",
                                                  "conclusion": "success", "html_url": ""}]}))
    serve(tmp_path / "fakegh", "repos/O/R/actions/jobs/9924/logs", low_win)
    serve(tmp_path / "fakegh", "repos/O/R/contents/test/duration_registry.json?ref=sha-r2",
          contents_payload(both_registry, "blob-r2"))
    serve(tmp_path / "fakegh", "repos/O/R/contents/test/duration_registry.json?ref=sha-r2b",
          contents_payload(both_registry, "blob-r2b"))

    second, state_dir = run_harvest(tmp_path, bin_path, env)
    assert second.returncode == 0
    events = (state_dir / "events.jsonl").read_text().strip()
    assert events == ""  # win-low / dead-registry / win-low never pairs on the new leg
    assert "::notice::" not in second.stdout


def test_cold_start_low_cancelled_low_never_fires(tmp_path):
    # First harvest, empty ledger: the cancelled run lands as a line-wide gap
    # (no series known yet) and must still fence the series the same batch
    # discovers, exactly as a named gap would.
    runs = [
        make_run(861, 1, "main", "2026-11-20T00:00:00Z", "sha-a"),
        make_run(862, 2, "main", "2026-11-21T00:00:00Z", "sha-b", conclusion="cancelled"),
        make_run(863, 3, "main", "2026-11-22T00:00:00Z", "sha-c"),
    ]
    low_log = MAIN_GREEN_MACOS.replace("537.7", "600.0").replace("1.018", "1.000")
    bin_path, env = setup_fake_gh(
        tmp_path, runs,
        {
            861: {"jobs": {MACOS_JOB: (9601, "success")}, "logs": {9601: low_log}},
            862: {"jobs": {}, "logs": {}},  # cancelled: no jobs are consulted
            863: {"jobs": {MACOS_JOB: (9603, "success")}, "logs": {9603: low_log}},
        },
        {"sha-a": MAIN_REGISTRY, "sha-c": MAIN_REGISTRY},  # sha-b never read
    )
    process, state_dir = run_harvest(tmp_path, bin_path, env)
    assert process.returncode == 0
    events = (state_dir / "events.jsonl").read_text().strip()
    assert events == ""  # low / cancelled / low never pairs
    assert "::notice::" not in process.stdout


def test_budget_death_fences_the_backlog_and_the_listing_recollects_it(tmp_path):
    # Harvest 1 dies of API budget mid-run-2: no identity is carried across
    # harvests — run 873 (never reached) is fenced with a retryable
    # `run-unprocessed` line-wide gap so the timeline has no silent hole, and
    # harvest 2 re-collects both runs from the window listing (the cursor
    # never moved past them), upgrading the fences into samples.
    runs = [
        make_run(871, 1, "main", "2026-11-20T00:00:00Z", "sha-a"),
        make_run(872, 2, "main", "2026-11-21T00:00:00Z", "sha-b"),
        make_run(873, 3, "main", "2026-11-22T00:00:00Z", "sha-c"),
    ]
    low_log = MAIN_GREEN_MACOS.replace("537.7", "600.0").replace("1.018", "1.000")  # h=-7.14
    jobs_and_logs = {
        871: {"jobs": {MACOS_JOB: (9701, "success")}, "logs": {9701: low_log}},
        872: {"jobs": {MACOS_JOB: (9702, "success")}, "logs": {9702: low_log}},
        873: {"jobs": {MACOS_JOB: (9703, "success")}, "logs": {9703: low_log}},
    }
    registries = {"sha-a": MAIN_REGISTRY, "sha-b": MAIN_REGISTRY, "sha-c": MAIN_REGISTRY}
    bin_path, env = setup_fake_gh(tmp_path, runs, jobs_and_logs, registries)
    # Calls: release-branch discovery 1 + listing 1 + run 871 (registry/jobs/log) 3
    # + run 872 registry 1 + jobs 1 + log REFUSED -> api-budget-exhausted; run 873 never starts.
    first, state_dir = run_harvest(tmp_path, bin_path, env, extra_args=("--max-api-calls", "7"))
    assert first.returncode == 0
    samples = {json.loads(line)["key"]: json.loads(line)
               for line in (state_dir / "samples.jsonl").read_text().splitlines() if line}
    assert samples["872:1:macOS ARM64:ctest"]["type"] == "gap"
    assert "api-budget-exhausted" in samples["872:1:macOS ARM64:ctest"]["reason"]
    assert samples["873:1::"]["reason"] == "run-unprocessed"  # the un-reached run is fenced, not silent
    assert tracker.is_retryable_gap(samples["873:1::"])  # retryable: the listing re-presents it next round
    state = json.loads((state_dir / "state.json").read_text())
    assert "pending" not in state  # no identity crosses the harvest boundary
    assert (state_dir / "events.jsonl").read_text().strip() == ""  # one low + fenced hole fires nothing
    requested = (tmp_path / "requests.log").read_text()
    assert not re.search(r"/actions/runs/\d+$", requested)  # collection never resolves identity by id

    second, state_dir = run_harvest(tmp_path, bin_path, env)
    assert second.returncode == 0
    samples = {json.loads(line)["key"]: json.loads(line)
               for line in (state_dir / "samples.jsonl").read_text().splitlines() if line}
    assert samples["872:1:macOS ARM64:ctest"]["type"] == "sample"  # the retryable gap upgraded
    assert samples["873:1:macOS ARM64:ctest"]["type"] == "sample"
    assert "873:1::" not in samples  # the fence retired once the run was collected
    events = [json.loads(line) for line in (state_dir / "events.jsonl").read_text().splitlines() if line]
    assert len(events) == 1 and events[0]["first_run"]["id"] == 871
    state = json.loads((state_dir / "state.json").read_text())
    assert "pending" not in state


def test_threshold_change_with_no_new_runs_clears_the_active_signal(tmp_path):
    bin_path, env = basic_scenario(tmp_path)
    first, state_dir = run_harvest(tmp_path, bin_path, env)
    assert first.returncode == 0
    assert len([l for l in first.stdout.splitlines() if l.startswith("::notice::")]) == 1
    events_before = (state_dir / "events.jsonl").read_text()

    # Threshold revised 6% -> 7%, no new runs anywhere: the old-era signal
    # must not survive as active, no event may fire under a mixed identity,
    # and the persisted history stays as it was.
    second, state_dir = run_harvest(tmp_path, bin_path, env, extra_args=("--threshold-pct", "7"))
    assert second.returncode == 0
    assert len([l for l in second.stdout.splitlines() if l.startswith("::notice::")]) == 0
    assert "Observation config changed" in second.stdout
    assert (state_dir / "events.jsonl").read_text() == events_before
    assert "ACTIVE" not in second.stdout.split("### Active low-headroom signals")[1].split("###")[0] or \
        "None." in second.stdout.split("### Active low-headroom signals")[1].split("###")[0]


def test_threshold_change_then_new_runs_fire_under_the_new_identity(tmp_path):
    tmp2 = tmp_path / "scenario"
    tmp2.mkdir()
    low = MAIN_GREEN_MACOS.replace("537.7", "600.0").replace("1.018", "1.000")  # low at 6%, low at 7%
    very_low = MAIN_GREEN_MACOS.replace("537.7", "640.0").replace("1.018", "1.000")
    healthy = MAIN_GREEN_MACOS  # h=5.68: low at 6, healthy at 7
    runs = [make_run(881, 1, "main", "2026-11-20T00:00:00Z", "sha-a")]
    bin_path, env = setup_fake_gh(
        tmp2, runs, {881: {"jobs": {MACOS_JOB: (9801, "success")}, "logs": {9801: healthy}}},
        {"sha-a": MAIN_REGISTRY},
    )
    first, state_dir = run_harvest(tmp2, bin_path, env, extra_args=("--threshold-pct", "7"))
    assert first.returncode == 0
    assert (state_dir / "events.jsonl").read_text().strip() == ""  # 5.68% is healthy at 7%

    # Revise to 6% and deliver two low runs created AFTER the change moment
    # (the era boundary is the change; runs the change predates stay in the
    # prior era): the new pair fires strictly under the new config identity.
    runs += [
        make_run(882, 2, "main", "2026-11-25T00:00:00Z", "sha-b"),
        make_run(883, 3, "main", "2026-11-26T00:00:00Z", "sha-c"),
    ]
    for run in runs:
        serve(tmp2 / "fakegh", f"repos/O/R/actions/runs/{run['id']}", json.dumps(run))
    serve(tmp2 / "fakegh", "repos/O/R/actions/workflows/ci.yml/runs?event=push&per_page=100&page=1",
          json.dumps({"total_count": len(runs), "workflow_runs": runs}))
    serve(tmp2 / "fakegh", "repos/O/R/actions/runs/882/attempts/1/jobs",
          json.dumps({"total_count": 1, "jobs": [{"id": 9802, "name": MACOS_JOB, "status": "completed",
                                                  "conclusion": "success", "html_url": ""}]}))
    serve(tmp2 / "fakegh", "repos/O/R/actions/jobs/9802/logs", low)
    serve(tmp2 / "fakegh", "repos/O/R/actions/runs/883/attempts/1/jobs",
          json.dumps({"total_count": 1, "jobs": [{"id": 9803, "name": MACOS_JOB, "status": "completed",
                                                  "conclusion": "success", "html_url": ""}]}))
    serve(tmp2 / "fakegh", "repos/O/R/actions/jobs/9803/logs", very_low)
    serve(tmp2 / "fakegh", "repos/O/R/contents/test/duration_registry.json?ref=sha-b",
          contents_payload(MAIN_REGISTRY, "blob-b"))
    serve(tmp2 / "fakegh", "repos/O/R/contents/test/duration_registry.json?ref=sha-c",
          contents_payload(MAIN_REGISTRY, "blob-c"))

    second, state_dir = run_harvest(tmp2, bin_path, env, extra_args=("--threshold-pct", "6"))
    assert second.returncode == 0
    events = [json.loads(line) for line in (state_dir / "events.jsonl").read_text().splitlines() if line]
    assert len(events) == 1
    assert events[0]["config_id"] == tracker.config_identity(6.0, 2)
    assert events[0]["threshold_pct"] == 6.0
    assert events[0]["first_run"]["id"] == 882  # the change reset the streak; era starts at 882

    # Wall-clock independence witness, read back from production's own ledger
    # write: the era boundary this harvest fenced carries the synthetic change
    # moment handed to the subprocess (see HARVEST_BOOTSTRAP), not the
    # executing machine's clock — and the closed ordering the event assertion
    # rests on is 881 < change < 882 < 883, determined by fixture data alone.
    ledger = [json.loads(line) for line in (state_dir / "samples.jsonl").read_text().splitlines() if line]
    markers = [record for record in ledger if record.get("type") == "config-change"]
    assert len(markers) == 1 and markers[0]["run_created_at"] == SYNTHETIC_NOW
    created = {record["run_id"]: record["run_created_at"] for record in ledger
               if record.get("type") == "sample" and record["line"] == "main" and record["job"] == MACOS_JOB}
    assert created[881] < SYNTHETIC_NOW < created[882] < created[883]  # ISO strings sort chronologically


# --- publish chain --------------------------------------------------------------------


def make_bare_remote(tmp_path: Path) -> tuple[Path, Path]:
    """A throwaway remote with a gh-pages branch carrying bench/ + headroom/."""
    bare = tmp_path / "pub.git"
    subprocess.run(["git", "init", "-q", "--bare", str(bare)], check=True)
    # A global core.hooksPath (if the machine sets one) would redirect hook
    # lookup away from this repo's hooks/ for receive-pack too — pin it back.
    subprocess.run(["git", "-C", str(bare), "config", "core.hooksPath", str(bare / "hooks")], check=True)
    work = tmp_path / "seed"
    subprocess.run(["git", "init", "-q", str(work)], check=True)
    subprocess.run(["git", "-C", str(work), "config", "user.email", "seed@test"], check=True)
    subprocess.run(["git", "-C", str(work), "config", "user.name", "seed"], check=True)
    (work / "bench").mkdir()
    (work / "headroom").mkdir()
    (work / "bench" / "chart.json").write_text("chart-v1")
    (work / "headroom" / "samples.jsonl").write_text("")
    (work / "headroom" / "events.jsonl").write_text("")
    (work / "headroom" / "state.json").write_text('{"schema": 1, "config": null, "cursor": null}')
    (work / "headroom" / "summary.md").write_text("old")
    subprocess.run(["git", "-C", str(work), "add", "-A"], check=True)
    subprocess.run(["git", "-C", str(work), "commit", "-q", "-m", "seed"], check=True)
    subprocess.run(["git", "-C", str(work), "branch", "-M", "main"], check=True)
    subprocess.run(["git", "-C", str(work), "push", "-q", str(bare), "main:gh-pages"], check=True)
    return bare, work


def read_remote_file(bare: Path, ref_path: str) -> str:
    result = subprocess.run(["git", "-C", str(bare), "show", ref_path], capture_output=True, text=True, check=True)
    return result.stdout


def remote_tree_blob(bare: Path, ref_path: str) -> str:
    result = subprocess.run(["git", "-C", str(bare), "rev-parse", ref_path], capture_output=True, text=True, check=True)
    return result.stdout.strip()


def install_concurrent_update_hook(bare: Path) -> None:
    """First push: land a foreign headroom record on the remote, then reject the
    push as non-fast-forward — exactly what a concurrent harvest produces.
    Second push: accept."""
    hook = bare / "hooks" / "pre-receive"
    hook.write_text(
        "#!/bin/bash\n"
        'set -e\n'
        'count_file="$PWD/.pushcount"\n'
        'n=$(($(cat "$count_file" 2>/dev/null || echo 0) + 1))\n'
        'echo "$n" > "$count_file"\n'
        'if [ "$n" != "1" ]; then exit 0; fi\n'
        '# Leave the quarantine: the concurrent commit must survive the rejection.\n'
        'unset GIT_QUARANTINE_PATH GIT_OBJECT_DIRECTORY GIT_ALTERNATE_OBJECT_DIRECTORIES\n'
        'old=$(git rev-parse refs/heads/gh-pages)\n'
        'tmp=$(mktemp -p "$PWD")\n'
        'git cat-file blob "refs/heads/gh-pages:headroom/samples.jsonl" > "$tmp"\n'
        'printf \'{"type":"sample","key":"999:1:macOS ARM64:ctest","line":"main","branch":"main",'
        '"job":"macOS ARM64","phase":"ctest","run_id":999,"run_number":998,"run_url":"",'
        '"attempt":1,"head_sha":"x","run_created_at":"2026-11-23T00:00:00Z","registry_blob_sha":"b",'
        '"registry_sha256":"r","segment":"seg-a","budget_seconds":560,"rulers":7,"phase_seconds":500.0,'
        '"gate_factor":1.0,"raw_factor":1.0,"implied_c":500.0,"headroom_pct":10.71,'
        '"headroom_interval_pct":[10.7,10.8],"approximate":true,"log_lines":["x"],'
        '"harvested_at":"2026-11-24T00:00:00Z","config_id":"CFG"}\\n\' >> "$tmp"\n'
        'newblob=$(git hash-object -w "$tmp")\n'
        'rm -f "$tmp"\n'
        'evt=$(git rev-parse "refs/heads/gh-pages:headroom/events.jsonl")\n'
        'stt=$(git rev-parse "refs/heads/gh-pages:headroom/state.json")\n'
        'smr=$(git rev-parse "refs/heads/gh-pages:headroom/summary.md")\n'
        'newht=$(printf \'100644 blob %s\\tsamples.jsonl\\n100644 blob %s\\tevents.jsonl\\n'
        '100644 blob %s\\tstate.json\\n100644 blob %s\\tsummary.md\\n\' '
        '"$newblob" "$evt" "$stt" "$smr" | git mktree)\n'
        'bt=$(git rev-parse "refs/heads/gh-pages:bench")\n'
        'newrt=$(printf \'040000 tree %s\\tbench\\n040000 tree %s\\theadroom\\n\' "$bt" "$newht" | git mktree)\n'
        'commit=$(git -c user.name=concurrent-harvester -c user.email=concurrent@harvest.invalid commit-tree "$newrt" -p "$old" -m "concurrent harvest")\n'
        'git update-ref refs/heads/gh-pages "$commit"\n'
        'echo "non-fast-forward" >&2\n'
        'exit 1\n'
    )
    hook.chmod(0o755)


def test_publish_replaces_only_headroom_and_merges_a_concurrent_update(tmp_path):
    bare, _ = make_bare_remote(tmp_path)
    install_concurrent_update_hook(bare)
    bin_path, env = basic_scenario(tmp_path)
    process, state_dir = run_harvest(tmp_path, bin_path, env, publish=True, publish_remote=str(bare))
    assert process.returncode == 0, process.stdout + process.stderr
    assert "published to gh-pages:headroom/" in process.stdout

    published_samples = read_remote_file(bare, "refs/heads/gh-pages:headroom/samples.jsonl")
    keys = {json.loads(line)["key"] for line in published_samples.splitlines() if line}
    assert "101:1:macOS ARM64:ctest" in keys  # ours survived
    assert "999:1:macOS ARM64:ctest" in keys  # the concurrent side's record survived too
    assert read_remote_file(bare, "refs/heads/gh-pages:bench/chart.json") == "chart-v1"  # untouched
    # Snapshot self-consistency: the published event ledger must cover every
    # event the published samples derive — the merge cannot leave summary,
    # samples and events disagreeing about the same history.
    remote_events = {json.loads(line)["event_id"]
                     for line in read_remote_file(bare, "refs/heads/gh-pages:headroom/events.jsonl").splitlines() if line}
    derived = tracker.evaluate_series(
        [json.loads(line) for line in published_samples.splitlines() if line],
        tracker.config_identity(6.0, 2), 6.0, 2,
    )[2]
    assert {event["event_id"] for event in derived} <= remote_events


def test_publish_failure_names_itself_not_persisted_and_exits_three(tmp_path):
    bin_path, env = basic_scenario(tmp_path)
    missing_remote = tmp_path / "does-not-exist.git"
    process, state_dir = run_harvest(tmp_path, bin_path, env, publish=True, publish_remote=str(missing_remote))
    assert process.returncode == 3
    assert "NOT PERSISTED" in process.stdout
    assert "::warning::[ctest-headroom] publish failed" in process.stdout
    # The local state is still complete for the next harvest to publish.
    assert (state_dir / "samples.jsonl").exists()


def make_bench_only_remote(tmp_path: Path) -> Path:
    """A gh-pages ref that carries benchmark history but has no headroom/ yet —
    the first-production-publish precondition on a clean runner."""
    bare = tmp_path / "bench-only.git"
    subprocess.run(["git", "init", "-q", "--bare", str(bare)], check=True)
    subprocess.run(["git", "-C", str(bare), "config", "core.hooksPath", str(bare / "hooks")], check=True)
    work = tmp_path / "bench-seed"
    subprocess.run(["git", "init", "-q", str(work)], check=True)
    subprocess.run(["git", "-C", str(work), "config", "user.email", "seed@test"], check=True)
    subprocess.run(["git", "-C", str(work), "config", "user.name", "seed"], check=True)
    (work / "bench").mkdir()
    (work / "bench" / "chart.json").write_text("chart-v1")
    subprocess.run(["git", "-C", str(work), "add", "-A"], check=True)
    subprocess.run(["git", "-C", str(work), "commit", "-q", "-m", "seed"], check=True)
    subprocess.run(["git", "-C", str(work), "branch", "-M", "main"], check=True)
    subprocess.run(["git", "-C", str(work), "push", "-q", str(bare), "main:gh-pages"], check=True)
    return bare


def test_first_publish_on_a_ref_without_headroom_succeeds_end_to_end(tmp_path):
    bare = make_bench_only_remote(tmp_path)
    bin_path, env = basic_scenario(tmp_path)
    process, state_dir = run_harvest(tmp_path, bin_path, env, publish=True, publish_remote=str(bare))
    assert process.returncode == 0, process.stdout + process.stderr
    assert "published to gh-pages:headroom/" in process.stdout
    published_samples = read_remote_file(bare, "refs/heads/gh-pages:headroom/samples.jsonl")
    assert "101:1:macOS ARM64:ctest" in published_samples
    # The published summary describes THIS ledger, not a stale or absent file.
    published_summary = read_remote_file(bare, "refs/heads/gh-pages:headroom/summary.md")
    assert "macOS ARM64" in published_summary and "Ctest headroom" in published_summary
    assert read_remote_file(bare, "refs/heads/gh-pages:bench/chart.json") == "chart-v1"
    assert len([l for l in process.stdout.splitlines() if l.startswith("::notice::")]) == 1


def test_publish_merges_a_remote_that_advanced_after_bootstrap(tmp_path):
    bare, _ = make_bare_remote(tmp_path)
    bin_path, env = basic_scenario(tmp_path)
    # Harvest locally (no publish): the ledger now holds our records.
    _, state_dir = run_harvest(tmp_path, bin_path, env)
    repo_root = tmp_path / "repo-root"
    repo_root.mkdir(parents=True, exist_ok=True)
    subprocess.run(["git", "init", "-q", str(repo_root)], check=True)
    # ...meanwhile another harvester lands a record on the remote BEFORE our
    # publish runs — a legal fast-forward away from our bootstrap snapshot.
    work = tmp_path / "competitor"
    subprocess.run(["git", "clone", "-q", "--single-branch", "--branch", "gh-pages", str(bare), str(work)], check=True)
    subprocess.run(["git", "-C", str(work), "config", "user.email", "c@test"], check=True)
    subprocess.run(["git", "-C", str(work), "config", "user.name", "competitor"], check=True)
    foreign = json.dumps(
        {"type": "sample", "key": "999:1:macOS ARM64:ctest", "line": "main", "branch": "main",
         "job": MACOS_JOB, "phase": "ctest", "run_id": 999, "run_number": 998, "run_url": "",
         "attempt": 1, "head_sha": "x", "run_created_at": "2026-11-25T00:00:00Z", "config_id": "cfg-x"}
    ) + "\n"
    with (work / "headroom" / "samples.jsonl").open("a") as fh:
        fh.write(foreign)
    subprocess.run(["git", "-C", str(work), "add", "-A"], check=True)
    subprocess.run(["git", "-C", str(work), "commit", "-q", "-m", "competitor"], check=True)
    subprocess.run(["git", "-C", str(work), "push", "-q", "origin", "gh-pages"], check=True)

    ok, detail = tracker.publish_state(repo_root, str(bare), "gh-pages", state_dir)
    assert ok, detail
    published = read_remote_file(bare, "refs/heads/gh-pages:headroom/samples.jsonl")
    keys = {json.loads(line)["key"] for line in published.splitlines() if line}
    assert "999:1:macOS ARM64:ctest" in keys  # the concurrent side's record survived a legal fast-forward
    assert "101:1:macOS ARM64:ctest" in keys


def test_publish_rebuild_survives_a_legacy_row_on_a_live_fence_key(tmp_path):
    # The publish-side face of the same-key collision: the remote ledger was
    # written by pre-shrink code and still carries `run-pending` residue on
    # the key our harvest fences with `run-in-progress` (run 2 is still in
    # progress when this harvest runs). Publish merges the fetched headroom/
    # into the local state and rebuilds summary + events from the union —
    # the residue must never enter that adjudication, or the fence vanishes
    # and the two low samples pair into a pseudo event that gets published.
    bare, seed = make_bare_remote(tmp_path)
    tracker.write_jsonl(seed / "headroom" / "samples.jsonl", [
        make_sample(1, 560.0, 1.0, 560.0, created="2026-11-20T00:00:00Z"),  # low
        make_gap(2, created="2026-11-21T00:00:00Z", job="", phase="", reason="run-pending"),
        make_sample(3, 560.0, 1.0, 560.0, created="2026-11-22T00:00:00Z"),  # low
    ])
    subprocess.run(["git", "-C", str(seed), "add", "-A"], check=True)
    subprocess.run(["git", "-C", str(seed), "commit", "-q", "-m", "legacy residue"], check=True)
    subprocess.run(["git", "-C", str(seed), "push", "-q", str(bare), "main:gh-pages"], check=True)

    runs = [
        make_run(1, 3001, "main", "2026-11-20T00:00:00Z", "sha-1"),
        make_run(2, 3002, "main", "2026-11-21T00:00:00Z", "sha-2", conclusion=None),  # in progress → live fence
        make_run(3, 3003, "main", "2026-11-22T00:00:00Z", "sha-3"),
    ]
    jobs_and_logs = {
        1: {"jobs": {MACOS_JOB: (7001, "success")}, "logs": {7001: MAIN_GREEN_MACOS}},
        3: {"jobs": {MACOS_JOB: (7003, "success")}, "logs": {7003: MAIN_GREEN_MACOS}},
    }
    bin_path, env = setup_fake_gh(tmp_path, runs, jobs_and_logs, {"sha-1": MAIN_REGISTRY, "sha-3": MAIN_REGISTRY})
    process, state_dir = run_harvest(tmp_path, bin_path, env, publish=True, publish_remote=str(bare))
    assert process.returncode == 0, process.stdout + process.stderr
    assert "published to gh-pages:headroom/" in process.stdout

    published = read_remote_file(bare, "refs/heads/gh-pages:headroom/samples.jsonl")
    records = [json.loads(line) for line in published.splitlines() if line]
    assert not [record for record in records if record.get("reason") in ("run-pending", "run-not-found")]
    assert [record for record in records if record["run_id"] == 2 and record.get("reason") == "run-in-progress"]
    # the rebuilt snapshot describes a ledger whose fence broke the pairing
    assert read_remote_file(bare, "refs/heads/gh-pages:headroom/events.jsonl").strip() == ""
    assert "ACTIVE" not in read_remote_file(bare, "refs/heads/gh-pages:headroom/summary.md")


# --- the workflow wiring contract ------------------------------------------------------


def load_workflow():
    return yaml.load(WORKFLOW_PATH.read_text(encoding="utf-8"), Loader=yaml.BaseLoader)


def named_step(job, needle: str):
    for step in job["steps"]:
        name = step.get("name") or ""
        run = step.get("run") or ""
        if needle in name or needle in run:
            return step
    return None


def test_benchmark_summary_guard_and_topology_are_untouched():
    workflow = load_workflow()
    summary = workflow["jobs"]["benchmark-summary"]
    assert summary["if"] == "always() && github.event_name == 'push' && github.ref == 'refs/heads/main'"
    assert summary["needs"] == ["bench-ipo", "build"]  # build added: the current run's matrix must be done
    on = workflow["on"]  # yaml's BaseLoader keeps the top-level `on:` key a plain string
    assert on["push"]["branches"] == ["main", "release/**"]
    assert "pull_request" in on


def test_headroom_step_is_observation_only_and_non_blocking():
    workflow = load_workflow()
    summary = workflow["jobs"]["benchmark-summary"]
    step = named_step(summary, "ctest_headroom_tracker.py")
    assert step is not None
    assert str(step.get("continue-on-error", "")).lower() == "true"
    run = step["run"]
    assert "--emit-annotation" in run
    assert "--publish" in run
    assert "--current-run" in run
    assert "--job-summary" in run
    assert "github.repository" in run and "github.run_id" in run
    # Token permissions stay at the job level, already declared for gh-pages + Actions read.
    assert set(summary["permissions"]) == {"contents", "actions"}
    upload = next(
        (s for s in summary["steps"] if (s.get("name") or "") == "Upload headroom evidence"), None
    )
    assert upload is not None and upload.get("if") == "always()"
    # The upload is convenience evidence, never a gate: if it fails the job
    # stays green and the benchmark steps behind it still run.
    assert str(upload.get("continue-on-error", "")).lower() == "true"
    # The observation step runs before the benchmark artifact download.
    step_names = [s.get("name") or s.get("uses", "") for s in summary["steps"]]
    assert step_names.index(next(n for n in step_names if "headroom" in n.lower())) < step_names.index(
        next(n for n in step_names if "Download benchmark" in n)
    )


def test_annotation_emission_is_a_real_workflow_command_not_a_log_prefix():
    # The review-01 defect shape: a local machine printing "::notice::" reaches no
    # annotation consumer. The tracker must print inside the hosted-runner step —
    # pinned by the step's presence above — and its annotation carries the series
    # identity and points at persistent evidence.
    text = tracker.annotation_line(
        {
            "line": "main", "job": MACOS_JOB, "phase": "ctest",
            "headroom_latest_pct": 3.21, "threshold_pct": 6.0, "streak_at_fire": 3,
            "segment": "seg-x", "first_run": {"number": 7, "url": "u"},
        }
    )
    assert text.startswith("::notice::")
    assert "main/macOS ARM64/ctest" in text and "3.21%" in text and "seg-x" in text
