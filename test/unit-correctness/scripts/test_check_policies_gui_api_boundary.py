"""Regression net for `check_gui_api_boundary` in `scripts/check_policies.py`.

The GUI reaches the engine only through the C API: src/gui/ must not include core/ or config/,
and of the public headers under src/include/ it may include only the engine's capability headers
(the engine export surface of cmake/export_surfaces.cmake minus the analytic family). The second
half is what replaced "include the C API header" when that header was split into one per
capability; a policy gate's red state has to be demonstrated, so every shape it rejects is written
here and watched go red, next to the spellings the GUI actually uses, which must stay green.
"""
from __future__ import annotations

import shutil
import sys
from pathlib import Path

import pytest

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "scripts"))

import check_policies  # noqa: E402

RULE = "gui-api-boundary"
CAPABILITY = [f"lumice_{c}.h" for c in ("base", "scene", "render", "editor", "engine", "raypath")]
ANALYTIC = ["lumice_analytic.h", "lumice_analytic_core.h"]


@pytest.fixture
def src_root(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> Path:
    """A scratch tree: the real export-surface declaration and an empty file per public header."""
    src = tmp_path / "src"
    (src / "include").mkdir(parents=True)
    (src / "gui").mkdir()
    (src / "util").mkdir()
    (src / "util" / "helper.hpp").write_text("\n", encoding="utf-8")
    for name in CAPABILITY + ANALYTIC:
        (src / "include" / name).write_text("\n", encoding="utf-8")
    (tmp_path / "cmake").mkdir()
    shutil.copy(REPO / "cmake" / "export_surfaces.cmake", tmp_path / "cmake" / "export_surfaces.cmake")
    monkeypatch.setattr(check_policies, "REPO_ROOT", tmp_path)
    monkeypatch.setattr(check_policies, "SRC", src)
    return src


def _violations(src_root: Path, body: str, name: str = "gui/panel.cpp") -> list:
    path = src_root / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(body, encoding="utf-8")
    return check_policies.check_gui_api_boundary()


# --- must stay red ------------------------------------------------------------


@pytest.mark.parametrize("line", ['#include "core/optics.hpp"', '#include <config/sim_data.hpp>', '#include "../core/math.hpp"'])
def test_core_and_config_includes_are_flagged(src_root: Path, line: str) -> None:
    out = _violations(src_root, f"{line}\n")
    assert len(out) == 1
    assert out[0].rule == RULE


@pytest.mark.parametrize("spelling", ["include/{}", "{}"])
@pytest.mark.parametrize("name", ANALYTIC)
def test_analytic_header_is_flagged(src_root: Path, spelling: str, name: str) -> None:
    """A real public header that is not a capability header of the engine."""
    out = _violations(src_root, f'#include "{spelling.format(name)}"\n')
    assert len(out) == 1
    assert out[0].rule == RULE
    assert f"`{name}`" in out[0].message


def test_public_header_outside_the_surface_is_flagged(src_root: Path) -> None:
    """Any other file in src/include/ — an aggregate header added later, say — is not allowed."""
    (src_root / "include" / "not_a_capability.h").write_text("\n", encoding="utf-8")
    out = _violations(src_root, '#include "not_a_capability.h"\n')
    assert len(out) == 1


def test_include_dir_spelling_is_flagged_even_without_the_file(src_root: Path) -> None:
    """A deleted header cannot come back through the GUI: the `include/` spelling alone is read."""
    out = _violations(src_root, '#include "include/not_a_capability.h"\n', name="gui/app.hpp")
    assert len(out) == 1
    assert out[0].line == 1


def test_missing_declaration_is_a_violation(src_root: Path) -> None:
    (src_root.parent / "cmake" / "export_surfaces.cmake").unlink()
    out = _violations(src_root, '#include "include/lumice_base.h"\n')
    assert len(out) == 1
    assert "declaration missing" in out[0].message


# --- must stay green ----------------------------------------------------------


@pytest.mark.parametrize("spelling", ["include/{}", "{}", "<{}>"])
@pytest.mark.parametrize("name", CAPABILITY)
def test_capability_headers_pass(src_root: Path, spelling: str, name: str) -> None:
    target = spelling.format(name)
    line = f"#include {target}" if target.startswith("<") else f'#include "{target}"'
    assert _violations(src_root, f"{line}\n") == []


def test_non_public_includes_pass(src_root: Path) -> None:
    body = '#include "imgui.h"\n#include "util/helper.hpp"\n#include "gui/app.hpp"\n#include <vector>\n'
    assert _violations(src_root, body) == []


def test_commented_include_is_not_a_hit(src_root: Path) -> None:
    assert _violations(src_root, '// #include "lumice_analytic.h"\nint x;\n') == []


def test_files_outside_gui_are_not_scanned(src_root: Path) -> None:
    """The shells and bridges may use the analytic capability; the rule is the GUI's."""
    assert _violations(src_root, '#include "lumice_analytic_core.h"\n', name="server/c_api_x.cpp") == []


def test_real_surface_is_the_six_capability_headers() -> None:
    """The partition the rule reads, pinned: a change to it is a change to what the GUI may see."""
    assert check_policies._gui_capability_headers() == set(CAPABILITY)


def test_check_is_registered_in_checks() -> None:
    assert check_policies.check_gui_api_boundary in check_policies.CHECKS
