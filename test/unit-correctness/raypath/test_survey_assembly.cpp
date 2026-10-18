#include <gtest/gtest.h>

#include <cmath>
#include <nlohmann/json.hpp>
#include <set>
#include <string>

#include "core/optics.hpp"  // IceRefractiveIndex: the dispersion closed form's n pair
#include "raypath/detail/survey/survey_assembly.hpp"
#include "raypath/detail/survey/survey_json.hpp"

namespace lumice::raypath::survey {
namespace {

constexpr double kPi = 3.14159265358979323846;

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

// The C01 shape in memory: a regular hexagonal column, every axis slot uniform over the full
// sphere (random orientation), sun on the horizon. The corpus config's crystal and scene
// without the corpus file.
ConfigManager RandomPrismScene() {
  PrismCrystalParam prism;
  prism.h_ = { DistributionType::kNoRandom, 1.0f, 0 };
  for (auto& distance : prism.d_) {
    distance = { DistributionType::kNoRandom, 1, 0 };
  }
  CrystalConfig crystal{};
  crystal.id_ = 1;
  crystal.param_ = prism;
  // Full-sphere uniform: the internal latitude's center is 90 (the config's zenith-0 slot
  // spelling), and IsFullSphereUniform requires exactly that center with a full 360 range.
  crystal.axis_.latitude_dist = { DistributionType::kUniform, 90, 360 };
  crystal.axis_.azimuth_dist = crystal.axis_.roll_dist = { DistributionType::kUniform, 0, 360 };
  ConfigManager config;
  config.crystals_.emplace(1, crystal);
  config.scene_.light_source_.param_ = { 0, 0, 0 };
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

TEST(SurveyAssembly, NonRankZeroPlateClassStaysUnevaluatedWithTheFamilyRestrictedReason) {
  const ConfigManager config = PlateScene();
  SurveyRequest request = BaseRequest();
  request.path_layers = { { 3, 5 } };  // through two side faces: a deviating prism path
  SurveyDocument document;
  ASSERT_TRUE(AssembleSurvey(config, request, &document).Ok());
  EXPECT_FALSE(document.early_stopped);
  EXPECT_TRUE(document.objects.empty());
  ASSERT_EQ(document.unevaluated.size(), 1u);
  // A plate is a family-restricted class: the T1 rank must be taken on the limit family, and
  // that routing is not wired — the reason names exactly that, not a generic not-wired.
  EXPECT_EQ(document.unevaluated.front().reason,
            "family-restricted T1 routing not wired (gap: measure-to-family mapping)");
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

TEST(SurveyAssembly, RandomPrismLiteralAssemblesTheThreeT1Cards) {
  const ConfigManager config = RandomPrismScene();
  SurveyRequest request = BaseRequest();
  request.path_layers = { { 3, 5 } };
  request.symmetry_bits = 0;  // the literal single member: the seconds-scale shape of the arm
  SurveyDocument document;
  ASSERT_TRUE(AssembleSurvey(config, request, &document).Ok());
  EXPECT_FALSE(document.early_stopped);
  // The document-level unevaluated list is empty: the full-dimensional arm produced its cards
  // (the rank-0-family and rank-1-family kinds are structurally absent on this route, not
  // uncomputed).
  EXPECT_TRUE(document.unevaluated.empty());
  ASSERT_EQ(document.objects.size(), 3u);
  std::set<std::string> kinds;
  for (const SurveyObject& card : document.objects) {
    kinds.insert(SurveyObjectKindName(card.kind));
  }
  EXPECT_EQ(kinds, std::set<std::string>({ "support_set", "caustic_edge", "bright_domain" }));

  // The support_set card: the corpus 450nm partition shape (three intervals, the first closed,
  // the rest open components) with the endpoint mechanism vocabulary.
  const SurveyObject& support = document.objects[0];
  EXPECT_EQ(support.kind, SurveyObjectKind::kSupportSet);
  ASSERT_TRUE(support.support_set.has_value());
  EXPECT_EQ(support.evidence.state, EvidenceState::kFinite);
  ASSERT_EQ(support.support_set->intervals.size(), 3u);
  EXPECT_EQ(support.support_set->intervals[0].n_closed, 1);
  EXPECT_EQ(support.support_set->intervals[1].n_closed, 0);
  EXPECT_EQ(support.support_set->intervals[2].n_closed, 0);
  std::set<std::string> sources;
  for (const SurveyObject::EndpointLabel& label : support.support_set->endpoints) {
    sources.insert(label.source);
  }
  EXPECT_EQ(sources, std::set<std::string>({ "interior_minimum", "boundary_extremum", "corner" }));
  EXPECT_EQ(support.support_set->symmetry_bits, 0u);
  EXPECT_EQ(support.support_set->member_count, 1);

  // The caustic_edge card: the min-deviation fold, certified closed-form on the two-face chain.
  const SurveyObject& edge = document.objects[1];
  EXPECT_EQ(edge.kind, SurveyObjectKind::kCausticEdge);
  ASSERT_TRUE(edge.caustic_edge.has_value());
  EXPECT_TRUE(edge.caustic_edge->coincident_with_support);
  EXPECT_EQ(edge.caustic_edge->profile, "finite_jump");
  ASSERT_EQ(edge.evidence.state, EvidenceState::kCertified);
  EXPECT_EQ(edge.evidence.method, std::string(kCertifiedMethodSlabMinDeviationClosedForm));
  ASSERT_TRUE(edge.caustic_edge->certificate.has_value());
  // The corpus anchor: the closed form for the 60-degree prism (face normals 120 degrees apart).
  const double closed_form_deg = 2.0 * std::asin(document.refractive_index * 0.5) * 180.0 / kPi - 60.0;
  const double error = std::fabs(edge.caustic_edge->position_deg - closed_form_deg);
  EXPECT_LT(error, 1e-4);  // the engine's own cross-check tolerance is 1e-6 rad (~5.7e-5 deg)
  EXPECT_EQ(edge.caustic_edge->certificate->formula, "2*asin(n*sin(A/2)) - A");
  EXPECT_NEAR(edge.caustic_edge->certificate->apex_angle_deg, 60.0, 1e-6);
  // The support's lower end is the same edge (the coincidence the corpus lesson turns on).
  EXPECT_NEAR(support.support_set->intervals[0].lower_deg, edge.caustic_edge->position_deg, 1e-4);

  // The bright_domain card: the span plus the probe leg's readings (the peak is a finite
  // profile quantity, not the certified edge; the readings reference the probes[] entry).
  const SurveyObject& domain = document.objects[2];
  EXPECT_EQ(domain.kind, SurveyObjectKind::kBrightDomain);
  ASSERT_TRUE(domain.bright_domain.has_value());
  EXPECT_TRUE(domain.bright_domain->has_peak);
  EXPECT_GT(domain.bright_domain->peak_value, 0.0);
  EXPECT_TRUE(domain.bright_domain->has_edge_reading);
  // The finite edge reading sits in the same bin as the certified position (half a bin).
  EXPECT_NEAR(domain.bright_domain->edge_delta_deg, edge.caustic_edge->position_deg, 0.05);
  EXPECT_GT(domain.bright_domain->kept_count, 0);
  EXPECT_EQ(domain.bright_domain->probe_index, 0);
  ASSERT_EQ(document.probes.size(), 1u);
  EXPECT_EQ(document.probes.front().kind, "solar_radial");
  // The chromatic layer's v1 legs: the dispersion direction is the corpus's red-inner-blue-
  // outer, the displacement matches the closed-form pair, and the two mechanisms stay in
  // separate sub-blocks with the tint leg's honest unevaluated.
  ASSERT_TRUE(document.positional_dispersion.has_value());
  EXPECT_EQ(document.positional_dispersion->direction, "red_inner_blue_outer");
  EXPECT_LT(document.positional_dispersion->value_red_deg, document.positional_dispersion->value_blue_deg);
  const double dispersion_closed_form =
      std::fabs((2.0 * std::asin(IceRefractiveIndex::Get(450.0) * 0.5) - 1.0471975511965976) * 180.0 / kPi -
                (2.0 * std::asin(IceRefractiveIndex::Get(650.0) * 0.5) - 1.0471975511965976) * 180.0 / kPi);
  EXPECT_NEAR(document.positional_dispersion->displacement_deg, dispersion_closed_form, 1e-4);
  EXPECT_DOUBLE_EQ(document.positional_dispersion->n_red, IceRefractiveIndex::Get(650.0));
  EXPECT_DOUBLE_EQ(document.positional_dispersion->n_blue, IceRefractiveIndex::Get(450.0));
  ASSERT_TRUE(document.diagnose.has_value());
  EXPECT_FALSE(document.diagnose->verdict_kind.empty());
  ASSERT_TRUE(document.weight_tint.has_value());
  EXPECT_EQ(document.weight_tint->state, EvidenceState::kUnevaluated);
  EXPECT_EQ(document.chromatic.state, EvidenceState::kFinite);
  // The document's own evidence stays finite: the routing refused nothing.
  EXPECT_EQ(document.evidence.state, EvidenceState::kFinite);
}

TEST(SurveyAssembly, StandaloneCrystalNotInAnySceneLayerStillRoutes) {
  ConfigManager config = PlateScene();
  config.scene_.ms_.clear();  // the crystal is configured but allocated to no scattering layer
  SurveyDocument document;
  ASSERT_TRUE(AssembleSurvey(config, BaseRequest(), &document).Ok());
  // The standalone branch: no scene layer is named, and the identity says so.
  EXPECT_FALSE(document.scene_layer.has_value());
  EXPECT_EQ(document.scene_identity, std::string("standalone configured crystal (not a scene allocation)"));
  // The routing itself is unchanged: the same plate class collapses and stops.
  EXPECT_TRUE(document.early_stopped);
  ASSERT_EQ(document.objects.size(), 1u);
  EXPECT_EQ(document.objects.front().kind, SurveyObjectKind::kSourceImage);
  EXPECT_EQ(document.objects.front().evidence.state, EvidenceState::kCertified);
  EXPECT_TRUE(document.unevaluated.empty());
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
