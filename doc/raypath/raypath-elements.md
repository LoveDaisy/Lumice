[中文版](raypath-elements.zh.md)

# The Element Algebra: D6h Members, Three Views, and Local Classes

This document records the engine's per-element symmetry algebra — the closed-form derivation of
every D6h group member's three views (label permutation, pose compensation, sky action), the
per-element measure-retention conditions and how each reduces to the existing per-bit family
predicates, and the local-class definition they serve. `src/core/crystal.hpp`'s `CrystalElement`
is the production surface; the three-view tests
(`test/unit-correctness/core/test_crystal_element.cpp`) and the optical witnesses
(`test/unit-correctness/analytic/test_element_optical_views.cpp`) pin every formula here.

Readers: contributors touching `CrystalElement`, the local-class resolution, or the pose
compensation. The per-bit P/B/D language lives in
[raypath-symmetry.md](raypath-symmetry.md); this document is the layer underneath it.

---

## 1. Why elements, not bits

The P/B/D bits and their per-bit gates (`SymmetryGating`, `DeriveSymmetryGating`) name group
**generators**. A local equivalence class is an orbit under whole group **elements**, and the
element doing the work is often one no bit pattern names: the composite `B·σ_a` is a single
proper half-turn about a horizontal axis whose label action mixes D's prism reflection with B's
basal/cone swap and whose pose compensation reflects both roll and zenith. On the Parry side of
the −52° tangent arc, it is `B·σ_0` — not B, not D, and not any bit pair — that merges 4-2 with
8-1 into one physical class, because the compensation formulas of B alone and D alone describe
different pose maps than the composite does.

Consequences the implementation takes as axioms:

- A local class is computed **per element**; it is never projected onto the P/B/D bits, and the
  bits keep their list/filter semantics untouched.
- The composite's measure-retention must be judged as a whole (§4): its three conjuncts happen to
  be components of the existing per-bit predicates, but neither "B applicable" nor "D applicable"
  alone implies the composite is retained (and conversely).

## 2. Conventions (all read off the engine, not assumed)

- Pose: `R = Rz(azimuth − 180°) · Ry(−zenith) · Rz(roll)`, row-major body → world
  (`single_path_analysis.hpp`'s stated contract).
- Face normals: prism face `3+i` sits at crystal-frame azimuth `i·60°` (`kHexFaceCos/Sin`),
  basal 1 at `+z`, basal 2 at `−z`; **face 3's normal is at azimuth 0** — the anchor every
  formula below is derived against.
- `σ_a` is the vertical mirror plane at azimuth `a·30°`, mapping prism index `i → (a−i) mod 6`
  (the same plane `MirrorFaceIndex(i, sigma_a)` names). `B` (σ_h) is the horizontal mirror
  `diag(1, 1, −1)`: basal 1↔2, cone rings 13..18 ↔ 23..28, prism faces fixed.
- Angles are mod 360°; the closed-form angle maps below are stated in the canonical zenith chart
  `zen ∈ [0°, 180°]`. Degenerate charts (zen = 0° or 180°, where azimuth and roll are not
  unique) are handled at matrix level — the production interface (`CompensatedPose`) returns
  matrices, and the angle maps must agree with them as matrices **at the poles too** (§6).

## 3. The conjugation trap

The one derivation error worth writing down: for a **proper** `g`,
`g · R_axis(θ) · g⁻¹ = R_{g·axis}(θ)` — conjugation preserves the angle. For an **improper** `g`
(a mirror), it flips the angle:

```
g · R_axis(θ) · g⁻¹ = R_{g·axis}(−θ)     (det g = −1)
```

The useful consequences (F_y = diag(1,−1,1), the vertical mirror at azimuth 0; σ_h = diag(1,1,−1)):

| Conjugation | Result | Reading |
|---|---|---|
| `F_y · Rz(θ) · F_y` | `Rz(−θ)` | axis fixed (z in the mirror plane... the plane contains ±z), angle flips |
| `F_y · Ry(θ) · F_y` | `Ry(θ)` | axis flips to −y AND angle flips: the two cancel — **F_y commutes with Ry** |
| `σ_h · Rz(θ) · σ_h` | `Rz(θ)` | axis flips to −z AND angle flips: they cancel — **σ_h commutes with Rz** |
| `σ_h · Ry(θ) · σ_h` | `Ry(−θ)` | axis fixed, angle flips |

The mnemonic: a mirror that CONTAINS the rotation axis commutes with it; a mirror perpendicular
to the axis inverts the rotation. (F_y's plane is the xz-plane, which contains y as a line? No —
y points out of the xz-plane; F_y sends y to −y, still on the same axis LINE, and the improper
angle flip turns `R_{−y}(θ)` into `R_{+y}(θ)`.) Dropping this flip produced a σ_a compensation
with a spurious extra ±180° that contradicted `ComputeSigmaA` and the "roll plays no part" clause
of B's condition — both of which the corrected derivation reproduces exactly (§4, §5).

## 4. The five families: three views each

D6h has 24 elements; the engine names them in five families (`CrystalElement::Family`), with
`ψ_a = a·30°`, `φ_s` the sun's world azimuth:

| Family | Matrix (crystal frame) | det | Label action | Pose compensation `T_g` | Sky action `σ_g` |
|---|---|---|---|---|---|
| identity | I | +1 | fixes every face | unchanged | identity |
| `P_k` | `Rz(60k°)` | +1 | prism index `i → i+k` (basal fixed) | `az`, `zen` unchanged; `roll → roll − 60k°` | identity |
| `H_a` = `B·σ_a` | `2ûûᵀ − I`, `û` the horizontal direction at azimuth `ψ_a` | +1 | basal 1↔2; prism `i → a−i`; cone rings swap | `az → az + 180°`; `zen → 180° − zen`; `roll → −roll − 60a°` | identity |
| `σ_a` | `Rz(60a°)·F_y` | −1 | prism `i → a−i` (basal, rings fixed) | `az → 2φ_s − az`; `zen` unchanged; `roll → −roll − 60a°` | the sun's vertical mirror `M` |
| `S_k` = `σ_h·Rz(60k°)` | `diag(1,1,−1)·Rz(60k°)` (k=3: the inversion) | −1 | basal 1↔2; prism `i → i+k`; cone rings swap | `az → 2φ_s − az + 180°`; `zen → 180° − zen`; `roll → roll − 60k°` | the sun's vertical mirror `M` |

`M = Rz(2φ_s)·F_y`: the world mirror whose plane contains the sun. Only a reflection fixing the
incident light turns a mirrored scene into the same scene, which is why the improper
compensation `R' = M·R·gᵀ` is the only one available and why an improper element moves sky
points: the sibling member carries, at `M(q)`, what the original carries at `q`.

Key identities used in the derivation (all verified by the core tests at matrix level):

- `Rx(π)·Rz(γ) = Rz(−γ)·Rx(π)` and `Rx(π) = Ry(π)·Rz(π)` — the H_a reduction.
- The gauge freedom `Rz(A)·Ry(B)·Rz(C) = Rz(A+π)·Ry(−B)·Rz(C−π)` — canonicalising the zenith.
- `σ_a·Rz(60k°) = σ_{a−k}` — why five families close the group (1 + 5 + 6 + 6 + 6 = 24).

### 4.1 Why the compensation has that shape

The equivariance behind both regimes, stated once: the crystal-frame trace satisfies
`T(slots_g, g·u) = g·T(slots, u)` for every element (the crystal is `g`-invariant; the g-image
of a valid trace is the trace of the relabelled sequence). A proper `g` then compensates inside
SO(3): with `R' = R·gᵀ` the sibling's body-frame incident direction is `g·u` and its world rays
coincide with the original's segment for segment — same sky point. An improper `g` cannot stay
in SO(3) by itself; the world mirror M repairs the determinant (`det M · det gᵀ = +1`) exactly
when it fixes the sun, and the sibling's world rays come out mirrored — same ray pattern,
reflected, at the mirrored sky point.

## 5. Per-element measure retention

`μ∘T_g = μ` decomposes along `T_g`'s three angle slots, and every conjunct is an existing
closed-form predicate — no new family rule is needed (the pre-implementation hypothesis, here
confirmed):

| Condition | Posed by | Existing predicate |
|---|---|---|
| azimuth uniform over a full turn | az shift (P_k, H_a, S_k) or az reflection (σ_a, S_k, H_a) | `IsFullTurnUniform(azimuth)` |
| zenith symmetric about 90° | `zen → 180° − zen` (H_a, S_k) | the zenith conjunct of `IsBApplicable` |
| roll shift by −60k | `roll → roll − 60k°` (P_k, S_k) | roll full-turn uniform (`IsPApplicable`); the pole-fold gluing keeps the k=3 half-turn (`GluingRetainsHalfTurn`) |
| roll reflection about −a·30° | `roll → −roll − 60a°` (H_a, σ_a) | the roll anchor names `σ_a`: `ComputeSigmaA(roll mean) = a` is exactly this statement |

Two structural warnings the tests pin:

- **Judge the composite as a whole.** H_a's retention = (az uniform) ∧ (zen symmetric) ∧ (roll
  symmetric about −ψ_a), all three at once. B alone does not touch roll and σ_a alone does not
  touch zenith; neither one's applicability implies the composite's.
- **Retention is not admission.** A measure-retained improper element still moves sky points, so
  it enters a local class at `q` only when `σ_g(q) = q` — `ElementFixesSkyDirection`. The Parry
  axis retains B (uniform azimuth, zenith symmetric about 90°, roll untouched) and yet B joins no
  ±52° class, because ±52° is not in the sun's vertical plane.

Worked scenarios (the acceptance cases, restated in this algebra):

- **Parry** (azimuth uniform, zenith Gaussian 90°±1°, roll Gaussian 0°±1°): retained elements =
  σ_0, H_0, B/S_k with the roll conditions the ±1° roll forbids except k=0; all improper ones
  drop out of any ±52° class by the sky action. At −52° the classes are the H_0 orbits
  {4-2, 8-1} and {4-1, 8-2}; the lit one is {4-2, 8-1}, representative 4-2 by lexicographic
  order.
- **Column** (azimuth uniform, zenith locked 90°, roll full-turn uniform): P_k and H_a are all
  retained; the twelve literal members of the tangent-arc pair 3-5/3-7 form one orbit whose
  lexicographic representative is 3-5, stable along the arc because `G_q` is q-independent here.
- **Plate** (azimuth uniform, zenith locked 0°): nothing with `zen → 180° − zen` is retained, so
  the class at each side of the sun is the single member; the two sides are connected by σ_a,
  which is exactly the statement "each side is carried by one member, and the two sides' integral
  energies are equal".

## 6. Exactness: what is bit-exact, and why

- The grid/witness elements with axis at azimuth 0 (H_0, σ_0, P_3) act on directions by exact
  sign flips, and the face-normal table lives on ±1, ±1/2, ±√3/2 — IEEE-exact constants. Every
  dot product in the chain flips term by term with the summation order unchanged, so the
  deviation-grid identity `D_{gP}(g·u) = D_P(u)` and the H_0/σ_0 world-ray witnesses hold **bit
  for bit** (the `analytic` test asserts equality, not tolerance).
- Elements with a 30°-phase axis (H_a, σ_a, a ≠ 0) carry √3/2 into their matrices; their
  identities are held to the 1e-12 kinematic band, not to bit equality.
- The sky-view predicate's 1e-12 band is the azimuth-wrap rounding allowance (sin(±π) and
  sin(±2π) round to ≤ 2.5e-16), not a physical tolerance: a genuine off-plane azimuth of 1e-9 rad
  reads ≥ 1e-9·cos(altitude), five orders above the band.

## 7. What this layer does not claim

- Retention predicates are stated per element for the repo's distribution families; a new
  distribution type extends the conjunct tables (the per-bit predicates' own switch statements),
  not this document's algebra.
- The local-class DEFINITION and its orbit/naming machinery live one layer up (the raypath
  module); this document and `CrystalElement` provide the per-element facts those layers consume.
- No claim here is a sampling statement: every formula is closed-form and every test above the
  grid/witness level is pure algebra.

---

## See Also

- [Raypath Symmetry — P, B, D Filter Toggles](raypath-symmetry.md) — the per-bit layer and its two meanings
- [Coordinate Convention](../geometry/coordinate-convention.md) — the pose and direction conventions restated in §2
