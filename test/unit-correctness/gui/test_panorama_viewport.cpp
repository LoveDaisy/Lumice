// The Analyze workspace's panorama publish conversion (src/gui/panorama_viewport.hpp).
//
// The proposition this file exists for is a red/green pair on ONE defect: with
// ImGuiConfigFlags_ViewportsEnable the panorama child window's GetWindowPos() is an OS-screen
// absolute coordinate, and the publish site used to hand it to a GL viewport — which counts in
// main-window-local pixels — without subtracting the main viewport's origin first. The symptom
// is the sky picture sliding inside the child window whenever the OS window moves (and sitting
// offset whenever it rests away from the screen's top-left corner), while the ROI ring and the
// pick crosshair, drawn by ImGui's own projection, stay pinned. No ImGui-context test can see
// that input: the gui_test harness cannot enable ViewportsEnable, and a hidden GLFW window pins
// the main viewport's origin to (0, 0) — so the nail feeds the pure function a nonzero origin
// directly.
//
// The expectations below are hand-computed constants, not recomputations of the function's
// formula — a test that restates its implementation would pass through the very defect it
// exists to catch. The main case asserts the localized placement (red before the fix, green
// after); the translation-invariance case asserts the same property in algebraic form: moving
// the OS window translates the rect and the origin together, and the output must not move.
// Two further cases pin what the fix must NOT change: the (0, 0) degenerate — multi-viewport
// off, the shape every existing document and every committed reference image exercises — must
// reproduce the legacy publication bitwise, and the fractional-DPI case holds the truncation
// and the bottom-up Y convention at values a hand calculation can carry exactly (dyadic
// rationals only, no 0.1s).
//
// Header-only and ImGui-free, hence unit_correctness_test rather than gui_unit_test.

#include <gtest/gtest.h>

#include "gui/panorama_viewport.hpp"

namespace {

using lumice::gui::PanoramaChildRectToGlViewport;
using lumice::gui::PanoramaGlViewport;
using lumice::gui::PanoramaScreenPoint;
using lumice::gui::PanoramaScreenRect;
using lumice::gui::PanoramaSurface;

// A 2x display: DisplaySize (1280, 720) points over a (2560, 1440) framebuffer, so every
// product below is an exact dyadic and the hand-computed expectations carry no rounding doubt.
PanoramaSurface HiDpiSurface() {
  PanoramaSurface s;
  s.display_w = 1280.0f;
  s.display_h = 720.0f;
  s.fb_w = 2560;
  s.fb_h = 1440;
  return s;
}

}  // namespace

// The red/green carrier. The OS window rests 100 pt right and 50 pt down of the screen's
// top-left corner; the child window sits at (150, 80) absolute — i.e. (50, 30) window-local —
// at 400x300 pt on the 2x surface. By hand: x = 50*2 = 100, y = (720 - (30+300))*2 = 780
// (GL bottom-up), w = 800, h = 600.
TEST(PanoramaViewport, NonzeroOriginPlacesRectAtWindowLocalPixels) {
  PanoramaScreenRect rect;
  rect.x = 150.0f;
  rect.y = 80.0f;
  rect.w = 400.0f;
  rect.h = 300.0f;
  PanoramaScreenPoint origin;
  origin.x = 100.0f;
  origin.y = 50.0f;

  const PanoramaGlViewport vp = PanoramaChildRectToGlViewport(rect, origin, HiDpiSurface());
  EXPECT_EQ(vp.vp_x, 100);
  EXPECT_EQ(vp.vp_y, 780);
  EXPECT_EQ(vp.vp_w, 800);
  EXPECT_EQ(vp.vp_h, 600);
  EXPECT_EQ(vp.dpi_scale_x, 2.0f);
  EXPECT_EQ(vp.dpi_scale_y, 2.0f);
}

// The same proposition in the algebraic form the defect violates most directly: dragging the
// OS window translates the child rect and the main viewport's origin TOGETHER — the
// window-local geometry is untouched — so the published viewport must not move by a pixel.
// Deliberately NOT the "same rect, different origin" pairing: under the pre-fix formula (which
// reads only the rect) that pairing holds vacuously while the defect is live and breaks once
// fixed — the mirror image of a pin. Both rect and origin shift by (200, 100) below.
TEST(PanoramaViewport, WindowMoveTranslatesRectAndOriginTogetherAndViewportStays) {
  PanoramaScreenRect rect;
  rect.x = 150.0f;
  rect.y = 80.0f;
  rect.w = 400.0f;
  rect.h = 300.0f;
  PanoramaScreenPoint origin;
  origin.x = 100.0f;
  origin.y = 50.0f;

  PanoramaScreenRect moved_rect = rect;
  moved_rect.x += 200.0f;
  moved_rect.y += 100.0f;
  PanoramaScreenPoint moved_origin = origin;
  moved_origin.x += 200.0f;
  moved_origin.y += 100.0f;

  const PanoramaGlViewport before = PanoramaChildRectToGlViewport(rect, origin, HiDpiSurface());
  const PanoramaGlViewport after = PanoramaChildRectToGlViewport(moved_rect, moved_origin, HiDpiSurface());
  EXPECT_EQ(after.vp_x, before.vp_x);
  EXPECT_EQ(after.vp_y, before.vp_y);
  EXPECT_EQ(after.vp_w, before.vp_w);
  EXPECT_EQ(after.vp_h, before.vp_h);
  EXPECT_EQ(after.dpi_scale_x, before.dpi_scale_x);
  EXPECT_EQ(after.dpi_scale_y, before.dpi_scale_y);
}

// Degenerate input, legacy contract: with the main viewport at (0, 0) — multi-viewport off —
// the conversion must reproduce the pre-extraction publication bitwise, since that is the shape
// every existing document and every committed reference image exercises. Hand-computed on the
// same 2x surface: x = 150*2 = 300, y = (720 - (80+300))*2 = 680.
TEST(PanoramaViewport, ZeroOriginReproducesLegacyPublicationBitwise) {
  PanoramaScreenRect rect;
  rect.x = 150.0f;
  rect.y = 80.0f;
  rect.w = 400.0f;
  rect.h = 300.0f;
  PanoramaScreenPoint origin;  // (0, 0): the defaults

  const PanoramaGlViewport vp = PanoramaChildRectToGlViewport(rect, origin, HiDpiSurface());
  EXPECT_EQ(vp.vp_x, 300);
  EXPECT_EQ(vp.vp_y, 680);
  EXPECT_EQ(vp.vp_w, 800);
  EXPECT_EQ(vp.vp_h, 600);
  EXPECT_EQ(vp.dpi_scale_x, 2.0f);
  EXPECT_EQ(vp.dpi_scale_y, 2.0f);
}

// The semantics the fix must leave alone, at values a hand calculation carries exactly: a
// non-uniform DPI (fb 1000x675 over DisplaySize 800x600 -> 1.25 x 1.125, both dyadic), a
// fractional rect, and the truncate-toward-zero each cast performs. x = 40.5*1.25 = 50.625 ->
// 50; y = (600 - (30.25+96.5))*1.125 = 532.40625 -> 532 (the bottom-up flip, pinned on a
// fractional case); w = 128.5*1.25 = 160.625 -> 160; h = 96.5*1.125 = 108.5625 -> 108.
TEST(PanoramaViewport, FractionalDpiTruncationAndBottomUpYStayAsTheyWere) {
  PanoramaSurface s;
  s.display_w = 800.0f;
  s.display_h = 600.0f;
  s.fb_w = 1000;
  s.fb_h = 675;
  PanoramaScreenRect rect;
  rect.x = 40.5f;
  rect.y = 30.25f;
  rect.w = 128.5f;
  rect.h = 96.5f;
  PanoramaScreenPoint origin;  // (0, 0): isolate the rounding/Y semantics from the origin axis

  const PanoramaGlViewport vp = PanoramaChildRectToGlViewport(rect, origin, s);
  EXPECT_EQ(vp.vp_x, 50);
  EXPECT_EQ(vp.vp_y, 532);
  EXPECT_EQ(vp.vp_w, 160);
  EXPECT_EQ(vp.vp_h, 108);
  EXPECT_EQ(vp.dpi_scale_x, 1.25f);
  EXPECT_EQ(vp.dpi_scale_y, 1.125f);
}
