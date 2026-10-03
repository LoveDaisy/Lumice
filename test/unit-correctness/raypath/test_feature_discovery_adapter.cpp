#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
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
  EXPECT_EQ(static_cast<int>(std::count_if(batch.samples.begin(), batch.samples.end(),
                                           [](const auto& sample) { return sample.accumulates_measure; })),
            measure.evaluated_row_count);
  ASSERT_FALSE(batch.samples.empty());
  EXPECT_EQ(batch.samples.front().measure_kind, analytic::SupportMeasureKind::kContinuous);
  EXPECT_EQ(batch.samples.front().support_dimension, 1);
  EXPECT_TRUE(batch.samples.front().finite_width);
  EXPECT_GT(batch.coordinate_dimension, batch.samples.front().support_dimension);
  EXPECT_FALSE(batch.edges.empty());
  EXPECT_FALSE(batch.cell_axes.empty());
  EXPECT_TRUE(std::all_of(batch.cell_axes.begin(), batch.cell_axes.end(), [&](const auto& axis) {
    return !batch.samples[static_cast<size_t>(axis.lower)].accumulates_measure &&
           batch.samples[static_cast<size_t>(axis.center)].accumulates_measure &&
           !batch.samples[static_cast<size_t>(axis.upper)].accumulates_measure;
  }));
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

TEST(FeatureDiscoveryAdapter, ReplaysTwentyOneIndependentShapeCoordinatesThroughThreeRealLayers) {
  ConfigManager config;
  config.scene_.light_source_.param_ = SunParam{ 90.0f, 0.0f, 0.0f };
  config.scene_.light_source_.spectrum_ = std::vector<WlParam>{ { 550.0f, 1.0f } };
  config.scene_.max_hits_ = 6;
  for (int layer_index = 0; layer_index < 3; ++layer_index) {
    PrismCrystalParam prism;
    prism.h_ = { DistributionType::kUniform, 1.0f, 0.05f };
    for (auto& distance : prism.d_) {
      distance = { DistributionType::kUniform, 1.0f, 0.02f };
    }
    CrystalConfig crystal;
    crystal.id_ = static_cast<IdType>(layer_index + 1);
    crystal.param_ = prism;
    crystal.axis_.latitude_dist = { DistributionType::kNoRandom, 90.0f, 0.0f };
    crystal.axis_.azimuth_dist = { DistributionType::kNoRandom, 0.0f, 0.0f };
    crystal.axis_.roll_dist = { DistributionType::kNoRandom, 0.0f, 0.0f };
    config.crystals_.emplace(crystal.id_, crystal);
    ScatteringSetting setting{};
    setting.crystal_ = crystal;
    setting.crystal_proportion_ = 1.0f;
    MsInfo layer{};
    layer.setting_.push_back(std::move(setting));
    layer.prob_ = layer_index < 2 ? 0.5f : 0.0f;
    config.scene_.ms_.push_back(std::move(layer));
  }

  SceneMeasureRequest request;
  request.layer_crystal_ids = { 1, 2, 3 };
  request.path_layers = { { 1, 2 }, { 1, 2 }, { 1, 2 } };
  request.sample_count = 8;
  request.seed = 0x64944u;

  std::vector<SceneMeasureRow> control_rows;
  SceneMeasureResult control_measure;
  const Error control_error = BuildSceneMeasure(
      config, request, [&](const SceneMeasureRow& row) { control_rows.push_back(row); }, &control_measure);
  ASSERT_TRUE(control_error.Ok()) << control_error.message;
  const auto control = std::find_if(control_rows.begin(), control_rows.end(), [](const auto& row) {
    return row.status == SceneMeasureStatus::kConfirmed &&
           row.contribution_status == SceneMeasureNumericStatus::kAvailable && row.contribution > 0.0;
  });
  ASSERT_NE(control, control_rows.end()) << control_measure.reason;
  ASSERT_EQ(control->layers.size(), 3u);
  for (const SceneMeasureLayerRow& layer : control->layers) {
    EXPECT_EQ(layer.faces, (std::vector<int>{ 1, 2 }));
    EXPECT_EQ(layer.status, SceneMeasureStatus::kConfirmed);
    EXPECT_EQ(layer.field.path_status, analytic::DiagnosticPathStatus::kOk);
    EXPECT_EQ(layer.field.entry_status, analytic::DiagnosticEntryStatus::kOk);
    EXPECT_GT(layer.entry_measure, 0.0);
    EXPECT_TRUE(std::all_of(layer.entries.begin(), layer.entries.end(),
                            [](const auto& entry) { return entry.filter_evaluated && entry.accepted; }));
    for (int component = 0; component < 3; ++component) {
      EXPECT_NEAR(layer.outgoing_direction[component], layer.incident_direction[component], 1e-12);
    }
  }

  analytic::FeatureSupportBatch batch;
  SceneMeasureResult measure;
  analytic::FeatureReevaluateFn reevaluate;
  const Error error = BuildFeatureSupportBatch(config, request, &batch, &measure, &reevaluate);
  ASSERT_TRUE(error.Ok()) << error.message;
  ASSERT_TRUE(reevaluate);
  EXPECT_EQ(batch.version, analytic::kFeatureSupportBatchVersion);
  EXPECT_EQ(batch.coordinate_dimension, 25);
  const auto positive = std::find_if(batch.samples.begin(), batch.samples.end(), [](const auto& sample) {
    return sample.accumulates_measure && sample.weight > 0.0 && sample.numerically_available;
  });
  ASSERT_NE(positive, batch.samples.end()) << measure.reason;
  EXPECT_EQ(positive->support_dimension, 21);
  ASSERT_TRUE(positive->direction_jacobian_available);
  for (int coordinate : positive->active_coordinates) {
    double norm2 = 0.0;
    for (int component = 0; component < 3; ++component) {
      const double value =
          positive->direction_jacobian[static_cast<size_t>(component * batch.coordinate_dimension + coordinate)];
      norm2 += value * value;
    }
    EXPECT_LT(norm2, 1e-8) << "parallel-face direction must not depend on shape coordinate " << coordinate;
  }
  EXPECT_TRUE(std::any_of(batch.cell_axes.begin(), batch.cell_axes.end(), [&](const auto& axis) {
    if (axis.center != static_cast<int>(std::distance(batch.samples.begin(), positive))) {
      return false;
    }
    const double lower = batch.samples[static_cast<size_t>(axis.lower)].weight;
    const double upper = batch.samples[static_cast<size_t>(axis.upper)].weight;
    return std::fabs(lower - upper) > 1e-12;
  })) << "native 2A/S weight must be recomputed when a sampled shape coordinate changes";

  analytic::FeatureReevaluationRequest invalid_request;
  invalid_request.provenance = positive->provenance;
  invalid_request.provenance.sample_index = -1;
  invalid_request.coordinates = positive->coordinates;
  analytic::FeatureSupportSample invalid_sample;
  std::string callback_error;
  EXPECT_FALSE(reevaluate(invalid_request, &invalid_sample, &callback_error));
  EXPECT_NE(callback_error.find("does not name a materialized product branch"), std::string::npos);
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

TEST(FeatureDiscoveryAdapter, PhysicalDirectionFilterCreatesItsOwnBoundaryMechanism) {
  SceneMeasureRequest request;
  request.layer_crystal_ids = { 1 };
  request.path_layers = { { 1 } };
  request.sample_count = 128;
  request.sun_node_count = 2;
  request.illuminant_node_count = 2;
  request.seed = 0x64943u;

  ConfigManager config = Scene();
  analytic::FeatureSupportBatch baseline;
  SceneMeasureResult baseline_measure;
  ASSERT_TRUE(BuildFeatureSupportBatch(config, request, &baseline, &baseline_measure).Ok());
  const auto centre = std::find_if(baseline.samples.begin(), baseline.samples.end(),
                                   [](const auto& sample) { return sample.numerically_available; });
  ASSERT_NE(centre, baseline.samples.end());
  std::vector<double> angular_distances;
  for (const auto& sample : baseline.samples) {
    if (!sample.numerically_available) {
      continue;
    }
    const double dot =
        std::clamp(centre->direction[0] * sample.direction[0] + centre->direction[1] * sample.direction[1] +
                       centre->direction[2] * sample.direction[2],
                   -1.0, 1.0);
    angular_distances.push_back(std::acos(dot) * 180.0 / 3.14159265358979323846);
  }
  ASSERT_FALSE(angular_distances.empty());
  std::sort(angular_distances.begin(), angular_distances.end());
  DirectionFilterParam direction;
  direction.lon_ =
      static_cast<float>(std::atan2(centre->direction[1], centre->direction[0]) * 180.0 / 3.14159265358979323846);
  direction.lat_ = static_cast<float>(std::asin(centre->direction[2]) * 180.0 / 3.14159265358979323846);
  direction.radii_ = static_cast<float>(angular_distances[angular_distances.size() / 2]);
  config.scene_.ms_[0].setting_[0].filter_ = { 17, FilterConfig::kSymNone, FilterConfig::kFilterIn,
                                               SimpleFilterParam{ direction } };

  analytic::FeatureSupportBatch filtered;
  SceneMeasureResult filtered_measure;
  analytic::FeatureReevaluateFn reevaluate;
  ASSERT_TRUE(BuildFeatureSupportBatch(config, request, &filtered, &filtered_measure, &reevaluate).Ok());
  ASSERT_TRUE(reevaluate);
  analytic::FeatureDiscoveryOptions options;
  options.sky_z_bins = 4;
  options.sky_azimuth_bins = 8;
  const analytic::FeatureDiscoveryResult result = analytic::DiscoverFeatures(filtered, options, reevaluate);
  EXPECT_TRUE(std::any_of(result.candidates.begin(), result.candidates.end(), [](const auto& candidate) {
    return candidate.mechanism == analytic::FeatureMechanism::kFilterBoundary && candidate.has_weight_sides &&
           candidate.status == analytic::FeatureEvidenceStatus::kConfirmed;
  }));
  EXPECT_TRUE(std::any_of(result.candidates.begin(), result.candidates.end(), [](const auto& candidate) {
    return candidate.mechanism == analytic::FeatureMechanism::kWeightKink && candidate.has_weight_sides &&
           std::any_of(candidate.active_constraints.begin(), candidate.active_constraints.end(),
                       [](const std::string& name) { return name.find("physical_filter_share") != std::string::npos; });
  })) << "the real filter acceptance jump must also remain readable in the weight ledger";
  EXPECT_FALSE(std::any_of(baseline.samples.begin(), baseline.samples.end(), [](const auto& sample) {
    return std::any_of(sample.constraints.begin(), sample.constraints.end(), [](const auto& constraint) {
      return constraint.kind == analytic::ConstraintKind::kFilter && constraint.value < 0.0;
    });
  })) << "the default pass-through filter ledger must not manufacture a rejected side";
}

}  // namespace
}  // namespace lumice::raypath
