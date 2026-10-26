"""Independent layout and lifetime checks against the product shared library."""

from __future__ import annotations

import ctypes as C
import math
import shutil
import subprocess
import sys
import textwrap
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
pytestmark = pytest.mark.slow


class PointRequest(C.Structure):
    _fields_ = [
        ("struct_size", C.c_size_t), ("crystal_id", C.c_int),
        ("faces", C.POINTER(C.c_int)), ("face_count", C.c_int),
        ("layer_face_counts", C.POINTER(C.c_int)), ("layer_count", C.c_int),
        ("target_altitude_deg", C.c_double), ("target_azimuth_deg", C.c_double),
        ("wavelength_nm", C.c_double), ("sample_count", C.c_int),
        ("sun_grid_lat_count", C.c_int), ("warm_json", C.c_void_p),
        ("warm_json_len", C.c_size_t), ("deadline_ms", C.c_int),
        ("interrupted", C.c_void_p),
    ]


class ReportRequest(C.Structure):
    _fields_ = [
        ("struct_size", C.c_size_t), ("crystal_id", C.c_int),
        ("faces", C.POINTER(C.c_int)), ("face_count", C.c_int),
        ("layer_face_counts", C.POINTER(C.c_int)), ("layer_count", C.c_int),
        ("wavelengths_nm", C.POINTER(C.c_double)),
        ("wavelength_weights", C.POINTER(C.c_double)),
        ("wavelength_count", C.c_int), ("sample_count", C.c_int),
        ("budget_ms", C.c_int), ("max_optical_evaluations", C.c_uint64),
        ("max_field_evaluations", C.c_uint64), ("bandwidth_rad", C.c_double),
        ("location_resolution_rad", C.c_double), ("scene_layer_plus_one", C.c_int),
        ("symmetry_bits_plus_one", C.c_int),
    ]


class SurveyRequest(C.Structure):
    _fields_ = [
        ("struct_size", C.c_size_t), ("crystal_id", C.c_int),
        ("faces", C.POINTER(C.c_int)), ("face_count", C.c_int),
        ("layer_face_counts", C.POINTER(C.c_int)), ("layer_count", C.c_int),
        ("wavelength_nm", C.c_double), ("budget_ms", C.c_int),
        ("symmetry_bits_plus_one", C.c_int), ("mc_ess_draws", C.c_int),
    ]


class ProbeRequest(C.Structure):
    _fields_ = [
        ("struct_size", C.c_size_t), ("crystal_id", C.c_int),
        ("faces", C.POINTER(C.c_int)), ("face_count", C.c_int),
        ("layer_face_counts", C.POINTER(C.c_int)), ("layer_count", C.c_int),
        ("along", C.c_int), ("at_altitude_deg", C.c_double),
        ("at_azimuth_deg", C.c_double), ("wavelengths_nm", C.POINTER(C.c_double)),
        ("wavelength_count", C.c_int), ("sample_count", C.c_int),
        ("warm_json", C.c_void_p), ("warm_json_len", C.c_size_t),
        ("budget_ms", C.c_int), ("interrupted", C.c_void_p),
    ]


class QueryOptions(C.Structure):
    _fields_ = [
        ("struct_size", C.c_size_t), ("deadline_ms", C.c_int),
        ("cancel", C.c_void_p), ("warm", C.c_void_p),
        ("include_context", C.c_int),
    ]


class SunGrid(C.Structure):
    _fields_ = [
        ("struct_size", C.c_size_t), ("lat_count", C.c_int), ("lon_count", C.c_int),
        ("deviation_rad", C.POINTER(C.c_double)), ("valid", C.POINTER(C.c_uint8)),
        ("entry_measure", C.POINTER(C.c_double)), ("cell_count", C.c_size_t),
    ]


class FuturePointRequest(C.Structure):
    _fields_ = [("base", PointRequest), ("tail", C.c_ubyte * 32)]


def _product_library():
    return C.CDLL(str(_product_library_path()))


def _product_library_path():
    name = {
        "darwin": "liblumice.dylib", "linux": "liblumice.so", "win32": "lumice.dll",
    }["win32" if sys.platform.startswith("win") else "linux" if sys.platform.startswith("linux") else "darwin"]
    matches = list((ROOT / "build" / "Release" / "shared").rglob(name))
    if not matches:
        pytest.fail("product shared library needs ./scripts/build.sh -sj release", pytrace=False)
    assert len(matches) == 1, matches
    install = ROOT / "build" / "cmake_install" / "shared"
    product = install / name if sys.platform.startswith("win") else install / "lib" / name
    assert product.is_file(), f"product library was built but not installed: {product}"
    return product


def _check_published_request_extents_and_future_tail_keep_borrowed_result_alive():
    assert (C.sizeof(PointRequest), C.sizeof(ReportRequest), C.sizeof(SurveyRequest),
            C.sizeof(ProbeRequest)) == (112, 120, 72, 112)
    assert (PointRequest.warm_json_len.offset + C.sizeof(C.c_size_t),
            ReportRequest.sample_count.offset + C.sizeof(C.c_int),
            SurveyRequest.symmetry_bits_plus_one.offset + C.sizeof(C.c_int)) == (96, 72, 64)
    assert (PointRequest.faces.offset, PointRequest.face_count.offset,
            PointRequest.layer_face_counts.offset, PointRequest.layer_count.offset,
            PointRequest.target_altitude_deg.offset, PointRequest.target_azimuth_deg.offset,
            PointRequest.wavelength_nm.offset, PointRequest.sample_count.offset,
            PointRequest.sun_grid_lat_count.offset, PointRequest.warm_json.offset,
            PointRequest.warm_json_len.offset, PointRequest.deadline_ms.offset) == (
                16, 24, 32, 40, 48, 56, 64, 72, 76, 80, 88, 96)
    lib = _product_library()
    lib.LUMICE_SceneFromJsonFile.argtypes = [C.c_char_p, C.POINTER(C.c_void_p)]
    lib.LUMICE_SceneFromJsonFile.restype = C.c_int
    lib.LUMICE_SceneDestroy.argtypes = [C.c_void_p]
    lib.LUMICE_PathWorkspaceCreate.argtypes = [C.c_void_p, C.POINTER(C.c_void_p), C.c_void_p, C.c_size_t]
    lib.LUMICE_PathWorkspaceCreate.restype = C.c_int
    lib.LUMICE_PathWorkspaceDestroy.argtypes = [C.c_void_p]
    lib.LUMICE_PathWorkspaceQueryPoint.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p,
                                                    C.POINTER(C.c_void_p), C.c_void_p, C.c_size_t]
    lib.LUMICE_PathWorkspaceQueryPoint.restype = C.c_int
    lib.LUMICE_SinglePathResultToJson.argtypes = [C.c_void_p, C.c_void_p, C.c_size_t, C.POINTER(C.c_size_t)]
    lib.LUMICE_SinglePathResultToJson.restype = C.c_int
    lib.LUMICE_PathPointGetSunGrid.argtypes = [C.c_void_p, C.c_void_p]
    lib.LUMICE_PathPointGetSunGrid.restype = C.c_int
    lib.LUMICE_SinglePathResultDestroy.argtypes = [C.c_void_p]

    scene = C.c_void_p()
    config = ROOT / "test" / "e2e" / "configs" / "raypath_analysis_halo_22.json"
    assert lib.LUMICE_SceneFromJsonFile(str(config).encode(), C.byref(scene)) == 0
    workspace = C.c_void_p()
    err = C.create_string_buffer(256)
    assert lib.LUMICE_PathWorkspaceCreate(scene, C.byref(workspace), err, len(err)) == 0, err.value
    lib.LUMICE_SceneDestroy(scene)

    faces = (C.c_int * 2)(3, 5)
    counts = (C.c_int * 1)(2)
    request = PointRequest()
    request.crystal_id = 1
    request.faces = faces
    request.face_count = 2
    request.layer_face_counts = counts
    request.layer_count = 1
    request.target_altitude_deg = 20
    request.target_azimuth_deg = 25
    request.sample_count = 100
    request.sun_grid_lat_count = 12
    options = QueryOptions()
    options.struct_size = C.sizeof(options)
    options.deadline_ms = 15000
    options.include_context = 1
    result = C.c_void_p()
    try:
        request.struct_size = 96
        assert lib.LUMICE_PathWorkspaceQueryPoint(workspace, C.byref(request), C.byref(options),
                                                  C.byref(result), err, len(err)) == 0, err.value
        lib.LUMICE_SinglePathResultDestroy(result)
        for invalid in (95, 104):
            request.struct_size = invalid
            result = C.c_void_p(1)
            assert lib.LUMICE_PathWorkspaceQueryPoint(workspace, C.byref(request), C.byref(options),
                                                      C.byref(result), err, len(err)) == 5
            assert result.value is None
            assert err.value.startswith(b"invalid_argument:")

        future = FuturePointRequest()
        future.base = request
        C.memset(C.addressof(future) + C.sizeof(PointRequest), 0xA5, 32)
        future.base.struct_size = 128
        result = C.c_void_p()
        assert lib.LUMICE_PathWorkspaceQueryPoint(workspace, C.byref(future), C.byref(options),
                                                  C.byref(result), err, len(err)) == 0, err.value
        assert C.string_at(C.addressof(future) + 112, 32) == bytes([0xA5] * 32)
    finally:
        lib.LUMICE_PathWorkspaceDestroy(workspace)

    assert C.sizeof(SunGrid) == 48
    grid = SunGrid()
    grid.struct_size = C.sizeof(grid)
    assert lib.LUMICE_PathPointGetSunGrid(result, C.byref(grid)) == 0
    assert (grid.lat_count, grid.lon_count, grid.cell_count) == (12, 24, 288)
    assert bool(grid.deviation_rad) and bool(grid.valid) and bool(grid.entry_measure)
    adjacent = C.cast(C.cast(grid.deviation_rad, C.c_void_p).value + 8, C.POINTER(C.c_double))[0]
    assert adjacent == grid.deviation_rad[1] or (math.isnan(adjacent) and math.isnan(grid.deviation_rad[1]))
    assert C.cast(C.cast(grid.valid, C.c_void_p).value + 1, C.POINTER(C.c_uint8))[0] == grid.valid[1]
    valid_cells = [i for i in range(grid.cell_count) if grid.valid[i]]
    assert valid_cells and math.isfinite(grid.deviation_rad[valid_cells[0]])

    class FutureGrid(C.Structure):
        _fields_ = [("base", SunGrid), ("tail", C.c_ubyte * 16)]

    extended = FutureGrid()
    extended.base.struct_size = C.sizeof(extended)
    C.memset(C.addressof(extended) + C.sizeof(SunGrid), 0xA5, 16)
    assert lib.LUMICE_PathPointGetSunGrid(result, C.byref(extended)) == 0
    assert C.string_at(C.addressof(extended) + 48, 16) == bytes([0xA5] * 16)
    too_small = FutureGrid()
    C.memset(C.addressof(too_small), 0xA5, C.sizeof(too_small))
    too_small.base.struct_size = 47
    untouched = C.string_at(C.addressof(too_small) + 47, 17)
    assert lib.LUMICE_PathPointGetSunGrid(result, C.byref(too_small)) == 2
    assert C.string_at(C.addressof(too_small) + 47, 17) == untouched

    length = C.c_size_t()
    try:
        assert lib.LUMICE_SinglePathResultToJson(result, None, 0, C.byref(length)) == 0
        assert length.value > 4
        small = C.create_string_buffer(4)
        C.memset(small, ord("x"), 4)
        full_length = C.c_size_t()
        assert lib.LUMICE_SinglePathResultToJson(result, small, 4, C.byref(full_length)) == 0
        assert full_length.value == length.value
        assert small.raw[3] == 0
        assert small.raw[:3] != b"xxx"
    finally:
        lib.LUMICE_SinglePathResultDestroy(result)


def _check_other_request_families_accept_published_extents_and_preserve_future_tails():
    lib = _product_library()
    lib.LUMICE_SceneFromJsonFile.argtypes = [C.c_char_p, C.POINTER(C.c_void_p)]
    lib.LUMICE_SceneFromJsonFile.restype = C.c_int
    lib.LUMICE_SceneDestroy.argtypes = [C.c_void_p]
    lib.LUMICE_PathWorkspaceCreate.argtypes = [C.c_void_p, C.POINTER(C.c_void_p), C.c_void_p, C.c_size_t]
    lib.LUMICE_PathWorkspaceCreate.restype = C.c_int
    lib.LUMICE_PathWorkspaceDestroy.argtypes = [C.c_void_p]
    faces = (C.c_int * 2)(3, 5)
    counts = (C.c_int * 1)(2)
    config = ROOT / "test" / "e2e" / "configs" / "raypath_analysis_halo_22.json"
    scene = C.c_void_p()
    assert lib.LUMICE_SceneFromJsonFile(str(config).encode(), C.byref(scene)) == 0
    workspace = C.c_void_p()
    err = C.create_string_buffer(256)
    assert lib.LUMICE_PathWorkspaceCreate(scene, C.byref(workspace), err, len(err)) == 0, err.value
    lib.LUMICE_SceneDestroy(scene)
    options = QueryOptions()
    options.struct_size = C.sizeof(options)
    options.deadline_ms = 15000
    options.include_context = 1
    cases = [
        (ReportRequest, "LUMICE_PathWorkspaceQueryReport", "LUMICE_PathFeatureReportDestroy", 72, 96),
        (SurveyRequest, "LUMICE_PathWorkspaceQuerySurvey", "LUMICE_PathSurveyDestroy", 64, 68),
        (ProbeRequest, "LUMICE_PathWorkspaceQueryProbe", "LUMICE_PathProbeDestroy", 112, 111),
    ]
    try:
        for typ, query_name, destroy_name, old_extent, invalid_extent in cases:
            request = typ()
            request.struct_size = old_extent
            request.crystal_id = 1
            request.faces = faces
            request.face_count = 2
            request.layer_face_counts = counts
            request.layer_count = 1
            if typ is ReportRequest:
                request.sample_count = 64
            elif typ is SurveyRequest:
                request.wavelength_nm = 550
                request.budget_ms = 15000
            else:
                request.at_altitude_deg = 20
                request.at_azimuth_deg = 25
                request.sample_count = 100
                request.budget_ms = 15000
            query = getattr(lib, query_name)
            query.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p,
                              C.POINTER(C.c_void_p), C.c_void_p, C.c_size_t]
            query.restype = C.c_int
            destroy = getattr(lib, destroy_name)
            destroy.argtypes = [C.c_void_p]
            result = C.c_void_p()
            assert query(workspace, C.byref(request), C.byref(options), C.byref(result), err, len(err)) == 0, err.value
            destroy(result)
            request.struct_size = invalid_extent
            result = C.c_void_p(1)
            assert query(workspace, C.byref(request), C.byref(options), C.byref(result), err, len(err)) == 5
            assert result.value is None
            assert err.value.startswith(b"invalid_argument:")

            class Future(C.Structure):
                _fields_ = [("base", typ), ("tail", C.c_ubyte * 32)]

            future = Future()
            future.base = request
            C.memset(C.addressof(future) + C.sizeof(typ), 0xA5, 32)
            future.base.struct_size = C.sizeof(typ) + 16
            result = C.c_void_p()
            assert query(workspace, C.byref(future), C.byref(options), C.byref(result), err, len(err)) == 0, err.value
            assert C.string_at(C.addressof(future) + C.sizeof(typ), 32) == bytes([0xA5] * 32)
            destroy(result)
    finally:
        lib.LUMICE_PathWorkspaceDestroy(workspace)


def _run_independent_client(check):
    _product_library_path()
    run = subprocess.run([sys.executable, "-I", str(Path(__file__).resolve()), check],
                         capture_output=True, text=True, check=False)
    assert run.returncode == 0, f"{check}: exit {run.returncode}\n{run.stdout}\n{run.stderr}"


def test_missing_shared_build_fails_closed(monkeypatch, tmp_path):
    monkeypatch.setattr(sys.modules[__name__], "ROOT", tmp_path)
    with pytest.raises(pytest.fail.Exception, match="product shared library needs"):
        _product_library_path()


def test_published_request_extents_and_future_tail_keep_borrowed_result_alive():
    _run_independent_client("point")


def test_other_request_families_accept_published_extents_and_preserve_future_tails():
    _run_independent_client("other")


def test_external_c_client_links_only_product_and_uses_typed_warm(tmp_path):
    if sys.platform.startswith("win"):
        pytest.skip("the independent Windows shared-export leg uses its own compiler smoke")
    compiler = shutil.which("cc")
    assert compiler is not None
    product = _product_library_path()
    include = tmp_path / "include"
    include.mkdir()
    for name in ("lumice_base.h", "lumice_scene.h", "lumice_raypath.h"):
        shutil.copy2(ROOT / "src" / "include" / name, include / name)
    source = tmp_path / "client.c"
    source.write_text(textwrap.dedent(r"""
        #include <lumice_scene.h>
        #include <lumice_raypath.h>
        #include <stddef.h>
        #include <stdlib.h>
        #include <string.h>

        int main(int argc, char** argv) {
          if (argc != 2) return 1;
          LUMICE_Scene* scene = NULL;
          if (LUMICE_SceneFromJsonFile(argv[1], &scene) != LUMICE_OK) return 2;
          LUMICE_PathWorkspace* workspace = NULL;
          if (LUMICE_PathWorkspaceCreate(scene, &workspace, NULL, 0) != LUMICE_OK) return 3;
          LUMICE_SceneDestroy(scene);
          const int faces[] = {3, 5};
          const int counts[] = {2};
          LUMICE_PathQueryOptions options = {0};
          options.struct_size = sizeof(options);
          options.deadline_ms = 15000;
          options.include_context = 1;
          LUMICE_PathSurveyRequest survey = {0};
          survey.struct_size = sizeof(survey);
          survey.crystal_id = 1;
          survey.faces = faces;
          survey.face_count = 2;
          survey.layer_face_counts = counts;
          survey.layer_count = 1;
          survey.wavelength_nm = 550;
          survey.budget_ms = 15000;
          LUMICE_PathSurvey* survey_result = NULL;
          if (LUMICE_PathWorkspaceQuerySurvey(workspace, &survey, &options, &survey_result, NULL, 0) != LUMICE_OK)
            return 4;
          LUMICE_PathSurveyInfo survey_info = {0};
          survey_info.struct_size = sizeof(survey_info);
          if (LUMICE_PathSurveyGetInfo(survey_result, &survey_info) != LUMICE_PATH_READ_OK ||
              survey_info.member_count == 0) return 9;
          LUMICE_PathSurveyEvidence survey_evidence = {0};
          survey_evidence.struct_size = sizeof(survey_evidence);
          if (LUMICE_PathSurveyGetEvidence(survey_result, LUMICE_PATH_EVIDENCE_DOCUMENT, 0,
                                           &survey_evidence) != LUMICE_PATH_READ_OK ||
              survey_evidence.state == 0) return 17;
          LUMICE_PathSurveyScope survey_scope = {0};
          survey_scope.struct_size = sizeof(survey_scope);
          if (LUMICE_PathSurveyGetScope(survey_result, &survey_scope) != LUMICE_PATH_READ_OK ||
              !survey_scope.scene_identity.data || survey_scope.scene_identity.length == 0) return 25;
          char* identity_copy = malloc(survey_scope.scene_identity.length);
          if (!identity_copy) return 31;
          memcpy(identity_copy, survey_scope.scene_identity.data, survey_scope.scene_identity.length);
          LUMICE_PathWarmData* warm = NULL;
          if (LUMICE_PathWarmDataFromSurvey(survey_result, &warm) != LUMICE_OK) return 5;
          options.warm = warm;
          LUMICE_SinglePathRequest point = {0};
          point.struct_size = sizeof(point);
          point.crystal_id = 1;
          point.faces = faces;
          point.face_count = 2;
          point.layer_face_counts = counts;
          point.layer_count = 1;
          point.target_altitude_deg = 20;
          point.target_azimuth_deg = 25;
          point.sample_count = 100;
          point.sun_grid_lat_count = 12;
          LUMICE_SinglePathResult* point_result = NULL;
          if (LUMICE_PathWorkspaceQueryPoint(workspace, &point, &options, &point_result, NULL, 0) != LUMICE_OK)
            return 6;
          LUMICE_PathPointInfo point_info = {0};
          point_info.struct_size = sizeof(point_info);
          if (LUMICE_PathPointGetInfo(point_result, &point_info) != LUMICE_PATH_READ_OK ||
              point_info.grid_lat_count != 12 || point_info.grid_lon_count != 24) return 10;
          LUMICE_PathSunGrid grid = {0};
          grid.struct_size = sizeof(grid);
          if (LUMICE_PathPointGetSunGrid(point_result, &grid) != LUMICE_PATH_READ_OK ||
              grid.cell_count != 288 || !grid.deviation_rad || !grid.valid || !grid.entry_measure) return 11;
          size_t valid_cell = 0;
          while (valid_cell < grid.cell_count && !grid.valid[valid_cell]) valid_cell++;
          if (valid_cell == grid.cell_count) return 12;
          const double borrowed_deviation = grid.deviation_rad[valid_cell];
          double* grid_copy = malloc(grid.cell_count * sizeof(double));
          if (!grid_copy) return 32;
          memcpy(grid_copy, grid.deviation_rad, grid.cell_count * sizeof(double));
          if (!(borrowed_deviation >= 0.0) || sizeof(grid.deviation_rad[0]) != 8 ||
              sizeof(grid.valid[0]) != 1) return 13;
          LUMICE_PathProbeRequest probe = {0};
          probe.struct_size = sizeof(probe);
          probe.crystal_id = 1;
          probe.faces = faces;
          probe.face_count = 2;
          probe.layer_face_counts = counts;
          probe.layer_count = 1;
          probe.at_altitude_deg = 20;
          probe.at_azimuth_deg = 25;
          probe.sample_count = 100;
          LUMICE_PathProbe* probe_result = NULL;
          if (LUMICE_PathWorkspaceQueryProbe(workspace, &probe, &options, &probe_result, NULL, 0) != LUMICE_OK)
            return 7;
          LUMICE_PathProbeInfo probe_info = {0};
          probe_info.struct_size = sizeof(probe_info);
          if (LUMICE_PathProbeGetInfo(probe_result, &probe_info) != LUMICE_PATH_READ_OK ||
              probe_info.profile_count == 0) return 14;
          LUMICE_PathProbeProfile profile = {0};
          profile.struct_size = sizeof(profile);
          if (LUMICE_PathProbeGetProfile(probe_result, 0, &profile) != LUMICE_PATH_READ_OK ||
              profile.state == 0 || profile.along != LUMICE_PATH_PROBE_ALONG_RADIAL ||
              !profile.semantics.data || profile.semantics.length == 0) return 18;
          char* semantics_copy = malloc(profile.semantics.length);
          if (!semantics_copy) return 33;
          memcpy(semantics_copy, profile.semantics.data, profile.semantics.length);
          LUMICE_PathQuantitySpec quantity = {0};
          quantity.struct_size = sizeof(quantity);
          if (LUMICE_PathProbeGetQuantity(probe_result, 0, &quantity) != LUMICE_PATH_READ_OK ||
              quantity.kind != LUMICE_PATH_QUANTITY_BAND_SUM ||
              quantity.unit != LUMICE_PATH_UNIT_PER_SR) return 24;
          options.warm = NULL;
          LUMICE_PathFeatureReportRequest report = {0};
          report.struct_size = sizeof(report);
          report.crystal_id = 1;
          report.faces = faces;
          report.face_count = 2;
          report.layer_face_counts = counts;
          report.layer_count = 1;
          report.sample_count = 64;
          LUMICE_PathFeatureReport* report_result = NULL;
          if (LUMICE_PathWorkspaceQueryReport(workspace, &report, &options, &report_result, NULL, 0) != LUMICE_OK)
            return 8;
          LUMICE_PathReportInfo report_info = {0};
          report_info.struct_size = sizeof(report_info);
          if (LUMICE_PathReportGetInfo(report_result, &report_info) != LUMICE_PATH_READ_OK ||
              report_info.execution != LUMICE_PATH_OUTCOME_COMPLETED ||
              report_info.coverage_count == 0) return 15;
          LUMICE_PathReportCoverage coverage = {0};
          coverage.struct_size = sizeof(coverage);
          if (LUMICE_PathReportGetCoverage(report_result, 0, &coverage) != LUMICE_PATH_READ_OK ||
              coverage.subject.length == 0 ||
              coverage.role != LUMICE_PATH_COVERAGE_BASE_INPUT ||
              coverage.status != LUMICE_PATH_COVERAGE_SUPPORTED) return 19;
          char* subject_copy = malloc(coverage.subject.length);
          if (!subject_copy) return 34;
          memcpy(subject_copy, coverage.subject.data, coverage.subject.length);
          const int multi_faces[] = {3, 5, 1, 3};
          const int multi_counts[] = {2, 2};
          report.faces = multi_faces;
          report.face_count = 4;
          report.layer_face_counts = multi_counts;
          report.layer_count = 2;
          LUMICE_PathFeatureReport* unsupported = NULL;
          if (LUMICE_PathWorkspaceQueryReport(workspace, &report, &options, &unsupported, NULL, 0) != LUMICE_OK)
            return 20;
          LUMICE_PathReportInfo unsupported_info = {0};
          unsupported_info.struct_size = sizeof(unsupported_info);
          if (LUMICE_PathReportGetInfo(unsupported, &unsupported_info) != LUMICE_PATH_READ_OK ||
              unsupported_info.outcome != LUMICE_PATH_REPORT_UNSUPPORTED_MULTICRYSTAL ||
              unsupported_info.requested_layer_count != 2) return 21;
          LUMICE_PathFaceSequence requested = {0};
          requested.struct_size = sizeof(requested);
          if (LUMICE_PathReportGetRequestedLayer(unsupported, 0, &requested) != LUMICE_PATH_READ_OK ||
              requested.face_count != 2 || requested.faces[0] != 3 || requested.faces[1] != 5) return 22;
          LUMICE_PathReportScope missing_scope = {0};
          missing_scope.struct_size = sizeof(missing_scope);
          if (LUMICE_PathReportGetScope(unsupported, &missing_scope) != LUMICE_PATH_READ_NOT_AVAILABLE)
            return 23;
          const int other_faces[] = {1, 2};
          survey.faces = other_faces;
          survey.wavelength_nm = 550;
          LUMICE_PathSurvey* replacement = NULL;
          if (LUMICE_PathWorkspaceQuerySurvey(workspace, &survey, &options, &replacement, NULL, 0) != LUMICE_OK)
            return 26;
          LUMICE_PathSurveyDestroy(replacement);
          LUMICE_PathWorkspaceDestroy(workspace);
          if (memcmp(grid.deviation_rad, grid_copy, grid.cell_count * sizeof(double)) != 0 ||
              requested.face_count != 2 || requested.faces[0] != 3 || requested.faces[1] != 5 ||
              memcmp(survey_scope.scene_identity.data, identity_copy, survey_scope.scene_identity.length) != 0 ||
              memcmp(profile.semantics.data, semantics_copy, profile.semantics.length) != 0 ||
              memcmp(coverage.subject.data, subject_copy, coverage.subject.length) != 0 ||
              LUMICE_PathSurveyGetScope(survey_result, &survey_scope) != LUMICE_PATH_READ_OK ||
              LUMICE_PathPointGetInfo(point_result, &point_info) != LUMICE_PATH_READ_OK ||
              LUMICE_PathProbeGetInfo(probe_result, &probe_info) != LUMICE_PATH_READ_OK ||
              LUMICE_PathReportGetInfo(report_result, &report_info) != LUMICE_PATH_READ_OK) return 16;
          LUMICE_PathSurveyDestroy(survey_result);
          LUMICE_Scene* second_scene = NULL;
          if (LUMICE_SceneFromJsonFile(argv[1], &second_scene) != LUMICE_OK) return 27;
          LUMICE_PathWorkspace* second_workspace = NULL;
          if (LUMICE_PathWorkspaceCreate(second_scene, &second_workspace, NULL, 0) != LUMICE_OK) return 28;
          LUMICE_SceneDestroy(second_scene);
          options.warm = warm;
          LUMICE_PathProbe* warm_probe = NULL;
          if (LUMICE_PathWorkspaceQueryProbe(second_workspace, &probe, &options, &warm_probe, NULL, 0) != LUMICE_OK)
            return 35;
          LUMICE_PathProbeInfo warm_probe_info = {0};
          warm_probe_info.struct_size = sizeof(warm_probe_info);
          LUMICE_PathContext warm_probe_context = {0};
          warm_probe_context.struct_size = sizeof(warm_probe_context);
          if (LUMICE_PathProbeGetInfo(warm_probe, &warm_probe_info) != LUMICE_PATH_READ_OK ||
              warm_probe_info.profile_count == 0 ||
              LUMICE_PathProbeGetContext(warm_probe, &warm_probe_context) != LUMICE_PATH_READ_OK ||
              warm_probe_context.source != LUMICE_PATH_CONTEXT_WARM) return 36;
          LUMICE_PathProbeDestroy(warm_probe);
          LUMICE_SinglePathResult* warm_point = NULL;
          if (LUMICE_PathWorkspaceQueryPoint(second_workspace, &point, &options, &warm_point, NULL, 0) != LUMICE_OK)
            return 29;
          LUMICE_PathContext warm_context = {0};
          warm_context.struct_size = sizeof(warm_context);
          if (LUMICE_PathPointGetContext(warm_point, &warm_context) != LUMICE_PATH_READ_OK ||
              warm_context.source != LUMICE_PATH_CONTEXT_WARM) return 30;
          LUMICE_SinglePathResultDestroy(warm_point);
          LUMICE_PathWorkspaceDestroy(second_workspace);
          LUMICE_PathWarmDataDestroy(warm);
          LUMICE_PathFeatureReportDestroy(unsupported);
          LUMICE_SinglePathResultDestroy(point_result);
          LUMICE_PathProbeDestroy(probe_result);
          LUMICE_PathFeatureReportDestroy(report_result);
          free(identity_copy);
          free(grid_copy);
          free(semantics_copy);
          free(subject_copy);
          return 0;
        }
    """), encoding="utf-8")
    executable = tmp_path / "client"
    deps = tmp_path / "client.d"
    compile_cmd = [compiler, "-std=c11", "-I", str(include), "-MMD", "-MF", str(deps),
                   str(source), str(product), f"-Wl,-rpath,{product.parent}", "-o", str(executable)]
    built = subprocess.run(compile_cmd, capture_output=True, text=True, check=False)
    assert built.returncode == 0, f"{compile_cmd}\n{built.stdout}\n{built.stderr}"
    header_dependencies = deps.read_text(encoding="utf-8")
    assert str(include) in header_dependencies
    assert str(ROOT / "src") not in header_dependencies
    config = ROOT / "test" / "e2e" / "configs" / "raypath_analysis_halo_22.json"
    run = subprocess.run([str(executable), str(config)], capture_output=True, text=True, check=False)
    assert run.returncode == 0, f"C consumer exited {run.returncode}\n{run.stdout}\n{run.stderr}"


if __name__ == "__main__":
    {"point": _check_published_request_extents_and_future_tail_keep_borrowed_result_alive,
     "other": _check_other_request_families_accept_published_extents_and_preserve_future_tails}[sys.argv[1]]()
