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
}

TEST(Workspace, BothContextScopesReuseOnlyTheirOwnMembers) {
  // A regular prism's 60-degree side-face wedge has these twelve physical members.
  // This list is a face-geometry oracle, not the production fingerprint.
  const std::vector<std::vector<int>> literal_members{ { 3, 5 } };
  const std::vector<std::vector<int>> class_members{ { 3, 5 }, { 3, 7 }, { 4, 6 }, { 4, 8 }, { 5, 3 }, { 5, 7 },
                                                     { 6, 4 }, { 6, 8 }, { 7, 3 }, { 7, 5 }, { 8, 4 }, { 8, 6 } };
  const auto sorted = [](std::vector<std::vector<int>> members) {
    std::sort(members.begin(), members.end());
    return members;
  };
  const auto check_scope = [&](unsigned scope) {
    SCOPED_TRACE(scope);
    const ConfigManager config = survey::RandomPrismScene();
    std::unique_ptr<AnalysisWorkspace> workspace;
    ASSERT_TRUE(AnalysisWorkspace::Create(config, &workspace).Ok());
    SurveyRequest survey_request = SurveyOf({ 3, 5 });
    survey_request.symmetry_bits = scope;
    survey::SurveyResult source;
    ASSERT_TRUE(workspace->QuerySurvey(survey_request, &source).Ok());
    const auto& expected = scope == 0 ? literal_members : class_members;
    EXPECT_EQ(sorted(source.Data().members.members), expected);
    const long runs = workspace->StructureRunCount();

    SinglePathRequest point_request = PointOf({ 3, 5 });
    point_request.symmetry_bits = scope;
    PointResult warm_point;
    ASSERT_TRUE(workspace->QueryPoint(point_request, true, &warm_point).Ok());
    EXPECT_EQ(warm_point.Data().members.members, literal_members);
    ASSERT_TRUE(warm_point.Data().survey_context.structure.has_value());
    EXPECT_EQ(sorted(warm_point.Data().survey_context.structure->Members().members), expected);
    EXPECT_EQ(warm_point.Data().survey_context.source, ContextSource::kWarm);
    EXPECT_EQ(workspace->StructureRunCount(), runs);

    ProbeRequest probe_request;
    probe_request.crystal_id = 1;
    probe_request.path_layers = { { 3, 5 } };
    probe_request.symmetry_bits = scope;
    probe_request.at_altitude_deg = -30;
    probe_request.sample_count = 2000000;  // the producing survey's actual lattice
    probe_request.budget_ms = 120000;
    ProbeResult warm_probe;
    ASSERT_TRUE(workspace->QueryProbe(probe_request, &warm_probe).Ok());
    EXPECT_EQ(warm_probe.Data().context.source, ContextSource::kWarm);
    ASSERT_TRUE(warm_probe.Data().context.structure.has_value());
    EXPECT_EQ(sorted(warm_probe.Data().context.structure->Members().members), expected);
    EXPECT_EQ(workspace->StructureRunCount(), runs);

    std::unique_ptr<AnalysisWorkspace> cold;
    ASSERT_TRUE(AnalysisWorkspace::Create(config, &cold).Ok());
    PointResult cold_point;
    ASSERT_TRUE(cold->QueryPoint(point_request, true, &cold_point).Ok());
    ASSERT_TRUE(cold_point.Data().survey_context.structure.has_value());
    EXPECT_EQ(sorted(cold_point.Data().survey_context.structure->Members().members), expected);
    EXPECT_EQ(cold_point.Data().members.members, warm_point.Data().members.members);
    EXPECT_EQ(cold_point.Data().survey_context.object.state, warm_point.Data().survey_context.object.state);
    ProbeResult cold_probe;
    ASSERT_TRUE(cold->QueryProbe(probe_request, &cold_probe).Ok());
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
    ASSERT_EQ(warm_profile.Reading().samples.size(), cold_profile.Reading().samples.size());
    for (size_t i = 0; i < warm_profile.Reading().samples.size(); ++i) {
      const double a = warm_profile.Reading().samples[i].value;
      const double b = cold_profile.Reading().samples[i].value;
      EXPECT_TRUE((std::isnan(a) && std::isnan(b)) || a == b);
    }

    // An explicit incompatible candidate must not fall back to the compatible automatic slot.
    WarmCandidate wrong = HarvestSurveyResult(source);
    point_request.symmetry_bits = scope == 0 ? 7u : 0u;
    point_request.warm_candidate = wrong;
    PointResult refused;
    EXPECT_FALSE(workspace->QueryPoint(point_request, true, &refused).Ok());
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

}  // namespace
}  // namespace lumice::raypath
