// The edit modal binds its target by INDEX, and deleting from a vector is the one operation that
// changes what an index means.
//
// `g_modal_layer_idx` / `g_modal_entry_idx` name a position, not an object. `erase()` shifts every
// later element up one, so after a delete the same pair of numbers can denote a different entry —
// and in Immediate mode the modal is a plain window that keeps writing its edit buffers into
// whatever now sits at those numbers, one write per frame. The bounds guard in RenderEditModals
// only catches the index falling off the END of the vector, which is the single case where the
// mistake is not silent.
//
// NotifyEntryDeleted / NotifyLayerDeleted are the whole of the repair: the delete site tells the
// binding what just happened to the index space, and the binding either follows its entry to the
// new index or closes because that entry is gone. What is asserted here is that arithmetic, over
// every ordering of (deleted index, bound index) plus the two no-op cases — a proposition about one
// unit, with no frame and no input event in it, so it belongs here rather than in gui_test. The
// end-to-end half — that panels.cpp's delete button actually reaches these functions — needs a real
// frame and lives in test/gui/functional/test_entry_management.cpp.
//
// The second positional binding the same two functions keep honest: state.pick_link_source, the
// entry the "Link to..." eyedropper is armed on. It names a position too, it outlives the modal
// (arming it closes the modal), and the card delete buttons stay clickable while it is armed — so
// the same three outcomes apply, with "the deleted item IS the armed one" ending pick mode rather
// than re-aiming it at the entry that slid into its place.
//
// What a user sees when this breaks: they delete one card and a DIFFERENT card's crystal and filter
// are replaced by the deleted one's, with no error anywhere — or the link they are picking a model
// for lands on a card they never armed.

#include <gtest/gtest.h>

#include "gui/edit_modals.hpp"
#include "gui/gui_state.hpp"
#include "gui/panels.hpp"

namespace gui = lumice::gui;

namespace {

// A document with `layer_count` layers of `entries_per_layer` entries, every entry on a crystal
// slot of its own so a slot's identity names exactly one entry.
gui::GuiState MakeDoc(int layer_count, int entries_per_layer) {
  gui::GuiState s;
  s.crystals.clear();
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

// The modal statics are file-level in edit_modals.cpp and therefore shared by every case in this
// binary; a case that returned early on a failed expectation would otherwise leave the next one
// starting from an open modal.
class EditModalDeleteBinding : public ::testing::Test {
 protected:
  void SetUp() override { gui::ResetModalState(); }
  void TearDown() override { gui::ResetModalState(); }

  // The crystal slot the modal is currently aimed at, or -1 if it is closed or aimed out of range.
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

  static void Open(gui::GuiState& s, int layer_idx, int entry_idx) {
    gui::EditRequest req{ gui::EditTarget::kCrystal, layer_idx, entry_idx };
    gui::OpenEditModal(req, s);
  }
};

// ---------------------------------------------------------------------------
// Entry deletes, over the three orderings of (deleted index, bound index).
// ---------------------------------------------------------------------------

TEST_F(EditModalDeleteBinding, DeletingTheBoundEntryClosesTheModal) {
  gui::GuiState s = MakeDoc(1, 3);
  Open(s, 0, 1);
  ASSERT_TRUE(gui::IsEditModalOpen());

  s.layers[0].entries.erase(s.layers[0].entries.begin() + 1);
  gui::NotifyEntryDeleted(s, 0, 1);

  EXPECT_FALSE(gui::IsEditModalOpen());
}

TEST_F(EditModalDeleteBinding, DeletingAnEntryBeforeTheBoundOneFollowsItDown) {
  gui::GuiState s = MakeDoc(1, 3);
  Open(s, 0, 2);
  const int edited = BoundCrystalId(s);
  ASSERT_EQ(edited, s.layers[0].entries[2].crystal_id);

  s.layers[0].entries.erase(s.layers[0].entries.begin() + 0);
  gui::NotifyEntryDeleted(s, 0, 0);

  EXPECT_TRUE(gui::IsEditModalOpen());
  EXPECT_EQ(gui::GetEditModalTarget().entry_idx, 1);
  // The index moved because the entry did; asserting the slot is what separates that from an
  // index that merely happens to be in range.
  EXPECT_EQ(BoundCrystalId(s), edited);
}

TEST_F(EditModalDeleteBinding, DeletingAnEntryAfterTheBoundOneLeavesItAlone) {
  gui::GuiState s = MakeDoc(1, 3);
  Open(s, 0, 0);
  const int edited = BoundCrystalId(s);

  s.layers[0].entries.erase(s.layers[0].entries.begin() + 2);
  gui::NotifyEntryDeleted(s, 0, 2);

  EXPECT_TRUE(gui::IsEditModalOpen());
  EXPECT_EQ(gui::GetEditModalTarget().entry_idx, 0);
  EXPECT_EQ(BoundCrystalId(s), edited);
}

TEST_F(EditModalDeleteBinding, AnEntryDeleteInAnotherLayerDoesNotMoveTheBinding) {
  gui::GuiState s = MakeDoc(2, 3);
  Open(s, 1, 0);
  const int edited = BoundCrystalId(s);

  s.layers[0].entries.erase(s.layers[0].entries.begin() + 0);
  gui::NotifyEntryDeleted(s, 0, 0);

  EXPECT_TRUE(gui::IsEditModalOpen());
  EXPECT_EQ(gui::GetEditModalTarget().layer_idx, 1);
  EXPECT_EQ(gui::GetEditModalTarget().entry_idx, 0);
  EXPECT_EQ(BoundCrystalId(s), edited);
}

// ---------------------------------------------------------------------------
// Layer deletes. Same defect, same index space, one level up: the layer x button is reachable
// while the Immediate-mode editor is open, and deleting a layer above the bound one leaves
// g_modal_layer_idx in range and pointing at the layer that shifted up into it.
// ---------------------------------------------------------------------------

TEST_F(EditModalDeleteBinding, DeletingTheBoundLayerClosesTheModal) {
  gui::GuiState s = MakeDoc(3, 2);
  Open(s, 1, 0);
  ASSERT_TRUE(gui::IsEditModalOpen());

  s.layers.erase(s.layers.begin() + 1);
  gui::NotifyLayerDeleted(s, 1);

  EXPECT_FALSE(gui::IsEditModalOpen());
}

TEST_F(EditModalDeleteBinding, DeletingALayerBeforeTheBoundOneFollowsItDown) {
  gui::GuiState s = MakeDoc(3, 2);
  Open(s, 2, 1);
  const int edited = BoundCrystalId(s);

  s.layers.erase(s.layers.begin() + 0);
  gui::NotifyLayerDeleted(s, 0);

  EXPECT_TRUE(gui::IsEditModalOpen());
  EXPECT_EQ(gui::GetEditModalTarget().layer_idx, 1);
  EXPECT_EQ(gui::GetEditModalTarget().entry_idx, 1);
  EXPECT_EQ(BoundCrystalId(s), edited);
}

TEST_F(EditModalDeleteBinding, DeletingALayerAfterTheBoundOneLeavesItAlone) {
  gui::GuiState s = MakeDoc(3, 2);
  Open(s, 0, 1);
  const int edited = BoundCrystalId(s);

  s.layers.erase(s.layers.begin() + 2);
  gui::NotifyLayerDeleted(s, 2);

  EXPECT_TRUE(gui::IsEditModalOpen());
  EXPECT_EQ(gui::GetEditModalTarget().layer_idx, 0);
  EXPECT_EQ(BoundCrystalId(s), edited);
}

// ---------------------------------------------------------------------------
// With no modal open there is no binding to keep, and the stale indices left behind are overwritten
// wholesale by the next OpenEditModal. Both notifications must be inert rather than resurrect one.
// ---------------------------------------------------------------------------

TEST_F(EditModalDeleteBinding, NotificationsAreInertWhileNoModalIsOpen) {
  gui::GuiState s = MakeDoc(2, 3);
  ASSERT_FALSE(gui::IsEditModalOpen());

  gui::NotifyEntryDeleted(s, 0, 0);
  gui::NotifyLayerDeleted(s, 0);

  EXPECT_FALSE(gui::IsEditModalOpen());
  EXPECT_EQ(gui::GetEditModalTarget().layer_idx, -1);
  EXPECT_EQ(gui::GetEditModalTarget().entry_idx, -1);
}

// ---------------------------------------------------------------------------
// pick_link_source, over the same orderings. Armed through StartLinkPickMode — the production entry
// point — with no modal open, so the notification's modal branch is inert and what moves is the
// pick source alone.
// ---------------------------------------------------------------------------

TEST_F(EditModalDeleteBinding, DeletingTheArmedEntryEndsPickMode) {
  gui::GuiState s = MakeDoc(1, 3);
  gui::StartLinkPickMode(s, 0, 1);
  ASSERT_TRUE(s.pick_link_source.has_value());

  s.layers[0].entries.erase(s.layers[0].entries.begin() + 1);
  gui::NotifyEntryDeleted(s, 0, 1);

  EXPECT_FALSE(s.pick_link_source.has_value());
}

TEST_F(EditModalDeleteBinding, DeletingAnEntryBeforeTheArmedOneFollowsItDown) {
  gui::GuiState s = MakeDoc(1, 3);
  gui::StartLinkPickMode(s, 0, 2);
  const int armed = s.layers[0].entries[2].crystal_id;

  s.layers[0].entries.erase(s.layers[0].entries.begin() + 0);
  gui::NotifyEntryDeleted(s, 0, 0);

  ASSERT_TRUE(s.pick_link_source.has_value());
  EXPECT_EQ(s.pick_link_source->layer_idx, 0);
  EXPECT_EQ(s.pick_link_source->entry_idx, 1);
  EXPECT_EQ(s.layers[0].entries[s.pick_link_source->entry_idx].crystal_id, armed);
}

TEST_F(EditModalDeleteBinding, DeletingAnEntryAfterTheArmedOneLeavesItAlone) {
  gui::GuiState s = MakeDoc(1, 3);
  gui::StartLinkPickMode(s, 0, 0);

  s.layers[0].entries.erase(s.layers[0].entries.begin() + 2);
  gui::NotifyEntryDeleted(s, 0, 2);

  ASSERT_TRUE(s.pick_link_source.has_value());
  EXPECT_EQ(s.pick_link_source->entry_idx, 0);
}

TEST_F(EditModalDeleteBinding, AnEntryDeleteInAnotherLayerDoesNotMoveTheArmedOne) {
  gui::GuiState s = MakeDoc(2, 3);
  gui::StartLinkPickMode(s, 1, 2);

  s.layers[0].entries.erase(s.layers[0].entries.begin() + 0);
  gui::NotifyEntryDeleted(s, 0, 0);

  ASSERT_TRUE(s.pick_link_source.has_value());
  EXPECT_EQ(s.pick_link_source->layer_idx, 1);
  EXPECT_EQ(s.pick_link_source->entry_idx, 2);
}

TEST_F(EditModalDeleteBinding, DeletingTheArmedEntrysLayerEndsPickMode) {
  gui::GuiState s = MakeDoc(3, 2);
  gui::StartLinkPickMode(s, 1, 0);

  s.layers.erase(s.layers.begin() + 1);
  gui::NotifyLayerDeleted(s, 1);

  EXPECT_FALSE(s.pick_link_source.has_value());
}

TEST_F(EditModalDeleteBinding, DeletingALayerBeforeTheArmedEntrysFollowsItDown) {
  gui::GuiState s = MakeDoc(3, 2);
  gui::StartLinkPickMode(s, 2, 1);
  const int armed = s.layers[2].entries[1].crystal_id;

  s.layers.erase(s.layers.begin() + 0);
  gui::NotifyLayerDeleted(s, 0);

  ASSERT_TRUE(s.pick_link_source.has_value());
  EXPECT_EQ(s.pick_link_source->layer_idx, 1);
  EXPECT_EQ(s.pick_link_source->entry_idx, 1);
  EXPECT_EQ(s.layers[1].entries[1].crystal_id, armed);
}

TEST_F(EditModalDeleteBinding, DeletingALayerAfterTheArmedEntrysLeavesItAlone) {
  gui::GuiState s = MakeDoc(3, 2);
  gui::StartLinkPickMode(s, 0, 1);

  s.layers.erase(s.layers.begin() + 2);
  gui::NotifyLayerDeleted(s, 2);

  ASSERT_TRUE(s.pick_link_source.has_value());
  EXPECT_EQ(s.pick_link_source->layer_idx, 0);
}

TEST_F(EditModalDeleteBinding, NotificationsAreInertWhileNothingIsArmed) {
  gui::GuiState s = MakeDoc(2, 3);
  ASSERT_FALSE(s.pick_link_source.has_value());

  s.layers[0].entries.erase(s.layers[0].entries.begin() + 0);
  gui::NotifyEntryDeleted(s, 0, 0);
  s.layers.erase(s.layers.begin() + 0);
  gui::NotifyLayerDeleted(s, 0);

  EXPECT_FALSE(s.pick_link_source.has_value());
}

}  // namespace
