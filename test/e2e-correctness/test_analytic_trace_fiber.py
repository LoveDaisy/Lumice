"""LUMICE_ANALYTIC_TraceFiber / TraceFiberBatch's C ABI contract, through ctypes against the build tree.

The continuation itself is checked in-process by test/unit-correctness/analytic/
(test_fiber_continuation.cpp, test_path_fiber.cpp: LI's conformance cases C01-C12 and two LI
parity-fixture pins), which compile the kernel but not the C wrapper. This file owns what only the
wrapper does (doc/analytic-api.md sections 4.3, 4.4, 8.2): one batch call over independent problems,
element-level versus call-level errors, per-problem initial_tangent_sign, the N = 0 / N = 1 pointer
rules, u = R^T (-incident), the result block and its release, the struct_size stride of the batch,
and the zero-fill on every call error.

Each case runs in a child interpreter so a crash in the library is a failed case, not a dead
pytest. Needs a shared build (`./scripts/build.sh -sj release`); skipped when there is none, or in
a CUDA configure, which does not produce the library. Free of test/e2e's helpers and of numpy.

symmetry_semantics: none — every problem traces one concrete face sequence (doc/analytic-api.md
section 3); nothing here compares face sequences across a symmetry.
"""
from __future__ import annotations

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


# The bindings every child starts with: the structs field for field with lumice_analytic.h, the
# problems the cases trace, and helpers.
_PRELUDE = textwrap.dedent(
    """
    import ctypes, json, math, sys
    from ctypes import POINTER, Structure, byref, c_double, c_int, c_uint32, c_void_p, sizeof

    class Crystal(Structure):
        _fields_ = [("kind", c_int), ("height", c_double), ("face_distance", c_double * 6),
                    ("upper_h", c_double), ("lower_h", c_double),
                    ("upper_wedge_deg", c_double), ("lower_wedge_deg", c_double)]

    class FiberProblem(Structure):
        _fields_ = [("faces", POINTER(c_int)), ("face_count", c_int), ("refractive_index", c_double),
                    ("incident_direction", c_double * 3), ("target_direction", c_double * 3),
                    ("seed_pose", c_double * 9), ("initial_tangent_sign", c_int)]

    class Options(Structure):
        _fields_ = [("seed_residual_tolerance", c_double), ("step_initial", c_double), ("step_min", c_double),
                    ("step_max", c_double), ("max_accepted_steps", c_int), ("closure_min_steps", c_int),
                    ("closure_pose_tolerance", c_double)]

    class FiberResult(Structure):
        _fields_ = [("struct_size", c_uint32), ("status", c_int), ("reason", c_int), ("pose_count", c_int),
                    ("poses", POINTER(c_double)), ("crystal_frame_sun_directions", POINTER(c_double)),
                    ("arclength_increments", POINTER(c_double)), ("residual_norms", POINTER(c_double)),
                    ("tangents", POINTER(c_double)), ("storage", c_void_p)]

    OK, NULL_ARG, INVALID_VALUE, INVALID_CONFIG = 0, 1, 2, 3
    CLOSED, EVENT, NUMERICAL, BUDGET = 0, 1, 2, 3
    R_CLOSED, R_TIR, R_INFEASIBLE, R_INVALID_INPUT, R_STEP_BUDGET = 0, 100, 102, 204, 300

    lib = ctypes.CDLL(LIB)
    lib.LUMICE_ANALYTIC_TraceFiberBatch.restype = c_int
    lib.LUMICE_ANALYTIC_TraceFiberBatch.argtypes = [POINTER(Crystal), POINTER(FiberProblem), c_int,
                                                    POINTER(Options), c_void_p]
    lib.LUMICE_ANALYTIC_TraceFiber.restype = c_int
    lib.LUMICE_ANALYTIC_TraceFiber.argtypes = [POINTER(Crystal), POINTER(FiberProblem), POINTER(Options),
                                               POINTER(FiberResult)]
    lib.LUMICE_ANALYTIC_ReleaseFiberResult.restype = None
    lib.LUMICE_ANALYTIC_ReleaseFiberResult.argtypes = [c_void_p]

    def prism(height=1.0):
        c = Crystal(kind=0, height=height)
        for i in range(6):
            c.face_distance[i] = 1.0
        return c

    # LI parity fixtures 3-5__random and 3-5-6-7__random (li_rev bfbd042): the canonical sun, a seed
    # on each fiber and its target. 3-5 closes; 3-5-6-7 is an arc cut by exit TIR at both ends.
    SUN = [-0.9659258262890683, -0.0, -0.25881904510252074]
    P35 = dict(faces=[3, 5], target=[-0.9112582539764014, 8.326672684688674e-17, 0.41183539741003355],
               seed=[0.3258565506826661, -0.8542836236664049, -0.4049901217469277, 0.17797600617978254,
                     -0.36528256190089714, 0.9137248990781696, -0.9285160470349928, -0.36982176829726143,
                     0.03301227183938536])
    P3567 = dict(faces=[3, 5, 6, 7], target=[0.6275025327724766, -5.551115123125783e-17, 0.7786145203912691],
                 seed=[0.9739245884679638, -0.15831553091898734, 0.16250258043288573, -0.1957670169785059,
                       -0.22445280180182878, 0.9546183608262762, -0.1146567531410371, -0.9615389396343489,
                       -0.24959306185470612])

    KEEP = []  # keeps each problem's faces array alive for the call

    def problem(spec, sign=1, **over):
        faces = over.get("faces", spec["faces"])
        fa = (c_int * len(faces))(*faces)
        KEEP.append(fa)
        return FiberProblem(fa if faces else None, over.get("face_count", len(faces)), over.get("n", 1.31),
                            (c_double * 3)(*over.get("incident", SUN)), (c_double * 3)(*over.get("target", spec["target"])),
                            (c_double * 9)(*over.get("seed", spec["seed"])), sign)

    def results(count, size=None, fill=0x5A):
        arr = (FiberResult * count)()
        ctypes.memset(arr, fill, sizeof(arr))  # garbage the call must overwrite
        for r in arr:
            r.struct_size = sizeof(FiberResult) if size is None else size
        return arr

    def batch(problems, options=None, crystal=None, out=None):
        arr = (FiberProblem * len(problems))(*problems)
        out = results(len(problems)) if out is None else out
        rc = lib.LUMICE_ANALYTIC_TraceFiberBatch(byref(crystal or prism()), arr, len(problems),
                                                 byref(options) if options is not None else None, addr(out))
        return rc, out

    def addr(obj):
        return None if obj is None else ctypes.addressof(obj)

    def release(r):
        lib.LUMICE_ANALYTIC_ReleaseFiberResult(addr(r))

    def zeroed_after_size(r):
        raw = ctypes.string_at(ctypes.addressof(r), sizeof(r))
        return all(b == 0 for b in raw[4:])

    def arrays(r):
        n = r.pose_count
        take = lambda p, k: [p[i] for i in range(k)] if p else None
        return dict(poses=take(r.poses, 9 * n), sun=take(r.crystal_frame_sun_directions, 3 * n),
                    inc=take(r.arclength_increments, max(n - 1, 0)), res=take(r.residual_norms, n),
                    tan=take(r.tangents, 3 * n))

    def check_result(r, incident=SUN):
        # Shapes, pointer rules and u = R^T (-s) for any result.
        n = r.pose_count
        a = arrays(r)
        if n == 0:
            assert not (r.poses or r.crystal_frame_sun_directions or r.arclength_increments or r.residual_norms
                        or r.tangents or r.storage), "N = 0 carries no arrays"
            return a
        assert r.storage and r.poses and r.crystal_frame_sun_directions and r.residual_norms and r.tangents
        assert (r.arclength_increments is not None and bool(r.arclength_increments)) == (n > 1), n
        for i in range(n):
            R = a["poses"][9 * i:9 * i + 9]
            u = [-(R[0 * 3 + k] * incident[0] + R[1 * 3 + k] * incident[1] + R[2 * 3 + k] * incident[2])
                 for k in range(3)]
            assert all(abs(u[k] - a["sun"][3 * i + k]) < 1e-15 for k in range(3)), (i, u)
            t = a["tan"][3 * i:3 * i + 3]
            assert abs(math.sqrt(sum(x * x for x in t)) - 1.0) < 1e-14
            assert a["res"][i] <= 1e-11
        if n > 1:
            assert all(x >= 0.0 for x in a["inc"])
        return a
    """
)


def _run_child(body: str) -> subprocess.CompletedProcess[str]:
    code = f"LIB = {str(_find())!r}\n" + _PRELUDE + "\n" + textwrap.dedent(body)
    proc = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True, check=False)
    assert proc.returncode == 0, f"child failed ({proc.returncode}):\n{proc.stdout}\n{proc.stderr}"
    return proc


def test_one_batch_traces_every_problem_independently() -> None:
    proc = _run_child(
        """
        bad_target = [0.6, 0.8, 1e-4]  # not unit
        nan_seed = list(P35["seed"])
        nan_seed[4] = float("nan")
        problems = [
            problem(P35, 1),
            problem(P35, -1),
            problem(P35, 1, target=bad_target),        # element-level: non-unit target
            problem(P3567, 1),
            problem(P3567, -1),
            problem(P35, 1, faces=[3, 13]),            # element-level: face 13 is not on a prism
            problem(P35, 1, seed=nan_seed),            # element-level (LI C10): non-finite seed
            problem(P35, 5),                           # element-level: initial_tangent_sign
        ]
        rc, out = batch(problems)
        assert rc == OK, rc
        got = [(r.status, r.reason, r.pose_count) for r in out]

        # 3-5: one closed loop, both orientations; the same loop, samples in reverse order.
        fwd, rev = check_result(out[0]), check_result(out[1])
        assert out[0].status == CLOSED and out[0].reason == R_CLOSED, got
        assert out[1].status == CLOSED and out[1].reason == R_CLOSED, got
        assert all(abs(fwd["tan"][k] + rev["tan"][k]) < 1e-15 for k in range(3)), (fwd["tan"][:3], rev["tan"][:3])
        len_f, len_r = sum(fwd["inc"]), sum(rev["inc"])
        assert abs(len_f - 5.670160732846) < 2e-3 * 5.670160732846 and abs(len_f - len_r) < 2e-3 * len_f, (len_f, len_r)
        n = out[0].pose_count
        assert max(abs(fwd["poses"][9 * (n - 1) + k] - P35["seed"][k]) for k in range(9)) < 1e-12  # back at the seed

        # 3-5-6-7: an arc cut by exit TIR at both ends; each orientation ends on its own event.
        for r in (out[3], out[4]):
            check_result(r)
            assert (r.status, r.reason) == (EVENT, R_TIR), got
            assert r.pose_count > 1

        # Element-level input errors: that element only, the others traced as usual.
        for r in (out[2], out[5], out[6], out[7]):
            assert (r.status, r.reason, r.pose_count) == (NUMERICAL, R_INVALID_INPUT, 0), got
            check_result(r)

        for r in out:
            release(r)
            assert r.struct_size == sizeof(FiberResult) and zeroed_after_size(r)
            release(r)  # zeroed: no-op
        release(None)
        print(json.dumps({"results": got, "loop": len_f}))
        """
    )
    assert "results" in proc.stdout


def test_n0_and_n1_results_follow_the_pointer_rules() -> None:
    _run_child(
        """
        # N = 0: a seed off the path's domain (LI tests/test_optics.py: exit TIR at the seed; the
        # 60-degree prism's minimum-deviation sun, target e3).
        ext = math.asin(1.31 * 0.5)
        mindev = [-math.cos(ext), math.sin(ext), 0.0]
        w = [0.5447316801391622, -1.506228738763967, -1.190186580432801]
        th = math.sqrt(sum(x * x for x in w))
        k = [x / th for x in w]
        c, s = math.cos(th), math.sin(th)
        rot = [c + k[0] * k[0] * (1 - c), k[0] * k[1] * (1 - c) - k[2] * s, k[0] * k[2] * (1 - c) + k[1] * s,
               k[1] * k[0] * (1 - c) + k[2] * s, c + k[1] * k[1] * (1 - c), k[1] * k[2] * (1 - c) - k[0] * s,
               k[2] * k[0] * (1 - c) - k[1] * s, k[2] * k[1] * (1 - c) + k[0] * s, c + k[2] * k[2] * (1 - c)]
        rc, out = batch([problem(P35, 1, incident=mindev, target=[0.0, 0.0, 1.0], seed=rot)])
        assert rc == OK and (out[0].status, out[0].reason, out[0].pose_count) == (EVENT, R_TIR, 0)
        check_result(out[0], mindev)

        # N = 1: seed the 3-5-6-7 arc at its last accepted pose, next to the exit TIR boundary; the
        # orientation that heads into the boundary ends on its first trial with the seed alone.
        rc, arc = batch([problem(P3567, 1)])
        end = arrays(arc[0])["poses"][9 * (arc[0].pose_count - 1):]
        rc, out = batch([problem(P3567, 1, seed=end), problem(P3567, -1, seed=end)])
        assert rc == OK
        ones = [r for r in out if r.pose_count == 1]
        assert len(ones) == 1, [(r.status, r.reason, r.pose_count) for r in out]
        one = ones[0]
        assert (one.status, one.reason) == (EVENT, R_TIR)
        check_result(one)
        assert not one.arclength_increments and one.storage
        for r in list(out) + list(arc):
            release(r)
        """
    )


def test_trace_fiber_is_the_batch_of_one() -> None:
    _run_child(
        """
        p = problem(P3567, -1)
        single = FiberResult()
        single.struct_size = sizeof(FiberResult)
        assert lib.LUMICE_ANALYTIC_TraceFiber(byref(prism()), byref(p), None, byref(single)) == OK
        rc, out = batch([p])
        assert rc == OK
        assert (single.status, single.reason, single.pose_count) == (out[0].status, out[0].reason, out[0].pose_count)
        assert arrays(single) == arrays(out[0])
        release(single)
        release(out[0])
        """
    )


def test_options_zero_means_default_and_fields_take_effect() -> None:
    _run_child(
        """
        rc, a = batch([problem(P35, 1)])
        rc, b = batch([problem(P35, 1)], options=Options())
        assert arrays(a[0]) == arrays(b[0])  # all-zero options == NULL options
        o = Options()
        o.step_initial, o.step_max = 0.02, 0.02
        rc, c = batch([problem(P35, 1)], options=o)
        assert rc == OK and c[0].status == CLOSED and c[0].pose_count > a[0].pose_count, (c[0].pose_count, a[0].pose_count)
        o = Options()
        o.max_accepted_steps = 5
        rc, d = batch([problem(P35, 1)], options=o)
        assert rc == OK and (d[0].status, d[0].reason, d[0].pose_count) == (BUDGET, R_STEP_BUDGET, 6)
        check_result(d[0])
        for r in (a[0], b[0], c[0], d[0]):
            release(r)
        """
    )


def test_call_errors_zero_fill_every_element() -> None:
    _run_child(
        """
        def expect_all_zeroed(out, name):
            for r in out:
                assert r.struct_size == sizeof(FiberResult) and zeroed_after_size(r), name

        opts = []
        for field, value in (("step_initial", -0.1), ("step_min", float("nan")), ("step_min", 0.05),
                             ("max_accepted_steps", -1), ("closure_pose_tolerance", float("inf"))):
            o = Options()
            setattr(o, field, value)
            opts.append((field, value, o))
        for field, value, o in opts:  # an invalid options block (0.05 > the default initial step)
            rc, out = batch([problem(P35), problem(P35)], options=o)
            assert rc == INVALID_VALUE, (field, value, rc)
            expect_all_zeroed(out, field)

        rc, out = batch([problem(P35), problem(P35)], crystal=prism(height=0.0))
        assert rc == INVALID_CONFIG, rc
        expect_all_zeroed(out, "zero-volume prism")

        # faces NULL with face_count > 0 is the caller's memory bug: call-level, whichever element.
        rc, out = batch([problem(P35), problem(P35, faces=[], face_count=2)])
        assert rc == NULL_ARG, rc
        expect_all_zeroed(out, "null faces")

        out = results(2)
        arr = (FiberProblem * 2)(problem(P35), problem(P35))
        assert lib.LUMICE_ANALYTIC_TraceFiberBatch(None, arr, 2, None, addr(out)) == NULL_ARG
        expect_all_zeroed(out, "null crystal")
        out = results(2)
        assert lib.LUMICE_ANALYTIC_TraceFiberBatch(byref(prism()), None, 2, None, addr(out)) == NULL_ARG
        expect_all_zeroed(out, "null problems")
        assert lib.LUMICE_ANALYTIC_TraceFiberBatch(byref(prism()), arr, 2, None, None) == NULL_ARG

        # count < 0: the array's length is unknown, nothing is touched. count == 0: a no-op success.
        for count, want in ((-1, INVALID_VALUE), (0, OK)):
            out = results(2)
            before = ctypes.string_at(ctypes.addressof(out), sizeof(out))
            assert lib.LUMICE_ANALYTIC_TraceFiberBatch(byref(prism()), arr, count, None, addr(out)) == want
            assert ctypes.string_at(ctypes.addressof(out), sizeof(out)) == before, count

        # struct_size smaller than the struct: the stride is unusable, only element 0 is zero-filled.
        out = results(2, size=8)
        rc = lib.LUMICE_ANALYTIC_TraceFiberBatch(byref(prism()), arr, 2, None, addr(out))
        assert rc == INVALID_VALUE, rc
        raw = ctypes.string_at(ctypes.addressof(out), sizeof(out))
        assert raw[4:8] == bytes(4) and raw[8:sizeof(FiberResult)] == bytes([0x5A]) * (sizeof(FiberResult) - 8)

        # Elements disagreeing on struct_size: call-level, every element zero-filled within its size.
        out = results(2)
        out[1].struct_size = sizeof(FiberResult) + 8
        rc = lib.LUMICE_ANALYTIC_TraceFiberBatch(byref(prism()), arr, 2, None, addr(out))
        assert rc == INVALID_VALUE, rc
        assert zeroed_after_size(out[0])
        """
    )


def test_a_newer_callers_larger_struct_sets_the_stride() -> None:
    _run_child(
        """
        # A caller compiled against a header with one more trailing field (section 8.2): the stride
        # is its struct_size, and the field the library does not know stays as the caller left it.
        class Newer(Structure):
            _fields_ = FiberResult._fields_ + [("future", c_double)]
        arr = (Newer * 2)()
        for r in arr:
            r.struct_size = sizeof(Newer)
            r.future = 42.0
        probs = (FiberProblem * 2)(problem(P35, 1), problem(P3567, 1))
        rc = lib.LUMICE_ANALYTIC_TraceFiberBatch(byref(prism()), probs, 2, None, addr(arr))
        assert rc == OK, rc
        assert (arr[0].status, arr[0].reason) == (CLOSED, R_CLOSED)
        assert (arr[1].status, arr[1].reason) == (EVENT, R_TIR)
        assert arr[0].future == 42.0 and arr[1].future == 42.0
        for r in arr:
            release(r)
            assert r.future == 42.0
        """
    )
