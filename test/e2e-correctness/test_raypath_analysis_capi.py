"""The analysis run, end to end through the C API (lumice.h "Raypath Analysis Run").

The scene is the 22° halo of ``test/e2e/configs/halo_22.json`` — same crystal, same sun —
with a 200k-ray budget (``raypath_analysis_halo_22.json``), so the top chain the C++ white-box
test already pins on the consumer (``test_raypath_histogram_consumer.cpp``) is read here
through the full stack: JSON commit, the forced-CPU session, the frame getters, the ctypes
mirrors.

AC3, three assertions on one scene:
  * full sky: the top chain is the 22° raypath, ``3-5``;
  * a 2° cone centred on the 22° ring (straight above the sun): the same top chain;
  * the same cone pointed at halo-free sky (altitude 85°, above both the 22° and the 46°
    rings): the top chain's energy is far below the on-ring cone's — the measured ratio and
    the threshold derived from it are at the assertion.
AC1 through ctypes: a render in progress refuses the analysis with LUMICE_ERR_SERVER.
The scene is the call's own (v4.36): every run above starts on a server that never committed
anything, and ``test_analysis_needs_no_commit_and_the_commit_after_it_renders`` says so
explicitly — analysis first, then a commit that renders on its own budget.
AC2 through ctypes: under a Metal preference the run reports the CPU and logs the forcing.
The read-time symmetry (v4.33): the same run read under none / P / P|B / P|B|D conserves the
sums, never gains rows as bits are added, and the 22° path's ORBIT — the whole of it at the
finest, one row under P|B|D — carries the same energy at every one of them.
The fixed-seed contract across sessions: ten analyses back to back on ONE server (the GUI's
shape) reproduce each other row for row, on the 22° scene and on a plate+column two-layer
scene (``raypath_analysis_pc_two_layer.json``).

Requires the shared-lib build (``./scripts/build.sh -sj release``); run with
``pytest -v -m slow``.
"""

from __future__ import annotations

import ctypes
import json
import math
import statistics
import warnings
from pathlib import Path

import pytest

from test.e2e import capi_runner as cr
from test.e2e.runner import get_project_root

_CONFIG = str(get_project_root() / "test" / "e2e" / "configs" / "raypath_analysis_halo_22.json")
# Plate + column, two scattering layers each holding both crystals, ms_prob 0.3, the same 200k
# budget: the multi-layer scene the fixed-seed determinism contract is checked on beside the
# single-layer halo above (its finest table is several thousand rows and its chains cross layers,
# so a session-to-session drift that the 22° scene's rows happened to hide would show here).
_CONFIG_PC_TWO_LAYER = str(get_project_root() / "test" / "e2e" / "configs" / "raypath_analysis_pc_two_layer.json")

# The direction light travels for a sun at (altitude, azimuth) is the antipode of where the
# sun sits — the convention every direction in the C API uses (lumice.h, the marker family).
_SUN_ALTITUDE_DEG = 20.0
# Where the 22° halo's energy sits, straight above the sun: the ring's minimum deviation is
# 21.8°, its energy piles up just outside it, so +23° lands on the bright edge.
_ON_RING_ALTITUDE_DEG = _SUN_ALTITUDE_DEG + 23.0
# Above the 46° ring's top edge (20 + 46 = 66°) and every other arc of a randomly oriented
# prism: nothing but scattered residue lands here.
_OFF_RING_ALTITUDE_DEG = 85.0

_HALO_22 = "3-5"

# A fixed seed makes every run below a single-worker deterministic run (the seed contract),
# so the three energies are comparable and the ratio is a number, not a distribution.
_SEED = 20260911


def _sunlight_dir(altitude_deg: float, azimuth_deg: float = 0.0):
    lon = math.radians(azimuth_deg + 180.0)
    lat = math.radians(-altitude_deg)
    return (math.cos(lat) * math.cos(lon), math.cos(lat) * math.sin(lon), math.sin(lat))


def _full_sky_request() -> cr.LUMICE_RaypathAnalysisRequest:
    req = cr.LUMICE_RaypathAnalysisRequest()
    req.roi_mode = cr.LUMICE_RAYPATH_ROI_FULL_SKY
    # The scene's own ray budget, as every request here asked for before v4.32 gave the request
    # one of its own: a zero-initialized `infinite` would be a budget of zero rays.
    req.infinite = cr.LUMICE_RAYPATH_RAY_BUDGET_SCENE_DEFAULT
    return req


def _cone_request(altitude_deg: float, radius_deg: float = 2.0, rings: int = 4) -> cr.LUMICE_RaypathAnalysisRequest:
    req = _full_sky_request()
    req.roi_mode = cr.LUMICE_RAYPATH_ROI_CONE
    x, y, z = _sunlight_dir(altitude_deg)
    req.cone_center[0] = x
    req.cone_center[1] = y
    req.cone_center[2] = z
    req.cone_radius_rad = math.radians(radius_deg)
    req.cone_ring_count = rings
    return req


@pytest.mark.slow
def test_full_sky_top_chain_is_the_22_degree_path():
    r = cr.run_raypath_analysis_capi(_CONFIG, _full_sky_request(), sim_seed=_SEED)
    assert r.roi_mode == cr.LUMICE_RAYPATH_ROI_FULL_SKY
    assert r.sim_ray_num == 200000
    assert r.active_backend == cr.LUMICE_BACKEND_CPU
    assert len(r.entries) >= 2
    top = r.entries[0]
    assert top.display == _HALO_22, [e.display for e in r.entries[:5]]
    assert top.chain == [(1, [3, 5])]
    assert top.ring_energy == []
    # Sorted, and every counted ray is one the run traced (max_hits 7 bounds the fan-out).
    energies = [e.energy for e in r.entries]
    assert energies == sorted(energies, reverse=True)
    assert sum(e.count for e in r.entries) <= r.sim_ray_num * 8
    # The undeviated pass through opposite prism faces is #2, at roughly 80% of the halo:
    # measured 1.25x / 1.27x / 1.24x over the same three seeds (the C++ white-box test reads
    # 1.24x on its own run). A 1.1x floor is far from the ~52k-ray count's noise and still
    # catches a reordering.
    assert r.entries[1].display == "3-6"
    assert top.energy > 1.1 * r.entries[1].energy


def _is_22_degree_path(chain) -> bool:
    """A single-layer chain through two prism faces two apart (60° prism angle): the 22° halo.

    A physical statement about the hexagonal prism (faces 3..8 around it), NOT a re-derivation of
    core's reduction rule: every member of the orbit core folds into "3-5" under P/B/D is such a
    pair, and so is the mirror-image family "3-7" that only D folds in.
    """
    if len(chain) != 1:
        return False
    _crystal, faces = chain[0]
    if len(faces) != 2 or not all(3 <= f <= 8 for f in faces):
        return False
    return (faces[0] - faces[1]) % 6 in (2, 4)


def _is_undeviated_pass(chain) -> bool:
    """A single-layer chain through two OPPOSITE prism faces: the sun's own image, undeviated."""
    if len(chain) != 1:
        return False
    _crystal, faces = chain[0]
    if len(faces) != 2 or not all(3 <= f <= 8 for f in faces):
        return False
    return (faces[0] - faces[1]) % 6 == 3


def _fingerprint(r: cr.RaypathAnalysisResult):
    """Everything the fixed-seed contract promises to reproduce: the total, the row count, every
    row's identity/energy/count, the unrecorded remainder, and the budget the stats saw."""
    return (
        sum(e.energy for e in r.entries) + r.other_energy,
        len(r.entries),
        tuple((e.display, e.energy, e.count) for e in r.entries),
        (r.other_energy, r.other_count),
        r.sim_ray_num,
    )


@pytest.mark.slow
@pytest.mark.parametrize("config", [_CONFIG, _CONFIG_PC_TWO_LAYER], ids=["halo_22", "pc_two_layer"])
def test_fixed_seed_analysis_is_bit_identical_across_ten_sessions_of_one_server(config):
    """Ten analyses of one scene on ONE server under a fixed seed are the same analysis, ten times.

    The seed contract (a non-zero sim_seed sizes the server to one worker so the run is
    deterministic) is stated per run, and the GUI keeps one server for the life of the window —
    so it has to hold for the tenth session as much as for the first. Compared with ``==`` on
    the doubles, no tolerance: determinism, not precision. Read at the finest (symmetry 0) so
    a drift in any single chain is visible, not folded into an orbit with its neighbours.

    The defect this pins was deterministic — the worker's RNG was seeded at construction only,
    so every session after the first continued the stream from where the previous one had
    left it and every one of the ten totals differed — which is why ten sessions is coverage
    of the GUI's shape rather than a sample size. `sim_seed=0` (several workers) is out of
    the contract and not asserted on.
    """
    results = cr.run_raypath_analysis_capi_sessions(
        config, _full_sky_request(), sessions=10, sim_seed=_SEED, max_entries=None, chain_id_symmetry=0)
    assert len(results) == 10
    first = _fingerprint(results[0])
    assert first[4] == 200000, "the scene's own budget"
    assert first[1] > 0
    for i, r in enumerate(results[1:], start=2):
        fp = _fingerprint(r)
        assert fp[0] == first[0], f"session {i}: total energy {fp[0]!r} != session 1's {first[0]!r}"
        assert fp[1] == first[1], f"session {i}: {fp[1]} rows != session 1's {first[1]}"
        assert fp == first, f"session {i}: rows differ from session 1's"


@pytest.mark.slow
def test_read_time_symmetry_conserves_sums_and_the_22_degree_orbit_leads():
    """One run per symmetry (the seed makes them the same run), read under that symmetry.

    Expected row counts of the 22° orbit, from the prism's geometry: at the finest the orbit
    is 6 rotations x 2 mirror images = 12 rows; P folds the rotations (2 rows: "3-5" and its
    mirror "3-7"); B touches no prism-only path (still 2); D folds the mirror (1 row).

    The claim is about the orbit's UNION, not about the top row: the undeviated pass "3-6" is
    its own mirror image, so its orbit has 6 finest members to the halo's 12, and at the finest
    (and under P, where the halo is still split in two) a single undeviated row outweighs a
    single halo row. Only under D does "3-5" lead on its own — the C++ white-box test says the
    same. What holds at every symmetry is that the halo's rows together carry more than the
    undeviated pass's rows together, which is what the P|B|D order says.
    """
    by_sym = {}
    for sym in (0, cr.LUMICE_RAYPATH_SYMMETRY_P,
                cr.LUMICE_RAYPATH_SYMMETRY_P | cr.LUMICE_RAYPATH_SYMMETRY_B, cr.LUMICE_RAYPATH_SYMMETRY_ALL):
        # Every row: the unreduced read of this run has thousands, past the runner's default cap.
        by_sym[sym] = cr.run_raypath_analysis_capi(_CONFIG, _full_sky_request(), sim_seed=_SEED,
                                                   chain_id_symmetry=sym, max_entries=None)
    pbd = by_sym[cr.LUMICE_RAYPATH_SYMMETRY_ALL]
    total_count = sum(e.count for e in pbd.entries)
    total_energy = sum(e.energy for e in pbd.entries)
    prev_rows = None
    for sym, r in by_sym.items():
        assert r.sim_ray_num == pbd.sim_ray_num, sym
        assert sum(e.count for e in r.entries) == total_count, sym
        assert math.isclose(sum(e.energy for e in r.entries), total_energy, rel_tol=1e-9), sym
        if prev_rows is not None:
            assert len(r.entries) <= prev_rows, f"symmetry {sym}: rows grew as bits were added"
        prev_rows = len(r.entries)
        # The orbit's rows, at this symmetry: the union carries what the one P|B|D row carries,
        # and it is the leading orbit — ahead of the undeviated pass's rows together.
        orbit = [e for e in r.entries if _is_22_degree_path(e.chain)]
        assert sum(e.count for e in orbit) == pbd.entries[0].count, (sym, [e.display for e in orbit])
        assert math.isclose(sum(e.energy for e in orbit), pbd.entries[0].energy, rel_tol=1e-9), sym
        undeviated = [e for e in r.entries if _is_undeviated_pass(e.chain)]
        assert sum(e.energy for e in orbit) > 1.1 * sum(e.energy for e in undeviated), sym
        # And the top row is a member of one of those two orbits at every symmetry.
        assert _is_22_degree_path(r.entries[0].chain) or _is_undeviated_pass(r.entries[0].chain), (
            sym, r.entries[0].display)
        expected_orbit_rows = {0: 12, cr.LUMICE_RAYPATH_SYMMETRY_P: 2,
                               cr.LUMICE_RAYPATH_SYMMETRY_P | cr.LUMICE_RAYPATH_SYMMETRY_B: 2,
                               cr.LUMICE_RAYPATH_SYMMETRY_ALL: 1}[sym]
        assert len(orbit) == expected_orbit_rows, (sym, [e.display for e in orbit])
    # The display text follows the read: the single-crystal single-layer shape, no crystal prefix.
    assert pbd.entries[0].display == _HALO_22
    finest = by_sym[0]
    assert {e.display for e in finest.entries if _is_22_degree_path(e.chain)} >= {"3-5", "4-6", "3-7"}
    assert {e.display for e in by_sym[cr.LUMICE_RAYPATH_SYMMETRY_P].entries if _is_22_degree_path(e.chain)} == {
        "3-5", "3-7"}


@pytest.mark.slow
def test_cone_on_and_off_the_22_degree_ring():
    on_ring = cr.run_raypath_analysis_capi(_CONFIG, _cone_request(_ON_RING_ALTITUDE_DEG), sim_seed=_SEED)
    assert on_ring.roi_mode == cr.LUMICE_RAYPATH_ROI_CONE
    assert on_ring.entries, "the cone on the ring must count rays"
    top = on_ring.entries[0]
    assert top.display == _HALO_22, [e.display for e in on_ring.entries[:5]]
    assert len(top.ring_energy) == 4
    assert math.isclose(sum(top.ring_energy), top.energy, rel_tol=1e-9)
    assert all(e.display != "3-6" for e in on_ring.entries), "the sun's image cannot land 23° from the sun"

    off_ring = cr.run_raypath_analysis_capi(_CONFIG, _cone_request(_OFF_RING_ALTITUDE_DEG), sim_seed=_SEED)
    off_top_energy = off_ring.entries[0].energy if off_ring.entries else 0.0
    off_display = off_ring.entries[0].display if off_ring.entries else "(nothing)"
    # Measured at this budget over three seeds (20260911 / 1 / 7): the on-ring cone's top
    # chain carries 67x / 41x / 44x the energy of whatever leads the halo-free cone (a
    # scattered 1-3-2 residue, 11-16 rays) — the same solid angle on both sides, so
    # this is a per-steradian comparison too. 10x sits 4x under the weakest of those, far
    # from the 11-ray count's own noise, and a wrong centre convention or a broken membership
    # test would miss it by more than an order of magnitude in the other direction.
    assert top.energy > 10.0 * off_top_energy, (
        f"on-ring {top.display} energy {top.energy:.4g} vs off-ring {off_display} energy {off_top_energy:.4g}"
    )


@pytest.mark.slow
def test_render_in_progress_refuses_the_analysis():
    lib = cr._load_lib()
    server = lib.LUMICE_CreateServer()
    assert server
    try:
        cr._commit_config(lib, server, _CONFIG)  # starts a render run; 200k rays is in progress for a while
        req = _full_sky_request()
        scene = ctypes.c_void_p()
        assert lib.LUMICE_SceneFromJsonFile(_CONFIG.encode("utf-8"), ctypes.byref(scene)) == 0
        try:
            err = lib.LUMICE_StartRaypathAnalysis(server, scene, ctypes.byref(req))
            assert err == 7, f"expected LUMICE_ERR_SERVER (7) while the render runs, got {err}"
            lib.LUMICE_StopServer(server)
            assert lib.LUMICE_StartRaypathAnalysis(server, scene, ctypes.byref(req)) == 0
            # And the other direction: a commit over the running analysis.
            assert lib.LUMICE_CommitScene(server, scene, None) == 7
        finally:
            lib.LUMICE_SceneDestroy(scene)
    finally:
        lib.LUMICE_DestroyServer(server)


@pytest.mark.slow
def test_analysis_needs_no_commit_and_the_commit_after_it_renders():
    """AC4 (v4.36): a server that never committed analyses; the commit after it renders."""
    lib = cr._load_lib()
    server = lib.LUMICE_CreateServer()
    assert server
    try:
        drain = cr.LUMICE_DrainResult()
        assert lib.LUMICE_GetDrainStatus(server, ctypes.byref(drain)) == 0
        assert drain.current_epoch == 0, "positive control: nothing submitted yet"
        cr._start_raypath_analysis(lib, server, _CONFIG, _full_sky_request())
        assert lib.LUMICE_GetDrainStatus(server, ctypes.byref(drain)) == 0
        assert drain.current_epoch == 1, "the analysis is a submission of its own: it minted the epoch"
        cr._wait_drained(lib, server, 180)
        info = cr.LUMICE_RaypathAnalysisInfo()
        with cr._result_frame(lib, server) as frame:
            assert lib.LUMICE_FrameGetRaypathAnalysisInfo(frame, cr.LUMICE_RAYPATH_SYMMETRY_ALL, ctypes.byref(info)) == 0
        assert info.present == 1 and info.entry_count >= 1
        # The commit after it: a render, with an image and no histogram, at the next epoch.
        cr._commit_config(lib, server, _CONFIG)
        assert lib.LUMICE_GetDrainStatus(server, ctypes.byref(drain)) == 0
        assert drain.current_epoch == 2
        cr._wait_drained(lib, server, 180)
        with cr._result_frame(lib, server) as frame:
            assert lib.LUMICE_FrameGetRaypathAnalysisInfo(frame, cr.LUMICE_RAYPATH_SYMMETRY_ALL, ctypes.byref(info)) == 0
            assert info.present == 0, "a render frame carries no histogram"
            renders = (cr.LUMICE_RenderResult * 2)()
            assert lib.LUMICE_FrameGetRender(frame, renders, 1) == 0
            assert renders[0].img_buffer, "the render's image"
    finally:
        lib.LUMICE_DestroyServer(server)


@pytest.mark.slow
def test_analysis_forces_cpu_under_a_metal_preference():
    r = cr.run_raypath_analysis_capi(
        _CONFIG, _full_sky_request(), sim_seed=_SEED, preferred_backend=cr.LUMICE_BACKEND_METAL
    )
    assert r.active_backend == cr.LUMICE_BACKEND_CPU
    assert any("forcing CPU route" in line for line in r.log_lines), r.log_lines[-10:]
    # And it really ran on the CPU: only that path carries the chain ids the histogram needs.
    assert r.entries and r.entries[0].display == _HALO_22


@pytest.mark.slow
def test_unproject_pixel_round_trip_through_ctypes():
    """The ctypes binding of LUMICE_UnprojectPixel (the exact inverse is pinned in C++)."""
    lib = cr._load_lib()
    view = cr.LUMICE_AnnotationView(width=256, height=192, lens_type=1, lens_fov=120.0,
                                    view_elevation=20.0, visible=2)
    out = (ctypes.c_float * 3)()
    valid = ctypes.c_int(-1)
    assert lib.LUMICE_UnprojectPixel(ctypes.byref(view), 128, 96, out, ctypes.byref(valid)) == 0
    assert valid.value == 1
    x, y, z = out[0], out[1], out[2]
    assert math.isclose(math.sqrt(x * x + y * y + z * z), 1.0, abs_tol=1e-5)
    # Looking up at elevation 20: the direction light travels points down (z < 0), and the
    # centre pixel is the view axis, 20° above the horizon.
    assert math.isclose(math.degrees(math.asin(-z)), 20.0, abs_tol=0.5)
    # At fov 150 the corner (160 px from the centre) is past the lens's image circle
    # (2f·sin(90°) ≈ 158 px); at fov 120 it would still be inside, since the lens images
    # up to 180° whatever fov the frame is cut at.
    view.lens_fov = 150.0
    assert lib.LUMICE_UnprojectPixel(ctypes.byref(view), 0, 0, out, ctypes.byref(valid)) == 0
    assert valid.value == 0, "the corner of a 150° fisheye canvas is outside the image circle"


# ---------------------------------------------------------------------------------------------
# The analysis session follows scene.ray_allocation (the same field the render commit reads,
# owner ruling 2026-09-13): `adaptive` binds the online Neyman deal to an analysis too. Three
# prisms, proportions 100 / 100 / 0.2, each with one `filter_in` raypath, so the third crystal's
# one row is rare under the population share: under `proportional` it gets 0.1% of the rays.
_CONFIG_CHALLENGE = str(
    get_project_root() / "test" / "e2e" / "configs" / "raypath_analysis_challenge_98_120_144.json")
_RARE_CRYSTAL_ID = 3
# Measured on this scene at its 1M-ray budget, one server, sessions back to back, sim_seed=0:
# the rare row's relative standard deviation across sessions is ~0.2 under proportional and
# ~0.02 under adaptive — 10.5× on the first measurement — and every row's mean agrees between
# the arms to z < 1 over 5M × 12. The thresholds sit at half and three times those figures.
# They are NOT to be relaxed on a red: both assertions compare statistics of two random arms,
# the shape that turns into a flake when its margin is thin, and the margin is what makes them
# stand. What the margin has to absorb is the estimate's OWN noise — a sample sd from n
# sessions is itself uncertain by ~1/√(2(n−1)), so the measured ratio scattered 5.2–16.9× over
# six repeats at n = 10 (one of them on the threshold) — so a red that is not a real regression
# is answered with more sessions (30 puts the ratio's own spread at ~0.19 in log, the 5×
# floor ≈3σ below a true ~9×), never with a looser ratio.
_CHALLENGE_SESSIONS = 30
_RARE_ROW_MIN_IMPROVEMENT = 5.0
# The mean-agreement assertion is one test over ALL rows (the worst row's z against a Šidák
# threshold), and this is its family-wise false-alarm rate: the chance that a run with no
# offset between the arms reds at all, whichever row does it. The value is the two-sided tail
# of the 3σ single-row test the assertion started as, 2·(1−Φ(3)); it is a number of its own
# rather than derived from that z so that a per-row threshold and the family rate cannot be
# moved through each other by accident. Under this rate each of R rows is held at
# α_R = 1 − (1 − α)^(1/R); asserting every row at α instead compounds to ≈ R·α.
# The nominal figure assumes a Gaussian z; with sample sds from 30 sessions the statistic is
# closer to Student-t (≈58 df), whose tails lift the real rate to ≈0.65% per run (measured by
# drawing 30-vs-30 from 2550 recorded sessions per arm centred on a common mean). That is the
# rate `_CONFIRM_SESSIONS` exists for — answered by more sessions, not a looser α.
_FAMILY_ALPHA = 0.0027
# A first-stage red is not a verdict: it draws `_CONFIRM_SESSIONS` FRESH sessions per arm and
# judges all 30 + 60 of them together against the SAME threshold, and only a red there fails.
# Why this and not a lower bar or a fixed seed. A per-run false-alarm rate of ≈0.65% is a red
# every couple of weeks on a CI leg that runs this case about a dozen times a day, and that is
# what it produced: the same row read low twice in ten days on CI (worker_count=3 there), while
# 2550 sessions per arm on a local machine — at that worker count, at two others, and under CPU
# oversubscription — bound every row's arm difference to under 0.1% (C1: −0.04% ± 0.05%, 95%),
# an order of magnitude below either red. Requiring both stages makes the rule strictly
# stricter than the first stage alone, so its false-alarm rate can only fall (measured: 0.65% →
# 0.03% per run), and it gives up nothing against a real offset: an offset large enough to red
# the first stage reads ≈√3× larger in z over 90 sessions than over 30, so it reds again
# (detection of a −0.7% / −1.0% C1 offset: 45.9% / 88.8% single-stage vs 45.8% / 88.8% here,
# by the same resampling). A fixed sim_seed would also stop the flake, but sim_seed ≠ 0 pins
# the server to ONE worker, which is not the shape the automatic worker count ships.
# The cost is paid only on a first-stage red: 120 more sessions, in about one run in 150.
_CONFIRM_SESSIONS = 60


def _sidak_z_threshold(alpha_total: float, r: int) -> float:
    """Two-sided z at which the worst of `r` rows reds with family-wise probability `alpha_total`."""
    alpha_per = 1.0 - (1.0 - alpha_total) ** (1.0 / max(r, 1))
    return statistics.NormalDist().inv_cdf(1.0 - alpha_per / 2.0)


def _shares(r: cr.RaypathAnalysisResult) -> dict:
    """Each row's share of the run's counted energy (entries + the unrecorded remainder)."""
    total = sum(e.energy for e in r.entries) + r.other_energy
    assert total > 0.0
    return {e.display: e.energy / total for e in r.entries}


def _energies(r: cr.RaypathAnalysisResult) -> dict:
    """Each row's counted energy, Σ(Y·w) as the histogram sums it — no normalisation."""
    return {e.display: e.energy for e in r.entries}


def _rare_row_display(results) -> str:
    """The one row on the rare crystal — its `filter_in` admits a single orbit under P|B|D."""
    names = {e.display for r in results for e in r.entries if e.chain and e.chain[0][0] == _RARE_CRYSTAL_ID}
    assert len(names) == 1, f"expected one row on crystal {_RARE_CRYSTAL_ID} under P|B|D, saw {sorted(names)}"
    return next(iter(names))


def _mean_sd(xs):
    n = len(xs)
    mean = sum(xs) / n
    var = sum((x - mean) ** 2 for x in xs) / (n - 1)
    return mean, math.sqrt(var)


def _score_rows(energies_p, energies_a, rows):
    """(z, row, mean_p, mean_a) for every row with a finite z, worst first: each row's arm
    difference over the two arms' pooled standard error. A row seen in one arm only counts as 0
    in the other's sessions, which is what its estimator says."""
    scored = []
    for row in rows:
        m_p, s_p = _mean_sd([e.get(row, 0.0) for e in energies_p])
        m_a, s_a = _mean_sd([e.get(row, 0.0) for e in energies_a])
        pooled_se = math.sqrt(s_p * s_p / len(energies_p) + s_a * s_a / len(energies_a))
        diff = abs(m_a - m_p)
        if pooled_se == 0.0:
            assert diff == 0.0, f"row {row!r}: constant in both arms yet different ({m_p} vs {m_a})"
            continue
        scored.append((diff / pooled_se, row, m_p, m_a))
    scored.sort(reverse=True)
    return scored


def _run_arm(tmp_path, ray_allocation: str, sessions: int = _CHALLENGE_SESSIONS):
    doc = json.loads(Path(_CONFIG_CHALLENGE).read_text(encoding="utf-8"))
    doc["scene"]["ray_allocation"] = ray_allocation
    path = tmp_path / f"challenge_{ray_allocation}.json"
    path.write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8")
    # sim_seed=0: a fresh random stream per session, so the spread ACROSS sessions is the
    # estimator's real noise (a fixed seed would make ten identical sessions and a sd of 0).
    return cr.run_raypath_analysis_capi_sessions(
        str(path), _full_sky_request(), sessions=sessions, sim_seed=0, max_entries=None,
        chain_id_symmetry=cr.LUMICE_RAYPATH_SYMMETRY_ALL)


@pytest.mark.slow
def test_adaptive_allocation_cuts_rare_row_noise_without_moving_the_means(tmp_path):
    """`adaptive` on an analysis: the rare row's share is ≥5× less noisy across sessions than
    under `proportional`, and no row's mean energy moved between the arms.

    Two arms of thirty sessions each on one server per arm, sim_seed=0 (random), the scene's
    own 1M-ray budget. The first assertion is the reason the analysis binds the online deal at
    all (the histogram's Σ(Y·w) carries the p/q correction, so what changes is variance, not
    expectation); the second is that unbiasedness, per row, as the worst row's z over the two
    arms' pooled standard error against a Šidák threshold at `_FAMILY_ALPHA` family-wise, a red
    of which is confirmed on `_CONFIRM_SESSIONS` more sessions per arm before it fails. Both
    arms are also each a positive/negative control for the bind: the proportional arm must log
    no online tally line, the adaptive arm one cold start per session from the analysis site.
    """
    prop = _run_arm(tmp_path, "proportional")
    adap = _run_arm(tmp_path, "adaptive")
    assert len(prop) == _CHALLENGE_SESSIONS and len(adap) == _CHALLENGE_SESSIONS
    assert prop[0].sim_ray_num == 1_000_000, "the scene's own budget"

    # The bind itself, off the log lines the server emits (their wording is pinned by the C++
    # unit test on the same site, test_ray_allocation_online_analysis.cpp).
    cold = "StartRaypathAnalysis: ray-allocation online tally started"
    for r in prop:
        assert not any(cold in line for line in r.log_lines), "proportional bound an online tally"
        assert not any("RayAllocationOnline(final)" in line for line in r.log_lines)
    for i, r in enumerate(adap, start=1):
        assert sum(cold in line for line in r.log_lines) == 1, f"adaptive session {i}: not one cold start"
        assert not any("keeping the online tally" in line for line in r.log_lines), \
            f"adaptive session {i}: an analysis carried a tally forward"

    rare = _rare_row_display(prop + adap)
    shares_p = [_shares(r) for r in prop]
    shares_a = [_shares(r) for r in adap]
    rows = sorted(set().union(*(s.keys() for s in shares_p + shares_a)))

    # 1. The rare row's noise. If proportional never hit it in ten sessions its rel_sd is
    #    undefined (0/0): that is an even louder statement of the same fact (a row adaptive
    #    measures and proportional cannot see at all), so it reads as an infinite ratio and
    #    is warned about rather than divided by. The measured 0.206 says it is hit at this
    #    budget; the guard is for the tail of that distribution.
    mean_p, sd_p = _mean_sd([s.get(rare, 0.0) for s in shares_p])
    mean_a, sd_a = _mean_sd([s.get(rare, 0.0) for s in shares_a])
    assert mean_a > 0.0, f"adaptive never counted the rare row {rare!r}"
    rel_a = sd_a / mean_a
    if mean_p == 0.0:
        warnings.warn(f"proportional never counted the rare row {rare!r} in {_CHALLENGE_SESSIONS} sessions; "
                      f"its rel_sd is undefined and taken as infinite (adaptive: {rel_a:.4f})")
        rel_p = math.inf
    else:
        rel_p = sd_p / mean_p
    assert rel_a * _RARE_ROW_MIN_IMPROVEMENT <= rel_p, (
        f"rare row {rare!r}: rel_sd adaptive {rel_a:.4f} vs proportional {rel_p:.4f} "
        f"({rel_p / rel_a if rel_a > 0 else float('inf'):.1f}×, need ≥ {_RARE_ROW_MIN_IMPROVEMENT}×)")

    # 2. Every row's mean ENERGY agrees between the arms. Energy rather than share: Σ(Y·w) is
    #    the quantity the p/q correction makes unbiased, and a share divides it by the run's
    #    total, which under proportional is mostly the rare row's own noise (per-session rel_sd
    #    ≈0.19 against ≈0.02 for the other rows) — so a rare-row shortfall in that arm reads as
    #    every OTHER row's share having moved, and the red this assertion once raised named a
    #    non-rare row for exactly that. One z per row, judged together: the worst against the
    #    Šidák threshold for R rows, so the stage reds at _FAMILY_ALPHA — and a first-stage red
    #    is confirmed over 30 + _CONFIRM_SESSIONS sessions per arm before it fails (the
    #    constant's comment says why that neither lowers the bar nor gives up power).
    energies_p = [_energies(r) for r in prop]
    energies_a = [_energies(r) for r in adap]
    first = _score_rows(energies_p, energies_a, rows)
    if not first or first[0][0] <= _sidak_z_threshold(_FAMILY_ALPHA, len(rows)):
        return
    energies_p += [_energies(r) for r in _run_arm(tmp_path, "proportional", _CONFIRM_SESSIONS)]
    energies_a += [_energies(r) for r in _run_arm(tmp_path, "adaptive", _CONFIRM_SESSIONS)]
    rows = sorted(set().union(*(e.keys() for e in energies_p + energies_a)))
    scored = _score_rows(energies_p, energies_a, rows)
    z_max = _sidak_z_threshold(_FAMILY_ALPHA, len(rows))

    def by_z(scored_rows):
        return ", ".join(f"{row} z={z:.2f} ({m_p:.1f} vs {m_a:.1f})" for z, row, m_p, m_a in scored_rows)

    # The failure message carries both stages and the worst row's per-session values of both
    # arms: a red is a comparison of two random arms whose only post-mortem question is "one
    # outlier session, or a shifted arm?", and the per-case temp data is gone by the time
    # anyone asks it.
    assert not scored or scored[0][0] <= z_max, (
        f"row {scored[0][1]!r}: mean energy proportional {scored[0][2]:.2f} vs adaptive "
        f"{scored[0][3]:.2f} over {len(energies_p)} vs {len(energies_a)} sessions, z = {scored[0][0]:.2f} > "
        f"{z_max:.2f} (Šidák over {len(rows)} rows at family-wise α = {_FAMILY_ALPHA}), confirming a red "
        f"of the first {_CHALLENGE_SESSIONS} — the p/q correction is not carrying this row's expectation; "
        f"all rows by z: {by_z(scored)}; first stage: {by_z(first)}; per-session energy of {scored[0][1]!r} "
        f"— proportional: " + " ".join(f"{e.get(scored[0][1], 0.0):.0f}" for e in energies_p) + "; adaptive: "
        + " ".join(f"{e.get(scored[0][1], 0.0):.0f}" for e in energies_a))
