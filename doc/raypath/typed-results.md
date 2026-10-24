# Typed raypath results

The query products are values, not execution workspaces. Public result leaves do not
retain configuration managers, requests, cancellation pointers, or deferred computations.
Mathematical kernels remain the authority over their formulas and refusal domains.

## Migration ledger

This ledger names the existing machine-data groups and their producers. A row is not a
claim that all of its migration is complete; the ownership column describes the current
state. The JSON field contract remains [raypath-cli-output.md](raypath-cli-output.md).
No wire evolution is authorized by a type move. Timing values are observations, not
bitwise compatibility targets.

| Producer / former owner | Machine-data groups | Value owner / consumers |
|---|---|---|
| `single_path_analysis.hpp` | metadata, outcome, components, incomplete, discovery, point_mass, reach, sun_grid, measure_registration | `point_result.hpp`; legacy execution envelope, point codec |
| `survey_context.hpp` | object, edge, band_sum, reference wavelength, source/degradation | context value; point/probe |
| `probe_analysis.hpp` | metadata, profile spec/samples/readings/dark regions, red_edge_width | `probe_result.hpp`, `results/probe_line.hpp`; probe query/codec |
| `detail/survey/survey_document.hpp` | request, scope, resolved members, routing, objects, unevaluated declarations, evidence | `survey_result.hpp`, `results/survey_records.hpp`; survey assembly/codec/reuse |
| family routing | actual measure, ladder, fixed/free dims, killed proposals, level0 refusal, tails, bands, origin | survey records; structure extraction pending |
| positional dispersion and diagnose | index pair, followed onsets, verdicts, thresholds, coverage | survey records; capability extraction pending |
| tint/TIR/MC ESS/witness | state, method, numbers, counts, wavelength, lattice, refusal | survey records; return-value service extraction pending |
| warm admission | fingerprint, resolved location, producer/scope receipts, seed poses/profile rows | `survey_reuse`; admission remains mandatory |
| geometry/support | partition, intervals, topology, onsets, constant curves per wavelength, family groups | shared capability extraction pending |
| report enumeration | object kind/member/slot, four states, u/sky position, thresholds, vocabulary | report completion migration pending |
| report input assembly | light, crystal, axes, shape, layers, spectrum, support dimensions, seed/options | report completion migration pending |
| feature discovery / MC | records, source tokens, measure, observation options, spectral verification, unfinished | report completion migration pending |
| attribution/ruling | annotations, unattributed, counts, ruling, outcome/basis | report completion migration pending |
| report serializer | coverage, budgets, timing, unsupported path | report completion migration pending |

## Execution compatibility

`SinglePathResult` temporarily extends the unique `PointResult` data value with the
legacy deferred-context inputs. `SurveyContextOf` / `EnsureSurveyContext` are execution
operations, not readers of `PointResult`. The workspace migration will replace this
envelope. The public point value itself contains no execution input.

The former survey document and its evidence header no longer own a second set of
fields. Warm candidates remain untrusted regardless of whether their source is JSON
or a typed survey; the resolved-location and per-fact admission gates still apply.
