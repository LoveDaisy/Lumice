#!/usr/bin/env python3
"""Runtime gate: every PR-layer test slower than THRESHOLD_S must be registered.

Reads the duration reports one CI job's pytest calls wrote
(`scripts/duration_report_plugin.py`, `{id: seconds}`) and compares them with the
tracked registry `test/duration_registry.json`, whose entries are the slow tests
someone has argued for. Red when, for the job named by `--job`:

1. an id NOT in the registry for this job took more than THRESHOLD_S;
2. a registered id took more than SLOWDOWN_FACTOR x its registered seconds;
3. a registered id is absent from this job's reports (renamed, moved or
   deleted: the entry would otherwise outlive what it argues for). A skipped
   test is still present, with ~0 s, so a platform skip never reads as stale;
4. the registry itself is malformed, or an entry's reason is empty or "TODO".

A registered id that ran under THRESHOLD_S is reported as a notice, not red, so a
test hovering at the threshold does not force its entry in and out on alternate
runs. Deleting the entry is the author's call.

This is a RUNTIME check, unlike the four diff-scoped checkers named in AGENTS.md:
its input exists only after a CI job has run the tests, so it cannot run in the
pre-commit hook and is not part of `check_policies.py`.

There is no exemption switch, by design: no flag, no environment variable, no
inline marker. The only way to let a slow test through is a registry entry,
which is a reviewable diff. THRESHOLD_S and SLOWDOWN_FACTOR are constants for the
same reason; changing them is changing this file.

The thresholds, the reasoning behind them and the registration procedure are in
doc/testing-architecture.md section 7.
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

THRESHOLD_S = 30.0
SLOWDOWN_FACTOR = 2.0
# Registered seconds are rounded up to this step, so re-measuring does not churn
# the file over a second or two.
ROUND_TO_S = 5
# Emitted with --emit-candidates only: below THRESHOLD_S but close enough that
# the threshold's margin is worth watching.
BORDERLINE_FLOOR_S = 20.0

REGISTRY_VERSION = 1
ENTRY_FIELDS = ("id", "job", "ci_seconds", "reason")
PLACEHOLDER_REASON = "TODO"


class RegistryError(Exception):
    pass


def load_registry(path: Path) -> list[dict]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as e:
        raise RegistryError(f"{path}: cannot read: {e}") from e
    if not isinstance(data, dict) or set(data) != {"version", "entries"}:
        raise RegistryError(f"{path}: top level must be an object with exactly 'version' and 'entries'")
    if data["version"] != REGISTRY_VERSION:
        raise RegistryError(f"{path}: version {data['version']!r} is not {REGISTRY_VERSION}")
    entries = data["entries"]
    if not isinstance(entries, list):
        raise RegistryError(f"{path}: 'entries' must be a list")
    seen = set()
    for i, entry in enumerate(entries):
        where = f"{path}: entries[{i}]"
        if not isinstance(entry, dict) or set(entry) != set(ENTRY_FIELDS):
            raise RegistryError(f"{where}: must have exactly the fields {', '.join(ENTRY_FIELDS)}")
        for field in ("id", "job", "reason"):
            if not isinstance(entry[field], str):
                raise RegistryError(f"{where}: '{field}' must be a string")
        seconds = entry["ci_seconds"]
        if isinstance(seconds, bool) or not isinstance(seconds, (int, float)) or seconds <= 0:
            raise RegistryError(f"{where}: 'ci_seconds' must be a positive number")
        if not entry["id"] or not entry["job"]:
            raise RegistryError(f"{where}: 'id' and 'job' must be non-empty")
        key = (entry["id"], entry["job"])
        if key in seen:
            raise RegistryError(f"{where}: duplicate entry for {entry['id']} in job {entry['job']!r}")
        seen.add(key)
    return entries


def load_reports(paths: list[Path]) -> dict[str, float]:
    merged: dict[str, float] = {}
    for path in paths:
        data = json.loads(path.read_text(encoding="utf-8"))
        if not isinstance(data, dict):
            raise ValueError(f"{path}: a duration report must be a JSON object")
        for key, seconds in data.items():
            # The same shared fixture can appear in two reports of one job; the
            # same test cannot, so max is right for both.
            merged[key] = max(merged.get(key, 0.0), float(seconds))
    return merged


def round_up(seconds: float) -> int:
    return max(ROUND_TO_S, int(math.ceil(seconds / ROUND_TO_S)) * ROUND_TO_S)


def skeleton(test_id: str, job: str, seconds: float) -> str:
    return json.dumps(
        {"id": test_id, "job": job, "ci_seconds": round_up(seconds), "reason": PLACEHOLDER_REASON}
    )


def _function_of(test_id: str) -> str:
    return re.sub(r"\[.*\]$", "", test_id)


def check(job: str, durations: dict[str, float], entries: list[dict]) -> tuple[list[str], list[str]]:
    """Return (errors, notices) for one job."""
    errors: list[str] = []
    notices: list[str] = []
    registered = {e["id"]: e for e in entries if e["job"] == job}

    for entry in entries:
        reason = entry["reason"].strip()
        if not reason or reason == PLACEHOLDER_REASON:
            errors.append(
                f"{entry['id']} (job {entry['job']!r}): the registry entry has no reason; say what defect "
                "the test guards against and why it cannot be faster or move to a cheaper layer"
            )

    unregistered = sorted(
        (t, i) for i, t in durations.items() if i not in registered and t > THRESHOLD_S
    )
    for seconds, test_id in reversed(unregistered):
        errors.append(
            f"{test_id} took {seconds:.1f}s in job {job!r}, over the {THRESHOLD_S:.0f}s threshold, and is not "
            f"registered. Make it faster or move it to a cheaper layer, or add this entry to "
            f"test/duration_registry.json with a real reason:\n    {skeleton(test_id, job, seconds)}"
        )

    missing = [test_id for test_id in registered if test_id not in durations]
    if registered and len(missing) == len(registered):
        errors.append(
            f"none of the {len(registered)} ids registered for job {job!r} appear in its reports. If the tests "
            "still exist, the job name passed as --job in .github/workflows/ci.yml probably does not match "
            "the registry's 'job' field (or the job lost its --duration-report wiring)"
        )
    else:
        for test_id in missing:
            siblings = sorted(i for i in durations if _function_of(i) == _function_of(test_id))
            hint = f" Ids of the same function in this run: {', '.join(siblings)}." if siblings else ""
            errors.append(
                f"{test_id} is registered for job {job!r} but did not run in it (renamed, moved or deleted?). "
                f"Update or remove its registry entry.{hint}"
            )

    for test_id, entry in sorted(registered.items()):
        if test_id not in durations:
            continue
        seconds = durations[test_id]
        limit = SLOWDOWN_FACTOR * entry["ci_seconds"]
        if seconds > limit:
            errors.append(
                f"{test_id} took {seconds:.1f}s in job {job!r}, more than {SLOWDOWN_FACTOR:g}x its registered "
                f"{entry['ci_seconds']}s. Find out why it slowed down; if the new cost is justified, raise "
                "'ci_seconds' in test/duration_registry.json in the same change"
            )
        elif seconds <= THRESHOLD_S:
            notices.append(
                f"{test_id} is registered for job {job!r} but took {seconds:.1f}s, under the "
                f"{THRESHOLD_S:.0f}s threshold; its registry entry may no longer be needed"
            )

    if not registered:
        notices.append(f"the registry has no entries for job {job!r}")
    return errors, notices


def emit_candidates(job: str, durations: dict[str, float]) -> None:
    over = sorted(((t, i) for i, t in durations.items() if t > THRESHOLD_S), reverse=True)
    near = sorted(
        ((t, i) for i, t in durations.items() if BORDERLINE_FLOOR_S < t <= THRESHOLD_S), reverse=True
    )
    print(f"### Duration candidates for job `{job}` (> {THRESHOLD_S:.0f}s: {len(over)})")
    print("```")
    for seconds, test_id in over:
        print(f"{seconds:8.1f}  {skeleton(test_id, job, seconds)}")
    print("```")
    print(f"### Borderline ({BORDERLINE_FLOOR_S:.0f}-{THRESHOLD_S:.0f}s: {len(near)})")
    print("```")
    for seconds, test_id in near:
        print(f"{seconds:8.1f}  {test_id}")
    print("```")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--job", required=True, help="this CI job's name, as written in the registry")
    parser.add_argument("--report", type=Path, action="append", required=True,
                        help="a duration report written by duration_report_plugin (repeatable)")
    parser.add_argument("--registry", type=Path, default=DEFAULT_REGISTRY)
    parser.add_argument("--emit-candidates", action="store_true",
                        help="print the ids over (and near) the threshold as registry skeletons; never red")
    args = parser.parse_args(argv)

    durations = load_reports(args.report)
    if args.emit_candidates:
        emit_candidates(args.job, durations)
        return 0
    try:
        entries = load_registry(args.registry)
    except RegistryError as e:
        print(f"::error::{e}")
        return 1

    errors, notices = check(args.job, durations, entries)
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
    print(f"check_test_durations: OK for job {args.job!r} ({len(durations)} ids checked)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
