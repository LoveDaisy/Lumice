#!/usr/bin/env python3
"""Convert one CTest run and its gTest XML files into duration-registry input."""
from __future__ import annotations

import argparse
import json
import math
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


class ReportError(Exception):
    pass


def _seconds(value: str | None, where: str) -> float:
    try:
        seconds = float(value or "")
    except ValueError as error:
        raise ReportError(f"{where}: invalid time {value!r}") from error
    if not math.isfinite(seconds) or seconds < 0:
        raise ReportError(f"{where}: time must be finite and non-negative")
    return seconds


def _testcases(path: Path) -> list[ET.Element]:
    try:
        root = ET.parse(path).getroot()
    except (OSError, ET.ParseError) as error:
        raise ReportError(f"{path}: cannot read XML: {error}") from error
    cases = list(root.iter("testcase"))
    if not cases:
        raise ReportError(f"{path}: XML contains no test cases")
    return cases


def _gtest_specs(values: list[str]) -> dict[str, Path]:
    specs: dict[str, Path] = {}
    for value in values:
        name, separator, raw_path = value.partition("=")
        if not separator or not name or not raw_path:
            raise ReportError(f"invalid --gtest value {value!r}; expected CTEST_NAME=PATH")
        if name in specs:
            raise ReportError(f"duplicate --gtest mapping for {name}")
        specs[name] = Path(raw_path)
    return specs


def collect(ctest_junit: Path, gtest_specs: dict[str, Path]) -> dict[str, float]:
    ctest_cases = _testcases(ctest_junit)
    ctest_names: set[str] = set()
    durations: dict[str, float] = {}
    for case in ctest_cases:
        name = case.get("name", "")
        if not name:
            raise ReportError(f"{ctest_junit}: CTest case has no name")
        if name in ctest_names:
            raise ReportError(f"{ctest_junit}: duplicate CTest case {name}")
        ctest_names.add(name)
        if name in gtest_specs:
            continue
        durations[f"CTest::{name}"] = _seconds(case.get("time"), f"{ctest_junit}:{name}")

    missing = sorted(set(gtest_specs) - ctest_names)
    if missing:
        raise ReportError(f"{ctest_junit}: expected gTest CTest cases did not run: {', '.join(missing)}")

    for ctest_name, path in sorted(gtest_specs.items()):
        for case in _testcases(path):
            suite = case.get("classname", "")
            name = case.get("name", "")
            if not suite or not name:
                raise ReportError(f"{path}: gTest case must have classname and name")
            test_id = f"{ctest_name}::{suite}.{name}"
            if test_id in durations:
                raise ReportError(f"{path}: duplicate duration id {test_id}")
            durations[test_id] = _seconds(case.get("time"), f"{path}:{suite}.{name}")
    return durations


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ctest-junit", type=Path, required=True)
    parser.add_argument("--gtest", action="append", default=[], metavar="CTEST_NAME=PATH")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        if args.output.exists():
            raise ReportError(f"{args.output}: refusing to overwrite an existing report")
        durations = collect(args.ctest_junit, _gtest_specs(args.gtest))
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(durations, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except (OSError, ReportError) as error:
        print(f"collect_ctest_durations: error: {error}", file=sys.stderr)
        return 1
    print(f"collect_ctest_durations: wrote {len(durations)} ids to {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
