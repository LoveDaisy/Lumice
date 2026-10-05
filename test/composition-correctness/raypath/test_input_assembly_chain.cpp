#include <cmath>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <set>

#include "analytic/so3.hpp"
#include "core/color_util.hpp"
#include "core/lat_lut.hpp"
#include "core/simulator.hpp"
#include "gtest/gtest.h"
#include "raypath/detail/diagnostic_sampler.hpp"
#include "raypath/detail/feature_discovery.hpp"
#include "raypath/detail/input_assembly.hpp"
#include "raypath/detail/path_feature_report.hpp"
#include "raypath/detail/path_feature_report_json.hpp"

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

rp::InputSnapshot Capture(const ns::SceneConfig& scene, uint8_t bits = 0, std::vector<int> faces = { 3, 6 }) {
  std::vector<rp::LayerSelection> selections;
  for (size_t i = 0; i < scene.ms_.size(); ++i)
    selections.push_back({ i, static_cast<ns::IdType>(i + 1), faces, bits });
  rp::InputSnapshot snapshot;
  const auto error = rp::CaptureInput(scene, "scene revision 31", selections, &snapshot);
  EXPECT_TRUE(error.Ok()) << error.message;
  return snapshot;
}

std::vector<rp::LayerSample> Samples(const rp::InputSnapshot& snapshot) {
  std::vector<rp::LayerSample> samples;
  for (const auto& layer : snapshot.layers) {
    const auto plan = std::visit([](const auto& p) { return ns::BuildShapeDrawPlan(p); }, layer.crystal.param_);
    rp::LayerSample sample;
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

rp::AssembledInput Assemble(const rp::InputSnapshot& snapshot, const std::vector<rp::LayerSample>& samples) {
  rp::AssembledInput input;
  const auto error =
      rp::AssembleInput(snapshot, samples, { { 1.f, .3f }, "cap sample" }, rp::DiscreteSpectrumSum{}, &input);
  EXPECT_TRUE(error.Ok()) << error.message;
  return input;
}

TEST(AssembledInputChain, DiagnosticMeasureKeepsWholeOuterDrawsAndReplayableSources) {
  const auto snapshot = Capture(Scene(1));
  const rp::DiagnosticSampler sampler(snapshot, 1497, rp::DiscreteSpectrumSum{});
  rp::DiagnosticMeasure out;
  rp::SamplingBudget budget{ 32, 96 };
  ASSERT_TRUE(rp::BuildDiagnosticMeasure(sampler, budget, &out).Ok());
  EXPECT_EQ(out.completed_samples, 32u);
  EXPECT_EQ(out.optical_evaluations, 96u);
  EXPECT_FALSE(out.budget_exhausted);
  EXPECT_EQ(out.sources.size(), out.components.size());
  rp::AssembledInput invalid_replay;
  EXPECT_FALSE(rp::ReplayDiagnosticSource(out, out.sources.size(), &invalid_replay).Ok());
  ASSERT_GT(out.components.size(), 0u);
  for (const auto& component : out.components) {
    const auto& source = out.sources[component.source_token];
    rp::AssembledInput input;
    rp::ChainEvaluation physical;
    if (!rp::ReplayDiagnosticSource(out, component.source_token, &input).Ok() ||
        !rp::EvaluateChain(input, { source.member_index }, source.spectral_row, &physical).Ok()) {
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
  ASSERT_TRUE(rp::BuildDiagnosticMeasure(sampler, budget, &out).Ok());
  EXPECT_TRUE(out.budget_exhausted);
  EXPECT_EQ(out.completed_samples, 1u);
  EXPECT_EQ(out.optical_evaluations, 5u);
  for (const auto& component : out.components) {
    EXPECT_EQ(component.sample_index, 0u);
  }
  budget.deadline = std::chrono::steady_clock::now();
  ASSERT_TRUE(rp::BuildDiagnosticMeasure(sampler, budget, &out).Ok());
  EXPECT_TRUE(out.budget_exhausted);
  EXPECT_EQ(out.completed_samples, 0u);
  EXPECT_EQ(out.optical_evaluations, 0u);
  const rp::DiagnosticSampler multi(Capture(Scene(2)), 1497, rp::DiscreteSpectrumSum{});
  EXPECT_EQ(rp::BuildDiagnosticMeasure(multi, {}, &out).code, rp::ErrorCode::kMultiLayerUnsupported);
}

TEST(AssembledInputChain, SourceReplayOwnsSpectrumAfterCallerAndSamplerExpire) {
  auto scene = Scene(1);
  rp::DiagnosticMeasure first;
  rp::DiagnosticMeasure second;
  {
    auto snapshot = Capture(scene);
    rp::SpectrumRequest request = rp::WavelengthSample{ 450.f, 0, "first spectrum" };
    const rp::DiagnosticSampler a(snapshot, 1497, request);
    request = rp::WavelengthSample{ 650.f, 2, "second spectrum" };
    const rp::DiagnosticSampler b(snapshot, 1497, request);
    ASSERT_TRUE(rp::BuildDiagnosticMeasure(a, { 32, 32 }, &first).Ok());
    ASSERT_TRUE(rp::BuildDiagnosticMeasure(b, { 32, 32 }, &second).Ok());
  }
  scene.ms_.clear();
  ASSERT_FALSE(first.sources.empty());
  ASSERT_FALSE(second.sources.empty());
  rp::AssembledInput a;
  rp::AssembledInput b;
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
  rp::ChainEvaluation physical;
  ASSERT_TRUE(rp::EvaluateChain(a, { first.sources[0].member_index }, 0, &physical).Ok());
  for (int j = 0; j < 3; ++j) {
    EXPECT_DOUBLE_EQ(first.components[0].xyz_weight[j], physical.xyz[j] / first.completed_samples);
  }
}

TEST(AssembledInputChain, JointSamplerReplaysPrefixesAndCorrelatedShapeFromSnapshot) {
  auto scene = Scene(1, true);
  scene.light_source_.param_ = { 20.f, 13.f, 1.4f };
  auto& crystal = scene.ms_[0].setting_[0].crystal_;
  auto& shape = std::get<ns::PrismCrystalParam>(crystal.param_);
  shape.sync_group_[ns::kShapeScalarFace0] = shape.sync_group_[ns::kShapeScalarFace3] = 1;
  crystal.axis_.azimuth_dist = { ns::DistributionType::kUniform, 0.f, 360.f };
  crystal.axis_.latitude_dist = { ns::DistributionType::kGaussian, 88.f, 2.f };
  const auto snapshot = Capture(scene, 0, { 3, 5 });
  const rp::DiagnosticSampler sampler(snapshot, 1497, rp::DiscreteSpectrumSum{});
  rp::AssembledInput a;
  rp::AssembledInput b;
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
    const rp::DiagnosticSampler branch(changed, 1497, rp::DiscreteSpectrumSum{});
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

TEST(AssembledInputChain, GaussianDensityPeakMatchesIndependentPhysicalQuadrature) {
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
    const rp::DiagnosticSampler sampler(snapshot, seed, rp::DiscreteSpectrumSum{});
    rp::DiagnosticMeasure measure;
    if (!rp::BuildDiagnosticMeasure(sampler, { 262144, 262144 }, &measure).Ok()) {
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
      // reference self-difference is <0.000016 degrees. Not a global accuracy bar.
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

TEST(AssembledInputChain, CorrelatedShapeFiniteSunColourCrossingsMatchIndependentPhysics) {
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
    const rp::DiagnosticSampler sampler(snapshot, seed, rp::DiscreteSpectrumSum{});
    rp::DiagnosticMeasure measure;
    if (!rp::BuildDiagnosticMeasure(sampler, { 65536, 196608 }, &measure).Ok()) {
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

TEST(AssembledInputChain, TargetFreeDiscoveryRetainsActualsAndBudgetState) {
  auto scene = Scene(1);
  scene.light_source_.param_ = { 20.f, 0.f, 0.f };
  scene.light_source_.spectrum_ = std::vector<ns::WlParam>{ { 550.f, 1.f } };
  auto& axis = scene.ms_[0].setting_[0].crystal_.axis_;
  axis.latitude_dist = { ns::DistributionType::kNoRandom, 90.f, 0.f };
  axis.azimuth_dist = { ns::DistributionType::kNoRandom, 180.f, 0.f };
  axis.roll_dist = { ns::DistributionType::kGaussian, 57.2957795f, 1.1459156f };
  const rp::DiagnosticSampler sampler(Capture(scene, 0, { 3, 5 }), 1497, rp::DiscreteSpectrumSum{});
  constexpr double kRad = 3.14159265358979323846 / 180;
  rp::DiscoveryOptions options{ { 262144, 524288 }, .02 * kRad, .005 * kRad, 100000000, 1, 3, 0 };
  rp::DiscoveryResult result;
  ASSERT_TRUE(rp::DiscoverFeatures(sampler, options, &result).Ok());
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
  ASSERT_TRUE(rp::DiscoverFeatures(sampler, options, &result).Ok());
  EXPECT_TRUE(result.budget_exhausted);
  EXPECT_TRUE(result.features.empty());
  EXPECT_FALSE(result.unfinished.empty());
  options.sampling.deadline = std::chrono::steady_clock::now();
  ASSERT_TRUE(rp::DiscoverFeatures(sampler, options, &result).Ok());
  EXPECT_EQ(result.measure.optical_evaluations, 0u);
  EXPECT_TRUE(result.budget_exhausted);
}

TEST(AssembledInputChain, FixedObservationDoesNotConfuseScaleResponseWithLocationError) {
  constexpr double kRad = 3.14159265358979323846 / 180;
  auto scene = Scene(1);
  scene.light_source_.param_ = { 20.f, 0.f, 0.f };
  scene.light_source_.spectrum_ = std::vector<ns::WlParam>{ { 450.f, .2f }, { 550.f, .3f }, { 650.f, .5f } };
  auto& axis = scene.ms_[0].setting_[0].crystal_.axis_;
  axis.latitude_dist = { ns::DistributionType::kUniform, 90.f, 360.f };
  axis.azimuth_dist = axis.roll_dist = { ns::DistributionType::kUniform, 0.f, 360.f };
  const rp::DiagnosticSampler sampler(Capture(scene, 0, { 3, 5 }), 9713, rp::DiscreteSpectrumSum{});
  rp::DiscoveryOptions options{ { 65536, 524288 }, kRad, .05 * kRad, 250000000, 8, 8, 0 };
  rp::DiscoveryResult result;
  ASSERT_TRUE(rp::DiscoverFeatures(sampler, options, &result).Ok());
  bool found = false;
  const double sun[]{ std::cos(20 * kRad), 0, std::sin(20 * kRad) };
  for (const auto& feature : result.features) {
    if (feature.kind != "intensity_ridge" || feature.evidence != rp::DiagnosticEvidence::kActual) {
      continue;
    }
    found = true;
    EXPECT_LT(feature.prefix_movement_rad, .05 * kRad);
    EXPECT_TRUE(feature.scale_movement_rad.has_value());
    if (feature.scale_movement_rad) {
      // Independent LI quadrature: 128x256 vs 256x512 differs by 0.000054 deg.
      // Changing h genuinely moves the peak by 0.245599 deg, not sampling error.
      EXPECT_NEAR(*feature.scale_movement_rad / kRad, .2455992291, .02);
    }
    for (const auto& q : feature.sky_points) {
      EXPECT_NEAR(std::acos(ns::analytic::so3::Dot3(q.data(), sun)) / kRad, 23.0996842241, .05);
    }
  }
  EXPECT_TRUE(found);
}

TEST(AssembledInputChain, OrdinaryEdgeHasPhysicalRadiusAndSeparatePositiveContrast) {
  constexpr double kRad = 3.14159265358979323846 / 180;
  auto scene = Scene(1);
  scene.light_source_.param_ = { 20.f, 0.f, 0.f };
  scene.light_source_.spectrum_ = std::vector<ns::WlParam>{ { 450.f, .2f }, { 550.f, .3f }, { 650.f, .5f } };
  auto& axis = scene.ms_[0].setting_[0].crystal_.axis_;
  axis.latitude_dist = { ns::DistributionType::kUniform, 90.f, 360.f };
  axis.azimuth_dist = axis.roll_dist = { ns::DistributionType::kUniform, 0.f, 360.f };
  const auto snapshot = Capture(scene, 0, { 3, 5 });
  const rp::DiagnosticSampler sampler(snapshot, 1497, rp::DiscreteSpectrumSum{});
  rp::DiscoveryOptions options{ { 65536, 524288 }, kRad, .05 * kRad, 250000000, 1, 8, 0, 3 };
  rp::DiscoveryResult result;
  ASSERT_TRUE(rp::DiscoverFeatures(sampler, options, &result).Ok());
  int edges = 0;
  for (const auto& feature : result.features) {
    if (!feature.deviation_minimum) {
      continue;
    }
    ++edges;
    const auto& minimum = *feature.deviation_minimum;
    EXPECT_EQ(feature.evidence, rp::DiagnosticEvidence::kActual);
    const double n = minimum.source.refractive_index;
    EXPECT_NEAR(minimum.deviation_rad, 2 * std::asin(n * .5) - 60 * kRad, 1e-10);
    EXPECT_GT(feature.transverse_contrast, .7);
    EXPECT_LT(feature.observation_contrast_error, .05 * feature.transverse_contrast);
    EXPECT_GT(minimum.value.entry.value, 0);
    EXPECT_EQ(feature.geometry, rp::DiagnosticGeometry::kPolyline);
    EXPECT_TRUE(feature.orbit_axis.has_value());
    EXPECT_GT(feature.orbit_end_rad, feature.orbit_begin_rad);
    EXPECT_GT(std::abs(minimum.deviation_rad / kRad - 23.0996842241), .8);
  }
  EXPECT_EQ(edges, 3);
  // Same path/shape under restricted support must not be promoted using the
  // Haar chart. A sharply peaked distribution is not an orientation orbit.
  axis.latitude_dist = { ns::DistributionType::kGaussian, 90.f, 1.f };
  const rp::DiagnosticSampler restricted(Capture(scene, 0, { 3, 5 }), 1497, rp::DiscreteSpectrumSum{});
  options.sampling.requested_samples = 1024;
  options.max_field_evaluations = 1;
  ASSERT_TRUE(rp::DiscoverFeatures(restricted, options, &result).Ok());
  for (const auto& feature : result.features) {
    EXPECT_FALSE(feature.deviation_minimum.has_value());
  }
}

TEST(AssembledInputChain, AutomaticColourContoursHaveIndependentFixedObservationPositions) {
  std::ifstream in(std::string(LUMICE_DIAGNOSTIC_FIXTURE_DIR) + "/automatic-colour.json");
  ASSERT_TRUE(in.good());
  const auto fixture = nlohmann::json::parse(in);
  constexpr double kRad = 3.14159265358979323846 / 180;
  auto scene = Scene(1);
  scene.light_source_.param_ = { 20.f, 0.f, .53f };
  scene.light_source_.spectrum_ = std::vector<ns::WlParam>{ { 450.f, .2f }, { 550.f, .3f }, { 650.f, .5f } };
  auto& crystal = scene.ms_[0].setting_[0].crystal_;
  auto& shape = std::get<ns::PrismCrystalParam>(crystal.param_);
  shape.h_ = shape.d_[0] = { ns::DistributionType::kUniform, 1.f, 1.4f };
  shape.sync_group_[ns::kShapeScalarHeight] = shape.sync_group_[ns::kShapeScalarFace0] = 1;
  crystal.axis_.latitude_dist = { ns::DistributionType::kGaussian, 90.f, 1.f };
  crystal.axis_.azimuth_dist = crystal.axis_.roll_dist = { ns::DistributionType::kUniform, 0.f, 360.f };
  const rp::DiagnosticSampler sampler(Capture(scene, 0, { 3, 5 }), 1497, rp::DiscreteSpectrumSum{});
  rp::DiscoveryOptions options{ { 65536, 524288 }, kRad, .05 * kRad, 250000000, 8, 8, 0 };
  rp::DiscoveryResult result;
  ASSERT_TRUE(rp::DiscoverFeatures(sampler, options, &result).Ok());
  std::ifstream bands_in(std::string(LUMICE_DIAGNOSTIC_FIXTURE_DIR) + "/automatic-colour-band.json");
  ASSERT_TRUE(bands_in.good());
  const auto bands_fixture = nlohmann::json::parse(bands_in);
  for (const std::string channel : { "x", "y" }) {
    const auto feature = std::find_if(result.features.begin(), result.features.end(), [&](const auto& f) {
      return f.kind == "chromaticity_" + channel + "_contour" && f.evidence == rp::DiagnosticEvidence::kActual;
    });
    if (feature == result.features.end()) {
      ADD_FAILURE() << channel;
      return;
    }
    EXPECT_EQ(feature->geometry, rp::DiagnosticGeometry::kPolyline);
    EXPECT_GT(feature->transverse_contrast, .001);
    EXPECT_EQ(feature->bandwidth_rad, kRad);
    const auto band = std::find_if(result.features.begin(), result.features.end(), [&](const auto& f) {
      return f.kind == "chromaticity_" + channel + "_band" && f.evidence == rp::DiagnosticEvidence::kActual;
    });
    if (band == result.features.end() || !band->band) {
      ADD_FAILURE() << channel;
      return;
    }
    EXPECT_FALSE(band->scale_movement_rad.has_value());
    EXPECT_EQ(band->geometry, rp::DiagnosticGeometry::kBand);
    EXPECT_LT(band->band->levels[0], band->band->levels[1]);
    for (const auto& ref : bands_fixture.at("results")) {
      if (ref.at("power") != 18 || ref.at("channel") != channel) {
        continue;
      }
      const size_t side = ref.at("side");
      const size_t vertex = ref.at("vertex");
      if (vertex >= band->band->boundaries[side].size()) {
        ADD_FAILURE();
        return;
      }
      const auto& point = band->band->boundaries[side][vertex];
      const auto q = ref.at("query").get<std::array<double, 3>>();
      const auto normal = ref.at("normal").get<std::array<double, 3>>();
      const double offset = std::atan2(ns::analytic::so3::Dot3(point.query.direction.data(), normal.data()),
                                       ns::analytic::so3::Dot3(point.query.direction.data(), q.data())) /
                            kRad;
      // The independent reference refines <=0.012 deg here; native-to-reference
      // <=0.029 deg. Both fit the unchanged local 0.05-deg resolution together.
      EXPECT_NEAR(offset, ref.at("reference_offset_deg").get<double>(), .038);
      EXPECT_NEAR(point.field.xy[channel == "x" ? 0 : 1].value, band->band->levels[side], 1e-7);
      EXPECT_EQ(point.query.bandwidth_rad, kRad);
    }
    // Actual reference curves are at the automatically chosen, recorded level,
    // not the preselected 0.4 crossings used by the local corrector fixture.
    for (const auto& ref : fixture.at("results")) {
      if (ref.at("power") != 16 || ref.at("seed") != 1497 || ref.at("channel") != channel) {
        continue;
      }
      const size_t vertex = ref.at("vertex");
      if (vertex >= feature->sky_points.size()) {
        ADD_FAILURE();
        return;
      }
      // The recorded level is compared across ISAs: arm64 FMA chains and x86-64
      // separately-rounded arithmetic drift ~16 float ulp here (measured 4.7e-7,
      // identical on Ubuntu x86_64 and Windows MSVC against this fixture). The
      // assertion guards the automatic level choice, which moves by far more
      // than 1e-6 when the selection logic actually changes.
      EXPECT_NEAR(feature->level, ref.at("level").get<double>(), 1e-6);
      const auto q = ref.at("query").get<std::array<double, 3>>();
      const auto normal = ref.at("normal").get<std::array<double, 3>>();
      const double offset = ref.at("reference_offset_deg").get<double>() * kRad;
      std::array<double, 3> expected;
      for (int j = 0; j < 3; ++j) {
        expected[j] = std::cos(offset) * q[j] + std::sin(offset) * normal[j];
      }
      double cross[3];
      ns::analytic::so3::Cross3(expected.data(), feature->sky_points[vertex].data(), cross);
      // Independent 2^14x24 to 2^16x48 refinement moves these roots <=0.031 deg.
      // Native error <=0.009 deg; the local 0.05-deg budget includes both.
      EXPECT_LT(std::atan2(ns::analytic::so3::Norm3(cross),
                           ns::analytic::so3::Dot3(expected.data(), feature->sky_points[vertex].data())),
                .05 * kRad);
    }
  }
}

TEST(AssembledInputChain, DeclaredSourceCornersMatchIndependentGeometryWithoutSkyJoining) {
  std::ifstream in(std::string(LUMICE_DIAGNOSTIC_FIXTURE_DIR) + "/source-box.json");
  ASSERT_TRUE(in.good());
  const auto fixture = nlohmann::json::parse(in);
  auto scene = Scene(1);
  scene.light_source_.param_ = { 20, 0, 0 };
  scene.light_source_.spectrum_ = std::vector<ns::WlParam>{ { 550, 1 } };
  ns::PyramidCrystalParam p;
  p.h_prs_ = { ns::DistributionType::kNoRandom, 0, 0 };
  p.h_pyr_u_ = { ns::DistributionType::kNoRandom, .62f, 0 };
  p.h_pyr_l_ = { ns::DistributionType::kNoRandom, .43f, 0 };
  p.wedge_angle_u_ = 25;
  p.wedge_angle_l_ = 34;
  for (int j = 0; j < 6; ++j) {
    p.d_[j] = { ns::DistributionType::kNoRandom, fixture["shape"]["face_distance"][j].get<float>(), 0 };
  }
  auto& crystal = scene.ms_[0].setting_[0].crystal_;
  crystal.param_ = p;
  crystal.axis_.latitude_dist = { ns::DistributionType::kUniform, fixture["latitude_mean"].get<float>(), .5f };
  crystal.axis_.roll_dist = { ns::DistributionType::kUniform, fixture["roll_mean"].get<float>(), .5f };
  crystal.axis_.azimuth_dist = { ns::DistributionType::kNoRandom, fixture["azimuth_mean"].get<float>(), 0 };
  const auto snapshot = Capture(scene, 0, { 13, 15 });
  const auto support = rp::DescribeSupport(snapshot);
  EXPECT_EQ(support.pose_support_dimension, 2);
  rp::DiagnosticSampler sampler(snapshot, 1497, rp::DiscreteSpectrumSum{});
  rp::DiscoveryResult result;
  ASSERT_TRUE(rp::DiscoverFeatures(sampler, { { 64, 1000 }, .02, .001, 1, 1, 1, 0, 0 }, &result).Ok());
  std::set<std::pair<double, double>> corners;
  for (const auto& feature : result.features) {
    if (feature.kind != "declared_source_corner") {
      continue;
    }
    EXPECT_EQ(feature.evidence, rp::DiagnosticEvidence::kCandidate);
    if (feature.source_parameters.size() != 2 || !feature.boundary_value) {
      ADD_FAILURE();
      return;
    }
    corners.insert({ feature.source_parameters[0].second, feature.source_parameters[1].second });
    EXPECT_GT(feature.boundary_value->entry.value, 0);
    const auto& sky = feature.sky_points[0];
    double nearest = 10;
    for (const auto& ref : fixture["expected_sky_corners"]) {
      const auto q = ref.get<std::array<double, 3>>();
      double cross[3];
      ns::analytic::so3::Cross3(sky.data(), q.data(), cross);
      nearest =
          std::min(nearest, std::atan2(ns::analytic::so3::Norm3(cross), ns::analytic::so3::Dot3(sky.data(), q.data())));
    }
    // The reference uses the ACTUAL product CDF endpoint poses, not ideal
    // uniform-angle ends. The LUT brackets a histogram and has different ends.
    EXPECT_LT(nearest, 1e-6);
    const size_t corner =
        static_cast<size_t>(2 * feature.source_parameters[0].second + feature.source_parameters[1].second);
    const auto expected_pose = fixture["actual_product_endpoint_poses"][corner].get<std::array<double, 9>>();
    for (int j = 0; j < 9; ++j) {
      EXPECT_NEAR(feature.boundary_source->pose[j], expected_pose[j], 1e-6);
    }
  }
  EXPECT_EQ(corners.size(), 4u);
  auto& axis = scene.ms_[0].setting_[0].crystal_.axis_;
  axis.latitude_dist = { ns::DistributionType::kNoRandom, 90, 0 };
  axis.azimuth_dist = axis.roll_dist = { ns::DistributionType::kUniform, 0, 360 };
  const auto pole = rp::DescribeSupport(Capture(scene, 0, { 13, 15 }));
  EXPECT_EQ(pole.pose_coordinate_count, 2);
  EXPECT_EQ(pole.pose_support_dimension, 1);
  axis.latitude_dist = { ns::DistributionType::kGaussian, 90, .001f };
  EXPECT_EQ(rp::DescribeSupport(Capture(scene, 0, { 13, 15 })).pose_support_dimension, 3);
}

TEST(AssembledInputChain, FeatureReportRetainsLayerScopeAndSourceNumerics) {
  auto scene = Scene(2);
  scene.light_source_.param_ = { 20.f, 0.f, 0.f };
  scene.light_source_.spectrum_ = std::vector<ns::WlParam>{ { 550.f, 1.f } };
  auto& axis = scene.ms_[1].setting_[0].crystal_.axis_;
  axis.latitude_dist = { ns::DistributionType::kUniform, 90.f, 360.f };
  axis.azimuth_dist = axis.roll_dist = { ns::DistributionType::kUniform, 0.f, 360.f };
  const rp::DiscoveryOptions options{ { 16384, 60000 }, .02, .001, 20000000, 1, 4, 0, 1 };
  rp::PathFeatureReport report;
  ASSERT_TRUE(rp::BuildPathFeatureReport(scene, "two-layer scene, second-layer single path", { { 1, 2, { 3, 5 }, 0 } },
                                         rp::DiscreteSpectrumSum{}, 1497, options, &report)
                  .Ok());
  const auto json = nlohmann::json::parse(rp::PathFeatureReportToJson(report, "test"));
  EXPECT_EQ(json.at("scope").at("layers").at(0).at("scene_layer"), 1);
  EXPECT_EQ(json.at("spectrum").at(0).at("nm"), 550);
  EXPECT_TRUE(json.contains("budgets"));
  EXPECT_TRUE(json.contains("unfinished"));
  bool edge = false;
  for (const auto& feature : json.at("actual_features")) {
    if (!feature.contains("physical_position")) {
      continue;
    }
    edge = true;
    EXPECT_EQ(feature.at("evidence"), "actual");
    const auto& physical = feature.at("physical_position");
    EXPECT_TRUE(physical.at("value").at("entry").at("geometry_evaluated"));
    EXPECT_EQ(physical.at("source").at("pose").size(), 9u);
    EXPECT_TRUE(json.at("sources").contains(std::to_string(feature.at("source_token").get<uint64_t>())));
  }
  EXPECT_TRUE(edge);
  const auto error =
      rp::BuildPathFeatureReport(scene, "two crystal chain", { { 0, 1, { 3, 5 }, 0 }, { 1, 2, { 3, 5 }, 0 } },
                                 rp::DiscreteSpectrumSum{}, 1497, options, &report);
  EXPECT_EQ(error.code, rp::ErrorCode::kMultiLayerUnsupported);
}

TEST(AssembledInputChain, FeatureReportRefinesTheSameContinuousSpectrumObservation) {
  auto scene = Scene(1);
  scene.light_source_.param_ = { 20.f, 0.f, .53f };
  scene.light_source_.spectrum_ = ns::IlluminantType::kD65;
  auto& crystal = scene.ms_[0].setting_[0].crystal_;
  auto& shape = std::get<ns::PrismCrystalParam>(crystal.param_);
  shape.h_ = shape.d_[0] = { ns::DistributionType::kUniform, 1.f, 1.4f };
  shape.sync_group_[ns::kShapeScalarHeight] = shape.sync_group_[ns::kShapeScalarFace0] = 1;
  crystal.axis_.latitude_dist = { ns::DistributionType::kGaussian, 90.f, 1.f };
  crystal.axis_.azimuth_dist = crystal.axis_.roll_dist = { ns::DistributionType::kUniform, 0.f, 360.f };
  rp::SpectrumQuadrature quadrature;
  quadrature.rule = "dyadic full-band trapezoid";
  quadrature.evaluation_budget = 33;
  for (int j = 0; j <= 32; ++j) {
    quadrature.nodes.push_back(
        { std::min(380.f + 400.f * j / 32, std::nextafter(780.f, 380.f)), (j == 0 || j == 32 ? .5 : 1.) / 32 });
  }
  constexpr double kRad = 3.14159265358979323846 / 180;
  const rp::DiscoveryOptions options{ { 8192, 1200000 }, kRad, .05 * kRad, 250000000, 2, 4, 0, 0 };
  rp::PathFeatureReport report;
  ASSERT_TRUE(rp::BuildPathFeatureReport(scene, "actual D65 scene", { { 0, 1, { 3, 5 }, 0 } }, quadrature, 1497,
                                         options, &report)
                  .Ok());
  EXPECT_EQ(report.spectral_optical_evaluations, 8192u * 65);
  EXPECT_GT(report.spectral_field_evaluations, 0u);
  EXPECT_TRUE(report.spectral_movement_rad.has_value());
  EXPECT_GT(report.spectral_seconds, 0);
  EXPECT_TRUE(std::any_of(report.discovery.features.begin(), report.discovery.features.end(),
                          [](const auto& f) { return f.evidence == rp::DiagnosticEvidence::kActual; }));
  const auto json = nlohmann::json::parse(rp::PathFeatureReportToJson(report, "test"));
  // Two cap/shape corners, one cap edge and two shape ends also trace optics.
  EXPECT_EQ(report.discovery.event_path_evaluations, 5u);
  EXPECT_EQ(json.at("budgets").at("optical_evaluations"), 8192u * (33 + 33 + 65) + 5);
  EXPECT_EQ(json.at("spectrum").size(), 33u);
}

TEST(AssembledInputChain, AtomRequiresDeclaredZeroDimensionalSourceNotSampledRank) {
  auto scene = Scene(1);
  scene.light_source_.param_ = { 20.f, 0.f, 0.f };
  auto& axis = scene.ms_[0].setting_[0].crystal_.axis_;
  axis.latitude_dist = { ns::DistributionType::kNoRandom, 90.f, 0.f };
  axis.azimuth_dist = { ns::DistributionType::kNoRandom, 180.f, 0.f };
  axis.roll_dist = { ns::DistributionType::kNoRandom, 57.29578f, 0.f };
  rp::DiscoveryOptions options{ { 32, 96 }, .02, .005, 0, 1, 1, 0 };
  rp::DiscoveryResult result;
  const rp::DiagnosticSampler fixed(Capture(scene, 0, { 3, 5 }), 1497, rp::DiscreteSpectrumSum{});
  ASSERT_TRUE(rp::DiscoverFeatures(fixed, options, &result).Ok());
  ASSERT_EQ(result.features.size(), 3u);
  EXPECT_EQ(result.measure.completed_samples, 1u);
  for (const auto& feature : result.features) {
    EXPECT_EQ(feature.evidence, rp::DiagnosticEvidence::kActual);
    EXPECT_EQ(feature.geometry, rp::DiagnosticGeometry::kAtom);
    EXPECT_GT(feature.atom_xyz_mass[1], 0);
  }
  const auto original_spectrum = scene.light_source_.spectrum_;
  scene.light_source_.spectrum_ = std::vector<ns::WlParam>{ { 550.f, 0.f } };
  const rp::DiagnosticSampler dark(Capture(scene, 0, { 3, 5 }), 1497, rp::DiscreteSpectrumSum{});
  ASSERT_TRUE(rp::DiscoverFeatures(dark, options, &result).Ok());
  EXPECT_TRUE(result.features.empty());
  scene.light_source_.spectrum_ = original_spectrum;
  scene.light_source_.param_.diameter_ = .53f;
  const rp::DiagnosticSampler cap(Capture(scene, 0, { 3, 5 }), 1497, rp::DiscreteSpectrumSum{});
  ASSERT_TRUE(rp::DiscoverFeatures(cap, options, &result).Ok());
  EXPECT_EQ(result.measure.completed_samples, 32u);
  for (const auto& feature : result.features) {
    EXPECT_NE(feature.geometry, rp::DiagnosticGeometry::kAtom);
  }
  EXPECT_TRUE(result.budget_exhausted);
  scene.light_source_.param_.diameter_ = 0;
  axis.roll_dist = { ns::DistributionType::kGaussian, 57.29578f, 1e-6f };
  const rp::DiagnosticSampler thin(Capture(scene, 0, { 3, 5 }), 1497, rp::DiscreteSpectrumSum{});
  ASSERT_TRUE(rp::DiscoverFeatures(thin, options, &result).Ok());
  EXPECT_EQ(result.measure.completed_samples, 32u);
  EXPECT_TRUE(result.features.empty());
}

TEST(AssembledInputChain, TargetFreeDeepInterfaceUsesEverySlotAndRealSource) {
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
  const rp::DiagnosticSampler sampler(Capture(scene, 0, { 13, 15, 26, 28 }), 1497, rp::DiscreteSpectrumSum{});
  rp::DiscoveryOptions options{ { 16384, 200000 }, .02, .005, 1, 1, 1, 16 };
  rp::DiscoveryResult result;
  ASSERT_TRUE(rp::DiscoverFeatures(sampler, options, &result).Ok());
  bool late = false;
  for (const auto& feature : result.features) {
    if (!feature.interface_event) {
      continue;
    }
    EXPECT_EQ(feature.evidence, rp::DiagnosticEvidence::kCandidate);
    const auto& event = *feature.interface_event;
    EXPECT_GT(event.value.entry.value, 0);
    EXPECT_NEAR(event.value.interfaces[feature.internal_slot].discriminant, 0, 1e-10);
    rp::AssembledInput original;
    if (!rp::ReplayDiagnosticSource(result.measure, *feature.source_token, &original).Ok()) {
      ADD_FAILURE();
      return;
    }
    EXPECT_EQ(event.source.incident, original.source.incident_direction);
    EXPECT_EQ(event.source.refractive_index,
              original.spectrum.rows[result.measure.sources[*feature.source_token].spectral_row].refractive_index);
    if (feature.paired_interface && feature.paired_interface->complete) {
      const auto& paired = *feature.paired_interface;
      for (int channel = 0; channel < 3; ++channel) {
        EXPECT_GE(paired.without_slot_xyz[channel] * (1 + 64 * std::numeric_limits<double>::epsilon()),
                  paired.actual_xyz[channel]);
      }
      EXPECT_TRUE(paired.chromaticity_available);
    }
    late |= feature.internal_slot == 2;
  }
  EXPECT_TRUE(late);
  EXPECT_TRUE(result.budget_exhausted);
  EXPECT_GT(result.event_path_evaluations, 0u);
  EXPECT_LE(result.event_path_evaluations + result.replicate_path_evaluations + result.measure.optical_evaluations,
            options.sampling.max_optical_evaluations);
}

TEST(AssembledInputChain, HaarOrbitConditionsOnActualCapRayAndRetainsPhysicalFamily) {
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
    rp::ChainEvaluation original;
    if (!rp::EvaluateChain(input, { 0 }, 1, &original).Ok()) {
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
      rp::ChainEvaluation rotated;
      if (!rp::EvaluateChain(input, { 0 }, 1, &rotated).Ok() || rotated.layers.size() != 1) {
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

TEST(AssembledInputChain, TwoLayersUseActualDirectionsOneWavelengthAndOneSpectralCharge) {
  const auto snapshot = Capture(Scene(2));
  const auto input = Assemble(snapshot, Samples(snapshot));
  ASSERT_EQ(input.layers.size(), 2u);
  ASSERT_EQ(input.spectrum.rows.size(), 3u);
  // Unit-height regular prism with circumradius 1/2: each side area is 1/2,
  // and S = 6*(1/2) + 2*(3*sqrt(3)/8). Normal 3->6 transmits through a slab.
  const double surface = 3 + 3 * std::sqrt(3.) / 4;
  for (size_t wi = 0; wi < input.spectrum.rows.size(); ++wi) {
    SCOPED_TRACE(wi);
    rp::ChainEvaluation result;
    const auto error = rp::EvaluateChain(input, { 0, 0 }, wi, &result);
    EXPECT_TRUE(error.Ok()) << error.message;
    if (!error.Ok() || result.layers.size() != 2u) {
      ADD_FAILURE() << "chain was not evaluated";
      continue;
    }
    const double n = input.spectrum.rows[wi].refractive_index;
    const double reflect = std::pow((n - 1) / (n + 1), 2);
    const double transmission = std::pow(1 - reflect, 2);
    for (const auto& layer : result.layers) {
      EXPECT_EQ(layer.status, rp::ContributionStatus::kPositive);
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

TEST(AssembledInputChain, EnsembleSetsStayFixedWhileActualDrawChangesGeometryAndContributions) {
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
  // The analytic geometry must carry the realized leaders too; changing only
  // the production surface area can otherwise hide a nominal-shape substitution.
  EXPECT_DOUBLE_EQ(irregular.layers[0].shape.face_distance[0], static_cast<double>(.8f));
  EXPECT_DOUBLE_EQ(irregular.layers[1].shape.face_distance[0], static_cast<double>(1.1f));
  EXPECT_EQ(irregular.layers[1].shape.face_distance[0], irregular.layers[1].shape.face_distance[3]);
  std::vector<size_t> cursor(2, 0);
  double sum = 0;
  double representative = 0;
  size_t visited = 0;
  do {
    rp::ChainEvaluation eval;
    EXPECT_TRUE(rp::EvaluateChain(irregular, cursor, 0, &eval).Ok());
    if (visited == 0)
      representative = eval.optical_weight;
    sum += eval.optical_weight;
    visited++;
  } while (rp::NextMemberChain(irregular, &cursor));
  const auto count = irregular.layers[0].scope.members.size() * irregular.layers[1].scope.members.size();
  EXPECT_EQ(visited, count);
  EXPECT_GT(sum, 0);
  EXPECT_GT(std::abs(sum - count * representative), 1e-4);
  // Factorized storage grows by a sum; no spectral/member Cartesian rows exist.
  EXPECT_EQ(irregular.spectrum.rows.size(), 3u);
  EXPECT_EQ(irregular.layers[0].scope.members.size(), 6u);
  EXPECT_EQ(irregular.layers[1].scope.members.size(), 2u);
}

TEST(AssembledInputChain, SnapshotSurvivesSceneMutationAndSourceDestruction) {
  rp::InputSnapshot snapshot;
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
  rp::AssembledInput output;
  EXPECT_FALSE(rp::AssembleInput(bad, Samples(bad), { { .4f, .5f }, "cap" }, rp::DiscreteSpectrumSum{}, &output).Ok());
  EXPECT_TRUE(output.layers.empty());
}

TEST(AssembledInputChain, SamplesMustBelongToTheirSnapshotLayerAndCrystal) {
  const auto snapshot = Capture(Scene(2));
  auto samples = Samples(snapshot);
  rp::AssembledInput output;
  std::swap(samples[0], samples[1]);
  EXPECT_FALSE(rp::AssembleInput(snapshot, samples, { { .4f, .5f }, "cap" }, rp::DiscreteSpectrumSum{}, &output).Ok());
  EXPECT_TRUE(output.layers.empty());
  samples = Samples(snapshot);
  samples[0].identity.scene_identity = "another revision";
  EXPECT_FALSE(rp::AssembleInput(snapshot, samples, { { .4f, .5f }, "cap" }, rp::DiscreteSpectrumSum{}, &output).Ok());
  EXPECT_TRUE(output.layers.empty());
  samples = Samples(snapshot);
  samples[0].identity.crystal_id = samples[1].identity.crystal_id;
  EXPECT_FALSE(rp::AssembleInput(snapshot, samples, { { .4f, .5f }, "cap" }, rp::DiscreteSpectrumSum{}, &output).Ok());
  EXPECT_TRUE(output.layers.empty());
}

TEST(AssembledInputChain, ZeroPrismPyramidIsNotRejectedAndMissingFaceIsNotEmptyCrystal) {
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
  rp::ChainEvaluation missing;
  ASSERT_TRUE(rp::EvaluateChain(input, { 0 }, 0, &missing).Ok());
  ASSERT_EQ(missing.layers.size(), 1u);
  EXPECT_EQ(missing.layers[0].status, rp::ContributionStatus::kMissingFace);
  EXPECT_EQ(missing.optical_weight, 0);
  // Opposite signed planes cannot bound a volume. The assembly path rejects
  // it; assembly preserves that status rather than replacing the actual draw.
  auto samples = Samples(snapshot);
  for (size_t i = 0; i < samples[0].shape.size; ++i) {
    if (samples[0].shape.values[i].slot >= ns::kShapeScalarFace0)
      samples[0].shape.values[i].value = -1.f;
  }
  const auto empty = Assemble(snapshot, samples);
  EXPECT_EQ(empty.layers[0].geometry_status, ns::analytic::Status::kInvalidConfig);
  rp::ChainEvaluation rejected;
  ASSERT_TRUE(rp::EvaluateChain(empty, { 0 }, 0, &rejected).Ok());
  ASSERT_EQ(rejected.layers.size(), 1u);
  EXPECT_EQ(rejected.layers[0].status, rp::ContributionStatus::kRejectedShape);
}
TEST(AssembledInputChain, BentFirstLayerFeedsItsExitToTheNextLayer) {
  auto scene = Scene(2);
  scene.ms_[0].setting_[0].crystal_.axis_.azimuth_dist = { ns::DistributionType::kNoRandom, 221.f, 0.f };
  auto snapshot = Capture(scene);
  snapshot.layers[0].representative = { 3, 5 };
  const auto input = Assemble(snapshot, Samples(snapshot));
  rp::ChainEvaluation bent;
  ASSERT_TRUE(rp::EvaluateChain(input, { 0, 0 }, 0, &bent).Ok());
  ASSERT_EQ(bent.layers.size(), 2u);
  EXPECT_GT(bent.optical_weight, 0);
  EXPECT_EQ(bent.layers[1].incident, bent.layers[0].outgoing);
  EXPECT_GT(std::abs(bent.layers[1].incident[1] - input.source.incident_direction[1]), .2);
  // If the second layer incorrectly restarts at the sun, it becomes the
  // normal-incidence slab: its entry area and Fresnel product both change.
  const auto straight_snapshot = Capture(Scene(1));
  const auto straight_input = Assemble(straight_snapshot, Samples(straight_snapshot));
  rp::ChainEvaluation straight;
  ASSERT_TRUE(rp::EvaluateChain(straight_input, { 0 }, 0, &straight).Ok());
  ASSERT_EQ(straight.layers.size(), 1u);
  EXPECT_GT(std::abs(bent.layers[1].entry_area - straight.layers[0].entry_area), .01);
  EXPECT_GT(std::abs(bent.layers[1].interface_product - straight.layers[0].interface_product), 1e-4);
}

TEST(AssembledInputChain, AllAxisBranchesReachTheSampleMatrixWithoutDrawingRng) {
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
      EXPECT_EQ(input.layers[0].sample_pose[j], expected.GetMat()[j]);
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
