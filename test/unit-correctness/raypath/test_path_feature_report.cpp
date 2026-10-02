// Target-free path feature reports: the two fixed, independently computed Lumice Integral
// diagnostic cases, physical-L2 member accounting, explicit coverage, and the separate JSON
// contract. Reference revision: 366b7946e07231f6a714216279f57e07b0b71957; source artifact hashes:
// reference.json 02c11d624d41ac536ed5cf85d2327c563839192013f657847152f4e35972e47f,
// arrays.npz 3439e5bfdf2ee2a436490f7038589915a966693628ee7e5ac89d8730227f4e7bf.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "config/config_manager.hpp"
#include "raypath/path_feature_report.hpp"
#include "raypath/path_feature_report_json.hpp"

namespace lumice::raypath {
namespace {

constexpr Distribution kFullTurn{ DistributionType::kUniform, 0.0f, 360.0f };

ConfigManager Scene(bool random, bool horizontal, bool rhombic) {
  PrismCrystalParam prism;
  prism.h_ = { DistributionType::kNoRandom, 1.0f, 0.0f };
  const float distances[6] = { 1.5f, 1.0f, 1.0f, 1.5f, 1.0f, 1.0f };
  for (int i = 0; i < 6; i++) {
    prism.d_[i] = { DistributionType::kNoRandom, rhombic ? distances[i] : 1.0f, 0.0f };
  }
  CrystalConfig crystal;
  crystal.id_ = 1;
  crystal.param_ = prism;
  if (random) {
    crystal.axis_.latitude_dist = { DistributionType::kUniform, 90.0f, 360.0f };
    crystal.axis_.azimuth_dist = kFullTurn;
    crystal.axis_.roll_dist = kFullTurn;
  } else if (horizontal) {
    // Internal latitude 90 is external zenith 0: a vertical c axis, with a uniform Rz spin.
    crystal.axis_.latitude_dist = { DistributionType::kNoRandom, 90.0f, 0.0f };
    crystal.axis_.azimuth_dist = kFullTurn;
    crystal.axis_.roll_dist = { DistributionType::kNoRandom, 0.0f, 0.0f };
  }
  ConfigManager config;
  config.crystals_.emplace(1, crystal);
  config.scene_.light_source_.param_ = SunParam{ horizontal ? 9.0f : 20.0f, horizontal ? 180.0f : 0.0f, 0.5f };
  config.scene_.light_source_.spectrum_ = std::vector<WlParam>{ { 550.0f, 1.0f } };
  ScatteringSetting setting{};
  setting.crystal_ = crystal;
  setting.crystal_proportion_ = 1.0f;
  MsInfo layer{};
  layer.prob_ = 0.0f;
  layer.setting_.push_back(std::move(setting));
  config.scene_.ms_.push_back(std::move(layer));
  return config;
}

PathFeatureReport Analyse(const ConfigManager& config, std::vector<int> faces, int samples = 8192,
                          SceneSpectrumSource spectrum_source = SceneSpectrumSource::kScene) {
  PathFeatureReportRequest request;
  request.crystal_id = 1;
  request.path_layers = { std::move(faces) };
  request.sample_count = samples;
  request.scene_spectrum_source = spectrum_source;
  PathFeatureReport report;
  const Error error = AnalyzePathFeatureReport(config, request, &report);
  EXPECT_TRUE(error.Ok()) << error.message;
  return report;
}

const PathFeature* Feature(const PathFeatureReport& report, const std::string& id) {
  const auto it = std::find_if(report.features.begin(), report.features.end(),
                               [&](const PathFeature& feature) { return feature.id == id; });
  return it == report.features.end() ? nullptr : &*it;
}

double Metric(const PathFeature& feature, const std::string& name) {
  const auto it = std::find_if(feature.metrics.begin(), feature.metrics.end(),
                               [&](const FeatureMetric& metric) { return metric.name == name; });
  EXPECT_NE(it, feature.metrics.end()) << name;
  return it == feature.metrics.end() ? 0.0 : it->value;
}

TEST(PathFeatureReport, RandomRegular315KeepsTheTwoMechanismsAndCounterfactualSeparate) {
  const PathFeatureReport report =
      Analyse(Scene(true, false, false), { 3, 1, 5 }, 8192, SceneSpectrumSource::kLegacyReferenceEndpoints);
  EXPECT_EQ(report.members.size(), 24u);

  const PathFeature* ordinary = Feature(report, "random_regular.3-1-5.solar_dispersion_edge");
  ASSERT_NE(ordinary, nullptr);
  ASSERT_EQ(ordinary->positions.size(), 2u);
  EXPECT_EQ(ordinary->evidence_status, "confirmed");
  EXPECT_NEAR(*ordinary->positions[0].deviation_deg, 21.612019265, 1e-5);
  EXPECT_NEAR(*ordinary->positions[1].deviation_deg, 22.371148713, 1e-5);

  const PathFeature* candidate = Feature(report, "random_regular.3-1-5.solar_caustic_candidate");
  ASSERT_NE(candidate, nullptr);
  EXPECT_EQ(candidate->evidence_status, "candidate");

  const PathFeature* tir = Feature(report, "random_regular.3-1-5.antisolar_tir_blue_band");
  ASSERT_NE(tir, nullptr);
  ASSERT_EQ(tir->positions.size(), 2u);
  EXPECT_EQ(tir->evidence_status, "confirmed");
  EXPECT_EQ(tir->visible, true);
  EXPECT_NEAR(*tir->positions[0].deviation_deg, 130.453312912, 0.2);
  EXPECT_NEAR(*tir->positions[1].deviation_deg, 138.774676645, 0.2);
  EXPECT_NEAR(Metric(*tir, "production_blue_red_ratio"), 2.072278, 0.03);
  EXPECT_NEAR(Metric(*tir, "without_internal_R_blue_red_ratio"), 0.753493, 0.02);
  EXPECT_GT(Metric(*tir, "production_blue_red_ratio"), 1.0);
  EXPECT_LT(Metric(*tir, "without_internal_R_blue_red_ratio"), 1.0);

  const PathFeature* exit_gate = Feature(report, "random_regular.3-1-5.exit_gate");
  ASSERT_NE(exit_gate, nullptr);
  EXPECT_EQ(exit_gate->visible, false);
}

TEST(PathFeatureReport, RhombicPlateReportsTwoPhysicalMembersAtSeparate120DegreeLocations) {
  const PathFeatureReport report =
      Analyse(Scene(false, true, true), { 1, 3, 4, 2 }, 8192, SceneSpectrumSource::kLegacyReferenceEndpoints);
  ASSERT_EQ(report.members.size(), 2u);
  ASSERT_EQ(report.features.size(), 2u);
  std::vector<double> relative_azimuths;
  for (const PathFeature& feature : report.features) {
    if (feature.positions.size() != 2u || !feature.positions[0].relative_solar_azimuth_deg.has_value()) {
      ADD_FAILURE() << feature.id << " has no two-wavelength positioned branch";
      continue;
    }
    relative_azimuths.push_back(*feature.positions[0].relative_solar_azimuth_deg);
    for (const FeaturePosition& position : feature.positions) {
      if (!position.spherical_separation_deg.has_value()) {
        ADD_FAILURE() << feature.id << " has no spherical separation";
        continue;
      }
      EXPECT_NEAR(*position.spherical_separation_deg, 117.599764152, 1e-9);
    }
    EXPECT_DOUBLE_EQ(Metric(feature, "physical_member_count"), 1.0);
  }
  ASSERT_EQ(relative_azimuths.size(), 2u);
  std::sort(relative_azimuths.begin(), relative_azimuths.end());
  EXPECT_NEAR(relative_azimuths[0], -120.0, 1e-10);
  EXPECT_NEAR(relative_azimuths[1], 120.0, 1e-10);

  // The first member is the LI representative. The a=1 normalisation cancels Lumice's half-scale
  // geometry and is therefore directly comparable to the independent fixture.
  const auto& rows = report.members[0].wavelengths;
  ASSERT_EQ(report.members[0].faces, std::vector<int>({ 1, 3, 4, 2 }));
  ASSERT_EQ(rows.size(), 2u);
  EXPECT_NEAR(rows[0].brightness.fine_mean_at, 0.0008701215155375575, 3e-10);
  EXPECT_NEAR(rows[1].brightness.fine_mean_at, 0.0008910040657962451, 3e-10);
}

TEST(PathFeatureReport, UnsupportedOrientationIsCoverageNotAnAbsenceClaim) {
  const PathFeatureReport report = Analyse(Scene(false, false, false), { 3, 5 }, 64);
  ASSERT_FALSE(report.coverage.empty());
  const auto orientation = std::find_if(report.coverage.begin(), report.coverage.end(),
                                        [](const CoverageItem& item) { return item.subject == "orientation_measure"; });
  ASSERT_NE(orientation, report.coverage.end());
  EXPECT_EQ(orientation->status, CoverageStatus::kNotSupported);
  EXPECT_TRUE(report.features.empty());
  ASSERT_FALSE(report.members.empty());
  EXPECT_EQ(report.members.front().wavelengths.front().brightness.status, CoverageStatus::kNotSupported);
}

TEST(PathFeatureReport, AComputedBrightnessOutsideTheFixedDetectorCaseDoesNotClaimAFeature) {
  ConfigManager config = Scene(true, false, false);
  std::get<PrismCrystalParam>(config.crystals_.at(1).param_).h_.center = 0.8f;
  const PathFeatureReport report = Analyse(config, { 3, 5 }, 64);
  ASSERT_FALSE(report.members.empty());
  EXPECT_EQ(report.members.front().wavelengths.front().brightness.status, CoverageStatus::kSupported);
  EXPECT_TRUE(report.features.empty());
  ASSERT_FALSE(report.coverage.empty());
  EXPECT_EQ(report.coverage.back().subject, "positioned_features");
  EXPECT_EQ(report.coverage.back().status, CoverageStatus::kNotSupported);
}

TEST(PathFeatureReport, TirBandRequiresTwoDistinctRefractiveIndices) {
  PathFeatureReportRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 1, 5 } };
  request.wavelengths_nm = { 550.0 };
  request.sample_count = 8192;
  PathFeatureReport report;
  const Error error = AnalyzePathFeatureReport(Scene(true, false, false), request, &report);
  ASSERT_TRUE(error.Ok()) << error.message;
  EXPECT_EQ(Feature(report, "random_regular.3-1-5.antisolar_tir_blue_band"), nullptr);
  const auto coverage = std::find_if(report.coverage.begin(), report.coverage.end(), [](const CoverageItem& item) {
    return item.subject == "3-1-5 antisolar TIR band";
  });
  ASSERT_NE(coverage, report.coverage.end());
  EXPECT_EQ(coverage->status, CoverageStatus::kNotSupported);
}

TEST(PathFeatureReport, DefaultDetectorSpectrumMatchesTheSceneMeasureSpectrum) {
  ConfigManager config = Scene(true, false, false);
  config.scene_.light_source_.spectrum_ = std::vector<WlParam>{ { 500.0f, 0.25f }, { 620.0f, 0.75f } };
  const PathFeatureReport report = Analyse(config, { 3, 5 }, 64);

  ASSERT_EQ(report.wavelengths.size(), 2u);
  ASSERT_EQ(report.scene_measure.spectrum_nodes.size(), report.wavelengths.size());
  for (size_t index = 0; index < report.wavelengths.size(); index++) {
    EXPECT_DOUBLE_EQ(report.wavelengths[index].wavelength_nm, report.scene_measure.spectrum_nodes[index].wavelength_nm);
    EXPECT_DOUBLE_EQ(report.wavelengths[index].weight, report.scene_measure.spectrum_nodes[index].weight);
  }
  ASSERT_FALSE(report.members.empty());
  ASSERT_EQ(report.members.front().wavelengths.size(), report.wavelengths.size());
  for (size_t index = 0; index < report.wavelengths.size(); index++) {
    EXPECT_DOUBLE_EQ(report.members.front().wavelengths[index].wavelength.wavelength_nm,
                     report.scene_measure.spectrum_nodes[index].wavelength_nm);
    EXPECT_DOUBLE_EQ(report.members.front().wavelengths[index].wavelength.weight,
                     report.scene_measure.spectrum_nodes[index].weight);
  }
}

TEST(PathFeatureReportJson, UsesASeparateSchemaAndDoesNotAcquireATarget) {
  const PathFeatureReport report = Analyse(Scene(true, false, false), { 3, 5 }, 64);
  const nlohmann::json doc = nlohmann::json::parse(PathFeatureReportToJson(report, "test-version"));
  EXPECT_EQ(doc["schema"], "lumice.path-feature-report");
  EXPECT_EQ(doc["schema_version"], 3);
  EXPECT_EQ(doc["scene_measure"]["spectrum_nodes"].size(), 1u);
  EXPECT_EQ(doc["scene_measure"]["spectrum_nodes"][0]["source"], "scene_discrete");
  ASSERT_FALSE(doc["scene_measure"]["sampled_rows"].empty());
  const auto& sampled_row = doc["scene_measure"]["sampled_rows"][0];
  EXPECT_TRUE(sampled_row.contains("joint_target_density"));
  EXPECT_TRUE(sampled_row.contains("joint_log_proposal_density"));
  EXPECT_TRUE(sampled_row.contains("joint_log_target_density"));
  EXPECT_EQ(sampled_row["joint_importance_weight_status"], "available");
  ASSERT_FALSE(sampled_row["layers"].empty());
  EXPECT_TRUE(sampled_row["layers"][0].contains("total_surface_area"));
  EXPECT_TRUE(sampled_row["layers"][0].contains("normalized_entry_factor"));
  EXPECT_TRUE(sampled_row["layers"][0].contains("selected_crystal_share"));
  ASSERT_EQ(sampled_row["layers"][0]["entries"].size(), 1u);
  EXPECT_EQ(sampled_row["layers"][0]["entries"][0]["entry_index"], 0);
  EXPECT_TRUE(sampled_row["layers"][0]["entries"][0].contains("accepted_share"));
  EXPECT_TRUE(sampled_row["layers"][0]["entries"][0].contains("acceptance_support_constant"));
  const auto evaluated_row = std::find_if(
      doc["scene_measure"]["sampled_rows"].begin(), doc["scene_measure"]["sampled_rows"].end(),
      [](const auto& row) { return !row["layers"].empty() && row["layers"][0]["entries"][0]["filter_evaluated"]; });
  ASSERT_NE(evaluated_row, doc["scene_measure"]["sampled_rows"].end());
  EXPECT_TRUE((*evaluated_row)["layers"][0]["entries"][0]["accepted"]);
  EXPECT_TRUE(doc["scene_measure"].contains("total_contribution_status"));
  EXPECT_TRUE(doc["scene_measure"].contains("absolute_error_estimate_status"));
  EXPECT_EQ(doc["generator"]["lumice"], "test-version");
  EXPECT_EQ(doc["meta"]["requested_faces"], nlohmann::json({ 3, 5 }));
  EXPECT_FALSE(doc["meta"].contains("target"));
  EXPECT_FALSE(doc.contains("components"));
  EXPECT_FALSE(doc["physical_l2_members"].empty());
  EXPECT_FALSE(doc["coverage"].empty());
  EXPECT_FALSE(doc["limitations"].empty());
}

TEST(PathFeatureReportJson, ShapeSamplesExposeTheDistributionJacobianAfterSyncAndHeightFold) {
  ConfigManager config = Scene(true, false, false);
  auto& shape = std::get<PrismCrystalParam>(config.crystals_.at(1).param_);
  shape.h_ = { DistributionType::kUniform, -0.25f, 0.25f };
  shape.sync_group_[kShapeScalarHeight] = 7;
  shape.sync_group_[kShapeScalarFace0] = 7;
  config.scene_.ms_[0].setting_[0].crystal_.param_ = shape;

  PathFeatureReportRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 5 } };
  request.wavelengths_nm = { 550.0 };
  request.sample_count = 64;
  request.member_selection = SceneMemberSelection::kConcrete;
  request.scene_measure_sample_count = 2;
  PathFeatureReport report;
  const Error error = AnalyzePathFeatureReport(config, request, &report);
  ASSERT_TRUE(error.Ok()) << error.message;

  const nlohmann::json doc = nlohmann::json::parse(PathFeatureReportToJson(report, "test-version"));
  ASSERT_FALSE(doc["scene_measure"]["sampled_rows"].empty());
  const auto& samples = doc["scene_measure"]["sampled_rows"][0]["layers"][0]["shape"];
  const auto by_name = [&](const char* name) {
    return std::find_if(samples.begin(), samples.end(), [name](const auto& sample) { return sample["name"] == name; });
  };
  const auto height = by_name("height");
  const auto follower = by_name("face_distance[0]");
  const auto atom = by_name("face_distance[1]");
  ASSERT_NE(height, samples.end());
  ASSERT_NE(follower, samples.end());
  ASSERT_NE(atom, samples.end());
  EXPECT_EQ((*height)["mapping_jacobian"], 0.25);
  EXPECT_EQ((*follower)["mapping_jacobian"], 0.25);
  EXPECT_EQ((*atom)["mapping_jacobian"], 0.0);
}

TEST(PathFeatureReportJson, ZeroWidthTypedPoseFactorsSerializeAsAtoms) {
  ConfigManager config = Scene(false, false, false);
  AxisDistribution axis;
  axis.latitude_dist = { DistributionType::kGaussian, 45.0f, 0.0f };
  axis.azimuth_dist = { DistributionType::kUniform, 12.0f, 0.0f };
  axis.roll_dist = { DistributionType::kLaplacian, 7.0f, 0.0f };
  config.crystals_.at(1).axis_ = axis;
  config.scene_.ms_[0].setting_[0].crystal_.axis_ = axis;

  const PathFeatureReport report = Analyse(config, { 3, 5 }, 64);
  const nlohmann::json doc = nlohmann::json::parse(PathFeatureReportToJson(report, "test-version"));
  const auto& factors = doc["scene_measure"]["factors"];
  for (const char* name : { "pose.latitude", "pose.azimuth", "pose.roll" }) {
    const auto factor =
        std::find_if(factors.begin(), factors.end(), [name](const auto& item) { return item["name"] == name; });
    if (factor == factors.end()) {
      ADD_FAILURE() << "missing JSON pose factor " << name;
      continue;
    }
    EXPECT_EQ((*factor)["support_dimension"], 0) << name;
    EXPECT_EQ((*factor)["measure"], "atom") << name;
  }
}

TEST(PathFeatureReportJson, MultiLayerPyramidRowsCarryReconstructibleShapeProvenance) {
  const auto pyramid = [](IdType id, float upper_wedge, float lower_wedge) {
    PyramidCrystalParam shape;
    shape.h_pyr_u_ = { DistributionType::kNoRandom, 0.4f, 0.0f };
    shape.h_prs_ = { DistributionType::kNoRandom, 1.0f, 0.0f };
    shape.h_pyr_l_ = { DistributionType::kNoRandom, 0.5f, 0.0f };
    for (Distribution& distance : shape.d_) {
      distance = { DistributionType::kNoRandom, 1.0f, 0.0f };
    }
    shape.wedge_angle_u_ = upper_wedge;
    shape.wedge_angle_l_ = lower_wedge;
    CrystalConfig crystal;
    crystal.id_ = id;
    crystal.param_ = shape;
    crystal.axis_.latitude_dist = { DistributionType::kNoRandom, 90.0f, 0.0f };
    crystal.axis_.azimuth_dist = { DistributionType::kNoRandom, 180.0f, 0.0f };
    crystal.axis_.roll_dist = { DistributionType::kNoRandom, 0.0f, 0.0f };
    return crystal;
  };
  const CrystalConfig first = pyramid(1, 22.0f, 31.0f);
  const CrystalConfig second = pyramid(2, 27.0f, 36.0f);
  ConfigManager config;
  config.scene_.light_source_.param_ = SunParam{ 20.0f, 17.0f, 0.0f };
  config.scene_.light_source_.spectrum_ = std::vector<WlParam>{ { 550.0f, 1.0f } };
  for (const CrystalConfig* crystal : { &first, &second }) {
    config.crystals_.emplace(crystal->id_, *crystal);
    ScatteringSetting setting{};
    setting.crystal_ = *crystal;
    setting.crystal_proportion_ = 1.0f;
    MsInfo layer{};
    layer.prob_ = crystal->id_ == first.id_ ? 1.0f : 0.0f;
    layer.setting_.push_back(std::move(setting));
    config.scene_.ms_.push_back(std::move(layer));
  }

  PathFeatureReportRequest request;
  request.crystal_id = first.id_;
  request.layer_crystal_ids = { first.id_, second.id_ };
  request.path_layers = { { 3, 6 }, { 3, 6 } };
  request.wavelengths_nm = { 550.0 };
  request.sample_count = 64;
  request.scene_measure_sample_count = 2;
  request.member_selection = SceneMemberSelection::kConcrete;
  PathFeatureReport report;
  const Error error = AnalyzePathFeatureReport(config, request, &report);
  ASSERT_TRUE(error.Ok()) << error.message;

  const nlohmann::json doc = nlohmann::json::parse(PathFeatureReportToJson(report, "test-version"));
  const auto& rows = doc["scene_measure"]["sampled_rows"];
  const auto row = std::find_if(rows.begin(), rows.end(), [](const auto& item) { return item["layers"].size() == 2; });
  ASSERT_NE(row, rows.end());
  for (const auto& layer : (*row)["layers"]) {
    if (layer["crystal_kind"] != "pyramid") {
      ADD_FAILURE() << "multi-layer provenance must identify pyramid geometry";
      continue;
    }
    const int id = layer["crystal_id"];
    if (id != first.id_ && id != second.id_) {
      ADD_FAILURE() << "unexpected crystal id in multi-layer provenance";
      continue;
    }
    const CrystalConfig& expected = id == first.id_ ? first : second;
    const auto& expected_shape = std::get<PyramidCrystalParam>(expected.param_);
    EXPECT_EQ(layer["upper_wedge_deg"], expected_shape.wedge_angle_u_);
    EXPECT_EQ(layer["lower_wedge_deg"], expected_shape.wedge_angle_l_);
  }
}

TEST(PathFeatureReportJson, NonFiniteMeasureValuesCarryAnExplicitNumericalStatus) {
  PathFeatureReport report;
  report.scene_measure.status = SceneMeasureStatus::kNumericalIncomplete;
  report.scene_measure.reason = "total contribution accumulation exceeds the finite double range";
  report.scene_measure.total_contribution = std::numeric_limits<double>::infinity();
  report.scene_measure.total_contribution_status = SceneMeasureNumericStatus::kOverflow;
  const nlohmann::json doc = nlohmann::json::parse(PathFeatureReportToJson(report, "test-version"));
  EXPECT_EQ(doc["scene_measure"]["status"], "numerical_incomplete");
  EXPECT_TRUE(doc["scene_measure"]["total_contribution"].is_null());
  EXPECT_EQ(doc["scene_measure"]["total_contribution_status"], "overflow");
  EXPECT_EQ(doc["scene_measure"]["reason"], report.scene_measure.reason);
}

}  // namespace
}  // namespace lumice::raypath
