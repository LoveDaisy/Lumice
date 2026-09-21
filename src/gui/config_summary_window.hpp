#ifndef LUMICE_GUI_CONFIG_SUMMARY_WINDOW_HPP
#define LUMICE_GUI_CONFIG_SUMMARY_WINDOW_HPP

// The read-only "Summary" window: the page config_summary.hpp builds, drawn as one fixed-width,
// scrollable ImGui window for the user to screenshot and share. Floating and non-modal like the
// Colors and Raypath Analysis windows; opened from the top bar's feature-button group, closed by
// its own X or by any document reset (app.cpp ResetFrontendState).
//
// No control in it edits anything. A control would make it a second Settings panel, and its
// chrome would end up in every screenshot taken of it.

namespace lumice::gui {

struct GuiState;

// No-op while state.config_summary_window_open is false. The window's ImGui id is
// "###ConfigSummary" (its visible title is "Summary" behind an icon); the top bar's button is
// labelled the same way, and gui_test locates both by those strings.
void RenderConfigSummaryWindow(GuiState& state);

}  // namespace lumice::gui

#endif  // LUMICE_GUI_CONFIG_SUMMARY_WINDOW_HPP
