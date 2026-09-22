#include "gui/config_summary.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <optional>
#include <utility>

#include "gui/axis_presets.hpp"
#include "gui/defaults_diff.hpp"
#include "gui/field_editor_registry.hpp"
#include "gui/file_io.hpp"
#include "gui/gui_logger.hpp"
#include "gui/gui_state_tiers.hpp"
#include "gui/panels.hpp"
#include "gui/raypath_segments.hpp"
#include "gui/shape_scalar_domain.hpp"
#include "include/lumice.h"

namespace lumice::gui {

namespace {

using nlohmann::json;

std::string Format(const char* fmt, double value) {
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), fmt, value);
  return buffer;
}

// "exposure_offset" -> "Exposure offset". Display only: the serialized key is what the walk
// carries, this is how the page spells it. No word list — a key the page has never seen gets the
// same treatment as every other, which is what lets a new field arrive with no edit here.
std::string Humanize(std::string_view key) {
  std::string out;
  out.reserve(key.size());
  for (const char c : key) {
    out.push_back(c == '_' ? ' ' : c);
  }
  if (!out.empty() && out[0] >= 'a' && out[0] <= 'z') {
    out[0] = static_cast<char>(out[0] - 'a' + 'A');
  }
  return out;
}

// The settings groups' titles and their order on the page. A root key not in this list still
// gets a group — titled by Humanize and placed after these — so the list only decides how the
// three known groups read; it cannot hide a fourth.
struct SettingsGroupTitle {
  const char* root_key;
  const char* title;
};
constexpr SettingsGroupTitle kSettingsGroupTitles[] = {
  { "sun", "Sun" },
  { "sim", "Simulation" },
  { "renderer", "Render" },
};

const char* SettingsGroupTitleFor(std::string_view root_key) {
  for (const auto& entry : kSettingsGroupTitles) {
    if (root_key == entry.root_key) {
      return entry.title;
    }
  }
  return nullptr;
}

// The distribution letters, indexed by AxisDistType. The one table behind DistributionLetter,
// DistributionLetterForWireName and DistributionLegend; a sixth AxisDistType has to be given a
// letter here before the build passes again.
constexpr const char* kDistributionLetters[] = { "G", "U", "Z", "L", "G*" };
static_assert(sizeof(kDistributionLetters) / sizeof(kDistributionLetters[0]) ==
                  static_cast<size_t>(AxisDistType::kCount),
              "Update kDistributionLetters when adding AxisDistType");

// The notation itself (see the header): `fmt` is the number format, `wire_name` the type's wire
// spelling, `no_random` a fixed shape scalar, `full_circle` a full-circle uniform axis.
std::string FormatDistributionCell(const char* fmt, bool no_random, std::string_view wire_name, bool full_circle,
                                   double center, double spread) {
  if (no_random) {
    return Format(fmt, center);
  }
  const char* letter = DistributionLetterForWireName(wire_name);
  if (full_circle) {
    return letter;
  }
  return std::string(letter) + " " + Format(fmt, center) + "(" + Format(fmt, spread) + ")";
}

// The two document tables' column lists. Each builder sizes a row from its list and fills cells
// by these indices, which is what holds ConfigSummaryTable's "every row has columns.size() cells"
// invariant without a check at draw time.
enum CrystalsColumn : size_t {
  kColNumber = 0,
  kColEnabled,
  kColWeight,
  kColCrystal,
  kColZenith,
  kColAzimuth,
  kColRoll,
  kColFilter,
  kCrystalsColumnCount,
};

std::vector<std::string> CrystalsColumns() {
  std::vector<std::string> columns(kCrystalsColumnCount);
  columns[kColNumber] = "#";
  columns[kColEnabled] = "Enabled";
  columns[kColWeight] = "Weight";
  columns[kColCrystal] = "Crystal";
  columns[kColZenith] = "Zenith";
  columns[kColAzimuth] = "Azimuth";
  columns[kColRoll] = "Roll";
  columns[kColFilter] = "Filter";
  return columns;
}

enum ShapeColumn : size_t {
  kShapeColNumber = 0,
  kShapeColHeight,
  kShapeColPrismH,
  kShapeColUpperH,
  kShapeColLowerH,
  kShapeColUpperA,
  kShapeColLowerA,
  kShapeColFace0,  // six consecutive face columns from here
  kShapeColumnCount = kShapeColFace0 + 6,
};

std::vector<std::string> ShapeColumns() {
  std::vector<std::string> columns(kShapeColumnCount);
  columns[kShapeColNumber] = "#";
  // The shape scalars' labels are the edit modal's, read from the one table that owns them.
  columns[kShapeColHeight] = kShapeScalarLabels[LUMICE_SHAPE_SCALAR_HEIGHT];
  columns[kShapeColPrismH] = kShapeScalarLabels[LUMICE_SHAPE_SCALAR_PRISM_H];
  columns[kShapeColUpperH] = kShapeScalarLabels[LUMICE_SHAPE_SCALAR_UPPER_H];
  columns[kShapeColLowerH] = kShapeScalarLabels[LUMICE_SHAPE_SCALAR_LOWER_H];
  // The two wedge angles are not shape-scalar slots (no distribution, no sync); the modal's
  // words for them.
  columns[kShapeColUpperA] = "Upper A";
  columns[kShapeColLowerA] = "Lower A";
  for (int i = 0; i < 6; ++i) {
    columns[kShapeColFace0 + static_cast<size_t>(i)] = kShapeScalarLabels[LUMICE_SHAPE_SCALAR_FACE_0 + i];
  }
  return columns;
}

// The entry's crystal, or null for a dangling crystal id (the card prints "<missing>" for it and
// there is nothing more to describe).
const CrystalConfig* CrystalOfEntry(const GuiState& state, const EntryCard& entry) {
  if (entry.crystal_id < 0 || static_cast<size_t>(entry.crystal_id) >= state.crystals.size()) {
    return nullptr;
  }
  return &state.crystals[static_cast<size_t>(entry.crystal_id)];
}

// The Crystals table of layer `layer_idx`: one row per entry, in the layer's order.
ConfigSummaryTable BuildCrystalsTable(const GuiState& state, int layer_idx) {
  const Layer& layer = state.layers[static_cast<size_t>(layer_idx)];
  ConfigSummaryTable table;
  table.columns = CrystalsColumns();
  for (int entry_idx = 0; entry_idx < static_cast<int>(layer.entries.size()); ++entry_idx) {
    const EntryCard& entry = layer.entries[static_cast<size_t>(entry_idx)];
    ConfigSummaryTable::Row row;
    row.cells.resize(table.columns.size());
    row.cells[kColNumber] = std::to_string(DisplayEntryNumber(entry_idx));
    // The card's own words: the identity triple as the card and the Colors window spell it.
    row.cells[kColCrystal] = FormatCrystalIdentity(state, entry.crystal_id);
    const CrystalConfig* cr = CrystalOfEntry(state, entry);
    if (cr == nullptr) {
      table.rows.push_back(std::move(row));
      continue;
    }
    row.cells[kColEnabled] = entry.enabled ? "true" : "false";
    row.cells[kColWeight] = Format("%.1f", entry.proportion);

    // The Axis tab: the preset the classifier reads off the three distributions leads the Zenith
    // cell, and the three distributions follow in full — a preset name alone would hide a retuned
    // zenith std, the one parameter of a preset the user can change and the one most worth
    // comparing; the name alone in a column of its own would cost a column for a word that
    // belongs to the zenith anyway.
    row.cells[kColZenith] = AxisPresetName(*cr) + " · " + FormatAxisDistCell(cr->zenith);
    row.cells[kColAzimuth] = FormatAxisDistCell(cr->azimuth);
    row.cells[kColRoll] = FormatAxisDistCell(cr->roll);

    // The Filter tab, as the card summarises it, and the filter's name after it when it has one.
    // The card's summary truncates a long raypath and collapses a multi-row sum of products to
    // its first row "(+N more)"; the page keeps that spelling — the filter editor is the place a
    // multi-row filter is read in full, and a column that grew a line per row would undo the
    // table's one-entry-one-row shape for the one field that is rarely more than a row.
    std::optional<FilterConfig> filter;
    if (entry.filter_id.has_value() && *entry.filter_id >= 0 &&
        static_cast<size_t>(*entry.filter_id) < state.filters.size()) {
      filter = state.filters[static_cast<size_t>(*entry.filter_id)];
    }
    row.cells[kColFilter] = FilterSummary(filter);
    if (filter.has_value() && !filter->name.empty()) {
      row.cells[kColFilter] += " · " + filter->name;
    }
    table.rows.push_back(std::move(row));
  }
  return table;
}

// The Shape table of layer `layer_idx`: row i is entry i, as in the Crystals table. The edit
// modal's Crystal tab, column for column: the type's own scalars (a prism fills Height, a pyramid
// the other five, the rest left empty), then the six faces.
ConfigSummaryTable BuildShapeTable(const GuiState& state, int layer_idx) {
  const Layer& layer = state.layers[static_cast<size_t>(layer_idx)];
  ConfigSummaryTable table;
  table.columns = ShapeColumns();
  for (int entry_idx = 0; entry_idx < static_cast<int>(layer.entries.size()); ++entry_idx) {
    const EntryCard& entry = layer.entries[static_cast<size_t>(entry_idx)];
    ConfigSummaryTable::Row row;
    row.cells.resize(table.columns.size());
    row.cells[kShapeColNumber] = std::to_string(DisplayEntryNumber(entry_idx));
    const CrystalConfig* cr = CrystalOfEntry(state, entry);
    if (cr == nullptr) {
      table.rows.push_back(std::move(row));
      continue;
    }
    if (cr->type == CrystalType::kPrism) {
      row.cells[kShapeColHeight] = FormatShapeDistCell(cr->height, LUMICE_SHAPE_SCALAR_HEIGHT);
    } else {
      row.cells[kShapeColPrismH] = FormatShapeDistCell(cr->prism_h, LUMICE_SHAPE_SCALAR_PRISM_H);
      row.cells[kShapeColUpperH] = FormatShapeDistCell(cr->upper_h, LUMICE_SHAPE_SCALAR_UPPER_H);
      row.cells[kShapeColLowerH] = FormatShapeDistCell(cr->lower_h, LUMICE_SHAPE_SCALAR_LOWER_H);
      row.cells[kShapeColUpperA] = Format("%.3f", cr->upper_alpha);
      row.cells[kShapeColLowerA] = Format("%.3f", cr->lower_alpha);
    }
    for (int i = 0; i < 6; ++i) {
      row.cells[kShapeColFace0 + static_cast<size_t>(i)] =
          FormatShapeDistCell(cr->face_distance[static_cast<size_t>(i)], LUMICE_SHAPE_SCALAR_FACE_0 + i);
    }
    table.rows.push_back(std::move(row));
  }
  return table;
}

}  // namespace

bool ConfigSummaryIncludesRootKey(std::string_view key) {
  for (const char* excluded : kDiffEngineExcludedRootKeys) {
    if (key == excluded) {
      return false;
    }
  }
  for (const auto& entry : kFieldTierTable) {
    if (key != entry.name) {
      continue;
    }
    switch (entry.tier) {
      case FieldTier::kStructSoft:
      case FieldTier::kStructHard:
      case FieldTier::kDisplay:
        return true;
      case FieldTier::kView:
      case FieldTier::kSession:
        return false;
    }
  }
  // No tier row under this name: see the header for why "unknown" reads as hidden.
  return false;
}

std::vector<ConfigSummaryRow> BuildConfigSummaryRows(const GuiState& state) {
  json doc;
  try {
    doc = json::parse(SerializeGuiStateJson(state));
  } catch (const std::exception& e) {
    // SerializeGuiStateJson builds its document programmatically, so this is not expected; log
    // rather than abort, because a window that cannot list its rows must still not take the app
    // down. The page then shows its document section and an empty settings section.
    GUI_LOG_WARNING("[GUI] Summary: could not serialize the current state ({})", e.what());
    return {};
  }
  std::vector<ConfigSummaryRow> rows;
  for (auto it = doc.begin(); it != doc.end(); ++it) {
    if (!ConfigSummaryIncludesRootKey(it.key())) {
      continue;
    }
    for (auto& [key_path, value] : CollectJsonLeaves(it.key(), *it)) {
      rows.push_back({ std::move(key_path), std::move(value) });
    }
  }
  return rows;
}

// Whether the whole root key `root` is declared to have no main-panel control
// (FieldTierEntry::has_main_panel_surface). A root with no tier row reads as "has one": the walk
// only reaches shown-tier roots, and every one of those has a row.
namespace {
bool RootKeyHasMainPanelSurface(std::string_view root) {
  for (const auto& entry : kFieldTierTable) {
    if (root == entry.name) {
      return entry.has_main_panel_surface;
    }
  }
  return true;
}

// The version string the page prints, when a test has pinned one. Written only by
// SetConfigSummaryVersionForTest below, read only by BuildConfigSummary; empty in every product
// build, which is what makes the product path unconditionally the real version. No synchronization:
// gui_test writes it from its single-threaded ResetTestState() before any frame is drawn, and
// nothing else touches it — do not reuse it from a multi-threaded path.
std::optional<std::string> g_version_override_for_test;
}  // namespace

// Declared in gui/config_summary_test_hooks.hpp, which this file deliberately does not include —
// see that header. The signature must match it verbatim; nothing checks that for us.
void SetConfigSummaryVersionForTest(std::string version) {
  g_version_override_for_test = std::move(version);
}

ConfigSummary BuildConfigSummary(const GuiState& state) {
  ConfigSummary summary;
  summary.version = g_version_override_for_test.value_or(LUMICE_GetVersionString());

  // Settings: one group per root key. The known three in their listed order, anything else
  // after them in the order the walk produced it, and the popup-only fields last under their own
  // heading. Per leaf, the field editor registry decides the label, whether the row is on the
  // page at all, and which group it sits in — see the header.
  std::vector<std::pair<std::string, ConfigSummaryGroup>> groups;
  ConfigSummaryGroup popup_only{ kSettingsPopupOnlyGroupTitle, {} };
  for (auto& row : BuildConfigSummaryRows(state)) {
    const std::string root = row.key_path.substr(0, row.key_path.find('.'));
    const std::string leaf = row.key_path.size() > root.size() ? row.key_path.substr(root.size() + 1) : root;
    const FieldEditorEntry* editor = FindFieldEditor(row.key_path);
    const char* declared_label = LabelFor(row.key_path);
    ConfigSummaryField field{ declared_label != nullptr ? declared_label : Humanize(leaf), FormatDiffValue(row.value) };

    const bool popup_only_leaf = editor != nullptr && !editor->has_main_panel_surface;
    if (popup_only_leaf || !RootKeyHasMainPanelSurface(root)) {
      popup_only.fields.push_back(std::move(field));
      continue;
    }
    // The panel's BeginDisabled expression, read rather than restated: a leaf the panel greys or
    // does not draw right now is not printed. An unregistered leaf has no gate and is always on.
    if (editor != nullptr && !editor->Constraint(state).enabled) {
      continue;
    }
    auto it = groups.begin();
    while (it != groups.end() && it->first != root) {
      ++it;
    }
    if (it == groups.end()) {
      const char* title = SettingsGroupTitleFor(root);
      groups.push_back({ root, ConfigSummaryGroup{ title != nullptr ? title : Humanize(root), {} } });
      it = groups.end() - 1;
    }
    it->second.fields.push_back(std::move(field));
  }
  // Within a group, alphabetical by the label the reader sees. The walk's own order is the
  // serialized key's, which read alphabetically only while labels were spelled from keys; a
  // reader scanning for "Sky Color" or "Mode" is served by the order of the words on the page.
  const auto by_label = [](const ConfigSummaryField& a, const ConfigSummaryField& b) {
    return std::lexicographical_compare(
        a.label.begin(), a.label.end(), b.label.begin(), b.label.end(), [](char x, char y) {
          return std::tolower(static_cast<unsigned char>(x)) < std::tolower(static_cast<unsigned char>(y));
        });
  };
  for (auto& [root, group] : groups) {
    std::sort(group.fields.begin(), group.fields.end(), by_label);
  }
  std::sort(popup_only.fields.begin(), popup_only.fields.end(), by_label);
  for (const auto& known : kSettingsGroupTitles) {
    for (auto& [root, group] : groups) {
      if (root == known.root_key) {
        summary.settings.push_back(std::move(group));
        root.clear();  // consumed
      }
    }
  }
  for (auto& [root, group] : groups) {
    if (!root.empty()) {
      summary.settings.push_back(std::move(group));
    }
  }
  if (!popup_only.fields.empty()) {
    summary.settings.push_back(std::move(popup_only));
  }

  // Document: one heading and two tables per layer.
  for (int layer_idx = 0; layer_idx < static_cast<int>(state.layers.size()); ++layer_idx) {
    const Layer& layer = state.layers[static_cast<size_t>(layer_idx)];
    ConfigSummaryLayer page_layer;
    // The layer header's slider, under the panel's own label and in its "%.2f".
    page_layer.heading = "Layer " + std::to_string(DisplayLayerNumber(layer_idx)) + " · Multi-scatter prob. " +
                         Format("%.2f", layer.probability) + " · " + std::to_string(layer.entries.size()) +
                         (layer.entries.size() == 1 ? " entry" : " entries");
    page_layer.crystals = BuildCrystalsTable(state, layer_idx);
    page_layer.shape = BuildShapeTable(state, layer_idx);
    summary.document.push_back(std::move(page_layer));
  }
  return summary;
}

int CountConfigSummaryFields(const ConfigSummary& summary) {
  int count = 0;
  for (const auto& group : summary.settings) {
    count += static_cast<int>(group.fields.size());
  }
  return count;
}

std::string FormatConfigSummaryAsText(const ConfigSummary& summary) {
  auto join_cells = [](const std::vector<std::string>& cells) {
    std::string line;
    for (size_t i = 0; i < cells.size(); ++i) {
      if (i > 0) {
        line += '\t';
      }
      line += cells[i];
    }
    return line;
  };
  std::string out = "Lumice " + summary.version + "\n";
  for (const auto& group : summary.settings) {
    out += '\n';
    out += group.title + "\n";
    for (const auto& field : group.fields) {
      out += field.label + "\t" + field.value + "\n";
    }
  }
  for (const auto& layer : summary.document) {
    out += '\n';
    out += layer.heading + "\n";
    for (const ConfigSummaryTable* table : { &layer.crystals, &layer.shape }) {
      if (table->rows.empty()) {
        continue;
      }
      out += join_cells(table->columns) + "\n";
      for (const auto& row : table->rows) {
        out += join_cells(row.cells) + "\n";
      }
    }
  }
  if (!summary.document.empty()) {
    out += '\n';
    out += DistributionLegend() + "\n";
  }
  return out;
}

const char* DistributionLetter(AxisDistType type) {
  const int index = static_cast<int>(type);
  if (index < 0 || index >= static_cast<int>(AxisDistType::kCount)) {
    return "?";
  }
  return kDistributionLetters[index];
}

const char* DistributionLetterForWireName(std::string_view wire_name) {
  const std::optional<AxisDistType> type = AxisDistTypeFromJsonName(wire_name);
  return type.has_value() ? DistributionLetter(*type) : "?";
}

std::string FormatShapeDistCell(const ShapeDist& dist, int slot) {
  // A shape scalar has no full-circle case: its domain is a length, not an angle.
  std::string out =
      FormatDistributionCell(ShapeScalarDomainFor(slot).fmt, dist.type == ShapeDistType::kNoRandom,
                             ShapeDistTypeToString(dist.type), /*full_circle=*/false, dist.center, dist.spread);
  if (dist.sync_group != 0) {
    out += " · sync " + std::to_string(dist.sync_group);
  }
  return out;
}

std::string FormatAxisDistCell(const AxisDist& axis) {
  // An axis is always a distribution (AxisDist has no fixed alternative); the modal's "%.3g".
  return FormatDistributionCell("%.3g", /*no_random=*/false, AxisDistTypeJsonName(axis.type),
                                axis_preset_detail::IsFullUniform360(axis), axis.mean, axis.std);
}

std::string DistributionLegend() {
  std::string legend;
  for (int i = 0; i < static_cast<int>(AxisDistType::kCount); ++i) {
    const auto type = static_cast<AxisDistType>(i);
    if (!legend.empty()) {
      legend += " · ";
    }
    legend += std::string(DistributionLetter(type)) + " " + AxisDistTypeLabel(type);
  }
  return legend;
}

}  // namespace lumice::gui
