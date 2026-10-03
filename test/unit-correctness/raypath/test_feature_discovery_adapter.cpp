#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>
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
  ASSERT_EQ(batch.parameter_descriptors.size(), static_cast<size_t>(batch.coordinate_dimension));
  ASSERT_EQ(batch.samples.front().active_coordinates.size(), 1u);
  const int pose_coordinate = batch.samples.front().active_coordinates.front();
  EXPECT_EQ(batch.parameter_descriptors[static_cast<size_t>(pose_coordinate)].role,
            analytic::FeatureParameterRole::kPose);
  EXPECT_EQ(batch.parameter_descriptors[static_cast<size_t>(pose_coordinate)].group_id, 0);
  EXPECT_TRUE(std::any_of(batch.scopes.begin(), batch.scopes.end(),
                          [](const auto& scope) { return scope.kind == analytic::FeatureSupportScopeKind::kJoint; }));
  EXPECT_TRUE(std::any_of(batch.scopes.begin(), batch.scopes.end(), [](const auto& scope) {
    return scope.kind == analytic::FeatureSupportScopeKind::kConditional;
  }));
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
  EXPECT_EQ(positive->mapping_evidence_kind, analytic::MappingEvidenceKind::kExactImageDimensionUpperBound);
  EXPECT_EQ(positive->image_dimension_upper_bound, 0);
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

  const analytic::FeatureDiscoveryResult discovery = analytic::DiscoverFeatures(batch, {}, reevaluate);
  const auto atom = std::find_if(discovery.candidates.begin(), discovery.candidates.end(), [](const auto& candidate) {
    return candidate.mechanism == analytic::FeatureMechanism::kMeasureAtom &&
           candidate.status == analytic::FeatureEvidenceStatus::kConfirmed;
  });
  EXPECT_NE(atom, discovery.candidates.end())
      << "the exact shape-only direction proof must preserve real positive-mass continuous atoms";
  const auto same_positive_provenance = [&](const analytic::FeatureCandidate& candidate) {
    return candidate.provenance.member_index == positive->provenance.member_index &&
           candidate.provenance.layer_index == positive->provenance.layer_index &&
           candidate.provenance.interface_index == positive->provenance.interface_index &&
           candidate.provenance.spectrum_node_id == positive->provenance.spectrum_node_id &&
           candidate.provenance.source_node_id == positive->provenance.source_node_id &&
           candidate.provenance.sample_index == positive->provenance.sample_index;
  };
  auto check_conditional_scope = [](const analytic::FeatureCandidate& candidate, std::set<int>* conditional_layers) {
    ASSERT_FALSE(candidate.scope_parameters.empty());
    for (const auto& parameter : candidate.scope_parameters) {
      EXPECT_EQ(parameter.role, analytic::FeatureParameterRole::kShape);
      conditional_layers->insert(parameter.group_id);
    }
  };
  for (analytic::FeatureMechanism mechanism :
       { analytic::FeatureMechanism::kMeasureAtom, analytic::FeatureMechanism::kStrictConfinement }) {
    std::set<int> conditional_layers;
    std::set<uint64_t> evidence_ids;
    bool found_joint_mass = false;
    for (const analytic::FeatureCandidate& candidate : discovery.candidates) {
      if (candidate.mechanism != mechanism || !same_positive_provenance(candidate)) {
        continue;
      }
      evidence_ids.insert(candidate.evidence_id);
      if (candidate.scope_kind == analytic::FeatureSupportScopeKind::kJoint) {
        found_joint_mass = found_joint_mass || candidate.weighted_mass > 0.0;
        continue;
      }
      EXPECT_DOUBLE_EQ(candidate.weighted_mass, 0.0);
      check_conditional_scope(candidate, &conditional_layers);
    }
    EXPECT_TRUE(found_joint_mass);
    EXPECT_EQ(conditional_layers, (std::set<int>{ 0, 1, 2 }));
    EXPECT_EQ(evidence_ids.size(), 1u) << "one source event must correlate all scope projections";
  }
  const double sky_mass = std::accumulate(discovery.sky_field.begin(), discovery.sky_field.end(), 0.0,
                                          [](double sum, const auto& node) { return sum + node.value; });
  EXPECT_NEAR(sky_mass, measure.total_contribution, 1e-12);

  analytic::FeatureReevaluationRequest complete_request;
  complete_request.provenance = positive->provenance;
  complete_request.coordinates = positive->coordinates;
  const int positive_index = static_cast<int>(std::distance(batch.samples.begin(), positive));
  const auto first_axis = std::find_if(batch.cell_axes.begin(), batch.cell_axes.end(),
                                       [positive_index](const auto& axis) { return axis.center == positive_index; });
  ASSERT_NE(first_axis, batch.cell_axes.end());
  const int shifted_coordinate = first_axis->coordinate_index;
  const double upper =
      batch.samples[static_cast<size_t>(first_axis->upper)].coordinates[static_cast<size_t>(shifted_coordinate)];
  complete_request.coordinates[static_cast<size_t>(shifted_coordinate)] +=
      0.2 * (upper - complete_request.coordinates[static_cast<size_t>(shifted_coordinate)]);
  analytic::FeatureSupportSample complete_sample;
  std::string complete_error;
  ASSERT_TRUE(reevaluate(complete_request, &complete_sample, &complete_error)) << complete_error;
  EXPECT_TRUE(complete_sample.direction_jacobian_available);
  EXPECT_TRUE(
      std::all_of(complete_sample.active_coordinates.begin(), complete_sample.active_coordinates.end(),
                  [&](int coordinate) {
                    return complete_sample.direction_jacobian_column_available[static_cast<size_t>(coordinate)] != 0;
                  }))
      << "a non-node product callback point must return all 21 active shape columns";

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
  analytic::FeatureReevaluateFn reevaluate;
  const Error error = BuildFeatureSupportBatch(config, request, &batch, &measure, &reevaluate);
  ASSERT_TRUE(error.Ok()) << error.message;
  ASSERT_TRUE(reevaluate);
  const auto complete = std::find_if(batch.samples.begin(), batch.samples.end(), [](const auto& sample) {
    return sample.accumulates_measure && sample.direction_jacobian_available && sample.active_coordinates.size() == 2u;
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

  const int center_index = static_cast<int>(std::distance(batch.samples.begin(), complete));
  analytic::FeatureReevaluationRequest callback_request;
  callback_request.provenance = complete->provenance;
  callback_request.coordinates = complete->coordinates;
  for (int coordinate : complete->active_coordinates) {
    const auto axis = std::find_if(batch.cell_axes.begin(), batch.cell_axes.end(), [&](const auto& candidate) {
      return candidate.center == center_index && candidate.coordinate_index == coordinate;
    });
    EXPECT_NE(axis, batch.cell_axes.end());
    if (axis == batch.cell_axes.end()) {
      return;
    }
    const double upper = batch.samples[static_cast<size_t>(axis->upper)].coordinates[static_cast<size_t>(coordinate)];
    callback_request.coordinates[static_cast<size_t>(coordinate)] +=
        0.2 * (upper - callback_request.coordinates[static_cast<size_t>(coordinate)]);
  }
  analytic::FeatureSupportSample callback_sample;
  std::string callback_error;
  ASSERT_TRUE(reevaluate(callback_request, &callback_sample, &callback_error)) << callback_error;
  EXPECT_FALSE(callback_sample.accumulates_measure);
  EXPECT_TRUE(callback_sample.direction_jacobian_available);
  EXPECT_EQ(callback_sample.direction_jacobian.size(), 3u * static_cast<size_t>(batch.coordinate_dimension));
  for (int coordinate : callback_sample.active_coordinates) {
    EXPECT_NE(callback_sample.direction_jacobian_column_available[static_cast<size_t>(coordinate)], 0)
        << "the arbitrary callback point must retain every full-chain active column";
  }
}

TEST(FeatureDiscoveryAdapter, ReportsConditionalPoseFoldInsideTheActualFiniteSolarSource) {
  PrismCrystalParam prism;
  prism.h_ = { DistributionType::kNoRandom, 0.73f, 0.0f };
  const float face_distances[6] = { 1.37f, 0.91f, 1.12f, 1.46f, 0.83f, 1.05f };
  for (int face = 0; face < 6; ++face) {
    prism.d_[static_cast<size_t>(face)] = { DistributionType::kNoRandom, face_distances[face], 0.0f };
  }
  CrystalConfig crystal;
  crystal.id_ = 1;
  crystal.param_ = prism;
  crystal.axis_.latitude_dist = { DistributionType::kNoRandom, 90.0f, 0.0f };
  crystal.axis_.azimuth_dist = { DistributionType::kNoRandom, 0.0f, 0.0f };
  crystal.axis_.roll_dist = { DistributionType::kUniform, 0.0f, 360.0f };

  ConfigManager config;
  config.crystals_.emplace(1, crystal);
  config.scene_.light_source_.param_ = SunParam{ 9.0f, 17.0f, 0.53f };
  config.scene_.light_source_.spectrum_ = std::vector<WlParam>{ { 550.0f, 1.0f } };
  config.scene_.max_hits_ = 2;
  ScatteringSetting setting{};
  setting.crystal_ = crystal;
  setting.crystal_proportion_ = 1.0f;
  MsInfo layer{};
  layer.setting_.push_back(std::move(setting));
  config.scene_.ms_.push_back(std::move(layer));

  SceneMeasureRequest request;
  request.layer_crystal_ids = { 1 };
  request.path_layers = { { 3, 5 } };
  request.sample_count = 64;
  request.sun_node_count = 8;
  request.illuminant_node_count = 8;
  request.seed = 0;

  analytic::FeatureSupportBatch batch;
  SceneMeasureResult measure;
  analytic::FeatureReevaluateFn reevaluate;
  const Error error = BuildFeatureSupportBatch(config, request, &batch, &measure, &reevaluate);
  ASSERT_TRUE(error.Ok()) << error.message;
  ASSERT_TRUE(reevaluate);
  EXPECT_EQ(batch.visited_row_count, 512u);
  EXPECT_EQ(measure.evaluated_row_count, 512);
  EXPECT_GT(measure.total_contribution, 0.0);
  EXPECT_EQ(batch.parameter_descriptors[1].role, analytic::FeatureParameterRole::kSource);
  EXPECT_EQ(batch.parameter_descriptors[2].role, analytic::FeatureParameterRole::kSource);

  const analytic::FeatureDiscoveryResult discovery = analytic::DiscoverFeatures(batch, {}, reevaluate);
  const auto conditional_fold =
      std::find_if(discovery.candidates.begin(), discovery.candidates.end(), [](const auto& candidate) {
        return candidate.mechanism == analytic::FeatureMechanism::kInteriorRankLoss &&
               candidate.status == analytic::FeatureEvidenceStatus::kConfirmed &&
               candidate.scope_kind == analytic::FeatureSupportScopeKind::kConditional &&
               !candidate.scope_parameters.empty() &&
               std::all_of(
                   candidate.scope_parameters.begin(), candidate.scope_parameters.end(),
                   [](const auto& parameter) { return parameter.role == analytic::FeatureParameterRole::kPose; });
      });
  ASSERT_NE(conditional_fold, discovery.candidates.end());
  EXPECT_EQ(conditional_fold->provenance.spectrum_node_id, 0);
  EXPECT_GE(conditional_fold->provenance.source_node_id, 0);
  EXPECT_LT(conditional_fold->provenance.source_node_id, 8);
  EXPECT_DOUBLE_EQ(conditional_fold->weighted_mass, 0.0)
      << "conditional evidence must not duplicate source-measure mass";
  EXPECT_FALSE(std::any_of(discovery.candidates.begin(), discovery.candidates.end(), [](const auto& candidate) {
    return candidate.mechanism == analytic::FeatureMechanism::kInteriorRankLoss &&
           candidate.status == analytic::FeatureEvidenceStatus::kConfirmed &&
           candidate.scope_kind == analytic::FeatureSupportScopeKind::kJoint;
  })) << "the two finite-source tangent directions keep the joint mapping regular";
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
