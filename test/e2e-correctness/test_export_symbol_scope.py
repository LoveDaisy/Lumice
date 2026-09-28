"""What each of the three shared libraries actually exports, read off the built binaries.

liblumice, liblumice_testapi and liblumice_analytic are linked from the same engine objects, so
what separates their surfaces is nothing but each library's export list, generated from its
headers by scripts/gen_export_list.py (root CMakeLists.txt, lumice_apply_export_list). A green
build says nothing about whether that worked: a list the linker ignored, a whitelist that let the
engine's C++ symbols through, or a Windows DLL still exporting everything all link fine. So this
reads the dynamic symbol table of each built library with the platform's own tool — `nm -D` on
Linux, `nm -gU` on macOS, `dumpbin /exports` on Windows — and compares it with the generator's own
parse of the headers:

    liblumice           == the functions lumice.h declares
    liblumice_testapi   == lumice.h + test/support/lumice_test_api.h
    liblumice_analytic  == lumice_analytic.h, every name LUMICE_ANALYTIC_*, and at least one

The expected side reuses the generator's parser on purpose (one authority for "what a header
declares"). What that cannot see — a declaration that lost its visibility marker, gone from both
sides at once — the generator itself refuses at build time.

Each library is also loaded, in a child process so its engine copy never shares this process with
the test library the rest of the suite loads (doc/analytic-api.md section 2.6), and the analytic
placeholder is called: its answer must equal the header's LUMICE_ANALYTIC_API_VERSION.

Needs a shared build (`./scripts/build.sh -sj release`); skipped when there is none at all. When a
shared build exists, a missing library is a failure rather than a skip — a lookup that silently
found nothing would otherwise read as a pass — except liblumice_analytic in a CUDA configure,
which by design does not produce it. Deliberately free of numpy/Pillow and of test/e2e's helpers,
so the Windows CI legs can run it with nothing but pytest installed.
"""
from __future__ import annotations

import re
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

import gen_export_list  # noqa: E402

pytestmark = pytest.mark.slow

SHARED_OUT = ROOT / "build" / "Release" / "shared"
LUMICE_H = ROOT / "src" / "include" / "lumice.h"
TEST_API_H = ROOT / "test" / "support" / "lumice_test_api.h"
ANALYTIC_H = ROOT / "src" / "include" / "lumice_analytic.h"

# File-name patterns per library and platform. On Windows the `lumice` target is named after its
# ISA tier (lumice-engine.<tier>.dll, root CMakeLists.txt) — the one irregular name.
_PATTERNS = {
    "lumice": {"win32": "lumice-engine.*.dll", "darwin": "liblumice.dylib", "linux": "liblumice.so"},
    "lumice_testapi": {
        "win32": "lumice_testapi.dll",
        "darwin": "liblumice_testapi.dylib",
        "linux": "liblumice_testapi.so",
    },
    "lumice_analytic": {
        "win32": "lumice_analytic.dll",
        "darwin": "liblumice_analytic.dylib",
        "linux": "liblumice_analytic.so",
    },
}

_EXPECTED_HEADERS = {
    "lumice": [LUMICE_H],
    "lumice_testapi": [LUMICE_H, TEST_API_H],
    "lumice_analytic": [ANALYTIC_H],
}

# Symbols GNU ld defines in every shared object's dynamic table on its own, whatever the version
# script says. Named one by one so that anything else unexpected still fails.
_GNU_LINKER_DEFINED = frozenset({"_init", "_fini", "__bss_start", "_edata", "_end"})


def _platform() -> str:
    if sys.platform.startswith("win"):
        return "win32"
    if sys.platform == "darwin":
        return "darwin"
    return "linux"


def _cuda_configured() -> bool:
    """True if a shared configure in this tree has CUDA on (liblumice_analytic is then absent)."""
    for cache in (ROOT / "build" / "CMakeCache.txt", ROOT / "build" / "cmake_build" / "shared" / "CMakeCache.txt"):
        if cache.is_file():
            text = cache.read_text(encoding="utf-8", errors="replace")
            if "BUILD_SHARED_LIBS:BOOL=ON" in text and "LUMICE_CUDA_ENABLED:BOOL=ON" in text:
                return True
    return False


def _find(lib: str) -> Path:
    if not SHARED_OUT.is_dir():
        pytest.skip(f"no shared build at {SHARED_OUT} (./scripts/build.sh -sj release)")
    pattern = _PATTERNS[lib][_platform()]
    hits = sorted(p for p in SHARED_OUT.rglob(pattern) if p.is_file())
    if not hits:
        if lib == "lumice_analytic" and _cuda_configured():
            pytest.skip("CUDA configure: liblumice_analytic is not produced (doc/analytic-api.md 2.3)")
        pytest.fail(f"{pattern} not found under {SHARED_OUT} although a shared build exists there")
    if len(hits) > 1:
        pytest.fail(f"more than one {pattern} under {SHARED_OUT}: {hits}")
    return hits[0]


def _run(cmd: list[str]) -> str:
    if shutil.which(cmd[0]) is None:
        pytest.fail(f"`{cmd[0]}` not on PATH — needed to read the export table")
    proc = subprocess.run(cmd, capture_output=True, text=True, check=False)
    assert proc.returncode == 0, f"{cmd} failed ({proc.returncode}):\n{proc.stderr}"
    return proc.stdout


def _exported(path: Path) -> set[str]:
    plat = _platform()
    names: set[str] = set()
    if plat == "win32":
        # dumpbin table rows: `ordinal hint RVA name`, the name possibly followed by `= forwarder`.
        row = re.compile(r"^\s+\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(\S+)")
        for line in _run(["dumpbin", "/nologo", "/exports", str(path)]).splitlines():
            m = row.match(line)
            if m:
                names.add(m.group(1))
        return names
    cmd = ["nm", "-gU", str(path)] if plat == "darwin" else ["nm", "-D", "--defined-only", str(path)]
    for line in _run(cmd).splitlines():
        parts = line.split()
        if len(parts) < 3:
            continue
        name = parts[-1]
        if plat == "darwin":
            name = name[1:] if name.startswith("_") else name  # Mach-O C mangling
        elif name in _GNU_LINKER_DEFINED:
            continue
        names.add(name)
    return names


def _expected(lib: str) -> set[str]:
    return set(gen_export_list.parse_headers(_EXPECTED_HEADERS[lib]))


@pytest.mark.parametrize("lib", sorted(_PATTERNS))
def test_export_set_equals_header_declarations(lib: str) -> None:
    path = _find(lib)
    actual = _exported(path)
    expected = _expected(lib)
    assert actual == expected, (
        f"{path.name}: exported but not declared: {sorted(actual - expected)}; "
        f"declared but not exported: {sorted(expected - actual)}"
    )


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


@pytest.mark.parametrize("lib", sorted(_PATTERNS))
def test_library_loads_and_resolves_every_declared_function(lib: str) -> None:
    path = _find(lib)
    names = sorted(_expected(lib))
    _load_in_child(path, f"for n in {names!r}:\n    getattr(lib, n)")


def test_analytic_placeholder_returns_header_version() -> None:
    m = re.search(r"#define\s+LUMICE_ANALYTIC_API_VERSION\s+(\d+)", ANALYTIC_H.read_text(encoding="utf-8"))
    assert m, "LUMICE_ANALYTIC_API_VERSION not found in lumice_analytic.h"
    out = _load_in_child(
        _find("lumice_analytic"),
        "lib.LUMICE_ANALYTIC_GetApiVersion.restype = ctypes.c_int\n"
        "print(lib.LUMICE_ANALYTIC_GetApiVersion())",
    )
    assert int(out.strip()) == int(m.group(1))
