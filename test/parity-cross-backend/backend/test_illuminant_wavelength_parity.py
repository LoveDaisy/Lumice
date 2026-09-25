"""Standard-illuminant wavelength sampling: legacy CPU vs the GPU backends.

The two routes draw wavelengths differently. A GPU backend gives every ray its own
wavelength, an index into a 64-entry midpoint table over [380, 780] nm (``wl_pool.hpp``).
The legacy CPU route traces one wavelength per physics batch (128 rays), continuous on
[380, 780], and takes the batches' wavelengths from a randomly shifted golden-ratio
sequence (``WavelengthStratifier``) rather than independently. Both are uniform on the
band, so a D65 scene must come out with the same frame-level spectral statistics on
either route — that is what this file checks, and what it reads them from:

  * ``R = sum(flt_buf Y) / snapshot_intensity`` — the landed energy's weight-averaged
    y-bar, i.e. the wavelength distribution as the image ledger sees it. (Under a single
    wavelength this is a closed-form constant; ``test_cuda_energy_accounting_parity.py``
    asserts that one, and keeps D65 out of it for the reason the first test here exists.)
  * the frame chromaticity ``x, y`` from the summed XYZ.

Two tests, two questions.

``test_legacy_illuminant_frame_statistics_are_seed_stable`` needs no GPU. With one
wavelength per batch drawn independently, a 2M-ray frame is an average over ~15600 random
wavelengths and ``R`` moved by percents between seeds — which is what made D65 unusable
as a ledger check before, and what the stratified schedule removes. Measured on this
config over 12 seeds: spread of ``R`` 0.053%, of ``x`` 0.00013, of ``y`` 0.00019. The
bounds (0.2%, 0.0006, 0.0006) sit ~3-4x above that; a build with the stratifier reverted
to independent draws reads 2.2%, 0.0020 and 0.0056 on the same four seeds.

``test_legacy_and_gpu_sample_the_same_wavelength_distribution`` compares the 6-seed means
of the two routes. Measured on Metal over 12 seeds: ``R`` -0.064%, ``x`` -0.00006,
``y`` -0.00021 against legacy, with the GPU route carrying the larger per-seed spread
(``R`` 0.10%, ``x``/``y`` ~0.0002 per seed, per-ray draws over 64 entries). The ``y``
offset is 3.4 standard errors off zero and is the GPU table's midpoint quadrature of the
colour-matching functions, not sampling noise; the bounds (0.3% on ``R``, 0.0008 on
``x``/``y``) are that offset plus five standard errors of a 6-seed GPU mean. A wavelength
range or density that differs between the routes moves these statistics by percents; the
reverted build above also fails here, on ``x`` (-0.00092), through its own seed scatter.

Requires the shared-lib build (``./scripts/build.sh -sj release``). The GPU test runs on
Metal on macOS and on CUDA where ``LUMICE_HAS_CUDA=1``; elsewhere it skips.
All tests are @pytest.mark.slow.
"""

from __future__ import annotations

import os
import platform
import statistics

import pytest

from test.e2e.capi_runner import BufferedSimResult, run_scene_capi_buffered
from test.e2e.runner import get_project_root

_CONFIG = str(get_project_root() / "test" / "e2e" / "configs" / "illuminant_wavelength_parity.json")
_TIMEOUT = 900

_STABILITY_SEEDS = (42, 43, 44, 45)
_T_R_SPREAD = 0.002
_T_XY_SPREAD = 0.0006

_PARITY_SEEDS = (42, 43, 44, 45, 46, 47)
_T_R_RATIO = 0.003
_T_XY_DIFF = 0.0008

_GPU_BACKENDS = []
if platform.system() == "Darwin":
    _GPU_BACKENDS.append("metal")
if platform.system() in ("Linux", "Windows") and os.environ.get("LUMICE_HAS_CUDA") == "1":
    _GPU_BACKENDS.append("cuda")


def _frame_stats(backend: str, seed: int) -> dict:
    r: BufferedSimResult = run_scene_capi_buffered(_CONFIG, sim_seed=seed, backend=backend, timeout_sec=_TIMEOUT)
    assert r.routed_backend == backend and not r.fell_back, (
        f"seed {seed}: requested {backend!r}, routed {r.routed_backend!r}, fell_back={r.fell_back}"
    )
    x_sum, y_sum, z_sum = (float(r.flt_buf[..., c].sum()) for c in range(3))
    assert r.snapshot_intensity > 0.0 and y_sum > 0.0, f"{backend} seed {seed}: empty frame"
    total = x_sum + y_sum + z_sum
    return {"R": y_sum / float(r.snapshot_intensity), "x": x_sum / total, "y": y_sum / total}


@pytest.mark.slow
def test_legacy_illuminant_frame_statistics_are_seed_stable():
    rows = [_frame_stats("legacy", seed) for seed in _STABILITY_SEEDS]
    rs = [row["R"] for row in rows]
    r_spread = max(rs) / min(rs) - 1.0
    x_spread = max(row["x"] for row in rows) - min(row["x"] for row in rows)
    y_spread = max(row["y"] for row in rows) - min(row["y"] for row in rows)
    detail = f"R={rs} x/y={[(round(row['x'], 6), round(row['y'], 6)) for row in rows]}"
    assert r_spread <= _T_R_SPREAD, f"R spread {r_spread:.4%} > {_T_R_SPREAD:.2%} across seeds: {detail}"
    assert x_spread <= _T_XY_SPREAD, f"x spread {x_spread:.6f} > {_T_XY_SPREAD}: {detail}"
    assert y_spread <= _T_XY_SPREAD, f"y spread {y_spread:.6f} > {_T_XY_SPREAD}: {detail}"


@pytest.mark.slow
@pytest.mark.skipif(not _GPU_BACKENDS, reason="no GPU backend on this host (Metal: macOS; CUDA: LUMICE_HAS_CUDA=1)")
@pytest.mark.parametrize("gpu", _GPU_BACKENDS)
def test_legacy_and_gpu_sample_the_same_wavelength_distribution(gpu: str):
    legacy = [_frame_stats("legacy", seed) for seed in _PARITY_SEEDS]
    device = [_frame_stats(gpu, seed) for seed in _PARITY_SEEDS]
    mean = {k: (statistics.mean(r[k] for r in legacy), statistics.mean(r[k] for r in device)) for k in ("R", "x", "y")}
    detail = ", ".join(f"{k}: legacy {a:.6f} {gpu} {b:.6f}" for k, (a, b) in mean.items())
    r_ratio = mean["R"][1] / mean["R"][0] - 1.0
    assert abs(r_ratio) <= _T_R_RATIO, f"R ratio {r_ratio:+.4%} beyond {_T_R_RATIO:.1%} ({detail})"
    for k in ("x", "y"):
        diff = mean[k][1] - mean[k][0]
        assert abs(diff) <= _T_XY_DIFF, f"{k} differs by {diff:+.6f} > {_T_XY_DIFF} ({detail})"
