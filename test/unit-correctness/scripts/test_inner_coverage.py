"""The inner loop removes only registered executions contained in a parent."""
from __future__ import annotations

import json
import sys
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


def test_registration_change_refuses_containment(monkeypatch: pytest.MonkeyPatch) -> None:
    result = plan("test/fixtures/v3_config.json", "test/composition-correctness/raypath/test_input_assembly_chain.cpp")

    class Result:
        returncode = 0
        stderr = ""
        stdout = json.dumps({"tests": []})

    monkeypatch.setattr(test_inner.subprocess, "run", lambda *args, **kwargs: Result())
    with pytest.raises(RuntimeError, match="no longer contained"):
        test_inner.validate_coverage(result)
