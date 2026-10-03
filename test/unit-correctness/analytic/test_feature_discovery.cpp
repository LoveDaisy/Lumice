#include <gtest/gtest.h>

#include <string>

#include "analytic/feature_discovery.hpp"

namespace lumice::analytic {
namespace {

FeatureSupportSample Sample(uint64_t id, double x) {
  FeatureSupportSample sample;
  sample.sample_id = id;
  sample.support_dimension = 1;
  sample.coordinates = { x };
  sample.direction[0] = 1.0;
  sample.weight = 1.0;
  sample.direction_jacobian_available = true;
  sample.direction_jacobian = { 0.0, 1.0, 0.0 };
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

}  // namespace
}  // namespace lumice::analytic
