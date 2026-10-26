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
#include "raypath/single_path_json.hpp"
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

// A completed value's serialization is pure and stable: two reads of the same result are
// byte-identical and start nothing (the deferred-materialization path that once paid per read
// is gone; the result owns its context block).
namespace lumice::raypath {
TEST(QueryResultReading, RepeatedSerializationIsIdenticalAndStartsNothing) {
  const ConfigManager config = survey::RandomPrismScene();
  SinglePathRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 1, 2 } };
  request.target_altitude_deg = 30.0;
  request.target_azimuth_deg = 90.0;
  request.sample_count = 2000;
  request.sun_grid_lat_count = 0;
  request.budget_ms = 120000;
  PointResult result;
  ASSERT_TRUE(AnalyzePathPointValue(config, request, true, &result).Ok());
  const std::string first = ToJson(result, "test-version");
  const std::string second = ToJson(result, "test-version");
  EXPECT_EQ(first, second);
  EXPECT_FALSE(first.empty());
}
}  // namespace lumice::raypath

// ============================================================================================
// The stop-authority handshakes (S2): every stoppable stage runs for real, is ENTERED (the
// poll ledger moves — proof the loop is inside its pollable region), then cancelled from this
// thread; the worker must return with the cancelled outcome and honest partial coverage. No
// sleep is proof of anything here: the ledger counter is the entry signal and the timeout the
// failure mode (a hung stage FAILS the wait, it never hangs the suite).
// ============================================================================================

#include <atomic>
#include <chrono>
#include <thread>

namespace lumice::raypath {
namespace {

using Clock = std::chrono::steady_clock;

// Bounded entry wait on the poll ledger: true once the site moved, false at the bound (the
// failure path — the test then reports the stage never reached a poll).
bool WaitForPoll(const analytic::StopToken& token, int site, std::chrono::milliseconds bound) {
  const auto end = Clock::now() + bound;
  while (token.PollCount(site) == 0) {
    if (Clock::now() > end) {
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return true;
}

struct WorkerHandle {
  std::thread worker;
  std::atomic<bool> done{ false };
  template <class Fn>
  explicit WorkerHandle(Fn&& fn)
      : worker([this, fn] {
          fn();
          done = true;
        }) {}
  // Bounded join: true when the worker returned; false at the bound (a stage that ignored the
  // cancel — the invariant these tests exist to forbid).
  bool JoinBounded(std::chrono::milliseconds bound) {
    const auto end = Clock::now() + bound;
    while (!done.load()) {
      if (Clock::now() > end) {
        return false;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    worker.join();
    return true;
  }
};

constexpr auto kEnterBound = std::chrono::seconds(30);
constexpr auto kJoinBound = std::chrono::seconds(30);

// The context assembly (the S0 measurement's dominant unsplittable block: 12.8 s, zero polls
// then) is entered, cancelled, and returns with the named refusal — never a fabricated context.
TEST(QueryStopHandshake, PointContextIsEnteredThenCancelled) {
  const ConfigManager config = survey::RandomPrismScene();  // the heavy full-dim column
  SinglePathRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 5 } };
  request.target_altitude_deg = 20.0;
  request.target_azimuth_deg = 25.0;
  request.sample_count = 2000;
  request.sun_grid_lat_count = 0;
  request.budget_ms = 120000;
  analytic::StopToken token;
  request.cancel = &token;

  PointResult result;
  WorkerHandle worker([&] {
    (void)AnalyzePathPointValue(config, request, true, &result);  // include_context: the cut lands in the context
  });
  ASSERT_TRUE(WaitForPoll(token, kPollSurveyT0, kEnterBound)) << "the context's T0 leg never polled";
  ASSERT_TRUE(WaitForPoll(token, kPollStructureMember, kEnterBound)) << "the structure walk never polled";
  token.RequestCancel();
  ASSERT_TRUE(worker.JoinBounded(kJoinBound)) << "the context ignored the cancel";
  EXPECT_EQ(result.Data().meta.execution, ExecutionOutcome::kCancelled);
  EXPECT_EQ(result.Data().survey_context.object.reason, "stop_requested");
  EXPECT_EQ(result.Data().survey_context.edge.reason, "stop_requested");
  EXPECT_GT(token.PollCount(kPollStructureMember), 0);
  EXPECT_GT(token.PollCount(analytic::kPollWalkSteps) + token.PollCount(analytic::kPollWalkPieces), 0)
      << "the walk's own step/piece polls are the coverage evidence";
}

// The discovery legs: entered via the lattice poll, cancelled mid-lattice, partial components.
TEST(QueryStopHandshake, PointDiscoveryIsEnteredThenCancelled) {
  const ConfigManager config = survey::PlateScene(0.3, 0.3);
  SinglePathRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 5 } };
  request.target_altitude_deg = 20.0;
  request.target_azimuth_deg = 25.0;
  request.sample_count = 1000000;
  request.sun_grid_lat_count = 0;
  request.budget_ms = 120000;
  analytic::StopToken token;
  request.cancel = &token;

  PointResult result;
  WorkerHandle worker([&] { (void)AnalyzePathPointValue(config, request, false, &result); });
  ASSERT_TRUE(WaitForPoll(token, analytic::kPollBandLattice, kEnterBound)) << "the discovery lattice never polled";
  token.RequestCancel();
  ASSERT_TRUE(worker.JoinBounded(kJoinBound)) << "the discovery legs ignored the cancel";
  EXPECT_EQ(result.Data().meta.execution, ExecutionOutcome::kCancelled);
  EXPECT_TRUE(result.Data().meta.interrupted);
}

// The legacy signal entry stays a first-class cancel: the same single authority reads it, the
// outcome names cancelled — no second mechanism, no lost semantics.
TEST(QueryStopHandshake, TheLegacySignalFlagIsAnExplicitCancel) {
  const ConfigManager config = survey::PlateScene(0.3, 0.3);
  SinglePathRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 5 } };
  request.target_altitude_deg = 20.0;
  request.target_azimuth_deg = 25.0;
  request.sample_count = 1000000;
  request.sun_grid_lat_count = 0;
  request.budget_ms = 120000;
  volatile int signalled = 0;  // the caller's one-way flag (sig_atomic_t's shape)
  request.interrupted = &signalled;

  // The token is the ENTRY OBSERVER only (its ledger proves the loop is inside); the SIGNAL is
  // the cancel source — the single authority reads both, and the outcome must name the signal.
  analytic::StopToken observer;
  request.cancel = &observer;
  PointResult result;
  WorkerHandle worker([&] { (void)AnalyzePathPointValue(config, request, false, &result); });
  ASSERT_TRUE(WaitForPoll(observer, analytic::kPollBandLattice, kEnterBound)) << "the discovery lattice never polled";
  signalled = 1;
  ASSERT_TRUE(worker.JoinBounded(kJoinBound)) << "the legacy signal was ignored";
  EXPECT_FALSE(observer.CancelRequested());  // the token never fired: the signal alone stopped it
  EXPECT_EQ(result.Data().meta.execution, ExecutionOutcome::kCancelled);  // the signal IS an explicit cancel
  EXPECT_TRUE(result.Data().meta.interrupted);
}

// The probe's context phase (the S0 measurement's dominant block, and the exact path where a
// cut once fell through to a disengaged optional): entered via the T0 poll, cancelled, and the
// query returns with the named refusal — the context claims nothing, the lambdas never start.
TEST(QueryStopHandshake, ProbeContextIsEnteredThenCancelled) {
  const ConfigManager config = survey::RandomPrismScene();  // the heavy full-dim column
  ProbeRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 5 } };
  request.along = ProbeAlong::kRadial;
  request.at_altitude_deg = 20.0;
  request.at_azimuth_deg = 25.0;
  request.budget_ms = 120000;
  analytic::StopToken token;
  request.cancel = &token;

  ProbeResult result;
  WorkerHandle worker([&] { (void)AnalyzePathProbe(config, request, &result); });
  ASSERT_TRUE(WaitForPoll(token, kPollSurveyT0, kEnterBound)) << "the probe context never polled";
  token.RequestCancel();
  ASSERT_TRUE(worker.JoinBounded(kJoinBound)) << "the probe context ignored the cancel";
  EXPECT_EQ(result.Data().meta.execution, ExecutionOutcome::kCancelled);
  EXPECT_EQ(result.Data().context.object.reason, "stop_requested");
  EXPECT_EQ(result.Data().context.edge.reason, "stop_requested");
  EXPECT_TRUE(result.Data().context.cut_by_stop);  // the typed signal, not the display string
  EXPECT_TRUE(result.Data().meta.interrupted);     // the first lambda boundary saw the token
}

// The probe's lambda phase: entered via the lattice poll, cancelled mid-line, the cut profile
// refuses with the stop's name and claims no readings it did not run.
TEST(QueryStopHandshake, ProbeLambdaLineIsEnteredThenCancelled) {
  const ConfigManager config = survey::RandomPrismScene();
  ProbeRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 1, 2 } };  // the identity arm: a cheap context, a real polled line
  request.along = ProbeAlong::kRadial;
  request.at_altitude_deg = 20.0;
  request.at_azimuth_deg = 25.0;
  request.wavelengths_nm = { 450.0, 550.0, 650.0 };
  request.budget_ms = 120000;
  analytic::StopToken token;
  request.cancel = &token;

  ProbeResult result;
  WorkerHandle worker([&] { (void)AnalyzePathProbe(config, request, &result); });
  ASSERT_TRUE(WaitForPoll(token, analytic::kPollEventsLattice, kEnterBound)) << "the band-sum lattice never polled";
  token.RequestCancel();
  ASSERT_TRUE(worker.JoinBounded(kJoinBound)) << "the lambda phase ignored the cancel";
  EXPECT_EQ(result.Data().meta.execution, ExecutionOutcome::kCancelled);
  EXPECT_TRUE(result.Data().meta.interrupted);
  EXPECT_LT(result.Data().profiles.size(), 3u);  // the later lambdas never started
  bool any_stop_refused = false;
  for (const ProbeProfile& profile : result.Data().profiles) {
    if (profile.State() == survey::EvidenceState::kRefused && profile.RefusalReason() == "stop_requested") {
      any_stop_refused = true;  // the cut line's honest refusal, never partial bins as full
    }
  }
  EXPECT_TRUE(any_stop_refused);
}

// Deadline and cancel stay separately observable (the outcome axis's whole point): the same
// probe shape, budget 1 ms, runs to the window and names deadline_exceeded — not cancelled.
TEST(QueryStopHandshake, DeadlineExhaustionNamesItselfNotACancel) {
  const ConfigManager config = survey::RandomPrismScene();
  ProbeRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 1, 2 } };  // the identity arm: the window fires at the first boundary
  request.along = ProbeAlong::kRadial;
  request.at_altitude_deg = 20.0;
  request.at_azimuth_deg = 25.0;
  request.wavelengths_nm = { 450.0, 550.0, 650.0 };
  request.budget_ms = 1;  // the lambda window fires at the first boundary
  ProbeResult result;
  const Error error = AnalyzePathProbe(config, request, &result);
  EXPECT_TRUE(error.Ok());
  EXPECT_TRUE(result.Data().meta.interrupted);
  EXPECT_EQ(result.Data().meta.execution, ExecutionOutcome::kDeadlineExceeded);
}

// The MC ESS leg: entered via the sampler's draw poll, cancelled, the leg refuses with its
// budget-exhausted shape (the stop folds into the one judgment the draws poll).
TEST(QueryStopHandshake, McEssDrawsAreEnteredThenCancelled) {
  const ConfigManager config = survey::RandomPrismScene();
  InputSnapshot snapshot;
  const std::vector<int> faces{ 3, 5, 7 };
  const std::vector<LayerSelection> selection{ { 0, 1, faces, 7 } };
  ASSERT_TRUE(CaptureInput(config.scene_, "handshake snapshot", selection, &snapshot).Ok());
  double sun_hat[3];
  SunIncidentDirection(config.scene_.light_source_.param_, sun_hat);
  analytic::StopToken token;
  analytic::DiscoveryStop stop;
  stop.token = &token;

  quantity::McRegionEssInput input;
  input.snapshot = &snapshot;
  input.wavelength_nm = 550.0;
  for (int k = 0; k < 3; k++) {
    input.sun_dir[static_cast<size_t>(k)] = -sun_hat[k];
  }
  input.member_count = 12;
  input.draws = 200000;  // the cap: long enough to be mid-run when the cancel lands
  input.budget_ms = 120000;
  input.stop = &stop;

  McRegionEssResult ess;
  WorkerHandle worker([&] { ess = quantity::EvaluateMcRegionEss(input); });
  ASSERT_TRUE(WaitForPoll(token, kPollSamplerDraws, kEnterBound)) << "the MC draws never polled";
  token.RequestCancel();
  ASSERT_TRUE(worker.JoinBounded(kJoinBound)) << "the MC draws ignored the cancel";
  EXPECT_EQ(ess.State(), survey::EvidenceState::kRefused);
  EXPECT_EQ(std::string(ess.Mode()), "budget_exhausted");  // the leg's honest truncation shape
}

// The report's phases in one handshake: discovery entered (sampler polls), then the
// enumeration entered (its member polls), cancelled inside the enumeration — the budget
// fields report the cut and the outcome names the cancel.
TEST(QueryStopHandshake, ReportEnumerationIsEnteredThenCancelled) {
  const ConfigManager config = survey::PlateScene(0.3, 0.3);
  PathFeatureReportRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 1, 5 } };
  request.sample_count = 64;
  request.budget_ms = 120000;
  request.max_optical_evaluations = 20000;
  request.max_field_evaluations = 1000000;
  analytic::StopToken token;
  request.cancel = &token;

  FeatureReportResult result;
  WorkerHandle worker([&] { (void)AnalyzePathFeatureReportValue(config, request, &result); });
  ASSERT_TRUE(WaitForPoll(token, kPollSamplerDraws, kEnterBound)) << "the report discovery never polled";
  ASSERT_TRUE(WaitForPoll(token, kPollEnumerationMember, kEnterBound))
      << "the enumeration leg never polled (discovery consumed everything?)";
  token.RequestCancel();
  ASSERT_TRUE(worker.JoinBounded(kJoinBound)) << "the report ignored the cancel";
  EXPECT_EQ(result.Data().execution, ExecutionOutcome::kCancelled);
  EXPECT_TRUE(result.Data().core.budget.truncated);  // the cut is declared, never hidden
}

}  // namespace
}  // namespace lumice::raypath
