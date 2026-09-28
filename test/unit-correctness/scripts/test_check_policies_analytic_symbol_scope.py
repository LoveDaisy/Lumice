"""Regression net for `check_analytic_symbol_scope` in `scripts/check_policies.py`.

The rule is the prefix boundary of liblumice_analytic, the first library published for outside
consumers: each shared library's export list is generated from its own headers, so where
`LUMICE_ANALYTIC_` may be spelled decides which library can export a function, and what else the
analytic header spells decides whether its consumers are pulled into lumice.h. A policy gate's red
state has to be demonstrated rather than assumed; this file is the "write a violating symbol and
watch the checker go red" probe made permanent, in both directions the rule guards.
"""
from __future__ import annotations

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "scripts"))

import check_policies  # noqa: E402

RULE = "analytic-symbol-scope"
HEADER = "include/lumice_analytic.h"


@pytest.fixture
def src_root(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> Path:
    src = tmp_path / "src"
    src.mkdir()
    monkeypatch.setattr(check_policies, "REPO_ROOT", tmp_path)
    monkeypatch.setattr(check_policies, "SRC", src)
    return src


def _violations(src_root: Path, body: str, name: str = "scratch.cpp") -> list:
    path = src_root / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(body, encoding="utf-8")
    return check_policies.check_analytic_symbol_scope()


# --- must stay red: the analytic prefix outside its home --------------------


def test_declaration_in_product_header_is_flagged(src_root: Path) -> None:
    """The shape the rule exists for: an analytic function declared in lumice.h, which would put
    it into liblumice's (and liblumice_testapi's) export list."""
    out = _violations(
        src_root,
        "#ifndef LUMICE_H_\nint LUMICE_ANALYTIC_GetApiVersion(void);\n#endif\n",
        name="include/lumice.h",
    )
    assert len(out) == 1
    assert out[0].rule == RULE
    assert out[0].line == 2


def test_call_from_engine_code_is_flagged(src_root: Path) -> None:
    out = _violations(
        src_root, "void F() {\n  (void)LUMICE_ANALYTIC_GetApiVersion();\n}\n", name="server/c_api.cpp"
    )
    assert len(out) == 1


def test_struct_tag_outside_home_is_flagged(src_root: Path) -> None:
    out = _violations(src_root, "struct LUMICE_ANALYTIC_Crystal_ {\n  double c;\n};\n", name="core/x.hpp")
    assert len(out) == 1


def test_sibling_directory_with_a_matching_name_is_not_home(src_root: Path) -> None:
    """`src/analytic_extra/` is not `src/analytic/`: home is a directory, not a name prefix."""
    out = _violations(src_root, "int x = LUMICE_ANALYTIC_API_VERSION;\n", name="analytic_extra/x.cpp")
    assert len(out) == 1


# --- must stay red: the analytic header reaching into the other surfaces -----


@pytest.mark.parametrize(
    "line",
    [
        "LUMICE_ANALYTIC_API int LUMICE_ANALYTIC_F(LUMICE_Scene* s);",  # a lumice.h type
        "LUMICE_ANALYTIC_API LUMICE_ErrorCode LUMICE_ANALYTIC_G(void);",  # lumice.h's error enum
        "#define LUMICE_ANALYTIC_MAX LUMICE_MAX_CONFIG_CRYSTALS",  # a lumice.h macro
        "LUMICE_ANALYTIC_API void LUMICE_ANALYTIC_H(LUMICE_TEST_RenderDomainMask* m);",  # test surface
    ],
)
def test_analytic_header_naming_another_surface_is_flagged(src_root: Path, line: str) -> None:
    out = _violations(src_root, f"#ifndef LUMICE_ANALYTIC_H_\n{line}\n#endif\n", name=HEADER)
    assert len(out) == 1
    assert out[0].rule == RULE
    assert out[0].line == 2


# --- must stay green: the prefix at home, and prose ------------------------


def test_prefix_in_its_own_header_is_not_flagged(src_root: Path) -> None:
    out = _violations(
        src_root,
        "#ifndef LUMICE_ANALYTIC_H_\n#define LUMICE_ANALYTIC_API_VERSION 1\n"
        "LUMICE_ANALYTIC_API int LUMICE_ANALYTIC_GetApiVersion(void);\n#endif\n",
        name=HEADER,
    )
    assert out == []


def test_prefix_in_its_own_directory_is_not_flagged(src_root: Path) -> None:
    out = _violations(
        src_root,
        "int LUMICE_ANALYTIC_GetApiVersion(void) { return LUMICE_ANALYTIC_API_VERSION; }\n",
        name="analytic/deeper/analytic_api.cpp",
    )
    assert out == []


def test_implementation_may_use_other_names(src_root: Path) -> None:
    """Direction (b) is about the published header only: the implementation is free to call into
    the engine, which is the whole point of linking lumice_obj."""
    out = _violations(src_root, "#include \"lumice.h\"\nLUMICE_ErrorCode e;\n", name="analytic/x.cpp")
    assert out == []


def test_comments_are_not_hits_in_either_direction(src_root: Path) -> None:
    out = _violations(
        src_root,
        "// unlike LUMICE_Scene in lumice.h, this header shares no type\n"
        "/* LUMICE_TEST_ hooks are elsewhere */\nint x;\n",
        name=HEADER,
    )
    assert out == []
    out = _violations(src_root, "// see LUMICE_ANALYTIC_GetApiVersion\nint y;\n", name="core/y.cpp")
    assert out == []


def test_other_prefixes_do_not_trip_the_rule_outside_home(src_root: Path) -> None:
    """LUMICE_ and LUMICE_TEST_ are other rules' business; `\\b` keeps MY_LUMICE_ANALYTIC_X out."""
    out = _violations(
        src_root,
        "int a = LUMICE_API_VERSION;\nint b = LUMICE_TEST_X;\nint MY_LUMICE_ANALYTIC_X = 0;\n",
    )
    assert out == []


def test_files_outside_src_are_not_scanned(src_root: Path) -> None:
    """Tests legitimately name the prefix (the export-set test asserts on it)."""
    outside = src_root.parent / "test"
    outside.mkdir()
    (outside / "x.cpp").write_text("int v = LUMICE_ANALYTIC_V;\n", encoding="utf-8")
    assert check_policies.check_analytic_symbol_scope() == []


def test_check_is_registered_in_checks(src_root: Path) -> None:
    assert check_policies.check_analytic_symbol_scope in check_policies.CHECKS


def test_real_tree_is_green() -> None:
    """The committed tree satisfies the rule — the allowlist names the real home."""
    assert check_policies.check_analytic_symbol_scope() == []
