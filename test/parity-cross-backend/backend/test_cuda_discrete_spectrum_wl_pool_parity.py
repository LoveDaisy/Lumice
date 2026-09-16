"""CUDA discrete-spectrum parity: every listed wavelength must reach the image.

A discrete (custom) spectrum is a list of ``(wavelength, weight)`` rows. The
simulator runs it one wavelength at a time — ``simulator.cpp`` calls
``BeginSession`` once per row with the same ``spec.scene`` pointer and a
different ``spec.wl`` — and each session's ``ComputeWlPool`` fills the whole
M-slot wavelength pool with that single wavelength's refractive index, CMF
triple and SPD weight. The CUDA backend caches the uploaded pool across the
per-batch ``BeginSession`` cycle so an illuminant scene (D65, where
``spec.wl`` is the empty ``WlParam{}`` for every session) pays the H2D once
per scene. That cache used to be keyed on the scene pointer alone: with a
discrete list, the pool computed for the FIRST wavelength survived every
later session unchanged, so the whole list was traced and colour-weighted as
the first wavelength only. Five equal-weight wavelengths 560–720 nm rendered
as pure 560 nm — green — while legacy and Metal (which rebuilds the pool on
every ``BeginSession``) rendered the orange-yellow mixture.

What this measures. Only chromaticity can see this defect: with equal SPD
weights every wavelength lands the same total energy, so the energy-ratio
metric the other CUDA parity files assert is blind to it, and the whole-frame
correlation of two smooth halos is nearly so. This file sums the linear XYZ
plane over the whole frame and compares each backend's normalised
``(X, Y, Z)`` shares of that sum — the frame-integrated chromaticity. For
560 nm alone the CIE 1931 CMF gives shares ≈ (0.37, 0.63, 0.00); for the
five-wavelength mixture ≈ (0.54, 0.46, 0.00) — a 0.17 gap on the X share.
The mixture is deliberately confined to the long-wavelength side (Z ≈ 0
throughout) so the defect changes the X:Y balance and nothing else; the
assertion is on the L∞ over the three shares. The defect was first isolated
with a bright-region mask (``Y > 15% of peak``) over tone-mapped PNGs; on the
linear XYZ plane that mask is not usable here — the undeviated-sun pixel of
this scene alone holds ~30% of the frame's Y (77k against 8.4k for the next
pixel), so at 15% the mask is one pixel — and no mask is needed, because the
defect rescales every pixel's CMF triple the same way, so the frame integral
sees it in full.

Two orderings of the same list run as two rows: ``forward`` (560 first) and
``reversed`` (720 first). Under the defect the rendered colour is whatever
wavelength happens to be listed first, so the two rows fail in *opposite*
directions (green vs deep red) from one healthy target; a wrong-first-row
explanation is ruled out by the pair, and a fix that only happens to get
the first wavelength right is not enough to turn both green. The reversed
config is generated from the committed one at test time — the two rows must
differ in list order and nothing else, which a second hand-edited JSON file
cannot guarantee.

MEASUREMENTS-TBD (filled in from the CUDA reference machine before merge).

Requires:
  - ``LUMICE_CUDA_ENABLED=ON`` build with the CUDA toolchain.
  - NVIDIA device visible to the runtime.
  - Shared-lib build produced by the CUDA-enabled Release configuration
    (a plain ``./scripts/build.sh -sj release`` is not enough on its own).

All tests are @pytest.mark.slow.
"""
from __future__ import annotations

import json
import os
import platform
import tempfile

import numpy as np
import pytest

from test.e2e.capi_runner import BufferedSimResult, run_scene_capi_buffered
from test.e2e.runner import get_project_root

CONFIGS_DIR = get_project_root() / "test" / "e2e" / "configs"
_CONFIG = "discrete_spectrum_wl_pool_parity"
_TIMEOUT = 600  # 1M rays × 5 wavelengths; the single-worker legacy arm is the slow one

# max |share_cuda - share_legacy| over (X, Y, Z). See the module docstring.
_T_SHARE_TOL = 0.02

_CUDA_AVAILABLE = (
    platform.system() in ("Linux", "Windows") and os.environ.get("LUMICE_HAS_CUDA") == "1"
)

pytestmark = pytest.mark.skipif(
    not _CUDA_AVAILABLE,
    reason=(
        "CUDA backend requires Linux + LUMICE_HAS_CUDA=1 + LUMICE_CUDA_ENABLED=ON "
        "build with an NVIDIA device. Skipping on this host."
    ),
)

_ORDERS = ("forward", "reversed")
_SEEDS = (42, 43, 44)
_PARAMS = [(order, seed) for order in _ORDERS for seed in _SEEDS]
_IDS = [f"{order}-seed{seed}" for order, seed in _PARAMS]


def _config_path(order: str) -> str:
    """The committed config for ``forward``; a temp copy with the spectrum list reversed otherwise."""
    src_path = CONFIGS_DIR / f"{_CONFIG}.json"
    if order == "forward":
        return str(src_path)
    cfg = json.loads(src_path.read_text())
    cfg["scene"]["light_source"]["spectrum"] = list(reversed(cfg["scene"]["light_source"]["spectrum"]))
    tmp = tempfile.NamedTemporaryFile("w", suffix=".json", delete=False)
    json.dump(cfg, tmp)
    tmp.flush()
    tmp.close()
    return tmp.name


def _assert_routed(r: BufferedSimResult, expected: str, label: str) -> None:
    assert r.routed_backend == expected, (
        f"{label}/{expected}: routed={r.routed_backend!r} (expected {expected!r}); "
        f"see log_lines for the actual path."
    )
    assert not r.fell_back, (
        f"{label}/{expected}: fell back to legacy. Backend was REQUESTED but did not run."
    )


def frame_xyz_shares(flt_buf: np.ndarray) -> np.ndarray:
    """Normalised (X, Y, Z) shares of the XYZ sum over the whole frame.

    Returns a length-3 array that sums to 1; raises if the frame's XYZ sum is
    not positive, because a share of nothing is not a measurement.
    """
    total = flt_buf.reshape(-1, 3).sum(axis=0)
    denom = float(total.sum())
    assert denom > 0.0, "frame XYZ sum is not positive: the arm landed nothing"
    return total / denom


def _fmt(shares: np.ndarray) -> str:
    return "(" + ", ".join(f"{v:.4f}" for v in shares) + ")"


@pytest.mark.slow
@pytest.mark.parametrize(("order", "seed"), _PARAMS, ids=_IDS)
def test_cuda_discrete_spectrum_chromaticity_matches_legacy(order: str, seed: int):
    """max |XYZ share_cuda − share_legacy| ≤ 0.02 over the whole frame.

    Both arms run the same config and seed. A red here means the CUDA arm's
    frame has a different integrated colour than legacy's — for a discrete
    spectrum, that one or more listed wavelengths did not reach the image.
    """
    cfg = _config_path(order)
    label = f"{_CONFIG}/{order}/seed{seed}"

    legacy = run_scene_capi_buffered(cfg, sim_seed=seed, backend="legacy", timeout_sec=_TIMEOUT)
    cuda = run_scene_capi_buffered(cfg, sim_seed=seed, backend="cuda", timeout_sec=_TIMEOUT)

    _assert_routed(legacy, "legacy", label)
    _assert_routed(cuda, "cuda", label)

    s_legacy = frame_xyz_shares(legacy.flt_buf)
    s_cuda = frame_xyz_shares(cuda.flt_buf)
    linf = float(np.abs(s_cuda - s_legacy).max())

    print(
        f"[discrete-spectrum] {label}: legacy XYZ shares={_fmt(s_legacy)} "
        f"cuda XYZ shares={_fmt(s_cuda)} max|diff|={linf:.4f} (tol {_T_SHARE_TOL})"
    )
    assert linf <= _T_SHARE_TOL, (
        f"{label}: frame XYZ shares differ by {linf:.4f} > {_T_SHARE_TOL}: "
        f"legacy={_fmt(s_legacy)} cuda={_fmt(s_cuda)}. "
        "The CUDA arm rendered a different colour than legacy for the same discrete "
        "spectrum. Suspect the wl pool cache in cuda_trace_backend.cu BeginSession: if "
        "it survives a session whose spec.wl differs from the one it was built for, "
        "every wavelength after the first is traced and colour-weighted as the first "
        "(560 nm alone reads shares ~(0.37, 0.63, 0.00); the five-wavelength mixture "
        "~(0.54, 0.46, 0.00))."
    )
