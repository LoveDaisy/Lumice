// AnalysisWorkspace (the typed-architecture design's data-and-reuse owner): the S4 acceptance
// reads — lazy creation, build-once geometry per current selection, the bounded slot occupancy
// under selection switching, the typed survey->point warm chain (no JSON, no recompute), the
// cancelled-block rejection, and the results' independence from the caller's config, the scene
// and the workspace itself.
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
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
  EXPECT_EQ(QueryDeadlineResolutionCount(), queries + 2);
  EXPECT_EQ(BudgetResolutionCount(BudgetWindow::kLegGuardrail), survey_windows);

  analytic::StopToken token;
  token.RequestCancel();
  survey::SurveyResult cut;
  const long before_cut = workspace->StructureRunCount();
  ASSERT_TRUE(workspace->QuerySurvey(survey_request, &cut, &token).Ok());
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

TEST(Workspace, ExplicitTypedWarmOutlivesItsSourceAndIsNotOverwritten) {
  std::unique_ptr<AnalysisWorkspace> producer;
  ASSERT_TRUE(AnalysisWorkspace::Create(survey::RandomPrismScene(), &producer).Ok());
  survey::SurveyResult source_result;
  ASSERT_TRUE(producer->QuerySurvey(SurveyOf({ 1, 2 }), &source_result).Ok());
  SinglePathRequest request = PointOf({ 1, 2 });
  request.warm_candidate = HarvestSurveyResult(source_result);
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
  ASSERT_GT(token.PollCount(kPollSurveyT0), 0) << "the survey's structure leg never polled";
  token.RequestCancel();
  while (!done && std::chrono::steady_clock::now() < bound) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  ASSERT_TRUE(done) << "the workspace survey ignored the cancel";
  worker.join();
  EXPECT_EQ(workspace->OccupiedSlotCount(), 1);  // the geometry slot only — no warm, no result

  // The refused slot shows behaviorally: the next point runs an inline context.
  const long runs = workspace->StructureRunCount();
  PointResult point;
  ASSERT_TRUE(workspace->QueryPoint(PointOf({ 1, 2 }), true, &point).Ok());
  EXPECT_GT(workspace->StructureRunCount(), runs);
  EXPECT_EQ(point.Data().survey_context.source, ContextSource::kInline);
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
}

}  // namespace
}  // namespace lumice::raypath
