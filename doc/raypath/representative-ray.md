# The representative ray

> Status: as-built record (v4.60, scrum 742 piece 2). The face is the engine's answer to "show
> me one real crystal-interior path at the pose I selected": one concrete member's (crystal +
> face sequence) legal corridor at one explicit pose and wavelength, recovered as a drawable
> path — the entry point, every internal reflection point, the exit point, and the world-frame
> direction of every leg. Read this before representative-ray work, before touching the C ray
> readers, and before questioning a refusal's cause name.

## What it is, and what it is not

The query binds to the request's member/pose/wavelength. It is never a class-total path, never
a beam, and carries no claim about brightness: the only energy-like quantity it ships is the
per-interface Fresnel factor chain (T at entry and exit, R at internal faces) and its product,
plus the corridor's own A_P at the pose. Display lengths, beam animation and interpolation
between poses are the GUI's business (742.6) and are deliberately absent here.

The result payload (world frame throughout; the pose rides in the metadata so a consumer never
recomputes anything):

- `contacts`: one row per face of the sequence — row k sits on the original finite face
  `faces[k]` — each with its point and that interface's transmittance.
- `legs`: `face_count + 1` rows of world-frame unit directions — row 0 is the incident
  direction, row k runs from contact k−1 to contact k, the last row is the outgoing direction.

## The recovery, on the corridor authority

There is no second geometry implementation anywhere in the chain — the raypath layer sequences
three analytic-kernel authorities and assembles their outputs:

1. **The corridor's gates** (`Corridor::Evaluate`, `src/analytic/entry_measure.cpp`): the
   entry-backface gate, the exit critical-angle gate, and the corridor-area gate (LI's
   `area_eps`). The evaluation also cuts the corridor intersection polygon — the overlap of the
   entry face and every ghost face projected along the internal direction — into the caller's
   `CorridorDiagnostics`.
2. **The chain** (`TracePathChain`, `src/analytic/path_chain.hpp` — the one implementation
   `EvaluatePath` wraps; the ray query calls it directly because it needs the failure's name
   too): direction-level validity, the body-frame segment directions, the per-interface
   transmittances and the world outgoing direction.
3. **The recovery** (`Corridor::RecoverRepresentativePath`): the intersection polygon is
   convex, so its vertex average is interior; that centroid plus the internal direction
   (d = u × w from the recorded projection basis) defines the one straight line of the unfolded
   picture. The line meets every ghost plane in order (a non-increasing meeting is refused);
   each meeting point folds back through the construction's own reflection chain — the full
   affine inverse, not a rotation — to the original finite face. All of the data this needs
   (per-polygon placed planes, original corners, real face planes, whole-crystal half-spaces)
   is recorded once at corridor construction; recovery reads only that record plus the
   diagnostics handed in, so `Evaluate`'s answers are untouched and a recovered pose can keep
   evaluating.

### The independent containment invariants

The recovery verifies its own output against the ORIGINAL crystal — the same checks the unit
tests run independently, run at query time so a numerically degenerate corridor refuses named
instead of emitting a trajectory:

- **I0 plane membership**: every folded point lies on its original finite face's plane. A
  fold-back that dropped the affine translation lands on the origin-parallel plane and misses
  by the face's own offset — the check that pins the fold to full affine positions.
- **I1 finite-face containment**: every folded point lies inside its original polygon (the
  unplaced `FacePolygonTable` ring, in the ring's own winding margins). A wrong face order or
  a wrong transform is caught here.
- **I2 in-crystal propagation**: every leg's midpoint satisfies every present face's inward
  half-space — the convex-crystal containment test over the WHOLE crystal, not just the
  sequence's faces. A path that punches through the crystal is caught here.

The tolerances are one scale-relative bound, `1e-6 × min_edge_length` (`kRecoverTolRel` in
`entry_measure.cpp`): above the float closed-form corners' own planarity noise (per-corner
~FLT_EPSILON × size, so legal thin corridors keep recovering), orders of magnitude below the
feature sizes the area gate resolves, and structural defects (wrong order, dropped translation,
grazing breakdown) violate by crystal-scale amounts, so detection keeps six orders of magnitude
of margin. The I3/I4 checks — recovered leg directions vs `TracePathChain`'s segments, and the
outgoing direction vs the point query's — live in the tests (analytic and raypath suites), not
in the query: they cross two independent mechanisms and are evidence, not gates.

## Refusals

The legal domain is the conjunction of the three authorities; whichever refuses first names the
refusal, which rides the RESULT (never an error code, never zeros). The vocabulary is the
corridor's and the chain's own:

| reason | spoken by | evidence carried |
|---|---|---|
| `entry_backface` | the corridor's entry gate | status name; no geometry (`geometry_evaluated` 0) |
| `exit_critical_angle` | the corridor's exit gate | status name; no geometry |
| `corridor_empty` | the corridor's area gate | status name, measured area ≤ threshold, geometry evaluated |
| `path_infeasible` / `tir_boundary` / `non_finite` | the chain's validity gates | the chain failure name, the corridor's own evidence |

Reachability: the corridor's entry and exit gates encode the same Snell physics as the chain's
first and last gates, so on ordinary inputs a chain-failure name should never surface — the arm
is defensive vocabulary for the gate pair's one remaining seam (the corridor judges the
body-frame optics it derived at the centroid, the chain judges the pose's own direction chain),
and no query-level test constructs a pose that reaches it. The three names exist so the refusal
is never unnamed if that seam ever opens.
| `degenerate_corridor` | the recovery's containment | status name, the corridor's evidence |

A refusal carries the measured corridor quantities (area, threshold, the status names) so a
consumer can show WHY — and it carries no trajectory numbers (`entry_measure` is pinned to 0 at
the completion boundary). Request-level problems (unknown crystal, a face the crystal lacks, a
pose that is no rotation, a wavelength outside the table, more than one layer) are the shared
`Error` vocabulary, not refusals.

## C surface (v4.60)

`LUMICE_AnalyzeRepresentativeRay` (standalone) and `LUMICE_PathWorkspaceQueryRepresentativeRay`
(workspace; shares the prepared-geometry slot, refuses a warm option like the map's) produce the
opaque `LUMICE_RepresentativeRay`; `LUMICE_RepresentativeRayGetInfo` always answers —
`entry_status` spelled on BOTH arms (`"ok"` on the produced one), `geometry_evaluated` 1 and
both measured corridor quantities (`area_perp_internal`, `area_threshold`) carried on both arms
— and `GetContacts` / `GetLegs` answer NOT_AVAILABLE on a refusal. Readers borrow handle-owned
storage; the bulk readers project once and hand out the same pointers on every read; results
outlive scene and workspace. No budget field, no cancel: the query is one synchronous
single-shot evaluation. The C contract tests pin the two-reads-same-pointers rule, the
cross-destroy lifetime, the refusal vocabulary and the standalone/workspace numeric agreement
(`test_representative_ray_c_api.cpp`).

## Cost (measured, not targeted)

Single query, C01 member `3-5` on the random prism, Apple Silicon (M-series, macOS arm64),
working tree at the v4.60 bridge (base `6a7455e04`), 20,000 queries per arm, three runs:

| arm | µs per query |
|---|---|
| standalone entry (scene parsed once; per-query document parse + table build) | 4.7 – 5.5 |
| workspace entry (prepared-geometry reuse) | 1.9 – 2.2 |

Both arms are microseconds: the slider/consumption cost that 742.6 worried about is two orders
of magnitude under any interactive budget, and the prepared-geometry reuse saves roughly half
the standalone cost (the per-query scene-document parse dominates it). There is no hard latency
contract — the honest number is here, and the workspace form is the one a GUI should call.

The recovery's construction-time record is paid by every corridor consumer, not only this face —
measured, not assumed: constructing the corridor (asymmetric pyramid, four faces) went from
~0.29 µs before the record to ~1.20 µs with it (+0.9 µs per construction, the original corner
copies and the per-polygon/crystal plane loops). The other corridor consumers are diagnostic
paths that construct one corridor per row or per call and then run far heavier work behind it —
a `Jet<3>` derivative chain in the diagnostic batch, a multi-pose contour scan in dp_contour —
so the one-time +0.9 µs is noise at their granularity; no lazy-record machinery is warranted at
this number.

## Test anchors

- `test/unit-correctness/analytic/test_entry_measure.cpp` (`EntryMeasureRecovery.*`): the
  corpus's two-face transmission (C01) and internal-reflection (C03 `3-5-6-7`, the asymmetric
  pyramid `13-15-26-28`) pins recover and pass the independent I0–I3 verification; the thin
  corridor just above the area gate recovers while just below it refuses; a corrupted
  intersection is caught by the guards (the detection-power arm); the refusals spell their
  names.
- `test/unit-correctness/raypath/test_representative_ray.cpp`: the typed query — the pose
  sourced from a point query's own discovery output, number-for-number agreement with that
  pose's `PointDetail` (outgoing, per-interface transmittances, total, A_P), the refusal matrix
  on the pin poses, the request refusals and the completion boundary.
- `test/unit-correctness/raypath/test_representative_ray_c_api.cpp`: the C surface contracts
  above.
