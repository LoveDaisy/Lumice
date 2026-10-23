#!/usr/bin/env python3
"""Diff-scoped check: new test translation units beyond the free quota must be declared.

Fifth diff-scoped entry point next to check_policies.py, check_new_refs.py,
check_new_gui_tests.py and check_loop_fatal_asserts.py. The first asks "what
state is the repo in?"; the middle three each ask "what did you just write?"
over one domain. This one asks the same question over the compile-cost domain
the code-mass census measured: test TU count grew 47 -> 314 in four months
(6.7x), and test sources are 69% of a serial build (660s of 956s on the census
machine). The per-miss build budget prices what a PR *actually cost* after the
fact; this gate is the *before* half of the same question — how much compile
mass a PR declares it is adding — and the two share no code, no registry and
no region of ci.yml by design.

The rule
--------
A test TU is an ADDED file whose repo path starts with `test/` and whose
suffix is one of TEST_TU_SUFFIXES (see is_test_tu — that predicate and its two
constants are the single definition; every rule below reads through it).
Renames do not count (rename detection on, same stance as check_new_refs.py:
a `git mv` relocates content its author never wrote); a copy reads as a plain
add and counts, because whoever created the new path put a TU there.

The first FREE_NEW_TEST_TUS added test TUs in one PR are free. Beyond the
quota, every added TU must be covered by ledger entries THIS DIFF adds to
`test/tu_cost_ledger.json`, each with a real reason. The ledger is an
append-only audit record of bulk test-mass additions; it is never cleaned and
never expires, and entries do not carry across PRs — an entry merged last week
is inventory for this PR's base, not quota for this PR.

Two mechanical invariants hold the ledger honest (both are assertions the
checker evaluates from git, not conventions it trusts):

  1. Append-only. Every entry visible at the base ref must still be present,
     unmodified, at the head ref — otherwise rewording one existing entry's
     reason would surface in the set difference as a "new declaration" and
     mint quota for free. Edits and deletions of existing entries are red.

  2. Cross-checked sum. When the diff adds ledger entries at all, their
     `new_test_tus` values must sum to EXACTLY the number of test TUs the
     diff adds — under-claiming sneaks past the quota, over-claiming mints
     unused quota into the audit trail. The count itself is computed here
     from git, so the comparison is assertion vs assertion, never self-report
     vs trust. The check applies ONLY when the diff adds entries: the
     steady-state path (a small PR, a ledger that already exists, zero new
     entries) must stay green, and a naive `sum() == new_tus` over an empty
     set would kill the free quota for every PR after the first declaration.

The hub half (notice, not a gate)
---------------------------------
The same checker makes one more thing visible without blocking anything: a PR
that touches a hub header — one whose depfile closure reaches
HUB_NOTICE_MIN_TUS translation units — gets a `::notice` naming the header and
its closure. That is the whole feature: enforcement strength matched to event
frequency. Quota-exceeding TU additions are rare (~3% of merged PRs), so a
declaration gate fits; hub touches are routine (a quarter of merged PRs touch
a closure>=150 header), and a per-PR forced declaration for a routine event
decays into boilerplate. The notice costs nothing, names the header at review
time, and prices nothing — the compile cost of a hub touch is already priced
mechanically by the per-miss build budget, so this half never estimates
seconds.

The closure comes from `ninja -t deps` output passed via --deps (the census's
authority on include reachability; a regex include graph was measured to miss
edges systematically and is not used). The notice half is evaluated on the
judging CI leg's toolchain view only: conditional CUDA/Metal headers are
invisible there and never trigger. Without --deps the half is skipped with one
audible line (CI is the authority; local is fast feedback). A deps payload
that parses to zero translation units fails loud with a `deps-parse-error`
marker instead of passing silently: hub misses have no red state of their own
to expose a silently dead checker, so a short red beats an eternal green.

Ledger corrections
------------------
The append-only invariant means a merged entry cannot be edited through the
normal path — the checker is deliberately blind to intent there. A genuine
correction needs an owner decision first; until the checker grows an explicit,
loud amendment channel, there is no in-place workaround to reach for.

Usage
-----
  python3 scripts/check_new_test_tus.py --staged          # pre-commit: the index
  python3 scripts/check_new_test_tus.py --range A..B      # CI: PR vs merge-base
  python3 scripts/check_new_test_tus.py --range A..B --deps deps.txt --summary sum.md

Exit code 0 when clean, 1 when the TU half is red or a deps payload is
unparsable. Notices never affect the exit code.
"""
from __future__ import annotations

import argparse
import datetime
import json
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path, PurePosixPath

sys.path.insert(0, str(Path(__file__).resolve().parent))

from check_new_refs import added_lines, blob  # noqa: E402
from check_policies import REPO_ROOT, Violation  # noqa: E402

RULE = "undeclared-new-test-tus"

# Calibration (130 most recent merged PRs at the time of writing, 2026-09-16
# through 2026-10-19, added test TUs per PR): min=0 p25=0 median=0 p75=1
# p90=2 p95=4 max=11; only 4 of 130 PRs exceeded 5, all bulk feature PRs
# (schema3 report, analytic module, diagnostics work). The quota sits just
# above the routine mass so an ordinary PR never sees the gate. Changing the
# value is a re-calibration: new numbers in this comment AND in the rule
# record in doc/testing/testing-architecture.md §7.9 — never a silent bump.
FREE_NEW_TEST_TUS = 5

# Calibration (same PR sample, hub-touch frequency via ninja depfile closures
# over 522 TUs / 253 project headers): headers with closure >= 150 number 15
# and were touched by 32/130 (25%) of merged PRs; closure >= 100 is 37 heads /
# 44/130 (34%). A quarter of PRs touching the threshold means hub touches are
# routine events — which is exactly why this half is a notice and not a gate.
# Same re-calibration discipline as FREE_NEW_TEST_TUS.
HUB_NOTICE_MIN_TUS = 150

# The mechanical definition of "a test TU" for this gate, in one place: an
# added file whose repo-relative path starts with TEST_TU_SCOPE_PREFIX and
# whose suffix is in TEST_TU_SUFFIXES. is_test_tu() is the only reader; the
# unit tests pass through these constants from both directions (a `test/`
# .cpp must hit, a non-`test/` path and a non-listed suffix must not), so an
# invisible-character edit or a narrowed constant fails a named case instead
# of silently shrinking the rule.
TEST_TU_SCOPE_PREFIX = "test/"
TEST_TU_SUFFIXES = frozenset({".cpp", ".cc", ".mm", ".cu"})


def is_test_tu(rel: str) -> bool:
    return rel.startswith(TEST_TU_SCOPE_PREFIX) and PurePosixPath(rel).suffix in TEST_TU_SUFFIXES


# The hub half's header definition, same single-reader discipline: a touched
# file under src/ or test/ ending in .h/.hpp. Suffixes beyond the list (.hh,
# .ipp, ...) are deliberately out — the closure census counted exactly these
# two, and the closures this half reports come from that same counting.
HUB_SCOPE_PREFIXES = ("src/", "test/")
HUB_HEADER_SUFFIXES = frozenset({".h", ".hpp"})


def is_hub_header(rel: str) -> bool:
    return rel.startswith(HUB_SCOPE_PREFIXES) and PurePosixPath(rel).suffix in HUB_HEADER_SUFFIXES


LEDGER_RELPATH = "test/tu_cost_ledger.json"
LEDGER_VERSION = 1
LEDGER_FIELDS = frozenset({"date", "new_test_tus", "reason"})
PLACEHOLDER_REASON = "TODO"

# Dependency lines the closure census excluded from its counting: object files
# are the TUs themselves, and .cpp/.cc/.mm/.cu are sources, not headers.
DEPENDENCY_EXCLUDED_SUFFIXES = (".cpp", ".cc", ".mm", ".cu", ".o", ".obj")


@dataclass
class Verdict:
    violations: list[Violation] = field(default_factory=list)
    new_tus: int = 0
    claimed: int = 0
    touched_headers: list[str] = field(default_factory=list)


def _git_name_only(diff_args: list[str], statuses: str) -> list[str]:
    proc = subprocess.run(
        [
            "git",
            "-c",
            "core.quotePath=false",
            "diff",
            "--name-only",
            "--no-color",
            "-M",
            f"--diff-filter={statuses}",
            *diff_args,
        ],
        cwd=REPO_ROOT,
        capture_output=True,
        check=True,
    )
    return proc.stdout.decode("utf-8", errors="replace").splitlines()


# --- the ledger ---------------------------------------------------------------


def validate_ledger(data) -> tuple[list[tuple[str, int, str]], list[str]]:
    """Parse and fully validate a ledger document.

    Returns (entries as tuples, errors). Every run validates the whole file,
    not just what the diff touched: a malformed ledger is a broken audit
    record wherever it came from. An entry joins the returned list only when
    every field passes, so the set algebra downstream never sees a partial
    tuple standing in for a rejected one.
    """
    errors: list[str] = []
    if not isinstance(data, dict):
        return [], [f"{LEDGER_RELPATH}: top level must be an object"]
    if set(data) != {"version", "declarations"}:
        return [], [
            f"{LEDGER_RELPATH}: top level must hold exactly 'version' and "
            f"'declarations', got {sorted(data)}"
        ]
    if data["version"] != LEDGER_VERSION:
        return [], [
            f"{LEDGER_RELPATH}: version must be {LEDGER_VERSION}, got {data['version']!r}"
        ]
    declarations = data["declarations"]
    if not isinstance(declarations, list):
        return [], [f"{LEDGER_RELPATH}: 'declarations' must be a list"]

    entries: list[tuple[str, int, str]] = []
    for i, entry in enumerate(declarations):
        if not isinstance(entry, dict):
            errors.append(f"{LEDGER_RELPATH}: declaration #{i} must be an object")
            continue
        if set(entry) != LEDGER_FIELDS:
            errors.append(
                f"{LEDGER_RELPATH}: declaration #{i} must hold exactly "
                f"{sorted(LEDGER_FIELDS)}, got {sorted(entry)}"
            )
            continue
        date, tus, reason = entry["date"], entry["new_test_tus"], entry["reason"]
        entry_ok = True
        if not isinstance(date, str) or not date.strip():
            errors.append(f"{LEDGER_RELPATH}: declaration #{i} 'date' must be a non-empty string")
            entry_ok = False
        if isinstance(tus, bool) or not isinstance(tus, int) or tus < 1:
            errors.append(
                f"{LEDGER_RELPATH}: declaration #{i} 'new_test_tus' must be an integer >= 1"
            )
            entry_ok = False
        if not isinstance(reason, str) or not reason.strip():
            errors.append(
                f"{LEDGER_RELPATH}: declaration #{i} 'reason' must be a non-empty string "
                "saying why this PR adds the test mass it does"
            )
            entry_ok = False
        elif reason.strip() == PLACEHOLDER_REASON:
            errors.append(
                f"{LEDGER_RELPATH}: declaration #{i} 'reason' is the placeholder; write the "
                "real reason (which feature needs the tests and why they cannot be fewer)"
            )
            entry_ok = False
        if entry_ok:
            entries.append((date, tus, reason))

    deduplicated = list(dict.fromkeys(entries))
    if len(deduplicated) != len(entries):
        errors.append(f"{LEDGER_RELPATH}: duplicate declarations (identical date/tus/reason)")
    return deduplicated, errors


def load_ledger(ref: str) -> tuple[list[tuple[str, int, str]], list[str]]:
    """Ledger entries at `ref` (empty string = the index). Missing file = none."""
    text = blob(ref, LEDGER_RELPATH)
    if not text:
        return [], []
    try:
        data = json.loads(text)
    except json.JSONDecodeError as error:
        return [], [f"{LEDGER_RELPATH}: not valid JSON: {error}"]
    return validate_ledger(data)


def skeleton(new_tus: int, today: str) -> str:
    return json.dumps(
        {"date": today, "new_test_tus": new_tus, "reason": PLACEHOLDER_REASON},
        ensure_ascii=False,
    )


# --- the TU half ---------------------------------------------------------------


def check_tu_half(diff_args: list[str], content_ref: str, base_ref: str) -> Verdict:
    verdict = Verdict()

    added = [rel for rel in _git_name_only(diff_args, "A") if is_test_tu(rel)]
    verdict.new_tus = len(added)

    # The head version (index for --staged, head commit for --range) is what
    # this change would publish, so its schema is this change's business. The
    # base version's schema was already settled when it merged; it matters
    # here only as the substrate of the append-only comparison, so an
    # unreadable base is named rather than silently compared against a
    # partial picture.
    head_entries, head_errors = load_ledger(content_ref)
    base_entries, base_errors = load_ledger(base_ref)
    for error in head_errors:
        verdict.violations.append(Violation(REPO_ROOT / LEDGER_RELPATH, 1, RULE, error))
    for error in base_errors:
        verdict.violations.append(
            Violation(
                REPO_ROOT / LEDGER_RELPATH,
                1,
                RULE,
                f"base version unreadable, append-only check impossible: {error}",
            )
        )
    if head_errors or base_errors:
        return verdict

    head_set = set(head_entries)
    base_set = set(base_entries)

    vanished = base_set - head_set
    if vanished:
        listing = "; ".join(
            f"date={d!r} new_test_tus={n} reason={r!r}" for d, n, r in sorted(vanished)
        )
        verdict.violations.append(
            Violation(
                REPO_ROOT / LEDGER_RELPATH,
                1,
                RULE,
                "the ledger is append-only: this diff modified or deleted existing "
                f"declaration(s): {listing}. Existing entries are audit records of "
                "already-merged work and cannot be rewritten here.",
            )
        )

    new_entries = head_set - base_set
    verdict.claimed = sum(n for _d, n, _r in new_entries)
    if new_entries and verdict.claimed != verdict.new_tus:
        verdict.violations.append(
            Violation(
                REPO_ROOT / LEDGER_RELPATH,
                1,
                RULE,
                f"the entries this diff adds declare {verdict.claimed} new test TU(s) in "
                f"total, but the diff adds {verdict.new_tus}. The declared sum must equal "
                "the diff's actual added test TU count, computed from git — claim exactly "
                "what this PR adds, no more and no less.",
            )
        )

    if verdict.new_tus > FREE_NEW_TEST_TUS + verdict.claimed:
        today = datetime.date.today().isoformat()
        verdict.violations.append(
            Violation(
                REPO_ROOT / LEDGER_RELPATH,
                1,
                RULE,
                f"this diff adds {verdict.new_tus} test translation unit(s); the free quota "
                f"is {FREE_NEW_TEST_TUS} and declared entries cover {verdict.claimed}, leaving "
                f"{verdict.new_tus - FREE_NEW_TEST_TUS - verdict.claimed} undeclared. Add one "
                f"declaration per bulk addition to {LEDGER_RELPATH} with a real reason (the "
                "skeleton below pre-fills the count this diff adds; append to 'declarations', "
                f"never edit existing entries):\n    {skeleton(verdict.new_tus, today)}",
            )
        )
    return verdict


def diff_verdict(
    diff_args: list[str], content_ref: str | None, base_ref: str
) -> Verdict:
    """The diff-scoped verdict main() judges on, as a callable unit.

    This is the TU half ONLY: the hub-closure half is a CI-side notice fed by
    --deps, never a gate, and that "notices do not enter the verdict" boundary
    lives here — a caller running this function cannot accidentally gate on
    notices because it never sees them. scripts/test_inner.py's checker layer
    runs this same function instead of shelling out to this CLI or re-deriving
    which entry point takes which arguments — one seam per checker module,
    updated here when the internal shape moves.
    """
    return check_tu_half(diff_args, content_ref, base_ref)


# --- the hub half ---------------------------------------------------------------


class DepsParseError(ValueError):
    pass


def parse_deps(text: str, root_str: str) -> tuple[int, dict[str, int]]:
    """Aggregate `ninja -t deps` stdout into (TU count, header -> closure TUs).

    Line-for-line the census probe's parser, which was validated on the full
    real output of this repo's build: 4-space-indented lines are dependencies
    of the most recent target line; a target counts as a TU when its output
    ends in .o/.obj and is not a CMake "generated" output; a dependency counts
    toward a header's closure when it lives under <root>/src or <root>/test
    and is not an excluded (source/object) suffix.
    """
    closures: dict[str, int] = {}
    tu_count = 0
    root = root_str.rstrip("/")
    abs_prefixes = tuple(f"{root}/{p}" for p in HUB_SCOPE_PREFIXES)
    for line in text.splitlines():
        if line.startswith("    "):
            p = line.strip()
            if p.startswith(abs_prefixes):
                rel = p[len(root) + 1 :]
            elif p.startswith(HUB_SCOPE_PREFIXES):
                rel = p
            else:
                continue
            if rel.endswith(DEPENDENCY_EXCLUDED_SUFFIXES):
                continue
            if PurePosixPath(rel).suffix in HUB_HEADER_SUFFIXES:
                closures[rel] = closures.get(rel, 0) + 1
        else:
            out = line.split(":")[0].strip()
            if out.endswith((".o", ".obj")) and "generated" not in out:
                tu_count += 1
    if tu_count == 0:
        raise DepsParseError(
            "deps-parse-error: the --deps payload aggregates to 0 translation units. "
            "`ninja -t deps` output from a configured build always lists object targets; "
            "zero means the payload is empty, truncated or not ninja deps output. "
            "Failing loud rather than passing: a silently dead hub half has no red of "
            "its own to announce itself."
        )
    return tu_count, closures


def hub_notices(touched: list[str], closures: dict[str, int]) -> list[tuple[str, int]]:
    """(header, closure) pairs at or above the notice threshold, biggest first."""
    hits = [
        (rel, closures.get(rel, 0)) for rel in touched if closures.get(rel, 0) >= HUB_NOTICE_MIN_TUS
    ]
    return sorted(hits, key=lambda pair: (-pair[1], pair[0]))


def render_notices(notices: list[tuple[str, int]], tu_count: int | None) -> list[str]:
    lines = [
        f"::notice title=hub-header-closure::this PR touches {rel}, whose depfile "
        f"closure reaches {closure} translation units"
        for rel, closure in notices
    ]
    if tu_count is not None:
        lines.append(
            f"hub-header-closure: {len(notices)} touched header(s) at or above the "
            f"{HUB_NOTICE_MIN_TUS}-TU notice threshold (deps payload: {tu_count} TUs). "
            "Notice only — the compile cost of a hub touch is priced by the per-miss "
            "build budget, not here."
        )
    return lines


def append_summary(path: Path, notices: list[tuple[str, int]]) -> None:
    if not notices:
        return
    rows = "\n".join(f"| `{rel}` | {closure} |" for rel, closure in notices)
    with path.open("a", encoding="utf-8") as fh:
        fh.write(
            "### Test-TU hub-header closure notice\n\n"
            "Headers touched by this PR whose depfile closure reaches "
            f"{HUB_NOTICE_MIN_TUS} translation units (notice, not a gate):\n\n"
            f"| header | closure (TUs) |\n|---|---|\n{rows}\n"
        )


# --- main -----------------------------------------------------------------------


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    group = parser.add_mutually_exclusive_group()
    group.add_argument(
        "--staged",
        action="store_true",
        help="check the staged diff; this is what happens with no arguments too, "
        "so passing it is documentation rather than a switch",
    )
    group.add_argument("--range", dest="rng", metavar="BASE..HEAD", help="check a commit range")
    parser.add_argument(
        "--deps",
        type=Path,
        default=None,
        metavar="FILE",
        help="ninja -t deps stdout; enables the hub-closure notice half "
        "(without it the half is skipped with an audible line)",
    )
    parser.add_argument(
        "--summary",
        type=Path,
        default=None,
        metavar="FILE",
        help="append the hub notices to this file (a GitHub step summary) when any fire",
    )
    args = parser.parse_args()

    # Same range contract as the sibling gates: presence, not truthiness, and
    # the three-dot form rejected rather than mis-split.
    if args.rng is not None:
        if ".." not in args.rng:
            parser.error("--range expects BASE..HEAD")
        if "..." in args.rng:
            parser.error(
                "--range expects the two-dot form BASE..HEAD; for merge-base "
                "semantics pass `$(git merge-base A B)..B` explicitly"
            )
        base, head = args.rng.split("..", 1)
        if not base or not head:
            parser.error("--range expects BASE..HEAD")
        diff_args, content_ref, base_ref = [base, head], head, base
    else:
        diff_args, content_ref, base_ref = ["--cached"], "", "HEAD"

    verdict = diff_verdict(diff_args, content_ref, base_ref)

    notices: list[tuple[str, int]] = []
    deps_tus: int | None = None
    if args.deps is None:
        print(
            "hub-header-closure: skipped locally (no --deps); CI's judging leg "
            "is the authority for this half.",
            file=sys.stderr,
        )
    else:
        try:
            deps_text = args.deps.read_text(encoding="utf-8", errors="replace")
        except OSError as error:
            print(f"deps-parse-error: cannot read {args.deps}: {error}", file=sys.stderr)
            return 1
        try:
            deps_tus, closures = parse_deps(deps_text, str(REPO_ROOT))
        except DepsParseError as error:
            print(str(error), file=sys.stderr)
            return 1
        verdict.touched_headers = sorted(rel for rel in added_lines(diff_args) if is_hub_header(rel))
        notices = hub_notices(verdict.touched_headers, closures)

    if verdict.violations:
        print("New-test-TU cost check FAILED:\n", file=sys.stderr)
        for v in sorted(verdict.violations, key=lambda x: (str(x.path), x.line)):
            print(v.render(), file=sys.stderr)
        print(
            f"\n{len(verdict.violations)} violation(s). See the rule record in "
            "doc/testing/testing-architecture.md §7.9; the free quota is "
            f"{FREE_NEW_TEST_TUS} added test TUs per PR.",
            file=sys.stderr,
        )
        return 1

    for line in render_notices(notices, deps_tus):
        print(line)
    if notices and args.summary is not None:
        append_summary(args.summary, notices)

    detail = f"{verdict.new_tus} added test TU(s) scanned"
    if verdict.claimed:
        detail += f", {verdict.claimed} declared by this diff"
    print(f"New-test-TU cost check passed ({detail}).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
