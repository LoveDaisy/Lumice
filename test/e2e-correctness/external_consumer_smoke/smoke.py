"""Minimal ctypes consumer of liblumice_analytic — a sample for a Python binding.

Locates the library purely by the install-tree layout doc/analytic-api.md section 8.7 fixes as a
contract, via the environment variable it names (LUMICE_ANALYTIC_INSTALL_DIR). Uses nothing from
lumice.h or test/e2e/capi_runner.py: this is the *published* engine, a distinct copy of every
static from the test-only liblumice_testapi (section 2.6).

symmetry_semantics: none — this fixture compares no face sequence across a symmetry
(doc/analytic-api.md section 3); besides the version query it evaluates one concrete path, 3-5
through a regular prism.
"""
from __future__ import annotations

import ctypes
import math
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
    if version <= 0:
        return 1
    return _evaluate_path(lib)


# The two structs of lumice_analytic.h, field for field — what a binding declares.
class Crystal(ctypes.Structure):
    _fields_ = [
        ("kind", ctypes.c_int),
        ("height", ctypes.c_double),
        ("face_distance", ctypes.c_double * 6),
        ("upper_h", ctypes.c_double),
        ("lower_h", ctypes.c_double),
        ("upper_wedge_deg", ctypes.c_double),
        ("lower_wedge_deg", ctypes.c_double),
    ]


class PathEvaluation(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_uint32),
        ("valid", ctypes.c_int),
        ("outgoing_direction", ctypes.c_double * 3),
        ("fresnel_transmission", ctypes.c_double),
        ("segment_count", ctypes.c_int),
        ("segment_directions", ctypes.POINTER(ctypes.c_double)),
        ("interface_transmittances", ctypes.POINTER(ctypes.c_double)),
        ("storage", ctypes.c_void_p),
    ]


def _evaluate_path(lib: ctypes.CDLL) -> int:
    """One EvaluatePath call: path 3-5 through a regular prism, identity pose, horizontal sunlight."""
    lib.LUMICE_ANALYTIC_EvaluatePath.restype = ctypes.c_int
    lib.LUMICE_ANALYTIC_EvaluatePath.argtypes = [
        ctypes.POINTER(Crystal),
        ctypes.POINTER(ctypes.c_int),
        ctypes.c_int,
        ctypes.c_double,
        ctypes.POINTER(ctypes.c_double),
        ctypes.POINTER(ctypes.c_double),
        ctypes.POINTER(PathEvaluation),
    ]
    lib.LUMICE_ANALYTIC_ReleasePathEvaluation.restype = None
    lib.LUMICE_ANALYTIC_ReleasePathEvaluation.argtypes = [ctypes.POINTER(PathEvaluation)]
    crystal = Crystal(kind=0, height=1.0)
    for i in range(6):
        crystal.face_distance[i] = 1.0
    faces = (ctypes.c_int * 2)(3, 5)
    incident = (ctypes.c_double * 3)(-math.cos(math.radians(40.0)), math.sin(math.radians(40.0)), 0.0)
    pose = (ctypes.c_double * 9)(1, 0, 0, 0, 1, 0, 0, 0, 1)
    out = PathEvaluation(struct_size=ctypes.sizeof(PathEvaluation))
    rc = lib.LUMICE_ANALYTIC_EvaluatePath(ctypes.byref(crystal), faces, 2, 1.31, incident, pose, ctypes.byref(out))
    d = list(out.outgoing_direction)
    ok = rc == 0 and out.valid == 1 and out.segment_count == 3 and abs(math.hypot(*d) - 1.0) < 1e-12
    print(f"LUMICE_ANALYTIC_EvaluatePath rc={rc} valid={out.valid} segments={out.segment_count}")
    lib.LUMICE_ANALYTIC_ReleasePathEvaluation(ctypes.byref(out))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
