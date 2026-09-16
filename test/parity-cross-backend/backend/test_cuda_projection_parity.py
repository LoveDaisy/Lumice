"""Per-projection CUDA-vs-legacy-CPU parity battery (scrum-gpu-projection-parity 315.5).

CUDA mirror of ``test_metal_projection_parity.py``. Same 11-type coverage, same
metrics (block-mean ds_corr + total-Y energy + cross-seed self-consistency), same
no-fallback assertion, same tolerances (all imported from
``test/e2e/_projection_battery.py`` so Metal and CUDA stay in lockstep).

After 315.2–315.4 the CUDA exit tail projects via the shared
``lm_proj::ProjectExitToPixel`` (verified on dev49 by the main session — Mac
cannot compile CUDA) and ``CudaTraceBackend::IsCompatible`` accepts every lens
type. This suite pins per-projection parity vs legacy CPU + confirms every type
routes to CUDA with no silent fallback.

Gating: CUDA is only available on a ``LUMICE_CUDA_ENABLED=ON`` build with an
NVIDIA device. Skipped unless Linux/Windows AND
``LUMICE_HAS_CUDA=1`` is set — the same gate as ``test_cuda_exit_seam_parity.py``.
On Mac this whole module is skipped at collection. All tests @pytest.mark.slow.
"""

from __future__ import annotations

import json
import math
import os
import platform
import subprocess
import sys

import pytest

from test.e2e.capi_runner import BufferedSimResult, run_scene_capi_buffered
from test.e2e._parity_metrics import _raw_corr_ds as _raw_corr_ds_impl, _DS_BH, _DS_BW
from test.e2e._projection_battery import (
    PROJECTION_TYPES,
    T_ENERGY_TOL,
    T_PSNR_DB,
    T_RAW_CORR_DS,
    T_SELF_MARGIN,
    write_projection_config,
)
from test.e2e.runner import get_project_root

_SEED = 42
_SEED_B = 7
_TIMEOUT = 300  # CUDA dev49 docker first-launch JIT can take a minute

# Same availability gate as test_cuda_exit_seam_parity.py: Linux/Windows +
# explicit LUMICE_HAS_CUDA=1 opt-in (set it in the CUDA run environment). On any
# other host the CUDA backend falls back to legacy and routed_backend would come
# out "legacy" → the parity assertions would be false positives.
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


# --------------------------------------------------------------------------- #
# Fixtures / helpers
# --------------------------------------------------------------------------- #

@pytest.fixture(scope="module")
def _proj_configs(tmp_path_factory) -> dict:
    out_dir = tmp_path_factory.mktemp("proj_parity_cuda")
    return {t: write_projection_config(t, out_dir) for t in PROJECTION_TYPES}


def _raw_corr_ds(a: BufferedSimResult, b: BufferedSimResult) -> float:
    return _raw_corr_ds_impl(a, b, _DS_BH, _DS_BW)


def _render_psnr(a: BufferedSimResult, b: BufferedSimResult) -> float:
    if a.rgb_buf.shape != b.rgb_buf.shape:
        raise ValueError(f"render shape mismatch: {a.rgb_buf.shape} vs {b.rgb_buf.shape}")
    diff = a.rgb_buf.astype(float) - b.rgb_buf.astype(float)
    mse = float((diff * diff).mean())
    if mse == 0.0:
        return float("inf")
    return 10.0 * math.log10((255.0 * 255.0) / mse)


def _run(cfg_path, backend: str, seed: int = _SEED) -> BufferedSimResult:
    return run_scene_capi_buffered(str(cfg_path), sim_seed=seed, backend=backend, timeout_sec=_TIMEOUT)


def _assert_routed_cuda(r: BufferedSimResult, lens_type: str) -> None:
    """No-fallback assertion (scrum success criterion): the type routed to CUDA."""
    assert r.routed_backend == "cuda", (
        f"{lens_type}: routed={r.routed_backend!r} (expected 'cuda'); "
        f"LUMICE_GetActiveBackend did not report CUDA. log tail: {r.log_lines[-5:]}"
    )
    assert not r.fell_back, (
        f"{lens_type}: CUDA was requested but fell back to legacy CPU - the "
        f"projection is NOT actually accelerated. log tail: {r.log_lines[-5:]}"
    )


# --------------------------------------------------------------------------- #
# Per-projection parity — parametrized over all 11 LensParam::LensType values.
# --------------------------------------------------------------------------- #

@pytest.mark.slow
@pytest.mark.parametrize("lens_type", PROJECTION_TYPES)
def test_cuda_projection_parity(lens_type: str, _proj_configs):
    """CUDA-vs-legacy parity + no-fallback for one projection type."""
    cfg = _proj_configs[lens_type]

    legacy_a = _run(cfg, "legacy", seed=_SEED)
    cuda_a = _run(cfg, "cuda", seed=_SEED)

    assert legacy_a.routed_backend == "legacy" and not legacy_a.fell_back, (
        f"{lens_type}: legacy oracle routed={legacy_a.routed_backend!r} "
        f"fell_back={legacy_a.fell_back} - env pollution suspected."
    )
    _assert_routed_cuda(cuda_a, lens_type)

    corr = _raw_corr_ds(cuda_a, legacy_a)
    psnr = _render_psnr(cuda_a, legacy_a)

    cuda_y = float(cuda_a.flt_buf[..., 1].sum())
    legacy_y = float(legacy_a.flt_buf[..., 1].sum())
    assert legacy_y > 0.0, f"{lens_type}: legacy total Y == 0 - scene carries no signal"
    energy_ratio = cuda_y / legacy_y

    cuda_b = _run(cfg, "cuda", seed=_SEED_B)
    legacy_b = _run(cfg, "legacy", seed=_SEED_B)
    _assert_routed_cuda(cuda_b, lens_type)
    cuda_self = _raw_corr_ds(cuda_a, cuda_b)
    legacy_self = _raw_corr_ds(legacy_a, legacy_b)

    print(
        f"[proj-parity] {lens_type}: ds_corr={corr:.4f} psnr={psnr:.2f}dB "
        f"energy_ratio={energy_ratio:.4f} cuda_self={cuda_self:.4f} "
        f"legacy_self={legacy_self:.4f}"
    )

    assert corr >= T_RAW_CORR_DS, (
        f"{lens_type}: ds_corr {corr:.4f} < {T_RAW_CORR_DS}. The shared "
        f"ProjectExitToPixel path diverged from the CPU oracle for this projection."
    )
    assert psnr >= T_PSNR_DB, f"{lens_type}: render PSNR {psnr:.2f} dB < {T_PSNR_DB}"
    assert abs(energy_ratio - 1.0) <= T_ENERGY_TOL, (
        f"{lens_type}: cuda/legacy total-Y ratio {energy_ratio:.4f} outside "
        f"[1 +/- {T_ENERGY_TOL}] - global energy imbalance in the exit/emit path."
    )
    assert cuda_self >= legacy_self - T_SELF_MARGIN, (
        f"{lens_type}: cuda cross-seed self-consistency {cuda_self:.4f} < "
        f"legacy_self {legacy_self:.4f} - {T_SELF_MARGIN} - suspect PCG stream "
        f"collapse / orientation undersampling in the CUDA path for this projection."
    )


# --------------------------------------------------------------------------- #
# Negative smoke — verify the no-fallback detector has teeth.
# --------------------------------------------------------------------------- #

@pytest.mark.slow
def test_cuda_projection_no_fallback_detector():
    """A CUDA request the process cannot honour MUST read back as a different route.

    Lens type is no longer a fallback trigger (315.3/315.4 relaxed IsCompatible);
    neither is renderer count any more: a CUDA session serves N renderers at once
    (one device plane each), and the only count it refuses (more than
    kMaxRenderersDeviceCuda = 4) is the count the C API already rejects at parse
    time (LUMICE_MAX_CONFIG_RENDERERS = 4), so no config that reaches the
    simulator makes CUDA fall back on this host. What still fires is the routing
    layer's own refusal when the requested device is not there ("requested but
    no eligible CUDA device; falling back to legacy CPU"): the run executes on
    legacy, and LUMICE_GetActiveBackend — the same call the per-projection
    ``_assert_routed_cuda`` checks read — says so. `fell_back` stays False and
    is pinned as such: the refusal sizes the server for the CPU route, and the
    product's flag (LUMICE_GetBackendFallbackFlag) means "a GPU-sized route lost
    its backend", which no legal config can trigger today — its teeth are shown
    by breaking a CanUseBackend gate in source, not by a fixture.
    The device probe is cached once per process and the CUDA runtime reads
    CUDA_VISIBLE_DEVICES at initialisation, so the refusal has to be provoked in
    a child interpreter with the device hidden; the child runs the very same
    capi_runner path and hands back its parsed verdict.
    """
    cfg = get_project_root() / "test" / "e2e" / "configs" / "halo_22.json"
    child = (
        "import json, sys\n"
        "from test.e2e.capi_runner import run_scene_capi_buffered\n"
        f"r = run_scene_capi_buffered({str(cfg)!r}, sim_seed={_SEED}, backend='cuda', timeout_sec={_TIMEOUT})\n"
        "print('VERDICT ' + json.dumps({'routed': r.routed_backend, 'fell_back': r.fell_back, "
        "'tail': list(r.log_lines)[-5:]}))\n"
    )
    env = dict(os.environ)
    env["CUDA_VISIBLE_DEVICES"] = ""
    proc = subprocess.run(
        [sys.executable, "-c", child], cwd=str(get_project_root()), env=env,
        capture_output=True, text=True, timeout=_TIMEOUT + 60, check=False,
    )
    verdict_lines = [ln for ln in proc.stdout.splitlines() if ln.startswith("VERDICT ")]
    assert proc.returncode == 0 and len(verdict_lines) == 1, (
        f"child interpreter did not produce a verdict (rc={proc.returncode}). "
        f"stdout tail: {proc.stdout[-800:]!r} stderr tail: {proc.stderr[-800:]!r}"
    )
    verdict = json.loads(verdict_lines[0][len("VERDICT "):])
    assert verdict["routed"] == "legacy" and not verdict["fell_back"], (
        "Expected the routing layer to refuse a CUDA request with CUDA_VISIBLE_DEVICES='' and "
        f"run legacy (not a fallback: the server was sized for the CPU route), but got "
        f"{verdict}. The no-fallback detector may be broken."
    )
