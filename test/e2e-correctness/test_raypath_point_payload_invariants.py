"""Arclength/factor payload invariants of the ``raypath point`` document, end to end.

The fiber-arclength payload audit (doc/raypath/fiber-arclength-payload-audit.md) established,
whitebox plus live evidence, what a downstream factor-curve consumer may assume about the
point document. This file pins the load-bearing subset of that contract at the CLI face, on
the same closed-producing scene the subcommand's own e2e reads
(test/e2e/configs/raypath_analysis_halo_22.json, ``3-5`` at a 22-ring target, deterministic):

  * count contract — every component carries exactly ``len(points) - 1`` arclength increments;
  * increment semantics — increments are per-step radians, each equal (to numerical precision)
    to the SO(3) geodesic angle between its two poses, recomputed here from the poses alone
    (atan2 form, never acos of the trace) — the values are not trusted by re-reading the
    increment field;
  * cumulative axis — increments are non-negative, so a running sum is a monotone arclength;
  * closed closure — the last pose of a closed component lands back on its seed (machine
    precision), the loop's total length being the increment sum, not any fixed angle;
  * factor coherence — ``factor_product`` is present exactly when the pose is valid and the
    Jacobian is available, and ``k_carea``/``singular_values`` exactly when the Jacobian is;
  * the component integral — the declared ``fiber_integral`` equals the left-endpoint sum of
    ``factor_product * increment`` over segments whose first point carries a finite product
    (same skip rule as the producer), and ``available_points`` counts points, not segments.

The C reader's zero-fill counterpart of the null semantics is the whitebox audit's
(src/server/c_api_raypath.cpp BeginRead); the JSON face is the testable projection here.
Unavailable-arm live rows (an actually unavailable Jacobian, an invalid pose, an interrupted
run) do not occur on this scene and are not staged here — the coherence invariants above are
what a consumer's filter must implement regardless.
"""

from __future__ import annotations

import json
import math

import pytest

from test.e2e.runner import find_lumice_binary, get_project_root, run_lumice

_CONFIG = get_project_root() / "test" / "e2e" / "configs" / "raypath_analysis_halo_22.json"
_REQUEST = ["--crystal", "1", "--path", "3-5", "--target", "20,25", "--events", "50k", "--grid", "0"]

# Recomputation tolerances. The producer and this file evaluate the same atan2 geodesic form on
# the same pose pairs, so agreement is at the double round-trip level (audit measured <= 6.2e-17
# rad across the corpus's closed and arc components); the bounds leave room for libm variance.
_INCREMENT_ABS_TOL_RAD = 1e-12
_CLOSURE_ABS_TOL_RAD = 1e-9


def _so3_distance(a: list[float], b: list[float]) -> float:
    """Geodesic angle between two row-major rotations, atan2 of the skew norm and the trace."""
    rel = [sum(a[k * 3 + i] * b[k * 3 + j] for k in range(3)) for i in range(3) for j in range(3)]
    cosine = max(-1.0, min(1.0, (rel[0] + rel[4] + rel[8] - 1.0) / 2.0))
    skew = [(rel[7] - rel[5]) / 2.0, (rel[2] - rel[6]) / 2.0, (rel[3] - rel[1]) / 2.0]
    return math.atan2(math.sqrt(sum(s * s for s in skew)), cosine)


def _raypath(*args: str):
    return run_lumice(["raypath", "point", "-f", str(_CONFIG), *args])


@pytest.fixture(scope="module", autouse=True)
def _binary():
    try:
        find_lumice_binary()
    except FileNotFoundError as e:
        pytest.skip(str(e))


@pytest.fixture(scope="module")
def doc():
    result = _raypath(*_REQUEST)
    assert result.returncode == 0, result.stderr
    document = json.loads(result.stdout)
    assert document["outcome"] == "discovered"
    assert document["meta"]["interrupted"] is False
    assert document["components"], "the 22-ring target must find its fiber"
    return document


def test_a_closed_component_exists(doc):
    assert any(c["kind"] == "closed" for c in doc["components"])


def test_count_contract_and_increment_semantics(doc):
    for component in doc["components"]:
        points = component["points"]
        increments = component["arclength_increments"]
        assert len(increments) == len(points) - 1, component["kind"]
        for i, increment in enumerate(increments):
            recomputed = _so3_distance(points[i]["pose"], points[i + 1]["pose"])
            assert abs(recomputed - increment) <= _INCREMENT_ABS_TOL_RAD, (component["kind"], i)
            assert increment >= 0.0


def test_closed_component_closes_back_on_its_seed(doc):
    for component in doc["components"]:
        if component["kind"] != "closed":
            continue
        assert component["seed_index"] == 0
        points = component["points"]
        closure = _so3_distance(points[0]["pose"], points[-1]["pose"])
        assert closure <= _CLOSURE_ABS_TOL_RAD, closure
        # The loop's length is its increment sum; no canonical angle exists.
        assert sum(component["arclength_increments"]) > 0.0


def test_seed_pose_is_the_component_seed(doc):
    for component in doc["components"]:
        assert component["points"][component["seed_index"]]["pose"] == component["seed"]


def test_factor_nulls_follow_the_flags(doc):
    for component in doc["components"]:
        for point in component["points"]:
            available = point["jacobian"]["available"]
            usable = point["valid"] and available
            assert point["jacobian"]["available"] in (True, False)
            assert (point["k_carea"] is not None) == available
            assert (point["factor_product"] is not None) == usable
            if not available:
                assert point["jacobian"]["value"] is None
                assert point["jacobian"]["singular_values"] == [None, None]


def test_declared_integral_matches_the_left_endpoint_rule(doc):
    for component in doc["components"]:
        points = component["points"]
        increments = component["arclength_increments"]
        integral = 0.0
        used = 0
        for i in range(len(increments)):
            product = points[i]["factor_product"]
            if product is None:
                continue
            integral += product * increments[i]
            used += 1
        factors = component["factors"]
        completeness = factors["completeness"]
        assert completeness["total_points"] == len(points)
        assert completeness["available_points"] == sum(
            1 for p in points if p["factor_product"] is not None
        )
        if used > 0:
            assert factors["fiber_integral"] is not None
            assert math.isclose(factors["fiber_integral"], integral, rel_tol=1e-12, abs_tol=0.0)
        else:
            assert factors["fiber_integral"] is None
