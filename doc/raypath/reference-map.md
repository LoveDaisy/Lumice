# The Class Reference Map

The engine-side reference map delivered for the analyze GUI's single-path view (scrum 742.1):
the whole physical class's single-reference-lambda band-sum grid over an explicit view, published
through the raypath C API (`lumice_raypath.h`, v4.59). This file is the durable record of the
map's domain, units, view mapping and stop semantics — the consumption entry a GUI client needs.

## What is evaluated

One map request names a captured scene, one single-layer raypath class (the request's face
sequence and symmetry bits) and one view. The engine resolves the class's physical members (the
existing expansion authority), then evaluates **every** member with the same `analytic::BandSum`
kernel on one shared antipodal-Fibonacci lattice and one shared PixelTable — the same kernel the
survey's radial line and the query probe run (one implementation, three triggers):

- **Band members** (deflecting paths): the per-sr pixel values sum member-by-member. The class
  map is the sum of the member maps — never the representative's map times the member count.
- **Rank-0 members** (identity paths, `path_rank`): their whole contribution is a point mass in
  the sun direction. Their masses (the existing `RankZeroMass` semantics) sum into a separate
  arm, placed by the existing `PlacePointMass` primitive onto the cell holding the sun. A
  rank-0 class's sky image IS that mass; its zero pixel cells are not "no light".

Caliber (echoed by the result readers, never implied): one reference wavelength (the probe
resolution's rule — the scene's spectrum when exactly one discrete wavelength, else 550 nm), the
nominal geometry, the point sun, the scene's declared pose density. Per-lambda maps are a later
face (742.4's dispersion reads positions, not a colored map).

## The view and its mapping

A view is a (deviation x azimuth-about-sun) grid in the probe frame — the line probe's own frame
(`MakeProbeFrame`), so a map and a probe line at the same lambda read the same coordinates. A
request either names the all-sky view (`full_sky`: delta [0, 180], azimuth [0, 360)) or an
explicit window centred on a sky point (altitude/azimuth in the probe's at-point convention)
with degree spans, plus the grid's rows (delta) and columns (azimuth), row-major, at most
`LUMICE_PATH_MAP_MAX_PIXELS` (262,144) cells. A full-sky request with both grid dims at 0 takes
the default grid (180 x 360); any other sub-1 dim is refused.

`LUMICE_PathMapGetInfo` carries the mapping description: the window's delta/azimuth bounds, the
grid dims, and the frame's `incident`/`e1`/`e2` axes. A cell centre's sky direction is

```
d = cos(delta) * incident + sin(delta) * (cos(azimuth) * e1 + sin(azimuth) * e2)
```

and the inverse reading maps a direction through the same frame. A GUI maps pixel indices to
sky directions (and back for point picks) through these fields alone — it derives no second
convention. Every request rebuilds the PixelTable and re-runs the class: zooming is a real
re-evaluation, never an upscale of an older raster.

## Units and reading the cells

`LUMICE_PathMapGetPixels` borrows five row-major columns: cell-centre `delta_deg`/`azimuth_deg`,
the per-sr `values` (LI Phase I pixel convention: power per steradian along the path, per unit
incident irradiance, hexagon edge a = 1), summed member event `counts`, and a `status` byte
(`OK` / `SINGULAR` / `POINT_MASS`). A cell covering the sun direction or its antipode is
SINGULAR: NaN value, never a written zero. `LUMICE_PATH_MAP_PIXEL_POINT_MASS` marks the cell
holding `m_total / Omega`; `LUMICE_PathMapGetPointMass` reads the mass, its error and the
per-member method names (`lattice_mean` under the random density, `psi_average` otherwise).

The quantity spec (`LUMICE_PathMapGetQuantity`) declares the aggregation:
`kClassLatticePowerSum` over `kPhysicalClass`, per-steradian band-sum at the reference
wavelength, solar-center source, nominal geometry.

## Execution, stop and honesty

The query runs synchronously on the calling thread (a caller's background thread is its own
decision — the workspace entry is the GUI's shape); the budget/cancel polls sit at the
per-member boundaries. A stop keeps the **completed member prefix** — full-lattice values — and
reports `truncated` with `members_evaluated` in Info/Evidence; a member cut mid-lattice is
discarded, never mixed into the prefix. Same request, bitwise-same map. Finite zero
(`kept_count == 0` cells or a whole empty window) is a finite claim, distinct from the named
`measure_not_expressible` refusal (the scene's axis distribution is not expressible in the
map's pose families — documented with the axis's own name in the refusal reason). The map never
substitutes a measure silently and never reports a zero map for a refusal.

The workspace entry (`LUMICE_PathWorkspaceQueryMap`) shares the geometry slot and the cancel
token with the other faces, takes no warm data, and keeps no cached result: the result is the
frozen typed value, readable after the Scene and Workspace are gone.

## Constants and their measured basis

Default budget 15 s / max 120 s (the probe face's published pair), default lattice 2,000,000
(the survey probe's count), full-sky default grid 180 x 360, cell bound 262,144. Basis: the M1
probe on the C01 class (12 members, n=2M) measured a full-sky class map at ~3.4 s and a
0.02-degree inner-edge view at ~1.9 s; 4x the cells at a fixed domain cost +15% (the pixel count
is second-order; event construction dominates) — the interactive envelope of the default
constants, with `sample_count` as the coarse-to-fine knob.

## Verification anchors

The unit suite pins: class sum == sum of member maps (bitwise) with the representative-times-N
negative control; the C15 identity class reading as a mass arm over an honest zero texture; a
constructed mixed class splitting arms per member; bitwise determinism and real view
recomputation; the stop prefix; the refusal matrix; singular-cell NaN honesty; and the
closed-form minimum-deviation edge (`2*asin(n/2) - 60`) contained in the first non-empty cell,
with the probe-consistency leg on the same kernel. The typed ABI e2e adds the independent C
consumer leg (installed headers, product library, cross-destroy reads).

## The GUI consumer (742.5)

The Analyze workspace's raypath view is the first GUI consumer, and its shape is the reference
for later ones. It derives no second convention: the picture and every pick convert through the
Info-echoed frame alone (cell direction -> sky position by the §12 display rule, position ->
travel for a pick), the view's window requests are the disk's bounding rectangle in the
(delta, azimuth-about-sun) plane — the tangent-cone azimuth bound off the poles, the full
circle with a quasi-uniform grid when the disk contains a pole, where the azimuth's reference
deviation no longer exists — and the display tier samples every request at n = 500k (the
measured interaction arm: full sky ~0.9 s, narrow windows sub-second, the pole-containing
refinement shape ~2 s), with the caliber line echoing the Info's own count. Requests ride the workspace entry on the GUI's one background worker,
queued per face behind the survey; a view change supersedes only the map face, and an outcome
that fails the intent's id is dropped, never shown. The consumer-side rules live in
`raypath-analysis.md` §5.1.8 (the 742.5 as-built and the 742.6 handover); this file stays the
map's own contract.
