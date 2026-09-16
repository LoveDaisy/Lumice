"""Dual-renderer CUDA throughput gate: serving two planes ≥ 85% of serving one.

The proposition, statistic, threshold provenance and denominator policy are in
``test/e2e/_multi_renderer_throughput.py`` (shared with the Metal file); this
file owns the CUDA skip gate and the measured figures for the CUDA reference
machine.

Honesty note on this backend's margin. The CUDA reference machine
(``doc/machines.md``: RTX 5090 D under WSL2) does not lock its clocks — the SM
clock boosts 180→2407 MHz on demand — and a drain-aligned sample there carries
a CoV of 0.10–0.17, against Metal's 0.06–0.10. The N-plane port measured the
ratio at n=7 as 0.708 / 0.842 (below the gate) and at n=21 paired as
0.889 / 0.974 (above it, with the interquartile range straddling 0.85). The
gate here uses the ratio of medians over 21 interleaved reps per arm, the
highest-power form of that measurement, and keeps 0.85: the number is the
design target, and a gate that reads red on this machine is reporting the
machine's noise floor, not a licence to lower the bar. The figures below are
what this file measured when it was written; a red should be read against
them (and re-run) before anything else.

Measured (CUDA reference machine, Linux role — RTX 5090 D under WSL2, CUDA
12.9, `sm_120` SASS; the same fixtures and drain-aligned rate as the Metal
file; two consecutive runs of this gate, 21 interleaved reps each):
    run 1  dual 282.84M (CoV 0.148)  single_a 312.29M (0.164)  single_b 285.13M (0.135)
           dual/single_a = 0.906, dual/single_b = 0.992; legacy 13.41M → 21.1×
    run 2  dual 265.01M (CoV 0.144)  single_a 300.38M (0.120)  single_b 292.65M (0.132)
           dual/single_a = 0.882, dual/single_b = 0.906; legacy 13.94M → 19.0×
Both runs pass; the run-to-run swing of the ratio (0.906→0.882, 0.992→0.906)
is the noise floor described above, and is why the gate is a median over 21
samples and not a single pair.

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
