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

#include "gui/theme.hpp"
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

}  // namespace
