#include <gtest/gtest.h>

#include <cmath>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <vector>

#include "core/optics.hpp"  // IceRefractiveIndex: the dispersion table the fixture reads
#include "raypath/detail/survey/survey_assembly.hpp"
#include "raypath/detail/survey/survey_json.hpp"
#include "raypath/detail/survey/survey_witness.hpp"
#include "raypath/scene_to_analytic.hpp"
#include "survey_test_fixtures.hpp"

namespace lumice::raypath::survey {
namespace {

// The witness leg's inputs for the C17-shaped regular prism (the corpus shape without the
// corpus file): full-sphere uniform axis, sun at 15 degrees, the touching 3-1-5-7 chain.
struct WitnessFixture {
  analytic::FaceNormalTable table;
  analytic::FacePolygonTable polygons;
  analytic::CrystalShape crystal;
  PoseDensityConversion density;
  std::vector<int> representative{ 3, 1, 5, 7 };
  double incident[3]{};
  double refractive_index = 0.0;

  static WitnessFixture Make() {
    const ConfigManager config = RegularPrismSceneWithSun(15.0);
    const auto configured = config.crystals_.find(1);
    WitnessFixture fix;
    const CrystalConversion conversion = ConvertCrystal(configured->second.param_);
    EXPECT_EQ(analytic::BuildFaceNormals(conversion.shape, &fix.table, &fix.polygons), analytic::Status::kOk);
    fix.crystal = conversion.shape;
    fix.density = ConvertAxisToPoseDensity(configured->second.axis_);
    EXPECT_TRUE(fix.density.Ok());
    std::vector<int> slots;
    EXPECT_TRUE(ResolveSingleLayerPath({ { 3, 1, 5, 7 } }, fix.table, &slots).Ok());
    SunIncidentDirection(config.scene_.light_source_.param_, fix.incident);
    fix.refractive_index = IceRefractiveIndex::Get(550.0);
    return fix;
  }

  WitnessInput Input(int sample_count) const {
    WitnessInput input;
    input.table = &table;
    input.polygons = &polygons;
    input.crystal = &crystal;
    input.representative = &representative;
    for (int k = 0; k < 3; k++) {
      input.incident[k] = incident[k];
    }
    input.refractive_index = refractive_index;
    input.wavelength_nm = 550.0;
    input.member_count = 24;  // the corpus class's size (the bench pins the enumeration itself)
    input.sample_count = sample_count;
    return input;
  }
};

// The existence ladder: kept_count > 0 at every rung (the witness is an existence question, so
// the rungs agree in sign); the pinned value is the largest rung, still sub-second (the wall
// times are measured in the task's progress record, not asserted here — a timing assert would
// measure the machine, not the leg).
TEST(SurveyWitness, ExistenceLadderKeepsPositiveCountsAtEveryRung) {
  const WitnessFixture fix = WitnessFixture::Make();
  for (const int n : { 100000, 200000, 500000 }) {
    // Non-fatal per rung: each rung is an independent measurement; one bad rung must not
    // silently skip the others.
    SurveyObject card;
    AssembleWitness(fix.Input(n), &card);
    EXPECT_TRUE(card.witness.has_value());
    if (!card.witness.has_value()) {
      continue;
    }
    EXPECT_EQ(card.witness->state, EvidenceState::kFinite);
    EXPECT_GT(card.witness->kept_count, 0);
    EXPECT_EQ(card.witness->sample_count, n);
  }
}

// The positive shape: the argmax event's readings are all positive, the provenance keys echo
// the caliber, and the source note is spelled inside the document.
TEST(SurveyWitness, PositiveWitnessCarriesTheFullReadingSet) {
  const WitnessFixture fix = WitnessFixture::Make();
  SurveyObject card;
  AssembleWitness(fix.Input(100000), &card);
  ASSERT_TRUE(card.witness.has_value());
  const SurveyObject::WitnessPayload& witness = *card.witness;
  EXPECT_EQ(witness.state, EvidenceState::kFinite);
  EXPECT_GT(witness.kept_count, 0);
  EXPECT_GT(witness.weight, 0.0);
  EXPECT_GT(witness.entry, 0.0);
  EXPECT_GT(witness.interface_product, 0.0);
  EXPECT_TRUE(std::isfinite(witness.deviation_deg));
  EXPECT_EQ(witness.sample_count, 100000);
  EXPECT_EQ(witness.lattice, "antipodal_fibonacci");
  EXPECT_TRUE(witness.deterministic);
  EXPECT_FALSE(witness.source_note.empty());
  EXPECT_EQ(witness.coverage.members_evaluated, 1);
  EXPECT_DOUBLE_EQ(witness.coverage.wavelength_nm, 550.0);
}

// The negative control: a representative naming a face the crystal lacks refuses with the same
// defensive mode as the sibling legs, and no numeric field is written.
TEST(SurveyWitness, UnresolvableRepresentativeRefusesWithMemberResolution) {
  const WitnessFixture fix = WitnessFixture::Make();
  const std::vector<int> unresolvable{ 99, 99 };
  WitnessInput input = fix.Input(100000);
  input.representative = &unresolvable;
  SurveyObject card;
  AssembleWitness(input, &card);
  ASSERT_TRUE(card.witness.has_value());
  EXPECT_EQ(card.witness->state, EvidenceState::kRefused);
  EXPECT_EQ(card.witness->mode, "member_resolution");
  EXPECT_EQ(card.witness->kept_count, 0);
  EXPECT_DOUBLE_EQ(card.witness->weight, 0.0);
}

// The coexistence at full scale: the C17-shaped class's partition refuses (the corpus's
// slab_crease_touching), and the SAME refused support card carries the finite witness — the
// card-level evidence and the witness sub-block never merge, and the chromatic legs say they
// were not run instead of the stale v1 reason.
TEST(SurveyWitness, RefusedSupportCardCarriesTheFiniteWitness) {
  const ConfigManager config = RegularPrismSceneWithSun(15.0);
  SurveyRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 1, 5, 7 } };
  request.symmetry_bits = 0;  // the literal single member: the seconds-scale shape
  SurveyResult document;
  ASSERT_TRUE(AssembleSurvey(config, request, &document).Ok());
  EXPECT_FALSE(document.early_stopped);
  ASSERT_EQ(document.objects.size(), 1u);
  const SurveyObject& card = document.objects.front();
  EXPECT_EQ(card.kind, SurveyObjectKind::kSupportSet);
  // The refused card's own evidence stays refused; the witness sub-block is finite on it.
  EXPECT_EQ(card.evidence.state, EvidenceState::kRefused);
  EXPECT_EQ(card.evidence.mode, "slab_crease_touching");
  ASSERT_TRUE(card.witness.has_value());
  EXPECT_EQ(card.witness->state, EvidenceState::kFinite);
  EXPECT_GT(card.witness->kept_count, 0);
  EXPECT_GT(card.witness->entry, 0.0);
  EXPECT_GT(card.witness->interface_product, 0.0);
  // The document-level evidence carries the refusal; the chromatic legs name the axis refusal.
  EXPECT_EQ(document.evidence.state, EvidenceState::kRefused);
  EXPECT_EQ(document.evidence.mode, "slab_crease_touching");
  EXPECT_EQ(document.chromatic.state, EvidenceState::kUnevaluated);
  EXPECT_NE(document.chromatic.reason.find("slab_crease_touching"), std::string::npos);
  ASSERT_TRUE(document.weight_tint.has_value());
  EXPECT_EQ(document.weight_tint->evidence.state, EvidenceState::kUnevaluated);
  EXPECT_EQ(document.weight_tint->evidence.reason, document.chromatic.reason);
  EXPECT_FALSE(document.positional_dispersion.has_value());
  EXPECT_FALSE(document.diagnose.has_value());
  EXPECT_TRUE(document.probes.empty());
  // The arm's budget note names its own legs, not the early-stop arm's claim.
  EXPECT_NE(document.budget_note.find("witness"), std::string::npos);
}

// The serialized faces: the refused witness is exactly {state, mode}; the finite witness on the
// refused card carries the full field set; a full-dimensional route's cards carry no witness at
// all (the payload appears only where its leg ran).
TEST(SurveyWitness, SerializedWitnessShapesFollowThePayloadRegistry) {
  const ConfigManager config = RegularPrismSceneWithSun(15.0);
  SurveyRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 1, 5, 7 } };
  request.symmetry_bits = 0;
  SurveyResult document;
  ASSERT_TRUE(AssembleSurvey(config, request, &document).Ok());
  const auto parsed = nlohmann::json::parse(SurveyResultToJson(document, "test"));
  const auto& witness = parsed.at("objects")[0].at("witness");
  EXPECT_EQ(witness.at("state"), "finite");
  EXPECT_GT(witness.at("kept_count").get<long long>(), 0);
  EXPECT_EQ(witness.at("lattice"), "antipodal_fibonacci");
  EXPECT_EQ(witness.at("deterministic"), true);
  EXPECT_GT(witness.at("weight").get<double>(), 0.0);
  EXPECT_GT(witness.at("entry").get<double>(), 0.0);
  EXPECT_GT(witness.at("interface_product").get<double>(), 0.0);
  EXPECT_FALSE(witness.at("source_note").get<std::string>().empty());

  // The refused shape and the absent payload, pinned synthetically: a card with a refused
  // witness serializes exactly {state, mode}; a full-dimensional card serializes none.
  SurveyResult shapes;
  SurveyObject refused_card;
  SurveyObject::WitnessPayload refused_payload;  // the struct default state is refused
  refused_payload.mode = "no_declared_source_witness";
  refused_card.witness = std::move(refused_payload);
  shapes.objects.push_back(refused_card);
  SurveyObject t1_card;
  t1_card.kind = SurveyObjectKind::kSupportSet;
  shapes.objects.push_back(t1_card);
  const auto shape_doc = nlohmann::json::parse(SurveyResultToJson(shapes, "test"));
  const auto& refused = shape_doc.at("objects")[0].at("witness");
  EXPECT_EQ(refused.size(), 2u);  // {state, mode} — the whole shape, like the tint's refusal
  EXPECT_EQ(refused.at("state"), "refused");
  EXPECT_EQ(refused.at("mode"), "no_declared_source_witness");
  EXPECT_FALSE(shape_doc.at("objects")[1].contains("witness"));
}

}  // namespace
}  // namespace lumice::raypath::survey
