#ifndef LUMICE_GUI_ANALYZE_WORKSPACE_HPP
#define LUMICE_GUI_ANALYZE_WORKSPACE_HPP

// The Analyze workspace's four-column shell (the analyze-gui-v1 plan's M1-2): the frame layout
// drawn when GuiState::workspace selects Analyze, in place of the Render-mode left/right/preview
// panels. One scene document, one set of editors — the left column hosts the crystal list and the
// Sun/Simulation controls through the very functions the Render workspace uses
// (RenderSceneControls, AddScatteringLayer, ProcessEditRequest), so there is no second editing
// semantics to keep in step.
//
// Layout (the plan's R2, the four-column skeleton at any width):
//   left 300   — crystal list (compact rows, each opening the entry editor modal the cards open)
//                + Sun + Simulation
//   middle     — two columns x two rows; upper = scene panorama | raypath share list, lower =
//                raypath view | raypath 3D. One horizontal divider across both columns adjusts
//                the upper/lower split (GuiState::AnalyzeWorkspaceSession::middle_rows_ratio,
//                session memory). In this slice the four regions are honest not-wired placeholders
//                (the share list lives in the Raypath Analysis window until its migration); the
//                panorama and the real list land in the plan's M2.
//   right 400  — sun-direction globe placeholder above the selection-details placeholder (the
//                class-level overview consumes the survey query in the plan's M3).
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

// The Analyze workspace's frame layout. `window` is the host GLFW window (unused by the
// placeholders, part of the Render*Panel signature family the frame loop calls).
void RenderAnalyzeWorkspace(GLFWwindow* window, float window_width, float window_height);

}  // namespace lumice::gui

#endif  // LUMICE_GUI_ANALYZE_WORKSPACE_HPP
