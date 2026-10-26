"""Negative controls for the narrow typed raypath dependency boundary."""
from __future__ import annotations

import sys
from pathlib import Path

import pytest

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "scripts"))
import check_policies  # noqa: E402


@pytest.fixture
def tree(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> Path:
    monkeypatch.setattr(check_policies, "REPO_ROOT", tmp_path)
    monkeypatch.setattr(check_policies, "SRC", tmp_path / "src")
    src = tmp_path / "src"
    for rel, content in {
        "raypath/domain/scope.hpp": '#include "raypath/results/quantity.hpp"\n',
        "raypath/results/quantity.hpp": '#include "core/def.hpp"\n',
        "core/def.hpp": "#pragma once\n",
        "config/config_manager.hpp": "#include <nlohmann/json.hpp>\n",
        "raypath/survey_report.hpp": '#include "raypath/domain/scope.hpp"\n#include "config/config_manager.hpp"\n',
        "raypath/probe_json.hpp": "#pragma once\n",
        "raypath/detail/survey/survey_assembly.hpp": "#pragma once\n",
        "raypath/single_path_json.cpp": '#include "raypath/results/quantity.hpp"\n',
        "server/c_api_raypath.cpp": '#include "raypath/domain/scope.hpp"\n',
    }.items():
        path = src / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
    return src


def _violations() -> list[check_policies.Violation]:
    violations = check_policies.check_raypath_typed_boundary()
    assert all(v.rule == "raypath-typed-boundary" for v in violations)
    return violations


def test_value_declarations_and_codec_values_are_legal(tree: Path) -> None:
    assert _violations() == []


@pytest.mark.parametrize("includer, target", [
    ("raypath/domain/scope.hpp", "raypath/survey_report.hpp"),
    ("raypath/domain/scope.hpp", "raypath/detail/survey/survey_assembly.hpp"),
    ("raypath/results/quantity.hpp", "raypath/probe_json.hpp"),
])
def test_public_values_reject_direct_execution_or_codec(tree: Path, includer: str, target: str) -> None:
    path = tree / includer
    path.write_text(f'#include "{target}"\n', encoding="utf-8")
    assert any(v.path == path and target in v.message for v in _violations())


def test_public_closure_rejects_intermediate_include_and_json(tree: Path) -> None:
    intermediate = tree / "core" / "def.hpp"
    intermediate.write_text('#include "raypath/survey_report.hpp"\n', encoding="utf-8")
    assert any(v.path == intermediate and "raypath/domain/scope.hpp" in v.message for v in _violations())
    intermediate.write_text("#include <nlohmann/json.hpp>\n", encoding="utf-8")
    assert any(v.path == intermediate and "includes JSON" in v.message for v in _violations())


def test_facade_rejects_direct_and_transitive_implementation(tree: Path) -> None:
    facade = tree / "raypath" / "survey_report.hpp"
    detail = "raypath/detail/survey/survey_assembly.hpp"
    facade.write_text(f'#include "{detail}"\n', encoding="utf-8")
    assert any(v.path == facade and detail in v.message for v in _violations())
    facade.write_text('#include "raypath/probe_json.hpp"\n', encoding="utf-8")
    assert any(v.path == facade and "probe_json.hpp" in v.message for v in _violations())
    intermediate = tree / "raypath" / "prepared_geometry.hpp"
    intermediate.write_text(f'#include "{detail}"\n', encoding="utf-8")
    facade.write_text('#include "raypath/prepared_geometry.hpp"\n', encoding="utf-8")
    assert any(v.path == intermediate and "survey_report.hpp" in v.message for v in _violations())


def test_facade_json_is_forbidden_only_through_raypath_headers(tree: Path) -> None:
    facade = tree / "raypath" / "survey_report.hpp"
    assert _violations() == []  # config declarations may bring their own JSON dependency
    facade.write_text('#include <nlohmann/json.hpp>\n', encoding="utf-8")
    assert any(v.path == facade and "includes JSON" in v.message for v in _violations())
    intermediate = tree / "raypath" / "prepared_geometry.hpp"
    intermediate.write_text('#include <nlohmann/json.hpp>\n', encoding="utf-8")
    facade.write_text('#include "raypath/prepared_geometry.hpp"\n', encoding="utf-8")
    assert any(v.path == intermediate and "includes JSON" in v.message for v in _violations())


def test_bridge_rejects_detail_but_allows_public_query(tree: Path) -> None:
    bridge = tree / "server" / "c_api_raypath.cpp"
    bridge.write_text('#include "raypath/survey_report.hpp"\n', encoding="utf-8")
    assert _violations() == []
    bridge.write_text('#include "raypath/detail/survey/survey_assembly.hpp"\n', encoding="utf-8")
    assert any(v.path == bridge and "bridge includes" in v.message for v in _violations())


@pytest.mark.parametrize("includer, target", [
    ("raypath/domain/scope.hpp", "raypath/survey_report.hpp"),
    ("raypath/results/quantity.hpp", "raypath/probe_json.hpp"),
    ("raypath/survey_report.hpp", "raypath/detail/survey/survey_assembly.hpp"),
    ("server/c_api_raypath.cpp", "raypath/detail/survey/survey_assembly.hpp"),
    ("raypath/single_path_json.cpp", "raypath/survey_report.hpp"),
])
@pytest.mark.parametrize("opening, closing", [('"', '"'), ('<', '>')])
def test_policy_process_exit_names_the_target_rule(tree: Path, monkeypatch: pytest.MonkeyPatch,
                                                   capsys: pytest.CaptureFixture[str], includer: str,
                                                   target: str, opening: str, closing: str) -> None:
    monkeypatch.setattr(check_policies, "CHECKS", [check_policies.check_raypath_typed_boundary])
    monkeypatch.setattr(sys, "argv", ["check_policies.py"])
    assert check_policies.main() == 0
    (tree / includer).write_text(f'#include {opening}{target}{closing}\n', encoding="utf-8")
    assert check_policies.main() == 1
    assert "[raypath-typed-boundary]" in capsys.readouterr().err


def test_codec_rejects_query_through_a_value_header(tree: Path) -> None:
    codec = tree / "raypath" / "single_path_json.cpp"
    codec.write_text('#include "raypath/results/quantity.hpp"\n', encoding="utf-8")
    value = tree / "raypath" / "results" / "quantity.hpp"
    value.write_text('#include "raypath/survey_report.hpp"\n', encoding="utf-8")
    assert any(v.path == value and "survey_report.hpp" in v.message for v in _violations())
    value.write_text('#include "core/def.hpp"\n', encoding="utf-8")
    codec.write_text('#include "raypath/survey_report.hpp"\n', encoding="utf-8")
    assert any(v.path == codec and "survey_report.hpp" in v.message for v in _violations())
