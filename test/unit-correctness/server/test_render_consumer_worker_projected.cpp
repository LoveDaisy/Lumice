// RenderConsumer::Consume's worker-projected short-circuit (SimData::projected_) against
// the consumer's own projection loop.
//
// On the legacy CPU route the simulator worker projects every outgoing ray of a batch
// itself (BuildWorkerProjectionSidecars, core/worker_projection.cpp) and the consumer
// accumulates straight from the resulting per-renderer ProjectedRayList instead of
// running its own lm_proj::ProjectExitToPixel loop. The two paths share one
// classification function (ProjectAndClassifyRay), so the proposition pinned here is not
// "two hand-written loops agree" but "the sidecar built by the production function, fed
// through the short-circuit branch, reproduces the legacy loop's accumulation bit for
// bit" — XYZ plane, snapshot intensity, and every per-color-class lane.
//
// Coverage:
//   1. WorkerProjectedMatchesLegacyProjection — full-sphere batch with per-ray component
//      masks, one renderer; xyz / intensity / lanes bit-identical.
//   2. DualFisheyeOverlapSplitMatchesLegacy — a dual-fisheye renderer with a non-zero
//      overlap ring: the sidecar carries hits in both rings, the overlap ring accumulates
//      into pixels but NOT into landed_weight_ / snapshot_intensity_, and the result matches
//      the legacy loop bit for bit.
//   3. MultiRendererSidecarIsIndexedByRendererIndex — two renderers of different lens /
//      resolution in one batch; the consumer at renderer_index_ = 1 reads its own entry,
//      not renderer 0's.
//   4. RendererIndexOutOfBoundsFallsBackSafely — a sidecar with fewer entries than this
//      consumer's renderer_index_ must not read past the vector (release builds compile the
//      assert out): it logs once, falls back to the consumer's own loop, and the result is
//      what the legacy loop would have produced.
//   5. EmptyProjectedFallsBackToLegacy — regression lock: an empty sidecar is not a
//      worker-projected batch, and the legacy loop's output is unchanged by the refactor
//      that routed it through ProjectAndClassifyRay.

#include <gtest/gtest.h>
#include <spdlog/sinks/ostream_sink.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "config/color_class_table.hpp"
#include "config/component_table.hpp"
#include "config/proj_config.hpp"
#include "config/render_config.hpp"
#include "config/sim_data.hpp"
#include "core/worker_projection.hpp"
#include "server/render.hpp"
#include "server/server.hpp"
#include "util/logger.hpp"

namespace lumice {
namespace {

constexpr float kWl = 550.0f;

// A deterministic full-sphere batch: Fibonacci-spiral directions, weights that vary with the
// index (so a pixel receiving two rays cannot pass by symmetry), and a two-bit component mask
// pattern so the lane accumulation has something to disagree about.
SimData MakeSphereBatch(size_t n) {
  SimData data;
  data.curr_wl_ = kWl;
  data.root_ray_count_ = n;
  data.emitted_energy_ = static_cast<float>(n);
  data.ray_seg_count_ = n;
  data.outgoing_d_.reserve(n * 3);
  data.outgoing_w_.reserve(n);
  data.outgoing_component_.reserve(n);
  const double golden = 3.14159265358979323846 * (3.0 - std::sqrt(5.0));
  for (size_t i = 0; i < n; ++i) {
    const double z = 1.0 - 2.0 * (static_cast<double>(i) + 0.5) / static_cast<double>(n);
    const double r = std::sqrt(std::max(0.0, 1.0 - z * z));
    const double th = golden * static_cast<double>(i);
    data.outgoing_d_.push_back(static_cast<float>(r * std::cos(th)));
    data.outgoing_d_.push_back(static_cast<float>(r * std::sin(th)));
    data.outgoing_d_.push_back(static_cast<float>(z));
    data.outgoing_w_.push_back(0.25f + 0.5f * static_cast<float>(i % 7) / 7.0f);
    data.outgoing_component_.push_back(static_cast<uint64_t>(1 + (i % 3)));  // bits {0}, {1}, {0,1}
  }
  return data;
}

RenderConfig MakeRenderConfig(LensParam::LensType type, float fov, int w, int h, float overlap = 0.0f) {
  RenderConfig cfg;
  cfg.id_ = 0;
  cfg.lens_.type_ = type;
  cfg.lens_.fov_ = fov;
  cfg.resolution_[0] = w;
  cfg.resolution_[1] = h;
  cfg.view_.az_ = 30.0f;
  cfg.view_.el_ = 45.0f;
  cfg.view_.ro_ = 0.0f;
  cfg.visible_ = RenderConfig::kFull;
  cfg.intensity_factor_ = 1.0f;
  cfg.overlap_ = overlap;
  return cfg;
}

// One single-member `any` class per set bit, the same shape
// test_render_consumer_component_lanes.cpp builds its lanes from.
ColorClassTable MakeSingletonClassTable(uint64_t mask) {
  ColorClassTable t;
  for (uint8_t bit = 0; bit < ComponentTable::kMaxBits; ++bit) {
    if (((mask >> bit) & 1ULL) == 0) {
      continue;
    }
    ColorClass cls;
    cls.combine_ = ColorClassCombine::kAny;
    cls.member_bits_ = static_cast<uint64_t>(1) << bit;
    t.classes_.push_back(cls);
    t.referenced_mask_ |= cls.member_bits_;
  }
  return t;
}

// Everything the consumer publishes that the projection loop feeds: the XYZ plane, the
// intensity scalar, and every lane. Copied out so two consumers can be compared after both
// have been torn down.
struct Accumulated {
  std::vector<float> xyz;
  float intensity = 0.0f;
  std::vector<std::vector<float>> lanes;
};

Accumulated Snapshot(RenderConsumer& rc, size_t n_classes) {
  rc.PrepareSnapshot();
  const RawXyzResult raw = rc.GetRawXyzResult();
  const size_t pix = static_cast<size_t>(raw.img_width_) * static_cast<size_t>(raw.img_height_);
  Accumulated out;
  out.xyz.assign(raw.xyz_buffer_, raw.xyz_buffer_ + pix * 3);
  out.intensity = raw.snapshot_intensity_;
  for (size_t c = 0; c < n_classes; ++c) {
    const float* lane = rc.GetColorClassLaneY(c);
    EXPECT_NE(lane, nullptr) << "lane " << c;
    out.lanes.emplace_back(lane, lane + pix);
  }
  return out;
}

// Bit-for-bit: the short-circuit branch runs the same SpectrumToXyz / AccumulateColorClassLanes
// calls over the same (pixel, w, component) triples in the same order the legacy loop
// compacts them into, so anything short of equality is a real divergence.
void ExpectIdentical(const Accumulated& a, const Accumulated& b) {
  ASSERT_EQ(a.xyz.size(), b.xyz.size());
  size_t diff = 0;
  for (size_t i = 0; i < a.xyz.size(); ++i) {
    diff += (a.xyz[i] != b.xyz[i]) ? 1 : 0;
  }
  EXPECT_EQ(diff, 0u) << "xyz planes differ at " << diff << " of " << a.xyz.size() << " floats";
  EXPECT_EQ(a.intensity, b.intensity);
  ASSERT_EQ(a.lanes.size(), b.lanes.size());
  for (size_t c = 0; c < a.lanes.size(); ++c) {
    EXPECT_EQ(a.lanes[c], b.lanes[c]) << "lane " << c;
  }
}

size_t NonZeroCount(const std::vector<float>& v) {
  size_t n = 0;
  for (float x : v) {
    n += (x != 0.0f) ? 1 : 0;
  }
  return n;
}

// The legacy arm: the batch as the worker publishes it WITHOUT the sidecar, so the consumer
// runs its own loop. The worker arm: the same batch after BuildWorkerProjectionSidecars.
Accumulated LegacyArm(const SimData& batch, const RenderConfig& cfg, const ColorClassTable& classes,
                      size_t renderer_index = 0) {
  RenderConsumer rc(cfg, classes, SunParam{ 0.0f, 0.0f, 0.5f }, renderer_index);
  rc.Consume(batch);
  return Snapshot(rc, classes.classes_.size());
}

Accumulated WorkerArm(const SimData& batch, const std::vector<RenderConfig>& renders, const RenderConfig& cfg,
                      const ColorClassTable& classes, size_t renderer_index = 0) {
  SimData projected = batch;
  BuildWorkerProjectionSidecars(projected, renders);
  EXPECT_EQ(projected.projected_.size(), renders.size());
  RenderConsumer rc(cfg, classes, SunParam{ 0.0f, 0.0f, 0.5f }, renderer_index);
  rc.Consume(projected);
  return Snapshot(rc, classes.classes_.size());
}

class LogCapture {
 public:
  LogCapture() : sink_(std::make_shared<spdlog::sinks::ostream_sink_mt>(oss_)) { GetSharedSink()->add_sink(sink_); }
  ~LogCapture() { GetSharedSink()->remove_sink(sink_); }
  LogCapture(const LogCapture&) = delete;
  LogCapture& operator=(const LogCapture&) = delete;

  std::string Text() const { return oss_.str(); }

 private:
  std::ostringstream oss_;
  std::shared_ptr<spdlog::sinks::ostream_sink_mt> sink_;
};

int CountOccurrences(const std::string& text, const std::string& needle) {
  int n = 0;
  for (size_t pos = text.find(needle); pos != std::string::npos; pos = text.find(needle, pos + needle.size())) {
    ++n;
  }
  return n;
}

// -----------------------------------------------------------------------------
// 1. Single renderer, with lanes.
// -----------------------------------------------------------------------------
TEST(RenderConsumerWorkerProjected, WorkerProjectedMatchesLegacyProjection) {
  const SimData batch = MakeSphereBatch(20000);
  const RenderConfig cfg = MakeRenderConfig(LensParam::kFisheyeEqualArea, 150.0f, 96, 64);
  const ColorClassTable classes = MakeSingletonClassTable(0x3ull);

  const Accumulated legacy = LegacyArm(batch, cfg, classes);
  const Accumulated worker = WorkerArm(batch, { cfg }, cfg, classes);

  // The scene must actually land: an all-black pair would agree for the wrong reason.
  ASSERT_GT(NonZeroCount(legacy.xyz), 100u);
  ASSERT_GT(legacy.intensity, 0.0f);
  ASSERT_GT(NonZeroCount(legacy.lanes[0]), 0u);
  ASSERT_GT(NonZeroCount(legacy.lanes[1]), 0u);
  ExpectIdentical(legacy, worker);
}

// -----------------------------------------------------------------------------
// 2. Dual-fisheye overlap ring.
// -----------------------------------------------------------------------------
TEST(RenderConsumerWorkerProjected, DualFisheyeOverlapSplitMatchesLegacy) {
  const SimData batch = MakeSphereBatch(20000);
  // A wide overlap band (|sky.z| < 0.3) so a good fraction of the sphere lands twice.
  const RenderConfig cfg = MakeRenderConfig(LensParam::kDualFisheyeEqualArea, 180.0f, 128, 64, 0.3f);
  const ColorClassTable classes = MakeSingletonClassTable(0x3ull);

  // White-box on the sidecar itself: both rings populated, and landed_weight_ is the main
  // ring's sum only — the overlap ring is what the legacy loop keeps out of total_intensity_.
  SimData projected = batch;
  BuildWorkerProjectionSidecars(projected, { cfg });
  ASSERT_EQ(projected.projected_.size(), 1u);
  const auto& pr = projected.projected_[0];
  ASSERT_GT(pr.main_pixel_.size(), 0u);
  ASSERT_GT(pr.overlap_pixel_.size(), 0u) << "overlap ring empty — the scene does not exercise the split";
  EXPECT_EQ(pr.main_w_.size(), pr.main_pixel_.size());
  EXPECT_EQ(pr.main_component_.size(), pr.main_pixel_.size());
  EXPECT_EQ(pr.overlap_w_.size(), pr.overlap_pixel_.size());
  EXPECT_EQ(pr.overlap_component_.size(), pr.overlap_pixel_.size());
  float main_sum = 0.0f;
  for (float w : pr.main_w_) {
    main_sum += w;
  }
  EXPECT_FLOAT_EQ(pr.landed_weight_, main_sum);
  float overlap_sum = 0.0f;
  for (float w : pr.overlap_w_) {
    overlap_sum += w;
  }
  ASSERT_GT(overlap_sum, 0.0f);

  const Accumulated legacy = LegacyArm(batch, cfg, classes);
  const Accumulated worker = WorkerArm(batch, { cfg }, cfg, classes);
  ASSERT_GT(legacy.intensity, 0.0f);
  ExpectIdentical(legacy, worker);

  // And the overlap ring stayed out of the intensity: the consumer's scalar is the main
  // ring's sum, normalized the way PrepareSnapshot normalizes it, so it must move with
  // main_sum and not with main_sum + overlap_sum. Pin the ratio against a batch whose rays
  // all land in the main ring (no overlap) carrying the same total main weight.
  RenderConsumer probe(cfg, classes);
  probe.Consume(projected);
  probe.PrepareSnapshot();
  const float with_overlap = probe.GetRawXyzResult().snapshot_intensity_;
  SimData main_only = projected;
  main_only.projected_[0].overlap_pixel_.clear();
  main_only.projected_[0].overlap_w_.clear();
  main_only.projected_[0].overlap_component_.clear();
  RenderConsumer probe2(cfg, classes);
  probe2.Consume(main_only);
  probe2.PrepareSnapshot();
  EXPECT_EQ(probe2.GetRawXyzResult().snapshot_intensity_, with_overlap)
      << "dropping the overlap ring changed the intensity — overlap hits leaked into landed_weight_";
}

// -----------------------------------------------------------------------------
// 3. Two renderers in one batch, consumer at index 1.
// -----------------------------------------------------------------------------
TEST(RenderConsumerWorkerProjected, MultiRendererSidecarIsIndexedByRendererIndex) {
  const SimData batch = MakeSphereBatch(12000);
  const RenderConfig cfg0 = MakeRenderConfig(LensParam::kLinear, 60.0f, 64, 48);
  const RenderConfig cfg1 = MakeRenderConfig(LensParam::kFisheyeEquidistant, 170.0f, 80, 80);
  const ColorClassTable classes;  // no lanes: the point is the index, not the lanes

  const std::vector<RenderConfig> renders = { cfg0, cfg1 };
  const Accumulated legacy1 = LegacyArm(batch, cfg1, classes, /*renderer_index=*/1);
  const Accumulated worker1 = WorkerArm(batch, renders, cfg1, classes, /*renderer_index=*/1);
  ASSERT_GT(NonZeroCount(legacy1.xyz), 100u);
  ExpectIdentical(legacy1, worker1);

  // Negative control: the two renderers really do see different pictures, so reading
  // entry 0 from consumer 1 could not have passed above.
  const Accumulated legacy0 = LegacyArm(batch, cfg0, classes, /*renderer_index=*/0);
  EXPECT_NE(legacy0.intensity, legacy1.intensity);
}

// -----------------------------------------------------------------------------
// 4. Sidecar shorter than renderer_index_: log once, fall back, stay correct.
// -----------------------------------------------------------------------------
TEST(RenderConsumerWorkerProjected, RendererIndexOutOfBoundsFallsBackSafely) {
  constexpr const char* kNotice = "worker-projected batch carries";
  const SimData batch = MakeSphereBatch(8000);
  const RenderConfig cfg = MakeRenderConfig(LensParam::kFisheyeEqualArea, 180.0f, 64, 64);
  const ColorClassTable classes = MakeSingletonClassTable(0x3ull);

  // A batch projected for ONE renderer, consumed by a consumer that believes it is renderer 1.
  SimData projected = batch;
  BuildWorkerProjectionSidecars(projected, { cfg });
  ASSERT_EQ(projected.projected_.size(), 1u);

  const Accumulated legacy = LegacyArm(batch, cfg, classes, /*renderer_index=*/1);
  ASSERT_GT(NonZeroCount(legacy.xyz), 100u);

  LogCapture capture;
  RenderConsumer rc(cfg, classes, SunParam{ 0.0f, 0.0f, 0.5f }, /*renderer_index=*/1);
  rc.Consume(projected);
  rc.Consume(projected);
  EXPECT_EQ(CountOccurrences(capture.Text(), kNotice), 1) << capture.Text();

  // Two consumes of the same batch through the fallback == two consumes through the legacy
  // loop: the fallback IS the legacy loop.
  RenderConsumer twice(cfg, classes, SunParam{ 0.0f, 0.0f, 0.5f }, /*renderer_index=*/1);
  twice.Consume(batch);
  twice.Consume(batch);
  ExpectIdentical(Snapshot(twice, classes.classes_.size()), Snapshot(rc, classes.classes_.size()));
}

// -----------------------------------------------------------------------------
// 5. Regression lock on the legacy loop itself.
// -----------------------------------------------------------------------------
TEST(RenderConsumerWorkerProjected, EmptyProjectedFallsBackToLegacy) {
  const SimData batch = MakeSphereBatch(8000);
  ASSERT_TRUE(batch.projected_.empty());
  const RenderConfig cfg = MakeRenderConfig(LensParam::kDualFisheyeEqualArea, 180.0f, 128, 64, 0.2f);
  const ColorClassTable classes = MakeSingletonClassTable(0x3ull);

  // An empty sidecar is "project it yourself", not "nothing landed": the consumer must
  // produce the picture from outgoing_d_/w_ on its own. Had it taken the short-circuit
  // branch over an empty list the plane would be black — so the non-zero check below is
  // the witness that the legacy loop ran, and the equality is the witness that its refactor
  // onto ProjectAndClassifyRay changed nothing about which pixel / ring a ray lands in.
  RenderConsumer rc(cfg, classes);
  rc.Consume(batch);
  const Accumulated legacy = Snapshot(rc, classes.classes_.size());
  ASSERT_GT(NonZeroCount(legacy.xyz), 100u);
  ASSERT_GT(legacy.intensity, 0.0f);

  const Accumulated worker = WorkerArm(batch, { cfg }, cfg, classes);
  ExpectIdentical(legacy, worker);
}

}  // namespace
}  // namespace lumice
