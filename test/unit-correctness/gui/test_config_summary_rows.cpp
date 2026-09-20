// The Summary window's settings section (src/gui/config_summary.*): which serialized root keys
// reach the page, decided by the field-tier registry rather than by a hand-written list.
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

#include "gui/config_summary.hpp"
#include "gui/defaults_diff.hpp"
#include "gui/field_editor_registry.hpp"
#include "gui/file_io.hpp"
#include "gui/gui_state_tiers.hpp"

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
  for (const auto& g : page.document) {
    if (g.title == title) {
      return &g;
    }
  }
  return nullptr;
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
// (test/gui/visual/test_gui_config_summary.cpp), rebuilt here so the line-count claim below is
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

// Which fields share a display line is the page's own cut (ConfigSummaryLines), asserted on a
// synthetic group so the rule is pinned apart from any document: -1 is a line of its own; a run
// sharing an id is gathered at its first member's position, wherever the rest sit, and cut into
// lines of kPackedFieldsPerRow; a run of one is one line.
TEST(ConfigSummaryRows, LinesGatherRowGroupsAndCutThemAtThePackingWidth) {
  gui::ConfigSummaryGroup group;
  group.fields = {
    { "a", "1", -1 }, { "b", "2", 7 }, { "c", "3", -1 }, { "d", "4", 7 },
    { "e", "5", 7 },  { "f", "6", 7 }, { "g", "7", 9 },  { "h", "8", 7 },
  };
  const auto lines = gui::ConfigSummaryLines(group);
  ASSERT_EQ(lines.size(), 5u);
  EXPECT_EQ(lines[0].size(), 1u);
  EXPECT_EQ(lines[0][0]->label, "a");
  // The run 7 (b d e f h) at b's position: 3 + 2.
  ASSERT_EQ(lines[1].size(), static_cast<size_t>(gui::kPackedFieldsPerRow));
  EXPECT_EQ(lines[1][0]->label, "b");
  EXPECT_EQ(lines[1][1]->label, "d");
  EXPECT_EQ(lines[1][2]->label, "e");
  ASSERT_EQ(lines[2].size(), 2u);
  EXPECT_EQ(lines[2][0]->label, "f");
  EXPECT_EQ(lines[2][1]->label, "h");
  EXPECT_EQ(lines[3][0]->label, "c");
  ASSERT_EQ(lines[4].size(), 1u);
  EXPECT_EQ(lines[4][0]->label, "g");
  EXPECT_EQ(gui::CountConfigSummaryLines(group), 5);
}

// The "same-kind scalars share a line" claim, measured: on the two-layer reference document every
// entry occupies at most half the lines it did when every field was a line of its own — and the
// number of FIELDS is that earlier count, since the packing moved nothing and dropped nothing.
TEST(ConfigSummaryRows, EveryEntryOccupiesAtMostHalfTheLinesOfOnePerField) {
  const gui::GuiState state = MakeTwoLayerState();
  const gui::ConfigSummary page = gui::BuildConfigSummary(state);
  int entries = 0;
  for (const auto& g : page.document) {
    if (g.level != 1) {
      continue;
    }
    ++entries;
    const int one_per_field = static_cast<int>(g.fields.size());
    const int lines = gui::CountConfigSummaryLines(g);
    EXPECT_LE(lines * 2, one_per_field) << g.title << ": " << lines << " lines for " << one_per_field << " fields";
  }
  EXPECT_EQ(entries, 3);
  // And the page as a whole shrank by at least a third: the settings groups pack nothing, so the
  // saving is the document's alone.
  EXPECT_LE(gui::CountConfigSummaryLines(page) * 3, gui::CountConfigSummaryFields(page) * 2);
}
