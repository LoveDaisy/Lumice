[中文版](filter-architecture_zh.md)

# Filter Architecture

This document captures the core design constraints for Lumice's filter subsystem.
It is a **specification**: all implementation must align with the invariants stated here.
If an implementation detail conflicts with a statement in this document, fix the doc first
(when the doc is wrong) or fix the code (when the code drifts), but never silently diverge.

**Target audience**: contributors implementing features that touch filter routing, crystal
configuration, multi-scatter propagation, or the C API, as well as reviewers performing
code-review against Design A.

---

## §1 Binding Model: Filter Is Per Scattering Entry

A filter is bound to a **scattering entry**, alongside that entry's crystal reference. Crystal and
filter identities are two independent axes:

```
entry  ──► crystal_id
       └─► optional filter_id
```

### JSON / Configuration

`scene.scattering[].entries[].filter` is the semantic binding. `ParseScatteringInfo` resolves the
crystal and optional filter for each entry into one `ScatteringSetting`; two entries may reference
the same crystal while naming different filters, or while only one names a filter. The parser must
not collapse those bindings by crystal id.

### GUI Linked Entries

Two entries are **linked** if and only if they share both `crystal_id` and `filter_id`.
The GUI renders a link badge when ≥ 2 entries form such a pair.
A linked group is the **atomic share unit**: any edit to one member must be observable
on all members.

Edit propagation rules — automatic vs needs explicit propagation:

1. **Crystal content edit** (in-place overwrite at the shared pool slot): automatic.
   All entries sharing the `crystal_id` see the update via pool indirection.
2. **Filter content edit** (in-place overwrite at the shared pool slot): automatic.
   Same mechanism via shared `filter_id`.
3. **Filter add** (`entry.filter_id: None → Some N`): needs propagation.
   The new pool slot is bound only to the editing entry by default; linked siblings
   (previously at `(cid, None)` with this entry) must also have their `filter_id`
   flipped to N, otherwise the group decoheres.
4. **Filter remove** (`entry.filter_id: Some → None`): needs propagation.
   Linked siblings must also have `filter_id` cleared.

See the "Linked group invariants" block in `src/gui/gui_state.hpp` and the propagation owner
`PropagateFilterIdToLinked` in `src/gui/edit_modals.cpp`.

### Same Crystal, Different Filters

The core configuration permits two entries to reuse one crystal id while naming different filter
ids (or no filter on one entry). They share crystal content but not filter content and are not a GUI
linked group, because the pair differs. If both crystal and filter edits must be independent, use
the GUI's "Unlink" or "Duplicate" operation to fork private pool slots. "Link to…" does the
opposite: it adopts the target's `(crystal_id, filter_id)` pair atomically.

---

## §2 Filter Role in Simulation (Design A)

Filters **participate in simulation propagation**. They act as gates at every
ray-exit-crystal decision point inside the simulator.

### Decision Rule (per ray, per crystal exit)

```
ray exits crystal
    │
    ├─ filter check passes  ──►  ray is eligible for outgoing / MS propagation
    │
    └─ filter check fails   ──►  ray is DROPPED
                                 (neither emitted as outgoing
                                  nor forwarded to the next MS layer)
```

`FilterSpec::Check(ray)` is called once per outgoing-ray candidate, using the filter resolved on the
active scattering entry. A failing ray does not appear in any downstream buffer.

### Design Rationale

Placing the gate at the simulator level (rather than at the consumer / render level)
gives two benefits:

1. **Correctness**: filter selectivity applies at every layer in the multi-scatter chain,
   so the physically correct subset of rays propagates through the full crystal sequence.
2. **Performance**: high-selectivity filters prune the ray population early; the pruning
   effect multiplies across layers (see §3).

---

## §3 Multi-Scatter Semantics

In a multi-scatter (MS) configuration, each layer carries one `prob` and a list of scattering
entries. For a ray routed through entry *i*, the decision tuple is `(prob_layer, crystal_i,
filter_i)`:

```
Layer 0:  prob₀ × crystal₀ × filter₀
               │
         filter₀ pass only
               │
Layer 1:  prob₁ × crystal₁ × filter₁
               │
         filter₁ pass only
               │
         ...
```

Filter is evaluated at **each crystal exit** within the simulator. The active entry's filter at
layer N controls whether that ray may enter layer N+1. Rays that fail at any layer are dropped;
they do not propagate further and do not appear in the output buffers.

Higher filter selectivity → faster simulation: the surviving ray fraction compounds
multiplicatively as `∏ᵢ (pass_rateᵢ × probᵢ)` across layers.

---

## §4 Render Phase: No Filter Logic

The render / consumer phase performs **no filter operations**.

All rays received by `RenderConsumer` have already been filtered inside the simulator.
The render pass is pure projection and XYZ accumulation:

```
simulator output  →  RenderConsumer  →  projection  →  XYZ accumulate
                                         (no filter)
```

`RenderConfig` does not hold a filter field. The former `ms_filter_` field was removed when 219.4
restored Design A. Applying filters at the consumer level is a design violation.

### Empty-batch contract

Because the filter gate runs simulator-side and can legitimately reject **every** ray in a
given `Consume()` batch (see the §7 branch table — a low pass-rate filter's "this batch got
zero survivors" is a normal tail event, not a malformed state), `outgoing_d_` and
`outgoing_w_` may both be empty while `rays_` is non-empty. `RenderConsumer` **must tolerate
this**: with `outgoing_w_.size() == 0` the copy, projection, and accumulation loops are all
no-ops and `snapshot_intensity_` stays zero. The consumer may **not** assume that a non-empty
`rays_` implies non-empty `outgoing_*`; the only sizing invariant it may rely on is the
`outgoing_d_.size() == 3 * outgoing_w_.size()` parallel-array relationship documented in
`sim_data.hpp` (asserted by `RenderConsumer::Consume` in `src/server/render.cpp`).

> A stale assertion in that same spot used to encode "`rays_` non-empty ⟹ `outgoing_w_`
> non-empty", causing a Debug-only `SIGABRT` whenever a filtered batch happened to empty out.
> It was replaced with the size-consistency check above, because the emptied-batch state it
> tripped on is exactly what Design A allows.

---

## §5 Data Flow Invariants

### Config representation

Each `SceneConfig.ms_.setting_[]` is a `ScatteringSetting` that carries both the resolved
`crystal_` and that entry's resolved `filter_` (`src/config/proj_config.hpp`). Reusing a crystal id
in source JSON does not merge the settings or their filters.

The GUI keeps crystal and filter objects in separate pools. An `EntryCard` stores both ids; pool
content is shared only along the corresponding id, while the linked badge and add/remove
propagation use the complete `(crystal_id, filter_id)` pair (§1).

### Filter check entry point

Every outgoing-ray candidate in the simulator calls `FilterSpec::Check(ray)` once.
`FilterSpec::Create(config, crystal, axis_dist)` builds a spec from that entry's filter config and
its referenced crystal/axis context.

The call site is in `CollectData()` inside `src/core/simulator.cpp` — see §2 for the
decision rule.

### RenderConfig invariant

`RenderConfig` must not hold filter fields.  Filters are a simulation-side concern only.

### EntryExit filter expressiveness (task-ee-filter-uplift, 2026-06-01)

The `entry_exit` filter type accepts two orthogonal axes of constraint:

1. **Structural axis** — entry-face set × exit-face set, each side independently
   either a single face number, a list of faces (OR), or wildcard (any face).
2. **Length axis** — minimum and / or maximum bounds on the parallel `RaypathRecorder::size_`.

The core schema reflects this directly:

```cpp
// src/config/filter_config.hpp
struct EntryExitFilterParam {
  std::optional<IdType> entry_;     // nullopt = wildcard
  std::optional<IdType> exit_;      // nullopt = wildcard
  size_t min_len_ = 1;              // path-length lower bound (>=1 by geometry)
  std::optional<size_t> max_len_;   // nullopt = no upper bound
};
```

Match semantics (`EntryExitSpec::Match`, `src/core/filter_spec.cpp`):

| Branch                  | Structural check                                                    |
|-------------------------|---------------------------------------------------------------------|
| both ends set           | orbit on `[entry, exit]` against the recorder's first/last face |
| only `entry_` set       | orbit on `[entry]` against the recorder's first face           |
| only `exit_` set        | orbit on `[exit]` against the recorder's last face              |
| both wildcard           | structural check skipped; length bounds alone decide            |

Length bounds are evaluated before the orbit check; `size == 0` always rejects before either
endpoint is read.

**Multi-value and composition routing** live in the GUI → scene translation layer. The GUI stores
a filter as sum-of-products rows: one row is an AND of raypath or EntryExit factors, and rows are
OR alternatives. An EntryExit factor uses the text grammar `entry:<faces> & exit:<faces> &
len:<bounds>`; comma-separated face lists are OR alternatives within that factor. Empty entry or
exit text is the wildcard.

`ExpandSopToClauses` (`src/gui/file_io.cpp`) expands each EntryExit face-list cartesian product and
each row's factor alternatives into simple core filters, then `ExpandFilterToScene` registers those
terms and, where needed, one `ComplexFilter` composition. This keeps the core
`EntryExitFilterParam` a pure single-entry/single-exit predicate while the GUI can express mixed
raypath/EntryExit AND terms and multiple OR rows. The editor is the row-based filter surface in
`src/gui/edit_modals.cpp`; `src/gui/raypath_segments.hpp` owns the small-domain grammar.

JSON schema is backward compatible: pre-uplift `{"entry": 3, "exit": 5}`
deserializes to `entry_=3, exit_=5, min_len_=1, max_len_=nullopt` with no
behavioral change.  Absent or explicit-null `entry`/`exit`/`max_len` map to
`nullopt`; absent `min_len` defaults to 1.  The to-json side omits each field
when it carries the default value.

---

## §5.1 Device-Side Filter Match (Metal and CUDA)

> Before the device-match port (and as stated in `doc/backends/seam-design.md` §4.5 before its
> update), filter evaluation was assumed to require host-side crystal configuration and therefore
> could not move to a device. **The isolated device-match parity spike refuted this.**

**What changed first (Metal)**: the `ReduceBuffer` core of raypath filter matching was
ported to MSL and validated against the full host filter check:
- **1.44M per-ray checks, 0 mismatches** across all filter types (None, Direction,
  Crystal, Raypath/EntryExit, Complex).
- **No register pressure impact** (1.3–2.7 GB/s, well within the trace-kernel budget).

**Current shared route**: `src/core/device_filter_desc.hpp` is the host-side descriptor authority
for both GPU backends. Metal evaluates it through `DeviceFilterCheck` in
`src/core/metal/lumice_trace.metal`; CUDA evaluates the same fields through the shared matcher in
`src/core/shared/filter_shared.h`. CUDA's fused emit gates run the matcher on device, while final-
layer records drained on the host still run `FilterSpec::Check`; `test_cuda_filter_parity.py`
exercises both arms. CPU continues to use `FilterSpec::Check` directly.

**Decision breakdown** (per filter type):
| Filter type | Device match feasibility |
|-------------|-------------------------|
| `None` | trivial |
| `Direction` | trivial (solid-angle comparison) |
| `Crystal` | `ray_crystal_config_id == descriptor.crystal_id` |
| `Raypath` / `EntryExit` | `ReduceBuffer` + `GetFn` table + memcmp + length bound |
| `Complex` | boolean composition of the above |

The key insight: §4.5 of `doc/backends/seam-design.md` conflated orbit **construction**
(config-dependent, host-side, per-session once) with per-ray orbit **matching** (bounded integer
kernel, fully portable to a device). Only the match is hot-path; the orbit table is built once and
bound in each backend's device buffers.

**As-built (scrum-267)**: `DeviceFilterCheck` is inlined into the trace kernel's
per-ray emit gate (ms_mode==1 branch). Each ray that leaves a crystal is immediately
gated device-side:

```
emit gate (in trace kernel, per-ray leaving):
  1. device prob check (PCG)   — continue vs outgoing vs drop
  2. DeviceFilterCheck(path[], wl_idx, orbit_table, …)  — filter-fail → drop
  3. write to cont_d_out / exit seam (surviving rays only)
```

This gate is **correct-by-construction** for the multi-MS +16% bug: the path
accumulator `path[]` lives in registers at the moment of gating, identical to
CPU `CollectData`'s parallel `RaypathRecorder` at the same decision point (`CollectData` in
`src/core/simulator.cpp`).

**Parity validation**: `test_metal_filter_match_parity.mm` —
`kFilterMatchTestKernelSrc` micro-kernel isolates `DeviceFilterCheck` against the
host `FilterSpec::Check` oracle. All filter types (including Raypath PBD/BD and
Complex multi-level) verified to 0 mismatch. See also `doc/testing/testing-architecture.md
§4.2` for the full parity battery methodology.

## §6 Historical Context

### task-200 (query-filter-uplift-v2, 2026-05-18) — Routing Decision: Reverted

**Goal**: move filter from simulator-side to consumer-side so that live-editing a filter
would not re-trigger a full simulation.

**Implementation**: `config_manager.cpp:194-225` (removed in task-revert-filter-to-simulator-side / 219.4)
added a post-parse auto-binding that copies `scattering.entries[].filter` into `renderer.ms_filter_`.  This activated the
previously-dormant `FilterRay` path in `render.cpp`.  In the simulator, the filter was
demoted to a pure MS-branch gate (filter-fail rays were still emitted as outgoing).

**Discovered problems** (scrum-prob-zero-leak / #219, 2026-05-22):

1. The intended benefit — "filter edit skips simulation" — was never realized in the GUI.
   Changing a filter still triggered `MarkFilterDirty → full sim re-run`.
2. Three implementation bugs were found in the task-200 code path:
   - Post-parse flattening lost per-entry filter binding for multi-level MS configs.
   - Crystal context was frozen at config-parse time, preventing live crystal updates
     from reaching the consumer-side filter.
   - Chain-walk boundary condition in the config binder caused index overflow for
     certain MS configurations.
3. Consumer-side filtering is less efficient: filter selectivity no longer prunes the ray
   population during simulation, so the multi-layer pruning benefit (§3) is lost.

**Decision**: revert to Design A (task-revert-filter-to-simulator-side, 219.4).

### scrum-210 (filter-architecture-refactor, 2026-05-19) — Retained

scrum-210 introduced `FilterSpec` polymorphism and the canonical-form / orbit-based
approach to raypath matching.  This is a **pure algorithm-layer improvement**, orthogonal
to the routing decision.  task-revert (219.4) migrates the filter *call sites* (consumer
→ simulator) but does not touch the `FilterSpec` algorithm interface.

Retained components: `src/core/filter_spec.{hpp,cpp}`, the per-batch `specs_table`
call in the simulator, and the scrum-210 test suite.

### scrum-prob-zero-leak / #219 (2026-05-22) — Design A baseline

This scrum audited the task-200 bugs, evaluated revert options, and adopted Design A
as the canonical routing model.  Off mode redesign was tracked as a backlog item.

### scrum-221 (adaptive-additivity-redesign, 2026-05-24) — Off mode redesign shipped

Implemented Off mode via F1 anchor lane.  ABI swap removed `unfiltered_*`
fields and introduced `anchor_p99_y` / `anchor_snapshot_intensity` (renamed to
`anchor_p995_y` in apply-new-defaults).  Additivity testing
(partition invariant) added to the E2E test suite.

### scrum-237 (remove-anchor-lane, 2026-05-28) — Anchor lane removal

Removed the F1 anchor lane in favor of per-frame visible-framebuffer self-P99.5
EV normalization. Filter-fail rays now terminate immediately in `CollectData`
(Design A semantics), eliminating the ~2× multi-scattering performance tax.
ABI break: `LUMICE_RawXyzResult` lost `anchor_p995_y` and `anchor_snapshot_intensity`.
The `test_partition_buffer_additivity` test was removed.

---

## §7 CollectData Branch Table (Design A)

`CollectData()` in `src/core/simulator.cpp` implements the following branch table for
outgoing candidates:

| filter | prob | outcome |
|--------|------|---------|
| pass | pass | `IsContinue()` — next MS scatter |
| pass | fail | `IsOutgoing()` — emit |
| fail | —    | ray terminates (`w_` set negative; not outgoing, not continue) |

Filter-fail rays carry no special flag — `w_ = -1.0f` (the TIR sentinel) is reused to
exclude them from `IsOutgoing()` and `IsContinue()`. They remain in `all_data` (with
TIR-like semantics) but never reach the consumer's outgoing accumulator.

### Current relative-exposure anchor

Filter termination changes which rays land, but it does not give the relative exposure path a
private metering rule. The current mono relative scale is scene-anchored:

```
scale = intensity_factor × TargetWhiteToLinear(135)
        / (axis_solid_angle × anchor_l99_sky)
```

`anchor_l99_sky` is the P99 sky radiance measured on the session's fixed full-sky equal-area
anchor plane. It is independent of lens, FOV, view, `visible`, and output resolution.
`axis_solid_angle` converts that radiance into the per-pixel integrated-radiance units held by this
renderer. `RenderConsumer::ExposureScale` owns the server/CLI formula; the GUI reads both fields
from `LUMICE_RawXyzResult` and reaches the same per-pixel result through `LUMICE_ComputeEvAuto`
plus the preview shader's relative-illumination factor. The two expressions deliberately look
different because the server buffer and GUI source texture use different units.

This is the **current** contract. The self-P99.5 rule in §6 is retained only as the dated state
introduced by the 2026-05-28 anchor-lane removal and later replaced. See `doc/rendering/ev-pipeline-architecture.md` §2.8
for the derivation and migration history.

---

## §8 Invariant Automation Status

The routing invariant is automated at each implemented seam:

- `test_simulator.cpp`'s `CollectDataFilterDispatch` cases pin all three CPU branch outcomes,
  including filter-fail termination even when the probability branch would continue.
- `test_render_consumer_empty_batch.cpp` pins the resulting consumer contract: a fully filtered
  batch is legal, but mismatched `outgoing_d_` / `outgoing_w_` arrays are not.
- `test_device_filter_check_host.cpp`, `test_metal_filter_match_parity.mm`, and
  `test_cuda_filter_parity.py` compare the device matcher and backend routing against the host
  semantics. `test_ms_filter_leak.py` is the full-stack regression sentinel that filter-fail rays
  do not leak into later MS layers.
- GUI tests pin atomic filter add/remove propagation for entries sharing a `(crystal_id,
  filter_id)` pair.

There is deliberately no loader assertion that all uses of one crystal id name the same filter:
that would reject the valid per-entry state defined in §1. The relevant obligations are instead
that each entry resolves its own filter, and that operations advertised as editing a linked pair
propagate filter-id changes to every member of that pair.

---

## Cross-References

| Concern | Location |
|---------|----------|
| Binding model, GUI linked-group invariants | `src/gui/gui_state.hpp` — `Linked group invariants`; `src/gui/edit_modals.cpp` — `PropagateFilterIdToLinked` |
| Per-entry crystal/filter config | `src/config/proj_config.hpp` — `ScatteringSetting` |
| Simulator-side filter check (Design A gate) | `src/core/simulator.cpp` — `CollectData()` |
| Empty-batch consumer contract (Design A) | `src/server/render.cpp` — `RenderConsumer::Consume()` |
| FilterSpec algorithm interface | `src/core/filter_spec.hpp` |
| Relative-exposure result fields | `src/include/lumice_engine.h` — `LUMICE_RawXyzResult.anchor_l99_sky` / `axis_solid_angle` |
| EV pipeline (scene-sky relative anchor) | `doc/rendering/ev-pipeline-architecture.md` |
| Filter JSON schema | `doc/configuration.md` |
| Raypath semantics, P/B/D filter toggles | `doc/raypath/raypath-symmetry.md` |
| Adaptive Brightness Off mode, additivity | `doc/rendering/adaptive-brightness.md` |
