// The fixed-dt clock contract, pinned by tests that ride the real harness.
//
// Under --fixed-dt the pool runs at full wall-clock speed while the UI frame clock stays a
// deterministic 1/60 per frame (the engine injects ConfigFixedDeltaTime every frame). The
// engine's built-in watchdog consumed that same injected dt, so its "60 seconds" were simulated
// seconds — roughly 8 real ones at full speed — and it deterministically killed tests whose
// legal waits are bounded in real seconds (the pre-fix verdict_anchor_hover_and_toggle_on_the_map
// red; the harness re-cast landed in 8b7437a0d). The harness bypasses the engine-side protection
// in this mode (queueing with
// ImGuiTestRunFlags_RunFromGui) and re-casts it on the wall clock (FixedDtWatchdog in
// test_gui_main.cpp); every leg of that contract has a case here. All legs are registered only
// under --fixed-dt — the contract they pin does not exist in other modes — and all of them drive
// the harness through ctx->Yield() on the real engine, so no helper is tested unwired.
//
// Green legs (ride the correctness pool):
//   1. fixed_dt_ui_clock_survives_past_the_simulated_watchdog_window — a test crossing the old
//      kill window in simulated time (3700 frames = 61.7 sim-s > the 60 sim-s default) completes,
//      while the UI frame clock it depends on is unchanged (dt == 1/60, ImGui time advances
//      exactly frames/60). Under the pre-fix watchdog this shape is a deterministic red.
//   2. fixed_dt_real_clock_warning_does_not_fail_a_test — with the real-clock warning threshold
//      pulled inside the test's own scope, a real-time crossing neither fails nor aborts, and the
//      warning actually lands in the test's log (the same channel the engine's own warning uses).
//      The live threshold read is what makes per-test guards work.
//   3. fixed_dt_whole_test_budget_resets_for_the_next_test — runs after leg 2 (registration
//      order), which consumed ~3 real seconds: with a 0.1s warning threshold raised at this
//      test's own start, a healthy per-test reset produces a warning crossing ~0.1s in (asserted
//      present in this test's log), while a leaked budget has its crossings already behind it
//      and would never warn. Guard restore itself has no dedicated detection leg: it is
//      guaranteed by RAII on every path (ScopedWatchdogThreshold's destructor), and a leak
//      would surface as warning-line noise in later pool tests while silently disarming their
//      kill baseline — noise, not red.
// Red probes (registered ONLY when the --filter equals the probe's full name — a deliberately
// failing case must never ride a pool run or a substring-filtered enumeration; note the probes
// are fixed-dt-only like every leg, so invoking the documented command without --fixed-dt
// registers zero tests and exits 0 — an expected-red mismatch shaped as "0/0 passed", not as
// the probe failing):
//   4. fixed_dt_watchdog_selfkill_probe — the kill-test leg. Expect the harness watchdog's
//      "[Watchdog] ... aborting." line, Output.Status = Error and a non-zero exit:
//      gui_test --fixed-dt --filter fixed_dt_watchdog_selfkill_probe --no-user-config
//   5. fixed_dt_watchdog_killapp_probe — the kill-app leg (engine parity: threshold+5s then
//      emergency exit(1)). Expect the "[Watchdog] Emergency process exit" line and the process
//      ending with exit code 1 before the loop completes:
//      gui_test --fixed-dt --filter fixed_dt_watchdog_killapp_probe --no-user-config
//   6. fixed_dt_watchdog_double_cross_probe — the crossing matrix's same-tick cell (warning and
//      kill-test crossed by one tick both emit their lines):
//      gui_test --fixed-dt --filter fixed_dt_watchdog_double_cross_probe --no-user-config

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

double RealSecondsSince(const std::chrono::steady_clock::time_point& since) {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - since).count();
}

bool FilterEqualsProbeName(const char* filter, const char* probe_name) {
  if (filter == nullptr) {
    return false;
  }
  // Exact full-name equality, case-insensitive (the engine's own matching is case-insensitive).
  // Deliberately NOT a substring test: a probe is a standalone tool named in full, and a wide
  // gate would pull a deliberately-failing case into ordinary filtered runs (e.g. --filter
  // fixed_dt enumerating the green legs).
  const auto lower = [](std::string s) {
    for (char& c : s) {
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
  };
  return lower(filter) == lower(probe_name);
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
  // The sim/real decoupling itself is machine-speed dependent by nature, so the wall-clock side
  // is evidence, not a pinned assertion: a wall-clock bound in a pool-riding case would be a
  // load-sensitive false-red vector (the wall-clock-flake family). The sim-time asserts above
  // carry the deterministic contract; the ratio below documents the decoupling.
  ctx->LogInfo("fixed-dt clock: %d frames = %.2f sim-s in %.2f real-s", kFramesPastDefaultKill, sim_elapsed,
               RealSecondsSince(real_before));
}

// ---- Leg 2: a real-clock warning crossing does not fail or abort the test ----

void RealClockWarningDoesNotFailTheTest(ImGuiTestContext* ctx) {
  // The harness watchdog reads the threshold fields every tick, so a value set from inside the
  // test takes effect mid-run — the exact mechanism per-test guards rely on.
  ScopedWatchdogThreshold guard(ctx->EngineIO, /*warn_s=*/1.0f, /*kill_s=*/FLT_MAX);
  const auto real_before = std::chrono::steady_clock::now();
  // Real-time driven loop: the warning crossing (1s real) fires ~1s in and the test rides
  // through it. The loop runs 3s deliberately so leg 3 behind this one can detect budget leaks
  // by inheriting this test's consumed wall time. The safety here is the guard's kill_s=FLT_MAX:
  // budgets are real-clock seconds, and the sim clock is not a safety quantity at all (at full
  // speed it races tens of sim-seconds per real second — no watchdog comparison reads it).
  while (RealSecondsSince(real_before) < 3.0) {
    ctx->Yield();
  }
  IM_CHECK_EQ(ctx->Test->Output.Status, ImGuiTestStatus_Running);
  // Bidirectional: the warning must actually have fired into the test's log (the same channel
  // the engine's own warning uses). If the warning branch regresses to never firing, this reds
  // — the leg no longer passes vacuously.
  IM_CHECK(strstr(ctx->Test->Output.Log.Buffer.c_str(), "[Watchdog]") != nullptr);
}

// ---- Leg 3: the whole-test budget resets between consecutive tests ----

void WholeTestBudgetResetsForTheNextTest(ImGuiTestContext* ctx) {
  // This leg runs right after leg 2 in registration order, and leg 2 consumed ~3 real seconds.
  // The watchdog compares crossings (previous tick's elapsed < threshold, current elapsed at or
  // past it), so a budget that leaked across the test boundary would have its crossings already
  // behind it and would never fire again — the reset therefore cannot be detected with a kill.
  // It is detected through a warning crossing instead: with a 0.1s warning threshold raised at
  // this test's own start, a healthy per-test reset puts the elapsed back to zero and the
  // warning fires ~0.1s in (logged into this test's own log, engine-parity channel); a leaked
  // budget inherits ~3s of elapsed, the 0.1s crossing was missed, the warning never appears and
  // the presence check below reds. The loop stays well under the untouched 60s kill window.
  ScopedWatchdogThreshold guard(ctx->EngineIO, /*warn_s=*/0.1f, /*kill_s=*/FLT_MAX);
  const auto real_before = std::chrono::steady_clock::now();
  while (RealSecondsSince(real_before) < 0.5) {
    ctx->Yield();
  }
  IM_CHECK_EQ(ctx->Test->Output.Status, ImGuiTestStatus_Running);
  IM_CHECK(strstr(ctx->Test->Output.Log.Buffer.c_str(), "[Watchdog]") != nullptr);
}

// ---- Leg 4 (red probe): the real-clock kill-test marks the test Error and exits non-zero ----

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

// ---- Leg 5 (red probe): the kill-app emergency exit ends the process ----

void WatchdogKillAppProbe(ImGuiTestContext* ctx) {
  // Deliberately fatal — see the file header. Kill-app carries the engine's +5s arming delay and
  // ends the process with exit(1) (engine parity), so this probe's contract is purely external:
  // the "[Watchdog] Emergency process exit" line and a non-zero process exit while the loop is
  // still running. The loop below would need 7s; the exit lands at ~5s. No guard around the
  // threshold write: the process never reaches a restore point.
  ctx->EngineIO->ConfigWatchdogKillApp = 0.0f;
  const auto real_before = std::chrono::steady_clock::now();
  while (RealSecondsSince(real_before) < 7.0) {
    ctx->Yield();
  }
  // Bidirectional: if the kill-app branch ever regresses to never firing, the loop finishes with
  // Running status and this assert reds on its own (the healthy path exits the process ~5s in
  // and never reaches this line).
  IM_CHECK_EQ(ctx->Test->Output.Status, ImGuiTestStatus_Error);
}

// ---- Leg 6 (red probe): one tick crossing warning and kill-test together ----

void WatchdogDoubleCrossProbe(ImGuiTestContext* ctx) {
  // Deliberately red — the crossing matrix's same-tick cell: with both thresholds at 1s, the
  // first tick past 1s crosses warning AND kill-test together (two independent branches, one
  // tick). Both halves assert in-probe: the warning line via the strstr below, the kill half
  // via the Error status after it — without the strstr a degenerate warning branch would still
  // red through the kill, and the same-tick cell would silently lose the warning half of its
  // automatic verdict. External contract on top: the log carries BOTH the "[Watchdog] ... may
  // be excessive." line and the "[Watchdog] ... aborting." line, and the process exits non-zero.
  ScopedWatchdogThreshold guard(ctx->EngineIO, /*warn_s=*/1.0f, /*kill_s=*/1.0f);
  const auto real_before = std::chrono::steady_clock::now();
  while (RealSecondsSince(real_before) < 2.0) {
    ctx->Yield();
  }
  IM_CHECK(strstr(ctx->Test->Output.Log.Buffer.c_str(), "may be excessive") != nullptr);
  IM_CHECK_EQ(ctx->Test->Output.Status, ImGuiTestStatus_Error);
}

}  // namespace

void RegisterFixedDtClockContractTests(ImGuiTestEngine* engine, const char* filter, bool fixed_dt_mode) {
  if (!fixed_dt_mode) {
    // The legs pin the fixed-dt clock contract; their assertions (dt == 1/60, sim-time windows,
    // harness watchdog legs) do not exist in other modes, and a non-fixed-dt full run would red
    // them as strangers rather than as regressions.
    return;
  }
  ImGuiTest* t =
      IM_REGISTER_TEST(engine, "harness_contract", "fixed_dt_ui_clock_survives_past_the_simulated_watchdog_window");
  t->TestFunc = UiClockSurvivesPastSimulatedWatchdogWindow;
  t = IM_REGISTER_TEST(engine, "harness_contract", "fixed_dt_real_clock_warning_does_not_fail_a_test");
  t->TestFunc = RealClockWarningDoesNotFailTheTest;
  t = IM_REGISTER_TEST(engine, "harness_contract", "fixed_dt_whole_test_budget_resets_for_the_next_test");
  t->TestFunc = WholeTestBudgetResetsForTheNextTest;
  // The probes exist only under a filter naming them in full: a deliberately-failing case must
  // never ride a pool run or a substring-filtered enumeration (expected-red probes are
  // standalone, outer-captured).
  if (FilterEqualsProbeName(filter, "fixed_dt_watchdog_selfkill_probe")) {
    t = IM_REGISTER_TEST(engine, "harness_contract", "fixed_dt_watchdog_selfkill_probe");
    t->TestFunc = WatchdogSelfKillProbe;
  }
  if (FilterEqualsProbeName(filter, "fixed_dt_watchdog_killapp_probe")) {
    t = IM_REGISTER_TEST(engine, "harness_contract", "fixed_dt_watchdog_killapp_probe");
    t->TestFunc = WatchdogKillAppProbe;
  }
  if (FilterEqualsProbeName(filter, "fixed_dt_watchdog_double_cross_probe")) {
    t = IM_REGISTER_TEST(engine, "harness_contract", "fixed_dt_watchdog_double_cross_probe");
    t->TestFunc = WatchdogDoubleCrossProbe;
  }
}
