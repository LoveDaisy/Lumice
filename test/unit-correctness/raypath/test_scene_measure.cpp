#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

#include "raypath/scene_measure.hpp"

namespace lumice::raypath {
namespace {

constexpr Distribution kFullTurn{ DistributionType::kUniform, 0.0f, 360.0f };

CrystalConfig Prism(IdType id, const AxisDistribution& axis = AxisDistribution{}) {
  PrismCrystalParam prism;
  prism.h_ = { DistributionType::kNoRandom, 1.0f, 0.0f };
  for (auto& distance : prism.d_) {
    distance = { DistributionType::kNoRandom, 1.0f, 0.0f };
  }
  CrystalConfig crystal;
  crystal.id_ = id;
  crystal.param_ = prism;
  crystal.axis_ = axis;
  return crystal;
}

AxisDistribution RandomAxis() {
  AxisDistribution axis;
  axis.latitude_dist = { DistributionType::kUniform, 90.0f, 360.0f };
  axis.azimuth_dist = kFullTurn;
  axis.roll_dist = kFullTurn;
  return axis;
}

ConfigManager Scene(std::vector<CrystalConfig> crystals, std::vector<float> probabilities,
                    std::vector<WlParam> spectrum = { { 550.0f, 1.0f } }) {
  ConfigManager config;
  config.scene_.light_source_.param_ = SunParam{ 20.0f, 17.0f, 0.0f };
  config.scene_.light_source_.spectrum_ = std::move(spectrum);
  EXPECT_EQ(crystals.size(), probabilities.size());
  for (size_t i = 0; i < crystals.size(); i++) {
    config.crystals_.emplace(crystals[i].id_, crystals[i]);
    ScatteringSetting setting{};
    setting.crystal_ = crystals[i];
    setting.crystal_proportion_ = 1.0f;
    MsInfo layer{};
    layer.prob_ = probabilities[i];
    layer.setting_.push_back(std::move(setting));
    config.scene_.ms_.push_back(std::move(layer));
  }
  return config;
}

SceneMeasureRequest Request(std::vector<IdType> crystals, std::vector<std::vector<int>> layers, int samples = 64) {
  SceneMeasureRequest request;
  request.layer_crystal_ids = std::move(crystals);
  request.path_layers = std::move(layers);
  request.sample_count = samples;
  request.sun_node_count = 4;
  request.illuminant_node_count = 4;
  request.seed = 0x6493u;
  return request;
}

SceneMeasureResult Build(const ConfigManager& config, const SceneMeasureRequest& request) {
  SceneMeasureResult result;
  const Error error = BuildSceneMeasure(config, request, &result);
  EXPECT_TRUE(error.Ok()) << error.message;
  return result;
}

double ShapeValue(const SceneMeasureLayerRow& row, const char* name) {
  const auto found = std::find_if(row.shape.begin(), row.shape.end(),
                                  [name](const ShapeScalarSample& sample) { return sample.name == name; });
  EXPECT_NE(found, row.shape.end()) << name;
  return found == row.shape.end() ? 0.0 : found->value;
}

TEST(SceneMeasure, SceneSpectrumWeightsScaleOnceAndKeepZeroNodes) {
  const ConfigManager config = Scene({ Prism(1, RandomAxis()) }, { 0.0f }, { { 550.0f, 2.0f }, { 600.0f, 0.0f } });
  SceneMeasureRequest request = Request({ 1 }, { { 3, 5 } });
  const SceneMeasureResult scene = Build(config, request);
  ASSERT_EQ(scene.spectrum_nodes.size(), 2u);
  EXPECT_EQ(scene.spectrum_nodes[0].source, "scene_discrete");
  EXPECT_DOUBLE_EQ(scene.spectrum_nodes[0].weight, 2.0);
  EXPECT_DOUBLE_EQ(scene.spectrum_nodes[1].weight, 0.0);
  EXPECT_TRUE(std::any_of(scene.rows.begin(), scene.rows.end(), [](const SceneMeasureRow& row) {
    return row.spectrum_node_id == 1 && row.status == SceneMeasureStatus::kZeroWeight;
  }));

  request.spectrum_source = SceneSpectrumSource::kDiagnostic;
  request.diagnostic_wavelengths_nm = { 550.0 };
  request.diagnostic_wavelength_weights = { 1.0 };
  const SceneMeasureResult unit = Build(config, request);
  request.diagnostic_wavelength_weights = { 3.0 };
  const SceneMeasureResult triple = Build(config, request);
  EXPECT_NEAR(triple.total_contribution, 3.0 * unit.total_contribution,
              1e-12 * std::max(1.0, std::fabs(unit.total_contribution)));
  EXPECT_NEAR(triple.coarse_contribution, 3.0 * unit.coarse_contribution,
              1e-12 * std::max(1.0, std::fabs(unit.coarse_contribution)));
}

TEST(SceneMeasure, ShapeSyncGroupSharesRawDrawAndHeightFoldUsesTheActualSample) {
  CrystalConfig crystal = Prism(1, RandomAxis());
  auto& prism = std::get<PrismCrystalParam>(crystal.param_);
  prism.h_ = { DistributionType::kUniform, 1.0f, 0.4f };
  prism.d_[0] = prism.h_;
  prism.sync_group_[kShapeScalarHeight] = 7;
  prism.sync_group_[kShapeScalarFace0] = 7;
  PrepareSyncGroups(prism);
  const SceneMeasureResult result = Build(Scene({ crystal }, { 0.0f }), Request({ 1 }, { { 3, 5 } }, 32));
  ASSERT_FALSE(result.rows.empty());
  bool differs_from_center = false;
  for (const SceneMeasureRow& row : result.rows) {
    if (row.layers.size() != 1u) {
      ADD_FAILURE() << "expected one layer, got " << row.layers.size();
      continue;
    }
    const double height = ShapeValue(row.layers[0], "height");
    const double face0 = ShapeValue(row.layers[0], "face_distance[0]");
    EXPECT_DOUBLE_EQ(height, std::fabs(face0));
    differs_from_center = differs_from_center || std::fabs(height - 1.0) > 1e-4;
  }
  EXPECT_TRUE(differs_from_center);
  const auto height_factor = std::find_if(result.factors.begin(), result.factors.end(),
                                          [](const auto& factor) { return factor.name == "shape.height"; });
  const auto face_factor = std::find_if(result.factors.begin(), result.factors.end(),
                                        [](const auto& factor) { return factor.name == "shape.face_distance[0]"; });
  ASSERT_NE(height_factor, result.factors.end());
  ASSERT_NE(face_factor, result.factors.end());
  EXPECT_EQ(height_factor->latent_id, face_factor->latent_id);
}

TEST(SceneMeasure, GeneralPoseDistributionsAreMeasuredWithoutFamilyWhitelist) {
  AxisDistribution axis;
  axis.latitude_dist = { DistributionType::kLaplacian, 62.0f, 4.0f };
  axis.azimuth_dist = { DistributionType::kGaussian, 31.0f, 7.0f };
  axis.roll_dist = { DistributionType::kZigzag, 9.0f, 3.0f };
  const SceneMeasureResult result = Build(Scene({ Prism(1, axis) }, { 0.0f }), Request({ 1 }, { { 3, 5 } }, 32));
  EXPECT_EQ(result.rows.size(), 32u);
  EXPECT_NE(std::find_if(result.factors.begin(), result.factors.end(),
                         [](const auto& factor) {
                           return factor.name == "pose.latitude" &&
                                  factor.distribution == DistributionType::kLaplacian && factor.support_dimension == 1;
                         }),
            result.factors.end());
  EXPECT_NE(std::find_if(result.factors.begin(), result.factors.end(),
                         [](const auto& factor) {
                           return factor.name == "pose.azimuth" && factor.distribution == DistributionType::kGaussian;
                         }),
            result.factors.end());
  EXPECT_NE(std::find_if(result.factors.begin(), result.factors.end(),
                         [](const auto& factor) {
                           return factor.name == "pose.roll" && factor.distribution == DistributionType::kZigzag;
                         }),
            result.factors.end());
  EXPECT_TRUE(std::any_of(result.rows.begin(), result.rows.end(), [](const SceneMeasureRow& row) {
    return std::fabs(row.layers[0].pose_lon_lat_roll_rad[2]) > 1e-4;
  }));
}

TEST(SceneMeasure, EveryProductDistributionTypeUsesTheSamePoseAndShapeRoute) {
  constexpr DistributionType kTypes[] = {
    DistributionType::kNoRandom, DistributionType::kUniform,   DistributionType::kGaussian,
    DistributionType::kZigzag,   DistributionType::kLaplacian, DistributionType::kGaussianLegacy,
  };
  for (const DistributionType type : kTypes) {
    SCOPED_TRACE(static_cast<int>(type));
    AxisDistribution axis;
    axis.latitude_dist = { type, type == DistributionType::kZigzag ? 75.0f : 60.0f,
                           type == DistributionType::kNoRandom ? 0.0f : 2.0f };
    axis.azimuth_dist = { type, 13.0f, type == DistributionType::kNoRandom ? 0.0f : 2.0f };
    axis.roll_dist = { type, 7.0f, type == DistributionType::kNoRandom ? 0.0f : 2.0f };
    CrystalConfig crystal = Prism(1, axis);
    auto& prism = std::get<PrismCrystalParam>(crystal.param_);
    prism.h_ = { type, 1.0f, type == DistributionType::kNoRandom ? 0.0f : 0.1f };
    const SceneMeasureResult result = Build(Scene({ crystal }, { 0.0f }), Request({ 1 }, { { 3, 5 } }, 4));
    EXPECT_EQ(result.rows.size(), 4u);
    const auto shape = std::find_if(result.factors.begin(), result.factors.end(),
                                    [](const auto& factor) { return factor.name == "shape.height"; });
    const auto latitude = std::find_if(result.factors.begin(), result.factors.end(),
                                       [](const auto& factor) { return factor.name == "pose.latitude"; });
    if (shape == result.factors.end() || latitude == result.factors.end()) {
      ADD_FAILURE() << "shape.height and pose.latitude descriptors must both exist";
      continue;
    }
    EXPECT_EQ(shape->distribution, type);
    EXPECT_EQ(latitude->distribution, type);
    EXPECT_EQ(shape->support_dimension, type == DistributionType::kNoRandom ? 0 : 1);
    EXPECT_EQ(latitude->support_dimension, type == DistributionType::kNoRandom ? 0 : 1);
  }
}

TEST(SceneMeasure, PyramidRowsCarryAllRandomHeightsAndDeterministicWedges) {
  PyramidCrystalParam pyramid;
  pyramid.h_pyr_u_ = { DistributionType::kUniform, 0.4f, 0.2f };
  pyramid.h_prs_ = { DistributionType::kGaussian, 1.0f, 0.1f };
  pyramid.h_pyr_l_ = { DistributionType::kLaplacian, 0.5f, 0.05f };
  for (auto& distance : pyramid.d_) {
    distance = { DistributionType::kNoRandom, 1.0f, 0.0f };
  }
  pyramid.wedge_angle_u_ = 22.0f;
  pyramid.wedge_angle_l_ = 31.0f;
  CrystalConfig crystal;
  crystal.id_ = 1;
  crystal.param_ = pyramid;
  crystal.axis_ = RandomAxis();
  const SceneMeasureResult result = Build(Scene({ crystal }, { 0.0f }), Request({ 1 }, { { 13, 3, 5 } }, 16));
  ASSERT_FALSE(result.rows.empty());
  const SceneMeasureLayerRow& layer = result.rows.front().layers.front();
  EXPECT_TRUE(std::isfinite(ShapeValue(layer, "upper_h")));
  EXPECT_TRUE(std::isfinite(ShapeValue(layer, "prism_h")));
  EXPECT_TRUE(std::isfinite(ShapeValue(layer, "lower_h")));
  EXPECT_TRUE(std::any_of(result.rows.begin(), result.rows.end(), [](const SceneMeasureRow& row) {
    return std::fabs(ShapeValue(row.layers.front(), "upper_h") - 0.4) > 1e-4;
  }));
}

TEST(SceneMeasure, PhysicalMaskNarrowsRatherThanReplacingTheGeometryGates) {
  const ConfigManager config = Scene({ Prism(1, RandomAxis()) }, { 0.0f });
  SceneMeasureRequest all_request = Request({ 1 }, { { 3, 5 } }, 2);
  all_request.member_selection = SceneMemberSelection::kAllPhysical;
  const SceneMeasureResult all = Build(config, all_request);
  ASSERT_GT(all.member_chains.size(), 1u);

  SceneMeasureRequest mask_request = all_request;
  mask_request.member_selection = SceneMemberSelection::kPhysicalMask;
  mask_request.physical_member_mask = uint64_t{ 1 } << 1;
  const SceneMeasureResult selected = Build(config, mask_request);
  ASSERT_EQ(selected.member_chains.size(), 1u);
  EXPECT_EQ(selected.member_chains[0], all.member_chains[1]);
  EXPECT_EQ(selected.rows.size(), 2u);
}

TEST(SceneMeasure, CrossLayerRowsCarryOutgoingDirectionAndGlobalWeightsOnlyOnce) {
  const ConfigManager config = Scene({ Prism(1, RandomAxis()), Prism(2, RandomAxis()) }, { 1.0f, 0.0f });
  SceneMeasureRequest request = Request({ 1, 2 }, { { 3, 5 }, { 3, 5 } }, 512);
  request.spectrum_source = SceneSpectrumSource::kDiagnostic;
  request.diagnostic_wavelengths_nm = { 550.0 };
  request.diagnostic_wavelength_weights = { 1.0 };
  const SceneMeasureResult unit = Build(config, request);
  const auto complete = std::find_if(unit.rows.begin(), unit.rows.end(),
                                     [](const SceneMeasureRow& row) { return row.layers.size() == 2u; });
  ASSERT_NE(complete, unit.rows.end());
  for (int i = 0; i < 3; i++) {
    EXPECT_DOUBLE_EQ(complete->layers[1].incident_direction[i], complete->layers[0].outgoing_direction[i]);
  }

  request.diagnostic_wavelength_weights = { 4.0 };
  const SceneMeasureResult quadruple = Build(config, request);
  EXPECT_NEAR(quadruple.total_contribution, 4.0 * unit.total_contribution,
              1e-12 * std::max(1.0, std::fabs(unit.total_contribution)));
  EXPECT_EQ(quadruple.spectrum_nodes.size(), 1u);
  EXPECT_TRUE(std::all_of(quadruple.rows.begin(), quadruple.rows.end(), [](const SceneMeasureRow& row) {
    return row.layers.empty() || std::all_of(row.layers.begin(), row.layers.end(),
                                             [&](const SceneMeasureLayerRow&) { return row.spectrum_node_id == 0; });
  }));
}

TEST(SceneMeasure, FiniteSolarDiscIsAUnitMassCapRatherThanItsCenter) {
  ConfigManager config = Scene({ Prism(1, RandomAxis()) }, { 0.0f });
  config.scene_.light_source_.param_.diameter_ = 1.0f;
  SceneMeasureRequest request = Request({ 1 }, { { 3, 5 } }, 2);
  request.sun_node_count = 8;
  const SceneMeasureResult result = Build(config, request);
  ASSERT_EQ(result.sun_nodes.size(), 8u);
  const double mass = std::accumulate(result.sun_nodes.begin(), result.sun_nodes.end(), 0.0,
                                      [](double sum, const SunMeasureNode& node) { return sum + node.mass; });
  EXPECT_DOUBLE_EQ(mass, 1.0);
  EXPECT_TRUE(std::any_of(result.sun_nodes.begin() + 1, result.sun_nodes.end(), [&](const SunMeasureNode& node) {
    return std::fabs(node.incident_direction[0] - result.sun_nodes[0].incident_direction[0]) > 1e-7 ||
           std::fabs(node.incident_direction[1] - result.sun_nodes[0].incident_direction[1]) > 1e-7 ||
           std::fabs(node.incident_direction[2] - result.sun_nodes[0].incident_direction[2]) > 1e-7;
  }));
}

}  // namespace
}  // namespace lumice::raypath
