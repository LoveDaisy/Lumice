"""The engine hosts the analytic capability as itself, not as a copy of liblumice_analytic.

The engine libraries export lumice_analytic_core.h's functions next to their own (cmake/
export_surfaces.cmake), built from the same objects liblumice_analytic is. What makes the two
different is one translation unit: src/analytic/analytic_lib.cpp, liblumice_analytic's library
management, whose load-time object removes the console sink. Linked into the engine by mistake it
changes no export table — test_export_symbol_scope.py cannot see it — but the engine's own
warnings would stop reaching stderr. So this checks behaviour, through liblumice_testapi (the
engine surface plus test hooks, the library pytest loads; test/e2e/capi_runner.py lib_candidates
is the authority on that choice):

  1. LUMICE_ANALYTIC_EvaluatePath gives bit-identical results in the engine and in
     liblumice_analytic (same objects, same compiler: any difference is a different code path).
  2. A rejected crystal with no callback installed leaves its warning on stderr — the opposite of
     liblumice_analytic, which must stay silent (test_analytic_evaluate_path.py). This is the case
     that goes red when the management TU is linked into the engine.
  3. With LUMICE_SetLogCallback installed the same warning reaches the callback at
     LUMICE_LOG_WARNING. The callback sink is attached independently of the console sink, so this
     one stays green under the mislink; it pins the engine's routing, not the mislink.

What this does not cover: the release packages' engines. Their two ISA builds per platform
(x86-64-v4 is not loadable on every runner) are checked on their export tables only
(scripts/check_export_surface.py in .github/workflows/release.yml); that they behave like this
build rests on their being built from the same lumice_obj.

Each case runs in a child interpreter so the engine copy never shares this process with the library
the rest of the suite loads (doc/analytic-api.md section 2.6). Needs a shared build
(`./scripts/build.sh -sj release`); skipped when there is none. In a CUDA configure
liblumice_analytic is not produced and case 1 skips; cases 2 and 3 still run. Free of test/e2e's
helpers and of numpy/Pillow, so the Windows shared-export CI legs can run it.

symmetry_semantics: none — one concrete face sequence is evaluated (doc/analytic-api.md section 3).
"""
from __future__ import annotations

import json
import subprocess
import sys
import textwrap
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

import check_export_surface as ces  # noqa: E402

pytestmark = pytest.mark.slow

SHARED_OUT = ROOT / "build" / "Release" / "shared"
WARNING_TEXT = "failed closed-form validity gate"
LUMICE_LOG_WARNING = 4  # lumice_base.h LUMICE_LogLevel


def _find(lib: str) -> Path:
    if not SHARED_OUT.is_dir():
        pytest.skip(f"no shared build at {SHARED_OUT} (./scripts/build.sh -sj release)")
    hits = ces.find_all(SHARED_OUT, lib)
    if not hits:
        if lib == "lumice_analytic" and ces.cuda_configured(ROOT):
            pytest.skip("CUDA configure: liblumice_analytic is not produced (doc/analytic-api.md 2.3)")
        pytest.fail(f"{ces.PATTERNS[lib][ces.platform_key()]} not found under {SHARED_OUT}")
    if len(hits) > 1:
        pytest.fail(f"more than one {lib} under {SHARED_OUT}: {hits}")
    return hits[0]


# The two structs field for field with lumice_analytic_core.h — the subset of
# test_analytic_evaluate_path.py's bindings these cases need. A drift from the header shows up as
# LUMICE_ANALYTIC_EvaluatePath refusing struct_size, which `call` asserts against.
_PRELUDE = textwrap.dedent(
    """
    import ctypes, json, math
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

    OK, INVALID_VALUE, INVALID_CONFIG = 0, 2, 3
    lib = ctypes.CDLL(LIB)
    lib.LUMICE_ANALYTIC_EvaluatePath.restype = c_int
    lib.LUMICE_ANALYTIC_EvaluatePath.argtypes = [POINTER(Crystal), POINTER(c_int), c_int, c_double,
                                                 POINTER(c_double), POINTER(c_double), POINTER(PathEvaluation)]
    lib.LUMICE_ANALYTIC_ReleasePathEvaluation.restype = None
    lib.LUMICE_ANALYTIC_ReleasePathEvaluation.argtypes = [POINTER(PathEvaluation)]

    def prism():
        c = Crystal(kind=0, height=1.0)
        for i in range(6):
            c.face_distance[i] = 1.0
        return c

    IDENTITY = (c_double * 9)(1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0)
    # Sunlight tilted off the horizontal, entering prism face 3 and leaving through face 5.
    SUN = [-0.7, 0.6, 0.3]
    SUN = [v / math.sqrt(sum(w * w for w in SUN)) for v in SUN]

    def call(crystal, faces):
        out = PathEvaluation()
        out.struct_size = sizeof(PathEvaluation)
        fa = (c_int * len(faces))(*faces)
        rc = lib.LUMICE_ANALYTIC_EvaluatePath(byref(crystal), fa, len(faces), 1.31, (c_double * 3)(*SUN),
                                              IDENTITY, byref(out))
        assert rc != INVALID_VALUE, "INVALID_VALUE: the ctypes bindings no longer match the header"
        return rc, out
    """
)


def _run_child(lib: str, body: str) -> subprocess.CompletedProcess[str]:
    code = f"LIB = {str(_find(lib))!r}\n" + _PRELUDE + "\n" + textwrap.dedent(body)
    proc = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True, check=False)
    assert proc.returncode == 0, f"child failed ({proc.returncode}):\n{proc.stdout}\n{proc.stderr}"
    return proc


_EVALUATE = """
rc, out = call(prism(), [3, 5])
assert rc == OK and out.valid == 1, (rc, out.valid)
n = out.segment_count
bits = {
    "outgoing_direction": [v.hex() for v in out.outgoing_direction],
    "fresnel_transmission": out.fresnel_transmission.hex(),
    "segment_count": n,
    "segment_directions": [out.segment_directions[i].hex() for i in range(3 * n)],
    "interface_transmittances": [out.interface_transmittances[i].hex() for i in range(n - 1)],
}
lib.LUMICE_ANALYTIC_ReleasePathEvaluation(byref(out))
print(json.dumps(bits))
"""


def test_evaluate_path_is_bit_identical_to_liblumice_analytic() -> None:
    engine = json.loads(_run_child("lumice_testapi", _EVALUATE).stdout)
    analytic = json.loads(_run_child("lumice_analytic", _EVALUATE).stdout)
    assert engine["segment_count"] == 3, engine
    assert engine == analytic


def test_rejected_crystal_warning_reaches_stderr_without_a_callback() -> None:
    proc = _run_child(
        "lumice_testapi",
        """
        c = prism()
        c.face_distance[3] = -1.0  # faces 3 and 6 meet: the closed-form cross section is empty
        rc, out = call(c, [3, 5])
        assert rc == INVALID_CONFIG, rc
        """,
    )
    assert WARNING_TEXT in proc.stderr, (
        "the engine's warning did not reach stderr: is liblumice_analytic's management TU "
        f"(src/analytic/analytic_lib.cpp) linked into the engine?\nstderr: {proc.stderr!r}"
    )


def test_rejected_crystal_warning_reaches_the_engine_log_callback() -> None:
    proc = _run_child(
        "lumice_testapi",
        """
        CB = ctypes.CFUNCTYPE(None, c_int, c_char_p, c_char_p)
        got = []
        cb = CB(lambda level, name, msg: got.append((level, msg.decode())))
        lib.LUMICE_SetLogCallback.restype = None
        lib.LUMICE_SetLogCallback.argtypes = [CB]
        lib.LUMICE_SetLogCallback(cb)
        c = prism()
        c.face_distance[3] = -1.0
        rc, out = call(c, [3, 5])
        assert rc == INVALID_CONFIG, rc
        lib.LUMICE_SetLogCallback(CB())
        print(json.dumps(got))
        """,
    )
    got = json.loads(proc.stdout)
    assert any(level == LUMICE_LOG_WARNING and WARNING_TEXT in msg for level, msg in got), got
