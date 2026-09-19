"""CUDA hit-budget parity: `max_hits` buys the same number of face interactions
on every backend.

`max_hits` counts every face a ray interacts with inside one crystal, the entry
face included. Legacy spends iteration 0 of its hit loop on the entry-face
`HitSurface` (`simulator.cpp`, the `for (i = 0; i < max_hits_; ...)` loop around
`TraceRayBasicInfo`) and Metal spends `hit == 0` of `trace_layer_kernel` on
`to_face = root_tf[tid]`, so on both a ray meets at most `max_hits - 1` interior
faces. The CUDA megakernel handles the entry face before its main loop, and that
loop must therefore run `max_hits - 1` interior iterations, not `max_hits`.

What this guards. The loop used to run `hit = 0 .. max_hits - 1` after the entry
block — one interior bounce more than the other two backends, whose refracted
exit is energy legacy never emits. The whole-image signature was small (+0.5%
total landed Y on `dual_fisheye_ref`, well inside the ±5% Monte-Carlo tolerance
the other CUDA parity files use), and it was path-selective: the
entry=3→exit=4 raypath bucket (adjacent prism sides; a 120° wedge has no direct
transmission, so every ray there carries internal reflections and the last
bounce weighs the most) read +2.2..2.8% across three seeds while the other five
buckets sat in −0.6..+2.0%. Sweeping `max_hits` isolated the stage: for every
`k` the old kernel at `max_hits = k` matched legacy at `max_hits = k + 1` to
Monte-Carlo noise (−0.4..+1.5%), and differed from legacy at the same `k` by
+814% (k=1), +49.6% (k=2), +0.06% (k=7).

The oracle is deterministic, not statistical. At `max_hits = 1` legacy can emit
exactly one thing per ray — the external reflection off the entry face — so any
raypath that passes *through* the crystal has energy identically 0 there, and
the entry=3→exit=5 `entry_exit` filter (the 22° halo path, one interior face)
selects nothing: `Y == 0` on both backends is a zero-vs-nonzero claim with no
sampling term. Revert the loop start to `0u` and the CUDA arm of that row reads
a large positive number. The `max_hits = 2` row is the other edge: the same
filter must now select the direct 3→5 transmission on both backends, so a
budget that over-corrected to `max_hits - 2` interior faces would read CUDA
`Y == 0` here and fail; the old kernel read +34% on this row (its second
interior face added the 3→5→…→5 reflected-then-exited family). The unfiltered
`max_hits = 1` row compares the two external-reflection-only images by total Y
under the ordinary Monte-Carlo tolerance; it is the row a reader can check by
eye (+822% before the fix at 500k rays, +814% at 2M).

Measured on the reference machine after the fix (500k rays, seed 42): the two
zero rows read exactly 0 on both backends, the `max_hits = 2` ratio 0.9960 and
the unfiltered `max_hits = 1` ratio 1.0059.

Scene: `dual_fisheye_ref` (single tumbling prism, D65, no scattering) with
`ray_num` and `max_hits` overridden through a temp config. 500k rays keeps each
arm under a few seconds; the zero rows do not depend on ray count at all.

Requires:
  - ``LUMICE_CUDA_ENABLED=ON`` build with the CUDA toolchain.
  - NVIDIA device visible to the runtime.
  - Shared-lib build produced by the CUDA-enabled Release configuration.

All tests are @pytest.mark.slow.
"""
from __future__ import annotations

import json
import os
import platform
from pathlib import Path

import pytest

from test.e2e.capi_runner import BufferedSimResult, run_scene_capi_buffered
from test.e2e.runner import get_project_root

CONFIGS_DIR = get_project_root() / "test" / "e2e" / "configs"
_SEED = 42
_TIMEOUT = 300
_RAY_NUM = 500_000
# Same Monte-Carlo bound the other CUDA parity files use for cuda/legacy total Y
# (two different samplers); the zero rows below use no tolerance at all.
_T_ENERGY_TOL = 0.05

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

# entry=3 → exit=5 under PBD symmetry: the whole C6/mirror family of the
# side→skip-one-side path (22° halo). One interior face at minimum, so it is
# unreachable at max_hits=1 and reachable (direct transmission) at max_hits=2.
_THROUGH_FILTER = {
    "id": 1,
    "type": "entry_exit",
    "entry": 3,
    "exit": 5,
    "symmetry": "PBD",
    "action": "filter_in",
}


def _write_config(tmp_path: Path, max_hits: int, with_filter: bool) -> str:
    data = json.loads((CONFIGS_DIR / "dual_fisheye_ref.json").read_text())
    data["scene"]["ray_num"] = _RAY_NUM
    data["scene"]["max_hits"] = max_hits
    if with_filter:
        data["filter"] = [_THROUGH_FILTER]
        data["scene"]["scattering"][0]["entries"][0]["filter"] = _THROUGH_FILTER["id"]
    path = tmp_path / f"dual_fisheye_ref_h{max_hits}_{'through' if with_filter else 'nofilter'}.json"
    path.write_text(json.dumps(data))
    return str(path)


def _run(cfg_path: str, backend: str) -> BufferedSimResult:
    r = run_scene_capi_buffered(cfg_path, sim_seed=_SEED, backend=backend, timeout_sec=_TIMEOUT)
    assert r.routed_backend == backend, (
        f"{Path(cfg_path).name}/{backend}: routed={r.routed_backend!r} (expected {backend!r}); "
        f"see log_lines for the actual path."
    )
    assert not r.fell_back, f"{Path(cfg_path).name}/{backend}: backend was requested but fell back."
    return r


def _landed_y(r: BufferedSimResult) -> float:
    return float(r.flt_buf[..., 1].sum())


@pytest.mark.slow
def test_hit_budget_one_leaves_no_through_path(tmp_path: Path):
    """max_hits=1 + entry=3→exit=5 filter: landed Y is exactly 0 on BOTH backends.

    Legacy proves the oracle: with one budget slot only the entry-face external
    reflection exists, whose face sequence is `[3]` and cannot end on face 5.
    A CUDA kernel that spends one more interior interaction than legacy emits
    the first-face refracted exit here and reads a large positive Y instead.
    """
    cfg = _write_config(tmp_path, max_hits=1, with_filter=True)
    legacy_y = _landed_y(_run(cfg, "legacy"))
    cuda_y = _landed_y(_run(cfg, "cuda"))
    print(f"[hit-budget] max_hits=1 through-filter: legacy_Y={legacy_y:.6g} cuda_Y={cuda_y:.6g}")
    assert legacy_y == 0.0, (
        f"legacy emitted Y={legacy_y:.6g} through a 3→5 path at max_hits=1; the oracle's premise "
        "(one budget slot = entry face only) no longer holds on the reference backend."
    )
    assert cuda_y == 0.0, (
        f"CUDA emitted Y={cuda_y:.6g} through a 3→5 path at max_hits=1 — the kernel is spending "
        "at least one interior interaction beyond the budget legacy/Metal honour."
    )


@pytest.mark.slow
def test_hit_budget_two_reaches_first_exit_on_both(tmp_path: Path):
    """max_hits=2 + entry=3→exit=5 filter: the direct transmission exists on both backends.

    Pins the other edge of the budget: a kernel that over-corrected to
    `max_hits - 2` interior faces would emit nothing here. The ratio is held to
    the ordinary Monte-Carlo tolerance because the two backends sample
    orientations independently.
    """
    cfg = _write_config(tmp_path, max_hits=2, with_filter=True)
    legacy_y = _landed_y(_run(cfg, "legacy"))
    cuda_y = _landed_y(_run(cfg, "cuda"))
    ratio = cuda_y / legacy_y if legacy_y > 0.0 else float("nan")
    print(f"[hit-budget] max_hits=2 through-filter: legacy_Y={legacy_y:.6g} cuda_Y={cuda_y:.6g} ratio={ratio:.4f}")
    assert legacy_y > 0.0, "legacy emitted nothing through 3→5 at max_hits=2; scene premise broken."
    assert cuda_y > 0.0, "CUDA emitted nothing through 3→5 at max_hits=2 — one interior face too few."
    assert abs(ratio - 1.0) <= _T_ENERGY_TOL, (
        f"max_hits=2 through-filter cuda/legacy Y ratio {ratio:.4f} outside [1 ± {_T_ENERGY_TOL}]."
    )


@pytest.mark.slow
def test_hit_budget_one_external_reflection_energy_matches(tmp_path: Path):
    """max_hits=1, no filter: both images are the entry-face external reflection alone.

    Total landed Y agrees to Monte-Carlo tolerance. Before the budget fix this
    row read +814% (CUDA added the whole first-face refracted exit).
    """
    cfg = _write_config(tmp_path, max_hits=1, with_filter=False)
    legacy_y = _landed_y(_run(cfg, "legacy"))
    cuda_y = _landed_y(_run(cfg, "cuda"))
    assert legacy_y > 0.0, "legacy emitted no external reflection at max_hits=1; scene premise broken."
    ratio = cuda_y / legacy_y
    print(f"[hit-budget] max_hits=1 no-filter: legacy_Y={legacy_y:.6g} cuda_Y={cuda_y:.6g} ratio={ratio:.4f}")
    assert abs(ratio - 1.0) <= _T_ENERGY_TOL, (
        f"max_hits=1 no-filter cuda/legacy Y ratio {ratio:.4f} outside [1 ± {_T_ENERGY_TOL}]: "
        "CUDA is emitting more than the entry-face external reflection with a one-slot budget."
    )
