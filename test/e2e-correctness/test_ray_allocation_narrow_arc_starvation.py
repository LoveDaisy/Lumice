"""E2E test: adaptive ray allocation does not starve a filtered entry's narrow arc.

The failure shape this guards: `scene.ray_allocation = "adaptive"` deals rays by
Neyman, q_i ∝ p_i·√E[e²], which minimizes the WHOLE frame's summed variance. A
`filter_in` entry whose raypath almost never passes has a tiny E[e²] per dealt
ray, so Neyman alone deals it far below its proportional share -- and the narrow
arc that entry draws is the only place its rays land, so that one feature the
user asked for comes out much noisier than under `"proportional"`. The relative
floor q_i ≥ p_norm_i / R in ComputeAdaptiveRayAllocationWeights
(core/simulator.hpp) is the fix; this test fails if it is removed or weakened.

Scene (`ray_allocation_narrow_arc_starvation.json`): a Parry arc -- randomly
oriented columns (entry 0) next to Parry-oriented columns filtered to the [3, 5]
raypath (entry 1), equal proportions, sun at 4°, 512×384, 2,000,000 rays.

Method: every arm is rendered twice with two fixed seeds (a seeded run is
single-threaded and reproducible, so the outcome is deterministic on a given
platform), exported as the float XYZ accumulator (`--format npy`). The Parry
arc's mask comes from a third render of entry 1 alone (proportional, 2× rays):
pixels above the 80th percentile of its non-zero Y, pooled into 2×2 bins with at
least 2 masked pixels. The noise of an arm is the pooled relative difference of
its two frames over that mask, sqrt(Σ(A−B)² / Σ((A+B)/2)² / 2). The assertion is
noise(adaptive) / noise(proportional) ≤ threshold.

Why that metric: on the same renders, per-bin RMS of the relative difference at
4×4 bins (the metric first used to diagnose this) separated red from green only
1.28× in the worst pair -- 40 bins is too few, and the proportional arm's own
noise estimate scatters the ratio. 2×2 bins pooled gave the widest worst-case
separation of the six bin-size / mask / estimator combinations compared (1.70×).
Pooling over absolute differences is safe here only because the mask holds the
arc alone: a mask that took in the sun's disk would be dominated by a handful of
its bins.

Calibration (2026-09-28, legacy CPU path on arm64 macOS, seed pairs 1:2 … 15:16):
  red   (pure Neyman, i.e. without the relative floor): 2.07 – 2.57 (seed 1:2: 2.29)
  green (relative floor at R = 2):                     1.06 – 1.22 (seed 1:2: 1.19)
Threshold 1.6 -- the geometric midpoint of the worst red and the worst green,
leaving about 1.3× on either side. A different platform draws a different
sample from the same distribution (the seeds fix the stream, not the floating
point), which is what the margin on the green side is for.

Cost: four 2M-ray renders plus one 4M-ray mask render, single-threaded -- about
23 s on the calibration machine.
"""

import json
import os
import shutil
import tempfile
import unittest

import numpy as np

from test.e2e.base import LumiceTestCase
from test.e2e.runner import get_project_root

CONFIG = get_project_root() / "test" / "e2e" / "configs" / "ray_allocation_narrow_arc_starvation.json"

# See the module docstring for the calibration behind the number.
MAX_ADAPTIVE_OVER_PROPORTIONAL_ARC_NOISE = 1.6
SEEDS = (1, 2)
MASK_SEED = 3
FILTERED_ENTRY = 1
MASK_PERCENTILE = 80
BIN = 2


def _bin(x):
    h, w = x.shape
    return x[: h // BIN * BIN, : w // BIN * BIN].reshape(h // BIN, BIN, w // BIN, BIN).sum((1, 3))


def _pooled_relative_noise(a, b, mask):
    A, B = _bin(a)[mask], _bin(b)[mask]
    return float(np.sqrt(np.sum((A - B) ** 2) / np.sum(((A + B) / 2.0) ** 2) / 2.0))


class TestRayAllocationNarrowArcStarvation(LumiceTestCase):
    """adaptive keeps a filtered entry's narrow arc about as clean as proportional does."""

    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        if not CONFIG.exists():
            raise unittest.SkipTest(f"Config not found: {CONFIG}")
        cls._cfg_dir = tempfile.mkdtemp(prefix="lumice_e2e_narrow_arc_")
        base = json.loads(CONFIG.read_text())
        variants = {}
        for mode in ("adaptive", "proportional"):
            cfg = json.loads(json.dumps(base))
            cfg["scene"]["ray_allocation"] = mode
            variants[mode] = cfg
        iso = json.loads(json.dumps(base))
        layer = iso["scene"]["scattering"][0]
        layer["entries"] = [layer["entries"][FILTERED_ENTRY]]
        iso["scene"]["ray_allocation"] = "proportional"
        iso["scene"]["ray_num"] = 2 * base["scene"]["ray_num"]
        variants["mask"] = iso
        paths = {}
        for name, cfg in variants.items():
            paths[name] = os.path.join(cls._cfg_dir, f"{name}.json")
            with open(paths[name], "w") as f:
                json.dump(cfg, f)
        cls.renders = {
            (mode, seed): cls.render_once(paths[mode], ["--format", "npy", "--seed", str(seed)])
            for mode in ("adaptive", "proportional")
            for seed in SEEDS
        }
        cls.renders[("mask", MASK_SEED)] = cls.render_once(
            paths["mask"], ["--format", "npy", "--seed", str(MASK_SEED)]
        )

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls._cfg_dir, ignore_errors=True)
        super().tearDownClass()

    def _y(self, key):
        result = self.renders[key]
        self.assertEqual(result.returncode, 0, f"{key} failed:\nstdout: {result.stdout}\nstderr: {result.stderr}")
        path = os.path.join(result.output_dir, "img_00.npy")
        self.assertTrue(os.path.isfile(path), f"{key}: no img_00.npy in {result.output_dir}")
        return np.load(path)[..., 1].astype(np.float64)

    def test_adaptive_does_not_starve_the_filtered_arc(self):
        iso = self._y(("mask", MASK_SEED))
        lit = iso[iso > 0]
        self.assertGreater(lit.size, 0, "the filtered entry alone drew nothing: the scene no longer has its arc")
        mask = _bin((iso > np.percentile(lit, MASK_PERCENTILE)).astype(float)) >= BIN * BIN // 2
        self.assertGreaterEqual(int(mask.sum()), 100, "the arc mask is too small for a stable noise estimate")
        noise = {
            mode: _pooled_relative_noise(self._y((mode, SEEDS[0])), self._y((mode, SEEDS[1])), mask)
            for mode in ("adaptive", "proportional")
        }
        self.assertGreater(noise["proportional"], 0.0)
        ratio = noise["adaptive"] / noise["proportional"]
        self.assertLessEqual(
            ratio,
            MAX_ADAPTIVE_OVER_PROPORTIONAL_ARC_NOISE,
            f"the filtered entry's arc is {ratio:.2f}× as noisy under adaptive as under proportional "
            f"(noise {noise}); threshold {MAX_ADAPTIVE_OVER_PROPORTIONAL_ARC_NOISE}. Neyman is starving "
            f"the filter_in entry again -- suspect the relative floor in "
            f"ComputeAdaptiveRayAllocationWeights (core/simulator.cpp).",
        )


if __name__ == "__main__":
    unittest.main()
