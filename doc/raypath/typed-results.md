# Typed raypath results

The four query products are completed values, not execution workspaces. Their public
headers are `point_result.hpp`, `probe_result.hpp`, `survey_result.hpp`, and
`feature_report_result.hpp` under `src/raypath/`. Each owns immutable data through
`FrozenValue<T>`; `Data()` returns a const reference. No result retains a configuration
manager, request, cancellation pointer, sampler, or deferred computation.

The query entry headers remain separate from these value headers. The typed entries are
`AnalyzePathPointValue`, `AnalyzePathProbe`, `AnalyzePathSurveyValue`, and
`AnalyzePathFeatureReportValue`. The point entry takes an explicit `include_context`;
false produces an unevaluated/not-requested context without running a survey. Its explicit
`FinalizePointResult` adapter is also available to internal callers already holding an
execution envelope. Reading or serializing a completed typed point does not materialize
anything.

## Ownership and references

A completion allocates one process-local identity. Copying a result preserves both that
identity and its shared const data. Moving preserves the identity on the destination.
Constructing a result from revised assembly data allocates a different identity, even
when the object count and ordinals are unchanged. The public data view cannot change an
object/member mapping. Mutable `PointData`, `ProbeData`, `SurveyData`, and
`FeatureReportData` are construction values, not additional computation pipelines.

`ObjectRef`, `MemberRef`, `RecordRef`, and `SourceRef` combine that identity with a local
ordinal. The concrete results create references only within their tables and reject
foreign identities or out-of-range references on resolution. Report annotation/matched
record/source/unattributed associations use those checked lookups. Buckets remain a
view of the same object order through the existing `BucketOf` rule, not separate tables.

A point/probe context owns its admitted structural projection as an immutable
`ContextStructure`. Its object and edge references belong to that projection, not to an
unrelated survey's ordinal. The projection retains member face sequences and their
location/selection semantics. Typed harvest and JSON import retain all supplied member
rows; numeric qualification still goes through `ValidateWarmCandidate` and the existing
per-fact gates. Disk integers never become process-local result identities. A missing or
incompatible warm index retains the existing degradation behavior.

## The completion boundary

A frozen root is produced only by its completion entry, and that entry is where every
cross-field invariant is enforced — not a per-field convenience check scattered across adapters.

- **Report.** `FeatureReportData::outcome` starts at `kNotCompleted`; the completion assembly is
  the only intended assigner of a final outcome (it is a public field, so this is a documented
  discipline, not a type barrier — the freeze validates the value's consistency instead).
  `FeatureReportResult` refuses to freeze the construction slot, and `PathFeatureReportV3ToJson`
  throws on a slot value. Coverage rows carry a machine role (`kBaseInput` / `kBoundedDiscovery` /
  `kDeclaredSkip`): the codec partitions by that role, so a renamed subject keeps its limitations
  and a row inserted at any position keeps the vocabulary ledger in place. Only the
  bounded-discovery row may carry limitations, the unsupported arm publishes exactly its one
  refusal row, and the outcome agrees with the bounded row's status (`kCompleted` ↔ supported,
  `kPartial` ↔ numerical incomplete). The freeze checks the tables the reader walks by ordinal:
  annotations and objects agree in length, every `matched_record`, `source_token`,
  `source_connected_feature` and unattributed `record_index` is in range, and each object's `u`
  preimage is a whole number of triples.
- **Point.** The completion marker is private to the value; `FinalizePointData` is the only
  function that sets it, and it also refuses a sun grid whose parallel arrays disagree in length
  (the encoder iterates the deviation column and reads the other two by index). The data
  constructor refuses the unmarked slot and `ToJson` throws on it, so a default-constructed point
  cannot be published as `kDiscovered`.
- **Survey.** The warm index is one row per held pose (`WarmSeed`: pose and the leg that held it),
  so the serializer cannot walk a parallel `seed_sources` column past its end.
  The `SurveyResult` constructor *is* the completion boundary: `FinalizeSurveyData`
  binds every claim's declared scope from the document's own facts (members, geometry, observation,
  quantity spec), and `Validated` then rejects inconsistent object adjuncts, a bright domain whose
  `probe_index` names no probe, an object member outside the class, and a successful probe claim
  without a quantity scope. Product adapters hand over data and keep no second binding rule, so
  every construction path — production, warm decode, tests — yields the same scopes.
- **Probe.** The claim is private: a writable reading exists only as the return of `BeginFinite`, a
  certified arm only through `BeginCertified`, and a refusal's mode is set together with its arm by
  `Refuse`. The completion marker is private and set only by `FinalizeProbeData`; the data
  constructor refuses the unmarked slot and `ProbeToJson` throws on it. The freeze rejects a
  successful profile whose scope names another wavelength or representative than its own
  declaration, and a red-edge width whose two edge readings are absent.
- **Report object records.** The unassessed chromatic arm is read-only: `Chromatic()` returns the
  declared defaults, and filling a verdict requires the explicit `SetChromatic` transition
  (`MutableChromatic()` throws before it).

## Symbol placement recorded, not hidden

Several placements lag their file location and are listed here so nobody takes them for
internals: `results/report_structure.hpp` still lives in `lumice::raypath::schema3` although its
record types are part of the report completion value; the probe core in
`detail/quantity/probe_line.*` still lives in `lumice::raypath::survey` although it is a shared
capability beside `quantity::tint`/`tir_share`/`mc_ess`/`witness`; and `domain/evidence.hpp`,
`domain/measure.hpp`, `domain/object_kind.hpp` and `results/probe_line.hpp` remain in
`lumice::raypath::survey` for the same historical reason (the `survey::` prefix is therefore
visible in `ProbeResult` fields and `Claim`). All are renames deferred, not ownership statements.

## Tables a public serializer walks by index

The completion boundary must make every one of these safe before the value can freeze. This is
the census, checked against the four serializers and their typed readers:

| Table | Reader | Guard |
|---|---|---|
| report `core.objects` ↔ `annotations` | `PathFeatureReportV3ToJson` | equal length at freeze |
| report `coverage.declared_not_produced` ↔ reason | the vocabulary row | one row type (`DeclaredNotProduced`) — a mismatch is unrepresentable |
| report ordinals (`matched_record`, `source_token`, `source_connected_feature`, `unattributed.record_index`) | serializer + typed accessors | in range at freeze |
| report `object.u` | attribution / serializer | multiple of three at freeze |
| survey `warm_index.seeds` ↔ `seed_sources` | `SurveyResultToJson` | one row type (`WarmSeed`) — a mismatch is unrepresentable |
| point `sun_grid.{deviation_rad,valid,entry_measure}` | `SunGridJson` | equal length at `FinalizePointData` |
| survey `grid_spacing_deg[2]` | the ladder block | fixed-size array, no index risk |

Adding a reader that pairs two containers means adding its row here and giving it a guard (a
single row type where possible); the census is the checklist that replaces per-round discovery.

## Encoding cost and output-scale boundary

Serialization is a pure projection over a completed value: it starts no physical computation
(measured — a second serialization of the same result is byte-identical and produces no
context), and its CPU cost scales with the output document's size (the JSON text length),
not with the query's physical work. The encoding boundaries the execution contract declares:
the deadline and the cancel token govern the execution alone; an already-frozen result's
encoding runs to completion however large the document, with no interrupted half-document.
Budget accounting on the wire stays the observation it always was (timing blocks), never a
bitwise comparison target.

## Claims and independent states

`Claim<T>` has four exclusive arms: `Certified<T>`, `Finite<T>`, `Refused`, and
`Unevaluated`. The latter two contain no `T`. Default construction is unevaluated. A
finite zero is a successful numeric value, not a missing value. `ClaimedValue<T>` is the
single implementation of construction-time transitions, scope binding, evidence
projection, and success-only numeric access. A transition to refusal discards numbers
but retains the asked scope.

Survey objects select one of six payload kinds. Kind is derived from the variant rather
than stored beside independent optional payloads. Support, delta-edge, bright-domain,
and source-certificate setters reject an incompatible kind/state. Family band, origin and
sky position are cleared when kind or successful state changes; `SurveyResult` rejects
inconsistent construction values before freezing them. A family fold's representative
sky position is not a delta edge or an ordered curve; its delta payload remains absent.
Witness attachment is restricted to support objects, and its claim is independent of
the partition claim: a refused partition plus a finite positive witness is valid. A
witness never upgrades the family routing's structural-output verdict.

Point mass and discovered fibers are mutually exclusive point payloads. The discovered
arm owns incomplete components as well as completed ones. Probe profiles carry a spec
and a claim; refused profiles have no samples/readings, while a finite all-zero profile
keeps its bins. Survey probes likewise own their samples/readings only in a successful
claim arm; their default and refused arms carry no numeric payload. Completion binds
each successful survey probe's `ClaimScope` to its actual quantity, representative
member, resolved location and observation, rather than leaving scope only in the spec.
Probe direction, provenance, context evidence/source/anchor, and dark-run classification
have closed typed vocabularies.

Report existence, visibility, chromatic assessment, and corroboration remain separate
state machines. They are not converted to survey evidence. Unassessed chromatic data
has a distinct arm containing the schema's declared defaults, not an evaluated colour
claim. The previous walk-arclength, unknown/nonfinite, partial coverage, and
no-related-feature rules are unchanged.

## Capability owners

- `detail/structure/axis.*`: the sole `AssembleAxis`, its walk/partition/onset ordering,
  and contract translations formerly owned by the schema3 geometry adapter.
- `detail/structure/support.*`: `SupportRowOf`, constant-delta circles and their
  wavelength continuation, support equality, and family clustering.
- `detail/structure/class_support.*`: class support intervals, member rows and endpoint onsets as returned facts; `detail/survey/survey_t1.cpp` alone maps them to a support card.
- `detail/structure/family.*`: freeze ordering, rank scans, per-candidate survival,
  retained representatives and seed poses as `FamilySurvivor` facts. The survey adapter
  maps each survivor to a card, schedules quantity legs and mounts the returned structure.
  Level-zero refusal does not gate the ladder.
- `detail/quantity/probe_line.*`: the common frame/layout/profile reader and one
  `EvaluateProbeLine` implementation for survey and explicit probes.
- `detail/quantity/{tint,tir_share,mc_ess,witness}.*`: capability-owned returned claims
  and value types in `results/quantity.hpp`. These services neither depend on survey records
  nor mutate query results. Survey adapters alone mount the values and decide block evidence.

The mathematical implementations were moved, not replaced with parallel kernels. The
C06 constant-TIR-circle data remains on the existing report support surface. This does
not introduce a survey circle product, Lowitz curve connectivity, new quadrature, or
new collision geometry.

## Quantity scope

`QuantitySpec` describes kind, unit, method, member aggregation, reference measure,
density weighting, source/spectral observation, shape basis, and regularization where
applicable. Its shared const `QuantityContext` owns the actual crystal distribution,
light spectrum/source parameters, resolved members, and observation metadata. A spec
selects the relevant part of those inputs; the context is not a universal cache key.
`GeometryKind` is the single nominal/realized/declared-population vocabulary, and
`BindQuantityScope` is the single quantity-to-claim projection of shape, observation
and measure, used both by services and by query mounting.

- Band-sum profiles are representative-member per-steradian values under the normalized
  Haar/PoseDensity protocol. `member_count` does not silently multiply stored samples.
  Each online wavelength retains its own observation scope.
- Point factors distinguish raw entry area from `A/a²`, transmission, normal Jacobian,
  regularized coarea factor, angle-coordinate `DeclaredPoseDensity`, factor product,
  fiber-arclength integral, and orbit integral. Mass is not labelled per steradian;
  the two density reference measures are not conflated.
- Tint has exclusive mass and class-power readings. Its typed mass protocol distinguishes
  psi-average, lattice mean, and single-pose direct evaluation. The actual method is
  derived from the reading and then recorded in its quantity spec. The fixed chromatic
  pair is not an integrated SPD/CMF colour measurement.
- Witness and class-power calculations carry no rho weighting; the witness is an
  existence sample, not a distribution estimate.
- TIR share retains the already accumulated numerator/denominator, per-interface
  weights, and excluded-chart counts, beside its ratios. Its scope remains the nominal
  representative and fixed pair, not full-spectrum or regional TIR.
- MC ESS retains sum of weights and sum of squared outer-draw weights, counts, seed,
  and band domain under the actual product sampler. It is sampling quality, not intensity.
- Structural family claims distinguish actual support from ideal-frozen levels; the
  input measure remains actual and the band records retain the frozen dimensions and
  survival ruler.

## Field-group migration ledger

The JSON contract remains [raypath-cli-output.md](raypath-cli-output.md). No normal-path
wire-field or numerical evolution is introduced by these moves. The optional, non-default
`scope.geometry_kind` also accepts `declared_population` to preserve conservative warm
round-trips of that scope; it never becomes a nominal structural reuse candidate. The default of a
hand-constructed, uncomputed typed declaration is now unevaluated; successful production
legs explicitly construct their evidence. Timing values are observations, not bitwise
comparison targets. Static wire prose and enum spelling remain codec responsibilities.

| Previous owner / producer | Existing machine-data groups | Unique value owner and consumers |
|---|---|---|
| `single_path_analysis.hpp` | meta/outcome/components/incomplete/discovery/point_mass/reach/sun_grid/measure_registration | `point_result.hpp`; point execution, typed completion and codec |
| legacy C `LUMICE_Analyze*` entries | Scene-taking request adapters and optional `warm_json` decode; no independent result model | retained as compatibility shims over the same typed completions; retirement target after callers use workspace queries and explicit WarmData import |
| query_control's unified deadline (`ResolveQueryDeadline`/`QueryDeadline`) | resolve-once absolute deadline — execution state rather than a wire key | the C workspace's four query entries resolve it once; stoppable legs consume its absolute stop and the typed result retains the observed outcome |
| deleted envelope (former `SinglePathResult` deferral) | context_config/context_request/context_warm/survey_context_ready — never wire keys (verified pre-deletion) | no owner: the deferred state is unrepresentable; a declared context is produced by `AnalyzePathPointValue` in the execution |
| context assembly/reuse | object/edge/band_sum, reference lambda, source and degradation | `survey_context.hpp` plus owned structural projection; point/probe and shared context codec |
| `probe_analysis.hpp`, survey probe | meta, profile spec/samples/readings/dark_regions, red_edge_width | `probe_result.hpp`, `results/probe_line.hpp`; quantity service, probe query and codec |
| former survey document | request/scope/resolved members, routing, objects, unevaluated/evidence | `survey_result.hpp`, `results/survey_records.hpp`; survey assembly, typed harvest and codec |
| survey family | measure/ladder/frozen/free dimensions/killed/level0 refusal/tails/band/origin | structure facts and survey records; survey adapter alone mounts cards without rerunning rank |
| survey chromatic legs | positional dispersion, followed onsets, diagnose/verdict/thresholds/coverage | survey records; existing chromatic producer and codec, with their original refusal shape |
| tint/TIR/ESS/witness | methods, values, counts, pair/lattice, coverage/refusal | claim-owned capability values in `results/quantity.hpp`; survey adapters mount them without reimplementation |
| warm admission | fingerprint, resolved location, producer/scope receipts, seeds and profile rows | `survey_reuse`; both import paths remain untrusted until admitted |
| geometry/support | member partition/intervals/topology/onsets/constant curves per lambda/family groups | `results/structure.hpp`; structure capability and report/survey consumers |
| enumeration | kind/member/slot/phi, independent states, u/sky position/counterfactual/thresholds/vocabulary | `results/report_structure.hpp`, `report_enumeration.hpp`; enumeration and completed report |
| report input | light/crystal/axis/shape/layers/spectrum/support_dimensions/seed/options | `results/report_scope.hpp`, pure scene values; captured before moving execution storage |
| discovery/MC | every record subfield, measure/source identities, observation ruler, spectral verification, unfinished | `results/report_records.hpp`, `report_attribution.hpp`; completed MC evidence has no sampler |
| report source replay | positive_source_witness and source-token rows | completion stage materializes `PositiveSourceWitness`; codec never calls `ReplayDiagnosticSource` |
| attribution/ruling | annotations/unattributed/counts/ruling/outcome/basis | completed report; checked local references and original independent ruling |
| former report serializer assembly | coverage/budgets/timing, unsupported multi-crystal path | `FeatureReportData` completion; serializer takes one `FeatureReportResult` plus generator text |

The maximal MC-record fixture and existing literal JSON key/null/absence pins retain
the `kMcMigrationLedger` as their reference. The tests also exercise C01 edge/probe
semantics, C10 independent ladder routing, C16 survival/representatives, C17 refused
support plus witness, C06 support circles, and the existing unavailable-density cases.
This is a value/ownership migration, not a new uniform test census over the full corpus.

## Remaining execution adapters

The legacy point execution envelope (`SinglePathResult` with its deferred-context inputs
`context_config`/`context_request`/`context_warm`/`survey_context_ready`, and the
`AnalyzeSinglePath`/`SurveyContextOf`/`EnsureSurveyContext`/`FinalizePointResult`
materialization family) is deleted: one typed entry, `AnalyzePathPointValue`, runs the
discovery legs and produces a declared context in the execution itself — a completed
`PointResult` owns its context block, and no reader (getter, serializer, repeated read)
can start physical computation. The C surface's `LUMICE_AnalyzeSinglePath` name remains the
published ABI, but its handle owns the completed `PointResult`; the report, survey and
probe handles likewise own their frozen results. Their `ToJson` entries derive and cache
an optional document from those values, and their typed C getters borrow immutable data
from the result handle rather than parsing the document. The new C workspace captures a
Scene at creation and runs the four typed queries through the same execution owners;
explicit typed warm data comes from a completed result or a legacy document import.
The MC draw-limit include remains an existing bridge-boundary exception.

`AssembleSurveyData` is construction-time staging used by query policy and focused
fixtures, not a second public result model or a second computation path. The old
`survey_document.hpp`, schema3 geometry/support owners, evidence header, and empty
structure-object forwarding header are removed. Mutable construction values cannot
change a previously published snapshot.
