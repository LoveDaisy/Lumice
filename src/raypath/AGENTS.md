# AGENTS.md — src/raypath/

Module surface rules for the raypath analysis module. The field reference and
schema authority is `doc/raypath/raypath-cli-output.md`; the layered
(schema3) design record is `doc/raypath/raypath-schema3-layered-design.md`.

- The module's public headers — the ones the bridge `src/server/c_api_raypath.cpp`
  compiles from this directory — are `path_feature_report.hpp` /
  `single_path_analysis.hpp` / `single_path_json.hpp` (the point query) and
  `probe_analysis.hpp` / `probe_json.hpp` (the probe query, the L2 draw-a-line
  face), plus `survey_report.hpp` (the survey's serialization entry, whose
  request/analysis types the bridge uses). The two query faces also consume the
  shared line-probe core and the survey context through
  `detail/survey/survey_probe.hpp` / `detail/survey/survey_document.hpp`
  (detail/ is internal to this module; the bridge compiling a detail header is a
  fact of the survey's assembly types living there, not an invitation).
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
