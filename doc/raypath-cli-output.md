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
| `target.altitude_deg`, `target.azimuth_deg`, `target.direction`, `target.deviation_deg` | The sky point; `direction` is the light-travel direction arriving from it (§4); `deviation_deg` is the sun–target angle |
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
| `outgoing_direction` | World, the exit propagation — displayed at the sky point it comes from (§4; zeros when not valid) |
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
- Directions follow the light-travel convention (`doc/coordinate-convention.md`, "Direction-Vector
  Semantics"): `incident_direction` propagates sun → crystal; `target_direction` is the propagation of
  light arriving from the target sky point (the search seeks exits displayed there); `outgoing_direction`
  is the exit propagation, displayed at the sky point it comes from. For any such `d` that point sits at
  altitude `asin(-d.z)`, azimuth `atan2(y, x) − 180`.
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

## 7. Target-free path feature report (schema 3)

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

The document has two evidence layers. The **geometry layer** enumerates the chain's structural
objects on the orientation-reduced sphere (the `u`-S² pre-image: critical sets of the deviation
map, gate boundaries, transmission-weight kinks, restricted-family images) and attaches exact
certificates — the delta-axis partition, endpoint onsets, visibility certificates, chromatic
verdicts. The **MC layer** is demoted to corroboration: the schema-2 observation records ride
along as `mc_evidence`, they may support or contradict a structural object, and they neither
promote nor erase one. An MC red flag (`not_observed_despite_sufficient_ess`, an
`sufficient-ESS` unattributed structure) is reported as a disagreement, never resolved silently.

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
only after it finishes. Bounded serialization follows the numerical deadline. The MC and
attribution legs are deadline-bounded by that quality budget. The geometry layer's
**enumeration leg** carries two different kinds of bound, and the difference is the contract:

- Its **quality budgets are report-only** — the leg's cost is measured and reported
  (`timing.enumeration`, `budgets.enumeration`), never clamped by `--budget-ms`.
- A fixed **anti-hang cap is enforced** (`budgets.enumeration.hang_cap_ms`, 300000 ms): a
  member or leg the cap cuts ships the honest truncated face (`walk_truncated`, `walk_s` 0.0,
  no curve body) and `budgets.enumeration.truncated` plus `truncation_note` name what was cut.
  The cap is a robustness floor against pathological inputs, not a quality knob, and is
  deliberately not coupled to `--budget-ms`.

Two declared refusal bounds ride beside it, same honesty shape: an object whose curve body is
denser than the carry bound ships truncated with no body (real kernel output the report leg
declines to carry), and a chromatic feature whose red/blue curves are too dense to compare is
reported not assessed inside its verdict (`coverage_complete` false, the refusal in `notes`) —
never half-compared. Sampling is replayable, but which partial records fit a wall-clock deadline
can vary across machines.

### 7.1 One document and one owner

`LUMICE_AnalyzePathFeatureReport` returns an immutable opaque result. CLI and C clients read the
same serialization. The v4.52 request suffix is read only when its complete group fits
`struct_size`; the v4.51 prefix remains accepted with the new defaults.
The JSON schema changed meaning and is therefore **version 3**, not an append to version 2.
A reader requiring version 2 must reject version 3, and a version-3 reader must reject version 2
(§1 version discipline; the in-repo face is the e2e `_load_report` gate).
The target/fiber document and `--warm` contract in sections 1–6 are unchanged.

An unsupported-chain document keeps the early shape: `schema`, `schema_version`, `outcome`,
`requested_path_layers`, the three empty feature buckets, an empty `mc_evidence`, the
`not_supported` coverage row and zero-work budgets — the requested input, no fabricated payload.

### 7.2 Top-level keys

| Key | Meaning |
|---|---|
| `schema`, `schema_version` | `lumice.path-feature-report`, 3 |
| `generator` | Lumice version and the analytic kernel's `analytic_api_version` |
| `conventions` | Unit conventions plus the spelling notes: `walk_s` (null = no arclength applies; 0.0 = truncated with no pre-truncation arclength exposed), `null` (JSON cannot spell non-finite numbers), `min_margin_rad`, `features_buckets`, `chromatic_thresholds` |
| `outcome` | `completed`, `partial`, `unsupported_multicrystal`, or `no_related_feature` |
| `scope` | The input identity: scene identity, spectral scope and rows, seed, light, layers, physical members, support dimensions, request echo |
| `support` | The delta-axis block: per-member partition intervals, endpoint onsets, constant curves; the family aggregate (§7.3) |
| `features` | `{actual, candidate, unfinished}` — a **derived view** over the structure-object records (existence × visibility); no bucket is stored on an object (§7.4) |
| `unattributed_structures` | The MC side's forward detector: sky structures no object accounts for, with the detector's silences visible (§7.5) |
| `mc_evidence` | The demoted observation record: the schema-2 feature records with classification preserved, observation options, spectral verification, stage incompleteness (§7.5) |
| `coverage` | Bounded-search honesty rows, the object-kind vocabulary row, declared measure/density skips |
| `observation` | Narrowed to the corroboration-observation declaration; `ruler` points at `mc_evidence.observation_options` |
| `budgets` | The work account, including the `enumeration` and `attribution` legs; `budgets.enumeration` also carries the enforced anti-hang cap (`hang_cap_ms`, `truncated`, `truncation_note`) |
| `timing` | Per-stage seconds, including `enumeration` and `attribution` |
| `sources` | Token → `{outer_sample, member, spectral_row}`; records also carry their own witness objects |
| `no_related_feature` | The absence ruling: `issued`, `basis` and the named checks, emitted unconditionally (§7.6) |

`completed` means the declared bounded stages finished, not that every hypothesis was
corroborated. MC-side incompleteness — budget exhaustion, or unfinished stage strings in
`mc_evidence.unfinished` — yields `partial`; a `partial` document can still carry usable
structure. `no_related_feature` is the absence ruling of §7.6, never an empty-sample inference.
Ordinary invalid arguments remain C errors and CLI errors, distinct from the structured
unsupported-chain outcome.

### 7.3 scope and support

`scope` carries the assembled input, verbatim:

- `scene_identity`, `seed`, `member_semantics` (`"physical_L2"`), `light`;
- `layers[]` (`scene_layer`, `crystal`, `representative_faces`, `symmetry_bits`) and
  `physical_members` — concrete physical-L2 face sequences, never an L1 label orbit;
- `spectrum[]` rows (`nm`, `source_weight`, `measure_mass`, `index`, `XYZ_coefficient`,
  `provenance`) and `spectrum_scope` — **two keys, two things**: the assembled rows versus the
  one-string declaration of what the spectrum is (the v2 document's single `spectrum` string
  reading no longer exists);
- `support_dimensions` (pose/shape/source/spectral dimension counts; `joint_optical_rank` stays
  null with its `rank_scope` caveat), `requested_outer_samples` (the request verbatim; 0 = auto —
  the resolved count lives in `budgets.requested_outer_samples`), `budget_ms`.

`support` is the delta-axis face — the schema-1 `reach`, in target-free form, per physical
member:

- `partition`: `coverage` (`complete` / `incomplete` / `unknown`), `walk_status`, `walk_closed`,
  `intervals[]` (`lower_rad`, `upper_rad`, `n_components`, `n_closed`, `n_open`). When the
  partition walk escaped, `escape_regime_slug` names the regime — the slug is data, not an enum
  promise. The typed `escape_regime` appears only beside a non-empty `escape_regime_slug` (the
  two gates stack: empty slug ⇒ no typed key), and only when a producer set a contract-registered
  regime (none does today: every kernel regime slug lacks a contract value); at the `unset`
  default — no escape, or a refusal whose regime the slug alone names — the key is omitted rather
  than spelled `"unset"`, and `null` stays reserved for non-finite numbers. A `message` may ride
  beside a refusal.
- `endpoint_onsets[]`: every partition endpoint carries one — `value_rad`, `location`
  (`boundary` / `interior`), `source` (`interior_minimum` / `boundary_extremum` / `corner`),
  `profile` (`finite_jump` / `boundary_onset` / `log_divergence`), `gradient_norm`,
  `multiplicity`, and `measure_limit` when `has_measure_limit`.
- `constant_curves[]`: constant-D_P closed curves with their n-continuation dispersion table —
  `d_p_rad`, `weight_step`, `wavelengths_nm[]`, `critical_d_p_rad[]` (one critical value per
  spectral row). This is how a structure position is reproduced at every wavelength without
  re-walking.
- `families[]`: the per-family aggregate — `shared`, `intervals`, `members`, `phi_class_note`.
  A non-shared family emits an **empty** `intervals` list by contract: a disagreeing family
  reports no union rather than a fabricated one, and the per-member rows above are the authority.

### 7.4 Structure objects and the derived buckets

Each object record carries its identity — `kind` (an open registry, currently `kind_1`,
`kind_1_restricted`, `kind_2`, `kind_3`, `corridor_closed`, `s3_support_boundary`,
`s4_branch_boundary`, `s5_density_feature`, `s6_junction`), `member`, `slot`,
`phi_class_note` — and four state machines:

- `existence`: `computed` | `escaped` (with `escape_regime_slug` — the regime name is data) |
  `walk_truncated` | `s4_declared`. `walk_s` follows the conventions note above.
- `visibility`: `certified` | `partial` | `unlit` | `unproven`, with `lit_fraction`, the
  `evidence` form (`structural` / `sampled_exhaustive` / `sampled_partial`), `jets_ok`,
  `saw_zero_area`, `saw_zero_transmission` and a `reason`. `certified` is the pointwise
  certificate (measure · area · transmission > 0 with non-degenerate jets over in-support
  samples). `unlit` means the geometry is there and the light is not — it lands in `candidate`,
  which is the precise form of "has a contour, no illumination".
- `chromatic`: `assessed` + `verdict` + `thresholds`. The verdict carries `kind`, `color`,
  `visible`, `coverage_complete`, `faces`, `n_red`/`n_blue`, optional `position_rad`,
  `features[]` (per-feature `kind`/`color`/`delta_red`/`delta_blue`/`shift`/`spread`/
  `contrast`/`weight`/`lit_fraction`), optional `tint` (`energy_red`, `energy_blue`, `ratio`,
  `tir_fraction_red`, `tir_fraction_blue`, `direction_dispersion`) and `notes`. `thresholds`
  ride every object: they are declared calibration parameters, not universal physics.
- `corroboration`: `observed` | `consistent` | `not_observed_insufficient_ess` |
  `not_observed_despite_sufficient_ess` (a true-disagreement red flag) | `unchecked`, with the
  ruler text, `tolerance_rad`, and the matched MC record index and its ESS. The base ruler
  matches on the delta axis and does not discriminate azimuth — a declared limitation, spelled
  in the `ruler` field.

Geometry holds `u` — the orientation-reduced pre-image, a first-class citizen — and
`sky_position` when the object has one (unit world **viewing** directions, negative
propagation). `diagnostics.counterfactual` is the paired counterfactual
(`available`, `with_slot`, `without_slot`): the same source with one slot's reflectance removed.

The three `features` buckets are a derived view, computed as computed+`certified` → `actual`;
computed+`partial`/`unlit`/`unproven` → `candidate`; everything else → `unfinished`. No bucket
field is stored on an object; a reader re-derives or trusts, but cannot find a stored bucket to
drift from the states.

### 7.5 mc_evidence: the demoted observation record

**Share-domain caveat (added after the frame-vs-analysis calibration audit).** Every
`mc_evidence` field whose semantics compares an MC share against a render-arm
denominator inherits two calibrated facts: an analysis share lives on the whole-sky
domain while a render frame share lives on the lens landing domain
(`doc/coordinate-convention.md` §13), and — until the dual-fisheye fold-boundary
defect is fixed — a dual-fisheye frame additionally under-counts by the scene's
direct-through energy (measured 18.9% on the reference scene, in a scene-dependent
way). Same-domain (cone ROI) comparison is the only exact form; whole-sky-vs-frame
share comparisons are corroboration-grade, never exact — with one exception: the
rectangular full-sky frame's landing domain *is* the whole sky, which is what makes
it the calibration arm (agreement to 4e-8 relative, unit-pinned).

`mc_evidence.records` carries the schema-2 discovery records unchanged in shape, with the
classification preserved verbatim on each record's `evidence` field (`actual` / `candidate` /
`unfinished`). Record kinds keep their schema-2 semantics:

- `physical_position` is a locally corrected deviation minimum of the actual direction map, not
  the smoothed Y maximum and not a global-minimum certificate.
- `conditional_internal_tir` retains the internal slot, reached-interface incidence,
  discriminant and Fresnel factor; it does **not** assert an observed blue band.
- `paired_interface` removes only the named slot's reflectance at fixed source geometry; its
  XYZ/xy effect is conditional on that source and is not the integrated sky-field effect.
- `declared_source_boundary` / `declared_source_corner` concern explicit uniform/cap/product-CDF
  coordinates with other draws fixed; they do not claim an outer sky boundary.

The three-quantities discipline is unchanged — physical/source position ≠ fixed observation
(normalized vMF kernel, `kappa = 1/h²`, default `h` one degree, `.05`-degree location target) ≠
scale response — but it now scopes the corroboration layer only. Record-level shapes keep their
schema-2 contracts: geometry is `point`, `polyline`, `band` or `atom`; `field_points` retain the
two-vector tangent basis, XYZ and 2D xy jets, normal curvatures, ESS and solver correction (jets
per steradian, derivatives per radian / radian²; field walks default to eight vertices, and a
point limit or observation mask is a numerical/window endpoint, not a physical source edge). A
chromatic `band` stores two solved levels of the same xy field with an explicit local-window
definition — neither a natural unique "blue edge" nor a confidence interval. A fixed
shape/pose/point-source with discrete spectral support can produce positive `atom` records;
finite spread, a finite sun or a continuous quadrature node does not establish one.
`product_area_threshold`, `geometric_contact_bracket` and `optical_domain_gate` keep different
predicates; a source interval stores both endpoint poses/values and its width. A source-range
geometry point is explicitly the **valid-side witness**, not an exact gate image; unreached
fields stay unavailable and a rejected optical side has no invented zero direction.
`source_connected_feature` requires the same source identity and recorded numerical connector,
never sky proximity; source tokens and record ids are local to the document, not cross-run ids.

`observation_options`, `spectral_verification` (which can downgrade records),
`observation_scope_note` and `unfinished` (stage-level incompleteness strings) complete the
block.

`unattributed_structures` is the forward direction: an MC-significant sky structure is checked
against every object image (tolerance `max(h, declared width)`), gated by an ESS floor so a
starved MC cannot vote. Each structure reports `record_index`, `position`, `delta_rad`,
`record_ess`, `min_margin_rad` and the `ruler`; the detector's silences are visible
(`skipped_no_position`, `skipped_below_ess`) and a detector failure is an `error`, not an empty
array.

### 7.6 no_related_feature: the dual-form ruling

The outcome value `no_related_feature` is issued only by the ruling, and the ruling block is
emitted unconditionally — a refusal to issue is itself part of the report:

- Form A, `basis: "zero_spectral_signal"`: an exact zero spectral XYZ signal. Zero signal
  implies absence by itself; A has priority over B.
- Form B, `basis: "certified_smooth_radiance"`: a certificate, checked and named field by field
  — `partition_complete_no_escape`, `all_objects_unlit_or_none` (the check is over **lit**
  objects; kind-1 is pure geometry and exists even where A = 0), `no_sufficient_ess_unattributed`
  (a sufficient-ESS unattributed structure vetoes B), `s4_scope_declared` (the v1 certificate
  does not cover branch-boundary features), `two_d_valid_support` (B v1 signs only on 2D-valid
  support). Any failed check withholds the basis.
- When neither form is licensed, the block still reports every check with `issued: false` and
  the fixed note: the document reports `completed` with an empty feature list, and an empty list
  does not prove absence.
- `basis` also rides the top level beside `outcome`, exactly when issued. A restricted family's
  absence is expressed by the support block — the restricted image itself is the feature — not
  by form B.

### 7.7 Version discipline and the v2 → v3 migration map

A reader requiring version 2 must reject version 3 rather than infer old meanings. The mechanical
authority for this map is the migration ledger the unit tests reconcile against; the summary:

| v2 key | v3 home |
|---|---|
| `support` (dimension description) | `scope.support_dimensions` — the v3 top-level `support` name is reused by the delta-axis block |
| `physical_members` | `scope.physical_members` |
| `spectrum` | `scope.spectrum` (rows); the declared string splits to `scope.spectrum_scope` |
| `requested_outer_samples`, `budget_ms` | `scope.requested_outer_samples`, `scope.budget_ms` (the resolved count stays in `budgets.requested_outer_samples`) |
| `observation` | demoted into `mc_evidence.observation_options` + the scope note; the v3 top-level `observation` is re-issued narrowed to the corroboration declaration |
| `spectral_verification` | `mc_evidence.spectral_verification` |
| `actual_features` / `candidates` / `unfinished` arrays | `mc_evidence.records`, classification kept on each record's `evidence` field |
| `unfinished_reasons` | `mc_evidence.unfinished` |
| `sources` | the re-derived projection beside `mc_evidence.records[].source_token` |
| — | new: `support` (delta axis), `features` buckets over structure objects, `unattributed_structures`, `no_related_feature` (ruling), `generator`, the `enumeration`/`attribution` budget and timing legs |

Version-1 removals stay removed (old `random_regular.*` ids, fixed red/blue endpoints, the
formula detector matrix, unconditional visibility claims, `evidence_status: confirmed`).
Per-member LI-normalized brightness rows stay removed; the chromatic verdict's `tint` block is
the schema-3 colour-ratio face. Weighting still uses the product's `2A/S` times all interface
factors and the actual spectral XYZ coefficient.

### 7.8 Examples

```bash
# Actual scene spectrum, budget-aware prefix and partial results when necessary.
Lumice raypath -f config.json --crystal 1 --path 3-1-5 --report
# Explicit monochromatic diagnostic, not the scene SPD.
Lumice raypath -f config.json --crystal 1 --path 3-5 --report --wavelength 550 --events 8192
# Stop early, still serialize the available evidence rather than claiming absence.
Lumice raypath -f config.json --crystal 1 --path 3-5 --report --budget-ms 1
```

Output goes to stdout or atomically to `-o`; progress goes to stderr. Ctrl-C terminates the
synchronous call without a completed JSON document and never replaces the output file with half
a document. The GUI workspace is not added by this interface.


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
