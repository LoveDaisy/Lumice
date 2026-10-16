[中文版](README_zh.md)

# Documentation Index

Lumice documentation is organized by audience and topic. Start with the user manual for product use, the architecture and developer guides for contribution, or the topic collections for subsystem details.

## Start Here

### Users

- [User manual](user-manual/00-overview.md): installation, GUI and CLI quickstarts, recipes, FAQ, raypath analysis, and configuration summaries
- [Configuration reference](configuration.md): JSON configuration fields, defaults, and validation
- [GUI guide](gui/gui-guide.md): application workflow, previews, and `.lmc` project files
- [C API reference](interfaces/c_api.md): engine API usage, errors, ownership, and integration

### Contributors

- [System architecture](architecture.md): modules, dependencies, and data flow
- [Developer guide](developer-guide.md): setup, coding conventions, testing, and debugging
- [Testing architecture](testing/testing-architecture.md): test layers, ownership, scope, and comparison rulers
- [Environment-variable policy](env-var-policy.md): decision gate and ownership rules for environment knobs
- [Contributing guide](../CONTRIBUTING.md): branches, pull requests, releases, and contributor workflow

## Topic Collections

### Geometry

Coordinate conventions, crystal representation, orientation sampling, numerical predicates, and geometry-randomization behavior:

- [Coordinate convention](geometry/coordinate-convention.md) ([中文](geometry/coordinate-convention_zh.md))
- [Crystal geometry representation](geometry/crystal-geometry-representation.md)
- [Crystal orientation sampling](geometry/crystal-orientation-sampling.md) ([中文](geometry/crystal-orientation-sampling_zh.md))
- [Geometry randomization performance](geometry/geometry-randomization-perf.md)
- [Geometry randomization value and measurement](geometry/geometry-randomization-value-and-measurement.md)
- [Near-pole area-measure sampling](geometry/near-pole-area-measure-sampling.md)
- [Numerical robustness](geometry/numerical-robustness.md)

### GUI

GUI usage, state ownership, lifecycle, layout history, and visual language:

- [GUI guide](gui/gui-guide.md) ([中文](gui/gui-guide_zh.md))
- [Custom spectrum and raypath color](gui/gui-custom-spectrum-and-raypath-color.md)
- [GUI preview lifecycle architecture](gui/gui-preview-lifecycle-architecture.md)
- [GUI state governance](gui/gui-state-governance.md)
- [GUI visual language](gui/gui-visual-language.md)
- [GUI layout architecture and disposition](gui/gui-layout-architecture.md)

### Raypath

Raypath representation, filtering and symmetry, analysis products, report schemas, and diagnostic research:

- [Raypath and ray-segment architecture](raypath/raypath-rayseg-architecture.md)
- [Filter architecture](raypath/filter-architecture.md) ([中文](raypath/filter-architecture_zh.md))
- [Raypath symmetry](raypath/raypath-symmetry.md) ([中文](raypath/raypath-symmetry.zh.md))
- [Raypath analysis](raypath/raypath-analysis.md)
- [Raypath analysis panel](raypath/raypath-analysis-panel.md)
- [Raypath CLI output](raypath/raypath-cli-output.md)
- [Schema 3 layered design](raypath/raypath-schema3-layered-design.md)
- [Feature-diagnostic acceptance](raypath/raypath-feature-diagnostic-acceptance.md)
- [Feature-diagnostic discovery research](raypath/feature-diagnostic-discovery-research.md)

### Rendering

Accumulation, exposure, projection overlays, and screen/print display models:

- [Accumulator–consumer architecture](rendering/accumulator-consumer-architecture.md)
- [Adaptive brightness](rendering/adaptive-brightness.md) ([中文](rendering/adaptive-brightness.zh.md))
- [EV pipeline architecture](rendering/ev-pipeline-architecture.md)
- [Overlay label placement](rendering/overlay-label-placement.md)
- [Print-mode subtractive ink](rendering/print-mode-subtractive-ink.md)

### Backends

The host/device seam and the verified GPU execution architecture:

- [TraceBackend seam design](backends/seam-design.md)
- [Trace-backend frame lifecycle](backends/trace-backend-frame-lifecycle.md)
- [GPU single-engine implementation](backends/gpu-single-engine-implementation.md)
- [GPU route history](backends/gpu-route-history.md)

### Testing

Test organization, performance measurement, reference machines, and remote verification workflows:

- [Testing architecture](testing/testing-architecture.md) ([中文](testing/testing-architecture_zh.md))
- [Performance testing](testing/performance-testing.md) ([中文](testing/performance-testing_zh.md))
- [Verification machines](testing/machines.md)
- [Remote CUDA build and testing](testing/gpu-remote-cuda-build-testing.md)
- [Windows remote testing](testing/windows-remote-testing.md) ([中文](testing/windows-remote-testing_zh.md))
- [XYZ statistics tool](testing/xyz-stats-tool.md)

### Interfaces

Published C surfaces, lifecycle rules, package boundaries, and product-line decisions:

- [C API reference](interfaces/c_api.md) ([中文](interfaces/c_api_zh.md))
- [C API lifecycle architecture](interfaces/capi-lifecycle-architecture.md)
- [Analytic API](interfaces/analytic-api.md)
- [API layering and product lines](interfaces/api-layering-and-product-lines.md)

## Other Collections

- [`user-manual/`](user-manual/00-overview.md): task-oriented product documentation
- `research/`: research records that retain their original status and scope
  - [Inverse-rendering research](research/inverse-rendering.md) ([中文](research/inverse-rendering_zh.md))
  - [Beam-tracing research](research/beam-tracing.md) ([中文](research/beam-tracing_zh.md))
- [`cases/`](cases/raypath-analysis-cases.md): registered raypath-analysis case corpus
- [`prototypes/`](prototypes/analyze-workspace.html): interactive design prototypes
- [`api/`](api/html/): Doxygen output; generate it locally with `doxygen .doxygen-config`
- [`figs/`](figs/): shared documentation images

## Root Documents

The `doc/` root is reserved for entry points and cross-topic references:

- [Documentation index](README.md) ([中文](README_zh.md))
- [System architecture](architecture.md) ([中文](architecture_zh.md))
- [Configuration reference](configuration.md) ([中文](configuration_zh.md))
- [Developer guide](developer-guide.md) ([中文](developer-guide_zh.md))
- [Environment-variable policy](env-var-policy.md)

## Language and Generated Documentation

English is the default language. Documents with `_zh.md` or `.zh.md` suffixes are Chinese partners where one exists; not every specialist design record is bilingual. Source comments and generated API documentation are in English.

Generate the Doxygen documentation from the repository root:

```bash
doxygen .doxygen-config
```

The generated output remains under `doc/api/html/`.

## Project Links

- [Project README](../README.md)
- [Project README in Chinese](../README_zh.md)
- [Example configuration](../examples/config_example.json)
