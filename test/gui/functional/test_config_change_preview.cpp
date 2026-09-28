// What the preview shows in the frames after the user changes the simulation config.
//
// Stage 1 of this file is an observation probe, not yet a regression gate: each arm runs a real
// simulation, applies one config change through the path the user would take, and then records —
// frame by frame, for a fixed wall-clock window — the quantities that decide what is on screen:
// the shader's mono intensity scale (0 means the shader multiplies every texel by zero, i.e. a
// black preview), the reconcile-derived sim state, the texture upload count, the committed epoch
// and the display epoch floor. A read-back of the preview through the export FBO confirms the
// white-box numbers against actual pixels at the start and at the end of the window.
//
// The printed `[config_change_probe]` lines are the evidence; the few IM_CHECKs only guard that the
// arm actually reached the state it claims to be observing from.

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

void PrintFrame(const char* arm, const char* when, double ms) {
  fprintf(stderr,
          "[config_change_probe] %s %s t=%.0fms state=%s si=%.4g scale=%.4g uploads=%llu epoch=%llu floor=%llu "
          "dirty=%d\n",
          arm, when, ms, SimStateName(gui::g_state.sim_state), gui::g_state.snapshot_intensity,
          gui::g_preview_vp.params.exposure.intensity_scale, gui::g_state.texture_upload_count,
          static_cast<unsigned long long>(gui::g_state.committed_epoch),
          static_cast<unsigned long long>(gui::g_state.display_epoch_floor), gui::g_state.dirty ? 1 : 0);
}

struct ProbeArm {
  const char* name;
  void (*seed)();                   // before the first run
  bool infinite;                    // false: observe from a finished run; true: from a running one
  void (*edit)(ImGuiTestContext*);  // the config change under observation
};

void AttachFilterToFirstCard() {
  gui::FilterConfig f;
  f.SetRaypath(gui::RaypathParams{ "3-1-5" });
  gui::SetFilter(gui::g_state, gui::g_state.layers[0].entries[0], f);
}

void ClickDuplicate(ImGuiTestContext* ctx) {
  ctx->ItemClick(kDupFirstCard);
}

void RunProbe(ImGuiTestContext* ctx, const ProbeArm& arm) {
  ScopedProbeScene scene;
  IM_CHECK(scene.ok());
  arm.seed();
  gui::g_state.sim.max_hits = 8;
  gui::g_state.renderer.sim_resolution_index = 0;
  if (arm.infinite) {
    gui::g_state.sim.infinite = true;
  } else {
    gui::g_state.sim.infinite = false;
    gui::g_state.sim.ray_num_millions = 0.25f;
  }
  gui::DoRun(/*user_initiated=*/true);
  if (arm.infinite) {
    IM_CHECK(WaitForSimRestartAtLeast(ctx, 0, 10000));
    ctx->Yield(10);
  } else {
    IM_CHECK(DriveUntil(
        ctx,
        [] { return gui::g_state.sim_state == SimState::kDone && gui::g_state.run_intent == RunIntent::kRunCompleted; },
        20));
    ctx->Yield(3);
  }
  PrintFrame(arm.name, "before", 0.0);
  fprintf(stderr, "[config_change_probe] %s before lit=%.3f\n", arm.name, PreviewLitFraction(ctx, "before"));

  const unsigned long long uploads0 = gui::g_state.texture_upload_count;
  arm.edit(ctx);

  const auto t0 = std::chrono::steady_clock::now();
  const auto Ms = [&t0] {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  };
  bool prev_black = gui::g_preview_vp.params.exposure.intensity_scale == 0.0f;
  PrintFrame(arm.name, "after-edit", Ms());
  int black_frames = 0;
  int frames = 0;
  double first_black_ms = -1.0;
  double recovered_ms = -1.0;
  while (Ms() < 3000.0) {
    ctx->Yield();
    ++frames;
    const bool black = gui::g_preview_vp.params.exposure.intensity_scale == 0.0f;
    if (black) {
      ++black_frames;
      if (first_black_ms < 0) {
        first_black_ms = Ms();
      }
    }
    if (!black && prev_black && first_black_ms >= 0 && recovered_ms < 0) {
      recovered_ms = Ms();
    }
    if (black != prev_black) {
      PrintFrame(arm.name, black ? "->black" : "->lit", Ms());
    }
    prev_black = black;
  }
  PrintFrame(arm.name, "end", Ms());
  fprintf(stderr,
          "[config_change_probe] %s summary frames=%d black_frames=%d first_black_ms=%.0f recovered_ms=%.0f "
          "uploads_delta=%llu end_lit=%.3f\n",
          arm.name, frames, black_frames, first_black_ms, recovered_ms, gui::g_state.texture_upload_count - uploads0,
          PreviewLitFraction(ctx, "end"));
}

}  // namespace

void RegisterConfigChangePreviewTests(ImGuiTestEngine* engine) {
  // AC1 arm 1: one crystal, no filter, finished run, Duplicate.
  ImGuiTest* t1 = IM_REGISTER_TEST(engine, "config_change_probe", "dup_unfiltered_card_on_done");
  t1->TestFunc = [](ImGuiTestContext* ctx) {
    RunProbe(ctx, ProbeArm{ "dup_nofilter_done", [] {}, false, ClickDuplicate });
  };
  // AC1 arm 2: the user's report — one crystal WITH a filter, finished run, Duplicate.
  ImGuiTest* t2 = IM_REGISTER_TEST(engine, "config_change_probe", "dup_filtered_card_on_done");
  t2->TestFunc = [](ImGuiTestContext* ctx) {
    RunProbe(ctx, ProbeArm{ "dup_filter_done", AttachFilterToFirstCard, false, ClickDuplicate });
  };
  // AC1 arm 3: two crystals, the first filtered, finished run, Duplicate the filtered one.
  ImGuiTest* t3 = IM_REGISTER_TEST(engine, "config_change_probe", "dup_filtered_card_among_two_on_done");
  t3->TestFunc = [](ImGuiTestContext* ctx) {
    RunProbe(ctx, ProbeArm{ "dup_filter_multi_done",
                            [] {
                              AttachFilterToFirstCard();
                              gui::DuplicateEntryBelow(gui::g_state, 0, 0);
                              gui::g_state.layers[0].entries[1].filter_id.reset();
                            },
                            false, ClickDuplicate });
  };
  // AC2 control: a filter text edit (a hard-tier change that is not a Duplicate) on a finished run.
  ImGuiTest* t4 = IM_REGISTER_TEST(engine, "config_change_probe", "filter_text_edit_on_done");
  t4->TestFunc = [](ImGuiTestContext* ctx) {
    RunProbe(ctx, ProbeArm{ "filter_edit_done", AttachFilterToFirstCard, false, [](ImGuiTestContext* c) {
                             // SetRaypath, not MutableRaypathText: FilterConfig equality reads the summand
                             // text, which only the former rewrites — the edit the editor itself makes.
                             gui::g_state.filters[static_cast<size_t>(*gui::g_state.layers[0].entries[0].filter_id)]
                                 .SetRaypath(gui::RaypathParams{ "3-1-5-7" });
                             c->Yield();
                           } });
  };
  // AC2 control: a crystal parameter edit (soft tier) on a finished run.
  ImGuiTest* t5 = IM_REGISTER_TEST(engine, "config_change_probe", "crystal_edit_on_done");
  t5->TestFunc = [](ImGuiTestContext* ctx) {
    RunProbe(ctx, ProbeArm{ "crystal_edit_done", [] {}, false,
                            [](ImGuiTestContext* c) {
                              gui::g_state.crystals[gui::g_state.layers[0].entries[0].crystal_id].height = 2.5f;
                              c->Yield();
                            } });
  };
  // AC1 arm 4: Duplicate a filtered card while an infinite run is still simulating (auto-commit path).
  ImGuiTest* t6 = IM_REGISTER_TEST(engine, "config_change_probe", "dup_filtered_card_while_running");
  t6->TestFunc = [](ImGuiTestContext* ctx) {
    RunProbe(ctx, ProbeArm{ "dup_filter_running", AttachFilterToFirstCard, true, ClickDuplicate });
  };
  // AC2 control: Duplicate a filtered card on a finished run, then press Revert.
  ImGuiTest* t8 = IM_REGISTER_TEST(engine, "config_change_probe", "dup_filtered_card_then_revert_on_done");
  t8->TestFunc = [](ImGuiTestContext* ctx) {
    RunProbe(ctx, ProbeArm{ "dup_filter_revert_done", AttachFilterToFirstCard, false, [](ImGuiTestContext* c) {
                             ClickDuplicate(c);
                             c->Yield(5);
                             c->ItemClick("##TopBar/Revert");
                           } });
  };
  // Same while running, no filter.
  ImGuiTest* t7 = IM_REGISTER_TEST(engine, "config_change_probe", "dup_unfiltered_card_while_running");
  t7->TestFunc = [](ImGuiTestContext* ctx) {
    RunProbe(ctx, ProbeArm{ "dup_nofilter_running", [] {}, true, ClickDuplicate });
  };
}
