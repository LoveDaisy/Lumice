// The Summary page's labels are the panel's words, and the word has ONE home.
//
// FieldEditorEntry::label (src/gui/field_editor_registry.hpp) is that home: the main panel's widget
// call sites build their id strings from it through PanelLabel(), and the Summary page prints it
// through LabelFor(). The claim this file holds is that the call sites really do read it — that no
// panel source still carries the word as a bare literal beside a copy in the registry. It cannot be
// asserted by calling anything: a widget call site lives inside an ImGui frame. So the panel
// sources are read off disk and scanned, the same shape test_bg_color_picker.cpp and
// test_user_defaults.cpp use for their own "the branch's text says so" claims.
//
// The list of words is NOT restated here. It is derived from the registry — every key that declares
// a label — so a label added there is scanned for with no edit to this file, and the keys the scan
// skips are named once, below.
//
// The scan strips comments first, and does so with a scanner that knows where string literals are:
// a line-wise "cut at //" would take a `//` INSIDE a string ("http://...") as a comment start and
// swallow the real code after it on that line — including a literal the scan is looking for — and
// report a clean file. The probe case at the bottom is that failure, written down: the same line
// through the naive cut and through this scanner, one losing the literal and one keeping it, so
// the reason the scanner is a state machine is a red/green fact and not a comment.

#include <gtest/gtest.h>

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "gui/field_editor_registry.hpp"

namespace gui = lumice::gui;

namespace {

// The registered keys whose declared label is NOT a panel literal, because the main panel edits
// them with a group of per-value buttons that has no single widget label to read back:
//   - renderer.visible: three RadioButtons (Upper / Full / Lower) under a SeparatorText
//     ("Visibility") that names the group; the registry's "Visible" is the page's word alone.
// Excluded from the scans below by name — each exception is stated here, once, rather than by a
// pattern that would silently widen. (renderer.display_mode was the second one until the
// Screenshot export options gave its "Show As" a heading of its own; it is now read there through
// PanelLabel and covered by the rule like any other key.)
constexpr const char* kExcludedFromLabelParityCheck[] = { "renderer.visible" };

bool IsExcludedFromLabelParityCheck(const std::string& key) {
  for (const char* excluded : kExcludedFromLabelParityCheck) {
    if (key == excluded) {
      return true;
    }
  }
  return false;
}

std::string ReadFile(const char* path) {
  std::ifstream in(path);
  EXPECT_TRUE(in.is_open()) << path;
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

// Comments removed, string literals kept whole. The scanner tracks whether it is inside a
// double-quoted literal (an unescaped `"` toggles it; `\"` inside does not), and `//` or `/*` only
// open a comment while it is not. A `//` comment runs to the end of its line, a `/* */` comment
// to its close; both are replaced by a single space so tokens on either side stay separate.
std::string StripComments(const std::string& src) {
  std::string out;
  out.reserve(src.size());
  bool in_string = false;
  size_t i = 0;
  while (i < src.size()) {
    const char c = src[i];
    if (in_string) {
      out.push_back(c);
      if (c == '\\' && i + 1 < src.size()) {
        out.push_back(src[i + 1]);
        i += 2;
        continue;
      }
      if (c == '"') {
        in_string = false;
      }
      ++i;
      continue;
    }
    if (c == '"') {
      in_string = true;
      out.push_back(c);
      ++i;
      continue;
    }
    if (c == '/' && i + 1 < src.size() && src[i + 1] == '/') {
      while (i < src.size() && src[i] != '\n') {
        ++i;
      }
      out.push_back(' ');
      continue;
    }
    if (c == '/' && i + 1 < src.size() && src[i + 1] == '*') {
      const size_t close = src.find("*/", i + 2);
      i = close == std::string::npos ? src.size() : close + 2;
      out.push_back(' ');
      continue;
    }
    out.push_back(c);
    ++i;
  }
  return out;
}

// The scanner this file must NOT use, kept as the probe's red arm: cut every line at its first
// `//`, whatever is around it.
std::string StripCommentsNaive(const std::string& src) {
  std::string out;
  size_t start = 0;
  while (start <= src.size()) {
    const size_t nl = src.find('\n', start);
    const std::string line = src.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
    const size_t slash = line.find("//");
    out += slash == std::string::npos ? line : line.substr(0, slash);
    out.push_back('\n');
    if (nl == std::string::npos) {
      break;
    }
    start = nl + 1;
  }
  return out;
}

// Whether `code` contains `"<label>"` or `"<label>##` — the two shapes a widget label literal takes.
// A prefix match ("Roll" against "Roll is locked to 0…", a tooltip) does not count, which is why
// the character after the label is part of the pattern.
bool HasLabelLiteral(const std::string& code, const std::string& label) {
  const std::string quoted = "\"" + label;
  for (size_t at = code.find(quoted); at != std::string::npos; at = code.find(quoted, at + 1)) {
    const size_t after = at + quoted.size();
    if (after < code.size() && (code[after] == '"' || code.compare(after, 2, "##") == 0)) {
      return true;
    }
  }
  return false;
}

struct DeclaredLabel {
  std::string key_path;
  std::string label;
};

std::vector<DeclaredLabel> DeclaredLabels() {
  std::vector<DeclaredLabel> out;
  for (const std::string& key : gui::RegisteredFieldEditorKeyPaths()) {
    const char* label = gui::LabelFor(key);
    if (label != nullptr && !IsExcludedFromLabelParityCheck(key)) {
      out.push_back({ key, label });
    }
  }
  return out;
}

}  // namespace

// Every declared label is read at a call site: the panel sources contain PanelLabel("<key>") for the
// key, and no bare literal of the word in either widget-label shape. Both halves are needed — a
// call site could read the registry AND keep the literal somewhere else, which is the two-copies
// state the single home exists to end.
TEST(ConfigSummaryLabels, EveryDeclaredLabelIsReadFromTheRegistryAndNotRestated) {
  const std::string panels = StripComments(ReadFile(LUMICE_GUI_PANELS_CPP_PATH));
  const std::string app_panels = StripComments(ReadFile(LUMICE_GUI_APP_PANELS_CPP_PATH));
  ASSERT_FALSE(panels.empty());
  ASSERT_FALSE(app_panels.empty());
  const std::string code = panels + "\n" + app_panels;

  const auto declared = DeclaredLabels();
  ASSERT_GE(declared.size(), 15u) << "the registry declares far fewer labels than the Summary prints";
  for (const auto& [key, label] : declared) {
    const std::string read_site = "PanelLabel(\"" + key + "\"";
    EXPECT_NE(code.find(read_site), std::string::npos) << key << ": no call site reads its label from the registry";
    EXPECT_FALSE(HasLabelLiteral(code, label)) << key << ": \"" << label << "\" is still a bare literal in a panel";
  }
}

// The exceptions are real and stay exceptions: each excluded key declares a label the page prints,
// and no call site reads it (its control is a group of per-value buttons). If a widget carrying
// that label ever appears, this goes red and the key comes off the exclusion — the rule then
// covers it.
TEST(ConfigSummaryLabels, TheExcludedFieldsAreTheDeclaredLabelsNoCallSiteReads) {
  const std::string code = StripComments(ReadFile(LUMICE_GUI_APP_PANELS_CPP_PATH));
  for (const char* key : kExcludedFromLabelParityCheck) {
    if (gui::LabelFor(key) == nullptr) {
      ADD_FAILURE() << key << ": declares no label";
      continue;
    }
    EXPECT_EQ(code.find(std::string("PanelLabel(\"") + key + "\""), std::string::npos) << key;
    EXPECT_FALSE(HasLabelLiteral(code, gui::LabelFor(key))) << key;
  }
}

// The probe: one line with no comment on it at all, a `//` inside a string literal, and a widget
// literal in the real code after it. The naive cut loses the widget call and with it the literal
// the scan exists to find (a false clean); the string-aware scanner keeps both. Assertions on both
// arms, so the case fails if the scanner regresses to the naive shape AND if the probe stops
// discriminating.
TEST(ConfigSummaryLabels, TheCommentStripperKeepsCodeAfterASlashSlashInsideAString) {
  const std::string line = "auto* w = LogPath(\"http://example.com\"); SliderWithInput(\"Rays(M)\", &v);\n";
  const std::string naive = StripCommentsNaive(line);
  const std::string aware = StripComments(line);

  EXPECT_EQ(naive.find("SliderWithInput"), std::string::npos)
      << "the naive cut kept the code: the probe has no red arm";
  EXPECT_FALSE(HasLabelLiteral(naive, "Rays(M)"));

  EXPECT_NE(aware.find("SliderWithInput(\"Rays(M)\", &v);"), std::string::npos);
  EXPECT_TRUE(HasLabelLiteral(aware, "Rays(M)"));

  // And a real comment is still removed, on a line with a string before it, so the scanner is not
  // merely "never strip".
  const std::string commented = "Foo(\"a\"); // Bar(\"Rays(M)\");\nBaz(\"Max hits\"); /* \"EV\" */ Qux();\n";
  const std::string stripped = StripComments(commented);
  EXPECT_FALSE(HasLabelLiteral(stripped, "Rays(M)"));
  EXPECT_FALSE(HasLabelLiteral(stripped, "EV"));
  EXPECT_TRUE(HasLabelLiteral(stripped, "Max hits"));
  EXPECT_NE(stripped.find("Qux();"), std::string::npos);
}
