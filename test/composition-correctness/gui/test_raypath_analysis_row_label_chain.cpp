// The crystal number each row of the Raypath Analysis list shows: a chain's "C<core>(" prefix
// names the crystal by the scene's insertion-order id, while the crystal cards are numbered by
// their pool slot — two systems with no shared key, and a card drag-reorder or a card delete
// moves the first while the second stands still. The proposition is the label chain: the
// Analyze-time scene snapshot -> AnalysisCoreToPoolMap -> AnalysisRowLabel, pinned on the two
// edits that used to scramble it (the label must follow the crystal, not the position) and on
// the greyed excluded rows (which spell the LIVE document's numbers, the map their own display
// text was built from). The pixels are gui_test's.

#include <gtest/gtest.h>

#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "gui/analysis_panel.hpp"
#include "gui/app.hpp"
#include "gui/gui_state.hpp"
#include "gui/panels.hpp"

namespace lumice::gui {
namespace {

constexpr uint8_t kPbd = LUMICE_RAYPATH_SYMMETRY_P | LUMICE_RAYPATH_SYMMETRY_B | LUMICE_RAYPATH_SYMMETRY_D;

// `n` crystals in the pool, one entry each in one layer, pool ids 0..n-1 in entry order — the
// insertion order that makes the scene's ids equal the pool ids, so a hand-written display
// "C<i>(...)" names pool i unambiguously.
void SeedMultiCrystalDocument(int n) {
  DoNew();
  g_state.crystals.assign(n, CrystalConfig{});
  g_state.filters.clear();
  Layer layer;
  layer.probability = 1.0f;
  for (int i = 0; i < n; ++i) {
    EntryCard entry;
    entry.crystal_id = i;
    entry.proportion = 100.0f;
    layer.entries.push_back(entry);
  }
  g_state.layers.assign(1, layer);
}

struct ChainSpec {
  const char* display;  // the full core text, C-prefixed as the multi-entry layer formats it
  int scene_crystal_id;
  double energy;
};

// A whole-sky result of single-segment chains, `generation` stamped so a later one is adopted.
std::shared_ptr<AnalysisPayload> ResultOf(unsigned long long generation, std::initializer_list<ChainSpec> chains) {
  auto p = std::make_shared<AnalysisPayload>();
  p->snapshot_generation = generation;
  p->roi_mode = LUMICE_RAYPATH_ROI_FULL_SKY;
  for (const ChainSpec& c : chains) {
    LUMICE_RaypathHistogramEntry e{};
    e.chain_len = 1;
    e.chain[0].crystal_id = c.scene_crystal_id;
    e.chain[0].segment_len = 2;
    e.chain[0].segment[0] = 3;
    e.chain[0].segment[1] = 5;
    snprintf(e.display, sizeof(e.display), "%s", c.display);
    e.energy = c.energy;
    e.count = 1000;
    p->entries.push_back(e);
  }
  return p;
}

// Adopt `payload` as RefreshAnalysisEntries would leave it: the entries read under P|B|D.
void ShowResult(const std::shared_ptr<AnalysisPayload>& payload) {
  ASSERT_TRUE(AdoptAnalysisPayloadIfNew(g_state, payload));
  g_state.analysis_result.entries_symmetry = kPbd;
}

// Analyze again, the way DoAnalyze captures the scene: the snapshot retaken from the live
// document as it stands now, before the new result is put on show.
void Reanalyze(const std::shared_ptr<AnalysisPayload>& payload) {
  g_state.analysis_result.analyzed_scene = GuiState::AnalysisSceneIdentity::From(g_state);
  ShowResult(payload);
}

std::string LabelOf(std::string_view display) {
  return AnalysisRowLabel(display, AnalysisCoreToPoolMap(g_state));
}

// The map RenderResultList draws the greyed rows' labels with: the LIVE document's
// pool->core numbering, inverted — the same map ComputeExcludedRaypathRows built those rows'
// display text from.
std::map<int, int> LiveCoreToPoolMap() {
  std::map<int, int> core_to_pool;
  for (const auto& [pool, core] : ComputeCrystalPoolToCoreIdMap(g_state)) {
    core_to_pool.emplace(core, pool);
  }
  return core_to_pool;
}

// Three crystals, the result on show naming each by its (aligned) scene id. Every row's label
// spells the card's pool number: the prefix the user can find on a card, not the scene's.
TEST(RaypathAnalysisRowLabelChain, RowLabelsSpellTheCardNumberNotTheSceneNumber) {
  SeedMultiCrystalDocument(3);
  g_state.analysis_result.analyzed_scene = GuiState::AnalysisSceneIdentity::From(g_state);
  ShowResult(ResultOf(1, { { "C0(3-5)", 0, 3.0 }, { "C1(3-5)", 1, 2.0 }, { "C2(3-5)", 2, 1.0 } }));
  EXPECT_EQ(LabelOf("C0(3-5)"), "#0(3-5)");
  EXPECT_EQ(LabelOf("C1(3-5)"), "#1(3-5)");
  EXPECT_EQ(LabelOf("C2(3-5)"), "#2(3-5)");
}

// A drag-reorder (the card list's own primitive) moves the entries; the cards and the labels
// do not follow — until a re-analyze renumbers the scene by the NEW insertion order, and the
// labels re-attach to the same cards they named before. The label follows the crystal, never
// the position: the same display text names the same card across the reorder, and after the
// renumber a card's chain spells the card's number however far the card moved.
TEST(RaypathAnalysisRowLabelChain, DraggedCardsKeepTheirNumbersAcrossARenumber) {
  SeedMultiCrystalDocument(3);
  g_state.analysis_result.analyzed_scene = GuiState::AnalysisSceneIdentity::From(g_state);
  ShowResult(ResultOf(1, { { "C0(3-5)", 0, 3.0 }, { "C1(3-5)", 1, 2.0 }, { "C2(3-5)", 2, 1.0 } }));
  EXPECT_EQ(LabelOf("C0(3-5)"), "#0(3-5)");
  EXPECT_EQ(LabelOf("C1(3-5)"), "#1(3-5)");
  EXPECT_EQ(LabelOf("C2(3-5)"), "#2(3-5)");

  // Card 0 dragged below the other two: the live layer is now pools [1, 2, 0]. The result on
  // show is the one computed before the drag — its labels are its own, still.
  MoveEntryWithinLayer(g_state, 0, 0, 2);
  ASSERT_EQ(g_state.layers[0].entries.size(), 3u);
  EXPECT_EQ(g_state.layers[0].entries[0].crystal_id, 1);
  EXPECT_EQ(g_state.layers[0].entries[2].crystal_id, 0);
  EXPECT_EQ(LabelOf("C0(3-5)"), "#0(3-5)") << "the snapshot's numbering does not move with the live order";
  EXPECT_EQ(LabelOf("C1(3-5)"), "#1(3-5)");
  EXPECT_EQ(LabelOf("C2(3-5)"), "#2(3-5)");

  // Re-analyze: the scene is renumbered by the new first-reference order (pool 1 -> C0, pool
  // 2 -> C1, pool 0 -> C2), and the labels re-attach to the cards, not to the positions.
  Reanalyze(ResultOf(2, { { "C0(3-5)", 0, 3.0 }, { "C1(3-5)", 1, 2.0 }, { "C2(3-5)", 2, 1.0 } }));
  EXPECT_EQ(LabelOf("C0(3-5)"), "#1(3-5)") << "C0 now names the card that was dragged to the back";
  EXPECT_EQ(LabelOf("C1(3-5)"), "#2(3-5)");
  EXPECT_EQ(LabelOf("C2(3-5)"), "#0(3-5)") << "the dragged card is still card #0, wherever it sits";
}

// A card delete sparsifies the pool (slots are never reclaimed), so the scene after the next
// analyze numbers the survivors 0..n-1 while their cards keep #0 and #2. The labels must spell
// the card numbers across that gap — the deletion path needs no drag to scramble a naive
// position-based numbering.
TEST(RaypathAnalysisRowLabelChain, DeletedCardLeavesTheSurvivorsOnTheirOwnNumbers) {
  SeedMultiCrystalDocument(3);
  g_state.analysis_result.analyzed_scene = GuiState::AnalysisSceneIdentity::From(g_state);
  ShowResult(ResultOf(1, { { "C0(3-5)", 0, 3.0 }, { "C1(3-5)", 1, 2.0 }, { "C2(3-5)", 2, 1.0 } }));
  EXPECT_EQ(LabelOf("C2(3-5)"), "#2(3-5)");

  // Delete card #1 (the card delete button's structural step, DeleteLayerEntry — the same
  // primitive RenderLayer's delete dispatches). The pool slot is not reclaimed: cards #0 and
  // #2 remain.
  ASSERT_TRUE(DeleteLayerEntry(g_state, 0, 1));
  ASSERT_EQ(g_state.layers[0].entries.size(), 2u);
  EXPECT_EQ(g_state.layers[0].entries[0].crystal_id, 0);
  EXPECT_EQ(g_state.layers[0].entries[1].crystal_id, 2);
  Reanalyze(ResultOf(2, { { "C0(3-5)", 0, 3.0 }, { "C1(3-5)", 1, 2.0 } }));
  EXPECT_EQ(LabelOf("C0(3-5)"), "#0(3-5)");
  EXPECT_EQ(LabelOf("C1(3-5)"), "#2(3-5)") << "the survivor keeps its card number, not its scene position";
}

// The greyed excluded rows spell the LIVE document's numbers: their display text is built from
// the live pool->core map, and their label rewrites it back under the same map, so the row a
// user excluded by clicking "C0(3-5)" shows the card that carries the filter.
TEST(RaypathAnalysisRowLabelChain, ExcludedRowLabelsSpellTheLiveCardNumber) {
  SeedMultiCrystalDocument(2);
  g_state.analysis_result.analyzed_scene = GuiState::AnalysisSceneIdentity::From(g_state);
  ShowResult(ResultOf(1, { { "C0(3-5)", 0, 3.0 }, { "C1(3-5)", 1, 1.0 } }));

  // Exclude C0's chain the way the Exclude button does.
  g_state.analysis.selected_entry = "C0(3-5)";
  std::string why;
  ASSERT_EQ(EvaluateExcludeEligibility(g_state, &why), ExcludeEligibility::kOk) << why;
  ASSERT_TRUE(ApplyExcludeSelectedRaypath(g_state));
  ASSERT_EQ(g_state.filters.size(), 1u);
  EXPECT_EQ(g_state.filters[0].name, "Exclude #0(3-5)") << "named after the label the user clicked";

  // The next result no longer carries the chain: the filter took. The greyed row is the live
  // document's C0 read back, and its label spells card #0.
  Reanalyze(ResultOf(2, { { "C1(3-5)", 1, 1.0 } }));
  const std::vector<ExcludedRaypathRow> rows = ComputeExcludedRaypathRows(g_state);
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows[0].display, "C0(3-5)");
  const std::string label = ExcludedRowLabel(rows[0].display, LiveCoreToPoolMap());
  EXPECT_EQ(label.find("#0(3-5)"), 0u) << label;
  EXPECT_NE(label.find("excluded"), std::string::npos) << label;
}

}  // namespace
}  // namespace lumice::gui
