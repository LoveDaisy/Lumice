// The greyed "excluded" rows of the Raypath Analysis list, across the units they are derived
// from: an analysis result in GuiState -> "Exclude this raypath" (analysis_panel.cpp) writes the
// document's filter pool through the edit modal's write primitive (edit_modals.cpp) and remembers
// the row's share -> a LATER result without that chain -> ComputeExcludedRaypathRows reads the
// filters back as rows -> "Include again" takes one out again through the same primitives, and
// the frame-tail reconciler (gui_state_reconcile.cpp) reads that as a document edit. The
// proposition is the derived set — which rows, in which order, with which memory — and what
// the pool looks like after Include again; the pixels are gui_test's.

#include <gtest/gtest.h>

#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gui/analysis_panel.hpp"
#include "gui/analysis_result.hpp"
#include "gui/app.hpp"
#include "gui/gui_state.hpp"
#include "gui/gui_state_reconcile.hpp"
#include "gui/raypath_segments.hpp"
#include "lumice.h"

namespace lumice::gui {
namespace {

constexpr uint8_t kPbd = LUMICE_RAYPATH_SYMMETRY_P | LUMICE_RAYPATH_SYMMETRY_B | LUMICE_RAYPATH_SYMMETRY_D;

// One prism, no filter, one entry in one layer — the shape the 22-degree halo's chain comes from.
void SeedUnfilteredPrismDocument() {
  DoNew();
  g_state.crystals.assign(1, CrystalConfig{});
  g_state.crystals[0].type = CrystalType::kPrism;
  g_state.crystals[0].height = 1.0f;
  for (int i = 0; i < 6; ++i) {
    g_state.crystals[0].face_distance[i] = 1.0f;
  }
  g_state.filters.clear();
  Layer layer;
  layer.probability = 1.0f;
  EntryCard entry;
  entry.crystal_id = 0;
  entry.proportion = 100.0f;
  layer.entries.assign(1, entry);
  g_state.layers.assign(1, layer);
}

// A second prism in the pool, as a second entry of the root layer: the layer then holds two
// entries, and core prefixes every single-segment chain of it with the crystal's scene id.
void AddSecondPrismToRootLayer() {
  g_state.crystals.push_back(g_state.crystals[0]);
  EntryCard entry;
  entry.crystal_id = 1;
  entry.proportion = 100.0f;
  g_state.layers[0].entries.push_back(entry);
}

struct ChainSpec {
  const char* display;
  int scene_crystal_id;
  int faces[2];
  double energy;
};

// A whole-sky result of single-segment chains, `generation` stamped so a later one is adopted.
std::shared_ptr<AnalysisPayload> ResultOf(unsigned long long generation, const std::vector<ChainSpec>& chains) {
  auto p = std::make_shared<AnalysisPayload>();
  p->snapshot_generation = generation;
  p->roi_mode = LUMICE_RAYPATH_ROI_FULL_SKY;
  for (const ChainSpec& c : chains) {
    LUMICE_RaypathHistogramEntry e{};
    e.chain_len = 1;
    e.chain[0].crystal_id = c.scene_crystal_id;
    e.chain[0].segment_len = 2;
    e.chain[0].segment[0] = c.faces[0];
    e.chain[0].segment[1] = c.faces[1];
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

void ExcludeRow(const char* display) {
  g_state.analysis.selected_entry = display;
  std::string why;
  ASSERT_EQ(EvaluateExcludeEligibility(g_state, &why), ExcludeEligibility::kOk) << why;
  ASSERT_TRUE(ApplyExcludeSelectedRaypath(g_state));
}

const ExcludedRaypathRow* RowFor(const std::vector<ExcludedRaypathRow>& rows, const std::string& display) {
  for (const ExcludedRaypathRow& r : rows) {
    if (r.display == display) {
      return &r;
    }
  }
  return nullptr;
}

// Exclude 3-5 of a two-row list (75% of the total), then a new result arrives without it: the
// list derives one greyed row for it, carrying the share it had, keyed to the filter row Exclude
// wrote so Include again knows where to erase. The share survives only under the symmetry it
// was read with; a second Exclude of the same chain — the idempotent no-op — does not renumber
// the memory.
TEST(RaypathAnalysisExcludedRowsChain, ExcludeThenNewResultWithoutTheRow_ProducesOneExcludedRow) {
  SeedUnfilteredPrismDocument();
  ShowResult(ResultOf(1, { { "3-5", 0, { 3, 5 }, 3.0 }, { "1-3", 0, { 1, 3 }, 1.0 } }));
  EXPECT_TRUE(ComputeExcludedRaypathRows(g_state).empty()) << "nothing excluded yet";
  ExcludeRow("3-5");
  // The memory Exclude wrote: keyed by (pool slot, the filter row's token), the row's Energy
  // cell as shown (3 of 4 = 75%), under the list's symmetry, first in the session.
  const auto memory = g_state.analysis.excluded_memory.find(std::make_pair(0, std::string("3-5")));
  ASSERT_NE(memory, g_state.analysis.excluded_memory.end());
  EXPECT_DOUBLE_EQ(memory->second.share_pct, 75.0);
  EXPECT_EQ(memory->second.symmetry_bits, kPbd);
  EXPECT_EQ(memory->second.seq, 1u);
  EXPECT_EQ(g_state.analysis.excluded_seq, 1u);
  // Again: already a row of the filter, nothing written, nothing renumbered.
  ASSERT_TRUE(ApplyExcludeSelectedRaypath(g_state));
  EXPECT_EQ(g_state.analysis.excluded_seq, 1u);
  EXPECT_EQ(g_state.analysis.excluded_memory.find(std::make_pair(0, std::string("3-5")))->second.seq, 1u);

  // The filter took: the next result has no 3-5.
  ShowResult(ResultOf(2, { { "1-3", 0, { 1, 3 }, 1.0 } }));
  const std::vector<ExcludedRaypathRow> rows = ComputeExcludedRaypathRows(g_state);
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows[0].display, "3-5");
  EXPECT_EQ(rows[0].token, "3-5");
  EXPECT_EQ(rows[0].pool_crystal_id, 0);
  EXPECT_EQ(rows[0].filter_slot, 0);
  EXPECT_EQ(rows[0].param_index, 0u);
  ASSERT_TRUE(rows[0].was_pct.has_value());
  EXPECT_DOUBLE_EQ(*rows[0].was_pct, 75.0);
  EXPECT_EQ(rows[0].memory_seq, 1u);
  // The texts the row is drawn and copied with.
  EXPECT_EQ(ExcludedRowWasText(rows[0].was_pct), "was 75.00%");
  EXPECT_EQ(ExcludedRowCopyText(rows[0]), "3-5,excluded,75.0000");
  EXPECT_NE(ExcludedRowLabel(rows[0].display).find("3-5"), std::string::npos);
  EXPECT_NE(ExcludedRowLabel(rows[0].display).find("excluded"), std::string::npos);

  // Read under another symmetry: the share is not comparable, the row still is a row.
  g_state.analysis_result.entries_symmetry = LUMICE_RAYPATH_SYMMETRY_P;
  const std::vector<ExcludedRaypathRow> under_p = ComputeExcludedRaypathRows(g_state);
  ASSERT_EQ(under_p.size(), 1u);
  EXPECT_FALSE(under_p[0].was_pct.has_value());
  EXPECT_EQ(under_p[0].memory_seq, 1u) << "the ordering key is kept; only the number is withheld";
  EXPECT_EQ(ExcludedRowWasText(under_p[0].was_pct), "\xe2\x80\x94");
  EXPECT_EQ(ExcludedRowCopyText(under_p[0]), "3-5,excluded,");
}

// The filter is in the document but the result on show still lists the chain (the list was not
// re-analyzed, or was read under a symmetry the filter does not merge): it is a result row, and
// it is not also a greyed one.
TEST(RaypathAnalysisExcludedRowsChain, ResultStillHasTheRow_DoesNotGrayItOut) {
  SeedUnfilteredPrismDocument();
  ShowResult(ResultOf(1, { { "3-5", 0, { 3, 5 }, 1.0 } }));
  ExcludeRow("3-5");
  ASSERT_EQ(g_state.filters.size(), 1u);
  ASSERT_EQ(g_state.filters[0].param.size(), 1u);
  EXPECT_TRUE(ComputeExcludedRaypathRows(g_state).empty());
}

// A filter row that is not a chain — the editor's AND grammar — has no row in the list; the
// pure token beside it does, with nothing remembered for it (nobody clicked Exclude for it).
TEST(RaypathAnalysisExcludedRowsChain, HandWrittenSyntaxRowIsNotGrayedOut) {
  SeedUnfilteredPrismDocument();
  FilterConfig typed;
  typed.name = "typed";
  typed.action = 1;
  typed.param = FromLegacyRaypath(RaypathParams{ "3-5" });
  typed.param.push_back(SummandText{ "entry:2 & len:3", {} });
  g_state.filters.assign(1, typed);
  g_state.layers[0].entries[0].filter_id = 0;
  ShowResult(ResultOf(1, { { "1-3", 0, { 1, 3 }, 1.0 } }));
  const std::vector<ExcludedRaypathRow> rows = ComputeExcludedRaypathRows(g_state);
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows[0].display, "3-5");
  EXPECT_EQ(rows[0].param_index, 0u);
  EXPECT_FALSE(rows[0].was_pct.has_value());
  EXPECT_EQ(rows[0].memory_seq, 0u);
}

// An In filter's rows are not exclusions and never grey; nor are an Out filter's rows on a
// crystal the root layer does not hold (no single-segment chain can be its).
TEST(RaypathAnalysisExcludedRowsChain, InFiltersAndNonRootCrystalsProduceNoRow) {
  SeedUnfilteredPrismDocument();
  FilterConfig keep;
  keep.name = "keep";
  keep.action = 0;
  keep.param = FromLegacyRaypath(RaypathParams{ "3-5" });
  g_state.filters.assign(1, keep);
  g_state.layers[0].entries[0].filter_id = 0;
  ShowResult(ResultOf(1, { { "1-3", 0, { 1, 3 }, 1.0 } }));
  EXPECT_TRUE(ComputeExcludedRaypathRows(g_state).empty()) << "an In filter";

  // A second crystal in a second layer only, with an Out filter.
  g_state.crystals.push_back(g_state.crystals[0]);
  FilterConfig out;
  out.name = "drop";
  out.action = 1;
  out.param = FromLegacyRaypath(RaypathParams{ "2-4" });
  g_state.filters.push_back(out);
  Layer second;
  second.probability = 0.5f;
  EntryCard entry;
  entry.crystal_id = 1;
  entry.proportion = 100.0f;
  entry.filter_id = 1;
  second.entries.assign(1, entry);
  g_state.layers.push_back(second);
  EXPECT_TRUE(ComputeExcludedRaypathRows(g_state).empty()) << "an Out filter on a crystal not in the root layer";
}

// Include again on a filter's only row: the filter is unbound from the entry (the modal's
// Remove Filter shape — the slot stays in the pool, nothing references it), the memory is
// dropped, the row is gone, and the reconciler reads the change as the document edit it is.
TEST(RaypathAnalysisExcludedRowsChain, IncludeAgain_RemovesTheRowFromTheFilter) {
  SeedUnfilteredPrismDocument();
  ShowResult(ResultOf(1, { { "3-5", 0, { 3, 5 }, 1.0 } }));
  ExcludeRow("3-5");
  ShowResult(ResultOf(2, {}));
  std::vector<ExcludedRaypathRow> rows = ComputeExcludedRaypathRows(g_state);
  ASSERT_EQ(rows.size(), 1u);
  // The document as the last Run committed it: with the filter. Include again must read as a
  // hard change against it, as Exclude did against the filter-less baseline.
  g_state.last_committed_state = GuiState::ConfigSnapshot::From(g_state);
  const GuiEffects before = ReconcileGuiEffects(g_state);
  EXPECT_FALSE(before.need_resim);
  EXPECT_FALSE(before.need_hard_reset);

  ASSERT_TRUE(ApplyIncludeAgain(g_state, rows[0]));
  EXPECT_FALSE(g_state.layers[0].entries[0].filter_id.has_value());
  EXPECT_EQ(g_state.filters.size(), 1u) << "the slot is left in the pool, as Remove Filter leaves it";
  EXPECT_TRUE(g_state.analysis.excluded_memory.empty());
  EXPECT_TRUE(ComputeExcludedRaypathRows(g_state).empty());
  const GuiEffects after = ReconcileGuiEffects(g_state);
  EXPECT_TRUE(after.need_resim);
  EXPECT_TRUE(after.need_hard_reset);

  // The row the click was made from is stale now: a second apply finds no entry on the slot
  // and does nothing.
  EXPECT_FALSE(ApplyIncludeAgain(g_state, rows[0]));
}

// Include again on one of two rows: the other stays, the filter stays bound, in place.
TEST(RaypathAnalysisExcludedRowsChain, IncludeAgain_LeavesTheFilterWhenOtherRowsRemain) {
  SeedUnfilteredPrismDocument();
  FilterConfig two;
  two.name = "two";
  two.action = 1;
  two.param = FromLegacyRaypath(RaypathParams{ "1-3" });
  two.param.push_back(FromLegacyRaypath(RaypathParams{ "3-5" }).front());
  g_state.filters.assign(1, two);
  g_state.layers[0].entries[0].filter_id = 0;
  ShowResult(ResultOf(1, {}));
  std::vector<ExcludedRaypathRow> rows = ComputeExcludedRaypathRows(g_state);
  ASSERT_EQ(rows.size(), 2u);
  // No memory for either: document order.
  EXPECT_EQ(rows[0].display, "1-3");
  EXPECT_EQ(rows[1].display, "3-5");
  EXPECT_EQ(rows[1].param_index, 1u);

  ASSERT_TRUE(ApplyIncludeAgain(g_state, rows[1]));
  ASSERT_TRUE(g_state.layers[0].entries[0].filter_id.has_value());
  EXPECT_EQ(*g_state.layers[0].entries[0].filter_id, 0);
  ASSERT_EQ(g_state.filters.size(), 1u);
  ASSERT_EQ(g_state.filters[0].param.size(), 1u);
  EXPECT_EQ(g_state.filters[0].param[0].text, "1-3");
  EXPECT_EQ(g_state.filters[0].name, "two");
  rows = ComputeExcludedRaypathRows(g_state);
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows[0].display, "1-3");

  // A row derived before an edit that moved it: the re-check refuses rather than erasing by a
  // stale index. Here the filter was edited by hand to hold "2-4" at that index.
  g_state.filters[0].param[0] = FromLegacyRaypath(RaypathParams{ "2-4" }).front();
  EXPECT_FALSE(ApplyIncludeAgain(g_state, rows[0]));
  EXPECT_EQ(g_state.filters[0].param.size(), 1u);
}

// Two crystals in the root layer, each excluding its own 3-5 (the same token on two pool
// slots, shown as C0(3-5) and C1(3-5)): two rows, each with its own share, the later exclusion
// first. The memory is keyed by the pool slot, so neither clobbers the other.
TEST(RaypathAnalysisExcludedRowsChain, TwoRootLayerCrystalsExcludingTheSameToken_DoNotClobberEachOthersMemory) {
  SeedUnfilteredPrismDocument();
  AddSecondPrismToRootLayer();
  ShowResult(ResultOf(1, { { "C0(3-5)", 0, { 3, 5 }, 3.0 }, { "C1(3-5)", 1, { 3, 5 }, 1.0 } }));
  ExcludeRow("C0(3-5)");
  ExcludeRow("C1(3-5)");
  ASSERT_EQ(g_state.analysis.excluded_memory.size(), 2u);
  ShowResult(ResultOf(2, {}));
  const std::vector<ExcludedRaypathRow> rows = ComputeExcludedRaypathRows(g_state);
  ASSERT_EQ(rows.size(), 2u);
  EXPECT_EQ(rows[0].display, "C1(3-5)") << "excluded second, listed first";
  EXPECT_EQ(rows[0].pool_crystal_id, 1);
  ASSERT_TRUE(rows[0].was_pct.has_value());
  EXPECT_DOUBLE_EQ(*rows[0].was_pct, 25.0);
  EXPECT_EQ(rows[1].display, "C0(3-5)");
  EXPECT_EQ(rows[1].pool_crystal_id, 0);
  ASSERT_TRUE(rows[1].was_pct.has_value());
  EXPECT_DOUBLE_EQ(*rows[1].was_pct, 75.0);
  EXPECT_EQ(rows[0].token, rows[1].token);
}

// Include again on a filter's only row, where that slot is shared by two entries of the crystal
// (the linked group PropagateFilterIdToLinked exists for) and another crystal holds a slot of its
// own: every entry on the shared slot is unbound, and the other crystal's entry and filter are
// untouched — the reach of the unbind is exactly (this crystal, this slot).
TEST(RaypathAnalysisExcludedRowsChain, IncludeAgain_OnlyClearsEntriesReferencingThisSlot) {
  SeedUnfilteredPrismDocument();
  AddSecondPrismToRootLayer();
  FilterConfig shared;
  shared.name = "shared";
  shared.action = 1;
  shared.param = FromLegacyRaypath(RaypathParams{ "3-5" });
  FilterConfig other;
  other.name = "other";
  other.action = 1;
  other.param = FromLegacyRaypath(RaypathParams{ "1-3" });
  FilterConfig third;
  third.name = "third";
  third.action = 1;
  third.param = FromLegacyRaypath(RaypathParams{ "2-4" });
  g_state.filters = { shared, other, third };
  g_state.layers[0].entries[0].filter_id = 0;  // crystal 0, the shared slot
  g_state.layers[0].entries[1].filter_id = 1;  // crystal 1, its own slot
  Layer second;
  second.probability = 0.5f;
  EntryCard linked;
  linked.crystal_id = 0;
  linked.proportion = 100.0f;
  linked.filter_id = 0;  // crystal 0 again, the shared slot
  EntryCard unrelated;
  unrelated.crystal_id = 0;
  unrelated.proportion = 100.0f;
  unrelated.filter_id = 2;  // crystal 0, a different slot
  second.entries = { linked, unrelated };
  g_state.layers.push_back(second);
  ShowResult(ResultOf(1, {}));

  const std::vector<ExcludedRaypathRow> rows = ComputeExcludedRaypathRows(g_state);
  const ExcludedRaypathRow* row = RowFor(rows, "C0(3-5)");
  ASSERT_NE(row, nullptr);
  EXPECT_EQ(row->filter_slot, 0);
  ASSERT_TRUE(ApplyIncludeAgain(g_state, *row));
  EXPECT_FALSE(g_state.layers[0].entries[0].filter_id.has_value());
  EXPECT_FALSE(g_state.layers[1].entries[0].filter_id.has_value()) << "the linked sibling on the same slot";
  ASSERT_TRUE(g_state.layers[0].entries[1].filter_id.has_value());
  EXPECT_EQ(*g_state.layers[0].entries[1].filter_id, 1) << "the other crystal's slot";
  ASSERT_TRUE(g_state.layers[1].entries[1].filter_id.has_value());
  EXPECT_EQ(*g_state.layers[1].entries[1].filter_id, 2) << "the same crystal's other slot";
  ASSERT_EQ(g_state.filters.size(), 3u);
  EXPECT_EQ(g_state.filters[1].param, other.param);
  EXPECT_EQ(g_state.filters[2].param, third.param);
  // And the derivation agrees: C1(1-3) and C0(2-4) are still rows, C0(3-5) is not.
  const std::vector<ExcludedRaypathRow> after = ComputeExcludedRaypathRows(g_state);
  EXPECT_EQ(after.size(), 2u);
  EXPECT_EQ(RowFor(after, "C0(3-5)"), nullptr);
  EXPECT_NE(RowFor(after, "C1(1-3)"), nullptr);
  EXPECT_NE(RowFor(after, "C0(2-4)"), nullptr);
}

// One crystal, two entries with two distinct Out filters that both hold 3-5: one row for it
// (FilterCoversRow, the predicate Exclude's idempotence reads), attributed to the first filter.
TEST(RaypathAnalysisExcludedRowsChain, ATokenHeldByTwoOutFiltersOfOneCrystalIsOneRow) {
  SeedUnfilteredPrismDocument();
  FilterConfig a;
  a.name = "a";
  a.action = 1;
  a.param = FromLegacyRaypath(RaypathParams{ "3-5" });
  a.param.push_back(FromLegacyRaypath(RaypathParams{ "1-3" }).front());
  FilterConfig b;
  b.name = "b";
  b.action = 1;
  b.param = FromLegacyRaypath(RaypathParams{ "3-5" });
  g_state.filters = { a, b };
  g_state.layers[0].entries[0].filter_id = 0;
  Layer second = g_state.layers[0];
  second.entries[0].filter_id = 1;
  g_state.layers.push_back(second);
  ShowResult(ResultOf(1, {}));
  const std::vector<ExcludedRaypathRow> rows = ComputeExcludedRaypathRows(g_state);
  ASSERT_EQ(rows.size(), 2u);
  EXPECT_EQ(rows[0].display, "3-5");
  EXPECT_EQ(rows[0].filter_slot, 0);
  EXPECT_EQ(rows[1].display, "1-3");
  EXPECT_EQ(rows[1].filter_slot, 0);
}

// A new document forgets the session: the memory and its counter go with the rest of
// RaypathAnalysisSession (ResetFrontendState's whole-struct reset), and with no result on show
// there are no rows to derive.
TEST(RaypathAnalysisExcludedRowsChain, ANewDocumentForgetsTheMemory) {
  SeedUnfilteredPrismDocument();
  ShowResult(ResultOf(1, { { "3-5", 0, { 3, 5 }, 1.0 } }));
  ExcludeRow("3-5");
  ASSERT_EQ(g_state.analysis.excluded_memory.size(), 1u);
  ASSERT_EQ(g_state.analysis.excluded_seq, 1u);
  DoNew();
  EXPECT_TRUE(g_state.analysis.excluded_memory.empty());
  EXPECT_EQ(g_state.analysis.excluded_seq, 0u);
  EXPECT_TRUE(ComputeExcludedRaypathRows(g_state).empty());
}

}  // namespace
}  // namespace lumice::gui
