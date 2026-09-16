"""Per-renderer GPU-vs-legacy parity checks for multi-renderer sessions.

The one implementation behind ``test_metal_multi_renderer_parity.py`` and
``test_cuda_multi_renderer_parity.py`` (``test/parity-cross-backend/backend/``):
the two backends serve N renderers through the same seam (``SessionSpec::renders``,
one device plane + one landed-weight slot per renderer), so the proposition —
each renderer's plane matches legacy's, and each renderer's two ledgers agree —
is one proposition, checked by one function. The per-backend files own only
what differs: the skip gate, the ``backend`` mode, and the calibrated bars.

Four checks per renderer, and the reason the fourth exists:

  (a) block-mean Pearson corr ≥ ``Scene.corr_floor`` at ``Scene.block``,
  (b) render PSNR ≥ ``T_PSNR_DB``,
  (c) |Y_gpu[i] / Y_legacy[i] − 1| ≤ ``T_ENERGY_TOL`` — the projection battery's
      own three (``_projection_battery.py``), applied to each plane SEPARATELY.
      Never to the sum over renderers: a plane that received another renderer's
      energy and a plane that lost it cancel in a sum and pass a pooled energy
      check.
  (d) the two-ledger ratio ``R[i] = sum(Y plane i) / snapshot_intensity[i]``,
      compared across backends per renderer. ``snapshot_intensity`` is the
      renderer's ``landed_weight`` slot; the plane is its per-pixel accumulation.
      Both are fed by the same exit weights, so their ratio is a constant that a
      correct backend reproduces and that (a)–(c) cannot see broken: fold
      renderer 1's landed weight into renderer 0's slot and every plane is still
      right, so corr / PSNR / energy all stay green, while ``R[0]`` drops to
      ~0.25 and ``R[1]`` has no denominator (measured on Metal by doing exactly
      that). That is the shape of a defect the CUDA port had (its reduction
      epilogue merged the N slots), and only this ratio caught it. The bar is
      ``Scene.ledger_tol``, calibrated per scene and per backend in the test
      file that owns it.

Failures are collected across renderers and raised once, so a red on renderer
0 never hides renderer 1's reading — which is where a misrouted plane shows.
"""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import List

import numpy as np

from test.e2e.capi_runner import BufferedSimResult
from test.e2e._parity_metrics import _raw_corr_ds, render_psnr
from test.e2e._projection_battery import T_ENERGY_TOL, T_PSNR_DB


@dataclass(frozen=True)
class Scene:
    config: str          # fixture name under test/e2e/configs/, without .json
    num_renderers: int
    block: int           # block-mean tile (square) for the corr ruler
    corr_floor: float
    ledger_tol: float    # |R_gpu / R_legacy − 1| bar


def lens_types(configs_dir: Path, config_name: str) -> List[str]:
    with open(configs_dir / f"{config_name}.json", encoding="utf-8") as f:
        return [r["lens"]["type"] for r in json.load(f)["render"]]


def _y_sum(plane: np.ndarray) -> float:
    return float(plane[..., 1].sum())


def _ledger_ratio(plane: np.ndarray, snapshot_intensity: float) -> float:
    """sum(Y plane) / snapshot_intensity — nan when the scalar ledger is empty."""
    if snapshot_intensity == 0.0:
        return float("nan")
    return _y_sum(plane) / snapshot_intensity


def check_multi_renderer_parity(
    scene: Scene, lens: List[str], legacy: BufferedSimResult, gpu: BufferedSimResult, gpu_name: str
) -> None:
    """Assert checks (a)–(d) for every renderer of `scene`; prints one line per renderer."""
    assert len(lens) == scene.num_renderers, (
        f"{scene.config}: fixture has {len(lens)} renderers, test expects {scene.num_renderers}"
    )
    assert len(gpu.flt_bufs) == scene.num_renderers, (
        f"{scene.config}: {gpu_name} arm returned {len(gpu.flt_bufs)} planes, expected {scene.num_renderers}"
    )
    assert len(legacy.flt_bufs) == scene.num_renderers, (
        f"{scene.config}: legacy arm returned {len(legacy.flt_bufs)} planes, expected {scene.num_renderers}"
    )

    failures = []
    for i, lens_type in enumerate(lens):
        g_plane, l_plane = gpu.flt_bufs[i], legacy.flt_bufs[i]
        assert g_plane.shape == l_plane.shape, (
            f"{scene.config} renderer[{i}]: plane shape {gpu_name} {g_plane.shape} vs legacy {l_plane.shape}"
        )
        corr = _raw_corr_ds({"xyz": g_plane}, {"xyz": l_plane}, scene.block, scene.block)
        psnr = render_psnr(gpu.rgb_bufs[i], legacy.rgb_bufs[i])
        legacy_y = _y_sum(l_plane)
        assert legacy_y > 0.0, f"{scene.config} renderer[{i}]: legacy total Y == 0 - no signal"
        energy_ratio = _y_sum(g_plane) / legacy_y
        r_gpu = _ledger_ratio(g_plane, gpu.snapshot_intensities[i])
        r_legacy = _ledger_ratio(l_plane, legacy.snapshot_intensities[i])
        assert r_legacy == r_legacy and r_legacy != 0.0, (
            f"{scene.config} renderer[{i}]: legacy snapshot_intensity == 0 - no ledger to compare"
        )
        ledger_ratio = r_gpu / r_legacy

        print(
            f"[multi-renderer-parity] {scene.config} renderer[{i}] {lens_type} "
            f"{g_plane.shape[1]}x{g_plane.shape[0]}: ds{scene.block}_corr={corr:.4f} "
            f"psnr={psnr:.2f}dB energy_ratio={energy_ratio:.4f} "
            f"R_{gpu_name}/R_legacy={ledger_ratio:.6f} "
            f"(snapshot_intensity {gpu_name}={gpu.snapshot_intensities[i]:.6g} "
            f"legacy={legacy.snapshot_intensities[i]:.6g}; ledger tol +/-{scene.ledger_tol * 100:.1f}%)"
        )

        if corr < scene.corr_floor:
            failures.append(
                f"renderer[{i}] {lens_type}: ds{scene.block}_corr {corr:.4f} < {scene.corr_floor}"
            )
        if psnr < T_PSNR_DB:
            failures.append(f"renderer[{i}] {lens_type}: PSNR {psnr:.2f} dB < {T_PSNR_DB}")
        if abs(energy_ratio - 1.0) > T_ENERGY_TOL:
            failures.append(
                f"renderer[{i}] {lens_type}: energy_ratio {energy_ratio:.4f} outside "
                f"[1 +/- {T_ENERGY_TOL}]"
            )
        if not (ledger_ratio == ledger_ratio) or abs(ledger_ratio - 1.0) > scene.ledger_tol:
            failures.append(
                f"renderer[{i}] {lens_type}: R_{gpu_name}/R_legacy {ledger_ratio:.6f} outside "
                f"[1 +/- {scene.ledger_tol}] - this renderer's landed-weight slot and its plane "
                f"disagree (a merged or misrouted slot; the planes alone cannot show it)"
            )

    assert not failures, f"{scene.config}: per-renderer parity failed:\n  " + "\n  ".join(failures)
