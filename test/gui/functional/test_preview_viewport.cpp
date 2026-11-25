// The preview panel itself: the invariants it enforces every frame, and what a drag on it does.
//
// What this suite is for. `RenderPreviewPanel` (src/gui/app_panels.cpp) is the only place the
// renderer's pose is both displayed and edited, and it re-derives that pose on every frame rather
// than only on the frames a control was touched — which is why a lens whose projection has no
// notion of "which way am I facing" can zero the angles out from under a value the user set two
// frames ago. Nothing about that is visible without a real frame, and nothing about the drag is
// visible without a real input event.
//
// The drag's gain is the sharp one. It is not a constant: the same pixel distance has to turn the
// view by more when the field of view is wide and less when it is narrow, or a 1-degree telephoto
// view becomes unusable while a 179-degree view barely moves. It used to be a constant, and the
// cases below carry the shape of that bug — they source the expected magnitude from the same
// function the handler calls, and assert the SIGN, the WRAP and the CLAMP, which are the handler's
// own and not the gain law's.
//
// Deliberately NOT here, with where each lives instead. The gain law itself is asserted against the
// projection pipeline in unit-correctness/gui/test_preview_renderer.cpp; which view controls are
// greyed per lens is functional/test_view_display_controls.cpp; what the preview's pixels look like
// is visual/test_preview_pixels.cpp and the committed lens-projection reference group.
//
// What a user sees when these break: a view that snaps back to level whenever they let go, a drag
// that spins the sky at a speed unrelated to how zoomed in they are, or an azimuth that jams at
// due south instead of carrying on round.

#include <stb_image.h>

#include <cmath>
#include <cstdint>
#include <string>

#include "gui/file_io.hpp"
#include "gui/gui_state.hpp"
#include "gui/preview_renderer.hpp"
#include "gui/view_look_at.hpp"
#include "test_gui_shared.hpp"

namespace {

const char* const kInteract = "**/##preview_interact";

// The interaction surface is only submitted when there is something to interact with — a texture or
// a background image — so every drag case has to put one there first. The upload is a GL call and
// therefore has to happen on the main thread rather than in the test coroutine, which is what this
// GuiFunc is for.
bool g_upload_done = false;

void UploadSynthTexture(ImGuiTestContext*) {
  if (!g_upload_done) {
    InitSynthTexture();
    gui::g_preview.UploadTexture(g_synth_tex.data(), kSynthTexW, kSynthTexH);
    g_upload_done = true;
  }
}

// Degrees the handler should produce for a given pixel delta, read off the live viewport.
//
// Sourced from the same function the handler calls rather than restated as a literal: the literals
// these cases used to carry (18 degrees for a 60 px drag) were the old fixed 0.3 deg/px in
// disguise, which is why making the gain field-of-view-aware turned four of them red — they were
// pinning a constant none of them was about.
float ExpectedDeg(int lens_type, float fov, float pixels) {
  return pixels * gui::ComputeDragGainDegPerPixel(lens_type, fov, gui::g_preview_vp.vp_w, gui::g_preview_vp.vp_h);
}

void SeedPose(int lens_type, float fov, float az, float el) {
  gui::g_state.renderer.lens_type = lens_type;
  gui::g_state.renderer.fov = fov;
  gui::g_state.renderer.azimuth = az;
  gui::g_state.renderer.elevation = el;
  gui::g_state.renderer.roll = 0.0f;
}

// ---- the background pan/zoom gesture ------------------------------------------------------
//
// Alt is the modifier on every platform, and these cases are the reason it is. The gesture was
// first written to take Cmd on macOS, which reads as the native choice and cannot work: ImGui
// rewrites Super+LeftClick into a RIGHT click at the event queue (ConfigMacOSXBehaviors, on by
// default under __APPLE__), so the left drag never reaches the handler. Only a case that drove a
// real mac frame could say so — the pure function deciding WHICH key to read was correct, and the
// premise underneath it was false. Which is also why these live here and not beside the
// background's pixels: what is asserted is the ROUTING, that the modifier hands the canvas to
// exactly one of the two owners and the other sees nothing. A suite that only drove the
// background could not tell "the camera was left alone" from "the camera was never reachable".
constexpr ImGuiKey kBgMod = ImGuiMod_Alt;

// The background image the gesture cases push in, borrowed from the background suite's references.
// Landscape against the harness's landscape viewport, so the contain fit squeezes one axis and
// neither scale_x nor scale_y is 1 — an image whose aspect matched the viewport would let a
// transform that ignored the fit pass.
const char* const kBgFile = LUMICE_TEST_REF_DIR "/bg_test_landscape.jpg";

// Both uploads are GL calls, so they happen here rather than in the test coroutine. Same shape as
// UploadSynthTexture above; the render texture is still required, since a background alone would
// leave the export path (and, historically, several viewport invariants) on a different branch
// from the one every other case in this file exercises.
void UploadSynthTextureAndBackground(ImGuiTestContext*) {
  UploadSynthTexture(nullptr);
  if (g_bg_test.bg_upload_requested && !g_bg_test.bg_upload_done) {
    int w = 0;
    int h = 0;
    int channels = 0;
    unsigned char* raw = stbi_load(g_bg_test.bg_image_path.c_str(), &w, &h, &channels, 3);
    if (raw) {
      gui::g_preview.UploadBgTexture(raw, w, h);
      stbi_image_free(raw);
    }
    g_bg_test.bg_upload_done = true;
  }
}

// The context-invalidation case's document-reset leg. The kOpenBaked spelling swaps the preview
// texture INSIDE ResetFrontendState — a GL upload — so it runs here, on the main/GL thread, on a
// frame the TestFunc's held right press is still down. The swap is the whole point: the picture
// never leaves the screen, so nothing except the reset event itself can end the gesture.
struct RollResetRequest {
  bool requested = false;
  bool done = false;
};
RollResetRequest g_roll_reset;

void UploadSynthTextureAndMaybeReset(ImGuiTestContext* ctx) {
  UploadSynthTexture(ctx);
  if (g_roll_reset.requested && !g_roll_reset.done) {
    gui::LmcTexture tex;
    tex.width = 4;
    tex.height = 4;
    tex.mode = gui::PreviewRenderer::TextureMode::kSrgbComposited;
    tex.srgb.assign(4 * 4 * 3, 40);
    gui::ResetFrontendState(gui::g_state, gui::FrontendResetReason::kOpenBaked, &tex);
    g_roll_reset.done = true;
  }
}

// Put a render texture and a background in place and return once both have landed. `shown` is the
// bg_show flag rather than a second helper because "loaded but hidden" is one of the cases below.
void LoadBackground(ImGuiTestContext* ctx, bool shown) {
  g_upload_done = false;
  g_bg_test.Reset();
  g_bg_test.bg_image_path = kBgFile;
  g_bg_test.bg_upload_requested = true;
  ctx->Yield(4);
  IM_CHECK(g_bg_test.bg_upload_done);
  IM_CHECK(gui::g_preview.HasBackground());
  gui::g_state.bg_show = shown;
  ctx->Yield(2);
}

// One modifier-held drag on the canvas.
void ModDrag(ImGuiTestContext* ctx, ImVec2 delta) {
  ctx->KeyDown(kBgMod);
  ctx->ItemDragWithDelta(kInteract, delta);
  ctx->KeyUp(kBgMod);
  ctx->Yield(2);
}

// One modifier-held wheel notch over the canvas. The hover has to be re-established each time:
// the wheel is delivered to whatever the mouse is over, not to whatever was last dragged.
void ModWheel(ImGuiTestContext* ctx, float delta) {
  ctx->MouseMove(kInteract);
  ctx->KeyDown(kBgMod);
  ctx->MouseWheelY(delta);
  ctx->KeyUp(kBgMod);
  ctx->Yield(2);
}

// ---- the roll gesture (right drag) --------------------------------------------------------
//
// The expected roll is SCREEN GEOMETRY ONLY — the angle the cursor swept around the actual render
// viewport's centre — recomputed here from ImGui window rectangles with atan2. There is no
// production helper for the mapping to lean on (the branch is scene code, by design: a16 wants the
// reference derived from the cursor's own geometry, not from whatever computes it), so these
// helpers ARE the independent side: same window-local, y-down convention io.MousePos lives in.
constexpr float kDeg2RadF = 3.14159265358979323846f / 180.0f;
constexpr float kRad2DegF = 57.29577951308232f;
// Tolerance, derived from the input quantization and nothing else: the engine delivers integer
// window points, so each endpoint's angle around the centre is off by up to ~0.5 pt / r radians
// (r >= 120 pt in these cases -> ~0.24 deg per endpoint, ~0.5 for a sweep). Every defect this
// suite must catch is orders of magnitude above that — a sign flip is twice the sweep, a wrap
// failure is 180, a fixed-gain mapping misses by tens — so 0.6 deg carries no false green.
constexpr float kRollTolDeg = 0.6f;

// Centre of the ACTUAL render viewport: the panel rectangle is the GL viewport (vp_w/vp_h come
// from the panel's size, not the padded content region), so its centre is the panel window's
// centre — not the InvisibleButton's, which sits inside one WindowPadding on every side.
ImVec2 PreviewCentre(ImGuiTestContext* ctx) {
  ImGuiWindow* w = ctx->GetWindowByRef("//##PreviewPanel");
  IM_CHECK_SILENT_RETV(w != nullptr, ImVec2(0, 0));
  return ImVec2(w->Pos.x + w->Size.x * 0.5f, w->Pos.y + w->Size.y * 0.5f);
}

ImVec2 PolarPt(ImVec2 centre, float radius_pt, float angle_deg) {
  const float rad = angle_deg * kDeg2RadF;
  return ImVec2(centre.x + radius_pt * std::cos(rad), centre.y + radius_pt * std::sin(rad));
}

float AngleAroundCentre(ImVec2 centre, ImVec2 p) {
  return std::atan2(p.y - centre.y, p.x - centre.x) * kRad2DegF;
}

float SweepDeg(float from_deg, float to_deg) {
  float d = to_deg - from_deg;
  if (d > 180.0f) {
    d -= 360.0f;
  }
  if (d < -180.0f) {
    d += 360.0f;
  }
  return d;
}

// One right-button press at `from`, travel to `to`, release. The engine animates the move, so
// every interpolated frame goes through the handler — which is why a gesture's total roll is the
// SWEEP OF THE ENDPOINTS: each frame measures endpoint-to-endpoint and the sum telescopes, as long
// as no frame's chord dips into the dead zone and hover never breaks. The cases below choose
// chords that cannot.
void RollDrag(ImGuiTestContext* ctx, ImVec2 from, ImVec2 to) {
  ctx->MouseMoveToPos(from);
  ctx->Yield(1);
  ctx->MouseDown(ImGuiMouseButton_Right);
  ctx->Yield(1);
  ctx->MouseMoveToPos(to);
  ctx->Yield(2);
  ctx->MouseUp(ImGuiMouseButton_Right);
  ctx->Yield(2);
}

}  // namespace

void RegisterPreviewViewportTests(ImGuiTestEngine* engine) {
  // P33. With nothing to show, the panel says so and reports no viewport — the second half is what
  // stops the rest of the GUI from computing drag gains and overlay placements against a rectangle
  // that is not being drawn into.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_viewport", "an_empty_preview_says_so_and_claims_no_viewport");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(3);

      IM_CHECK(!gui::g_preview.HasTexture());
      IM_CHECK(!gui::g_preview.HasBackground());
      IM_CHECK(!gui::g_preview_vp.active);
      // No interaction surface either: there is nothing to drag.
      IM_CHECK(!ctx->ItemExists(kInteract));
    };
  }

  // The other side of the same claim, so the case above cannot pass by the panel being broken:
  // once a texture arrives the viewport becomes active and the surface appears.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_viewport", "a_rendered_preview_claims_a_viewport_to_drag");
    t->GuiFunc = UploadSynthTexture;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      g_upload_done = false;
      ctx->Yield(4);

      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK(gui::g_preview_vp.active);
      IM_CHECK_GT(gui::g_preview_vp.vp_w, 0);
      IM_CHECK_GT(gui::g_preview_vp.vp_h, 0);
      IM_CHECK(ctx->ItemExists(kInteract));
    };
  }

  // P34. A full-sky projection shows the whole sky at once, so there is no direction to be facing —
  // and the panel enforces that every frame rather than only on the frame the lens changed. The
  // angles are set FIRST and the lens second, so what is asserted is the per-frame guard rather
  // than a one-off reset in the lens-change handler.
  //
  // The set is iterated from the constant the product uses, so a lens added to it is covered here
  // without anyone remembering to add a row.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_viewport", "a_full_sky_lens_zeroes_the_pose_every_frame");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(2);
      for (int lens : gui::kFullSkyLensTypes) {
        gui::g_state.renderer.lens_type = lens;
        gui::g_state.renderer.elevation = 30.0f;
        gui::g_state.renderer.azimuth = 45.0f;
        gui::g_state.renderer.roll = 10.0f;
        ctx->Yield(3);
        const auto& r = gui::g_state.renderer;
        if (r.elevation != 0.0f || r.azimuth != 0.0f || r.roll != 0.0f) {
          IM_ERRORF("lens %d left the pose at el=%f az=%f roll=%f", lens, static_cast<double>(r.elevation),
                    static_cast<double>(r.azimuth), static_cast<double>(r.roll));
        }

        if (ctx->IsError()) {
          break;
        }
      }
    };
  }

  // P35 / P36. A rightward drag turns the view, and which WAY depends on the projection: under the
  // globe the user is spinning a sphere in front of them, everywhere else they are turning their
  // own head, so the two signs are opposite. One case, because the claim is the difference — a
  // handler that had lost the branch would agree with one half and not the other.
  //
  // The magnitude is bounded rather than pinned: what is under test is the wiring, and the gain
  // that produces the number has its own coverage at the unit layer.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_viewport", "a_rightward_drag_turns_opposite_ways_on_globe_and_elsewhere");
    t->GuiFunc = UploadSynthTexture;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      struct Case {
        const char* name;
        int lens;
        float sign;
      };
      const Case kCases[] = {
        { "globe", gui::kLensTypeGlobe, +1.0f },
        { "fisheye", gui::kLensTypeFisheyeEquidist, -1.0f },
      };

      // One lens per call rather than one loop body: the texture precondition and the
      // non-degenerate-viewport guard are fatal (without either, everything below is vacuous), and a
      // fatal assert inside a loop would return out of the case and take the second lens with it.
      auto drag_on = [ctx](const Case& c) {
        ResetTestState();
        g_upload_done = false;
        ctx->Yield(4);
        IM_CHECK(gui::g_preview.HasTexture());
        SeedPose(c.lens, 60.0f, 0.0f, 0.0f);
        ctx->Yield(2);

        const float expected = c.sign * ExpectedDeg(c.lens, 60.0f, 60.0f);
        // A degenerate viewport would make every comparison below vacuously true.
        IM_CHECK_GT(std::fabs(expected), 1.0f);
        ctx->ItemDragWithDelta(kInteract, ImVec2(60.0f, 0.0f));
        ctx->Yield(2);

        const auto& r = gui::g_state.renderer;
        if (std::fabs(r.azimuth - expected) > 1.0f) {
          IM_ERRORF("%s: azimuth landed on %f, expected about %f", c.name, static_cast<double>(r.azimuth),
                    static_cast<double>(expected));
        }
        // A horizontal drag is horizontal: nothing else moved.
        if (std::fabs(r.elevation) > 0.5f || std::fabs(r.roll) > 0.001f) {
          IM_ERRORF("%s: a horizontal drag also moved el=%f roll=%f", c.name, static_cast<double>(r.elevation),
                    static_cast<double>(r.roll));
        }
      };

      for (const Case& c : kCases) {
        drag_on(c);

        if (ctx->IsError()) {
          break;
        }
      }
    };
  }

  // P35's sharp half, and the reason the gain is a function rather than a constant: the same drag
  // has to turn the view further at a wide field of view than at a narrow one. A constant gain
  // satisfies every other case in this file.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_viewport", "the_same_drag_turns_further_at_a_wider_field_of_view");
    t->GuiFunc = UploadSynthTexture;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      g_upload_done = false;
      ctx->Yield(4);
      IM_CHECK(gui::g_preview.HasTexture());

      SeedPose(gui::kLensTypeFisheyeEquidist, 10.0f, 0.0f, 0.0f);
      ctx->Yield(2);
      ctx->ItemDragWithDelta(kInteract, ImVec2(60.0f, 0.0f));
      ctx->Yield(2);
      const float narrow = std::fabs(gui::g_state.renderer.azimuth);

      SeedPose(gui::kLensTypeFisheyeEquidist, 160.0f, 0.0f, 0.0f);
      ctx->Yield(2);
      ctx->ItemDragWithDelta(kInteract, ImVec2(60.0f, 0.0f));
      ctx->Yield(2);
      const float wide = std::fabs(gui::g_state.renderer.azimuth);

      IM_CHECK_GT(narrow, 0.0f);  // the premise: both drags did something
      IM_CHECK_GT(wide, narrow);
    };
  }

  // P37. Azimuth is a circle, so a drag past due south has to come out the other side rather than
  // stop there — a pinned value would leave the user unable to turn any further in the direction
  // they were already turning.
  //
  // The overshoot is asserted rather than assumed: if the gain ever shrinks enough that 200 px no
  // longer clears the remaining 10 degrees, this case would stop exercising the wrap and go on
  // passing quietly on an unwrapped value.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_viewport", "the_azimuth_wraps_round_instead_of_pinning_at_the_seam");
    t->GuiFunc = UploadSynthTexture;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      g_upload_done = false;
      ctx->Yield(4);
      IM_CHECK(gui::g_preview.HasTexture());
      SeedPose(gui::kLensTypeGlobe, 60.0f, 170.0f, 0.0f);
      ctx->Yield(2);

      const float unwrapped = 170.0f + ExpectedDeg(gui::kLensTypeGlobe, 60.0f, 200.0f);
      IM_CHECK_GT(unwrapped, 180.0f);  // precondition: the drag really does cross the seam
      ctx->ItemDragWithDelta(kInteract, ImVec2(200.0f, 0.0f));
      ctx->Yield(2);

      IM_CHECK_LT(gui::g_state.renderer.azimuth, 0.0f);  // wrapped, not pinned at +180
      IM_CHECK_LT(std::fabs(gui::g_state.renderer.azimuth - (unwrapped - 360.0f)), 1.0f);
    };
  }

  // P36 / P38. Elevation is NOT a circle — past the pole the view would be upside down — so it
  // clamps, and the globe clamps one degree short of the other lenses because its orbit degenerates
  // exactly at the pole. Both limits in one case, since the claim is that they differ.
  //
  // Each row asserts the drag overshot the limit before asserting the limit held, so neither can
  // decay into "never reached it, therefore never violated it". The field of view differs per row
  // for that reason alone: the gain scales with it, and at 60 degrees a 1000 px drag never reaches
  // the looser limit at all.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_viewport", "elevation_clamps_one_degree_short_of_the_pole_on_globe");
    t->GuiFunc = UploadSynthTexture;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      struct Limit {
        const char* name;
        int lens;
        float fov;
        float dy;  // globe drives elevation up on an upward drag, the others on a downward one
        float limit;
      };
      const Limit kLimits[] = {
        { "globe", gui::kLensTypeGlobe, 60.0f, -1000.0f, 89.0f },
        { "fisheye", gui::kLensTypeFisheyeEquidist, 360.0f, 1000.0f, 90.0f },
      };

      // A lambda per row, for the same reason as the sign case above: the texture precondition is
      // fatal and would otherwise take the second row with it.
      auto drive = [ctx](const Limit& l) {
        ResetTestState();
        g_upload_done = false;
        ctx->Yield(4);
        IM_CHECK(gui::g_preview.HasTexture());
        SeedPose(l.lens, l.fov, 0.0f, 0.0f);
        ctx->Yield(2);

        const float reach = ExpectedDeg(l.lens, l.fov, 1000.0f);
        if (reach <= l.limit) {
          IM_ERRORF("%s: a 1000 px drag only reaches %f degrees, which never touches the %f limit", l.name,
                    static_cast<double>(reach), static_cast<double>(l.limit));
          return;
        }
        ctx->ItemDragWithDelta(kInteract, ImVec2(0.0f, l.dy));
        ctx->Yield(2);

        const float el = gui::g_state.renderer.elevation;
        if (el < l.limit || el >= l.limit + 0.5f) {
          IM_ERRORF("%s: elevation settled at %f, expected the %f limit", l.name, static_cast<double>(el),
                    static_cast<double>(l.limit));
        }
      };

      for (const Limit& l : kLimits) {
        drive(l);

        if (ctx->IsError()) {
          break;
        }
      }
    };
  }

  // P38. A full-sky projection has nothing to aim, so neither the drag nor the wheel may move
  // anything — and the surface is still submitted, so "nothing happened" has to be the handler's
  // decision rather than the absence of a target to click.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_viewport", "a_full_sky_lens_ignores_the_drag_it_still_accepts");
    t->GuiFunc = UploadSynthTexture;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      g_upload_done = false;
      ctx->Yield(4);
      IM_CHECK(gui::g_preview.HasTexture());

      gui::g_state.renderer.lens_type = gui::kLensTypeDualFisheyeEqualArea;
      ctx->Yield(3);
      IM_CHECK(ctx->ItemExists(kInteract));  // the premise: there IS something to drag on
      const float fov_before = gui::g_state.renderer.fov;

      ctx->ItemDragWithDelta(kInteract, ImVec2(120.0f, 80.0f));
      ctx->Yield(3);
      IM_CHECK_EQ(gui::g_state.renderer.azimuth, 0.0f);
      IM_CHECK_EQ(gui::g_state.renderer.elevation, 0.0f);
      IM_CHECK_EQ(gui::g_state.renderer.fov, fov_before);
    };
  }

  // The routing itself: with the modifier held, the drag owns the background and the camera does
  // not move. Both halves matter — a handler that moved the background AND kept orbiting would
  // satisfy any assertion that only looked at bg_offset_*.
  //
  // Direction is asserted, not just "the value changed". A sign error here is the failure this
  // gesture is most likely to ship with: everything still moves, smoothly, at the right speed, in
  // the wrong direction, and no structural test can tell. Dragging right must slide the image
  // right, which means sampling FURTHER LEFT in the texture, which means a SMALLER u offset — so
  // the offset moves opposite to the cursor, and that is what is pinned.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_viewport", "the_modifier_drag_pans_the_background_and_leaves_the_camera_put");
    t->GuiFunc = UploadSynthTextureAndBackground;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      LoadBackground(ctx, /*shown=*/true);
      SeedPose(gui::kLensTypeFisheyeEquidist, 60.0f, 12.0f, -7.0f);
      ctx->Yield(2);

      ModDrag(ctx, ImVec2(60.0f, 0.0f));
      const float after_right_x = gui::g_state.bg_offset_x;
      IM_CHECK_LT(after_right_x, 0.0f);                   // image followed the cursor to the right
      IM_CHECK_EQ(gui::g_state.bg_offset_y, 0.0f);        // a horizontal drag is horizontal only
      IM_CHECK_EQ(gui::g_state.renderer.azimuth, 12.0f);  // the camera saw none of it
      IM_CHECK_EQ(gui::g_state.renderer.elevation, -7.0f);

      // Down the screen. Independent axis, and its own sign: the texture's v runs opposite to the
      // screen's y (scale_y is negative), so this is not the x case restated.
      gui::g_state.bg_offset_x = 0.0f;
      ctx->Yield(2);
      ModDrag(ctx, ImVec2(0.0f, 40.0f));
      IM_CHECK_LT(gui::g_state.bg_offset_y, 0.0f);
      IM_CHECK_EQ(gui::g_state.bg_offset_x, 0.0f);
      IM_CHECK_EQ(gui::g_state.renderer.azimuth, 12.0f);
      IM_CHECK_EQ(gui::g_state.renderer.elevation, -7.0f);
    };
  }

  // Twice the drag moves twice as far, and twice the zoom moves half as far.
  //
  // The second half is the claim that makes the gesture usable at all: the pan is solved from the
  // same scale the fragment shader samples with, so the image stays glued to the cursor at every
  // zoom level rather than needing a separate sensitivity curve the way the camera drag does. A
  // pan that ignored zoom would pass every other case in this file and then feel broken the moment
  // anyone zoomed in.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_viewport", "the_background_pan_tracks_the_cursor_at_any_zoom");
    t->GuiFunc = UploadSynthTextureAndBackground;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      LoadBackground(ctx, /*shown=*/true);

      ModDrag(ctx, ImVec2(40.0f, 0.0f));
      const float one_unit = std::fabs(gui::g_state.bg_offset_x);
      IM_CHECK_GT(one_unit, 0.0f);  // the premise: the drag did something

      gui::g_state.bg_offset_x = 0.0f;
      ctx->Yield(2);
      ModDrag(ctx, ImVec2(80.0f, 0.0f));
      const float two_units = std::fabs(gui::g_state.bg_offset_x);
      IM_CHECK_LT(std::fabs(two_units - 2.0f * one_unit), 0.1f * one_unit);

      // Same 40 px at 2x zoom. The offset is in texture widths, and at 2x each screen pixel covers
      // half a texture width of what it did, so the same drag is worth half the offset.
      gui::g_state.bg_offset_x = 0.0f;
      gui::g_state.bg_scale = 2.0f;
      ctx->Yield(2);
      ModDrag(ctx, ImVec2(40.0f, 0.0f));
      const float zoomed = std::fabs(gui::g_state.bg_offset_x);
      IM_CHECK_LT(std::fabs(zoomed - 0.5f * one_unit), 0.1f * one_unit);
    };
  }

  // The unmodified gesture, unchanged. This is the other half of AC3 and the reason the camera
  // branches carry an explicit `!bg_modifier`: the claim is not merely that the new code works,
  // it is that the old code was not disturbed, which only a case that drives it can say.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_viewport", "a_plain_drag_still_turns_the_camera_with_a_background_loaded");
    t->GuiFunc = UploadSynthTextureAndBackground;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      LoadBackground(ctx, /*shown=*/true);
      SeedPose(gui::kLensTypeFisheyeEquidist, 60.0f, 0.0f, 0.0f);
      ctx->Yield(2);

      const float expected = ExpectedDeg(gui::kLensTypeFisheyeEquidist, 60.0f, 60.0f);
      ctx->ItemDragWithDelta(kInteract, ImVec2(60.0f, 0.0f));
      ctx->Yield(2);

      IM_CHECK_LT(std::fabs(gui::g_state.renderer.azimuth + expected), 0.5f);  // the pre-existing law
      IM_CHECK_EQ(gui::g_state.bg_offset_x, 0.0f);                             // and the background sat still
      IM_CHECK_EQ(gui::g_state.bg_offset_y, 0.0f);
    };
  }

  // The wheel, both ways round. One case rather than two, because the property is the SPLIT: each
  // owner must move under its own modifier state and stay put under the other's. Two cases could
  // both pass against a handler that moved both every time.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_viewport",
                         "the_modifier_sends_the_wheel_to_the_background_and_bare_wheel_to_the_field_of_view");
    t->GuiFunc = UploadSynthTextureAndBackground;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      LoadBackground(ctx, /*shown=*/true);
      SeedPose(gui::kLensTypeFisheyeEquidist, 60.0f, 0.0f, 0.0f);
      ctx->Yield(2);

      ModWheel(ctx, 2.0f);
      IM_CHECK_GT(gui::g_state.bg_scale, 1.0f);       // scrolling up magnifies the photograph
      IM_CHECK_EQ(gui::g_state.renderer.fov, 60.0f);  // and the lens did not move

      const float zoomed = gui::g_state.bg_scale;
      ctx->MouseMove(kInteract);
      ctx->MouseWheelY(2.0f);
      ctx->Yield(2);
      IM_CHECK_LT(gui::g_state.renderer.fov, 60.0f);  // bare wheel still narrows the field of view
      IM_CHECK_EQ(gui::g_state.bg_scale, zoomed);     // and the photograph did not move
    };
  }

  // The fourth square of the routing table, and the one an implementation gets wrong by omission:
  // modifier held with nothing to move. The gesture must do NOTHING — not fall through to the
  // camera. A user pressing the modifier has said what they meant to move; silently orbiting
  // instead is a worse answer than no answer, because it is indistinguishable from a plain drag
  // and leaves them with a pose they did not ask for.
  //
  // Driven at both reachable spellings of "nothing to move": no image at all, and an image that is
  // loaded but hidden. They are separate branches of `bg_active` and a check of only one would
  // pass against a handler that tested HasBackground() and forgot bg_show.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_viewport",
                                    "the_modifier_gesture_does_nothing_when_there_is_no_background_to_move");
    t->GuiFunc = UploadSynthTextureAndBackground;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      // No background at all: only the render texture, which is what puts the canvas on screen.
      ResetTestState();
      g_upload_done = false;
      ctx->Yield(4);
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK(!gui::g_preview.HasBackground());
      SeedPose(gui::kLensTypeFisheyeEquidist, 60.0f, 5.0f, 3.0f);
      ctx->Yield(2);

      ModDrag(ctx, ImVec2(60.0f, 40.0f));
      ModWheel(ctx, 2.0f);
      IM_CHECK_EQ(gui::g_state.bg_offset_x, 0.0f);
      IM_CHECK_EQ(gui::g_state.bg_offset_y, 0.0f);
      IM_CHECK_EQ(gui::g_state.bg_scale, 1.0f);
      IM_CHECK_EQ(gui::g_state.renderer.azimuth, 5.0f);
      IM_CHECK_EQ(gui::g_state.renderer.elevation, 3.0f);
      IM_CHECK_EQ(gui::g_state.renderer.fov, 60.0f);

      // Loaded but hidden. Same expectation, different branch.
      ResetTestState();
      LoadBackground(ctx, /*shown=*/false);
      SeedPose(gui::kLensTypeFisheyeEquidist, 60.0f, 5.0f, 3.0f);
      ctx->Yield(2);

      ModDrag(ctx, ImVec2(60.0f, 40.0f));
      ModWheel(ctx, 2.0f);
      IM_CHECK_EQ(gui::g_state.bg_offset_x, 0.0f);
      IM_CHECK_EQ(gui::g_state.bg_offset_y, 0.0f);
      IM_CHECK_EQ(gui::g_state.bg_scale, 1.0f);
      IM_CHECK_EQ(gui::g_state.renderer.azimuth, 5.0f);
      IM_CHECK_EQ(gui::g_state.renderer.elevation, 3.0f);
      IM_CHECK_EQ(gui::g_state.renderer.fov, 60.0f);
    };
  }

  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_viewport", "a_camera_drag_takes_over_from_a_locked_target");
    t->GuiFunc = UploadSynthTexture;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      g_upload_done = false;
      ctx->Yield(4);
      IM_CHECK(gui::g_preview.HasTexture());

      gui::g_state.sun.altitude = 20.0f;
      IM_CHECK(gui::SelectLookAtTarget(gui::g_state, gui::LookAtId::kSun));
      IM_CHECK(gui::SetLookAtLocked(gui::g_state, true));
      ctx->Yield(2);
      IM_CHECK(gui::g_state.look_at.locked);

      ctx->ItemDragWithDelta(kInteract, ImVec2(60.0f, 40.0f));
      ctx->Yield(2);
      IM_CHECK(!gui::g_state.look_at.locked);
      const float dragged_azimuth = gui::g_state.renderer.azimuth;
      const float dragged_elevation = gui::g_state.renderer.elevation;
      IM_CHECK_NE(dragged_azimuth, 0.0f);
      IM_CHECK_NE(dragged_elevation, 20.0f);

      gui::g_state.sun.altitude = 55.0f;
      ctx->Yield(3);
      IM_CHECK_EQ(gui::g_state.renderer.azimuth, dragged_azimuth);
      IM_CHECK_EQ(gui::g_state.renderer.elevation, dragged_elevation);
      IM_CHECK_EQ(gui::g_state.look_at.target, gui::LookAtId::kSun);
    };
  }

  // The roll gesture's main claim, and its direction: a right drag turns the picture WITH the
  // cursor around the viewport's centre, by exactly the swept angle, and moves nothing else.
  // Both sweep directions, because the claim is the pairing — a sign error is the failure this
  // gesture is most likely to ship with, and everything else (smoothness, magnitude, wrap) can
  // be right while the turn is backwards. The expected value is the endpoints' angular sweep
  // recomputed from the window rectangle; the sign behind it was derived from BuildViewMatrix
  // (the camera basis rotates by -roll; the projections hand view +y to screen up, so the
  // y-down screen angle of the content moves by +delta-roll) — which is why the y-down atan2
  // here, with NO sign flip, is the expected law.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_viewport",
                                    "a_right_drag_rolls_the_picture_with_the_cursor_by_the_swept_angle");
    t->GuiFunc = UploadSynthTexture;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      g_upload_done = false;
      ctx->Yield(4);
      IM_CHECK(gui::g_preview.HasTexture());
      SeedPose(gui::kLensTypeFisheyeEquidist, 60.0f, 12.0f, -7.0f);
      ctx->Yield(2);

      const ImVec2 c = PreviewCentre(ctx);
      const float a0 = AngleAroundCentre(c, PolarPt(c, 150.0f, 20.0f));
      const float a1 = AngleAroundCentre(c, PolarPt(c, 150.0f, 65.0f));
      const float sweep = SweepDeg(a0, a1);
      IM_CHECK_GT(sweep, 40.0f);  // the premise: a real 45-degree clockwise sweep, not noise

      RollDrag(ctx, PolarPt(c, 150.0f, 20.0f), PolarPt(c, 150.0f, 65.0f));
      IM_CHECK_LT(std::fabs(gui::g_state.renderer.roll - sweep), kRollTolDeg);
      IM_CHECK_EQ(gui::g_state.renderer.azimuth, 12.0f);
      IM_CHECK_EQ(gui::g_state.renderer.elevation, -7.0f);
      IM_CHECK_EQ(gui::g_state.renderer.fov, 60.0f);

      // The mirrored arc: equal magnitude, opposite direction, landing back where it started.
      const float rolled = gui::g_state.renderer.roll;
      RollDrag(ctx, PolarPt(c, 150.0f, 65.0f), PolarPt(c, 150.0f, 20.0f));
      IM_CHECK_LT(std::fabs(gui::g_state.renderer.roll - (rolled - sweep)), kRollTolDeg);
      IM_CHECK_LT(std::fabs(gui::g_state.renderer.roll), kRollTolDeg);
    };
  }

  // Roll is a circle the way azimuth is: the slider's [-180, 180] domain is a WRAP, not a wall —
  // a drag past the seam comes out the other side instead of pinning, and keeps accumulating
  // from the wrapped value. The overshoot is asserted before the drag, so the case cannot decay
  // into "never reached the seam, therefore never wrapped".
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_viewport", "the_roll_drag_wraps_round_instead_of_pinning_at_the_seam");
    t->GuiFunc = UploadSynthTexture;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      g_upload_done = false;
      ctx->Yield(4);
      IM_CHECK(gui::g_preview.HasTexture());
      SeedPose(gui::kLensTypeFisheyeEquidist, 60.0f, 0.0f, 0.0f);
      gui::g_state.renderer.roll = 170.0f;
      ctx->Yield(2);

      const ImVec2 c = PreviewCentre(ctx);
      const float unwrapped = 170.0f + SweepDeg(AngleAroundCentre(c, PolarPt(c, 150.0f, 10.0f)),
                                                AngleAroundCentre(c, PolarPt(c, 150.0f, 40.0f)));
      IM_CHECK_GT(unwrapped, 180.0f);  // precondition: the sweep really does cross the seam
      RollDrag(ctx, PolarPt(c, 150.0f, 10.0f), PolarPt(c, 150.0f, 40.0f));
      IM_CHECK_LT(gui::g_state.renderer.roll, 0.0f);  // wrapped, not pinned at +180
      IM_CHECK_LT(std::fabs(gui::g_state.renderer.roll - (unwrapped - 360.0f)), kRollTolDeg);
    };
  }

  // The pointing mapping's three degeneracies, in one press: inside the dead zone nothing
  // accumulates; the first valid sample OUT of it only sets the reference (no jump); and a chord
  // that cuts across the centre in one step is dropped rather than read as a ~180-degree spin —
  // after which a fresh short arc still works, on a rebuilt reference.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_viewport", "the_dead_zone_and_a_centre_crossing_hold_the_reference_quiet");
    t->GuiFunc = UploadSynthTexture;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      g_upload_done = false;
      ctx->Yield(4);
      IM_CHECK(gui::g_preview.HasTexture());
      SeedPose(gui::kLensTypeFisheyeEquidist, 60.0f, 0.0f, 0.0f);
      ctx->Yield(2);

      const ImVec2 c = PreviewCentre(ctx);
      // Press AT the centre and jiggle inside the dead zone: nothing to measure against yet.
      ctx->MouseMoveToPos(c);
      ctx->Yield(1);
      ctx->MouseDown(ImGuiMouseButton_Right);
      ctx->Yield(1);
      ctx->MouseMoveToPos(PolarPt(c, 8.0f, 0.0f));  // inside the 16 pt dead zone
      ctx->Yield(2);
      ctx->MouseMoveToPos(PolarPt(c, 8.0f, 150.0f));  // still inside: a huge nominal angle change
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.renderer.roll, 0.0f);

      // Out of the zone: the FIRST valid point establishes the reference and rolls nothing.
      ctx->MouseMoveToPos(PolarPt(c, 120.0f, 30.0f));
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.renderer.roll, 0.0f);
      // From the reference, a real arc accumulates.
      ctx->MouseMoveToPos(PolarPt(c, 120.0f, 60.0f));
      ctx->Yield(2);
      const float after_arc = gui::g_state.renderer.roll;
      IM_CHECK_LT(std::fabs(after_arc - 30.0f), kRollTolDeg);

      // Across the centre: the chord must actually DIP INTO the dead zone for the step to be a
      // crossing, so the jump lands on antipodes of the r=50 ring (45° -> 225°) — a diameter
      // whose chord passes through the centre itself, robustly inside the zone even after the
      // engine's integer snapping. (A 150-degree jump between r=120 points would be a legitimate
      // fast swirl: its chord stays well outside the zone and measures as the sweep it is.)
      // The crossing adds no roll and no ~180-degree jump; the engine may deliver the move as
      // one frame's jump or animate it — the near-centre chords are dropped either way. The
      // press ends here: what the crossing must NOT do is spend the picture.
      ctx->MouseMoveToPos(PolarPt(c, 50.0f, 225.0f));
      ctx->Yield(3);
      IM_CHECK_LT(std::fabs(gui::g_state.renderer.roll - after_arc), kRollTolDeg);
      ctx->MouseUp(ImGuiMouseButton_Right);
      ctx->Yield(2);

      // And the gesture still works after a crossing: a fresh press re-establishes its own
      // reference and the next arc accumulates from it.
      ctx->MouseMoveToPos(PolarPt(c, 120.0f, 210.0f));
      ctx->Yield(1);
      ctx->MouseDown(ImGuiMouseButton_Right);
      ctx->Yield(1);
      ctx->MouseMoveToPos(PolarPt(c, 120.0f, 240.0f));
      ctx->Yield(2);
      IM_CHECK_LT(std::fabs(gui::g_state.renderer.roll - (after_arc + 30.0f)), kRollTolDeg);
      ctx->MouseUp(ImGuiMouseButton_Right);
      ctx->Yield(2);
    };
  }

  // Full-sky: the entry rejects the press rather than writing into a value the per-frame
  // invariant would zero — the pose stays at the canonical zero AND the press never became a
  // roll that got cleaned up afterwards. (Which of those two held is white-box: admission
  // carries `!full_sky`, so the write site is unreachable, not merely erased.)
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_viewport", "a_full_sky_lens_ignores_the_right_drag_it_still_accepts");
    t->GuiFunc = UploadSynthTexture;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      g_upload_done = false;
      ctx->Yield(4);
      IM_CHECK(gui::g_preview.HasTexture());
      gui::g_state.renderer.lens_type = gui::kLensTypeDualFisheyeEqualArea;
      ctx->Yield(3);
      IM_CHECK(ctx->ItemExists(kInteract));  // the premise: there IS something to right-drag on

      const ImVec2 c = PreviewCentre(ctx);
      RollDrag(ctx, PolarPt(c, 150.0f, 20.0f), PolarPt(c, 150.0f, 65.0f));
      IM_CHECK_EQ(gui::g_state.renderer.roll, 0.0f);
      IM_CHECK_EQ(gui::g_state.renderer.azimuth, 0.0f);
      IM_CHECK_EQ(gui::g_state.renderer.elevation, 0.0f);
    };
  }

  // Globe: the press is refused, the same way the lens greys the Roll slider — the field's
  // shared registry gate closes it, admission never happens, and the write site is structurally
  // unreachable (white-box: the same "rejected at entry, not erased afterwards" shape the
  // full-sky case above pins). The stored roll starts NON-zero on purpose: the one thing the
  // refusal must guarantee is that the gesture cannot move it, and a zero start could also pass
  // on a write-then-reset bug. The published half reads the frame's actual view projection — the
  // struct the deferred render hands to BuildViewMatrix, filled per-frame through
  // EffectiveRollForLens (preview_renderer.hpp) — so it is what the shader was drawn with, not a
  // re-run of the helper, which would agree with a write-side bug by construction.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_viewport", "a_globe_right_drag_is_refused_like_its_roll_slider");
    t->GuiFunc = UploadSynthTexture;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      g_upload_done = false;
      ctx->Yield(4);
      IM_CHECK(gui::g_preview.HasTexture());
      SeedPose(gui::kLensTypeGlobe, 60.0f, 5.0f, 3.0f);
      gui::g_state.renderer.roll = 25.0f;  // non-zero stored value: the drag must not touch it
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_preview_vp.params.view_proj.roll, 0.0f);  // published flat before the press

      const ImVec2 c = PreviewCentre(ctx);
      ctx->MouseMoveToPos(PolarPt(c, 150.0f, 20.0f));
      ctx->Yield(1);
      ctx->MouseDown(ImGuiMouseButton_Right);
      ctx->Yield(1);
      ctx->MouseMoveToPos(PolarPt(c, 150.0f, 65.0f));
      ctx->Yield(2);
      // Mid-hold, with the sweep delivered: the stored value is exactly what it was and the
      // published view is flat — the press never became a roll that anything cleaned up.
      IM_CHECK_EQ(gui::g_state.renderer.roll, 25.0f);
      IM_CHECK_EQ(gui::g_preview_vp.params.view_proj.roll, 0.0f);
      ctx->MouseUp(ImGuiMouseButton_Right);
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.renderer.roll, 25.0f);
      IM_CHECK_EQ(gui::g_preview_vp.params.view_proj.roll, 0.0f);
      IM_CHECK_EQ(gui::g_state.renderer.azimuth, 5.0f);
      IM_CHECK_EQ(gui::g_state.renderer.elevation, 3.0f);
    };
  }

  // Alt + right drag is nothing at all, both reachable spellings of "nothing to move": no
  // background at all, and a background loaded and shown. Alt states the photo's intent and the
  // photo has no rotate — swallowing the press into a roll would be indistinguishable from the
  // plain gesture and leave a pose the user did not ask for.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_viewport", "alt_plus_right_drag_moves_nothing_on_camera_or_photo");
    t->GuiFunc = UploadSynthTextureAndBackground;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      // No background: the render texture alone, which is what puts the canvas on screen.
      ResetTestState();
      g_upload_done = false;
      ctx->Yield(4);
      IM_CHECK(gui::g_preview.HasTexture());
      IM_CHECK(!gui::g_preview.HasBackground());
      SeedPose(gui::kLensTypeFisheyeEquidist, 60.0f, 5.0f, 3.0f);
      ctx->Yield(2);
      const ImVec2 c = PreviewCentre(ctx);
      ctx->KeyDown(kBgMod);
      RollDrag(ctx, PolarPt(c, 150.0f, 20.0f), PolarPt(c, 150.0f, 65.0f));
      ctx->KeyUp(kBgMod);
      IM_CHECK_EQ(gui::g_state.renderer.roll, 0.0f);
      IM_CHECK_EQ(gui::g_state.renderer.azimuth, 5.0f);
      IM_CHECK_EQ(gui::g_state.renderer.elevation, 3.0f);
      IM_CHECK_EQ(gui::g_state.bg_offset_x, 0.0f);
      IM_CHECK_EQ(gui::g_state.bg_scale, 1.0f);

      // Loaded and shown: same expectation, and the photo transform also sat still.
      ResetTestState();
      LoadBackground(ctx, /*shown=*/true);
      SeedPose(gui::kLensTypeFisheyeEquidist, 60.0f, 5.0f, 3.0f);
      ctx->Yield(2);
      const ImVec2 c2 = PreviewCentre(ctx);
      ctx->KeyDown(kBgMod);
      RollDrag(ctx, PolarPt(c2, 150.0f, 20.0f), PolarPt(c2, 150.0f, 65.0f));
      ctx->KeyUp(kBgMod);
      IM_CHECK_EQ(gui::g_state.renderer.roll, 0.0f);
      IM_CHECK_EQ(gui::g_state.renderer.azimuth, 5.0f);
      IM_CHECK_EQ(gui::g_state.renderer.elevation, 3.0f);
      IM_CHECK_EQ(gui::g_state.bg_offset_x, 0.0f);
      IM_CHECK_EQ(gui::g_state.bg_offset_y, 0.0f);
      IM_CHECK_EQ(gui::g_state.bg_scale, 1.0f);
    };
  }

  // Press ownership. A roll press must START on the preview; one that started elsewhere never
  // becomes a roll when the cursor wanders in. Alt arriving mid-press forfeits the gesture for
  // the REST of the press — letting go of Alt does not revive it — and only a fresh press works.
  // Leaving the face mid-press only pauses: the reference is rebuilt on re-entry, so the arc
  // after returning measures from where the cursor came back, and releasing while off the face
  // still ends the gesture cleanly.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_viewport",
                                    "a_roll_press_must_start_here_and_survives_alt_and_off_face_only_by_restart");
    t->GuiFunc = UploadSynthTexture;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      g_upload_done = false;
      ctx->Yield(4);
      IM_CHECK(gui::g_preview.HasTexture());
      SeedPose(gui::kLensTypeFisheyeEquidist, 60.0f, 0.0f, 0.0f);
      ctx->Yield(2);

      ImGuiWindow* preview = ctx->GetWindowByRef("//##PreviewPanel");
      IM_CHECK(preview != nullptr);  // fatal: without the panel every position below is fake
      const ImVec2 c = PreviewCentre(ctx);
      const ImVec2 off_face(preview->Rect().Max.x + 60.0f, preview->Pos.y + preview->Size.y * 0.5f);

      // Started off the face, wandered in, swept a real arc: nothing.
      ctx->MouseMoveToPos(off_face);
      ctx->Yield(1);
      ctx->MouseDown(ImGuiMouseButton_Right);
      ctx->Yield(1);
      ctx->MouseMoveToPos(PolarPt(c, 150.0f, 20.0f));
      ctx->Yield(2);
      ctx->MouseMoveToPos(PolarPt(c, 150.0f, 65.0f));
      ctx->Yield(2);
      ctx->MouseUp(ImGuiMouseButton_Right);
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.renderer.roll, 0.0f);

      // Alt mid-press forfeits the rest of the press; releasing Alt does not revive it.
      ctx->MouseMoveToPos(PolarPt(c, 150.0f, 20.0f));
      ctx->Yield(1);
      ctx->MouseDown(ImGuiMouseButton_Right);
      ctx->Yield(1);
      ctx->MouseMoveToPos(PolarPt(c, 150.0f, 35.0f));  // +15 before Alt arrives
      ctx->Yield(2);
      ctx->KeyDown(kBgMod);
      ctx->Yield(1);
      ctx->MouseMoveToPos(PolarPt(c, 150.0f, 65.0f));  // under Alt: nothing may accumulate
      ctx->Yield(2);
      ctx->KeyUp(kBgMod);
      ctx->Yield(1);
      const float after_alt = gui::g_state.renderer.roll;
      IM_CHECK_LT(std::fabs(after_alt - 15.0f), kRollTolDeg);
      ctx->MouseMoveToPos(PolarPt(c, 150.0f, 100.0f));  // Alt gone, SAME press: still forfeited
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.renderer.roll, after_alt);
      ctx->MouseUp(ImGuiMouseButton_Right);
      ctx->Yield(2);

      // Leaving the face pauses; the re-entry reference makes the next arc measure from where
      // the cursor came back. Released off the face, so the ending is also exercised out there.
      RollDrag(ctx, PolarPt(c, 150.0f, 20.0f), PolarPt(c, 150.0f, 40.0f));  // +20
      const float after_leg2 = gui::g_state.renderer.roll;
      ctx->MouseMoveToPos(PolarPt(c, 150.0f, 40.0f));
      ctx->Yield(1);
      ctx->MouseDown(ImGuiMouseButton_Right);
      ctx->Yield(1);
      ctx->MouseMoveToPos(off_face);  // off the face: paused, reference dropped
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.renderer.roll, after_leg2);
      ctx->MouseMoveToPos(PolarPt(c, 200.0f, 200.0f));  // re-enter: reference rebuilds, no jump
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.renderer.roll, after_leg2);
      ctx->MouseMoveToPos(PolarPt(c, 200.0f, 230.0f));  // the rebuilt reference now measures
      ctx->Yield(2);
      IM_CHECK_LT(std::fabs(gui::g_state.renderer.roll - (after_leg2 + 30.0f)), kRollTolDeg);
      ctx->MouseUp(ImGuiMouseButton_Right);
      ctx->Yield(2);
    };
  }

  // The T-view boundary: a roll is framing, not orientation. Look At Lock survives it (only
  // azimuth/elevation editing and the left drag orbit unlock), and the document is left
  // undirty and uncommitted — no MarkDirty, no epoch.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_viewport", "a_right_drag_rolls_without_touching_look_at_or_the_document");
    t->GuiFunc = UploadSynthTexture;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      g_upload_done = false;
      ctx->Yield(4);
      IM_CHECK(gui::g_preview.HasTexture());
      SeedPose(gui::kLensTypeFisheyeEquidist, 60.0f, 0.0f, 0.0f);
      gui::g_state.sun.altitude = 20.0f;
      IM_CHECK(gui::SelectLookAtTarget(gui::g_state, gui::LookAtId::kSun));
      IM_CHECK(gui::SetLookAtLocked(gui::g_state, true));
      ctx->Yield(2);
      IM_CHECK(gui::g_state.look_at.locked);
      const float az = gui::g_state.renderer.azimuth;
      const float el = gui::g_state.renderer.elevation;
      const bool dirty = gui::g_state.dirty;
      const uint64_t epoch = gui::g_state.committed_epoch;

      const ImVec2 c = PreviewCentre(ctx);
      RollDrag(ctx, PolarPt(c, 150.0f, 20.0f), PolarPt(c, 150.0f, 65.0f));
      IM_CHECK_GT(std::fabs(gui::g_state.renderer.roll), 10.0f);  // the premise: the roll happened
      IM_CHECK(gui::g_state.look_at.locked);                      // Lock untouched by roll
      IM_CHECK_EQ(gui::g_state.renderer.azimuth, az);             // and the locked view did not move
      IM_CHECK_EQ(gui::g_state.renderer.elevation, el);
      IM_CHECK_EQ(gui::g_state.dirty, dirty);
      IM_CHECK_EQ(gui::g_state.committed_epoch, epoch);
    };
  }

  // Context invalidation, both halves. A running roll press belongs to the document and lens it
  // was admitted under. A document reset (ResetFrontendState — the frontend reset owner every
  // New / Open / Revert routes through) ends the press even though the picture never left: an
  // OpenBaked swaps the texture within one call and a Revert keeps it, so "a picture-less frame"
  // can never be the boundary — the reset event is. A lens switch mid-press ends it too, with no
  // reset involved. Both leave the preview roll-capable again for the NEXT press, which is what
  // keeps the "nothing happened" legs honest.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_viewport", "a_document_reset_or_lens_switch_ends_a_running_roll_press");
    t->GuiFunc = UploadSynthTextureAndMaybeReset;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      g_upload_done = false;
      g_roll_reset = RollResetRequest{};
      ctx->Yield(4);
      IM_CHECK(gui::g_preview.HasTexture());

      // Leg 1 — document reset under a held press, with the picture NEVER leaving: the reset is
      // the kOpenBaked spelling (the owner clears and re-uploads inside one call), driven from
      // GuiFunc because the swap uploads a GL texture there, exactly as the every-frame fixture
      // upload does. The press is seeded under LINEAR — the post-reset default lens — so the
      // lens-identity check cannot be what ends it; only the reset hook can.
      SeedPose(gui::kLensTypeLinear, 90.0f, 0.0f, 0.0f);
      ctx->Yield(2);
      const ImVec2 c = PreviewCentre(ctx);
      ctx->MouseMoveToPos(PolarPt(c, 150.0f, 20.0f));
      ctx->Yield(1);
      ctx->MouseDown(ImGuiMouseButton_Right);
      ctx->Yield(1);
      ctx->MouseMoveToPos(PolarPt(c, 150.0f, 65.0f));
      ctx->Yield(2);
      IM_CHECK_GT(std::fabs(gui::g_state.renderer.roll), 10.0f);  // the press was rolling
      g_roll_reset.requested = true;
      ctx->Yield(6);
      IM_CHECK(g_roll_reset.done);  // the reset ran while the button was still down
      // Re-pin the pose the reset just replaced (same lens: LINEAR), so the held press has a
      // live, admissible context to misbehave against if the cancellation missed.
      SeedPose(gui::kLensTypeLinear, 90.0f, 0.0f, 0.0f);
      ctx->Yield(2);
      IM_CHECK(gui::g_preview.HasTexture());  // the premise: still a picture to roll over
      ctx->MouseMoveToPos(PolarPt(c, 150.0f, 20.0f));
      ctx->Yield(1);
      ctx->MouseMoveToPos(PolarPt(c, 150.0f, 65.0f));  // same arc, SAME press
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.renderer.roll, 0.0f);  // the old press rolled the new context not at all
      ctx->MouseUp(ImGuiMouseButton_Right);
      ctx->Yield(2);
      RollDrag(ctx, PolarPt(c, 150.0f, 20.0f), PolarPt(c, 150.0f, 65.0f));
      IM_CHECK_GT(std::fabs(gui::g_state.renderer.roll), 10.0f);  // a fresh press rolls again

      // Leg 2 — lens switch mid-press, no reset in sight: the sweep accumulated before the
      // switch stands, the switch ends the press, and the arc after it writes nothing.
      ResetTestState();
      g_upload_done = false;
      ctx->Yield(4);
      IM_CHECK(gui::g_preview.HasTexture());
      SeedPose(gui::kLensTypeFisheyeEquidist, 60.0f, 0.0f, 0.0f);
      ctx->Yield(2);
      const ImVec2 c2 = PreviewCentre(ctx);
      ctx->MouseMoveToPos(PolarPt(c2, 150.0f, 20.0f));
      ctx->Yield(1);
      ctx->MouseDown(ImGuiMouseButton_Right);
      ctx->Yield(1);
      ctx->MouseMoveToPos(PolarPt(c2, 150.0f, 65.0f));
      ctx->Yield(2);
      const float before_switch = gui::g_state.renderer.roll;
      IM_CHECK_GT(std::fabs(before_switch), 10.0f);
      gui::g_state.renderer.lens_type = gui::kLensTypeLinear;  // fisheye -> linear, mid-press
      ctx->Yield(2);
      ctx->MouseMoveToPos(PolarPt(c2, 150.0f, 20.0f));
      ctx->Yield(1);
      ctx->MouseMoveToPos(PolarPt(c2, 150.0f, 65.0f));  // same arc, SAME press
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.renderer.roll, before_switch);  // the spent press wrote nothing
      ctx->MouseUp(ImGuiMouseButton_Right);
      ctx->Yield(2);
    };
  }

  // The press position itself is the first reference: press and let the cursor ALREADY be moving
  // by the time the handler next samples — no in-place reference frame. The whole sweep from the
  // press point must accumulate; a gesture that starts measuring from wherever the cursor has
  // travelled to loses the first segment, which on a fast flick is the whole gesture.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_viewport",
                                    "the_press_position_is_the_reference_even_when_the_cursor_is_already_moving");
    t->GuiFunc = UploadSynthTexture;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      g_upload_done = false;
      ctx->Yield(4);
      IM_CHECK(gui::g_preview.HasTexture());
      SeedPose(gui::kLensTypeFisheyeEquidist, 60.0f, 0.0f, 0.0f);
      ctx->Yield(2);

      const ImVec2 c = PreviewCentre(ctx);
      const float a0 = AngleAroundCentre(c, PolarPt(c, 150.0f, 20.0f));
      const float a1 = AngleAroundCentre(c, PolarPt(c, 150.0f, 65.0f));
      const float sweep = SweepDeg(a0, a1);
      ctx->MouseMoveToPos(PolarPt(c, 150.0f, 20.0f));
      ctx->Yield(1);
      ctx->MouseDown(ImGuiMouseButton_Right);
      ctx->MouseMoveToPos(PolarPt(c, 150.0f, 65.0f));  // deliberately NO in-place frame after the press
      ctx->Yield(2);
      ctx->MouseUp(ImGuiMouseButton_Right);
      ctx->Yield(2);
      IM_CHECK_LT(std::fabs(gui::g_state.renderer.roll - sweep), kRollTolDeg);
      IM_CHECK_EQ(gui::g_state.renderer.azimuth, 0.0f);
      IM_CHECK_EQ(gui::g_state.renderer.elevation, 0.0f);
    };
  }
}
