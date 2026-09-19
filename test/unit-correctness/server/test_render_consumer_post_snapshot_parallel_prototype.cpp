// EXPLORE-571.14 prototype-only correctness probe. NOT meant to land as-is — if the follow-up
// task lands the ParallelRows change for real, this fixture's job (prove the parallel dispatch is
// byte-exact against an independent oracle AT a resolution that actually exercises multiple
// threads) should be folded into test_render_consumer_post_snapshot_fusion.cpp instead of kept as
// a second file. Deleted or promoted at conclude time depending on the explore's §0 verdict.
//
// Why a separate fixture at all: every existing PostSnapshot fixture (the fusion suite's 16x16,
// the markers suite's 128x64) sits BELOW core/parallel_rows.cpp's kParallelPixelThreshold
// (256x256 = 65536 px), so ParallelRows takes its serial inline fallback for all of them — none
// of the green ticks from the existing suites say anything about the code path this prototype
// actually adds. This fixture's 300x300 = 90000 px is chosen to clear that threshold, and it
// turns on markers_ (the one piece of PostSnapshot state — AnnotationLayers::on_marker_ring —
// that is mutated per pixel rather than only read), which is exactly the state a naive
// shared-`layers`-across-threads parallelization would race on.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <random>
#include <vector>

#include "config/color_class_table.hpp"
#include "config/light_config.hpp"
#include "config/render_config.hpp"
#include "config/sim_data.hpp"
#include "core/annotation_overlay.hpp"
#include "server/render.hpp"
#include "support/render_anchor.hpp"
#include "util/color_data.hpp"
#include "util/color_space.hpp"

namespace lumice {
namespace {

constexpr int kW = 300;
constexpr int kH = 300;
constexpr int kTotalPix = kW * kH;
static_assert(static_cast<size_t>(kTotalPix) > 65536, "must clear ParallelRows' pixel threshold");

constexpr float kMarkerHalfWidthPx = 1.5f;  // mirrors render.cpp's kMarkerHalfWidthPx

SunParam MakeSun() {
  return SunParam{ 35.0f, 20.0f, 0.5f };
}

RenderConfig MakeConfig() {
  RenderConfig cfg;
  cfg.id_ = 0;
  cfg.lens_.type_ = LensParam::kDualFisheyeEqualArea;
  cfg.lens_.fov_ = 180.0f;
  cfg.resolution_[0] = kW;
  cfg.resolution_[1] = kH;
  cfg.visible_ = RenderConfig::kFull;
  cfg.markers_radius_px_ = 9.0f;
  cfg.markers_opacity_ = 1.0f;
  cfg.markers_.push_back({ MarkerRefId::kZenith, true, { 1.0f, 0.0f, 0.0f } });
  cfg.markers_.push_back({ MarkerRefId::kNadir, true, { 0.0f, 1.0f, 0.0f } });
  cfg.markers_.push_back({ MarkerRefId::kSun, true, { 0.0f, 0.0f, 1.0f } });
  cfg.markers_.push_back({ MarkerRefId::kSubsun, true, { 1.0f, 1.0f, 0.0f } });
  cfg.markers_.push_back({ MarkerRefId::kAnthelion, true, { 0.0f, 1.0f, 1.0f } });
  cfg.markers_.push_back({ MarkerRefId::kAntisolar, true, { 1.0f, 0.0f, 1.0f } });
  return cfg;
}

// A scattered batch (not one hot pixel): representative of a real preview frame, and it exercises
// the background/annotation blend on many pixels rather than 255 blacks and one lit dot.
SimData MakeScatteredBatch() {
  SimData data;
  data.curr_wl_ = 550.0f;
  std::mt19937 rng(20260919);
  std::uniform_real_distribution<float> ang(0.0f, 1.0f);
  constexpr int kRays = 20000;
  data.outgoing_d_.reserve(static_cast<size_t>(kRays) * 3);
  data.outgoing_w_.reserve(kRays);
  for (int i = 0; i < kRays; ++i) {
    const float u = ang(rng);
    const float v = ang(rng);
    const float theta = std::acos(1.0f - 2.0f * u);  // uniform over the sphere
    const float phi = 2.0f * 3.14159265358979323846f * v;
    data.outgoing_d_.push_back(std::sin(theta) * std::cos(phi));
    data.outgoing_d_.push_back(std::sin(theta) * std::sin(phi));
    data.outgoing_d_.push_back(std::cos(theta));
    data.outgoing_w_.push_back(0.3f + 0.7f * ang(rng));
  }
  data.emitted_energy_ = std::accumulate(data.outgoing_w_.begin(), data.outgoing_w_.end(), 0.0f);
  return data;
}

// Independent re-derivation of the marker-ring contribution only (the colour/background chain is
// exactly what test_render_consumer_post_snapshot_fusion.cpp already re-derives independently;
// this fixture's whole point is the ring, so it borrows that file's oracle style rather than its
// code). `pts` and `colors` are parallel arrays over the configured markers, in config order —
// MarkerPointsForTest() exposes the SAME points production's CompositeAnnotations reads, matching
// this file's "no second call into the code under test" discipline in every step except one: the
// point itself is geometry this fixture does not re-derive, same as the existing markers suite's
// use of the same accessor.
void ApplyMarkerRings(const std::vector<annotation::CanvasPoint>& pts, const std::vector<std::array<float, 3>>& colors,
                      float radius_px, float alpha, int i, int width_px, float rgb[3]) {
  const auto px = static_cast<float>(i % width_px);
  const auto py = static_cast<float>(i / width_px);
  for (size_t k = 0; k < pts.size(); ++k) {
    if (!pts[k].valid) {
      continue;
    }
    const float d = std::hypot(px - pts[k].px, py - pts[k].py);
    if (std::fabs(d - radius_px) >= kMarkerHalfWidthPx) {
      continue;
    }
    for (int j = 0; j < 3; ++j) {
      rgb[j] = rgb[j] * (1.0f - alpha) + colors[k][j] * alpha;
    }
  }
}

void ScaledXyzToLinearRgb(const RenderConfig& cfg, const float* xyz_raw, int i, float scale, float rgb[3]) {
  float xyz[3];
  for (int j = 0; j < 3; j++) {
    xyz[j] = xyz_raw[i * 3 + j] * scale;
  }
  if (cfg.ray_color_[0] < 0) {
    float clipped[3];
    GamutClipXyz(xyz, clipped);
    XyzToLinearRgb(clipped, rgb);
  } else {
    float gray[3];
    for (int j = 0; j < 3; j++) {
      gray[j] = kWhitePointD65[j] * xyz[1];
    }
    for (int j = 0; j < 3; j++) {
      float v = 0;
      for (int k = 0; k < 3; k++) {
        v += gray[k] * kXyzToRgb[j * 3 + k];
      }
      rgb[j] = v * cfg.ray_color_[j];
    }
  }
}

TEST(RenderConsumerPostSnapshotParallelPrototype, LargeResolutionWithMarkersIsByteExactAgainstOracle) {
  RenderConfig cfg = MakeConfig();
  RenderConsumer rc(cfg, ColorClassTable{}, MakeSun());
  auto data = MakeScatteredBatch();
  rc.Consume(data);
  lumice::test::TakeSnapshotAtFormerSelfAnchor(&rc);

  const auto& all_pts = rc.MarkerPointsForTest();
  std::vector<annotation::CanvasPoint> pts;
  std::vector<std::array<float, 3>> colors;
  size_t imaged = 0;
  for (const auto& m : cfg.markers_) {
    const auto& p = all_pts[static_cast<size_t>(m.id_)];
    pts.push_back(p);
    colors.push_back({ m.color_[0], m.color_[1], m.color_[2] });
    if (p.valid) {
      imaged++;
    }
  }
  ASSERT_EQ(imaged, pts.size()) << "the fixture must image every configured marker, or this proves nothing about the "
                                   "ring scratch buffer";

  const float scale = rc.ExposureScale();
  ASSERT_GT(scale, 0.0f);
  const auto raw = rc.GetRawXyzResult();

  auto result = rc.GetResult();
  const auto* rr = std::get_if<RenderResult>(&result);
  ASSERT_NE(rr, nullptr);

  // paint_bg is read from the consumer's own visible_mask_ rather than re-derived from dual-fisheye
  // projection geometry: it is built once at construction (VisibleMask()'s declaration), entirely
  // independent of PostSnapshot() and untouched by this prototype's change, so borrowing it does
  // not turn this into a second call into the code actually under test — the loop's arithmetic and
  // the marker-ring compositing are still re-derived independently below, which is the part this
  // fixture exists to check.
  const auto& visible_mask = rc.VisibleMask();
  ASSERT_EQ(visible_mask.size(), static_cast<size_t>(kTotalPix));

  constexpr size_t kMaxReportedMismatches = 20;
  size_t mismatches = 0;
  size_t nonzero = 0;
  size_t ring_hits = 0;
  for (int i = 0; i < kTotalPix; ++i) {
    const bool paint_bg = visible_mask[static_cast<size_t>(i)] != 0;
    float rgb[3];
    if (paint_bg) {
      ScaledXyzToLinearRgb(cfg, raw.xyz_buffer_, i, scale, rgb);
      for (int j = 0; j < 3; ++j) {
        rgb[j] += cfg.background_[j];
      }
    } else {
      rgb[0] = rgb[1] = rgb[2] = 0.0f;
    }
    ApplyMarkerRings(pts, colors, cfg.markers_radius_px_, cfg.markers_opacity_, i, kW, rgb);
    {
      const auto px = static_cast<float>(i % kW);
      const auto py = static_cast<float>(i / kW);
      for (const auto& p : pts) {
        if (p.valid && std::fabs(std::hypot(px - p.px, py - p.py) - cfg.markers_radius_px_) < kMarkerHalfWidthPx) {
          ++ring_hits;
        }
      }
    }
    for (int j = 0; j < 3; ++j) {
      rgb[j] = std::clamp(rgb[j], 0.0f, 1.0f);
      rgb[j] = LinearToSrgb(rgb[j]);
      const auto expected_byte = static_cast<uint8_t>(rgb[j] * 255);
      // Non-fatal and capped: a fatal ASSERT_ inside this 90000-pixel loop would return out of the
      // function on the FIRST mismatch, silently hiding every row after it — exactly the failure
      // shape scripts/check_loop_fatal_asserts.py exists to catch. Capping the printed messages
      // (not the check itself) keeps a genuine widespread regression from flooding stdout with
      // ~270000 lines while still reporting the true total below.
      if (rr->img_buffer_[i * 3 + j] != expected_byte) {
        if (mismatches < kMaxReportedMismatches) {
          ADD_FAILURE() << "byte diverged at pixel " << i << " (row " << i / kW << ", col " << i % kW << "), channel "
                        << j << " — a parallel band either raced on shared scratch or split a pixel's own math";
        }
        ++mismatches;
      }
      if (expected_byte != 0) {
        ++nonzero;
      }
    }
  }
  EXPECT_EQ(mismatches, 0u) << mismatches << " total byte(s) diverged (messages capped at " << kMaxReportedMismatches
                            << ")";
  EXPECT_GT(nonzero, 0u);
  EXPECT_GT(ring_hits, 100u) << "too few ring pixels landed — the fixture is not really exercising the marker "
                                "scratch buffer";
}

}  // namespace
}  // namespace lumice
