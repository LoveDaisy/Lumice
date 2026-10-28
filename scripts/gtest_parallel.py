#!/usr/bin/env python3
"""Run the two expensive gTest targets with native gTest sharding.

CTest and the direct inner path call this same entry. Selection and sharding remain
GoogleTest's: this module only limits concurrency, isolates timing assertions and
accounts for the resulting XML before publishing one complete report.
"""
from __future__ import annotations

import math
import os
import platform
import re
import signal
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

SENSITIVE = {
    "ServerAnalysisRunGpuPool.PoolIsUsedUnderGpuPreference",
    "DiagnosticCapi.DeviationStopsBeforeMaterializingTheUnprocessedTail",
}


class RunError(Exception):
    pass


class Children:
    def __init__(self) -> None:
        self.cancelled = False
        self.active: list[subprocess.Popen] = []

    def cancel(self, _signum: int, _frame: object) -> None:
        self.cancelled = True

    def start(
        self, argv: list[str], env: dict[str, str], output, cwd: Path | None = None
    ) -> subprocess.Popen:
        if self.cancelled:
            raise RunError("cancelled before starting a test process")
        child = subprocess.Popen(argv, env=env, cwd=cwd, stdout=output, stderr=subprocess.STDOUT)
        self.active.append(child)
        if self.cancelled:
            self.stop_all()
            raise RunError("cancelled while starting a test process")
        return child

    def wait(self, child: subprocess.Popen) -> int:
        while True:
            if self.cancelled:
                raise RunError("test invocation cancelled")
            try:
                return child.wait(timeout=0.2)
            except subprocess.TimeoutExpired:
                continue

    def stop_all(self) -> None:
        for child in self.active:
            if child.poll() is None:
                child.terminate()
        for child in self.active:
            if child.poll() is None:
                try:
                    child.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait()
        self.active.clear()


def selected_tests(listing: str) -> set[str]:
    suite = ""
    selected = set()
    for line in listing.splitlines():
        if not line or line.startswith("Running main()"):
            continue
        if not line[:1].isspace():
            if not line.rstrip().endswith("."):
                raise RunError(f"unrecognized gTest suite listing: {line!r}")
            suite = line.strip()[:-1]
        elif suite:
            case = line.split("#", 1)[0].strip()
            if not case:
                raise RunError("empty case in gTest listing")
            if "DISABLED_" not in suite and not case.startswith("DISABLED_"):
                identity = f"{suite}.{case}"
                if identity in selected:
                    raise RunError(f"duplicate gTest listing: {identity}")
                selected.add(identity)
        else:
            raise RunError("gTest case appeared without a suite")
    if not selected:
        raise RunError("gTest selection is empty; refusing a false green")
    return selected


def report_destination(binary: Path, env: dict[str, str]) -> Path | None:
    output = env.get("GTEST_OUTPUT", "")
    if not output:
        return None
    if not output.startswith("xml:"):
        return None
    path = output[4:]
    if not path:
        return None
    if path.endswith(("/", os.sep)):
        return Path(path) / f"{binary.stem}.xml"
    return Path(path)


def begin_report(binary: Path, env: dict[str, str], cwd: Path | None = None) -> Path | None:
    output = report_destination(binary, env)
    if "GTEST_OUTPUT" in env and output is None:
        raise RunError("nonstandard gTest report destination is unsupported; use GTEST_OUTPUT=xml:<path>")
    if output is not None:
        if cwd is not None and not output.is_absolute():
            output = cwd / output
        output.unlink(missing_ok=True)
    return output


def merge_reports(paths: list[Path], expected: set[str]) -> ET.Element:
    suites: dict[str, ET.Element] = {}
    measured: set[str] = set()
    for path in paths:
        try:
            root = ET.parse(path).getroot()
        except (OSError, ET.ParseError) as error:
            raise RunError(f"missing or malformed shard report {path}: {error}") from error
        if root.tag != "testsuites":
            raise RunError(f"{path}: expected gTest testsuites root")
        cases = list(root.iter("testcase"))
        if root.get("tests") != str(len(cases)) or root.get("failures", "0") != "0":
            raise RunError(f"{path}: declared test/failure count differs from successful XML cases")
        executed_in_shard = 0
        for source in root.findall("testsuite"):
            name = source.get("name", "")
            if not name:
                raise RunError(f"{path}: unnamed testsuite")
            suite_cases = source.findall("testcase")
            if source.get("tests") != str(len(suite_cases)):
                raise RunError(f"{path}: {name} declared test count differs from XML cases")
            suite = suites.setdefault(name, ET.Element("testsuite", dict(source.attrib)))
            for case in suite_cases:
                identity = f"{case.get('classname', '')}.{case.get('name', '')}"
                if not case.get("classname") or not case.get("name"):
                    raise RunError(f"{path}: testcase lacks classname or name")
                if case.get("status") not in ("run", "notrun") or case.get("result") not in (
                    "completed", "skipped", "suppressed"
                ):
                    raise RunError(f"{path}: {identity} lacks a native gTest status")
                try:
                    seconds = float(case.get("time", ""))
                except ValueError as error:
                    raise RunError(f"{path}: {identity} lacks a valid time") from error
                if not math.isfinite(seconds) or seconds < 0:
                    raise RunError(f"{path}: {identity} has a non-finite or negative time")
                if case.find("failure") is not None:
                    raise RunError(f"{path}: {identity} has a failure despite successful child exit")
                if case.get("status") == "notrun":
                    if not any(c.get("name") == case.get("name") for c in suite):
                        suite.append(case)
                    continue
                if identity in measured:
                    raise RunError(f"duplicate executed case: {identity}")
                measured.add(identity)
                executed_in_shard += 1
                for old in list(suite):
                    if old.get("name") == case.get("name") and old.get("status") == "notrun":
                        suite.remove(old)
                suite.append(case)
        if executed_in_shard == 0:
            raise RunError(f"{path}: empty shard report")
    if measured != expected:
        raise RunError(
            f"executed cases differ from selection: missing={sorted(expected - measured)[:3]}, "
            f"unexpected={sorted(measured - expected)[:3]}"
        )
    result = ET.Element("testsuites", {"name": "AllTests"})
    for name in sorted(suites):
        suite = suites[name]
        cases = list(suite)
        suite.attrib.update({
            "tests": str(len(cases)),
            "failures": str(sum(c.find("failure") is not None for c in cases)),
            "disabled": str(sum(c.get("status") == "notrun" for c in cases)),
            "skipped": str(sum(c.find("skipped") is not None or c.get("result") == "skipped" for c in cases)),
            "errors": "0",
            "time": str(sum(float(c.get("time", "0") or 0) for c in cases)),
        })
        result.append(suite)
    result.attrib.update({
        "tests": str(sum(int(s.get("tests", "0")) for s in result)),
        "failures": str(sum(int(s.get("failures", "0")) for s in result)),
        "disabled": str(sum(int(s.get("disabled", "0")) for s in result)),
        "errors": "0",
        "time": str(sum(float(s.get("time", "0")) for s in result)),
    })
    return result


def publish_report(result: ET.Element, output: Path | None, children: Children) -> None:
    if children.cancelled:
        raise RunError("cancelled before report publication")
    if output is None:
        return
    output.parent.mkdir(parents=True, exist_ok=True)
    temp_output = output.parent / f".{output.name}.{os.getpid()}.tmp"
    try:
        ET.ElementTree(result).write(temp_output, encoding="utf-8", xml_declaration=True)
        if children.cancelled:
            raise RunError("cancelled during report publication")
        temp_output.replace(output)
        if children.cancelled:
            output.unlink(missing_ok=True)
            raise RunError("cancelled during report publication")
    finally:
        temp_output.unlink(missing_ok=True)


def capacity(target: str, count: int) -> int:
    # Unknown hosts keep the original one-process behavior. The supported Mac
    # bounds leave capacity for the test's own worker threads and the OS.
    cores = os.cpu_count() or 0
    if platform.system() != "Darwin" or platform.machine() != "arm64" or count < 20:
        return 1
    probe = subprocess.run(
        ["sysctl", "-n", "hw.perflevel0.physicalcpu"], capture_output=True, text=True, check=False,
    )
    if probe.returncode or not probe.stdout.strip().isdigit():
        return 1
    performance_cores = min(cores, int(probe.stdout.strip()))
    if performance_cores < 8:
        return 1
    return min(count, 4 if target == "unit" else 8, max(1, performance_cores // (2 if target == "unit" else 1)))


def run(target: str, argv: list[str]) -> int:
    if target not in ("unit", "composition") or not argv:
        raise RunError("usage: gtest_parallel.py <unit|composition> <binary> [gTest/fixture arguments]")
    binary = Path(argv[0]).resolve()
    env = dict(os.environ)
    if any(a.startswith("--gtest_output") for a in argv[1:]):
        raise RunError("CLI gTest report destination is unsupported; use GTEST_OUTPUT=xml:<path>")
    output = begin_report(binary, env)
    # Do not duplicate GoogleTest's CLI/env parser. Non-default selection or
    # external sharding is delegated to one original process, not reinterpreted.
    filters = [a.split("=", 1)[1] for a in argv[1:] if a.startswith("--gtest_filter=")]
    unsupported = (
        len(filters) > 1 or any(a.startswith("--gtest_") and not a.startswith("--gtest_filter=") for a in argv[1:])
        or any(key.startswith("GTEST_") and key not in {"GTEST_OUTPUT", "GTEST_COLOR"} for key in env)
    )
    children = Children()
    old_handlers = {sig: signal.getsignal(sig) for sig in (signal.SIGINT, signal.SIGTERM)}
    for sig in old_handlers:
        signal.signal(sig, children.cancel)
    try:
        with tempfile.TemporaryDirectory(prefix="lumice-gtest-") as tmp:
            base = Path(tmp)
            if unsupported:
                stdout = base / "native.txt"
                report = base / "native.xml"
                with stdout.open("wb") as sink:
                    child = children.start(argv, dict(env, GTEST_OUTPUT=f"xml:{report}"), sink)
                    rc = children.wait(child)
                text = stdout.read_text(errors="replace")
                sys.stdout.write(text)
                sys.stdout.flush()
                ran = re.findall(r"\[==========\] (\d+) tests? from .* ran\.", text)
                if rc or children.cancelled or not ran or int(ran[-1]) == 0:
                    raise RunError(f"native gTest did not complete a nonempty selection (exit {rc})")
                try:
                    root = ET.parse(report).getroot()
                except (OSError, ET.ParseError) as error:
                    raise RunError(f"native gTest report incomplete: {error}") from error
                expected = {
                    f"{case.get('classname', '')}.{case.get('name', '')}"
                    for case in root.iter("testcase") if case.get("status") != "notrun"
                }
                if len(expected) != int(ran[-1]):
                    raise RunError("native gTest report does not match its completed run count")
                publish_report(merge_reports([report], expected), output, children)
                return 0
            listing_output = base / "listing.txt"
            listing_env = dict(env)
            listing_env.pop("GTEST_OUTPUT", None)
            with listing_output.open("wb") as sink:
                child = children.start([*argv, "--gtest_list_tests"], listing_env, sink)
                rc = children.wait(child)
            if rc:
                raise RunError(f"gTest listing exited {rc}")
            expected = selected_tests(listing_output.read_text(errors="replace"))
            k = capacity(target, len(expected))
            print(f"gtest_parallel: target={target} selected={len(expected)} shards={k} "
                  f"serial={len(expected & SENSITIVE)}", flush=True)
            sensitive = expected & SENSITIVE if k > 1 else set()
            filter_str = filters[0] if filters else "*"
            positive = filter_str.split("-", 1)[0]
            negative = filter_str.split("-", 1)[1] if "-" in filter_str else ""
            if sensitive:
                negative = ":".join(part for part in [negative, *sorted(sensitive)] if part)
            parallel_filter = positive + (f"-{negative}" if negative else "")
            base_argv = [a for a in argv if not a.startswith("--gtest_filter=")]
            processes = []
            reports = []
            shards = range(k) if k > 1 else range(1)
            for i in shards:
                report = base / f"shard-{i}.xml"
                stdout = base / f"shard-{i}.txt"
                child_env = dict(env, GTEST_OUTPUT=f"xml:{report}")
                child_env.pop("GTEST_TOTAL_SHARDS", None)
                child_env.pop("GTEST_SHARD_INDEX", None)
                if k > 1:
                    child_env.update(GTEST_TOTAL_SHARDS=str(k), GTEST_SHARD_INDEX=str(i))
                with stdout.open("wb") as sink:
                    child = children.start([*base_argv, f"--gtest_filter={parallel_filter}"], child_env, sink)
                processes.append((child, stdout))
                reports.append(report)
            for child, stdout in processes:
                rc = children.wait(child)
                sys.stdout.write(stdout.read_text(errors="replace"))
                sys.stdout.flush()
                if rc:
                    raise RunError(f"gTest shard exited {rc}")
            if sensitive:
                report = base / "serial.xml"
                stdout = base / "serial.txt"
                with stdout.open("wb") as sink:
                    child = children.start(
                        [*base_argv, f"--gtest_filter={':'.join(sorted(sensitive))}"],
                        dict(env, GTEST_OUTPUT=f"xml:{report}"), sink,
                    )
                    rc = children.wait(child)
                sys.stdout.write(stdout.read_text(errors="replace"))
                if rc:
                    raise RunError(f"isolated gTest exited {rc}")
                reports.append(report)
            result = merge_reports(reports, expected)
            publish_report(result, output, children)
            return 0
    finally:
        children.stop_all()
        for sig, handler in old_handlers.items():
            signal.signal(sig, handler)


def main() -> int:
    try:
        return run(sys.argv[1] if len(sys.argv) > 1 else "", sys.argv[2:])
    except (OSError, ET.ParseError, RunError, ValueError) as error:
        print(f"gtest_parallel: FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
