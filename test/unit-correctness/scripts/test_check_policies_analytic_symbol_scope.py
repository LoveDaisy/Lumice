"""Regression net for `check_analytic_symbol_scope` and the export-surface declaration it reads.

The public headers under src/include/ form two families: the analytic one (the headers
liblumice_analytic exports, declared in cmake/export_surfaces.cmake — lumice_analytic_core.h and
lumice_analytic.h over it) and the engine one (every other header there). The rule keeps the
LUMICE_ANALYTIC_ prefix out of the engine headers and every other LUMICE_ identifier out of the
analytic ones; code is not restricted, since the engine libraries export the analytic capability.
A policy gate's red state has to be demonstrated rather than assumed; this file is the "write a
violating symbol and watch the checker go red" probe made permanent, in both directions the rule
guards, plus the green state the rule used to forbid (a shell calling an analytic function).
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

RULE = "analytic-symbol-scope"
CORE = "include/lumice_analytic_core.h"
UMBRELLA = "include/lumice_analytic.h"
ENGINE = "include/lumice.h"
DECL = REPO / "cmake" / "export_surfaces.cmake"

_CLEAN = {
    ENGINE: "#ifndef LUMICE_H_\n#define LUMICE_H_\nint LUMICE_GetVersion(void);\n#endif\n",
    CORE: "#ifndef LUMICE_ANALYTIC_CORE_H_\n#define LUMICE_ANALYTIC_API_VERSION 6\n"
    "LUMICE_ANALYTIC_API int LUMICE_ANALYTIC_EvaluatePath(void);\n#endif\n",
    UMBRELLA: '#ifndef LUMICE_ANALYTIC_H_\n#include "lumice_analytic_core.h"\n'
    "LUMICE_ANALYTIC_API int LUMICE_ANALYTIC_GetApiVersion(void);\n#endif\n",
}


@pytest.fixture
def src_root(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> Path:
    """A scratch tree: the real declaration, and a clean minimal header of each family."""
    src = tmp_path / "src"
    for rel, body in _CLEAN.items():
        (src / rel).parent.mkdir(parents=True, exist_ok=True)
        (src / rel).write_text(body, encoding="utf-8")
    (tmp_path / "cmake").mkdir()
    shutil.copy(DECL, tmp_path / "cmake" / "export_surfaces.cmake")
    monkeypatch.setattr(check_policies, "REPO_ROOT", tmp_path)
    monkeypatch.setattr(check_policies, "SRC", src)
    return src


def _violations(src_root: Path, body: str, name: str) -> list:
    path = src_root / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(body, encoding="utf-8")
    return check_policies.check_analytic_symbol_scope()


def test_clean_scratch_tree_is_green(src_root: Path) -> None:
    assert check_policies.check_analytic_symbol_scope() == []


# --- must stay red: an engine header naming the analytic prefix --------------


def test_declaration_in_engine_header_is_flagged(src_root: Path) -> None:
    """The shape the rule exists for: an analytic function declared in lumice.h, a second
    definition next to lumice_analytic_core.h's."""
    out = _violations(
        src_root,
        "#ifndef LUMICE_H_\nLUMICE_API int LUMICE_ANALYTIC_TraceFiber(void);\n#endif\n",
        name=ENGINE,
    )
    assert len(out) == 1
    assert out[0].rule == RULE
    assert out[0].path == src_root / ENGINE
    assert out[0].line == 2


def test_any_header_outside_the_declared_family_is_an_engine_header(src_root: Path) -> None:
    """The family is what export_surfaces.cmake declares, not a file-name pattern: a new header
    whose name merely starts with lumice_analytic is still an engine header."""
    out = _violations(src_root, "struct LUMICE_ANALYTIC_Crystal_ {\n  double c;\n};\n", name="include/lumice_analytic_extra.h")
    assert len(out) == 1
    out = _violations(src_root, "#define X LUMICE_ANALYTIC_API_VERSION\n", name="include/lumice_render.h")
    assert len(out) == 2


# --- must stay red: an analytic header reaching into the other surfaces -------


@pytest.mark.parametrize("header", [CORE, UMBRELLA])
@pytest.mark.parametrize(
    "line",
    [
        "LUMICE_ANALYTIC_API int LUMICE_ANALYTIC_F(LUMICE_Scene* s);",  # a lumice.h type
        "LUMICE_ANALYTIC_API LUMICE_ErrorCode LUMICE_ANALYTIC_G(void);",  # lumice.h's error enum
        "#define LUMICE_ANALYTIC_MAX LUMICE_MAX_CONFIG_CRYSTALS",  # a lumice.h macro
        "LUMICE_ANALYTIC_API void LUMICE_ANALYTIC_H(LUMICE_TEST_RenderDomainMask* m);",  # test surface
    ],
)
def test_analytic_header_naming_another_surface_is_flagged(src_root: Path, header: str, line: str) -> None:
    out = _violations(src_root, f"#ifndef GUARD_\n{line}\n#endif\n", name=header)
    assert len(out) == 1
    assert out[0].rule == RULE
    assert out[0].path == src_root / header
    assert out[0].line == 2


# --- must stay green: code calling the capability, and prose ----------------


@pytest.mark.parametrize("name", ["gui/panel.cpp", "server/c_api.cpp", "main.cpp", "raypath/x.hpp"])
def test_code_calling_an_analytic_function_is_not_flagged(src_root: Path, name: str) -> None:
    """Red under the rule's directory-allowlist form; the engine libraries now export the
    capability (lumice_analytic_core.h), so a shell or a bridge may call it."""
    out = _violations(src_root, "void F() {\n  (void)LUMICE_ANALYTIC_EvaluatePath();\n}\n", name=name)
    assert out == []


def test_comments_are_not_hits_in_either_direction(src_root: Path) -> None:
    out = _violations(
        src_root,
        "// unlike LUMICE_Scene in lumice.h, this header shares no type\n"
        "/* LUMICE_TEST_ hooks are elsewhere */\nint x;\n",
        name=CORE,
    )
    assert out == []
    out = _violations(src_root, "// see LUMICE_ANALYTIC_GetApiVersion\nint y;\n", name=ENGINE)
    assert out == []


def test_word_boundary_keeps_lookalikes_out(src_root: Path) -> None:
    """`\\b` keeps MY_LUMICE_ANALYTIC_X out of the engine direction."""
    out = _violations(src_root, "int MY_LUMICE_ANALYTIC_X = 0;\n", name=ENGINE)
    assert out == []


def test_headers_outside_src_include_are_not_scanned(src_root: Path) -> None:
    """Tests legitimately name the prefix (the export-set test asserts on it)."""
    outside = src_root.parent / "test" / "support"
    outside.mkdir(parents=True)
    (outside / "lumice_test_api.h").write_text("int v = LUMICE_ANALYTIC_V;\n", encoding="utf-8")
    assert check_policies.check_analytic_symbol_scope() == []


# --- the declaration the family comes from ------------------------------------


def test_missing_family_header_is_a_violation_not_a_skip(src_root: Path) -> None:
    """A declared analytic header that does not exist must not silently shrink the family (the
    engine direction would then scan it as an engine header, or not at all)."""
    (src_root / CORE).unlink()
    out = check_policies.check_analytic_symbol_scope()
    assert len(out) == 1
    assert "lumice_analytic_core.h" in out[0].message
    assert out[0].path.name == "export_surfaces.cmake"


def test_missing_declaration_is_a_violation(src_root: Path) -> None:
    (src_root.parent / "cmake" / "export_surfaces.cmake").unlink()
    out = check_policies.check_analytic_symbol_scope()
    assert len(out) == 1
    assert "declaration missing" in out[0].message


@pytest.mark.parametrize(
    ("text", "fragment"),
    [
        ('set(LUMICE_ENGINE_SURFACE_HEADERS\n  "src/include/lumice.h"\n)\n', "not one bare token"),
        ("set(LUMICE_ENGINE_SURFACE_HEADERS\n  src/include/lumice.h src/x.h\n)\n", "not one bare token"),
        ("if(APPLE)\nset(LUMICE_ENGINE_SURFACE_HEADERS\n  a.h\n)\nendif()\n", "expected `set("),
        ("set(SOMETHING_ELSE\n  a.h\n)\n", "is not LUMICE_<LIBRARY>_SURFACE_HEADERS"),
        ("set(LUMICE_A_SURFACE_HEADERS\n  a.h\n)\nset(LUMICE_A_SURFACE_HEADERS\n  b.h\n)\n", "declared twice"),
        ("set(LUMICE_A_SURFACE_HEADERS\n  ${LUMICE_B_SURFACE_HEADERS}\n)\n", "names no surface list declared above"),
        ("set(LUMICE_A_SURFACE_HEADERS\n  ${SOME_VAR}\n)\n", "names no surface list declared above"),
        ("set(LUMICE_A_SURFACE_HEADERS\n  a.h\n)\nset(LUMICE_B_SURFACE_HEADERS\n  ${LUMICE_A_SURFACE_HEADERS}\n  a.h\n)\n", "named twice"),
        ("set(LUMICE_A_SURFACE_HEADERS\n)\n", "is empty"),
        ("set(LUMICE_A_SURFACE_HEADERS\n  a.h\n", "unterminated"),
    ],
)
def test_declaration_outside_the_subset_is_an_error(text: str, fragment: str) -> None:
    with pytest.raises(check_policies.RestrictedCMakeError) as e:
        check_policies.parse_export_surfaces(text)
    assert fragment in str(e.value)


def test_reference_expands_to_the_earlier_list_in_order() -> None:
    surfaces = check_policies.parse_export_surfaces(
        "# c\nset(LUMICE_A_SURFACE_HEADERS\n  a.h\n  b.h\n)\n\nset(LUMICE_B_SURFACE_HEADERS\n"
        "  ${LUMICE_A_SURFACE_HEADERS}\n  c.h\n)\n"
    )
    assert surfaces == {"A": ["a.h", "b.h"], "B": ["a.h", "b.h", "c.h"]}


def test_real_declaration_is_the_expected_partition() -> None:
    """The packaging decision itself (cmake/export_surfaces.cmake's header): the engine hosts the
    capability, the test library is a superset of the engine, and liblumice_analytic alone adds the
    library-management header."""
    s = check_policies.parse_export_surfaces(DECL.read_text(encoding="utf-8"))
    assert s["ENGINE"] == [
        "src/include/lumice_base.h",
        "src/include/lumice_scene.h",
        "src/include/lumice_render.h",
        "src/include/lumice_editor.h",
        "src/include/lumice_engine.h",
        "src/include/lumice_raypath.h",
        "src/include/lumice_analytic_core.h",
    ]
    assert s["TESTAPI"] == s["ENGINE"] + ["test/support/lumice_test_api.h"]
    assert s["ANALYTIC"] == ["src/include/lumice_analytic_core.h", "src/include/lumice_analytic.h"]


def test_cmake_and_python_read_the_same_surfaces() -> None:
    """CMake generates each library's export list from this declaration and the export test and
    this rule judge against it; if the two readers ever disagreed about a list, the tests would be
    checking a surface the build does not produce."""
    cmake = shutil.which("cmake")
    assert cmake, "cmake is required to compare the declaration's two readers"
    surfaces = check_policies.parse_export_surfaces(DECL.read_text(encoding="utf-8"))
    script = f'include("{DECL.as_posix()}")\n' + "".join(
        f'message(STATUS "SURFACE {key}=${{LUMICE_{key}_SURFACE_HEADERS}}")\n' for key in surfaces
    )
    probe = Path(__file__).with_name("_export_surfaces_probe.cmake")
    try:
        probe.write_text(script, encoding="utf-8")
        res = subprocess.run([cmake, "-P", str(probe)], capture_output=True, text=True, check=True)
    finally:
        probe.unlink(missing_ok=True)
    lines = [ln.removeprefix("-- ") for ln in (res.stdout + res.stderr).splitlines()]
    for key, headers in surfaces.items():
        line = next(ln for ln in lines if ln.startswith(f"SURFACE {key}="))
        assert line.split("=", 1)[1].split(";") == headers, key


def test_check_is_registered_in_checks() -> None:
    assert check_policies.check_analytic_symbol_scope in check_policies.CHECKS


def test_real_tree_is_green() -> None:
    """The committed tree satisfies the rule."""
    assert check_policies.check_analytic_symbol_scope() == []
