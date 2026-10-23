#!/usr/bin/env python3
"""Collect this run's compiler-cache miss count for the per-miss build budget.

The build phase's registry budget (`test/duration_registry.json`, schema v4)
judges `seconds <= overhead_seconds + max_seconds_per_miss * misses`; this
script writes the `misses` half as a closed three-field report
`{"phase": ..., "tool": ..., "misses": ...}`. It must run after the build and
before the cache-statistics diagnostics (on Windows, before sccache's
`--stop-server`). The count it reads is the per-run increment, not a lifetime
total: the workflow zeros ccache (`ccache -z`) before Configure, and the
sccache server process starts fresh in every job.

The collector is strict on every leg — a tool failure, unparseable statistics
or a ccache internal error exits non-zero, because a miss count that cannot be
trusted is not a measurement. Whether that failure turns a leg red is decided
by the workflow, which scopes strictness to the leg whose registry entry
consumes the report. sccache `cache_write_errors` are the one counter that
does not fail collection: a failed cache write does not distort the miss count
(the compilation happened and was counted as a miss), so it is logged for
triage only.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

# Single parsing authority: the two stats shapes below are
# prepare_ci_compiler_cache's contract. This script imports its parsers and its
# subprocess runner rather than growing a second copy; the one piece it adds is
# the per-language miss total, which the seed audit does not consume.
from prepare_ci_compiler_cache import CacheReadinessError, _load_sccache_stats, _parse_ccache_stats, _run


def _sccache_misses(payload: dict) -> int:
    # sccache (v0.17.0, server.rs) serializes misses as a per-language count
    # object whose displayed total is counts.values().sum()
    # (PerLanguageCount::all); adv_counts keys the same events by compiler kind
    # and must not be summed in.
    misses = payload["stats"].get("cache_misses")
    if not isinstance(misses, dict) or not isinstance(misses.get("counts"), dict):
        raise CacheReadinessError("sccache statistics do not contain a per-language cache_misses object")
    total = 0
    for value in misses["counts"].values():
        if isinstance(value, bool) or not isinstance(value, int) or value < 0:
            raise CacheReadinessError("sccache per-language miss counts are not non-negative integers")
        total += value
    return total


def collect_misses(tool: str) -> int:
    if tool == "ccache":
        return _parse_ccache_stats(_run([tool, "--print-stats"]))["cache_miss"]
    payload = _load_sccache_stats(_run([tool, "--show-stats", "--stats-format=json"]))
    write_errors = payload["stats"]["cache_write_errors"]
    if write_errors != 0:
        print(
            f"collect_cache_stats: warning: sccache reported {write_errors} cache write error(s); "
            "write failures do not distort the miss count, but the cache should be inspected",
            file=sys.stderr,
        )
    return _sccache_misses(payload)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--tool", choices=("ccache", "sccache"), required=True)
    parser.add_argument("--phase", required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args(argv)

    if not args.phase.strip():
        parser.error("--phase must be non-empty")
    if args.report.exists():
        print(f"collect_cache_stats: error: {args.report}: refusing to overwrite an existing report", file=sys.stderr)
        return 1

    try:
        misses = collect_misses(args.tool)
    except CacheReadinessError as error:
        print(f"collect_cache_stats: error: {error}", file=sys.stderr)
        return 1

    payload = {"phase": args.phase, "tool": args.tool, "misses": misses}
    try:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except OSError as error:
        print(f"collect_cache_stats: error: cannot write {args.report}: {error}", file=sys.stderr)
        return 1
    print(f"collect_cache_stats: {args.phase} misses {misses} ({args.tool})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
