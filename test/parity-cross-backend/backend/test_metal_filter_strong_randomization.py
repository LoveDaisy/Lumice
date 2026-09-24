"""Regression sentinel: entry_exit filter must not collapse on Metal under strong
shape randomization.

Guards the Metal per-instance GetFn-table fix. Before it, the Metal emit gate
remapped each ray's LOCAL polygon path to canonical face-numbers through a
single fixed-seed PROTOTYPE crystal's GetFn table, shared across the whole
K-shape pool. Its implicit "topology stable / any sampled instance suffices"
invariant is false under strong face_distance randomization: degenerate draws
drop lateral faces per instance, so the prototype's local->canonical mapping
mis-maps the actual traced instances. The `entry_exit 3->5` filtered output then
collapsed to EXACTLY zero nonzero pixels on Metal at gauss std >= 0.30, while
legacy CPU (which remaps on the live instance per record) stayed monotone. The
fix uploads a per-instance poly_fn table indexed by the ray's own polygon offset.

This test replays the ORIGINAL diagnosis scenario (the shared
`degenerate_pipeline_gaussian_std0NN.json` configs, entry_exit 3->5, 400k rays)
rather than a synthetic one, and pins two contracts across the std sweep:

  1. Collapse signature: Metal filtered output is never zero at any std. This is
     the crisp regression teeth -- the bug produced an exact 0.
  2. Cross-backend magnitude: Metal's nonzero-pixel count stays same-magnitude as
     legacy CPU. The count is a coverage ruler, not an energy one: it rises with
     how many rays are actually traced, not only with what the image converges
     to. Since the projected-area entry factor became one estimator family run
     per backend (CPU keeps a ray with probability A/(S/2), Metal traces every
     ray at that weight; lm_pcg::entry_acceptance), Metal's count reads 11-15%
     higher than it did when Metal also discarded, at every std, while its
     linear total is unchanged (std 0.4: 5796 vs 5797, fixed seed) and its
     16x16 block means moved 8% -- under CPU's own run-to-run 19%. The gap to
     CPU therefore now reads 11-25% instead of the earlier 1-8%; the band
     below is placed over that and still trips on a re-collapse (rel 1.0).
     Separately, and pre-dating that change, the K-shape pool granularity
     (CPU samples a fresh crystal per ray, the GPU pool reuses each crystal
     across a batch, and a fixed seed freezes which K shapes it holds) puts
     Metal's linear total above CPU's by more than the count shows: 1.05x at
     std 0.2 and 1.17-1.20x at std 0.4, the same on the build before the
     entry factor existed.

@pytest.mark.slow: uses the installed CLI binary (built by the shared-lib CI
phase). Darwin-only (Metal).
"""

import os
import platform
import subprocess

import numpy as np
import pytest
from PIL import Image

from test.e2e.runner import find_lumice_binary, get_project_root

pytestmark = pytest.mark.skipif(
    platform.system() != "Darwin", reason="Metal backend is only available on macOS"
)

_CONFIGS_DIR = get_project_root() / "test" / "e2e" / "configs"
_TIMEOUT = 180

# std sweep from the 397.1 diagnosis. std <= 0.25 agreed across backends before
# the fix; std >= 0.30 is where Metal collapsed to 0. Keep the full sweep so the
# test characterises the whole curve, not just the broken tail.
_STD_TAGS = ["020", "025", "030", "040", "050"]

# Cross-backend magnitude band. First measured post-fix at +0.3% (std=0) to
# +3.5% (std=0.4-0.5), band 0.15. Re-measured 2026-09-24 once Metal traced every
# ray at its projected-area entry weight while CPU keeps discarding (see the
# module docstring): Metal +10.6% / +12.2% / +18.0% / +22.6% / +22.3% over CPU at
# std 0.20 / 0.25 / 0.30 / 0.40 / 0.50 in one sweep, up to +25.4% in another
# (CPU's per-run noise on the count is ~1-2%). 0.35 sits ~10 points over the
# worst reading and far under a collapse (Metal 0 -> rel diff 1.0).
_MAGNITUDE_BAND = 0.35


def _run_nonzero_pixels(config_path, backend, workdir):
    """Run the CLI once for `config_path` on `backend` in `workdir` and return the
    number of nonzero pixels in the produced filtered image (img_01.jpg).

    backend: "cpu" (legacy, no env override) or "metal" (LUMICE_TRACE_BACKEND).
    """
    env = dict(os.environ)
    if backend == "metal":
        env["LUMICE_TRACE_BACKEND"] = "metal"
    else:
        env.pop("LUMICE_TRACE_BACKEND", None)  # legacy CPU is the unset default

    binary = find_lumice_binary()
    out_img = workdir / "img_01.jpg"
    if out_img.exists():
        out_img.unlink()
    proc = subprocess.run(
        [str(binary), "-f", str(config_path)],
        cwd=str(workdir),
        env=env,
        capture_output=True,
        text=True,
        timeout=_TIMEOUT,
    )
    assert proc.returncode == 0, (
        f"CLI ({backend}) exited {proc.returncode} for {config_path.name}\n"
        f"stderr tail:\n{proc.stderr[-2000:]}"
    )
    assert out_img.exists(), (
        f"CLI ({backend}) produced no img_01.jpg for {config_path.name}\n"
        f"stdout tail:\n{proc.stdout[-1000:]}"
    )
    with Image.open(out_img) as img:
        arr = np.asarray(img.convert("L"))
    return int(np.count_nonzero(arr))


@pytest.mark.slow
@pytest.mark.parametrize("std_tag", _STD_TAGS)
def test_metal_entry_exit_filter_no_collapse(std_tag, tmp_path):
    config = _CONFIGS_DIR / f"degenerate_pipeline_gaussian_std{std_tag}.json"
    assert config.exists(), f"missing fixture {config}"

    cpu_nnz = _run_nonzero_pixels(config, "cpu", tmp_path)
    metal_nnz = _run_nonzero_pixels(config, "metal", tmp_path)

    std = int(std_tag) / 100.0
    # CPU sanity: the entry_exit 3->5 filter always leaves a substantial pattern.
    assert cpu_nnz > 1000, f"std={std}: CPU filtered output unexpectedly sparse ({cpu_nnz})"

    # (1) Collapse signature: the bug produced EXACTLY 0 on Metal at std >= 0.30.
    assert metal_nnz > 0, (
        f"std={std}: Metal filtered output collapsed to 0 nonzero pixels while CPU "
        f"has {cpu_nnz} -- the per-instance GetFn-table regression has returned."
    )

    # (2) Cross-backend magnitude: same order, within the K-granularity + noise band.
    rel = abs(metal_nnz - cpu_nnz) / cpu_nnz
    assert rel <= _MAGNITUDE_BAND, (
        f"std={std}: Metal nnz {metal_nnz} vs CPU nnz {cpu_nnz} differ by "
        f"{rel:.1%} > {_MAGNITUDE_BAND:.0%} band -- filter match no longer tracks CPU."
    )
