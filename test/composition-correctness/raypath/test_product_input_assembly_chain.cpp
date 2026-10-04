#include <cmath>

#include "core/color_util.hpp"
#include "core/lat_lut.hpp"
#include "core/simulator.hpp"
#include "gtest/gtest.h"
#include "raypath/product_input_assembly.hpp"

namespace {
namespace ns = lumice;
namespace rp = lumice::raypath;

ns::SceneConfig Scene(size_t count, bool random_shape = false) {
  ns::SceneConfig scene{};
  scene.light_source_.param_ = { 0.f, 0.f, 0.f };
  scene.light_source_.spectrum_ = std::vector<ns::WlParam>{ { 450.f, 2.f }, { 550.f, .3f }, { 650.f, 4.f } };
  for (size_t i = 0; i < count; ++i) {
    ns::PrismCrystalParam p;
    p.h_ = { ns::DistributionType::kNoRandom, 1.f, 0.f };
    for (auto& d : p.d_)
      d = { random_shape ? ns::DistributionType::kUniform : ns::DistributionType::kNoRandom, 1.f,
            random_shape ? .4f : 0.f };
    ns::ScatteringSetting entry{};
    entry.crystal_ = { static_cast<ns::IdType>(i + 1), p, {} };
    entry.crystal_.axis_.azimuth_dist = { ns::DistributionType::kNoRandom, 180.f, 0.f };
    entry.crystal_.axis_.roll_dist = { ns::DistributionType::kUniform, 0.f, 360.f };
    entry.crystal_proportion_ = 1.f;
    scene.ms_.push_back({ .5f, { entry } });
  }
  return scene;
}

rp::ProductInputSnapshot Capture(const ns::SceneConfig& scene, uint8_t bits = 0, std::vector<int> faces = { 3, 6 }) {
  std::vector<rp::ProductLayerSelection> selections;
  for (size_t i = 0; i < scene.ms_.size(); ++i)
    selections.push_back({ i, static_cast<ns::IdType>(i + 1), faces, bits });
  rp::ProductInputSnapshot snapshot;
  const auto error = rp::CaptureProductInput(scene, "scene revision 31", selections, &snapshot);
  EXPECT_TRUE(error.Ok()) << error.message;
  return snapshot;
}

std::vector<rp::ProductLayerSample> Samples(const rp::ProductInputSnapshot& snapshot) {
  std::vector<rp::ProductLayerSample> samples;
  for (const auto& layer : snapshot.layers) {
    const auto plan = std::visit([](const auto& p) { return ns::BuildShapeDrawPlan(p); }, layer.crystal.param_);
    rp::ProductLayerSample sample;
    sample.identity = { snapshot.scene_identity, layer.layer_index, layer.crystal.id_ };
    sample.axis = rp::DistributedAxisDraw{ ns::DistributionLatentDraw{}, {}, { .5f } };
    sample.provenance = "explicit shape leaders and pose draw";
    for (int i = 0; i < ns::kShapeScalarCount; ++i) {
      if (plan[i].applicable && plan[i].leader_slot == i) {
        sample.shape.values[sample.shape.size++] = { i, plan[i].distribution.center };
      }
    }
    samples.push_back(sample);
  }
  return samples;
}

rp::ProductInput Assemble(const rp::ProductInputSnapshot& snapshot,
                          const std::vector<rp::ProductLayerSample>& samples) {
  rp::ProductInput input;
  const auto error =
      rp::AssembleProductInput(snapshot, samples, { { 1.f, .3f }, "cap sample" }, rp::DiscreteSpectrumSum{}, &input);
  EXPECT_TRUE(error.Ok()) << error.message;
  return input;
}

TEST(ProductInputChain, TwoLayersUseActualDirectionsOneWavelengthAndOneSpectralCharge) {
  const auto snapshot = Capture(Scene(2));
  const auto input = Assemble(snapshot, Samples(snapshot));
  ASSERT_EQ(input.layers.size(), 2u);
  ASSERT_EQ(input.spectrum.rows.size(), 3u);
  // Unit-height regular prism with circumradius 1/2: each side area is 1/2,
  // and S = 6*(1/2) + 2*(3*sqrt(3)/8). Normal 3->6 transmits through a slab.
  const double surface = 3 + 3 * std::sqrt(3.) / 4;
  for (size_t wi = 0; wi < input.spectrum.rows.size(); ++wi) {
    SCOPED_TRACE(wi);
    rp::ProductChainEvaluation result;
    const auto error = rp::EvaluateProductChain(input, { 0, 0 }, wi, &result);
    EXPECT_TRUE(error.Ok()) << error.message;
    if (!error.Ok() || result.layers.size() != 2u) {
      ADD_FAILURE() << "chain was not evaluated";
      continue;
    }
    const double n = input.spectrum.rows[wi].refractive_index;
    const double reflect = std::pow((n - 1) / (n + 1), 2);
    const double transmission = std::pow(1 - reflect, 2);
    for (const auto& layer : result.layers) {
      EXPECT_EQ(layer.status, rp::ProductContributionStatus::kPositive);
      EXPECT_NEAR(layer.entry_area, .5, 1e-6);
      EXPECT_NEAR(layer.entry_weight, 1 / surface, 1e-6);
      EXPECT_NEAR(layer.interface_product, transmission, 1e-12);
    }
    EXPECT_EQ(result.layers[1].incident, result.layers[0].outgoing);
    const double expected = std::pow(transmission / surface, 2);
    EXPECT_NEAR(result.optical_weight, expected, 1e-7);
    for (int j = 0; j < 3; ++j)
      EXPECT_NEAR(result.xyz[j], expected * input.spectrum.rows[wi].coefficient[j], 1e-7);
  }
  EXPECT_TRUE(ns::analytic::ValidateRotation(input.layers[0].analytic_pose.data()));
}

TEST(ProductInputChain, EnsembleSetsStayFixedWhileActualDrawChangesGeometryAndContributions) {
  auto scene = Scene(2, true);
  scene.light_source_.param_ = { 7.f, 23.f, .5f };
  auto& p = std::get<ns::PrismCrystalParam>(scene.ms_[1].setting_[0].crystal_.param_);
  p.sync_group_[ns::kShapeScalarFace0] = p.sync_group_[ns::kShapeScalarFace3] = 1;
  const auto snapshot = Capture(scene, ns::sym::kSymP);
  auto samples = Samples(snapshot);
  const auto regular = Assemble(snapshot, samples);
  // A different draw from the SAME iid ensemble, not a different config.
  samples[0].shape.values[1].value = .8f;
  samples[1].shape.values[1].value = 1.1f;
  const auto irregular = Assemble(snapshot, samples);
  ASSERT_EQ(irregular.layers.size(), 2u);
  EXPECT_EQ(regular.layers[0].scope.members, irregular.layers[0].scope.members);
  EXPECT_EQ(regular.layers[1].scope.members, irregular.layers[1].scope.members);
  EXPECT_NE(regular.layers[0].surface_area, irregular.layers[0].surface_area);
  EXPECT_EQ(irregular.layers[1].shape.face_distance[0], irregular.layers[1].shape.face_distance[3]);
  std::vector<size_t> cursor(2, 0);
  double sum = 0;
  double representative = 0;
  size_t visited = 0;
  do {
    rp::ProductChainEvaluation eval;
    EXPECT_TRUE(rp::EvaluateProductChain(irregular, cursor, 0, &eval).Ok());
    if (visited == 0)
      representative = eval.optical_weight;
    sum += eval.optical_weight;
    visited++;
  } while (rp::NextProductMemberChain(irregular, &cursor));
  const auto count = irregular.layers[0].scope.members.size() * irregular.layers[1].scope.members.size();
  EXPECT_EQ(visited, count);
  EXPECT_GT(sum, 0);
  EXPECT_GT(std::abs(sum - count * representative), 1e-4);
  // Factorized storage grows by a sum; no spectral/member Cartesian rows exist.
  EXPECT_EQ(irregular.spectrum.rows.size(), 3u);
  EXPECT_EQ(irregular.layers[0].scope.members.size(), 6u);
  EXPECT_EQ(irregular.layers[1].scope.members.size(), 2u);
}

TEST(ProductInputChain, SnapshotSurvivesSceneMutationAndSourceDestruction) {
  rp::ProductInputSnapshot snapshot;
  {
    auto scene = Scene(1);
    snapshot = Capture(scene, ns::sym::kSymP);
    scene.ms_.clear();
    scene.light_source_.spectrum_ = std::vector<ns::WlParam>{ { 700.f, 99.f } };
  }
  const auto input = Assemble(snapshot, Samples(snapshot));
  ASSERT_EQ(input.layers.size(), 1u);
  EXPECT_EQ(input.layers[0].scope.members.size(), 6u);
  EXPECT_EQ(input.spectrum.rows[0].wavelength_nm, 450.f);
  EXPECT_EQ(input.layers[0].scope.snapshot.scene_identity, snapshot.scene_identity);
  auto bad = snapshot;
  bad.layers[0].scene_identity = "later scene";
  rp::ProductInput output;
  EXPECT_FALSE(
      rp::AssembleProductInput(bad, Samples(bad), { { .4f, .5f }, "cap" }, rp::DiscreteSpectrumSum{}, &output).Ok());
  EXPECT_TRUE(output.layers.empty());
}

TEST(ProductInputChain, SamplesMustBelongToTheirSnapshotLayerAndCrystal) {
  const auto snapshot = Capture(Scene(2));
  auto samples = Samples(snapshot);
  rp::ProductInput output;
  std::swap(samples[0], samples[1]);
  EXPECT_FALSE(
      rp::AssembleProductInput(snapshot, samples, { { .4f, .5f }, "cap" }, rp::DiscreteSpectrumSum{}, &output).Ok());
  EXPECT_TRUE(output.layers.empty());
  samples = Samples(snapshot);
  samples[0].identity.scene_identity = "another revision";
  EXPECT_FALSE(
      rp::AssembleProductInput(snapshot, samples, { { .4f, .5f }, "cap" }, rp::DiscreteSpectrumSum{}, &output).Ok());
  EXPECT_TRUE(output.layers.empty());
  samples = Samples(snapshot);
  samples[0].identity.crystal_id = samples[1].identity.crystal_id;
  EXPECT_FALSE(
      rp::AssembleProductInput(snapshot, samples, { { .4f, .5f }, "cap" }, rp::DiscreteSpectrumSum{}, &output).Ok());
  EXPECT_TRUE(output.layers.empty());
}

TEST(ProductInputChain, ZeroPrismPyramidIsNotRejectedAndMissingFaceIsNotEmptyCrystal) {
  auto scene = Scene(1);
  ns::PyramidCrystalParam p;
  p.h_prs_ = { ns::DistributionType::kNoRandom, 0.f, 0.f };
  p.h_pyr_u_ = p.h_pyr_l_ = { ns::DistributionType::kNoRandom, .7f, 0.f };
  for (auto& d : p.d_)
    d = { ns::DistributionType::kNoRandom, 1.f, 0.f };
  scene.ms_[0].setting_[0].crystal_.param_ = p;
  const auto snapshot = Capture(scene);
  const auto input = Assemble(snapshot, Samples(snapshot));
  ASSERT_EQ(input.layers.size(), 1u);
  EXPECT_EQ(input.layers[0].geometry_status, ns::analytic::Status::kOk);
  EXPECT_LT(input.layers[0].normals.SlotOf(3), 0);
  EXPECT_GE(input.layers[0].normals.SlotOf(13), 0);
  rp::ProductChainEvaluation missing;
  ASSERT_TRUE(rp::EvaluateProductChain(input, { 0 }, 0, &missing).Ok());
  ASSERT_EQ(missing.layers.size(), 1u);
  EXPECT_EQ(missing.layers[0].status, rp::ProductContributionStatus::kMissingFace);
  EXPECT_EQ(missing.optical_weight, 0);
  // Opposite signed planes cannot bound a volume. The product factory rejects
  // it; assembly preserves that status rather than replacing the actual draw.
  auto samples = Samples(snapshot);
  for (size_t i = 0; i < samples[0].shape.size; ++i) {
    if (samples[0].shape.values[i].slot >= ns::kShapeScalarFace0)
      samples[0].shape.values[i].value = -1.f;
  }
  const auto empty = Assemble(snapshot, samples);
  EXPECT_EQ(empty.layers[0].geometry_status, ns::analytic::Status::kInvalidConfig);
  rp::ProductChainEvaluation rejected;
  ASSERT_TRUE(rp::EvaluateProductChain(empty, { 0 }, 0, &rejected).Ok());
  ASSERT_EQ(rejected.layers.size(), 1u);
  EXPECT_EQ(rejected.layers[0].status, rp::ProductContributionStatus::kRejectedShape);
}
TEST(ProductInputChain, BentFirstLayerFeedsItsExitToTheNextLayer) {
  auto scene = Scene(2);
  scene.ms_[0].setting_[0].crystal_.axis_.azimuth_dist = { ns::DistributionType::kNoRandom, 221.f, 0.f };
  auto snapshot = Capture(scene);
  snapshot.layers[0].representative = { 3, 5 };
  const auto input = Assemble(snapshot, Samples(snapshot));
  rp::ProductChainEvaluation bent;
  ASSERT_TRUE(rp::EvaluateProductChain(input, { 0, 0 }, 0, &bent).Ok());
  ASSERT_EQ(bent.layers.size(), 2u);
  EXPECT_GT(bent.optical_weight, 0);
  EXPECT_EQ(bent.layers[1].incident, bent.layers[0].outgoing);
  EXPECT_GT(std::abs(bent.layers[1].incident[1] - input.source.incident_direction[1]), .2);
  // If the second layer incorrectly restarts at the sun, it becomes the
  // normal-incidence slab: its entry area and Fresnel product both change.
  const auto straight_snapshot = Capture(Scene(1));
  const auto straight_input = Assemble(straight_snapshot, Samples(straight_snapshot));
  rp::ProductChainEvaluation straight;
  ASSERT_TRUE(rp::EvaluateProductChain(straight_input, { 0 }, 0, &straight).Ok());
  ASSERT_EQ(straight.layers.size(), 1u);
  EXPECT_GT(std::abs(bent.layers[1].entry_area - straight.layers[0].entry_area), .01);
  EXPECT_GT(std::abs(bent.layers[1].interface_product - straight.layers[0].interface_product), 1e-4);
}

TEST(ProductInputChain, AllAxisBranchesReachTheProductMatrixWithoutDrawingRng) {
  const ns::DistributionType types[]{ ns::DistributionType::kNoRandom,  ns::DistributionType::kGaussianLegacy,
                                      ns::DistributionType::kUniform,   ns::DistributionType::kGaussian,
                                      ns::DistributionType::kLaplacian, ns::DistributionType::kZigzag };
  for (auto type : types) {
    SCOPED_TRACE(static_cast<int>(type));
    auto scene = Scene(1);
    auto& axis = scene.ms_[0].setting_[0].crystal_.axis_;
    axis.latitude_dist = { type, 100.f, 20.f };
    axis.roll_dist = { ns::DistributionType::kZigzag, 0.f, 30.f };
    const auto snapshot = Capture(scene);
    auto samples = Samples(snapshot);
    rp::DistributedAxisDraw draw{ ns::DistributionLatentDraw{ 1.f }, {}, { .75f } };
    ns::LatitudeSample latitude;
    if (type == ns::DistributionType::kNoRandom)
      latitude = { 100.f * ns::math::kDegreeToRad, false };
    else if (type == ns::DistributionType::kGaussianLegacy)
      latitude = ns::TransformLegacyLatitude(120.f);
    else {
      draw.latitude = ns::LatitudeLutDraw{ .4f, .3f };
      latitude = ns::TransformLatitudeLut(*ns::GetSharedLatLut(axis.latitude_dist), { .4f, .3f });
    }
    samples[0].axis = draw;
    ns::RandomNumberGenerator expected_rng = ns::RandomNumberGenerator::GetInstance();
    const auto input = Assemble(snapshot, samples);
    EXPECT_EQ(ns::RandomNumberGenerator::GetInstance().GetUniform(), expected_rng.GetUniform());
    if (input.layers.size() != 1u) {
      ADD_FAILURE();
      continue;
    }
    const auto angles = ns::ComposeAxisAngles(latitude, 180.f, 30.f);
    const auto expected = ns::BuildCrystalRotation(angles[0], angles[1], angles[2]);
    for (int j = 0; j < 9; ++j)
      EXPECT_EQ(input.layers[0].product_pose[j], expected.GetMat()[j]);
    EXPECT_TRUE(ns::analytic::ValidateRotation(input.layers[0].analytic_pose.data()));
  }
  auto scene = Scene(1);
  auto& axis = scene.ms_[0].setting_[0].crystal_.axis_;
  axis.latitude_dist = { ns::DistributionType::kUniform, 90.f, 360.f };
  axis.azimuth_dist = axis.roll_dist = { ns::DistributionType::kUniform, 0.f, 360.f };
  const auto snapshot = Capture(scene);
  auto samples = Samples(snapshot);
  samples[0].axis = rp::FullSphereAxisDraw{ .3f, .7f, { .2f } };
  const auto input = Assemble(snapshot, samples);
  ASSERT_EQ(input.layers.size(), 1u);
  const auto point = ns::TransformFullSpherePoint(.3f, .7f);
  EXPECT_EQ(input.layers[0].angles[0], point[0]);
  EXPECT_EQ(input.layers[0].angles[1], point[1]);
  EXPECT_EQ(input.layers[0].angles[2], (.2f - .5f) * 360.f * ns::math::kDegreeToRad);
}

}  // namespace
