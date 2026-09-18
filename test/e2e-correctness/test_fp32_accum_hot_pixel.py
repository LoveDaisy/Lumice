"""The hot pixels of a long legacy-CPU run keep the colour of the light that made them.

The beta report this pins: on a horizontal-plate scene lit by a single 550 nm line, the sub-sun
changed colour with the ray budget — yellow-green at 10M rays, cyan at 30M and beyond — while the
Metal route drew it the same at every budget. A monochromatic scene has exactly one legal colour
per pixel: X:Y:Z must equal the CIE colour-matching functions at that wavelength, whatever the
pixel's brightness, so ``X/Y`` on the brightest pixels is an oracle with no reference image and no
second implementation behind it. The legacy route was drifting off it because ``RenderConsumer``
kept its XYZ plane as a float32 running sum, and a hot pixel's 1e7 per-ray adds round in one
direction once each addend nears the sum's ulp — measured at 30M rays: X/Y 0.3703 against the
0.43565 the CMF gives, and Y itself 10% high; at 100M the sun pixel read 0.3383 (−22%). The
accumulator is now ``double`` (``src/server/render.cpp``), and this file is the full-stack witness
that the number came back: the unit-level oracle
(``test/unit-correctness/server/test_render_consumer_fp32_accum_oracle.cpp``) proves the arithmetic
on a synthetic batch; this one proves it on the user's scene through the C API.

The scene is the report's own config (``fp32_accum_subsun_550.json``: prism plate, zenith std 0,
sun at 20 deg, 0.1 deg disc, 550 nm only, 360 deg equal-area fisheye at 1024x1024 looking at the
sun), with the ray budget written by the test. Two pixels are read: the sun (on axis, (512,512))
and the sub-sun (its reflection below the horizon, (512,687)); both are found as the brightest
pixel of their half of the frame rather than addressed by hand, and their positions are asserted
so a scene edit that moves them is caught rather than silently measured elsewhere.

Measured after the fix (seed 42, legacy, 100M rays): X/Y 0.43565 on both pixels, Z/Y 0.008794 on
both — the CMF ratios to five digits. Y/emitted: sun 0.102902 legacy vs 0.102702 Metal (0.19%
apart), sub-sun 0.0997818 vs 0.099148 (0.64%). The Metal route's own X/Y reads 0.43590 / 0.43642
at 100M — a FIXED few-tenths-of-a-percent offset from the on-device fp32 atomics inside one drain
window, which does not move with the budget and is not this defect; it is why the Metal arm is
compared on Y/emitted at 1% and not held to the CMF bar. Both cases go red on the pre-fix build:
the sun pixel reads X/Y 0.33833 there, and its Y/emitted sits +16.9% above Metal's.

@pytest.mark.slow: needs the shared-lib build (``./scripts/build.sh -sj release``) and the legacy
arm at 100M rays runs ~3.5 min (a fixed seed pins legacy to one worker). The Metal arm is
Darwin-only and skips elsewhere; the CMF assertion on the legacy arm runs on every platform.
"""

from __future__ import annotations

import json
import platform
import shutil
import tempfile
from pathlib import Path

import numpy as np
import pytest

from test.e2e.capi_runner import BufferedSimResult, run_scene_capi_buffered
from test.e2e.runner import get_project_root

BASE_CONFIG = get_project_root() / "test" / "e2e" / "configs" / "fp32_accum_subsun_550.json"

_SEED = 42
_RAY_NUM = 100_000_000
_TIMEOUT = 1800

# CIE 1931 2-degree CMF at 550 nm (util/color_data.hpp: kCmfX/kCmfY/kCmfZ[550-360]).
_CMF_X_OVER_Y = 0.43565
_CMF_Z_OVER_Y = 0.008794
# The report's own acceptance band on X/Y; the fix lands at the fifth digit, the defect at
# the second (0.3703 at 30M), so the band is ~50x clear of both.
_XY_TOL = 0.001
# Z/Y is 50x smaller than X/Y, so its band is scaled to keep the same relative width.
_ZY_TOL = 0.001 * _CMF_Z_OVER_Y / _CMF_X_OVER_Y
# Legacy vs Metal on Y/emitted per hot pixel: measured 0.19% / 0.64%, Metal's fixed offset included.
_METAL_Y_TOL = 0.01

# Where the two hot pixels are in this scene, and how far a found peak may sit from there.
_SUN_PX = (512, 512)
_SUBSUN_PX = (512, 687)
_PX_SLACK = 3
_HORIZON_ROW = 600  # rows above hold the sun, rows below the sub-sun


def _hot_pixels(result: BufferedSimResult) -> dict:
    """The brightest pixel of each half of the frame, as (x, y) and its XYZ triple."""
    y = result.flt_buf[:, :, 1]
    top = np.unravel_index(int(np.argmax(y[:_HORIZON_ROW])), y[:_HORIZON_ROW].shape)
    bottom = np.unravel_index(int(np.argmax(y[_HORIZON_ROW:])), y[_HORIZON_ROW:].shape)
    sun = (int(top[1]), int(top[0]))
    subsun = (int(bottom[1]), int(bottom[0]) + _HORIZON_ROW)
    return {
        "sun": (sun, result.flt_buf[sun[1], sun[0]].astype("float64")),
        "subsun": (subsun, result.flt_buf[subsun[1], subsun[0]].astype("float64")),
    }


def _run(backend: str, tmp_dir: Path) -> BufferedSimResult:
    with open(BASE_CONFIG, encoding="utf-8") as fp:
        doc = json.load(fp)
    doc["scene"]["ray_num"] = _RAY_NUM
    path = tmp_dir / f"{backend}_{_RAY_NUM}.json"
    path.write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8")
    result = run_scene_capi_buffered(str(path), sim_seed=_SEED, backend=backend, timeout_sec=_TIMEOUT)
    assert result.has_valid_data, f"{path.name}: simulation produced no data"
    assert result.routed_backend == backend, f"{path.name}: asked for {backend}, ran {result.routed_backend}"
    assert not result.fell_back, f"{path.name}: backend fell back"
    return result


@pytest.fixture(scope="module")
def legacy_run():
    tmp_dir = Path(tempfile.mkdtemp(prefix="lumice_fp32_accum_"))
    try:
        yield _run("legacy", tmp_dir)
    finally:
        shutil.rmtree(tmp_dir, ignore_errors=True)


@pytest.fixture(scope="module")
def metal_run():
    if platform.system() != "Darwin":
        pytest.skip("Metal backend is only available on macOS")
    tmp_dir = Path(tempfile.mkdtemp(prefix="lumice_fp32_accum_metal_"))
    try:
        yield _run("metal", tmp_dir)
    finally:
        shutil.rmtree(tmp_dir, ignore_errors=True)


def _assert_at(found: tuple, expected: tuple, name: str) -> None:
    assert abs(found[0] - expected[0]) <= _PX_SLACK and abs(found[1] - expected[1]) <= _PX_SLACK, (
        f"{name} peak found at {found}, expected near {expected} — the scene moved, so the pixels "
        f"below are not the ones this file describes"
    )


@pytest.mark.slow
def test_legacy_hot_pixels_hold_the_cmf_ratio_at_100m_rays(legacy_run):
    """X/Y and Z/Y on the sun and the sub-sun equal the 550 nm CMF ratios — the beta defect's own bar."""
    pixels = _hot_pixels(legacy_run)
    _assert_at(pixels["sun"][0], _SUN_PX, "sun")
    _assert_at(pixels["subsun"][0], _SUBSUN_PX, "sub-sun")
    for name, (px, xyz) in pixels.items():
        x, y, z = xyz
        assert y > 0, f"{name} at {px} carries no Y"
        assert abs(x / y - _CMF_X_OVER_Y) <= _XY_TOL, f"{name} at {px}: X/Y={x / y:.5f}, CMF gives {_CMF_X_OVER_Y}"
        assert abs(z / y - _CMF_Z_OVER_Y) <= _ZY_TOL, f"{name} at {px}: Z/Y={z / y:.6f}, CMF gives {_CMF_Z_OVER_Y}"


@pytest.mark.slow
def test_legacy_hot_pixel_energy_matches_metal_to_one_percent(legacy_run, metal_run):
    """Y/emitted per hot pixel agrees across the two routes — the magnitude half of the same drift."""
    legacy = _hot_pixels(legacy_run)
    metal = _hot_pixels(metal_run)
    for name in ("sun", "subsun"):
        lp, lxyz = legacy[name]
        mp, mxyz = metal[name]
        _assert_at(mp, lp, f"metal {name}")
        l_ratio = lxyz[1] / legacy_run.emitted_energy
        m_ratio = mxyz[1] / metal_run.emitted_energy
        assert abs(l_ratio / m_ratio - 1.0) <= _METAL_Y_TOL, (
            f"{name}: Y/emitted legacy {l_ratio:.6g} vs metal {m_ratio:.6g} ({(l_ratio / m_ratio - 1.0) * 100:+.2f}%)"
        )
