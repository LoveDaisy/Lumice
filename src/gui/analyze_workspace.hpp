#ifndef LUMICE_GUI_ANALYZE_WORKSPACE_HPP
#define LUMICE_GUI_ANALYZE_WORKSPACE_HPP

// The Analyze workspace's frame shell: the layout drawn when GuiState::workspace selects
// Analyze, in place of the Render-mode left/right/preview panels. One scene document, one set of
// editors — the left column hosts the crystal list and the Sun/Simulation controls through the
// very functions the Render workspace uses (RenderSceneControls, AddScatteringLayer,
// ProcessEditRequest), so there is no second editing semantics to keep in step.
//
// Layout (doc/gui/analyze-interaction-contract.md §2/§4, at any window size):
//   left 300 (fixed)   — crystal list (each row opens the entry editor modal the cards open)
//                        + Sun + Simulation
//   main view column   — the scene panorama as the upper row's large main view (the committed
//                        texture through the deferred blit), below it the depth band: the map /
//                        3D / globe regions' TRANSITION host until their windows take them
//                        (755.4/.5). The band is responsive: side-by-side when its width gives
//                        each region its content floor, stacked + internal scroll otherwise.
//                        The panorama/band split is GuiState::AnalyzeWorkspaceSession::
//                        middle_rows_ratio — a session ratio that retires with the band.
//   right column       — the share list above (RenderAnalysisPanelBody's fixed head + the result
//                        table's own inner scroll), the diagnostics region below (selection
//                        identity + survey states + point block; the idle state's guidance /
//                        run status / observation summary otherwise).
//   D1 / D2            — the two new splitters (column, right-column rows), each storing a
//                        session ratio clamped at the pixel floors of gui_constants.hpp.
//
// The Render workspace keeps kLeftPanelWidth/kRightPanelWidth; the two layouts are independent,
// and nothing in the Render path is touched by this file.
//
// Level-triggered viewport note: the Render-mode preview publishes g_preview_vp (active, curve
// labels) every frame it draws. The frame that draws the workspace instead publishes neither —
// RenderAnalyzeWorkspace clears both at entry, so every consumer (DoAnalyze's IN_FRAME canvas,
// the deferred blit, the screenshot export) reads "no picture on screen" honestly.

struct GLFWwindow;

namespace lumice::gui {

struct GuiState;
class SurveyExecutor;

// The background queries' frame pumps — survey, map, point, ray — as ONE shared entry: drain
// finished outcomes through the adoption gates, then reconcile each intent against its live
// inputs. Called by the Analyze workspace's frame entry, and — since the Raypath view window
// (analyze_map_view.hpp) took the map face out of the band — by that window's own frames in
// the Render workspace, where it is the only analysis face rendering (the cross-face stance:
// the pumps run on any frame an analysis face draws; the window keeps following the current
// selection, view and lambda while it floats). Per-frame idempotent: a drain clears its slot,
// a key-match re-asks nothing, so the Analyze-mode frame's own entry call and the window's
// overlap harmlessly.
void PumpAnalyzeQueries(GuiState& state, SurveyExecutor& executor, double now_s);

// The Analyze workspace's frame layout. `window` is the host GLFW window (unused by the
// placeholders, part of the Render*Panel signature family the frame loop calls).
void RenderAnalyzeWorkspace(GLFWwindow* window, float window_width, float window_height);

}  // namespace lumice::gui

#endif  // LUMICE_GUI_ANALYZE_WORKSPACE_HPP
