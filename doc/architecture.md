[中文版](architecture_zh.md)

# System Architecture

This document describes Lumice's current high-level architecture. It intentionally avoids duplicating complete API declarations, configuration fields, and test matrices; those have dedicated authorities linked below.

## 1. System shape

Lumice has two application shells over one engine:

- `Lumice`, the command-line application;
- `LumiceGUI`, the interactive GUI.

Both use the public C capability headers under `src/include/`. The GUI does not include the engine's `core/` or `config/` headers directly. A committed scene enters the server, a render or analysis session produces batches, consumers reduce those batches into results, and callers acquire an immutable published frame.

```text
JSON / GUI document
        │
        ▼
public C capabilities (`lumice_*.h`)
        │
        ▼
scene commit ──► session orchestration ──► CPU workers or one GPU engine
                                              │
                                              ▼
                                           SimData
                                              │
                      ┌───────────────────────┼──────────────────────┐
                      ▼                       ▼                      ▼
                render / anchor          statistics       raypath histogram
                      └───────────────────────┼──────────────────────┘
                                              ▼
                                  immutable `ResultFrame`
                                              │
                                              ▼
                                    CLI / GUI / C client
```

The engine supports one committed project at a time. A project has one scene and may have multiple render entries.

## 2. Enforced source layers

`cmake/lumice_layers.cmake` is the single source of truth for engine layering. CMake uses it to build the foundation object set, and `scripts/check_policies.py` uses the same manifest to reject unowned files and upward includes.

From lowest to highest:

| Layer | Main responsibility |
|---|---|
| `foundation` | geometry, optics, buffers, shared host/device primitives, utility infrastructure, base API |
| `analytic` | published analytic kernels and their C bridge |
| `scene` | configuration model, scene construction, scene/editor capabilities |
| `view` | projection and annotation-view construction |
| `sim` | simulator, ray generation, CPU/Metal/CUDA trace backends |
| `render` | consumers, exposure, composition, result types |
| `engine` | server orchestration and engine capability |
| `raypath` | path-analysis and report assembly |
| `capi` | remaining public C bridges and package surface |

A source may include its own layer or a lower layer. Existing exceptions are enumerated in `cmake/lumice_layer_allowlist.txt`, which only shrinks.

The application shell is outside this manifest by design:

- `src/gui/` uses only the engine's `lumice_*.h` capability headers plus stateless helpers from `src/util/`;
- `src/launcher/` contains the Windows pre-engine loader;
- `src/main.cpp` is the CLI shell.

## 3. Runtime sessions

### 3.1 Scene commit

Configuration JSON is parsed into the scene model in `src/config/`. GUI documents are projected into the same public scene capability before commit. `ServerImpl` owns the committed scene, session epoch, worker groups, consumers, and published results.

A commit may reuse compatible engine state, but it starts a new render epoch. `LUMICE_ContinueRender` is the distinct append operation: it adds rays to the current compatible accumulation instead of resetting it.

### 3.2 CPU and GPU routes

`ResolveGpuRoute` is the single routing decision used when the server is sized.

- The CPU route uses a measured, platform-aware worker count. A caller may provide an explicit count; a fixed seed uses one worker by contract.
- Metal and CUDA use the GPU single-engine route: one simulator owns a large device dispatch rather than mirroring the CPU worker pipeline.
- Raypath-analysis sessions always trace on CPU. A GPU-sized server keeps a standing CPU analysis pool; a CPU-sized server reuses its ordinary worker group.

The backend seam is `TraceBackend`. Host code owns scene/configuration semantics; backend implementations own device execution. See `backends/seam-design.md` and `backends/trace-backend-frame-lifecycle.md` for the detailed contract.

### 3.3 Batch production and consumption

Workers publish `SimData` batches through the server queues. The payload shape depends on the route: legacy outgoing rays, worker-projected sidecars, or device-fused planes. Consumers interpret those payloads through one server-owned session:

- `RenderConsumer`: per-render accumulation, projection, exposure, annotations, and optional color-class lanes;
- `AnchorConsumer`: the scene-wide relative-exposure anchor, once per render session;
- `StatsConsumer`: ray and geometry counters;
- `RaypathHistogramConsumer`: complete-path energy aggregation for analysis sessions.

A render session has one `RenderConsumer` per render entry, one session-wide anchor consumer, and statistics. An analysis session replaces render/anchor consumers with a histogram consumer plus statistics.

### 3.4 Published frames and completion

`DoSnapshot()` publishes a new immutable `ResultFrame`; it does not overwrite buffers already handed to a reader. C callers must acquire `LUMICE_ResultFrame*`, read any result families they need, then release that frame. This handle is the reader's real share of the result lifetime.

Simulation lifecycle and result drainage are separate facts:

- `LUMICE_GetSimLifecycle` reports whether production is running, stopping, or complete;
- `LUMICE_GetDrainStatus` reports whether every produced batch for the epoch has reached the consumers;
- `LUMICE_AcquireResultFrame` obtains a same-generation snapshot of render, raw XYZ, composite, statistics, or analysis data.

A final result requires both production completion and drain completion. The detailed C contract is in `interfaces/c_api.md` and `interfaces/capi-lifecycle-architecture.md`.

## 4. Geometry, rendering, and analysis

### Geometry and simulation

The crystal family is parameterized and bounded. Planes, stable face-number slots, and closed-form outlines are the primary representation; polygon/triangle forms are derived for consumers that need them. CPU and GPU routes share the same face numbering, optical primitives, projection conventions, and host-side scene semantics.

See `geometry/crystal-geometry-representation.md`, `geometry/coordinate-convention.md`, and `geometry/numerical-robustness.md` before changing geometry or predicates.

### Rendering and exposure

Render accumulation is linear XYZ. The relative mono exposure anchor is measured on a fixed full-sky equal-area domain; absolute exposure uses emitted energy. Display conversion, print mode, annotations, and color-class composition happen after accumulation according to the render configuration.

See `rendering/accumulator-consumer-architecture.md` and `rendering/ev-pipeline-architecture.md` for the authoritative formulas and ownership.

### Raypath products

There are two distinct products:

- `analyze`: a dedicated CPU Monte Carlo session that ranks complete raypath chains in a sky ROI;
- `raypath`: deterministic single-path fiber/feature analysis and JSON reporting implemented by `src/raypath/` over the analytic capability.

The raypath module exposes only its documented public headers to the engine; `src/raypath/detail/` remains internal. See `raypath/raypath-analysis.md` and `raypath/raypath-cli-output.md`.

## 5. Public surfaces and packages

There is no umbrella `lumice.h`. Engine capabilities are split across:

- `lumice_base.h`
- `lumice_scene.h`
- `lumice_render.h`
- `lumice_editor.h`
- `lumice_engine.h`
- `lumice_raypath.h`

The analytic capability uses `lumice_analytic_core.h`; the standalone analytic library adds `lumice_analytic.h` for library management.

Shared builds produce three distinct libraries:

- `liblumice`: product engine capabilities;
- `liblumice_testapi`: the same engine objects plus test-only hooks;
- `liblumice_analytic`: foundation plus analytic kernel only, on supported non-CUDA shared builds.

Export lists are generated from each package's headers and verified against built binaries. API versions for the engine and analytic library are independent. See `interfaces/api-layering-and-product-lines.md` and `interfaces/analytic-api.md`.

## 6. GUI boundary

`src/gui/` is an application shell, not a second engine implementation. It:

- owns interactive document and display state;
- builds/commits scenes through the public C API;
- polls lifecycle, drain, and acquired result frames;
- reprojects the engine's full-sky preview texture for interactive lenses;
- persists `.lmc` documents as a binary container with JSON state and an optional preview payload;
- exports CLI-compatible configuration JSON through a separate projection of GUI state.

The simulation-commit projection and JSON-export projection intentionally differ where GUI display semantics require it. See `gui/gui-state-governance.md` and `gui/gui-preview-lifecycle-architecture.md`.

## 7. Build and verification boundaries

The principal targets are:

- `lumice` and `Lumice` for the engine library and CLI;
- `LumiceGUI` and `lumice_gui_obj` when `BUILD_GUI=ON`;
- unit, composition, parity, golden, GUI, E2E, performance, and regression suites when their build options are enabled.

Use `scripts/build.sh` and `scripts/test.sh` rather than reconstructing target combinations by hand. Test ownership and execution scope are specified in `testing/testing-architecture.md`; contributor commands and source-boundary rules are summarized in `developer-guide.md`.

## Related documentation

- [Configuration reference](configuration.md)
- [Developer guide](developer-guide.md)
- [C API reference](interfaces/c_api.md)
- [Backend seam design](backends/seam-design.md)
- [Accumulator–consumer architecture](rendering/accumulator-consumer-architecture.md)
- [GUI state governance](gui/gui-state-governance.md)
- [Testing architecture](testing/testing-architecture.md)
