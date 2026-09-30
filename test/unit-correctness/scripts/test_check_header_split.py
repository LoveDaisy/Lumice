"""Unit tests for `scripts/check_header_split.py`, the standing invariants of the engine's
capability headers (one declaring header per name, an acyclic include graph, an umbrella that
declares nothing and gathers every header). Scratch trees for the red states; the real tree for
the green one. No build needed.
"""
from __future__ import annotations

import sys
from pathlib import Path

import pytest

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "scripts"))

import check_header_split as chs  # noqa: E402

SURFACES = """set(LUMICE_ENGINE_SURFACE_HEADERS
  src/include/lumice_a.h
  src/include/lumice_b.h
  src/include/lumice_analytic_core.h
)

set(LUMICE_ANALYTIC_SURFACE_HEADERS
  src/include/lumice_analytic_core.h
)
"""

A_H = """#ifndef LUMICE_A_H_
#define LUMICE_A_H_
#define LUMICE_API
#define LUMICE_LIMIT 4
typedef struct LUMICE_Thing_ LUMICE_Thing;
typedef void (*LUMICE_Callback)(int level);
LUMICE_API int LUMICE_A(void);
#endif
"""

B_H = """#ifndef LUMICE_B_H_
#define LUMICE_B_H_
#include "lumice_a.h"
typedef struct LUMICE_Pair_ {
  int x;
} LUMICE_Pair;
LUMICE_API int LUMICE_B(const LUMICE_Thing* t);
#endif
"""

UMBRELLA = """#ifndef LUMICE_H_
#define LUMICE_H_
#include "lumice_a.h"
#include "lumice_b.h"
#endif
"""


def _tree(tmp_path: Path, a: str = A_H, b: str = B_H, umbrella: str | None = UMBRELLA) -> Path:
    (tmp_path / "cmake").mkdir()
    (tmp_path / "cmake" / "export_surfaces.cmake").write_text(SURFACES)
    inc = tmp_path / "src" / "include"
    inc.mkdir(parents=True)
    (inc / "lumice_a.h").write_text(a)
    (inc / "lumice_b.h").write_text(b)
    (inc / "lumice_analytic_core.h").write_text("#define LUMICE_ANALYTIC_API\n")
    if umbrella is not None:
        (inc / "lumice.h").write_text(umbrella)
    return tmp_path


def test_scratch_tree_green(tmp_path: Path) -> None:
    assert chs.check(_tree(tmp_path)) == []


def test_family_is_engine_surface_minus_analytic(tmp_path: Path) -> None:
    assert chs.family_headers(_tree(tmp_path)) == ["src/include/lumice_a.h", "src/include/lumice_b.h"]


def test_declarations_by_kind() -> None:
    d = chs.declarations(A_H + B_H.replace("#ifndef LUMICE_B_H_\n#define LUMICE_B_H_\n", ""))
    assert d["function"] == ["LUMICE_A", "LUMICE_B"]
    assert d["type"] == ["LUMICE_Thing", "LUMICE_Callback", "LUMICE_Pair"]
    assert d["macro"] == ["LUMICE_API", "LUMICE_LIMIT"]  # include guard excluded


@pytest.mark.parametrize(
    ("addition", "expected"),
    [
        ("LUMICE_API int LUMICE_A(void);\n", "function LUMICE_A is declared in 2 headers"),
        ("typedef struct LUMICE_Thing_ LUMICE_Thing;\n", "type LUMICE_Thing is declared in 2 headers"),
        ("#define LUMICE_LIMIT 4\n", "macro LUMICE_LIMIT is declared in 2 headers"),
    ],
)
def test_name_declared_in_two_headers_is_red(tmp_path: Path, addition: str, expected: str) -> None:
    # Each copy is one C accepts silently: a compatible redeclaration, an identical typedef, an
    # identical #define.
    b = B_H.replace("#endif\n", addition + "#endif\n")
    msgs = chs.check(_tree(tmp_path, b=b))
    assert any(expected in m for m in msgs), msgs


def test_include_cycle_is_red(tmp_path: Path) -> None:
    a = A_H.replace("#define LUMICE_A_H_\n", '#define LUMICE_A_H_\n#include "lumice_b.h"\n')
    msgs = chs.check(_tree(tmp_path, a=a))
    assert any(m.startswith("include cycle:") and "lumice_a.h" in m and "lumice_b.h" in m for m in msgs), msgs


def test_capability_header_including_umbrella_is_a_cycle(tmp_path: Path) -> None:
    b = B_H.replace('#include "lumice_a.h"', '#include "lumice.h"')
    msgs = chs.check(_tree(tmp_path, b=b))
    assert any(m.startswith("include cycle:") for m in msgs), msgs


def test_umbrella_declaring_something_is_red(tmp_path: Path) -> None:
    umbrella = UMBRELLA.replace("#endif\n", "#define LUMICE_STRAY 1\n#endif\n")
    msgs = chs.check(_tree(tmp_path, umbrella=umbrella))
    assert any("lumice.h: declares LUMICE_STRAY" in m for m in msgs), msgs


def test_umbrella_missing_a_header_is_red(tmp_path: Path) -> None:
    umbrella = UMBRELLA.replace('#include "lumice_b.h"\n', "")
    msgs = chs.check(_tree(tmp_path, umbrella=umbrella))
    assert msgs == ["src/include/lumice.h: does not include lumice_b.h"]


def test_no_umbrella_is_fine(tmp_path: Path) -> None:
    assert chs.check(_tree(tmp_path, umbrella=None)) == []


def test_listed_header_missing_is_red(tmp_path: Path) -> None:
    root = _tree(tmp_path)
    (root / "src" / "include" / "lumice_b.h").unlink()
    msgs = chs.check(root)
    assert any("lumice_b.h: listed in" in m for m in msgs), msgs


def test_real_tree_holds_the_invariants() -> None:
    assert chs.check(REPO) == []
