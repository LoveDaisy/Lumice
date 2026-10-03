#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#include "analytic/feature_discovery.hpp"

namespace lumice::analytic {
namespace {

const FeatureCandidate* Candidate(const FeatureDiscoveryResult& result, FeatureMechanism mechanism) {
  const auto found = std::find_if(result.candidates.begin(), result.candidates.end(),
                                  [mechanism](const FeatureCandidate& item) { return item.mechanism == mechanism; });
  return found == result.candidates.end() ? nullptr : &*found;
}

std::vector<const FeatureCandidate*> Candidates(const FeatureDiscoveryResult& result, FeatureMechanism mechanism) {
  std::vector<const FeatureCandidate*> matches;
  for (const FeatureCandidate& candidate : result.candidates) {
    if (candidate.mechanism == mechanism) {
      matches.push_back(&candidate);
    }
  }
  return matches;
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

FeatureSupportSample GraphSample(uint64_t id, double u, double v, double graph, double graph_u, double graph_v,
                                 bool accumulates_measure) {
  FeatureSupportSample sample;
  sample.sample_id = id;
  sample.support_dimension = 2;
  sample.coordinates = { u, v };
  sample.active_coordinates = { 0, 1 };
  const double raw[3] = { 1.0, u, graph };
  const double norm = std::sqrt(raw[0] * raw[0] + raw[1] * raw[1] + raw[2] * raw[2]);
  for (int component = 0; component < 3; ++component) {
    sample.direction[component] = raw[component] / norm;
  }
  const double raw_columns[2][3] = { { 0.0, 1.0, graph_u }, { 0.0, 0.0, graph_v } };
  sample.direction_jacobian.resize(6);
  for (int coordinate = 0; coordinate < 2; ++coordinate) {
    double projection = 0.0;
    for (int component = 0; component < 3; ++component) {
      projection += sample.direction[component] * raw_columns[coordinate][component];
    }
    for (int component = 0; component < 3; ++component) {
      sample.direction_jacobian[static_cast<size_t>(component * 2 + coordinate)] =
          (raw_columns[coordinate][component] - sample.direction[component] * projection) / norm;
    }
  }
  sample.weight = accumulates_measure ? 1.0 : 0.0;
  sample.accumulates_measure = accumulates_measure;
  sample.direction_jacobian_available = true;
  sample.direction_jacobian_column_available = { 1, 1 };
  sample.direction_jacobian_resolution = 1e-6;
  return sample;
}

FeatureSupportBatch DenseSkyBatch(int z_bins, int azimuth_bins, const std::function<double(int, int)>& density) {
  constexpr double kPi = 3.14159265358979323846;
  FeatureSupportBatch batch;
  batch.complete_visit = true;
  const double solid_angle = 4.0 * kPi / static_cast<double>(z_bins * azimuth_bins);
  for (int z_index = 0; z_index < z_bins; ++z_index) {
    const double z = -1.0 + (static_cast<double>(z_index) + 0.5) * 2.0 / z_bins;
    const double radius = std::sqrt(1.0 - z * z);
    for (int azimuth_index = 0; azimuth_index < azimuth_bins; ++azimuth_index) {
      const double azimuth = (static_cast<double>(azimuth_index) + 0.5) * 2.0 * kPi / azimuth_bins;
      FeatureSupportSample sample;
      sample.sample_id = static_cast<uint64_t>(z_index * azimuth_bins + azimuth_index + 1);
      sample.measure_kind = SupportMeasureKind::kAtom;
      sample.direction[0] = radius * std::cos(azimuth);
      sample.direction[1] = radius * std::sin(azimuth);
      sample.direction[2] = z;
      sample.weight = density(z_index, azimuth_index) * solid_angle;
      batch.samples.push_back(std::move(sample));
    }
  }
  batch.visited_row_count = batch.samples.size();
  return batch;
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

  empty.visited_row_count = 100;
  empty.materialization_complete = false;
  const FeatureDiscoveryResult incomplete_result = DiscoverFeatures(empty, {});
  ASSERT_FALSE(incomplete_result.mechanisms.empty());
  EXPECT_TRUE(
      std::all_of(incomplete_result.mechanisms.begin(), incomplete_result.mechanisms.end(),
                  [](const auto& record) { return record.status == FeatureEvidenceStatus::kNumericalIncomplete; }));

  FeatureSupportBatch malformed;
  malformed.coordinate_dimension = 1;
  malformed.visited_row_count = 0;
  malformed.samples.push_back(Sample(1, 0.0));
  const FeatureDiscoveryResult malformed_result = DiscoverFeatures(malformed, {});
  ASSERT_FALSE(malformed_result.mechanisms.empty());
  EXPECT_EQ(malformed_result.mechanisms.front().status, FeatureEvidenceStatus::kNotSupported);
}

TEST(FeatureDiscoveryModel, RejectsMalformedCellAndEdgeTopology) {
  FeatureSupportBatch batch;
  batch.coordinate_dimension = 2;
  batch.visited_row_count = 1;
  FeatureSupportSample center = Sample(1, 0.0);
  center.support_dimension = 2;
  center.coordinates = { 0.0, 0.25 };
  center.active_coordinates = { 0, 1 };
  center.direction_jacobian = { 0.0, 0.0, 1.0, 0.0, 0.0, 0.0 };
  center.direction_jacobian_column_available = { 1, 1 };
  center.constraints[0].gradient = { -1.0, 0.0 };
  FeatureSupportSample lower = center;
  lower.sample_id = 2;
  lower.accumulates_measure = false;
  lower.coordinates = { -1.0, 0.0 };
  FeatureSupportSample upper = center;
  upper.sample_id = 3;
  upper.accumulates_measure = false;
  upper.coordinates = { 1.0, 0.0 };
  batch.samples = { center, lower, upper };
  batch.cell_axes.push_back({ 0, 0, 1, 0, 2, 2.0 });

  std::string error;
  EXPECT_FALSE(ValidateFeatureSupportBatch(batch, &error));
  EXPECT_NE(error.find("differ only"), std::string::npos) << error;

  batch.cell_axes.clear();
  batch.edges.push_back({ 0, 2, std::sqrt(1.0 + 0.25 * 0.25) });
  batch.samples[2].provenance.member_index = 1;
  EXPECT_FALSE(ValidateFeatureSupportBatch(batch, &error));
  EXPECT_NE(error.find("provenance branch"), std::string::npos) << error;
}

TEST(FeatureDiscoveryModel, RejectsAnUnknownBatchVersion) {
  FeatureSupportBatch batch;
  batch.version = kFeatureSupportBatchVersion + 1;
  std::string error;
  EXPECT_FALSE(ValidateFeatureSupportBatch(batch, &error));
  EXPECT_NE(error.find("version"), std::string::npos);
}

TEST(FeatureDiscoveryModel, RequiresExactMappingEvidenceToCoverACompleteCell) {
  FeatureSupportBatch batch;
  batch.coordinate_dimension = 1;
  batch.visited_row_count = 1;
  batch.complete_visit = true;
  FeatureSupportSample center = Sample(1, 0.0);
  center.direction_jacobian = { 0.0, 0.0, 0.0 };
  center.mapping_evidence_kind = MappingEvidenceKind::kExactImageDimensionUpperBound;
  center.image_dimension_upper_bound = 0;
  FeatureSupportSample lower = center;
  lower.sample_id = 2;
  lower.coordinates[0] = -1.0;
  lower.accumulates_measure = false;
  lower.mapping_evidence_kind = MappingEvidenceKind::kNone;
  lower.image_dimension_upper_bound = -1;
  FeatureSupportSample upper = lower;
  upper.sample_id = 3;
  upper.coordinates[0] = 1.0;
  batch.samples = { center, lower, upper };

  std::string error;
  EXPECT_FALSE(ValidateFeatureSupportBatch(batch, &error));
  EXPECT_NE(error.find("complete support cell"), std::string::npos) << error;

  batch.cell_axes.push_back({ 0, 0, 1, 0, 2, 2.0 });
  EXPECT_TRUE(ValidateFeatureSupportBatch(batch, &error)) << error;

  batch.samples[0].image_dimension_upper_bound = 2;
  EXPECT_FALSE(ValidateFeatureSupportBatch(batch, &error));
  EXPECT_NE(error.find("exact valid bound"), std::string::npos) << error;
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

TEST(FeatureDiscoveryDifferential, IsolatedRestrictedRankLossRemainsACandidateWithoutANeighborhood) {
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
  EXPECT_EQ(rank_loss->status, FeatureEvidenceStatus::kCandidate);
  EXPECT_DOUBLE_EQ(rank_loss->singular_values[0], 1.0);
  EXPECT_DOUBLE_EQ(rank_loss->singular_values[1], 0.0);
}

TEST(FeatureDiscoveryDifferential, RealCellNeighborhoodConfirmsRankLossAgainstItsLocalRegularRank) {
  FeatureSupportBatch batch;
  batch.coordinate_dimension = 2;
  batch.visited_row_count = 1;
  batch.complete_visit = true;
  for (int index = 0; index < 3; ++index) {
    FeatureSupportSample sample;
    sample.sample_id = static_cast<uint64_t>(index + 1);
    sample.support_dimension = 2;
    sample.coordinates = { static_cast<double>(index - 1), 0.0 };
    sample.active_coordinates = { 0, 1 };
    sample.direction[2] = 1.0;
    sample.weight = 1.0;
    sample.accumulates_measure = index == 1;
    sample.direction_jacobian_available = true;
    sample.direction_jacobian = index == 1 ? std::vector<double>{ 1.0, 0.0, 0.0, 0.0, 0.0, 0.0 } :
                                             std::vector<double>{ 1.0, 0.0, 0.0, 1.0, 0.0, 0.0 };
    sample.direction_jacobian_column_available = { 1, 1 };
    sample.direction_jacobian_error = 1e-12;
    sample.direction_jacobian_resolution = 1e-4;
    batch.samples.push_back(std::move(sample));
  }
  batch.cell_axes.push_back({ 0, 0, 0, 1, 2, 2.0 });

  const FeatureDiscoveryResult result = DiscoverFeatures(batch, {});
  const FeatureCandidate* rank_loss = Candidate(result, FeatureMechanism::kInteriorRankLoss);
  ASSERT_NE(rank_loss, nullptr);
  EXPECT_EQ(rank_loss->status, FeatureEvidenceStatus::kConfirmed);
  EXPECT_EQ(rank_loss->mapping_rank, 1);
}

TEST(FeatureDiscoveryDifferential, RefinesABracketedFoldBetweenMaterializedNodes) {
  FeatureSupportBatch batch;
  batch.coordinate_dimension = 1;
  batch.visited_row_count = 1;
  batch.complete_visit = true;
  auto fold_sample = [](uint64_t id, double x, bool accumulates_measure) {
    FeatureSupportSample sample = Sample(id, x);
    const double phase = x * x;
    sample.accumulates_measure = accumulates_measure;
    sample.direction[0] = std::cos(phase);
    sample.direction[1] = std::sin(phase);
    sample.direction_jacobian = { -2.0 * x * std::sin(phase), 2.0 * x * std::cos(phase), 0.0 };
    return sample;
  };
  batch.samples = { fold_sample(1, 0.25, true), fold_sample(2, -0.75, false), fold_sample(3, 1.25, false) };
  batch.cell_axes.push_back({ 1, 0, 1, 0, 2, 2.0 });
  int callback_count = 0;
  const FeatureReevaluateFn callback = [&](const FeatureReevaluationRequest& request, FeatureSupportSample* sample,
                                           std::string*) {
    ++callback_count;
    *sample = fold_sample(100 + static_cast<uint64_t>(callback_count), request.coordinates[0], false);
    sample->provenance = request.provenance;
    return true;
  };

  const FeatureDiscoveryResult result = DiscoverFeatures(batch, {}, callback);
  const FeatureCandidate* rank_loss = Candidate(result, FeatureMechanism::kInteriorRankLoss);
  ASSERT_NE(rank_loss, nullptr);
  EXPECT_EQ(rank_loss->status, FeatureEvidenceStatus::kConfirmed);
  EXPECT_EQ(rank_loss->mapping_rank, 0);
  EXPECT_NEAR(rank_loss->direction[0], 1.0, 1e-12);
  EXPECT_NEAR(rank_loss->direction[1], 0.0, 1e-12);
  EXPECT_GT(callback_count, 0);
  EXPECT_LE(callback_count, FeatureDiscoveryOptions{}.maximum_refinement_steps);
}

TEST(FeatureDiscoveryDifferential, PreservesConditionalScopeAndParameterRoles) {
  FeatureSupportBatch batch;
  batch.coordinate_dimension = 1;
  batch.visited_row_count = 1;
  batch.complete_visit = true;
  auto fold_sample = [](uint64_t id, double x, bool accumulates_measure) {
    FeatureSupportSample sample = Sample(id, x);
    const double phase = x * x;
    sample.accumulates_measure = accumulates_measure;
    sample.direction[0] = std::cos(phase);
    sample.direction[1] = std::sin(phase);
    sample.direction_jacobian = { -2.0 * x * std::sin(phase), 2.0 * x * std::cos(phase), 0.0 };
    return sample;
  };
  batch.samples = { fold_sample(1, 0.25, true), fold_sample(2, -0.75, false), fold_sample(3, 1.25, false) };
  batch.cell_axes.push_back({ 7, 0, 1, 0, 2, 2.0 });
  batch.parameter_descriptors.push_back({ FeatureParameterRole::kPose, 3 });
  batch.scopes.push_back({ 42, 7, FeatureSupportScopeKind::kConditional });
  const FeatureReevaluateFn callback = [&](const FeatureReevaluationRequest& request, FeatureSupportSample* sample,
                                           std::string*) {
    *sample = fold_sample(100, request.coordinates[0], false);
    sample->provenance = request.provenance;
    return true;
  };

  const FeatureDiscoveryResult result = DiscoverFeatures(batch, {}, callback);
  const FeatureCandidate* rank_loss = Candidate(result, FeatureMechanism::kInteriorRankLoss);
  ASSERT_NE(rank_loss, nullptr);
  EXPECT_EQ(rank_loss->scope_kind, FeatureSupportScopeKind::kConditional);
  EXPECT_EQ(rank_loss->scope_id, 42);
  EXPECT_EQ(rank_loss->scope_active_coordinates, std::vector<int>({ 0 }));
  ASSERT_EQ(rank_loss->scope_parameters.size(), 1u);
  EXPECT_EQ(rank_loss->scope_parameters[0].role, FeatureParameterRole::kPose);
  EXPECT_EQ(rank_loss->scope_parameters[0].group_id, 3);

  batch.parameter_descriptors[0].group_id = -1;
  std::string error;
  EXPECT_FALSE(ValidateFeatureSupportBatch(batch, &error));
  EXPECT_NE(error.find("group"), std::string::npos);
}

TEST(FeatureDiscoveryDifferential, DirectEvidenceExpandsToJointAndConditionalScopesWithoutDuplicatingMass) {
  FeatureSupportBatch batch;
  batch.version = kFeatureSupportBatchVersion;
  batch.coordinate_dimension = 1;
  batch.visited_row_count = 1;
  batch.complete_visit = true;
  auto constant_sample = [](uint64_t id, double x, bool accumulates_measure) {
    FeatureSupportSample sample = Sample(id, x);
    sample.accumulates_measure = accumulates_measure;
    sample.weight = accumulates_measure ? 1.0 : 0.0;
    sample.direction_jacobian = { 0.0, 0.0, 0.0 };
    sample.mapping_evidence_kind =
        accumulates_measure ? MappingEvidenceKind::kExactImageDimensionUpperBound : MappingEvidenceKind::kNone;
    sample.image_dimension_upper_bound = accumulates_measure ? 0 : -1;
    return sample;
  };
  batch.samples = { constant_sample(1, 0.0, true), constant_sample(2, -1.0, false), constant_sample(3, 1.0, false) };
  batch.cell_axes = { { 7, 0, 1, 0, 2, 2.0 }, { 8, 0, 1, 0, 2, 2.0 } };
  batch.parameter_descriptors = { { FeatureParameterRole::kPose, 2 } };
  batch.scopes = { { 42, 7, FeatureSupportScopeKind::kJoint }, { 43, 8, FeatureSupportScopeKind::kConditional } };

  const FeatureDiscoveryResult result = DiscoverFeatures(batch, {});
  const auto check_scope_projection = [&](FeatureMechanism mechanism) {
    const std::vector<const FeatureCandidate*> candidates = Candidates(result, mechanism);
    ASSERT_EQ(candidates.size(), 2u);
    const auto joint = std::find_if(candidates.begin(), candidates.end(), [](const FeatureCandidate* candidate) {
      return candidate->scope_kind == FeatureSupportScopeKind::kJoint;
    });
    const auto conditional = std::find_if(candidates.begin(), candidates.end(), [](const FeatureCandidate* candidate) {
      return candidate->scope_kind == FeatureSupportScopeKind::kConditional;
    });
    ASSERT_NE(joint, candidates.end());
    ASSERT_NE(conditional, candidates.end());
    EXPECT_EQ((*joint)->scope_id, 42);
    EXPECT_EQ((*conditional)->scope_id, 43);
    EXPECT_DOUBLE_EQ((*joint)->weighted_mass, 1.0);
    EXPECT_DOUBLE_EQ((*conditional)->weighted_mass, 0.0);
    EXPECT_NE((*joint)->evidence_id, 0u);
    EXPECT_EQ((*joint)->evidence_id, (*conditional)->evidence_id);
    ASSERT_EQ((*conditional)->scope_parameters.size(), 1u);
    EXPECT_EQ((*conditional)->scope_parameters[0].role, FeatureParameterRole::kPose);
    EXPECT_EQ((*conditional)->scope_parameters[0].group_id, 2);
  };
  for (FeatureMechanism mechanism : { FeatureMechanism::kMeasureAtom, FeatureMechanism::kStrictConfinement }) {
    check_scope_projection(mechanism);
  }
}

TEST(FeatureDiscoveryDifferential, SearchesAContinuousCellWithoutASignedFoldBracket) {
  FeatureSupportBatch batch;
  batch.coordinate_dimension = 1;
  batch.visited_row_count = 1;
  batch.complete_visit = true;
  auto regular_sample = [](uint64_t id, double x, bool accumulates_measure) {
    FeatureSupportSample sample = Sample(id, x);
    sample.accumulates_measure = accumulates_measure;
    sample.direction[0] = std::cos(x);
    sample.direction[1] = std::sin(x);
    sample.direction_jacobian = { -std::sin(x), std::cos(x), 0.0 };
    return sample;
  };
  batch.samples = { regular_sample(1, 0.0, true), regular_sample(2, -0.5, false), regular_sample(3, 0.5, false) };
  batch.cell_axes.push_back({ 1, 0, 1, 0, 2, 1.0 });
  int callback_count = 0;
  const FeatureReevaluateFn callback = [&](const FeatureReevaluationRequest& request, FeatureSupportSample* sample,
                                           std::string*) {
    ++callback_count;
    *sample = regular_sample(100 + static_cast<uint64_t>(callback_count), request.coordinates[0], false);
    sample->provenance = request.provenance;
    return true;
  };

  const FeatureDiscoveryResult result = DiscoverFeatures(batch, {}, callback);
  EXPECT_EQ(Candidate(result, FeatureMechanism::kInteriorRankLoss), nullptr);
  EXPECT_GT(callback_count, 0);
  EXPECT_LE(callback_count, FeatureDiscoveryOptions{}.maximum_refinement_steps);
}

TEST(FeatureDiscoveryDifferential, ReportsIncompleteWhenAFoldCallbackExhaustsItsBudget) {
  FeatureSupportBatch batch;
  batch.coordinate_dimension = 1;
  batch.visited_row_count = 1;
  batch.complete_visit = true;
  auto fold_sample = [](uint64_t id, double x, bool accumulates_measure) {
    FeatureSupportSample sample = Sample(id, x);
    const double phase = x * x;
    sample.accumulates_measure = accumulates_measure;
    sample.direction[0] = std::cos(phase);
    sample.direction[1] = std::sin(phase);
    sample.direction_jacobian = { -2.0 * x * std::sin(phase), 2.0 * x * std::cos(phase), 0.0 };
    return sample;
  };
  batch.samples = { fold_sample(1, 0.25, true), fold_sample(2, -0.75, false), fold_sample(3, 1.25, false) };
  batch.cell_axes.push_back({ 1, 0, 1, 0, 2, 2.0 });
  int callback_count = 0;
  const FeatureReevaluateFn callback = [&](const FeatureReevaluationRequest& request, FeatureSupportSample* sample,
                                           std::string*) {
    ++callback_count;
    *sample = fold_sample(100, request.coordinates[0], false);
    sample->provenance = request.provenance;
    sample->direction_jacobian_available = false;
    sample->direction_jacobian.clear();
    sample->direction_jacobian_column_available.clear();
    return true;
  };
  FeatureDiscoveryOptions options;
  options.maximum_refinement_steps = 1;

  const FeatureDiscoveryResult result = DiscoverFeatures(batch, options, callback);
  EXPECT_EQ(Candidate(result, FeatureMechanism::kInteriorRankLoss), nullptr);
  EXPECT_EQ(callback_count, 1);
  EXPECT_EQ(result.mechanisms.front().status, FeatureEvidenceStatus::kNumericalIncomplete);
}

TEST(FeatureDiscoveryDifferential, PositiveMeasureConstantBranchIsAnAtomNotRankLossEverywhere) {
  FeatureSupportBatch batch;
  batch.coordinate_dimension = 1;
  batch.visited_row_count = 1;
  batch.complete_visit = true;
  for (int index = 0; index < 3; ++index) {
    FeatureSupportSample sample = Sample(static_cast<uint64_t>(index + 1), static_cast<double>(index - 1));
    sample.accumulates_measure = index == 1;
    sample.direction_jacobian = { 0.0, 0.0, 0.0 };
    batch.samples.push_back(std::move(sample));
  }
  batch.cell_axes.push_back({ 0, 0, 0, 1, 2, 2.0 });
  batch.samples[1].mapping_evidence_kind = MappingEvidenceKind::kExactImageDimensionUpperBound;
  batch.samples[1].image_dimension_upper_bound = 0;

  const FeatureDiscoveryResult result = DiscoverFeatures(batch, {});
  EXPECT_EQ(Candidate(result, FeatureMechanism::kInteriorRankLoss), nullptr);
  const FeatureCandidate* atom = Candidate(result, FeatureMechanism::kMeasureAtom);
  ASSERT_NE(atom, nullptr);
  EXPECT_EQ(atom->status, FeatureEvidenceStatus::kConfirmed);
  ASSERT_NE(Candidate(result, FeatureMechanism::kStrictConfinement), nullptr);
}

TEST(FeatureDiscoveryDifferential, AxisRankZeroDoesNotProveAContinuousAtomOnAJointSupport) {
  FeatureSupportBatch batch;
  batch.coordinate_dimension = 2;
  batch.visited_row_count = 1;
  batch.complete_visit = true;
  auto joint_sample = [](uint64_t id, double x, double y, bool accumulates_measure) {
    FeatureSupportSample sample;
    sample.sample_id = id;
    sample.support_dimension = 2;
    sample.coordinates = { x, y };
    sample.active_coordinates = { 0, 1 };
    const double phase = x * x * y * y;
    const double slopes[2] = { 2.0 * x * y * y, 2.0 * y * x * x };
    sample.direction[0] = std::cos(phase);
    sample.direction[1] = std::sin(phase);
    sample.weight = accumulates_measure ? 1.0 : 0.0;
    sample.accumulates_measure = accumulates_measure;
    sample.direction_jacobian_available = true;
    sample.direction_jacobian = { -std::sin(phase) * slopes[0],
                                  -std::sin(phase) * slopes[1],
                                  std::cos(phase) * slopes[0],
                                  std::cos(phase) * slopes[1],
                                  0.0,
                                  0.0 };
    sample.direction_jacobian_column_available = { 1, 1 };
    sample.direction_jacobian_resolution = 1e-4;
    return sample;
  };
  batch.samples = { joint_sample(1, 0.0, 0.0, true), joint_sample(2, -1.0, 0.0, false),
                    joint_sample(3, 1.0, 0.0, false), joint_sample(4, 0.0, -1.0, false),
                    joint_sample(5, 0.0, 1.0, false) };
  batch.cell_axes = { { 0, 0, 1, 0, 2, 2.0 }, { 0, 1, 3, 0, 4, 2.0 } };
  int callback_count = 0;
  const FeatureReevaluateFn callback = [&](const FeatureReevaluationRequest& request, FeatureSupportSample* sample,
                                           std::string*) {
    ++callback_count;
    *sample = joint_sample(100 + static_cast<uint64_t>(callback_count), request.coordinates[0], request.coordinates[1],
                           false);
    sample->provenance = request.provenance;
    return true;
  };

  const FeatureDiscoveryResult result = DiscoverFeatures(batch, {}, callback);
  EXPECT_EQ(Candidate(result, FeatureMechanism::kMeasureAtom), nullptr);
  EXPECT_GT(callback_count, 0) << "joint support search must inspect coordinates away from the axial probes";
}

TEST(FeatureDiscoveryDifferential, RefinesAMultidimensionalRankLossUsingTheCompleteTangentMap) {
  FeatureSupportBatch batch;
  batch.coordinate_dimension = 2;
  batch.visited_row_count = 1;
  batch.complete_visit = true;
  auto fold_sample = [](uint64_t id, double u, double v, bool accumulates_measure) {
    FeatureSupportSample sample;
    sample.sample_id = id;
    sample.support_dimension = 2;
    sample.coordinates = { u, v };
    sample.active_coordinates = { 0, 1 };
    const double phase = v * v;
    sample.direction[0] = std::cos(u) * std::cos(phase);
    sample.direction[1] = std::sin(u) * std::cos(phase);
    sample.direction[2] = std::sin(phase);
    sample.weight = accumulates_measure ? 1.0 : 0.0;
    sample.accumulates_measure = accumulates_measure;
    sample.direction_jacobian_available = true;
    sample.direction_jacobian = {
      -std::sin(u) * std::cos(phase),
      -2.0 * v * std::cos(u) * std::sin(phase),
      std::cos(u) * std::cos(phase),
      -2.0 * v * std::sin(u) * std::sin(phase),
      0.0,
      2.0 * v * std::cos(phase),
    };
    sample.direction_jacobian_column_available = { 1, 1 };
    sample.direction_jacobian_resolution = 1e-4;
    return sample;
  };
  batch.samples = { fold_sample(1, 0.2, 0.25, true), fold_sample(2, -0.8, 0.25, false),
                    fold_sample(3, 1.2, 0.25, false), fold_sample(4, 0.2, -0.75, false),
                    fold_sample(5, 0.2, 1.25, false) };
  batch.cell_axes = { { 0, 0, 1, 0, 2, 2.0 }, { 0, 1, 3, 0, 4, 2.0 } };
  int callback_count = 0;
  const FeatureReevaluateFn callback = [&](const FeatureReevaluationRequest& request, FeatureSupportSample* sample,
                                           std::string*) {
    ++callback_count;
    *sample =
        fold_sample(100 + static_cast<uint64_t>(callback_count), request.coordinates[0], request.coordinates[1], false);
    sample->provenance = request.provenance;
    return true;
  };

  const FeatureDiscoveryResult result = DiscoverFeatures(batch, {}, callback);
  const FeatureCandidate* rank_loss = Candidate(result, FeatureMechanism::kInteriorRankLoss);
  ASSERT_NE(rank_loss, nullptr);
  EXPECT_EQ(rank_loss->status, FeatureEvidenceStatus::kConfirmed);
  EXPECT_EQ(rank_loss->mapping_rank, 1);
  EXPECT_NEAR(rank_loss->direction[0], std::cos(0.2), 1e-10);
  EXPECT_NEAR(rank_loss->direction[1], std::sin(0.2), 1e-10);
  EXPECT_NEAR(rank_loss->direction[2], 0.0, 1e-10);
  EXPECT_GT(callback_count, 0);
  EXPECT_LE(callback_count, FeatureDiscoveryOptions{}.maximum_refinement_steps);
}

TEST(FeatureDiscoveryDifferential, LocalizesAnOffAxisRankLossWithoutACenterOrEndpointBracket) {
  auto sample_at = [](uint64_t id, double u, double v, bool accumulates_measure) {
    const double graph = ((u - 0.5) * (u - 0.5) - 0.04) * v + (v - 0.5) * (v - 0.5) * (v - 0.5) / 3.0;
    const double graph_u = 2.0 * (u - 0.5) * v;
    const double graph_v = (u - 0.5) * (u - 0.5) + (v - 0.5) * (v - 0.5) - 0.04;
    return GraphSample(id, u, v, graph, graph_u, graph_v, accumulates_measure);
  };
  FeatureSupportBatch batch;
  batch.coordinate_dimension = 2;
  batch.visited_row_count = 1;
  batch.complete_visit = true;
  batch.samples = { sample_at(1, 0.0, 0.0, true), sample_at(2, -1.0, 0.0, false), sample_at(3, 1.0, 0.0, false),
                    sample_at(4, 0.0, -1.0, false), sample_at(5, 0.0, 1.0, false) };
  batch.cell_axes = { { 0, 0, 1, 0, 2, 2.0 }, { 0, 1, 3, 0, 4, 2.0 } };
  int callback_count = 0;
  std::vector<std::pair<double, double>> callback_coordinates;
  const FeatureReevaluateFn callback = [&](const FeatureReevaluationRequest& request, FeatureSupportSample* sample,
                                           std::string*) {
    ++callback_count;
    callback_coordinates.emplace_back(request.coordinates[0], request.coordinates[1]);
    *sample =
        sample_at(100 + static_cast<uint64_t>(callback_count), request.coordinates[0], request.coordinates[1], false);
    sample->provenance = request.provenance;
    return true;
  };
  FeatureDiscoveryOptions default_options;
  const FeatureDiscoveryResult default_result = DiscoverFeatures(batch, default_options, callback);
  const FeatureCandidate* default_rank_loss = Candidate(default_result, FeatureMechanism::kInteriorRankLoss);
  if (default_rank_loss == nullptr) {
    EXPECT_EQ(default_result.mechanisms[static_cast<size_t>(FeatureMechanism::kInteriorRankLoss)].status,
              FeatureEvidenceStatus::kNotDetectedAtResolution);
    ASSERT_EQ(default_result.coverage.size(), 1u);
    EXPECT_EQ(default_result.coverage[0].covered_subcell_count, default_result.coverage[0].total_subcell_count);
  }
  EXPECT_TRUE(std::any_of(callback_coordinates.begin(), callback_coordinates.end(), [](const auto& coordinates) {
    return std::fabs(coordinates.first) > 1e-12 && std::fabs(coordinates.second) > 1e-12;
  }));
  EXPECT_LE(callback_count, default_options.maximum_refinement_steps);

  callback_count = 0;
  callback_coordinates.clear();
  FeatureDiscoveryOptions options;
  options.maximum_refinement_steps = 512;

  const FeatureDiscoveryResult result = DiscoverFeatures(batch, options, callback);
  const FeatureCandidate* rank_loss = Candidate(result, FeatureMechanism::kInteriorRankLoss);
  ASSERT_NE(rank_loss, nullptr);
  EXPECT_EQ(rank_loss->status, FeatureEvidenceStatus::kConfirmed);
  EXPECT_LT(rank_loss->residual, 1e-4);
  EXPECT_TRUE(std::any_of(callback_coordinates.begin(), callback_coordinates.end(), [&](const auto& coordinates) {
    const double u = coordinates.first;
    const double v = coordinates.second;
    if (std::fabs((u - 0.5) * (u - 0.5) + (v - 0.5) * (v - 0.5) - 0.04) >= 1e-4) {
      return false;
    }
    const FeatureSupportSample expected = sample_at(0, u, v, false);
    double error2 = 0.0;
    for (int component = 0; component < 3; ++component) {
      const double delta = rank_loss->direction[component] - expected.direction[component];
      error2 += delta * delta;
    }
    return error2 < 1e-8;
  }));
  EXPECT_GT(callback_count, 0);
  EXPECT_LE(callback_count, options.maximum_refinement_steps);
  ASSERT_EQ(result.coverage.size(), 1u);
  EXPECT_EQ(result.coverage[0].status, rank_loss->status);
  EXPECT_NE(result.coverage[0].status, FeatureEvidenceStatus::kNotDetectedAtResolution);
  EXPECT_EQ(result.coverage[0].incomplete_reason, FeatureCoverageIncompleteReason::kNone);
}

TEST(FeatureDiscoveryDifferential, LocalizesAnEvenMinorRankLossWithoutASignChange) {
  auto sample_at = [](uint64_t id, double u, double v, bool accumulates_measure) {
    return GraphSample(id, u, v, v * v * v, 0.0, 3.0 * v * v, accumulates_measure);
  };
  FeatureSupportBatch batch;
  batch.coordinate_dimension = 2;
  batch.visited_row_count = 1;
  batch.complete_visit = true;
  batch.samples = { sample_at(1, 0.25, 0.25, true), sample_at(2, -0.75, 0.25, false), sample_at(3, 1.25, 0.25, false),
                    sample_at(4, 0.25, -0.75, false), sample_at(5, 0.25, 1.25, false) };
  batch.cell_axes = { { 0, 0, 1, 0, 2, 2.0 }, { 0, 1, 3, 0, 4, 2.0 } };
  int callback_count = 0;
  const FeatureReevaluateFn callback = [&](const FeatureReevaluationRequest& request, FeatureSupportSample* sample,
                                           std::string*) {
    ++callback_count;
    *sample =
        sample_at(100 + static_cast<uint64_t>(callback_count), request.coordinates[0], request.coordinates[1], false);
    sample->provenance = request.provenance;
    return true;
  };
  FeatureDiscoveryOptions options;
  options.maximum_refinement_steps = 512;

  const FeatureDiscoveryResult result = DiscoverFeatures(batch, options, callback);
  const FeatureCandidate* rank_loss = Candidate(result, FeatureMechanism::kInteriorRankLoss);
  ASSERT_NE(rank_loss, nullptr);
  EXPECT_EQ(rank_loss->status, FeatureEvidenceStatus::kConfirmed);
  EXPECT_LT(rank_loss->residual, 1e-10);
  EXPECT_NEAR(rank_loss->direction[2], 0.0, 1e-10);
  EXPECT_GT(callback_count, 0);
  EXPECT_LE(callback_count, options.maximum_refinement_steps);
}

TEST(FeatureDiscoveryDifferential, LocalizesAStationaryCubicWithoutASignChange) {
  auto sample_at = [](uint64_t id, double x, bool accumulates_measure) {
    FeatureSupportSample sample = Sample(id, x);
    const double phase = x * x * x;
    const double slope = 3.0 * x * x;
    sample.accumulates_measure = accumulates_measure;
    sample.weight = accumulates_measure ? 1.0 : 0.0;
    sample.direction[0] = std::cos(phase);
    sample.direction[1] = std::sin(phase);
    sample.direction_jacobian = { -slope * std::sin(phase), slope * std::cos(phase), 0.0 };
    return sample;
  };
  FeatureSupportBatch batch;
  batch.coordinate_dimension = 1;
  batch.visited_row_count = 1;
  batch.complete_visit = true;
  batch.samples = { sample_at(1, 0.1, true), sample_at(2, -0.3, false), sample_at(3, 0.5, false) };
  batch.cell_axes.push_back({ 0, 0, 1, 0, 2, 0.8 });
  int callback_count = 0;
  const FeatureReevaluateFn callback = [&](const FeatureReevaluationRequest& request, FeatureSupportSample* sample,
                                           std::string*) {
    ++callback_count;
    *sample = sample_at(100 + static_cast<uint64_t>(callback_count), request.coordinates[0], false);
    sample->provenance = request.provenance;
    return true;
  };
  FeatureDiscoveryOptions options;
  options.maximum_refinement_steps = 512;

  const FeatureDiscoveryResult result = DiscoverFeatures(batch, options, callback);
  const FeatureCandidate* rank_loss = Candidate(result, FeatureMechanism::kInteriorRankLoss);
  ASSERT_NE(rank_loss, nullptr);
  EXPECT_EQ(rank_loss->status, FeatureEvidenceStatus::kConfirmed);
  EXPECT_LT(rank_loss->residual, 1e-10);
  EXPECT_NEAR(rank_loss->direction[1], 0.0, 1e-10);
  EXPECT_GT(callback_count, 0);
  EXPECT_LE(callback_count, options.maximum_refinement_steps);
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
  const std::vector<const FeatureCandidate*> atoms = Candidates(result, FeatureMechanism::kMeasureAtom);
  ASSERT_EQ(atoms.size(), 2u);
  EXPECT_DOUBLE_EQ(atoms[0]->weighted_mass + atoms[1]->weighted_mass, 0.5);
  EXPECT_EQ(atoms[0]->status, FeatureEvidenceStatus::kConfirmed);
  EXPECT_EQ(atoms[1]->status, FeatureEvidenceStatus::kConfirmed);
  EXPECT_NE(atoms[0]->evidence_id, atoms[1]->evidence_id);
}

TEST(FeatureDiscoveryDifferential, RecordsJointGridCoverageAndDistinguishesMissingCallback) {
  const auto regular_sample = [](uint64_t id, double u, double v, bool accumulates_measure) {
    return GraphSample(id, u, v, 0.0, 0.0, 1.0, accumulates_measure);
  };
  FeatureSupportBatch batch;
  batch.coordinate_dimension = 2;
  batch.visited_row_count = 1;
  batch.complete_visit = true;
  batch.samples = { regular_sample(1, 0.0, 0.0, true), regular_sample(2, -1.0, 0.0, false),
                    regular_sample(3, 1.0, 0.0, false), regular_sample(4, 0.0, -1.0, false),
                    regular_sample(5, 0.0, 1.0, false) };
  batch.cell_axes = { { 7, 0, 1, 0, 2, 2.0 }, { 7, 1, 3, 0, 4, 2.0 } };
  batch.parameter_descriptors = { { FeatureParameterRole::kShape, 0 }, { FeatureParameterRole::kPose, 0 } };
  batch.scopes = { { 42, 7, FeatureSupportScopeKind::kJoint } };

  const FeatureDiscoveryResult missing = DiscoverFeatures(batch, {});
  ASSERT_EQ(missing.coverage.size(), 1u);
  EXPECT_EQ(missing.coverage[0].status, FeatureEvidenceStatus::kNumericalIncomplete);
  EXPECT_EQ(missing.coverage[0].incomplete_reason, FeatureCoverageIncompleteReason::kNoCallback);
  EXPECT_EQ(missing.coverage[0].materialized_node_count, 5);
  EXPECT_EQ(missing.mechanisms[0].status, FeatureEvidenceStatus::kNumericalIncomplete);

  std::vector<std::pair<double, double>> queries;
  const FeatureReevaluateFn callback = [&](const FeatureReevaluationRequest& request, FeatureSupportSample* sample,
                                           std::string*) {
    queries.emplace_back(request.coordinates[0], request.coordinates[1]);
    *sample = regular_sample(100 + queries.size(), request.coordinates[0], request.coordinates[1], false);
    sample->provenance = request.provenance;
    return true;
  };
  FeatureDiscoveryOptions options;
  options.maximum_refinement_steps = 24;
  const FeatureDiscoveryResult covered = DiscoverFeatures(batch, options, callback);
  ASSERT_EQ(covered.coverage.size(), 1u);
  const FeatureCoverageRecord& record = covered.coverage[0];
  EXPECT_EQ(record.cell_id, 7);
  EXPECT_EQ(record.scope_id, 42);
  EXPECT_EQ(record.active_coordinates, std::vector<int>({ 0, 1 }));
  EXPECT_EQ(record.lower_bounds, std::vector<double>({ -1.0, -1.0 }));
  EXPECT_EQ(record.upper_bounds, std::vector<double>({ 1.0, 1.0 }));
  EXPECT_EQ(record.status, FeatureEvidenceStatus::kNotDetectedAtResolution);
  EXPECT_EQ(record.incomplete_reason, FeatureCoverageIncompleteReason::kNone);
  EXPECT_EQ(record.covered_subcell_count, record.total_subcell_count);
  EXPECT_LE(record.callback_query_count, record.callback_budget);
  for (bool positive_u : { false, true }) {
    for (bool positive_v : { false, true }) {
      EXPECT_NE(std::find_if(queries.begin(), queries.end(),
                             [&](const auto& query) {
                               return (query.first > 0.0) == positive_u && (query.second > 0.0) == positive_v;
                             }),
                queries.end());
    }
  }
}

TEST(FeatureDiscoveryDifferential, DistinguishesUnavailableRankEvidenceFromCallbackFailure) {
  FeatureSupportBatch batch;
  batch.coordinate_dimension = 1;
  batch.visited_row_count = 1;
  batch.complete_visit = true;
  batch.samples = { Sample(1, 0.0), Sample(2, -1.0), Sample(3, 1.0) };
  for (FeatureSupportSample& sample : batch.samples) {
    sample.direction_jacobian = { 0.0, 0.0, 0.0 };
    sample.accumulates_measure = sample.sample_id == 1;
  }
  batch.cell_axes = { { 7, 0, 1, 0, 2, 2.0 } };
  int callback_count = 0;
  const FeatureReevaluateFn callback = [&](const FeatureReevaluationRequest&, FeatureSupportSample*, std::string*) {
    ++callback_count;
    return true;
  };

  const FeatureDiscoveryResult result = DiscoverFeatures(batch, {}, callback);
  ASSERT_EQ(result.coverage.size(), 1u);
  EXPECT_EQ(callback_count, 0);
  EXPECT_EQ(result.coverage[0].status, FeatureEvidenceStatus::kNumericalIncomplete);
  EXPECT_EQ(result.coverage[0].incomplete_reason, FeatureCoverageIncompleteReason::kEvidenceUnavailable);
}

TEST(FeatureDiscoveryDifferential, CurrentVersionImplicitCellUsesItsOwnJointScope) {
  FeatureSupportBatch batch;
  batch.coordinate_dimension = 1;
  batch.visited_row_count = 1;
  batch.complete_visit = true;
  batch.samples = { Sample(1, 0.0), Sample(2, -1.0), Sample(3, 1.0) };
  batch.samples[0].mapping_evidence_kind = MappingEvidenceKind::kExactImageDimensionUpperBound;
  batch.samples[0].image_dimension_upper_bound = 0;
  for (size_t index = 1; index < batch.samples.size(); ++index) {
    batch.samples[index].accumulates_measure = false;
    batch.samples[index].weight = 0.0;
  }
  batch.cell_axes = { { 7, 0, 1, 0, 2, 2.0 } };

  const FeatureDiscoveryResult result = DiscoverFeatures(batch, {});
  ASSERT_EQ(result.coverage.size(), 1u);
  EXPECT_EQ(result.coverage[0].scope_id, 7);
  for (FeatureMechanism mechanism : { FeatureMechanism::kMeasureAtom, FeatureMechanism::kStrictConfinement }) {
    const FeatureCandidate* candidate = Candidate(result, mechanism);
    if (candidate == nullptr) {
      ADD_FAILURE() << "missing candidate for mechanism " << static_cast<int>(mechanism);
      continue;
    }
    EXPECT_EQ(candidate->scope_id, 7);
    EXPECT_EQ(candidate->scope_kind, FeatureSupportScopeKind::kJoint);
    if (candidate->scope_parameters.size() != 1u) {
      ADD_FAILURE() << "unexpected scope parameter count for mechanism " << static_cast<int>(mechanism) << ": "
                    << candidate->scope_parameters.size();
      continue;
    }
    EXPECT_EQ(candidate->scope_parameters[0].role, FeatureParameterRole::kUnspecified);
  }
}

TEST(FeatureDiscoveryDifferential, ImplicitCellScopeDoesNotAliasAnExplicitScopeId) {
  FeatureSupportBatch batch;
  batch.coordinate_dimension = 1;
  batch.visited_row_count = 2;
  batch.complete_visit = true;
  batch.samples = { Sample(1, 0.0), Sample(2, -1.0), Sample(3, 1.0), Sample(4, 0.0), Sample(5, -1.0), Sample(6, 1.0) };
  for (size_t index = 0; index < batch.samples.size(); ++index) {
    FeatureSupportSample& sample = batch.samples[index];
    sample.provenance.member_index = static_cast<int>(index / 3u);
    sample.accumulates_measure = index % 3u == 0u;
    sample.weight = sample.accumulates_measure ? 1.0 : 0.0;
  }
  for (size_t index : { 0u, 3u }) {
    batch.samples[index].mapping_evidence_kind = MappingEvidenceKind::kExactImageDimensionUpperBound;
    batch.samples[index].image_dimension_upper_bound = 0;
  }
  batch.cell_axes = { { 7, 0, 1, 0, 2, 2.0 }, { 42, 0, 4, 3, 5, 2.0 } };
  batch.scopes = { { 42, 7, FeatureSupportScopeKind::kJoint } };

  const FeatureDiscoveryResult result = DiscoverFeatures(batch, {});
  ASSERT_EQ(result.coverage.size(), 2u);
  EXPECT_EQ(result.coverage[0].cell_id, 7);
  EXPECT_EQ(result.coverage[0].scope_id, 42);
  EXPECT_EQ(result.coverage[1].cell_id, 42);
  EXPECT_GE(result.coverage[1].scope_id, 0);
  EXPECT_NE(result.coverage[1].scope_id, result.coverage[0].scope_id);
}

TEST(FeatureDiscoveryDifferential, ExactMappingProofDoesNotClaimCompleteSupportCoverage) {
  FeatureSupportBatch batch;
  batch.coordinate_dimension = 1;
  batch.visited_row_count = 1;
  batch.complete_visit = true;
  batch.samples = { Sample(1, 0.0), Sample(2, -1.0), Sample(3, 1.0) };
  batch.samples[0].mapping_evidence_kind = MappingEvidenceKind::kExactImageDimensionUpperBound;
  batch.samples[0].image_dimension_upper_bound = 0;
  batch.samples[1].constraints[0].value = -1.0;
  for (size_t index = 1; index < batch.samples.size(); ++index) {
    batch.samples[index].accumulates_measure = false;
    batch.samples[index].weight = 0.0;
  }
  batch.cell_axes = { { 7, 0, 1, 0, 2, 2.0 } };
  batch.edges = { { 1, 0, 1.0 }, { 0, 2, 1.0 } };

  const FeatureDiscoveryResult result = DiscoverFeatures(batch, {});
  const FeatureCandidate* atom = Candidate(result, FeatureMechanism::kMeasureAtom);
  ASSERT_NE(atom, nullptr);
  EXPECT_EQ(atom->status, FeatureEvidenceStatus::kConfirmed);
  EXPECT_NE(atom->reason.find("reachable support"), std::string::npos);
  ASSERT_EQ(result.coverage.size(), 1u);
  EXPECT_EQ(result.coverage[0].status, FeatureEvidenceStatus::kNumericalIncomplete);
  EXPECT_EQ(result.coverage[0].incomplete_reason, FeatureCoverageIncompleteReason::kSupportBoundary);
  EXPECT_NE(Candidate(result, FeatureMechanism::kSupportBoundary), nullptr);
}

TEST(FeatureDiscoveryDifferential, CurrentVersionImplicitScopesHaveDistinctOrigins) {
  FeatureSupportBatch batch =
      DenseSkyBatch(8, 16, [](int z, int azimuth) { return (z == 3 && azimuth == 8) ? 20.0 : 1.0; });
  batch.coordinate_dimension = 1;
  batch.parameter_descriptors = { { FeatureParameterRole::kSpectrum, -1 } };
  for (FeatureSupportSample& sample : batch.samples) {
    sample.coordinates = { 550.0 };
  }

  const FeatureDiscoveryResult result = DiscoverFeatures(batch, {});
  ASSERT_FALSE(Candidates(result, FeatureMechanism::kMeasureAtom).empty());
  for (const FeatureCandidate* atom : Candidates(result, FeatureMechanism::kMeasureAtom)) {
    EXPECT_EQ(atom->scope_id, kPointMeasureFeatureScopeId);
  }
  ASSERT_FALSE(Candidates(result, FeatureMechanism::kBrightnessMaximum).empty());
  for (FeatureMechanism mechanism : { FeatureMechanism::kBrightnessMaximum, FeatureMechanism::kBrightnessRidge }) {
    for (const FeatureCandidate* candidate : Candidates(result, mechanism)) {
      EXPECT_EQ(candidate->scope_id, kFullSceneFeatureScopeId);
    }
  }
}

TEST(FeatureDiscoveryConstraints, RefinesANonFirstInterfaceKinkThroughTheCallerCallback) {
  FeatureSupportBatch batch;
  batch.coordinate_dimension = 1;
  batch.complete_visit = true;
  batch.samples = { Sample(1, -1.0), Sample(2, 1.0) };
  batch.visited_row_count = batch.samples.size();
  batch.edges.push_back({ 0, 1, 2.0 });
  for (FeatureSupportSample& sample : batch.samples) {
    sample.constraints.clear();
    SupportConstraint tir;
    tir.name = "layer[1].internal[2].tir";
    tir.kind = ConstraintKind::kTir;
    tir.layer_index = 1;
    tir.interface_index = 2;
    tir.value = sample.coordinates[0];
    sample.constraints.push_back(std::move(tir));
  }
  const FeatureReevaluateFn callback = [](const FeatureReevaluationRequest& request, FeatureSupportSample* sample,
                                          std::string*) {
    *sample = Sample(100, request.coordinates[0]);
    sample->accumulates_measure = false;
    sample->provenance = request.provenance;
    sample->constraints.clear();
    sample->direction[0] = std::cos(request.coordinates[0]);
    sample->direction[1] = std::sin(request.coordinates[0]);
    SupportConstraint tir;
    tir.name = "layer[1].internal[2].tir";
    tir.kind = ConstraintKind::kTir;
    tir.layer_index = 1;
    tir.interface_index = 2;
    tir.value = request.coordinates[0];
    sample->constraints.push_back(std::move(tir));
    return true;
  };

  const FeatureDiscoveryResult result = DiscoverFeatures(batch, {}, callback);
  const FeatureCandidate* kink = Candidate(result, FeatureMechanism::kOpticalKink);
  ASSERT_NE(kink, nullptr);
  EXPECT_EQ(kink->status, FeatureEvidenceStatus::kConfirmed);
  EXPECT_EQ(kink->provenance.layer_index, 1);
  EXPECT_EQ(kink->provenance.interface_index, 2);
  EXPECT_LE(kink->residual, 1e-8);
  EXPECT_NEAR(kink->direction[0], 1.0, 1e-12);
}

TEST(FeatureDiscoveryConstraints, RejectsMismatchedOrMalformedCallbackSamples) {
  enum class Defect { kWrongBranch, kWrongCoordinates, kWrongInterface, kMissingMargin, kNonFiniteDirection };
  for (Defect defect : { Defect::kWrongBranch, Defect::kWrongCoordinates, Defect::kWrongInterface,
                         Defect::kMissingMargin, Defect::kNonFiniteDirection }) {
    FeatureSupportBatch batch;
    batch.coordinate_dimension = 1;
    batch.complete_visit = true;
    batch.samples = { Sample(1, -1.0), Sample(2, 1.0) };
    batch.visited_row_count = batch.samples.size();
    batch.edges.push_back({ 0, 1, 2.0 });
    for (FeatureSupportSample& sample : batch.samples) {
      sample.constraints[0].name = "domain.actual";
      sample.constraints[0].kind = ConstraintKind::kDomain;
      sample.constraints[0].layer_index = 2;
      sample.constraints[0].interface_index = 3;
      sample.constraints[0].value = sample.coordinates[0];
    }
    const FeatureReevaluateFn callback = [defect](const FeatureReevaluationRequest& request,
                                                  FeatureSupportSample* sample, std::string*) {
      *sample = Sample(100, request.coordinates[0]);
      sample->accumulates_measure = false;
      sample->provenance = request.provenance;
      sample->constraints[0].name = "domain.actual";
      sample->constraints[0].kind = ConstraintKind::kDomain;
      sample->constraints[0].layer_index = 2;
      sample->constraints[0].interface_index = 3;
      sample->constraints[0].value = 0.0;
      if (defect == Defect::kWrongBranch) {
        ++sample->provenance.member_index;
      } else if (defect == Defect::kWrongCoordinates) {
        sample->coordinates[0] += 5.0;
      } else if (defect == Defect::kWrongInterface) {
        ++sample->constraints[0].interface_index;
      } else if (defect == Defect::kMissingMargin) {
        sample->constraints.clear();
      } else {
        sample->direction[0] = std::numeric_limits<double>::quiet_NaN();
      }
      return true;
    };

    const FeatureDiscoveryResult result = DiscoverFeatures(batch, {}, callback);
    const FeatureCandidate* boundary = Candidate(result, FeatureMechanism::kSupportBoundary);
    EXPECT_NE(boundary, nullptr);
    if (boundary != nullptr) {
      EXPECT_EQ(boundary->status, FeatureEvidenceStatus::kNumericalIncomplete);
    }
  }
}

TEST(FeatureDiscoveryConstraints, PreservesFilterSideWeightsAndDetectsExactCorners) {
  FeatureSupportBatch crossing;
  crossing.coordinate_dimension = 1;
  crossing.complete_visit = true;
  crossing.samples = { Sample(1, -1.0), Sample(2, 1.0) };
  crossing.visited_row_count = crossing.samples.size();
  crossing.edges.push_back({ 0, 1, 2.0 });
  crossing.samples[0].weight = 0.25;
  crossing.samples[1].weight = 0.75;
  for (FeatureSupportSample& sample : crossing.samples) {
    sample.constraints[0].name = "physical_filter";
    sample.constraints[0].kind = ConstraintKind::kFilter;
    sample.constraints[0].value = sample.coordinates[0];
  }
  const FeatureDiscoveryResult crossing_result = DiscoverFeatures(crossing, {});
  const FeatureCandidate* filter = Candidate(crossing_result, FeatureMechanism::kFilterBoundary);
  ASSERT_NE(filter, nullptr);
  EXPECT_EQ(filter->status, FeatureEvidenceStatus::kCandidate);
  ASSERT_TRUE(filter->has_weight_sides);
  EXPECT_DOUBLE_EQ(filter->weight_sides[0], 0.25);
  EXPECT_DOUBLE_EQ(filter->weight_sides[1], 0.75);

  FeatureSupportBatch corner_batch;
  corner_batch.coordinate_dimension = 1;
  corner_batch.complete_visit = true;
  corner_batch.visited_row_count = 1;
  FeatureSupportSample corner_sample = Sample(3, 0.0);
  corner_sample.constraints[0].value = 0.0;
  SupportConstraint second = corner_sample.constraints[0];
  second.name = "domain.second";
  second.kind = ConstraintKind::kDomain;
  corner_sample.constraints.push_back(std::move(second));
  corner_batch.samples.push_back(std::move(corner_sample));
  const FeatureDiscoveryResult corner_result = DiscoverFeatures(corner_batch, {});
  const FeatureCandidate* corner = Candidate(corner_result, FeatureMechanism::kSupportCorner);
  ASSERT_NE(corner, nullptr);
  EXPECT_EQ(corner->status, FeatureEvidenceStatus::kCandidate);
  EXPECT_EQ(corner->active_constraints.size(), 2u);
}

TEST(FeatureDiscoveryConcentration, RejectsDiffuseFiniteWidthAndKeepsStableNarrowMass) {
  auto make_batch = [](bool narrow) {
    FeatureSupportBatch batch;
    batch.coordinate_dimension = 1;
    batch.complete_visit = true;
    batch.materialization_complete = true;
    for (int index = 0; index < 8; ++index) {
      const double angle = narrow ? 0.002 * (index - 3.5) : index * 2.0 * 3.14159265358979323846 / 8.0;
      FeatureSupportSample sample;
      sample.sample_id = static_cast<uint64_t>(index + 1);
      sample.provenance.sample_index = index;
      sample.support_dimension = 1;
      sample.finite_width = true;
      sample.coordinates = { static_cast<double>(index) };
      sample.active_coordinates = { 0 };
      sample.direction[0] = std::cos(angle);
      sample.direction[1] = std::sin(angle);
      sample.weight = 0.125;
      batch.samples.push_back(std::move(sample));
    }
    batch.visited_row_count = batch.samples.size();
    return batch;
  };

  const FeatureDiscoveryResult diffuse = DiscoverFeatures(make_batch(false), {});
  EXPECT_EQ(Candidate(diffuse, FeatureMechanism::kFiniteWidthConcentration), nullptr);
  const FeatureDiscoveryResult narrow = DiscoverFeatures(make_batch(true), {});
  const FeatureCandidate* concentration = Candidate(narrow, FeatureMechanism::kFiniteWidthConcentration);
  ASSERT_NE(concentration, nullptr);
  EXPECT_EQ(concentration->status, FeatureEvidenceStatus::kConfirmed);
  EXPECT_LT(concentration->resolution, 0.01);
}

TEST(FeatureDiscoveryConcentration, AggregatesAcrossCellsWithoutInheritingTheFirstCellScope) {
  FeatureSupportBatch batch;
  batch.coordinate_dimension = 2;
  batch.complete_visit = true;
  batch.materialization_complete = true;
  for (int index = 0; index < 4; ++index) {
    FeatureSupportSample sample;
    sample.sample_id = static_cast<uint64_t>(index + 1);
    sample.provenance.member_index = 3;
    sample.provenance.spectrum_node_id = 5;
    sample.provenance.source_node_id = 7;
    sample.provenance.sample_index = index;
    sample.support_dimension = 1;
    sample.finite_width = true;
    sample.coordinates = { static_cast<double>(index), static_cast<double>(index) };
    sample.active_coordinates = { index % 2 };
    sample.direction[0] = 1.0;
    sample.direction[1] = 0.001 * static_cast<double>(index - 1);
    const double norm = std::hypot(sample.direction[0], sample.direction[1]);
    sample.direction[0] /= norm;
    sample.direction[1] /= norm;
    sample.weight = 1.0;
    batch.samples.push_back(std::move(sample));
  }
  batch.visited_row_count = batch.samples.size();

  const FeatureDiscoveryResult result = DiscoverFeatures(batch, {});
  const FeatureCandidate* concentration = Candidate(result, FeatureMechanism::kFiniteWidthConcentration);
  ASSERT_NE(concentration, nullptr);
  EXPECT_EQ(concentration->scope_id, kBranchAggregateFeatureScopeId);
  EXPECT_EQ(concentration->scope_kind, FeatureSupportScopeKind::kJoint);
  EXPECT_EQ(concentration->scope_active_coordinates, std::vector<int>({ 0, 1 }));
  EXPECT_EQ(concentration->provenance.sample_index, -1);
  EXPECT_DOUBLE_EQ(concentration->weighted_mass, 4.0);
}

TEST(FeatureDiscoverySkyField, FindsResolutionStableMaximumAndIsVisitOrderIndependent) {
  FeatureDiscoveryOptions options;
  options.sky_z_bins = 8;
  options.sky_azimuth_bins = 16;
  const auto density = [](int z_index, int azimuth_index) {
    const int azimuth_distance = std::min(std::abs(azimuth_index - 4), 16 - std::abs(azimuth_index - 4));
    return 400.0 - 7.0 * std::abs(z_index - 4) - 3.0 * azimuth_distance * azimuth_distance;
  };
  FeatureSupportBatch batch = DenseSkyBatch(options.sky_z_bins, options.sky_azimuth_bins, density);
  const FeatureDiscoveryResult forward = DiscoverFeatures(batch, options);
  const std::vector<const FeatureCandidate*> maxima = Candidates(forward, FeatureMechanism::kBrightnessMaximum);
  ASSERT_FALSE(maxima.empty());
  EXPECT_TRUE(std::any_of(maxima.begin(), maxima.end(), [](const FeatureCandidate* candidate) {
    return candidate->status == FeatureEvidenceStatus::kConfirmed;
  }));

  std::reverse(batch.samples.begin(), batch.samples.end());
  const FeatureDiscoveryResult reverse = DiscoverFeatures(batch, options);
  ASSERT_EQ(forward.sky_field.size(), reverse.sky_field.size());
  for (size_t index = 0; index < forward.sky_field.size(); ++index) {
    EXPECT_DOUBLE_EQ(forward.sky_field[index].value, reverse.sky_field[index].value);
    EXPECT_DOUBLE_EQ(forward.sky_field[index].normalized_value, reverse.sky_field[index].normalized_value);
  }
}

TEST(FeatureDiscoverySkyField, DistinguishesARidgeFromAnIsolatedMaximum) {
  FeatureDiscoveryOptions options;
  options.sky_z_bins = 8;
  options.sky_azimuth_bins = 16;
  const double z_profile[8] = { 1.0, 2.0, 2.0, 8.0, 10.0, 8.0, 2.0, 2.0 };
  const FeatureSupportBatch batch =
      DenseSkyBatch(options.sky_z_bins, options.sky_azimuth_bins, [&](int z_index, int) { return z_profile[z_index]; });
  const FeatureDiscoveryResult result = DiscoverFeatures(batch, options);
  EXPECT_EQ(Candidate(result, FeatureMechanism::kBrightnessMaximum), nullptr);
  const std::vector<const FeatureCandidate*> ridges = Candidates(result, FeatureMechanism::kBrightnessRidge);
  ASSERT_FALSE(ridges.empty());
  EXPECT_TRUE(std::any_of(ridges.begin(), ridges.end(), [](const FeatureCandidate* candidate) {
    return candidate->status == FeatureEvidenceStatus::kConfirmed;
  }));
}

TEST(FeatureDiscoverySkyField, RemovingOneSourceMovesTheSceneMaximum) {
  FeatureDiscoveryOptions options;
  options.sky_z_bins = 8;
  options.sky_azimuth_bins = 16;
  const auto source_density = [](int peak, double scale, int z_index, int azimuth_index) {
    const int azimuth_distance = std::min(std::abs(azimuth_index - peak), 16 - std::abs(azimuth_index - peak));
    return scale * (400.0 - 7.0 * std::abs(z_index - 4) - 3.0 * azimuth_distance * azimuth_distance);
  };
  FeatureSupportBatch dominant = DenseSkyBatch(
      8, 16, [&](int z_index, int azimuth_index) { return source_density(4, 2.0, z_index, azimuth_index); });
  FeatureSupportBatch secondary = DenseSkyBatch(
      8, 16, [&](int z_index, int azimuth_index) { return source_density(10, 1.0, z_index, azimuth_index); });
  for (FeatureSupportSample& sample : dominant.samples) {
    sample.provenance.source_node_id = 0;
  }
  for (FeatureSupportSample& sample : secondary.samples) {
    sample.sample_id += 1000;
    sample.provenance.source_node_id = 1;
  }
  FeatureSupportBatch combined = dominant;
  combined.samples.insert(combined.samples.end(), secondary.samples.begin(), secondary.samples.end());
  combined.visited_row_count = combined.samples.size();

  const auto strongest_maximum = [](const FeatureDiscoveryResult& result) {
    const auto maxima = Candidates(result, FeatureMechanism::kBrightnessMaximum);
    return **std::max_element(maxima.begin(), maxima.end(), [](const auto* first, const auto* second) {
      return first->weighted_mass < second->weighted_mass;
    });
  };
  const FeatureCandidate combined_maximum = strongest_maximum(DiscoverFeatures(combined, options));
  const FeatureCandidate secondary_maximum = strongest_maximum(DiscoverFeatures(secondary, options));
  EXPECT_GT(combined_maximum.direction[1], 0.0);
  EXPECT_LT(secondary_maximum.direction[1], 0.0);
  EXPECT_LT(combined_maximum.direction[0] * secondary_maximum.direction[0] +
                combined_maximum.direction[1] * secondary_maximum.direction[1] +
                combined_maximum.direction[2] * secondary_maximum.direction[2],
            0.5);
}

}  // namespace
}  // namespace lumice::analytic
