// SmallButton's nominal text-line height is not an ink bound: a rasterized
// descender can extend below FontSize. Keep layout, baseline, hit box and
// horizontal clipping unchanged, but include the label's actual vertical
// glyph bounds in the CPU text clip. AddText still intersects the window's
// draw-list clip, so this does not expand the renderer's scissor.
ImRect text_clip = bb;
if ((flags & ImGuiButtonFlags_AlignTextBaseLine) && style.FramePadding.y == 0.0f) {
  ImFont* font = g.Font;
  const float font_scale = g.FontSize / font->FontSize;
  float text_y = IM_TRUNC(bb.Min.y);
  const char* text_end = FindRenderedTextEnd(label);
  for (const char* p = label; p < text_end;) {
    unsigned int c = 0;
    p += ImTextCharFromUtf8(&c, p, text_end);
    if (c == '\n') {
      text_y += g.FontSize;
      continue;
    }
    const ImFontGlyph* glyph = font->FindGlyph((ImWchar)c);
    if (glyph && glyph->Visible) {
      text_clip.Min.y = ImMin(text_clip.Min.y, text_y + glyph->Y0 * font_scale);
      text_clip.Max.y = ImMax(text_clip.Max.y, text_y + glyph->Y1 * font_scale);
    }
  }
}
RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding, label, NULL, &label_size,
                  style.ButtonTextAlign, &text_clip);
