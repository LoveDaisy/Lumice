#pragma once

#include "imgui.h"

namespace lumice::gui {

// SetTooltip's formatting, positioning and replacement semantics, with a UI-scaled maximum
// text width. Call inside the existing hover predicate; short text still auto-sizes.
void SetWrappedTooltip(const char* fmt, ...) IM_FMTARGS(1);

}  // namespace lumice::gui
