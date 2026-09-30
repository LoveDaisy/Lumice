"""What each of the three shared libraries actually exports, read off the built binaries.

liblumice, liblumice_testapi and liblumice_analytic are linked from the same engine objects, so
what separates their surfaces is nothing but each library's export list, generated from its
headers by scripts/gen_export_list.py (root CMakeLists.txt, lumice_apply_export_list). Which
headers those are is declared once, in cmake/export_surfaces.cmake; this test reads that same
declaration (scripts/check_policies.py's parse_export_surfaces), never a copy of it. A green
build says nothing about whether that worked: a list the linker ignored, a whitelist that let the
engine's C++ symbols through, or a Windows DLL still exporting everything all link fine. So this
reads the dynamic symbol table of each built library with the platform's own tool — `nm -D` on
Linux, `nm -gU` on macOS, `dumpbin /exports` on Windows — and compares it with the generator's own
parse of the headers. Reading the table and computing both sides is scripts/check_export_surface.py,
the same code the release workflow runs on every engine file it packages; every file a lookup
finds is checked, so a tree holding more than one engine (one DLL per ISA tier) has each compared:

    liblumice           == LUMICE_ENGINE_SURFACE_HEADERS   (six lumice_*.h + lumice_analytic_core.h)
    liblumice_testapi   == LUMICE_TESTAPI_SURFACE_HEADERS  (the engine's + lumice_test_api.h)
    liblumice_analytic  == LUMICE_ANALYTIC_SURFACE_HEADERS (lumice_analytic.h + its core header),
                           every name LUMICE_ANALYTIC_*, and at least one

The expected side reuses the generator's parser on purpose (one authority for "what a header
declares"). What that cannot see — a declaration that lost its visibility marker, gone from both
sides at once — the generator itself refuses at build time.

Each library is also loaded, in a child process so its engine copy never shares this process with
the test library the rest of the suite loads (doc/analytic-api.md section 2.6), and the analytic
placeholder is called: its answer must equal the header's LUMICE_ANALYTIC_API_VERSION.
Separately, the engine must not carry liblumice_analytic's management functions: linked in, the
management TU would remove the engine's console sink at load.

Needs a shared build (`./scripts/build.sh -sj release`); skipped when there is none at all. When a
shared build exists, a missing library is a failure rather than a skip — a lookup that silently
found nothing would otherwise read as a pass — except liblumice_analytic in a CUDA configure,
which by design does not produce it. Deliberately free of numpy/Pillow and of test/e2e's helpers,
so the Windows CI legs can run it with nothing but pytest installed.
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

import check_export_surface as ces  # noqa: E402
import gen_export_list  # noqa: E402

pytestmark = pytest.mark.slow

SHARED_OUT = ROOT / "build" / "Release" / "shared"
ANALYTIC_CORE_H = ROOT / "src" / "include" / "lumice_analytic_core.h"

_LIBS = sorted(ces.PATTERNS)


def _find_all(lib: str) -> list[Path]:
    """Every built file of `lib`. More than one is legitimate only for the engine on Windows (one
    DLL per ISA tier); each is checked."""
    if not SHARED_OUT.is_dir():
        pytest.skip(f"no shared build at {SHARED_OUT} (./scripts/build.sh -sj release)")
    hits = ces.find_all(SHARED_OUT, lib)
    if not hits:
        if lib == "lumice_analytic" and ces.cuda_configured(ROOT):
            pytest.skip("CUDA configure: liblumice_analytic is not produced (doc/analytic-api.md 2.3)")
        pattern = ces.PATTERNS[lib][ces.platform_key()]
        pytest.fail(f"{pattern} not found under {SHARED_OUT} although a shared build exists there")
    return hits


def _find(lib: str) -> Path:
    """The one built file of `lib`, for the checks that load it."""
    hits = _find_all(lib)
    if len(hits) > 1:
        pytest.fail(f"more than one {lib} under {SHARED_OUT}: {hits}")
    return hits[0]


def _exported(path: Path) -> set[str]:
    try:
        return ces.exported(path)
    except ces.ExportTableError as e:
        pytest.fail(str(e))


def _expected(lib: str) -> set[str]:
    return ces.expected(ces.SURFACE_KEY[lib])


@pytest.mark.parametrize("lib", _LIBS)
def test_export_set_equals_header_declarations(lib: str) -> None:
    expected = _expected(lib)
    bad = []
    for path in _find_all(lib):
        extra, missing = ces.compare(_exported(path), expected)
        if extra or missing:
            bad.append(ces.describe(path.name, extra, missing))
    assert not bad, "\n".join(bad)


def test_analytic_exports_only_its_own_prefix() -> None:
    """doc/analytic-api.md section 7, stated independently of the header parse: every exported
    name is LUMICE_ANALYTIC_*, and there is at least one."""
    actual = _exported(_find("lumice_analytic"))
    assert actual
    assert all(n.startswith("LUMICE_ANALYTIC_") for n in actual), sorted(actual)


def _load_in_child(path: Path, body: str) -> str:
    code = (
        "import ctypes, sys\n"
        f"lib = ctypes.CDLL({str(path)!r})\n"
        f"{body}\n"
    )
    proc = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True, check=False)
    assert proc.returncode == 0, f"loading {path.name} failed:\n{proc.stdout}\n{proc.stderr}"
    return proc.stdout


@pytest.mark.parametrize("lib", _LIBS)
def test_library_loads_and_resolves_every_declared_function(lib: str) -> None:
    path = _find(lib)
    names = sorted(_expected(lib))
    _load_in_child(path, f"for n in {names!r}:\n    getattr(lib, n)")


def test_declaration_names_every_library() -> None:
    """The declaration is the authority on every library this test checks: a library it did not
    declare would otherwise be compared against nothing."""
    assert set(ces.surfaces()) == set(ces.SURFACE_KEY.values())


def test_engine_carries_no_library_management_function() -> None:
    """The engine libraries export lumice_analytic_core.h, never lumice_analytic.h's own two
    functions: those are liblumice_analytic's, and their TU (analytic_lib.cpp) also silences the
    console at load. The set equality above already implies this; stated on its own so the reason
    survives an edit of the declaration."""
    management = set(gen_export_list.parse_headers([ROOT / "src" / "include" / "lumice_analytic.h"]))
    assert management == {"LUMICE_ANALYTIC_GetApiVersion", "LUMICE_ANALYTIC_SetLogCallback"}
    for lib in ("lumice", "lumice_testapi"):
        for path in _find_all(lib):
            assert not (_exported(path) & management), path.name


def test_analytic_placeholder_returns_header_version() -> None:
    m = re.search(r"#define\s+LUMICE_ANALYTIC_API_VERSION\s+(\d+)", ANALYTIC_CORE_H.read_text(encoding="utf-8"))
    assert m, "LUMICE_ANALYTIC_API_VERSION not found in lumice_analytic_core.h"
    out = _load_in_child(
        _find("lumice_analytic"),
        "lib.LUMICE_ANALYTIC_GetApiVersion.restype = ctypes.c_int\n"
        "print(lib.LUMICE_ANALYTIC_GetApiVersion())",
    )
    assert int(out.strip()) == int(m.group(1))
