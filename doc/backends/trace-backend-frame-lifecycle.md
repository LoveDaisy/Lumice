# TraceBackend Frame Lifecycle (as-built)

This document describes the **current** ray-frame lifecycle across the
`TraceBackend` seam — how ray directions move between the world frame and each
crystal's local frame as rays cross the seam and pass between multi-scattering
(MS) layers. The original world-return fixes landed in `4b2639486` and
`55cd8773f`; device-resident continuation replaced the host transit in
`a91cc5a53`, and CUDA later adopted the same lifecycle. The code is authoritative
where historical mechanism descriptions differ.

## 1. Why frames matter

The ray tracer batches rays per crystal: many rays sharing one crystal
orientation are traced together in that crystal's **local frame** (the geometry
is fixed; the rays are rotated into it). But a halo's angular structure (e.g. the
22° ring) is a **world-frame** property — each ray carries a random crystal
orientation, so the same local-frame exit direction maps to different world
directions per ray. Projecting exit rays without first returning them to world
space scatters the ring into a featureless band.

This is exactly the bug 253.1 fixed: the Metal kernel projected exit directions
in crystal-local space and never applied the per-ray rotation, so the production
render showed a horizontal green band instead of the 22° ring (CPU, which does
apply the rotation, rendered the ring correctly).

## 2. Invariant 6 — ray-frame lifecycle (the contract)

Codified in `src/core/backend/trace_backend.hpp` (design invariant 6):

- Rays **crossing the seam are always world-space**: `TraceLayer`'s input rays
  (host ingest or device continuation) and any ray that **leaves** a backend
  (projection, continuation export, or readback) are world-space.
- The **crystal-local frame is a backend-internal detail**. A backend may
  transform world→local on ingest and **must** transform local→world before a
  ray leaves it.
- **Per-ray crystal orientation never crosses the seam**: each backend samples
  and owns its rotations internally. Whether the root buffers were filled by a
  device kernel or by the explicit host-generation fallback is not observable
  above the backend.

This is not a type-system gate. The independent evidence is the CPU-vs-Metal
spatial parity pair in
`test/parity-cross-backend/backend/test_metal_trace_parity.cpp`, the Metal and
CUDA root-generation parity tests, and the closed-form CPU/Metal anchors under
`test/golden-analytic/backend/`.

## 3. CPU reference (the frame chain to mirror)

`src/core/simulator.cpp`:

- **Ingest (world→local)**: `InitRay_rot` samples a per-ray `crystal_rot_`;
  `InitRayFirstMs` initializes the first-layer direction and
  `InitRayOtherMs` applies `ApplyInverse(r.d_)` to a carried world-space
  direction before tracing the next crystal.
- **Exit / continuation (local→world)**: `CollectData` applies
  `r.crystal_rot_.Apply` to direction and position before filter, outgoing, or
  continuation handling.
- **Inter-layer transit**: only the direction and carried ray state survive a
  layer boundary. `InitRayOtherMs` samples a **new** `crystal_rot_`, rotates the
  world-space direction into that crystal's local frame, and calls
  `InitRay_p_fid` to resample the entry point and hit face. Position and face
  identity from the previous crystal are not reused.

## 4. GPU backends (as-built)

Metal and CUDA share the same frame lifecycle. Their root-generation and
continuation kernels use the sampling and rotation primitives in
`src/core/shared/pcg_shared.h`; each backend keeps its own native kernel and
buffer implementation.

### 4.1 Supply form — device generation by default, host fallback retained

The default production path generates one rotation matrix per ray on device:

- Metal `gen_root_kernel` and CUDA `gen_root_kernel` sample orientation, build
  the row-major crystal→world matrix, rotate the incident world direction into
  crystal-local coordinates, and write the matrix to `root_rot`.
- Each trace kernel loads that matrix for the ray. The matrix is backend-local
  state; the host-facing `RootRaySource` carries only the root count on the
  production path.
- `LUMICE_DISABLE_DEVICE_GEN=1` selects the explicit diagnostic fallback. That
  path runs `InitRayFirstMs` on the host and copies the same per-ray matrices
  into the root buffers. Metal's device-vs-host test and CUDA's host-generation
  parity test keep the fallback statistically aligned with the default path.
- Closed-form golden-ray tests may inject host roots with identity rotations;
  this is a test-only path, not the normal root supply.

Device generation removes a per-ray host upload from the default path without
changing the seam invariant: orientation stays owned below `TraceBackend` in
both forms.

### 4.2 Exit projection (local→world, in-kernel)

Both GPU trace kernels load the per-ray row-major crystal→world matrix and
apply `world = m · local` before handing the direction to the shared
`lm_proj::ProjectExitToPixel` projection. This is the exact frame analogue of
CPU `CollectData`'s `crystal_rot_.Apply`. Using the transpose here would apply
`ApplyInverse` a second time and reintroduce the mis-oriented ring.

Every one of the 11 forward projection types enters through
`src/core/shared/projection_shared.h`; frame return therefore precedes the same
projection implementation on Metal, CUDA, and the legacy CPU oracle. Each
emitted ray is projected into every renderer in the session inside the same
kernel tail.

### 4.3 Multi-MS inter-layer transit

Inter-layer transit is entirely device-side on both GPU backends. Metal names
its pass `transit_root_kernel`; CUDA's corresponding continuation kernel uses
the same shared PCG and rotation primitives. The former host
`CopyContSliceToRootBuf` path is deleted.

Two halves mirror the CPU chain (§3):

1. **World return at the emit gate.** When a trace kernel emits a ray for the
   next MS layer, it multiplies the local exit direction by that ray's
   crystal→world matrix before writing the continuation direction. The same
   write carries weight, lifetime wavelength index, and the per-ray component
   mask; it does not carry the previous crystal's entry point or face.

2. **Resample into the next crystal.** The next layer's transit pass samples a
   fresh orientation and a fresh geometry-pool member, rotates the world-space
   continuation direction into the new local frame, samples the new entry point
   and face with projected-area weighting, and copies the carried wavelength
   and component state into the new root buffers.

The host dispatches these passes and observes only aggregate counters. No
per-ray direction, rotation, entry point, or face crosses back to host between
MS layers. Single-MS and multi-MS therefore follow one rule: local coordinates
exist only while a ray is inside one backend-owned crystal traversal.

### 4.4 Historical async-rotation proposal

The original design proposed a producer/consumer pool to overlap host
orientation sampling with device tracing. It was never needed on the current
production path: first-layer and continuation orientations are generated on
device, while the host generator is an explicit diagnostic fallback. There is
no host rotation stream to overlap.

## 5. Parity harness methodology (how this is guarded)

`test/parity-cross-backend/backend/test_metal_trace_parity.cpp`:

- **Independent oracle, not a kernel mirror.** Structural correctness is checked
  against the real `CpuTraceBackend`, not only the in-test kernel-shaped oracle.
  The latter shares frame assumptions and is blind to the original class of bug.
- **Spatially-sensitive metric, not an energy sum.** The
  `MetalVsCpuSingleLayerSpatialStructure` and
  `MetalVsCpuMultiLayerSpatialStructure` cases compare per-pixel Y-channel
  structure. A whole-image energy total cannot distinguish a world-space ring
  from a crystal-local band because rotation preserves total energy.
- **Negative control.** The single-layer case also evaluates the deliberately
  local-frame form and requires its correlation to collapse, proving the ruler
  can see the defect it claims to guard.

The retained `TwoLayerExitStatsAndXyz` and
`MultiPopTwoLayerExitStatsAndXyz` cases keep only assertions their in-test
oracle can answer honestly; the world-transit proposition belongs to the
independent CPU comparison above. Root-generation parity and the CPU/Metal
golden anchors provide separate evidence for the two ends of the chain.

## 6. Brightness ratio (resolved, no fix needed)

A ~1.35× Metal/CPU brightness ratio observed before the frame correction was a
**broken-frame artifact**: ring and band pixel values were not comparable. The
corrected full-random ring scene measured `ΣY_metal / ΣY_cpu ≈ 1.00011`; no
brightness compensation was required. This is a historical measurement, not a
current cross-machine tolerance. Current energy and image agreement is guarded
by the backend parity suites rather than by that single number.

## 7. Public-API impact

No frame-convention detail crosses the public C API. Backend selection and
fallback are observable through the engine capability, and `src/server/` owns
the CPU-multiworker versus GPU-single-engine routing, but consumers never see a
rotation matrix or local-frame ray. The frame lifecycle remains internal to
`src/core/`.

## 8. Per-ray wavelength (DR-3, scrum-268.8)

**Decision**: wavelength is a **per-ray lifetime property** (not per-batch or per-
threadgroup). Rationale: per-threadgroup was the original plan (DR-1(b)), but was
rejected because multi-MS continuation compresses ray populations — a photon's
`tg_id` becomes unstable across layer hops, so per-tg wavelength assignment would
let a photon change refraction index mid-path.

**Mechanism** (as-built, scrum-268.8):
- A `WlPool` of `M` entries (`{n, spd_weight, cmf}`) is pre-sampled on the host
  once per session (CPU-side Sellmeier evaluation, zero device Sellmeier).
- `gen_root_kernel` assigns each root ray a `wl_idx` from the pool
  (`wl_idx = global_idx % M`).
- The trace kernel reads the pool entry via `wl_idx` for per-ray refraction index.
- On continuation emit the `wl_idx` is written to `cont_wl_idx_out[tid]`.
- `transit_root_kernel` copies `cont_wl_idx_in → root_wl_idx_out` (§4.3, step 2)
  so the wavelength tag is preserved across all MS layers.
- The consumer accumulates XYZ using the per-ray CMF weight from the pool entry.

`M` is resolved from `LUMICE_WL_POOL_SIZE` (env var, read once at `BeginSession`;
`WlPoolSize()` exposes it for tests). A static analysis bug that would have
produced a flat spectrum (weight=0) on the `cpu_backend` was caught by the
anti-silent-regression hard gate deployed in §2's `IsOutgoing` path.
