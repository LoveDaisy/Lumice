#pragma once

#include "imgui.h"

namespace lumice::gui {

// Single owner of the GUI's visual language: ImGui style baseline plus the font
// atlas (body font + merged FontAwesome icon glyphs).
//
// Both the product (src/gui/main.cpp) and the test harness
// (test/gui/test_gui_main.cpp) call this and nothing else, so a screenshot taken
// by gui_test is evidence about the real app's appearance rather than a
// coincidence of two independently maintained initialization paths.
//
// Must be called after ImGui::CreateContext(), before the render loop — and again, at any frame
// boundary, when the UI scale changes (the caller then owns re-uploading the font texture: this
// file has no GL dependency, see main.cpp's g_content_scale_dirty path).
//
// The two scale parameters are the whole of the GUI's DPI story, and they are two rather than
// one because GLFW's coordinate space differs by platform:
//   layout_scale   — multiplies every layout size: ImGuiStyle (ScaleAllSizes), the body font's
//                    SizePixels, and every screen-pixel constant that reaches ImGui through
//                    UiPx()/UiPxI() below. On Windows/Linux (window coordinates ARE physical
//                    pixels) this is monitor content scale × the user's multiplier; on macOS
//                    (window coordinates are points, the OS already scales them) it is the user's
//                    multiplier alone.
//   raster_density — ImFontConfig::RasterizerDensity: rasterizes the atlas at a higher pixel
//                    density WITHOUT changing any metric, so glyphs stay sharp on a backing store
//                    denser than the coordinate space. macOS Retina passes the content scale here;
//                    Windows/Linux pass 1.0, since layout_scale already covers it there.
// Idempotent: every call re-derives style and atlas from the StyleColorsDark() baseline, so
// calling it twice with different scales never compounds (pinned by test_theme_scale.cpp).
void ApplyVisualLanguage(ImGuiIO& io, float layout_scale, float raster_density);

// The layout_scale the last ApplyVisualLanguage call installed (1.0 before any call). This is the
// number the docs call `ui_scale`.
float CurrentUiScale();

// The single screen-pixel outlet. Every layout size that was once a 1x design-basis literal — panel
// widths, column widths, gaps, hit radii, modal minimums — takes its value through here at the
// point of use, so `logical_px` is the number as designed at 96 dpi and the result is what ImGui
// is handed. Two things deliberately do NOT go through it: ImGuiStyle fields (ScaleAllSizes
// already scaled them — wrapping one would scale it twice), and any pixel quantity that lives in
// a RENDERED IMAGE rather than in the chrome (FBO resolutions, overlay-label padding in the export
// pipeline) — those are not screen layout and must stay put.
float UiPx(float logical_px);
int UiPxI(int logical_px);

// The ImGuiCond for a floating window's SetNextWindowSize (and, when the caller also wants to
// recentre, SetNextWindowPos) that must resize on the frame the UI scale changes, not just on the
// window's ordinary first-appearance frame: a window already open when the scale changes is
// showing content that grew or shrank through UiPx() while its own ImGui-tracked size did not
// move, since `first_use_cond` (Appearing for a popup that reopens every time, FirstUseEver for a
// window ImGui keeps across frames) only fires when ImGui creates the window, not when a live one
// is merely re-submitted.
//
// `tracked_scale` is the CALLER's own `static float` — per window, not shared — since the state
// being tracked ("what scale was this window last sized for") is a property of that window, not
// of the outlet. Pass a fresh `static float = 0.0f`; the very first call, at any real (nonzero)
// scale, then reads as a change and returns Always rather than `first_use_cond` — harmlessly, since
// on that same first frame the window has no ImGui-tracked size yet for either cond to override,
// so the two behave identically there. Every later call at an unchanged scale returns
// `first_use_cond` as intended.
//
// Exhaustive audit of every SetNextWindowSize()/SetNextWindowSizeConstraints() call in src/gui/
// (`grep -rn "SetNextWindowSize" src/gui/*.cpp`, cross-checked against every
// BeginPopupModal/ImGui::Begin() call so a window with no explicit size call is not missed
// either). Three windows use this function: defaults_panel.cpp,
// analysis_panel.cpp, color_window.cpp — all three are non-modal-in-effect floating windows whose
// ImGui-tracked size is set only on the appearing/first-use frame and can otherwise stay open
// indefinitely. Every remaining SetNextWindow* call in src/gui/ is exempt, for one of two
// independently-immune mechanisms, neither of which this function is needed for:
//   - ImGuiWindowFlags_AlwaysAutoResize with no explicit width/height passed via SetNextWindowSize
//     (or with height passed as 0, which ImGui never treats as "set by API" — see imgui.cpp's
//     window_size_*_set_by_api): the window's SizeFull is recomputed from its own content's ideal
//     size EVERY frame, not just on appearance, so it already tracks a scale change on the very
//     next frame. Covers edit_modals.cpp's "Custom Spectrum" modal (SetNextWindowSize's width hint
//     only survives the true appearing frame; height is 0 and always auto-fit) and every
//     AlwaysAutoResize confirmation popup in app_panels.cpp (Import Warning, Overwrite Config
//     File, Warning, Unsaved Changes, Save Modified Config — none of which call SetNextWindowSize
///    at all).
//   - SetNextWindowSizeConstraints() called unconditionally every frame the window is open (not
//     gated by any Cond — ImGui has no cond parameter for constraints): the min/max bound is
//     re-derived from UiPx() every frame and ImGui clamps the live window size into that bound at
//     every Begin(), so a scale increase grows the window on its very next frame without needing
//     an Always-cond SetNextWindowSize. Covers config_summary_window.cpp's Summary window (paired
//     with AlwaysAutoResize, belt-and-braces) and edit_modals.cpp's "Edit Entry" modal (also paired
//     with AlwaysAutoResize on its Staged/BeginPopupModal path).
//   - The five fixed chrome panels (TopBar/LeftPanel/RightPanel/PreviewPanel/StatusBar/LogPanel,
//     app_panels.cpp via the local SetNextPanelGeometry helper) call SetNextWindowSize with no
//     Cond argument at all, which ImGui treats as Always — and they do so unconditionally on every
//     single frame of the main render loop, not behind an "if just opened" gate, so there is no
//     stale frame for a scale change to be missed on.
ImGuiCond WindowResizeCondForScale(float& tracked_scale, ImGuiCond first_use_cond);

// The colour+geometry half of ApplyVisualLanguage: grid spacing plus the palette, applied to a
// caller-supplied style rather than to ImGui::GetStyle(). Split out so a test can hand it a
// scratch ImGuiStyle and ask what it wrote — the "was this slot claimed" question needs a style
// it owns, and it must reach the very code path the app uses, not a copy of it
// (test/gui/functional/test_theme_coverage.cpp).
//
// It deliberately does NOT call ImGui::StyleColorsDark() first: the coverage test fills its
// scratch style with a sentinel and requires none of it to survive, which is only a question
// about this function if the baseline preset is not run inside it.
void ApplyStyle(ImGuiStyle& style);

// The accent colour as it is CURRENTLY INSTALLED in the live style, optionally at a different
// alpha. This is the one place that knows how to read it back: ApplyPalette writes Palette::accent
// into several slots and ImGuiCol_CheckMark is one of them, so asking the style is both cheaper
// than exporting the Palette and — the part that matters — it follows whatever palette is actually
// in force rather than a compile-time constant.
//
// It exists because "borrow the accent here" was being spelled out at call sites as a literal blue
// that no longer moved when the palette did. A call site that wants the accent says so; the
// coupling to a particular ImGuiCol_ slot lives here and nowhere else.
ImVec4 AccentColor(float alpha = 1.0f);

// Draws ImGui::Checkbox, then — because the theme's FrameBorderSize is 0 — adds a
// low-contrast inset border around an UNCHECKED box so it stays legible against the panel
// background. A checked box is already filled with the accent colour and needs no such aid.
//
// Scope note: theme.cpp only houses widget-level compensations that are direct consequences
// of FrameBorderSize=0. If a future borderless-widget legibility issue is unrelated to that
// decision, it belongs in a separate widgets module, not here — and so does this one once a
// second such compensation appears (two is the point at which the pair, not the theme, is
// the thing being maintained).
bool Checkbox(const char* label, bool* v);

// Checkbox() above, for a value DERIVED from several booleans — an "All" row over a column of
// switches. `all_true` is the AND of them and `mixed` says whether the OR disagrees with it; the
// box then draws ImGui's filled square (ImGuiItemFlags_MixedValue) instead of a tick or nothing.
//
// No click rule of its own, on purpose. ImGui's Checkbox flips the value it was handed, and the
// value handed in is the AND — so a mixed box (AND false) turns everything ON, an all-on box turns
// everything off, and an all-off box turns everything on: the usual select-all convention falls out
// of passing the AND rather than the OR, with no state machine to explain "what does clicking
// 'some' do". The caller fans the returned `*all_true` back out to the source booleans.
//
// A wrapper around Checkbox() rather than a sibling of it: the inset border an unchecked box gets
// under FrameBorderSize=0 is the same aid a mixed box (also unchecked, as ImGui reads it) needs.
bool TriStateCheckbox(const char* label, bool* all_true, bool mixed);

}  // namespace lumice::gui
