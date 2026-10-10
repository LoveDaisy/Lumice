from __future__ import annotations

import json
import math
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

import pytest

SCRIPTS = Path(__file__).resolve().parents[3] / "scripts"
sys.path.insert(0, str(SCRIPTS))

import collect_ctest_durations as collector  # noqa: E402


def write_xml(path: Path, cases: list[dict[str, str]]) -> Path:
    root = ET.Element("testsuites")
    suite = ET.SubElement(root, "testsuite", name="suite")
    for fields in cases:
        ET.SubElement(suite, "testcase", **fields)
    ET.ElementTree(root).write(path, encoding="utf-8", xml_declaration=True)
    return path


def test_collects_parameterized_gtest_cases_and_plain_ctest_cases(tmp_path):
    ctest = write_xml(
        tmp_path / "ctest.xml",
        [
            {"name": "LumiceUnitCorrectnessTest", "classname": "ctest", "time": "3.5"},
            {"name": "LumiceCapiHeaderSelfContainment", "classname": "ctest", "time": "0.2"},
        ],
    )
    gtest = write_xml(
        tmp_path / "unit.xml",
        [
            {"name": "Handles/0", "classname": "TypedSuite/Int", "time": "1.25"},
            {"name": "Skipped", "classname": "Suite", "time": "0", "status": "notrun"},
        ],
    )
    assert collector.collect(ctest, {"LumiceUnitCorrectnessTest": gtest}) == {
        "LumiceUnitCorrectnessTest::TypedSuite/Int.Handles/0": 1.25,
        "LumiceUnitCorrectnessTest::Suite.Skipped": 0.0,
        "CTest::LumiceCapiHeaderSelfContainment": 0.2,
    }


@pytest.mark.parametrize(
    "kind",
    ["missing-gtest", "empty-ctest", "bad-xml", "duplicate-ctest", "duplicate-gtest", "nonfinite", "negative"],
)
def test_rejects_incomplete_or_invalid_reports(tmp_path, kind):
    ctest_cases = [{"name": "Unit", "classname": "ctest", "time": "1"}]
    gtest_cases = [{"name": "Case", "classname": "Suite", "time": "1"}]
    if kind == "empty-ctest":
        ctest_cases = []
    elif kind == "duplicate-ctest":
        ctest_cases *= 2
    elif kind == "duplicate-gtest":
        gtest_cases *= 2
    elif kind == "nonfinite":
        gtest_cases[0]["time"] = "nan"
    elif kind == "negative":
        gtest_cases[0]["time"] = "-1"
    ctest = write_xml(tmp_path / "ctest.xml", ctest_cases)
    gtest = write_xml(tmp_path / "gtest.xml", gtest_cases)
    if kind == "bad-xml":
        gtest.write_text("<broken", encoding="utf-8")
    specs = {"Missing": gtest} if kind == "missing-gtest" else {"Unit": gtest}
    with pytest.raises(collector.ReportError):
        collector.collect(ctest, specs)


def test_cli_refuses_to_overwrite_a_report(tmp_path):
    ctest = write_xml(tmp_path / "ctest.xml", [{"name": "Plain", "classname": "ctest", "time": "0.1"}])
    output = tmp_path / "durations.json"
    assert collector.main(["--ctest-junit", str(ctest), "--output", str(output)]) == 0
    original = output.read_text(encoding="utf-8")
    assert collector.main(["--ctest-junit", str(ctest), "--output", str(output)]) == 1
    assert output.read_text(encoding="utf-8") == original
