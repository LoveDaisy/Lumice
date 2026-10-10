#!/usr/bin/env python3
"""Run one CI phase, stream its output, and write its wall-clock duration."""
from __future__ import annotations

import argparse
import json
import math
import subprocess
import sys
import time
from pathlib import Path


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--phase", required=True)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args(argv)
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    if not command:
        parser.error("a command is required after --")
    if not args.phase.strip():
        parser.error("--phase must be non-empty")
    if args.report.exists():
        print(f"measure_ci_phase: error: {args.report}: refusing to overwrite an existing report", file=sys.stderr)
        return 1

    start = time.monotonic()
    try:
        result = subprocess.run(command, check=False)
    except OSError as error:
        print(f"measure_ci_phase: error: cannot start {command[0]!r}: {error}", file=sys.stderr)
        return 127
    seconds = time.monotonic() - start
    if not math.isfinite(seconds) or seconds < 0:
        print("measure_ci_phase: error: monotonic clock produced an invalid duration", file=sys.stderr)
        return 1
    payload = {"phase": args.phase, "seconds": seconds, "exit_code": result.returncode}
    try:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except OSError as error:
        print(f"measure_ci_phase: error: cannot write {args.report}: {error}", file=sys.stderr)
        return 1
    print(f"measure_ci_phase: {args.phase} took {seconds:.3f}s (exit {result.returncode})")
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
