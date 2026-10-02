"""Independent geometry/optics oracle for the analytic diagnostic-field consumer.

This module deliberately imports no Lumice binding and mirrors no C++ helper.  It restates the
closed-form face directions and textbook Snell/Fresnel relations from the public conventions so
the ctypes consumer has an oracle outside the implementation it tests.
"""

from __future__ import annotations

import math
from dataclasses import dataclass


Vector = tuple[float, float, float]
Matrix = tuple[float, ...]


def dot(a: Vector, b: Vector) -> float:
    return sum(x * y for x, y in zip(a, b))


def mat_vec(r: Matrix, v: Vector) -> Vector:
    return tuple(sum(r[3 * i + j] * v[j] for j in range(3)) for i in range(3))  # type: ignore[return-value]


def mat_mul(a: Matrix, b: Matrix) -> Matrix:
    return tuple(sum(a[3 * i + k] * b[3 * k + j] for k in range(3)) for i in range(3) for j in range(3))


def exp_rotation(w: Vector) -> Matrix:
    theta2 = dot(w, w)
    if theta2 < 1.0e-16:
        a = 1.0 - theta2 / 6.0
        b = 0.5 - theta2 / 24.0
    else:
        theta = math.sqrt(theta2)
        a = math.sin(theta) / theta
        b = (1.0 - math.cos(theta)) / theta2
    x, y, z = w
    g = (0.0, -z, y, z, 0.0, -x, -y, x, 0.0)
    g2 = mat_mul(g, g)
    return tuple((1.0 if i % 4 == 0 else 0.0) + a * g[i] + b * g2[i] for i in range(9))


def right_perturb(pose: Matrix, delta: Vector) -> Matrix:
    return mat_mul(pose, exp_rotation(delta))


def face_normal(face: int, upper_wedge_deg: float = 0.0, lower_wedge_deg: float = 0.0) -> Vector:
    if face == 1:
        return (0.0, 0.0, 1.0)
    if face == 2:
        return (0.0, 0.0, -1.0)
    side = face % 10 - 3
    azimuth = side * math.pi / 3.0
    if 3 <= face <= 8:
        return (math.cos(azimuth), math.sin(azimuth), 0.0)
    if 13 <= face <= 18:
        wedge = math.radians(upper_wedge_deg)
        return (math.cos(wedge) * math.cos(azimuth), math.cos(wedge) * math.sin(azimuth), math.sin(wedge))
    if 23 <= face <= 28:
        wedge = math.radians(lower_wedge_deg)
        return (math.cos(wedge) * math.cos(azimuth), math.cos(wedge) * math.sin(azimuth), -math.sin(wedge))
    raise ValueError(f"unsupported face {face}")


def prism_planes(face_distance: tuple[float, ...], height: float) -> dict[int, tuple[Vector, float]]:
    """Body-frame half-spaces n.x <= offset for the eight prism faces."""
    if len(face_distance) != 6:
        raise ValueError("a prism has six side distances")
    planes: dict[int, tuple[Vector, float]] = {1: ((0.0, 0.0, 1.0), abs(height) / 2.0),
                                               2: ((0.0, 0.0, -1.0), abs(height) / 2.0)}
    for i, distance in enumerate(face_distance):
        planes[3 + i] = (face_normal(3 + i), math.sqrt(3.0) * distance / 4.0)
    return planes


def _transmission(n1: float, cos_i: float, n2: float, cos_t: float) -> float:
    rs = (n1 * cos_i - n2 * cos_t) / (n1 * cos_i + n2 * cos_t)
    rp = (n2 * cos_i - n1 * cos_t) / (n2 * cos_i + n1 * cos_t)
    return 1.0 - 0.5 * (rs * rs + rp * rp)


@dataclass(frozen=True)
class Trace:
    valid: bool
    outgoing: Vector | None
    coefficients: tuple[float, ...]
    domain_margins: tuple[float, ...]
    tir_margins: tuple[float, ...]


def trace_path(
    faces: tuple[int, ...],
    refractive_index: float,
    incident: Vector,
    pose: Matrix,
    *,
    upper_wedge_deg: float = 0.0,
    lower_wedge_deg: float = 0.0,
) -> Trace:
    """Trace one concrete path using propagation directions and outward face normals."""
    normals = tuple(mat_vec(pose, face_normal(face, upper_wedge_deg, lower_wedge_deg)) for face in faces)
    margins: list[float] = []
    coefficients: list[float] = []
    tir: list[float] = []
    n = refractive_index

    entry_cos = -dot(normals[0], incident)
    entry_disc = 1.0 - (1.0 - entry_cos * entry_cos) / (n * n)
    margins.extend((entry_cos, entry_disc))
    if not (entry_cos > 0.0 and entry_disc > 0.0):
        return Trace(False, None, tuple(coefficients), tuple(margins), tuple(tir))
    cos_t = math.sqrt(entry_disc)
    k = entry_cos / n - cos_t
    direction = tuple(incident[i] / n + k * normals[0][i] for i in range(3))
    coefficients.append(_transmission(1.0, entry_cos, n, cos_t))

    for normal in normals[1:-1]:
        cos_i = dot(normal, direction)
        margins.append(cos_i)
        disc = 1.0 - n * n * (1.0 - cos_i * cos_i)
        tir.append(disc)
        if not cos_i > 0.0:
            return Trace(False, None, tuple(coefficients), tuple(margins), tuple(tir))
        if disc <= 0.0:
            coefficients.append(1.0)
        else:
            coefficients.append(1.0 - _transmission(n, cos_i, 1.0, math.sqrt(disc)))
        direction = tuple(direction[i] - 2.0 * cos_i * normal[i] for i in range(3))

    exit_cos = dot(normals[-1], direction)
    exit_disc = 1.0 - n * n * (1.0 - exit_cos * exit_cos)
    margins.extend((exit_cos, exit_disc))
    if not (exit_cos > 0.0 and exit_disc > 0.0):
        return Trace(False, None, tuple(coefficients), tuple(margins), tuple(tir))
    cos_out = math.sqrt(exit_disc)
    k = n * exit_cos - cos_out
    outgoing = tuple(n * direction[i] - k * normals[-1][i] for i in range(3))
    coefficients.append(_transmission(n, exit_cos, 1.0, cos_out))
    return Trace(True, outgoing, tuple(coefficients), tuple(margins), tuple(tir))

