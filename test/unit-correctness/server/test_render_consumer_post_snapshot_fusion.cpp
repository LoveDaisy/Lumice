// Bit-identical guard for RenderConsumer::PostSnapshot()'s XYZ→uint8 chain.
//
// PostSnapshot() used to run four separate full-buffer passes over a mutable
// scratch buffer (memcpy → scale → per-pixel XYZ→RGB+background → sRGB gamma →
// narrow to uint8). Those passes were fused into one per-pixel loop that keeps
// the intermediates in registers, which removed the scratch buffer entirely.
// Every pass was element-wise, so the fusion reorders no floating-point
// operation and the output must be byte-for-byte unchanged.
//
// This test states that claim as an executable contract: it independently
// recomputes the expected uint8 image from the consumer's own published inputs
// (GetRawXyzResult().xyz_buffer_ + ExposureScale()) using the same low-level
// color primitives PostSnapshot calls (GamutClipXyz / XyzToLinearRgb /
// LinearToSrgb, all in util/color_space.hpp and untouched by the fusion), and
// compares byte-for-byte — never with a tolerance. A tolerance here would
// silently accept exactly the floating-point reordering the fusion must not
// introduce.
//
// PostSnapshot also skips the background on pixels the lens does not image, so the
// re-derivation below carries its own domain/visibility predicate (PixelImagesSky). It is
// written out from the projection primitives rather than calling BuildVisibleMask, for the
// same reason the rest of this file does not call render.cpp: a contract test that invokes
// the code under test twice states nothing. This fixture's 16x16 canvas under a 180 deg
// equal-area fisheye puts all four corners outside the image circle (corner radius
// 1.33x the circle's), so the predicate is genuinely exercised, not decorative.
//
// Coverage: the branch combinations PostSnapshot can take, and its dispatch —
//   1. use_real_color (ray_color_[0] < 0) with a zero background;
//   2. the gray + ray_color tint fallback (no gamut clip) with a zero background;
//   3. use_real_color with a non-zero background, which lifts every empty pixel
//      off zero and drives the post-blend clamp on the lit pixel.
//   4. the subtractive (kPrint) operator, which replaces the whole colour branch
//      and the background blend with paper * 10^(-density). It is here rather
//      than only in test_render_consumer_print_mode.cpp because the property
//      this file exists for — the fused loop reorders no arithmetic — has to
//      hold for the third branch too, and it is the byte-level, no-tolerance
//      comparison that says so.
//   5. the row-parallel dispatch itself (ParallelRows, core/parallel_rows.hpp):
//      a 300x300 dual-fisheye canvas with every marker ring enabled, compared
//      byte-for-byte against an oracle that re-derives the ring compositing. The
//      four cases above all sit below ParallelRows' pixel threshold and so only
//      ever take its serial inline fallback; this one is the only case that runs
//      the multi-threaded path (see its own comment for why that matters).
//   6. the thread-budget gate of that dispatch: the same 300x300 scene through a
//      consumer whose budget forces the inline serial loop (0 — what a server with
//      every core busy passes) and through one whose budget admits a pool, compared
//      byte-for-byte against each other. Case 5 holds the parallel result against an
//      oracle; this one holds the NEW serial/parallel fork — the budget, distinct
//      from the pixel-count threshold — to the same bytes on both sides.
// Each case asserts its own coverage rather than assuming it: a non-black image
// (so the byte comparison is not vacuous) and, for case 3, that the post-blend
// clamp actually fired.

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "config/color_class_table.hpp"
#include "config/light_config.hpp"
#include "config/proj_config.hpp"
#include "config/render_config.hpp"
#include "config/sim_data.hpp"
#include "core/annotation_overlay.hpp"
#include "core/geo3d.hpp"
#include "core/projection.hpp"
#include "core/scatter_accum.hpp"  // MakeCameraRotation
#include "server/render.hpp"
#include "support/render_anchor.hpp"
#include "support/thread_budget.hpp"
#include "util/channel_math.hpp"
#include "util/color_data.hpp"
#include "util/color_space.hpp"
#include "util/ink_transfer.hpp"

namespace lumice {
namespace {

constexpr float kWl = 550.0f;

RenderConfig MakeSnapshotRenderConfig() {
  RenderConfig cfg;
  cfg.id_ = 0;
  cfg.lens_.type_ = LensParam::kFisheyeEqualArea;
  cfg.lens_.fov_ = 180.0f;
  cfg.resolution_[0] = 16;
  cfg.resolution_[1] = 16;
  cfg.view_.az_ = 0.0f;
  cfg.view_.el_ = 90.0f;
  cfg.view_.ro_ = 0.0f;
  cfg.visible_ = RenderConfig::kUpper;
  // This suite is a byte-level property test over the per-pixel fusion ORDER, which is
  // orthogonal to which exposure formula produced the scale. Pin the absolute anchor so the
  // fixed input keeps producing the fixed bytes these cases were calibrated against.
  cfg.ev_mode_ = RenderConfig::kAbsolute;
  return cfg;
}

// A deterministic batch: every ray points straight up, so the whole weight
// lands on one pixel and the other 255 pixels stay exactly zero. That mix is
// deliberate — the zero pixels exercise the background/clamp path on an
// otherwise black image, the lit pixel exercises the color transform.
SimData MakeUpwardBatch(const std::vector<float>& weights) {
  SimData data;
  data.curr_wl_ = kWl;
  data.outgoing_d_.reserve(weights.size() * 3);
  for (size_t i = 0; i < weights.size(); ++i) {
    data.outgoing_d_.push_back(0.0f);
    data.outgoing_d_.push_back(0.0f);
    data.outgoing_d_.push_back(-1.0f);  // sky-up
  }
  data.outgoing_w_ = weights;
  // The normalization denominator. In this fixture nothing is filtered and
  // every ray lands, so the energy emitted equals the energy that arrived — the
  // batch has to declare it either way, because the renderer divides by what was
  // emitted and no longer infers it from what landed.
  data.emitted_energy_ = std::accumulate(weights.begin(), weights.end(), 0.0f);
  return data;
}

// Does pixel `i` image a visible piece of sky? Independent re-derivation of the predicate
// PostSnapshot gates the background on, for this fixture's lens family (single equal-area
// fisheye) only — asserting a general mask belongs in the mask's own tests, not here.
//
// Deliberately complete rather than fixture-shaped: this fixture's `view.el_ = 90` points the
// 180 deg field straight up so `visible = kUpper` excludes nothing inside the circle, but the
// visibility half is still computed. Written the narrow way, changing `el_` here would silently
// stop testing what this function claims to test.
bool PixelImagesSky(const RenderConfig& cfg, int i) {
  const int w = cfg.resolution_[0];
  const int h = cfg.resolution_[1];
  const float short_pix = static_cast<float>(std::min(w, h));
  const float fov_rad = cfg.lens_.fov_ * 3.14159265358979323846f / 180.0f;
  // Equal-area: r = 1 (the inverse's domain edge) sits at theta = 90 deg.
  const float scale = short_pix / 2.0f / std::sqrt(2.0f) / std::sin(fov_rad / 4.0f);

  const float u = (static_cast<float>(i % w) + 0.5f - static_cast<float>(w) / 2.0f) / scale;
  const float v = (static_cast<float>(i / w) + 0.5f - static_cast<float>(h) / 2.0f) / scale;
  const projection::Dir3 c = projection::FisheyeEqualAreaInverse(-u, v, 1.0f);
  if (!c.valid) {
    return false;
  }
  float d[3]{ c.x, c.y, c.z };
  MakeCameraRotation(cfg).Apply(d);
  const float wz = -d[2];
  if (cfg.visible_ == RenderConfig::kUpper && wz > 0.0f) {
    return false;
  }
  if (cfg.visible_ == RenderConfig::kLower && wz < 0.0f) {
    return false;
  }
  return true;
}

// Per-pixel scaled XYZ → linear RGB, i.e. everything PostSnapshot does BEFORE
// the background blend. Shared by the expected-image builder, the
// clamp-coverage probe and the row-parallel case's oracle so they cannot drift apart.
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

// Independent re-derivation of PostSnapshot's per-pixel chain, written out step
// by step from the same primitives the production path calls. Deliberately
// literal (it does not call into render.cpp) so it is an independent statement
// of the formula rather than a second call into the code under test.
std::vector<uint8_t> ExpectedImage(const RenderConfig& cfg, const float* xyz_raw, int total_pix, float scale) {
  std::vector<uint8_t> out(static_cast<size_t>(total_pix) * 3);
  const bool print_mode = cfg.tone_ == RenderConfig::kPrint;
  for (int i = 0; i < total_pix; i++) {
    float rgb[3];
    const bool paint_bg = PixelImagesSky(cfg, i);
    if (print_mode) {
      // The subtractive branch replaces BOTH the colour transform and the background blend, so it
      // is written out here whole rather than folded into ScaledXyzToLinearRgb: under kPrint there
      // is no "linear RGB before the background" stage for that helper to return.
      const float e = paint_bg ? xyz_raw[i * 3 + 1] * scale : 0.0f;
      const float transmittance = InkTransmittance(InkOpticalDensity(e));
      for (int j = 0; j < 3; j++) {
        rgb[j] = cfg.paper_[j] * transmittance;
      }
    } else {
      ScaledXyzToLinearRgb(cfg, xyz_raw, i, scale, rgb);
    }
    for (int j = 0; j < 3; j++) {
      if (paint_bg && !print_mode) {
        rgb[j] += cfg.background_[j];
      }
    }
    // The channel-B-R display mode: after the background, on the post-gamma R and B of the pixel
    // the Normal mode would show, carried back to linear for the final clamp and gamma below.
    if (cfg.display_mode_ == RenderConfig::kDisplayChannelBr && !print_mode) {
      const float r_srgb = LinearToSrgb(std::clamp(rgb[0], 0.0f, 1.0f));
      const float b_srgb = LinearToSrgb(std::clamp(rgb[2], 0.0f, 1.0f));
      const float gray = SrgbToLinear(ChannelMathBrGray(r_srgb, b_srgb));
      rgb[0] = rgb[1] = rgb[2] = gray;
    }
    for (int j = 0; j < 3; j++) {
      rgb[j] = std::clamp(rgb[j], 0.0f, 1.0f);
      rgb[j] = LinearToSrgb(rgb[j]);
      out[i * 3 + j] = static_cast<uint8_t>(rgb[j] * 255);
    }
  }
  return out;
}

// How many channels the post-blend clamp actually altered. Used to prove the
// clamp branch was exercised — checking the output bytes for 255 cannot do
// that, because LinearToSrgb(1.0f) is 1.055f - 0.055f == 0.99999994f in float,
// so a fully saturated channel narrows to 254 and 255 is unreachable.
size_t CountPostBlendClamps(const RenderConfig& cfg, const float* xyz_raw, int total_pix, float scale) {
  size_t clamped = 0;
  for (int i = 0; i < total_pix; i++) {
    float rgb[3];
    ScaledXyzToLinearRgb(cfg, xyz_raw, i, scale, rgb);
    const float bg_scale = PixelImagesSky(cfg, i) ? 1.0f : 0.0f;
    for (int j = 0; j < 3; j++) {
      const float blended = rgb[j] + cfg.background_[j] * bg_scale;
      if (blended != std::clamp(blended, 0.0f, 1.0f)) {
        ++clamped;
      }
    }
  }
  return clamped;
}

struct Coverage {
  size_t nonzero_bytes = 0;
  size_t clamped_channels = 0;
  size_t unimaged_pixels = 0;
};

// Drives one consumer through a snapshot and asserts the produced image is
// byte-identical to the independently computed expectation. Reports what the
// scene actually covered through `cov` so each caller can assert its own
// coverage. Returns void because it uses ASSERT_*.
void RunAndCompare(const RenderConfig& cfg, const std::vector<float>& weights, const std::string& label,
                   Coverage* cov) {
  *cov = Coverage{};
  RenderConsumer rc(cfg, lumice::test::kTestThreadBudget, ColorClassTable{});
  auto data = MakeUpwardBatch(weights);
  rc.Consume(data);
  rc.PrepareSnapshot();
  rc.PostSnapshot();

  const int total_pix = cfg.resolution_[0] * cfg.resolution_[1];
  const auto raw = rc.GetRawXyzResult();
  const float scale = rc.ExposureScale();
  ASSERT_GT(scale, 0.0f) << label << ": exposure scale must be positive or PostSnapshot takes the early-out path";

  const auto expected = ExpectedImage(cfg, raw.xyz_buffer_, total_pix, scale);

  auto result = rc.GetResult();
  const auto* rr = std::get_if<RenderResult>(&result);
  ASSERT_NE(rr, nullptr) << label << ": RenderConsumer must produce a RenderResult";

  size_t nonzero = 0;
  for (size_t i = 0; i < expected.size(); ++i) {
    ASSERT_EQ(rr->img_buffer_[i], expected[i])
        << label << ": byte " << i << " diverged (pixel " << i / 3 << ", channel " << i % 3
        << ") — the fused loop must not reorder any "
           "floating-point operation";
    if (rr->img_buffer_[i] != 0) {
      ++nonzero;
    }
  }
  cov->nonzero_bytes = nonzero;
  cov->clamped_channels = CountPostBlendClamps(cfg, raw.xyz_buffer_, total_pix, scale);
  for (int i = 0; i < total_pix; ++i) {
    if (!PixelImagesSky(cfg, i)) {
      ++cov->unimaged_pixels;
    }
  }
}

// -----------------------------------------------------------------------------
// 1. use_real_color path (gamut clip → matrix), zero background.
// -----------------------------------------------------------------------------
TEST(RenderConsumerPostSnapshotFusion, RealColorZeroBackground) {
  RenderConfig cfg = MakeSnapshotRenderConfig();
  // ray_color_ keeps its {-1,-1,-1} default → use_real_color.
  Coverage cov;
  RunAndCompare(cfg, { 0.5f, 0.7f, 0.3f, 0.9f }, "RealColorZeroBackground", &cov);
  EXPECT_GT(cov.nonzero_bytes, 0u) << "an all-black image would make the byte comparison vacuous";
}

// -----------------------------------------------------------------------------
// 2. Gray + ray_color tint fallback (no gamut clip), zero background.
// -----------------------------------------------------------------------------
TEST(RenderConsumerPostSnapshotFusion, GrayTintZeroBackground) {
  RenderConfig cfg = MakeSnapshotRenderConfig();
  cfg.ray_color_[0] = 1.0f;  // >= 0 → gray + tint branch
  cfg.ray_color_[1] = 0.5f;
  cfg.ray_color_[2] = 0.2f;
  Coverage cov;
  RunAndCompare(cfg, { 0.5f, 0.7f, 0.3f, 0.9f }, "GrayTintZeroBackground", &cov);
  EXPECT_GT(cov.nonzero_bytes, 0u) << "an all-black image would make the byte comparison vacuous";
}

// -----------------------------------------------------------------------------
// 3. use_real_color + non-zero background: covers `rgb += background` and the
//    [0,1] clamp that follows it.
// -----------------------------------------------------------------------------
TEST(RenderConsumerPostSnapshotFusion, RealColorNonzeroBackground) {
  RenderConfig cfg = MakeSnapshotRenderConfig();
  cfg.background_[0] = 0.9f;
  cfg.background_[1] = 0.25f;
  cfg.background_[2] = 0.4f;
  Coverage cov;
  RunAndCompare(cfg, { 0.5f, 0.7f, 0.3f, 0.9f }, "RealColorNonzeroBackground", &cov);
  EXPECT_GT(cov.nonzero_bytes, 0u) << "an all-black image would make the byte comparison vacuous";
  EXPECT_GT(cov.clamped_channels, 0u) << "the post-blend clamp never fired — retune the background/weights so this "
                                         "scene actually covers the clamp";
  // With a zero background the mask changes nothing, so only this case can show it works.
  // 16x16 under a 180 deg equal-area fisheye leaves 48 corner pixels outside the image circle.
  EXPECT_EQ(cov.unimaged_pixels, 48u) << "no pixel fell outside the lens's domain — this case is what pins that the "
                                         "background is withheld there, and it just stopped covering it";
}

// -----------------------------------------------------------------------------
// 4. The subtractive (kPrint) operator on a non-white paper.
//
// Non-white on purpose: with paper = {1,1,1} the three channels carry the same
// number, so a bug that dropped the per-channel paper multiply (writing the bare
// transmittance) would produce identical bytes and pass. The paper here has three
// distinct components, so the multiply is load-bearing for the comparison.
// -----------------------------------------------------------------------------
TEST(RenderConsumerPostSnapshotFusion, PrintToneSubtractive) {
  RenderConfig cfg = MakeSnapshotRenderConfig();
  cfg.tone_ = RenderConfig::kPrint;
  cfg.paper_[0] = 0.94f;
  cfg.paper_[1] = 0.90f;
  cfg.paper_[2] = 0.82f;
  // Non-zero, and deliberately NOT the paper colour: the print branch must ignore the sky
  // background entirely, so leaving it at zero here would make "ignored" and "added" agree.
  cfg.background_[0] = 0.9f;
  cfg.background_[1] = 0.25f;
  cfg.background_[2] = 0.4f;
  Coverage cov;
  RunAndCompare(cfg, { 0.5f, 0.7f, 0.3f, 0.9f }, "PrintToneSubtractive", &cov);
  EXPECT_GT(cov.nonzero_bytes, 0u) << "an all-black image would make the byte comparison vacuous";
  // Same 48 corner pixels as case 3, and under kPrint they are the paper's own colour rather than
  // black — that is what makes them a real assertion here instead of a repeat of the case above.
  EXPECT_EQ(cov.unimaged_pixels, 48u) << "no pixel fell outside the lens's domain — this case is what pins that an "
                                         "unimaged pixel prints as bare paper, and it just stopped covering it";
}


// -----------------------------------------------------------------------------
// 4b. The channel-B-R display mode over a non-zero background. Non-zero on purpose: the mode reads
//     B - R off the picture the Normal mode would show, sky included, so this redder-than-blue
//     background moves the grey of every empty imaged pixel below mid grey — and must NOT move the
//     48 unimaged corners, which get no background and stay neutral. A zero background would make
//     "added before the mode" and "ignored" agree.
// -----------------------------------------------------------------------------
TEST(RenderConsumerPostSnapshotFusion, ChannelBrOverNonzeroBackground) {
  RenderConfig cfg = MakeSnapshotRenderConfig();
  cfg.display_mode_ = RenderConfig::kDisplayChannelBr;
  cfg.background_[0] = 0.3f;
  cfg.background_[1] = 0.25f;
  cfg.background_[2] = 0.1f;
  Coverage cov;
  RunAndCompare(cfg, { 0.5f, 0.7f, 0.3f, 0.9f }, "ChannelBrOverNonzeroBackground", &cov);
  EXPECT_GT(cov.nonzero_bytes, 0u) << "an all-black image would make the byte comparison vacuous";
  EXPECT_EQ(cov.unimaged_pixels, 48u);
}

// -----------------------------------------------------------------------------
// 5. The row-parallel dispatch, at a resolution that actually runs it.
//
// The helpers below (kParallelW .. ApplyMarkerRings) serve ONLY this case. They are
// a separate family from the RunAndCompare / ExpectedImage / PixelImagesSky chain
// above, which is shaped around a 16x16 single-fisheye fixture with one lit pixel:
// this case needs a dual-fisheye canvas large enough to clear ParallelRows' pixel
// threshold, a scattered batch, and a marker-ring oracle. The two families share
// only ScaledXyzToLinearRgb and must not be merged or made to reuse each other.
//
// Why this case exists at all: every other PostSnapshot fixture in the tree (the
// four 16x16 cases above, the markers suite's 128x64) sits BELOW
// core/parallel_rows.cpp's kParallelPixelThreshold (65536 px), so ParallelRows
// takes its serial inline fallback for all of them — none of their green ticks
// say anything about the multi-threaded path. 300x300 = 90000 px is chosen to
// clear that threshold, and markers_ is turned on because
// AnnotationLayers::on_marker_ring is the one piece of PostSnapshot state that is
// mutated per pixel rather than only read — exactly the state a naive
// shared-`layers`-across-threads parallelization would race on.
// -----------------------------------------------------------------------------

constexpr int kParallelW = 300;
constexpr int kParallelH = 300;
constexpr int kParallelTotalPix = kParallelW * kParallelH;
static_assert(static_cast<size_t>(kParallelTotalPix) > 65536, "must clear ParallelRows' pixel threshold");

constexpr float kMarkerHalfWidthPx = 1.5f;  // mirrors render.cpp's kMarkerHalfWidthPx

SunParam MakeParallelSun() {
  return SunParam{ 35.0f, 20.0f, 0.5f };
}

RenderConfig MakeParallelConfig() {
  RenderConfig cfg;
  cfg.id_ = 0;
  cfg.lens_.type_ = LensParam::kDualFisheyeEqualArea;
  cfg.lens_.fov_ = 180.0f;
  cfg.resolution_[0] = kParallelW;
  cfg.resolution_[1] = kParallelH;
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
  data.curr_wl_ = kWl;
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
// what ScaledXyzToLinearRgb above already re-derives; this case's whole point is the ring). `pts`
// and `colors` are parallel arrays over the configured markers, in config order —
// MarkerPointsForTest() exposes the SAME points production's CompositeAnnotations reads, matching
// this file's "no second call into the code under test" discipline in every step except one: the
// point itself is geometry this case does not re-derive, same as the existing markers suite's use
// of the same accessor.
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

TEST(RenderConsumerPostSnapshotFusion, LargeResolutionWithMarkersIsByteExactAgainstOracle) {
  RenderConfig cfg = MakeParallelConfig();
  RenderConsumer rc(cfg, lumice::test::kTestThreadBudget, ColorClassTable{}, MakeParallelSun());
  auto data = MakeScatteredBatch();
  rc.Consume(data);
  // This config leaves ev_mode_ at its relative default (unlike MakeSnapshotRenderConfig, which pins
  // kAbsolute), so the snapshot has to be taken at a sky anchor or ExposureScale() stays zero.
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
  // independent of PostSnapshot() and untouched by the parallel dispatch, so borrowing it does not
  // turn this into a second call into the code actually under test — the loop's arithmetic and the
  // marker-ring compositing are still re-derived independently below, which is the part this case
  // exists to check.
  const auto& visible_mask = rc.VisibleMask();
  ASSERT_EQ(visible_mask.size(), static_cast<size_t>(kParallelTotalPix));

  constexpr size_t kMaxReportedMismatches = 20;
  size_t mismatches = 0;
  size_t nonzero = 0;
  size_t ring_hits = 0;
  for (int i = 0; i < kParallelTotalPix; ++i) {
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
    ApplyMarkerRings(pts, colors, cfg.markers_radius_px_, cfg.markers_opacity_, i, kParallelW, rgb);
    {
      const auto px = static_cast<float>(i % kParallelW);
      const auto py = static_cast<float>(i / kParallelW);
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
          ADD_FAILURE() << "byte diverged at pixel " << i << " (row " << i / kParallelW << ", col " << i % kParallelW
                        << "), channel " << j
                        << " — a parallel band either raced on shared scratch or split a pixel's own math";
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

// Two consumers, one scene, one batch: the only thing that differs is the thread budget, so the
// only thing that can differ in the bytes is the dispatch path the budget selects. The parallel
// arm's budget is forced to at least 2 rather than taken from the machine, so that on a
// single-core host the case does not silently compare serial to serial.
TEST(RenderConsumerPostSnapshotFusion, ThreadBudgetForcedSerialMatchesUnconstrainedParallel) {
  RenderConfig cfg = MakeParallelConfig();
  const int parallel_budget = std::max(2, lumice::test::kTestThreadBudget);
  RenderConsumer rc_serial(cfg, /*thread_budget=*/0, ColorClassTable{}, MakeParallelSun());
  RenderConsumer rc_parallel(cfg, parallel_budget, ColorClassTable{}, MakeParallelSun());
  const auto data = MakeScatteredBatch();
  rc_serial.Consume(data);
  rc_parallel.Consume(data);
  lumice::test::TakeSnapshotAtFormerSelfAnchor(&rc_serial);
  lumice::test::TakeSnapshotAtFormerSelfAnchor(&rc_parallel);

  // The visible mask and the marker points are built by the constructor through the same budget,
  // so they are part of what is being compared, not only PostSnapshot's loop.
  EXPECT_EQ(rc_serial.VisibleMask(), rc_parallel.VisibleMask());

  auto serial = rc_serial.GetResult();
  auto parallel = rc_parallel.GetResult();
  const auto* rs = std::get_if<RenderResult>(&serial);
  const auto* rp = std::get_if<RenderResult>(&parallel);
  ASSERT_NE(rs, nullptr);
  ASSERT_NE(rp, nullptr);
  ASSERT_EQ(rs->img_width_, kParallelW);
  ASSERT_EQ(rs->img_height_, kParallelH);
  ASSERT_EQ(rp->img_width_, kParallelW);
  ASSERT_EQ(rp->img_height_, kParallelH);

  constexpr size_t kMaxReportedMismatches = 20;
  size_t mismatches = 0;
  size_t nonzero = 0;
  for (int i = 0; i < kParallelTotalPix * 3; ++i) {
    // Non-fatal and capped, for the same reason as the case above: a fatal assert inside the loop
    // would hide every byte after the first divergence.
    if (rs->img_buffer_[i] != rp->img_buffer_[i]) {
      if (mismatches < kMaxReportedMismatches) {
        ADD_FAILURE() << "byte " << i << " (pixel " << i / 3 << ", channel " << i % 3 << ") differs: serial "
                      << static_cast<int>(rs->img_buffer_[i]) << " vs parallel "
                      << static_cast<int>(rp->img_buffer_[i]);
      }
      ++mismatches;
    }
    if (rs->img_buffer_[i] != 0) {
      ++nonzero;
    }
  }
  EXPECT_EQ(mismatches, 0u) << mismatches << " total byte(s) diverged between the budget-0 (inline) and budget-"
                            << parallel_budget << " (pooled) consumers";
  EXPECT_GT(nonzero, 0u) << "an all-black image would make the byte comparison vacuous";
}

}  // namespace
}  // namespace lumice
