# `Lumice raypath` output contracts

`Lumice raypath` analyses **one single-layer raypath of one crystal entry over one sky point**:
the components of the fiber of crystal poses that send the sun into that point, each component's
poses with per-pose detail, and the path's deviation over the whole sun-direction sphere
(`doc/raypath-analysis.md` §5.1.8). `analyze` lists the raypaths that light the sky; `raypath` is
what you ask about one of them.

It writes one JSON document. This page is that document's field reference, and the one place a
reader (the Analyze-workspace prototype, a script, the GUI later) should learn it from.

There are two deliberately separate documents. `--target` writes the original target-fiber
document (`schema_version: 1`, sections 1–6); its keys, ordering, and `--warm` behavior are
unchanged. `--report` writes the target-free `lumice.path-feature-report` document in section 7.
Neither mode silently substitutes for the other.

## 1. Where the document comes from

The CLI is a thin shell. The computation and the serialization both live in the engine:

| Piece | Where |
|---|---|
| The analysis | `lumice::raypath::AnalyzeSinglePath` (`src/raypath/single_path_analysis.hpp`) over the analytic kernel (`src/analytic/`) |
| The JSON form and the `--warm` reader | `lumice::raypath::ToJson` / `ParseWarmSeeds` (`src/raypath/single_path_json.hpp`) — one file, shared key names |
| The public entry point | `LUMICE_AnalyzeSinglePath` → opaque `LUMICE_SinglePathResult` → `LUMICE_SinglePathResultToJson` (`lumice_raypath.h`, v4.50) |
| The subcommand | `src/main.cpp` (`ParseRaypathOptions` / `RunRaypath`) |

So the GUI, when it grows an Analyze workspace, reads **the same document** through the same
`lumice_raypath.h` call; there is no second serializer to drift from this one. The result is exposed as
JSON rather than as a C struct mirror on purpose: it holds variable-length nested lists whose
fields an interface will keep adding to. Typed readers can be appended to `lumice_raypath.h` later.

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
- `null` = a number that is not defined (NaN or an infinity — JSON cannot spell either).
- Numbers are the shortest decimal that reads back as the same double, so `seed` round-trips
  bit-exactly; the two numeric `sun_grid` arrays are rounded to 9 significant digits first (size).

These two encoding rules are consumer-visible contract, so they have exactly one implementation
owner: `src/raypath/detail/json_values.hpp` (`Num` / `GridNum` / `Array`), included by both
serializers — the single-path document and the section-7 report. Neither schema may hand-roll its
own copy of the non-finite-to-`null` mapping or the sun-grid rounding again.

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

## 7. Target-free path feature report (schema 2)

```bash
Lumice raypath -f <config> --crystal <id> --path <faces> --report \
  --budget-ms 15000 --max-evaluations 4000000
```

This is a **single-crystal, target-free, bounded diagnostic**, not a second renderer and not a
promise to enumerate every sky feature. Internal reflections are supported; a multi-crystal
chain produces `outcome: "unsupported_multicrystal"` with zero evaluations, not a report on its
first segment. A scene may contain several layers: the selected crystal's first occurrence is
used, or the C request's `scene_layer_plus_one` selects a layer explicitly. A configured crystal
that appears in no scattering entry may be inspected as a standalone object; its reported
`scene_layer` is null and its identity says it is not a scene allocation.

This is conditioned on the selected single-crystal path and the configured light source. It does
not apply scene entry proportions, continuation probabilities, filters, other-path backgrounds,
or illumination arriving from preceding scatterings. Selecting an entry in a later scene layer
does not claim to solve that layer's full multiple-scattering illumination.

The report uses the **actual product shape, correlations, orientation distribution, effective
solar diameter and spectrum**, through the same pure transforms as simulation. It does not replace
a finite sun with a point or a randomized shape with its mean. Physical L2 expansion is distinct
from label P/B/D matching. The C request's `symmetry_bits_plus_one` explicitly selects P/B/D bits
(or the single concrete path); zero retains the P|B|D default. There is no path-name or reference-shape detector dispatcher.

With no `--wavelength`, discrete spectra are summed with their actual weights; a continuous
illuminant uses full-band quadrature and a separate refinement check. `--wavelength <nm>` explicitly
selects a diagnostic spectrum instead, labelled as such. The C API accepts up to 32 diagnostic
wavelength/weight pairs; weights are charged once, not multiplied into the scene SPD again.

`--events` is an even **outer draw** count in `[64, 1000000]`, not a pixel count or a fiber seed
count. When omitted, a dyadic prefix up to 65536 is selected from the member/spectral expansion
and optical work available for the main estimate, independent repeat and spectral refinement.
The numerical defaults are 15000 ms, 4000000 optical evaluations and 250000000 weighted-component
field evaluations. `--budget-ms` permits up to 120000 ms; `--max-evaluations` up to 16777216;
`--max-field-evaluations` up to 1000000000. These bounds are checked inside numerical work, not
only after it finishes. Bounded serialization follows the numerical deadline. Sampling is
replayable, but which partial records fit a wall-clock deadline can vary across machines.

### 7.1 One document and one owner

`LUMICE_AnalyzePathFeatureReport` returns an immutable opaque result. CLI and C clients read the
same `PathFeatureReportToJson` serialization. The v4.52 request suffix is read only when its
complete group fits `struct_size`; the v4.51 prefix remains accepted with the new defaults.
The JSON schema changes meaning and is therefore **version 2**, not an append to version 1.
The target/fiber document and `--warm` contract in sections 1–6 are unchanged.

| Key | Meaning |
|---|---|
| `schema`, `schema_version` | `lumice.path-feature-report`, 2 |
| `outcome` | `completed`, `partial`, `unsupported_multicrystal`, or `no_related_feature` |
| `scope` | Value-owned selected input, actual/diagnostic spectral scope, seed, scene/layer/crystal identity |
| `support` | Pose coordinate count versus ZYZ support dimension, shape parameter count, source/spectral dimensions; unknown joint optical rank is null |
| `physical_members` | Concrete physical-L2 face sequences; never an L1 label orbit |
| `spectrum` | Actual nm, source weight, quadrature mass, refractive index and XYZ coefficient |
| `actual_features` | Positive, locally supported numerical/physical records at their declared observation or source scope |
| `candidates` | Verified conditional mechanism/source facts whose observed significance or full extent is unproved |
| `unfinished` | Numerical feature records that did not establish the required evidence, retaining available diagnostics |
| `unfinished_reasons` | Required discovery/verification stages left incomplete or unavailable |
| `coverage` | Supported scope and bounded-search limitations, not a claim of global completeness |
| `observation` | Kernel, bandwidth, local location target and search scope; no MC projection is imposed |
| `requested_outer_samples` | The request's `sample_count` verbatim: with no `--events` (auto mode) it is **0**, not the planned count; the resolved dyadic prefix actually used is `budgets.requested_outer_samples` |
| `budgets`, `timing` | Actual work, completed outer draws/repeat, exhaustion and per-stage seconds |
| `sources` | Call-local tokens linking outer draw, member and spectral row; records also carry explicit source witnesses |

An unsupported-chain document contains the requested path layers, empty evidence buckets, coverage
and zero-work budgets, but no fabricated physical input/result payload.

`completed` means the declared bounded search stages finished, not that every seed established a
feature. Unsuccessful local hypotheses remain in `unfinished` with their numerical diagnostics;
they are not promoted to actuals. Global non-exhaustiveness and bounded source-boundary sampling
live in `coverage[].limitations`, not in `unfinished_reasons`. Budget exhaustion or an incomplete
required stage still gives `partial`; moving coverage notes does not remove those reasons.

A `partial` document can contain independently usable actuals and candidates. Conversely, an
empty array does not prove absence. `no_related_feature` is currently issued for an exact zero
spectral XYZ signal, not from an empty finite sample. Ordinary invalid arguments remain C errors
and CLI errors, distinct from the structured unsupported-chain outcome.

### 7.2 What a position means

Three quantities must not be conflated:

1. **Physical/source position.** `physical_position` gives a locally corrected deviation minimum
   of the actual direction map, its source, positive finite-crystal support and numerical
   diagnostics. It is not the smoothed Y maximum and not a certificate of a global minimum.
   A finite source or distributed shape can leave this a conditional candidate.
2. **Fixed observation.** A peak, ridge or xy level set belongs to the recorded normalized vMF
   kernel (`kappa = 1/h²`), field/component, level and local window. The default `h` is one degree,
   and the local numerical location target is .05 degree; neither is an owner-defined universal
   physical precision. Prefix and independent-repeat movements concern this same observation.
3. **Scale response.** Changing `h` changes the observation. Its movement is reported separately,
   never used as an integration error or an uncertainty-band endpoint. A failed scale solve does
   not erase a valid fixed-observation result. Repeated estimates are evidence, not a rigorous
   global confidence bound.

Geometry is `point`, `polyline`, `band` or `atom`, with unit world **viewing** directions (negative
propagation). `field_points` retain the two-vector tangent basis, XYZ and two-dimensional xy jets,
normal curvatures, ESS and solver correction. Jets are per steradian, with derivatives per radian
and per radian squared. Field walks default to eight vertices; a point limit or observation mask
is a numerical/window endpoint, not a physical source edge.

A chromatic `band` stores **two solved levels of the same xy field**, their boundary points and
an explicit local-window definition. It is neither a natural unique “blue edge” nor a confidence
interval. Larger requested windows need their own convergence evidence: the committed eight-point
physical colour fixture fits its .05-degree local comparison budget including independent-reference
refinement; this is not a certificate for arbitrary extensions or all inputs.

A fixed shape/pose/point-source with discrete spectral support can produce positive `atom` records.
Finite spread, a finite sun, a continuous spectral quadrature node, or a sampled zero Jacobian does
not establish an atom. Two free ZYZ angles at a strict pole may describe one pose dimension;
parameter counts and the joint optical-map rank are kept separate.

### 7.3 Candidates, sources and endpoints

- `conditional_internal_tir` retains the actual internal slot, reached-interface incidence,
  discriminant, Fresnel factor and derivative availability. Every internal slot is eligible for
  the bounded search; there is no privileged first slot. It does **not** assert an observed blue band.
- `paired_interface` holds the original source geometry and actual spectrum fixed and removes
  only the named slot's reflectance. Its XYZ/xy effect is conditional on that source and sums all
  its exit directions; it is not the integrated sky-field effect or a unique-cause certificate.
- `product_area_threshold`, `geometric_contact_bracket` and `optical_domain_gate` keep different
  predicates. A source interval stores both endpoint poses/values and its width. The product
  `A=0` can coexist with positive raw corridor area below epsilon. Original clip-edge slot/index
  lineage comes from the existing clipper, not a second feasibility implementation.
- `declared_source_boundary` and `declared_source_corner` concern explicit uniform/cap/product-CDF
  coordinates with other draws fixed. They do not claim an outer sky boundary. Actual product CDF
  endpoints can differ from ideal continuous-distribution endpoints; no idealized substitute is used.
- `source_connected_feature` requires the same source identity and recorded numerical connector,
  never sky proximity. A positive source witness is only one contributor to the aggregate field.
  It is not a unique cause. Source tokens/feature ids are local to the document, not cross-run ids.

A source-range geometry point is explicitly the **valid-side witness**, not an exact gate image.
Unreached fields stay unavailable; a rejected optical side has no invented zero direction.
Conditional SO(3)/body-incidence event continuation is enabled only on the established Haar chart;
restricted source supports are not silently widened. Source walks have their own limit, separate
from the observation-window limit. The calculation does not certify all components, all junctions,
between-step absence of holes, or all weak features.

### 7.4 Examples and migration

```bash
# Actual scene spectrum, budget-aware prefix and partial results when necessary.
Lumice raypath -f config.json --crystal 1 --path 3-1-5 --report
# Explicit monochromatic diagnostic, not the scene SPD.
Lumice raypath -f config.json --crystal 1 --path 3-5 --report --wavelength 550 --events 8192
# Stop early, still serialize the available evidence rather than claiming absence.
Lumice raypath -f config.json --crystal 1 --path 3-5 --report --budget-ms 1
```

Version-1 `random_regular.*` ids, fixed red/blue endpoints, the formula detector matrix, unconditional
visibility claims, per-member LI-normalized brightness rows and `evidence_status: confirmed` are
removed. Read the three evidence buckets and their scope instead. Weighting now uses the product's
`2A/S` times all interface factors and the actual spectral XYZ coefficient, not the old nominal
`A*T` diagnostic convention. A reader requiring version 1 must reject version 2 rather than infer
old meanings. Output still goes to stdout or atomically to `-o`; progress goes to stderr. Ctrl-C
terminates the synchronous call without a completed JSON document and never replaces the output
file with half a document. The GUI workspace is not added by this interface.


## 8. Module boundary and surface

The raypath module under `src/raypath/` exposes exactly three public headers to the engine:

- `src/raypath/path_feature_report.hpp` — the feature-report entry point, narrowed to
  `PathFeatureReportRequest` + its constants + `AnalyzePathFeatureReport(config, request,
  product_version, std::string* json_out)`: the module runs the analysis, assembles the report and
  serializes it internally, so the caller never compiles the report types.
- `src/raypath/single_path_analysis.hpp` — `AnalyzeSinglePath` over the analytic kernel.
- `src/raypath/single_path_json.hpp` — `ToJson` / `ParseWarmSeeds` (`kSunGridSignificantDigits`
  included, as a published constant).

The bridge (`src/server/c_api_raypath.cpp`) compiles exactly these three (plus its own
server/config includes) — this minimal set *is* the module's surface to the engine, by
construction rather than by convention. Everything else under `src/raypath/` is module-internal
in one of two shapes: headers in `detail/` are internal by location and may be included only by
the module itself and its own tests (`test/unit-correctness/raypath/`,
`test/composition-correctness/raypath/`, which deliberately reach into `detail/` as the module's
own white-box oracle); the one legacy exception is `src/raypath/scene_to_analytic.{hpp,cpp}`,
internal by consumer fact though not relocated — its only includes are `single_path_analysis.cpp`,
`detail/input_assembly.cpp` and the module's own test — and a candidate for a future `detail/`
move in its own change. `detail/path_feature_report_json.{hpp,cpp}` is internal on the same
grounds: the bridge takes a JSON string now, and the only other consumer of the report types ever
was that same composition test.

The boundary is cheap to re-verify mechanically — the include closure of the three public
headers must contain no `raypath/detail` file and no `analytic/diagnostic_batch`:

```bash
printf '#include "raypath/path_feature_report.hpp"
#include "raypath/single_path_analysis.hpp"
#include "raypath/single_path_json.hpp"
' | g++ -H -fsyntax-only -x c++ - -std=c++17 -I src 2>&1 | grep -cE "analytic/diagnostic_batch|feature_discovery|raypath/detail"
```

`0` is the expected answer. This is the probe that pinned the boundary when the bridge surface
was narrowed (its baseline counted 6 hits — four internal headers plus the analytic kernel
surface — before the narrowing landed).
