# XYZ Stats Tool

> **Current status: unavailable with the current C API.** `scripts/dump_xyz_stats.py` still mirrors
> the pre-v4.23 `LUMICE_RawXyzResult` layout, where the tail held
> `unfiltered_xyz_buffer` / `unfiltered_snapshot_intensity`. The current structure instead publishes
> `emitted_energy`, `epoch`, `anchor_l99_sky`, and `axis_solid_angle`. Both layouts are 72 bytes on
> 64-bit builds, so the script's size assertion cannot detect the mismatch and a run may interpret
> `epoch` as a pointer. Do not run the commands below until the script is migrated to the installed
> C API contract in `src/include/lumice_engine.h`.
>
> The remainder records the tool's historical input/output contract. Its auto-EV explanation is
> likewise historical: current mono relative exposure uses `axis_solid_angle * anchor_l99_sky`, not
> a percentile of the current view's pixels.

Historically, the tool batch-ran 9 e2e scenes via the C API (`liblumice.dylib`) and extracted
per-scene normalized linear-Y percentile statistics from both the `unfiltered_xyz_buffer` and
`xyz_buffer`, producing per-scene JSON and a consolidated Markdown table.

## Historical prerequisites (do not run against the current ABI)

```bash
# Build release shared library first
./scripts/build.sh -sj release
# Library will be at build/Release/shared/lib/liblumice.dylib
```

Python dependencies: `numpy` (standard in the project Python environment).

## Historical usage (not runnable on the current ABI)

```bash
# All 9 scenes → xyz_stats_output/
python scripts/dump_xyz_stats.py

# Single scene
python scripts/dump_xyz_stats.py --scenes halo_22 --output /tmp/test

# Multiple scenes
python scripts/dump_xyz_stats.py --scenes halo_22,color,rp46

# Skip rp46/rp46_nof (for faster 7-scene run)
python scripts/dump_xyz_stats.py --no-rp46 --output /tmp/7scenes

# Use a custom library path
LUMICE_LIB=/path/to/liblumice.dylib python scripts/dump_xyz_stats.py
```

## Output

| File | Description |
|------|-------------|
| `<output>/<scene>.json` | Per-scene stats (see schema below) |
| `<output>/summary.json` | All scenes merged into one JSON array |
| `<output>/summary.md`   | Markdown table: 9 rows × key percentile columns |

### Per-scene JSON schema

```json
{
  "scene": "halo_22",
  "config": "test/e2e/configs/halo_22.json",
  "timestamp": "2026-05-11T08:40:45.843000",
  "run_duration_s": 2.5,
  "truncated": false,
  "unfiltered": {
    "pixel_count": 65536,
    "non_zero_count": 42908,
    "percentiles": {
      "p50": 0.0106, "p70": 0.0155, "p90": 0.0426,
      "p95": 0.0864, "p99": 0.1846,
      "p99_3": 0.1962, "p99_5": 0.2056, "p99_7": 0.2143,
      "p99_9": 0.2248, "p99_95": 0.2298, "p99_99": 0.2380
    },
    "mean": 0.037, "std": 3.22, "cv": 87.7,
    "max": 665.0, "saturation_rate": 0.00002
  },
  "filtered": { "..." }
}
```

**`saturation_rate`** = fraction of non-zero pixels with `normalized_Y > 1.0`.

## Normalization

Normalized Y is computed as:

```
norm_y = xyz_buffer[pixel * 3 + 1] / snapshot_intensity
```

where `snapshot_intensity` is the `LUMICE_RawXyzResult.snapshot_intensity` field
(filtered side) or `unfiltered_snapshot_intensity` (unfiltered side).

This is a per-pixel-scaled value: `norm_y = 1.0` corresponds to the average scene
intensity, not absolute physical luminance. The GUI auto-EV algorithm anchors EV
so that P99.5 of `norm_y` maps to `target_white` (default 1.0).

## Relation to GUI auto-EV

| This tool field | GUI variable | Role |
|-----------------|-------------|------|
| `unfiltered.percentiles.p99` | `g_state.p995_raw_y` | Approximate EV anchor (tool outputs P99/0.99; GUI uses P99.5/0.995 — close but not identical) |
| `unfiltered.percentiles.*` | Upstream of `ComputeEvAuto()` | Auto-EV input |
| `filtered.*` | Same path (filter-absent scenes) | Reference for filter-on scenes |

For scenes without display filters, `filtered == unfiltered` (both buffers contain
the same accumulated rays). The distinction matters when a display filter is active
in the GUI while the simulation accumulates all rays unfiltered.

## Timeout policy

| Scene type | Timeout |
|------------|---------|
| Regular (7 scenes) | 60 s |
| `rp46`, `rp46_nof` | 90 s |

If a scene times out but `has_valid_data=1`, the current data is used and
`"truncated": true` is set in the JSON.
