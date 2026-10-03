"""Independent ctypes consumer for the general feature-discovery ABI.

The cases load only liblumice_analytic in child interpreters. Their equal-area sky oracle and
constraint root are defined here rather than through engine or raypath helpers.
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


def _find_library() -> Path:
    if not SHARED_OUT.is_dir():
        pytest.skip(f"no shared build at {SHARED_OUT} (./scripts/build.sh -sj release)")
    hits = sorted(path for path in SHARED_OUT.rglob(_PATTERN[_platform()]) if path.is_file())
    if not hits:
        pytest.skip("shared analytic library is not present in this build")
    assert len(hits) == 1, hits
    return hits[0]


_PRELUDE = textwrap.dedent(
    r"""
    import ctypes, math
    from concurrent.futures import ThreadPoolExecutor
    from ctypes import (CFUNCTYPE, POINTER, Structure, byref, c_char_p, c_double, c_int, c_uint8,
                        c_uint32, c_uint64, c_void_p, sizeof)

    class Provenance(Structure):
        _fields_ = [("member_index", c_int), ("layer_index", c_int), ("interface_index", c_int),
                    ("spectrum_node_id", c_int), ("source_node_id", c_int), ("sample_index", c_int)]

    class Constraint(Structure):
        _fields_ = [("struct_size", c_uint32), ("name", c_char_p), ("kind", c_int),
                    ("layer_index", c_int), ("interface_index", c_int), ("value", c_double),
                    ("numerically_available", c_int), ("gradient_available", c_int),
                    ("gradient", POINTER(c_double))]

    class Sample(Structure):
        _fields_ = [("struct_size", c_uint32), ("sample_id", c_uint64), ("provenance", Provenance),
                    ("measure_kind", c_int), ("support_dimension", c_int), ("finite_width", c_int),
                    ("coordinates", POINTER(c_double)), ("active_coordinates", POINTER(c_int)),
                    ("direction", c_double * 3), ("weight", c_double),
                    ("direction_jacobian_available", c_int), ("direction_jacobian", POINTER(c_double)),
                    ("direction_jacobian_column_available", POINTER(c_uint8)),
                    ("direction_jacobian_error", c_double), ("direction_jacobian_resolution", c_double),
                    ("constraint_count", c_int), ("constraint_stride", c_uint32),
                    ("constraints", POINTER(Constraint)), ("numerically_available", c_int),
                    ("accumulates_measure", c_int), ("mapping_evidence_kind", c_int),
                    ("image_dimension_upper_bound", c_int), ("mapping_error_bound", c_double)]

    class Edge(Structure):
        _fields_ = [("first", c_int), ("second", c_int), ("parameter_distance", c_double)]

    class CellAxis(Structure):
        _fields_ = [("cell_id", c_int), ("coordinate_index", c_int), ("lower", c_int),
                    ("center", c_int), ("upper", c_int), ("parameter_span", c_double)]

    class Parameter(Structure):
        _fields_ = [("role", c_int), ("group_id", c_int)]

    class Scope(Structure):
        _fields_ = [("scope_id", c_int), ("cell_id", c_int), ("kind", c_int)]

    class Batch(Structure):
        _fields_ = [("struct_size", c_uint32), ("version", c_uint32), ("coordinate_dimension", c_int),
                    ("visited_row_count", c_uint64), ("complete_visit", c_int),
                    ("materialization_complete", c_int), ("sample_count", c_int),
                    ("sample_stride", c_uint32), ("samples", POINTER(Sample)), ("edge_count", c_int),
                    ("edges", POINTER(Edge)), ("cell_axis_count", c_int),
                    ("cell_axes", POINTER(CellAxis)), ("parameter_descriptor_count", c_int),
                    ("parameter_descriptors", POINTER(Parameter)), ("scope_count", c_int),
                    ("scopes", POINTER(Scope))]

    class Request(Structure):
        _fields_ = [("provenance", Provenance), ("coordinate_dimension", c_int),
                    ("coordinates", POINTER(c_double))]

    CALLBACK = CFUNCTYPE(c_int, POINTER(Request), POINTER(Sample), c_void_p)

    class Options(Structure):
        _fields_ = [("struct_size", c_uint32), ("margin_tolerance", c_double),
                    ("rank_relative_tolerance", c_double), ("sky_merge_tolerance", c_double),
                    ("maximum_refinement_steps", c_int), ("sky_z_bins", c_int),
                    ("sky_azimuth_bins", c_int)]

    class Candidate(Structure):
        _fields_ = [("mechanism", c_int), ("status", c_int), ("provenance", Provenance),
                    ("direction", c_double * 3), ("support_dimension", c_int), ("mapping_rank", c_int),
                    ("singular_values", c_double * 2), ("weighted_mass", c_double),
                    ("has_weight_sides", c_int), ("weight_sides", c_double * 2), ("residual", c_double),
                    ("resolution", c_double), ("active_constraint_count", c_int),
                    ("active_constraints", POINTER(c_char_p)), ("reason", c_char_p)]

    class Mechanism(Structure):
        _fields_ = [("mechanism", c_int), ("status", c_int), ("candidate_count", c_int),
                    ("reason", c_char_p)]

    class SkyNode(Structure):
        _fields_ = [("direction", c_double * 3), ("value", c_double), ("normalized_value", c_double),
                    ("gradient_norm", c_double), ("hessian_eigenvalues", c_double * 2),
                    ("error", c_double), ("resolution", c_double), ("sample_count", c_int),
                    ("status", c_int)]

    class Result(Structure):
        _fields_ = [("struct_size", c_uint32), ("visited_row_count", c_uint64),
                    ("evaluated_sample_count", c_int), ("complete_visit", c_int),
                    ("materialization_complete", c_int), ("candidate_count", c_int),
                    ("candidates", POINTER(Candidate)), ("mechanism_count", c_int),
                    ("mechanisms", POINTER(Mechanism)), ("sky_field_count", c_int),
                    ("sky_field", POINTER(SkyNode)), ("storage", c_void_p)]

    class CandidateScope(Structure):
        _fields_ = [("struct_size", c_uint32), ("scope_id", c_int), ("kind", c_int),
                    ("active_coordinate_count", c_int), ("active_coordinates", POINTER(c_int)),
                    ("active_parameters", POINTER(Parameter)), ("fixed_spectrum_node_id", c_int),
                    ("fixed_source_node_id", c_int)]

    OK, NULL_ARG, INVALID_VALUE = 0, 1, 2
    CONFIRMED, CANDIDATE, NUMERICAL_INCOMPLETE = 0, 1, 3
    ATOM, CONTINUOUS = 0, 1
    TIR = 2
    OPTICAL_KINK, MEASURE_ATOM, BRIGHTNESS_MAXIMUM = 3, 6, 9
    NO_MAPPING_EVIDENCE, EXACT_IMAGE_DIMENSION_UPPER_BOUND = 0, 1
    PARAMETER_POSE, SCOPE_CONDITIONAL = 4, 1

    lib = ctypes.CDLL(LIB)
    lib.LUMICE_ANALYTIC_GetApiVersion.restype = c_int
    assert lib.LUMICE_ANALYTIC_GetApiVersion() == 12
    lib.LUMICE_ANALYTIC_DiscoverFeatures.restype = c_int
    lib.LUMICE_ANALYTIC_DiscoverFeatures.argtypes = [POINTER(Batch), POINTER(Options), CALLBACK, c_void_p,
                                                      POINTER(Result)]
    lib.LUMICE_ANALYTIC_ReleaseFeatureDiscoveryResult.restype = None
    lib.LUMICE_ANALYTIC_ReleaseFeatureDiscoveryResult.argtypes = [POINTER(Result)]
    lib.LUMICE_ANALYTIC_GetFeatureCandidateScope.restype = c_int
    lib.LUMICE_ANALYTIC_GetFeatureCandidateScope.argtypes = [POINTER(Result), c_int, POINTER(CandidateScope)]

    def options():
        return Options(sizeof(Options), 0.0, 0.0, 0.0, 0, 8, 16)

    def output():
        value = Result()
        ctypes.memset(byref(value), 0x5A, sizeof(value))
        value.struct_size = sizeof(Result)
        return value

    def sky_batch(reverse=False):
        solid_angle = 4.0 * math.pi / (8 * 16)
        samples = []
        for iz in range(8):
            z = -1.0 + (iz + 0.5) * 2.0 / 8
            radius = math.sqrt(1.0 - z * z)
            for ia in range(16):
                phi = (ia + 0.5) * 2.0 * math.pi / 16
                distance = min(abs(ia - 4), 16 - abs(ia - 4))
                density = 400.0 - 7.0 * abs(iz - 4) - 3.0 * distance * distance
                sample = Sample()
                sample.struct_size = sizeof(Sample)
                sample.sample_id = iz * 16 + ia + 1
                sample.measure_kind = ATOM
                sample.direction[:] = (radius * math.cos(phi), radius * math.sin(phi), z)
                sample.weight = density * solid_angle
                sample.constraint_stride = sizeof(Constraint)
                sample.numerically_available = 1
                sample.accumulates_measure = 1
                samples.append(sample)
        if reverse:
            samples.reverse()
        array = (Sample * len(samples))(*samples)
        batch = Batch(sizeof(Batch), 3, 0, len(samples), 1, 1, len(samples), sizeof(Sample), array, 0, None, 0, None)
        return batch, array

    def discover(batch, callback=CALLBACK()):
        result = output()
        opts = options()
        rc = lib.LUMICE_ANALYTIC_DiscoverFeatures(byref(batch), byref(opts), callback, None, byref(result))
        return rc, result
    """
)


def _run_child(body: str) -> None:
    code = f"LIB = {str(_find_library())!r}\n" + _PRELUDE + textwrap.dedent(body)
    proc = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True, timeout=120)
    assert proc.returncode == 0, f"child failed (rc={proc.returncode}):\n{proc.stdout}\n{proc.stderr}"


def test_equal_area_oracle_order_independence_release_and_threads() -> None:
    _run_child(
        """
        def one(reverse=False):
            batch, keep = sky_batch(reverse)
            rc, result = discover(batch)
            assert rc == OK
            assert result.visited_row_count == 128 and result.evaluated_sample_count == 128
            assert result.mechanism_count == 11 and result.sky_field_count == 128
            maxima = [result.candidates[i] for i in range(result.candidate_count)
                      if result.candidates[i].mechanism == BRIGHTNESS_MAXIMUM]
            assert any(candidate.status == CONFIRMED for candidate in maxima)
            node = result.sky_field[4 * 16 + 4]
            assert abs(node.normalized_value - 400.0) < 1.0e-12
            snapshot = tuple(result.sky_field[i].value for i in range(result.sky_field_count))
            lib.LUMICE_ANALYTIC_ReleaseFeatureDiscoveryResult(byref(result))
            assert result.struct_size == sizeof(Result) and not result.storage and result.candidate_count == 0
            lib.LUMICE_ANALYTIC_ReleaseFeatureDiscoveryResult(byref(result))
            lib.LUMICE_ANALYTIC_ReleaseFeatureDiscoveryResult(None)
            return snapshot

        assert one(False) == one(True)
        with ThreadPoolExecutor(max_workers=4) as pool:
            snapshots = list(pool.map(lambda index: one(bool(index & 1)), range(12)))
        assert all(snapshot == snapshots[0] for snapshot in snapshots)
        """
    )


def test_callback_refines_nonfirst_interface_and_preserves_owned_strings() -> None:
    _run_child(
        """
        coordinates = [(c_double * 1)(-1.0), (c_double * 1)(1.0)]
        active = [(c_int * 1)(0), (c_int * 1)(0)]
        constraints = []
        samples = []
        for index, x in enumerate((-1.0, 1.0)):
            constraint = Constraint(sizeof(Constraint), b"layer[1].internal[2].tir", TIR, 1, 2, x, 1, 0, None)
            constraint_array = (Constraint * 1)(constraint)
            constraints.append(constraint_array)
            sample = Sample()
            sample.struct_size = sizeof(Sample)
            sample.sample_id = index + 1
            sample.measure_kind = CONTINUOUS
            sample.support_dimension = 1
            sample.coordinates = coordinates[index]
            sample.active_coordinates = active[index]
            sample.direction[:] = (1.0, 0.0, 0.0)
            sample.weight = 1.0
            sample.constraint_count = 1
            sample.constraint_stride = sizeof(Constraint)
            sample.constraints = constraint_array
            sample.numerically_available = 1
            sample.accumulates_measure = 1
            samples.append(sample)
        sample_array = (Sample * 2)(*samples)
        edges = (Edge * 1)(Edge(0, 1, 2.0))
        batch = Batch(sizeof(Batch), 3, 1, 2, 1, 1, 2, sizeof(Sample), sample_array, 1, edges, 0, None)

        @CALLBACK
        def refine(request, out, user_data):
            x = request.contents.coordinates[0]
            coordinate = (c_double * 1)(x)
            active_coordinate = (c_int * 1)(0)
            constraint = Constraint(sizeof(Constraint), b"layer[1].internal[2].tir", TIR, 1, 2, x, 1, 0, None)
            constraint_array = (Constraint * 1)(constraint)
            value = out.contents
            value.sample_id = 100
            value.provenance = request.contents.provenance
            value.measure_kind = CONTINUOUS
            value.support_dimension = 1
            value.coordinates = coordinate
            value.active_coordinates = active_coordinate
            value.direction[:] = (math.cos(x), math.sin(x), 0.0)
            value.weight = 1.0
            value.constraint_count = 1
            value.constraint_stride = sizeof(Constraint)
            value.constraints = constraint_array
            value.numerically_available = 1
            value.accumulates_measure = 0
            return 1

        rc, result = discover(batch, refine)
        assert rc == OK
        matches = [result.candidates[i] for i in range(result.candidate_count)
                   if result.candidates[i].mechanism == OPTICAL_KINK]
        assert len(matches) == 1
        candidate = matches[0]
        assert candidate.status == CONFIRMED and candidate.provenance.interface_index == 2
        assert candidate.residual <= 1.0e-8 and abs(candidate.direction[0] - 1.0) <= 1.0e-12
        assert candidate.active_constraints[0] == b"layer[1].internal[2].tir"
        assert b"callback refinement" in candidate.reason
        lib.LUMICE_ANALYTIC_ReleaseFeatureDiscoveryResult(byref(result))
        """
    )


def test_callback_rejects_wrong_branch_and_coordinates() -> None:
    _run_child(
        """
        def one(defect):
            keep = []
            samples = []
            for index, x in enumerate((-1.0, 1.0)):
                coordinate = (c_double * 1)(x)
                active = (c_int * 1)(0)
                constraint = Constraint(sizeof(Constraint), b"actual-domain", 0, 2, 3, x, 1, 0, None)
                constraints = (Constraint * 1)(constraint)
                keep.extend((coordinate, active, constraints))
                value = Sample()
                value.struct_size = sizeof(Sample)
                value.sample_id = index + 1
                value.measure_kind = CONTINUOUS
                value.support_dimension = 1
                value.coordinates = coordinate
                value.active_coordinates = active
                value.direction[:] = (math.cos(x), math.sin(x), 0.0)
                value.weight = 1.0
                value.constraint_count = 1
                value.constraint_stride = sizeof(Constraint)
                value.constraints = constraints
                value.numerically_available = 1
                value.accumulates_measure = 1
                samples.append(value)
            sample_array = (Sample * 2)(*samples)
            edge_array = (Edge * 1)(Edge(0, 1, 2.0))
            batch = Batch(sizeof(Batch), 3, 1, 2, 1, 1, 2, sizeof(Sample), sample_array,
                          1, edge_array, 0, None)

            @CALLBACK
            def refine(request, output, user_data):
                x = request.contents.coordinates[0]
                coordinate = (c_double * 1)(x if defect == "branch" else x + 5.0)
                active = (c_int * 1)(0)
                constraint = Constraint(sizeof(Constraint), b"actual-domain", 0, 2, 3, 0.0, 1, 0, None)
                constraints = (Constraint * 1)(constraint)
                keep.extend((coordinate, active, constraints))
                value = output.contents
                value.sample_id = 100
                value.provenance = request.contents.provenance
                if defect == "branch":
                    value.provenance.member_index += 1
                value.measure_kind = CONTINUOUS
                value.support_dimension = 1
                value.coordinates = coordinate
                value.active_coordinates = active
                value.direction[:] = (math.cos(x), math.sin(x), 0.0)
                value.weight = 0.0
                value.constraint_count = 1
                value.constraint_stride = sizeof(Constraint)
                value.constraints = constraints
                value.numerically_available = 1
                value.accumulates_measure = 0
                return 1

            rc, result = discover(batch, refine)
            assert rc == OK
            boundaries = [result.candidates[index] for index in range(result.candidate_count)
                          if result.candidates[index].mechanism == 1]
            assert len(boundaries) == 1 and boundaries[0].status == NUMERICAL_INCOMPLETE
            lib.LUMICE_ANALYTIC_ReleaseFeatureDiscoveryResult(byref(result))

        one("branch")
        one("coordinates")
        """
    )


def test_exact_mapping_certificate_is_required_for_a_continuous_atom() -> None:
    _run_child(
        """
        keep = []
        samples = []
        for index, x in enumerate((-1.0, 0.0, 1.0)):
            coordinates = (c_double * 1)(x)
            active = (c_int * 1)(0)
            jacobian = (c_double * 3)(0.0, 0.0, 0.0)
            available = (c_uint8 * 1)(1)
            keep.extend((coordinates, active, jacobian, available))
            sample = Sample()
            sample.struct_size = sizeof(Sample)
            sample.sample_id = index + 1
            sample.measure_kind = CONTINUOUS
            sample.support_dimension = 1
            sample.coordinates = coordinates
            sample.active_coordinates = active
            sample.direction[:] = (1.0, 0.0, 0.0)
            sample.weight = 1.0 if index == 1 else 0.0
            sample.direction_jacobian_available = 1
            sample.direction_jacobian = jacobian
            sample.direction_jacobian_column_available = available
            sample.direction_jacobian_resolution = 1.0e-4
            sample.constraint_stride = sizeof(Constraint)
            sample.numerically_available = 1
            sample.accumulates_measure = int(index == 1)
            samples.append(sample)
        samples[1].mapping_evidence_kind = EXACT_IMAGE_DIMENSION_UPPER_BOUND
        samples[1].image_dimension_upper_bound = 0
        sample_array = (Sample * 3)(*samples)
        edges = (Edge * 2)(Edge(0, 1, 1.0), Edge(1, 2, 1.0))
        axes = (CellAxis * 1)(CellAxis(7, 0, 0, 1, 2, 2.0))
        batch = Batch(sizeof(Batch), 3, 1, 1, 1, 1, 3, sizeof(Sample), sample_array,
                      2, edges, 1, axes)

        rc, result = discover(batch)
        assert rc == OK
        atoms = [result.candidates[i] for i in range(result.candidate_count)
                 if result.candidates[i].mechanism == MEASURE_ATOM]
        assert len(atoms) == 1 and atoms[0].status == CONFIRMED and atoms[0].weighted_mass == 1.0
        lib.LUMICE_ANALYTIC_ReleaseFeatureDiscoveryResult(byref(result))

        sample_array[1].mapping_evidence_kind = NO_MAPPING_EVIDENCE
        rc, result = discover(batch)
        assert rc == OK
        assert not [result.candidates[i] for i in range(result.candidate_count)
                    if result.candidates[i].mechanism == MEASURE_ATOM]
        lib.LUMICE_ANALYTIC_ReleaseFeatureDiscoveryResult(byref(result))
        """
    )


def test_v4_conditional_scope_metadata_uses_parallel_candidate_query() -> None:
    _run_child(
        """
        keep = []

        def fold(index, x, original):
            coordinate = (c_double * 1)(x)
            active = (c_int * 1)(0)
            phase = x * x
            jacobian = (c_double * 3)(-2 * x * math.sin(phase), 2 * x * math.cos(phase), 0.0)
            available = (c_uint8 * 1)(1)
            keep.extend((coordinate, active, jacobian, available))
            value = Sample()
            value.struct_size = sizeof(Sample)
            value.sample_id = index
            value.measure_kind = CONTINUOUS
            value.support_dimension = 1
            value.coordinates = coordinate
            value.active_coordinates = active
            value.direction[:] = (math.cos(phase), math.sin(phase), 0.0)
            value.weight = float(original)
            value.direction_jacobian_available = 1
            value.direction_jacobian = jacobian
            value.direction_jacobian_column_available = available
            value.direction_jacobian_resolution = 1.0e-6
            value.constraint_stride = sizeof(Constraint)
            value.numerically_available = 1
            value.accumulates_measure = int(original)
            return value

        samples = (Sample * 3)(fold(1, .25, True), fold(2, -.75, False), fold(3, 1.25, False))
        axes = (CellAxis * 1)(CellAxis(7, 0, 1, 0, 2, 2.0))
        parameters = (Parameter * 1)(Parameter(PARAMETER_POSE, 3))
        scopes = (Scope * 1)(Scope(42, 7, SCOPE_CONDITIONAL))
        batch = Batch()
        batch.struct_size = sizeof(Batch)
        batch.version = 4
        batch.coordinate_dimension = 1
        batch.visited_row_count = 1
        batch.complete_visit = 1
        batch.materialization_complete = 1
        batch.sample_count = 3
        batch.sample_stride = sizeof(Sample)
        batch.samples = samples
        batch.cell_axis_count = 1
        batch.cell_axes = axes
        batch.parameter_descriptor_count = 1
        batch.parameter_descriptors = parameters
        batch.scope_count = 1
        batch.scopes = scopes

        @CALLBACK
        def refine(request, output, user_data):
            value = fold(100, request.contents.coordinates[0], False)
            value.provenance = request.contents.provenance
            output[0] = value
            return 1

        rc, result = discover(batch, refine)
        assert rc == OK
        found = False
        for index in range(result.candidate_count):
            if result.candidates[index].mechanism != 0:
                continue
            metadata = CandidateScope()
            metadata.struct_size = sizeof(CandidateScope)
            assert lib.LUMICE_ANALYTIC_GetFeatureCandidateScope(byref(result), index, byref(metadata)) == OK
            if metadata.kind == SCOPE_CONDITIONAL:
                assert metadata.scope_id == 42 and metadata.active_coordinate_count == 1
                assert metadata.active_coordinates[0] == 0
                assert metadata.active_parameters[0].role == PARAMETER_POSE
                assert metadata.active_parameters[0].group_id == 3
                found = True
        assert found
        lib.LUMICE_ANALYTIC_ReleaseFeatureDiscoveryResult(byref(result))
        metadata = CandidateScope()
        metadata.struct_size = sizeof(CandidateScope)
        assert lib.LUMICE_ANALYTIC_GetFeatureCandidateScope(byref(result), 0, byref(metadata)) == INVALID_VALUE
        """
    )


def test_malformed_stride_version_and_required_pointer_are_call_errors() -> None:
    _run_child(
        """
        batch, keep = sky_batch()
        out = output()
        opts = options()
        batch.sample_stride = sizeof(Sample) - 1
        assert lib.LUMICE_ANALYTIC_DiscoverFeatures(byref(batch), byref(opts), CALLBACK(), None, byref(out)) == INVALID_VALUE
        assert out.struct_size == sizeof(Result) and not out.storage

        batch.sample_stride = sizeof(Sample)
        batch.version = 99
        assert lib.LUMICE_ANALYTIC_DiscoverFeatures(byref(batch), byref(opts), CALLBACK(), None, byref(out)) == INVALID_VALUE

        batch.version = 1
        batch.samples = None
        assert lib.LUMICE_ANALYTIC_DiscoverFeatures(byref(batch), byref(opts), CALLBACK(), None, byref(out)) == NULL_ARG
        assert lib.LUMICE_ANALYTIC_DiscoverFeatures(None, byref(opts), CALLBACK(), None, byref(out)) == NULL_ARG
        assert lib.LUMICE_ANALYTIC_DiscoverFeatures(byref(batch), byref(opts), CALLBACK(), None, None) == NULL_ARG
        """
    )


def test_v2_dynamic_coordinates_and_frozen_v1_prefix() -> None:
    _run_child(
        """
        dimension = 21
        coordinates = (c_double * dimension)(*([0.0] * dimension))
        active = (c_int * dimension)(*range(dimension))
        jacobian = (c_double * (3 * dimension))(*([0.0] * (3 * dimension)))
        available = (c_uint8 * dimension)(*([1] * dimension))
        sample = Sample()
        sample.struct_size = sizeof(Sample)
        sample.sample_id = 1
        sample.measure_kind = CONTINUOUS
        sample.support_dimension = dimension
        sample.finite_width = 1
        sample.coordinates = coordinates
        sample.active_coordinates = active
        sample.direction[:] = (0.0, 0.0, 1.0)
        sample.weight = 1.0
        sample.direction_jacobian_available = 1
        sample.direction_jacobian = jacobian
        sample.direction_jacobian_column_available = available
        sample.direction_jacobian_resolution = 1.0e-4
        sample.constraint_stride = sizeof(Constraint)
        sample.numerically_available = 1
        sample.accumulates_measure = 1
        samples = (Sample * 1)(sample)
        batch = Batch(sizeof(Batch), 3, dimension, 1, 1, 1, 1, sizeof(Sample), samples, 0, None, 0, None)
        rc, result = discover(batch)
        assert rc == OK and result.evaluated_sample_count == 1
        lib.LUMICE_ANALYTIC_ReleaseFeatureDiscoveryResult(byref(result))

        # The version-1 contract keeps its published 16-coordinate ceiling even when passed in
        # the new, larger outer structs.
        batch.version = 1
        result = output()
        opts = options()
        assert lib.LUMICE_ANALYTIC_DiscoverFeatures(byref(batch), byref(opts), CALLBACK(), None,
                                                    byref(result)) == INVALID_VALUE

        # A real version-2 caller's frozen sample prefix remains accepted without acquiring evidence.
        class Version2Sample(Structure):
            _fields_ = Sample._fields_[:-3]

        version2_sample = Version2Sample()
        version2_sample.struct_size = sizeof(Version2Sample)
        version2_sample.sample_id = 2
        version2_sample.measure_kind = ATOM
        version2_sample.direction[:] = (1.0, 0.0, 0.0)
        version2_sample.weight = 0.5
        version2_sample.constraint_stride = sizeof(Constraint)
        version2_sample.numerically_available = 1
        version2_sample.accumulates_measure = 1
        version2_samples = (Version2Sample * 1)(version2_sample)
        version2_batch = Batch(sizeof(Batch), 2, 0, 1, 1, 1, 1, sizeof(Version2Sample),
                               ctypes.cast(version2_samples, POINTER(Sample)), 0, None, 0, None)
        result = output()
        assert lib.LUMICE_ANALYTIC_DiscoverFeatures(byref(version2_batch), byref(opts), CALLBACK(), None,
                                                    byref(result)) == OK
        lib.LUMICE_ANALYTIC_ReleaseFeatureDiscoveryResult(byref(result))

        # A real version-1 caller's frozen prefixes remain accepted by the version-12 library.
        class LegacySample(Structure):
            _fields_ = Sample._fields_[:-4]

        class LegacyBatch(Structure):
            _fields_ = Batch._fields_[:11]

        legacy_sample = LegacySample()
        legacy_sample.struct_size = sizeof(LegacySample)
        legacy_sample.sample_id = 2
        legacy_sample.measure_kind = ATOM
        legacy_sample.direction[:] = (1.0, 0.0, 0.0)
        legacy_sample.weight = 0.5
        legacy_sample.constraint_stride = sizeof(Constraint)
        legacy_sample.numerically_available = 1
        legacy_samples = (LegacySample * 1)(legacy_sample)
        legacy_batch = LegacyBatch(sizeof(LegacyBatch), 1, 0, 1, 1, 1, 1, sizeof(LegacySample),
                                   ctypes.cast(legacy_samples, POINTER(Sample)), 0, None)
        result = output()
        assert lib.LUMICE_ANALYTIC_DiscoverFeatures(
            ctypes.cast(byref(legacy_batch), POINTER(Batch)), byref(opts), CALLBACK(), None,
            byref(result)) == OK
        assert result.evaluated_sample_count == 1
        lib.LUMICE_ANALYTIC_ReleaseFeatureDiscoveryResult(byref(result))
        """
    )
