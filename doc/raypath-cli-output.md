# `Lumice raypath` output (schema_version 1)

`Lumice raypath` analyses **one single-layer raypath of one crystal entry over one sky point**:
the components of the fiber of crystal poses that send the sun into that point, each component's
poses with per-pose detail, and the path's deviation over the whole sun-direction sphere
(`doc/raypath-analysis.md` §5.1.8). `analyze` lists the raypaths that light the sky; `raypath` is
what you ask about one of them.

It writes one JSON document. This page is that document's field reference, and the one place a
reader (the Analyze-workspace prototype, a script, the GUI later) should learn it from.

## 1. Where the document comes from

The CLI is a thin shell. The computation and the serialization both live in the engine:

| Piece | Where |
|---|---|
| The analysis | `lumice::raypath::AnalyzeSinglePath` (`src/raypath/single_path_analysis.hpp`) over the analytic kernel (`src/analytic/`) |
| The JSON form and the `--warm` reader | `lumice::raypath::ToJson` / `ParseWarmSeeds` (`src/raypath/single_path_json.hpp`) — one file, shared key names |
| The public entry point | `LUMICE_AnalyzeSinglePath` → opaque `LUMICE_SinglePathResult` → `LUMICE_SinglePathResultToJson` (`lumice.h`, v4.50) |
| The subcommand | `src/main.cpp` (`ParseRaypathOptions` / `RunRaypath`) |

So the GUI, when it grows an Analyze workspace, reads **the same document** through the same
`lumice.h` call; there is no second serializer to drift from this one. The result is exposed as
JSON rather than as a C struct mirror on purpose: it holds variable-length nested lists whose
fields an interface will keep adding to. Typed readers can be appended to `lumice.h` later.

## 2. Command line

```
Lumice raypath -f <config> --crystal <id> --path <faces> --target <alt>,<az> [options]
```

| Option | Meaning |
|---|---|
| `--crystal <id>` | The crystal entry (config id). Required. |
| `--path <faces>` | The raypath as `analyze` prints it: `3-5`, `3-6-4-8`; `C1(3-5)` if the layer names its crystal (must equal `--crystal`). Multi-layer chains (`(3-5) -> (1-3)`) are handed to the engine, which refuses them as `multi_layer_unsupported`. Required. |
| `--target <alt>,<az>` | The sky point, degrees; azimuth measured as the sun's (the `analyze --center` convention). Required. |
| `--wavelength <nm>` | In [350, 900]. Default: the config's, when its spectrum is exactly one wavelength; else 550. The choice is recorded in `meta.wavelength.source`. |
| `--events <N>` | Seed events of the component search — poses sampled to locate the fiber, **not** traced rays. K/M suffix; at most 100M (`LUMICE_SINGLE_PATH_MAX_SAMPLE_COUNT`). Default 1M. |
| `--grid <rows>` | Latitude rows of the sun-direction grid; longitude is twice that. [0, 720]; 0 leaves `sun_grid` out. Default 90. |
| `--warm <file>` | An earlier output; its seeds start the search (§5). |
| `-o <path>` | Write the document here instead of stdout (never both). |
| `-v` / `-d` / `-h` | As for every subcommand. `--backend`, `--workers`, `--seed` are not options here: the analysis has one route, runs on the calling thread and draws no random numbers. |

Exit status: 0 on success; 1 for a bad option (usage printed), a config that does not load, or a
request the engine refuses — then stderr carries `Error: <reason>: <detail>`, where `<reason>` is
one of `unknown_crystal_id`, `multi_layer_unsupported`, `invalid_path`, `face_not_in_crystal`,
`wavelength_out_of_range`, `invalid_target`, `invalid_argument`, `crystal_rejected`,
`path_infeasible`, `invalid_scene`.

**Progress and Ctrl-C differ from `analyze`, deliberately.** The engine call has no cancellation
point and no progress callback, so stderr gets one line when the analysis starts and one when it
ends, and Ctrl-C ends the process without writing anything (there is no partial result to write).
With `-o`, the document goes to `<path>.tmp` first and is renamed over `<path>`, so `<path>` is
never half a document; an interrupted run may leave the `.tmp` behind. Measured cost (arm64 mac,
`3-5`, grid 90): 1M events 0.03 s, 10M 0.28 s, 100M 2.8 s at 155 MB peak; a 720-row grid is a
13 MB document.

## 3. The document

Keys appear in the order below. **Fields are only ever appended**; a reader ignores keys it does
not know. `schema_version` is bumped only when a field changes meaning or is removed.

### 3.1 Top level

| Key | Type | Present | Meaning |
|---|---|---|---|
| `schema_version` | int | always | 1 |
| `generator.lumice` | string | always | The Lumice version that wrote it |
| `generator.analytic_api_version` | int | always | The analytic kernel's API version |
| `conventions` | object of strings | always | This page's §4 in words, so a document explains itself: `frames`, `directions`, `target_azimuth`, `pose`, `angles`, `sun_in_crystal`, `units`, `null`, `sun_grid`, `completeness`, `reach` |
| `meta` | object | always | Every conversion from the scene to the kernel, as applied (§3.2) |
| `outcome` | `"discovered"` \| `"point_mass"` | always | Which of the two branches below is present |
| `point_mass` | object | `outcome == "point_mass"` | §3.5 |
| `components` | array | `outcome == "discovered"` | §3.3; may be empty |
| `incomplete` | array | `outcome == "discovered"` | §3.4; may be empty |
| `discovery` | object | `outcome == "discovered"` | §3.6 |
| `reach` | object | `outcome == "discovered"` | §3.8; written whether or not `components` is empty |
| `sun_grid` | object | `--grid` > 0 (either outcome) | §3.7 |

### 3.2 `meta`

| Key | Meaning |
|---|---|
| `crystal.id`, `crystal.kind` | The entry; `"prism"` or `"pyramid"` |
| `crystal.shape[]` | Each shape scalar as the kernel got it: `name` (config key: `height`, `prism_h`, `upper_h`, `lower_h`, `face_distance[i]`), `value` (the distribution's centre: the fixed value, uniform midpoint, gauss mean, laplacian location), `distribution` (`fixed`, `uniform`, `gauss`, `zigzag`, `laplacian`, `gauss_legacy`), `spread` |
| `crystal.shape_is_nominal` | True when any scalar has a non-zero spread: the fiber and grid belong to the nominal crystal, not the population the simulation samples |
| `crystal.upper_wedge_deg`, `crystal.lower_wedge_deg` | Pyramid only |
| `faces` | The analysed face sequence |
| `sun.altitude_deg`, `sun.azimuth_deg`, `sun.diameter_deg` | From the config; the diameter is recorded, not used (the sun is a point here) |
| `sun.incident_direction` | World propagation direction sun → crystal |
| `target.altitude_deg`, `target.azimuth_deg`, `target.direction`, `target.deviation_deg` | The sky point; `direction` is crystal → observer; `deviation_deg` is the sun–target angle |
| `wavelength.nm`, `wavelength.source` (`user` / `config` / `default`), `wavelength.refractive_index` | |
| `discovery_settings.sample_count`, `band_half_width_rad`, `cluster_radius_rad`, `distance_threshold_rad`, `warm_seed_count` | The search as run |

### 3.3 `components[]` — the fiber

| Key | Meaning |
|---|---|
| `kind` | `"closed"` (a loop) or `"arc"` (ends on boundary events at both ends) |
| `seed` | 9 numbers, the pose the component was traced from (§5) |
| `forward` | `{status, reason, pose_count}` of the forward trace. `status`: `closed`, `event_terminated`, `numerical_failure`, `budget_exhausted`; `reason`: `closed_loop`, `tir_boundary`, `branch_boundary`, `path_infeasible`, `visibility_boundary`, `chart_boundary`, `rank_loss`, `topology_ambiguity`, `corrector_failure`, `linear_solve_failure`, `non_finite`, `step_underflow`, `invalid_numerical_input`, `step_budget`, `arclength_budget`, `evaluation_budget` |
| `backward` | Same shape; **arc only** — a closed component never runs a backward trace, so the key is absent |
| `seed_index` | Index of the seed in `points` (0 for a closed component) |
| `arclength_increments` | Radians on SO(3) between consecutive points; `len(points) - 1` entries |
| `points[]` | Ordered along the component: closed = the forward trace, seed first, last point the closing pose; arc = backward trace reversed, then forward, so the list runs from one boundary event to the other |

Each point:

| Key | Meaning |
|---|---|
| `pose` | 9 numbers, row-major body → world rotation |
| `angles.zenith_deg`, `azimuth_deg`, `roll_deg`, `degenerate` | The pose as the config's orientation angles (§4) |
| `sun_in_crystal` | Unit 3-vector: where the sun sits in the crystal frame. **This is the point to plot on the sun-direction sphere**: the component is a curve there, lying on the level set `sun_grid.deviation_rad == meta.target.deviation_deg` (in radians) |
| `residual_norm` | The continuation's residual at this point |
| `valid` | The path's direction-level validity at this pose |
| `outgoing_direction` | World, crystal → observer (zeros when not valid) |
| `segment_directions` | `(len(faces) + 1) × 3` numbers, body frame: incident, internal legs, outgoing |
| `interface_transmittances` | One per face: T at entry and exit, R at internal faces |
| `total_transmission` | Their product. Not a relative intensity on its own |
| `entry_measure` | The entry cross-section (perpendicular to the incident direction) whose rays follow exactly this face sequence through the finite crystal, in the crystal's length unit squared |

### 3.4 `incomplete[]`

Candidates the search found but could not turn into a component: `cause` (`arc_backward_failed`,
`arc_backward_closed_anomaly`, `unnamed_event`, `not_converged`), `seed`, `forward`, and
`backward` only for the two `arc_backward_*` causes.

### 3.5 `point_mass`

Rank 0: the outgoing direction does not depend on the pose (e.g. `1-2`, a flat plate's two basal
faces). No fiber exists to trace. `direction` (world), `altitude_deg` / `azimuth_deg` (the sky
point it lands on), `target_separation_deg` (from the requested target).

### 3.6 `discovery`

`complete` and the search funnel's counters: `pool_count`, `extra_seed_count`,
`raw_cluster_count`, `admissible_count`, `dedup_merged`, `arc_stitched`, `arc_backward_failed`,
`arc_backward_closed_anomaly`, `incomplete_unnamed_event`, `incomplete_not_converged`.
**`complete` is procedural, not a certificate** (`conventions.completeness` carries the sentence):
it means every admissible candidate of this sample closed or became an arc, never that every
component of the fiber was found.

### 3.7 `sun_grid`

`lat_count`, `lon_count` (= 2 × `lat_count`), and three flat row-major arrays of
`lat_count × lon_count` cells: `deviation_rad` (the path's deviation with the sun at that cell;
`null` where not valid), `valid` (0/1), `entry_measure` (0 where not valid). Cell `[i][j]` is at

```
lat_i = -90 + (i + 0.5) * 180 / lat_count
lon_j = -180 + (j + 0.5) * 360 / lon_count
u     = (cos lat cos lon, cos lat sin lon, sin lat)     # sun direction, body frame
```

No row sits on a pole, so no cell is degenerate; the longitude seam is periodic (a contour tracer
should wrap it). The deviation depends on the pose only through `u`.

### 3.8 `reach`

Whether the target's deviation δ (`meta.target.deviation_deg`, here in radians) lies in the range
of the path's deviation D over **every** valid sun direction — that is, whether any pose of the
*infinite* crystal could send the sun into the target at all.

| Key | Meaning |
|---|---|
| `target_in_range` | `true` when δ ∈ [`deviation_min_rad` − `tolerance_rad`, `deviation_max_rad` + `tolerance_rad`] |
| `target_deviation_rad` | δ |
| `deviation_min_rad`, `deviation_max_rad` | The probed range of D; both are values D actually takes at an evaluated valid sun direction. `null` if the probe finds no valid direction (then `target_in_range` is `true`: nothing is excluded) |
| `tolerance_rad` | How far the probed range may fall short of the true one (below) |
| `probe_lat_count` | Rows of the probe grid (360; longitude twice that) |

How it is probed. The probe has its own grid, the `sun_grid` layout at 360 rows, independent of
`--grid`, `--events` and `--warm`: the same request gives the same `reach` whatever those are,
including `--grid 0`. Its samples are the valid cell centres plus, on every edge between a valid
and an invalid neighbouring cell, the validity boundary found by bisection. The boundary matters
because D is square-root singular where the path reaches total reflection or grazing incidence:
there cell centres alone approach the range's end at half order in the cell size (the `3-5`
path's maximum still moves by 0.027 rad between 360 and 720 rows of centres), and no fixed
tolerance covers that. The tolerance is read off the probe's own convergence: `tolerance_rad` =
(1 + √2) × the change of the range's ends between a 180-row and the 360-row probe, which bounds the
360-row shortfall whenever it shrinks at least as fast as the square root of the cell size.
Measured against a 2880-row probe on eleven prism and pyramid paths, the actual shortfall is at
most 0.85 × `tolerance_rad`; the largest ratios come from an extremum where two validity
boundaries meet, the smallest from a smooth extremum. The probe costs about 0.01 s.

What each answer means:

- `false` — no valid sun direction reaches δ **at the probe's resolution**. It is not a
  certificate: a valid region too thin to contain a single cell centre is missed entirely.
- `true` — δ is not excluded. It does **not** promise a component: the valid set need not be
  connected, so δ can lie between two pieces' ranges; and when D does reach δ, the finite crystal
  may pass no ray along that fiber (below).

`reach` is direction-level: validity depends on the face normals and the refractive index, not
on the crystal's size. Two prisms of different height give the same `reach`; the effect of the
finite crystal is read from `entry_measure`, not from here.

### 3.9 Empty results

A document can end with nothing to plot in three ways, told apart as follows:

| `outcome` | `components` | `reach.target_in_range` | Reading |
|---|---|---|---|
| `point_mass` | absent | absent | Rank 0: the outgoing direction does not depend on the pose; there is no fiber. `point_mass.target_separation_deg` says how far the one sky point is from the target |
| `discovered` | `[]` | `false` | Geometrically unreachable: no pose of the infinite crystal sends the sun into the target (at the probe's resolution) |
| `discovered` | `[]` | `true` | Not excluded, none found: the fiber may exist while the finite crystal passes no ray along it (`entry_measure` is 0 everywhere on it), or the search missed a component (`discovery.complete` is procedural; try more `--events`), or the valid set's gaps leave δ unreached. This document does not tell these apart |
| `discovered` | non-empty | `true` | Components found |

Worked example, prism path `3-6-4-8` (four prism faces: a turn about the c-axis, whose D depends
only on the sun's latitude in the crystal, from 0 at the pole to exactly 120° at the equator; sun
at altitude 20°, 550 nm). At the target (20°, 120°), δ = 108.94°: a prism of height 0.8 gives two
arcs, one of height 0.5 gives `[]` with `target_in_range: true` — the fiber exists, the shorter
prism passes no ray along it. At (20°, 140°), δ = 124.02° is beyond 120° and the result is `[]` with
`target_in_range: false` for either height.

### 3.10 Degenerate paths

The level set of the deviation on the sun-direction sphere is the fiber for every path checked
(prism `3-5`, with an internal reflection `3-5-6-7`, pyramid `13-15-26-28`), but for some paths
the fiber and the orientation distribution stop being independent, and the document shows it:

- **Latitude-invariant paths** (the middle faces compose to a turn about the c-axis, e.g.
  `3-6-4-8`, or to the identity, e.g. `1-3-6-2`). D depends only on the sun's latitude in the
  crystal frame, so every row of `sun_grid.deviation_rad` is constant (row range ≈ 0, to ~1e-11
  rad) and the level set is a latitude circle. Nothing extra is written for this; a reader tests
  the row range, or the range of `sun_in_crystal`'s latitude over a component.
- **The fiber is an arc of that circle, not a loop.** The valid part of each circle is a fraction
  of its longitude (about 36% for `3-6-4-8`), so components are `kind: "arc"`, both ends are
  boundary events (`tir_boundary` / `path_infeasible`), and the curve's steps shrink to ~1e-5 at
  the ends — plotted points cluster there. The same δ often has two such circles, mirrored about the
  equator (`±lat`), and `sun_grid.valid` varies with longitude.
- **A flat-plate orientation family lands on one sky point.** The plate's `u` also lies on a
  latitude circle (latitude = the sun's altitude), so at δ = D(sun altitude) the whole family
  reaches one point: for the plate `1-4-5-2` (the 120° parhelion under this repository's face
  numbers) the fiber sits at latitude 20.000° for a sun at 20°, an arc of 60° in length. There is
  no field for this; it is the coincidence of `components[*].points[*].sun_in_crystal` with the
  orientation family, read by the consumer.
- **Rank 0** (`1-2` on a plate): `outcome: "point_mass"`, no level set exists (D ≡ 0); show one
  sky point and `target_separation_deg` rather than the "no components" wording of §3.9.

Column crystals, Parry and Lowitz orientation families (checked on the prism `3-5` only, explore
`level-set-degeneracy-column-parry-lowitz`): the family meets the fiber at isolated poses, judged in
pose space from `components[*].points[*].pose` — column `R[2][2] = 0` and Lowitz `R[2][1] = 0`
(codimension 1: a sign change along the fiber; two nearly merged roots = tangency, e.g. near the
upper tangent arc's vertex), Parry `R·x = +z` (codimension 2: a minimum distance going to zero, not a
sign change). The sun-sphere projection of a family (`sun_grid` band) is necessary but not
sufficient (3 of 5 column targets were false positives). Other paths can collapse under these
families as well: Lumice Integral found Parry-family collapse (roll 0) for paths whose fold matrix
is `S_x`, e.g. `1-6-2` (its `focusing.family_pinned` label does not cover Parry yet).

## 4. Conventions

- Frames: world `+z` is the zenith, azimuth counter-clockwise from `+x` seen from `+z`; body is the
  crystal frame, `+z` its c-axis (`doc/coordinate-convention.md`).
- Directions are propagation directions. The sky point a direction `d` comes from sits at altitude
  `asin(-d.z)`.
- Pose: `R = Rz(azimuth − 180) · Ry(−zenith) · Rz(roll)`, body → world, written row-major.
- Angles: degrees; zenith in [0, 180], azimuth and roll in (−180, 180]. At zenith 0 or 180 only
  azimuth ± roll is defined: roll is 0, azimuth carries the whole angle, `degenerate` is true.
- Units: `_deg` degrees, `_rad` radians, `_nm` nanometres.
- `null` = a number that is not defined (NaN).
- Numbers are the shortest decimal that reads back as the same double, so `seed` round-trips
  bit-exactly; the two numeric `sun_grid` arrays are rounded to 9 significant digits first (size).

## 5. `--warm`

`--warm <file>` reads `components[*].seed` and then `incomplete[*].seed` from an earlier output
and passes them to the search as extra starting poses. Densifying the search is not monotone — a
denser sample can lose a component a sparser one found — so feeding back seeds is how a
component, once found, is kept. The file must carry the same `schema_version`, and every seed
must be 9 numbers; otherwise the run fails with `invalid_argument: warm seeds: …`. A point-mass
output is accepted and holds no seeds.

A warm seed is a starting point, not a certificate. The engine's builds for different CPU tiers
(the release's baseline and x86-64-v3 engines) are compiled with different floating-point
contraction, so outputs of two tiers need not agree to the last bit.

## 6. Reading it from a page

```js
const doc = await (await fetch("r.json")).json();
if (doc.outcome === "discovered") {
  if (doc.components.length === 0)                // §3.9: unreachable, or reachable and none found
    note(doc.reach.target_in_range ? "no component found" : "target out of this path's reach");
  const g = doc.sun_grid;                          // contour: g.deviation_rad at doc.meta.target.deviation_deg * PI/180
  for (const c of doc.components)
    plot(c.points.map(p => p.sun_in_crystal));    // each component is a curve on that contour
}
```
