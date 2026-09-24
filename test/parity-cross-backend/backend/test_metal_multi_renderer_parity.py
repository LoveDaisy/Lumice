"""Per-renderer Metal-vs-legacy-CPU parity for multi-renderer sessions.

A Metal session serves every renderer of the config at once: the exit tail
projects each exit ray onto N device planes (one per renderer), accumulates N
landed-weight slots, and the host reads N planes back
(``MetalTraceBackend::Impl::AccumRendererPlanes``; ``doc/seam-design.md`` §4.2).
Before that, ``Simulator::CanUseBackend`` refused any config with more than one
renderer and the whole session ran on the legacy CPU path — so every existing
parity file, which runs one renderer, was structurally blind to the N-plane
path. This file is its coverage. Legacy CPU is the oracle, as everywhere in
this layer; the checks themselves live in ``test/e2e/_multi_renderer_parity.py``
(shared with the CUDA mirror), this file owns the scenes and the bars.

Two scenes, chosen for what they reach rather than for symmetry:

  * ``multi_lens`` — 3 renderers (linear / fisheye_equal_area /
    dual_fisheye_equal_area, all 256²), single wavelength (550 nm), 10M rays.
    A count strictly between 1 and the device cap (``kMaxRenderersDevice`` = 4),
    dense enough that the battery's 4×4 block ruler is informative, and the one
    scene here whose scalar ledger is a constant, so the two-ledger check runs
    at its tight bar.
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
    becomes possible, and it is a separate fixture so the throughput numbers
    keep the document they were measured on.

Bars (the checks are the shared module's (a)–(d)):

  * corr block and floor. The battery's 4×4 / 0.95 for ``multi_lens``. 16×16 /
    0.80 for the dual scene, where the 4×4 ruler reads 0.37 / 0.61 between two
    LEGACY seeds: at 16×16 legacy agrees with itself at 0.886–0.904 /
    0.959–0.963 over three seed pairs, and Metal against legacy reads
    0.883–0.887 / 0.959–0.962 over the same three pairs, i.e. indistinguishable
    from the oracle's own noise. A plane routed to the wrong renderer reads
    ~0 on either ruler (0.0000 measured, by writing every renderer's pixels
    into plane 0).
    RE-CALIBRATED 0.80 -> 0.65 when entry acceptance started keeping each ray
    dealt to a crystal with probability A/(S/2) (``doc/configuration.md``,
    ``proportion``). About half the dealt rays no longer enter, so at the same
    20M every plane got noisier and the oracle stopped agreeing with ITSELF at
    0.80: legacy against legacy now reads 0.775 / 0.794 / 0.769 on renderer[0]
    (0.909 / 0.917 / 0.910 on renderer[1]) over seed pairs 42-43 / 42-44 /
    43-44, and Metal against legacy 0.749 / 0.787 / 0.815 (0.904 / 0.911 /
    0.917) at seeds 42 / 43 / 44 — still indistinguishable from the oracle's
    own noise, so what went red was the floor, not the backend. 0.65 keeps the
    gap the old floor had under the oracle's worst self-agreement (about 0.09 then,
    0.12 now) and is still 0.65 above the wrong-plane break it exists for. The
    alternative, doubling the scene to 40M, was measured on the CUDA mirror and
    does not restore the old figure (0.863 against the historical 0.8885 there)
    while doubling the ~35 s single-worker legacy oracle on a CI leg that is
    already near the longest job; so the floor moved and the budget did not.
  * ledger tolerance, per scene:
      - single wavelength: 2%. Metal's ``R`` sits 0.27–0.47% below legacy's on
        every renderer of ``multi_lens`` — and by the same six digits in a
        single-renderer session of each renderer alone, and 0.38% on the
        battery's own ``parhelion`` row — so this is the Metal backend's
        baseline against legacy (whose scalar ledger is an fp32 running sum),
        not something the N-plane path adds. 2% is 4× above that baseline and
        12× below a merged slot (0.25 measured, by folding every slot into
        slot 0). The CUDA mirror uses the energy-accounting battery's 0.1%.
      - D65: 5%. The wavelength draw enters ``R`` (Metal samples per ray,
        legacy per batch), and the cross-backend ratio spreads ±1.8% over
        three seeds on the dual scene (0.987 / 1.018 / 0.995 and 0.990 /
        1.012 / 0.996). 5% is above that spread and 10× below a merge (0.39).

Post-fix residual of the single-wavelength gap (informational; the bars above
are unchanged by it). The 0.27–0.47% figures were read before PR #380 moved
the long-chain fp32 accumulators — legacy's scalar running sum among them —
to double. After that fix the same ratio, ``R_metal / R_legacy``, was
re-measured on the ``parhelion`` scene alone as a sweep over ``ray_num`` ∈
{1e6, 1e7, 1e8, 1e9}, three seeds each (seed spread ≤ 0.0008 pp):
  1e6 −0.021%, 1e7 +0.013%, 1e8 +0.020%, 1e9 +0.021%.
A good part of what had been attributed to Metal was legacy's own fp32
running sum overestimating. The shape settles the mechanism: the per-decade
increment shrinks about 5× per decade (+0.034 pp → +0.007 pp → +0.001 pp) and
the sign flips between 1e6 and 1e7 — a converging series, not the one-signed,
ever-more-negative divergence that a Metal-side ulp loss in the device-side
atomic XYZ accumulation would produce as N grows. That reads as
summation-order rounding — the two arms adding the same terms in a different
order — not energy being dropped. Extrapolating the last decade's
slope linearly to 1e11 rays (the GUI-reachable ceiling) gives ≈0.023%, about
0.00033 stop, some 300× below the 0.1 stop at which a brightness difference
would be worth acting on. Disposition: closed with no code change — neither
the 2% / 5% bars nor any accumulation path was touched. Scope of that
statement: it is one scene swept over N, not a re-measurement of
``multi_lens``'s 0.27–0.47% or the dual scene's readings, which stand as the
pre-PR #380 baseline they were.

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
PSNR and energy bars and the 4×4 corr floor are the battery's, not re-derived
here; the 16×16 floor and the ledger bars are calibrated above.

@pytest.mark.slow (shared-lib build: ``./scripts/build.sh -sj release``),
Darwin-only. Runs serially (capi_runner mutates os.environ + a process-global
log callback). Cost is the legacy oracle: ~10 s for ``multi_lens``, ~35 s for
the dual scene (a fixed seed pins legacy to one worker).
"""

from __future__ import annotations

import platform

import pytest

from test.e2e.capi_runner import BufferedSimResult, run_scene_capi_buffered
from test.e2e._multi_renderer_parity import Scene, check_multi_renderer_parity, lens_types
from test.e2e._parity_metrics import _DS_BH, _DS_BW
from test.e2e._projection_battery import T_RAW_CORR_DS
from test.e2e.runner import get_project_root

_SEED = 42
_TIMEOUT = 600

_CONFIGS_DIR = get_project_root() / "test" / "e2e" / "configs"

assert _DS_BH == _DS_BW, "the battery's block is square; the Scene rows assume one tile size"
_SCENES = [
    Scene("multi_lens", 3, block=_DS_BH, corr_floor=T_RAW_CORR_DS, ledger_tol=0.02),
    Scene("multi_renderer_parity_dual", 2, block=16, corr_floor=0.65, ledger_tol=0.05),
]

pytestmark = pytest.mark.skipif(
    platform.system() != "Darwin", reason="Metal backend is only available on macOS"
)


def _run(config_name: str, backend: str, num_renderers: int) -> BufferedSimResult:
    cfg = _CONFIGS_DIR / f"{config_name}.json"
    return run_scene_capi_buffered(
        str(cfg), sim_seed=_SEED, backend=backend, timeout_sec=_TIMEOUT, num_renderers=num_renderers
    )


@pytest.mark.slow
@pytest.mark.parametrize("scene", _SCENES, ids=lambda s: s.config)
def test_metal_multi_renderer_parity(scene: Scene):
    """Each renderer's Metal plane matches its legacy plane; each ledger pair agrees."""
    lens = lens_types(_CONFIGS_DIR, scene.config)
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
    check_multi_renderer_parity(scene, lens, legacy, metal, "metal")
