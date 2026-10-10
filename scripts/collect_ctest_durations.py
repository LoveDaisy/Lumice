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


def _ctest_metadata(path: Path, report_dir: Path) -> tuple[set[str], dict[str, Path]]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ReportError(f"{path}: cannot read CTest metadata: {error}") from error
    tests = data.get("tests") if isinstance(data, dict) else None
    if not isinstance(tests, list):
        raise ReportError(f"{path}: CTest metadata must contain a tests list")

    names: set[str] = set()
    specs: dict[str, Path] = {}
    report_owners: dict[Path, str] = {}
    for index, test in enumerate(tests):
        where = f"{path}: tests[{index}]"
        if not isinstance(test, dict):
            raise ReportError(f"{where}: test must be an object")
        name = test.get("name")
        command = test.get("command")
        properties = test.get("properties", [])
        if not isinstance(name, str) or not name:
            raise ReportError(f"{where}: test name must be non-empty")
        if name in names:
            raise ReportError(f"{where}: duplicate CTest case {name}")
        if not isinstance(command, list) or not command or not isinstance(command[0], str):
            raise ReportError(f"{where}: command must start with an executable path")
        if not isinstance(properties, list):
            raise ReportError(f"{where}: properties must be a list")
        names.add(name)

        labels: list[str] = []
        for prop in properties:
            if isinstance(prop, dict) and prop.get("name") == "LABELS":
                labels = prop.get("value", [])
                break
        if not isinstance(labels, list) or any(not isinstance(label, str) for label in labels):
            raise ReportError(f"{where}: LABELS must be a list of strings")
        if "gtest-duration" not in labels:
            continue

        executable = Path(command[0]).stem
        report = report_dir / f"{executable}.xml"
        if report in report_owners:
            raise ReportError(
                f"{where}: gTest report {report} is shared with {report_owners[report]}; "
                "each invocation needs one report per CTest case"
            )
        report_owners[report] = name
        specs[name] = report
    return names, specs


def collect(ctest_junit: Path, metadata_names: set[str], gtest_specs: dict[str, Path]) -> dict[str, float]:
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

    missing = sorted(metadata_names - ctest_names)
    unexpected = sorted(ctest_names - metadata_names)
    if missing or unexpected:
        details = []
        if missing:
            details.append(f"metadata tests did not run: {', '.join(missing)}")
        if unexpected:
            details.append(f"run tests absent from metadata: {', '.join(unexpected)}")
        raise ReportError(f"{ctest_junit}: CTest metadata mismatch: {'; '.join(details)}")

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
    parser.add_argument("--ctest-metadata", type=Path, required=True)
    parser.add_argument("--gtest-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        if args.output.exists():
            raise ReportError(f"{args.output}: refusing to overwrite an existing report")
        metadata_names, gtest_specs = _ctest_metadata(args.ctest_metadata, args.gtest_dir)
        durations = collect(args.ctest_junit, metadata_names, gtest_specs)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(durations, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except (OSError, ReportError) as error:
        print(f"collect_ctest_durations: error: {error}", file=sys.stderr)
        return 1
    print(f"collect_ctest_durations: wrote {len(durations)} ids to {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
