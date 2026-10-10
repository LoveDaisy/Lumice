from __future__ import annotations

import json
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


def write_metadata(path: Path, tests: list[tuple[str, str, bool]]) -> Path:
    payload = {
        "kind": "ctestInfo",
        "tests": [
            {
                "name": name,
                "command": [f"/bin/{executable}"],
                "properties": [
                    {
                        "name": "LABELS",
                        "value": ["unit-correctness", *(("gtest-duration",) if is_gtest else ())],
                    }
                ],
            }
            for name, executable, is_gtest in tests
        ],
        "version": {"major": 1, "minor": 0},
    }
    path.write_text(json.dumps(payload), encoding="utf-8")
    return path


def test_collects_parameterized_gtest_cases_and_plain_ctest_cases(tmp_path):
    ctest = write_xml(
        tmp_path / "ctest.xml",
        [
            {"name": "LumiceUnitCorrectnessTest", "classname": "ctest", "time": "3.5"},
            {"name": "LumiceCapiHeaderSelfContainment", "classname": "ctest", "time": "0.2"},
        ],
    )
    metadata = write_metadata(
        tmp_path / "metadata.json",
        [
            ("LumiceUnitCorrectnessTest", "unit_correctness_test", True),
            ("LumiceCapiHeaderSelfContainment", "capi_header_self_containment_test", False),
        ],
    )
    report_dir = tmp_path / "gtest"
    report_dir.mkdir()
    write_xml(
        report_dir / "unit_correctness_test.xml",
        [
            {"name": "Handles/0", "classname": "TypedSuite/Int", "time": "1.25"},
            {"name": "Skipped", "classname": "Suite", "time": "0", "status": "notrun"},
        ],
    )
    names, specs = collector._ctest_metadata(metadata, report_dir)
    assert collector.collect(ctest, names, specs) == {
        "LumiceUnitCorrectnessTest::TypedSuite/Int.Handles/0": 1.25,
        "LumiceUnitCorrectnessTest::Suite.Skipped": 0.0,
        "CTest::LumiceCapiHeaderSelfContainment": 0.2,
    }


@pytest.mark.parametrize(
    "kind",
    [
        "missing-gtest",
        "empty-ctest",
        "bad-xml",
        "duplicate-ctest",
        "duplicate-gtest",
        "nonfinite",
        "negative",
        "metadata-missing-run",
        "run-missing-metadata",
    ],
)
def test_rejects_incomplete_or_invalid_reports(tmp_path, kind):
    ctest_cases = [{"name": "Unit", "classname": "ctest", "time": "1"}]
    gtest_cases = [{"name": "Case", "classname": "Suite", "time": "1"}]
    metadata_tests = [("Unit", "unit_test", True)]
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
    elif kind == "metadata-missing-run":
        metadata_tests.append(("Other", "other_test", False))
    elif kind == "run-missing-metadata":
        ctest_cases.append({"name": "Other", "classname": "ctest", "time": "1"})
    ctest = write_xml(tmp_path / "ctest.xml", ctest_cases)
    metadata = write_metadata(tmp_path / "metadata.json", metadata_tests)
    report_dir = tmp_path / "gtest"
    report_dir.mkdir()
    gtest = report_dir / "unit_test.xml"
    if kind != "missing-gtest":
        write_xml(gtest, gtest_cases)
    if kind == "bad-xml":
        gtest.write_text("<broken", encoding="utf-8")
    names, specs = collector._ctest_metadata(metadata, report_dir)
    with pytest.raises(collector.ReportError):
        collector.collect(ctest, names, specs)


def test_rejects_two_ctest_cases_sharing_one_gtest_report(tmp_path):
    metadata = write_metadata(
        tmp_path / "metadata.json",
        [("UnitA", "unit_test", True), ("UnitB", "unit_test", True)],
    )
    with pytest.raises(collector.ReportError, match="is shared with UnitA"):
        collector._ctest_metadata(metadata, tmp_path / "gtest")


def test_cli_refuses_to_overwrite_a_report(tmp_path):
    ctest = write_xml(tmp_path / "ctest.xml", [{"name": "Plain", "classname": "ctest", "time": "0.1"}])
    metadata = write_metadata(tmp_path / "metadata.json", [("Plain", "plain_test", False)])
    report_dir = tmp_path / "gtest"
    report_dir.mkdir()
    output = tmp_path / "durations.json"
    args = [
        "--ctest-junit",
        str(ctest),
        "--ctest-metadata",
        str(metadata),
        "--gtest-dir",
        str(report_dir),
        "--output",
        str(output),
    ]
    assert collector.main(args) == 0
    original = output.read_text(encoding="utf-8")
    assert collector.main(args) == 1
    assert output.read_text(encoding="utf-8") == original
