// LUMICE_ContinueRender: trace more rays INTO a finished render's accumulation.
//
// Three propositions, each pinned from the side it can fail on:
//   1. Nothing a commit would reset is reset. The ray count, the emitted energy and the
//      render planes of "N, then N more" add up to those of the two halves, and the exposure
//      anchor lands where a single 2N run's does (ContinueAccumulates*).
//   2. The added rays are NEW samples, also under a fixed seed — the case where re-entering
//      a run would otherwise restart the stream and trace the first N rays a second time
//      (ContinuationIsANewStream*). Checked on the per-pixel increment, not on a total: a
//      replay adds exactly what is already there, which no sum-level statistic can tell from
//      "more of the same scene".
//   3. The lifecycle around it is the ordinary one — a new epoch, the drain signal re-armed,
//      RUNNING then COMPLETED — and the calls that have nothing to continue are refused
//      without side effects, the analysis session among them.
//
// Driven through the public C API: the contract is the one a GUI or a script consumes.

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <thread>
#include <vector>

#include "lumice.h"

namespace {

// Small image, cheap scene: the propositions here are about accumulation, not physics.
std::string MakeConfig(const std::string& ray_num) {
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
         ray_num + R"(,
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

constexpr unsigned long long kHalf = 100000;

LUMICE_ErrorCode CommitJson(LUMICE_Server* server, const std::string& json) {
  LUMICE_Scene* scene = nullptr;
  if (auto err = LUMICE_SceneFromJson(json.c_str(), &scene); err != LUMICE_OK) {
    return err;
  }
  const auto err = LUMICE_CommitScene(server, scene, /*out_reused=*/nullptr);
  LUMICE_SceneDestroy(scene);
  return err;
}

LUMICE_SimLifecycleResult ReadLifecycle(LUMICE_Server* server) {
  LUMICE_SimLifecycleResult lc{};
  EXPECT_EQ(LUMICE_GetSimLifecycle(server, &lc), LUMICE_OK);
  return lc;
}

bool WaitForDrain(LUMICE_Server* server, int timeout_ms = 60000) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    LUMICE_DrainResult d{};
    EXPECT_EQ(LUMICE_GetDrainStatus(server, &d), LUMICE_OK);
    if (d.drained_epoch == d.current_epoch) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  return false;
}

// Everything the propositions read, off ONE frame so the fields describe one snapshot.
struct Accumulation {
  unsigned long long sim_ray_num = 0;
  float emitted_energy = 0.0f;
  float anchor_l99_sky = 0.0f;
  std::vector<double> xyz;
};

Accumulation Read(LUMICE_Server* server) {
  Accumulation acc;
  LUMICE_ResultFrame* frame = nullptr;
  EXPECT_EQ(LUMICE_AcquireResultFrame(server, &frame), LUMICE_OK);
  if (frame == nullptr) {
    return acc;
  }
  LUMICE_StatsResult stats{};
  EXPECT_EQ(LUMICE_FrameGetStats(frame, &stats), LUMICE_OK);
  acc.sim_ray_num = stats.sim_ray_num;
  LUMICE_RawXyzResult raw[2]{};
  EXPECT_EQ(LUMICE_FrameGetRawXyz(frame, raw, 2), LUMICE_OK);
  if (raw[0].xyz_buffer != nullptr) {
    const size_t n = static_cast<size_t>(raw[0].img_width) * raw[0].img_height * 3;
    acc.xyz.assign(raw[0].xyz_buffer, raw[0].xyz_buffer + n);
    acc.emitted_energy = raw[0].emitted_energy;
    acc.anchor_l99_sky = raw[0].anchor_l99_sky;
  }
  LUMICE_ReleaseResultFrame(frame);
  return acc;
}

double Sum(const std::vector<double>& v) {
  double s = 0.0;
  for (double x : v) {
    s += x;
  }
  return s;
}

// Relative L1 distance ||a - b|| / ||b||.
double RelL1(const std::vector<double>& a, const std::vector<double>& b) {
  double num = 0.0;
  double den = 0.0;
  for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
    num += std::abs(a[i] - b[i]);
    den += std::abs(b[i]);
  }
  return den > 0.0 ? num / den : 0.0;
}

std::vector<double> Minus(const std::vector<double>& a, const std::vector<double>& b) {
  std::vector<double> d(a.size());
  for (size_t i = 0; i < a.size(); ++i) {
    d[i] = a[i] - b[i];
  }
  return d;
}

LUMICE_Server* MakeServer(unsigned int seed, int backend, int num_workers = 0) {
  LUMICE_ServerConfig cfg{};
  cfg.num_workers = num_workers;
  cfg.sim_seed = seed;
  cfg.preferred_backend = backend;
  return LUMICE_CreateServerEx(&cfg);
}

void Destroy(LUMICE_Server* server) {
  if (server != nullptr) {
    LUMICE_StopServer(server);
    LUMICE_DestroyServer(server);
  }
}

// Run N, then continue with N more; returns the accumulation after each half.
void RunThenContinue(LUMICE_Server* server, Accumulation* first, Accumulation* both) {
  ASSERT_EQ(CommitJson(server, MakeConfig(std::to_string(kHalf))), LUMICE_OK);
  ASSERT_TRUE(WaitForDrain(server)) << "the first half never drained";
  *first = Read(server);
  ASSERT_EQ(LUMICE_ContinueRender(server, /*infinite=*/0, kHalf), LUMICE_OK);
  ASSERT_TRUE(WaitForDrain(server)) << "the continuation never drained";
  *both = Read(server);
}

// The increment a continuation added, measured against the first half. A replay of the first
// half's stream adds exactly the first half again: the distance is float rounding (~1e-7).
// Two independent halves of this scene differ by Monte-Carlo noise, which at kHalf rays over
// 64x32 pixels is tens of percent. 1e-3 sits four orders of magnitude from either.
constexpr double kReplayRelL1 = 1e-3;

void ExpectNewStream(unsigned int seed, int backend) {
  LUMICE_Server* server = MakeServer(seed, backend);
  ASSERT_NE(server, nullptr);
  Accumulation first;
  Accumulation both;
  RunThenContinue(server, &first, &both);
  Destroy(server);
  if (::testing::Test::HasFatalFailure()) {
    return;
  }
  ASSERT_EQ(both.xyz.size(), first.xyz.size());
  ASSERT_GT(Sum(first.xyz), 0.0) << "the first half landed nothing — the comparison below would be vacuous";
  const double d = RelL1(Minus(both.xyz, first.xyz), first.xyz);
  EXPECT_GT(d, kReplayRelL1) << "the continuation added exactly what the first half had already added: the second N "
                                "rays are a replay of the first N (seed "
                             << seed << ", backend " << backend << ")";
}

}  // namespace

// ---- Proposition 1: nothing is reset -------------------------------------------------------

TEST(ContinueRender, ContinueAccumulatesCountEnergyAndPlanes) {
  LUMICE_Server* server = MakeServer(/*seed=*/0, LUMICE_BACKEND_CPU);
  ASSERT_NE(server, nullptr);
  Accumulation first;
  Accumulation both;
  RunThenContinue(server, &first, &both);
  Destroy(server);
  if (::testing::Test::HasFatalFailure()) {
    return;
  }

  EXPECT_EQ(first.sim_ray_num, kHalf);
  EXPECT_EQ(both.sim_ray_num, 2 * kHalf) << "the ray count restarted instead of continuing";
  // Emitted energy is charged at the spectrum's expectation per ray, not at the drawn
  // wavelength's weight, so it is (nearly) a pure function of the ray count.
  EXPECT_NEAR(both.emitted_energy / first.emitted_energy, 2.0, 1e-3) << "emitted energy did not continue";
  // Every pixel keeps what it had: the planes were not cleared under the new rays.
  ASSERT_EQ(both.xyz.size(), first.xyz.size());
  size_t lost = 0;
  for (size_t i = 0; i < first.xyz.size(); ++i) {
    if (both.xyz[i] + 1e-6 * std::abs(first.xyz[i]) < first.xyz[i]) {
      ++lost;
    }
  }
  EXPECT_EQ(lost, 0u) << "accumulated plane values went down across the continuation";
  // The plane total is noisier than its 1e5 rays suggest. The CPU route draws ONE wavelength
  // per 128-ray batch, so a half is ~780 colour draws, and X+Y+Z over a uniform draw on
  // [380, 780] nm has a coefficient of variation of 0.92: the ratio's sigma is
  // sqrt(2) * 0.92 / sqrt(780) = 0.046, measured 0.047 over 500 runs (range 1.864-2.170) — and the same 0.047 for
  // two independent fresh halves, so none of it is the continuation's. The band is 6.5 sigma
  // wide; a cleared plane reads about 1 and a double-counted one about 3, both far outside it.
  const double plane_ratio = Sum(both.xyz) / Sum(first.xyz);
  EXPECT_GT(plane_ratio, 1.7) << "the plane total did not roughly double";
  EXPECT_LT(plane_ratio, 2.3) << "the plane total more than doubled";
  // The exposure anchor is a P99 over its own accumulated plane, so it is extensive in the
  // ray count: taken over both halves it is about twice the first half's. A continuation that
  // restarted the anchor plane would read about 1. The band is wide because the P99 of this
  // small scene moves by several percent from seed to seed (measured 1.87-2.09 over six).
  ASSERT_GT(first.anchor_l99_sky, 0.0f);
  const double anchor_ratio = both.anchor_l99_sky / first.anchor_l99_sky;
  EXPECT_GT(anchor_ratio, 1.6) << "the exposure anchor did not take the first half's rays into account";
  EXPECT_LT(anchor_ratio, 2.4);
}

// "N then N" against single runs of 2N, as images. The tolerance is the scene's own noise,
// measured on the spot as the mean distance between three independent 2N runs — one pair is
// not enough: the plane total alone moves by several percent from seed to seed. Fixed seeds
// keep all runs reproducible.
TEST(ContinueRender, ContinuedRunMatchesOneRunOfTheSameTotal) {
  auto one_run = [](unsigned int seed) {
    LUMICE_Server* server = MakeServer(seed, LUMICE_BACKEND_CPU);
    EXPECT_NE(server, nullptr);
    EXPECT_EQ(CommitJson(server, MakeConfig(std::to_string(2 * kHalf))), LUMICE_OK);
    EXPECT_TRUE(WaitForDrain(server));
    Accumulation acc = Read(server);
    Destroy(server);
    return acc;
  };
  const Accumulation refs[3] = { one_run(11), one_run(12), one_run(14) };
  const double noise =
      (RelL1(refs[1].xyz, refs[0].xyz) + RelL1(refs[2].xyz, refs[0].xyz) + RelL1(refs[2].xyz, refs[1].xyz)) / 3.0;

  LUMICE_Server* server = MakeServer(13, LUMICE_BACKEND_CPU);
  ASSERT_NE(server, nullptr);
  Accumulation first;
  Accumulation b;
  RunThenContinue(server, &first, &b);
  Destroy(server);
  if (::testing::Test::HasFatalFailure()) {
    return;
  }

  EXPECT_EQ(b.sim_ray_num, refs[0].sim_ray_num);
  EXPECT_NEAR(b.emitted_energy / refs[0].emitted_energy, 1.0, 1e-3);
  const double dist = (RelL1(b.xyz, refs[0].xyz) + RelL1(b.xyz, refs[1].xyz) + RelL1(b.xyz, refs[2].xyz)) / 3.0;
  EXPECT_LT(dist, 1.5 * noise) << "N+N sits further from the 2N runs than they sit from each other (noise " << noise
                               << ", distance " << dist << ")";
}

// ---- Proposition 2: new samples ------------------------------------------------------------

// A fixed seed is the case a plain re-entry replays: the run's entry re-seeds the stream.
TEST(ContinueRender, ContinuationIsANewStreamUnderAFixedSeedCpu) {
  ExpectNewStream(/*seed=*/7, LUMICE_BACKEND_CPU);
}

TEST(ContinueRender, ContinuationIsANewStreamUnderARandomSeedCpu) {
  ExpectNewStream(/*seed=*/0, LUMICE_BACKEND_CPU);
}

// On the GPU route the backend is created per run and seeds itself from the seed it is
// handed — which is a non-zero constant per worker even when sim_seed is 0 — and restarts its
// device ray counters. So the random-seed case is a replay candidate here too, not just the
// fixed one.
TEST(ContinueRender, ContinuationIsANewStreamUnderAFixedSeedMetal) {
  if (LUMICE_IsBackendAvailable(LUMICE_BACKEND_METAL) == 0) {
    GTEST_SKIP() << "no Metal device";
  }
  ExpectNewStream(/*seed=*/7, LUMICE_BACKEND_METAL);
}

TEST(ContinueRender, ContinuationIsANewStreamUnderARandomSeedMetal) {
  if (LUMICE_IsBackendAvailable(LUMICE_BACKEND_METAL) == 0) {
    GTEST_SKIP() << "no Metal device";
  }
  ExpectNewStream(/*seed=*/0, LUMICE_BACKEND_METAL);
}

// Two continuations must not share a stream either: each gets an index of its own.
TEST(ContinueRender, SecondContinuationIsANewStreamToo) {
  LUMICE_Server* server = MakeServer(/*seed=*/7, LUMICE_BACKEND_CPU);
  ASSERT_NE(server, nullptr);
  Accumulation first;
  Accumulation second;
  RunThenContinue(server, &first, &second);
  ASSERT_EQ(LUMICE_ContinueRender(server, 0, kHalf), LUMICE_OK);
  ASSERT_TRUE(WaitForDrain(server));
  const Accumulation third = Read(server);
  Destroy(server);

  EXPECT_EQ(third.sim_ray_num, 3 * kHalf);
  const auto inc2 = Minus(second.xyz, first.xyz);
  const auto inc3 = Minus(third.xyz, second.xyz);
  EXPECT_GT(RelL1(inc3, inc2), kReplayRelL1) << "the second continuation replayed the first";
}

// ---- Proposition 3: the lifecycle around it ------------------------------------------------

TEST(ContinueRender, AdvancesTheEpochAndReArmsTheDrainSignal) {
  LUMICE_Server* server = MakeServer(0, LUMICE_BACKEND_CPU, /*num_workers=*/1);
  ASSERT_NE(server, nullptr);
  ASSERT_EQ(CommitJson(server, MakeConfig(std::to_string(kHalf))), LUMICE_OK);
  ASSERT_TRUE(WaitForDrain(server));
  const LUMICE_SimLifecycleResult before = ReadLifecycle(server);
  EXPECT_EQ(before.lifecycle, LUMICE_LIFECYCLE_COMPLETED);

  // A budget large enough that the continuation is still running when sampled below.
  ASSERT_EQ(LUMICE_ContinueRender(server, 0, 20 * kHalf), LUMICE_OK);
  const LUMICE_SimLifecycleResult during = ReadLifecycle(server);
  EXPECT_EQ(during.epoch, before.epoch + 1);
  EXPECT_EQ(during.session_kind, LUMICE_SESSION_RENDER);
  LUMICE_DrainResult drain{};
  ASSERT_EQ(LUMICE_GetDrainStatus(server, &drain), LUMICE_OK);
  EXPECT_NE(drain.drained_epoch, drain.current_epoch)
      << "the continuation read as drained from its first instant — a reader waiting for final totals would stop now";
  EXPECT_EQ(during.lifecycle, LUMICE_LIFECYCLE_RUNNING);

  // A second call while this one runs is refused, and changes nothing.
  EXPECT_EQ(LUMICE_ContinueRender(server, 0, kHalf), LUMICE_ERR_SERVER);
  EXPECT_EQ(ReadLifecycle(server).epoch, during.epoch);

  ASSERT_TRUE(WaitForDrain(server));
  EXPECT_EQ(ReadLifecycle(server).lifecycle, LUMICE_LIFECYCLE_COMPLETED);
  EXPECT_EQ(Read(server).sim_ray_num, 21 * kHalf);
  Destroy(server);
}

// The unbounded case: run, stop, continue unbounded, stop again. The count only grows, and a
// Stop straight after a continuation returns (the continuation re-enters every worker's run,
// and the stop has to find all of them).
TEST(ContinueRender, ContinueAfterStopOfAnUnboundedRun) {
  LUMICE_Server* server = MakeServer(0, LUMICE_BACKEND_CPU);
  ASSERT_NE(server, nullptr);
  ASSERT_EQ(CommitJson(server, MakeConfig("\"infinite\"")), LUMICE_OK);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  LUMICE_StopServer(server);
  unsigned long long stopped_at = 0;
  ASSERT_EQ(LUMICE_GetSimRayCount(server, &stopped_at), LUMICE_OK);
  ASSERT_GT(stopped_at, 0u);
  EXPECT_EQ(ReadLifecycle(server).lifecycle, LUMICE_LIFECYCLE_IDLE);

  ASSERT_EQ(LUMICE_ContinueRender(server, /*infinite=*/1, 0), LUMICE_OK);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  unsigned long long now_count = 0;
  while (std::chrono::steady_clock::now() < deadline) {
    ASSERT_EQ(LUMICE_GetSimRayCount(server, &now_count), LUMICE_OK);
    if (now_count > stopped_at) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  EXPECT_GT(now_count, stopped_at) << "the unbounded continuation added no rays";
  EXPECT_EQ(ReadLifecycle(server).lifecycle, LUMICE_LIFECYCLE_RUNNING);

  LUMICE_StopServer(server);
  EXPECT_EQ(ReadLifecycle(server).lifecycle, LUMICE_LIFECYCLE_IDLE);
  // Continue straight into a stop, with nothing in between.
  ASSERT_EQ(LUMICE_ContinueRender(server, 1, 0), LUMICE_OK);
  LUMICE_StopServer(server);
  unsigned long long final_count = 0;
  ASSERT_EQ(LUMICE_GetSimRayCount(server, &final_count), LUMICE_OK);
  EXPECT_GE(final_count, now_count);
  Destroy(server);
}

TEST(ContinueRender, RefusesWhenThereIsNothingToContinue) {
  EXPECT_EQ(LUMICE_ContinueRender(nullptr, 0, kHalf), LUMICE_ERR_NULL_ARG);

  LUMICE_Server* server = MakeServer(0, LUMICE_BACKEND_CPU, /*num_workers=*/1);
  ASSERT_NE(server, nullptr);
  // Never committed.
  EXPECT_EQ(LUMICE_ContinueRender(server, 0, kHalf), LUMICE_ERR_SERVER);
  EXPECT_EQ(ReadLifecycle(server).epoch, 0u);

  ASSERT_EQ(CommitJson(server, MakeConfig(std::to_string(kHalf))), LUMICE_OK);
  ASSERT_TRUE(WaitForDrain(server));
  const unsigned long long epoch = ReadLifecycle(server).epoch;
  // A zero budget is a malformed request, not "nothing to do".
  EXPECT_EQ(LUMICE_ContinueRender(server, 0, 0), LUMICE_ERR_INVALID_VALUE);
  EXPECT_EQ(ReadLifecycle(server).epoch, epoch);
  EXPECT_EQ(Read(server).sim_ray_num, kHalf);

  // An analysis session holds a histogram, not the render's accumulation: refused, and the
  // analysis result is left alone.
  LUMICE_Scene* scene = nullptr;
  ASSERT_EQ(LUMICE_SceneFromJson(MakeConfig(std::to_string(kHalf)).c_str(), &scene), LUMICE_OK);
  LUMICE_RaypathAnalysisRequest req{};
  req.roi_mode = LUMICE_RAYPATH_ROI_FULL_SKY;
  req.infinite = LUMICE_RAYPATH_RAY_BUDGET_SCENE_DEFAULT;
  ASSERT_EQ(LUMICE_StartRaypathAnalysis(server, scene, &req), LUMICE_OK);
  LUMICE_SceneDestroy(scene);
  ASSERT_TRUE(WaitForDrain(server));
  const LUMICE_SimLifecycleResult analysis = ReadLifecycle(server);
  ASSERT_EQ(analysis.session_kind, LUMICE_SESSION_ANALYSIS);
  EXPECT_EQ(LUMICE_ContinueRender(server, 0, kHalf), LUMICE_ERR_SERVER);
  const LUMICE_SimLifecycleResult after = ReadLifecycle(server);
  EXPECT_EQ(after.epoch, analysis.epoch);
  EXPECT_EQ(after.lifecycle, analysis.lifecycle);
  EXPECT_EQ(after.session_kind, LUMICE_SESSION_ANALYSIS);
  Destroy(server);
}
