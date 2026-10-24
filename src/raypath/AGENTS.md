# AGENTS.md — src/raypath/

Module surface rules for the raypath analysis module. The field reference and
schema authority is `doc/raypath/raypath-cli-output.md`; the layered
(schema3) design record is `doc/raypath/raypath-schema3-layered-design.md`.

- The bridge consumes `path_feature_report.hpp`, `survey_report.hpp`,
  `single_path_analysis.hpp` / `single_path_json.hpp`, and
  `probe_analysis.hpp` / `probe_json.hpp`. Shared input values live in the narrow
  `domain/error.hpp`, `domain/input_metadata.hpp`, `domain/scope.hpp` and
  `domain/measure.hpp` leaves, not in a query result header.
- `survey_reuse.hpp` owns warm candidate admission and per-fact/profile qualification;
  `survey_context.hpp` owns the shared context value. Both JSON decoding and typed
  `HarvestSurveyDocument` produce an untrusted `WarmCandidate`. Only
  `ValidateWarmCandidate` constructs an `AdmittedWarmSource`; the point/probe
  entries and their inline assemblies use it before consuming facts. A fingerprint
  admits identity, not numerical validity. The bridge only decodes and maps errors.
- Input scope preparation lives in `detail/input_scope.*`, with physical member
  expansion still owned by `detail/physical_member_scope.*`. Actual family-measure
  reading lives in `detail/measure_scope.*`; the ideal freezing ladder stays in
  `detail/survey/survey_family.*`. Do not merge actual weights with ideal rank inputs.
- The bridge still includes `detail/survey/survey_mc_ess.hpp` for its budget bound;
  the probe header still exposes the shared line-probe result types. These are
  existing surfaces, not a claim that the whole public include closure is detail-free.
- Everything under `src/raypath/detail/` is internal implementation:
  includable only by the module itself and its own tests (the boundary and
  its rationale: `doc/raypath/raypath-cli-output.md` §8).
- Computation and JSON serialization live engine-side, in this module. The
  `Lumice raypath` CLI and a future GUI consumer read the same
  `raypath-cli-output.md` field contract — do not write a second serializer.
  The survey context block's one JSON spelling lives in `single_path_json.hpp`
  (`SurveyContextJson`); the point and the probe documents both write through it.
- Target schema1 / report schema3 / probe schema5 live side by side in that
  document — schema3 is a target-free report and a v2 reader must refuse to read
  one (`schema_version` gate; §7.7 there holds the v2→v3 migration mapping).
  Fields are append-only.
