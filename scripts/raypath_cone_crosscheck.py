#!/usr/bin/env python3
"""Cone-domain cross-check between a render frame and a raypath analysis run.

The frame-vs-analysis share calibration (doc/coordinate-convention.md §13): the only exact form of an
image <-> analysis share comparison is same-domain — a cone ROI on both sides.
This tool drives both arms, integrates the frame over the cone's pixels through
the inverse of the engine's own projection (equal-area single fisheye and
dual-fisheye EAR only; every other family is rejected explicitly), and prints a
comparison row with a verdict.

Semantics borrowed verbatim from the engine (a56: one authority per meaning):
  - AltAzToDir(P) = -(position vector of P); a displayed point is -travel.
    (src/util/sky_direction.hpp; ConeMembership in
    src/server/raypath_histogram_consumer.cpp dots AltAzToDir(P) against travel.)
  - Forward binning px = floor(v + res/2), screen handedness -x on the
    single-lens family, dual-fisheye left disk = upper sky (§10/§11 there).
  - EAR inverse: rho^2 = 1 - cz, so equal-area pixels carry equal solid angle
    and a cone sum needs no per-pixel area weight.

Self-checks (fail = non-zero exit): inscribed-disk pixel count against the
discrete circle area, and the cone pixel count against its analytic cap area
within an explicit rim-band bound.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import subprocess
import sys
import tempfile

SUPPORTED_SINGLE = {"fisheye_equal_area"}
SUPPORTED_DUAL = {"dual_fisheye_equal_area"}


def die(msg: str) -> None:
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(2)


# ---------------------------------------------------------------- camera math


def camera_rotation(az_deg: float, el_deg: float, roll_deg: float) -> list[list[float]]:
    """MakeCameraRotation (src/core/camera_rotation.hpp): R = Rz(az) Ry(90-el) Rz(-90+roll),
    left-multiplied by Chain."""
    r = mat_mul(rot_z(math.radians(az_deg)), rot_y(math.radians(90.0 - el_deg)))
    return mat_mul(r, rot_z(math.radians(-90.0 + roll_deg)))


def mat_mul(a: list[list[float]], b: list[list[float]]) -> list[list[float]]:
    return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


def mat_vec(a: list[list[float]], v: tuple[float, float, float]) -> tuple[float, float, float]:
    return tuple(sum(a[i][k] * v[k] for k in range(3)) for i in range(3))  # type: ignore[return-value]


def rot_z(t: float) -> list[list[float]]:
    c, s = math.cos(t), math.sin(t)
    return [[c, -s, 0.0], [s, c, 0.0], [0.0, 0.0, 1.0]]


def rot_y(t: float) -> list[list[float]]:
    c, s = math.cos(t), math.sin(t)
    return [[c, 0.0, s], [0.0, 1.0, 0.0], [-s, 0.0, c]]


def lens_scale(lens_type: str, fov_deg: float, short_pix: float) -> float:
    """ComputeLensScale's EAR branch: short/2/sqrt(2)/sin(fov/4)."""
    if lens_type in SUPPORTED_SINGLE:
        return short_pix / 2.0 / math.sqrt(2.0) / math.sin(math.radians(fov_deg) / 4.0)
    die(f"unsupported lens {lens_type}")


# ------------------------------------------------------------ pixel -> display


def pixel_to_display_single(
    lens_type: str, fov_deg: float, res: tuple[int, int], view: dict, px: int, py: int
) -> tuple[float, float, float] | None:
    """Inverse of ProjectExitToPixel's single-lens EAR branch -> DISPLAYED sky
    direction (position vector). None when the pixel centre is off the disk."""
    w, h = res
    scale = lens_scale(lens_type, fov_deg, float(min(w, h)))
    vx = (w / 2.0 - (px + 0.5)) / scale  # screen handedness: xy.x = -k*c_x
    vy = (py + 0.5 - h / 2.0) / scale
    rho2 = vx * vx + vy * vy
    if rho2 > 1.0:
        return None
    cz = 1.0 - rho2
    s = math.sqrt(1.0 + cz)
    c = (vx * s, vy * s, cz)
    rot = camera_rotation(view.get("azimuth", 0.0), view.get("elevation", 0.0), view.get("roll", 0.0))
    return mat_vec(rot, c)  # display = R * c  (c = R^T * display)


def pixel_to_display_dual(
    lens_type: str, fov_deg: float, res: tuple[int, int], px: int, py: int
) -> tuple[float, float, float] | None:
    """Inverse of the dual-fisheye EAR branch -> DISPLAYED sky direction. The
    dual family ignores the camera pose (forward consumes -w directly)."""
    del lens_type, fov_deg
    w, h = res
    short = min(w // 2, h)
    r = short / 2.0
    cy = h / 2.0
    is_left = px < w / 2.0
    cx = w / 2.0 - r if is_left else w / 2.0 + r
    x_norm = (py + 0.5 - cy) / r  # fy = x_norm*r + cy
    y_norm = (cx - (px + 0.5)) / r if is_left else ((px + 0.5) - cx) / r  # fx = -+y_norm*r + cx
    rho2 = x_norm * x_norm + y_norm * y_norm
    if rho2 > 1.0:
        return None
    z_hemi = 1.0 - rho2
    s = math.sqrt(1.0 + z_hemi)
    sx, sy = x_norm * s, y_norm * s  # (sx, sy) = display x,y (forward consumed -w = display)
    return (sx, sy, z_hemi if is_left else -z_hemi)


# ---------------------------------------------------------------- frame tools


def frame_cone_energy(
    npy_path: str,
    cfg: dict,
    cone_pos: tuple[float, float, float],
    cos_radius: float,
) -> tuple[float, dict]:
    """Sum the Y channel over pixels whose DISPLAYED direction is inside the
    cone; returns (energy, audit) with the self-check numbers in audit."""
    import numpy as np

    renderer = cfg["render"][0]
    lens = renderer["lens"]
    ltype, fov = lens["type"], float(lens.get("fov", 180.0))
    res = (int(renderer["resolution"][0]), int(renderer["resolution"][1]))
    view = renderer.get("view", {})
    w, h = res
    img = np.load(npy_path)
    if img.shape[:2] != (h, w):
        die(f"npy shape {img.shape} != config resolution {(h, w)}")

    n_disk = 0
    n_cone = 0
    energy = 0.0
    for px in range(w):
        for py in range(h):
            d = (
                pixel_to_display_single(ltype, fov, res, view, px, py)
                if ltype in SUPPORTED_SINGLE
                else pixel_to_display_dual(ltype, fov, res, px, py)
            )
            if d is None:
                continue
            n_disk += 1
            dot = d[0] * cone_pos[0] + d[1] * cone_pos[1] + d[2] * cone_pos[2]
            if dot >= cos_radius:
                n_cone += 1
                energy += float(img[py, px, 1])

    # self-checks (a56): discrete disk area and analytic cone cap area. The
    # dual short side reuses pixel_to_display_dual's exact formula
    # (min(w // 2, h)) — one owner for the radius, not two (a56).
    short = min(w // 2, h) if ltype in SUPPORTED_DUAL else min(w, h)
    r_disk = short / 2.0
    if ltype in SUPPORTED_SINGLE:
        expected_disk = math.pi * r_disk * r_disk
        disk_rel = abs(n_disk - expected_disk) / expected_disk
    else:
        expected_disk = 2.0 * math.pi * r_disk * r_disk
        disk_rel = abs(n_disk - expected_disk) / expected_disk
    if disk_rel > 0.02:
        die(f"disk pixel self-check failed: {n_disk} vs {expected_disk:.1f} ({disk_rel:.3%})")
    omega_pix = (4.0 * math.pi if ltype in SUPPORTED_DUAL else 2.0 * math.pi) / n_disk
    radius_rad = math.acos(max(-1.0, min(1.0, cos_radius)))
    expected_cone = 2.0 * math.pi * (1.0 - math.cos(radius_rad))
    cone_rel = abs(n_cone * omega_pix - expected_cone) / expected_cone
    rim_bound = 1.0 - math.cos(radius_rad + 1.5 * omega_pix ** 0.5)
    audit = {
        "disk_pixels": n_disk,
        "disk_expected": round(expected_disk, 1),
        "cone_pixels": n_cone,
        "omega_pix_sr": round(omega_pix, 8),
        "cone_solid_angle_rel_err": round(cone_rel, 5),
        "cone_rim_area_bound_rel": round(rim_bound, 5),
    }
    if cone_rel > rim_bound:
        die(f"cone pixel self-check failed: rel err {cone_rel:.4%} > rim bound {rim_bound:.4%}")
    return energy, audit


# ------------------------------------------------------------------- analysis


def read_analyze_csv(path: str) -> tuple[float, dict[str, float], float]:
    """Returns (total_energy, {chain: share_percent}, max_noise_percent)."""
    total = None
    rows: dict[str, float] = {}
    max_noise = 0.0
    for line in open(path):
        if line.startswith("# total_energy:"):
            total = float(line.split(":")[1])
        elif line.startswith("#") or line.startswith("Raypath") or not line.strip():
            continue
        else:
            parts = line.rstrip("\n").split(",")
            if len(parts) >= 4:
                rows[parts[0]] = float(parts[1])
                max_noise = max(max_noise, float(parts[3]))
    if total is None:
        die(f"{path}: no total_energy header")
    return total, rows, max_noise


def parse_rays(spec: str) -> int:
    """CLI --rays suffix grammar (ParseRayBudget, src/main.cpp): decimal,
    1000-based (K/M/G = 1e3/1e6/1e9)."""
    mult = {"K": 1_000, "M": 1_000_000, "G": 1_000_000_000}
    s = spec.strip()
    if s and s[-1].upper() in mult:
        return int(float(s[:-1]) * mult[s[-1].upper()])
    return int(s)


def run(cmd: list[str], log: str) -> None:
    print(f"$ {' '.join(cmd)}  (log: {log})")
    with open(log, "w") as f:
        rc = subprocess.call(cmd, stdout=f, stderr=subprocess.STDOUT)
    if rc != 0:
        die(f"command failed rc={rc}: {' '.join(cmd)} (see {log})")


# ----------------------------------------------------------------------- main


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--config", required=True, help="render config json (the scene + the frame)")
    ap.add_argument("--cone", required=True, help="cone as alt,az,radius in degrees (position bearing, sun's convention)")
    ap.add_argument("--rays", default=None, help="ray budget for both arms, e.g. 4M")
    ap.add_argument("--seed", type=int, default=None)
    ap.add_argument("--filter-config", default=None, help="optional second config with the filter wired (scattering entry filter ref)")
    ap.add_argument("--chain", default=None, help="with --filter-config: the chain row to cross-check")
    ap.add_argument("--lumice", default=None, help="path to the Lumice binary (default: ../build/cmake_install/static/Lumice relative to this script)")
    ap.add_argument("--out-dir", default=None, help="scratch dir for npy/csv/log artifacts")
    ap.add_argument("--symmetry", default="PBD", help="analyze --symmetry (default PBD)")
    ap.add_argument("--mc-tol", type=float, default=0.005,
                    help="total-energy MC noise band as a fraction (default 0.005). The CSV +/- column is "
                         "per-row and does not bound the total; the reference scene's full-frame additivity "
                         "residue measured +0.08%%, so the default is a generous blind band — tighten it "
                         "with your scene's own residue as evidence.")
    ap.add_argument("--expect-frame-defect", action="store_true",
                    help="declare up front that the full-frame row is expected to be INCONSISTENT while the "
                         "dual-fisheye fold-boundary defect (doc/coordinate-convention.md §13) is unfixed; the "
                         "overall verdict is then carried by the remaining rows. Without this flag an "
                         "inconsistent full-frame row exits 1.")
    args = ap.parse_args()

    lumice = args.lumice or os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "build", "cmake_install", "static", "Lumice"
    )
    if not os.path.exists(lumice):
        die(f"Lumice binary not found at {lumice} (pass --lumice)")

    alt, az, radius = (float(x) for x in args.cone.split(","))
    if not (0.0 <= alt <= 90.0 and -180.0 <= az <= 180.0 and 0.0 < radius <= 180.0):
        die(f"cone {args.cone} out of range")
    # AltAzToDir negation -> position vector of the cone axis (sky_direction.hpp).
    a, z = math.radians(alt), math.radians(az)
    cone_pos = (math.cos(a) * math.cos(z), math.cos(a) * math.sin(z), math.sin(a))
    cos_radius = math.cos(math.radians(radius))

    cfg = json.load(open(args.config))
    renderer = cfg["render"][0]
    ltype = renderer["lens"]["type"]
    if ltype not in SUPPORTED_SINGLE | SUPPORTED_DUAL:
        die(f"lens {ltype} not supported (v1: equal-area single fisheye + dual fisheye EAR only)")
    res = (int(renderer["resolution"][0]), int(renderer["resolution"][1]))
    fov = float(renderer["lens"].get("fov", 180.0))
    if fov != 180.0:
        # The inverse mappings and the omega_pix self-check assume the
        # 180-deg hemisphere; a non-180 fov would only surface as an unrelated
        # disk-self-check failure, so guard it explicitly.
        die(f"fov {fov} not supported (this tool assumes the 180-deg hemisphere)")
    # cone must sit inside the landing domain: single EAR disk reaches 90 deg
    # from the view axis (corners reach further but a cone must not rely on
    # them); dual disks fold at the horizon.
    view = renderer.get("view", {})
    if ltype in SUPPORTED_SINGLE:
        b_alt = math.radians(view.get("elevation", 0.0))
        b_az = math.radians(view.get("azimuth", 0.0))
        cos_axis = (math.sin(b_alt) * math.sin(a) +
                    math.cos(b_alt) * math.cos(a) * math.cos(z - b_az))
        axis_angle = math.degrees(math.acos(max(-1.0, min(1.0, cos_axis))))
        if axis_angle + radius > 90.0:
            die(f"cone extends past the 90-deg disk rim (axis {axis_angle:.1f} deg + radius {radius} deg); "
                "keep the cone inside the disk so corner-region double-counting cannot enter")
    if args.rays:
        cfg["scene"]["ray_num"] = parse_rays(args.rays)

    out_dir = args.out_dir or tempfile.mkdtemp(prefix="cone_crosscheck_")
    os.makedirs(out_dir, exist_ok=True)
    cfg_path = os.path.join(out_dir, "config_full.json")
    json.dump(cfg, open(cfg_path, "w"), indent=1)

    seed_cmd = ["--seed", str(args.seed)] if args.seed is not None else []
    rays_cmd = ["--rays", args.rays] if args.rays else []

    print(f"== arm 1: analysis (cone {alt},{az},{radius}) ==")
    csv_path = os.path.join(out_dir, "analyze_cone.csv")
    run(
        [lumice, "analyze", "-f", cfg_path, "--roi", "cone", "--center", f"{alt},{az}",
         "--radius", str(radius), "--symmetry", args.symmetry, *rays_cmd, *seed_cmd, "--csv", csv_path],
        os.path.join(out_dir, "analyze.log"),
    )
    total, rows, noise = read_analyze_csv(csv_path)

    print("== arm 2: full frame ==")
    os.makedirs(os.path.join(out_dir, "full"), exist_ok=True)
    run(
        [lumice, "render", "-f", cfg_path, "-o", os.path.join(out_dir, "full"), "--format", "npy", *seed_cmd],
        os.path.join(out_dir, "full.log"),
    )
    npy = os.path.join(out_dir, "full", "img_01.npy")
    full_energy, audit = frame_cone_energy(npy, cfg, cone_pos, cos_radius)

    rows_out = []
    full_ratio = full_energy / total
    mc_tol = args.mc_tol  # see --mc-tol: per-row +/- does not bound the total; default generous on purpose
    quant = audit["cone_rim_area_bound_rel"]
    full_inconsistent = abs(full_ratio - 1.0) > mc_tol + quant
    if args.expect_frame_defect:
        if not full_inconsistent:
            print("NOTE: full-frame row is consistent — the declared fold-boundary defect no longer "
                  "reproduces; drop --expect-frame-defect")
        else:
            print(f"NOTE: full-frame ratio {full_ratio:.4f} vs the on-record defect reference "
                  "0.8107 (reference scene) — a materially different ratio means the defect grew "
                  "or changed shape, not that it reproduced as recorded")
    rows_out.append({
        "comparison": "full-frame cone Y vs analyze cone total",
        "frame": full_energy,
        "analysis": total,
        "ratio": round(full_ratio, 6),
        "verdict": ("expected-inconsistent (declared fold-boundary defect)" if full_inconsistent and args.expect_frame_defect
                    else "consistent" if not full_inconsistent else "INCONSISTENT"),
        "mc_tolerance": round(mc_tol, 5),
        "quantization_bound": round(quant, 5),
    })

    if args.filter_config:
        if not args.chain:
            die("--chain is required with --filter-config")
        fcfg = json.load(open(args.filter_config))
        if args.rays:
            fcfg["scene"]["ray_num"] = parse_rays(args.rays)
        fcfg_path = os.path.join(out_dir, "config_filter.json")
        json.dump(fcfg, open(fcfg_path, "w"), indent=1)
        print("== arm 3: filtered frame ==")
        os.makedirs(os.path.join(out_dir, "filter"), exist_ok=True)
        run(
            [lumice, "render", "-f", fcfg_path, "-o", os.path.join(out_dir, "filter"), "--format", "npy", *seed_cmd],
            os.path.join(out_dir, "filter.log"),
        )
        f_energy, _ = frame_cone_energy(os.path.join(out_dir, "filter", "img_01.npy"), fcfg, cone_pos, cos_radius)
        chain_share = rows.get(args.chain)
        if chain_share is None:
            die(f"chain {args.chain} not in analysis CSV rows: {sorted(rows)[:10]}...")
        chain_energy = chain_share / 100.0 * total
        ratio = f_energy / chain_energy
        rows_out.append({
            "comparison": f"filtered-frame cone Y vs analyze chain {args.chain}",
            "frame": f_energy,
            "analysis": chain_energy,
            "ratio": round(ratio, 6),
            "verdict": "consistent" if abs(ratio - 1.0) <= mc_tol + quant else "INCONSISTENT",
            "mc_tolerance": round(mc_tol, 5),
            "quantization_bound": round(quant, 5),
        })

    print(json.dumps({"cone": args.cone, "audit": audit, "rows": rows_out,
                      "analyze_max_row_noise_pct": noise}, indent=1))
    bad = [r for r in rows_out if r["verdict"].startswith("INCONSISTENT")]
    if bad:
        print(f"VERDICT: INCONSISTENT ({len(bad)}/{len(rows_out)} rows outside tolerance)")
        sys.exit(1)
    print("VERDICT: consistent")


if __name__ == "__main__":
    main()
