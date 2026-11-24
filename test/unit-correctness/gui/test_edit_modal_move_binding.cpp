// The move counterpart of test_edit_modal_delete_binding.cpp.
//
// A layer's entries can be put somewhere other than the end in two ways — Duplicate places the
// copy directly below its original, and a drag reorders a card within its layer — and both go
// through one primitive, MoveEntryWithinLayer, which rotates the vector and then hands the shift to
// NotifyEntryMoved. The positional bindings are the same two the delete side keeps honest: the edit
// modal's (layer, entry) pair, which the Immediate-mode editor writes through every frame while the
// cards behind it stay clickable and draggable, and state.pick_link_source, the entry the
// "Link to..." eyedropper is armed on.
//
// A move removes nothing, so unlike a delete there is no "the bound entry is gone" outcome: every
// binding must still denote the entry it denoted before, whichever of the moved card, the cards it
// jumped over, or the cards outside that span it was. The exhaustive cases below check exactly that
// against an oracle built by erase + insert rather than std::rotate, so an off-by-one in either the
// rotation or the index arithmetic cannot agree with itself.
//
// The end-to-end half — that a real drag on the thumbnail handle reaches MoveEntryWithinLayer — needs
// a frame and lives in test/gui/functional/test_entry_management.cpp.

#include <gtest/gtest.h>

#include <vector>

#include "gui/edit_modals.hpp"
#include "gui/gui_state.hpp"
#include "gui/panels.hpp"

namespace gui = lumice::gui;

namespace {

// Same shape as the delete-side fixture: every entry on a crystal slot of its own, so a slot's
// identity names exactly one entry.
gui::GuiState MakeDoc(int layer_count, int entries_per_layer) {
  gui::GuiState s;
  s.crystals.clear();
  s.filters.clear();
  s.layers.clear();
  for (int l = 0; l < layer_count; ++l) {
    gui::Layer layer;
    for (int e = 0; e < entries_per_layer; ++e) {
      gui::EntryCard card;
      card.crystal_id = static_cast<int>(s.crystals.size());
      gui::CrystalConfig c;
      c.height = 1.0f + static_cast<float>(s.crystals.size());
      s.crystals.push_back(c);
      layer.entries.push_back(card);
    }
    s.layers.push_back(layer);
  }
  return s;
}

std::vector<int> CrystalOrder(const gui::GuiState& s, int layer_idx) {
  std::vector<int> ids;
  for (const auto& e : s.layers[layer_idx].entries) {
    ids.push_back(e.crystal_id);
  }
  return ids;
}

// Independent of MoveEntryWithinLayer's rotate: take the element out, put it back at `to`.
std::vector<int> ExpectedOrderAfterMove(std::vector<int> ids, int from, int to) {
  const int moved = ids[from];
  ids.erase(ids.begin() + from);
  ids.insert(ids.begin() + to, moved);
  return ids;
}

class EditModalMoveBinding : public ::testing::Test {
 protected:
  void SetUp() override { gui::ResetModalState(); }
  void TearDown() override { gui::ResetModalState(); }

  static int BoundCrystalId(const gui::GuiState& s) {
    const gui::EditModalTarget t = gui::GetEditModalTarget();
    if (t.layer_idx < 0 || t.layer_idx >= static_cast<int>(s.layers.size())) {
      return -1;
    }
    const auto& entries = s.layers[t.layer_idx].entries;
    if (t.entry_idx < 0 || t.entry_idx >= static_cast<int>(entries.size())) {
      return -1;
    }
    return entries[t.entry_idx].crystal_id;
  }

  static int ArmedCrystalId(const gui::GuiState& s) {
    if (!s.pick_link_source.has_value()) {
      return -1;
    }
    return s.layers[s.pick_link_source->layer_idx].entries[s.pick_link_source->entry_idx].crystal_id;
  }

  static void Open(gui::GuiState& s, int layer_idx, int entry_idx) {
    gui::EditRequest req{ gui::EditTarget::kCrystal, layer_idx, entry_idx };
    gui::OpenEditModal(req, s);
  }
};

constexpr int kN = 5;

// ---------------------------------------------------------------------------
// Exhaustive: every (from, to, bound) over a five-card layer. Covers the moved card itself, cards
// inside the jumped-over span in both directions, and cards outside it, in one sweep.
// ---------------------------------------------------------------------------

TEST_F(EditModalMoveBinding, EveryMoveKeepsTheModalOnTheSameEntry) {
  for (int from = 0; from < kN; ++from) {
    for (int to = 0; to < kN; ++to) {
      for (int bound = 0; bound < kN; ++bound) {
        gui::ResetModalState();
        gui::GuiState s = MakeDoc(1, kN);
        Open(s, 0, bound);
        const int edited = BoundCrystalId(s);
        const std::vector<int> expected = ExpectedOrderAfterMove(CrystalOrder(s, 0), from, to);

        gui::MoveEntryWithinLayer(s, 0, from, to);

        SCOPED_TRACE(testing::Message() << "from=" << from << " to=" << to << " bound=" << bound);
        EXPECT_EQ(CrystalOrder(s, 0), expected);
        EXPECT_TRUE(gui::IsEditModalOpen());
        EXPECT_EQ(BoundCrystalId(s), edited);
      }
    }
  }
}

TEST_F(EditModalMoveBinding, EveryMoveKeepsTheArmedPickSourceOnTheSameEntry) {
  for (int from = 0; from < kN; ++from) {
    for (int to = 0; to < kN; ++to) {
      for (int armed = 0; armed < kN; ++armed) {
        gui::GuiState s = MakeDoc(1, kN);
        gui::StartLinkPickMode(s, 0, armed);
        const int armed_cid = ArmedCrystalId(s);

        gui::MoveEntryWithinLayer(s, 0, from, to);

        SCOPED_TRACE(testing::Message() << "from=" << from << " to=" << to << " armed=" << armed);
        if (!s.pick_link_source.has_value()) {
          ADD_FAILURE() << "a move ended pick mode";
          continue;
        }
        EXPECT_EQ(s.pick_link_source->layer_idx, 0);
        EXPECT_EQ(ArmedCrystalId(s), armed_cid);
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Named cases for the four arithmetic outcomes, so a failure reads as a rule rather than a triple.
// ---------------------------------------------------------------------------

TEST_F(EditModalMoveBinding, TheMovedEntryTakesItsBindingAlong) {
  gui::GuiState s = MakeDoc(1, kN);
  Open(s, 0, 1);
  gui::MoveEntryWithinLayer(s, 0, 1, 3);
  EXPECT_EQ(gui::GetEditModalTarget().entry_idx, 3);
}

TEST_F(EditModalMoveBinding, AnEntryJumpedOverDownwardShiftsUp) {
  gui::GuiState s = MakeDoc(1, kN);
  Open(s, 0, 3);
  gui::MoveEntryWithinLayer(s, 0, 1, 3);  // from < to: (1, 3] shifts toward 1
  EXPECT_EQ(gui::GetEditModalTarget().entry_idx, 2);
}

TEST_F(EditModalMoveBinding, AnEntryJumpedOverUpwardShiftsDown) {
  gui::GuiState s = MakeDoc(1, kN);
  Open(s, 0, 1);
  gui::MoveEntryWithinLayer(s, 0, 3, 1);  // from > to: [1, 3) shifts toward 3
  EXPECT_EQ(gui::GetEditModalTarget().entry_idx, 2);
}

TEST_F(EditModalMoveBinding, AnEntryOutsideTheSpanStaysPut) {
  gui::GuiState s = MakeDoc(1, kN);
  Open(s, 0, 4);
  gui::MoveEntryWithinLayer(s, 0, 0, 2);
  EXPECT_EQ(gui::GetEditModalTarget().entry_idx, 4);
}

TEST_F(EditModalMoveBinding, AMoveInAnotherLayerDoesNotTouchEitherBinding) {
  gui::GuiState s = MakeDoc(2, kN);
  Open(s, 1, 0);
  gui::StartLinkPickMode(s, 1, 2);  // arming closes the modal: re-open after it
  Open(s, 1, 0);
  ASSERT_TRUE(gui::IsEditModalOpen());

  gui::MoveEntryWithinLayer(s, 0, 0, 4);

  EXPECT_EQ(gui::GetEditModalTarget().layer_idx, 1);
  EXPECT_EQ(gui::GetEditModalTarget().entry_idx, 0);
  ASSERT_TRUE(s.pick_link_source.has_value());
  EXPECT_EQ(s.pick_link_source->layer_idx, 1);
  EXPECT_EQ(s.pick_link_source->entry_idx, 2);
}

TEST_F(EditModalMoveBinding, ANoOpMoveChangesNothing) {
  gui::GuiState s = MakeDoc(1, kN);
  Open(s, 0, 2);
  const std::vector<int> before = CrystalOrder(s, 0);

  gui::MoveEntryWithinLayer(s, 0, 2, 2);
  gui::MoveEntryWithinLayer(s, 0, -1, 2);
  gui::MoveEntryWithinLayer(s, 0, 2, kN);
  gui::MoveEntryWithinLayer(s, 7, 0, 1);

  EXPECT_EQ(CrystalOrder(s, 0), before);
  EXPECT_EQ(gui::GetEditModalTarget().entry_idx, 2);
}

TEST_F(EditModalMoveBinding, NotificationIsInertWhileNothingIsBound) {
  gui::GuiState s = MakeDoc(1, kN);
  gui::MoveEntryWithinLayer(s, 0, 0, 4);
  EXPECT_FALSE(gui::IsEditModalOpen());
  EXPECT_FALSE(s.pick_link_source.has_value());
}

// ---------------------------------------------------------------------------
// Drop gap -> final index. A drop into either gap next to the dragged card is a no-op.
// ---------------------------------------------------------------------------

TEST(CardDropGap, GapsBelowTheDraggedCardShiftUpByOne) {
  // Dragging card 1 of five: gaps 0..5.
  EXPECT_EQ(gui::DropGapToMoveTarget(1, 0), 0);
  EXPECT_EQ(gui::DropGapToMoveTarget(1, 1), 1);  // gap above itself: no-op
  EXPECT_EQ(gui::DropGapToMoveTarget(1, 2), 1);  // gap below itself: no-op
  EXPECT_EQ(gui::DropGapToMoveTarget(1, 3), 2);
  EXPECT_EQ(gui::DropGapToMoveTarget(1, 5), 4);  // below the last card
}

TEST(CardDropGap, EveryGapLandsTheCardThere) {
  // The card must end up immediately above whatever card was below the chosen gap.
  for (int from = 0; from < kN; ++from) {
    for (int gap = 0; gap <= kN; ++gap) {
      std::vector<int> ids{ 0, 1, 2, 3, 4 };
      const int to = gui::DropGapToMoveTarget(from, gap);
      if (to < 0 || to >= kN) {
        ADD_FAILURE() << "from=" << from << " gap=" << gap << " -> out-of-range target " << to;
        continue;
      }
      const std::vector<int> after = ExpectedOrderAfterMove(ids, from, to);
      SCOPED_TRACE(testing::Message() << "from=" << from << " gap=" << gap);
      if (gap < kN && gap != from) {
        EXPECT_EQ(after[to + 1], ids[gap]);  // the card that was below the gap is right below it
      }
      if (gap > 0 && gap - 1 != from) {
        EXPECT_EQ(after[to - 1], ids[gap - 1]);  // and the one above the gap right above it
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Duplicate places the copy directly below its original.
// ---------------------------------------------------------------------------

TEST_F(EditModalMoveBinding, DuplicateLandsDirectlyBelowTheOriginal) {
  gui::GuiState s = MakeDoc(1, 4);
  const std::vector<int> before = CrystalOrder(s, 0);

  const int copy_idx = gui::DuplicateEntryBelow(s, 0, 1);

  ASSERT_EQ(copy_idx, 2);
  const std::vector<int> after = CrystalOrder(s, 0);
  ASSERT_EQ(after.size(), 5u);
  EXPECT_EQ(after[0], before[0]);
  EXPECT_EQ(after[1], before[1]);
  EXPECT_EQ(after[3], before[2]);
  EXPECT_EQ(after[4], before[3]);
  // An independent copy: a fresh slot with the original's content.
  EXPECT_NE(after[2], before[1]);
  EXPECT_EQ(s.crystals[after[2]].height, s.crystals[before[1]].height);
}

TEST_F(EditModalMoveBinding, DuplicatingTheLastCardAppends) {
  gui::GuiState s = MakeDoc(1, 3);
  const int copy_idx = gui::DuplicateEntryBelow(s, 0, 2);
  EXPECT_EQ(copy_idx, 3);
  EXPECT_EQ(s.layers[0].entries.size(), 4u);
}

TEST_F(EditModalMoveBinding, DuplicateCarriesTheCardsFieldsAndClonesItsFilter) {
  gui::GuiState s = MakeDoc(1, 3);
  s.filters.emplace_back();
  s.layers[0].entries[0].filter_id = 0;
  s.layers[0].entries[0].proportion = 42.0f;
  s.layers[0].entries[0].enabled = false;

  const int copy_idx = gui::DuplicateEntryBelow(s, 0, 0);

  ASSERT_EQ(copy_idx, 1);
  const auto& copy = s.layers[0].entries[1];
  EXPECT_EQ(copy.proportion, 42.0f);
  EXPECT_FALSE(copy.enabled);
  ASSERT_TRUE(copy.filter_id.has_value());
  EXPECT_NE(*copy.filter_id, 0);  // its own filter slot, not a share
  EXPECT_EQ(s.filters.size(), 2u);
}

TEST_F(EditModalMoveBinding, DuplicateKeepsBindingsOnTheirEntries) {
  gui::GuiState s = MakeDoc(1, 4);
  Open(s, 0, 3);  // below the insertion point: must shift down with its entry
  const int edited = BoundCrystalId(s);

  gui::DuplicateEntryBelow(s, 0, 0);

  EXPECT_TRUE(gui::IsEditModalOpen());
  EXPECT_EQ(gui::GetEditModalTarget().entry_idx, 4);
  EXPECT_EQ(BoundCrystalId(s), edited);

  gui::ResetModalState();
  gui::GuiState s2 = MakeDoc(1, 4);
  gui::StartLinkPickMode(s2, 0, 1);  // the duplicated card itself: stays on the original
  const int armed = ArmedCrystalId(s2);
  gui::DuplicateEntryBelow(s2, 0, 1);
  ASSERT_TRUE(s2.pick_link_source.has_value());
  EXPECT_EQ(s2.pick_link_source->entry_idx, 1);
  EXPECT_EQ(ArmedCrystalId(s2), armed);
}

}  // namespace
