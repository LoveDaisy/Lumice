#include "gui/config_summary.hpp"

#include <cstdio>
#include <optional>
#include <utility>

#include "gui/axis_presets.hpp"
#include "gui/defaults_diff.hpp"
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

// The shape scalar as the edit modal's row shows it: the value in the slot's own format, and for
// a randomized one the spread and the distribution beside it; a sync group is named last, as the
// Sync column does.
std::string FormatShapeDist(const ShapeDist& dist, int slot) {
  const char* fmt = ShapeScalarDomainFor(slot).fmt;
  std::string out = Format(fmt, dist.center);
  if (dist.type != ShapeDistType::kNoRandom) {
    out += " ± " + Format(fmt, dist.spread) + " " + ShapeDistTypeToString(dist.type);
  }
  if (dist.sync_group != 0) {
    out += " · sync " + std::to_string(dist.sync_group);
  }
  return out;
}

// The axis as the axis modal's row shows it: distribution, mean, and the spread under the name
// that distribution gives it (Std / Range / Amplitude / Scale), in the modal's "%.3g".
std::string FormatAxisDist(const AxisDist& axis) {
  return std::string(AxisDistTypeLabel(axis.type)) + " · Mean " + Format("%.3g", axis.mean) + " · " +
         AxisDistSpreadLabel(axis.type) + " " + Format("%.3g", axis.std);
}

void AppendShapeRow(ConfigSummaryGroup& group, const CrystalConfig& cr, int slot) {
  group.fields.push_back({ kShapeScalarLabels[slot], FormatShapeDist(ShapeScalarAt(cr, slot), slot) });
}

ConfigSummaryGroup BuildEntryGroup(const GuiState& state, int layer_idx, int entry_idx) {
  const EntryCard& entry = state.layers[static_cast<size_t>(layer_idx)].entries[static_cast<size_t>(entry_idx)];
  ConfigSummaryGroup group;
  group.title = "Layer " + std::to_string(DisplayLayerNumber(layer_idx)) + " · Entry " +
                std::to_string(DisplayEntryNumber(entry_idx));
  group.level = 1;

  // The card's own four rows first, in the card's order and spelling.
  group.fields.push_back({ "Crystal", FormatCrystalIdentity(state, entry.crystal_id) });
  if (entry.crystal_id < 0 || static_cast<size_t>(entry.crystal_id) >= state.crystals.size()) {
    // FormatCrystalIdentity has already printed the dangling id; there is no crystal to describe.
    return group;
  }
  const CrystalConfig& cr = state.crystals[static_cast<size_t>(entry.crystal_id)];
  group.fields.push_back({ "Enabled", entry.enabled ? "true" : "false" });
  group.fields.push_back({ "Weight", Format("%.1f", entry.proportion) });

  // The edit modal's Crystal tab, row for row: the type's own scalars, then the six faces.
  if (cr.type == CrystalType::kPrism) {
    AppendShapeRow(group, cr, LUMICE_SHAPE_SCALAR_HEIGHT);
  } else {
    AppendShapeRow(group, cr, LUMICE_SHAPE_SCALAR_PRISM_H);
    AppendShapeRow(group, cr, LUMICE_SHAPE_SCALAR_UPPER_H);
    AppendShapeRow(group, cr, LUMICE_SHAPE_SCALAR_LOWER_H);
    group.fields.push_back({ "Upper A", Format("%.3f", cr.upper_alpha) });
    group.fields.push_back({ "Lower A", Format("%.3f", cr.lower_alpha) });
  }
  for (int i = 0; i < 6; ++i) {
    AppendShapeRow(group, cr, LUMICE_SHAPE_SCALAR_FACE_0 + i);
  }

  // The Axis tab: the preset the classifier reads off the three distributions, then the
  // distributions themselves — a preset name alone would hide a retuned zenith std, which is the
  // one parameter of a preset the user can change and the one most worth comparing.
  group.fields.push_back({ "Axis", AxisPresetName(cr) });
  group.fields.push_back({ "Zenith", FormatAxisDist(cr.zenith) });
  group.fields.push_back({ "Azimuth", FormatAxisDist(cr.azimuth) });
  group.fields.push_back({ "Roll", FormatAxisDist(cr.roll) });

  // The Filter tab, as the card summarises it. The card's line truncates a long raypath and
  // collapses a multi-row sum of products to its first row "(+N more)", which is right for a
  // 151-px column and wrong for a page whose point is to be read in full — so the rows follow,
  // one field each, in the editor's own row text.
  std::optional<FilterConfig> filter;
  if (entry.filter_id.has_value() && *entry.filter_id >= 0 &&
      static_cast<size_t>(*entry.filter_id) < state.filters.size()) {
    filter = state.filters[static_cast<size_t>(*entry.filter_id)];
  }
  group.fields.push_back({ "Filter", FilterSummary(filter) });
  if (filter.has_value()) {
    if (!filter->name.empty()) {
      group.fields.push_back({ "Filter name", filter->name });
    }
    if (!filter->IsDegenerateSingleFactor()) {
      for (size_t row = 0; row < filter->param.size(); ++row) {
        group.fields.push_back(
            { "Filter row " + std::to_string(row + 1), FormatSummandText(filter->param[row].factors) });
      }
    }
  }
  return group;
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

ConfigSummary BuildConfigSummary(const GuiState& state) {
  ConfigSummary summary;
  summary.version = LUMICE_GetVersionString();

  // Settings: one group per root key. The known three in their listed order, anything else
  // after them in the order the walk produced it.
  std::vector<std::pair<std::string, ConfigSummaryGroup>> groups;
  for (auto& row : BuildConfigSummaryRows(state)) {
    const std::string root = row.key_path.substr(0, row.key_path.find('.'));
    const std::string leaf = row.key_path.size() > root.size() ? row.key_path.substr(root.size() + 1) : root;
    auto it = groups.begin();
    while (it != groups.end() && it->first != root) {
      ++it;
    }
    if (it == groups.end()) {
      const char* title = SettingsGroupTitleFor(root);
      groups.push_back({ root, ConfigSummaryGroup{ title != nullptr ? title : Humanize(root), 0, {} } });
      it = groups.end() - 1;
    }
    it->second.fields.push_back({ Humanize(leaf), FormatDiffValue(row.value) });
  }
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

  // Document: each layer, then each of its entries under it.
  for (int layer_idx = 0; layer_idx < static_cast<int>(state.layers.size()); ++layer_idx) {
    const Layer& layer = state.layers[static_cast<size_t>(layer_idx)];
    ConfigSummaryGroup layer_group;
    layer_group.title = "Layer " + std::to_string(DisplayLayerNumber(layer_idx));
    layer_group.level = 0;
    // The layer header's slider, in its "%.2f".
    layer_group.fields.push_back({ "Multi-scatter prob.", Format("%.2f", layer.probability) });
    layer_group.fields.push_back({ "Entries", std::to_string(layer.entries.size()) });
    summary.document.push_back(std::move(layer_group));
    for (int entry_idx = 0; entry_idx < static_cast<int>(layer.entries.size()); ++entry_idx) {
      summary.document.push_back(BuildEntryGroup(state, layer_idx, entry_idx));
    }
  }
  return summary;
}

int CountConfigSummaryFields(const ConfigSummary& summary) {
  int count = 0;
  for (const auto& group : summary.settings) {
    count += static_cast<int>(group.fields.size());
  }
  for (const auto& group : summary.document) {
    count += static_cast<int>(group.fields.size());
  }
  return count;
}

}  // namespace lumice::gui
