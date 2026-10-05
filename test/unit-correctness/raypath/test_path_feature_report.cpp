#include <gtest/gtest.h>

#include <algorithm>
#include <nlohmann/json.hpp>

#include "raypath/detail/path_feature_report.hpp"
#include "raypath/detail/path_feature_report_json.hpp"

namespace lumice::raypath {
namespace {
ConfigManager Scene() {
  PrismCrystalParam prism;
  prism.h_ = { DistributionType::kNoRandom, 1, 0 };
  for (auto& distance : prism.d_) {
    distance = { DistributionType::kNoRandom, 1, 0 };
  }
  CrystalConfig crystal{};
  crystal.id_ = 1;
  crystal.param_ = prism;
  crystal.axis_.latitude_dist = { DistributionType::kUniform, 90, 360 };
  crystal.axis_.azimuth_dist = crystal.axis_.roll_dist = { DistributionType::kUniform, 0, 360 };
  ConfigManager config;
  config.crystals_.emplace(1, crystal);
  config.scene_.light_source_.param_ = { 20, 0, 0 };
  config.scene_.light_source_.spectrum_ = std::vector<WlParam>{ { 450, .2f }, { 550, .3f }, { 650, .5f } };
  ScatteringSetting entry{};
  entry.crystal_ = crystal;
  entry.crystal_proportion_ = 1;
  config.scene_.ms_.push_back({ 0, { entry } });
  return config;
}
TEST(PathFeatureReport, ActualSpectrumPhysicalScopeAndNoFormulaDispatcher) {
  PathFeatureReportRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 5 } };
  request.sample_count = 8192;
  request.max_field_evaluations = 1;
  PathFeatureReport report;
  ASSERT_TRUE(AssemblePathFeatureReport(Scene(), request, &report).Ok());
  const auto document = nlohmann::json::parse(PathFeatureReportToJson(report, "test"));
  EXPECT_EQ(document.at("schema_version"), 2);
  EXPECT_EQ(document.at("spectrum").size(), 3u);
  EXPECT_EQ(document.at("spectrum")[0]["nm"], 450);
  EXPECT_EQ(document.at("scope").at("member_semantics"), "physical_L2");
  EXPECT_EQ(document.at("outcome"), "partial");
  EXPECT_FALSE(document.contains("target"));
  EXPECT_FALSE(document.contains("internal_provisional"));
  EXPECT_FALSE(document.at("sources").empty());
  bool candidate = false;
  for (const auto& feature : report.discovery.features) {
    EXPECT_EQ(feature.kind.find("random_regular"), std::string::npos);
    candidate |= feature.deviation_minimum.has_value();
  }
  EXPECT_TRUE(candidate);
}
TEST(PathFeatureReport, LargeExpandedRequestsStopAsPartialNotAsAnUpfrontAbsence) {
  PathFeatureReportRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 1, 5 } };
  request.sample_count = 1000000;
  request.max_optical_evaluations = 100;
  request.max_field_evaluations = 1;
  request.budget_ms = 100;
  PathFeatureReport report;
  ASSERT_TRUE(AssemblePathFeatureReport(Scene(), request, &report).Ok());
  EXPECT_EQ(report.discovery.measure.optical_evaluations, 100u);
  EXPECT_TRUE(report.discovery.budget_exhausted);
  EXPECT_FALSE(report.discovery.unfinished.empty());
  EXPECT_EQ(nlohmann::json::parse(PathFeatureReportToJson(report, "test"))["outcome"], "partial");
}
TEST(PathFeatureReport, BoundedSearchCompletionIsDistinctFromGlobalCoverage) {
  PathFeatureReportRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 5 } };
  request.sample_count = 64;
  request.wavelengths_nm = { 550 };
  PathFeatureReport report;
  ASSERT_TRUE(AssemblePathFeatureReport(Scene(), request, &report).Ok());
  EXPECT_EQ(report.discovery.measure.completed_samples, 64u);
  EXPECT_FALSE(report.discovery.budget_exhausted);
  EXPECT_TRUE(report.discovery.unfinished.empty());
  EXPECT_FALSE(report.discovery.limitations.empty());
  const auto document = nlohmann::json::parse(PathFeatureReportToJson(report, "test"));
  EXPECT_EQ(document["outcome"], "completed");
  EXPECT_EQ(document["coverage"][1]["limitations"], report.discovery.limitations);
  EXPECT_FALSE(document["candidates"].empty());
  // Completing the bounded search does not upgrade unsuccessful local hypotheses.
  EXPECT_FALSE(document["unfinished"].empty());
}

TEST(PathFeatureReport, UnsupportedChainsDoNotBypassRequestValidation) {
  PathFeatureReportRequest valid;
  valid.crystal_id = 1;
  valid.path_layers = { { 3, 5 }, { 1, 3 } };
  auto config = Scene();
  PathFeatureReport report;
  ASSERT_TRUE(AssemblePathFeatureReport(config, valid, &report).Ok());
  EXPECT_TRUE(report.unsupported_multicrystal);
  EXPECT_EQ(report.discovery.measure.optical_evaluations, 0u);
  EXPECT_TRUE(report.snapshot.layers.empty());

  auto request = valid;
  request.crystal_id = 99;
  EXPECT_EQ(AssemblePathFeatureReport(config, request, &report).code, ErrorCode::kUnknownCrystalId);
  EXPECT_FALSE(report.unsupported_multicrystal);
  request = valid;
  request.scene_layer = 99;
  EXPECT_EQ(AssemblePathFeatureReport(config, request, &report).code, ErrorCode::kInvalidArgument);
  request = valid;
  request.budget_ms = -1;
  EXPECT_EQ(AssemblePathFeatureReport(config, request, &report).code, ErrorCode::kInvalidArgument);
  request = valid;
  request.symmetry_bits = 8;
  EXPECT_EQ(AssemblePathFeatureReport(config, request, &report).code, ErrorCode::kInvalidArgument);
  request = valid;
  request.bandwidth_rad = 0;
  EXPECT_EQ(AssemblePathFeatureReport(config, request, &report).code, ErrorCode::kInvalidArgument);
  request = valid;
  request.wavelengths_nm = { 550 };
  request.wavelength_weights = { -1 };
  EXPECT_EQ(AssemblePathFeatureReport(config, request, &report).code, ErrorCode::kWavelengthOutOfRange);
  config.scene_.light_source_.spectrum_ = std::vector<WlParam>{ { 550, -1 } };
  EXPECT_FALSE(AssemblePathFeatureReport(config, valid, &report).Ok());
}

TEST(PathFeatureReport, ExactZeroSpectralSignalIsNotAClaimBasedOnEmptySampling) {
  auto config = Scene();
  config.scene_.light_source_.spectrum_ = std::vector<WlParam>{ { 550, 0 } };
  PathFeatureReportRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 5 } };
  request.sample_count = 64;
  PathFeatureReport report;
  ASSERT_TRUE(AssemblePathFeatureReport(config, request, &report).Ok());
  EXPECT_TRUE(report.no_related_signal);
  EXPECT_EQ(nlohmann::json::parse(PathFeatureReportToJson(report, "test"))["outcome"], "no_related_feature");
  request.path_layers = { { 3, 5 }, { 1, 3 } };
  EXPECT_TRUE(AssemblePathFeatureReport(config, request, &report).Ok());
  EXPECT_EQ(nlohmann::json::parse(PathFeatureReportToJson(report, "test"))["outcome"], "unsupported_multicrystal");
}
}  // namespace
}  // namespace lumice::raypath
