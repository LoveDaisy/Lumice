"""Per-renderer CUDA-vs-legacy-CPU parity for multi-renderer sessions.

The CUDA mirror of ``test_metal_multi_renderer_parity.py``: a CUDA session
serves every renderer of the config at once (``EmitToDeviceXyz`` loops the N
``RendererPlaneDesc`` rows, one device plane + one ``landed_acc`` slot per
renderer, reduced per warp and per renderer in the epilogue), where before the
whole session fell back to the legacy CPU path. The checks are the shared
module's (``test/e2e/_multi_renderer_parity.py``); this file owns the CUDA
skip gate, the ``backend="cuda"`` arm, and the bars.

Scenes are the Metal file's — read its docstring for why these two and why the
dual scene is a quarter-resolution, 20M-ray derivative of the user's document
rather than the throughput fixture itself. The corr block and floor are
carried over from it as well (16×16 / 0.80 on the dual scene), on the grounds
that both backends project through the same ``lm_proj::ProjectExitToPixel``
and read the same oracle; the Metal readings there sit at the oracle's own
seed-to-seed noise, and CUDA's are expected to as well. The ledger bars are
this backend's own:

  * single wavelength: the energy-accounting battery's 0.1%
    (``test_cuda_energy_accounting_parity.py``). The CUDA backend's two
    ledgers agree with legacy's to ≤0.03% on ``multi_lens`` (measured by the
    port's own per-renderer verification), unlike Metal's, which sits ~0.4%
    below — so the tight bar applies here and not there.
  * D65: 5%, as in the Metal file: the wavelength draw enters ``R``, and the
    bar sits above the seed-to-seed spread and 10× below a merged slot.

Measured on the CUDA reference machine (RTX 5090 D, seed 42 both arms):
  multi_lens                 corr(4×4) 0.9995 / 1.0000 / 1.0000, PSNR 40.1 /
                             40.8 / 47.3 dB, energy 1.0018 / 1.0047 / 1.0055,
                             R ratio 0.99994 / 0.99969 / 1.00001
  multi_renderer_parity_dual corr(16×16) 0.8885 / 0.9610, PSNR 20.4 / 17.8 dB,
                             energy 0.9893 / 0.9885, R ratio 0.9813 / 0.9917
The dual scene's corr reads at the oracle's own seed-to-seed level (the Metal
file's 0.886–0.904 / 0.959–0.963), as expected for a shared projection.

Those readings predate entry acceptance (a ray dealt to a crystal is kept with
probability A/(S/2)), which left about half as many rays entering at the same
20M. Re-measured on the same machine afterwards: legacy against legacy 0.775 /
0.780 / 0.783 on renderer[0] (0.909 / 0.917 / 0.911 on renderer[1]) over seed
pairs 42-43 / 42-44 / 43-44, CUDA against legacy 0.790 / 0.755 / 0.778 (0.907 /
0.919 / 0.900) at seeds 42 / 43 / 44 — at the oracle's own level again, under
the old 0.80 floor on both sides. The floor was re-calibrated to 0.65 in the
Metal file (see its docstring for the reasoning, including why the 40M
alternative was not taken) and carried over here as before.

Requires (same gate as ``test_cuda_projection_parity.py``): Linux/Windows,
``LUMICE_HAS_CUDA=1``, a ``LUMICE_CUDA_ENABLED=ON`` shared-lib build and an
NVIDIA device. @pytest.mark.slow; runs serially.
"""

from __future__ import annotations

import os
import platform

import pytest

from test.e2e.capi_runner import BufferedSimResult, run_scene_capi_buffered
from test.e2e._multi_renderer_parity import Scene, check_multi_renderer_parity, lens_types
from test.e2e._parity_metrics import _DS_BH, _DS_BW
from test.e2e._projection_battery import T_RAW_CORR_DS
from test.e2e.runner import get_project_root

_SEED = 42
_TIMEOUT = 900  # CUDA first-launch JIT can take a minute; the legacy oracle ~35 s

_CONFIGS_DIR = get_project_root() / "test" / "e2e" / "configs"

assert _DS_BH == _DS_BW, "the battery's block is square; the Scene rows assume one tile size"
_SCENES = [
    Scene("multi_lens", 3, block=_DS_BH, corr_floor=T_RAW_CORR_DS, ledger_tol=0.001),
    Scene("multi_renderer_parity_dual", 2, block=16, corr_floor=0.65, ledger_tol=0.05),
]

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


def _run(config_name: str, backend: str, num_renderers: int) -> BufferedSimResult:
    cfg = _CONFIGS_DIR / f"{config_name}.json"
    return run_scene_capi_buffered(
        str(cfg), sim_seed=_SEED, backend=backend, timeout_sec=_TIMEOUT, num_renderers=num_renderers
    )


@pytest.mark.slow
@pytest.mark.parametrize("scene", _SCENES, ids=lambda s: s.config)
def test_cuda_multi_renderer_parity(scene: Scene):
    """Each renderer's CUDA plane matches its legacy plane; each ledger pair agrees."""
    lens = lens_types(_CONFIGS_DIR, scene.config)
    legacy = _run(scene.config, "legacy", scene.num_renderers)
    cuda = _run(scene.config, "cuda", scene.num_renderers)

    assert legacy.routed_backend == "legacy" and not legacy.fell_back, (
        f"{scene.config}: legacy oracle routed={legacy.routed_backend!r} "
        f"fell_back={legacy.fell_back} - env pollution suspected."
    )
    assert cuda.routed_backend == "cuda", (
        f"{scene.config}: routed={cuda.routed_backend!r} (expected 'cuda'); "
        f"LUMICE_GetActiveBackend did not report CUDA. log tail: {cuda.log_lines[-5:]}"
    )
    assert not cuda.fell_back, (
        f"{scene.config}: CUDA was requested for a {scene.num_renderers}-renderer config and "
        f"fell back to legacy CPU - the multi-renderer session is NOT on device. "
        f"log tail: {cuda.log_lines[-5:]}"
    )
    check_multi_renderer_parity(scene, lens, legacy, cuda, "cuda")
