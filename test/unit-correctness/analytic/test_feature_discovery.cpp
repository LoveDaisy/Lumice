#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "analytic/feature_discovery.hpp"

namespace lumice::analytic {
namespace {

const FeatureCandidate* Candidate(const FeatureDiscoveryResult& result, FeatureMechanism mechanism) {
  const auto found = std::find_if(result.candidates.begin(), result.candidates.end(),
                                  [mechanism](const FeatureCandidate& item) { return item.mechanism == mechanism; });
  return found == result.candidates.end() ? nullptr : &*found;
}

FeatureSupportSample Sample(uint64_t id, double x) {
  FeatureSupportSample sample;
  sample.sample_id = id;
  sample.support_dimension = 1;
  sample.coordinates = { x };
  sample.active_coordinates = { 0 };
  sample.direction[0] = 1.0;
  sample.weight = 1.0;
  sample.direction_jacobian_available = true;
  sample.direction_jacobian = { 0.0, 1.0, 0.0 };
  sample.direction_jacobian_column_available = { 1 };
  sample.direction_jacobian_error = 1e-12;
  sample.direction_jacobian_resolution = 1e-4;
  SupportConstraint constraint;
  constraint.name = "domain.entry";
  constraint.kind = ConstraintKind::kEntry;
  constraint.value = 1.0;
  constraint.gradient_available = true;
  constraint.gradient = { -1.0 };
  sample.constraints.push_back(std::move(constraint));
  return sample;
}

TEST(FeatureDiscoveryModel, AcceptsACompleteContinuousSupportBatch) {
  FeatureSupportBatch batch;
  batch.coordinate_dimension = 1;
  batch.visited_row_count = 2;
  batch.complete_visit = true;
  batch.samples = { Sample(10, -1.0), Sample(20, 1.0) };
  batch.edges.push_back({ 0, 1, 2.0 });

  std::string error;
  EXPECT_TRUE(ValidateFeatureSupportBatch(batch, &error)) << error;
  const FeatureDiscoveryResult result = DiscoverFeatures(batch, {});
  EXPECT_EQ(result.visited_row_count, 2u);
  EXPECT_EQ(result.evaluated_sample_count, 2);
  EXPECT_TRUE(result.complete_visit);
  EXPECT_EQ(result.mechanisms.size(), 11u);
}

TEST(FeatureDiscoveryModel, RejectsAnAtomThatPretendsToHaveFiniteWidth) {
  FeatureSupportBatch batch;
  batch.coordinate_dimension = 1;
  batch.visited_row_count = 1;
  FeatureSupportSample sample = Sample(1, 0.0);
  sample.measure_kind = SupportMeasureKind::kAtom;
  sample.finite_width = true;
  batch.samples.push_back(std::move(sample));

  std::string error;
  EXPECT_FALSE(ValidateFeatureSupportBatch(batch, &error));
  EXPECT_NE(error.find("atomic"), std::string::npos);
}

TEST(FeatureDiscoveryModel, DistinguishesAnEmptySupportFromAMalformedBatch) {
  FeatureSupportBatch empty;
  empty.complete_visit = true;
  const FeatureDiscoveryResult empty_result = DiscoverFeatures(empty, {});
  ASSERT_FALSE(empty_result.mechanisms.empty());
  EXPECT_EQ(empty_result.mechanisms.front().status, FeatureEvidenceStatus::kPhysicallyUnreachable);

  FeatureSupportBatch malformed;
  malformed.coordinate_dimension = 1;
  malformed.visited_row_count = 0;
  malformed.samples.push_back(Sample(1, 0.0));
  const FeatureDiscoveryResult malformed_result = DiscoverFeatures(malformed, {});
  ASSERT_FALSE(malformed_result.mechanisms.empty());
  EXPECT_EQ(malformed_result.mechanisms.front().status, FeatureEvidenceStatus::kNotSupported);
}

TEST(FeatureDiscoveryModel, RejectsAnUnknownBatchVersion) {
  FeatureSupportBatch batch;
  batch.version = kFeatureSupportBatchVersion + 1;
  std::string error;
  EXPECT_FALSE(ValidateFeatureSupportBatch(batch, &error));
  EXPECT_NE(error.find("version"), std::string::npos);
}

TEST(FeatureDiscoveryModel, NamesAllPublicStatesAndMechanisms) {
  EXPECT_STREQ(FeatureEvidenceStatusName(FeatureEvidenceStatus::kNumericalIncomplete), "numerical_incomplete");
  EXPECT_STREQ(FeatureMechanismName(FeatureMechanism::kOpticalKink), "optical_kink");
  EXPECT_STREQ(SupportMeasureKindName(SupportMeasureKind::kContinuous), "continuous");
  EXPECT_STREQ(ConstraintKindName(ConstraintKind::kFilter), "filter");
}

TEST(FeatureDiscoveryDifferential, RanksOnlyTheTwoDimensionalSkyTangent) {
  FeatureSupportBatch batch;
  batch.coordinate_dimension = 3;
  batch.visited_row_count = 1;
  batch.complete_visit = true;
  FeatureSupportSample sample;
  sample.sample_id = 1;
  sample.support_dimension = 3;
  sample.coordinates = { 0.0, 0.0, 0.0 };
  sample.active_coordinates = { 0, 1, 2 };
  sample.direction[2] = 1.0;
  sample.weight = 1.0;
  sample.direction_jacobian_available = true;
  sample.direction_jacobian = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0 };
  sample.direction_jacobian_column_available = { 1, 1, 1 };
  sample.direction_jacobian_error = 1e-12;
  sample.direction_jacobian_resolution = 1e-4;
  batch.samples.push_back(std::move(sample));

  const FeatureDiscoveryResult result = DiscoverFeatures(batch, {});
  EXPECT_EQ(Candidate(result, FeatureMechanism::kInteriorRankLoss), nullptr)
      << "the 3x3 embedding determinant is zero on every S2 map but its tangent rank is two";
}

TEST(FeatureDiscoveryDifferential, DetectsRestrictedRankLossWithTwoScaleEvidence) {
  FeatureSupportBatch batch;
  batch.coordinate_dimension = 2;
  batch.visited_row_count = 1;
  batch.complete_visit = true;
  FeatureSupportSample sample;
  sample.sample_id = 1;
  sample.support_dimension = 2;
  sample.coordinates = { 0.0, 0.0 };
  sample.active_coordinates = { 0, 1 };
  sample.direction[2] = 1.0;
  sample.weight = 1.0;
  sample.direction_jacobian_available = true;
  sample.direction_jacobian = { 1.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
  sample.direction_jacobian_column_available = { 1, 1 };
  sample.direction_jacobian_error = 1e-12;
  sample.direction_jacobian_resolution = 1e-4;
  batch.samples.push_back(std::move(sample));

  const FeatureDiscoveryResult result = DiscoverFeatures(batch, {});
  const FeatureCandidate* rank_loss = Candidate(result, FeatureMechanism::kInteriorRankLoss);
  ASSERT_NE(rank_loss, nullptr);
  EXPECT_EQ(rank_loss->mapping_rank, 1);
  EXPECT_EQ(rank_loss->status, FeatureEvidenceStatus::kConfirmed);
  EXPECT_DOUBLE_EQ(rank_loss->singular_values[0], 1.0);
  EXPECT_DOUBLE_EQ(rank_loss->singular_values[1], 0.0);
}

TEST(FeatureDiscoveryDifferential, IsolatedRankZeroDoesNotBecomeAPointMass) {
  FeatureSupportBatch batch;
  batch.coordinate_dimension = 1;
  batch.visited_row_count = 1;
  batch.complete_visit = true;
  FeatureSupportSample sample = Sample(1, 0.0);
  sample.direction_jacobian = { 0.0, 0.0, 0.0 };
  batch.samples.push_back(std::move(sample));

  const FeatureDiscoveryResult result = DiscoverFeatures(batch, {});
  ASSERT_NE(Candidate(result, FeatureMechanism::kInteriorRankLoss), nullptr);
  EXPECT_EQ(Candidate(result, FeatureMechanism::kMeasureAtom), nullptr);
}

TEST(FeatureDiscoveryDifferential, PositiveInputAtomsAccumulateMassAtOneSkyLocation) {
  FeatureSupportBatch batch;
  batch.visited_row_count = 2;
  batch.complete_visit = true;
  for (uint64_t id : { 1u, 2u }) {
    FeatureSupportSample sample;
    sample.sample_id = id;
    sample.measure_kind = SupportMeasureKind::kAtom;
    sample.direction[0] = 1.0;
    sample.weight = 0.25;
    batch.samples.push_back(std::move(sample));
  }

  const FeatureDiscoveryResult result = DiscoverFeatures(batch, {});
  const FeatureCandidate* atom = Candidate(result, FeatureMechanism::kMeasureAtom);
  ASSERT_NE(atom, nullptr);
  EXPECT_DOUBLE_EQ(atom->weighted_mass, 0.5);
  EXPECT_EQ(atom->status, FeatureEvidenceStatus::kConfirmed);
}

}  // namespace
}  // namespace lumice::analytic
