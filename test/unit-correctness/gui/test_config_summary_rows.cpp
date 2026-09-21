// The Summary window's page (src/gui/config_summary.*), asserted without a frame: which serialized
// root keys reach the settings section, decided by the field-tier registry rather than by a
// hand-written list; the shape of the document section's two tables per layer; and the one
// distribution notation the tables' cells are spelled in.
//
// The one-time red/green probe the design constraint names — add a governed GuiState field, watch
// a row appear with no change to the summary code, remove the field again — cannot be committed:
// a field that exists only to be observed is not a field. What is committed instead is the same
// proxy test_defaults_diff.cpp's AC1 case uses: the covered set is re-derived from the SERIALIZER
// and the TIER TABLE on every run, so a root key that starts being emitted under a shown tier must
// produce rows, and one under a hidden tier must not. A "special-case this key name" branch inside
// the walk turns it red in one direction or the other.
//
// Lives in gui_unit_test because BuildConfigSummaryRows serializes through file_io.cpp
// (lumice_gui_obj); no frame is rendered.

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <vector>

#include "gui/axis_presets.hpp"
#include "gui/config_summary.hpp"
#include "gui/defaults_diff.hpp"
#include "gui/field_editor_registry.hpp"
#include "gui/file_io.hpp"
#include "gui/gui_state_tiers.hpp"
#include "gui/shape_scalar_domain.hpp"
#include "include/lumice.h"

namespace gui = lumice::gui;

namespace {

using nlohmann::json;

bool RowSetCoversRootKey(const std::vector<gui::ConfigSummaryRow>& rows, const std::string& key) {
  return std::any_of(rows.begin(), rows.end(), [&](const gui::ConfigSummaryRow& row) {
    return row.key_path == key || row.key_path.rfind(key + ".", 0) == 0;
  });
}

bool TierIsShown(gui::FieldTier tier) {
  return tier == gui::FieldTier::kStructSoft || tier == gui::FieldTier::kStructHard || tier == gui::FieldTier::kDisplay;
}

// A document with one of everything the page must not show: a background image path, an overlay
// toggle, an open tool window — plus a custom spectrum and a non-default lens, so the shown side
// carries values that differ from factory too.
gui::GuiState MakeLoadedState() {
  gui::GuiState state = gui::InitDefaultState();
  state.bg_path = "/home/someone/private/sky.jpg";
  state.bg_show = true;
  state.show_grid_line = !state.show_grid_line;
  state.log_panel_open = true;
  state.color_window_open = true;
  state.renderer.lens_type = gui::kLensTypeFisheyeEqualArea;
  state.renderer.fov = 120.0f;
  state.sun.spectrum_index = gui::kCustomSpectrumIndex;
  state.sun.custom_spectrum = { { 450.0f, 1.0f }, { 550.0f, 0.5f } };
  return state;
}

}  // namespace

// Every root key the serializer emits is in exactly one of three buckets, decided by name and
// tier alone: excluded by kDiffEngineExcludedRootKeys, shown (a kStruct*/kDisplay row exists
// under that name), or hidden (a kView/kSession row, or no row at all). The walk must agree with
// that partition on every key — this is the generativity claim as a permanent assertion.
TEST(ConfigSummaryRows, EverySerializedRootKeyFollowsItsTier) {
  const gui::GuiState state = MakeLoadedState();
  const auto rows = gui::BuildConfigSummaryRows(state);
  ASSERT_FALSE(rows.empty());

  const json root = json::parse(gui::SerializeGuiStateJson(state));
  ASSERT_TRUE(root.is_object());

  std::set<std::string> excluded;
  for (const char* key : gui::kDiffEngineExcludedRootKeys) {
    excluded.insert(key);
  }

  int shown_keys = 0;
  for (auto it = root.begin(); it != root.end(); ++it) {
    const std::string& key = it.key();
    const bool covered = RowSetCoversRootKey(rows, key);

    bool expect_shown = false;
    if (excluded.count(key) == 0) {
      for (const auto& entry : gui::kFieldTierTable) {
        if (key == entry.name) {
          expect_shown = TierIsShown(entry.tier);
          break;
        }
      }
    }
    EXPECT_EQ(covered, expect_shown) << "root key \"" << key << "\"";
    EXPECT_EQ(gui::ConfigSummaryIncludesRootKey(key), expect_shown) << "root key \"" << key << "\"";
    shown_keys += expect_shown ? 1 : 0;
  }
  // The partition is not vacuous: the document's scene semantics (sun / sim / renderer) are the
  // shown side, and they are serialized under their own names.
  EXPECT_GE(shown_keys, 3);
  EXPECT_TRUE(RowSetCoversRootKey(rows, "sun"));
  EXPECT_TRUE(RowSetCoversRootKey(rows, "sim"));
  EXPECT_TRUE(RowSetCoversRootKey(rows, "renderer"));
}

// The converse, spelled from the tier table's side: every shown-tier field that the serializer
// writes under its own name produces at least one row. A shown-tier field that is NOT serialized
// (crystals / filters ride inside layers; raypath_color, use_gpu_backend and worker_count are not
// document keys) has nothing for the walk to see and is asserted absent below instead.
TEST(ConfigSummaryRows, EveryShownTierFieldThatIsSerializedProducesRows) {
  const gui::GuiState state = MakeLoadedState();
  const auto rows = gui::BuildConfigSummaryRows(state);
  const json root = json::parse(gui::SerializeGuiStateJson(state));

  std::set<std::string> excluded;
  for (const char* key : gui::kDiffEngineExcludedRootKeys) {
    excluded.insert(key);
  }
  for (const auto& entry : gui::kFieldTierTable) {
    if (!TierIsShown(entry.tier) || excluded.count(entry.name) != 0) {
      continue;
    }
    const bool serialized = root.contains(entry.name);
    EXPECT_EQ(RowSetCoversRootKey(rows, entry.name), serialized) << "field \"" << entry.name << "\"";
  }
}

// The document-structure namespace never reaches the settings walk: `layers` by the shared
// exclusion table, `crystals` / `filters` because the serializer inlines them under layers and
// emits no root key of their own. The second half is the guard: should either ever become a root
// key, this goes red and the page would show the same crystal twice (as structured entry rows and
// as raw "crystals[0].shape…" leaves) until it is decided which section owns it.
TEST(ConfigSummaryRows, DocumentStructureIsNotInTheSettingsWalk) {
  const gui::GuiState state = MakeLoadedState();
  const auto rows = gui::BuildConfigSummaryRows(state);
  for (const char* key : { "layers", "crystals", "filters" }) {
    EXPECT_FALSE(RowSetCoversRootKey(rows, key)) << key;
  }
  const json root = json::parse(gui::SerializeGuiStateJson(state));
  EXPECT_FALSE(root.contains("crystals"));
  EXPECT_FALSE(root.contains("filters"));
}

// The view/session side is hidden by name, including the keys that would leak the most: the
// machine-local file path of a background image, and which windows happen to be open.
TEST(ConfigSummaryRows, ViewAndSessionKeysAreHidden) {
  const gui::GuiState state = MakeLoadedState();
  const auto rows = gui::BuildConfigSummaryRows(state);
  for (const auto& row : rows) {
    EXPECT_NE(row.value.dump().find("/home/someone/private"), 0u);
    EXPECT_EQ(row.value.dump().find("sky.jpg"), std::string::npos) << row.key_path;
  }
  for (const char* key : { "bg_path", "bg_show", "overlay_grid_line", "log_panel_open", "color_window_open",
                           "aspect_ratio", "right_panel_collapsed" }) {
    EXPECT_FALSE(RowSetCoversRootKey(rows, key)) << key;
    EXPECT_FALSE(gui::ConfigSummaryIncludesRootKey(key)) << key;
  }
}

// Leaves are the diff engine's leaves: a colour triple is one row, the custom spectrum is one
// row, and a nested object is walked to its scalars — the same rule, because it is the same walk.
TEST(ConfigSummaryRows, LeafRuleIsTheDiffEngines) {
  const gui::GuiState state = MakeLoadedState();
  const auto rows = gui::BuildConfigSummaryRows(state);
  const auto find = [&](const char* key_path) -> const gui::ConfigSummaryRow* {
    const auto it = std::find_if(rows.begin(), rows.end(),
                                 [&](const gui::ConfigSummaryRow& row) { return row.key_path == key_path; });
    return it == rows.end() ? nullptr : &(*it);
  };
  ASSERT_NE(find("renderer.background"), nullptr);
  EXPECT_TRUE(find("renderer.background")->value.is_array());
  EXPECT_EQ(find("renderer.background.0"), nullptr);
  ASSERT_NE(find("sun.custom_spectrum"), nullptr);
  EXPECT_TRUE(find("sun.custom_spectrum")->value.is_array());
  ASSERT_NE(find("renderer.lens_type"), nullptr);
  EXPECT_EQ(find("renderer.lens_type")->value.get<std::string>(), "fisheye_equal_area");
  ASSERT_NE(find("renderer.fov"), nullptr);
  EXPECT_FLOAT_EQ(find("renderer.fov")->value.get<float>(), 120.0f);

  // And it agrees with the diff engine row for row on the shown root keys: same key paths, same
  // current values.
  const auto diff_rows = gui::BuildDefaultDiffRows(state, json::object());
  for (const auto& row : rows) {
    const auto it = std::find_if(diff_rows.begin(), diff_rows.end(),
                                 [&](const gui::DefaultDiffRow& d) { return d.key_path == row.key_path; });
    if (it == diff_rows.end()) {
      ADD_FAILURE() << row.key_path << " is not a diff-engine row";
      continue;
    }
    EXPECT_EQ(it->current_value, row.value) << row.key_path;
  }
}

namespace {

const gui::ConfigSummaryGroup* FindGroup(const gui::ConfigSummary& page, const std::string& title) {
  for (const auto& g : page.settings) {
    if (g.title == title) {
      return &g;
    }
  }
  return nullptr;
}

// The cell under column `column` of row `row_idx`, with a failure recorded when the table has no
// such column — distinct from a column that exists and holds "" (a legitimately empty cell), so a
// misspelled column name in a test cannot pass as "not applicable".
std::string Cell(const gui::ConfigSummaryTable& table, size_t row_idx, const std::string& column) {
  const auto it = std::find(table.columns.begin(), table.columns.end(), column);
  if (it == table.columns.end()) {
    ADD_FAILURE() << "no column \"" << column << "\"";
    return "<no column>";
  }
  if (row_idx >= table.rows.size()) {
    ADD_FAILURE() << "no row " << row_idx;
    return "<no row>";
  }
  return table.rows[row_idx].cells[static_cast<size_t>(it - table.columns.begin())];
}

// "ray_allocation" against "Ray allocation": the page's spelling of an undeclared label, compared
// without restating its capitalisation rule.
bool SameWords(std::string a, std::string b) {
  for (std::string* t : { &a, &b }) {
    for (auto& c : *t) {
      c = c == '_' ? ' ' : static_cast<char>(std::tolower(c));
    }
  }
  return a == b;
}

bool GroupHasLeaf(const gui::ConfigSummaryGroup* g, const std::string& leaf) {
  if (g == nullptr) {
    return false;
  }
  return std::any_of(g->fields.begin(), g->fields.end(),
                     [&](const gui::ConfigSummaryField& f) { return SameWords(f.label, leaf); });
}

// The two-layer document the visual reference "two_layers" is shot from
// (test/gui/visual/test_gui_config_summary.cpp), rebuilt here so the table-shape claims below are
// made about the same page the owner sees.
gui::GuiState MakeTwoLayerState() {
  gui::GuiState state = gui::InitDefaultState();
  state.crystals.assign(2, gui::CrystalConfig{});
  state.crystals[0].name = "plate";
  state.crystals[0].zenith = gui::AxisDist{ gui::AxisDistType::kGauss, 0.0f, 1.0f };
  state.crystals[1].type = gui::CrystalType::kPyramid;
  state.crystals[1].face_distance[2] = gui::ShapeDist{ gui::ShapeDistType::kUniform, 0.9f, 0.1f };
  gui::FilterConfig f;
  f.name = "cza";
  f.SetRaypath(gui::RaypathParams{ "3-5-1" });
  state.filters.assign(1, f);
  gui::Layer first;
  first.probability = 0.5f;
  gui::EntryCard a;
  a.crystal_id = 0;
  a.filter_id = 0;
  a.proportion = 60.0f;
  gui::EntryCard b;
  b.crystal_id = 1;
  b.proportion = 40.0f;
  first.entries = { a, b };
  gui::Layer second;
  gui::EntryCard c;
  c.crystal_id = 0;
  c.enabled = false;
  second.entries = { c };
  state.layers = { first, second };
  state.renderer.lens_type = gui::kLensTypeFisheyeEqualArea;
  state.renderer.fov = 120.0f;
  state.sun.altitude = 25.0f;
  return state;
}

}  // namespace

// A leaf with no main-panel control (FieldEditorEntry::has_main_panel_surface == false) is under
// the page's own "Settings" heading and under none of the panel groups — derived from the
// registry for every such leaf in the walk, not from a list of names here. sim.ray_allocation is
// asserted by name as well so the generic claim is known not to be vacuous.
TEST(ConfigSummaryRows, PopupOnlyLeavesAreUnderTheSettingsHeading) {
  const gui::GuiState state = MakeLoadedState();
  const gui::ConfigSummary page = gui::BuildConfigSummary(state);
  const gui::ConfigSummaryGroup* settings = FindGroup(page, gui::kSettingsPopupOnlyGroupTitle);

  int popup_only_leaves = 0;
  for (const auto& row : gui::BuildConfigSummaryRows(state)) {
    const gui::FieldEditorEntry* editor = gui::FindFieldEditor(row.key_path);
    if (editor == nullptr || editor->has_main_panel_surface) {
      continue;
    }
    ++popup_only_leaves;
    const std::string leaf = row.key_path.substr(row.key_path.find('.') + 1);
    EXPECT_TRUE(GroupHasLeaf(settings, leaf)) << row.key_path;
    for (const char* title : { "Sun", "Simulation", "Render" }) {
      EXPECT_FALSE(GroupHasLeaf(FindGroup(page, title), leaf)) << row.key_path << " under " << title;
    }
  }
  EXPECT_GE(popup_only_leaves, 1);
  ASSERT_NE(gui::FindFieldEditor("sim.ray_allocation"), nullptr);
  EXPECT_FALSE(gui::FindFieldEditor("sim.ray_allocation")->has_main_panel_surface);
  EXPECT_TRUE(GroupHasLeaf(settings, "ray_allocation"));
  // Last among the settings groups: after Sun / Simulation / Render.
  ASSERT_FALSE(page.settings.empty());
  EXPECT_EQ(page.settings.back().title, gui::kSettingsPopupOnlyGroupTitle);
}

// The root-key grain of the same flag (FieldTierEntry::has_main_panel_surface): a serialized
// root key flagged false is filed under the "Settings" heading whole. Generic over the tier table,
// and honest about today's population — the one flagged root (worker_count) is not a document
// key, so the loop finds nothing to route and says so, rather than asserting a row that is not
// there. What is pinned by name is that no panel-titled group carries such a root.
TEST(ConfigSummaryRows, PopupOnlyRootKeysAreUnderTheSettingsHeading) {
  const gui::GuiState state = MakeLoadedState();
  const gui::ConfigSummary page = gui::BuildConfigSummary(state);
  const auto rows = gui::BuildConfigSummaryRows(state);
  int flagged = 0;
  for (const auto& entry : gui::kFieldTierTable) {
    if (entry.has_main_panel_surface) {
      continue;
    }
    ++flagged;
    if (!RowSetCoversRootKey(rows, entry.name)) {
      continue;  // not serialized: nothing reaches the page under this root
    }
    for (const auto& g : page.settings) {
      if (g.title == gui::kSettingsPopupOnlyGroupTitle) {
        continue;
      }
      for (const auto& f : g.fields) {
        EXPECT_FALSE(SameWords(f.label, entry.name)) << entry.name << " under " << g.title;
      }
    }
  }
  EXPECT_GE(flagged, 1);
  EXPECT_FALSE(RowSetCoversRootKey(rows, "worker_count"));
}

// The document section is two tables per layer, paired row for row: row i of both tables is the
// layer's i-th entry, each row carries the entry's number in its "#" column, and every row has
// exactly one cell per column (an ImGui table given fewer cells shifts the rest over silently).
// On the two-layer reference document that is 2 + 1 rows over two layers.
TEST(ConfigSummaryRows, DocumentTablesPairRowsByEntryNumber) {
  const gui::GuiState state = MakeTwoLayerState();
  const gui::ConfigSummary page = gui::BuildConfigSummary(state);
  ASSERT_EQ(page.document.size(), state.layers.size());
  for (size_t l = 0; l < page.document.size(); ++l) {
    const gui::ConfigSummaryLayer& layer = page.document[l];
    const size_t entries = state.layers[l].entries.size();
    for (const gui::ConfigSummaryTable* table : { &layer.crystals, &layer.shape }) {
      if (table->rows.size() != entries) {
        ADD_FAILURE() << "layer " << l << ": " << table->rows.size() << " rows for " << entries << " entries";
        continue;
      }
      for (size_t r = 0; r < table->rows.size(); ++r) {
        EXPECT_EQ(table->rows[r].cells.size(), table->columns.size()) << "layer " << l << " row " << r;
        EXPECT_EQ(Cell(*table, r, "#"), std::to_string(r + 1)) << "layer " << l << " row " << r;
      }
    }
  }
  EXPECT_EQ(page.document[0].crystals.rows.size(), 2u);
  EXPECT_EQ(page.document[1].crystals.rows.size(), 1u);
  // The heading names the layer, its multi-scatter probability and its entry count.
  EXPECT_NE(page.document[0].heading.find("Layer 1"), std::string::npos);
  EXPECT_NE(page.document[0].heading.find("0.50"), std::string::npos);
  EXPECT_NE(page.document[0].heading.find("2 entries"), std::string::npos);
  EXPECT_NE(page.document[1].heading.find("1 entry"), std::string::npos);
}

// The Shape table's columns are the union over both crystal types, and a column that does not
// apply to a row is EMPTY — not "-", not "0": on the reference document the plate (a prism) fills
// Height and none of the pyramid columns, and the pyramid the reverse. The shape columns are
// labelled by the edit modal's own table (kShapeScalarLabels), which is the one place the words
// are spelled.
TEST(ConfigSummaryRows, ShapeTableLeavesInapplicableCellsEmpty) {
  const gui::GuiState state = MakeTwoLayerState();
  const gui::ConfigSummary page = gui::BuildConfigSummary(state);
  const gui::ConfigSummaryTable& shape = page.document[0].shape;
  ASSERT_EQ(shape.columns.size(), 13u);
  EXPECT_EQ(shape.columns[0], "#");
  for (const int slot : { LUMICE_SHAPE_SCALAR_HEIGHT, LUMICE_SHAPE_SCALAR_PRISM_H, LUMICE_SHAPE_SCALAR_UPPER_H,
                          LUMICE_SHAPE_SCALAR_LOWER_H }) {
    EXPECT_NE(std::find(shape.columns.begin(), shape.columns.end(), gui::kShapeScalarLabels[slot]), shape.columns.end())
        << gui::kShapeScalarLabels[slot];
  }
  // The six face columns are the last six, in face order.
  for (int i = 0; i < 6; ++i) {
    EXPECT_EQ(shape.columns[static_cast<size_t>(7 + i)], gui::kShapeScalarLabels[LUMICE_SHAPE_SCALAR_FACE_0 + i]);
  }
  // Row 0: the prism.
  EXPECT_EQ(Cell(shape, 0, gui::kShapeScalarLabels[LUMICE_SHAPE_SCALAR_HEIGHT]), "1");
  EXPECT_EQ(Cell(shape, 0, gui::kShapeScalarLabels[LUMICE_SHAPE_SCALAR_PRISM_H]), "");
  EXPECT_EQ(Cell(shape, 0, "Upper A"), "");
  // Row 1: the pyramid.
  EXPECT_EQ(Cell(shape, 1, gui::kShapeScalarLabels[LUMICE_SHAPE_SCALAR_HEIGHT]), "");
  EXPECT_NE(Cell(shape, 1, gui::kShapeScalarLabels[LUMICE_SHAPE_SCALAR_PRISM_H]), "");
  EXPECT_NE(Cell(shape, 1, "Upper A"), "");
  // The randomized face of the pyramid, in the notation; the fixed faces, bare numbers.
  EXPECT_EQ(Cell(shape, 1, "Face 5"), "U 0.900(0.100)");
  EXPECT_EQ(Cell(shape, 1, "Face 3"), "1.000");
  // The Crystals table carries the words: identity, the axis distributions, the filter — and no
  // shape column.
  const gui::ConfigSummaryTable& crystals = page.document[0].crystals;
  ASSERT_EQ(crystals.columns.size(), 8u);
  for (const auto& column : crystals.columns) {
    for (const char* label : gui::kShapeScalarLabels) {
      EXPECT_NE(column, label);
    }
  }
  // The preset name leads the zenith cell; the other two axis cells are the distributions alone.
  EXPECT_EQ(Cell(crystals, 0, "Zenith"), "Plate · G 0(1)");
  EXPECT_EQ(Cell(crystals, 0, "Azimuth"), "U");
  EXPECT_EQ(Cell(crystals, 0, "Roll"), "U");
  // The filter cell: the card's summary, then the filter's name.
  EXPECT_EQ(Cell(crystals, 0, "Filter"), "3-5-1 In PBD · cza");
  EXPECT_EQ(Cell(crystals, 1, "Filter"), "None");
}

// ---- The distribution notation ---------------------------------------------------------------
//
// One formatter for an axis distribution and a randomizable shape scalar, spelled
// `<letter> <centre>(<spread>)` with the panel's own two numbers — asserted here on each of the
// five distribution types, the two folds (a fixed scalar is its number; a full-circle uniform
// axis is the bare letter) and the degenerate wire name.
//
// The red state this pins: a spread halved into a ± ("0.900 ± 0.050") or a uniform's range
// written as bounds. The numbers in the parentheses are the ones on the Range / Std box, and the
// composition-correctness chain compares them against the export's, so a converted spelling
// fails both here and there.

TEST(ConfigSummaryRows, EveryDistributionTypeHasALetter) {
  EXPECT_STREQ(gui::DistributionLetter(gui::AxisDistType::kGauss), "G");
  EXPECT_STREQ(gui::DistributionLetter(gui::AxisDistType::kUniform), "U");
  EXPECT_STREQ(gui::DistributionLetter(gui::AxisDistType::kZigzag), "Z");
  EXPECT_STREQ(gui::DistributionLetter(gui::AxisDistType::kLaplacian), "L");
  EXPECT_STREQ(gui::DistributionLetter(gui::AxisDistType::kGaussLegacy), "G*");
  // The wire spellings both serializers produce map to the same letters ...
  for (int i = 0; i < static_cast<int>(gui::AxisDistType::kCount); ++i) {
    const auto type = static_cast<gui::AxisDistType>(i);
    EXPECT_STREQ(gui::DistributionLetterForWireName(gui::AxisDistTypeJsonName(type)), gui::DistributionLetter(type));
  }
  for (const auto shape : { gui::ShapeDistType::kUniform, gui::ShapeDistType::kGauss, gui::ShapeDistType::kZigzag,
                            gui::ShapeDistType::kLaplacian, gui::ShapeDistType::kGaussLegacy }) {
    EXPECT_STRNE(gui::DistributionLetterForWireName(gui::ShapeDistTypeToString(shape)), "?")
        << gui::ShapeDistTypeToString(shape);
  }
  // ... and an unknown spelling is a visible "?", not a crash and not a sixth letter.
  EXPECT_STREQ(gui::DistributionLetterForWireName("cauchy"), "?");
  EXPECT_STREQ(gui::DistributionLetterForWireName(""), "?");
  EXPECT_STREQ(gui::DistributionLetterForWireName("no_random"), "?");
}

TEST(ConfigSummaryRows, AxisCellIsLetterMeanAndSpreadUnconverted) {
  EXPECT_EQ(gui::FormatAxisDistCell(gui::AxisDist{ gui::AxisDistType::kGauss, 0.0f, 1.2f }), "G 0(1.2)");
  EXPECT_EQ(gui::FormatAxisDistCell(gui::AxisDist{ gui::AxisDistType::kUniform, 90.0f, 10.0f }), "U 90(10)");
  EXPECT_EQ(gui::FormatAxisDistCell(gui::AxisDist{ gui::AxisDistType::kZigzag, 0.0f, 5.0f }), "Z 0(5)");
  EXPECT_EQ(gui::FormatAxisDistCell(gui::AxisDist{ gui::AxisDistType::kLaplacian, 90.0f, 3.0f }), "L 90(3)");
  EXPECT_EQ(gui::FormatAxisDistCell(gui::AxisDist{ gui::AxisDistType::kGaussLegacy, 0.0f, 2.5f }), "G* 0(2.5)");
  // The full-circle uniform (the Random preset's azimuth and roll) folds to the bare letter — and
  // only that one: a uniform of any other mean or range is spelled in full. "Full circle" is the
  // preset classifier's own predicate (axis_presets.hpp IsFullUniform360, within its kEpsilon of
  // 1°), so the page folds exactly what the panel calls Random, no more and no less.
  EXPECT_EQ(gui::FormatAxisDistCell(gui::kAzFullUniform), "U");
  EXPECT_EQ(gui::FormatAxisDistCell(gui::kRollFreeUniform), "U");
  EXPECT_EQ(gui::FormatAxisDistCell(gui::AxisDist{ gui::AxisDistType::kUniform, 0.0f, 350.0f }), "U 0(350)");
  EXPECT_EQ(gui::FormatAxisDistCell(gui::AxisDist{ gui::AxisDistType::kUniform, 5.0f, 360.0f }), "U 5(360)");
  EXPECT_EQ(gui::FormatAxisDistCell(gui::AxisDist{ gui::AxisDistType::kGauss, 0.0f, 360.0f }), "G 0(360)");
  // No ±, no halving: the spread is the number on the panel.
  const std::string uniform = gui::FormatAxisDistCell(gui::AxisDist{ gui::AxisDistType::kUniform, 0.0f, 20.0f });
  EXPECT_EQ(uniform, "U 0(20)");
  EXPECT_EQ(uniform.find("±"), std::string::npos);
  EXPECT_EQ(uniform.find("10"), std::string::npos);
}

TEST(ConfigSummaryRows, ShapeCellIsTheSlotsNumberOrTheNotation) {
  // Fixed: the slot's own format and nothing else, for a length slot and a face slot alike.
  EXPECT_EQ(gui::FormatShapeDistCell(gui::ShapeDist{ 1.0f }, LUMICE_SHAPE_SCALAR_HEIGHT), "1");
  EXPECT_EQ(gui::FormatShapeDistCell(gui::ShapeDist{ 1.0f }, LUMICE_SHAPE_SCALAR_FACE_2), "1.000");
  // Randomized: the same notation as an axis, in the slot's format; the uniform's spread is the
  // full width — [0.85, 0.95] reads "0.900(0.100)", never "0.900 ± 0.050".
  const std::string uniform =
      gui::FormatShapeDistCell(gui::ShapeDist{ gui::ShapeDistType::kUniform, 0.9f, 0.1f }, LUMICE_SHAPE_SCALAR_FACE_2);
  EXPECT_EQ(uniform, "U 0.900(0.100)");
  EXPECT_EQ(uniform.find("±"), std::string::npos);
  EXPECT_EQ(uniform.find("0.050"), std::string::npos);
  // A length slot is "%.4g" (shape_scalar_domain.hpp), a wedge fraction or a face "%.3f": the
  // slot's own format, whatever it is.
  EXPECT_EQ(
      gui::FormatShapeDistCell(gui::ShapeDist{ gui::ShapeDistType::kGauss, 0.3f, 0.05f }, LUMICE_SHAPE_SCALAR_HEIGHT),
      "G 0.3(0.05)");
  EXPECT_EQ(
      gui::FormatShapeDistCell(gui::ShapeDist{ gui::ShapeDistType::kZigzag, 1.7f, 0.2f }, LUMICE_SHAPE_SCALAR_PRISM_H),
      "Z 1.7(0.2)");
  EXPECT_EQ(gui::FormatShapeDistCell(gui::ShapeDist{ gui::ShapeDistType::kLaplacian, 0.35f, 0.01f },
                                     LUMICE_SHAPE_SCALAR_UPPER_H),
            "L 0.350(0.010)");
  EXPECT_EQ(gui::FormatShapeDistCell(gui::ShapeDist{ gui::ShapeDistType::kGaussLegacy, 0.15f, 0.02f },
                                     LUMICE_SHAPE_SCALAR_LOWER_H),
            "G* 0.150(0.020)");
  // A shape scalar never folds: a uniform at 0(360) is a length, not a circle.
  EXPECT_EQ(gui::FormatShapeDistCell(gui::ShapeDist{ gui::ShapeDistType::kUniform, 0.0f, 360.0f },
                                     LUMICE_SHAPE_SCALAR_HEIGHT),
            "U 0(360)");
  // A sync group is named after the value, fixed or randomized.
  gui::ShapeDist synced{ 1.0f };
  synced.sync_group = 2;
  EXPECT_EQ(gui::FormatShapeDistCell(synced, LUMICE_SHAPE_SCALAR_FACE_0), "1.000 · sync 2");
  gui::ShapeDist synced_random{ gui::ShapeDistType::kGauss, 0.9f, 0.1f };
  synced_random.sync_group = 1;
  EXPECT_EQ(gui::FormatShapeDistCell(synced_random, LUMICE_SHAPE_SCALAR_FACE_0), "G 0.900(0.100) · sync 1");
}

// The legend is built from the same letter table as the cells and the combo's own labels, and
// names every letter exactly once.
TEST(ConfigSummaryRows, LegendSpellsEveryLetterOnce) {
  const std::string legend = gui::DistributionLegend();
  EXPECT_EQ(legend, "G Gauss · U Uniform · Z Zigzag · L Laplacian · G* Gauss (legacy)");
  for (int i = 0; i < static_cast<int>(gui::AxisDistType::kCount); ++i) {
    const auto type = static_cast<gui::AxisDistType>(i);
    const std::string item = std::string(gui::DistributionLetter(type)) + " " + gui::AxisDistTypeLabel(type);
    EXPECT_NE(legend.find(item), std::string::npos) << item;
  }
}
