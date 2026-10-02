"""Target-free ``Lumice raypath --report`` through the real static CLI.

The numeric assertions are the compact Lumice-side acceptance slice of the independently generated
diagnostic reference at revision 366b7946e07231f6a714216279f57e07b0b71957. The production report
does not read that research artifact at runtime.
"""

from __future__ import annotations

import json
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


def test_random_315_report_is_a_separate_document_with_both_feature_mechanisms():
    result = _report(_RANDOM, "3-1-5", "--events", "8192")
    assert result.returncode == 0, result.stderr
    doc = json.loads(result.stdout)
    assert doc["schema"] == "lumice.path-feature-report"
    assert doc["schema_version"] == 1
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


def test_random_35_report_pins_the_ordinary_minimum_deviation_boundary():
    result = _report(_RANDOM, "3-5", "--events", "8192")
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


def test_rhombic_plate_keeps_plus_and_minus_120_separate_from_spherical_distance():
    result = _report(_PLATE, "1-3-4-2", "--events", "8192")
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
    for member in doc["physical_l2_members"]:
        wavelengths = {sample["wavelength"]["nm"]: sample for sample in member["wavelengths"]}
        assert sorted(wavelengths) == pytest.approx([430.019737408, 694.362898124], abs=1e-9)
        for wavelength in wavelengths.values():
            brightness = wavelength["brightness"]
            assert brightness["status"] == "supported"
            assert brightness["fine_positive_count"] > 0
            assert brightness["fine_mean_A_times_T"] > 0
            assert brightness["absolute_difference"] < 3e-9
            assert len(brightness["fixed_outgoing_direction"]) == 3
            assert brightness["direction_residual_max_rad"] < 1e-12


def test_report_output_file_is_atomic_and_stdout_stays_empty(tmp_path):
    output = tmp_path / "feature-report.json"
    result = _report(_RANDOM, "3-5", "--events", "64", "-o", str(output))
    assert result.returncode == 0, result.stderr
    assert result.stdout == ""
    assert json.loads(output.read_text())["schema"] == "lumice.path-feature-report"
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
