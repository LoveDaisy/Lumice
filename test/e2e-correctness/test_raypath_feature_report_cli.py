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
_FINITE_SUN_FOLD = _ROOT / "test" / "e2e" / "configs" / "raypath_feature_finite_sun_fold.json"
_REFERENCE_WAVELENGTHS = (694.3628981235904, 430.0197374077313)


@pytest.fixture(scope="module")
def reference_configs(tmp_path_factory):
    """Author the independent reference spectrum explicitly instead of assuming a default."""
    directory = tmp_path_factory.mktemp("feature-reference-spectrum")
    paths = {}
    for name, source in (("random", _RANDOM), ("plate", _PLATE)):
        config = json.loads(source.read_text())
        config["scene"]["light_source"]["spectrum"] = [
            {
  "wavelength" : wavelength, "weight" : 1}
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


def test_random_315_report_is_a_separate_document_with_general_mechanism_records(reference_configs):
    result = _report(reference_configs["random"], "3-1-5", "--events", "64", "--wavelength", "550")
    assert result.returncode == 0, result.stderr
    doc = json.loads(result.stdout)
    assert doc["schema"] == "lumice.path-feature-report"
    assert doc["schema_version"] == 3
    assert doc["scene_measure"]["spectrum_nodes"][0]["source"] == "diagnostic"
    assert "target" not in doc["meta"]
    discovery = doc["feature_discovery"]
    assert discovery["visited_row_count"] == doc["scene_measure"]["evaluated_row_count"]
    assert discovery["complete_visit"] is True
    mechanisms = {record["mechanism"]: record for record in discovery["mechanisms"]}
    assert set(mechanisms) == {
        "interior_rank_loss", "support_boundary", "support_corner", "optical_kink",
        "filter_boundary", "weight_kink", "measure_atom", "strict_confinement",
        "finite_width_concentration", "brightness_maximum", "brightness_ridge",
    }
    assert mechanisms["finite_width_concentration"]["status"] == "numerical_incomplete"
    assert mechanisms["finite_width_concentration"]["candidate_count"] == 0
    assert mechanisms["brightness_maximum"]["status"] == "candidate"
    assert mechanisms["brightness_maximum"]["candidate_count"] > 0
    assert all(
        candidate["scope"]["origin"] == "full_scene"
        for candidate in discovery["candidates"]
        if candidate["mechanism"] in {"brightness_maximum", "brightness_ridge"}
    )
    coverage = discovery["continuous_coverage"]
    assert coverage
    assert all(item["callback_query_count"] <= item["callback_budget"] for item in coverage)
    assert all(
        {"coordinate", "role", "layer_index", "lower", "upper", "grid_resolution"} <= set(parameter)
        for item in coverage
        for parameter in item["parameters"]
    )
    assert all(feature["id"].startswith("general.") for feature in doc["features"])
    assert all(feature["location"] == "computed sky position" for feature in doc["features"])
    assert not any(feature["id"].startswith("random_regular.") for feature in doc["features"])
    assert "[raypath report]" in result.stderr


def test_nonreference_shape_and_unnamed_path_still_run_general_discovery(tmp_path):
    config = json.loads(_RANDOM.read_text())
    config["crystal"][0]["shape"] = {
        "height": 0.73,
        "face_distance": [1.37, 0.91, 1.12, 1.46, 0.83, 1.05],
    }
    path = tmp_path / "asymmetric.json"
    path.write_text(json.dumps(config))
    result = _report(path, "3-6", "--events", "64", "--wavelength", "550")
    assert result.returncode == 0, result.stderr
    doc = json.loads(result.stdout)
    assert doc["meta"]["requested_faces"] == [3, 6]
    assert doc["meta"]["crystal"]["shape_is_nominal"] is False
    assert doc["feature_discovery"]["visited_row_count"] > 0
    assert len(doc["feature_discovery"]["mechanisms"]) == 11
    assert all(feature["id"].startswith("general.") for feature in doc["features"])


def test_max_hits_one_external_reflection_is_reported_by_the_real_cli(tmp_path):
    config = json.loads(_RANDOM.read_text())
    config["scene"]["max_hits"] = 1
    config["scene"]["light_source"]["altitude"] = 90
    config["crystal"][0]["axis"] = {
        name: {"type": "gauss", "mean": 0, "std": 0}
        for name in ("zenith", "azimuth", "roll")
    }
    path = tmp_path / "one-face.json"
    path.write_text(json.dumps(config))

    result = _report(path, "1", "--events", "64", "--wavelength", "550")
    assert result.returncode == 0, result.stderr
    doc = json.loads(result.stdout)
    assert doc["meta"]["requested_faces"] == [1]
    assert doc["scene_measure"]["status"] == "confirmed"
    assert doc["scene_measure"]["member_chains"] == [[[1]]]
    assert doc["scene_measure"]["total_contribution"] > 0
    layer = doc["scene_measure"]["sampled_rows"][0]["layers"][0]
    assert layer["field"]["interfaces"][0]["kind"] == "external_reflection"
    assert layer["outgoing_direction"] == pytest.approx([0, 0, 1], abs=1e-7)
    assert doc["feature_discovery"]["visited_row_count"] == doc["scene_measure"]["evaluated_row_count"]
    assert len(doc["feature_discovery"]["mechanisms"]) == 11


def test_multiple_internal_reflections_keep_nonfirst_interface_provenance():
    result = _report(_RANDOM, "3-1-5-7", "--events", "64", "--wavelength", "550")
    assert result.returncode == 0, result.stderr
    candidates = json.loads(result.stdout)["feature_discovery"]["candidates"]
    optical_kinks = [
        candidate for candidate in candidates if candidate["mechanism"] == "optical_kink"
    ]
    assert optical_kinks
    assert any(
        candidate["provenance"]["layer_index"] == 0
        and candidate["provenance"]["interface_index"] == 2
        for candidate in optical_kinks
    )


def test_finite_solar_source_reports_conditional_pose_fold_without_joint_rank_loss():
    result = _report(_FINITE_SUN_FOLD, "3-5", "--events", "64", "--wavelength", "550")
    assert result.returncode == 0, result.stderr
    doc = json.loads(result.stdout)
    assert doc["scene_measure"]["evaluated_row_count"] == 512
    assert doc["scene_measure"]["total_contribution"] == pytest.approx(0.00730355653482653)
    folds = [
        candidate
        for candidate in doc["feature_discovery"]["candidates"]
        if candidate["mechanism"] == "interior_rank_loss"
    ]
    conditional = [
        candidate
        for candidate in folds
        if candidate["status"] == "confirmed" and candidate["scope"]["kind"] == "conditional"
    ]
    assert conditional
    assert all(
        parameter["role"] == "pose"
        for candidate in conditional
        for parameter in candidate["scope"]["active_parameters"]
    )
    assert {candidate["scope"]["fixed_spectrum_node_id"] for candidate in conditional} == {0}
    assert all(0 <= candidate["scope"]["fixed_source_node_id"] < 8 for candidate in conditional)
    assert all(candidate["scope"]["evidence_id"] > 0 for candidate in conditional)
    assert all(candidate["weighted_mass"] == 0 for candidate in conditional)
    assert not any(
        candidate["status"] == "confirmed" and candidate["scope"]["kind"] == "joint"
        for candidate in folds
    )


def test_report_states_the_feature_families_it_does_not_enumerate():
    result = _report(_RANDOM, "3-1-5", "--events", "64", "--wavelength", "550")
    assert result.returncode == 0, result.stderr
    limitations = json.loads(result.stdout)["limitations"]
    assert any("local numerical evidence" in limitation for limitation in limitations)
    assert any("color causality" in limitation for limitation in limitations)
    assert any("callback refinement" in limitation for limitation in limitations)
    assert not any("not an all-sky feature enumerator" in limitation for limitation in limitations)


def test_rhombic_plate_keeps_plus_and_minus_120_as_general_strict_confinement(reference_configs):
    result = _report(reference_configs["plate"], "1-3-4-2", "--events", "64", "--wavelength", "550")
    assert result.returncode == 0, result.stderr
    doc = json.loads(result.stdout)
    assert doc["physical_l2_members"] == []
    assert {tuple(chain[0]) for chain in doc["scene_measure"]["member_chains"]} == {
        (1, 3, 4, 2),
        (1, 3, 8, 2),
    }
    scoped_strict = [
        candidate
        for candidate in doc["feature_discovery"]["candidates"]
        if candidate["mechanism"] == "strict_confinement"
    ]
    evidence_ids = {candidate["scope"]["evidence_id"] for candidate in scoped_strict}
    assert len(evidence_ids) > 2
    assert len(evidence_ids) < len(scoped_strict)
    assert 0 not in evidence_ids
    assert all(
        candidate["weighted_mass"] == 0
        for candidate in scoped_strict
        if candidate["scope"]["kind"] == "conditional"
    )
    assert all(
        {candidate["scope"]["kind"] for candidate in scoped_strict if candidate["scope"]["evidence_id"] == evidence_id}
        == {
    "joint", "conditional"}
        for evidence_id in evidence_ids
    )
    strict = [feature for feature in doc["features"] if feature["mechanism"] == "strict_confinement"]
    assert len(strict) == len(scoped_strict)
    positions = [feature["positions"][0] for feature in strict]
    relative_azimuths = sorted({round(position["relative_solar_azimuth_deg"], 5) for position in positions})
    assert relative_azimuths == pytest.approx([-120, 120], abs=1e-5)
    for position in positions:
        assert position["spherical_separation_deg"] == pytest.approx(117.599764, abs=1e-5)


def test_rhombic_plate_1352_runs_the_same_mechanism_search_at_each_wavelength():
    for wavelength in _REFERENCE_WAVELENGTHS:
        result = _report(_PLATE, "1-3-5-2", "--events", "64", "--wavelength", repr(wavelength))
        assert result.returncode == 0, result.stderr
        doc = json.loads(result.stdout)
        assert doc["meta"]["sun"]["altitude_deg"] == pytest.approx(9.0)
        assert doc["meta"]["orientation_measure"] == "actual configured scene measure; see scene_measure.factors"
        assert {tuple(chain[0]) for chain in doc["scene_measure"]["member_chains"]
  } == {
            (1, 3, 5, 2), (1, 3, 7, 2),
        }
        assert len(doc["scene_measure"]["spectrum_nodes"]) == 1
        assert doc["scene_measure"]["spectrum_nodes"][0]["wavelength_nm"] == wavelength
        mechanisms = {record["mechanism"]: record for record in doc["feature_discovery"]["mechanisms"]}
        assert mechanisms["strict_confinement"]["status"] == "confirmed"


def test_report_default_detector_and_measure_use_the_same_scene_spectrum():
    result = _report(_RANDOM, "3-5", "--events", "64")
    assert result.returncode == 0, result.stderr
    document = json.loads(result.stdout)
    nodes = document["scene_measure"]["spectrum_nodes"]
    expected_wavelengths = [405.0 + 50.0 * index for index in range(8)]
    assert [node["wavelength_nm"] for node in nodes] == expected_wavelengths
    assert all(node["source"] == "scene_illuminant_uniform_380_780" for node in nodes)
    assert all(node["weight"] > 0 for node in nodes)
    assert [node["nm"] for node in document["wavelengths"]] == expected_wavelengths
    assert document["feature_discovery"]["visited_row_count"] == document["scene_measure"]["evaluated_row_count"]


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
