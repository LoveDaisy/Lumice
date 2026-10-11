#!/usr/bin/env python3
"""Verify that a compiler cache is complete enough to publish as a CI seed."""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

METADATA_NAMES = {
    ".gitignore",
    "CACHEDIR.TAG",
    "ccache.conf",
    "stats",
    "stats.lock",
}


class CacheReadinessError(Exception):
    pass


def _run(command: list[str]) -> str:
    try:
        result = subprocess.run(command, capture_output=True, text=True, check=False)
    except OSError as error:
        raise CacheReadinessError(f"cannot run {command[0]!r}: {error}") from error
    if result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip() or "no output"
        raise CacheReadinessError(f"{' '.join(command)} exited with {result.returncode}: {detail}")
    return result.stdout.strip()


def _parse_ccache_stats(raw: str) -> dict[str, int]:
    stats: dict[str, int] = {}
    for line in raw.splitlines():
        parts = line.split("\t")
        if len(parts) != 2 or not parts[0] or parts[0] in stats:
            raise CacheReadinessError("ccache --print-stats returned an unknown shape")
        try:
            stats[parts[0]] = int(parts[1])
        except ValueError as error:
            raise CacheReadinessError(f"ccache statistic {parts[0]!r} is not an integer") from error
    required = {"internal_error", "cache_miss", "local_storage_write"}
    missing = required - stats.keys()
    if missing:
        raise CacheReadinessError(f"ccache statistics are missing: {', '.join(sorted(missing))}")
    if stats["internal_error"] != 0:
        raise CacheReadinessError(f"ccache reported {stats['internal_error']} internal error(s)")
    # A direct-mode miss writes one result and one manifest before returning.
    minimum_writes = 2 * stats["cache_miss"]
    if stats["local_storage_write"] < minimum_writes:
        raise CacheReadinessError(
            f"ccache reported {stats['cache_miss']} miss(es) but only "
            f"{stats['local_storage_write']} local write(s); expected at least {minimum_writes}"
        )
    return stats


def _parse_sccache_stats(raw: str) -> dict:
    try:
        payload = json.loads(raw)
    except json.JSONDecodeError as error:
        raise CacheReadinessError(f"sccache returned invalid JSON statistics: {error}") from error
    if not isinstance(payload, dict) or not isinstance(payload.get("stats"), dict):
        raise CacheReadinessError("sccache statistics do not contain a stats object")
    write_errors = payload["stats"].get("cache_write_errors")
    if isinstance(write_errors, bool) or not isinstance(write_errors, int):
        raise CacheReadinessError("sccache statistics do not contain an integer cache_write_errors")
    if write_errors != 0:
        raise CacheReadinessError(f"sccache reported {write_errors} cache write error(s)")
    return payload


def collect_stats(tool: str) -> tuple[str, dict]:
    version = _run([tool, "--version"])
    if tool == "ccache":
        return version, _parse_ccache_stats(_run([tool, "--print-stats"]))
    return version, _parse_sccache_stats(_run([tool, "--show-stats", "--stats-format=json"]))


def scan_entries(cache_dir: Path) -> tuple[int, int]:
    if not cache_dir.is_dir():
        raise CacheReadinessError(f"cache directory does not exist: {cache_dir}")
    count = 0
    total_bytes = 0
    try:
        for path in cache_dir.rglob("*"):
            if not path.is_file() or path.name in METADATA_NAMES:
                continue
            size = path.stat().st_size
            if size <= 0:
                continue
            count += 1
            total_bytes += size
    except OSError as error:
        raise CacheReadinessError(f"cannot inspect cache directory {cache_dir}: {error}") from error
    if count == 0:
        raise CacheReadinessError(f"cache directory contains no stored compiler entries: {cache_dir}")
    return count, total_bytes


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tool", choices=("ccache", "sccache"), required=True)
    parser.add_argument("--cache-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args(argv)

    if args.report.exists():
        print(f"prepare_ci_compiler_cache: error: {args.report}: refusing to overwrite an existing report", file=sys.stderr)
        return 1

    report = {
        "tool": args.tool,
        "cache_dir": str(args.cache_dir),
        "ready": False,
        "version": None,
        "stats": None,
        "entry_files": 0,
        "entry_bytes": 0,
        "error": None,
    }
    try:
        report["version"], report["stats"] = collect_stats(args.tool)
        report["entry_files"], report["entry_bytes"] = scan_entries(args.cache_dir)
        report["ready"] = True
    except CacheReadinessError as error:
        report["error"] = str(error)

    try:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except OSError as error:
        print(f"prepare_ci_compiler_cache: error: cannot write {args.report}: {error}", file=sys.stderr)
        return 1

    if not report["ready"]:
        print(f"prepare_ci_compiler_cache: error: {report['error']}", file=sys.stderr)
        return 1
    print(
        f"prepare_ci_compiler_cache: ready ({report['entry_files']} files, "
        f"{report['entry_bytes']} bytes, {report['version'].splitlines()[0]})"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
