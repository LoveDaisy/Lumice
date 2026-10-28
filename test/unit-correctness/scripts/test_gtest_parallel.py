"""Native gTest shard accounting and cancellation boundaries."""
from __future__ import annotations

import os
import signal
import subprocess
import sys
import time
import xml.etree.ElementTree as ET
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "scripts"))
import gtest_parallel as parallel  # noqa: E402


def report(path: Path, names: list[str]) -> None:
    root = ET.Element("testsuites", {"tests": str(len(names))})
    suite = ET.SubElement(root, "testsuite", {"name": "Sample", "tests": str(len(names))})
    for name in names:
        ET.SubElement(suite, "testcase", {
            "classname": "Sample", "name": name, "status": "run", "result": "completed", "time": "0.0",
        })
    ET.ElementTree(root).write(path)


def test_listing_keeps_parameterized_names_and_omits_disabled() -> None:
    listing = """Running main() from test_main.cpp
Prefix/Suite.
  Case/0  # GetParam() = 0
  DISABLED_Old/0
DISABLED_Whole.
  Case
"""
    assert parallel.selected_tests(listing) == {"Prefix/Suite.Case/0"}


def test_missing_shard_report_fails_instead_of_merging_partial(tmp_path: Path) -> None:
    first = tmp_path / "first.xml"
    report(first, ["One"])
    with pytest.raises(parallel.RunError, match="missing or malformed shard report"):
        parallel.merge_reports([first, tmp_path / "missing.xml"], {"Sample.One", "Sample.Two"})


def test_missing_case_fails_even_when_all_reports_exist(tmp_path: Path) -> None:
    first = tmp_path / "first.xml"
    report(first, ["One"])
    with pytest.raises(parallel.RunError, match="missing=.*Sample.Two"):
        parallel.merge_reports([first], {"Sample.One", "Sample.Two"})


@pytest.mark.parametrize("damage", ["root-count", "root-failure", "suite-count", "missing-time", "missing-status", "truncated"])
def test_malformed_report_is_not_publishable(tmp_path: Path, damage: str) -> None:
    path = tmp_path / "damaged.xml"
    report(path, ["One"])
    if damage == "truncated":
        path.write_text("<testsuites>")
    else:
        root = ET.parse(path).getroot()
        suite = root.find("testsuite")
        assert suite is not None
        case = suite.find("testcase")
        assert case is not None
        if damage == "root-count":
            root.set("tests", "2")
        elif damage == "root-failure":
            root.set("failures", "1")
        elif damage == "suite-count":
            suite.set("tests", "2")
        elif damage == "missing-time":
            case.attrib.pop("time")
        else:
            case.attrib.pop("status")
        ET.ElementTree(root).write(path)
    with pytest.raises(parallel.RunError):
        parallel.merge_reports([path], {"Sample.One"})


def test_duplicate_execution_refused(tmp_path: Path) -> None:
    first = tmp_path / "first.xml"
    second = tmp_path / "second.xml"
    report(first, ["One"])
    report(second, ["One"])
    with pytest.raises(parallel.RunError, match="duplicate executed case"):
        parallel.merge_reports([first, second], {"Sample.One"})


@pytest.mark.parametrize("external_filter", [False, True])
def test_failed_run_cannot_publish_listing_xml(tmp_path: Path, external_filter: bool) -> None:
    binary = tmp_path / "fake_gtest"
    binary.write_text(
        "#!/usr/bin/env python3\n"
        "import os, sys\n"
        "from pathlib import Path\n"
        "if '--gtest_list_tests' in sys.argv:\n"
        "    if 'GTEST_OUTPUT' in os.environ:\n"
        "        Path(os.environ['GTEST_OUTPUT'][4:]).write_text('<listing-only/>')\n"
        "    print('Sample.\\n  Case')\n"
        "else:\n"
        "    if 'GTEST_OUTPUT' in os.environ:\n"
        "        Path(os.environ['GTEST_OUTPUT'][4:]).write_text('<partial/>')\n"
        "    sys.exit(3)\n"
    )
    binary.chmod(0o755)
    output = tmp_path / "report.xml"
    output.write_text("old green")
    env = dict(os.environ, GTEST_OUTPUT=f"xml:{output}")
    if external_filter:
        env["GTEST_FILTER"] = "Sample.Case"
    proc = subprocess.run(
        [sys.executable, str(Path(parallel.__file__)), "unit", str(binary)],
        env=env, capture_output=True, text=True,
    )
    assert proc.returncode != 0
    assert not output.exists()


def test_cancellation_during_publish_removes_new_report(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    output = tmp_path / "published.xml"
    report_root = ET.Element("testsuites")
    children = parallel.Children()
    original_replace = Path.replace

    def interrupt_after_replace(source: Path, destination: Path) -> Path:
        moved = original_replace(source, destination)
        children.cancelled = True
        return moved

    monkeypatch.setattr(Path, "replace", interrupt_after_replace)
    with pytest.raises(parallel.RunError, match="cancelled during report publication"):
        parallel.publish_report(report_root, output, children)
    assert not output.exists()


def test_unknown_capacity_and_small_selection_stay_serial(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(parallel.platform, "system", lambda: "Linux")
    assert parallel.capacity("unit", 1000) == 1
    monkeypatch.setattr(parallel.platform, "system", lambda: "Darwin")
    monkeypatch.setattr(parallel.platform, "machine", lambda: "arm64")
    assert parallel.capacity("unit", 10) == 1


@pytest.mark.skipif(os.name == "nt", reason="POSIX SIGTERM and process liveness probe")
@pytest.mark.parametrize("during_listing", [True, False])
def test_cancel_reaps_owned_child(tmp_path: Path, during_listing: bool) -> None:
    marker = tmp_path / "child.pid"
    binary = tmp_path / "fake_gtest"
    binary.write_text(
        "#!/usr/bin/env python3\n"
        "import os, sys, time\n"
        "from pathlib import Path\n"
        f"if ('--gtest_list_tests' in sys.argv) != {during_listing!r}:\n"
        "    print('Sample.\\n  Case')\n"
        "    sys.exit(0)\n"
        f"Path({str(marker)!r}).write_text(str(os.getpid()))\n"
        "time.sleep(30)\n"
    )
    binary.chmod(0o755)
    output = tmp_path / "report.xml"
    output.write_text("stale")
    env = dict(os.environ, GTEST_OUTPUT=f"xml:{output}")
    runner = subprocess.Popen(
        [sys.executable, str(Path(parallel.__file__)), "unit", str(binary)],
        env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    child_pid = None
    try:
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline and not marker.exists() and runner.poll() is None:
            time.sleep(0.01)
        assert marker.exists(), runner.communicate(timeout=2)
        child_pid = int(marker.read_text())
        runner.send_signal(signal.SIGTERM)
        _, stderr = runner.communicate(timeout=8)
        assert runner.returncode != 0, stderr
        assert not output.exists()
        with pytest.raises(ProcessLookupError):
            os.kill(child_pid, 0)
    finally:
        if runner.poll() is None:
            runner.kill()
            runner.wait()
        if child_pid is not None:
            try:
                os.kill(child_pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
