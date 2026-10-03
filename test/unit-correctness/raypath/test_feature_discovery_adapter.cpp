#include <gtest/gtest.h>

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
  crystal.axis_.latitude_dist = { DistributionType::kNoRandom, 90.0f, 0.0f };
  crystal.axis_.azimuth_dist = { DistributionType::kUniform, 0.0f, 360.0f };
  crystal.axis_.roll_dist = { DistributionType::kNoRandom, 0.0f, 0.0f };

  ConfigManager config;
  config.crystals_.emplace(1, crystal);
  config.scene_.light_source_.param_ = SunParam{ 20.0f, 0.0f, 0.0f };
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
  request.path_layers = { { 3, 6 } };
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

  std::string validation_error;
  EXPECT_TRUE(analytic::ValidateFeatureSupportBatch(batch, &validation_error)) << validation_error;
}

}  // namespace
}  // namespace lumice::raypath
