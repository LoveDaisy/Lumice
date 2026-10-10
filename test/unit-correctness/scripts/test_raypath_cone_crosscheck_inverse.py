"""Synthetic-frame cross-check of `raypath_cone_crosscheck.py`'s inverse mappings.

The tool reconstructs a pixel's displayed sky direction through
`pixel_to_display_single` / `pixel_to_display_dual` and sums the frame over the
cone. Every real cone position this tool had been driven with sat on a symmetry
axis of the dual-fisheye layout (the horizon or the zenith axis), where a
mirrored or axis-swapped inverse reads back the SAME pixels on the OTHER side
and the error is structurally invisible. This test closes that gap with
synthetic frames: fingerprint energies at non-axis points spanning all four
azimuth quadrants of both dual disks (and all four camera-frame quadrants of
the single disk), with expected read-backs fixed as literals.

Design: one power-of-two fingerprint per cone, written at the cone centre, cone
radius 30 deg. The fingerprint pixel's centre direction is within a bin
diagonal (~1.8 deg) of the cone axis, so it is deep inside the cone, every
other pixel is zero, and the read-back must equal the single fingerprint
exactly (`==`, one float32 term). Any sign / axis / handedness error in the
inverse displaces the fingerprint pixel's reconstructed direction by tens of
degrees - out of the cone - and the read-back drops to 0. Radius 30 deg (not
smaller) because the tool's cone pixel self-check bounds quantization by a
rim-band expression that only clears once the cone spans enough pixels.

The forward side (displayed direction -> pixel) is transcribed here from the
engine's own semantics rather than imported, so a transcription error in the
tool cannot wash itself out: dual-fisheye EAR = display = -travel, left disk
carries the upper hemisphere, k = 1/sqrt(1 + |z|), upper disk fx = -y_norm*r +
cx / lower disk fx = +y_norm*r + cx, fy = x_norm*r + cy (r_scale = 1, overlap
0 - the tool's supported envelope); single-lens EAR adds the camera rotation
R^T and the screen handedness x negation, scale = short/2 at fov 180.
"""
from __future__ import annotations

import math
import sys
from pathlib import Path

import numpy as np
import pytest

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "scripts"))

import raypath_cone_crosscheck as rcc  # noqa: E402

CONES_ALT_AZ = [(alt, az) for alt in (40.0, -40.0) for az in (45.0, 135.0, -135.0, -45.0)]
SINGLE_CONES_ALT_AZ = [(alt, az) for alt in (30.0, -30.0) for az in (30.0, -30.0)]
CONE_RADIUS_DEG = 30.0


def _display_dir(alt_deg: float, az_deg: float) -> tuple[float, float, float]:
    ca = math.cos(math.radians(alt_deg))
    return (ca * math.cos(math.radians(az_deg)), ca * math.sin(math.radians(az_deg)),
            math.sin(math.radians(alt_deg)))


def _mat_t_vec(rot: list[list[float]], v: tuple[float, float, float]) -> tuple[float, float, float]:
    return tuple(sum(rot[j][i] * v[j] for j in range(3)) for i in range(3))  # type: ignore[return-value]


def _dual_ear_forward_pixel(display: tuple[float, float, float], res: tuple[int, int]) -> tuple[int, int]:
    sx, sy, sz = display
    is_upper = sz >= 0.0
    z_hemi = sz if is_upper else -sz
    k = 1.0 / math.sqrt(1.0 + z_hemi)
    x_norm, y_norm = k * sx, k * sy
    w, h = res
    r = min(w // 2, h) / 2.0
    cy = h / 2.0
    if is_upper:
        cx = w / 2.0 - r
        fx = -y_norm * r + cx
    else:
        cx = w / 2.0 + r
        fx = y_norm * r + cx
    fy = x_norm * r + cy
    px = min(max(int(math.floor(fx)), 0), w - 1)
    py = min(max(int(math.floor(fy)), 0), h - 1)
    return px, py


def _single_ear_forward_pixel(
    display: tuple[float, float, float], res: tuple[int, int], rot: list[list[float]]
) -> tuple[int, int]:
    cx, cy, cz = _mat_t_vec(rot, display)
    k = 1.0 / math.sqrt(1.0 + cz)
    w, h = res
    scale = min(w, h) / 2.0  # EAR fov 180: short/2/sqrt(2)/sin(45 deg) collapses to short/2
    fx = -(k * cx) * scale + w / 2.0  # screen handedness: xy.x is negated before pixel
    fy = (k * cy) * scale + h / 2.0
    return int(math.floor(fx)), int(math.floor(fy))


def _angle_deg(a: tuple[float, float, float], b: tuple[float, float, float]) -> float:
    dot = max(-1.0, min(1.0, sum(x * y for x, y in zip(a, b))))
    return math.degrees(math.acos(dot))


def _make_cfg(lens_type: str, res: tuple[int, int]) -> dict:
    return {
        "render": [{
            "lens": {"type": lens_type, "fov": 180.0},
            "resolution": [res[0], res[1]],
            "view": {"azimuth": 0.0, "elevation": 0.0, "roll": 0.0},
        }],
    }


@pytest.mark.parametrize("lens_type,res,cones", [
    ("dual_fisheye_equal_area", (256, 128), CONES_ALT_AZ),
    ("fisheye_equal_area", (256, 256), SINGLE_CONES_ALT_AZ),
])
def test_cone_crosscheck_inverse_maps_four_quadrants(lens_type: str, res: tuple[int, int],
                                                     cones: list[tuple[float, float]], tmp_path) -> None:
    cfg = _make_cfg(lens_type, res)
    w, h = res
    rot = rcc.camera_rotation(0.0, 0.0, 0.0)
    npy_path = tmp_path / "synthetic.npy"
    img = np.zeros((h, w, 3), dtype=np.float32)

    written: list[tuple[tuple[float, float, float], tuple[int, int], float]] = []
    for i, (alt, az) in enumerate(cones):
        display = _display_dir(alt, az)
        px, py = (_dual_ear_forward_pixel(display, res) if lens_type.startswith("dual")
                  else _single_ear_forward_pixel(display, res, rot))
        fingerprint = float(2 ** i)
        img[py, px, 1] = np.float32(fingerprint)
        written.append((display, (px, py), fingerprint))
    np.save(npy_path, img)

    cos_radius = math.cos(math.radians(CONE_RADIUS_DEG))
    for i, (alt, az) in enumerate(cones):
        display, (px, py), fingerprint = written[i]
        cone_pos = _display_dir(alt, az)
        energy, audit = rcc.frame_cone_energy(str(npy_path), cfg, cone_pos, cos_radius)
        # Diagnostics first: the tool's own inverse at the fingerprint pixel must
        # reconstruct a direction within one bin diagonal of the cone axis.
        d = (rcc.pixel_to_display_dual(lens_type, 180.0, res, px, py) if lens_type.startswith("dual")
             else rcc.pixel_to_display_single(lens_type, 180.0, res, cfg["render"][0]["view"], px, py))
        assert d is not None, f"cone {alt},{az}: fingerprint pixel {(px, py)} reconstructed off-disk"
        off = _angle_deg(d, display)
        assert off <= 2.5, (
            f"cone {alt},{az}: inverse at fingerprint pixel {(px, py)} is {off:.1f} deg off the cone axis "
            f"(got {d}, want ~{display}) - pixel_to_display_{ 'dual' if lens_type.startswith('dual') else 'single' } "
            "disagrees with the engine's forward mapping"
        )
        assert energy == fingerprint, (
            f"cone {alt},{az}: read-back {energy!r} != fingerprint {fingerprint!r} "
            f"(cone_pixels={audit['cone_pixels']}) - the inverse mapping loses or misplaces "
            "non-axis energy"
        )
