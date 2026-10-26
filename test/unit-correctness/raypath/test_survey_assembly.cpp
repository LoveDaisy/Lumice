#include <gtest/gtest.h>

#include <cmath>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <utility>

#include "core/optics.hpp"  // IceRefractiveIndex: the dispersion closed form's n pair
#include "raypath/detail/survey/survey_assembly.hpp"
#include "raypath/survey_report.hpp"
#include "survey_test_fixtures.hpp"

namespace lumice::raypath::survey {
namespace {

constexpr double kPi = 3.14159265358979323846;

// The scenes live in survey_test_fixtures.hpp (the corpus shapes without the corpus files:
// PlateScene(height, wobble_std) is the C14/C15 plate, RandomPrismScene() the C01 column).

SurveyRequest BaseRequest() {
  SurveyRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 1, 2 } };
  return request;
}

TEST(SurveyAssembly, RankZeroClassCollapsesToOneSourceImageCardAndStops) {
  const ConfigManager config = PlateScene(0.3, 0.3);
  SurveyData document;
  ASSERT_TRUE(AssembleSurveyData(config, BaseRequest(), &document).Ok());
  EXPECT_TRUE(document.early_stopped);
  ASSERT_EQ(document.objects.size(), 1u);
  const SurveyObject& card = document.objects.front();
  EXPECT_EQ(card.Kind(), SurveyObjectKind::kSourceImage);
  EXPECT_EQ(card.State(), EvidenceState::kCertified);
  EXPECT_TRUE(CertifiedMethodRegistered(card.Method()));
  // The card is the sky-side object; the PBD class's one member is its preimage. The physical
  // plate ensemble is not symmetric under c -> -c (zenith 0 wobble is not zenith 180 wobble), so
  // the horizontal mirror is not applicable and the reversed sequence 2-1 is NOT in the class.
  ASSERT_EQ(card.members.size(), 1u);
  EXPECT_EQ(card.members.front(), std::vector<int>({ 1, 2 }));
  EXPECT_TRUE(card.has_position);
  EXPECT_DOUBLE_EQ(card.position_altitude_deg, 20.0);
  EXPECT_DOUBLE_EQ(card.position_azimuth_deg, 0.0);
  EXPECT_TRUE(card.Certificate() != nullptr);
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
  const ConfigManager config = PlateScene(0.3, 0.3);
  SurveyRequest request = BaseRequest();
  request.symmetry_bits = 0;
  SurveyData document;
  ASSERT_TRUE(AssembleSurveyData(config, request, &document).Ok());
  EXPECT_TRUE(document.early_stopped);
  // The early-stop document carries no family_routing key at all (the conditional-presence
  // negative): the rank-0 certificate's own card is the whole story.
  EXPECT_FALSE(document.family_routing.has_value());
  ASSERT_EQ(document.objects.size(), 1u);
  EXPECT_EQ(document.objects.front().members.size(), 1u);
  EXPECT_EQ(document.objects.front().members.front(), std::vector<int>({ 1, 2 }));
  ASSERT_EQ(document.member_verdicts.size(), 1u);
  EXPECT_EQ(document.member_verdicts.front().member, std::vector<int>({ 1, 2 }));
  EXPECT_EQ(document.request.symmetry_bits, 0u);
}

// The family-restricted plate class (the v1 not-wired skeleton's own shape) now routes into
// the family arm: the measure is read, the ladder runs (level 0's support, level 1's zenith
// freeze on the {azimuth, roll} pole spin chart — the wedge member's one-dimensional image,
// the pole degeneracy being data), and no unevaluated skeleton remains.
TEST(SurveyAssembly, FamilyRestrictedPlateRoutesIntoTheLadderArm) {
  const ConfigManager config = PlateScene(0.3, 0.3);
  SurveyRequest request = BaseRequest();
  request.path_layers = { { 3, 5 } };  // through two side faces: a deviating prism path
  request.symmetry_bits = 0;           // the literal member: the arm's per-member shape, not the 12-member sweep
  SurveyData document;
  ASSERT_TRUE(AssembleSurveyData(config, request, &document).Ok());
  EXPECT_FALSE(document.early_stopped);
  EXPECT_TRUE(document.unevaluated.empty());
  ASSERT_TRUE(document.family_routing.has_value());
  EXPECT_EQ(document.family_routing->measure.zenith.kind, FamilyDimKind::kGauss);
  EXPECT_NEAR(document.family_routing->measure.zenith.width_deg, 0.3, 1e-6);
  ASSERT_EQ(document.family_routing->ladder.size(), 2u);
  EXPECT_FALSE(document.family_routing->ladder[0].rank_asked);
  ASSERT_TRUE(document.family_routing->ladder[1].rank_asked);
  EXPECT_EQ(document.family_routing->ladder[1].frozen_dims.at(0).dim, analytic::FamilyDimension::kZenith);
  // The pole spin chart's wedge member: a one-dimensional image (the chart degeneracy is
  // data, not an error — the restricted-family rank test's own anchor).
  EXPECT_EQ(document.family_routing->ladder[1].generic_rank, 1);
  // The cards: the level-0 support plus the level-1 survivors, each carrying its band.
  std::set<std::string> kinds;
  for (const SurveyObject& card : document.objects) {
    kinds.insert(SurveyObjectKindName(card.Kind()));
    if (card.Kind() == SurveyObjectKind::kSupportSet) {
      continue;
    }
    EXPECT_TRUE(card.band.has_value());
  }
  EXPECT_NE(kinds.find("support_set"), kinds.end());
  EXPECT_NE(kinds.find("regular_image"), kinds.end());
  EXPECT_FALSE(document.member_verdicts.empty());
  for (const SurveyMemberVerdict& verdict : document.member_verdicts) {
    EXPECT_FALSE(verdict.rank_zero);
  }
  EXPECT_EQ(document.evidence.state, EvidenceState::kFinite);
  EXPECT_EQ(document.evidence.coverage.members_evaluated, static_cast<int>(document.member_verdicts.size()));
}

TEST(SurveyAssembly, RandomPrismLiteralAssemblesTheThreeT1Cards) {
  const ConfigManager config = RandomPrismScene();
  SurveyRequest request = BaseRequest();
  request.path_layers = { { 3, 5 } };
  request.symmetry_bits = 0;  // the literal single member: the seconds-scale shape of the arm
  SurveyData document;
  ASSERT_TRUE(AssembleSurveyData(config, request, &document).Ok());
  EXPECT_FALSE(document.early_stopped);
  // The document-level unevaluated list is empty: the full-dimensional arm produced its cards
  // (the rank-0-family and rank-1-family kinds are structurally absent on this route, not
  // uncomputed).
  EXPECT_TRUE(document.unevaluated.empty());
  ASSERT_EQ(document.objects.size(), 3u);
  std::set<std::string> kinds;
  for (const SurveyObject& card : document.objects) {
    kinds.insert(SurveyObjectKindName(card.Kind()));
  }
  EXPECT_EQ(kinds, std::set<std::string>({ "support_set", "caustic_edge", "bright_domain" }));
  // The wide-measure no-change assertion: a full-dimensional document carries no
  // family_routing key (the ladder never runs on the random family — the full-dimensional
  // machine IS the family here).
  EXPECT_FALSE(document.family_routing.has_value());

  // The support_set card: the corpus 450nm partition shape (three intervals, the first closed,
  // the rest open components) with the endpoint mechanism vocabulary.
  const SurveyObject& support = document.objects[0];
  EXPECT_EQ(support.Kind(), SurveyObjectKind::kSupportSet);
  ASSERT_TRUE(support.Support() != nullptr);
  EXPECT_EQ(support.State(), EvidenceState::kFinite);
  ASSERT_EQ(support.Support()->intervals.size(), 3u);
  EXPECT_EQ(support.Support()->intervals[0].n_closed, 1);
  EXPECT_EQ(support.Support()->intervals[1].n_closed, 0);
  EXPECT_EQ(support.Support()->intervals[2].n_closed, 0);
  std::set<std::string> sources;
  for (const SurveyObject::EndpointLabel& label : support.Support()->endpoints) {
    sources.insert(label.source);
  }
  EXPECT_EQ(sources, std::set<std::string>({ "interior_minimum", "boundary_extremum", "corner" }));
  EXPECT_EQ(support.Support()->symmetry_bits, 0u);
  EXPECT_EQ(support.Support()->member_count, 1);

  // The caustic_edge card: the min-deviation fold, certified closed-form on the two-face chain.
  const SurveyObject& edge = document.objects[1];
  EXPECT_EQ(edge.Kind(), SurveyObjectKind::kCausticEdge);
  ASSERT_TRUE(edge.Edge() != nullptr);
  EXPECT_TRUE(edge.Edge()->coincident_with_support);
  EXPECT_EQ(edge.Edge()->profile, "finite_jump");
  ASSERT_EQ(edge.State(), EvidenceState::kCertified);
  EXPECT_EQ(edge.Method(), std::string(kCertifiedMethodSlabMinDeviationClosedForm));
  ASSERT_TRUE(edge.Edge()->certificate.has_value());
  // The corpus anchor: the closed form for the 60-degree prism (face normals 120 degrees apart).
  const double closed_form_deg = 2.0 * std::asin(document.refractive_index * 0.5) * 180.0 / kPi - 60.0;
  const double error = std::fabs(edge.Edge()->position_deg - closed_form_deg);
  EXPECT_LT(error, 1e-4);  // the engine's own cross-check tolerance is 1e-6 rad (~5.7e-5 deg)
  EXPECT_EQ(edge.Edge()->certificate->formula, "2*asin(n*sin(A/2)) - A");
  EXPECT_NEAR(edge.Edge()->certificate->apex_angle_deg, 60.0, 1e-6);
  // The support's lower end is the same edge (the coincidence the corpus lesson turns on).
  EXPECT_NEAR(support.Support()->intervals[0].lower_deg, edge.Edge()->position_deg, 1e-4);

  // The bright_domain card: the span plus the probe leg's readings (the peak is a finite
  // profile quantity, not the certified edge; the readings reference the probes[] entry).
  const SurveyObject& domain = document.objects[2];
  EXPECT_EQ(domain.Kind(), SurveyObjectKind::kBrightDomain);
  ASSERT_TRUE(domain.Bright() != nullptr);
  EXPECT_TRUE(domain.Bright()->has_peak);
  EXPECT_GT(domain.Bright()->peak_value, 0.0);
  EXPECT_TRUE(domain.Bright()->has_edge_reading);
  // The finite edge reading sits in the same bin as the certified position (half a bin).
  EXPECT_NEAR(domain.Bright()->edge_delta_deg, edge.Edge()->position_deg, 0.05);
  EXPECT_GT(domain.Bright()->kept_count, 0);
  EXPECT_EQ(domain.Bright()->probe_index, 0);
  ASSERT_EQ(document.probes.size(), 1u);
  EXPECT_EQ(document.probes.front().kind, SurveyProbeKind::kSolarRadial);
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
  // The arm's own tint leg: the class power-sum shape (the reading is the lattice's weight
  // ratio, not the identity arm's point-mass masses).
  ASSERT_TRUE(document.weight_tint.has_value());
  EXPECT_EQ(document.weight_tint->State(), EvidenceState::kFinite);
  EXPECT_EQ(TintMethodName(document.weight_tint->Value().Method()), std::string(kClassTintMethodLatticePowerSum));
  EXPECT_TRUE(document.weight_tint->Value().IsClassPower());
  EXPECT_GT(document.weight_tint->Value().kept_count_red, 0);
  EXPECT_GT(document.weight_tint->Value().kept_count_blue, 0);
  EXPECT_EQ(document.chromatic.state, EvidenceState::kFinite);
  // The document's own evidence stays finite: the routing refused nothing.
  EXPECT_EQ(document.evidence.state, EvidenceState::kFinite);
}

TEST(SurveyAssembly, StandaloneCrystalNotInAnySceneLayerStillRoutes) {
  ConfigManager config = PlateScene(0.3, 0.3);
  config.scene_.ms_.clear();  // the crystal is configured but allocated to no scattering layer
  SurveyData document;
  ASSERT_TRUE(AssembleSurveyData(config, BaseRequest(), &document).Ok());
  // The standalone branch: no scene layer is named, and the identity says so.
  EXPECT_FALSE(document.scene_layer.has_value());
  EXPECT_EQ(document.scene_identity, std::string("standalone configured crystal (not a scene allocation)"));
  // The routing itself is unchanged: the same plate class collapses and stops.
  EXPECT_TRUE(document.early_stopped);
  ASSERT_EQ(document.objects.size(), 1u);
  EXPECT_EQ(document.objects.front().Kind(), SurveyObjectKind::kSourceImage);
  EXPECT_EQ(document.objects.front().Evidence().state, EvidenceState::kCertified);
  EXPECT_TRUE(document.unevaluated.empty());
}

TEST(SurveyAssembly, RequestRefusalsKeepTheReportErrorSemantics) {
  const ConfigManager config = PlateScene(0.3, 0.3);
  SurveyData document;
  SurveyRequest request = BaseRequest();
  request.crystal_id = 99;
  EXPECT_EQ(AssembleSurveyData(config, request, &document).code, ErrorCode::kUnknownCrystalId);

  request = BaseRequest();
  request.path_layers = { { 1, 2 }, { 1, 2 } };
  EXPECT_EQ(AssembleSurveyData(config, request, &document).code, ErrorCode::kMultiLayerUnsupported);

  request = BaseRequest();
  request.path_layers = { { 1, 99 } };
  EXPECT_EQ(AssembleSurveyData(config, request, &document).code, ErrorCode::kFaceNotInCrystal);

  request = BaseRequest();
  request.path_layers = {};
  EXPECT_EQ(AssembleSurveyData(config, request, &document).code, ErrorCode::kInvalidPath);

  request = BaseRequest();
  request.wavelength_nm = 200.0;
  EXPECT_EQ(AssembleSurveyData(config, request, &document).code, ErrorCode::kWavelengthOutOfRange);

  request = BaseRequest();
  request.budget_ms = kMaxSurveyBudgetMs + 1;
  EXPECT_EQ(AssembleSurveyData(config, request, &document).code, ErrorCode::kInvalidArgument);

  request = BaseRequest();
  request.symmetry_bits = 8;
  EXPECT_EQ(AssembleSurveyData(config, request, &document).code, ErrorCode::kInvalidArgument);
}

TEST(SurveyAssembly, EndToEndJsonCarriesTheSchema4Contract) {
  const ConfigManager config = PlateScene(0.3, 0.3);
  SurveyData document;
  ASSERT_TRUE(AssembleSurveyData(config, BaseRequest(), &document).Ok());
  const auto parsed = nlohmann::json::parse(SurveyResultToJson(SurveyResult(document), "test"));
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

// The dispersion table's followed rows (every onset the kernel's wavelength-critical table
// carried): on the C01 shape the min-deviation row is among them with its own stacking verdict,
// and every row's direction slug is consistent with its own value pair.
TEST(SurveyAssembly, DispersionTableCarriesTheFollowedOnsetRows) {
  const ConfigManager config = RandomPrismScene();
  SurveyRequest request = BaseRequest();
  request.path_layers = { { 3, 5 } };
  request.symmetry_bits = 0;
  SurveyData document;
  ASSERT_TRUE(AssembleSurveyData(config, request, &document).Ok());
  ASSERT_TRUE(document.positional_dispersion.has_value());
  const SurveyPositionalDispersion& dispersion = *document.positional_dispersion;
  EXPECT_TRUE(dispersion.has_min_deviation_row);
  ASSERT_FALSE(dispersion.followed_onsets.empty());
  bool saw_min_row = false;
  for (const SurveyFollowedOnset& row : dispersion.followed_onsets) {
    // Non-fatal per row: one malformed row must not silently skip the rows after it.
    if (row.values_deg.size() != 2u) {
      ADD_FAILURE() << "followed row without exactly two values: " << row.source;
      continue;
    }
    EXPECT_EQ(row.direction, row.values_deg[0] < row.values_deg[1] ? "red_inner_blue_outer" : "blue_inner_red_outer");
    double lo = row.values_deg[0];
    double hi = row.values_deg[1];
    if (hi < lo) {
      std::swap(lo, hi);
    }
    EXPECT_NEAR(row.displacement_deg, hi - lo, 1e-9);
    if (row.source == "interior_minimum" && row.profile == "finite_jump") {
      saw_min_row = true;
      EXPECT_NEAR(row.values_deg[0], dispersion.value_red_deg, 1e-12);
      EXPECT_NEAR(row.values_deg[1], dispersion.value_blue_deg, 1e-12);
    }
  }
  EXPECT_TRUE(saw_min_row);
  const auto parsed = nlohmann::json::parse(SurveyResultToJson(SurveyResult(document), "test"));
  const auto& dispersion_json = parsed.at("chromatic").at("positional_dispersion");
  // The min-row scalars stay in place on a class that has them, and the rows ride as the new key.
  EXPECT_DOUBLE_EQ(dispersion_json.at("value_red_deg").get<double>(), dispersion.value_red_deg);
  ASSERT_TRUE(dispersion_json.contains("followed_onsets"));
  EXPECT_EQ(dispersion_json.at("followed_onsets").size(), dispersion.followed_onsets.size());
}

// The C09 shape: a class whose support ends at an interior maximum (the corpus's 149.0849
// degree finite-jump edge at 550 nm) and whose dispersion table has no min-deviation row —
// the fold lands as a finite caustic_edge card (no two-face closed form, none claimed), and
// the followed rows are the dispersion leg's data.
TEST(SurveyAssembly, MaxFoldClassAssemblesAFiniteCausticCardAndRowOnlyDispersion) {
  const ConfigManager config = AsymmetricPyramidScene();
  SurveyRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 13, 15, 26, 28 } };
  request.symmetry_bits = 0;  // the literal chain, the corpus's filter spelling
  SurveyData document;
  ASSERT_TRUE(AssembleSurveyData(config, request, &document).Ok());
  EXPECT_FALSE(document.early_stopped);
  EXPECT_TRUE(document.unevaluated.empty());
  std::set<std::string> kinds;
  for (const SurveyObject& card : document.objects) {
    kinds.insert(SurveyObjectKindName(card.Kind()));
  }
  EXPECT_EQ(kinds, std::set<std::string>({ "support_set", "caustic_edge", "bright_domain" }));

  // The fold: finite, upper-edge, no certificate.
  const SurveyObject* edge = nullptr;
  const SurveyObject* support = nullptr;
  for (const SurveyObject& card : document.objects) {
    if (card.Kind() == SurveyObjectKind::kCausticEdge) {
      edge = &card;
    }
    if (card.Kind() == SurveyObjectKind::kSupportSet) {
      support = &card;
    }
  }
  ASSERT_NE(edge, nullptr);
  ASSERT_NE(support, nullptr);
  ASSERT_TRUE(edge->Edge() != nullptr);
  EXPECT_EQ(edge->State(), EvidenceState::kFinite);  // no two-face closed form on a 4-face chain
  EXPECT_FALSE(edge->Edge()->certificate.has_value());
  EXPECT_EQ(edge->Edge()->profile, "finite_jump");
  EXPECT_TRUE(edge->Edge()->has_measure_limit);
  EXPECT_TRUE(edge->Edge()->coincident_with_support);
  // The corpus anchor: 149.084904 degrees at n(550) = 1.3110129101 (LI cross-checked to 1.3e-6);
  // the band leaves room for cross-version onset reordering far below any physical ambiguity.
  EXPECT_NEAR(edge->Edge()->position_deg, 149.0849, 0.02);
  ASSERT_FALSE(support->Support()->intervals.empty());
  EXPECT_NEAR(support->Support()->intervals.back().upper_deg, edge->Edge()->position_deg, 0.02);

  // The dispersion: no min row, the rows carry the leg.
  ASSERT_TRUE(document.positional_dispersion.has_value());
  const SurveyPositionalDispersion& dispersion = *document.positional_dispersion;
  EXPECT_FALSE(dispersion.has_min_deviation_row);
  EXPECT_DOUBLE_EQ(dispersion.value_red_deg, 0.0);
  ASSERT_FALSE(dispersion.followed_onsets.empty());
  bool saw_max_row = false;
  for (const SurveyFollowedOnset& row : dispersion.followed_onsets) {
    if (row.source == "interior_maximum") {
      saw_max_row = true;
      // Non-fatal per row (the loop must not skip the rows after a malformed one).
      if (row.values_deg.size() != 2u) {
        ADD_FAILURE() << "max row without exactly two values";
        continue;
      }
      // The corpus spread (400/550/700 nm = 147.820/149.085/149.705): the pair brackets the
      // 550 nm reference position, blue inner, red outer.
      EXPECT_NEAR(row.values_deg[0], edge->Edge()->position_deg, 1.0);
      EXPECT_NEAR(row.values_deg[1], edge->Edge()->position_deg, 1.0);
      EXPECT_GT(row.values_deg[0], row.values_deg[1]);
      EXPECT_EQ(row.direction, "blue_inner_red_outer");
    }
  }
  EXPECT_TRUE(saw_max_row);
  const auto parsed = nlohmann::json::parse(SurveyResultToJson(SurveyResult(document), "test"));
  const auto& dispersion_json = parsed.at("chromatic").at("positional_dispersion");
  // The min-row scalars serialize only when the row exists: keys absent, rows present.
  EXPECT_FALSE(dispersion_json.contains("value_red_deg"));
  EXPECT_FALSE(dispersion_json.contains("direction"));
  EXPECT_TRUE(dispersion_json.contains("followed_onsets"));
  // The chromatic block is finite (a leg produced output) with the class tint's own reading.
  EXPECT_EQ(parsed.at("chromatic").at("state"), "finite");
  const auto& tint_json = parsed.at("chromatic").at("weight_tint");
  EXPECT_EQ(tint_json.at("evidence").at("state"), "finite");
  EXPECT_EQ(tint_json.at("method"), "lattice_power_sum");
  EXPECT_GT(tint_json.at("ratio").get<double>(), 0.0);
  // The caliber names the class power-sum reading, not the point-mass mass ratio.
  EXPECT_NE(tint_json.at("caliber").get<std::string>().find("class power-sum"), std::string::npos);
  EXPECT_FALSE(tint_json.contains("m_red"));  // the mass family is the point-mass shape's
  EXPECT_TRUE(tint_json.contains("energy_red"));
}

}  // namespace
TEST(SurveyAssembly, PublishedProbeCarriesItsQuantityAndResolvedScope) {
  for (int scenario = 0; scenario < 2; ++scenario) {
    SCOPED_TRACE(scenario);
    const auto check = [scenario] {
      const ConfigManager config = scenario == 0 ? RandomPrismScene() : PlateScene(0.3, 0.3);
      SurveyRequest request = BaseRequest();
      request.path_layers = { { 3, 5 } };
      request.symmetry_bits = 0;
      SurveyResult result;
      ASSERT_TRUE(AssembleSurvey(config, request, &result).Ok());
      const SurveyData& data = result.Data();
      ASSERT_EQ(data.probes.size(), 1u);
      const SurveyProbe& probe = data.probes.front();
      ASSERT_EQ(probe.State(), EvidenceState::kFinite);
      const ClaimScope& scope = probe.Scope();
      ASSERT_TRUE(scope.quantity);
      EXPECT_EQ(scope.quantity->kind, QuantityKind::kBandSum);
      EXPECT_EQ(scope.quantity->measure, probe.spec.quantity.measure);
      EXPECT_EQ(scope.measure.kind, probe.spec.quantity.measure);
      EXPECT_EQ(scope.quantity->context, data.quantity_context);
      EXPECT_EQ(scope.input, data.quantity_context);
      EXPECT_EQ(scope.members.members, (std::vector<std::vector<int>>{ probe.spec.representative }));
      EXPECT_EQ(scope.members.location, data.quantity_context->members.location);
      ASSERT_TRUE(scope.observation.wavelength_nm);
      EXPECT_DOUBLE_EQ(*scope.observation.wavelength_nm, probe.spec.wavelength_nm);
    };
    check();
  }
}

TEST(SurveyAssembly, PublishedResultOwnsItsScopeAndRefusedQuantity) {
  SurveyResult result;
  {
    ConfigManager config = FixedPosePlateScene(1.5);
    SurveyRequest request;
    request.crystal_id = 1;
    request.path_layers = { { 1, 2 } };
    ASSERT_TRUE(AnalyzePathSurveyValue(config, request, &result).Ok());
  }
  const auto& data = result.Data();
  ASSERT_EQ(data.objects.size(), 1u);
  const auto ref = result.Reference(ObjectId{ 0 });
  ASSERT_TRUE(ref);
  const auto members = result.ObjectMembers(*ref);
  ASSERT_TRUE(members);
  ASSERT_FALSE(members->empty());
  ASSERT_NE(result.Resolve(members->front()), nullptr);
  EXPECT_EQ(*result.Resolve(members->front()), (std::vector<int>{ 1, 2 }));
  ASSERT_TRUE(data.quantity_context);
  EXPECT_EQ(data.quantity_context->crystal.id_, 1);
  ASSERT_TRUE(data.weight_tint);
  EXPECT_EQ(data.weight_tint->State(), EvidenceState::kRefused);
  EXPECT_EQ(data.weight_tint->ValueOrNull(), nullptr);
  ASSERT_TRUE(data.weight_tint->Scope().quantity);
  EXPECT_EQ(data.weight_tint->Scope().quantity->kind, QuantityKind::kTintRatio);
  EXPECT_EQ(data.weight_tint->Scope().quantity->method, QuantityMethod::kPointMassBandSum);
  EXPECT_EQ(data.weight_tint->Scope().input, data.quantity_context);
}

TEST(SurveyAssembly, CompletionBindsTheDeclaredScopeAndRejectsDanglingReferences) {
  const ConfigManager config = RandomPrismScene();
  SurveyRequest request = BaseRequest();
  request.path_layers = { { 3, 5 } };
  request.symmetry_bits = 0;
  SurveyData document;
  ASSERT_TRUE(AssembleSurveyData(config, request, &document).Ok());

  // Completion binds the claim's declared scope from the document's own facts, on every
  // construction path: the finite probe's scope resolves quantity, representative and observation.
  const survey::SurveyResult result(document);
  ASSERT_FALSE(result.Data().probes.empty());
  const SurveyProbe& probe = result.Data().probes.front();
  ASSERT_TRUE(probe.Scope().quantity.has_value());
  EXPECT_EQ(probe.Scope().members.members, std::vector<std::vector<int>>{ probe.spec.representative });
  ASSERT_TRUE(probe.Scope().observation.wavelength_nm.has_value());
  EXPECT_DOUBLE_EQ(*probe.Scope().observation.wavelength_nm, probe.spec.wavelength_nm);

  // A bright domain may not reference a probe outside the result.
  SurveyData dangling = document;
  bool rewired = false;
  for (SurveyObject& object : dangling.objects) {
    if (auto* bright = object.Bright()) {
      bright->probe_index = static_cast<int>(dangling.probes.size()) + 3;
      rewired = true;
    }
  }
  ASSERT_TRUE(rewired);
  EXPECT_THROW(survey::SurveyResult{ dangling }, std::invalid_argument);

  // An object may not reference a member outside its own class.
  SurveyData foreign = document;
  ASSERT_FALSE(foreign.objects.empty());
  foreign.objects.front().members = { { 9, 9, 9 } };
  EXPECT_THROW(survey::SurveyResult{ foreign }, std::invalid_argument);
}

}  // namespace lumice::raypath::survey
