# AGENTS.md — src/raypath/

Module surface rules for the raypath analysis module. The field reference and
schema authority is `doc/raypath/raypath-cli-output.md`; the layered
(schema3) design record is `doc/raypath/raypath-schema3-layered-design.md`.

- The bridge consumes the four typed query entries through `single_path_analysis.hpp`,
  `probe_analysis.hpp`, `survey_report.hpp`, `path_feature_report.hpp`, and their codec
  declarations; it never includes `detail/` or `survey/` implementation headers. Shared
  input values live in narrow `domain/` leaves, not in a query result header.
- `survey_reuse.hpp` owns warm candidate admission and per-fact/profile qualification;
  `survey_context.hpp` owns the shared context value. Both JSON decoding and typed
  `HarvestSurveyResult` produce an untrusted `WarmCandidate`. Only
  `ValidateWarmCandidate` constructs an `AdmittedWarmSource`; the point/probe
  entries and their inline assemblies use it before consuming facts; public
  structure/context/profile decisions require the admission token. Resolved scene
  location is a separate identity gate because the routing fingerprint excludes it;
  location-unknown indexes degrade to inline context while validated seeds remain
  solver starts. A fingerprint admits identity, not numerical validity; the bridge
  maps requests and errors without deciding reuse.
- Input scope preparation lives in `detail/input_scope.*`, with physical member
  expansion still owned by `detail/physical_member_scope.*`. Actual family-measure
  reading lives in `detail/measure_scope.*`; the ideal freezing ladder lives in
  `detail/structure/family.*`. Do not merge actual weights with ideal rank inputs.
- The four public result headers (`point_result.hpp`, `probe_result.hpp`, `survey_result.hpp`,
  `feature_report_result.hpp`) own immutable typed snapshots with result-bound references.
  Their include closures contain no detail/config/JSON headers. Value leaves live in `results/`
  and `domain/`; shared numerical producers live in `detail/structure/` and `detail/quantity/`
  and do not write query documents. See `doc/raypath/typed-results.md` for the field ledger,
  claim/quantity contracts and retained compatibility adapters.
- Everything under `src/raypath/detail/` is internal implementation:
  includable only by the module itself and its own tests (the boundary and
  its rationale: `doc/raypath/raypath-cli-output.md` §8).
- The CLI's four analysis commands create a Workspace, import JSON warm candidates explicitly,
  query the typed results, and call the engine's single `ToJson` projection. A future GUI
  consumer may use typed C getters without JSON. `SurveyContextJson` has one JSON spelling
  declared in `detail/json_values.hpp`; point and probe both use it.
- Target schema1 / report schema3 / probe schema5 live side by side in that
  document — schema3 is a target-free report and a v2 reader must refuse to read
  one (`schema_version` gate; §7.7 there holds the v2→v3 migration mapping).
  Fields are append-only.
