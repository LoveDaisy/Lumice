// Composition chain: Duplicate's three-state focus transaction.
//
// Units in the chain: panels × edit_modals × gui_state. The wrapper orchestrates the editor's
// commit primitive, the plain clone, the positional compensation and the editor rebind; what the
// collaboration produces that no single unit can answer is the transaction itself — the order
// (commit before the pool snapshot, rebind after the final insert position) and the way the
// editor's positional state rides along.
//
// The state-2 flash arm needs a frame clock (ImGui::GetTime) and its pixel consumption needs a
// rendered frame, so both live with the gui_test witnesses in
// test/gui/functional/test_entry_management.cpp; what a no-frame test owns is the positional
// machinery below. The plain DuplicateEntryBelow keeps its own entry-level pins in
// test/unit-correctness/gui/test_edit_modal_move_binding.cpp — those predate this wrapper and
// stay single-module propositions.

#include <gtest/gtest.h>

#include <vector>

#include "gui/edit_modals.hpp"
#include "gui/entry_delete.hpp"
#include "gui/gui_state.hpp"
#include "gui/panels.hpp"

namespace lumice::gui {
namespace {

// Same shape as the move-binding fixture's: every entry on a crystal slot of its own, so a slot's
// identity names exactly one entry.
GuiState MakeDoc(int layer_count, int entries_per_layer) {
  GuiState s;
  s.crystals.clear();
  s.filters.clear();
  s.layers.clear();
  for (int l = 0; l < layer_count; ++l) {
    Layer layer;
    for (int e = 0; e < entries_per_layer; ++e) {
      EntryCard card;
      card.crystal_id = static_cast<int>(s.crystals.size());
      CrystalConfig c;
      c.height = 1.0f + static_cast<float>(s.crystals.size());
      s.crystals.push_back(c);
      layer.entries.push_back(card);
    }
    s.layers.push_back(layer);
  }
  return s;
}

// The crystal id the open modal is bound to, through the same (layer, entry) pair the card
// highlight reads; -1 when the binding is out of range or no modal is open.
int BoundCrystalId(const GuiState& s) {
  const EditModalTarget t = GetEditModalTarget();
  if (t.layer_idx < 0 || t.layer_idx >= static_cast<int>(s.layers.size())) {
    return -1;
  }
  const auto& entries = s.layers[t.layer_idx].entries;
  if (t.entry_idx < 0 || t.entry_idx >= static_cast<int>(entries.size())) {
    return -1;
  }
  return entries[t.entry_idx].crystal_id;
}

void Open(GuiState& s, int layer_idx, int entry_idx) {
  EditRequest req{ EditTarget::kCrystal, layer_idx, entry_idx };
  OpenEditModal(req, s);
}

class DuplicateFocusChain : public ::testing::Test {
 protected:
  void SetUp() override { ResetModalState(); }
  void TearDown() override { ResetModalState(); }
};

TEST_F(DuplicateFocusChain, RebindsTheEditorToTheClone) {
  GuiState s = MakeDoc(1, 3);
  Open(s, 0, 1);

  const int copy_idx = DuplicateEntryWithFocusFollow(s, 0, 1);

  // The clone sits directly below its original, on a fresh slot carrying its content...
  ASSERT_EQ(copy_idx, 2);
  const auto& copy = s.layers[0].entries[copy_idx];
  EXPECT_NE(copy.crystal_id, s.layers[0].entries[1].crystal_id);
  EXPECT_EQ(s.crystals[copy.crystal_id].height, s.crystals[s.layers[0].entries[1].crystal_id].height);
  // ...and the editor followed it: still open, bound to the copy, no flash armed (the active
  // border IS the focus signal in this state).
  EXPECT_TRUE(IsEditModalOpen());
  EXPECT_EQ(GetEditModalTarget().entry_idx, copy_idx);
  EXPECT_EQ(BoundCrystalId(s), copy.crystal_id);
  EXPECT_FALSE(s.duplicate_flash.has_value());
}

TEST_F(DuplicateFocusChain, OnAnotherCardKeepsTheEditorAndArmsNoFlash) {
  GuiState s = MakeDoc(1, 4);
  Open(s, 0, 3);
  const int edited = BoundCrystalId(s);

  const int copy_idx = DuplicateEntryWithFocusFollow(s, 0, 0);

  ASSERT_EQ(copy_idx, 1);
  EXPECT_TRUE(IsEditModalOpen());
  EXPECT_EQ(BoundCrystalId(s), edited);  // compensated onto its entry, not rebound
  EXPECT_FALSE(s.duplicate_flash.has_value());
}

TEST_F(DuplicateFocusChain, FlashFollowsItsEntryThroughMovesAndDeletes) {
  // Armed by hand: the arm itself needs a frame clock; what is under test here is purely the
  // positional bookkeeping the Notify* rules owe the flash.
  GuiState s = MakeDoc(1, 5);  // [a b c d e], flash on c (index 2)
  s.duplicate_flash = GuiState::DuplicateFlash{ 0, 2, 0.0 };

  // A move of an earlier card drags the flash's entry with it.
  MoveEntryWithinLayer(s, 0, 0, 4);  // -> [b c d e a]: c is now at 1
  ASSERT_TRUE(s.duplicate_flash.has_value());
  EXPECT_EQ(s.duplicate_flash->entry_idx, 1);

  // A move OF the flashed card follows it to its landing spot.
  MoveEntryWithinLayer(s, 0, 1, 3);  // -> [b d e c a]: c is now at 3
  ASSERT_TRUE(s.duplicate_flash.has_value());
  EXPECT_EQ(s.duplicate_flash->entry_idx, 3);

  // A delete before it shifts it down; a delete after it leaves it alone.
  ASSERT_TRUE(DeleteLayerEntry(s, 0, 0));  // -> [d e c a]: c at 2
  ASSERT_TRUE(s.duplicate_flash.has_value());
  EXPECT_EQ(s.duplicate_flash->entry_idx, 2);
  ASSERT_TRUE(DeleteLayerEntry(s, 0, 3));  // a was below c: -> [d e c], c stays at 2
  ASSERT_TRUE(s.duplicate_flash.has_value());
  EXPECT_EQ(s.duplicate_flash->entry_idx, 2);

  // Deleting the flashed entry itself drops the flash.
  ASSERT_TRUE(DeleteLayerEntry(s, 0, 2));
  EXPECT_FALSE(s.duplicate_flash.has_value());

  // Its layer going away ends it too; a sibling layer's deletion compensates instead.
  GuiState two = MakeDoc(2, 2);
  two.duplicate_flash = GuiState::DuplicateFlash{ 1, 0, 0.0 };
  NotifyLayerDeleted(two, 0);
  ASSERT_TRUE(two.duplicate_flash.has_value());
  EXPECT_EQ(two.duplicate_flash->layer_idx, 0);
  NotifyLayerDeleted(two, 0);
  EXPECT_FALSE(two.duplicate_flash.has_value());
}

}  // namespace
}  // namespace lumice::gui
