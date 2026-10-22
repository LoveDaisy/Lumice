# `Lumice raypath` output contracts

`Lumice raypath` analyses **one single-layer raypath of one crystal entry over one sky point**:
the components of the fiber of crystal poses that send the sun into that point, each component's
poses with per-pose detail, and the path's deviation over the whole sun-direction sphere
(`doc/raypath/raypath-analysis.md` §5.1.8). `analyze` lists the raypaths that light the sky; `raypath` is
what you ask about one of them.

It writes one JSON document. This page is that document's field reference, and the one place a
reader (the Analyze-workspace prototype, a script, the GUI later) should learn it from.

There are three deliberately separate documents. `raypath point` writes the target-fiber
document (`schema_version: 1`, sections 1–6); its keys, ordering, and `--warm` behavior are
supersets of the original `--target` form — fields are only ever appended. `raypath report`
writes the target-free `lumice.path-feature-report` document in section 7. `raypath survey`
writes the census document (`lumice.path-survey`, schema_version 4) in section 9. Neither mode
silently substitutes for another.

## 1. Where the document comes from

The CLI is a thin shell. The computation and the serialization both live in the engine:

| Piece | Where |
|---|---|
| The analysis | `lumice::raypath::AnalyzeSinglePath` (`src/raypath/single_path_analysis.hpp`) over the analytic kernel (`src/analytic/`) |
| The JSON form and the `--warm` reader | `lumice::raypath::ToJson` / `ParseWarmSeeds` (`src/raypath/single_path_json.hpp`) — one file, shared key names |
| The public entry point | `LUMICE_AnalyzeSinglePath` → opaque `LUMICE_SinglePathResult` → `LUMICE_SinglePathResultToJson` (`lumice_raypath.h`, v4.50) |
| The subcommand | `src/main.cpp` (`ParsePointOptions` / `RunPoint`; family dispatch in `main`) |

So the GUI, when it grows an Analyze workspace, reads **the same document** through the same
`lumice_raypath.h` call; there is no second serializer to drift from this one. The result is exposed as
JSON rather than as a C struct mirror on purpose: it holds variable-length nested lists whose
fields an interface will keep adding to. Typed readers can be appended to `lumice_raypath.h` later.

## 2. Command line

The `raypath` family asks one path four ways — the word after `raypath` names the question, and
bare `raypath` is a usage error:

```
Lumice raypath survey -f <config> --crystal <id> --path <faces> [--literal] [--lambda 550]   # schema 4 (§9)
Lumice raypath point  -f <config> --crystal <id> --path <faces> --target <alt>,<az> [--warm s.json]  # schema 1 (§3)
Lumice raypath probe  -f <config> --crystal <id> --path <faces> --along <form> --at <alt>,<az>       # schema 5 (§10)
                      [--lambda nm[,nm...]] [--events N] [--budget-ms N] [--warm s.json]
Lumice raypath report -f <config> --crystal <id> --path <faces> [--events N] ...              # schema 3 (§7)
```

| Option | Meaning |
|---|---|
| `--crystal <id>` | The crystal entry (config id). Required. |
| `--path <faces>` | The raypath as `analyze` prints it: `3-5`, `3-6-4-8`; `C1(3-5)` if the layer names its crystal (must equal `--crystal`). Multi-layer chains (`(3-5) -> (1-3)`) are handed to the engine, which refuses them as `multi_layer_unsupported`. Required. |
| `--target <alt>,<az>` | The sky point, degrees; azimuth measured as the sun's (the `analyze --center` convention). Required for `point`. |
| `--along <form>` | `probe` only: `radial` \| `edge-normal` \| `arc` \| `tangential` — which line the probe library profiles (§10). Required. |
| `--at <alt>,<az>` | `probe` only: the sky point the line passes through, same convention as `--target`. Required for every form — including `radial`, whose value is azimuth-free (the document declares it, and the warm reuse does not compare the azimuth). |
| `--lambda <nm>[,<nm>...]` | `probe` only: one or more wavelengths in [350, 900], comma-separated or the flag repeated — the chromatic lambda grid lives on the line (one profile per wavelength, each with its own refractive index), never a multi-wavelength map. Default: the config's single wavelength or 550. |
| `--wavelength <nm>` | In [350, 900]. Default: the config's, when its spectrum is exactly one wavelength; else 550. The choice is recorded in `meta.wavelength.source`. (`report` without it uses the actual scene spectrum.) |
| `--events <N>` | Seed events of the component search — poses sampled to locate the fiber, **not** traced rays. K/M suffix; at most 100M (`LUMICE_SINGLE_PATH_MAX_SAMPLE_COUNT`). Default 1M. (`report`: even outer samples, its own range. `probe`: the band-sum lattice n, default 2000000 — the survey probe's ruler.) |
| `--budget-ms <N>` | `point`, `probe` and `report`: the numerical deadline in milliseconds (default 15000; max 120000; `probe` checks it at the per-wavelength boundaries). Checked at the computation's loop boundaries: a stopped run serializes what it has and reports `meta.interrupted` — an honest early exit, exit code 0. Two legs are deliberately outside the polling: the inline context-only survey assembly (T0–T2, cheap and bounded by design) and the fiber continuation's inner steps (bounded by the `--max-evaluations` family instead) — a heavy case can overshoot the declared value by their cost |
| `--grid <rows>` | Latitude rows of the sun-direction grid; longitude is twice that. [0, 720]; 0 leaves `sun_grid` out. Default 90. |
| `--warm <file>` | What a previous output is worth (§5): a schema-1 `point` document's seeds start the search; a schema-4 `survey` document is the index the query consumes (§5, the `survey_context` block). |
| `-o <path>` | Write the document here instead of stdout (never both). |
| `-v` / `-d` / `-h` | As for every subcommand. `-h` after a command word prints that command's own page; bare `raypath -h` lists the four commands. `--backend`, `--workers`, `--seed` are not options here: the analysis has one route, runs on the calling thread and draws no random numbers. |

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
| `conventions` | object of strings | always | This page's §4 in words, so a document explains itself: `frames`, `directions`, `target_azimuth`, `pose`, `angles`, `sun_in_crystal`, `units`, `null`, `factors`, `survey_context`, `sun_grid`, `completeness`, `reach` |
| `meta` | object | always | Every conversion from the scene to the kernel, as applied (§3.2) |
| `outcome` | `"discovered"` \| `"point_mass"` | always | Which of the two branches below is present |
| `point_mass` | object | `outcome == "point_mass"` | §3.5 |
| `components` | array | `outcome == "discovered"` | §3.3; may be empty |
| `incomplete` | array | `outcome == "discovered"` | §3.4; may be empty |
| `discovery` | object | `outcome == "discovered"` | §3.6 |
| `reach` | object | `outcome == "discovered"` | §3.8; written whether or not `components` is empty |
| `measure_registration` | object | `outcome == "discovered"` | The declared measure's support kind and any registered read-as-zero gap (§3.11) |
| `sun_grid` | object | `--grid` > 0 (either outcome) | §3.7 |
| `survey_context` | object | always | Where the target sits in the survey's structure (§3.12; the `--warm` contract's query face) |

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
| `budget_ms`, `interrupted` | The budget receipt: the ms the deadline was in force for (0 = unbounded), and whether the run stopped early — a partial document, honestly marked (§2, `--budget-ms`) |

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
| `rho` | The declared joint pose density w.r.t. dλ dφ droll (radians, angle Lebesgue) at the pose's composed angles — the measure factor of the decomposition (§3.11) |
| `entry_measure_a2` | A/a²: `entry_measure` over the crystal length unit squared (a = 1 in this repo's configs — a declared identity, not a normalization) |
| `jacobian.value`, `jacobian.singular_values`, `jacobian.available` | The pose's normal Jacobian J_perp = σ1σ2 (the kernel's own read at the pose's own direction); `null` where it cannot be formed — NaN-honest, never a plausible 0 or 1 |
| `k_carea` | 1/(J_perp + `factors.epsilon`); `null` when the Jacobian is unavailable |
| `factor_product` | ρ · A/a² · T · K; `null` when any factor is unavailable or the pose is not valid |

The factors are the L2 query's per-pose decomposition: **multiplicative, in real units, never
additive percentages** (the corpus discipline the point output inherits). The document-level
`measure_registration` (§3.11) must be read before treating a `rho` of 0 as dark.

Each component carries one `factors` block:

| Key | Meaning |
|---|---|
| `epsilon` | The declared ε of k_carea (1e-12; data, never a hidden knob) |
| `fiber_integral` | Σ `factor_product` · arclength over consecutive points — the component's factor-weighted integral; `null` when no segment contributed |
| `fiber_integral_rule` | The summation rule, spelled in the document |
| `completeness.available_points`, `.total_points`, `.note` | How many points carried a usable product — procedural, the discovery note's own discipline; the integral covers the arclength those points open, not the component's full length |

### 3.4 `incomplete[]`

Candidates the search found but could not turn into a component: `cause` (`arc_backward_failed`,
`arc_backward_closed_anomaly`, `unnamed_event`, `not_converged`), `seed`, `forward`, and
`backward` only for the two `arc_backward_*` causes.

### 3.5 `point_mass`

Rank 0: the outgoing direction does not depend on the pose (e.g. `1-2`, a flat plate's two basal
faces). No fiber exists to trace. `direction` (world), `altitude_deg` / `azimuth_deg` (the sky
point it lands on), `target_separation_deg` (from the requested target), and `orbit_factors` —
the rank-0 branch's factor decomposition:

| Key | Meaning |
|---|---|
| `available` | False: the block is the honest unavailable state — `measure_support` names what the declared measure's support actually is and `reason` the named cause (e.g. `measure_support_not_spin_orbit`); no integral fields appear. True: the fields below |
| `measure_support` | The support kind the block integrates along |
| `sample_count` | Midpoint samples over [0, 2π) of the declared measure's spin orbit (1024 — the declared caliber) |
| `integral` | Σ ρ_θ · A/a² · T · Δθ over kept samples (A·T > 0 and valid — the measure layer's positive-part convention). For a uniform-azimuth orbit this is the LI #51 tint kernel's integral form, and the quotient of two runs at the wavelength-pool ends is that spot's tint |
| `integral_rule`, `kept_count` | The summation rule; the kept count (so "all dark" stays distinguishable from "no samples") |
| `points[]` | Per θ: `theta_rad`, `rho_theta`, `entry_measure_a2`, `total_transmission`, `valid` |

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

### 3.11 `measure_registration` (the declared measure's registration)

Written on the discovered branch. The declared measure's u-marginal support kind
(`point` / `spin_orbit` / `roll_orbit` / `latitude_orbit` / `area` /
`degenerate_sun_geometry` / `degenerate_axis_geometry`), whether the zonal fast path was in
force, and — when the measure is one of the density layer's registered read-as-zero families —
`registered_gap: true` with the named gap in `note`. A `rho` of 0 in a registered-gap family
means "the density layer did not answer", never "dark". The joint pose density itself is
closed-form at every pose; the gaps live one reduction up, at the u-marginal a solid-angle
consumer would read.

### 3.12 `survey_context` (where the target sits in the survey's structure)

The `--warm` contract's query face (§5): with a warm survey document, its facts; without one, the
same facts from a context-only (T0–T2) assembly run on this request's own config. Every field
carries its own state — `certified` (the support partition's containment fact), `finite` (a
computed reading), `unevaluated` with a named reason — so absence can never read as answered.

| Key | Meaning |
|---|---|
| `source` | `"warm"` (the given schema-4 document's facts) or `"inline"` (the context-only assembly) |
| `warm_index_absent` | True: a `--warm` schema-4 document was given but carried no (complete) `warm_index` block — the documented degradation (§5); the context still answers, inline |
| `object.state` (+ `reason`) | The target's deviation against the delta-axis support: `certified` (inside an interval, distance 0, or the nearest card's distance) or `unevaluated`. The value fields `target_inside_support`, `kind`, `members`, `distance_deg` exist exactly when the state is `certified` — an unevaluated object carries no distance, so absence can never be misread as the certified 0 that means "inside the support" |
| `edge.state` (+ `reason`) | The angular distance to the nearest edge. The value fields `distance_deg`, `basis` (`support_interval_endpoint` or `caustic_edge_position`) exist exactly when the state is `finite` |
| `band_sum.state` (+ `value`, `delta_deg`, `bin_deg`, `caliber`, or `reason`) | The warm document's solar-radial band-sum reading at the nearest probe sample. Consumed only when the scope gate passes: the query's resolved wavelength and sun must EQUAL the document's scope receipt exactly (identity, not approximation); the two named failures are `scope_field_missing` (a receipt field absent) and `scope_value_mismatch` (a value differs — a moved sun or a changed lambda). Inline contexts and degraded warm files name why they have no reading (`inline_context_runs_no_probe`, `warm_index_absent`, `warm_document_has_no_probe`) |

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
  crystal frame, `+z` its c-axis (`doc/geometry/coordinate-convention.md`).
- Directions follow the light-travel convention (`doc/geometry/coordinate-convention.md`, "Direction-Vector
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

## 5. `--warm`: two document families, one flag

`--warm <file>` reads what a previous document is worth. The document's family decides:

**A schema-1 `point` output** — the seed sheet. Its `components[*].seed` and then
`incomplete[*].seed` (9 row-major doubles each) start the search as extra starting poses.
Densifying the search is not monotone — a denser sample can lose a component a sparser one found —
so feeding back seeds is how a component, once found, is kept. The file must carry the same
`schema_version`, and every seed must be 9 numbers; otherwise the run fails with
`invalid_argument: warm seeds: …`. A point-mass output is accepted and holds no seeds.

**A schema-4 `survey` document** — the index. "Survey is the query's index" made concrete: the
query consumes the census's routing instead of re-deriving it. Consumption has three parts.

*Consistency gates (refusals).* The query refuses with `invalid_argument: warm index: <gate>: …`
when any of these fails:

1. **Crystal** — the document's `request.crystal_id` must equal the query's.
2. **Path** — the document's `request.path_layers` must equal the query's.
3. **Config fingerprint** — the document's `warm_index.config_fingerprint` must equal the query
   config's. The fingerprint is the hex SHA256 of the config's **routing-axis projection**,
   canonical JSON with the axis-set tag `lumice.routing-fingerprint-v1`: the selected crystal
   entry whole (shape param and axis distribution, the config's own serializers), the literal path
   layers, and the symmetry bits. Excluded on purpose (the routing ruling: sun and spectrum do not
   route): the sun position, the wavelength/spectrum; also the crystal's scene-layer allocation
   and every other scene/renderer field. One implementation computes it for both sides (the
   survey's writer and the query's reader); a config field that starts to affect routing must be
   added to the projection and to this axis list in the same change.
4. **Measure echo** — only family-arm documents carry `family_routing`; when present, its
   `measure` echo (kind/center/width per dimension) must equal the query config's own reading.
   Its absence is data (that arm did not run) and skips the gate with a declaration. (The
   fingerprint already carries the axis; this gate is the echo's independent chain.)

*The seed harvest.* The document's `warm_index.seeds` (each 9 numbers, with `seed_sources`
naming the leg that held the pose — `witness_argmax`, `family_collapse_anchor`,
`family_fold_anchor`, `single_pose_nominal`; arms that hold no pose leave `seeds: []`) join the
search as warm seeds.

*The survey context.* The document's structure answers where the target sits: the owning support
interval, the edge distance, and — when the document ran the solar-radial probe — the band-sum
reading at the nearest sample (§3.12). Without a `--warm` file the query computes the same
answers from a context-only (T0–T2) assembly on its own config: cheap by construction, and the
band-sum reading is then honestly absent (`inline_context_runs_no_probe`) — the probe is T3
quantification the context-only mode does not run.

**Degradation, not refusal.** A schema-4 document whose `warm_index` block is absent — every
survey document written before the block existed — or missing any of its fields is not refused
and never partially consumed: the query degrades visibly to the inline context (no structural
reuse, no seed consumption), and the document's `survey_context` declares it
(`warm_index_absent: true`, the band-sum reading `unevaluated` with that reason).

**The probe's consumption.** `raypath probe` reads the same index with the same gates. The
harvest's structure answers where the at point sits (the context block of §10), and the document's
radial probe samples become the radial form's profile when — and only when — the scope matches:
the requested wavelength equals the document's `scope.wavelength_nm`, the sun position equals the
document's, the document carries a probe at the same 0.1-degree bin, and the requested band-sum
lattice equals the count the warm values were computed at. The profile then names its provenance
`warm_reused` (the values are the document's own, checked bin-centre bit-equal; zero band-sum
calls) and every other profile names itself `recomputed`. The azimuth is deliberately absent from
the comparison: the radial profile's value is azimuth-free — a uniform-density property, the
condition `conventions.value` states — the form's declared semantics.

*The reuse criterion's premise.* That check is the whole check because the ruler constants are
schema: the 0.1-degree bin, the survey wedge, and the lattice n are constants of the probe's
caliber, echoed in every profile's spec, so the asked grid matching the warm grid is guaranteed by
the constants being equal. If any of them ever becomes a knob, the reuse criterion must grow the
comparison — a knobbed ruler silently reuses values computed on a different ruler otherwise. The
azimuth exemption rides its own stated condition: the radial value is azimuth-free under the
uniform density (`conventions.value`), and a measure outside that condition has no azimuth-free
line to exempt.

**Versions.** The schema-4 arm does not compare `schema_version` (the schema-1 arm above does):
the family name is the discriminator, `warm_index.produced_for` records what produced the
document, and append-only growth freezes the semantics of the keys this reader consumes. A
future change to a consumed key's meaning is the one thing that must gate on `schema_version`
here — that is this contract's version discipline.

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

The registry's **production coverage is itself reported** in the `coverage` block's vocabulary
row (`produced` / `declared_not_produced`, from `V1Coverage()`), so a reader can tell planned
vocabulary from what the enumerator actually emits. Today `produced` holds `kind_1`,
`kind_1_restricted`, `kind_2`, `kind_3` and `s6_junction`; `corridor_closed` (no v8 producer for
the A=0 corridor — the LI second batch is the donor), `s3_support_boundary`, `s4_branch_boundary`
(owner ruling: declared, not traced) and `s5_density_feature` (v2 measure layer) are declared
but never produced, each with its reason carried beside it. An object kind outside `produced`
cannot appear in `features[]` of this build.

### 7.5 mc_evidence: the demoted observation record

**Share-domain caveat (added after the frame-vs-analysis calibration audit).** Every
`mc_evidence` field whose semantics compares an MC share against a render-arm
denominator inherits a calibrated fact: an analysis share lives on the whole-sky
domain while a render frame share lives on the lens landing domain
(`doc/geometry/coordinate-convention.md` §13). Same-domain (cone ROI) comparison is the only
exact form; whole-sky-vs-frame share comparisons are corroboration-grade, never
exact — with two exceptions whose landing domain *is* (or now, after the
dual-fisheye fold-boundary fix, is again) the whole sky: the rectangular
full-sky frame (agreement to 4e-8 relative, unit-pinned) and, at exactly 1.0, the
dual-fisheye frame (the fold-boundary defect that under-counted a dual frame by
the scene's direct-through energy — 18.9% measured on the reference scene — is
fixed; see `doc/geometry/coordinate-convention.md` §13 for the mechanism and the boundary
semantics).

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

The raypath module under `src/raypath/` exposes exactly four public headers to the engine:

- `src/raypath/path_feature_report.hpp` — the feature-report entry point, narrowed to
  `PathFeatureReportRequest` + its constants + `AnalyzePathFeatureReport(config, request,
  product_version, std::string* json_out)`: the module runs the analysis, assembles the report and
  serializes it internally, so the caller never compiles the report types.
- `src/raypath/survey_report.hpp` — the survey entry point on the same shape:
  `SurveyRequest` + its constants + `AnalyzePathSurvey(config, request, product_version,
  std::string* json_out)` (v4.54; §9).
- `src/raypath/single_path_analysis.hpp` — `AnalyzeSinglePath` over the analytic kernel.
- `src/raypath/single_path_json.hpp` — `ToJson` / `ParseWarmSeeds` (`kSunGridSignificantDigits`
  included, as a published constant).

The bridge (`src/server/c_api_raypath.cpp`) compiles exactly these four (plus its own
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

The boundary is cheap to re-verify mechanically — the include closure of the four public
headers must contain no `raypath/detail` file and no `analytic/diagnostic_batch`:

```bash
printf '#include "raypath/path_feature_report.hpp"
#include "raypath/survey_report.hpp"
#include "raypath/single_path_analysis.hpp"
#include "raypath/single_path_json.hpp"
' | g++ -H -fsyntax-only -x c++ - -std=c++17 -I src 2>&1 | grep -cE "analytic/diagnostic_batch|feature_discovery|raypath/detail"
```

`0` is the expected answer. This is the probe that pinned the boundary when the bridge surface
was narrowed (its baseline counted 6 hits — four internal headers plus the analytic kernel
surface — before the narrowing landed).

## 9. `Lumice raypath survey` output (schema 4)

```bash
Lumice raypath survey -f <config> --crystal <id> --path <faces> \
  [--literal] [--lambda 550] [--budget 15s] [--mc-ess draws] [-o out.json]
```

The survey is the census face of the diagnostic pipeline (v4.54): **one single-layer raypath
class of one crystal entry, routed at the algebra layer (T0)**. The request's path is expanded
under its scope — the physical P|B|D class by default, the literal sequence with `--literal` —
and every member of the class is classified by the analytic kernel
(`LUMICE_ANALYTIC_ClassifyCriticalStructure`'s implementation, whose rank-0 arm is the identity
certificate: the fold matrix M = I to 1e-12 per entry and the entry/exit wedge zero to 1e-9°).
The routing has three outcomes and nothing else:

- **Every member carries the certificate** → the class collapses to exactly one `source_image`
  object at the sun (the sky-side object; the members are its preimage decomposition), and the
  configurational analysis **stops there**: no delta axis, no fiber, no field work, no MC. The
  chromatic layer's weight leg runs anyway (a source image can still tint — two parallel
  internal reflections square a Fresnel coefficient that grows toward TIR at blue, and the
  corpus's C15 sun disk reads blue for exactly that reason): the document carries a real
  `chromatic.weight_tint` (§9.6) — `finite` with the ratio when the axis maps to a pose
  density, a `density_not_expressible` refusal when it does not (a fixed pose is an honest
  capability boundary, not a physical impossibility). The position legs (`positional_dispersion`,
  `diagnose`) stay absent: a rank-0 path has no positional chromatic structure to follow.
- **A random-orientation class whose axis is expressible as a pose density** (the
  full-dimensional route: the T1 rank is taken on the family, and here the family is the whole
  rotation group) → the delta machine runs per member: the class-level `support_set`, the
  `caustic_edge` (the minimum-deviation fold), the `bright_domain` skeleton, the solar-radial
  `probes[]` slice, and the chromatic layer's dispersion and diagnose legs (§9.6). The deeper
  kinds `family_collapse_point` / `regular_image` are structurally absent on this route — they
  are the rank-0/rank-1 family outcomes, not uncomputed work.
- **A family-restricted measure** (a non-random, non-degenerate axis) → the measure-to-family
  routing (`family_routing`, §9.7): the measure is read in the concentration language
  (each dimension `{fixed | gauss | uniform}` × (center, width), the two default semantics
  being the loader's own), and the freezing ladder runs — level 0 is the actual support
  (`support_set`, always), levels 1..K freeze the k tightest gauss candidates and ask the
  restricted-family rank there (a collapse point, a fold arc, a regular image or a bright
  domain per member, each carrying a `band` label), and the per-object survival filter decides
  whether a frozen width's image blur stays inside the object's own angular scale (survivors
  become cards; the killed are reported inside the ladder level that proposed them). The
  all-fixed axis is the 0D arm: the image is evaluated at the one pose directly and the weight
  read the same way (never a band sum).
- **A measure shape outside the routing v1 domain** (a concentrated azimuth, a uniform zenith,
  a bounded uniform's hard support, a distribution type the vocabulary does not read) → one
  refused `support_set` card with the open mode `measure_shape_outside_routing_domain:
  <named shape>` and the witness sub-block's positive fact. The domain is widened by design
  decision, never by approximation (a bounded uniform is not silently swapped for a
  matching-width gauss). The retired v1 skeleton — the deeper kinds parked in `unevaluated`
  with a not-wired reason — is gone: a non-random axis is routed or refused, never parked.

Member expansion is a statement about the **ensemble**, not about the sequence: the physical
class of a plate's `1-2` is `{1-2}` alone, because the plate ensemble (c axis at the zenith,
±wobble) is not invariant under the horizontal mirror that maps `1-2` to `2-1`. A
zenith-symmetric family expands differently. `scope.members` records the expansion the routing
actually ran on.

Options: `--crystal`/`--path` as for the target mode (`--path` here refuses multi-layer chains
through the same engine error); `--literal` narrows the scope to the literal sequence;
`--lambda <nm>` ([350, 900]) selects the routing reference wavelength, defaulting like the
target mode's `--wavelength` (the scene's single discrete wavelength, else 550; the choice is
recorded in `scope.wavelength_source`) — it is named `--lambda` because it is a routing
reference, not a spectral integral; `--budget <N>[s]` (integer seconds, the `s` optional,
default 15, max 120) rides the document's budget block; `--mc-ess <draws>` (positive integer,
max 200000) requests the opt-in MC region ESS leg (§9.8). Exit status and stderr progress follow
the target mode's contract (`[raypath survey]` start/done lines; Ctrl-C ends the process and
writes nothing; `-o` is atomic, and the analysis runs after the output directory is checked).

### 9.1 Top-level keys

| Key | Meaning |
|---|---|
| `schema`, `schema_version` | `lumice.path-survey`, 4 — a new document family; it shares no fields with the schema-1/3 documents, and a reader of one must reject the others |
| `generator` | Lumice version and the analytic kernel's `analytic_api_version` |
| `request` | The echo: `crystal_id`, `path_layers`, `symmetry_bits` (7 = physical P\|B\|D, 0 = literal), `wavelength_nm` (null when unset) and `budget_ms` as resolved |
| `scope` | `scene_identity`, `scene_layer` (null for a standalone configured crystal), `members` (the expanded class), `wavelength_nm` + `wavelength_source` + `refractive_index`, `sun_altitude_deg` / `sun_azimuth_deg` |
| `routing` | The T0 leg: `early_stopped`, and per-member `members[]` records (`member`, `rank_zero`, `mechanism` — the kernel's roll-up slug, null when not computable, with `mechanism_unavailable` naming why — and the member's own `evidence` block) |
| `objects` | The object cards (§9.2): one `source_image` on an early stop; on the full-dimensional route the `support_set` (always), the `caustic_edge` (when the class has an extremum fold — the position needs a fold to sit on; the minimum-deviation fold when the class has one, else the interior maximum at the support's upper edge), and the `bright_domain` skeleton (the corpus shapes produce all three); one refused `support_set` when that route fails closed — and on that refused card, the declared-source `witness` sub-block (§9.2); the family route's own cards (the band-labelled survivors and the level-0 `support_set`, §9.2's family-form paragraph) |
| `family_routing` | present only on the family arm's documents (§9.7): the measure echo, the freezing ladder with every level's rank coverage and killed candidates, and the block's own evidence. An early stop, a full-dimensional route and an outside-domain refusal carry no `family_routing` key at all |
| `unevaluated` | `{kinds, reason}` entries naming what the routing did not compute and why; empty on an early stop, on the full-dimensional route and on the family routes (the v1 not-wired skeleton is retired — a non-random axis is routed or refused, never parked) |
| `probes` | The canonical probes; present only on a route that ran one (the full-dimensional route, and the family route when the density conversion carries the measure — §9.6) — an early-stop, an outside-domain-refused and a density-less family document carry no `probes` key at all |
| `tir_share` | The TIR share leg (§9.8): present only on a route that ran it (the full-dimensional route and the refused-support route) — an early-stop and a family-route document carry no key at all; a path with no internal interface carries the key with the `no_internal_interfaces` refusal (the leg ran, its question is meaningless for that shape) |
| `mc_region_ess` | The opt-in MC region ESS leg (§9.8): present only when the caller requested it AND the route is one that runs it (the full-dimensional and refused-support routes) — the flag on any other route produces no key |
| `chromatic` | The chromatic layer's own evidence block plus the optional sub-blocks (`positional_dispersion`, `diagnose`, `weight_tint`, §9.6). Its state follows the legs its routing arm ran: `unevaluated` only when no chromatic leg exists for the input class, `finite` when its legs computed, `refused` (with the leg's mode) when its only leg refused — on the identity arm the tint IS the block's only leg, so the block and the sub-block never disagree |
| `evidence` | The document-level state: the T0 routing verdict's own block (§9.3) |
| `warm_index` | The `--warm` contract's survey side (always written; §9.9) |
| `budget` | `requested_ms`, `enforced` (false) and the honesty note (the note names the legs the producing arm ran) |
| `timing` | `total_seconds` — the wall clock of the analysis **up to the end of assembly, excluding serialization**; each routing leg's cost rolls into this one figure |

### 9.2 Object kinds (the registry)

Six kinds are registered, append-only (a new kind is a new name in the registry, never a
re-spelling):

| Kind | Routing origin |
|---|---|
| `source_image` | T0 identity certificate (M = I and zero wedge): the sun's preimage |
| `family_collapse_point` | T1 rank 0 on the limit family |
| `regular_image` | T1 rank 1: a one-dimensional regular image |
| `caustic_edge` | T1 rank 2: the critical image of the relevant projection |
| `bright_domain` | a two-dimensional bright domain (weight profiles) |
| `support_set` | the support set: boundary curves with mechanism labels, holes |

The negative assertion over an early stop is mechanical: `objects[].kind` must equal exactly
`{"source_image"}`. A v1 `source_image` card carries `members` (the class), `position`
(`altitude_deg` / `azimuth_deg` = the scene sun), its certified `evidence` block and a
`certificate` declaring the kernel's own tolerances (`fold_tolerance` 1e-12,
`wedge_tolerance_deg` 1e-9) — the certificate itself lives in the kernel; the card restates the
ruler, it does not re-measure.

On the full-dimensional route each card carries its kind's payload (a payload key never appears
on a card of another kind):

**`support_set`** — the class's delta-axis support:

| Field | Meaning |
|---|---|
| `intervals[]` | `{lower_deg, upper_deg, n_components, n_closed, n_open}` — the partition's intervals (the kernel's own counts: a closed interval carries a closed level set, an open one carries that many open arcs) |
| `endpoints[]` | `{delta_deg, source, profile, multiplicity}` — the kernel onsets sitting at an interval endpoint, slugs passed through (`interior_minimum` / `boundary_extremum` / `corner` × `finite_jump` / ...) |
| `symmetry_bits` | the scope the partition ran on (7 = physical P\|B\|D, 0 = literal) |
| `member_count` | the class members the partition ran on |
| `density_tails` | present only when the density leg ran and the zenith measure is a gauss — the zenith window (the band sum's ±12σ floor) as `{dim, lower_deg, upper_deg}`; the width went into the ladder, and the support's own tail bound is a row here |

**`caustic_edge`** — the support's extremum fold (the sharp inner edge; the corpus's 22-degree
inner edge on a random prism): the minimum-deviation fold when the class has one, else the
interior maximum where the support's upper edge ends (the corpus's C09 shape — a support that
ends at a finite-jump edge and continues into nothing):

| Field | Meaning |
|---|---|
| `position_deg` | the fold's deviation at the reference index |
| `profile` | the kernel's onset-profile slug (`finite_jump` on the C01 and C09 shapes) |
| `measure_limit` | present when the kernel computed it (2π/√det H, the finite jump's level-set limit) |
| `coincident_with_support` | the fold sits on the support boundary — the end the fold actually occupies, lower for a minimum and upper for a maximum (within the partition's own merge constant); the marker that predicts a sharp edge rather than a soft cutoff |
| `certificate` | present when the position is certified: `formula` (`2*asin(n*sin(A/2)) - A`), `apex_angle_deg`, `cross_check_deg` (the kernel position vs the survey's restatement), `tolerance_deg` (1e-6 rad; a mismatch beyond it refuses as `closed_form_mismatch`, it never picks one reading silently). Only the two-face-chain minimum certifies — a maximum fold (and any longer chain) lands `finite` with no certificate claimed |

**`bright_domain`** — the support's interior span and the profile readings:

| Field | Meaning |
|---|---|
| `domain_lower_deg` / `domain_upper_deg` | the support's overall span |
| `peak` | `{delta_deg, value}` — the probe profile's argmax; a **finite** reading (the sharp edge lives on the caustic card, certified; the brightness peak lives here, finite — different quantities, different states) |
| `edge_reading_deg` | the probe's own edge reading (first non-empty bin, ± half a bin) |
| `kept_count` | the sample's kept events — the T2 **existence witness**: > 0 says the class carries finite positive weight somewhere; it is an existence statement, never a distribution statistic |
| `probe_index` | the `probes[]` entry the readings came from |

**`witness`** — the refused-support card's positive leg (it never appears on a card the
refused-support arm did not produce). A partition refusal says the support is unknown, not
that the class carries no light: the corpus's 3157 refuses every member's partition
(`slab_crease_touching`) while its declared-source events carry positive weight. The witness
sub-block hangs that positive fact on the SAME refused card — the card's own `evidence` stays
`refused`, the sub-block carries its own state, and the two coexist without either overriding
the other:

| Field | Meaning |
|---|---|
| `state` | `finite` (the witness ran and confirmed) or `refused` with `mode` `no_declared_source_witness` (the declared source offered no positive event, or the batch re-evaluation did not confirm the chain — fail closed, never a defaulted number) |
| `coverage` | the finite state's declaration (the batch re-evaluation's basis and wavelength) |
| `kept_count` / `sample_count` | kept events over the lattice size — an existence count, never a distribution statistic |
| `lattice`, `deterministic` | the sampler's own self-description (the antipodal Fibonacci lattice; no RNG anywhere) |
| `deviation_deg`, `weight` | the max-weight event's deviation (the discovery's own D convention) and its `w = A·T` |
| `entry`, `interface_product` | the diagnostic batch's own re-evaluation at that pose — a second, independent read of the forward chain |
| `source_note` | the declared source and the reading protocol, spelled inside the document |

The witness is the lattice argmax event of the declared measure's own sampler (the antipodal
Fibonacci lattice over SO(3)) — an **existence** statement. The corpus's target-specific
numbers are a different protocol and live on a different ruler.

**The family-form cards (the `family_routing` arm)** carry the same registry kinds with their
own payload shape. A family card carries `position` (the representative's sky position), its
`evidence`, the `band` label (below) and `family_origin` (`sample_count` / `crossing_count` —
per-kind coverage counts: a `family_collapse_point` carries its collapse component's sample
count, a family `caustic_edge` its fold component's crossing count; a `regular_image` /
`bright_domain` card's count comes from the consumed-side extent probe (the assembly's own
`EvaluateFamilyPoint` probes), not the rank primitive's output). The one key that crosses forms
is `bright_domain`: a family bright_domain card carries the class support's span
(`domain_lower_deg` / `domain_upper_deg`), and its profile readings — when the density carried
the measure — come from the probe leg exactly as on the full-dimensional route, on the
representative member's card only (`members` exactly the requested literal sequence; sibling
member cards carry the span alone); a family caustic_edge card carries no `caustic_edge`
payload (the fold arc's position is its representative crossing's sky position).

| Field | Meaning |
|---|---|
| `band.level` | the ladder level the card survived at (0 = the actual support's level: the measure's own fixed dimensions, or the 0D arm's single pose) |
| `band.frozen[]` | `{dim, verdict}` per frozen dimension — `narrow`/`wide` is a property of the (dimension, object) pair, never of the dimension alone |
| `band.blur_deg` / `band.scale_deg` | the survival filter's own two numbers: the frozen widths' image blur (the anchor images re-evaluated at ±width/2, clamped one-sided at a pole with `clamped: true`) against the object's declared scale (every curve-like object — a collapse point, a fold arc, a one-dimensional image track — declares the 1.0° structural cross-section floor, never its overall extent; only the two-dimensional bright domain declares its ≤16-point probe set's extent); shifted probes that find no valid image are skipped as data, and a dimension whose shifted probes all come back empty reads narrow (v1: no image carries no blur evidence) |
| `band.survival` | `"pass"` — a killed candidate never carries a card; it lives in `family_routing.ladder[].killed` with the same numbers |

### 9.3 Evidence states

Every declaration block (each object card, each member verdict, the document level, the
chromatic layer) carries a **required** `state` from the closed four-value vocabulary — a state
is always serialized, so absence can never read as "settled":

| State | Meaning | Accompanying field |
|---|---|---|
| `certified` | closed-form / algebraic / exhaustive proof | `method` — from the certified-method registry (`point_mass`, the kernel's rank-0 slug; `slab_min_deviation_closed_form`, the two-face-chain minimum-deviation edge) |
| `finite` | a finite computation | `coverage` — `{members_evaluated, geometry_basis, wavelength_nm}` |
| `refused` | a refusal | `mode` — an OPEN string (the analytic-api precedent: status closed / reason open), e.g. a boundary-walk status name |
| `unevaluated` | not computed | `reason` — an open string naming what did not run and why |

The schema default is `finite` with its coverage declaration; the default rule is pinned by the
in-repo unit tests. A `refused` state is data about the computation, never a physical
conclusion; an `unevaluated` block is a routing state, never an empty result.

### 9.4 Budget honesty

`--budget` is parsed, validated (an out-of-range value is rejected, not clamped), echoed and
**not enforced**: `budget.enforced` is false and the note says what the producing arm actually
ran — the full-dimensional arm's legs are the per-member delta partition, the band-sum probe,
the chromatic pair and the class tint's two-lambda power sum (seconds of work on a 12-member
class); the identity arm's are the millisecond routing verdict and the two-lambda band-sum tint
(the note names the per-lambda sample count); the refused-support arm's are the routing
verdict, the per-member partition attempt (the note names the refusal) and the witness lattice;
the family arm's are the routing verdict, the per-member delta partition, the ladder's
per-member per-level rank scans (the note names the grid) and — when the density conversion
carries the measure — the band-sum probe, the chromatic pair and the class tint (the 0D arm's
are the partition, the single-pose image evaluation and the single-pose weight).
Every arm's note names its own legs. Enforcement arrives with the quantification legs that
actually spend.

### 9.5 Version discipline

Fields are only ever appended; `schema_version` changes meaning only when a field's meaning
changes or is removed, and a reader written for one schema family (1, 3, 4) must reject the
others. The append discipline has a byte-stable face: the early-stop documents of a pre-T1
build and this build are identical apart from `timing` — a new key appears only when the leg
that fills it ran. The C API surface is `LUMICE_AnalyzePathSurvey` /
`LUMICE_PathSurveyToJson` / `LUMICE_PathSurveyDestroy` over an opaque immutable handle
(v4.54, `lumice_raypath.h`); the request struct grew one tail field after v4.54 —
`mc_ess_draws`, read only when the caller's `struct_size` covers it, so a v4.54-shaped caller
(the GUI) compiles and behaves unchanged — and the CLI and the GUI read the same serialized
document — there is no second serializer.

### 9.6 The probe and the chromatic caliber

The full-dimensional route's quantification layer (the family route runs the same legs when its measure's density conversion carries it — the probe's radial profile reads the family density, the readings landing on the representative member's own `bright_domain` card) runs at fixed constants — they are part of
the schema (the serialized `spec` echoes every one), not tuning knobs:

| Constant | Value | Meaning |
|---|---|---|
| probe domain | [0°, 180°] deviation from the sun — the whole delta axis | the radial range the slice covers; the corpus's C06/C09 supports sit outside the original [15°, 60°] slice, and a support-derived window would make the bin layout a per-class branch — one ruler for every class |
| probe bin | 0.1° (1800 samples) | one narrow azimuthal wedge per bin; under the uniform density the band-sum value is the full annulus's per-sr mean, so the wedge's azimuth carries no information |
| probe wedge half-width | 1.0° (`spec.wedge_half_width_deg`) | the wedge's azimuthal extent — part of the probe's caliber, so a reader can reconstruct the sliced solid angle |
| probe sample count | 2,000,000 | the band-sum lattice size (`n`); the sampler is the antipodal Fibonacci lattice — **deterministic, no seed** (the coverage declaration names lattice + count + determinism, never a nonexistent seed value) |
| probe reference λ/n | the document's `scope.wavelength_nm` / `refractive_index` | the probe shares the routing's reference ruler, so its edge reading, the support's lower end and the certified edge position are directly comparable |
| probe representative | the request's face sequence (`spec.representative`) standing for `spec.member_count` class members | the members are relabelings of one physical class; the class-level reading is the representative's ×N |
| chromatic λ pair | 650 nm / 450 nm | the pool's red and blue ends through the engine's dispersion table; `n_red`/`n_blue` are echoed verbatim; the D65 full-spectrum spatial integral is a later slice's work |
| tint sample count | 1,000,000 per lambda (`weight_tint.sample_count`, both arms) | the tint leg's lattice size, the same deterministic antipodal Fibonacci; the identity arm's point-mass branch and the T1 arm's class power-sum share the constant (each arm's own ladder pinned it at the same value); a new key appears only in a tint block that ran |

The probe's two readings stay two things: `edge` (first non-empty bin — geometrically no event
lies below the minimum-deviation edge, so that bin contains it; a finite reading good to half a
bin) and `peak` (the value argmax — the profile's brightest sample). The certified edge
position lives on the `caustic_edge` card; on the corpus's C01 shape the profile rises to its
peak within one bin of the edge and falls monotonically outward (the brightness approaches a
finite limit at the edge, then decays — the −const·√(δ−δ_min) near-edge term averaged inside
the first bin makes the argmax land on its doorstep), so `separation_deg` is a doorstep
quantity, not a displacement. **A probe with an empty profile serializes neither reading and no
`separation_deg`**: a zero there would read as "the peak sits at the edge", a claim about a
profile that has neither. `edge` / `peak` / `separation_deg` keys exist exactly when the
readings they describe do.

The chromatic block on this route is `finite` with up to three optional sub-blocks, and the
two mechanisms never merge fields. The block's state rule across all arms: **finite once any
leg produced output** — a refused leg with other real output keeps its refusal in its own
sub-block (the refused-support arm is the degenerate case: no leg runs, the block stays
`unevaluated` and names the axis refusal that stopped them).

- `positional_dispersion` — the critical onsets followed across the pair. `followed_onsets[]`
  is every row the kernel's wavelength-critical table carried, in table order, each
  `{source, profile, values_deg[2], displacement_deg, direction}` with the kernel's slugs
  passed through and the row's own stacking verdict (`red_inner_blue_outer` ⇔ values_deg[0] <
  values_deg[1]: the red edge sits closer to the sun). The min-deviation row's own scalars
  (`value_red_deg`/`value_blue_deg`/`displacement_deg`/`direction`) serialize **only when that
  row exists** — a corner-bounded support (C06) or one ending at an interior maximum (C09) has
  no minimum, and the followed rows are the leg's data instead of a refusal: keys exist exactly
  when the readings they describe do. Either way the block carries `red_label`/`blue_label`,
  `n_red`/`n_blue` and its own `evidence`.
- `diagnose` — the kernel's weight-kink and gate-edge analysis as a **separate sub-block**
  (a different kernel path than the dispersion table): `verdict_kind`, `color`, `visible`,
  `position_deg`, `coverage_complete`, `notes`, the declared `n_red`/`n_blue`, `features[]`
  (`kind`/`source`/`color`/`visible`/`delta_red_deg`/`delta_blue_deg`/`shift_deg`) and its own
  `evidence`. A colour edge can be positional (dispersion) or weight-driven (a gate or kink);
  conflating them would read one mechanism off the other's ruler.
- `weight_tint` — the transmission-weight ratio across the pair. Both arms compute it, as two
  different readings with two field families that never merge (see below): the identity arm's
  point-mass masses, and the full-dimensional route's class power-sum over the declared
  lattice.

**`weight_tint` (both arms, two readings).** The question is the same — how the transmission
weight tilts across the fixed pair — and the reading follows the arm's routing:

- **The identity arm (a rank-0 path).** One sky position, so the chromatic question is how
  bright per lambda: the leg runs the point-mass branch of the band sum twice — once per lambda
  of the fixed pair, through each lambda's refractive index — on the class representative
  (standing for `member_count` members, the probe's relabeled-equivalence discipline), with an
  empty pixel table (the ratio reads the masses, not a rendered map). The two calls share the
  lattice size, so the ratio's quadrature error cancels to first order; under a non-uniform
  pose density both masses come from the kernel's deterministic psi-average
  (`method: "psi_average"`), under the random family from lattice means (`"lattice_mean"`).
  Fields: `red_label`/`blue_label`, `n_red`/`n_blue` (the dispersion table's pair, echoed),
  `m_red`/`m_blue` with `m_red_error`/`m_blue_error` (the masses and their last-doubling
  quadrature changes — provenance, not a jitter bar to assert against), `method`,
  `sample_count`, `lattice`/`deterministic` (the sampler's own self-description),
  `representative`, `member_count`, `ratio` (= m_blue/m_red), `direction`,
  `kept_count_red`/`kept_count_blue` (the existence witnesses), `caliber` (the ratio's
  protocol, spelled inside the document), `evidence`.
- **The full-dimensional route (the class power-sum).** The class is a measure, not a point:
  the leg samples the declared source itself — two `BuildEvents` passes over one shared
  antipodal Fibonacci lattice, once per lambda through its refractive index — and the ratio is
  the blue weight sum over the red one, carried by the per-sample average powers
  (`energy_red`/`energy_blue`: the kept events' `w = A·T` summed over the lattice, divided by
  the lattice size). `method: "lattice_power_sum"`; the `m_*` family stays absent (a mass is
  not this reading's quantity), and the two readings' calibers must never be compared — a
  point-mass mass ratio at the sun direction and a whole-class power sum over the declared
  measure are different protocols that agree in neither magnitude nor threshold (the corpus's
  C15 and C06 numbers differ by an order of magnitude under the two calibers). The shared
  fields (`ratio`, `direction`, `kept_count_*`, `sample_count`, `lattice`/`deterministic`,
  `representative`, `member_count`, `caliber`, `evidence`) mean the same thing on both shapes;
  the `caliber` string names which reading a document carries.

**The ratio's caliber is not a colour ratio.** `ratio` compares transmission weights at two
narrow bands and nothing else — no spectral power distribution, no colour-matching functions,
no solar disk. It must never be read as, or compared against, a disk-integrated D65 B/R: the
two agree only in sign and rough magnitude, because the D65 integral weights the whole spectrum
while the pair isolates the Fresnel mechanism. The document carries the `caliber` string so a
reader never has to know this from the outside.

`direction` is the sign of the ratio itself, from a closed three-slug vocabulary: `blue_tinted`
(ratio > 1), `red_tinted` (ratio < 1), `neutral` (the exact-1.0 degenerate). No neutral band
lives in the model — how far from 1 counts as "really tinted" is an acceptance notion, and each
consumer pins its own threshold.

The leg refuses rather than approximates. On the identity arm the tint is the block's only leg,
so the refusal appears in both the sub-block and the chromatic block (they mirror);
`density_not_expressible` (the axis maps to no pose density — a fixed pose is the degenerate
case: its weight is exactly computable in principle, what is missing is a delta-distribution
pose encoding, a kernel capability boundary, not a physical impossibility), `not_rank_zero`
(the caller's certificate and the kernel's own rank check disagree — never read a map as a
mass), `zero_band_weight` (a band's mass degenerated; a silent division by zero must not reach
the document). `member_resolution` is the defensive mode both arms share (the representative's
face sequence stopped resolving to slots — unreachable past the request-level resolution, and
honest if it ever fires). On the full-dimensional route the block has other legs with real
output, so a refused tint keeps its refusal in the sub-block alone: the block-state rule is
finite once any leg produced output.

### 9.7 The family routing block (`family_routing`)

Present only on the family arm's documents. The measure echo is what the router read off each
orientation dimension — the concentration language, never the distribution-type language, in
routing coordinates (zenith = 90 − latitude):

| Field | Meaning |
|---|---|
| `measure.<dim>.kind` | `"fixed"` \| `"gauss"` \| `"uniform"` per dimension (`zenith` / `azimuth` / `roll`) |
| `measure.<dim>.center_deg` / `width_deg` | the fixed value / gauss mean and sigma; a uniform echoes the loader's own midpoint and full range (display only — a uniform is never a freeze candidate). Centers are echoed raw, without the clamp/fold the frozen widths carry in `frozen_dims[]` |

The ladder, one record per level (level 0 = the actual support; levels 1..K freeze the k
tightest gauss candidates tightest-first, ties zenith before roll; azimuth and uniforms never
freeze — the rank entry's own refusal domain):

| Field | Meaning |
|---|---|
| `ladder[].level` / `frozen_dims[]` | the level number; `{dim, center_deg, width_deg}` per frozen dimension — the freeze value and the frozen width reported together (the measure's fixed dimensions carry width 0) |
| `ladder[].free_dims[]` | the family's free dimensions at this level |
| `ladder[].rank` | present exactly when the rank question was asked (a level-0 with three free dimensions owns the support alone): `{generic_rank, grid, grid_spacing_deg[], samples, valid_samples, refused_members?}` — the first member's coverage whose rank run returned ok; relabelings share the rank structure; `refused_members` is present only when non-zero and counts the members the rank entry refused on their weight leg (data, not a verdict) |
| `ladder[].objects_proposed` / `objects_killed` | the level's candidate counts — every level reports, not just the deepest |
| `ladder[].killed[]` | the killed candidates: `{member, kind, position?, blur_deg?, scale_deg?, clamped?, reason, verdicts[]}` — the same numbers the surviving card's band would carry; the blur/scale/clamped group is present exactly when one of them was measured, and a `no_valid_image` kill leaves the pair unmeasured and omits it; `reason` is `survival_filter` (the blur exceeded the scale) or `no_valid_image` (the probe set found no valid image) |
| `level0_refusal_mode` | present only when the level-0 delta partition refused: the refusal mode the refused support card carries in its `evidence.mode`, echoed so a block-level reader need not walk `objects[]` |

The block's own `evidence` is finite once any of its legs produced output (the same rule the
chromatic block states). A level-0 delta partition that refuses scopes to its own card — one
refused `support_set` card carrying the witness sub-block — while the ladder enumerates
regardless: the partition is a geometry fact about the support, the ladder a measure fact, and
the only deep-level partition read is `bright_domain`'s domain bounds (present only when the
partition succeeded, never fabricated from a refused one). The refusal is echoed at block
level as `level0_refusal_mode` (the same string the refused card carries in `evidence.mode`).

Block state and document state are two independent levels: the block's `evidence` speaks for
its legs (the ladder's scans, the chromatic legs), the document's `evidence` for the class
verdict. The document refuses only when no finite structural object exists — a refused
partition with a finite collapse card on record stays a finite document; conversely a finite
block does not imply a finite document (the refused card is a refusal record, not a structural
object, and the witness is a payload with its own state that never counts). The one
block-level non-finite state on this route: a refused level-0 partition AND a density the
conversion cannot carry — every leg silent, and the block says `unevaluated` with the reason
instead of claiming a reading nothing produced. The block is write-only: survey documents are
serialized one way, and no in-tree reader deserializes them back into `SurveyDocument`.

### 9.8 The exception evidence legs (`tir_share` / `mc_region_ess`)

Two document-level optional blocks sit beside `family_routing`, each carrying its own evidence
state under the design's rule for exception legs: a probe is a **named finite evidence leg with
a coverage declaration** (N / resolution / seed / caliber), never a value of the main
four-state vocabulary and never a word of the MC annotation vocabulary
(`agrees`/`silent`/`conflicts`/`not-run` — those compare the MC evidence to the structure;
these legs measure the sampling itself). A key exists exactly when its leg ran; an arm that
does not carry a leg serializes no key at all, which reads as "this arm does not run the leg",
never as a refusal. A refusal serializes exactly `{state, mode}` — no defaulted number reaches
the document.

**`tir_share`** — the class tint's per-sample average power decomposed by internal-reflection
mechanism. The question: of the kept events' transmission weight (`w = A·T` over the declared
antipodal Fibonacci lattice, the class power-sum's own unit), what share sits on poses where
**every** internal interface of the chain is total-internal — the C15 near-TIR weight
mechanism read as a class-level estimate (a larger refractive index lowers the TIR threshold
and widens the window, so the share grows toward blue; the corpus's C15 anchor — 51.1% at
450 nm / 38.6% at 650 nm of the positive-A sample energy from twice-TIR poses — is the
actual-distribution caliber, a DIFFERENT protocol from this leg's declared-lattice × class
representative caliber: a direction reference only, never a magnitude assertion, and the `caliber`
string inside the block says so). Scope: the full-dimensional route and the refused-support
route (the partition's refusal says nothing about the internal-reflection weight split, which
the declared lattice still answers); the identity early-stop and the family routes do not run
the leg. Per lambda the leg runs one lattice pass at the class tint's own size (1,000,000 —
the two power-sum legs read one shared lattice), re-poses every kept event and re-derives the
chain through the diagnostic batch in one call, then classifies by the per-interface
discriminant sign. The shares read the discovery-side weights on BOTH sides (numerator and
denominator the same reader); the batch is only the classifier, and its entry /
interface_product readings never enter a share (the witness leg reports those under its own
semantics — a share is an estimator, not a witness).

Fields: `n_red`/`n_blue`, `interfaces[]` (`{face, share_red, share_blue}` per internal
interface — the face numbers of the representative's own chain), `share_all_tir_red` /
`share_all_tir_blue`, `sample_count`, `lattice`, `deterministic: true`,
`representative`, `member_count`, `kept_count_red`/`kept_count_blue`, `caliber`, `evidence`.
The output is a [0,1] share per lambda plus the per-interface decomposition — **never a
cross-lambda ratio** (the tint's `ratio` is a different quantity on a different field family;
the two calibers must not be compared, and neither may be read as a colour ratio). On a
one-internal-interface path (3-5-7) the aggregate IS the interface row, exactly. Kept events
whose candidate-pose chart degenerates at exactly-zero deviation (the pose's azimuthal freedom
is unconstrained there, so no TIR classification exists) sit outside the estimator's declared
support, excluded from both numerator and denominator; the caliber spells this. `kept_count_*`
stays the lattice's kept-event total — the excluded chart-degenerate subset is not subtracted
from it. Refusals:
`no_internal_interfaces` (a two-face chain — entry and exit only — leaves the leg no question
to ask), `member_resolution`, `zero_band_weight`, `batch_read_unavailable` (a validated pose
the batch still cannot trace: the two readers disagree, which is a defect to investigate, not
a row to drop).

**`mc_region_ess`** — the opt-in MC corroboration sampling's effective sample size per
solar-radial delta band, for THIS class. Requested with `--mc-ess <draws>` (or the C API's
`mc_ess_draws`); without the flag the key never appears. The mechanical chain is entirely the
schema-3 side's as-built machinery under the survey's own scope: the survey's input snapshot
feeds a `DiagnosticSampler` at a **fixed seed** (20261022, echoed — prefix-replay
reproducible: the same seed and request produce identical bands) and
`BuildDiagnosticMeasure` fills the measure under the budget deadline; the leg then buckets the
Y-channel weights by deviation from the sun into the fixed 5°×36 grid over [0°, 180°] and
reports each band's Kish effective sample size over outer-draw groups (one draw is one
sampling act even when it contributed several member rows — the schema-3 `PresenceEss`
grouping discipline, not a second ratio). Fields: `seed`, `draws`, `lambda_nm` (the survey's
reference lambda — the leg is single-lambda), `band_deg`, `bands[]` (`{delta_min_deg,
delta_max_deg, ess, draw_count, sum_weight}` — an empty band serializes its honest zero),
`deterministic: false` (seeded random; the field exists to state the leg's own shape against
the deterministic legs' true — a deterministic quadrature carries no ESS semantics, and the
witness lattice and the power-sum legs never carry an ESS field), `evidence`. The leg is a
**sampling-quality measurement, not a corroboration verdict**: no agrees/silent/conflicts
word applies, and the MC annotation vocabulary stays on the schema-3 side — the two live
side-by-side in the product but never in each other's fields. Refusals:
`sampler_unavailable` (the single-lambda spectrum cannot be assembled from the scene's light —
a reference lambda outside the continuous band, or a discrete light with no slot at the
reference wavelength), `budget_exhausted` (the deadline hit before the requested draw count
completed; a partial measure's band coverage would vary with machine speed, breaking the
seeded reproducibility the coverage declares).

The schema-4 field list for both legs is append-only like everything else: the 357 corpus
assertion bench pins their first corpus appearance — the 357 shape's
`slab_crease_not_carried` refusal with the finite witness, the finite `tir_share` with the
k=1 identity and the direction `share(450) > share(650)`, and the flagged run's finite
`mc_region_ess` with the Kish hard bounds (`0 < ess <= draws`).

### 9.9 The `warm_index` block (the `--warm` contract's survey side)

Always written — every document declares its seeds (possibly empty), its receipts and its
fingerprint. A reader treats a missing or incomplete block on the wire as the documented
`warm_index_absent` degradation (§5), never as refusal material.

| Field | Meaning |
|---|---|
| `seeds[]` | The poses the assembly actually held, 9 row-major doubles each; `[]` when no arm held one (the identity and full-dimensional arms hold none today) |
| `seed_sources[]` | Parallel to `seeds`: the leg that held the pose — `witness_argmax` (the refused-support card's lattice argmax), `family_collapse_anchor`, `family_fold_anchor`, `single_pose_nominal` (the 0D arm's production Euler pose). Killed candidates never contribute |
| `reuse_scope` | The declared reuse domain: structural facts (routing, object cards, support) reuse across sun and wavelength changes; probe band-sum readings are valid only at the producing document's wavelength and sun — the consuming query compares the `scope` receipt per read |
| `produced_for` | The producer receipt: schema family + the analytic kernel's API version |
| `config_fingerprint` | The routing-axis projection's SHA256 (§5, gate 3): the axis set is the crystal entry whole (shape + axis), the literal path layers, and the symmetry bits — sun, wavelength/spectrum and scene allocation excluded; one implementation serves the writer and the query's reader, and a config field that starts to affect routing extends the projection and this list in the same change |

**The in-tree reader's footprint, named block by block.** The schema-4 document has exactly one
in-tree consumer, the point query's warm reader — and it does NOT deserialize the document: it
reads `warm_index` (all five fields), the `scope` receipt's wavelength/sun/source (the band-sum
gate's comparison side), the `objects[]` ownership facts (each support card's intervals + members,
each caustic edge's position), the `probes[]` band-sum samples and bin, and the family arm's
`measure` echo (the fourth gate's comparison side). Nothing else — no reconstruction of
`SurveyDocument`, no second deserializer. The partial-reader boundary is the contract: §5's gates
name exactly these blocks, and a consumer needing more joins them here.

## 10. `Lumice raypath probe` output (schema 5)

The probe query's document — the canonical probe library's profile at a user-chosen location.
One line per wavelength: the line's spec echoed in full (every number a reader needs to reproduce
it), the reading's provenance named mechanically, the samples, the two readings (edge and peak,
two things), and the dark-region decomposition. A refused profile carries the spec shell the
request already fixes and a named mode — never silence.

### 10.1 Top-level keys

| Key | Meaning |
|---|---|
| `schema` | `"lumice.path-probe"`. |
| `schema_version` | `5`. Fields are append-only from here. |
| `generator` | `lumice` (the product version) and `analytic_api_version`. |
| `conventions` | The frames, the direction convention, the line's plane and its azimuth, the forms, the value's per-sr meaning, the dark-region classes, the `source` vocabulary and its premise, and the null spelling. |
| `meta` | The request as applied: `crystal` (id, kind, shape scalars, `shape_is_nominal`, pyramid wedges), `faces`, `sun`, `at` (altitude/azimuth/direction/`deviation_deg`/`azimuth_about_sun_deg`), `wavelengths` (one entry per profile: `nm`, `source`, `refractive_index`), `budget_ms`, `interrupted`. |
| `context` | Where the at point sits in the survey's structure — the point document's `survey_context` shape (§3.12), read at the at point: `source` (`warm` \| `inline`), `warm_index_absent`, and the `object` / `edge` / `band_sum` fields with their own states. |
| `profiles` | One per requested wavelength, in order (§10.2). |
| `derived` | Present when the request is an axis form and a 650 nm profile and the reference (the first requested) profile both carry edge readings: `red_edge_width_deg` (the red edge minus the reference edge — the C09 anchor's deviation-axis quantity) and `rule` spelling how it was formed. An arc form's edges are azimuth positions, and no width is derived from them. |

### 10.2 `profiles[]`

| Key | Meaning |
|---|---|
| `along` | The form, echoed: `radial` \| `edge-normal` \| `arc` \| `tangential`. |
| `spec` | The line's caliber, echoed: `delta_min_deg` / `delta_max_deg` (the line's delta extent — the bins' actual span; the arc forms carry an extent of `wedge_half_width_deg` total width, the constant delta ± half of it, while the constant deviation itself lives in each sample's `delta_deg`), `azimuth_deg` (the axis forms' line azimuth, the arc forms' centre azimuth), `window_half_width_deg` (0 = the full axis), `bin_deg`, `wedge_half_width_deg`, `semantics` (what the form claims — the radial form's azimuth-free value, the arc form's constant-delta reading, the edge-normal's anchor rule), `wavelength_nm`, `refractive_index`, `sample_count`, `lattice` (`antipodal_fibonacci`), `deterministic` (a probe consumes no seeds), `representative`, `member_count`. The edge-normal form adds `anchor_basis` (`support_interval_endpoint` \| `caustic_edge_position`) and `anchor_deg`. |
| `source` | The reading's provenance: `warm_reused` (the values are the warm document's own radial samples, checked bin-centre bit-equal against the asked grid — zero band-sum calls) or `recomputed` (band-sum ran here; lattice-deterministic, no seed dependence). The premise — the ruler constants are schema — is stated in `conventions.source` and §5. |
| `evidence` | `state: "finite"` with a `coverage` block (the spec's caliber restated: members evaluated, geometry basis, wavelength, lattice n, bin, delta domain, determinism, representative, member count), or `state: "refused"` with `mode`: `no_edge_in_context` (an edge-normal form with neither a support endpoint nor a certified edge in the context), `degenerate_arc_at_pole` (an arc through the sun or its antipode is not a circle there), `measure_not_expressible` (the config's axis maps to no pose density — the band-sum has no measure to read), `inline_survey_refused` (an edge-normal form whose inline context assembly refused — the context blocks carry the same named reason). |
| `samples[]` | Finite only. One row per bin: `delta_deg` and `azimuth_deg` (the bin centre's two coordinates in the line's plane), `value` (per sr, the band-sum convention), `k` (the bin's event count), `k_eff`. |
| `readings` | Finite only. `has_edge` / `edge_deg` (the first non-empty bin's centre — where events start, finite to half a bin) and `has_peak` / `peak_deg` / `peak_value` / `separation_deg` (the value argmax — where the profile is brightest; a different thing from the edge, and the two stay two fields). The positions sit on the line's sweep coordinate: the deviation for the axis forms, the azimuth for the arc forms (the same rule the `dark_regions` bounds follow). |
| `dark_regions` | Finite only. `segments[]`: consecutive bin runs classified against the context's support — `inside_support_positive` (bins with events), `inside_support_zero` (bins without events inside the support: a weak tail, never clamped into a written zero), `outside_support` (geometrically dark), `unknown` (the context did not certify a support; the run carries its named cause). Each run: `kind`, `lo_deg` / `hi_deg` (along the line's sweep coordinate: deviation for the axis forms, azimuth for the arc forms), `bin_count`, and `reason` when unknown. |

### 10.3 The forms' two line geometries

The four `--along` forms are two geometries on the (deviation, azimuth-about-the-sun) plane, both
fed to the same band-sum core the survey's built-in radial probe runs (one implementation, two
triggers — a slice of the L1 reference-lambda map, never a second evaluation path):

- `radial` and `edge-normal` are delta-axis lines at the line's azimuth: `radial` spans the whole
  axis (the survey probe's own ruler), `edge-normal` windows the same geometry at the context's
  nearest edge. The edge-normal's anchor is the closest of the support intervals' endpoints and
  the certified edge position to the at point's deviation (the context's own strict-lesser walk:
  an endpoint wins a tie against the certified edge).
- `arc` and `tangential` are constant-delta arcs through the at point, swept over azimuth about
  the sun (window ±10 deg, the wedge reading the same constant as a delta extent). The current
  context carries only delta-axis facts (support intervals, the certified edge position), so the
  arc form declares its constant-delta reading of "along the arc" in its `spec.semantics`; a true
  curve-following profile needs the curve context and is not claimed.

The window and ruler constants are schema, not knobs: the bin is the survey probe's 0.1 deg, the
wedge the survey's, the window forms' half width 10 deg, the lattice the survey probe's
antipodal-Fibonacci count (2M; `--events` overrides it, and the warm reuse compares it). Every
profile's spec echoes them. The arc forms' azimuth labels sweep unwrapped: a window crossing the
±180 seam reads azimuths past 180 — the direction math wraps, the labels do not; the sweep
coordinate is the line's own, not re-wrapped to the (-180, 180] convention.

### 10.4 Budget, interruption, and the exit contract

The budget is checked at the per-wavelength boundaries (a band-sum call itself is not
interruptible): a stopped run serializes the profiles it has, sets `meta.interrupted`, and exits
0 — an honest early exit, not a failure. Progress goes to stderr (one line at the start, one at
the end, the same shape as the other raypath commands); with `-o` the document is written to
`<path>.tmp` and renamed over `<path>`, so `<path>` is never half a file.

---
