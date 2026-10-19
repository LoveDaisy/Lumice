# AGENTS.md — src/raypath/

Module surface rules for the raypath analysis module. The field reference and
schema authority is `doc/raypath/raypath-cli-output.md`; the layered
(schema3) design record is `doc/raypath/raypath-schema3-layered-design.md`.

- The module's surface to the engine is exactly the three public headers
  `path_feature_report.hpp` / `single_path_analysis.hpp` /
  `single_path_json.hpp` — the bridge `src/server/c_api_raypath.cpp` compiles
  exactly these and nothing else from this directory.
- Everything under `src/raypath/detail/` is internal implementation:
  includable only by the module itself and its own tests (the boundary and
  its rationale: `doc/raypath/raypath-cli-output.md` §8).
- Computation and JSON serialization live engine-side, in this module. The
  `Lumice raypath` CLI and a future GUI consumer read the same
  `raypath-cli-output.md` field contract — do not write a second serializer.
- Target schema1 / report schema3 live side by side in that document —
  schema3 is a target-free report and a v2 reader must refuse to read one
  (`schema_version` gate; §7.7 there holds the v2→v3 migration mapping).
  Fields are append-only.
