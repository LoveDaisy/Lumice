#include <gtest/gtest.h>

#include <chrono>

#include "raypath/detail/input_assembly.hpp"
#include "raypath/detail/quantity/mc_ess.hpp"
#include "raypath/detail/schema3/report_assembly.hpp"
#include "raypath/path_feature_report.hpp"
#include "raypath/probe_analysis.hpp"
#include "raypath/query_control.hpp"
#include "raypath/scene_to_analytic.hpp"
#include "raypath/single_path_analysis.hpp"
#include "survey_test_fixtures.hpp"

// The budget owner's contract (query_control.hpp) and the runtime reads the S1 exit pins:
// one resolution per legacy leg per query — a query face's run moves its own windows'
// counters by exactly the leg count and nothing else's — and the unified query deadline
// resolved once, clamped onto every leg, never re-based.
namespace lumice::raypath {
namespace {

constexpr auto kMaxTime = std::chrono::steady_clock::time_point::max();

BudgetRequest JustBudget(int budget_ms) {
  BudgetRequest request;
  request.budget_ms = budget_ms;
  return request;
}

TEST(QueryControl, PointDiscoveryZeroMeansUnboundedAndEchoesZero) {
  const LegControl unbounded = ResolveLeg(BudgetWindow::kPointDiscovery, JustBudget(0));
  EXPECT_EQ(unbounded.stop.deadline, kMaxTime);
  EXPECT_EQ(unbounded.resolved_ms, 0);
  const LegControl bounded = ResolveLeg(BudgetWindow::kPointDiscovery, JustBudget(5000));
  EXPECT_LT(bounded.stop.deadline, kMaxTime);
  EXPECT_GT(bounded.stop.deadline, std::chrono::steady_clock::now());
  EXPECT_EQ(bounded.resolved_ms, 5000);
}

TEST(QueryControl, ProbeLambdasZeroResolvesTheDocumentedDefault) {
  const LegControl defaulted = ResolveLeg(BudgetWindow::kProbeLambdas, JustBudget(0));
  EXPECT_EQ(defaulted.resolved_ms, kDefaultProbeBudgetMs);
  EXPECT_LT(defaulted.stop.deadline, kMaxTime);
  const LegControl explicit_ms = ResolveLeg(BudgetWindow::kProbeLambdas, JustBudget(7000));
  EXPECT_EQ(explicit_ms.resolved_ms, 7000);
}

TEST(QueryControl, QualityDeadlinePrefersTheCallersAbsolute) {
  const auto pinned = std::chrono::steady_clock::now() + std::chrono::milliseconds(12345);
  BudgetRequest with_absolute = JustBudget(15000);
  with_absolute.absolute_deadline = pinned;
  const LegControl control = ResolveLeg(BudgetWindow::kQualityDeadline, with_absolute);
  EXPECT_EQ(control.stop.deadline, pinned);  // the budget does not move a pinned deadline
  const LegControl from_budget = ResolveLeg(BudgetWindow::kQualityDeadline, JustBudget(15000));
  EXPECT_LT(from_budget.stop.deadline, kMaxTime);
  EXPECT_EQ(from_budget.resolved_ms, 15000);
}

TEST(QueryControl, HangCapIsFixedAndBudgetIndependent) {
  const LegControl control = ResolveLeg(BudgetWindow::kHangCap, JustBudget(0));
  EXPECT_EQ(control.resolved_ms, static_cast<int>(schema3::kEnumerationHangCapMs));
  EXPECT_LT(control.stop.deadline, kMaxTime);
}

TEST(QueryControl, LegGuardrailEchoesTheSurveyBudgetVerbatim) {
  const LegControl control = ResolveLeg(BudgetWindow::kLegGuardrail, JustBudget(1234));
  EXPECT_EQ(control.resolved_ms, 1234);
  EXPECT_LT(control.stop.deadline, kMaxTime);
}

TEST(QueryControl, EachResolutionBumpsOnlyItsOwnWindow) {
  ResetBudgetResolutionCountsForTest();
  (void)ResolveLeg(BudgetWindow::kLegGuardrail, JustBudget(10));
  EXPECT_EQ(BudgetResolutionCount(BudgetWindow::kLegGuardrail), 1);
  for (const BudgetWindow other : { BudgetWindow::kPointDiscovery, BudgetWindow::kProbeLambdas,
                                    BudgetWindow::kQualityDeadline, BudgetWindow::kHangCap }) {
    EXPECT_EQ(BudgetResolutionCount(other), 0);
  }
  EXPECT_EQ(QueryDeadlineResolutionCount(), 0);
}

// The unified query contract: resolved once; legs clamp to the query's own point in time, so a
// leg taken later never gains budget (there is no leg-local now() to re-base on).
TEST(QueryControl, UnifiedDeadlineResolvesOnceAndLegsNeverGainBudget) {
  ResetBudgetResolutionCountsForTest();
  QueryDeadline deadline = ResolveQueryDeadline(JustBudget(100000));
  EXPECT_EQ(QueryDeadlineResolutionCount(), 1);
  EXPECT_EQ(deadline.resolved_ms, 100000);
  const auto leg_late = deadline.LegStop();   // taken "later": same construction, no new budget
  EXPECT_EQ(leg_late.deadline, deadline.at);  // a leg's deadline is never later than the query's
  EXPECT_GT(deadline.at, std::chrono::steady_clock::now());
  for (const BudgetWindow window :
       { BudgetWindow::kPointDiscovery, BudgetWindow::kProbeLambdas, BudgetWindow::kQualityDeadline,
         BudgetWindow::kHangCap, BudgetWindow::kLegGuardrail }) {
    EXPECT_EQ(BudgetResolutionCount(window), 0);  // the unified path installs no legacy window
  }

  QueryDeadline unbounded = ResolveQueryDeadline(JustBudget(0));  // explicit unbounded request
  EXPECT_EQ(unbounded.at, kMaxTime);
  EXPECT_EQ(unbounded.resolved_ms, 0);

  const auto pinned = std::chrono::steady_clock::now() + std::chrono::milliseconds(1);
  BudgetRequest with_absolute = JustBudget(999);
  with_absolute.absolute_deadline = pinned;
  EXPECT_EQ(ResolveQueryDeadline(with_absolute).at, pinned);  // the caller's absolute wins
}

// ---- the runtime reads: each legacy query face resolves its legs exactly once, nothing else ----

TEST(QueryControl, PointQueryResolvesTheDiscoveryLegOnce) {
  ResetBudgetResolutionCountsForTest();
  const ConfigManager config = survey::PlateScene(0.3, 0.3);
  SinglePathRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 5 } };
  request.target_altitude_deg = 20.0;
  request.target_azimuth_deg = 25.0;
  request.sample_count = 2000;
  request.sun_grid_lat_count = 0;
  request.budget_ms = 15000;
  PointResult result;
  ASSERT_TRUE(AnalyzePathPointValue(config, request, false, &result).Ok());
  EXPECT_EQ(BudgetResolutionCount(BudgetWindow::kPointDiscovery), 1);
  for (const BudgetWindow other : { BudgetWindow::kProbeLambdas, BudgetWindow::kQualityDeadline, BudgetWindow::kHangCap,
                                    BudgetWindow::kLegGuardrail }) {
    EXPECT_EQ(BudgetResolutionCount(other), 0);
  }
  EXPECT_EQ(QueryDeadlineResolutionCount(), 0);  // the legacy entry never resolves a unified deadline
}

TEST(QueryControl, ProbeQueryResolvesTheLambdaLegOnceAndNothingElse) {
  ResetBudgetResolutionCountsForTest();
  const ConfigManager config = survey::RandomPrismScene();
  ProbeRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 1, 2 } };  // the identity arm: the inline context is milliseconds
  request.along = ProbeAlong::kRadial;
  request.at_altitude_deg = -30.0;
  request.at_azimuth_deg = 0.0;
  request.budget_ms = 15000;
  ProbeResult result;
  ASSERT_TRUE(AnalyzePathProbe(config, request, &result).Ok());
  EXPECT_EQ(BudgetResolutionCount(BudgetWindow::kProbeLambdas), 1);
  for (const BudgetWindow other : { BudgetWindow::kPointDiscovery, BudgetWindow::kQualityDeadline,
                                    BudgetWindow::kHangCap, BudgetWindow::kLegGuardrail }) {
    EXPECT_EQ(BudgetResolutionCount(other), 0);  // the context segment resolves no window at all
  }
}

TEST(QueryControl, ReportQueryResolvesQualityAndHangCapOnceEach) {
  ResetBudgetResolutionCountsForTest();
  const ConfigManager config = survey::RandomPrismScene();
  PathFeatureReportRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 1, 2 } };  // the smallest physical class keeps the leg read cheap
  request.sample_count = 64;
  request.budget_ms = 15000;
  request.max_optical_evaluations = 20000;
  request.max_field_evaluations = 1000000;
  FeatureReportResult result;
  ASSERT_TRUE(AnalyzePathFeatureReportValue(config, request, &result).Ok());
  EXPECT_EQ(BudgetResolutionCount(BudgetWindow::kQualityDeadline), 1);
  EXPECT_EQ(BudgetResolutionCount(BudgetWindow::kHangCap), 1);
  for (const BudgetWindow other :
       { BudgetWindow::kPointDiscovery, BudgetWindow::kProbeLambdas, BudgetWindow::kLegGuardrail }) {
    EXPECT_EQ(BudgetResolutionCount(other), 0);
  }
}

TEST(QueryControl, McEssLegResolvesItsGuardrailOnce) {
  ResetBudgetResolutionCountsForTest();
  const ConfigManager config = survey::RandomPrismScene();
  InputSnapshot snapshot;
  const std::vector<int> faces{ 3, 5, 7 };
  const std::vector<LayerSelection> selection{ { 0, 1, faces, 7 } };
  ASSERT_TRUE(CaptureInput(config.scene_, "test snapshot", selection, &snapshot).Ok());
  double sun_hat[3];
  SunIncidentDirection(config.scene_.light_source_.param_, sun_hat);
  quantity::McRegionEssInput input;
  input.snapshot = &snapshot;
  input.wavelength_nm = 550.0;
  for (int k = 0; k < 3; k++) {
    input.sun_dir[static_cast<size_t>(k)] = -sun_hat[k];
  }
  input.member_count = 12;
  input.draws = 64;
  input.budget_ms = 120000;
  (void)quantity::EvaluateMcRegionEss(input);
  EXPECT_EQ(BudgetResolutionCount(BudgetWindow::kLegGuardrail), 1);
  for (const BudgetWindow other : { BudgetWindow::kPointDiscovery, BudgetWindow::kProbeLambdas,
                                    BudgetWindow::kQualityDeadline, BudgetWindow::kHangCap }) {
    EXPECT_EQ(BudgetResolutionCount(other), 0);
  }
}

}  // namespace
}  // namespace lumice::raypath
