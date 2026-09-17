"""A renderer change at CommitConfig must not leak the OLD renderer's projection into the new run.

What is at stake
----------------
On the legacy CPU route the simulator worker projects every batch itself, with the renderer
list the batch was generated under (``SimBatch::renders_``, captured together with the scene
under ``scene_mutex_``), and ``RenderConsumer::Consume`` accumulates the pre-projected pixels
without re-projecting. That binds a batch's projection to the renderer parameters EARLIER than
the consumer's own ``config_`` did — at generation, not at consumption — which is only an
improvement if a commit that changes the renderer really does retire every batch generated
under the old one before the new consumer sees anything. ``CommitConfig`` orders that as
``Stop()`` (both queues shut down, every worker joined) → consumers rebuilt → ``active_renders_``
updated, so no batch survives the boundary; this test is that argument made executable, so it
goes red on its own if the order ever changes.

The shape of the failure it would catch is not subtle: a stage-2 batch projected with stage 1's
renderer carries pixel indices for the wrong lens AND the wrong resolution, so the stage-2
picture either accumulates garbage or is missing whole batches. Either is a bit-level
difference from a fresh server that only ever saw stage 2.

Why a two-stage sequence on one server
--------------------------------------
``run_scene_sequence_capi_buffered`` commits stage A, waits until A's epoch is fully drained,
then commits stage B on the SAME server — one simulator pool, one consumer thread, one
lifetime — and copies out B. A fresh single-stage run of B is the reference. Under a fixed
non-zero ``sim_seed`` the server runs one worker and re-seeds it per session, so the two are
the same rays through the same projection and must agree exactly: ``==`` on the planes, not
a tolerance. The stages differ in lens family (linear → dual-fisheye with an overlap ring) and
in resolution, so a leaked projection cannot even index the right plane.

The sequence fixture itself insists on stage A being a reset-causing commit (it must mint a
new epoch), which a renderer change is; a stage pair that stopped being one would fail loudly
in the fixture, not silently here.

@pytest.mark.slow: drives the C API through the shared library (``./scripts/build.sh -sj``).
"""
from __future__ import annotations

import numpy as np
import pytest

from test.e2e.capi_runner import run_scene_capi_buffered, run_scene_sequence_capi_buffered
from test.e2e.runner import get_project_root

_CONFIG_DIR = get_project_root() / "test" / "e2e" / "configs"
_STAGE_A = str(_CONFIG_DIR / "worker_projection_commit_stage_a.json")
_STAGE_B = str(_CONFIG_DIR / "worker_projection_commit_stage_b.json")

_SEED = 42
_TIMEOUT = 600

pytestmark = [pytest.mark.slow]


@pytest.mark.slow
def test_stage_two_after_a_renderer_change_equals_a_fresh_run_of_stage_two() -> None:
    after_a = run_scene_sequence_capi_buffered(
        [_STAGE_A, _STAGE_B], sim_seed=_SEED, timeout_sec=_TIMEOUT, backend="legacy")
    fresh = run_scene_capi_buffered(_STAGE_B, sim_seed=_SEED, timeout_sec=_TIMEOUT, backend="legacy")

    assert after_a.routed_backend == "legacy" and fresh.routed_backend == "legacy", (
        f"this pins the legacy CPU route's worker-side projection; got "
        f"{after_a.routed_backend!r} / {fresh.routed_backend!r}")

    # The picture must actually be there — an empty pair would agree for the wrong reason.
    assert fresh.flt_buf.shape == (128, 256, 3), fresh.flt_buf.shape
    assert after_a.flt_buf.shape == fresh.flt_buf.shape
    lit = int(np.count_nonzero(fresh.flt_buf[:, :, 1]))
    assert lit > 500, f"stage B lit only {lit} pixels — the scene does not exercise the projection"

    differing = int(np.count_nonzero(after_a.flt_buf != fresh.flt_buf))
    assert differing == 0, (
        f"stage B after a stage-A commit differs from a fresh stage-B run at {differing} of "
        f"{fresh.flt_buf.size} floats — a batch generated under stage A's renderer reached "
        "stage B's consumer, or the run is no longer deterministic under a fixed seed")
    assert after_a.snapshot_intensity == fresh.snapshot_intensity
