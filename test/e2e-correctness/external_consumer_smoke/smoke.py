"""Minimal ctypes consumer of liblumice_analytic — a sample for a Python binding.

Locates the library purely by the install-tree layout doc/analytic-api.md section 8.7 fixes as a
contract, via the environment variable it names (LUMICE_ANALYTIC_INSTALL_DIR). Uses nothing from
lumice.h or test/e2e/capi_runner.py: this is the *published* engine, a distinct copy of every
static from the test-only liblumice_testapi (section 2.6).

symmetry_semantics: none — this fixture compares no face sequence (doc/analytic-api.md section 3);
it only round-trips the placeholder version query.
"""
from __future__ import annotations

import ctypes
import os
import pathlib
import platform
import sys


def _library_path(prefix: pathlib.Path) -> pathlib.Path:
    system = platform.system()
    if system == "Windows":
        return prefix / "bin" / "lumice_analytic.dll"
    if system == "Darwin":
        return prefix / "lib" / "liblumice_analytic.dylib"
    return prefix / "lib" / "liblumice_analytic.so"


def main() -> int:
    prefix = pathlib.Path(os.environ["LUMICE_ANALYTIC_INSTALL_DIR"])
    lib = ctypes.CDLL(str(_library_path(prefix)))
    lib.LUMICE_ANALYTIC_GetApiVersion.restype = ctypes.c_int
    lib.LUMICE_ANALYTIC_GetApiVersion.argtypes = []
    version = lib.LUMICE_ANALYTIC_GetApiVersion()
    print(f"LUMICE_ANALYTIC_GetApiVersion={version}")
    # Deliberately weak (version > 0), unlike the C consumer's exact check against the header
    # macro: a binding has no header to compare with, and pins the version it was written for
    # itself. The pytest driver compares this output against the installed header.
    return 0 if version > 0 else 1


if __name__ == "__main__":
    sys.exit(main())
