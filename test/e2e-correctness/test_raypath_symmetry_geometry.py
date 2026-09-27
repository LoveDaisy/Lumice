"""`Lumice analyze --symmetry` merges only raypaths the crystal's SHAPE makes equivalent.

The P/B/D reduction used to assume every prism and pyramid is a regular hexagonal crystal (full
D6h), whatever its ``face_distance`` or cones. On ``face_distance [1, 1.2, 1, 1.2, 1, 1.2]`` — a
three-fold axis, not six-fold — ``--symmetry P`` then folded near-face and far-face paths, rows
more than two times apart in energy, into one.

Each case runs one seeded scene twice: ``--symmetry none`` gives the finest rows (the truth), and
the reduced read gives the merged ones. A seeded run is single-worker and deterministic, so the
two reads see the same rays. The oracle is a Python orbit computation over the symmetry group the
case names BY HAND from the crystal's shape — never read back from the engine — so each reduced
row must carry exactly the energy of the finest rows in its true orbit: a row that merged an
inequivalent path is too heavy, a row that failed to merge an equivalent one too light.

Cases:
  * three-fold prism under P, D and PBD: allowed rotations are by two faces, allowed mirrors
    the even ones (``sigma_a`` is 0 for this axis), B holds;
  * a pyramid with unequal wedge angles (upper {1,0,1}, lower {2,0,3}) under B and PBD: the full
    six-fold section, but B would swap unlike cones and must not act;
  * the regular prism under PBD: the full group, i.e. the behavior before shape was consulted.
"""

from __future__ import annotations

from test.e2e.base import LumiceTestCase
from test.e2e.runner import get_project_root

CONFIGS_DIR = get_project_root() / "test" / "e2e" / "configs"
_SEED = "20260911"
# The CSV prints each share to 1e-4 %; a sum of a few rounded shares is off by at most that
# many half-units.
_SHARE_TOL = 2e-3


def _rows(text: str):
    """(raypath tuple, energy %) per data row of the analyze CSV. Every chain must have been
    recorded exactly — an `other` bucket would hide energy from the orbit sums."""
    lines = text.splitlines()
    assert "# record_full_hits: 0" in lines, "the chain record overflowed; the scene is too deep"
    header = lines.index("Raypath,Energy,Cumulative %,+/-")
    rows = []
    for line in lines[header + 1:]:
        label, share = line.split(",")[:2]
        rows.append((tuple(int(f) for f in label.split("-")), float(share)))
    return rows


def _rotation(k, b=False):
    return (tuple((i + k) % 6 for i in range(6)), b)


def _mirror(a, b=False):
    return (tuple((a - i) % 6 for i in range(6)), b)


def _apply(elem, face):
    perm, b = elem
    if face <= 2:
        return 3 - face if b else face
    pyr, pri = divmod(face, 10)
    if b and pyr in (1, 2):
        pyr = 3 - pyr
    return pyr * 10 + perm[pri - 3] + 3


def _group(generators):
    identity = _rotation(0)
    group = {identity}
    frontier = [identity]
    while frontier:
        h = frontier.pop()
        for g in generators:
            composed = (tuple(g[0][h[0][i]] for i in range(6)), g[1] != h[1])
            if composed not in group:
                group.add(composed)
                frontier.append(composed)
    return group


def _orbit_key(raypath, group):
    return min(tuple(_apply(g, f) for f in raypath) for g in group)


class TestRaypathSymmetryFollowsShape(LumiceTestCase):
    def _read(self, config, symmetry):
        result = self.run_lumice(["analyze", "-f", str(CONFIGS_DIR / config), "--seed", _SEED,
                                  "--symmetry", symmetry, "--chain-capacity", "1000000"], timeout=180)
        self.assertEqual(result.returncode, 0, result.stderr)
        return _rows(result.stdout)

    def _assert_rows_are_true_orbits(self, config, symmetry, generators):
        finest = self._read(config, "none")
        reduced = self._read(config, symmetry)
        group = _group(generators)
        truth = {}
        for raypath, share in finest:
            key = _orbit_key(raypath, group)
            truth[key] = truth.get(key, 0.0) + share
        self.assertEqual(len(reduced), len(truth),
                         f"{config} --symmetry {symmetry}: {len(reduced)} rows, {len(truth)} true orbits")
        for raypath, share in reduced:
            key = _orbit_key(raypath, group)
            self.assertIn(key, truth, f"{config} --symmetry {symmetry}: row {raypath} names no finest orbit")
            self.assertAlmostEqual(share, truth[key], delta=_SHARE_TOL,
                                   msg=f"{config} --symmetry {symmetry}: row {raypath} carries {share}%, "
                                       f"its true orbit {truth[key]:.4f}%")
        return finest

    def test_three_fold_prism(self):
        config = "raypath_symmetry_three_fold_prism.json"
        p = [_rotation(2)]
        d = [_mirror(0)]
        b = [_rotation(0, True)]
        finest = self._assert_rows_are_true_orbits(config, "P", p)
        self._assert_rows_are_true_orbits(config, "D", d)
        self._assert_rows_are_true_orbits(config, "PBD", p + b + d)
        # Not vacuous: the two buckets a 60° rotation would fold are really different, the
        # near-face 22° path well above the far-face one (the backlog measured 2.4x).
        share = dict(finest)
        near = share[(3, 5)] + share[(5, 7)] + share[(7, 3)]
        far = share[(4, 6)] + share[(6, 8)] + share[(8, 4)]
        self.assertGreater(near, 1.5 * far, f"near {near}% vs far {far}%")

    def test_pyramid_with_unequal_cones_keeps_b_off(self):
        config = "raypath_symmetry_unequal_cones_pyramid.json"
        p = [_rotation(1)]
        d = [_mirror(0)]
        finest = self._assert_rows_are_true_orbits(config, "B", [])
        self._assert_rows_are_true_orbits(config, "PBD", p + d)
        # Not vacuous: the scene does send light through both cones.
        faces = {f for raypath, _ in finest for f in raypath}
        self.assertTrue(faces & set(range(13, 19)) and faces & set(range(23, 29)), sorted(faces))

    def test_regular_prism_keeps_the_full_group(self):
        generators = [_rotation(1), _mirror(0), _rotation(0, True)]
        self._assert_rows_are_true_orbits("raypath_analysis_halo_22.json", "PBD", generators)


def test_group_helper_sizes():
    """The oracle's own arithmetic: the groups the cases above use have the orders D6h theory
    gives (full 24; three-fold with even mirrors and B 12; rotations alone 6 and 3)."""
    assert len(_group([_rotation(1), _mirror(0), _rotation(0, True)])) == 24
    assert len(_group([_rotation(2), _mirror(0), _rotation(0, True)])) == 12
    assert len(_group([_rotation(1)])) == 6
    assert len(_group([_rotation(2)])) == 3
