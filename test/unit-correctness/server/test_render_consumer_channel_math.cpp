// The channel-B-R display mode (RenderConfig::kDisplayChannelBr) on the CLI/server side, asserted on
// the properties the mode owes rather than on a table of bytes. The byte-exact "the fused loop
// computes exactly this" statement lives in test_render_consumer_post_snapshot_fusion.cpp's
// ChannelBrOverNonzeroBackground case, beside the other branches of that loop.
//
// What this file owns:
//   - the whole pixel chain agrees with an INDEPENDENT reference implementation of it: the exposed
//     XYZ of a handful of pixels was run through a separately written re-implementation of the
//     display pipeline (gamut clip, XYZ -> linear RGB, sRGB transfer curve, then B - R as a grey
//     offset), and its values are pinned below as literals. The renderer must land within 1/255 of
//     each. This is a cross-language check of the chain, not of the one-line formula (that is
//     test_channel_math.cpp), and it is what would catch, say, B - R taken before the gamma.
//   - a neutral pixel is mid grey on EVERY frame path: an unlit pixel, a pixel outside the image
//     circle, a snapshot before any light, and a snapshot whose exposure scale is zero. The last two
//     are early exits that never reach the pixel loop.
//   - the mode is inert under tone=print, and display_mode normal is the picture it always was.
//   - the annotations are drawn ON the diagnostic image in their own colour, not turned grey with it.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "config/color_class_table.hpp"
#include "config/light_config.hpp"
#include "config/render_config.hpp"
#include "config/sim_data.hpp"
#include "server/render.hpp"
#include "support/thread_budget.hpp"

namespace lumice {
namespace {

constexpr int kW = 64;
constexpr int kH = 64;
constexpr int kTotalPix = kW * kH;
// ChannelMathBrGray(x, x) == 0.5, narrowed the way the writer narrows (truncation of 127.5).
constexpr uint8_t kMidGrey = 127;

struct Ray {
  float d[3];
  float wl;
  float w;
};

// Several spectral colours on distinct pixels, so one frame carries a bluer pixel, a redder one, a
// near-neutral mix, a saturating one and a faint one. The zenith pixel receives three wavelengths.
const Ray kRays[]{
  { { 0.0f, 0.0f, -1.0f }, 450.0f, 1.0f },     { { 0.0f, 0.0f, -1.0f }, 550.0f, 1.0f },
  { { 0.0f, 0.0f, -1.0f }, 650.0f, 1.0f },     { { 0.5f, 0.0f, -0.866f }, 450.0f, 1.0f },
  { { -0.5f, 0.0f, -0.866f }, 650.0f, 1.0f },  { { 0.0f, 0.5f, -0.866f }, 550.0f, 1.0f },
  { { 0.0f, -0.5f, -0.866f }, 480.0f, 20.0f }, { { 0.3f, 0.3f, -0.906f }, 600.0f, 0.05f },
};

// A 180 deg equal-area fisheye pointed up, so the frame's corners fall outside the image circle.
// ev_mode absolute pins the exposure scale to the emitted energy, so the exposed XYZ below is a
// fixed function of kRays and does not depend on an anchor handoff.
RenderConfig MakeConfig(RenderConfig::DisplayMode mode) {
  RenderConfig cfg;
  cfg.id_ = 0;
  cfg.lens_.type_ = LensParam::kFisheyeEqualArea;
  cfg.lens_.fov_ = 180.0f;
  cfg.resolution_[0] = kW;
  cfg.resolution_[1] = kH;
  cfg.view_.el_ = 90.0f;
  cfg.visible_ = RenderConfig::kUpper;
  cfg.ev_mode_ = RenderConfig::kAbsolute;
  cfg.intensity_factor_ = 0.02f;
  cfg.display_mode_ = mode;
  return cfg;
}

struct Frame {
  std::vector<uint8_t> rgb;
  std::vector<float> exposed_xyz;
};

// `consume` false leaves the consumer with no light at all, which is the no-frame early exit.
Frame Snapshot(const RenderConfig& cfg, bool consume = true) {
  RenderConsumer rc(cfg, lumice::test::kTestThreadBudget, ColorClassTable{}, SunParam{ 45.0f, 0.0f, 0.5f });
  if (consume) {
    for (const auto& r : kRays) {
      SimData data;
      data.curr_wl_ = r.wl;
      for (float v : r.d) {
        data.outgoing_d_.push_back(v);
      }
      data.outgoing_w_.push_back(r.w);
      data.emitted_energy_ = r.w;
      rc.Consume(data);
    }
  }
  rc.PrepareSnapshot();
  rc.PostSnapshot();
  Frame f;
  const RawXyzResult raw = rc.GetRawXyzResult();
  const float scale = rc.ExposureScale();
  if (raw.xyz_buffer_ != nullptr) {
    f.exposed_xyz.resize(static_cast<size_t>(kTotalPix) * 3);
    for (size_t k = 0; k < f.exposed_xyz.size(); ++k) {
      f.exposed_xyz[k] = raw.xyz_buffer_[k] * scale;
    }
  }
  auto result = rc.GetResult();
  const auto* rr = std::get_if<RenderResult>(&result);
  if (rr != nullptr && rr->img_buffer_ != nullptr) {
    f.rgb.assign(rr->img_buffer_, rr->img_buffer_ + static_cast<size_t>(kTotalPix) * 3);
  }
  return f;
}

// The independent reference, frozen. `xyz` is the exposed XYZ the renderer reports for that pixel
// (pinned so a change upstream of this mode — a CMF table, the exposure formula — shows up as "the
// input moved" rather than as a formula failure); `gray` is the reference implementation's output
// on exactly that input, as a float in [0, 1]. No sky background: the reference chain has none.
struct Reference {
  int pixel;
  float xyz[3];
  double gray;
};
const Reference kReference[]{
  { 1312, { 0.0845804363f, 0.00955995359f, 0.445823431f }, 0.603596 },     // 450 nm: bluer
  { 1625, { 0.0133612938f, 0.00793727767f, 1.00631096e-05f }, 0.407164 },  // faint 600 nm
  { 2068, { 0.10904634f, 0.250307292f, 0.002201305f }, 0.454450 },         // 550 nm
  { 2080, { 0.264949083f, 0.286786079f, 0.44802472f }, 0.604496 },         // 450+550+650 mix
  { 2091, { 0.481217921f, 0.699486673f, 4.0904026f }, 1.000000 },          // saturating 480 nm
  { 2784, { 0.0713222846f, 0.0269188173f, 0.0f }, 0.357372 },              // 650 nm: redder
  { 0, { 0.0f, 0.0f, 0.0f }, 0.500000 },                                   // outside the circle
};

TEST(RenderConsumerChannelMath, MatchesAnIndependentReferenceOfTheWholeChain) {
  const Frame f = Snapshot(MakeConfig(RenderConfig::kDisplayChannelBr));
  ASSERT_EQ(f.rgb.size(), static_cast<size_t>(kTotalPix) * 3);
  ASSERT_EQ(f.exposed_xyz.size(), static_cast<size_t>(kTotalPix) * 3);
  for (const auto& ref : kReference) {
    const auto base = static_cast<size_t>(ref.pixel) * 3;
    for (int j = 0; j < 3; j++) {
      EXPECT_NEAR(f.exposed_xyz[base + j], ref.xyz[j], 1e-5f * std::max(1.0f, ref.xyz[j]))
          << "pixel " << ref.pixel << " channel " << j << ": the INPUT moved — re-derive the reference";
    }
    for (int j = 0; j < 3; j++) {
      EXPECT_NEAR(f.rgb[base + j] / 255.0, ref.gray, 1.0 / 255.0) << "pixel " << ref.pixel << " channel " << j;
    }
  }
  // The direction the diagnostic exists to show, stated plainly on the two single-wavelength pixels.
  EXPECT_GT(f.rgb[1312 * 3], kMidGrey) << "a 450 nm pixel must read bluer (lighter than mid grey)";
  EXPECT_LT(f.rgb[2784 * 3], kMidGrey) << "a 650 nm pixel must read redder (darker than mid grey)";
}

// Every output pixel is grey: R == G == B.
TEST(RenderConsumerChannelMath, EveryPixelIsGrey) {
  const Frame f = Snapshot(MakeConfig(RenderConfig::kDisplayChannelBr));
  ASSERT_EQ(f.rgb.size(), static_cast<size_t>(kTotalPix) * 3);
  int coloured = 0;
  for (int i = 0; i < kTotalPix; ++i) {
    if (f.rgb[i * 3] != f.rgb[i * 3 + 1] || f.rgb[i * 3] != f.rgb[i * 3 + 2]) {
      ++coloured;
    }
  }
  EXPECT_EQ(coloured, 0) << "pixels that are not grey";
}

// R == B == 0 is mid grey on every path that can produce a frame with no light in it. Pixel 0 is a
// corner (outside the image circle); the middle of the left edge's inner neighbour is inside the
// circle but unlit.
TEST(RenderConsumerChannelMath, NeutralIsMidGreyOnEveryFramePath) {
  constexpr int kCorner = 0;
  constexpr int kUnlitInside = 32 * kW + 10;
  const Frame lit = Snapshot(MakeConfig(RenderConfig::kDisplayChannelBr));
  ASSERT_FALSE(lit.rgb.empty());
  EXPECT_EQ(lit.rgb[kCorner * 3], kMidGrey);
  EXPECT_EQ(lit.rgb[kUnlitInside * 3], kMidGrey);

  // No light consumed at all: the no-frame fill.
  const Frame empty = Snapshot(MakeConfig(RenderConfig::kDisplayChannelBr), /*consume=*/false);
  ASSERT_FALSE(empty.rgb.empty());
  EXPECT_EQ(std::count(empty.rgb.begin(), empty.rgb.end(), kMidGrey), static_cast<std::ptrdiff_t>(empty.rgb.size()));

  // Light consumed but exposed at zero: the zero-scale exit.
  RenderConfig dark = MakeConfig(RenderConfig::kDisplayChannelBr);
  dark.intensity_factor_ = 0.0f;
  const Frame zero_scale = Snapshot(dark);
  ASSERT_FALSE(zero_scale.rgb.empty());
  EXPECT_EQ(std::count(zero_scale.rgb.begin(), zero_scale.rgb.end(), kMidGrey),
            static_cast<std::ptrdiff_t>(zero_scale.rgb.size()));
}

// display_mode normal is the Normal picture — byte for byte what a config that never mentions the
// field renders — and channel_br is not that picture (so the equality above is not vacuous).
TEST(RenderConsumerChannelMath, NormalIsTheUnchangedPicture) {
  RenderConfig untouched = MakeConfig(RenderConfig::kDisplayNormal);
  untouched.display_mode_ = RenderConfig{}.display_mode_;
  const Frame normal = Snapshot(MakeConfig(RenderConfig::kDisplayNormal));
  const Frame reference = Snapshot(untouched);
  const Frame channel = Snapshot(MakeConfig(RenderConfig::kDisplayChannelBr));
  EXPECT_EQ(normal.rgb, reference.rgb);
  EXPECT_NE(normal.rgb, channel.rgb);
}

// Under tone=print the mode is kept but has no effect: the bytes are print's own.
TEST(RenderConsumerChannelMath, InertUnderPrint) {
  RenderConfig print_normal = MakeConfig(RenderConfig::kDisplayNormal);
  print_normal.tone_ = RenderConfig::kPrint;
  RenderConfig print_channel = MakeConfig(RenderConfig::kDisplayChannelBr);
  print_channel.tone_ = RenderConfig::kPrint;
  EXPECT_EQ(Snapshot(print_normal).rgb, Snapshot(print_channel).rgb);
}

// A pure-red, fully opaque parallel drawn on the diagnostic image stays red: the annotations are
// composited AFTER the mode, in their own colour. Every pixel the line touched must be red-dominant
// (G == B, R above them), and at least one must have been touched.
TEST(RenderConsumerChannelMath, AnnotationsKeepTheirColourOnTop) {
  RenderConfig with_line = MakeConfig(RenderConfig::kDisplayChannelBr);
  GridLineParam line;
  line.value_ = 30.0f;
  line.opacity_ = 1.0f;
  line.color_[0] = 1.0f;
  line.color_[1] = 0.0f;
  line.color_[2] = 0.0f;
  with_line.elevation_grid_.push_back(line);
  const Frame plain = Snapshot(MakeConfig(RenderConfig::kDisplayChannelBr));
  const Frame lined = Snapshot(with_line);
  ASSERT_EQ(plain.rgb.size(), lined.rgb.size());
  int touched = 0;
  for (int i = 0; i < kTotalPix; ++i) {
    const size_t b = static_cast<size_t>(i) * 3;
    if (plain.rgb[b] == lined.rgb[b] && plain.rgb[b + 1] == lined.rgb[b + 1] && plain.rgb[b + 2] == lined.rgb[b + 2]) {
      continue;
    }
    ++touched;
    EXPECT_EQ(lined.rgb[b + 1], lined.rgb[b + 2]) << "pixel " << i;
    EXPECT_GT(lined.rgb[b], lined.rgb[b + 1]) << "pixel " << i << ": the line was turned grey";
  }
  EXPECT_GT(touched, 0) << "the parallel drew nothing — the case stopped covering what it claims";
}

}  // namespace
}  // namespace lumice
