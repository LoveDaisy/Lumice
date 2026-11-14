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
angle-Lebesgue **rho — and with it `factor_product` and `fiber_integral` — read identically
zero** (§4), while A/a^2, T and K stay informative per their own availability flags; the
rank-0 orbit-factor payload that does carry a measure-consistent weight exists **only** in the
`point_mass` outcome (`single_path_analysis.cpp:888-907`) and is not a per-pose factor curve
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
| C11 plate `1-4-5-2` @ (20,120) | 1 arc | 35 points, seed at index 14; seed cumulative arclength 6.460°; **rho == 0 on all points** (fixed-axis declared measure, §4); the other arms are present and finite — A/a^2 is 0 at the two valid arc endpoints (legal zero-area, not unavailable), K is a constant 0.9746014667 on the orbit |
| C03 random `3-5-6-7` @ (20,120) | 2 arcs | 56/67 points, seeds at 47/19; splice continuous |
| C10 h0.8 plate `3-6-4-8` @ (20,120) | 2 arcs | 59/71 points; **A/a^2 == 0 on 51/59 and 63/71 valid poses** (finite zero, product finite zero) |
| C01 random `3-5` @ (20,120) | none | `reach.target_in_range == false` — the honest out-of-domain refusal |
| C09 cone `13-15-26-28` @ (20,110) | none | `reach.target_in_range == true` with zero components — reachable, but the finite crystal passes no ray |
| C14 plate `1-2` @ (20,0) | none | `outcome == "point_mass"` (rank 0) — the degenerate chain, curves do not apply |

Count contract across the six real components of the four component-bearing cases (433
points in total); per-segment recomputation, monotone non-negative increments, closed
closure, arc seed placement, and the product invariant
(`factor_product` finite ⟺ `valid && jacobian_available`) on the recomputation bundle's
four components — C16/C11/C03, 303 points — all held (max abs deviation <= 6.2e-17 rad —
double round-trip level); C10's two arcs are recorded as live count rows only. The declared integrals
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
on every point of the C11 plate arc while every other arm is finite and available;
A/a^2 takes a legal zero at the two valid arc endpoints (§2), and T and K are strictly
positive on the arc;
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
in the angle-Lebesgue convention**, under which rho — and its weighted product and integral —
read identically zero on plates, while A/a^2, T and K remain per their own availability flags
(and say the convention in its plan). A measure-consistent contribution curve on an ordinary
discovered plate fiber has **no existing payload**; if 756 needs it, that is a new requirement
to record and put before the owner. This note deliberately does **not** sketch the replacement
— the naive "swap rho for the orbit density and keep the rest" is wrong: the orbit quadrature's
weight and its integration element must share one MeasureBinding
(`fiber_quadrature.hpp:10-14`), the declared density along a spin orbit is a dtheta-parameter
density distinct from the arc-length density (`declared_density.hpp:73-78`), and the existing
K (= 1/(J+eps) of the full SO(3) map, a constant 0.9746014667 on the whole C11 orbit) does not
appear in the orbit integral at all. Whether that K belongs in a measure-consistent fiber
contribution, and how a parameter element relates to the SO(3) arclength axis, are pending
semantics for the owner/756 to adjudicate — not choices this probe pre-judges. Neither the 0
nor the integral is a defect; treating either as missing data would be.

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
   rho, `factor_product` and `fiber_integral` read 0 in the angle-Lebesgue convention while
   A/a^2, T and K stay informative; a measure-consistent curve needs a payload that does not
   exist yet (§4, owner adjudication).
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

## 7. Current assembly refinement contract (2026-11-14)

The snapshot and live counts in §1–§6 remain historical, not today's sample-count promises.
Point assembly now adds bounded **ordinary samples on the geodesic interiors of original
continuation edges**. It does not change continuation, discovery seeds, or the closure-corrected
last pose. No new C entry, environment variable, CLI/config switch, or GUI production change is
required. The existing point execution enables refinement by default; internal C++ requests can
set `payload_evaluation_limit = 0` for an unchanged assembly baseline.

- **Sampling rule.** Probe original-edge midpoints before recursive subdivision, including edges
  whose endpoints are both low. Refine when the positive log-density midpoint departs from the
  endpoint log average by more than 0.005, or the three log values span more than 0.25. Ignore
  tails below 1e-6 of the component's observed anchor/midpoint maximum; never log zero or invent
  a positive floor. Depth is bounded by 16. These are measured finite-resolution choices, not a
  certificate against arbitrary peaks narrower than every probe. The rule targets rho variation,
  not a promise that every other factor has converged.
- **Query-level work bound.** At most 1024 extra evaluations, shared across components: each
  rho-only probe and each full `Detail` plus Jacobian evaluation consumes one unit, including
  rejected candidates and probes not retained. A full evaluation includes its own rho read.
  Quota exhaustion preserves an ordered, structurally complete component; it neither interrupts
  discovery nor certifies curve quality. Cancellation/deadline checks precede each extra
  evaluation; an in-flight evaluation may finish, but no next one starts after the stop is seen.
  Existing execution/interrupted reporting remains the stop authority.
- **Honest diagnostics.** An inserted pose is an interpolation, **not** an accepted corrector
  state and not necessarily an exact solution over the query target. Its `residual_norm` is NaN
  (C copies NaN; JSON writes null), never an inherited endpoint residual or a fabricated zero.
  Its rho, optical validity, A and T are evaluated at that pose. Jacobian/singular values/K use
  `EvaluatePathDiagnostics` at that same pose, with the existing availability gates. Invalid or
  nonfinite candidates are not inserted; positive endpoint margins never certify the interior.
- **Counting and ordering.** `point_count`, `total_points` and `available_points` describe the
  refined ordinary sample list. `forward/backward.pose_count` still count kernel trace anchors.
  Thus trace counts no longer determine `point_count`. Every original pose and diagnostic stays
  unchanged and in order; the original seed appears once and its `seed_index` is remapped. The
  arc's zero remains the backward far endpoint; seed cumulative arclength is unchanged.
- **Lengths and integrals.** Each inserted row splits one original edge. Its subedge increment
  is the directly measured SO(3) pose-pair distance, not a renormalized share; original unsplit
  increments stay unchanged. The N/N−1 relation, original-edge length sums, total length and
  closure remain at double precision (tests allow 1e-12 rad per segment and
  `1e-12 * max(1, length)` rad on sums). The existing left-endpoint integral is recomputed on
  the refined sequence, so its numeric value may change; unusable segments are still skipped,
  never renormalized. Plate angle-Lebesgue zero remains a legal zero, not a new measure curve.

Calibration used a **C16-derived variant**, not the owner's original scene: regular prism h1,
zenith Gaussian 90°±0.25°, full-turn uniform azimuth, roll Gaussian 0°±5°, sun (20°,0°), 550 nm,
literal 3-5, target (40°,30°), 200000 discovery samples, no sun grid. Independent uniform grids
on the fixed off edges (128/256/512/1024 subdivisions) converged in peak height, half-height
width and peak-normalized curve error. The isolated half-height window was 0.110165 rad wide:
off had 2 local samples and 16.18% peak-normalized linear-reconstruction error; on had 25 local
samples, maximum gap 9.09% of that width and 0.1513% error. Stored count changed 119→298, with
476 rho probes + 179 full evaluations. Three interleaved Release query pairs measured a median
on/off ratio of 1.0525; assembly medians were 0.1433→0.8825 ms. These numbers belong to that
machine/input/build, not a cross-platform latency guarantee.

Regression witnesses:

- `SinglePathPayload.*` independently reconstructs the narrow-peak window and error from fixed
  off-edge reference nodes, checks original anchors, subedge distances, seed order and integrals
  on closed C16 / arc C03 / legal-zero C11 / zero-area C10, and checks shared quota/cancel behavior.
- `test_raypath_point_payload_invariants.py` checks the default CLI request, null residuals,
  trace-vs-ordinary counts, pose-pair distances, flags and recomputed integrals.
- The real-frame GUI case `refined_payload_sample_follows_curve_globe_and_trace` reads a genuine
  nonuniform C result, independently accumulates every x coordinate, mouse-selects an added
  NaN-residual sample, and checks its stored index, factor values, submitted globe marker,
  rendered 3D pose and representative-ray key, plus playback reachability. It runs in the
  real-timing pool, not the fixed-dt correctness pool. The consumer remains the one described
  in §5; there is no second GUI sample list.
