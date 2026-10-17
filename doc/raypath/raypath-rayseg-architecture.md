# RaySeg Architecture: Field Invariants, Chain Rules, and State Machine

This document specifies the run-time constraints of the `RaySeg` struct — field semantics,
initialization rules, state-machine transitions, and filter interface contracts. It
complements `doc/raypath/raypath-symmetry.md` (PBD symbolic semantics) and
`doc/raypath/filter-architecture.md` (filter routing design); see §5 for exact boundary.

**Target audience**: contributors working on the simulator core, filter subsystem,
multi-scatter propagation, or consumer pipeline.

---

## §1 RaySeg Field Semantics & Invariants

`RaySeg` is defined in `src/core/raypath.hpp`. Each field is listed below with its semantic role,
valid range, and sentinel values.

### Geometric Fields

| Field | Type | Semantics | Coordinate Space | Source |
|-------|------|-----------|-----------------|--------|
| `d_[3]` | `float[3]` | Ray direction (unit vector). | Crystal-local when `IsNormal()`; world-space after `CollectData` rotation for outgoing/TIR. | `RaySeg::d_` |
| `p_[3]` | `float[3]` | Ray **end** point (not start point). | Same coordinate-space rule as `d_`. | `RaySeg::p_` |
| `w_` | `float` | Ray weight (intensity fraction). Non-negative for live rays; `-1.0f` is the TIR/filter-termination sentinel. | — | `RaySeg::w_` |
| `from_face_` | `IdType` | Polygon-face index the segment originated from (parent's `to_face_`). `kInvalidId` for the first segment of a chain. | — | `RaySeg::from_face_` |
| `to_face_` | `IdType` | Polygon-face index this segment hit. `kInvalidId` means "no hit" — either outgoing candidate or TIR-stopped. | — | `RaySeg::to_face_` |

### Chain & Identity Fields

| Field | Type | Semantics | Source |
|-------|------|-----------|--------|
| `prev_ray_idx_` | `size_t` | Legacy bookkeeping slot. The current CPU path initializes roots to `kInfSize` and has no reader that reconstructs a parent chain from it. | `RaySeg::prev_ray_idx_` |
| `root_ray_idx_` | `size_t` | Index of this crystal trace's entry segment in `all_data`; reset at each MS-layer crystal entry, then copied to hit-loop children. | `RaySeg::root_ray_idx_` |
| `crystal_idx_` | `IdType` | Index into `SimData::crystals_[]`. Range: `[0, kMaxCrystalNum)` or `kInvalidId` (init sentinel). | `RaySeg::crystal_idx_` |
| `crystal_config_id_` | `IdType` | `Crystal::config_id_` — ties the segment to its configuration-level crystal identity. | `RaySeg::crystal_config_id_` |
| `crystal_rot_` | `Rotation` | Crystal-to-world rotation matrix. Sampled per root ray; propagated to all children. | `RaySeg::crystal_rot_` |

The current-crystal face sequence is **not a `RaySeg` field**. `RayBuffer` stores one parallel
`RaypathRecorder` in `recorders_` for each segment and exposes it through `RecorderAt(idx)`; fan-out,
buffer moves, and filter calls must carry the segment and recorder together. This split keeps
`sizeof(RaySeg) == 96` while allowing recorder overflow storage (§2).

### State Flags

| Field | Type | Semantics | Source |
|-------|------|-----------|--------|
| `is_continue_` | `bool` | Set when this outgoing candidate continues to the next MS layer. Only meaningful when `to_face_ == kInvalidId && w_ >= 0`. Default `false`. | `src/core/raypath.hpp` (`is_continue_`) |

**Note (2026-05-28)**: `is_filter_dropped_` and `is_prior_filter_failed_`
plus the anchor-lane machinery were removed. Filter-fail rays now terminate
immediately in `CollectData` by reusing the TIR sentinel (`w_ = -1.0f`), so no
cross-MS-layer "prior filter failed" flag needs to persist. See
`doc/raypath/filter-architecture.md §6` (removal rationale) and `§7` (current branch table) for
the authoritative post-removal semantics.

### N4 Construction-Time Invariants

Checked by `RaySeg::IsValidComplete()` in the two single-segment `RayBuffer::EmplaceBack`
overloads (debug-only via `assert`; definitions in `src/core/raypath.hpp` and
`src/config/sim_data.cpp`). The buffer-to-buffer batch overload copies previously validated slots
and does not repeat the assertion.

| ID | Invariant | Predicate |
|----|-----------|-----------|
| N4-1 | `to_face_` in valid polygon range | Omitted — requires external crystal context. |
| N4-2 | `w_` is non-negative or the TIR sentinel `-1.0f` | `IsValidW(w)` |
| N4-3 | `is_continue_` implies `to_face_ == kInvalidId` | `IsValidContinueFace(is_continue_, to_face_)` |
| N4-4 | `crystal_idx_` in `[0, kMaxCrystalNum)` or `kInvalidId` | `IsValidCrystalIdx(crystal_idx_)` |
| N4-5 | `d_` and `p_` components are finite (no NaN/Inf) | `IsValidVec3Finite(d_)`, `IsValidVec3Finite(p_)` |

---

## §2 Ray Chain Construction & Traversal

### all_data Layout

`all_data` (`RayBuffer`) is the flat append-only store for all segments across all crystals
and MS layers within one CPU simulation run. Segments are appended by `InitRayFirstMs`,
`InitRayOtherMs`, and the hit-loop batch moves in `src/core/simulator.cpp`:

```
all_data = [ init_segs_crystal_0 | init_segs_crystal_1 | ... |
             hit1_children_c0    | hit1_children_c1    | ... |
             hit2_children_c0    | ...                        ]
```

### `prev_ray_idx_` / `root_ray_idx_` Bookkeeping

- `InitRay_d_w_previdx` initializes first-layer `prev_ray_idx_` to `kInfSize`. The current tree walk,
  filter path, and consumers do not read it, and no current assignment turns hit-loop children into
  a parent-index linked list. It must not be used as one without restoring and testing that contract.
- `InitRay_other_info` assigns `root_ray_idx_` from the entry segment's forthcoming `all_data`
  position. `TraceRayBasicInfo` copies that value to reflect/refract children.
- `InitRayOtherMs` calls `InitRay_other_info` again, so the value names the root of the **current
  crystal trace**, not a root spanning the whole multi-scatter chain. Cross-layer analysis identity
  is carried separately by `RayBuffer::chain_ids_`.


### crystal_idx_ Chain-Aware Propagation

`crystal_idx_` is set once per crystal trace in `InitRay_other_info` and propagated to both child
segments in `TraceRayBasicInfo`. It identifies which `Crystal` instance in
`SimData::crystals_[]` the segment belongs to.

### from_face_ / to_face_ Split

The `from_face_` / `to_face_` pair implements a linked-list of polygon faces along the ray:

1. **Entry segment** (`InitRay_p_fid`): `from_face_ = kInvalidId`, `to_face_` = sampled
   polygon face.
2. **Hit-loop children** (`TraceRayBasicInfo`): each child's `from_face_` = parent's
   `to_face_` (the face just hit); `to_face_` = result of `Propagate` (next face hit, or
   `kInvalidId` for outgoing/TIR).

The source-face guard passed to `Propagate` uses the parent's `to_face_` via
`BufferWrapper` with `step=2` (shared by reflect+refract pair), NOT `from_face_`
directly.

### Parallel Raypath Recorder

- `InitRay_other_info` calls `RecorderClear(i)` and appends the entry face with
  `RecorderAppend(i, crystal.GetFn(to_face_))`.
- `TraceRayBasicInfo` copies each parent recorder to both child slots through
  `RayBuffer::RecorderFanOut`; overflow slots are duplicated into the destination buffer's arena.
- `FillRayOtherInfo` appends the next face for each child that hit one.
- `RayBuffer::EmplaceBack` overloads move the segment and its parallel recorder together across
  hit-loop and MS-layer buffers.
- Length is capped at `kMaxHits` (64). The first `RaypathRecorder::kInlineCap` (15) face bytes live
  inline; longer paths use `RayBuffer::overflow_arena_` rather than being silently truncated at the
  inline capacity. `RecorderDataPtr` / the arena pointer passed to a filter resolves that storage.

---

## §3 Segment State Machine

### State Derivation

Segment kind is derived from two primary fields (`to_face_`, `w_`) and one 1-bit
flag (`is_continue_`). The four states are **mutually exclusive and exhaustive**
(source: `RaySeg::IsTir/IsNormal/IsContinue/IsOutgoing` in `src/core/raypath.hpp`):

```
                        ┌─────────────────────────────────────┐
                        │          w_ < 0?                    │
                        │   YES ──► TIR (also filter-fail;    │
                        │            see filter-arch. §7)     │
                        │   NO  ──┐                           │
                        │         │                           │
                        │    to_face_ != kInvalidId?          │
                        │   YES ──► NORMAL (in-crystal)       │
                        │   NO  ──┐                           │
                        │         │  (outgoing candidate)     │
                        │    is_continue_?                    │
                        │   YES ──► CONTINUE (next MS)        │
                        │   NO  ──► OUTGOING (emit)           │
                        └─────────────────────────────────────┘
```

| State | Predicate | Meaning |
|-------|-----------|---------|
| TIR | `w_ < 0` | Total internal reflection **or** filter-fail (the latter reuses the TIR sentinel; see `filter-architecture.md §7`). Absorbed by the simulator; never emitted. |
| Normal | `to_face_ != kInvalidId && w_ >= 0` | Ray hit another face; continues tracing inside the crystal. |
| Outgoing | `to_face_ == kInvalidId && w_ >= 0 && !is_continue_` | Ray exits crystal, passes filter (or no filter), does not continue to next MS. Emitted to output. |
| Continue | `is_continue_` (implies `to_face_ == kInvalidId && w_ >= 0`) | Ray exits crystal and continues to next MS layer. |

### Transition Rules in CollectData

`CollectData` (`src/core/simulator.cpp`) is the sole state-assignment function.
For the authoritative post-scrum-237 branch table (filter-fail = `w_ = -1.0f`
sentinel, ray terminates immediately, no anchor lane, no cross-layer
"prior filter failed" flag), see `doc/raypath/filter-architecture.md §7`. That section is
the single source of truth for the Design A branch table; this document does not
duplicate it to avoid a second drift point.

**Pre-loop reset** (pool-reuse guard): `is_continue_` is explicitly reset to
`false` for every segment at the top of the `CollectData` loop.

**Coordinate rotation**: outgoing candidates have `d_` and `p_` rotated from crystal-local to
world space with `crystal_rot_.Apply` before the filter check. TIR segments are rotated in the
preceding TIR branch. Normal segments remain in crystal-local coordinates (`CollectData` in
`src/core/simulator.cpp`).

**Routing after state assignment**:
- `IsNormal()` → re-enters `buffer_data[0]` for the next hit iteration.
- `IsContinue()` → appended to `init_data[1]` for the next MS layer.
- `IsOutgoing()` → collected in the wavelength loop into `outgoing_*` vectors.
- `IsTir()` (including filter-fail) → stays in `all_data` but is not emitted.

### Reset Points Summary

| Field | Reset Location | Trigger |
|-------|---------------|---------|
| `is_continue_` | `CollectData` loop top | Every segment, every hit iteration |
| `is_continue_` | `InitRayOtherMs` step 1.4 | Pool-reuse guard for MS layer entry |
| Raypath recorder | `InitRay_other_info` | Each crystal initialization |

**Note (current layout)**: no cross-MS filter-failure state lives in `RaySeg` — a filter-fail sets
`w_ = -1.0f` and terminates. Cross-layer state that does exist today is kept in `RayBuffer` parallel
columns: `components_` carries the raypath-colour component mask, and `chain_ids_` carries the
analysis chain id. This preserves the `sizeof(RaySeg) == 96` hot-loop invariant.

---

## §4 Filter Interface Contract

### FilterSpec::Check Preconditions

`FilterSpec::Check(const RaySeg& ray, const RaypathRecorder& rec, const uint8_t* overflow_arena)`
is called inside `CollectData` for outgoing candidates. At the call:

1. **Coordinate space**: `ray.d_` and `ray.p_` have already been rotated to world space through
   `crystal_rot_.Apply()`.
2. **Raypath**: `rec` is `buffer_data[1].RecorderAt(idx)` and is fully populated for the current
   crystal. When `rec.HasOverflow()`, `overflow_arena` points at that buffer's recorder arena.
3. **Segment kind**: `to_face_ == kInvalidId && w_ >= 0` (outgoing candidate).

The segment and recorder are one logical input even though they occupy separate arrays. Calling a
raypath-dependent spec with a recorder from another slot violates the contract just as surely as
passing the wrong `RaySeg` did before the storage split.

### Inputs Consumed by Filter Implementations

| FilterSpec subclass | Inputs consumed | Source |
|--------------------|-----------------|--------|
| `NoneSpec` | None (always matches) | `src/core/filter_spec.cpp` — `NoneSpec::Match` |
| `RaypathSpec` | `rec` + optional overflow arena (`RaypathOrbit::Contains`) | `src/core/filter_spec.cpp` — `RaypathSpec::Match` |
| `EntryExitSpec` | recorder length and first/last face bytes | `src/core/filter_spec.cpp` — `EntryExitSpec::Match` |
| `DirectionSpec` | `ray.d_[3]` (world-space direction) | `src/core/filter_spec.cpp` — `DirectionSpec::Match` |
| `CrystalSpec` | `ray.crystal_config_id_` | `src/core/filter_spec.cpp` — `CrystalSpec::Match` |
| `ComplexSpec` | Delegates the same segment/recorder/arena tuple to sub-filters | `src/core/filter_spec.cpp` — `ComplexSpec::Match` |

### FilterSpec::Create

`FilterSpec::Create(config, crystal, axis_dist)` builds a spec from one scattering entry's filter
configuration plus its referenced crystal and orientation context (`src/core/filter_spec.cpp`). It
pre-computes the requested symmetry's canonical/orbit data and action. These are construction-time
computations; `Match()` is stateless and performs no per-ray mutation.


---

## §5 Boundary with raypath-symmetry.md

| Concern | This document | `doc/raypath/raypath-symmetry.md` |
|---------|--------------|--------------------------|
| RaySeg struct fields & invariants | §1 — full field table, sentinel values, N4 checks | Not covered |
| Ray chain bookkeeping (`root_ray_idx_`; `prev_ray_idx_` is currently legacy/unread) | §2 — current assignments and scope | Not covered |
| Segment state machine (TIR/Normal/Outgoing/Continue/FilterDropped) | §3 — derivation rules, CollectData branch table, reset points | Not covered |
| Filter interface contract | §4 — preconditions, consumed fields per FilterSpec | Not covered |
| P, B, D symmetry toggles | Not covered (see cross-ref below) | §3 — full toggle semantics, enabling conditions |
| Face-number encoding (fn) | §2 (briefly, as parallel recorder input) | §2a — face numbering convention table |
| Canonical form / orbit reduction | Not covered | §3-§4 — P/B/D reduction formulas |
| GUI filter behavior | Not covered | §6 — D checkbox, informational indicator |

### Cross-References

- Raypath face-number encoding and PBD symmetry: `doc/raypath/raypath-symmetry.md`
- Filter routing design (Design A), post-anchor-lane-removal branch table: `doc/raypath/filter-architecture.md §2/§7`
- Filter JSON configuration schema: `doc/configuration.md`
- Crystal orientation sampling: `doc/geometry/crystal-orientation-sampling.md`

---

## Appendix: Key Constants

| Constant | Value | Defined in | Usage |
|----------|-------|-----------|-------|
| `kInvalidId` | `0xffff` | `src/core/def.hpp` | "No face" / "no crystal" sentinel for `IdType` fields |
| `kInfSize` | `SIZE_MAX` | `src/core/def.hpp` | Legacy/unset sentinel for `prev_ray_idx_` |
| `kMaxHits` | `64` | `src/core/def.hpp` | Maximum recorded face count per crystal |
| `kMaxCrystalNum` | `16` | `src/core/def.hpp` | Upper bound for `crystal_idx_` (N4-4) |
| `kSmallBatchRayNum` | `32` | `src/core/simulator.hpp` — `Simulator::kSmallBatchRayNum` | Default CPU inner-loop batch size |
