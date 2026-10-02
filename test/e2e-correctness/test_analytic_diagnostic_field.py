"""Independent consumer and oracle coverage for the general diagnostic field.

The pre-ABI test below fixes the non-reference geometry, multi-reflection and non-first-interface
coverage without loading Lumice.  The C ABI tests in this file are enabled after the field entry
point is built and compare it with the same independent oracle.

symmetry_semantics: none — every fixture names one concrete physical face sequence.
"""

from __future__ import annotations

import math

from test.e2e.diagnostic_field_oracle import exp_rotation, prism_planes, trace_path


def _find_four_face_pose() -> tuple[tuple[float, ...], object]:
    incident = (-math.cos(math.radians(15.0)), 0.0, -math.sin(math.radians(15.0)))
    for ix in range(-8, 9):
        for iy in range(-8, 9):
            for iz in range(-8, 9):
                pose = exp_rotation((0.11 * ix, 0.09 * iy, 0.13 * iz))
                result = trace_path((3, 5, 6, 7), 1.31, incident, pose)
                if result.valid:
                    return pose, result
    raise AssertionError("independent oracle found no valid four-face fixture")


def test_independent_oracle_covers_the_pre_abi_consumer_matrix() -> None:
    distances = (1.37, 0.91, 1.12, 1.46, 0.83, 1.05)
    planes = prism_planes(distances, 0.73)
    assert tuple(planes) == tuple(range(1, 9))
    assert len({round(planes[face][1], 12) for face in range(3, 9)}) == 6

    pose, traced = _find_four_face_pose()
    assert traced.valid and traced.outgoing is not None
    assert len(traced.coefficients) == 4
    assert len(traced.domain_margins) == 6
    assert len(traced.tir_margins) == 2
    assert abs(math.sqrt(sum(x * x for x in traced.outgoing)) - 1.0) < 2.0e-14
    assert pose != (1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0)

    pyramid = trace_path(
        (13, 15, 26, 28),
        1.31,
        (-0.9659258262890683, 0.0, -0.25881904510252074),
        (0.38947319055953666, -0.9206440277496026, -0.02692968630273812,
         -0.41810850781521075, -0.15067503460533604, -0.8958137695074904,
         0.8206674654573541, 0.3601549779132174, -0.44361298713678066),
        upper_wedge_deg=27.996455531220374,
        lower_wedge_deg=38.5704386184827,
    )
    assert pyramid.valid
    assert len(pyramid.coefficients) == 4 and len(pyramid.tir_margins) == 2

