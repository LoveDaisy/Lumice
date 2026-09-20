// Composition chain: a document reset and the edit modal bound into the document it replaces.
//
// Units in the chain: app (ResetFrontendState, the single frontend-reset owner) × edit_modals (the
// open modal's positional binding and its edit buffers).
//
// What the collaboration produces that is observable: after any reset reason, no edit modal is
// open. The modal binds its target by (layer, entry) POSITION and holds copies of that entry's
// crystal and filter; every reset reason replaces or restores the pool those copies came from, so
// the binding names whatever the new document has at the same numbers — or nothing — and the
// copies describe a crystal the document no longer holds. Under Immediate mode the modal writes
// those copies into its bound slot every frame, so a modal left open across a reset overwrites the
// new document's entry with the old one's crystal, with no error anywhere. Closing is the rule
// doc/gui-state-governance.md §11 states for a positional reference whose index space is replaced
// wholesale: there is nothing to compensate to.
//
// The frame-level half — that an Immediate modal really does not push into the new document — is
// test/gui/functional/test_edit_modal.cpp's a_new_document_closes_the_immediate_editor_before_it_can_push.

#include <gtest/gtest.h>

#include "gui/app.hpp"
#include "gui/edit_modals.hpp"
#include "gui/gui_state.hpp"
#include "gui/panels.hpp"         // EditRequest / EditTarget
#include "gui/user_defaults.hpp"  // MakeNewDocumentState

namespace lumice::gui {
namespace {

class EditModalDocumentResetChain : public ::testing::Test {
 protected:
  void SetUp() override {
    ResetModalState();
    g_state = MakeNewDocumentState();
  }
  void TearDown() override { ResetModalState(); }

  static void OpenOnEntryZero() {
    const EditRequest req{ EditTarget::kCrystal, 0, 0 };
    OpenEditModal(req, g_state);
    ASSERT_TRUE(IsEditModalOpen());
  }
};

// Every reason that takes no payload, one case each rather than a loop: a reason that stops closing
// the modal should name itself in the failure, and a fatal assert in a loop body would hide the
// reasons after the first (scripts/check_loop_fatal_asserts.py).

TEST_F(EditModalDocumentResetChain, ANewDocumentClosesTheOpenModal) {
  OpenOnEntryZero();
  ResetFrontendState(g_state, FrontendResetReason::kNewDocument);
  EXPECT_FALSE(IsEditModalOpen());
  EXPECT_EQ(GetEditModalTarget().layer_idx, -1);
  EXPECT_EQ(GetEditModalTarget().entry_idx, -1);
}

TEST_F(EditModalDocumentResetChain, OpeningABlankLmcClosesTheOpenModal) {
  OpenOnEntryZero();
  ResetFrontendState(g_state, FrontendResetReason::kOpenLmcBlank);
  EXPECT_FALSE(IsEditModalOpen());
}

TEST_F(EditModalDocumentResetChain, ImportingJsonClosesTheOpenModal) {
  OpenOnEntryZero();
  ResetFrontendState(g_state, FrontendResetReason::kOpenJson);
  EXPECT_FALSE(IsEditModalOpen());
}

TEST_F(EditModalDocumentResetChain, RevertClosesTheOpenModal) {
  OpenOnEntryZero();
  ResetFrontendState(g_state, FrontendResetReason::kRevert);
  EXPECT_FALSE(IsEditModalOpen());
}

// The reset does not touch the modal's view preference: it is a session setting the reset reasons
// have no say over, and the test-only ResetModalState is the one place that forces it.
TEST_F(EditModalDocumentResetChain, TheResetLeavesTheImmediateModePreferenceAlone) {
  g_state.modal_immediate_mode = true;
  OpenOnEntryZero();
  ResetFrontendState(g_state, FrontendResetReason::kNewDocument);
  EXPECT_FALSE(IsEditModalOpen());
  EXPECT_TRUE(g_state.modal_immediate_mode);
}

}  // namespace
}  // namespace lumice::gui
