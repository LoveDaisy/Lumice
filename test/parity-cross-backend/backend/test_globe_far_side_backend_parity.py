"""Globe far-side clip: GPU backend vs legacy CPU, on the finished CLI image.

On the globe one pixel images two sky directions — where its ray enters the sphere (near) and
where it leaves (far) — and ``visible`` clips each by its own direction
(``RenderConsumer::PostSnapshot``'s near / far split). That split needs the far side's share of
every pixel's energy kept apart from the total. On the legacy CPU path the consumer projects the
rays itself; on a device-fused GPU route the backend has to hand that share over as its own plane
(``TraceBackend::ReadbackFarXyzAccum`` -> ``SimData::xyz_pixel_data_far_``), and a backend or a
simulator drain that drops it does not fail: the consumer reads the share as zero and silently
draws the picture from before the far side had its own clip.

So this compares the one thing both routes end in, the PNG, on a scene where the split changes
most of the frame: a globe at fade 0.8 under ``visible: upper``, from below (el -35: near points
in the kept half with clipped far points behind them) and from above (el +35: clipped near points
with kept far points behind them). Ruler: 16x16 block means of the luminance, worst block.
Measured at 10M rays: CPU vs Metal differ by <= 2 LSB per block, while the same CPU image against
the unsplit picture (what a dropped far plane produces) differs by ~96 LSB (el -35) and ~137 LSB
(el +35) at the worst block — the bar sits far from both.

@pytest.mark.slow for cadence (two full renders per backend), not for the shared library: it
drives the static CLI.
"""

from __future__ import annotations

import os
import platform
from pathlib import Path

import numpy as np
import pytest
from PIL import Image

from test.e2e.runner import get_project_root, run_lumice

_CONFIG = get_project_root() / "test" / "e2e" / "configs" / "globe_far_side_own_visibility.json"
_BLOCK = 16
# Worst 16x16 block-mean luminance difference allowed between a GPU route and legacy CPU.
# Measured 2 LSB (Metal, 10M rays); a dropped far plane measures ~96 LSB.
_MAX_BLOCK_DIFF_LSB = 12.0

_HAS_METAL = platform.system() == "Darwin" and os.environ.get("LUMICE_SKIP_METAL_TESTS") != "1"
_HAS_CUDA = platform.system() in ("Linux", "Windows") and os.environ.get("LUMICE_HAS_CUDA") == "1"


def _render(backend: str, out_dir: Path) -> list[np.ndarray]:
    out_dir.mkdir(parents=True, exist_ok=True)
    r = run_lumice(["-f", str(_CONFIG), "-o", str(out_dir), "--format", "png", "--backend", backend])
    assert r.returncode == 0, f"{backend} render failed:\n{r.stderr[-2000:]}"
    log = r.stdout + r.stderr
    if backend != "cpu":
        # Routed to the requested backend and never dropped back to legacy CPU — a CPU-vs-CPU pair
        # would pass the comparison below without testing the device route at all.
        name = {"metal": "MetalTraceBackend", "cuda": "CudaTraceBackend"}[backend]
        assert f"routing via {name}" in log, f"{backend} was not routed:\n{log[-2000:]}"
        assert "falling back" not in log, f"{backend} fell back:\n{log[-2000:]}"
    return [
        np.asarray(Image.open(out_dir / f"img_0{i}.png").convert("L"), dtype=np.float64) for i in (1, 2)
    ]


def _block_means(img: np.ndarray) -> np.ndarray:
    h, w = img.shape
    return img[: h - h % _BLOCK, : w - w % _BLOCK].reshape(h // _BLOCK, _BLOCK, w // _BLOCK, _BLOCK).mean(axis=(1, 3))


@pytest.mark.slow
@pytest.mark.parametrize(
    "backend",
    [
        pytest.param("metal", marks=pytest.mark.skipif(not _HAS_METAL, reason="Metal needs macOS with a GPU")),
        pytest.param("cuda", marks=pytest.mark.skipif(not _HAS_CUDA, reason="CUDA needs LUMICE_HAS_CUDA=1")),
    ],
)
def test_globe_far_side_clip_matches_legacy_cpu(backend: str, tmp_path: Path) -> None:
    cpu = _render("cpu", tmp_path / "cpu")
    gpu = _render(backend, tmp_path / backend)
    for idx, (a, b) in enumerate(zip(cpu, gpu), start=1):
        assert a.shape == b.shape
        worst = float(np.abs(_block_means(a) - _block_means(b)).max())
        # Positive control: the frame is lit, so an all-black pair cannot pass as agreement.
        assert a.mean() > 5.0, f"img_0{idx}: the CPU frame is nearly black ({a.mean():.2f})"
        assert worst <= _MAX_BLOCK_DIFF_LSB, (
            f"img_0{idx}: worst {_BLOCK}x{_BLOCK} block differs by {worst:.1f} LSB between {backend} and legacy "
            f"CPU (bar {_MAX_BLOCK_DIFF_LSB}) — a far-side share not delivered by the device route reads ~96+"
        )
