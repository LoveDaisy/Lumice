"""CUDA energy-accounting parity: the two ledgers of one backend must agree.

Every backend keeps two independent tallies of the energy that landed on the
image:

  * the per-pixel XYZ buffer (``flt_buf``), accumulated pixel by pixel from
    ``kCmfY[wl] * w_exit`` — this is what the image is made of; and
  * ``snapshot_intensity``, a single scalar summed from the same exit weights
    (``landed_weight`` → ``total_intensity_``) and then divided by the
    normalization ``kNormScale * total_pix`` in the renderer — this is what
    the renderer exposes through the C API and what exposure math reads.

For a single-wavelength scene the two are tied by a constant that does not
depend on the rays at all:

    R = sum(flt_buf[..., Y]) / snapshot_intensity = kCmfY[wl] * kNormScale * total_pix

so ``R`` is deterministic up to float accumulation order, and ``R_cuda /
R_legacy`` measures whether the CUDA backend's *own* two ledgers stay in step
with each other the way legacy's do. This is a different invariant from the
``energy_ratio`` the other three CUDA parity files assert: ``energy_ratio``
compares one ledger across the two backends and tolerates the ±5% Monte-Carlo
divergence two different samplers legitimately produce; ``R`` compares the two
ledgers *within* each backend, where there is no sampling term to hide behind,
which is why its tolerance is two orders of magnitude tighter.

What this guards. The CUDA backend once accumulated ``landed_weight`` as a
single fp32 device scalar with one ``atomicAdd`` per exit, alive across the
whole drain window (64 batches × 262144 rays). Once the running sum reached
~2e6 its ulp was 0.125–0.25, and every exit weight below half an ulp was
dropped whole — a systematic under-count that the per-pixel buffer (each pixel
summing to ~15) never suffered. The two ledgers diverged by a deterministic
+1.74% on ``R`` while every image-side parity metric stayed green, because the
image ledger was the correct one; only the scalar the exposure pipeline reads
was wrong. The fix reduces per warp in registers and folds per layer into a
host ``double``. No parity test read ``snapshot_intensity`` at the time, so
the defect had no machine signal for as long as it lived; this file is that
signal. Revert the fix and the ``cpu_backend_route`` rows below read ~+1.7%
against a 0.1% tolerance.

The same defect shape came back one level down once the scalar was fixed.
The per-pixel XYZ plane is itself an fp32 device accumulator with one
``atomicAdd`` per exit, and on the third-clock drain path it too stays alive
for the whole window. For most pixels that chain is short, but a narrow-field
scene concentrates exits: on ``parhelion`` (fisheye 120°, 10M rays) the
plane's Y sum read +0.40% above legacy's at the default 64-batch window, and
the offset moved with the window length (16 batches → −0.135%, 4 → −0.039%,
1 → −0.008%) — rounding drift whose sign flips with chain length, not a
one-way loss. It was invisible while both backends' host-side long chains were
fp32 too (the two roundings cancelled on ``R``); widening legacy's
``internal_xyz_`` / ``total_intensity_`` to ``double`` removed one side of the
cancellation and the device plane's own drift showed up as four red rows here
and in ``test_cuda_multi_renderer_parity.py``. The fix makes the device plane
``double`` (``AccumXyzToPixel``'s CUDA variant is a 64-bit ``atomicAdd``), with
the drain converting to fp32 on device before the unchanged D2H; a per-batch
fold of an fp32 plane into a double one was measured correct but too slow
(``accum_shared.h`` records the alternatives).
``test_cuda_energy_ledger_independent_of_drain_window`` below is that
mechanism's direct check: the ratio must not move when the window length does.

Tolerance. After the fix the cross-backend spread of ``R`` on the rows below
is −0.004% or better at 2M rays and −0.029% on the 10M-ray ``parhelion`` rows
(stable to 0.002% across seeds — a fixed offset, not noise). The tolerance is
0.1%: ~3.4× above the worst measured spread, ≥10× below the smallest red-state
signature the fix's revert produces on the rows that can see it (+1.74% on
``cpu_backend_route``, −1.04% on ``parhelion``). It is deliberately not
tighter. When it was set, legacy's own ``R`` drifted above the closed-form
constant with ray count (+0.019% at 2M, +0.38% at 10M) because its
``total_intensity_`` was itself an fp32 running sum; that accumulator (with
``internal_xyz_`` beside it — the same rounding turned a monochromatic
sub-sun's hue with ray count) has since been widened to ``double`` and the
drift is gone, but the tolerance was not re-derived from the tighter spread
that leaves: the row compares the two backends' ledger ratios against each
other, not either one against the constant, and nothing measured since has
argued for spending the margin.

Not every row reddens on the revert. The defect scales with how much weight
the window accumulates, so the 400k-ray and 20k-ray rows read +0.003% and
+0.043% under the old code and stay green; they are here for the session
shapes they cover (filter + random geometry; per-layer fold), and the 2M/10M
rows are the ones that carry the red.

Scene selection. Only single-wavelength configs: under a D65 spectrum ``R``
becomes ``sum(cmf_y * w) / sum(w)``, which varies ~1% between seeds on legacy
alone (it draws one wavelength per batch), so the same ratio measured there
cannot resolve a sub-percent accounting error. The four configs cover the
four session shapes the CUDA backend has (single-MS no filter; fisheye 120°
view where most exits fall outside the frame; single-MS with a filter and
random geometry; two-layer MS with five crystals) — the per-layer fold is
only exercised by the last one.

Requires:
  - ``LUMICE_CUDA_ENABLED=ON`` build with the CUDA toolchain.
  - NVIDIA device visible to the runtime.
  - Shared-lib build produced by the CUDA-enabled Release configuration
    (a plain ``./scripts/build.sh -sj release`` is not enough on its own).

All tests are @pytest.mark.slow.
"""
from __future__ import annotations

import math
import os
import platform

import pytest

from test.e2e.capi_runner import BufferedSimResult, run_scene_capi_buffered
from test.e2e.runner import get_project_root

CONFIGS_DIR = get_project_root() / "test" / "e2e" / "configs"
_TIMEOUT = 900  # parhelion.json is 10M rays; the legacy arm is the slow one

# |R_cuda / R_legacy - 1| bound. See the module docstring for the derivation
# (measured post-fix spread ≤ 0.03%, revert signature ≥ 1.0% on the 2M/10M rows).
_T_R_RATIO_TOL = 0.001

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

# (config, seeds). Every entry is a single-wavelength scene (see docstring);
# adding a D65 config here would make the tolerance meaningless, not stricter.
_R_RATIO_CASES = (
    ("cpu_backend_route", (42, 43, 44)),            # 555 nm, rectangular 180°, 2M rays
    ("parhelion", (42, 43, 44)),                    # 550 nm, fisheye_equal_area 120°, 10M rays
    ("parity_random_geometry", (42, 43)),           # 550 nm, filter + random geometry
    ("orientation_sample_count_random", (42, 43)),  # 540 nm, 5 crystals, two MS layers
)
_R_RATIO_PARAMS = [(cfg, seed) for cfg, seeds in _R_RATIO_CASES for seed in seeds]
_R_RATIO_IDS = [f"{cfg}-seed{seed}" for cfg, seed in _R_RATIO_PARAMS]


def _run(config_name: str, backend: str, seed: int) -> BufferedSimResult:
    cfg = str(CONFIGS_DIR / f"{config_name}.json")
    return run_scene_capi_buffered(cfg, sim_seed=seed, backend=backend, timeout_sec=_TIMEOUT)


def _assert_routed(r: BufferedSimResult, expected: str, config_name: str) -> None:
    assert r.routed_backend == expected, (
        f"{config_name}/{expected}: routed={r.routed_backend!r} (expected {expected!r}); "
        f"see log_lines for the actual path."
    )
    assert not r.fell_back, (
        f"{config_name}/{expected}: fell back to legacy. Backend was REQUESTED but did not run."
    )


def _y_sum(r: BufferedSimResult) -> float:
    return float(r.flt_buf[..., 1].sum())


def _r_ratio(r: BufferedSimResult) -> float:
    """``sum(flt_buf Y) / snapshot_intensity``; nan when the scalar ledger is empty.

    Returning nan rather than raising keeps the diagnosis in the assertion
    message below, next to the other side's numbers, instead of in a traceback
    that shows only one backend.
    """
    if r.snapshot_intensity == 0.0:
        return float("nan")
    return _y_sum(r) / float(r.snapshot_intensity)


@pytest.mark.slow
@pytest.mark.parametrize(("config", "seed"), _R_RATIO_PARAMS, ids=_R_RATIO_IDS)
def test_cuda_energy_ledger_matches_legacy(config: str, seed: int):
    """|R_cuda / R_legacy − 1| ≤ 0.1% on a single-wavelength scene.

    Both arms run the same config and seed; the ratio is formed from each
    backend's own two ledgers, so a red here means one backend's scalar and
    per-pixel accounting have come apart — not that the two backends sampled
    different rays.
    """
    legacy = _run(config, "legacy", seed)
    cuda = _run(config, "cuda", seed)

    _assert_routed(legacy, "legacy", config)
    _assert_routed(cuda, "cuda", config)

    r_legacy = _r_ratio(legacy)
    r_cuda = _r_ratio(cuda)

    detail = (
        f"legacy: Ysum={_y_sum(legacy):.6g} snapshot_intensity={legacy.snapshot_intensity:.6g} "
        f"R={r_legacy:.6f}; "
        f"cuda: Ysum={_y_sum(cuda):.6g} snapshot_intensity={cuda.snapshot_intensity:.6g} "
        f"R={r_cuda:.6f}"
    )

    assert not math.isnan(r_legacy) and r_legacy > 0.0, (
        f"{config}/seed{seed}: legacy R is not a positive number ({detail}); "
        "the reference arm landed nothing, so no ratio can be formed."
    )
    assert not math.isnan(r_cuda) and r_cuda > 0.0, (
        f"{config}/seed{seed}: cuda R is not a positive number ({detail}); "
        "the cuda scalar ledger is empty while its image ledger is being compared."
    )

    ratio = r_cuda / r_legacy
    print(
        f"[energy-ledger] {config}/seed{seed}: R_cuda/R_legacy={ratio:.6f} "
        f"({(ratio - 1.0) * 100:+.4f}%, tol +/-{_T_R_RATIO_TOL * 100:.2f}%) — {detail}"
    )
    assert abs(ratio - 1.0) <= _T_R_RATIO_TOL, (
        f"{config}/seed{seed}: R_cuda/R_legacy = {ratio:.6f} "
        f"({(ratio - 1.0) * 100:+.4f}%) outside [1 +/- {_T_R_RATIO_TOL}]. {detail}. "
        "The cuda backend's scalar ledger (snapshot_intensity) and per-pixel ledger "
        "(flt_buf Y) have come apart. Suspect landed_weight accumulation: a single fp32 "
        "device scalar summed across the whole drain window drops sub-ulp exit weights "
        "and reads ~+1.7% here."
    )


# Drain-window lengths for the invariance check: the shipped default (64,
# Simulator::kDefaultXyzDrainBatches), the middle of the dose-response table
# and the shortest window the third clock allows. Set through
# LUMICE_XYZ_DRAIN_BATCHES, which Simulator::Run re-reads on every Run() (the
# env knob logs once but resolves every time), so one process can sweep it.
_DRAIN_WINDOW_BATCHES = (1, 16, 64)
# Spread bound on R_cuda/R_legacy across the three windows. Tighter than
# _T_R_RATIO_TOL because this compares one backend against itself at three
# settings whose only difference is how often the double plane is drained
# (measured post-fix spread 0.0000%), not two backends against each other; the
# fp32 plane reads a 0.54% spread on this scene (-0.008% / -0.135% / +0.400%).
_T_DRAIN_WINDOW_SPREAD = 0.0005
_DRAIN_WINDOW_CONFIG = "parhelion"
_DRAIN_WINDOW_SEED = 42


@pytest.mark.slow
def test_cuda_energy_ledger_independent_of_drain_window(monkeypatch: pytest.MonkeyPatch):
    """R_cuda / R_legacy agrees across LUMICE_XYZ_DRAIN_BATCHES ∈ {1, 16, 64} to ≤0.05%.

    The legacy arm runs once (it has no drain window); the cuda arm runs once
    per window length on the same seed. With the double plane the drain
    cadence only changes when the total is copied out, never how it is summed,
    so the three ratios must agree; an fp32 plane walks with the window (+0.40%
    at 64 vs −0.008% at 1 on this scene), which is the signature this test
    exists to catch.
    """
    legacy = _run(_DRAIN_WINDOW_CONFIG, "legacy", _DRAIN_WINDOW_SEED)
    _assert_routed(legacy, "legacy", _DRAIN_WINDOW_CONFIG)
    r_legacy = _r_ratio(legacy)
    assert not math.isnan(r_legacy) and r_legacy > 0.0, (
        f"{_DRAIN_WINDOW_CONFIG}: legacy R is not a positive number "
        f"(Ysum={_y_sum(legacy):.6g} snapshot_intensity={legacy.snapshot_intensity:.6g})"
    )

    ratios = {}
    for batches in _DRAIN_WINDOW_BATCHES:
        monkeypatch.setenv("LUMICE_XYZ_DRAIN_BATCHES", str(batches))
        cuda = _run(_DRAIN_WINDOW_CONFIG, "cuda", _DRAIN_WINDOW_SEED)
        _assert_routed(cuda, "cuda", _DRAIN_WINDOW_CONFIG)
        r_cuda = _r_ratio(cuda)
        assert not math.isnan(r_cuda) and r_cuda > 0.0, (
            f"{_DRAIN_WINDOW_CONFIG}/drain={batches}: cuda R is not a positive number "
            f"(Ysum={_y_sum(cuda):.6g} snapshot_intensity={cuda.snapshot_intensity:.6g})"
        )
        ratios[batches] = r_cuda / r_legacy
        print(
            f"[drain-window] {_DRAIN_WINDOW_CONFIG}/seed{_DRAIN_WINDOW_SEED} "
            f"LUMICE_XYZ_DRAIN_BATCHES={batches}: R_cuda/R_legacy={ratios[batches]:.6f} "
            f"({(ratios[batches] - 1.0) * 100:+.4f}%) — cuda Ysum={_y_sum(cuda):.6g} "
            f"snapshot_intensity={cuda.snapshot_intensity:.6g}"
        )

    spread = max(ratios.values()) - min(ratios.values())
    detail = ", ".join(f"{b}: {r:.6f}" for b, r in ratios.items())
    print(
        f"[drain-window] {_DRAIN_WINDOW_CONFIG}/seed{_DRAIN_WINDOW_SEED}: spread={spread * 100:.4f}% "
        f"(tol {_T_DRAIN_WINDOW_SPREAD * 100:.2f}%) — {detail}"
    )
    assert spread <= _T_DRAIN_WINDOW_SPREAD, (
        f"{_DRAIN_WINDOW_CONFIG}/seed{_DRAIN_WINDOW_SEED}: R_cuda/R_legacy moves with the drain "
        f"window (spread {spread * 100:.4f}% > {_T_DRAIN_WINDOW_SPREAD * 100:.2f}%): {detail}. "
        "The device XYZ plane is summing in fp32 across the window again — check that "
        "AccumXyzToPixel's CUDA variant still targets the double plane (d_xyz_acc_) and that "
        "ReadbackXyzAccum merges it through xyz_plane_to_float_kernel."
    )
