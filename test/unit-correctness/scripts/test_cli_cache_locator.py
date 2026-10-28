"""A build-only CLI must retain the ISA test's independent CMake-cache oracle."""
from __future__ import annotations

import importlib.util
from pathlib import Path

import pytest


def cli_isa_case():
    script = Path(__file__).resolve().parents[2] / "e2e-correctness" / "test_cli.py"
    spec = importlib.util.spec_from_file_location("lumice_cli_cache_probe", script)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


@pytest.mark.parametrize("flavor", ["static", "shared"])
def test_build_only_binary_uses_its_flavor_cache(tmp_path: Path, monkeypatch: pytest.MonkeyPatch, flavor: str) -> None:
    module = cli_isa_case()
    monkeypatch.setattr(module, "get_project_root", lambda: tmp_path)
    executable = tmp_path / "build" / "Release" / flavor / "bin" / "Lumice"
    executable.parent.mkdir(parents=True)
    executable.touch()
    expected = tmp_path / "build" / "cmake_build" / flavor / "CMakeCache.txt"
    expected.parent.mkdir(parents=True)
    expected.write_text("LUMICE_ISA_LEVEL:STRING=native\n")
    (tmp_path / "build" / "CMakeCache.txt").write_text("LUMICE_ISA_LEVEL:STRING=baseline\n")
    case = module.TestBenchmarkIsaField("test_isa_key_is_present_and_well_formed")
    case.lumice_bin = executable
    assert case._find_cmake_cache() == expected


def test_unknown_binary_does_not_borrow_unrelated_flavor_cache(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    module = cli_isa_case()
    monkeypatch.setattr(module, "get_project_root", lambda: tmp_path)
    cache = tmp_path / "build" / "cmake_build" / "static" / "CMakeCache.txt"
    cache.parent.mkdir(parents=True)
    cache.touch()
    unrelated = tmp_path / "elsewhere" / "Lumice"
    unrelated.parent.mkdir()
    unrelated.touch()
    case = module.TestBenchmarkIsaField("test_isa_key_is_present_and_well_formed")
    case.lumice_bin = unrelated
    assert case._find_cmake_cache() is None
