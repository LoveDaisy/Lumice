#include <gtest/gtest.h>

#include <cstring>
#include <fstream>
#include <memory>
#include <thread>

#include "config/config_manager.hpp"
#include "core/shared/projection_shared.h"
#include "core/simulator.hpp"
#include "util/queue.hpp"

extern std::string config_file_name;
using namespace std::chrono_literals;

namespace {

class V3TestProj : public ::testing::Test {
 protected:
  void SetUp() override {
    std::ifstream f(config_file_name);
    f >> config_json_;
  }

  nlohmann::json config_json_;
};


// For test: capture the batch's raw ray segments.
//
// This used to be an IConsume registered on the consumer thread, reading
// SimData::rays_. That buffer no longer travels on SimData — the legacy CPU
// path recycles it inside Simulator::SimWorkspace, so the only remaining seat
// for a content read is the simulator's own test observer, which fires
// synchronously on the producer thread just before the batch is queued. Same
// batch, same rays, same selection rule; only the seat moved.
void CopyRayData(void* ctx, const lumice::RayBuffer& all_data) {
  auto* output_data = static_cast<float*>(ctx);
  int offset = 0;
  for (const auto& r : all_data) {
    if (r.to_face_ != lumice::kInvalidId || r.w_ < 0) {
      continue;
    }
    std::memcpy(output_data + offset * 7 + 0, r.p_, 3 * sizeof(float));
    std::memcpy(output_data + offset * 7 + 3, r.d_, 3 * sizeof(float));
    output_data[offset * 7 + 6] = r.w_;
    offset++;
  }
}

// Queue drain. It registers no IConsume any more — the batch's ray content is
// captured by the simulator-side observer above — but the produced SimData
// still has to be taken off the queue and the loop's exit condition still
// exercises the "no ray segments" sentinel the server reads.
class Consumer {
 public:
  Consumer(lumice::QueuePtrS<lumice::SimData> data_queue) : data_queue_(data_queue), stop_(false) {}

  void Run() {
    while (true) {
      auto data = data_queue_->Get();
      if (stop_ || data.ray_seg_count_ == 0) {
        break;
      }
    }
  }

  void Stop() { stop_ = true; }

 private:
  lumice::QueuePtrS<lumice::SimData> data_queue_;
  std::atomic_bool stop_;
};

TEST_F(V3TestProj, SimpleProj) {
  lumice::ConfigManager config_manager = config_json_.get<lumice::ConfigManager>();

  auto config_queue = std::make_shared<lumice::Queue<lumice::SimBatch>>();
  auto data_queue = std::make_shared<lumice::Queue<lumice::SimData>>();

  constexpr int kMaxHits = 8;
  auto output_data = std::make_unique<float[]>(kMaxHits * 2 * 7);

  constexpr uint32_t kTestSeed = 42;
  lumice::Simulator simulator(config_queue, data_queue, kTestSeed);

  simulator.SetAllDataObserverForTest(&CopyRayData, output_data.get());

  // Still drained on its own thread: the batch must actually be produced and
  // published for the observer above to have fired on a complete batch.
  Consumer consumer(data_queue);

  std::thread prod_thread([&simulator]() { simulator.Run(); });
  std::thread cons_thread([&consumer]() { consumer.Run(); });

  const auto& config = config_manager.scene_;
  auto scene_ptr = std::make_shared<const lumice::SceneConfig>(config);
  config_queue->Emplace(lumice::SimBatch{ config.ray_num_, scene_ptr, 0 });

  std::this_thread::sleep_for(500ms);
  simulator.Stop();
  consumer.Stop();

  cons_thread.join();
  prod_thread.join();

  // Fixture regenerated after Crystal::CreatePrism switched from the numerical
  // triangle-mesh pipeline (FillHexCrystalCoef + CreateConvexPolyhedronMesh +
  // math.cpp::Triangulate) to the closed-form path (ComputeClosedFormPrism +
  // fixed-fan triangulation in crystal.cpp::AdaptClosedFormPrismToCrystalGeom
  // / BuildMeshFromCfGeom). Captured with seed=42 from
  // examples/config_example.json crystal id=1 (default axis = canonical pose).
  //
  // Why the y/z values drift while d/w/p[0] stay bit-identical: sample_triangle
  // in simulator.cpp draws area-weighted from the crystal's triangle list. The
  // closed-form path produces a different specific triangulation of the same
  // per-face polygon, so categorical_sample under the same uniform sequence
  // lands on a different triangle / point on the same face. Physics-invariant
  // fields — w (Fresnel weight along the path history), d (exit direction from
  // the same face with the same incident direction), and p[0] (±0.433013 =
  // ±√3/4, the x of the ±x prism side face) — reproduce bit-for-bit vs the
  // pre-swap capture; only p[1] / p[2] (position within the same face) shifted.
  // That is the exact signature of "same distribution, different draw" — the
  // ray-count-1 + distribution-unchanged invariant Crystal::CreatePrism now
  // guarantees is the one this suite actually depends on.
  //
  // The w column (and only it) was rescaled when InitRay_p_fid began multiplying
  // each entry ray's weight by its projected-area entry weight A/(S/2): p and d
  // reproduce bit-for-bit at this seed, and each ray's w history scales by one
  // constant per ray (0.5513 and 0.5510 for the two rays here, their A/(S/2)).
  float expect_out[kMaxHits * 2 * 7]{
    /* --------- p --------------->|<-------------- d ------------->|<-- w -->|*/
    0.433013f,  -0.139846f, 0.145981f,  0.938556f,  -0.000995f, -0.345125f, 0.010082f,  //
    0.433013f,  -0.171991f, 0.122222f,  0.939997f,  -0.000397f, -0.341181f, 0.010071f,  //
    -0.433013f, -0.140527f, -0.090337f, -0.938556f, -0.000995f, -0.345125f, 0.531326f,  //
    -0.433013f, -0.172262f, -0.111198f, -0.939997f, -0.000397f, -0.341181f, 0.531008f,  //
    0.433013f,  -0.141209f, -0.326654f, 0.938556f,  -0.000996f, -0.345125f, 0.009716f,  //
    0.433013f,  -0.172534f, -0.344618f, 0.939997f,  -0.000397f, -0.341181f, 0.009706f,  //
    -0.433013f, -0.141891f, -0.562971f, -0.938556f, -0.000996f, -0.345125f, 0.000178f,  //
    -0.433013f, -0.172805f, -0.578037f, -0.939997f, -0.000397f, -0.341181f, 0.000177f,  //
    0.433013f,  -0.142573f, -0.400712f, 0.938556f,  -0.000996f, 0.345125f,  0.000003f,  //
    0.433013f,  -0.173077f, -0.388543f, 0.939997f,  -0.000397f, 0.341181f,  0.000003f,  //
    -0.433013f, -0.143254f, -0.164395f, -0.938556f, -0.000996f, 0.345125f,  0.000000f,  //
    -0.433013f, -0.173349f, -0.155124f, -0.939997f, -0.000397f, 0.341181f,  0.000000f,  //
    0.000000f,  0.000000f,  0.000000f,  0.000000f,  0.000000f,  0.000000f,  0.000000f,  //
    0.000000f,  0.000000f,  0.000000f,  0.000000f,  0.000000f,  0.000000f,  0.000000f,  //
    0.000000f,  0.000000f,  0.000000f,  0.000000f,  0.000000f,  0.000000f,  0.000000f,  //
    0.000000f,  0.000000f,  0.000000f,  0.000000f,  0.000000f,  0.000000f,  0.000000f,  //
  };

  for (int i = 0; i < kMaxHits * 2; i++) {
    for (int j = 0; j < 7; j++) {
      EXPECT_NEAR(expect_out[i * 7 + j], output_data[i * 7 + j], 5e-5);
    }
  }
}

// ---------------------------------------------------------------------------
// ProjectionCastBounds — the float→int conversion-domain probe for the single-lens
// bump path (lm_proj::ProjectExitToPixel's kProjLinear branch, the
// static_cast<int>(LM_FLOOR(...)) pixel casts).
//
// The forward formula is allowed to return pixel expressions far outside any canvas
// (kProjLinear's dx/dz is unbounded for near-horizontal rays) and the CALLER owns the
// pixel bounds check, so the only in-function defence is keeping the cast input inside
// the float→int conversion domain, where the C++ cast is defined. Three leg families:
//   1. Conversion-domain-out legs (previously UB — float-cast-overflow): the cast must
//      stay defined — count==1, the pixel lands OFF-canvas (the caller's check rejects
//      it; no canvas clamp), and two calls agree (same site + same value = fixed
//      parameters, deterministic).
//   2. Defined-domain preserved legs: the clamp's identity region must cover every
//      previously-defined conversion value — NOT just |v| <= 2^24. Every float32 with
//      |v| >= 2^24 is an integer (ulp >= 2, floor exact), so casts in (2^24, 2^31) were
//      already defined and must stay bit-identical. The expected pixels below exceed
//      2^24 in magnitude and are pinned to the derivation (wx/|wz| = ±2^19, scale=64,
//      img_w/2 = img_h/2 = 32, shift = 0).
//   3. Negative control: the on-axis ray keeps its in-canvas pixel.
// ---------------------------------------------------------------------------
namespace {

lm_proj::ProjParams MakeLinearProjParams() {
  lm_proj::ProjParams p{};
  p.proj_type = lm_proj::kProjLinear;
  p.img_w = 64;
  p.img_h = 64;
  p.lens_shift_x = 0;
  p.lens_shift_y = 0;
  p.scale = 64.0f;
  p.r_scale = 1.0f;
  p.max_abs_dz = 0.0f;
  p.globe_back_fade = 0.0f;
  p.rot[0] = 1.0f;
  p.rot[1] = 0.0f;
  p.rot[2] = 0.0f;
  p.rot[3] = 0.0f;
  p.rot[4] = 1.0f;
  p.rot[5] = 0.0f;
  p.rot[6] = 0.0f;
  p.rot[7] = 0.0f;
  p.rot[8] = 1.0f;
  return p;
}

}  // namespace

TEST(ProjectionCastBounds, OutOfIntRangeLinearHitStaysDefinedAndOffCanvas) {
  const lm_proj::ProjParams p = MakeLinearProjParams();
  constexpr int kCanvasW = 64;
  constexpr int kCanvasH = 64;
  constexpr float kTinyWz = -(1.0f / 524288.0f);  // -2^-19, exact in float32
  auto is_off_canvas = [](int v, int bound) { return v < 0 || v >= bound; };

  // --- Family 1: conversion-domain-out inputs (previously UB) ---
  // scale=64 turns wx/|wz| = ±1e9 into a pixel expression of ±~6.4e10, past
  // INT_MAX / INT_MIN. Only x is handedness-flipped (xy.x = -xy.x), so wy = +1
  // yields a NEGATIVE py expression — still far past INT_MIN.
  {
    const auto a = lm_proj::ProjectExitToPixel(p, 1.0f, 0.0f, -1e-9f);
    const auto b = lm_proj::ProjectExitToPixel(p, 1.0f, 0.0f, -1e-9f);
    EXPECT_EQ(1, a.count);
    EXPECT_EQ(a.hits[0].px, b.hits[0].px);
    EXPECT_EQ(a.hits[0].py, b.hits[0].py);
    EXPECT_TRUE(is_off_canvas(a.hits[0].px, kCanvasW)) << "px = " << a.hits[0].px;
  }
  {
    const auto a = lm_proj::ProjectExitToPixel(p, -1.0f, 0.0f, -1e-9f);
    const auto b = lm_proj::ProjectExitToPixel(p, -1.0f, 0.0f, -1e-9f);
    EXPECT_EQ(1, a.count);
    EXPECT_EQ(a.hits[0].px, b.hits[0].px);
    EXPECT_EQ(a.hits[0].py, b.hits[0].py);
    EXPECT_TRUE(is_off_canvas(a.hits[0].px, kCanvasW)) << "px = " << a.hits[0].px;
  }
  {
    const auto a = lm_proj::ProjectExitToPixel(p, 0.0f, 1.0f, -1e-9f);
    const auto b = lm_proj::ProjectExitToPixel(p, 0.0f, 1.0f, -1e-9f);
    EXPECT_EQ(1, a.count);
    EXPECT_EQ(a.hits[0].px, b.hits[0].px);
    EXPECT_EQ(a.hits[0].py, b.hits[0].py);
    EXPECT_TRUE(is_off_canvas(a.hits[0].py, kCanvasH)) << "py = " << a.hits[0].py;
  }

  // --- Family 2: defined-domain preserved legs (bit-identical before/after the clamp) ---
  {
    // px = +2^19 * 64 + 32 = 33554464 (> 2^24, in int range; exactly representable).
    const auto hit = lm_proj::ProjectExitToPixel(p, 1.0f, 0.0f, kTinyWz);
    EXPECT_EQ(1, hit.count);
    EXPECT_EQ(33554464, hit.hits[0].px);
  }
  {
    // px = -2^19 * 64 + 32 = -33554400 (negative side of the identity region).
    const auto hit = lm_proj::ProjectExitToPixel(p, -1.0f, 0.0f, kTinyWz);
    EXPECT_EQ(1, hit.count);
    EXPECT_EQ(-33554400, hit.hits[0].px);
  }
  {
    // y axis is not handedness-flipped, so wy = -1 maps to +2^19 on the y expression:
    // py = +2^19 * 64 + 32 = 33554464.
    const auto hit = lm_proj::ProjectExitToPixel(p, 0.0f, -1.0f, kTinyWz);
    EXPECT_EQ(1, hit.count);
    EXPECT_EQ(33554464, hit.hits[0].py);
  }

  // --- Family 3: negative control — the on-axis ray stays in-canvas ---
  {
    const auto hit = lm_proj::ProjectExitToPixel(p, 0.0f, 0.0f, -0.5f);
    EXPECT_EQ(1, hit.count);
    EXPECT_EQ(32, hit.hits[0].px);
    EXPECT_EQ(32, hit.hits[0].py);
  }
}

}  // namespace