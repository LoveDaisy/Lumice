// Analytic oracle for the long-chain accumulators behind a rendered frame.
//
// THE DEFECT SHAPE. A per-ray `+=` into a float32 running sum is exact only while the addend
// is large against the sum's ulp. A hot pixel of a long legacy-CPU run collects 1e6–1e7 rays,
// its sum reaches ~1e6 while each addend stays ~0.1, and from there on every add rounds by
// up to half an ulp — in the SAME direction for a constant addend, because round-half-even on
// a fixed bit pattern is not noise, it is a bias. X, Y and Z sit at different magnitudes, so
// they bias by different amounts, the X:Y:Z ratio drifts, and GamutClipXyz turns a ratio drift
// of a few percent into a visible hue flip on a monochromatic sub-sun (beta report: yellow-green
// at 10M rays, cyan at 30M+). The GPU route never showed it because its device-side sums are
// bounded per drain window and the host-side fold is short.
//
// THE ORACLE. `n` identical rays (one wavelength, one weight) into one pixel have a closed-form
// double-precision answer: X = n * kCmfX[wl] * w, same for Y and Z. No simulation, no reference
// image, no second implementation — the reference is arithmetic. Every case below feeds that
// batch shape through a PRODUCTION consume path and asserts three things about the landed pixel:
// X/Y and Z/Y equal the CMF ratios to 1e-5 (hue), and Y equals the closed form to 1e-5
// (magnitude). The bar is placed at 1e-5 because a double running sum lands within one float
// rounding of the reference (~6e-8) and the float one at 1e-2 — either side is decades clear.
// (A float Neumaier pair was measured in between, at 4e-3 on X: its compensation term is
// itself a float running sum, which is why the fix is a wider sum and not a compensated one.)
//
// The batch shape matches production: 128 rays per Consume() call, so the per-batch scalars
// (a batch's own landed_weight, sidecar landed_weight_) stay short fp32 sums exactly as they
// are in a real run — the chain under test is the CROSS-batch one, which is the one that grows
// without bound.
//
// Coverage — one case per accumulation site the ray count can grow without bound:
//   1. LegacyProjectionXyzHoldsCmfRatio       — internal_xyz_ through the consumer's own
//                                               projection loop (SimData::outgoing_d_/w_).
//   2. WorkerProjectionXyzHoldsCmfRatio       — internal_xyz_ through the worker-projected
//                                               sidecar branch (SimData::projected_).
//   3. ColorClassLaneHoldsClosedFormY         — lane_y_ through AccumulateColorClassLanes (both
//                                               CPU projection forms share it; the legacy form
//                                               is driven here).
//   4. DeviceFusedLaneFoldHoldsClosedFormY    — lane_y_ through ConsumeDeviceFused's per-drain
//                                               fold, on a 2x2 plane so 1e6 folds stay cheap.
//   5. SnapshotIntensityHoldsClosedForm       — total_intensity_ (the exposure denominator's
//                                               numerator), same batch as case 1.
//
// NOT covered here, and deliberately: the on-device fp32 atomics inside one Metal/CUDA drain
// window. Those chains are bounded (64 batches per window) and measured at a FIXED ±0.3% on a
// hot pixel that does not move with ray count — bounded is not zero, but it is not this defect.
// AnchorConsumer's plane has its own oracle in test_anchor_consumer.cpp.

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "config/color_class_table.hpp"
#include "config/render_config.hpp"
#include "config/sim_data.hpp"
#include "core/color_util.hpp"
#include "server/render.hpp"
#include "server/server.hpp"

namespace lumice {
namespace {

constexpr float kWl = 550.0f;
constexpr int kCmfIdx = 550 - kCmfMinWavelength;
constexpr float kW = 0.127f;
constexpr size_t kBatchRays = 128;
constexpr size_t kTotalRays = 10'000'000;
constexpr size_t kBatches = kTotalRays / kBatchRays;  // 78125
constexpr double kRelTol = 1e-5;

// Closed-form double references for kTotalRays identical rays.
double RefX() {
  return static_cast<double>(kBatches * kBatchRays) * static_cast<double>(kCmfX[kCmfIdx]) * kW;
}
double RefY() {
  return static_cast<double>(kBatches * kBatchRays) * static_cast<double>(kCmfY[kCmfIdx]) * kW;
}
double RefZ() {
  return static_cast<double>(kBatches * kBatchRays) * static_cast<double>(kCmfZ[kCmfIdx]) * kW;
}

double RelErr(double got, double ref) {
  return std::fabs(got - ref) / std::fabs(ref);
}

// A zenith-pointing linear lens: every ray of the batch travels straight down (sky direction
// -d = +Z), so every ray lands in the same on-axis pixel and nothing lands anywhere else.
RenderConfig MakeZenithConfig(int w, int h) {
  RenderConfig cfg;
  cfg.id_ = 0;
  cfg.lens_.type_ = LensParam::kLinear;
  cfg.lens_.fov_ = 60.0f;
  cfg.resolution_[0] = w;
  cfg.resolution_[1] = h;
  cfg.view_.az_ = 0.0f;
  cfg.view_.el_ = 90.0f;
  cfg.view_.ro_ = 0.0f;
  cfg.visible_ = RenderConfig::kFull;
  cfg.intensity_factor_ = 1.0f;
  cfg.ev_mode_ = RenderConfig::kAbsolute;
  return cfg;
}

// 128 identical downward rays, component bit 0 set on every one.
SimData MakeLegacyBatch() {
  SimData data;
  data.curr_wl_ = kWl;
  data.root_ray_count_ = kBatchRays;
  data.emitted_energy_ = static_cast<float>(kBatchRays);
  data.ray_seg_count_ = kBatchRays;
  data.outgoing_d_.reserve(kBatchRays * 3);
  data.outgoing_w_.assign(kBatchRays, kW);
  data.outgoing_component_.assign(kBatchRays, 1ull);
  for (size_t i = 0; i < kBatchRays; ++i) {
    data.outgoing_d_.push_back(0.0f);
    data.outgoing_d_.push_back(0.0f);
    data.outgoing_d_.push_back(-1.0f);
  }
  return data;
}

// The same batch as the worker would publish it: already projected, one pixel, one weight.
SimData MakeWorkerBatch(int pixel) {
  SimData data;
  data.curr_wl_ = kWl;
  data.root_ray_count_ = kBatchRays;
  data.emitted_energy_ = static_cast<float>(kBatchRays);
  data.ray_seg_count_ = kBatchRays;
  data.projected_.resize(1);
  auto& pr = data.projected_[0];
  pr.main_pixel_.assign(kBatchRays, pixel);
  pr.main_w_.assign(kBatchRays, kW);
  pr.main_component_.assign(kBatchRays, 1ull);
  float landed = 0.0f;
  for (size_t i = 0; i < kBatchRays; ++i) {
    landed += kW;
  }
  pr.landed_weight_ = landed;
  return data;
}

ColorClassTable SingleClassBit0() {
  ColorClassTable t;
  ColorClass cls;
  cls.combine_ = ColorClassCombine::kAny;
  cls.member_bits_ = 1ull;
  t.classes_.push_back(cls);
  t.referenced_mask_ = 1ull;
  return t;
}

// The one pixel the batch landed in. Exactly one non-zero pixel is part of the proposition:
// were the rays spread over several, the per-pixel chain would be shorter than kTotalRays and
// the case would be testing something milder than it claims.
struct LandedPixel {
  int index = -1;
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

LandedPixel FindSingleLandedPixel(const float* xyz, int w, int h) {
  LandedPixel out;
  int non_zero = 0;
  for (int p = 0; p < w * h; ++p) {
    if (xyz[p * 3 + 0] != 0.0f || xyz[p * 3 + 1] != 0.0f || xyz[p * 3 + 2] != 0.0f) {
      ++non_zero;
      out.index = p;
      out.x = xyz[p * 3 + 0];
      out.y = xyz[p * 3 + 1];
      out.z = xyz[p * 3 + 2];
    }
  }
  EXPECT_EQ(non_zero, 1) << "the batch must land in exactly one pixel for the chain length to be kTotalRays";
  return out;
}

void ExpectCmfRatiosAndMagnitude(const LandedPixel& px, const char* what) {
  const double ratio_xy_ref = static_cast<double>(kCmfX[kCmfIdx]) / static_cast<double>(kCmfY[kCmfIdx]);
  const double ratio_zy_ref = static_cast<double>(kCmfZ[kCmfIdx]) / static_cast<double>(kCmfY[kCmfIdx]);
  const double ratio_xy = px.x / px.y;
  const double ratio_zy = px.z / px.y;
  EXPECT_LE(RelErr(ratio_xy, ratio_xy_ref), kRelTol)
      << what << ": X/Y=" << ratio_xy << " ref=" << ratio_xy_ref << " rel_err=" << RelErr(ratio_xy, ratio_xy_ref);
  EXPECT_LE(RelErr(ratio_zy, ratio_zy_ref), kRelTol)
      << what << ": Z/Y=" << ratio_zy << " ref=" << ratio_zy_ref << " rel_err=" << RelErr(ratio_zy, ratio_zy_ref);
  EXPECT_LE(RelErr(px.y, RefY()), kRelTol)
      << what << ": Y=" << px.y << " ref=" << RefY() << " rel_err=" << RelErr(px.y, RefY());
  // X and Z magnitudes are implied by the two ratios and Y, but print them for the record.
  EXPECT_LE(RelErr(px.x, RefX()), kRelTol) << what << ": X rel_err=" << RelErr(px.x, RefX());
  EXPECT_LE(RelErr(px.z, RefZ()), kRelTol) << what << ": Z rel_err=" << RelErr(px.z, RefZ());
}

// -----------------------------------------------------------------------------
// 1. Legacy projection loop.
// -----------------------------------------------------------------------------
TEST(RenderConsumerFp32AccumOracle, LegacyProjectionXyzHoldsCmfRatio) {
  const RenderConfig cfg = MakeZenithConfig(16, 16);
  const SimData batch = MakeLegacyBatch();
  RenderConsumer rc(cfg);
  for (size_t b = 0; b < kBatches; ++b) {
    rc.Consume(batch);
  }
  rc.PrepareSnapshot();
  const RawXyzResult raw = rc.GetRawXyzResult();
  const LandedPixel px = FindSingleLandedPixel(raw.xyz_buffer_, raw.img_width_, raw.img_height_);
  ASSERT_GE(px.index, 0);
  ExpectCmfRatiosAndMagnitude(px, "legacy internal_xyz_");
}

// -----------------------------------------------------------------------------
// 2. Worker-projected sidecar branch.
// -----------------------------------------------------------------------------
TEST(RenderConsumerFp32AccumOracle, WorkerProjectionXyzHoldsCmfRatio) {
  const RenderConfig cfg = MakeZenithConfig(16, 16);
  // Land in the same pixel the legacy loop picks, read off a one-batch legacy probe so the
  // two cases are talking about the same pixel rather than one chosen by hand.
  int pixel = -1;
  {
    RenderConsumer probe(cfg);
    probe.Consume(MakeLegacyBatch());
    probe.PrepareSnapshot();
    const RawXyzResult raw = probe.GetRawXyzResult();
    pixel = FindSingleLandedPixel(raw.xyz_buffer_, raw.img_width_, raw.img_height_).index;
  }
  ASSERT_GE(pixel, 0);
  const SimData batch = MakeWorkerBatch(pixel);
  RenderConsumer rc(cfg);
  for (size_t b = 0; b < kBatches; ++b) {
    rc.Consume(batch);
  }
  rc.PrepareSnapshot();
  const RawXyzResult raw = rc.GetRawXyzResult();
  const LandedPixel px = FindSingleLandedPixel(raw.xyz_buffer_, raw.img_width_, raw.img_height_);
  ASSERT_EQ(px.index, pixel);
  ExpectCmfRatiosAndMagnitude(px, "worker-projected internal_xyz_");
}

// -----------------------------------------------------------------------------
// 3. Per-color-class Y lane (shared by both CPU projection forms).
// -----------------------------------------------------------------------------
TEST(RenderConsumerFp32AccumOracle, ColorClassLaneHoldsClosedFormY) {
  const RenderConfig cfg = MakeZenithConfig(16, 16);
  const SimData batch = MakeLegacyBatch();
  RenderConsumer rc(cfg, SingleClassBit0());
  for (size_t b = 0; b < kBatches; ++b) {
    rc.Consume(batch);
  }
  rc.PrepareSnapshot();
  const RawXyzResult raw = rc.GetRawXyzResult();
  const LandedPixel px = FindSingleLandedPixel(raw.xyz_buffer_, raw.img_width_, raw.img_height_);
  ASSERT_GE(px.index, 0);
  const float* lane = rc.GetColorClassLaneY(0);
  ASSERT_NE(lane, nullptr);
  const double lane_y = lane[px.index];
  EXPECT_LE(RelErr(lane_y, RefY()), kRelTol)
      << "lane_y_: Y=" << lane_y << " ref=" << RefY() << " rel_err=" << RelErr(lane_y, RefY());
}

// -----------------------------------------------------------------------------
// 4. Device-fused lane fold.
// -----------------------------------------------------------------------------
TEST(RenderConsumerFp32AccumOracle, DeviceFusedLaneFoldHoldsClosedFormY) {
  // A 2x2 plane keeps each fold to a dozen floats so a million of them run in well under a
  // second; the chain length is the fold count, not the ray count, and 1e6 folds of a
  // 128-ray batch's worth of Y is where a float32 sum's ulp overtakes the addend.
  constexpr int kW2 = 2;
  constexpr int kH2 = 2;
  constexpr size_t kFolds = 1'000'000;
  constexpr int kPixel = 3;
  const RenderConfig cfg = MakeZenithConfig(kW2, kH2);
  const float y_per_fold = SpectrumToYSingle(kWl, kW) * static_cast<float>(kBatchRays);
  const double ref = static_cast<double>(kFolds) * static_cast<double>(y_per_fold);

  SimData batch;
  batch.curr_wl_ = kWl;
  batch.root_ray_count_ = kBatchRays;
  batch.emitted_energy_ = static_cast<float>(kBatchRays);
  batch.xyz_pixel_data_.assign(1, std::vector<float>(static_cast<size_t>(kW2 * kH2) * 3u, 0.0f));
  batch.xyz_pixel_data_[0][static_cast<size_t>(kPixel) * 3u + 1u] = y_per_fold;
  batch.xyz_landed_weight_.assign(1, kW * static_cast<float>(kBatchRays));
  batch.lane_class_count_ = 1;
  batch.lane_pixel_data_.assign(1, std::vector<float>(static_cast<size_t>(kW2 * kH2), 0.0f));
  batch.lane_pixel_data_[0][static_cast<size_t>(kPixel)] = y_per_fold;

  RenderConsumer rc(cfg, SingleClassBit0());
  for (size_t f = 0; f < kFolds; ++f) {
    rc.Consume(batch);
  }
  rc.PrepareSnapshot();
  const float* lane = rc.GetColorClassLaneY(0);
  ASSERT_NE(lane, nullptr);
  const double lane_y = lane[kPixel];
  EXPECT_LE(RelErr(lane_y, ref), kRelTol)
      << "device-fused lane_y_: Y=" << lane_y << " ref=" << ref << " rel_err=" << RelErr(lane_y, ref);
  // The XYZ plane beside it takes the same fold on the same numbers, so it must hold too — if
  // only this line is red, the fold count is what changed, not the lane accumulator.
  const RawXyzResult raw = rc.GetRawXyzResult();
  const double plane_y = raw.xyz_buffer_[static_cast<size_t>(kPixel) * 3u + 1u];
  EXPECT_LE(RelErr(plane_y, ref), kRelTol) << "control: device-fused internal_xyz_ rel_err=" << RelErr(plane_y, ref);
}

// -----------------------------------------------------------------------------
// 5. total_intensity_ (published as snapshot_intensity_ / (kNormScale * W*H)).
// -----------------------------------------------------------------------------
TEST(RenderConsumerFp32AccumOracle, SnapshotIntensityHoldsClosedForm) {
  constexpr int kW16 = 16;
  constexpr int kH16 = 16;
  const RenderConfig cfg = MakeZenithConfig(kW16, kH16);
  const SimData batch = MakeLegacyBatch();
  RenderConsumer rc(cfg);
  for (size_t b = 0; b < kBatches; ++b) {
    rc.Consume(batch);
  }
  rc.PrepareSnapshot();
  const RawXyzResult raw = rc.GetRawXyzResult();
  // Per-batch landed weight is the batch's own short fp32 sum of 128 weights — take it as the
  // consumer computes it, so the reference isolates the cross-batch chain.
  float per_batch = 0.0f;
  for (size_t i = 0; i < kBatchRays; ++i) {
    per_batch += kW;
  }
  const double ref_total = static_cast<double>(kBatches) * static_cast<double>(per_batch);
  const double ref_per_pixel = ref_total / (static_cast<double>(kNormScale) * static_cast<double>(kW16 * kH16));
  const double got = raw.snapshot_intensity_;
  EXPECT_LE(RelErr(got, ref_per_pixel), kRelTol)
      << "snapshot_intensity_: got=" << got << " ref=" << ref_per_pixel << " rel_err=" << RelErr(got, ref_per_pixel);
}

}  // namespace
}  // namespace lumice
