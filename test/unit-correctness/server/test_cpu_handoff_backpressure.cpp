// Regression guard for the CPU-route handoff backpressure (scene_cnt_cap_ / scene_cnt_wake_ /
// scene_cv_) in the "batch count is small but each handoff carries a lot of SimData" shape: when
// the CPU-route dispatch grain (LUMICE_CPU_HANDOFF_BATCHES) grows, both scene_cnt_cap_/
// scene_cnt_wake_'s VALUE and scene_cv_'s WAKE PREDICATE must move together, or the producer can
// stall forever once the in-flight ceiling is hit. A backpressure threshold sized in one unit
// (batch count) whose wake condition is not re-derived when the unit's meaning changes (dispatch
// grain becomes variable) is a recurring defect shape in this codebase — the threshold value and
// the predicate that reads it must be updated as one, not independently.
//
// Driven through the public C API (matches test_drain_contract.cpp's approach) rather than
// ServerImpl internals: scene_cnt_cap_/scene_cnt_wake_/scene_cv_ are private, and the property
// under test — "the producer is not permanently stalled" — is only observable from outside as
// "the run completes". A single worker keeps queued/cap small relative to a large handoff grain,
// so GenerateScene is forced to block on scene_cv_.wait() at least once (arithmetic verified
// below); if the wake predicate were left keyed to the old constant threshold instead of the
// grain-scaled one, that wait would never see sim_scene_cnt_ fall back under scene_cnt_wake_ and
// WaitForDrain would time out.

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>

#include "lumice.h"
#include "support/env_var.hpp"

namespace {

// One worker: workers=1, queued=max(workers*kQueuedHandoffsPerWorker, 2)=2 on every platform
// (kQueuedHandoffsPerWorker is 0.25 on Windows / 1.0 elsewhere, both floor to the same `2` at
// workers=1). With LUMICE_CPU_HANDOFF_BATCHES=512 (single-wavelength D65, so per_handoff=512):
//   scene_cnt_cap_  = max((1+2)*512, kMaxSceneCnt=128)  = 1536
//   scene_cnt_wake_ = max((1+1)*512, kMaxSceneCnt/2=64) = 1024
// Each handoff credits 512 SimData (one physics batch per 128 rays), so the 4th handoff
// (cumulative 2048) is what pushes sim_scene_cnt_ past scene_cnt_cap_ and forces GenerateScene to
// block — a run of >= 4 handoffs' worth of rays is therefore guaranteed to exercise the wait/wake
// path, not just approach it. 700000 rays / 65536 rays-per-handoff (512 physics batches * 128
// rays) is ~11 handoffs, giving several wait/wake cycles with headroom for scheduling jitter.
constexpr const char* kHandoffBatchesEnv = "512";
constexpr unsigned long long kRayNum = 700000;

std::string BuildConfig(unsigned long long ray_num) {
  return R"({
  "crystal": [{
    "id": 1, "type": "prism",
    "shape": {"height": 1.5},
    "axis": {"zenith": {"type": "gauss", "mean": 90.0, "std": 10.0},
             "azimuth": {"type": "uniform", "mean": 0.0, "std": 180.0},
             "roll": {"type": "uniform", "mean": 0.0, "std": 180.0}}
  }],
  "filter": [],
  "scene": {
    "light_source": {"type": "sun", "altitude": 20.0, "azimuth": 0.0,
                     "diameter": 0.5, "spectrum": "D65"},
    "ray_num": )" +
         std::to_string(ray_num) + R"(,
    "max_hits": 8,
    "scattering": [{"prob": 0.0, "entries": [{"crystal": 1, "proportion": 1.0}]}]
  },
  "render": [{
    "id": 1,
    "lens": {"type": "dual_fisheye_equal_area", "fov": 180.0},
    "resolution": [64, 32],
    "view": {"elevation": 0, "azimuth": 0, "roll": 0},
    "visible": "full", "background": [0, 0, 0],
    "intensity_factor": 1.0
  }]
})";
}

LUMICE_ErrorCode CommitJson(LUMICE_Server* server, const std::string& json) {
  LUMICE_Scene* scene = nullptr;
  if (auto err = LUMICE_SceneFromJson(json.c_str(), &scene); err != LUMICE_OK) {
    return err;
  }
  const auto err = LUMICE_CommitScene(server, scene, /*out_reused=*/nullptr);
  LUMICE_SceneDestroy(scene);
  return err;
}

bool IsDrained(const LUMICE_DrainResult& d) {
  return d.drained_epoch == d.current_epoch;
}

bool WaitForDrain(LUMICE_Server* server, int timeout_ms) {
  using clock = std::chrono::steady_clock;
  const auto deadline = clock::now() + std::chrono::milliseconds(timeout_ms);
  while (clock::now() < deadline) {
    LUMICE_DrainResult drain{};
    if (LUMICE_GetDrainStatus(server, &drain) == LUMICE_OK && IsDrained(drain)) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  return false;
}

class CpuHandoffBackpressure : public ::testing::Test {
 protected:
  void SetUp() override {
    // Forces a large, "few handoffs" dispatch grain regardless of the platform default — the
    // arithmetic in the comment above kHandoffBatchesEnv assumes this exact value.
    lumice::test::SetEnvVar("LUMICE_CPU_HANDOFF_BATCHES", kHandoffBatchesEnv);
    LUMICE_ServerConfig cfg{};
    cfg.num_workers = 1;
    server_ = LUMICE_CreateServerEx(&cfg);
    ASSERT_NE(server_, nullptr);
  }

  void TearDown() override {
    if (server_ != nullptr) {
      LUMICE_StopServer(server_);
      LUMICE_DestroyServer(server_);
      server_ = nullptr;
    }
    lumice::test::UnsetEnvVar("LUMICE_CPU_HANDOFF_BATCHES");
  }

  LUMICE_Server* server_ = nullptr;
};

// THE liveness property under test: with a handoff grain large enough that the in-flight
// ceiling is reached in a handful of handoffs (see the arithmetic above), the run must still
// reach drained within a bounded time. A wake predicate that was not updated in lockstep with
// the grain-scaled cap/wake thresholds (the exact 509.2-shaped regression) would leave
// GenerateScene parked on scene_cv_.wait() forever once the ceiling is first hit, and this
// would time out instead of draining.
TEST_F(CpuHandoffBackpressure, ProducerIsWokenAfterFewLargeHandoffsAndRunDrains) {
  ASSERT_EQ(CommitJson(server_, BuildConfig(kRayNum)), LUMICE_OK);
  ASSERT_TRUE(WaitForDrain(server_, 60000))
      << "run never drained — the producer likely stalled on scene_cv_.wait() after the "
         "in-flight ceiling was reached by a small number of large handoffs";

  LUMICE_ResultFrame* frame = nullptr;
  ASSERT_EQ(LUMICE_AcquireResultFrame(server_, &frame), LUMICE_OK);
  ASSERT_NE(frame, nullptr);
  LUMICE_StatsResult stats{};
  ASSERT_EQ(LUMICE_FrameGetStats(frame, &stats), LUMICE_OK);
  LUMICE_ReleaseResultFrame(frame);
  EXPECT_EQ(stats.sim_ray_num, kRayNum)
      << "drained but with a short total — a lost wake could also manifest as under-tracing "
         "rather than an outright hang";
}

// Same shape at the smallest possible handoff count: a run whose entire ray budget is a single
// handoff never reaches the cap at all, so this is the negative control confirming the fixture
// above is not just exercising ordinary completion — it isolates "many handoffs, ceiling
// reached and released" from "trivially fits under the ceiling".
TEST_F(CpuHandoffBackpressure, SingleHandoffRunNeverBlocksAndDrains) {
  // 512 physics batches * 128 rays = 65536 rays is exactly one handoff at
  // LUMICE_CPU_HANDOFF_BATCHES=512 — well under scene_cnt_cap_=1536, so GenerateScene's fast
  // path (no scene_cv_.wait()) is what this run exercises.
  constexpr unsigned long long kSingleHandoffRayNum = 65536;
  ASSERT_EQ(CommitJson(server_, BuildConfig(kSingleHandoffRayNum)), LUMICE_OK);
  ASSERT_TRUE(WaitForDrain(server_, 60000));

  LUMICE_ResultFrame* frame = nullptr;
  ASSERT_EQ(LUMICE_AcquireResultFrame(server_, &frame), LUMICE_OK);
  ASSERT_NE(frame, nullptr);
  LUMICE_StatsResult stats{};
  ASSERT_EQ(LUMICE_FrameGetStats(frame, &stats), LUMICE_OK);
  LUMICE_ReleaseResultFrame(frame);
  EXPECT_EQ(stats.sim_ray_num, kSingleHandoffRayNum);
}

}  // namespace
