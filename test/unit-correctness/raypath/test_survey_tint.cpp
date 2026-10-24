#include <gtest/gtest.h>

#include <cmath>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "core/optics.hpp"  // IceRefractiveIndex: the dispersion table the pair reads
#include "raypath/detail/survey/survey_assembly.hpp"
#include "raypath/detail/survey/survey_json.hpp"
#include "raypath/detail/survey/survey_tint.hpp"
#include "raypath/scene_to_analytic.hpp"
#include "survey_test_fixtures.hpp"

namespace lumice::raypath::survey {
namespace {

// A rank-0 plate path's BandSum at an empty pixel table: the point-mass branch fills m, its
// error and its method, places nothing, and keeps the kept-event count — the contract the tint
// leg's ratio reads. This is the kernel-side pin; the leg-level tests below consume it.
TEST(SurveyTint, BandSumAtEmptyPixelTableTakesThePointMassBranch) {
  const ConfigManager config = PlateScene(1.5, 0.3);
  const auto configured = config.crystals_.find(1);
  const CrystalConversion crystal = ConvertCrystal(configured->second.param_);
  analytic::FaceNormalTable table;
  analytic::FacePolygonTable polygons;
  ASSERT_EQ(analytic::BuildFaceNormals(crystal.shape, &table, &polygons), analytic::Status::kOk);
  std::vector<int> slots;
  ASSERT_TRUE(ResolveSingleLayerPath({ { 1, 3, 6, 2 } }, table, &slots).Ok());
  const PoseDensityConversion density = ConvertAxisToPoseDensity(configured->second.axis_);
  ASSERT_TRUE(density.Ok());
  EXPECT_EQ(density.spec.family, analytic::PoseFamily::kPlate);  // the wobble plate is a plate

  double incident[3];
  SunIncidentDirection(config.scene_.light_source_.param_, incident);
  const analytic::PoseDensity pose_density(density.spec);
  const analytic::PixelTable empty_table;
  const analytic::BandSumOutput band =
      analytic::BandSum(table, polygons, slots.data(), static_cast<int>(slots.size()), IceRefractiveIndex::Get(450.0),
                        incident, 100000, empty_table, pose_density);
  EXPECT_TRUE(band.rank_zero);
  EXPECT_TRUE(band.pixels.empty());
  EXPECT_EQ(band.point_mass_pixel, -1);
  EXPECT_GT(band.m, 0.0);
  EXPECT_EQ(band.method, analytic::PointMassMethod::kPsiAverage);  // a non-uniform density
  EXPECT_GT(band.kept_count, 0);
}

namespace {

// The tint leg's inputs for the C15-shaped plate (the corpus numbers without the corpus file).
struct TintFixture {
  analytic::FaceNormalTable table;
  analytic::FacePolygonTable polygons;
  PoseDensityConversion density;
  std::vector<int> representative{ 1, 3, 6, 2 };
  double incident[3]{};

  static TintFixture Make() {
    const ConfigManager config = PlateScene(1.5, 0.3);
    const auto configured = config.crystals_.find(1);
    TintFixture fix;
    const CrystalConversion crystal = ConvertCrystal(configured->second.param_);
    EXPECT_EQ(analytic::BuildFaceNormals(crystal.shape, &fix.table, &fix.polygons), analytic::Status::kOk);
    fix.density = ConvertAxisToPoseDensity(configured->second.axis_);
    SunIncidentDirection(config.scene_.light_source_.param_, fix.incident);
    return fix;
  }

  quantity::TintInput Input(int sample_count) const {
    quantity::TintInput input;
    input.table = &table;
    input.polygons = &polygons;
    input.representative = &representative;
    input.density = &density;
    for (int k = 0; k < 3; k++) {
      input.incident[k] = incident[k];
    }
    input.member_count = 6;  // the corpus class's size (the bench pins the enumeration itself)
    input.sample_count = sample_count;
    return input;
  }
};

}  // namespace

// The mechanism pin at a fast lattice: the ratio is the blue mass over the red one, the method
// is the deterministic psi average, and the direction is the corpus's blue tilt (the two
// parallel side-face reflections square a Fresnel R that grows toward TIR at blue).
TEST(SurveyTint, IdentityArmTintReadsBlueOverRedWithThePsiAverageMethod) {
  const TintFixture fix = TintFixture::Make();
  SurveyData document;
  AssembleWeightTint(fix.Input(200000), &document);
  ASSERT_TRUE(document.weight_tint.has_value());
  const WeightTintResult& tint = *document.weight_tint;
  EXPECT_EQ(tint.State(), EvidenceState::kFinite);
  EXPECT_EQ(tint.Value().Method(), QuantityMethod::kPointMassPsiAverage);
  EXPECT_GT(tint.Value().Mass().blue, 0.0);
  EXPECT_GT(tint.Value().Mass().red, 0.0);
  EXPECT_DOUBLE_EQ(tint.Value().ratio, tint.Value().Mass().blue / tint.Value().Mass().red);
  EXPECT_GT(tint.Value().ratio, 1.2);
  EXPECT_EQ(tint.Value().direction, TintDirection::kBlueTinted);
  EXPECT_EQ(tint.Value().n_blue, IceRefractiveIndex::Get(450.0));
  EXPECT_EQ(tint.Value().n_red, IceRefractiveIndex::Get(650.0));
  EXPECT_EQ(tint.Value().sample_count, 200000);
  EXPECT_GT(tint.Value().kept_count_red, 0);
  EXPECT_GT(tint.Value().kept_count_blue, 0);  // the existence witness, both lambdas
  // The block mirrors its only leg (the identity arm's block-state rule).
  EXPECT_EQ(document.chromatic.state, EvidenceState::kFinite);
  EXPECT_EQ(document.chromatic.coverage.members_evaluated, 6);
}

// Determinism: the leg is a deterministic quadrature — two runs, same bytes of ratio.
TEST(SurveyTint, TwoIdenticalRunsProduceTheSameRatio) {
  const TintFixture fix = TintFixture::Make();
  SurveyData first;
  AssembleWeightTint(fix.Input(100000), &first);
  SurveyData second;
  AssembleWeightTint(fix.Input(100000), &second);
  ASSERT_TRUE(first.weight_tint.has_value());
  ASSERT_TRUE(second.weight_tint.has_value());
  EXPECT_EQ(first.weight_tint->Value().ratio, second.weight_tint->Value().ratio);
  EXPECT_EQ(first.weight_tint->Value().Mass().red, second.weight_tint->Value().Mass().red);
  EXPECT_EQ(first.weight_tint->Value().Mass().blue, second.weight_tint->Value().Mass().blue);
}

// The honest refusal: a fixed-pose axis maps to no pose density, and the leg refuses instead of
// approximating. The identity arm still certifies the source image — the refusal is the tint's.
TEST(SurveyTint, FixedPoseAxisRefusesTheTintButNotTheCertificate) {
  const ConfigManager config = FixedPosePlateScene(1.5);
  SurveyRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 1, 3, 6, 2 } };
  SurveyData document;
  ASSERT_TRUE(AssembleSurveyData(config, request, &document).Ok());
  EXPECT_TRUE(document.early_stopped);
  ASSERT_EQ(document.objects.size(), 1u);
  EXPECT_EQ(document.objects.front().Evidence().state, EvidenceState::kCertified);
  ASSERT_TRUE(document.weight_tint.has_value());
  EXPECT_EQ(document.weight_tint->State(), EvidenceState::kRefused);
  EXPECT_EQ(document.weight_tint->Mode(), "density_not_expressible");
  // The block mirrors the refusal; no numeric field is written.
  EXPECT_EQ(document.chromatic.state, EvidenceState::kRefused);
  EXPECT_EQ(document.chromatic.mode, "density_not_expressible");
  EXPECT_EQ(document.weight_tint->ValueOrNull(), nullptr);
}

// The rank-disagreement refusal: a deviating sequence on the same plate ({3,5}: side face in,
// side face out — a deflection, not an identity) fails the kernel's own rank check, and the leg
// refuses rather than reading a map as a mass. Direct call, per the refusal being the leg's own.
TEST(SurveyTint, DeviatingSequenceRefusesWithNotRankZero) {
  const TintFixture fix = TintFixture::Make();
  const std::vector<int> deviating{ 3, 5 };
  quantity::TintInput input = fix.Input(100000);
  input.representative = &deviating;
  SurveyData document;
  AssembleWeightTint(input, &document);
  ASSERT_TRUE(document.weight_tint.has_value());
  EXPECT_EQ(document.weight_tint->State(), EvidenceState::kRefused);
  EXPECT_EQ(document.weight_tint->Mode(), "not_rank_zero");
  EXPECT_EQ(document.chromatic.state, EvidenceState::kRefused);
  EXPECT_EQ(document.chromatic.mode, "not_rank_zero");
  EXPECT_EQ(document.weight_tint->ValueOrNull(), nullptr);
}

// The member-resolution negative control: a representative naming a face the crystal lacks
// cannot resolve to slots, and the leg refuses with the same shape as every other refusal —
// mode "member_resolution", no numeric field written, the chromatic block mirroring it.
TEST(SurveyTint, UnresolvableRepresentativeRefusesWithMemberResolution) {
  const TintFixture fix = TintFixture::Make();
  const std::vector<int> unresolvable{ 99, 99 };
  quantity::TintInput input = fix.Input(100000);
  input.representative = &unresolvable;
  SurveyData document;
  AssembleWeightTint(input, &document);
  ASSERT_TRUE(document.weight_tint.has_value());
  EXPECT_EQ(document.weight_tint->State(), EvidenceState::kRefused);
  EXPECT_EQ(document.weight_tint->Mode(), "member_resolution");
  EXPECT_EQ(document.weight_tint->ValueOrNull(), nullptr);
  EXPECT_EQ(document.chromatic.state, EvidenceState::kRefused);
  EXPECT_EQ(document.chromatic.mode, "member_resolution");
}

// The identity arm's serialized shape: the chromatic sub-blocks a point-mass path does not have
// stay absent, and the early-stop document carries no probes key.
TEST(SurveyTint, IdentityDocumentCarriesNoPositionalDispersionOrDiagnose) {
  const ConfigManager config = PlateScene(1.5, 0.3);
  SurveyRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 1, 3, 6, 2 } };
  SurveyData document;
  ASSERT_TRUE(AssembleSurveyData(config, request, &document).Ok());
  const auto parsed = nlohmann::json::parse(SurveyResultToJson(SurveyResult(document), "test"));
  const auto& chromatic = parsed.at("chromatic");
  EXPECT_EQ(chromatic.at("state"), "finite");
  EXPECT_FALSE(chromatic.contains("positional_dispersion"));
  EXPECT_FALSE(chromatic.contains("diagnose"));
  ASSERT_TRUE(chromatic.contains("weight_tint"));
  EXPECT_EQ(chromatic.at("weight_tint").at("evidence").at("state"), "finite");
  EXPECT_EQ(chromatic.at("weight_tint").at("direction"), "blue_tinted");
  EXPECT_EQ(chromatic.at("weight_tint").at("method"), "psi_average");
  EXPECT_EQ(chromatic.at("weight_tint").at("lattice"), "antipodal_fibonacci");
  EXPECT_EQ(chromatic.at("weight_tint").at("deterministic"), true);
  EXPECT_EQ(chromatic.at("weight_tint").at("member_count"), static_cast<int>(parsed.at("scope").at("members").size()));
  EXPECT_FALSE(parsed.contains("probes"));  // an early stop produces no probes
}

}  // namespace
}  // namespace lumice::raypath::survey
