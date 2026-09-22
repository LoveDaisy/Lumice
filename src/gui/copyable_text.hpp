#ifndef LUMICE_GUI_COPYABLE_TEXT_HPP
#define LUMICE_GUI_COPYABLE_TEXT_HPP

// The one way read-only text leaves the GUI: a right-click "Copy" on an item, or a "Copy all"
// button over a block of it. Every consumer (the analysis list's rows, the log panel's lines, the
// filter editor's preview, the Summary page) goes through these two entry points, so the
// clipboard is written from exactly one place — ImGui::SetClipboardText, here — and a test that
// installs a clipboard stand-in (test/gui/test_gui_main.cpp) sees every copy the GUI can make.
//
// Two things a consumer has to know, stated here so nobody re-derives them at a call site:
//
//   * CopyMenuForLastItem hangs its popup on the LAST SUBMITTED ITEM's id
//     (ImGui::BeginPopupContextItem with no str_id), so that item must have a NON-ZERO id.
//     Selectable, Button and InvisibleButton do; Text / TextUnformatted / TextDisabled do not —
//     they reach ItemAdd() with id == 0, which is also why the test engine cannot address them.
//     Text that needs a menu gets an InvisibleButton laid over its rect (edit_modals.cpp's filter
//     preview shows the shape: overlay, then put the cursor back where the text left it).
//
//   * Both entry points are LAZY: the builder callback runs only on the frame the popup is
//     actually open (or the button actually pressed), never on the frame that merely submits the
//     item. That is what makes "copy all 4096 log lines" free on every other frame. The other
//     side of the same fact: the callback is invoked within the call that received it, in the
//     same frame, and never stored — a lambda may capture the caller's locals by reference, and
//     a `const char*` passed to the single-text overload only has to outlive this frame.

#include <functional>
#include <string>
#include <vector>

#include "imgui.h"

namespace lumice::gui {

// One line of the right-click menu: the item's label, and the text it puts on the clipboard.
struct CopyMenuEntry {
  std::string label;
  std::string text;
};

// A right-click menu on the last submitted item, one MenuItem per entry the builder returns.
// Returns true on the frame an entry was chosen (and its text written to the clipboard).
inline bool CopyMenuForLastItem(const std::function<std::vector<CopyMenuEntry>()>& build_entries) {
  if (!ImGui::BeginPopupContextItem()) {
    return false;
  }
  bool copied = false;
  for (const CopyMenuEntry& entry : build_entries()) {
    if (ImGui::MenuItem(entry.label.c_str())) {
      ImGui::SetClipboardText(entry.text.c_str());
      copied = true;
    }
  }
  ImGui::EndPopup();
  return copied;
}

// The single-entry form: a right-click menu holding one "Copy", which copies `text`. The pointer
// is read only while the popup is open, on the frame the item is chosen; see the header comment.
inline bool CopyMenuForLastItem(const char* text) {
  return CopyMenuForLastItem(
      [text] { return std::vector<CopyMenuEntry>{ { "Copy", std::string(text != nullptr ? text : "") } }; });
}

// A button that, when pressed, builds a block of text and writes it to the clipboard. Returns true
// on the frame it was pressed. The small form (ImGui::SmallButton: no vertical frame padding) is
// for a line of text the button shares — it leaves that line's height alone.
inline bool CopyAllButton(const char* label, const std::function<std::string()>& build_text) {
  if (!ImGui::Button(label)) {
    return false;
  }
  ImGui::SetClipboardText(build_text().c_str());
  return true;
}

inline bool CopyAllSmallButton(const char* label, const std::function<std::string()>& build_text) {
  if (!ImGui::SmallButton(label)) {
    return false;
  }
  ImGui::SetClipboardText(build_text().c_str());
  return true;
}

}  // namespace lumice::gui

#endif  // LUMICE_GUI_COPYABLE_TEXT_HPP
