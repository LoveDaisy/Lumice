"""Dual-renderer CUDA throughput gate: serving two planes ≥ 85% of serving one.

The proposition, statistic, threshold provenance and denominator policy are in
``test/e2e/_multi_renderer_throughput.py`` (shared with the Metal file); this
file owns the CUDA skip gate and the measured figures for the CUDA reference
machine.

Honesty note on this backend's margin. The CUDA reference machine
(``doc/machines.md``: RTX 5090 D under WSL2) does not lock its clocks — the SM
clock boosts 180→2407 MHz on demand — and a drain-aligned sample there carries
a CoV of 0.10–0.17, against Metal's 0.06–0.10. Fourteen idle same-commit runs
of the old 21-rep ratio-of-medians statistic measured the worst arm at mean
0.911, standard deviation 0.045, range 0.848–0.971, with one false red. The
three absolute arm rates did not fall together, the ratio had no material
correlation with absolute throughput, and the faster single arm changed
between runs: GitHub issue #458 was CUDA run-to-run variance, not interference
or a backend regression.

The CUDA precise gate therefore keeps the 0.85 design target but uses 63
interleaved reps and the 10% winsorized mean of each rep's paired dual/single
ratios. Run-level resampling of the measured triples estimates about 0.7%
joint false-red probability and 80% single-run power for a 10.7% true ratio
regression. A controlled multi-plane-only tail-delay probe at that boundary
made the gate red in 5/5 idle runs; the unmodified control passed 6/6. These
figures calibrate this reference machine, not every CUDA device, and the daily
record remains the long-term check on drift.

Requires (same gate as the CUDA parity files): Linux/Windows,
``LUMICE_HAS_CUDA=1``, a ``LUMICE_CUDA_ENABLED=ON`` build and an NVIDIA device.
The CI ``cuda-compile`` legs build but do not run it (no GPU on the runner).
@pytest.mark.slow.
"""

from __future__ import annotations

import os
import platform

import pytest

from test.e2e._multi_renderer_throughput import run_dual_renderer_gate
from test.e2e.runner import get_project_root

_CUDA_AVAILABLE = (
    platform.system() in ("Linux", "Windows") and os.environ.get("LUMICE_HAS_CUDA") == "1"
)

pytestmark = pytest.mark.skipif(
    not _CUDA_AVAILABLE,
    reason=(
        "CUDA backend requires Linux/Windows + LUMICE_HAS_CUDA=1 + "
        "LUMICE_CUDA_ENABLED=ON build with an NVIDIA device."
    ),
)

_CONFIGS_DIR = get_project_root() / "test" / "e2e" / "configs"
_TIMEOUT = 300  # CUDA first-launch JIT can take a minute


@pytest.mark.slow
def test_cuda_dual_renderer_throughput(tmp_path):
    run_dual_renderer_gate(_CONFIGS_DIR, tmp_path, "cuda", _TIMEOUT, "cuda-dual-throughput")
