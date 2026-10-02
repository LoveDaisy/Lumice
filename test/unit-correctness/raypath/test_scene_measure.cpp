#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

#include "core/trace_ops.hpp"
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

double SurfaceArea(const CrystalGeom& geometry) {
  double surface_area = 0.0;
  for (int face = 0; face < geometry.face_cnt; ++face) {
    const float* vertices = geometry.face_vtx + face * kCrystalGeomMaxVtxPerFace * 3;
    for (int vertex = 1; vertex + 1 < geometry.face_vtx_cnt[face]; ++vertex) {
      double edge0[3]{};
      double edge1[3]{};
      for (int coordinate = 0; coordinate < 3; ++coordinate) {
        edge0[coordinate] = static_cast<double>(vertices[3 * vertex + coordinate]) - vertices[coordinate];
        edge1[coordinate] = static_cast<double>(vertices[3 * (vertex + 1) + coordinate]) - vertices[coordinate];
      }
      const double cross[3] = { edge0[1] * edge1[2] - edge0[2] * edge1[1], edge0[2] * edge1[0] - edge0[0] * edge1[2],
                                edge0[0] * edge1[1] - edge0[1] * edge1[0] };
      surface_area += 0.5 * std::sqrt(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]);
    }
  }
  return surface_area;
}

TEST(SceneMeasure, NativeContributionUsesActualSurfaceNormalizationAndIsScaleInvariant) {
  AxisDistribution axis;
  axis.latitude_dist = { DistributionType::kNoRandom, 90.0f, 0.0f };
  axis.azimuth_dist = { DistributionType::kNoRandom, 180.0f, 0.0f };
  axis.roll_dist = { DistributionType::kNoRandom, 0.0f, 0.0f };
  const auto request = Request({ 1 }, { { 3, 6 } }, 2);
  double contributions[2]{};
  double entry_areas[2]{};
  const float scales[2] = { 1.0f, 8.0f };
  for (int trial = 0; trial < 2; ++trial) {
    const float scale = scales[trial];
    CrystalConfig crystal = Prism(1, axis);
    auto& param = std::get<PrismCrystalParam>(crystal.param_);
    param.h_.center *= scale;
    float distances[6]{};
    for (int face = 0; face < 6; ++face) {
      param.d_[face].center *= scale;
      distances[face] = param.d_[face].center;
    }
    const auto geometry = Crystal::CreatePrism(param.h_.center, distances);
    const double surface_area = SurfaceArea(geometry.CfGeom());
    const auto result = Build(Scene({ crystal }, { 0.0f }), request);
    EXPECT_EQ(result.status, SceneMeasureStatus::kConfirmed);
    EXPECT_GT(result.total_contribution, 0.0);
    double expected = 0.0;
    for (const auto& row : result.rows) {
      EXPECT_EQ(row.layers.size(), 1u);
      if (row.layers.size() != 1u) {
        continue;
      }
      const auto& layer = row.layers.front();
      EXPECT_EQ(layer.status, SceneMeasureStatus::kConfirmed);
      EXPECT_NEAR(layer.total_surface_area, surface_area, 1e-7 * surface_area);
      EXPECT_DOUBLE_EQ(layer.normalized_entry_factor, 2.0 * layer.entry_measure / layer.total_surface_area);
      EXPECT_NEAR(layer.normalized_entry_factor, 2.0 * layer.entry_measure / surface_area,
                  1e-7 * layer.normalized_entry_factor);
      expected += row.global_weight * row.joint_sample_mass * row.joint_importance_weight * layer.crystal_share *
                  layer.continuation_mass * (2.0 * layer.entry_measure / surface_area) * layer.fresnel_weight;
      entry_areas[trial] = layer.entry_measure;
    }
    EXPECT_NEAR(result.total_contribution, expected, 1e-7 * expected);
    contributions[trial] = result.total_contribution;
  }
  EXPECT_NEAR(entry_areas[1], 64.0 * entry_areas[0], 1e-8 * entry_areas[1]);
  EXPECT_NEAR(contributions[1], contributions[0], 1e-7 * contributions[0]);
}

TEST(SceneMeasure, NativeNormalizationUsesEveryRandomShapeAndPyramidSurface) {
  AxisDistribution axis;
  axis.latitude_dist = { DistributionType::kNoRandom, 90.0f, 0.0f };
  axis.azimuth_dist = { DistributionType::kNoRandom, 180.0f, 0.0f };
  axis.roll_dist = { DistributionType::kNoRandom, 0.0f, 0.0f };

  CrystalConfig random_prism = Prism(1, axis);
  auto& prism = std::get<PrismCrystalParam>(random_prism.param_);
  prism.h_ = { DistributionType::kUniform, 1.0f, 0.4f };
  for (int face = 0; face < 6; ++face) {
    prism.d_[face] = { DistributionType::kUniform, 0.9f + 0.05f * face, 0.2f };
  }
  const auto prism_result = Build(Scene({ random_prism }, { 0.0f }), Request({ 1 }, { { 3, 6 } }, 32));
  double min_surface = std::numeric_limits<double>::infinity();
  double max_surface = 0.0;
  int checked_prisms = 0;
  for (const auto& row : prism_result.rows) {
    const auto& layer = row.layers.front();
    if (layer.status != SceneMeasureStatus::kConfirmed) {
      continue;
    }
    float distances[6];
    for (int face = 0; face < 6; ++face) {
      const std::string name = "face_distance[" + std::to_string(face) + "]";
      distances[face] = static_cast<float>(ShapeValue(layer, name.c_str()));
    }
    const auto geometry = Crystal::CreatePrism(static_cast<float>(ShapeValue(layer, "height")), distances);
    const double expected_surface = SurfaceArea(geometry.CfGeom());
    EXPECT_NEAR(layer.total_surface_area, expected_surface, 1e-6 * expected_surface);
    EXPECT_DOUBLE_EQ(layer.normalized_entry_factor, 2.0 * layer.entry_measure / layer.total_surface_area);
    min_surface = std::min(min_surface, expected_surface);
    max_surface = std::max(max_surface, expected_surface);
    checked_prisms++;
  }
  EXPECT_GT(checked_prisms, 1);
  EXPECT_GT(max_surface - min_surface, 1e-3);

  PyramidCrystalParam pyramid;
  pyramid.h_pyr_u_ = { DistributionType::kNoRandom, 0.4f, 0.0f };
  pyramid.h_prs_ = { DistributionType::kNoRandom, 1.0f, 0.0f };
  pyramid.h_pyr_l_ = { DistributionType::kNoRandom, 0.5f, 0.0f };
  const float pyramid_distances[6] = { 1.0f, 1.1f, 0.9f, 1.2f, 0.8f, 1.05f };
  for (int face = 0; face < 6; ++face) {
    pyramid.d_[face] = { DistributionType::kNoRandom, pyramid_distances[face], 0.0f };
  }
  pyramid.wedge_angle_u_ = 22.0f;
  pyramid.wedge_angle_l_ = 31.0f;
  CrystalConfig pyramid_crystal;
  pyramid_crystal.id_ = 2;
  pyramid_crystal.param_ = pyramid;
  pyramid_crystal.axis_ = axis;
  const auto pyramid_result = Build(Scene({ pyramid_crystal }, { 0.0f }), Request({ 2 }, { { 3, 6 } }, 2));
  const auto geometry = Crystal::CreatePyramid(pyramid.wedge_angle_u_, pyramid.wedge_angle_l_, pyramid.h_pyr_u_.center,
                                               pyramid.h_prs_.center, pyramid.h_pyr_l_.center, pyramid_distances);
  const double expected_surface = SurfaceArea(geometry.CfGeom());
  const auto confirmed = std::find_if(pyramid_result.rows.begin(), pyramid_result.rows.end(), [](const auto& row) {
    return !row.layers.empty() && row.layers.front().status == SceneMeasureStatus::kConfirmed;
  });
  ASSERT_NE(confirmed, pyramid_result.rows.end());
  EXPECT_NEAR(confirmed->layers.front().total_surface_area, expected_surface, 1e-6 * expected_surface);
  EXPECT_DOUBLE_EQ(confirmed->layers.front().normalized_entry_factor,
                   2.0 * confirmed->layers.front().entry_measure / confirmed->layers.front().total_surface_area);
}

TEST(SceneMeasure, NativeContributionMultipliesEachLayerNormalization) {
  AxisDistribution axis;
  axis.latitude_dist = { DistributionType::kNoRandom, 90.0f, 0.0f };
  axis.azimuth_dist = { DistributionType::kNoRandom, 180.0f, 0.0f };
  axis.roll_dist = { DistributionType::kNoRandom, 0.0f, 0.0f };
  const auto result =
      Build(Scene({ Prism(1, axis), Prism(2, axis) }, { 1.0f, 0.0f }), Request({ 1, 2 }, { { 3, 6 }, { 3, 6 } }, 2));
  double expected = 0.0;
  int complete_rows = 0;
  for (const auto& row : result.rows) {
    if (row.layers.size() != 2u || !std::all_of(row.layers.begin(), row.layers.end(), [](const auto& layer) {
          return layer.status == SceneMeasureStatus::kConfirmed;
        })) {
      continue;
    }
    double contribution = row.global_weight * row.joint_sample_mass * row.joint_importance_weight;
    for (const auto& layer : row.layers) {
      contribution *=
          layer.crystal_share * layer.continuation_mass * layer.normalized_entry_factor * layer.fresnel_weight;
    }
    expected += contribution;
    complete_rows++;
  }
  EXPECT_GT(complete_rows, 0);
  EXPECT_NEAR(result.total_contribution, expected, 1e-12 * std::max(1.0, expected));
}

TEST(SceneMeasure, SyncLeaderDeterminesFollowerSupportRatherThanItsDeclaredDistribution) {
  CrystalConfig crystal = Prism(1);
  auto& shape = std::get<PrismCrystalParam>(crystal.param_);
  shape.sync_group_[kShapeScalarHeight] = 7;
  shape.sync_group_[kShapeScalarFace0] = 7;
  shape.d_[0] = { DistributionType::kGaussian, 9.0f, 0.5f };
  const auto result = Build(Scene({ crystal }, { 0.0f }), Request({ 1 }, { { 3, 5 } }, 2));
  const auto follower = std::find_if(result.factors.begin(), result.factors.end(),
                                     [](const auto& factor) { return factor.name == "shape.face_distance[0]"; });
  ASSERT_NE(follower, result.factors.end());
  EXPECT_EQ(follower->support_dimension, 0);
  for (const auto& row : result.rows) {
    EXPECT_DOUBLE_EQ(ShapeValue(row.layers.front(), "face_distance[0]"), 1.0);
  }
}

TEST(SceneMeasure, SyncRandomLeaderIsCountedOnceEvenWhenFollowerDeclaresAnAtom) {
  CrystalConfig crystal = Prism(1);
  auto& shape = std::get<PrismCrystalParam>(crystal.param_);
  shape.h_ = { DistributionType::kGaussian, 1.0f, 0.1f };
  shape.sync_group_[kShapeScalarHeight] = 7;
  shape.sync_group_[kShapeScalarFace0] = 7;
  const auto result = Build(Scene({ crystal }, { 0.0f }), Request({ 1 }, { { 3, 5 } }, 2));
  const auto leader = std::find_if(result.factors.begin(), result.factors.end(),
                                   [](const auto& factor) { return factor.name == "shape.height"; });
  const auto follower = std::find_if(result.factors.begin(), result.factors.end(),
                                     [](const auto& factor) { return factor.name == "shape.face_distance[0]"; });
  ASSERT_NE(leader, result.factors.end());
  ASSERT_NE(follower, result.factors.end());
  EXPECT_EQ(follower->support_dimension, 1);
  EXPECT_EQ(follower->latent_id, leader->latent_id);
  for (const auto& row : result.rows) {
    const auto density = std::find_if(row.latents.begin(), row.latents.end(),
                                      [](const auto& latent) { return latent.name == "shape.height"; });
    if (density == row.latents.end()) {
      ADD_FAILURE() << "the actual leader latent must be present";
      continue;
    }
    EXPECT_NEAR(row.joint_proposal_density, density->proposal_density_or_mass, 1e-12);
  }
}

TEST(SceneMeasure, ShapeTraceCarriesTheLeaderDistributionJacobianThroughSyncAndHeightFold) {
  CrystalConfig crystal = Prism(1);
  auto& shape = std::get<PrismCrystalParam>(crystal.param_);
  shape.h_ = { DistributionType::kUniform, -0.25f, 0.25f };
  shape.sync_group_[kShapeScalarHeight] = 7;
  shape.sync_group_[kShapeScalarFace0] = 7;
  const auto result = Build(Scene({ crystal }, { 0.0f }), Request({ 1 }, { { 3, 5 } }, 2));
  ASSERT_FALSE(result.rows.empty());
  const auto expect_trace = [](const auto& row) {
    ASSERT_FALSE(row.layers.empty());
    const auto& samples = row.layers.front().shape;
    const auto height =
        std::find_if(samples.begin(), samples.end(), [](const auto& sample) { return sample.name == "height"; });
    const auto follower = std::find_if(samples.begin(), samples.end(),
                                       [](const auto& sample) { return sample.name == "face_distance[0]"; });
    const auto atom = std::find_if(samples.begin(), samples.end(),
                                   [](const auto& sample) { return sample.name == "face_distance[1]"; });
    ASSERT_NE(height, samples.end());
    ASSERT_NE(follower, samples.end());
    ASSERT_NE(atom, samples.end());
    EXPECT_TRUE(height->absolute_value_fold);
    EXPECT_LT(height->raw_value, 0.0);
    EXPECT_DOUBLE_EQ(height->mapping_jacobian, 0.25);
    EXPECT_DOUBLE_EQ(follower->mapping_jacobian, 0.25);
    EXPECT_DOUBLE_EQ(atom->mapping_jacobian, 0.0);
    EXPECT_EQ(height->latent_id, follower->latent_id);
  };
  for (const auto& row : result.rows) {
    expect_trace(row);
  }
}

TEST(SceneMeasure, PoleLongitudeAndRollHaveOneRotationSupportDimension) {
  AxisDistribution axis;
  axis.latitude_dist = { DistributionType::kNoRandom, 90.0f, 0.0f };
  axis.azimuth_dist = kFullTurn;
  axis.roll_dist = kFullTurn;
  const auto result = Build(Scene({ Prism(1, axis) }, { 0.0f }), Request({ 1 }, { { 3, 5 } }, 32));
  // At the north pole, longitude and roll rotate about the same world axis.
  // Their sum spans one rotation circle, independent of the chosen angular chart.
  EXPECT_TRUE(std::all_of(result.rows.begin(), result.rows.end(),
                          [](const auto& row) { return row.layers.front().pose_support_rank == 1; }));
}

TEST(SceneMeasure, FiniteLatitudeWidthAtAPoleIsNotAStrictSupportCollapse) {
  AxisDistribution axis;
  axis.latitude_dist = { DistributionType::kGaussianLegacy, 90.0f, 1e-8f };
  axis.azimuth_dist = kFullTurn;
  axis.roll_dist = kFullTurn;
  const auto result = Build(Scene({ Prism(1, axis) }, { 0.0f }), Request({ 1 }, { { 3, 5 } }, 32));
  // All three independent angles have positive width. The intended physical support has
  // dimension three; loss of resolution in a float angular chart must stay unavailable.
  EXPECT_TRUE(std::all_of(result.rows.begin(), result.rows.end(), [](const auto& row) {
    const int rank = row.layers.front().pose_support_rank;
    return rank == 3 || rank == -1;
  }));
}

TEST(SceneMeasure, PoseTangentMatchesIndependentRotationDifferenceAwayFromChartSingularity) {
  AxisDistribution axis;
  axis.latitude_dist = { DistributionType::kNoRandom, 33.0f, 0.0f };
  axis.azimuth_dist = { DistributionType::kNoRandom, 17.0f, 0.0f };
  axis.roll_dist = { DistributionType::kNoRandom, 29.0f, 0.0f };
  const auto result = Build(Scene({ Prism(1, axis) }, { 0.0f }), Request({ 1 }, { { 3, 5 } }, 2));
  ASSERT_FALSE(result.rows.empty());
  ASSERT_FALSE(result.rows.front().layers.empty());
  const auto& layer = result.rows.front().layers.front();
  constexpr float kStep = 1e-3f;
  for (int coordinate = 0; coordinate < 3; coordinate++) {
    float plus[3] = { static_cast<float>(layer.pose_lon_lat_roll_rad[0]),
                      static_cast<float>(layer.pose_lon_lat_roll_rad[1]),
                      static_cast<float>(layer.pose_lon_lat_roll_rad[2]) };
    float minus[3] = { plus[0], plus[1], plus[2] };
    plus[coordinate] += kStep;
    minus[coordinate] -= kStep;
    const Rotation plus_rotation = BuildCrystalRotation(plus[0], plus[1], plus[2]);
    const Rotation minus_rotation = BuildCrystalRotation(minus[0], minus[1], minus[2]);
    for (int element = 0; element < 9; element++) {
      const double finite_difference =
          (plus_rotation.GetMat()[element] - minus_rotation.GetMat()[element]) / (2.0 * kStep);
      EXPECT_NEAR(layer.pose_tangent_drotation[coordinate * 9 + element], finite_difference, 5e-4)
          << "coordinate=" << coordinate << " element=" << element;
    }
  }
}

TEST(SceneMeasure, FiniteSourceNoHitDoesNotClaimAtomicExhaustion) {
  auto config = Scene({ Prism(1) }, { 0.0f });
  config.scene_.light_source_.param_.diameter_ = 180.0f;
  auto request = Request({ 1 }, { { 3, 5 } }, 2);
  request.sun_node_count = 1;
  bool observed_no_hit = false;
  for (uint32_t seed = 1; seed <= 16; seed++) {
    request.seed = seed;
    const auto result = Build(config, request);
    if (result.status_counts.confirmed != 0) {
      continue;
    }
    observed_no_hit = true;
    EXPECT_EQ(result.status, SceneMeasureStatus::kNumericalIncomplete);
    const auto sun = std::find_if(result.factors.begin(), result.factors.end(),
                                  [](const auto& factor) { return factor.name == "sun_disc"; });
    if (sun == result.factors.end()) {
      ADD_FAILURE() << "finite solar source descriptor is missing";
      continue;
    }
    EXPECT_EQ(sun->support_dimension, 2);
  }
  EXPECT_TRUE(observed_no_hit);

  request.path_layers = { { 99 } };
  SunMeasureNode source_atom;
  source_atom.incident_direction[2] = -1.0;
  source_atom.mass = 1.0;
  request.source_sun_nodes = { source_atom };
  const auto enumerated_source = Build(config, request);
  EXPECT_EQ(enumerated_source.status, SceneMeasureStatus::kPhysicallyUnreachable);
  const auto sun = std::find_if(enumerated_source.factors.begin(), enumerated_source.factors.end(),
                                [](const auto& factor) { return factor.name == "sun_disc"; });
  ASSERT_NE(sun, enumerated_source.factors.end());
  EXPECT_EQ(sun->support_dimension, 0);
}

TEST(SceneMeasure, ContinuousIlluminantNoHitDoesNotClaimAtomicExhaustion) {
  auto config = Scene({ Prism(1) }, { 0.0f });
  config.scene_.light_source_.spectrum_ = IlluminantType::kD65;
  auto request = Request({ 1 }, { { 99 } }, 2);
  request.illuminant_node_count = 1;
  const auto result = Build(config, request);
  EXPECT_EQ(result.status, SceneMeasureStatus::kNumericalIncomplete);
  const auto spectrum = std::find_if(result.factors.begin(), result.factors.end(),
                                     [](const auto& factor) { return factor.name == "spectrum"; });
  ASSERT_NE(spectrum, result.factors.end());
  EXPECT_EQ(spectrum->support_dimension, 1);
}

TEST(SceneMeasure, EqualProposalAndTargetRemainUnitImportanceInManyLayers) {
  constexpr int kLayerCount = 64;
  std::vector<CrystalConfig> crystals;
  std::vector<IdType> ids;
  std::vector<float> continuation(kLayerCount, 1.0f);
  continuation.back() = 0.0f;
  std::vector<std::vector<int>> forward(kLayerCount, { 3, 6 });
  std::vector<std::vector<int>> backward(kLayerCount, { 6, 3 });
  AxisDistribution axis;
  axis.latitude_dist = { DistributionType::kGaussianLegacy, 90.0f, 1e-8f };
  axis.azimuth_dist = { DistributionType::kGaussianLegacy, 180.0f, 1e-8f };
  axis.roll_dist = { DistributionType::kGaussianLegacy, 0.0f, 1e-8f };
  for (int i = 0; i < kLayerCount; i++) {
    ids.push_back(i + 1);
    auto crystal = Prism(i + 1, axis);
    auto& shape = std::get<PrismCrystalParam>(crystal.param_);
    shape.h_ = { DistributionType::kGaussian, 1.0f, 1e-8f };
    for (auto& distance : shape.d_) {
      distance = { DistributionType::kGaussian, 1.0f, 1e-8f };
    }
    crystals.push_back(std::move(crystal));
  }
  auto request = Request(ids, forward, 2);
  request.member_selection = SceneMemberSelection::kExplicitChains;
  request.explicit_member_chains = { forward, backward };
  const auto result = Build(Scene(std::move(crystals), continuation), request);
  bool saw_complete_chain = false;
  bool saw_density_underflow = false;
  for (const auto& row : result.rows) {
    if (row.layers.size() != kLayerCount || row.evaluation_status != SceneMeasureStatus::kConfirmed) {
      continue;
    }
    saw_complete_chain = true;
    // Each latent has exactly the same proposal and target. Their ratio is one even when
    // separate products of hundreds of Gaussian densities underflow in floating arithmetic.
    EXPECT_DOUBLE_EQ(row.joint_importance_weight, 1.0);
    EXPECT_EQ(row.joint_importance_weight_status, SceneMeasureNumericStatus::kAvailable);
    EXPECT_TRUE(std::isfinite(row.joint_log_proposal_density));
    EXPECT_DOUBLE_EQ(row.joint_log_proposal_density, row.joint_log_target_density);
    EXPECT_EQ(row.joint_proposal_density_status, row.joint_target_density_status);
    saw_density_underflow =
        saw_density_underflow || row.joint_proposal_density_status == SceneMeasureNumericStatus::kUnderflow;
  }
  EXPECT_TRUE(saw_complete_chain);
  EXPECT_TRUE(saw_density_underflow);
}

TEST(SceneMeasure, OverflowingFiniteSourceWeightHasANumericalStatus) {
  constexpr int kLayerCount = 64;
  std::vector<CrystalConfig> crystals;
  std::vector<IdType> ids;
  std::vector<float> continuation(kLayerCount, 1.0f);
  continuation.back() = 0.0f;
  std::vector<std::vector<int>> forward(kLayerCount, { 3, 6 });
  std::vector<std::vector<int>> backward(kLayerCount, { 6, 3 });
  AxisDistribution axis;
  axis.latitude_dist = { DistributionType::kNoRandom, 90.0f, 0.0f };
  axis.azimuth_dist = { DistributionType::kNoRandom, 180.0f, 0.0f };
  axis.roll_dist = { DistributionType::kNoRandom, 0.0f, 0.0f };
  for (int i = 0; i < kLayerCount; i++) {
    ids.push_back(i + 1);
    auto crystal = Prism(i + 1, axis);
    std::get<PrismCrystalParam>(crystal.param_).h_ = { DistributionType::kNoRandom, 1.0f, 0.0f };
    crystals.push_back(std::move(crystal));
  }
  auto request = Request(ids, forward, 2);
  request.member_selection = SceneMemberSelection::kExplicitChains;
  request.explicit_member_chains = { forward, backward };
  request.spectrum_source = SceneSpectrumSource::kDiagnostic;
  request.diagnostic_wavelengths_nm = { 550.0 };
  request.diagnostic_wavelength_weights = { std::numeric_limits<double>::max() };
  const auto result = Build(Scene(std::move(crystals), continuation), request);
  EXPECT_TRUE(std::any_of(result.rows.begin(), result.rows.end(), [](const auto& row) {
    return row.layers.size() == kLayerCount && std::all_of(row.layers.begin(), row.layers.end(), [](const auto& layer) {
             return layer.status == SceneMeasureStatus::kConfirmed;
           });
  }));
  EXPECT_EQ(result.status, SceneMeasureStatus::kNumericalIncomplete);
  EXPECT_NE(result.reason.find("finite double range"), std::string::npos);
  EXPECT_TRUE(result.total_contribution_status == SceneMeasureNumericStatus::kOverflow ||
              result.coarse_contribution_status == SceneMeasureNumericStatus::kOverflow ||
              result.sampled_measure_mass_status == SceneMeasureNumericStatus::kOverflow);
}

TEST(SceneMeasure, ZeroCrystalEnergyLayerHasAValidZeroMeasure) {
  auto config = Scene({ Prism(1) }, { 0.0f });
  config.scene_.ms_[0].setting_[0].crystal_proportion_ = 0.0f;
  SceneMeasureResult result;
  const auto error = BuildSceneMeasure(config, Request({ 1 }, { { 3, 6 } }, 2), &result);
  ASSERT_TRUE(error.Ok()) << error.message;
  EXPECT_EQ(result.status, SceneMeasureStatus::kZeroWeight);
  EXPECT_DOUBLE_EQ(result.total_contribution, 0.0);
}

TEST(SceneMeasure, PositiveConditionalMassUnderflowIsNotAnExactZeroMeasure) {
  constexpr int kLayerCount = 64;
  AxisDistribution axis;
  axis.latitude_dist = { DistributionType::kNoRandom, 90.0f, 0.0f };
  axis.azimuth_dist = { DistributionType::kNoRandom, 180.0f, 0.0f };
  axis.roll_dist = { DistributionType::kNoRandom, 0.0f, 0.0f };
  std::vector<CrystalConfig> crystals;
  std::vector<IdType> ids;
  std::vector<float> continuation(kLayerCount, 1e-8f);
  continuation.back() = 0.0f;
  for (int layer = 0; layer < kLayerCount; ++layer) {
    crystals.push_back(Prism(layer + 1, axis));
    ids.push_back(layer + 1);
  }
  const auto result = Build(Scene(std::move(crystals), continuation),
                            Request(ids, std::vector<std::vector<int>>(kLayerCount, { 3, 6 }), 2));
  EXPECT_EQ(result.status, SceneMeasureStatus::kNumericalIncomplete);
  EXPECT_EQ(result.status_counts.zero_weight, 0);
  EXPECT_TRUE(std::any_of(result.rows.begin(), result.rows.end(), [](const auto& row) {
    return row.layers.size() == kLayerCount && std::all_of(row.layers.begin(), row.layers.end(), [](const auto& layer) {
             return layer.status == SceneMeasureStatus::kConfirmed && layer.crystal_share > 0.0 &&
                    layer.continuation_mass > 0.0;
           });
  }));
}

TEST(SceneMeasure, AZeroLaterConditionIsKnownEvenWhenAnEarlierFieldIsInvalid) {
  const auto request = Request({ 1, 2 }, { { 99 }, { 3, 6 } }, 2);
  const auto zero = Build(Scene({ Prism(1), Prism(2) }, { 0.5f, 1.0f }), request);
  EXPECT_EQ(zero.status, SceneMeasureStatus::kZeroWeight);
  EXPECT_TRUE(std::all_of(zero.rows.begin(), zero.rows.end(),
                          [](const auto& row) { return row.status == SceneMeasureStatus::kZeroWeight; }));
  const auto positive_mass_control = Build(Scene({ Prism(1), Prism(2) }, { 0.5f, 0.0f }), request);
  EXPECT_EQ(positive_mass_control.status, SceneMeasureStatus::kPhysicallyUnreachable);
}

TEST(SceneMeasure, SelectedZeroShareIsZeroMeasureWithinAPositiveMixture) {
  AxisDistribution axis;
  axis.latitude_dist = { DistributionType::kNoRandom, 90.0f, 0.0f };
  axis.azimuth_dist = { DistributionType::kNoRandom, 180.0f, 0.0f };
  axis.roll_dist = { DistributionType::kNoRandom, 0.0f, 0.0f };
  auto config = Scene({ Prism(1, axis) }, { 0.0f });
  config.scene_.ms_[0].setting_[0].crystal_proportion_ = 0.0f;
  ScatteringSetting positive{};
  positive.crystal_ = Prism(2, axis);
  positive.crystal_proportion_ = 1.0f;
  config.scene_.ms_[0].setting_.push_back(positive);
  config.crystals_.emplace(2, positive.crystal_);
  const auto result = Build(config, Request({ 1 }, { { 3, 6 } }, 2));
  EXPECT_EQ(result.status, SceneMeasureStatus::kZeroWeight);
  EXPECT_DOUBLE_EQ(result.total_contribution, 0.0);
  EXPECT_TRUE(std::all_of(result.rows.begin(), result.rows.end(),
                          [](const auto& row) { return row.status == SceneMeasureStatus::kZeroWeight; }));
  const auto control = Build(config, Request({ 2 }, { { 3, 6 } }, 2));
  EXPECT_GT(control.total_contribution, 0.0);
}

TEST(SceneMeasure, ZeroExitProbabilityIsZeroPathMeasure) {
  AxisDistribution axis;
  axis.latitude_dist = { DistributionType::kNoRandom, 90.0f, 0.0f };
  axis.azimuth_dist = { DistributionType::kNoRandom, 180.0f, 0.0f };
  axis.roll_dist = { DistributionType::kNoRandom, 0.0f, 0.0f };
  const auto zero = Build(Scene({ Prism(1, axis) }, { 1.0f }), Request({ 1 }, { { 3, 6 } }, 2));
  EXPECT_EQ(zero.status, SceneMeasureStatus::kZeroWeight);
  EXPECT_DOUBLE_EQ(zero.total_contribution, 0.0);
  const auto control = Build(Scene({ Prism(1, axis) }, { 0.0f }), Request({ 1 }, { { 3, 6 } }, 2));
  EXPECT_GT(control.total_contribution, 0.0);
}

TEST(SceneMeasure, ZeroContinuationMakesTheWholeMultiLayerChainAZeroMeasure) {
  AxisDistribution axis;
  axis.latitude_dist = { DistributionType::kNoRandom, 90.0f, 0.0f };
  axis.azimuth_dist = { DistributionType::kNoRandom, 180.0f, 0.0f };
  axis.roll_dist = { DistributionType::kNoRandom, 0.0f, 0.0f };
  const auto request = Request({ 1, 2 }, { { 3, 6 }, { 3, 6 } }, 2);
  const auto zero = Build(Scene({ Prism(1, axis), Prism(2, axis) }, { 0.0f, 0.0f }), request);
  EXPECT_EQ(zero.status, SceneMeasureStatus::kZeroWeight);
  EXPECT_DOUBLE_EQ(zero.total_contribution, 0.0);
  EXPECT_TRUE(std::all_of(zero.rows.begin(), zero.rows.end(), [](const auto& row) {
    return row.status == SceneMeasureStatus::kZeroWeight && !row.layers.empty() &&
           row.layers.front().continuation_mass == 0.0;
  }));
  const auto control = Build(Scene({ Prism(1, axis), Prism(2, axis) }, { 1.0f, 0.0f }), request);
  EXPECT_GT(control.total_contribution, 0.0);
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
