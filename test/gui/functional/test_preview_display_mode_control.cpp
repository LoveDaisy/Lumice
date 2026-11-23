// The display-mode segmented control in the preview's top-right corner (RenderDisplayModeControl,
// src/gui/app_panels.cpp): the one entry point for renderer.display_mode in the main window.
//
// What a real frame is needed for, and so what is asserted here:
//   - that a click on a segment writes the field and the next published frame carries it, without
//     dirtying the document (display-time: nothing is re-simulated);
//   - that the gate is the registry's — under Print both segments are greyed and a click writes
//     nothing — rather than a second copy of the rule;
//   - that the control does not leak input to the viewport it sits on. Two different mechanisms
//     keep it from doing so, and each has its own case: a press is kept off the camera by
//     submission order (the control takes the hover first, so `##preview_interact` never becomes
//     active), while the wheel is kept off it by an explicit mask, because the interaction
//     button's IsItemHovered() does not consult which item holds the hover.
//
// Deliberately NOT here: the control's placement arithmetic, which is a pure function asserted
// without a frame in unit-correctness/gui/test_gui_widget_rules.cpp; the tooltip TEXT, which is
// drawn through ImGui::SetTooltip (TextUnformatted with id 0, invisible to the test engine's item
// registry — the limit test_view_display_controls.cpp records for its own tooltips), so the gate's
// reason is asserted on the function the tooltip prints; and the idle/hover opacity, a feel value
// with no ruler, judged on screenshots.

#include <string>

#include "gui/field_editor_registry.hpp"
#include "gui/gui_state.hpp"
#include "test_gui_shared.hpp"

namespace {

const char* const kInteract = "**/##preview_interact";

// Built from the same names the control draws, so a renamed mode is followed here rather than
// leaving a case that looks for a label no longer on screen.
std::string SegmentRef(int mode) {
  return std::string("**/") + gui::kDisplayModeNames[mode] + "##preview_display_mode";
}

// The control is part of the preview's interaction layer, which is only submitted when there is a
// picture to interact with. The upload is a GL call and so runs on the main thread.
bool g_upload_done = false;

void UploadSynthTexture(ImGuiTestContext*) {
  if (!g_upload_done) {
    InitSynthTexture();
    gui::g_preview.UploadTexture(g_synth_tex.data(), kSynthTexW, kSynthTexH);
    g_upload_done = true;
  }
}

// Reset, then wait for the texture. Returns false (after reporting) when the precondition did not
// hold, so a caller can stop before asserting on a panel that was never drawn.
bool BringUpPreview(ImGuiTestContext* ctx) {
  ResetTestState();
  g_upload_done = false;
  ctx->Yield(4);
  IM_CHECK_RETV(gui::g_preview.HasTexture(), false);
  IM_CHECK_RETV(ctx->ItemExists(SegmentRef(0).c_str()), false);
  return true;
}

}  // namespace

void RegisterPreviewDisplayModeControlTests(ImGuiTestEngine* engine) {
  // No picture, no control: it belongs to the interaction layer, which an empty preview does not
  // have. The other half of BringUpPreview's precondition, so that one cannot pass vacuously.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_display_mode", "an_empty_preview_has_no_display_mode_control");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(3);
      IM_CHECK(!gui::g_preview.HasTexture());
      IM_CHECK(!ctx->ItemExists(SegmentRef(0).c_str()));
      IM_CHECK(!ctx->ItemExists(SegmentRef(1).c_str()));
    };
  }

  // AC1. Each segment writes its mode, the frame after publishes it to the shader's parameters, and
  // the document is not dirtied — the mode is a display-time field. Both directions, so a control
  // that could only switch ON would fail the second half.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_display_mode", "a_segment_click_switches_the_mode_without_dirtying");
    t->GuiFunc = UploadSynthTexture;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      if (!BringUpPreview(ctx)) {
        return;
      }
      IM_CHECK_EQ(gui::g_state.renderer.display_mode, LUMICE_DISPLAY_MODE_NORMAL);
      IM_CHECK(!gui::g_state.dirty);

      ctx->ItemClick(SegmentRef(LUMICE_DISPLAY_MODE_CHANNEL_BR).c_str());
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.renderer.display_mode, LUMICE_DISPLAY_MODE_CHANNEL_BR);
      IM_CHECK_EQ(gui::g_preview_vp.params.display_mode, LUMICE_DISPLAY_MODE_CHANNEL_BR);
      IM_CHECK(!gui::g_state.dirty);

      ctx->ItemClick(SegmentRef(LUMICE_DISPLAY_MODE_NORMAL).c_str());
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.renderer.display_mode, LUMICE_DISPLAY_MODE_NORMAL);
      IM_CHECK_EQ(gui::g_preview_vp.params.display_mode, LUMICE_DISPLAY_MODE_NORMAL);
      IM_CHECK(!gui::g_state.dirty);
    };
  }

  // AC2. Under Print both segments are greyed, the registry names a reason (the text the tooltip
  // prints), and a click writes nothing. Back under Screen they are live again — so the grey is the
  // gate reading the live tone, not a control that is simply always disabled.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_display_mode", "print_greys_the_control_and_keeps_the_value");
    t->GuiFunc = UploadSynthTexture;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      if (!BringUpPreview(ctx)) {
        return;
      }
      gui::g_state.renderer.display_mode = LUMICE_DISPLAY_MODE_CHANNEL_BR;
      gui::g_state.renderer.tone = LUMICE_TONE_PRINT;
      ctx->Yield(3);

      IM_CHECK(IsDisabled(ctx->ItemInfo(SegmentRef(0).c_str())));
      IM_CHECK(IsDisabled(ctx->ItemInfo(SegmentRef(1).c_str())));
      const gui::FieldEditorConstraint c = gui::ConstraintFor("renderer.display_mode", gui::g_state);
      IM_CHECK(!c.enabled);
      IM_CHECK(c.disabled_reason != nullptr && c.disabled_reason[0] != '\0');

      // Hover for real so this frame's disabled-tooltip path runs, then press: nothing is written.
      ctx->MouseMove(SegmentRef(LUMICE_DISPLAY_MODE_NORMAL).c_str(), ImGuiTestOpFlags_NoCheckHoveredId);
      ctx->MouseClick(ImGuiMouseButton_Left);
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.renderer.display_mode, LUMICE_DISPLAY_MODE_CHANNEL_BR);

      gui::g_state.renderer.tone = LUMICE_TONE_SCREEN;
      ctx->Yield(3);
      IM_CHECK(!IsDisabled(ctx->ItemInfo(SegmentRef(0).c_str())));
      IM_CHECK(!IsDisabled(ctx->ItemInfo(SegmentRef(1).c_str())));
      IM_CHECK_EQ(gui::g_state.renderer.display_mode, LUMICE_DISPLAY_MODE_CHANNEL_BR);
    };
  }

  // AC3, the press half. A drag that STARTS on the control moves neither the camera nor anything
  // else the viewport owns. A drag of the same size on the viewport itself does orbit — the
  // positive control, without which "the pose did not move" could mean the drag never happened.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "preview_display_mode", "a_drag_from_the_control_does_not_orbit_the_camera");
    t->GuiFunc = UploadSynthTexture;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      if (!BringUpPreview(ctx)) {
        return;
      }
      gui::g_state.renderer.lens_type = gui::kLensTypeFisheyeEquidist;
      gui::g_state.renderer.fov = 120.0f;
      gui::g_state.renderer.azimuth = 10.0f;
      gui::g_state.renderer.elevation = 20.0f;
      ctx->Yield(3);

      ctx->ItemDragWithDelta(SegmentRef(LUMICE_DISPLAY_MODE_NORMAL).c_str(), ImVec2(-60.0f, 40.0f));
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.renderer.azimuth, 10.0f);
      IM_CHECK_EQ(gui::g_state.renderer.elevation, 20.0f);

      ctx->ItemDragWithDelta(kInteract, ImVec2(-60.0f, 40.0f));
      ctx->Yield(2);
      IM_CHECK_NE(gui::g_state.renderer.azimuth, 10.0f);
    };
  }

  // AC3, the hover half. A wheel notch over the control leaves the field of view alone, while the
  // same notch over the viewport changes it (positive control). This is the case the explicit mask
  // exists for: without it the interaction button still reads as hovered under the control.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_display_mode", "a_wheel_over_the_control_does_not_zoom");
    t->GuiFunc = UploadSynthTexture;
    t->TestFunc = [](ImGuiTestContext* ctx) {
      if (!BringUpPreview(ctx)) {
        return;
      }
      gui::g_state.renderer.lens_type = gui::kLensTypeFisheyeEquidist;
      gui::g_state.renderer.fov = 120.0f;
      ctx->Yield(3);

      ctx->MouseMove(SegmentRef(LUMICE_DISPLAY_MODE_CHANNEL_BR).c_str());
      ctx->MouseWheelY(1.0f);
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.renderer.fov, 120.0f);

      ctx->MouseMove(kInteract);
      ctx->MouseWheelY(1.0f);
      ctx->Yield(2);
      IM_CHECK_LT(gui::g_state.renderer.fov, 120.0f);
    };
  }

  // AC4. The right panel no longer carries a display-mode row: one field, one entry point in the
  // main window. The Settings panel and the Summary page still present the field through the
  // registry, which their own suites cover.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "preview_display_mode", "the_right_panel_has_no_display_mode_row");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      ctx->Yield(3);
      IM_CHECK(ctx->ItemExists("//##RightPanel/Mode##display_tone"));  // the neighbour is still there
      IM_CHECK(!ctx->ItemExists("//##RightPanel/Show As##display_mode"));
    };
  }
}
