// Developer microbench for RenderConsumer::PostSnapshot()'s fused pixel loop at the HI-RES tier,
// the segment that doc/gui-preview-lifecycle-architecture.md §12.1 measured at 94.7% of DoSnapshot
// (87,614 us of 92,520 us at 2048x1024). It is a manual before/after tool for changes to that loop
// — the row-parallel dispatch (core/parallel_rows.hpp) was accepted on this bench's same-process
// controlled A/B (§12.4) — and sits on the same footing as the other bench_*.cpp files here: built
// by CI's bench compile-only job, run by hand, not wired into the benchmark-summary report chain.
#include <benchmark/benchmark.h>

#include <cmath>
#include <numeric>
#include <random>
#include <vector>

#include "config/color_class_table.hpp"
#include "config/light_config.hpp"
#include "config/render_config.hpp"
#include "config/sim_data.hpp"
#include "server/render.hpp"
#include "test/support/render_anchor.hpp"
#include "test/support/thread_budget.hpp"

using namespace lumice;  // NOLINT(google-build-using-namespace) benchmark code

namespace {

// Matches doc/gui-preview-lifecycle-architecture.md §12.1's HI-RES tier (2048x1024, dual-fisheye,
// no raypath colouring) as closely as a standalone microbench can, so its numbers sit on the same
// footing as the segment-timing baseline they are compared against.
RenderConfig MakeHiResConfig(bool with_markers) {
  RenderConfig cfg;
  cfg.id_ = 0;
  cfg.lens_.type_ = LensParam::kDualFisheyeEqualArea;
  cfg.lens_.fov_ = 180.0f;
  cfg.resolution_[0] = 2048;
  cfg.resolution_[1] = 1024;
  cfg.visible_ = RenderConfig::kFull;
  if (with_markers) {
    cfg.markers_radius_px_ = 20.0f;
    cfg.markers_opacity_ = 1.0f;
    cfg.markers_.push_back({ MarkerRefId::kZenith, true, { 1.0f, 0.0f, 0.0f } });
    cfg.markers_.push_back({ MarkerRefId::kNadir, true, { 0.0f, 1.0f, 0.0f } });
  }
  return cfg;
}

SimData MakeScatteredBatch(size_t n_rays) {
  SimData data;
  data.curr_wl_ = 550.0f;
  std::mt19937 rng(20260919);
  std::uniform_real_distribution<float> ang(0.0f, 1.0f);
  data.outgoing_d_.reserve(n_rays * 3);
  data.outgoing_w_.reserve(n_rays);
  for (size_t i = 0; i < n_rays; ++i) {
    const float u = ang(rng);
    const float v = ang(rng);
    const float theta = std::acos(1.0f - 2.0f * u);
    const float phi = 2.0f * 3.14159265358979323846f * v;
    data.outgoing_d_.push_back(std::sin(theta) * std::cos(phi));
    data.outgoing_d_.push_back(std::sin(theta) * std::sin(phi));
    data.outgoing_d_.push_back(std::cos(theta));
    data.outgoing_w_.push_back(0.3f + 0.7f * ang(rng));
  }
  data.emitted_energy_ = std::accumulate(data.outgoing_w_.begin(), data.outgoing_w_.end(), 0.0f);
  return data;
}

// Arg 0: markers on/off. Arg 1: the thread budget handed to the consumer (core/parallel_rows.hpp),
// where kUnconstrained resolves to hardware_concurrency() at run time — the value the pool was
// sized to before the budget existed, and what the property tests pass. The three tiers
// registered below are the three points the budget rule is judged on: 1 = the forced inline
// serial loop (what a server with every core busy gets), 2 = the smallest budget that stands up a
// pool (a 12-core machine running 10 simulation workers), unconstrained = the old full-core pool.
// Budget 2 relative to 1 is the number the GUI keeps from the parallelization once the pool no
// longer competes with the workers.
constexpr int kUnconstrained = 0;

void BM_PostSnapshotHiRes(benchmark::State& state) {
  const bool with_markers = state.range(0) != 0;
  const int thread_budget =
      state.range(1) == kUnconstrained ? lumice::test::kTestThreadBudget : static_cast<int>(state.range(1));
  RenderConfig cfg = MakeHiResConfig(with_markers);
  RenderConsumer rc(cfg, thread_budget, ColorClassTable{}, SunParam{ 35.0f, 20.0f, 0.5f });
  auto data = MakeScatteredBatch(500000);
  rc.Consume(data);
  // Anchor + PrepareSnapshot once, matching the property-test helper's ordering; only PostSnapshot
  // itself (the fused pixel loop) is inside the timed region.
  rc.PrepareSnapshot();
  const RawXyzResult raw = rc.GetRawXyzResult();
  const float p99 = ComputeP99Y(raw.xyz_buffer_, raw.img_width_, raw.img_height_, kMonoAnchorDownsampleFactor);
  const float omega_axis = rc.AxisSolidAngle();
  rc.SetAnchorL99Sky(omega_axis > 0.0f ? p99 / omega_axis : 0.0f);
  for (auto _ : state) {  // NOLINT(readability-identifier-naming) benchmark idiom
    rc.PostSnapshot();
  }
}
BENCHMARK(BM_PostSnapshotHiRes)
    ->ArgNames({ "markers", "budget" })
    ->Args({ 0, 1 })
    ->Args({ 0, 2 })
    ->Args({ 0, kUnconstrained })
    ->Args({ 1, 1 })
    ->Args({ 1, 2 })
    ->Args({ 1, kUnconstrained })
    ->Unit(benchmark::kMillisecond)
    ->Iterations(30);

}  // namespace
