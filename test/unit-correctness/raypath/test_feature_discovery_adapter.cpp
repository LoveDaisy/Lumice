#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "raypath/feature_discovery_adapter.hpp"

namespace lumice::raypath {
namespace {

ConfigManager Scene() {
  PrismCrystalParam prism;
  prism.h_ = { DistributionType::kNoRandom, 1.0f, 0.0f };
  for (auto& distance : prism.d_) {
    distance = { DistributionType::kNoRandom, 1.0f, 0.0f };
  }
  CrystalConfig crystal;
  crystal.id_ = 1;
  crystal.param_ = prism;
  crystal.axis_.latitude_dist = { DistributionType::kUniform, 90.0f, 2.0f };
  crystal.axis_.azimuth_dist = { DistributionType::kNoRandom, 0.0f, 0.0f };
  crystal.axis_.roll_dist = { DistributionType::kNoRandom, 0.0f, 0.0f };

  ConfigManager config;
  config.crystals_.emplace(1, crystal);
  config.scene_.light_source_.param_ = SunParam{ 90.0f, 0.0f, 0.0f };
  config.scene_.max_hits_ = 2;
  config.scene_.light_source_.spectrum_ = std::vector<WlParam>{ { 550.0f, 1.0f } };
  ScatteringSetting setting{};
  setting.crystal_ = crystal;
  setting.crystal_proportion_ = 1.0f;
  MsInfo layer{};
  layer.setting_.push_back(std::move(setting));
  config.scene_.ms_.push_back(std::move(layer));
  return config;
}

TEST(FeatureDiscoveryAdapter, ConsumesEveryVisitorRowAndKeepsContinuousSupportDistinctFromAtoms) {
  SceneMeasureRequest request;
  request.layer_crystal_ids = { 1 };
  request.path_layers = { { 1 } };
  request.sample_count = 8;
  request.sun_node_count = 2;
  request.illuminant_node_count = 2;
  request.seed = 0x6494u;

  analytic::FeatureSupportBatch batch;
  SceneMeasureResult measure;
  const Error error = BuildFeatureSupportBatch(Scene(), request, &batch, &measure);
  ASSERT_TRUE(error.Ok()) << error.message;
  EXPECT_TRUE(batch.complete_visit);
  EXPECT_TRUE(batch.materialization_complete);
  EXPECT_EQ(batch.visited_row_count, static_cast<uint64_t>(measure.evaluated_row_count));
  EXPECT_EQ(batch.samples.size(), static_cast<size_t>(measure.evaluated_row_count));
  ASSERT_FALSE(batch.samples.empty());
  EXPECT_EQ(batch.samples.front().measure_kind, analytic::SupportMeasureKind::kContinuous);
  EXPECT_EQ(batch.samples.front().support_dimension, 1);
  EXPECT_TRUE(batch.samples.front().finite_width);
  EXPECT_GT(batch.coordinate_dimension, batch.samples.front().support_dimension);
  EXPECT_FALSE(batch.edges.empty());
  const int numerical_count = static_cast<int>(std::count_if(
      batch.samples.begin(), batch.samples.end(), [](const auto& sample) { return sample.numerically_available; }));
  const int jacobian_storage_count =
      static_cast<int>(std::count_if(batch.samples.begin(), batch.samples.end(),
                                     [](const auto& sample) { return !sample.direction_jacobian.empty(); }));
  EXPECT_TRUE(std::any_of(batch.samples.begin(), batch.samples.end(),
                          [](const auto& sample) {
                            return sample.direction_jacobian_available && sample.direction_jacobian_resolution > 0.0;
                          }))
      << "numerical=" << numerical_count << " jacobian_storage=" << jacobian_storage_count;

  std::string validation_error;
  EXPECT_TRUE(analytic::ValidateFeatureSupportBatch(batch, &validation_error)) << validation_error;

  analytic::FeatureDiscoveryOptions options;
  options.sky_z_bins = 4;
  options.sky_azimuth_bins = 8;
  const analytic::FeatureDiscoveryResult discovery = analytic::DiscoverFeatures(batch, options);
  EXPECT_EQ(discovery.visited_row_count, batch.visited_row_count);
  EXPECT_EQ(discovery.sky_field.size(), 32u);
  EXPECT_TRUE(std::any_of(discovery.candidates.begin(), discovery.candidates.end(), [](const auto& candidate) {
    return candidate.mechanism == analytic::FeatureMechanism::kFiniteWidthConcentration;
  })) << "generic discovery must run on the materialized scene support without a named-family whitelist";
}

TEST(FeatureDiscoveryAdapter, DifferentiatesTheComposedDirectionThroughBothLayers) {
  ConfigManager config = Scene();
  CrystalConfig second = config.crystals_.at(1);
  second.id_ = 2;
  config.crystals_.emplace(2, second);
  config.scene_.ms_.front().prob_ = 1.0f;
  ScatteringSetting setting{};
  setting.crystal_ = second;
  setting.crystal_proportion_ = 1.0f;
  MsInfo layer{};
  layer.setting_.push_back(std::move(setting));
  config.scene_.ms_.push_back(std::move(layer));

  SceneMeasureRequest request;
  request.layer_crystal_ids = { 1, 2 };
  request.path_layers = { { 1 }, { 2, 1 } };
  request.sample_count = 64;
  request.sun_node_count = 2;
  request.illuminant_node_count = 2;
  request.seed = 0x64942u;

  analytic::FeatureSupportBatch batch;
  SceneMeasureResult measure;
  const Error error = BuildFeatureSupportBatch(config, request, &batch, &measure);
  ASSERT_TRUE(error.Ok()) << error.message;
  const auto complete = std::find_if(batch.samples.begin(), batch.samples.end(), [](const auto& sample) {
    return sample.direction_jacobian_available && sample.active_coordinates.size() == 2u;
  });
  ASSERT_NE(complete, batch.samples.end()) << "a full-chain row must differentiate every active layer coordinate";
  double column_norm2[2]{};
  for (int column = 0; column < 2; ++column) {
    const int coordinate = complete->active_coordinates[static_cast<size_t>(column)];
    for (int component = 0; component < 3; ++component) {
      const double value = complete->direction_jacobian[static_cast<size_t>(component) * complete->coordinates.size() +
                                                        static_cast<size_t>(coordinate)];
      column_norm2[column] += value * value;
    }
  }
  EXPECT_GT(column_norm2[0], 1e-10)
      << "the first reflection must propagate through the second layer into the final direction";
  EXPECT_LT(column_norm2[1], 1e-8)
      << "the parallel-face second layer must preserve its incident direction for every pose";
  EXPECT_LT(complete->direction_jacobian_error, 1e-4);
}

}  // namespace
}  // namespace lumice::raypath
