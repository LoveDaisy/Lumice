// RenderConsumer's side of the globe's far-side clip: on the globe one pixel images two sky
// directions — where its ray enters the sphere (near) and where it leaves (far) — and each is shown
// or hidden by its OWN `visible` verdict. The far side's light used to follow the near point's
// verdict instead, so under `visible: upper` looking down the far side's lower hemisphere showed
// through the near side's upper one.
//
// Every case places a ray EXACTLY on a pixel's near or far direction (the renderer's own inverse,
// mask_detail::PixelToWorld / GlobeFarPixelToWorld, which the forward lands back on the same pixel
// — test_annotation_overlay.cpp's WeightIsTheForwardProjectionsFarHitWeight pins that), so which
// side of the sphere a pixel's energy came from is known by construction rather than inferred.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "config/color_class_table.hpp"
#include "config/render_config.hpp"
#include "config/sim_data.hpp"
#include "core/lens_proj_build.hpp"
#include "core/scatter_accum.hpp"  // MakeCameraRotation
#include "server/render.hpp"
#include "server/server.hpp"
#include "support/render_anchor.hpp"
#include "support/thread_budget.hpp"

namespace lumice {
namespace {

constexpr int kRes = 48;
constexpr int kTotalPix = kRes * kRes;

RenderConfig MakeGlobeConfig(float el, RenderConfig::VisibleRange visible, float fade) {
  RenderConfig cfg;
  cfg.id_ = 0;
  cfg.lens_.type_ = LensParam::kGlobe;
  cfg.lens_.fov_ = 30.0f;
  cfg.resolution_[0] = kRes;
  cfg.resolution_[1] = kRes;
  cfg.view_.el_ = el;
  cfg.view_.az_ = 20.0f;
  cfg.visible_ = visible;
  cfg.globe_back_fade_ = fade;
  cfg.background_[0] = 0.25f;
  cfg.background_[1] = 0.25f;
  cfg.background_[2] = 0.25f;
  return cfg;
}

struct PixelDirs {
  int index = -1;
  mask_detail::MaskDir near_dir{};
  mask_detail::MaskDir far_dir{};
};

// The first pixel (row-major) whose near and far directions fall on the requested sides of the
// horizon under `upper` (which keeps z <= 0). Returns index -1 when the view has none.
PixelDirs FindPixel(const RenderConfig& cfg, bool near_kept, bool far_kept) {
  const Rotation rot = MakeCameraRotation(cfg);
  const lm_proj::ProjParams p = BuildProjParams(cfg, rot, static_cast<float>(kRes));
  // Stay a couple of pixels off the horizon in either direction so float rounding in the forward
  // cannot move the ray into the neighbouring (other-side) pixel.
  constexpr float kMargin = 0.05f;
  for (int py = 0; py < kRes; ++py) {
    for (int px = 0; px < kRes; ++px) {
      const mask_detail::MaskDir near_dir = mask_detail::PixelToWorld(cfg, p, rot, px, py);
      float mu = 0.0f;
      const mask_detail::MaskDir far_dir = mask_detail::GlobeFarPixelToWorld(cfg, p, rot, px, py, &mu);
      if (!near_dir.valid || !far_dir.valid) {
        continue;
      }
      const bool near_ok = near_kept ? near_dir.z < -kMargin : near_dir.z > kMargin;
      const bool far_ok = far_kept ? far_dir.z < -kMargin : far_dir.z > kMargin;
      if (near_ok && far_ok && lm_proj::GlobeBackFadeWeight(mu, cfg.globe_back_fade_) > 0.1f) {
        return { py * kRes + px, near_dir, far_dir };
      }
    }
  }
  return {};
}

SimData RaysAlong(const std::vector<mask_detail::MaskDir>& dirs) {
  SimData data;
  data.curr_wl_ = 550.0f;
  for (const auto& d : dirs) {
    data.outgoing_d_.insert(data.outgoing_d_.end(), { d.x, d.y, d.z });
    data.outgoing_w_.push_back(1.0f);
  }
  return data;
}

// One batch, one snapshot at the test anchor, the finished sRGB image.
std::vector<uint8_t> Snapshot(RenderConsumer* rc, const SimData& data) {
  rc->Consume(data);
  lumice::test::TakeSnapshotAtFormerSelfAnchor(rc);
  auto result = rc->GetResult();
  const auto* rr = std::get_if<RenderResult>(&result);
  if (rr == nullptr || rr->img_buffer_ == nullptr) {
    return {};
  }
  return { rr->img_buffer_, rr->img_buffer_ + static_cast<size_t>(kTotalPix) * 3u };
}

bool IsBlack(const std::vector<uint8_t>& img, int i) {
  return img[i * 3] == 0 && img[i * 3 + 1] == 0 && img[i * 3 + 2] == 0;
}

bool SamePixel(const std::vector<uint8_t>& img, int a, int b) {
  return std::equal(img.begin() + a * 3, img.begin() + a * 3 + 3, img.begin() + b * 3);
}

// Near clipped, far visible: the far side's light shows, and over black — the background follows
// the near side alone. This is the case the old rule got wrong in the dark direction.
TEST(RenderConsumerGlobeFarVisibility, FarLightShowsThroughAClippedNearPoint) {
  const RenderConfig cfg = MakeGlobeConfig(35.0f, RenderConfig::kUpper, 0.8f);
  const PixelDirs a = FindPixel(cfg, /*near_kept=*/false, /*far_kept=*/true);
  ASSERT_GE(a.index, 0) << "the view has no pixel whose near point is clipped and far point kept";
  // A second, independent lit pixel so the frame's exposure does not hinge on the pixel under test.
  const PixelDirs lit = FindPixel(cfg, /*near_kept=*/true, /*far_kept=*/true);
  ASSERT_GE(lit.index, 0);

  RenderConsumer rc(cfg, lumice::test::kTestThreadBudget, ColorClassTable{});
  ASSERT_EQ(rc.FarVisibleMask().size(), static_cast<size_t>(kTotalPix));
  ASSERT_EQ(rc.VisibleMask()[a.index], 0u);
  ASSERT_EQ(rc.FarVisibleMask()[a.index], 1u);

  const auto img = Snapshot(&rc, RaysAlong({ a.far_dir, lit.near_dir }));
  ASSERT_EQ(img.size(), static_cast<size_t>(kTotalPix) * 3u);
  EXPECT_FALSE(IsBlack(img, a.index)) << "the far side's light was clipped by the near point's verdict";
  const float far_energy = rc.SnapshotFarXyzForTest()[a.index * 3 + 1];
  EXPECT_GT(far_energy, 0.0f) << "the far hit was not kept in the far-side share";
}

// Near visible, far clipped: the far side's light does NOT show, only the near side's (here none,
// so the pixel is exactly the bare sky). This is the defect the owner saw: looking down under
// `upper`, the far lower hemisphere lit up the near upper one.
TEST(RenderConsumerGlobeFarVisibility, ClippedFarLightStaysOutOfAVisibleNearPoint) {
  const RenderConfig cfg = MakeGlobeConfig(-35.0f, RenderConfig::kUpper, 0.8f);
  const PixelDirs b = FindPixel(cfg, /*near_kept=*/true, /*far_kept=*/false);
  ASSERT_GE(b.index, 0) << "the view has no pixel whose near point is kept and far point clipped";
  const PixelDirs lit = FindPixel(cfg, /*near_kept=*/true, /*far_kept=*/true);
  ASSERT_GE(lit.index, 0);
  ASSERT_NE(lit.index, b.index);

  RenderConsumer rc(cfg, lumice::test::kTestThreadBudget, ColorClassTable{});
  ASSERT_EQ(rc.VisibleMask()[b.index], 1u);
  ASSERT_EQ(rc.FarVisibleMask()[b.index], 0u);

  // A bare-sky reference pixel: kept near side, no ray anywhere near it.
  int sky = -1;
  for (int i = 0; i < kTotalPix && sky < 0; ++i) {
    if (rc.VisibleMask()[i] != 0 && i != b.index && i != lit.index) {
      sky = i;
    }
  }
  ASSERT_GE(sky, 0);

  const auto img = Snapshot(&rc, RaysAlong({ b.far_dir, lit.near_dir }));
  ASSERT_EQ(img.size(), static_cast<size_t>(kTotalPix) * 3u);
  EXPECT_TRUE(SamePixel(img, b.index, sky)) << "light from a clipped far direction reached a visible near pixel";
  EXPECT_FALSE(SamePixel(img, lit.index, sky)) << "the control pixel is not lit; the comparison above is vacuous";

  // The raw XYZ is not a display: it still carries the far hit (`visible` never culls energy).
  const RawXyzResult raw = rc.GetRawXyzResult();
  EXPECT_GT(raw.xyz_buffer_[b.index * 3 + 1], 0.0f) << "the display clip removed energy from the raw buffer";
}

// The same pixel, near visible, with BOTH sides lit: it shows the near side's light only — i.e. it
// renders exactly like a pixel that received the near ray alone.
TEST(RenderConsumerGlobeFarVisibility, NearOnlyPixelShowsTheTotalMinusTheFarShare) {
  const RenderConfig cfg = MakeGlobeConfig(-35.0f, RenderConfig::kUpper, 0.8f);
  const PixelDirs b = FindPixel(cfg, /*near_kept=*/true, /*far_kept=*/false);
  ASSERT_GE(b.index, 0);

  RenderConsumer both(cfg, lumice::test::kTestThreadBudget, ColorClassTable{});
  RenderConsumer near_only(cfg, lumice::test::kTestThreadBudget, ColorClassTable{});
  // The far ray lands on its own pixel only, so the exposure anchor (a P99 over the frame) can move
  // between the two arms; compare in XYZ, the quantity the colour chain starts from.
  both.Consume(RaysAlong({ b.near_dir, b.far_dir }));
  near_only.Consume(RaysAlong({ b.near_dir }));
  both.PrepareSnapshot();
  near_only.PrepareSnapshot();
  const float* far_share = both.SnapshotFarXyzForTest();
  ASSERT_NE(far_share, nullptr);
  const RawXyzResult raw_both = both.GetRawXyzResult();
  const RawXyzResult raw_near = near_only.GetRawXyzResult();
  for (int j = 0; j < 3; ++j) {
    EXPECT_FLOAT_EQ(raw_both.xyz_buffer_[b.index * 3 + j] - far_share[b.index * 3 + j],
                    raw_near.xyz_buffer_[b.index * 3 + j])
        << "channel " << j;
  }
}

// fade 0 and `visible: full` (front off): the far side's share is not kept at all — no mask, no
// buffer — so PostSnapshot takes the exact pre-existing path for every pixel, which is what makes
// those frames byte-identical to the frames before this clip existed.
TEST(RenderConsumerGlobeFarVisibility, NoSplitAtZeroFadeOrUnderFull) {
  for (const auto& cfg :
       { MakeGlobeConfig(35.0f, RenderConfig::kUpper, 0.0f), MakeGlobeConfig(35.0f, RenderConfig::kFull, 0.8f) }) {
    RenderConsumer rc(cfg, lumice::test::kTestThreadBudget, ColorClassTable{});
    EXPECT_TRUE(rc.FarVisibleMask().empty());
    rc.PrepareSnapshot();
    EXPECT_EQ(rc.SnapshotFarXyzForTest(), nullptr);
  }
}

// The device-fused route hands the far side's share over as its own plane; the consumer folds it
// the same way it folds the total. A backend that omits it leaves the share at zero.
TEST(RenderConsumerGlobeFarVisibility, DeviceFusedFarPlaneIsFoldedIntoTheFarShare) {
  const RenderConfig cfg = MakeGlobeConfig(35.0f, RenderConfig::kUpper, 0.8f);
  const PixelDirs a = FindPixel(cfg, /*near_kept=*/false, /*far_kept=*/true);
  ASSERT_GE(a.index, 0);
  RenderConsumer rc(cfg, lumice::test::kTestThreadBudget, ColorClassTable{});

  SimData data;
  data.xyz_pixel_data_.assign(1, std::vector<float>(static_cast<size_t>(kTotalPix) * 3u, 0.0f));
  data.xyz_pixel_data_far_.assign(1, std::vector<float>(static_cast<size_t>(kTotalPix) * 3u, 0.0f));
  data.xyz_landed_weight_ = { 1.0f };
  for (int j = 0; j < 3; ++j) {
    data.xyz_pixel_data_[0][a.index * 3 + j] = 3.0f;
    data.xyz_pixel_data_far_[0][a.index * 3 + j] = 2.0f;
  }
  rc.Consume(data);
  data.xyz_pixel_data_far_.clear();  // a batch from a backend that does not produce the plane
  rc.Consume(data);
  rc.PrepareSnapshot();
  ASSERT_NE(rc.SnapshotFarXyzForTest(), nullptr);
  EXPECT_FLOAT_EQ(rc.SnapshotFarXyzForTest()[a.index * 3 + 1], 2.0f);
  EXPECT_FLOAT_EQ(rc.GetRawXyzResult().xyz_buffer_[a.index * 3 + 1], 6.0f);
}

}  // namespace
}  // namespace lumice
