"""Dual-renderer Metal throughput gate: serving two planes ≥ 85% of serving one.

The proposition, statistic, threshold provenance and denominator policy are in
``test/e2e/_multi_renderer_throughput.py`` (shared with the CUDA file); this
file owns the Metal skip gate and the measured figures for the Metal reference
machine.

Measured (Apple M-series development Mac, the Metal reference machine role in
``doc/machines.md``; ``multi_renderer_throughput_{dual,single_a,single_b}``
fixtures — the user's document, 3 crystals + 3 raypath filters, D65,
fisheye_equidistant 1024² + dual_fisheye_equal_area 2048×1024; drain-aligned
rate over 10 drains of 2,097,152 rays; the gate's own 21 interleaved reps):
    dual      median 27.85M rays/s  (CoV 0.104, 21.7–30.4M)
    single_a  median 29.30M rays/s  (CoV 0.113, 21.3–32.2M)
    single_b  median 30.67M rays/s  (CoV 0.105, 22.8–31.8M)
    dual/single_a = 0.950, dual/single_b = 0.908
    legacy CPU dual (12 workers, finite 5M): 6.70M rays/s → dual Metal 4.16×
An earlier 15-rep sample of the same arms read 0.935 / 0.938 and 4.33×.
Before N-plane accumulation the same dual config ran on the legacy CPU path
at ~1.06× legacy.

@pytest.mark.slow — needs the release binary; Darwin-only (Metal). ~70 s.
"""

from __future__ import annotations

import platform

import pytest

from test.e2e._multi_renderer_throughput import run_dual_renderer_gate
from test.e2e.runner import get_project_root

pytestmark = pytest.mark.skipif(
    platform.system() != "Darwin", reason="Metal backend is only available on macOS"
)

_CONFIGS_DIR = get_project_root() / "test" / "e2e" / "configs"
_TIMEOUT = 240


@pytest.mark.slow
def test_metal_dual_renderer_throughput(tmp_path):
    run_dual_renderer_gate(_CONFIGS_DIR, tmp_path, "metal", _TIMEOUT, "metal-dual-throughput")
