#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "raypath/detail/input_assembly.hpp"
#include "raypath/detail/survey/survey_assembly.hpp"
#include "raypath/detail/survey/survey_mc_ess.hpp"
#include "raypath/scene_to_analytic.hpp"
#include "raypath/survey_report.hpp"
#include "survey_test_fixtures.hpp"

namespace lumice::raypath::survey {
namespace {

// The ESS leg's input for the corpus-shaped prism: the snapshot the survey assembly itself
// captures (the same CaptureInput call AssembleSurvey makes), at the scene's default lambda.
struct McEssFixture {
  InputSnapshot snapshot;
  double wavelength_nm = 0.0;
  double sun_dir[3]{};
  int member_count = 0;

  static McEssFixture MakeFrom(const ConfigManager& config, double lambda_nm) {
    const auto configured = config.crystals_.find(1);
    McEssFixture fix;
    fix.wavelength_nm = lambda_nm;
    std::vector<int> faces{ 3, 5, 7 };
    const std::vector<LayerSelection> selection{ { 0, 1, faces, 7 } };
    EXPECT_TRUE(CaptureInput(config.scene_, "test snapshot", selection, &fix.snapshot).Ok());
    double sun_hat[3];
    SunIncidentDirection(config.scene_.light_source_.param_, sun_hat);
    for (int k = 0; k < 3; k++) {
      fix.sun_dir[k] = -sun_hat[k];
    }
    fix.member_count = 12;  // the corpus class's size (the bench pins the enumeration itself)
    return fix;
  }

  static McEssFixture Make() {
    const ConfigManager config = RandomPrismScene();
    WavelengthChoice wavelength;
    EXPECT_TRUE(ResolveWavelength(config.scene_.light_source_, std::nullopt, &wavelength).Ok());
    return MakeFrom(config, wavelength.wavelength_nm);
  }

  quantity::McRegionEssInput Input(long long draws) const {
    quantity::McRegionEssInput input;
    input.snapshot = &snapshot;
    input.wavelength_nm = wavelength_nm;
    for (int k = 0; k < 3; k++) {
      input.sun_dir[k] = sun_dir[k];
    }
    input.member_count = member_count;
    input.draws = draws;
    input.budget_ms = 120000;
    return input;
  }
};

// The leg's positive shape: the full band grid echoed, every hard Kish bound held where draws
// landed, the request echoed, and the deterministic flag false (the field that states the leg
// against the deterministic legs' true).
TEST(McRegionEssResult, PositiveShapeEchoesTheRequestAndHoldsTheKishBounds) {
  const McEssFixture fix = McEssFixture::Make();
  const long long draws = 2000;
  SurveyData document;
  AssembleMcRegionEss(fix.Input(draws), &document);
  ASSERT_TRUE(document.mc_region_ess.has_value());
  const McRegionEssResult& ess = *document.mc_region_ess;
  ASSERT_EQ(ess.State(), EvidenceState::kFinite);
  EXPECT_EQ(ess.Value().quantity.shape, GeometryKind::kDeclaredPopulation);
  EXPECT_NE(ess.Scope().geometry.kind, GeometryKind::kNominal);
  EXPECT_EQ(ess.Scope().geometry.kind, ess.Value().quantity.shape);
  EXPECT_EQ(ess.Value().seed, quantity::kSurveyMcEssSeed);
  EXPECT_EQ(ess.Value().draws, draws);
  EXPECT_DOUBLE_EQ(ess.Value().lambda_nm, fix.wavelength_nm);
  EXPECT_DOUBLE_EQ(ess.Value().band_deg, 5.0);
  ASSERT_EQ(ess.Value().bands.size(), static_cast<size_t>(quantity::kSurveyMcEssBandCount));
  EXPECT_EQ(ess.Value().bands.front().delta_min_deg, 0.0);
  EXPECT_EQ(ess.Value().bands.back().delta_max_deg, 180.0);
  EXPECT_FALSE(ess.Value().deterministic);
  long long total_draw_hits = 0;
  for (const McRegionEssBand& band : ess.Value().bands) {
    EXPECT_GE(band.ess, 0.0);
    EXPECT_LE(band.ess, static_cast<double>(draws));  // the Kish hard bound
    EXPECT_GE(band.draw_count, 0);
    EXPECT_GE(band.sum_weight, 0.0);
    total_draw_hits += band.draw_count;
  }
  EXPECT_GT(total_draw_hits, 0);  // the measure carries light somewhere on this path
}

// Reproducibility: the leg is seeded and prefix-replay reproducible — two runs agree to the
// last bit on every band's numbers (deterministic=false is a shape statement, not a wobble).
TEST(McRegionEssResult, SameSeedTwoRunsAgreeBitwise) {
  const McEssFixture fix = McEssFixture::Make();
  SurveyData first;
  AssembleMcRegionEss(fix.Input(500), &first);
  SurveyData second;
  AssembleMcRegionEss(fix.Input(500), &second);
  ASSERT_TRUE(first.mc_region_ess.has_value());
  ASSERT_TRUE(second.mc_region_ess.has_value());
  ASSERT_EQ(first.mc_region_ess->State(), EvidenceState::kFinite);
  ASSERT_EQ(second.mc_region_ess->State(), EvidenceState::kFinite);
  for (size_t i = 0; i < first.mc_region_ess->Value().bands.size(); i++) {
    EXPECT_EQ(first.mc_region_ess->Value().bands[i].ess, second.mc_region_ess->Value().bands[i].ess);
    EXPECT_EQ(first.mc_region_ess->Value().bands[i].draw_count, second.mc_region_ess->Value().bands[i].draw_count);
    EXPECT_EQ(first.mc_region_ess->Value().bands[i].sum_weight, second.mc_region_ess->Value().bands[i].sum_weight);
  }
}

// The assembly's opt-in discipline: without the flag the document carries no key at all (the
// existing documents' zero-evolution guarantee), with it the key appears with the echoes.
TEST(McRegionEssResult, AssemblyCarriesTheKeyOnlyWhenRequested) {
  const ConfigManager config = RandomPrismScene();
  SurveyRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 5 } };
  request.symmetry_bits = 0;
  SurveyData document;
  ASSERT_TRUE(AssembleSurveyData(config, request, &document).Ok());
  EXPECT_FALSE(document.mc_region_ess.has_value());  // the flag did not ask

  request.mc_ess_draws = 500;
  SurveyResult published;
  ASSERT_TRUE(AnalyzePathSurveyValue(config, request, &published).Ok());
  const auto& with_leg = published.Data();
  ASSERT_TRUE(with_leg.mc_region_ess.has_value());
  EXPECT_EQ(with_leg.mc_region_ess->State(), EvidenceState::kFinite);
  EXPECT_EQ(with_leg.mc_region_ess->Value().draws, 500);
  EXPECT_NE(with_leg.mc_region_ess->Scope().geometry.kind, GeometryKind::kNominal);
  EXPECT_EQ(with_leg.mc_region_ess->Scope().geometry.kind, GeometryKind::kDeclaredPopulation);
  // The note names the leg it ran.
  EXPECT_NE(with_leg.budget_note.find("MC region ESS"), std::string::npos);
}

// The request boundary: a draw count past the cap refuses at the engine's front door, before
// any sampling work.
TEST(McRegionEssResult, DrawsPastTheCapRefuseAtTheAssembly) {
  const ConfigManager config = RandomPrismScene();
  SurveyRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 5 } };
  request.symmetry_bits = 0;
  request.mc_ess_draws = quantity::kSurveyMcEssMaxDraws + 1;
  SurveyData document;
  const Error error = AssembleSurveyData(config, request, &document);
  EXPECT_FALSE(error.Ok());
  EXPECT_EQ(error.code, ErrorCode::kInvalidArgument);
}

// The honesty note on the refused path: a leg that refused names the refusal — it never claims
// the draws it did not run (the note and the refused block must tell the same story).
TEST(McRegionEssResult, RefusedLegNoteNamesTheRefusalNotARun) {
  const ConfigManager config = RandomPrismScene();
  SurveyRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 5 } };
  request.symmetry_bits = 0;
  request.mc_ess_draws = quantity::kSurveyMcEssMaxDraws;
  request.budget_ms = 1;  // the deadline hits long before the requested draws can complete
  SurveyData document;
  ASSERT_TRUE(AssembleSurveyData(config, request, &document).Ok());
  ASSERT_TRUE(document.mc_region_ess.has_value());
  EXPECT_EQ(document.mc_region_ess->State(), EvidenceState::kRefused);
  EXPECT_EQ(document.mc_region_ess->Mode(), "budget_exhausted");
  EXPECT_NE(document.budget_note.find("MC region ESS"), std::string::npos);
  EXPECT_NE(document.budget_note.find("refused"), std::string::npos);
  EXPECT_EQ(document.budget_note.find("seeded draws"), std::string::npos);
}

// The discrete-light slot match (the sampler needs a slot AT the reference lambda): a lambda
// equal to a slot's — float-exact, the matcher compares floats — builds and runs finite; one
// between slots refuses as sampler_unavailable, the documented contract, not a defaulted
// sample.
TEST(McRegionEssResult, DiscreteLightSlotHitRunsFinite) {
  const McEssFixture fix = McEssFixture::MakeFrom(DiscreteSlotScene(), 550.0);
  SurveyData document;
  AssembleMcRegionEss(fix.Input(500), &document);
  ASSERT_TRUE(document.mc_region_ess.has_value());
  EXPECT_EQ(document.mc_region_ess->State(), EvidenceState::kFinite);
  EXPECT_EQ(document.mc_region_ess->Value().draws, 500);
}

TEST(McRegionEssResult, DiscreteLightSlotMissRefusesSamplerUnavailable) {
  const McEssFixture fix = McEssFixture::MakeFrom(DiscreteSlotScene(), 600.0);
  SurveyData document;
  AssembleMcRegionEss(fix.Input(500), &document);
  ASSERT_TRUE(document.mc_region_ess.has_value());
  EXPECT_EQ(document.mc_region_ess->State(), EvidenceState::kRefused);
  EXPECT_EQ(document.mc_region_ess->Mode(), "sampler_unavailable");
}

// The serialized shape on a real document: the key list matches the planned schema-4 append
// exactly, and the block sits at the document's top level.
TEST(McRegionEssResult, SerializedKeySetMatchesThePlannedAppend) {
  const McEssFixture fix = McEssFixture::Make();
  SurveyData document;
  AssembleMcRegionEss(fix.Input(200), &document);
  const auto parsed = nlohmann::json::parse(SurveyResultToJson(SurveyResult(document), "test"));
  ASSERT_TRUE(parsed.contains("mc_region_ess"));
  const auto& block = parsed.at("mc_region_ess");
  EXPECT_EQ(block.at("seed"), quantity::kSurveyMcEssSeed);
  EXPECT_EQ(block.at("deterministic"), false);
  EXPECT_TRUE(block.contains("evidence"));
  EXPECT_EQ(block.at("evidence").at("state"), "finite");
  EXPECT_TRUE(block.at("evidence").contains("coverage"));
  EXPECT_TRUE(block.contains("bands"));
  EXPECT_EQ(block.at("bands").size(), 36u);
}

}  // namespace
}  // namespace lumice::raypath::survey
