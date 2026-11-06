// The body font's glyph coverage (src/gui/theme.cpp, AddBodyFont), read off the live atlas.
//
// The proposition is about what the atlas HOLDS, not about what a panel draws: a codepoint left
// out of the font's glyph range rasterises as ImGui's fallback "?" — which looks like a typo, not
// like a missing glyph, and which no screenshot reference can distinguish from a typo either (the
// modal_layout references were shot with that "?" in the crystal name hint for months). Asking
// the atlas directly, with FindGlyphNoFallback, is the one read that cannot be fooled by the
// fallback. The negative row pins the reason the analysis list draws its chain joiner as an icon
// rather than as U+2192: Roboto Medium has no glyph for it, and if a future font swap gives it
// one, this row is what says JoinerForDisplay could go back to a plain character.

#include "test_gui_shared.hpp"

void RegisterBodyFontGlyphCoverageTests(ImGuiTestEngine* engine) {
  ImGuiTest* t = IM_REGISTER_TEST(engine, "font_glyph_coverage", "general_punctuation_present_arrows_absent");
  t->TestFunc = [](ImGuiTestContext* ctx) {
    // One frame first, so the atlas read below is the one the renderer has built and drawn with.
    ctx->Yield(1);
    ImFontAtlas* atlas = ImGui::GetIO().Fonts;
    IM_CHECK(atlas != nullptr);
    IM_CHECK(atlas->Fonts.Size >= 1);
    // The body font is the first one added (AddBodyFont runs before MergeIconGlyphs, and the
    // AddFontDefault fallback replaces it rather than preceding it).
    ImFont* body = atlas->Fonts[0];
    IM_CHECK(body != nullptr);
    ctx->LogInfo("[font_glyph_coverage] atlas %dx%d, %d fonts, %d glyphs, size %.0f, texture %llu", atlas->TexWidth,
                 atlas->TexHeight, atlas->Fonts.Size, body->Glyphs.Size, body->FontSize,
                 static_cast<unsigned long long>(atlas->TexID));
    // Independent display-text anchors: the wavelength label and map deviation readout.
    // Log both before asserting, so a red run distinguishes a range miss from a fallback.
    for (ImWchar cp : { 0x03B4, 0x03BB }) {
      const ImFontGlyph* glyph = body->FindGlyphNoFallback(cp);
      ctx->LogInfo("[font_glyph_coverage] U+%04X present=%d advance=%.3f fallback=%.3f", cp, glyph != nullptr,
                   glyph != nullptr ? glyph->AdvanceX : 0.0f, body->FallbackAdvanceX);
    }
    IM_CHECK(body->FindGlyphNoFallback(0x03BB) != nullptr);  // lambda: wavelength
    IM_CHECK(body->FindGlyphNoFallback(0x03B4) != nullptr);  // delta: deviation from sun
    const auto check_present = [ctx, body](ImWchar cp) {
      if (body->FindGlyphNoFallback(cp) == nullptr) {
        IM_ERRORF("Missing display glyph U+%04X", cp);
      }
    };
    // The other built-in non-ASCII body characters, including UTF-8 byte-escaped literals.
    for (ImWchar cp : { 0x00B0, 0x00B1, 0x00B7, 0x00D7, 0x2013, 0x2014, 0x2026 }) {
      check_present(cp);
    }
    // Actual GUI icon consumers, not the production font-loading range. PLUS is ASCII and
    // intentionally comes from the body font; the remaining glyphs are the merged FA face.
    for (ImWchar cp :
         { 0x002B, 0xF002, 0xF00C, 0xF00D, 0xF013, 0xF04B, 0xF04D, 0xF04E, 0xF053, 0xF054, 0xF05A, 0xF05B,
           0xF05E, 0xF061, 0xF062, 0xF063, 0xF06A, 0xF06E, 0xF070, 0xF071, 0xF07C, 0xF0C1, 0xF0C5, 0xF0C7,
           0xF0E2, 0xF141, 0xF15B, 0xF15C, 0xF1F8, 0xF1FB, 0xF204, 0xF205, 0xF304, 0xF53F, 0xF56F, 0xF6DD }) {
      check_present(cp);
    }
    // General Punctuation landed: the dashes and the single angle quote, plus the guillemet from
    // Latin-1 Supplement that ImGui's default range already covered.
    IM_CHECK(body->FindGlyphNoFallback(0x2013) != nullptr);  // en dash
    IM_CHECK(body->FindGlyphNoFallback(0x2014) != nullptr);  // em dash
    IM_CHECK(body->FindGlyphNoFallback(0x203A) != nullptr);  // single right-pointing angle quote
    IM_CHECK(body->FindGlyphNoFallback(0x00BB) != nullptr);  // right-pointing double angle quote
    // The icon the chain joiner is drawn with is merged into the same font.
    IM_CHECK(body->FindGlyphNoFallback(0xF061) != nullptr);  // ICON_FA_ARROW_RIGHT
    // Still absent, by the font's cmap and not by the range: U+2192 is why the joiner is an icon.
    IM_CHECK(body->FindGlyphNoFallback(0x2192) == nullptr);
  };
}
