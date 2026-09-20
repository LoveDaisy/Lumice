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
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <vector>

#include "gui/config_summary.hpp"
#include "gui/defaults_diff.hpp"
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
