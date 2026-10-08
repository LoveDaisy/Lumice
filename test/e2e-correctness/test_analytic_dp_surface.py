"""The version-8 u-S^2 field layer through ctypes against the shared build.

The kernels and the C wrapper are checked in-process by
test/unit-correctness/analytic/test_dp_capi.cpp; this file owns the end-to-end half through the
installed surface of a real shared library: the struct layouts as ctypes sees them, the anchors
of the scrum's verified fixtures (beta 4-8-7-5's partition [0, 50.161740, 120] deg, 3-1-6's
closed-form kink with its zero spread and the dark-hole rim verdict, 3-5-6-7's two marched
kinks, the rhombic plate's blue tint), the declared threshold snapshot, and the struct_size
gate. Needs a shared build (`./scripts/build.sh -sj release`); skipped when there is none, or in
a CUDA configure, which does not produce the library.

symmetry_semantics: none — every call evaluates one concrete face sequence (doc/analytic-api.md
section 3); the class tint's members are the PBD label orbit (L1), compared to no symmetry
reduction of the representative.
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


# The bindings every child starts with: the version-8 structs field for field with
# lumice_analytic_core.h, plus the two v6 helpers the calls take (Crystal, PoseDensity).
_PRELUDE = textwrap.dedent(
    """
    import ctypes, math, sys
    from ctypes import POINTER, Structure, byref, c_char_p, c_double, c_int, c_size_t, c_uint8, c_uint32, \
c_ulonglong, c_void_p, sizeof

    OK, NULL_ARG, INVALID_VALUE, INVALID_CONFIG, UNKNOWN = 0, 1, 2, 3, 4

    class Crystal(Structure):
        _fields_ = [("kind", c_int), ("height", c_double), ("face_distance", c_double * 6),
                    ("upper_h", c_double), ("lower_h", c_double),
                    ("upper_wedge_deg", c_double), ("lower_wedge_deg", c_double)]

    def prism(height=1.0, fd=None):
        c = Crystal(kind=0, height=height)
        for i, d in enumerate(fd or [1.0] * 6):
            c.face_distance[i] = d
        return c

    def beta():  # fd = [2, 1, 1, 2, 1, 1], h = 3 (the 52.x fixtures' crystal)
        return prism(3.0, fd=[2.0, 1.0, 1.0, 2.0, 1.0, 1.0])

    class PoseDensity(Structure):
        _fields_ = [("family", c_int), ("zenith_mean_deg", c_double), ("zenith_std_deg", c_double),
                    ("roll_mean_deg", c_double), ("roll_std_deg", c_double)]

    def random_density():
        return PoseDensity(family=0)

    def plate_density():
        return PoseDensity(family=2, zenith_mean_deg=0.0, zenith_std_deg=1.0)

    class PlateFamily(Structure):
        _fields_ = [("sun_altitude_deg", c_double), ("zenith_std_deg", c_double),
                    ("samples", c_int), ("seed", c_ulonglong)]

    class BoundaryLoopResult(Structure):
        _fields_ = [("struct_size", c_uint32), ("status", c_int), ("status_name", c_char_p),
                    ("message", c_char_p), ("critical_point_count", c_int),
                    ("critical_point_positions", POINTER(c_double)), ("critical_point_values", POINTER(c_double)),
                    ("critical_point_kinds", POINTER(c_int)), ("critical_point_flags", POINTER(c_uint8)),
                    ("corner_count", c_int), ("corner_positions", POINTER(c_double)),
                    ("corner_values", POINTER(c_double)), ("has_plateau", c_int), ("plateau_value", c_double),
                    ("first_point", c_double * 3), ("storage", c_void_p)]

    class WeightKinkArc(Structure):
        _fields_ = [("first_point", c_int), ("point_count", c_int), ("closed", c_int),
                    ("end_gate", c_int * 2)]

    class WeightKinkCurve(Structure):
        _fields_ = [("step", c_int), ("margin", c_int), ("index", c_double), ("coverage", c_int),
                    ("has_normal", c_int), ("normal", c_double * 3), ("failed_seeds", c_int),
                    ("status", c_int), ("status_name", c_char_p), ("note", c_char_p), ("spread", c_double),
                    ("first_arc", c_int), ("arc_count", c_int)]

    class WeightKinksResult(Structure):
        _fields_ = [("struct_size", c_uint32), ("curve_count", c_int), ("curves", POINTER(WeightKinkCurve)),
                    ("arc_count", c_int), ("arcs", POINTER(WeightKinkArc)), ("arc_point_count", c_int),
                    ("arc_points", POINTER(c_double)), ("arc_values", POINTER(c_double)), ("storage", c_void_p)]

    class CriticalOnset(Structure):
        _fields_ = [("value", c_double), ("location", c_int), ("source", c_int), ("profile", c_int),
                    ("gradient_norm", c_double), ("has_measure_limit", c_int), ("measure_limit", c_double),
                    ("multiplicity", c_int)]

    class ClassificationResult(Structure):
        _fields_ = [("struct_size", c_uint32), ("halo_map_rank", c_int), ("escaped", c_int),
                    ("escape_status", c_int), ("escape_status_name", c_char_p), ("escape_message", c_char_p),
                    ("onset_count", c_int), ("onsets", POINTER(CriticalOnset)),
                    ("has_gradient_norm_range", c_int), ("gradient_norm_range", c_double * 2),
                    ("confined_dimensions", c_int), ("confined_width_count", c_int),
                    ("confined_widths_rad", POINTER(c_double)), ("family_pinned", c_int),
                    ("mechanism", c_char_p), ("storage", c_void_p)]

    class DeviationInterval(Structure):
        _fields_ = [("lower", c_double), ("upper", c_double), ("n_components", c_int),
                    ("n_closed", c_int), ("n_open", c_int)]

    class PartitionResult(Structure):
        _fields_ = [("struct_size", c_uint32), ("walk_status", c_int), ("walk_status_name", c_char_p),
                    ("walk_message", c_char_p), ("escaped", c_int), ("regime", c_int),
                    ("regime_name", c_char_p), ("message", c_char_p), ("interval_count", c_int),
                    ("intervals", POINTER(DeviationInterval)), ("lattice_n", c_int),
                    ("domain_components", c_int), ("complement_components", c_int),
                    ("audit_verdict", c_char_p), ("storage", c_void_p)]

    class WavelengthOnsetRow(Structure):
        _fields_ = [("location", c_int), ("source", c_int), ("profile", c_int),
                    ("jacobian_focusing", c_int), ("first_value", c_int), ("displacement_deg", c_double)]

    class WavelengthTableResult(Structure):
        _fields_ = [("struct_size", c_uint32), ("escaped", c_int), ("message", c_char_p),
                    ("label_count", c_int), ("labels", POINTER(c_char_p)), ("indices", POINTER(c_double)),
                    ("onset_count", c_int), ("onsets", POINTER(WavelengthOnsetRow)),
                    ("values_deg", POINTER(c_double)), ("storage", c_void_p)]

    class RestrictedCurveResult(Structure):
        _fields_ = [("struct_size", c_uint32), ("closed", c_int), ("existence", c_int), ("note", c_char_p),
                    ("point_count", c_int), ("u", POINTER(c_double)), ("tangent", POINTER(c_double)),
                    ("d_p", POINTER(c_double)), ("wavelength_count", c_int),
                    ("wavelengths_nm", POINTER(c_double)), ("indices", POINTER(c_double)),
                    ("critical_d_p", POINTER(c_double)), ("support_param", POINTER(c_double)),
                    ("routed_nonfinite", c_int), ("storage", c_void_p)]

    class ChromaticThresholds(Structure):
        _fields_ = [("n_red", c_double), ("n_blue", c_double), ("edge_min_shift_rad", c_double),
                    ("edge_spread_per_shift", c_double), ("calibration_white_max_deviation", c_double),
                    ("tint_ratio_min", c_double)]

    class ChromaticFeature(Structure):
        _fields_ = [("kind", c_int), ("source", c_char_p), ("color", c_int),
                    ("positive_fraction", c_double), ("delta_red", c_double), ("delta_blue", c_double),
                    ("shift", c_double), ("spread", c_double), ("direction_dispersion", c_double),
                    ("contrast", c_double), ("weight", c_double), ("lit_fraction", c_double),
                    ("visible", c_int)]

    class ChromaticResult(Structure):
        _fields_ = [("struct_size", c_uint32), ("verdict_kind", c_int), ("color", c_int), ("visible", c_int),
                    ("has_position", c_int), ("position", c_double), ("coverage_complete", c_int),
                    ("feature_count", c_int), ("features", POINTER(ChromaticFeature)),
                    ("note_count", c_int), ("notes", POINTER(c_char_p)), ("thresholds", ChromaticThresholds),
                    ("member_count", c_int), ("member_sizes", POINTER(c_int)), ("members", POINTER(c_int)),
                    ("lit_red_count", c_int), ("lit_sizes_red", POINTER(c_int)),
                    ("lit_members_red", POINTER(c_int)),
                    ("lit_blue_count", c_int), ("lit_sizes_blue", POINTER(c_int)),
                    ("lit_members_blue", POINTER(c_int)), ("has_tint", c_int), ("energy_red", c_double),
                    ("energy_blue", c_double), ("ratio", c_double), ("tir_fraction_red", c_double),
                    ("tir_fraction_blue", c_double), ("tint_direction_dispersion", c_double), ("storage", c_void_p)]

    lib = ctypes.CDLL(LIB)
    lib.LUMICE_ANALYTIC_GetApiVersion.restype = c_int
    for name, restype, argtypes in [
        ("LUMICE_ANALYTIC_TraceBoundaryLoop", c_int, [POINTER(Crystal), POINTER(c_int), c_int, c_double,
                                                      POINTER(BoundaryLoopResult)]),
        ("LUMICE_ANALYTIC_ReleaseBoundaryLoopResult", None, [POINTER(BoundaryLoopResult)]),
        ("LUMICE_ANALYTIC_TraceWeightKinks", c_int, [POINTER(Crystal), POINTER(c_int), c_int, c_double,
                                                     POINTER(WeightKinksResult)]),
        ("LUMICE_ANALYTIC_ReleaseWeightKinksResult", None, [POINTER(WeightKinksResult)]),
        ("LUMICE_ANALYTIC_ClassifyCriticalStructure", c_int, [POINTER(Crystal), POINTER(c_int), c_int, c_double,
                                                              PoseDensity, POINTER(ClassificationResult)]),
        ("LUMICE_ANALYTIC_ReleaseClassificationResult", None, [POINTER(ClassificationResult)]),
        ("LUMICE_ANALYTIC_PartitionDeviationAxis", c_int, [POINTER(Crystal), POINTER(c_int), c_int, c_double,
                                                           POINTER(PartitionResult)]),
        ("LUMICE_ANALYTIC_ReleasePartitionResult", None, [POINTER(PartitionResult)]),
        ("LUMICE_ANALYTIC_TraceWavelengthCriticalTable", c_int, [POINTER(Crystal), POINTER(c_int), c_int,
                                                                 PoseDensity, POINTER(c_char_p), POINTER(c_double),
                                                                 c_int, POINTER(WavelengthTableResult)]),
        ("LUMICE_ANALYTIC_ReleaseWavelengthTableResult", None, [POINTER(WavelengthTableResult)]),
        ("LUMICE_ANALYTIC_TraceRestrictedFamilyCurve", c_int, [POINTER(Crystal), POINTER(c_int), c_int, PoseDensity,
                                                               POINTER(c_double), c_double, POINTER(c_double),
                                                               POINTER(c_double), c_int, c_int,
                                                               POINTER(RestrictedCurveResult)]),
        ("LUMICE_ANALYTIC_ReleaseRestrictedCurveResult", None, [POINTER(RestrictedCurveResult)]),
        ("LUMICE_ANALYTIC_DiagnoseChromatic", c_int, [POINTER(Crystal), POINTER(c_int), c_int, c_double, c_double,
                                                      POINTER(ChromaticResult)]),
        ("LUMICE_ANALYTIC_DiagnoseClassTint", c_int, [POINTER(Crystal), POINTER(c_int), c_int, PlateFamily,
                                                      c_double, c_double, POINTER(ChromaticResult)]),
        ("LUMICE_ANALYTIC_ReleaseChromaticResult", None, [POINTER(ChromaticResult)]),
    ]:
        fn = getattr(lib, name)
        fn.restype = restype
        fn.argtypes = argtypes

    def call(lib_name, *args):
        return getattr(lib, lib_name)(*args)

    def release(name, result):
        getattr(lib, name)(result)
        getattr(lib, name)(result)  # repeat-safe

    def deg(rad):
        return rad * 180.0 / math.pi
    """
)

def _run(body: str) -> str:
    """Runs one child interpreter with the bindings plus `body`; returns its stdout."""
    lib = _find()
    prelude = _PRELUDE.replace("ctypes.CDLL(LIB)", f"ctypes.CDLL(r'{lib}')")
    code = prelude + "\n" + textwrap.dedent(body)
    proc = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True)
    if proc.returncode != 0:
        pytest.fail(f"child interpreter failed:\n{proc.stdout}\n{proc.stderr}")
    return proc.stdout


def test_api_version_is_eight():
    out = _run(
        """
        version = lib.LUMICE_ANALYTIC_GetApiVersion()
        assert version == 8, version
        print("ok")
        """
    )
    assert "ok" in out


def test_boundary_loop_walks_and_partition_replays_the_beta_anchor():
    out = _run(
        """
        crystal = prism()
        faces = (ctypes.c_int * 2)(3, 5)
        loop = BoundaryLoopResult(struct_size=sizeof(BoundaryLoopResult))
        rc = lib.LUMICE_ANALYTIC_TraceBoundaryLoop(byref(crystal), faces, 2, 1.31, byref(loop))
        assert rc == OK, rc
        assert loop.status == 0  # WALK_OK
        assert loop.status_name == b"ok", loop.status_name
        assert not loop.message
        assert loop.critical_point_count > 0 and loop.corner_count > 0 and loop.has_plateau == 0
        u2 = sum(loop.first_point[i] ** 2 for i in range(3))
        assert abs(u2 - 1.0) < 1e-12
        release("LUMICE_ANALYTIC_ReleaseBoundaryLoopResult", loop)
        assert not loop.critical_point_positions and not loop.storage

        # The beta crystal's 4-8-7-5: the slab crease partitions, [0, 50.161740, 120] deg.
        beta_crystal = beta()
        beta_faces = (ctypes.c_int * 4)(4, 8, 7, 5)
        part = PartitionResult(struct_size=sizeof(PartitionResult))
        rc = lib.LUMICE_ANALYTIC_PartitionDeviationAxis(byref(beta_crystal), beta_faces, 4, 1.3110129, byref(part))
        assert rc == OK, rc
        assert part.walk_status == 0 and part.walk_status_name == b"ok" and not part.walk_message
        assert part.escaped == 0 and part.interval_count == 2
        assert abs(deg(part.intervals[0].upper) - 50.161740000307894) < 1e-9
        assert abs(deg(part.intervals[1].upper) - 120.00000000000001) < 1e-9
        assert abs(deg(part.intervals[0].lower)) < 1e-9
        for i in range(part.interval_count):
            row = part.intervals[i]
            assert row.n_components == row.n_closed + row.n_open
        assert part.audit_verdict is None  # a disk pays no audit
        release("LUMICE_ANALYTIC_ReleasePartitionResult", part)
        assert not part.intervals

        # A bad face number is a call error and leaves the result zero-filled for a safe release.
        bad = (ctypes.c_int * 2)(3, 99)
        part2 = PartitionResult(struct_size=sizeof(PartitionResult))
        rc = lib.LUMICE_ANALYTIC_PartitionDeviationAxis(byref(crystal), bad, 2, 1.31, byref(part2))
        assert rc == INVALID_VALUE, rc
        assert not part2.storage and part2.interval_count == 0
        release("LUMICE_ANALYTIC_ReleasePartitionResult", part2)
        print("ok")
        """
    )
    assert "ok" in out


def test_weight_kinks_closed_form_and_marched():
    out = _run(
        """
        # 3-1-6 at the blue end: one closed-form curve, one level set (spread ~ 0).
        crystal = prism(0.5)
        faces = (ctypes.c_int * 3)(3, 1, 6)
        kinks = WeightKinksResult(struct_size=sizeof(WeightKinksResult))
        rc = lib.LUMICE_ANALYTIC_TraceWeightKinks(byref(crystal), faces, 3, 1.317, byref(kinks))
        assert rc == OK, rc
        assert kinks.curve_count == 1
        curve = kinks.curves[0]
        assert curve.step == 1 and curve.margin == 3
        assert abs(curve.index - 1.317) < 1e-15
        assert curve.coverage == 0  # CLOSED_FORM_AUTHORITY
        assert curve.has_normal == 1 and curve.failed_seeds == 0 and curve.status == 0
        assert curve.status_name == b"ok"
        assert curve.spread < 1e-15
        assert curve.arc_count == 1
        arc = kinks.arcs[curve.first_arc]
        assert arc.point_count > 0
        for i in range(arc.point_count):
            u = [kinks.arc_points[3 * (arc.first_point + i) + j] for j in range(3)]
            assert abs(sum(x * x for x in u) - 1.0) < 1e-12
        release("LUMICE_ANALYTIC_ReleaseWeightKinksResult", kinks)

        # 3-5-6-7: two internal reflections. The internal-1 onset coincides with the exit-Snell
        # boundary piece (its arc's values are the closure limit); the internal-2 onset misses
        # U_P entirely at this index — an empty curve with NaN spread, data, not a failure.
        faces = (ctypes.c_int * 4)(3, 5, 6, 7)
        kinks = WeightKinksResult(struct_size=sizeof(WeightKinksResult))
        rc = lib.LUMICE_ANALYTIC_TraceWeightKinks(byref(crystal), faces, 4, 1.3110129, byref(kinks))
        assert rc == OK, rc
        assert kinks.curve_count == 2
        first = kinks.curves[0]
        assert first.step == 1 and first.margin == 3 and first.status == 0 and first.failed_seeds == 0
        assert first.arc_count >= 1
        second = kinks.curves[1]
        assert second.step == 2 and second.margin == 5 and second.status == 0
        assert second.arc_count == 0 and second.spread != second.spread  # NaN: no arcs
        total = sum(kinks.arcs[first.first_arc + a].point_count for a in range(first.arc_count))
        assert total == kinks.arc_point_count
        for i in range(kinks.arc_point_count):
            v = kinks.arc_values[i]
            assert not (v != v)  # finite: the closure routing, not NaN
        release("LUMICE_ANALYTIC_ReleaseWeightKinksResult", kinks)
        print("ok")
        """
    )
    assert "ok" in out


def test_classify_point_mass_mechanisms_and_wavelength_table():
    out = _run(
        """
        crystal = prism()
        straight = (ctypes.c_int * 2)(3, 6)
        cls = ClassificationResult(struct_size=sizeof(ClassificationResult))
        rc = lib.LUMICE_ANALYTIC_ClassifyCriticalStructure(byref(crystal), straight, 2, 1.31, random_density(),
                                                           byref(cls))
        assert rc == OK, rc
        assert cls.halo_map_rank == 0 and cls.mechanism == b"point_mass" and cls.onset_count == 0
        release("LUMICE_ANALYTIC_ReleaseClassificationResult", cls)

        faces = (ctypes.c_int * 2)(3, 5)
        cls = ClassificationResult(struct_size=sizeof(ClassificationResult))
        rc = lib.LUMICE_ANALYTIC_ClassifyCriticalStructure(byref(crystal), faces, 2, 1.31, random_density(),
                                                           byref(cls))
        assert rc == OK, rc
        assert cls.halo_map_rank == 2 and cls.mechanism == b"none"
        assert cls.escaped == 0 and cls.onset_count >= 1 and cls.has_gradient_norm_range == 1
        release("LUMICE_ANALYTIC_ReleaseClassificationResult", cls)

        # The wavelength table: the interior minimum follows n across the declared pair.
        labels = [ctypes.c_char_p(b"red"), ctypes.c_char_p(b"blue")]
        indices = (ctypes.c_double * 2)(1.307, 1.317)
        table = WavelengthTableResult(struct_size=sizeof(WavelengthTableResult))
        rc = lib.LUMICE_ANALYTIC_TraceWavelengthCriticalTable(byref(crystal), faces, 2, random_density(),
                                                              (ctypes.c_char_p * 2)(*[ctypes.cast(l, ctypes.c_char_p)
                                                                                      for l in labels]),
                                                              indices, 2, byref(table))
        assert rc == OK, rc and not table.escaped
        assert table.label_count == 2 and table.labels[0] == b"red" and table.labels[1] == b"blue"
        assert table.onset_count >= 1
        found = None
        for i in range(table.onset_count):
            row = table.onsets[i]
            if row.location == 0 and row.source == 0:  # INTERIOR, INTERIOR_MINIMUM
                found = row
        assert found is not None
        d_min = lambda n: deg(2.0 * math.asin(n / 2.0) - math.pi / 3.0)
        assert abs(table.values_deg[found.first_value] - d_min(1.307)) < 1e-6
        assert abs(table.values_deg[found.first_value + 1] - d_min(1.317)) < 1e-6
        assert abs(found.displacement_deg - (d_min(1.317) - d_min(1.307))) < 1e-6
        release("LUMICE_ANALYTIC_ReleaseWavelengthTableResult", table)

        # An empty index set is the kernel's escape, not a call error.
        table = WavelengthTableResult(struct_size=sizeof(WavelengthTableResult))
        rc = lib.LUMICE_ANALYTIC_TraceWavelengthCriticalTable(byref(crystal), faces, 2, random_density(),
                                                              None, None, 0, byref(table))
        assert rc == OK and table.escaped == 1 and b"indices is empty" in table.message
        release("LUMICE_ANALYTIC_ReleaseWavelengthTableResult", table)
        print("ok")
        """
    )
    assert "ok" in out


def test_restricted_curve_records_where_the_field_is_not():
    out = _run(
        """
        crystal = prism()
        altitude = math.radians(20.0)
        sun = (ctypes.c_double * 3)(math.cos(altitude), 0.0, math.sin(altitude))
        wavelengths = (ctypes.c_double * 2)(550.0, 440.0)
        indices = (ctypes.c_double * 2)(1.3110129, 1.3175)
        curve = RestrictedCurveResult(struct_size=sizeof(RestrictedCurveResult))
        rc = lib.LUMICE_ANALYTIC_TraceRestrictedFamilyCurve(byref(crystal),
                                                            (ctypes.c_int * 2)(3, 5), 2, plate_density(),
                                                            sun, 1.3110129, wavelengths, indices, 2, 16,
                                                            byref(curve))
        assert rc == OK, rc
        assert curve.closed == 1 and curve.existence == 0 and curve.point_count == 16
        assert curve.wavelength_count == 2
        for i in range(16):
            u = [curve.u[3 * i + j] for j in range(3)]
            t = [curve.tangent[3 * i + j] for j in range(3)]
            assert abs(sum(x * x for x in u) - 1.0) < 1e-12
            assert abs(sum(x * x for x in t) - 1.0) < 1e-12
            assert abs(curve.support_param[i] - 2.0 * math.pi * (i + 0.5) / 16.0) < 1e-12
        nan_count = sum(1 for i in range(16) if curve.d_p[i] != curve.d_p[i])
        nan_count += sum(1 for i in range(32) if curve.critical_d_p[i] != curve.critical_d_p[i])
        assert curve.routed_nonfinite > 0 and nan_count == curve.routed_nonfinite
        release("LUMICE_ANALYTIC_ReleaseRestrictedCurveResult", curve)

        # No family axis: an empty curve with the reason, a success.
        curve = RestrictedCurveResult(struct_size=sizeof(RestrictedCurveResult))
        rc = lib.LUMICE_ANALYTIC_TraceRestrictedFamilyCurve(byref(crystal), (ctypes.c_int * 2)(3, 5), 2,
                                                            random_density(), sun, 1.31, None, None, 0, 8,
                                                            byref(curve))
        assert rc == OK and curve.point_count == 0 and b"no family axis" in curve.note
        release("LUMICE_ANALYTIC_ReleaseRestrictedCurveResult", curve)
        print("ok")
        """
    )
    assert "ok" in out


def test_chromatic_rim_thresholds_and_plate_tint():
    out = _run(
        """
        crystal = prism(0.5)
        faces = (ctypes.c_int * 3)(3, 1, 6)
        chroma = ChromaticResult(struct_size=sizeof(ChromaticResult))
        rc = lib.LUMICE_ANALYTIC_DiagnoseChromatic(byref(crystal), faces, 3, 1.307, 1.317, byref(chroma))
        assert rc == OK, rc
        assert chroma.verdict_kind == 0 and chroma.color == 0 and chroma.visible == 1  # EDGE, BLUE
        assert chroma.coverage_complete == 1 and chroma.note_count == 0
        assert chroma.feature_count == 1
        edge = chroma.features[0]
        assert edge.kind == 0 and edge.source == b"internal_1_tir_discriminant" and edge.color == 0
        closed_form = 2.0 * math.asin(math.sqrt(1.317 ** 2 - 1.0))
        assert abs(edge.delta_blue - closed_form) < 2e-3
        assert abs(chroma.position - closed_form) < 2e-3
        # The declared threshold snapshot, field for field (LI's frozen values).
        t = chroma.thresholds
        assert abs(t.n_red - 1.307) < 1e-15 and abs(t.n_blue - 1.317) < 1e-15
        assert abs(t.edge_min_shift_rad - math.radians(0.5)) < 1e-12
        assert abs(t.edge_spread_per_shift - 1.0) < 1e-15
        assert abs(t.calibration_white_max_deviation - 0.05) < 1e-15
        assert abs(t.tint_ratio_min - 1.10) < 1e-12
        assert chroma.member_count == 0 and chroma.has_tint == 0  # the class suffix is empty
        release("LUMICE_ANALYTIC_ReleaseChromaticResult", chroma)

        # The rhombic plate's parhelion class: 24 members, blue tint at ratio ~1.492.
        plate = prism(0.5, fd=[1.5, 1.0, 1.0, 1.5, 1.0, 1.0])
        family = PlateFamily(sun_altitude_deg=9.0, zenith_std_deg=1.0, samples=100000, seed=3)
        chroma = ChromaticResult(struct_size=sizeof(ChromaticResult))
        rc = lib.LUMICE_ANALYTIC_DiagnoseClassTint(byref(plate), (ctypes.c_int * 4)(1, 3, 5, 2), 4, family,
                                                   1.307, 1.317, byref(chroma))
        assert rc == OK, rc
        assert chroma.member_count == 24
        assert sum(chroma.member_sizes[i] for i in range(24)) == 96
        assert chroma.verdict_kind == 2 and chroma.color == 0 and chroma.visible == 1 and chroma.has_tint == 1
        assert abs(chroma.ratio - 1.492) < 5e-2
        assert chroma.lit_red_count == 4 and chroma.lit_blue_count == 4
        for i in range(4):
            assert 0 <= chroma.lit_members_red[i] < 24 and chroma.lit_sizes_red[i] == 4
        release("LUMICE_ANALYTIC_ReleaseChromaticResult", chroma)
        print("ok")
        """
    )
    assert "ok" in out


def test_struct_size_gate_refuses_undersized_results():
    out = _run(
        """
        crystal = prism()
        faces = (ctypes.c_int * 2)(3, 5)
        # ctypes laid the struct out; a struct_size of 4 (just the field) has no readable prefix.
        loop = BoundaryLoopResult(struct_size=4)
        rc = lib.LUMICE_ANALYTIC_TraceBoundaryLoop(byref(crystal), faces, 2, 1.31, byref(loop))
        assert rc == INVALID_VALUE, rc
        assert loop.status == 0 and not loop.storage
        # A null out pointer is NULL_ARG, not a crash.
        rc = lib.LUMICE_ANALYTIC_TraceBoundaryLoop(byref(crystal), faces, 2, 1.31, None)
        assert rc == NULL_ARG, rc
        print("ok")
        """
    )
    assert "ok" in out
