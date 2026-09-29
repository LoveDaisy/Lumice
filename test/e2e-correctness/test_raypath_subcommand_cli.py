"""The `raypath` subcommand, end to end through the static CLI (`Lumice raypath`).

The scene is ``test/e2e/configs/raypath_analysis_halo_22.json`` (a prism of height 1.2, sun at
altitude 20°, D65), the scene `analyze`'s e2e reads. The analysis is deterministic, so every run
below is exact, not statistical; the search is kept small (50k seed events, a 12-row grid), which
finds the one closed component the 22° halo has at the target used here.

What is pinned — the subcommand's own contract; the analysis's physics is the unit layer's
(test/unit-correctness/raypath/):
  * the option surface — `-h`; each missing or malformed request option exits 1 naming it; the
    three options this subcommand does not take are unknown options, not accepted and ignored;
  * the output contract — stdout is exactly one JSON document and nothing else, with ``-o`` it is
    empty and the file holds the document; progress is stderr's;
  * the document's two outcomes as the CLI delivers them — ``3-5`` is discovered with a closed
    component and a rows × 2 rows grid, rank-0 ``1-2`` is a point mass;
  * refusals the analysis owns reach the user with their reason — a multi-layer chain is
    ``multi_layer_unsupported``;
  * ``--warm`` — every seed of an earlier output comes back as a component seed;
  * ``reach`` tells an unreachable target from a reachable one — ``3-5`` on its halo ring is in
    range, ``3-6-4-8`` at 124° from the sun (beyond its 120° maximum deviation) is not, with no
    component either way for the latter. Which empty result is which is the unit layer's to pin
    with a finite crystal that blocks a reachable fiber; here only the deterministic flag is read.
The field-level shape of the document is the unit layer's (test_single_path_json.cpp) and is
not re-asserted here beyond what the CLI adds.
"""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from test.e2e.runner import find_lumice_binary, get_project_root, run_lumice

_CONFIG = get_project_root() / "test" / "e2e" / "configs" / "raypath_analysis_halo_22.json"
_GRID_ROWS = 12
# A point on the 22° halo's ring near the sun's altitude (sun at 20,0).
_REQUEST = ["--crystal", "1", "--path", "3-5", "--target", "20,25", "--events", "50k", "--grid", str(_GRID_ROWS)]


def _raypath(*args: str):
    return run_lumice(["raypath", "-f", str(_CONFIG), *args])


@pytest.fixture(scope="module", autouse=True)
def _binary():
    try:
        find_lumice_binary()
    except FileNotFoundError as e:
        pytest.skip(str(e))


@pytest.fixture(scope="module")
def halo():
    """One `3-5` run to stdout, shared by the cases that read it."""
    result = _raypath(*_REQUEST)
    assert result.returncode == 0, result.stderr
    return result


def test_help_names_the_request_options():
    result = run_lumice(["raypath", "-h"])
    assert result.returncode == 0
    for option in ("--crystal", "--path", "--target", "--wavelength", "--events", "--grid", "--warm", "-o"):
        assert option in result.stdout, option
    # The top-level page lists the subcommand.
    assert "raypath" in run_lumice(["-h"]).stdout


@pytest.mark.parametrize(
    "args, named",
    [
        (["--path", "3-5", "--target", "20,25"], "--crystal"),
        (["--crystal", "1", "--target", "20,25"], "--path"),
        (["--crystal", "1", "--path", "3-5"], "--target"),
        (["--crystal", "1", "--path", "3-5", "--target", "20"], "--target"),
        (["--crystal", "1", "--path", "3-x", "--target", "20,25"], "--path"),
        (["--crystal", "1", "--path", "3-5", "--target", "20,25", "--events", "200M"], "--events"),
        (["--crystal", "1", "--path", "3-5", "--target", "20,25", "--grid", "721"], "--grid"),
        (["--crystal", "1", "--path", "C2(3-5)", "--target", "20,25"], "C2"),
        (["--crystal", "1", "--path", "3-5", "--target", "20,25", "--wavelength", "1000"], "wavelength"),
        (["--crystal", "1", "--path", "3-5", "--target", "20,25", "--backend", "cpu"], "--backend"),
        (["--crystal", "1", "--path", "3-5", "--target", "20,25", "--workers", "2"], "--workers"),
        (["--crystal", "1", "--path", "3-5", "--target", "20,25", "--seed", "7"], "--seed"),
    ],
)
def test_a_bad_request_exits_1_naming_the_option(args, named):
    result = _raypath(*args)
    assert result.returncode == 1, result.stderr
    assert named in result.stderr, result.stderr
    assert result.stdout.strip() == "" or "Usage:" in result.stdout, result.stdout[:200]
    assert "{" not in result.stdout, "no JSON on a refused request"


def test_missing_config_is_refused():
    result = run_lumice(["raypath", *_REQUEST])
    assert result.returncode == 1
    assert "-f" in result.stderr


def test_halo_path_is_discovered_with_a_closed_component_and_the_grid(halo):
    doc = json.loads(halo.stdout)  # stdout is the document and nothing else
    assert doc["schema_version"] == 1
    assert doc["outcome"] == "discovered"
    assert doc["meta"]["faces"] == [3, 5]
    assert doc["meta"]["crystal"]["id"] == 1
    # D65 names no single wavelength, so the default is taken — and recorded as such.
    assert doc["meta"]["wavelength"] == {**doc["meta"]["wavelength"], "nm": 550.0, "source": "default"}
    assert any(c["kind"] == "closed" for c in doc["components"])
    grid = doc["sun_grid"]
    assert (grid["lat_count"], grid["lon_count"]) == (_GRID_ROWS, 2 * _GRID_ROWS)
    for key in ("deviation_rad", "valid", "entry_measure"):
        assert len(grid[key]) == _GRID_ROWS * 2 * _GRID_ROWS, key
    assert any(grid["valid"]), "a 3-5 path is valid somewhere on the sun sphere"
    assert doc["reach"]["target_in_range"] is True
    assert "[raypath]" in halo.stderr


def test_a_target_beyond_the_paths_deviation_range_is_out_of_reach():
    # (20, 140) sits 124.02° from the sun; 3-6-4-8 deviates light by at most 120°.
    result = _raypath("--crystal", "1", "--path", "3-6-4-8", "--target", "20,140", "--events", "1k", "--grid", "0")
    assert result.returncode == 0, result.stderr
    doc = json.loads(result.stdout)
    assert doc["outcome"] == "discovered"
    assert doc["components"] == []
    reach = doc["reach"]
    assert reach["target_in_range"] is False
    assert reach["target_deviation_rad"] > reach["deviation_max_rad"] + reach["tolerance_rad"]


def test_rank_zero_path_is_a_point_mass():
    result = _raypath("--crystal", "1", "--path", "1-2", "--target", "20,25", "--grid", "0")
    assert result.returncode == 0, result.stderr
    doc = json.loads(result.stdout)
    assert doc["outcome"] == "point_mass"
    assert "components" not in doc and "sun_grid" not in doc and "reach" not in doc
    assert len(doc["point_mass"]["direction"]) == 3


def test_multi_layer_chain_is_refused_with_its_reason():
    result = _raypath("--crystal", "1", "--path", "(3-5) -> (1-3)", "--target", "20,25")
    assert result.returncode == 1
    assert "multi_layer_unsupported" in result.stderr, result.stderr
    assert result.stdout == ""


def test_output_file_takes_the_document_and_stdout_stays_empty(tmp_path, halo):
    out = tmp_path / "r.json"
    result = _raypath(*_REQUEST, "-o", str(out))
    assert result.returncode == 0, result.stderr
    assert result.stdout == ""
    assert out.read_text() == halo.stdout  # deterministic: the same document either way
    assert not Path(str(out) + ".tmp").exists()


def test_warm_seeds_come_back_as_component_seeds(tmp_path, halo):
    first = json.loads(halo.stdout)
    seeds = [c["seed"] for c in first["components"]] + [c["seed"] for c in first["incomplete"]]
    assert seeds
    warm = tmp_path / "first.json"
    warm.write_text(halo.stdout)
    result = _raypath(*_REQUEST, "--warm", str(warm))
    assert result.returncode == 0, result.stderr
    second = json.loads(result.stdout)
    assert second["meta"]["discovery_settings"]["warm_seed_count"] == len(seeds)
    found = [c["seed"] for c in second["components"]]
    for seed in seeds:
        assert any(max(abs(a - b) for a, b in zip(seed, s)) < 1e-9 for s in found), seed
