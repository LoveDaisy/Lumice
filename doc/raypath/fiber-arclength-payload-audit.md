# Fiber arclength and factor payload audit

> Status: audit snapshot over main `9930c9a89` (2026-11-04). Line anchors below bind to that
> commit; re-verify by symbol name after the code moves. This note is the durable conclusion of
> the payload probe authorized by the 756 issue's owner-action item; its consumers are the 756
> factor-curve plan and the 755.5 pose-trace-window layout work. Question this note answers for
> those readers: *which fields carry a factor curve along a real SO(3) arclength, which points
> must be filtered out, and how refusals and partial results are reported honestly.*

## Verdict

**The existing C point-result payload is sufficient for drawing 756's multi-factor contribution
curves in the existing angle-Lebesgue convention** (rho, A/a^2, T, K, and their product along a
true SO(3) arclength). Every field such a curve needs is populated at the production assembly
level for every stored pose, and the counting, ordering and closure contracts below were
cross-validated on live query output, not just read out of headers. Drawing those fields
requires **no `src/` change**.

The verdict is scoped, and one boundary matters for 756's plan: this says nothing about a
*measure-consistent* contribution curve on fixed-axis (plate) scenes. On such scenes the
angle-Lebesgue factors read identically zero (§4); the rank-0 orbit-factor payload that does
carry a measure-consistent weight exists **only** in the `point_mass` outcome
(`single_path_analysis.cpp:888-907`) and is not a per-pose factor curve
(`point_result.hpp:102-108` has no K, no product, no SO(3) arclength), so an ordinary
discovered plate fiber has **no existing data source to switch to**. If 756 needs
measure-consistent plate curves, that is a new minimum-payload requirement to put before the
owner (§4), not a consumption-side option.

What 756 must honor either way is the consumption contract (§5) — consumer-side discipline,
not engine work.

## 1. Production chain, per stage

```
TraceFiber (src/analytic/fiber_continuation.hpp)      trace: poses + increments + per-pose diagnostics
  -> ToComponent (src/raypath/single_path_analysis.cpp)  assembly: two-way splice + factor Detail() + integral
  -> FiberComponent (src/raypath/point_result.hpp)      storage: NaN-honest PointDetail
  -> LUMICE_PathPointGet{Component,Sample} (src/server/c_api_raypath.cpp)
  -> ComponentJson (src/raypath/single_path_json.cpp)   JSON face: Num() non-finite -> null
```

- **Increments are per-step, not cumulative.** Each accepted step pushes one
  `so3::Distance(current, candidate)` (`fiber_continuation.hpp:806-807`, advance defined at
  `:640`); increment `i` connects stored poses `i` and `i+1`. Units: radians on SO(3)
  (`lumice_analytic_core.h:304`, `point_result.hpp:203-204`).
- **Closure replaces, not appends.** On a closed loop the last pose is replaced by the
  closure-corrected pose and the last increment by `Distance(before_last, closing_pose)`
  (`fiber_continuation.hpp:852-853`, closing advance at `:842`). `sum(increments)` is the full
  loop length.
- **`jacobian_available`** comes from `NormalJacobianOf` at the pose's own direction, written
  per pose by `WriteDiagnostics` from the same evaluation that accepted the pose
  (`fiber_continuation.hpp:283-304`, `:665-675`; the push-0 at `:686` is the pre-init placeholder
  that `WriteDiagnostics` immediately overwrites — the owning write, not a separate path). A
  pose that fails evaluation is never appended, so a stored row with `available == 0` is the
  defensive-honesty path, not a normal state.
- **Arc splice.** Backward poses are appended reversed (dropping the backward trace's pose 0,
  which is the seed the forward trace owns), forward poses in order
  (`single_path_analysis.cpp:379-393`); backward increments likewise reversed (`:386-388`);
  `seed_index = nb - 1` (`:389`). **The arclength zero of an arc sits at the first point of the
  combined list — the backward trace's far boundary event — not at the seed**; the seed's
  cumulative arclength is the backward leg's length.
- **Per-pose factors** (`Detail`, `single_path_analysis.cpp:263-309`): `rho` is computed
  unconditionally; `jacobian`/`singular_values`/`k_carea = 1/(J + epsilon)` follow
  `jacobian_available` (epsilon = `kFactorsEpsilon = 1e-12`, `point_result.hpp:21`);
  `total_transmission`/`entry_measure` are filled only for a valid pose and keep their storage
  default `0.0` otherwise — **a 0 there is not distinguishable from "unavailable" except
  through the `valid` flag**; `entry_measure_a2 = entry_measure` (a = 1, declared identity,
  `:302`); `factor_product = rho * A/a^2 * T * K` exactly when `valid && jacobian_available`,
  NaN otherwise (`:303-307`).
- **Component integral**: left-endpoint rectangular sum of `factor_product * increment` over
  segments whose first point carries a finite product; skipped segments are not renormalized;
  `integral_available` true iff at least one segment contributed
  (`single_path_analysis.cpp:396-413`, `point_result.hpp:181-189`). `available_points` counts
  *points* with a usable product (`total_points` all points) — note the last point never opens
  a segment, so usable points and contributing segments differ by one.
- **C face = zero-fill plus conditional fill.** `BeginRead` value-initializes the whole output
  struct before the readers fill it (`c_api_raypath.cpp:1370-1383`); both
  `LUMICE_PathPointGetSample` (`:2272`) and `LUMICE_PathPointGetComponent` (`:2235`) enter
  through it. `rho`/`A/a^2`/`T`/`valid`/pose fields are written unconditionally (`:2286-2298`);
  `jacobian`/`singular_values`/`k_carea`/`factor_product` are written whenever
  `jacobian_available`, **whatever `valid` says** (`:2300-2305`); `fiber_integral` only when
  `integral_available` (`:2263-2266`). Two consequences a C consumer must not conflate:
  - with `jacobian_available == 0` the fields stay at the BeginRead zero — **a zero there is
    the clear, not data**;
  - with `jacobian_available != 0` but `valid == 0`, storage carries NaN for `factor_product`
    (`single_path_analysis.cpp:303-307`) and the reader copies that NaN through — the C caller
    receives NaN, not the zero. (An invalid-but-Jacobian-available pose was not observed live;
    whitebox-verified only.)
- **JSON face = null, with the flag alongside.** Every factor field goes through `Num()`
  (non-finite -> `null`, `detail/json_values.hpp:33-39`) and the `jacobian.available` boolean
  rides along (`single_path_json.cpp:179-182`); `fiber_integral` is `null` with its rule string
  when unavailable (`:191-207`). Poses serialize at full double precision (`Num`, not the
  sun-grid's 9-digit `GridNum`), so pose-pair recomputation survives the round trip.

## 2. Live verification (canonical cases, CLI `raypath point`)

Independent recomputation: geodesic angle from consecutive stored `pose[9]` pairs, atan2 form
(`so3.hpp:135-146`), never re-reading the increment field. Two chains, deliberately not sharing
a failure mode: whitebox assembly reading vs. arithmetic on live output.

| case (chain, target) | components | result |
|---|---|---|
| C16 column, sun 20, target (−20,0), 550 nm (`3-5`) | 1 closed | 145 points / 144 increments; first-last pose distance **6.4e-16 rad**; loop length 328.398° (a closed loop has no canonical angle); matches the corpus C16 record |
| C11 plate `1-4-5-2` @ (20,120) | 1 arc | 35 points, seed at index 14; seed cumulative arclength 6.460°; **rho == 0 on all points** (fixed-axis declared measure, §4) |
| C03 random `3-5-6-7` @ (20,120) | 2 arcs | 56/67 points, seeds at 47/19; splice continuous |
| C10 h0.8 plate `3-6-4-8` @ (20,120) | 2 arcs | 59/71 points; **A/a^2 == 0 on 51/59 and 63/71 valid poses** (finite zero, product finite zero) |
| C01 random `3-5` @ (20,120) | none | `reach.target_in_range == false` — the honest out-of-domain refusal |
| C09 cone `13-15-26-28` @ (20,110) | none | `reach.target_in_range == true` with zero components — reachable, but the finite crystal passes no ray |
| C14 plate `1-2` @ (20,0) | none | `outcome == "point_mass"` (rank 0) — the degenerate chain, curves do not apply |

Across the four real components (433 points): count contract, per-segment recomputation
(max abs deviation <= 6.2e-17 rad — double round-trip level), monotone non-negative increments,
closed closure, arc seed placement, and the product invariant
(`factor_product` finite ⟺ `valid && jacobian_available`) all held. The declared integrals
matched a left-endpoint recomputation to <= 1.8e-21 **absolute** (relative <= 1.8e-16 on the
non-zero integrals; the C16 loop's integral is ~5.1e-63, where only the absolute statement is
meaningful).

The payload invariants above are pinned as an e2e test:
`test/e2e-correctness/test_raypath_point_payload_invariants.py` (closed-producing halo scene,
count/recompute/monotone/closure/null-coherence/integral-rule).

## 3. Availability matrix

The same "unavailable" fact has three faces; a curve consumer filters by the predicate of its
layer. The two unavailability gates (`valid`, `jacobian_available`) are independent and the C
face treats them differently, so the matrix splits their four combinations for the gated arms.
**Grading**: wb✓ = whitebox-established; live✓ = observed live; a live-absent row keeps its
whitebox contract but was not seen in the canonical corpus.

| arm | valid ∧ J-avail | valid ∧ ¬J-avail | ¬valid ∧ J-avail | ¬valid ∧ ¬J-avail | grade |
|---|---|---|---|---|---|
| rho | finite number (all layers) | finite number | finite number | finite number | wb✓ live✓ (incl. the rho≡0 declared-measure row) |
| A/a^2 | finite number | finite number | **storage 0.0** / JSON `0` / C 0 — read only via `valid` | same | wb✓ live✓ (zero-value row live on C10 h0.8; ¬valid rows live—) |
| T | finite number | finite number | as A/a^2 | as A/a^2 | wb✓ live✓ (all > 0 live) |
| J / sigma | finite | NaN / `null` / **C zero** (flag clear → BeginRead clear survives) | finite (independent gate) | NaN / `null` / C zero | wb✓ live✓ for J-avail rows; ¬J-avail rows wb✓ live— |
| K = 1/(J+eps) | finite | as J | finite (follows J) | as J | wb✓ live✓ |
| factor_product | finite | NaN / `null` / **C zero** | NaN / `null` / **C NaN** (reader copies storage NaN through; `c_api_raypath.cpp:2300-2305` gates on `jacobian_available` only) | NaN / `null` / C zero | wb✓ live✓ (0 mismatches over 433 points; ¬valid∧J-avail cell wb✓ live—) |
| `arclength_increments` | present (N−1 entries) | present | present | present | wb✓ live✓ |
| `fiber_integral` | contributes its segment | segment skipped | segment skipped | segment skipped | wb✓ live✓ (incl. a live available-at-0.0 row) |
| `available_points` / `total_points` | counts | — | — | — | wb✓ live✓ (ints, always present) |

The one-line filter for every layer and every combination stays: usable ⟺
`valid && jacobian_available` (JSON: `factor_product != null`; C: both flags). A C-side value
comparison against 0 is never a usability test — but note the ¬valid∧J-avail cell above hands
the C caller a NaN, not a zero, so "check the flags" is the *only* portable rule. Component
level: the `fiber_integral` row reads each segment by its **first** point's combination;
`integral_available` is true iff at least one segment contributed, with `null` (JSON) or the
BeginRead zero (C) otherwise (`single_path_analysis.cpp:396-413`); `available_points` counts
points, not segments.

Condition rows established whitebox-only (not produced by any canonical live query; the honest
display paths exist and consumers must still implement them, but visual acceptance should not
assume they are visible in typical scenes):

- `jacobian_available == 0` on a stored pose (defensive path; acceptance gates reject first).
- An invalid pose inside a real component (arc end poses sit on boundary events and were valid
  in every live sample).
- `interrupted == true` partial documents (budget exhaustion serializes what it has).
- `incomplete[]` entries carry cause + pose counts only — **no per-point payload by design**
  (`single_path_analysis.cpp` `ToIncomplete`); curves are drawn from `components` alone.

## 4. The rho arm's measure semantics and the plate-scene boundary

On fixed-axis scenes (plates: zenith and roll pinned, the C11/C10/C14 family) the declared
orientation measure lives on a spin orbit, and **rho — the joint density w.r.t. angle Lebesgue
in three angles — is 0 a.e. on that support** (`declared_density.cpp:505-513`: a singular slot
returns 0.0, its mass carried by UMarginal's support kinds, not by this double). Live: rho == 0
on every point of the C11 plate arc while every other factor is finite and positive;
`factor_product` and `fiber_integral` inherit the 0 (`fiber_integral == 0.0` with
`integral_available == true` — a usable zero, not an absence). The document's
`measure_registration` block names the support (`support_kind`, `registered_gap`; JSON field
notes at `single_path_json.cpp:75-84`).

What does **not** follow: that a measure-consistent plate curve is available by switching
payloads. The only measure-consistent weight in the tree is the rank-0 orbit-factor branch, and
its boundary is hard:

- `orbit_factors` is built **only** when the outcome is `point_mass` (`rank.rank_zero`,
  `single_path_analysis.cpp:888-907`). A discovered plate fiber — C11's 35-point arc, C10's
  arcs — carries no orbit block at all.
- Even in the rank-0 outcome, `OrbitFactorPoint` (`point_result.hpp:102-108`) carries
  `theta_rad`/`rho_theta`/`entry_measure_a2`/`total_transmission`/`valid` — **no K, no factor
  product, no SO(3) arclength** — i.e. it is a different quadrature on a different orbit
  parameter, not the per-pose factor curve 756 draws.

So for plate-family scenes 756 has exactly one zero-cost option: **declare that its curves are
in the angle-Lebesgue convention**, which reads identically zero on plates (and say so in its
plan). A measure-consistent contribution curve on an ordinary discovered plate fiber has **no
existing payload**; if 756 needs it, that is a new requirement to record and put before the
owner. Its minimal shape, stated here only as a requirement sketch (the probe's constraint
forbids implementing it): a per-pose weight on the fiber's own poses that replaces the
angle-Lebesgue rho with the declared measure's density (the UMarginal/spin-orbit coordinate
along the same poses), assembled at the same point as `Detail` (`single_path_analysis.cpp`
`ToComponent`), so that `factor_product`'s fourth arm becomes measure-consistent while the
increment-based arclength stays as is. Neither the 0 nor the integral is a defect; treating
either as missing data would be.

## 5. Consumption contract for factor curves (756 / 755.5)

1. **x-axis**: within one component, build `x[0] = 0`, `x[i+1] = x[i] + increments[i]` — i.e.
   `[0]` prefixed to `cumsum(arclength_increments)` — so the axis has exactly `len(points)`
   entries, one per point (the increments themselves are the N−1 edges). Filter factors by the
   predicate below **keeping each point's original index and x value**; never delete points
   first and re-cumulate. Closed: zero at the seed (`seed_index == 0`, first and last poses
   coincide to machine precision, loop length is whatever the physics gives — 328.4° live, not
   360°). Arc: zero at the backward far-end boundary event; the seed sits mid-curve at
   `x[seed_index]`.
2. **Filter predicate per layer**: JSON — `jacobian.available && valid` (equivalently
   `factor_product != null`); C — the flags only (`jacobian_available`, `valid`,
   `integral_available`), **never a value comparison against 0 or NaN** (§3: a C caller sees
   the BeginRead zero when the Jacobian flag is clear but storage's NaN copied through when
   only `valid` fails).
3. **Read `measure_registration` before trusting a rho curve** (§4). On plate-family scenes,
   remember the boundary: angle-Lebesgue curves read 0; a measure-consistent curve needs a
   payload that does not exist yet (§4, owner adjudication).
4. **Integrals**: left-endpoint rule, skipped segments counted in completeness, not
   renormalized; recompute with the same rule or numbers will disagree by the skipped share.
5. **Components are the only curve source**: `incomplete[]` has no points; `point_mass` and
   empty-outcome documents have no curve at all (the rank-0 orbit branch answers a different
   question — §4; refusals are honest empties — distinguish them via `reach`, per
   `doc/raypath/raypath-cli-output.md`).

## 6. Scope of this audit

Read-only: no `src/` behavior change, no ABI change (hard constraint of the probe). The
repository deltas this probe contributed are the e2e invariant test above, this note, and its
index line; the live query outputs and the recomputation script were working files of the
probe's task record and are intentionally not repository assets. Downstream: 756 (factor
curves) consumes §4-§5; 755.5 (pose-trace window) consumes §1's splice/zero conventions and §5
item 1.
