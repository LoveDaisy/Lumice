"""`render --format npy`: the CLI's float32 export of the unexposed XYZ accumulator.

The 8-bit image the CLI normally writes is baked -- exposure scale, background, clamp,
sRGB gamma -- and its highlights are flattened by exactly the operators a quantitative
comparison against a numerical (non-Monte-Carlo) solution cannot undo. `--format npy`
writes the accumulator the image was baked FROM, per renderer, as `img_XX.npy` (shape
`(H, W, 3)`, float32, C order, channels X/Y/Z) plus a sidecar `img_XX.json` that records
which normalization was applied and every scalar needed to move to the other one, or on
to the baked image. These tests hold that path to three propositions:

  * the file is what it says it is (numpy loads it, the shape/dtype are the sidecar's);
  * it is a deterministic function of the seed and a stable one across seeds;
  * it is the SAME accumulator the 8-bit image comes from, not a second pipeline -- shown
    by re-baking the `normalized` export in Python with the production colour chain
    and matching the CLI's own PNG to the byte.

Fixture: `raw_float_export_oracle.json`, the shape of the first consumer's scene (a point
sun at 15 deg through near-horizontal prisms, one [3, 5] PBD raypath admitted, a 6 deg
linear lens looking below the horizon) with `ray_num` cut to 1M so a run costs ~1 s.
Two of its fields are load-bearing for the parity test and are documented at the test.
"""

import glob
import hashlib
import json
import os
from pathlib import Path

import numpy as np
from PIL import Image

from test.e2e.base import LumiceTestCase
from test.e2e.runner import get_project_root

CONFIGS_DIR = get_project_root() / "test" / "e2e" / "configs"
CONFIG = CONFIGS_DIR / "raw_float_export_oracle.json"
COLOR_CONFIG = CONFIGS_DIR / "raypath_color_three_arcs.json"

# Every sidecar key the export promises. A consumer that holds `raw` and wants `normalized`
# (or either and wants the baked image) needs no field outside this set.
SIDECAR_KEYS = {
    "normalization",
    "renderer_id",
    "width",
    "height",
    "channels",
    "dtype",
    "total_pixels",
    "emitted_energy",
    "intensity_factor",
    "axis_solid_angle",
    "anchor_l99_sky",
    "ev_mode",
    "sim_ray_num",
    "seed",
    "lumice_api_version",
}

# kNormScale, src/core/color_util.hpp. The `normalized` mode's scale is
# kNormScale * total_pixels / emitted_energy, the absolute-mode exposure scale at
# intensity_factor = 1 (LUMICE_RawXyzResult::emitted_energy's comment in src/include/lumice.h).
K_NORM_SCALE = np.float32(0.08)

# The production colour chain's constants, copied from src/util/color_data.hpp
# (kWhitePointD65, kXyzToRgb) and src/util/color_space.hpp (kSrgbCurveCutoffLinear). Copied,
# not imported: the C++ side has no Python binding, and these tests exist to check the export
# against that chain, so the oracle is deliberately an independent transcription. If either
# side's constants ever move, the byte-exact test below goes red -- which is the signal wanted.
WHITE_POINT_D65 = np.array([0.95047, 1.00000, 1.08883], dtype=np.float32)
XYZ_TO_RGB = np.array(
    [
        [3.2404542, -1.5371385, -0.4985314],
        [-0.9692660, 1.8760108, 0.0415560],
        [0.0556434, -0.2040259, 1.0572252],
    ],
    dtype=np.float32,
)
SRGB_CURVE_CUTOFF_LINEAR = np.float32(0.0031308)


def _load_export(out_dir):
    """The one renderer's (array, sidecar) pair from an npy run's output directory."""
    npys = sorted(glob.glob(os.path.join(out_dir, "img_*.npy")))
    assert len(npys) == 1, f"expected one .npy in {out_dir}, found {npys}"
    npy = Path(npys[0])
    sidecar = npy.with_suffix(".json")
    assert sidecar.is_file(), f"no sidecar next to {npy}"
    return np.load(npy), json.loads(sidecar.read_text())


def _gamut_clip_xyz(xyz):
    """GamutClipXyz, src/util/color_space.cpp: pull each pixel toward its D65 grey of equal Y
    until every linear RGB channel is non-negative, scaling the chromatic difference by the
    smallest positive a/b over the three channels (s starts at 1 and only shrinks)."""
    gray = WHITE_POINT_D65[None, None, :] * xyz[..., 1:2]
    diff = xyz - gray
    a = -(gray @ XYZ_TO_RGB.T)
    b = diff @ XYZ_TO_RGB.T
    s = np.ones(xyz.shape[:2], dtype=np.float32)
    for j in range(3):
        same_sign = a[..., j] * b[..., j] > 0
        safe_b = np.where(b[..., j] == 0, np.float32(1), b[..., j])
        ratio = np.where(same_sign, a[..., j] / safe_b, np.float32(1)).astype(np.float32)
        s = np.where(same_sign & (ratio < s), ratio, s)
    return diff * s[..., None] + gray


def _linear_to_srgb(linear):
    """LinearToSrgb, src/util/color_space.hpp: the straight segment below the cutoff, the
    2.4 curve above it, both evaluated in double and narrowed to float as the C++ does."""
    lin64 = linear.astype(np.float64)
    straight = (lin64 * 12.92).astype(np.float32)
    curve = (1.055 * np.power(lin64, 1.0 / 2.4) - 0.055).astype(np.float32)
    return np.where(linear < SRGB_CURVE_CUTOFF_LINEAR, straight, curve)


def _bake_normalized_to_srgb8(xyz_normalized, intensity_factor, background):
    """RenderConsumer::PostSnapshot's per-pixel chain (src/server/render.cpp) for the subset of
    the render configuration the fixture uses: screen tone, real colour (`ray_color` unset),
    `visible: full`, no grid / markers / labels. The stages, in the loop's order:
    exposure scale -> gamut clip -> XYZ-to-linear-RGB with clamp -> + background -> clamp ->
    sRGB gamma -> truncating narrow to uint8. Annotation layers, the print tone, the
    `ray_color` tint and the `visible` clip are NOT reproduced; a fixture that exercised any of
    them would need this oracle extended, not its tolerance raised."""
    xyz = xyz_normalized.astype(np.float32) * np.float32(intensity_factor)
    clipped = _gamut_clip_xyz(xyz)
    rgb = np.clip(clipped @ XYZ_TO_RGB.T, 0, 1).astype(np.float32)
    rgb = rgb + np.asarray(background, dtype=np.float32)[None, None, :]
    rgb = np.clip(rgb, 0, 1).astype(np.float32)
    return (_linear_to_srgb(rgb) * 255).astype(np.uint8)


class TestNpyFormat(LumiceTestCase):
    """AC1 / AC5: the file and its sidecar."""

    @classmethod
    def setUpClass(cls):
        cls.export = cls.render_once(CONFIG, ["--format", "npy", "--seed", "7"])

    def test_numpy_loads_the_declared_shape_and_dtype(self):
        self.assertEqual(self.export.returncode, 0, self.export.stderr)
        arr, meta = _load_export(self.export.output_dir)
        self.assertEqual(arr.dtype, np.float32)
        self.assertEqual(arr.shape, (meta["height"], meta["width"], 3))
        self.assertTrue(arr.flags["C_CONTIGUOUS"])
        # The fixture's resolution is [251, 801] = [width, height]; a transposed write would
        # still load, with the axes swapped, so pin the orientation against the config.
        cfg = json.loads(CONFIG.read_text())
        self.assertEqual((meta["width"], meta["height"]), tuple(cfg["render"][0]["resolution"]))
        self.assertTrue(np.isfinite(arr).all())
        self.assertGreater(arr.max(), 0.0, "the accumulator carried no energy at all")

    def test_files_follow_the_image_numbering_and_stdout_reports_them(self):
        out = Path(self.export.output_dir)
        self.assertTrue((out / "img_01.npy").is_file())
        self.assertTrue((out / "img_01.json").is_file())
        self.assertFalse(list(out.glob("*.jpg")) or list(out.glob("*.png")), "npy replaces the 8-bit image")
        saved = [l for l in self.export.stdout.splitlines() if l.startswith("Saved: ")]
        self.assertTrue(any(l.endswith("img_01.npy (251x801)") for l in saved), saved)
        self.assertTrue(any(l.endswith("img_01.json") for l in saved), saved)

    def test_sidecar_carries_every_promised_field(self):
        _, meta = _load_export(self.export.output_dir)
        self.assertEqual(set(meta), SIDECAR_KEYS)
        self.assertEqual(meta["normalization"], "raw")  # the default when neither flag nor config says
        self.assertEqual(meta["renderer_id"], 1)
        self.assertEqual(meta["channels"], "XYZ")
        self.assertEqual(meta["dtype"], "float32")
        self.assertEqual(meta["total_pixels"], meta["width"] * meta["height"])
        self.assertEqual(meta["ev_mode"], "absolute")
        self.assertEqual(meta["seed"], 7)
        self.assertEqual(meta["sim_ray_num"], 1000000)
        self.assertAlmostEqual(meta["emitted_energy"], 1000000.0)  # one 550 nm line at weight 1
        self.assertAlmostEqual(meta["intensity_factor"], 2.5)
        self.assertGreater(meta["axis_solid_angle"], 0.0)
        self.assertGreater(meta["anchor_l99_sky"], 0.0)
        self.assertIsInstance(meta["lumice_api_version"], int)

    def test_unseeded_run_records_seed_null(self):
        run = self.render_once(CONFIG, ["--format", "npy"])
        self.assertEqual(run.returncode, 0, run.stderr)
        _, meta = _load_export(run.output_dir)
        self.assertIn("seed", meta)
        self.assertIsNone(meta["seed"])


class TestNormalizationModes(LumiceTestCase):
    """AC4: `raw` and `normalized` differ by one scalar, the flag overrides the config."""

    @classmethod
    def setUpClass(cls):
        cls.raw = cls.render_once(CONFIG, ["--format", "npy", "--seed", "7"])
        cls.normalized = cls.render_once(
            CONFIG, ["--format", "npy", "--seed", "7", "--raw-normalization", "normalized"]
        )

    def test_normalized_is_raw_times_the_documented_scalar(self):
        raw, meta_raw = _load_export(self.raw.output_dir)
        norm, meta_norm = _load_export(self.normalized.output_dir)
        self.assertEqual(meta_raw["normalization"], "raw")
        self.assertEqual(meta_norm["normalization"], "normalized")
        scale = K_NORM_SCALE * np.float32(meta_raw["total_pixels"]) / np.float32(meta_raw["emitted_energy"])
        # intensity_factor (2.5 in the fixture) must NOT be in the scalar: `normalized` is the
        # intensity_factor = 1 baseline, the fixture's factor is the sidecar's to report.
        np.testing.assert_allclose(norm, raw * scale, rtol=1e-6, atol=0)
        self.assertNotAlmostEqual(meta_raw["intensity_factor"], 1.0)

    def test_config_key_selects_the_mode_and_the_flag_overrides_it(self):
        doc = json.loads(CONFIG.read_text())
        doc["raw_export"] = {"normalization": "normalized"}
        cfg = Path(self.output_dir) / "normalized_by_config.json"
        cfg.write_text(json.dumps(doc))

        by_config = os.path.join(self.output_dir, "by_config")
        os.makedirs(by_config)
        result = self.run_lumice(["-f", str(cfg), "-o", by_config, "--format", "npy", "--seed", "7"])
        self.assertEqual(result.returncode, 0, result.stderr)
        _, meta = _load_export(by_config)
        self.assertEqual(meta["normalization"], "normalized")

        overridden = os.path.join(self.output_dir, "overridden")
        os.makedirs(overridden)
        result = self.run_lumice(
            ["-f", str(cfg), "-o", overridden, "--format", "npy", "--seed", "7", "--raw-normalization", "raw"]
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        _, meta = _load_export(overridden)
        self.assertEqual(meta["normalization"], "raw")

    def test_unknown_config_value_is_rejected_not_defaulted(self):
        """A misspelt mode in the config must not silently become `raw`: the user wrote the key
        to make it take effect, and the default is the one mode they did not ask for."""
        doc = json.loads(CONFIG.read_text())
        doc["raw_export"] = {"normalization": "Normalised"}
        cfg = Path(self.output_dir) / "bad_mode.json"
        cfg.write_text(json.dumps(doc))
        for fmt in ("npy", "jpg"):
            result = self.run_lumice(["-f", str(cfg), "-o", self.output_dir, "--format", fmt])
            self.assertNotEqual(result.returncode, 0, f"--format {fmt} accepted a bad raw_export.normalization")
            self.assertIn("raw_export.normalization", result.stderr)


class TestSeedBehaviour(LumiceTestCase):
    """AC7: same seed, same bytes; different seed, the same field within Monte-Carlo noise."""

    # Measured on this fixture at 1M rays across three seed pairs (7/8, 7/9, 8/9): the Y
    # channel's Pearson correlation over 4x4 block means reads 0.984 on every pair, and the
    # total landed energy agrees within 1.5%. Per-pixel correlation is only ~0.79 at this ray
    # budget (17% of pixels carry energy), which is why the ruler is the block mean: it
    # averages the shot noise down without hiding a structural difference -- a transposed or
    # mis-scaled export reads near 0 either way. Thresholds sit well under the measurements so
    # ordinary noise never trips them; they are not calibrated to catch a subtle drift.
    BLOCK = 4
    MIN_BLOCK_CORR = 0.95
    MAX_ENERGY_RATIO_DEVIATION = 0.05

    def _export(self, subdir, seed):
        out_dir = os.path.join(self.output_dir, subdir)
        os.makedirs(out_dir)
        result = self.run_lumice(["-f", str(CONFIG), "-o", out_dir, "--format", "npy", "--seed", str(seed)])
        self.assertEqual(result.returncode, 0, result.stderr)
        return out_dir

    def test_same_seed_is_byte_identical(self):
        a = self._export("seed_a", 7)
        b = self._export("seed_b", 7)
        digest = lambda d: hashlib.md5(Path(d, "img_01.npy").read_bytes()).hexdigest()
        self.assertEqual(digest(a), digest(b))
        self.assertEqual(Path(a, "img_01.json").read_text(), Path(b, "img_01.json").read_text())

    def test_different_seeds_agree_within_monte_carlo_noise(self):
        a, meta_a = _load_export(self._export("seed_7", 7))
        b, meta_b = _load_export(self._export("seed_8", 8))
        self.assertEqual(a.shape, b.shape)
        self.assertEqual(a.dtype, b.dtype)
        self.assertFalse(np.array_equal(a, b), "two seeds produced the same accumulator")
        for key in ("emitted_energy", "total_pixels", "intensity_factor", "axis_solid_angle", "ev_mode"):
            self.assertEqual(meta_a[key], meta_b[key], key)

        def block_mean(y):
            h, w = y.shape
            k = self.BLOCK
            return y[: h // k * k, : w // k * k].reshape(h // k, k, w // k, k).mean(axis=(1, 3))

        ya, yb = a[..., 1], b[..., 1]
        corr = np.corrcoef(block_mean(ya).ravel(), block_mean(yb).ravel())[0, 1]
        self.assertGreater(corr, self.MIN_BLOCK_CORR, f"block-mean correlation {corr:.4f}")
        ratio = ya.sum() / yb.sum()
        self.assertLess(abs(ratio - 1.0), self.MAX_ENERGY_RATIO_DEVIATION, f"energy ratio {ratio:.4f}")


class TestParityWithBakedImage(LumiceTestCase):
    """AC8: the export IS the accumulator the PNG was baked from.

    Re-bake the `normalized` export in Python with a transcription of the production colour
    chain and compare against the PNG the CLI wrote from the same config and seed. Byte-exact
    is the measured result on this fixture (0 of 603,153 channel values differ); the assertion
    allows 1 LSB so a float32-vs-float64 rounding difference at a truncation boundary cannot
    make the test flaky, which still leaves any real divergence (a missing operator, a wrong
    scalar) tens of LSB out of reach.

    Two fixture fields make this comparison possible at all, and a fixture that changed either
    would need this test rethought, not re-tolerated:

      * `ev_mode: absolute`. `normalized` applies the ABSOLUTE-mode scale regardless of the
        renderer's own `ev_mode`; under `relative` the PNG is anchored to the sky's P99 instead,
        and the two would differ by a constant gain that is not an error.
      * `intensity_factor: 2.5` -- not 1. It is applied here from the sidecar, which is what
        shows the export left it out and reported it, rather than baked it in silently.
    """

    @classmethod
    def setUpClass(cls):
        cls.npy = cls.render_once(
            CONFIG, ["--format", "npy", "--raw-normalization", "normalized", "--seed", "7"]
        )
        cls.png = cls.render_once(CONFIG, ["--format", "png", "--seed", "7"])

    def test_rebaked_export_matches_the_cli_png_within_one_lsb(self):
        self.assertEqual(self.npy.returncode, 0, self.npy.stderr)
        self.assertEqual(self.png.returncode, 0, self.png.stderr)
        xyz, meta = _load_export(self.npy.output_dir)
        self.assertEqual(meta["normalization"], "normalized")
        self.assertEqual(meta["ev_mode"], "absolute")
        cfg = json.loads(CONFIG.read_text())["render"][0]
        self.assertEqual(cfg["ev_mode"], "absolute")
        self.assertNotIn("ray_color", cfg)
        self.assertEqual(cfg["visible"], "full")
        self.assertNotIn("grid", cfg)

        png = np.asarray(Image.open(Path(self.png.output_dir, "img_01.png")).convert("RGB"))
        rebaked = _bake_normalized_to_srgb8(xyz, meta["intensity_factor"], cfg["background"])
        self.assertEqual(rebaked.shape, png.shape)
        diff = np.abs(rebaked.astype(np.int16) - png.astype(np.int16))
        self.assertLessEqual(
            int(diff.max()),
            1,
            f"max |rebaked - png| = {diff.max()} LSB at {np.unravel_index(diff.argmax(), diff.shape)}; "
            f"{int((diff > 1).sum())} values differ by more than 1",
        )
        # The picture is not trivially black: the parity has to be over pixels that carry light.
        self.assertGreater(int((png > 0).sum()), 1000)


class TestOptionInteractions(LumiceTestCase):
    """AC9: the flag's contract with --format, and what happens to the colour composite."""

    def test_raw_normalization_requires_npy_format(self):
        for fmt_args in ([], ["--format", "png"], ["--format", "jpg"]):
            result = self.run_lumice(
                ["-f", str(CONFIG), "-o", self.output_dir, "--raw-normalization", "raw"] + fmt_args
            )
            self.assertNotEqual(result.returncode, 0, f"accepted --raw-normalization with {fmt_args or 'default'}")
            self.assertIn("--raw-normalization requires --format npy", result.stderr)

    def test_raw_normalization_rejects_unknown_value_and_missing_value(self):
        result = self.run_lumice(["-f", str(CONFIG), "--format", "npy", "--raw-normalization", "bogus"])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("--raw-normalization", result.stderr)
        result = self.run_lumice(["-f", str(CONFIG), "--format", "npy", "--raw-normalization"])
        self.assertNotEqual(result.returncode, 0)

    def test_help_lists_npy_and_the_flag(self):
        result = self.run_lumice(["render", "--help"])
        self.assertEqual(result.returncode, 0)
        self.assertIn("npy", result.stdout)
        self.assertIn("--raw-normalization", result.stdout)

    def test_raypath_color_composite_stays_8bit_under_npy(self):
        """The mono image goes float; the per-raypath composite keeps its 8-bit jpg form.
        There is no float composite -- the composite is a display product, not a measurement."""
        doc = json.loads(COLOR_CONFIG.read_text())
        doc["scene"]["ray_num"] = 20000  # the oracle here is which files exist, not their content
        cfg = Path(self.output_dir) / "color_cheap.json"
        cfg.write_text(json.dumps(doc))
        result = self.run_lumice(["-f", str(cfg), "-o", self.output_dir, "--format", "npy", "--seed", "1"])
        self.assertEqual(result.returncode, 0, result.stderr)
        out = Path(self.output_dir)
        names = sorted(p.name for p in out.iterdir() if p.name.startswith("img_"))
        self.assertEqual(names, ["img_01.json", "img_01.npy", "img_01_components.jpg"])
        # The composite line keeps the 8-bit path's `Saved:` shape, and the class-signal line
        # test_raypath_color.py reads still follows it.
        self.assertIn("img_01_components.jpg (512x256)", result.stdout)
        self.assertTrue(any(l.startswith("ColorClassSignal:") for l in result.stdout.splitlines()))
