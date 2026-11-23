// AnalysisWorkspace (the typed-architecture design's data-and-reuse owner): the S4 acceptance
// reads — lazy creation, build-once geometry per current selection, the bounded slot occupancy
// under selection switching, the typed survey->point warm chain (no JSON, no recompute), the
// cancelled-block rejection, and the results' independence from the caller's config, the scene
// and the workspace itself.
#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <thread>

#include "config/config_manager.hpp"
#include "core/crystal_param.hpp"
#include "raypath/detail/survey/survey_assembly.hpp"
#include "raypath/detail/survey/survey_probe.hpp"
#include "raypath/prepared_geometry.hpp"
#include "raypath/probe_analysis.hpp"
#include "raypath/query_control.hpp"
#include "raypath/single_path_analysis.hpp"
#include "raypath/single_path_json.hpp"
#include "raypath/survey_report.hpp"
#include "raypath/survey_reuse.hpp"
#include "raypath/workspace.hpp"
#include "survey_test_fixtures.hpp"

namespace lumice::raypath {
namespace {

SurveyRequest SurveyOf(std::vector<int> faces, std::optional<double> lambda = std::nullopt) {
  SurveyRequest request;
  request.crystal_id = 1;
  request.path_layers = { std::move(faces) };
  request.wavelength_nm = lambda;
  request.budget_ms = 120000;
  return request;
}

SinglePathRequest PointOf(std::vector<int> faces) {
  SinglePathRequest request;
  request.crystal_id = 1;
  request.path_layers = { std::move(faces) };
  request.target_altitude_deg = 30.0;
  request.target_azimuth_deg = 90.0;
  request.sample_count = 2000;
  request.sun_grid_lat_count = 0;
  request.budget_ms = 120000;
  return request;
}

// A two-crystal scene: both entries are the corpus column shape, so switching selections
// between them exercises the slot-replacement (the occupancy read) without new physics.
ConfigManager TwoCrystalScene() {
  ConfigManager config = survey::RandomPrismScene();
  CrystalConfig second = config.crystals_.at(1);
  second.id_ = 2;
  config.crystals_.emplace(2, second);
  return config;
}

TEST(Workspace, QueryDeadlinesResolveOnceWithoutChangingLegacyWindows) {
  std::unique_ptr<AnalysisWorkspace> workspace;
  ASSERT_TRUE(AnalysisWorkspace::Create(survey::RandomPrismScene(), &workspace).Ok());
  const long queries = QueryDeadlineResolutionCount();
  const long point_windows = BudgetResolutionCount(BudgetWindow::kPointDiscovery);
  const long survey_windows = BudgetResolutionCount(BudgetWindow::kLegGuardrail);

  SinglePathRequest point_request = PointOf({ 1, 2 });
  point_request.budget_ms = 0;
  PointResult point;
  ASSERT_TRUE(workspace->QueryPoint(point_request, false, &point).Ok());
  EXPECT_EQ(QueryDeadlineResolutionCount(), queries + 1);
  EXPECT_EQ(BudgetResolutionCount(BudgetWindow::kPointDiscovery), point_windows);
  EXPECT_EQ(point.Data().meta.budget_ms, 0);

  SurveyRequest survey_request = SurveyOf({ 1, 2 });
  survey_request.budget_ms = 0;
  survey::SurveyResult survey;
  ASSERT_TRUE(workspace->QuerySurvey(survey_request, &survey).Ok());
  EXPECT_EQ(survey.Data().execution, ExecutionOutcome::kCompleted);
  EXPECT_EQ(QueryDeadlineResolutionCount(), queries + 2);
  EXPECT_EQ(BudgetResolutionCount(BudgetWindow::kLegGuardrail), survey_windows);

  analytic::StopToken token;
  token.RequestCancel();
  survey::SurveyResult cut;
  const long before_cut = workspace->StructureRunCount();
  ASSERT_TRUE(workspace->QuerySurvey(survey_request, &cut, &token).Ok());
  EXPECT_EQ(cut.Data().execution, ExecutionOutcome::kCancelled);
  EXPECT_EQ(QueryDeadlineResolutionCount(), queries + 3);
  EXPECT_GT(workspace->StructureRunCount(), before_cut);

  ProbeRequest probe_request;
  probe_request.crystal_id = 1;
  probe_request.path_layers = { { 1, 2 } };
  probe_request.at_altitude_deg = -30.0;
  probe_request.sample_count = 100;
  probe_request.budget_ms = 0;
  const long probe_windows = BudgetResolutionCount(BudgetWindow::kProbeLambdas);
  ProbeResult probe;
  ASSERT_TRUE(workspace->QueryProbe(probe_request, &probe).Ok());
  EXPECT_EQ(QueryDeadlineResolutionCount(), queries + 4);
  EXPECT_EQ(BudgetResolutionCount(BudgetWindow::kProbeLambdas), probe_windows);

  PathFeatureReportRequest report_request;
  report_request.crystal_id = 1;
  report_request.path_layers = { { 1, 2 } };
  report_request.sample_count = 64;
  report_request.budget_ms = 0;
  const long quality_windows = BudgetResolutionCount(BudgetWindow::kQualityDeadline);
  const long hang_windows = BudgetResolutionCount(BudgetWindow::kHangCap);
  FeatureReportResult report;
  ASSERT_TRUE(workspace->QueryReport(report_request, &report).Ok());
  EXPECT_EQ(QueryDeadlineResolutionCount(), queries + 5);
  EXPECT_EQ(BudgetResolutionCount(BudgetWindow::kQualityDeadline), quality_windows);
  EXPECT_EQ(BudgetResolutionCount(BudgetWindow::kHangCap), hang_windows);
}

TEST(Workspace, McEssInUnifiedQueryUsesEntryDeadlineNotLegacyGuardrail) {
  std::unique_ptr<AnalysisWorkspace> workspace;
  ASSERT_TRUE(AnalysisWorkspace::Create(survey::RandomPrismScene(), &workspace).Ok());
  SurveyRequest request = SurveyOf({ 3, 5 }, 550.0);
  request.symmetry_bits = 0;
  request.mc_ess_draws = 64;
  request.budget_ms = 0;
  const long queries = QueryDeadlineResolutionCount();
  const long guardrails = BudgetResolutionCount(BudgetWindow::kLegGuardrail);
  survey::SurveyResult result;
  ASSERT_TRUE(workspace->QuerySurvey(request, &result).Ok());
  ASSERT_TRUE(result.Data().mc_region_ess.has_value());
  EXPECT_EQ(result.Data().mc_region_ess->State(), survey::EvidenceState::kFinite);
  EXPECT_EQ(QueryDeadlineResolutionCount(), queries + 1);
  EXPECT_EQ(BudgetResolutionCount(BudgetWindow::kLegGuardrail), guardrails);
}

TEST(Workspace, SurveyDeadlineIsFrozenAsAnExecutionOutcome) {
  std::unique_ptr<AnalysisWorkspace> workspace;
  ASSERT_TRUE(AnalysisWorkspace::Create(survey::RandomPrismScene(), &workspace).Ok());
  SurveyRequest request = SurveyOf({ 3, 5 }, 550.0);
  request.symmetry_bits = 0;
  request.mc_ess_draws = 64;
  request.budget_ms = 1;
  analytic::StopToken token;
  survey::SurveyResult result;
  ASSERT_TRUE(workspace->QuerySurvey(request, &result, &token).Ok());
  EXPECT_GT(token.PollCount(kPollSurveyT0), 0);
  EXPECT_EQ(result.Data().execution, ExecutionOutcome::kDeadlineExceeded);
}

TEST(Workspace, SurveyWitnessObservesQueryCancelBeforeCompletingItsLattice) {
  std::unique_ptr<AnalysisWorkspace> workspace;
  ASSERT_TRUE(AnalysisWorkspace::Create(survey::RegularPrismSceneWithSun(15.0), &workspace).Ok());
  SurveyRequest request = SurveyOf({ 3, 1, 5, 7 }, 550.0);
  request.symmetry_bits = 0;
  request.budget_ms = 0;
  analytic::StopToken token;
  survey::SurveyResult result;
  Error error;
  std::atomic<bool> done{ false };
  std::thread worker([&] {
    error = workspace->QuerySurvey(request, &result, &token);
    done = true;
  });
  const auto bound = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (token.PollCount(analytic::kPollEventsLattice) == 0 && !done && std::chrono::steady_clock::now() < bound) {
    std::this_thread::yield();
  }
  const bool entered_witness = token.PollCount(analytic::kPollEventsLattice) > 0;
  token.RequestCancel();
  worker.join();
  ASSERT_TRUE(error.Ok());
  ASSERT_TRUE(entered_witness) << "the survey witness never entered its lattice";
  EXPECT_EQ(result.Data().execution, ExecutionOutcome::kCancelled);
  bool found_witness = false;
  for (const auto& object : result.Data().objects) {
    if (const auto* witness = object.Witness()) {
      found_witness = true;
      EXPECT_EQ(witness->State(), survey::EvidenceState::kRefused);
      EXPECT_EQ(witness->Mode(), "stop_requested");
    }
  }
  EXPECT_TRUE(found_witness);
  const int stray = FirstUnattributedPollSite(token, kPollFaceSurvey);
  EXPECT_EQ(stray, -1) << "the survey run ticked site " << stray << ", outside its attribution";
}

TEST(Workspace, SurveyClassTintObservesQueryCancelBeforeCompletingItsLattice) {
  std::unique_ptr<AnalysisWorkspace> workspace;
  ASSERT_TRUE(AnalysisWorkspace::Create(survey::RandomPrismScene(), &workspace).Ok());
  SurveyRequest request = SurveyOf({ 3, 5 }, 550.0);
  request.symmetry_bits = 0;
  request.budget_ms = 0;
  analytic::StopToken token;
  survey::SurveyResult result;
  Error error;
  std::atomic<bool> done{ false };
  std::thread worker([&] {
    error = workspace->QuerySurvey(request, &result, &token);
    done = true;
  });
  const auto bound = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (token.PollCount(analytic::kPollEventsLattice) == 0 && !done && std::chrono::steady_clock::now() < bound) {
    std::this_thread::yield();
  }
  const bool entered_tint = token.PollCount(analytic::kPollEventsLattice) > 0;
  token.RequestCancel();
  worker.join();
  ASSERT_TRUE(error.Ok());
  ASSERT_TRUE(entered_tint) << "the full-dimensional tint never entered its lattice";
  ASSERT_TRUE(result.Data().weight_tint.has_value());
  EXPECT_EQ(result.Data().weight_tint->State(), survey::EvidenceState::kRefused);
  EXPECT_EQ(result.Data().weight_tint->Mode(), "stop_requested");
  EXPECT_EQ(result.Data().execution, ExecutionOutcome::kCancelled);
  const int stray = FirstUnattributedPollSite(token, kPollFaceSurvey);
  EXPECT_EQ(stray, -1) << "the tint run ticked site " << stray << ", outside its attribution";
}

TEST(Workspace, SurveyChromaticObservesQueryCancelBeforePairFinishes) {
  std::unique_ptr<AnalysisWorkspace> workspace;
  ASSERT_TRUE(AnalysisWorkspace::Create(survey::RandomPrismScene(), &workspace).Ok());
  SurveyRequest request = SurveyOf({ 3, 5 }, 550.0);
  request.symmetry_bits = 0;
  request.budget_ms = 0;
  analytic::StopToken token;
  survey::SurveyResult result;
  Error error;
  std::atomic<bool> done{ false };
  std::thread worker([&] {
    error = workspace->QuerySurvey(request, &result, &token);
    done = true;
  });
  const auto bound = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (token.PollCount(kPollSurveyChromatic) == 0 && !done && std::chrono::steady_clock::now() < bound) {
    std::this_thread::yield();
  }
  const bool entered_chromatic = token.PollCount(kPollSurveyChromatic) > 0;
  token.RequestCancel();
  worker.join();
  ASSERT_TRUE(error.Ok());
  ASSERT_TRUE(entered_chromatic) << "the full-dimensional survey never entered chromatic analysis";
  EXPECT_EQ(result.Data().execution, ExecutionOutcome::kCancelled);
  EXPECT_EQ(result.Data().chromatic.state, survey::EvidenceState::kRefused);
  EXPECT_EQ(result.Data().chromatic.mode, "stop_requested");
  EXPECT_FALSE(result.Data().diagnose.has_value());
  const int stray = FirstUnattributedPollSite(token, kPollFaceSurvey);
  EXPECT_EQ(stray, -1) << "the chromatic run ticked site " << stray << ", outside its attribution";
}

TEST(Workspace, CreationIsLazyAndOwnsNothingButTheSnapshot) {
  std::unique_ptr<AnalysisWorkspace> workspace;
  ASSERT_TRUE(AnalysisWorkspace::Create(survey::RandomPrismScene(), &workspace).Ok());
  EXPECT_EQ(workspace->GeometryBuildCount(), 0);  // no structure ran, no tables built
  EXPECT_EQ(workspace->StructureRunCount(), 0);
  EXPECT_EQ(workspace->OccupiedSlotCount(), 0);
}

TEST(Workspace, FirstQueryBuildsOnlyTheSelectedAndRequeriesReuse) {
  std::unique_ptr<AnalysisWorkspace> workspace;
  ASSERT_TRUE(AnalysisWorkspace::Create(TwoCrystalScene(), &workspace).Ok());
  survey::SurveyResult first;
  ASSERT_TRUE(workspace->QuerySurvey(SurveyOf({ 1, 2 }, 550.0), &first).Ok());
  EXPECT_EQ(workspace->GeometryBuildCount(), 1);  // the selection's crystal only
  const long runs_after_first = workspace->StructureRunCount();
  EXPECT_GE(runs_after_first, 1);

  survey::SurveyResult again;
  ASSERT_TRUE(workspace->QuerySurvey(SurveyOf({ 1, 2 }, 550.0), &again).Ok());
  EXPECT_EQ(workspace->GeometryBuildCount(), 1);                // same selection: nothing rebuilt
  EXPECT_EQ(workspace->StructureRunCount(), runs_after_first);  // the cached result answered

  analytic::StopToken cancelled_token;
  cancelled_token.RequestCancel();
  survey::SurveyResult cut;
  ASSERT_TRUE(workspace->QuerySurvey(SurveyOf({ 1, 2 }, 550.0), &cut, &cancelled_token).Ok());
  EXPECT_EQ(cut.Data().execution, ExecutionOutcome::kCancelled);
  EXPECT_NE(cut.Identity(), first.Identity());
  const long runs_after_cut = workspace->StructureRunCount();
  EXPECT_GT(runs_after_cut, runs_after_first);
  survey::SurveyResult retained;
  ASSERT_TRUE(workspace->QuerySurvey(SurveyOf({ 1, 2 }, 550.0), &retained).Ok());
  EXPECT_EQ(retained.Identity(), first.Identity());
  EXPECT_EQ(workspace->StructureRunCount(), runs_after_cut);

  // A point on the same selection reuses the geometry slot too.
  PointResult point;
  const Error point_error = workspace->QueryPoint(PointOf({ 1, 2 }), true, &point);
  ASSERT_TRUE(point_error.Ok()) << ErrorCodeName(point_error.code) << ": " << point_error.message;
  EXPECT_EQ(workspace->GeometryBuildCount(), 1);

  // Switching to the second crystal builds exactly the new selection.
  SurveyRequest other = SurveyOf({ 1, 2 }, 550.0);
  other.crystal_id = 2;  // a distinct selection with the same face class: the geometry differs
  survey::SurveyResult second;
  ASSERT_TRUE(workspace->QuerySurvey(other, &second).Ok());
  EXPECT_EQ(workspace->GeometryBuildCount(), 2);  // only the new crystal's tables
}

TEST(Workspace, SlotOccupancyIsBoundedUnderSelectionSwitching) {
  std::unique_ptr<AnalysisWorkspace> workspace;
  ASSERT_TRUE(AnalysisWorkspace::Create(TwoCrystalScene(), &workspace).Ok());
  // Round-robin over distinct selections, twice: the occupied-slot read must stay constant
  // (the slots replace, they never accumulate the switching history).
  const std::vector<std::pair<long long, std::vector<int>>> selections = {
    { 1, { 1, 2 } },
    { 2, { 1, 2 } },
    { 1, { 1, 2 } },
    { 2, { 1, 2 } },
  };  // distinct selections by crystal; the arms stay cheap — the read is the slots, not the physics
  long max_occupied = 0;
  for (int round = 0; round < 2; round++) {
    for (const auto& [crystal, faces] : selections) {
      SurveyRequest request = SurveyOf(faces, 550.0);
      request.crystal_id = crystal;
      survey::SurveyResult result;
      if (const Error error = workspace->QuerySurvey(request, &result); !error.Ok()) {
        ADD_FAILURE() << "survey on crystal " << crystal << " failed: " << error.message;  // keep switching
        continue;
      }
      max_occupied = std::max(max_occupied, workspace->OccupiedSlotCount());
    }
  }
  EXPECT_EQ(workspace->OccupiedSlotCount(), max_occupied);  // no growth with N switches
  EXPECT_LE(workspace->OccupiedSlotCount(), 3);             // the construction bound: 3 slots
}

TEST(Workspace, SurveyToPointIsTheTypedWarmChainWithNoRecompute) {
  std::unique_ptr<AnalysisWorkspace> workspace;
  ASSERT_TRUE(AnalysisWorkspace::Create(survey::RandomPrismScene(), &workspace).Ok());
  survey::SurveyResult survey;
  ASSERT_TRUE(workspace->QuerySurvey(SurveyOf({ 1, 2 }), &survey).Ok());
  const long runs = workspace->StructureRunCount();
  ASSERT_GE(runs, 1);

  PointResult warm_point;
  ASSERT_TRUE(workspace->QueryPoint(PointOf({ 1, 2 }), true, &warm_point).Ok());
  // The zero-recompute read: the point query added no structure run, and the behavioral proof
  // is the result's own context provenance — the warm slot's facts answered the context.
  EXPECT_EQ(workspace->StructureRunCount(), runs);
  EXPECT_EQ(warm_point.Data().survey_context.source, ContextSource::kWarm);

  // A probe on the same selection rides the same slot.
  ProbeRequest probe_request;
  probe_request.crystal_id = 1;
  probe_request.path_layers = { { 1, 2 } };
  probe_request.along = ProbeAlong::kRadial;
  probe_request.at_altitude_deg = -30.0;
  probe_request.at_azimuth_deg = 0.0;
  probe_request.budget_ms = 120000;
  ProbeResult probe;
  ASSERT_TRUE(workspace->QueryProbe(probe_request, &probe).Ok());
  EXPECT_EQ(probe.Data().context.source, ContextSource::kWarm);
  EXPECT_EQ(workspace->StructureRunCount(), runs);
}

TEST(Workspace, AutomaticScopeMismatchIsAMissButExplicitMismatchIsRefused) {
  const ConfigManager config = survey::RandomPrismScene();
  std::unique_ptr<AnalysisWorkspace> workspace;
  ASSERT_TRUE(AnalysisWorkspace::Create(config, &workspace).Ok());
  SurveyRequest literal = SurveyOf({ 1, 2 });
  literal.symmetry_bits = 0;
  survey::SurveyResult survey;
  ASSERT_TRUE(workspace->QuerySurvey(literal, &survey).Ok());
  ASSERT_EQ(survey.Data().execution, ExecutionOutcome::kCompleted);
  ASSERT_EQ(workspace->OccupiedSlotCount(), 3);
  const long runs = workspace->StructureRunCount();

  SinglePathRequest point_request = PointOf({ 1, 2 });  // default physical context
  PointResult point;
  const Error point_error = workspace->QueryPoint(point_request, true, &point);
  ASSERT_TRUE(point_error.Ok()) << point_error.message;
  EXPECT_EQ(point.Data().survey_context.source, ContextSource::kInline);
  EXPECT_GT(workspace->StructureRunCount(), runs);

  ProbeRequest probe_request;
  probe_request.crystal_id = 1;
  probe_request.path_layers = { { 1, 2 } };
  probe_request.at_altitude_deg = -30.0;
  probe_request.sample_count = 100;
  probe_request.budget_ms = 120000;
  ProbeResult probe;
  const Error probe_error = workspace->QueryProbe(probe_request, &probe);
  ASSERT_TRUE(probe_error.Ok()) << probe_error.message;
  EXPECT_EQ(probe.Data().context.source, ContextSource::kInline);

  point_request.warm_candidate = HarvestSurveyResult(survey);
  EXPECT_FALSE(workspace->QueryPoint(point_request, true, &point).Ok());
  probe_request.warm_candidate = HarvestSurveyResult(survey);
  EXPECT_FALSE(workspace->QueryProbe(probe_request, &probe).Ok());

  // The literal automatic slot is compatible now, but cannot override bad explicit input.
  point_request.symmetry_bits = probe_request.symmetry_bits = 0;
  point_request.warm_candidate.reset();
  probe_request.warm_candidate.reset();
  ASSERT_TRUE(workspace->QueryPoint(point_request, true, &point).Ok());
  EXPECT_EQ(point.Data().survey_context.source, ContextSource::kWarm);
  ASSERT_TRUE(workspace->QueryProbe(probe_request, &probe).Ok());
  EXPECT_EQ(probe.Data().context.source, ContextSource::kWarm);
  WarmCandidate wrong = HarvestSurveyResult(survey);
  wrong.config_fingerprint.clear();
  point_request.warm_candidate = probe_request.warm_candidate = wrong;
  EXPECT_FALSE(workspace->QueryPoint(point_request, true, &point).Ok());
  EXPECT_FALSE(workspace->QueryProbe(probe_request, &probe).Ok());
}

TEST(Workspace, BothContextScopesReuseOnlyTheirOwnMembers) {
  // On a regular prism with a full-sphere uniform orientation, P and D fix each basal
  // face; B exchanges the two. Thus reflection off face 1 has exactly these two physical
  // members, independently of the production fingerprint and member-expansion algorithm.
  const std::vector<std::vector<int>> literal_members{ { 1 } };
  const std::vector<std::vector<int>> class_members{ { 1 }, { 2 } };
  const auto sorted = [](std::vector<std::vector<int>> members) {
    std::sort(members.begin(), members.end());
    return members;
  };
  const auto check_scope = [&](unsigned scope) {
    SCOPED_TRACE(scope);
    const ConfigManager config = survey::RandomPrismScene();
    std::unique_ptr<AnalysisWorkspace> workspace;
    ASSERT_TRUE(AnalysisWorkspace::Create(config, &workspace).Ok());
    SurveyRequest survey_request = SurveyOf(literal_members.front());
    survey_request.symmetry_bits = scope;
    // Build the real structure and a small, real probe through their existing producers.
    // This completed value tests explicit consumption, NOT automatic survey-slot insertion;
    // ContextScopePreservesAutomaticExplicitAndLegacyContracts covers both automatic scopes.
    constexpr int kProfileSamples = 20000;
    PreparedGeometry geometry;
    ASSERT_TRUE(PrepareGeometry(config.crystals_.at(1), &geometry).Ok());
    survey::SurveyAssemblyOptions options;
    options.context_only = true;
    options.geometry = &geometry;
    survey::SurveyData data;
    ASSERT_TRUE(survey::AssembleSurveyData(config, survey_request, &data, options).Ok());
    survey::CrystalSceneScope producer_scope;
    ASSERT_TRUE(survey::ResolveCrystalSceneScope(config, survey_request, &producer_scope).Ok());
    survey::ProbeInput producer;
    producer.table = &geometry.table;
    producer.polygons = &geometry.polygons;
    producer.representative = &survey_request.path_layers.front();
    producer.density = &producer_scope.density;
    producer.wavelength_nm = data.wavelength_nm;
    producer.refractive_index = data.refractive_index;
    producer.member_count = static_cast<int>(data.members.size());
    producer.sample_count = kProfileSamples;
    SunIncidentDirection(config.scene_.light_source_.param_, producer.incident);
    survey::RunSolarRadialProbe(producer, &data);
    survey::SurveyResult source(std::move(data));
    ASSERT_EQ(source.Data().execution, ExecutionOutcome::kCompleted);
    ASSERT_EQ(source.Data().probes.size(), 1u);
    ASSERT_EQ(source.Data().probes.front().State(), survey::EvidenceState::kFinite);
    EXPECT_EQ(source.Data().probes.front().spec.sample_count, kProfileSamples);
    const auto& expected = scope == 0 ? literal_members : class_members;
    EXPECT_EQ(sorted(source.Data().members), expected);
    EXPECT_EQ(workspace->OccupiedSlotCount(), 0);
    const long runs = workspace->StructureRunCount();

    SinglePathRequest point_request = PointOf(literal_members.front());
    point_request.symmetry_bits = scope;
    point_request.warm_candidate = HarvestSurveyResult(source);
    PointResult warm_point;
    ASSERT_TRUE(workspace->QueryPoint(point_request, true, &warm_point).Ok());
    EXPECT_EQ(warm_point.Data().meta.execution, ExecutionOutcome::kCompleted);
    EXPECT_EQ(warm_point.Data().members.members, literal_members);
    ASSERT_TRUE(warm_point.Data().survey_context.structure.has_value());
    EXPECT_EQ(sorted(warm_point.Data().survey_context.structure->Members().members), expected);
    EXPECT_EQ(warm_point.Data().survey_context.source, ContextSource::kWarm);
    EXPECT_EQ(workspace->StructureRunCount(), runs);

    ProbeRequest probe_request;
    probe_request.crystal_id = 1;
    probe_request.path_layers = literal_members;
    probe_request.symmetry_bits = scope;
    probe_request.at_altitude_deg = -30;
    probe_request.sample_count = kProfileSamples;
    probe_request.budget_ms = 120000;
    probe_request.warm_candidate = HarvestSurveyResult(source);
    ProbeResult warm_probe;
    ASSERT_TRUE(workspace->QueryProbe(probe_request, &warm_probe).Ok());
    EXPECT_EQ(warm_probe.Data().meta.execution, ExecutionOutcome::kCompleted);
    EXPECT_EQ(warm_probe.Data().context.source, ContextSource::kWarm);
    ASSERT_TRUE(warm_probe.Data().context.structure.has_value());
    EXPECT_EQ(sorted(warm_probe.Data().context.structure->Members().members), expected);
    EXPECT_EQ(workspace->StructureRunCount(), runs);

    // The numerical oracle recomputes its profile independently, but shares the immutable
    // structure: re-solving the same T0/T1 context proves no additional numeric fact.
    // No profile facts are available on this candidate, so WarmReused cannot answer this leg.
    WarmCandidate structure_only = HarvestSurveyResult(source);
    structure_only.facts.has_probe = false;
    structure_only.facts.probe_samples.clear();
    probe_request.warm_candidate = std::move(structure_only);
    std::unique_ptr<AnalysisWorkspace> cold;
    ASSERT_TRUE(AnalysisWorkspace::Create(config, &cold).Ok());
    ProbeResult cold_probe;
    ASSERT_TRUE(cold->QueryProbe(probe_request, &cold_probe).Ok());
    EXPECT_EQ(cold_probe.Data().meta.execution, ExecutionOutcome::kCompleted);
    EXPECT_EQ(cold_probe.Data().context.source, ContextSource::kWarm);
    ASSERT_TRUE(cold_probe.Data().context.structure.has_value());
    EXPECT_EQ(sorted(cold_probe.Data().context.structure->Members().members), expected);
    ASSERT_EQ(warm_probe.Data().profiles.size(), 1u);
    ASSERT_EQ(cold_probe.Data().profiles.size(), 1u);
    const auto& warm_profile = warm_probe.Data().profiles.front();
    const auto& cold_profile = cold_probe.Data().profiles.front();
    ASSERT_EQ(warm_profile.State(), survey::EvidenceState::kFinite);
    ASSERT_EQ(cold_profile.State(), survey::EvidenceState::kFinite);
    EXPECT_EQ(warm_profile.Reading().source, ProbeProfileSource::kWarmReused);
    EXPECT_EQ(cold_profile.Reading().source, ProbeProfileSource::kRecomputed);
    EXPECT_EQ(warm_profile.Scope().members.members, literal_members);
    EXPECT_EQ(cold_profile.Scope().members.members, literal_members);
    EXPECT_EQ(warm_profile.sample_count, kProfileSamples);
    EXPECT_EQ(cold_profile.sample_count, kProfileSamples);
    ASSERT_FALSE(warm_profile.Reading().samples.empty());
    ASSERT_EQ(warm_profile.Reading().samples.size(), cold_profile.Reading().samples.size());
    size_t positive_bins = 0;
    for (size_t i = 0; i < warm_profile.Reading().samples.size(); ++i) {
      const auto& warm_sample = warm_profile.Reading().samples[i];
      const auto& cold_sample = cold_profile.Reading().samples[i];
      EXPECT_EQ(warm_sample.delta_deg, cold_sample.delta_deg);
      EXPECT_TRUE((std::isnan(warm_sample.value) && std::isnan(cold_sample.value)) ||
                  warm_sample.value == cold_sample.value);
      EXPECT_EQ(warm_sample.k, cold_sample.k);
      if (std::isfinite(cold_sample.value)) {
        EXPECT_TRUE(std::isfinite(warm_sample.value));
        EXPECT_EQ(warm_sample.k_eff, cold_sample.k_eff);
        positive_bins += cold_sample.value > 0.0 && cold_sample.k > 0;
      }
    }
    EXPECT_GT(positive_bins, 0u);  // all-empty/all-NaN profiles cannot pass the numeric oracle

    workspace.reset();
    source = survey::SurveyResult{};
    EXPECT_EQ(sorted(warm_point.Data().survey_context.structure->Members().members), expected);
    EXPECT_EQ(sorted(warm_probe.Data().context.structure->Members().members), expected);
  };
  for (unsigned scope : { 0u, 7u }) {
    check_scope(scope);
  }
}

TEST(Workspace, ExplicitTypedWarmOutlivesItsSourceAndIsNotOverwritten) {
  std::unique_ptr<AnalysisWorkspace> producer;
  ASSERT_TRUE(AnalysisWorkspace::Create(survey::RandomPrismScene(), &producer).Ok());
  survey::SurveyResult source_result;
  ASSERT_TRUE(producer->QuerySurvey(SurveyOf({ 1, 2 }), &source_result).Ok());
  SinglePathRequest request = PointOf({ 1, 2 });
  request.warm_candidate = HarvestSurveyResult(source_result);
  EXPECT_EQ(request.warm_candidate->origin, WarmOrigin::kCompletedResult);
  producer.reset();
  source_result = survey::SurveyResult{};

  std::unique_ptr<AnalysisWorkspace> consumer;
  ASSERT_TRUE(AnalysisWorkspace::Create(survey::RandomPrismScene(), &consumer).Ok());
  PointResult result;
  ASSERT_TRUE(consumer->QueryPoint(request, true, &result).Ok());
  EXPECT_EQ(consumer->StructureRunCount(), 0);
  EXPECT_EQ(result.Data().survey_context.source, ContextSource::kWarm);
}

TEST(Workspace, CrossShapeRequestsNeverHitTheSurveySlot) {
  // The slot key must cover every request field that changes the document: a same-selection
  // request with different mc_ess_draws / budget_ms / scene_layer is a different document and
  // must run fresh (the structure-run counter moves), never serve the cached one silently.
  std::unique_ptr<AnalysisWorkspace> workspace;
  ASSERT_TRUE(AnalysisWorkspace::Create(TwoCrystalScene(), &workspace).Ok());
  survey::SurveyResult first;
  ASSERT_TRUE(workspace->QuerySurvey(SurveyOf({ 1, 2 }, 550.0), &first).Ok());
  const long runs = workspace->StructureRunCount();
  ASSERT_GE(runs, 1);

  struct Variant {
    const char* what;
    void (*mutate)(SurveyRequest*);
  };
  const Variant variants[] = {
    { "mc_ess_draws", [](SurveyRequest* r) { r->mc_ess_draws = 64; } },
    { "budget_ms", [](SurveyRequest* r) { r->budget_ms = 30000; } },
    { "scene_layer", [](SurveyRequest* r) { r->scene_layer = 0; } },
  };
  long expected_runs = runs;
  for (const Variant& variant : variants) {
    SurveyRequest request = SurveyOf({ 1, 2 }, 550.0);
    variant.mutate(&request);
    survey::SurveyResult result;
    if (const Error error = workspace->QuerySurvey(request, &result); !error.Ok()) {
      ADD_FAILURE() << variant.what << ": " << error.message;
      continue;
    }
    expected_runs += 1;
    EXPECT_EQ(workspace->StructureRunCount(), expected_runs)
        << variant.what << " served a stale document instead of one fresh run";
  }
}

TEST(Workspace, ACancelledSurveyNeverFillsTheWarmSlot) {
  std::unique_ptr<AnalysisWorkspace> workspace;
  ASSERT_TRUE(AnalysisWorkspace::Create(survey::RandomPrismScene(), &workspace).Ok());
  SurveyRequest request = SurveyOf({ 1, 2 }, 550.0);  // any arm: the cancel read is the slots'
  analytic::StopToken token;

  // Enter the survey's structure leg (its first T0 poll), then cancel: the run stops mid-way,
  // and the slots must refuse the cut document — a partial block never masquerades as cached.
  survey::SurveyResult cut;
  std::atomic<bool> done{ false };
  std::thread worker([&] {
    (void)workspace->QuerySurvey(request, &cut, &token);
    done = true;
  });
  const auto bound = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (token.PollCount(kPollSurveyT0) == 0 && std::chrono::steady_clock::now() < bound) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  const bool entered = token.PollCount(kPollSurveyT0) > 0;
  token.RequestCancel();
  while (!done && std::chrono::steady_clock::now() < bound) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  ASSERT_TRUE(done) << "the workspace survey ignored the cancel";
  worker.join();
  ASSERT_TRUE(entered) << "the survey's structure leg never polled";
  EXPECT_EQ(cut.Data().execution, ExecutionOutcome::kCancelled);
  EXPECT_EQ(workspace->OccupiedSlotCount(), 1);  // the geometry slot only — no warm, no result
  const int stray = FirstUnattributedPollSite(token, kPollFaceSurvey);
  EXPECT_EQ(stray, -1) << "the cancelled run ticked site " << stray << ", outside its attribution";

  // The refused slot shows behaviorally: the next point runs an inline context.
  const long runs = workspace->StructureRunCount();
  PointResult point;
  ASSERT_TRUE(workspace->QueryPoint(PointOf({ 1, 2 }), true, &point).Ok());
  EXPECT_GT(workspace->StructureRunCount(), runs);
  EXPECT_EQ(point.Data().survey_context.source, ContextSource::kInline);

  const long before_complete = workspace->StructureRunCount();
  survey::SurveyResult complete;
  ASSERT_TRUE(workspace->QuerySurvey(request, &complete).Ok());
  EXPECT_EQ(complete.Data().execution, ExecutionOutcome::kCompleted);
  EXPECT_GT(workspace->StructureRunCount(), before_complete);
  const long after_complete = workspace->StructureRunCount();
  survey::SurveyResult cached;
  ASSERT_TRUE(workspace->QuerySurvey(request, &cached).Ok());
  EXPECT_EQ(workspace->StructureRunCount(), after_complete);
  EXPECT_EQ(cached.Identity(), complete.Identity());
  EXPECT_EQ(cut.Data().execution, ExecutionOutcome::kCancelled);
}

TEST(Workspace, ResultsAndWarmOutliveTheConfigTheSceneAndTheWorkspace) {
  survey::SurveyResult survey;
  PointResult point;
  {
    ConfigManager config = survey::RandomPrismScene();
    std::unique_ptr<AnalysisWorkspace> workspace;
    ASSERT_TRUE(AnalysisWorkspace::Create(config, &workspace).Ok());
    ASSERT_TRUE(workspace->QuerySurvey(SurveyOf({ 1, 2 }), &survey).Ok());
    ASSERT_TRUE(workspace->QueryPoint(PointOf({ 1, 2 }), true, &point).Ok());
  }  // the workspace AND the caller's config are gone here

  EXPECT_FALSE(ToJson(point, "test-version").empty());          // still serializable
  const WarmCandidate harvested = HarvestSurveyResult(survey);  // warm is re-extractable
  EXPECT_TRUE(harvested.decode_error.Ok());
}

TEST(Workspace, TheStopAuthorityWorksThroughTheWorkspace) {
  // The step-granularity determination's evidence: the workspace's queries run the same
  // executions with the same poll sites — a cancel inside a workspace query stops at them.
  std::unique_ptr<AnalysisWorkspace> workspace;
  ASSERT_TRUE(AnalysisWorkspace::Create(survey::RandomPrismScene(), &workspace).Ok());
  SinglePathRequest request = PointOf({ 3, 5 });
  analytic::StopToken token;
  request.cancel = &token;
  request.sample_count = 1000000;

  PointResult result;
  std::atomic<bool> done{ false };
  std::thread worker([&] {
    (void)workspace->QueryPoint(request, false, &result, &token);  // the token rides the query
    done = true;
  });
  const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (token.PollCount(analytic::kPollBandLattice) == 0 && !done && std::chrono::steady_clock::now() < end) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  token.RequestCancel();  // safe either way: entered, or the run already finished
  while (!done && std::chrono::steady_clock::now() < end) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  worker.join();  // joined on every path — a joinable thread's destructor would terminate
  EXPECT_TRUE(done);
  EXPECT_GT(token.PollCount(analytic::kPollBandLattice), 0) << "the workspace query never polled";
  EXPECT_EQ(result.Data().meta.execution, ExecutionOutcome::kCancelled);
  const int stray = FirstUnattributedPollSite(token, kPollFacePoint);
  EXPECT_EQ(stray, -1) << "the workspace point run ticked site " << stray << ", outside its attribution";
}

// ---- the interpretive-point chain ------------------------------------------------------------
// The engine-side answer form the adjudication pinned: row + click in, the interpretive class
// and the selected member's own payload out, resolution and member evaluations under ONE
// deadline. The Parry scene is the acceptance scene (prism 1.2 regular; zenith gauss 90±1,
// azimuth full turn, roll gauss 0±1; sun 20°/0°); the click sides and the fixed-member and
// stop paths are the chain's own contract.

ConfigManager InterpretiveParryScene() {
  // The acceptance scene itself (test_single_path_analysis's ParryMinus52Lights fixture):
  // prism height 1.2 regular — the basal-involving members' geometry depends on the height, so
  // the fixture must match the archived evidence's shape, not any regular prism.
  PrismCrystalParam prism;
  prism.h_ = { DistributionType::kNoRandom, 1.2f, 0 };
  for (auto& distance : prism.d_) {
    distance = { DistributionType::kNoRandom, 1.0f, 0 };
  }
  ConfigManager scene;
  CrystalConfig crystal;
  crystal.id_ = 1;
  crystal.param_ = prism;
  crystal.axis_.azimuth_dist = { DistributionType::kUniform, 0.0f, 360.0f };
  crystal.axis_.latitude_dist = { DistributionType::kGaussian, 0.0f, 1.0f };
  crystal.axis_.roll_dist = { DistributionType::kGaussian, 0.0f, 1.0f };
  scene.crystals_.emplace(1, crystal);
  scene.scene_.light_source_.param_ = SunParam{ 20.0f, 0.0f, 0.5f };
  scene.scene_.light_source_.spectrum_ = IlluminantType::kD65;
  ScatteringSetting entry{};
  entry.crystal_ = crystal;
  entry.crystal_proportion_ = 1;
  scene.scene_.ms_.push_back({ 0, { entry } });
  return scene;
}

InterpretivePointQuery InterpretiveQuery(double altitude, double azimuth) {
  InterpretivePointQuery query;
  query.crystal_id = 1;
  query.representative = { 4, 1 };
  query.symmetry_bits = 7;  // P | B | D: the analysis row's full bit set
  query.target_altitude_deg = altitude;
  query.target_azimuth_deg = azimuth;
  query.sample_count = 100000;  // the arc recovers well under the kernel default; cost is the suite's
  query.sun_grid_lat_count = 0;
  return query;
}

TEST(InterpretivePointChain, ParryMinus52AutoSelectsTheCarryingClass) {
  std::unique_ptr<AnalysisWorkspace> workspace;
  ASSERT_TRUE(AnalysisWorkspace::Create(InterpretiveParryScene(), &workspace).Ok());
  InterpretivePointAnswer answer;
  ASSERT_TRUE(workspace->QueryInterpretivePoint(InterpretiveQuery(36.0, -52.0), false, &answer).Ok());
  // The class facts stand regardless of the search: two half-turn classes in representative order.
  ASSERT_EQ(answer.classes.size(), 2u);
  EXPECT_EQ(answer.classes[0].representative, (std::vector<int>{ 4, 1 }));
  EXPECT_EQ(answer.classes[1].representative, (std::vector<int>{ 4, 2 }));
  // The search: 4-1's class carries no ordinary density reading here; 4-2's does.
  ASSERT_EQ(answer.selected_class, 1);
  ASSERT_GE(answer.selected_member, 0);
  EXPECT_EQ(answer.classes[1].member_sequences[static_cast<size_t>(answer.selected_member)],
            (std::vector<int>{ 4, 2 }));
  EXPECT_FALSE(answer.member_fixed);
  // The payload is the selected member's OWN evaluation, its interpretive pose on the lit arc.
  ASSERT_EQ(answer.point.Data().OutcomeValue(), Outcome::kDiscovered);
  const InterpretivePose& pose = answer.point.Data().Discovered().interpretive;
  ASSERT_TRUE(pose.available);
  const PointDetail& peak = answer.point.Data().Discovered().components[pose.component_index].points[pose.point_index];
  EXPECT_GT(peak.rho, 0.0) << "the interpretive pose explains the clicked arc with positive weight";
}

TEST(InterpretivePointChain, ParryPlus52SelectsTheParentRepresentative) {
  std::unique_ptr<AnalysisWorkspace> workspace;
  ASSERT_TRUE(AnalysisWorkspace::Create(InterpretiveParryScene(), &workspace).Ok());
  InterpretivePointAnswer answer;
  ASSERT_TRUE(workspace->QueryInterpretivePoint(InterpretiveQuery(36.0, 52.0), false, &answer).Ok());
  // The row representative's own class carries the +52 light: the first class evaluated wins,
  // and the shown member stays the parent row's 4-1 (the list semantics' face).
  ASSERT_EQ(answer.selected_class, 0);
  EXPECT_EQ(answer.classes[0].member_sequences[static_cast<size_t>(answer.selected_member)],
            (std::vector<int>{ 4, 1 }));
  const InterpretivePose& pose = answer.point.Data().Discovered().interpretive;
  ASSERT_TRUE(pose.available);
}

TEST(InterpretivePointChain, FixedMemberKeepsItsOwnGeometryAndLogFacts) {
  std::unique_ptr<AnalysisWorkspace> workspace;
  ASSERT_TRUE(AnalysisWorkspace::Create(InterpretiveParryScene(), &workspace).Ok());
  InterpretivePointQuery query = InterpretiveQuery(36.0, -52.0);
  query.fixed_member = { 4, 1 };  // the dark member, fixed: never auto-swapped
  InterpretivePointAnswer answer;
  ASSERT_TRUE(workspace->QueryInterpretivePoint(query, false, &answer).Ok());
  EXPECT_TRUE(answer.member_fixed);
  ASSERT_EQ(answer.selected_class, 0);
  ASSERT_GE(answer.selected_member, 0);
  EXPECT_EQ(answer.classes[0].member_sequences[static_cast<size_t>(answer.selected_member)],
            (std::vector<int>{ 4, 1 }));
  // The member's own geometry stands and its density reads stay honest deep tails — no fake
  // zero, no swap to the lit sibling.
  ASSERT_EQ(answer.point.Data().OutcomeValue(), Outcome::kDiscovered);
  ASSERT_FALSE(answer.point.Data().Discovered().components.empty());
  int valid = 0;
  for (const FiberComponent& component : answer.point.Data().Discovered().components) {
    for (const PointDetail& p : component.points) {
      if (!p.valid) {
        continue;
      }
      EXPECT_TRUE(std::isfinite(p.log_rho));
      EXPECT_LT(p.log_rho, -100.0) << "the ±1° roll anchor never reaches this arc: an honest deep tail";
      ++valid;
    }
  }
  EXPECT_GT(valid, 0);
}

TEST(InterpretivePointChain, FixedMemberOutsideTheRowIsRefused) {
  std::unique_ptr<AnalysisWorkspace> workspace;
  ASSERT_TRUE(AnalysisWorkspace::Create(InterpretiveParryScene(), &workspace).Ok());
  InterpretivePointQuery query = InterpretiveQuery(36.0, -52.0);
  query.fixed_member = { 3, 5 };  // a legal face sequence that is not this row's member
  InterpretivePointAnswer answer;
  const Error error = workspace->QueryInterpretivePoint(query, false, &answer);
  EXPECT_FALSE(error.Ok());
  EXPECT_EQ(error.code, ErrorCode::kInvalidPath);
}

TEST(InterpretivePointChain, CancelStopsTheSearchAndLeavesTheClassesStanding) {
  std::unique_ptr<AnalysisWorkspace> workspace;
  ASSERT_TRUE(AnalysisWorkspace::Create(InterpretiveParryScene(), &workspace).Ok());
  analytic::StopToken token;
  token.RequestCancel();  // cancelled before the chain starts
  InterpretivePointAnswer answer;
  ASSERT_TRUE(workspace->QueryInterpretivePoint(InterpretiveQuery(36.0, -52.0), false, &answer, &token).Ok());
  // The algebra is not the search's to invalidate: both classes stand; the honest outcome rides
  // on the payload and nothing was selected.
  ASSERT_EQ(answer.classes.size(), 2u);
  EXPECT_EQ(answer.selected_class, -1);
  EXPECT_EQ(answer.selected_member, -1);
  EXPECT_EQ(answer.point.Data().meta.execution, ExecutionOutcome::kCancelled);
}

TEST(InterpretivePointChain, SharedDeadlineStopsTheSearchHonestly) {
  std::unique_ptr<AnalysisWorkspace> workspace;
  ASSERT_TRUE(AnalysisWorkspace::Create(InterpretiveParryScene(), &workspace).Ok());
  InterpretivePointQuery query = InterpretiveQuery(36.0, -52.0);
  query.budget_ms = 1;  // the first member's evaluation cannot finish inside it
  InterpretivePointAnswer answer;
  ASSERT_TRUE(workspace->QueryInterpretivePoint(query, false, &answer).Ok());
  // One deadline for the whole chain: the first member's stop ends the search — a later member
  // never gains budget — and the answer says so honestly.
  ASSERT_EQ(answer.classes.size(), 2u);
  EXPECT_EQ(answer.selected_class, -1);
  EXPECT_EQ(answer.point.Data().meta.execution, ExecutionOutcome::kDeadlineExceeded);
}

}  // namespace
}  // namespace lumice::raypath
