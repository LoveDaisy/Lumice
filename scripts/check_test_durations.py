#!/usr/bin/env python3
"""Runtime gate for slow individual tests and cumulative CI phase budgets.

Per-test reports are `{id: seconds}`. Phase reports come from
`measure_ci_phase.py`; cache reports (per-miss counts) come from
`collect_cache_stats.py` and are consumed only by phase budgets carrying a
`per_miss` block, which judge the build phase on
`seconds <= overhead_seconds + max_seconds_per_miss * misses` with the
absolute `max_seconds` kept as an outer bound. All are checked against the
single tracked authority, `test/duration_registry.json`. A maintainer-
authorized cold-build run may accept only over-limit phase budgets whose
registered members include `build`.
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

REGISTRY_VERSION = 3
ENTRY_FIELDS = ("id", "job", "ci_seconds", "reason")
PHASE_FIELDS = ("job", "phase", "max_seconds", "reason")
PER_MISS_FIELDS = ("overhead_seconds", "max_seconds_per_miss")
CACHE_TOOLS = ("ccache", "sccache")
PLACEHOLDER_REASON = "TODO"
# Hint in the outer-bound red message only: the reference leg's full-tree
# compile-request count. A hint, not a judgment — per-leg signatures live in
# each build entry's registry reason; update the number when the reference
# leg's build definition moves by an amount that would mislead a triager.
FULL_TREE_SIGNATURE_TUS = 524


class RegistryError(Exception):
    pass


def _positive_number(value, where: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise RegistryError(f"{where} must be a positive number")
    number = float(value)
    if not math.isfinite(number) or number <= 0:
        raise RegistryError(f"{where} must be a positive finite number")
    return number


def _non_negative_number(value, where: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise RegistryError(f"{where} must be a non-negative number")
    number = float(value)
    if not math.isfinite(number) or number < 0:
        raise RegistryError(f"{where} must be a non-negative finite number")
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
        allowed = (set(PHASE_FIELDS), set(PHASE_FIELDS) | {"members"}, set(PHASE_FIELDS) | {"per_miss"})
        if fields not in allowed:
            raise RegistryError(f"{where}: must have the phase fields and optional 'members' or 'per_miss'")
        for field in ("job", "phase", "reason"):
            if not isinstance(budget[field], str) or (field != "reason" and not budget[field]):
                raise RegistryError(f"{where}: '{field}' must be a non-empty string")
        _positive_number(budget["max_seconds"], f"{where}: 'max_seconds'")
        per_miss = budget.get("per_miss")
        if per_miss is not None:
            # The allowed field sets above already exclude members+per_miss, so
            # a budget here carries per_miss on a single phase, never an
            # aggregate.
            if not isinstance(per_miss, dict) or set(per_miss) != set(PER_MISS_FIELDS):
                raise RegistryError(f"{where}: 'per_miss' must have exactly the fields {', '.join(PER_MISS_FIELDS)}")
            _non_negative_number(per_miss["overhead_seconds"], f"{where}: 'per_miss.overhead_seconds'")
            _positive_number(per_miss["max_seconds_per_miss"], f"{where}: 'per_miss.max_seconds_per_miss'")
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
        if exit_code != 0:
            raise ValueError(f"{path}: phase {phase!r} exited with code {exit_code}")
        if phase in phases:
            raise ValueError(f"{path}: duplicate phase report for {phase}")
        phases[phase] = float(seconds)
    return phases


def load_cache_reports(paths: list[Path]) -> dict[str, dict]:
    reports: dict[str, dict] = {}
    for path in paths:
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            raise ValueError(f"{path}: cannot read cache report: {error}") from error
        if not isinstance(data, dict) or set(data) != {"phase", "tool", "misses"}:
            raise ValueError(f"{path}: cache report must contain exactly phase, tool and misses")
        phase, tool, misses = data["phase"], data["tool"], data["misses"]
        if not isinstance(phase, str) or not phase:
            raise ValueError(f"{path}: phase must be a non-empty string")
        if tool not in CACHE_TOOLS:
            raise ValueError(f"{path}: tool must be one of {', '.join(CACHE_TOOLS)}")
        if isinstance(misses, bool) or not isinstance(misses, int) or misses < 0:
            raise ValueError(f"{path}: misses must be a non-negative integer")
        if phase in reports:
            raise ValueError(f"{path}: duplicate cache report for {phase}")
        reports[phase] = {"tool": tool, "misses": misses}
    return reports


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


def check_phases(
    job: str,
    phases: dict[str, float],
    budgets: list[dict],
    allow_cold_build: bool = False,
    cache_reports: dict[str, dict] | None = None,
) -> tuple[list[str], list[str], list[str]]:
    errors: list[str] = []
    notices: list[str] = []
    warnings: list[str] = []
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
        # A per_miss budget carries two independent conditions: the adaptive
        # per-miss limit (the authoritative ruler — what one unit of change
        # costs to compile) and the absolute max_seconds (the outer bound /
        # disaster backstop). The report is consumed only where a per_miss
        # entry needs it: no per_miss budget for the phase means the report is
        # never read, so collecting one is harmless and missing one is silent.
        per_miss = budget.get("per_miss")
        report = (cache_reports or {}).get(budget["phase"]) if per_miss is not None else None
        adaptive_violation = ""
        if per_miss is not None:
            if report is None:
                errors.append(
                    f"phase budget {budget['phase']!r} for job {job!r} has a per-miss limit but no cache report "
                    "was passed (--cache-report); the per-miss check fails closed — pass the collector's report "
                    "or drop the per_miss block"
                )
            else:
                misses = report["misses"]
                limit = per_miss["overhead_seconds"] + per_miss["max_seconds_per_miss"] * misses
                if seconds > limit:
                    if misses:
                        adaptive_violation = (
                            f"phase budget {budget['phase']!r} for job {job!r} took {seconds:.1f}s with {misses} "
                            f"misses ({seconds / misses:.2f} s/miss > {per_miss['max_seconds_per_miss']:g}; "
                            "compile-cost regression)"
                        )
                    else:
                        adaptive_violation = (
                            f"phase budget {budget['phase']!r} for job {job!r} took {seconds:.1f}s with 0 misses "
                            f"(adaptive limit: {per_miss['overhead_seconds']:g}s overhead + 0 misses = {limit:g}s)"
                        )
        messages: list[str] = []
        if adaptive_violation:
            if seconds > budget["max_seconds"]:
                adaptive_violation += f" and over its {budget['max_seconds']}s outer bound"
            messages.append(adaptive_violation)
        elif seconds > budget["max_seconds"]:
            if report is not None:
                misses = report["misses"]
                detail = f" with {misses} misses"
                if misses:
                    detail += (
                        f" ({seconds / misses:.2f} s/miss healthy; broad closure — full-tree signature "
                        f"is ~{FULL_TREE_SIGNATURE_TUS} compile requests on the x86_64 reference leg)"
                    )
                messages.append(
                    f"phase budget {budget['phase']!r} for job {job!r} took {seconds:.1f}s{detail} "
                    f"over its {budget['max_seconds']}s outer bound"
                )
            else:
                messages.append(
                    f"phase budget {budget['phase']!r} for job {job!r} took {seconds:.1f}s "
                    f"({'+'.join(members)}), over its {budget['max_seconds']}s limit"
                )
        if messages:
            for message in messages:
                if allow_cold_build and "build" in members:
                    warnings.append(f"{message}; accepted with cold-build exception")
                else:
                    errors.append(message)
        else:
            notice = (
                f"phase budget {budget['phase']!r} for job {job!r}: {seconds:.1f}s / {budget['max_seconds']}s"
            )
            if report is not None:
                misses = report["misses"]
                notice += f", {misses} misses" + (f" ({seconds / misses:.2f} s/miss)" if misses else "")
            notices.append(notice)
    if not selected:
        notices.append(f"the registry has no phase budgets for job {job!r}")
    if allow_cold_build:
        notices.append(f"cold-build exception is enabled for job {job!r}")
    return errors, notices, warnings


def write_summary(
    path: Path, job: str, errors: list[str], warnings: list[str], notices: list[str], allow_cold_build: bool
) -> None:
    lines = [
        "## CI duration check",
        "",
        f"- Job: `{job}`",
        f"- Cold-build exception: `{'enabled' if allow_cold_build else 'disabled'}`",
        f"- Result: `{'FAIL' if errors else 'PASS'}`",
    ]
    for title, messages in (("Errors", errors), ("Cold-build exceptions", warnings), ("Notices", notices)):
        if not messages:
            continue
        lines.extend(["", f"### {title}", ""])
        lines.extend(f"- {message}" for message in messages)
    with path.open("a", encoding="utf-8") as output:
        output.write("\n".join(lines) + "\n")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--job", required=True)
    parser.add_argument("--report", type=Path, action="append", default=[])
    parser.add_argument("--phase-report", type=Path, action="append", default=[])
    parser.add_argument("--cache-report", type=Path, action="append", default=[])
    parser.add_argument("--registry", type=Path, default=DEFAULT_REGISTRY)
    parser.add_argument("--allow-cold-build", action="store_true")
    parser.add_argument("--summary", type=Path)
    args = parser.parse_args(argv)
    if not args.report and not args.phase_report:
        parser.error("at least one --report or --phase-report is required")
    try:
        durations = load_reports(args.report)
        phases = load_phase_reports(args.phase_report)
        cache_reports = load_cache_reports(args.cache_report)
        entries, budgets = load_registry(args.registry)
    except (RegistryError, ValueError) as error:
        message = str(error)
        print(f"::error::{message}")
        if args.summary:
            try:
                write_summary(args.summary, args.job, [message], [], [], args.allow_cold_build)
            except OSError as summary_error:
                print(f"::error::{args.summary}: cannot write summary: {summary_error}")
        return 1
    errors, notices = check(args.job, durations, entries)
    phase_errors, phase_notices, warnings = check_phases(
        args.job,
        phases,
        budgets,
        allow_cold_build=args.allow_cold_build,
        cache_reports=cache_reports,
    )
    errors.extend(phase_errors)
    notices.extend(phase_notices)
    for message in notices:
        print(f"::notice::{message}")
    for message in warnings:
        print(f"::warning::{message}")
    for message in errors:
        first, _, rest = message.partition("\n")
        print(f"::error::{first}")
        if rest:
            print(rest)
    if args.summary:
        try:
            write_summary(args.summary, args.job, errors, warnings, notices, args.allow_cold_build)
        except OSError as error:
            print(f"::error::{args.summary}: cannot write summary: {error}")
            return 1
    if errors:
        print(f"check_test_durations: {len(errors)} problem(s) for job {args.job!r}")
        return 1
    suffix = " accepted with cold-build exception" if warnings else ""
    print(
        f"check_test_durations: OK for job {args.job!r} "
        f"({len(durations)} ids, {len(phases)} phases checked){suffix}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
