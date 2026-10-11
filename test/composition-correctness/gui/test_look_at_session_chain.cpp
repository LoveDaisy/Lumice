// Composition chain: named camera follow intent across document lifecycle, persistence and effects.

#include <gtest/gtest.h>

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

#include "gui/app.hpp"
#include "gui/file_io.hpp"
#include "gui/gui_state_reconcile.hpp"
#include "gui/user_defaults.hpp"
#include "gui/view_look_at.hpp"

namespace lumice::gui {
namespace {

class LookAtSessionChain : public ::testing::Test {
 protected:
  void SetUp() override { SeedLockedSession(); }

  static void SeedLockedSession() {
    g_state = MakeNewDocumentState();
    g_state.renderer.fov = 47.0f;
    g_state.renderer.azimuth = 23.0f;
    g_state.renderer.elevation = -17.0f;
    g_state.renderer.roll = 9.0f;
    g_state.look_at = { LookAtId::kSubsun, true };
  }

  static void ExpectReset(FrontendResetReason reason) {
    const RenderConfig camera_before = g_state.renderer;
    ResetFrontendState(g_state, reason);
    EXPECT_EQ(g_state.look_at.target, LookAtId::kSun);
    EXPECT_FALSE(g_state.look_at.locked);
    EXPECT_FLOAT_EQ(g_state.renderer.fov, camera_before.fov);
    EXPECT_FLOAT_EQ(g_state.renderer.azimuth, camera_before.azimuth);
    EXPECT_FLOAT_EQ(g_state.renderer.elevation, camera_before.elevation);
    EXPECT_FLOAT_EQ(g_state.renderer.roll, camera_before.roll);
  }
};

TEST_F(LookAtSessionChain, NewDocumentClearsIntentWithoutChangingTheInstalledCamera) {
  ExpectReset(FrontendResetReason::kNewDocument);
}

TEST_F(LookAtSessionChain, BlankLmcOpenClearsIntentWithoutChangingTheLoadedCamera) {
  ExpectReset(FrontendResetReason::kOpenLmcBlank);
}

TEST_F(LookAtSessionChain, JsonOpenClearsIntentWithoutChangingTheImportedCamera) {
  ExpectReset(FrontendResetReason::kOpenJson);
}

TEST_F(LookAtSessionChain, RevertClearsIntentWithoutRevertingTheView) {
  ExpectReset(FrontendResetReason::kRevert);
}

TEST_F(LookAtSessionChain, CancelledAndFailedOpenLeaveTheCurrentIntentAlone) {
  DoOpen(std::filesystem::path{});
  EXPECT_EQ(g_state.look_at.target, LookAtId::kSubsun);
  EXPECT_TRUE(g_state.look_at.locked);

  const std::filesystem::path missing =
      std::filesystem::path(LUMICE_TEST_FIXTURE_DIR) / "missing-look-at-document.json";
  ASSERT_FALSE(std::filesystem::exists(missing));
  DoOpen(missing);
  EXPECT_EQ(g_state.look_at.target, LookAtId::kSubsun);
  EXPECT_TRUE(g_state.look_at.locked);
}

TEST(LookAtSessionPersistenceChain, DocumentJsonAndSnapshotDoNotCarryFollowIntent) {
  GuiState state = MakeNewDocumentState();
  state.look_at = { LookAtId::kSubsun, true };

  const std::string serialized = SerializeGuiStateJson(state);
  EXPECT_EQ(serialized.find("look_at"), std::string::npos);

  GuiState loaded;
  loaded.look_at = { LookAtId::kZenith, true };
  ASSERT_TRUE(DeserializeGuiStateJson(serialized, loaded));
  EXPECT_EQ(loaded.look_at.target, LookAtId::kSun);
  EXPECT_FALSE(loaded.look_at.locked);

  const GuiState::ConfigSnapshot snapshot = GuiState::ConfigSnapshot::From(state);
  state.look_at = { LookAtId::kZenith, false };
  snapshot.ApplyTo(state);
  EXPECT_EQ(state.look_at.target, LookAtId::kZenith);
  EXPECT_FALSE(state.look_at.locked);
}

TEST(LookAtSessionEffectsChain, PureCameraIntentAndPoseChangesHaveNoSimulationOrDisplayEffect) {
  GuiState state = MakeNewDocumentState();
  state.last_committed_state = GuiState::ConfigSnapshot::From(state);

  state.look_at = { LookAtId::kSubsun, true };
  state.renderer.azimuth = 31.0f;
  state.renderer.elevation = -24.0f;

  EXPECT_EQ(ReconcileGuiEffects(state), GuiEffects{});
}

TEST(LookAtSessionEffectsChain, LockedAndUnlockedSunEditsHaveTheSameExistingSimulationEffect) {
  GuiState unlocked = MakeNewDocumentState();
  unlocked.last_committed_state = GuiState::ConfigSnapshot::From(unlocked);
  GuiState locked = unlocked;

  ASSERT_TRUE(SetLookAtLocked(locked, true));
  unlocked.sun.altitude = 42.0f;
  locked.sun.altitude = 42.0f;
  ASSERT_TRUE(UpdateLockedLookAt(locked));

  const GuiEffects unlocked_effects = ReconcileGuiEffects(unlocked);
  const GuiEffects locked_effects = ReconcileGuiEffects(locked);
  EXPECT_EQ(locked_effects, unlocked_effects);
  EXPECT_TRUE(locked_effects.need_resim);
  EXPECT_FALSE(locked_effects.need_hard_reset);
  EXPECT_FALSE(locked_effects.need_display_push);
}

}  // namespace
}  // namespace lumice::gui
