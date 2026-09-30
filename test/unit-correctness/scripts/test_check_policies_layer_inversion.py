"""Regression net for `check_layer_inversion` in `scripts/check_policies.py`.

The rule is what keeps the layering of src/ true after it was drawn: cmake/lumice_layers.cmake says
which layer owns each file, and every #include must point at the same or a lower layer. The
foundation list is also the source set CMake builds lumice_foundation_obj from — the only engine
objects liblumice_analytic links — so an inversion into foundation is not a style finding, it is a
link failure of the published library waiting for the next build. A gate's red state has to be
demonstrated rather than assumed: every case here runs the checker against a scratch copy of the
real src/ and cmake/ trees, never against the working tree itself.
"""
from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path

import pytest

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "scripts"))

import check_policies  # noqa: E402

RULE = "layer-inversion"


@pytest.fixture
def tree(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> Path:
    shutil.copytree(REPO / "src", tmp_path / "src")
    (tmp_path / "cmake").mkdir()
    for name in ("lumice_layers.cmake", "lumice_layer_allowlist.txt"):
        shutil.copy2(REPO / "cmake" / name, tmp_path / "cmake" / name)
    monkeypatch.setattr(check_policies, "REPO_ROOT", tmp_path)
    monkeypatch.setattr(check_policies, "SRC", tmp_path / "src")
    return tmp_path


def _messages(tree: Path) -> list[str]:
    out = check_policies.check_layer_inversion()
    assert all(v.rule == RULE for v in out)
    return [f"{v.path.relative_to(tree).as_posix()}:{v.line}: {v.message}" for v in out]


def _prepend(path: Path, line: str) -> None:
    path.write_text(line + "\n" + path.read_text(encoding="utf-8"), encoding="utf-8")


# --- green: the tree as committed -------------------------------------------


def test_committed_tree_is_clean(tree: Path) -> None:
    assert _messages(tree) == []


# --- red: an inverted edge ---------------------------------------------------


def test_foundation_including_the_engine_is_flagged(tree: Path) -> None:
    """The acceptance probe: optics is foundation, server.hpp is the engine's own header."""
    _prepend(tree / "src" / "core" / "optics.cpp", '#include "server/server.hpp"')
    msgs = _messages(tree)
    assert len(msgs) == 1
    assert msgs[0].startswith("src/core/optics.cpp:1: core/optics.cpp -> server/server.hpp")
    assert "`foundation` includes the higher layer `engine`" in msgs[0]


def test_include_resolved_through_the_public_include_dir_is_an_edge(tree: Path) -> None:
    """`#include "lumice_engine.h"` resolves via src/include/, not relative to src/ — the path the
    prototype of this gate did not search, which hid every edge into the C API headers."""
    _prepend(tree / "src" / "core" / "math.cpp", '#include "lumice_engine.h"')
    msgs = _messages(tree)
    assert len(msgs) == 1
    assert "core/math.cpp -> include/lumice_engine.h" in msgs[0]
    assert "`foundation` includes the higher layer `engine`" in msgs[0]


def test_directory_relative_include_is_an_edge(tree: Path) -> None:
    _prepend(tree / "src" / "core" / "geo3d.cpp", '#include "../server/server.hpp"')
    msgs = _messages(tree)
    assert len(msgs) == 1
    assert "core/geo3d.cpp -> server/server.hpp" in msgs[0]


def test_layered_file_including_the_shell_is_flagged(tree: Path) -> None:
    _prepend(tree / "src" / "server" / "server.cpp", '#include "gui/app.hpp"')
    msgs = _messages(tree)
    assert len(msgs) == 1
    assert "higher layer `shell`" in msgs[0]


def test_scene_bridge_reaching_for_the_server_is_flagged(tree: Path) -> None:
    """The C API bridges are registered at the layer of the header they implement, so this rule is
    what holds each one to its layer: the scene bridge may not touch the engine."""
    _prepend(tree / "src" / "server" / "c_api_scene.cpp", '#include "server/server.hpp"')
    msgs = _messages(tree)
    assert len(msgs) == 1
    assert "server/c_api_scene.cpp -> server/server.hpp" in msgs[0]
    assert "`scene` includes the higher layer `engine`" in msgs[0]


@pytest.mark.parametrize("header", ["server/c_api_internal.hpp"])
def test_bridge_including_an_aggregate_header_is_flagged(tree: Path, header: str) -> None:
    """The internal aggregate sits at `capi`, above every bridge: a bridge that took it would
    silently reach every layer through it."""
    _prepend(tree / "src" / "server" / "c_api_render.cpp", f'#include "{header}"')
    msgs = _messages(tree)
    assert len(msgs) == 1
    assert f"server/c_api_render.cpp -> {header}" in msgs[0]
    assert "`render` includes the higher layer `capi`" in msgs[0]


def test_same_layer_and_downward_edges_pass(tree: Path) -> None:
    _prepend(tree / "src" / "server" / "server.cpp", '#include "core/optics.hpp"')
    _prepend(tree / "src" / "core" / "optics.cpp", '#include "core/math.hpp"')
    assert _messages(tree) == []


def test_commented_out_include_is_not_an_edge(tree: Path) -> None:
    _prepend(tree / "src" / "core" / "optics.cpp", '// #include "server/server.hpp"')
    assert _messages(tree) == []


# --- red: the allowlist only shrinks ----------------------------------------


def test_removing_an_allowlist_entry_reddens_its_edge(tree: Path) -> None:
    # The committed allowlist may be empty, so the case plants its own grandfathered edge rather
    # than relying on one the tree happens to carry.
    _prepend(tree / "src" / "server" / "stats.hpp", '#include "server/server.hpp"')
    allow = tree / "cmake" / "lumice_layer_allowlist.txt"
    entry = "server/stats.hpp -> server/server.hpp"
    text = allow.read_text(encoding="utf-8")
    allow.write_text(text + entry + "\n", encoding="utf-8")
    assert _messages(tree) == []
    allow.write_text(text, encoding="utf-8")
    msgs = _messages(tree)
    assert len(msgs) == 1
    assert msgs[0].startswith("src/server/stats.hpp:")


def test_entry_for_an_edge_that_no_longer_exists_is_stale(tree: Path) -> None:
    allow = tree / "cmake" / "lumice_layer_allowlist.txt"
    allow.write_text(allow.read_text(encoding="utf-8") + "core/math.cpp -> server/server.hpp\n", encoding="utf-8")
    msgs = _messages(tree)
    assert len(msgs) == 1
    assert "STALE" in msgs[0] and msgs[0].startswith("cmake/lumice_layer_allowlist.txt:")


def test_malformed_allowlist_line_is_flagged(tree: Path) -> None:
    allow = tree / "cmake" / "lumice_layer_allowlist.txt"
    allow.write_text(allow.read_text(encoding="utf-8") + "core/math.cpp server/server.hpp\n", encoding="utf-8")
    assert any("expected `<includer> -> <included>`" in m for m in _messages(tree))


# --- red: ownership ----------------------------------------------------------


def test_new_unowned_file_is_flagged(tree: Path) -> None:
    (tree / "src" / "core" / "brand_new.cpp").write_text("int x = 0;\n", encoding="utf-8")
    msgs = _messages(tree)
    assert len(msgs) == 1
    assert msgs[0].startswith("src/core/brand_new.cpp:1: UNOWNED")


def test_manifest_entry_naming_no_file_is_flagged(tree: Path) -> None:
    (tree / "src" / "core" / "miller_wedge.cpp").unlink()
    msgs = _messages(tree)
    assert any("`core/miller_wedge.cpp` is listed but does not exist" in m for m in msgs)


def test_shell_files_need_no_owner(tree: Path) -> None:
    (tree / "src" / "gui" / "brand_new.cpp").write_text("int x = 0;\n", encoding="utf-8")
    assert _messages(tree) == []


# --- red: the manifest's restricted syntax -----------------------------------


@pytest.mark.parametrize(
    ("old", "new", "fragment"),
    [
        ("  core/math.cpp\n", '  "core/math.cpp"\n', "not one bare token"),
        ("  core/math.cpp\n", "  ${SOME_VAR}\n", "not one bare token"),
        ("  core/math.cpp\n", "  core/math.cpp core/math.hpp\n", "not one bare token"),
        ("set(LUMICE_LAYER_foundation_FILES\n", "if(APPLE)\nset(LUMICE_LAYER_foundation_FILES\n", "expected `set("),
        ("  core/math.cpp\n", "  core/math.mm\n", "foundation compiles only"),
    ],
)
def test_manifest_outside_the_subset_is_an_error(tree: Path, old: str, new: str, fragment: str) -> None:
    manifest = tree / "cmake" / "lumice_layers.cmake"
    text = manifest.read_text(encoding="utf-8")
    assert text.count(old) == 1
    manifest.write_text(text.replace(old, new), encoding="utf-8")
    msgs = _messages(tree)
    assert len(msgs) == 1
    assert msgs[0].startswith("cmake/lumice_layers.cmake:")
    assert fragment in msgs[0]


def test_file_owned_by_two_layers_is_an_error(tree: Path) -> None:
    manifest = tree / "cmake" / "lumice_layers.cmake"
    text = manifest.read_text(encoding="utf-8")
    text = text.replace("set(LUMICE_LAYER_engine_FILES\n", "set(LUMICE_LAYER_engine_FILES\n  core/math.cpp\n")
    manifest.write_text(text, encoding="utf-8")
    assert any("owned by both" in m for m in _messages(tree))


# --- one manifest, two readers ------------------------------------------------


def test_cmake_and_python_read_the_same_layers() -> None:
    """CMake builds lumice_foundation_obj from the manifest and this checker judges against it;
    if the two readers ever disagreed about a list, the gate would be checking a layering the
    build does not have. Compares every list, not only foundation."""
    cmake = shutil.which("cmake")
    assert cmake, "cmake is required to compare the manifest's two readers"
    order, owner = check_policies.parse_layer_manifest(
        (REPO / "cmake" / "lumice_layers.cmake").read_text(encoding="utf-8")
    )
    script = (
        f'include("{(REPO / "cmake" / "lumice_layers.cmake").as_posix()}")\n'
        'message(STATUS "ORDER=${LUMICE_LAYER_ORDER}")\n'
        "foreach(layer IN LISTS LUMICE_LAYER_ORDER)\n"
        '  message(STATUS "LAYER ${layer}=${LUMICE_LAYER_${layer}_FILES}")\n'
        "endforeach()\n"
    )
    probe = Path(__file__).with_name("_layer_manifest_probe.cmake")
    try:
        probe.write_text(script, encoding="utf-8")
        res = subprocess.run([cmake, "-P", str(probe)], capture_output=True, text=True, check=True)
    finally:
        probe.unlink(missing_ok=True)
    lines = [ln.removeprefix("-- ") for ln in (res.stdout + res.stderr).splitlines()]
    cmake_order = next(ln for ln in lines if ln.startswith("ORDER=")).removeprefix("ORDER=").split(";")
    assert cmake_order == order
    for layer in order:
        line = next(ln for ln in lines if ln.startswith(f"LAYER {layer}="))
        cmake_files = sorted(f for f in line.split("=", 1)[1].split(";") if f)
        assert cmake_files == sorted(p for p, lay in owner.items() if lay == layer), layer
