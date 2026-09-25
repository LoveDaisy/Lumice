// The globe lens's back-side fade, in rendered pixels.
//
// The globe looks at the sky from OUTSIDE a unit sphere, so a pixel's ray crosses the sphere twice:
// the near point the globe has always shown, and a far point behind it. `globe_back_fade` lets the
// far point's light through, weighted by how far behind the silhouette it sits (a smoothstep to 0
// at the fade range — lm_proj::GlobeBackFadeWeight). Three claims about the shader can only be
// read off pixels:
//
//   * a range of 0 adds nothing: the frame is the near side alone, which is the globe as it was;
//   * a range > 0 adds exactly what the CPU mirror predicts — near light x its relative
//     illumination, plus far light x ITS relative illumination x the fade weight. This is the
//     check that holds the GLSL transcription of the weight and of the far-side Jacobian to
//     src/gui/preview_jacobian.hpp, which test_globe_back_fade.cpp in turn holds to core's copy;
//   * the far side contributes light and nothing else: no grid line, horizon or lens border is
//     drawn at the far point (AC6 of the feature — a far-side overlay at full strength would sit
//     on top of the near side and read as part of it).
//
// Capture path: RenderExportToRgba's off-screen FBO, the same one test_preview_background.cpp
// uses, with a UNIFORM XYZ field uploaded directly and no simulation — so every pixel's expected
// value is closed-form, and the direction a texel came from does not matter.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "gui/export_fbo_renderer.hpp"
#include "gui/gui_constants.hpp"
#include "gui/preview_jacobian.hpp"
#include "test_gui_shared.hpp"
#include "util/color_space.hpp"

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr int kCanvas = 256;
constexpr float kFov = 30.0f;  // the globe default

// The shader's D65 white (preview_renderer.cpp kWhitePointD65): an XYZ field of this chromaticity
// converts to equal linear RGB channels, so a pixel's expected value is one number.
constexpr float kD65[3] = { 0.95047f, 1.00000f, 1.08883f };

// Byte slack for the prediction: the float16 texture storage (~1e-3 relative) and the GPU's
// float-to-UNORM rounding against the CPU's lround. The things this suite exists to catch — a far
// side left out, doubled, or missing its Jacobian — are tens of bytes away.
constexpr int kPredictToleranceLsb = 2;

struct Request {
  bool requested = false;
  bool done = false;
  float fade = 0.0f;
  float luminance = 0.0f;  // uniform field Y, in D65 chromaticity
  bool overlays = false;
  std::vector<unsigned char> rgba;
};

Request g_req;

void RunRequest() {
  constexpr int kTex = 64;
  std::vector<float> xyz(static_cast<std::size_t>(kTex) * kTex * 3);
  for (std::size_t i = 0; i < xyz.size(); ++i) {
    xyz[i] = kD65[i % 3] * g_req.luminance;
  }
  gui::g_preview.UploadXyzTexture(xyz.data(), kTex, kTex);

  gui::PreviewParams params{};
  params.view_proj.lens_type = gui::kLensTypeGlobe;
  params.view_proj.fov = kFov;
  params.view_proj.elevation = 20.0f;
  params.view_proj.azimuth = 30.0f;
  params.view_proj.globe_back_fade = g_req.fade;
  params.source.max_abs_dz = gui::kDualFisheyeOverlap;
  params.source.r_scale = 1.0f / std::sqrt(1.0f + gui::kDualFisheyeOverlap);
  params.exposure.intensity_factor = 1.0f;
  params.exposure.intensity_scale = 1.0f;
  if (g_req.overlays) {
    params.overlay.show_horizon = true;
    params.overlay.show_grid = true;
    params.overlay.show_lens_border = true;
    params.overlay.elevation_deg = { -60.0f, -30.0f, 0.0f, 30.0f, 60.0f };
    params.overlay.longitude_deg = { -150.0f, -90.0f, -30.0f, 30.0f, 90.0f, 150.0f };
    params.overlay.horizon_alpha = 1.0f;
    params.overlay.grid_alpha = 1.0f;
  }
  g_req.rgba = gui::RenderExportToRgba(gui::g_preview, params, kCanvas, kCanvas);
  g_req.done = true;
  g_req.requested = false;
}

void GlobeBackFadeGuiFunc(ImGuiTestContext* /*ctx*/) {
  if (g_req.requested && !g_req.done) {
    RunRequest();
  }
}

// One frame from the render thread (the GL context lives there, not on the test coroutine).
std::vector<unsigned char> Render(ImGuiTestContext* ctx, float fade, float luminance, bool overlays) {
  g_req = Request{};
  g_req.fade = fade;
  g_req.luminance = luminance;
  g_req.overlays = overlays;
  g_req.requested = true;
  for (int i = 0; i < 60 && !g_req.done; ++i) {
    ctx->Yield();
  }
  return g_req.rgba;
}

// The shader's `pos` at the centre of buffer pixel (col, row): centre-origin, y-up. The buffer is
// top-down, but every quantity here is radially symmetric, so the sign of y does not matter.
float RhoAt(int col, int row) {
  const float x = static_cast<float>(col) + 0.5f - kCanvas * 0.5f;
  const float y = kCanvas * 0.5f - (static_cast<float>(row) + 0.5f);
  return std::sqrt(x * x + y * y);
}

float Focal() {
  return kCanvas * 0.5f / std::tan(kFov * 0.5f * kPi / 180.0f);
}

float RhoLimb() {
  return Focal() / std::sqrt(gui::kGlobeCameraD * gui::kGlobeCameraD - 1.0f);
}

// hit_eye.z of the far crossing for a pixel at `rho`: the other root of the solve the shader's
// globeFarDir runs.
float FarMu(float rho) {
  const float d = gui::kGlobeCameraD;
  const float k = rho / Focal();
  const float k2 = k * k;
  return (d * k2 - std::sqrt(std::max(1.0f - k2 * (d * d - 1.0f), 0.0f))) / (k2 + 1.0f);
}

int Byte(const std::vector<unsigned char>& rgba, int col, int row, int ch) {
  return rgba[(static_cast<std::size_t>(row) * kCanvas + col) * 4 + ch];
}

}  // namespace

void RegisterPreviewGlobeBackFadeTests(ImGuiTestEngine* engine) {
  // The prediction, per pixel, across the whole disc at three ranges — 0 among them, which is the
  // "adds nothing" claim stated as the same equation with the far term at weight 0.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_globe_back_fade", "the_far_side_adds_what_the_cpu_mirror_predicts");
    t->GuiFunc = GlobeBackFadeGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      // Dim enough that no probed pixel clips: the far side's relative illumination reaches ~2.8
      // at the centre and the near side's grows toward the rim.
      constexpr float kY = 0.08f;
      const float focal = Focal();
      const float rho_limb = RhoLimb();
      for (float fade : { 0.0f, 0.4f, 1.5f }) {
        const std::vector<unsigned char> rgba = Render(ctx, fade, kY, false);
        if (rgba.size() != static_cast<std::size_t>(kCanvas) * kCanvas * 4) {
          IM_ERRORF("fade %.2f: the off-screen render produced no frame", static_cast<double>(fade));
          break;
        }
        int checked = 0;
        int worst = 0;
        for (int row = 0; row < kCanvas; row += 3) {
          for (int col = 0; col < kCanvas; col += 3) {
            const float rho = RhoAt(col, row);
            // Two pixels inside the silhouette, where the relative illumination's own half-pixel
            // guard and the disc's anti-aliasing-free edge are both out of the way.
            if (rho > rho_limb - 2.0f) {
              continue;
            }
            float gain = gui::RelIllumGlobe(rho, focal);
            if (fade > 0.0f) {
              gain += gui::RelIllumGlobeFar(rho, focal) * gui::GlobeBackFadeWeight(FarMu(rho), fade);
            }
            const float linear = kY * gain;
            if (linear > 0.95f) {
              continue;  // would clip; the prediction is about the sum, not the clamp
            }
            const int expected = static_cast<int>(std::lround(lumice::LinearToSrgb(linear) * 255.0f));
            for (int ch = 0; ch < 3; ++ch) {
              worst = std::max(worst, std::abs(Byte(rgba, col, row, ch) - expected));
            }
            ++checked;
          }
        }
        if (checked < 1000) {
          IM_ERRORF("fade %.2f: only %d pixels were predictable — the probe grid no longer covers the disc",
                    static_cast<double>(fade), checked);
        }
        if (worst > kPredictToleranceLsb) {
          IM_ERRORF(
              "fade %.2f: a pixel inside the disc is %d LSB from near + far x relIllumFar x weight "
              "(tolerance %d). A miss near 0 range means the far side leaks in; a miss that grows with "
              "the range means the shader's weight or far-side Jacobian has left the CPU mirror.",
              static_cast<double>(fade), worst, kPredictToleranceLsb);
        }
        // The next range drives the context again; after a report that would only echo it.
        if (ctx->IsError()) {
          break;
        }
      }
    };
  }

  // Monotone in the range, and confined to the disc. Checked over EVERY pixel rather than a probe
  // grid, since "never darker" and "nothing outside" are claims about all of them.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_globe_back_fade", "a_larger_range_only_adds_light_inside_the_disc");
    t->GuiFunc = GlobeBackFadeGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      constexpr float kY = 0.05f;
      const std::vector<unsigned char> f0 = Render(ctx, 0.0f, kY, false);
      const std::vector<unsigned char> f1 = Render(ctx, 0.4f, kY, false);
      const std::vector<unsigned char> f2 = Render(ctx, 1.5f, kY, false);
      const std::size_t n = static_cast<std::size_t>(kCanvas) * kCanvas * 4;
      IM_CHECK(f0.size() == n && f1.size() == n && f2.size() == n);
      const float rho_limb = RhoLimb();
      int darker = 0;
      int outside_changed = 0;
      int brighter_near_rim = 0;
      for (int row = 0; row < kCanvas; ++row) {
        for (int col = 0; col < kCanvas; ++col) {
          const float rho = RhoAt(col, row);
          for (int ch = 0; ch < 3; ++ch) {
            const int a = Byte(f0, col, row, ch);
            const int b = Byte(f1, col, row, ch);
            const int c = Byte(f2, col, row, ch);
            if (b < a || c < b) {
              ++darker;
            }
            if (rho > rho_limb + 1.5f && (a != b || a != c)) {
              ++outside_changed;
            }
          }
          // Just inside the rim the far point is barely behind the silhouette, so its weight is ~1
          // at either range: the brightening there is the continuity claim, seen on screen.
          if (rho > rho_limb - 12.0f && rho < rho_limb - 4.0f && Byte(f1, col, row, 1) > Byte(f0, col, row, 1)) {
            ++brighter_near_rim;
          }
        }
      }
      IM_CHECK_EQ(darker, 0);
      IM_CHECK_EQ(outside_changed, 0);
      IM_CHECK_GT(brighter_near_rim, 100);
    };
  }

  // The far side brings light and nothing else. With a ZERO-energy field the far side has no light
  // to add, so any difference a range makes can only be something drawn — an overlay evaluated at
  // the far point. Every overlay the globe has is on (grid, horizon, lens border — the border is a
  // no-op on globe, which is itself part of what is held here) and at full opacity.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_globe_back_fade", "no_overlay_is_drawn_on_the_far_side");
    t->GuiFunc = GlobeBackFadeGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      const std::vector<unsigned char> f0 = Render(ctx, 0.0f, 0.0f, true);
      const std::vector<unsigned char> f1 = Render(ctx, 1.5f, 0.0f, true);
      const std::size_t n = static_cast<std::size_t>(kCanvas) * kCanvas * 4;
      IM_CHECK(f0.size() == n && f1.size() == n);
      // A positive control, so an identical pair is attributable to the far side drawing nothing
      // rather than to the overlays not being drawn at all.
      int lit = 0;
      for (std::size_t i = 0; i < n; i += 4) {
        if (f0[i] != 0 || f0[i + 1] != 0 || f0[i + 2] != 0) {
          ++lit;
        }
      }
      IM_CHECK_GT(lit, 200);
      IM_CHECK(f0 == f1);
    };
  }
}
