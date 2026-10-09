// The no_related_feature dual-form ruling (src/raypath/detail/schema3/no_related_feature.{hpp,cpp})
// and the zero-spectral-signal predicate's extraction (input_assembly.{hpp,cpp}). Pinned here,
// per scrum.md section 3's contract: rule A outranks rule B (the priority pin included — zero
// signal with every B condition satisfied still issues A); rule B's FOUR conditions each break
// the issuance on their own leg (partition-incomplete / an object outside unlit-or-none /
// sufficient-ESS unattributed in play / the S4 scope declaration missing), each with its own
// failure naming; rule 3's kArea gate is a separate door beside the four (a restricted family
// does not issue even with all four conditions green); and the full B positive issues
// certified_smooth_radiance. The predicate itself: a zero-weight discrete spectrum is the exact
// zero (the v2 assembler's rows carry zero coefficients), a nonzero one is not, and a
// continuous/quadrature spectrum never is — the extraction is behavior-neutral (the v2 report
// test ExactZeroSpectralSignalIsNotAClaimBasedOnEmptySampling pins the same contract end to end).

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "raypath/detail/input_assembly.hpp"
#include "raypath/detail/schema3/no_related_feature.hpp"

namespace lumice::raypath::schema3 {
namespace {

// A synthetic core whose four condition inputs are all controlled by hand.
struct RulingCore {
  Schema3DiscoveryCore core;

  RulingCore& WithCompletePartition() {
    MemberSupport row;
    row.member = { 3, 5 };
    row.axis.context.coverage = PartitionContext::Coverage::kComplete;
    analytic::DeviationInterval interval;
    interval.lower = 0.3;
    interval.upper = 1.2;
    row.axis.intervals = { interval };
    core.support.members.push_back(std::move(row));
    return *this;
  }
  RulingCore& WithRefusedRow() {
    MemberSupport row;
    row.member = { 3, 1, 5 };
    row.axis.context.coverage = PartitionContext::Coverage::kIncomplete;
    row.axis.regime_slug = "slab_crease_touching";
    core.support.members.push_back(std::move(row));
    return *this;
  }
  RulingCore& WithUnlitObject() {
    StructureObjectRecord object;
    object.kind = ObjectKind::kKind1;
    object.member = { 3, 5 };
    object.existence = ExistenceState::kComputed;
    object.visibility.state = VisibilityState::kUnlit;
    core.objects.push_back(std::move(object));
    return *this;
  }
  RulingCore& WithCertifiedObject() {
    StructureObjectRecord object;
    object.kind = ObjectKind::kKind1;
    object.member = { 3, 5 };
    object.existence = ExistenceState::kComputed;
    object.visibility.state = VisibilityState::kCertified;
    core.objects.push_back(std::move(object));
    return *this;
  }
  RulingCore& WithS4Declared() {
    core.coverage.declared_not_produced.push_back(ObjectKind::kS4BranchBoundary);
    core.coverage.declared_reasons.push_back(
        "owner ruling 2026-10-07: v1 declares the branch boundary, does not trace it (open math)");
    return *this;
  }
};

UnattributedOutcome CleanForward() {
  return UnattributedOutcome{};
}

UnattributedOutcome ForwardWithOneStructure() {
  UnattributedOutcome outcome;
  UnattributedStructure structure;
  structure.record_index = 0;
  structure.min_margin_rad = 0.02;
  outcome.structures.push_back(std::move(structure));
  return outcome;
}

// The all-green B premise: complete partition, one unlit object, clean forward, S4 declared,
// kArea support.
NoRelatedRuling RulingOn(RulingCore fixture, const UnattributedOutcome& forward, USupportKind kind, bool zero_signal) {
  return DeriveNoRelatedFeature(fixture.core, forward, kind, zero_signal);
}

// ---- rule B: the positive and the four break legs ----------------------------------------------

TEST(NoRelatedFeature, FullConditionsOnAreaSupportIssueCertifiedSmoothRadiance) {
  const NoRelatedRuling ruling = RulingOn(RulingCore().WithCompletePartition().WithUnlitObject().WithS4Declared(),
                                          CleanForward(), USupportKind::kArea, false);
  EXPECT_TRUE(ruling.issued);
  EXPECT_EQ(ruling.basis, NoRelatedBasis::kCertifiedSmoothRadiance);
  EXPECT_TRUE(ruling.partition_complete_no_escape);
  EXPECT_TRUE(ruling.all_objects_unlit_or_none);
  EXPECT_TRUE(ruling.no_sufficient_ess_unattributed);
  EXPECT_TRUE(ruling.s4_scope_declared);
  EXPECT_TRUE(ruling.two_d_valid_support);
  // The scope declaration's own text rides the ruling (the S4 leg's provenance).
  EXPECT_NE(ruling.s4_scope_note.find("declares the branch boundary"), std::string::npos);
  EXPECT_TRUE(ruling.absence_not_proven_note.empty());
}

TEST(NoRelatedFeature, BrokenPartitionBlocksIssuanceAndNamesTheRow) {
  const NoRelatedRuling ruling = RulingOn(RulingCore().WithRefusedRow().WithUnlitObject().WithS4Declared(),
                                          CleanForward(), USupportKind::kArea, false);
  EXPECT_FALSE(ruling.issued);
  EXPECT_FALSE(ruling.partition_complete_no_escape);
  EXPECT_NE(ruling.partition_failure.find("did not complete"), std::string::npos);
  EXPECT_NE(ruling.partition_failure.find("slab_crease_touching"), std::string::npos);
  EXPECT_FALSE(ruling.absence_not_proven_note.empty());
}

TEST(NoRelatedFeature, ObjectOutsideUnlitOrNoneBlocksIssuanceAndNamesTheObject) {
  // A certified object breaks the arm (only unlit-or-none keeps it); so does the default
  // unproven certificate — the fail-closed reading of A6.
  const NoRelatedRuling certified =
      RulingOn(RulingCore().WithCompletePartition().WithCertifiedObject().WithS4Declared(), CleanForward(),
               USupportKind::kArea, false);
  EXPECT_FALSE(certified.issued);
  EXPECT_FALSE(certified.all_objects_unlit_or_none);
  EXPECT_NE(certified.unlit_failure.find("certified"), std::string::npos);

  RulingCore unproven_fixture = RulingCore().WithCompletePartition().WithS4Declared();
  StructureObjectRecord unproven;
  unproven.visibility.state = VisibilityState::kUnproven;  // the default certificate
  unproven_fixture.core.objects.push_back(std::move(unproven));
  const NoRelatedRuling unproven_ruling =
      DeriveNoRelatedFeature(unproven_fixture.core, CleanForward(), USupportKind::kArea, false);
  EXPECT_FALSE(unproven_ruling.issued);
  EXPECT_NE(unproven_ruling.unlit_failure.find("fail closed"), std::string::npos);
}

TEST(NoRelatedFeature, SufficientEssUnattributedContradictsAndBlocksIssuance) {
  // The "B rejected by MC contradiction" leg: one unattributed structure at standing and the
  // certificate must not issue — the MC just disagreed.
  const NoRelatedRuling ruling = RulingOn(RulingCore().WithCompletePartition().WithUnlitObject().WithS4Declared(),
                                          ForwardWithOneStructure(), USupportKind::kArea, false);
  EXPECT_FALSE(ruling.issued);
  EXPECT_FALSE(ruling.no_sufficient_ess_unattributed);
  EXPECT_NE(ruling.unattributed_failure.find("contradicts the claim"), std::string::npos);
  EXPECT_NE(ruling.unattributed_failure.find("1 unattributed structure(s)"), std::string::npos);
}

TEST(NoRelatedFeature, MissingS4ScopeDeclarationBlocksIssuance) {
  // The S4 leg: without the coverage's declared-not-produced row the scope statement is
  // missing and the certificate does not issue.
  const NoRelatedRuling ruling =
      RulingOn(RulingCore().WithCompletePartition().WithUnlitObject(), CleanForward(), USupportKind::kArea, false);
  EXPECT_FALSE(ruling.issued);
  EXPECT_FALSE(ruling.s4_scope_declared);
  EXPECT_NE(ruling.s4_failure.find("does not declare"), std::string::npos);
  EXPECT_TRUE(ruling.s4_scope_note.empty());
}

TEST(NoRelatedFeature, RestrictedFamilySupportDoesNotIssue) {
  // Rule 3's gate, beside the four: a kSpinOrbit (restricted family) measure does not issue
  // even with all four conditions green — its absence is the support block's expression.
  const NoRelatedRuling ruling = RulingOn(RulingCore().WithCompletePartition().WithUnlitObject().WithS4Declared(),
                                          CleanForward(), USupportKind::kSpinOrbit, false);
  EXPECT_FALSE(ruling.issued);
  EXPECT_TRUE(ruling.partition_complete_no_escape);
  EXPECT_TRUE(ruling.all_objects_unlit_or_none);
  EXPECT_TRUE(ruling.no_sufficient_ess_unattributed);
  EXPECT_TRUE(ruling.s4_scope_declared);
  EXPECT_FALSE(ruling.two_d_valid_support);
}

TEST(NoRelatedFeature, EmptyObjectSetKeepsTheUnlitOrNoneArmTrue) {
  // The "or none" reading of condition 2: no objects at all keeps the arm true — the
  // certificate still needs the other three conditions and the kArea gate.
  const NoRelatedRuling ruling =
      RulingOn(RulingCore().WithCompletePartition().WithS4Declared(), CleanForward(), USupportKind::kArea, false);
  EXPECT_TRUE(ruling.all_objects_unlit_or_none);
  EXPECT_TRUE(ruling.issued);
  EXPECT_EQ(ruling.basis, NoRelatedBasis::kCertifiedSmoothRadiance);
}

// ---- rule A: the zero signal outranks everything ------------------------------------------------

TEST(NoRelatedFeature, ZeroSignalIssuesBasisAEvenWhenBWouldAlsoIssue) {
  // The priority pin: every B condition green AND the zero signal — A issues.
  const NoRelatedRuling ruling = RulingOn(RulingCore().WithCompletePartition().WithUnlitObject().WithS4Declared(),
                                          CleanForward(), USupportKind::kArea, true);
  EXPECT_TRUE(ruling.issued);
  EXPECT_EQ(ruling.basis, NoRelatedBasis::kZeroSpectralSignal);
  EXPECT_EQ(std::string(NoRelatedBasisName(ruling.basis)), "zero_spectral_signal");
  // B's conditions are still evaluated and reported — the priority does not blind the report.
  EXPECT_TRUE(ruling.partition_complete_no_escape);
  EXPECT_TRUE(ruling.two_d_valid_support);
}

TEST(NoRelatedFeature, ZeroSignalIssuesBasisAEvenWhenBIsBlocked) {
  // The same priority over a blocked B: a certified object plus a contradiction at standing,
  // and the zero signal still issues A (and only A).
  const NoRelatedRuling ruling = RulingOn(RulingCore().WithCompletePartition().WithCertifiedObject(),
                                          ForwardWithOneStructure(), USupportKind::kSpinOrbit, true);
  EXPECT_TRUE(ruling.issued);
  EXPECT_EQ(ruling.basis, NoRelatedBasis::kZeroSpectralSignal);
  EXPECT_FALSE(ruling.two_d_valid_support);
  EXPECT_EQ(std::string(NoRelatedBasisName(NoRelatedBasis::kCertifiedSmoothRadiance)), "certified_smooth_radiance");
}

// ---- the zero-signal predicate, at its new single home -------------------------------------------

TEST(ZeroSpectralSignal, ExactZeroCoefficientRowsAreTheConfigStatement) {
  InputSnapshot snapshot;
  snapshot.light.spectrum_ = std::vector<WlParam>{ { 550, 0 } };
  AssembledInput representative;
  SpectralRow zero_row;
  zero_row.wavelength_nm = 550;
  zero_row.coefficient = { 0, 0, 0 };
  representative.spectrum.rows = { zero_row };
  EXPECT_TRUE(ZeroSpectralSignal(snapshot, representative));
  // A nonzero coefficient breaks it.
  SpectralRow lit_row = zero_row;
  lit_row.coefficient = { 0.1, 0.2, 0.3 };
  representative.spectrum.rows = { zero_row, lit_row };
  EXPECT_FALSE(ZeroSpectralSignal(snapshot, representative));
  // A continuous illuminant (not a discrete WlParam list) is never the exact zero — the
  // coefficient rows say nothing about the band integral.
  snapshot.light.spectrum_ = IlluminantType::kD65;
  representative.spectrum.rows = { zero_row };
  EXPECT_FALSE(ZeroSpectralSignal(snapshot, representative));
}

}  // namespace
}  // namespace lumice::raypath::schema3
