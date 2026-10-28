"""The inner loop removes only registered executions contained in a parent."""
from __future__ import annotations

import json
import os
import re
import signal
import subprocess
import sys
import time
from pathlib import Path
from unittest.mock import patch

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "scripts"))
import test_inner  # noqa: E402


def plan(*files: str) -> dict:
    result, _ = test_inner.derive(list(files), "main", False)
    return result


def test_mixed_faces_keep_original_selection_but_execute_parent_once() -> None:
    result = plan(
        "test/fixtures/v3_config.json",
        "test/composition-correctness/raypath/test_input_assembly_chain.cpp",
        "test/unit-correctness/analytic/test_band_sum.cpp",
    )
    assert {"ctest", "composition-gtest", "unit-gtest", "fast-e2e"} <= set(result["requested_layers"])
    assert "ctest" in result["layers"]
    assert "composition-gtest" not in result["layers"]
    assert "unit-gtest" not in result["layers"]
    assert result["gtest_filter"]
    assert result["covered"]["unit-gtest"].startswith("ctest ")


def test_unit_label_contains_direct_unit_but_does_not_remove_e2e() -> None:
    result = plan("src/main.cpp", "test/unit-correctness/analytic/test_band_sum.cpp")
    assert "ctest-unit-correctness" in result["layers"]
    assert "unit-gtest" in result["requested_layers"]
    assert "unit-gtest" not in result["layers"]
    assert "fast-e2e" in result["layers"]


def test_removed_unit_execution_keeps_its_per_suite_guard(monkeypatch: pytest.MonkeyPatch) -> None:
    result = plan("src/main.cpp", "test/unit-correctness/analytic/test_band_sum.cpp")
    repo = test_inner.REPO
    working = repo / test_inner.BUILD_TREE / "test"
    unit = {
        "name": "LumiceUnitCorrectnessTest",
        "command": [
            sys.executable, str(repo / "scripts/gtest_parallel.py"), "unit",
            str(repo / "build/Release/static/bin/unit_correctness_test"),
            str(repo / "test/fixtures/v3_config.json"), str(working), str(repo),
        ],
        "properties": [
            {"name": "LABELS", "value": ["unit-correctness", "gtest-duration"]},
            {"name": "WORKING_DIRECTORY", "value": str(working)},
        ],
    }

    def ctest(argv: list[str], **_kwargs: object):
        selected = [unit] if "-L" in argv or argv[-1] == "^LumiceUnitCorrectnessTest$" else []
        return type("Result", (), {"returncode": 0, "stderr": "", "stdout": json.dumps({"tests": selected})})()

    monkeypatch.setattr(test_inner.subprocess, "run", ctest)
    monkeypatch.setattr(test_inner, "reconcile", lambda *_args: ["ReviewUnregisteredSuite"])
    with pytest.raises(RuntimeError, match="declared suites absent.*ReviewUnregisteredSuite"):
        test_inner.validate_coverage(result)
    monkeypatch.setattr(test_inner, "reconcile", lambda *_args: [])
    test_inner.validate_coverage(result)


def test_gtest_environment_filter_prevents_unit_containment() -> None:
    with patch.dict(test_inner.os.environ, {"GTEST_FILTER": "OtherSuite.*"}):
        result = plan("src/main.cpp", "test/unit-correctness/analytic/test_band_sum.cpp")
    assert "unit-gtest" in result["layers"]
    assert "unit-gtest" not in result["covered"]


def test_report_environment_prevents_direct_unit_containment() -> None:
    with patch.dict(test_inner.os.environ, {"GTEST_OUTPUT": "xml:/tmp/report/"}):
        result = plan("src/main.cpp", "test/unit-correctness/analytic/test_band_sum.cpp")
    assert "unit-gtest" in result["layers"]
    assert "ctest-unit-correctness" in result["layers"]


def test_kernel_parity_is_not_repeated_after_full_ctest() -> None:
    with patch.object(test_inner, "load_layer_map", return_value={"src/core/shared/kernel.hpp": "sim"}):
        result = plan("src/core/shared/kernel.hpp")
    assert "parity" in result["requested_layers"]
    assert "parity" not in result["layers"]


@pytest.mark.skipif(os.name == "nt", reason="POSIX SIGTERM and PID liveness probe")
def test_cancel_during_suite_reconciliation_reaps_listing_child(tmp_path: Path) -> None:
    working_dir = tmp_path / "build/cmake_build/static/test"
    working_dir.mkdir(parents=True)
    marker = tmp_path / "listing.pid"
    binary = tmp_path / "slow_gtest"
    binary.write_text(
        "#!/usr/bin/env python3\n"
        "import os, time\n"
        "from pathlib import Path\n"
        f"Path({str(marker)!r}).write_text(str(os.getpid()))\n"
        "time.sleep(30)\n"
    )
    binary.chmod(0o755)
    script = (
        "import sys; from pathlib import Path; "
        f"sys.path.insert(0, {str(Path(test_inner.__file__).parent)!r}); "
        "import test_inner; "
        f"test_inner.REPO = Path({str(tmp_path)!r}); "
        f"test_inner.selected_suite_headers(Path({str(binary)!r}), 'Sample.*')"
    )
    outer = subprocess.Popen([sys.executable, "-c", script], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    child_pid = None
    try:
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline and not marker.exists() and outer.poll() is None:
            time.sleep(0.01)
        assert marker.exists(), outer.communicate(timeout=2)
        child_pid = int(marker.read_text())
        outer.send_signal(signal.SIGTERM)
        _, stderr = outer.communicate(timeout=8)
        assert outer.returncode != 0, stderr
        with pytest.raises(ProcessLookupError):
            os.kill(child_pid, 0)
    finally:
        if outer.poll() is None:
            outer.kill()
            outer.wait()
        if child_pid is not None:
            try:
                os.kill(child_pid, signal.SIGKILL)
            except ProcessLookupError:
                pass


@pytest.mark.skipif(os.name == "nt", reason="POSIX shebang stand-in executable")
@pytest.mark.parametrize("listing_fails", [False, True])
def test_direct_preflight_failure_clears_old_xml_and_cannot_publish_listing(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, listing_fails: bool,
) -> None:
    monkeypatch.setattr(test_inner, "REPO", tmp_path)
    build_dir = tmp_path / test_inner.BUILD_TREE
    (build_dir / "test").mkdir(parents=True)
    (build_dir / "CMakeCache.txt").write_text("CMAKE_BUILD_TYPE:STRING=Release\n")
    binary = tmp_path / "build/Release/static/bin/unit_correctness_test"
    binary.parent.mkdir(parents=True)
    report = tmp_path / "previous.xml"
    binary.write_text(
        "#!/usr/bin/env python3\n"
        "import os, sys\n"
        "from pathlib import Path\n"
        "if 'GTEST_OUTPUT' in os.environ:\n"
        "    Path(os.environ['GTEST_OUTPUT'][4:]).write_text('<listing-only/>')\n"
        f"print('Known.\\n  Case')\n"
        f"sys.exit({7 if listing_fails else 0})\n"
    )
    binary.chmod(0o755)
    plan_file = tmp_path / "plan.json"
    plan_file.write_text(json.dumps({
        "gtest_filter": "Missing.*", "gtest_suites": {"plain": ["Missing"], "param": {}},
    }))
    report.write_text("<old-green/>")
    with patch.dict(os.environ, {"GTEST_OUTPUT": f"xml:{report}"}):
        if listing_fails:
            with pytest.raises(RuntimeError, match="--gtest_list_tests failed"):
                test_inner.cmd_gtest(type("Args", (), {"plan_file": str(plan_file), "target": "unit"})())
        else:
            assert test_inner.cmd_gtest(type("Args", (), {"plan_file": str(plan_file), "target": "unit"})()) == 1
    assert not report.exists()


def test_runtime_containment_checks_the_original_unanchored_selector(monkeypatch: pytest.MonkeyPatch) -> None:
    with patch.object(test_inner, "load_layer_map", return_value={"src/core/shared/kernel.hpp": "sim"}):
        result = plan("src/core/shared/kernel.hpp")
    registered = [
        {"name": "LumiceParityTest", "command": ["bin"], "properties": []},
        {"name": "ExtraLumiceParityTest", "command": ["other"], "properties": []},
    ]

    def ctest(argv: list[str], **_kwargs: object):
        if "-R" in argv:
            selected = [test for test in registered if re.search(argv[-1], test["name"])]
        else:
            selected = registered[:1]  # the second registration has no parity label
        return type("Result", (), {"returncode": 0, "stderr": "", "stdout": json.dumps({"tests": selected})})()

    monkeypatch.setattr(test_inner.subprocess, "run", ctest)
    with pytest.raises(RuntimeError, match="parity: CTest registration no longer contained"):
        test_inner.validate_coverage(result)


def test_registration_change_refuses_containment(monkeypatch: pytest.MonkeyPatch) -> None:
    result = plan("test/fixtures/v3_config.json", "test/composition-correctness/raypath/test_input_assembly_chain.cpp")

    class Result:
        returncode = 0
        stderr = ""
        stdout = json.dumps({"tests": []})

    monkeypatch.setattr(test_inner.subprocess, "run", lambda *args, **kwargs: Result())
    with pytest.raises(RuntimeError, match="no longer contained"):
        test_inner.validate_coverage(result)
