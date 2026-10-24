#include <gtest/gtest.h>

#include <cmath>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "core/optics.hpp"  // IceRefractiveIndex: the dispersion table the pair reads
#include "raypath/detail/survey/survey_assembly.hpp"
#include "raypath/detail/survey/survey_json.hpp"
#include "raypath/detail/survey/survey_tir_share.hpp"
#include "raypath/scene_to_analytic.hpp"
#include "survey_test_fixtures.hpp"

namespace lumice::raypath::survey {
namespace {

// The TIR share leg's inputs: table/polygons for the lattice and slot resolution, the crystal
// shape for the batch's own re-evaluation. The scenes are the C09 asymmetric pyramid (its
// 13-15-26-28 chain carries two internal interfaces over a spread of deviations) and the C01/C18
// regular prism (3-5-7 carries one; 3-5 carries none). The C15 plate is deliberately NOT a
// fixture here: its 1-3-6-2 chain is an identity map on the nominal plate (every kept event
// sits at exactly-zero deviation), so every pose is chart-degenerate and the leg has no
// question to ask on it — the identity arm does not run this leg for the same reason.
struct TirShareFixture {
  analytic::FaceNormalTable table;
  analytic::FacePolygonTable polygons;
  analytic::CrystalShape shape;
  std::vector<int> representative;
  double incident[3]{};

  // Which corpus-shaped scene the fixture builds; the member_count echo is the class size the
  // representative stands for (the benches pin the enumerations themselves).
  enum class Scene { kAsymmetricPyramid, kRegularPrism };
  static TirShareFixture Make(Scene scene) {
    const ConfigManager config = scene == Scene::kAsymmetricPyramid ? AsymmetricPyramidScene() : RandomPrismScene();
    const auto configured = config.crystals_.find(1);
    TirShareFixture fix;
    const CrystalConversion crystal = ConvertCrystal(configured->second.param_);
    EXPECT_EQ(analytic::BuildFaceNormals(crystal.shape, &fix.table, &fix.polygons), analytic::Status::kOk);
    fix.shape = crystal.shape;
    SunIncidentDirection(config.scene_.light_source_.param_, fix.incident);
    return fix;
  }

  quantity::TirShareInput Input(int sample_count) const {
    quantity::TirShareInput input;
    input.table = &table;
    input.polygons = &polygons;
    input.crystal = &shape;
    input.representative = &representative;
    for (int k = 0; k < 3; k++) {
      input.incident[k] = incident[k];
    }
    input.member_count = 12;
    input.sample_count = sample_count;
    return input;
  }
};

// The leg's positive shape on a k=2 path: both quantities present, every share inside [0,1],
// the all-TIR aggregate bounded by each per-interface share (all-TIR poses are a subset of each
// single-interface-TIR set — subset sums of non-negative weights can only sit below), and the
// echoes complete.
TEST(TirShareResult, TwoInternalInterfacesSeparateTheAggregateFromThePerInterfaceShares) {
  TirShareFixture fix = TirShareFixture::Make(TirShareFixture::Scene::kAsymmetricPyramid);
  const std::vector<int> representative{ 13, 15, 26, 28 };
  fix.representative = representative;
  SurveyData document;
  AssembleTirShare(fix.Input(20000), &document);
  ASSERT_TRUE(document.tir_share.has_value());
  const TirShareResult& tir = *document.tir_share;
  if (tir.State() != EvidenceState::kFinite) {
    ADD_FAILURE() << "unexpected refusal mode: " << tir.Mode();
    return;
  }
  ASSERT_EQ(tir.Value().interfaces.size(), 2u);
  EXPECT_EQ(tir.Value().interfaces[0].face, 15);
  EXPECT_EQ(tir.Value().interfaces[1].face, 26);
  for (const TirShareInterface& row : tir.Value().interfaces) {
    EXPECT_GE(row.share_red, 0.0);
    EXPECT_LE(row.share_red, 1.0);
    EXPECT_GE(row.share_blue, 0.0);
    EXPECT_LE(row.share_blue, 1.0);
    EXPECT_LE(tir.Value().share_all_tir_red, row.share_red);
    EXPECT_LE(tir.Value().share_all_tir_blue, row.share_blue);
  }
  EXPECT_GE(tir.Value().share_all_tir_red, 0.0);
  EXPECT_LE(tir.Value().share_all_tir_red, 1.0);
  EXPECT_GE(tir.Value().share_all_tir_blue, 0.0);
  EXPECT_LE(tir.Value().share_all_tir_blue, 1.0);
  EXPECT_EQ(tir.Value().n_red, IceRefractiveIndex::Get(650.0));
  EXPECT_EQ(tir.Value().n_blue, IceRefractiveIndex::Get(450.0));
  EXPECT_EQ(tir.Value().sample_count, 20000);
  EXPECT_EQ(tir.Value().lattice, "antipodal_fibonacci");
  EXPECT_EQ(tir.Value().deterministic, true);
  EXPECT_EQ(tir.Value().representative, representative);
  EXPECT_EQ(tir.Value().member_count, 12);
  EXPECT_GT(tir.Value().kept_count_red, 0);
  EXPECT_GT(tir.Value().kept_count_blue, 0);
  EXPECT_FALSE(tir.Value().caliber.empty());
}

// The k=1 degenerate form: with one internal interface the aggregate IS the per-interface row
// (identical additions in identical order), exactly — the assertion bench pins the identity at
// this shape.
TEST(TirShareResult, OneInternalInterfaceMakesTheAggregateIdenticalToTheRow) {
  TirShareFixture fix = TirShareFixture::Make(TirShareFixture::Scene::kRegularPrism);
  const std::vector<int> representative{ 3, 5, 7 };
  fix.representative = representative;
  SurveyData document;
  AssembleTirShare(fix.Input(20000), &document);
  ASSERT_TRUE(document.tir_share.has_value());
  const TirShareResult& tir = *document.tir_share;
  if (tir.State() != EvidenceState::kFinite) {
    ADD_FAILURE() << "unexpected refusal mode: " << tir.Mode();
    return;
  }
  ASSERT_EQ(tir.Value().interfaces.size(), 1u);
  EXPECT_EQ(tir.Value().interfaces[0].face, 5);
  EXPECT_DOUBLE_EQ(tir.Value().share_all_tir_red, tir.Value().interfaces[0].share_red);
  EXPECT_DOUBLE_EQ(tir.Value().share_all_tir_blue, tir.Value().interfaces[0].share_blue);
}

// The no-question shape: a two-face chain has one entry and one exit and no internal interface,
// and the leg refuses rather than reporting a share (or an absence) — the question is
// meaningless for the path shape.
TEST(TirShareResult, SlabPathWithoutInternalInterfacesRefuses) {
  TirShareFixture fix = TirShareFixture::Make(TirShareFixture::Scene::kRegularPrism);
  const std::vector<int> representative{ 3, 5 };
  fix.representative = representative;
  SurveyData document;
  AssembleTirShare(fix.Input(20000), &document);
  ASSERT_TRUE(document.tir_share.has_value());
  EXPECT_EQ(document.tir_share->State(), EvidenceState::kRefused);
  EXPECT_EQ(document.tir_share->Mode(), "no_internal_interfaces");
}

// The negative control: a representative naming a face the crystal lacks cannot resolve, and
// the leg refuses with the sibling legs' member_resolution shape.
TEST(TirShareResult, UnresolvableRepresentativeRefusesWithMemberResolution) {
  TirShareFixture fix = TirShareFixture::Make(TirShareFixture::Scene::kRegularPrism);
  const std::vector<int> unresolvable{ 99, 99 };
  fix.representative = unresolvable;
  SurveyData document;
  AssembleTirShare(fix.Input(20000), &document);
  ASSERT_TRUE(document.tir_share.has_value());
  EXPECT_EQ(document.tir_share->State(), EvidenceState::kRefused);
  EXPECT_EQ(document.tir_share->Mode(), "member_resolution");
}

// Determinism: the leg is a deterministic lattice plus a deterministic batch — two runs agree
// to the last bit on every share.
TEST(TirShareResult, TwoIdenticalRunsProduceTheSameShares) {
  TirShareFixture fix = TirShareFixture::Make(TirShareFixture::Scene::kAsymmetricPyramid);
  const std::vector<int> representative{ 13, 15, 26, 28 };
  fix.representative = representative;
  SurveyData first;
  AssembleTirShare(fix.Input(20000), &first);
  SurveyData second;
  AssembleTirShare(fix.Input(20000), &second);
  ASSERT_TRUE(first.tir_share.has_value());
  ASSERT_TRUE(second.tir_share.has_value());
  ASSERT_EQ(first.tir_share->State(), EvidenceState::kFinite);
  EXPECT_EQ(first.tir_share->Value().share_all_tir_red, second.tir_share->Value().share_all_tir_red);
  EXPECT_EQ(first.tir_share->Value().share_all_tir_blue, second.tir_share->Value().share_all_tir_blue);
  EXPECT_EQ(first.tir_share->Value().interfaces[0].share_red, second.tir_share->Value().interfaces[0].share_red);
  EXPECT_EQ(first.tir_share->Value().interfaces[1].share_blue, second.tir_share->Value().interfaces[1].share_blue);
}

// The wiring, T1 arms: the full-dimensional route carries the document-level key (here on the
// two-face prism path, where the leg itself refuses with no_internal_interfaces — the key is
// the leg having run, not its verdict); the identity arm's early stop carries no key at all.
TEST(TirShareResult, AssemblyWiringFollowsTheArmScope) {
  const ConfigManager config = RandomPrismScene();
  SurveyRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 5 } };
  request.symmetry_bits = 0;  // the literal single member: the seconds-scale shape of the arm
  SurveyData document;
  ASSERT_TRUE(AssembleSurveyData(config, request, &document).Ok());
  EXPECT_FALSE(document.early_stopped);
  ASSERT_TRUE(document.tir_share.has_value());
  EXPECT_EQ(document.tir_share->State(), EvidenceState::kRefused);
  EXPECT_EQ(document.tir_share->Mode(), "no_internal_interfaces");
  // The leg's refusal never touches the chromatic block (the T1 arm's block carries the
  // class tint's own finite state).
  EXPECT_EQ(document.chromatic.state, EvidenceState::kFinite);

  const ConfigManager plate = PlateScene(1.5, 0.3);
  SurveyRequest plate_request;
  plate_request.crystal_id = 1;
  plate_request.path_layers = { { 1, 3, 6, 2 } };
  SurveyData plate_document;
  ASSERT_TRUE(AssembleSurveyData(plate, plate_request, &plate_document).Ok());
  EXPECT_TRUE(plate_document.early_stopped);
  EXPECT_FALSE(plate_document.tir_share.has_value());  // the identity arm does not run the leg
}

// The wiring, refused-support arm: the C17-shaped touching chain's partition refuses, the
// witness hangs its positive fact on the refused card, and the TIR share leg runs on the same
// path — finite, with its two per-interface rows (the refusal says the support is unknown, not
// that the internal-reflection question vanished).
TEST(TirShareResult, RefusedSupportArmStillRunsTheLeg) {
  const ConfigManager config = RegularPrismSceneWithSun(15.0);
  SurveyRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 1, 5, 7 } };
  request.symmetry_bits = 0;
  SurveyData document;
  ASSERT_TRUE(AssembleSurveyData(config, request, &document).Ok());
  ASSERT_EQ(document.objects.size(), 1u);
  EXPECT_EQ(document.objects.front().Evidence().state, EvidenceState::kRefused);
  ASSERT_TRUE(document.objects.front().Witness() != nullptr);
  ASSERT_TRUE(document.tir_share.has_value());
  ASSERT_EQ(document.tir_share->State(), EvidenceState::kFinite);
  EXPECT_EQ(document.tir_share->Value().interfaces.size(), 2u);
  EXPECT_EQ(document.tir_share->Value().interfaces[0].face, 1);
  EXPECT_EQ(document.tir_share->Value().interfaces[1].face, 5);
}

}  // namespace
}  // namespace lumice::raypath::survey
