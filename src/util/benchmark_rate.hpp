#pragma once

#include <cstdint>

namespace lumice {

// The rate estimator `Lumice benchmark` reports as `rays_per_sec` / `rate_basis`: given what the
// poll loop in RunBenchmarkPass (src/main.cpp) observed, decide which basis the numbers support
// and compute the rate on it. The poll loop only collects observations; every decision about
// what those observations are worth is made here, once.
//
// Pulled out of the poll loop so every branch is reachable on purpose. Inside the loop the
// `active_short` branch is reached only when a GPU drain-quantum publish and the IDLE transition
// land one poll apart — a scheduling race hit on a few percent of runs — so the branch that once
// reported ~20x the true rate could only be guarded by repeating whole benchmark runs until the
// race came up. As a pure function the same input is a literal.
//
// Lives in src/util/ because it is a stateless helper over plain numbers with no simulation or
// configuration semantics (the shape AGENTS.md admits there). Times are seconds as doubles, never
// clock types, so a test states them exactly.

// An active window no longer than this is too short to measure anything: neither `steady` nor
// `active_short` is taken at or below it, and the run falls through to `wall_fallback`.
constexpr double kMinActiveSec = 1e-4;

enum class RateBasis {
  kDrainAligned,  // infinite ray_num: rate over the drain #1 -> drain #(N+1) window
  kTooFewDrains,  // infinite ray_num, but the window never closed (or degenerated)
  kSteady,        // finite ray_num: rays after the first observed chunk / time after it
  kActiveShort,   // finite ray_num, counter published exactly once: wall-clock lower bound
  kWallFallback,  // finite ray_num, no usable active window at all: wall-clock lower bound
};

// The strings are the `rate_basis` values of the parsed [BENCHMARK] JSON contract.
constexpr const char* RateBasisName(RateBasis basis) {
  switch (basis) {
    case RateBasis::kDrainAligned:
      return "drain_aligned";
    case RateBasis::kTooFewDrains:
      return "too_few_drains";
    case RateBasis::kSteady:
      return "steady";
    case RateBasis::kActiveShort:
      return "active_short";
    case RateBasis::kWallFallback:
      return "wall_fallback";
  }
  return "wall_fallback";
}

struct RateObservation {
  // Both paths.
  double wall_sec = 0.0;    // run start -> the poll that saw IDLE
  std::uint64_t r_end = 0;  // sim_ray_num at that poll

  // Finite-ray_num path (drain_count_mode == false).
  bool active_started = false;             // some poll saw sim_ray_num > 0
  double active_sec = 0.0;                 // first poll with sim_ray_num > 0 -> the IDLE poll
  std::uint64_t rays_at_active_start = 0;  // sim_ray_num at that first poll

  // Infinite-ray_num path (drain_count_mode == true).
  bool drain_count_mode = false;
  bool window_closed = false;             // drain #(N+1) was observed
  double drain_window_sec = 0.0;          // drain #1 poll -> drain #(N+1) poll (as a duration)
  std::uint64_t rays_at_first_drain = 0;  // sim_ray_num at drain #1
  std::uint64_t rays_at_final_drain = 0;  // sim_ray_num at drain #(N+1)
};

struct RateEstimate {
  double rays_per_sec = 0.0;
  RateBasis basis = RateBasis::kWallFallback;
  // Set only on kDrainAligned; zero otherwise (the JSON reports them on the drain path only).
  double window_sec = 0.0;
  std::uint64_t window_rays = 0;
};

inline RateEstimate EstimateBenchmarkRate(const RateObservation& obs) {
  RateEstimate est;
  // Shared by `too_few_drains`, `active_short` and `wall_fallback`: all degrade to a wall-clock
  // lower bound. Computed once so the branches cannot silently diverge if only one is edited.
  const double wall_bounded_rate = obs.wall_sec > 0 ? static_cast<double>(obs.r_end) / obs.wall_sec : 0.0;

  if (obs.drain_count_mode) {
    if (obs.window_closed && obs.drain_window_sec > 0 && obs.rays_at_final_drain > obs.rays_at_first_drain) {
      est.window_sec = obs.drain_window_sec;
      est.window_rays = obs.rays_at_final_drain - obs.rays_at_first_drain;
      est.rays_per_sec = static_cast<double>(est.window_rays) / est.window_sec;
      est.basis = RateBasis::kDrainAligned;
    } else {
      est.rays_per_sec = wall_bounded_rate;
      est.basis = RateBasis::kTooFewDrains;
    }
  } else if (obs.active_sec > kMinActiveSec && obs.r_end > obs.rays_at_active_start) {
    est.rays_per_sec = static_cast<double>(obs.r_end - obs.rays_at_active_start) / obs.active_sec;
    est.basis = RateBasis::kSteady;
  } else if (obs.active_started && obs.active_sec > kMinActiveSec) {
    // Degenerate window: sim_ray_num was observed exactly ONCE (r_end == rays_at_active_start), so
    // there is no interior sample and `active_sec` measures IDLE-detection latency, not trace
    // duration. On a GPU backend this is not hypothetical: sim_ray_num advances in whole drain
    // quanta (kDefaultXyzDrainBatches * dispatch_size = 64 * 32768 = 2,097,152 rays with the Metal
    // default), so a config whose entire ray_num is below one quantum publishes its counter exactly
    // once, at the end — whether that publish shares a poll with the IDLE transition or precedes
    // it by one is a race, which is why the same binary/config/machine alternated between
    // `wall_fallback` and a measured 14-29x phantom rate at ~4% of runs when this branch divided
    // by `active_sec` itself. Use the wall-clock denominator instead — it is a real duration that
    // provably contains the whole trace, so the number is a conservative LOWER bound on the true
    // rate rather than an unbounded upward fantasy. Same formula as `wall_fallback`, but the basis
    // label is deliberately kept distinct: the two say different things (`active_short` = exactly
    // one counter publish observed; `wall_fallback` = no usable active window at all), and merging
    // them would change the rate_basis value set for no gain. See doc/performance-testing.md §C
    // rule 5 for the full derivation (quantum value, 240-run measurement, short-window bias decay
    // curve).
    est.rays_per_sec = wall_bounded_rate;
    est.basis = RateBasis::kActiveShort;
  } else {
    est.rays_per_sec = wall_bounded_rate;
    est.basis = RateBasis::kWallFallback;
  }
  return est;
}

}  // namespace lumice
