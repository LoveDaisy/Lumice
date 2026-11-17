#pragma once

#include "IconsFontAwesome6.h"
#include "imgui.h"

namespace lumice::gui {

// SetTooltip's formatting, positioning and replacement semantics, with a UI-scaled maximum
// text width. Call inside the existing hover predicate; short text still auto-sizes.
void SetWrappedTooltip(const char* fmt, ...) IM_FMTARGS(1);

// The GUI's one explicit help carrier for stable functional/conceptual explanations (the
// two-tier tooltip rule in doc/gui/gui-visual-language.md): a transparent, borderless FontAwesome
// circle-info SmallButton in TextDisabled that joins the previous item (SameLine) and shows its
// explanation through SetWrappedTooltip on hover. `id_suffix` is the stable item id after "##";
// callers own its uniqueness across the window. No click side effect, no value change, and the
// text is only formatted on hover.
void InfoIcon(const char* id_suffix, const char* fmt, ...) IM_FMTARGS(2);

// InfoIcon drawn at the current fresh line instead of joining the previous item — the standalone
// section-marker shape (e.g. a help marker directly under a SeparatorText).
void InfoIconLine(const char* id_suffix, const char* fmt, ...) IM_FMTARGS(2);

// The layout advance an InfoIcon occupies joined after a control: the icon frame plus one
// ItemSpacing. Measurement-side counterpart of the SameLine join, for width-budgeted rows.
float InfoIconAdvance();

}  // namespace lumice::gui
