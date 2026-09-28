// What the preview shows in the frames after the user changes the simulation config.
//
// A config edit never blanks the frame on screen; the new generation replaces it under
// doc/gui-preview-lifecycle-architecture.md §7's rules (quality gate, 500 ms timeout fallback,
// terminal frame always uploaded). Each case runs a real simulation, applies one config change
// through the path the user takes, and watches the shader's mono intensity scale frame by frame for
// a fixed wall-clock window (0 means the shader multiplies every texel by zero — a black preview).
// A read-back through the export FBO at the end checks the white-box number against actual pixels.
//
// The regression this pins: a filter-carrying Duplicate (or any other struct-hard edit) used to zero
// the display intensity on the spot. On a finished run nothing commits afterwards, so the preview
// stayed black until the next Run — and a Revert then reported "Done" over a black preview. The
// zero-ray case pins the other half of the same rule: once a new config that lands no ray at all
// actually runs, the preview does turn black, so an empty result is not mistaken for "not applied".

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "IconsFontAwesome6.h"
#include "gui/server_poller.hpp"
#include "test_gui_shared.hpp"
#include "test_screenshot.hpp"

namespace {

using SimState = gui::GuiState::SimState;
using RunIntent = gui::RunIntent;

const char* const kDupFirstCard = "**/" ICON_FA_COPY "##dup_0_0";

struct ScopedProbeScene {
  ScopedProbeScene() {
    gui::JoinPendingStop();
    ResetTestState();
    gui::g_server = LUMICE_CreateServer();
    gui::ResetServerConstructionTrackers();
  }

  ~ScopedProbeScene() {
    gui::g_server_poller.Stop();
    gui::JoinPendingStop();
    if (gui::g_server != nullptr) {
      LUMICE_StopServer(gui::g_server);
      LUMICE_DestroyServer(gui::g_server);
      gui::g_server = nullptr;
    }
    gui::ResetServerConstructionTrackers();
    gui::g_stop_inflight.store(false);
    gui::g_state.run_intent = RunIntent::kNone;
    gui::g_state.committed_epoch = 0;
    gui::g_state.display_epoch_floor = 0;
    gui::g_state.dirty = false;
  }

  ScopedProbeScene(const ScopedProbeScene&) = delete;
  ScopedProbeScene& operator=(const ScopedProbeScene&) = delete;

  bool ok() const { return gui::g_server != nullptr; }
};

template <typename Pred>
bool DriveUntil(ImGuiTestContext* ctx, Pred pred, int timeout_s) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_s);
  while (!pred()) {
    if (std::chrono::steady_clock::now() > deadline) {
      return false;
    }
    ctx->Yield();
  }
  return true;
}

const char* SimStateName(SimState s) {
  switch (s) {
    case SimState::kIdle:
      return "Idle";
    case SimState::kSimulating:
      return "Simulating";
    case SimState::kDone:
      return "Done";
    case SimState::kModified:
      return "Modified";
    default:
      return "Other";
  }
}

// Fraction of non-black pixels in the preview as the export FBO renders it with THIS frame's
// published viewport params (the same exposure uniforms the on-screen draw uses). -1 on failure.
double PreviewLitFraction(ImGuiTestContext* ctx, const char* tag) {
  gui::PreviewViewport vp = gui::g_preview_vp;
  vp.vp_w = 128;
  vp.vp_h = 128;
  const std::string path = GuiTestTempPath(std::string("lumice_cfg_change_probe_") + tag + ".png").string();
  if (!RequestAndWaitPreviewExport(ctx, vp, path)) {
    return -1.0;
  }
  std::vector<unsigned char> px;
  int w = 0;
  int h = 0;
  int ch = 0;
  if (!lumice::test::LoadPng(path.c_str(), px, w, h, ch) || ch < 3) {
    return -1.0;
  }
  long long lit = 0;
  const long long total = static_cast<long long>(w) * h;
  for (long long i = 0; i < total; ++i) {
    const unsigned char* p = px.data() + i * ch;
    if (p[0] != 0 || p[1] != 0 || p[2] != 0) {
      ++lit;
    }
  }
  std::remove(path.c_str());
  return total > 0 ? static_cast<double>(lit) / static_cast<double>(total) : 0.0;
}

struct Observation {
  int frames = 0;
  int black_frames = 0;
  double first_black_ms = -1.0;
  unsigned long long uploads_delta = 0;
  unsigned long long epoch_at_edit = 0;
  unsigned long long epoch_at_end = 0;
  SimState state_at_end = SimState::kIdle;
  double end_lit = -1.0;
};

// Switches the test harness's copy of the real app's live-edit auto-commit on or off for one scope.
struct ScopedMainLoopCommit {
  explicit ScopedMainLoopCommit(bool on) : prev_(g_enable_main_loop_commit) { g_enable_main_loop_commit = on; }
  ~ScopedMainLoopCommit() { g_enable_main_loop_commit = prev_; }
  ScopedMainLoopCommit(const ScopedMainLoopCommit&) = delete;
  ScopedMainLoopCommit& operator=(const ScopedMainLoopCommit&) = delete;

 private:
  bool prev_;
};

enum class RunStart { kFinished, kRunning };

// Starts a run from the scene `seed` set up, applies `edit`, then watches the preview for
// `window_ms` of wall clock. While running, the live-edit auto-commit of the real app's main loop
// is switched on for the duration (it is what advances the epoch after an edit mid-run); after a
// finished run the real app does not auto-commit, and neither does this.
template <typename Seed, typename Edit>
Observation ObserveEdit(ImGuiTestContext* ctx, RunStart start, Seed seed, Edit edit, double window_ms) {
  Observation obs;
  ScopedProbeScene scene;
  IM_CHECK_RETV(scene.ok(), obs);
  seed();
  gui::g_state.sim.max_hits = 8;
  gui::g_state.renderer.sim_resolution_index = 0;
  const bool running = start == RunStart::kRunning;
  gui::g_state.sim.infinite = running;
  if (!running) {
    gui::g_state.sim.ray_num_millions = 0.25f;
  }
  const ScopedMainLoopCommit auto_commit(running);
  gui::DoRun(/*user_initiated=*/true);
  bool reached = false;
  if (running) {
    reached = WaitForSimRestartAtLeast(ctx, 0, 10000);
    ctx->Yield(10);
  } else {
    reached = DriveUntil(
        ctx,
        [] { return gui::g_state.sim_state == SimState::kDone && gui::g_state.run_intent == RunIntent::kRunCompleted; },
        20);
    ctx->Yield(3);
  }
  IM_CHECK_RETV(reached, obs);
  const bool lit_before = gui::g_preview_vp.params.exposure.intensity_scale > 0.0f;

  IM_CHECK_RETV(lit_before, obs);  // the case must start from a visible picture to prove anything

  const unsigned long long uploads0 = gui::g_state.texture_upload_count;
  obs.epoch_at_edit = gui::g_state.committed_epoch;
  edit(ctx);
  const auto t0 = std::chrono::steady_clock::now();
  const auto Ms = [&t0] {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  };
  while (Ms() < window_ms) {
    ctx->Yield();
    ++obs.frames;
    if (gui::g_preview_vp.params.exposure.intensity_scale == 0.0f) {
      ++obs.black_frames;
      if (obs.first_black_ms < 0) {
        obs.first_black_ms = Ms();
      }
    }
  }
  obs.uploads_delta = gui::g_state.texture_upload_count - uploads0;
  obs.epoch_at_end = gui::g_state.committed_epoch;
  obs.state_at_end = gui::g_state.sim_state;
  obs.end_lit = PreviewLitFraction(ctx, "end");
  fprintf(stderr,
          "[config_change_preview] frames=%d black_frames=%d first_black_ms=%.0f uploads_delta=%llu end_lit=%.3f "
          "state=%s\n",
          obs.frames, obs.black_frames, obs.first_black_ms, obs.uploads_delta, obs.end_lit,
          SimStateName(obs.state_at_end));
  return obs;
}

void AttachFilterToFirstCard() {
  gui::FilterConfig f;
  f.SetRaypath(gui::RaypathParams{ "3-1-5" });
  gui::SetFilter(gui::g_state, gui::g_state.layers[0].entries[0], f);
}

void SetFirstCardFilterPath(const char* path) {
  gui::g_state.filters[static_cast<size_t>(*gui::g_state.layers[0].entries[0].filter_id)].SetRaypath(
      gui::RaypathParams{ path });
}

void ClickDuplicate(ImGuiTestContext* ctx) {
  ctx->ItemClick(kDupFirstCard);
}

// A struct-hard edit on a finished run: the picture stays up for the whole window and the top bar
// says Modified. `edit` differs per case; the verdict does not.
void CheckFinishedRunKeepsPicture(ImGuiTestContext* ctx, void (*edit)(ImGuiTestContext*), SimState expect_state) {
  const Observation obs = ObserveEdit(ctx, RunStart::kFinished, AttachFilterToFirstCard, edit, 800.0);
  IM_CHECK_EQ(obs.black_frames, 0);
  IM_CHECK_GT(obs.end_lit, 0.0);
  IM_CHECK_EQ(static_cast<int>(obs.state_at_end), static_cast<int>(expect_state));
}

}  // namespace

void RegisterConfigChangePreviewTests(ImGuiTestEngine* engine) {
  // The user's report: one crystal carrying a filter, finished run, Duplicate. The clone appends a
  // filter slot, which is a struct-hard edit.
  ImGuiTest* t = IM_REGISTER_TEST(engine, "config_change_preview", "dup_filtered_card_on_done_keeps_picture");
  t->TestFunc = [](ImGuiTestContext* ctx) { CheckFinishedRunKeepsPicture(ctx, ClickDuplicate, SimState::kModified); };

  // The same tier reached without a Duplicate: a filter text edit on a finished run.
  t = IM_REGISTER_TEST(engine, "config_change_preview", "filter_edit_on_done_keeps_picture");
  t->TestFunc = [](ImGuiTestContext* ctx) {
    CheckFinishedRunKeepsPicture(
        ctx,
        [](ImGuiTestContext* c) {
          // SetRaypath, not MutableRaypathText: FilterConfig equality reads the summand text, which
          // only the former rewrites — the edit the editor itself makes.
          SetFirstCardFilterPath("3-1-5-7");
          c->Yield();
        },
        SimState::kModified);
  };

  // Duplicate, then Revert: the state returns to Done, and the picture must be the one Done refers to.
  t = IM_REGISTER_TEST(engine, "config_change_preview", "dup_filtered_card_then_revert_keeps_picture");
  t->TestFunc = [](ImGuiTestContext* ctx) {
    CheckFinishedRunKeepsPicture(
        ctx,
        [](ImGuiTestContext* c) {
          ClickDuplicate(c);
          c->Yield(5);
          c->ItemClick("##TopBar/Revert");
        },
        SimState::kDone);
  };

  // While running, the same Duplicate advances the epoch through the live-edit auto-commit. The old
  // frame stays up until the new generation's first frame replaces it — no black frame in between.
  t = IM_REGISTER_TEST(engine, "config_change_preview", "dup_filtered_card_while_running_has_no_black_frame");
  t->TestFunc = [](ImGuiTestContext* ctx) {
    const Observation obs = ObserveEdit(ctx, RunStart::kRunning, AttachFilterToFirstCard, ClickDuplicate, 1500.0);
    IM_CHECK_EQ(obs.black_frames, 0);
    IM_CHECK_GT(obs.uploads_delta, 0ull);  // the new generation did reach the screen
    IM_CHECK_GT(obs.epoch_at_end, obs.epoch_at_edit);
    IM_CHECK_GT(obs.end_lit, 0.0);
  };

  // The case the old immediate clear existed for: while running, edit the filter to a path longer
  // than max_hits (8), so the new config lands no ray at all. Once it runs, its empty frame reaches
  // the screen (quality-gate timeout fallback, 500 ms) and the preview turns black rather than
  // lingering on the previous config's picture.
  t = IM_REGISTER_TEST(engine, "config_change_preview", "filter_edited_to_zero_rays_while_running_turns_black");
  t->TestFunc = [](ImGuiTestContext* ctx) {
    const Observation obs = ObserveEdit(
        ctx, RunStart::kRunning, AttachFilterToFirstCard,
        [](ImGuiTestContext* c) {
          SetFirstCardFilterPath("3-1-5-7-3-1-5-7-3-1");
          c->Yield();
        },
        2000.0);
    IM_CHECK_GE(obs.first_black_ms, 0.0);
    IM_CHECK_LT(obs.first_black_ms, 1500.0);
    IM_CHECK_EQ(obs.end_lit, 0.0);
  };
}
