// The schema3 object record (src/raypath/detail/schema3/structure_object.hpp): the four-state
// machines' data layer, pinned before any producer exists. The derivation's exhaustive 4x4
// existence x visibility grid, the fail-closed arms (escaped / walk_truncated / s4_declared and
// UNKNOWN cast values never reach actual), the open enums' registered-table walks, the default
// states (corroboration unchecked, counterfactual unavailable — the fail-visible registered gap),
// and the record-level BucketOf wiring.
//
// symmetry_semantics: none — pure data layer, no crystal, no measure.

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

#include "raypath/detail/schema3/structure_object.hpp"

namespace lumice::raypath::schema3 {
namespace {

// The exhaustive grid: every KNOWN existence x visibility pair, one expected bucket each. The
// contract's derivation: computed+certified -> actual; computed+{partial,unlit,unproven} ->
// candidate; everything else -> unfinished.
TEST(StructureObject, BucketGridExhaustive) {
  const std::vector<ExistenceState> existences = RegisteredExistenceStates();
  const std::vector<VisibilityState> visibilities = RegisteredVisibilityStates();
  ASSERT_EQ(existences.size(), 4u);
  ASSERT_EQ(visibilities.size(), 4u);
  for (const ExistenceState e : existences) {
    for (const VisibilityState v : visibilities) {
      const FeatureBucket bucket = DeriveBucket(e, v);
      if (e != ExistenceState::kComputed) {
        // AC1's "not-masquerading-as-computed" leg: escaped / walk_truncated / s4_declared are
        // unfinished REGARDLESS of the visibility verdict they happen to carry.
        EXPECT_EQ(bucket, FeatureBucket::kUnfinished) << ExistenceStateName(e) << " x " << VisibilityStateName(v);
      } else if (v == VisibilityState::kCertified) {
        EXPECT_EQ(bucket, FeatureBucket::kActual) << "computed x certified";
      } else {
        EXPECT_EQ(bucket, FeatureBucket::kCandidate) << "computed x " << VisibilityStateName(v);
      }
    }
  }
}

// The fail-closed leg for enum values the report side does not know (cast-constructed — the
// open-enum hazard the contract's own fail-closed stance names): they route, they do not crash
// and they never reach actual.
TEST(StructureObject, UnknownEnumValuesFailClosed) {
  const auto unknown_existence = static_cast<ExistenceState>(99);
  const auto unknown_visibility = static_cast<VisibilityState>(99);
  EXPECT_EQ(DeriveBucket(unknown_existence, VisibilityState::kCertified), FeatureBucket::kUnfinished);
  EXPECT_EQ(DeriveBucket(unknown_existence, unknown_visibility), FeatureBucket::kUnfinished);
  // A computed object with an unknown visibility: computed, but nothing certifies it.
  EXPECT_EQ(DeriveBucket(ExistenceState::kComputed, unknown_visibility), FeatureBucket::kCandidate);
  // The four known non-computed existences under the unknown visibility stay unfinished too.
  for (const ExistenceState e : RegisteredExistenceStates()) {
    if (e == ExistenceState::kComputed) {
      continue;
    }
    EXPECT_EQ(DeriveBucket(e, unknown_visibility), FeatureBucket::kUnfinished);
  }
}

// The object-kind table: names unique and non-null, the walk covers every registered value, and
// the S1-S6 vocabulary's declared-not-produced members are PRESENT in the table (the vocabulary
// slots exist even where v1 has no producer — the 词表三态 discipline's data side).
TEST(StructureObject, ObjectKindTableWalk) {
  const std::vector<ObjectKind>& kinds = RegisteredObjectKinds();
  ASSERT_EQ(kinds.size(), 9u);
  for (const ObjectKind kind : kinds) {
    const char* name = ObjectKindName(kind);
    if (name == nullptr) {
      ADD_FAILURE() << "registered kind without a name";
      continue;
    }
    EXPECT_STRNE(name, "unknown");
    for (const ObjectKind other : kinds) {
      if (other != kind) {
        EXPECT_STRNE(name, ObjectKindName(other)) << "duplicate name: " << name;
      }
    }
  }
  EXPECT_STREQ(ObjectKindName(ObjectKind::kCorridorClosed), "corridor_closed");
  EXPECT_STREQ(ObjectKindName(ObjectKind::kS4BranchBoundary), "s4_branch_boundary");
}

// The corroboration table: five values, unique names — and the v1 carry-invariant: the DEFAULT
// is `unchecked` and nothing in this module derives another state (666.2's job).
TEST(StructureObject, CorroborationTableWalkAndDefault) {
  const std::vector<CorroborationState>& states = RegisteredCorroborationStates();
  ASSERT_EQ(states.size(), 5u);
  for (const CorroborationState state : states) {
    const char* name = CorroborationStateName(state);
    if (name == nullptr) {
      ADD_FAILURE() << "registered corroboration state without a name";
      continue;
    }
    for (const CorroborationState other : states) {
      if (other != state) {
        EXPECT_STRNE(name, CorroborationStateName(other));
      }
    }
  }
  StructureObjectRecord record;
  EXPECT_EQ(record.corroboration, CorroborationState::kUnchecked);
  EXPECT_STREQ(CorroborationStateName(record.corroboration), "unchecked");
}

// Bucket names (the read-report vocabulary).
TEST(StructureObject, BucketNames) {
  EXPECT_STREQ(FeatureBucketName(FeatureBucket::kActual), "actual");
  EXPECT_STREQ(FeatureBucketName(FeatureBucket::kCandidate), "candidate");
  EXPECT_STREQ(FeatureBucketName(FeatureBucket::kUnfinished), "unfinished");
}

// The record-level wiring: BucketOf reads the record's own machines; an escaped record stays
// unfinished even with a certified-looking visibility verdict in its fields.
TEST(StructureObject, BucketOfReadsTheRecord) {
  StructureObjectRecord computed_certified;
  computed_certified.existence = ExistenceState::kComputed;
  computed_certified.visibility.state = VisibilityState::kCertified;
  EXPECT_EQ(BucketOf(computed_certified), FeatureBucket::kActual);

  StructureObjectRecord escaped;
  escaped.existence = ExistenceState::kEscaped;
  escaped.escape_regime_slug = "slab_crease";  // kernel slug, data
  escaped.walk_s = std::nan("");
  escaped.visibility.state = VisibilityState::kCertified;  // must be IGNORED by the derivation
  EXPECT_EQ(BucketOf(escaped), FeatureBucket::kUnfinished);

  StructureObjectRecord c09;
  c09.existence = ExistenceState::kComputed;
  c09.visibility.state = VisibilityState::kUnlit;
  EXPECT_EQ(BucketOf(c09), FeatureBucket::kCandidate);
}

// The v1 defaults: a freshly built record is computed+unproven (candidate), chromatic not
// assessed, corroboration unchecked, counterfactual unavailable (the fail-visible registered
// gap — never a plausible zero), geometry empty.
TEST(StructureObject, RecordDefaults) {
  StructureObjectRecord record;
  EXPECT_EQ(BucketOf(record), FeatureBucket::kCandidate);
  EXPECT_FALSE(record.chromatic_assessed);
  EXPECT_EQ(record.slot, -1);
  EXPECT_TRUE(record.phi_class_note.empty());
  EXPECT_TRUE(record.u.empty());
  EXPECT_FALSE(record.has_sky_position);
  EXPECT_EQ(record.visibility.state, VisibilityState::kUnproven);
  EXPECT_STREQ(record.visibility.reason, "");
  EXPECT_FALSE(record.counterfactual.available);
  EXPECT_STREQ(CorroborationStateName(record.corroboration), "unchecked");
  // The declared-parameter snapshot is present even when unassessed: the record always carries
  // which criterion a chromatic label would come from.
  EXPECT_DOUBLE_EQ(record.chromatic_thresholds.n_red, 1.307);
  EXPECT_DOUBLE_EQ(record.chromatic_thresholds.n_blue, 1.317);
}

}  // namespace
}  // namespace lumice::raypath::schema3
