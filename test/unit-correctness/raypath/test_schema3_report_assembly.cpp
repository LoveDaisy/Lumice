// The schema3 report assembly (src/raypath/detail/schema3/report_assembly.{hpp,cpp}): the
// production wiring of the schema3 modules over a REAL v2 report (scrum 666.3 Step 1). The
// living cases promote the mc_attribution tests' verified RunLiving shape to the production
// entry — the enumeration now runs with its declared measure and density sides (the orbit
// streams, the restricted leg, the 33-row n-continuation table) instead of the naked core —
// and pin the assembly's own faces: the annotation parallelism, the budget legs, the two
// declared skips (degenerate sun, unsupported density), and the A4 cost baseline.

#include <gtest/gtest.h>

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "raypath/detail/path_feature_report.hpp"
#include "raypath/detail/schema3/mc_evidence.hpp"
#include "raypath/detail/schema3/report_assembly.hpp"
#include "raypath/detail/schema3/structure_object.hpp"

namespace lumice::raypath {
using schema3::AssembledSchema3Report;
using schema3::AssembleSchema3Report;
using schema3::ConstantDeltaCurve;
using schema3::CorroborationState;
using schema3::kMcObservationScopeNote;
using schema3::MemberSupport;
using schema3::ObjectKind;
namespace {

// The Haar prism scene (the living tests' shape): crystal 1 (h = 1, all face distances 1),
// full-sphere Haar axis. The spectrum shape is the caller's.
ConfigManager PrismScene(bool continuous_spectrum, float sun_altitude_deg = 20) {
  PrismCrystalParam prism;
  prism.h_ = { DistributionType::kNoRandom, 1, 0 };
  for (auto& distance : prism.d_) {
    distance = { DistributionType::kNoRandom, 1, 0 };
  }
  CrystalConfig crystal{};
  crystal.id_ = 1;
  crystal.param_ = prism;
  crystal.axis_.latitude_dist = { DistributionType::kUniform, 90, 360 };
  crystal.axis_.azimuth_dist = crystal.axis_.roll_dist = { DistributionType::kUniform, 0, 360 };
  ConfigManager config;
  config.crystals_.emplace(1, crystal);
  config.scene_.light_source_.param_ = { sun_altitude_deg, 0, 0 };
  config.scene_.light_source_.spectrum_ =
      continuous_spectrum ? SpectrumConfig(IlluminantType::kD65) : SpectrumConfig(std::vector<WlParam>{ { 550, 1 } });
  ScatteringSetting entry{};
  entry.crystal_ = crystal;
  entry.crystal_proportion_ = 1;
  config.scene_.ms_.push_back({ 0, { entry } });
  return config;
}

// The C12 plate scene (h = 1, fd = [1.5, 1, 1, 1.5, 1, 1]), plate axis with a Gaussian zenith
// (mean 0, std 1 — the form ConvertAxisToPoseDensity accepts; a FIXED zenith plate is the
// declared density-skip shape and reads no restricted leg), sun 9 deg.
ConfigManager PlateScene() {
  PrismCrystalParam prism;
  prism.h_ = { DistributionType::kNoRandom, 1, 0 };
  const double fd[6] = { 1.5, 1, 1, 1.5, 1, 1 };
  for (int i = 0; i < 6; i++) {
    prism.d_[i] = { DistributionType::kNoRandom, static_cast<float>(fd[i]), 0 };
  }
  CrystalConfig crystal{};
  crystal.id_ = 1;
  crystal.param_ = prism;
  // The plate family: azimuth uniform, latitude and roll locked.
  crystal.axis_.latitude_dist = { DistributionType::kGaussian, 90, 1 };
  crystal.axis_.azimuth_dist = { DistributionType::kUniform, 0, 360 };
  crystal.axis_.roll_dist = { DistributionType::kUniform, 0, 360 };  // the v1 plate is free-roll
  ConfigManager config;
  config.crystals_.emplace(1, crystal);
  config.scene_.light_source_.param_ = { 9, 0, 0 };
  config.scene_.light_source_.spectrum_ = SpectrumConfig(std::vector<WlParam>{ { 550, 1 } });
  ScatteringSetting entry{};
  entry.crystal_ = crystal;
  entry.crystal_proportion_ = 1;
  config.scene_.ms_.push_back({ 0, { entry } });
  return config;
}

// One real report, assembled in full by the production entry under test.
AssembledSchema3Report AssembleOrDie(const ConfigManager& config, const PathFeatureReportRequest& request) {
  PathFeatureReport report;
  const Error error = AssemblePathFeatureReport(config, request, &report);
  EXPECT_TRUE(error.Ok()) << error.message;
  if (!error.Ok()) {
    return {};
  }
  return AssembleSchema3Report(report, request.max_optical_evaluations);
}

}  // namespace

TEST(Schema3ReportAssembly, ExplicitEventsProduceObservedKind1) {
  // The 666.2 A-arm shape through the production assembly: Haar prism, 550 nm, explicit events.
  // The MC finds the minimum-deviation peak (kActual); the kind-1 object of 3-5 derives
  // `observed`; the ruling cannot issue (a lit, witnessed object breaks the unlit-or-none arm).
  // budget_ms sits at the cap so the wall clock never bites (the replayability guard).
  const ConfigManager config = PrismScene(false);
  PathFeatureReportRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 5 } };
  request.sample_count = 16384;
  request.wavelengths_nm = { 550 };
  request.budget_ms = 120000;
  request.max_optical_evaluations = 4000000;
  const AssembledSchema3Report assembled = AssembleOrDie(config, request);
  ASSERT_FALSE(assembled.core.objects.empty());
  ASSERT_EQ(assembled.annotations.size(), assembled.core.objects.size());

  // The demoted evidence keeps the v2 classification; the peak clears the significance bar.
  size_t actuals = 0;
  for (const DiagnosticFeatureRecord& record : assembled.mc.discovery.features) {
    actuals += record.evidence == DiagnosticEvidence::kActual ? 1 : 0;
  }
  ASSERT_GT(actuals, 0u) << "explicit events must surface at least one kActual on the 3-5 control cell";
  EXPECT_EQ(assembled.mc.observation_options.bandwidth_rad, request.bandwidth_rad);
  EXPECT_EQ(assembled.mc.observation_scope_note, std::string(kMcObservationScopeNote));
  EXPECT_FALSE(assembled.mc.spectral_verification.available) << "an explicit single wavelength has nothing to verify";

  bool kind1_observed = false;
  for (size_t i = 0; i < assembled.core.objects.size(); i++) {
    if (assembled.core.objects[i].kind != ObjectKind::kKind1) {
      continue;
    }
    if (assembled.annotations[i].state == CorroborationState::kObserved) {
      kind1_observed = true;
      EXPECT_GE(assembled.annotations[i].matched_record, 0);
      EXPECT_LT(assembled.annotations[i].matched_record,
                static_cast<long long>(assembled.mc.discovery.features.size()));
      EXPECT_FALSE(assembled.annotations[i].ruler.empty());
    }
  }
  EXPECT_TRUE(kind1_observed) << "the kind-1 critical delta must be observed at explicit-event standing";

  // The rule-B arm the observed object breaks: the certificate cannot issue.
  ASSERT_FALSE(assembled.ruling.issued);
  EXPECT_FALSE(assembled.ruling.all_objects_unlit_or_none);
  EXPECT_NE(assembled.ruling.absence_not_proven_note.find("does not prove absence"), std::string::npos);
  EXPECT_TRUE(assembled.ruling.two_d_valid_support) << "the Haar axis is a kArea measure";

  // The budget and timing legs the report block will serialize.
  EXPECT_EQ(assembled.core.budget.sampling_evaluations, 0)
      << "the Haar axis is kRandom: no orbit circle exists, the stream-less shape is the declared v1 form";
  EXPECT_EQ(assembled.core.budget.max_optical_evaluations, request.max_optical_evaluations);
  EXPECT_GE(assembled.enumeration_seconds, 0.0);
  EXPECT_GE(assembled.attribution_seconds, 0.0);
  EXPECT_TRUE(assembled.measure_skip_note.empty());
  EXPECT_TRUE(assembled.density_skip_note.empty());
}

TEST(Schema3ReportAssembly, AutoStarveStaysSilentAndCarries33RowContinuation) {
  // The 666.2 B-arm shape through the production assembly: continuous illuminant, auto budget.
  // The starvation is deterministic (the weighted field never reaches standing): no kActual, no
  // unattributed output, and the kind-1 (unproven at this budget's measure) reads `consistent`
  // on sufficient recorded presence. The same run carries the 33-row n-continuation table: the
  // constant-D_P curves were re-read at every spectral node.
  const ConfigManager config = PrismScene(true);
  PathFeatureReportRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 5 } };
  request.sample_count = 0;    // auto
  request.budget_ms = 120000;  // the evaluations cap, not the clock, must end this run
  const AssembledSchema3Report assembled = AssembleOrDie(config, request);
  ASSERT_EQ(assembled.annotations.size(), assembled.core.objects.size());
  for (const DiagnosticFeatureRecord& record : assembled.mc.discovery.features) {
    EXPECT_NE(record.evidence, DiagnosticEvidence::kActual) << "the starvation premise must hold";
  }
  EXPECT_TRUE(assembled.unattributed.structures.empty());
  EXPECT_EQ(assembled.unattributed.skipped_below_ess, 0);
  EXPECT_EQ(assembled.mc.spectral_verification.available, true) << "a continuous quadrature is the in-scope signal";
  bool consistent_kind1 = false;
  for (size_t i = 0; i < assembled.core.objects.size(); i++) {
    if (assembled.core.objects[i].kind == ObjectKind::kKind1 &&
        assembled.annotations[i].state == CorroborationState::kConsistent) {
      consistent_kind1 = true;
    }
  }
  EXPECT_TRUE(consistent_kind1) << "unproven kind-1 on sufficient presence reads consistent";
}

TEST(Schema3ReportAssembly, ContinuousSpectrumFeedsThe33RowContinuationTable) {
  // The n-continuation leg at production resolution: the beta plate (the C05/C06 crystal)
  // under the D65 quadrature. The assembly builds the (nm, index) table from the assembled
  // spectrum rows and hands it to the enumeration; the basal TIR kink's constant circle comes
  // back re-read at all 33 nodes with the corpus's 149.247 deg anchor at 550.
  PrismCrystalParam prism;
  prism.h_ = { DistributionType::kNoRandom, 3, 0 };
  const double fd[6] = { 2.0, 1.0, 1.0, 2.0, 1.0, 1.0 };
  for (int i = 0; i < 6; i++) {
    prism.d_[i] = { DistributionType::kNoRandom, static_cast<float>(fd[i]), 0 };
  }
  CrystalConfig crystal{};
  crystal.id_ = 1;
  crystal.param_ = prism;
  crystal.axis_.latitude_dist = { DistributionType::kGaussian, 90, 1 };
  crystal.axis_.azimuth_dist = { DistributionType::kUniform, 0, 360 };
  crystal.axis_.roll_dist = { DistributionType::kUniform, 0, 360 };
  ConfigManager config;
  config.crystals_.emplace(1, crystal);
  config.scene_.light_source_.param_ = { 20, 0, 0 };
  config.scene_.light_source_.spectrum_ = SpectrumConfig(IlluminantType::kD65);
  ScatteringSetting entry{};
  entry.crystal_ = crystal;
  entry.crystal_proportion_ = 1;
  config.scene_.ms_.push_back({ 0, { entry } });

  PathFeatureReportRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 4, 8, 1, 7, 5 } };  // the C06 chain: the physical-member scope
  request.sample_count = 64;                    // follows the chain length; the MC side is not the subject
  request.budget_ms = 120000;
  const AssembledSchema3Report assembled = AssembleOrDie(config, request);
  constexpr double kDeg = 3.14159265358979323846 / 180.0;
  bool found = false;
  for (const MemberSupport& row : assembled.core.support.members) {
    for (const ConstantDeltaCurve& curve : row.constant_curves) {
      if (curve.wavelengths_nm.size() != 33u || curve.wavelengths_nm.size() != curve.critical_d_p.size()) {
        ADD_FAILURE() << "the D65 dyadic quadrature is 33 nodes, parallel to the critical values";
        continue;
      }
      // The 33-node grid has no exact 550 nm node; the nodes bracketing it carry the corpus's
      // 149.247 deg anchor within the dispersion slope's own width (~0.05 deg across the pair).
      for (size_t j = 0; j < curve.wavelengths_nm.size(); j++) {
        if (curve.wavelengths_nm[j] > 540.0 && curve.wavelengths_nm[j] < 560.0) {
          EXPECT_NEAR(curve.critical_d_p[j] / kDeg, 149.2468, 0.2);
          found = true;
        }
      }
    }
  }
  EXPECT_TRUE(found) << "the basal TIR kink's constant circle must ride the continuation table";
}

TEST(Schema3ReportAssembly, PlateRestrictedCarriesItsAnnotation) {
  // The 666.2 C-arm shape through the production assembly: the C12 plate's fully-dark
  // restricted member (the C09 shape) at real MC standing. The pinned member is 2-4-5-1 — the
  // measured family-pinned shape on this crystal at sun 9 deg (the living C arm's premise).
  // The production measure side changes the stream resolution (grid 720, all members), not the
  // arm: unlit restricted, zero recorded presence, not_observed_insufficient_ess.
  const ConfigManager config = PlateScene();
  PathFeatureReportRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 2, 4, 5, 1 } };
  request.sample_count = 65536;
  request.wavelengths_nm = { 550 };
  request.budget_ms = 120000;
  const AssembledSchema3Report assembled = AssembleOrDie(config, request);
  ASSERT_EQ(assembled.annotations.size(), assembled.core.objects.size());
  EXPECT_TRUE(assembled.measure_skip_note.empty());
  EXPECT_TRUE(assembled.density_skip_note.empty()) << "the Gaussian-zenith plate converts; the leg must run";
  bool restricted_found = false;
  for (size_t i = 0; i < assembled.core.objects.size(); i++) {
    if (assembled.core.objects[i].kind != ObjectKind::kKind1Restricted) {
      continue;
    }
    restricted_found = true;
    EXPECT_EQ(assembled.core.objects[i].visibility.state, VisibilityState::kUnlit)
        << "the C09 premise: the pinned member is fully dark on its declared orbit";
    EXPECT_EQ(assembled.annotations[i].state, CorroborationState::kNotObservedInsufficientEss)
        << assembled.annotations[i].reason;
    EXPECT_DOUBLE_EQ(assembled.annotations[i].presence_ess, 0.0);
  }
  EXPECT_TRUE(restricted_found) << "2-4-5-1 must be family-pinned on the plate";
}

TEST(Schema3ReportAssembly, DeclaredSkipsCoverDegenerateSunAndUnsupportedDensity) {
  // Skip arm 1 — the sun at a pole: MakeUMarginal reports kDegenerateSunGeometry, the assembly
  // skips the measure side whole (no streams, kind-1 unproven) and declares the skip; the rule-B
  // gate reads the non-kArea kind and refuses to issue.
  {
    const ConfigManager config = PrismScene(false, 90);
    PathFeatureReportRequest request;
    request.crystal_id = 1;
    request.path_layers = { { 3, 5 } };
    request.sample_count = 64;
    request.wavelengths_nm = { 550 };
    request.budget_ms = 120000;
    const AssembledSchema3Report assembled = AssembleOrDie(config, request);
    EXPECT_FALSE(assembled.measure_skip_note.empty());
    EXPECT_TRUE(assembled.density_skip_note.empty()) << "the Haar axis converts fine; only the sun is degenerate";
    EXPECT_EQ(assembled.core.budget.sampling_evaluations, 0) << "no streams without a measure";
    EXPECT_FALSE(assembled.core.objects.empty());
    for (size_t i = 0; i < assembled.core.objects.size(); i++) {
      if (assembled.core.objects[i].kind == ObjectKind::kKind1) {
        EXPECT_EQ(assembled.core.objects[i].visibility.state, VisibilityState::kUnproven);
        // The corroboration judgment still consults MC presence (the record side is real);
        // only the OBJECT's visibility is the skip's fail-closed shape. Both not-observed arms
        // are legitimate here; pin neither.
        EXPECT_TRUE(assembled.annotations[i].state == CorroborationState::kConsistent ||
                    assembled.annotations[i].state == CorroborationState::kNotObservedInsufficientEss)
            << assembled.annotations[i].reason;
      }
    }
    EXPECT_FALSE(assembled.ruling.issued);
    EXPECT_FALSE(assembled.ruling.two_d_valid_support) << "the degenerate kind is not a kArea support";
  }
  // Skip arm 2 — an axis distribution the v1 density conversion cannot express (azimuth not
  // uniform over the full turn): the density leg is declared skipped, the measure still builds,
  // and the enumeration runs stream-less (kind-1 unproven, no restricted leg).
  {
    PrismCrystalParam prism;
    prism.h_ = { DistributionType::kNoRandom, 1, 0 };
    for (auto& distance : prism.d_) {
      distance = { DistributionType::kNoRandom, 1, 0 };
    }
    CrystalConfig crystal{};
    crystal.id_ = 1;
    crystal.param_ = prism;
    crystal.axis_.latitude_dist = { DistributionType::kUniform, 90, 360 };
    crystal.axis_.azimuth_dist = { DistributionType::kUniform, 0, 180 };  // not the full turn
    crystal.axis_.roll_dist = { DistributionType::kUniform, 0, 360 };
    ConfigManager config;
    config.crystals_.emplace(1, crystal);
    config.scene_.light_source_.param_ = { 20, 0, 0 };
    config.scene_.light_source_.spectrum_ = SpectrumConfig(std::vector<WlParam>{ { 550, 1 } });
    ScatteringSetting entry{};
    entry.crystal_ = crystal;
    entry.crystal_proportion_ = 1;
    config.scene_.ms_.push_back({ 0, { entry } });

    PathFeatureReportRequest request;
    request.crystal_id = 1;
    request.path_layers = { { 3, 5 } };
    request.sample_count = 64;
    request.wavelengths_nm = { 550 };
    request.budget_ms = 120000;
    const AssembledSchema3Report assembled = AssembleOrDie(config, request);
    EXPECT_TRUE(assembled.measure_skip_note.empty());
    EXPECT_NE(assembled.density_skip_note.find("not expressible"), std::string::npos) << assembled.density_skip_note;
    EXPECT_EQ(assembled.core.budget.sampling_evaluations, 0) << "no streams without a density spec";
    EXPECT_FALSE(assembled.core.objects.empty());
  }
}

TEST(Schema3ReportAssembly, CostBaselineThreeStandardScenarios) {
  // The A4 initial judgment (666.3 plan): the assembly module's absolute cost on the three
  // standard scenarios, printed for the task progress and the later full-path comparison. The
  // cap keeps a pathological slowdown visible instead of hanging.
  struct Scenario {
    const char* name;
    ConfigManager config;
    std::vector<int> path;
  };
  std::vector<Scenario> scenarios;
  scenarios.push_back({ "prism-3-5", PrismScene(false), { 3, 5 } });
  scenarios.push_back({ "prism-3-1-5", PrismScene(false), { 3, 1, 5 } });
  scenarios.push_back({ "plate-1-3-4-2", PlateScene(), { 1, 3, 4, 2 } });
  for (const Scenario& scenario : scenarios) {
    PathFeatureReportRequest request;
    request.crystal_id = 1;
    request.path_layers = { scenario.path };
    request.sample_count = 8192;
    request.wavelengths_nm = { 550 };
    request.budget_ms = 120000;
    const AssembledSchema3Report assembled = AssembleOrDie(scenario.config, request);
    if (assembled.annotations.size() != assembled.core.objects.size()) {
      ADD_FAILURE() << "annotations must stay parallel to the objects for " << scenario.name;
      continue;
    }
    std::cout << "[ASSEMBLY-COST] scenario=" << scenario.name << " members=" << assembled.core.support.members.size()
              << " objects=" << assembled.core.objects.size() << " enumeration_s=" << assembled.enumeration_seconds
              << " attribution_s=" << assembled.attribution_seconds
              << " sampling_evaluations=" << assembled.core.budget.sampling_evaluations << " measure_note=["
              << assembled.measure_skip_note << "] density_note=[" << assembled.density_skip_note << "]" << std::endl;
  }
}

}  // namespace lumice::raypath
