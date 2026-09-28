[中文版](raypath-symmetry.zh.md)

# Raypath Symmetry: P, B, D Filter Toggles

This document explains the geometric reasoning behind the `symmetry` field in Lumice filters,
covering the two meanings one P/B/D bit set has (a filter's label equivalence and the raypath
analysis list's physical classes, §1.1), the two sources of raypath equivalence, the precise
semantics of the P, B, and D toggles, the orientation-ensemble condition each of them needs for a
physical merge, D's mirror derivation, and the corresponding GUI behavior.

**Target audience**: advanced users who want to understand filter behavior at the crystal
geometry level, beta testers, and future contributors.

---

## 1. Background

Lumice traces light rays through ice crystal faces and accumulates the ray path as a sequence
of face numbers (e.g., `[3, 5]` for a prism-to-prism path). Two geometrically distinct ray
paths are **equivalent** if they produce the same observable halo pattern.

Exploiting raypath equivalence reduces the number of unique paths the simulator must track:
instead of storing every symmetric variant separately, `ReduceRaypath` maps all members of an
equivalence class to a single canonical representative. `ExpandRaypath` does the inverse —
regenerating all symmetric variants from the canonical form for output purposes.

The `symmetry` field on a filter is a bitmask of which equivalence relations to apply.

### 1.1 Two Meanings of One Bit Set

The same P/B/D bits mean two different things, depending on who reads them. The engine names the
meaning at every reduction (`SymmetrySemantics` in `src/core/crystal.hpp`, translated into gating
by the single function `DeriveSymmetryGating`); nothing infers it.

- **A filter's P/B/D (and a colour ref's) is a label equivalence.** P relabels the prism faces by
  any multiple of 60°, B swaps 1↔2 and the upper and lower cones (13..18 ↔ 23..28), whatever the
  crystal's shape and orientation distribution; D mirrors in the plane the roll mean selects,
  when §4's condition holds. It is a shorthand for writing a family of face sequences at once, and
  a saved filter keeps meaning the same sequences whatever crystal it is bound to. On a crystal
  whose shape or orientation lacks the element (§2) a filter's P/B/D therefore merges paths that
  are **not** physically equivalent — a Parry arc's lit 3-5 with its dark 4-6, a three-fold prism's
  near-face paths with its far-face ones. That is the user's call: the filter editor says so
  beside the checkbox (§6), and a filter that must select one physical class writes its members
  as separate rows with no symmetry instead.
- **The raypath-analysis list's P/B/D is a physical equivalence.** One row of that list is one
  physical class — a quantity its energy share describes — so a row merges two paths only when the
  crystal's shape (§2a) **and** its orientation ensemble (§2b) make them equivalent. The list's
  reduction (`server/raypath_histogram_consumer.cpp`) and the chain ids it reads use this meaning.
  See `raypath-analysis-panel.md` §7.

The two are why "Exclude this raypath" (analysis panel) sometimes writes a filter with no symmetry
and one row per member: a list row is a physical class, and when the label class under the same
bits is larger, the bits would remove paths the row does not hold.

§2 and §4 below are the mathematics both meanings are stated in; what differs is whether the
conditions in §2 decide what merges (the analysis list) or only what the filter editor warns about
(a filter).

---

## 2. Two Sources of Symmetry

### 2a. Crystal Geometric Symmetry (D6h — intrinsic)

A hexagonal ice crystal belongs to the D6h point group, which contains:

- **C6**: six-fold rotation about the c-axis (60° steps)
- **σh**: one horizontal mirror plane (perpendicular to the c-axis)
- **σv / σd**: six vertical mirror planes (alternating through prism faces and prism edges)
- Plus the combination of these (improper axes, inversion)

This geometric symmetry is **intrinsic** — it holds for any ice crystal regardless of how it
is oriented in the atmosphere.

**But only for a regular shape.** D6h is the symmetry of a *regular* hexagonal prism or of a
pyramid whose upper and lower cones match. A config can describe less: `face_distance`
`[1, 1.2, 1, 1.2, 1, 1.2]` has a three-fold axis (rotations by 120°, three vertical mirrors), not
a six-fold one; unequal `upper_h` / `lower_h` or wedge angles remove σh. The analysis list's
reduction therefore uses **the elements the toggles request, intersected with the elements the
crystal's shape really has and with the elements its orientation ensemble admits** (§2b) — never
more. Merging by an element the shape lacks folds inequivalent paths into one row: on the
three-fold prism above, P merges near-face and far-face paths whose energies differ by 2.4×. A
filter's P does exactly that, by design (§1.1).

"The shape" is the shape *distribution* a crystal config draws from, sync groups included, not
one drawn instance: six `face_distance` values drawn i.i.d. from one distribution keep the full
group, because rotating a draw relabels it into another equally likely draw. The single
derivation is `DeriveGeometricSymmetry` (`src/core/crystal.hpp`), carried by every `Crystal`
(`GeomSymmetry()`) and by the analysis list's reduce context; `LUMICE_GetCrystalSymmetry` exposes
it through the C API, where the filter editor reads it for its hints. Values are compared at the closed-form geometry's relative tolerance.

Face numbering convention:

| Group | Faces |
|-------|-------|
| Basal (top/bottom) | 1 (top), 2 (bottom) |
| Prism (side) | 3 – 8 (six faces, 60° apart) |
| Upper pyramid | 13 – 18 |
| Lower pyramid | 23 – 28 |

### 2b. Orientation Distribution Symmetry (ensemble — conditional)

A symmetry element of the shape relabels faces; two raypaths it maps onto each other carry the
same light only if the crystal **ensemble** holds the two labelings with equal weight — i.e. the
orientation distribution is invariant under the element. Each element moves a different part of
the orientation:

- **P (60° rotation about the c-axis)** is a shift of the **roll** angle. It holds iff roll's
  distribution is invariant under a 60° shift — in this engine, a uniform roll over a full turn.
  Azimuth does not enter: a Parry arc has a uniform azimuth and a locked roll, and lights 3-5
  while its 60° image 4-6 stays dark.
- **B (σh)** reverses the c-axis: the same physical crystal labeled upside down is orientation
  (zenith, azimuth) against (180° − zenith, azimuth + 180°). It holds iff the **zenith** is
  symmetric about 90° **and the azimuth** is invariant under a half turn (in practice: uniform over
  360°). Roll does not enter. A plate (zenith ≈ 0°) fails — face 1 is always the one facing up, and
  1-3 and 2-3 are nothing alike; a column (zenith ≈ 90°) passes, locked roll or not. A column with a
  non-uniform azimuth fails too: measured, 3-2 carries 1.98% and 3-1 none.
- **D (one vertical mirror)** needs azimuth uniform over 360° and a roll anchor on a multiple of
  30°, which selects the mirror (§4).

**The key insight**: as physical symmetries, P, B, and D are not properties of a single crystal.
They are properties of the *ensemble* of crystals described by the axis distribution. The analysis
list applies each only where its condition holds — a toggle set on an ensemble that does not admit
it has no effect there (fewer rows merge), never a wrong merge. A filter applies P and B whatever
the ensemble (§1.1); only D reads its condition in both meanings. The three conditions are core's
`detail::IsPApplicable` / `IsBApplicable` / `IsDApplicable` (`src/core/crystal.hpp`), exposed as
`LUMICE_IsPApplicable` / `LUMICE_IsBApplicable` / `LUMICE_IsDApplicable` — what the filter
editor's hints read.

An earlier version of this section tied P to a uniform azimuth and B to "a symmetric zenith
distribution (plate crystals)". Both were wrong as physical conditions.

---

## 3. P, B, D Toggle Semantics

### P — C6 Rotational Equivalence

Applies the six-fold rotational symmetry of the hexagonal prism about the c-axis. Under C6,
each prism face maps to the next (3→4→5→6→7→8→3), and the basal and pyramid faces rotate
correspondingly.

**Filter**: always applies — all six label rotations collapse to one.

**Analysis list — enabling condition**: roll is uniform over 360° (§2b) — true for random, plate
and column orientations, false for Parry and Lowitz, whose roll is locked. Azimuth plays no part.

**Effect**: the canonical raypath uses the smallest face permutation representative; six
rotationally equivalent paths collapse to one. In the analysis list, on a shape with only a three-
or two-fold axis only the rotations it has are used (three or two paths collapse), and on a shape
with no rotation P has no effect (§2a).

### B — Horizontal Mirror (σh)

Applies the horizontal mirror plane through the crystal's equator. Under σh:

- Basal faces: 1 ↔ 2
- Prism faces 3–8: unchanged (they straddle the mirror plane)
- Upper pyramid faces: 13 ↔ 23, 14 ↔ 24, 15 ↔ 25, 16 ↔ 26, 17 ↔ 27, 18 ↔ 28

**Filter**: always applies.

**Analysis list — enabling condition**: the zenith distribution is symmetric about 90° (neither
end of the c-axis is preferentially up) and the azimuth is uniform over 360° (§2b). True for
random, column and Parry orientations; false for plate and Lowitz, whose c-axis stays near
vertical.

**Effect**: paths entering through the top basal become equivalent to paths entering through
the bottom basal; upper-pyramid paths become equivalent to lower-pyramid paths. In the analysis
list only on a shape whose two halves match: a pyramid with different upper and lower heights or
wedge angles has no σh, and B has no effect there (§2a).

### D — Vertical Mirror (σv or σd)

Applies a single vertical mirror plane whose orientation is determined by the **roll mean**
of the axis distribution. Depending on the roll mean, the mirror is either a σv plane
(passing through two opposite prism face centers) or a σd plane (passing through two opposite
prism edges).

Under D, basal faces 1 and 2 are always fixed. Prism faces map according to the
σ-by-roll-mean formula (see §4). Pyramid faces follow the same prism mapping.

**Enabling condition**: see §4, in both meanings. The analysis list additionally needs the shape
itself to have the mirror §4 selects (§2a); a filter does not.

---

## 4. D Enabling Condition and σ Derivation

### Geometric Basis

In the Lumice rotation chain `R = Rz(az−π) · Ry(−zenith) · Rz(roll)`, the world-up
direction `(0,0,1)` maps to the crystal-frame vector `(cos roll, −sin roll, 0)`. This vector
rotates in the crystal XY plane as roll changes, sweeping through the prism faces in the
reverse C6 order: face 3 → 8 → 7 → 6 → 5 → 4 → 3 every 360° (or equivalently every 180°,
since faces 3 and 6 share the same mirror-plane axis).

When azimuth is uniform over 360°, the ensemble average is rotationally symmetric about the
vertical axis. If the roll distribution is also concentrated at a specific value (or
distributed symmetrically about it), the combined ensemble may respect a particular vertical
mirror plane determined by the roll mean.

### Enabling Condition

D is applicable when **both** of the following hold:

1. **Azimuth is uniform 360°**: `az_dist.type == Uniform && az_dist.std ≈ 360°`
2. **Roll mean is a multiple of 30°**: `roll_dist.mean mod 30° ≈ 0°`
   (tolerance ε ≈ 0.001°; applies regardless of roll distribution type)

When D is not applicable, the D toggle in the filter has no effect on the simulation even if
it is checked — the symmetry reduction is simply skipped.

### σ-by-Roll-Mean Formula

Let `mean` be the roll mean in degrees. Compute:

```
n = round(mean / 30°) mod 6       -- which 30° sector (0..5)
a = (6 − n) mod 6                 -- mirror parameter (0..5)
```

The D mirror maps each prism face as:

```
pri_idx     = face − 3            -- 0-based prism index (0..5)
pri_idx_new = (a − pri_idx) mod 6
face_new    = pri_idx_new + 3
```

Basal faces (1, 2) and the pyramid position indicator are unchanged; pyramid faces 13–18 and
23–28 follow the same prism mapping (shift face by ±10 as needed, preserve 1x/2x prefix).

### Reference Table

| n | roll mean (mod 180°) | a | Mirror type | Through | Prism fixed | Prism swaps |
|---|---|---|---|---|---|---|
| 0 | 0° | 0 | σv | face-3 axis | 3, 6 | 4↔8, 5↔7 |
| 1 | 30° | 5 | σd | edge 3–8 | — | 3↔8, 4↔7, 5↔6 |
| 2 | 60° | 4 | σv | face-8 axis | 5, 8 | 3↔7, 4↔6 |
| 3 | 90° | 3 | σd | edge 8–7 | — | 3↔6, 4↔5, 7↔8 |
| 4 | 120° | 2 | σv | face-7 axis | 4, 7 | 3↔5, 6↔8 |
| 5 | 150° | 1 | σd | edge 7–6 | — | 3↔4, 5↔8, 6↔7 |

Period is 180°: n=0 and n=6 (roll=0° and roll=180°) yield the same σv through the face-3
axis.

---

## 5. Typical Scenario Reference

Which elements are physical symmetries for each orientation family (the GUI's axis presets and two
more), on a shape that has all of D6h — what the analysis list merges under, and what the filter
editor's hints read. A filter's P and B apply on every row of this table (§1.1); its D follows the
D column.

| Orientation | Az | Zenith | Roll | P | B | D |
|---|---|---|---|---|---|---|
| Random | uniform 360° | uniform, full turn | uniform 360° | ✓ | ✓ | ✓ (D coincides with P here) |
| Plate (parhelia) | uniform 360° | Gauss ≈ 0° | uniform 360° | ✓ | ✗ (face 1 always up) | ✓ |
| Column (tangent arcs) | uniform 360° | Gauss ≈ 90° | uniform 360° | ✓ | ✓ | ✓ |
| Parry | uniform 360° | Gauss ≈ 90° | Gauss, mean 0° | ✗ (roll locked) | ✓ | ✓ (σv through face 3) |
| Lowitz | uniform 360° | Gauss ≈ 0°, wide | Gauss, mean 0° | ✗ | ✗ | ✓ |
| Odd-roll config | uniform 360° | Gauss ≈ 45° | Gauss, mean 15° | ✗ | ✗ | ✗ (15° not a multiple of 30°) |
| Any, non-uniform azimuth | Gauss | any | any | as roll decides | ✗ | ✗ |

Notes:
- When roll is `uniform 360°`, D adds nothing P does not already merge (the ensemble has full
  rotational symmetry); enabling D alongside P is harmless but redundant.
- A ✗ cell means the analysis list ignores the toggle for that crystal, not that it is an error to
  set it: the list simply keeps those rows apart. On a filter, a ✗ in the P or B column means the
  toggle merges paths that are not physically equivalent (the editor says so, §6).

---

## 6. GUI Behavior

**D checkbox**: always remains enabled (never greyed out). The checkbox controls whether the
simulator attempts to apply D symmetry reduction; the simulator internally checks the
enabling condition and skips D if it is not satisfied.

**Informational indicator**: when the current axis configuration does not satisfy D's enabling
condition (az not uniform 360°, or roll mean not a multiple of 30°), a transparent `(i)`
button appears to the right of the D checkbox. Hovering over it shows the tooltip:

> D applies when azimuth = uniform 360° and roll mean is a multiple of 30°.
> Current config does not meet this condition, so D has no effect.

This design follows a "weak hint" principle: the user retains control and is not blocked from
checking D, but is informed when the toggle has no practical effect.

The indicator asks the engine's own predicate rather than re-deriving the condition: the GUI calls
`LUMICE_IsDApplicable`, which forwards to the same `detail::IsDApplicableParams` the simulator uses.
It used to keep a private transcription instead, and the two had drifted to different float
tolerances (1e-3 against core's 1e-5) — so for an azimuth range 3.05e-5° short of a full turn, which
is what the sqrt-scaled Range slider once stored at its stop, the hint said D was live while the
engine had already dropped it. A hint that disagrees with the thing it describes is worse than no
hint, which is why this one has no copy of the rule to drift.

**P and B hints (filter / colour ref)**: a filter's P and B always act (§1.1), so their `(i)` button
says when acting merges paths that are not physically equivalent on this crystal, and what to do
instead: beside **P** when roll is not uniform over 360°, or when the shape repeats only every
120° / 180° (or not at all); beside **B** when the zenith is not symmetric about 90° or the azimuth
is not uniform, or when the upper and lower pyramid parts differ. Each tooltip ends with the exact
route: untick the element and write each wanted path as its own row. The reasons are the engine's
own (`LUMICE_IsPApplicable` / `LUMICE_IsBApplicable` for the axis, `LUMICE_GetCrystalSymmetry` for
the shape); when the axis already rules P out, the P hint gives that reason and not the shape's
rotation step.

**D shape hint**: when the axis passes the §4 condition but the shape lacks the mirror it selects,
the `(i)` beside **D** says D merges paths that are not physically equivalent, with the same route.

**Faces the shape never has**: a shape can leave a face with no area at all — `face_distance`
`[2, 1, 2, 1, 2, 1]` does that to faces 3, 5 and 7. A filter naming such a face still matches
through the faces its own P/B/D relabels it to: "3-5" with P matches 4-6, 6-8 and 8-4 there. Only
a face that neither exists nor maps onto an existing face under the filter's own bits matches
nothing, and that is reported rather than changed: loading a scene logs a warning per such face,
and the Edit Entry modal notes it under the filter row (`LUMICE_CouldFilterMatchFace`, core's
`CouldFilterMatchFace` over `CouldFaceExist`). The check is exact for fixed and uniform shape
scalars and stays quiet for any other distribution, where it cannot be sure.

**Analysis panel**: its toggles act scene-wide and show no per-crystal hints; each crystal is
reduced under its own shape and orientation (§1.1). Its hover text says so, to keep it apart from a
filter's P/B/D.

---

## 7. Out of Scope

The following are **not** covered by P, B, or D:

- **Complex filter symmetry**: filters of type `complex` compose multiple sub-filters;
  symmetry reduction across sub-filter boundaries is not implemented.
- **Entry/exit pairs with multiple values**: when an `entry_exit` filter lists multiple entry
  or exit faces, cross-face symmetry beyond what P/B/D express is not handled.

---

## See Also

- [Configuration Guide — filter section](configuration.md#filter)
- [Crystal Orientation Sampling](crystal-orientation-sampling.md)
- [Coordinate Convention](coordinate-convention.md)
