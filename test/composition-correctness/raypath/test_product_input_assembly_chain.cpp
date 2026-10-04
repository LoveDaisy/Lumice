#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>

#include "analytic/so3.hpp"
#include "core/color_util.hpp"
#include "core/lat_lut.hpp"
#include "core/simulator.hpp"
#include "gtest/gtest.h"
#include "raypath/product_diagnostic_sampler.hpp"
#include "raypath/product_feature_discovery.hpp"
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

TEST(ProductInputChain, DiagnosticMeasureKeepsWholeOuterDrawsAndReplayableSources) {
  const auto snapshot = Capture(Scene(1));
  const rp::ProductDiagnosticSampler sampler(snapshot, 1497, rp::DiscreteSpectrumSum{});
  rp::ProductDiagnosticMeasure out;
  rp::ProductSamplingBudget budget{ 32, 96 };
  ASSERT_TRUE(rp::BuildProductDiagnosticMeasure(sampler, budget, &out).Ok());
  EXPECT_EQ(out.completed_samples, 32u);
  EXPECT_EQ(out.optical_evaluations, 96u);
  EXPECT_FALSE(out.budget_exhausted);
  EXPECT_EQ(out.sources.size(), out.components.size());
  rp::ProductInput invalid_replay;
  EXPECT_FALSE(rp::ReplayDiagnosticSource(out, out.sources.size(), &invalid_replay).Ok());
  ASSERT_GT(out.components.size(), 0u);
  for (const auto& component : out.components) {
    const auto& source = out.sources[component.source_token];
    rp::ProductInput input;
    rp::ProductChainEvaluation physical;
    if (!rp::ReplayDiagnosticSource(out, component.source_token, &input).Ok() ||
        !rp::EvaluateProductChain(input, { source.member_index }, source.spectral_row, &physical).Ok()) {
      ADD_FAILURE();
      return;
    }
    EXPECT_EQ(component.sample_index, source.sample_index);
    for (int j = 0; j < 3; ++j) {
      EXPECT_DOUBLE_EQ(component.xyz_weight[j], physical.xyz[j] / 32);
      EXPECT_DOUBLE_EQ(component.direction[j], -physical.layers[0].outgoing[j]);
    }
  }
  budget.max_optical_evaluations = 5;
  ASSERT_TRUE(rp::BuildProductDiagnosticMeasure(sampler, budget, &out).Ok());
  EXPECT_TRUE(out.budget_exhausted);
  EXPECT_EQ(out.completed_samples, 1u);
  EXPECT_EQ(out.optical_evaluations, 5u);
  for (const auto& component : out.components) {
    EXPECT_EQ(component.sample_index, 0u);
  }
  budget.deadline = std::chrono::steady_clock::now();
  ASSERT_TRUE(rp::BuildProductDiagnosticMeasure(sampler, budget, &out).Ok());
  EXPECT_TRUE(out.budget_exhausted);
  EXPECT_EQ(out.completed_samples, 0u);
  EXPECT_EQ(out.optical_evaluations, 0u);
  const rp::ProductDiagnosticSampler multi(Capture(Scene(2)), 1497, rp::DiscreteSpectrumSum{});
  EXPECT_EQ(rp::BuildProductDiagnosticMeasure(multi, {}, &out).code, rp::ErrorCode::kMultiLayerUnsupported);
}

TEST(ProductInputChain, SourceReplayOwnsSpectrumAfterCallerAndSamplerExpire) {
  auto scene = Scene(1);
  rp::ProductDiagnosticMeasure first;
  rp::ProductDiagnosticMeasure second;
  {
    auto snapshot = Capture(scene);
    rp::ProductSpectrumRequest request = rp::ProductWavelengthSample{ 450.f, 0, "first spectrum" };
    const rp::ProductDiagnosticSampler a(snapshot, 1497, request);
    request = rp::ProductWavelengthSample{ 650.f, 2, "second spectrum" };
    const rp::ProductDiagnosticSampler b(snapshot, 1497, request);
    ASSERT_TRUE(rp::BuildProductDiagnosticMeasure(a, { 32, 32 }, &first).Ok());
    ASSERT_TRUE(rp::BuildProductDiagnosticMeasure(b, { 32, 32 }, &second).Ok());
  }
  scene.ms_.clear();
  ASSERT_FALSE(first.sources.empty());
  ASSERT_FALSE(second.sources.empty());
  rp::ProductInput a;
  rp::ProductInput b;
  ASSERT_TRUE(rp::ReplayDiagnosticSource(first, 0, &a).Ok());
  ASSERT_TRUE(rp::ReplayDiagnosticSource(second, 0, &b).Ok());
  ASSERT_EQ(a.spectrum.rows.size(), 1u);
  ASSERT_EQ(b.spectrum.rows.size(), 1u);
  EXPECT_EQ(a.spectrum.rows[0].wavelength_nm, 450.f);
  EXPECT_EQ(a.spectrum.rows[0].source_weight, 2.f);
  EXPECT_EQ(b.spectrum.rows[0].wavelength_nm, 650.f);
  EXPECT_EQ(b.spectrum.rows[0].source_weight, 4.f);
  EXPECT_NE(a.spectrum.rows[0].coefficient, b.spectrum.rows[0].coefficient);
  // No replay call accepts a replacement spectrum or an unrelated sampler.
  rp::ProductChainEvaluation physical;
  ASSERT_TRUE(rp::EvaluateProductChain(a, { first.sources[0].member_index }, 0, &physical).Ok());
  for (int j = 0; j < 3; ++j) {
    EXPECT_DOUBLE_EQ(first.components[0].xyz_weight[j], physical.xyz[j] / first.completed_samples);
  }
}

TEST(ProductInputChain, JointSamplerReplaysPrefixesAndCorrelatedShapeFromSnapshot) {
  auto scene = Scene(1, true);
  scene.light_source_.param_ = { 20.f, 13.f, 1.4f };
  auto& crystal = scene.ms_[0].setting_[0].crystal_;
  auto& shape = std::get<ns::PrismCrystalParam>(crystal.param_);
  shape.sync_group_[ns::kShapeScalarFace0] = shape.sync_group_[ns::kShapeScalarFace3] = 1;
  crystal.axis_.azimuth_dist = { ns::DistributionType::kUniform, 0.f, 360.f };
  crystal.axis_.latitude_dist = { ns::DistributionType::kGaussian, 88.f, 2.f };
  const auto snapshot = Capture(scene, 0, { 3, 5 });
  const rp::ProductDiagnosticSampler sampler(snapshot, 1497, rp::DiscreteSpectrumSum{});
  rp::ProductInput a;
  rp::ProductInput b;
  ASSERT_TRUE(sampler.Draw(17, &a).Ok());
  ASSERT_TRUE(sampler.Draw((uint64_t{ 1 } << 32) + 17, &b).Ok());
  EXPECT_NE(a.layers[0].analytic_pose, b.layers[0].analytic_pose);
  ASSERT_TRUE(sampler.Draw(17, &b).Ok());
  EXPECT_EQ(a.layers[0].analytic_pose, b.layers[0].analytic_pose);
  EXPECT_EQ(a.source.incident_direction, b.source.incident_direction);
  EXPECT_EQ(a.layers[0].shape_sample.raw, b.layers[0].shape_sample.raw);
  EXPECT_EQ(a.layers[0].shape.face_distance[0], a.layers[0].shape.face_distance[3]);
  uint32_t next = 0;
  for (const auto& dimension : sampler.Dimensions()) {
    EXPECT_EQ(dimension.offset, next);
    next += dimension.width;
  }
  EXPECT_GT(next, 8u);
  for (const auto type :
       { ns::DistributionType::kNoRandom, ns::DistributionType::kUniform, ns::DistributionType::kGaussian,
         ns::DistributionType::kLaplacian, ns::DistributionType::kZigzag, ns::DistributionType::kGaussianLegacy }) {
    auto changed = snapshot;
    changed.layers[0].crystal.axis_.latitude_dist = { type, 20.f, 5.f };
    changed.layers[0].crystal.axis_.azimuth_dist.type = type;
    const rp::ProductDiagnosticSampler branch(changed, 1497, rp::DiscreteSpectrumSum{});
    for (const auto& dimension : branch.Dimensions()) {
      if (dimension.name == "layer.0.azimuth") {
        EXPECT_EQ(dimension.width,
                  ns::BuildDistributionDrawPlan(changed.layers[0].crystal.axis_.azimuth_dist).uniform_count);
      }
    }
    if (!branch.Draw(17, &b).Ok()) {
      ADD_FAILURE() << "valid distribution branch rejected";
      return;
    }
    EXPECT_TRUE(ns::analytic::ValidateRotation(b.layers[0].analytic_pose.data()));
  }
  scene.ms_.clear();
  ASSERT_TRUE(sampler.Draw(17, &b).Ok());
  EXPECT_EQ(a.layers[0].analytic_pose, b.layers[0].analytic_pose);
}

TEST(ProductInputChain, GaussianDensityPeakMatchesIndependentPhysicalQuadrature) {
  auto scene = Scene(1);
  scene.light_source_.param_ = { 20.f, 0.f, 0.f };
  scene.light_source_.spectrum_ = std::vector<ns::WlParam>{ { 550.f, 1.f } };
  auto& axis = scene.ms_[0].setting_[0].crystal_.axis_;
  axis.latitude_dist = { ns::DistributionType::kNoRandom, 90.f, 0.f };
  axis.azimuth_dist = { ns::DistributionType::kNoRandom, 180.f, 0.f };
  axis.roll_dist = { ns::DistributionType::kGaussian, static_cast<float>(180 / 3.14159265358979323846),
                     static_cast<float>(.02 * 180 / 3.14159265358979323846) };
  const auto snapshot = Capture(scene, 0, { 3, 5 });
  // Independent LI geometry/optics, Gaussian angular quadrature at two orders.
  // See the fixture for inputs, raw results and the reference's own error.
  std::ifstream input(std::string(LUMICE_DIAGNOSTIC_FIXTURE_DIR) + "/gaussian-density.json");
  ASSERT_TRUE(input.good());
  const auto fixture = nlohmann::json::parse(input);
  const auto& references = fixture.at("reference");
  ASSERT_EQ(references.size(), 4u);
  for (uint32_t seed : { 1497u, 9713u }) {
    const rp::ProductDiagnosticSampler sampler(snapshot, seed, rp::DiscreteSpectrumSum{});
    rp::ProductDiagnosticMeasure measure;
    if (!rp::BuildProductDiagnosticMeasure(sampler, { 262144, 262144 }, &measure).Ok()) {
      ADD_FAILURE();
      return;
    }
    EXPECT_EQ(measure.completed_samples, 262144u);
    std::array<double, 3> mean{};
    for (const auto& row : measure.components) {
      for (int j = 0; j < 3; ++j) {
        mean[j] += row.xyz_weight[1] * row.direction[j];
      }
    }
    const double norm = ns::analytic::so3::Norm3(mean.data());
    for (auto& v : mean) {
      v /= norm;
    }
    for (int scale = 0; scale < 2; ++scale) {
      const double h = (.01 * (scale + 1)) * 3.14159265358979323846 / 180;
      const auto peak = ns::analytic::CorrectSphericalField(
          measure.components, mean, { ns::analytic::FieldEquation::kLogYPeak, 0, h, 1e-10, h * .4, 64 }, nullptr);
      if (peak.status != ns::analytic::FieldSolveStatus::kConverged) {
        ADD_FAILURE() << "seed=" << seed << " scale=" << scale;
        return;
      }
      const double az = std::atan2(peak.query.direction[1], peak.query.direction[0]) * 180 / 3.14159265358979323846;
      // Local 0.002-degree resolution, 1/5 of the narrower observation width;
      // reference self-difference is <0.000016 degrees. Not a global product bar.
      EXPECT_NEAR(az, references.at(2 + scale).at("az_deg").get<double>(), .002);
      EXPECT_LT(peak.log_y_curvatures[1], 0);
      EXPECT_GT(peak.field.xyz[1].value, 0);
      EXPECT_GT(peak.field.effective_samples_y, 10000);
      auto displaced = peak.query;
      const double delta = .15 * 3.14159265358979323846 / 180;
      for (int j = 0; j < 3; ++j) {
        displaced.direction[j] = std::cos(delta) * peak.query.direction[j] + std::sin(delta) * peak.query.basis[0][j];
        displaced.basis[0][j] = -std::sin(delta) * peak.query.direction[j] + std::cos(delta) * peak.query.basis[0][j];
      }
      ns::analytic::SphericalFieldValue flank;
      EXPECT_TRUE(ns::analytic::EvaluateSphericalField(measure.components, displaced, &flank));
      EXPECT_GT(peak.field.xyz[1].value, flank.xyz[1].value);
    }
  }
}

TEST(ProductInputChain, CorrelatedShapeFiniteSunColourCrossingsMatchIndependentPhysics) {
  std::ifstream in(std::string(LUMICE_DIAGNOSTIC_FIXTURE_DIR) + "/physical-colour.json");
  ASSERT_TRUE(in.good());
  const auto fixture = nlohmann::json::parse(in);
  auto scene = Scene(1);
  scene.light_source_.param_ = { 20.f, 0.f, .53f };
  scene.light_source_.spectrum_ = std::vector<ns::WlParam>{ { 450.f, .2f }, { 550.f, .3f }, { 650.f, .5f } };
  auto& crystal = scene.ms_[0].setting_[0].crystal_;
  auto& shape = std::get<ns::PrismCrystalParam>(crystal.param_);
  shape.h_ = shape.d_[0] = { ns::DistributionType::kUniform, 1.f, 1.4f };
  shape.sync_group_[ns::kShapeScalarHeight] = shape.sync_group_[ns::kShapeScalarFace0] = 1;
  crystal.axis_.latitude_dist = { ns::DistributionType::kGaussian, 90.f, 1.f };
  crystal.axis_.azimuth_dist = crystal.axis_.roll_dist = { ns::DistributionType::kUniform, 0.f, 360.f };
  const auto snapshot = Capture(scene, 0, { 3, 5 });
  constexpr double kRad = 3.14159265358979323846 / 180;
  for (uint32_t seed : { 1497u, 9713u }) {
    const rp::ProductDiagnosticSampler sampler(snapshot, seed, rp::DiscreteSpectrumSum{});
    rp::ProductDiagnosticMeasure measure;
    if (!rp::BuildProductDiagnosticMeasure(sampler, { 65536, 196608 }, &measure).Ok()) {
      ADD_FAILURE();
      return;
    }
    EXPECT_EQ(measure.completed_samples, 65536u);
    for (int i = 0; i < 4; ++i) {
      const auto q = fixture.at("queries").at(i).get<std::array<double, 3>>();
      const auto normal = fixture.at("normals").at(i).get<std::array<double, 3>>();
      const auto equation =
          i < 2 ? ns::analytic::FieldEquation::kChromaticityX : ns::analytic::FieldEquation::kChromaticityY;
      const auto crossing = ns::analytic::CorrectSphericalField(measure.components, q,
                                                                { equation, .4, kRad, 1e-8, .1 * kRad, 32 }, nullptr);
      if (crossing.status != ns::analytic::FieldSolveStatus::kConverged) {
        ADD_FAILURE() << "seed=" << seed << " query=" << i;
        return;
      }
      const double offset = std::atan2(ns::analytic::so3::Dot3(crossing.query.direction.data(), normal.data()),
                                       ns::analytic::so3::Dot3(crossing.query.direction.data(), q.data())) /
                            kRad;
      const double expected = fixture.at("independent_reference").at(1).at("positions").at(i).at("offset_deg");
      EXPECT_NEAR(offset, expected, .05);
      EXPECT_NEAR(crossing.field.xy[i / 2].value, .4, 1e-7);
      EXPECT_GT(crossing.field.xyz[1].value, 0);
      EXPECT_GT(crossing.field.effective_samples_y, 1000);
    }
  }
}

TEST(ProductInputChain, TargetFreeDiscoveryRetainsActualsAndBudgetState) {
  auto scene = Scene(1);
  scene.light_source_.param_ = { 20.f, 0.f, 0.f };
  scene.light_source_.spectrum_ = std::vector<ns::WlParam>{ { 550.f, 1.f } };
  auto& axis = scene.ms_[0].setting_[0].crystal_.axis_;
  axis.latitude_dist = { ns::DistributionType::kNoRandom, 90.f, 0.f };
  axis.azimuth_dist = { ns::DistributionType::kNoRandom, 180.f, 0.f };
  axis.roll_dist = { ns::DistributionType::kGaussian, 57.2957795f, 1.1459156f };
  const rp::ProductDiagnosticSampler sampler(Capture(scene, 0, { 3, 5 }), 1497, rp::DiscreteSpectrumSum{});
  constexpr double kRad = 3.14159265358979323846 / 180;
  rp::ProductDiscoveryOptions options{ { 262144, 262144 }, .02 * kRad, .005 * kRad, 100000000, 1, 3, 0 };
  rp::ProductDiscoveryResult result;
  ASSERT_TRUE(rp::DiscoverProductFeatures(sampler, options, &result).Ok());
  bool peak = false;
  for (const auto& feature : result.features) {
    if (feature.kind == "intensity_peak" && feature.evidence == rp::DiagnosticEvidence::kActual) {
      peak = true;
      const auto& q = feature.sky_points[0];
      EXPECT_NEAR(std::atan2(q[1], q[0]) / kRad, 26.59704, .002);
      EXPECT_GT(feature.transverse_contrast, .1);
    }
    EXPECT_NE(feature.kind, "chromaticity_x_contour");
    EXPECT_NE(feature.kind, "chromaticity_y_contour");
  }
  EXPECT_TRUE(peak);
  EXPECT_FALSE(result.budget_exhausted);
  EXPECT_GT(result.field_component_evaluations, 0u);
  EXPECT_LE(result.field_component_evaluations, options.max_field_evaluations);
  options.max_field_evaluations = 1;
  ASSERT_TRUE(rp::DiscoverProductFeatures(sampler, options, &result).Ok());
  EXPECT_TRUE(result.budget_exhausted);
  EXPECT_TRUE(result.features.empty());
  EXPECT_FALSE(result.unfinished.empty());
  options.sampling.deadline = std::chrono::steady_clock::now();
  ASSERT_TRUE(rp::DiscoverProductFeatures(sampler, options, &result).Ok());
  EXPECT_EQ(result.measure.optical_evaluations, 0u);
  EXPECT_TRUE(result.budget_exhausted);
}

TEST(ProductInputChain, TargetFreeDeepInterfaceUsesEverySlotAndRealSource) {
  auto scene = Scene(1);
  ns::PyramidCrystalParam p;
  p.h_prs_ = { ns::DistributionType::kNoRandom, .73f, 0.f };
  p.h_pyr_u_ = { ns::DistributionType::kNoRandom, .62f, 0.f };
  p.h_pyr_l_ = { ns::DistributionType::kNoRandom, .43f, 0.f };
  p.wedge_angle_u_ = 25.f;
  p.wedge_angle_l_ = 34.f;
  const float distances[]{ 1.13f, .97f, 1.07f, 1.19f, .91f, 1.04f };
  for (int j = 0; j < 6; ++j) {
    p.d_[j] = { ns::DistributionType::kNoRandom, distances[j], 0.f };
  }
  auto& crystal = scene.ms_[0].setting_[0].crystal_;
  crystal.param_ = p;
  crystal.axis_.latitude_dist = { ns::DistributionType::kUniform, 90.f, 360.f };
  crystal.axis_.azimuth_dist = crystal.axis_.roll_dist = { ns::DistributionType::kUniform, 0.f, 360.f };
  scene.light_source_.param_ = { 20.f, 0.f, .53f };
  const rp::ProductDiagnosticSampler sampler(Capture(scene, 0, { 13, 15, 26, 28 }), 1497, rp::DiscreteSpectrumSum{});
  rp::ProductDiscoveryOptions options{ { 16384, 100000 }, .02, .005, 1, 1, 1, 16 };
  rp::ProductDiscoveryResult result;
  ASSERT_TRUE(rp::DiscoverProductFeatures(sampler, options, &result).Ok());
  bool late = false;
  for (const auto& feature : result.features) {
    if (!feature.interface_event) {
      continue;
    }
    EXPECT_EQ(feature.evidence, rp::DiagnosticEvidence::kCandidate);
    const auto& event = *feature.interface_event;
    EXPECT_GT(event.value.entry.value, 0);
    EXPECT_NEAR(event.value.interfaces[feature.internal_slot].discriminant, 0, 1e-10);
    rp::ProductInput original;
    if (!rp::ReplayDiagnosticSource(result.measure, *feature.source_token, &original).Ok()) {
      ADD_FAILURE();
      return;
    }
    EXPECT_EQ(event.source.incident, original.source.incident_direction);
    EXPECT_EQ(event.source.refractive_index,
              original.spectrum.rows[result.measure.sources[*feature.source_token].spectral_row].refractive_index);
    late |= feature.internal_slot == 2;
  }
  EXPECT_TRUE(late);
  EXPECT_TRUE(result.budget_exhausted);
  EXPECT_GT(result.event_path_evaluations, 0u);
  EXPECT_LE(result.event_path_evaluations + result.measure.optical_evaluations,
            options.sampling.max_optical_evaluations);
}

TEST(ProductInputChain, HaarOrbitConditionsOnActualCapRayAndRetainsPhysicalFamily) {
  auto scene = Scene(1, true);
  scene.light_source_.param_ = { 20.f, 13.f, 1.4f };
  auto& crystal = scene.ms_[0].setting_[0].crystal_;
  crystal.axis_.azimuth_dist = { ns::DistributionType::kUniform, 0.f, 360.f };
  crystal.axis_.latitude_dist = { ns::DistributionType::kUniform, 90.f, 360.f };
  auto& shape = std::get<ns::PrismCrystalParam>(crystal.param_);
  shape.sync_group_[ns::kShapeScalarFace0] = shape.sync_group_[ns::kShapeScalarFace3] = 1;
  const auto snapshot = Capture(scene, 0, { 3, 5 });
  auto samples = Samples(snapshot);
  size_t positive = 0;
  // Explicit deterministic draws, not a distribution estimate. Every captured
  // positive pose must retain its A/T/sky mapping throughout the conditional spin.
  for (int j = 0; j < 512; ++j) {
    samples[0].axis = rp::FullSphereAxisDraw{ (j % 8 + .5f) / 8, (j / 8 % 8 + .5f) / 8, { (j / 64 + .5f) / 8 } };
    auto input = Assemble(snapshot, samples);
    const auto axis = rp::SingleCrystalIncidentOrbit(input);
    if (!axis) {
      ADD_FAILURE() << "declared Haar input has no orbit";
      return;
    }
    EXPECT_EQ(*axis, input.source.incident_direction);
    EXPECT_NE(*axis, input.source.center_direction);
    rp::ProductChainEvaluation original;
    if (!rp::EvaluateProductChain(input, { 0 }, 1, &original).Ok()) {
      ADD_FAILURE();
      return;
    }
    if (!(original.optical_weight > 0)) {
      continue;
    }
    ++positive;
    const auto pose = input.layers[0].analytic_pose;
    for (double angle : { .3, 1.1, 3.7 }) {
      const double rotation_vector[] = { angle * (*axis)[0], angle * (*axis)[1], angle * (*axis)[2] };
      double rotation[9];
      ns::analytic::so3::Exp(rotation_vector, rotation);
      ns::analytic::so3::MatMul(rotation, pose.data(), input.layers[0].analytic_pose.data());
      rp::ProductChainEvaluation rotated;
      if (!rp::EvaluateProductChain(input, { 0 }, 1, &rotated).Ok() || rotated.layers.size() != 1) {
        ADD_FAILURE();
        return;
      }
      EXPECT_NEAR(rotated.layers[0].entry_area, original.layers[0].entry_area, 1e-13);
      EXPECT_NEAR(rotated.layers[0].interface_product, original.layers[0].interface_product, 1e-13);
      EXPECT_NEAR(rotated.optical_weight, original.optical_weight, 1e-13);
      for (int k = 0; k < 3; ++k) {
        double expected = 0;
        for (int l = 0; l < 3; ++l) {
          expected += rotation[3 * k + l] * original.layers[0].outgoing[l];
        }
        EXPECT_NEAR(rotated.layers[0].outgoing[k], expected, 1e-13);
      }
    }
  }
  EXPECT_GT(positive, 0u);
  auto input = Assemble(snapshot, samples);
  input.layers[0].scope.snapshot.crystal.axis_.latitude_dist = { ns::DistributionType::kGaussian, 90.f, .01f };
  EXPECT_FALSE(rp::SingleCrystalIncidentOrbit(input));
  input = Assemble(snapshot, samples);
  input.layers[0].scope.snapshot.crystal.axis_.roll_dist = { ns::DistributionType::kNoRandom, 0.f, 0.f };
  EXPECT_FALSE(rp::SingleCrystalIncidentOrbit(input));
  input = Assemble(snapshot, samples);
  input.layers.push_back(input.layers[0]);
  EXPECT_FALSE(rp::SingleCrystalIncidentOrbit(input));
  scene.light_source_.param_.diameter_ = 0;
  const auto point = Capture(scene, 0, { 3, 5 });
  input = Assemble(point, samples);
  EXPECT_EQ(rp::SingleCrystalIncidentOrbit(input), std::make_optional(input.source.incident_direction));
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
