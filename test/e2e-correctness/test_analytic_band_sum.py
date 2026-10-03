"""LUMICE_ANALYTIC_BandSum's C ABI contract, through ctypes against the build tree.

The band sum itself is checked in-process by test/unit-correctness/analytic/ (test_band_sum.cpp, and
LiParityBandSum in test_li_parity.cpp on all eleven LI band_sum fixtures), which compile the kernel
but not the C wrapper. This file owns what only the wrapper does (doc/analytic-api.md sections 4.6,
5, 8.2): the problem and pixel-table structs as read through the ABI, the pose-density family
mapping, the pixel status and rank-0 fields of the result, its release, and every call error with
the zero-fill it promises. Two LI fixtures are replayed through the ABI as the value oracle — a
Parry density (the roll half of the density, which only a roll family reads) and a rank-0 path (the
point-mass fields) — so a field wired to the wrong struct member reads as a wrong number, not only
as a wrong shape.

Each case runs in a child interpreter so a crash in the library is a failed case, not a dead
pytest. Needs a shared build (`./scripts/build.sh -sj release`); skipped when there is none, or in
a CUDA configure, which does not produce the library. Free of test/e2e's helpers and of numpy.

symmetry_semantics: none — every problem is one concrete face sequence (doc/analytic-api.md
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
FIXTURES = ROOT / "test" / "fixtures" / "li-parity"

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


# The bindings every child starts with: the structs field for field with lumice_analytic.h, a
# problem built from an LI fixture, and helpers.
_PRELUDE = textwrap.dedent(
    """
    import ctypes, json, math, sys
    from ctypes import POINTER, Structure, byref, c_double, c_int, c_uint32, c_void_p, sizeof

    class Crystal(Structure):
        _fields_ = [("kind", c_int), ("height", c_double), ("face_distance", c_double * 6),
                    ("upper_h", c_double), ("lower_h", c_double),
                    ("upper_wedge_deg", c_double), ("lower_wedge_deg", c_double)]

    class PoseDensity(Structure):
        _fields_ = [("family", c_int), ("zenith_mean_deg", c_double), ("zenith_std_deg", c_double),
                    ("roll_mean_deg", c_double), ("roll_std_deg", c_double)]

    class PixelTable(Structure):
        _fields_ = [("pixel_count", c_int), ("centre", POINTER(c_double)), ("corners", POINTER(c_double)),
                    ("solid_angle", POINTER(c_double))]

    class Problem(Structure):
        _fields_ = [("faces", POINTER(c_int)), ("face_count", c_int), ("refractive_index", c_double),
                    ("incident_direction", c_double * 3), ("sample_count", c_int),
                    ("pose_density", PoseDensity), ("pixels", PixelTable)]

    class BandPixel(Structure):
        _fields_ = [("status", c_int), ("value", c_double), ("delta", c_double), ("delta_lo", c_double),
                    ("delta_hi", c_double), ("k", c_int), ("k_rho_pos", c_int), ("k_eff", c_double)]

    class Result(Structure):
        _fields_ = [("struct_size", c_uint32), ("rank_zero", c_int), ("kept_count", c_int), ("pixel_count", c_int),
                    ("pixels", POINTER(BandPixel)), ("point_mass", c_double), ("point_mass_error", c_double),
                    ("point_mass_method", c_int), ("point_mass_pixel", c_int), ("storage", c_void_p)]

    OK, NULL_ARG, INVALID_VALUE, INVALID_CONFIG = 0, 1, 2, 3
    PX_OK, PX_SINGULAR, PX_POINT_MASS = 0, 1, 2
    LATTICE_MEAN, TWIST_AVERAGE = 0, 1
    FAMILY = {"random": 0, "column": 1, "plate": 2, "parry": 3, "lowitz": 4}
    STATUS = {"ok": PX_OK, "singular": PX_SINGULAR, "point_mass": PX_POINT_MASS}

    lib = ctypes.CDLL(LIB)
    lib.LUMICE_ANALYTIC_GetApiVersion.restype = c_int
    assert lib.LUMICE_ANALYTIC_GetApiVersion() == 14, "these bindings require lumice_analytic.h version 14"
    lib.LUMICE_ANALYTIC_BandSum.restype = c_int
    lib.LUMICE_ANALYTIC_BandSum.argtypes = [POINTER(Crystal), POINTER(Problem), c_void_p]
    lib.LUMICE_ANALYTIC_ReleaseBandSumResult.restype = None
    lib.LUMICE_ANALYTIC_ReleaseBandSumResult.argtypes = [c_void_p]

    KEEP = []  # keeps the problem's arrays alive for the call

    def doubles(values):
        arr = (c_double * len(values))(*values)
        KEEP.append(arr)
        return arr

    def load(name):
        with open(FIXTURES + "/" + name + ".json", encoding="utf-8") as fp:
            return json.load(fp)

    def crystal_of(f):
        c = f["input"]["crystal"]
        out = Crystal(kind={"prism": 0, "pyramid": 1}[c["kind"]], height=c["height"], upper_h=c["upper_h"],
                      lower_h=c["lower_h"], upper_wedge_deg=c["upper_wedge_deg"], lower_wedge_deg=c["lower_wedge_deg"])
        for i in range(6):
            out.face_distance[i] = c["face_distance"][i]
        return out

    def problem_of(f):
        i = f["input"]
        faces = (c_int * len(i["faces"]))(*i["faces"])
        KEEP.append(faces)
        d = i["pose_density"]
        density = PoseDensity(FAMILY[d["family"]], d.get("zenith_mean_deg", 0.0), d.get("zenith_std_deg", 0.0),
                              d.get("roll_mean_deg", 0.0), d.get("roll_std_deg", 0.0))
        px = i["pixels"]
        flat = lambda rows: [x for row in rows for x in (row if not isinstance(row[0], list) else
                                                         [y for corner in row for y in corner])]
        table = PixelTable(len(px["centre"]), doubles(flat(px["centre"])), doubles(flat(px["corners"])),
                           doubles(px["solid_angle"]))
        return Problem(faces, len(i["faces"]), i["refractive_index"], (c_double * 3)(*i["incident_direction"]),
                       i["sample"]["n"], density, table)

    def result(size=None):
        r = Result()
        ctypes.memset(byref(r), 0x5A, sizeof(r))  # garbage the call must overwrite
        r.struct_size = sizeof(Result) if size is None else size
        return r

    def call(crystal, prob, out=None):
        out = result() if out is None else out
        rc = lib.LUMICE_ANALYTIC_BandSum(byref(crystal) if crystal is not None else None,
                                         byref(prob) if prob is not None else None, ctypes.addressof(out))
        return rc, out

    def zeroed_after_size(r):
        # Within the bytes the caller declared: a caller older than the library keeps the rest.
        raw = ctypes.string_at(ctypes.addressof(r), min(r.struct_size, sizeof(r)))
        return all(b == 0 for b in raw[4:])

    def close(got, expected, rel, allowance=0.0):
        return abs(got - expected) <= rel * abs(expected) + allowance
    """
)


def _run(body: str) -> None:
    lib = _find()
    script = f"LIB = {str(lib)!r}\nFIXTURES = {str(FIXTURES)!r}\n" + _PRELUDE + textwrap.dedent(body)
    proc = subprocess.run([sys.executable, "-c", script], capture_output=True, text=True, timeout=120)
    assert proc.returncode == 0, f"child failed (rc={proc.returncode}):\n{proc.stdout}\n{proc.stderr}"


def test_band_sum_replays_an_li_fixture_with_a_roll_density():
    # 3-5 under Parry (LI layer 2: the whole call regenerates the sample): every pixel's status,
    # counts and value against LI through the ABI, within the fixture's own tolerances.
    _run(
        """
        f = load("3-5__band_sum_parry")
        rc, r = call(crystal_of(f), problem_of(f))
        assert rc == OK, rc
        try:
            assert r.rank_zero == 0 and r.kept_count > 0 and r.storage
            assert (r.point_mass, r.point_mass_error, r.point_mass_method, r.point_mass_pixel) == (0.0, 0.0, 0, 0)
            expected = f["expected"]["pixels"]
            assert r.pixel_count == len(expected)
            rel = f["tolerance"]["value_relative"]["value"]
            lit = 0
            for p, e in enumerate(expected):
                g = r.pixels[p]
                assert g.status == STATUS[e["status"]], (p, g.status, e["status"])
                assert close(g.delta, e["delta"], 1e-12, 1e-15), (p, g.delta, e["delta"])
                if e["status"] == "singular":
                    assert math.isnan(g.value) and math.isnan(g.k_eff) and g.k == 0 and g.k_rho_pos == 0, p
                    continue
                a = e["allowance"]
                assert abs(g.k - e["K"]) <= a["K_layer2"], (p, g.k, e["K"])
                assert abs(g.k_rho_pos - e["K_rho_pos"]) <= a["K_rho_pos_layer2"], (p, g.k_rho_pos, e["K_rho_pos"])
                assert close(g.value, e["value"], rel, a["value_layer2"]), (p, g.value, e["value"])
                assert close(g.k_eff, e["K_eff"], rel, a["K_eff_layer2"]), (p, g.k_eff, e["K_eff"])
                lit += e["value"] > 0.0
            assert lit >= 2, "the fixture must light pixels for this comparison to read anything"
        finally:
            lib.LUMICE_ANALYTIC_ReleaseBandSumResult(ctypes.addressof(r))
        assert zeroed_after_size(r) and r.struct_size == sizeof(Result)
        """
    )


def test_rank_zero_reports_the_point_mass():
    # 3-6 is rank 0: no band, the lattice mean of w landing on the first pixel that holds the sun.
    _run(
        """
        f = load("3-6__band_sum_rank0")
        rc, r = call(crystal_of(f), problem_of(f))
        assert rc == OK, rc
        pm = f["expected"]["point_mass"]
        assert r.rank_zero == 1 and r.point_mass_method == LATTICE_MEAN and r.point_mass_error == 0.0
        assert close(r.point_mass, pm["m"], 1e-10), (r.point_mass, pm["m"])
        expected = f["expected"]["pixels"]
        assert r.point_mass_pixel == [e["status"] for e in expected].index("point_mass")
        for p, e in enumerate(expected):
            g = r.pixels[p]
            assert g.status == STATUS[e["status"]], p
            assert close(g.value, e["value"], 1e-10), (p, g.value, e["value"])
            assert math.isnan(g.delta) and math.isnan(g.delta_lo) and math.isnan(g.k_eff) and g.k == 0, p
        lib.LUMICE_ANALYTIC_ReleaseBandSumResult(ctypes.addressof(r))
        assert zeroed_after_size(r)
        lib.LUMICE_ANALYTIC_ReleaseBandSumResult(ctypes.addressof(r))  # again: a no-op on a zeroed struct
        lib.LUMICE_ANALYTIC_ReleaseBandSumResult(None)
        """
    )


def test_an_empty_pixel_table_is_a_success():
    _run(
        """
        f = load("3-5__band_sum_random")
        prob = problem_of(f)
        prob.pixels = PixelTable(0, None, None, None)
        rc, r = call(crystal_of(f), prob)
        assert rc == OK and r.pixel_count == 0 and not r.pixels and r.kept_count > 0, (rc, r.pixel_count)
        lib.LUMICE_ANALYTIC_ReleaseBandSumResult(ctypes.addressof(r))
        """
    )


def test_call_errors_zero_fill_the_result():
    _run(
        """
        f = load("3-5__band_sum_plate")
        crystal = crystal_of(f)

        def expect(code, mutate=None, crystal_arg=crystal, size=None, prob_none=False):
            prob = problem_of(f)
            if mutate:
                mutate(prob)
            rc, r = call(crystal_arg, None if prob_none else prob, result(size))
            assert rc == code, (rc, code)
            assert zeroed_after_size(r), "a call error leaves the result zero-filled after struct_size"

        def set_(field, value):
            return lambda p: setattr(p, field, value)

        def density(**kw):
            def m(p):
                for k, v in kw.items():
                    setattr(p.pose_density, k, v)
            return m

        def pixel_field(field):
            def m(p):
                getattr(p.pixels, field)[0] = 0.5
            return m

        # NULL arguments.
        expect(NULL_ARG, crystal_arg=None)
        expect(NULL_ARG, prob_none=True)
        expect(NULL_ARG, set_("faces", None))
        for field in ("centre", "corners", "solid_angle"):
            expect(NULL_ARG, lambda p, fl=field: setattr(p.pixels, fl, None))
        assert lib.LUMICE_ANALYTIC_BandSum(byref(crystal), byref(problem_of(f)), None) == NULL_ARG
        # struct_size, the path, the index, the direction, n and the pixels.
        expect(INVALID_VALUE, size=sizeof(Result) - 8)
        expect(INVALID_VALUE, set_("face_count", 1))
        expect(INVALID_VALUE, lambda p: setattr(p, "faces", (c_int * 2)(3, 25)))
        expect(INVALID_VALUE, set_("refractive_index", 0.0))
        expect(INVALID_VALUE, set_("refractive_index", float("nan")))
        expect(INVALID_VALUE, lambda p: setattr(p, "incident_direction", (c_double * 3)(0.0, 0.0, -1.1)))
        expect(INVALID_VALUE, set_("sample_count", 0))
        expect(INVALID_VALUE, set_("sample_count", 10000001))
        expect(INVALID_VALUE, lambda p: setattr(p.pixels, "pixel_count", -1))
        expect(INVALID_VALUE, pixel_field("centre"))
        expect(INVALID_VALUE, pixel_field("corners"))
        expect(INVALID_VALUE, lambda p: p.pixels.solid_angle.__setitem__(0, 0.0))
        # The pose density: family, widths, unused fields, the zenith mean's range.
        expect(INVALID_VALUE, density(family=5))
        expect(INVALID_VALUE, density(zenith_std_deg=0.0))
        expect(INVALID_VALUE, density(roll_std_deg=1.0))
        expect(INVALID_VALUE, density(zenith_mean_deg=181.0))
        expect(INVALID_VALUE, density(family=FAMILY["parry"]))  # a roll family without its roll width
        expect(INVALID_VALUE, density(family=FAMILY["random"]))  # random with the plate's zenith fields left set
        # The crystal: a prism with a pyramid field is a value error, a rejected shape a config error.
        bad = crystal_of(f)
        bad.upper_h = 0.5
        expect(INVALID_VALUE, crystal_arg=bad)
        bad = crystal_of(f)
        bad.height = 0.0
        expect(INVALID_CONFIG, crystal_arg=bad)
        """
    )
