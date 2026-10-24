#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <string>

#include "raypath/detail/survey/survey_family.hpp"  // the declared constants the echo asserts
#include "raypath/detail/survey/survey_json.hpp"
#include "raypath/detail/survey/survey_probe.hpp"  // kProbeWedgeHalfWidthDeg: the spec echo's source
#include "raypath/survey_result.hpp"

namespace lumice::raypath::survey {
namespace {

nlohmann::json Serialized(const SurveyData& document) {
  return nlohmann::json::parse(SurveyResultToJson(SurveyResult(document), "test"));
}

// A minimal hand-built document in the early-stop shape. Its two members are deliberately NOT
// the C14 real class (which is {1-2} alone — the plate ensemble is not mirror-symmetric): the
// fixture covers the N-member serialization path, not that one corpus case.
SurveyData SourceImageDocument() {
  SurveyData document;
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
  card.SetKind(SurveyObjectKind::kSourceImage);
  card.members = document.members;
  card.has_position = true;
  card.position_altitude_deg = 20.0;
  card.position_azimuth_deg = 0.0;
  card.SetState(EvidenceState::kCertified);
  card.Method() = kCertifiedMethodPointMass;
  card.SetCertificate(SurveyCertificate{});
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

TEST(SurveyJsonV4, DefaultEvidenceIsUnevaluatedUntilAProducerRuns) {
  const auto document = Serialized(SourceImageDocument());
  const auto& chromatic = document.at("chromatic");
  EXPECT_EQ(chromatic.at("state"), "unevaluated");
  EXPECT_EQ(chromatic.at("reason"), "not_requested");
  EXPECT_FALSE(chromatic.contains("coverage"));
}

TEST(SurveyJsonV4, RefusedBlockSerializesStateAndItsOpenMode) {
  SurveyData skeleton;
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
  unevaluated.reason = "fixture: a routing leg not wired";
  SurveyData skeleton;
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
  SurveyData document;
  document.request.crystal_id = 1;
  document.request.path_layers = { { 3, 5 } };
  document.request.symmetry_bits = 7;
  document.members = { { 3, 5 } };
  document.wavelength_nm = 550.0;
  document.refractive_index = 1.306;

  SurveyObject support;
  support.SetKind(SurveyObjectKind::kSupportSet);
  support.members = { { 3, 5 } };
  support.SetState(EvidenceState::kFinite);  // the partition is a finite computation; the
                                             // certified card is the caustic edge's position
  support.Coverage().members_evaluated = 12;
  support.Coverage().wavelength_nm = 550.0;
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
  support.SetSupport(support_payload);
  document.objects.push_back(std::move(support));

  SurveyObject edge;
  edge.SetKind(SurveyObjectKind::kCausticEdge);
  edge.members = { { 3, 5 } };
  edge.SetState(EvidenceState::kCertified);
  edge.Method() = kCertifiedMethodSlabMinDeviationClosedForm;
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
  edge.SetEdge(edge_payload);
  document.objects.push_back(std::move(edge));

  SurveyObject domain;
  domain.SetKind(SurveyObjectKind::kBrightDomain);
  domain.members = { { 3, 5 } };
  domain.SetState(EvidenceState::kFinite);
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
  domain.SetBright(domain_payload);
  document.objects.push_back(std::move(domain));

  SurveyProbe probe;
  probe.spec.wavelength_nm = 550.0;
  probe.spec.refractive_index = 1.306;
  probe.spec.sample_count = 2000000;
  probe.spec.wedge_half_width_deg = kProbeWedgeHalfWidthDeg;
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
  document.positional_dispersion->has_min_deviation_row = true;  // the min-row scalars below
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
  document.weight_tint = SurveyWeightTint{};
  document.weight_tint->SetState(EvidenceState::kUnevaluated);
  document.weight_tint->Reason() =
      "chromatic legs not run: the delta axis refused (slab_crease_touching), and the "
      "chromatic legs sit downstream of it";
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
  EXPECT_DOUBLE_EQ(probe_json[0].at("spec").at("wedge_half_width_deg").get<double>(), kProbeWedgeHalfWidthDeg);
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
  EXPECT_EQ(chromatic.at("weight_tint").at("reason"),
            "chromatic legs not run: the delta axis refused (slab_crease_touching), and the "
            "chromatic legs sit downstream of it");
}

// The tint leg's three serialized shapes. The unevaluated one rides the fixture above; the
// refused and finite ones are pinned here.
TEST(SurveyJsonV4, WeightTintRefusedSerializesStateAndModeOnly) {
  SurveyData document;
  document.weight_tint = SurveyWeightTint{};
  document.weight_tint->SetState(EvidenceState::kRefused);
  document.weight_tint->Mode() = "density_not_expressible";
  const auto parsed = Serialized(document);
  const auto& tint = parsed.at("chromatic").at("weight_tint");
  EXPECT_EQ(tint.at("state"), "refused");
  EXPECT_EQ(tint.at("mode"), "density_not_expressible");
  EXPECT_EQ(tint.size(), 2u);  // the refusal is the whole shape — no fields, no evidence nesting
}

TEST(SurveyJsonV4, WeightTintFiniteCarriesTheFullFieldSet) {
  SurveyData document;
  document.weight_tint = SurveyWeightTint{};
  SurveyWeightTint& tint = *document.weight_tint;
  tint.SetState(EvidenceState::kFinite);
  tint.Value().n_red = 1.30803882;
  tint.Value().n_blue = 1.31569243;
  tint.Value().Mass().red = 0.001462474986811242;
  tint.Value().Mass().blue = 0.0024096673723776026;
  tint.Value().Mass().red_error = 1.2e-7;
  tint.Value().Mass().blue_error = 2.1e-7;
  tint.Value().Mass().protocol = TintMassProtocol::kPsiAverage;
  tint.Value().sample_count = 1000000;
  tint.Value().representative = { 1, 3, 6, 2 };
  tint.Value().member_count = 6;
  tint.Value().ratio = 1.6476639902276924;
  tint.Value().direction = SurveyTintDirection::kBlueTinted;
  tint.Value().kept_count_red = 312000;
  tint.Value().kept_count_blue = 298000;
  tint.SetState(EvidenceState::kFinite);
  tint.Coverage().members_evaluated = 1;
  tint.Coverage().wavelength_nm = 450.0;

  const auto parsed = Serialized(document);
  const auto& tint_json = parsed.at("chromatic").at("weight_tint");
  EXPECT_EQ(tint_json.at("evidence").at("state"), "finite");  // the evidence rides nested
  EXPECT_EQ(tint_json.at("red_label"), "650nm");
  EXPECT_EQ(tint_json.at("blue_label"), "450nm");
  EXPECT_DOUBLE_EQ(tint_json.at("n_red").get<double>(), 1.30803882);
  EXPECT_DOUBLE_EQ(tint_json.at("n_blue").get<double>(), 1.31569243);
  EXPECT_DOUBLE_EQ(tint_json.at("ratio").get<double>(), 1.6476639902276924);
  EXPECT_EQ(tint_json.at("method"), "psi_average");
  EXPECT_EQ(tint_json.at("sample_count"), 1000000);
  // The lattice self-description (S4): the sample's own sampler kind and determinism, the same
  // two keys the probe spec carries.
  EXPECT_EQ(tint_json.at("lattice"), "antipodal_fibonacci");
  EXPECT_EQ(tint_json.at("deterministic"), true);
  EXPECT_EQ(tint_json.at("representative"), std::vector<int>({ 1, 3, 6, 2 }));
  EXPECT_EQ(tint_json.at("member_count"), 6);
  EXPECT_EQ(tint_json.at("direction"), "blue_tinted");
  EXPECT_EQ(tint_json.at("kept_count_red"), 312000);
  EXPECT_EQ(tint_json.at("kept_count_blue"), 298000);
  // The caliber statement: a reader can tell the lambda-pair transmission-weight ratio from a
  // disk-integrated D65 B/R without leaving the document.
  EXPECT_NE(tint_json.at("caliber").get<std::string>().find("SPD"), std::string::npos);
  EXPECT_TRUE(tint_json.at("evidence").at("coverage").contains("members_evaluated"));
}

// The class power-sum shape (the T1 arm's reading): energy fields instead of masses, the
// lattice_power_sum method, and the point-mass family's keys stay absent — one shape per
// document, never both.
TEST(SurveyJsonV4, WeightTintClassPowerSumCarriesTheEnergyFieldFamily) {
  SurveyData document;
  document.weight_tint = SurveyWeightTint{};
  SurveyWeightTint& tint = *document.weight_tint;
  tint.SetState(EvidenceState::kFinite);
  tint.Value().reading = TintClassPowerReading{};
  tint.Value().n_red = 1.30803882;
  tint.Value().n_blue = 1.31569243;
  tint.Value().Power().red = 2.4e-5;
  tint.Value().Power().blue = 3.7e-5;
  tint.Value().sample_count = 1000000;
  tint.Value().representative = { 4, 8, 1, 7, 5 };
  tint.Value().member_count = 8;
  tint.Value().ratio = 1.541;
  tint.Value().direction = SurveyTintDirection::kBlueTinted;
  tint.Value().kept_count_red = 312000;
  tint.Value().kept_count_blue = 298000;
  tint.SetState(EvidenceState::kFinite);
  tint.Coverage().members_evaluated = 1;
  tint.Coverage().wavelength_nm = 450.0;

  const auto parsed = Serialized(document);
  const auto& tint_json = parsed.at("chromatic").at("weight_tint");
  EXPECT_EQ(tint_json.at("method"), "lattice_power_sum");
  EXPECT_DOUBLE_EQ(tint_json.at("energy_red").get<double>(), 2.4e-5);
  EXPECT_DOUBLE_EQ(tint_json.at("energy_blue").get<double>(), 3.7e-5);
  EXPECT_FALSE(tint_json.contains("m_red"));
  EXPECT_FALSE(tint_json.contains("m_red_error"));
  EXPECT_EQ(tint_json.at("direction"), "blue_tinted");
}

// The identity arm's refusal shape (review Minor#1): the tint is the block's only leg there,
// so the block mirrors the leg's refusal — the two never disagree, and the sibling legs stay
// absent (a point-mass path has no positional dispersion and no diagnose).
TEST(SurveyJsonV4, IdentityArmBlockMirrorsTheTintRefusal) {
  SurveyData document;
  document.chromatic.state = EvidenceState::kRefused;
  document.chromatic.mode = "density_not_expressible";
  document.weight_tint = SurveyWeightTint{};
  document.weight_tint->SetState(EvidenceState::kRefused);
  document.weight_tint->Mode() = "density_not_expressible";
  const auto parsed = Serialized(document);
  const auto& chromatic = parsed.at("chromatic");
  EXPECT_EQ(chromatic.at("state"), "refused");
  EXPECT_EQ(chromatic.at("mode"), "density_not_expressible");
  EXPECT_EQ(chromatic.at("weight_tint").at("state"), "refused");
  EXPECT_EQ(chromatic.at("weight_tint").at("mode"), "density_not_expressible");
  EXPECT_FALSE(chromatic.contains("positional_dispersion"));
  EXPECT_FALSE(chromatic.contains("diagnose"));
}

TEST(SurveyJsonV4, EmptyProbeProfileSerializesNoReadingKeys) {
  SurveyData document;
  SurveyProbe probe;  // no edge, no peak: the honest empty readings
  probe.spec.wavelength_nm = 550.0;
  probe.spec.representative = { 3, 5 };
  probe.kept_count = 0;
  document.probes.push_back(probe);
  const auto parsed = Serialized(document);
  const auto& probe_json = parsed.at("probes")[0];
  EXPECT_FALSE(probe_json.contains("edge"));
  EXPECT_FALSE(probe_json.contains("peak"));
  // A serialized separation of 0 on an empty profile would read as "the peak sits at the
  // edge" — a claim about a profile that has neither; the key stays absent instead.
  EXPECT_FALSE(probe_json.contains("separation_deg"));
}

TEST(SurveyJsonV4, EarlyStopDocumentCarriesNoneOfTheT1Keys) {
  SurveyData early_stop = SourceImageDocument();
  // The assembly names why this particular chromatic leg was not requested.
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
  SurveyData skeleton;
  skeleton.early_stopped = false;
  SurveyUnevaluated entry;
  entry.kinds = { SurveyObjectKind::kFamilyCollapsePoint, SurveyObjectKind::kRegularImage };
  entry.reason = "fixture: a routing leg not wired";
  skeleton.unevaluated.push_back(entry);
  const auto document = Serialized(skeleton);
  const auto& unevaluated = document.at("unevaluated");
  ASSERT_EQ(unevaluated.size(), 1u);
  EXPECT_EQ(unevaluated[0].at("kinds")[0], "family_collapse_point");
  EXPECT_EQ(unevaluated[0].at("kinds")[1], "regular_image");
  EXPECT_EQ(unevaluated[0].at("reason"), "fixture: a routing leg not wired");
}

// The exception evidence legs' conditional presence at the model layer: a document whose arms
// ran neither leg serializes neither key — the C14 byte baseline's structural guarantee (the
// pre-append documents are byte-stable because the keys cannot appear unfilled).
TEST(SurveyJsonV4, DefaultDocumentCarriesNeitherExceptionLegKey) {
  SurveyData document;
  const auto parsed = Serialized(document);
  EXPECT_FALSE(parsed.contains("tir_share"));
  EXPECT_FALSE(parsed.contains("mc_region_ess"));
}

// The appends' full field sets on a hand-built document: every planned key round-trips with
// its value (the schema-4 field list, pinned key by key).
TEST(SurveyJsonV4, ExceptionLegsSerializeTheirAppendShape) {
  SurveyData document;

  document.tir_share = SurveyTirShare{};
  SurveyTirShare& tir = *document.tir_share;
  tir.SetState(EvidenceState::kFinite);
  tir.Value().n_red = 1.31;
  tir.Value().n_blue = 1.34;
  tir.Value().interfaces.push_back({ 5, 0.25, 0.4 });
  tir.Value().interfaces.push_back({ 7, 0.1, 0.2 });
  tir.Value().share_all_tir_red = 0.05;
  tir.Value().share_all_tir_blue = 0.12;
  tir.Value().sample_count = 250000;
  tir.Value().representative = { 3, 5, 7 };
  tir.Value().member_count = 12;
  tir.Value().kept_count_red = 240000;
  tir.Value().kept_count_blue = 241000;
  tir.Value().caliber = "fixture caliber";
  tir.SetState(EvidenceState::kFinite);
  tir.Coverage().members_evaluated = 1;
  tir.Coverage().wavelength_nm = 550.0;

  document.mc_region_ess = SurveyMcRegionEss{};
  SurveyMcRegionEss& ess = *document.mc_region_ess;
  ess.SetState(EvidenceState::kFinite);
  ess.Value().seed = 20261022;
  ess.Value().draws = 20000;
  ess.Value().lambda_nm = 550.0;
  ess.Value().band_deg = 5.0;
  ess.Value().bands.push_back({ 20.0, 25.0, 512.0, 800, 0.5 });
  ess.Value().bands.push_back({ 25.0, 30.0, 0.0, 0, 0.0 });
  ess.SetState(EvidenceState::kFinite);
  ess.Coverage().members_evaluated = 12;
  ess.Coverage().wavelength_nm = 550.0;

  const auto parsed = Serialized(document);
  ASSERT_TRUE(parsed.contains("tir_share"));
  const auto& tir_json = parsed.at("tir_share");
  EXPECT_EQ(tir_json.at("n_red"), tir.Value().n_red);
  EXPECT_EQ(tir_json.at("n_blue"), tir.Value().n_blue);
  ASSERT_EQ(tir_json.at("interfaces").size(), 2u);
  EXPECT_EQ(tir_json.at("interfaces")[0].at("face"), 5);
  EXPECT_EQ(tir_json.at("interfaces")[0].at("share_red"), tir.Value().interfaces[0].share_red);
  EXPECT_EQ(tir_json.at("interfaces")[0].at("share_blue"), tir.Value().interfaces[0].share_blue);
  EXPECT_EQ(tir_json.at("share_all_tir_red"), tir.Value().share_all_tir_red);
  EXPECT_EQ(tir_json.at("share_all_tir_blue"), tir.Value().share_all_tir_blue);
  EXPECT_EQ(tir_json.at("sample_count"), tir.Value().sample_count);
  EXPECT_EQ(tir_json.at("lattice"), "antipodal_fibonacci");
  EXPECT_EQ(tir_json.at("deterministic"), true);
  const std::vector<int> expected_representative{ 3, 5, 7 };
  EXPECT_EQ(tir_json.at("representative"), expected_representative);
  EXPECT_EQ(tir_json.at("member_count"), 12);
  EXPECT_EQ(tir_json.at("kept_count_red"), tir.Value().kept_count_red);
  EXPECT_EQ(tir_json.at("kept_count_blue"), tir.Value().kept_count_blue);
  EXPECT_EQ(tir_json.at("caliber"), "fixture caliber");
  EXPECT_EQ(tir_json.at("evidence").at("state"), "finite");

  ASSERT_TRUE(parsed.contains("mc_region_ess"));
  const auto& ess_json = parsed.at("mc_region_ess");
  EXPECT_EQ(ess_json.at("seed"), 20261022);
  EXPECT_EQ(ess_json.at("draws"), 20000);
  EXPECT_EQ(ess_json.at("lambda_nm"), 550.0);
  EXPECT_EQ(ess_json.at("band_deg"), 5.0);
  ASSERT_EQ(ess_json.at("bands").size(), 2u);
  EXPECT_EQ(ess_json.at("bands")[0].at("delta_min_deg"), 20.0);
  EXPECT_EQ(ess_json.at("bands")[0].at("delta_max_deg"), 25.0);
  EXPECT_EQ(ess_json.at("bands")[0].at("ess"), 512.0);
  EXPECT_EQ(ess_json.at("bands")[0].at("draw_count"), 800);
  EXPECT_EQ(ess_json.at("bands")[0].at("sum_weight"), 0.5);
  EXPECT_EQ(ess_json.at("bands")[1].at("ess"), 0.0);  // an empty band serializes its honest zero
  EXPECT_EQ(ess_json.at("deterministic"), false);
  EXPECT_EQ(ess_json.at("evidence").at("state"), "finite");
}

// The refused shapes: exactly the evidence block ({state, mode}) — no defaulted number, the
// tint leg's refused shape at the new top-level keys.
TEST(SurveyJsonV4, ExceptionLegRefusalsSerializeStateAndModeOnly) {
  SurveyData document;
  document.tir_share = SurveyTirShare{};
  document.tir_share->SetState(EvidenceState::kRefused);
  document.tir_share->Mode() = "no_internal_interfaces";
  document.mc_region_ess = SurveyMcRegionEss{};
  document.mc_region_ess->SetState(EvidenceState::kFinite);
  document.mc_region_ess->Value().seed = 20261022;  // filled before the refusal: still must not serialize
  document.mc_region_ess->SetState(EvidenceState::kRefused);
  document.mc_region_ess->Mode() = "budget_exhausted";

  const auto parsed = Serialized(document);
  ASSERT_TRUE(parsed.contains("tir_share"));
  EXPECT_EQ(parsed.at("tir_share").at("state"), "refused");
  EXPECT_EQ(parsed.at("tir_share").at("mode"), "no_internal_interfaces");
  EXPECT_EQ(parsed.at("tir_share").size(), 2u);  // state + mode, nothing else
  ASSERT_TRUE(parsed.contains("mc_region_ess"));
  EXPECT_EQ(parsed.at("mc_region_ess").at("state"), "refused");
  EXPECT_EQ(parsed.at("mc_region_ess").at("mode"), "budget_exhausted");
  EXPECT_EQ(parsed.at("mc_region_ess").size(), 2u);
}


// The family routing block's serialized shape on a hand-built document (the schema-4 append's
// own discipline): the measure echo, the ladder's rank coverage exactly when the rank question
// was asked, the killed records with their numbers, the band and family_origin on the cards,
// and the conditional presence of the whole key.
TEST(SurveyJsonV4, FamilyRoutingBlockSerializesItsAppendShape) {
  SurveyData document;
  document.family_routing = FamilyRoutingBlock{};
  FamilyRoutingBlock& block = *document.family_routing;
  block.measure.zenith = { FamilyDimKind::kGauss, 0.0, 40.0 };
  block.measure.azimuth = { FamilyDimKind::kUniformFull, 0.0, 360.0 };
  block.measure.roll = { FamilyDimKind::kGauss, 0.0, 0.3 };

  FamilyLadderLevel level0;
  level0.level = 0;
  level0.free_dims = { analytic::FamilyDimension::kZenith, analytic::FamilyDimension::kAzimuth,
                       analytic::FamilyDimension::kRoll };
  level0.rank_asked = false;  // three free dimensions: the support alone, no rank key
  block.ladder.push_back(level0);

  FamilyLadderLevel level1;
  level1.level = 1;
  level1.frozen_dims = { { analytic::FamilyDimension::kRoll, 0.0, 0.3 } };
  level1.free_dims = { analytic::FamilyDimension::kZenith, analytic::FamilyDimension::kAzimuth };
  level1.rank_asked = true;
  level1.generic_rank = 2;
  level1.grid = kFamilyLadderGrid;
  level1.grid_spacing_deg[0] = 1.4173;
  level1.grid_spacing_deg[1] = 2.8125;
  level1.samples = 16384;
  level1.valid_samples = 14000;
  level1.objects_proposed = 4;
  level1.objects_killed = 1;
  FamilyKilledCandidate killed;
  killed.member = { 3, 5 };
  killed.kind = "regular_image";
  killed.has_position = true;
  killed.position_altitude_deg = 12.5;
  killed.position_azimuth_deg = 40.0;
  killed.blur_measured = true;  // a survival_filter kill carries its numbers by definition
  killed.blur_deg = 41.2;
  killed.scale_deg = 22.7;
  killed.clamped = true;
  killed.reason = "survival_filter";
  killed.verdicts = { { "roll", "narrow" }, { "zenith", "wide" } };
  level1.killed.push_back(killed);
  block.ladder.push_back(level1);
  block.evidence.state = EvidenceState::kFinite;
  block.evidence.coverage.members_evaluated = 1;

  SurveyObject card;
  card.SetKind(SurveyObjectKind::kCausticEdge);
  card.members = { { 3, 5 } };
  card.has_position = true;
  card.position_altitude_deg = 36.0;
  card.position_azimuth_deg = 20.6;
  card.SetState(EvidenceState::kFinite);
  SurveyObject::BandPayload band;
  band.level = 1;
  band.frozen = { { "roll", "narrow" } };
  band.blur_deg = 0.21;
  band.scale_deg = 3.2;
  card.band = band;
  SurveyObject::FamilyOriginPayload origin;
  origin.crossing_count = 5;
  card.family_origin = origin;
  document.objects.push_back(card);

  const auto parsed = Serialized(document);
  ASSERT_TRUE(parsed.contains("family_routing"));
  const auto& family = parsed.at("family_routing");
  EXPECT_EQ(family.at("measure").at("zenith").at("kind"), "gauss");
  EXPECT_DOUBLE_EQ(family.at("measure").at("zenith").at("width_deg"), 40.0);
  EXPECT_EQ(family.at("measure").at("azimuth").at("kind"), "uniform");
  const auto& ladder = family.at("ladder");
  ASSERT_EQ(ladder.size(), 2u);
  EXPECT_EQ(ladder.at(0).at("free_dims"), nlohmann::json::array({ "zenith", "azimuth", "roll" }));
  EXPECT_FALSE(ladder.at(0).contains("rank"));
  const auto& rank_level = ladder.at(1);
  EXPECT_EQ(rank_level.at("free_dims"), nlohmann::json::array({ "zenith", "azimuth" }));
  EXPECT_EQ(rank_level.at("rank").at("generic_rank"), 2);
  EXPECT_EQ(rank_level.at("rank").at("grid"), kFamilyLadderGrid);
  ASSERT_EQ(rank_level.at("rank").at("grid_spacing_deg").size(), 2u);
  EXPECT_EQ(rank_level.at("objects_proposed"), 4);
  ASSERT_EQ(rank_level.at("killed").size(), 1u);
  const auto& killed_json = rank_level.at("killed").at(0);
  EXPECT_EQ(killed_json.at("kind"), "regular_image");
  EXPECT_EQ(killed_json.at("reason"), "survival_filter");
  EXPECT_EQ(killed_json.at("clamped"), true);
  ASSERT_EQ(killed_json.at("verdicts").size(), 2u);
  EXPECT_EQ(killed_json.at("verdicts").at(1).at("verdict"), "wide");
  const auto& card_json = parsed.at("objects").at(0);
  EXPECT_EQ(card_json.at("band").at("level"), 1);
  EXPECT_EQ(card_json.at("band").at("survival"), "pass");
  EXPECT_DOUBLE_EQ(card_json.at("band").at("blur_deg"), 0.21);
  EXPECT_EQ(card_json.at("family_origin").at("crossing_count"), 5);
  // No caustic_edge payload key: the family-form card carries band and family_origin, not the
  // full-dimensional arm's delta-axis payload (a payload key never crosses forms).
  EXPECT_FALSE(card_json.contains("caustic_edge"));
}

// The absence semantics: a document no family arm produced serializes no family_routing key at
// all — the block's presence IS the statement that the arm ran.
TEST(SurveyJsonV4, FamilyRoutingKeyIsAbsentWithoutTheArm) {
  const auto parsed = Serialized(SourceImageDocument());
  EXPECT_FALSE(parsed.contains("family_routing"));
}

// The block-level echo of the level-0 partition's refusal: the key exists exactly when the
// partition refused, carrying the same mode string the refused card does.
TEST(SurveyJsonV4, Level0RefusalModeKeyExistsExactlyWhenSet) {
  SurveyData document;
  document.family_routing = FamilyRoutingBlock{};
  const auto absent = Serialized(document);
  ASSERT_TRUE(absent.contains("family_routing"));
  EXPECT_FALSE(absent.at("family_routing").contains("level0_refusal_mode"));

  document.family_routing->level0_refusal_mode = "slab_crease_touching";
  const auto present = Serialized(document);
  EXPECT_EQ(present.at("family_routing").at("level0_refusal_mode"), "slab_crease_touching");
}

}  // namespace
}  // namespace lumice::raypath::survey
