// What the Screenshot export options do to the render parameters, with no frame and no GL.
//
// The options popup (Save > Screenshot...) lets a user leave families out of one export. Its three
// promises are all properties of the functions pinned here, not of the popup:
//   - untouched, it IS the screen: the prefilled selection maps the preview's params onto
//     themselves, field for field — the "all ticked exports exactly what it did before" claim;
//   - it only subtracts: a family the panel has off stays off even when the selection bit is set
//     (the popup greys the box, but the clamp is here, so a wrong popup cannot add a line);
//   - lines and labels are separate bits (the removed "Include Overlay in Screenshot" gated the text
//     only — the labels half is pinned in the composition test beside this one).
// The display mode is the exception to subtraction: either mode may be exported, but only one the
// uploaded texture can draw, and a mode change carries the background photo's visibility with it.
//
// What a user sees when this breaks: an unticked horizon still in the PNG, or a line the screen was
// not showing appearing in it; a B-R export with the photo showing through it; a "B-R" export of a
// colored composite that measures the palette instead of the light.

#include <gtest/gtest.h>

#include <array>

#include "gui/gui_state.hpp"
#include "gui/preview_renderer.hpp"
#include "gui/screenshot_export_options.hpp"
#include "lumice.h"

namespace gui = lumice::gui;

namespace {

// Every overlay switch on, every marker ring and name on: the state in which a selection has the
// most to take away.
gui::GuiState AllOnState() {
  gui::GuiState state;
  state.show_horizon_line = true;
  state.show_horizon_label = true;
  state.show_grid_line = true;
  state.show_grid_label = true;
  state.show_sun_circles_line = true;
  state.show_sun_circles_label = true;
  state.show_view_dist_line = true;
  state.show_view_dist_label = true;
  state.show_lens_border_line = true;
  for (gui::MarkerAppearance& m : state.markers) {
    m.show = true;
    m.label = true;
  }
  return state;
}

// The params the preview would hold for `state`: the line flags copied from the panel (as
// app_panels.cpp does), each marker at a distinct on-canvas position when its ring is on, and a
// background photo in the frame. Distinct marker positions so "kept" cannot be confused with a
// default value.
gui::PreviewParams ParamsFor(const gui::GuiState& state) {
  gui::PreviewParams params;
  params.overlay.show_horizon = state.show_horizon_line;
  params.overlay.show_grid = state.show_grid_line;
  params.overlay.show_sun_circles = state.show_sun_circles_line;
  params.overlay.show_view_dist = state.show_view_dist_line;
  params.overlay.show_lens_border = state.show_lens_border_line;
  params.overlay.marker_screen_pos = gui::MakeAllSentinelMarkerPositions();
  for (int i = 0; i < LUMICE_ANNOTATION_MARKER_COUNT; ++i) {
    if (state.markers[i].show) {
      params.overlay.marker_screen_pos[i] = { 10.0f * static_cast<float>(i + 1), -5.0f * static_cast<float>(i + 1) };
    }
  }
  params.display_mode = state.renderer.display_mode;
  params.bg.enabled = gui::BgPhotoInFrame(true, state.bg_show, state.renderer);
  return params;
}

// No operator== on PreviewParams; these are the fields the selection may touch, compared one by
// one so a failure names the field.
void ExpectSameSelectableFields(const gui::PreviewParams& got, const gui::PreviewParams& want) {
  EXPECT_EQ(got.overlay.show_horizon, want.overlay.show_horizon);
  EXPECT_EQ(got.overlay.show_grid, want.overlay.show_grid);
  EXPECT_EQ(got.overlay.show_sun_circles, want.overlay.show_sun_circles);
  EXPECT_EQ(got.overlay.show_view_dist, want.overlay.show_view_dist);
  EXPECT_EQ(got.overlay.show_lens_border, want.overlay.show_lens_border);
  EXPECT_EQ(got.overlay.marker_screen_pos, want.overlay.marker_screen_pos);
  EXPECT_EQ(got.display_mode, want.display_mode);
  EXPECT_EQ(got.bg.enabled, want.bg.enabled);
}

constexpr gui::ScreenshotFrameFacts kPhotoNoComposite{ /*has_background=*/true, /*payload_is_composite=*/false };

}  // namespace

// The prefill is the panel, bit for bit — in two complementary patterns, so a bit wired to the
// wrong switch reads wrong in one of them.
TEST(ScreenshotExportOptions, PrefillReadsEveryPanelSwitch) {
  for (int pattern = 0; pattern < 2; ++pattern) {
    const bool a = pattern == 0;
    gui::GuiState state;
    state.show_horizon_line = a;
    state.show_horizon_label = !a;
    state.show_grid_line = !a;
    state.show_grid_label = a;
    state.show_sun_circles_line = a;
    state.show_sun_circles_label = !a;
    state.show_view_dist_line = !a;
    state.show_view_dist_label = a;
    state.show_lens_border_line = a;
    for (gui::MarkerAppearance& m : state.markers) {
      m.show = false;
      m.label = false;
    }
    // One marker's ring or one marker's name is enough to put its family on screen.
    state.markers[2].show = a;
    state.markers[4].label = !a;
    state.renderer.display_mode = a ? LUMICE_DISPLAY_MODE_CHANNEL_BR : 0;

    const gui::ScreenshotExportSelection sel = gui::MakeScreenshotExportSelectionFromState(state);
    EXPECT_EQ(sel.horizon_line, a) << "pattern " << pattern;
    EXPECT_EQ(sel.horizon_label, !a) << "pattern " << pattern;
    EXPECT_EQ(sel.grid_line, !a) << "pattern " << pattern;
    EXPECT_EQ(sel.grid_label, a) << "pattern " << pattern;
    EXPECT_EQ(sel.sun_circles_line, a) << "pattern " << pattern;
    EXPECT_EQ(sel.sun_circles_label, !a) << "pattern " << pattern;
    EXPECT_EQ(sel.view_dist_line, !a) << "pattern " << pattern;
    EXPECT_EQ(sel.view_dist_label, a) << "pattern " << pattern;
    EXPECT_EQ(sel.lens_border_line, a) << "pattern " << pattern;
    EXPECT_EQ(sel.markers_line, a) << "pattern " << pattern;
    EXPECT_EQ(sel.markers_label, !a) << "pattern " << pattern;
    EXPECT_EQ(sel.display_mode, state.renderer.display_mode) << "pattern " << pattern;
  }
}

// Untouched, the selection is the identity on the preview's params — with everything on, and with
// a mixed panel (the prefill carries the offs, and applying them changes nothing either).
TEST(ScreenshotExportOptions, PrefilledSelectionLeavesTheParamsAsTheScreenHasThem) {
  gui::GuiState all_on = AllOnState();
  gui::GuiState mixed = AllOnState();
  mixed.show_grid_line = false;
  mixed.show_sun_circles_label = false;
  mixed.markers[1].show = false;
  mixed.markers[3].label = false;
  mixed.renderer.display_mode = LUMICE_DISPLAY_MODE_CHANNEL_BR;

  for (const gui::GuiState* state : { &all_on, &mixed }) {
    const gui::PreviewParams before = ParamsFor(*state);
    gui::PreviewParams after = before;
    gui::ApplyScreenshotExportSelectionToParams(gui::MakeScreenshotExportSelectionFromState(*state), *state,
                                                kPhotoNoComposite, after);
    ExpectSameSelectableFields(after, before);
  }
}

// Clearing one line bit drops that family's line and nothing else.
TEST(ScreenshotExportOptions, ClearingALineBitDropsOnlyThatFamily) {
  const gui::GuiState state = AllOnState();
  const gui::PreviewParams screen = ParamsFor(state);
  struct Case {
    const char* name;
    bool gui::ScreenshotExportSelection::*bit;
    bool gui::OverlayDecoration::*flag;
  };
  const Case cases[] = {
    { "horizon", &gui::ScreenshotExportSelection::horizon_line, &gui::OverlayDecoration::show_horizon },
    { "grid", &gui::ScreenshotExportSelection::grid_line, &gui::OverlayDecoration::show_grid },
    { "sun circles", &gui::ScreenshotExportSelection::sun_circles_line, &gui::OverlayDecoration::show_sun_circles },
    { "view circles", &gui::ScreenshotExportSelection::view_dist_line, &gui::OverlayDecoration::show_view_dist },
    { "lens border", &gui::ScreenshotExportSelection::lens_border_line, &gui::OverlayDecoration::show_lens_border },
  };
  for (const Case& c : cases) {
    gui::ScreenshotExportSelection sel = gui::MakeScreenshotExportSelectionFromState(state);
    sel.*c.bit = false;
    gui::PreviewParams params = screen;
    gui::ApplyScreenshotExportSelectionToParams(sel, state, kPhotoNoComposite, params);
    gui::PreviewParams want = screen;
    want.overlay.*c.flag = false;
    SCOPED_TRACE(c.name);
    ExpectSameSelectableFields(params, want);
  }
}

// The clamp is here, not in the popup: a selection bit set for a family the panel has off cannot
// bring it back — even when the params handed in claim it is on.
TEST(ScreenshotExportOptions, ASetBitCannotAddAFamilyThePanelHasOff) {
  gui::GuiState state;
  state.show_horizon_line = false;
  state.show_grid_line = false;
  state.show_sun_circles_line = false;
  state.show_view_dist_line = false;
  state.show_lens_border_line = false;
  for (gui::MarkerAppearance& m : state.markers) {
    m.show = false;
  }
  gui::PreviewParams params = ParamsFor(AllOnState());  // a caller that got the params wrong
  gui::ApplyScreenshotExportSelectionToParams(gui::ScreenshotExportSelection{}, state, kPhotoNoComposite, params);
  EXPECT_FALSE(params.overlay.show_horizon);
  EXPECT_FALSE(params.overlay.show_grid);
  EXPECT_FALSE(params.overlay.show_sun_circles);
  EXPECT_FALSE(params.overlay.show_view_dist);
  EXPECT_FALSE(params.overlay.show_lens_border);
  EXPECT_EQ(params.overlay.marker_screen_pos, gui::MakeAllSentinelMarkerPositions());
}

// The reference points are one family: clearing its line bit drops every ring; keeping it keeps
// each ring exactly as its own switch put it, including the ones switched off.
TEST(ScreenshotExportOptions, ReferencePointRingsGoAsOneFamilyAndKeepTheirOwnSwitches) {
  gui::GuiState state = AllOnState();
  state.markers[0].show = false;
  const gui::PreviewParams screen = ParamsFor(state);

  gui::ScreenshotExportSelection keep = gui::MakeScreenshotExportSelectionFromState(state);
  gui::PreviewParams kept = screen;
  gui::ApplyScreenshotExportSelectionToParams(keep, state, kPhotoNoComposite, kept);
  EXPECT_EQ(kept.overlay.marker_screen_pos, screen.overlay.marker_screen_pos);
  EXPECT_EQ(kept.overlay.marker_screen_pos[0], gui::MakeAllSentinelMarkerPositions()[0])
      << "the switched-off ring came back";

  gui::ScreenshotExportSelection drop = keep;
  drop.markers_line = false;
  gui::PreviewParams dropped = screen;
  gui::ApplyScreenshotExportSelectionToParams(drop, state, kPhotoNoComposite, dropped);
  EXPECT_EQ(dropped.overlay.marker_screen_pos, gui::MakeAllSentinelMarkerPositions());
  EXPECT_TRUE(dropped.overlay.show_horizon) << "a ring bit reached another family";
}

// Either display mode may be exported from either, and the photo follows the mode it is exported
// in: hidden under B-R, back under Normal — the rule the preview frame applies (BgPhotoInFrame).
TEST(ScreenshotExportOptions, TheDisplayModeIsAFreeChoiceAndCarriesThePhotoWithIt) {
  gui::GuiState normal = AllOnState();
  normal.bg_show = true;
  gui::GuiState br = normal;
  br.renderer.display_mode = LUMICE_DISPLAY_MODE_CHANNEL_BR;

  gui::ScreenshotExportSelection to_br = gui::MakeScreenshotExportSelectionFromState(normal);
  to_br.display_mode = LUMICE_DISPLAY_MODE_CHANNEL_BR;
  gui::PreviewParams from_normal = ParamsFor(normal);
  ASSERT_TRUE(from_normal.bg.enabled) << "premise: the photo is on the Normal screen";
  gui::ApplyScreenshotExportSelectionToParams(to_br, normal, kPhotoNoComposite, from_normal);
  ExpectSameSelectableFields(from_normal, ParamsFor(br));

  gui::ScreenshotExportSelection to_normal = gui::MakeScreenshotExportSelectionFromState(br);
  to_normal.display_mode = 0;
  gui::PreviewParams from_br = ParamsFor(br);
  ASSERT_FALSE(from_br.bg.enabled) << "premise: the photo is off the B-R screen";
  gui::ApplyScreenshotExportSelectionToParams(to_normal, br, kPhotoNoComposite, from_br);
  ExpectSameSelectableFields(from_br, ParamsFor(normal));
}

// The texture on screen was uploaded for the CURRENT mode: while a colored composite is being
// previewed, the Normal texture is the composite and the B-R one is the full spectrum, so the other
// mode cannot be drawn from what is uploaded and the export keeps the screen's mode.
TEST(ScreenshotExportOptions, AModeTheUploadedTextureCannotDrawIsNotExported) {
  constexpr gui::ScreenshotFrameFacts kComposite{ /*has_background=*/false, /*payload_is_composite=*/true };
  gui::GuiState normal;
  normal.show_composite_preview = true;
  normal.last_uploaded_as_composite = true;
  gui::GuiState br = normal;
  br.renderer.display_mode = LUMICE_DISPLAY_MODE_CHANNEL_BR;
  br.last_uploaded_as_composite = false;

  EXPECT_TRUE(gui::ScreenshotDisplayModeRenderable(normal, kComposite, 0));
  EXPECT_FALSE(gui::ScreenshotDisplayModeRenderable(normal, kComposite, LUMICE_DISPLAY_MODE_CHANNEL_BR));
  EXPECT_TRUE(gui::ScreenshotDisplayModeRenderable(br, kComposite, LUMICE_DISPLAY_MODE_CHANNEL_BR));
  EXPECT_FALSE(gui::ScreenshotDisplayModeRenderable(br, kComposite, 0));

  gui::ScreenshotExportSelection sel = gui::MakeScreenshotExportSelectionFromState(normal);
  sel.display_mode = LUMICE_DISPLAY_MODE_CHANNEL_BR;
  gui::PreviewParams params = ParamsFor(normal);
  gui::ApplyScreenshotExportSelectionToParams(sel, normal, kComposite, params);
  EXPECT_EQ(params.display_mode, 0);

  // No composite in play — a mono payload, or the composite preview switched off — and both modes
  // draw from the one full-spectrum texture.
  gui::GuiState mono = normal;
  mono.show_composite_preview = false;
  mono.last_uploaded_as_composite = false;
  for (int mode = 0; mode < gui::kDisplayModeCount; ++mode) {
    EXPECT_TRUE(gui::ScreenshotDisplayModeRenderable(mono, kComposite, mode)) << "mode " << mode;
    EXPECT_TRUE(gui::ScreenshotDisplayModeRenderable(mono, kPhotoNoComposite, mode)) << "mode " << mode;
  }
  EXPECT_FALSE(gui::ScreenshotDisplayModeRenderable(mono, kPhotoNoComposite, gui::kDisplayModeCount));
}
