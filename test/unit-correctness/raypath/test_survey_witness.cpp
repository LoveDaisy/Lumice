#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "analytic/discovery.hpp"  // DiscoveryStop — the shared stop judgment
#include "analytic/stop.hpp"       // StopToken — the poll ledger the handshakes read
#include "core/optics.hpp"         // IceRefractiveIndex: the dispersion table the fixture reads
#include "raypath/detail/survey/survey_assembly.hpp"
#include "raypath/detail/survey/survey_witness.hpp"
#include "raypath/query_control.hpp"  // the survey legs' poll sites
#include "raypath/scene_to_analytic.hpp"
#include "raypath/survey_report.hpp"
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

  quantity::WitnessInput Input(int sample_count) const {
    quantity::WitnessInput input;
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
    card.SetKind(SurveyObjectKind::kSupportSet);
    AssembleWitness(fix.Input(n), &card);
    EXPECT_TRUE(card.Witness() != nullptr);
    if (card.Witness() == nullptr) {
      continue;
    }
    EXPECT_EQ(card.Witness()->State(), EvidenceState::kFinite);
    EXPECT_GT(card.Witness()->Value().kept_count, 0);
    EXPECT_EQ(card.Witness()->Value().sample_count, n);
  }
}

// The positive shape: the argmax event's readings are all positive, the provenance keys echo
// the caliber, and the source note is spelled inside the document.
TEST(SurveyWitness, PositiveWitnessCarriesTheFullReadingSet) {
  const WitnessFixture fix = WitnessFixture::Make();
  SurveyObject card;
  card.SetKind(SurveyObjectKind::kSupportSet);
  AssembleWitness(fix.Input(100000), &card);
  ASSERT_TRUE(card.Witness() != nullptr);
  const SurveyObject::WitnessPayload& witness = *card.Witness();
  EXPECT_EQ(witness.State(), EvidenceState::kFinite);
  EXPECT_GT(witness.Value().kept_count, 0);
  EXPECT_GT(witness.Value().weight, 0.0);
  EXPECT_GT(witness.Value().entry, 0.0);
  EXPECT_GT(witness.Value().interface_product, 0.0);
  EXPECT_TRUE(std::isfinite(witness.Value().deviation_deg));
  EXPECT_EQ(witness.Value().sample_count, 100000);
  EXPECT_EQ(witness.Value().lattice, "antipodal_fibonacci");
  EXPECT_TRUE(witness.Value().deterministic);
  EXPECT_FALSE(witness.Value().source_note.empty());
  EXPECT_EQ(witness.Coverage().members_evaluated, 1);
  EXPECT_DOUBLE_EQ(witness.Coverage().wavelength_nm, 550.0);
}

// The negative control: a representative naming a face the crystal lacks refuses with the same
// defensive mode as the sibling legs, and no numeric field is written.
TEST(SurveyWitness, UnresolvableRepresentativeRefusesWithMemberResolution) {
  const WitnessFixture fix = WitnessFixture::Make();
  const std::vector<int> unresolvable{ 99, 99 };
  quantity::WitnessInput input = fix.Input(100000);
  input.representative = &unresolvable;
  SurveyObject card;
  card.SetKind(SurveyObjectKind::kSupportSet);
  AssembleWitness(input, &card);
  ASSERT_TRUE(card.Witness() != nullptr);
  EXPECT_EQ(card.Witness()->State(), EvidenceState::kRefused);
  EXPECT_EQ(card.Witness()->Mode(), "member_resolution");
  EXPECT_EQ(card.Witness()->ValueOrNull(), nullptr);
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
  SurveyData document;
  ASSERT_TRUE(AssembleSurveyData(config, request, &document).Ok());
  EXPECT_FALSE(document.early_stopped);
  ASSERT_EQ(document.objects.size(), 1u);
  const SurveyObject& card = document.objects.front();
  EXPECT_EQ(card.Kind(), SurveyObjectKind::kSupportSet);
  // The refused card's own evidence stays refused; the witness sub-block is finite on it.
  EXPECT_EQ(card.State(), EvidenceState::kRefused);
  EXPECT_EQ(card.Mode(), "slab_crease_touching");
  ASSERT_TRUE(card.Witness() != nullptr);
  EXPECT_EQ(card.Witness()->State(), EvidenceState::kFinite);
  EXPECT_GT(card.Witness()->Value().kept_count, 0);
  EXPECT_GT(card.Witness()->Value().entry, 0.0);
  EXPECT_GT(card.Witness()->Value().interface_product, 0.0);
  // The document-level evidence carries the refusal; the chromatic legs name the axis refusal.
  EXPECT_EQ(document.evidence.state, EvidenceState::kRefused);
  EXPECT_EQ(document.evidence.mode, "slab_crease_touching");
  EXPECT_EQ(document.chromatic.state, EvidenceState::kUnevaluated);
  EXPECT_NE(document.chromatic.reason.find("slab_crease_touching"), std::string::npos);
  ASSERT_TRUE(document.weight_tint.has_value());
  EXPECT_EQ(document.weight_tint->State(), EvidenceState::kUnevaluated);
  EXPECT_EQ(document.weight_tint->Reason(), document.chromatic.reason);
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
  SurveyData document;
  ASSERT_TRUE(AssembleSurveyData(config, request, &document).Ok());
  const auto parsed = nlohmann::json::parse(SurveyResultToJson(SurveyResult(document), "test"));
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
  SurveyData shapes;
  SurveyObject refused_card;
  refused_card.SetKind(SurveyObjectKind::kSupportSet);
  SurveyObject::WitnessPayload refused_payload;
  refused_payload.SetState(EvidenceState::kRefused);
  refused_payload.Mode() = "no_declared_source_witness";
  refused_card.SetWitness(std::move(refused_payload));
  shapes.objects.push_back(refused_card);
  SurveyObject t1_card;
  t1_card.SetKind(SurveyObjectKind::kSupportSet);
  shapes.objects.push_back(t1_card);
  const auto shape_doc = nlohmann::json::parse(SurveyResultToJson(SurveyResult(shapes), "test"));
  const auto& refused = shape_doc.at("objects")[0].at("witness");
  EXPECT_EQ(refused.size(), 2u);  // {state, mode} — the whole shape, like the tint's refusal
  EXPECT_EQ(refused.at("state"), "refused");
  EXPECT_EQ(refused.at("mode"), "no_declared_source_witness");
  EXPECT_FALSE(shape_doc.at("objects")[1].contains("witness"));
}

// ============================================================================================
// The cooperative-stop pins. The leg's lattice pass was already stoppable (the post-lattice
// check refuses a cut pass — pinned here at both entries: an expired deadline and a ledger
// handshake cancel); the post-lattice phase boundary gains its own named poll (coverage
// pinned), and an unstopped run stays byte-identical to the pre-change golden.
// ============================================================================================

// Bounded entry wait on the poll ledger (the handshake basis — the counter is the entry
// signal, the bound the failure path).
bool WaitForWitnessPoll(const analytic::StopToken& token, int site, std::chrono::milliseconds bound) {
  const auto end = std::chrono::steady_clock::now() + bound;
  while (token.PollCount(site) == 0) {
    if (std::chrono::steady_clock::now() > end) {
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return true;
}

// The pre-change golden document (captured before the post-lattice phase poll landed; see the
// byte-identity pin below). Filled from the dump run.
constexpr const char* kGoldenWitnessDocument =
    R"JSON({"schema":"lumice.path-survey","schema_version":4,"generator":{"lumice":"test","analytic_api_version":8},"request":{"crystal_id":0,"path_layers":[],"symmetry_bits":7,"wavelength_nm":null,"budget_ms":0},"scope":{"scene_identity":"","scene_layer":null,"members":[],"wavelength_nm":0.0,"wavelength_source":"","refractive_index":0.0,"sun_altitude_deg":null,"sun_azimuth_deg":null},"routing":{"early_stopped":false,"members":[]},"objects":[{"kind":"support_set","members":[],"evidence":{"state":"unevaluated","reason":"not_requested"},"witness":{"state":"finite","coverage":{"members_evaluated":1,"geometry_basis":"the declared-source lattice argmax pose, re-evaluated by the diagnostic batch on the nominal crystal shape","wavelength_nm":550.0},"kept_count":16558,"sample_count":200000,"lattice":"antipodal_fibonacci","deterministic":true,"deviation_deg":153.0820618701839,"weight":0.06443543134543582,"entry":0.07207003219060117,"interface_product":0.8940669150115771,"source_note":"declared source: the antipodal Fibonacci lattice over SO(3) (the declared measure's own sampler); witness = the max-weight kept event's pose; kept_count is an existence count, weight the lattice's w = A*T, entry and interface_product the diagnostic batch's own re-evaluation at that pose"}}],"unevaluated":[],"warm_index":{"seeds":[],"seed_sources":[],"reuse_scope":"","produced_for":"","config_fingerprint":""},"chromatic":{"state":"unevaluated","reason":"not_requested"},"evidence":{"state":"unevaluated","reason":"not_requested"},"budget":{"requested_ms":0,"enforced":false,"note":"the v1 survey's only computation is the millisecond T0 routing leg, which is no budget consumer; budget enforcement lands with the band-sum/T3 quantification legs"},"timing":{"total_seconds":0.0}})JSON";

// An already-expired deadline refuses before the argmax: BuildEvents' own polls cut the
// lattice at its first boundary, the leg's post-lattice check names the refusal — a cut
// lattice claims no witness.
TEST(SurveyWitnessStop, ExpiredDeadlineRefusesBeforeTheArgmax) {
  const WitnessFixture fix = WitnessFixture::Make();
  analytic::DiscoveryStop stop;
  stop.deadline = std::chrono::steady_clock::now() - std::chrono::seconds(1);
  quantity::WitnessInput input = fix.Input(50000);
  input.stop = &stop;
  SurveyObject card;
  card.SetKind(SurveyObjectKind::kSupportSet);
  AssembleWitness(input, &card);
  ASSERT_TRUE(card.Witness() != nullptr);
  EXPECT_EQ(card.Witness()->State(), EvidenceState::kRefused);
  EXPECT_EQ(card.Witness()->Mode(), "stop_requested");
}

// The lattice handshake: entered (the lattice's own poll site moved), cancelled from this
// thread, and the leg returns the named refusal within a bounded window.
TEST(SurveyWitnessStop, LatticeCancelStopsTheLeg) {
  const WitnessFixture fix = WitnessFixture::Make();
  analytic::StopToken token;
  analytic::DiscoveryStop stop;
  stop.token = &token;
  quantity::WitnessInput input = fix.Input(1000000);
  input.stop = &stop;
  SurveyObject card;
  card.SetKind(SurveyObjectKind::kSupportSet);
  std::atomic<bool> done{ false };
  std::thread worker([&] {
    AssembleWitness(input, &card);
    done = true;
  });
  const auto join_bound = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  const bool entered = WaitForWitnessPoll(token, analytic::kPollEventsLattice, std::chrono::seconds(30));
  token.RequestCancel();  // safe either way: entered, or the run already finished
  while (!done && std::chrono::steady_clock::now() < join_bound) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  ASSERT_TRUE(done) << "the leg ignored the cancel (entered=" << entered << ")";
  worker.join();  // joined on every path — a joinable thread's destructor would terminate
  ASSERT_TRUE(entered) << "the lattice pass never polled";
  ASSERT_TRUE(card.Witness() != nullptr);
  EXPECT_EQ(card.Witness()->State(), EvidenceState::kRefused);
  EXPECT_EQ(card.Witness()->Mode(), "stop_requested");
}

// The post-lattice phase boundary is real evidence: its own site moves on an unstopped run
// (the argmax + independent re-evaluation section's poll point).
TEST(SurveyWitnessStop, ThePostLatticePhasePollsItsOwnSite) {
  const WitnessFixture fix = WitnessFixture::Make();
  analytic::StopToken token;
  analytic::DiscoveryStop stop;
  stop.token = &token;
  quantity::WitnessInput input = fix.Input(50000);
  input.stop = &stop;
  SurveyObject card;
  card.SetKind(SurveyObjectKind::kSupportSet);
  AssembleWitness(input, &card);
  ASSERT_TRUE(card.Witness() != nullptr);
  EXPECT_EQ(card.Witness()->State(), EvidenceState::kFinite);
  EXPECT_GT(token.PollCount(raypath::kPollSurveyWitnessPhase), 0) << "the post-lattice phase never polled";
}

// The no-stop path's zero-evolution contract at unit scale: an unstopped run serializes
// byte-identically to the pre-change golden.
TEST(SurveyWitnessStop, AnUnstoppedRunIsByteIdenticalToThePreChangeGolden) {
  const WitnessFixture fix = WitnessFixture::Make();
  SurveyObject card;
  card.SetKind(SurveyObjectKind::kSupportSet);
  AssembleWitness(fix.Input(200000), &card);
  ASSERT_TRUE(card.Witness() != nullptr);
  EXPECT_EQ(card.Witness()->State(), EvidenceState::kFinite);
  SurveyData document;
  document.objects.push_back(std::move(card));
  const std::string json = SurveyResultToJson(SurveyResult(document), "test");
  EXPECT_EQ(json, std::string(kGoldenWitnessDocument));
}

// The family arm's refused-corner witness takes the query's stop. This corner is the one
// call path the sibling pins do not reach: the leg-level pins prove EvaluateWitness is
// stoppable and the assembly-level pins prove the refused-support path, but neither touches
// survey_family.cpp's own WitnessInput — the wiring that rides input.stop into the corner is
// one line, and if a refactor drops it the corner's lattice runs unpolled again with nothing
// red. The handshake: enter the corner's lattice (its ledger site), cancel, and the document
// must carry the named refusal AND the refused-support truncation sentence (which the wiring
// feeds); an unwired corner produces a finite witness and no sentence.
TEST(SurveyWitnessStop, FamilyRefusedCornerWitnessTakesTheQueryStop) {
  ConfigManager config = RegularPrismSceneWithSun(15.0);
  CrystalConfig& crystal = config.crystals_.find(1)->second;
  crystal.axis_.latitude_dist = { DistributionType::kGaussian, 90, 0.3f };
  // The scene entry carries its own copy of the crystal; keep the two in step.
  for (auto& setting : config.scene_.ms_.front().setting_) {
    setting.crystal_.axis_.latitude_dist = crystal.axis_.latitude_dist;
  }
  SurveyRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 1, 5, 7 } };
  request.symmetry_bits = 0;
  request.budget_ms = kMaxSurveyBudgetMs;  // the deadline must NOT fire; the cancel is the stop
  analytic::StopToken token;
  analytic::DiscoveryStop stop;
  stop.deadline = std::chrono::steady_clock::time_point::max();
  stop.token = &token;
  SurveyAssemblyOptions options;
  options.stop = &stop;
  SurveyData document;
  std::atomic<bool> done{ false };
  std::thread worker([&] {
    (void)AssembleSurveyData(config, request, &document, options);
    done = true;
  });
  const auto join_bound = std::chrono::steady_clock::now() + std::chrono::seconds(60);
  const bool entered = WaitForWitnessPoll(token, analytic::kPollEventsLattice, std::chrono::seconds(30));
  token.RequestCancel();  // safe either way: entered, or the run already finished
  while (!done && std::chrono::steady_clock::now() < join_bound) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  ASSERT_TRUE(done) << "the family arm ignored the cancel (entered=" << entered << ")";
  worker.join();  // joined on every path — a joinable thread's destructor would terminate
  ASSERT_TRUE(entered) << "the corner's witness lattice never polled";
  const SurveyObject* witness_card = nullptr;
  for (const SurveyObject& card : document.objects) {
    if (card.Witness() != nullptr) {
      witness_card = &card;
      break;
    }
  }
  ASSERT_TRUE(witness_card != nullptr) << "the refused corner never assembled the witness card";
  EXPECT_EQ(witness_card->Witness()->State(), EvidenceState::kRefused);
  EXPECT_EQ(witness_card->Witness()->Mode(), "stop_requested");
  EXPECT_NE(document.budget_note.find(" The witness lattice leg stopped at a cooperative poll"), std::string::npos)
      << "the refused-support note names the corner's stop";
}

}  // namespace
}  // namespace lumice::raypath::survey
