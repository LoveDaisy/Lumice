"""LUMICE_ANALYTIC_DiscoverComponents' C ABI contract, through ctypes against the build tree.

Discovery itself is checked in-process by test/unit-correctness/analytic/test_discovery.cpp (LI
docs/phase1-math-contract.md section 9.5: every conformance row C15-C21 on LI's own scenes, and the
band of two LI parity fixtures point by point), which compiles the kernel but not the C wrapper.
This file owns what only the wrapper does (doc/analytic-api.md sections 4.3, 4.4, 8.2): the result
block with its component and candidate arrays and the nested FiberResult views, NULL for a trace
not run, the funnel fields, zero meaning "default" in both options blocks, the call errors with
their zero-fill, and the release. One call is compared with LI's parity fixture 3-5-6-7__random
(li_rev bfbd042) end to end, and one sparse-then-dense round trip passes the sparse components as
extra seeds, the calling pattern the header prescribes for densification.

Each case runs in a child interpreter so a crash in the library is a failed case, not a dead
pytest. Needs a shared build (`./scripts/build.sh -sj release`); skipped when there is none, or in
a CUDA configure, which does not produce the library. Free of test/e2e's helpers and of numpy.

symmetry_semantics: none — every call discovers one concrete face sequence (doc/analytic-api.md
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
# scenes the cases discover on, and helpers.
_PRELUDE = textwrap.dedent(
    """
    import ctypes, math, sys
    from ctypes import POINTER, Structure, byref, c_double, c_int, c_uint32, c_void_p, sizeof

    class Crystal(Structure):
        _fields_ = [("kind", c_int), ("height", c_double), ("face_distance", c_double * 6),
                    ("upper_h", c_double), ("lower_h", c_double),
                    ("upper_wedge_deg", c_double), ("lower_wedge_deg", c_double)]

    class Continuation(Structure):
        _fields_ = [("seed_residual_tolerance", c_double), ("step_initial", c_double), ("step_min", c_double),
                    ("step_max", c_double), ("max_accepted_steps", c_int), ("closure_min_steps", c_int),
                    ("closure_pose_tolerance", c_double)]

    class FiberResult(Structure):
        _fields_ = [("struct_size", c_uint32), ("status", c_int), ("reason", c_int), ("pose_count", c_int),
                    ("poses", POINTER(c_double)), ("crystal_frame_sun_directions", POINTER(c_double)),
                    ("arclength_increments", POINTER(c_double)), ("residual_norms", POINTER(c_double)),
                    ("tangents", POINTER(c_double)), ("storage", c_void_p)]

    class DiscoveryProblem(Structure):
        _fields_ = [("faces", POINTER(c_int)), ("face_count", c_int), ("refractive_index", c_double),
                    ("incident_direction", c_double * 3), ("target_direction", c_double * 3),
                    ("extra_seeds", POINTER(c_double)), ("extra_seed_count", c_int)]

    class DiscoveryOptions(Structure):
        _fields_ = [("sample_count", c_int), ("band_half_width", c_double), ("cluster_radius", c_double),
                    ("distance_threshold", c_double)]

    class Component(Structure):
        _fields_ = [("kind", c_int), ("seed", c_double * 9), ("forward", POINTER(FiberResult)),
                    ("backward", POINTER(FiberResult))]

    class Incomplete(Structure):
        _fields_ = [("cause", c_int), ("seed", c_double * 9), ("forward", POINTER(FiberResult)),
                    ("backward", POINTER(FiberResult))]

    class DiscoveryResult(Structure):
        _fields_ = [("struct_size", c_uint32), ("completeness", c_int),
                    ("component_count", c_int), ("components", POINTER(Component)),
                    ("incomplete_count", c_int), ("incomplete", POINTER(Incomplete)),
                    ("pool_count", c_int), ("extra_seed_count", c_int), ("raw_cluster_count", c_int),
                    ("admissible_count", c_int), ("dedup_merged", c_int), ("arc_stitched", c_int),
                    ("arc_backward_failed", c_int), ("arc_backward_closed_anomaly", c_int),
                    ("incomplete_unnamed_event", c_int), ("incomplete_not_converged", c_int),
                    ("storage", c_void_p)]

    OK, NULL_ARG, INVALID_VALUE, INVALID_CONFIG = 0, 1, 2, 3
    COMPLETE, UNKNOWN = 0, 1
    CLOSED_KIND, ARC_KIND = 0, 1
    NOT_CONVERGED = 3
    CLOSED, EVENT, NUMERICAL, BUDGET = 0, 1, 2, 3
    R_CLOSED, R_TIR = 0, 100

    lib = ctypes.CDLL(LIB)
    assert lib.LUMICE_ANALYTIC_GetApiVersion() >= 4
    lib.LUMICE_ANALYTIC_DiscoverComponents.restype = c_int
    lib.LUMICE_ANALYTIC_DiscoverComponents.argtypes = [c_void_p, c_void_p, c_void_p, c_void_p, c_void_p]
    lib.LUMICE_ANALYTIC_ReleaseDiscoveryResult.restype = None
    lib.LUMICE_ANALYTIC_ReleaseDiscoveryResult.argtypes = [c_void_p]

    def prism():
        c = Crystal(kind=0, height=1.0)
        for i in range(6):
            c.face_distance[i] = 1.0
        return c

    # LI parity fixtures (li_rev bfbd042): the canonical sun and three targets on the regular prism.
    SUN = [-0.9659258262890683, -0.0, -0.25881904510252074]
    T3567 = [0.6275025327724766, -5.551115123125783e-17, 0.7786145203912691]   # 3-5-6-7__random
    T35 = [-0.9112582539764014, 8.326672684688674e-17, 0.41183539741003355]    # 3-5__random
    T35_EDGE = [-0.8810956222125053, 0.0, 0.4729381614100923]                  # 3-5__near_boundary
    # LI's seed of the one component 3-5-6-7__random finds.
    LI_SEED_3567 = [0.9763888040245257, -0.17498786475368247, 0.12666550660895576, -0.15685644806433433,
                    -0.1711426278424896, 0.9726799348376668, -0.1485293172279377, -0.9695820997337532,
                    -0.19454972063636772]

    KEEP = []  # keeps each call's arrays alive

    def problem(faces, target, extra=(), n=1.31, incident=SUN, **over):
        fa = (c_int * len(faces))(*faces) if faces else None
        flat = [x for m in extra for x in m]
        ea = (c_double * len(flat))(*flat) if flat else None
        KEEP.extend([fa, ea])
        return DiscoveryProblem(fa, over.get("face_count", len(faces)), n, (c_double * 3)(*incident),
                                (c_double * 3)(*target), ea, over.get("extra_seed_count", len(extra)))

    def result(size=None):
        r = DiscoveryResult()
        ctypes.memset(ctypes.addressof(r), 0x5A, sizeof(r))  # garbage the call must overwrite
        r.struct_size = sizeof(DiscoveryResult) if size is None else size
        return r

    def addr(obj):
        return None if obj is None else ctypes.addressof(obj)

    def discover(p, options=None, continuation=None, crystal=None, out=None):
        out = result() if out is None else out
        crystal = prism() if crystal is None else crystal
        rc = lib.LUMICE_ANALYTIC_DiscoverComponents(addr(crystal), addr(p), addr(options), addr(continuation),
                                                    addr(out))
        return rc, out

    def release(r):
        lib.LUMICE_ANALYTIC_ReleaseDiscoveryResult(addr(r))

    def zeroed_after_size(r):
        raw = ctypes.string_at(ctypes.addressof(r), sizeof(r))
        return all(b == 0 for b in raw[4:])

    def distance(a, b):
        # SO(3) geodesic angle between two row-major rotations.
        tr = sum(a[3 * k + i] * b[3 * k + i] for i in range(3) for k in range(3))
        return math.acos(max(-1.0, min(1.0, (tr - 1.0) / 2.0)))

    def check_trace(t):
        # A nested FiberResult is a view: the library's struct_size, no storage of its own, and the
        # usual arrays with u = R^T (-s).
        assert t.struct_size == sizeof(FiberResult) and not t.storage
        n = t.pose_count
        assert n >= 1 and t.poses and t.residual_norms and t.tangents and t.crystal_frame_sun_directions
        for i in range(n):
            R = [t.poses[9 * i + k] for k in range(9)]
            u = [-(R[0 * 3 + k] * SUN[0] + R[1 * 3 + k] * SUN[1] + R[2 * 3 + k] * SUN[2]) for k in range(3)]
            assert all(abs(u[k] - t.crystal_frame_sun_directions[3 * i + k]) < 1e-15 for k in range(3))
            assert t.residual_norms[i] <= 1e-11
        return sum(t.arclength_increments[i] for i in range(n - 1)) if n > 1 else 0.0

    def check_funnel(r):
        assert r.admissible_count == r.dedup_merged + r.component_count + r.incomplete_count
        arcs = sum(r.components[i].kind == ARC_KIND for i in range(r.component_count))
        assert r.arc_stitched == arcs
        assert r.incomplete_count == (r.arc_backward_failed + r.arc_backward_closed_anomaly
                                      + r.incomplete_unnamed_event + r.incomplete_not_converged)
        assert r.completeness == (COMPLETE if r.incomplete_count == 0 else UNKNOWN)
        assert bool(r.components) == (r.component_count > 0) and bool(r.incomplete) == (r.incomplete_count > 0)
    """
)


def _run_child(body: str) -> subprocess.CompletedProcess[str]:
    code = f"LIB = {str(_find())!r}\n" + _PRELUDE + "\n" + textwrap.dedent(body)
    proc = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True, check=False)
    assert proc.returncode == 0, f"child failed ({proc.returncode}):\n{proc.stdout}\n{proc.stderr}"
    return proc


def test_one_call_matches_the_li_parity_fixture_and_the_block_is_views() -> None:
    _run_child(
        """
        # LI 3-5-6-7__random at its N = 1e5: pool 42, 6 clusters, all admissible, 5 folded, one arc cut by
        # exit TIR at both ends.
        rc, r = discover(problem([3, 5, 6, 7], T3567), DiscoveryOptions(sample_count=100000))
        assert rc == OK, rc
        check_funnel(r)
        got = (r.pool_count, r.extra_seed_count, r.raw_cluster_count, r.admissible_count, r.dedup_merged,
               r.arc_stitched, r.component_count, r.incomplete_count, r.completeness)
        assert got == (42, 0, 6, 6, 5, 1, 1, 0, COMPLETE), got
        c = r.components[0]
        assert c.kind == ARC_KIND and c.forward and c.backward
        f, b = c.forward.contents, c.backward.contents
        assert (f.status, f.reason, b.status, b.reason) == (EVENT, R_TIR, EVENT, R_TIR)
        length = check_trace(f) + check_trace(b)
        assert abs(length - 2.467168223892592) < 2e-3 * 2.467168223892592, length
        seed = list(c.seed)
        assert distance(seed, LI_SEED_3567) < 1e-9, distance(seed, LI_SEED_3567)
        # Both traces start at the seed.
        assert max(abs(f.poses[k] - seed[k]) for k in range(9)) < 1e-15
        assert max(abs(b.poses[k] - seed[k]) for k in range(9)) < 1e-15

        # A closed component: no backward trace.
        rc, closed = discover(problem([3, 5], T35), DiscoveryOptions(sample_count=100000))
        assert rc == OK and closed.component_count == 1, (rc, closed.component_count)
        k = closed.components[0]
        assert k.kind == CLOSED_KIND and k.forward and not k.backward
        assert (k.forward.contents.status, k.forward.contents.reason) == (CLOSED, R_CLOSED)
        assert abs(check_trace(k.forward.contents) - 5.670160732846) < 2e-3 * 5.670160732846

        for x in (r, closed):
            release(x)
            assert x.struct_size == sizeof(DiscoveryResult) and zeroed_after_size(x)
            release(x)  # a no-op on a zeroed struct
        release(None)
        """
    )


def test_sparse_components_as_extra_seeds_of_a_denser_call() -> None:
    _run_child(
        """
        # The densification pattern of the header: the sparse call's component seeds become the dense
        # call's extra seeds. Each extra seed is only a Gauss-Newton start: the dense band is unchanged,
        # and every sparse component is on some dense curve.
        rc, sparse = discover(problem([3, 5], T35_EDGE), DiscoveryOptions(sample_count=100000))
        assert rc == OK and sparse.component_count >= 1, rc
        seeds = [list(sparse.components[i].seed) for i in range(sparse.component_count)]
        rc, cold = discover(problem([3, 5], T35_EDGE))
        rc2, warm = discover(problem([3, 5], T35_EDGE, extra=seeds))
        assert rc == OK and rc2 == OK
        for x in (sparse, cold, warm):
            check_funnel(x)
        assert warm.extra_seed_count == len(seeds) and cold.extra_seed_count == 0
        assert warm.pool_count == cold.pool_count > sparse.pool_count
        assert warm.component_count >= sparse.component_count

        def curve(t):
            return [[t.poses[9 * i + k] for k in range(9)] for i in range(t.pose_count)]

        for s in seeds:
            near = min(distance(s, p) for i in range(warm.component_count)
                       for t in (warm.components[i].forward, warm.components[i].backward) if t
                       for p in curve(t.contents))
            assert near < 0.08, near
        for x in (sparse, cold, warm):
            release(x)
        """
    )


def test_an_empty_band_is_a_complete_empty_result() -> None:
    _run_child(
        """
        # Inside the 3-5 minimum deviation (LI's dark pixel, row 40): no candidate at all.
        rc, r = discover(problem([3, 5], [-0.9936161962973833, -0.010323239346674772, 0.11234004266027031]))
        assert rc == OK, rc
        check_funnel(r)
        assert (r.pool_count, r.raw_cluster_count, r.admissible_count, r.component_count) == (0, 0, 0, 0)
        assert r.completeness == COMPLETE and not r.components and not r.incomplete
        release(r)
        assert zeroed_after_size(r)
        """
    )


def test_options_zero_means_default_and_the_continuation_governs_every_trace() -> None:
    _run_child(
        """
        p = problem([3, 5, 6, 7], T3567)
        rc, a = discover(p)
        rc2, b = discover(p, DiscoveryOptions(), Continuation())
        assert rc == OK and rc2 == OK
        fields = [name for name, _ in DiscoveryResult._fields_ if name not in ("components", "incomplete", "storage")]
        assert [getattr(a, f) for f in fields] == [getattr(b, f) for f in fields]
        assert a.pool_count > 42  # the default N is 1e6, not the fixture's 1e5
        # A wider band feeds more events.
        rc, wide = discover(p, DiscoveryOptions(band_half_width=math.radians(0.4)))
        assert rc == OK and wide.pool_count > a.pool_count

        # A starving continuation budget: no component, every candidate NOT_CONVERGED, no backward trace.
        rc, starved = discover(p, None, Continuation(max_accepted_steps=5))
        assert rc == OK, rc
        check_funnel(starved)
        assert starved.component_count == 0 and starved.incomplete_count >= 1
        assert starved.completeness == UNKNOWN
        for i in range(starved.incomplete_count):
            c = starved.incomplete[i]
            assert c.cause == NOT_CONVERGED and c.forward and not c.backward
            assert (c.forward.contents.status, c.forward.contents.pose_count) == (BUDGET, 6)
        for x in (a, b, wide, starved):
            release(x)
        """
    )


def test_call_errors_zero_fill_the_result() -> None:
    _run_child(
        """
        good = problem([3, 5], T35)
        not_rotation = [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.5]
        cases = [
            ("NULL crystal", NULL_ARG, lambda out: lib.LUMICE_ANALYTIC_DiscoverComponents(
                None, addr(good), None, None, addr(out))),
            ("NULL problem", NULL_ARG, lambda out: lib.LUMICE_ANALYTIC_DiscoverComponents(
                addr(prism()), None, None, None, addr(out))),
            ("faces NULL", NULL_ARG, lambda out: discover(problem([], T35, face_count=2), out=out)[0]),
            ("extra_seeds NULL", NULL_ARG, lambda out: discover(problem([3, 5], T35, extra_seed_count=1), out=out)[0]),
            ("one face", INVALID_VALUE, lambda out: discover(problem([3], T35), out=out)[0]),
            ("face 13 on a prism", INVALID_VALUE, lambda out: discover(problem([3, 13], T35), out=out)[0]),
            ("index", INVALID_VALUE, lambda out: discover(problem([3, 5], T35, n=float("nan")), out=out)[0]),
            ("non-unit target", INVALID_VALUE, lambda out: discover(problem([3, 5], [0.6, 0.8, 1e-4]), out=out)[0]),
            ("target on the sun", INVALID_VALUE, lambda out: discover(problem([3, 5], SUN), out=out)[0]),
            ("target opposite", INVALID_VALUE, lambda out: discover(problem([3, 5], [-x for x in SUN]), out=out)[0]),
            ("negative extra count", INVALID_VALUE,
             lambda out: discover(problem([3, 5], T35, extra_seed_count=-1), out=out)[0]),
            ("extra seed not a rotation", INVALID_VALUE,
             lambda out: discover(problem([3, 5], T35, extra=[not_rotation]), out=out)[0]),
            ("negative sample_count", INVALID_VALUE,
             lambda out: discover(good, DiscoveryOptions(sample_count=-1), out=out)[0]),
            ("negative eta", INVALID_VALUE,
             lambda out: discover(good, DiscoveryOptions(distance_threshold=-0.08), out=out)[0]),
            ("NaN band", INVALID_VALUE,
             lambda out: discover(good, DiscoveryOptions(band_half_width=float("nan")), out=out)[0]),
            ("bad continuation", INVALID_VALUE,
             lambda out: discover(good, None, Continuation(step_min=1.0), out=out)[0]),
            ("crystal field", INVALID_VALUE,
             lambda out: discover(good, crystal=Crystal(kind=0, height=1.0, upper_h=0.5), out=out)[0]),
            ("empty crystal", INVALID_CONFIG,
             lambda out: discover(good, crystal=Crystal(kind=0, height=0.0), out=out)[0]),
        ]
        for name, expected, call in cases:
            out = result()
            rc = call(out)
            assert rc == expected, (name, rc)
            assert out.struct_size == sizeof(DiscoveryResult) and zeroed_after_size(out), name
            release(out)

        # A struct_size below this struct's: ERR_INVALID_VALUE, and only the declared bytes are touched.
        small = result(size=8)
        rc = lib.LUMICE_ANALYTIC_DiscoverComponents(addr(prism()), addr(good), None, None, addr(small))
        assert rc == INVALID_VALUE, rc
        raw = ctypes.string_at(ctypes.addressof(small), sizeof(small))
        assert raw[4:8] == bytes(4) and all(b == 0x5A for b in raw[8:]), raw[:16]
        assert lib.LUMICE_ANALYTIC_DiscoverComponents(addr(prism()), addr(good), None, None, None) == NULL_ARG
        """
    )
