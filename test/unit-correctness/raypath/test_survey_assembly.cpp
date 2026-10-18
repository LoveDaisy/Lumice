#include <gtest/gtest.h>

#include <cmath>
#include <nlohmann/json.hpp>
#include <set>
#include <string>

#include "raypath/detail/survey/survey_assembly.hpp"
#include "raypath/detail/survey/survey_json.hpp"

namespace lumice::raypath::survey {
namespace {

// The C14 shape in memory: a hexagonal plate prism — c axis at the zenith (config zenith 0, the
// latitude slot's 90) with a 0.3-degree Gaussian wobble, azimuth and roll uniform over 360 — sun
// at 20/0. The same numbers the corpus config carries, without the corpus file (which never
// enters the tree). Note the axis lives in the config's latitude slot, where 90 IS zenith 0.
ConfigManager PlateScene() {
  PrismCrystalParam prism;
  prism.h_ = { DistributionType::kNoRandom, 0.3f, 0 };
  for (auto& distance : prism.d_) {
    distance = { DistributionType::kNoRandom, 1, 0 };
  }
  CrystalConfig crystal{};
  crystal.id_ = 1;
  crystal.param_ = prism;
  crystal.axis_.latitude_dist = { DistributionType::kGaussian, 90, 0.3f };
  crystal.axis_.azimuth_dist = crystal.axis_.roll_dist = { DistributionType::kUniform, 0, 360 };
  ConfigManager config;
  config.crystals_.emplace(1, crystal);
  config.scene_.light_source_.param_ = { 20, 0, 0 };
  config.scene_.light_source_.spectrum_ = IlluminantType::kD65;
  ScatteringSetting entry{};
  entry.crystal_ = crystal;
  entry.crystal_proportion_ = 1;
  config.scene_.ms_.push_back({ 0, { entry } });
  return config;
}

SurveyRequest BaseRequest() {
  SurveyRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 1, 2 } };
  return request;
}

TEST(SurveyAssembly, RankZeroClassCollapsesToOneSourceImageCardAndStops) {
  const ConfigManager config = PlateScene();
  SurveyDocument document;
  ASSERT_TRUE(AssembleSurvey(config, BaseRequest(), &document).Ok());
  EXPECT_TRUE(document.early_stopped);
  ASSERT_EQ(document.objects.size(), 1u);
  const SurveyObject& card = document.objects.front();
  EXPECT_EQ(card.kind, SurveyObjectKind::kSourceImage);
  EXPECT_EQ(card.evidence.state, EvidenceState::kCertified);
  EXPECT_TRUE(CertifiedMethodRegistered(card.evidence.method));
  // The card is the sky-side object; the PBD class's one member is its preimage. The physical
  // plate ensemble is not symmetric under c -> -c (zenith 0 wobble is not zenith 180 wobble), so
  // the horizontal mirror is not applicable and the reversed sequence 2-1 is NOT in the class.
  ASSERT_EQ(card.members.size(), 1u);
  EXPECT_EQ(card.members.front(), std::vector<int>({ 1, 2 }));
  EXPECT_TRUE(card.has_position);
  EXPECT_DOUBLE_EQ(card.position_altitude_deg, 20.0);
  EXPECT_DOUBLE_EQ(card.position_azimuth_deg, 0.0);
  EXPECT_TRUE(card.certificate.has_value());
  // The negative half of the smoke contract, at the model level: no other kind exists.
  for (const SurveyMemberVerdict& verdict : document.member_verdicts) {
    EXPECT_TRUE(verdict.rank_zero);
  }
  EXPECT_TRUE(document.unevaluated.empty());
  EXPECT_EQ(document.evidence.state, EvidenceState::kCertified);
  EXPECT_EQ(document.wavelength_source, std::string(WavelengthSourceName(WavelengthSource::kDefault)));
  EXPECT_DOUBLE_EQ(document.wavelength_nm, 550.0);
  EXPECT_TRUE(std::isfinite(document.refractive_index));
  EXPECT_EQ(document.scene_layer.value_or(-1), 0);
}

TEST(SurveyAssembly, LiteralScopeKeepsOnlyTheRequestedSequence) {
  const ConfigManager config = PlateScene();
  SurveyRequest request = BaseRequest();
  request.symmetry_bits = 0;
  SurveyDocument document;
  ASSERT_TRUE(AssembleSurvey(config, request, &document).Ok());
  EXPECT_TRUE(document.early_stopped);
  ASSERT_EQ(document.objects.size(), 1u);
  EXPECT_EQ(document.objects.front().members.size(), 1u);
  EXPECT_EQ(document.objects.front().members.front(), std::vector<int>({ 1, 2 }));
  ASSERT_EQ(document.member_verdicts.size(), 1u);
  EXPECT_EQ(document.member_verdicts.front().member, std::vector<int>({ 1, 2 }));
  EXPECT_EQ(document.request.symmetry_bits, 0u);
}

TEST(SurveyAssembly, NonRankZeroClassProducesTheUnevaluatedSkeleton) {
  const ConfigManager config = PlateScene();
  SurveyRequest request = BaseRequest();
  request.path_layers = { { 3, 5 } };  // through two side faces: a deviating prism path
  SurveyDocument document;
  ASSERT_TRUE(AssembleSurvey(config, request, &document).Ok());
  EXPECT_FALSE(document.early_stopped);
  EXPECT_TRUE(document.objects.empty());
  ASSERT_EQ(document.unevaluated.size(), 1u);
  EXPECT_EQ(document.unevaluated.front().reason, "survey v1: T1/T2 not wired");
  ASSERT_EQ(document.unevaluated.front().kinds.size(), 5u);
  for (const SurveyObjectKind kind : document.unevaluated.front().kinds) {
    EXPECT_NE(kind, SurveyObjectKind::kSourceImage);
  }
  EXPECT_FALSE(document.member_verdicts.empty());
  for (const SurveyMemberVerdict& verdict : document.member_verdicts) {
    EXPECT_FALSE(verdict.rank_zero);
  }
  // The T0 facts stand on their own finite footing; the routing verdict is not a refusal unless
  // a classification actually refused.
  EXPECT_EQ(document.evidence.state, EvidenceState::kFinite);
  EXPECT_EQ(document.evidence.coverage.members_evaluated, static_cast<int>(document.member_verdicts.size()));
}

TEST(SurveyAssembly, RequestRefusalsKeepTheReportErrorSemantics) {
  const ConfigManager config = PlateScene();
  SurveyDocument document;
  SurveyRequest request = BaseRequest();
  request.crystal_id = 99;
  EXPECT_EQ(AssembleSurvey(config, request, &document).code, ErrorCode::kUnknownCrystalId);

  request = BaseRequest();
  request.path_layers = { { 1, 2 }, { 1, 2 } };
  EXPECT_EQ(AssembleSurvey(config, request, &document).code, ErrorCode::kMultiLayerUnsupported);

  request = BaseRequest();
  request.path_layers = { { 1, 99 } };
  EXPECT_EQ(AssembleSurvey(config, request, &document).code, ErrorCode::kFaceNotInCrystal);

  request = BaseRequest();
  request.path_layers = {};
  EXPECT_EQ(AssembleSurvey(config, request, &document).code, ErrorCode::kInvalidPath);

  request = BaseRequest();
  request.wavelength_nm = 200.0;
  EXPECT_EQ(AssembleSurvey(config, request, &document).code, ErrorCode::kWavelengthOutOfRange);

  request = BaseRequest();
  request.budget_ms = kMaxSurveyBudgetMs + 1;
  EXPECT_EQ(AssembleSurvey(config, request, &document).code, ErrorCode::kInvalidArgument);

  request = BaseRequest();
  request.symmetry_bits = 8;
  EXPECT_EQ(AssembleSurvey(config, request, &document).code, ErrorCode::kInvalidArgument);
}

TEST(SurveyAssembly, EndToEndJsonCarriesTheSchema4Contract) {
  const ConfigManager config = PlateScene();
  SurveyDocument document;
  ASSERT_TRUE(AssembleSurvey(config, BaseRequest(), &document).Ok());
  const auto parsed = nlohmann::json::parse(SurveyDocumentToJson(document, "test"));
  EXPECT_EQ(parsed.at("schema"), "lumice.path-survey");
  EXPECT_EQ(parsed.at("schema_version"), 4);
  ASSERT_EQ(parsed.at("objects").size(), 1u);
  EXPECT_EQ(parsed.at("objects")[0].at("kind"), "source_image");
  EXPECT_EQ(parsed.at("objects")[0].at("evidence").at("state"), "certified");
  EXPECT_FALSE(parsed.at("objects")[0].at("evidence").at("method").get<std::string>().empty());
  EXPECT_DOUBLE_EQ(parsed.at("objects")[0].at("position").at("altitude_deg").get<double>(), 20.0);
  EXPECT_DOUBLE_EQ(parsed.at("objects")[0].at("position").at("azimuth_deg").get<double>(), 0.0);
  // The negative assertion's mechanical form: the kind set is exactly {source_image}.
  std::set<std::string> kinds;
  for (const auto& object : parsed.at("objects")) {
    kinds.insert(object.at("kind").get<std::string>());
  }
  EXPECT_EQ(kinds, std::set<std::string>({ "source_image" }));
}

}  // namespace
}  // namespace lumice::raypath::survey
