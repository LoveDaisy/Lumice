// The globe lens's back-side fade, in rendered pixels.
//
// The globe looks at the sky from OUTSIDE a unit sphere, so a pixel's ray crosses the sphere twice:
// the near point the globe has always shown, and a far point behind it. `globe_back_fade` lets the
// far point's light through, weighted by how far behind the silhouette it sits (exponential fog,
// exp(-depth / range) — lm_proj::GlobeBackFadeWeight). Four claims about the shader can only be
// read off pixels:
//
//   * a range of 0 adds nothing: the frame is the near side alone, which is the globe as it was;
//   * a range > 0 adds exactly what the CPU mirror predicts — near light x its relative
//     illumination, plus far light x ITS relative illumination x the fade weight. This is the
//     check that holds the GLSL transcription of the weight and of the far-side Jacobian to
//     src/gui/preview_jacobian.hpp, which test_globe_back_fade.cpp in turn holds to core's copy;
//   * the far side's curves (grid, horizon, and the circle families beside them) are drawn at the
//     far point faded by the same weight, confined to the disc, and UNDER the near side's: a
//     near-side line pixel is byte-identical with the fade on or off, since a far-side line at
//     full strength would sit on top of the near side and read as part of it;
//   * nothing else is drawn at the far point — the markers are near-side points (core projects
//     them; the shader only draws the rings it is handed) and the globe has no lens border.
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
#include "gui/preview_renderer.hpp"
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
  bool markers = false;
  int visible = gui::kVisibleFull;
  float elevation = 20.0f;
  float sky = 0.0f;  // linear background, all three channels
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
  params.view_proj.elevation = g_req.elevation;
  params.view_proj.visible = g_req.visible;
  for (float& c : params.background_color_linear) {
    c = g_req.sky;
  }
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
  if (g_req.markers) {
    // One ring inside the disc, in the shader's centre-origin, y-up pixel space, and the border.
    params.overlay.show_lens_border = true;
    params.overlay.marker_screen_pos[0] = { 20.0f, -15.0f };
    params.overlay.marker_color[0] = { 1.0f, 1.0f, 1.0f };
    params.overlay.markers_alpha = 1.0f;
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
std::vector<unsigned char> Render(ImGuiTestContext* ctx, float fade, float luminance, bool overlays,
                                  bool markers = false, int visible = gui::kVisibleFull, float elevation = 20.0f,
                                  float sky = 0.0f) {
  g_req = Request{};
  g_req.visible = visible;
  g_req.elevation = elevation;
  g_req.sky = sky;
  g_req.fade = fade;
  g_req.luminance = luminance;
  g_req.overlays = overlays;
  g_req.markers = markers;
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

// The world z of the near (far = false) or far (far = true) crossing at buffer pixel (col, row) —
// the shader's globeInverse / globeFarDir solve, restated on the CPU from BuildViewMatrix. Only the
// sign of z is read (`visible` keeps z <= 0 under upper), so a direction is returned un-normalized
// in magnitude but exact in sign. `ok` is false off the sphere. The buffer is top-down and the
// shader's `pos` y-up, hence the flip.
float CrossingZ(int col, int row, float elevation, bool far_root, bool* ok) {
  const float x = static_cast<float>(col) + 0.5f - kCanvas * 0.5f;
  const float y = kCanvas * 0.5f - (static_cast<float>(row) + 0.5f);
  const float focal = Focal();
  const float len = std::sqrt(x * x + y * y + focal * focal);
  const float dx = x / len;
  const float dy = y / len;
  const float dz = -focal / len;
  const float d = gui::kGlobeCameraD;
  const float b = d * dz;
  const float disc = b * b - (d * d - 1.0f);
  *ok = disc >= 0.0f;
  if (!*ok) {
    return 0.0f;
  }
  const float t = far_root ? -b + std::sqrt(disc) : -b - std::sqrt(disc);
  const float hx = t * dx;
  const float hy = t * dy;
  const float hz = d + t * dz;
  float m[9];
  gui::BuildViewMatrix(elevation, 30.0f, 0.0f, m);  // column-major, as the shader's u_view_matrix
  return m[2] * hx + m[5] * hy + m[8] * hz;
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

  // The far side's curves. With a ZERO-energy field the far side has no light to add, so any
  // difference a range makes can only be something drawn at the far point. Grid and horizon at
  // full opacity: a near-side line pixel is then fully the line's colour whatever lies under it,
  // which is what makes "the near side wins" a byte-exact claim.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_globe_back_fade", "far_side_curves_fade_in_the_disc_under_the_near_ones");
    t->GuiFunc = GlobeBackFadeGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      const std::vector<unsigned char> f0 = Render(ctx, 0.0f, 0.0f, true);
      const std::vector<unsigned char> f_short = Render(ctx, 0.3f, 0.0f, true);
      const std::vector<unsigned char> f_long = Render(ctx, 1.5f, 0.0f, true);
      const std::size_t n = static_cast<std::size_t>(kCanvas) * kCanvas * 4;
      IM_CHECK(f0.size() == n && f_short.size() == n && f_long.size() == n);
      const float rho_limb = RhoLimb();
      int near_lit = 0;
      int near_changed = 0;
      int far_drawn = 0;
      int outside_changed = 0;
      long long ink_short = 0;
      long long ink_long = 0;
      for (int row = 0; row < kCanvas; ++row) {
        for (int col = 0; col < kCanvas; ++col) {
          const std::size_t i = (static_cast<std::size_t>(row) * kCanvas + col) * 4;
          const bool lit0 = f0[i] != 0 || f0[i + 1] != 0 || f0[i + 2] != 0;
          const bool differs =
              !std::equal(f0.begin() + static_cast<std::ptrdiff_t>(i), f0.begin() + static_cast<std::ptrdiff_t>(i + 3),
                          f_long.begin() + static_cast<std::ptrdiff_t>(i));
          if (lit0) {
            ++near_lit;
            if (differs || !std::equal(f0.begin() + static_cast<std::ptrdiff_t>(i),
                                       f0.begin() + static_cast<std::ptrdiff_t>(i + 3),
                                       f_short.begin() + static_cast<std::ptrdiff_t>(i))) {
              ++near_changed;
            }
            continue;
          }
          if (differs) {
            ++far_drawn;
            if (RhoAt(col, row) > rho_limb + 1.5f) {
              ++outside_changed;
            }
          }
          for (int ch = 0; ch < 3; ++ch) {
            ink_short += f_short[i + ch];
            ink_long += f_long[i + ch];
          }
        }
      }
      // Positive control: the near side draws, so an unchanged near side is a claim about order.
      IM_CHECK_GT(near_lit, 200);
      IM_CHECK_EQ(near_changed, 0);
      IM_CHECK_GT(far_drawn, 200);
      IM_CHECK_EQ(outside_changed, 0);
      // The same lines under a longer fog carry more of their colour: the far side's lines fade on
      // the range exactly as its light does.
      IM_CHECK_GT(ink_long, ink_short);
      IM_CHECK_GT(ink_short, 0);
    };
  }

  // Nothing but curves is drawn on the far side: with the markers and the lens border on and no
  // curve family, a zero-energy frame is the same bytes at any range.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_globe_back_fade", "no_marker_or_border_is_drawn_on_the_far_side");
    t->GuiFunc = GlobeBackFadeGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      const std::vector<unsigned char> f0 = Render(ctx, 0.0f, 0.0f, false, true);
      const std::vector<unsigned char> f1 = Render(ctx, 1.5f, 0.0f, false, true);
      const std::size_t n = static_cast<std::size_t>(kCanvas) * kCanvas * 4;
      IM_CHECK(f0.size() == n && f1.size() == n);
      // A positive control, so an identical pair is attributable to the far side drawing nothing
      // rather than to the ring not being drawn at all.
      int lit = 0;
      for (std::size_t i = 0; i < n; i += 4) {
        if (f0[i] != 0 || f0[i + 1] != 0 || f0[i + 2] != 0) {
          ++lit;
        }
      }
      IM_CHECK_GT(lit, 20);
      IM_CHECK(f0 == f1);
    };
  }

  // Each side of a globe pixel is clipped by ITS OWN direction. Under `visible: upper` the near and
  // far crossings of one pixel generally sit at different altitudes, and the picture must be the
  // prediction built per side: near light x relIllum where the near direction is kept, plus far light
  // x relIllumFar x weight where the FAR direction is kept, plus the sky only where the near one is.
  // Two camera elevations, because each puts one of the two disagreeing cases on screen:
  //   near clipped, far kept  -> the far side's light alone, no sky (it used to be black);
  //   near kept, far clipped  -> the near side's light and the sky alone (the far side's clipped
  //                              light used to leak in: the owner's "looking down under upper, the
  //                              back's lower hemisphere shows through").
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_globe_back_fade", "each_side_of_a_pixel_is_clipped_by_its_own_direction");
    t->GuiFunc = GlobeBackFadeGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      constexpr float kY = 0.08f;
      constexpr float kSky = 0.02f;
      constexpr float kFade = 1.5f;
      // Keep off the horizon on both crossings: a pixel whose direction is within this much of
      // z = 0 is left out, so a sub-pixel disagreement about which side it is on cannot read as red.
      constexpr float kZMargin = 0.03f;
      const float focal = Focal();
      const float rho_limb = RhoLimb();
      int near_out_far_in = 0;
      int near_in_far_out = 0;
      for (float elevation : { 35.0f, -35.0f }) {
        const std::vector<unsigned char> rgba =
            Render(ctx, kFade, kY, false, false, gui::kVisibleUpper, elevation, kSky);
        if (rgba.size() != static_cast<std::size_t>(kCanvas) * kCanvas * 4) {
          IM_ERRORF("el %.0f: the off-screen render produced no frame", static_cast<double>(elevation));
          break;
        }
        int checked = 0;
        int worst = 0;
        int worst_col = -1;
        int worst_row = -1;
        for (int row = 0; row < kCanvas; row += 2) {
          for (int col = 0; col < kCanvas; col += 2) {
            const float rho = RhoAt(col, row);
            if (rho > rho_limb - 2.0f) {
              continue;
            }
            bool ok_near = false;
            bool ok_far = false;
            const float z_near = CrossingZ(col, row, elevation, false, &ok_near);
            const float z_far = CrossingZ(col, row, elevation, true, &ok_far);
            if (!ok_near || !ok_far || std::fabs(z_near) < kZMargin || std::fabs(z_far) < kZMargin) {
              continue;
            }
            const bool near_kept = z_near < 0.0f;  // upper keeps z <= 0
            const bool far_kept = z_far < 0.0f;
            float linear = 0.0f;
            if (near_kept) {
              linear += kY * gui::RelIllumGlobe(rho, focal) + kSky;
            }
            if (far_kept) {
              linear += kY * gui::RelIllumGlobeFar(rho, focal) * gui::GlobeBackFadeWeight(FarMu(rho), kFade);
            }
            if (linear > 0.95f) {
              continue;
            }
            near_out_far_in += (!near_kept && far_kept) ? 1 : 0;
            near_in_far_out += (near_kept && !far_kept) ? 1 : 0;
            const int expected = static_cast<int>(std::lround(lumice::LinearToSrgb(linear) * 255.0f));
            for (int ch = 0; ch < 3; ++ch) {
              const int diff = std::abs(Byte(rgba, col, row, ch) - expected);
              if (diff > worst) {
                worst = diff;
                worst_col = col;
                worst_row = row;
              }
            }
            ++checked;
          }
        }
        if (checked < 1000) {
          IM_ERRORF("el %.0f: only %d pixels were predictable", static_cast<double>(elevation), checked);
        }
        if (worst > kPredictToleranceLsb) {
          IM_ERRORF(
              "el %.0f: pixel (%d, %d) is %d LSB from the per-side prediction (tolerance %d) — a side shown or "
              "hidden by the other side's direction, or the sky painted under a clipped near point",
              static_cast<double>(elevation), worst_col, worst_row, worst, kPredictToleranceLsb);
        }
        if (ctx->IsError()) {
          break;
        }
      }
      // Both disagreeing cases were actually on screen, or the prediction above never tested them.
      IM_CHECK_GT(near_out_far_in, 200);
      IM_CHECK_GT(near_in_far_out, 200);
    };
  }
}
