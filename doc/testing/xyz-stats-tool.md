# XYZ Stats Tool

`scripts/dump_xyz_stats.py` batch-runs the e2e scenes through the Lumice CLI's
public float32 export and computes anchor-relative linear-Y percentile and
distribution statistics per scene, producing per-scene JSON and a consolidated
summary. It is a diagnostic for auto-EV / exposure-anchor work: it shows where a
scene's linear luminance sits relative to the level the mono relative exposure
anchor treats as white.

## How it gets data

The tool runs no C API and loads no shared library. Each scene is one CLI
subprocess:

```
Lumice -f <config> -o <tmpdir> --format npy --raw-normalization raw [--seed <N>]
```

`--format npy` writes each renderer's unexposed linear XYZ accumulator as
`img_XX.npy` (shape `(H, W, 3)`, float32, channels X/Y/Z) plus a sidecar
`img_XX.json` carrying the exposure scalars (`SaveRawFloatResults` in
`src/main.cpp`). The sidecar's key set is pinned key-by-key by
`test/e2e-correctness/test_raw_float_export.py` (fast pytest leg), so a schema
drift turns that contract test red before this tool can misread a file. Any
raypath-colour composite the CLI additionally writes goes out as jpg and is
ignored — only the mono XYZ accumulators are statistics objects.

## Prerequisites

```bash
./scripts/build.sh -j release
# Binary lands at build/cmake_install/static/Lumice
```

Python dependencies: `numpy`. The binary location can be overridden with the
`LUMICE_BIN` environment variable (absolute, or relative to the repository root).

## Usage

```bash
# All 9 scenes → xyz_stats_output/
python scripts/dump_xyz_stats.py

# Single scene
python scripts/dump_xyz_stats.py --scenes halo_22 --output /tmp/test

# Multiple scenes
python scripts/dump_xyz_stats.py --scenes halo_22,color,rp46

# Skip rp46/rp46_nof (faster 7-scene run)
python scripts/dump_xyz_stats.py --no-rp46 --output /tmp/7scenes

# Per-run random seeds (CLI default), losing cross-run comparability
python scripts/dump_xyz_stats.py --seed random
```

### Seeding

The tool forwards a fixed seed to the CLI (`--seed`, default `7`). Two reasons:

- Cross-engine-version comparison is this tool's main use; run-to-run
  Monte-Carlo noise would otherwise pollute the comparison. (The old C-API
  incarnation was deterministic only by accident — an in-process seed counter
  that reset per launch.)
- The CLI contract makes a seeded run single-threaded (the pool is sized to one
  worker), so expect longer wall-clock per scene than an unseeded run.

## Output

| File | Description |
|------|-------------|
| `<output>/<scene>.json` | Per-scene result (see schema below) |
| `<output>/summary.json` | All scenes merged into one JSON array |
| `<output>/summary.md`   | Markdown table: one row per scene × renderer |

### Per-scene JSON schema

```json
{
  "scene": "halo_22",
  "config": "<repo-root>/test/e2e/configs/halo_22.json",
  "timestamp": "2026-10-17T15:06:31.257902",
  "run_duration_s": 17.91,
  "truncated": false,
  "cli": {
    "binary": "<repo-root>/build/cmake_install/static/Lumice",
    "seed": 7,
    "lumice_api_version": 453
  },
  "renderers": [
    {
      "renderer_id": 1,
      "ev_mode": "absolute",
      "anchor": {
        "anchor_l99_sky": 85009480.0,
        "axis_solid_angle": 6.103516352595761e-05,
        "emitted_energy": 879532672.0,
        "intensity_factor": 1.0,
        "sim_ray_num": 10000000
      },
      "stats": {
        "pixel_count": 65536,
        "non_zero_count": 65536,
        "percentiles": {
          "p50": 0.094, "p70": 0.135, "p90": 0.326,
          "p95": 0.703, "p99": 1.631,
          "p99_3": 1.756, "p99_5": 1.842, "p99_7": 1.938,
          "p99_9": 2.062, "p99_95": 2.105, "p99_99": 2.263
        },
        "mean": 0.274, "std": 21.738, "cv": 79.202,
        "max": 5542.81, "saturation_rate": 0.0316
      }
    }
  ]
}
```

`renderers[]` holds one entry per exported accumulator (all current e2e scenes
have a single renderer; the loader globs `img_*.npy` and handles several). The
`anchor` scalars are the sidecar's published values, recorded verbatim so a
reader can re-derive or re-normalize offline. On a failed or unreadable run the
scene entry carries an `"error"` string and an empty `renderers` array.

**`saturation_rate`** = fraction of non-zero pixels with `norm_y > 1.0`.

## Normalization

Normalized Y is computed as:

```
norm_y = Y(pixel) / (axis_solid_angle * anchor_l99_sky)
```

This is the per-pixel relative illumination of the current mono relative
exposure contract: the renderer's relative mode converges (per scene, at
`intensity_factor = 1`) to displaying `L(pos) · m(pos) / L99_sky`, where
`m(pos) = Ω_p(pos)/Ω_axis` — the two-sided shared-anchor form of
`doc/rendering/ev-pipeline-architecture.md` §2.8. `norm_y` is that expression's
input stage, before the `target_white` constant; `norm_y > 1.0` therefore means
the pixel sits above the level auto-EV maps to white.

Two older statements are retired and deliberately not reproduced here:

- The old denominator `snapshot_intensity` (average landed intensity per pixel)
  no longer exists in the published contract.
- The old claim that GUI auto-EV anchors "P99.5 of the current view's pixels to
  `target_white`" described the pre-§2.8 self-anchoring design. The anchor is
  now measured on a fixed full-sky plane (`anchor_l99_sky`), independent of the
  current view.

Scenes whose config sets `ev_mode: absolute` still publish both anchor scalars
(the anchor is measured unconditionally), so anchor-relative statistics work
identically for both modes; `ev_mode` is recorded per renderer so the reader
knows which branch baked the scene's own output image.

## Relation to GUI auto-EV

| This tool field | GUI variable | Role |
|-----------------|-------------|------|
| `anchor.axis_solid_angle * anchor.anchor_l99_sky` | `g_state.p99_raw_y` | The mono relative EV anchor (`src/gui/app.cpp`, `SyncFromPoller`) |
| (same) → `LUMICE_ComputeEvAuto` | `g_state.ev_auto` | Auto-EV output |

The tool does not output a P99.5-of-view percentile as an EV anchor anymore;
`stats.percentiles.p99_5` is a distribution descriptor of the anchor-relative
luminance, not the exposure anchor.

## Timeout and truncation

| Scene type | Timeout |
|------------|---------|
| Regular (7 scenes) | 180 s |
| `rp46`, `rp46_nof` | 90 s |

The regular timeout is sized for the seeded (single-threaded) runs this tool
makes: `multi_scatter` and `color` exceed the 60 s the unseeded runs used to
need, so a tighter limit would truncate mid-convergence.

The CLI materializes its export at ~1 s intervals while simulating, so on
timeout the tool kills the subprocess and reads the last frame that reached
disk, marking the scene `"truncated": true`. If no readable npy/sidecar pair
exists (nothing saved yet, or the kill landed mid-write), the scene gets an
error row instead — the tool never emits statistics from a half-written file.
A zero-valued anchor product (`axis_solid_angle * anchor_l99_sky <= 0`, e.g. a
sky with no positive-Y samples) yields zero stats plus a stderr warning.

## Verification boundary

Verified 2026-10-17 on macOS (Darwin 23.6.0, arm64), static release build of
Lumice 4.7.3-dev, seed 7:

- All 9 scenes complete without truncation at the 180 s / 90 s tiers; per-scene
  wall-clock ranges from ~8 s (`rp46`) to ~90 s (`color`, the slowest — which
  is what the 180 s regular tier is sized against, see the timeout table).
- Schema of every per-scene JSON matches this document; percentiles are
  monotonic, `saturation_rate ∈ [0,1]`, `anchor_l99_sky > 0` on every scene
  (including `ev_mode: absolute` ones — the anchor publishes unconditionally).
- Truncation paths exercised by deliberately short timeouts: a kill after the
  CLI's first ~1 s periodic save yields `"truncated": true` plus the last
  frame's statistics; a kill before any save yields an error row and an empty
  `renderers` array. Neither hangs or crashes the tool.

Not covered by that verification: Windows/Linux runs, GPU-backend routes
(the tool exercises whatever `--backend auto` selects), and multi-renderer
scenes (no current e2e scene has more than one renderer — the loader path is
glob-generic but was exercised single-renderer only).
