"""Independent consumer and C ABI coverage for the general diagnostic field.

The fast test fixes the oracle's input matrix without loading Lumice. Slow cases run the final
ctypes consumer in child interpreters against only liblumice_analytic.

symmetry_semantics: none — every fixture names one concrete physical face sequence.
"""

from __future__ import annotations

import math
import subprocess
import sys
import textwrap
from pathlib import Path

import pytest

from test.e2e.diagnostic_field_oracle import exp_rotation, prism_planes, trace_path

ROOT = Path(__file__).resolve().parents[2]
SHARED_OUT = ROOT / "build" / "Release" / "shared"
_PATTERN = {"win32": "lumice_analytic.dll", "darwin": "liblumice_analytic.dylib", "linux": "liblumice_analytic.so"}


def _find_four_face_pose() -> tuple[tuple[float, ...], object]:
    incident = (-math.cos(math.radians(15.0)), 0.0, -math.sin(math.radians(15.0)))
    for ix in range(-8, 9):
        for iy in range(-8, 9):
            for iz in range(-8, 9):
                pose = exp_rotation((0.11 * ix, 0.09 * iy, 0.13 * iz))
                result = trace_path((3, 5, 6, 7), 1.31, incident, pose)
                if result.valid:
                    return pose, result
    raise AssertionError("independent oracle found no valid four-face fixture")


def test_independent_oracle_covers_the_pre_abi_consumer_matrix() -> None:
    distances = (1.37, 0.91, 1.12, 1.46, 0.83, 1.05)
    planes = prism_planes(distances, 0.73)
    assert tuple(planes) == tuple(range(1, 9))
    assert len({round(planes[face][1], 12) for face in range(3, 9)}) == 6

    pose, traced = _find_four_face_pose()
    assert traced.valid and traced.outgoing is not None
    assert len(traced.coefficients) == 4
    assert len(traced.domain_margins) == 6
    assert len(traced.tir_margins) == 2
    assert abs(math.sqrt(sum(x * x for x in traced.outgoing)) - 1.0) < 2.0e-14
    assert pose != (1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0)

    pyramid = trace_path(
        (13, 15, 26, 28),
        1.31,
        (-0.9659258262890683, 0.0, -0.25881904510252074),
        (0.38947319055953666, -0.9206440277496026, -0.02692968630273812,
         -0.41810850781521075, -0.15067503460533604, -0.8958137695074904,
         0.8206674654573541, 0.3601549779132174, -0.44361298713678066),
        upper_wedge_deg=27.996455531220374,
        lower_wedge_deg=38.5704386184827,
    )
    assert pyramid.valid
    assert len(pyramid.coefficients) == 4 and len(pyramid.tir_margins) == 2


def _platform() -> str:
    if sys.platform.startswith("win"):
        return "win32"
    if sys.platform == "darwin":
        return "darwin"
    return "linux"


def _find_library() -> Path:
    if not SHARED_OUT.is_dir():
        pytest.skip(f"no shared build at {SHARED_OUT} (./scripts/build.sh -sj release)")
    hits = sorted(path for path in SHARED_OUT.rglob(_PATTERN[_platform()]) if path.is_file())
    if not hits:
        pytest.skip("shared analytic library is not present in this build")
    assert len(hits) == 1, hits
    return hits[0]


_PRELUDE = textwrap.dedent(
    """
    import ctypes, math, sys
    from concurrent.futures import ThreadPoolExecutor
    from ctypes import POINTER, Structure, byref, c_char_p, c_double, c_int, c_uint32, c_uint64, c_void_p, sizeof

    sys.path.insert(0, ROOT)
    from test.e2e.diagnostic_field_oracle import (
        entry_measure_prism, right_perturb, trace_entry_measure_prism, trace_path)

    class Crystal(Structure):
        _fields_ = [("kind", c_int), ("height", c_double), ("face_distance", c_double * 6),
                    ("upper_h", c_double), ("lower_h", c_double),
                    ("upper_wedge_deg", c_double), ("lower_wedge_deg", c_double)]

    class Row(Structure):
        _fields_ = [("refractive_index", c_double), ("incident_direction", c_double * 3),
                    ("pose", c_double * 9)]

    class Interface(Structure):
        _fields_ = [("interface_index", c_int), ("face_number", c_int), ("kind", c_int),
                    ("coefficient", c_double), ("pose_derivative_available", c_int),
                    ("index_derivative_available", c_int), ("pose_gradient", c_double * 3),
                    ("index_derivative", c_double)]

    class Margin(Structure):
        _fields_ = [("name", c_char_p), ("interface_index", c_int), ("value", c_double),
                    ("pose_derivative_available", c_int), ("index_derivative_available", c_int),
                    ("pose_gradient", c_double * 3), ("index_derivative", c_double)]

    # Frozen, explicit mirror of the first published version 7 layout. Keep this definition and
    # size fixed when the current C header grows: it is the old-consumer compatibility fixture.
    class ResultV7(Structure):
        _fields_ = [("struct_size", c_uint32), ("row_error", c_int), ("path_status", c_int),
                    ("entry_status", c_int), ("outgoing_direction", c_double * 3),
                    ("entry_measure", c_double), ("fresnel_weight", c_double),
                    ("interface_count", c_int), ("interfaces", POINTER(Interface)),
                    ("domain_margin_count", c_int), ("domain_margins", POINTER(Margin)),
                    ("tir_margin_count", c_int), ("tir_margins", POINTER(Margin)),
                    ("direction_pose_jacobian_available", c_int),
                    ("direction_pose_hessian_available", c_int),
                    ("direction_index_derivative_available", c_int),
                    ("entry_pose_gradient_available", c_int), ("entry_index_derivative_available", c_int),
                    ("direction_pose_jacobian", c_double * 9),
                    ("direction_pose_hessian", c_double * 27),
                    ("direction_index_derivative", c_double * 3),
                    ("entry_pose_gradient", c_double * 3), ("entry_index_derivative", c_double),
                    ("storage", c_void_p)]

    RESULT_V7_SIZE = 480
    assert sizeof(ResultV7) == RESULT_V7_SIZE
    assert ResultV7.storage.offset + sizeof(c_void_p) == RESULT_V7_SIZE

    OK, NULL_ARG, INVALID_VALUE = 0, 1, 2
    PATH_OK, ENTRY_OK = 0, 1
    ENTRY_T, INTERNAL_R, EXIT_T, EXTERNAL_R = 0, 1, 2, 3

    lib = ctypes.CDLL(LIB)
    lib.LUMICE_ANALYTIC_GetApiVersion.restype = c_int
    assert lib.LUMICE_ANALYTIC_GetApiVersion() == 12
    lib.LUMICE_ANALYTIC_EvaluateDiagnosticFieldBatch.restype = c_int
    lib.LUMICE_ANALYTIC_EvaluateDiagnosticFieldBatch.argtypes = [POINTER(Crystal), POINTER(c_int), c_int,
                                                                 POINTER(Row), c_int, c_void_p]
    lib.LUMICE_ANALYTIC_ReleaseDiagnosticFieldResult.restype = None
    lib.LUMICE_ANALYTIC_ReleaseDiagnosticFieldResult.argtypes = [c_void_p]

    DISTANCES = (1.37, 0.91, 1.12, 1.46, 0.83, 1.05)
    INCIDENT = (-0.9659258262890683, 0.0, -0.25881904510252074)
    POSE = (0.6813748546859479, -0.19119700588730165, 0.7065210629142165,
            -0.6591854405220774, -0.579842196951831, 0.47880850205253056,
            0.3181239733270946, -0.7919764716036737, -0.521124175241158)
    FACES = (3, 5, 6, 7)

    def prism():
        crystal = Crystal(kind=0, height=0.73)
        for index, value in enumerate(DISTANCES):
            crystal.face_distance[index] = value
        return crystal

    def row(index=1.31, incident=INCIDENT, pose=POSE):
        return Row(index, (c_double * 3)(*incident), (c_double * 9)(*pose))

    def results(count, result_type=ResultV7):
        values = (result_type * count)()
        for value in values:
            ctypes.memset(byref(value), 0x5A, sizeof(value))
            value.struct_size = sizeof(result_type)
        return values

    def call(crystal, faces, rows, out):
        face_array = (c_int * len(faces))(*faces) if faces is not None else None
        return lib.LUMICE_ANALYTIC_EvaluateDiagnosticFieldBatch(
            byref(crystal) if crystal is not None else None, face_array, len(faces) if faces is not None else 0,
            rows, len(rows), ctypes.addressof(out))

    def close(a, b, tolerance):
        assert abs(a - b) <= tolerance, (a, b, tolerance)
    """
)


def _run_child(body: str) -> None:
    code = f"LIB = {str(_find_library())!r}\nROOT = {str(ROOT)!r}\n" + _PRELUDE + textwrap.dedent(body)
    proc = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True, timeout=120)
    assert proc.returncode == 0, f"child failed (rc={proc.returncode}):\n{proc.stdout}\n{proc.stderr}"


@pytest.mark.slow
def test_one_face_external_reflection_has_independent_direction_and_fresnel_oracles() -> None:
    _run_child(
        """
        crystal = prism()
        for index in range(6):
            crystal.face_distance[index] = 1.0
        identity = (1., 0., 0., 0., 1., 0., 0., 0., 1.)
        rows = (Row * 2)(row(index=1.31, incident=(0., 0., -1.), pose=identity),
                         row(index=1.47, incident=(0., 0., -1.), pose=identity))
        out = results(2)
        assert call(crystal, (1,), rows, out) == OK
        expected_jacobian = (0., 2., 0., -2., 0., 0., 0., 0., 0.)
        expected_hessian = [0.] * 27
        expected_hessian[9 * 0 + 3 * 0 + 2] = 1.
        expected_hessian[9 * 0 + 3 * 2 + 0] = 1.
        expected_hessian[9 * 1 + 3 * 1 + 2] = 1.
        expected_hessian[9 * 1 + 3 * 2 + 1] = 1.
        expected_hessian[9 * 2 + 3 * 0 + 0] = -4.
        expected_hessian[9 * 2 + 3 * 1 + 1] = -4.
        try:
            for result, index in zip(out, (1.31, 1.47)):
                assert result.row_error == OK and result.path_status == PATH_OK
                assert result.entry_status == ENTRY_OK and result.interface_count == 1
                assert result.interfaces[0].kind == EXTERNAL_R and result.interfaces[0].face_number == 1
                close(result.fresnel_weight, ((index - 1.) / (index + 1.)) ** 2, 1e-12)
                close(result.interfaces[0].coefficient, result.fresnel_weight, 1e-15)
                close(result.entry_measure, 3. * math.sqrt(3.) / 8., 1e-7)
                for actual, expected in zip(result.outgoing_direction, (0., 0., 1.)):
                    close(actual, expected, 1e-12)
                assert result.domain_margin_count == 1 and result.tir_margin_count == 1
                assert result.domain_margins[0].name.decode() == "external_reflection_incidence_cosine"
                assert result.tir_margins[0].name.decode() == "external_reflection_tir_discriminant"
                close(result.domain_margins[0].value, 1., 1e-15)
                close(result.tir_margins[0].value, 1., 1e-15)
                assert result.direction_pose_jacobian_available and result.direction_pose_hessian_available
                assert result.direction_index_derivative_available
                for actual, expected in zip(result.direction_pose_jacobian, expected_jacobian):
                    close(actual, expected, 1e-7)
                for actual, expected in zip(result.direction_pose_hessian, expected_hessian):
                    close(actual, expected, 2e-5)
                for actual in result.direction_index_derivative:
                    close(actual, 0., 1e-12)
                assert result.entry_pose_gradient_available and result.entry_index_derivative_available
                for actual in result.entry_pose_gradient:
                    close(actual, 0., 1e-8)
                close(result.entry_index_derivative, 0., 1e-12)
                interface = result.interfaces[0]
                assert interface.pose_derivative_available and interface.index_derivative_available
                for actual in interface.pose_gradient:
                    close(actual, 0., 1e-8)
                close(interface.index_derivative, 4. * (index - 1.) / (index + 1.) ** 3, 2e-9)
                assert result.domain_margins[0].pose_derivative_available
                assert result.domain_margins[0].index_derivative_available
                assert result.tir_margins[0].pose_derivative_available
                assert result.tir_margins[0].index_derivative_available
                close(result.domain_margins[0].index_derivative, 0., 1e-12)
                close(result.tir_margins[0].index_derivative, 0., 1e-12)
        finally:
            for result in out:
                lib.LUMICE_ANALYTIC_ReleaseDiagnosticFieldResult(byref(result))

        sine, cosine = 0.6, 0.8
        branch_rows = (Row * 4)(row(index=1.31, incident=(sine, 0., -cosine), pose=identity),
                                row(index=0.5, incident=(sine, 0., -cosine), pose=identity),
                                row(index=sine, incident=(sine, 0., -cosine), pose=identity),
                                row(index=sine + 1e-3, incident=(sine, 0., -cosine), pose=identity))
        branch_out = results(4)
        assert call(crystal, (1,), branch_rows, branch_out) == OK
        try:
            for result in branch_out:
                assert result.row_error == OK and result.path_status == PATH_OK
                for actual, expected in zip(result.outgoing_direction, (sine, 0., cosine)):
                    close(actual, expected, 1e-12)
            transmitted_cosine = math.sqrt(1. - sine * sine / (1.31 * 1.31))
            rs = (cosine - 1.31 * transmitted_cosine) / (cosine + 1.31 * transmitted_cosine)
            rp = (1.31 * cosine - transmitted_cosine) / (1.31 * cosine + transmitted_cosine)
            close(branch_out[0].interfaces[0].coefficient, .5 * (rs * rs + rp * rp), 2e-12)
            assert branch_out[1].tir_margins[0].value < 0.
            close(branch_out[1].interfaces[0].coefficient, 1., 0.)
            assert branch_out[1].interfaces[0].index_derivative_available
            close(branch_out[1].interfaces[0].index_derivative, 0., 1e-12)
            close(branch_out[2].tir_margins[0].value, 0., 2e-15)
            assert branch_out[2].direction_pose_jacobian_available
            assert branch_out[2].direction_index_derivative_available
            assert not branch_out[2].interfaces[0].pose_derivative_available
            assert not branch_out[2].interfaces[0].index_derivative_available
            assert branch_out[3].interfaces[0].coefficient < 1.
        finally:
            for result in branch_out:
                lib.LUMICE_ANALYTIC_ReleaseDiagnosticFieldResult(byref(result))
        """
    )


@pytest.mark.slow
def test_ctypes_consumer_matches_independent_field_and_derivative_oracles() -> None:
    _run_child(
        """
        bad_pose = list(POSE)
        bad_pose[0] = 2.0
        rows = (Row * 4)(row(), row(index=0.0), row(pose=bad_pose), row(index=1.32))
        out = results(4)
        assert call(prism(), FACES, rows, out) == OK
        assert out[0].row_error == OK and out[0].path_status == PATH_OK and out[0].entry_status == ENTRY_OK
        assert out[1].row_error == INVALID_VALUE and not out[1].storage
        assert out[2].row_error == INVALID_VALUE and not out[2].storage
        assert out[3].row_error == OK and out[3].storage

        expected = trace_path(FACES, 1.31, INCIDENT, POSE)
        assert expected.valid
        for got, want in zip(out[0].outgoing_direction, expected.outgoing):
            close(got, want, 3e-12)
        assert out[0].interface_count == 4 and out[0].domain_margin_count == 6 and out[0].tir_margin_count == 2
        assert [out[0].interfaces[i].kind for i in range(4)] == [ENTRY_T, INTERNAL_R, INTERNAL_R, EXIT_T]
        assert [out[0].interfaces[i].face_number for i in range(4)] == list(FACES)
        for index, want in enumerate(expected.coefficients):
            close(out[0].interfaces[index].coefficient, want, 3e-12)
        for index, want in enumerate(expected.domain_margins):
            close(out[0].domain_margins[index].value, want, 3e-12)
        for index, want in enumerate(expected.tir_margins):
            close(out[0].tir_margins[index].value, want, 3e-12)
        assert [out[0].domain_margins[i].name.decode() for i in range(6)] == [
            "entry_incidence_cosine", "entry_snell_discriminant", "internal_1_incidence_cosine",
            "internal_2_incidence_cosine", "exit_incidence_cosine", "exit_snell_discriminant"]
        close(out[0].entry_measure, entry_measure_prism(DISTANCES, 0.73, FACES, 1.31, INCIDENT, POSE), 2e-10)
        close(out[0].fresnel_weight, math.prod(expected.coefficients), 3e-12)

        pose_step = 5e-5
        direction_samples = []
        for axis in range(3):
            delta = [0.0, 0.0, 0.0]
            delta[axis] = pose_step
            hi = trace_path(FACES, 1.31, INCIDENT, right_perturb(POSE, tuple(delta)))
            delta[axis] = -pose_step
            lo = trace_path(FACES, 1.31, INCIDENT, right_perturb(POSE, tuple(delta)))
            direction_samples.append((lo, hi))
            for component in range(3):
                want = (hi.outgoing[component] - lo.outgoing[component]) / (2 * pose_step)
                close(out[0].direction_pose_jacobian[3 * component + axis], want, 2e-6)
            want_margin = (hi.domain_margins[-1] - lo.domain_margins[-1]) / (2 * pose_step)
            close(out[0].domain_margins[5].pose_gradient[axis], want_margin, 2e-6)
            want_tir = (hi.tir_margins[1] - lo.tir_margins[1]) / (2 * pose_step)
            close(out[0].tir_margins[1].pose_gradient[axis], want_tir, 2e-6)
            want_coeff = (hi.coefficients[1] - lo.coefficients[1]) / (2 * pose_step)
            close(out[0].interfaces[1].pose_gradient[axis], want_coeff, 2e-6)
            entry_hi = entry_measure_prism(DISTANCES, 0.73, FACES, 1.31, INCIDENT,
                                           right_perturb(POSE, tuple(-x for x in delta)))
            entry_lo = entry_measure_prism(DISTANCES, 0.73, FACES, 1.31, INCIDENT,
                                           right_perturb(POSE, tuple(delta)))
            close(out[0].entry_pose_gradient[axis], (entry_hi - entry_lo) / (2 * pose_step), 2e-5)

        # Validate all 27 entries independently.  In particular the 18 off-diagonal slots use
        # four oracle evaluations rather than treating the implementation's symmetry as an oracle.
        hessian_step = 5e-4
        for component in range(3):
            for axis0 in range(3):
                for axis1 in range(3):
                    if axis0 == axis1:
                        delta = [0.0, 0.0, 0.0]
                        delta[axis0] = hessian_step
                        hi = trace_path(FACES, 1.31, INCIDENT,
                                        right_perturb(POSE, tuple(delta))).outgoing[component]
                        delta[axis0] = -hessian_step
                        lo = trace_path(FACES, 1.31, INCIDENT,
                                        right_perturb(POSE, tuple(delta))).outgoing[component]
                        want = (hi - 2 * expected.outgoing[component] + lo) / (hessian_step * hessian_step)
                    else:
                        corners = []
                        for sign0, sign1 in ((1.0, 1.0), (1.0, -1.0), (-1.0, 1.0), (-1.0, -1.0)):
                            delta = [0.0, 0.0, 0.0]
                            delta[axis0] = sign0 * hessian_step
                            delta[axis1] = sign1 * hessian_step
                            corners.append(trace_path(
                                FACES, 1.31, INCIDENT, right_perturb(POSE, tuple(delta))).outgoing[component])
                        want = (corners[0] - corners[1] - corners[2] + corners[3]) / (
                            4 * hessian_step * hessian_step)
                    close(out[0].direction_pose_hessian[9 * component + 3 * axis0 + axis1], want, 2e-3)

        index_step = 1e-5
        index_lo = trace_path(FACES, 1.31 - index_step, INCIDENT, POSE)
        index_hi = trace_path(FACES, 1.31 + index_step, INCIDENT, POSE)
        for component in range(3):
            want = (index_hi.outgoing[component] - index_lo.outgoing[component]) / (2 * index_step)
            close(out[0].direction_index_derivative[component], want, 2e-6)
        for index in range(out[0].interface_count):
            assert out[0].interfaces[index].index_derivative_available
            want = (index_hi.coefficients[index] - index_lo.coefficients[index]) / (2 * index_step)
            close(out[0].interfaces[index].index_derivative, want, 2e-6)
        for index in range(out[0].domain_margin_count):
            assert out[0].domain_margins[index].index_derivative_available
            want = (index_hi.domain_margins[index] - index_lo.domain_margins[index]) / (2 * index_step)
            close(out[0].domain_margins[index].index_derivative, want, 2e-6)
        for index in range(out[0].tir_margin_count):
            assert out[0].tir_margins[index].index_derivative_available
            want = (index_hi.tir_margins[index] - index_lo.tir_margins[index]) / (2 * index_step)
            close(out[0].tir_margins[index].index_derivative, want, 2e-6)
        entry_index_lo = entry_measure_prism(DISTANCES, 0.73, FACES, 1.31 - index_step, INCIDENT, POSE)
        entry_index_hi = entry_measure_prism(DISTANCES, 0.73, FACES, 1.31 + index_step, INCIDENT, POSE)
        assert out[0].entry_index_derivative_available
        close(out[0].entry_index_derivative, (entry_index_hi - entry_index_lo) / (2 * index_step), 2e-5)
        assert out[0].direction_pose_jacobian_available and out[0].direction_pose_hessian_available
        assert out[0].direction_index_derivative_available and out[0].entry_pose_gradient_available

        domain_faces = (3, 5)
        domain_pose = (0.048986947498166344, -0.9981838880720896, -0.03506001380630651,
                       -0.9819637714270643, -0.054549356672966676, 0.1810290564834884,
                       -0.18261278865278968, 0.02555960249792641, -0.9828526217803968)
        domain_rows = (Row * 1)(row(index=1.3632360204317753, pose=domain_pose))
        domain_out = results(1)
        assert call(prism(), domain_faces, domain_rows, domain_out) == OK
        assert domain_out[0].path_status == PATH_OK
        assert 0.0 < min(domain_out[0].domain_margins[i].value for i in range(4)) < 1.0e-4
        assert not domain_out[0].direction_index_derivative_available
        assert all(not domain_out[0].interfaces[i].index_derivative_available for i in range(2))
        assert all(not domain_out[0].domain_margins[i].index_derivative_available for i in range(4))
        lib.LUMICE_ANALYTIC_ReleaseDiagnosticFieldResult(byref(domain_out[0]))

        kink_pose = (-0.49760634739842935, -0.8471133033132857, 0.1865126654638934,
                     -0.6613497486591641, 0.23139308033739314, -0.713494045049034,
                     0.5612525572122176, -0.47838927007367726, -0.6753808357520372)
        kink_rows = (Row * 1)(row(pose=kink_pose))
        kink_out = results(1)
        assert call(prism(), (1, 5, 2, 3), kink_rows, kink_out) == OK
        assert kink_out[0].path_status == PATH_OK and kink_out[0].tir_margin_count == 2
        close(kink_out[0].tir_margins[1].value, 2.8686495105012533e-05, 2e-12)
        assert kink_out[0].tir_margins[1].pose_derivative_available
        assert not kink_out[0].interfaces[2].pose_derivative_available
        assert not kink_out[0].interfaces[2].index_derivative_available
        lib.LUMICE_ANALYTIC_ReleaseDiagnosticFieldResult(byref(kink_out[0]))

        topology_faces = (3, 5, 6)
        topology_index = 1.0565260582028604
        topology_pose = (0.9868343484263193, 0.08505714790749298, 0.13756180558514317,
                         -0.15974959411732936, 0.6454506154525829, 0.746909345363432,
                         -0.025259373415633023, -0.7590512397735246, 0.6505406823965161)
        topology_rows = (Row * 1)(row(index=topology_index, pose=topology_pose))
        topology_out = results(1)
        assert call(prism(), topology_faces, topology_rows, topology_out) == OK
        assert topology_out[0].path_status == PATH_OK and topology_out[0].entry_status == ENTRY_OK
        assert topology_out[0].direction_index_derivative_available
        assert not topology_out[0].entry_index_derivative_available
        topology_step = 2.0e-6 * topology_index
        active_sets = {
            trace_entry_measure_prism(DISTANCES, 0.73, topology_faces, topology_index + offset,
                                      INCIDENT, topology_pose).active_constraints
            for offset in (-topology_step, -topology_step / 2.0, 0.0,
                           topology_step / 2.0, topology_step)
        }
        assert len(active_sets) > 1
        lib.LUMICE_ANALYTIC_ReleaseDiagnosticFieldResult(byref(topology_out[0]))

        for result in out:
            lib.LUMICE_ANALYTIC_ReleaseDiagnosticFieldResult(byref(result))
            assert not result.storage and result.interface_count == 0
            lib.LUMICE_ANALYTIC_ReleaseDiagnosticFieldResult(byref(result))
        """
    )


@pytest.mark.slow
def test_ctypes_batch_stride_errors_pyramid_and_concurrent_calls() -> None:
    _run_child(
        """
        assert lib.LUMICE_ANALYTIC_EvaluateDiagnosticFieldBatch(None, None, 0, None, 0, None) == OK

        class PaddedResult(Structure):
            _fields_ = ResultV7._fields_ + [("tail", c_uint64)]

        class GuardedPaddedBatch(Structure):
            _fields_ = [("before", c_uint64), ("rows", PaddedResult * 2), ("after", c_uint64)]

        guarded = GuardedPaddedBatch()
        guarded.before = 0x13579BDF2468ACE0
        guarded.after = 0x02468ACE13579BDF
        padded = guarded.rows
        for result in padded:
            ctypes.memset(byref(result), 0x5A, sizeof(result))
            result.struct_size = sizeof(PaddedResult)
        padded[0].tail = 0x123456789ABCDEF0
        padded[1].tail = 0x0FEDCBA987654321
        rows = (Row * 2)(row(), row(index=1.32))
        assert call(prism(), FACES, rows, padded) == OK
        assert padded[0].storage and padded[1].storage
        assert padded[0].tail == 0x123456789ABCDEF0 and padded[1].tail == 0x0FEDCBA987654321
        for result in padded:
            lib.LUMICE_ANALYTIC_ReleaseDiagnosticFieldResult(byref(result))
            assert not result.storage and result.interface_count == 0
            lib.LUMICE_ANALYTIC_ReleaseDiagnosticFieldResult(byref(result))
        assert padded[0].tail == 0x123456789ABCDEF0 and padded[1].tail == 0x0FEDCBA987654321
        assert guarded.before == 0x13579BDF2468ACE0 and guarded.after == 0x02468ACE13579BDF

        nonuniform = results(2)
        nonuniform[1].struct_size = RESULT_V7_SIZE - sizeof(c_void_p)
        assert call(prism(), FACES, rows, nonuniform) == INVALID_VALUE
        assert not nonuniform[0].storage and nonuniform[0].row_error == 0

        class ShortResult(Structure):
            _fields_ = [("struct_size", c_uint32),
                        ("payload", ctypes.c_ubyte * (RESULT_V7_SIZE - sizeof(c_void_p) - sizeof(c_uint32)))]

        assert sizeof(ShortResult) == RESULT_V7_SIZE - sizeof(c_void_p)

        class GuardedShortBatch(Structure):
            _fields_ = [("before", c_uint64), ("rows", ShortResult * 2), ("after", c_uint64)]

        short_guarded = GuardedShortBatch()
        ctypes.memset(byref(short_guarded), 0xA5, sizeof(short_guarded))
        short_guarded.before = 0x1122334455667788
        short_guarded.after = 0x8877665544332211
        short = short_guarded.rows
        short[0].struct_size = sizeof(ShortResult)
        short[1].struct_size = sizeof(ShortResult)
        assert call(prism(), FACES, rows, short) == INVALID_VALUE
        assert bytes(short[0].payload) == bytes(len(short[0].payload))
        assert bytes(short[1].payload) == bytes([0xA5]) * len(short[1].payload)
        assert short_guarded.before == 0x1122334455667788 and short_guarded.after == 0x8877665544332211

        bad_faces = results(2)
        assert call(prism(), (3, 99), rows, bad_faces) == INVALID_VALUE
        assert all(not result.storage and result.row_error == 0 for result in bad_faces)
        for result in bad_faces:
            assert ctypes.string_at(ctypes.addressof(result) + sizeof(c_uint32),
                                    RESULT_V7_SIZE - sizeof(c_uint32)) == bytes(
                                        RESULT_V7_SIZE - sizeof(c_uint32))

        pyramid = Crystal(kind=1, height=0.5, upper_h=0.25, lower_h=0.6,
                          upper_wedge_deg=27.996455531220374, lower_wedge_deg=38.5704386184827)
        for index, value in enumerate((1.0, 1.1, 0.9, 1.0, 1.2, 0.95)):
            pyramid.face_distance[index] = value
        pyramid_pose = (0.5428686575453007, -0.8316390089186302, -0.11691954284807132,
                        0.4876689198046398, 0.42550852801054295, -0.7623132671329267,
                        0.6837197125368983, 0.3568179527926528, 0.6365597405219098)
        pyramid_faces = (13, 15, 26, 28)
        pyramid_rows = (Row * 1)(row(pose=pyramid_pose))
        pyramid_out = results(1)
        assert call(pyramid, pyramid_faces, pyramid_rows, pyramid_out) == OK
        expected = trace_path(pyramid_faces, 1.31, INCIDENT, pyramid_pose,
                              upper_wedge_deg=pyramid.upper_wedge_deg, lower_wedge_deg=pyramid.lower_wedge_deg)
        assert expected.valid and pyramid_out[0].path_status == PATH_OK
        for got, want in zip(pyramid_out[0].outgoing_direction, expected.outgoing):
            close(got, want, 3e-12)
        assert pyramid_out[0].interfaces[2].face_number == 26 and pyramid_out[0].tir_margin_count == 2
        lib.LUMICE_ANALYTIC_ReleaseDiagnosticFieldResult(byref(pyramid_out[0]))

        def worker(index):
            local_rows = (Row * 4)(*[row(index=1.31 + index * 1e-5) for _ in range(4)])
            local_out = results(4)
            rc = call(prism(), FACES, local_rows, local_out)
            values = tuple(local_out[0].outgoing_direction)
            for result in local_out:
                lib.LUMICE_ANALYTIC_ReleaseDiagnosticFieldResult(byref(result))
            return rc, values

        with ThreadPoolExecutor(max_workers=8) as pool:
            concurrent = list(pool.map(worker, range(8)))
        assert all(rc == OK and all(math.isfinite(x) for x in values) for rc, values in concurrent)
        """
    )
