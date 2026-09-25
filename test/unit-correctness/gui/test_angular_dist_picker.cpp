// The Angular Distance picker's logic (src/gui/angular_dist_picker.hpp): the radius measured at a
// canvas pixel, the pixels that are not pickable, and the list/overlay writes a pick makes. The
// ImGui half — arming, the ring on the preview, the click, Esc and right-click — is
// test/gui/functional/test_angular_dist_picker.cpp.
//
// The radius oracles are INDEPENDENT of the code under test: a closed-form lens formula for the
// axis-centred family, and core's own inverse (LUMICE_UnprojectPixel) for the sun-centred one —
// never a second call into PixelToWorldDir, which is what the picker itself calls.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

#include "gui/analysis_panel.hpp"  // PreviewAnnotationView
#include "gui/angular_dist_picker.hpp"
#include "gui/angular_dist_rules.hpp"
#include "gui/annotation_anchors.hpp"  // GuiSunWorldDir
#include "gui/gui_constants.hpp"
#include "gui/gui_state.hpp"
#include "gui/overlay_labels.hpp"
#include "gui/preview_renderer.hpp"
#include "include/lumice.h"

namespace lumice::gui {
namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kRad2Deg = 180.0f / kPi;
constexpr int kW = 200;
constexpr int kH = 100;

std::optional<float> AngleAt(const GuiState& state, AngularDistFamily family, int px, int py) {
  return AngularDistAtCanvasPixel(family, PreviewAnnotationView(state, kW, kH),
                                  BuildPreviewViewProjFromRenderer(state.renderer), state.sun.altitude, px, py);
}

// The pixel's centre, in the shader's centre-origin y-up coordinates.
void ShaderPos(int px, int py, float* x, float* y) {
  *x = static_cast<float>(px) + 0.5f - kW * 0.5f;
  *y = kH * 0.5f - (static_cast<float>(py) + 0.5f);
}

// Core's direction at the pixel, and whether it is sky there.
bool CoreDir(const GuiState& state, int px, int py, float out[3]) {
  const LUMICE_AnnotationView view = PreviewAnnotationView(state, kW, kH);
  int valid = 0;
  EXPECT_EQ(LUMICE_UnprojectPixel(&view, px, py, out, &valid), LUMICE_OK);
  return valid != 0;
}

// Whether the shader mirror (the lens's projection domain alone) images the pixel.
bool MirrorImages(const GuiState& state, int px, int py) {
  const ViewProjection vp = BuildPreviewViewProjFromRenderer(state.renderer);
  float vm[9];
  BuildViewMatrix(vp.elevation, vp.azimuth, vp.roll, vm);
  float sx = 0.0f;
  float sy = 0.0f;
  ShaderPos(px, py, &sx, &sy);
  float d[3];
  bool valid = false;
  detail::PixelToWorldDirForTesting(sx, sy, kW, kH, vp.lens_type, vp.fov, vm, &d[0], &d[1], &d[2], &valid);
  return valid;
}

float AngleDeg(const float a[3], const float b[3]) {
  const float dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  return std::acos(std::max(-1.0f, std::min(1.0f, dot))) * kRad2Deg;
}

GuiState LinearState() {
  GuiState state;
  state.renderer.lens_type = kLensTypeLinear;
  state.renderer.fov = 60.0f;
  state.renderer.elevation = 20.0f;
  state.renderer.azimuth = 35.0f;
  state.renderer.roll = 10.0f;
  state.renderer.visible = kVisibleFull;
  state.renderer.front = false;
  state.sun.altitude = 25.0f;
  return state;
}

// ---- the radius: axis family against closed-form lens formulas ----

// Linear: the angle off the optical axis is atan(r / f), f = (short edge / 2) / tan(fov / 2) —
// whatever the camera's orientation, since the axis turns with it.
TEST(AngularDistPicker, AxisFamilyOnALinearLensIsTheRectilinearOffAxisAngle) {
  const GuiState state = LinearState();
  const float focal = std::min(kW, kH) * 0.5f / std::tan(state.renderer.fov * 0.5f / kRad2Deg);
  for (const auto& [px, py] : std::vector<std::pair<int, int>>{ { 100, 50 }, { 130, 40 }, { 10, 90 }, { 199, 0 } }) {
    float sx = 0.0f;
    float sy = 0.0f;
    ShaderPos(px, py, &sx, &sy);
    const float expect = std::atan(std::hypot(sx, sy) / focal) * kRad2Deg;
    const std::optional<float> got = AngleAt(state, AngularDistFamily::kView, px, py);
    EXPECT_TRUE(got.has_value()) << px << "," << py;
    EXPECT_NEAR(got.value_or(-1.0f), expect, 0.01f) << px << "," << py;
  }
}

// Equidistant fisheye: theta = (r / image radius) * fov / 2.
TEST(AngularDistPicker, AxisFamilyOnAnEquidistantFisheyeIsLinearInRadius) {
  GuiState state = LinearState();
  state.renderer.lens_type = kLensTypeFisheyeEquidist;
  state.renderer.fov = 180.0f;
  const float img_radius = std::min(kW, kH) * 0.5f;
  for (const auto& [px, py] : std::vector<std::pair<int, int>>{ { 100, 50 }, { 120, 30 }, { 60, 60 }, { 145, 50 } }) {
    float sx = 0.0f;
    float sy = 0.0f;
    ShaderPos(px, py, &sx, &sy);
    const float expect = std::hypot(sx, sy) / img_radius * 90.0f;
    const std::optional<float> got = AngleAt(state, AngularDistFamily::kView, px, py);
    EXPECT_TRUE(got.has_value()) << px << "," << py;
    EXPECT_NEAR(got.value_or(-1.0f), expect, 0.01f) << px << "," << py;
  }
}

// ---- the radius: sun family against core's inverse ----
//
// Core's LUMICE_UnprojectPixel is a separate implementation of the same lens inverse, held to the
// shader mirror by the parity gates; 0.05 degrees is well inside what those gates allow and far
// below anything the list's %.1f display could show.
void ExpectSunFamilyMatchesCore(const GuiState& state, const std::vector<std::pair<int, int>>& pixels) {
  float sun[3];
  GuiSunWorldDir(state.sun.altitude, sun);
  for (const auto& [px, py] : pixels) {
    float core[3];
    const bool core_sky = CoreDir(state, px, py, core);
    EXPECT_TRUE(core_sky) << "pixel " << px << "," << py << " is meant to be sky";
    const std::optional<float> got = AngleAt(state, AngularDistFamily::kSun, px, py);
    EXPECT_TRUE(got.has_value()) << px << "," << py;
    if (!core_sky || !got.has_value()) {
      continue;
    }
    EXPECT_NEAR(*got, AngleDeg(core, sun), 0.05f)
        << "lens " << state.renderer.lens_type << " pixel " << px << "," << py;
  }
}

TEST(AngularDistPicker, SunFamilyMatchesCoreAcrossLensFamilies) {
  GuiState linear = LinearState();
  ExpectSunFamilyMatchesCore(linear, { { 100, 50 }, { 20, 20 }, { 180, 80 } });

  GuiState fisheye = LinearState();
  fisheye.renderer.lens_type = kLensTypeFisheyeEqualArea;
  fisheye.renderer.fov = 180.0f;
  ExpectSunFamilyMatchesCore(fisheye, { { 100, 50 }, { 120, 20 }, { 70, 70 } });

  // Globe: the sphere seen from outside; the camera's own angles still apply.
  GuiState globe = LinearState();
  globe.renderer.lens_type = kLensTypeGlobe;
  globe.renderer.fov = 60.0f;
  ExpectSunFamilyMatchesCore(globe, { { 100, 50 }, { 110, 40 } });

  // Full-sky lenses: world-space pictures (no view matrix), one disc per hemisphere / one strip.
  GuiState dual = LinearState();
  dual.renderer.lens_type = kLensTypeDualFisheyeEqualArea;
  ExpectSunFamilyMatchesCore(dual, { { 50, 50 }, { 150, 50 }, { 40, 30 }, { 160, 70 } });

  GuiState rect = LinearState();
  rect.renderer.lens_type = kLensTypeRectangular;
  ExpectSunFamilyMatchesCore(rect, { { 100, 50 }, { 10, 10 }, { 190, 90 } });
}

// The ring through the sun's own position has radius ~0: the sun direction's pixel is found by
// core's forward, independently of the inverse under test.
TEST(AngularDistPicker, SunFamilyReadsZeroAtTheSun) {
  GuiState state = LinearState();
  state.renderer.elevation = 25.0f;
  const LUMICE_AnnotationView view = PreviewAnnotationView(state, kW, kH);
  float sun[3];
  GuiSunWorldDir(state.sun.altitude, sun);
  float fx = 0.0f;
  float fy = 0.0f;
  int valid = 0;
  ASSERT_EQ(LUMICE_ProjectDirection(&view, sun, &fx, &fy, &valid), LUMICE_OK);
  ASSERT_EQ(valid, 1);
  const std::optional<float> got =
      AngleAt(state, AngularDistFamily::kSun, static_cast<int>(std::floor(fx)), static_cast<int>(std::floor(fy)));
  ASSERT_TRUE(got.has_value());
  // One canvas pixel at fov 60 over a 100-pixel short edge is ~0.6 degrees.
  EXPECT_LT(*got, 1.0f);
}

// ---- pixels that are not pickable ----

TEST(AngularDistPicker, OffTheCanvasIsNotPickable) {
  const GuiState state = LinearState();
  EXPECT_FALSE(AngleAt(state, AngularDistFamily::kSun, -1, 50).has_value());
  EXPECT_FALSE(AngleAt(state, AngularDistFamily::kSun, kW, 50).has_value());
  EXPECT_FALSE(AngleAt(state, AngularDistFamily::kView, 100, kH).has_value());
}

// Outside the lens's image: the equal-area fisheye's circle, the globe's sphere, the dual
// fisheye's two discs. Each of these the mirror also rejects.
TEST(AngularDistPicker, OutsideTheLensImageIsNotPickable) {
  GuiState fisheye = LinearState();
  fisheye.renderer.lens_type = kLensTypeFisheyeEqualArea;
  fisheye.renderer.fov = 180.0f;
  EXPECT_FALSE(AngleAt(fisheye, AngularDistFamily::kView, 5, 5).has_value());
  EXPECT_FALSE(AngleAt(fisheye, AngularDistFamily::kSun, 195, 95).has_value());

  GuiState globe = LinearState();
  globe.renderer.lens_type = kLensTypeGlobe;
  globe.renderer.fov = 60.0f;
  EXPECT_FALSE(AngleAt(globe, AngularDistFamily::kSun, 2, 2).has_value());

  GuiState dual = LinearState();
  dual.renderer.lens_type = kLensTypeDualFisheyeEqualArea;
  EXPECT_FALSE(AngleAt(dual, AngularDistFamily::kSun, 100, 2).has_value()) << "between the two discs, at the top";
  EXPECT_FALSE(AngleAt(dual, AngularDistFamily::kView, 2, 2).has_value());
}

// The pixels `visible` and `front` hide are a verdict only core's inverse gives: the shader mirror
// images them. So the proposition worth pinning is "mirror images it, core says not sky ⇒ not
// pickable", and it must actually OCCUR in the scene, or the scan proves nothing.
void ExpectClippedPixelsAreNotPickable(const GuiState& state, const char* what) {
  int clipped = 0;
  int sky = 0;
  for (int py = 0; py < kH; py += 3) {
    for (int px = 0; px < kW; px += 3) {
      float core[3];
      const bool core_sky = CoreDir(state, px, py, core);
      const bool mirror = MirrorImages(state, px, py);
      const std::optional<float> got = AngleAt(state, AngularDistFamily::kSun, px, py);
      if (mirror && !core_sky) {
        ++clipped;
        EXPECT_FALSE(got.has_value()) << what << ": clipped pixel " << px << "," << py << " was pickable";
      }
      if (mirror && core_sky) {
        ++sky;
        EXPECT_TRUE(got.has_value()) << what << ": sky pixel " << px << "," << py << " was not pickable";
      }
    }
  }
  EXPECT_GT(clipped, 50) << what << ": the scene clips nothing, so this scan tested nothing";
  EXPECT_GT(sky, 50) << what << ": the scene shows no sky";
}

TEST(AngularDistPicker, TheSideVisibleHidesIsNotPickable) {
  GuiState state = LinearState();
  state.renderer.elevation = 0.0f;  // the horizon across the middle of the frame
  state.renderer.roll = 0.0f;
  state.renderer.visible = kVisibleUpper;
  ExpectClippedPixelsAreNotPickable(state, "linear, visible upper");
}

TEST(AngularDistPicker, TheHemisphereFrontHidesIsNotPickable) {
  // `front` applies only under a lens that can image behind the camera and is not full-sky or the
  // globe (EffectiveFrontForLens): an equal-area fisheye wider than 180 degrees is such a lens.
  GuiState state = LinearState();
  state.renderer.lens_type = kLensTypeFisheyeEqualArea;
  state.renderer.fov = 300.0f;
  state.renderer.front = true;
  ExpectClippedPixelsAreNotPickable(state, "equal-area fisheye 300, front");
}

// ---- the writes ----

TEST(AngularDistPicker, CommitFollowsTheEditorsAddRules) {
  std::vector<float> angles = { 22.0f, 46.0f };
  EXPECT_TRUE(CommitAngularDistPick(angles, 31.37f));
  EXPECT_EQ(angles, (std::vector<float>{ 22.0f, 31.37f, 46.0f })) << "unrounded, and the list stays sorted";

  EXPECT_FALSE(CommitAngularDistPick(angles, 22.005f)) << "within the duplicate epsilon of 22";
  EXPECT_EQ(angles.size(), 3u);

  EXPECT_TRUE(CommitAngularDistPick(angles, 0.01f));
  EXPECT_FLOAT_EQ(angles.front(), 0.1f) << "clamped into the editor's band";

  std::vector<float> full;
  for (int i = 0; i < kMaxAnnotationCircles; ++i) {
    full.push_back(1.0f + static_cast<float>(i));
  }
  EXPECT_FALSE(CommitAngularDistPick(full, 100.0f)) << "nothing is added past the cap";
  EXPECT_EQ(static_cast<int>(full.size()), kMaxAnnotationCircles);
}

TEST(AngularDistPicker, PreviewInjectionTouchesTheFramesCopyAndOnlyTheArmedFamily) {
  OverlayDecoration ov;
  ov.angular_dist_deg = { 22.0f };
  ov.view_dist_deg = { 46.0f };
  InjectAngularDistPickPreview(AngularDistFamily::kSun, 30.0f, ov);
  EXPECT_EQ(ov.angular_dist_deg, (std::vector<float>{ 22.0f, 30.0f }));
  EXPECT_TRUE(ov.show_sun_circles) << "the ring is drawn whatever the line switch says";
  EXPECT_EQ(ov.view_dist_deg, (std::vector<float>{ 46.0f }));
  EXPECT_FALSE(ov.show_view_dist);

  OverlayDecoration ov2;
  InjectAngularDistPickPreview(AngularDistFamily::kView, 12.5f, ov2);
  EXPECT_EQ(ov2.view_dist_deg, (std::vector<float>{ 12.5f }));
  EXPECT_TRUE(ov2.show_view_dist);
  EXPECT_TRUE(ov2.angular_dist_deg.empty());
}

TEST(AngularDistPicker, ArmingSwitchesTheFamilysLineOnAndDisarmsTheAnalysisPick) {
  GuiState state;
  state.show_sun_circles_line = false;
  state.show_sun_circles_label = false;
  state.show_view_dist_line = false;
  state.analysis.pick_armed = true;
  ArmAngularDistPicker(state, AngularDistFamily::kSun);
  EXPECT_TRUE(state.angular_dist_picker.armed);
  EXPECT_EQ(state.angular_dist_picker.family, AngularDistFamily::kSun);
  EXPECT_TRUE(state.show_sun_circles_line);
  EXPECT_FALSE(state.show_sun_circles_label) << "the label switch is the user's alone";
  EXPECT_FALSE(state.show_view_dist_line) << "the other family is left alone";
  EXPECT_FALSE(state.analysis.pick_armed);
  EXPECT_EQ(&AngularDistFamilyAngles(state, AngularDistFamily::kSun), &state.sun_circle_angles);
  EXPECT_EQ(&AngularDistFamilyAngles(state, AngularDistFamily::kView), &state.view_dist_angles);
}

TEST(AngularDistPicker, ReadoutUsesTheListsFormat) {
  EXPECT_EQ(FormatAngularDistPickReadout(22.04f), "22.0\xc2\xb0");
  EXPECT_EQ(FormatAngularDistPickReadout(31.37f), "31.4\xc2\xb0");
}

}  // namespace
}  // namespace lumice::gui
