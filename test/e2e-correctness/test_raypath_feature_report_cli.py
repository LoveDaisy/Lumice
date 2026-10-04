"""Target-free ``Lumice raypath --report`` through the real static CLI.

The schema-two route consumes physical input and reports bounded numerical evidence.
Scientific kernels have independent fixtures in the analytic and composition layers;
this layer owns the CLI, spectrum choice, scope, budget and output-file contracts.
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


def test_bounded_nonfixed_report_completes_without_claiming_global_coverage():
    result = _report(_RANDOM, "3-5", "--events", "64", "--wavelength", "550")
    assert result.returncode == 0, result.stderr
    doc = json.loads(result.stdout)
    assert doc["outcome"] == "completed"
    assert doc["unfinished_reasons"] == []
    assert not doc["budgets"]["exhausted"]
    assert doc["coverage"][1]["limitations"]
    assert doc["candidates"]
    # A completed bounded search is not a proof that every local hypothesis worked.
    assert doc["unfinished"]


def test_315_report_keeps_conditional_tir_distinct_from_observed_colour():
    result = _report(_RANDOM, "3-1-5", "--events", "8192", "--wavelength", "550")
    assert result.returncode == 0, result.stderr
    doc = json.loads(result.stdout)
    assert doc["schema"] == "lumice.path-feature-report"
    assert doc["schema_version"] == 2
    assert "target" not in doc
    assert len(doc["physical_members"]) == 24
    candidates = [f for f in (doc["actual_features"] + doc["candidates"] + doc["unfinished"]) if f["kind"] == "conditional_internal_tir"]
    assert candidates
    for feature in candidates:
        assert feature["evidence"] == "candidate"
        slot = feature["internal_slot"]
        event = feature["interface_event"]
        assert event["value"]["entry"]["area"] > 0
        assert abs(event["value"]["interfaces"][slot]["discriminant"]) < 1e-10
        assert feature["source_token"] is not None
    assert not any("blue_band" in f["kind"] for f in (doc["actual_features"] + doc["candidates"] + doc["unfinished"]))
    assert "[raypath report]" in result.stderr


def test_report_physical_radius_is_not_the_smoothed_brightness_peak():
    import math

    result = _report(_RANDOM, "3-5", "--events", "8192", "--wavelength", "550")
    assert result.returncode == 0, result.stderr
    doc = json.loads(result.stdout)
    assert doc["scope"]["layers"][0]["representative_faces"] == [3, 5]
    edges = [f for f in (doc["actual_features"] + doc["candidates"] + doc["unfinished"]) if "physical_position" in f]
    assert edges
    for feature in edges:
        position = feature["physical_position"]
        n = position["source"]["refractive_index"]
        assert position["deviation_rad"] == pytest.approx(2 * math.asin(n / 2) - math.pi / 3, abs=1e-10)
        assert position["value"]["entry"]["area"] > 0
        assert feature["geometry"]["sky_points"]
    assert "not the scene SPD" in doc["scope"]["spectrum"]


def test_default_report_consumes_actual_continuous_spectrum_and_records_scope():
    result = _report(_RANDOM, "3-5")
    assert result.returncode == 0, result.stderr
    doc = json.loads(result.stdout)
    assert len(doc["spectrum"]) == 33
    assert "continuous" in doc["scope"]["spectrum"]
    assert sum(row["measure_mass"] for row in doc["spectrum"]) == pytest.approx(1)
    if doc["spectral_verification"]["optical_evaluations"] == 0:
        # A bounded call may exhaust its deadline before spectral refinement.
        # Zero work is valid only with the explicit incomplete outcome/reason.
        assert doc["budgets"]["exhausted"], doc["budgets"]
        assert doc["outcome"] == "partial"
        assert "continuous spectral quadrature refinement incomplete" in doc["unfinished_reasons"]
    assert doc["observation"]["kernel"] == "normalized_vMF"
    assert doc["outcome"] in {"completed", "partial"}
    assert doc["coverage"][1]["limitations"]  # bounded search is not an absence certificate


def test_rhombic_plate_uses_physical_members_and_measured_peaks():
    import math

    result = _report(_PLATE, "1-3-4-2", "--events", "8192", "--wavelength", "550")
    assert result.returncode == 0, result.stderr
    doc = json.loads(result.stdout)
    assert {tuple(member) for member in doc["physical_members"]} == {(1, 3, 4, 2), (1, 3, 8, 2)}
    peaks = [f for f in (doc["actual_features"] + doc["candidates"] + doc["unfinished"]) if f["kind"] == "intensity_peak" and f["evidence"] == "actual"]
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
    doc = json.loads(result.stdout)
    assert doc["outcome"] == "partial"
    assert doc["budgets"]["exhausted"]
    if budget[0] == "--max-evaluations":
        assert doc["budgets"]["optical_evaluations"] == 100


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


def test_report_multicrystal_is_a_structured_zero_work_refusal():
    result = _report(_RANDOM, "(3-5)->(1-3)")
    assert result.returncode == 0, result.stderr
    doc = json.loads(result.stdout)
    assert doc["outcome"] == "unsupported_multicrystal"
    assert doc["requested_path_layers"] == [[3, 5], [1, 3]]
    assert doc["actual_features"] == []
    assert doc["budgets"]["optical_evaluations"] == 0


def test_endpoint_bisection_budget_is_partial_not_a_physical_stop():
    # This cap interrupts the final source curve inside its optical bracket.
    # Other report stages still finish; their completion cannot hide this stop.
    result = _report(_RANDOM, "3-1-5", "--events", "8192", "--wavelength", "550",
                     "--max-evaluations", "405930")
    assert result.returncode == 0, result.stderr
    doc = json.loads(result.stdout)
    assert doc["outcome"] == "partial"
    assert doc["budgets"]["exhausted"]
    assert doc["budgets"]["optical_evaluations"] <= 405930
    features = doc["actual_features"] + doc["candidates"] + doc["unfinished"]
    curves = [c for f in features for c in f.get("source_curves", [])]
    stopped = [c for c in curves if c["termination"] == 6]
    assert stopped
    assert any(c["points"] for c in stopped)
