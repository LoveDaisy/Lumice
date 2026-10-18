#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>

#include "raypath/detail/survey/survey_document.hpp"
#include "raypath/detail/survey/survey_json.hpp"

namespace lumice::raypath::survey {
namespace {

nlohmann::json Serialized(const SurveyDocument& document) {
  return nlohmann::json::parse(SurveyDocumentToJson(document, "test"));
}

// A minimal hand-built document in the early-stop shape. Its two members are deliberately NOT
// the C14 real class (which is {1-2} alone — the plate ensemble is not mirror-symmetric): the
// fixture covers the N-member serialization path, not that one corpus case.
SurveyDocument SourceImageDocument() {
  SurveyDocument document;
  document.request.crystal_id = 1;
  document.request.path_layers = { { 1, 2 } };
  document.request.symmetry_bits = 7;
  document.request.budget_ms = 15000;
  document.members = { { 1, 2 }, { 2, 1 } };
  document.wavelength_nm = 550.0;
  document.wavelength_source = "default";
  document.refractive_index = 1.306;
  document.sun_altitude_deg = 20.0;
  document.sun_azimuth_deg = 0.0;

  SurveyMemberVerdict verdict;
  verdict.member = { 1, 2 };
  verdict.rank_zero = true;
  verdict.mechanism = kCertifiedMethodPointMass;
  verdict.evidence.state = EvidenceState::kCertified;
  verdict.evidence.method = kCertifiedMethodPointMass;
  document.member_verdicts.push_back(verdict);

  SurveyObject card;
  card.kind = SurveyObjectKind::kSourceImage;
  card.members = document.members;
  card.has_position = true;
  card.position_altitude_deg = 20.0;
  card.position_azimuth_deg = 0.0;
  card.evidence.state = EvidenceState::kCertified;
  card.evidence.method = kCertifiedMethodPointMass;
  card.certificate = SurveyCertificate{};
  document.objects.push_back(card);

  document.early_stopped = true;
  document.evidence.state = EvidenceState::kCertified;
  document.evidence.method = kCertifiedMethodPointMass;
  document.budget_ms = 15000;
  return document;
}

TEST(SurveyJsonV4, SchemaNameVersionAndRequiredTopLevelKeys) {
  const auto document = Serialized(SourceImageDocument());
  EXPECT_EQ(document.at("schema"), "lumice.path-survey");
  EXPECT_EQ(document.at("schema_version"), 4);
  for (const char* key : { "generator", "request", "scope", "routing", "objects", "unevaluated", "chromatic",
                           "evidence", "budget", "timing" }) {
    EXPECT_TRUE(document.contains(key)) << key;
  }
  EXPECT_EQ(document.at("generator").at("lumice"), "test");
}

TEST(SurveyJsonV4, EvidenceStateVocabularyIsClosedAndSwitchable) {
  const EvidenceState states[] = { EvidenceState::kCertified, EvidenceState::kFinite, EvidenceState::kRefused,
                                   EvidenceState::kUnevaluated };
  for (const EvidenceState state : states) {
    EvidenceState round_trip = EvidenceState::kFinite;
    if (!EvidenceStateFromName(EvidenceStateName(state), &round_trip)) {
      ADD_FAILURE() << "name not in the registry: " << EvidenceStateName(state);
      continue;
    }
    EXPECT_EQ(round_trip, state);
  }
  EvidenceState untouched = EvidenceState::kFinite;
  EXPECT_FALSE(EvidenceStateFromName("settled", &untouched));
  EXPECT_EQ(untouched, EvidenceState::kFinite);  // a refused name writes nothing
}

TEST(SurveyJsonV4, ObjectKindRegistryHasSixAppendOnlyNames) {
  const auto& kinds = AllSurveyObjectKinds();
  ASSERT_EQ(kinds.size(), 6u);
  const char* expected[] = { "source_image", "family_collapse_point", "regular_image",
                             "caustic_edge", "bright_domain",         "support_set" };
  for (size_t i = 0; i < kinds.size(); i++) {
    EXPECT_STREQ(SurveyObjectKindName(kinds[i]), expected[i]);
    SurveyObjectKind round_trip = kinds[0];
    if (!SurveyObjectKindFromName(expected[i], &round_trip)) {
      ADD_FAILURE() << "kind name not in the registry: " << expected[i];
      continue;
    }
    EXPECT_EQ(round_trip, kinds[i]);
  }
}

TEST(SurveyJsonV4, DefaultEvidenceIsFiniteWithCoverageDeclaration) {
  const auto document = Serialized(SourceImageDocument());
  // The chromatic block of the early-stop fixture stays at its default, so the serialized
  // document shows the default rule: state present, finite, coverage declared.
  const auto& chromatic = document.at("chromatic");
  EXPECT_EQ(chromatic.at("state"), "finite");
  EXPECT_TRUE(chromatic.contains("coverage"));
  EXPECT_TRUE(chromatic.at("coverage").contains("geometry_basis"));
  EXPECT_TRUE(chromatic.at("coverage").contains("wavelength_nm"));
  EXPECT_TRUE(chromatic.at("coverage").contains("members_evaluated"));
}

TEST(SurveyJsonV4, RefusedBlockSerializesStateAndItsOpenMode) {
  SurveyDocument skeleton;
  skeleton.chromatic = SurveyEvidence{};
  skeleton.chromatic.state = EvidenceState::kRefused;
  skeleton.chromatic.mode = "walk_truncated";
  const auto document = Serialized(skeleton);
  const auto& chromatic = document.at("chromatic");
  EXPECT_EQ(chromatic.at("state"), "refused");
  EXPECT_TRUE(chromatic.contains("mode"));
  EXPECT_EQ(chromatic.at("mode"), "walk_truncated");
  // The other three states' accompaniments stay out of a refused block.
  EXPECT_FALSE(chromatic.contains("reason"));
  EXPECT_FALSE(chromatic.contains("coverage"));
  EXPECT_FALSE(chromatic.contains("method"));
}

TEST(SurveyJsonV4, CertifiedBlocksCarryARegisteredMethod) {
  const auto document = Serialized(SourceImageDocument());
  const auto& card = document.at("objects")[0];
  ASSERT_EQ(card.at("kind"), "source_image");
  EXPECT_EQ(card.at("evidence").at("state"), "certified");
  const std::string method = card.at("evidence").at("method").get<std::string>();
  EXPECT_FALSE(method.empty());
  EXPECT_TRUE(CertifiedMethodRegistered(method));

  // The refused and unevaluated shapes carry their open mode / reason instead.
  SurveyEvidence refused;
  refused.state = EvidenceState::kRefused;
  refused.mode = "budget";
  SurveyEvidence unevaluated;
  unevaluated.state = EvidenceState::kUnevaluated;
  unevaluated.reason = "survey v1: T1/T2 not wired";
  SurveyDocument skeleton;
  skeleton.chromatic = refused;
  EvidenceState ignored = EvidenceState::kFinite;
  EXPECT_TRUE(EvidenceStateFromName("refused", &ignored));
  EXPECT_EQ(EvidenceStateName(unevaluated.state), std::string("unevaluated"));
}

TEST(SurveyJsonV4, SourceImageCardCarriesMembersPositionAndCertificate) {
  const auto document = Serialized(SourceImageDocument());
  const auto& card = document.at("objects")[0];
  EXPECT_EQ(card.at("members").size(), 2u);
  EXPECT_EQ(card.at("members")[0], std::vector<int>({ 1, 2 }));
  EXPECT_EQ(card.at("members")[1], std::vector<int>({ 2, 1 }));
  EXPECT_DOUBLE_EQ(card.at("position").at("altitude_deg").get<double>(), 20.0);
  EXPECT_DOUBLE_EQ(card.at("position").at("azimuth_deg").get<double>(), 0.0);
  EXPECT_TRUE(card.at("certificate").contains("fold_tolerance"));
  EXPECT_TRUE(card.at("certificate").contains("wedge_tolerance_deg"));
  // The T0 leg rides the routing block, one verdict per member.
  EXPECT_EQ(document.at("routing").at("early_stopped"), true);
  ASSERT_EQ(document.at("routing").at("members").size(), 1u);
  EXPECT_EQ(document.at("routing").at("members")[0].at("mechanism"), "point_mass");
}

TEST(SurveyJsonV4, UnevaluatedEntriesNameKindsAndReason) {
  SurveyDocument skeleton;
  skeleton.early_stopped = false;
  SurveyUnevaluated entry;
  entry.kinds = { SurveyObjectKind::kFamilyCollapsePoint, SurveyObjectKind::kRegularImage };
  entry.reason = "survey v1: T1/T2 not wired";
  skeleton.unevaluated.push_back(entry);
  const auto document = Serialized(skeleton);
  const auto& unevaluated = document.at("unevaluated");
  ASSERT_EQ(unevaluated.size(), 1u);
  EXPECT_EQ(unevaluated[0].at("kinds")[0], "family_collapse_point");
  EXPECT_EQ(unevaluated[0].at("kinds")[1], "regular_image");
  EXPECT_EQ(unevaluated[0].at("reason"), "survey v1: T1/T2 not wired");
}

}  // namespace
}  // namespace lumice::raypath::survey
