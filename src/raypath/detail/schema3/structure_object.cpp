#include "raypath/detail/schema3/structure_object.hpp"

namespace lumice::raypath::schema3 {

// The registered tables of the module's open enums (the contract's mechanism: a value outside
// its table is found by the coverage test the same change must extend, not by a promise).

const char* ObjectKindName(ObjectKind kind) {
  switch (kind) {
    case ObjectKind::kKind1:
      return "kind_1";
    case ObjectKind::kKind1Restricted:
      return "kind_1_restricted";
    case ObjectKind::kKind2:
      return "kind_2";
    case ObjectKind::kKind3:
      return "kind_3";
    case ObjectKind::kCorridorClosed:
      return "corridor_closed";
    case ObjectKind::kS3SupportBoundary:
      return "s3_support_boundary";
    case ObjectKind::kS4BranchBoundary:
      return "s4_branch_boundary";
    case ObjectKind::kS5DensityFeature:
      return "s5_density_feature";
    case ObjectKind::kS6Junction:
      return "s6_junction";
  }
  return nullptr;  // an unregistered value is a nullptr name, not a plausible word
}

const std::vector<ObjectKind>& RegisteredObjectKinds() {
  static const std::vector<ObjectKind> kKinds = {
    ObjectKind::kKind1,
    ObjectKind::kKind1Restricted,
    ObjectKind::kKind2,
    ObjectKind::kKind3,
    ObjectKind::kCorridorClosed,
    ObjectKind::kS3SupportBoundary,
    ObjectKind::kS4BranchBoundary,
    ObjectKind::kS5DensityFeature,
    ObjectKind::kS6Junction,
  };
  return kKinds;
}

const char* CorroborationStateName(CorroborationState state) {
  switch (state) {
    case CorroborationState::kObserved:
      return "observed";
    case CorroborationState::kConsistent:
      return "consistent";
    case CorroborationState::kNotObservedInsufficientEss:
      return "not_observed_insufficient_ess";
    case CorroborationState::kNotObservedDespiteSufficientEss:
      return "not_observed_despite_sufficient_ess";
    case CorroborationState::kUnchecked:
      return "unchecked";
  }
  return nullptr;
}

const std::vector<CorroborationState>& RegisteredCorroborationStates() {
  static const std::vector<CorroborationState> kStates = {
    CorroborationState::kObserved,
    CorroborationState::kConsistent,
    CorroborationState::kNotObservedInsufficientEss,
    CorroborationState::kNotObservedDespiteSufficientEss,
    CorroborationState::kUnchecked,
  };
  return kStates;
}

const char* FeatureBucketName(FeatureBucket bucket) {
  switch (bucket) {
    case FeatureBucket::kActual:
      return "actual";
    case FeatureBucket::kCandidate:
      return "candidate";
    case FeatureBucket::kUnfinished:
      return "unfinished";
  }
  return nullptr;
}

FeatureBucket DeriveBucket(ExistenceState existence, VisibilityState visibility) {
  // The contract's derivation, with the fail-closed arms spelled: only a computed object reads
  // its visibility at all; only a certified one is actual. A cast-constructed existence lands in
  // the default (not computed -> unfinished) without dereferencing anything; a cast visibility on
  // a computed object is "not certified" -> candidate.
  if (existence != ExistenceState::kComputed) {
    return FeatureBucket::kUnfinished;
  }
  switch (visibility) {
    case VisibilityState::kCertified:
      return FeatureBucket::kActual;
    case VisibilityState::kPartial:
    case VisibilityState::kUnlit:
    case VisibilityState::kUnproven:
      return FeatureBucket::kCandidate;
  }
  return FeatureBucket::kCandidate;  // unknown visibility value: computed, uncertified — fail closed
}

FeatureBucket BucketOf(const StructureObjectRecord& record) {
  return DeriveBucket(record.existence, record.visibility.state);
}

}  // namespace lumice::raypath::schema3
