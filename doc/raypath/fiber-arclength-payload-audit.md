# Fiber arclength and factor payload audit

> Status: audit snapshot over main `9930c9a89` (2026-11-04). Line anchors below bind to that
> commit; re-verify by symbol name after the code moves. This note is the durable conclusion of
> the payload probe authorized by the 756 issue's owner-action item; its consumers are the 756
> factor-curve plan and the 755.5 pose-trace-window layout work. Question this note answers for
> those readers: *which fields carry a factor curve along a real SO(3) arclength, which points
> must be filtered out, and how refusals and partial results are reported honestly.*

## Verdict

**The existing C point-result payload is sufficient for the 756 multi-factor contribution
curves** (rho, A/a^2, T, K, and their product along a true SO(3) arclength). Every field a
curve needs is populated at the production assembly level for every stored pose; the counting,
ordering and closure contracts below were cross-validated on live query output, not just read
out of headers. **There is no engine-side payload gap**; the 756 work needs no `src/` change to
draw its curves. What 756 must do is honor the consumption contract (§5) — it is consumer-side
discipline, not engine work.

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
  `jacobian`/`singular_values`/`k_carea`/`factor_product` only when `jacobian_available`
  (`:2300-2305`); `fiber_integral` only when `integral_available` (`:2263-2266`). **A zero in an
  unfilled C field is the clear, not data: availability is the flags' job, never a value's.**
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
matched a left-endpoint recomputation to <= 1.7e-21 relative.

The payload invariants above are pinned as an e2e test:
`test/e2e-correctness/test_raypath_point_payload_invariants.py` (closed-producing halo scene,
count/recompute/monotone/closure/null-coherence/integral-rule).

## 3. Availability matrix

The same "unavailable" fact has three faces; a curve consumer filters by the predicate of its
layer. **Grading**: wb✓ = whitebox-established; live✓ = observed live; a live-absent row keeps
its whitebox contract but was not seen in the canonical corpus.

| arm | always populated? | invalid pose | jacobian unavailable | expression per layer | grade |
|---|---|---|---|---|---|
| rho | yes | still computed | still computed | number in all three layers (no NaN path) | wb✓ live✓ (incl. the rho≡0 declared-measure row) |
| A/a^2 | field present | **default 0.0** (read via `valid`) | independent of Jacobian | finite number in all layers | wb✓ live✓ (zero-value row live on C10 h0.8) |
| T | field present | default 0.0 (read via `valid`) | independent | finite number in all layers | wb✓ live✓ (all > 0 live; 0 row not seen) |
| J / sigma | flag-guarded | independent gate | **NaN** / `null` / zero + flag | flag + value/`null`/0 | wb✓ live✓ for available rows; unavailable row wb✓ live— |
| K = 1/(J+eps) | follows J | follows J (not independent) | NaN / `null` / zero + flag | same as J | wb✓ live✓ |
| factor_product | `valid && jac` gate | NaN / `null` / zero + flag | same | same | wb✓ live✓ (0 mismatches over 433 points) |
| `arclength_increments` | present whenever the component is | — | — | plain array, N−1 entries | wb✓ live✓ |
| `fiber_integral` | skip-not-renormalize partial sum | skipped segments | via factor product | `integral_available` flag; `null`/0 when none | wb✓ live✓ (incl. a live available-at-0.0 row) |
| `available_points` / `total_points` | always | — | — | ints | wb✓ live✓ |

Condition rows established whitebox-only (not produced by any canonical live query; the honest
display paths exist and consumers must still implement them, but visual acceptance should not
assume they are visible in typical scenes):

- `jacobian_available == 0` on a stored pose (defensive path; acceptance gates reject first).
- An invalid pose inside a real component (arc end poses sit on boundary events and were valid
  in every live sample).
- `interrupted == true` partial documents (budget exhaustion serializes what it has).
- `incomplete[]` entries carry cause + pose counts only — **no per-point payload by design**
  (`single_path_analysis.cpp` `ToIncomplete`); curves are drawn from `components` alone.

## 4. The rho arm's measure semantics (the one consumer-side trap)

On fixed-axis scenes (plates: zenith and roll pinned, the C11/C10/C14 family) the declared
orientation measure lives on a spin orbit, and **rho — the joint density w.r.t. angle Lebesgue
in three angles — is 0 a.e. on that support**. Live: rho == 0 on every point of the C11 plate
arc while every other factor is finite and positive; `factor_product` and `fiber_integral`
inherit the 0 (`fiber_integral == 0.0` with `integral_available == true` — a usable zero, not
an absence). The document's `measure_registration` block names the support
(`support_kind`, `registered_gap`; JSON field notes at `single_path_json.cpp:75-84`); the
measure-consistent weighting for such supports is the rank-0 orbit-factor branch
(`OrbitFactors`, `point_result.hpp:102-124`), not the per-pose rho arm.

756 must therefore make an explicit convention decision for plate-family scenes — either
declare that its curves are in the angle-Lebesgue convention (and read identically zero on
plates) or switch to the orbit-measure branch for them — and say so in its plan. Neither the
0 nor the integral is a defect; treating either as missing data would be.

## 5. Consumption contract for factor curves (756 / 755.5)

1. **x-axis**: within one component, `cumsum(arclength_increments)` (radians). Closed: zero at
   the seed (`seed_index == 0`, first and last poses coincide to machine precision, loop length
   is whatever the physics gives — 328.4° live, not 360°). Arc: zero at the backward far-end
   boundary event; the seed sits mid-curve at cumulative `sum(increments[:seed_index])`.
2. **Filter predicate per layer**: JSON — `jacobian.available && valid` (equivalently
   `factor_product != null`); C — the flags only (`jacobian_available`, `valid`,
   `integral_available`), **never a value comparison against 0** (§1 zero-fill).
3. **Read `measure_registration` before trusting a rho curve** (§4).
4. **Integrals**: left-endpoint rule, skipped segments counted in completeness, not
   renormalized; recompute with the same rule or numbers will disagree by the skipped share.
5. **Components are the only curve source**: `incomplete[]` has no points; `point_mass` and
   empty-outcome documents have no curve at all (rank-0 chains need the orbit branch; refusals
   are honest empties — distinguish them via `reach`, per `doc/raypath/raypath-cli-output.md`).

## 6. Scope of this audit

Read-only: no `src/` behavior change, no ABI change (hard constraint of the probe). The
repository deltas this probe contributed are the e2e invariant test above, this note, and its
index line; the live query outputs and the recomputation script were working files of the
probe's task record and are intentionally not repository assets. Downstream: 756 (factor
curves) consumes §4-§5; 755.5 (pose-trace window) consumes §1's splice/zero conventions and §5
item 1.
