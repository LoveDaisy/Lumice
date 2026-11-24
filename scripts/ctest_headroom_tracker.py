#!/usr/bin/env python3
"""Ctest headroom tracker: observe machine-factor margin erosion, never judge.

Harvests the duration checker's machine-factor readings from the CI run logs
of push runs on `main` and `release/*` (two independent lines), turns each
into an implied C (`phase seconds / raw factor`) and a headroom percentage
against the budget of the run's OWN registry snapshot, persists
samples/gaps/events, and raises a visible non-blocking signal when a series
stays below the headroom threshold for a minimum streak of consecutive
samples.

What this is not: a gate. Nothing here opens issues or tasks, changes any
registry constant, or alters any existing CI or local performance verdict.
The duration gate (`scripts/check_test_durations.py`) remains the only
judge; this tracker consumes its printed lines and never depends on the
checker's newer appended headroom notice — the two lines share only the old
v4 formats below, and that shared old format is the load-bearing contract.

Consumed formats (job logs store the runner-rendered spelling `##[notice]` /
`##[warning]` / `##[error]`, optionally prefixed by a runner timestamp;
`::notice::`-style spelling is tolerated too):

  green:  phase budget 'P' for job 'J': <T>s / <E>s (machine factor <F> from <N> rulers)
  red:    phase budget 'P' for job 'J' took <T>s (<members>), over its machine-normalized limit <B>s x factor <F> = <E>s
  clamp:  phase budget 'P' for job 'J': calibration band exceeded (raw factor <RAW>, clamped to <F>); ...

Reading semantics (evidence 2026-11-24, release run 45146282519):
C = T / f_raw — the clamp keeps the gate's budget sane and must not inflate
the machine-relative reading (a 0.726 raw clamped to 0.800 would fake ~9%
less implied C); headroom = 100 x (1 - C/B), never B/C - 1 (different
denominators, ~9.5% vs 8.66% on the same pair). T and the factors are
log-displayed (0.1 s / 0.001 precision), so every aggregate reading is a
documented log-rounding approximation: the original lines are kept next to
each number, and a displayed value whose rounding interval straddles the
threshold is recorded "threshold-uncertain" — never forced low or healthy.
The clamp warning overrides the gate factor with its raw value; it is
printed AFTER the green notice in real logs, so the whole log is parsed
before any association.

Sequence discipline: one sample per (run, job, phase) from attempt 1 — a
rerun (attempt 2+) never replaces or extends the first result, and a run
whose attempt 1 yields no computable number is a gap, not "fixed green" by
its rerun. The population is push runs on `main` and existing `release/*`
branches only; PR runs are candidate trees and never enter a push-run
series. A cancelled, unreadable, or number-less run is an honest gap that
breaks continuity — a gap neither fakes health nor accumulates a streak,
and two low samples separated by a gap must not pair into a trigger. A
budget/ruler/clamp change in the snapshot forms a new calibration segment;
a streak cannot span one. A threshold configuration change breaks streak
continuity as well (re-fired only as a new event, never silently carried).
No new runs means no samples and an untouched streak.

Persistence: the state directory holds `samples.jsonl` (append-only
sequence ledger of samples and gaps, union-merged by record key),
`events.jsonl` (append-only fired signals, union-merged by event id),
`state.json` (cursor + observation config), and `summary.md` (rebuilt every
harvest). Streaks and events are recomputed from the full ledger on every
harvest, which makes re-harvesting idempotent: the same history always
derives the same event ids. With `--publish` the state directory is
bootstrapped from and published back to the `headroom/` directory of the
publish ref (gh-pages) — that directory is replaced wholesale and no other
path is touched; a non-fast-forward push is re-merged (bounded retries),
never force-pushed, and a failed publish leaves the summary explicitly
naming itself NOT PERSISTED while the local artifact keeps the evidence for
the next harvest to re-collect.

Exit codes: 0 harvest completed (gaps are data, not errors); 2 usage or
catastrophic local failure; 3 harvest completed but publication failed.
Infra errors (auth/network/disk) are also printed as `::warning::` so they
are visible on the CI run page; with `--emit-annotation`, each active
signal prints one `::notice::` line (a real Actions annotation when run
inside a workflow step).
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import re
import subprocess
import sys
import time
import urllib.parse
from datetime import datetime, timedelta, timezone
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
REGISTRY_PATH_IN_REPO = "test/duration_registry.json"

DEFAULT_THRESHOLD_PCT = 6.0  # provisional ~2 sigma; revised via the observation config, never silently
DEFAULT_MIN_STREAK = 2
DEFAULT_WINDOW_DAYS = 30
DEFAULT_MAX_RUNS = 100
DEFAULT_MAX_API_CALLS = 300
DEFAULT_TIME_BUDGET_SEC = 300.0
PUBLISH_RETRIES = 3
PUBLISH_DIR = "headroom"
STATE_FILES = ("samples.jsonl", "events.jsonl", "state.json", "summary.md")
STATE_SCHEMA = 1
MAX_LIST_PAGES = 50

# Run conclusions that carry no trustworthy attempt-1 evidence. `success` and
# `failure` are processable (a red ctest job is exactly where the red error
# line lives); None (in progress — the current run) is processable because
# `needs: build` has completed the build matrix by the time this runs.
UNUSABLE_RUN_CONCLUSIONS = {"cancelled", "timed_out", "skipped", "stale", "startup_failure", "action_required", "neutral"}

# --- log line parsing ----------------------------------------------------------

_TS_PREFIX = re.compile(r"^\d{4}-\d{2}-\d{2}T[\d:.]+Z\s+")
_LEVEL_PREFIX = re.compile(r"^(?:##\[(notice|warning|error)\]|::(notice|warning|error)::)\s*")

_GREEN = re.compile(
    r"phase budget '(?P<phase>[^']*)' for job '(?P<job>[^']*)': "
    r"(?P<seconds>\d+(?:\.\d+)?)s / (?P<effective>\d+(?:\.\d+)?)s "
    r"\(machine factor (?P<factor>\d+(?:\.\d+)?) from (?P<rulers>\d+) rulers\)"
)
_RED = re.compile(
    r"phase budget '(?P<phase>[^']*)' for job '(?P<job>[^']*)' took (?P<seconds>\d+(?:\.\d+)?)s "
    r"\((?P<members>[^)]*)\), over its machine-normalized limit "
    r"(?P<budget>\d+(?:\.\d+)?)s x factor (?P<factor>\d+(?:\.\d+)?) = (?P<effective>\d+(?:\.\d+)?)s"
)
_CLAMP = re.compile(
    r"phase budget '(?P<phase>[^']*)' for job '(?P<job>[^']*)': calibration band exceeded "
    r"\(raw factor (?P<raw>\d+(?:\.\d+)?), clamped to (?P<factor>\d+(?:\.\d+)?)\)"
)

LEVEL_OF_KIND = {"green": "##[notice]", "red": "##[error]", "clamp": "##[warning]"}


class HarvestError(Exception):
    """A named failure class; the reason lands in a gap record, never a guess."""


class ReadingError(Exception):
    """A log that cannot yield one trustworthy reading for a series."""


def parse_checker_lines(text: str) -> list[dict]:
    """Parse a job log into structured checker output lines.

    The whole log is read before anything associates: the clamp warning is
    printed after the green notice in real logs, so stopping at the first
    match would silently drop the raw factor.
    """
    parsed: list[dict] = []
    for raw_line in text.splitlines():
        line = _TS_PREFIX.sub("", raw_line)
        marker = _LEVEL_PREFIX.match(line)
        if not marker:
            continue
        body = line[marker.end():]
        for kind, pattern in (("green", _GREEN), ("red", _RED), ("clamp", _CLAMP)):
            hit = pattern.search(body)
            if hit:
                parsed.append({"kind": kind, "_body": body, **hit.groupdict()})
                break
    return parsed


def extract_reading(parsed_lines: list[dict], job: str, phase: str) -> dict:
    """Associate parsed lines into exactly one reading for (job, phase).

    Returns the displayed operands plus their source lines; raises
    ReadingError with a reason class when the log cannot give one. The
    clamp warning overrides the gate factor with its raw value; multiple
    or contradictory readings refuse rather than pick one silently.
    """
    greens, reds, clamps = [], [], []
    for entry in parsed_lines:
        if entry["job"] != job or entry["phase"] != phase:
            continue
        {"green": greens, "red": reds, "clamp": clamps}[entry["kind"]].append(entry)
    if len(greens) + len(reds) > 1:
        raise ReadingError("ambiguous-multiple-readings")
    if len(clamps) > 1:
        raise ReadingError("ambiguous-multiple-clamp-warnings")
    source = greens[0] if greens else (reds[0] if reds else None)
    if source is None:
        raise ReadingError("no-machine-factor-reading")
    if clamps:
        raw_factor = float(clamps[0]["raw"])
        if raw_factor <= 0:
            raise ReadingError("unreadable-clamp-raw")
    else:
        # No warning means the gate factor sat inside the band: raw == gate,
        # at the log's displayed precision (0.001) — a documented
        # approximation carried into the rounding interval, never silently
        # treated as exact.
        raw_factor = float(source["factor"])
    evidence = sorted({entry_source(entry) for entry in greens + reds + clamps})
    return {
        "seconds": float(source["seconds"]),
        "gate_factor": float(source["factor"]),
        "raw_factor": raw_factor,
        "rulers": int(source.get("rulers") or 0),
        "log_lines": evidence,
    }


def entry_source(entry: dict) -> str:
    """Rebuild the original log line from a parsed entry (the evidence text)."""
    return f"{LEVEL_OF_KIND[entry['kind']]}{entry['_body']}"


def implied_and_headroom(seconds: float, raw_factor: float, budget_seconds: float) -> tuple[float, float]:
    implied_c = seconds / raw_factor
    headroom_pct = 100.0 * (1.0 - implied_c / budget_seconds)
    return implied_c, headroom_pct


def rounding_interval_pct(seconds: float, raw_factor: float, budget_seconds: float) -> tuple[float, float]:
    """Display-precision uncertainty bracket on the headroom percentage.

    T displays at 0.1 s and the factors at 0.001; B comes from the registry
    (exact). The bracket is what decides threshold-uncertain cases near the
    boundary — a sample the log's precision cannot classify honestly is
    recorded uncertain, not forced either way.
    """
    seconds_lo, seconds_hi = seconds - 0.05, seconds + 0.05
    factor_lo = max(raw_factor - 0.0005, 1e-9)
    factor_hi = raw_factor + 0.0005
    c_max = seconds_hi / factor_lo
    c_min = max(seconds_lo, 0.0) / factor_hi
    return (
        100.0 * (1.0 - c_max / budget_seconds),
        100.0 * (1.0 - c_min / budget_seconds),
    )


def classify_headroom(interval: tuple[float, float], threshold_pct: float) -> str:
    lo, hi = interval
    if lo >= threshold_pct:
        return "healthy"  # even the worst-case reading clears the threshold
    if hi < threshold_pct:
        return "low"  # strictly below: equal is not low
    return "uncertain"


def classify_harvest_error(error: HarvestError, default: str) -> str:
    """Name the failure class for a gap record, keeping gh's own HTTP verdict.

    Expired logs (404/410), permission gaps (403) and other transport
    failures are distinct facts — the summary must not blur them into one
    'unknown', and none of them may read as a healthy sample.
    """
    message = error.args[0] if error.args else str(error)
    tail = message.rsplit(": ", 1)[-1]
    if "(HTTP 404)" in message or "(HTTP 410)" in message:
        return f"{default}: not found or expired ({tail})"
    if "(HTTP 403)" in message:
        return f"{default}: forbidden — check token permissions ({tail})"
    if message.startswith("gh-api-failed"):
        return f"{default}: {tail}"
    if message.startswith("api-budget-exhausted"):
        return "api-budget-exhausted"
    return message or default


# --- registry snapshot ----------------------------------------------------------

def parse_registry_snapshot(text: str) -> dict:
    """Extract the machine_factor budgets of one registry snapshot.

    Returns {(job, phase): {...}}; the segment id is a content hash of the
    budget-defining fields, so any budget/ruler/clamp change opens a new
    calibration segment and no streak can silently span one.
    """
    data = json.loads(text)
    if not isinstance(data, dict) or data.get("version") != 4:
        raise HarvestError("unsupported-registry-version")
    budgets: dict[tuple[str, str], dict] = {}
    for budget in data.get("phase_budgets", []):
        machine = budget.get("machine_factor")
        if not machine:
            continue
        canonical = json.dumps(
            {
                "job": budget["job"],
                "phase": budget["phase"],
                "max_seconds": budget["max_seconds"],
                "ruler_seconds": machine["ruler_seconds"],
                "clamp_band": machine["clamp_band"],
            },
            sort_keys=True,
            separators=(",", ":"),
        )
        budgets[(budget["job"], budget["phase"])] = {
            "budget_seconds": float(budget["max_seconds"]),
            "rulers": len(machine["ruler_seconds"]),
            "segment": "seg-" + hashlib.sha256(canonical.encode()).hexdigest()[:12],
        }
    return budgets


def config_identity(threshold_pct: float, min_streak: int) -> str:
    canonical = json.dumps(
        {"min_streak": min_streak, "threshold_pct": threshold_pct},
        sort_keys=True,
        separators=(",", ":"),
    )
    return "cfg-" + hashlib.sha256(canonical.encode()).hexdigest()[:12]


# --- GitHub transport ------------------------------------------------------------

class ApiBudget:
    """Hard caps on API calls and wall time; exhaustion ends the harvest at a
    run boundary (the cursor stops there, the remainder is next harvest's
    backlog) — never mid-page silent skips."""

    def __init__(self, max_calls: int, deadline_mono: float):
        self.calls = 0
        self.max_calls = max_calls
        self.deadline = deadline_mono

    def spend(self) -> bool:
        if self.calls >= self.max_calls or time.monotonic() >= self.deadline:
            return False
        self.calls += 1
        return True

    @property
    def exhausted(self) -> bool:
        return self.calls >= self.max_calls or time.monotonic() >= self.deadline


class GhClient:
    """The tracker's only GitHub access, through `gh api` (auth comes from
    the ambient gh configuration — the script never handles tokens)."""

    def __init__(self, gh_bin: str, repo: str, budget: ApiBudget, timeout_sec: float = 120.0):
        self.gh_bin = gh_bin
        self.repo = repo
        self.budget = budget
        self.timeout_sec = timeout_sec

    def api(self, path: str) -> str:
        if not self.budget.spend():
            raise HarvestError("api-budget-exhausted")
        try:
            result = subprocess.run(
                [self.gh_bin, "api", path],
                capture_output=True,
                text=True,
                errors="replace",
                timeout=self.timeout_sec,
            )
        except subprocess.TimeoutExpired:
            raise HarvestError(f"gh-timeout: {path}: no output within {self.timeout_sec}s")
        if result.returncode != 0:
            detail = result.stderr.strip().splitlines()[-1] if result.stderr.strip() else "no stderr"
            raise HarvestError(f"gh-api-failed: {path}: {detail[:200]}")
        return result.stdout

    def api_json(self, path: str) -> dict:
        return json.loads(self.api(path))

    def registry_at(self, head_sha: str) -> tuple[str, str]:
        """The registry snapshot at a commit: (text, blob sha).

        Read through the contents API at the run's head_sha — never the
        current default branch, never a local checkout. No substitution
        fallback: an unreadable snapshot is a gap.
        """
        payload = self.api_json(f"repos/{self.repo}/contents/{REGISTRY_PATH_IN_REPO}?ref={head_sha}")
        if payload.get("encoding") != "base64":
            raise HarvestError("registry-encoding-unsupported")
        text = base64.b64decode(payload["content"]).decode("utf-8")
        return text, str(payload.get("sha") or "")

    def attempt_one_jobs(self, run_id: int) -> dict[str, dict]:
        """Attempt-1 jobs of a run, by job name.

        Attempt 1 is always addressable (`attempts/1/jobs`), also after a
        rerun — verified against a run with run_attempt 2. Later attempts
        are audit-only and are never fetched: they must not replace or
        extend the first result.
        """
        payload = self.api_json(f"repos/{self.repo}/actions/runs/{run_id}/attempts/1/jobs")
        return {job["name"]: job for job in payload.get("jobs", [])}

    def get_run(self, run_id: int) -> dict:
        """Re-resolve one run by id (pending entries may have completed,
        been cancelled, or been deleted since they were discovered)."""
        return self.api_json(f"repos/{self.repo}/actions/runs/{run_id}")

    def job_log(self, job_id: int) -> str:
        return self.api(f"repos/{self.repo}/actions/jobs/{job_id}/logs")


def line_of_branch(branch: str) -> str:
    return "main" if branch == "main" else "release"


def in_population(branch: str) -> bool:
    return branch == "main" or branch.startswith("release/")


def list_push_runs(
    client: GhClient, since_iso: str, current_run: int | None
) -> tuple[list[dict], list[dict], list[dict]]:
    """Completed push runs on main / release/*, oldest first.

    Returns (harvestable, out_of_population, unfinished). Unfinished runs
    (still in progress, not the current run) are returned, not dropped: the
    caller must keep them pending — a run merely DISCOVERED late that is
    then filtered out here would be skipped by the created=> cursor forever,
    leaving no sample and no gap (a silent hole in the sequence).
    """
    harvestable: dict[int, dict] = {}
    out_of_population: list[dict] = []
    unfinished: list[dict] = []
    page = 1
    while True:
        query = f"repos/{client.repo}/actions/workflows/ci.yml/runs?event=push&per_page=100&page={page}"
        query += "&created=" + urllib.parse.quote(f">{since_iso}", safe="")
        payload = client.api_json(query)
        batch = payload.get("workflow_runs", [])
        for run in batch:
            branch = run.get("head_branch") or ""
            if not in_population(branch):
                out_of_population.append(run)
            elif run.get("status") == "completed" or (current_run is not None and run["id"] == current_run):
                # Any terminal conclusion (success/failure/cancelled/...) is
                # harvestable: a cancelled run must become a named gap, not
                # silently vanish — a vanished run would let a streak bridge
                # it. The current run is harvestable before completion
                # because `needs: build` has finished its build matrix.
                harvestable[run["id"]] = run
            else:
                unfinished.append(run)
        if len(batch) < 100 or page >= MAX_LIST_PAGES:
            break
        page += 1
    ordered = sorted(harvestable.values(), key=lambda r: (r.get("created_at") or "", r["id"]))
    return ordered, out_of_population, unfinished


# --- persistence ------------------------------------------------------------------

def load_jsonl(path: Path) -> list[dict]:
    if not path.exists():
        return []
    records = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line:
            records.append(json.loads(line))
    return records


def write_jsonl(path: Path, records: list[dict]) -> None:
    path.write_text(
        "".join(json.dumps(record, ensure_ascii=False, sort_keys=False) + "\n" for record in records),
        encoding="utf-8",
    )


def load_state(path: Path) -> dict:
    if not path.exists():
        return {"schema": STATE_SCHEMA, "config": None, "cursor": None}
    return json.loads(path.read_text(encoding="utf-8"))


def record_key(record: dict) -> str:
    return record["key"]


def event_key(event: dict) -> str:
    return event["event_id"]


def merge_records(*batches: list[dict], key_of=record_key) -> list[dict]:
    """Union-merge record batches by key, with upgrade-only supersession.

    Numeric evidence is immutable once written: a sample is never replaced.
    A gap may be superseded upward only — a retryable collection failure
    (budget exhausted mid-run, transport error) must be able to become real
    evidence on a later harvest, and a retryable gap may firm up into a
    terminal one — but nothing ever downgrades, and equal classes keep the
    earliest copy.
    """
    merged: dict[str, dict] = {}
    for batch in batches:
        for record in batch:
            key = key_of(record)
            existing = merged.get(key)
            if existing is None or record_precedence(record) > record_precedence(existing):
                merged[key] = record
    return list(merged.values())


RETRYABLE_GAP_PREFIXES = ("api-budget-exhausted", "harvest-error", "run-in-progress")


def is_retryable_gap(record: dict) -> bool:
    """Whether a gap's missing evidence may still be collected later.

    Terminal: the run's own shape says the evidence does not exist (no
    parsable reading, cancelled, job absent, expired log, unparsable
    snapshot). Retryable: the tracker's own budget, a transient transport
    failure, or an in-progress run stopped collection — the same attempt-1
    evidence may read fine next harvest, and must be given the chance.
    """
    if record.get("type") != "gap":
        return False
    reason = record.get("reason") or ""
    if reason.startswith(RETRYABLE_GAP_PREFIXES):
        return True
    if "not found or expired" in reason:
        return False  # the log or snapshot is gone; no retry will resurrect it
    if reason.startswith(("registry-unreadable", "jobs-unreadable", "log-unreadable")):
        return True  # transport-class failure; the HTTP verdict text says which
    return False


def record_precedence(record: dict) -> int:
    if record.get("type") == "sample":
        return 2
    return 0 if is_retryable_gap(record) else 1


def runs_collection_complete(records: list[dict]) -> bool:
    """A run is fully collected when none of its records is a retryable gap."""
    return all(not is_retryable_gap(record) for record in records)


def drop_superseded_line_wide_gaps(records: list[dict]) -> list[dict]:
    """Retire a run's line-wide retryable gaps once the run is collected.

    A cold-start collection failure (or an in-progress run) records a
    line-wide gap keyed `run:1::`; the successful collection later writes
    per-series records keyed `run:1:job:phase`. The upgrade-only merge
    cannot replace across those keys, so the stale line-wide row would pin
    the run pending forever. Once a run holds real per-series evidence that
    is fully terminal-or-sample, its line-wide retryable rows have served
    their purpose (the timeline barrier they provided is now redundant) and
    are dropped. Line-wide TERMINAL rows survive: they fence series that
    may not exist yet.
    """
    per_series: dict[int, list[dict]] = {}
    for record in records:
        if record.get("job") != "":
            per_series.setdefault(record["run_id"], []).append(record)
    complete_runs = {
        run_id for run_id, recs in per_series.items() if runs_collection_complete(recs)
    }
    return [
        record for record in records
        if not (
            record.get("job") == ""
            and record.get("type") == "gap"
            and is_retryable_gap(record)
            and record["run_id"] in complete_runs
        )
    ]


def utc_now_iso() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def one_second_before(iso: str) -> str:
    """The listing window's lower bound: one second before the cursor.

    The API's `created=>` boundary granularity is one second, and the
    cursor run itself may share that second with another run; re-listing it
    is free (already-known runs are skipped), missing one is not.
    """
    moment = datetime.strptime(iso, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
    return (moment - timedelta(seconds=1)).strftime("%Y-%m-%dT%H:%M:%SZ")


# --- records -----------------------------------------------------------------------

def gap_record(*, run: dict, line: str, job: str, phase: str, reason: str, config_id: str) -> dict:
    return {
        "type": "gap",
        "key": f"{run['id']}:1:{job}:{phase}",
        "line": line,
        "branch": run.get("head_branch") or "",
        "job": job,
        "phase": phase,
        "run_id": run["id"],
        "run_number": run.get("run_number"),
        "run_url": run.get("html_url") or "",
        "attempt": 1,
        "head_sha": run.get("head_sha") or "",
        "run_created_at": run.get("created_at") or "",
        "reason": reason,
        "config_id": config_id,
        "harvested_at": utc_now_iso(),
    }


def sample_record(
    *,
    run: dict,
    line: str,
    job: str,
    phase: str,
    budget_entry: dict,
    reading: dict,
    registry_blob_sha: str,
    registry_sha256: str,
    config_id: str,
) -> dict:
    seconds = reading["seconds"]
    raw_factor = reading["raw_factor"]
    budget_seconds = budget_entry["budget_seconds"]
    implied_c, headroom_pct = implied_and_headroom(seconds, raw_factor, budget_seconds)
    interval = rounding_interval_pct(seconds, raw_factor, budget_seconds)
    return {
        "type": "sample",
        "key": f"{run['id']}:1:{job}:{phase}",
        "line": line,
        "branch": run.get("head_branch") or "",
        "job": job,
        "phase": phase,
        "run_id": run["id"],
        "run_number": run.get("run_number"),
        "run_url": run.get("html_url") or "",
        "attempt": 1,
        "head_sha": run.get("head_sha") or "",
        "run_created_at": run.get("created_at") or "",
        "registry_blob_sha": registry_blob_sha,
        "registry_sha256": registry_sha256,
        "segment": budget_entry["segment"],
        "config_id": config_id,
        "budget_seconds": budget_seconds,
        "rulers": budget_entry["rulers"],
        "phase_seconds": seconds,
        "gate_factor": reading["gate_factor"],
        "raw_factor": raw_factor,
        "implied_c": implied_c,
        "headroom_pct": headroom_pct,
        "headroom_interval_pct": [round(interval[0], 4), round(interval[1], 4)],
        "approximate": True,  # displayed log operands (0.1s / 0.001) — kept with the original lines
        "log_lines": reading["log_lines"],
        "harvested_at": utc_now_iso(),
    }


def harvest_run(client: GhClient, run: dict, config_id: str) -> list[dict]:
    """Every (job, phase) of one run: a sample or an honest gap, never a skip.

    Whole-run failures (cancelled, unreadable snapshot, jobs listing dead)
    record a single BRANCH-SCOPED line-wide gap rather than per-series rows:
    the branch barrier projects onto EVERY series of that branch — including
    series this same batch or a later harvest introduces — so a new leg's two
    lows can never pair across a dead run that predates the leg's first
    sample. Narrowing the fence to the series known at harvest start would
    leave exactly that hole.
    """
    branch = run.get("head_branch") or ""
    line = line_of_branch(branch)

    def line_wide_gaps(reason: str) -> list[dict]:
        return [gap_record(run=run, line=line, job="", phase="", reason=reason, config_id=config_id)]

    conclusion = run.get("conclusion")
    if conclusion in UNUSABLE_RUN_CONCLUSIONS:
        return line_wide_gaps(f"run-{conclusion}")
    try:
        registry_text, blob_sha = client.registry_at(run["head_sha"])
        budgets = parse_registry_snapshot(registry_text)
    except HarvestError as error:
        return line_wide_gaps(classify_harvest_error(error, default="registry-unreadable"))
    except (json.JSONDecodeError, KeyError, TypeError) as error:
        return line_wide_gaps(f"registry-unparsable: {error}")

    registry_sha256 = hashlib.sha256(registry_text.encode("utf-8")).hexdigest()
    try:
        jobs = client.attempt_one_jobs(run["id"])
    except HarvestError as error:
        return line_wide_gaps(classify_harvest_error(error, default="jobs-unreadable"))

    records: list[dict] = []
    for (job, phase), budget_entry in sorted(budgets.items()):
        job_info = jobs.get(job)
        if job_info is None:
            records.append(gap_record(run=run, line=line, job=job, phase=phase, reason="job-absent", config_id=config_id))
            continue
        if job_info.get("conclusion") not in ("success", "failure"):
            records.append(
                gap_record(
                    run=run, line=line, job=job, phase=phase,
                    reason=f"job-{job_info.get('conclusion')}", config_id=config_id,
                )
            )
            continue
        try:
            log_text = client.job_log(job_info["id"])
        except HarvestError as error:
            records.append(
                gap_record(
                    run=run, line=line, job=job, phase=phase,
                    reason=classify_harvest_error(error, default="log-unreadable"), config_id=config_id,
                )
            )
            continue
        parsed = parse_checker_lines(log_text)
        try:
            reading = extract_reading(parsed, job, phase)
        except ReadingError as error:
            records.append(
                gap_record(run=run, line=line, job=job, phase=phase, reason=f"reading: {error}", config_id=config_id)
            )
            continue
        records.append(
            sample_record(
                run=run,
                line=line,
                job=job,
                phase=phase,
                budget_entry=budget_entry,
                reading=reading,
                registry_blob_sha=blob_sha,
                registry_sha256=registry_sha256,
                config_id=config_id,
            )
        )
    if not records:
        records.append(gap_record(run=run, line=line, job="", phase="", reason="no-machine-factor-budget", config_id=config_id))
    return records


# --- streak walk -------------------------------------------------------------------

def annotate_status(record: dict, threshold_pct: float) -> str:
    """The live classification of a record under the CURRENT config.

    Samples are re-classified from their stored rounding brackets at every
    harvest; a stored `status` would go stale when the threshold changes.
    Gaps stay gaps.
    """
    if record.get("type") != "sample":
        return "gap"
    interval = (record["headroom_interval_pct"][0], record["headroom_interval_pct"][1])
    return classify_headroom(interval, threshold_pct)


def walk_series(
    entries: list[dict], current_config_id: str, threshold_pct: float, min_streak: int
) -> dict:
    """Recompute streak continuity and fired events for one series' history.

    Deterministic from the ledger: the same history always derives the same
    event ids, so re-harvesting never duplicates a signal. Rules:
      - the evaluated era is the suffix of the history after the LAST
        config-change marker (projected into the timeline by group_series):
        history from earlier config eras can neither fire new events under a
        foreign identity nor keep a signal active across a change — including
        a change BACK to a threshold whose hash older samples carry
      - low: streak +1; reaching min_streak fires one event
      - healthy: streak reset and the active event cleared (recovery is an
        observed healthy reading — absence of evidence is not recovery)
      - uncertain / gap: streak reset, active event kept
      - a calibration segment change between neighbours resets the streak —
        no streak spans a re-anchor
    """
    era_start = 0
    for index, entry in enumerate(entries):
        if entry.get("type") == "config-change":
            era_start = index + 1
    era = entries[era_start:]
    streak = 0
    streak_refs: list[dict] = []
    active: dict | None = None
    fired: list[dict] = []
    previous: dict | None = None
    for entry in era:
        if previous is not None and entry.get("segment") != previous.get("segment"):
            streak = 0
            streak_refs = []
        status = annotate_status(entry, threshold_pct)
        if status == "low":
            streak += 1
            streak_refs.append(entry)
            if streak >= min_streak and active is None:
                active = fire_event(
                    entry, streak_refs, current_config_id, threshold_pct, min_streak, streak
                )
                fired.append(active)
        elif status == "healthy":
            streak = 0
            streak_refs = []
            active = None
        else:  # uncertain / gap
            streak = 0
            streak_refs = []
        previous = entry
    return {"streak": streak, "streak_refs": streak_refs, "active": active, "fired": fired}


def fire_event(
    latest: dict,
    streak_refs: list[dict],
    config_id: str,
    threshold_pct: float,
    min_streak: int,
    streak: int,
) -> dict:
    first = streak_refs[0]
    second = streak_refs[1] if len(streak_refs) > 1 else first
    identity = json.dumps(
        {
            "line": latest["line"],
            "branch": latest.get("branch") or "",
            "job": latest["job"],
            "phase": latest["phase"],
            "config_id": config_id,
            "segment": latest.get("segment"),
            "first_run_id": first["run_id"],
        },
        sort_keys=True,
        separators=(",", ":"),
    )
    return {
        "event_id": "evt-" + hashlib.sha256(identity.encode()).hexdigest()[:12],
        "line": latest["line"],
        "branch": latest.get("branch") or "",
        "job": latest["job"],
        "phase": latest["phase"],
        "threshold_pct": threshold_pct,
        "min_streak": min_streak,
        "config_id": config_id,
        "segment": latest.get("segment"),
        "first_run": {"id": first["run_id"], "number": first.get("run_number"), "url": first.get("run_url") or ""},
        "second_run": {"id": second["run_id"], "number": second.get("run_number"), "url": second.get("run_url") or ""},
        "latest_run": {"id": latest["run_id"], "number": latest.get("run_number"), "url": latest.get("run_url") or ""},
        "streak_at_fire": streak,
        "headroom_first_pct": first.get("headroom_pct"),
        "headroom_latest_pct": latest.get("headroom_pct"),
        "fired_at": utc_now_iso(),
    }


def config_change_marker(previous_config_id: str, config_id: str) -> dict:
    """A ledger fence for an observation-config change.

    Era boundaries cannot be derived from the samples' config hashes alone:
    a threshold revised 6% -> 7% -> 6% returns to a hash the older samples
    already carry, and hashing would resurrect the era that the 7% interlude
    explicitly reset. Each observed change writes its own fence — the key
    carries the detection time, because a transition may LEGITIMATELY recur
    (6->7->6->7) and a second occurrence must not be merged away into the
    first: the newest fence is the live era boundary. Concurrent harvesters
    detecting the same change write near-simultaneous fences, which is
    harmless — the era starts after the last of them either way.
    """
    return {
        "type": "config-change",
        "key": f"cfg-change:{previous_config_id or 'none'}:{config_id}:{utc_now_iso()}",
        "line": "",
        "branch": "",
        "job": "",
        "phase": "",
        "run_id": 0,
        "run_number": None,
        "run_url": "",
        "attempt": 0,
        "head_sha": "",
        "run_created_at": utc_now_iso(),
        "previous_config_id": previous_config_id or "",
        "config_id": config_id,
    }


def group_series(records: list[dict]) -> dict[tuple[str, str, str, str], list[dict]]:
    """Group the ledger into per-branch series timelines.

    The series key carries the ACTUAL branch: two release lines inherit the
    same registry and would otherwise pair each other's low samples into a
    cross-branch trigger (and each other's recovery into a silent reset).
    Line-wide gaps (job == "" and type == "gap") — cancelled runs or
    unreadable snapshots recorded before the line's series were known — are
    projected onto every series of the same line and branch as continuity
    barriers, including series this same batch or a later harvest discovers:
    the projection re-runs on the full merged ledger each time, so a barrier
    recorded before a series existed still fences it. Config-change markers
    project the same way and carry the era boundary the walk evaluates from.
    """
    series: dict[tuple[str, str, str, str], list[dict]] = {}
    line_wide: list[dict] = []
    markers: list[dict] = []
    for record in records:
        if record.get("type") == "config-change":
            markers.append(record)
        elif record.get("job") == "":
            line_wide.append(record)
            continue
        else:
            key = (record["line"], record.get("branch") or "", record["job"], record["phase"])
            series.setdefault(key, []).append(record)
    for key, entries in series.items():
        seen_runs = {entry["run_id"] for entry in entries}
        for gap in line_wide:
            if gap["line"] == key[0] and (gap.get("branch") or "") == key[1] and gap["run_id"] not in seen_runs:
                entries.append(gap)
        entries.extend(markers)
    for entries in series.values():
        entries.sort(key=lambda r: (r.get("run_created_at") or "", r["run_id"], r.get("type") or ""))
    return series


def series_label(line: str, branch: str, job: str, phase: str) -> str:
    where = line if line == "main" else f"{line}:{branch}"
    return f"{where}/{job}/{phase}"


def evaluate_series(
    records: list[dict], current_config_id: str, threshold_pct: float, min_streak: int
) -> tuple[list[dict], list[dict], list[dict]]:
    """Per-series walk over the merged ledger; returns (summary rows, active events, fired events)."""
    rows: list[dict] = []
    active_all: list[dict] = []
    fired_all: list[dict] = []
    for (line, branch, job, phase), entries in sorted(group_series(records).items()):
        result = walk_series(entries, current_config_id, threshold_pct, min_streak)
        latest = entries[-1]
        active_all.extend([result["active"]] if result["active"] else [])
        fired_all.extend(result["fired"])
        rows.append(
            {
                "label": series_label(line, branch, job, phase),
                "latest": latest if latest.get("type") == "sample" else None,
                "latest_run_number": latest.get("run_number"),
                "latest_reason": latest.get("reason", ""),
                "status": annotate_status(latest, threshold_pct),
                "streak": result["streak"],
                "signal": "ACTIVE" if result["active"] else "-",
            }
        )
    return rows, active_all, fired_all


# --- summary + annotations -----------------------------------------------------------

def render_summary(
    *,
    repo: str,
    config: dict,
    config_changed_note: str | None,
    totals: dict,
    per_series: list[dict],
    active_events: list[dict],
    gaps_this_harvest: list[dict],
    out_of_population: int,
    backlog_note: str | None,
    publish_note: str,
) -> str:
    lines = [
        "## Ctest headroom (observation only — never a gate)",
        "",
        f"- Config: threshold `{config['threshold_pct']}%`, min streak `{config['min_streak']}`, "
        f"identity `{config['config_id']}`",
    ]
    if config_changed_note:
        lines.append(f"- {config_changed_note}")
    lines.extend(
        [
            f"- Coverage: {totals['runs_processed']} run(s) processed this harvest, "
            f"{totals['runs_overlap']} run(s) listed but not processed (already-harvested overlap "
            f"or budget backlog), "
            f"{totals['records_new']} new sample/gap record(s), {out_of_population} out-of-population run(s) ignored",
            f"- Sample pool: {totals['samples']} sample(s) and {totals['gaps']} gap(s) across "
            f"{len(per_series)} series",
        ]
    )
    if backlog_note:
        lines.append(f"- Backlog: {backlog_note} — remaining runs are next harvest's input, not skipped")
    if gaps_this_harvest:
        lines.append("")
        lines.append("### Gaps this harvest")
        lines.append("")
        for gap in gaps_this_harvest:
            where = f"{gap['line']}/{gap['job'] or '(line)'}/{gap['phase'] or '-'}"
            lines.append(f"- `{where}` run {gap['run_number']}: {gap['reason']} ({gap['run_url']})")
    lines.append("")
    lines.append("### Series (latest reading; every reading is a log-rounding approximation)")
    lines.append("")
    lines.append("| Line / job / phase | Latest run | T / raw f | implied C | headroom | Status | Streak | Signal |")
    lines.append("|---|---|---|---|---|---|---|---|")
    for entry in per_series:
        sample = entry["latest"]
        if sample is None:
            latest_text = f"run {entry['latest_run_number']}: gap ({entry['latest_reason']})"
            lines.append(
                f"| {entry['label']} | {latest_text} | - | - | - | - | "
                f"{entry['streak']} | {entry['signal']} |"
            )
        else:
            lines.append(
                f"| {entry['label']} | "
                f"[{sample['run_number']}]({sample['run_url']}) | "
                f"{sample['phase_seconds']}s / {sample['raw_factor']} | "
                f"{sample['implied_c']:.1f}s vs {sample['budget_seconds']}s | "
                f"{sample['headroom_pct']:.2f}% | {entry['status']} | {entry['streak']} | {entry['signal']} |"
            )
    lines.append("")
    lines.append("### Active low-headroom signals (observation — disposition is the owner's)")
    lines.append("")
    if active_events:
        for event in active_events:
            lines.append(
                f"- `{event['line']}/{event['job']}/{event['phase']}`: headroom "
                f"{event['headroom_latest_pct']:.2f}% < {event['threshold_pct']}% over "
                f"{event['streak_at_fire']}+ consecutive runs, segment `{event['segment']}`, "
                f"first [run {event['first_run']['number']}]({event['first_run']['url']}), "
                f"latest [run {event['latest_run']['number']}]({event['latest_run']['url']})"
            )
    else:
        lines.append("None.")
    lines.append("")
    lines.append(f"- Persistence: {publish_note}")
    lines.append("")
    return "\n".join(lines)


def annotation_line(event: dict) -> str:
    where = event["line"] if event["line"] == "main" else f"{event['line']}:{event.get('branch') or ''}"
    return (
        f"::notice::[ctest-headroom] {where}/{event['job']}/{event['phase']}: headroom "
        f"{event['headroom_latest_pct']:.2f}% < {event['threshold_pct']}% over "
        f"{event['streak_at_fire']}+ consecutive runs (segment {event['segment']}, "
        f"first run {event['first_run']['number']}: {event['first_run']['url']}) — "
        "observation only; disposition belongs to the owner"
    )


def report_warnings(warnings: list[str]) -> None:
    for warning in warnings:
        print(f"::warning::[ctest-headroom] {warning}")


# --- publish ---------------------------------------------------------------------------

def _git(repo_root: Path, *args: str) -> subprocess.CompletedProcess:
    return subprocess.run(
        ["git", "-C", str(repo_root), *args],
        capture_output=True,
        text=True,
        errors="replace",
        timeout=180,
    )


def fetch_publish_ref(repo_root: Path, remote: str, ref: str) -> bool:
    return _git(repo_root, "fetch", "--depth=1", remote, ref).returncode == 0


def read_published_files(repo_root: Path, ref_head: str, names: tuple[str, ...], into: Path) -> bool:
    """Copy published state files out of a fetched ref head, without a worktree.

    Returns False only when the headroom/ directory does not exist on the ref
    (a legitimately fresh line). If the directory exists but ANY state file
    fails to read, that is a corrupt or partial published state: the caller
    must fail loudly, never treat it as empty — silently continuing here is
    how a concurrent harvester's records get overwritten.
    """
    probe = _git(repo_root, "show", f"{ref_head}:{PUBLISH_DIR}/{names[0]}")
    if probe.returncode != 0:
        return False
    into.mkdir(parents=True, exist_ok=True)
    for name in names:
        blob = _git(repo_root, "show", f"{ref_head}:{PUBLISH_DIR}/{name}")
        if blob.returncode != 0:
            raise HarvestError(f"published-state-corrupt: {PUBLISH_DIR}/{name} unreadable on {ref_head}")
        (into / name).write_text(blob.stdout, encoding="utf-8")
    return True


def read_published_state(repo_root: Path, remote: str, ref: str, into: Path) -> bool:
    """Bootstrap the local state dir from the published headroom/ directory.

    Returns False when the ref or the directory does not exist yet — a fresh
    line starts from now, it does not fabricate earlier history.
    """
    if not fetch_publish_ref(repo_root, remote, ref):
        return False
    return read_published_files(repo_root, "FETCH_HEAD", STATE_FILES, into)


def merge_into_state_dir(state_dir: Path, theirs: Path) -> None:
    """Union-merge another copy of the state into ours (records by key, cursor
    and pending max/union). Both sides' facts survive; the config of the
    acting harvest wins and the config-change annotation explains the break.

    Pending is UNIONED, not kept one-sided: the remote may have a newer
    cursor while still owing runs older than it — dropping their pending
    entries here would push those runs out of the created=> window forever
    (the cursor moves on, the run never completes its collection, and no
    gap records the hole).
    """
    samples = merge_records(load_jsonl(state_dir / "samples.jsonl"), load_jsonl(theirs / "samples.jsonl"))
    samples = drop_superseded_line_wide_gaps(samples)
    write_jsonl(state_dir / "samples.jsonl", samples)
    events = merge_records(
        load_jsonl(state_dir / "events.jsonl"), load_jsonl(theirs / "events.jsonl"), key_of=event_key
    )
    write_jsonl(state_dir / "events.jsonl", events)
    ours_state = load_state(state_dir / "state.json")
    theirs_state = load_state(theirs / "state.json") if (theirs / "state.json").exists() else {}
    ours_cursor = ours_state.get("cursor") or {}
    theirs_cursor = theirs_state.get("cursor") or {}

    def cursor_order(cursor: dict) -> tuple[str, int]:
        return (cursor.get("created_at") or "", cursor.get("run_id") or 0)

    winner = ours_cursor if cursor_order(ours_cursor) >= cursor_order(theirs_cursor) else theirs_cursor
    ours_state["cursor"] = winner
    pending: dict[int, dict] = {entry["run_id"]: entry for entry in ours_state.get("pending") or []}
    for entry in theirs_state.get("pending") or []:
        pending.setdefault(entry["run_id"], entry)
    ours_state["pending"] = sorted(pending.values(), key=lambda p: (p.get("created_at") or "", p["run_id"]))
    (state_dir / "state.json").write_text(json.dumps(ours_state, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def publish_state(
    repo_root: Path, remote: str, ref: str, state_dir: Path, rebuild_summary=None
) -> tuple[bool, str]:
    """Replace headroom/ on the publish ref with the state dir; touch nothing else.

    EVERY successful fetch is merged before committing — the first attempt
    included. Between this harvest's bootstrap and its publish, another
    aggregation run may have landed records: committing our bootstrap-era
    snapshot on top of that newer commit would be a legal fast-forward that
    silently deletes their evidence, and the rejection-retry path would
    never see it. Merging the actual parent snapshot each attempt (records
    union-merged by key, events by id, cursor max) makes every push carry
    both sides' facts; force push does not exist on this path. A fetched
    headroom/ whose files do not all read is a hard failure, never treated
    as empty.

    `rebuild_summary` runs after each merge and before the commit so the
    published copy always describes the exact ledger being committed.
    """
    worktrees: list[Path] = []
    hooks_off = state_dir.parent / f"{state_dir.name}-no-hooks"
    hooks_off.mkdir(parents=True, exist_ok=True)

    def git_in(worktree: Path, *args: str) -> subprocess.CompletedProcess:
        return subprocess.run(
            ["git", "-C", str(worktree), "-c", f"core.hooksPath={hooks_off}", *args],
            capture_output=True,
            text=True,
            errors="replace",
            timeout=180,
        )

    try:
        for attempt in range(1, PUBLISH_RETRIES + 1):
            if not fetch_publish_ref(repo_root, remote, ref):
                return False, "fetch of the publish ref failed"
            theirs = state_dir.parent / f"{state_dir.name}-theirs-{attempt}"
            theirs.mkdir(parents=True, exist_ok=True)
            try:
                had_published = read_published_files(repo_root, "FETCH_HEAD", STATE_FILES, theirs)
            except HarvestError as error:
                return False, str(error)
            if had_published:
                merge_into_state_dir(state_dir, theirs)
                if rebuild_summary is not None:
                    rebuild_summary(state_dir)
            worktree = state_dir.parent / f"{state_dir.name}-publish-{attempt}"
            _git(repo_root, "worktree", "remove", "--force", str(worktree))
            add = _git(repo_root, "worktree", "add", "--detach", str(worktree), "FETCH_HEAD")
            if add.returncode != 0:
                return False, f"worktree add failed: {add.stderr.strip()[:200]}"
            worktrees.append(worktree)
            target = worktree / PUBLISH_DIR
            if target.exists():
                for existing in target.iterdir():
                    if existing.is_file():
                        existing.unlink()
                    else:
                        subprocess.run(["rm", "-rf", str(existing)], check=True)
            else:
                target.mkdir(parents=True)
            for name in STATE_FILES:
                (target / name).write_text((state_dir / name).read_text(encoding="utf-8"), encoding="utf-8")
            staged = git_in(worktree, "add", "-A", PUBLISH_DIR)
            if staged.returncode != 0:
                return False, f"git add failed: {staged.stderr.strip()[:200]}"
            unchanged = git_in(worktree, "status", "--porcelain")
            if not unchanged.stdout.strip():
                return True, "already up to date (no change)"
            commit = git_in(
                worktree,
                "-c", "user.name=github-actions[bot]",
                "-c", "user.email=github-actions[bot]@users.noreply.github.com",
                "commit", "-m", f"headroom: harvest {utc_now_iso()}",
            )
            if commit.returncode != 0:
                return False, f"commit failed: {commit.stderr.strip()[:200]}"
            push = git_in(worktree, "push", remote, f"HEAD:refs/heads/{ref}")
            if push.returncode == 0:
                return True, f"published to {ref}:{PUBLISH_DIR}/"
            stderr = push.stderr.lower()
            if "non-fast-forward" in stderr or "rejected" in stderr or "fetch first" in stderr:
                continue  # bounded retry with merge
            return False, f"push failed: {push.stderr.strip()[:200]}"
        return False, "publish retries exhausted (the remote kept moving)"
    finally:
        for worktree in worktrees:
            _git(repo_root, "worktree", "remove", "--force", str(worktree))
            _git(repo_root, "worktree", "prune")


# --- harvest driver ----------------------------------------------------------------------

def finalize(
    *,
    config: dict,
    config_changed_note: str | None,
    totals: dict,
    per_series: list[dict],
    active_events: list[dict],
    gaps_this_harvest: list[dict],
    out_of_population: int,
    backlog_note: str | None,
    publish_note: str,
    paths: dict,
    args: argparse.Namespace,
) -> None:
    """Write the local outcome summary; print signals and warnings.

    Ledgers/state/summary are already on disk (the publish snapshot was
    written before publishing; this pass rewrites the local summary with the
    attempt's actual outcome and appends it to the Job summary).
    """
    summary = render_summary(
        repo=args.repo,
        config=config,
        config_changed_note=config_changed_note,
        totals=totals,
        per_series=per_series,
        active_events=active_events,
        gaps_this_harvest=gaps_this_harvest,
        out_of_population=out_of_population,
        backlog_note=backlog_note,
        publish_note=publish_note,
    )
    paths["summary"].write_text(summary, encoding="utf-8")
    if args.job_summary:
        try:
            with args.job_summary.open("a", encoding="utf-8") as output:
                output.write("\n" + summary)
        except OSError as error:
            print(f"::warning::[ctest-headroom] job summary write failed: {error}")
    if args.emit_annotation:
        for event in active_events:
            print(annotation_line(event))
    print(summary, end="")


def persist_ledgers(
    *, samples: list[dict], events: list[dict], fired_events: list[dict],
    cursor: dict | None, pending: list[dict], config: dict, paths: dict,
) -> None:
    """Write the merged ledgers and state BEFORE publishing.

    A publish-side merge (concurrent harvest on the remote) unions more
    facts into these files; the post-publish reload in harvest_command then
    reads the merged truth, so nothing the merge brought in is lost.
    """
    all_events = merge_records(events, fired_events, key_of=event_key)
    write_jsonl(paths["samples"], samples)
    write_jsonl(paths["events"], all_events)
    state = {"schema": STATE_SCHEMA, "config": config, "cursor": cursor, "pending": pending}
    paths["state"].write_text(json.dumps(state, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def harvest_command(args: argparse.Namespace) -> int:
    state_dir = args.state_dir
    state_dir.mkdir(parents=True, exist_ok=True)
    paths = {
        "samples": state_dir / "samples.jsonl",
        "events": state_dir / "events.jsonl",
        "state": state_dir / "state.json",
        "summary": state_dir / "summary.md",
    }
    warnings: list[str] = []

    config_id = config_identity(args.threshold_pct, args.min_streak)
    config = {"threshold_pct": args.threshold_pct, "min_streak": args.min_streak, "config_id": config_id}

    if args.publish:
        try:
            read_published_state(args.repo_root, args.publish_remote, args.publish_ref, state_dir)
        except HarvestError as error:
            warnings.append(f"published state bootstrap: {error}")

    previous_state = load_state(paths["state"])
    config_changed_note = None
    era_markers: list[dict] = []
    if previous_state.get("config") and previous_state["config"].get("config_id") != config_id:
        config_changed_note = (
            f"Observation config changed (threshold {previous_state['config'].get('threshold_pct')}% -> "
            f"{args.threshold_pct}%, streak {previous_state['config'].get('min_streak')} -> {args.min_streak}): "
            f"signals re-derive under identity `{config_id}`; the era boundary is fenced in the ledger, "
            f"so history from earlier config eras cannot fire events or keep a signal active under this one"
        )
        era_markers.append(
            config_change_marker(previous_state["config"].get("config_id") or "", config_id)
        )

    samples = load_jsonl(paths["samples"])
    events = load_jsonl(paths["events"])
    records_by_run: dict[int, list[dict]] = {}
    for record in samples:
        records_by_run.setdefault(record["run_id"], []).append(record)
    complete_run_ids = {run_id for run_id, recs in records_by_run.items() if runs_collection_complete(recs)}

    if previous_state.get("cursor"):
        since_iso = one_second_before(previous_state["cursor"]["created_at"])
    else:
        since_iso = (datetime.now(timezone.utc) - timedelta(days=args.window_days)).strftime("%Y-%m-%dT%H:%M:%SZ")

    budget = ApiBudget(args.max_api_calls, time.monotonic() + args.time_budget_sec)
    client = GhClient(args.gh_bin, args.repo, budget, timeout_sec=args.api_timeout_sec)
    new_records: list[dict] = []

    # Runs still owed collection: pending entries from the previous state
    # (re-resolved by id — a completed, cancelled or deleted run changes what
    # it can give us) plus anything the window listing finds that the ledger
    # has not fully collected. A run with only retryable gaps is NOT done.
    work: dict[int, dict] = {}
    for entry in previous_state.get("pending") or []:
        if entry["run_id"] in complete_run_ids:
            continue
        try:
            run_obj = client.get_run(entry["run_id"])
        except HarvestError as error:
            reason = classify_harvest_error(error, default="run-unreadable")
            if "not found or expired" in reason:
                # Deleted from the server: a terminal gap fences the sequence,
                # and the pending entry is discharged.
                new_records.append(
                    gap_record(
                        run={"id": entry["run_id"], "run_number": entry.get("run_number"),
                             "head_branch": entry.get("branch") or "", "html_url": "",
                             "head_sha": "", "created_at": entry.get("created_at") or ""},
                        line=line_of_branch(entry.get("branch") or "main"),
                        job="", phase="", reason="run-not-found", config_id=config_id,
                    )
                )
                continue
            warnings.append(f"pending run {entry.get('run_number')} re-resolve failed: {reason}")
            work[entry["run_id"]] = {
                "id": entry["run_id"], "run_number": entry.get("run_number"),
                "head_branch": entry.get("branch") or "", "created_at": entry.get("created_at") or "",
                "html_url": "", "head_sha": "", "status": "unknown", "conclusion": None,
            }
            continue
        work[run_obj["id"]] = run_obj

    try:
        runs, out_of_population, unfinished = list_push_runs(client, since_iso, args.current_run)
    except HarvestError as error:
        warnings.append(f"run listing failed: {classify_harvest_error(error, default='listing-failed')}")
        runs, unfinished, out_of_population = [], [], []
    for run in unfinished:
        work.setdefault(run["id"], run)
    for run in runs:
        if run["id"] not in complete_run_ids:
            work.setdefault(run["id"], run)

    ordered_work = sorted(work.values(), key=lambda r: (r.get("created_at") or "", r["id"]))
    processed_runs: list[dict] = []
    backlog_note = None
    for run in ordered_work:
        if run["id"] in complete_run_ids:
            continue
        if len(processed_runs) >= args.max_runs or budget.exhausted:
            backlog_note = (
                f"run budget ({args.max_runs}) reached" if len(processed_runs) >= args.max_runs
                else f"API/time budget reached ({budget.calls} calls)"
            )
            break
        status = run.get("status")
        harvestable_now = status == "completed" or (
            status == "in_progress" and run["id"] == args.current_run
        )
        if not harvestable_now:
            if status == "unknown":
                # A transient re-resolve failure left this pending run with no
                # trustworthy identity: collecting it now would fetch a
                # registry "snapshot" with no ref. It stays pending for the
                # next harvest — the warn names it, nothing is signed.
                warnings.append(
                    f"pending run {run.get('run_number')} could not be re-resolved; "
                    "kept pending (no evidence collected)"
                )
            else:
                # Still in progress and not this harvest's own run: it must
                # fence the timeline NOW — a pending-only identity would leave
                # the two low samples around it adjacent in the ledger and
                # pair them — so a retryable line-wide gap is recorded; the
                # run stays pending and the gap upgrades to real evidence
                # when collection succeeds.
                new_records.append(
                    gap_record(run=run, line=line_of_branch(run.get("head_branch") or ""), job="", phase="",
                               reason="run-in-progress", config_id=config_id)
                )
            continue
        try:
            batch = harvest_run(client, run, config_id)
        except HarvestError as error:
            warnings.append(f"run {run.get('run_number')} harvest failed: {classify_harvest_error(error, default='harvest-failed')}")
            batch = [gap_record(run=run, line=line_of_branch(run.get("head_branch") or ""), job="", phase="",
                                reason="harvest-error", config_id=config_id)]
        new_records.extend(batch)
        processed_runs.append(run)

    merged_samples = drop_superseded_line_wide_gaps(
        merge_records(samples + era_markers, new_records)
    )
    records_by_run = {}
    for record in merged_samples:
        records_by_run.setdefault(record["run_id"], []).append(record)

    # Pending = every owed run that is still not fully collected, now or from
    # previous states. This is what keeps a mid-run budget death or a
    # transient transport failure from becoming a permanent hole: the cursor
    # may move past such a run, but pending pins its identity for the next
    # harvest to re-resolve and re-collect, and the retryable gap is then
    # upgraded in place (merge is upgrade-only).
    pending: list[dict] = []
    for run_id, run_obj in work.items():
        recs = records_by_run.get(run_id, [])
        if recs and runs_collection_complete(recs):
            continue
        pending.append(
            {
                "run_id": run_id,
                "created_at": run_obj.get("created_at") or "",
                "run_number": run_obj.get("run_number"),
                "branch": run_obj.get("head_branch") or "",
            }
        )
    pending.sort(key=lambda p: (p["created_at"], p["run_id"]))

    _, _, fired_events = evaluate_series(merged_samples, config_id, args.threshold_pct, args.min_streak)

    cursor = previous_state.get("cursor")
    if processed_runs:
        last = processed_runs[-1]
        candidate = {
            "run_id": last["id"],
            "created_at": last.get("created_at") or "",
            "run_number": last.get("run_number"),
        }
        if not cursor or (candidate["created_at"], candidate["run_id"]) >= (cursor.get("created_at") or "", cursor.get("run_id") or 0):
            cursor = candidate

    persist_ledgers(
        samples=merged_samples, events=events, fired_events=fired_events,
        cursor=cursor, pending=pending, config=config, paths=paths,
    )

    # The publish snapshot must be complete before publishing: all four files,
    # with a summary derived from exactly this ledger. The attempt's outcome
    # line is written to the local copy after the publish returns — the
    # published copy carries the neutral published-state line instead.
    published_copy_note = (
        "published to the gh-pages headroom/ directory (this copy describes the committed ledger; "
        "the harvest attempt's outcome is in the aggregation run's Job summary)"
    )

    # The publish snapshot must be complete and self-consistent: all four
    # files derived from exactly the ledger being committed. A publish-side
    # merge (concurrent harvester) can bring records that change the derived
    # events — those MUST be recomputed into events.jsonl along with the
    # summary, or the snapshot would carry a summary and an event ledger that
    # disagree about the same samples.
    def render_into(state: Path, note: str) -> None:
        merged = drop_superseded_line_wide_gaps(load_jsonl(state / "samples.jsonl"))
        write_jsonl(state / "samples.jsonl", merged)
        rows, active, fired = evaluate_series(merged, config_id, args.threshold_pct, args.min_streak)
        existing_events = load_jsonl(state / "events.jsonl")
        write_jsonl(state / "events.jsonl", merge_records(existing_events, fired, key_of=event_key))
        summary = render_summary(
            repo=args.repo,
            config=config,
            config_changed_note=config_changed_note,
            totals={
                "runs_processed": len(processed_runs),
                "runs_overlap": max(0, len(runs) + len(unfinished) + len(previous_state.get("pending") or []) - len(processed_runs)),
                "records_new": len(new_records),
                "samples": sum(1 for r in merged if r["type"] == "sample"),
                "gaps": sum(1 for r in merged if r["type"] == "gap"),
            },
            per_series=rows,
            active_events=active,
            gaps_this_harvest=[r for r in new_records if r["type"] == "gap"],
            out_of_population=len(out_of_population),
            backlog_note=backlog_note,
            publish_note=note,
        )
        (state / "summary.md").write_text(summary, encoding="utf-8")
        return

    render_into(state_dir, published_copy_note)

    publish_note = "local only (no --publish)"
    if args.publish:
        ok, detail = publish_state(
            args.repo_root, args.publish_remote, args.publish_ref, state_dir,
            rebuild_summary=lambda state: render_into(state, published_copy_note),
        )
        publish_note = (
            detail if ok
            else f"NOT PERSISTED — {detail}; the next harvest re-collects from the published cursor"
        )
        if not ok:
            warnings.append(f"publish failed: {detail}")

    # Post-publish reload: a concurrent-publish merge may have unioned the
    # other side's facts into the ledgers; the summary and the signals must
    # describe the merged truth, not the pre-publish snapshot. Recompute is
    # deterministic, so this cannot invent events the pre-publish pass
    # did not see.
    final_samples = load_jsonl(paths["samples"])
    per_series_rows, active_events, _ = evaluate_series(
        final_samples, config_id, args.threshold_pct, args.min_streak
    )
    totals = {
        "runs_processed": len(processed_runs),
        "runs_overlap": max(0, len(runs) + len(unfinished) + len(previous_state.get("pending") or []) - len(processed_runs)),
        "records_new": len(new_records),
        "samples": sum(1 for r in final_samples if r["type"] == "sample"),
        "gaps": sum(1 for r in final_samples if r["type"] == "gap"),
    }
    finalize(
        config=config,
        config_changed_note=config_changed_note,
        totals=totals, per_series=per_series_rows,
        active_events=active_events, gaps_this_harvest=[r for r in new_records if r["type"] == "gap"],
        out_of_population=len(out_of_population), backlog_note=backlog_note,
        publish_note=publish_note, paths=paths, args=args,
    )
    report_warnings(warnings)
    if args.publish and publish_note.startswith("NOT PERSISTED"):
        return 3
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="command", required=True)
    harvest = sub.add_parser("harvest", help="harvest CI run logs into the headroom ledger")
    harvest.add_argument("--repo", required=True, help="OWNER/REPO")
    harvest.add_argument("--state-dir", type=Path, required=True)
    harvest.add_argument("--repo-root", type=Path, default=REPO_ROOT, help="git repo used for publish fetch/push")
    harvest.add_argument("--window-days", type=int, default=DEFAULT_WINDOW_DAYS)
    harvest.add_argument("--current-run", type=int, help="run id to include even though not completed yet")
    harvest.add_argument("--max-runs", type=int, default=DEFAULT_MAX_RUNS)
    harvest.add_argument("--max-api-calls", type=int, default=DEFAULT_MAX_API_CALLS)
    harvest.add_argument("--time-budget-sec", type=float, default=DEFAULT_TIME_BUDGET_SEC)
    harvest.add_argument("--threshold-pct", type=float, default=DEFAULT_THRESHOLD_PCT)
    harvest.add_argument("--min-streak", type=int, default=DEFAULT_MIN_STREAK)
    harvest.add_argument("--gh-bin", default="gh")
    harvest.add_argument("--api-timeout-sec", type=float, default=120.0,
                         help="per-call subprocess timeout; a timeout is a retryable failure")
    harvest.add_argument("--emit-annotation", action="store_true")
    harvest.add_argument("--job-summary", type=Path)
    harvest.add_argument("--publish", action="store_true")
    harvest.add_argument("--publish-ref", default="gh-pages")
    harvest.add_argument("--publish-remote", default="origin")
    args = parser.parse_args(argv)
    if args.command == "harvest":
        try:
            return harvest_command(args)
        except Exception as error:  # noqa: BLE001 - the tracker is observation: any crash
            # must still leave the evidence trail (a named warning and a
            # summary naming the crash) instead of a bare traceback.
            message = f"harvest crashed: {type(error).__name__}: {error}"
            print(f"::warning::[ctest-headroom] {message}")
            try:
                state_dir = Path(args.state_dir)
                state_dir.mkdir(parents=True, exist_ok=True)
                with (state_dir / "summary.md").open("a", encoding="utf-8") as output:
                    output.write(f"\n## HARVEST CRASHED\n\n- {message}\n")
            except OSError:
                pass
            return 2
    parser.error(f"unknown command {args.command!r}")
    return 2


if __name__ == "__main__":
    sys.exit(main())
