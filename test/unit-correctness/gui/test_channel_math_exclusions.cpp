// The GUI half of "the channel-B-R display mode takes over the colour channel", asked the questions
// that need no window: which controls the mode greys, which gate greys the mode itself, and the one
// predicate every exclusion reads.
//
// The CLI half — the composite the server declines to produce and the notices it emits — is
// test/unit-correctness/server/test_channel_math_colour_exclusions.cpp. Whether the preview's
// pixels honour the mode is test/gui/parity/test_gui_preview_export_parity.cpp and
// test/gui/parity/test_gui_cli_export_parity.cpp.
//
// Every case carries its control arm, for the reason the print file gives: the gate is one `if` on
// a field that is 0 everywhere else, so "it did not fire" is the half that is easy to get right by
// accident.

#include <gtest/gtest.h>

#include <string>

#include "gui/field_editor_registry.hpp"
#include "gui/gui_state.hpp"
#include "include/lumice.h"

namespace gui = lumice::gui;

namespace {

gui::GuiState StateWith(int display_mode, int tone) {
  gui::GuiState state;
  state.renderer.display_mode = display_mode;
  state.renderer.tone = tone;
  return state;
}

bool Mentions(const char* haystack, const char* needle) {
  return haystack != nullptr && std::string(haystack).find(needle) != std::string::npos;
}

}  // namespace

// "On screen", not merely "selected": under print the mode is inert, so every exclusion it imposes
// has to lift with it — the background photo and the composite are print's to decide then.
TEST(ChannelMathExclusionsGui, TheDiagnosticIsOnScreenOnlyUnderScreenTone) {
  EXPECT_TRUE(gui::IsChannelBrDisplay(StateWith(LUMICE_DISPLAY_MODE_CHANNEL_BR, LUMICE_TONE_SCREEN).renderer));
  EXPECT_FALSE(gui::IsChannelBrDisplay(StateWith(LUMICE_DISPLAY_MODE_CHANNEL_BR, LUMICE_TONE_PRINT).renderer));
  EXPECT_FALSE(gui::IsChannelBrDisplay(StateWith(LUMICE_DISPLAY_MODE_NORMAL, LUMICE_TONE_SCREEN).renderer));
}

// The combo is greyed under print, with a reason that names why, and live under screen.
TEST(ChannelMathExclusionsGui, TheDisplayModeComboIsInapplicableUnderPrint) {
  const auto under_print = gui::ConstraintFor("renderer.display_mode", StateWith(0, LUMICE_TONE_PRINT));
  EXPECT_FALSE(under_print.enabled);
  EXPECT_TRUE(Mentions(under_print.disabled_reason, "Print")) << under_print.disabled_reason;
  EXPECT_TRUE(gui::ConstraintFor("renderer.display_mode", StateWith(0, LUMICE_TONE_SCREEN)).enabled);
}

// The background photo is excluded while the diagnostic is on, and the reason names the mode rather
// than falling through to "no image is loaded" — which is what this test binary would otherwise
// see, having no photo. Under print the print reason wins, as the gate orders them.
TEST(ChannelMathExclusionsGui, TheBackgroundPhotoIsInapplicableUnderTheDiagnostic) {
  const auto channel = gui::ConstraintFor("bg_show", StateWith(LUMICE_DISPLAY_MODE_CHANNEL_BR, LUMICE_TONE_SCREEN));
  EXPECT_FALSE(channel.enabled);
  EXPECT_TRUE(Mentions(channel.disabled_reason, "Channel B-R")) << channel.disabled_reason;

  const auto normal = gui::ConstraintFor("bg_show", StateWith(LUMICE_DISPLAY_MODE_NORMAL, LUMICE_TONE_SCREEN));
  EXPECT_FALSE(Mentions(normal.disabled_reason, "Channel B-R")) << normal.disabled_reason;

  const auto print = gui::ConstraintFor("bg_show", StateWith(LUMICE_DISPLAY_MODE_CHANNEL_BR, LUMICE_TONE_PRINT));
  EXPECT_FALSE(print.enabled);
  EXPECT_TRUE(Mentions(print.disabled_reason, "Print")) << print.disabled_reason;
}

// Annotation colours are deliberately NOT gated by this mode — the overlays are drawn on top of the
// diagnostic in their own colours — which is the opposite of print's instance 4.
TEST(ChannelMathExclusionsGui, OverlayColoursStayEditable) {
  const gui::GuiState channel = StateWith(LUMICE_DISPLAY_MODE_CHANNEL_BR, LUMICE_TONE_SCREEN);
  for (const char* key : { "overlay_horizon_color", "overlay_grid_color", "overlay_sun_circles_color" }) {
    EXPECT_TRUE(gui::ConstraintFor(key, channel).enabled) << key;
  }
}
