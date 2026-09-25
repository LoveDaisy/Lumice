// The globe lens's back-side fade: the weight at which the sphere's far side shows through.
//
// The weight is written three times, because three worlds need it and none can include the others:
// core's lm_proj::GlobeBackFadeWeight (src/core/shared/projection_shared.h), which the CLI's forward
// projection applies on every backend; the GUI's CPU mirror GlobeBackFadeWeight
// (src/gui/preview_jacobian.hpp), barred from core headers by the C-API boundary; and the preview
// shader's globeBackFadeWeight, which GLSL cannot share with either. This file holds the two C++
// copies to each other sample by sample and holds the shape to what the feature promises; the
// shader is held to the CPU mirror by reading pixels (test/gui/functional/test_preview_globe_back_fade.cpp).
// "To each other" means to within kSameFormulaTol, not bit for bit — see its comment.
//
// What the feature promises, as the cases below state it:
//   * a range of 0 is the camera-facing hemisphere only — weight 0 everywhere, so the default
//     renders what the globe drew before the field existed;
//   * at the silhouette the far side joins the near side continuously, at weight 1;
//   * the weight falls monotonically with distance from the camera and is 0 once the far point is
//     `range` behind the silhouette;
//   * on the CLI side the far point lands on the SAME pixel as the near point of its ray — which is
//     what lets the fade be a weight on the existing forward projection rather than a second one.

#include <cfloat>
#include <cmath>
#include <cstring>

#include "core/shared/projection_shared.h"
#include "gtest/gtest.h"
#include "gui/preview_jacobian.hpp"

namespace {

constexpr float kD = lm_proj::kGlobeCameraD;

// The deepest far-side point, straight behind the sphere's centre: (D + 1) - sqrt(D^2 - 1).
float MaxDepth() {
  return (kD + 1.0f) - std::sqrt(kD * kD - 1.0f);
}

// Fade ranges from zero, through one inside the far side, one exactly at its deepest point, and one
// past it; far-side mu from the silhouette (1/D) to the antipode of the camera (-1).
constexpr float kRanges[] = { 0.0f, 0.05f, 0.3f, 0.8f, 1.12701665f, 1.5f };
constexpr int kMuSteps = 400;

float MuAt(int i) {
  return 1.0f / kD - (1.0f / kD + 1.0f) * static_cast<float>(i) / static_cast<float>(kMuSteps);
}

// How far two evaluations of the SAME weight formula on the SAME mu may land apart, and why the
// bound is absolute. The weight is an inline function in a header, and a test binary holds more
// than one compiled copy of it: this TU is built without the -march flag, while lumice_obj is
// built with LUMICE_ISA_LEVEL's (root CMakeLists.txt, lumice_apply_isa_march), and the linker keeps
// ONE of the out-of-line copies of an inline function such as ProjectExitToPixel — which can be
// lumice_obj's. With FMA available that copy contracts `1 - t*t*(3 - 2t)` into fused
// multiply-adds, this TU's does not, and the result ends in `1 - x` with x near 1, so the rounding
// difference is a fraction of FLT_EPSILON in absolute terms but unbounded in ULPs once the weight
// nears 0 (one side rounds to 0, the other to a tiny positive number). Measured on a GCC 13 /
// -march=native build over every float mu in [-1, 1/D] at six ranges: the largest absolute gap
// is 0.5 * FLT_EPSILON. EXPECT_FLOAT_EQ's 4-ULP window is therefore the wrong ruler (it failed at
// 11 ULP); 4 * FLT_EPSILON keeps an 8x margin and stays orders of magnitude below any real
// defect — a wrong mu, the wrong point, or a drifted coefficient moves the weight by far more.
constexpr float kSameFormulaTol = 4.0f * FLT_EPSILON;

}  // namespace

TEST(GlobeBackFade, CoreAndGuiCopiesAgree) {
  for (float range : kRanges) {
    for (int i = 0; i <= kMuSteps; ++i) {
      const float mu = MuAt(i);
      // Not bit-exact: the two copies may be compiled under different flags (see kSameFormulaTol).
      EXPECT_NEAR(lm_proj::GlobeBackFadeWeight(mu, range), lumice::gui::GlobeBackFadeWeight(mu, range), kSameFormulaTol)
          << "range=" << range << " mu=" << mu;
    }
  }
}

TEST(GlobeBackFade, ZeroRangeShowsNoFarSide) {
  for (int i = 0; i <= kMuSteps; ++i) {
    EXPECT_EQ(lm_proj::GlobeBackFadeWeight(MuAt(i), 0.0f), 0.0f) << "mu=" << MuAt(i);
    EXPECT_EQ(lm_proj::GlobeBackFadeWeight(MuAt(i), -0.5f), 0.0f) << "mu=" << MuAt(i);
  }
}

TEST(GlobeBackFade, StartsAtOneOnTheSilhouetteAndFallsMonotonically) {
  for (float range : kRanges) {
    if (range <= 0.0f) {
      continue;
    }
    EXPECT_FLOAT_EQ(lm_proj::GlobeBackFadeWeight(1.0f / kD, range), 1.0f) << "range=" << range;
    float prev = 1.0f;
    for (int i = 0; i <= kMuSteps; ++i) {
      const float w = lm_proj::GlobeBackFadeWeight(MuAt(i), range);
      EXPECT_LE(w, prev) << "range=" << range << " mu=" << MuAt(i);
      EXPECT_GE(w, 0.0f);
      EXPECT_LE(w, 1.0f);
      prev = w;
    }
  }
  // Continuity at the rim, as a limit rather than one point: a far point a hair behind the
  // silhouette carries nearly the full weight.
  EXPECT_GT(lm_proj::GlobeBackFadeWeight(1.0f / kD - 1e-3f, 0.3f), 0.999f);
}

TEST(GlobeBackFade, ReachesZeroAtTheRangeAndGrowsWithIt) {
  // Distance from the camera to a surface point is sqrt(D^2 + 1 - 2 D mu); invert it for the point
  // `depth` behind the silhouette.
  const auto mu_at_depth = [](float depth) {
    const float dist = std::sqrt(kD * kD - 1.0f) + depth;
    return (kD * kD + 1.0f - dist * dist) / (2.0f * kD);
  };
  for (float range : { 0.05f, 0.3f, 0.8f }) {
    EXPECT_EQ(lm_proj::GlobeBackFadeWeight(mu_at_depth(range * 1.01f), range), 0.0f) << "range=" << range;
    EXPECT_GT(lm_proj::GlobeBackFadeWeight(mu_at_depth(range * 0.5f), range), 0.0f) << "range=" << range;
  }
  // A larger range never shows less of any far point.
  for (int i = 0; i <= kMuSteps; ++i) {
    float prev = 0.0f;
    for (float range : kRanges) {
      const float w = lm_proj::GlobeBackFadeWeight(MuAt(i), range);
      EXPECT_GE(w, prev) << "range=" << range << " mu=" << MuAt(i);
      prev = w;
    }
  }
  // Past the deepest point every far point shows at some weight — the slider's top is meaningful.
  EXPECT_GT(lm_proj::GlobeBackFadeWeight(-1.0f, MaxDepth() * 1.1f), 0.0f);
}

// The forward half. With an identity camera rotation ProjectExitToPixel's globe branch sees c = -w,
// the camera sits at (0, 0, -D) looking toward +z, and a pixel's ray is (0,0,-D) + t (a, b, 1). Its
// two sphere crossings must land on the same pixel; the near one as an ordinary landed hit of
// weight 1, the far one — only when a range is set — as a non-landed hit carrying the fade weight.
TEST(GlobeBackFade, ForwardProjectionPutsTheFarPointOnItsRaysPixel) {
  lm_proj::ProjParams p{};
  p.proj_type = lm_proj::kProjGlobe;
  p.img_w = 512;
  p.img_h = 512;
  p.scale = 256.0f / std::tan(15.0f * 3.14159265f / 180.0f);  // fov 30, the globe default
  const float kIdentity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
  std::memcpy(p.rot, kIdentity, sizeof(kIdentity));

  const float kSlopes[][2] = { { 0.0013f, 0.0021f }, { 0.101f, -0.043f }, { -0.187f, 0.117f }, { 0.231f, 0.0517f } };
  for (const auto& ab : kSlopes) {
    const float a = ab[0];
    const float b = ab[1];
    // |(0,0,-D) + t (a,b,1)|^2 = 1  ->  (a^2 + b^2 + 1) t^2 - 2 D t + (D^2 - 1) = 0
    const double qa = static_cast<double>(a) * a + static_cast<double>(b) * b + 1.0;
    const double disc = kD * kD - qa * (kD * kD - 1.0);
    if (!(disc > 0.0)) {
      ADD_FAILURE() << "slope (" << a << ", " << b << ") misses the sphere";
      continue;
    }
    const double t_near = (kD - std::sqrt(disc)) / qa;
    const double t_far = (kD + std::sqrt(disc)) / qa;
    const auto c_at = [&](double t, float* c) {
      c[0] = static_cast<float>(t * a);
      c[1] = static_cast<float>(t * b);
      c[2] = static_cast<float>(-kD + t);
    };
    float cn[3];
    float cf[3];
    c_at(t_near, cn);
    c_at(t_far, cf);

    p.globe_back_fade = 0.0f;
    const lm_proj::ProjResult near0 = lm_proj::ProjectExitToPixel(p, -cn[0], -cn[1], -cn[2]);
    const lm_proj::ProjResult far0 = lm_proj::ProjectExitToPixel(p, -cf[0], -cf[1], -cf[2]);
    if (near0.count != 1) {
      ADD_FAILURE() << "near point of slope (" << a << ", " << b << ") not imaged at range 0";
      continue;
    }
    EXPECT_TRUE(near0.hits[0].bump_landed);
    EXPECT_EQ(near0.hits[0].weight, 1.0f);
    EXPECT_EQ(far0.count, 0) << "range 0 must cull the far side, as before the field existed";

    p.globe_back_fade = MaxDepth() * 1.1f;
    const lm_proj::ProjResult near1 = lm_proj::ProjectExitToPixel(p, -cn[0], -cn[1], -cn[2]);
    const lm_proj::ProjResult far1 = lm_proj::ProjectExitToPixel(p, -cf[0], -cf[1], -cf[2]);
    if (near1.count != 1 || far1.count != 1) {
      ADD_FAILURE() << "slope (" << a << ", " << b << "): near/far hit counts " << near1.count << "/" << far1.count;
      continue;
    }
    EXPECT_EQ(near1.hits[0].px, near0.hits[0].px);
    EXPECT_EQ(near1.hits[0].py, near0.hits[0].py);
    EXPECT_EQ(near1.hits[0].weight, 1.0f);
    EXPECT_EQ(far1.hits[0].px, near1.hits[0].px) << "slope (" << a << ", " << b << ")";
    EXPECT_EQ(far1.hits[0].py, near1.hits[0].py) << "slope (" << a << ", " << b << ")";
    EXPECT_FALSE(far1.hits[0].bump_landed) << "a far-side hit must not move landed_weight";
    // The eye-space mu of the far point is -c.z. The mu reaching the weight is bit-identical on both
    // sides (the identity rotation is exact); the weight itself may come from a differently
    // compiled copy of the formula (see kSameFormulaTol).
    EXPECT_NEAR(far1.hits[0].weight, lm_proj::GlobeBackFadeWeight(-cf[2], p.globe_back_fade), kSameFormulaTol);
    EXPECT_GT(far1.hits[0].weight, 0.0f);
    EXPECT_LT(far1.hits[0].weight, 1.0f);
  }
}
