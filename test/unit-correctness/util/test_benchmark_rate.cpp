// The rate estimator behind `Lumice benchmark`'s `rays_per_sec` / `rate_basis`, one literal input
// per branch. In a real run the `active_short` branch is reached only when a GPU drain-quantum
// publish and the IDLE transition land one poll apart — a race hit on a few percent of runs — so
// these cases are what makes that branch reachable on every run rather than by repetition.
//
// Inputs are hand-picked integers whose rates divide exactly, so the expected values are spelled
// out rather than re-derived from the formula. The one property that does not restate the code is
// `DegenerateBasesNeverClaimMoreWorkThanTheRunHadTimeFor`: rays claimed per second times the run's
// wall clock cannot exceed the rays the run traced. That is the quantity the original defect
// (dividing a single-publish run by IDLE-detection latency) broke by ~20x.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "util/benchmark_rate.hpp"

namespace lumice {
namespace {

// A finite-ray_num run whose counter was observed more than once.
RateObservation SteadyRun() {
  RateObservation obs;
  obs.wall_sec = 2.0;
  obs.r_end = 4000;
  obs.active_started = true;
  obs.active_sec = 1.5;
  obs.rays_at_active_start = 1000;
  return obs;
}

// A finite-ray_num run whose counter was published exactly once (below one drain quantum), one
// poll before IDLE was seen: active_sec is IDLE-detection latency, not a trace duration.
RateObservation SinglePublishRun(double wall_sec, double active_sec) {
  RateObservation obs;
  obs.wall_sec = wall_sec;
  obs.r_end = 1'000'000;
  obs.active_started = true;
  obs.active_sec = active_sec;
  obs.rays_at_active_start = 1'000'000;
  return obs;
}

// An infinite-ray_num run whose drain #1 -> drain #11 window closed.
RateObservation ClosedDrainWindowRun() {
  RateObservation obs;
  obs.drain_count_mode = true;
  obs.wall_sec = 3.0;
  obs.r_end = 23'068'672;
  obs.active_started = true;
  obs.active_sec = 2.5;
  obs.rays_at_active_start = 2'097'152;
  obs.window_closed = true;
  obs.drain_window_sec = 2.0;
  obs.rays_at_first_drain = 2'097'152;
  obs.rays_at_final_drain = 23'068'672;
  return obs;
}

TEST(BenchmarkRate, BasisNamesAreTheJsonContractStrings) {
  EXPECT_EQ(std::string(RateBasisName(RateBasis::kDrainAligned)), "drain_aligned");
  EXPECT_EQ(std::string(RateBasisName(RateBasis::kTooFewDrains)), "too_few_drains");
  EXPECT_EQ(std::string(RateBasisName(RateBasis::kSteady)), "steady");
  EXPECT_EQ(std::string(RateBasisName(RateBasis::kActiveShort)), "active_short");
  EXPECT_EQ(std::string(RateBasisName(RateBasis::kWallFallback)), "wall_fallback");
}

TEST(BenchmarkRate, SteadyExcludesTheFirstChunkAndTheSetup) {
  const RateEstimate est = EstimateBenchmarkRate(SteadyRun());
  EXPECT_EQ(est.basis, RateBasis::kSteady);
  EXPECT_DOUBLE_EQ(est.rays_per_sec, 2000.0);  // (4000 - 1000) / 1.5
  EXPECT_EQ(est.window_sec, 0.0);
  EXPECT_EQ(est.window_rays, 0u);
}

TEST(BenchmarkRate, SinglePublishDividesByTheWallClockNotByIdleLatency) {
  const RateEstimate est = EstimateBenchmarkRate(SinglePublishRun(/*wall_sec=*/0.25, /*active_sec=*/0.001));
  EXPECT_EQ(est.basis, RateBasis::kActiveShort);
  EXPECT_DOUBLE_EQ(est.rays_per_sec, 4'000'000.0);  // 1,000,000 / 0.25, not / 0.001
}

TEST(BenchmarkRate, DegenerateBasesNeverClaimMoreWorkThanTheRunHadTimeFor) {
  // Sweep the single-publish domain toward active_sec -> kMinActiveSec, where the defect's phantom
  // rate grew without bound. work_ratio is 1.0 for every honest wall-bounded estimate.
  const double walls[] = { 0.25, 1.0, 4.0 };
  const double actives[] = { 0.2, 0.05, 0.01, 1e-3, 2e-4, kMinActiveSec * 1.0001 };
  for (const double wall : walls) {
    for (const double active : actives) {
      if (active >= wall) {
        continue;
      }
      const RateObservation obs = SinglePublishRun(wall, active);
      const RateEstimate est = EstimateBenchmarkRate(obs);
      EXPECT_EQ(est.basis, RateBasis::kActiveShort) << "wall=" << wall << " active=" << active;
      const double work_ratio = est.rays_per_sec * obs.wall_sec / static_cast<double>(obs.r_end);
      EXPECT_LE(work_ratio, 1.0 + 1e-12) << "wall=" << wall << " active=" << active;
    }
  }
}

TEST(BenchmarkRate, WallFallbackWhenTracingWasNeverSeenActive) {
  RateObservation obs = SinglePublishRun(/*wall_sec=*/0.5, /*active_sec=*/0.2);
  obs.active_started = false;
  const RateEstimate est = EstimateBenchmarkRate(obs);
  EXPECT_EQ(est.basis, RateBasis::kWallFallback);
  EXPECT_DOUBLE_EQ(est.rays_per_sec, 2'000'000.0);  // 1,000,000 / 0.5
}

TEST(BenchmarkRate, AnActiveWindowAtTheThresholdIsTooShortForEitherMeasuredBasis) {
  // Strictly greater than kMinActiveSec is required, for both steady and active_short.
  RateObservation steady = SteadyRun();
  steady.active_sec = kMinActiveSec;
  EXPECT_EQ(EstimateBenchmarkRate(steady).basis, RateBasis::kWallFallback);
  EXPECT_DOUBLE_EQ(EstimateBenchmarkRate(steady).rays_per_sec, 2000.0);  // 4000 / 2.0

  const RateObservation single = SinglePublishRun(/*wall_sec=*/0.25, /*active_sec=*/kMinActiveSec);
  EXPECT_EQ(EstimateBenchmarkRate(single).basis, RateBasis::kWallFallback);
}

TEST(BenchmarkRate, AZeroWallClockReportsZeroRatherThanDividing) {
  RateObservation obs = SinglePublishRun(/*wall_sec=*/0.0, /*active_sec=*/0.0);
  const RateEstimate est = EstimateBenchmarkRate(obs);
  EXPECT_EQ(est.basis, RateBasis::kWallFallback);
  EXPECT_EQ(est.rays_per_sec, 0.0);

  obs.drain_count_mode = true;
  const RateEstimate drain = EstimateBenchmarkRate(obs);
  EXPECT_EQ(drain.basis, RateBasis::kTooFewDrains);
  EXPECT_EQ(drain.rays_per_sec, 0.0);
}

TEST(BenchmarkRate, DrainAlignedMeasuresOnlyTheClosedWindow) {
  const RateEstimate est = EstimateBenchmarkRate(ClosedDrainWindowRun());
  EXPECT_EQ(est.basis, RateBasis::kDrainAligned);
  EXPECT_EQ(est.window_rays, 20'971'520u);  // 23,068,672 - 2,097,152: ten drains
  EXPECT_EQ(est.window_sec, 2.0);
  EXPECT_DOUBLE_EQ(est.rays_per_sec, 10'485'760.0);  // 20,971,520 / 2.0
}

TEST(BenchmarkRate, TooFewDrainsWhenTheWindowNeverClosedOrDegenerated) {
  RateObservation not_closed = ClosedDrainWindowRun();
  not_closed.window_closed = false;
  RateObservation zero_duration = ClosedDrainWindowRun();
  zero_duration.drain_window_sec = 0.0;
  RateObservation no_new_rays = ClosedDrainWindowRun();
  no_new_rays.rays_at_final_drain = no_new_rays.rays_at_first_drain;

  for (const RateObservation& obs : { not_closed, zero_duration, no_new_rays }) {
    const RateEstimate est = EstimateBenchmarkRate(obs);
    // The drain path never falls through to the finite-path bases, even though this run's active
    // window would qualify for `steady`.
    EXPECT_EQ(est.basis, RateBasis::kTooFewDrains);
    EXPECT_DOUBLE_EQ(est.rays_per_sec, 23'068'672.0 / 3.0);
    EXPECT_EQ(est.window_sec, 0.0);
    EXPECT_EQ(est.window_rays, 0u);
  }
}

}  // namespace
}  // namespace lumice
