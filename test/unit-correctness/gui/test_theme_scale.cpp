// The ui_scale outlet (src/gui/theme.{hpp,cpp}): ApplyVisualLanguage(io, layout_scale,
// raster_density) is the ONE place the GUI's scale is applied, and UiPx()/UiPxI()/CurrentUiScale()
// read back what it installed.
//
// gui_unit_test rather than gui_test: an ImGui context with no backend is enough — the style is a
// struct, the atlas builds on the CPU (io.Fonts->Build() is what ApplyVisualLanguage ends with), and
// nothing here needs a frame. What the cases pin:
//   - the outlet at 1.0 is the identity, in both the style and the font — the mechanical half of
//     the "default behaviour unchanged" claim the visual references then confirm pixel by pixel;
//   - the outlet at another scale reaches the font's SizePixels, not just the style, since a scaled
//     style with an unscaled atlas is a bigger frame around the same small text;
//   - a second call does not compound on the first (the regression that would make a window
//     dragged between monitors grow on every crossing).

#include <gtest/gtest.h>

#include <vector>

#include "gui/app.hpp"
#include "gui/theme.hpp"
#include "gui/ui_scale.hpp"
#include "gui/window_sizing.hpp"
#include "imgui.h"

namespace {

namespace gui = lumice::gui;

class ThemeScale : public ::testing::Test {
 protected:
  void SetUp() override { ctx_ = ImGui::CreateContext(); }
  void TearDown() override { ImGui::DestroyContext(ctx_); }

  // The style's geometry as a list of numbers: every field ScaleAllSizes touches, plus the border
  // sizes and alignments ApplyStyle sets. A field list rather than a byte image of the struct —
  // ImGuiStyle carries three bools and therefore padding, whose bytes differ between a stack
  // instance and the context's heap one for no reason a test should be reading.
  static std::vector<float> Geometry(const ImGuiStyle& s) {
    return { s.WindowPadding.x,
             s.WindowPadding.y,
             s.WindowRounding,
             s.WindowBorderSize,
             s.WindowMinSize.x,
             s.WindowMinSize.y,
             s.WindowTitleAlign.x,
             s.WindowTitleAlign.y,
             s.ChildRounding,
             s.ChildBorderSize,
             s.PopupRounding,
             s.PopupBorderSize,
             s.FramePadding.x,
             s.FramePadding.y,
             s.FrameRounding,
             s.FrameBorderSize,
             s.ItemSpacing.x,
             s.ItemSpacing.y,
             s.ItemInnerSpacing.x,
             s.ItemInnerSpacing.y,
             s.CellPadding.x,
             s.CellPadding.y,
             s.TouchExtraPadding.x,
             s.TouchExtraPadding.y,
             s.IndentSpacing,
             s.ColumnsMinSpacing,
             s.ScrollbarSize,
             s.ScrollbarRounding,
             s.GrabMinSize,
             s.GrabRounding,
             s.LogSliderDeadzone,
             s.TabRounding,
             s.TabBorderSize,
             s.TabMinWidthForCloseButton,
             s.TabBarBorderSize,
             s.TabBarOverlineSize,
             s.SeparatorTextBorderSize,
             s.SeparatorTextAlign.x,
             s.SeparatorTextAlign.y,
             s.SeparatorTextPadding.x,
             s.SeparatorTextPadding.y,
             s.DockingSeparatorSize,
             s.DisplayWindowPadding.x,
             s.DisplayWindowPadding.y,
             s.DisplaySafeAreaPadding.x,
             s.DisplaySafeAreaPadding.y,
             s.MouseCursorScale };
  }

  ImGuiContext* ctx_ = nullptr;
};

TEST_F(ThemeScale, AtOneTheOutletIsTheIdentityForStyleFontAndPixels) {
  ImGuiIO& io = ImGui::GetIO();
  // The baseline the visual references were shot against: StyleColorsDark + ApplyStyle, nothing
  // scaled. ScaleAllSizes truncates each field, so this also proves no field the theme sets is
  // fractional in a way a scale of exactly 1.0 would alter.
  ImGuiStyle baseline;
  ImGui::StyleColorsDark(&baseline);
  gui::ApplyStyle(baseline);

  // Something other than the baseline was in force before: ApplyVisualLanguage must not depend on
  // what it finds in the live style.
  ImGui::GetStyle().WindowMinSize = ImVec2(99.0f, 99.0f);
  gui::ApplyVisualLanguage(io, 1.0f, 1.0f);

  EXPECT_EQ(Geometry(ImGui::GetStyle()), Geometry(baseline));
  EXPECT_FLOAT_EQ(gui::CurrentUiScale(), 1.0f);
  EXPECT_FLOAT_EQ(gui::UiPx(100.0f), 100.0f);
  EXPECT_FLOAT_EQ(gui::UiPx(1.5f), 1.5f);
  EXPECT_EQ(gui::UiPxI(400), 400);
  ASSERT_FALSE(io.Fonts->Fonts.empty());
  EXPECT_FLOAT_EQ(io.Fonts->Fonts[0]->FontSize, 15.0f) << "the 15px body font of doc/gui-visual-language.md §4.1";
}

TEST_F(ThemeScale, ALargerScaleReachesTheFontAndThePixelOutletAlike) {
  ImGuiIO& io = ImGui::GetIO();
  gui::ApplyVisualLanguage(io, 1.5f, 1.0f);

  EXPECT_FLOAT_EQ(gui::CurrentUiScale(), 1.5f);
  EXPECT_FLOAT_EQ(gui::UiPx(100.0f), 150.0f);
  EXPECT_EQ(gui::UiPxI(400), 600);
  // Rounded, not truncated: 1.5 × 29 = 43.5 → 44, so a column width scaled up and a neighbour
  // scaled down do not drift apart by the truncation bias.
  EXPECT_EQ(gui::UiPxI(29), 44);
  ASSERT_FALSE(io.Fonts->Fonts.empty());
  // 15 × 1.5 = 22.5, rounded to 23: ImGui truncates a fractional SizePixels on its own (imgui.cpp,
  // AddFont), so theme.cpp rounds explicitly rather than let 22.5 silently become 22.
  EXPECT_FLOAT_EQ(io.Fonts->Fonts[0]->FontSize, 23.0f)
      << "the atlas is rebuilt at round(kBodyFontSizePx × layout_scale)";

  // The style scaled by the same factor as the font: ItemSpacing is (8, 3) at 1x.
  const ImGuiStyle& style = ImGui::GetStyle();
  EXPECT_FLOAT_EQ(style.ItemSpacing.x, 12.0f);
  EXPECT_FLOAT_EQ(style.IndentSpacing, 24.0f);
}

// The regression this whole file is most for. ScaleAllSizes multiplies the style it is handed,
// so an implementation that scaled the LIVE style instead of re-deriving from the baseline would
// pass both cases above and fail only on the second call — 1.5 then 1.5 again reading 2.25.
// Same for the atlas: appended fonts would leave Fonts[0] at the first size.
TEST_F(ThemeScale, ASecondCallReplacesTheFirstRatherThanCompounding) {
  ImGuiIO& io = ImGui::GetIO();
  gui::ApplyVisualLanguage(io, 1.5f, 1.0f);
  const std::vector<float> once = Geometry(ImGui::GetStyle());
  const int fonts_once = io.Fonts->Fonts.Size;

  gui::ApplyVisualLanguage(io, 1.5f, 1.0f);
  EXPECT_EQ(Geometry(ImGui::GetStyle()), once) << "same scale twice must be the same style";
  EXPECT_EQ(io.Fonts->Fonts.Size, fonts_once) << "the atlas is rebuilt, not appended to";
  EXPECT_FLOAT_EQ(io.Fonts->Fonts[0]->FontSize, 23.0f);

  gui::ApplyVisualLanguage(io, 1.0f, 1.0f);
  ImGuiStyle baseline;
  ImGui::StyleColorsDark(&baseline);
  gui::ApplyStyle(baseline);
  EXPECT_EQ(Geometry(ImGui::GetStyle()), Geometry(baseline)) << "scaling back to 1.0 lands on the baseline";
  EXPECT_FLOAT_EQ(io.Fonts->Fonts[0]->FontSize, 15.0f);
  EXPECT_FLOAT_EQ(gui::UiPx(100.0f), 100.0f);
}

// raster_density is a rasterization-only knob: the font's metrics — and therefore every layout
// size — must read exactly as they do at density 1. This is the macOS Retina contract: sharper
// glyphs, nothing larger.
TEST_F(ThemeScale, RasterDensityChangesNoMetric) {
  ImGuiIO& io = ImGui::GetIO();
  gui::ApplyVisualLanguage(io, 1.0f, 1.0f);
  const std::vector<float> style_at_one = Geometry(ImGui::GetStyle());
  const float font_at_one = io.Fonts->Fonts[0]->FontSize;
  const float advance_at_one = io.Fonts->Fonts[0]->GetCharAdvance('M');
  const int tex_area_at_one = io.Fonts->TexWidth * io.Fonts->TexHeight;

  gui::ApplyVisualLanguage(io, 1.0f, 2.0f);
  EXPECT_EQ(Geometry(ImGui::GetStyle()), style_at_one);
  EXPECT_FLOAT_EQ(io.Fonts->Fonts[0]->FontSize, font_at_one);
  // NEAR, not EQ: the advance is measured at SizePixels × density and divided back, so the last
  // float bit can move; a metric that CHANGED would be off by a factor of the density, not 1e-3.
  EXPECT_NEAR(io.Fonts->Fonts[0]->GetCharAdvance('M'), advance_at_one, 1e-3f);
  EXPECT_FLOAT_EQ(gui::UiPx(100.0f), 100.0f);
  // The one thing that IS allowed to change: the atlas holds more texels per glyph.
  EXPECT_GT(io.Fonts->TexWidth * io.Fonts->TexHeight, tex_area_at_one)
      << "sanity: the density did reach the rasterizer — a denser atlas is a larger one";
}

TEST(UiScalePolicy, MonitorAndUserInputsStayInTheirPlatformSpecificUnits) {
  const gui::UiScaleParams params = gui::ResolveUiScaleParams(/*monitor_scale=*/1.5f,
                                                              /*user_multiplier=*/1.25f);
#if defined(__APPLE__)
  EXPECT_FLOAT_EQ(params.layout_scale, 1.25f);
  EXPECT_FLOAT_EQ(params.raster_density, 1.5f);
#else
  EXPECT_FLOAT_EQ(params.layout_scale, 1.875f);
  EXPECT_FLOAT_EQ(params.raster_density, 1.0f);
#endif
}

TEST(WindowResizeState, ManualResizeSelectsFreeAndClearsTheClamp) {
  gui::g_state = {};
  gui::g_state.aspect_preset = gui::AspectPreset::k16x9;
  gui::g_state.aspect_clamp.was_clamped = true;
  gui::ResetWindowResizeEvents();

  gui::WindowSizeCallback(nullptr, /*width=*/1400, /*height=*/900);
  gui::FinishWindowEventPoll();

  EXPECT_EQ(gui::g_state.aspect_preset, gui::AspectPreset::kFree);
  EXPECT_FALSE(gui::g_state.aspect_clamp.was_clamped);
  gui::g_state = {};
}

TEST(WindowResizeState, SizeBeforeContentScalePreservesIntentUntilTheNextManualResize) {
  gui::g_state = {};
  gui::g_state.aspect_preset = gui::AspectPreset::k16x9;
  gui::ResetWindowResizeEvents();
  // GLFW Win32's WM_DPICHANGED calls SetWindowPos before its content-scale notification.
  const unsigned int revision = gui::WindowContentScaleRevision();
  gui::WindowSizeCallback(nullptr, /*width=*/1400, /*height=*/900);
  gui::NotifyWindowContentScaleChanged();
  gui::FinishWindowEventPoll();
  EXPECT_NE(gui::WindowContentScaleRevision(), revision);
  EXPECT_EQ(gui::g_state.aspect_preset, gui::AspectPreset::k16x9);
  EXPECT_TRUE(gui::g_ui_scale_dirty);
  gui::g_ui_scale_dirty = false;
  gui::WindowSizeCallback(nullptr, /*width=*/1350, /*height=*/880);
  gui::FinishWindowEventPoll();
  EXPECT_EQ(gui::g_state.aspect_preset, gui::AspectPreset::kFree);
  gui::g_state = {};
}

TEST(WindowResizeEvents, ZeroOneOrTwoSynchronousCallbacksCannotExemptTheNextManualSize) {
  for (int callback_count : { 0, 1, 2 }) {
    SCOPED_TRACE(callback_count);
    gui::WindowResizeEvents events;
    events.BeginRequest(1400, 900);
    for (int i = 0; i < callback_count; ++i) {
      EXPECT_TRUE(events.RecordResize(1400, 900));
    }
    events.EndRequest(1400, 900);
    EXPECT_FALSE(events.FinishEventPoll());
    EXPECT_FALSE(events.RecordResize(1350, 880));
    EXPECT_TRUE(events.FinishEventPoll());
  }
}

TEST(WindowResizeEvents, DelayedRequestsRemainCorrelatedWithinTheNextPoll) {
  gui::WindowResizeEvents events;
  events.BeginRequest(1400, 900);
  events.EndRequest(1600, 980);
  events.BeginRequest(1500, 940);
  events.EndRequest(1600, 980);
  EXPECT_TRUE(events.RecordResize(1400, 900));
  EXPECT_TRUE(events.RecordResize(1500, 940));
  EXPECT_FALSE(events.FinishEventPoll());
  EXPECT_FALSE(events.RecordResize(1450, 920));
  EXPECT_TRUE(events.FinishEventPoll());
}

TEST(WindowResizeEvents, UnacknowledgedRequestsCannotExemptOldReadbackOrLaterManualTargets) {
  for (int manual_width : { 1400, 1600 }) {
    SCOPED_TRACE(manual_width);
    gui::WindowResizeEvents events;
    events.BeginRequest(1400, 900);
    events.EndRequest(1600, 980);
    EXPECT_FALSE(events.FinishEventPoll());
    EXPECT_FALSE(events.FinishEventPoll());
    EXPECT_FALSE(events.RecordResize(manual_width, manual_width == 1400 ? 900 : 980));
    EXPECT_TRUE(events.FinishEventPoll());
  }

  gui::WindowResizeEvents events;
  events.BeginRequest(1400, 900);
  events.EndRequest(1600, 980);
  EXPECT_FALSE(events.RecordResize(1600, 980));
  EXPECT_TRUE(events.FinishEventPoll());
}

TEST(WindowResizeEvents, AManualSizeAfterSettlementSupersedesAnUnacknowledgedRequest) {
  gui::WindowResizeEvents events;
  events.BeginRequest(1400, 900);
  events.EndRequest(1600, 980);
  EXPECT_FALSE(events.FinishEventPoll());
  EXPECT_FALSE(events.RecordResize(1350, 880));
  EXPECT_TRUE(events.FinishEventPoll());
  EXPECT_FALSE(events.RecordResize(1400, 900));
  EXPECT_TRUE(events.FinishEventPoll());
}

TEST(WindowResizeEvents, AdjustedAsynchronousResultSettlesOnceAndDoesNotExemptTheNextManualSize) {
  gui::WindowResizeEvents events;
  events.BeginRequest(1400, 900);
  events.EndRequest(1600, 980);
  EXPECT_TRUE(events.RecordResize(1420, 910));
  EXPECT_TRUE(events.RecordResize(1420, 910));
  EXPECT_FALSE(events.FinishEventPoll());
  EXPECT_FALSE(events.RecordResize(1430, 910));
  EXPECT_TRUE(events.FinishEventPoll());

  events.BeginRequest(1400, 900);
  events.EndRequest(1600, 980);
  EXPECT_TRUE(events.RecordResize(1420, 910));
  EXPECT_FALSE(events.RecordResize(1430, 910));
  EXPECT_TRUE(events.FinishEventPoll());
}

// WindowResizeCondForScale — the shared rule behind defaults_panel.cpp/analysis_panel.cpp/
// color_window.cpp re-applying SetNextWindowSize on a scale change even though the window is
// already open (the regression a Major code-review finding caught: fixed only in defaults_panel,
// leaving the other two panels pinned at their old pixel size while their UiPx()-derived content
// grew). No ImGui context needed — it only reads/writes CurrentUiScale() and the caller's tracked
// float, never touches the live style or an ImGui window.
// The very first call ever, with the caller's static still at its zero-init, reads 0.0 != the
// current (nonzero) scale and so returns Always rather than first_use_cond — harmlessly, since a
// window with no ImGui-tracked size yet has nothing for Always to override that first_use_cond
// would not also have set. Documented here so a future change to the "!=" comparison does not
// silently start requiring callers to pre-seed `tracked_scale` at the real scale.
TEST(WindowResizeCondForScale, FirstCallEverReadsAsAChangeAndReturnsAlwaysHarmlessly) {
  ImGuiContext* ctx = ImGui::CreateContext();
  gui::ApplyVisualLanguage(ImGui::GetIO(), 1.0f, 1.0f);

  float tracked = 0.0f;  // the caller's fresh static, as every call site declares it
  EXPECT_EQ(gui::WindowResizeCondForScale(tracked, ImGuiCond_FirstUseEver), ImGuiCond_Always);
  EXPECT_FLOAT_EQ(tracked, 1.0f);
  // The very next call, at the same scale, settles into the steady state.
  EXPECT_EQ(gui::WindowResizeCondForScale(tracked, ImGuiCond_FirstUseEver), ImGuiCond_FirstUseEver);

  ImGui::DestroyContext(ctx);
}

TEST(WindowResizeCondForScale, SameScaleAgainStaysAtFirstUseCond) {
  ImGuiContext* ctx = ImGui::CreateContext();
  gui::ApplyVisualLanguage(ImGui::GetIO(), 1.5f, 1.0f);

  float tracked = 1.5f;  // already sized for the current scale, as a window left open would be
  EXPECT_EQ(gui::WindowResizeCondForScale(tracked, ImGuiCond_Appearing), ImGuiCond_Appearing);

  ImGui::DestroyContext(ctx);
}

TEST(WindowResizeCondForScale, ScaleChangeSinceTheLastCallForcesAlwaysAndUpdatesTracked) {
  ImGuiContext* ctx = ImGui::CreateContext();
  gui::ApplyVisualLanguage(ImGui::GetIO(), 2.0f, 1.0f);

  float tracked = 1.0f;  // sized for the old scale — the window was open when it changed
  EXPECT_EQ(gui::WindowResizeCondForScale(tracked, ImGuiCond_FirstUseEver), ImGuiCond_Always);
  EXPECT_FLOAT_EQ(tracked, 2.0f) << "so the NEXT call, at this same scale, reads as unchanged";
  EXPECT_EQ(gui::WindowResizeCondForScale(tracked, ImGuiCond_FirstUseEver), ImGuiCond_FirstUseEver);

  ImGui::DestroyContext(ctx);
}

}  // namespace
