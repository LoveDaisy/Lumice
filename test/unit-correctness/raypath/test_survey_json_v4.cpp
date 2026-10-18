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

TEST(SurveyJsonV4, CertifiedMethodRegistryRoundTripsAndStaysClosed) {
  for (const std::string_view method : kCertifiedMethods) {
    EXPECT_TRUE(CertifiedMethodRegistered(method)) << method;
  }
  EXPECT_TRUE(CertifiedMethodRegistered(kCertifiedMethodPointMass));
  EXPECT_TRUE(CertifiedMethodRegistered(kCertifiedMethodSlabMinDeviationClosedForm));
  EXPECT_FALSE(CertifiedMethodRegistered("trust_me"));
}

TEST(SurveyJsonV4, T1PayloadsProbeAndChromaticLegsSerializeTheirKeys) {
  SurveyDocument document;
  document.request.crystal_id = 1;
  document.request.path_layers = { { 3, 5 } };
  document.request.symmetry_bits = 7;
  document.members = { { 3, 5 } };
  document.wavelength_nm = 550.0;
  document.refractive_index = 1.306;

  SurveyObject support;
  support.kind = SurveyObjectKind::kSupportSet;
  support.members = { { 3, 5 } };
  support.evidence.state = EvidenceState::kFinite;  // the partition is a finite computation; the
                                                    // certified card is the caustic edge's position
  support.evidence.coverage.members_evaluated = 12;
  support.evidence.coverage.wavelength_nm = 550.0;
  SurveyObject::SupportSetPayload support_payload;
  SurveyObject::Interval interval;
  interval.lower_deg = 22.2716;
  interval.upper_deg = 43.4598;
  interval.n_components = 1;
  interval.n_closed = 1;
  interval.n_open = 0;
  support_payload.intervals.push_back(interval);
  SurveyObject::EndpointLabel lower;
  lower.delta_deg = 22.2716;
  lower.source = "interior_minimum";
  lower.profile = "finite_jump";
  support_payload.endpoints.push_back(lower);
  support_payload.symmetry_bits = 7;
  support_payload.member_count = 12;
  support.support_set = support_payload;
  document.objects.push_back(std::move(support));

  SurveyObject edge;
  edge.kind = SurveyObjectKind::kCausticEdge;
  edge.members = { { 3, 5 } };
  edge.evidence.state = EvidenceState::kCertified;
  edge.evidence.method = kCertifiedMethodSlabMinDeviationClosedForm;
  SurveyObject::CausticEdgePayload edge_payload;
  edge_payload.position_deg = 22.2716;
  edge_payload.profile = "finite_jump";
  edge_payload.has_measure_limit = true;
  edge_payload.measure_limit = 12.5;
  edge_payload.coincident_with_support = true;
  SurveyObject::EdgeCertificate certificate;
  certificate.formula = "2*asin(n*sin(A/2)) - A";
  certificate.apex_angle_deg = 60.0;
  certificate.cross_check_deg = 1e-9;
  certificate.tolerance_deg = 1e-6;
  edge_payload.certificate = certificate;
  edge.caustic_edge = edge_payload;
  document.objects.push_back(std::move(edge));

  SurveyObject domain;
  domain.kind = SurveyObjectKind::kBrightDomain;
  domain.members = { { 3, 5 } };
  domain.evidence.state = EvidenceState::kFinite;
  SurveyObject::BrightDomainPayload domain_payload;
  domain_payload.domain_lower_deg = 22.2716;
  domain_payload.domain_upper_deg = 50.6188;
  domain_payload.has_peak = true;
  domain_payload.peak_delta_deg = 22.85;
  domain_payload.peak_value = 0.42;
  domain_payload.has_edge_reading = true;
  domain_payload.edge_delta_deg = 22.30;
  domain_payload.kept_count = 123456;
  domain_payload.probe_index = 0;
  domain.bright_domain = domain_payload;
  document.objects.push_back(std::move(domain));

  SurveyProbe probe;
  probe.spec.wavelength_nm = 550.0;
  probe.spec.refractive_index = 1.306;
  probe.spec.sample_count = 2000000;
  probe.spec.representative = { 3, 5 };
  probe.spec.member_count = 12;
  SurveyProbeSample sample;
  sample.delta_deg = 22.35;
  sample.value = 0.4;
  sample.k = 9;
  sample.k_eff = 7.2;
  probe.samples.push_back(sample);
  probe.has_edge = true;
  probe.edge_bin = 73;
  probe.edge_delta_deg = 22.35;
  probe.has_peak = true;
  probe.peak_delta_deg = 22.85;
  probe.peak_value = 0.42;
  probe.separation_deg = 0.5;
  probe.kept_count = 123456;
  document.probes.push_back(std::move(probe));

  document.positional_dispersion = SurveyPositionalDispersion{};
  document.positional_dispersion->n_red = 1.3053;
  document.positional_dispersion->n_blue = 1.3096;
  document.positional_dispersion->value_red_deg = 21.69;
  document.positional_dispersion->value_blue_deg = 22.27;
  document.positional_dispersion->displacement_deg = 0.58;
  document.diagnose = SurveyDiagnoseBlock{};
  document.diagnose->verdict_kind = "edge";
  document.diagnose->color = "blue";
  document.diagnose->has_position = true;
  document.diagnose->position_deg = 22.3;
  document.weight_tint = SurveyEvidence{};
  document.weight_tint->state = EvidenceState::kUnevaluated;
  document.weight_tint->reason = "class tint not wired (C15 slice)";
  document.chromatic.state = EvidenceState::kFinite;
  document.chromatic.coverage.members_evaluated = 12;
  document.chromatic.coverage.wavelength_nm = 550.0;
  document.early_stopped = false;
  document.evidence.state = EvidenceState::kFinite;

  const auto parsed = Serialized(document);
  // The three payload keys ride their own cards, each under its kind.
  const auto& objects = parsed.at("objects");
  ASSERT_EQ(objects.size(), 3u);
  const auto& support_json = objects[0].at("support_set");
  EXPECT_EQ(support_json.at("intervals").size(), 1u);
  EXPECT_DOUBLE_EQ(support_json.at("intervals")[0].at("lower_deg").get<double>(), 22.2716);
  EXPECT_EQ(support_json.at("intervals")[0].at("n_closed"), 1);
  EXPECT_EQ(support_json.at("endpoints")[0].at("source"), "interior_minimum");
  EXPECT_EQ(support_json.at("endpoints")[0].at("profile"), "finite_jump");
  EXPECT_EQ(support_json.at("symmetry_bits"), 7u);
  EXPECT_EQ(support_json.at("member_count"), 12);
  const auto& edge_json = objects[1].at("caustic_edge");
  EXPECT_DOUBLE_EQ(edge_json.at("position_deg").get<double>(), 22.2716);
  EXPECT_EQ(edge_json.at("profile"), "finite_jump");
  EXPECT_TRUE(edge_json.at("coincident_with_support").get<bool>());
  EXPECT_DOUBLE_EQ(edge_json.at("measure_limit").get<double>(), 12.5);
  EXPECT_EQ(edge_json.at("certificate").at("formula"), "2*asin(n*sin(A/2)) - A");
  EXPECT_DOUBLE_EQ(edge_json.at("certificate").at("cross_check_deg").get<double>(), 1e-9);
  const auto& domain_json = objects[2].at("bright_domain");
  EXPECT_DOUBLE_EQ(domain_json.at("domain_lower_deg").get<double>(), 22.2716);
  EXPECT_DOUBLE_EQ(domain_json.at("peak").at("delta_deg").get<double>(), 22.85);
  EXPECT_DOUBLE_EQ(domain_json.at("edge_reading_deg").get<double>(), 22.30);
  EXPECT_EQ(domain_json.at("kept_count"), 123456);
  EXPECT_EQ(domain_json.at("probe_index"), 0);
  // A payload key never appears on a card of another kind.
  for (const auto& object : objects) {
    for (const char* key : { "support_set", "caustic_edge", "bright_domain" }) {
      EXPECT_EQ(object.contains(key), object.at("kind") == key) << key;
    }
  }

  const auto& probe_json = parsed.at("probes");
  ASSERT_EQ(probe_json.size(), 1u);
  EXPECT_EQ(probe_json[0].at("kind"), "solar_radial");
  EXPECT_EQ(probe_json[0].at("spec").at("sample_count"), 2000000);
  EXPECT_EQ(probe_json[0].at("spec").at("lattice"), "antipodal_fibonacci");
  EXPECT_TRUE(probe_json[0].at("spec").at("deterministic").get<bool>());
  EXPECT_EQ(probe_json[0].at("spec").at("representative"), std::vector<int>({ 3, 5 }));
  EXPECT_EQ(probe_json[0].at("spec").at("member_count"), 12);
  EXPECT_DOUBLE_EQ(probe_json[0].at("edge").at("delta_deg").get<double>(), 22.35);
  EXPECT_DOUBLE_EQ(probe_json[0].at("peak").at("value").get<double>(), 0.42);
  EXPECT_DOUBLE_EQ(probe_json[0].at("separation_deg").get<double>(), 0.5);
  ASSERT_EQ(probe_json[0].at("samples").size(), 1u);
  EXPECT_EQ(probe_json[0].at("samples")[0].at("k"), 9);

  const auto& chromatic = parsed.at("chromatic");
  EXPECT_EQ(chromatic.at("state"), "finite");
  const auto& dispersion = chromatic.at("positional_dispersion");
  EXPECT_DOUBLE_EQ(dispersion.at("value_red_deg").get<double>(), 21.69);
  EXPECT_DOUBLE_EQ(dispersion.at("value_blue_deg").get<double>(), 22.27);
  EXPECT_EQ(dispersion.at("direction"), "red_inner_blue_outer");
  const auto& diagnose = chromatic.at("diagnose");
  EXPECT_EQ(diagnose.at("verdict_kind"), "edge");
  EXPECT_DOUBLE_EQ(diagnose.at("position_deg").get<double>(), 22.3);
  EXPECT_EQ(chromatic.at("weight_tint").at("state"), "unevaluated");
  EXPECT_EQ(chromatic.at("weight_tint").at("reason"), "class tint not wired (C15 slice)");
}

TEST(SurveyJsonV4, EarlyStopDocumentCarriesNoneOfTheT1Keys) {
  SurveyDocument early_stop = SourceImageDocument();
  // What AssembleSurvey sets on a real early stop (the fixture's default-finite chromatic is
  // the schema-default rule pinned by DefaultEvidenceIsFiniteWithCoverageDeclaration above).
  early_stop.chromatic.state = EvidenceState::kUnevaluated;
  early_stop.chromatic.reason = "survey v1: chromatic layer not wired";
  const auto document = Serialized(early_stop);
  EXPECT_FALSE(document.contains("probes"));  // a T1-leg key: absent, not an empty array
  for (const auto& object : document.at("objects")) {
    EXPECT_FALSE(object.contains("support_set"));
    EXPECT_FALSE(object.contains("caustic_edge"));
    EXPECT_FALSE(object.contains("bright_domain"));
  }
  const auto& chromatic = document.at("chromatic");
  EXPECT_FALSE(chromatic.contains("positional_dispersion"));
  EXPECT_FALSE(chromatic.contains("diagnose"));
  EXPECT_FALSE(chromatic.contains("weight_tint"));
  // The early-stop chromatic shape is byte-stable by contract: state + reason, nothing else.
  EXPECT_EQ(chromatic.at("state"), "unevaluated");
  EXPECT_EQ(chromatic.at("reason"), "survey v1: chromatic layer not wired");
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
