#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
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
  EXPECT_GT(scene.evaluated_row_count, scene.stored_row_count);
  EXPECT_TRUE(scene.rows_truncated);

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
  ASSERT_FALSE(result.rows.front().latents.empty());
  const auto& first_layer = result.rows.front().layers.front();
  const auto height_sample = std::find_if(first_layer.shape.begin(), first_layer.shape.end(),
                                          [](const auto& sample) { return sample.name == "height"; });
  const auto face_sample = std::find_if(first_layer.shape.begin(), first_layer.shape.end(),
                                        [](const auto& sample) { return sample.name == "face_distance[0]"; });
  ASSERT_NE(height_sample, first_layer.shape.end());
  ASSERT_NE(face_sample, first_layer.shape.end());
  EXPECT_EQ(height_sample->latent_id, face_sample->latent_id);
  EXPECT_EQ(height_sample->leader_slot, face_sample->leader_slot);
  EXPECT_TRUE(height_sample->absolute_value_fold);
  EXPECT_DOUBLE_EQ(height_sample->raw_value, face_sample->raw_value);
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
  EXPECT_TRUE(std::all_of(result.rows.begin(), result.rows.end(), [](const SceneMeasureRow& row) {
    if (row.layers.empty() || row.layers[0].pose_support_rank != 3) {
      return false;
    }
    return std::all_of(row.latents.begin(), row.latents.end(), [](const LatentMeasureSample& latent) {
      return std::isfinite(latent.coordinate) && std::isfinite(latent.proposal_density_or_mass) &&
             std::isfinite(latent.target_density_or_mass) && std::isfinite(latent.mapping_jacobian);
    });
  }));
  EXPECT_TRUE(std::any_of(result.rows.front().latents.begin(), result.rows.front().latents.end(),
                          [](const LatentMeasureSample& latent) {
                            return latent.name == "pose.latitude_fold_branch" &&
                                   latent.base_measure == LatentBaseMeasure::kBernoulliCounting;
                          }));
  EXPECT_TRUE(std::any_of(std::begin(result.rows.front().layers[0].pose_tangent_drotation),
                          std::end(result.rows.front().layers[0].pose_tangent_drotation),
                          [](double value) { return std::fabs(value) > 1e-4; }));
}

TEST(SceneMeasure, ZeroWidthPoseGeneratorsHaveRankZeroDespiteTheirDistributionNames) {
  AxisDistribution axis;
  axis.latitude_dist = { DistributionType::kGaussian, 45.0f, 0.0f };
  axis.azimuth_dist = { DistributionType::kUniform, 12.0f, 0.0f };
  axis.roll_dist = { DistributionType::kZigzag, 7.0f, 0.0f };
  const SceneMeasureResult result = Build(Scene({ Prism(1, axis) }, { 0.0f }), Request({ 1 }, { { 3, 5 } }, 2));
  ASSERT_FALSE(result.rows.empty());
  EXPECT_TRUE(std::all_of(result.rows.begin(), result.rows.end(), [](const SceneMeasureRow& row) {
    return !row.layers.empty() && row.layers[0].pose_support_rank == 0;
  }));
  EXPECT_TRUE(std::all_of(result.factors.begin(), result.factors.end(), [](const MeasureFactorDescriptor& factor) {
    return factor.layer_index < 0 || factor.name.rfind("pose.", 0) != 0 || factor.support_dimension == 0;
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

TEST(SceneMeasure, PhysicalMaskUsesStableLayerLocalEntryFaces) {
  const ConfigManager config = Scene({ Prism(1, RandomAxis()) }, { 0.0f });
  SceneMeasureRequest all_request = Request({ 1 }, { { 3, 5 } }, 2);
  all_request.member_selection = SceneMemberSelection::kAllPhysical;
  const SceneMeasureResult all = Build(config, all_request);
  ASSERT_GT(all.member_chains.size(), 1u);

  SceneMeasureRequest mask_request = all_request;
  mask_request.member_selection = SceneMemberSelection::kPhysicalMask;
  const int entry_face = all.member_chains[1][0].front();
  ASSERT_GE(entry_face, 0);
  ASSERT_LT(entry_face, 64);
  mask_request.physical_member_mask = uint64_t{ 1 } << entry_face;
  const SceneMeasureResult selected = Build(config, mask_request);
  ASSERT_FALSE(selected.member_chains.empty());
  EXPECT_TRUE(std::all_of(selected.member_chains.begin(), selected.member_chains.end(),
                          [entry_face](const auto& chain) { return chain.front().front() == entry_face; }));
  EXPECT_EQ(selected.rows.size(), selected.member_chains.size() * 2u);
}

TEST(SceneMeasure, ExplicitMemberChainsAreVerbatimAndNeedNotBeACartesianProduct) {
  const ConfigManager config = Scene({ Prism(1), Prism(2) }, { 1.0f, 0.0f });
  SceneMeasureRequest request = Request({ 1, 2 }, { { 3, 5 }, { 4, 2 } }, 2);
  request.member_selection = SceneMemberSelection::kExplicitChains;
  request.explicit_member_chains = { { { 3, 5 }, { 4, 2 } }, { { 3, 1, 5 }, { 4, 0, 2 } } };
  const SceneMeasureResult result = Build(config, request);
  EXPECT_EQ(result.member_chains, request.explicit_member_chains);
  EXPECT_EQ(result.evaluated_row_count, 2 * 2);
}

TEST(SceneMeasure, VisitorReceivesTheCompleteReplayableFieldBeyondRepresentativeStorage) {
  const ConfigManager config = Scene({ Prism(1, RandomAxis()) }, { 0.0f });
  SceneMeasureRequest request = Request({ 1 }, { { 3, 5 } }, 128);
  int visited = 0;
  double visited_sum = 0.0;
  SceneMeasureResult result;
  const Error error = BuildSceneMeasure(
      config, request,
      [&](const SceneMeasureRow& row) {
        visited++;
        visited_sum += row.contribution;
      },
      &result);
  ASSERT_TRUE(error.Ok()) << error.message;
  EXPECT_EQ(visited, result.evaluated_row_count);
  EXPECT_EQ(result.stored_row_count, 64);
  EXPECT_TRUE(result.rows_truncated);
  EXPECT_NEAR(visited_sum, result.total_contribution, 1e-12 * std::max(1.0, std::fabs(result.total_contribution)));
  EXPECT_NE(result.stored_row_selection.find("bottom-k hash"), std::string::npos);
}

TEST(SceneMeasure, CrossLayerRowsCarryOutgoingDirectionAndGlobalWeightsOnlyOnce) {
  ConfigManager config = Scene({ Prism(1, RandomAxis()), Prism(2, RandomAxis()) }, { 1.0f, 0.0f });
  config.scene_.light_source_.param_.diameter_ = 1.0f;
  SceneMeasureRequest request = Request({ 1, 2 }, { { 3, 5 }, { 3, 5 } }, 512);
  request.spectrum_source = SceneSpectrumSource::kDiagnostic;
  request.diagnostic_wavelengths_nm = { 520.0, 550.0 };
  request.diagnostic_wavelength_weights = { 0.5, 1.0 };
  const SceneMeasureResult unit = Build(config, request);
  ASSERT_EQ(unit.sun_nodes.size(), 4u);
  const auto complete = std::find_if(unit.rows.begin(), unit.rows.end(),
                                     [](const SceneMeasureRow& row) { return row.layers.size() == 2u; });
  ASSERT_NE(complete, unit.rows.end());
  for (int i = 0; i < 3; i++) {
    EXPECT_DOUBLE_EQ(complete->layers[1].incident_direction[i], complete->layers[0].outgoing_direction[i]);
  }
  EXPECT_EQ(complete->layers[0].source_sun_node_id, complete->sun_node_id);
  EXPECT_EQ(complete->layers[1].source_sun_node_id, complete->sun_node_id);
  EXPECT_EQ(complete->layers[0].source_spectrum_node_id, complete->spectrum_node_id);
  EXPECT_EQ(complete->layers[1].source_spectrum_node_id, complete->spectrum_node_id);
  EXPECT_DOUBLE_EQ(complete->global_weight, complete->spectrum_weight * complete->sun_mass);

  request.diagnostic_wavelength_weights = { 2.0, 4.0 };
  const SceneMeasureResult quadruple = Build(config, request);
  EXPECT_NEAR(quadruple.total_contribution, 4.0 * unit.total_contribution,
              1e-12 * std::max(1.0, std::fabs(unit.total_contribution)));
  EXPECT_EQ(quadruple.spectrum_nodes.size(), 2u);
  EXPECT_TRUE(std::all_of(quadruple.rows.begin(), quadruple.rows.end(), [](const SceneMeasureRow& row) {
    return row.layers.empty() || std::all_of(row.layers.begin(), row.layers.end(),
                                             [&](const SceneMeasureLayerRow&) { return row.spectrum_node_id >= 0; });
  }));
}

TEST(SceneMeasure, SourceNodeWeightsActOnceAndCrossLayerRowsRetainTheirSourceIdentity) {
  ConfigManager config = Scene({ Prism(1, RandomAxis()), Prism(2, RandomAxis()) }, { 1.0f, 0.0f });
  SceneMeasureRequest request = Request({ 1, 2 }, { { 3, 5 }, { 3, 5 } }, 512);
  request.spectrum_source = SceneSpectrumSource::kDiagnostic;
  request.diagnostic_wavelengths_nm = { 550.0 };
  const SceneMeasureResult default_source = Build(config, request);
  ASSERT_EQ(default_source.sun_nodes.size(), 1u);
  SunMeasureNode first = default_source.sun_nodes.front();
  SunMeasureNode second = first;
  first.mass = 0.2;
  second.mass = 0.8;
  request.source_sun_nodes = { first, second };

  double unit_by_source[2]{};
  SceneMeasureResult unit;
  Error error = BuildSceneMeasure(
      config, request,
      [&](const SceneMeasureRow& row) {
        unit_by_source[row.sun_node_id] += row.contribution;
        for (const auto& layer : row.layers) {
          EXPECT_EQ(layer.source_sun_node_id, row.sun_node_id);
          EXPECT_EQ(layer.source_spectrum_node_id, row.spectrum_node_id);
        }
        if (row.layers.size() == 2u) {
          for (int i = 0; i < 3; i++) {
            EXPECT_DOUBLE_EQ(row.layers[1].incident_direction[i], row.layers[0].outgoing_direction[i]);
          }
        }
      },
      &unit);
  ASSERT_TRUE(error.Ok()) << error.message;
  ASSERT_GT(unit_by_source[0], 0.0);
  ASSERT_GT(unit_by_source[1], 0.0);

  request.source_sun_nodes[0].mass = 0.4;
  double changed_by_source[2]{};
  SceneMeasureResult changed;
  error = BuildSceneMeasure(
      config, request, [&](const SceneMeasureRow& row) { changed_by_source[row.sun_node_id] += row.contribution; },
      &changed);
  ASSERT_TRUE(error.Ok()) << error.message;
  EXPECT_NEAR(changed_by_source[0], 2.0 * unit_by_source[0], 1e-12 * std::max(1.0, unit_by_source[0]));
  EXPECT_NEAR(changed_by_source[1], unit_by_source[1], 1e-12 * std::max(1.0, unit_by_source[1]));
}

TEST(SceneMeasure, StatusSeparatesZeroSourceFromNumericalEvaluationAndNoHitProof) {
  CrystalConfig bad = Prism(1);
  std::get<PrismCrystalParam>(bad.param_).h_ = { DistributionType::kNoRandom, std::numeric_limits<float>::quiet_NaN(),
                                                 0.0f };
  const SceneMeasureResult zero =
      Build(Scene({ bad }, { 0.0f }, { { 550.0f, 0.0f } }), Request({ 1 }, { { 3, 5 } }, 2));
  EXPECT_EQ(zero.status, SceneMeasureStatus::kZeroWeight);
  EXPECT_GT(zero.status_counts.zero_weight, 0);
  EXPECT_GT(zero.status_counts.numerical_incomplete, 0);
  ASSERT_FALSE(zero.rows.empty());
  EXPECT_EQ(zero.rows.front().status, SceneMeasureStatus::kZeroWeight);
  EXPECT_EQ(zero.rows.front().evaluation_status, SceneMeasureStatus::kNumericalIncomplete);

  const SceneMeasureResult random_miss =
      Build(Scene({ Prism(1, RandomAxis()) }, { 0.0f }), Request({ 1 }, { { 99 } }, 2));
  EXPECT_EQ(random_miss.status, SceneMeasureStatus::kNumericalIncomplete);
  const SceneMeasureResult atomic_miss = Build(Scene({ Prism(1) }, { 0.0f }), Request({ 1 }, { { 99 } }, 2));
  EXPECT_EQ(atomic_miss.status, SceneMeasureStatus::kPhysicallyUnreachable);
}

TEST(SceneMeasure, SourceResolutionIsPartOfTheResultStatusAndErrorBudget) {
  ConfigManager finite_sun = Scene({ Prism(1, RandomAxis()) }, { 0.0f });
  finite_sun.scene_.light_source_.param_.diameter_ = 1.0f;
  SceneMeasureRequest sun_request = Request({ 1 }, { { 3, 5 } }, 128);
  sun_request.sun_node_count = 1;
  const SceneMeasureResult sun = Build(finite_sun, sun_request);
  EXPECT_EQ(sun.status, SceneMeasureStatus::kNumericalIncomplete);
  EXPECT_DOUBLE_EQ(sun.sun_node_error_estimate, 0.0);

  ConfigManager illuminant = Scene({ Prism(1, RandomAxis()) }, { 0.0f });
  illuminant.scene_.light_source_.spectrum_ = IlluminantType::kD65;
  SceneMeasureRequest spectrum_request = Request({ 1 }, { { 3, 5 } }, 128);
  spectrum_request.illuminant_node_count = 1;
  const SceneMeasureResult spectrum = Build(illuminant, spectrum_request);
  EXPECT_EQ(spectrum.status, SceneMeasureStatus::kNumericalIncomplete);
  EXPECT_DOUBLE_EQ(spectrum.spectrum_node_error_estimate, 0.0);
}

TEST(SceneMeasure, FiniteSolarDiscIsANormalizedSourceMeasureRatherThanAnAreaFactor) {
  ConfigManager config = Scene({ Prism(1, RandomAxis()) }, { 0.0f });
  config.scene_.light_source_.param_.diameter_ = 1.0f;
  SceneMeasureRequest request = Request({ 1 }, { { 3, 5 } }, 2);
  request.sun_node_count = 8;
  const SceneMeasureResult result = Build(config, request);
  ASSERT_EQ(result.sun_nodes.size(), 8u);
  const double mass = std::accumulate(result.sun_nodes.begin(), result.sun_nodes.end(), 0.0,
                                      [](double sum, const SunMeasureNode& node) { return sum + node.mass; });
  EXPECT_NEAR(mass, 1.0, 1e-14);
  EXPECT_TRUE(std::any_of(result.sun_nodes.begin() + 1, result.sun_nodes.end(), [&](const SunMeasureNode& node) {
    return std::fabs(node.incident_direction[0] - result.sun_nodes[0].incident_direction[0]) > 1e-7 ||
           std::fabs(node.incident_direction[1] - result.sun_nodes[0].incident_direction[1]) > 1e-7 ||
           std::fabs(node.incident_direction[2] - result.sun_nodes[0].incident_direction[2]) > 1e-7;
  }));
}

}  // namespace
}  // namespace lumice::raypath
