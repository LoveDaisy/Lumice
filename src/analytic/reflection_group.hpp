#ifndef LUMICE_ANALYTIC_REFLECTION_GROUP_HPP_
#define LUMICE_ANALYTIC_REFLECTION_GROUP_HPP_

// The two group-theoretic predicates of the focusing / chromatic layers (LI
// symmetry/reflection_group.py, ported for scrum 660.4) — and only those two. The full LI module
// (the D3h 12-element authority table, conjugacy classes, eigenvalue classes, ray-path enumeration)
// has no consumer in this kernel and is deliberately NOT ported: the fold matrix of a face
// sequence already has its single authority here (dp_field.hpp FoldMatrixOf, LI
// geometry.fold_matrix — 660.1 established that identification), and a second reflection-product
// loop in a group table would be exactly the divergence that constant exists to prevent (a56).
//
// Ported:
//  - CommutesWithRotationAbout: M commutes with the rotations about an axis, decided by a
//    Rodrigues probe at a deliberately non-special angle (LI commutes_with_rotation_about; the
//    family_pinned criterion of the focusing layer, LI focusing.py 52.3).
//  - PbdOrbit: the Lumice "PBD" filter orbit of a face sequence as label arithmetic (LI
//    pbd_orbit): D6 on the side-face rings x the top-bottom swap, twelve images each possibly
//    swapped, deduplicated. Label semantics (L1, LI conventions #21 / raypath-symmetry.md 1.1):
//    the orbit is a context-free combination on face numbers and deliberately merges paths that
//    are not physically equivalent when the crystal lacks the symmetry element — the rhombic
//    plate carries a class whose literal representative it cannot even realise. Intersecting
//    with physical equivalence (L2) belongs to the analysis list, not here.
//
// Internal header of the analytic kernel: nothing here is part of the C ABI.

#include <vector>

namespace lumice::analytic {

// Whether the 3x3 row-major matrix `m` commutes with every rotation about `axis` (unit or not),
// decided by one probe rotation at `probe_deg` (LI commutes_with_rotation_about, default 37 deg).
// A true commutation holds for ALL angles, so one non-special probe settles it; special angles
// (0, 90, 180 deg) hit larger centralizers ({I, R_a(pi)} admits every mirror whose plane contains
// a, {I, R_a(+-90 deg)} more) and would misjudge reflections — the probe angle is a correctness
// parameter, not a knob. Comparison is numpy allclose's default (rtol 1e-5, atol 1e-8, element
// |a - b| <= atol + rtol |b|), LI's own choice, kept verbatim.
bool CommutesWithRotationAbout(const double m[9], const double axis[3], double probe_deg = 37.0);

// The PBD orbit of `faces` (count entries): every image under D6's twelve elements (rotations
// n -> n + shift and flips n -> shift - n on each side-face ring 3..8 / 13..18 / 23..28, basal
// faces 1 / 2 fixed) and under the top-bottom swap (1 <-> 2, 13+i <-> 23+i, sides unchanged) of
// each image, deduplicated and sorted lexicographically — the members of a path class (LI
// pbd_orbit, L1 label semantics). Returns false without touching `out` when a face number is
// outside the supported rings (the analytic layer's own face universe); the caller fails closed.
bool PbdOrbit(const int* faces, int count, std::vector<std::vector<int>>* out);

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_REFLECTION_GROUP_HPP_
