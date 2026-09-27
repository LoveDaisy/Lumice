"""A filter's P/B/D is a label equivalence: it matches the same raypaths it matched in v4.7.0.

P relabels the prism faces by any multiple of 60 degrees and B swaps 1<->2 and the upper and
lower cones, whatever the crystal's shape and orientation distribution; D mirrors in the plane the
roll mean selects. That is the filter's meaning a saved document relies on. For a while the filter
reduction followed the crystal's physical symmetry instead (the raypath-analysis list's meaning),
and every filter written that way on a low-symmetry crystal silently matched less, or nothing.

Each case binds one ``filter_in`` raypath filter to one crystal and lists, with ``analyze
--symmetry none``, every chain the filter let through. Two things are asserted:

  * every row is in the filter raypath's LABEL orbit, computed here by hand (never read back from
    the engine);
  * the rows are exactly the set v4.7.0 produced for the same seeded run. The sets were captured
    once from a v4.7.0 build, whose rows were byte-identical to this build's on every case; only the
    raypath labels are frozen here, not the energies, so the check holds on any platform. Each
    set holds a path the physical reduction would have dropped, so a filter that narrows to the
    crystal's own symmetry again goes red here.

Cases (all the crystal ensembles a physical reduction treats differently from the label one):
  * three-fold prism + P: the far-face rotations 4-6, 6-8, 8-4 are matched alongside 3-5;
  * rhombic prism [2, 1, 1, 2, 1, 1] + P: the filter names absent faces (3) and still matches
    their present label images;
  * pyramid with unequal cones + B: the lower-cone image 23-6 is matched;
  * a shape lacking the roll-selected mirror + D: the mirror image 8-6 is matched;
  * a regular prism with a locked roll (Parry) + P: the rotated 5-7 is matched.
"""

from __future__ import annotations

from test.e2e.base import LumiceTestCase
from test.e2e.runner import get_project_root

CONFIGS_DIR = get_project_root() / "test" / "e2e" / "configs"
_SEED = "7"


def _rows(text: str):
    lines = text.splitlines()
    header = lines.index("Raypath,Energy,Cumulative %,+/-")
    return [line.split(",")[0] for line in lines[header + 1:]]


def _label_orbit(raypath, p=False, b=False, d_sigma=None):
    """Every relabelling of `raypath` under the label group the bits generate."""
    def rotate(face, k):
        if face <= 2:
            return face
        pyr, pri = divmod(face, 10)
        return pyr * 10 + (pri - 3 + k) % 6 + 3

    def mirror(face, a):
        if face <= 2:
            return face
        pyr, pri = divmod(face, 10)
        return pyr * 10 + (a - (pri - 3)) % 6 + 3

    def flip(face):
        if face <= 2:
            return 3 - face
        pyr, pri = divmod(face, 10)
        return (3 - pyr) * 10 + pri if pyr in (1, 2) else face

    orbit = {tuple(raypath)}
    if p:
        orbit |= {tuple(rotate(f, k) for f in rp) for rp in orbit for k in range(6)}
    if d_sigma is not None:
        orbit |= {tuple(mirror(f, d_sigma) for f in rp) for rp in orbit}
    if b:
        orbit |= {tuple(flip(f) for f in rp) for rp in orbit}
    return {"-".join(str(f) for f in rp) for rp in orbit}


class TestFilterSymmetryIsALabelEquivalence(LumiceTestCase):
    def _assert_matches(self, config, orbit, v470_rows):
        result = self.run_lumice(["analyze", "-f", str(CONFIGS_DIR / config), "--seed", _SEED, "--symmetry", "none"],
                                 timeout=180)
        self.assertEqual(result.returncode, 0, result.stderr)
        rows = set(_rows(result.stdout))
        self.assertTrue(rows <= orbit, f"{config}: rows outside the filter's label orbit: {sorted(rows - orbit)}")
        self.assertEqual(rows, v470_rows, f"{config}: v4.7.0 matched {sorted(v470_rows)}")

    def test_three_fold_prism_p(self):
        self._assert_matches("filter_label_symmetry_three_fold_p.json", _label_orbit([3, 5], p=True),
                             {"3-5", "5-7", "7-3", "4-6", "6-8", "8-4"})

    def test_rhombic_prism_p_through_an_absent_face(self):
        self._assert_matches("filter_label_symmetry_rhombic_p.json", _label_orbit([3, 5], p=True), {"5-7", "8-4"})

    def test_unequal_cones_b(self):
        self._assert_matches("filter_label_symmetry_unequal_cones_b.json", _label_orbit([13, 6], b=True),
                             {"13-6", "23-6"})

    def test_d_on_a_shape_without_that_mirror(self):
        # Roll mean 30 with a uniform azimuth: sigma_a 5, which sends prism index i to 5 - i.
        self._assert_matches("filter_label_symmetry_d_on.json", _label_orbit([3, 5], d_sigma=5), {"3-5", "8-6"})

    def test_parry_p(self):
        self._assert_matches("filter_label_symmetry_parry_p.json", _label_orbit([3, 5], p=True), {"3-5", "5-7"})

    def test_three_crystal_challenge_every_filter_matches(self):
        """The beta document these filters come from: three crystals whose face_distance leaves
        faces 3, 5, 7 (or 3, 6) without area, each with a PBD filter naming some of those faces.
        Each crystal's filter lets rays through, as in v4.7.0 — none of its rows is empty."""
        result = self.run_lumice(["analyze", "-f", str(CONFIGS_DIR / "raypath_analysis_challenge_98_120_144.json"),
                                  "--seed", "20260911", "--rays", "2M", "--symmetry", "none"], timeout=300)
        self.assertEqual(result.returncode, 0, result.stderr)
        rows = _rows(result.stdout)
        for crystal in ("C1(", "C2(", "C3("):
            self.assertTrue(any(r.startswith(crystal) for r in rows), f"no row for {crystal}: {rows}")


def test_label_orbit_helper_sizes():
    """The oracle's own arithmetic: P alone gives six labels, P with B on a cone path twelve."""
    assert len(_label_orbit([3, 5], p=True)) == 6
    assert len(_label_orbit([13, 5], p=True, b=True)) == 12
    assert _label_orbit([3, 5], d_sigma=5) == {"3-5", "8-6"}
