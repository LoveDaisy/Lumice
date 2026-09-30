"""Unit tests for `scripts/gen_export_list.py`, the one authority on what each shared library
exports. Pure text in, text out — no build needed. What a real built library exports is checked
against the same parse by test/e2e-correctness/test_export_symbol_scope.py; this file pins the
parse and the three output formats themselves.
"""
from __future__ import annotations

import sys
from pathlib import Path

import pytest

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "scripts"))

import gen_export_list as gel  # noqa: E402

HEAD = '#ifdef __cplusplus\nextern "C" {\n#endif\n#define LUMICE_API\n'
TAIL = "#ifdef __cplusplus\n}\n#endif\n"


def _parse(body: str) -> list[str]:
    return gel.parse_header_text(HEAD + body + TAIL)


# --- what is a function -----------------------------------------------------


def test_plain_and_pointer_returning_declarations_are_extracted() -> None:
    body = (
        "LUMICE_API void LUMICE_A(int x);\n"
        "LUMICE_API LUMICE_Server* LUMICE_B(void);\n"  # pointer return type: still a function
        "LUMICE_API const char *LUMICE_C(void);\n"
        "LUMICE_API LUMICE_ErrorCode LUMICE_D(const LUMICE_Scene* s,\n"
        "                                     int* out);\n"  # multi-line
    )
    assert _parse(body) == ["LUMICE_A", "LUMICE_B", "LUMICE_C", "LUMICE_D"]


def test_function_pointer_typedef_is_not_a_function() -> None:
    # Both names sit inside `(*NAME)`, so the character right after each name is `)`, never
    # `(` — _CALL_SHAPE's required shape — with or without inner spacing. No separate filter
    # is needed: this pins that natural exclusion, not a dedicated pointer-declarator branch.
    body = (
        "typedef void (*LUMICE_LogCallback)(int level, const char* msg);\n"
        "typedef void ( * LUMICE_Spaced )(void);\n"
        "LUMICE_API void LUMICE_SetLogCallback(LUMICE_LogCallback cb);\n"
    )
    assert _parse(body) == ["LUMICE_SetLogCallback"]


def test_macros_types_and_sizeof_are_not_functions() -> None:
    body = (
        "#define LUMICE_MAX_THINGS 8\n"
        "#define LUMICE_FN_LIKE(x) ((x) + 1)\n"
        "#define LUMICE_CONTINUED(x) \\\n  LUMICE_OTHER(x)\n"
        "typedef unsigned long long LUMICE_RayCount;\n"
        "typedef enum LUMICE_Kind_ { LUMICE_KIND_A = 0 } LUMICE_Kind;\n"
        "typedef struct LUMICE_S_ { int n; } LUMICE_S;\n"
        "LUMICE_API void LUMICE_Only(void);\n"
    )
    assert _parse(body) == ["LUMICE_Only"]


def test_names_in_comments_are_not_functions() -> None:
    body = "// LUMICE_Commented(void);\n/* LUMICE_Block(int); */\nLUMICE_API void LUMICE_Real(void);\n"
    assert _parse(body) == ["LUMICE_Real"]


def test_all_three_prefixes_and_markers_are_recognised() -> None:
    body = (
        "LUMICE_API int LUMICE_X(void);\n"
        "LUMICE_TEST_API int LUMICE_TEST_Y(void);\n"
        "LUMICE_ANALYTIC_API int LUMICE_ANALYTIC_Z(void);\n"
    )
    assert _parse(body) == ["LUMICE_X", "LUMICE_TEST_Y", "LUMICE_ANALYTIC_Z"]


# --- the marker requirement --------------------------------------------------


def test_unmarked_declaration_is_an_error() -> None:
    """A declaration without its marker is hidden under -fvisibility=hidden yet would still be
    listed — and a test comparing the library with this parse could not see it. Refuse."""
    with pytest.raises(gel.ExportListError, match="LUMICE_Forgotten"):
        _parse("LUMICE_API void LUMICE_Fine(void);\nvoid LUMICE_Forgotten(void);\n")


def test_marker_in_a_define_does_not_mark_the_next_declaration() -> None:
    """`#define LUMICE_API ...` right above the first declaration must not count as its marker."""
    with pytest.raises(gel.ExportListError, match="LUMICE_First"):
        gel.parse_header_text(
            'extern "C" {\n#if defined(_WIN32)\n#define LUMICE_API\n#else\n'
            '#define LUMICE_API __attribute__((visibility("default")))\n#endif\n'
            "void LUMICE_First(void);\n}\n"
        )


def test_marker_version_macro_is_not_a_marker() -> None:
    with pytest.raises(gel.ExportListError):
        _parse("int x = LUMICE_API_VERSION; void LUMICE_Nope(void);\n")


# --- union and formats ----------------------------------------------------------


def test_multiple_headers_are_unioned_and_sorted(tmp_path: Path) -> None:
    a = tmp_path / "a.h"
    b = tmp_path / "b.h"
    a.write_text(HEAD + "LUMICE_API void LUMICE_Z(void);\nLUMICE_API void LUMICE_A(void);\n" + TAIL)
    b.write_text(HEAD + "LUMICE_TEST_API void LUMICE_TEST_M(void);\nLUMICE_API void LUMICE_A(void);\n" + TAIL)
    assert gel.parse_headers([a, b]) == ["LUMICE_A", "LUMICE_TEST_M", "LUMICE_Z"]


def test_gnu_version_script() -> None:
    out = gel.render(["LUMICE_A", "LUMICE_B"], "gnu")
    assert out == "{\n  global:\n    LUMICE_A;\n    LUMICE_B;\n  local:\n    *;\n};\n"


def test_darwin_list_carries_the_mach_o_underscore() -> None:
    assert gel.render(["LUMICE_A", "LUMICE_B"], "darwin") == "_LUMICE_A\n_LUMICE_B\n"


def test_windows_def() -> None:
    assert gel.render(["LUMICE_A"], "def") == "EXPORTS\n  LUMICE_A\n"


def test_empty_list_is_refused() -> None:
    with pytest.raises(gel.ExportListError):
        gel.render([], "gnu")


def test_output_rewritten_only_on_change(tmp_path: Path) -> None:
    """Keeps an unrelated header edit from relinking all three libraries (Ninja restats)."""
    h = tmp_path / "h.h"
    h.write_text(HEAD + "LUMICE_API void LUMICE_A(void);\n" + TAIL)
    out = tmp_path / "out" / "x.map"
    assert gel.main(["--format", "gnu", "--output", str(out), str(h)]) == 0
    first = out.stat().st_mtime_ns
    assert gel.write_if_changed(out, out.read_text()) is False
    assert out.stat().st_mtime_ns == first


def test_cli_fails_on_unmarked_declaration(tmp_path: Path) -> None:
    h = tmp_path / "h.h"
    h.write_text(HEAD + "void LUMICE_A(void);\n" + TAIL)
    assert gel.main(["--format", "def", "--output", str(tmp_path / "x.def"), str(h)]) == 1
    assert not (tmp_path / "x.def").exists()


# --- the real headers --------------------------------------------------------------


@pytest.mark.parametrize(
    ("headers", "prefix"),
    [
        (["src/include/lumice_base.h"], "LUMICE_"),
        (["src/include/lumice_scene.h"], "LUMICE_"),
        (["src/include/lumice_render.h"], "LUMICE_"),
        (["src/include/lumice_editor.h"], "LUMICE_"),
        (["src/include/lumice_engine.h"], "LUMICE_"),
        (["src/include/lumice_raypath.h"], "LUMICE_"),
        (["src/include/lumice_analytic.h"], "LUMICE_ANALYTIC_"),
        (["src/include/lumice_analytic_core.h"], "LUMICE_ANALYTIC_"),
        (["test/support/lumice_test_api.h"], "LUMICE_TEST_"),
    ],
)
def test_real_headers_parse_nonempty_with_their_prefix(headers: list[str], prefix: str) -> None:
    names = gel.parse_headers([REPO / h for h in headers])
    assert names
    assert all(n.startswith(prefix) for n in names)
    if prefix == "LUMICE_":
        assert not any(n.startswith(("LUMICE_TEST_", "LUMICE_ANALYTIC_")) for n in names)
