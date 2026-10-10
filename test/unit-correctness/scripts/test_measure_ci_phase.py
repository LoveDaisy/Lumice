from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

SCRIPTS = Path(__file__).resolve().parents[3] / "scripts"
MEASURE = SCRIPTS / "measure_ci_phase.py"


def run_measure(report: Path, phase: str, command: list[str]):
    return subprocess.run(
        [sys.executable, str(MEASURE), "--report", str(report), "--phase", phase, "--", *command],
        capture_output=True,
        text=True,
        check=False,
    )


def test_records_wall_clock_and_propagates_success(tmp_path):
    report = tmp_path / "run one" / "phase.json"
    result = run_measure(report, "ctest", [sys.executable, "-c", "print('live output')"])
    assert result.returncode == 0
    assert "live output" in result.stdout
    payload = json.loads(report.read_text(encoding="utf-8"))
    assert payload["phase"] == "ctest"
    assert payload["exit_code"] == 0
    assert payload["seconds"] >= 0


def test_records_and_propagates_command_failure(tmp_path):
    report = tmp_path / "failed.json"
    result = run_measure(report, "tests", [sys.executable, "-c", "raise SystemExit(7)"])
    assert result.returncode == 7
    payload = json.loads(report.read_text(encoding="utf-8"))
    assert payload["exit_code"] == 7


def test_missing_command_is_red_without_report(tmp_path):
    report = tmp_path / "missing.json"
    result = run_measure(report, "build", [str(tmp_path / "does not exist")])
    assert result.returncode == 127
    assert not report.exists()


def test_refuses_to_overwrite_an_invocation_report(tmp_path):
    report = tmp_path / "phase.json"
    first = run_measure(report, "one", [sys.executable, "-c", "pass"])
    second = run_measure(report, "two", [sys.executable, "-c", "pass"])
    assert first.returncode == 0
    assert second.returncode == 1
    assert json.loads(report.read_text(encoding="utf-8"))["phase"] == "one"
