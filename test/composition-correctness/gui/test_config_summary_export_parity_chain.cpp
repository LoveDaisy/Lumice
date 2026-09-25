// Summary page -> BuildExportJsonOrWarn: the two descriptions of one document agree.
//
// The page (src/gui/config_summary.*) prints what the GUI SHOWS; the export arm
// (file_io.hpp SceneIntent::kJsonExport) writes what the CLI needs to draw the picture on screen.
// Both are "the document as the user sees it", from two encoders, so this chain walks EVERY leaf
// of the exported core JSON and demands one of two things of it: a rule that finds the same value
// on the page, or an entry in the explicit exclusion table below with a reason. A leaf that
// matches neither fails the case — that meta-assertion is what keeps a new exported field from
// slipping past unexamined, and it is the whole difference between "parity" and "the fields I
// thought of".
//
// Two things this chain is the red-state for, both from the design constraint the page was built
// under: anchoring the page to the COMMIT arm (the lens would read dual_fisheye_equal_area on a
// linear document — the lens rule below goes red), and printing a field the GUI never shows (the
// sun's azimuth: exported, fixed at 0, no control — asserted absent from the Sun group). Verified
// red once by hand on each, the way the AC asks; the assertions are what remain.
//
// Number spelling: the settings section formats through FormatDiffValue (the diff engine's
// display form), the document section through the edit modal's own slot formats, so a rule
// compares NUMBERS re-spelled the page's way rather than raw strings — "1.0" and "1" are the same
// value read off two encoders, not a divergence.
//
// Labels are the PANEL's words, spelled here as literals on purpose ("Rays(M)", "EV Anchor",
// "Sky Color"): this file pins what a reader of the page sees, and reading the registry's
// LabelFor back would make the assertion true of any spelling at all. That the registry's word is
// the panel's word is test_config_summary_labels.cpp's job.
//
// A third thing the page does that the export does not: it prints only the fields the panel
// currently shows or enables. So a settings rule is conditional on the registry's own gate — a
// field whose ConstraintFor(...).enabled is false (Sky Color under Print, Roll under a full-sky
// lens, Rays(M) with Infinite rays on) is asserted ABSENT from the page while the export still
// writes it, and ExpectPageMatchesExport closes the loop in both directions over every registered
// leaf: enabled <=> on the page. The three documents at the bottom each take one of those gates.

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include "gui/app.hpp"
#include "gui/axis_presets.hpp"
#include "gui/config_summary.hpp"
#include "gui/defaults_diff.hpp"
#include "gui/field_editor_registry.hpp"
#include "gui/file_io.hpp"
#include "gui/gui_state.hpp"
#include "gui/raypath_segments.hpp"
#include "gui/shape_scalar_domain.hpp"
#include "support/scene_json_helpers.hpp"

namespace lumice::gui {
namespace {

using nlohmann::json;

// ---- The page, indexed for lookups --------------------------------------------------------

const ConfigSummaryGroup* FindGroup(const ConfigSummary& page, const std::string& title) {
  for (const auto& g : page.settings) {
    if (g.title == title) {
      return &g;
    }
  }
  return nullptr;
}

// ---- The document tables, indexed for lookups --------------------------------------------
//
// The document section is two tables per layer, row i of both being the layer's i-th entry
// (config_summary.hpp ConfigSummaryLayer), so a document value is located by (layer, row, column)
// rather than by a group title and a label.

struct EntryLocator {
  int layer_idx;
  int row_idx;
};

std::string LocatorText(const EntryLocator& at) {
  return "layer " + std::to_string(at.layer_idx) + " row " + std::to_string(at.row_idx);
}

// The cell under column `column` of the entry's row in `table`, or nullopt with a failure
// recorded. "No such column" is a failure of its own, distinct from a column that exists and
// holds "" (a legitimately empty, not-applicable cell): a misspelled column name in a rule below
// must not read as "the page omits it" and pass.
std::optional<std::string> TableCell(const ConfigSummaryTable& table, const EntryLocator& at, const std::string& column,
                                     const char* table_name) {
  const auto col = std::find(table.columns.begin(), table.columns.end(), column);
  if (col == table.columns.end()) {
    ADD_FAILURE() << "the " << table_name << " table has no column \"" << column << "\"";
    return std::nullopt;
  }
  if (at.row_idx < 0 || static_cast<size_t>(at.row_idx) >= table.rows.size()) {
    ADD_FAILURE() << "the " << table_name << " table of " << LocatorText(at) << " has no such row";
    return std::nullopt;
  }
  return table.rows[static_cast<size_t>(at.row_idx)].cells[static_cast<size_t>(col - table.columns.begin())];
}

const ConfigSummaryLayer* PageLayer(const ConfigSummary& page, int layer_idx) {
  if (layer_idx < 0 || static_cast<size_t>(layer_idx) >= page.document.size()) {
    ADD_FAILURE() << "the page has no layer " << layer_idx;
    return nullptr;
  }
  return &page.document[static_cast<size_t>(layer_idx)];
}

// A Crystals-table cell of an entry, or nullopt with a failure recorded.
std::optional<std::string> CrystalsCell(const ConfigSummary& page, const EntryLocator& at, const std::string& column) {
  const ConfigSummaryLayer* layer = PageLayer(page, at.layer_idx);
  if (layer == nullptr) {
    return std::nullopt;
  }
  return TableCell(layer->crystals, at, column, "Crystals");
}

// A Shape-table cell of an entry, by shape slot (LUMICE_SHAPE_SCALAR_*), or nullopt.
std::optional<std::string> ShapeCell(const ConfigSummary& page, const EntryLocator& at, int slot) {
  const ConfigSummaryLayer* layer = PageLayer(page, at.layer_idx);
  if (layer == nullptr) {
    return std::nullopt;
  }
  return TableCell(layer->shape, at, kShapeScalarLabels[slot], "Shape");
}

// A Shape-table cell by column word, for the two wedge angles that are not shape slots.
std::optional<std::string> ShapeCellNamed(const ConfigSummary& page, const EntryLocator& at,
                                          const std::string& column) {
  const ConfigSummaryLayer* layer = PageLayer(page, at.layer_idx);
  if (layer == nullptr) {
    return std::nullopt;
  }
  return TableCell(layer->shape, at, column, "Shape");
}

// The value printed under `label` in group `title`, or nullopt with a failure recorded.
std::optional<std::string> PageValue(const ConfigSummary& page, const std::string& title, const std::string& label) {
  const ConfigSummaryGroup* g = FindGroup(page, title);
  if (g == nullptr) {
    ADD_FAILURE() << "the page has no group titled \"" << title << "\"";
    return std::nullopt;
  }
  for (const auto& f : g->fields) {
    if (f.label == label) {
      return f.value;
    }
  }
  ADD_FAILURE() << "group \"" << title << "\" has no row labelled \"" << label << "\"";
  return std::nullopt;
}

// Whether group `title` carries a row labelled `label`, with no failure recorded either way.
bool HasRow(const ConfigSummary& page, const std::string& title, const std::string& label) {
  const ConfigSummaryGroup* g = FindGroup(page, title);
  if (g == nullptr) {
    return false;
  }
  for (const auto& f : g->fields) {
    if (f.label == label) {
      return true;
    }
  }
  return false;
}

std::string Fmt(const char* fmt, double v) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), fmt, v);
  return buf;
}

// The export's number as the page would spell it, compared against the page's text. The settings
// rows go through FormatDiffValue, and a value is allowed to carry more around the number — so the
// check is "the page's value contains the number spelled this way", and the caller says which
// format. (Document cells have their own helpers below, ExpectCellNumber and kin.)
void ExpectPageNumber(const ConfigSummary& page, const std::string& title, const std::string& label, const json& v,
                      const char* fmt = nullptr) {
  const auto got = PageValue(page, title, label);
  if (!got.has_value()) {
    return;
  }
  const std::string want = fmt == nullptr ? FormatDiffValue(v) : Fmt(fmt, v.get<double>());
  EXPECT_NE(got->find(want), std::string::npos) << title << " / " << label << ": page says \"" << *got
                                                << "\", export has " << v.dump() << " (\"" << want << "\")";
}

void ExpectPageText(const ConfigSummary& page, const std::string& title, const std::string& label,
                    const std::string& want) {
  const auto got = PageValue(page, title, label);
  if (!got.has_value()) {
    return;
  }
  EXPECT_NE(got->find(want), std::string::npos)
      << title << " / " << label << ": page says \"" << *got << "\", export has \"" << want << "\"";
}

// A settings row behind the registry's gate: when the panel enables `key_path` right now the page
// carries the row and its value is checked; when it does not, the row must be absent — the export
// still writes the field, and this is where the two are allowed to differ.
void ExpectGatedNumber(const GuiState& state, const ConfigSummary& page, const char* key_path, const std::string& title,
                       const std::string& label, const json& v) {
  if (ConstraintFor(key_path, state).enabled) {
    ExpectPageNumber(page, title, label, v);
  } else {
    EXPECT_FALSE(HasRow(page, title, label))
        << key_path << " is disabled on the panel but " << title << " / " << label << " is on the page";
  }
}

void ExpectGatedText(const GuiState& state, const ConfigSummary& page, const char* key_path, const std::string& title,
                     const std::string& label, const std::string& want) {
  if (ConstraintFor(key_path, state).enabled) {
    ExpectPageText(page, title, label, want);
  } else {
    EXPECT_FALSE(HasRow(page, title, label))
        << key_path << " is disabled on the panel but " << title << " / " << label << " is on the page";
  }
}

// A document cell against the export's number, spelled the cell's way (the caller's `fmt` is the
// slot's own): the cell may carry more around the number — the distribution letter and the spread
// of a randomized "U 0.900(0.100)", the preset name leading a zenith "Plate · G 0(1)" — so the
// check is "the cell contains the number spelled this way".
void ExpectCellNumber(const std::optional<std::string>& cell, const EntryLocator& at, const std::string& column,
                      const json& v, const char* fmt) {
  if (!cell.has_value()) {
    return;
  }
  const std::string want = Fmt(fmt, v.get<double>());
  EXPECT_NE(cell->find(want), std::string::npos) << LocatorText(at) << " / " << column << ": the cell says \"" << *cell
                                                 << "\", export has " << v.dump() << " (\"" << want << "\")";
}

void ExpectCellText(const std::optional<std::string>& cell, const EntryLocator& at, const std::string& column,
                    const std::string& want) {
  if (!cell.has_value()) {
    return;
  }
  EXPECT_NE(cell->find(want), std::string::npos)
      << LocatorText(at) << " / " << column << ": the cell says \"" << *cell << "\", export has \"" << want << "\"";
}

// The export's distribution type ("gauss") against a cell in the page's notation: the cell
// carries the type's LETTER (config_summary.hpp DistributionLetter), so the assertion is that the
// letter the page assigns to this wire word is in the cell — compared as a whole token, so a "G"
// is not found inside a "G*" and a preset name ("Gauss" is not one, but "Random" holds no letter
// either) cannot stand in for the letter.
void ExpectCellDistributionType(const std::optional<std::string>& cell, const EntryLocator& at,
                                const std::string& column, const std::string& wire) {
  if (!cell.has_value()) {
    return;
  }
  const std::string letter = DistributionLetterForWireName(wire);
  ASSERT_NE(letter, "?") << "the page has no letter for the wire word \"" << wire << "\"";
  bool found = false;
  size_t pos = 0;
  while ((pos = cell->find(letter, pos)) != std::string::npos) {
    const size_t end = pos + letter.size();
    const bool starts = pos == 0 || (*cell)[pos - 1] == ' ';
    const bool ends = end == cell->size() || (*cell)[end] == ' ';
    if (starts && ends) {
      found = true;
      break;
    }
    pos = end;
  }
  EXPECT_TRUE(found) << LocatorText(at) << " / " << column << ": the cell says \"" << *cell << "\", export has type \""
                     << wire << "\" (letter \"" << letter << "\")";
}

// ---- The export, flattened ---------------------------------------------------------------

struct Leaf {
  std::string path;  // "render[0].lens.type", "crystal[1].shape.face_distance[3]"
  json value;
};

void Flatten(const std::string& prefix, const json& node, std::vector<Leaf>& out) {
  if (node.is_object()) {
    for (auto it = node.begin(); it != node.end(); ++it) {
      Flatten(prefix.empty() ? it.key() : prefix + "." + it.key(), *it, out);
    }
    return;
  }
  if (node.is_array()) {
    // A numeric array is one leaf (a colour, a canvas, a face-distance sextet) — the same rule the
    // page's settings rows use; an array of objects is walked.
    bool all_scalar = true;
    for (const auto& e : node) {
      all_scalar = all_scalar && !e.is_structured();
    }
    if (all_scalar && !node.empty()) {
      out.push_back({ prefix, node });
      return;
    }
    for (size_t i = 0; i < node.size(); ++i) {
      Flatten(prefix + "[" + std::to_string(i) + "]", node[i], out);
    }
    if (node.empty()) {
      out.push_back({ prefix, node });
    }
    return;
  }
  out.push_back({ prefix, node });
}

// ---- The rules --------------------------------------------------------------------------

// Everything the export writes that the page does not carry, each with the reason. A leaf
// matching one of these is accounted for; nothing else is silently skipped.
struct Exclusion {
  const char* pattern;
  const char* reason;
};
const Exclusion kExcluded[] = {
  { R"(^scene\.light_source\.type$)", "constant \"sun\"; not a setting" },
  { R"(^scene\.light_source\.azimuth$)",
    "fixed at 0 with no control (doc/gui-state-governance.md §9) — asserted ABSENT from the page below" },
  { R"(^render\[0\]\.id$)", "bookkeeping" },
  { R"(^crystal\[\d+\]\.id$)", "bookkeeping: the core id, which the page maps back to the pool id" },
  { R"(^filter$)", "an empty filter list: nothing to show" },
  { R"(^filter\[\d+\]\.id$)", "bookkeeping" },
  { R"(^filter\[\d+\]\.type$)", "the filter's wire encoding (raypath / entry_exit / complex), not a setting" },
  { R"(^filter\[\d+\]\.composition$)",
    "the OR node a multi-row sum of products expands to; its members are asserted one by one — the "
    "first by its row text, the rest by the \"(+N more)\" count the Filter cell carries for them" },
  { R"(^scene\.scattering\[\d+\]\.entries\[\d+\]\.filter$)",
    "the core filter id; the page shows the filter by its editor summary instead (checked per filter)" },
  { R"(^render\[0\]\.ray_color$)",
    "the export writes core's \"use the material colour\" sentinel [-1,-1,-1] and never reads the "
    "GuiState field (gui_state.hpp RenderConfigResimFields); the page shows the stored field the "
    "Settings panel's read-only row shows" },
  { R"(^render\[0\]\.lens_shift$)", "no GUI control exists (file_io.hpp, kJsonExport's first documented exception)" },
  { R"(^render\[0\]\.overlap$)", "the commit texture's seam band; 0 on the export arm, never shown" },
  { R"(^render\[0\]\.grid(\..*)?$)",
    "overlay annotations are kView-tier view preferences, which the page hides by tier "
    "(test_config_summary_rows.cpp); the export writes them because the CLI has no overlay stage" },
};

using Rule = std::function<void(const std::smatch&, const json&)>;

struct RuleEntry {
  std::regex pattern;
  Rule rule;
};

// The pool id the page prints for a scene's crystal id (the inverse of the export's numbering).
int PoolIdForCoreId(const GuiState& state, int core_id) {
  for (const auto& [pool, core] : ComputeCrystalPoolToCoreIdMap(state)) {
    if (core == core_id) {
      return pool;
    }
  }
  return -1;
}

// Which entries on the page show scene crystal `core_id`: every entry whose crystal maps to it,
// as (layer, row) into the page's tables. A crystal's cells are asserted on each of them.
std::vector<EntryLocator> EntriesForCoreCrystal(const GuiState& state, int core_id) {
  const int pool = PoolIdForCoreId(state, core_id);
  std::vector<EntryLocator> at;
  for (size_t l = 0; l < state.layers.size(); ++l) {
    for (size_t e = 0; e < state.layers[l].entries.size(); ++e) {
      if (state.layers[l].entries[e].crystal_id == pool) {
        at.push_back({ static_cast<int>(l), static_cast<int>(e) });
      }
    }
  }
  EXPECT_FALSE(at.empty()) << "scene crystal " << core_id << " (pool " << pool << ") is on no entry";
  return at;
}

// Shape-slot lookups by the export's key names (LUMICE_ShapeScalarSyncKeyName's spellings).
int ShapeSlotForExportKey(const std::string& key) {
  static const std::map<std::string, int> kSlots = {
    { "height", LUMICE_SHAPE_SCALAR_HEIGHT },
    { "prism_h", LUMICE_SHAPE_SCALAR_PRISM_H },
    { "upper_h", LUMICE_SHAPE_SCALAR_UPPER_H },
    { "lower_h", LUMICE_SHAPE_SCALAR_LOWER_H },
  };
  const auto it = kSlots.find(key);
  return it == kSlots.end() ? -1 : it->second;
}

// ---- filter ----
// Free functions rather than BuildRules-local lambdas: the rules are std::functions that outlive
// BuildRules, so a rule that captured a local lambda by reference would call through a dead stack
// frame — which is what once read `doc` as a number on Linux and MSVC while macOS happened to
// leave the frame intact.
//
// The export's filters are CORE filters: BuildScene expands each GUI filter's sum of products
// into one raypath / entry-exit filter per row plus, for a multi-row SoP, a "complex" node
// composing them (file_io.cpp ExpandSopToClauses). Entries reference the top node. So the
// Filter cells a core filter must be found in are the entries whose top node is it, or composes
// it.
std::vector<EntryLocator> EntriesReachingFilter(const json& doc, int export_idx) {
  std::vector<EntryLocator> at;
  const json& filters = doc["filter"];
  const json& scattering = doc["scene"]["scattering"];
  for (size_t l = 0; l < scattering.size(); ++l) {
    const json& entries = scattering[l]["entries"];
    for (size_t e = 0; e < entries.size(); ++e) {
      if (!entries[e].contains("filter")) {
        continue;
      }
      const int top = entries[e]["filter"].get<int>();
      bool reaches = top == export_idx;
      if (!reaches && filters[top].contains("composition")) {
        for (const auto& member : filters[top]["composition"]) {
          reaches = reaches || member.get<int>() == export_idx;
        }
      }
      if (reaches) {
        at.push_back({ static_cast<int>(l), static_cast<int>(e) });
      }
    }
  }
  EXPECT_FALSE(at.empty()) << "exported filter " << export_idx << " is on no entry";
  return at;
}

// The Filter cell is the card's summary (panels.cpp FilterSummary), which prints a multi-row
// sum of products as its FIRST row and "(+N more)" — the filter editor is where the rows are
// read in full. So a composed member's row text is on the page only for the first member; for
// a later one (position k > 0 in its top node's composition) what the page owes is the count:
// "(+N more)" with N >= k, which is how the reader is told the rows exist. Position k, or
// nullopt for a first member or a filter that is nobody's member.
std::optional<int> HiddenRowPosition(const json& doc, int export_idx) {
  for (const auto& f : doc["filter"]) {
    if (!f.contains("composition")) {
      continue;
    }
    const json& members = f["composition"];
    for (size_t k = 1; k < members.size(); ++k) {
      if (members[k].get<int>() == export_idx) {
        return static_cast<int>(k);
      }
    }
  }
  return std::nullopt;
}

// The row-text assertion, in the two forms above.
void ExpectRowText(const json& doc, const ConfigSummary& page, int export_idx, const std::string& text,
                   const char* what) {
  const std::optional<int> hidden = HiddenRowPosition(doc, export_idx);
  for (const auto& at : EntriesReachingFilter(doc, export_idx)) {
    const auto cell = CrystalsCell(page, at, "Filter");
    if (!cell.has_value()) {
      continue;
    }
    if (!hidden.has_value()) {
      EXPECT_NE(cell->find(text), std::string::npos)
          << LocatorText(at) << ": " << what << " \"" << text << "\" is not in the Filter cell \"" << *cell << "\"";
      continue;
    }
    const std::smatch more = [&] {
      std::smatch found;
      std::regex_search(*cell, found, std::regex(R"(\(\+(\d+) more\))"));
      return found;
    }();
    if (more.empty()) {
      ADD_FAILURE() << LocatorText(at) << ": " << what << " \"" << text << "\" is row " << *hidden
                    << " of a multi-row filter, and the Filter cell \"" << *cell << "\" does not say \"(+N more)\"";
      continue;
    }
    EXPECT_GE(std::stoi(more[1]), *hidden)
        << LocatorText(at) << ": the Filter cell \"" << *cell << "\" counts fewer hidden rows than row " << *hidden;
  }
};

std::vector<RuleEntry> BuildRules(const GuiState& state, const ConfigSummary& page, const json& doc) {
  std::vector<RuleEntry> rules;
  const auto add = [&](const char* pattern, Rule rule) { rules.push_back({ std::regex(pattern), std::move(rule) }); };

  // ---- scene ----
  add(R"(^scene\.light_source\.altitude$)",
      [&](const std::smatch&, const json& v) { ExpectPageNumber(page, "Sun", "Altitude", v); });
  add(R"(^scene\.light_source\.diameter$)",
      [&](const std::smatch&, const json& v) { ExpectPageNumber(page, "Sun", "Diameter", v); });
  add(R"(^scene\.light_source\.spectrum$)",
      [&](const std::smatch&, const json& v) { ExpectPageText(page, "Sun", "Spectrum", v.get<std::string>()); });
  add(R"(^scene\.light_source\.spectrum\[\d+\]\.(wavelength|weight)$)", [&](const std::smatch&, const json& v) {
    // A custom spectrum is exported as its table, walked here per cell; the page prints the same
    // table as one row (the array leaf the .lmc carries) and names the preset slot "custom".
    ExpectPageText(page, "Sun", "Spectrum", "custom");
    ExpectPageNumber(page, "Sun", "Custom spectrum", v);
  });
  add(R"(^scene\.max_hits$)",
      [&](const std::smatch&, const json& v) { ExpectPageNumber(page, "Simulation", "Max hits", v); });
  add(R"(^scene\.ray_allocation$)", [&](const std::smatch&, const json& v) {
    // No main-panel control (field_editor_registry.cpp: has_main_panel_surface=false), so the page
    // files it under its own "Settings" heading and not beside Rays(M).
    ExpectPageText(page, kSettingsPopupOnlyGroupTitle, "Ray allocation", v.get<std::string>());
    EXPECT_FALSE(HasRow(page, "Simulation", "Ray allocation"));
  });
  add(R"(^scene\.ray_num$)", [&](const std::smatch&, const json& v) {
    // The page shows the slider's unit (millions); the export the count — or the word "infinite",
    // in which case the panel greys the slider and the page carries Infinite rays = true and no
    // Rays(M) row.
    if (v.is_string()) {
      EXPECT_EQ(v.get<std::string>(), "infinite");
      EXPECT_FALSE(HasRow(page, "Simulation", "Rays(M)"));
      ExpectPageText(page, "Simulation", "Infinite rays", "true");
      return;
    }
    ExpectGatedNumber(state, page, "sim.ray_num_millions", "Simulation", "Rays(M)", json(v.get<double>() / 1.0e6));
  });
  add(R"(^scene\.scattering\[(\d+)\]\.prob$)", [&](const std::smatch& m, const json& v) {
    // The layer's heading line carries its multi-scatter probability, in the slider's "%.2f".
    const ConfigSummaryLayer* layer = PageLayer(page, std::stoi(m[1]));
    if (layer != nullptr) {
      const std::string want = Fmt("%.2f", v.get<double>());
      EXPECT_NE(layer->heading.find(want), std::string::npos)
          << "layer " << m[1] << ": heading \"" << layer->heading << "\" does not carry prob " << want;
    }
  });
  add(R"(^scene\.scattering\[(\d+)\]\.entries\[(\d+)\]\.crystal$)", [&](const std::smatch& m, const json& v) {
    // The page leads the crystal cell with the POOL id (FormatCrystalIdentity's "#N"), so the
    // export's core id has to map back to the pool id of this very entry.
    const EntryLocator at{ std::stoi(m[1]), std::stoi(m[2]) };
    const int pool = PoolIdForCoreId(state, v.get<int>());
    ASSERT_GE(pool, 0);
    EXPECT_EQ(state.layers[at.layer_idx].entries[at.row_idx].crystal_id, pool);
    ExpectCellText(CrystalsCell(page, at, "Crystal"), at, "Crystal", "#" + std::to_string(pool));
    // And the row is the entry's: both tables number it as the card does.
    ExpectCellText(CrystalsCell(page, at, "#"), at, "#", std::to_string(DisplayEntryNumber(at.row_idx)));
    const ConfigSummaryLayer* layer = PageLayer(page, at.layer_idx);
    if (layer != nullptr) {
      ExpectCellText(TableCell(layer->shape, at, "#", "Shape"), at, "#",
                     std::to_string(DisplayEntryNumber(at.row_idx)));
    }
  });
  add(R"(^scene\.scattering\[(\d+)\]\.entries\[(\d+)\]\.proportion$)", [&](const std::smatch& m, const json& v) {
    // An excluded entry is exported at proportion 0 with its weight kept; the page keeps the
    // weight too and says Enabled = false, which is what the user sees on the card.
    const EntryLocator at{ std::stoi(m[1]), std::stoi(m[2]) };
    const EntryCard& card = state.layers[at.layer_idx].entries[at.row_idx];
    if (card.enabled) {
      ExpectCellNumber(CrystalsCell(page, at, "Weight"), at, "Weight", v, "%.1f");
      ExpectCellText(CrystalsCell(page, at, "Enabled"), at, "Enabled", "true");
    } else {
      EXPECT_FLOAT_EQ(v.get<float>(), 0.0f);
      ExpectCellText(CrystalsCell(page, at, "Enabled"), at, "Enabled", "false");
    }
  });

  // ---- render ----
  add(R"(^render\[0\]\.lens\.type$)",
      [&](const std::smatch&, const json& v) { ExpectPageText(page, "Render", "Lens Type", v.get<std::string>()); });
  // The view fields the full-sky gate covers: fov, the three pose angles, visible and front (roll
  // and front under the wider full-sky-or-globe gate). Each is asserted absent when its gate is
  // shut, present with the export's value when open.
  add(R"(^render\[0\]\.lens\.fov$)",
      [&](const std::smatch&, const json& v) { ExpectGatedNumber(state, page, "renderer.fov", "Render", "FOV", v); });
  add(R"(^render\[0\]\.view\.(elevation|azimuth|roll)$)", [&](const std::smatch& m, const json& v) {
    std::string label = m[1];
    label[0] = static_cast<char>(label[0] - 'a' + 'A');
    ExpectGatedNumber(state, page, ("renderer." + m[1].str()).c_str(), "Render", label, v);
  });
  add(R"(^render\[0\]\.visible$)", [&](const std::smatch&, const json& v) {
    ExpectGatedText(state, page, "renderer.visible", "Render", "Visible", v.get<std::string>());
  });
  add(R"(^render\[0\]\.front$)", [&](const std::smatch&, const json& v) {
    ExpectGatedText(state, page, "renderer.front", "Render", "Front", v.get<bool>() ? "true" : "false");
  });
  // Globe only: the far side's fade range, behind the registry's WhenGlobe gate.
  add(R"(^render\[0\]\.globe_back_fade$)", [&](const std::smatch&, const json& v) {
    ExpectGatedNumber(state, page, "renderer.globe_back_fade", "Render", "Back fade", v);
  });
  add(R"(^render\[0\]\.tone$)",
      [&](const std::smatch&, const json& v) { ExpectPageText(page, "Render", "Mode", v.get<std::string>()); });
  add(R"(^render\[0\]\.display_mode$)", [&](const std::smatch&, const json& v) {
    ExpectGatedText(state, page, "renderer.display_mode", "Render", "Show As", v.get<std::string>());
  });
  add(R"(^render\[0\]\.ev_mode$)",
      [&](const std::smatch&, const json& v) { ExpectPageText(page, "Render", "EV Anchor", v.get<std::string>()); });
  // The two grounds: the panel shows ONE swatch, Sky Color under Screen and Paper Color under
  // Print, so the page carries one row — and the export always writes both fields.
  add(R"(^render\[0\]\.background$)", [&](const std::smatch&, const json& v) {
    ExpectGatedNumber(state, page, "renderer.background", "Render", "Sky Color", v);
  });
  add(R"(^render\[0\]\.paper$)", [&](const std::smatch&, const json& v) {
    ExpectGatedNumber(state, page, "renderer.paper", "Render", "Paper Color", v);
  });
  add(R"(^render\[0\]\.intensity_factor$)", [&](const std::smatch&, const json& v) {
    // The export bakes 2^exposure_offset (doc/ev-pipeline-architecture.md §2.4); the page shows
    // the EV the user set. Same quantity, the user's spelling.
    ExpectPageNumber(page, "Render", "EV", json(std::log2(v.get<double>())));
  });
  add(R"(^render\[0\]\.resolution$)", [&](const std::smatch&, const json& v) {
    // The canvas is the sim resolution stretched to the aspect preset (file_io.cpp, the export
    // arm's canvas block): its SHORT edge is the resolution the user picked, and that is what
    // the page prints — the aspect preset itself is a view preference, hidden by tier.
    ASSERT_EQ(v.size(), 2u);
    ExpectPageNumber(page, "Render", "Resolution", json(std::min(v[0].get<int>(), v[1].get<int>())));
  });

  // ---- crystal ----
  add(R"(^crystal\[(\d+)\]\.type$)", [&](const std::smatch& m, const json& v) {
    const std::string want = v.get<std::string>() == "prism" ? "Prism" : "Pyramid";
    for (const auto& at : EntriesForCoreCrystal(state, std::stoi(m[1]))) {
      ExpectCellText(CrystalsCell(page, at, "Crystal"), at, "Crystal", want);
    }
  });
  add(R"(^crystal\[(\d+)\]\.axis\.(zenith|azimuth|roll)\.(type|mean|std)$)", [&](const std::smatch& m, const json& v) {
    std::string column = m[2];
    column[0] = static_cast<char>(column[0] - 'a' + 'A');
    const int core_id = std::stoi(m[1]);
    const CrystalConfig& cr = state.crystals[static_cast<size_t>(PoolIdForCoreId(state, core_id))];
    const AxisDist& axis = m[2] == "zenith" ? cr.zenith : m[2] == "azimuth" ? cr.azimuth : cr.roll;
    for (const auto& at : EntriesForCoreCrystal(state, core_id)) {
      const auto cell = CrystalsCell(page, at, column);
      if (m[3] == "type") {
        ExpectCellDistributionType(cell, at, column, v.get<std::string>());
      } else if (axis_preset_detail::IsFullUniform360(axis)) {
        // The one fold the notation makes: a full-circle uniform is the bare letter, and the two
        // numbers it stands for are asserted on the export instead — the cell hides nothing but
        // 0 and 360 (within the preset classifier's own 1° kEpsilon, since "full circle" is its
        // predicate), and ends in the letter alone.
        EXPECT_NEAR(v.get<float>(), m[3] == "mean" ? 0.0f : 360.0f, 1.0f) << LocatorText(at) << " / " << column;
        if (cell.has_value()) {
          EXPECT_TRUE(*cell == "U" || cell->size() >= 2 && cell->compare(cell->size() - 2, 2, " U") == 0)
              << LocatorText(at) << " / " << column << ": full-circle uniform, cell says \"" << *cell << "\"";
        }
      } else {
        // The mean and the spread are in the cell in the modal's "%.3g".
        ExpectCellNumber(cell, at, column, v, "%.3g");
      }
    }
  });
  add(R"(^crystal\[(\d+)\]\.shape\.(height|prism_h|upper_h|lower_h)$)", [&](const std::smatch& m, const json& v) {
    const int slot = ShapeSlotForExportKey(m[2]);
    ASSERT_GE(slot, 0);
    // A fixed scalar exports as a bare number, a randomized one as {type, mean, std}; the cell
    // prints the centre in the slot's format either way, and the letter and spread around it.
    const char* fmt = ShapeScalarDomainFor(slot).fmt;
    const std::string column = kShapeScalarLabels[slot];
    for (const auto& at : EntriesForCoreCrystal(state, std::stoi(m[1]))) {
      const auto cell = ShapeCell(page, at, slot);
      if (v.is_number()) {
        ExpectCellNumber(cell, at, column, v, fmt);
      } else {
        ExpectCellNumber(cell, at, column, v["mean"], fmt);
        ExpectCellNumber(cell, at, column, v["std"], fmt);
        ExpectCellDistributionType(cell, at, column, v["type"].get<std::string>());
      }
    }
  });
  add(R"(^crystal\[(\d+)\]\.shape\.(height|prism_h|upper_h|lower_h)\.(type|mean|std)$)",
      [&](const std::smatch& m, const json& v) {
        // The randomized form walked to its scalars (Flatten only keeps numeric arrays whole).
        const int slot = ShapeSlotForExportKey(m[2]);
        ASSERT_GE(slot, 0);
        const std::string column = kShapeScalarLabels[slot];
        for (const auto& at : EntriesForCoreCrystal(state, std::stoi(m[1]))) {
          const auto cell = ShapeCell(page, at, slot);
          if (m[3] == "type") {
            ExpectCellDistributionType(cell, at, column, v.get<std::string>());
          } else {
            ExpectCellNumber(cell, at, column, v, ShapeScalarDomainFor(slot).fmt);
          }
        }
      });
  add(R"(^crystal\[(\d+)\]\.shape\.(upper|lower)_wedge_angle$)", [&](const std::smatch& m, const json& v) {
    const std::string column = m[2] == "upper" ? "Upper A" : "Lower A";
    for (const auto& at : EntriesForCoreCrystal(state, std::stoi(m[1]))) {
      ExpectCellNumber(ShapeCellNamed(page, at, column), at, column, v, "%.3f");
    }
  });
  add(R"(^crystal\[(\d+)\]\.shape\.face_distance$)", [&](const std::smatch& m, const json& v) {
    // Six fixed faces export as one numeric array; each is its own cell of the Shape table.
    ASSERT_EQ(v.size(), 6u);
    for (const auto& at : EntriesForCoreCrystal(state, std::stoi(m[1]))) {
      for (int i = 0; i < 6; ++i) {
        const int slot = LUMICE_SHAPE_SCALAR_FACE_0 + i;
        ExpectCellNumber(ShapeCell(page, at, slot), at, kShapeScalarLabels[slot], v[i], ShapeScalarDomainFor(slot).fmt);
      }
    }
  });
  add(R"(^crystal\[(\d+)\]\.shape\.face_distance\[(\d+)\]\.(type|mean|std)$)",
      [&](const std::smatch& m, const json& v) {
        // A randomized face turns the sextet into an array of objects, walked per face.
        const int slot = LUMICE_SHAPE_SCALAR_FACE_0 + std::stoi(m[2]);
        for (const auto& at : EntriesForCoreCrystal(state, std::stoi(m[1]))) {
          const auto cell = ShapeCell(page, at, slot);
          if (m[3] == "type") {
            ExpectCellDistributionType(cell, at, kShapeScalarLabels[slot], v.get<std::string>());
          } else {
            ExpectCellNumber(cell, at, kShapeScalarLabels[slot], v, ShapeScalarDomainFor(slot).fmt);
          }
        }
      });
  add(R"(^crystal\[(\d+)\]\.shape\.face_distance\[(\d+)\]$)", [&](const std::smatch& m, const json& v) {
    // A fixed face beside randomized siblings: a bare number inside the object array.
    const int slot = LUMICE_SHAPE_SCALAR_FACE_0 + std::stoi(m[2]);
    for (const auto& at : EntriesForCoreCrystal(state, std::stoi(m[1]))) {
      ExpectCellNumber(ShapeCell(page, at, slot), at, kShapeScalarLabels[slot], v, ShapeScalarDomainFor(slot).fmt);
    }
  });
  add(R"(^crystal\[(\d+)\]\.shape\.sync_group.*$)", [&](const std::smatch& m, const json& v) {
    // The page marks a grouped slot "· sync N"; the export lists the group per slot name. Every
    // non-zero group number in the export has to appear in that crystal's cells, in either table.
    for (const auto& at : EntriesForCoreCrystal(state, std::stoi(m[1]))) {
      const ConfigSummaryLayer* layer = PageLayer(page, at.layer_idx);
      if (layer == nullptr) {
        continue;
      }
      const auto has_marker = [&](int group) {
        const std::string marker = "sync " + std::to_string(group);
        for (const ConfigSummaryTable* table : { &layer->crystals, &layer->shape }) {
          if (static_cast<size_t>(at.row_idx) >= table->rows.size()) {
            continue;
          }
          for (const auto& cell : table->rows[static_cast<size_t>(at.row_idx)].cells) {
            if (cell.find(marker) != std::string::npos) {
              return true;
            }
          }
        }
        return false;
      };
      // One slot's group, or the face sextet's six.
      const json groups = v.is_array() ? v : json::array({ v });
      for (const auto& g_id : groups) {
        if (g_id.is_number_integer() && g_id.get<int>() != 0) {
          EXPECT_TRUE(has_marker(g_id.get<int>()))
              << LocatorText(at) << ": sync group " << g_id.get<int>() << " not on the page";
        }
      }
    }
  });
  add(R"(^crystal\[(\d+)\]\.name$)", [&](const std::smatch& m, const json& v) {
    for (const auto& at : EntriesForCoreCrystal(state, std::stoi(m[1]))) {
      ExpectCellText(CrystalsCell(page, at, "Crystal"), at, "Crystal", v.get<std::string>());
    }
  });

  // ---- filter ----
  add(R"(^filter\[(\d+)\]\.action$)", [&](const std::smatch& m, const json& v) {
    const std::string want = v.get<std::string>() == "filter_in" ? " In" : " Out";
    for (const auto& at : EntriesReachingFilter(doc, std::stoi(m[1]))) {
      ExpectCellText(CrystalsCell(page, at, "Filter"), at, "Filter", want);
    }
  });
  add(R"(^filter\[(\d+)\]\.symmetry$)", [&](const std::smatch& m, const json& v) {
    // FilterSummary's suffix is the letters of the symmetries that are ON — the same letters, in
    // the same order, the export writes. The cell may carry " · <name>" after the summary.
    for (const auto& at : EntriesReachingFilter(doc, std::stoi(m[1]))) {
      const auto got = CrystalsCell(page, at, "Filter");
      if (got.has_value()) {
        std::string summary = *got;
        const size_t name_at = summary.find(" · ");
        if (name_at != std::string::npos) {
          summary = summary.substr(0, name_at);
        }
        const size_t sp = summary.rfind(' ');
        const std::string sym = sp == std::string::npos ? "" : summary.substr(sp + 1);
        EXPECT_EQ(sym == "In" || sym == "Out" ? "" : sym, v.get<std::string>())
            << LocatorText(at) << ": the cell says \"" << *got << "\"";
      }
    }
  });
  add(R"(^filter\[(\d+)\]\.raypath$)", [&](const std::smatch& m, const json& v) {
    // The face-number list, as the editor's row text spells it ("3-5-1").
    std::string text;
    for (const auto& face : v) {
      text += (text.empty() ? "" : "-") + std::to_string(face.get<int>());
    }
    ExpectRowText(doc, page, std::stoi(m[1]), text, "raypath");
  });
  add(R"(^filter\[(\d+)\]\.(entry|exit)$)", [&](const std::smatch& m, const json& v) {
    // An entry-exit row: each face number the export names is in the cell.
    const json faces = v.is_array() ? v : json::array({ v });
    for (const auto& face : faces) {
      ExpectRowText(doc, page, std::stoi(m[1]), std::to_string(face.get<int>()), m[2].str().c_str());
    }
  });
  add(R"(^filter\[(\d+)\]\.(min_len|max_len|length)$)", [&](const std::smatch& m, const json& v) {
    ExpectRowText(doc, page, std::stoi(m[1]), std::to_string(v.get<int>()), m[2].str().c_str());
  });
  return rules;
}

// The chain itself, over one document.
void ExpectPageMatchesExport(const GuiState& state, const char* scenario) {
  SCOPED_TRACE(scenario);
  std::string out;
  std::string warning;
  ASSERT_TRUE(BuildExportJsonOrWarn(state, &out, &warning)) << warning;
  const json doc = json::parse(out);
  const ConfigSummary page = BuildConfigSummary(state);

  std::vector<Leaf> leaves;
  Flatten("", doc, leaves);
  ASSERT_FALSE(leaves.empty());

  std::vector<std::regex> excluded;
  for (const auto& ex : kExcluded) {
    excluded.emplace_back(ex.pattern);
  }
  const auto rules = BuildRules(state, page, doc);

  int matched = 0;
  for (const auto& leaf : leaves) {
    bool accounted = false;
    for (const auto& ex : excluded) {
      if (std::regex_match(leaf.path, ex)) {
        accounted = true;
        break;
      }
    }
    if (accounted) {
      continue;
    }
    for (const auto& rule : rules) {
      std::smatch m;
      if (std::regex_match(leaf.path, m, rule.pattern)) {
        rule.rule(m, leaf.value);
        accounted = true;
        ++matched;
        break;
      }
    }
    // The meta-assertion: a leaf the export writes that no rule reads and no exclusion names.
    EXPECT_TRUE(accounted) << "exported leaf \"" << leaf.path << "\" = " << leaf.value.dump()
                           << " is neither matched to the page nor explicitly excluded";
  }
  EXPECT_GT(matched, 10) << "the rules matched almost nothing — the export shape has moved";

  // Both directions at once, over every registered settings leaf rather than only the ones the
  // rules above happen to name: a leaf the panel enables is on the page under its panel label,
  // and a leaf on the page is one the panel enables. The label is the registry's here (this is
  // the membership claim; the spelling claim is the rules' literals above) and a popup-only leaf
  // is looked for under the "Settings" heading, which has no gate.
  for (const auto& row : BuildConfigSummaryRows(state)) {
    const FieldEditorEntry* editor = FindFieldEditor(row.key_path);
    if (editor == nullptr) {
      continue;
    }
    const std::string root = row.key_path.substr(0, row.key_path.find('.'));
    const std::string leaf = row.key_path.substr(root.size() + 1);
    const char* declared = LabelFor(row.key_path);
    // An undeclared label is the key's own words ("ray_allocation" -> "Ray allocation"); compared
    // case-insensitively with underscores as spaces so the page's capitalisation is not restated.
    std::string label = declared != nullptr ? declared : leaf;
    const auto same_words = [](std::string a, std::string b) {
      for (std::string* t : { &a, &b }) {
        for (auto& c : *t) {
          c = c == '_' ? ' ' : static_cast<char>(std::tolower(c));
        }
      }
      return a == b;
    };
    const auto has_row_words = [&](const char* title) {
      const ConfigSummaryGroup* g = FindGroup(page, title);
      if (g == nullptr) {
        return false;
      }
      for (const auto& f : g->fields) {
        if (same_words(f.label, label)) {
          return true;
        }
      }
      return false;
    };
    if (!editor->has_main_panel_surface) {
      EXPECT_TRUE(has_row_words(kSettingsPopupOnlyGroupTitle))
          << row.key_path << " has no main-panel control and is not under the Settings heading";
      for (const char* title : { "Sun", "Simulation", "Render" }) {
        EXPECT_FALSE(has_row_words(title)) << row.key_path << " is under " << title << " as well";
      }
      continue;
    }
    const char* title = root == "sun" ? "Sun" : root == "sim" ? "Simulation" : root == "renderer" ? "Render" : "";
    if (*title == '\0') {
      ADD_FAILURE() << row.key_path << ": a registered leaf under a root this chain has no group title for";
      continue;
    }
    const bool enabled = ConstraintFor(row.key_path, state).enabled;
    EXPECT_EQ(has_row_words(title), enabled) << row.key_path << ": panel " << (enabled ? "enables" : "disables")
                                             << " it, page " << (has_row_words(title) ? "prints" : "omits") << " it";
  }

  // The other half of the design constraint: nothing on the page that the GUI does not show. The
  // one exported field that is user-INvisible by design is the sun's azimuth; the Sun group must
  // not have an Azimuth row (the Render group's Azimuth is the camera's, a real control).
  const ConfigSummaryGroup* sun = FindGroup(page, "Sun");
  ASSERT_NE(sun, nullptr);
  for (const auto& f : sun->fields) {
    EXPECT_NE(f.label, "Azimuth") << "the sun's azimuth is on the page, and there is no control for it";
  }
  ASSERT_TRUE(doc["scene"]["light_source"].contains("azimuth"))
      << "the export no longer writes the sun azimuth; the absence assertion above is vacuous";
}

// ---- Documents ---------------------------------------------------------------------------

void SeedDefaultDocument() {
  DoNew();
}

// Every renderer field off its default, the linear lens the constraint names, and the front
// clip on — the document on which a commit-arm anchoring is most visibly wrong.
void SeedLinearFrontDocument() {
  DoNew();
  g_state.renderer.lens_type = kLensTypeLinear;
  g_state.renderer.fov = 55.0f;
  g_state.renderer.elevation = 30.0f;
  g_state.renderer.azimuth = 77.0f;
  g_state.renderer.roll = 5.0f;
  g_state.renderer.visible = 0;
  g_state.renderer.front = true;
  g_state.renderer.exposure_offset = 1.5f;
  g_state.renderer.ev_mode = 1;
  g_state.renderer.tone = 1;
  g_state.renderer.sim_resolution_index = 2;
  g_state.renderer.background[0] = 0.1f;
  g_state.renderer.background[1] = 0.2f;
  g_state.renderer.background[2] = 0.3f;
  g_state.renderer.paper[0] = 0.9f;
  g_state.renderer.paper[1] = 0.8f;
  g_state.renderer.paper[2] = 0.7f;
  g_state.sun.altitude = 33.0f;
  g_state.sun.diameter = 0.7f;
  g_state.sun.spectrum_index = 4;
  g_state.sim.max_hits = 12;
  g_state.sim.ray_num_millions = 2.5f;
  g_state.sim.ray_allocation_adaptive = false;
  g_state.aspect_preset = AspectPreset::k16x9;
}

// Two layers, three entries, both crystal types, a Plate preset, randomized and synced shape
// scalars, a named raypath filter, a multi-row SoP filter, an excluded entry, and a custom
// spectrum — the document section with every branch taken.
// The one lens that shows the back-fade row: globe, with the range off its default.
void SeedGlobeDocument() {
  DoNew();
  g_state.renderer.lens_type = kLensTypeGlobe;
  g_state.renderer.fov = 30.0f;
  g_state.renderer.elevation = 20.0f;
  g_state.renderer.azimuth = 40.0f;
  g_state.renderer.globe_back_fade = 0.6f;
}

void SeedRichDocument() {
  DoNew();
  g_state.crystals.assign(2, CrystalConfig{});
  CrystalConfig& plate = g_state.crystals[0];
  plate.name = "plate";
  plate.type = CrystalType::kPrism;
  plate.height = ShapeDist{ ShapeDistType::kUniform, 0.3f, 0.05f };
  plate.zenith = AxisDist{ AxisDistType::kGauss, 0.0f, 2.5f };
  plate.azimuth = kAzFullUniform;
  plate.roll = kRollFreeUniform;
  CrystalConfig& pyr = g_state.crystals[1];
  pyr.type = CrystalType::kPyramid;
  pyr.prism_h = 1.7f;
  pyr.upper_h = 0.35f;
  pyr.lower_h = 0.15f;
  pyr.upper_alpha = 31.5f;
  pyr.lower_alpha = 25.0f;
  pyr.face_distance[2] = ShapeDist{ ShapeDistType::kGauss, 0.9f, 0.1f };
  pyr.face_distance[0].sync_group = 1;
  pyr.face_distance[1].sync_group = 1;
  pyr.zenith = AxisDist{ AxisDistType::kLaplacian, 90.0f, 3.0f };
  pyr.azimuth = AxisDist{ AxisDistType::kZigzag, 10.0f, 20.0f };
  pyr.roll = kRollLockedGauss;

  g_state.filters.assign(2, FilterConfig{});
  g_state.filters[0].name = "cza";
  g_state.filters[0].SetRaypath(RaypathParams{ "3-5-1" });
  g_state.filters[0].action = 0;
  g_state.filters[0].sym_p = true;
  g_state.filters[0].sym_b = false;
  g_state.filters[0].sym_d = true;
  FilterConfig& sop = g_state.filters[1];
  sop.name = "two rows";
  sop.action = 1;
  sop.sym_p = false;
  sop.sym_b = true;
  sop.sym_d = false;
  for (const std::string& row : { std::string("3-5"), std::string("1-3-2") }) {
    sop.param.push_back(SummandText{ row, ParseSummandText(row) });
  }

  Layer first;
  first.probability = 0.35f;
  EntryCard a;
  a.crystal_id = 0;
  a.filter_id = 0;
  a.proportion = 60.0f;
  EntryCard b;
  b.crystal_id = 1;
  b.filter_id = 1;
  b.proportion = 40.0f;
  first.entries = { a, b };
  g_state.filters.push_back(FilterConfig{});
  FilterConfig& ee = g_state.filters[2];
  EntryExitParams ep;
  ep.entry_text = "3";
  ep.exit_text = "5";
  ep.length_mode = 1;
  ep.min_len = 4;
  ee.SetEntryExit(ep);

  Layer second;
  second.probability = 0.0f;
  EntryCard c;
  c.crystal_id = 0;
  c.proportion = 25.0f;
  c.enabled = false;
  EntryCard d;
  d.crystal_id = 1;
  d.filter_id = 2;
  d.proportion = 75.0f;
  second.entries = { c, d };
  g_state.layers = { first, second };

  g_state.sun.spectrum_index = kCustomSpectrumIndex;
  g_state.sun.custom_spectrum = { { 450.0f, 1.0f }, { 550.0f, 0.5f }, { 620.0f, 0.25f } };
}

// The default document under a full-sky lens: fov, the pose angles, visible and front all stop
// applying on the panel (NotUnderFullSky / NotUnderFullSkyOrGlobe), while the export keeps
// writing each of them.
void SeedFullSkyDocument() {
  DoNew();
  g_state.renderer.lens_type = kLensTypeDualFisheyeEqualArea;
  g_state.renderer.fov = 180.0f;
  g_state.renderer.elevation = 12.0f;
  g_state.renderer.azimuth = 34.0f;
}

// The default document with Infinite rays on: the Rays(M) slider is greyed on the panel, the
// export writes a count regardless.
void SeedInfiniteRaysDocument() {
  DoNew();
  g_state.sim.infinite = true;
  g_state.sim.ray_num_millions = 7.0f;
}

}  // namespace

TEST(ConfigSummaryExportParityChain, DefaultDocument) {
  SeedDefaultDocument();
  ExpectPageMatchesExport(g_state, "default");
  // Screen tone: the sky's row, not the paper's — spelled out beside the gated rule so the two
  // grounds' exclusivity is a stated fact of this document and not only a consequence of the gate.
  const ConfigSummary page = BuildConfigSummary(g_state);
  EXPECT_TRUE(HasRow(page, "Render", "Sky Color"));
  EXPECT_FALSE(HasRow(page, "Render", "Paper Color"));
  // Rays(M) on, Ray allocation under its own heading and nowhere else.
  EXPECT_TRUE(HasRow(page, "Simulation", "Rays(M)"));
  EXPECT_TRUE(HasRow(page, kSettingsPopupOnlyGroupTitle, "Ray allocation"));
  EXPECT_FALSE(HasRow(page, "Simulation", "Ray allocation"));
}

TEST(ConfigSummaryExportParityChain, FullSkyLensHidesTheViewFieldsThePanelGreys) {
  SeedFullSkyDocument();
  ExpectPageMatchesExport(g_state, "full_sky");
  const ConfigSummary page = BuildConfigSummary(g_state);
  for (const char* label : { "FOV", "Elevation", "Azimuth", "Roll", "Visible", "Front" }) {
    EXPECT_FALSE(HasRow(page, "Render", label)) << label;
  }
  EXPECT_EQ(PageValue(page, "Render", "Lens Type").value_or(""), "dual_fisheye_equal_area");
  // The export still carries them: the page is what differs, not the document.
  std::string out;
  std::string warning;
  ASSERT_TRUE(BuildExportJsonOrWarn(g_state, &out, &warning)) << warning;
  const json doc = json::parse(out);
  EXPECT_TRUE(doc["render"][0].contains("fov") || doc["render"][0]["lens"].contains("fov"));
  EXPECT_TRUE(doc["render"][0].contains("view"));
}

TEST(ConfigSummaryExportParityChain, InfiniteRaysHidesTheRayTotalThePanelGreys) {
  SeedInfiniteRaysDocument();
  ExpectPageMatchesExport(g_state, "infinite");
  const ConfigSummary page = BuildConfigSummary(g_state);
  EXPECT_FALSE(HasRow(page, "Simulation", "Rays(M)"));
  EXPECT_EQ(PageValue(page, "Simulation", "Infinite rays").value_or(""), "true");
  std::string out;
  std::string warning;
  ASSERT_TRUE(BuildExportJsonOrWarn(g_state, &out, &warning)) << warning;
  EXPECT_TRUE(json::parse(out)["scene"].contains("ray_num"));
}

TEST(ConfigSummaryExportParityChain, LinearFrontDocumentPrintsTheUsersLens) {
  SeedLinearFrontDocument();
  ExpectPageMatchesExport(g_state, "linear_front");
  // Spelled out as well as reached through the rules: the constraint's own example. The commit
  // arm would say dual_fisheye_equal_area / 180 / view (0,0,0) here.
  const ConfigSummary page = BuildConfigSummary(g_state);
  EXPECT_EQ(PageValue(page, "Render", "Lens Type").value_or(""), "linear");
  EXPECT_EQ(PageValue(page, "Render", "FOV").value_or(""), "55");
  EXPECT_EQ(PageValue(page, "Render", "Azimuth").value_or(""), "77");
  EXPECT_EQ(PageValue(page, "Render", "Visible").value_or(""), "upper");
  EXPECT_EQ(PageValue(page, "Render", "Front").value_or(""), "true");
  EXPECT_EQ(PageValue(page, "Render", "EV").value_or(""), "1.5");
  EXPECT_EQ(PageValue(page, "Render", "Resolution").value_or(""), "2048");
  // This document is under Print (tone = 1): the paper's row, not the sky's — the mirror image of
  // DefaultDocument's pair, so the two cases together are the exclusivity in both tones.
  EXPECT_EQ(PageValue(page, "Render", "Mode").value_or(""), "print");
  EXPECT_TRUE(HasRow(page, "Render", "Paper Color"));
  EXPECT_FALSE(HasRow(page, "Render", "Sky Color"));
}

TEST(ConfigSummaryExportParityChain, GlobeDocumentPrintsTheBackFade) {
  SeedGlobeDocument();
  ExpectPageMatchesExport(g_state, "globe");
  const ConfigSummary page = BuildConfigSummary(g_state);
  EXPECT_EQ(PageValue(page, "Render", "Lens Type").value_or(""), "globe");
  EXPECT_EQ(PageValue(page, "Render", "Back fade").value_or(""), "0.6");
  std::string out;
  std::string warning;
  ASSERT_TRUE(BuildExportJsonOrWarn(g_state, &out, &warning)) << warning;
  EXPECT_FLOAT_EQ(json::parse(out)["render"][0]["globe_back_fade"].get<float>(), 0.6f);
}

TEST(ConfigSummaryExportParityChain, RichDocument) {
  SeedRichDocument();
  ExpectPageMatchesExport(g_state, "rich");
  const ConfigSummary page = BuildConfigSummary(g_state);
  // Spelled out as well as reached through the rules: the cells as the reader sees them.
  const EntryLocator l1e1{ 0, 0 };
  const EntryLocator l1e2{ 0, 1 };
  const EntryLocator l2e1{ 1, 0 };
  EXPECT_EQ(CrystalsCell(page, l1e1, "Zenith").value_or(""), "Plate · G 0(2.5)");
  EXPECT_EQ(CrystalsCell(page, l1e1, "Azimuth").value_or(""), "U");
  EXPECT_EQ(CrystalsCell(page, l1e1, "Roll").value_or(""), "U");
  EXPECT_EQ(ShapeCell(page, l1e1, LUMICE_SHAPE_SCALAR_HEIGHT).value_or(""), "U 0.3(0.05)");
  EXPECT_EQ(ShapeCell(page, l1e1, LUMICE_SHAPE_SCALAR_PRISM_H).value_or(""), "");
  EXPECT_EQ(CrystalsCell(page, l1e1, "Filter").value_or(""), "3-5-1 In PD · cza");
  EXPECT_EQ(ShapeCell(page, l1e2, LUMICE_SHAPE_SCALAR_HEIGHT).value_or(""), "");
  EXPECT_EQ(ShapeCell(page, l1e2, LUMICE_SHAPE_SCALAR_PRISM_H).value_or(""), "1.7");
  EXPECT_EQ(ShapeCellNamed(page, l1e2, "Upper A").value_or(""), "31.500");
  EXPECT_EQ(CrystalsCell(page, l1e2, "Zenith").value_or(""), "Custom · L 90(3)");
  EXPECT_EQ(CrystalsCell(page, l1e2, "Azimuth").value_or(""), "Z 10(20)");
  EXPECT_EQ(CrystalsCell(page, l1e2, "Roll").value_or(""), "G 0(1)");
  // The two-row filter: first row, the hidden count, the suffix, the name.
  EXPECT_EQ(CrystalsCell(page, l1e2, "Filter").value_or(""), "3-5 (+1 more) Out B · two rows");
  EXPECT_EQ(ShapeCell(page, l1e2, LUMICE_SHAPE_SCALAR_FACE_2).value_or(""), "G 0.900(0.100)");
  EXPECT_EQ(ShapeCell(page, l1e2, LUMICE_SHAPE_SCALAR_FACE_0).value_or(""), "1.000 · sync 1");
  EXPECT_EQ(CrystalsCell(page, l2e1, "Enabled").value_or(""), "false");
  ASSERT_EQ(page.document.size(), 2u);
  EXPECT_EQ(page.document[0].heading, "Layer 1 · Multi-scatter prob. 0.35 · 2 entries");
  EXPECT_EQ(page.document[1].heading, "Layer 2 · Multi-scatter prob. 0.00 · 2 entries");
  EXPECT_FALSE(page.version.empty());
}

}  // namespace lumice::gui
