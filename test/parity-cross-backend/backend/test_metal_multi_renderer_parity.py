"""Per-renderer Metal-vs-legacy-CPU parity for multi-renderer sessions.

A Metal session serves every renderer of the config at once: the exit tail
projects each exit ray onto N device planes (one per renderer), accumulates N
landed-weight slots, and the host reads N planes back
(``MetalTraceBackend::Impl::AccumRendererPlanes``; ``doc/seam-design.md`` §4.2).
Before that, ``Simulator::CanUseBackend`` refused any config with more than one
renderer and the whole session ran on the legacy CPU path — so every existing
parity file, which runs one renderer, was structurally blind to the N-plane
path. This file is its coverage. Legacy CPU is the oracle, as everywhere in
this layer.

Two scenes, chosen for what they reach rather than for symmetry:

  * ``multi_lens`` — 3 renderers (linear / fisheye_equal_area /
    dual_fisheye_equal_area, all 256²), single wavelength (550 nm), 10M rays.
    A count strictly between 1 and the device cap (``kMaxRenderersDevice`` = 4),
    dense enough that the battery's 4×4 block ruler is informative, and the one
    scene here whose scalar ledger is a constant, so the two-ledger check
    below runs at its tight bar.
  * ``multi_renderer_parity_dual`` — the user's two-renderer document
    (fisheye_equidistant + dual_fisheye_equal_area, D65, three crystals with
    raypath filters), at a quarter of its resolution (256² + 512×256) and 20M
    rays. Two planes of DIFFERENT sizes in one session, which ``multi_lens``
    does not have: a wrong per-renderer plane offset (``xyz_off`` / ``lane_off``
    prefix sums) only shows when the planes are not all the same size. The
    scene the throughput gate measures (``multi_renderer_throughput_dual``, 5M
    rays at full resolution) is the same document, and at that size it is
    noise: legacy's own two-seed block-mean corr is 0.007 / 0.026, i.e. the
    battery's ruler cannot tell the oracle from itself, so nothing can be said
    with it. The quarter-resolution, 20M-ray derivative is where a statement
    becomes possible (below), and it is a separate fixture so the throughput
    numbers keep the document they were measured on.

Four checks per renderer, and the reason the fourth exists:

  (a) block-mean Pearson corr, (b) render PSNR ≥ T_PSNR_DB, (c)
      |Y_metal[i] / Y_legacy[i] − 1| ≤ T_ENERGY_TOL — the battery's own three
      (``test/e2e/_projection_battery.py``), applied to each plane SEPARATELY.
      Never to the sum over renderers: a plane that received another
      renderer's energy and a plane that lost it cancel in a sum and pass a
      pooled energy check. The block size is per scene: the battery's 4×4 for
      ``multi_lens``; 16×16 for the dual scene, where the 4×4 ruler reads
      0.37 / 0.61 between two LEGACY seeds — at 16×16 legacy agrees with
      itself at 0.886–0.904 / 0.959–0.963 over three seed pairs, and Metal
      against legacy reads 0.883–0.887 / 0.959–0.962 over the same three
      pairs, i.e. indistinguishable from the oracle's own noise. A plane
      routed to the wrong renderer reads ~0 on either ruler.
  (d) the two-ledger ratio ``R[i] = sum(Y plane i) / snapshot_intensity[i]``,
      compared across backends per renderer. ``snapshot_intensity`` is the
      renderer's ``landed_weight`` slot; the plane is its per-pixel
      accumulation. Both are fed by the same exit weights, so their ratio is a
      constant that a correct backend reproduces and that (a)–(c) cannot see
      broken: fold renderer 1's landed weight into renderer 0's slot and every
      plane is still right, so corr / PSNR / energy all stay green, while
      ``R[0]`` drops to ~0.25 and ``R[1]`` has no denominator. That is the
      exact shape of a defect the CUDA port had (its reduction epilogue merged
      the N slots), and only this ratio caught it. The bar is per scene:
        - single wavelength: 2%. Metal's ``R`` sits 0.27–0.47% below legacy's
          on every renderer of ``multi_lens`` — and by the same six digits in
          a single-renderer session of each renderer alone, and 0.38% on the
          battery's own ``parhelion`` row — so this is the Metal backend's
          baseline against legacy (whose scalar ledger is an fp32 running sum),
          not something the N-plane path adds. 2% is 4× above that baseline
          and 25× below a merged slot. (The CUDA backend reads ≤0.03% here; its
          mirror file uses the energy-accounting battery's 0.1%.)
        - D65: 5%. The wavelength draw enters ``R`` (Metal samples per ray,
          legacy per batch), and the cross-backend ratio spreads ±1.8% over
          three seeds on the dual scene (0.987 / 1.018 / 0.995 and 0.990 /
          1.012 / 0.996). 5% is above that spread and 10× below a merge.

Routing: every Metal arm must report ``routed_backend == "metal"`` and
``fell_back == False`` through the C API (``LUMICE_GetActiveBackend`` /
``LUMICE_GetBackendFallbackFlag``) — this is also the e2e statement that a
multi-renderer config no longer falls back.

Measured on the reference machine (Apple M-series, seed 42 both arms; the
readings are deterministic per backend for a fixed seed):
  multi_lens                 corr(4×4) 0.9863 / 0.9907 / 0.9999, PSNR 40.1 /
                             40.7 / 47.4 dB, energy 0.9954 / 0.9955 / 0.9971,
                             R ratio 0.99527 / 0.99536 / 0.99731
  multi_renderer_parity_dual corr(16×16) 0.8830 / 0.9618, PSNR 20.5 / 17.7 dB,
                             energy 0.9977 / 0.9979, R ratio 0.9873 / 0.9902
Thresholds (b)–(c) and the 4×4 corr floor are the battery's, not re-derived
here; the 16×16 floor and the ledger bars are calibrated above.

@pytest.mark.slow (shared-lib build: ``./scripts/build.sh -sj release``),
Darwin-only. Runs serially (capi_runner mutates os.environ + a process-global
log callback). Cost is the legacy oracle: ~10 s for ``multi_lens``, ~35 s for
the dual scene (a fixed seed pins legacy to one worker).
"""

from __future__ import annotations

import json
import platform
from dataclasses import dataclass

import numpy as np
import pytest

from test.e2e.capi_runner import BufferedSimResult, run_scene_capi_buffered
from test.e2e._parity_metrics import _DS_BH, _DS_BW, _raw_corr_ds, render_psnr
from test.e2e._projection_battery import T_ENERGY_TOL, T_PSNR_DB, T_RAW_CORR_DS
from test.e2e.runner import get_project_root

_SEED = 42
_TIMEOUT = 600

# Two-ledger bars (rationale in the module docstring).
T_LEDGER_TOL_SINGLE_WL = 0.02
T_LEDGER_TOL_D65 = 0.05

# 16×16 block-mean corr floor for the sparse dual scene (measured 0.883 / 0.962
# against an oracle self-consistency of 0.886–0.963; a misrouted plane reads ~0).
_DS16 = 16
T_RAW_CORR_DS16 = 0.80

_CONFIGS_DIR = get_project_root() / "test" / "e2e" / "configs"


@dataclass(frozen=True)
class Scene:
    config: str
    num_renderers: int
    block: int         # block-mean tile for the corr ruler
    corr_floor: float
    ledger_tol: float


_SCENES = [
    Scene("multi_lens", 3, _DS_BH, T_RAW_CORR_DS, T_LEDGER_TOL_SINGLE_WL),
    Scene("multi_renderer_parity_dual", 2, _DS16, T_RAW_CORR_DS16, T_LEDGER_TOL_D65),
]
assert _DS_BH == _DS_BW, "the battery's block is square; the Scene rows assume one tile size"

pytestmark = pytest.mark.skipif(
    platform.system() != "Darwin", reason="Metal backend is only available on macOS"
)


def _run(config_name: str, backend: str, num_renderers: int) -> BufferedSimResult:
    cfg = _CONFIGS_DIR / f"{config_name}.json"
    return run_scene_capi_buffered(
        str(cfg), sim_seed=_SEED, backend=backend, timeout_sec=_TIMEOUT, num_renderers=num_renderers
    )


def _lens_types(config_name: str) -> list[str]:
    with open(_CONFIGS_DIR / f"{config_name}.json", encoding="utf-8") as f:
        return [r["lens"]["type"] for r in json.load(f)["render"]]


def _y_sum(plane: np.ndarray) -> float:
    return float(plane[..., 1].sum())


def _ledger_ratio(plane: np.ndarray, snapshot_intensity: float) -> float:
    """sum(Y plane) / snapshot_intensity — nan when the scalar ledger is empty."""
    if snapshot_intensity == 0.0:
        return float("nan")
    return _y_sum(plane) / snapshot_intensity


@pytest.mark.slow
@pytest.mark.parametrize("scene", _SCENES, ids=lambda s: s.config)
def test_metal_multi_renderer_parity(scene: Scene):
    """Each renderer's Metal plane matches its legacy plane; each ledger pair agrees."""
    lens_types = _lens_types(scene.config)
    assert len(lens_types) == scene.num_renderers, (
        f"{scene.config}: fixture has {len(lens_types)} renderers, test expects {scene.num_renderers}"
    )

    legacy = _run(scene.config, "legacy", scene.num_renderers)
    metal = _run(scene.config, "metal", scene.num_renderers)

    assert legacy.routed_backend == "legacy" and not legacy.fell_back, (
        f"{scene.config}: legacy oracle routed={legacy.routed_backend!r} "
        f"fell_back={legacy.fell_back} - env pollution suspected."
    )
    assert metal.routed_backend == "metal", (
        f"{scene.config}: routed={metal.routed_backend!r} (expected 'metal'); "
        f"LUMICE_GetActiveBackend did not report Metal. log tail: {metal.log_lines[-5:]}"
    )
    assert not metal.fell_back, (
        f"{scene.config}: Metal was requested for a {scene.num_renderers}-renderer config and "
        f"fell back to legacy CPU - the multi-renderer session is NOT on device. "
        f"log tail: {metal.log_lines[-5:]}"
    )
    assert len(metal.flt_bufs) == scene.num_renderers
    assert len(legacy.flt_bufs) == scene.num_renderers

    failures = []
    for i, lens_type in enumerate(lens_types):
        m_plane, l_plane = metal.flt_bufs[i], legacy.flt_bufs[i]
        assert m_plane.shape == l_plane.shape, (
            f"{scene.config} renderer[{i}]: plane shape metal {m_plane.shape} vs legacy {l_plane.shape}"
        )
        corr = _raw_corr_ds({"xyz": m_plane}, {"xyz": l_plane}, scene.block, scene.block)
        psnr = render_psnr(metal.rgb_bufs[i], legacy.rgb_bufs[i])
        legacy_y = _y_sum(l_plane)
        assert legacy_y > 0.0, f"{scene.config} renderer[{i}]: legacy total Y == 0 - no signal"
        energy_ratio = _y_sum(m_plane) / legacy_y
        r_metal = _ledger_ratio(m_plane, metal.snapshot_intensities[i])
        r_legacy = _ledger_ratio(l_plane, legacy.snapshot_intensities[i])
        assert r_legacy == r_legacy and r_legacy != 0.0, (
            f"{scene.config} renderer[{i}]: legacy snapshot_intensity == 0 - no ledger to compare"
        )
        ledger_ratio = r_metal / r_legacy

        print(
            f"[multi-renderer-parity] {scene.config} renderer[{i}] {lens_type} "
            f"{m_plane.shape[1]}x{m_plane.shape[0]}: ds{scene.block}_corr={corr:.4f} "
            f"psnr={psnr:.2f}dB energy_ratio={energy_ratio:.4f} "
            f"R_metal/R_legacy={ledger_ratio:.6f} "
            f"(snapshot_intensity metal={metal.snapshot_intensities[i]:.6g} "
            f"legacy={legacy.snapshot_intensities[i]:.6g}; ledger tol +/-{scene.ledger_tol * 100:.1f}%)"
        )

        # Collected, not asserted one by one: a red on renderer 0 must not hide
        # renderer 1's reading, which is where a misrouted plane shows.
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
                f"renderer[{i}] {lens_type}: R_metal/R_legacy {ledger_ratio:.6f} outside "
                f"[1 +/- {scene.ledger_tol}] - this renderer's landed-weight slot and its plane "
                f"disagree (a merged or misrouted slot; the planes alone cannot show it)"
            )

    assert not failures, f"{scene.config}: per-renderer parity failed:\n  " + "\n  ".join(failures)
