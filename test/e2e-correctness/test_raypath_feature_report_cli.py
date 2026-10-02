"""Target-free ``Lumice raypath --report`` through the real static CLI.

The numeric assertions are the compact Lumice-side acceptance slice of the independently generated
diagnostic reference at revision 366b7946e07231f6a714216279f57e07b0b71957. The production report
does not read that research artifact at runtime.
"""

from __future__ import annotations

import json
import struct
from pathlib import Path

import pytest

from test.e2e.runner import find_lumice_binary, get_project_root, run_lumice

_ROOT = get_project_root()
_RANDOM = _ROOT / "test" / "e2e" / "configs" / "raypath_feature_random_regular.json"
_PLATE = _ROOT / "test" / "e2e" / "configs" / "raypath_feature_rhombic_plate.json"
_REFERENCE_WAVELENGTHS = (694.3628981235904, 430.0197374077313)
# Scene wavelength parameters are float32; the analytic reference endpoints are doubles.
_SCENE_REFERENCE_WAVELENGTHS = tuple(
    struct.unpack("f", struct.pack("f", wavelength))[0]
    for wavelength in _REFERENCE_WAVELENGTHS
)


@pytest.fixture(scope="module")
def reference_configs(tmp_path_factory):
    """Author the independent reference spectrum explicitly instead of assuming a default."""
    directory = tmp_path_factory.mktemp("feature-reference-spectrum")
    paths = {}
    for name, source in (("random", _RANDOM), ("plate", _PLATE)):
        config = json.loads(source.read_text())
        config["scene"]["light_source"]["spectrum"] = [
            {"wavelength": wavelength, "weight": 1}
            for wavelength in _REFERENCE_WAVELENGTHS
        ]
        path = directory / f"{name}.json"
        path.write_text(json.dumps(config))
        paths[name] = path
    return paths


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


def test_random_315_report_is_a_separate_document_with_both_feature_mechanisms(reference_configs):
    result = _report(reference_configs["random"], "3-1-5", "--events", "8192")
    assert result.returncode == 0, result.stderr
    doc = json.loads(result.stdout)
    assert doc["schema"] == "lumice.path-feature-report"
    assert doc["schema_version"] == 3
    assert doc["scene_measure"]["spectrum_nodes"][0]["source"] == "scene_discrete"
    assert "target" not in doc["meta"]
    assert len(doc["physical_l2_members"]) == 24
    features = {feature["id"]: feature for feature in doc["features"]}
    ordinary = features["random_regular.3-1-5.solar_dispersion_edge"]
    assert ordinary["evidence_status"] == "confirmed"
    assert ordinary["positions"][0]["deviation_deg"] == pytest.approx(21.612019265, abs=1e-5)
    assert ordinary["positions"][1]["deviation_deg"] == pytest.approx(22.371148713, abs=1e-5)
    assert features["random_regular.3-1-5.solar_caustic_candidate"]["evidence_status"] == "candidate"
    tir = features["random_regular.3-1-5.antisolar_tir_blue_band"]
    assert tir["evidence_status"] == "confirmed"
    assert tir["positions"][0]["deviation_deg"] == pytest.approx(130.358885186, abs=1e-6)
    assert tir["positions"][1]["deviation_deg"] == pytest.approx(138.854666882, abs=1e-6)
    assert tir["metrics"]["production_blue_red_ratio"] > 1
    assert tir["metrics"]["without_internal_R_blue_red_ratio"] < 1
    assert tir["metrics"]["sample_count"] == 14
    assert features["random_regular.3-1-5.exit_gate"]["visible"] is False
    assert "[raypath report]" in result.stderr


def test_random_35_report_pins_the_ordinary_minimum_deviation_boundary(reference_configs):
    result = _report(reference_configs["random"], "3-5", "--events", "8192")
    assert result.returncode == 0, result.stderr
    doc = json.loads(result.stdout)
    assert doc["meta"]["requested_faces"] == [3, 5]
    assert len(doc["features"]) == 1
    feature = doc["features"][0]
    assert feature["id"] == "random_regular.3-5.inner_edge"
    assert feature["evidence_status"] == "confirmed"
    assert feature["mechanism"] == "ordinary minimum-deviation dispersion"
    assert feature["positions"][0]["deviation_deg"] == pytest.approx(21.612019265, abs=1e-5)
    assert feature["positions"][1]["deviation_deg"] == pytest.approx(22.371148713, abs=1e-5)


def test_report_states_the_feature_families_it_does_not_enumerate():
    result = _report(_RANDOM, "3-1-5", "--events", "8192")
    assert result.returncode == 0, result.stderr
    limitations = json.loads(result.stdout)["limitations"]
    assert "not an all-sky feature enumerator" in limitations
    assert any("open or multiple components" in limitation for limitation in limitations)
    assert any("general oriented kink curves" in limitation for limitation in limitations)
    assert any("cone-crystal empty results" in limitation for limitation in limitations)
    assert any("rank-0 feature discovery" in limitation for limitation in limitations)


def test_rhombic_plate_keeps_plus_and_minus_120_separate_from_spherical_distance(reference_configs):
    result = _report(reference_configs["plate"], "1-3-4-2", "--events", "8192")
    assert result.returncode == 0, result.stderr
    doc = json.loads(result.stdout)
    assert len(doc["physical_l2_members"]) == 2
    assert {tuple(member["faces"]) for member in doc["physical_l2_members"]} == {
        (1, 3, 4, 2),
        (1, 3, 8, 2),
    }
    assert len(doc["features"]) == 2
    positions = [feature["positions"][0] for feature in doc["features"]]
    assert sorted(position["relative_solar_azimuth_deg"] for position in positions) == [-120.0, 120.0]
    for position in positions:
        assert position["spherical_separation_deg"] == pytest.approx(117.599764152, abs=1e-9)
    expected_red = 0.0008701214984864252
    expected_blue = 0.0008910040533079951
    expected_ratio = 1.023999584952096
    for member in doc["physical_l2_members"]:
        wavelengths = {sample["wavelength"]["nm"]: sample for sample in member["wavelengths"]}
        assert sorted(wavelengths) == sorted(_SCENE_REFERENCE_WAVELENGTHS)
        for wavelength in wavelengths.values():
            brightness = wavelength["brightness"]
            assert brightness["status"] == "supported"
            assert brightness["fine_positive_count"] > 0
            assert brightness["fine_mean_A_times_T"] > 0
            assert brightness["absolute_difference"] < 3e-9
            assert len(brightness["fixed_outgoing_direction"]) == 3
            assert brightness["direction_residual_max_rad"] < 1e-12
        red = wavelengths[_SCENE_REFERENCE_WAVELENGTHS[0]]["brightness"]["fine_mean_A_times_T"]
        blue = wavelengths[_SCENE_REFERENCE_WAVELENGTHS[1]]["brightness"]["fine_mean_A_times_T"]
        assert red == pytest.approx(expected_red, abs=1e-9)
        assert blue == pytest.approx(expected_blue, abs=1e-9)
        assert blue / red == pytest.approx(expected_ratio, abs=1e-9)


def test_rhombic_plate_1352_keeps_the_blue_l2_members_and_tint_values():
    # Use double-precision diagnostic wavelengths for the reference's strict ratio oracle.
    # Scene wavelengths are float32, whose quantization changes this ratio by about 1.8e-8.
    members_by_wavelength = []
    for wavelength in _REFERENCE_WAVELENGTHS:
        result = _report(_PLATE, "1-3-5-2", "--events", "8192", "--wavelength", repr(wavelength))
        assert result.returncode == 0, result.stderr
        doc = json.loads(result.stdout)
        assert doc["meta"]["sun"]["altitude_deg"] == pytest.approx(9.0)
        assert doc["meta"]["orientation_measure"] == "Rz(theta), theta uniform under dtheta/(2*pi); c axis exactly vertical"
        members = {tuple(member["faces"]): member for member in doc["physical_l2_members"]}
        assert set(members) == {(1, 3, 5, 2), (1, 3, 7, 2)}
        assert len(doc["scene_measure"]["spectrum_nodes"]) == 1
        assert doc["scene_measure"]["spectrum_nodes"][0]["wavelength_nm"] == wavelength
        members_by_wavelength.append(members)
    expected_red = 0.0001104536442463968
    expected_blue = 0.0001803007775026744
    expected_ratio = 1.6323660367462811
    for faces in members_by_wavelength[0]:
        red = members_by_wavelength[0][faces]["wavelengths"][0]["brightness"]["fine_mean_A_times_T"]
        blue = members_by_wavelength[1][faces]["wavelengths"][0]["brightness"]["fine_mean_A_times_T"]
        assert red == pytest.approx(expected_red, abs=1e-9)
        assert blue == pytest.approx(expected_blue, abs=1e-9)
        assert blue / red == pytest.approx(expected_ratio, abs=1e-9)


def test_report_default_detector_and_measure_use_the_same_scene_spectrum():
    result = _report(_RANDOM, "3-5", "--events", "64")
    assert result.returncode == 0, result.stderr
    document = json.loads(result.stdout)
    nodes = document["scene_measure"]["spectrum_nodes"]
    expected_wavelengths = [405.0 + 50.0 * index for index in range(8)]
    assert [node["wavelength_nm"] for node in nodes] == expected_wavelengths
    assert all(node["source"] == "scene_illuminant_uniform_380_780" for node in nodes)
    assert all(node["weight"] > 0 for node in nodes)
    assert document["physical_l2_members"]
    for member in document["physical_l2_members"]:
        assert [sample["wavelength"]["nm"] for sample in member["wavelengths"]] == expected_wavelengths


def test_report_output_file_is_atomic_and_stdout_stays_empty(tmp_path):
    output = tmp_path / "feature-report.json"
    result = _report(_RANDOM, "3-5", "--events", "64", "-o", str(output))
    assert result.returncode == 0, result.stderr
    assert result.stdout == ""
    assert json.loads(output.read_text())["schema"] == "lumice.path-feature-report"
    assert not Path(str(output) + ".tmp").exists()


def test_report_events_controls_the_actual_scene_measure_budget():
    documents = []
    for events in (64, 128):
        result = _report(_RANDOM, "3-5", "--events", str(events))
        assert result.returncode == 0, result.stderr
        document = json.loads(result.stdout)
        measure = document["scene_measure"]
        assert measure["requested_sample_count"] == events
        expected_rows = (
            len(measure["member_chains"])
            * len(measure["spectrum_nodes"])
            * len(measure["sun_nodes"])
            * events
        )
        assert measure["evaluated_row_count"] == expected_rows
        assert f"{events} integration samples" in result.stderr
        documents.append(document)
    assert (
        documents[1]["scene_measure"]["evaluated_row_count"]
        == 2 * documents[0]["scene_measure"]["evaluated_row_count"]
    )


def test_report_default_budget_and_help_describe_schema_three_scene_sampling():
    result = _report(_RANDOM, "3-5", "--wavelength", "550")
    assert result.returncode == 0, result.stderr
    document = json.loads(result.stdout)
    assert document["schema_version"] == 3
    assert document["scene_measure"]["requested_sample_count"] == 8192
    assert "8192 integration samples" in result.stderr

    help_result = run_lumice(["raypath", "-h"])
    assert help_result.returncode == 0
    assert "report schema 3" in help_result.stdout
    assert "joint scene-measure samples" in help_result.stdout


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
