# Analyze presentation tests — retention adjudication (PR #628)

The 2026-11-22 CI run [45052003928](https://github.com/LoveDaisy/Lumice/actions/runs/45052003928)
(SHA `68164ef72`, PR #628) red twice on the Ubuntu x86_64 ctest phase budget — attempt 1
(job 137298959167) and attempt 2 (job 137306831520). Owner ruling: the budget is not raised;
every new test must state what it verifies and what quantity it guards, or it leaves the
branch; if the kept set still overruns, zero-sum deletion applies to lower-importance
incumbents. This document is the authoritative per-case adjudication produced under that
ruling; the disposition it records was applied on this branch in the same commit that added
the table.

## Cost scenarios (the gate's own evidence)

Both reds are the same SHA, two attempts, two runner speeds. Row-level data: the
`ctest-durations-ubuntu-x64` artifacts of each attempt; exact factors recomputed as the median
measured/calibrated ratio over the registry's 7-id ruler subset (the gate's own formula).

| Scenario | Run / attempt | Job | Phase seconds | Factor | Effective budget (360 × factor) | Normalized (÷ factor) |
|---|---|---|---|---|---|---|
| ATT1 | 45052003928 / 1 | 137298959167 | 532.73 | 1.414552 | 509.24 | 376.61 |
| ATT2 | 45052003928 / 2 | 137306831520 | 419.57 | 1.138371 | 409.81 | 368.57 |

Overrun: +16.6s (ATT1) and +8.6s (ATT2) normalized. The ctest phase is serial and
compute-dominated (the registry's Ubuntu x86_64 ctest entry), so removing a test removes its
row seconds from the phase additively; fixed overhead stays.

## Where the 21 new cases live

The 21 new gtest cases introduced by PR #628 (diff `a9b0149b1..68164ef72`) are registered in
two gated binaries: `composition_correctness_test` (suite `AnalyzeOverviewChain.`, 6 cases)
and `gui_unit_test` (15 cases across `AnalyzeOverviewModel`, `AnalyzeMapView`,
`AnalyzePointView`, `ReferenceArcDraw`). The PR's changes to
`test/gui/functional/test_raypath_analysis_panel.cpp` register no new cases in the gated
phase (that file is the gui_test leg, a different CI job).

## Per-case adjudication

CI seconds are per attempt ("ATT1 / ATT2"). "Replaces/kept by" names the cheapest existing
carrier that could hold the same assertion.

### The four heavy composition-chain cases (55.46s ATT2 / 75.70s ATT1)

| Case | Verifies | Guarded quantity | CI s | Disposition |
|---|---|---|---|---|
| `AnalyzeOverviewChain.FullDimensionalDispersionAnchorsToTheClosedForm` (pre-existing C01 anchor; listed for context) | The C01 3-5 document's dispersion block equals the closed-form slab minimum, restated from the engine's own table and certificate | Closed-form identity at both lambdas (1e-4° band) against the corpus's registered 21.612°/22.371° | 12.64 / 17.26 | kept (pre-existing) |
| `AnalyzeOverviewChain.OverlayFactsReadBackSupportEdgeAndTurns` | Real-document read-back of the map overlay facts: touching intervals merge to ONE reach range (not full-circle), the certified edge sits at the support's inner bound, `object_index` names the same card the qualified inner-edge object points at, TIR turns parse only from visible discriminants | Structural read-back facts on the C01 document: interval-merge cardinality, edge-at-bound (±0.05°), object-index identity, turns⊆visible-features biconditional | 12.42 / 17.23 | **deleted — absorbed into the C01 anchor** |
| `AnalyzeOverviewChain.FullDimensionalDocumentCarriesNoPositionedCards` | The full-dimensional C01 document carries no positioned object cards, so the map's points group is honestly empty for the class | Boolean absence over every `SurveyObjectRow` of the document | 12.45 / 17.26 | **deleted — absorbed into the C01 anchor** |
| `AnalyzeOverviewChain.ExtendedSunKeepsTheAngularDistancePresentation` | With a 2° solar disc, routing and cards are unchanged (the certified coincident card stays at 21.9161°) and the qualification flip is owned by the caller's point-sun premise, not by the document: premise false → unqualified + flat lines, premise true → qualifies | Disc-invisibility of the certified card (1e-6° on the same deviation) and the two-branch premise flip on a real document | 12.60 / 17.18 | kept — the only extended-sun corpus document; its fixture `test/e2e/configs/raypath_feature_random_regular_extended_sun.json` has no other consumer |
| `AnalyzeOverviewChain.ChromaticDocumentCarriesLabelsAndTurnsWithoutMinRow` | The real 3-1-5 document carries dispersion labels WITHOUT a minimum-deviation row and visible TIR turns on the same document — the witness shape where a min-row-gated caliber would wrongly report "wavelength unknown"; plus the qualified-object honesty branch (`dispersion_available` false with a named reason) | Metadata premise of the draw-side TIR caliber: labels present ∧ min row absent ∧ turns present on one real document | 17.99 / 24.03 | kept — the only real-document witness of this shape (the C01 3-5 anchor document HAS the min row, asserted true there); no synthetic or cheaper carrier produces it |

Absorption proof (no coverage reduction, no double-counted gain): the anchor
(`FullDimensionalDispersionAnchorsToTheClosedForm`) runs the identical request — same scene
config, same faces `{3, 5}`, same default budget — as the two absorbed cases. The absorbed
blocks assert against that one survey object; every assertion moved verbatim (the absorbed
case's premise is the anchor's own `point_sun_premise = true`). Savings are claimed once: the
two deleted rows' seconds leave the phase; the anchor's own row grows by microseconds of
typed-reader work. Failure granularity: the absorbed blocks sit under the anchor's name with
delimiting comments naming the two origins.

### The remaining 17 cases (≈2.0s ATT2 combined)

| Case | Verifies (guard) | CI s | Disposition |
|---|---|---|---|
| `AnalyzeOverviewChain.MaxFoldUpperEdgeDoesNotQualifyAsTheInnerEdgeObject` | The qualification gate's certificate leg: a finite, coincident, UNCERTIFIED edge card at the support's UPPER bound (the C09 counterexample, position pinned at 149.0849°) must not upgrade to the inner-edge object — no object, no "light begins", flat line stays | Refusal vocabulary of the gate: card shape (coincident ∧ ¬certified), upper-end position (card > support lower + 1.0°), and the three absences | 1.52 / 2.03 | kept — the only negative leg of the qualification gate; exclusive fixture `test/e2e/configs/raypath_feature_max_fold_pyramid.json` |
| `AnalyzeOverviewChain.IdentityDocumentRowSwitchesUnderItsOwnKindGate` | The identity arm's real positioned row (certified source image at the sun) hides under exactly its own kind gate and the parent graphics gate — the real-row half of the switching matrix whose synthetic matrix lives at the unit tier | Per-kind gate semantics on a REAL document row (the unit tier's rows are synthetic) | 0.25 / 0.32 | kept — the unit tier cannot produce a real identity-arm row |
| `AnalyzeOverviewModel.OverlayFactsAreEmptyOnTheIdentityArm` | The builder fabricates nothing on the identity arm: no reach intervals, no edge arcs, no TIR turns (`reach_full_range` stays false — empty ≠ full range) | Empty-set equality on all three fact lists of a real rank-0 document | 0.24 / 0.32 | kept — complements (does not duplicate) `IdentityArmReadsBackSourceImageAndTint`, which guards the model arm, not the facts |
| `AnalyzeMapView.PointMarkerVisibleMapsEachKindToItsOwnGate` | The exhaustive kind→gate mapping: three image kinds switch independently, non-image kinds follow the parent alone, the parent gate removes all | Truth table over 4 kinds × 3 gate levels on the one predicate the draw loop, hit collection and legend share | ≈0.00 | kept — the shared predicate's only exhaustive enumeration |
| `AnalyzeMapView.ArcStrokeHitTestsSubmittedSegmentsOnly` | Point-to-segment distance corners (clamp, endpoints, degenerate) and the stroke hit predicate: gaps between submitted segments are not pickable, empty strokes never hit, tolerance is the caller's | Geometric predicates with hand-derived oracles | ≈0.00 | kept |
| `AnalyzePointView.InterfaceRolesFollowTheContactPosition` | Per-factor readout roles derive from the contact position: entry/internal/exit naming, reflectance vs transmittance wording, repeated-face contacts keep their own sequence, missing/mismatched face sequence degrades to contact indices without fabricating face numbers | Readout wording against a fixed numeric chain (C03's 3-5-6-5 shape) | ≈0.00 | kept — the role/naming contract of the point view's factor lines |
| `AnalyzePointView.LossReadingPicksTheLargestAbsoluteDrop` | The loss readout picks the largest absolute per-interface transmittance drop (not the largest ratio, not the first) | Selection rule against a crafted drop profile | ≈0.00 | kept |
| `AnalyzePointView.LossReadoutLinesCarryDenominatorAndScope` | Loss lines state their denominator and scope instead of a bare percentage | Denominator/scope strings on the loss lines | ≈0.00 | kept |
| `ReferenceArcDraw.SharedStrokeSubmitsOnceAndTheLegendCarriesBothOwners` | The shared stroke (AC2): a coincident edge at a reach bound submits ONE walk with both gates on (gate algebra: reach-only = both-on count, edge-only = half), legend owners and the two labels | Ink-count algebra across the 3 gate combinations + legend/label contracts | ≈0.00 | kept |
| `ReferenceArcDraw.NonCoincidentEdgeNeverMergesEvenAtTheSameDelta` | Merge authority is the card's own coincidence declaration, never screen distance or angle equality: at the SAME delta a non-coincident edge adds its own stroke | Additive ink identity (both = reach-only + edge-only) + no shared legend | ≈0.00 | kept |
| `ReferenceArcDraw.CoincidenceIdentityHoldsAcrossTheDisplayGates` | The edge identity is built from producers' declarations BEFORE gates and survives every gate combination (including non-bit-equal positions, off by one tolerance step) — hiding an owner changes visibility, never identity | Identity persistence across the 3 gate states with a deliberately non-bit-equal position | ≈0.00 | kept |
| `ReferenceArcDraw.LightSideTickPointsOutwardAcrossTheFisheyeDiskSplit` | The light-side tick is built in sky space: across the dual-fisheye disk split (sun on the equator), the tip equals the projection of the outward sky point per a test-local oracle — a screen-space direction would point inward | Tick direction against an independent test-local basis + projection | ≈0.00 | kept — the reviewer's disk-split regression |
| `ReferenceArcDraw.LabelsSplitKeepsInkAndGraphicsRemovesBoth` | The labels/graphics split: labels-off keeps ink and drops readouts; graphics-off drops both | The 2-bit visibility contract | ≈0.00 | kept |
| `ReferenceArcDraw.UnconcludedEdgeDrawsDashedAndFullRangeDrawsText` | Honesty forms: an unconcluded edge draws the weakened dashed walk (fewer segments than concluded); a full-range reach draws text, no circle pair, and says so in legend and label | Dash-phase segment-count relation + full-range text form | ≈0.00 | kept |
| `ReferenceArcDraw.TirTurnDrawsItsPairWithOneLabelAndObeyItsGate` | One visible TIR feature draws its two-delta pair in neutral ink with ONE paired readout; its gate removes ink, readout and legend; the pair never merges into a bound stroke | Pair cardinality + gate + no-merge identity | ≈0.00 | kept |
| `ReferenceArcDraw.EmptyFactsSubmitNothingWithEveryGateOn` | The fabrication detector: every gate on and no facts → no ink, no readout, no legend bit | Zero-submission over the empty fact set | ≈0.00 | kept |
| `ReferenceArcDraw.SharedStrokeLabelObeysEachOwnerLabelsSwitch` | The shared stroke's label obeys each owner's labels switch (4 combinations), with the geometry constant throughout | Label-fallback matrix + labels-independent geometry | ≈0.00 | kept |

The nine `ReferenceArcDraw` cases and the composition read-back legs are deliberately
complementary, not redundant: the fixture injects `SurveyOverlayFacts` directly (the draw
rules), while the composition tier proves the real document produces facts worth drawing.
Cheap does not auto-admit each of them — the table above states what each guards; their
combined cost is ≈0.5s, four orders below the budget question.

## Projection after the disposition (both scenarios)

Deleted rows: `OverlayFactsReadBackSupportEdgeAndTurns` and
`FullDimensionalDocumentCarriesNoPositionedCards`, −24.87s (ATT2) / −34.48s (ATT1).

| Scenario | Phase − deletions | ÷ factor | vs 360 budget |
|---|---|---|---|
| ATT1 | 532.73 − 34.48 = 498.25 | 352.28 | green, margin 7.7s |
| ATT2 | 419.57 − 24.87 = 394.70 | 346.80 | green, margin 13.2s |

Both comparable historical scenarios project under budget, so the zero-sum deletion of
incumbent tests (AC3's conditional branch) is NOT triggered — and by the task's ruling is
therefore not performed; no incumbent loses its slot in this change. These are historical
projections, not CI measurements: only the gate itself closes the duration question (PR CI
on the updated branch head). The budget, ruler set, and registry stay untouched by this
change, as does every test selector.

## Kept-set composition tiers (unchanged division of labor)

- composition (`composition_correctness_test`): real-document facts — the arms, the read-backs,
  the refusal counterexample, the premise flip. Seven `AnalyzeOverviewChain` cases remain.
- unit (`gui_unit_test`): draw rules and pure logic on the injection seam, ≈0 cost — the 15
  cases adjudicated above, all kept.
