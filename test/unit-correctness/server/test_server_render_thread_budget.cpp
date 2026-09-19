// The server's idle-core budget for its RenderConsumers — the number every row-parallel W*H loop
// on the render side (visible mask, annotation masks, PostSnapshot's fused pixel loop) is allowed
// to occupy, see core/parallel_rows.hpp — is
//
//     render_thread_budget = max(0, PhysicalCoreCount() − worker_count)
//
// computed once in ServerImpl's constructor next to worker_count. Physical cores, not
// hardware_concurrency(): on an SMT machine the logical count minus the workers is not idle
// capacity (the extra threads land on the busy cores' siblings — see the member's declaration
// in server.cpp for the measurement), and on a machine without SMT the two are equal. This file
// pins the formula at the worker counts that matter: 1 (the GPU single-engine route — the GUI on
// that route keeps phys−1 cores of parallelism), phys−2 (the typical CPU-route shape, where the
// budget sits exactly on ParallelRows' "< 2 ⇒ inline" gate and must clear it), phys (every core
// busy — the clamp yields 0 and the consumers run their loops inline; what "inline at budget 0"
// means is held by test_parallel_rows.cpp, and that a budget-0 consumer produces the same bytes
// as an unconstrained one by test_render_consumer_post_snapshot_fusion.cpp) and phys+3 (an
// explicit worker count above the core count, which the clamp must absorb).
//
// The value is read off the one channel the live server exposes it on — the construction-time
// `ServerImpl: ... render_thread_budget=N` log line, whose shape the regression sentinels under
// test/regression-sentinel/ also parse for worker_count — rather than through a test-only
// accessor: the formula's inputs are the same two numbers that line already carries, and a
// private member read added for a test would be a second surface to keep in step with the log.
//
// The GPU-route case constructs a real GPU-preferred server and reads the worker_count IT settled
// on, instead of assuming 1: the claim is about the route the server actually sized itself for.

#include <gtest/gtest.h>

#include <algorithm>
#include <regex>
#include <string>

#include "server/server.hpp"
#include "support/log_capture.hpp"
#include "util/cpu_info.hpp"
#include "util/logger.hpp"

namespace lumice {
namespace {

struct ConstructionLine {
  int worker_count = -1;
  int render_thread_budget = -1;
};

// The construction line of the LAST server built while `text` was captured. A capture may hold
// lines from other servers in the same process only if they were built inside this capture's
// lifetime, which no case here does — but "last" keeps the parse honest if one ever does.
ConstructionLine ParseConstructionLine(const std::string& text) {
  static const std::regex kLine(R"(ServerImpl: gpu_route=\w+ worker_count=(\d+) analysis_pool_worker_count=\d+ )"
                                R"(render_thread_budget=(\d+))");
  ConstructionLine out;
  for (auto it = std::sregex_iterator(text.begin(), text.end(), kLine); it != std::sregex_iterator(); ++it) {
    out.worker_count = std::stoi((*it)[1].str());
    out.render_thread_budget = std::stoi((*it)[2].str());
  }
  return out;
}

int ExpectedBudget(int worker_count) {
  return std::max(0, PhysicalCoreCount() - worker_count);
}

ConstructionLine BuildCpuServerAndRead(int num_workers) {
  test::LogCapture capture;
  Server server(num_workers, 0, BackendKind::kCpu);
  const ConstructionLine line = ParseConstructionLine(capture.Text());
  server.Stop();
  return line;
}

TEST(ServerRenderThreadBudget, OneWorkerLeavesAllButOneCore) {
  const ConstructionLine line = BuildCpuServerAndRead(1);
  ASSERT_EQ(line.worker_count, 1)
      << "the construction line was not logged, or an explicit num_workers was not honoured";
  EXPECT_EQ(line.render_thread_budget, ExpectedBudget(1));
  EXPECT_EQ(line.render_thread_budget, PhysicalCoreCount() - 1);
}

TEST(ServerRenderThreadBudget, TwoIdleCoresIsABudgetOfExactlyTwo) {
  const int phys = PhysicalCoreCount();
  if (phys < 4) {
    GTEST_SKIP() << "PhysicalCoreCount=" << phys << ": phys−2 would not be a multi-worker pool";
  }
  const ConstructionLine line = BuildCpuServerAndRead(phys - 2);
  ASSERT_EQ(line.worker_count, phys - 2);
  EXPECT_EQ(line.render_thread_budget, 2) << "phys−2 workers must leave a budget that clears ParallelRows' < 2 gate";
}

TEST(ServerRenderThreadBudget, EveryCoreBusyClampsTheBudgetToZero) {
  const int phys = PhysicalCoreCount();
  const ConstructionLine line = BuildCpuServerAndRead(phys);
  ASSERT_EQ(line.worker_count, phys);
  EXPECT_EQ(line.render_thread_budget, 0);
}

TEST(ServerRenderThreadBudget, MoreWorkersThanCoresStillClampsToZero) {
  // An explicit num_workers above the core count is honoured verbatim by the server, so the
  // difference goes negative and the max(0, ·) clamp is what keeps the budget a count.
  const int phys = PhysicalCoreCount();
  const ConstructionLine line = BuildCpuServerAndRead(phys + 3);
  ASSERT_EQ(line.worker_count, phys + 3);
  EXPECT_EQ(line.render_thread_budget, 0);
}

TEST(ServerRenderThreadBudget, GpuRouteBudgetIsPhysicalCoresMinusItsSingleWorker) {
#if defined(__APPLE__) || defined(LUMICE_CUDA_ENABLED)
#if defined(__APPLE__)
  const BackendKind kGpu = BackendKind::kMetal;
#else
  const BackendKind kGpu = BackendKind::kCuda;
#endif
  Logger probe{ "ServerRenderThreadBudget" };
  if (!ResolveGpuRoute(kGpu, probe)) {
    GTEST_SKIP() << "GPU route unavailable on this machine (or overridden by LUMICE_TRACE_BACKEND)";
  }
  test::LogCapture capture;
  // num_workers=0 (automatic): on the GPU route it sizes only the standing analysis pool, so the
  // render worker count is whatever the route decides — that is the number under test.
  Server server(0, 0, kGpu);
  const ConstructionLine line = ParseConstructionLine(capture.Text());
  server.Stop();
  ASSERT_GE(line.worker_count, 1) << "the construction line was not logged";
  EXPECT_EQ(line.worker_count, 1) << "the GPU single-engine route sizes to one worker";
  EXPECT_EQ(line.render_thread_budget, ExpectedBudget(line.worker_count));
  EXPECT_EQ(line.render_thread_budget, PhysicalCoreCount() - 1);
#else
  GTEST_SKIP() << "no GPU backend in this build";
#endif
}

}  // namespace
}  // namespace lumice
