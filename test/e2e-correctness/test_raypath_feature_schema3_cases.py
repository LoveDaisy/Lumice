"""Schema3 acceptance wiring (the C-cells and the dissolved (iii) red states), real CLI.

The corpus (`doc/cases/raypath-analysis-cases.md`, C01-C16) and the redesign conclusions fix the
anchors; this layer pins them against the schema_version-3 report face
(`doc/raypath-cli-output.md` §7): the delta-axis support block, structural-object records and
the derived buckets. The (iii) cells are the schema-2 "seed did not solve" red states: under
schema3 the geometry layer replaces that path structurally, so each cell must complete with
computed objects.

Fixture-comment convention: numbers that ride the flip-time defaults (grid 720 / 33-row
spectrum table) say so; an owner ruling that changes those defaults re-pins in the same change.
"""

from __future__ import annotations

import json
import math
from functools import lru_cache
from pathlib import Path

import pytest

from test.e2e.runner import find_lumice_binary, get_project_root, run_lumice

_ROOT = get_project_root()
_CONFIGS = _ROOT / "test" / "e2e" / "configs"
_RANDOM = _CONFIGS / "raypath_feature_random_regular.json"
_BETA = _CONFIGS / "raypath_feature_beta_prism.json"
_CONE = _CONFIGS / "raypath_feature_cone.json"
_COLUMN = _CONFIGS / "raypath_feature_column.json"
_PLATE_GAUSS = _CONFIGS / "raypath_feature_plate_gauss.json"
_RHOMBIC = _CONFIGS / "raypath_feature_rhombic_plate.json"
_RHOMBIC_GAUSS = _CONFIGS / "raypath_feature_rhombic_plate_gauss.json"

# Corpus anchors (doc/cases/raypath-analysis-cases.md C05/C06; unit layer: test_support_block.cpp).
_C05_UPPER_DEG = 50.161740000307894
_HOLE_EDGE_AT = {400.0: 150.495979, 550.0: 149.246752, 700.0: 148.645350}
_HOLE_EDGE_DISPERSION_400_700_DEG = 1.850629  # the corpus's +1.8506 (+1.851 LI numeric anchor)


@pytest.fixture(scope="module", autouse=True)
def _binary():
    try:
        find_lumice_binary()
    except FileNotFoundError as error:
        pytest.skip(str(error))


@lru_cache(maxsize=None)
def _document(config: Path, path: str, args: tuple[str, ...]) -> dict:
    result = run_lumice(
        ["raypath", "-f", str(config), "--crystal", "1", "--path", path, "--report", *args]
    )
    assert result.returncode == 0, result.stderr
    doc = json.loads(result.stdout)
    assert doc["schema"] == "lumice.path-feature-report"
    assert doc["schema_version"] == 3
    return doc


def _report(config: Path, path: str, *args: str) -> dict:
    return _document(config, path, tuple(args))


def _objects(doc: dict) -> list[dict]:
    buckets = doc["features"]
    return buckets["actual"] + buckets["candidate"] + buckets["unfinished"]


# ---- the dissolved (iii) red states (AC2) ------------------------------------------------------


@pytest.mark.parametrize(
    "config, path",
    [
        (_RANDOM, "3-1-5"),
        (_RANDOM, "3-1-6"),
        (_RANDOM, "3-1-4-5"),
        (_RANDOM, "3-4-1-5"),
        (_RANDOM, "3-5-6-7"),
        (_BETA, "4-8-7-5"),
    ],
)
def test_dissolved_seed_failure_cells_complete_with_computed_objects(config, path):
    # Under schema2 each of these chains reported actual-discovery failure ("seed did not
    # solve the local field equations"). The schema3 geometry layer replaces that path
    # structurally: the cell completes, and every enumerated object is computed (the probe
    # round found exactly this shape; a named escape here would be a conscious re-pin, not a
    # tolerated drift).
    doc = _report(config, path, "--wavelength", "550", "--events", "8192")
    assert doc["outcome"] == "completed"
    assert not doc["budgets"]["exhausted"]
    assert doc["mc_evidence"]["unfinished"] == []
    objects = _objects(doc)
    assert objects
    for record in objects:
        assert record["existence"]["state"] == "computed", record["kind"]
        assert "escape_regime_slug" not in record["existence"]


# ---- C05/C06: the beta crystal's support complement and the hole edge (AC1) --------------------


def test_beta_prism_support_pins_the_c05c06_complement():
    c05 = _report(_BETA, "4-8-7-5", "--wavelength", "550", "--events", "8192")
    c06 = _report(_BETA, "4-8-1-7-5", "--wavelength", "550", "--events", "8192")

    cut = math.radians(120.0)
    upper = math.radians(_C05_UPPER_DEG)
    for member in c05["support"]["members"]:
        partition = member["partition"]
        assert partition["coverage"] == "complete"
        assert partition["walk_status"] == "ok" and partition["walk_closed"] is True
        intervals = partition["intervals"]
        assert len(intervals) == 2
        assert intervals[0]["lower_rad"] == pytest.approx(0.0, abs=1e-12)
        # The unit layer pins this endpoint to 1e-9 deg; the CLI value sits ~1e-6 deg off
        # it (the walk's own merge constant, a platform-independent convergence scale, not
        # a geometry difference — every real break here moves the endpoint by degrees).
        assert intervals[0]["upper_rad"] == pytest.approx(upper, abs=1e-6)
        assert intervals[1]["upper_rad"] == pytest.approx(cut, abs=1e-8)

    # The complement as interval predicates: C06's lower edge IS the same 120-degree cut, and
    # both rows re-derive it independently (no bit-coincidence between two partition runs —
    # each endpoint agrees with the reference cut within the walk's own convergence).
    for member in c06["support"]["members"]:
        intervals = member["partition"]["intervals"]
        assert len(intervals) == 2
        assert intervals[0]["lower_rad"] == pytest.approx(cut, abs=1e-8)
        assert intervals[1]["upper_rad"] == pytest.approx(math.pi, abs=1e-8)

    # Every partition endpoint of both rows carries its onset (the C05/C06 measured shape).
    for doc in (c05, c06):
        for member in doc["support"]["members"]:
            onsets = member["endpoint_onsets"]
            assert onsets
            for interval in member["partition"]["intervals"]:
                for endpoint in (interval["lower_rad"], interval["upper_rad"]):
                    assert any(abs(o["value_rad"] - endpoint) <= 1e-9 for o in onsets)


def test_beta_prism_hole_edge_is_the_constant_closed_curve_with_its_dispersion():
    # The C06 hole edge = the basal-TIR kink's constant-D_P closed curve. Single-wavelength
    # runs carry that wavelength's critical value; the corpus's dispersion anchor is the
    # 400-700 shift. Closed-form provenance: the corpus's 149.247@550 / 150.496@400 /
    # 148.645@700 (unit layer reproduces them from the engine's own dispersion).
    curves_by_lambda = {}
    for nm in (400, 550, 700):
        doc = _report(_BETA, "4-8-1-7-5", "--wavelength", str(nm), "--events", "8192")
        for member in doc["support"]["members"]:
            curves = member["constant_curves"]
            assert len(curves) == 1, member["member"]
            curve = curves[0]
            assert curve["weight_step"] == 2
            assert curve["wavelengths_nm"] == [float(nm)]
            curves_by_lambda.setdefault(nm, set()).add(round(curve["d_p_rad"], 12))

    # The circle is constant across the family: every member reads the same value per lambda.
    for nm, values in curves_by_lambda.items():
        assert len(values) == 1, (nm, values)
        d_p_deg = math.degrees(values.pop())
        assert d_p_deg == pytest.approx(_HOLE_EDGE_AT[float(nm)], abs=1e-3)

    dispersion = _HOLE_EDGE_AT[400.0] - _HOLE_EDGE_AT[700.0]
    assert _HOLE_EDGE_DISPERSION_400_700_DEG == pytest.approx(dispersion, abs=2e-3)


# ---- C09: the cone crystal's finite-crystal gate (AC1) ------------------------------------------


def test_cone_chain_support_covers_the_corpus_band_and_kind1_is_lit():
    doc = _report(_CONE, "13-15-26-28", "--wavelength", "550", "--events", "8192")
    assert {tuple(m) for m in doc["scope"]["physical_members"]} == {
        (13, 15, 26, 28),
        (13, 17, 26, 24),
    }
    families = doc["support"]["families"]
    assert len(families) == 1 and families[0]["shared"]
    first = families[0]["intervals"][0]
    # The corpus's infinite-crystal contour band (delta ~98-120 deg) sits inside the
    # supported interval.
    assert first["lower_rad"] < math.radians(98.0)
    assert first["upper_rad"] > math.radians(120.0)

    kind1 = [f for f in _objects(doc) if f["kind"] == "kind_1"]
    assert len(kind1) == 2
    for record in kind1:
        assert record["existence"]["state"] == "computed"
        # Production face for this chain under a wobbling declared axis: the restricted
        # orbit lights on part of the support (deterministic, sampled_exhaustive). The
        # corpus's unlit verdict for the finite fixed-pose crystal stays anchored at the
        # unit layer (the fully-dark pinned member); the report side does not reproduce it
        # for this declared distribution.
        assert record["visibility"]["state"] == "partial"
        assert record["visibility"]["evidence"] == "sampled_exhaustive"


# ---- C11: the plate family's restricted object and the azimuth-difference support (AC1) ---------


def test_plate_family_restricted_object_pins_the_azimuth_difference_support():
    doc = _report(_PLATE_GAUSS, "1-4-5-2", "--events", "8192")
    families = doc["support"]["families"]
    assert len(families) == 1 and families[0]["shared"]
    assert {tuple(m) for m in families[0]["members"]} == {(1, 4, 5, 2), (1, 8, 7, 2)}
    # 120 deg is the AZIMUTH difference to the sun, not the spherical deflection (the
    # corpus's pin: the spot's own deflection is 108.937 deg, anchored at the contour
    # docking unit test through the same producer).
    assert families[0]["intervals"][0]["upper_rad"] == pytest.approx(math.radians(120.0), abs=1e-8)

    restricted = [f for f in _objects(doc) if f["kind"] == "kind_1_restricted"]
    assert len(restricted) == 2
    for record in restricted:
        assert record["existence"]["state"] == "computed"
        assert record["visibility"]["state"] == "partial"
        assert record["visibility"]["evidence"] == "sampled_exhaustive"
        # The corpus's effective arc: 60 deg of the 360-deg family orbit = 1/6 lit.
        assert record["visibility"]["lit_fraction"] == pytest.approx(1.0 / 6.0, abs=1e-3)
        assert record["corroboration"]["state"] == "observed"
        # The u pre-image is first-class: the restricted orbit stream rides the object
        # (one point per grid node; grid 720 is the flip-time default).
        assert len(record["geometry"]["u"]) > 0
        assert len(record["geometry"]["u"]) % 3 == 0


# ---- C12: the rhombic plate's chromatic face and the density-skip boundary (AC1) ----------------


def test_rhombic_plate_density_skip_is_declared_and_the_gauss_axis_converts():
    # Fixed-zenith plate: the pose-density conversion refuses the axis form, and the
    # refusal is a DECLARED coverage row (not a silent degrade).
    fixed = _report(_RHOMBIC, "1-3-4-2", "--wavelength", "550", "--events", "8192")
    skips = [r for r in fixed["coverage"] if r["subject"] == "pose density conversion"]
    assert len(skips) == 1 and skips[0]["status"] == "not_supported"

    # The gauss-zenith form converts (no skip row) and the chromatic leg assesses the
    # kind-1 objects under both axis forms.
    gauss = _report(_RHOMBIC_GAUSS, "1-3-4-2", "--wavelength", "550", "--events", "8192")
    assert not [r for r in gauss["coverage"] if r["subject"] == "pose density conversion"]
    for doc in (fixed, gauss):
        assessed = [f for f in _objects(doc) if f["chromatic"]["assessed"]]
        assert assessed
        for record in assessed:
            thresholds = record["chromatic"]["thresholds"]
            assert thresholds["n_red"] == pytest.approx(1.307)
            assert thresholds["n_blue"] == pytest.approx(1.317)
    # Production-face boundary (recorded in the corpus): the sky-tint block (the corpus's
    # 1.010/1.525 wire values) does not surface on these objects — the tint numbers stay
    # anchored at the fiber-quadrature / contour-docking unit layer until the producer
    # wires them through.
    for doc in (fixed, gauss):
        for record in _objects(doc):
            assert "tint" not in record["chromatic"]["verdict"]


# ---- C16: the column family keeps the lit kind-1 and the junctions (AC1) ------------------------


def test_column_family_carries_lit_kind1_with_junctions():
    doc = _report(_COLUMN, "3-5", "--wavelength", "550", "--events", "8192")
    assert {tuple(m) for m in doc["scope"]["physical_members"]} == {(3, 5), (3, 7)}
    kind1 = [f for f in _objects(doc) if f["kind"] == "kind_1"]
    assert len(kind1) == 2
    for record in kind1:
        assert record["visibility"]["state"] == "partial"
        assert record["visibility"]["evidence"] == "sampled_exhaustive"
    assert any(f["kind"] == "s6_junction" for f in _objects(doc))


# ---- C02: the TIR kink's kind-3 objects and the counterfactual's current face (AC1) -------------


def test_tir_kink_objects_and_the_paired_counterfactual_at_the_kink():
    doc = _report(_RANDOM, "3-1-5", "--wavelength", "550", "--events", "8192")
    kinks = [f for f in _objects(doc) if f["kind"] == "kind_3"]
    assert kinks
    for record in kinks:
        assert record["slot"] == 1
        assert len(record["geometry"]["u"]) > 0
        # Current production face: the structural counterfactual is not wired for these
        # objects; the mc layer's paired_interface below carries the removable-slot
        # comparison instead.
        assert record["diagnostics"]["counterfactual"]["available"] is False

    paired = [r for r in doc["mc_evidence"]["records"] if "paired_interface" in r]
    assert paired
    for record in paired:
        block = record["paired_interface"]
        assert block["complete"] is True
        assert block["slot"] == 1
        # At the TIR kink the slot's Fresnel reflectance is already ~1 (discriminant ~ 0),
        # so removing it is a numerical no-op — the current face the probe round measured.
        # A wired-up counterfactual that moves these numbers is a deliberate semantics
        # change and re-pins this.
        assert block["xy_difference"] == pytest.approx([0.0, 0.0], abs=1e-6)
