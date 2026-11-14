#pragma once

// =================================================================================================
// The Analyze workspace's shared widget primitives — the ONE implementation each of the small
// repeated UI mechanisms the analyze windows run (a56: one semantic, one authoritative copy).
//
// The family has five members, all consumed only by the analyze windows:
//   1. DisabledWrappedText — the disabled-colour wrapped text pair (ImGui has no
//      DisabledTextWrapped; the overview's and the point view's hand copies were this body
//      verbatim).
//   2. WarningTextWrapped — the warning-colour wrapped text line (the staleness banners, the
//      partial-document notes; the colour comes from semantic_colors.hpp's single source).
//   3. DrawSwatchSquare + DrawLegendIconSlot — the legend's icon slot: draw at the cursor, a
//      placeholder advances the layout, the line continues. The three legends' placeholder
//      geometries differ per site and travel as parameters.
//   4. AnalyzeSplitter — the analyze splitters' one recipe: an invisible strip, the delta-only
//      write from the DISPLAYED boundary, the two-tier floor clamp, the Resize cursor, the
//      tri-state centre line, and the drag-hint glyph. Every split's stored ratio, floors and
//      direction convention travel as parameters (see the function's comment for the dialects).
//
// Everything here is header-only and gui-side: imgui, gui constants and the theme outlet — no
// core/ or config/ include, so the gui-api-boundary rule is untouched. New consumers include
// this header directly; nothing re-declares any of the five bodies.
// =================================================================================================

#include <algorithm>
#include <cstdarg>

#include "gui/gui_constants.hpp"    // kAnalyzeSplitterSize — the strips' shared thickness
#include "gui/semantic_colors.hpp"  // WarningTextColor — the warning line's colour source
#include "gui/theme.hpp"            // UiPx — the logical-px outlet every geometry reads through
#include "imgui.h"

namespace lumice::gui {

// ---- Text lines ------------------------------------------------------------------------------------

// The disabled-colour wrapped text pair, ImGui's missing DisabledTextWrapped: one colour push,
// one TextWrappedV, one pop. The regions' text wraps, never clips — the cells' widths run from
// the side-by-side floors down to the ③ split, below several lines' full extent.
inline void DisabledWrappedText(const char* fmt, ...) {
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
  va_list args;
  va_start(args, fmt);
  ImGui::TextWrappedV(fmt, args);
  va_end(args);
  ImGui::PopStyleColor();
}

// The warning-colour wrapped text line — the analyze family's one "needs your attention" line:
// the freshness banners, the staleness hints, the partial-document notes. The colour is the
// semantic source's (WarningTextColor, semantic_colors.hpp); the icon, where a site carries one,
// rides the format string — sites without an icon pass none, so each site's icon form is
// unchanged. The staleness bars' wrap-width pins and item-rect feedback fields stay at their
// sites: this helper owns colour + line only.
inline void WarningTextWrapped(const char* fmt, ...) {
  ImGui::PushStyleColor(ImGuiCol_Text, WarningTextColor());
  va_list args;
  va_start(args, fmt);
  ImGui::TextWrappedV(fmt, args);
  va_end(args);
  ImGui::PopStyleColor();
}

// ---- Legend swatches -------------------------------------------------------------------------------

// The solid swatch square the segment/curve legends draw: a filled side×side rect at the
// current cursor. Pure ink — it advances nothing; the slot below owns the layout claim.
inline void DrawSwatchSquare(ImU32 colour, float side) {
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + side, pos.y + side), colour);
}

// The legend's icon slot: the caller's draw runs at the current cursor (it reads the cursor
// itself — the slot owns only the placement rule), a slot_w × slot_h placeholder advances the
// layout, then the line continues with `spacing` (ImGui's SameLine spacing convention: <0 = the
// style's ItemSpacing.x, 0 = none). The three legends' current placeholder pairs:
//   - the segment swatch (swatch × text line height), default spacing;
//   - the curve arm (swatch × swatch), default spacing;
//   - the map's icons (frame height × 0 — zero height, no line-height claim), zero spacing.
template <typename DrawIcon>
inline void DrawLegendIconSlot(DrawIcon&& draw_icon, float slot_w, float slot_h, float spacing = -1.0f) {
  draw_icon();
  ImGui::Dummy(ImVec2(slot_w, slot_h));
  ImGui::SameLine(0.0f, spacing);
}

// ---- Splitters -------------------------------------------------------------------------------------

// The drag axis: kLeftRight = the strip runs vertically, the hand drags along x, and the tracked
// region sits on the strip's RIGHT (dragging right shrinks it — D1/D3's convention);
// kUpDown = the strip runs horizontally, the hand drags along y, and the tracked region sits
// ABOVE (dragging down grows it — D2/D5/D6/D7's convention). The sign and the cursor follow the
// axis; the recipe is otherwise identical.
enum class SplitterDragAxis { kLeftRight, kUpDown };

// The analyze splitters' ONE recipe — the five hand-written points this consolidated (the two
// workspace splits, the pose window's column/row splits, the Raypath view's strip):
//
//   - An InvisibleButton of the shared thickness (kAnalyzeSplitterSize) × `length_px` — the
//     tests' addressing anchor and the whole hit surface.
//   - A delta-only write FROM THE DISPLAYED BOUNDARY: `shown_px` is the frame's clamped
//     derivation of the tracked region's size (the value the layout spent), never the raw
//     stored value — at a window whose layout is clamped away from the stored ratio a raw write
//     would spend the first stretch of the drag dead while the strip stands still.
//   - The two-tier clamp. Normal tier (`upper_px >= floor_a_px`, the same bound the site's
//     layout clamps the display with): new = clamp(shown ± delta, floor_a, upper), written as
//     new/denom. Overflow tier (the floors cannot both fit): the stored ratio pins to
//     `overflow_pin_ratio`, the dialect's own compressed value — the workspace splits pass
//     floor_a/(floor_a+floor_b) (their ratio divides the strip-inclusive span, the ③ split's
//     identity), the px-dialect splits pass floor_a/denom (what their clamp's lo==hi band
//     writes). The white-box equivalence: the workspace's old ratio-space arithmetic
//     (clamp(clamp(*ratio,lo,hi) ∓ d/area, lo, hi)) maps onto this form exactly — its lo/hi
//     pre-clamp IS the shown derivation (clamp(*ratio,lo,hi) = clamp(*ratio·area, floor_a,
//     upper)/area), and the tier conditions coincide (area ≥ floors+strip ⇔ upper ≥ floor_a),
//     including the boundary (both write floor_a/area when upper == floor_a); only the
//     overflow pin needs the parameter, and only there must the stored value match bit-for-bit.
//   - The ResizeEW/ResizeNS cursor on hover|active, the tri-state Separator centre line, and
//     the drag-hint glyph — a static triangle pair pointing along the drag axis, centred on the
//     strip, drawn in the SAME tri-state colour as the line (no new colour; the existing
//     hover/active brightening is the feedback).
//
// `denom_px` is what the stored ratio divides (the workspace dialect: the strip-inclusive span;
// the px dialect: the strip-exclusive one — each site passes its own). `length_px` is the
// strip's extent along its run direction. The caller owns the flow (SameLine) around the strip
// where it sits inline.
inline void AnalyzeSplitter(const char* id, SplitterDragAxis axis, float length_px, float shown_px, float denom_px,
                            float floor_a_px, float upper_px, float overflow_pin_ratio, float* stored_ratio) {
  const float thickness = UiPx(kAnalyzeSplitterSize);
  const ImVec2 strip_size =
      axis == SplitterDragAxis::kLeftRight ? ImVec2(thickness, length_px) : ImVec2(length_px, thickness);
  ImGui::InvisibleButton(id, strip_size);
  const bool hovered = ImGui::IsItemHovered();
  const bool active = ImGui::IsItemActive();
  if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    const float delta = ImGui::GetIO().MouseDelta.y * (axis == SplitterDragAxis::kUpDown ? 1.0f : 0.0f) +
                        ImGui::GetIO().MouseDelta.x * (axis == SplitterDragAxis::kLeftRight ? -1.0f : 0.0f);
    if (delta != 0.0f) {
      // The two tiers branch BEFORE any clamp: when the floors cannot both fit the bounds are
      // inverted and std::clamp's precondition would be violated (libc++ hardening aborts) —
      // the overflow tier pins the dialect's own split instead (see the comment above).
      if (upper_px >= floor_a_px) {
        const float new_shown = std::clamp(shown_px + delta, floor_a_px, upper_px);
        *stored_ratio = new_shown / std::max(1.0f, denom_px);
      } else {
        *stored_ratio = overflow_pin_ratio;
      }
    }
  }
  if (hovered || active) {
    ImGui::SetMouseCursor(axis == SplitterDragAxis::kLeftRight ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
  }
  const ImU32 col = ImGui::GetColorU32(active  ? ImGuiCol_SeparatorActive :
                                       hovered ? ImGuiCol_SeparatorHovered :
                                                 ImGuiCol_Separator);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 strip_min = ImGui::GetItemRectMin();
  const ImVec2 strip_max = ImGui::GetItemRectMax();
  const ImVec2 centre((strip_min.x + strip_max.x) * 0.5f, (strip_min.y + strip_max.y) * 0.5f);
  if (axis == SplitterDragAxis::kLeftRight) {
    dl->AddLine(ImVec2(centre.x, strip_min.y), ImVec2(centre.x, strip_max.y), col, 1.0f);
  } else {
    dl->AddLine(ImVec2(strip_min.x, centre.y), ImVec2(strip_max.x, centre.y), col, 1.0f);
  }
  // The drag-hint glyph: a filled triangle pair pointing along the drag axis, bases on the
  // centre line — the one affordance the strips carry. Geometry lives here so re-tuning it is
  // a one-site edit. Winding: AddTriangleFilled goes through AddConvexPolyFilled, whose
  // anti-aliased path presumes the SCREEN-CLOCKWISE order (the per-edge normal (dy,-dx) must
  // point outward — a counter-clockwise triangle lands its opaque inner vertices outside the
  // polygon and draws a lopsided fill), so all four triangles below are wound clockwise.
  const float tip = UiPx(3.0f);
  const float half_h = UiPx(2.4f);
  if (axis == SplitterDragAxis::kLeftRight) {
    dl->AddTriangleFilled(ImVec2(centre.x - tip, centre.y), ImVec2(centre.x, centre.y - half_h),
                          ImVec2(centre.x, centre.y + half_h), col);
    dl->AddTriangleFilled(ImVec2(centre.x + tip, centre.y), ImVec2(centre.x, centre.y + half_h),
                          ImVec2(centre.x, centre.y - half_h), col);
  } else {
    dl->AddTriangleFilled(ImVec2(centre.x, centre.y - tip), ImVec2(centre.x + half_h, centre.y),
                          ImVec2(centre.x - half_h, centre.y), col);
    dl->AddTriangleFilled(ImVec2(centre.x, centre.y + tip), ImVec2(centre.x - half_h, centre.y),
                          ImVec2(centre.x + half_h, centre.y), col);
  }
}

}  // namespace lumice::gui
