// The fixed-dt clock contract, pinned by tests that ride the real harness.
//
// Under --fixed-dt the pool runs at full wall-clock speed while the UI frame clock stays a
// deterministic 1/60 per frame (the engine injects ConfigFixedDeltaTime every frame). The
// engine's built-in watchdog consumed that same injected dt, so its "60 seconds" were simulated
// seconds — roughly 8 real ones at full speed — and it deterministically killed tests whose
// legal waits are bounded in real seconds (verdict_anchor_hover_and_toggle_on_the_map, 785.6
// evidence). The harness now neutralizes the engine-side protection in this mode and re-casts it
// on the wall clock (FixedDtWatchdog in test_gui_main.cpp); these cases pin the three legs of
// that contract:
//
//   1. fixed_dt_ui_clock_survives_past_the_simulated_watchdog_window — a test crossing the old
//      kill window in simulated time (3700 frames = 61.7 sim-s > the 60 sim-s default) completes,
//      while the UI frame clock it depends on is unchanged (dt == 1/60, ImGui time advances
//      exactly frames/60). Under the pre-fix watchdog this shape is a deterministic red.
//   2. fixed_dt_real_clock_warning_does_not_fail_a_test — with the real-clock warning threshold
//      pulled inside the test's own scope, a real-time crossing neither fails nor aborts: the
//      warning is a log line, and the live threshold read is what makes per-test guards work.
//   3. fixed_dt_watchdog_selfkill_probe — the kill leg, registered ONLY when the --filter names
//      it (a deliberately-failing case must never ride a pool run). Expect the harness watchdog's
//      "[Watchdog] ... aborting." line, Output.Status = Error and a non-zero exit:
//      gui_test --fixed-dt --filter fixed_dt_watchdog_selfkill_probe --no-user-config
//
// All three drive the harness through ctx->Yield() on the real engine — no helper is tested
// unwired.

#include <cctype>
#include <cfloat>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <string>

#include "imgui.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "test_gui_shared.hpp"

namespace {

// Self-scoped threshold override with restore on every path, shaped like the per-test guards in
// the analysis-panel/lens/parity suites (their semantics: values are real-clock seconds, honored
// live by FixedDtWatchdog, restored to whatever baseline the harness set).
struct ScopedWatchdogThreshold {
  ImGuiTestEngineIO* io;
  float saved_warn;
  float saved_kill;

  ScopedWatchdogThreshold(ImGuiTestEngineIO* engine_io, float warn_s, float kill_s)
      : io(engine_io), saved_warn(engine_io->ConfigWatchdogWarning), saved_kill(engine_io->ConfigWatchdogKillTest) {
    io->ConfigWatchdogWarning = warn_s;
    io->ConfigWatchdogKillTest = kill_s;
  }
  ~ScopedWatchdogThreshold() {
    io->ConfigWatchdogWarning = saved_warn;
    io->ConfigWatchdogKillTest = saved_kill;
  }
};

double RealSecondsSince(const std::chrono::steady_clock::time_point& since) {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - since).count();
}

// ---- Leg 1: simulated time past the old kill window, UI frame clock unchanged ----

void UiClockSurvivesPastSimulatedWatchdogWindow(ImGuiTestContext* ctx) {
  const double sim_time_before = ImGui::GetTime();
  const auto real_before = std::chrono::steady_clock::now();
  // 61.7 simulated seconds — past the engine's 60 sim-s default kill-test window that
  // deterministically killed verdict_anchor before the harness clock fix.
  constexpr int kFramesPastDefaultKill = 3700;
  ctx->Yield(kFramesPastDefaultKill);
  // Still alive: a watchdog kill would have marked Error through the engine's own check path
  // (and, per engine parity, this TestFunc keeps running — so the status read is what makes a
  // future kill red at this line instead of only at batch end).
  IM_CHECK_EQ(ctx->Test->Output.Status, ImGuiTestStatus_Running);
  // The UI frame clock is untouched by the watchdog change: the injected dt is still the exact
  // float the harness configured, and ImGui time advanced exactly one dt per frame.
  IM_CHECK_EQ(ImGui::GetIO().DeltaTime, 1.0f / 60.0f);
  const double sim_elapsed = ImGui::GetTime() - sim_time_before;
  const double sim_expected = static_cast<double>(kFramesPastDefaultKill) / 60.0;
  IM_CHECK(sim_elapsed > sim_expected - 0.05 && sim_elapsed < sim_expected + 0.05);
  // Wall clock must stay far below the kill budget on any machine the pool is usable on at all:
  // this is the decoupling the fix exists for (the old watchdog crossed here in ~6 real seconds).
  const double real_elapsed = RealSecondsSince(real_before);
  IM_CHECK_LT(real_elapsed, 60.0);
  ctx->LogInfo("fixed-dt clock: %d frames = %.2f sim-s in %.2f real-s", kFramesPastDefaultKill, sim_elapsed,
               real_elapsed);
}

// ---- Leg 2: a real-clock warning crossing does not fail or abort the test ----

void RealClockWarningDoesNotFailTheTest(ImGuiTestContext* ctx) {
  // The harness watchdog reads the threshold fields every tick, so a value set from inside the
  // test takes effect mid-run — the exact mechanism per-test guards rely on.
  ScopedWatchdogThreshold guard(ctx->EngineIO, /*warn_s=*/1.0f, /*kill_s=*/FLT_MAX);
  const auto real_before = std::chrono::steady_clock::now();
  // Real-time driven loop: the warning crossing (1s real) lands while simulated time stays
  // ~1.2 sim-s — far under any kill window. The test must ride through the crossing and finish.
  while (RealSecondsSince(real_before) < 1.4) {
    ctx->Yield();
  }
  IM_CHECK_EQ(ctx->Test->Output.Status, ImGuiTestStatus_Running);
}

// ---- Leg 3 (red probe): the real-clock kill marks the test Error and exits non-zero ----

void WatchdogSelfKillProbe(ImGuiTestContext* ctx) {
  // Deliberately red — see the file header. The kill crosses while the loop yields; per engine
  // parity the TestFunc keeps executing afterwards, so reaching the final check with Error
  // status is the documented post-kill state. If the watchdog were broken the other way (never
  // fires), the loop would finish with Running status and this check would red on its own —
  // the probe fails in both directions, never silently.
  ScopedWatchdogThreshold guard(ctx->EngineIO, /*warn_s=*/FLT_MAX, /*kill_s=*/1.0f);
  const auto real_before = std::chrono::steady_clock::now();
  while (RealSecondsSince(real_before) < 2.0) {
    ctx->Yield();
  }
  IM_CHECK_EQ(ctx->Test->Output.Status, ImGuiTestStatus_Error);
}

bool FilterNamesProbe(const char* filter, const char* probe_name) {
  if (filter == nullptr) {
    return false;
  }
  // Case-insensitive substring, matching the engine's filter semantics (name OR category).
  const std::string haystack = filter;
  const std::string needle = probe_name;
  const auto lower = [](std::string s) {
    for (char& c : s) {
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
  };
  return lower(haystack).find(lower(needle)) != std::string::npos;
}

}  // namespace

void RegisterFixedDtClockContractTests(ImGuiTestEngine* engine, const char* filter) {
  ImGuiTest* t =
      IM_REGISTER_TEST(engine, "harness_contract", "fixed_dt_ui_clock_survives_past_the_simulated_watchdog_window");
  t->TestFunc = UiClockSurvivesPastSimulatedWatchdogWindow;
  t = IM_REGISTER_TEST(engine, "harness_contract", "fixed_dt_real_clock_warning_does_not_fail_a_test");
  t->TestFunc = RealClockWarningDoesNotFailTheTest;
  // The kill probe exists only under an explicit filter naming it: a deliberately-failing case
  // must never ride a pool run (plan §6.2 — expected-red probes are standalone, outer-captured).
  if (FilterNamesProbe(filter, "fixed_dt_watchdog_selfkill_probe")) {
    t = IM_REGISTER_TEST(engine, "harness_contract", "fixed_dt_watchdog_selfkill_probe");
    t->TestFunc = WatchdogSelfKillProbe;
  }
}
