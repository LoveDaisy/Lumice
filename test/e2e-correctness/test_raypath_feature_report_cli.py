"""Target-free ``Lumice raypath --report`` through the real static CLI.

The schema-two route consumes physical input and reports bounded numerical evidence.
Scientific kernels have independent fixtures in the analytic and composition layers;
this layer owns the CLI, spectrum choice, scope, budget and output-file contracts.
"""

from __future__ import annotations

import json
import math
from pathlib import Path

import pytest

from test.e2e.runner import find_lumice_binary, get_project_root, run_lumice

_ROOT = get_project_root()
_RANDOM = _ROOT / "test" / "e2e" / "configs" / "raypath_feature_random_regular.json"
_PLATE = _ROOT / "test" / "e2e" / "configs" / "raypath_feature_rhombic_plate.json"


@pytest.fixture(scope="module", autouse=True)
def _binary():
    try:
        find_lumice_binary()
    except FileNotFoundError as error:
        pytest.skip(str(error))


def _report(config: Path, path: str, *args: str):
    return run_lumice(
        ["raypath", "-f", str(config), "--crystal", "1", "--path", path, "--report", *args]
    )


def test_v2_reader_rejects_a_v3_document_naming_both_versions():
    class _Result:
        stdout = json.dumps({"schema": "lumice.path-feature-report", "schema_version": 3})

    with pytest.raises(AssertionError, match=r"schema_version 3; this reader requires schema_version 2"):
        _load_report(_Result(), 2)


def _load_report(result, expect_version: int):
    """Parse a report document, refusing a version this reader does not understand.

    The version gate is the in-repo face of the reader discipline (v2 readers reject v3 and
    vice versa): the error names BOTH versions, so a consumer that silently misreads a newer
    document can never come back green here.
    """
    doc = json.loads(result.stdout)
    version = doc.get("schema_version")
    if version != expect_version:
        raise AssertionError(
            f"document is schema_version {version}; this reader requires schema_version {expect_version}"
        )
    return doc


def test_bounded_nonfixed_report_completes_without_claiming_global_coverage():
    result = _report(_RANDOM, "3-5", "--events", "64", "--wavelength", "550")
    assert result.returncode == 0, result.stderr
    doc = _load_report(result, 3)
    assert doc["outcome"] == "completed"
    assert doc["mc_evidence"]["unfinished"] == []
    assert not doc["budgets"]["exhausted"]
    assert doc["coverage"][1]["limitations"]
    assert doc["features"]["candidate"]
    # A completed bounded search is not a proof that every local hypothesis worked: the MC
    # side still carries unfinished records (the record face of the demoted evidence).
    unfinished_records = [r for r in doc["mc_evidence"]["records"] if r["evidence"] == "unfinished"]
    assert unfinished_records


def test_315_report_keeps_conditional_tir_distinct_from_observed_colour():
    result = _report(_RANDOM, "3-1-5", "--events", "8192", "--wavelength", "550")
    assert result.returncode == 0, result.stderr
    doc = _load_report(result, 3)
    assert doc["schema"] == "lumice.path-feature-report"
    assert doc["schema_version"] == 3
    assert "target" not in doc
    assert len(doc["scope"]["physical_members"]) == 24
    candidates = [f for f in (doc["mc_evidence"]["records"]) if f["kind"] == "conditional_internal_tir"]
    assert candidates
    for feature in candidates:
        assert feature["evidence"] == "candidate"
        slot = feature["internal_slot"]
        event = feature["interface_event"]
        assert event["value"]["entry"]["area"] > 0
        assert abs(event["value"]["interfaces"][slot]["discriminant"]) < 1e-10
        assert feature["source_token"] is not None
    assert not any("blue_band" in f["kind"] for f in (doc["mc_evidence"]["records"]))
    assert "[raypath report]" in result.stderr


def test_report_physical_radius_is_not_the_smoothed_brightness_peak():
    import math

    result = _report(_RANDOM, "3-5", "--events", "8192", "--wavelength", "550")
    assert result.returncode == 0, result.stderr
    doc = _load_report(result, 3)
    assert doc["scope"]["layers"][0]["representative_faces"] == [3, 5]
    edges = [f for f in (doc["mc_evidence"]["records"]) if "physical_position" in f]
    assert edges
    for feature in edges:
        position = feature["physical_position"]
        n = position["source"]["refractive_index"]
        assert position["deviation_rad"] == pytest.approx(2 * math.asin(n / 2) - math.pi / 3, abs=1e-10)
        assert position["value"]["entry"]["area"] > 0
        assert feature["geometry"]["sky_points"]
    assert "not the scene SPD" in doc["scope"]["spectrum_scope"]


def test_default_report_consumes_actual_continuous_spectrum_and_records_scope():
    result = _report(_RANDOM, "3-5")
    assert result.returncode == 0, result.stderr
    doc = _load_report(result, 3)
    assert len(doc["scope"]["spectrum"]) == 33
    assert "continuous" in doc["scope"]["spectrum_scope"]
    assert sum(row["measure_mass"] for row in doc["scope"]["spectrum"]) == pytest.approx(1)
    if doc["mc_evidence"]["spectral_verification"]["optical_evaluations"] == 0:
        # A bounded call may exhaust its deadline before spectral refinement.
        # Zero work is valid only with the explicit incomplete outcome/reason.
        assert doc["budgets"]["exhausted"], doc["budgets"]
        assert doc["outcome"] == "partial"
        assert "continuous spectral quadrature refinement incomplete" in doc["mc_evidence"]["unfinished"]
    assert doc["mc_evidence"]["observation_options"]["kernel"] == "normalized_vMF"
    assert doc["outcome"] in {"completed", "partial"}
    assert doc["coverage"][1]["limitations"]  # bounded search is not an absence certificate


def test_rhombic_plate_uses_physical_members_and_measured_peaks():
    import math

    result = _report(_PLATE, "1-3-4-2", "--events", "8192", "--wavelength", "550")
    assert result.returncode == 0, result.stderr
    doc = _load_report(result, 3)
    assert {tuple(member) for member in doc["scope"]["physical_members"]} == {(1, 3, 4, 2), (1, 3, 8, 2)}
    peaks = [f for f in (doc["mc_evidence"]["records"]) if f["kind"] == "intensity_peak" and f["evidence"] == "actual"]
    assert peaks
    relative = []
    for feature in peaks:
        q = feature["geometry"]["sky_points"][0]
        az = math.degrees(math.atan2(q[1], q[0]))
        relative.append((az - 180 + 180) % 360 - 180)
        assert feature["field_points"][0]["xyz"][1]["value"] > 0
    assert min(abs(x - 120) for x in relative) < .1
    assert min(abs(x + 120) for x in relative) < .1


@pytest.mark.parametrize("budget", [["--budget-ms", "1"], ["--max-evaluations", "100"]])
def test_report_low_budget_returns_partial_json_instead_of_empty_feature_claim(budget):
    result = _report(_RANDOM, "3-5", *budget)
    assert result.returncode == 0, result.stderr
    doc = _load_report(result, 3)
    assert doc["outcome"] == "partial"
    assert doc["budgets"]["exhausted"]
    if budget[0] == "--max-evaluations":
        assert doc["budgets"]["optical_evaluations"] == 100


def test_report_output_file_is_atomic_and_stdout_stays_empty(tmp_path):
    output = tmp_path / "feature-report.json"
    result = _report(_RANDOM, "3-5", "--events", "64", "-o", str(output))
    assert result.returncode == 0, result.stderr
    assert result.stdout == ""
    assert json.loads(output.read_text())["schema_version"] == 3
    assert not Path(str(output) + ".tmp").exists()


@pytest.mark.parametrize(
    "extra, named",
    [
        (["--target", "20,25"], "--target"),
        (["--grid", "12"], "--grid"),
        (["--warm", "old.json"], "--warm"),
    ],
)
def test_report_rejects_target_only_options(extra, named):
    result = _report(_RANDOM, "3-5", *extra)
    assert result.returncode == 1
    assert named in result.stderr
    assert "Usage:" in result.stderr
    assert result.stdout == ""


@pytest.mark.parametrize(
    "args, named",
    [
        (["--report", "--path", "3-5"], "--crystal"),
        (["--report", "--crystal", "1"], "--path"),
        (["--report", "--crystal", "not-an-id", "--path", "3-5"], "--crystal"),
        (["--crystal", "1", "--path", "3-5", "--events", "63", "--report"], "--events"),
    ],
)
def test_report_parse_errors_write_usage_only_to_stderr_regardless_of_option_order(args, named):
    result = run_lumice(["raypath", "-f", str(_RANDOM), *args])
    assert result.returncode == 1
    assert named in result.stderr
    assert "Usage:" in result.stderr
    assert result.stdout == ""


def test_report_multicrystal_is_a_structured_zero_work_refusal():
    result = _report(_RANDOM, "(3-5)->(1-3)")
    assert result.returncode == 0, result.stderr
    doc = _load_report(result, 3)
    assert doc["outcome"] == "unsupported_multicrystal"
    assert doc["requested_path_layers"] == [[3, 5], [1, 3]]
    assert doc["features"]["actual"] == []
    assert doc["budgets"]["optical_evaluations"] == 0


def test_endpoint_bisection_budget_is_partial_not_a_physical_stop():
    # This cap interrupts the final source curve inside its optical bracket.
    # Other report stages still finish; their completion cannot hide this stop.
    result = _report(_RANDOM, "3-1-5", "--events", "8192", "--wavelength", "550",
                     "--max-evaluations", "405930")
    assert result.returncode == 0, result.stderr
    doc = _load_report(result, 3)
    assert doc["outcome"] == "partial"
    assert doc["budgets"]["exhausted"]
    assert doc["budgets"]["optical_evaluations"] <= 405930
    features = doc["mc_evidence"]["records"]
    curves = [c for f in features for c in f.get("source_curves", [])]
    stopped = [c for c in curves if c["termination"] == 6]
    assert stopped
    assert any(c["points"] for c in stopped)
    complete = _report(_RANDOM, "3-1-5", "--events", "8192", "--wavelength", "550")
    assert complete.returncode == 0, complete.stderr
    full = _load_report(complete, 3)
    assert full["outcome"] == "completed"
    assert not full["budgets"]["exhausted"]
    full_features = full["mc_evidence"]["records"]
    full_curves = {f["source_token"]: f["source_curves"] for f in full_features if "source_curves" in f}
    for feature in features:
        for index, curve in enumerate(feature.get("source_curves", [])):
            if curve["termination"] == 6:
                finished = full_curves[feature["source_token"]][index]
                assert finished["termination"] == 3
                assert finished["events"]


@pytest.mark.parametrize("altitude, bounded_axis", [(0, False), (25, False), (0, True), (25, True)])
@pytest.mark.parametrize("diameter", [0, 2])
def test_declared_sun_boundary_is_the_physical_cap_rim(tmp_path, altitude, bounded_axis, diameter):
    config = json.loads(_RANDOM.read_text())
    config["scene"]["light_source"].update(diameter=diameter, altitude=altitude)
    if bounded_axis:
        config["crystal"][0]["axis"]["azimuth"]["std"] = .5
    path = tmp_path / "sun-boundary.json"
    path.write_text(json.dumps(config))
    result = _report(path, "3-5", "--events", "64", "--wavelength", "550")
    assert result.returncode == 0, result.stderr
    doc = _load_report(result, 3)
    # Restricted orientation can leave unrelated SO(3) searches unfinished;
    # these declared-source candidates must still be evaluated within budget.
    assert not doc["budgets"]["exhausted"]
    events = [f for f in doc["mc_evidence"]["records"] if "declared_source_event" in f]
    sun_events = [f for f in events if "sun.cap_radial" in dict(f["declared_source_event"]["coordinates"])]
    if diameter == 0:
        assert not sun_events
        if bounded_axis:
            # The absence is specific to the point sun, not an empty report.
            assert any("axis.azimuth_uniform" in dict(f["declared_source_event"]["coordinates"]) for f in events)
        return

    assert any(f["kind"] == "declared_source_boundary" for f in sun_events)
    corners = [f for f in sun_events if f["kind"] == "declared_source_corner"]
    assert len(corners) == (2 if bounded_axis else 0)
    if bounded_axis:
        assert {dict(f["declared_source_event"]["coordinates"])["axis.azimuth_uniform"] for f in corners} == {0, 1}
    # Incident rays point away from the sun; derive the centre from the scene,
    # independently of the product latent-coordinate transform.
    altitude_rad = math.radians(altitude)
    center = [-math.cos(altitude_rad), 0, -math.sin(altitude_rad)]
    for feature in sun_events:
        event = feature["declared_source_event"]
        source, value = event["source"], event["value"]
        incident = source["incident"]
        cosine = sum(a * b for a, b in zip(center, incident)) / math.sqrt(sum(x * x for x in incident))
        angle = math.acos(max(-1, min(1, cosine)))
        # The product cap uses float cos(radius) and sqrt(1-cos²(radius)).
        # At a 1-degree radius, float rounding allows a few microradians;
        # 1e-5 rad also covers its float frame rotation, not sampling error.
        assert angle == pytest.approx(math.radians(diameter / 2), abs=1e-5)
        assert value["path_valid"]
        assert value["entry"]["area"] > value["entry"]["area_threshold"]
        assert value["interface_product"] > 0
        member = doc["scope"]["physical_members"][doc["sources"][str(feature["source_token"])]["member"]]
        # Independent two-interface Snell construction for this regular prism:
        # face 3 has normal +x, subsequent side normals are 60 degrees apart.
        # Use the actual returned pose/member, not the representative path.
        ray = incident
        for face, eta, sign in zip(member, [1 / source["refractive_index"], source["refractive_index"]], [-1, 1]):
            assert 3 <= face <= 8
            theta = math.radians(60 * (face - 3))
            pose = source["pose"]
            normal = [sign * (pose[3 * j] * math.cos(theta) + pose[3 * j + 1] * math.sin(theta)) for j in range(3)]
            incidence = sum(a * b for a, b in zip(ray, normal))
            assert incidence > 0
            discriminant = 1 - eta * eta * (1 - incidence * incidence)
            assert discriminant > 0
            ray = [eta * (v - incidence * n) + math.sqrt(discriminant) * n for v, n in zip(ray, normal)]
        assert value["outgoing"] == pytest.approx(ray, abs=1e-10)
        assert feature["geometry"]["sky_points"][0] == pytest.approx([-v for v in ray], abs=1e-10)
