#!/usr/bin/env python3
"""Compare what a built shared library exports with what its headers declare.

    check_export_surface.py <engine|testapi|analytic> FILE [FILE ...]

Each FILE's dynamic export table is read with the platform's own tool — `nm -D --defined-only` on
Linux, `nm -gU` on macOS, `dumpbin /exports` on Windows — and must equal, name for name, the set
of functions the library's surface headers declare. Which headers those are is declared once, in
cmake/export_surfaces.cmake (read through check_policies.parse_export_surfaces), and what a header
declares is gen_export_list.parse_headers' answer — the same parser that generated the export list
the linker was handed. So both sides of the comparison come from the one authority, and what is
left to disagree is exactly what the linker did with the list.

The files are only read, never loaded: the release packages carry engines built for ISA tiers the
runner's CPU may not have (an x86-64-v4 liblumice.so on a runner without AVX-512), and an export
table does not depend on the tier. Every file named must pass; a file that is missing, or no file
at all, is a failure, so an empty loop cannot read as green.

Used by test/e2e-correctness/test_export_symbol_scope.py (the build tree, every platform) and by
.github/workflows/release.yml (every engine file in the linux-x64 and windows-x64 packages). The
case it exists for is the one no link step reports: GNU ld drops a name from a version script
without a word, and the library links, loads and runs until the first caller of that name.

Also importable: the test uses `exported`, `expected`, `find_all` and `cuda_configured` from here
instead of keeping copies.
"""
from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

import check_policies  # noqa: E402
import gen_export_list  # noqa: E402

# The declaration's library keys (LUMICE_<KEY>_SURFACE_HEADERS) by CMake target name.
SURFACE_KEY = {"lumice": "ENGINE", "lumice_testapi": "TESTAPI", "lumice_analytic": "ANALYTIC"}

# File-name patterns per target and platform. On Windows the `lumice` target is named after its
# ISA tier (lumice-engine.<tier>.dll, root CMakeLists.txt) — the one irregular name, and the reason
# a lookup may find more than one engine.
PATTERNS = {
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

# Symbols GNU ld defines in every shared object's dynamic table on its own, whatever the version
# script says. Named one by one so that anything else unexpected still fails.
GNU_LINKER_DEFINED = frozenset({"_init", "_fini", "__bss_start", "_edata", "_end"})

# dumpbin table rows: `ordinal hint RVA name`, the name possibly followed by `= forwarder`.
_DUMPBIN_ROW = re.compile(r"^\s+\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(\S+)")


class ExportTableError(RuntimeError):
    """The export table could not be read (tool missing, tool failed, file missing)."""


def platform_key() -> str:
    if sys.platform.startswith("win"):
        return "win32"
    if sys.platform == "darwin":
        return "darwin"
    return "linux"


# --- expected side ----------------------------------------------------------


def surfaces() -> dict[str, list[str]]:
    """The declaration: library key -> header paths relative to the repository root."""
    return check_policies.parse_export_surfaces((ROOT / check_policies.EXPORT_SURFACES_REL).read_text(encoding="utf-8"))


def expected(key: str) -> set[str]:
    """What the library whose declaration key is `key` (ENGINE / TESTAPI / ANALYTIC) must export."""
    return set(gen_export_list.parse_headers([ROOT / rel for rel in surfaces()[key]]))


# --- actual side: parse (pure) ----------------------------------------------


def parse_nm(text: str, plat: str) -> set[str]:
    """Names in `nm -gU` (darwin) or `nm -D --defined-only` (linux) output."""
    names: set[str] = set()
    for line in text.splitlines():
        parts = line.split()
        if len(parts) < 3:
            continue
        name = parts[-1]
        if plat == "darwin":
            name = name[1:] if name.startswith("_") else name  # Mach-O C mangling
        elif name in GNU_LINKER_DEFINED:
            continue
        names.add(name)
    return names


def parse_dumpbin(text: str) -> set[str]:
    """Names in `dumpbin /exports` output."""
    names: set[str] = set()
    for line in text.splitlines():
        m = _DUMPBIN_ROW.match(line)
        if m:
            names.add(m.group(1))
    return names


# --- actual side: run the tool ----------------------------------------------


def _run(cmd: list[str]) -> str:
    if shutil.which(cmd[0]) is None:
        raise ExportTableError(f"`{cmd[0]}` not on PATH — needed to read the export table")
    proc = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if proc.returncode != 0:
        raise ExportTableError(f"{cmd} failed ({proc.returncode}):\n{proc.stderr}")
    return proc.stdout


def exported(path: Path, plat: str | None = None) -> set[str]:
    """The names `path` exports, read off its dynamic export table."""
    plat = plat or platform_key()
    if not Path(path).is_file():
        raise ExportTableError(f"{path}: no such file")
    if plat == "win32":
        return parse_dumpbin(_run(["dumpbin", "/nologo", "/exports", str(path)]))
    cmd = ["nm", "-gU", str(path)] if plat == "darwin" else ["nm", "-D", "--defined-only", str(path)]
    return parse_nm(_run(cmd), plat)


# --- comparison -------------------------------------------------------------


def compare(actual: set[str], want: set[str]) -> tuple[list[str], list[str]]:
    """(exported but not declared, declared but not exported), each sorted; both empty = equal."""
    return sorted(actual - want), sorted(want - actual)


def describe(name: str, extra: list[str], missing: list[str]) -> str:
    return f"{name}: exported but not declared: {extra}; declared but not exported: {missing}"


def check(key: str, files: list[Path], read=exported, out=sys.stdout) -> int:
    """Check every file against surface `key`; one line per file. 0 only if all of them match
    and there was at least one. `read` is the export-table reader (injectable for tests)."""
    if not files:
        print("FAIL: no files given — nothing was checked", file=out)
        return 1
    want = expected(key)
    failed = 0
    for f in files:
        try:
            actual = read(Path(f))
        except ExportTableError as e:
            print(f"FAIL {f}: {e}", file=out)
            failed += 1
            continue
        extra, missing = compare(actual, want)
        if extra or missing:
            print(f"FAIL {describe(str(f), extra, missing)}", file=out)
            failed += 1
        else:
            print(f"OK   {f}: {len(actual)} names == {key} surface", file=out)
    return 1 if failed else 0


# --- locating build-tree libraries (used by the tests) ----------------------


def find_all(directory: Path, target: str, plat: str | None = None) -> list[Path]:
    """Every file under `directory` named like `target`'s library on this platform, sorted. More
    than one is legitimate for the engine on Windows (one DLL per ISA tier)."""
    pattern = PATTERNS[target][plat or platform_key()]
    return sorted(p for p in Path(directory).rglob(pattern) if p.is_file())


def cuda_configured(root: Path = ROOT) -> bool:
    """True if a shared configure in `root`'s build tree has CUDA on (liblumice_analytic is then
    not produced, doc/analytic-api.md section 2.3)."""
    for cache in (root / "build" / "CMakeCache.txt", root / "build" / "cmake_build" / "shared" / "CMakeCache.txt"):
        if cache.is_file():
            text = cache.read_text(encoding="utf-8", errors="replace")
            if "BUILD_SHARED_LIBS:BOOL=ON" in text and "LUMICE_CUDA_ENABLED:BOOL=ON" in text:
                return True
    return False


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("surface", choices=sorted(k.lower() for k in SURFACE_KEY.values()))
    ap.add_argument("files", nargs="*", type=Path)
    args = ap.parse_args(argv)
    return check(args.surface.upper(), args.files)


if __name__ == "__main__":
    sys.exit(main())
