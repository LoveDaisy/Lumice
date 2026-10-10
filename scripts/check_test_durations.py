#!/usr/bin/env python3
"""Runtime gate for slow individual tests and cumulative CI phase budgets.

Per-test reports are `{id: seconds}`. Phase reports come from
`measure_ci_phase.py`. Both are checked against the single tracked authority,
`test/duration_registry.json`; there is no exemption switch.
"""
from __future__ import annotations

import argparse
import json
import math
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_REGISTRY = REPO_ROOT / "test" / "duration_registry.json"

THRESHOLD_S = 60.0
SLOWDOWN_FACTOR = 2.0
ROUND_TO_S = 5

REGISTRY_VERSION = 2
ENTRY_FIELDS = ("id", "job", "ci_seconds", "reason")
PHASE_FIELDS = ("job", "phase", "max_seconds", "reason")
PLACEHOLDER_REASON = "TODO"


class RegistryError(Exception):
    pass


def _positive_number(value, where: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise RegistryError(f"{where} must be a positive number")
    number = float(value)
    if not math.isfinite(number) or number <= 0:
        raise RegistryError(f"{where} must be a positive finite number")
    return number


def load_registry(path: Path) -> tuple[list[dict], list[dict]]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise RegistryError(f"{path}: cannot read: {error}") from error
    expected_top = {"version", "entries", "phase_budgets"}
    if not isinstance(data, dict) or set(data) != expected_top:
        raise RegistryError(f"{path}: top level must have exactly {', '.join(sorted(expected_top))}")
    if data["version"] != REGISTRY_VERSION:
        raise RegistryError(f"{path}: version {data['version']!r} is not {REGISTRY_VERSION}")

    entries = data["entries"]
    if not isinstance(entries, list):
        raise RegistryError(f"{path}: 'entries' must be a list")
    seen_entries = set()
    for index, entry in enumerate(entries):
        where = f"{path}: entries[{index}]"
        if not isinstance(entry, dict) or set(entry) != set(ENTRY_FIELDS):
            raise RegistryError(f"{where}: must have exactly the fields {', '.join(ENTRY_FIELDS)}")
        for field in ("id", "job", "reason"):
            if not isinstance(entry[field], str):
                raise RegistryError(f"{where}: '{field}' must be a string")
        _positive_number(entry["ci_seconds"], f"{where}: 'ci_seconds'")
        if not entry["id"] or not entry["job"]:
            raise RegistryError(f"{where}: 'id' and 'job' must be non-empty")
        key = (entry["id"], entry["job"])
        if key in seen_entries:
            raise RegistryError(f"{where}: duplicate entry for {entry['id']} in job {entry['job']!r}")
        seen_entries.add(key)

    budgets = data["phase_budgets"]
    if not isinstance(budgets, list):
        raise RegistryError(f"{path}: 'phase_budgets' must be a list")
    seen_budgets = set()
    for index, budget in enumerate(budgets):
        where = f"{path}: phase_budgets[{index}]"
        fields = set(budget) if isinstance(budget, dict) else set()
        if fields not in (set(PHASE_FIELDS), set(PHASE_FIELDS) | {"members"}):
            raise RegistryError(f"{where}: must have the phase fields and optional 'members'")
        for field in ("job", "phase", "reason"):
            if not isinstance(budget[field], str) or (field != "reason" and not budget[field]):
                raise RegistryError(f"{where}: '{field}' must be a non-empty string")
        _positive_number(budget["max_seconds"], f"{where}: 'max_seconds'")
        members = budget.get("members", [budget["phase"]])
        if not isinstance(members, list) or not members or any(not isinstance(v, str) or not v for v in members):
            raise RegistryError(f"{where}: 'members' must be a non-empty list of non-empty strings")
        if len(set(members)) != len(members):
            raise RegistryError(f"{where}: 'members' contains duplicates")
        key = (budget["job"], budget["phase"])
        if key in seen_budgets:
            raise RegistryError(f"{where}: duplicate phase budget {budget['phase']} in job {budget['job']!r}")
        seen_budgets.add(key)
    return entries, budgets


def load_reports(paths: list[Path]) -> dict[str, float]:
    merged: dict[str, float] = {}
    for path in paths:
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            raise ValueError(f"{path}: cannot read duration report: {error}") from error
        if not isinstance(data, dict):
            raise ValueError(f"{path}: a duration report must be a JSON object")
        for key, value in data.items():
            try:
                seconds = float(value)
            except (TypeError, ValueError) as error:
                raise ValueError(f"{path}: {key!r} has invalid duration {value!r}") from error
            if not isinstance(key, str) or not key or not math.isfinite(seconds) or seconds < 0:
                raise ValueError(f"{path}: duration ids must be non-empty and values finite and non-negative")
            merged[key] = max(merged.get(key, 0.0), seconds)
    return merged


def load_phase_reports(paths: list[Path]) -> dict[str, float]:
    phases: dict[str, float] = {}
    for path in paths:
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            raise ValueError(f"{path}: cannot read phase report: {error}") from error
        if not isinstance(data, dict) or set(data) != {"phase", "seconds", "exit_code"}:
            raise ValueError(f"{path}: phase report must contain exactly phase, seconds and exit_code")
        phase = data["phase"]
        seconds = data["seconds"]
        exit_code = data["exit_code"]
        if not isinstance(phase, str) or not phase:
            raise ValueError(f"{path}: phase must be a non-empty string")
        if isinstance(seconds, bool) or not isinstance(seconds, (int, float)) or not math.isfinite(seconds) or seconds < 0:
            raise ValueError(f"{path}: seconds must be finite and non-negative")
        if isinstance(exit_code, bool) or not isinstance(exit_code, int):
            raise ValueError(f"{path}: exit_code must be an integer")
        if phase in phases:
            raise ValueError(f"{path}: duplicate phase report for {phase}")
        phases[phase] = float(seconds)
    return phases


def round_up(seconds: float) -> int:
    return max(ROUND_TO_S, int(math.ceil(seconds / ROUND_TO_S)) * ROUND_TO_S)


def skeleton(test_id: str, job: str, seconds: float) -> str:
    return json.dumps({"id": test_id, "job": job, "ci_seconds": round_up(seconds), "reason": PLACEHOLDER_REASON})


def _function_of(test_id: str) -> str:
    return re.sub(r"\[.*\]$", "", test_id)


def check(job: str, durations: dict[str, float], entries: list[dict]) -> tuple[list[str], list[str]]:
    errors: list[str] = []
    notices: list[str] = []
    registered = {entry["id"]: entry for entry in entries if entry["job"] == job}
    for entry in entries:
        reason = entry["reason"].strip()
        if not reason or reason == PLACEHOLDER_REASON:
            errors.append(
                f"{entry['id']} (job {entry['job']!r}): the registry entry has no reason; say what defect "
                "the test guards against and why it cannot be faster or move to a cheaper layer"
            )
    for seconds, test_id in reversed(sorted((value, key) for key, value in durations.items()
                                             if key not in registered and value > THRESHOLD_S)):
        errors.append(
            f"{test_id} took {seconds:.1f}s in job {job!r}, over the {THRESHOLD_S:.0f}s threshold, and is not "
            "registered. Make it faster or move it to a cheaper layer, or add this entry to "
            f"test/duration_registry.json with a real reason:\n    {skeleton(test_id, job, seconds)}"
        )
    missing = [test_id for test_id in registered if test_id not in durations]
    if registered and len(missing) == len(registered):
        errors.append(
            f"none of the {len(registered)} ids registered for job {job!r} appear in its reports. If the tests "
            "still exist, the job name passed as --job probably does not match the registry"
        )
    else:
        for test_id in missing:
            siblings = sorted(value for value in durations if _function_of(value) == _function_of(test_id))
            hint = f" Ids of the same function in this run: {', '.join(siblings)}." if siblings else ""
            errors.append(f"{test_id} is registered for job {job!r} but did not run in it. Update or remove it.{hint}")
    for test_id, entry in sorted(registered.items()):
        if test_id not in durations:
            continue
        seconds = durations[test_id]
        limit = SLOWDOWN_FACTOR * entry["ci_seconds"]
        if seconds > limit:
            errors.append(
                f"{test_id} took {seconds:.1f}s in job {job!r}, more than {SLOWDOWN_FACTOR:g}x its registered "
                f"{entry['ci_seconds']}s. Find out why it slowed down or update the justified cost"
            )
        elif seconds < entry["ci_seconds"] / SLOWDOWN_FACTOR:
            notices.append(
                f"{test_id} is registered for job {job!r} at {entry['ci_seconds']}s but took {seconds:.1f}s; "
                "if it is now reliably faster, lower or remove its entry"
            )
    if not registered:
        notices.append(f"the registry has no entries for job {job!r}")
    return errors, notices


def check_phases(job: str, phases: dict[str, float], budgets: list[dict]) -> tuple[list[str], list[str]]:
    errors: list[str] = []
    notices: list[str] = []
    selected = [budget for budget in budgets if budget["job"] == job]
    known_members = {member for budget in selected for member in budget.get("members", [budget["phase"]])}
    for phase in sorted(set(phases) - known_members):
        errors.append(f"phase {phase!r} was reported for job {job!r} but has no registered phase budget")
    for budget in budgets:
        reason = budget["reason"].strip()
        if not reason or reason == PLACEHOLDER_REASON:
            errors.append(f"phase {budget['phase']} (job {budget['job']!r}) has no reason")
    for budget in selected:
        members = budget.get("members", [budget["phase"]])
        missing = [member for member in members if member not in phases]
        if missing:
            errors.append(
                f"phase budget {budget['phase']!r} for job {job!r} is missing reports for: {', '.join(missing)}"
            )
            continue
        seconds = sum(phases[member] for member in members)
        if seconds > budget["max_seconds"]:
            errors.append(
                f"phase budget {budget['phase']!r} for job {job!r} took {seconds:.1f}s "
                f"({'+'.join(members)}), over its {budget['max_seconds']}s limit"
            )
        else:
            notices.append(
                f"phase budget {budget['phase']!r} for job {job!r}: {seconds:.1f}s / {budget['max_seconds']}s"
            )
    if not selected:
        notices.append(f"the registry has no phase budgets for job {job!r}")
    return errors, notices


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--job", required=True)
    parser.add_argument("--report", type=Path, action="append", default=[])
    parser.add_argument("--phase-report", type=Path, action="append", default=[])
    parser.add_argument("--registry", type=Path, default=DEFAULT_REGISTRY)
    args = parser.parse_args(argv)
    if not args.report and not args.phase_report:
        parser.error("at least one --report or --phase-report is required")
    try:
        durations = load_reports(args.report)
        phases = load_phase_reports(args.phase_report)
        entries, budgets = load_registry(args.registry)
    except (RegistryError, ValueError) as error:
        print(f"::error::{error}")
        return 1
    errors, notices = check(args.job, durations, entries)
    phase_errors, phase_notices = check_phases(args.job, phases, budgets)
    errors.extend(phase_errors)
    notices.extend(phase_notices)
    for message in notices:
        print(f"::notice::{message}")
    for message in errors:
        first, _, rest = message.partition("\n")
        print(f"::error::{first}")
        if rest:
            print(rest)
    if errors:
        print(f"check_test_durations: {len(errors)} problem(s) for job {args.job!r}")
        return 1
    print(f"check_test_durations: OK for job {args.job!r} ({len(durations)} ids, {len(phases)} phases checked)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
