// The Angular Distance picker, driven through the real panel and the real preview.
//
// What this suite is for. The radius at a pixel, which pixels are pickable and the writes a pick
// makes are pure and are asserted in unit-correctness/gui/test_angular_dist_picker.cpp. What that
// file cannot see is the mode itself: a Pick button that has to be reachable and correctly
// disabled, a candidate ring that has to reach the PUBLISHED PreviewParams (the frame the shader
// draws) and nothing else, a click that has to add exactly that radius, two ways to cancel, and the
// camera gestures that have to stop while it is armed and stay stopped for the rest of the press
// that confirmed it. All of that needs a live frame, so it lives here.
//
// The expected radius is never read back from the code under test. For the axis family on a
// linear lens it is the closed-form off-axis angle of the cursor's canvas pixel; for the sun
// family it is measured through core's inverse (LUMICE_UnprojectPixel), a separate implementation
// of the lens math from the shader mirror the picker measures with — hence a tolerance, 0.05 deg,
// far below the 0.1 deg the list prints to.
//
// No reference image: every proposition is a widget outcome or a published parameter, so this is
// `functional/`, not `visual/`.

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

#include "IconsFontAwesome6.h"
#include "gui/analysis_panel.hpp"      // PreviewAnnotationView, CanvasPixel
#include "gui/annotation_anchors.hpp"  // GuiSunWorldDir
#include "gui/app.hpp"
#include "gui/gui_constants.hpp"
#include "gui/gui_state.hpp"
#include "include/lumice.h"
#include "test_gui_shared.hpp"

namespace {

// The full labels, icon glyph included: the engine derives the id from the whole string.
const char* const kPickButton = "**/" ICON_FA_CROSSHAIRS "##pick_circle";
const char* const kSunFold = "**/###sun_circles_fold";
const char* const kViewFold = "**/###view_dist_fold";

constexpr float kRad2Deg = 57.29577951308232f;
constexpr float kAngleTolDeg = 0.05f;

// A picture for the preview to draw: a small shown background is enough — the panel's picture
// branch (overlays, gestures, and so the picker) runs whenever a texture or a background is up.
// Uploaded from GuiFunc because a TestFunc runs on the engine's coroutine, the wrong thread for GL.
struct ProbeBg {
  bool requested = false;
  bool done = false;
};
ProbeBg g_probe;

void ProbeGuiFunc(ImGuiTestContext* /*ctx*/) {
  if (g_probe.requested && !g_probe.done) {
    const std::vector<unsigned char> rgb(4 * 4 * 3, 40);
    gui::g_preview.UploadBgTexture(rgb.data(), 4, 4);
    g_probe.done = true;
  }
}

// Reset, then put a picture up and a known camera in place. Nothing is simulated.
void InstallPicture(ImGuiTestContext* ctx, int lens_type, float fov) {
  ResetTestState();
  ctx->Yield(2);
  g_probe = ProbeBg{};
  g_probe.requested = true;
  ctx->Yield(3);
  IM_CHECK(g_probe.done);
  gui::g_state.bg_show = true;
  gui::g_state.bg_alpha = 0.0f;
  auto& rc = gui::g_state.renderer;
  rc.lens_type = lens_type;
  rc.fov = fov;
  rc.elevation = 10.0f;
  rc.azimuth = 20.0f;
  rc.roll = 0.0f;
  rc.visible = gui::kVisibleFull;
  rc.front = false;
  gui::g_state.sun.altitude = 25.0f;
  gui::g_state.sun_circle_angles = { 22.0f };
  gui::g_state.view_dist_angles = { 46.0f };
  gui::g_state.show_sun_circles_line = false;
  gui::g_state.show_view_dist_line = false;
  ctx->Yield(3);
  IM_CHECK(gui::g_preview_vp.active);
}

// Open the family's angle editor and press its Pick button. The fold is addressed under the right
// panel, the Pick button in the popup — a different window — so the ref is only set for the fold.
void ArmThroughTheButton(ImGuiTestContext* ctx, const char* fold) {
  gui::g_state.angular_dist_section_open = true;
  ctx->Yield(3);
  ctx->SetRef("//##RightPanel");
  ctx->ItemClick(fold);
  ctx->SetRef("");
  ctx->Yield(3);
  ctx->ItemClick(kPickButton);
  ctx->Yield(2);
}

ImVec2 PreviewOrigin(ImGuiTestContext* ctx) {
  ImGuiWindow* w = ctx->GetWindowByRef("//##PreviewPanel");
  IM_CHECK_SILENT_RETV(w != nullptr, ImVec2(0, 0));
  return w->Pos;
}

// The screen point at the centre of canvas pixel (px, py), and the canvas pixel under the mouse:
// the panel's own window-origin / DPI relation (see PreviewPointToCanvasPixel).
ImVec2 CanvasPixelScreenPos(ImGuiTestContext* ctx, int px, int py) {
  const ImVec2 o = PreviewOrigin(ctx);
  return ImVec2(o.x + (static_cast<float>(px) + 0.5f) / gui::g_preview_vp.dpi_scale_x,
                o.y + (static_cast<float>(py) + 0.5f) / gui::g_preview_vp.dpi_scale_y);
}

std::optional<gui::CanvasPixel> MouseCanvasPixel(ImGuiTestContext* ctx) {
  const ImVec2 o = PreviewOrigin(ctx);
  const ImVec2 m = ImGui::GetIO().MousePos;
  return gui::PreviewPointToCanvasPixel(m.x - o.x, m.y - o.y, gui::g_preview_vp.dpi_scale_x,
                                        gui::g_preview_vp.dpi_scale_y, gui::g_preview_vp.vp_w, gui::g_preview_vp.vp_h);
}

// Closed form: on a linear lens the angle off the optical axis at a pixel's centre is atan(r / f),
// f = (short edge / 2) / tan(fov / 2), whatever the camera's orientation.
float LinearOffAxisDeg(const gui::CanvasPixel& p) {
  const float w = static_cast<float>(gui::g_preview_vp.vp_w);
  const float h = static_cast<float>(gui::g_preview_vp.vp_h);
  const float focal = std::min(w, h) * 0.5f / std::tan(gui::g_state.renderer.fov * 0.5f / kRad2Deg);
  const float x = static_cast<float>(p.px) + 0.5f - w * 0.5f;
  const float y = h * 0.5f - (static_cast<float>(p.py) + 0.5f);
  return std::atan(std::hypot(x, y) / focal) * kRad2Deg;
}

// Core's inverse at the pixel, then the angle to the sun; nullopt when core says it is not sky.
std::optional<float> SunAngleByCore(const gui::CanvasPixel& p) {
  const LUMICE_AnnotationView view =
      gui::PreviewAnnotationView(gui::g_state, gui::g_preview_vp.vp_w, gui::g_preview_vp.vp_h);
  float d[3] = { 0.0f, 0.0f, 0.0f };
  int valid = 0;
  if (LUMICE_UnprojectPixel(&view, p.px, p.py, d, &valid) != LUMICE_OK || valid == 0) {
    return std::nullopt;
  }
  float s[3];
  gui::GuiSunWorldDir(gui::g_state.sun.altitude, s);
  const float dot = std::max(-1.0f, std::min(1.0f, d[0] * s[0] + d[1] * s[1] + d[2] * s[2]));
  return std::acos(dot) * kRad2Deg;
}

// Move to canvas pixel (px, py) and let the panel publish a frame there.
std::optional<gui::CanvasPixel> HoverCanvasPixel(ImGuiTestContext* ctx, int px, int py) {
  ctx->MouseMoveToPos(CanvasPixelScreenPos(ctx, px, py));
  ctx->Yield(2);
  return MouseCanvasPixel(ctx);
}

// Pick on the sun family at canvas pixel (px, py), which must be sky: the published candidate and
// the added radius both agree with core's measurement there.
void ExpectSunPickAddsCoresRadius(ImGuiTestContext* ctx, int px, int py) {
  ArmThroughTheButton(ctx, kSunFold);
  IM_CHECK(gui::g_state.angular_dist_picker.armed);
  const std::optional<gui::CanvasPixel> at = HoverCanvasPixel(ctx, px, py);
  IM_CHECK(at.has_value());
  const std::optional<float> want = SunAngleByCore(*at);
  IM_CHECK(want.has_value());
  const std::vector<float>& published = gui::g_preview_vp.params.overlay.angular_dist_deg;
  IM_CHECK_EQ(published.size(), (size_t)2);
  IM_CHECK_LT(ImFabs(published.back() - *want), kAngleTolDeg);
  IM_CHECK(gui::g_preview_vp.params.overlay.show_sun_circles);

  ctx->MouseClick(0);
  ctx->Yield(2);
  IM_CHECK(!gui::g_state.angular_dist_picker.armed);
  const std::vector<float>& list = gui::g_state.sun_circle_angles;
  IM_CHECK_EQ(list.size(), (size_t)2);
  const float added = ImFabs(list[0] - 22.0f) < 1e-4f ? list[1] : list[0];
  IM_CHECK_LT(ImFabs(added - *want), kAngleTolDeg);
}

}  // namespace

void RegisterAngularDistPickerTests(ImGuiTestEngine* engine) {
  // The button: greyed with no picture to point at; with one, it arms the picker for its own row's
  // family, switches that family's line on, and closes the editor so the next click is the
  // preview's.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "angular_dist_picker", "button_needs_a_picture_and_arms_its_family");
    t->GuiFunc = ProbeGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      const ScopedPopups popup_guard(ctx);
      ResetTestState();
      gui::g_state.angular_dist_section_open = true;
      ctx->Yield(3);
      IM_CHECK(!gui::g_preview_vp.active);
      ctx->SetRef("//##RightPanel");
      ctx->ItemClick(kSunFold);
      ctx->SetRef("");
      ctx->Yield(3);
      IM_CHECK(IsDisabled(ctx->ItemInfo(kPickButton)));
      ctx->KeyPress(ImGuiKey_Escape);
      ctx->Yield(2);

      InstallPicture(ctx, gui::kLensTypeLinear, 90.0f);
      ArmThroughTheButton(ctx, kViewFold);
      IM_CHECK(gui::g_state.angular_dist_picker.armed);
      IM_CHECK(gui::g_state.angular_dist_picker.family == gui::AngularDistFamily::kView);
      IM_CHECK(gui::g_state.show_view_dist_line);
      IM_CHECK(!gui::g_state.show_sun_circles_line);
      IM_CHECK(!ctx->ItemExists(kPickButton));  // the editor closed
    };
  }

  // plan Step 4/6 promised this half too: at the cap, greyed even with a picture up — with
  // PreviewShowsPicture() satisfied, at_limit is the only reason left the button could be disabled
  // for, so this isolates that branch from the no-picture one the test above already covers.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "angular_dist_picker", "a_full_list_greys_the_pick_button");
    t->GuiFunc = ProbeGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      const ScopedPopups popup_guard(ctx);
      InstallPicture(ctx, gui::kLensTypeLinear, 90.0f);
      gui::g_state.sun_circle_angles.clear();
      for (int i = 0; i < gui::kMaxAnnotationCircles; ++i) {
        gui::g_state.sun_circle_angles.push_back(5.0f + static_cast<float>(i) * 5.0f);
      }
      gui::g_state.angular_dist_section_open = true;
      ctx->Yield(3);
      ctx->SetRef("//##RightPanel");
      ctx->ItemClick(kSunFold);
      ctx->SetRef("");
      ctx->Yield(3);
      IM_CHECK(IsDisabled(ctx->ItemInfo(kPickButton)));
      ctx->KeyPress(ImGuiKey_Escape);
      ctx->Yield(2);
    };
  }

  // AC5: enter, move, click. The ring the frame publishes is the cursor's radius; the click adds
  // exactly that radius, disarms, and the rest of that same press does not orbit the camera.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "angular_dist_picker", "the_ring_follows_the_cursor_and_the_click_adds_its_radius");
    t->GuiFunc = ProbeGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      InstallPicture(ctx, gui::kLensTypeLinear, 90.0f);
      ArmThroughTheButton(ctx, kViewFold);
      IM_CHECK(gui::g_state.angular_dist_picker.armed);
      const int w = gui::g_preview_vp.vp_w;
      const int h = gui::g_preview_vp.vp_h;

      // Two positions, so "follows" is a claim about motion rather than about one frame.
      const std::optional<gui::CanvasPixel> a = HoverCanvasPixel(ctx, w * 3 / 10, h / 2);
      IM_CHECK(a.has_value());
      const std::vector<float>& published = gui::g_preview_vp.params.overlay.view_dist_deg;
      IM_CHECK_EQ(published.size(), (size_t)2);
      IM_CHECK_EQ(published.front(), 46.0f);
      IM_CHECK_LT(ImFabs(published.back() - LinearOffAxisDeg(*a)), kAngleTolDeg);
      IM_CHECK(gui::g_preview_vp.params.overlay.show_view_dist);
      IM_CHECK_EQ(gui::g_state.view_dist_angles.size(), (size_t)1);  // the document is untouched

      const std::optional<gui::CanvasPixel> b = HoverCanvasPixel(ctx, w * 2 / 5, h * 3 / 5);
      IM_CHECK(b.has_value());
      const float want = LinearOffAxisDeg(*b);
      IM_CHECK_GT(ImFabs(want - LinearOffAxisDeg(*a)), 1.0f);  // the two positions really differ
      IM_CHECK_EQ(gui::g_preview_vp.params.overlay.view_dist_deg.size(), (size_t)2);
      IM_CHECK_LT(ImFabs(gui::g_preview_vp.params.overlay.view_dist_deg.back() - want), kAngleTolDeg);

      const float az = gui::g_state.renderer.azimuth;
      const float el = gui::g_state.renderer.elevation;
      ctx->MouseDown(0);
      ctx->Yield(2);
      IM_CHECK(!gui::g_state.angular_dist_picker.armed);
      const std::vector<float>& list = gui::g_state.view_dist_angles;
      IM_CHECK_EQ(list.size(), (size_t)2);
      IM_CHECK_LT(ImFabs(list.front() - want), kAngleTolDeg);  // sorted: the new radius is < 46
      IM_CHECK_EQ(list.back(), 46.0f);
      // The rest of the press drags; the latch keeps it off the camera.
      ctx->MouseMoveToPos(CanvasPixelScreenPos(ctx, w / 2, h / 3));
      ctx->Yield(2);
      ctx->MouseUp(0);
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.renderer.azimuth, az);
      IM_CHECK_EQ(gui::g_state.renderer.elevation, el);
      // Disarmed: the ring is gone from the frame.
      IM_CHECK_EQ(gui::g_preview_vp.params.overlay.view_dist_deg.size(), (size_t)2);
      IM_CHECK_EQ(gui::g_preview_vp.params.overlay.view_dist_deg.back(), 46.0f);
    };
  }

  // Esc and a right click over the preview both cancel: the list is untouched, and the line switch
  // the Pick button turned on stays on — entering the mode set it, a cancel does not undo that.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "angular_dist_picker", "esc_and_right_click_cancel_leaving_the_list");
    t->GuiFunc = ProbeGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      InstallPicture(ctx, gui::kLensTypeLinear, 90.0f);
      const int w = gui::g_preview_vp.vp_w;
      const int h = gui::g_preview_vp.vp_h;

      ArmThroughTheButton(ctx, kSunFold);
      IM_CHECK(gui::g_state.angular_dist_picker.armed);
      IM_CHECK(HoverCanvasPixel(ctx, w * 2 / 5, h / 2).has_value());
      IM_CHECK_EQ(gui::g_preview_vp.params.overlay.angular_dist_deg.size(), (size_t)2);
      ctx->KeyPress(ImGuiKey_Escape);
      ctx->Yield(2);
      IM_CHECK(!gui::g_state.angular_dist_picker.armed);
      IM_CHECK_EQ(gui::g_state.sun_circle_angles.size(), (size_t)1);
      IM_CHECK_EQ(gui::g_preview_vp.params.overlay.angular_dist_deg.size(), (size_t)1);
      IM_CHECK(gui::g_state.show_sun_circles_line);

      ArmThroughTheButton(ctx, kSunFold);
      IM_CHECK(gui::g_state.angular_dist_picker.armed);
      IM_CHECK(HoverCanvasPixel(ctx, w * 2 / 5, h / 2).has_value());
      ctx->MouseClick(ImGuiMouseButton_Right);
      ctx->Yield(2);
      IM_CHECK(!gui::g_state.angular_dist_picker.armed);
      IM_CHECK_EQ(gui::g_state.sun_circle_angles.size(), (size_t)1);
      IM_CHECK_EQ(gui::g_state.sun_circle_angles.front(), 22.0f);
      IM_CHECK(gui::g_state.show_sun_circles_line);
    };
  }

  // AC3: off sky there is no ring and a click is inert — no ring added, the mode stays armed so the
  // user can aim again. Two kinds of off-sky: outside the lens's image (a fisheye's corner) and the
  // side `visible` hides. And while armed the camera takes no drag and no wheel — the drag done off
  // sky here, where it cannot also count as a pick.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "angular_dist_picker", "off_sky_shows_no_ring_and_camera_gestures_are_locked");
    t->GuiFunc = ProbeGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      // Orthographic, not equal-area: an equal-area fisheye goes on imaging (behind the camera) out
      // to 1.41 image radii, so its corners can still be sky; orthographic's domain ends at the
      // image circle.
      InstallPicture(ctx, gui::kLensTypeFisheyeOrthographic, 180.0f);
      const int w = gui::g_preview_vp.vp_w;
      const int h = gui::g_preview_vp.vp_h;
      ArmThroughTheButton(ctx, kSunFold);
      IM_CHECK(gui::g_state.angular_dist_picker.armed);

      // Bottom-left corner: outside the image circle (the top-right one holds the display-mode
      // control, which masks the preview).
      const std::optional<gui::CanvasPixel> corner = HoverCanvasPixel(ctx, 3, h - 4);
      IM_CHECK(corner.has_value());
      IM_CHECK(!SunAngleByCore(*corner).has_value());  // precondition: really off sky
      IM_CHECK_EQ(gui::g_preview_vp.params.overlay.angular_dist_deg.size(), (size_t)1);

      const float az = gui::g_state.renderer.azimuth;
      const float el = gui::g_state.renderer.elevation;
      const float fov = gui::g_state.renderer.fov;
      ctx->MouseDown(0);
      ctx->MouseMoveToPos(CanvasPixelScreenPos(ctx, 40, h - 40));
      ctx->Yield(2);
      ctx->MouseUp(0);
      ctx->Yield(2);
      ctx->MouseWheelY(1.0f);
      ctx->Yield(2);
      IM_CHECK(gui::g_state.angular_dist_picker.armed);
      IM_CHECK_EQ(gui::g_state.sun_circle_angles.size(), (size_t)1);
      IM_CHECK_EQ(gui::g_state.renderer.azimuth, az);
      IM_CHECK_EQ(gui::g_state.renderer.elevation, el);
      IM_CHECK_EQ(gui::g_state.renderer.fov, fov);

      // `visible` = upper on a level linear view: the lower half of the frame is ground.
      gui::g_state.renderer.lens_type = gui::kLensTypeLinear;
      gui::g_state.renderer.fov = 90.0f;
      gui::g_state.renderer.elevation = 0.0f;
      gui::g_state.renderer.visible = gui::kVisibleUpper;
      ctx->Yield(2);
      const std::optional<gui::CanvasPixel> ground = HoverCanvasPixel(ctx, w * 2 / 5, h * 4 / 5);
      IM_CHECK(ground.has_value());
      IM_CHECK(!SunAngleByCore(*ground).has_value());
      IM_CHECK_EQ(gui::g_preview_vp.params.overlay.angular_dist_deg.size(), (size_t)1);
      ctx->MouseClick(0);
      ctx->Yield(2);
      IM_CHECK(gui::g_state.angular_dist_picker.armed);
      IM_CHECK_EQ(gui::g_state.sun_circle_angles.size(), (size_t)1);
      // ...and the upper half is sky: the ring appears there (the positive control for the above).
      const std::optional<gui::CanvasPixel> sky = HoverCanvasPixel(ctx, w * 2 / 5, h / 5);
      IM_CHECK(sky.has_value());
      IM_CHECK(SunAngleByCore(*sky).has_value());
      IM_CHECK_EQ(gui::g_preview_vp.params.overlay.angular_dist_deg.size(), (size_t)2);
      ctx->KeyPress(ImGuiKey_Escape);
      ctx->Yield(2);
    };
  }

  // AC3: the sun family on the lenses whose pictures are not a plain view — the globe (the sphere
  // from outside) and the dual fisheye (two world-space discs) — picks where the picture is sky and
  // adds the radius core measures there.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "angular_dist_picker", "globe_and_dual_fisheye_pick_on_their_sky");
    t->GuiFunc = ProbeGuiFunc;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      InstallPicture(ctx, gui::kLensTypeGlobe, 60.0f);
      // A little off the sphere's centre: on the globe, and off any ring already drawn.
      ExpectSunPickAddsCoresRadius(ctx, gui::g_preview_vp.vp_w * 11 / 20, gui::g_preview_vp.vp_h * 9 / 20);
      if (ctx->IsError()) {
        return;  // the helper's own check already reported; the second lens would build on its state
      }

      InstallPicture(ctx, gui::kLensTypeDualFisheyeEqualArea, 360.0f);
      // The centre of the left disc (the discs are side by side, each half the width).
      ExpectSunPickAddsCoresRadius(ctx, gui::g_preview_vp.vp_w / 4, gui::g_preview_vp.vp_h / 2);
      ctx->Yield(1);
    };
  }
}
