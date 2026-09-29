"""LUMICE_ANALYTIC_EvaluatePath's C ABI contract, through ctypes against the build tree.

The evaluator's physics is checked in-process by
test/unit-correctness/analytic/test_path_evaluation.cpp, which compiles the kernel but not the C
wrapper (the wrapper's load-time initialiser silences the engine's console sink for the whole
process). This file owns what only the wrapper does: return codes, the zero-fill of the result
after struct_size on every error (doc/analytic-api.md sections 4.4 and 8.2), library-owned storage
and its release, the mapping of a valid=0 pose to a successful call, and the crystal-construction
warning reaching the host's log callback instead of stderr (section 6) — the end-to-end half that
test_analytic_log_sink.py could not drive before the library had a function that builds a crystal.

Each case runs in a child interpreter so a crash in the library is a failed case, not a dead
pytest. Needs a shared build (`./scripts/build.sh -sj release`); skipped when there is none, or in
a CUDA configure, which does not produce the library. Free of test/e2e's helpers and of numpy.

symmetry_semantics: none — every call evaluates one concrete face sequence (doc/analytic-api.md
section 3); nothing here compares face sequences across a symmetry.
"""
from __future__ import annotations

import json
import subprocess
import sys
import textwrap
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
SHARED_OUT = ROOT / "build" / "Release" / "shared"

pytestmark = pytest.mark.slow

_PATTERN = {"win32": "lumice_analytic.dll", "darwin": "liblumice_analytic.dylib", "linux": "liblumice_analytic.so"}


def _platform() -> str:
    if sys.platform.startswith("win"):
        return "win32"
    if sys.platform == "darwin":
        return "darwin"
    return "linux"


def _cuda_configured() -> bool:
    for cache in (ROOT / "build" / "CMakeCache.txt", ROOT / "build" / "cmake_build" / "shared" / "CMakeCache.txt"):
        if cache.is_file():
            text = cache.read_text(encoding="utf-8", errors="replace")
            if "BUILD_SHARED_LIBS:BOOL=ON" in text and "LUMICE_CUDA_ENABLED:BOOL=ON" in text:
                return True
    return False


def _find() -> Path:
    if not SHARED_OUT.is_dir():
        pytest.skip(f"no shared build at {SHARED_OUT} (./scripts/build.sh -sj release)")
    pattern = _PATTERN[_platform()]
    hits = sorted(p for p in SHARED_OUT.rglob(pattern) if p.is_file())
    if not hits:
        if _cuda_configured():
            pytest.skip("CUDA configure: liblumice_analytic is not produced (doc/analytic-api.md 2.3)")
        pytest.fail(f"{pattern} not found under {SHARED_OUT} although a shared build exists there")
    if len(hits) > 1:
        pytest.fail(f"more than one {pattern} under {SHARED_OUT}: {hits}")
    return hits[0]


# The bindings every child starts with: the two structs field for field with lumice_analytic.h, and
# helpers that build a call from plain lists.
_PRELUDE = textwrap.dedent(
    """
    import ctypes, json, math, sys
    from ctypes import POINTER, Structure, byref, c_double, c_int, c_uint32, c_void_p, c_char_p, sizeof

    class Crystal(Structure):
        _fields_ = [("kind", c_int), ("height", c_double), ("face_distance", c_double * 6),
                    ("upper_h", c_double), ("lower_h", c_double),
                    ("upper_wedge_deg", c_double), ("lower_wedge_deg", c_double)]

    class PathEvaluation(Structure):
        _fields_ = [("struct_size", c_uint32), ("valid", c_int), ("outgoing_direction", c_double * 3),
                    ("fresnel_transmission", c_double), ("segment_count", c_int),
                    ("segment_directions", POINTER(c_double)), ("interface_transmittances", POINTER(c_double)),
                    ("storage", c_void_p)]

    OK, NULL_ARG, INVALID_VALUE, INVALID_CONFIG = 0, 1, 2, 3
    lib = ctypes.CDLL(LIB)
    lib.LUMICE_ANALYTIC_EvaluatePath.restype = c_int
    lib.LUMICE_ANALYTIC_EvaluatePath.argtypes = [POINTER(Crystal), POINTER(c_int), c_int, c_double,
                                                 POINTER(c_double), POINTER(c_double), POINTER(PathEvaluation)]
    lib.LUMICE_ANALYTIC_ReleasePathEvaluation.restype = None
    lib.LUMICE_ANALYTIC_ReleasePathEvaluation.argtypes = [POINTER(PathEvaluation)]

    def prism(height=1.0, **extra):
        c = Crystal(kind=0, height=height)
        for i in range(6):
            c.face_distance[i] = 1.0
        for k, v in extra.items():
            setattr(c, k, v)
        return c

    IDENTITY = [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]
    # Horizontal sunlight entering prism face 3 (outward normal +x) and leaving through face 5.
    SUN = [-math.cos(math.radians(40.0)), math.sin(math.radians(40.0)), 0.0]

    def fresh(size=None):
        out = PathEvaluation()
        ctypes.memset(byref(out), 0x5A, sizeof(out))  # garbage the call must overwrite
        out.struct_size = sizeof(PathEvaluation) if size is None else size
        return out

    def call(crystal, faces, n=1.31, incident=SUN, pose=IDENTITY, out=None):
        out = fresh() if out is None else out
        fa = (c_int * len(faces))(*faces)
        rc = lib.LUMICE_ANALYTIC_EvaluatePath(byref(crystal) if crystal is not None else None, fa, len(faces), n,
                                              (c_double * 3)(*incident), (c_double * 9)(*pose), byref(out))
        return rc, out

    def zeroed_after_size(out):
        raw = ctypes.string_at(ctypes.addressof(out), sizeof(out))
        return all(b == 0 for b in raw[4:])
    """
)


def _run_child(body: str) -> subprocess.CompletedProcess[str]:
    code = f"LIB = {str(_find())!r}\n" + _PRELUDE + "\n" + textwrap.dedent(body)
    proc = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True, check=False)
    assert proc.returncode == 0, f"child failed ({proc.returncode}):\n{proc.stdout}\n{proc.stderr}"
    return proc


def test_valid_path_fills_the_result_and_release_zeroes_it() -> None:
    proc = _run_child(
        """
        rc, out = call(prism(), [3, 5])
        assert rc == OK and out.valid == 1, (rc, out.valid)
        d = list(out.outgoing_direction)
        assert abs(math.sqrt(sum(v * v for v in d)) - 1.0) < 1e-14, d
        assert out.segment_count == 3
        seg = [out.segment_directions[i] for i in range(9)]
        assert seg[:3] == SUN, seg  # identity pose: body frame = world frame
        assert all(abs(a - b) < 1e-15 for a, b in zip(seg[6:], d)), (seg, d)
        t = [out.interface_transmittances[i] for i in range(2)]
        assert all(0.0 < v < 1.0 for v in t), t
        assert abs(out.fresnel_transmission - t[0] * t[1]) < 1e-15
        # Horizontal ray through a vertical 60-degree wedge: deviation between the 22-degree minimum
        # and 180 degrees, staying horizontal.
        deviation = math.degrees(math.acos(sum(a * b for a, b in zip(SUN, d))))
        assert 21.8 < deviation < 180.0 and abs(d[2]) < 1e-15, (deviation, d)
        assert out.storage
        lib.LUMICE_ANALYTIC_ReleasePathEvaluation(byref(out))
        assert out.struct_size == sizeof(PathEvaluation) and zeroed_after_size(out)
        lib.LUMICE_ANALYTIC_ReleasePathEvaluation(byref(out))  # a zeroed struct: no-op
        lib.LUMICE_ANALYTIC_ReleasePathEvaluation(None)
        print(json.dumps({"deviation": deviation}))
        """
    )
    assert "deviation" in proc.stdout


def test_invalid_pose_is_a_successful_call_with_valid_zero() -> None:
    _run_child(
        """
        # Sunlight arriving from inside face 3's half-space: it cannot enter through face 3.
        rc, out = call(prism(), [3, 5], incident=[-v for v in SUN])
        assert rc == OK, rc
        assert out.valid == 0 and out.segment_count == 0 and out.fresnel_transmission == 0.0
        assert not out.segment_directions and not out.interface_transmittances and not out.storage
        assert zeroed_after_size(out)
        lib.LUMICE_ANALYTIC_ReleasePathEvaluation(byref(out))
        """
    )


def test_call_errors_return_their_code_and_zero_fill_the_result() -> None:
    _run_child(
        """
        tilted = [0.6, 0.8, 1e-4]
        mirror = [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, -1.0]
        cases = [
            ("null crystal", lambda: call(None, [3, 5]), NULL_ARG),
            ("one face", lambda: call(prism(), [3]), INVALID_VALUE),
            ("65 faces", lambda: call(prism(), [3, 5] * 32 + [3]), INVALID_VALUE),
            ("face 13 on a prism", lambda: call(prism(), [3, 13]), INVALID_VALUE),
            ("face 0", lambda: call(prism(), [0, 5]), INVALID_VALUE),
            ("n = 0", lambda: call(prism(), [3, 5], n=0.0), INVALID_VALUE),
            ("n = nan", lambda: call(prism(), [3, 5], n=float("nan")), INVALID_VALUE),
            ("non-unit incident", lambda: call(prism(), [3, 5], incident=tilted), INVALID_VALUE),
            ("improper pose", lambda: call(prism(), [3, 5], pose=mirror), INVALID_VALUE),
            ("prism with a wedge", lambda: call(prism(upper_wedge_deg=28.0), [3, 5]), INVALID_VALUE),
            ("unknown kind", lambda: call(prism(kind=5), [3, 5]), INVALID_VALUE),
            ("zero-volume prism", lambda: call(prism(height=0.0), [3, 5]), INVALID_CONFIG),
            ("struct_size too small", lambda: call(prism(), [3, 5], out=fresh(8)), INVALID_VALUE),
        ]
        for name, run, want in cases:
            rc, out = run()
            assert rc == want, (name, rc, want)
            if name == "struct_size too small":
                raw = ctypes.string_at(ctypes.addressof(out), sizeof(out))
                assert raw[4:8] == bytes(4) and raw[8:] == bytes([0x5A]) * (sizeof(out) - 8), name
                assert out.struct_size == 8
            else:
                assert out.struct_size == sizeof(PathEvaluation) and zeroed_after_size(out), name
            lib.LUMICE_ANALYTIC_ReleasePathEvaluation(byref(out))
        # NULL out: nothing to fill, the code alone reports it.
        fa = (c_int * 2)(3, 5)
        rc = lib.LUMICE_ANALYTIC_EvaluatePath(byref(prism()), fa, 2, 1.31, (c_double * 3)(*SUN),
                                              (c_double * 9)(*IDENTITY), None)
        assert rc == NULL_ARG, rc
        """
    )


def test_rejected_crystal_warning_reaches_the_callback_not_stderr() -> None:
    proc = _run_child(
        """
        lib.LUMICE_ANALYTIC_SetLogCallback.restype = None
        CB = ctypes.CFUNCTYPE(None, c_int, c_char_p, c_char_p)
        got = []
        cb = CB(lambda level, name, msg: got.append((level, msg.decode())))
        lib.LUMICE_ANALYTIC_SetLogCallback.argtypes = [CB]
        lib.LUMICE_ANALYTIC_SetLogCallback(cb)
        c = prism()
        c.face_distance[3] = -1.0  # faces 3 and 6 meet: the closed-form cross section is empty
        rc, out = call(c, [3, 5])
        assert rc == INVALID_CONFIG, rc
        warnings = [m for level, m in got if level == 4]
        assert any("failed closed-form validity gate" in m for m in warnings), got
        print(json.dumps(warnings))
        """
    )
    assert proc.stderr == "", proc.stderr


def test_rejected_crystal_without_callback_writes_nothing() -> None:
    proc = _run_child(
        """
        c = prism()
        c.face_distance[3] = -1.0
        rc, out = call(c, [3, 5])
        assert rc == INVALID_CONFIG, rc
        """
    )
    assert proc.stderr == "", proc.stderr
