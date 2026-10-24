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

- **Report.** `FeatureReportData::outcome` starts at `kNotCompleted`; only the completion
  assembly assigns the final outcome. `FeatureReportResult` refuses to freeze the construction
  slot, and `PathFeatureReportV3ToJson` throws on a slot value. Coverage rows carry a machine role
  (`kBaseInput` / `kBoundedDiscovery` / `kDeclaredSkip`): the codec partitions by that role, so a
  renamed subject keeps its limitations and a row inserted at any position keeps the vocabulary
  ledger in place. Only the bounded-discovery row may carry limitations.
- **Point.** `PointData::completion` starts at `kNotCompleted`; `FinalizePointResult` is the only
  entry that marks it. The data constructor refuses the slot and `ToJson` throws on it, so a
  default-constructed point cannot be published as `kDiscovered`.
- **Survey.** The `SurveyResult` constructor *is* the completion boundary: `FinalizeSurveyData`
  binds every claim's declared scope from the document's own facts (members, geometry, observation,
  quantity spec), and `Validated` then rejects inconsistent object adjuncts, a bright domain whose
  `probe_index` names no probe, an object member outside the class, and a successful probe claim
  without a quantity scope. Product adapters hand over data and keep no second binding rule, so
  every construction path — production, warm decode, tests — yields the same scopes.
- **Probe.** A writable reading exists only as the return of `BeginFinite`, and a refusal's mode is
  set together with its arm by `Refuse`; no accessor writes across arms. `ProbeResult` rejects a
  successful profile claim that carries no declared quantity scope.
- **Report object records.** The unassessed chromatic arm is read-only: `Chromatic()` returns the
  declared defaults, and filling a verdict requires the explicit `SetChromatic` transition
  (`MutableChromatic()` throws before it).

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

`SinglePathResult` is the temporary legacy execution envelope: one `PointData` plus the
old deferred-context inputs. `SurveyContextOf` and `EnsureSurveyContext` operate only on
that envelope. Workspace execution will replace them; the completed `PointResult`
contains neither. The legacy string-returning report/survey entry points are thin typed
assembly → codec adapters. The C bridge remains a legacy JSON handle consumer until its
typed-view migration; the MC draw-limit include is an existing bridge-boundary exception.

`AssembleSurveyData` is construction-time staging used by query policy and focused
fixtures, not a second public result model or a second computation path. The old
`survey_document.hpp`, schema3 geometry/support owners, evidence header, and empty
structure-object forwarding header are removed. Mutable construction values cannot
change a previously published snapshot.
