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
// doc/gui/gui-state-governance.md §11 states for a positional reference whose index space is replaced
// wholesale: there is nothing to compensate to.
//
// The frame-level half — that an Immediate modal really does not push into the new document — is
// not covered by a dedicated test today; it follows mechanically from the gate above, not from a
// separate mechanism: RenderEditModals (edit_modals.cpp) only calls PullBuffersFromPool and only
// reaches the Immediate branch's push/draw when g_active_modal == ActiveModal::kOpen, and
// CloseEditModalOnDocumentReset() (exercised by every case below) sets it to kNone before any
// later frame renders. Once that guard is false there is no frame left in which the old buffers
// could be pushed into the new pool. The same call site and gate are what the Staged case in
// test/gui/functional/test_edit_modal.cpp (a_new_document_closes_the_editor_and_its_uncommitted_edit)
// exercises for the Staged branch, and AC1's functional/*exclude* cases exercise the same
// per-frame pull/push machinery this closes off — neither is a substitute for a dedicated
// Immediate-mode frame-level case, which remains a gap should this gate ever be relaxed.

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
    // The preference is process-wide across this binary's cases; factory baseline, including on
    // the failing path (SetUp/TearDown run regardless of assertions).
    SetEditEntryStagedModePreference(false);
    g_state = MakeNewDocumentState();
  }
  void TearDown() override {
    ResetModalState();
    SetEditEntryStagedModePreference(false);
  }

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

// The saved mode preference is the application's, not the document's: no reset reason may rewrite
// it (AC5), and the next session opened after any reset adopts what it says — not the reset's
// byproduct. The preference is set to the NON-factory value (Staged) so the witness cannot pass on
// a build that merely leaves everything at factory; the session field is then only ever observed
// through the adoption, which is the production read path.
TEST_F(EditModalDocumentResetChain, NoResetReasonRewritesTheSavedModePreference) {
  SetEditEntryStagedModePreference(true);
  OpenOnEntryZero();
  EXPECT_FALSE(g_state.modal_immediate_mode);  // guard: the open really adopted Staged

  ResetFrontendState(g_state, FrontendResetReason::kNewDocument);
  EXPECT_FALSE(IsEditModalOpen());
  EXPECT_TRUE(GetEditEntryStagedModePreference());
  // The session field is untouched by the reset (the contract the old case pinned): it still holds
  // what the closed session adopted. The preference is what governs the next open.
  EXPECT_FALSE(g_state.modal_immediate_mode);

  // The next close→open boundary adopts the surviving preference, not the reset byproduct.
  OpenOnEntryZero();
  EXPECT_FALSE(g_state.modal_immediate_mode);

  for (const FrontendResetReason reason :
       { FrontendResetReason::kOpenLmcBlank, FrontendResetReason::kOpenJson, FrontendResetReason::kRevert }) {
    // Non-fatal on purpose (check_loop_fatal_asserts): one reason failing must not hide the others.
    ResetFrontendState(g_state, reason);
    EXPECT_FALSE(IsEditModalOpen()) << static_cast<int>(reason);
    EXPECT_TRUE(GetEditEntryStagedModePreference()) << static_cast<int>(reason);
  }
  OpenOnEntryZero();
  EXPECT_FALSE(g_state.modal_immediate_mode);
}
}  // namespace
}  // namespace lumice::gui
