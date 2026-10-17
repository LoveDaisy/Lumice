#!/usr/bin/env python3
"""Batch XYZ percentile statistics tool for Lumice exposure-anchor exploration.

Runs the e2e scenes through the Lumice CLI's public float32 export
(`Lumice -f <config> -o <dir> --format npy --raw-normalization raw`), loads the
per-renderer `img_XX.npy` accumulators plus their sidecar JSON, computes
anchor-relative linear-Y percentile + distribution stats, and writes per-scene
JSON plus a consolidated summary.

No in-process ABI binding and no shared library: the only machine-facing contract
consumed is
the CLI npy sidecar, whose key set is pinned key-by-key by
test/e2e-correctness/test_raw_float_export.py (the fast pytest leg), so a
schema drift turns the contract test red before this tool can misread a file.

Usage:
    python scripts/dump_xyz_stats.py [--scenes <s1,s2,...>] [--output <dir>]
                                    [--no-rp46] [--seed <N>|random]

Prerequisites:
    Build the static release CLI: ./scripts/build.sh -j release
    Binary: build/cmake_install/static/Lumice  (override via LUMICE_BIN env)

Normalization:
    norm_y = Y(pixel) / (axis_solid_angle * anchor_l99_sky)

    This is the per-pixel relative illumination the mono relative exposure
    anchor publishes (ev-pipeline-architecture.md §2.8; the GUI consumes the
    same product as `g_state.p99_raw_y` before `LUMICE_ComputeEvAuto`).
    norm_y > 1.0 means the pixel sits above the target_white-mapped level,
    which is what `saturation_rate` counts. A seeded run is single-threaded by
    CLI contract; the fixed default seed exists so cross-version comparisons
    compare the engine, not Monte-Carlo noise.
"""

import argparse
import datetime
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import numpy as np

REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BIN = REPO_ROOT / "build" / "cmake_install" / "static" / "Lumice"
DEFAULT_SEED = 7

# ─── Scene table ──────────────────────────────────────────────────────────────

ALL_SCENES = {
    "halo_22":      "test/e2e/configs/halo_22.json",
    "multi_scatter": "test/e2e/configs/multi_scatter.json",
    "color":        "test/e2e/configs/color.json",
    "pyramid":      "test/e2e/configs/pyramid.json",
    "cza":          "test/e2e/configs/cza.json",
    "parhelion":    "test/e2e/configs/parhelion.json",
    "filters":      "test/e2e/configs/filters.json",
    "rp46":         "test/e2e/configs/raypath_symmetry_4_6.json",
    "rp46_nof":     "test/e2e/configs/raypath_symmetry_4_6_nofilter.json",
}
RP46_SCENES = {"rp46", "rp46_nof"}

TIMEOUT_DEFAULT_S = 60
TIMEOUT_RP46_S = 90

PERCENTILE_KEYS = ["p50", "p70", "p90", "p95", "p99", "p99_3", "p99_5", "p99_7", "p99_9", "p99_95", "p99_99"]
PERCENTILE_VALUES = [50.0, 70.0, 90.0, 95.0, 99.0, 99.3, 99.5, 99.7, 99.9, 99.95, 99.99]

# ─── Statistics computation ───────────────────────────────────────────────────

def _zero_stats(pixel_count):
    return {
        "pixel_count":    pixel_count,
        "non_zero_count": 0,
        "percentiles":    {k: 0.0 for k in PERCENTILE_KEYS},
        "mean":           0.0,
        "std":            0.0,
        "cv":             0.0,
        "max":            0.0,
        "saturation_rate": 0.0,
    }


def compute_stats(xyz, pixel_count, denom):
    """Anchor-relative linear-Y stats: norm_y = Y / (axis_solid_angle * anchor_l99_sky)."""
    if xyz is None or pixel_count <= 0 or denom <= 0.0:
        return _zero_stats(pixel_count)

    y_all = xyz[..., 1].ravel() if xyz.ndim == 3 else np.asarray(xyz).ravel()[1::3]
    nonzero_mask = y_all > 0.0
    nonzero_count = int(nonzero_mask.sum())

    if nonzero_count == 0:
        return _zero_stats(pixel_count)

    norm_y = y_all[nonzero_mask].astype(np.float64) / denom
    pct_vals = np.percentile(norm_y, PERCENTILE_VALUES)

    mean_v = float(np.mean(norm_y))
    std_v  = float(np.std(norm_y))
    max_v  = float(np.max(norm_y))

    return {
        "pixel_count":    pixel_count,
        "non_zero_count": nonzero_count,
        "percentiles":    {k: float(v) for k, v in zip(PERCENTILE_KEYS, pct_vals)},
        "mean":           mean_v,
        "std":            std_v,
        "cv":             std_v / mean_v if mean_v > 0.0 else 0.0,
        "max":            max_v,
        "saturation_rate": float(np.sum(norm_y > 1.0) / nonzero_count),
    }

# ─── Export-directory loading ────────────────────────────────────────────────

def load_renderer_outputs(tmpdir):
    """Load every `img_*.npy` + sidecar pair from a CLI export directory.

    Returns (rows, unreadable): rows is a list of (meta_dict, xyz_array) sorted
    by renderer id, unreadable counts npy/sidecar pairs that could not be read
    (a kill landing mid-write). A row whose sidecar is unreadable is dropped:
    without the sidecar's anchor scalars the accumulator cannot be normalized.
    """
    rows = []
    unreadable = 0
    for npy_path in sorted(tmpdir.glob("img_*.npy")):
        meta = None
        meta_path = npy_path.with_suffix(".json")
        if meta_path.is_file():
            try:
                with open(meta_path, encoding="utf-8") as f:
                    meta = json.load(f)
            except (OSError, json.JSONDecodeError):
                meta = None
        try:
            xyz = np.load(npy_path)
        except Exception:
            xyz = None

        if meta is None or xyz is None or xyz.ndim < 1 or xyz.shape[-1] != 3:
            unreadable += 1
            continue
        rows.append((meta, xyz))
    return rows, unreadable

# ─── Per-scene runner ─────────────────────────────────────────────────────────

def run_scene(scene_name, config_path, timeout_s, lumice_bin, seed):
    """Run one scene via the CLI; returns its result dict (never raises)."""
    print(f"  [{scene_name}] Starting (timeout={timeout_s}s, seed={seed})...", flush=True)
    t_start = time.time()

    tmpdir = Path(tempfile.mkdtemp(prefix=f"xyz_stats_{scene_name}_"))
    cmd = [str(lumice_bin), "-f", str(config_path), "-o", str(tmpdir),
           "--format", "npy", "--raw-normalization", "raw"]
    if seed != "random":
        cmd += ["--seed", str(seed)]

    truncated = False
    error = None
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout_s)
        if proc.returncode != 0:
            tail = (proc.stderr or "").strip().splitlines()[-3:]
            error = f"Lumice exited {proc.returncode}: {' | '.join(tail)}"
    except subprocess.TimeoutExpired:
        truncated = True
        print(f"  [{scene_name}] Timeout after {timeout_s}s — using last saved frame (truncated)",
              file=sys.stderr, flush=True)

    rows, unreadable = load_renderer_outputs(tmpdir)
    if error is None and not rows:
        error = "no readable img_*.npy/img_*.json pair in the export directory"
    elif error is None and unreadable:
        error = f"{unreadable} img_* export file pair(s) unreadable (corrupt write?)"

    renderers = []
    api_version = None
    for meta, xyz in rows:
        renderer_id = meta.get("renderer_id")
        pixel_count = int(xyz.size // 3)
        denom = float(meta["axis_solid_angle"]) * float(meta["anchor_l99_sky"])
        if denom <= 0.0:
            print(f"  [{scene_name}] WARNING: renderer {renderer_id} anchor product "
                  f"(axis_solid_angle * anchor_l99_sky) is {denom}; emitting zero stats",
                  file=sys.stderr, flush=True)
            stats = _zero_stats(pixel_count)
        else:
            stats = compute_stats(xyz, pixel_count, denom)
        renderers.append({
            "renderer_id": renderer_id,
            "ev_mode":     meta.get("ev_mode"),
            "anchor": {
                "anchor_l99_sky":  meta.get("anchor_l99_sky"),
                "axis_solid_angle": meta.get("axis_solid_angle"),
                "emitted_energy":  meta.get("emitted_energy"),
                "intensity_factor": meta.get("intensity_factor"),
                "sim_ray_num":     meta.get("sim_ray_num"),
            },
            "stats": stats,
        })
        if api_version is None and isinstance(meta.get("lumice_api_version"), int):
            api_version = meta["lumice_api_version"]

    result = {
        "scene":          scene_name,
        "config":         config_path,
        "timestamp":      datetime.datetime.now().isoformat(),
        "run_duration_s": round(time.time() - t_start, 2),
        "truncated":      truncated,
        "cli": {
            "binary":            str(lumice_bin),
            "seed":              seed if seed == "random" else int(seed),
            "lumice_api_version": api_version,
        },
        "renderers": renderers,
    }
    if error is not None:
        result["error"] = error
        result["renderers"] = []

    shutil.rmtree(tmpdir, ignore_errors=True)
    return result

# ─── Summary Markdown ─────────────────────────────────────────────────────────

_SUMMARY_PCTS = ["p95", "p99", "p99_5", "p99_9"]


def build_summary_md(all_results, total_elapsed_s):
    lines = []
    lines.append("# XYZ Stats Summary\n")
    lines.append(f"Generated: {datetime.datetime.now().isoformat()}  ")
    lines.append(f"Total wall-time: {total_elapsed_s:.1f}s  ")
    lines.append("Normalization: norm_y = Y / (axis_solid_angle * anchor_l99_sky)\n")

    cols = ["scene", "rend", "dur_s", "trunc"] + _SUMMARY_PCTS + ["max", "sat_rate", "l99_sky", "omega_axis"]
    lines.append("| " + " | ".join(cols) + " |")
    lines.append("| " + " | ".join(["---"] * len(cols)) + " |")

    for r in all_results:
        if r.get("error"):
            row = [r["scene"], "-", "-", "N"] + ["ERR"] * (len(cols) - 4)
            lines.append("| " + " | ".join(row) + " |")
            continue
        for rend in r["renderers"]:
            d = rend["stats"]
            row = [r["scene"], str(rend["renderer_id"]), str(r["run_duration_s"]),
                   "Y" if r["truncated"] else "N"]
            for p in _SUMMARY_PCTS:
                val = d["percentiles"].get(p, float("nan"))
                row.append(f"{val:.5f}")
            row.append(f"{d['max']:.5f}")
            row.append(f"{d['saturation_rate']:.5f}")
            row.append(f"{rend['anchor']['anchor_l99_sky']:.6g}")
            row.append(f"{rend['anchor']['axis_solid_angle']:.6g}")
            lines.append("| " + " | ".join(row) + " |")

    return "\n".join(lines) + "\n"

# ─── Main ─────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Dump XYZ percentile stats for Lumice scenes")
    parser.add_argument("--scenes", type=str, default=None,
                        help="Comma-separated scene names (default: all 9)")
    parser.add_argument("--output", type=str, default="xyz_stats_output",
                        help="Output directory (default: xyz_stats_output/)")
    parser.add_argument("--no-rp46", action="store_true",
                        help="Skip rp46 and rp46_nof scenes")
    parser.add_argument("--seed", type=str, default=str(DEFAULT_SEED),
                        help="Fixed RNG seed forwarded to the CLI (default: 7) so cross-version "
                             "runs compare the engine, not Monte-Carlo noise; seeded runs are "
                             "single-threaded by CLI contract. Pass 'random' for CLI-default "
                             "randomness per run.")
    args = parser.parse_args()

    if args.seed != "random":
        try:
            if int(args.seed) <= 0:
                raise ValueError
        except ValueError:
            print(f"[ERROR] --seed must be a positive integer or 'random', got '{args.seed}'",
                  file=sys.stderr)
            sys.exit(1)

    if args.scenes:
        scene_names = [s.strip() for s in args.scenes.split(",")]
        unknown = [s for s in scene_names if s not in ALL_SCENES]
        if unknown:
            print(f"[ERROR] Unknown scene(s): {', '.join(unknown)}", file=sys.stderr)
            print(f"[ERROR] Valid scenes: {', '.join(ALL_SCENES)}", file=sys.stderr)
            sys.exit(1)
    else:
        scene_names = list(ALL_SCENES.keys())

    if args.no_rp46:
        scene_names = [s for s in scene_names if s not in RP46_SCENES]
        print("[INFO] --no-rp46: skipping rp46/rp46_nof", flush=True)

    lumice_bin = Path(os.environ.get("LUMICE_BIN", str(DEFAULT_BIN)))
    if not lumice_bin.is_absolute():
        lumice_bin = REPO_ROOT / lumice_bin
    if not lumice_bin.is_file():
        print(f"[ERROR] Lumice binary not found: {lumice_bin}", file=sys.stderr)
        print("[ERROR] Build first: ./scripts/build.sh -j release", file=sys.stderr)
        sys.exit(1)

    os.makedirs(args.output, exist_ok=True)
    print(f"[INFO] Binary: {lumice_bin}", flush=True)
    print(f"[INFO] Seed: {args.seed}", flush=True)
    print(f"[INFO] Running {len(scene_names)} scene(s): {', '.join(scene_names)}", flush=True)

    all_results = []
    t_total = time.time()

    for scene_name in scene_names:
        config_path = str(REPO_ROOT / ALL_SCENES[scene_name])
        timeout_s = TIMEOUT_RP46_S if scene_name in RP46_SCENES else TIMEOUT_DEFAULT_S
        result = run_scene(scene_name, config_path, timeout_s, lumice_bin, args.seed)

        all_results.append(result)
        out_path = os.path.join(args.output, f"{scene_name}.json")
        with open(out_path, "w", encoding="utf-8") as f:
            json.dump(result, f, indent=2)
        print(f"  [{scene_name}] -> {out_path}", flush=True)

    total_elapsed = time.time() - t_total
    print(f"\n[INFO] Total wall-time: {total_elapsed:.1f}s", flush=True)

    summary_json = os.path.join(args.output, "summary.json")
    with open(summary_json, "w", encoding="utf-8") as f:
        json.dump(all_results, f, indent=2)
    print(f"[INFO] Summary JSON:     {summary_json}", flush=True)

    summary_md = os.path.join(args.output, "summary.md")
    with open(summary_md, "w", encoding="utf-8") as f:
        f.write(build_summary_md(all_results, total_elapsed))
    print(f"[INFO] Summary Markdown: {summary_md}", flush=True)


if __name__ == "__main__":
    main()
