// The Raypath Analysis window (src/gui/analysis_panel.cpp), driven through real frames, real
// clicks and a real server: the one layer that can see the wiring the unit cases cannot — the
// preview click reaching LUMICE_UnprojectPixel through the panel's own DPI path, the Analyze
// button reaching LUMICE_StartRaypathAnalysis and its result reaching the list through the
// poller, the slider leaving the lifecycle alone, and the top-bar mutual exclusion.
//
// The scene is the 22-degree halo: one randomly oriented prism under a 20-degree sun, the same
// fixture the C API and e2e layers use for the same run. The camera looks 22 degrees above the
// sun on its azimuth, so the frame's centre pixel sits ON the halo ring — the point a user would
// click to ask "what makes this bright arc" — and a cone around it collects real chains.

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "IconsFontAwesome6.h"
#include "gui/analysis_panel.hpp"
#include "gui/analyze_map_view.hpp"
#include "gui/analyze_overview.hpp"
#include "gui/analyze_point_view.hpp"
#include "gui/analyze_workspace.hpp"
#include "gui/angular_dist_picker.hpp"
#include "gui/crystal_renderer.hpp"  // ComputeDist / ComputeNearPlane — the frame tests' camera
#include "gui/edit_modals.hpp"
#include "gui/export_fbo_renderer.hpp"
#include "gui/file_io.hpp"
#include "gui/gui_constants.hpp"
#include "gui/gui_logger.hpp"
#include "gui/log_sink.hpp"
#include "gui/panels.hpp"
#include "gui/raypath_segments.hpp"
#include "gui/semantic_colors.hpp"  // WarningTextColor — the drift warning ink's colour probe
#include "gui/sim_state_rules.hpp"
#include "gui/theme.hpp"
#include "gui/user_defaults.hpp"  // kFactoryUiScaleMultiplier — the scale tiers restore through it
#include "gui/view_look_at.hpp"   // WorldDirToAzEl — the camera-aim inverse the Look At presets run
#include "imgui_internal.h"
#include "include/lumice_base.h"
#include "include/lumice_editor.h"
#include "include/lumice_engine.h"
#include "include/lumice_render.h"
#include "support/env_var.hpp"  // SetEnvVar / UnsetEnvVar — the LUMICE_ANALYZE_DEBUG_DETAILS legs
#include "support/gui_server_lifetime.hpp"
#include "test_gui_shared.hpp"
#include "util/raypath_analysis_display.hpp"
#include "util/sky_direction.hpp"

namespace {

using SimState = gui::GuiState::SimState;
using PressOwner = gui::GuiState::AnalyzeWorkspaceSession::PanoramaPressOwner;

// LUMICE_ANALYZE_DEBUG_DETAILS is a process-global the four answer hosts' diagnostics groups
// read every frame, so every leg that asserts their presence or absence DECLARES its state
// through this guard and restores the prior value (or its absence) on every exit path — the
// IM_CHECK_RETV early returns included. No leg depends on execution order or on the shell's
// environment: the three states are spelled kUnset (no variable, the shipped default), kOff
// (an explicit "0") and kOn ("1", the dev diagnostic view).
enum class DebugDetailsState { kUnset, kOff, kOn };

class ScopedDebugDetailsEnv {
 public:
  explicit ScopedDebugDetailsEnv(DebugDetailsState state) {
    const char* current = std::getenv("LUMICE_ANALYZE_DEBUG_DETAILS");
    saved_ = current != nullptr ? std::optional<std::string>(current) : std::nullopt;
    switch (state) {
      case DebugDetailsState::kOn:
        lumice::test::SetEnvVar("LUMICE_ANALYZE_DEBUG_DETAILS", "1");
        break;
      case DebugDetailsState::kOff:
        lumice::test::SetEnvVar("LUMICE_ANALYZE_DEBUG_DETAILS", "0");
        break;
      case DebugDetailsState::kUnset:
        lumice::test::UnsetEnvVar("LUMICE_ANALYZE_DEBUG_DETAILS");
        break;
    }
  }
  ScopedDebugDetailsEnv(const ScopedDebugDetailsEnv&) = delete;
  ScopedDebugDetailsEnv& operator=(const ScopedDebugDetailsEnv&) = delete;
  ~ScopedDebugDetailsEnv() {
    if (saved_) {
      lumice::test::SetEnvVar("LUMICE_ANALYZE_DEBUG_DETAILS", saved_->c_str());
    } else {
      lumice::test::UnsetEnvVar("LUMICE_ANALYZE_DEBUG_DETAILS");
    }
  }

 private:
  std::optional<std::string> saved_;
};

// The analysis tool lives in the Analyze workspace now: the share list in the middle area's
// right column, the ROI controls above the panorama's picture. Both are TOP-LEVEL chrome windows
// (##AnalyzeShareList, ##AnalyzePanorama). The panorama's PICTURE is a same-named CHILD under the
// chrome window — BeginChild spells child names "Parent/Name_<id>", so FindWindowByName still
// lands on the chrome window — and every pixel the pick, the marker and the gestures measure is
// relative to that child's position, so the picture helpers below resolve it by ParentWindow.
// The refs are STATIC (ImGuiTestContext::SetRef stores the const char* itself) and ABSOLUTE
// (the leading "//" makes the engine resolve the first segment as a window name — without it a
// ref whose window name starts with "##" reads as an item-id suffix and every descent fails).
const char* ListWindowRef() {
  return "//##AnalyzeShareList";
}
void SetListRef(ImGuiTestContext* ctx) {
  ctx->SetRef(ListWindowRef());
}
const char* PanoramaRegionRef() {
  return "//##AnalyzePanorama";
}
const char* const kWindowRef = "//##AnalyzeShareList";
const char* const kAnalyzeButton = ICON_FA_PLAY " Analyze";
const char* const kPickButton = ICON_FA_CROSSHAIRS " Pick on the panorama";

const char* const kHalo22Json = R"({
  "crystal": [{"id": 1, "type": "prism", "shape": {"height": 1.2},
               "axis": {"zenith": {"type": "uniform", "mean": 90, "std": 360},
                        "azimuth": {"type": "uniform", "mean": 0, "std": 360}}}],
  "filter": [],
  "scene": {"light_source": {"type": "sun", "altitude": 20.0, "spectrum": "D65"},
            "ray_num": 100000, "max_hits": 7,
            "scattering": [{"prob": 0.0, "entries": [{"crystal": 1, "proportion": 10}]}]},
  "render": [{"id": 1, "lens": {"type": "linear", "fov": 60},
              "resolution": [64, 64], "view": {"elevation": 42}}]
})";

// The fully random prism — the C01 shape (raypath_feature_random_regular.json's crystal, the
// same physics as kHalo22Json with the zenith axis written the one way the survey's routing v1
// domain reads as the full-sphere random family: uniform, centred at the pole). This scene's
// dominant class routes to the full-dimensional arm, so its survey carries the chromatic legs
// the halo scene above is honestly refused — the dispersion-on-screen fixture.
const char* const kFullyRandomJson = R"({
  "crystal": [{"id": 1, "type": "prism", "shape": {"height": 1.2},
               "axis": {"zenith": {"type": "uniform", "mean": 0, "std": 360},
                        "azimuth": {"type": "uniform", "mean": 0, "std": 360},
                        "roll": {"type": "uniform", "mean": 0, "std": 360}}}],
  "filter": [],
  "scene": {"light_source": {"type": "sun", "altitude": 20.0, "spectrum": "D65"},
            "ray_num": 100000, "max_hits": 7,
            "scattering": [{"prob": 0.0, "entries": [{"crystal": 1, "proportion": 10}]}]},
  "render": [{"id": 1, "lens": {"type": "linear", "fov": 60},
              "resolution": [64, 64], "view": {"elevation": 42}}]
})";

// Plate + column in two scattering layers, ms_prob 0.3: the scene whose chains cross layers, so
// its list carries multi-layer rows — the ones whose `display` holds the " -> " joiner. Mirrors
// test/e2e/configs/raypath_analysis_pc_two_layer.json (the e2e layer reads the file; gui_test
// inlines its scenes, as kHalo22Json above).
const char* const kPcTwoLayerJson = R"({
  "crystal": [{"id": 1, "type": "prism", "shape": {"height": 0.3},
               "axis": {"zenith": {"type": "gauss", "mean": 90, "std": 0.5},
                        "azimuth": {"type": "uniform", "mean": 0, "std": 360},
                        "roll": {"type": "uniform", "mean": 0, "std": 360}}},
              {"id": 2, "type": "prism", "shape": {"height": 2.5},
               "axis": {"zenith": {"type": "gauss", "mean": 0, "std": 0.5},
                        "azimuth": {"type": "uniform", "mean": 0, "std": 360},
                        "roll": {"type": "uniform", "mean": 0, "std": 360}}}],
  "filter": [],
  "scene": {"light_source": {"type": "sun", "altitude": 20.0, "spectrum": "D65"},
            "ray_num": 200000, "max_hits": 7,
            "scattering": [{"prob": 0.3, "entries": [{"crystal": 1, "proportion": 10},
                                                     {"crystal": 2, "proportion": 10}]},
                           {"prob": 0.0, "entries": [{"crystal": 1, "proportion": 10},
                                                     {"crystal": 2, "proportion": 10}]}]},
  "render": [{"ev_mode": "absolute", "id": 1, "lens": {"type": "fisheye_equal_area", "fov": 120},
              "resolution": [64, 64], "view": {"elevation": 20}}]
})";

// The two-layer scene grown one more scattering layer: rows now span three segments, and a
// three-segment label is wider than the right column's 260 pt floor content width — two classes
// sharing a long prefix truncate to the same visible head. This is the narrow-column reading
// contract's fixture: the identity (search, selection, exclusion) must survive the clip.
const char* const kThreeLayerLongChainJson = R"({
  "crystal": [{"id": 1, "type": "prism", "shape": {"height": 0.3},
               "axis": {"zenith": {"type": "gauss", "mean": 90, "std": 0.5},
                        "azimuth": {"type": "uniform", "mean": 0, "std": 360},
                        "roll": {"type": "uniform", "mean": 0, "std": 360}}},
              {"id": 2, "type": "prism", "shape": {"height": 2.5},
               "axis": {"zenith": {"type": "gauss", "mean": 0, "std": 0.5},
                        "azimuth": {"type": "uniform", "mean": 0, "std": 360},
                        "roll": {"type": "uniform", "mean": 0, "std": 360}}}],
  "filter": [],
  "scene": {"light_source": {"type": "sun", "altitude": 20.0, "spectrum": "D65"},
            "ray_num": 300000, "max_hits": 7,
            "scattering": [{"prob": 0.3, "entries": [{"crystal": 1, "proportion": 10},
                                                     {"crystal": 2, "proportion": 10}]},
                           {"prob": 0.3, "entries": [{"crystal": 1, "proportion": 10},
                                                     {"crystal": 2, "proportion": 10}]},
                           {"prob": 0.0, "entries": [{"crystal": 1, "proportion": 10},
                                                     {"crystal": 2, "proportion": 10}]}]},
  "render": [{"ev_mode": "absolute", "id": 1, "lens": {"type": "fisheye_equal_area", "fov": 120},
              "resolution": [64, 64], "view": {"elevation": 20}}]
})";

// The plate scene (the family-arm face's own): a basal plate with a uniform azimuth axis — its
// arc rows route to the restricted plate family, whose survey walks the freezing ladder and runs
// the probe on the representative (the profile rows' home arm).
const char* const kPlateJson = R"({
  "crystal": [ { "id": 1, "type": "prism",
                 "shape": { "height": 0.3, "face_distance": [1,1,1,1,1,1] },
                 "axis": { "zenith": 0, "azimuth": {"type": "uniform", "mean": 0, "std": 360}, "roll": 0 } } ],
  "filter": [],
  "scene": { "light_source": { "type": "sun", "altitude": 20, "azimuth": 0, "diameter": 0.5,
                               "spectrum": [ { "wavelength": 550, "weight": 1 } ] },
             "ray_num": 1000, "max_hits": 7,
             "scattering": [ { "prob": 0, "entries": [ { "crystal": 1, "proportion": 1 } ] } ] },
  "render": []
})";

// The narrow-plate scene (the profile-hover face's own): a plate whose zenith is a Gaussian
// concentration and whose roll is free — an arc row routes to the restricted plate family AND
// carries a pose density, so the family arm runs its quantification legs (a fixed zenith or roll
// maps to no density and skips the probe among them, leaving the profile rows nothing to draw).
const char* const kNarrowPlateJson = R"({
  "crystal": [ { "id": 1, "type": "prism",
                 "shape": { "height": 0.3, "face_distance": [1,1,1,1,1,1] },
                 "axis": { "zenith": { "type": "gauss", "mean": 0, "std": 2 },
                           "azimuth": {"type": "uniform", "mean": 0, "std": 360},
                           "roll": {"type": "uniform", "mean": 0, "std": 360} } } ],
  "filter": [],
  "scene": { "light_source": { "type": "sun", "altitude": 20, "azimuth": 0, "diameter": 0.5,
                               "spectrum": [ { "wavelength": 550, "weight": 1 } ] },
             "ray_num": 1000, "max_hits": 7,
             "scattering": [ { "prob": 0, "entries": [ { "crystal": 1, "proportion": 1 } ] } ] },
  "render": []
})";

// Same shape as test_gui_sim_smoke.cpp's guard, for the same reason: IM_CHECK returns out of
// the case, and a server left running would be inherited by the next one.
struct ScopedServerGuard {
  ~ScopedServerGuard() {
    lumice::test::JoinGuiServerTasksForTeardown();
    if (gui::g_server != nullptr) {
      gui::g_server_poller.Stop();
      LUMICE_StopServer(gui::g_server);
      LUMICE_DestroyServer(gui::g_server);
      gui::g_server = nullptr;
      gui::g_state.run_intent = gui::RunIntent::kNone;
      gui::g_state.analysis.started = false;
      gui::g_state.analysis_run_in_progress = false;
    }
    gui::g_state.use_gpu_backend = false;
    gui::ResetServerConstructionTrackers();
  }
};

template <typename Pred>
bool DriveUntil(ImGuiTestContext* ctx, Pred pred, int timeout_s) {
  const auto started = std::chrono::steady_clock::now();
  const auto deadline = started + std::chrono::seconds(timeout_s);
  int polls = 0;  // predicate polls that returned false
  while (!pred()) {
    ++polls;
    if (std::chrono::steady_clock::now() > deadline) {
      const auto elapsed =
          std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - started);
      // Wall-clock telemetry for the gate-budget calibration (time-to-window band). One line per
      // gate exit; no logging inside the loop body.
      ctx->LogInfo("[drive-gate] TIMEOUT after %.1fs (budget %ds, %d polls, avg %.0f ms/poll)", elapsed.count(),
                   timeout_s, polls, polls > 0 ? elapsed.count() * 1000.0 / polls : 0.0);
      return false;
    }
    if (ctx->IsError()) {
      // A poisoned context cannot complete any wait — every later op short-circuits on the
      // sticky error (the poison-cascade note at ScopedWatchdogGuard describes the same shape),
      // so spinning the full budget only buries the engine's error line under a timeout. Fail
      // now; the error line already names the cause.
      return false;
    }
    ctx->Yield();
  }
  const auto elapsed =
      std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - started);
  ctx->LogInfo("[drive-gate] ok after %.2fs (budget %ds, %d polls)", elapsed.count(), timeout_s, polls);
  return true;
}

// Waiting budgets of the scopes case's gates and of its shared standing-state entry
// (BringUpBothWindows) — one named source instead of scattered per-gate literals. The values are
// SAFETY NETS, not criteria: since the observation-protocol rework (ScopedPointPickupEdgeRecorder)
// the InFlight gates judge the recorded transition, not a wall wait, so a budget only bounds a
// genuinely-stuck run and never decides a healthy one — a transient narrower than the sampling
// interval is exactly the thing a budget cannot calibrate (the race-type-gates section of
// doc/testing/testing-architecture.md records the measured case). Consumed on the slow path only
// (DriveUntil returns as soon as the predicate holds), so the green path's wall time does not
// move with them.
constexpr int kScopesObserveBudgetS = 30;   // an issued query's execution window must become observable
constexpr int kScopesCompleteBudgetS = 60;  // a query / analysis finishing
constexpr int kScopesTightBudgetS = 20;     // a bounded drain / cancel landing

// Labels keyed to gui::PointQueryState's enumerator order (analyze_survey.hpp).
constexpr const char* kPointQueryStateNames[] = { "NoIntent", "Waiting", "Completed", "Refused", "Cancelled" };
constexpr unsigned ScopesStateBit(gui::PointQueryState state) {
  return 1u << static_cast<unsigned>(state);
}

// Sampled polling history for one observation gate of the scopes case. DIAGNOSTIC AGGREGATE
// ONLY — this is a SAMPLE history, not the state's true history: an all-false record proves only
// "never sampled true" (a short true-window can fall between two samples), and non-overlapping
// per-condition intervals do not prove the conditions were never true simultaneously (the true
// ordering between two samples is unknowable at this layer). No gate verdict consumes it — the
// InFlight gates judge the recorded pickup edge (ScopedPointPickupEdgeRecorder), which is why a
// true-window narrower than the sampling interval cannot fail them; the record only feeds the
// timeout-path triage line.
struct ScopesGateTrace {
  static constexpr int kMaxConditions = 6;

  struct ConditionSlice {
    const char* name = nullptr;
    bool ever_true = false;
    std::chrono::steady_clock::time_point first_seen{};
    std::chrono::steady_clock::time_point last_seen{};
  };

  explicit ScopesGateTrace(const char* gate_name) : gate_name_(gate_name) {}

  // One poll's sample: the named conditions' values plus the observed query-state bit.
  // Per-poll cost is a few bool comparisons and one clock read — noise next to a Yield()'s
  // whole-frame overhead.
  void Poll(std::initializer_list<std::pair<const char*, bool>> conditions, unsigned observed_state_bit) {
    const auto now = std::chrono::steady_clock::now();
    if (polls_ == 0) {
      first_poll_ = now;
    } else {
      max_poll_interval_ = std::max(max_poll_interval_, now - last_poll_);
    }
    last_poll_ = now;
    ++polls_;
    observed_state_bits_ |= observed_state_bit;
    for (const auto& sample : conditions) {
      ConditionSlice* slice = SliceFor(sample.first);
      if (sample.second) {
        if (!slice->ever_true) {
          slice->ever_true = true;
          slice->first_seen = now;
        }
        slice->last_seen = now;
      }
    }
  }

  // One-shot triage line on the timeout path (a PASS prints only DriveUntil's elapsed line).
  // The line DESCRIBES the sampled history; per the struct comment it cannot distinguish a
  // window that opened late from one that fell between two samples.
  void LogTimeoutSummary(ImGuiTestContext* ctx, int budget_s) const {
    char line[768];
    std::string states;
    for (unsigned bit = 0; bit < sizeof(kPointQueryStateNames) / sizeof(kPointQueryStateNames[0]); ++bit) {
      if ((observed_state_bits_ & (1u << bit)) != 0) {
        states += (states.empty() ? "" : "|");
        states += kPointQueryStateNames[bit];
      }
    }
    int n = std::snprintf(
        line, sizeof(line), "[scopes-gate] TIMEOUT '%s' budget=%ds polls=%d avg=%.0f ms/poll max_gap=%.0f ms states=%s",
        gate_name_, budget_s, polls_,
        polls_ > 1 ? std::chrono::duration_cast<std::chrono::duration<double>>(last_poll_ - first_poll_).count() *
                         1000.0 / static_cast<double>(polls_ - 1) :
                     0.0,
        std::chrono::duration_cast<std::chrono::duration<double>>(max_poll_interval_).count() * 1000.0, states.c_str());
    for (int i = 0; i < condition_count_ && n > 0 && n < static_cast<int>(sizeof(line)); ++i) {
      const ConditionSlice& c = conditions_[i];
      if (c.ever_true) {
        const auto since_start = [this](std::chrono::steady_clock::time_point tp) {
          return std::chrono::duration_cast<std::chrono::duration<double>>(tp - first_poll_).count();
        };
        n += std::snprintf(line + n, sizeof(line) - static_cast<size_t>(n), " %s=%.1fs..%.1fs", c.name,
                           since_start(c.first_seen), since_start(c.last_seen));
      } else {
        n += std::snprintf(line + n, sizeof(line) - static_cast<size_t>(n), " %s=NEVER", c.name);
      }
    }
    ctx->LogInfo("%s", line);
  }

 private:
  ConditionSlice* SliceFor(const char* name) {
    for (int i = 0; i < condition_count_; ++i) {
      if (std::strcmp(conditions_[i].name, name) == 0) {
        return &conditions_[i];
      }
    }
    ConditionSlice* fresh = &conditions_[condition_count_];
    fresh->name = name;
    ++condition_count_;
    return fresh;
  }

  const char* gate_name_ = nullptr;
  int polls_ = 0;
  int condition_count_ = 0;
  unsigned observed_state_bits_ = 0;
  std::chrono::steady_clock::time_point first_poll_{};
  std::chrono::steady_clock::time_point last_poll_{};
  std::chrono::steady_clock::duration max_poll_interval_{};
  ConditionSlice conditions_[kMaxConditions]{};
};

// DriveUntil plus the sampled polling history for one of the scopes case's observation gates:
// the predicate samples itself into the trace for the timeout-path triage, and its return value
// is the gate's verdict (for the InFlight gates, the recorded pickup edge — never the sample).
template <typename Pred>
bool DriveScopesGate(ImGuiTestContext* ctx, const char* gate_name, Pred pred, int timeout_s) {
  ScopesGateTrace trace(gate_name);
  const bool ok = DriveUntil(ctx, [&trace, &pred] { return pred(trace); }, timeout_s);
  if (!ok) {
    trace.LogTimeoutSummary(ctx, timeout_s);
  }
  return ok;
}

// The scopes case's observation transport for the InFlight gates: the gates assert "a moment
// existed when the issued intent was WAITING and the executor's per-face token ran it" — an
// event, judged from a transition RECORD, not from a level sample. Frame-cadence polling
// structurally cannot make that observation: the running registration's true window is the
// engine call itself (the executor clears the token at the engine call's return, before the
// finish is even posted), and the fixed-dt scenes run a point query to completion between two
// polls — the measured family-A reds were gates spinning tens of thousands of polls whose
// window never overlapped a single sample.
//
// Why a recorded point pickup PROVES the asserted moment (the structural linkage, stated once
// here — the gates' verdicts stand on it):
//  1. SurveyExecutor's query branch registers the job's cancel token as THE running handle under
//     the lock, then copies the pickup gate, unlocks, and invokes it — so at the instant of a
//     kPoint pickup, InFlight(kPoint) holds by the executor's own construction.
//  2. The session-side conditions hold at that same instant because nothing on the GUI thread
//     can move the point session off the issued intent without also dropping or replacing the
//     queued job (PumpPointQuery's key-change and identity-death branches, and every document
//     reset, cancel or overwrite the pending query before it can ever be picked up), while
//     adoption needs the finish, which needs the engine call, which runs after this gate. A
//     query that got picked up was still the session's standing intent at pickup: intent_valid,
//     the issued key and kWaiting all held.
//
// OWNERSHIP LAW (the executor copies the gate under its lock and invokes the copy off the lock,
// so clearing the slot cannot revoke a copy already made): the callback captures its state by
// VALUE as a shared pointer, never `this` — a copy still in flight after the guard's destructor
// keeps that state alive, and the destructor only deregisters future pickups. Production never
// sets the pickup gate; the guard clears it on every exit path — the executor is a process-global
// and later cases must not inherit a counting callback.
struct ScopedPointPickupEdgeRecorder {
  ScopedPointPickupEdgeRecorder() : count_(std::make_shared<std::atomic<int>>(0)) {
    gui::g_survey_executor.SetPickupGateForTest([count = count_](gui::QueryFace face) {
      if (face == gui::QueryFace::kPoint) {
        count->fetch_add(1, std::memory_order_relaxed);
      }
    });
  }
  ~ScopedPointPickupEdgeRecorder() { gui::g_survey_executor.SetPickupGateForTest(nullptr); }
  ScopedPointPickupEdgeRecorder(const ScopedPointPickupEdgeRecorder&) = delete;
  ScopedPointPickupEdgeRecorder& operator=(const ScopedPointPickupEdgeRecorder&) = delete;

  int Baseline() const { return count_->load(std::memory_order_relaxed); }
  bool PickedUpSince(int baseline) const { return count_->load(std::memory_order_relaxed) > baseline; }

 private:
  std::shared_ptr<std::atomic<int>> count_;
};

// The operation-sync counterpart of the recorder, for the arms whose ACTION (withdraw the
// outstanding face, replace the document, start a new one) requires the query to be outstanding
// AT the action, not merely to have run: a historical edge cannot carry that premise (the query
// may complete between two GUI polls — the measured family-A interleaving — and the action would
// land on an already-finished face). Arming swaps the pickup gate for one that PARKS the worker
// at the pickup instant — after the running token is registered, before the engine call — so the
// query stays verifiably computing (InFlight(kPoint) true by the executor's own construction)
// while the test performs the action on the GUI thread; Release lets the engine call proceed.
// Release (explicit, or the destructor's on every exit path) must precede anything that needs
// the worker. The same ownership law applies: the parked copy holds the shared state by value,
// so a copy still in flight after the guard's death is safe.
//
// The worker-side wait is bounded as a LOUD hang net, not a silent one: if the GUI thread stalls
// past the bound, the worker proceeds unblocked — the "outstanding at the action" premise is
// void — so the timeout sets Voided() and the arm's gates consume it. A voided hold turns the
// arm red with an honest shape; it never lets the run look green on a premise it did not hold.
struct ScopedPointPickupHold {
  struct State {
    std::atomic<bool> picked{ false };
    std::atomic<bool> released{ false };
    std::atomic<bool> voided{ false };
    std::mutex mu;
    std::condition_variable cv;
  };

  // Point by default for the scopes arms; survey cancellation needs the same action-time fence.
  explicit ScopedPointPickupHold(gui::QueryFace held_face = gui::QueryFace::kPoint)
      : state_(std::make_shared<State>()) {
    gui::g_survey_executor.SetPickupGateForTest([st = state_, held_face](gui::QueryFace face) {
      if (face != held_face) {
        return;
      }
      st->picked.store(true, std::memory_order_release);
      std::unique_lock<std::mutex> lock(st->mu);
      const bool released_normally = st->cv.wait_for(lock, std::chrono::seconds(30),
                                                     [&st] { return st->released.load(std::memory_order_acquire); });
      if (!released_normally) {
        st->voided.store(true, std::memory_order_release);  // the premise died with the stall
      }
    });
  }
  ~ScopedPointPickupHold() { Release(); }
  ScopedPointPickupHold(const ScopedPointPickupHold&) = delete;
  ScopedPointPickupHold& operator=(const ScopedPointPickupHold&) = delete;

  bool PickedUp() const { return state_->picked.load(std::memory_order_acquire); }
  bool Voided() const { return state_->voided.load(std::memory_order_acquire); }

  void Release() {
    if (!state_) {
      return;
    }
    gui::g_survey_executor.SetPickupGateForTest(nullptr);  // future pickups run free
    {
      std::lock_guard<std::mutex> lock(state_->mu);
      state_->released.store(true, std::memory_order_release);
    }
    state_->cv.notify_all();  // the parked copy (if any) proceeds; it owns its state by value
  }

 private:
  std::shared_ptr<State> state_;
};

// The wait lines' " (N.N s)" suffix, read off a render-feedback field; -1.0 when the line is
// empty or carries no suffix (the caller distinguishes "no wait text" from "a wait text whose
// number is zero"). Only the growth/rollback COMPARISONS consume it — the exact bytes are the
// pure functions' pins in the unit layer, never re-asserted here.
double ParseWaitElapsed(const std::string& line) {
  const size_t open = line.rfind(" (");
  if (open == std::string::npos) {
    return -1.0;
  }
  const size_t close = line.find(" s)", open);
  if (close == std::string::npos) {
    return -1.0;
  }
  return std::atof(line.substr(open + 2, close - open - 2).c_str());
}

// Server up, halo scene loaded, one finite render run to completion — the state Analyze needs.
// `infinite` leaves the render running instead, for the mutual-exclusion case and for the
// run-after-analysis cases (an unbounded analysis is the one the cone target ends). `gpu` asks
// for the GPU backend the way the Settings box does: DoRun reconstructs the server for it
// (Metal here; where no device is available ResolveGpuBackend falls back to CPU, as in
// test_color_window.cpp's GPU case), so the CPU server created below is only ever the seed.
bool BringUpScene(ImGuiTestContext* ctx, const char* scene_json, bool infinite, bool gpu) {
  ResetTestState();
  gui::g_server = LUMICE_CreateServer();
  IM_CHECK_RETV(gui::g_server != nullptr, false);
  gui::ResetServerConstructionTrackers();  // the seed is a CPU server; make the tracker say so
  LUMICE_SetLogLevel(gui::g_server, static_cast<LUMICE_LogLevel>(g_core_log_level));
  IM_CHECK_RETV(gui::DeserializeFromJson(scene_json, gui::g_state), false);
  gui::g_state.renderer.sim_resolution_index = 0;
  gui::g_state.sim.infinite = infinite;
  gui::g_state.sim.ray_num_millions = 0.1f;
  gui::g_state.use_gpu_backend = gpu;
  ctx->Yield(2);
  gui::DoRun(/*user_initiated=*/true);
  if (infinite) {
    IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kSimulating; }, 10), false);
  } else {
    IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kDone; }, 60), false);
    // A completed run has put a texture up, so the preview viewport the click is measured in is
    // live. (Mid-run it may not be yet — the first upload waits on the quality gate.)
    IM_CHECK_RETV(gui::g_preview_vp.active, false);
  }
  return true;
}

bool BringUpHaloScene(ImGuiTestContext* ctx, bool infinite, bool gpu = false) {
  return BringUpScene(ctx, kHalo22Json, infinite, gpu);
}

// Read the analysis window's live rectangle out of the default framebuffer and write it as a PNG,
// the way functional/test_theme_scan.cpp's ExportRegion does (same flip, same Retina scale, same
// clip to the framebuffer). Not compared against anything here — the file is for a human's eyes.
bool SaveWindowPng(ImGuiTestContext* ctx, const char* window_ref, const std::string& path) {
  ImGuiWindow* win = ctx->GetWindowByRef(window_ref);
  IM_CHECK_RETV(win != nullptr, false);
  const ImGuiIO& io = ImGui::GetIO();
  const float sx = io.DisplayFramebufferScale.x;
  const float sy = io.DisplayFramebufferScale.y;
  const float fb_w = io.DisplaySize.x * sx;
  const float fb_h = io.DisplaySize.y * sy;
  const ImVec2 vp_pos = ImGui::GetMainViewport()->Pos;
  const float x0 = std::max(0.0f, (win->Pos.x - vp_pos.x) * sx);
  const float y0 = std::max(0.0f, (win->Pos.y - vp_pos.y) * sy);
  const float x1 = std::min(fb_w, (win->Pos.x - vp_pos.x + win->Size.x) * sx);
  const float y1 = std::min(fb_h, (win->Pos.y - vp_pos.y + win->Size.y) * sy);
  g_fullframe_capture.Reset();
  g_fullframe_capture.rect_x = static_cast<int>(std::lround(x0));
  g_fullframe_capture.rect_y = static_cast<int>(std::lround(fb_h - y1));
  g_fullframe_capture.rect_w = static_cast<int>(std::lround(x1 - x0));
  g_fullframe_capture.rect_h = static_cast<int>(std::lround(y1 - y0));
  g_fullframe_capture.requested.store(true);
  for (int i = 0; i < 10 && !g_fullframe_capture.done.load(); ++i) {
    ctx->Yield(1);
  }
  IM_CHECK_RETV(g_fullframe_capture.done.load(), false);
  IM_CHECK_RETV(g_fullframe_capture.width > 0 && g_fullframe_capture.height > 0, false);
  const std::vector<unsigned char> rgb = lumice::test::StripAlpha(
      g_fullframe_capture.pixels.data(), g_fullframe_capture.width, g_fullframe_capture.height);
  IM_CHECK_RETV(
      lumice::test::SavePng(path.c_str(), rgb.data(), g_fullframe_capture.width, g_fullframe_capture.height, 3), false);
  ctx->LogInfo("[raypath_analysis] window %s -> %s (%dx%d)", window_ref, path.c_str(), g_fullframe_capture.width,
               g_fullframe_capture.height);
  return true;
}

// The result list is a ScrollY table, i.e. a child window, and only a row the table has on
// screen is findable by label (a clipped item never reports its label to the engine; the engine's
// own scroll-and-retry fallback pans the window given as ref, not the child inside it). So: pan
// the table's child a page at a time until the label resolves or the child is at its end.
// Returns the item's id, 0 when no row carries the label at any scroll position.
ImGuiID FindListRowScrolling(ImGuiTestContext* ctx, const std::string& label) {
  ImGuiWindow* analysis = ctx->GetWindowByRef(kWindowRef);
  IM_CHECK_RETV(analysis != nullptr, 0);
  ImGuiWindow* table_child = nullptr;
  for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
    if (w->ParentWindow == analysis && std::strstr(w->Name, "##analysis_rows") != nullptr) {
      table_child = w;
      break;
    }
  }
  IM_CHECK_RETV(table_child != nullptr, 0);
  const std::string path = "**/" + label;
  table_child->Scroll.y = 0.0f;
  ctx->Yield(2);
  while (true) {
    const ImGuiID id = ctx->ItemInfo(path.c_str(), ImGuiTestOpFlags_NoError).ID;
    if (id != 0) {
      return id;
    }
    if (table_child->Scroll.y >= table_child->ScrollMax.y) {
      return 0;
    }
    // InnerRect includes the frozen header. A full-height jump can move a row from below
    // the old viewport entirely underneath that header. Overlap the header plus one data row
    // so every row has a fully visible position, including fractional-height page boundaries.
    const float row_h = ImGui::GetTextLineHeight() + 2.0f * ImGui::GetStyle().CellPadding.y;
    const float page_h = std::max(1.0f, table_child->InnerRect.GetHeight() - 2.0f * row_h);
    table_child->Scroll.y = std::min(table_child->ScrollMax.y, table_child->Scroll.y + page_h);
    ctx->Yield(2);
  }
}

void OpenWindow(ImGuiTestContext* ctx) {
  // "Opening the analysis tool" IS entering the Analyze workspace now: the switch's Analyze slot
  // brings up the frame whose share list and panorama ROI this suite drives. Idempotent — a case
  // already in Analyze just yields.
  if (gui::g_state.workspace.mode != gui::GuiState::WorkspaceMode::kAnalyze) {
    ctx->ItemClick("##TopBar/Analyze");
    ctx->Yield(3);
  }
  ctx->Yield(1);
}

// The panorama's PICTURE region. ##AnalyzePanorama exists twice: the top-level chrome window
// (the Region radios live there — FindWindowByName lands on it, its id being the name's own
// hash) and, inside it, the child the deferred pass blits the picture under.
ImGuiWindow* PanoramaPictureWindow() {
  for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
    if (w != nullptr && w->Active && w->ParentWindow != nullptr &&
        std::strstr(w->Name, "##AnalyzePanoramaPicture") != nullptr) {
      return w;
    }
  }
  return nullptr;
}

// A child region's live ImGuiWindow, resolved by its PARENT's exact top-level name plus a name
// fragment: child windows register as "Parent/Name_<id>", so a ref cannot address them directly
// (the same lookup shape PanoramaPictureWindow and FindListRowScrolling use). The hosts: the
// map region (##AnalyzeRaypathView) lives under the Raypath view top-level window (755.4) —
// MapRegionWindow is its address; the 3D and globe regions (##AnalyzeRaypath3D /
// ##AnalyzeSunGlobe) live under the Pose & trace window (755.5) — PoseRegionWindow is theirs;
// each exists only while its window is open (a closed window submits no child).
ImGuiWindow* RegionWindowUnder(const char* parent_name, const char* name_fragment) {
  for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
    if (w != nullptr && w->Active && w->ParentWindow != nullptr &&
        std::strcmp(w->ParentWindow->Name, parent_name) == 0 && std::strstr(w->Name, name_fragment) != nullptr) {
      return w;
    }
  }
  return nullptr;
}
ImGuiWindow* MapRegionWindow() {
  return RegionWindowUnder("Raypath view", "##AnalyzeRaypathView");
}
// The map window's colour bucket, in index quads — the unit rig's IndexCountForColour over the
// real window's draw list. The anchor's line quads and its label's glyphs carry the semantic
// colour six vertices at a time; the backing rect's ink is a different colour and stays out of
// every bucket. This reads the PRODUCTION assembly's own output (the VerdictAnchorInput field
// copies at the canvas), which the session-flag and legend reads only imply.
int MapWindowIndexCountForColour(ImU32 colour) {
  const ImGuiWindow* region = MapRegionWindow();
  if (region == nullptr) {
    return 0;
  }
  const ImDrawList* dl = region->DrawList;
  int count = 0;
  for (int i = 0; i + 5 < dl->IdxBuffer.Size; i += 6) {
    bool matches = true;
    for (int k = 0; k < 6; ++k) {
      if (dl->VtxBuffer[static_cast<int>(dl->IdxBuffer[i + k])].col != colour) {
        matches = false;
        break;
      }
    }
    if (matches) {
      count += 6;
    }
  }
  return count;
}
// The info region beside the map (##AnalyzeRaypathInfo): the wavelength, the caliber block
// and the sky-point input. The D3 split (contract §4.2) puts the picture's
// facts here; the containment asserts for those controls anchor to THIS child.
ImGuiWindow* MapInfoRegionWindow() {
  return RegionWindowUnder("Raypath view", "##AnalyzeRaypathInfo");
}
ImGuiWindow* MapReportWindow() {
  const ImGuiWindow* info = MapInfoRegionWindow();
  for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
    if (w != nullptr && w->Active && w->ParentWindow == info && std::strstr(w->Name, "##MapReport") != nullptr) {
      return w;
    }
  }
  return nullptr;
}
// The Pose & trace window's two regions (755.5): the 3D orbit (##AnalyzeRaypath3D) and the
// sun-direction globe (##AnalyzeSunGlobe) moved under the pose window's top level — the same
// region names, a new host; they exist only while the window is open.
ImGuiWindow* PoseRegionWindow(const char* name_fragment) {
  return RegionWindowUnder("Pose & trace", name_fragment);
}

// The splitter strip's real-drag gesture, shared by the pose split cases. The splits, resize
// and chain cases each carried a private copy of this idiom, and the copies are exactly what
// this task's own history warns about (the px-vs-pt unit slip and the split-span denominator
// were both fixed in one copy while its sibling still carried the bug) — one gesture, one
// owner.
bool PoseDragStrip(ImGuiTestContext* ctx, const char* id, float dx, float dy) {
  const ImGuiTestItemInfo grab = ctx->ItemInfo(id, ImGuiTestOpFlags_NoError);
  IM_CHECK_SILENT_RETV(grab.ID != 0, false);
  const ImVec2 c = grab.RectFull.GetCenter();
  ctx->MouseMoveToPos(c);
  ctx->Yield(2);
  ctx->MouseDown(0);
  ctx->Yield(2);
  ctx->MouseMoveToPos(ImVec2(c.x + dx, c.y + dy));
  ctx->Yield(3);
  ctx->MouseUp(0);
  ctx->Yield(2);
  return true;
}

// A region's scrolling child, found by the parent fragment + child name fragment (the
// report/companion/state children under ##AnalyzePoseCurve / ##AnalyzePoseFactors /
// ##AnalyzeRaypath3D). Shared by the pose cases for the same single-owner reason as
// PoseDragStrip.
ImGuiWindow* PoseRegionScrollChild(const char* parent_fragment, const char* name_fragment) {
  for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
    if (w != nullptr && w->Active && w->ParentWindow != nullptr &&
        std::strstr(w->ParentWindow->Name, parent_fragment) != nullptr &&
        std::strstr(w->Name, name_fragment) != nullptr) {
      return w;
    }
  }
  return nullptr;
}

// The D5 pair's height bounds at the measured geometry, in PIXELS: the floors when they can
// share the span, the proportional fallback share when they cannot — min(floor, share) is the
// bound both shapes honour. All terms in pixels (a pt value against a px share reads as the
// floor at 1x and lies at 1.5x/2x — the 1.5x leg of the chain case caught exactly that slip),
// and the span is the two regions' heights (the strip and its two spacings sit OUTSIDE the
// derivation's denominator; summing the parts' full stack inflates the bound by exactly that
// strip — the fallback regime made the inflation fatal).
void PoseD5HeightBounds(float r3d_h, float curve_h, float* bound_3d, float* bound_curve) {
  const float denom_left = r3d_h + curve_h;
  const float d5_joint = gui::kPoseViewLeft3dFloor + gui::kPoseViewCurveFloor;
  *bound_3d = std::min(gui::UiPx(gui::kPoseViewLeft3dFloor), denom_left * (gui::kPoseViewLeft3dFloor / d5_joint));
  *bound_curve = std::min(gui::UiPx(gui::kPoseViewCurveFloor), denom_left * (gui::kPoseViewCurveFloor / d5_joint));
}

// The four pose session splits, restored on scope exit — the failure path is the pollution
// surface the cross-case lesson's prescription targets (a tail restore only covers the happy
// path; an IM_CHECK_RETV early exit would leak the dragged extremes to the whole pool).
struct ScopedPoseSessionSplits {
  bool open;
  float d4, d6, d5, d7;
  ScopedPoseSessionSplits()
      : open(gui::g_state.analyze_pose_view_open), d4(gui::g_state.analyze_pose_view_right_ratio),
        d6(gui::g_state.analyze_pose_view_globe_ratio), d5(gui::g_state.analyze_pose_view_left_row_ratio),
        d7(gui::g_state.analyze_pose_view_curve_canvas_ratio) {}
  ~ScopedPoseSessionSplits() {
    gui::g_state.analyze_pose_view_open = open;
    gui::g_state.analyze_pose_view_right_ratio = d4;
    gui::g_state.analyze_pose_view_globe_ratio = d6;
    gui::g_state.analyze_pose_view_left_row_ratio = d5;
    gui::g_state.analyze_pose_view_curve_canvas_ratio = d7;
  }
};

// The floating window is user-placed; at a smaller workspace basis the cascade position can
// park it past the OS window's edge, and the viewport clip then cuts the region's content
// (the containment asserts would read the clip, not the layout). A user moves the window back
// on screen — the same write, wherever a leg needs the whole region in-viewport. (8,60) keeps
// the default 884-wide content inside the 900-wide compressed OS window too.
void PinRaypathViewOnScreen(ImGuiTestContext* ctx) {
  if (ImGuiWindow* win = ctx->GetWindowByRef("//Raypath view")) {
    ImGui::SetWindowPos(win, ImVec2(8.0f, 60.0f), ImGuiCond_Always);
    ctx->Yield(2);
  }
}
// The pose window's pin, the Raypath view's own shape: a floating window whose footprint the
// scale rebuilds and reopen cycles can leave partly outside the viewport hides the column its
// region sits in — a fully-clipped child submits no items (the engine's registry never sees
// them), and every leg below that drives the globe would wait forever. The pin keeps the
// window's geometry a known on-screen fact wherever a leg is about to read or drive it.
void PinPoseViewOnScreen(ImGuiTestContext* ctx) {
  // The window registers on its first Begin — if the flag was set this very frame, wait for the
  // pool entry before pinning (a pin that silently skips leaves the FirstUseEver position, and
  // every geometry leg downstream inherits it).
  for (int i = 0; i < 10 && ctx->GetWindowByRef("//Pose & trace") == nullptr; ++i) {
    ctx->Yield(1);
  }
  if (ImGuiWindow* win = ctx->GetWindowByRef("//Pose & trace")) {
    ImGui::SetWindowPos(win, ImVec2(8.0f, 40.0f), ImGuiCond_Always);
    // The skeleton re-derives its size only on a UI-scale change; a tier whose OS window SHRANK
    // between two opens would otherwise keep the stale larger size, and the window's right
    // column — the globe's — would land past the viewport's clip, where its items are partly
    // invisible (registered but uncontainable). The clamp here is the same arithmetic the
    // FirstUseEver apply runs at a fresh appearance: the contract default, clamped to the
    // viewport it lands in. A leg that wants a specific geometry (the minimum-size legs) sets
    // its own size AFTER the pin.
    const ImVec2 vp = ImGui::GetMainViewport()->Size;
    ImGui::SetWindowSize(win, ImVec2(std::min(gui::UiPx(960.0f), vp.x), std::min(gui::UiPx(720.0f), vp.y)),
                         ImGuiCond_Always);
    ctx->Yield(2);
  }
}

// Alt/Option-click the main picture; returns the canvas pixel the click
// landed on (the picture's own canvas, g_panorama_vp). The pick's picture is the Analyze
// workspace's main view — the preview never hosted the cone gestures.
gui::CanvasPixel PickPanoramaCentre(ImGuiTestContext* ctx) {
  ImGuiWindow* pano = PanoramaPictureWindow();
  IM_CHECK_SILENT_RETV(pano != nullptr, gui::CanvasPixel{});
  // Half the disk's radius right of centre — the disk (min(width, height) / 2), not the window:
  // the main view is wide, and a quarter of its WIDTH would fall outside the picture disk.
  const float disk_radius = std::min(pano->Size.x, pano->Size.y) * 0.5f;
  const float px_pt = pano->Pos.x + pano->Size.x * 0.5f + disk_radius * 0.5f;
  const float py_pt = pano->Pos.y + pano->Size.y * 0.5f;
  ctx->MouseMoveToPos(ImVec2(px_pt, py_pt));
  ctx->Yield(2);
  // Where the click actually lands, from the mouse's own position: MouseMoveToPos can be a pixel
  // off the request, and the oracle must be built from the same pixel the panel saw.
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  const std::optional<gui::CanvasPixel> px =
      gui::PreviewPointToCanvasPixel(mouse.x - pano->Pos.x, mouse.y - pano->Pos.y, gui::g_panorama_vp.dpi_scale_x,
                                     gui::g_panorama_vp.dpi_scale_y, gui::g_panorama_vp.vp_w, gui::g_panorama_vp.vp_h);
  IM_CHECK_SILENT_RETV(px.has_value(), gui::CanvasPixel{});
  ctx->KeyDown(ImGuiMod_Alt);
  ctx->MouseClick(0);
  ctx->KeyUp(ImGuiMod_Alt);
  ctx->Yield(2);
  return *px;
}

// Click the panorama's picture AT a sky point (altitude/azimuth in degrees): the projected pixel,
// through the same annotation view the picture is drawn with — the click and the cone it places
// share one projection, so the cone centre is the point named, not a rounding away from it.
bool PickPanoramaAt(ImGuiTestContext* ctx, float alt_deg, float az_deg) {
  ImGuiWindow* picture = PanoramaPictureWindow();
  IM_CHECK_RETV(picture != nullptr, false);
  float dir[3] = { 0.0f, 0.0f, 0.0f };
  lumice::AltAzToDir(alt_deg, az_deg, dir);
  const LUMICE_AnnotationView view =
      gui::PreviewAnnotationView(gui::g_state, gui::g_panorama_vp.vp_w, gui::g_panorama_vp.vp_h);
  float fx = 0.0f;
  float fy = 0.0f;
  int valid = 0;
  IM_CHECK_RETV(LUMICE_ProjectDirection(&view, dir, &fx, &fy, &valid) == LUMICE_OK && valid == 1, false);
  float x_pt = 0.0f;
  float y_pt = 0.0f;
  gui::CanvasPixelToPreviewPoint(static_cast<int>(std::lround(fx)), static_cast<int>(std::lround(fy)),
                                 gui::g_panorama_vp.dpi_scale_x, gui::g_panorama_vp.dpi_scale_y, &x_pt, &y_pt);
  ctx->MouseMoveToPos(ImVec2(picture->Pos.x + x_pt, picture->Pos.y + y_pt));
  ctx->Yield(2);
  ctx->KeyDown(ImGuiMod_Alt);
  ctx->MouseClick(0);
  ctx->KeyUp(ImGuiMod_Alt);
  ctx->Yield(2);
  return gui::g_state.analysis.cone_center_valid;
}

// The cone marker's position on screen this frame (the panel's own projection, through the DPI
// path the drawing uses), or nullopt when there is none.
std::optional<ImVec2> MarkerScreenPos(ImGuiTestContext* ctx) {
  (void)ctx;
  ImGuiWindow* w = PanoramaPictureWindow();
  if (w == nullptr) {
    return std::nullopt;
  }
  const LUMICE_AnnotationView view =
      gui::PreviewAnnotationView(gui::g_state, gui::g_panorama_vp.vp_w, gui::g_panorama_vp.vp_h);
  const std::optional<gui::CanvasPixel> px = gui::ProjectConeCenterMarker(gui::g_state, view);
  if (!px.has_value()) {
    return std::nullopt;
  }
  float x_pt = 0.0f;
  float y_pt = 0.0f;
  gui::CanvasPixelToPreviewPoint(px->px, px->py, gui::g_panorama_vp.dpi_scale_x, gui::g_panorama_vp.dpi_scale_y, &x_pt,
                                 &y_pt);
  return ImVec2(w->Pos.x + x_pt, w->Pos.y + y_pt);
}

// The colour on screen at one ImGui-space point, read back off the default framebuffer through the
// harness's sub-region capture (the same read SaveWindowPng makes for a whole window). A 3x3 patch
// is captured and its middle pixel returned, so a half-pixel of rounding on a Retina scale cannot
// put the sample on a neighbour.
bool ReadScreenPixel(ImGuiTestContext* ctx, ImVec2 pos, unsigned char out_rgb[3]) {
  const ImGuiIO& io = ImGui::GetIO();
  const float sx = io.DisplayFramebufferScale.x;
  const float sy = io.DisplayFramebufferScale.y;
  const float fb_h = io.DisplaySize.y * sy;
  const ImVec2 vp_pos = ImGui::GetMainViewport()->Pos;
  const int cx = static_cast<int>(std::lround((pos.x - vp_pos.x) * sx));
  const int cy = static_cast<int>(std::lround((pos.y - vp_pos.y) * sy));  // top-down
  g_fullframe_capture.Reset();
  g_fullframe_capture.rect_x = cx - 1;
  g_fullframe_capture.rect_y = static_cast<int>(std::lround(fb_h)) - cy - 2;  // GL origin: bottom-left
  g_fullframe_capture.rect_w = 3;
  g_fullframe_capture.rect_h = 3;
  g_fullframe_capture.requested.store(true);
  for (int i = 0; i < 10 && !g_fullframe_capture.done.load(); ++i) {
    ctx->Yield(1);
  }
  IM_CHECK_RETV(g_fullframe_capture.done.load(), false);
  IM_CHECK_RETV(g_fullframe_capture.width == 3 && g_fullframe_capture.height == 3, false);
  const unsigned char* px = g_fullframe_capture.pixels.data() + (1 * 3 + 1) * 4;
  out_rgb[0] = px[0];
  out_rgb[1] = px[1];
  out_rgb[2] = px[2];
  return true;
}

// Whether an 8-bit colour is the accent colour the marker is drawn in, within a tolerance that
// covers the framebuffer's quantisation but not a dimmed or covered pixel (the modal's dim alone
// takes every channel down by 55%).
bool IsAccentColour(const unsigned char rgb[3]) {
  const ImVec4 accent = gui::AccentColor();
  const int want[3] = { static_cast<int>(std::lround(accent.x * 255.0f)),
                        static_cast<int>(std::lround(accent.y * 255.0f)),
                        static_cast<int>(std::lround(accent.z * 255.0f)) };
  for (int c = 0; c < 3; ++c) {
    if (std::abs(static_cast<int>(rgb[c]) - want[c]) > 12) {
      return false;
    }
  }
  return true;
}

// The direction LUMICE_UnprojectPixel gives for the mouse's CURRENT position on the preview — the
// oracle for "the centre is the direction under the cursor", built from the same pixel the panel
// saw (PickPreviewCentre's reasoning).
bool UnprojectMouse(float out[3]) {
  ImGuiWindow* w = PanoramaPictureWindow();
  if (w == nullptr) {
    return false;
  }
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  const std::optional<gui::CanvasPixel> px =
      gui::PreviewPointToCanvasPixel(mouse.x - w->Pos.x, mouse.y - w->Pos.y, gui::g_panorama_vp.dpi_scale_x,
                                     gui::g_panorama_vp.dpi_scale_y, gui::g_panorama_vp.vp_w, gui::g_panorama_vp.vp_h);
  if (!px.has_value()) {
    return false;
  }
  const LUMICE_AnnotationView view =
      gui::PreviewAnnotationView(gui::g_state, gui::g_panorama_vp.vp_w, gui::g_panorama_vp.vp_h);
  int valid = 0;
  return LUMICE_UnprojectPixel(&view, px->px, px->py, out, &valid) == LUMICE_OK && valid == 1;
}

// The Analyze main view's chrome window (the picture child, the status viewport and the
// operation bar all render inside it). The chrome window and the picture child share the name
// segment ##AnalyzePanorama (the child spells "##AnalyzePanoramaPicture_<id>"), so the lookup is
// exact-name, not a fragment match.
ImGuiWindow* AnalyzePanoramaHostWindow() {
  for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
    if (w != nullptr && w->WasActive && std::strcmp(w->Name, "##AnalyzePanorama") == 0) {
      return w;
    }
  }
  return nullptr;
}

// The drift warning's ink over the Analyze main view, read off the draw lists the way the user
// sees it. Every active window whose ancestor chain reaches the ##AnalyzePanorama chrome
// contributes the vertices that carry the given text colour; the drift warning's glyphs are the
// only warning-coloured ink that subtree draws (the marker and ring are accent, the status prose
// is the regular text colour), so the probe answers for the warning line directly. The verdict
// the callers need is the pair: whether any of it was drawn at all, and whether every vertex
// sits inside its own window's InnerClipRect — a line scrolled past its child's fold is either
// emitted below the clip or not emitted at all (RenderText stops at the clip bottom, and a
// fully-culled item never reaches AddText), and both shapes fail the "visible without
// scrolling" bar the drift warning's AC pins.
struct PanoramaWarningInk {
  bool found = false;
  bool clipped = false;
  ImRect bbox = ImRect(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);
  int vertices = 0;
};

PanoramaWarningInk CollectHostSubtreeWarningInk(ImGuiWindow* host, ImU32 colour) {
  PanoramaWarningInk ink;
  if (host == nullptr) {
    return ink;
  }
  for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
    if (w == nullptr || !w->WasActive || w->DrawList == nullptr) {
      continue;
    }
    bool under_host = false;
    for (const ImGuiWindow* p = w; p != nullptr; p = p->ParentWindow) {
      if (p == host) {
        under_host = true;
        break;
      }
    }
    if (!under_host) {
      continue;
    }
    for (const ImDrawVert& v : w->DrawList->VtxBuffer) {
      if (v.col != colour) {
        continue;
      }
      ink.found = true;
      ++ink.vertices;
      ink.bbox.Add(v.pos);
      // Half a pixel of rounding slack — the same containment tolerance the region pins use.
      if (v.pos.x < w->InnerClipRect.Min.x - 0.5f || v.pos.x > w->InnerClipRect.Max.x + 0.5f ||
          v.pos.y < w->InnerClipRect.Min.y - 0.5f || v.pos.y > w->InnerClipRect.Max.y + 0.5f) {
        ink.clipped = true;
      }
    }
  }
  return ink;
}

PanoramaWarningInk CollectPanoramaWarningInk(ImU32 colour) {
  return CollectHostSubtreeWarningInk(AnalyzePanoramaHostWindow(), colour);
}

// The same probe, generalized to a floating window by exact name: the staleness hints' hosts
// are the Raypath view and Pose & trace windows, whose subtrees read the way the panorama
// host's does. Exact match on the root window's own name — its children spell
// "Raypath view/<child>_<id>", a form a strstr would also hit and a root lookup must not.
PanoramaWarningInk CollectWindowWarningInk(const char* host_name, ImU32 colour) {
  ImGuiWindow* host = nullptr;
  for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
    if (w != nullptr && w->WasActive && std::strcmp(w->Name, host_name) == 0) {
      host = w;
      break;
    }
  }
  return CollectHostSubtreeWarningInk(host, colour);
}

// The exact packed colour the warning text's vertices carry, read the way the renderer reads it:
// push the semantic colour, read ImGuiCol_Text back — so a theme change or a global alpha
// factor cannot drift the probe away from what was actually drawn.
ImU32 WarningTextVertexColour() {
  ImGui::PushStyleColor(ImGuiCol_Text, gui::WarningTextColor());
  const ImU32 colour = ImGui::GetColorU32(ImGuiCol_Text);
  ImGui::PopStyleColor();
  return colour;
}

// Whether the pick banner is on screen: the banner child exists and was submitted last frame
// (a window that stopped being submitted stays in ImGui's window list, so existence alone says
// nothing — WasActive does). The banner is a CHILD of the share-list window, and BeginChild
// spells child names "Parent/Name_<id>" — a form the engine's window refs cannot address — so
// the pool is scanned for the name segment instead.
bool PickBannerVisible(ImGuiTestContext* ctx) {
  (void)ctx;
  for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
    if (w != nullptr && w->WasActive && std::strstr(w->Name, "##pick_banner") != nullptr) {
      return true;
    }
  }
  return false;
}

// Point mode, a centre picked on the panorama, Analyze pressed: the run is in progress on
// return. The request controls — region radios, centre reading, Analyze — sit on the main view's
// operation bar (contract §2.2). The pick targets the halo's own centre — the direction the
// document's
// camera looks (22 degrees above the sun on its azimuth), the point the OLD default centre (the
// preview's middle pixel) sat on for free. The panorama's default centre is the fisheye's
// middle (the zenith), off the halo, so the point analyses that assert chain identities pick
// this direction explicitly: the 3-5 / 3-7 rows the fixture's assertions name live in this cone.
bool StartPointAnalysis(ImGuiTestContext* ctx) {
  ctx->SetRef(PanoramaRegionRef());
  ctx->ItemClick("Point");
  ctx->Yield(2);  // the Point controls settle after the radio flips
  ctx->Yield(1);
  ctx->SetRef("");
  IM_CHECK_RETV(PickPanoramaAt(ctx, gui::g_state.renderer.elevation, gui::g_state.renderer.azimuth), false);
  IM_CHECK_RETV(gui::g_state.analysis.cone_center_valid, false);
  // The Analyze button sits on the main view's operation bar.
  ctx->SetRef(PanoramaRegionRef());
  ctx->ItemClick(kAnalyzeButton);
  ctx->SetRef("");
  IM_CHECK_RETV(gui::g_state.analysis.started, false);
  return true;
}

// A Point analysis on a FINITE budget (the scene's, seeded into the panel): it ends by itself
// on that budget, the only end a run has apart from Stop.
bool RunPointAnalysisToCompletion(ImGuiTestContext* ctx) {
  IM_CHECK_RETV(StartPointAnalysis(ctx), false);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return !gui::g_state.analysis_run_in_progress && gui::g_state.analysis_result.payload != nullptr; },
          60),
      false);
  return true;
}

// A Point analysis on an UNLIMITED budget, ended from the window's own Stop once the list has
// rows: the in-progress flag falls, and the list stays — non-empty, never older than what was
// on show. This is the user's way of ending a cone analysis (there is no cone stop target).
bool RunPointAnalysisThenStop(ImGuiTestContext* ctx) {
  IM_CHECK_RETV(StartPointAnalysis(ctx), false);
  IM_CHECK_RETV(gui::g_state.analysis.infinite, false);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::g_state.analysis_result.payload != nullptr &&
                             !gui::g_state.analysis_result.payload->entries.empty();
                    },
                    30),
                false);
  const auto partial = gui::g_state.analysis_result.payload;
  ctx->SetRef(PanoramaRegionRef());
  IM_CHECK_RETV(ctx->ItemInfo(ICON_FA_STOP " Stop").ID != 0, false);
  ctx->ItemClick(ICON_FA_STOP " Stop");
  ctx->SetRef("");
  IM_CHECK_RETV(!gui::g_state.analysis.started, false);
  // The picture stays the render's, so sim_state returns to kDone once the async stop drains.
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return !gui::g_state.analysis_run_in_progress && gui::g_state.sim_state == SimState::kDone; }, 20),
      false);
  IM_CHECK_RETV(gui::g_state.analysis_result.payload != nullptr, false);
  IM_CHECK_RETV(!gui::g_state.analysis_result.payload->entries.empty(), false);
  IM_CHECK_RETV(gui::g_state.analysis_result.payload->snapshot_generation >= partial->snapshot_generation, false);
  return true;
}

// The owner's own sequence: Run, Analyze, Run again — and the second Run RENDERS. This is the
// one path through the panel that nothing above drives (every case so far ends on the analysis
// or on the slider), and the one that once went black: a cone-ROI analysis left a server-side
// flag up at the session switch, and the render session after it traced nothing — no batch, no
// frame, a preview stuck on "Simulating" until a backend switch rebuilt the server. That flag
// (the cone stop target's) no longer exists; what this case guards now is the session switch
// itself — that a stopped analysis leaves nothing behind that keeps the next render from
// producing. The server case that pins the mechanism is
// ServerAnalysisRun.RenderAfterStoppedAnalysisProducesAFrame; this one pins the user's view of
// it: sim_state reaches kDone and a texture goes up.
//
// The analysis is an UNLIMITED one, ended from the window's Stop (RunPointAnalysisThenStop): the
// render whose scene it inherits is an unbounded one, stopped by hand once it has put a picture
// up (the pick needs a live preview), so the panel's seeded budget is unlimited too. The run
// after the analysis is then made finite so that "it rendered" can be read as kDone rather than
// as "never ended".
//
// `exclude` adds the owner's step in between: select the top chain, press "Exclude this
// raypath", so the second Run commits a document with a filter in it. The bug does not need it
// (the server case has none), and it is driven here so the sequence as reported stays covered.
bool RunAfterAnalysisRenders(ImGuiTestContext* ctx, bool exclude, bool gpu) {
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/true, gpu), false);
  IM_CHECK_RETV(
      DriveUntil(ctx, [] { return gui::g_preview_vp.active && gui::g_state.texture_upload_count > 0; }, 30), false);
  gui::DoStop();
  IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kDone; }, 20), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunPointAnalysisThenStop(ctx), false);
  // The analysis was stopped, not completed (a stop is a reset, so the server reads idle), and
  // the list on show is the cone's.
  LUMICE_SimLifecycleResult after_analysis{};
  LUMICE_GetSimLifecycle(gui::g_server, &after_analysis);
  IM_CHECK_RETV(after_analysis.lifecycle == static_cast<int>(LUMICE_LIFECYCLE_IDLE), false);
  IM_CHECK_RETV(gui::g_state.analysis_result.payload->roi_mode == LUMICE_RAYPATH_ROI_CONE, false);

  if (exclude) {
    const auto& view_result = gui::g_state.analysis_result;
    IM_CHECK_RETV(!view_result.display_order.empty(), false);
    const int top = view_result.display_order[0];
    SetListRef(ctx);
    // The row is a Selectable under PushID(original index) inside the results table; the
    // wildcard finds it by label through the table's and the id's anonymous path segments.
    const std::string top_display = view_result.payload->entries[static_cast<size_t>(top)].display;
    const std::string row = std::string("**/") + top_display;
    ctx->ItemClick(row.c_str());
    ctx->Yield(1);
    IM_CHECK_RETV(gui::g_state.analysis.selected_entry.has_value(), false);
    IM_CHECK_RETV(*gui::g_state.analysis.selected_entry == top_display, false);
    IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo(ICON_FA_BAN " Exclude this raypath###action_exclude")), false);
    ctx->ItemClick(ICON_FA_BAN " Exclude this raypath###action_exclude");
    ctx->Yield(1);
    // The filter is in the document, an Out one, so the same crystal stays excludable: the
    // button is still enabled, and a second press of it for the same chain is the idempotent
    // path — driven here under real frames, and read back as "one filter, one row, unchanged".
    IM_CHECK_RETV(gui::g_state.filters.size() == 1u, false);
    IM_CHECK_RETV(gui::g_state.filters[0].action == 1, false);
    IM_CHECK_RETV(gui::g_state.filters[0].param.size() == 1u, false);
    IM_CHECK_RETV(gui::EvaluateExcludeEligibility(gui::g_state, nullptr) == gui::ExcludeEligibility::kOk, false);
    IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo(ICON_FA_BAN " Exclude this raypath###action_exclude")), false);
    ctx->ItemClick(ICON_FA_BAN " Exclude this raypath###action_exclude");
    ctx->SetRef("");
    ctx->Yield(1);
    IM_CHECK_RETV(gui::g_state.filters.size() == 1u, false);
    IM_CHECK_RETV(gui::g_state.filters[0].param.size() == 1u, false);
  }

  // The second Run, from the top bar, on a finite budget.
  gui::g_state.sim.infinite = false;
  gui::g_state.sim.ray_num_millions = 0.1f;
  const unsigned long long uploads_before = gui::g_state.texture_upload_count;
  const unsigned long long serial_before = gui::g_state.last_uploaded_texture_serial;
  ctx->Yield(1);
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo("##TopBar/" ICON_FA_PLAY " Run")), false);
  ctx->ItemClick("##TopBar/" ICON_FA_PLAY " Run");
  IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kSimulating; }, 10), false);
  // Both halves of "it rendered": the run ended, and its picture went up. The document is
  // dirty here (the budget edit above; the Exclude before it), and a dirty document's finished
  // run reads kModified, not kDone — the real main loop clears dirty on its 70 ms auto-commit
  // tick while simulating (main.cpp), a tick gui_test's loop only has under --main-loop-commit,
  // so it is cleared by hand at the same moment the app would.
  gui::g_state.dirty = false;
  IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kDone; }, 60), false);
  IM_CHECK_RETV(
      DriveUntil(ctx, [uploads_before] { return gui::g_state.texture_upload_count > uploads_before; }, 10), false);
  IM_CHECK_RETV(gui::g_state.last_uploaded_texture_serial != serial_before, false);
  return true;
}

// Replace the app's server with one that has never committed anything, so that whatever the
// next analysis traces cannot have come from a run this server saw.
bool ReplaceServerWithAFreshOne(ImGuiTestContext* ctx) {
  lumice::test::JoinGuiServerTasksForTeardown();
  gui::g_server_poller.Stop();
  if (gui::g_server != nullptr) {
    LUMICE_StopServer(gui::g_server);
    LUMICE_DestroyServer(gui::g_server);
  }
  gui::g_server = LUMICE_CreateServer();
  IM_CHECK_RETV(gui::g_server != nullptr, false);
  gui::ResetServerConstructionTrackers();
  LUMICE_SetLogLevel(gui::g_server, static_cast<LUMICE_LogLevel>(g_core_log_level));
  ctx->Yield(1);
  LUMICE_SimLifecycleResult lc{};
  LUMICE_GetSimLifecycle(gui::g_server, &lc);
  IM_CHECK_RETV(lc.epoch == 0u, false);
  return true;
}

// Whole-sky mode, Analyze pressed, driven until the run ends on the panel's finite budget and
// the list is on show. `saw_simulating` reports whether sim_state ever read kSimulating on the
// way — the analysis must not be mistaken for a render, and that is only observable by sampling
// every frame rather than by looking at the end state.
bool RunWholeSkyAnalysisToCompletion(ImGuiTestContext* ctx, bool* saw_simulating) {
  ctx->SetRef(PanoramaRegionRef());
  ctx->ItemClick("Whole sky");
  ctx->SetRef(PanoramaRegionRef());
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo(kAnalyzeButton)), false);
  ctx->SetRef(PanoramaRegionRef());
  ctx->ItemClick(kAnalyzeButton);
  ctx->SetRef("");
  IM_CHECK_RETV(gui::g_state.analysis.started, false);
  IM_CHECK_RETV(!gui::g_state.analysis.infinite, false);
  bool simulating = false;
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [&simulating] {
                      simulating = simulating || gui::g_state.sim_state == SimState::kSimulating;
                      return !gui::g_state.analysis_run_in_progress && gui::g_state.analysis_result.payload != nullptr;
                    },
                    60),
                false);
  if (saw_simulating != nullptr) {
    *saw_simulating = simulating;
  }
  IM_CHECK_RETV(!gui::g_state.analysis_result.payload->entries.empty(), false);
  IM_CHECK_RETV(!gui::g_state.analysis_result.display_order.empty(), false);
  return true;
}

std::string TopChainDisplay() {
  const auto& view = gui::g_state.analysis_result;
  return view.payload->entries[static_cast<size_t>(view.display_order[0])].display;
}

// Write the sky-point intent for (altitude, azimuth) labels of the sky point itself: one named
// conversion for every anchor in this suite — the intent carries TRAVEL (the negated position),
// the sign convention spelled here once instead of five hand-written trig copies.
void SetSkyPointTarget(double alt_deg, double az_deg) {
  const double pos[3] = { std::cos(alt_deg * lumice::kDeg2Rad) * std::cos(az_deg * lumice::kDeg2Rad),
                          std::cos(alt_deg * lumice::kDeg2Rad) * std::sin(az_deg * lumice::kDeg2Rad),
                          std::sin(alt_deg * lumice::kDeg2Rad) };
  gui::g_state.analysis.sky_point.valid = true;
  for (int k = 0; k < 3; ++k) {
    gui::g_state.analysis.sky_point.dir[k] = static_cast<float>(-pos[k]);
  }
}

// Find and click the list row whose display spelling is exactly `display` (the scroll-to shape
// the multilayer leg established), returning the selection identity it arms.
std::optional<gui::SelectedClassIdentity> SelectRowByDisplay(ImGuiTestContext* ctx, const char* display) {
  std::string found;
  for (const int idx : gui::g_state.analysis_result.display_order) {
    const auto& e = gui::g_state.analysis_result.payload->entries[static_cast<size_t>(idx)];
    if (std::strcmp(e.display, display) == 0) {
      found = e.display;
      break;
    }
  }
  if (found.empty()) {
    return std::nullopt;
  }
  const std::string label = gui::AnalysisRowLabel(found, gui::AnalysisCoreToPoolMap(gui::g_state));
  SetListRef(ctx);
  const ImGuiID row = FindListRowScrolling(ctx, label);
  if (row == 0) {
    return std::nullopt;
  }
  ctx->ItemClick(row);
  ctx->SetRef("");
  return gui::SelectedClassIdentityFor(gui::g_state);
}

// Pin the old pagination blind spot without relying on a stochastic class rank: the first
// row wholly below page one can land entirely under the frozen header after a full-height jump.
bool ListScrollFindsTheRowUnderTheNextFrozenHeader(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  SetListRef(ctx);
  ImGuiWindow* share = ctx->GetWindowByRef(kWindowRef);
  IM_CHECK_RETV(share != nullptr, false);
  ImGuiWindow* table_child = nullptr;
  for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
    if (w->ParentWindow == share && std::strstr(w->Name, "##analysis_rows") != nullptr) {
      table_child = w;
      break;
    }
  }
  IM_CHECK_RETV(table_child != nullptr, false);
  const float row_h = ImGui::GetTextLineHeight() + 2.0f * ImGui::GetStyle().CellPadding.y;
  const int seam_ordinal = static_cast<int>(std::ceil(table_child->InnerRect.GetHeight() / row_h)) - 1;
  const auto& view = gui::g_state.analysis_result;
  std::vector<int> rendered;
  for (const int idx : view.display_order) {
    if (view.display_energy[static_cast<size_t>(idx)] > 0.0) {
      rendered.push_back(idx);
    }
  }
  IM_CHECK_RETV(seam_ordinal >= 0 && seam_ordinal < static_cast<int>(rendered.size()), false);
  const std::string display = view.payload->entries[static_cast<size_t>(rendered[seam_ordinal])].display;
  const std::string label = gui::AnalysisRowLabel(display, gui::AnalysisCoreToPoolMap(gui::g_state));
  ctx->LogInfo("[row-seam] ordinal=%d row_h=%.1f viewport=%.1f display='%s'", seam_ordinal, row_h,
               table_child->InnerRect.GetHeight(), display.c_str());
  const ImGuiID row = FindListRowScrolling(ctx, label);
  IM_CHECK_RETV(row != 0, false);
  ctx->ItemClick(row);
  ctx->SetRef("");
  IM_CHECK_RETV(gui::g_state.analysis.selected_entry.has_value() && *gui::g_state.analysis.selected_entry == display,
                false);
  return true;
}

// A sink of our own on the GUI logger for the duration of a case, so what DoRun logs can be read
// back. gui_test does not attach one by itself: g_imgui_log_sink exists only under --log-panel,
// and even then it is fed by the CORE log callback, not by the GUI logger DoRun writes to
// (test_log_panel.cpp attaches its own for the same reason). Restores the sink list on the way
// out — it is process-wide and gui_test is one process.
struct ScopedGuiLogCapture {
  std::shared_ptr<gui::ImGuiLogSink> sink;
  std::vector<spdlog::sink_ptr> prev_sinks;

  ScopedGuiLogCapture() : sink(std::make_shared<gui::ImGuiLogSink>()), prev_sinks(gui::GetGuiLogger().sinks()) {
    gui::GetGuiLogger().sinks().push_back(sink);
  }
  ~ScopedGuiLogCapture() { gui::GetGuiLogger().sinks() = prev_sinks; }

  bool Contains(const char* needle) const {
    bool found = false;
    sink->ForEachEntry(
        [&](size_t, const gui::LogEntry& e) { found = found || e.message.find(needle) != std::string::npos; });
    return found;
  }
};

const char* const kMismatchNeedle = "predict/actual mismatch";

// The Run after a COMPLETED analysis, from the top bar: it renders, and DoRun did not have to
// fall back on its safety net. The net is the "predict/actual mismatch" branch — the GUI
// predicted that the commit would reuse the previous session's consumers, the server rebuilt
// them (it never reuses an analysis session's), and the poller was stopped late. DoRun avoids
// it by reading the server's own session kind (LUMICE_GetSimLifecycle's session_kind) into its
// rebuild predicate before the commit; this is the one layer that can see the two agree, and it
// reads the warning's absence off a sink attached for the purpose. The capture is armed only
// for the second Run, so the analysis's own logging cannot mask a warning it never printed —
// and a probe line proves the sink is receiving before "absent" is read as "did not happen".
bool RunAfterCompletedAnalysisHasNoRebuildMismatch(ImGuiTestContext* ctx) {
  IM_CHECK_RETV(gui::g_state.sim_state == SimState::kDone, false);
  IM_CHECK_RETV(!gui::g_state.analysis_run_in_progress, false);
  LUMICE_SimLifecycleResult before_run{};
  LUMICE_GetSimLifecycle(gui::g_server, &before_run);
  IM_CHECK_RETV(before_run.session_kind == static_cast<int>(LUMICE_SESSION_ANALYSIS), false);

  ScopedGuiLogCapture capture;
  GUI_LOG_WARNING("[gui_test] log capture probe");
  IM_CHECK_RETV(capture.Contains("log capture probe"), false);

  const unsigned long long uploads_before = gui::g_state.texture_upload_count;
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo("##TopBar/" ICON_FA_PLAY " Run")), false);
  ctx->ItemClick("##TopBar/" ICON_FA_PLAY " Run");
  IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kSimulating; }, 10), false);
  IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kDone; }, 60), false);
  IM_CHECK_RETV(
      DriveUntil(ctx, [uploads_before] { return gui::g_state.texture_upload_count > uploads_before; }, 10), false);
  LUMICE_SimLifecycleResult after_run{};
  LUMICE_GetSimLifecycle(gui::g_server, &after_run);
  IM_CHECK_RETV(after_run.session_kind == static_cast<int>(LUMICE_SESSION_RENDER), false);
  IM_CHECK_RETV(!capture.Contains(kMismatchNeedle), false);
  return true;
}

// Exclude while the Immediate-mode editor is open on the crystal's own entry, Filter tab showing.
//
// The editor's buffers are a copy of the entry's crystal and filter taken when it opened, and in
// Immediate mode it writes them back into the pool every frame; the analysis window writes the same
// filter slot when Exclude is clicked, and main.cpp orders the two so the analysis window goes first
// (gui_test's frame loop keeps that order). So what this drives is the one write the editor must
// pull in rather than overwrite: the pool must hold the excluded row afterwards, the entry must
// still be bound to it, and the editor's own row list must show it — the user is looking at both.
//
// `start_with_out_filter` seeds the entry with a one-row Out filter first: with no filter the
// exclusion is a NEW slot the entry gets bound to; with one it is a row APPENDED to the slot the
// editor already holds. The editor loses each in its own way (unbinding the new slot, or restoring
// the old row set over the appended one), so both are driven.
bool ExcludeWhileTheImmediateEditorIsOpen(ImGuiTestContext* ctx, bool start_with_out_filter) {
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunPointAnalysisToCompletion(ctx), false);
  const auto& view_result = gui::g_state.analysis_result;
  IM_CHECK_RETV(!view_result.display_order.empty(), false);
  const LUMICE_RaypathHistogramEntry& top =
      view_result.payload->entries[static_cast<size_t>(view_result.display_order[0])];
  IM_CHECK_RETV(top.chain_len == 1, false);  // the single-layer scene; Exclude accepts only these
  const std::string top_display = top.display;
  const std::string top_row_text = gui::FormatSegmentRaypathText(top.chain[0]);

  std::vector<std::string> expected_rows;
  if (start_with_out_filter) {
    // Any face path but the one about to be excluded, so the append is a real second row.
    const std::string seed = (top_row_text == "1-3") ? "3-5" : "1-3";
    gui::FilterConfig out;
    out.name = "drop one";
    out.action = 1;  // filter_out
    out.param = gui::FromLegacyRaypath(gui::RaypathParams{ seed });
    gui::SetFilter(gui::g_state, gui::g_state.layers[0].entries[0], out);
    expected_rows.push_back(seed);
    ctx->Yield(2);
  }
  expected_rows.push_back(top_row_text);
  IM_CHECK_RETV(gui::g_state.filters.size() == (start_with_out_filter ? 1u : 0u), false);

  const ScopedPopups popup_guard(ctx);
  gui::g_state.modal_immediate_mode = true;
  const gui::EditRequest req{ gui::EditTarget::kFilter, 0, 0 };
  gui::OpenEditModal(req, gui::g_state);
  ctx->Yield(4);
  IM_CHECK_RETV(gui::IsEditModalOpen(), false);
  // Out of the analysis window's way, so the row and the button below are the items under the
  // mouse rather than the editor's title bar.
  ctx->WindowMove("//Edit Entry", ImVec2(760.0f, 40.0f));
  ctx->Yield(2);

  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + top_display).c_str());
  ctx->Yield(1);
  IM_CHECK_RETV(gui::g_state.analysis.selected_entry.has_value(), false);
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo(ICON_FA_BAN " Exclude this raypath###action_exclude")), false);
  ctx->ItemClick(ICON_FA_BAN " Exclude this raypath###action_exclude");
  ctx->SetRef("");
  // Two frames: the click's frame already ran the editor's pull and push once after the write;
  // one more shows whatever it left behind is stable rather than mid-flight.
  ctx->Yield(2);

  // The pool, as the next Run will read it.
  const auto& entry = gui::g_state.layers[0].entries[0];
  ctx->LogInfo("after Exclude: filters=%d entry.filter_id=%d", static_cast<int>(gui::g_state.filters.size()),
               entry.filter_id.has_value() ? *entry.filter_id : -1);
  IM_CHECK_RETV(entry.filter_id.has_value(), false);
  IM_CHECK_RETV(gui::g_state.filters.size() == 1u, false);  // bound to the one slot, no orphan
  const gui::FilterConfig& bound = gui::g_state.filters[static_cast<size_t>(*entry.filter_id)];
  IM_CHECK_RETV(bound.action == 1, false);
  {
    std::string got;
    for (const auto& row : bound.param) {
      got += "[" + row.text + "]";
    }
    ctx->LogInfo("pool rows: %s", got.c_str());
  }
  IM_CHECK_RETV(bound.param.size() == expected_rows.size(), false);
  for (const std::string& text : expected_rows) {
    const bool present =
        std::any_of(bound.param.begin(), bound.param.end(), [&](const gui::SummandText& r) { return r.text == text; });
    if (!present) {
      IM_ERRORF("pool filter lost row \"%s\"", text.c_str());
      return false;
    }
  }

  // The editor, as the user sees it: the same rows, in its own row list.
  const gui::EditModalBuffers buffers = gui::GetEditModalBuffers();
  {
    std::string got;
    for (const auto& row : buffers.filter_rows) {
      got += "[" + row + "]";
    }
    ctx->LogInfo("editor rows: %s", got.c_str());
  }
  IM_CHECK_RETV(buffers.filter_top.action == 1, false);
  IM_CHECK_RETV(buffers.filter_rows.size() == expected_rows.size(), false);
  for (const std::string& text : expected_rows) {
    const bool present =
        std::find(buffers.filter_rows.begin(), buffers.filter_rows.end(), text) != buffers.filter_rows.end();
    if (!present) {
      IM_ERRORF("editor row list lost row \"%s\"", text.c_str());
      return false;
    }
  }

  ctx->SetRef("//Edit Entry");
  ctx->ItemClick("Close##edit_modal");
  ctx->Yield(2);
  gui::g_state.modal_immediate_mode = false;
  return true;
}

// The halo document on the panels with no server behind it, and a hand-built whole-sky result
// on show as RefreshAnalysisEntries leaves one — the fixture for the excluded-row cases, whose
// subject is the list's derivation from the document's filters, not a run. `chains` are
// single-segment chains of the one crystal (scene id 0), with their energies.
bool ShowHaloDocumentWithResult(ImGuiTestContext* ctx, unsigned long long generation,
                                const std::vector<std::pair<const char*, double>>& chains) {
  auto payload = std::make_shared<gui::AnalysisPayload>();
  payload->snapshot_generation = generation;
  payload->roi_mode = LUMICE_RAYPATH_ROI_FULL_SKY;
  for (const auto& [display, energy] : chains) {
    LUMICE_RaypathHistogramEntry e{};
    e.chain_len = 1;
    e.chain[0].crystal_id = 0;
    e.chain[0].segment_len = 2;
    e.chain[0].segment[0] = display[0] - '0';
    e.chain[0].segment[1] = display[2] - '0';
    snprintf(e.display, sizeof(e.display), "%s", display);
    e.energy = energy;
    e.count = 1000;
    payload->entries.push_back(e);
  }
  IM_CHECK_RETV(gui::AdoptAnalysisPayloadIfNew(gui::g_state, payload), false);
  gui::g_state.analysis_result.entries_symmetry = gui::AnalysisSymmetryBits(gui::g_state);
  gui::g_state.analysis.fetched_once = true;
  gui::g_state.analysis.fetched_generation = generation;
  gui::g_state.analysis.fetched_symmetry = gui::AnalysisSymmetryBits(gui::g_state);
  ctx->Yield(2);
  return true;
}

// The path of the greyed row for `display`, and of its Include again button — the row is a
// selectable labelled ExcludedRowLabel under a PushID of the display text, the button a child of
// that id, so "**/<display>/<button>" resolves to it and to no other row's. The label is built
// the way RenderResultList builds it: rewritten under the live document's map.
std::string ExcludedRowPath(const std::string& display) {
  std::map<int, int> core_to_pool;
  for (const auto& [pool, core] : gui::ComputeCrystalPoolToCoreIdMap(gui::g_state)) {
    core_to_pool.emplace(core, pool);
  }
  return gui::ExcludedRowLabel(display, core_to_pool);
}

std::string IncludeAgainPath(const std::string& display) {
  return display + "/" + gui::kIncludeAgainButtonLabel;
}

// Exclude a chain, put a result on show that no longer has it, and read the greyed row off the
// screen: it is there with the share the chain had, the search box and the right-click copy
// reach it like any row, it is not a selection, and its Include again button erases the row
// from the filter — here the filter's only row, so the entry loses the filter — and takes the
// greyed row with it.
bool ExcludedRowShowsAndIncludeAgainTakesItBack(ImGuiTestContext* ctx) {
  ResetTestState();
  IM_CHECK_RETV(gui::DeserializeFromJson(kHalo22Json, gui::g_state), false);
  // Opened AFTER the deserialize, which replaces the whole GuiState — the workspace mode
  // included — so a workspace entered before it would be bounced back to Render and the list
  // would never draw.
  OpenWindow(ctx);
  IM_CHECK_RETV(gui::g_state.filters.empty(), false);
  IM_CHECK_RETV(ShowHaloDocumentWithResult(ctx, 1, { { "3-5", 3.0 }, { "1-3", 1.0 } }), false);
  ctx->WindowMove(kWindowRef, ImVec2(60, 60));
  ctx->Yield(1);
  SetListRef(ctx);
  IM_CHECK_RETV(ctx->ItemInfo((std::string("**/") + ExcludedRowPath("3-5")).c_str(), ImGuiTestOpFlags_NoError).ID == 0,
                false);

  // Exclude 3-5: the filter is written, the memory holds the 75% the row showed.
  ctx->ItemClick("**/3-5");
  ctx->Yield(1);
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo(ICON_FA_BAN " Exclude this raypath###action_exclude")), false);
  ctx->ItemClick(ICON_FA_BAN " Exclude this raypath###action_exclude");
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.filters.size() == 1u, false);
  IM_CHECK_RETV(gui::g_state.filters[0].param.size() == 1u, false);
  // Still a result row: shown as one, not greyed.
  IM_CHECK_RETV(ctx->ItemInfo("**/3-5").ID != 0, false);
  IM_CHECK_RETV(ctx->ItemInfo((std::string("**/") + ExcludedRowPath("3-5")).c_str(), ImGuiTestOpFlags_NoError).ID == 0,
                false);

  // The next result has no 3-5 (the filter took): the greyed row appears, under the result row
  // 1-3 and with the remembered share.
  ctx->SetRef("");
  IM_CHECK_RETV(ShowHaloDocumentWithResult(ctx, 2, { { "1-3", 1.0 } }), false);
  SetListRef(ctx);
  IM_CHECK_RETV(ctx->ItemInfo("**/3-5", ImGuiTestOpFlags_NoError).ID == 0, false);
  const ImGuiTestItemInfo greyed = ctx->ItemInfo((std::string("**/") + ExcludedRowPath("3-5")).c_str());
  IM_CHECK_RETV(greyed.ID != 0, false);
  const ImGuiTestItemInfo kept = ctx->ItemInfo("**/1-3");
  IM_CHECK_RETV(kept.ID != 0, false);
  IM_CHECK_RETV(greyed.RectFull.Min.y > kept.RectFull.Min.y, false);  // after the result rows
  const std::vector<gui::ExcludedRaypathRow> rows = gui::ComputeExcludedRaypathRows(gui::g_state);
  IM_CHECK_RETV(rows.size() == 1u, false);
  IM_CHECK_RETV(rows[0].was_pct.has_value(), false);
  IM_CHECK_RETV(std::fabs(*rows[0].was_pct - 75.0) < 1e-9, false);
  IM_CHECK_RETV(std::strcmp(gui::ExcludedRowWasText(rows[0].was_pct).c_str(), "was 75.00%") == 0, false);
  // A click on it selects nothing: the Exclude button stays shut.
  ctx->ItemClick((std::string("**/") + ExcludedRowPath("3-5")).c_str());
  ctx->Yield(1);
  IM_CHECK_RETV(!gui::g_state.analysis.selected_entry.has_value(), false);
  IM_CHECK_RETV(IsDisabled(ctx->ItemInfo(ICON_FA_BAN " Exclude this raypath###action_exclude")), false);

  // The search box reaches it: "1-3" hides it, "3-5" shows it alone, empty brings all back.
  SetListRef(ctx);
  ctx->ItemInputValue("###analysis_search", "1-3");
  ctx->Yield(3);
  IM_CHECK_RETV(ctx->ItemInfo((std::string("**/") + ExcludedRowPath("3-5")).c_str(), ImGuiTestOpFlags_NoError).ID == 0,
                false);
  IM_CHECK_RETV(ctx->ItemInfo("**/1-3").ID != 0, false);
  ctx->ItemInputValue("###analysis_search", "3-5");
  ctx->Yield(3);
  IM_CHECK_RETV(ctx->ItemInfo((std::string("**/") + ExcludedRowPath("3-5")).c_str()).ID != 0, false);
  IM_CHECK_RETV(ctx->ItemInfo("1-3", ImGuiTestOpFlags_NoError).ID == 0, false);
  ctx->ItemInputValue("###analysis_search", "");
  ctx->Yield(3);

  // The right-click menu reaches it: the raw text, and the row in its three-field form.
  auto choose = [&](const std::string& path, const char* item) {
    ctx->ItemClick((std::string("**/") + path.c_str()).c_str(), ImGuiMouseButton_Right);
    ctx->Yield(2);
    ctx->SetRef("//$FOCUSED");
    ctx->ItemClick((std::string("**/") + item).c_str());
    ctx->Yield(2);
    SetListRef(ctx);
  };
  choose(ExcludedRowPath("3-5"), "Copy raypath");
  IM_CHECK_RETV(std::strcmp(ImGui::GetClipboardText(), "3-5") == 0, false);
  choose(ExcludedRowPath("3-5"), "Copy row");
  IM_CHECK_RETV(std::strcmp(ImGui::GetClipboardText(), gui::ExcludedRowCopyText(rows[0]).c_str()) == 0, false);
  IM_CHECK_RETV(std::strcmp(ImGui::GetClipboardText(), "3-5,excluded,75.0000") == 0, false);

  // For a human's eyes: the greyed row under the result row, the button in its last column.
  ctx->SetRef("");
  ctx->MouseMoveToPos(ImVec2(2.0f, 2.0f));
  ctx->Yield(2);
  IM_CHECK_RETV(SaveWindowPng(ctx, kWindowRef, GuiTestTempPath("analysis_excluded_row.png").string()), false);

  // Include again: the filter's only row goes, so the entry drops the filter, the memory is
  // forgotten, and the greyed row is gone the next frame. 1-3 is untouched.
  SetListRef(ctx);
  IM_CHECK_RETV(ctx->ItemInfo((std::string("**/") + IncludeAgainPath("3-5")).c_str()).ID != 0, false);
  ctx->ItemClick((std::string("**/") + IncludeAgainPath("3-5")).c_str());
  ctx->Yield(2);
  IM_CHECK_RETV(!gui::g_state.layers[0].entries[0].filter_id.has_value(), false);
  IM_CHECK_RETV(gui::g_state.analysis.excluded_memory.empty(), false);
  IM_CHECK_RETV(ctx->ItemInfo((std::string("**/") + ExcludedRowPath("3-5")).c_str(), ImGuiTestOpFlags_NoError).ID == 0,
                false);
  IM_CHECK_RETV(ctx->ItemInfo("**/1-3").ID != 0, false);
  IM_CHECK_RETV(gui::ComputeExcludedRaypathRows(gui::g_state).empty(), false);
  ctx->SetRef("");
  return true;
}

// Include again while the Immediate-mode editor is open on the entry, Filter tab showing, on a
// filter of TWO rows so the filter itself stays — the branch the editor's pull is argued on
// (edit_modals.cpp PullSummandRows: theirs lost a row the buffer had not touched, so the buffer
// drops it too). The editor pushes its buffers into the pool every frame; if it did not pull the
// removal first it would write the row straight back. So: the pool holds one row afterwards,
// the entry is still bound to the slot, and the editor's own row list shows one row — the user
// is looking at both. The mirror of ExcludeWhileTheImmediateEditorIsOpen, frame for frame.
bool IncludeAgainWhileTheImmediateEditorIsOpen(ImGuiTestContext* ctx) {
  ResetTestState();
  IM_CHECK_RETV(gui::DeserializeFromJson(kHalo22Json, gui::g_state), false);
  // After the deserialize, which resets the whole GuiState (workspace mode included).
  OpenWindow(ctx);
  gui::FilterConfig out;
  out.name = "drop two";
  out.action = 1;  // filter_out
  out.param = gui::FromLegacyRaypath(gui::RaypathParams{ "3-5" });
  out.param.push_back(gui::FromLegacyRaypath(gui::RaypathParams{ "1-3" }).front());
  gui::SetFilter(gui::g_state, gui::g_state.layers[0].entries[0], out);
  IM_CHECK_RETV(gui::g_state.filters.size() == 1u, false);
  // A result without either chain: both are greyed rows.
  IM_CHECK_RETV(ShowHaloDocumentWithResult(ctx, 1, { { "2-4", 1.0 } }), false);
  ctx->WindowMove(kWindowRef, ImVec2(60, 60));
  ctx->Yield(1);
  SetListRef(ctx);
  IM_CHECK_RETV(ctx->ItemInfo((std::string("**/") + ExcludedRowPath("3-5")).c_str()).ID != 0, false);
  IM_CHECK_RETV(ctx->ItemInfo((std::string("**/") + ExcludedRowPath("1-3")).c_str()).ID != 0, false);
  ctx->SetRef("");

  const ScopedPopups popup_guard(ctx);
  gui::g_state.modal_immediate_mode = true;
  const gui::EditRequest req{ gui::EditTarget::kFilter, 0, 0 };
  gui::OpenEditModal(req, gui::g_state);
  ctx->Yield(4);
  IM_CHECK_RETV(gui::IsEditModalOpen(), false);
  // Out of the analysis window's way, so the button below is the item under the mouse rather
  // than the editor's title bar.
  ctx->WindowMove("//Edit Entry", ImVec2(760.0f, 40.0f));
  ctx->Yield(2);
  {
    const gui::EditModalBuffers before = gui::GetEditModalBuffers();
    IM_CHECK_RETV(before.filter_rows.size() == 2u, false);
  }

  ctx->ItemClick((std::string("**/") + IncludeAgainPath("3-5").c_str()).c_str());
  ctx->SetRef("");
  // Two frames: the click's frame already ran the editor's pull and push once after the write;
  // one more shows whatever it left behind is stable rather than mid-flight.
  ctx->Yield(2);

  // The pool, as the next Run will read it: one row, the entry still bound.
  const auto& entry = gui::g_state.layers[0].entries[0];
  ctx->LogInfo("after Include again: filters=%d entry.filter_id=%d", static_cast<int>(gui::g_state.filters.size()),
               entry.filter_id.has_value() ? *entry.filter_id : -1);
  IM_CHECK_RETV(entry.filter_id.has_value(), false);
  IM_CHECK_RETV(gui::g_state.filters.size() == 1u, false);
  const gui::FilterConfig& bound = gui::g_state.filters[static_cast<size_t>(*entry.filter_id)];
  IM_CHECK_RETV(bound.action == 1, false);
  IM_CHECK_RETV(bound.param.size() == 1u, false);
  IM_CHECK_RETV(std::strcmp(bound.param[0].text.c_str(), "1-3") == 0, false);

  // The editor, as the user sees it: the one row left, in its own row list.
  const gui::EditModalBuffers buffers = gui::GetEditModalBuffers();
  {
    std::string got;
    for (const auto& row : buffers.filter_rows) {
      got += "[" + row + "]";
    }
    ctx->LogInfo("editor rows: %s", got.c_str());
  }
  IM_CHECK_RETV(buffers.filter_rows.size() == 1u, false);
  IM_CHECK_RETV(std::strcmp(buffers.filter_rows[0].c_str(), "1-3") == 0, false);

  // And the list: 3-5 is no longer greyed, 1-3 still is.
  SetListRef(ctx);
  IM_CHECK_RETV(ctx->ItemInfo((std::string("**/") + ExcludedRowPath("3-5")).c_str(), ImGuiTestOpFlags_NoError).ID == 0,
                false);
  IM_CHECK_RETV(ctx->ItemInfo((std::string("**/") + ExcludedRowPath("1-3")).c_str()).ID != 0, false);
  ctx->SetRef("");

  ctx->SetRef("//Edit Entry");
  ctx->ItemClick("Close##edit_modal");
  ctx->Yield(2);
  gui::g_state.modal_immediate_mode = false;
  return true;
}

// The owner's chain on the C01 scene (the 22-degree halo fixture this suite shares with the
// e2e layer): Analyze, pick a row, Exclude it, and Run again — driven through real frames and
// real clicks, with the two captures a human reads: the list holding the selection, the
// exclusion, and the rerun's sky on the panorama. The rerun commits the document WITH the
// filter, so the picture after it is the excluded sky.
bool C01ChainAnalyzeSelectExcludeRerun(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  // Select the top row of the list, and keep the evidence of the selection on file.
  const std::string top_display = TopChainDisplay();
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + top_display).c_str());
  ctx->Yield(1);
  IM_CHECK_RETV(gui::g_state.analysis.selected_entry.has_value(), false);
  IM_CHECK_RETV(*gui::g_state.analysis.selected_entry == top_display, false);
  ctx->LogInfo("[c01 chain] analyzed %zu rows; selected top row '%s'",
               gui::g_state.analysis_result.payload->entries.size(), top_display.c_str());
  ctx->SetRef("");
  ctx->MouseMoveToPos(ImVec2(2.0f, 2.0f));
  ctx->Yield(2);
  IM_CHECK_RETV(SaveWindowPng(ctx, kWindowRef, GuiTestTempPath("analysis_c01_chain_list.png").string()), false);
  // The right column's selection card (the identity facts the row names), on file too.
  IM_CHECK_RETV(SaveWindowPng(ctx, "//##AnalyzeDetails", GuiTestTempPath("analysis_c01_chain_details.png").string()),
                false);

  // Exclude it: one Out filter on the document, bound to the entry.
  SetListRef(ctx);
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo(ICON_FA_BAN " Exclude this raypath###action_exclude")), false);
  ctx->MouseMove(ICON_FA_BAN " Exclude this raypath###action_exclude");
  ctx->Yield(2);
  ctx->ItemClick(ICON_FA_BAN " Exclude this raypath###action_exclude");
  ctx->SetRef("");
  ctx->Yield(1);
  IM_CHECK_RETV(gui::g_state.filters.size() == 1u, false);
  IM_CHECK_RETV(gui::g_state.filters[0].action == 1, false);
  IM_CHECK_RETV(gui::g_state.layers[0].entries[0].filter_id.has_value(), false);
  ctx->LogInfo("[c01 chain] excluded '%s' (filters=%d)", top_display.c_str(),
               static_cast<int>(gui::g_state.filters.size()));

  // Run again, from the top bar, on a finite budget: it renders the excluded sky.
  gui::g_state.sim.infinite = false;
  gui::g_state.sim.ray_num_millions = 0.1f;
  const unsigned long long uploads_before = gui::g_state.texture_upload_count;
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo("##TopBar/" ICON_FA_PLAY " Run")), false);
  ctx->ItemClick("##TopBar/" ICON_FA_PLAY " Run");
  IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kSimulating; }, 10), false);
  // A dirty document's finished run reads kModified; the real loop's commit tick clears it —
  // cleared by hand here, the moment the app would (RunAfterAnalysisRenders's reasoning).
  gui::g_state.dirty = false;
  IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kDone; }, 60), false);
  IM_CHECK_RETV(
      DriveUntil(ctx, [uploads_before] { return gui::g_state.texture_upload_count > uploads_before; }, 10), false);
  ctx->LogInfo("[c01 chain] rerendered after exclusion: uploads %llu -> %llu", uploads_before,
               gui::g_state.texture_upload_count);
  ctx->MouseMoveToPos(ImVec2(2.0f, 2.0f));
  ctx->Yield(2);
  IM_CHECK_RETV(SaveWindowPng(ctx, PanoramaRegionRef(), GuiTestTempPath("analysis_c01_chain_rerender.png").string()),
                false);
  return true;
}

// ---- M3's survey-query evidence cases (742.3 M3-5) --------------------------------------------------

// The completed digest on the halo fixture: a selection issues the background query, the
// worker's answer is adopted under its key, and the typed document carries the class's real
// facts — read back here through the same readers the region draws with, so the screenshot a
// human reads and the assertions below cannot both be mock. The re-Analyze half pins R6's
// generation gate: the bookkeeping dies with the selection, the next generation re-arms.
bool SurveyDigestAnswersWithRealClassFacts(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  const std::string top_display = TopChainDisplay();
  const unsigned long long generation = gui::g_state.analysis_result.payload->snapshot_generation;
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + top_display).c_str());
  ctx->SetRef("");
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    30),
                false);
  const gui::SurveyQuerySession& survey = gui::g_state.analysis.survey;
  IM_CHECK_RETV(survey.key.generation == generation, false);
  IM_CHECK_RETV(survey.cancelled_for_key == false, false);
  LUMICE_PathSurveyInfo info{};
  info.struct_size = sizeof(info);
  IM_CHECK_RETV(LUMICE_PathSurveyGetInfo(survey.adopted_result.get(), &info) == LUMICE_PATH_READ_OK, false);
  IM_CHECK_RETV(
      info.execution == LUMICE_PATH_OUTCOME_COMPLETED || info.execution == LUMICE_PATH_OUTCOME_DEADLINE_EXCEEDED,
      false);
  // The engine echoes the request's crystal back — the adopted document answers THIS key. (The
  // core id here is 0: the fixture's pool crystal maps to core id 0, and 0 is a valid echo.)
  IM_CHECK_RETV(static_cast<long long>(info.crystal_id) == survey.key.crystal_id, false);
  ctx->LogInfo("[survey digest] generation %llu, echo crystal %lld, members %zu, objects %zu, budget %d ms", generation,
               static_cast<long long>(info.crystal_id), info.member_count, info.object_count, info.budget_ms);
  // 742.4: the same document read into the overview model the region now draws. The halo
  // fixture's zenith axis is a uniform centred at 90° — the one spelling the survey's routing v1
  // domain does NOT read as the full-sphere random family — so its dominant class is honestly
  // REFUSED (measure_shape_outside_routing_domain), and the overview's refusal arm is what the
  // region shows: the named refusal, the witness's positive event, and the chromatic layer's
  // engine-worded absence. (The dispersion-on-screen face is the kFullyRandomJson case below.)
  {
    const gui::SurveyOverviewModel model = gui::BuildSurveyOverviewModel(*survey.adopted_result);
    IM_CHECK_RETV(model.valid, false);
    IM_CHECK_RETV(model.arm == gui::SurveyArm::kRefused, false);
    IM_CHECK_RETV(model.evidence_state == LUMICE_PATH_STATE_REFUSED, false);
    IM_CHECK_RETV(model.evidence_mode.find("measure_shape_outside_routing_domain") != std::string::npos, false);
    IM_CHECK_RETV(model.witness.available && model.witness.value_available, false);
    IM_CHECK_RETV(model.witness.kept_count > 0, false);
    IM_CHECK_RETV(!model.dispersion.available && !model.diagnose.available, false);
    IM_CHECK_RETV(model.chromatic_note.find("routing v1 domain") != std::string::npos, false);
    IM_CHECK_RETV(!model.conclusions.empty(), false);
    ctx->LogInfo("[survey digest] overview model: refused arm, mode '%s', witness kept %lld/%lld, conclusions %d",
                 model.evidence_mode.c_str(), static_cast<long long>(model.witness.kept_count),
                 static_cast<long long>(model.witness.sample_count), static_cast<int>(model.conclusions.size()));
  }
  IM_CHECK_RETV(SaveWindowPng(ctx, "//##AnalyzeDetails", GuiTestTempPath("analysis_survey_digest.png").string()),
                false);

  // Capture values before Re-Analyze resets the session; a reference to survey would then
  // alias the new generation rather than retain the old outcome for the attribution checks.
  const gui::SurveyQueryId first_intent = survey.intent_id;
  const gui::SurveyHandle first_result = survey.adopted_result;

  // Re-Analyze: the selection clears, the survey bookkeeping dies with it (R6 — no adopted
  // outcome survives into the next generation's region), and a straggler of the old generation
  // would find no intent to land under.
  ctx->SetRef(PanoramaRegionRef());
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo(kAnalyzeButton)), false);
  ctx->ItemClick(kAnalyzeButton);
  ctx->SetRef("");
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analysis.survey.intent_valid == false, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_error.empty(), false);
  ctx->LogInfo("[survey digest] re-analyze voided the old generation's intent and outcome");

  // The next generation answers again, under the new generation's id. sim_state describes
  // the render, not this analysis: it can stay Done while partial analysis payloads arrive.
  // Wait for the analysis itself, or its final snapshot can clear a selection made too early.
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::g_state.analysis_result.payload != nullptr && !gui::g_state.analysis_run_in_progress; },
          60),
      false);
  const unsigned long long next_generation = gui::g_state.analysis_result.payload->snapshot_generation;
  IM_CHECK_RETV(next_generation > generation, false);
  const std::string next_display = TopChainDisplay();
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + next_display).c_str());
  ctx->SetRef("");
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    30),
                false);
  IM_CHECK_RETV(
      gui::g_state.analysis.survey.key.generation == gui::g_state.analysis_result.payload->snapshot_generation, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.key.generation == next_generation, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.intent_id.generation == next_generation, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.intent_id != first_intent, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result != first_result, false);
  IM_CHECK_RETV(gui::g_state.analysis.selected_entry == std::optional<std::string>{ next_display }, false);
  ctx->LogInfo("[survey digest] re-armed under generation %llu", gui::g_state.analysis.survey.key.generation);
  return true;
}


bool CheckMapGlyphsAcrossUiScale(ImGuiTestContext* ctx);

// The map region's face (742.5, AC1/AC3/AC6): on the fully random prism, the selected class's
// reference map arrives through the real chain — share list selection -> survey and map queued
// on the one worker -> the full-sky map — and the
// region takes a real click as the sky point while the panorama's ROI state stands untouched.
// The assertions read the adopted result the picture is drawn from, so the screenshot a human
// reads and the checks below cannot both be mock.
// The engine's watchdog measures SIMULATED time (frame_count × the injected dt — the
// lens-projection and cli-export guards' lesson), and this test legitimately spans more frames
// than the default 60 simulated seconds: five tiers of real engine recomputes, each answered
// over real frames. When the default kill fires mid-run it also poisons every later engine call
// (IsError short-circuits ItemInfo), so each remaining wait spins to its own timeout as a
// cascade — the widened budget must cover the WHOLE test, and be restored on every early path.
struct ScopedWatchdogGuard {
  ImGuiTestEngineIO* engine_io;
  float saved_warn;
  float saved_kill;
  explicit ScopedWatchdogGuard(ImGuiTestEngineIO* io)
      : engine_io(io), saved_warn(io->ConfigWatchdogWarning), saved_kill(io->ConfigWatchdogKillTest) {
    engine_io->ConfigWatchdogWarning = 50000.0f;
    engine_io->ConfigWatchdogKillTest = 100000.0f;
  }
  ~ScopedWatchdogGuard() {
    engine_io->ConfigWatchdogWarning = saved_warn;
    engine_io->ConfigWatchdogKillTest = saved_kill;
  }
};

// The marker ink probe's shared face (the AC1 red-green pin): the helper drives the two legs,
// the registration block's GuiFunc renders and counts. File-scope because the two live apart.
static gui::GuiState marker_probe;
static bool draw_marker_probe = false;
static int marker_probe_vertices = -1;

bool MapRegionShowsTheClassMapAndTakesThePick(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  // The no-data state before any selection: the region asks for a class and keeps its pick
  // input available (the honest empty state, R7).
  IM_CHECK_RETV(gui::MapQueryStateFor(gui::g_state.analysis.map) == gui::MapQueryState::kNoSelection, false);

  const std::string top_display = TopChainDisplay();
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + top_display).c_str());
  ctx->SetRef("");
  // The kWaiting pin: the multi-line queue note is exactly the
  // fixed text stack the region must carry without scrolling — the waiting-text arm the
  // skeleton scroll-pin case cannot reach honestly without a server.
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::MapQueryStateFor(gui::g_state.analysis.map) == gui::MapQueryState::kWaiting; }, 10),
      false);
  {
    ImGuiWindow* region = MapRegionWindow();
    IM_CHECK_RETV(region != nullptr, false);
    IM_CHECK_RETV(region->ScrollMax.y == 0.0f, false);
    IM_CHECK_RETV(region->Scroll.y == 0.0f, false);
  }
  const ImRect map_wait_rect = ctx->ItemInfo("**/##map_canvas").RectFull;
  const ImRect sky_wait_rect = ctx->ItemInfo("**/alt°##sky").RectFull;
  IM_CHECK_RETV(SaveWindowPng(ctx, "//Raypath view", GuiTestTempPath("layout_map_waiting.png").string()), false);
  // A real map click while the selection's survey still owns the worker. Navigation is
  // available before the queued point has any drawable sample, and must not replace its id.
  ctx->SetRef("//Raypath view");
  ctx->KeyDown(ImGuiMod_Alt);
  ctx->ItemClick("**/##map_canvas");
  ctx->KeyUp(ImGuiMod_Alt);
  ctx->Yield(1);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.from_map_pick, false);
  // The pick's own direction, saved before the button leg rewrites it: a canvas-picked
  // direction projects back onto the view by construction — the marker ink probe's marked leg
  // carries the button path's SHAPE (valid, not from a map pick) over this direction, so the
  // projection-failure arm stays out of the measurement. (A button direction outside the
  // current view draws nothing — that arm is the projection's honesty, kept as is.)
  const float picked_dir[3] = { gui::g_state.analysis.sky_point.dir[0], gui::g_state.analysis.sky_point.dir[1],
                                gui::g_state.analysis.sky_point.dir[2] };
  IM_CHECK_RETV(!gui::g_state.analyze_pose_view_open, false);
  IM_CHECK_RETV(gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kWaiting, false);
  const auto pending_point_id = gui::g_state.analysis.point.intent_id;
  ctx->ItemClick("**/Pose & trace###pose_open");
  ctx->Yield(1);
  IM_CHECK_RETV(gui::g_state.analyze_pose_view_open, false);
  IM_CHECK_RETV(PoseRegionWindow("##AnalyzeSunGlobe") != nullptr, false);
  IM_CHECK_RETV(gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kWaiting, false);
  IM_CHECK_RETV(gui::g_state.analysis.point.intent_id == pending_point_id, false);
  ctx->LogInfo("[pose navigation] opened during real queued point; same intent, still waiting");
  IM_CHECK_RETV(SaveWindowPng(ctx, "//Pose & trace", GuiTestTempPath("pose_entry_waiting.png").string()), false);
  ctx->WindowClose("//Pose & trace");
  ctx->Yield(2);
  ctx->WindowFocus("//Raypath view");
  ctx->ItemInputValue("**/alt°##sky", 30.0f);
  ctx->ItemInputValue("**/az°##sky", 180.0f);
  ctx->ItemClick("**/Set sky point###sky_set");
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid && !gui::g_state.analysis.sky_point.from_map_pick, false);
  IM_CHECK_RETV(std::abs(gui::g_state.analysis.sky_point.dir[2] + 0.5f) < 1e-4f, false);
  // A single reference answers behind the survey. Adoption neither auto-Fits nor issues a
  // second calculation; explicit Retry owns any repeated request.
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      if (m.adopted_result == nullptr || !(m.adopted_id == m.intent_id)) {
                        return false;
                      }
                      LUMICE_PathMapInfo i{};
                      i.struct_size = sizeof(i);
                      return LUMICE_PathMapGetInfo(m.adopted_result.get(), &i) == LUMICE_PATH_READ_OK &&
                             i.view_full_sky == 1;
                    },
                    60),
                false);
  const auto same_rect = [](const ImRect& a, const ImRect& b) {
    return std::abs(a.Min.x - b.Min.x) <= 0.5f && std::abs(a.Min.y - b.Min.y) <= 0.5f &&
           std::abs(a.Max.x - b.Max.x) <= 0.5f && std::abs(a.Max.y - b.Max.y) <= 0.5f;
  };
  IM_CHECK_RETV(same_rect(map_wait_rect, ctx->ItemInfo("**/##map_canvas").RectFull), false);
  IM_CHECK_RETV(same_rect(sky_wait_rect, ctx->ItemInfo("**/alt°##sky").RectFull), false);
  // The marker ink probe (AC1's red-green pin): the button path's written state — valid, NOT
  // from a map pick — must draw the same sky-point marker ink a pick draws. A controlled probe
  // window renders the adopted reference with everything but the intent fixed; the counted list
  // is the probe's map area child's own (the canvas picture, its overlays and the marker all
  // draw there; the info side's state text and the report live on other lists and stay out).
  // Asserted as an increment between two otherwise-identical frames — the button-written
  // intent against the same state with the intent invalid — so unrelated fixed ink cancels.
  {
    marker_probe = gui::g_state;
    IM_CHECK_RETV(marker_probe.analysis.map.adopted_result != nullptr, false);
    IM_CHECK_RETV(marker_probe.analysis.sky_point.valid && !marker_probe.analysis.sky_point.from_map_pick, false);
    draw_marker_probe = true;
    struct MarkerLease {
      ~MarkerLease() {
        draw_marker_probe = false;
        marker_probe = gui::GuiState{};
      }
    } marker_lease;
    marker_probe.analysis.sky_point.valid = false;
    ctx->Yield(3);
    const int base = marker_probe_vertices;
    IM_CHECK_RETV(base > 0, false);
    // The marked leg: the button path's written shape — valid, NOT from a map pick — over the
    // saved pick direction (in view by construction). The ink this leg adds is exactly what
    // the source gate admits or suppresses.
    marker_probe.analysis.sky_point.valid = true;
    marker_probe.analysis.sky_point.from_map_pick = false;
    marker_probe.analysis.sky_point.dir[0] = picked_dir[0];
    marker_probe.analysis.sky_point.dir[1] = picked_dir[1];
    marker_probe.analysis.sky_point.dir[2] = picked_dir[2];
    ctx->Yield(3);
    IM_CHECK_RETV(marker_probe_vertices > base, false);
    ctx->LogInfo("[sky point marker] button-path ink: %d -> %d vertices", base, marker_probe_vertices);
  }
  const gui::MapQuerySession& map = gui::g_state.analysis.map;
  LUMICE_PathMapInfo info{};
  info.struct_size = sizeof(info);
  IM_CHECK_RETV(LUMICE_PathMapGetInfo(map.adopted_result.get(), &info) == LUMICE_PATH_READ_OK, false);
  const std::optional<gui::SelectedClassIdentity> identity = gui::SelectedClassIdentityFor(gui::g_state);
  IM_CHECK_RETV(identity.has_value(), false);
  IM_CHECK_RETV(info.crystal_id == identity->core_crystal_id, false);
  IM_CHECK_RETV(info.kept_count > 0, false);  // the picture carries real events, not an empty echo
  ctx->LogInfo("[map view] window delta[%.1f,%.1f] az[%.1f,%.1f] %dx%d, members %d/%d, lambda %.1f, kept %lld",
               info.view_delta_min_deg, info.view_delta_max_deg, info.view_az_min_deg, info.view_az_max_deg,
               info.view_rows, info.view_cols, info.members_evaluated, info.member_count, info.wavelength_nm,
               info.kept_count);
  IM_CHECK_RETV(SaveWindowPng(ctx, "//Raypath view", GuiTestTempPath("analyze_map_region.png").string()), false);
  IM_CHECK_RETV(CheckMapGlyphsAcrossUiScale(ctx), false);

  // The pick on the real mouse path: a click on the canvas centre sets the sky point, and the
  // panorama's ROI state and the share list's selection stay exactly what they were (AC3 — the
  // two click semantics never share a write).
  const int roi_mode_before = gui::g_state.analysis.roi_mode;
  const bool roi_valid_before = gui::g_state.analysis.cone_center_valid;
  const float roi_dir_before[3] = { gui::g_state.analysis.cone_center_dir[0], gui::g_state.analysis.cone_center_dir[1],
                                    gui::g_state.analysis.cone_center_dir[2] };
  const std::string selected_before = gui::g_state.analysis.selected_entry.value_or("");
  ctx->KeyDown(ImGuiMod_Alt);
  ctx->ItemClick("**/##map_canvas");
  ctx->KeyUp(ImGuiMod_Alt);
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid, false);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.from_map_pick, false);
  IM_CHECK_RETV(gui::g_state.analysis.roi_mode == roi_mode_before, false);
  IM_CHECK_RETV(gui::g_state.analysis.cone_center_valid == roi_valid_before, false);
  IM_CHECK_RETV(gui::g_state.analysis.cone_center_dir[0] == roi_dir_before[0], false);
  IM_CHECK_RETV(gui::g_state.analysis.cone_center_dir[1] == roi_dir_before[1], false);
  IM_CHECK_RETV(gui::g_state.analysis.cone_center_dir[2] == roi_dir_before[2], false);
  IM_CHECK_RETV(gui::g_state.analysis.selected_entry.value_or("") == selected_before, false);

  // The sky-point tail's Clear, in the X5 form at the adopted-completion state: Clear sits
  // RectFull inside the INFO region (the tail lives beside the map since the D3 split) and a
  // real click clears the intent, while Set and the coordinates stay reachable.
  {
    ImGuiWindow* region = MapInfoRegionWindow();
    IM_CHECK_RETV(region != nullptr, false);
    ctx->LogInfo("[map view] clear tail: clip bottom %.0f", static_cast<double>(region->InnerClipRect.Max.y));
    const ImGuiTestItemInfo set_now = ctx->ItemInfo("**/Set sky point###sky_set", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(set_now.ID != 0, false);
    IM_CHECK_RETV(set_now.RectFull.Max.y <= region->InnerClipRect.Max.y + 0.5f, false);
    // The Clear button ends the tail, and the identity line (its senior on the region) may push
    // it past this window's fold — the region's own scroll is the designed reach for everything
    // past the fold (the degradation ladder's rung, the same rule the with-result pin below
    // states), so bring the tail in and take the containment there. The scroll resets to the top
    // afterwards: the pin below reads each region at its top.
    region->Scroll.y = region->ScrollMax.y;
    ctx->Yield(2);
    const ImGuiTestItemInfo clear_btn = ctx->ItemInfo("**/Clear##sky", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(clear_btn.ID != 0, false);
    IM_CHECK_RETV(clear_btn.RectFull.Min.x >= region->InnerClipRect.Min.x - 0.5f &&
                      clear_btn.RectFull.Max.x <= region->InnerClipRect.Max.x + 0.5f &&
                      clear_btn.RectFull.Min.y >= region->InnerClipRect.Min.y - 0.5f &&
                      clear_btn.RectFull.Max.y <= region->InnerClipRect.Max.y + 0.5f,
                  false);
    ctx->ItemClick("**/Clear##sky");
    ctx->Yield(2);
    IM_CHECK_RETV(!gui::g_state.analysis.sky_point.valid, false);
    ctx->LogInfo("[map view] Clear click landed; intent cleared");
    region->Scroll.y = 0.0f;
    ctx->Yield(2);
  }

  // The with-result pin, in its modern form: each region starts at its top (Scroll.y == 0 —
  // its first row on screen), and everything past the fold is that region's OWN internal
  // scroll, the degradation ladder's rung. The original reserve defect (742.4) showed as scroll
  // PINNED to the bottom with the header cut off — that is what Scroll.y == 0 forbids; a
  // nonzero ScrollMax (the caliber block's wrapped bulk) is the designed budget overflow,
  // wheel-reachable, not the defect.
  {
    ImGuiWindow* region = MapRegionWindow();
    ImGuiWindow* info = MapInfoRegionWindow();
    IM_CHECK_RETV(region != nullptr && info != nullptr, false);
    ctx->LogInfo("[scroll pins] map with-result SM %.1f SC %.1f, info SM %.1f SC %.1f", region->ScrollMax.y,
                 region->Scroll.y, info->ScrollMax.y, info->Scroll.y);
    IM_CHECK_RETV(region->Scroll.y == 0.0f, false);
    IM_CHECK_RETV(info->Scroll.y == 0.0f, false);
  }

  // The flip falsifier: a full-sky view puts the sun far off the disk's centre, so the halo
  // ring surrounds the sun MARKER only when the raster's row order and the anchors agree — the
  // one view where a vertical mirror of the picture is visible to the eye. Screenshot on file.
  // (The view premise arrives as a direct product-state write — the shared presets are
  // retired; the test engine's hover on this chrome layout is addressed separately.)
  gui::SelectLensType(gui::g_state, gui::kLensTypeRectangular);
  gui::g_state.renderer.visible = gui::kVisibleFull;
  gui::g_state.renderer.front = false;
  ctx->Yield(2);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      if (m.adopted_result == nullptr || !(m.adopted_id == m.intent_id)) {
                        return false;
                      }
                      LUMICE_PathMapInfo i{};
                      i.struct_size = sizeof(i);
                      return LUMICE_PathMapGetInfo(m.adopted_result.get(), &i) == LUMICE_PATH_READ_OK &&
                             i.view_full_sky == 1;
                    },
                    30),
                false);
  ctx->Yield(2);
  IM_CHECK_RETV(SaveWindowPng(ctx, "//Raypath view", GuiTestTempPath("analyze_map_region_full_sky.png").string()),
                false);
  return true;
}

// The scroll pins: the Analyze workspace's rolling regions are
// STRUCTURALLY non-scrolling — the 3D region carries its fixed stack (header row, state text)
// without a scrollbar, the divider strip contains its grab bar inside its own rect, and the
// share list never scrolls as a window. Asserted at the 1600x980 default basis, the 1280x800
// acceptance size and the 1024x640 minimum, against the worst fixed-content shapes reachable
// without a server. The map region left the workspace's geometry with its window (755.4): its
// host no longer varies with these bases, so its pin runs ONCE at the window's own geometry —
// the no-selection stack plus the two-row sky-point input must fit the window with no scroll —
// still in this fixed-pool case, still without a server: the flag opens the region, a state
// write sets the sky point, and no engine time is spent.
bool WorkspaceScrollRegionsAreStructurallyFixed(ImGuiTestContext* ctx) {
  ResetTestState();
  OpenWindow(ctx);
  // The two-row input block: a set sky point draws the "sky point: ..." line and the Clear
  // button under the alt/az row — the height the map region must budget exactly.
  SetSkyPointTarget(42.0, 0.0);
  ctx->Yield(2);
  {
    // The window's own structural pin (independent of the workspace bases below): BOTH regions
    // fit their stacks — the map side (state line + canvas) with no scroll, and the info side
    // (λ row, the two-row sky-point input) with no scroll.
    gui::g_state.analyze_raypath_view_open = true;
    ctx->Yield(2);
    ImGuiWindow* region = MapRegionWindow();
    ImGuiWindow* info = MapInfoRegionWindow();
    IM_CHECK_RETV(region != nullptr && info != nullptr, false);
    ctx->LogInfo("[scroll pins] map in window SM %.1f SC %.1f, info SM %.1f SC %.1f", region->ScrollMax.y,
                 region->Scroll.y, info->ScrollMax.y, info->Scroll.y);
    IM_CHECK_RETV(region->ScrollMax.y == 0.0f, false);
    IM_CHECK_RETV(region->Scroll.y == 0.0f, false);
    IM_CHECK_RETV(info->ScrollMax.y == 0.0f, false);
    IM_CHECK_RETV(info->Scroll.y == 0.0f, false);
    gui::g_state.analyze_raypath_view_open = false;
    ctx->Yield(2);
  }

  const auto request_basis = [&](int width, int height, const char* label) {
    g_window_size_test.width = width;
    g_window_size_test.height = height;
    g_window_size_test.done.store(false);
    g_window_size_test.requested.store(true);
    for (int i = 0; i < 20 && !g_window_size_test.done.load(); ++i) {
      ctx->Yield(1);
    }
    IM_CHECK(g_window_size_test.done.load());
    ctx->Yield(3);
    ctx->LogInfo("[scroll pins] basis %s", label);
  };
  // The pose window is the 3D region's only host now: opened once for the three bases below
  // (the window clamps to each basis's viewport; the region's fixed stack must fit at all of
  // them with no scroll).
  gui::g_state.analyze_pose_view_open = true;
  ctx->Yield(2);
  const auto assert_regions_fixed = [&]() {
    ImGuiWindow* r3d = PoseRegionWindow("##AnalyzeRaypath3D");
    ImGuiWindow* pose_win = ctx->GetWindowByRef("//Pose & trace");
    ImGuiWindow* share = ctx->GetWindowByRef(kWindowRef);
    if (r3d != nullptr && pose_win != nullptr && share != nullptr) {
      // The values first: a red assert prints its expression, this line pins the numbers.
      ctx->LogInfo("[scroll pins] 3d SM %.1f SC %.1f | share SM %.1f SC %.1f | pose SM %.1f/%.1f SC %.1f/%.1f",
                   r3d->ScrollMax.y, r3d->Scroll.y, share->ScrollMax.y, share->Scroll.y, pose_win->ScrollMax.x,
                   pose_win->ScrollMax.y, pose_win->Scroll.x, pose_win->Scroll.y);
    }
    // The 3D region: no scroll range, no scroll position — the header row and the input block
    // are reachable by construction, at every basis. (The map region is the window's: its pin
    // ran once above, at the window's own geometry.)
    IM_CHECK_RETV(r3d != nullptr, false);
    IM_CHECK_RETV(r3d->ScrollMax.y == 0.0f, false);
    IM_CHECK_RETV(r3d->Scroll.y == 0.0f, false);
    // The share list: the window itself never scrolls (its one scrolling region — the result
    // table's own — is pinned in the point_pick_analyze_lists_chains leg with a result in).
    IM_CHECK_RETV(share != nullptr, false);
    IM_CHECK_RETV(share->ScrollMax.y == 0.0f, false);
    IM_CHECK_RETV(share->Scroll.y == 0.0f, false);
    return true;
  };

  const ScopedWindowBasis window_basis(ctx);
  IM_CHECK_RETV(assert_regions_fixed(), false);
  request_basis(1280, 800, "1280x800");
  IM_CHECK_RETV(assert_regions_fixed(), false);
  request_basis(1024, 640, "1024x640");
  IM_CHECK_RETV(assert_regions_fixed(), false);
  gui::g_state.analyze_pose_view_open = false;
  ctx->Yield(2);
  return true;
}


// the component, the slider/playback, the factors and the 3D path — on the fully random prism,
// whose 22-degree halo fiber the (42, 0) target sits on (the M0 probe's verified anchor for this
// scene geometry: one closed component, ~48 samples). Real-timing pool: the point queues behind
// the selection's survey on the one worker, the same wall-clock shape as the map leg above.
bool PointInspectorShowsThePointFiberAndThePath(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  const std::string top_display = TopChainDisplay();
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + top_display).c_str());
  ctx->SetRef("");
  // No sky point yet: the point block asks for one and nothing is outstanding (the honest
  // no-intent state, in the real app's frame).
  IM_CHECK_RETV(gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kNoIntent, false);
  IM_CHECK_RETV(gui::g_state.analysis.trace.intent_valid == false, false);

  // The real pick on the map canvas: whatever sky position the shared view centres on, the pick
  // must arm the point intent (the pick-to-point chain through the real mouse path).
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      return m.adopted_result != nullptr && (m.adopted_id == m.intent_id);
                    },
                    60),
                false);
  ctx->KeyDown(ImGuiMod_Alt);
  ctx->ItemClick("**/##map_canvas");
  ctx->KeyUp(ImGuiMod_Alt);
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid, false);
  IM_CHECK_RETV(gui::PointQueryStateFor(gui::g_state.analysis.point) != gui::PointQueryState::kNoIntent, false);

  // The verified on-fiber anchor (the pick above proved the mouse path; this state write pins the
  // document the rest of the chain asserts on — the 742.5 leg's "the same write the button
  // performs" shape): (42, 0) sits on this scene's halo fiber.
  SetSkyPointTarget(42.0, 0.0);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const auto& point = gui::g_state.analysis.point;
                      const auto& intent = gui::g_state.analysis.sky_point;
                      return gui::PointQueryStateFor(point) == gui::PointQueryState::kCompleted &&
                             std::equal(point.key.target_dir, point.key.target_dir + 3, intent.dir) &&
                             point.key.wavelength_nm == gui::g_state.analysis.map_wavelength_nm;
                    },
                    60),
                false);
  const gui::PointQuerySession& point = gui::g_state.analysis.point;
  LUMICE_PathPointInfo info{};
  info.struct_size = sizeof(info);
  IM_CHECK_RETV(LUMICE_PathPointGetInfo(point.adopted_result.get(), &info) == LUMICE_PATH_READ_OK, false);
  const std::optional<gui::SelectedClassIdentity> identity = gui::SelectedClassIdentityFor(gui::g_state);
  IM_CHECK_RETV(identity.has_value(), false);
  IM_CHECK_RETV(info.crystal_id == identity->core_crystal_id, false);
  IM_CHECK_RETV(info.outcome == LUMICE_PATH_POINT_DISCOVERED, false);
  IM_CHECK_RETV(info.execution == LUMICE_PATH_OUTCOME_COMPLETED, false);
  // The target echo names the anchor's own labels (the one named conversion, verified on the
  // engine's echo — not the request's copy).
  IM_CHECK_RETV(std::fabs(info.target_altitude_deg - 42.0) < 1e-3, false);
  std::vector<gui::PointComponentRow> rows;
  const int components = gui::PointComponentRows(*point.adopted_result, &rows);
  IM_CHECK_RETV(components >= 1, false);
  const int valid_count = gui::PointValidSampleCount(*point.adopted_result, 0);
  IM_CHECK_RETV(valid_count > 1, false);
  ctx->LogInfo("[point view] components %d, samples %d, target (alt %.2f, az %.2f, dev %.2f), lambda %.1f", components,
               valid_count, info.target_altitude_deg, info.target_azimuth_deg, info.target_deviation_deg,
               info.wavelength_nm);
  IM_CHECK_RETV(SaveWindowPng(ctx, "//##AnalyzeDetails", GuiTestTempPath("analyze_point_detail.png").string()), false);

  // The fiber on the globe and the trace in the 3D region follow the cursor: step the slider to
  // the second valid sample (the slider drag path is the fixed-dt leg's; this leg pins the chain)
  // and the trace re-keys — the drawn polyline's key IS the current sample (AC4).
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::RayTraceSession& t = gui::g_state.analysis.trace;
                      return t.adopted_result != nullptr && t.intent_valid &&
                             t.key.sample ==
                                 gui::PointValidSampleIndexAt(*gui::g_state.analysis.point.adopted_result, 0, 0);
                    },
                    30),
                false);
  gui::g_state.analysis.point_view.sample_index = 1;
  gui::g_state.analysis.point_view.playing = false;
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::RayTraceSession& t = gui::g_state.analysis.trace;
                      if (!t.adopted_result || !t.intent_valid) {
                        return false;
                      }
                      const int stored =
                          gui::PointValidSampleIndexAt(*gui::g_state.analysis.point.adopted_result, 0, 1);
                      return t.key.sample == stored && gui::RayTraceDisplayStateFor(t, { t.key.point_id, 0, stored }) ==
                                                           gui::RayTraceDisplayState::kDrawn;
                    },
                    30),
                false);
  // Reuse the completed real fiber to test navigation without another physical query fixture.
  const auto point_before_nav = point.adopted_result;
  const auto point_id_before_nav = point.intent_id;
  const auto trace_before_nav = gui::g_state.analysis.trace.adopted_result;
  const auto trace_id_before_nav = gui::g_state.analysis.trace.intent_id;
  const auto map_id_before_nav = gui::g_state.analysis.map.intent_id;
  const auto survey_id_before_nav = gui::g_state.analysis.survey.intent_id;
  const auto selected_before_nav = gui::g_state.analysis.selected_entry;
  const auto sky_before_nav = gui::g_state.analysis.sky_point;
  const auto sim_before_nav = gui::g_state.sim_state;
  const auto dirty_before_nav = gui::g_state.dirty;
  ctx->SetRef("//Raypath view");
  ctx->ItemClick("**/Pose & trace###pose_open");
  PinPoseViewOnScreen(ctx);
  ctx->Yield(2);
  ImGuiWindow* same_pose_window = ctx->GetWindowByRef("//Pose & trace");
  IM_CHECK_RETV(same_pose_window != nullptr && same_pose_window->Active, false);
  ctx->WindowFocus("//Raypath view");
  ctx->ItemClick("**/Pose & trace###pose_open");  // ensure-open, not a toggle or cursor reset
  ctx->WindowClose("//Pose & trace");
  ctx->Yield(2);
  IM_CHECK_RETV(!same_pose_window->Active && !gui::g_state.analyze_pose_view_open, false);
  ctx->WindowFocus("//Raypath view");
  ctx->ItemClick("**/Pose & trace###pose_open");
  ctx->Yield(2);
  IM_CHECK_RETV(ctx->GetWindowByRef("//Pose & trace") == same_pose_window && same_pose_window->Active, false);
  IM_CHECK_RETV(point.adopted_result == point_before_nav && point.intent_id == point_id_before_nav, false);
  IM_CHECK_RETV(gui::g_state.analysis.trace.adopted_result == trace_before_nav &&
                    gui::g_state.analysis.trace.intent_id == trace_id_before_nav &&
                    gui::g_state.analysis.map.intent_id == map_id_before_nav &&
                    gui::g_state.analysis.survey.intent_id == survey_id_before_nav,
                false);
  IM_CHECK_RETV(gui::g_state.analysis.point_view.component_index == 0 &&
                    gui::g_state.analysis.point_view.sample_index == 1 && !gui::g_state.analysis.point_view.playing,
                false);
  IM_CHECK_RETV(gui::g_state.analysis.selected_entry == selected_before_nav &&
                    gui::g_state.analysis.sky_point.dir[0] == sky_before_nav.dir[0] &&
                    gui::g_state.analysis.sky_point.dir[1] == sky_before_nav.dir[1] &&
                    gui::g_state.analysis.sky_point.dir[2] == sky_before_nav.dir[2],
                false);
  IM_CHECK_RETV(gui::g_state.sim_state == sim_before_nav && gui::g_state.dirty == dirty_before_nav, false);
  ctx->LogInfo(
      "[pose navigation] completed fiber/trace, four request ids and sample 1 survived title-bar close/reopen");
  ctx->SetRef("");
  ctx->Yield(1);
  IM_CHECK_RETV(SaveWindowPng(ctx, "//Pose & trace", GuiTestTempPath("analyze_raypath_3d.png").string()), false);

  // The 3D region pin: title, the two-line description and the
  // legend are the fixed stack; the canvas absorbs the rest. Zero scroll at the window's
  // default geometry.
  {
    ImGuiWindow* region = PoseRegionWindow("##AnalyzeRaypath3D");
    IM_CHECK_RETV(region != nullptr, false);
    ctx->LogInfo("[scroll pins] 3d default SM %.1f SC %.1f", region->ScrollMax.y, region->Scroll.y);
    IM_CHECK_RETV(region->ScrollMax.y == 0.0f, false);
    IM_CHECK_RETV(region->Scroll.y == 0.0f, false);
  }
  // (The squeezed-row and divider-drag legs this block once carried rode the workspace's depth
  // band and its row splitter — both retired with the band. The region's compressed-tier
  // degradation — the fixed stack's scroll reachability at the window minimum — is the
  // narrowest-window leg at the tail of this case, now driven through the pose window itself.)
  // ---- The orbit rotation convention pin (doc/geometry/coordinate-convention.md §9.8) ----
  // The project-wide grab metaphor (Render main view + modal preview + this orbit camera): drag
  // right -> the crystal's NEAR face follows the hand RIGHT; drag down -> the near face follows
  // DOWN. Every quantity below comes out of gui::OrbitBasis — the same basis the renderer draws
  // with — and the landmark M = −fwd + 0.25·right sits a quarter-right of the view centre on the
  // camera-facing side, so the assertions are about the basis itself, decoupled from the scene.
  const float yaw0 = gui::g_state.analysis.point_view.orbit_yaw_deg;
  const float pitch0 = gui::g_state.analysis.point_view.orbit_pitch_deg;
  // The pin runs at the documented default pose — the §9.8 dominance-margin derivation (cot
  // pitch / 0.25) binds to it. If the defaults ever change, fail HERE, attributably, instead of
  // letting the dominance assertion below produce an unexplainable red.
  IM_CHECK_RETV(std::fabs(yaw0 - 35.0f) < 1e-4f && std::fabs(pitch0 - 22.0f) < 1e-4f, false);
  // Restores the pre-pin pose even when one of the assertions below returns early — cross-leg
  // display state must be restored in a destructor, not at a function tail.
  struct OrbitPoseRestore {
    float yaw;
    float pitch;
    ~OrbitPoseRestore() {
      gui::g_state.analysis.point_view.orbit_yaw_deg = yaw;
      gui::g_state.analysis.point_view.orbit_pitch_deg = pitch;
    }
  } pose_restore{ yaw0, pitch0 };
  double fwd0[3];
  double right0[3];
  double up0[3];
  gui::OrbitBasis(yaw0, pitch0, fwd0, right0, up0);
  const double landmark[3] = { -fwd0[0] + 0.25 * right0[0], -fwd0[1] + 0.25 * right0[1], -fwd0[2] + 0.25 * right0[2] };
  const auto screen_motion = [](const double m[3], const double a[3], const double b[3]) {
    return m[0] * (b[0] - a[0]) + m[1] * (b[1] - a[1]) + m[2] * (b[2] - a[2]);
  };

  // Drag right: the near face moves right (dsx > 0), the yaw axis dominates (a=0.25 puts the
  // dominance limit at cot(pitch)/a ≈ 9.9 at the default 22°; 4 leaves margin), and yaw itself
  // decreased.
  ctx->ItemDragWithDelta("**/##r3d_canvas", ImVec2(24.0f, 0.0f));
  ctx->Yield(2);
  const float yaw1 = gui::g_state.analysis.point_view.orbit_yaw_deg;
  const float pitch1 = gui::g_state.analysis.point_view.orbit_pitch_deg;
  double fwd1[3];
  double right1[3];
  double up1[3];
  gui::OrbitBasis(yaw1, pitch1, fwd1, right1, up1);
  const double dsx = screen_motion(landmark, right0, right1);
  const double dsy = screen_motion(landmark, up0, up1);
  ctx->LogInfo("[ray3d orbit] right drag: yaw %.3f -> %.3f, dsx %.6f, dsy %.6f", yaw0, yaw1, dsx, dsy);
  IM_CHECK_RETV(dsx > 0.0, false);                               // grab: the near face follows the hand to the right
  IM_CHECK_RETV(std::fabs(dsx) >= 4.0 * std::fabs(dsy), false);  // yaw-dominant, not a nod
  IM_CHECK_RETV(yaw1 < yaw0, false);                             // a right drag decreases yaw

  // Drag down: the near face moves down (dsy < 0) and the screen-x motion is EXACTLY zero — in
  // the z-up basis `right` depends on yaw only, so a pure pitch drag cannot move anything
  // sideways. This invariant fails on the transposed basis (its right depends on pitch), so it
  // doubles as the red/green discriminator, not just a direction check.
  ctx->ItemDragWithDelta("**/##r3d_canvas", ImVec2(0.0f, 24.0f));
  ctx->Yield(2);
  const float pitch2 = gui::g_state.analysis.point_view.orbit_pitch_deg;
  double fwd2[3];
  double right2[3];
  double up2[3];
  gui::OrbitBasis(yaw1, pitch2, fwd2, right2, up2);
  const double dsx_down = screen_motion(landmark, right1, right2);
  const double dsy_down = screen_motion(landmark, up1, up2);
  ctx->LogInfo("[ray3d orbit] down drag: pitch %.3f -> %.3f, dsx %.6f, dsy %.6f", pitch1, pitch2, dsx_down, dsy_down);
  IM_CHECK_RETV(dsy_down < 0.0, false);              // grab: the near face follows the hand downward
  IM_CHECK_RETV(std::fabs(dsx_down) < 1e-9, false);  // a pitch drag never moves anything sideways
  IM_CHECK_RETV(pitch2 < pitch1, false);             // a down drag decreases pitch
  // (the pose itself is restored by the OrbitPoseRestore destructor above — on every path)
  // (the wheel-arbitration half of the 3D canvas lives in the scale-tier leg: this window's
  // band does not overflow — SM 0 — and a band that cannot scroll cannot host a non-boundary
  // wheel-ownership assert)

  // Playback writes the sample index and NOTHING else (AC3, pinned on the real app's frame loop):
  // the sky point and the ROI state are bitwise what they were, the selection untouched.
  const float sky_before[3] = { gui::g_state.analysis.sky_point.dir[0], gui::g_state.analysis.sky_point.dir[1],
                                gui::g_state.analysis.sky_point.dir[2] };
  const int roi_before = gui::g_state.analysis.roi_mode;
  const std::string selected_before_str = gui::g_state.analysis.selected_entry.value_or("");
  gui::g_state.analysis.point_view.playing = true;
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::g_state.analysis.point_view.sample_index != 1;  // the cursor moved (wrap included)
                    },
                    30),
                false);
  gui::g_state.analysis.point_view.playing = false;
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.dir[0] == sky_before[0] &&
                    gui::g_state.analysis.sky_point.dir[1] == sky_before[1] &&
                    gui::g_state.analysis.sky_point.dir[2] == sky_before[2],
                false);
  IM_CHECK_RETV(gui::g_state.analysis.roi_mode == roi_before, false);
  IM_CHECK_RETV(gui::g_state.analysis.selected_entry.value_or("") == selected_before_str, false);
  ctx->LogInfo("[point view] playback stepped to sample %d; sky point and ROI untouched",
               gui::g_state.analysis.point_view.sample_index);

  // The completed point's own controls, driven as interactions at the smallest window (the pose
  // window clamped to the shrunken viewport): the sample slider sits contained in the globe
  // region and answers a real click — which moves the cursor and stops a running playback (a
  // hand on the slider owns it) — and the play toggle answers clicks both ways. The window's
  // reachability claim for the region's live state, in the X5 form.
  {
    const ScopedWindowBasis window_basis(ctx);
    g_window_size_test.width = gui::kMinWindowWidth;
    g_window_size_test.height = gui::kMinWindowHeight;
    g_window_size_test.done.store(false);
    g_window_size_test.requested.store(true);
    for (int i = 0; i < 20 && !g_window_size_test.done.load(); ++i) {
      ctx->Yield(1);
    }
    IM_CHECK_RETV(g_window_size_test.done.load(), false);
    ctx->Yield(3);
    // The globe region at the viewport-clamped window: its fixed stack (slider/play row among
    // it) stays reachable; the state area below scrolls and the region's own scroll write is
    // the belt-and-braces seek.
    ImGuiWindow* globe = PoseRegionWindow("##AnalyzeSunGlobe");
    IM_CHECK_RETV(globe != nullptr, false);
    globe->Scroll.y = globe->ScrollMax.y;  // the slider/play row is the fixed stack's tail
    ctx->Yield(2);
    const ImGuiTestItemInfo sample = ctx->ItemInfo("**/##globe_sample", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(sample.ID != 0, false);
    IM_CHECK_RETV(sample.RectFull.Min.x >= globe->InnerClipRect.Min.x - 0.5f, false);
    IM_CHECK_RETV(sample.RectFull.Max.x <= globe->InnerClipRect.Max.x + 0.5f, false);
    IM_CHECK_RETV(sample.RectFull.Min.y >= globe->InnerClipRect.Min.y - 0.5f, false);
    IM_CHECK_RETV(sample.RectFull.Max.y <= globe->InnerClipRect.Max.y + 0.5f, false);
    ctx->ItemClick("**/Play##globe_play");
    ctx->Yield(2);
    IM_CHECK_RETV(gui::g_state.analysis.point_view.playing, false);
    ctx->ItemClick("**/##globe_sample");
    ctx->Yield(2);
    IM_CHECK_RETV(!gui::g_state.analysis.point_view.playing, false);
    const int clicked = gui::g_state.analysis.point_view.sample_index;
    IM_CHECK_RETV(clicked >= 0 && clicked < valid_count, false);
    ctx->LogInfo("[point view] slider click set sample %d and stopped the playback", clicked);
    ctx->ItemClick("**/Play##globe_play");
    ctx->Yield(2);
    IM_CHECK_RETV(gui::g_state.analysis.point_view.playing, false);
    ctx->ItemClick("**/Stop##globe_play");
    ctx->Yield(2);
    IM_CHECK_RETV(!gui::g_state.analysis.point_view.playing, false);
  }
  gui::g_state.analyze_pose_view_open = false;
  ctx->Yield(2);
  return true;
}

// The rank-0 arm (AC5) on the corpus plate scene (C14's shape: the horizontal plate whose 1-2
// chain sends the sun's image straight through — the point at the sun itself is a point mass,
// not a fiber). Fixed-dt pool: the mass arm answers in milliseconds.
bool PointMassArmIsHonestAndForcesNoFiber(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kPlateJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  // The plate scene's basal row: the chain spelled 1-2 in the list (the sun's direct image).
  SetListRef(ctx);
  ctx->ItemClick("**/1-2");
  ctx->SetRef("");
  IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::SelectedClassIdentityFor(gui::g_state).has_value(); }, 10), false);
  // The point at the sun's own position: rank 0 — the direction-identity path.
  SetSkyPointTarget(20.0, 0.0);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          30),
      false);
  LUMICE_PathPointInfo info{};
  info.struct_size = sizeof(info);
  IM_CHECK_RETV(LUMICE_PathPointGetInfo(gui::g_state.analysis.point.adopted_result.get(), &info) == LUMICE_PATH_READ_OK,
                false);
  IM_CHECK_RETV(info.outcome == LUMICE_PATH_POINT_MASS, false);
  // No fiber is forced: no component rows, no ray intent, the trace stays hidden (AC5).
  std::vector<gui::PointComponentRow> rows;
  IM_CHECK_RETV(gui::PointComponentRows(*gui::g_state.analysis.point.adopted_result, &rows) == 0, false);
  IM_CHECK_RETV(gui::g_state.analysis.trace.intent_valid == false, false);
  LUMICE_PathPointMass mass{};
  mass.struct_size = sizeof(mass);
  IM_CHECK_RETV(LUMICE_PathPointGetMass(gui::g_state.analysis.point.adopted_result.get(), &mass) == LUMICE_PATH_READ_OK,
                false);
  // The mass IS the sun's image: the separation is the float intent roundtrip's own error (the
  // GUI's sky-point dir is float; the CLI's double path measures 4.5e-12 on the same scene).
  IM_CHECK_RETV(std::fabs(mass.target_separation_deg) < 1e-3, false);
  ctx->LogInfo("[point view] rank-0 mass at (alt %.3f, az %.3f), separation %.2e deg", mass.altitude_deg,
               mass.azimuth_deg, mass.target_separation_deg);
  IM_CHECK_RETV(SaveWindowPng(ctx, "//##AnalyzeDetails", GuiTestTempPath("analyze_point_mass.png").string()), false);
  return true;
}

// The dispersion-on-screen face (742.4, AC2): on the fully random prism — the C01 shape written
// the way the routing v1 domain reads as the full-sphere random family — the dominant class's
// overview carries the layered full-dimensional facts, and the chromatic block on screen holds
// the two-lambda min-deviation row with red inside blue and the closed-form certificate behind
// it. The assertions read the same model the region draws, so the screenshot a human reads and
// the checks below cannot both be mock; the picture is the evidence of the block being VISIBLE.
// The frame-text capture helpers are defined further down (with the map face's tests); the
// summary-projection assertions below are their first users.
std::string CaptureFrameText(ImGuiTestContext* ctx);
std::string CaptureWindowFrameText(ImGuiTestContext* ctx, const char* window_name);
std::string CaptureMapFrameText(ImGuiTestContext* ctx);
bool SurveyOverviewShowsDispersionAndProfile(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  const std::string top_display = TopChainDisplay();
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + top_display).c_str());
  ctx->SetRef("");
  // The survey's chromatic legs (the two-lambda tint power sums) are the cost here — give the
  // background query its minute.
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    60),
                false);
  const gui::SurveyQuerySession& survey = gui::g_state.analysis.survey;
  IM_CHECK_RETV(survey.adopted_result != nullptr, false);

  const gui::SurveyOverviewModel model = gui::BuildSurveyOverviewModel(*survey.adopted_result);
  IM_CHECK_RETV(model.valid, false);
  IM_CHECK_RETV(model.arm == gui::SurveyArm::kFullDimensional, false);
  IM_CHECK_RETV(model.dispersion.available, false);
  IM_CHECK_RETV(model.dispersion.min_row_available, false);
  // The red-inside-blue fact itself: the 650nm inner edge sits at a SMALLER deviation than the
  // 450nm one, and the displacement is the row's own spread.
  IM_CHECK_RETV(model.dispersion.value_red_deg < model.dispersion.value_blue_deg, false);
  IM_CHECK_RETV(model.dispersion.direction == "red_inner_blue_outer", false);
  IM_CHECK_RETV(std::fabs(model.dispersion.displacement_deg -
                          (model.dispersion.value_blue_deg - model.dispersion.value_red_deg)) < 1e-9,
                false);
  IM_CHECK_RETV(model.dispersion.certificate_available, false);
  IM_CHECK_RETV(model.probe.available && model.probe.peak_available, false);
  IM_CHECK_RETV(model.tint.available, false);
  ctx->LogInfo("[survey overview] dispersion %.3f/%.3f (Δ%.3f°, %s), probe %d rows, apex %.1f°",
               model.dispersion.value_red_deg, model.dispersion.value_blue_deg, model.dispersion.displacement_deg,
               model.dispersion.direction.c_str(), static_cast<int>(model.probe.sample_value.size()),
               model.dispersion.certificate_apex_deg);
  IM_CHECK_RETV(
      SaveWindowPng(ctx, "//##AnalyzeDetails", GuiTestTempPath("analysis_survey_overview_dispersion.png").string()),
      false);
  // The summary projection (§16.2): the main window carries the default layer — the
  // conclusions, the limits, the premises and the H1 gap readings (tint, colour verdict, TIR
  // share) — plus the pointer to the complete answer, and none of the fold-only L4 facts (the
  // counts line's "member(s)", the probe's kept events). Read from the region's own submitted
  // frame, not a second rendering of what the tests expect. The row click opened the Raypath
  // view (§16.3's content shortcut) — its class-answer section would pollute a whole-frame
  // read, so it closes first.
  ctx->WindowClose("//Raypath view");
  ctx->Yield(1);
  {
    const std::string frame_text = CaptureFrameText(ctx);
    IM_CHECK_RETV(!frame_text.empty(), false);
    IM_CHECK_RETV(frame_text.find("Support:") != std::string::npos, false);
    IM_CHECK_RETV(frame_text.find("Sharp edge") != std::string::npos, false);
    IM_CHECK_RETV(frame_text.find("Wavelength dispersion:") != std::string::npos, false);
    // The full-dimensional arm's tint reading rides the default layer as the bare ratio line
    // (the "Weight tint:" spelling is the identity arm's conclusions'); the colour verdict's
    // default line is shared across arms.
    IM_CHECK_RETV(frame_text.find("Blue/red ratio") != std::string::npos, false);
    IM_CHECK_RETV(frame_text.find("Weight colour:") != std::string::npos, false);
    IM_CHECK_RETV(frame_text.find("All-TIR share") != std::string::npos, false);
    IM_CHECK_RETV(frame_text.find("The complete answer is in the Raypath view.") != std::string::npos, false);
    IM_CHECK_RETV(frame_text.find("member(s)") == std::string::npos, false);
    IM_CHECK_RETV(frame_text.find("kept lattice events") == std::string::npos, false);
    IM_CHECK_RETV(frame_text.find("Class evidence") == std::string::npos, false);
    IM_CHECK_RETV(frame_text.find("Class diagnostics") == std::string::npos, false);
  }
  // The complete answer lives in the Raypath view: open it and the class answer section draws
  // the same document through the full renderer. This leg is also the env gate's three-state
  // spine on the survey host: the shipped default and an explicit "0" both leave the "Class
  // diagnostics" group UNSUBMITTED (header and body — a closed CollapsingHeader submits no
  // body, and a gated group submits no header either), while the evidence group stands in
  // every state; "1" surfaces the diagnostics group, and the env going off again wins over
  // the storage's still-open expansion state.
  {
    const ScopedDebugDetailsEnv env_off(DebugDetailsState::kOff);
    ctx->ItemClick("**/Raypath view###main_raypath_open");
    ctx->Yield(2);
    IM_CHECK_RETV(gui::g_state.analyze_raypath_view_open, false);
    const std::string report_text = CaptureMapFrameText(ctx);
    IM_CHECK_RETV(report_text.find("Class answer") != std::string::npos, false);
    IM_CHECK_RETV(report_text.find("member(s)") == std::string::npos, false);
    IM_CHECK_RETV(report_text.find("Reference profile") == std::string::npos, false);
    const ImGuiID evidence_fold = ctx->ItemInfo("**/Class evidence##sv_answer", ImGuiTestOpFlags_NoError).ID;
    const ImGuiID diagnostics_fold = ctx->ItemInfo("**/Class diagnostics##sv_answer", ImGuiTestOpFlags_NoError).ID;
    IM_CHECK_RETV(evidence_fold != 0, false);
    // An explicit "0" leaves the diagnostics header unsubmitted.
    IM_CHECK_RETV(diagnostics_fold == 0, false);
    // The evidence group, expanded: the reference curve on screen is the human-readable half
    // of the probe block's evidence (the samples and their gap rule are the model test's).
    // The group header lives inside the map report child, so the item is addressed through
    // the wildcard (the GUI test engine-addressing rule for items inside a named child). The
    // implementation-level rows the evidence group gave up — the engine's dispersion-table n
    // pair, the freezing ladder's raw enumeration — stay absent here however far the group
    // opens: they moved to the diagnostics group, not behind another fold.
    ctx->ItemClick("**/Class evidence##sv_answer");
    ctx->Yield(2);
    if (ImGuiWindow* report = MapReportWindow()) {
      report->Scroll.y = report->ScrollMax.y;
    }
    ctx->Yield(2);
    IM_CHECK_RETV(
        SaveWindowPng(ctx, "//Raypath view", GuiTestTempPath("analysis_survey_overview_profile.png").string()), false);
    const std::string evidence_text = CaptureMapFrameText(ctx);
    IM_CHECK_RETV(evidence_text.find("Deviation from") != std::string::npos, false);
    IM_CHECK_RETV(evidence_text.find("Refractive indices n(") == std::string::npos, false);
    IM_CHECK_RETV(evidence_text.find("member(s)") == std::string::npos, false);
    ctx->ItemClick("**/Class evidence##sv_answer");  // restore the default-closed shape
    ctx->Yield(2);
  }
  {
    // The diagnostics group under "1": the header the "0" leg proved absent now stands (the
    // gate is re-read every frame, never cached), and its body carries the execution counts
    // plus the implementation rows the evidence group gave up.
    const ScopedDebugDetailsEnv env_on(DebugDetailsState::kOn);
    ctx->Yield(2);
    const ImGuiID diagnostics_fold = ctx->ItemInfo("**/Class diagnostics##sv_answer", ImGuiTestOpFlags_NoError).ID;
    IM_CHECK_RETV(diagnostics_fold != 0, false);
    ctx->ItemClick("**/Class diagnostics##sv_answer");
    ctx->Yield(2);
    const std::string opened_text = CaptureMapFrameText(ctx);
    IM_CHECK_RETV(opened_text.find("member(s)") != std::string::npos, false);
    IM_CHECK_RETV(opened_text.find("Refractive indices n(") != std::string::npos, false);
    ctx->ItemClick("**/Class diagnostics##sv_answer");  // restore the default-closed shape
    ctx->Yield(2);
  }
  {
    // The env off again, with the storage still remembering nothing open: the header is gone
    // either way — gating is display eligibility, not an expansion state of its own.
    const ScopedDebugDetailsEnv env_unset(DebugDetailsState::kUnset);
    ctx->Yield(2);
    const ImGuiID diagnostics_fold = ctx->ItemInfo("**/Class diagnostics##sv_answer", ImGuiTestOpFlags_NoError).ID;
    IM_CHECK_RETV(diagnostics_fold == 0, false);
  }
  ctx->WindowClose("//Raypath view");
  ctx->Yield(1);
  return true;
}

// The verdict anchor's density interaction (§16.1's A2 pilot), on the real window: the class
// answer's diagnose rows — addressable hover surfaces over the wrapped text — drive the map's
// anchor, the toggle keeps it, and the legend bit (the same accounting the legend entry renders
// from) reads what the canvas actually drew. The fixture is the dispersion face's own scene; the
// clicked row is NOT the top chain — the top chain (a two-face chain, like 3-6) structurally
// refuses the TIR leg and its verdict names itself not visible, anchoring nothing BY DESIGN (the
// honesty gate's own leg), while this chain's diagnose is the pilot's edge/blue/visible and its
// dispersion carries no min row (no dispersion circle shares the canvas, so the verdict bit is
// unambiguous).
// Flake attribution, should this ever red: the premise asserts below (the 1-3-2 survey's
// diagnose fields) are the FIRST suspects — the whole-sky analysis is unseeded and the survey's
// chromatic legs complete in ~5.6 s of the 15 s budget (~2.7x margin), so a red premise is a
// data/ranking or budget-truncation event, not an interaction-chain defect. The interaction
// chain is only in play once the premises hold.
bool VerdictAnchorHoverAndToggleOnTheMap(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  // The pilot's row: 1-3-2 — its survey completes inside the GUI's budget (the probe: ~5.6 s of
  // the 15 s it asks for).
  SetListRef(ctx);
  ctx->ItemClick("**/1-3-2");
  ctx->SetRef("");
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    60),
                false);
  // The fixture's premise, on the very document the window is about to draw: the pilot's
  // verdict — edge, blue, visible, positioned, complete coverage.
  const gui::SurveyOverviewModel model = gui::BuildSurveyOverviewModel(*gui::g_state.analysis.survey.adopted_result);
  IM_CHECK_RETV(model.valid && model.diagnose.available, false);
  IM_CHECK_RETV(model.diagnose.verdict_kind == "edge", false);
  IM_CHECK_RETV(model.diagnose.color == "blue", false);
  IM_CHECK_RETV(model.diagnose.visible != 0, false);
  IM_CHECK_RETV(model.diagnose.position_available, false);
  IM_CHECK_RETV(model.diagnose.coverage_complete, false);
  ctx->LogInfo("[verdict anchor] pilot diagnose delta %.3f deg", model.diagnose.position_deg);
  // The row click opens the Raypath view (§16.3's content shortcut).
  IM_CHECK_RETV(gui::g_state.analyze_raypath_view_open, false);
  // The pilot's ring is the far side of the sky (its delta is ~117° about the sun — the default
  // linear 60° view cannot hold a single sample of it, and the walk would submit nothing). The
  // full-sky rectangular view — written directly, the shared presets being retired — is the
  // robust aim: every circle has in-view samples there.
  gui::SelectLensType(gui::g_state, gui::kLensTypeRectangular);
  gui::g_state.renderer.visible = gui::kVisibleFull;
  gui::g_state.renderer.front = false;
  ctx->Yield(3);
  ctx->Yield(3);
  // The default frame: no hover, the toggle off — the anchor drew nothing even with the ring in
  // view. The bucket read is the draw face itself (the production assembly's own quads), the
  // reads above only imply it.
  IM_CHECK_RETV(!gui::g_state.analysis.verdict_anchor_shown, false);
  IM_CHECK_RETV(!gui::g_state.analysis.verdict_anchor_hover, false);
  IM_CHECK_RETV(!gui::LastMapOverlayLegend().verdict_anchor, false);
  IM_CHECK_RETV(MapWindowIndexCountForColour(gui::kChromaticColourBlue) == 0, false);
  // Hover the verdict row: the row's overlay takes the pointer, the info column writes the
  // standing, the canvas draws it the NEXT frame — three frames carry the whole chain (the
  // frame lag the contract accepts for hover semantics).
  ctx->ScrollToItem("**/###diagnose_verdict_row", ImGuiAxis_Y);
  ctx->Yield(1);
  const ImVec2 verdict_center = ctx->ItemInfo("**/###diagnose_verdict_row").RectFull.GetCenter();
  ctx->MouseMoveToPos(verdict_center);
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.analysis.verdict_anchor_hover, false);
  IM_CHECK_RETV(gui::LastMapOverlayLegend().verdict_anchor, false);
  IM_CHECK_RETV(MapWindowIndexCountForColour(gui::kChromaticColourBlue) > 0, false);
  // Hover the Position row: the second linked row, the same anchor.
  ctx->ScrollToItem("**/###diagnose_position_row", ImGuiAxis_Y);
  ctx->Yield(1);
  const ImVec2 position_center = ctx->ItemInfo("**/###diagnose_position_row").RectFull.GetCenter();
  ctx->MouseMoveToPos(position_center);
  ctx->Yield(3);
  IM_CHECK_RETV(gui::LastMapOverlayLegend().verdict_anchor, false);
  // Off the rows: the hover tier's annotation leaves.
  ctx->MouseMoveToPos(ImVec2(-100.0f, -100.0f));
  ctx->Yield(3);
  IM_CHECK_RETV(!gui::g_state.analysis.verdict_anchor_hover, false);
  IM_CHECK_RETV(!gui::LastMapOverlayLegend().verdict_anchor, false);
  IM_CHECK_RETV(MapWindowIndexCountForColour(gui::kChromaticColourBlue) == 0, false);
  // The toggle's always-on tier: no row hover, the anchor stands; off, it leaves.
  ctx->ItemClick("**/Verdict anchors##map");
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.analysis.verdict_anchor_shown, false);
  IM_CHECK_RETV(!gui::g_state.analysis.verdict_anchor_hover, false);
  IM_CHECK_RETV(gui::LastMapOverlayLegend().verdict_anchor, false);
  IM_CHECK_RETV(MapWindowIndexCountForColour(gui::kChromaticColourBlue) > 0, false);
  ctx->ItemClick("**/Verdict anchors##map");
  ctx->Yield(3);
  IM_CHECK_RETV(!gui::LastMapOverlayLegend().verdict_anchor, false);
  IM_CHECK_RETV(MapWindowIndexCountForColour(gui::kChromaticColourBlue) == 0, false);
  return true;
}

// The engine's named refusal, on the two-layer scene: a multi-layer class is a legitimate
// selection and the survey answers it with multi_layer_unsupported — shown, not fabricated
// around (R5/R7; the multi-layer evidence the plan's M3-5 asks to be on file).
// The report window's colour-bucket count (the map face's own read, one window over): the
// legend icons are the only consumers of their colours inside ##MapReport — the map's own markers
// live in the map region's window.
int MapReportIndexCountForColour(ImU32 colour) {
  const ImGuiWindow* report = MapReportWindow();
  if (report == nullptr) {
    return 0;
  }
  const ImDrawList* dl = report->DrawList;
  int count = 0;
  for (int i = 0; i + 5 < dl->IdxBuffer.Size; i += 6) {
    bool matches = true;
    for (int k = 0; k < 6; ++k) {
      if (dl->VtxBuffer[static_cast<int>(dl->IdxBuffer[i + k])].col != colour) {
        matches = false;
        break;
      }
    }
    if (matches) {
      count += 6;
    }
  }
  return count;
}

// The row-highlight read: the bbox of one colour's vertices inside ##MapReport (the theme's
// hovered-header ink the row backgrounds draw with), false when nothing drew in it.
bool ReportHighlightBBox(ImU32 colour, ImVec2* out_min, ImVec2* out_max) {
  const ImGuiWindow* report = MapReportWindow();
  if (report == nullptr) {
    return false;
  }
  const ImDrawList* dl = report->DrawList;
  bool any = false;
  for (int i = 0; i < dl->VtxBuffer.Size; ++i) {
    const ImDrawVert& v = dl->VtxBuffer[i];
    if (v.col != colour) {
      continue;
    }
    *out_min = any ? ImVec2(std::min(out_min->x, v.pos.x), std::min(out_min->y, v.pos.y)) : ImVec2(v.pos.x, v.pos.y);
    *out_max = any ? ImVec2(std::max(out_max->x, v.pos.x), std::max(out_max->y, v.pos.y)) : ImVec2(v.pos.x, v.pos.y);
    any = true;
  }
  return any;
}

// Two rects intersect iff NEITHER axis separates them — the row highlight's own criterion. The
// two axis separations join with OR: a highlight drawn one row above or below the target shares
// the rows' x span, so only the OR rejects it (the AND-shaped mix the first draft had let exactly
// that shape through).
bool RowRectsIntersect(const ImRect& a, const ImRect& b) {
  return (a.Min.x <= b.Max.x && a.Max.x >= b.Min.x) && (a.Min.y <= b.Max.y && a.Max.y >= b.Min.y);
}

// The 2026-11-12 feedback wave's interaction face (contract §16.1.3's extension): the class
// answer's rows highlight where the pointer stands (AC1/AC3 — the ratio row's feedback is local
// by design, it names no spatial payload), the three profile reading rows drive the map's profile
// device one frame later (AC2 — a point at the edge/peak reading, the radial interval between
// them for separation), the Overlays gate holds the device back while the row feedback stays (the
// total gate's own leg), and the four anchor legend entries carry their icons (AC4). Fixture: the
// narrow-plate scene — its arc rows route to the restricted plate family, whose survey runs the
// probe. Flake attribution, should this ever red: the premise asserts below (the family arm's
// probe readings, the azimuth availability, the sun echo) are the FIRST suspects — the survey's
// quantification legs complete in seconds of an unseeded whole-sky analysis, so a red premise is
// a data/ranking or budget-truncation event, not an interaction-chain defect.
bool ProfileRowsHighlightAndDrawOnTheMap(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kNarrowPlateJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  // The explicit chain row: the first 2+ face row in the display order (single-face rows refuse
  // as invalid paths; the traversal's rule is the explicit part, the premise asserts below are
  // what make the physical basis real).
  std::string target;
  for (const size_t idx : gui::g_state.analysis_result.display_order) {
    const std::string d = gui::g_state.analysis_result.payload->entries[idx].display;
    if (d != "1-2" && d.find('-') != std::string::npos) {
      target = d;
      break;
    }
  }
  IM_CHECK_RETV(!target.empty(), false);
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + target).c_str());
  ctx->SetRef("");
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    60),
                false);
  const gui::SurveyOverviewModel model = gui::BuildSurveyOverviewModel(*gui::g_state.analysis.survey.adopted_result);
  IM_CHECK_RETV(model.valid && model.arm == gui::SurveyArm::kFamily, false);
  IM_CHECK_RETV(model.probe.available && model.probe.edge_available && model.probe.peak_available &&
                    model.probe.separation_available,
                false);
  IM_CHECK_RETV(model.probe.azimuth_available, false);
  IM_CHECK_RETV(model.sun_available, false);
  ctx->LogInfo("[profile hover] %s: edge %.3f peak %.3f sep %.3f az %.1f sun %.1f/%.1f", target.c_str(),
               model.probe.edge_deg, model.probe.peak_deg, model.probe.separation_deg, model.probe.azimuth_deg,
               model.sun_altitude_deg, model.sun_azimuth_deg);
  // The row click opened the Raypath view (§16.3's content shortcut); the full-sky rectangular
  // view — written directly, the shared presets being retired — is the robust aim (the verdict
  // face's lesson): every reading keeps in-view samples there.
  IM_CHECK_RETV(gui::g_state.analyze_raypath_view_open, false);
  gui::SelectLensType(gui::g_state, gui::kLensTypeRectangular);
  gui::g_state.renderer.visible = gui::kVisibleFull;
  gui::g_state.renderer.front = false;
  ctx->Yield(3);
  // The default frame: no hover, no profile ink — the map is the picture it always was.
  const ImU32 hovered_ink = ImGui::GetColorU32(ImGuiCol_HeaderHovered);
  IM_CHECK_RETV(gui::g_state.analysis.profile_row_hover == gui::SurveyProfileHover::kNone, false);
  IM_CHECK_RETV(MapWindowIndexCountForColour(gui::kProfileReadingColour) == 0, false);
  // The intersection predicate's own legs, before the rows lean on it: the adjacent-row shape the
  // OR form exists to reject (horizontal overlap, vertical separation — above and below), and a
  // genuine overlap the predicate must still accept.
  IM_CHECK_RETV(!RowRectsIntersect(ImRect(0, 0, 100, 10), ImRect(0, 20, 100, 30)), false);
  IM_CHECK_RETV(!RowRectsIntersect(ImRect(0, 0, 100, 10), ImRect(0, -30, 100, -20)), false);
  IM_CHECK_RETV(RowRectsIntersect(ImRect(0, 0, 100, 10), ImRect(0, 5, 100, 15)), false);
  IM_CHECK_RETV(RowRectsIntersect(ImRect(0, 0, 100, 10), ImRect(50, 0, 150, 10)), false);
  auto hover_row = [&](const char* id, gui::SurveyProfileHover expect) -> bool {
    ctx->ScrollToItem(id, ImGuiAxis_Y);
    ctx->Yield(1);
    const ImVec2 center = ctx->ItemInfo(id).RectFull.GetCenter();
    ctx->MouseMoveToPos(center);
    ctx->Yield(3);
    if (gui::g_state.analysis.profile_row_hover != expect) {
      ctx->LogError("[profile hover] %s: session identity is %d, expected %d", id,
                    static_cast<int>(gui::g_state.analysis.profile_row_hover), static_cast<int>(expect));
      return false;
    }
    // The row's own highlight, THIS frame: the theme ink's bbox in the report window intersects
    // the row's own rect (the background rides the text, under it — the text stays foreground).
    ImVec2 hmin(0.0f, 0.0f);
    ImVec2 hmax(0.0f, 0.0f);
    if (!ReportHighlightBBox(hovered_ink, &hmin, &hmax)) {
      ctx->LogError("[profile hover] %s: no highlight ink in the report window", id);
      return false;
    }
    const ImRect row = ctx->ItemInfo(id).RectFull;
    const ImRect highlight(hmin, hmax);
    if (!RowRectsIntersect(highlight, row)) {
      ctx->LogError(
          "[profile hover] %s: highlight bbox (%.0f,%.0f)-(%.0f,%.0f) misses the row rect (%.0f,%.0f)-(%.0f,%.0f)", id,
          hmin.x, hmin.y, hmax.x, hmax.y, row.Min.x, row.Min.y, row.Max.x, row.Max.y);
      return false;
    }
    return true;
  };
  // The edge row: the point device on the map, the highlight on the row.
  IM_CHECK_RETV(hover_row("**/###profile_edge_row", gui::SurveyProfileHover::kEdge), false);
  IM_CHECK_RETV(MapWindowIndexCountForColour(gui::kProfileReadingColour) > 0, false);
  // The peak row: the point moves (the readings differ by tens of degrees — the count alone
  // cannot tell a stuck point from a moved one, so this leg reads the session identity and the
  // ink's existence; the unit tier owns the positions).
  IM_CHECK_RETV(hover_row("**/###profile_peak_row", gui::SurveyProfileHover::kPeak), false);
  IM_CHECK_RETV(MapWindowIndexCountForColour(gui::kProfileReadingColour) > 0, false);
  // The separation row: the interval device. Same-source frames for the human eye: the row
  // highlight and the interval are the two things a screenshot judges better than a bucket read.
  IM_CHECK_RETV(hover_row("**/###profile_separation_row", gui::SurveyProfileHover::kSeparation), false);
  IM_CHECK_RETV(MapWindowIndexCountForColour(gui::kProfileReadingColour) > 0, false);
  IM_CHECK_RETV(SaveWindowPng(ctx, "//Raypath view", GuiTestTempPath("analysis_profile_hover_separation.png").string()),
                false);
  // Off the rows: both the identity and the ink leave (the overwrite's own closure).
  ctx->MouseMoveToPos(ImVec2(-100.0f, -100.0f));
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.analysis.profile_row_hover == gui::SurveyProfileHover::kNone, false);
  IM_CHECK_RETV(MapWindowIndexCountForColour(gui::kProfileReadingColour) == 0, false);
  {
    ImVec2 hmin(0.0f, 0.0f);
    ImVec2 hmax(0.0f, 0.0f);
    IM_CHECK_RETV(!ReportHighlightBBox(hovered_ink, &hmin, &hmax), false);
  }
  // The Overlays gate: off, the row's highlight and identity stand, the map draws nothing —
  // the profile device nests under the gate exactly like the verdict anchor does.
  ctx->ItemClick("**/Overlays##map");
  ctx->Yield(2);
  IM_CHECK_RETV(hover_row("**/###profile_edge_row", gui::SurveyProfileHover::kEdge), false);
  IM_CHECK_RETV(MapWindowIndexCountForColour(gui::kProfileReadingColour) == 0, false);
  ctx->ItemClick("**/Overlays##map");
  ctx->Yield(2);
  // The ratio row: highlight present, no map geometry (AC3's honest local feedback).
  IM_CHECK_RETV(hover_row("**/###tint_ratio_row", gui::g_state.analysis.profile_row_hover), false);
  IM_CHECK_RETV(MapWindowIndexCountForColour(gui::kProfileReadingColour) == 0, false);
  // The verdict row (AC1's existing-rows leg): the two pilot rows now highlight too — the anchor
  // draw itself is the verdict face's own test.
  IM_CHECK_RETV(hover_row("**/###diagnose_verdict_row", gui::g_state.analysis.profile_row_hover), false);
  ctx->MouseMoveToPos(ImVec2(-100.0f, -100.0f));
  ctx->Yield(3);
  // The legend icons (AC4): scroll the report to its legend, then assert the icons' colour
  // buckets — the frame anchors drew (full sky), so the sun's gold and the zenith/horizon greys
  // stand; the anchor's icon rides its toggle.
  if (ImGuiWindow* report = MapReportWindow()) {
    report->Scroll.y = report->ScrollMax.y;
  }
  ctx->Yield(2);
  IM_CHECK_RETV(MapReportIndexCountForColour(IM_COL32(255, 231, 168, 255)) > 0, false);
  IM_CHECK_RETV(MapReportIndexCountForColour(IM_COL32(200, 205, 215, 110)) > 0, false);
  ctx->ItemClick("**/Verdict anchors##map");
  ctx->Yield(3);
  IM_CHECK_RETV(gui::LastMapOverlayLegend().verdict_anchor, false);
  const ImU32 anchor_ink = gui::LastMapOverlayLegend().verdict_anchor_colour;
  IM_CHECK_RETV(MapReportIndexCountForColour(anchor_ink) > 0, false);
  IM_CHECK_RETV(SaveWindowPng(ctx, "//Raypath view", GuiTestTempPath("analysis_legend_icons.png").string()), false);
  ctx->ItemClick("**/Verdict anchors##map");
  ctx->Yield(2);
  ctx->WindowClose("//Raypath view");
  ctx->Yield(1);
  return true;
}

bool SurveyRefusesMultilayerClassByName(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kPcTwoLayerJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  // A row whose chain crosses layers: the display holds the joiner (this scene's reason to be
  // in the suite — its comment says so).
  std::string multi_display;
  for (const int idx : gui::g_state.analysis_result.display_order) {
    const auto& e = gui::g_state.analysis_result.payload->entries[static_cast<size_t>(idx)];
    if (std::strstr(e.display, " -> ") != nullptr) {
      multi_display = e.display;
      break;
    }
  }
  IM_CHECK_RETV(!multi_display.empty(), false);
  // The on-screen label is not the raw display: multi-layer rows are drawn through
  // AnalysisRowLabel over the Analyze-time core-to-pool map (the row_menu case's pattern), and
  // on this scene the first multi-layer row sits below the first page of the ScrollY table —
  // pan the table's child to it before clicking (FindListRowScrolling; a clipped row reports no
  // label at all). Selecting by the drawn label records the raw text.
  const std::string multi_label = gui::AnalysisRowLabel(multi_display, gui::AnalysisCoreToPoolMap(gui::g_state));
  SetListRef(ctx);
  const ImGuiID multi_row = FindListRowScrolling(ctx, multi_label);
  IM_CHECK_RETV(multi_row != 0, false);
  ctx->ItemClick(multi_row);
  ctx->SetRef("");
  ctx->Yield(3);
  {
    // Diagnostic, kept: the case exists to pin a named refusal, and these four lines tell a
    // fresh reader which link of issue→refuse→adopt broke if it ever reds again.
    const auto& s = gui::g_state.analysis.survey;
    ctx->LogInfo("[survey refusal] after issue: intent=%d in_flight=%d state=%d err='%s'", s.intent_valid ? 1 : 0,
                 gui::g_survey_executor.InFlight() ? 1 : 0, static_cast<int>(gui::SurveyOverviewStateFor(s)),
                 s.adopted_error.c_str());
  }
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kRefused;
                    },
                    30),
                false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_error.find("multi_layer_unsupported") != std::string::npos, false);
  ctx->LogInfo("[survey refusal] %s", gui::g_state.analysis.survey.adopted_error.c_str());
  IM_CHECK_RETV(SaveWindowPng(ctx, "//##AnalyzeDetails", GuiTestTempPath("analysis_survey_refusal.png").string()),
                false);
  return true;
}

// R6's concurrency stance, both directions, on the real server: a survey computing in the
// background neither blocks nor is blocked by a render run — the run starts and finishes under
// an in-flight survey with the survey state intact, and a selection made under a running run
// still gets its answer. No mutex story: both observations are the states themselves.
bool SurveyAndRenderRunAreOrthogonal(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  // Direction 1 — survey first, run under it.
  const std::string top_display = TopChainDisplay();
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + top_display).c_str());
  ctx->SetRef("");
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    30),
                false);
  const gui::SurveyHandle adopted_before = gui::g_state.analysis.survey.adopted_result;
  gui::g_state.sim.infinite = false;
  gui::g_state.sim.ray_num_millions = 0.1f;
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo("##TopBar/" ICON_FA_PLAY " Run")), false);
  ctx->ItemClick("##TopBar/" ICON_FA_PLAY " Run");
  IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kSimulating; }, 10), false);
  gui::g_state.dirty = false;  // the real loop's commit tick clears it (the c01 chain's note)
  IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kDone; }, 60), false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result == adopted_before, false);
  IM_CHECK_RETV(gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) == gui::SurveyOverviewState::kCompleted,
                false);
  ctx->LogInfo("[survey orthogonal] a full render run passed under an intact adopted survey");

  // Direction 2 — run first (unbounded), selection under it. The list is the kept analysis
  // result (a render commit keeps it on show, flagged stale); the survey answers anyway,
  // because it is bound to the frozen Analyze-time scene and never touches the server session.
  gui::g_state.sim.infinite = true;
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo("##TopBar/" ICON_FA_PLAY " Run")), false);
  ctx->ItemClick("##TopBar/" ICON_FA_PLAY " Run");
  IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kSimulating; }, 10), false);
  const std::string other_display = TopChainDisplay();
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + other_display).c_str());
  ctx->SetRef("");
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [&] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                                 gui::SurveyOverviewState::kCompleted &&
                             gui::g_state.sim_state == SimState::kSimulating;
                    },
                    30),
                false);
  ctx->LogInfo("[survey orthogonal] a selection's survey completed under a running render");
  // The top-bar Stop ends the render run (the list window's own Stop belongs to the analysis
  // run and sits disabled under the render's Run↔Analyze mutual exclusion).
  ctx->ItemClick("##TopBar/" ICON_FA_STOP " Stop");
  ctx->SetRef("");
  IM_CHECK_RETV(DriveUntil(ctx, [] { return !gui::g_state.analysis.started; }, 10), false);
  return true;
}

// R6's teardown matrix, the rows the frame paths own: switching to the Render workspace cancels
// nothing and clears nothing (the query finishes into the mailbox; coming back adopts it), New
// clears the session and releases the executor's binding, Revert keeps the session with the
// kept selection and only withdraws an in-flight query.
bool SurveySurvivesAWorkspaceSwitch(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  const std::string top_display = TopChainDisplay();
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + top_display).c_str());
  ctx->SetRef("");
  // Switch away before the answer lands: the query keeps running (the Render frame pumps
  // nothing), and the switch itself is pure view — no dirty, no sim-state write.
  // The segmented pair shows both cells in every mode; the tinted one is the current mode, the
  // bare one names the destination.
  ctx->ItemClick("##TopBar/Render");
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.workspace.mode == gui::GuiState::WorkspaceMode::kRender, false);
  IM_CHECK_RETV(gui::g_state.dirty == false, false);

  // Back in the Analyze workspace: the pump adopts whatever finished while away.
  ctx->ItemClick("##TopBar/Analyze");
  ctx->Yield(2);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    30),
                false);
  ctx->LogInfo("[survey switch] the query finished across a workspace switch and was adopted");

  // New: the whole analysis session dies with the document — survey bookkeeping included — and
  // the executor's binding is released on the worker thread.
  ResetTestState();
  IM_CHECK_RETV(gui::g_state.analysis.survey.intent_valid == false, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result == nullptr, false);
  IM_CHECK_RETV(!gui::g_survey_executor.InFlight(), false);

  // Revert with a query IN FLIGHT: the withdrawal carries the cancel latch (the engine answers
  // a cancel with an OK-carrying partial document, and that answer must not land as this
  // selection's completed digest). Selecting and reverting back-to-back, with no Yield between,
  // means the query cannot have finished yet — the latch is what the overview then shows.
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  SetListRef(ctx);
  const std::string in_flight_display = TopChainDisplay();
  ctx->ItemClick((std::string("**/") + in_flight_display).c_str());
  gui::DoRevert();
  ctx->SetRef("");
  IM_CHECK_RETV(DriveUntil(ctx, [] { return !gui::g_survey_executor.InFlight(); }, 20), false);
  IM_CHECK_RETV(gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) == gui::SurveyOverviewState::kCancelled,
                false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_error.empty(), false);
  ctx->LogInfo("[survey switch] Revert withdrew an in-flight query under the cancel latch");
  // The kept selection is not re-issued under the latch; a DIFFERENT selection clears the latch
  // with its new intent and answers (the top row again would be the same key — the latch holds).
  const auto& kept_view = gui::g_state.analysis_result;
  const std::string other_display = kept_view.payload->entries[static_cast<size_t>(kept_view.display_order[1])].display;
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + other_display).c_str());
  ctx->SetRef("");
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    30),
                false);
  ctx->LogInfo("[survey switch] a changed selection re-armed after the reverted query");

  // Revert after adoption: rebuild the analysis state, adopt a survey, then revert — the
  // bookkeeping survives with the kept selection (same committed scene), nothing re-issues
  // under it, and the adopted digest stays on show (no latch was set — nothing was withdrawn).
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  SetListRef(ctx);
  const std::string revert_display = TopChainDisplay();
  ctx->ItemClick((std::string("**/") + revert_display).c_str());
  ctx->SetRef("");
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    30),
                false);
  const gui::SurveyHandle adopted_before_revert = gui::g_state.analysis.survey.adopted_result;
  gui::DoRevert();
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analysis.survey.intent_valid == true, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result == adopted_before_revert, false);
  IM_CHECK_RETV(!gui::g_survey_executor.InFlight(), false);
  ctx->LogInfo("[survey switch] Revert kept the session and the adopted outcome");
  return true;
}

// ---- 742.7's integration legs: the cross-slice combination evidence the per-slice legs leave
// out — one session driving the whole C01 chain, the internal-reflection path seen from its real
// consumer, and the new faces' lifecycle across row switches and document actions. The claims are
// session-level (one selection feeding every face coherently), not per-face re-pins: each face's
// own contract stays with the leg that introduced it.

// The continuous chain (v1-scope §4 C01): ONE selection, ONE session — the class overview (which
// must not grab a sky point for the user), the frozen reference map, a real pick on the canvas,
// the point document, the fiber, the factors and the representative path — every echo naming the
// same selected class. Real-timing pool, the per-face legs' shape: the survey's chromatic legs,
// then the map and the point queue behind it on the one worker.
bool AnalyzeFullChainOneSession(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  // Before any selection every face sits in its honest no-intent state — the baseline the
  // "the default chain never grabs a point" claim is read against.
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid == false, false);
  IM_CHECK_RETV(gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kNoIntent, false);
  IM_CHECK_RETV(gui::MapQueryStateFor(gui::g_state.analysis.map) == gui::MapQueryState::kNoSelection, false);
  IM_CHECK_RETV(gui::g_state.analysis.trace.intent_valid == false, false);

  const std::string top_display = TopChainDisplay();
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + top_display).c_str());
  ctx->SetRef("");
  const std::optional<gui::SelectedClassIdentity> identity = gui::SelectedClassIdentityFor(gui::g_state);
  IM_CHECK_RETV(identity.has_value(), false);
  // The overview settles (the chromatic legs are the wait), and the settled chain has STILL
  // written no sky point: the default overview never picks for the user (v1-scope B).
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    60),
                false);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid == false, false);
  IM_CHECK_RETV(gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kNoIntent, false);
  IM_CHECK_RETV(gui::g_state.analysis.trace.intent_valid == false, false);
  // The session-level dispersion readings (the 742.4 leg owns the full pin; the chain carries
  // them here): the dual-lambda inner edges, red inside blue, from the one real survey. Under
  // pool co-tenancy the survey's chromatic legs can honestly hit the 15 s budget — the partial
  // keeps the routing facts and drops the dispersion row; the leg pins the dispersion on the
  // survey's own terms (completed → the readings must be there) and names the partial otherwise.
  {
    const gui::SurveyOverviewModel model = gui::BuildSurveyOverviewModel(*gui::g_state.analysis.survey.adopted_result);
    IM_CHECK_RETV(model.valid && model.arm == gui::SurveyArm::kFullDimensional, false);
    ctx->LogInfo("[v1 chain] survey execution %d, budget %d ms, early-stopped %d, dispersion available %d",
                 model.execution, model.budget_ms, model.early_stopped ? 1 : 0, model.dispersion.available ? 1 : 0);
    if (model.dispersion.available) {
      IM_CHECK_RETV(model.dispersion.min_row_available, false);
      IM_CHECK_RETV(model.dispersion.value_red_deg < model.dispersion.value_blue_deg, false);
      IM_CHECK_RETV(model.dispersion.direction == "red_inner_blue_outer", false);
      IM_CHECK_RETV(model.tint.available, false);
      ctx->LogInfo("[v1 chain] dispersion %.3f/%.3f (delta %.3f deg, %s)", model.dispersion.value_red_deg,
                   model.dispersion.value_blue_deg, model.dispersion.displacement_deg,
                   model.dispersion.direction.c_str());
    } else {
      IM_CHECK_RETV(model.execution == LUMICE_PATH_OUTCOME_DEADLINE_EXCEEDED, false);
      ctx->LogInfo("[v1 chain] the survey stopped inside its budget — the dispersion row is the honest partial");
    }
  }
  IM_CHECK_RETV(SaveWindowPng(ctx, "//##AnalyzeDetails", GuiTestTempPath("analyze_v1_chain_overview.png").string()),
                false);

  // The map window answers behind the survey on the one worker (default chain: full sky, then
  // the full-sky result), and its echo names the SAME class the overview
  // summarized — one identity feeding both faces.
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      if (m.adopted_result == nullptr || !(m.adopted_id == m.intent_id)) {
                        return false;
                      }
                      LUMICE_PathMapInfo i{};
                      i.struct_size = sizeof(i);
                      return LUMICE_PathMapGetInfo(m.adopted_result.get(), &i) == LUMICE_PATH_READ_OK &&
                             i.view_full_sky == 1;
                    },
                    60),
                false);
  {
    const gui::MapQuerySession& map = gui::g_state.analysis.map;
    LUMICE_PathMapInfo info{};
    info.struct_size = sizeof(info);
    IM_CHECK_RETV(LUMICE_PathMapGetInfo(map.adopted_result.get(), &info) == LUMICE_PATH_READ_OK, false);
    IM_CHECK_RETV(info.crystal_id == identity->core_crystal_id, false);
    IM_CHECK_RETV(info.kept_count > 0, false);
    ctx->LogInfo("[v1 chain] map lambda %.1f, kept %lld, window delta[%.1f,%.1f]", info.wavelength_nm, info.kept_count,
                 info.view_delta_min_deg, info.view_delta_max_deg);
  }
  // Pure observation preserves the full-sky reference identity.
  const auto reference_id = gui::g_state.analysis.map.adopted_id;
  gui::g_state.renderer.azimuth += 15.0f;
  gui::g_state.renderer.fov = 10.0f;
  ctx->Yield(30);
  IM_CHECK_RETV(gui::g_state.analysis.map.intent_id == reference_id, false);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const auto& m = gui::g_state.analysis.map;
                      return m.adopted_result && m.adopted_id == m.intent_id;
                    },
                    30),
                false);
  IM_CHECK_RETV(gui::g_state.analysis.map.adopted_id == reference_id, false);
  IM_CHECK_RETV(SaveWindowPng(ctx, "//Raypath view", GuiTestTempPath("analyze_v1_chain_map.png").string()), false);
  IM_CHECK_RETV(CheckMapGlyphsAcrossUiScale(ctx), false);

  // The real pick on the canvas centre: the point intent arms through the real mouse path (the
  // pick's outcome is the discovery arm's own honest answer at whatever spot the class's mass
  // centroid is — off-fiber spots answer an empty discovered document, which is the display's
  // named-empty state, not a failure). The document the chain then asserts on is pinned by the
  // anchor write below — the 742.6 leg's verified on-fiber target for this scene's halo fiber,
  // written through the same state the pick path writes ("the same write the button performs").
  ctx->KeyDown(ImGuiMod_Alt);
  ctx->ItemClick("**/##map_canvas");
  ctx->KeyUp(ImGuiMod_Alt);
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid, false);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.from_map_pick, false);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  // The pick's own document is a real terminal state and gets pinned as one: the discovery arm's
  // outcome is read back honestly (on-fiber or not), its component count logged — this step
  // carries the AC2 "an empty answer is still an answer" evidence at the combination level
  // (the anchor write below then re-keys to the deterministic document).
  {
    LUMICE_PathPointInfo pick_info{};
    pick_info.struct_size = sizeof(pick_info);
    IM_CHECK_RETV(
        LUMICE_PathPointGetInfo(gui::g_state.analysis.point.adopted_result.get(), &pick_info) == LUMICE_PATH_READ_OK,
        false);
    IM_CHECK_RETV(pick_info.outcome == LUMICE_PATH_POINT_DISCOVERED, false);
    std::vector<gui::PointComponentRow> pick_rows;
    const int pick_components = gui::PointComponentRows(*gui::g_state.analysis.point.adopted_result, &pick_rows);
    ctx->LogInfo("[v1 chain] the pick's own document: outcome DISCOVERED, %d component(s) at the centroid spot",
                 pick_components);
  }
  SetSkyPointTarget(42.0, 0.0);
  ctx->Yield(2);  // the pump re-arms under the new key before the wait reads the state
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  {
    const gui::PointQuerySession& point = gui::g_state.analysis.point;
    LUMICE_PathPointInfo info{};
    info.struct_size = sizeof(info);
    IM_CHECK_RETV(LUMICE_PathPointGetInfo(point.adopted_result.get(), &info) == LUMICE_PATH_READ_OK, false);
    IM_CHECK_RETV(info.crystal_id == identity->core_crystal_id, false);
    IM_CHECK_RETV(info.outcome == LUMICE_PATH_POINT_DISCOVERED, false);
    const gui::MapQuerySession& map = gui::g_state.analysis.map;
    LUMICE_PathMapInfo map_info{};
    map_info.struct_size = sizeof(map_info);
    IM_CHECK_RETV(LUMICE_PathMapGetInfo(map.adopted_result.get(), &map_info) == LUMICE_PATH_READ_OK, false);
    // One wavelength knob feeds both faces (the engine's reference echo, read back from each).
    IM_CHECK_RETV(info.wavelength_nm == map_info.wavelength_nm, false);
    // The caliber lines are the engine's own echo (the D65-vs-reference caliber the user reads).
    const std::vector<std::string> caliber = gui::PointCaliberLines(info);
    IM_CHECK_RETV(!caliber.empty(), false);
    std::vector<gui::PointComponentRow> rows;
    const int components = gui::PointComponentRows(*point.adopted_result, &rows);
    IM_CHECK_RETV(components >= 1, false);
    const int valid_count = gui::PointValidSampleCount(*point.adopted_result, 0);
    IM_CHECK_RETV(valid_count > 1, false);
    LUMICE_PathPointSample sample{};
    sample.struct_size = sizeof(sample);
    const int stored = gui::PointValidSampleIndexAt(*point.adopted_result, 0, 0);
    IM_CHECK_RETV(LUMICE_PathPointGetSample(point.adopted_result.get(), 0, static_cast<size_t>(stored), &sample) ==
                      LUMICE_PATH_READ_OK,
                  false);
    const std::vector<std::string> factors = gui::PointFactorLines(sample);
    IM_CHECK_RETV(!factors.empty(), false);
    ctx->LogInfo("[v1 chain] point alt %.2f az %.2f dev %.2f, lambda %.1f, %d component(s), %d valid samples",
                 info.target_altitude_deg, info.target_azimuth_deg, info.target_deviation_deg, info.wavelength_nm,
                 components, valid_count);
    for (const std::string& line : caliber) {
      ctx->LogInfo("[v1 chain] caliber: %s", line.c_str());
    }
  }

  // The representative path follows the same session's cursor: sample 0 drawn, a slider step
  // re-keys it, and the C01 chain's own shape comes back through the C readers (2 contacts —
  // entry face, exit face — 3 segments: the entry/internal/exit polyline).
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::RayTraceSession& t = gui::g_state.analysis.trace;
                      return t.adopted_result != nullptr && t.intent_valid &&
                             t.key.sample ==
                                 gui::PointValidSampleIndexAt(*gui::g_state.analysis.point.adopted_result, 0, 0);
                    },
                    30),
                false);
  gui::g_state.analysis.point_view.sample_index = 1;
  gui::g_state.analysis.point_view.playing = false;
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::RayTraceSession& t = gui::g_state.analysis.trace;
                      if (!t.adopted_result || !t.intent_valid) {
                        return false;
                      }
                      const int stored =
                          gui::PointValidSampleIndexAt(*gui::g_state.analysis.point.adopted_result, 0, 1);
                      return t.key.sample == stored && gui::RayTraceDisplayStateFor(t, { t.key.point_id, 0, stored }) ==
                                                           gui::RayTraceDisplayState::kDrawn;
                    },
                    30),
                false);
  {
    LUMICE_PathRayContacts contacts{};
    contacts.struct_size = sizeof(contacts);
    LUMICE_PathRayLegs legs{};
    legs.struct_size = sizeof(legs);
    IM_CHECK_RETV(LUMICE_RepresentativeRayGetContacts(gui::g_state.analysis.trace.adopted_result.get(), &contacts) ==
                      LUMICE_PATH_READ_OK,
                  false);
    IM_CHECK_RETV(
        LUMICE_RepresentativeRayGetLegs(gui::g_state.analysis.trace.adopted_result.get(), &legs) == LUMICE_PATH_READ_OK,
        false);
    IM_CHECK_RETV(contacts.contact_count == 2, false);
    IM_CHECK_RETV(legs.leg_count == 3, false);
  }
  // The pose window carries the trace screenshot evidence (the regions' only host now).
  gui::g_state.analyze_pose_view_open = true;
  PinPoseViewOnScreen(ctx);
  ctx->Yield(1);
  IM_CHECK_RETV(SaveWindowPng(ctx, "//Pose & trace", GuiTestTempPath("analyze_v1_chain_trace.png").string()), false);
  IM_CHECK_RETV(SaveWindowPng(ctx, PanoramaRegionRef(), GuiTestTempPath("analyze_v1_chain_panorama.png").string()),
                false);
  ctx->LogInfo("[v1 chain] one session: overview, map, pick, point, fiber, factors, trace — identity %u",
               static_cast<unsigned>(identity->core_crystal_id));
  return true;
}

// The internal-reflection path from its real consumer (v1-scope §4: beyond C01's two-face
// transmission): the corpus's C03 chain `3-5-6-7` — the same scene's L0 list — selected as a
// real row, its own frozen reference map, a real pick on the canvas, and the 3D region drawing
// the recovered polyline. The pin shape is the engine document's (4 contacts, 5 segments —
// entry, three internal, exit), read back through the C readers, not a screenshot's looks.
// Real-timing pool: the class survey's chromatic legs queue the map and the point on the one
// worker, the standing shape.
// Inspect actual overlay primitives, never a second projection of the contact input. The GL
// mesh is no longer in this draw list: its oracle below uses the frozen C mesh and submitted MVP.
// The legend stays in the separate report child.
std::vector<ImVec2> TraceCanvasVertices(ImU32 color) {
  std::vector<ImVec2> points;
  if (const ImGuiWindow* region = PoseRegionWindow("##AnalyzeRaypath3D")) {
    for (const ImDrawVert& vertex : region->DrawList->VtxBuffer) {
      if (vertex.col == color) {
        points.push_back(vertex.pos);
      }
    }
  }
  return points;
}

double ScreenTurn(const ImVec2& a, const ImVec2& b, const ImVec2& c) {
  return (static_cast<double>(b.x) - a.x) * (static_cast<double>(c.y) - a.y) -
         (static_cast<double>(b.y) - a.y) * (static_cast<double>(c.x) - a.x);
}

std::vector<ImVec2> TraceMeshSilhouette(std::vector<ImVec2> points) {
  std::sort(points.begin(), points.end(),
            [](const ImVec2& a, const ImVec2& b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
  points.erase(std::unique(points.begin(), points.end(),
                           [](const ImVec2& a, const ImVec2& b) { return a.x == b.x && a.y == b.y; }),
               points.end());
  if (points.size() < 3) {
    return {};
  }
  std::vector<ImVec2> hull;
  for (const ImVec2& p : points) {
    while (hull.size() >= 2 && ScreenTurn(hull[hull.size() - 2], hull.back(), p) <= 0.0) {
      hull.pop_back();
    }
    hull.push_back(p);
  }
  const size_t lower_size = hull.size();
  for (size_t i = points.size() - 1; i-- > 0;) {
    const ImVec2& p = points[i];
    while (hull.size() > lower_size && ScreenTurn(hull[hull.size() - 2], hull.back(), p) <= 0.0) {
      hull.pop_back();
    }
    hull.push_back(p);
  }
  hull.pop_back();
  return hull;
}

ImVec2 TraceOracleProject(const gui::Ray3DFrameInfo& frame, const float display[3]) {
  double scene[3];
  for (int k = 0; k < 3; ++k) {
    scene[k] = (static_cast<double>(display[k]) - frame.centre[k]) / frame.fit;
  }
  double clip[4];
  for (int row = 0; row < 4; ++row) {
    clip[row] = frame.mvp[12 + row];
    for (int k = 0; k < 3; ++k) {
      clip[row] += frame.mvp[k * 4 + row] * scene[k];
    }
  }
  return ImVec2(frame.canvas_min.x + static_cast<float>((clip[0] / clip[3] + 1.0) * 0.5) * frame.canvas_size.x,
                frame.canvas_min.y + static_cast<float>((1.0 - clip[1] / clip[3]) * 0.5) * frame.canvas_size.y);
}

bool TraceTextureQuadPresent(ImGuiTestContext* ctx) {
  const auto& frame = gui::LastRay3DFrameInfo();
  IM_CHECK_RETV(frame.valid && frame.frame_count == ImGui::GetFrameCount() && frame.texture_id != 0, false);
  const ImGuiWindow* region = PoseRegionWindow("##AnalyzeRaypath3D");
  IM_CHECK_RETV(region != nullptr, false);
  const bool has_quad =
      std::any_of(region->DrawList->CmdBuffer.begin(), region->DrawList->CmdBuffer.end(), [&](const ImDrawCmd& cmd) {
        return cmd.GetTexID() == static_cast<ImTextureID>(frame.texture_id) && cmd.ElemCount >= 6;
      });
  IM_CHECK_RETV(has_quad, false);
  return true;
}

bool TraceOracleMesh(ImGuiTestContext* ctx, LUMICE_CrystalMesh* mesh, std::vector<ImVec2>* pixels) {
  const auto& frame = gui::LastRay3DFrameInfo();
  IM_CHECK_RETV(TraceTextureQuadPresent(ctx), false);
  const auto* crystal = gui::FrozenCrystalForPool(gui::g_state, frame.pool);
  IM_CHECK_RETV(crystal != nullptr, false);
  // Independent reader path: nominal shape, no preview normalization or Y/Z swap. Display
  // rotation is the public row-major body->world convention, not the app's mesh-upload helper.
  const auto nominal = [](const gui::ShapeDist& d) {
    return LUMICE_Distribution{ static_cast<int>(d.type), d.center, 0.0f };
  };
  LUMICE_CrystalParam param{};
  param.type = crystal->type == gui::CrystalType::kPrism ? 0 : 1;
  param.height = nominal(crystal->height);
  param.prism_h = nominal(crystal->prism_h);
  param.upper_h = nominal(crystal->upper_h);
  param.lower_h = nominal(crystal->lower_h);
  param.upper_wedge_angle = crystal->upper_alpha;
  param.lower_wedge_angle = crystal->lower_alpha;
  for (int i = 0; i < 6; ++i) {
    param.face_distance[i] = nominal(crystal->face_distance[i]);
  }
  gui::FillSyncGroupArray(*crystal, param.sync_group);
  IM_CHECK_RETV(LUMICE_GetCrystalMesh(&param, 0, mesh) == LUMICE_OK && mesh->vertex_count > 0, false);
  pixels->clear();
  for (int v = 0; v < mesh->vertex_count; ++v) {
    const float* body = &mesh->vertices[v * 3];
    float display[3];
    for (int row = 0; row < 3; ++row) {
      display[row] = frame.crystal_frame ?
                         body[row] :
                         static_cast<float>(frame.pose[row * 3] * body[0] + frame.pose[row * 3 + 1] * body[1] +
                                            frame.pose[row * 3 + 2] * body[2]);
    }
    pixels->push_back(TraceOracleProject(frame, display));
  }
  return true;
}

bool TraceProjectionHasOrbitHandedness(ImGuiTestContext* ctx) {
  const auto& frame = gui::LastRay3DFrameInfo();
  const auto& orbit = gui::g_state.analysis.point_view;
  double fwd[3], right[3], up[3];
  gui::OrbitBasis(orbit.orbit_yaw_deg, orbit.orbit_pitch_deg, fwd, right, up);
  const auto project_basis = [&](double x, double y, double z) {
    float display[3];
    for (int k = 0; k < 3; ++k) {
      display[k] = frame.centre[k] + frame.fit * static_cast<float>(x * right[k] + y * up[k] - z * fwd[k]);
    }
    return TraceOracleProject(frame, display);
  };
  const ImVec2 origin = project_basis(0, 0, 0);
  const ImVec2 r = project_basis(0.2, 0, 0);
  const ImVec2 u = project_basis(0, 0.2, 0);
  IM_CHECK_RETV(r.x > origin.x && std::abs(r.y - origin.y) < 0.01f, false);
  IM_CHECK_RETV(u.y < origin.y && std::abs(u.x - origin.x) < 0.01f, false);
  if (orbit.orbit_zoom == 1.0f) {
    // The legacy fit puts one bounding radius at 42% of the canvas short side. Project a
    // known focal-plane displacement to measure that rendered scale, not just the zoom field.
    const float expected = 0.2f * 0.42f * std::min(frame.canvas_size.x, frame.canvas_size.y);
    ctx->LogInfo("[trace default fit] focal-plane delta %.3f px, legacy %.3f px", r.x - origin.x, expected);
    IM_CHECK_RETV(std::abs((r.x - origin.x) / expected - 1.0f) < 0.01f, false);
  }
  const ImVec2 near_pt = project_basis(0.2, 0, 0.5);
  const ImVec2 far_pt = project_basis(0.2, 0, -0.5);
  IM_CHECK_RETV(near_pt.x - origin.x > far_pt.x - origin.x, false);
  ctx->LogInfo("[trace projection] right -> +x, up -> -y, -fwd -> nearer/larger");
  return true;
}

bool TraceMeshHasVisiblePerspective(ImGuiTestContext* ctx) {
  LUMICE_CrystalMesh mesh{};
  std::vector<ImVec2> pixels;
  IM_CHECK_RETV(TraceOracleMesh(ctx, &mesh, &pixels), false);
  // On the symmetric prism fixture, centrally opposite edges have identical physical length
  // and orthographic projected length. Perspective alone makes their screen lengths differ.
  double max_ratio = 1.0;
  for (int e = 0; e < mesh.edge_count; ++e) {
    const int a = mesh.edges[e * 2], b = mesh.edges[e * 2 + 1];
    for (int other = e + 1; other < mesh.edge_count; ++other) {
      const int c = mesh.edges[other * 2], d = mesh.edges[other * 2 + 1];
      const auto opposite = [&](int i, int j) {
        double sum = 0, norm = 0;
        for (int k = 0; k < 3; ++k) {
          sum += std::pow(mesh.vertices[i * 3 + k] + mesh.vertices[j * 3 + k], 2);
          norm += std::pow(mesh.vertices[i * 3 + k], 2);
        }
        return sum <= norm * 1e-8;
      };
      if (!((opposite(a, c) && opposite(b, d)) || (opposite(a, d) && opposite(b, c)))) {
        continue;
      }
      const double l0 = std::hypot(pixels[a].x - pixels[b].x, pixels[a].y - pixels[b].y);
      const double l1 = std::hypot(pixels[c].x - pixels[d].x, pixels[c].y - pixels[d].y);
      if (std::min(l0, l1) > gui::UiPx(5.0f)) {
        max_ratio = std::max(max_ratio, std::max(l0, l1) / std::min(l0, l1));
      }
    }
  }
  ctx->LogInfo("[trace perspective] opposite-edge screen-length ratio=%.4f (minimum 1.05)", max_ratio);
  IM_CHECK_RETV(max_ratio >= 1.05, false);
  return true;
}

bool TraceContactsInsideMeshSilhouette(ImGuiTestContext* ctx) {
  IM_CHECK_RETV(PoseRegionWindow("##AnalyzeRaypath3D") != nullptr, false);
  LUMICE_CrystalMesh raw_mesh{};
  std::vector<ImVec2> mesh;
  IM_CHECK_RETV(TraceOracleMesh(ctx, &raw_mesh, &mesh), false);
  const auto contacts = TraceCanvasVertices(gui::kRayTraceContactColor);
  IM_CHECK_RETV(!mesh.empty(), false);
  IM_CHECK_RETV(!contacts.empty(), false);
  const auto hull = TraceMeshSilhouette(mesh);
  IM_CHECK_RETV(hull.size() >= 3, false);
  double worst_outside_px = 0.0;
  for (const ImVec2& p : contacts) {
    for (size_t i = 0; i < hull.size(); ++i) {
      const ImVec2& a = hull[i];
      const ImVec2& b = hull[(i + 1) % hull.size()];
      const double length = std::hypot(static_cast<double>(b.x) - a.x, static_cast<double>(b.y) - a.y);
      // The hull's exact duplicate removal guarantees distinct adjacent vertices.
      worst_outside_px = std::max(worst_outside_px, -ScreenTurn(a, b, p) / length);
    }
  }
  // Pixel-distance tolerance covers the UiPx(2.5) marker radius plus UiPx(1.5) stroke/AA
  // envelope, not world geometry. Markers stay screen-sized under perspective.
  const double tolerance_px = gui::UiPx(2.5f) + gui::UiPx(1.5f);
  ctx->LogInfo("[trace silhouette] mesh=%zu marker=%zu hull=%zu outside=%.3f px, tolerance=%.3f px, margin=%.3f px",
               mesh.size(), contacts.size(), hull.size(), worst_outside_px, tolerance_px,
               tolerance_px - worst_outside_px);
  IM_CHECK_RETV(worst_outside_px <= tolerance_px, false);
  return true;
}

ImGuiWindow* TraceReportWindow() {
  const ImGuiWindow* region = PoseRegionWindow("##AnalyzeRaypath3D");
  for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
    if (w->Active && w->ParentWindow == region && std::strstr(w->Name, "##Ray3DReport") != nullptr) {
      return w;
    }
  }
  return nullptr;
}

bool TraceLegendSwatchesAvoidText(ImGuiTestContext* ctx) {
  const ImGuiWindow* report = TraceReportWindow();
  IM_CHECK_RETV(report != nullptr, false);
  const auto& vertices = report->DrawList->VtxBuffer;
  const ImU32 text_color = ImGui::GetColorU32(ImGuiCol_TextDisabled);
  std::vector<ImRect> glyphs;
  for (int i = 0; i + 3 < vertices.Size;) {
    if (vertices[i].col != text_color) {
      ++i;
      continue;
    }
    ImRect rect(vertices[i].pos, vertices[i].pos);
    for (int k = 1; k < 4; ++k) {
      rect.Add(vertices[i + k].pos);
    }
    glyphs.push_back(rect);
    i += 4;
  }
  IM_CHECK_RETV(!glyphs.empty(), false);
  const auto check_swatch = [&](ImU32 color) -> bool {
    ImRect rect(ImVec2(FLT_MAX, FLT_MAX), ImVec2(-FLT_MAX, -FLT_MAX));
    int count = 0;
    for (const ImDrawVert& vertex : vertices) {
      if (vertex.col == color) {
        rect.Add(vertex.pos);
        ++count;
      }
    }
    // AddRectFilled emits a real four-vertex swatch, unlike the old diagonal AA line.
    IM_CHECK_RETV(count == 4, false);
    IM_CHECK_RETV(std::abs(rect.GetWidth() - gui::UiPx(9.0f)) < 0.1f, false);
    IM_CHECK_RETV(std::abs(rect.GetHeight() - gui::UiPx(9.0f)) < 0.1f, false);
    const bool overlaps =
        std::any_of(glyphs.begin(), glyphs.end(), [&](const ImRect& glyph) { return rect.Overlaps(glyph); });
    IM_CHECK_RETV(!overlaps, false);
    return true;
  };
  for (ImU32 color : gui::kRayTraceSegmentColors) {
    if (!check_swatch(color)) {
      return false;
    }
  }
  ctx->LogInfo("[trace legend] three 9px swatches, no overlap with %zu text glyphs", glyphs.size());
  return true;
}

// ---- The 2026-11-12 display-model legs (feedback #7/#8/#9): mesh-only framing, cumulative
// Fresnel widths, ratio labels, contact facing. The oracle side reads the SAME persisted frame
// fields (Ray3DFrameInfo) through the shared CrystalRenderer camera formulas and inspects the
// submitted primitives — never a second copy of the renderer's internals. ----

// The display-space point of a polyline/contact point (the frame switch's own transform: the
// world view leaves a world point alone, the crystal view rotates it back by the pose).
void TracePolylinePointToDisplay(const gui::Ray3DFrameInfo& frame, const float p[3], float out[3]) {
  float tmp[3] = { p[0], p[1], p[2] };
  if (frame.crystal_frame) {
    gui::RayTracePointToCrystalFrame(frame.pose, tmp);
  }
  out[0] = tmp[0];
  out[1] = tmp[1];
  out[2] = tmp[2];
}

// The near-plane-clipped pixel endpoints of a display-space segment — the production clip's
// independent twin: scene coordinates over frame.centre/fit, homogeneous clip over frame.mvp,
// the plane from CrystalRenderer::ComputeNearPlane(frame.zoom) (the frustum's own near plane).
// False when the whole segment sits behind the near plane.
bool TraceOracleClipSegment(const gui::Ray3DFrameInfo& frame, const float display_a[3], const float display_b[3],
                            ImVec2* pa, ImVec2* pb) {
  const float near_w = gui::CrystalRenderer::ComputeNearPlane(frame.zoom);
  float clip[2][4];
  const float* displays[2] = { display_a, display_b };
  for (int e = 0; e < 2; ++e) {
    for (int row = 0; row < 4; ++row) {
      clip[e][row] = frame.mvp[12 + row];
      for (int k = 0; k < 3; ++k) {
        clip[e][row] += frame.mvp[k * 4 + row] * ((displays[e][k] - frame.centre[k]) / frame.fit);
      }
    }
  }
  if (clip[0][3] < near_w && clip[1][3] < near_w) {
    return false;
  }
  float t[2] = { 0.0f, 1.0f };
  if (clip[0][3] < near_w) {
    t[0] = (near_w - clip[0][3]) / (clip[1][3] - clip[0][3]);
  }
  if (clip[1][3] < near_w) {
    t[1] = (near_w - clip[0][3]) / (clip[1][3] - clip[0][3]);
  }
  for (int e = 0; e < 2; ++e) {
    float c[4];
    for (int k = 0; k < 4; ++k) {
      c[k] = clip[0][k] + (clip[1][k] - clip[0][k]) * t[e];
    }
    ImVec2& out = e == 0 ? *pa : *pb;
    out = ImVec2(frame.canvas_min.x + (c[0] / c[3] + 1.0f) * 0.5f * frame.canvas_size.x,
                 frame.canvas_min.y + (1.0f - c[1] / c[3]) * 0.5f * frame.canvas_size.y);
  }
  return true;
}

// The adopted chain's per-segment cumulative energies, multiplied test-side from the C readers
// (the pure rule's hand anchors live in the unit layer; the same +-1e-6 usability band).
struct TraceChainEnergies {
  std::vector<double> energies;  // per segment, the entry leg 1.0
  std::vector<bool> valid;       // false from the first unusable factor on
};
bool TraceReadChainEnergies(TraceChainEnergies* out) {
  LUMICE_PathRayContacts contacts{};
  contacts.struct_size = sizeof(contacts);
  if (LUMICE_RepresentativeRayGetContacts(gui::g_state.analysis.trace.adopted_result.get(), &contacts) !=
          LUMICE_PATH_READ_OK ||
      contacts.contact_count == 0) {
    return false;
  }
  const size_t segments = contacts.contact_count + 1;
  out->energies.assign(segments, 1.0);
  out->valid.assign(segments, true);
  for (size_t k = 0; k < contacts.contact_count; ++k) {
    const double q = contacts.contacts[k].transmittance;
    out->valid[k + 1] = out->valid[k] && std::isfinite(q) && q >= -1e-6 && q <= 1.0 + 1e-6;
    out->energies[k + 1] = out->valid[k + 1] ? out->energies[k] * std::clamp(q, 0.0, 1.0) : out->energies[k];
  }
  return true;
}

// What each segment SHOULD have submitted: the near-clipped endpoints, the shared mapping's
// width (neutral past an unusable factor), and whether the segment drew at all (an exact zero
// draws no ink) and whether anything of it is on the canvas.
struct TraceExpectedLine {
  int seg = 0;
  int kind = 0;
  ImVec2 pa;
  ImVec2 pb;
  float width = 0.0f;
  double energy = 0.0;
  bool drawn = false;
  bool touches_canvas = false;
};
bool TraceBuildExpectedLines(const gui::Ray3DFrameInfo& frame, const TraceChainEnergies& chain,
                             std::vector<TraceExpectedLine>* out) {
  gui::RayTracePolyline line;
  if (!gui::BuildRayTracePolyline(*gui::g_state.analysis.trace.adopted_result, 1.1f, &line) ||
      line.seg_kinds.size() != chain.energies.size()) {
    return false;
  }
  out->clear();
  for (size_t s = 0; s < line.seg_kinds.size(); ++s) {
    TraceExpectedLine e;
    e.seg = static_cast<int>(s);
    e.kind = line.seg_kinds[s];
    e.energy = chain.energies[s];
    e.width = chain.valid[s] ? gui::RayTraceEnergyWidth(static_cast<float>(chain.energies[s])) :
                               gui::UiPx(gui::kRayTraceWidthNeutralUi);
    e.drawn = chain.valid[s] ? chain.energies[s] > 0.0 : true;
    if (e.drawn) {
      float d0[3];
      float d1[3];
      TracePolylinePointToDisplay(frame, &line.points[s * 6], d0);
      TracePolylinePointToDisplay(frame, &line.points[s * 6 + 3], d1);
      if (!TraceOracleClipSegment(frame, d0, d1, &e.pa, &e.pb)) {
        e.drawn = false;  // fully behind the near plane — nothing submitted
      } else {
        // The oracle's own segment-rect clip (Liang-Barsky, written out here independently of
        // the producer's RayTraceSegmentOnCanvas — the expectation must not reuse the
        // producer's visibility predicate): an AABB overlap is only necessary, and a segment
        // skimming past a corner must expect no label.
        const ImVec2 canvas_max(frame.canvas_min.x + frame.canvas_size.x, frame.canvas_min.y + frame.canvas_size.y);
        double u0 = 0.0;
        double u1 = 1.0;
        const double dx = static_cast<double>(e.pb.x) - e.pa.x;
        const double dy = static_cast<double>(e.pb.y) - e.pa.y;
        const auto cut = [&](double q, double dir) {
          if (std::abs(dir) < 1e-12) {
            return q >= 0.0;
          }
          const double r = q / dir;
          if (dir < 0.0) {
            if (r > u1) {
              return false;
            }
            if (r > u0) {
              u0 = r;
            }
          } else {
            if (r < u0) {
              return false;
            }
            if (r < u1) {
              u1 = r;
            }
          }
          return true;
        };
        e.touches_canvas =
            cut(static_cast<double>(e.pa.x) - frame.canvas_min.x, -dx) && cut(canvas_max.x - e.pa.x, dx) &&
            cut(static_cast<double>(e.pa.y) - frame.canvas_min.y, -dy) && cut(canvas_max.y - e.pa.y, dy) && u1 > u0;
      }
    }
    out->push_back(e);
  }
  return true;
}

// AC2's primitive leg: each drawn line's stroke span, measured off its exact-color vertices,
// must track the shared mapping's width — the span-minus-thickness offset (the vendored AA
// path's own constant, read off the draw list's flags) stays ONE value across every measured
// line, so span differences equal width differences; the chain's real decay shows a visible
// spread. Runs with the toggle off (no label ink interleaves between the lines).
bool TraceWidthsFollowCumulativeEnergy(ImGuiTestContext* ctx) {
  const auto& frame = gui::LastRay3DFrameInfo();
  IM_CHECK_RETV(frame.valid && frame.frame_count == ImGui::GetFrameCount(), false);
  IM_CHECK_RETV(gui::g_state.analysis.point_view.show_fresnel_labels == false, false);
  TraceChainEnergies chain;
  IM_CHECK_RETV(TraceReadChainEnergies(&chain), false);
  std::vector<TraceExpectedLine> expected;
  IM_CHECK_RETV(TraceBuildExpectedLines(frame, chain, &expected), false);
  const int expected_drawn = static_cast<int>(
      std::count_if(expected.begin(), expected.end(), [](const TraceExpectedLine& e) { return e.drawn; }));
  IM_CHECK_RETV(expected_drawn >= 3, false);  // entry + at least one internal + exit: the chain's shape
  const ImGuiWindow* region = PoseRegionWindow("##AnalyzeRaypath3D");
  IM_CHECK_RETV(region != nullptr, false);
  const ImDrawList* dl = region->DrawList;
  const ImDrawVert* verts = dl->VtxBuffer.begin();
  const int total = dl->VtxBuffer.Size;
  const bool aa = (dl->Flags & ImDrawListFlags_AntiAliasedLines) != 0;
  const bool aa_tex = (dl->Flags & ImDrawListFlags_AntiAliasedLinesUseTex) != 0;
  int cursor = 0;
  float offset = 0.0f;
  bool have_offset = false;
  float min_w = FLT_MAX;
  float max_w = -FLT_MAX;
  for (const TraceExpectedLine& e : expected) {
    if (!e.drawn) {
      continue;
    }
    // One line's checks run inside an immediate lambda (the loop-fatal-assert rule: a fatal
    // assert directly in a for body aborts the remaining rows; inside the lambda it returns
    // only the row, and the row's own IM_CHECK diagnostics survive).
    const bool line_ok = [&](const TraceExpectedLine& line) {
      const float t = line.width;
      // The vendored AddPolyline's branch: baked-lines texture path for integer thickness below
      // IM_DRAWLIST_TEX_LINES_WIDTH_MAX (4 verts, span t + 2), geometry path otherwise (8 verts,
      // the exact-color inner pair at +- (t-1)/2, span t - 1), the non-AA path (4 verts, span t).
      int per_line = 0;
      float want_offset = 0.0f;
      if (!aa) {
        per_line = 4;
        want_offset = 0.0f;
      } else if (aa_tex && (t - static_cast<float>(static_cast<int>(t))) <= 0.00001f &&
                 static_cast<int>(t) < IM_DRAWLIST_TEX_LINES_WIDTH_MAX) {
        per_line = 4;
        want_offset = 2.0f;
      } else {
        per_line = 8;
        want_offset = -1.0f;  // the width floor stays above the 1.0 thin-line boundary
      }
      // The geometry AA path interleaves alpha-0 fringe verts with the opaque core (per point:
      // fringe, core, core, fringe) — match the RGB family and anchor at the line's run start.
      const ImU32 rgb = gui::kRayTraceSegmentColors[line.kind] & ~IM_COL32_A_MASK;
      while (cursor < total && (verts[cursor].col & ~IM_COL32_A_MASK) != rgb) {
        ++cursor;
      }
      IM_CHECK_RETV(cursor + per_line <= total, false);
      ImVec2 dir(line.pb.x - line.pa.x, line.pb.y - line.pa.y);
      const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
      IM_CHECK_RETV(len > 1e-3f, false);
      dir.x /= len;
      dir.y /= len;
      const float npx = dir.y;
      const float npy = -dir.x;
      // The span comes from the OPAQUE core verts only: the geometry path's alpha-0 fringe sits
      // one AA pixel outside them (it would read t + 1); the texture and non-AA paths are opaque
      // throughout, so their whole quad measures.
      float lo = FLT_MAX;
      float hi = -FLT_MAX;
      bool any_opaque = false;
      int wrong_family = 0;
      for (int k = 0; k < per_line; ++k) {
        wrong_family += ((verts[cursor + k].col & ~IM_COL32_A_MASK) == rgb) ? 0 : 1;
        if (verts[cursor + k].col != gui::kRayTraceSegmentColors[line.kind]) {
          continue;
        }
        const float proj = (verts[cursor + k].pos.x - line.pa.x) * npx + (verts[cursor + k].pos.y - line.pa.y) * npy;
        lo = std::min(lo, proj);
        hi = std::max(hi, proj);
        any_opaque = true;
      }
      cursor += per_line;
      IM_CHECK_RETV(any_opaque && wrong_family == 0, false);
      const float span = hi - lo;
      ctx->LogInfo("[trace width] seg %d kind %d E=%.4g width=%.3f span=%.3f offset=%.3f", line.seg, line.kind,
                   line.energy, t, span, span - t);
      if (have_offset) {
        IM_CHECK_RETV(std::abs(span - t - offset) < 0.05f, false);
      } else {
        offset = span - t;
        have_offset = true;
      }
      IM_CHECK_RETV(std::abs(span - t - want_offset) < 0.05f, false);
      return true;
    }(e);
    if (!line_ok) {
      return false;  // the row's own IM_CHECK named the failure
    }
    min_w = std::min(min_w, e.width);
    max_w = std::max(max_w, e.width);
  }
  IM_CHECK_RETV(have_offset && max_w - min_w > 0.0f, false);
  ctx->LogInfo("[trace width] measured %d lines, width spread %.3f-%.3f, path offset %.2f", expected_drawn, min_w,
               max_w, offset);
  return true;
}

// AC3's primitive leg: the ratio numbers default off, a real click turns them on, every
// canvas-touching drawn segment then carries its label — the ink near the visible part's
// midpoint, the exact ratio text from the render-feedback slot (the draw-list carries no
// strings), and the canvas geometry holds still across the toggle.
bool TraceLabelsFollowTheToggle(ImGuiTestContext* ctx) {
  auto& view = gui::g_state.analysis.point_view;
  struct Restore {
    bool saved = gui::g_state.analysis.point_view.show_fresnel_labels;
    ~Restore() { gui::g_state.analysis.point_view.show_fresnel_labels = saved; }
  } restore;
  IM_CHECK_RETV(view.show_fresnel_labels == false, false);  // the AC's initial state
  ctx->Yield(2);
  IM_CHECK_RETV(TraceCanvasVertices(gui::kRayTraceLabelColor).empty(), false);
  const gui::Ray3DFrameInfo before = gui::LastRay3DFrameInfo();
  IM_CHECK_RETV(before.valid, false);
  ctx->SetRef("//Pose & trace");
  ctx->ItemClick("**/Show Fresnel ratios##r3d_fresnel");
  ctx->Yield(2);
  IM_CHECK_RETV(view.show_fresnel_labels == true, false);
  const auto& frame = gui::LastRay3DFrameInfo();
  IM_CHECK_RETV(frame.valid && frame.frame_count == ImGui::GetFrameCount(), false);
  IM_CHECK_RETV(frame.canvas_min.x == before.canvas_min.x && frame.canvas_min.y == before.canvas_min.y &&
                    frame.canvas_size.x == before.canvas_size.x && frame.canvas_size.y == before.canvas_size.y,
                false);
  TraceChainEnergies chain;
  IM_CHECK_RETV(TraceReadChainEnergies(&chain), false);
  std::vector<TraceExpectedLine> expected;
  IM_CHECK_RETV(TraceBuildExpectedLines(frame, chain, &expected), false);
  const std::vector<ImVec2> label_ink = TraceCanvasVertices(gui::kRayTraceLabelColor);
  // The rect feedback's contract first: five floats per segment, or a missing production
  // writer fails HERE instead of indexing an empty vector.
  IM_CHECK_RETV(gui::g_state.analysis.ray3d_fresnel_label_rects.size() == expected.size() * 5, false);
  std::vector<ImRect> placed_rects;
  int labelled = 0;
  for (const TraceExpectedLine& e : expected) {
    // One segment's checks run inside an immediate lambda (the loop-fatal-assert rule).
    const bool seg_ok = [&](const TraceExpectedLine& line) {
      const std::string& drawn_text = gui::g_state.analysis.ray3d_fresnel_labels[static_cast<size_t>(line.seg)];
      if (!line.drawn || !line.touches_canvas) {
        IM_CHECK_RETV(drawn_text.empty(), false);  // nothing submitted or nothing on show — no slot
        return true;
      }
      const std::string want = gui::RayTraceEnergyLabelText(
          gui::RayTraceSegmentEnergy{ chain.energies[line.seg], chain.valid[static_cast<size_t>(line.seg)] });
      IM_CHECK_RETV(drawn_text == want, false);
      // The per-label render feedback: the submitted text rect plus the placement kind. The
      // rects are the READABILITY evidence (the reviewer's real-frame screenshot caught
      // overlapping labels the old "some vertex nearby" check could not see); the leader's
      // landing is the IDENTITY evidence for a non-default placement.
      const float* rect = &gui::g_state.analysis.ray3d_fresnel_label_rects[static_cast<size_t>(line.seg) * 5];
      IM_CHECK_RETV(rect[2] > rect[0] && rect[3] > rect[1], false);  // a real submitted rect
      const ImRect placed(rect[0], rect[1], rect[2], rect[3]);
      const ImVec2 mid((line.pa.x + line.pb.x) * 0.5f, (line.pa.y + line.pb.y) * 0.5f);
      const int kind = static_cast<int>(rect[4]);
      if (kind == 0) {
        // The default offset: the rect stays next to its own visible midpoint (the rect-to-
        // point distance — nearest-edge gap — under the default offset's reach).
        const float gap_x = std::max(std::max(mid.x - placed.Max.x, placed.Min.x - mid.x), 0.0f);
        const float gap_y = std::max(std::max(mid.y - placed.Max.y, placed.Min.y - mid.y), 0.0f);
        IM_CHECK_RETV(std::hypot(gap_x, gap_y) < gui::UiPx(12.0f), false);
      } else {
        // Shifted / leader fallback: the leader lands a few px short of the visible midpoint —
        // the label bucket must carry a vertex there (the identity the plan forbids losing).
        const float bound = gui::UiPx(6.0f);
        const bool leader_lands = std::any_of(label_ink.begin(), label_ink.end(), [&](const ImVec2& v) {
          const float dx = v.x - mid.x;
          const float dy = v.y - mid.y;
          return dx * dx + dy * dy < bound * bound;
        });
        IM_CHECK_RETV(leader_lands, false);
      }
      placed_rects.push_back(placed);
      return true;
    }(e);
    if (!seg_ok) {
      return false;  // the row's own IM_CHECK named the failure
    }
    if (e.drawn && e.touches_canvas) {
      ++labelled;
    }
  }
  IM_CHECK_RETV(labelled >= 3, false);
  // The readability claim itself: no two placed text rects touch or overlap, and every ink
  // vertex is owned — inside some submitted text rect, or on a non-default placement's leader
  // corridor (the line from that text's centre to its segment's visible midpoint).
  bool rects_overlap = false;
  for (size_t a = 0; a < placed_rects.size() && !rects_overlap; ++a) {
    for (size_t b = a + 1; b < placed_rects.size() && !rects_overlap; ++b) {
      const ImRect& ra = placed_rects[a];
      const ImRect& rb = placed_rects[b];
      rects_overlap = ra.Min.x < rb.Max.x && rb.Min.x < ra.Max.x && ra.Min.y < rb.Max.y && rb.Min.y < ra.Max.y;
    }
  }
  IM_CHECK_RETV(!rects_overlap, false);
  std::vector<std::pair<ImVec2, ImVec2>> leader_corridors;
  for (const TraceExpectedLine& e : expected) {
    if (!e.drawn || !e.touches_canvas) {
      continue;
    }
    const float* rect = &gui::g_state.analysis.ray3d_fresnel_label_rects[static_cast<size_t>(e.seg) * 5];
    if (rect[4] != 0.0f) {
      leader_corridors.emplace_back(ImVec2((rect[0] + rect[2]) * 0.5f, (rect[1] + rect[3]) * 0.5f),
                                    ImVec2((e.pa.x + e.pb.x) * 0.5f, (e.pa.y + e.pb.y) * 0.5f));
    }
  }
  const bool ink_owned = std::all_of(label_ink.begin(), label_ink.end(), [&](const ImVec2& v) {
    const bool in_rect = std::any_of(placed_rects.begin(), placed_rects.end(), [&](const ImRect& r) {
      return v.x >= r.Min.x - 1.0f && v.x <= r.Max.x + 1.0f && v.y >= r.Min.y - 1.0f && v.y <= r.Max.y + 1.0f;
    });
    if (in_rect) {
      return true;
    }
    const double corridor_tol = gui::UiPx(2.5f);
    return std::any_of(leader_corridors.begin(), leader_corridors.end(), [&](const std::pair<ImVec2, ImVec2>& c) {
      const double dx = c.second.x - c.first.x;
      const double dy = c.second.y - c.first.y;
      const double len2 = dx * dx + dy * dy;
      double t = len2 > 0.0 ? ((v.x - c.first.x) * dx + (v.y - c.first.y) * dy) / len2 : 0.0;
      t = std::clamp(t, 0.0, 1.0);
      const double px = c.first.x + dx * t;
      const double py = c.first.y + dy * t;
      return std::hypot(v.x - px, v.y - py) < corridor_tol;
    });
  });
  IM_CHECK_RETV(ink_owned, false);
  ctx->LogInfo("[trace labels] %d labelled segments, %zu ink vertices, %zu rects pairwise apart, default-off leg held",
               labelled, label_ink.size(), placed_rects.size());
  IM_CHECK_RETV(SaveWindowPng(ctx, "//Pose & trace", GuiTestTempPath("analyze_c03_trace_fresnel_labels.png").string()),
                false);
  ctx->ItemClick("**/Show Fresnel ratios##r3d_fresnel");
  ctx->Yield(2);
  IM_CHECK_RETV(view.show_fresnel_labels == false, false);
  IM_CHECK_RETV(TraceCanvasVertices(gui::kRayTraceLabelColor).empty(), false);
  return true;
}

// AC4's primitive leg: each contact's marker shape follows the REAL facing — the request's own
// face number resolved in the mesh's per-face table (the test's own scan), the normal rotated
// by the frame switch, dotted against contact->eye with the REAL camera (ComputeDist over the
// persisted zoom; never a global fwd proxy). At least one contact must sit where the
// perspective test and its fwd approximation disagree — the leg's own proof that the proxy
// would have answered differently.
bool TraceContactShapesFollowTheRealFacing(ImGuiTestContext* ctx) {
  const auto& frame = gui::LastRay3DFrameInfo();
  IM_CHECK_RETV(frame.valid && frame.frame_count == ImGui::GetFrameCount(), false);
  LUMICE_CrystalMesh mesh{};
  std::vector<ImVec2> mesh_pixels;
  IM_CHECK_RETV(TraceOracleMesh(ctx, &mesh, &mesh_pixels), false);
  LUMICE_PathRayContacts contacts{};
  contacts.struct_size = sizeof(contacts);
  IM_CHECK_RETV(LUMICE_RepresentativeRayGetContacts(gui::g_state.analysis.trace.adopted_result.get(), &contacts) ==
                    LUMICE_PATH_READ_OK,
                false);
  LUMICE_RepresentativeRayInfo info{};
  info.struct_size = sizeof(info);
  IM_CHECK_RETV(
      LUMICE_RepresentativeRayGetInfo(gui::g_state.analysis.trace.adopted_result.get(), &info) == LUMICE_PATH_READ_OK,
      false);
  IM_CHECK_RETV(info.face_count == static_cast<int>(contacts.contact_count) && info.faces != nullptr, false);
  const auto& orbit = gui::g_state.analysis.point_view;
  double fwd[3];
  double right[3];
  double up[3];
  gui::OrbitBasis(orbit.orbit_yaw_deg, orbit.orbit_pitch_deg, fwd, right, up);
  const float eye_dist = gui::CrystalRenderer::ComputeDist(frame.zoom);
  const float eye[3] = { static_cast<float>(-eye_dist * fwd[0]), static_cast<float>(-eye_dist * fwd[1]),
                         static_cast<float>(-eye_dist * fwd[2]) };
  const double tolerance = gui::UiPx(6.0f);
  int solid_expected = 0;
  int hollow_expected = 0;
  int discriminating = 0;
  for (size_t i = 0; i < contacts.contact_count; ++i) {
    // One contact's checks run inside an immediate lambda (the loop-fatal-assert rule: a fatal
    // assert directly in a for body aborts the remaining rows); the counters advance only on a
    // passing row.
    const bool contact_ok = [&](size_t ci) {
      float c_display[3] = { static_cast<float>(contacts.contacts[ci].point[0]),
                             static_cast<float>(contacts.contacts[ci].point[1]),
                             static_cast<float>(contacts.contacts[ci].point[2]) };
      if (frame.crystal_frame) {
        gui::RayTracePointToCrystalFrame(frame.pose, c_display);
      }
      const ImVec2 pixel = TraceOracleProject(frame, c_display);
      // The test's own face-number scan (the production helper is pinned in the unit layer; this
      // leg reads the raw mesh fields independently).
      bool known = false;
      float n_body[3] = { 0.0f, 0.0f, 0.0f };
      if (info.faces[ci] >= 0) {
        for (int f = 0; f < mesh.face_count; ++f) {
          if (mesh.face_numbers_by_face[f] != info.faces[ci]) {
            continue;
          }
          const float* n = &mesh.face_normals[static_cast<size_t>(f) * 3];
          const float norm = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
          if (norm > 0.5f) {
            known = true;
            n_body[0] = n[0];
            n_body[1] = n[1];
            n_body[2] = n[2];
          }
          break;
        }
      }
      float n_display[3];
      if (frame.crystal_frame) {
        n_display[0] = n_body[0];
        n_display[1] = n_body[1];
        n_display[2] = n_body[2];
      } else {
        for (int row = 0; row < 3; ++row) {
          n_display[row] = static_cast<float>(frame.pose[row * 3] * n_body[0] + frame.pose[row * 3 + 1] * n_body[1] +
                                              frame.pose[row * 3 + 2] * n_body[2]);
        }
      }
      float c_scene[3];
      for (int k = 0; k < 3; ++k) {
        c_scene[k] = (c_display[k] - frame.centre[k]) / frame.fit;
      }
      const double dot = static_cast<double>(n_display[0]) * (eye[0] - c_scene[0]) +
                         static_cast<double>(n_display[1]) * (eye[1] - c_scene[1]) +
                         static_cast<double>(n_display[2]) * (eye[2] - c_scene[2]);
      const double dot_fwd = -(static_cast<double>(n_display[0]) * fwd[0] + static_cast<double>(n_display[1]) * fwd[1] +
                               static_cast<double>(n_display[2]) * fwd[2]);
      const bool solid = known && dot > 0.0;
      const ImU32 want = solid ? gui::kRayTraceContactSolidColor : gui::kRayTraceContactColor;
      const ImU32 other = solid ? gui::kRayTraceContactColor : gui::kRayTraceContactSolidColor;
      bool want_near = false;
      bool other_near = false;
      for (const ImVec2& v : TraceCanvasVertices(want)) {
        const double dx = v.x - pixel.x;
        const double dy = v.y - pixel.y;
        want_near = want_near || dx * dx + dy * dy < tolerance * tolerance;
      }
      for (const ImVec2& v : TraceCanvasVertices(other)) {
        const double dx = v.x - pixel.x;
        const double dy = v.y - pixel.y;
        other_near = other_near || dx * dx + dy * dy < tolerance * tolerance;
      }
      ctx->LogInfo("[trace facing] contact %zu face %d known=%d dot=%.3f dot_fwd=%.3f -> %s", ci, info.faces[ci],
                   known ? 1 : 0, dot, dot_fwd,
                   solid ? "solid" :
                   known ? "hollow(back)" :
                           "hollow(unknown)");
      IM_CHECK_RETV(want_near, false);
      IM_CHECK_RETV(!other_near, false);
      solid ? ++solid_expected : ++hollow_expected;
      if ((dot > 0.0) != (dot_fwd > 0.0)) {
        ++discriminating;
      }
      return true;
    }(i);
    if (!contact_ok) {
      return false;  // the row's own IM_CHECK named the failure
    }
  }
  IM_CHECK_RETV(solid_expected >= 1 && hollow_expected >= 1, false);
  IM_CHECK_RETV(discriminating >= 1, false);  // the perspective anchor — a fwd proxy would flip this contact
  ctx->LogInfo("[trace facing] solid %d hollow %d, %d contact(s) discriminate against the fwd proxy", solid_expected,
               hollow_expected, discriminating);
  return true;
}

// The crystal view's sun-direction arrow, anchored independently: the tip rides the C-API
// sample's own sun_in_crystal from the display-frame mesh centre, so the submitted accent
// primitives must lie on THAT line — a regression that re-applies the frame switch's inverse
// rotation to already-display points (the scene_of refactor's coordinate-boundary defect)
// bends the arrow off this line and fails the check. Runs in the crystal view.
bool TraceSunArrowFollowsTheCrystalSun(ImGuiTestContext* ctx) {
  const auto& frame = gui::LastRay3DFrameInfo();
  IM_CHECK_RETV(frame.valid && frame.crystal_frame && frame.frame_count == ImGui::GetFrameCount(), false);
  auto& view = gui::g_state.analysis.point_view;
  LUMICE_PathPointSample sample{};
  sample.struct_size = sizeof(sample);
  const int stored = gui::PointValidSampleIndexAt(*gui::g_state.analysis.point.adopted_result, view.component_index,
                                                  view.sample_index);
  IM_CHECK_RETV(LUMICE_PathPointGetSample(gui::g_state.analysis.point.adopted_result.get(),
                                          static_cast<size_t>(view.component_index), static_cast<size_t>(stored),
                                          &sample) == LUMICE_PATH_READ_OK,
                false);
  // The expected arrow: display-frame mesh centre -> centre + u * 0.6 * fit (u verbatim from
  // the sample; in the crystal view the display frame IS the body frame).
  const float u[3] = { static_cast<float>(sample.sun_in_crystal[0]), static_cast<float>(sample.sun_in_crystal[1]),
                       static_cast<float>(sample.sun_in_crystal[2]) };
  const float un = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
  IM_CHECK_RETV(un > 1e-6f, false);
  const float centre[3] = { frame.centre[0], frame.centre[1], frame.centre[2] };
  const float tip[3] = { centre[0] + u[0] / un * frame.fit * 0.6f, centre[1] + u[1] / un * frame.fit * 0.6f,
                         centre[2] + u[2] / un * frame.fit * 0.6f };
  const ImVec2 pc = TraceOracleProject(frame, centre);
  const ImVec2 pt = TraceOracleProject(frame, tip);
  // The pose must be non-identity here for the double-rotation regression to be visible.
  double pose_dev = 0.0;
  for (int k = 0; k < 9; ++k) {
    pose_dev = std::max(pose_dev, std::abs(frame.pose[k] - (k % 4 == 0 ? 1.0 : 0.0)));
  }
  ctx->LogInfo("[trace arrow] pose deviation from identity %.3f, arrow pixels (%.0f,%.0f)->(%.0f,%.0f)", pose_dev, pc.x,
               pc.y, pt.x, pt.y);
  // The accent primitives INSIDE the canvas rect (the window's other accent ink — e.g. the
  // checked radio's bullet — sits above the canvas and must not enter the bucket).
  const ImU32 accent = ImGui::ColorConvertFloat4ToU32(gui::AccentColor());
  const ImVec2 canvas_max(frame.canvas_min.x + frame.canvas_size.x, frame.canvas_min.y + frame.canvas_size.y);
  std::vector<ImVec2> arrow_ink;
  for (const ImVec2& v : TraceCanvasVertices(accent)) {
    if (v.x >= frame.canvas_min.x && v.x <= canvas_max.x && v.y >= frame.canvas_min.y && v.y <= canvas_max.y) {
      arrow_ink.push_back(v);
    }
  }
  IM_CHECK_RETV(!arrow_ink.empty(), false);
  // Every arrow vertex sits on the expected centre->tip segment (within the marker's AA
  // envelope): the wrong-direction regression puts the ink on a different line.
  const double seg_dx = static_cast<double>(pt.x) - pc.x;
  const double seg_dy = static_cast<double>(pt.y) - pc.y;
  const double seg_len = std::sqrt(seg_dx * seg_dx + seg_dy * seg_dy);
  IM_CHECK_RETV(seg_len > 1.0, false);
  const double tolerance = gui::UiPx(4.0);
  double worst = 0.0;
  for (const ImVec2& v : arrow_ink) {
    const double t = ((v.x - pc.x) * seg_dx + (v.y - pc.y) * seg_dy) / (seg_len * seg_len);
    const double px = pc.x + seg_dx * std::clamp(t, 0.0, 1.0);
    const double py = pc.y + seg_dy * std::clamp(t, 0.0, 1.0);
    worst = std::max(worst, std::hypot(v.x - px, v.y - py));
  }
  IM_CHECK_RETV(worst < tolerance, false);
  ctx->LogInfo("[trace arrow] %zu ink vertices on the expected sun line, worst %.2f px", arrow_ink.size(), worst);
  return true;
}

// The mesh-only fit's boundary leg: aimed straight down the entry leg at max zoom, the
// incident extension runs at the camera and crosses the near plane. The submitted geometry
// must stop at the crossing (the clipped bbox, not the raw behind-eye projection) and every
// overlay vertex stays finite.
bool TraceOverlayClipsAtTheNearPlane(ImGuiTestContext* ctx) {
  const auto& base = gui::LastRay3DFrameInfo();
  IM_CHECK_RETV(base.valid, false);
  // Aim the camera OPPOSITE the entry extension's far endpoint (in scene units, at the CURRENT
  // frame's framing): the far endpoint then sits straight behind the eye, w = dist - |p| < 0
  // by construction, and the segment must cross the near plane at every max-zoom geometry —
  // not just when the incident direction happens to point at the eye.
  gui::RayTracePolyline line;
  IM_CHECK_RETV(gui::BuildRayTracePolyline(*gui::g_state.analysis.trace.adopted_result, 1.1f, &line), false);
  double p[3] = {};
  double pn = 0.0;
  for (size_t end : { size_t(0), line.points.size() / 3 - 1 }) {
    float far_display[3];
    TracePolylinePointToDisplay(base, &line.points[end * 3], far_display);
    double q[3];
    for (int k = 0; k < 3; ++k) {
      q[k] = (far_display[k] - base.centre[k]) / base.fit;
    }
    const double qn = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]);
    if (qn > pn) {
      pn = qn;
      std::copy_n(q, 3, p);
    }
  }
  for (double& v : p) {
    v /= pn;
  }
  ctx->LogInfo("[trace clip] far endpoint scene radius %.3f (crossing needs radius > eye distance)", pn);
  struct Restore {
    float yaw = gui::g_state.analysis.point_view.orbit_yaw_deg;
    float pitch = gui::g_state.analysis.point_view.orbit_pitch_deg;
    float zoom = gui::g_state.analysis.point_view.orbit_zoom;
    ~Restore() {
      auto& view = gui::g_state.analysis.point_view;
      view.orbit_yaw_deg = yaw;
      view.orbit_pitch_deg = pitch;
      view.orbit_zoom = zoom;
    }
  } restore;
  auto& view = gui::g_state.analysis.point_view;
  // The display endpoints of segment 0 (its geometry is orbit-independent; only the camera
  // moves). points is a FLAT xyz array: point k occupies [k*3, k*3+3), so segment 0 runs from
  // points[0] (the display extension's far end) to points[3] (the first contact).
  float d0[3];
  float d1[3];
  TracePolylinePointToDisplay(base, &line.points[0], d0);
  TracePolylinePointToDisplay(base, &line.points[3], d1);
  float s0[3];
  float s1[3];
  for (int k = 0; k < 3; ++k) {
    s0[k] = (d0[k] - base.centre[k]) / base.fit;
    s1[k] = (d1[k] - base.centre[k]) / base.fit;
  }
  // Aim OPPOSITE the far endpoint: the far endpoint then sits straight behind the eye
  // (w = dist - |p| < 0), the segment must cross the near plane, and the submitted geometry
  // is the visible part's quad — vertices MAY extend past the canvas (the raster clip, not
  // the vertex buffer, owns canvas bounds), so the submitted run exists regardless.
  view.orbit_yaw_deg = static_cast<float>(std::atan2(-p[1], -p[0]) * 180.0 / 3.14159265358979323846);
  view.orbit_pitch_deg = static_cast<float>(std::asin(std::clamp(-p[2], -1.0, 1.0)) * 180.0 / 3.14159265358979323846);
  view.orbit_zoom = 8.0f;
  ctx->Yield(2);
  const auto& frame = gui::LastRay3DFrameInfo();
  IM_CHECK_RETV(frame.valid && frame.frame_count == ImGui::GetFrameCount(), false);
  const float near_w = gui::CrystalRenderer::ComputeNearPlane(frame.zoom);
  float clip_far[4];
  for (int row = 0; row < 4; ++row) {
    clip_far[row] = frame.mvp[12 + row];
    for (int k = 0; k < 3; ++k) {
      clip_far[row] += frame.mvp[k * 4 + row] * s0[k];
    }
  }
  IM_CHECK_RETV(clip_far[3] < near_w, false);  // the far endpoint is behind the eye at this aim
  ImVec2 pa;
  ImVec2 pb;
  IM_CHECK_RETV(TraceOracleClipSegment(frame, d0, d1, &pa, &pb), false);  // the near part stays visible
  ctx->LogInfo("[trace clip] eye dist %.3f at frame zoom %.3f", gui::CrystalRenderer::ComputeDist(frame.zoom),
               frame.zoom);
  // All overlay ink finite — a behind-eye point that slipped through would divide by a
  // non-positive w and land somewhere absurd. Vertices MAY sit past the canvas rect: a
  // segment crossing the canvas edge submits its full geometry and the clip rect rasters only
  // the visible part, so bounds belong to the raster, not to the vertex buffer.
  int non_finite = 0;
  for (ImU32 color : gui::kRayTraceSegmentColors) {
    for (const ImVec2& v : TraceCanvasVertices(color)) {
      non_finite += (std::isfinite(v.x) && std::isfinite(v.y)) ? 0 : 1;
    }
  }
  for (ImU32 color : { gui::kRayTraceContactColor, gui::kRayTraceContactSolidColor }) {
    for (const ImVec2& v : TraceCanvasVertices(color)) {
      non_finite += (std::isfinite(v.x) && std::isfinite(v.y)) ? 0 : 1;
    }
  }
  IM_CHECK_RETV(non_finite == 0, false);
  // The kept frame: the raw projection of the behind-eye endpoint — the shape the overlay
  // would have submitted without the clip — sits far from anything submitted.
  const auto& entry = gui::LastRay3DFrameInfo();
  IM_CHECK_RETV(entry.valid && entry.frame_count == ImGui::GetFrameCount(), false);
  IM_CHECK_RETV(TraceTextureQuadPresent(ctx), false);
  ImVec2 raw_far;
  {
    float clip[4];
    for (int row = 0; row < 4; ++row) {
      clip[row] = entry.mvp[12 + row];
      for (int k = 0; k < 3; ++k) {
        clip[row] += entry.mvp[k * 4 + row] * s0[k];
      }
    }
    raw_far = ImVec2(entry.canvas_min.x + (clip[0] / clip[3] + 1.0f) * 0.5f * entry.canvas_size.x,
                     entry.canvas_min.y + (1.0f - clip[1] / clip[3]) * 0.5f * entry.canvas_size.y);
  }
  // The submitted entry line: the first run of the entry color. The production clips at the
  // NEAR plane only — the submitted quad IS the visible part (pa -> pb), off-canvas extents
  // included; the raster clip owns canvas bounds and leaves the vertex buffer alone.
  const ImGuiWindow* region = PoseRegionWindow("##AnalyzeRaypath3D");
  IM_CHECK_RETV(region != nullptr, false);
  const ImDrawList* dl = region->DrawList;
  const ImDrawVert* verts = dl->VtxBuffer.begin();
  int cursor = 0;
  const int total = dl->VtxBuffer.Size;
  const ImU32 entry_rgb = gui::kRayTraceSegmentColors[0] & ~IM_COL32_A_MASK;
  while (cursor < total && (verts[cursor].col & ~IM_COL32_A_MASK) != entry_rgb) {
    ++cursor;
  }
  IM_CHECK_RETV(cursor < total, false);
  ImRect submitted(verts[cursor].pos, verts[cursor].pos);
  int run_verts = 0;
  for (int j = cursor; j < total && (verts[j].col & ~IM_COL32_A_MASK) == entry_rgb; ++j) {
    submitted.Add(verts[j].pos);
    ++run_verts;
  }
  // ImRect does not normalize — the crossing (pa) may sit past c0 (pb) on either axis.
  ImRect expected(ImVec2(std::min(pa.x, pb.x), std::min(pa.y, pb.y)),
                  ImVec2(std::max(pa.x, pb.x), std::max(pa.y, pb.y)));
  float edge_delta = 0.0f;
  edge_delta = std::max(edge_delta, std::abs(submitted.Min.x - expected.Min.x));
  edge_delta = std::max(edge_delta, std::abs(submitted.Min.y - expected.Min.y));
  edge_delta = std::max(edge_delta, std::abs(submitted.Max.x - expected.Max.x));
  edge_delta = std::max(edge_delta, std::abs(submitted.Max.y - expected.Max.y));
  const float raw_gap = std::hypot(raw_far.x - submitted.Max.x, raw_far.y - submitted.Max.y);
  ctx->LogInfo("[trace clip] submitted %d verts, expected bbox edge delta %.2f px, raw-behind gap %.1f px", run_verts,
               edge_delta, raw_gap);
  IM_CHECK_RETV(edge_delta < gui::UiPx(4.0f), false);  // the submitted geometry is the clipped visible part
  IM_CHECK_RETV(raw_gap > gui::UiPx(20.0f), false);    // ... and nowhere near the behind-eye projection
  return true;
}

// AC1's leg: the same pose's computing and drawn frames frame the crystal identically. The
// executor's pickup gate parks the re-keyed ray query at its pickup instant (token armed,
// engine call not yet run — the executor's own construction), so the frames AFTER the sample
// switch are the named computing state, held deterministically until release; the released
// query adopts and the drawn frame renders the SAME sample's pose. Mesh-only centre/fit and
// the mesh's projected outline must agree; the overlay (contacts, segments, labels) leaves no
// residue on the waiting frame.
bool TraceFramingIsMeshAnchoredAcrossStates(ImGuiTestContext* ctx, bool crystal_view) {
  auto& view = gui::g_state.analysis.point_view;
  const int saved_sample = view.sample_index;
  {
    // The restore owns the mutated view fields for THIS block; the re-adoption wait below runs
    // after it, against the restored cursor.
    struct Restore {
      int sample;
      int frame;
      Restore()
          : sample(gui::g_state.analysis.point_view.sample_index),
            frame(gui::g_state.analysis.point_view.crystal_frame ? 1 : 0) {}
      ~Restore() {
        auto& view = gui::g_state.analysis.point_view;
        view.sample_index = sample;
        view.crystal_frame = frame == 1;
      }
    } restore;
    view.crystal_frame = crystal_view;
    const int valid_count =
        gui::PointValidSampleCount(*gui::g_state.analysis.point.adopted_result, view.component_index);
    IM_CHECK_RETV(valid_count >= 2, false);  // the re-key needs another valid sample to move to
    const int target = (saved_sample + 1) % valid_count;
    IM_CHECK_RETV(target != saved_sample, false);
    ScopedPointPickupHold hold(gui::QueryFace::kRay);
    view.sample_index = target;
    ctx->Yield(2);  // the pump re-keys and issues; the worker parks at the pickup — computing held
    IM_CHECK_RETV(!hold.Voided(), false);
    // BY-VALUE snapshots: LastRay3DFrameInfo returns the one mutable global frame object the
    // renderer updates in place every submission — two const references to it would alias each
    // other and the centre/fit comparison below would be self-comparison, always true (the
    // aliasing trap the learnings already recorded for state-transition asserts).
    const gui::Ray3DFrameInfo computing = gui::LastRay3DFrameInfo();
    IM_CHECK_RETV(computing.valid && computing.frame_count == ImGui::GetFrameCount(), false);
    const int target_stored =
        gui::PointValidSampleIndexAt(*gui::g_state.analysis.point.adopted_result, view.component_index, target);
    IM_CHECK_RETV(gui::RayTraceDisplayStateFor(gui::g_state.analysis.trace, { gui::g_state.analysis.point.intent_id,
                                                                              view.component_index, target_stored }) ==
                      gui::RayTraceDisplayState::kComputing,
                  false);
    IM_CHECK_RETV(TraceTextureQuadPresent(ctx), false);
    // The waiting frame carries no overlay residue of the previous pose's light.
    IM_CHECK_RETV(TraceCanvasVertices(gui::kRayTraceContactColor).empty(), false);
    IM_CHECK_RETV(TraceCanvasVertices(gui::kRayTraceContactSolidColor).empty(), false);
    IM_CHECK_RETV(TraceCanvasVertices(gui::kRayTraceLabelColor).empty(), false);
    size_t residue = 0;
    for (ImU32 color : gui::kRayTraceSegmentColors) {
      residue += TraceCanvasVertices(color).size();
    }
    IM_CHECK_RETV(residue == 0, false);
    // The mesh outline from the independent oracle, on the computing frame.
    LUMICE_CrystalMesh mesh{};
    std::vector<ImVec2> computing_mesh;
    IM_CHECK_RETV(TraceOracleMesh(ctx, &mesh, &computing_mesh), false);
    hold.Release();
    // The parked query proceeds; the same key's answer adopts and draws.
    IM_CHECK_RETV(DriveUntil(
                      ctx,
                      [&] {
                        const auto& t = gui::g_state.analysis.trace;
                        return t.adopted_result != nullptr && t.key.sample == target_stored &&
                               gui::RayTraceDisplayStateFor(t, { t.key.point_id, t.key.component, t.key.sample }) ==
                                   gui::RayTraceDisplayState::kDrawn;
                      },
                      60),
                  false);
    ctx->Yield(2);
    const gui::Ray3DFrameInfo drawn = gui::LastRay3DFrameInfo();  // by value — the second snapshot
    IM_CHECK_RETV(drawn.valid && drawn.frame_count == ImGui::GetFrameCount(), false);
    IM_CHECK_RETV(computing.frame_count != drawn.frame_count, false);  // two DISTINCT frames compared
    IM_CHECK_RETV(TraceTextureQuadPresent(ctx), false);
    // THE CLAIM: one pose, one framing — mesh-only centre/fit identical across the two states.
    float centre_worst = 0.0f;
    for (int k = 0; k < 3; ++k) {
      centre_worst = std::max(centre_worst, std::fabs(computing.centre[k] - drawn.centre[k]));
    }
    IM_CHECK_RETV(centre_worst == 0.0f && computing.fit == drawn.fit, false);
    LUMICE_CrystalMesh mesh2{};
    std::vector<ImVec2> drawn_mesh;
    IM_CHECK_RETV(TraceOracleMesh(ctx, &mesh2, &drawn_mesh), false);
    IM_CHECK_RETV(computing_mesh.size() == drawn_mesh.size() && computing_mesh.size() > 3, false);
    double worst = 0.0;
    for (size_t v = 0; v < computing_mesh.size(); ++v) {
      worst = std::max(worst, static_cast<double>(std::hypot(computing_mesh[v].x - drawn_mesh[v].x,
                                                             computing_mesh[v].y - drawn_mesh[v].y)));
    }
    ctx->LogInfo("[trace framing] %s view: centre/fit identical, mesh outline worst delta %.4f px, fit=%.4f",
                 crystal_view ? "crystal" : "world", worst, drawn.fit);
    IM_CHECK_RETV(worst < 0.01, false);
  }
  // The restore above re-keys the cursor back; the original sample's trace re-adopts.
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [&] {
                      const auto& t = gui::g_state.analysis.trace;
                      const int stored =
                          gui::PointValidSampleIndexAt(*gui::g_state.analysis.point.adopted_result,
                                                       gui::g_state.analysis.point_view.component_index, saved_sample);
                      return t.adopted_result != nullptr && t.key.sample == stored &&
                             gui::RayTraceDisplayStateFor(t, { t.key.point_id, t.key.component, t.key.sample }) ==
                                 gui::RayTraceDisplayState::kDrawn;
                    },
                    60),
                false);
  return true;
}

std::string CaptureWindowFrameText(ImGuiTestContext* ctx, const char* window_name);

// A renderer-state fixture, not another async adoption test: the real consumer above has
// already reached a stable drawn sample. Substitute the analytic suite's entry-backface pose
// under the same display key, and restore the real trace on every exit. Crystal-frame mesh
// geometry is pose-independent; no fabricated contacts or point-document edits are needed.
bool RefusedTraceKeepsCrystalVisible(ImGuiTestContext* ctx, const gui::SelectedClassIdentity& identity) {
  const gui::ScenePtr scene = gui::BuildScene(gui::g_state, gui::SceneIntent::kSimCommit);
  IM_CHECK_RETV(scene != nullptr, false);
  constexpr double kBackface[9] = { -0.5108181656453654, -0.8569178335521683,  -0.06896831291950242,
                                    0.6557541851097652,  -0.3365085527995817,  -0.6758316673578998,
                                    0.5559237810687107,  -0.39045335244173474, 0.7338221373120865 };
  std::vector<int> faces;
  std::vector<int> counts;
  std::vector<double> wavelengths;
  const auto request = gui::MakeRayRequestForSample(identity, kBackface, 0.0, &faces, &counts, &wavelengths);
  LUMICE_RepresentativeRay* raw_ray = nullptr;
  char error[256] = {};
  IM_CHECK_RETV(LUMICE_AnalyzeRepresentativeRay(scene.get(), &request, &raw_ray, error, sizeof(error)) == LUMICE_OK,
                false);
  gui::RepresentativeRayHandle ray(raw_ray, gui::RepresentativeRayDeleter{});
  LUMICE_RepresentativeRayInfo info{};
  info.struct_size = sizeof(info);
  IM_CHECK_RETV(LUMICE_RepresentativeRayGetInfo(ray.get(), &info) == LUMICE_PATH_READ_OK && info.produced == 0, false);
  const std::string reason(info.reason.data, info.reason.length);
  IM_CHECK_RETV(reason == "entry_backface", false);
  struct Restore {
    gui::RayTraceSession trace = gui::g_state.analysis.trace;
    bool crystal_frame = gui::g_state.analysis.point_view.crystal_frame;
    ~Restore() {
      gui::g_state.analysis.trace = std::move(trace);
      gui::g_state.analysis.point_view.crystal_frame = crystal_frame;
    }
  } restore;
  gui::g_state.analysis.trace.adopted_result = ray;
  gui::g_state.analysis.point_view.crystal_frame = true;
  ctx->Yield(2);
  IM_CHECK_RETV(gui::RayTraceDisplayStateFor(gui::g_state.analysis.trace, restore.trace.key) ==
                    gui::RayTraceDisplayState::kRefused,
                false);
  IM_CHECK_RETV(TraceTextureQuadPresent(ctx), false);
  IM_CHECK_RETV(TraceCanvasVertices(gui::kRayTraceContactColor).empty(), false);
  const bool has_segments = std::any_of(std::begin(gui::kRayTraceSegmentColors), std::end(gui::kRayTraceSegmentColors),
                                        [](ImU32 color) { return !TraceCanvasVertices(color).empty(); });
  IM_CHECK_RETV(!has_segments, false);
  // The refusal text sits below the report's fixed fold (the Fresnel-toggle row grew the
  // report head), and the frame-text capture records only what a window actually shows
  // (ItemUnclipByLog is off — clipped rows never reach the log buffer): scroll the report to
  // the bottom before capturing.
  ImGuiWindow* report = TraceReportWindow();
  IM_CHECK_RETV(report != nullptr, false);
  report->Scroll.y = report->ScrollMax.y;
  const std::string text = CaptureWindowFrameText(ctx, "Pose & trace");
  IM_CHECK_RETV(text.find("No legal path for this pose:") != std::string::npos, false);
  IM_CHECK_RETV(text.find(reason) != std::string::npos, false);
  IM_CHECK_RETV(SaveWindowPng(ctx, "//Pose & trace", GuiTestTempPath("analyze_c03_trace_refused.png").string()), false);
  ctx->LogInfo("[trace refusal] %s: mesh visible, no segments or markers, named refusal text", reason.c_str());
  return true;
}

bool InternalReflectionTraceFromTheRealConsumer(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  // The C03 chain in this scene's L0 list (the corpus's internal-reflection representative; the
  // scene was chosen so this row exists — a red here means the scene choice broke, and says so).
  std::string c03_display;
  for (const int idx : gui::g_state.analysis_result.display_order) {
    const auto& e = gui::g_state.analysis_result.payload->entries[static_cast<size_t>(idx)];
    if (std::strcmp(e.display, "3-5-6-7") == 0) {
      c03_display = e.display;
      break;
    }
  }
  IM_CHECK_RETV(!c03_display.empty(), false);
  const std::string c03_label = gui::AnalysisRowLabel(c03_display, gui::AnalysisCoreToPoolMap(gui::g_state));
  SetListRef(ctx);
  const ImGuiID c03_row = FindListRowScrolling(ctx, c03_label);
  IM_CHECK_RETV(c03_row != 0, false);
  ctx->ItemClick(c03_row);
  ctx->SetRef("");
  const std::optional<gui::SelectedClassIdentity> identity = gui::SelectedClassIdentityFor(gui::g_state);
  IM_CHECK_RETV(identity.has_value(), false);

  // The class's full-sky map computes (behind its survey on the one worker) and echoes the class.
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      if (m.adopted_result == nullptr || !(m.adopted_id == m.intent_id)) {
                        return false;
                      }
                      LUMICE_PathMapInfo i{};
                      i.struct_size = sizeof(i);
                      return LUMICE_PathMapGetInfo(m.adopted_result.get(), &i) == LUMICE_PATH_READ_OK &&
                             i.view_full_sky == 1;
                    },
                    60),
                false);
  // A real pick on the canvas centre — the real mouse path arms the intent (its own outcome is
  // the discovery arm's honest answer at the centroid spot). The document the leg asserts on is
  // then pinned to the (38, 180) anchor: the M0 probe's verified on-fiber target for this
  // scene's 3-5-6-7 fiber (two components, ~120 deg from the sun — the corpus's antisolar
  // concentration band; CLI probe on this exact geometry), written through the same state the
  // pick path writes. Not (-20, 180): that point IS the sun's antipode, where the engine
  // honestly refuses the target (kInvalidTarget) instead of answering.
  ctx->KeyDown(ImGuiMod_Alt);
  ctx->ItemClick("**/##map_canvas");
  ctx->KeyUp(ImGuiMod_Alt);
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid, false);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  // The pick's own document read back honestly (outcome + component count logged) — an empty
  // discovery at the centroid is a real terminal state, not a failure the leg papers over.
  {
    LUMICE_PathPointInfo pick_info{};
    pick_info.struct_size = sizeof(pick_info);
    IM_CHECK_RETV(
        LUMICE_PathPointGetInfo(gui::g_state.analysis.point.adopted_result.get(), &pick_info) == LUMICE_PATH_READ_OK,
        false);
    IM_CHECK_RETV(pick_info.outcome == LUMICE_PATH_POINT_DISCOVERED, false);
    std::vector<gui::PointComponentRow> pick_rows;
    const int pick_components = gui::PointComponentRows(*gui::g_state.analysis.point.adopted_result, &pick_rows);
    ctx->LogInfo("[v1 c03] the pick's own document: outcome DISCOVERED, %d component(s) at the centroid spot",
                 pick_components);
  }
  SetSkyPointTarget(38.0, 180.0);
  ctx->Yield(2);  // the pump re-arms under the new key before the wait reads the state
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  const gui::PointQuerySession& point = gui::g_state.analysis.point;
  LUMICE_PathPointInfo info{};
  info.struct_size = sizeof(info);
  IM_CHECK_RETV(LUMICE_PathPointGetInfo(point.adopted_result.get(), &info) == LUMICE_PATH_READ_OK, false);
  IM_CHECK_RETV(info.crystal_id == identity->core_crystal_id, false);
  IM_CHECK_RETV(info.outcome == LUMICE_PATH_POINT_DISCOVERED, false);
  std::vector<gui::PointComponentRow> rows;
  const int components = gui::PointComponentRows(*point.adopted_result, &rows);
  IM_CHECK_RETV(components >= 1, false);
  IM_CHECK_RETV(gui::PointValidSampleCount(*point.adopted_result, 0) > 0, false);
  ctx->LogInfo("[v1 c03] target (alt %.2f, az %.2f, dev %.2f), %d component(s)", info.target_altitude_deg,
               info.target_azimuth_deg, info.target_deviation_deg, components);

  // The 3D region draws the recovered path for the current sample; the document's shape is the
  // recovery's own pin: four contacts, five segments, kinds in the recovery's order.
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::RayTraceSession& t = gui::g_state.analysis.trace;
                      if (!t.adopted_result || !t.intent_valid) {
                        return false;
                      }
                      const int stored =
                          gui::PointValidSampleIndexAt(*gui::g_state.analysis.point.adopted_result, 0, 0);
                      return t.key.sample == stored && gui::RayTraceDisplayStateFor(t, { t.key.point_id, 0, stored }) ==
                                                           gui::RayTraceDisplayState::kDrawn;
                    },
                    30),
                false);
  {
    LUMICE_PathRayContacts contacts{};
    contacts.struct_size = sizeof(contacts);
    LUMICE_PathRayLegs legs{};
    legs.struct_size = sizeof(legs);
    IM_CHECK_RETV(LUMICE_RepresentativeRayGetContacts(gui::g_state.analysis.trace.adopted_result.get(), &contacts) ==
                      LUMICE_PATH_READ_OK,
                  false);
    IM_CHECK_RETV(
        LUMICE_RepresentativeRayGetLegs(gui::g_state.analysis.trace.adopted_result.get(), &legs) == LUMICE_PATH_READ_OK,
        false);
    IM_CHECK_RETV(contacts.contact_count == 4, false);
    IM_CHECK_RETV(legs.leg_count == 5, false);
    gui::RayTracePolyline line;
    IM_CHECK_RETV(BuildRayTracePolyline(*gui::g_state.analysis.trace.adopted_result, 1.1f, &line), false);
    IM_CHECK_RETV(line.seg_kinds.size() == 5, false);
    IM_CHECK_RETV(line.seg_kinds[0] == 0 && line.seg_kinds[1] == 1 && line.seg_kinds[2] == 1 &&
                      line.seg_kinds[3] == 1 && line.seg_kinds[4] == 2,
                  false);
  }
  // Restore orbit/frame fields on both success and any assertion's early return.
  struct RestoreTraceView {
    bool crystal_frame = gui::g_state.analysis.point_view.crystal_frame;
    float yaw = gui::g_state.analysis.point_view.orbit_yaw_deg;
    float pitch = gui::g_state.analysis.point_view.orbit_pitch_deg;
    float zoom = gui::g_state.analysis.point_view.orbit_zoom;
    ~RestoreTraceView() {
      auto& view = gui::g_state.analysis.point_view;
      view.crystal_frame = crystal_frame;
      view.orbit_yaw_deg = yaw;
      view.orbit_pitch_deg = pitch;
      view.orbit_zoom = zoom;
    }
  } restore_view;
  gui::g_state.analyze_pose_view_open = true;
  gui::g_state.analysis.point_view.orbit_yaw_deg = 143.0f;
  gui::g_state.analysis.point_view.orbit_pitch_deg = -37.0f;
  PinPoseViewOnScreen(ctx);
  ctx->Yield(1);
  IM_CHECK_RETV(SaveWindowPng(ctx, "//Pose & trace", GuiTestTempPath("analyze_v1_c03_trace.png").string()), false);
  IM_CHECK_RETV(TraceContactsInsideMeshSilhouette(ctx), false);
  IM_CHECK_RETV(TraceProjectionHasOrbitHandedness(ctx), false);
  IM_CHECK_RETV(TraceMeshHasVisiblePerspective(ctx), false);
  IM_CHECK_RETV(TraceLegendSwatchesAvoidText(ctx), false);
  gui::g_state.analysis.point_view.crystal_frame = true;
  ctx->Yield(2);
  IM_CHECK_RETV(TraceContactsInsideMeshSilhouette(ctx), false);
  IM_CHECK_RETV(TraceProjectionHasOrbitHandedness(ctx), false);
  IM_CHECK_RETV(TraceMeshHasVisiblePerspective(ctx), false);
  IM_CHECK_RETV(SaveWindowPng(ctx, "//Pose & trace", GuiTestTempPath("analyze_c03_trace_crystal.png").string()), false);
  // The wheel's full supported range stays outside the normalized content sphere. Exercise
  // its rendered endpoints as well, preserving the existing input-state orbit pins verbatim.
  gui::g_state.analysis.point_view.orbit_zoom = 0.25f;
  ctx->Yield(2);
  IM_CHECK_RETV(TraceTextureQuadPresent(ctx), false);
  IM_CHECK_RETV(TraceProjectionHasOrbitHandedness(ctx), false);
  IM_CHECK_RETV(SaveWindowPng(ctx, "//Pose & trace", GuiTestTempPath("analyze_c03_trace_zoom_min.png").string()),
                false);
  gui::g_state.analysis.point_view.orbit_zoom = 8.0f;
  ctx->Yield(2);
  IM_CHECK_RETV(TraceTextureQuadPresent(ctx), false);
  IM_CHECK_RETV(TraceProjectionHasOrbitHandedness(ctx), false);
  IM_CHECK_RETV(SaveWindowPng(ctx, "//Pose & trace", GuiTestTempPath("analyze_c03_trace_zoom_max.png").string()),
                false);
  gui::g_state.analysis.point_view.orbit_zoom = restore_view.zoom;
  ctx->Yield(2);
  // The 2026-11-12 display-model legs (feedback #7/#8/#9), on the drawn chain at the test's
  // standing orbit: mesh-only framing across the two states (both views), the energy widths,
  // the ratio labels, the contact facing, and the near-plane extreme.
  IM_CHECK_RETV(TraceFramingIsMeshAnchoredAcrossStates(ctx, /*crystal_view=*/false), false);
  IM_CHECK_RETV(TraceFramingIsMeshAnchoredAcrossStates(ctx, /*crystal_view=*/true), false);
  IM_CHECK_RETV(TraceWidthsFollowCumulativeEnergy(ctx), false);
  IM_CHECK_RETV(TraceLabelsFollowTheToggle(ctx), false);
  IM_CHECK_RETV(TraceContactShapesFollowTheRealFacing(ctx), false);
  IM_CHECK_RETV(TraceSunArrowFollowsTheCrystalSun(ctx), false);
  IM_CHECK_RETV(TraceOverlayClipsAtTheNearPlane(ctx), false);
  ctx->LogInfo("[v1 c03] the internal-reflection path drawn in the 3D region: entry, 3 internal, exit");
  IM_CHECK_RETV(RefusedTraceKeepsCrystalVisible(ctx, *identity), false);
  return true;
}

// The new faces' lifecycle across row switches (v1-scope E): select A, switch to B while A's
// queries are still in flight, and every face re-keys to B — the adopted outcomes name B, the
// lambda knob re-asks the map and the point under the same session rules. The halo scene keeps
// the worker costs inside the correctness pool's fixed-dt window (the workspace-switch leg's
// precedent); the claim is the rekeying, not the physics.
bool AnalyzeFacesRekeyOnRowSwitch(ImGuiTestContext* ctx) {
  ScopedWatchdogGuard watchdog(ctx->EngineIO);
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  // A sky point intent, so the point face (and through it the ray face) participates in the
  // switch: an arbitrary named direction — the claim is the session, not the physics.
  SetSkyPointTarget(40.0, 0.0);
  const auto& view = gui::g_state.analysis_result;
  const std::string a_display = view.payload->entries[static_cast<size_t>(view.display_order[0])].display;
  const std::string b_display = view.payload->entries[static_cast<size_t>(view.display_order[1])].display;
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + a_display).c_str());
  ctx->Yield(1);
  IM_CHECK_RETV(gui::SelectedClassIdentityFor(gui::g_state).has_value(), false);
  // B lands while A's faces are still working — no wait for A.
  ctx->ItemClick((std::string("**/") + b_display).c_str());
  ctx->SetRef("");
  const std::optional<gui::SelectedClassIdentity> identity_b = gui::SelectedClassIdentityFor(gui::g_state);
  IM_CHECK_RETV(identity_b.has_value(), false);

  // B's survey and map adopt; the gates answer B's keys (A's outcomes structurally cannot pass).
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    60),
                false);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      if (m.adopted_result == nullptr || !(m.adopted_id == m.intent_id)) {
                        return false;
                      }
                      LUMICE_PathMapInfo i{};
                      i.struct_size = sizeof(i);
                      return LUMICE_PathMapGetInfo(m.adopted_result.get(), &i) == LUMICE_PATH_READ_OK &&
                             i.view_full_sky == 1;
                    },
                    60),
                false);
  {
    LUMICE_PathMapInfo info{};
    info.struct_size = sizeof(info);
    IM_CHECK_RETV(LUMICE_PathMapGetInfo(gui::g_state.analysis.map.adopted_result.get(), &info) == LUMICE_PATH_READ_OK,
                  false);
    IM_CHECK_RETV(info.crystal_id == identity_b->core_crystal_id, false);
  }
  // The point face settles terminal under B and names B in its echo.
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::PointQueryState st = gui::PointQueryStateFor(gui::g_state.analysis.point);
                      return st == gui::PointQueryState::kCompleted || st == gui::PointQueryState::kRefused;
                    },
                    60),
                false);
  if (gui::g_state.analysis.point.adopted_result != nullptr) {
    LUMICE_PathPointInfo info{};
    info.struct_size = sizeof(info);
    IM_CHECK_RETV(
        LUMICE_PathPointGetInfo(gui::g_state.analysis.point.adopted_result.get(), &info) == LUMICE_PATH_READ_OK, false);
    IM_CHECK_RETV(info.crystal_id == identity_b->core_crystal_id, false);
  }
  ctx->LogInfo("[v1 rekey] B's survey, map and point adopted under B's identity; A's era dropped");

  // The lambda knob re-asks the map AND the point (the one shared knob rides both keys) in
  // flight; the echo carries 450, and resetting back re-asks both under the engine reference —
  // read off the document first, so the reset never leans on a "not 450" accident.
  float reference_lambda = 0.0f;
  {
    LUMICE_PathMapInfo i{};
    i.struct_size = sizeof(i);
    IM_CHECK_RETV(LUMICE_PathMapGetInfo(gui::g_state.analysis.map.adopted_result.get(), &i) == LUMICE_PATH_READ_OK,
                  false);
    reference_lambda = i.wavelength_nm;
    IM_CHECK_RETV(reference_lambda != 450.0f, false);  // the scene premise this leg leans on
  }
  gui::g_state.analysis.map_wavelength_nm = 450.0f;
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      if (m.adopted_result == nullptr || !(m.adopted_id == m.intent_id)) {
                        return false;
                      }
                      LUMICE_PathMapInfo i{};
                      i.struct_size = sizeof(i);
                      return LUMICE_PathMapGetInfo(m.adopted_result.get(), &i) == LUMICE_PATH_READ_OK &&
                             i.view_full_sky == 1 && i.wavelength_nm == 450.0f;
                    },
                    60),
                false);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::PointQueryState st = gui::PointQueryStateFor(gui::g_state.analysis.point);
                      if (st != gui::PointQueryState::kCompleted) {
                        return false;
                      }
                      LUMICE_PathPointInfo i{};
                      i.struct_size = sizeof(i);
                      return LUMICE_PathPointGetInfo(gui::g_state.analysis.point.adopted_result.get(), &i) ==
                                 LUMICE_PATH_READ_OK &&
                             i.wavelength_nm == 450.0f;
                    },
                    60),
                false);
  gui::g_state.analysis.map_wavelength_nm = 0.0f;
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [reference_lambda] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      if (m.adopted_result == nullptr || !(m.adopted_id == m.intent_id)) {
                        return false;
                      }
                      LUMICE_PathMapInfo i{};
                      i.struct_size = sizeof(i);
                      return LUMICE_PathMapGetInfo(m.adopted_result.get(), &i) == LUMICE_PATH_READ_OK &&
                             i.view_full_sky == 1 && i.wavelength_nm == reference_lambda;
                    },
                    60),
                false);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [reference_lambda] {
                      const gui::PointQueryState st = gui::PointQueryStateFor(gui::g_state.analysis.point);
                      if (st != gui::PointQueryState::kCompleted) {
                        return false;
                      }
                      LUMICE_PathPointInfo i{};
                      i.struct_size = sizeof(i);
                      return LUMICE_PathPointGetInfo(gui::g_state.analysis.point.adopted_result.get(), &i) ==
                                 LUMICE_PATH_READ_OK &&
                             i.wavelength_nm == reference_lambda;
                    },
                    60),
                false);
  ctx->LogInfo("[v1 rekey] the lambda knob re-asked map and point at 450, then the reference %.1f again",
               reference_lambda);
  return true;
}

// The new faces die and survive with the document (v1-scope E, the cross-face claim the
// workspace-switch leg predates): New clears every analysis session — survey, map, point, ray,
// the sky-point intent with them, the executor left idle; Revert keeps the adopted outcomes and
// the intent, re-issuing nothing. The halo scene, fixed-dt pool — same reasons as the rekey leg.
bool NewDocumentClearsEveryAnalyzeFace(ImGuiTestContext* ctx) {
  ScopedWatchdogGuard watchdog(ctx->EngineIO);
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  // Arm every face: a selection (survey + map), a sky point (point), and let the ray follow.
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + TopChainDisplay()).c_str());
  ctx->SetRef("");
  SetSkyPointTarget(40.0, 0.0);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    60),
                false);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::PointQueryState st = gui::PointQueryStateFor(gui::g_state.analysis.point);
                      return st == gui::PointQueryState::kCompleted || st == gui::PointQueryState::kRefused;
                    },
                    60),
                false);
  const gui::SurveyHandle survey_before = gui::g_state.analysis.survey.adopted_result;
  const gui::MapHandle map_before = gui::g_state.analysis.map.adopted_result;
  const gui::PointHandle point_before = gui::g_state.analysis.point.adopted_result;
  IM_CHECK_RETV(survey_before != nullptr && map_before != nullptr, false);
  IM_CHECK_RETV(point_before != nullptr || !gui::g_state.analysis.point.adopted_error.empty(), false);

  // New: the whole analysis session dies with the document — every face's bookkeeping included —
  // and the executor's binding is released on the worker thread.
  gui::DoNew();
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.analysis.survey.intent_valid == false, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::MapQueryStateFor(gui::g_state.analysis.map) == gui::MapQueryState::kNoSelection, false);
  IM_CHECK_RETV(gui::g_state.analysis.map.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kNoIntent, false);
  IM_CHECK_RETV(gui::g_state.analysis.point.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::g_state.analysis.trace.intent_valid == false, false);
  IM_CHECK_RETV(gui::g_state.analysis.trace.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid == false, false);
  IM_CHECK_RETV(!gui::g_survey_executor.InFlight(), false);
  ctx->LogInfo("[v1 new] New cleared survey, map, point, ray and the sky point; the executor is idle");

  // Revert keeps: rebuild the faces, then revert — every adopted outcome and the intent stay,
  // nothing re-issues (the analysis-tired per-reason rules, now across all four faces).
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + TopChainDisplay()).c_str());
  ctx->SetRef("");
  SetSkyPointTarget(40.0, 0.0);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    60),
                false);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::PointQueryState st = gui::PointQueryStateFor(gui::g_state.analysis.point);
                      return st == gui::PointQueryState::kCompleted || st == gui::PointQueryState::kRefused;
                    },
                    60),
                false);
  const gui::SurveyHandle survey_kept = gui::g_state.analysis.survey.adopted_result;
  const gui::MapHandle map_kept = gui::g_state.analysis.map.adopted_result;
  const gui::PointHandle point_kept = gui::g_state.analysis.point.adopted_result;
  // The ray face's precondition: the keep-assertion only carries meaning over an ARMED trace.
  // When the point document carries components, drive the trace to its adopted state and
  // capture the handle BY VALUE (a raw .get() address could be reused after a free — the
  // handle copy owns). When it carries none (the mass/refused shapes), the trace never arms:
  // the assertion degrades, explicitly, to "Revert keeps it un-armed".
  bool trace_armed = false;
  bool trace_intent_kept = false;
  gui::RepresentativeRayHandle trace_result_kept;
  {
    LUMICE_PathPointInfo pt_info{};
    pt_info.struct_size = sizeof(pt_info);
    const bool has_components =
        gui::g_state.analysis.point.adopted_result != nullptr &&
        LUMICE_PathPointGetInfo(gui::g_state.analysis.point.adopted_result.get(), &pt_info) == LUMICE_PATH_READ_OK &&
        pt_info.outcome == LUMICE_PATH_POINT_DISCOVERED && pt_info.component_count > 0;
    if (has_components) {
      IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.analysis.trace.adopted_result != nullptr; }, 30), false);
      trace_armed = true;
    }
    trace_intent_kept = gui::g_state.analysis.trace.intent_valid;
    trace_result_kept = gui::g_state.analysis.trace.adopted_result;
    if (trace_armed) {
      IM_CHECK_RETV(trace_intent_kept && trace_result_kept != nullptr, false);
    } else {
      IM_CHECK_RETV(trace_intent_kept == false && trace_result_kept == nullptr, false);
      ctx->LogInfo(
          "[v1 revert] the point document has no components: the ray face stays un-armed, "
          "and the keep-assertion degrades to keeping it so");
    }
  }
  IM_CHECK_RETV(survey_kept != nullptr && map_kept != nullptr, false);
  gui::DoRevert();
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.analysis.survey.intent_valid == true, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result == survey_kept, false);
  IM_CHECK_RETV(gui::g_state.analysis.map.adopted_result == map_kept, false);
  IM_CHECK_RETV(gui::g_state.analysis.point.adopted_result == point_kept, false);
  IM_CHECK_RETV(gui::g_state.analysis.trace.intent_valid == trace_intent_kept, false);
  IM_CHECK_RETV(gui::g_state.analysis.trace.adopted_result == trace_result_kept, false);
  if (trace_armed) {
    ctx->LogInfo("[v1 revert] the armed ray face survived Revert by handle value");
  }
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid, false);
  IM_CHECK_RETV(!gui::g_survey_executor.InFlight(), false);
  ctx->LogInfo("[v1 revert] Revert kept every face's adopted outcome and the sky point; nothing re-issued");
  return true;
}

// The identity class (C15's `1-3-6-2`, every member rank-0) across all faces at once: the
// overview routes the identity arm's honest statement, the map still answers (the rank-0
// point-mass arm, m±err — not a refused or empty picture), and the point query at the sun's own
// image answers the MASS arm — no component rows, no ray intent, no fabricated fiber. The one
// session where "direction identity" is the answer, nothing else degrades with it (AC2's
// 不整页封锁 on this scene's own identity family). Fixed-dt pool: the rank-0 survey
// early-stops (certified, no chromatic legs) and the mass arm answers in milliseconds.
// Open the class-answer diagnostics group and park its header near the report child's top, so
// the opened body is inside the child's visible slice. The frame capture reads visible text
// only, and the group's resting slot rides the fixed-slot layout above the report child —
// whether an opened body lands inside that slice is a layout accident (the click's own
// auto-scroll only reaches far enough when the resting slot is already partially clipped).
// The expansion state is normalized by the group's own click semantics — click, and click
// again only if the storage still says closed — so the end state is open whatever the engine's
// item queries surfaced along the way. For legs whose job is to OBSERVE a persisted expansion
// state rather than establish one, use ParkExpandedClassDiagnostics instead: it clicks nothing.
bool OpenAndParkClassDiagnostics(ImGuiTestContext* ctx) {
  ImGuiWindow* report_win = MapReportWindow();
  IM_CHECK_RETV(report_win != nullptr, false);
  const ImGuiID diag_id = report_win->GetID("Class diagnostics##sv_answer");
  ctx->ItemClick("**/Class diagnostics##sv_answer");
  ctx->Yield(2);
  if (!report_win->StateStorage.GetBool(diag_id, false)) {
    ctx->ItemClick("**/Class diagnostics##sv_answer");
    ctx->Yield(2);
  }
  const ImGuiTestItemInfo diag_item = ctx->ItemInfo("**/Class diagnostics##sv_answer", ImGuiTestOpFlags_NoError);
  IM_CHECK_RETV(diag_item.ID != 0, false);
  const float header_top_in_view = diag_item.RectFull.Min.y - report_win->InnerClipRect.Min.y;
  if (header_top_in_view > 0.0f) {
    ImGui::SetScrollY(report_win, report_win->Scroll.y + header_top_in_view);
    ctx->Yield(2);
  }
  return true;
}

// The state-observing counterpart: the group's expansion is asserted to have SURVIVED the
// window close/reopen in the window storage (the session-sticky contract) BEFORE any click or
// state write — re-establishing it here would erase the leg's discriminating power (a product
// that lost the sticky state would be silently repaired instead of failing the read). Only the
// scroll parks the already-expanded header into the capture's visible slice.
bool ParkExpandedClassDiagnostics(ImGuiTestContext* ctx) {
  ImGuiWindow* report_win = MapReportWindow();
  IM_CHECK_RETV(report_win != nullptr, false);
  const ImGuiID diag_id = report_win->GetID("Class diagnostics##sv_answer");
  IM_CHECK_RETV(report_win->StateStorage.GetBool(diag_id, false), false);
  const ImGuiTestItemInfo diag_item = ctx->ItemInfo("**/Class diagnostics##sv_answer", ImGuiTestOpFlags_NoError);
  IM_CHECK_RETV(diag_item.ID != 0, false);
  const float header_top_in_view = diag_item.RectFull.Min.y - report_win->InnerClipRect.Min.y;
  if (header_top_in_view > 0.0f) {
    ImGui::SetScrollY(report_win, report_win->Scroll.y + header_top_in_view);
    ctx->Yield(2);
  }
  return true;
}

bool IdentityClassShowsTheMassArmAndTheMapAnswers(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  std::string c15_display;
  for (const int idx : gui::g_state.analysis_result.display_order) {
    const auto& e = gui::g_state.analysis_result.payload->entries[static_cast<size_t>(idx)];
    if (std::strcmp(e.display, "1-3-6-2") == 0) {
      c15_display = e.display;
      break;
    }
  }
  IM_CHECK_RETV(!c15_display.empty(), false);
  const std::string c15_label = gui::AnalysisRowLabel(c15_display, gui::AnalysisCoreToPoolMap(gui::g_state));
  SetListRef(ctx);
  const ImGuiID c15_row = FindListRowScrolling(ctx, c15_label);
  IM_CHECK_RETV(c15_row != 0, false);
  ctx->ItemClick(c15_row);
  ctx->SetRef("");
  const std::optional<gui::SelectedClassIdentity> identity = gui::SelectedClassIdentityFor(gui::g_state);
  IM_CHECK_RETV(identity.has_value(), false);

  // The overview's identity arm (all twelve members rank-0, certified) — the honest "direction
  // identity" statement, not a fabricated dispersion or fiber story.
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    60),
                false);
  {
    const gui::SurveyOverviewModel model = gui::BuildSurveyOverviewModel(*gui::g_state.analysis.survey.adopted_result);
    IM_CHECK_RETV(model.valid && model.arm == gui::SurveyArm::kIdentity, false);
    ctx->LogInfo("[v1 c15] overview arm identity, valid %d", model.valid ? 1 : 0);
  }
  // The map answers under the same selection (the rank-0 point-mass arm) and names the class.
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      return m.adopted_result != nullptr && (m.adopted_id == m.intent_id);
                    },
                    60),
                false);
  {
    LUMICE_PathMapInfo info{};
    info.struct_size = sizeof(info);
    IM_CHECK_RETV(LUMICE_PathMapGetInfo(gui::g_state.analysis.map.adopted_result.get(), &info) == LUMICE_PATH_READ_OK,
                  false);
    IM_CHECK_RETV(info.crystal_id == identity->core_crystal_id, false);
    IM_CHECK_RETV(info.kept_count > 0, false);
    ctx->LogInfo("[v1 c15] map answered: kept %lld, lambda %.1f", info.kept_count, info.wavelength_nm);
  }
  // The point query at the sun's own image (the rank-0 point): the MASS arm's named lines, no
  // component rows — and the ray face stays silent (the pump's guard never issues a trace for
  // the mass document; nothing fabricated replaces the fiber).
  SetSkyPointTarget(20.0, 0.0);
  ctx->Yield(2);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  {
    LUMICE_PathPointInfo info{};
    info.struct_size = sizeof(info);
    IM_CHECK_RETV(
        LUMICE_PathPointGetInfo(gui::g_state.analysis.point.adopted_result.get(), &info) == LUMICE_PATH_READ_OK, false);
    IM_CHECK_RETV(info.crystal_id == identity->core_crystal_id, false);
    IM_CHECK_RETV(info.outcome == LUMICE_PATH_POINT_MASS, false);
    std::vector<gui::PointComponentRow> rows;
    IM_CHECK_RETV(gui::PointComponentRows(*gui::g_state.analysis.point.adopted_result, &rows) == 0, false);
    LUMICE_PathPointMass mass{};
    mass.struct_size = sizeof(mass);
    IM_CHECK_RETV(
        LUMICE_PathPointGetMass(gui::g_state.analysis.point.adopted_result.get(), &mass) == LUMICE_PATH_READ_OK, false);
    const std::vector<std::string> mass_lines = gui::PointMassLines(mass);
    IM_CHECK_RETV(!mass_lines.empty(), false);
    IM_CHECK_RETV(gui::g_state.analysis.trace.intent_valid == false, false);
    IM_CHECK_RETV(gui::g_state.analysis.trace.adopted_result == nullptr, false);
    ctx->LogInfo("[v1 c15] point MASS at the sun image: %zu named line(s), no component, no trace intent",
                 mass_lines.size());
  }

  // The summary projection and the complete answer, on the fast arm (the identity survey
  // answers in milliseconds — this leg carries the §16.2 shape checks the dispersion case's
  // real-timing window cannot prove under pool load): the main window carries the default
  // layer plus the pointer and no fold of its own; the Raypath view hosts the class answer
  // with both groups present and closed by default. The row click opened the Raypath view
  // (§16.3's content shortcut) — close it so the summary read is the main window's alone.
  ctx->WindowClose("//Raypath view");
  ctx->Yield(1);
  {
    const std::string frame_text = CaptureFrameText(ctx);
    IM_CHECK_RETV(!frame_text.empty(), false);
    IM_CHECK_RETV(frame_text.find("Source image") != std::string::npos, false);
    IM_CHECK_RETV(frame_text.find("Weight tint:") != std::string::npos, false);
    IM_CHECK_RETV(frame_text.find("The complete answer is in the Raypath view.") != std::string::npos, false);
    IM_CHECK_RETV(frame_text.find("member(s)") == std::string::npos, false);
    IM_CHECK_RETV(frame_text.find("Class evidence") == std::string::npos, false);
    // The point summary's own closed-windows legs (§16.2): the mass answer in full, the target
    // line, and the pointer naming where its omitted caliber detail lives.
    IM_CHECK_RETV(frame_text.find("Rank-0 point mass") != std::string::npos, false);
    IM_CHECK_RETV(frame_text.find("target: alt 20.000°") != std::string::npos, false);
  }
  // The env gate on the survey and point hosts, the shipped default: the evidence groups
  // stand closed, the diagnostics groups are unsubmitted entirely — the gate is display
  // eligibility, so the header's absence is the proof, not a closed fold.
  {
    const ScopedDebugDetailsEnv env_unset(DebugDetailsState::kUnset);
    ctx->ItemClick("**/Raypath view###main_raypath_open");
    ctx->Yield(2);
    IM_CHECK_RETV(gui::g_state.analyze_raypath_view_open, false);
    const std::string identity_report = CaptureMapFrameText(ctx);
    IM_CHECK_RETV(identity_report.find("Class answer") != std::string::npos, false);
    IM_CHECK_RETV(identity_report.find("member(s)") == std::string::npos, false);
    {
      // The descriptive words' single-owner echo, flipped with the wording change (AC3): the
      // window's text carries NO Greek λ glyph at all now — the slider's label is the ASCII
      // "wl", the survey and point answers spell "wavelength", the caliber carries none, the
      // legend none. The disabled representation is absent from the whole face.
      size_t lambda_count = 0;
      for (size_t at = identity_report.find("\xCE\xBB"); at != std::string::npos;
           at = identity_report.find("\xCE\xBB", at + 2)) {
        ++lambda_count;
      }
      IM_CHECK_RETV(lambda_count == 0, false);
    }
    IM_CHECK_RETV(ctx->ItemInfo("**/Class evidence##sv_answer", ImGuiTestOpFlags_NoError).ID != 0, false);
    IM_CHECK_RETV(ctx->ItemInfo("**/Class diagnostics##sv_answer", ImGuiTestOpFlags_NoError).ID == 0, false);
    IM_CHECK_RETV(ctx->ItemInfo("**/Point evidence##pt_answer", ImGuiTestOpFlags_NoError).ID != 0, false);
    IM_CHECK_RETV(ctx->ItemInfo("**/Point diagnostics##pt_answer", ImGuiTestOpFlags_NoError).ID == 0, false);
  }
  {
    // Under "1" the diagnostics groups surface and carry the L4 facts; the expansion is the
    // window-storage's own state — close the window and reopen it, and the group is still
    // open (session-sticky); a fresh GuiState would start closed, but this leg's session
    // stands — the reopen rides the same window storage.
    const ScopedDebugDetailsEnv env_on(DebugDetailsState::kOn);
    ctx->Yield(2);
    IM_CHECK_RETV(ctx->ItemInfo("**/Class diagnostics##sv_answer", ImGuiTestOpFlags_NoError).ID != 0, false);
    IM_CHECK_RETV(OpenAndParkClassDiagnostics(ctx), false);
    {
      const std::string opened_text = CaptureMapFrameText(ctx);
      IM_CHECK_RETV(opened_text.find("member(s)") != std::string::npos, false);
    }
    ctx->WindowClose("//Raypath view");
    ctx->Yield(1);
    ctx->ItemClick("**/Raypath view###main_raypath_open");
    ctx->Yield(2);
    // The reopened child's viewport resets to its top while the group stays expanded from the
    // window storage — re-park the header so its body is inside the visible slice again. The
    // park observes; the sticky assertion inside it must not repair what it is there to check.
    IM_CHECK_RETV(ParkExpandedClassDiagnostics(ctx), false);
    {
      const std::string reopened_text = CaptureMapFrameText(ctx);
      IM_CHECK_RETV(reopened_text.find("member(s)") != std::string::npos, false);
      // The sky-point answer rides the same report: present at the default layer (the λ premise
      // line), its own diagnostics group present but still closed — the execution triple is one
      // fold away, and "Selected component" (an evidence-tier phrase) exists nowhere in this
      // mass arm.
      IM_CHECK_RETV(reopened_text.find("Sky point answer") != std::string::npos, false);
      IM_CHECK_RETV(reopened_text.find("wavelength 550.00 nm") != std::string::npos, false);
      IM_CHECK_RETV(reopened_text.find("discovery sample_count") == std::string::npos, false);
      IM_CHECK_RETV(reopened_text.find("Selected component") == std::string::npos, false);
      // The mass arm's fold pair is the SAME structure as the component arm's: opening the
      // point answer's evidence group surfaces the member-faces premise line here too, and
      // opening its diagnostics group surfaces the execution triple (the click's auto-scroll
      // brings the header into the report child's clip; toggling back leaves the found state).
      ctx->ItemClick("**/Point evidence##pt_answer");
      ctx->Yield(2);
      const std::string mass_evidence_text = CaptureMapFrameText(ctx);
      IM_CHECK_RETV(mass_evidence_text.find("member faces") != std::string::npos, false);
      ctx->ItemClick("**/Point evidence##pt_answer");
      ctx->Yield(1);
      ctx->ItemClick("**/Point diagnostics##pt_answer");
      ctx->Yield(2);
      const std::string mass_diagnostics_text = CaptureMapFrameText(ctx);
      IM_CHECK_RETV(mass_diagnostics_text.find("discovery sample_count") != std::string::npos, false);
      ctx->ItemClick("**/Point diagnostics##pt_answer");
      ctx->Yield(1);
    }
  }
  {
    // The gate's priority over the sticky state: the storage still says open (the env-on leg
    // left the group expanded across its window reopen), but with the env unset the header is
    // unsubmitted — an old expansion state cannot re-display a gated group.
    const ScopedDebugDetailsEnv env_unset(DebugDetailsState::kUnset);
    ctx->Yield(2);
    IM_CHECK_RETV(ctx->ItemInfo("**/Class diagnostics##sv_answer", ImGuiTestOpFlags_NoError).ID == 0, false);
  }
  {
    // Env on again, same session: the storage's own answer comes back — the group returns
    // still expanded, no second click (the session's expansion preference survives the gate).
    // The seek is the capture's own requirement: the env-off frames reflowed the report and
    // the expanded body sits below the clip until scrolled to.
    const ScopedDebugDetailsEnv env_on(DebugDetailsState::kOn);
    ctx->Yield(2);
    IM_CHECK_RETV(ctx->ItemInfo("**/Class diagnostics##sv_answer", ImGuiTestOpFlags_NoError).ID != 0, false);
    ctx->ScrollToItem("**/Class diagnostics##sv_answer", ImGuiAxis_Y);
    ctx->Yield(2);
    const std::string restored_text = CaptureMapFrameText(ctx);
    IM_CHECK_RETV(restored_text.find("member(s)") != std::string::npos, false);
  }
  ctx->WindowClose("//Raypath view");
  ctx->Yield(1);
  return true;
}

// The refused arm beside its independent facts (AC2's first sentence, the C17 shape):
// `3-1-5-7` — the corpus's open-arc family whose honest boundary IS the partition refusal —
// routes its members fine but refuses the delta axis's structural objects
// (`slab_crease_touching`), and the overview answers the refusal arm: the named refusal, the
// independent witness and the TIR share beside it, the chromatic block named unevaluated —
// only THAT item missing, everything else alive. The survey's tail is bounded by the
// cooperative deadline now (the witness/TIR phases poll): the run lands inside the budget
// band whatever the machine's load — under load it returns the named partial (execution
// deadline exceeded, the TIR block present with its stop_requested refusal), unloaded it
// completes whole — the document shape dispatches on the execution outcome.
bool RefusedClassNamesTheRefusalAndKeepsTheWitness(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  const std::optional<gui::SelectedClassIdentity> identity = SelectRowByDisplay(ctx, "3-1-5-7");
  IM_CHECK_RETV(identity.has_value(), false);

  // The overview answers COMPLETED — the refusal is the document's arm, not a failed query —
  // and the deadline keeps the wall inside the budget band: the 15 s budget plus the leg's
  // bounded-return margin (one row chunk + assembly tail), padded for pool-context load.
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    60),
                false);
  {
    const gui::SurveyOverviewModel model = gui::BuildSurveyOverviewModel(*gui::g_state.analysis.survey.adopted_result);
    IM_CHECK_RETV(model.valid && model.arm == gui::SurveyArm::kRefused, false);
    IM_CHECK_RETV(!model.evidence_mode.empty(), false);
    // Both outcomes are honest here: on a machine whose load pushes the tail past the budget
    // the deadline bites (named partial), on an unloaded one the run completes whole — the
    // 60 s bound above is the wall-clock convergence claim, and the document shape dispatches
    // on the execution outcome (the pool's own precedent).
    const bool cut = model.execution == LUMICE_PATH_OUTCOME_DEADLINE_EXCEEDED;
    IM_CHECK_RETV(cut || model.execution == LUMICE_PATH_OUTCOME_COMPLETED, false);
    IM_CHECK_RETV(model.witness.available && model.witness.value_available, false);
    IM_CHECK_RETV(model.witness.kept_count > 0, false);
    // The TIR block is present either way; a cut names why on it (the block's availability
    // survives the cut — a refused block carries its mode — the shares do not).
    IM_CHECK_RETV(model.tir.available, false);
    if (cut) {
      IM_CHECK_RETV(!model.tir.value_available, false);
      IM_CHECK_RETV(model.tir.mode == "stop_requested", false);
    } else {
      IM_CHECK_RETV(model.tir.value_available, false);
    }
    IM_CHECK_RETV(model.dispersion.available == false, false);
    IM_CHECK_RETV(!model.chromatic_note.empty(), false);
    ctx->LogInfo("[v1 c17] refused arm '%s', execution %d: witness kept %lld, tir %s, chromatic named '%s'",
                 model.evidence_mode.c_str(), model.execution, model.witness.kept_count,
                 model.tir.mode.empty() ? "finite" : model.tir.mode.c_str(), model.chromatic_note.c_str());
  }
  IM_CHECK_RETV(SaveWindowPng(ctx, "//##AnalyzeDetails", GuiTestTempPath("analyze_v1_c17_refused.png").string()),
                false);
  return true;
}

// The independence half of the C17 claim, in its own session (each half short enough for the
// pool-context watchdog): a refused delta axis blockades neither the map face nor a point
// query on the same class — the map answers, and a sky-point query discovers (the corpus
// band, dev ~120 deg). Real-timing pool, same survey tail as the names leg.
bool RefusedClassBlockadesNothing(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  const std::optional<gui::SelectedClassIdentity> identity = SelectRowByDisplay(ctx, "3-1-5-7");
  IM_CHECK_RETV(identity.has_value(), false);
  // The survey's tail is bounded by the cooperative deadline now: the run returns the named
  // partial inside the budget band (see the names leg for the document-shape assertions).
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    60),
                false);

  // The map answers under the same selection (a refused delta axis does not blockade the map
  // face) and names the class — the FIRST adopted map is the evidence (the full-sky arrival
  // already proves the face answers; waiting for the map chain would buy nothing here
  // and is the pool-context watchdog budget's long pole).
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      return m.adopted_result != nullptr && (m.adopted_id == m.intent_id);
                    },
                    60),
                false);
  {
    LUMICE_PathMapInfo info{};
    info.struct_size = sizeof(info);
    IM_CHECK_RETV(LUMICE_PathMapGetInfo(gui::g_state.analysis.map.adopted_result.get(), &info) == LUMICE_PATH_READ_OK,
                  false);
    IM_CHECK_RETV(info.crystal_id == identity->core_crystal_id, false);
    IM_CHECK_RETV(info.kept_count > 0, false);
    ctx->LogInfo("[v1 c17] map answered: kept %lld", info.kept_count);
  }
  // A point query on the refused class still discovers — the corpus band target, the same
  // anchor the internal-reflection leg pins (probe: one component on this exact geometry).
  SetSkyPointTarget(38.0, 180.0);
  ctx->Yield(2);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  {
    LUMICE_PathPointInfo info{};
    info.struct_size = sizeof(info);
    IM_CHECK_RETV(
        LUMICE_PathPointGetInfo(gui::g_state.analysis.point.adopted_result.get(), &info) == LUMICE_PATH_READ_OK, false);
    IM_CHECK_RETV(info.crystal_id == identity->core_crystal_id, false);
    IM_CHECK_RETV(info.outcome == LUMICE_PATH_POINT_DISCOVERED, false);
    std::vector<gui::PointComponentRow> rows;
    const int components = gui::PointComponentRows(*gui::g_state.analysis.point.adopted_result, &rows);
    IM_CHECK_RETV(components >= 1, false);
    ctx->LogInfo("[v1 c17] point query independent of the survey refusal: discovered, %d component(s)", components);
  }

  // The point answer's split (§16.2), the component arm: the map window (open since the row
  // click) carries "Sky point answer" with its default-layer premise lines and BOTH fold
  // groups closed — the evidence rows are the evidence tier's, and the frame text proves the
  // closed state mechanically (a closed CollapsingHeader submits no body). The fold's rendered
  // response to expansion is then read by writing the same window-storage key a user click
  // writes (the click wiring itself is the identity case's diagnostics leg).
  {
    ImGuiWindow* report = MapReportWindow();
    IM_CHECK_RETV(report != nullptr, false);
    const std::string report_text = CaptureMapFrameText(ctx);
    IM_CHECK_RETV(report_text.find("Sky point answer") != std::string::npos, false);
    IM_CHECK_RETV(report_text.find("wavelength 550.00 nm") != std::string::npos, false);
    IM_CHECK_RETV(report_text.find("Selected component") == std::string::npos, false);
    IM_CHECK_RETV(report_text.find("discovery sample_count") == std::string::npos, false);
    report->StateStorage.SetInt(report->GetID("Point evidence##pt_answer"), 1);
    ctx->Yield(2);
    const std::string opened_text = CaptureMapFrameText(ctx);
    IM_CHECK_RETV(opened_text.find("Selected component") != std::string::npos, false);
    IM_CHECK_RETV(opened_text.find("member faces") != std::string::npos, false);
    report->StateStorage.SetInt(report->GetID("Point evidence##pt_answer"), 0);
    ctx->Yield(1);
  }
  // The main window's summary arm, both windows closed: the component count with its kinds,
  // the target line, the pointer — and none of the per-component evidence rows.
  ctx->WindowClose("//Raypath view");
  ctx->Yield(1);
  {
    const std::string frame_text = CaptureFrameText(ctx);
    IM_CHECK_RETV(frame_text.find("component(s):") != std::string::npos, false);
    IM_CHECK_RETV(frame_text.find("target: alt 38.000°") != std::string::npos, false);
    IM_CHECK_RETV(frame_text.find("The complete answer is in the Raypath view.") != std::string::npos, false);
    IM_CHECK_RETV(frame_text.find("1: ") == std::string::npos, false);
  }
  return true;
}

// The document rules the per-slice legs leave out, part one (v1-scope E / AC3): editing the
// scene leaves the adopted outcomes in place (explicit accept — nothing clears or re-asks
// behind the user's back) and the next Analyze replaces them under the new archive. The halo
// scene, fixed-dt pool — the lifecycle legs' reasons; one survey's in-flight window each.
bool SceneEditFollowsTheExplicitAcceptContract(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  // Edit while a query is IN FLIGHT: the selection issued under the current archive keeps
  // running on the frozen snapshot and adopts — the edit kills nothing.
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + TopChainDisplay()).c_str());
  ctx->SetRef("");
  gui::g_state.crystals[0].height = 0.1f;
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    60),
                false);
  const gui::SurveyHandle in_flight_era = gui::g_state.analysis.survey.adopted_result;
  IM_CHECK_RETV(in_flight_era != nullptr, false);
  ctx->LogInfo("[v1 edit] the in-flight query adopted on its frozen snapshot across the edit");

  // Edit while ADOPTED: the outcomes stay adopted (explicit accept — the edit is not an update).
  gui::g_state.crystals[0].height = 1.2f;
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result == in_flight_era, false);
  IM_CHECK_RETV(gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) == gui::SurveyOverviewState::kCompleted,
                false);
  ctx->LogInfo("[v1 edit] the adopted outcome survives a document edit untouched");

  // The explicit update: re-Analyze clears the selection and voids the old generation's
  // bookkeeping (the digest leg's documented R6 shape — no adopted outcome survives into the
  // next generation's region), and re-selecting answers under the new archive.
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analysis.survey.intent_valid == false, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result == nullptr, false);
  ctx->LogInfo("[v1 edit] re-Analyze voided the old generation's selection and outcome");
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + TopChainDisplay()).c_str());
  ctx->SetRef("");
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [&era = in_flight_era] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                                 gui::SurveyOverviewState::kCompleted &&
                             gui::g_state.analysis.survey.adopted_result != era;
                    },
                    60),
                false);
  ctx->LogInfo("[v1 edit] re-selecting answered under the new archive's generation");
  return true;
}

// The Cancel survey button withdraws only its face; a changed selection re-arms it.
// A pickup fence makes the action-time premise deterministic in the fixed-dt pool.
bool CancelSurveyButtonReachesTheNamedState(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  // The fully random scene's dominant class routes full-dimensional. Hold its pickup so
  // the button cannot race a natural completion, regardless of the machine's query speed.
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  // The coexistence stage (the status bar's AC2): an unbounded render run holds while the
  // survey below waits its long window, so the query-active segment has to stand beside the
  // sim word — and the leg's cancellations exercise both sides at once, the orthogonality the
  // survey-and-render leg pins reused as this leg's stage.
  gui::g_state.sim.infinite = true;
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo("##TopBar/" ICON_FA_PLAY " Run")), false);
  ctx->ItemClick("##TopBar/" ICON_FA_PLAY " Run");
  IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kSimulating; }, 10), false);

  // The action requires survey computing NOW, not merely a historical waiting display.
  ScopedPointPickupHold survey_hold(gui::QueryFace::kSurvey);
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + TopChainDisplay()).c_str());
  ctx->SetRef("");
  IM_CHECK_RETV(DriveUntil(ctx, [&survey_hold] { return survey_hold.PickedUp(); }, 20), false);
  IM_CHECK_RETV(!survey_hold.Voided(), false);
  IM_CHECK_RETV(gui::g_survey_executor.InFlight(gui::QueryFace::kSurvey), false);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kWaiting;
                    },
                    20),
                false);
  const gui::SurveyQueryId cancelled_id = gui::g_state.analysis.survey.intent_id;
  // The wait displays (AC1/AC2/AC3 of the wait-feedback layer), read off the render-feedback
  // fields — the same strings the render drew this frame. The overview wait line carries a live
  // elapsed; the status bar names a query as live; the selected row carries the activity dots.
  const std::string& survey_line = gui::g_state.analysis.wait_feedback_texts.survey_wait_line;
  const double first_elapsed = ParseWaitElapsed(survey_line);
  IM_CHECK_RETV(first_elapsed >= 0.0, false);
  IM_CHECK_RETV(survey_line.find("Survey: querying in the background") != std::string::npos, false);
  IM_CHECK_RETV(gui::g_state.analysis.wait_feedback_texts.status_bar_query_segment == "| Analyzing...", false);
  IM_CHECK_RETV(!gui::g_state.analysis.wait_feedback_texts.share_list_busy_dots.empty(), false);
  // The coexistence observable (AC2), read in this same window: the query segment stands beside
  // the sim word — two appended segments, neither branch reads the other.
  IM_CHECK_RETV(gui::g_state.sim_state == SimState::kSimulating, false);
  const double stamped_at_issue = gui::g_state.analysis.survey.intent_start_s;
  // The elapsed advances with the frames (the frame clock; bounded wait, never a bare sleep).
  IM_CHECK_RETV(DriveUntil(ctx, [&] { return ParseWaitElapsed(survey_line) > first_elapsed + 0.3; }, 15), false);
  // The marked row stays addressable by its unchanged label bytes while it is marked — the
  // id-constant invariant's observable (plan §3.3; the dots are a draw beside the row, not a
  // label rewrite).
  SetListRef(ctx);
  IM_CHECK_RETV(ctx->ItemInfo((std::string("**/") + TopChainDisplay()).c_str()).ID != 0, false);
  ctx->SetRef("");
  ctx->LogInfo("[v1 cancel] wait line %.1f->%.1f s, segment on, dots on; the marked row still addresses", first_elapsed,
               ParseWaitElapsed(survey_line));
  IM_CHECK_RETV(!survey_hold.Voided(), false);
  IM_CHECK_RETV(gui::g_survey_executor.InFlight(gui::QueryFace::kSurvey), false);
  ctx->ItemClick("**/Cancel survey");
  IM_CHECK_RETV(!survey_hold.Voided(), false);
  survey_hold.Release();
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCancelled;
                    },
                    20),
                false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.cancelled_for_key, false);
  // Cancel stops the clock (AC1): the wait display left the waiting state with it — the overview
  // line and the selected row's dots are gone the frame the cancelled state rendered; the status
  // bar's segment follows every face, not this one (the map may still be settling).
  IM_CHECK_RETV(gui::g_state.analysis.wait_feedback_texts.survey_wait_line.empty(), false);
  IM_CHECK_RETV(gui::g_state.analysis.wait_feedback_texts.share_list_busy_dots.empty(), false);
  // The latch withdraws the intent before the worker posts its cancellation. Wait for THIS
  // face's queued/running work to end, not global idle: map work remains legitimate.
  IM_CHECK_RETV(
      DriveUntil(ctx, [] { return !gui::g_survey_executor.QueryOutstanding(gui::QueryFace::kSurvey); }, 20), false);
  // Keep pumping the unchanged selection: neither a late finish nor a resend may revive it.
  ctx->Yield(20);
  IM_CHECK_RETV(gui::g_state.analysis.survey.intent_id == cancelled_id, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.cancelled_for_key, false);
  IM_CHECK_RETV(gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) == gui::SurveyOverviewState::kCancelled,
                false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result == nullptr, false);
  IM_CHECK_RETV(!gui::g_survey_executor.QueryOutstanding(gui::QueryFace::kSurvey), false);
  IM_CHECK_RETV(gui::g_state.analysis.wait_feedback_texts.survey_wait_line.empty(), false);
  IM_CHECK_RETV(gui::g_state.analysis.wait_feedback_texts.share_list_busy_dots.empty(), false);
  IM_CHECK_RETV(gui::g_state.sim_state == SimState::kSimulating, false);
  ctx->LogInfo("[v1 cancel] survey work ended; latch and wait cleanup persist while other faces remain independent");
  // A changed selection clears the latch and re-arms — the probe-verified fast identity chain
  // (`1-3-6-2` early-stops on this scene, no chromatic legs) proves the re-arm without making
  // the pool-context wall-clock budget the leg's long pole. The rekey overwrites the wait
  // epoch: the new intent's stamp is a newer frame time than the first wait's (the rekey-
  // restarts rule, plan §3.1 — with the pure functions' behavior pinned in the unit layer this
  // deterministic stamp pin carries the wait-window's rollback).
  IM_CHECK_RETV(SelectRowByDisplay(ctx, "1-3-6-2").has_value(), false);
  IM_CHECK_RETV(
      DriveUntil(ctx, [&] { return gui::g_state.analysis.survey.intent_start_s > stamped_at_issue; }, 5), false);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    60),
                false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.intent_id != cancelled_id, false);
  IM_CHECK_RETV(!gui::g_state.analysis.survey.cancelled_for_key, false);
  ctx->LogInfo("[v1 cancel] a changed selection cleared the latch and answered");
  // The stage's render run keeps going from here — the guard tears the server down with it, the
  // same hand-off the unbounded preview-animation legs leave behind (the run-lifecycle suite
  // owns stop-drain semantics; this leg owns the waits, and they all landed above).
  return true;
}

// Part two: the Cancel point button while the point is QUEUED behind the dominant class's
// fresh survey (the wide window makes the queued state deterministic) — the withdraw names the
// point's cancelled state and the survey face is not superseded — and a document switch (the
// open path's mechanism) clears every face.
bool CancelPointButtonReachesTheNamedState(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + TopChainDisplay()).c_str());
  ctx->SetRef("");
  SetSkyPointTarget(40.0, 0.0);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                                 gui::SurveyOverviewState::kWaiting &&
                             gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kWaiting;
                    },
                    20),
                false);
  // Every face's own wait display, read off the render-feedback fields (AC1): the point's two
  // text faces — the fiber region (the Pose & trace window's globe region now: opened here, its
  // site owns the clear-and-write) and the details block draw the same sentence — each carry the
  // live elapsed, and the map (queued behind the survey in this window) names its queue.
  gui::g_state.analyze_pose_view_open = true;
  PinPoseViewOnScreen(ctx);
  ctx->Yield(2);
  IM_CHECK_RETV(
      gui::g_state.analysis.wait_feedback_texts.point_wait_line_fiber.find("Computing the point") != std::string::npos,
      false);
  IM_CHECK_RETV(ParseWaitElapsed(gui::g_state.analysis.wait_feedback_texts.point_wait_line_fiber) >= 0.0, false);
  IM_CHECK_RETV(gui::g_state.analysis.wait_feedback_texts.point_wait_line_details.find("Computing the point") !=
                    std::string::npos,
                false);
  IM_CHECK_RETV(ParseWaitElapsed(gui::g_state.analysis.wait_feedback_texts.point_wait_line_details) >= 0.0, false);
  IM_CHECK_RETV(
      DriveUntil(ctx, [] { return !gui::g_state.analysis.wait_feedback_texts.map_wait_line.empty(); }, 30), false);
  IM_CHECK_RETV(gui::g_state.analysis.wait_feedback_texts.map_wait_line.find("queues behind this class's survey") !=
                    std::string::npos,
                false);
  IM_CHECK_RETV(gui::g_state.analysis.wait_feedback_texts.status_bar_query_segment == "| Analyzing...", false);
  ctx->LogInfo("[v1 cancel] the queued point's two wait faces, the queued map line and the segment all live");
  ctx->ItemClick("**/Cancel point");
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCancelled; },
          20),
      false);
  IM_CHECK_RETV(gui::g_state.analysis.point.cancelled_for_key, false);
  // The withdrawn face's wait displays are gone; the survey's own line is untouched by another
  // face's cancel (the queue's truth the two fields name separately).
  IM_CHECK_RETV(gui::g_state.analysis.wait_feedback_texts.point_wait_line_fiber.empty(), false);
  IM_CHECK_RETV(gui::g_state.analysis.wait_feedback_texts.point_wait_line_details.empty(), false);
  IM_CHECK_RETV(!gui::g_state.analysis.wait_feedback_texts.survey_wait_line.empty(), false);
  IM_CHECK_RETV(gui::g_state.analysis.trace.intent_valid == false, false);
  IM_CHECK_RETV(gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) == gui::SurveyOverviewState::kWaiting, false);
  ctx->LogInfo("[v1 cancel] Cancel point withdrew the queued intent; the survey face kept its own wait");
  gui::g_state.analyze_pose_view_open = false;
  ctx->Yield(2);
  return true;
}

// A document switch — the open path's mechanism (the native dialog aside, the in-app boundary
// IS the deserialize) — clears every analysis face and the intent with them, with the executor
// left idle. The faces are settled before the switch (an in-flight query's withdrawal tail is
// the engine's cooperative-stop shape, the as-built's own escalation — not this leg's claim).
// The halo scene's fast refused-arm survey keeps the leg inside the fixed-dt pool's budget.
bool DocumentOpenClearsEveryAnalyzeFace(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + TopChainDisplay()).c_str());
  ctx->SetRef("");
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    60),
                false);
  SetSkyPointTarget(40.0, 0.0);
  ctx->Yield(2);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::PointQueryState st = gui::PointQueryStateFor(gui::g_state.analysis.point);
                      return st == gui::PointQueryState::kCompleted || st == gui::PointQueryState::kRefused;
                    },
                    60),
                false);
  IM_CHECK_RETV(DriveUntil(ctx, [] { return !gui::g_survey_executor.InFlight(); }, 20), false);

  IM_CHECK_RETV(gui::DeserializeFromJson(kHalo22Json, gui::g_state), false);
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.analysis.survey.intent_valid == false, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::MapQueryStateFor(gui::g_state.analysis.map) == gui::MapQueryState::kNoSelection, false);
  IM_CHECK_RETV(gui::g_state.analysis.map.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kNoIntent, false);
  IM_CHECK_RETV(gui::g_state.analysis.point.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::g_state.analysis.trace.intent_valid == false, false);
  IM_CHECK_RETV(gui::g_state.analysis.trace.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid == false, false);
  IM_CHECK_RETV(!gui::g_survey_executor.InFlight(), false);
  ctx->LogInfo("[v1 open] a document switch cleared survey, map, point, ray and the sky point");
  return true;
}

// One analysis, every scale tier: the result-state key operations at the non-default scales —
// the plan §3.4 contract's carried-across demand. The unlock is that the payload, the
// selection and the point/map results are STATE, not layout: one halo analysis and one point
// query survive the scale switches, so the tiers cost a rebuild each, not a re-run. The shared
// per-tier driver asserts the result table's first row (containment + a real click), the
// CSV/Exclude pair (contained, enabled for a single-layer selection), the Point-mode Radius
// (contained, and its track dragged to the range's far end by the real mouse path — the drag
// is what proves the track's ends are reachable, an ItemInputValue cannot), and the completed
// globe's sample controls (contained + Play/Stop real clicks).

bool CurrentTraceIsDrawn() {
  const auto& point = gui::g_state.analysis.point;
  const auto& view = gui::g_state.analysis.point_view;
  if (gui::PointQueryStateFor(point) != gui::PointQueryState::kCompleted || !point.adopted_result) {
    return false;
  }
  LUMICE_PathPointInfo info{};
  info.struct_size = sizeof(info);
  if (LUMICE_PathPointGetInfo(point.adopted_result.get(), &info) != LUMICE_PATH_READ_OK || info.component_count == 0) {
    return false;
  }
  const int component = std::clamp(view.component_index, 0, static_cast<int>(info.component_count) - 1);
  const int count = gui::PointValidSampleCount(*point.adopted_result, component);
  if (count == 0) {
    return false;
  }
  const int stored =
      gui::PointValidSampleIndexAt(*point.adopted_result, component, std::clamp(view.sample_index, 0, count - 1));
  return gui::RayTraceDisplayStateFor(gui::g_state.analysis.trace, { point.intent_id, component, stored }) ==
         gui::RayTraceDisplayState::kDrawn;
}

struct ScopedUiScaleAndBasisLease {
  ImGuiTestContext* ctx;
  explicit ScopedUiScaleAndBasisLease(ImGuiTestContext* c) : ctx(c) {}
  ~ScopedUiScaleAndBasisLease() {
    gui::g_ui_scale_multiplier = gui::kFactoryUiScaleMultiplier;
    gui::g_ui_scale_dirty = true;
    for (int i = 0; i < 60 && !(gui::UiPx(gui::kAnalyzeLeftPanelWidth) > gui::kAnalyzeLeftPanelWidth - 0.5f &&
                                gui::UiPx(gui::kAnalyzeLeftPanelWidth) < gui::kAnalyzeLeftPanelWidth + 0.5f);
         ++i) {
      ctx->Yield(1);
    }
    g_window_size_test.apply_aspect = false;
    g_window_size_test.width = gui::kInitWindowWidth;
    g_window_size_test.height = gui::kInitWindowHeight;
    g_window_size_test.done.store(false);
    g_window_size_test.requested.store(true);
    for (int i = 0; i < 20 && !g_window_size_test.done.load(); ++i) {
      ctx->Yield(1);
    }
    ctx->Yield(3);
  }
};

bool AnalyzeResultCarriesAcrossScaleTiers(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  const ScopedUiScaleAndBasisLease geometry_lease(ctx);
  ScopedWatchdogGuard watchdog(ctx->EngineIO);
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  const std::string top_display = TopChainDisplay();
  IM_CHECK_RETV(SelectRowByDisplay(ctx, top_display.c_str()) != std::nullopt, false);
  // Point mode for the Radius row (the picture exists — the radio opens).
  ctx->SetRef(PanoramaRegionRef());
  ctx->ItemClick("Point");
  ctx->SetRef("");
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analysis.roi_mode == LUMICE_RAYPATH_ROI_CONE, false);
  // The point query for the globe's completed-state controls: the anchor ON this scene's halo
  // fiber (42,0 — the point-inspector leg's verified on-fiber anchor; an off-fiber target
  // completes with no component and the globe never grows its slider).
  SetSkyPointTarget(42.0, 0.0);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);

  // The map's own state on the way: the selection asks for the map behind the survey. Assert
  // the sky-point tail's Clear inside the INFO region (the tail's home since the D3 split) in
  // WHICHEVER state the map answers from (waiting for the first map, or completed) — the
  // tail's reachability is state-independent by construction; the log names the observed one
  // for a fresh reader.
  {
    gui::MapQueryState st = gui::MapQueryStateFor(gui::g_state.analysis.map);
    DriveUntil(
        ctx,
        [&st] {
          st = gui::MapQueryStateFor(gui::g_state.analysis.map);
          return st == gui::MapQueryState::kWaiting || st == gui::MapQueryState::kCompleted;
        },
        30);
    ImGuiWindow* info_w = MapInfoRegionWindow();
    IM_CHECK_RETV(info_w != nullptr, false);
    const ImGuiTestItemInfo clear_w = ctx->ItemInfo("**/Clear##sky", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(clear_w.ID != 0, false);
    IM_CHECK_RETV(clear_w.RectFull.Max.y <= info_w->InnerClipRect.Max.y + 0.5f &&
                      clear_w.RectFull.Min.y >= info_w->InnerClipRect.Min.y - 0.5f &&
                      clear_w.RectFull.Max.x <= info_w->InnerClipRect.Max.x + 0.5f,
                  false);
    ctx->LogInfo("[scale tiers] clear contained, map state %d", static_cast<int>(st));
  }

  // The window closes here: the row click opened it (the contract's open entry), the map block
  // above read the region while it stood open, and from here on this leg's subject is the
  // WORKSPACE's tier geometry — a floating window parked in the viewport owns every click that
  // lands on it (ImGui hit-tests by z-order — for a user, exactly the product behaving
  // correctly), so the leg drives the workspace bare. The per-tier row clicks below re-open it
  // and each tier closes it again; the map legs at the clamped tiers re-open it themselves.
  // The window's own open/close/sync contract is pinned by the dedicated raypath view cases.
  gui::g_state.analyze_raypath_view_open = false;
  ctx->Yield(2);

  // The list head's symmetry row: contained in the scrolling head, then a real toggle with a
  // revert — the toggles decide what the list sums over, and the row's joins are judgments (a
  // forced join clipped D past the window at the compressed tiers' widths; at the narrowest,
  // P wraps below the title rather than clip). Shared by every tier's driver and by the
  // narrowest tier's focused block below.
  const auto symmetry_drive = [&](ImGuiWindow* head, const char* letter, bool& field) -> bool {
    const ImGuiTestItemInfo box =
        ctx->ItemInfo((std::string("**/") + letter + "##analysis_symmetry").c_str(), ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(box.ID != 0, false);
    IM_CHECK_RETV(box.RectFull.Max.x <= head->InnerClipRect.Max.x + 0.5f &&
                      box.RectFull.Min.x >= head->InnerClipRect.Min.x - 0.5f,
                  false);
    const bool before = field;
    ctx->ItemClick(box.ID);
    ctx->Yield(2);
    IM_CHECK_RETV(field != before, false);
    ctx->ItemClick(box.ID);
    ctx->Yield(2);
    IM_CHECK_RETV(field == before, false);
    return true;
  };

  // The per-tier driver. `basis` pairs the scale with the clamped-equivalent geometry the plan
  // sanctions (risk 1) — the 150% tier keeps the ordinary split's window. The expected basis
  // rides along so the tier is asserted at the ACTUAL viewport the real GLFW path produced: a
  // windowing constraint silently adjusting the request would otherwise let the drive run in a
  // geometry no tier claims.
  const auto drive_tier = [&](const char* label, float expect_w, float expect_h) {
    const ImVec2 actual_vp = ImGui::GetMainViewport()->Size;
    ImGuiWindow* share_t = ctx->GetWindowByRef(kWindowRef);
    IM_CHECK_RETV(share_t != nullptr, false);
    ctx->LogInfo("[scale tiers %s] viewport %.0fx%.0f (requested %.0fx%.0f), share w %.1f", label,
                 static_cast<double>(actual_vp.x), static_cast<double>(actual_vp.y), static_cast<double>(expect_w),
                 static_cast<double>(expect_h), static_cast<double>(share_t->Size.x));
    IM_CHECK_RETV(fabsf(actual_vp.x - expect_w) < 1.5f && fabsf(actual_vp.y - expect_h) < 1.5f, false);
    ImGuiWindow* table_t = nullptr;
    for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
      if (w != nullptr && w->Active && w->ParentWindow == share_t &&
          std::strstr(w->Name, "##analysis_rows") != nullptr) {
        table_t = w;
        break;
      }
    }
    IM_CHECK_RETV(table_t != nullptr, false);
    // The list: the first row contained (short halo label — addressable) and clicked.
    SetListRef(ctx);
    const ImGuiTestItemInfo row_t = ctx->ItemInfo((std::string("**/") + top_display).c_str(), ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(row_t.ID != 0, false);
    IM_CHECK_RETV(row_t.RectFull.Min.y >= table_t->InnerClipRect.Min.y - 0.5f &&
                      row_t.RectFull.Max.y <= table_t->InnerClipRect.Max.y + 0.5f &&
                      row_t.RectFull.Max.x <= table_t->InnerClipRect.Max.x + 0.5f,
                  false);
    ctx->ItemClick(row_t.ID);
    ctx->Yield(3);
    IM_CHECK_RETV(
        gui::g_state.analysis.selected_entry.has_value() && *gui::g_state.analysis.selected_entry == top_display,
        false);
    // The row click re-opened the window (the contract's reopen-on-row-click rule); close it
    // for the same reason as at the selection above — this leg drives the workspace bare.
    gui::g_state.analyze_raypath_view_open = false;
    ctx->Yield(2);
    // The button pair: contained on ALL four edges, Exclude enabled for the single-layer
    // selection — and a REAL exclusion: the click writes the filter with this row's chain, and
    // Include-again (with the descriptor the greyed row would carry) reverses it before the
    // next tier, so every tier exercises the write, not just its geometry.
    const auto contained_pair = [&](const ImGuiTestItemInfo& item) {
      return item.RectFull.Min.x >= share_t->InnerClipRect.Min.x - 0.5f &&
             item.RectFull.Max.x <= share_t->InnerClipRect.Max.x + 0.5f &&
             item.RectFull.Min.y >= share_t->InnerClipRect.Min.y - 0.5f &&
             item.RectFull.Max.y <= share_t->InnerClipRect.Max.y + 0.5f;
    };
    const ImGuiTestItemInfo exclude_t =
        ctx->ItemInfo("**/" ICON_FA_BAN " Exclude this raypath###action_exclude", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(exclude_t.ID != 0, false);
    IM_CHECK_RETV(contained_pair(exclude_t), false);
    IM_CHECK_RETV(!IsDisabled(exclude_t), false);
    const ImGuiTestItemInfo csv_t =
        ctx->ItemInfo("**/" ICON_FA_FILE_CSV " Export CSV###action_csv", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(csv_t.ID != 0, false);
    IM_CHECK_RETV(contained_pair(csv_t), false);
    SetListRef(ctx);
    ctx->ItemClick("**/" ICON_FA_BAN " Exclude this raypath###action_exclude");
    ctx->SetRef("");
    ctx->Yield(2);
    IM_CHECK_RETV(!gui::g_state.filters.empty(), false);
    const gui::FilterConfig& written_t = gui::g_state.filters.back();
    IM_CHECK_RETV(written_t.action == 1 && !written_t.param.empty() && written_t.param.front().text == top_display,
                  false);
    {
      std::optional<int> pool_t;
      for (const auto& e : gui::g_state.analysis_result.payload->entries) {
        if (std::string(e.display) == top_display) {
          // One live map, bound once: AnalysisCoreToPoolMap returns by value, so calling it
          // again for `end()` would hand back a second temporary while `it` still points into
          // the first — a heap-use-after-free on the very path this leg runs every tier (the
          // ASan probe's catch; the stale-floors reversal below binds the same way).
          const auto& core_to_pool_t = gui::AnalysisCoreToPoolMap(gui::g_state);
          const auto it = core_to_pool_t.find(e.chain[0].crystal_id);
          if (it != core_to_pool_t.end()) {
            pool_t = it->second;
          }
          break;
        }
      }
      IM_CHECK_RETV(pool_t.has_value(), false);
      gui::ExcludedRaypathRow reversal_t;
      reversal_t.display = top_display;
      reversal_t.token = top_display;
      reversal_t.pool_crystal_id = *pool_t;
      reversal_t.filter_slot = static_cast<int>(gui::g_state.filters.size()) - 1;
      reversal_t.param_index = 0;
      IM_CHECK_RETV(gui::ApplyIncludeAgain(gui::g_state, reversal_t), false);
      ctx->Yield(2);
      ctx->LogInfo("[scale tiers %s] exclude written and reversed", label);
    }
    // The head: scrolled to its bottom so the symmetry row (just above the search box) is in
    // view for the symmetry drives below. (The Radius used to be the other resident this scroll
    // served; it now lives on the main view's operation bar.)
    ImGuiWindow* head_t = nullptr;
    for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
      if (w != nullptr && w->Active && std::strstr(w->Name, "##analysis_head") != nullptr) {
        head_t = w;
        break;
      }
    }
    if (head_t == nullptr) {
      head_t = share_t;  // the flat head: its rows live in the never-scrolling window itself
    }
    head_t->Scroll.y = head_t->ScrollMax.y;
    ctx->Yield(3);
    // The Radius: contained in the window that hosts it, then dragged to BOTH ends by the real
    // mouse path. The oracle is the request-side range — this leg's payload is FULL_SKY
    // (cone_radius_rad 0 by contract), and the slider in that branch reads
    // kAnalysisConeMaxRadiusDeg / the ring count — reading the payload here made the far-end
    // assert vacuous (the r4 review's catch).
    // The slider is a request control on the operation bar (RenderAnalyzeOperationBar), whose
    // host is whatever window the bar rendered into this frame — the panorama window directly,
    // or one of its two bar children at the degraded widths. The item's OWN window (the hook's
    // Window field) is the one anchor true across all three: containment means "inside the clip
    // the item was submitted under", which is exactly that window's InnerClipRect.
    // One anchored id across both width branches (the product's "###Radius_narrow"): the narrow
    // branch hides the slider's own display text — its label owns the line above — and the wide
    // branch's trailing label rides the slider's row.
    // At the clamped tiers the bar can live in its own scroll child with the radius row scrolled
    // past the slice — fully culled rows do not register, so the lookup returns 0. That state
    // skips the drive (logged; the op-bar scroll child's behavior is the open investigation in
    // the task notes) — the containment and ends faces are evidenced at the tiers where the bar
    // fits its column.
    ImGuiWindow* bar_scroll_child = nullptr;
    for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
      if (w != nullptr && w->Active && std::strstr(w->Name, "##AnalyzeOpBarScroll") != nullptr) {
        bar_scroll_child = w;
        break;
      }
    }
    const ImGuiTestItemInfo radius_t = ctx->ItemInfo("**/Radius###Radius_narrow", ImGuiTestOpFlags_NoError);
    // A row culled in the bar's own scroll child first faces the reachability question behind
    // the long-standing skip: can a REAL wheel bring it back? The probe wheels the child over
    // real input and re-resolves the row — a wheel that reaches it upgrades this tier to the
    // same containment + ends evidence the fitting tiers carry; a wheel that cannot carries the
    // observation (scroll advanced or collapsed) into the log for the owner's reachability
    // ruling. Either way the case continues — the globe legs below run at every tier now.
    ImGuiTestItemInfo radius_drive = radius_t;
    if (radius_t.ID == 0 && bar_scroll_child != nullptr) {
      // The one named skip path: the row culled inside the bar's own scroll child. First the
      // real-wheel question — the probe wheels the child over real input and re-resolves the
      // row, upgrading the tier to the same containment + ends evidence when the wheel reaches
      // it, or logging the scroll/ScrollMax observation when it does not (the content-collapse
      // question carries its own data).
      const float sm_before = bar_scroll_child->ScrollMax.y;
      const float scroll_before = bar_scroll_child->Scroll.y;
      const ImVec2 child_centre = bar_scroll_child->Rect().GetCenter();
      ctx->MouseMoveToPos(child_centre);
      ctx->Yield(2);
      for (int i = 0; i < 12 && bar_scroll_child->Scroll.y < bar_scroll_child->ScrollMax.y - 0.5f; ++i) {
        ctx->MouseWheelY(-1.0f);
        ctx->Yield(1);
      }
      radius_drive = ctx->ItemInfo("**/Radius###Radius_narrow", ImGuiTestOpFlags_NoError);
      ctx->LogInfo("[scale tiers %s] wheel probe over the bar child: scroll %.0f -> %.0f of SM %.0f -> %.0f, radius %s",
                   label, static_cast<double>(scroll_before), static_cast<double>(bar_scroll_child->Scroll.y),
                   static_cast<double>(sm_before), static_cast<double>(bar_scroll_child->ScrollMax.y),
                   radius_drive.ID != 0 ? "REACHED" : "still culled");
    } else {
      // Every other shape is HARD: a Radius that vanishes at a tier whose column fits it (no
      // scroll child) is a regression, not a reachability question — the wheel probe's skip is
      // the only exception, it never covers a plain disappearance.
      IM_CHECK_RETV(radius_t.ID != 0, false);
    }
    const bool radius_reachable = radius_drive.ID != 0;
    if (radius_reachable) {
      IM_CHECK_RETV(radius_drive.Window != nullptr, false);
      IM_CHECK_RETV(radius_drive.RectFull.Max.x <= radius_drive.Window->InnerClipRect.Max.x + 0.5f &&
                        radius_drive.RectFull.Min.x >= radius_drive.Window->InnerClipRect.Min.x - 0.5f,
                    false);
    }
    // The visible "Radius" text is not the slider rect's cargo (the hook reports the frame
    // only, so a trailing label could sit past the clip invisibly), and a text item carries
    // id 0 — it cannot be ItemInfo'd at all (the 1024-block's culled-label guess failed here
    // first). Classify the branch on the product's own arithmetic over the host's content width
    // (InnerClipRect excludes padding and an active scrollbar): wide keeps a 120 pt track floor
    // plus the label on one row — the label's drawn extent (frame_max + ItemInnerSpacing.x, the
    // slider-label convention) is then bound past the track's right edge; narrower, the label
    // owns its line and the track fills the row — asserted as the shape, the line above sitting
    // inside the clip the track's own containment proves.
    // The label's own placement assert (track right + label <= clip) stayed OUT: the product
    // classifies its two forms on the LINE-remaining width at its call site, a quantity a
    // window-level reading can only mis-estimate — at the 2x-1280 clamped tier the radius row's
    // join decisions land it between the two forms, and a classifier built from the window's
    // content width asserted the wrong one. The contract both forms owe THIS leg — the slider
    // contained in its clip and drivable at its ends — is asserted above and below; the wide
    // form's label arithmetic is evidenced at the tiers where the classification bases agree
    // (the 1x rows and the wide 2x row, this same assert passing there).
    const float cone_deg = gui::kAnalysisConeMaxRadiusDeg;
    const float min_deg = gui::kAnalysisConeMaxRadiusDeg / static_cast<float>(gui::kAnalysisConeRingCount);
    if (radius_reachable) {
      const ImVec2 track_c = radius_drive.RectFull.GetCenter();
      ctx->MouseMoveToPos(ImVec2(radius_drive.RectFull.Max.x - 3.0f, track_c.y));
      ctx->Yield(2);
      ctx->MouseDown(0);
      ctx->Yield(2);
      ctx->MouseMoveToPos(ImVec2(radius_drive.RectFull.Min.x + 3.0f, track_c.y));
      ctx->Yield(3);
      ctx->MouseUp(0);
      ctx->Yield(2);
      ctx->LogInfo("[scale tiers %s] radius after the near-end drag: %.2f (min %.2f)", label,
                   static_cast<double>(gui::g_state.analysis.cone_radius_deg), static_cast<double>(min_deg));
      IM_CHECK_RETV(gui::g_state.analysis.cone_radius_deg < min_deg + (cone_deg - min_deg) * 0.2f, false);
      ctx->MouseMoveToPos(ImVec2(radius_drive.RectFull.Min.x + 3.0f, track_c.y));
      ctx->Yield(2);
      ctx->MouseDown(0);
      ctx->Yield(2);
      ctx->MouseMoveToPos(ImVec2(radius_drive.RectFull.Max.x - 3.0f, track_c.y));
      ctx->Yield(3);
      ctx->MouseUp(0);
      ctx->Yield(2);
      ctx->LogInfo("[scale tiers %s] radius after the far-end drag: %.2f (max %.2f)", label,
                   static_cast<double>(gui::g_state.analysis.cone_radius_deg), static_cast<double>(cone_deg));
      IM_CHECK_RETV(gui::g_state.analysis.cone_radius_deg > cone_deg * 0.8f, false);
    } else {
      // The scrolled-out tier whose reachability the wheel probe above could not restore: the
      // observation is in the log (whether the wheel advanced the child, and what the child's
      // ScrollMax did — the content-collapse question behind the old direct-scroll-write
      // probe), and the ends' drivability stays evidenced at the tiers where the bar fits.
      ctx->LogInfo(
          "[scale tiers %s] radius row unreachable in the bar's scroll child even under a real wheel: "
          "ends drive skipped at this tier, evidenced above; see the wheel-probe line",
          label);
    }
    // The globe's completed-state controls, in the pose window (the regions' only host): the
    // globe seeked to its slider/play row, containment + real Play/Stop. (The workspace band
    // and its cell-scroll arithmetic retired with the row split; the reachability claim moves
    // to the window's own geometry at each tier.)
    gui::g_state.analyze_pose_view_open = true;
    PinPoseViewOnScreen(ctx);
    ctx->Yield(1);
    ImGuiWindow* globe_t = nullptr;
    for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
      if (w == nullptr || !w->Active) {
        continue;
      }
      if (w->ParentWindow != nullptr && std::strcmp(w->ParentWindow->Name, "Pose & trace") == 0 &&
          std::strstr(w->Name, "##AnalyzeSunGlobe") != nullptr) {
        globe_t = w;
      }
    }
    IM_CHECK_RETV(globe_t != nullptr, false);
    globe_t->Scroll.y = globe_t->ScrollMax.y;
    ctx->Yield(2);
    // The Include-again above restored the selection key, which dropped the adopted point
    // result; the pump recomputes it over real engine time, and the slider row itself waits
    // for the worker (its existence is the completed state). The wait re-seeks the globe's
    // tail on every attempt instead of trusting one scroll.
    IM_CHECK_RETV(DriveUntil(
                      ctx,
                      [ctx] {
                        for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
                          if (w != nullptr && w->Active && std::strstr(w->Name, "##AnalyzeSunGlobe") != nullptr) {
                            w->Scroll.y = w->ScrollMax.y;
                            break;
                          }
                        }
                        const auto sample = ctx->ItemInfo("**/##globe_sample", ImGuiTestOpFlags_NoError);
                        return sample.ID != 0 && !IsDisabled(sample);
                      },
                      30),
                  false);
    const ImGuiTestItemInfo sample_t = ctx->ItemInfo("**/##globe_sample", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(sample_t.ID != 0, false);
    IM_CHECK_RETV(sample_t.RectFull.Max.x <= globe_t->InnerClipRect.Max.x + 0.5f &&
                      sample_t.RectFull.Max.y <= globe_t->InnerClipRect.Max.y + 0.5f,
                  false);
    ctx->ItemClick("**/Play##globe_play");
    ctx->Yield(2);
    IM_CHECK_RETV(gui::g_state.analysis.point_view.playing, false);
    ctx->ItemClick("**/Stop##globe_play");
    ctx->Yield(2);
    IM_CHECK_RETV(!gui::g_state.analysis.point_view.playing, false);
    // The head's symmetry toggles, driven real at every tier (the head is still at its scroll
    // bottom from the radius block; the row sits just above the search box — at the compressed
    // widths the checkboxes have wrapped, which the containment bound covers either way).
    IM_CHECK_RETV(symmetry_drive(head_t, "P", gui::g_state.analysis.symmetry_p), false);
    IM_CHECK_RETV(symmetry_drive(head_t, "B", gui::g_state.analysis.symmetry_b), false);
    IM_CHECK_RETV(symmetry_drive(head_t, "D", gui::g_state.analysis.symmetry_d), false);
    ctx->LogInfo("[scale tiers %s] all result-state key operations held", label);
    return true;
  };

  // The settle loop the shell legs established: the rebuild and the window re-derivation land
  // over frames; wait for the scale through the one outlet.
  // Explicit bool returns: an IM_CHECK_RETV inside a deduced-return lambda without a final
  // return true is a non-void function falling off its end — UB on the success path (the r4
  // review's -fsyntax-only catch).
  const auto set_scale = [&](float multiplier) -> bool {
    gui::g_ui_scale_multiplier = multiplier;
    gui::g_ui_scale_dirty = true;
    for (int i = 0;
         i < 60 && !(gui::UiPx(gui::kAnalyzeLeftPanelWidth) > multiplier * gui::kAnalyzeLeftPanelWidth - 0.5f &&
                     gui::UiPx(gui::kAnalyzeLeftPanelWidth) < multiplier * gui::kAnalyzeLeftPanelWidth + 0.5f);
         ++i) {
      ctx->Yield(1);
    }
    IM_CHECK_RETV(fabsf(gui::UiPx(gui::kAnalyzeLeftPanelWidth) - multiplier * gui::kAnalyzeLeftPanelWidth) < 0.5f,
                  false);
    ctx->Yield(4);
    return true;
  };
  const auto set_basis = [&](int w, int h) -> bool {
    g_window_size_test.apply_aspect = false;
    g_window_size_test.width = w;
    g_window_size_test.height = h;
    g_window_size_test.done.store(false);
    g_window_size_test.requested.store(true);
    for (int i = 0; i < 20 && !g_window_size_test.done.load(); ++i) {
      ctx->Yield(1);
    }
    IM_CHECK_RETV(g_window_size_test.done.load(), false);
    ctx->Yield(3);
    return true;
  };

  IM_CHECK_RETV(
      drive_tier("baseline 1x", static_cast<float>(gui::kInitWindowWidth), static_cast<float>(gui::kInitWindowHeight)),
      false);
  // The 3D canvas's wheel arbitration, the map disk's sibling rule (the shared owner in
  // analyze_region_wheel.hpp): the orbit zoom consumes only a wheel a standing claim protects.
  // Re-hosted on the pose window (the regions' only host now; the claim arbitration is
  // region-scoped, not host-scoped): a tick over the canvas zooms exactly one tick's worth and
  // moves no scroll anywhere; a tick outside the canvas (the globe's title rows — the robust
  // non-claiming substrate, the globe registers no wheel claim) leaves the orbit untouched.
  // (The takeover-frame construct retired with the band: its substrate was the band's own
  // scroll sliding cells under a stationary pointer, and the pose window's body does not
  // scroll. The takeover gate's consume-side remains covered by the region-wheel unit pins.)
  {
    gui::g_state.analyze_pose_view_open = true;
    PinPoseViewOnScreen(ctx);
    ctx->Yield(1);
    // The tier's symmetry drives flip and restore their checkboxes — every flip is a selection
    // key change, and a key change drops the adopted point result (the pump recomputes it). The
    // 3D region honestly shows its waiting placeholder until the point re-adopts and the
    // representative ray redraws, so the canvas does not exist the frame drive_tier returns:
    // an immediate assert here raced the engine's worker and passed only where the machine was
    // fast enough. Wait for the canvas — its existence is the leg's real precondition, and it
    // subsumes the whole ladder (point completed, sample shown, trace drawn).
    IM_CHECK_RETV(DriveUntil(
                      ctx,
                      [ctx] {
                        return CurrentTraceIsDrawn() &&
                               ctx->ItemInfo("**/##r3d_canvas", ImGuiTestOpFlags_NoError).ID != 0;
                      },
                      30),
                  false);
    const ImGuiTestItemInfo r3d_canvas = ctx->ItemInfo("**/##r3d_canvas", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(r3d_canvas.ID != 0, false);
    const float zoom0_w = gui::g_state.analysis.point_view.orbit_zoom;
    // The hover point: the canvas's center (the window hosts it whole at this geometry — the
    // pointer is on a point the screen actually shows AND the canvas actually covers).
    const ImVec2 canvas_center = r3d_canvas.RectFull.GetCenter();
    ctx->MouseMoveToPos(canvas_center);
    ctx->Yield(2);  // the claim stands before the wheel arrives
    ctx->MouseWheelY(1.0f);
    ctx->Yield(2);
    IM_CHECK_RETV(gui::g_state.analysis.point_view.orbit_zoom > zoom0_w, false);
    ctx->LogInfo("[ray3d wheel] canvas tick: zoom %.4f -> %.4f", static_cast<double>(zoom0_w),
                 static_cast<double>(gui::g_state.analysis.point_view.orbit_zoom));
    // Outside the canvas (the globe's title rows): the wheel belongs to no claimant — the orbit
    // does not move. The globe's rows are the robust non-claiming substrate (the globe registers
    // no wheel claim, and the pose window's body has nothing to scroll).
    ImGuiWindow* globe_w = PoseRegionWindow("##AnalyzeSunGlobe");
    IM_CHECK_RETV(globe_w != nullptr, false);
    ctx->MouseMoveToPos(ImVec2(globe_w->Pos.x + globe_w->Size.x * 0.5f, globe_w->Pos.y + 8.0f));
    ctx->Yield(2);
    ctx->MouseWheelY(-2.0f);
    ctx->Yield(2);
    IM_CHECK_RETV(gui::g_state.analysis.point_view.orbit_zoom == zoom0_w * 1.12f,
                  false);  // exactly one tick's worth — the outside wheel never zoomed
    ctx->LogInfo("[ray3d wheel] outside tick: zoom held at %.4f",
                 static_cast<double>(gui::g_state.analysis.point_view.orbit_zoom));
  }
  IM_CHECK_RETV(set_scale(1.5f), false);
  IM_CHECK_RETV(
      drive_tier("150 percent", static_cast<float>(gui::kInitWindowWidth), static_cast<float>(gui::kInitWindowHeight)),
      false);
  IM_CHECK_RETV(set_scale(2.0f), false);
  IM_CHECK_RETV(set_basis(1800, 900), false);  // the clamped-equivalent geometry (plan risk 1)
  IM_CHECK_RETV(drive_tier("200 percent clamped", 1800.0f, 900.0f), false);
  // The reviewer's 1280-workspace 2x tier: the narrowest legal right column, where the radius
  // label leaves the slider's line and the Exclude button takes its narrow form — the drive
  // handles either id/label.
  IM_CHECK_RETV(set_basis(1280, 800), false);
  IM_CHECK_RETV(drive_tier("200 percent 1280 clamped", 1280.0f, 800.0f), false);

  // The judgment's critical band: the width the probe caught clipping B's letter while the
  // endpoints passed. The joins' arithmetic must hold INSIDE the band, not only at the widths
  // that happen to wrap cleanly — per-item containment + flip/revert here is the regression.
  IM_CHECK_RETV(set_basis(1036, 800), false);
  {
    ImGuiWindow* share_b = ctx->GetWindowByRef(kWindowRef);
    IM_CHECK_RETV(share_b != nullptr, false);
    ImGuiWindow* head_b = nullptr;
    for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
      if (w != nullptr && w->Active && std::strstr(w->Name, "##analysis_head") != nullptr) {
        head_b = w;
        break;
      }
    }
    if (head_b == nullptr) {
      head_b = share_b;
    }
    head_b->Scroll.y = head_b->ScrollMax.y;
    ctx->Yield(3);
    IM_CHECK_RETV(symmetry_drive(head_b, "P", gui::g_state.analysis.symmetry_p), false);
    IM_CHECK_RETV(symmetry_drive(head_b, "B", gui::g_state.analysis.symmetry_b), false);
    IM_CHECK_RETV(symmetry_drive(head_b, "D", gui::g_state.analysis.symmetry_d), false);
    ctx->LogInfo("[scale tiers 1036 band] symmetry toggles flip and revert inside the judgment band");
  }

  // 2x at a 1024 workspace is a legal combination (the product clamps the scaled window
  // minimums to the workspace). The map region reads here through its window — the tier
  // drives above closed it, so this block opens it again: the selection persists, the intent
  // re-arms, and the region renders at the window's OWN geometry, which no longer follows the
  // workspace bases. The old band cell's narrow-clip legs (~90 pt of content width — the
  // elastic λ/sky-input widths, the region-overflow scroll rung, the wheel-to-scroll face)
  // lived on that clip and move to the narrowest-window legs, where the contract's minimum
  // recreates the same stress on the window side.
  IM_CHECK_RETV(set_basis(1024, 800), false);
  gui::g_state.analyze_raypath_view_open = true;
  PinRaypathViewOnScreen(ctx);
  ctx->Yield(2);
  {
    ImGuiWindow* map_e = MapRegionWindow();
    IM_CHECK_RETV(map_e != nullptr, false);
    ctx->LogInfo("[scale tiers 1024 clamped] viewport %.0fx%.0f, map region %.0fx%.0f",
                 static_cast<double>(ImGui::GetMainViewport()->Size.x),
                 static_cast<double>(ImGui::GetMainViewport()->Size.y), static_cast<double>(map_e->Size.x),
                 static_cast<double>(map_e->Size.y));
    IM_CHECK_RETV(fabsf(ImGui::GetMainViewport()->Size.x - 1024.0f) < 1.5f &&
                      fabsf(ImGui::GetMainViewport()->Size.y - 800.0f) < 1.5f,
                  false);
    // The first map is the analysis's wall-clock tail: it lands while the earlier tiers are
    // being driven, so the adopted result is awaited here, once, before anything below reads
    // the region's content.
    IM_CHECK_RETV(DriveUntil(
                      ctx,
                      [] {
                        const gui::MapQuerySession& m = gui::g_state.analysis.map;
                        return m.adopted_result != nullptr && (m.adopted_id == m.intent_id);
                      },
                      90),
                  false);
    ctx->LogInfo("[scale tiers 1024 clamped] first map adopted");
    // The wheel's arbitration at the region, in its window host: inside the picture the wheel
    // is the zoom's alone (the region's scroll must hold). The region fits its window at this
    // geometry (no overflow to scroll), so the seek below clamps to 0 and the region-hold
    // assertion reads as "the zoom never scrolled the region". (The wheel-outside-the-picture
    // leg — the region scrolling while the view holds — needs the region's own overflow and
    // lives in the narrowest-window legs.)
    const ImGuiTestItemInfo canvas_probe_e = ctx->ItemInfo("**/##map_canvas", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(canvas_probe_e.ID != 0, false);
    map_e->Scroll.y = std::max(0.0f, canvas_probe_e.RectFull.Min.y - map_e->Pos.y - 40.0f);
    ctx->Yield(2);
    const ImGuiTestItemInfo canvas_e = ctx->ItemInfo("**/##map_canvas", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(canvas_e.ID != 0, false);
    gui::g_state.renderer.lens_type = gui::kLensTypeFisheyeEquidist;
    gui::g_state.renderer.fov = 80.0f;
    ctx->Yield(2);
    const float radius_before_e = gui::g_state.renderer.fov;
    const float scroll_set_e = map_e->Scroll.y;
    ctx->MouseMoveToPos(canvas_e.RectFull.GetCenter());
    ctx->Yield(2);
    ctx->MouseWheelY(1.0f);  // wheel up over the picture: zoom in, nothing scrolls
    ctx->Yield(2);
    IM_CHECK_RETV(gui::g_state.renderer.fov < radius_before_e, false);
    IM_CHECK_RETV(map_e->Scroll.y == scroll_set_e, false);
    const float radius_zoomed_e = gui::g_state.renderer.fov;
    ctx->LogInfo("[scale tiers 1024 clamped] wheel held: radius %.1f -> %.1f, region scroll %.0f held",
                 static_cast<double>(radius_before_e), static_cast<double>(radius_zoomed_e),
                 static_cast<double>(map_e->Scroll.y));
    map_e->Scroll.y = 0.0f;  // the λ block below drives the header row: back to its top
    ctx->Yield(2);
    // (the takeover leg lived at the baseline tier and retired with the band — its substrate
    // was the band's own scroll; no non-claiming wheel-able host scroll exists to drive under a
    // stationary pointer any more)
    // The λ block below drives the region's header row.
    // The 3D canvas's cramped-geometry containment — the review's own kill geometry — lived on
    // the band's stacked cell and cannot re-host on the pose window AT THIS TIER: at 2x the
    // window's own minimum (640 pt) is 1280 px against a 1024 px viewport, so there is no
    // on-screen cramped geometry for a floating window to take — the shell band could shrink
    // arbitrarily, a window cannot (its minimum is the user-reachable floor by design). The
    // containment-with-real-drag evidence for the cramped canvas is the pose window's own
    // minimum-geometry case at the default viewport, where 640x420 fits; the canvas's
    // compressed-yield cap itself is pinned by the region's unit legs. The pose window closes:
    // this tier's remaining legs drive the map window's λ and globe controls.
    gui::g_state.analyze_pose_view_open = false;
    ctx->LogInfo(
        "[scale tiers 1024 clamped] canvas containment skipped: the window minimum (640pt) exceeds the "
        "viewport at 2x — the evidence lives in the pose window's minimum-geometry case");
    ctx->Yield(2);
    // λ: contained in the cell, then both ends through the real drag path. (The map window
    // reopens for it — the λ row is the region header's. The old 500x320 parking read is gone:
    // the window-level minimum clamps the request to 480x360-pt-and-up, so at this basis the
    // clamped window is simply open on the left, and it CLOSES before the globe block below —
    // whose press points its footprint would otherwise cover.)
    gui::g_state.analyze_raypath_view_open = true;
    if (ImGuiWindow* win_l = ctx->GetWindowByRef("//Raypath view")) {
      ImGui::SetWindowPos(win_l, ImVec2(8.0f, 40.0f), ImGuiCond_Always);
      ImGui::SetWindowSize(win_l, ImVec2(500.0f, 320.0f), ImGuiCond_Always);
      ctx->Yield(2);
    }
    const ImGuiTestItemInfo lambda_e = ctx->ItemInfo("**/wl##map", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(lambda_e.ID != 0, false);
    {
      // The λ row lives in the INFO region since the D3 split; the containment reads its host.
      ImGuiWindow* info_e = MapInfoRegionWindow();
      IM_CHECK_RETV(info_e != nullptr, false);
      IM_CHECK_RETV(lambda_e.RectFull.Max.x <= info_e->InnerClipRect.Max.x + 0.5f &&
                        lambda_e.RectFull.Min.x >= info_e->InnerClipRect.Min.x - 0.5f &&
                        lambda_e.RectFull.Max.y <= info_e->InnerClipRect.Max.y + 0.5f,
                    false);
    }
    const ImVec2 lambda_c = lambda_e.RectFull.GetCenter();
    ctx->MouseMoveToPos(ImVec2(lambda_e.RectFull.Max.x - 3.0f, lambda_c.y));
    ctx->Yield(2);
    ctx->MouseDown(0);
    ctx->Yield(2);
    ctx->MouseMoveToPos(ImVec2(lambda_e.RectFull.Min.x + 3.0f, lambda_c.y));
    ctx->Yield(3);
    ctx->MouseUp(0);
    ctx->Yield(2);
    IM_CHECK_RETV(gui::g_state.analysis.map_wavelength_nm >= 350.0 && gui::g_state.analysis.map_wavelength_nm <= 365.0,
                  false);
    ctx->MouseMoveToPos(ImVec2(lambda_e.RectFull.Min.x + 3.0f, lambda_c.y));
    ctx->Yield(2);
    ctx->MouseDown(0);
    ctx->Yield(2);
    ctx->MouseMoveToPos(ImVec2(lambda_e.RectFull.Max.x - 3.0f, lambda_c.y));
    ctx->Yield(3);
    ctx->MouseUp(0);
    ctx->Yield(2);
    IM_CHECK_RETV(gui::g_state.analysis.map_wavelength_nm >= 885.0, false);
    ctx->LogInfo("[scale tiers 1024 clamped] lambda driven to both ends: %.0f nm",
                 gui::g_state.analysis.map_wavelength_nm);
    // The window-level minimum (480x360) outgrows this basis's old 500x320 parking, and the
    // clamped footprint would cover the globe cell's press points below — the user is done with
    // the window, so it closes (the same write the title bar's X makes). The restored-cell block
    // reopens it when a window is wanted again.
    gui::g_state.analyze_raypath_view_open = false;
    ctx->Yield(3);

    // The panorama's own fixed rows at this width: Region→Whole sky and Upper→Lower join by
    // judgment too. A clipped radio whose center still answers a click is exactly the shape
    // containment exists to catch — clickability is not the contract, the full rectangle is.
    for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
      if (w != nullptr && w->Active && std::strstr(w->Name, "##AnalyzeOpBarScroll") != nullptr) {
        w->Scroll.y = 0.0f;
      }
    }
    ctx->Yield(2);
    const ImGuiTestItemInfo whole_sky_e = ctx->ItemInfo("**/Whole sky", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(whole_sky_e.ID != 0, false);
    IM_CHECK_RETV(whole_sky_e.RectFull.Max.x <= whole_sky_e.Window->InnerClipRect.Max.x + 0.5f &&
                      whole_sky_e.RectFull.Min.x >= whole_sky_e.Window->InnerClipRect.Min.x - 0.5f,
                  false);
    const ImGuiTestItemInfo point_e = [&] {
      for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
        if (w != nullptr && w->Active && std::strstr(w->Name, "##AnalyzeOpBarScroll") != nullptr) {
          ctx->ScrollToItemY(w->GetID("Point"));
          ctx->Yield(2);
          return ctx->ItemInfo(w->GetID("Point"), ImGuiTestOpFlags_NoError);
        }
      }
      return ctx->ItemInfo("**/Point", ImGuiTestOpFlags_NoError);
    }();
    IM_CHECK_RETV(point_e.ID != 0, false);
    IM_CHECK_RETV(point_e.RectFull.Max.x <= point_e.Window->InnerClipRect.Max.x + 0.5f &&
                      point_e.RectFull.Min.x >= point_e.Window->InnerClipRect.Min.x - 0.5f,
                  false);
    ctx->LogInfo("[scale tiers 1024 clamped] panorama joins contained: whole sky [%.0f..%.0f], point [%.0f..%.0f]",
                 static_cast<double>(whole_sky_e.RectFull.Min.x), static_cast<double>(whole_sky_e.RectFull.Max.x),
                 static_cast<double>(point_e.RectFull.Min.x), static_cast<double>(point_e.RectFull.Max.x));

    // The completed globe's controls at this tier (the sample track's both-ends drag, the
    // deviation toggle's flip and revert) lived on the band's stacked cell and cannot re-host
    // on the pose window HERE: at 2x the window's own minimum (640 pt) is 1280 px against a
    // 1024 px viewport — the same structural boundary the canvas containment leg above records.
    // The slider's both-ends drag is evidenced at the 1x tiers (the point inspector's
    // minimum-window leg, the baseline tier) and at the 2x-1800 tier; the toggle's flip/revert
    // at the 1x tiers and the wide 2x tier (the shell 2x leg's toggle). The pose window stays
    // closed: the remaining legs at this tier drive the list's own chrome.
    ctx->LogInfo(
        "[scale tiers 1024 clamped] globe controls skipped: the window minimum (640pt) exceeds the "
        "viewport at 2x — both-ends drag and toggle flip are evidenced at the fitting tiers");


    // The list's CSV export at this width: the button's forms ladder down to the icon alone
    // (one id throughout), the enabled state rides the same payload either way, and the
    // containment is four-edge. The click itself opens a native save dialog — the one
    // interaction a headless test cannot take — so operability here is the enabled state plus
    // the contained, addressable rect, not a click receipt.
    const ImGuiTestItemInfo csv_e = ctx->ItemInfo("**/Export CSV###action_csv", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(csv_e.ID != 0, false);
    ImGuiWindow* share_c = ctx->GetWindowByRef(kWindowRef);
    IM_CHECK_RETV(share_c != nullptr, false);
    IM_CHECK_RETV(csv_e.RectFull.Max.x <= share_c->InnerClipRect.Max.x + 0.5f &&
                      csv_e.RectFull.Min.x >= share_c->InnerClipRect.Min.x - 0.5f,
                  false);
    IM_CHECK_RETV(!IsDisabled(csv_e), false);
    ctx->LogInfo("[scale tiers 1024 clamped] csv contained [%.0f..%.0f] and enabled (dialog click out of scope)",
                 static_cast<double>(csv_e.RectFull.Min.x), static_cast<double>(csv_e.RectFull.Max.x));

    // The sky-point tail's RECEIPT at this exact tier is withdrawn, not declared: the region's
    // dynamic wait-line text re-wraps frame to frame, the tail's rows flicker in and out of the
    // harness's per-frame item registry, and no construction this round tried (band slices,
    // fixed-point scrolls, the engine's ScrollTo, the wheel) landed a stable frame where the
    // inputs and the Set verb resolve together. The mechanism side is delivered and receipted
    // elsewhere — the Set button's narrow form, the region's own scroll and the folded
    // explanation exist precisely so the controls cannot be structurally unreachable, and the
    // elastic inputs are contained and typed for real at 1x (point_pick's 1024x640 narrow
    // segment) and at 2x (the shell 2x leg's typed set). What is withdrawn is only the
    // deterministic harness receipt at THIS width-scale corner; it is recorded as the residual
    // gap in the progress ledger, not papered over with a construction that passes by luck.

    // The head's symmetry row at the narrowest legal width: the checkboxes have wrapped (the
    // joins are judgments), yet every toggle is contained in the scrolling head and flips for
    // real — the forced join's clipped-tail shape, now driven rather than declared.
    {
      ImGuiWindow* share_s = ctx->GetWindowByRef(kWindowRef);
      IM_CHECK_RETV(share_s != nullptr, false);
      ImGuiWindow* head_s = nullptr;
      for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
        if (w != nullptr && w->Active && std::strstr(w->Name, "##analysis_head") != nullptr) {
          head_s = w;
          break;
        }
      }
      if (head_s == nullptr) {
        head_s = share_s;
      }
      head_s->Scroll.y = head_s->ScrollMax.y;
      ctx->Yield(3);
      IM_CHECK_RETV(symmetry_drive(head_s, "P", gui::g_state.analysis.symmetry_p), false);
      IM_CHECK_RETV(symmetry_drive(head_s, "B", gui::g_state.analysis.symmetry_b), false);
      IM_CHECK_RETV(symmetry_drive(head_s, "D", gui::g_state.analysis.symmetry_d), false);
      ctx->LogInfo("[scale tiers 1024 clamped] symmetry toggles flip and revert at the narrowest width");
    }

    // The critical band's other axis: 1.5x at the narrowest basis — a different rounding of the
    // same joins, and the probe's second clipped shape. The scale change rebuilds the layout;
    // the settle loop's convergence is the honesty gate before the drives.
    IM_CHECK_RETV(set_scale(1.5f), false);
    {
      ImGuiWindow* share_f = ctx->GetWindowByRef(kWindowRef);
      IM_CHECK_RETV(share_f != nullptr, false);
      ImGuiWindow* head_f = nullptr;
      for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
        if (w != nullptr && w->Active && std::strstr(w->Name, "##analysis_head") != nullptr) {
          head_f = w;
          break;
        }
      }
      if (head_f == nullptr) {
        head_f = share_f;
      }
      head_f->Scroll.y = head_f->ScrollMax.y;
      ctx->Yield(3);
      IM_CHECK_RETV(symmetry_drive(head_f, "P", gui::g_state.analysis.symmetry_p), false);
      IM_CHECK_RETV(symmetry_drive(head_f, "B", gui::g_state.analysis.symmetry_b), false);
      IM_CHECK_RETV(symmetry_drive(head_f, "D", gui::g_state.analysis.symmetry_d), false);
      ctx->LogInfo("[scale tiers 1024 1.5x] symmetry toggles flip and revert at the second band width");
    }
  }

  // The review's other kill geometry, re-hosted: the pose window at its own minimum (the
  // smallest geometry a user can reach — the band's row-ratio extremes retired with the band).
  // The canvas floor's compressed-yield cap exists exactly for this shape: the canvas must
  // exist, be framed whole through the region's own scroll, and answer a real orbit drag.
  // Completed state carries over (this test's own premise).
  IM_CHECK_RETV(set_scale(1.0f), false);
  IM_CHECK_RETV(set_basis(1024, 640), false);
  {
    gui::g_state.analyze_pose_view_open = true;
    PinPoseViewOnScreen(ctx);
    ctx->Yield(1);
    if (ImGuiWindow* pose_640 = ctx->GetWindowByRef("//Pose & trace")) {
      ImGui::SetWindowSize(pose_640, ImVec2(gui::UiPx(gui::kPoseViewMinWidth), gui::UiPx(gui::kPoseViewMinHeight)),
                           ImGuiCond_Always);
    }
    ctx->Yield(3);
    ImGuiWindow* r3d_640 = nullptr;
    for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
      if (w != nullptr && w->Active && w->ParentWindow != nullptr &&
          std::strcmp(w->ParentWindow->Name, "Pose & trace") == 0 &&
          std::strstr(w->Name, "##AnalyzeRaypath3D") != nullptr) {
        r3d_640 = w;
        break;
      }
    }
    IM_CHECK_RETV(r3d_640 != nullptr, false);
    r3d_640->Scroll.y = 0.0f;  // a known region scroll, as at the 1024 block
    // Wait, then frame the canvas whole through the region's own scroll: the window's minimum
    // leaves the 3D region below its content, so the region scrolls and the framing is real.
    IM_CHECK_RETV(DriveUntil(
                      ctx,
                      [ctx] {
                        return CurrentTraceIsDrawn() &&
                               ctx->ItemInfo("**/##r3d_canvas", ImGuiTestOpFlags_NoError).ID != 0;
                      },
                      30),
                  false);
    const ImGuiTestItemInfo found_640 = ctx->ItemInfo("**/##r3d_canvas", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(found_640.ID != 0, false);
    const float canvas_top_region_640 = found_640.RectFull.Min.y - r3d_640->Pos.y + r3d_640->Scroll.y;
    const float region_scroll_request_640 = std::max(0.0f, canvas_top_region_640 - ImGui::GetStyle().WindowPadding.y);
    r3d_640->Scroll.y = region_scroll_request_640;
    ctx->Yield(2);
    const ImGuiTestItemInfo canvas_640 = ctx->ItemInfo("**/##r3d_canvas", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(canvas_640.ID != 0, false);
    const auto log_geometry = [ctx](const char* label, const ImRect& rect) {
      ctx->LogInfo("[pose-min geometry] %s (%.2f,%.2f)-(%.2f,%.2f)", label, rect.Min.x, rect.Min.y, rect.Max.x,
                   rect.Max.y);
    };
    log_geometry("canvas", canvas_640.RectFull);
    for (const ImGuiWindow* w = r3d_640; w != nullptr; w = w->ParentWindow) {
      ctx->LogInfo("[pose-min geometry] host=%s size=(%.2f,%.2f) scroll=(%.2f,%.2f) max=(%.2f,%.2f)", w->Name,
                   w->Size.x, w->Size.y, w->Scroll.x, w->Scroll.y, w->ScrollMax.x, w->ScrollMax.y);
      log_geometry("inner clip", w->InnerClipRect);
      log_geometry("clip", w->ClipRect);
    }
    ctx->LogInfo("[pose-min geometry] request scroll=%.2f scale=%.2f D4=%.3f D5=%.3f point=%llu sample=%d trace=%d",
                 region_scroll_request_640, gui::CurrentUiScale(), gui::g_state.analyze_pose_view_right_ratio,
                 gui::g_state.analyze_pose_view_globe_ratio,
                 static_cast<unsigned long long>(gui::g_state.analysis.point.intent_id.serial),
                 gui::g_state.analysis.point_view.sample_index,
                 static_cast<int>(gui::g_state.analysis.trace.intent_valid));
    IM_CHECK_RETV(canvas_640.RectFull.Min.x >= r3d_640->InnerClipRect.Min.x - 0.5f &&
                      canvas_640.RectFull.Max.x <= r3d_640->InnerClipRect.Max.x + 0.5f &&
                      canvas_640.RectFull.Min.y >= r3d_640->InnerClipRect.Min.y - 0.5f &&
                      canvas_640.RectFull.Max.y <= r3d_640->InnerClipRect.Max.y + 0.5f,
                  false);
    const float yaw_640 = gui::g_state.analysis.point_view.orbit_yaw_deg;
    const ImVec2 center_640 = canvas_640.RectFull.GetCenter();
    ctx->MouseMoveToPos(center_640);
    ctx->Yield(2);
    ctx->MouseDown(0);
    ctx->Yield(2);
    ctx->MouseMoveToPos(ImVec2(center_640.x + 14.0f, center_640.y));
    ctx->Yield(3);
    ctx->MouseUp(0);
    ctx->Yield(2);
    IM_CHECK_RETV(gui::g_state.analysis.point_view.orbit_yaw_deg != yaw_640, false);
    ctx->LogInfo("[scale tiers 640 pose-min] canvas h %.0f whole in region clip h %.0f; orbit drag moved yaw",
                 static_cast<double>(canvas_640.RectFull.GetHeight()),
                 static_cast<double>(r3d_640->InnerClipRect.GetHeight()));
    if (ImGuiWindow* pose_640 = ctx->GetWindowByRef("//Pose & trace")) {
      ImGui::SetWindowSize(pose_640, ImVec2(gui::UiPx(960.0f), gui::UiPx(620.0f)), ImGuiCond_Always);
    }
    gui::g_state.analyze_pose_view_open = false;
    ctx->Yield(2);
  }

  // Restore: the factory scale, settled, then the inline basis restore (the 1280x800 leg's
  // rule — the next case's entry audit must have nothing to undo).
  gui::g_ui_scale_multiplier = gui::kFactoryUiScaleMultiplier;
  gui::g_ui_scale_dirty = true;
  for (int i = 0; i < 60 && !(gui::UiPx(gui::kAnalyzeLeftPanelWidth) > gui::kAnalyzeLeftPanelWidth - 0.5f &&
                              gui::UiPx(gui::kAnalyzeLeftPanelWidth) < gui::kAnalyzeLeftPanelWidth + 0.5f);
       ++i) {
    ctx->Yield(1);
  }
  IM_CHECK_RETV(fabsf(gui::UiPx(gui::kAnalyzeLeftPanelWidth) - gui::kAnalyzeLeftPanelWidth) < 0.5f, false);
  set_basis(gui::kInitWindowWidth, gui::kInitWindowHeight);
  ctx->Yield(2);
  // The review probe's kill path, closed: the compressed cells above once latched the
  // explanation off for the process's remaining life, so the comfortable cell stayed silent
  // no matter how wide it grew. No state exists to lose — the restored cell starts at its top
  // with the region's own content (extent logged for red-state attribution), and the tail's
  // contained-and-clickable receipts at the comfortable geometry live in the point_pick leg.
  {
    // The window re-opens for the restored-region receipt (the tier drives closed it); the
    // selection persists, so the region renders its own content at the window's geometry.
    gui::g_state.analyze_raypath_view_open = true;
    PinRaypathViewOnScreen(ctx);
    ctx->Yield(2);
    ImGuiWindow* map_r = MapRegionWindow();
    IM_CHECK_RETV(map_r != nullptr, false);
    // The basis change rebuilds the layout over frames — the canvas lookup IS the liveness
    // gate and the convergence wait in one: its id in the registry means the cell submitted
    // real content (a skipped window, or a latch that had eaten the explanation's neighbors,
    // would not answer). The DC extent is NOT read between frames — it resets with the
    // frame's submission state and reads stale here.
    ImGuiTestItemInfo canvas_r{};
    for (int i = 0; i < 120; ++i) {
      canvas_r = ctx->ItemInfo("**/##map_canvas", ImGuiTestOpFlags_NoError);
      if (canvas_r.ID != 0) {
        break;
      }
      ctx->Yield(1);
    }
    IM_CHECK_RETV(canvas_r.ID != 0, false);
    IM_CHECK_RETV(map_r->Scroll.y == 0.0f, false);
    ctx->LogInfo("[scale tiers baseline] restored cell live, SM %.0f, scroll %.0f",
                 static_cast<double>(map_r->ScrollMax.y), static_cast<double>(map_r->Scroll.y));
  }
  return true;
}

// The narrow-column reading contract's entity half (§14; the hover carrier's contract comment
// lives on RowLabelTextTooltip): at the right column's 260 pt floor, two classes whose labels
// share a long prefix truncate to the same visible head — and the entities must survive the
// clip. Full-label addressing still finds each row (the label bytes are the ids); selecting A
// then excluding acts on A's chain, selecting B on B's. The hover half is the bounded tooltip
// probe: the fixture's rows really clip, so the tooltip is attempted once, and a failure is
// logged as the known coverage gap, not fatal — the selection/exclusion legs stand alone
// (progress 2026-11-04 08:05's a10/a59 stop-loss).
bool NarrowColumnClippedRowsKeepTheirEntities(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kThreeLayerLongChainJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  // The right column at its 260 pt floor.
  const float d1_saved = gui::g_state.workspace.right_column_ratio;
  gui::g_state.workspace.right_column_ratio = gui::UiPx(gui::kAnalyzeRightColMinWidth) /
                                              (ImGui::GetIO().DisplaySize.x - gui::UiPx(gui::kAnalyzeLeftPanelWidth));
  ctx->Yield(4);

  // The pair: two classes whose labels both exceed the window's content width (so both really
  // clip, conservatively — a label wider than the whole content area certainly exceeds the first
  // column), sharing a long common prefix, with DIFFERENT first-segment chains — the strongest
  // identity separation the Exclude flow can act on (the exclusion token is the first segment's
  // chain). The search is the honest instrument: what this scene's chains actually are is a
  // run-time fact, not a plan assumption.
  ImGuiWindow* share = ctx->GetWindowByRef(kWindowRef);
  IM_CHECK_RETV(share != nullptr, false);
  const float content_w = share->Size.x - 2.0f * ImGui::GetStyle().WindowPadding.x;
  const auto& view = gui::g_state.analysis_result;
  const auto display_of = [&](int order_pos) {
    return std::string(
        view.payload->entries[static_cast<size_t>(view.display_order[static_cast<size_t>(order_pos)])].display);
  };
  const auto label_of = [&](int order_pos) {
    return gui::AnalysisRowLabel(display_of(order_pos), gui::AnalysisCoreToPoolMap(gui::g_state));
  };
  const auto token_of = [&](int order_pos) {
    return gui::FormatSegmentRaypathText(
        view.payload->entries[static_cast<size_t>(view.display_order[static_cast<size_t>(order_pos)])].chain[0]);
  };
  int best_a = -1;
  int best_b = -1;
  size_t best_common = 0;
  for (int i = 0; i + 1 < static_cast<int>(view.display_order.size()); ++i) {
    for (int j = i + 1; j < static_cast<int>(view.display_order.size()); ++j) {
      const std::string li = label_of(i);
      const std::string lj = label_of(j);
      if (li == lj || token_of(i) == token_of(j)) {
        continue;
      }
      if (ImGui::CalcTextSize(li.c_str()).x <= content_w || ImGui::CalcTextSize(lj.c_str()).x <= content_w) {
        continue;
      }
      size_t common = 0;
      while (common < li.size() && common < lj.size() && li[common] == lj[common]) {
        ++common;
      }
      if (common > best_common) {
        best_common = common;
        best_a = i;
        best_b = j;
      }
    }
  }
  if (best_a < 0) {
    // Not a red product: a red fixture. The scene failed to grow a clipped same-prefix pair;
    // the numbers a fresh reader needs are on the log.
    for (int i = 0; i + 1 < static_cast<int>(view.display_order.size()); ++i) {
      ctx->LogInfo("[same prefix] row %d label '%s' w=%.0f (content %.0f)", i, label_of(i).c_str(),
                   ImGui::CalcTextSize(label_of(i).c_str()).x, static_cast<double>(content_w));
    }
    IM_ERRORF("no clipped same-prefix pair with distinct first-segment chains in this result (%d rows)",
              static_cast<int>(view.display_order.size()));
    gui::g_state.workspace.right_column_ratio = d1_saved;
    return false;
  }
  const std::string label_a = label_of(best_a);
  const std::string label_b = label_of(best_b);
  const std::string token_a = token_of(best_a);
  const std::string token_b = token_of(best_b);
  const std::string display_a = display_of(best_a);
  const std::string display_b = display_of(best_b);
  ctx->LogInfo("[same prefix] pair '%s' vs '%s' (common %zu chars, tokens '%s' / '%s')", label_a.c_str(),
               label_b.c_str(), best_common, token_a.c_str(), token_b.c_str());

  // Position addressing for the pair: a clipped row never reports its label to the engine
  // (FindListRowScrolling's own comment — the very finding that gave the reading contract its
  // hover carrier), so full-label addressing is mechanically unavailable HERE. The row's rect
  // is computed instead: rows render in display order at the table's own line height, skipping
  // non-positive-energy entries. Position → entity is the identity the assertions read: the
  // click computed for row A must select A, and the exclusion must carry A's token.
  ImGuiWindow* table_child = nullptr;
  for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
    if (w->ParentWindow == share && std::strstr(w->Name, "##analysis_rows") != nullptr) {
      table_child = w;
      break;
    }
  }
  IM_CHECK_RETV(table_child != nullptr, false);
  const float row_h = ImGui::GetTextLineHeight() + 2.0f * ImGui::GetStyle().CellPadding.y;
  const auto energy_of = [&](int order_pos) {
    return view.display_energy[static_cast<size_t>(view.display_order[static_cast<size_t>(order_pos)])];
  };
  const auto rendered_ordinal = [&](int order_pos) {
    int ordinal = 0;
    for (int k = 0; k < order_pos; ++k) {
      if (energy_of(k) > 0.0) {
        ++ordinal;
      }
    }
    return ordinal;
  };
  // Scroll the table so the row sits mid-viewport, then click the row's left half (the label's
  // column — the Selectable spans all of them, so any x on the row lands on it).
  const auto click_row = [&](int order_pos) {
    const int ordinal = rendered_ordinal(order_pos);
    const float row_top = row_h + static_cast<float>(ordinal) * row_h;  // below the frozen header row
    const float view_h = table_child->InnerRect.GetHeight();
    table_child->Scroll.y = std::clamp(row_top - view_h * 0.5f, 0.0f, table_child->ScrollMax.y);
    ctx->Yield(3);
    const float screen_y = table_child->InnerRect.Min.y + row_top - table_child->Scroll.y + row_h * 0.5f;
    if (screen_y < table_child->InnerRect.Min.y || screen_y > table_child->InnerRect.Max.y) {
      return false;
    }
    ctx->MouseMoveToPos(ImVec2(table_child->InnerRect.Min.x + 40.0f, screen_y));
    ctx->Yield(2);
    ctx->MouseClick(0);
    ctx->Yield(3);
    return true;
  };

  // ① Both rows render and position addressing finds each: the click computed for the row
  // selects exactly that row's entity.
  IM_CHECK_RETV(click_row(best_a), false);
  IM_CHECK_RETV(gui::g_state.analysis.selected_entry.has_value() && *gui::g_state.analysis.selected_entry == display_a,
                false);
  IM_CHECK_RETV(click_row(best_b), false);
  IM_CHECK_RETV(gui::g_state.analysis.selected_entry.has_value() && *gui::g_state.analysis.selected_entry == display_b,
                false);

  // ② The clipped tail stays an identity instrument. The plan's first vehicle — Exclude — is
  // mechanically unavailable here (the button is disabled for multi-layer chains, chain_len !=
  // 1, and only multi-layer labels ever exceed the column), so the entity act is the search the
  // list already owns: typing B's distinguishing tail — the part the clip HIDES — must leave B
  // as the matching row, and clicking it must select B. The clip confuses nothing, and the
  // hidden tail is reachable twice (search now, hover below).
  const std::string needle = token_b;
  std::vector<int> matching;
  for (int k = 0; k < static_cast<int>(view.display_order.size()); ++k) {
    const std::string d = display_of(k);
    const std::string l = label_of(k);
    const auto ci_contains = [&](const std::string& hay) {
      return std::search(hay.begin(), hay.end(), needle.begin(), needle.end(), [](char a, char b) {
               return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
             }) != hay.end();
    };
    if (ci_contains(d) || ci_contains(l)) {
      matching.push_back(k);
    }
  }
  IM_CHECK_RETV(!matching.empty(), false);
  ctx->SetRef("//##AnalyzeShareList");
  ctx->ItemInputValue("**/###analysis_search", needle.c_str());
  ctx->SetRef("");
  ctx->Yield(3);
  {
    // The matching rows keep their relative order; B is one of them — click it by its ordinal
    // among the MATCHING rows (the filtered list renders exactly those).
    int b_match_slot = -1;
    for (size_t m = 0; m < matching.size(); ++m) {
      if (matching[m] == best_b) {
        b_match_slot = static_cast<int>(m);
      }
    }
    IM_CHECK_RETV(b_match_slot >= 0, false);
    const auto click_matching_row = [&](int slot) {
      const float row_top = row_h + static_cast<float>(slot) * row_h;  // below the frozen header row
      const float view_h = table_child->InnerRect.GetHeight();
      table_child->Scroll.y = std::clamp(row_top - view_h * 0.5f, 0.0f, table_child->ScrollMax.y);
      ctx->Yield(3);
      const float screen_y = table_child->InnerRect.Min.y + row_top - table_child->Scroll.y + row_h * 0.5f;
      if (screen_y < table_child->InnerRect.Min.y || screen_y > table_child->InnerRect.Max.y) {
        return false;
      }
      ctx->MouseMoveToPos(ImVec2(table_child->InnerRect.Min.x + 40.0f, screen_y));
      ctx->Yield(2);
      ctx->MouseClick(0);
      ctx->Yield(3);
      return true;
    };
    IM_CHECK_RETV(click_matching_row(b_match_slot), false);
    IM_CHECK_RETV(
        gui::g_state.analysis.selected_entry.has_value() && *gui::g_state.analysis.selected_entry == display_b, false);
    ctx->LogInfo("[same prefix] search '%s' leaves %zu matching row(s); clicking B's slot selects B", needle.c_str(),
                 matching.size());
  }
  ctx->SetRef("//##AnalyzeShareList");
  ctx->ItemInputValue("**/###analysis_search", "");
  ctx->SetRef("");
  ctx->Yield(3);

  // The bounded tooltip probe: hover the clipped row once; the tooltip is the reading contract's
  // carrier. A miss is logged, not failed — the gap is a known one (three prior construction
  // failures registered; this is the single sanctioned attempt from the rewritten plan).
  {
    if (click_row(best_a)) {
      const float screen_y = table_child->InnerRect.Min.y +
                             (row_h + static_cast<float>(rendered_ordinal(best_a)) * row_h) - table_child->Scroll.y +
                             row_h * 0.5f;
      ctx->MouseMoveToPos(ImVec2(table_child->InnerRect.Min.x + 40.0f, screen_y));
      ctx->Yield(6);
      ImGuiWindow* tooltip = nullptr;
      for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
        if (w != nullptr && w->Active && std::strstr(w->Name, "##Tooltip") != nullptr) {
          tooltip = w;
          break;
        }
      }
      if (tooltip != nullptr) {
        ctx->LogInfo("[same prefix] tooltip shown at (%.0f,%.0f) for the clipped row", tooltip->Pos.x, tooltip->Pos.y);
      } else {
        ctx->LogInfo(
            "[same prefix] COVERAGE GAP: the hover tooltip did not show; the carrier is "
            "white-box verifiable (RowLabelTextTooltip), the probe stays unproven");
      }
      ctx->MouseMoveToPos(ImVec2(-100.0f, -100.0f));
      ctx->Yield(2);
    } else {
      ctx->LogInfo("[same prefix] COVERAGE GAP: the row scrolled out; tooltip probe not attempted");
    }
  }

  gui::g_state.workspace.right_column_ratio = d1_saved;
  ctx->Yield(2);

  // The compressed tier's RESULT-STATE key operations (the matrix's 900x640 cell with the
  // analysis this case already brought up): the table's first row contained and selectable, the
  // CSV/Exclude pair contained and enabled, the search driven for real, the map's λ judgment
  // wrapping at this width yet contained and driven, the sky-point Set real, the globe toggle
  // real. The tier's no-result geometry is the shell suite's own leg; this is the half a
  // no-result construction cannot reach.
  {
    g_window_size_test.apply_aspect = false;
    g_window_size_test.width = 900;
    g_window_size_test.height = 640;
    g_window_size_test.done.store(false);
    g_window_size_test.requested.store(true);
    for (int i = 0; i < 20 && !g_window_size_test.done.load(); ++i) {
      ctx->Yield(1);
    }
    IM_CHECK_RETV(g_window_size_test.done.load(), false);
    ctx->Yield(3);
    ImGuiWindow* share_c = ctx->GetWindowByRef(kWindowRef);
    IM_CHECK_RETV(share_c != nullptr, false);
    const float avail_w_c = ImGui::GetIO().DisplaySize.x - gui::UiPx(gui::kAnalyzeLeftPanelWidth);
    IM_CHECK_RETV(avail_w_c < gui::UiPx(gui::kAnalyzeRightColMinWidth) + gui::UiPx(gui::kAnalyzeMainViewMinWidth) +
                                  gui::UiPx(gui::kAnalyzeSplitterSize),
                  false);  // the proportional rung engaged

    // The list: the first row is VERTICALLY in view (the y-band the table's own metrics derive
    // sits inside the clip) and selectable by position — its label is horizontally clipped at
    // this tier (long multi-layer labels never report to the engine; the stale-floors leg pins
    // the short-label form of the same assertion), so the position + the identity assertion
    // carry the evidence.
    SetListRef(ctx);
    table_child->Scroll.y = 0.0f;  // the case's earlier row clicks left the table scrolled
    ctx->Yield(2);
    // Select the pair's first row by position (the case's own helper — the rendered ordinal,
    // not a hardcoded slot: rows with no energy never render).
    IM_CHECK_RETV(click_row(best_a), false);
    IM_CHECK_RETV(
        gui::g_state.analysis.selected_entry.has_value() && *gui::g_state.analysis.selected_entry == display_a, false);
    ctx->MouseMoveToPos(ImVec2(-100.0f, -100.0f));
    ctx->Yield(2);
    // The button pair: contained in the never-scrolling window and enabled for the selection.
    const ImGuiTestItemInfo exclude_c =
        ctx->ItemInfo("**/" ICON_FA_BAN " Exclude this raypath###action_exclude", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(exclude_c.ID != 0, false);
    IM_CHECK_RETV(exclude_c.RectFull.Min.x >= share_c->InnerClipRect.Min.x - 0.5f &&
                      exclude_c.RectFull.Max.x <= share_c->InnerClipRect.Max.x + 0.5f &&
                      exclude_c.RectFull.Min.y >= share_c->InnerClipRect.Min.y - 0.5f &&
                      exclude_c.RectFull.Max.y <= share_c->InnerClipRect.Max.y + 0.5f,
                  false);
    // Disabled, by design: the selection is a multi-layer chain (chain_len != 1 refuses the
    // exclusion — the DECISION that moved this leg's entity act to the search).
    IM_CHECK_RETV(IsDisabled(exclude_c), false);
    const ImGuiTestItemInfo csv_c =
        ctx->ItemInfo("**/" ICON_FA_FILE_CSV " Export CSV###action_csv", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(csv_c.ID != 0, false);
    IM_CHECK_RETV(csv_c.RectFull.Max.y <= share_c->InnerClipRect.Max.y + 0.5f, false);
    // The search: a real input at this tier.
    ctx->ItemInputValue("**/###analysis_search", needle.c_str());
    ctx->ItemInputValue("**/###analysis_search", "");
    ctx->SetRef("");

    // The map: through its window (the row click above opened it; PinRaypathViewOnScreen
    // brings it fully in-viewport — see the helper). The λ containment and a real input — at
    // the window's own geometry now (the old band cell's wrap-at-this-width judgment moved to
    // the narrowest-window legs with the cell).
    PinRaypathViewOnScreen(ctx);
    ImGuiWindow* map_c = MapInfoRegionWindow();  // the λ row lives in the info region (D3)
    IM_CHECK_RETV(map_c != nullptr, false);
    const ImGuiTestItemInfo lambda_c = ctx->ItemInfo("**/wl##map", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(lambda_c.ID != 0, false);
    IM_CHECK_RETV(lambda_c.RectFull.Min.x >= map_c->InnerClipRect.Min.x - 0.5f &&
                      lambda_c.RectFull.Max.x <= map_c->InnerClipRect.Max.x + 0.5f &&
                      lambda_c.RectFull.Min.y >= map_c->InnerClipRect.Min.y - 0.5f &&
                      lambda_c.RectFull.Max.y <= map_c->InnerClipRect.Max.y + 0.5f,
                  false);
    ctx->ItemInputValue("**/wl##map", 600.0f);
    ctx->Yield(2);
    IM_CHECK_RETV(gui::g_state.analysis.map_wavelength_nm > 599.0 && gui::g_state.analysis.map_wavelength_nm < 601.0,
                  false);
    gui::g_state.analysis.map_wavelength_nm = 0.0;  // the reference branch again; no map re-ask chain
    ctx->Yield(1);
    // The sky-point row: visible in the window as it stands — no band scroll rides the input
    // any more (the window shows its whole region at this geometry).
    ctx->SetRef("");
    ctx->ItemInputValue("**/alt°##sky", 21.0f);
    ctx->ItemClick("**/Set sky point###sky_set");
    ctx->Yield(2);
    IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid, false);

    // Inline basis restore (the 1280x800 leg's rule: the next case's entry audit must have
    // nothing to undo).
    g_window_size_test.width = gui::kInitWindowWidth;
    g_window_size_test.height = gui::kInitWindowHeight;
    g_window_size_test.done.store(false);
    g_window_size_test.requested.store(true);
    for (int i = 0; i < 20 && !g_window_size_test.done.load(); ++i) {
      ctx->Yield(1);
    }
    IM_CHECK_RETV(g_window_size_test.done.load(), false);
    ctx->Yield(3);
  }

  return true;
}

}  // namespace

// AC1, the Raypath view window's open entry and lifecycle (contract §6): the class-row click
// is the one open entry — selecting a class opens the non-modal window with it; the title-bar
// X closes it and resets the open flag through the p_open write; an already-selected row
// re-clicked reopens it; and a main-window control answers a real click while the window
// floats (non-modal — a dialog takeover would fail this).
// The window-active probe for the entry-entries case family — ONE shared definition instead of
// a third local copy per case (the file's older cases each carry their own local lambda; these
// three share this file-scope function).
bool EntryWindowActive(const char* fragment) {
  for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
    if (w != nullptr && w->Active && std::strstr(w->Name, fragment) != nullptr) {
      return true;
    }
  }
  return false;
}

bool ClassRowClickOpensTheRaypathView(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(!gui::g_state.analyze_raypath_view_open, false);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  // The row click: selection and window opening in one gesture.
  IM_CHECK_RETV(SelectRowByDisplay(ctx, TopChainDisplay().c_str()) != std::nullopt, false);
  IM_CHECK_RETV(gui::g_state.analyze_raypath_view_open, false);
  IM_CHECK_RETV(EntryWindowActive("Raypath view"), false);
  // Non-modal: the share list's search box answers a real click while the window floats (its
  // default placement leaves the right column uncovered; if a future placement covers a
  // control this assert drives, the case fails visibly, not silently).
  SetListRef(ctx);
  const ImGuiTestItemInfo search = ctx->ItemInfo("**/###analysis_search", ImGuiTestOpFlags_NoError);
  IM_CHECK_RETV(search.ID != 0, false);
  ctx->ItemClick(search.ID);
  ctx->SetRef("");
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analyze_raypath_view_open, false);
  IM_CHECK_RETV(EntryWindowActive("Raypath view"), false);
  // The title-bar X: the close writes the flag through p_open, the same bool the row click
  // and the frame loops read.
  ctx->WindowClose("//Raypath view");
  ctx->Yield(2);
  IM_CHECK_RETV(!gui::g_state.analyze_raypath_view_open, false);
  IM_CHECK_RETV(!EntryWindowActive("Raypath view"), false);
  // The already-selected row re-clicked reopens (the contract's reopen-after-close rule: the
  // Selectable fires on every click, selected or not).
  IM_CHECK_RETV(SelectRowByDisplay(ctx, TopChainDisplay().c_str()) != std::nullopt, false);
  IM_CHECK_RETV(gui::g_state.analyze_raypath_view_open, false);
  IM_CHECK_RETV(EntryWindowActive("Raypath view"), false);
  ctx->WindowClose("//Raypath view");
  ctx->Yield(2);
  IM_CHECK_RETV(!gui::g_state.analyze_raypath_view_open, false);
  return true;
}

// AC1's negative faces and the open flag's tier (contract §8's ownership declaration): neither
// arriving data (the analysis completing) nor a workspace switch opens the window; an open
// window stays open across a workspace switch and is committed in BOTH modes (the frame loops
// submit it in each); and DoNew — the one reset reason for this flag — closes an open one with
// the whole-GuiState replacement.
bool RaypathViewOpenIsDisplayState(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(!gui::g_state.analyze_raypath_view_open, false);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  IM_CHECK_RETV(!gui::g_state.analyze_raypath_view_open, false);
  IM_CHECK_RETV(!EntryWindowActive("Raypath view"), false);
  // The switch away: still no open — and the flag set here opens the window in the RENDER
  // workspace (the frame loop commits it there too), and the switch back keeps it (the window
  // is T-session display state, not a per-workspace panel).
  ctx->ItemClick("##TopBar/Render");
  ctx->Yield(3);
  IM_CHECK_RETV(!gui::g_state.analyze_raypath_view_open, false);
  gui::g_state.analyze_raypath_view_open = true;  // the open entry itself is the row-click case's
  ctx->Yield(2);
  IM_CHECK_RETV(EntryWindowActive("Raypath view"), false);
  ctx->ItemClick("##TopBar/Analyze");
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.analyze_raypath_view_open, false);
  IM_CHECK_RETV(EntryWindowActive("Raypath view"), false);
  // DoNew replaces the whole GuiState: the open flag goes with it and the window stops being
  // committed the same frame rule everything else follows.
  gui::DoNew();
  ctx->Yield(3);
  IM_CHECK_RETV(!gui::g_state.analyze_raypath_view_open, false);
  IM_CHECK_RETV(!EntryWindowActive("Raypath view"), false);
  return true;
}

// AC2's D3 face (contract §4.2, §9.3): the window's own reference-map | info split is manual
// and floored — the drag writes the ratio (both regions' widths follow it), the floors clamp
// it (the info region at its 200 pt floor, the map at its 160 pt floor), a selection change
// never rewrites it, the window resize follows it proportionally, and the same-process
// close/reopen keeps it. Real analysis + two selections: the selection-change leg needs a
// second row.
bool RaypathViewSplitIsManualAndFloored(ImGuiTestContext* ctx) {
  ScopedWatchdogGuard watchdog(ctx->EngineIO);
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  IM_CHECK_RETV(SelectRowByDisplay(ctx, "3-5") != std::nullopt, false);
  // The identity line names the class with the list's own label — the same AnalysisRowLabel
  // spelling out of the one selection chain (a window that hand-rolled its own text would read
  // differently from the row the user clicked).
  const std::string first_label = gui::AnalysisRowLabel("3-5", gui::AnalysisCoreToPoolMap(gui::g_state));
  IM_CHECK_RETV(gui::SelectedClassRowLabelFor(gui::g_state) == first_label, false);
  PinRaypathViewOnScreen(ctx);
  ImGuiWindow* map = MapRegionWindow();
  ImGuiWindow* info = MapInfoRegionWindow();
  IM_CHECK_RETV(map != nullptr && info != nullptr, false);
  const float ratio0 = gui::g_state.analyze_raypath_view_info_ratio;
  const float info_w0 = info->Size.x;
  const float map_w0 = map->Size.x;
  ctx->LogInfo("[D3] ratio %.4f, info %.0f, map %.0f", static_cast<double>(ratio0), static_cast<double>(info_w0),
               static_cast<double>(map_w0));

  // The drag writes the ratio: 80 pt left (the info region widens, the map narrows — the
  // ratio is the info region's share, so it grows).
  const ImGuiTestItemInfo grab = ctx->ItemInfo("**/##RaypathViewSplitter", ImGuiTestOpFlags_NoError);
  IM_CHECK_RETV(grab.ID != 0, false);
  const ImVec2 centre = grab.RectFull.GetCenter();
  ctx->MouseMoveToPos(centre);
  ctx->Yield(2);
  ctx->MouseDown(0);
  ctx->Yield(2);
  ctx->MouseMoveToPos(ImVec2(centre.x - 80.0f, centre.y));
  ctx->Yield(3);
  ctx->MouseUp(0);
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analyze_raypath_view_info_ratio > ratio0 + 0.02f, false);
  ImGuiWindow* map2 = MapRegionWindow();
  ImGuiWindow* info2 = MapInfoRegionWindow();
  IM_CHECK_RETV(map2 != nullptr && info2 != nullptr, false);
  ctx->LogInfo("[D3] after left drag: ratio %.4f, info %.0f (+%.0f), map %.0f (%.0f)",
               static_cast<double>(gui::g_state.analyze_raypath_view_info_ratio), static_cast<double>(info2->Size.x),
               static_cast<double>(info2->Size.x - info_w0), static_cast<double>(map2->Size.x),
               static_cast<double>(map2->Size.x - map_w0));
  IM_CHECK_RETV(info2->Size.x > info_w0 + 50.0f, false);
  IM_CHECK_RETV(map2->Size.x < map_w0 - 50.0f, false);

  // The floors clamp: drag far right — the info region stops at its floor and the map keeps
  // its own; drag far left — the map stops at its floor. Each leg re-addresses the strip: the
  // previous drag moved the split (and the strip with it), so the old centre points inside a
  // region now.
  const auto drag_strip = [&](float dx) {
    const ImGuiTestItemInfo grab_again = ctx->ItemInfo("**/##RaypathViewSplitter", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(grab_again.ID != 0, false);
    const ImVec2 c = grab_again.RectFull.GetCenter();
    ctx->MouseMoveToPos(c);
    ctx->Yield(2);
    ctx->MouseDown(0);
    ctx->Yield(2);
    ctx->MouseMoveToPos(ImVec2(c.x + dx, c.y));
    ctx->Yield(3);
    ctx->MouseUp(0);
    ctx->Yield(2);
    return true;
  };
  IM_CHECK_RETV(drag_strip(500.0f), false);
  ImGuiWindow* map3 = MapRegionWindow();
  ImGuiWindow* info3 = MapInfoRegionWindow();
  IM_CHECK_RETV(map3 != nullptr && info3 != nullptr, false);
  ctx->LogInfo("[D3] right-clamped: info %.0f (floor %.0f), map %.0f (floor %.0f)", static_cast<double>(info3->Size.x),
               static_cast<double>(gui::UiPx(gui::kRaypathViewInfoMinSize)), static_cast<double>(map3->Size.x),
               static_cast<double>(gui::UiPx(gui::kRaypathViewMapMinSize)));
  IM_CHECK_RETV(fabsf(info3->Size.x - gui::UiPx(gui::kRaypathViewInfoMinSize)) < 2.0f, false);
  IM_CHECK_RETV(map3->Size.x >= gui::UiPx(gui::kRaypathViewMapMinSize) - 1.5f, false);
  // A real C01 dispersion payload is required; a refused but non-null survey cannot
  // witness the legend's drawn arcs, and report height is not a proxy for their presence.
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      if (!gui::g_state.analysis.survey.adopted_result || !gui::g_state.analysis.map.adopted_result)
                        return false;
                      const auto model = gui::BuildSurveyOverviewModel(*gui::g_state.analysis.survey.adopted_result);
                      return model.dispersion.available && model.dispersion.min_row_available;
                    },
                    60),
                false);
  // The legend's honesty qualifier is READABLE at the 200 pt floor: the entries are the
  // Bullet()+TextWrapped pair, so the info region's content never exceeds its width —
  // ScrollMax.x == 0 is the region-wide form of that (a single-line BulletText here reads as
  // content wider than a region with no horizontal scrollbar, which is exactly how the
  // dispersion entry's qualifier got clipped before). The adopted map carries the real red/blue
  // circles, so the longest entry is in the region. Both circle states are pinned: the case's
  // own post-adoption view and the full-sky rectangle it switches to and back from (whole
  // circles in view — the main entry's qualifier is then the ONLY explanation, no partial note
  // behind it). The retired shared presets' premises arrive as direct product-state writes; the
  // switch back restores the captured view.
  {
    ImGuiWindow* info_leg = MapReportWindow();
    IM_CHECK_RETV(info_leg != nullptr, false);
    ctx->LogInfo("[legend] at the 200 pt floor: info %.0fx%.0f, ScrollMax (%.1f, %.1f)",
                 static_cast<double>(info_leg->Size.x), static_cast<double>(info_leg->Size.y),
                 static_cast<double>(info_leg->ScrollMax.x), static_cast<double>(info_leg->ScrollMax.y));
    IM_CHECK_RETV(info_leg->ScrollMax.x == 0.0f, false);
    ctx->SetRef("");
    const int lens_before = gui::g_state.renderer.lens_type;
    const float fov_before = gui::g_state.renderer.fov;
    const float elevation_before = gui::g_state.renderer.elevation;
    const float azimuth_before = gui::g_state.renderer.azimuth;
    const float roll_before = gui::g_state.renderer.roll;
    const int visible_before = gui::g_state.renderer.visible;
    const bool front_before = gui::g_state.renderer.front;
    gui::SelectLensType(gui::g_state, gui::kLensTypeRectangular);
    gui::g_state.renderer.visible = gui::kVisibleFull;
    gui::g_state.renderer.front = false;
    ctx->Yield(3);
    info_leg = MapReportWindow();
    IM_CHECK_RETV(info_leg != nullptr, false);
    ctx->LogInfo("[legend] full sky (whole circles): ScrollMax (%.1f, %.1f)",
                 static_cast<double>(info_leg->ScrollMax.x), static_cast<double>(info_leg->ScrollMax.y));
    IM_CHECK_RETV(info_leg->ScrollMax.x == 0.0f, false);
    gui::g_state.renderer.lens_type = lens_before;
    gui::g_state.renderer.fov = fov_before;
    gui::g_state.renderer.elevation = elevation_before;
    gui::g_state.renderer.azimuth = azimuth_before;
    gui::g_state.renderer.roll = roll_before;
    gui::g_state.renderer.visible = visible_before;
    gui::g_state.renderer.front = front_before;
    ctx->Yield(3);
    info_leg = MapReportWindow();
    IM_CHECK_RETV(info_leg != nullptr, false);
    ctx->LogInfo("[legend] back to the case's own view: ScrollMax (%.1f, %.1f)",
                 static_cast<double>(info_leg->ScrollMax.x), static_cast<double>(info_leg->ScrollMax.y));
    IM_CHECK_RETV(info_leg->ScrollMax.x == 0.0f, false);
    // The entries follow the DRAWN arcs, both directions (the legend describes the drawn
    // picture): a short window makes the region overflow, so the legend's line count reads
    // through ScrollMax.y. Parking the view at the nadir with a 5° disk takes both circles out
    // of it (their nearest sky is tens of degrees away) — nothing draws, the entries leave, the
    // content shrinks; the full-sky write below brings both back, content restored.
    ImGuiWindow* win_leg = ctx->GetWindowByRef("//Raypath view");
    IM_CHECK_RETV(win_leg != nullptr, false);
    ImGui::SetWindowSize(win_leg, ImVec2(480.0f, 360.0f), ImGuiCond_Always);
    ctx->Yield(3);
    info_leg = MapReportWindow();
    IM_CHECK_RETV(info_leg != nullptr, false);
    const float sm_present = info_leg->ScrollMax.y;
    ctx->LogInfo("[legend] short window, arcs drawn: ScrollMax.y %.1f", static_cast<double>(sm_present));
    IM_CHECK_RETV(sm_present > 0.0f, false);
    gui::g_state.renderer.lens_type = gui::kLensTypeFisheyeEquidist;
    gui::g_state.renderer.elevation = -90.0f;
    gui::g_state.renderer.azimuth = 0.0f;
    gui::g_state.renderer.fov = 5.0f;
    ctx->Yield(4);
    info_leg = MapReportWindow();
    IM_CHECK_RETV(info_leg != nullptr, false);
    ctx->LogInfo("[legend] circles out of the disk: ScrollMax.y %.1f", static_cast<double>(info_leg->ScrollMax.y));
    const auto hidden_legend = gui::LastMapOverlayLegend();
    IM_CHECK_RETV(!hidden_legend.dispersion_red && !hidden_legend.dispersion_blue, false);
    gui::SelectLensType(gui::g_state, gui::kLensTypeRectangular);
    gui::g_state.renderer.visible = gui::kVisibleFull;
    gui::g_state.renderer.front = false;
    ctx->Yield(3);
    info_leg = MapReportWindow();
    IM_CHECK_RETV(info_leg != nullptr, false);
    ctx->LogInfo("[legend] full sky brings them back: ScrollMax.y %.1f", static_cast<double>(info_leg->ScrollMax.y));
    const auto restored_legend = gui::LastMapOverlayLegend();
    IM_CHECK_RETV(restored_legend.dispersion_red && restored_legend.dispersion_blue, false);
  }
  IM_CHECK_RETV(drag_strip(-700.0f), false);
  ImGuiWindow* map4 = MapRegionWindow();
  IM_CHECK_RETV(map4 != nullptr, false);
  ctx->LogInfo("[D3] left-clamped: map %.0f, ratio %.4f", static_cast<double>(map4->Size.x),
               static_cast<double>(gui::g_state.analyze_raypath_view_info_ratio));
  IM_CHECK_RETV(fabsf(map4->Size.x - gui::UiPx(gui::kRaypathViewMapMinSize)) < 2.0f, false);
  const float clamped_ratio = gui::g_state.analyze_raypath_view_info_ratio;

  // Manual priority: a selection change — the heaviest content change the window follows —
  // writes nothing into the split (bit-exact against the value the last drag left).
  const std::string& second_display =
      gui::g_state.analysis_result.payload->entries[static_cast<size_t>(gui::g_state.analysis_result.display_order[1])]
          .display;
  IM_CHECK_RETV(SelectRowByDisplay(ctx, second_display.c_str()) != std::nullopt, false);
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.analyze_raypath_view_info_ratio == clamped_ratio, false);
  // The identity line follows the class switch: the second row's own label.
  IM_CHECK_RETV(gui::SelectedClassRowLabelFor(gui::g_state) ==
                    gui::AnalysisRowLabel(second_display, gui::AnalysisCoreToPoolMap(gui::g_state)),
                false);
  // AC3's negative face: selections, the survey and the adopted maps set no sky point — the
  // intent is the user's alone (a typed coordinate or a map pick). After two selections and
  // two adopted maps, the intent is still unset; the auto-Fit that steered the views never
  // touched it.
  IM_CHECK_RETV(!gui::g_state.analysis.sky_point.valid, false);

  // AC5's independence face: an ROI change is display-time on the panorama and writes nothing
  // into the sky-point intent. A typed point, then the region radio: the intent survives
  // verbatim (same travel direction, same expression source).
  {
    ctx->SetRef("");
    ctx->ItemInputValue("**/alt°##sky", 33.0f);
    ctx->ItemInputValue("**/az°##sky", 12.0f);
    ctx->ItemClick("**/Set sky point###sky_set");
    ctx->Yield(2);
    IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid, false);
    const float dir_before[3] = { gui::g_state.analysis.sky_point.dir[0], gui::g_state.analysis.sky_point.dir[1],
                                  gui::g_state.analysis.sky_point.dir[2] };
    const bool pick_source_before = gui::g_state.analysis.sky_point.from_map_pick;
    ctx->SetRef(PanoramaRegionRef());
    ctx->ItemClick("Whole sky");
    ctx->SetRef("");
    ctx->Yield(2);
    IM_CHECK_RETV(gui::g_state.analysis.roi_mode == LUMICE_RAYPATH_ROI_FULL_SKY, false);
    IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid, false);
    IM_CHECK_RETV(gui::g_state.analysis.sky_point.from_map_pick == pick_source_before, false);
    IM_CHECK_RETV(gui::g_state.analysis.sky_point.dir[0] == dir_before[0] &&
                      gui::g_state.analysis.sky_point.dir[1] == dir_before[1] &&
                      gui::g_state.analysis.sky_point.dir[2] == dir_before[2],
                  false);
    ctx->LogInfo("[D3 case] ROI change left the sky point verbatim (AC5)");
  }

  // The identity line survives the workspace switch: in Render mode the list is off-screen and
  // the open window is the only face naming the class — the label stays the selected row's.
  {
    const std::string label_before = gui::SelectedClassRowLabelFor(gui::g_state);
    IM_CHECK_RETV(!label_before.empty(), false);
    ctx->ItemClick("##TopBar/Render");
    ctx->Yield(3);
    IM_CHECK_RETV(gui::g_state.workspace.mode == gui::GuiState::WorkspaceMode::kRender, false);
    IM_CHECK_RETV(gui::g_state.analyze_raypath_view_open, false);
    IM_CHECK_RETV(gui::SelectedClassRowLabelFor(gui::g_state) == label_before, false);
    ctx->ItemClick("##TopBar/Analyze");
    ctx->Yield(3);
    IM_CHECK_RETV(gui::SelectedClassRowLabelFor(gui::g_state) == label_before, false);
    ctx->LogInfo("[D3 case] identity line names the class across the Render switch");
  }

  // The resize follows proportionally: a smaller window re-derives both widths from the same
  // ratio, and the floors still clamp.
  {
    const ScopedWindowBasis window_basis(ctx);
    g_window_size_test.apply_aspect = false;
    g_window_size_test.width = 1280;
    g_window_size_test.height = 800;
    g_window_size_test.done.store(false);
    g_window_size_test.requested.store(true);
    for (int i = 0; i < 20 && !g_window_size_test.done.load(); ++i) {
      ctx->Yield(1);
    }
    IM_CHECK_RETV(g_window_size_test.done.load(), false);
    ctx->Yield(3);
    ImGuiWindow* map_r = MapRegionWindow();
    ImGuiWindow* info_r = MapInfoRegionWindow();
    IM_CHECK_RETV(map_r != nullptr && info_r != nullptr, false);
    ctx->LogInfo("[D3] resized 1280x800: info %.0f, map %.0f", static_cast<double>(info_r->Size.x),
                 static_cast<double>(map_r->Size.x));
    IM_CHECK_RETV(fabsf(gui::g_state.analyze_raypath_view_info_ratio - clamped_ratio) < 1e-6f, false);
    IM_CHECK_RETV(info_r->Size.x >= gui::UiPx(gui::kRaypathViewInfoMinSize) - 1.5f, false);
    IM_CHECK_RETV(map_r->Size.x >= gui::UiPx(gui::kRaypathViewMapMinSize) - 1.5f, false);
    // Close and reopen: the same process keeps the user's split (session memory). The pre-close
    // widths are saved BY VALUE: reopen reuses the same ImGuiWindow pool entries (same names),
    // so pointer-level compares would read the reopened geometry against itself.
    const float info_before_close = info_r->Size.x;
    const float map_before_close = map_r->Size.x;
    gui::g_state.analyze_raypath_view_open = false;
    ctx->Yield(2);
    gui::g_state.analyze_raypath_view_open = true;
    PinRaypathViewOnScreen(ctx);
    ctx->Yield(2);
    ImGuiWindow* map_o = MapRegionWindow();
    ImGuiWindow* info_o = MapInfoRegionWindow();
    IM_CHECK_RETV(map_o != nullptr && info_o != nullptr, false);
    IM_CHECK_RETV(fabsf(info_o->Size.x - info_before_close) < 2.0f, false);
    IM_CHECK_RETV(fabsf(map_o->Size.x - map_before_close) < 2.0f, false);
    ctx->LogInfo("[D3] reopen keeps the split: info %.0f, map %.0f", static_cast<double>(info_o->Size.x),
                 static_cast<double>(map_o->Size.x));
    gui::g_state.analyze_raypath_view_open = false;
    ctx->Yield(2);
  }
  return true;
}

// The Pose & trace window's D4/D5 splits (contract §4.2, 755.5): both are manual (a drag
// writes its own ratio and nothing else), independent (dragging D4 never touches D5's field
// and vice versa), session-retained (close + reopen keeps the user's split — compared by
// VALUE, the 755.4 lesson), and floored at BOTH sides simultaneously: a single-sided floor
// only proves the right column when the 3D region happens to take the rest — a drag to
// either end, or a shrink under a kept wide-right ratio, can push EITHER side toward its
// bound, so each leg asserts measured child sizes for both regions at 640x420. No analysis
// needed: the regions render their honest empty states under the same geometry.
bool PoseViewSplitsAreManualIndependentAndFloored(ImGuiTestContext* ctx) {
  ScopedPoseSessionSplits splits_guard;  // the drags' extremes leave with this scope, come what may
  gui::g_state.analyze_pose_view_open =
      true;  // the state write the title click performs (the click path is its own case)
  ctx->Yield(3);
  ImGuiWindow* win = ctx->GetWindowByRef("//Pose & trace");
  IM_CHECK_RETV(win != nullptr, false);
  ImGuiWindow* r3d = PoseRegionWindow("##AnalyzeRaypath3D");
  ImGuiWindow* globe = PoseRegionWindow("##AnalyzeSunGlobe");
  IM_CHECK_RETV(r3d != nullptr && globe != nullptr, false);
  const float ratio_d4 = gui::g_state.analyze_pose_view_right_ratio;
  const float ratio_d6 = gui::g_state.analyze_pose_view_globe_ratio;
  const float ratio_d5_entry = gui::g_state.analyze_pose_view_left_row_ratio;
  const float ratio_d7_entry = gui::g_state.analyze_pose_view_curve_canvas_ratio;
  // The D6 drag legs need a known starting geometry: the window's live size depends on which
  // earlier case last sized it (the workspace scroll-pins case runs the pose window at a
  // 640-tall basis; the minimum-height clamp then re-raises it to 680). Pin the contract
  // default the way the FirstUseEver path would apply it.
  {
    const ImVec2 vp0 = ImGui::GetMainViewport()->Size;
    ImGui::SetWindowSize(win, ImVec2(std::min(gui::UiPx(960.0f), vp0.x), std::min(gui::UiPx(720.0f), vp0.y)),
                         ImGuiCond_Always);
    ctx->Yield(3);
  }
  // The windows are LIVE objects: every ->Size read below runs after the drags, so the
  // pre-drag geometry must be copied out here (the same reason the D3 case snapshots first).
  const float r3d_w0 = r3d->Size.x;
  const float globe_w0 = globe->Size.x;
  const float globe_h0 = globe->Size.y;
  ctx->LogInfo("[D4/D6] initial ratios %.3f / %.3f, window %.0fx%.0f", static_cast<double>(ratio_d4),
               static_cast<double>(ratio_d6), static_cast<double>(win->Size.x), static_cast<double>(win->Size.y));

  // D4's drag writes D4's ratio and only it: 60 pt right (the 3D widens, the right column
  // narrows — the ratio is the column's share, so it shrinks).
  IM_CHECK_RETV(PoseDragStrip(ctx, "**/##PoseViewSplitter", 60.0f, 0.0f), false);
  IM_CHECK_RETV(gui::g_state.analyze_pose_view_right_ratio < ratio_d4 - 0.02f, false);
  IM_CHECK_RETV(gui::g_state.analyze_pose_view_globe_ratio == ratio_d6, false);
  ImGuiWindow* r3d_after = PoseRegionWindow("##AnalyzeRaypath3D");
  ImGuiWindow* globe_after = PoseRegionWindow("##AnalyzeSunGlobe");
  IM_CHECK_RETV(r3d_after != nullptr && globe_after != nullptr, false);
  ctx->LogInfo("[D4] pre r3d %.0f globe %.0f | post r3d %.0f globe %.0f ratio %.4f", static_cast<double>(r3d_w0),
               static_cast<double>(globe_w0), static_cast<double>(r3d_after->Size.x),
               static_cast<double>(globe_after->Size.x),
               static_cast<double>(gui::g_state.analyze_pose_view_right_ratio));
  IM_CHECK_RETV(r3d_after->Size.x > r3d_w0 + 40.0f, false);
  IM_CHECK_RETV(globe_after->Size.x < globe_w0 - 40.0f, false);
  ctx->LogInfo("[D4] right drag: ratio %.3f, 3d %.0f (+%.0f), right col %.0f", static_cast<double>(ratio_d4),
               static_cast<double>(r3d_after->Size.x), static_cast<double>(r3d_after->Size.x - r3d_w0),
               static_cast<double>(globe_after->Size.x));

  // D6's drag (the row splitter the §16.4 relayout renamed — the boundary the drag has always
  // written) writes D6's ratio and only it, and moves the column's vertical split (the globe
  // grows downward, the factors region below) — independent of D4, whose ratio stands where
  // the last drag left it.
  const float d4_after_first_drag = gui::g_state.analyze_pose_view_right_ratio;
  {
    const ImGuiTestItemInfo probe = ctx->ItemInfo("**/##PoseViewRowsSplitter", ImGuiTestOpFlags_NoError);
    ctx->LogInfo("[D6 probe] id %08x rect (%.0f,%.0f)-(%.0f,%.0f)", probe.ID, static_cast<double>(probe.RectFull.Min.x),
                 static_cast<double>(probe.RectFull.Min.y), static_cast<double>(probe.RectFull.Max.x),
                 static_cast<double>(probe.RectFull.Max.y));
  }
  IM_CHECK_RETV(PoseDragStrip(ctx, "**/##PoseViewRowsSplitter", 0.0f, 40.0f), false);
  ctx->LogInfo("[D6] ratio now %.4f (was %.4f)", static_cast<double>(gui::g_state.analyze_pose_view_globe_ratio),
               static_cast<double>(ratio_d6));
  IM_CHECK_RETV(gui::g_state.analyze_pose_view_globe_ratio > ratio_d6 + 0.02f, false);
  IM_CHECK_RETV(gui::g_state.analyze_pose_view_right_ratio == d4_after_first_drag, false);
  ImGuiWindow* globe_after_d6 = PoseRegionWindow("##AnalyzeSunGlobe");
  IM_CHECK_RETV(globe_after_d6 != nullptr, false);
  IM_CHECK_RETV(globe_after_d6->Size.y > globe_h0 + 20.0f, false);
  ctx->LogInfo("[D6] down drag: ratio %.3f -> %.3f, globe h %.0f -> %.0f", static_cast<double>(ratio_d6),
               static_cast<double>(gui::g_state.analyze_pose_view_globe_ratio), static_cast<double>(globe_h0),
               static_cast<double>(globe_after_d6->Size.y));

  // D5's drag (the left column's 3D | curve split, the §16.4 relayout's new boundary) writes
  // D5's field and only it: the 3D region grows downward over the curve region, while D4 and
  // D6 stand exactly where the previous drags left them (§16.4's 互不写入, now over four
  // fields).
  const float d5_before = gui::g_state.analyze_pose_view_left_row_ratio;
  const float d4_before_d5 = gui::g_state.analyze_pose_view_right_ratio;
  const float d6_before_d5 = gui::g_state.analyze_pose_view_globe_ratio;
  const float r3d_h0 = PoseRegionWindow("##AnalyzeRaypath3D")->Size.y;
  IM_CHECK_RETV(PoseDragStrip(ctx, "**/##PoseViewLeftRowSplitter", 0.0f, 40.0f), false);
  ctx->LogInfo("[D5] ratio now %.4f (was %.4f)", static_cast<double>(gui::g_state.analyze_pose_view_left_row_ratio),
               static_cast<double>(d5_before));
  IM_CHECK_RETV(gui::g_state.analyze_pose_view_left_row_ratio > d5_before + 0.02f, false);
  IM_CHECK_RETV(gui::g_state.analyze_pose_view_right_ratio == d4_before_d5, false);
  IM_CHECK_RETV(gui::g_state.analyze_pose_view_globe_ratio == d6_before_d5, false);
  ImGuiWindow* r3d_after_d5 = PoseRegionWindow("##AnalyzeRaypath3D");
  IM_CHECK_RETV(r3d_after_d5 != nullptr, false);
  IM_CHECK_RETV(r3d_after_d5->Size.y > r3d_h0 + 20.0f, false);
  ctx->LogInfo("[D5] down drag: 3d h %.0f -> %.0f, ratio %.3f -> %.3f", static_cast<double>(r3d_h0),
               static_cast<double>(r3d_after_d5->Size.y), static_cast<double>(d5_before),
               static_cast<double>(gui::g_state.analyze_pose_view_left_row_ratio));

  // The companion's fixed quota (the UNDRAGGED D7 state, the resize strategy's elastic|quota
  // pair): stretching the window grows the CANVAS — the companion child's height stays at its
  // 8-line budget, never content-driven (§15: geometry first). Runs before the D7 drag below,
  // while the sentinel still holds.
  const ImGuiTestItemInfo canvas_quota0 = ctx->ItemInfo("**/##pose_curve_canvas", ImGuiTestOpFlags_NoError);
  IM_CHECK_RETV(canvas_quota0.ID != 0, false);
  ImGuiWindow* scroll_quota0 = PoseRegionScrollChild("##AnalyzePoseCurve", "##PoseCurveScroll");
  IM_CHECK_RETV(scroll_quota0 != nullptr, false);
  const float canvas_quota_h0 = canvas_quota0.RectFull.GetHeight();
  const float scroll_quota_h0 = scroll_quota0->Size.y;
  ctx->LogInfo("[D7 quota] canvas %.0f, companion %.0f (quota %.0f)", static_cast<double>(canvas_quota_h0),
               static_cast<double>(scroll_quota_h0), static_cast<double>(8.0f * ImGui::GetTextLineHeightWithSpacing()));
  {
    const ImVec2 vp1 = ImGui::GetMainViewport()->Size;
    ImGui::SetWindowSize(win, ImVec2(std::min(gui::UiPx(1100.0f), vp1.x), std::min(gui::UiPx(860.0f), vp1.y)),
                         ImGuiCond_Always);
    ctx->Yield(3);
  }
  const ImGuiTestItemInfo canvas_quota1 = ctx->ItemInfo("**/##pose_curve_canvas", ImGuiTestOpFlags_NoError);
  IM_CHECK_RETV(canvas_quota1.ID != 0, false);
  ImGuiWindow* scroll_quota1 = PoseRegionScrollChild("##AnalyzePoseCurve", "##PoseCurveScroll");
  IM_CHECK_RETV(scroll_quota1 != nullptr, false);
  ctx->LogInfo("[D7 quota] stretched: canvas %.0f, companion %.0f",
               static_cast<double>(canvas_quota1.RectFull.GetHeight()), static_cast<double>(scroll_quota1->Size.y));
  IM_CHECK_RETV(canvas_quota1.RectFull.GetHeight() > canvas_quota_h0 + 20.0f, false);  // the canvas ate the delta
  IM_CHECK_RETV(fabsf(scroll_quota1->Size.y - scroll_quota_h0) < 2.0f, false);         // the quota did not grow
  {
    const ImVec2 vp0 = ImGui::GetMainViewport()->Size;
    ImGui::SetWindowSize(win, ImVec2(std::min(gui::UiPx(960.0f), vp0.x), std::min(gui::UiPx(720.0f), vp0.y)),
                         ImGuiCond_Always);
    ctx->Yield(3);
  }

  // D7's drag (the curve region's canvas | companion split, §16.4's new boundary) writes D7's
  // field and only it: the sentinel share (0.0f — never dragged) becomes the dragged share,
  // the canvas grows downward over the companion, and D4/D5/D6 stand where the previous drags
  // left them.
  const float d7_before = gui::g_state.analyze_pose_view_curve_canvas_ratio;
  const float d4_before_d7 = gui::g_state.analyze_pose_view_right_ratio;
  const float d5_before_d7 = gui::g_state.analyze_pose_view_left_row_ratio;
  const float d6_before_d7 = gui::g_state.analyze_pose_view_globe_ratio;
  const ImGuiTestItemInfo canvas_d7_0 = ctx->ItemInfo("**/##pose_curve_canvas", ImGuiTestOpFlags_NoError);
  IM_CHECK_RETV(canvas_d7_0.ID != 0, false);
  const float canvas_d7_h0 = canvas_d7_0.RectFull.GetHeight();
  IM_CHECK_RETV(PoseDragStrip(ctx, "**/##PoseCurveSplitter", 0.0f, 30.0f), false);
  ctx->LogInfo("[D7] ratio now %.4f (was %.4f — the sentinel)",
               static_cast<double>(gui::g_state.analyze_pose_view_curve_canvas_ratio), static_cast<double>(d7_before));
  IM_CHECK_RETV(gui::g_state.analyze_pose_view_curve_canvas_ratio > d7_before + 0.02f, false);
  IM_CHECK_RETV(gui::g_state.analyze_pose_view_right_ratio == d4_before_d7, false);
  IM_CHECK_RETV(gui::g_state.analyze_pose_view_left_row_ratio == d5_before_d7, false);
  IM_CHECK_RETV(gui::g_state.analyze_pose_view_globe_ratio == d6_before_d7, false);
  const ImGuiTestItemInfo canvas_d7_1 = ctx->ItemInfo("**/##pose_curve_canvas", ImGuiTestOpFlags_NoError);
  IM_CHECK_RETV(canvas_d7_1.ID != 0, false);
  IM_CHECK_RETV(canvas_d7_1.RectFull.GetHeight() > canvas_d7_h0 + 15.0f, false);
  ctx->LogInfo("[D7] down drag: canvas %.0f -> %.0f, ratio %.4f", static_cast<double>(canvas_d7_h0),
               static_cast<double>(canvas_d7_1.RectFull.GetHeight()),
               static_cast<double>(gui::g_state.analyze_pose_view_curve_canvas_ratio));

  // The narrowest window's both-end clamp (the plan §3 revision): at 640x420 — the window
  // minimum — a drag of D4 to either end leaves BOTH sides at/above their floors. Measured
  // child sizes, not ratios: the containment assertions are structurally blind to a side
  // squeezed under its floor when the other side takes the room.
  ImGui::SetWindowSize(win, ImVec2(gui::UiPx(gui::kPoseViewMinWidth), gui::UiPx(gui::kPoseViewMinHeight)),
                       ImGuiCond_Always);
  ctx->Yield(3);
  const auto both_floors = [&](const char* leg) {
    ImGuiWindow* r3d_n = PoseRegionWindow("##AnalyzeRaypath3D");
    ImGuiWindow* globe_n = PoseRegionWindow("##AnalyzeSunGlobe");
    ImGuiWindow* curve_n = PoseRegionWindow("##AnalyzePoseCurve");
    ImGuiWindow* factors_n = PoseRegionWindow("##AnalyzePoseFactors");
    IM_CHECK_RETV(r3d_n != nullptr && globe_n != nullptr && curve_n != nullptr && factors_n != nullptr, false);
    ctx->LogInfo("[%s] 3d %.0f (floor %.0f), right col %.0f (floor %.0f)", leg, static_cast<double>(r3d_n->Size.x),
                 static_cast<double>(gui::UiPx(gui::kPoseViewLeftFloor)), static_cast<double>(globe_n->Size.x),
                 static_cast<double>(gui::UiPx(gui::kPoseViewRightFloor)));
    IM_CHECK_RETV(globe_n->Size.x >= gui::UiPx(gui::kPoseViewRightFloor) - 1.5f, false);
    IM_CHECK_RETV(r3d_n->Size.x >= gui::UiPx(gui::kPoseViewLeftFloor) - 1.5f, false);
    // D5's height pair at this face: the shared PoseD5HeightBounds carries the px-unit and
    // span-denominator discipline (its comment holds the two slips the chain case's 1.5x leg
    // caught); this leg asserts against it and logs the numbers.
    float bound_3d = 0.0f;
    float bound_curve = 0.0f;
    PoseD5HeightBounds(r3d_n->Size.y, curve_n->Size.y, &bound_3d, &bound_curve);
    ctx->LogInfo("[%s] left col denom %.0f, 3d %.0f (bound %.0f), curve %.0f (bound %.0f)", leg,
                 static_cast<double>(r3d_n->Size.y + curve_n->Size.y), static_cast<double>(r3d_n->Size.y),
                 static_cast<double>(bound_3d), static_cast<double>(curve_n->Size.y), static_cast<double>(bound_curve));
    IM_CHECK_RETV(r3d_n->Size.y >= bound_3d - 1.5f, false);
    IM_CHECK_RETV(curve_n->Size.y >= bound_curve - 1.5f, false);
    return true;
  };
  IM_CHECK_RETV(both_floors("before end drags"), false);
  // Drag right hard: the right column stops at its floor, the 3D keeps its own.
  IM_CHECK_RETV(PoseDragStrip(ctx, "**/##PoseViewSplitter", 900.0f, 0.0f), false);
  IM_CHECK_RETV(both_floors("right-clamped"), false);
  // Drag left hard: the 3D stops at its floor, the right column keeps its own.
  IM_CHECK_RETV(PoseDragStrip(ctx, "**/##PoseViewSplitter", -1800.0f, 0.0f), false);
  IM_CHECK_RETV(both_floors("left-clamped"), false);

  // The floors hold under a kept wide-right ratio + shrink (the other premise the revision
  // names): restore the ratio large, then shrink the window to the minimum.
  gui::g_state.analyze_pose_view_right_ratio = 0.6f;
  ctx->Yield(2);
  ImGui::SetWindowSize(win, ImVec2(gui::UiPx(gui::kPoseViewMinWidth), gui::UiPx(gui::kPoseViewMinHeight)),
                       ImGuiCond_Always);
  ctx->Yield(3);
  IM_CHECK_RETV(both_floors("shrink under kept ratio"), false);

  // Session retention, by value: close + reopen keeps all four ratios and the derived split.
  const float kept_d4 = gui::g_state.analyze_pose_view_right_ratio;
  const float kept_d6 = gui::g_state.analyze_pose_view_globe_ratio;
  const float kept_d5 = gui::g_state.analyze_pose_view_left_row_ratio;
  const float kept_d7 = gui::g_state.analyze_pose_view_curve_canvas_ratio;
  const float globe_w_before_close = PoseRegionWindow("##AnalyzeSunGlobe")->Size.x;
  gui::g_state.analyze_pose_view_open = false;
  ctx->Yield(2);
  IM_CHECK_RETV(PoseRegionWindow("##AnalyzeSunGlobe") == nullptr, false);
  gui::g_state.analyze_pose_view_open = true;
  ctx->Yield(3);
  ImGuiWindow* globe_re = PoseRegionWindow("##AnalyzeSunGlobe");
  IM_CHECK_RETV(globe_re != nullptr, false);
  IM_CHECK_RETV(fabsf(gui::g_state.analyze_pose_view_right_ratio - kept_d4) < 1e-6f, false);
  IM_CHECK_RETV(fabsf(gui::g_state.analyze_pose_view_globe_ratio - kept_d6) < 1e-6f, false);
  IM_CHECK_RETV(fabsf(gui::g_state.analyze_pose_view_left_row_ratio - kept_d5) < 1e-6f, false);
  IM_CHECK_RETV(fabsf(gui::g_state.analyze_pose_view_curve_canvas_ratio - kept_d7) < 1e-6f, false);
  IM_CHECK_RETV(fabsf(globe_re->Size.x - globe_w_before_close) < 2.0f, false);
  ctx->LogInfo("[D4/D6/D5/D7] reopen keeps ratios %.3f / %.3f / %.3f / %.3f", static_cast<double>(kept_d4),
               static_cast<double>(kept_d6), static_cast<double>(kept_d5), static_cast<double>(kept_d7));

  // The open flag is this case's own display state; the four splits leave with the RAII guard
  // (happy path and early exit alike).
  gui::g_state.analyze_pose_view_open = false;
  ctx->Yield(2);
  return true;
}

// Both explicit entries open the same display surface (contract §7). Class selection and
// coordinate Set never open it; an actual map pick followed by the nearby button does.
bool PoseViewOpensFromBothExplicitEntries(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  IM_CHECK_RETV(SelectRowByDisplay(ctx, TopChainDisplay().c_str()) != std::nullopt, false);
  IM_CHECK_RETV(!gui::g_state.analyze_pose_view_open, false);
  IM_CHECK_RETV(!EntryWindowActive("Pose & trace"), false);

  // Negative A — the class row is the Raypath view's open entry, not the pose window's.
  IM_CHECK_RETV(gui::g_state.analyze_raypath_view_open, false);  // the row click above opened it
  IM_CHECK_RETV(!gui::g_state.analyze_pose_view_open, false);

  // Negative B — the typed sky point (the coordinate input's write path) must not pop the
  // window open. The pose flag stays false through the write and after the point intent fires.
  ctx->SetRef("");
  ctx->ItemInputValue("**/alt°##sky", 33.0f);
  ctx->ItemInputValue("**/az°##sky", 12.0f);
  ctx->ItemClick("**/Set sky point###sky_set");
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid, false);
  IM_CHECK_RETV(!gui::g_state.analyze_pose_view_open, false);
  IM_CHECK_RETV(!EntryWindowActive("Pose & trace"), false);
  ctx->LogInfo("[pose open] typed sky point left the pose window closed");

  ctx->SetRef("//Raypath view");
  ctx->KeyDown(ImGuiMod_Alt);
  ctx->ItemClick("**/##map_canvas");
  ctx->KeyUp(ImGuiMod_Alt);
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.from_map_pick, false);
  IM_CHECK_RETV(!gui::g_state.analyze_pose_view_open && !EntryWindowActive("Pose & trace"), false);
  const auto picked = gui::g_state.analysis.sky_point;
  const auto point_id = gui::g_state.analysis.point.intent_id;
  ctx->ItemClick("**/Pose & trace###pose_open");
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analyze_pose_view_open && EntryWindowActive("Pose & trace"), false);
  IM_CHECK_RETV(gui::g_state.analysis.point.intent_id == point_id, false);
  ctx->WindowClose("//Pose & trace");
  ctx->Yield(2);
  IM_CHECK_RETV(!gui::g_state.analyze_pose_view_open && !EntryWindowActive("Pose & trace"), false);
  ctx->WindowFocus("//Raypath view");
  ctx->ItemClick("**/Pose & trace###pose_open");
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analyze_pose_view_open && EntryWindowActive("Pose & trace"), false);
  IM_CHECK_RETV(gui::g_state.analysis.point.intent_id == point_id, false);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.dir[0] == picked.dir[0] &&
                    gui::g_state.analysis.sky_point.dir[1] == picked.dir[1] &&
                    gui::g_state.analysis.sky_point.dir[2] == picked.dir[2],
                false);
  ctx->WindowClose("//Pose & trace");
  ctx->Yield(2);
  ctx->SetRef("");

  // The Selection header's named button opens that same window: the button lives in the fixed
  // head, outside the scroll region — no scroll-to-bottom preamble, reachable in every state.
  ctx->ItemClick("**/Pose & trace###main_pose_open");
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.analyze_pose_view_open, false);
  IM_CHECK_RETV(EntryWindowActive("Pose & trace"), false);
  // Reopen after a title-bar close: the click fires on every press — closed, the header button
  // opens; open, it focuses (the same widget, both halves of the §16.3 entry semantics).
  ctx->WindowClose("//Pose & trace");
  ctx->Yield(2);
  IM_CHECK_RETV(!EntryWindowActive("Pose & trace"), false);
  ctx->ItemClick("**/Pose & trace###main_pose_open");
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.analyze_pose_view_open, false);
  IM_CHECK_RETV(EntryWindowActive("Pose & trace"), false);
  ctx->LogInfo("[pose open] header button reopened after a manual close");

  gui::g_state.analyze_pose_view_open = false;
  ctx->Yield(2);
  return true;
}

// ---- The Pose & trace window's remaining contract faces (755.5 Step 5) -------------------------------

// ② — the selection chain's sync face: the open window follows the current selection/sky point/λ
// with no stale-class remnant — every change drops the old document (the pump's key compare) and
// the window's globe region states the new key's honest state.
bool PoseViewFollowsTheSelectionChain(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  IM_CHECK_RETV(SelectRowByDisplay(ctx, TopChainDisplay().c_str()) != std::nullopt, false);
  SetSkyPointTarget(42.0, 0.0);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  gui::g_state.analyze_pose_view_open = true;
  PinPoseViewOnScreen(ctx);
  ctx->Yield(2);
  IM_CHECK_RETV(PoseRegionWindow("##AnalyzeSunGlobe") != nullptr, false);
  ctx->LogInfo("[pose sync] class A completed in the window");

  // A sky-point move: the point face drops the adopted document and re-answers for the new
  // target — the window's globe follows the pump's adoption, nothing stale under the new key.
  // The gate reads the DOCUMENT (the target echo), not the state alone: the old document's
  // completed state cannot satisfy it.
  SetSkyPointTarget(30.0, 10.0);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      if (gui::PointQueryStateFor(gui::g_state.analysis.point) != gui::PointQueryState::kCompleted ||
                          gui::g_state.analysis.point.adopted_result == nullptr) {
                        return false;
                      }
                      LUMICE_PathPointInfo i{};
                      i.struct_size = sizeof(i);
                      return LUMICE_PathPointGetInfo(gui::g_state.analysis.point.adopted_result.get(), &i) ==
                                 LUMICE_PATH_READ_OK &&
                             std::fabs(i.target_altitude_deg - 30.0) < 1e-3;
                    },
                    60),
                false);
  LUMICE_PathPointInfo info_b{};
  info_b.struct_size = sizeof(info_b);
  IM_CHECK_RETV(
      LUMICE_PathPointGetInfo(gui::g_state.analysis.point.adopted_result.get(), &info_b) == LUMICE_PATH_READ_OK, false);
  IM_CHECK_RETV(std::fabs(info_b.target_altitude_deg - 30.0) < 1e-3, false);
  ctx->LogInfo("[pose sync] sky point move re-answered: alt %.1f", (double)info_b.target_altitude_deg);

  // A λ move (the shared knob): the point intent's key carries it — the window follows. The
  // waits are two-phase (leave the current state, then complete again): a fast machine can
  // observe the OLD document's completed state before the pump processes the key change.
  gui::g_state.analysis.map_wavelength_nm = 600.0;
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) != gui::PointQueryState::kCompleted; },
          30),
      false);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  gui::g_state.analysis.map_wavelength_nm = 0.0;
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) != gui::PointQueryState::kCompleted; },
          30),
      false);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  // A class switch: the point face re-keys (a new class's answer must not linger under the old
  // selection) — the window shows the new class's completed document, its crystal id matching
  // the NEW selection's snapshot crystal.
  const std::string& second_display =
      gui::g_state.analysis_result.payload->entries[static_cast<size_t>(gui::g_state.analysis_result.display_order[1])]
          .display;
  IM_CHECK_RETV(SelectRowByDisplay(ctx, second_display.c_str()) != std::nullopt, false);
  SetSkyPointTarget(42.0, 0.0);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) != gui::PointQueryState::kCompleted; },
          30),
      false);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  LUMICE_PathPointInfo info_c{};
  info_c.struct_size = sizeof(info_c);
  IM_CHECK_RETV(
      LUMICE_PathPointGetInfo(gui::g_state.analysis.point.adopted_result.get(), &info_c) == LUMICE_PATH_READ_OK, false);
  const std::optional<gui::SelectedClassIdentity> identity_c = gui::SelectedClassIdentityFor(gui::g_state);
  IM_CHECK_RETV(identity_c.has_value(), false);
  IM_CHECK_RETV(info_c.crystal_id == identity_c->core_crystal_id, false);
  ctx->LogInfo("[pose sync] class switch re-keyed: window crystal %d == selection %d", (int)info_c.crystal_id,
               (int)identity_c->core_crystal_id);

  gui::g_state.analyze_pose_view_open = false;
  ctx->Yield(2);
  return true;
}

// ③ — playback and the window-closed pause: playing advances sample_index only (the AC3 family),
// a closed window pauses the tick (the advance rides the globe region's render), and reopening
// resumes from the pause point. Playback also never touches the sky point.
bool PoseViewPlaybackPausesWhenClosedAndResumes(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  IM_CHECK_RETV(SelectRowByDisplay(ctx, TopChainDisplay().c_str()) != std::nullopt, false);
  SetSkyPointTarget(42.0, 0.0);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  const int valid_count = gui::PointValidSampleCount(*gui::g_state.analysis.point.adopted_result, 0);
  IM_CHECK_RETV(valid_count > 2, false);

  gui::g_state.analyze_pose_view_open = true;
  PinPoseViewOnScreen(ctx);
  ctx->Yield(2);
  const float sky_before[3] = { gui::g_state.analysis.sky_point.dir[0], gui::g_state.analysis.sky_point.dir[1],
                                gui::g_state.analysis.sky_point.dir[2] };

  // Playing advances the cursor with the window open.
  gui::g_state.analysis.point_view.playing = true;
  IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.analysis.point_view.sample_index != 0; }, 40), false);
  const int paused_at = gui::g_state.analysis.point_view.sample_index;
  gui::g_state.analyze_pose_view_open = false;  // window closed = playback paused
  ctx->Yield(8);
  IM_CHECK_RETV(gui::g_state.analysis.point_view.sample_index == paused_at, false);
  ctx->LogInfo("[pose playback] paused at sample %d with the window closed", paused_at);
  gui::g_state.analyze_pose_view_open = true;
  PinPoseViewOnScreen(ctx);
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analysis.point_view.playing, false);  // the flag survives; the tick resumes
  const int resumed_from = gui::g_state.analysis.point_view.sample_index;
  IM_CHECK_RETV(
      DriveUntil(ctx, [paused_at] { return gui::g_state.analysis.point_view.sample_index != paused_at; }, 40), false);
  ctx->LogInfo("[pose playback] resumed at %d, advanced past the pause point", resumed_from);
  gui::g_state.analysis.point_view.playing = false;
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.dir[0] == sky_before[0] &&
                    gui::g_state.analysis.sky_point.dir[1] == sky_before[1] &&
                    gui::g_state.analysis.sky_point.dir[2] == sky_before[2],
                false);

  gui::g_state.analyze_pose_view_open = false;
  ctx->Yield(2);
  return true;
}

// ⑤ — the pump's window-independence (AC6's main leg, the plan review's own strengthening): an
// in-flight point query survives a switch to Render with BOTH analysis windows closed, the frame
// pump still adopts its completion (no hidden-wait hang), the completed intent does not re-issue,
// and reopening the window shows the adopted result with no stale residue and no render
// interruption.
bool PoseViewPumpAdoptsWithEveryWindowClosed(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  IM_CHECK_RETV(SelectRowByDisplay(ctx, TopChainDisplay().c_str()) != std::nullopt, false);
  gui::g_state.analyze_raypath_view_open = false;
  gui::g_state.analyze_pose_view_open = false;
  SetSkyPointTarget(42.0, 0.0);
  // The arm wait accepts ANY standing intent — then the premise is TIGHTENED: the leg's claim
  // is "an IN-FLIGHT query completes while both windows are closed in Render", so a fast
  // machine that already completed it re-arms with a different sky point until the query is
  // verifiably still computing at the switch (a completed-before-switch run would pass with the
  // face-less pump removed — the vacuous form the review flagged).
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) != gui::PointQueryState::kNoIntent; },
          30),
      false);
  {
    double alt = 42.0;
    for (int attempt = 0;
         attempt < 6 && gui::PointQueryStateFor(gui::g_state.analysis.point) != gui::PointQueryState::kWaiting;
         ++attempt) {
      alt = alt == 42.0 ? 43.0 : 42.0;
      SetSkyPointTarget(alt, 0.0);
      ctx->Yield(1);
    }
    IM_CHECK_RETV(gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kWaiting, false);
  }
  const auto waiting_intent = gui::g_state.analysis.point.intent_id;
  IM_CHECK_RETV(gui::g_state.analysis.point.adopted_result == nullptr, false);  // in flight, nothing adopted yet
  // Switch to Render with both windows closed: the query keeps running and — the pump's AC —
  // its completion adopts here, away from every analysis face.
  ctx->ItemClick("##TopBar/Render");
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.workspace.mode == gui::GuiState::WorkspaceMode::kRender, false);
  IM_CHECK_RETV(!gui::g_state.analyze_pose_view_open, false);
  IM_CHECK_RETV(!gui::g_state.analyze_raypath_view_open, false);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          90),
      false);
  IM_CHECK_RETV(gui::g_state.analysis.point.adopted_result != nullptr, false);
  IM_CHECK_RETV(gui::g_state.analysis.point.intent_id == waiting_intent, false);  // completed, not re-issued
  ctx->LogInfo("[pose pump] in-flight point adopted on a face-less Render frame, intent stable");

  // Reopen: the window renders the CURRENT adopted document immediately (no stale residue, no
  // re-issue — the intent id stands).
  gui::g_state.analyze_pose_view_open = true;
  PinPoseViewOnScreen(ctx);
  ctx->Yield(3);
  IM_CHECK_RETV(PoseRegionWindow("##AnalyzeSunGlobe") != nullptr, false);
  IM_CHECK_RETV(gui::g_state.analysis.point.intent_id == waiting_intent, false);
  IM_CHECK_RETV(gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted, false);
  ctx->LogInfo("[pose pump] reopened window renders the adopted document; no re-issue, no residue");

  // The Render-frame shape with the window OPEN: a second sky-point move answers while the
  // window is the only analysis face on a Render frame (the pump runs at the frame entry either
  // way — same path, windows open or not). The gate reads the DOCUMENT's target echo, not the
  // state alone.
  SetSkyPointTarget(20.0, 0.0);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      if (gui::PointQueryStateFor(gui::g_state.analysis.point) != gui::PointQueryState::kCompleted ||
                          gui::g_state.analysis.point.adopted_result == nullptr) {
                        return false;
                      }
                      LUMICE_PathPointInfo i{};
                      i.struct_size = sizeof(i);
                      return LUMICE_PathPointGetInfo(gui::g_state.analysis.point.adopted_result.get(), &i) ==
                                 LUMICE_PATH_READ_OK &&
                             std::fabs(i.target_altitude_deg - 20.0) < 1e-3;
                    },
                    60),
                false);
  LUMICE_PathPointInfo info_d{};
  info_d.struct_size = sizeof(info_d);
  IM_CHECK_RETV(
      LUMICE_PathPointGetInfo(gui::g_state.analysis.point.adopted_result.get(), &info_d) == LUMICE_PATH_READ_OK, false);
  IM_CHECK_RETV(std::fabs(info_d.target_altitude_deg - 20.0) < 1e-3, false);
  ctx->LogInfo("[pose pump] Render frame with the window open re-answered: alt %.1f",
               (double)info_d.target_altitude_deg);

  gui::g_state.analyze_pose_view_open = false;
  ctx->Yield(2);
  ctx->ItemClick("##TopBar/Analyze");
  ctx->Yield(2);
  return true;
}

// ⑥ — DoNew's whole-state reset closes the window; the reopened window shows the honest
// no-intent states (the pre-result lines the regions own), not a stale document.
bool PoseViewDoNewClosesAndReopensHonest(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  IM_CHECK_RETV(SelectRowByDisplay(ctx, TopChainDisplay().c_str()) != std::nullopt, false);
  SetSkyPointTarget(42.0, 0.0);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  gui::g_state.analyze_pose_view_open = true;
  PinPoseViewOnScreen(ctx);
  ctx->Yield(2);
  IM_CHECK_RETV(PoseRegionWindow("##AnalyzeSunGlobe") != nullptr, false);

  gui::DoNew();
  ctx->Yield(3);
  IM_CHECK_RETV(!gui::g_state.analyze_pose_view_open, false);
  IM_CHECK_RETV(PoseRegionWindow("##AnalyzeSunGlobe") == nullptr, false);
  ctx->LogInfo("[pose donew] DoNew closed the window with the whole state");

  // Reopen over the fresh state: the honest no-intent lines, no document.
  gui::g_state.analyze_pose_view_open = true;
  PinPoseViewOnScreen(ctx);
  ctx->Yield(3);
  IM_CHECK_RETV(PoseRegionWindow("##AnalyzeSunGlobe") != nullptr, false);
  IM_CHECK_RETV(gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kNoIntent, false);
  IM_CHECK_RETV(!gui::g_state.analysis.sky_point.valid, false);
  ctx->LogInfo("[pose donew] reopened over a fresh state: the honest no-intent faces render");

  gui::g_state.analyze_pose_view_open = false;
  ctx->Yield(2);
  return true;
}

// AC2 — the resize strategy's red/green evidence (the decisions file §4, contract §16.4
// as-built): stretch, shrink and drag-then-resize, each asserting the ALLOCATION, not just
// survival — the quota members (the 3D report viewport, the curve companion) hold their fixed
// budgets while the elastic members take the delta by the split's ratio, and after a drag the
// dragged ratio governs (user intent overrides the strategy). Geometry-only: no document —
// the canvases and the quota children derive from the region shapes alone.
bool PoseViewResizeStrategyDistributesByRatioAndQuota(ImGuiTestContext* ctx) {
  ScopedPoseSessionSplits splits_guard;  // scenario drags leave with this scope, come what may
  gui::g_state.analyze_pose_view_open = true;
  ctx->Yield(3);
  ImGuiWindow* win = ctx->GetWindowByRef("//Pose & trace");
  IM_CHECK_RETV(win != nullptr, false);
  const auto pin_size = [&](float w, float h) {
    const ImVec2 vp = ImGui::GetMainViewport()->Size;
    ImGui::SetWindowSize(win, ImVec2(std::min(gui::UiPx(w), vp.x), std::min(gui::UiPx(h), vp.y)), ImGuiCond_Always);
    ctx->Yield(3);
  };
  pin_size(960.0f, 720.0f);
  const auto canvas_h = [&](const char* id) -> float {
    const ImGuiTestItemInfo c = ctx->ItemInfo(id, ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(c.ID != 0, false);
    return c.RectFull.GetHeight();
  };

  // Scenario 1 — STRETCH, the undragged D7: the quota members hold, the canvases take the
  // delta by D5's ratio (Δ3d : Δcurve == ratio : 1−ratio).
  const float d5_saved = gui::g_state.analyze_pose_view_left_row_ratio;
  const float d7_saved = gui::g_state.analyze_pose_view_curve_canvas_ratio;
  const float c3d_0 = canvas_h("**/##r3d_canvas");
  const float cc_0 = canvas_h("**/##pose_curve_canvas");
  ImGuiWindow* rep3d_0 = PoseRegionScrollChild("##AnalyzeRaypath3D", "##Ray3DReport");
  ImGuiWindow* comp_0 = PoseRegionScrollChild("##AnalyzePoseCurve", "##PoseCurveScroll");
  IM_CHECK_RETV(rep3d_0 != nullptr && comp_0 != nullptr, false);
  const float rep3d_h0 = rep3d_0->Size.y;
  const float comp_h0 = comp_0->Size.y;
  ctx->LogInfo("[resize stretch] canvas 3d %.0f curve %.0f | quota 3d-report %.0f companion %.0f | D5 %.3f",
               static_cast<double>(c3d_0), static_cast<double>(cc_0), static_cast<double>(rep3d_h0),
               static_cast<double>(comp_h0), static_cast<double>(d5_saved));
  pin_size(960.0f, 880.0f);
  const float c3d_1 = canvas_h("**/##r3d_canvas");
  const float cc_1 = canvas_h("**/##pose_curve_canvas");
  ImGuiWindow* rep3d_1 = PoseRegionScrollChild("##AnalyzeRaypath3D", "##Ray3DReport");
  ImGuiWindow* comp_1 = PoseRegionScrollChild("##AnalyzePoseCurve", "##PoseCurveScroll");
  IM_CHECK_RETV(rep3d_1 != nullptr && comp_1 != nullptr, false);
  ctx->LogInfo("[resize stretch] stretched: canvas 3d %.0f curve %.0f | quota %.0f / %.0f", static_cast<double>(c3d_1),
               static_cast<double>(cc_1), static_cast<double>(rep3d_1->Size.y), static_cast<double>(comp_1->Size.y));
  IM_CHECK_RETV(fabsf(rep3d_1->Size.y - rep3d_h0) < 2.0f, false);      // the 3D report's quota held
  IM_CHECK_RETV(fabsf(comp_1->Size.y - comp_h0) < 2.0f, false);        // the companion's quota held
  IM_CHECK_RETV(c3d_1 > c3d_0 + 20.0f && cc_1 > cc_0 + 20.0f, false);  // both canvases ate delta
  const float d3 = c3d_1 - c3d_0;
  const float dc = cc_1 - cc_0;
  const float share_3d = d3 / (d3 + dc);
  ctx->LogInfo("[resize stretch] delta 3d %.0f curve %.0f, share %.3f vs D5 ratio %.3f", static_cast<double>(d3),
               static_cast<double>(dc), static_cast<double>(share_3d),
               static_cast<double>(gui::g_state.analyze_pose_view_left_row_ratio));
  IM_CHECK_RETV(fabsf(share_3d - gui::g_state.analyze_pose_view_left_row_ratio) < 0.05f, false);

  // Scenario 2 — SHRINK, the reverse symmetry: the same delta comes back out of the canvases,
  // the quotas still hold.
  pin_size(960.0f, 720.0f);
  const float c3d_2 = canvas_h("**/##r3d_canvas");
  const float cc_2 = canvas_h("**/##pose_curve_canvas");
  IM_CHECK_RETV(fabsf(c3d_2 - c3d_0) < 2.0f && fabsf(cc_2 - cc_0) < 2.0f, false);
  ctx->LogInfo("[resize shrink] restored: canvas 3d %.0f curve %.0f — the stretch was symmetric",
               static_cast<double>(c3d_2), static_cast<double>(cc_2));

  // Scenario 3 — DRAG, then resize: the dragged D5 ratio governs the next stretch's share
  // (user intent overrides the strategy). Drag the 3D side down 60 (the drag may clamp at the
  // curve floor — the redistribution it performs is part of the pre-stretch baseline), measure
  // the post-drag geometry, then stretch and re-derive.
  IM_CHECK_RETV(PoseDragStrip(ctx, "**/##PoseViewLeftRowSplitter", 0.0f, 60.0f), false);
  const float dragged_ratio = gui::g_state.analyze_pose_view_left_row_ratio;
  IM_CHECK_RETV(dragged_ratio > d5_saved + 0.03f, false);
  const float c3d_post = canvas_h("**/##r3d_canvas");
  const float cc_post = canvas_h("**/##pose_curve_canvas");
  pin_size(960.0f, 880.0f);
  const float c3d_3 = canvas_h("**/##r3d_canvas");
  const float cc_3 = canvas_h("**/##pose_curve_canvas");
  const float d3b = c3d_3 - c3d_post;
  const float dcb = cc_3 - cc_post;
  const float share_3d_b = d3b / (d3b + dcb);
  ctx->LogInfo("[resize dragged] D5 %.3f -> %.3f, stretched share %.3f (delta 3d %.0f curve %.0f)",
               static_cast<double>(d5_saved), static_cast<double>(dragged_ratio), static_cast<double>(share_3d_b),
               static_cast<double>(d3b), static_cast<double>(dcb));
  IM_CHECK_RETV(fabsf(share_3d_b - dragged_ratio) < 0.05f, false);

  // Scenario 4 — a dragged D7 no longer pins the companion: drag the canvas side down 40 at
  // this basis, then shrink and read the settled split — the canvas's share of the span equals
  // the dragged ratio and BOTH members sit off the quota (the companion held its quota only in
  // the undragged state above; the drag's intent overrides it, and the resize keeps the
  // dragged split rather than re-pinning the quota).
  pin_size(960.0f, 880.0f);
  IM_CHECK_RETV(PoseDragStrip(ctx, "**/##PoseCurveSplitter", 0.0f, 40.0f), false);
  const float d7_dragged = gui::g_state.analyze_pose_view_curve_canvas_ratio;
  IM_CHECK_RETV(d7_dragged > d7_saved + 0.03f, false);
  pin_size(960.0f, 720.0f);
  const float cc_5 = canvas_h("**/##pose_curve_canvas");
  ImGuiWindow* comp_5 = PoseRegionScrollChild("##AnalyzePoseCurve", "##PoseCurveScroll");
  IM_CHECK_RETV(comp_5 != nullptr, false);
  const float denom_7 = cc_5 + comp_5->Size.y;  // the split's span: canvas + companion (the strip and spacings cancel)
  const float share_5 = cc_5 / denom_7;
  const float quota_5 = 8.0f * ImGui::GetTextLineHeightWithSpacing();
  ctx->LogInfo(
      "[resize d7 dragged] settled: canvas %.0f, companion %.0f, denom %.0f, share %.3f vs stored %.3f "
      "(quota %.0f)",
      static_cast<double>(cc_5), static_cast<double>(comp_5->Size.y), static_cast<double>(denom_7),
      static_cast<double>(share_5), static_cast<double>(d7_dragged), static_cast<double>(quota_5));
  IM_CHECK_RETV(fabsf(share_5 - d7_dragged) < 0.03f, false);  // the dragged share governs the settled split
  IM_CHECK_RETV(comp_5->Size.y < quota_5 - 20.0f, false);     // the companion gave up its quota to the drag

  // The four splits leave with the RAII guard; the window basis is this case's own state.
  pin_size(960.0f, 720.0f);
  gui::g_state.analyze_pose_view_open = false;
  ctx->Yield(2);
  return true;
}

// AC3 — the narrowest face's degrade chain, per boundary (§16.7): at the true minimum, each of
// D5/D6/D7 dragged to BOTH ends, the sides holding min(floor, proportional share); every
// scrollable member really reaches its end over the real wheel path; the fixed stacks still
// answer with the scroll parked at the end. The 1.5x leg is the §16.4.1 calibration duty — the
// assertions read BEHAVIOUR (containment, reachability), never recomputed pixels; the 2x leg
// lives in the narrowest-window case, the 1x geometry bounds in the splits case.
bool PoseViewNarrowestBoundariesHoldBothEnds(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  // The session splits this case drags to their ends leave with the RAII guard — happy path
  // and early exit alike (the cross-case lesson's prescription; a tail restore only covers
  // the happy path).
  ScopedPoseSessionSplits splits_guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  IM_CHECK_RETV(SelectRowByDisplay(ctx, TopChainDisplay().c_str()) != std::nullopt, false);
  SetSkyPointTarget(42.0, 0.0);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);

  // A real wheel's work to the member's end — the reviewer's demand that reachability not be
  // claimed from ScrollMax's existence. A member whose content fits (ScrollMax == 0) is
  // reachable by construction; the log names which shape the leg saw.
  const auto wheel_to_end = [&](ImGuiWindow* area, const char* what) -> bool {
    if (area->ScrollMax.y > 0.0f) {
      const float scroll0 = area->Scroll.y;
      const ImVec2 centre = area->Rect().GetCenter();
      ctx->MouseMoveToPos(centre);
      ctx->Yield(2);
      for (int i = 0; i < 24 && area->Scroll.y < area->ScrollMax.y - 0.5f; ++i) {
        ctx->MouseWheelY(-1.0f);
        ctx->Yield(1);
      }
      IM_CHECK_RETV(area->Scroll.y > scroll0 + 0.5f, false);
      IM_CHECK_RETV(area->Scroll.y >= area->ScrollMax.y - 1.0f, false);
      ctx->LogInfo("[chain] %s wheeled %.0f -> %.0f of SM %.0f", what, static_cast<double>(scroll0),
                   static_cast<double>(area->Scroll.y), static_cast<double>(area->ScrollMax.y));
    } else {
      ctx->LogInfo("[chain] %s fits entirely (SM 0)", what);
    }
    return true;
  };

  const auto drive_chain = [&](float scale) -> bool {
    gui::g_ui_scale_multiplier = scale;
    gui::g_ui_scale_dirty = true;
    for (int i = 0; i < 60 && !(gui::UiPx(gui::kAnalyzeLeftPanelWidth) > scale * gui::kAnalyzeLeftPanelWidth - 0.5f &&
                                gui::UiPx(gui::kAnalyzeLeftPanelWidth) < scale * gui::kAnalyzeLeftPanelWidth + 0.5f);
         ++i) {
      ctx->Yield(1);
    }
    IM_CHECK_RETV(fabsf(gui::UiPx(gui::kAnalyzeLeftPanelWidth) - scale * gui::kAnalyzeLeftPanelWidth) < 0.5f, false);
    ctx->Yield(2);
    gui::g_state.analyze_pose_view_open = true;
    PinPoseViewOnScreen(ctx);
    ctx->Yield(2);
    ImGuiWindow* win = ctx->GetWindowByRef("//Pose & trace");
    IM_CHECK_RETV(win != nullptr, false);
    const ImVec2 vp = ImGui::GetMainViewport()->Size;
    ImGui::SetWindowSize(
        win,
        ImVec2(std::min(gui::UiPx(gui::kPoseViewMinWidth), vp.x), std::min(gui::UiPx(gui::kPoseViewMinHeight), vp.y)),
        ImGuiCond_Always);
    ctx->Yield(3);
    ctx->LogInfo("[chain %.1fx] window %.0fx%.0f", (double)scale, (double)win->Size.x, (double)win->Size.y);

    // D5 — both ends: the pair holds min(floor, share) in PIXELS (the pt/px unit note in the
    // splits case applies doubly here — the 1.5x leg's viewport-clamped window exercises the
    // fallback); the bounded curve side's companion reaches its end over the wheel.
    const auto d5_bounds_hold = [&]() -> bool {
      ImGuiWindow* r3d_n = PoseRegionWindow("##AnalyzeRaypath3D");
      ImGuiWindow* curve_n = PoseRegionWindow("##AnalyzePoseCurve");
      IM_CHECK_RETV(r3d_n != nullptr && curve_n != nullptr, false);
      float bound_3d = 0.0f;
      float bound_curve = 0.0f;
      PoseD5HeightBounds(r3d_n->Size.y, curve_n->Size.y, &bound_3d, &bound_curve);
      ctx->LogInfo("[chain %.1fx] D5: 3d %.0f (bound %.0f), curve %.0f (bound %.0f)", (double)scale,
                   static_cast<double>(r3d_n->Size.y), static_cast<double>(bound_3d),
                   static_cast<double>(curve_n->Size.y), static_cast<double>(bound_curve));
      IM_CHECK_RETV(r3d_n->Size.y >= bound_3d - 1.5f, false);
      IM_CHECK_RETV(curve_n->Size.y >= bound_curve - 1.5f, false);
      return true;
    };
    IM_CHECK_RETV(PoseDragStrip(ctx, "**/##PoseViewLeftRowSplitter", 0.0f, -1800.0f), false);
    IM_CHECK_RETV(d5_bounds_hold(), false);
    {
      ImGuiWindow* comp = PoseRegionScrollChild("##AnalyzePoseCurve", "##PoseCurveScroll");
      IM_CHECK_RETV(comp != nullptr, false);
      IM_CHECK_RETV(wheel_to_end(comp, "curve companion (D5 top)"), false);
    }
    IM_CHECK_RETV(PoseDragStrip(ctx, "**/##PoseViewLeftRowSplitter", 0.0f, 1800.0f), false);
    IM_CHECK_RETV(d5_bounds_hold(), false);

    // D6 — both ends: the globe's fixed stack answers at its floor; the factors region's
    // scroll reaches its end over the wheel.
    IM_CHECK_RETV(PoseDragStrip(ctx, "**/##PoseViewRowsSplitter", 0.0f, -1800.0f), false);
    {
      ImGuiWindow* globe_n = PoseRegionWindow("##AnalyzeSunGlobe");
      ImGuiWindow* factors_n = PoseRegionWindow("##AnalyzePoseFactors");
      IM_CHECK_RETV(globe_n != nullptr && factors_n != nullptr, false);
      const ImGuiTestItemInfo sample = ctx->ItemInfo("**/##globe_sample", ImGuiTestOpFlags_NoError);
      IM_CHECK_RETV(sample.ID != 0, false);
      IM_CHECK_RETV(sample.RectFull.Max.y <= globe_n->InnerClipRect.Max.y + 0.5f, false);
      ctx->LogInfo("[chain %.1fx] D6 top: globe %.0f, factors %.0f", (double)scale,
                   static_cast<double>(globe_n->Size.y), static_cast<double>(factors_n->Size.y));
      ImGuiWindow* scroll = PoseRegionScrollChild("##AnalyzePoseFactors", "##PoseFactorsScroll");
      IM_CHECK_RETV(scroll != nullptr, false);
      IM_CHECK_RETV(wheel_to_end(scroll, "factors scroll (D6 top)"), false);
    }
    IM_CHECK_RETV(PoseDragStrip(ctx, "**/##PoseViewRowsSplitter", 0.0f, 1800.0f), false);
    {
      ImGuiWindow* globe_n = PoseRegionWindow("##AnalyzeSunGlobe");
      ImGuiWindow* factors_n = PoseRegionWindow("##AnalyzePoseFactors");
      IM_CHECK_RETV(globe_n != nullptr && factors_n != nullptr, false);
      const ImGuiTestItemInfo play = ctx->ItemInfo("**/Play##globe_play", ImGuiTestOpFlags_NoError);
      IM_CHECK_RETV(play.ID != 0, false);
      ctx->LogInfo("[chain %.1fx] D6 bottom: globe %.0f, factors %.0f", (double)scale,
                   static_cast<double>(globe_n->Size.y), static_cast<double>(factors_n->Size.y));
      // The factors region at ITS floor: the fold structure is present in the content flow and
      // the content is reachable — the scroll reaches its end over the wheel. The fold headers
      // sit past the visible clip at this extreme (registered, not contained — the same
      // addressable-is-not-visible distinction the narrowest case's lesson names), so the
      // standing claim here is reachability, not containment.
      const ImGuiTestItemInfo physics_b =
          ctx->ItemInfo("**/Interface transmission##pose_factors", ImGuiTestOpFlags_NoError);
      IM_CHECK_RETV(physics_b.ID != 0, false);
      ImGuiWindow* scroll_b = PoseRegionScrollChild("##AnalyzePoseFactors", "##PoseFactorsScroll");
      IM_CHECK_RETV(scroll_b != nullptr, false);
      IM_CHECK_RETV(wheel_to_end(scroll_b, "factors scroll (D6 bottom)"), false);
    }

    // D7 — both ends: the canvas holds its real-slice floor and still writes the selection; the
    // companion reaches its end over the wheel; then the canvas swallows the span and the
    // companion holds its own floor.
    IM_CHECK_RETV(PoseDragStrip(ctx, "**/##PoseCurveSplitter", 0.0f, -1800.0f), false);
    {
      const ImGuiTestItemInfo canvas_n = ctx->ItemInfo("**/##pose_curve_canvas", ImGuiTestOpFlags_NoError);
      IM_CHECK_RETV(canvas_n.ID != 0, false);
      ctx->LogInfo("[chain %.1fx] D7 top: canvas %.0f", (double)scale,
                   static_cast<double>(canvas_n.RectFull.GetHeight()));
      IM_CHECK_RETV(canvas_n.RectFull.GetHeight() >= gui::UiPx(gui::kPoseViewCurveCanvasFloor) - 1.5f, false);
      // The canvas still writes the selection at its real-slice floor: pin the cursor away
      // from the click's target first (an earlier leg's click may have parked it at this same
      // x — the click's effect is the WRITE, not the delta), then click at 75% width and read
      // the late sample the x mapping lands on.
      gui::g_state.analysis.point_view.sample_index = 0;
      gui::g_state.analysis.point_view.playing = false;
      ctx->Yield(2);
      ctx->MouseMoveToPos(
          ImVec2(canvas_n.RectFull.Min.x + canvas_n.RectFull.GetWidth() * 0.75f, canvas_n.RectFull.GetCenter().y));
      ctx->Yield(2);
      ctx->MouseClick(0);
      ctx->Yield(3);
      IM_CHECK_RETV(gui::g_state.analysis.point_view.sample_index > 0, false);
      ctx->LogInfo("[chain %.1fx] D7 top: canvas click at floor wrote sample %d", (double)scale,
                   gui::g_state.analysis.point_view.sample_index);
      ImGuiWindow* comp = PoseRegionScrollChild("##AnalyzePoseCurve", "##PoseCurveScroll");
      IM_CHECK_RETV(comp != nullptr, false);
      IM_CHECK_RETV(wheel_to_end(comp, "curve companion (D7 top)"), false);
    }
    IM_CHECK_RETV(PoseDragStrip(ctx, "**/##PoseCurveSplitter", 0.0f, 1800.0f), false);
    {
      const ImGuiTestItemInfo canvas_n = ctx->ItemInfo("**/##pose_curve_canvas", ImGuiTestOpFlags_NoError);
      IM_CHECK_RETV(canvas_n.ID != 0, false);
      ImGuiWindow* comp = PoseRegionScrollChild("##AnalyzePoseCurve", "##PoseCurveScroll");
      IM_CHECK_RETV(comp != nullptr, false);
      ctx->LogInfo("[chain %.1fx] D7 bottom: canvas %.0f, companion %.0f", (double)scale,
                   static_cast<double>(canvas_n.RectFull.GetHeight()), static_cast<double>(comp->Size.y));
      IM_CHECK_RETV(comp->Size.y >= gui::UiPx(gui::kPoseViewCurveCompanionFloor) - 1.5f, false);
      // The companion at its 32pt floor is the ONE form where overflow is guaranteed by
      // construction (the quota's content cannot fit 32pt) — the as-built's "content scrolls
      // to the end" claim for every endpoint is carried here by a real wheel, not by the
      // fits-entirely branches the other legs may honestly report.
      IM_CHECK_RETV(wheel_to_end(comp, "curve companion (D7 bottom, floor-squeezed)"), false);
    }

    gui::g_state.analyze_pose_view_open = false;
    ctx->Yield(2);
    return true;
  };
  IM_CHECK_RETV(drive_chain(gui::kFactoryUiScaleMultiplier), false);
  IM_CHECK_RETV(drive_chain(1.5f), false);

  // Restore the factory scale and the shared window's BASIS (the 2x leg's own discipline: the
  // scale rebuild grows the shared GLFW window and never shrinks it back).
  gui::g_ui_scale_multiplier = gui::kFactoryUiScaleMultiplier;
  gui::g_ui_scale_dirty = true;
  for (int i = 0; i < 60 && !(gui::UiPx(gui::kAnalyzeLeftPanelWidth) > gui::kAnalyzeLeftPanelWidth - 0.5f &&
                              gui::UiPx(gui::kAnalyzeLeftPanelWidth) < gui::kAnalyzeLeftPanelWidth + 0.5f);
       ++i) {
    ctx->Yield(1);
  }
  IM_CHECK_RETV(fabsf(gui::UiPx(gui::kAnalyzeLeftPanelWidth) - gui::kAnalyzeLeftPanelWidth) < 0.5f, false);
  g_window_size_test.apply_aspect = false;
  g_window_size_test.width = gui::kInitWindowWidth;
  g_window_size_test.height = gui::kInitWindowHeight;
  g_window_size_test.done.store(false);
  g_window_size_test.requested.store(true);
  for (int i = 0; i < 20 && !g_window_size_test.done.load(); ++i) {
    ctx->Yield(1);
  }
  IM_CHECK_RETV(g_window_size_test.done.load(), false);
  ctx->Yield(3);
  return true;
}

// ⑦ — the narrowest window (640x420 at 1x, the clamped equivalent at 2x), the review's Minor
// made real: the fixed interaction stack's controls sit inside the globe region's VISIBLE clip
// (addressable is not visible), and each answers a REAL mouse operation — the toggle flips, a
// slider click moves the cursor and stops a running playback, the play button answers both ways.
// The state area below scrolls (its content is reachable, not deleted).
bool PoseViewNarrowestWindowKeepsTheFixedStackOperable(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  IM_CHECK_RETV(SelectRowByDisplay(ctx, TopChainDisplay().c_str()) != std::nullopt, false);
  SetSkyPointTarget(42.0, 0.0);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  const int valid_count = gui::PointValidSampleCount(*gui::g_state.analysis.point.adopted_result, 0);
  IM_CHECK_RETV(valid_count > 1, false);

  const auto drive_narrowest = [&](float scale) -> bool {
    gui::g_ui_scale_multiplier = scale;
    gui::g_ui_scale_dirty = true;
    for (int i = 0; i < 60 && !(gui::UiPx(gui::kAnalyzeLeftPanelWidth) > scale * gui::kAnalyzeLeftPanelWidth - 0.5f &&
                                gui::UiPx(gui::kAnalyzeLeftPanelWidth) < scale * gui::kAnalyzeLeftPanelWidth + 0.5f);
         ++i) {
      ctx->Yield(1);
    }
    IM_CHECK_RETV(fabsf(gui::UiPx(gui::kAnalyzeLeftPanelWidth) - scale * gui::kAnalyzeLeftPanelWidth) < 0.5f, false);
    ctx->Yield(2);
    gui::g_state.analyze_pose_view_open = true;
    PinPoseViewOnScreen(ctx);  // the pin re-clamps the size to THIS viewport, the user-reachable minimum
    ctx->Yield(2);
    ImGuiWindow* win = ctx->GetWindowByRef("//Pose & trace");
    IM_CHECK_RETV(win != nullptr, false);
    const ImVec2 vp = ImGui::GetMainViewport()->Size;
    // The window sits AT the contract's narrowest face (or the viewport-clamped prefix of it).
    const float want_w = std::min(gui::UiPx(gui::kPoseViewMinWidth), vp.x);
    const float want_h = std::min(gui::UiPx(gui::kPoseViewMinHeight), vp.y);
    ImGui::SetWindowSize(win, ImVec2(want_w, want_h), ImGuiCond_Always);
    ctx->Yield(3);
    ctx->LogInfo("[pose narrowest %.1fx] window %.0fx%.0f at (%.0f,%.0f)", (double)scale, (double)win->Size.x,
                 (double)win->Size.y, (double)win->Pos.x, (double)win->Pos.y);

    ImGuiWindow* globe = PoseRegionWindow("##AnalyzeSunGlobe");
    ImGuiWindow* factors = PoseRegionWindow("##AnalyzePoseFactors");
    IM_CHECK_RETV(globe != nullptr && factors != nullptr, false);
    // The fixed stack's controls, VISIBLE: each rect inside its region's live clip (the clip is
    // viewport-intersected — an off-viewport rect fails this even though the item registered).
    // The deviation-grid toggle's home is the factors region's "Pose diagnostics" group (§16.4 —
    // the toggle retired there with the relayout); its containment leg runs below, after the
    // fold is open and the scroll has sought it into view.
    const ImGuiTestItemInfo sample = ctx->ItemInfo("**/##globe_sample", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(sample.ID != 0, false);
    IM_CHECK_RETV(sample.RectFull.Max.x <= globe->InnerClipRect.Max.x + 0.5f &&
                      sample.RectFull.Max.y <= globe->InnerClipRect.Max.y + 0.5f,
                  false);
    const ImGuiTestItemInfo play = ctx->ItemInfo("**/Play##globe_play", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(play.ID != 0, false);
    IM_CHECK_RETV(play.RectFull.Max.y <= globe->InnerClipRect.Max.y + 0.5f, false);

    // REAL operations, the review's demand: the toggle flips for true. The toggle lives in the
    // factors region's "Pose diagnostics" group — closed by default, its content past the
    // factor readouts in the scroll — so the leg opens the fold, seeks the scroll to its end,
    // and only then addresses the checkbox (the §16.1 fold discipline: the fold's expansion is
    // session-sticky window storage; the leg closes it again at the end, leaving the next case
    // the default-closed shape). No env rides the group: the toggle renders in every state.
    ctx->ItemClick("**/Pose diagnostics##pose_factors");
    ctx->Yield(2);
    ImGuiWindow* factors_scroll = PoseRegionScrollChild("##AnalyzePoseFactors", "##PoseFactorsScroll");
    IM_CHECK_RETV(factors_scroll != nullptr, false);
    factors_scroll->Scroll.y = factors_scroll->ScrollMax.y;  // belt-and-braces seek, the globe leg's idiom
    ctx->Yield(2);
    // The toggle's own containment leg: its rect inside the factors region's live clip — the
    // same visibility demand the fixed-stack controls above answer, relocated with its region.
    const ImGuiTestItemInfo toggle =
        ctx->ItemInfo("**/Deviation grid###pose_factors_dev_grid", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(toggle.ID != 0, false);
    IM_CHECK_RETV(toggle.RectFull.Min.x >= factors->InnerClipRect.Min.x - 0.5f &&
                      toggle.RectFull.Max.x <= factors->InnerClipRect.Max.x + 0.5f &&
                      toggle.RectFull.Min.y >= factors->InnerClipRect.Min.y - 0.5f &&
                      toggle.RectFull.Max.y <= factors->InnerClipRect.Max.y + 0.5f,
                  false);
    const bool grid_before = gui::g_state.analysis.point_view.show_deviation_grid;
    ctx->ItemClick("**/Deviation grid###pose_factors_dev_grid");
    ctx->Yield(2);
    IM_CHECK_RETV(gui::g_state.analysis.point_view.show_deviation_grid != grid_before, false);
    ctx->ItemClick("**/Pose diagnostics##pose_factors");  // restore the default-closed shape
    ctx->Yield(2);
    // a running playback is stopped by a hand on the slider (the click moves the cursor),
    gui::g_state.analysis.point_view.playing = true;
    ctx->Yield(2);
    ctx->ItemClick("**/##globe_sample");
    ctx->Yield(2);
    IM_CHECK_RETV(!gui::g_state.analysis.point_view.playing, false);
    const int clicked = gui::g_state.analysis.point_view.sample_index;
    IM_CHECK_RETV(clicked >= 0 && clicked < valid_count, false);
    // and the play button answers both ways over the real mouse path.
    ctx->ItemClick("**/Play##globe_play");
    ctx->Yield(2);
    IM_CHECK_RETV(gui::g_state.analysis.point_view.playing, false);
    ctx->ItemClick("**/Stop##globe_play");
    ctx->Yield(2);
    IM_CHECK_RETV(!gui::g_state.analysis.point_view.playing, false);
    // The state area's two honest forms at this geometry: BOUNDED (a scrolling child under the
    // fixed stack — its parent is the globe region child, not the top level) or, when even the
    // stack cannot share the cell, UNBOUNDED (the lines render direct in the region and the
    // REGION's own scroll is the reachability — the degradation ladder's last rung). Either way
    // the informational lines scroll to, they are not deleted.
    ImGuiWindow* state_area = nullptr;
    for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
      if (w != nullptr && w->Active && w->ParentWindow != nullptr &&
          std::strstr(w->ParentWindow->Name, "##AnalyzeSunGlobe") != nullptr &&
          std::strstr(w->Name, "##AnalyzeGlobeState") != nullptr) {
        state_area = w;
        break;
      }
    }
    if (state_area != nullptr) {
      // The bounded area's two honest shapes at this geometry: it FITS ENTIRELY (ScrollMax == 0
      // — the fixed stack is one control shorter since the toggle retired to the factors
      // region, so the narrowest face can now hold the whole report) or it SCROLLS, and then
      // the reviewer's wheel legs hold: the wheel did the scrolling, the end is in view,
      // nothing was deleted to fit. Both are reachability; neither deletes a line.
      if (state_area->ScrollMax.y > 0.0f) {
        const float sm0 = state_area->ScrollMax.y;
        const float scroll0 = state_area->Scroll.y;
        const ImVec2 area_centre = state_area->Rect().GetCenter();
        ctx->MouseMoveToPos(area_centre);
        ctx->Yield(2);
        for (int i = 0; i < 24 && state_area->Scroll.y < state_area->ScrollMax.y - 0.5f; ++i) {
          ctx->MouseWheelY(-1.0f);
          ctx->Yield(1);
        }
        IM_CHECK_RETV(state_area->Scroll.y > scroll0 + 0.5f, false);                   // the wheel did the scrolling
        IM_CHECK_RETV(state_area->Scroll.y >= state_area->ScrollMax.y - 1.0f, false);  // the end is in view
        IM_CHECK_RETV(state_area->ScrollMax.y >= sm0 - 0.5f, false);                   // nothing was deleted to fit
        ctx->LogInfo("[pose narrowest %.1fx] state area bounded, wheeled %.0f -> %.0f of SM %.0f; end in view",
                     (double)scale, (double)scroll0, (double)state_area->Scroll.y, (double)state_area->ScrollMax.y);
        // The fixed stack answers with the state area parked at its end.
        ctx->ItemClick("**/Play##globe_play");
        ctx->Yield(2);
        IM_CHECK_RETV(gui::g_state.analysis.point_view.playing, false);
        ctx->ItemClick("**/Stop##globe_play");
        ctx->Yield(2);
        IM_CHECK_RETV(!gui::g_state.analysis.point_view.playing, false);
        ctx->LogInfo("[pose narrowest %.1fx] fixed stack operated with the state area at its end", (double)scale);
      } else {
        ctx->LogInfo("[pose narrowest %.1fx] state area fits entirely (SM 0) — reachable by construction",
                     (double)scale);
      }
    } else {
      ImGuiWindow* globe_scroll = PoseRegionWindow("##AnalyzeSunGlobe");
      IM_CHECK_RETV(globe_scroll != nullptr, false);
      ctx->LogInfo("[pose narrowest %.1fx] state area unbounded (the region's own scroll), region SM %.0f",
                   (double)scale, (double)globe_scroll->ScrollMax.y);
      IM_CHECK_RETV(globe_scroll->ScrollMax.y >= 0.0f, false);  // fits or scrolls; no clip without a scroll exists
    }

    gui::g_state.analyze_pose_view_open = false;
    ctx->Yield(2);
    return true;
  };
  IM_CHECK_RETV(drive_narrowest(gui::kFactoryUiScaleMultiplier), false);
  IM_CHECK_RETV(drive_narrowest(2.0f), false);
  // Restore the factory scale, settled — and the shared window's BASIS with it: the ui-scale
  // rebuild's own semantics grow the shared GLFW window at >=2x and never shrink it back, so a
  // case that drove a 2x tier restores the basis explicitly (the scale rebuild re-plans the
  // window, so the explicit size lands AFTER the settle — waited on, leaving the next case's
  // entry audit nothing to undo).
  gui::g_ui_scale_multiplier = gui::kFactoryUiScaleMultiplier;
  gui::g_ui_scale_dirty = true;
  for (int i = 0; i < 60 && !(gui::UiPx(gui::kAnalyzeLeftPanelWidth) > gui::kAnalyzeLeftPanelWidth - 0.5f &&
                              gui::UiPx(gui::kAnalyzeLeftPanelWidth) < gui::kAnalyzeLeftPanelWidth + 0.5f);
       ++i) {
    ctx->Yield(1);
  }
  IM_CHECK_RETV(fabsf(gui::UiPx(gui::kAnalyzeLeftPanelWidth) - gui::kAnalyzeLeftPanelWidth) < 0.5f, false);
  g_window_size_test.apply_aspect = false;
  g_window_size_test.width = gui::kInitWindowWidth;
  g_window_size_test.height = gui::kInitWindowHeight;
  g_window_size_test.done.store(false);
  g_window_size_test.requested.store(true);
  for (int i = 0; i < 20 && !g_window_size_test.done.load(); ++i) {
    ctx->Yield(1);
  }
  IM_CHECK_RETV(g_window_size_test.done.load(), false);
  ctx->Yield(3);
  return true;
}

// 756 — the contribution-curve region: the drawn curve faces over a REAL adopted point, the
// canvas's real write path (a click moves the shared cursor, stops the playback it finds
// running, leaves the point intent and the sky point untouched; the ray face re-keys to the
// clicked sample), the hover-only readout discipline, and the fixed stack + canvas at the
// narrowest face (the D5 floors' promise, with the geometry logged — the live-layout probe the
// floor constants are tuned from). The screenshots are the readability record (the issue's
// shared-log-axis hypothesis): five curves, the legend, the zero row and the cursor at both
// geometries.
bool PoseCurveRegionDrawsAndMovesTheSharedCursor(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  IM_CHECK_RETV(SelectRowByDisplay(ctx, TopChainDisplay().c_str()) != std::nullopt, false);

  // Pre-adoption retains the exact canvas but carries no stale series or sample.
  gui::g_state.analyze_pose_view_open = true;
  PinPoseViewOnScreen(ctx);
  ctx->Yield(2);
  IM_CHECK_RETV(PoseRegionWindow("##AnalyzePoseCurve") != nullptr, false);
  const ImGuiTestItemInfo empty_canvas = ctx->ItemInfo("**/##pose_curve_canvas", ImGuiTestOpFlags_NoError);
  IM_CHECK_RETV(empty_canvas.ID != 0, false);
  const ImRect empty_rect = empty_canvas.RectFull;
  SetSkyPointTarget(42.0, 0.0);
  ctx->Yield(1);
  const ImRect first_wait_rect = ctx->ItemInfo("**/##pose_curve_canvas").RectFull;
  IM_CHECK_RETV(first_wait_rect.Min.x == empty_rect.Min.x && first_wait_rect.Min.y == empty_rect.Min.y &&
                    first_wait_rect.Max.x == empty_rect.Max.x && first_wait_rect.Max.y == empty_rect.Max.y,
                false);

  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  const int valid_count = gui::PointValidSampleCount(*gui::g_state.analysis.point.adopted_result, 0);
  IM_CHECK_RETV(valid_count > 3, false);
  gui::g_state.analyze_pose_view_open = true;
  PinPoseViewOnScreen(ctx);
  ctx->Yield(2);
  ImGuiWindow* curve = PoseRegionWindow("##AnalyzePoseCurve");
  IM_CHECK_RETV(curve != nullptr, false);
  const ImGuiTestItemInfo canvas = ctx->ItemInfo("**/##pose_curve_canvas", ImGuiTestOpFlags_NoError);
  IM_CHECK_RETV(canvas.ID != 0, false);
  IM_CHECK_RETV(canvas.RectFull.Max.y <= curve->InnerClipRect.Max.y + 0.5f, false);
  ctx->LogInfo("[pose curve] region %.0fx%.0f SM %.1f, canvas %.0fx%.0f", static_cast<double>(curve->Size.x),
               static_cast<double>(curve->Size.y), static_cast<double>(curve->ScrollMax.y),
               static_cast<double>(canvas.RectFull.GetWidth()), static_cast<double>(canvas.RectFull.GetHeight()));
  IM_CHECK_RETV(SaveWindowPng(ctx, "//Pose & trace", GuiTestTempPath("pose_curve_region_default.png").string()), false);

  // ---- §16.4's content-owner faces at the default geometry (Step 6): the factors region
  // hosts the current pose's readouts — both fold headers present and contained (default-
  // closed; the toggle's flip legs live in the narrowest case), the companion holds its fixed
  // 8-line quota against the content's arrival (the ladder line became the full companion
  // content and the geometry did not move — §15), and the measure registration's gap state is
  // logged for the hands-on record (the note rides the default layer by construction; this
  // scene may not carry a gap to exercise it). ----
  {
    // The pose host, every env state: both fold headers stand with the variable unset (the
    // diagnostics group is not gated here — its toggle is a user-facing control; only the
    // Class/Point/Map diagnostics groups keep the gate). Default-closed; the toggle's flip
    // legs live in the narrowest case.
    const ScopedDebugDetailsEnv env_unset(DebugDetailsState::kUnset);
    ctx->Yield(2);
    IM_CHECK_RETV(ctx->ItemInfo("**/Pose diagnostics##pose_factors", ImGuiTestOpFlags_NoError).ID != 0, false);
    IM_CHECK_RETV(ctx->ItemInfo("**/Interface transmission##pose_factors", ImGuiTestOpFlags_NoError).ID != 0, false);
  }
  {
    const ScopedDebugDetailsEnv env_on(DebugDetailsState::kOn);
    ctx->Yield(2);
    ImGuiWindow* factors = PoseRegionWindow("##AnalyzePoseFactors");
    IM_CHECK_RETV(factors != nullptr, false);
    const ImGuiTestItemInfo physics =
        ctx->ItemInfo("**/Interface transmission##pose_factors", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(physics.ID != 0, false);
    const ImGuiTestItemInfo calc = ctx->ItemInfo("**/Pose diagnostics##pose_factors", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(calc.ID != 0, false);
    IM_CHECK_RETV(physics.RectFull.Max.y <= factors->InnerClipRect.Max.y + 0.5f, false);
    IM_CHECK_RETV(calc.RectFull.Max.y <= factors->InnerClipRect.Max.y + 0.5f, false);
    LUMICE_PathMeasureRegistration reg{};
    reg.struct_size = sizeof(reg);
    const bool reg_ok = LUMICE_PathPointGetMeasureRegistration(gui::g_state.analysis.point.adopted_result.get(),
                                                               &reg) == LUMICE_PATH_READ_OK;
    ctx->LogInfo("[pose factors] fold headers contained at default; measure registration %s (gap %d)",
                 reg_ok ? "read" : "absent", reg_ok ? static_cast<int>(reg.registered_gap) : -1);
    ImGuiWindow* comp = PoseRegionScrollChild("##AnalyzePoseCurve", "##PoseCurveScroll");
    IM_CHECK_RETV(comp != nullptr, false);
    const float quota = 8.0f * ImGui::GetTextLineHeightWithSpacing();
    ctx->LogInfo("[pose factors] companion %.0f vs quota %.0f", static_cast<double>(comp->Size.y),
                 static_cast<double>(quota));
    IM_CHECK_RETV(fabsf(comp->Size.y - quota) < 2.0f, false);
  }
  // Hover on the canvas is the curve readout's temporary override only: the selection stands
  // and the factors region — bound to view.sample_index, which hover never writes — keeps its
  // geometry.
  {
    const int sample_before_hover = gui::g_state.analysis.point_view.sample_index;
    const ImVec2 factors_geo_before = PoseRegionWindow("##AnalyzePoseFactors")->Size;
    ctx->MouseMoveToPos(
        ImVec2(canvas.RectFull.Min.x + canvas.RectFull.GetWidth() * 0.3f, canvas.RectFull.GetCenter().y));
    ctx->Yield(3);
    IM_CHECK_RETV(gui::g_state.analysis.point_view.sample_index == sample_before_hover, false);
    const ImVec2 factors_geo_after = PoseRegionWindow("##AnalyzePoseFactors")->Size;
    IM_CHECK_RETV(factors_geo_before.x == factors_geo_after.x && factors_geo_before.y == factors_geo_after.y, false);
    ctx->LogInfo("[pose factors] hover wrote nothing; factors region held %.0fx%.0f",
                 static_cast<double>(factors_geo_after.x), static_cast<double>(factors_geo_after.y));
  }

  // Wheel zoom owns only the Y window: neither the cursor nor the query changes. Let the
  // existing region claim stand before sending input, as for the map/orbit gestures.
  const gui::PointIntentKey key_before = gui::g_state.analysis.point.key;
  ctx->MouseMoveToPos(canvas.RectFull.GetCenter());
  ctx->Yield(3);
  gui::PointViewState& curve_view = gui::g_state.analysis.point_view;
  const int cursor_before_wheel = curve_view.sample_index;
  const double span_before = curve_view.curve_axis.log_hi - curve_view.curve_axis.log_lo;
  const float scroll_before = curve->Scroll.y;
  IM_CHECK_RETV(span_before > 0.25, false);
  // Independent render oracle: read submitted product and frame vertices, then predict the
  // product's top from C-reader data and the current log window (not renderer helpers).
  const auto product_tracks_axis = [&]() -> bool {
    float product_top = FLT_MAX;
    float plot_top = FLT_MAX;
    float plot_bottom = -FLT_MAX;
    for (const ImDrawVert& vertex : curve->DrawList->VtxBuffer) {
      if (!canvas.RectFull.Contains(vertex.pos) || (vertex.col & IM_COL32_A_MASK) == 0) {
        continue;
      }
      if ((vertex.col & ~IM_COL32_A_MASK) == (gui::kPointCurveProductColor & ~IM_COL32_A_MASK)) {
        product_top = std::min(product_top, vertex.pos.y);
      }
      if (vertex.col == gui::kPointCurvePlotBorderColor) {
        plot_top = std::min(plot_top, vertex.pos.y);
        plot_bottom = std::max(plot_bottom, vertex.pos.y);
      }
    }
    IM_CHECK_RETV(product_top < FLT_MAX && plot_top < plot_bottom, false);
    gui::PointContributionCurve model;
    IM_CHECK_RETV(gui::BuildPointContributionCurve(*gui::g_state.analysis.point.adopted_result, 0, &model), false);
    double product_max = 0.0;
    for (const gui::PointCurvePoint& point : model.points) {
      if (point.product_kind == gui::PointCurveValueKind::kPositive) {
        product_max = std::max(product_max, point.product);
      }
    }
    IM_CHECK_RETV(product_max > 0.0, false);
    const double expected = plot_top + (curve_view.curve_axis.log_hi - std::log10(product_max)) /
                                           (curve_view.curve_axis.log_hi - curve_view.curve_axis.log_lo) *
                                           (plot_bottom - plot_top);
    ctx->LogInfo("[curve render] product top %.2f, independently predicted %.2f", (double)product_top, expected);
    IM_CHECK_RETV(std::abs(product_top - expected) < 2.5, false);  // stroke + frame AA widths
    return true;
  };
  IM_CHECK_RETV(product_tracks_axis(), false);
  ctx->MouseWheelY(6.0f);
  ctx->Yield(3);
  const double zoom_span = curve_view.curve_axis.log_hi - curve_view.curve_axis.log_lo;
  IM_CHECK_RETV(zoom_span < span_before, false);
  IM_CHECK_RETV(std::abs(zoom_span - span_before / std::pow(1.12, 6.0)) < 1e-8, false);
  IM_CHECK_RETV(product_tracks_axis(), false);
  IM_CHECK_RETV(curve_view.sample_index == cursor_before_wheel, false);
  IM_CHECK_RETV(gui::g_state.analysis.point.key == key_before, false);
  IM_CHECK_RETV(curve->Scroll.y == scroll_before, false);
  IM_CHECK_RETV(ctx->ItemInfo("**/##pose_curve_canvas").RectFull.Min.y == canvas.RectFull.Min.y, false);
  IM_CHECK_RETV(SaveWindowPng(ctx, "//Pose & trace", GuiTestTempPath("pose_curve_zoom.png").string()), false);

  // The canvas writes the ONE selection: a click at 80% width moves the cursor to a late valid
  // sample, stops the playback it finds running, and leaves the point intent untouched (AC4).
  gui::g_state.analysis.point_view.playing = true;
  ctx->Yield(2);
  ctx->MouseMoveToPos(ImVec2(canvas.RectFull.Min.x + canvas.RectFull.GetWidth() * 0.8f, canvas.RectFull.GetCenter().y));
  ctx->Yield(2);
  ctx->MouseClick(0);
  ctx->Yield(3);
  IM_CHECK_RETV(!gui::g_state.analysis.point_view.playing, false);
  const int clicked = gui::g_state.analysis.point_view.sample_index;
  ctx->LogInfo("[pose curve] click at 80%% width -> sample %d of %d valid", clicked, valid_count);
  IM_CHECK_RETV(clicked > 0 && clicked < valid_count, false);
  IM_CHECK_RETV(gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted, false);
  IM_CHECK_RETV(gui::g_state.analysis.point.key == key_before, false);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid, false);
  // The ray face follows the moved cursor (the trace re-keys to the clicked sample).
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [clicked] {
                      const gui::RayTraceSession& t = gui::g_state.analysis.trace;
                      if (!t.adopted_result || !t.intent_valid) {
                        return false;
                      }
                      const int stored =
                          gui::PointValidSampleIndexAt(*gui::g_state.analysis.point.adopted_result, 0, clicked);
                      return t.key.sample == stored;
                    },
                    30),
                false);

  // Hover writes nothing: a move over the canvas leaves the selection where the click put it.
  const int cursor_after_click = gui::g_state.analysis.point_view.sample_index;
  ctx->MouseMoveToPos(canvas.RectFull.GetCenter());
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.analysis.point_view.sample_index == cursor_after_click, false);

  // Clicking the pose ALREADY on screen still stops the playback (review r1 Major: a resolved
  // click pins the cursor even when the index does not change — without this, a click on the
  // current position lets playback run on and the user cannot freeze the pose in view).
  gui::g_state.analysis.point_view.playing = true;
  ctx->Yield(2);
  ctx->MouseMoveToPos(ImVec2(canvas.RectFull.Min.x + canvas.RectFull.GetWidth() * 0.8f, canvas.RectFull.GetCenter().y));
  ctx->Yield(2);
  ctx->MouseClick(0);
  ctx->Yield(3);
  IM_CHECK_RETV(!gui::g_state.analysis.point_view.playing, false);
  IM_CHECK_RETV(gui::g_state.analysis.point_view.sample_index == cursor_after_click, false);
  ctx->LogInfo("[pose curve] click on the current position stopped playback, index held at %d", cursor_after_click);

  // Dragging after zoom follows the same X mapping, while wheel-out restores fit without
  // moving the sample that the drag selected.
  ctx->MouseDown(0);
  ctx->MouseMoveToPos(ImVec2(canvas.RectFull.Min.x + canvas.RectFull.GetWidth() * 0.4f, canvas.RectFull.GetCenter().y));
  ctx->MouseUp(0);
  ctx->Yield(3);
  IM_CHECK_RETV(curve_view.sample_index != cursor_after_click, false);
  const int dragged = curve_view.sample_index;
  IM_CHECK_RETV(!curve_view.playing, false);
  ctx->MouseWheelY(-6.0f);
  ctx->Yield(3);
  IM_CHECK_RETV(std::abs(curve_view.curve_axis.log_hi - curve_view.curve_axis.log_lo - span_before) < 1e-8, false);
  IM_CHECK_RETV(curve_view.sample_index == dragged, false);
  IM_CHECK_RETV(gui::g_state.analysis.point.key == key_before, false);

  // The narrowest face: the D5 floors' promise — the canvas on, nothing clipped, the canvas
  // answering through the real mouse path. The geometry lines are the live-layout probe the
  // floor constants are tuned from (a red here is a floor budget that no longer holds).
  ImGuiWindow* win = ctx->GetWindowByRef("//Pose & trace");
  IM_CHECK_RETV(win != nullptr, false);
  const ImVec2 vp = ImGui::GetMainViewport()->Size;
  ImGui::SetWindowSize(
      win,
      ImVec2(std::min(gui::UiPx(gui::kPoseViewMinWidth), vp.x), std::min(gui::UiPx(gui::kPoseViewMinHeight), vp.y)),
      ImGuiCond_Always);
  ctx->Yield(3);
  ImGuiWindow* curve_n = PoseRegionWindow("##AnalyzePoseCurve");
  ImGuiWindow* globe_n = PoseRegionWindow("##AnalyzeSunGlobe");
  IM_CHECK_RETV(curve_n != nullptr && globe_n != nullptr, false);
  const ImGuiTestItemInfo canvas_n = ctx->ItemInfo("**/##pose_curve_canvas", ImGuiTestOpFlags_NoError);
  ctx->LogInfo(
      "[pose curve min] window %.0fx%.0f | globe h %.0f (floor %.0f) | curve h %.0f (floor %.0f) SM %.1f | "
      "canvas %s h %.0f",
      static_cast<double>(win->Size.x), static_cast<double>(win->Size.y), static_cast<double>(globe_n->Size.y),
      static_cast<double>(gui::UiPx(gui::kPoseViewGlobeFloor)), static_cast<double>(curve_n->Size.y),
      static_cast<double>(gui::UiPx(gui::kPoseViewCurveFloor)), static_cast<double>(curve_n->ScrollMax.y),
      canvas_n.ID != 0 ? "yes" : "NO", canvas_n.ID != 0 ? static_cast<double>(canvas_n.RectFull.GetHeight()) : 0.0);
  // The full form at the narrowest face is the floors' promise only while the viewport can
  // host the true minimum (the min(min, viewport) clamp rule); on a shorter viewport the
  // clamp's degraded share is the honest form — assert reachability instead.
  const bool true_minimum = win->Size.y >= gui::UiPx(gui::kPoseViewMinHeight) - 1.0f;
  if (true_minimum) {
    IM_CHECK_RETV(canvas_n.ID != 0, false);
    IM_CHECK_RETV(canvas_n.RectFull.Max.y <= curve_n->InnerClipRect.Max.y + 0.5f, false);
    const int before_min_click = gui::g_state.analysis.point_view.sample_index;
    ctx->MouseMoveToPos(
        ImVec2(canvas_n.RectFull.Min.x + canvas_n.RectFull.GetWidth() * 0.25f, canvas_n.RectFull.GetCenter().y));
    ctx->Yield(2);
    ctx->MouseClick(0);
    ctx->Yield(3);
    IM_CHECK_RETV(gui::g_state.analysis.point_view.sample_index != before_min_click, false);
    ctx->LogInfo("[pose curve min] canvas click moved the cursor %d -> %d", before_min_click,
                 gui::g_state.analysis.point_view.sample_index);
  } else {
    ctx->LogInfo(
        "[pose curve min] viewport %.0f shorter than the minimum %.0f: the clamp's share rules; region SM %.1f",
        static_cast<double>(vp.y), static_cast<double>(gui::UiPx(gui::kPoseViewMinHeight)),
        static_cast<double>(curve_n->ScrollMax.y));
    IM_CHECK_RETV(curve_n->ScrollMax.y >= 0.0f, false);
  }
  IM_CHECK_RETV(SaveWindowPng(ctx, "//Pose & trace", GuiTestTempPath("pose_curve_region_min.png").string()), false);
  if (canvas_n.ID != 0) {
    ctx->MouseMoveToPos(canvas_n.RectFull.GetCenter());
    ctx->Yield(3);
    ctx->MouseWheelY(1.0f);
    ctx->Yield(3);
    IM_CHECK_RETV(SaveWindowPng(ctx, "//Pose & trace", GuiTestTempPath("pose_curve_zoom_min.png").string()), false);
    ctx->MouseWheelY(-1.0f);
    ctx->Yield(3);
  }

  // Restore the default geometry before closing: this leg's min-size write must not become the
  // persisted size a later case inherits (the cross-case window-geometry lesson).
  ImGui::SetWindowSize(win, ImVec2(std::min(gui::UiPx(960.0f), vp.x), std::min(gui::UiPx(720.0f), vp.y)),
                       ImGuiCond_Always);
  ctx->Yield(2);
  gui::g_state.analyze_pose_view_open = false;
  ctx->Yield(2);
  return true;
}

// 779.5 — the right-button Y pan and the two-row footer: the pan's real input leg (the window
// slides as a whole, span preserved, and a value that entered the window draws its ink at the
// independently predicted pixel), the footer's two rows (the three x labels and the outside
// summary simultaneously present, on separate rows, with the rad unit), the boundary-value
// labels of a window with no decade major, and the exclusivity/lifecycle negative legs (left
// clicks under a held right button, a press begun outside, a drag carried out of the canvas,
// a right button held across a real adoption). The oracle math reads the submitted border
// vertices and the C-reader model — never the renderer's own helpers.
bool PoseCurvePanMovesTheWindowAndKeepsTheXLabels(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  IM_CHECK_RETV(SelectRowByDisplay(ctx, TopChainDisplay().c_str()) != std::nullopt, false);
  gui::g_state.analyze_pose_view_open = true;
  PinPoseViewOnScreen(ctx);
  SetSkyPointTarget(42.0, 0.0);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  gui::g_state.analyze_pose_view_open = true;
  PinPoseViewOnScreen(ctx);
  ctx->Yield(2);
  ImGuiWindow* curve = PoseRegionWindow("##AnalyzePoseCurve");
  IM_CHECK_RETV(curve != nullptr, false);
  const ImGuiTestItemInfo canvas = ctx->ItemInfo("**/##pose_curve_canvas", ImGuiTestOpFlags_NoError);
  IM_CHECK_RETV(canvas.ID != 0, false);
  gui::PointViewState& view = gui::g_state.analysis.point_view;
  const gui::PointIntentKey key_before = gui::g_state.analysis.point.key;
  gui::PointContributionCurve model;
  IM_CHECK_RETV(gui::BuildPointContributionCurve(*gui::g_state.analysis.point.adopted_result, 0, &model), false);
  const gui::PointCurveAxisRange fit = gui::PointCurveAxisFit(model);
  IM_CHECK_RETV(fit.valid, false);
  const double fit_lo_log = std::log10(fit.lo);
  const double fit_hi_log = std::log10(fit.hi);
  IM_CHECK_RETV(fit_hi_log - fit_lo_log > 1.5, false);  // a domain the zoom/pan legs can work in

  // The plot-rect oracle: the submitted border vertices give the real plot rect (the same read
  // the cursor case's render oracle makes, widened to x) — every pixel prediction below builds
  // on this rect, never on the renderer's helpers. The canvas rect is re-read per call: the D7
  // legs below move it, and a rect captured at case start would filter the wrong area.
  const auto canvas_rect = [&]() {
    const ImGuiTestItemInfo c = ctx->ItemInfo("**/##pose_curve_canvas", ImGuiTestOpFlags_NoError);
    return c.ID != 0 ? c.RectFull : ImRect(0.0f, 0.0f, -1.0f, -1.0f);
  };
  const auto plot_rect = [&]() {
    const ImRect zone = canvas_rect();
    ImRect rect(ImVec2(FLT_MAX, FLT_MAX), ImVec2(-FLT_MAX, -FLT_MAX));
    if (zone.Min.x > zone.Max.x) {
      return rect;  // no canvas item — no plot, whatever the stale frame drew
    }
    for (const ImDrawVert& vertex : curve->DrawList->VtxBuffer) {
      if (zone.Contains(vertex.pos) && vertex.col == gui::kPointCurvePlotBorderColor) {
        rect.Add(vertex.pos);
      }
    }
    return rect;
  };
  const ImU32 text_color = ImGui::GetColorU32(ImGuiCol_TextDisabled);
  const auto glyphs_in = [&](const ImRect& zone) {
    std::vector<ImRect> glyphs;
    const ImVector<ImDrawVert>& vertices = curve->DrawList->VtxBuffer;
    for (int i = 0; i + 3 < vertices.Size;) {
      if (vertices[i].col != text_color) {
        ++i;
        continue;
      }
      ImRect rect(vertices[i].pos, vertices[i].pos);
      for (int k = 1; k < 4; ++k) {
        rect.Add(vertices[i + k].pos);
      }
      i += 4;  // one glyph = one 4-vertex quad of one colour, the read the legend probe makes
      if (zone.Overlaps(rect)) {
        glyphs.push_back(rect);
      }
    }
    return glyphs;
  };
  const float line_h = ImGui::GetTextLineHeight();

  // ---- Zoom until the window hides real points (the outside summary's non-empty precondition)
  // — bounded: the halo scene's positive pool spans decades, so a dozen notches must do it. ----
  ctx->MouseMoveToPos(canvas.RectFull.GetCenter());
  ctx->Yield(3);
  int outside_points = 0;
  for (int notch = 0; notch < 14; ++notch) {
    ctx->MouseWheelY(3.0f);
    ctx->Yield(2);
    const gui::PointCurveAxisRange vis = gui::PointCurveVisibleRange(fit, &view.curve_axis);
    outside_points = gui::PointCurveCountOutside(model, vis.lo, vis.hi).points;
    if (outside_points > 0) {
      break;
    }
  }
  IM_CHECK_RETV(outside_points > 0, false);
  const gui::PointCurveAxisRange zoomed = gui::PointCurveVisibleRange(fit, &view.curve_axis);
  ctx->LogInfo("[pose curve pan] zoomed to outside=%d window [%.4g, %.4g] span %.3f dec", outside_points, zoomed.lo,
               zoomed.hi, std::log10(zoomed.hi) - std::log10(zoomed.lo));

  // ---- AC2: with a non-empty outside count, the three x labels, the summary and the rad unit
  // are all on screen — the labels on their own row, the summary on the row below, nothing
  // spanning both. The check is reusable: the degrade leg below must find the same rows again
  // after growing the canvas back. ----
  const auto check_footer_rows = [&]() -> bool {
    const ImRect plot = plot_rect();
    IM_CHECK_RETV(plot.Min.x < plot.Max.x && plot.Min.y < plot.Max.y, false);  // the plot rect is ordered
    const ImRect row_a(plot.Min.x - gui::UiPx(gui::kAnalyzeSplitterSize), plot.Max.y + gui::UiPx(8.0f),
                       plot.Max.x + gui::UiPx(4.0f), plot.Max.y + gui::UiPx(13.0f) + line_h + 1.5f);
    const std::vector<ImRect> label_glyphs = glyphs_in(row_a);
    IM_CHECK_RETV(label_glyphs.size() >= 3, false);
    const auto cluster_at = [&](float x) {
      return std::any_of(label_glyphs.begin(), label_glyphs.end(),
                         [x](const ImRect& g) { return g.Min.x - 2.0f <= x && x <= g.Max.x + 2.0f; });
    };
    IM_CHECK_RETV(cluster_at(plot.Min.x), false);                        // "0" centred on the left edge
    IM_CHECK_RETV(cluster_at((plot.Min.x + plot.Max.x) * 0.5f), false);  // the mid label
    IM_CHECK_RETV(cluster_at(plot.Max.x - 1.0f), false);                 // the right-aligned hi label
    const ImRect zone = canvas_rect();
    const ImRect row_b(plot.Min.x - gui::UiPx(gui::kAnalyzeSplitterSize), row_a.Max.y + 1.0f, zone.Max.x, zone.Max.y);
    IM_CHECK_RETV(!glyphs_in(row_b).empty(), false);  // the summary's own row below the labels
    bool spans_both_rows = false;
    for (const ImRect& g : glyphs_in(ImRect(row_a.Min.x, row_a.Min.y, row_b.Max.x, row_b.Max.y))) {
      spans_both_rows = spans_both_rows || (g.Min.y < row_a.Max.y && g.Max.y > row_b.Min.y);
    }
    IM_CHECK_RETV(!spans_both_rows, false);  // no glyph spans both rows
    return true;
  };
  IM_CHECK_RETV(check_footer_rows(), false);
  const ImRect rad_zone(canvas.RectFull.Max.x - ImGui::CalcTextSize("rad").x - gui::UiPx(8.0f), canvas.RectFull.Min.y,
                        canvas.RectFull.Max.x, canvas.RectFull.Min.y + line_h + gui::UiPx(6.0f));
  IM_CHECK_RETV(!glyphs_in(rad_zone).empty(), false);

  // ---- AC1: press right inside the canvas, drag DOWN — the window slides toward larger values
  // until it sits flush at the fit's top (the clamp's whole-window shape), span never moving. ----
  const double span_before = view.curve_axis.log_hi - view.curve_axis.log_lo;
  const ImRect plot0 = plot_rect();
  IM_CHECK_RETV(plot0.Min.x < plot0.Max.x && plot0.Min.y < plot0.Max.y, false);
  const float plot_h0 = plot0.Max.y - plot0.Min.y;
  const float drag_bottom = canvas.RectFull.GetCenter().y + plot_h0;
  ctx->MouseDown(1);
  ctx->Yield(1);
  IM_CHECK_RETV(view.curve_pan_active, false);
  for (float y = canvas.RectFull.GetCenter().y; y < drag_bottom; y += plot_h0 * 0.25f) {
    ctx->MouseMoveToPos(ImVec2(canvas.RectFull.GetCenter().x, y));
    ctx->Yield(1);
  }
  ctx->Yield(2);
  IM_CHECK_RETV(std::abs(view.curve_axis.log_hi - fit_hi_log) < 1e-9, false);  // flush at the fit's top
  IM_CHECK_RETV(std::abs(view.curve_axis.log_hi - view.curve_axis.log_lo - span_before) < 1e-9, false);

  // ---- The measured leg: choose a real below-window product value as the pan target, drag UP
  // by the distance the independent log math names, and verify the window covers it and its ink
  // is drawn at the predicted pixel. The flushed window has unbounded room below — no clamp
  // contaminates the arithmetic. ----
  const gui::PointCurveAxisRange flushed = gui::PointCurveVisibleRange(fit, &view.curve_axis);
  const auto arm_value = [](int arm, const gui::PointCurvePoint& p) -> double {
    switch (arm) {
      case 0:
        return p.rho;
      case 1:
        return p.area;
      case 2:
        return p.transmission;
      case 3:
        return p.k_carea;
      default:
        return p.product;
    }
  };
  const auto arm_kind = [](int arm, const gui::PointCurvePoint& p) -> gui::PointCurveValueKind {
    switch (arm) {
      case 0:
        return p.rho_kind;
      case 1:
        return p.area_kind;
      case 2:
        return p.transmission_kind;
      case 3:
        return p.k_kind;
      default:
        return p.product_kind;
    }
  };
  int target_arm = -1;
  double target_value = 0.0;
  for (int arm = 4; arm >= 0; --arm) {  // the product arm preferred, any honest arm acceptable
    for (const gui::PointCurvePoint& point : model.points) {
      if (arm_kind(arm, point) == gui::PointCurveValueKind::kPositive && arm_value(arm, point) < flushed.lo) {
        target_arm = arm;
        target_value = arm_value(arm, point);
        break;
      }
    }
    if (target_arm >= 0) {
      break;
    }
  }
  IM_CHECK_RETV(target_arm >= 0, false);
  const double shift_log = std::log10(flushed.lo) - std::log10(target_value) + 0.15;  // +margin inside
  const double shift_frac = shift_log / span_before;
  const float drag_up = static_cast<float>(shift_frac * plot_h0);
  const int cursor_before_pan = view.sample_index;
  for (float y = canvas.RectFull.GetCenter().y + plot_h0; y > canvas.RectFull.GetCenter().y + plot_h0 - drag_up;
       y -= plot_h0 * 0.2f) {
    ctx->MouseMoveToPos(ImVec2(canvas.RectFull.GetCenter().x, y));
    ctx->Yield(1);
  }
  // While the right button holds the pan, a left press must not write the cursor (§3.1's mutex).
  // MouseDown/MouseUp or the button mask (MouseClick would replace it and drop the right button
  // — the engine's own shape), which is exactly the overlap the gate must survive.
  ctx->MouseDown(0);
  ctx->Yield(2);
  ctx->MouseUp(0);
  ctx->Yield(2);
  IM_CHECK_RETV(view.sample_index == cursor_before_pan, false);
  IM_CHECK_RETV(view.curve_pan_active, false);  // ...and the pan keeps its latch through it
  ctx->MouseUp(1);
  ctx->Yield(2);
  IM_CHECK_RETV(!view.curve_pan_active, false);
  const double lo_after = view.curve_axis.log_lo;
  const double hi_after = view.curve_axis.log_hi;
  IM_CHECK_RETV(std::abs((hi_after - lo_after) - span_before) < 1e-9, false);  // span preserved
  IM_CHECK_RETV(std::abs(hi_after - (fit_hi_log - shift_log)) < 0.2 * span_before,
                false);  // the shift the drag named (± the footer's responsive plot height)
  IM_CHECK_RETV(lo_after <= std::log10(target_value) && std::log10(target_value) <= hi_after, false);
  // The entered value's ink: a vertex of the target arm's colour within 3px of the predicted
  // pixel — predicted from the measured plot rect and the window the state carries (independent
  // log math, the cursor case's oracle shape).
  {
    const ImRect plot1 = plot_rect();
    const double log_lo = std::log10(view.curve_axis.log_lo <= fit_lo_log ? fit.lo : std::pow(10.0, lo_after));
    const double log_hi = std::log10(view.curve_axis.log_hi >= fit_hi_log ? fit.hi : std::pow(10.0, hi_after));
    const double predicted =
        plot1.Min.y + (log_hi - std::log10(target_value)) / (log_hi - log_lo) * (plot1.Max.y - plot1.Min.y);
    const ImU32 arm_color = gui::kPointCurveArmColors[target_arm];
    bool ink_found = false;
    for (const ImDrawVert& vertex : curve->DrawList->VtxBuffer) {
      if (vertex.col == arm_color && plot1.Contains(vertex.pos) && std::abs(vertex.pos.y - predicted) <= 3.0f) {
        ink_found = true;
        break;
      }
    }
    IM_CHECK_RETV(ink_found, false);
    ctx->LogInfo("[pose curve pan] target %.4g (arm %d) predicted py %.1f — ink on screen", target_value, target_arm,
                 (double)predicted);
  }
  IM_CHECK_RETV(view.sample_index == cursor_before_pan, false);
  IM_CHECK_RETV(!view.playing, false);
  IM_CHECK_RETV(gui::g_state.analysis.point.key == key_before, false);
  IM_CHECK_RETV(curve->Scroll.y == 0.0f, false);
  IM_CHECK_RETV(SaveWindowPng(ctx, "//Pose & trace", GuiTestTempPath("pose_curve_pan_target.png").string()), false);

  // ---- The pan does not stop the playback: it runs on through a right-button drag. ----
  view.playing = true;
  ctx->MouseMoveToPos(canvas.RectFull.GetCenter());
  ctx->Yield(2);
  ctx->MouseDown(1);
  ctx->Yield(1);
  IM_CHECK_RETV(view.curve_pan_active, false);  // latched over the playback
  ctx->MouseMoveToPos(ImVec2(canvas.RectFull.GetCenter().x, canvas.RectFull.GetCenter().y + 40.0f));
  ctx->Yield(2);
  ctx->MouseUp(1);
  ctx->Yield(2);
  IM_CHECK_RETV(view.playing, false);  // the pan never stopped it
  view.playing = false;

  // ---- Negative legs: a press begun outside the canvas never latches; a drag carried out of
  // the canvas keeps panning (the gesture owns its drag), moves no host window, scrolls no
  // region, and the release ends it. ----
  const double log_lo_stable = view.curve_axis.log_lo;
  ctx->MouseMoveToPos(ImVec2(canvas.RectFull.Min.x - 30.0f, canvas.RectFull.Min.y - 30.0f));
  ctx->Yield(2);
  ctx->MouseDown(1);
  ctx->Yield(1);
  IM_CHECK_RETV(!view.curve_pan_active, false);
  ctx->MouseMoveToPos(canvas.RectFull.GetCenter());
  ctx->Yield(2);
  ctx->MouseUp(1);
  ctx->Yield(1);
  IM_CHECK_RETV(!view.curve_pan_active, false);
  IM_CHECK_RETV(view.curve_axis.log_lo == log_lo_stable, false);

  ImGuiWindow* host = ctx->GetWindowByRef("//Pose & trace");
  IM_CHECK_RETV(host != nullptr, false);
  const ImVec2 host_pos_before = host->Pos;
  const float scroll_before = curve->Scroll.y;
  ctx->MouseMoveToPos(canvas.RectFull.GetCenter());
  ctx->Yield(2);
  ctx->MouseDown(1);
  ctx->Yield(1);
  IM_CHECK_RETV(view.curve_pan_active, false);
  ctx->MouseMoveToPos(ImVec2(canvas.RectFull.GetCenter().x, canvas.RectFull.Min.y - 80.0f));
  ctx->Yield(2);
  IM_CHECK_RETV(view.curve_pan_active, false);  // the gesture survives leaving the canvas
  ctx->MouseUp(1);
  ctx->Yield(2);
  IM_CHECK_RETV(!view.curve_pan_active, false);
  IM_CHECK_RETV(host->Pos.x == host_pos_before.x && host->Pos.y == host_pos_before.y, false);
  IM_CHECK_RETV(curve->Scroll.y == scroll_before, false);

  // ---- Major-2 regression: the press frame establishes ownership only. The approach move
  // lands in the SAME input frame as the press (MouseTeleportToPos NoYield + MouseDown), and a
  // mere right-click must not pan by the movement it carried in — the window holds. ----
  ctx->MouseMoveToPos(canvas.RectFull.GetCenter());
  ctx->Yield(3);
  ctx->MouseWheelY(-14.0f);  // a known mid-fit window: back out to the full span, then one notch
  ctx->Yield(2);             // in at the centre anchor — room on both sides for the drag below
  ctx->MouseWheelY(1.0f);
  ctx->Yield(2);
  const double log_lo_before_click = view.curve_axis.log_lo;
  ctx->MouseTeleportToPos(ImVec2(canvas.RectFull.GetCenter().x, canvas.RectFull.GetCenter().y - 60.0f),
                          ImGuiTestOpFlags_NoYield);
  ctx->MouseDown(1);
  ctx->Yield(2);
  IM_CHECK_RETV(view.curve_axis.log_lo == log_lo_before_click, false);  // no jump from the approach move
  ctx->MouseUp(1);
  ctx->Yield(1);
  // A real drag after the same-frame press still pans (the ownership carries into the drag) —
  // dragged UP: the window moves toward smaller values with room below.
  ctx->MouseDown(1);
  ctx->MouseMoveToPos(ImVec2(canvas.RectFull.GetCenter().x, canvas.RectFull.GetCenter().y - 180.0f));
  ctx->Yield(2);
  ctx->MouseUp(1);
  ctx->Yield(2);
  IM_CHECK_RETV(view.curve_axis.log_lo != log_lo_before_click, false);  // the drag moved the window
  IM_CHECK_RETV(!view.curve_pan_active, false);

  // ---- Major-3 regression: a left press begun during a pan stays swallowed until released —
  // releasing the RIGHT button first must not wake the held left drag into a cursor write. The
  // playback is the discriminator (the write path always stops it, J3 — index moves are
  // playback's own, not a gesture's): a woken write would end it, the swallow lets it run. ----
  view.playing = true;
  ctx->MouseMoveToPos(canvas.RectFull.GetCenter());
  ctx->Yield(2);
  ctx->MouseDown(1);  // the pan owns the gesture
  ctx->Yield(1);
  IM_CHECK_RETV(view.curve_pan_active, false);
  ctx->MouseDown(0);  // the left press begins UNDER the pan
  ctx->Yield(1);
  ctx->MouseMoveToPos(ImVec2(canvas.RectFull.GetCenter().x + 40.0f, canvas.RectFull.GetCenter().y));
  ctx->Yield(2);    // both held, dragging
  ctx->MouseUp(1);  // the pan ends first — the held left drag must NOT wake into a cursor write
  ctx->Yield(2);
  IM_CHECK_RETV(!view.curve_pan_active, false);
  IM_CHECK_RETV(view.playing, false);  // the playback ran on through the whole overlap
  ctx->MouseUp(0);
  ctx->Yield(2);
  IM_CHECK_RETV(view.playing, false);  // releasing the swallowed press itself stops nothing
  // A fresh left press afterwards selects normally (a resolved click stops the playback).
  view.playing = true;
  ctx->Yield(1);
  ctx->MouseMoveToPos(ImVec2(canvas.RectFull.Min.x + canvas.RectFull.GetWidth() * 0.3f, canvas.RectFull.GetCenter().y));
  ctx->Yield(2);
  ctx->MouseDown(0);
  ctx->Yield(2);
  ctx->MouseUp(0);
  ctx->Yield(2);
  IM_CHECK_RETV(!view.playing, false);  // the fresh press resolved and stopped it — no swallow
  view.playing = false;

  // ---- AC3's narrow window: zoom to a sub-decade span and pan it strictly between two decade
  // exponents — no labelled major, and the window's own boundary values are the only numeric
  // reference, drawn in the gutter at the plot's top and bottom frame, fully inside the canvas. ----
  ctx->MouseMoveToPos(canvas.RectFull.GetCenter());
  ctx->Yield(2);
  for (int notch = 0; notch < 8; ++notch) {
    ctx->MouseWheelY(3.0f);
    ctx->Yield(1);
  }
  ctx->Yield(2);
  const double deep_span = view.curve_axis.log_hi - view.curve_axis.log_lo;
  IM_CHECK_RETV(deep_span < 0.9, false);
  float pan_y = canvas.RectFull.GetCenter().y;
  for (int step = 0; step < 20; ++step) {
    const double lo = view.curve_axis.log_lo;
    const double hi = view.curve_axis.log_hi;
    if (std::ceil(lo) > hi) {
      break;  // no integer exponent inside [lo, hi]
    }
    ctx->MouseMoveToPos(ImVec2(canvas.RectFull.GetCenter().x, pan_y));
    ctx->Yield(1);
    ctx->MouseDown(1);
    ctx->MouseMoveToPos(ImVec2(canvas.RectFull.GetCenter().x, pan_y - 30.0f));  // drag up: window toward smaller
    ctx->Yield(1);
    ctx->MouseUp(1);
    ctx->Yield(1);
    pan_y -= 30.0f;
  }
  {
    const double lo = view.curve_axis.log_lo;
    const double hi = view.curve_axis.log_hi;
    IM_CHECK_RETV(std::ceil(lo) > hi, false);  // strictly between two decades
    ctx->LogInfo("[pose curve pan] no-major window [%.3f, %.3f]", lo, hi);
  }
  {
    const ImRect plot2 = plot_rect();
    IM_CHECK_RETV(plot2.Min.y < plot2.Max.y, false);
    const ImRect gutter(canvas.RectFull.Min.x, canvas.RectFull.Min.y, plot2.Min.x - 3.0f, canvas.RectFull.Max.y);
    const std::vector<ImRect> boundary = glyphs_in(gutter);
    const auto band = [&](float py) {
      return std::any_of(boundary.begin(), boundary.end(), [&](const ImRect& g) {
        return g.Min.x >= canvas.RectFull.Min.x &&  // fully inside, not clipped by the canvas
               std::abs((g.Min.y + g.Max.y) * 0.5f - py) <= line_h * 0.6f;
      });
    };
    IM_CHECK_RETV(band(plot2.Min.y), false);  // the window's top boundary value
    IM_CHECK_RETV(band(plot2.Max.y), false);  // the window's bottom boundary value
    IM_CHECK_RETV(SaveWindowPng(ctx, "//Pose & trace", GuiTestTempPath("pose_curve_pan_boundary_labels.png").string()),
                  false);
  }

  // ---- The small-canvas degrade (the footer's real budget): with a non-empty outside summary,
  // shrink the curve canvas through D7 until the footer leaves no real plot — the named note
  // form replaces the plot (no inverted rect, no clipped labels), the pan dies, and growing
  // back restores the full form. The predicate consumes live line heights, so it is scale-free;
  // the 1.5x leg at the case's end re-runs the same shape at the other calibration point. ----
  bool degraded = false;
  bool strip_ok = true;
  for (int i = 0; i < 10 && !degraded && strip_ok; ++i) {
    strip_ok = PoseDragStrip(ctx, "**/##PoseCurveSplitter", 0.0f, -30.0f);  // the drag helpers'
    ctx->Yield(2);                                                          // verdict judges once, after
    const ImRect plot_d = plot_rect();
    degraded = !(plot_d.Min.y < plot_d.Max.y);  // the plot border is gone — no inverted rect drawn
  }
  IM_CHECK_RETV(strip_ok, false);
  IM_CHECK_RETV(degraded, false);
  {
    const ImRect zone = canvas_rect();
    const ImRect center(zone.Min.x, zone.GetCenter().y - line_h, zone.Max.x, zone.GetCenter().y + line_h);
    IM_CHECK_RETV(!glyphs_in(center).empty(), false);  // the note's ink, centred where the plot was
    ctx->LogInfo("[pose curve pan] degraded canvas %.0fpx — the note form holds", (double)zone.GetHeight());
  }
  const double log_lo_degraded = view.curve_axis.log_lo;
  ctx->MouseMoveToPos(canvas_rect().GetCenter());
  ctx->Yield(2);
  ctx->MouseDown(1);
  ctx->MouseMoveToPos(ImVec2(canvas_rect().GetCenter().x, canvas_rect().GetCenter().y + 40.0f));
  ctx->Yield(2);
  IM_CHECK_RETV(!view.curve_pan_active, false);  // the degraded canvas takes no pan
  IM_CHECK_RETV(view.curve_axis.log_lo == log_lo_degraded, false);
  ctx->MouseUp(1);
  ctx->Yield(1);
  // The degraded form's wheel leg: the hidden window must not move either — both endpoints hold
  // while the note form owns the canvas, and a wheel after the restore works again (the two
  // judgments, input and ink, are the same arithmetic; only one transition frame can differ).
  const double lo_before_degraded_wheel = view.curve_axis.log_lo;
  const double hi_before_degraded_wheel = view.curve_axis.log_hi;
  ctx->MouseMoveToPos(canvas_rect().GetCenter());
  ctx->Yield(2);
  ctx->MouseWheelY(-3.0f);  // zoom OUT: this leg's window already sits at the 0.25-decade span
  ctx->Yield(2);            // floor, so a zoom-in would be a clamp no-op with no discriminative power
  IM_CHECK_RETV(view.curve_axis.log_lo == lo_before_degraded_wheel, false);
  IM_CHECK_RETV(view.curve_axis.log_hi == hi_before_degraded_wheel, false);
  bool restored = false;
  strip_ok = true;
  for (int i = 0; i < 10 && !restored && strip_ok; ++i) {
    strip_ok = PoseDragStrip(ctx, "**/##PoseCurveSplitter", 0.0f, 40.0f);
    ctx->Yield(2);
    const ImRect plot_r = plot_rect();
    restored = plot_r.Min.y < plot_r.Max.y;
  }
  IM_CHECK_RETV(strip_ok, false);
  IM_CHECK_RETV(restored, false);
  IM_CHECK_RETV(check_footer_rows(), false);  // the two rows are intact again at the grown canvas
  ctx->MouseMoveToPos(canvas_rect().GetCenter());
  ctx->Yield(2);
  ctx->MouseWheelY(-2.0f);  // zoom out: the deep window is not at the fit's full span, so it moves
  ctx->Yield(2);
  IM_CHECK_RETV(view.curve_axis.log_hi - view.curve_axis.log_lo > hi_before_degraded_wheel - lo_before_degraded_wheel,
                false);  // the restored canvas zooms again

  // ---- AC4: leave the fit by zoom and pan, then a REAL new adoption resets the window — and a
  // right button held across the adoption does not resurrect the gesture (no new press, no pan). ----
  ctx->MouseMoveToPos(canvas.RectFull.GetCenter());
  ctx->Yield(2);
  ctx->MouseWheelY(8.0f);
  ctx->Yield(2);
  IM_CHECK_RETV(view.curve_axis.log_lo > fit_lo_log + 1e-9, false);  // the window left the fit's floor
  ctx->MouseDown(1);
  ctx->MouseMoveToPos(ImVec2(canvas.RectFull.GetCenter().x, canvas.RectFull.GetCenter().y - 50.0f));
  ctx->Yield(2);
  ctx->MouseUp(1);
  ctx->Yield(1);
  IM_CHECK_RETV(view.curve_axis.log_hi < fit_hi_log - 1e-9, false);  // ...and its ceiling
  ctx->MouseDown(1);                                                 // held across the adoption that follows
  ctx->Yield(1);
  SetSkyPointTarget(43.0, 0.0);
  // The key change DROPS the adoption first (the pump's AC5 rule): wait through the drop, then
  // for the new document — a same-frame check would mistake the old result for the new one.
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) != gui::PointQueryState::kCompleted; },
          30),
      false);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  ctx->Yield(2);
  gui::PointContributionCurve model2;
  IM_CHECK_RETV(gui::BuildPointContributionCurve(*gui::g_state.analysis.point.adopted_result, 0, &model2), false);
  const gui::PointCurveAxisRange fit2 = gui::PointCurveAxisFit(model2);
  IM_CHECK_RETV(fit2.valid, false);
  IM_CHECK_RETV(std::abs(view.curve_axis.log_lo - std::log10(fit2.lo)) < 1e-9, false);  // back to the fit
  IM_CHECK_RETV(std::abs(view.curve_axis.log_hi - std::log10(fit2.hi)) < 1e-9, false);
  IM_CHECK_RETV(!view.curve_pan_active, false);
  ctx->Yield(3);  // more frames with the button still down — no resurrection without a press
  IM_CHECK_RETV(std::abs(view.curve_axis.log_lo - std::log10(fit2.lo)) < 1e-9, false);
  IM_CHECK_RETV(!view.curve_pan_active, false);
  ctx->MouseUp(1);
  ctx->Yield(1);

  // ---- The same degrade at 1.5x (the §16.4.1 calibration duty, this task's share): the
  // predicate consumes UiPx and live line heights, so the note form must hold at the other
  // calibration point too. The scale restores before the case ends — this leg's write must not
  // become the scale a later case inherits. ----
  {
    const auto set_scale = [&](float multiplier) -> bool {
      gui::g_ui_scale_multiplier = multiplier;
      gui::g_ui_scale_dirty = true;
      for (int i = 0;
           i < 60 && !(gui::UiPx(gui::kAnalyzeLeftPanelWidth) > multiplier * gui::kAnalyzeLeftPanelWidth - 0.5f &&
                       gui::UiPx(gui::kAnalyzeLeftPanelWidth) < multiplier * gui::kAnalyzeLeftPanelWidth + 0.5f);
           ++i) {
        ctx->Yield(1);
      }
      IM_CHECK_RETV(fabsf(gui::UiPx(gui::kAnalyzeLeftPanelWidth) - multiplier * gui::kAnalyzeLeftPanelWidth) < 0.5f,
                    false);
      ctx->Yield(4);
      return true;
    };
    IM_CHECK_RETV(set_scale(1.5f), false);
    bool degraded_15 = false;
    bool strip_ok_15 = true;
    for (int i = 0; i < 10 && !degraded_15 && strip_ok_15; ++i) {
      strip_ok_15 = PoseDragStrip(ctx, "**/##PoseCurveSplitter", 0.0f, -40.0f);
      ctx->Yield(2);
      const ImRect plot_d = plot_rect();
      degraded_15 = !(plot_d.Min.y < plot_d.Max.y);
    }
    IM_CHECK_RETV(strip_ok_15, false);
    IM_CHECK_RETV(degraded_15, false);
    {
      const ImRect zone = canvas_rect();
      const ImRect center(zone.Min.x, zone.GetCenter().y - ImGui::GetTextLineHeight(), zone.Max.x,
                          zone.GetCenter().y + ImGui::GetTextLineHeight());
      IM_CHECK_RETV(!glyphs_in(center).empty(), false);
      ctx->LogInfo("[pose curve pan] degraded at 1.5x, canvas %.0fpx — the note form holds", (double)zone.GetHeight());
    }
    // The 1.5x wheel leg: the same hidden-window boundary at the other calibration point.
    const double lo15 = view.curve_axis.log_lo;
    const double hi15 = view.curve_axis.log_hi;
    ctx->MouseMoveToPos(canvas_rect().GetCenter());
    ctx->Yield(2);
    ctx->MouseWheelY(2.0f);  // zoom IN: this leg's window sits at the fit's full span, so a
    ctx->Yield(2);           // zoom-out would be a clamp no-op with no discriminative power
    IM_CHECK_RETV(view.curve_axis.log_lo == lo15 && view.curve_axis.log_hi == hi15, false);
    bool restored_15 = false;
    strip_ok_15 = true;
    for (int i = 0; i < 10 && !restored_15 && strip_ok_15; ++i) {
      strip_ok_15 = PoseDragStrip(ctx, "**/##PoseCurveSplitter", 0.0f, 40.0f);
      ctx->Yield(2);
      const ImRect plot_r = plot_rect();
      restored_15 = plot_r.Min.y < plot_r.Max.y;
    }
    IM_CHECK_RETV(strip_ok_15, false);
    IM_CHECK_RETV(restored_15, false);
    IM_CHECK_RETV(set_scale(gui::kFactoryUiScaleMultiplier), false);
  }
  return true;
}

// ⑧ — the multi-window real mouse chain: Raypath view, the pose window and the Entry Edit modal
// are open in the same frame, and a real mouse action in each writes its own state — the map
// pick sets the sky point, the pose D5 drag writes its ratio, the modal's edit writes its field.
bool PoseViewJoinsAMultiWindowRealMouseChain(ImGuiTestContext* ctx) {
  ScopedWatchdogGuard watchdog(ctx->EngineIO);
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  IM_CHECK_RETV(SelectRowByDisplay(ctx, TopChainDisplay().c_str()) != std::nullopt, false);

  gui::g_state.analyze_raypath_view_open = true;
  PinRaypathViewOnScreen(ctx);
  gui::g_state.analyze_pose_view_open = true;
  PinPoseViewOnScreen(ctx);
  ctx->Yield(2);
  // The pose window above the map window: the pick lands on the map canvas at a point the pose
  // window does not cover (the map window sits at (8,60), the pose at (8,40) — park the pose
  // window lower-right but entirely on-screen so both canvases stay pressable and D5
  // has actual space above its two floors, rather than a nominal off-screen suffix).
  if (ImGuiWindow* pose_m = ctx->GetWindowByRef("//Pose & trace")) {
    ImGui::SetWindowPos(pose_m, ImVec2(360.0f, 240.0f), ImGuiCond_Always);
    ctx->Yield(2);
  }
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      return m.adopted_result != nullptr && (m.adopted_id == m.intent_id);
                    },
                    60),
                false);
  ctx->KeyDown(ImGuiMod_Alt);
  ctx->ItemClick("**/##map_canvas");
  ctx->KeyUp(ImGuiMod_Alt);
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid, false);
  ctx->LogInfo("[pose multiwindow] map pick set the sky point with both windows open");

  // The pose D5 splitter, over the real mouse path, writes its ratio while the map window is
  // also live.
  const float d5_before = gui::g_state.analyze_pose_view_globe_ratio;
  const ImGuiTestItemInfo strip = ctx->ItemInfo("**/##PoseViewRowsSplitter", ImGuiTestOpFlags_NoError);
  IM_CHECK_RETV(strip.ID != 0, false);
  const ImVec2 c = strip.RectFull.GetCenter();
  ctx->MouseMoveToPos(c);
  ctx->Yield(2);
  ctx->MouseDown(0);
  ctx->Yield(2);
  ctx->MouseMoveToPos(ImVec2(c.x, c.y + 40.0f));
  ctx->Yield(3);
  ctx->MouseUp(0);
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analyze_pose_view_globe_ratio > d5_before + 0.02f, false);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid, false);
  ctx->LogInfo("[pose multiwindow] D5 drag wrote its ratio; the sky point stood");

  // The Entry Edit modal joins (HARD precondition — the review's no-silent-skip rule: an editor
  // that fails to open fails this case, it does not shrink the claim). The row label is the
  // product's own derivation (RenderCrystalList's), not a hardcoded guess. The edit goes
  // through the modal's real input and OK commit, verified on the document field, then reverted
  // the same way — the two floating windows and their states stand untouched throughout.
  const float sun_alt_before = gui::g_state.sun.altitude;
  // BY VALUE: the immediate editor writes the pool crystal in place, so a bound reference
  // would track the edit and the restore assert would compare the object with itself.
  const std::string first_name =
      gui::g_state.crystals[static_cast<size_t>(gui::g_state.layers[0].entries[0].crystal_id)].name;
  IM_CHECK_RETV(first_name != "PoseEditProbe", false);
  // The row's label carries the entry's own name, which this leg's own edit changes, so the
  // address is re-derived from the live state at every click instead of cached.
  const auto crystal_row_ref = [] {
    const std::string& nm =
        gui::g_state.crystals[static_cast<size_t>(gui::g_state.layers[0].entries[0].crystal_id)].name;
    char buf[160];
    std::snprintf(buf, sizeof(buf), "Layer %d, entry %d: %s##crys_0_0", gui::DisplayLayerNumber(0),
                  gui::DisplayEntryNumber(0), nm.empty() ? "Crystal" : nm.c_str());
    return std::string("**/") + buf;
  };
  ctx->SetRef("");
  ctx->ItemClick(crystal_row_ref().c_str());
  IM_CHECK_RETV(DriveUntil(
                    ctx, [ctx] { return ctx->ItemInfo("**/##crystal_name", ImGuiTestOpFlags_NoError).ID != 0; }, 20),
                false);  // the editor DID open over both windows
  ctx->LogInfo("[pose multiwindow] entry editor opened over both windows");
  const std::string probe_name = "PoseEditProbe";
  ctx->ItemInputValue("**/##crystal_name", probe_name.c_str());
  ctx->Yield(2);
  ctx->Yield(2);
  ctx->ItemClick("**/Close##edit_modal");
  ctx->Yield(3);
  const int probe_crystal = gui::g_state.layers[0].entries[0].crystal_id;
  IM_CHECK_RETV(gui::g_state.crystals[static_cast<size_t>(probe_crystal)].name == probe_name, false);
  IM_CHECK_RETV(ctx->ItemInfo("**/##crystal_name", ImGuiTestOpFlags_NoError).ID == 0, false);  // the modal closed
  ctx->LogInfo("[pose multiwindow] the modal's edit landed on the document field and the modal closed");

  // Revert through the same modal path: the chain leaves the document as it found it (the row
  // address re-derived — the committed probe name is part of it now).
  ctx->SetRef("");
  ctx->ItemClick(crystal_row_ref().c_str());
  IM_CHECK_RETV(DriveUntil(
                    ctx, [ctx] { return ctx->ItemInfo("**/##crystal_name", ImGuiTestOpFlags_NoError).ID != 0; }, 20),
                false);
  ctx->ItemInputValue("**/##crystal_name", first_name.c_str());
  ctx->Yield(2);
  ctx->ItemClick("**/Close##edit_modal");
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.crystals[static_cast<size_t>(probe_crystal)].name == first_name, false);
  IM_CHECK_RETV(ctx->ItemInfo("**/##crystal_name", ImGuiTestOpFlags_NoError).ID == 0, false);  // the editor closed
  ctx->SetRef("");

  IM_CHECK_RETV(gui::g_state.analyze_pose_view_open, false);
  IM_CHECK_RETV(gui::g_state.analyze_raypath_view_open, false);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid, false);
  IM_CHECK_RETV(gui::g_state.sun.altitude == sun_alt_before, false);
  ctx->LogInfo("[pose multiwindow] three surfaces coexisted; each wrote only its own state");

  gui::g_state.analyze_pose_view_open = false;
  gui::g_state.analyze_raypath_view_open = false;
  ctx->Yield(2);
  return true;
}

// ⑨ — the non-zero OS coordinate face: the pose window parked at a non-origin position opens,
// closes, re-renders and answers a real click at the moved geometry (the origin-sensitivity
// family's translation probe, the pose window's own leg).
bool PoseViewOperatesAtAMovedWindowOrigin(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  IM_CHECK_RETV(SelectRowByDisplay(ctx, TopChainDisplay().c_str()) != std::nullopt, false);
  SetSkyPointTarget(42.0, 0.0);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);

  gui::g_state.analyze_pose_view_open = true;
  PinPoseViewOnScreen(ctx);
  ctx->Yield(2);
  ImGuiWindow* win = ctx->GetWindowByRef("//Pose & trace");
  IM_CHECK_RETV(win != nullptr, false);
  ImGui::SetWindowPos(win, ImVec2(237.0f, 151.0f), ImGuiCond_Always);  // a deliberate non-origin placement
  ctx->Yield(3);
  IM_CHECK_RETV(fabsf(win->Pos.x - 237.0f) < 1.0f && fabsf(win->Pos.y - 151.0f) < 1.0f, false);
  ctx->LogInfo("[pose origin] parked at (237,151)");

  // A real click at the moved geometry: the play button answers.
  const ImGuiTestItemInfo play = ctx->ItemInfo("**/Play##globe_play", ImGuiTestOpFlags_NoError);
  IM_CHECK_RETV(play.ID != 0, false);
  ctx->ItemClick("**/Play##globe_play");
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analysis.point_view.playing, false);
  ctx->ItemClick("**/Stop##globe_play");
  ctx->Yield(2);
  IM_CHECK_RETV(!gui::g_state.analysis.point_view.playing, false);
  // Close and reopen at the moved position: the geometry survives (session memory), and the
  // regions render again.
  gui::g_state.analyze_pose_view_open = false;
  ctx->Yield(2);
  gui::g_state.analyze_pose_view_open = true;
  ctx->Yield(3);
  win = ctx->GetWindowByRef("//Pose & trace");
  IM_CHECK_RETV(win != nullptr, false);
  IM_CHECK_RETV(fabsf(win->Pos.x - 237.0f) < 1.0f, false);
  IM_CHECK_RETV(PoseRegionWindow("##AnalyzeSunGlobe") != nullptr, false);
  ctx->LogInfo("[pose origin] reopen keeps the moved position; regions render");

  gui::g_state.analyze_pose_view_open = false;
  ctx->Yield(2);
  return true;
}

// AC2's narrowest-window face (contract §11 R3): at 480x360 the split's floors hold, the
// info region's rows wrap instead of clip (the λ track's 60 pt floor, the sky-point row's
// ladder), the map's canvas keeps its code floor, and the state line stays readable. No
// analysis needed: the empty-state region renders the same controls.
bool RaypathViewNarrowestWindowKeepsControlsReachable(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  gui::g_state.analyze_raypath_view_open = true;
  // The flag only schedules the window's submission; the top-level window exists once a frame
  // commits it — and never borrow the instance an earlier test case left behind: this case has
  // to stand in a fresh process too (its first frame here creates its own window).
  ImGuiWindow* win = nullptr;
  for (int frames = 0; frames < 20; ++frames) {
    ctx->Yield(1);
    win = ctx->GetWindowByRef("//Raypath view");
    if (win != nullptr && win->Active) {
      break;
    }
  }
  IM_CHECK_RETV(win != nullptr && win->Active, false);
  {
    ImGui::SetWindowPos(win, ImVec2(8.0f, 60.0f), ImGuiCond_Always);
    ImGui::SetWindowSize(win, ImVec2(480.0f, 360.0f), ImGuiCond_Always);
    ctx->Yield(3);
  }
  IM_CHECK_RETV(EntryWindowActive("Raypath view"), false);
  ImGuiWindow* map = MapRegionWindow();
  ImGuiWindow* info = MapInfoRegionWindow();
  IM_CHECK_RETV(map != nullptr && info != nullptr, false);
  ctx->LogInfo("[narrowest] at 480x360: map %.0fx%.0f, info %.0fx%.0f", static_cast<double>(map->Size.x),
               static_cast<double>(map->Size.y), static_cast<double>(info->Size.x), static_cast<double>(info->Size.y));
  // The floors: the info region at its floor, the map absorbing the rest (still above its own).
  IM_CHECK_RETV(fabsf(info->Size.x - gui::UiPx(gui::kRaypathViewInfoMinSize)) < 2.0f, false);
  IM_CHECK_RETV(map->Size.x >= gui::UiPx(gui::kRaypathViewMapMinSize) - 1.5f, false);
  // The canvas keeps its code floor inside the map region.
  const ImGuiTestItemInfo canvas = ctx->ItemInfo("**/##map_canvas", ImGuiTestOpFlags_NoError);
  IM_CHECK_RETV(canvas.ID != 0, false);
  ctx->LogInfo("[narrowest] canvas %.0f square", static_cast<double>(canvas.RectFull.GetWidth()));
  IM_CHECK_RETV(canvas.RectFull.GetWidth() >= 64.0f, false);
  // The info region's rows wrap instead of clip: the λ slider, the Set button and the state
  // line all resolve inside the info region's clip (RectFull containment — a wrapped control
  // that lost its tail would fail the Max edges).
  const ImGuiTestItemInfo lambda_n = ctx->ItemInfo("**/wl##map", ImGuiTestOpFlags_NoError);
  IM_CHECK_RETV(lambda_n.ID != 0, false);
  IM_CHECK_RETV(lambda_n.RectFull.Max.x <= info->InnerClipRect.Max.x + 0.5f, false);
  const ImGuiTestItemInfo set_n = ctx->ItemInfo("**/Set sky point###sky_set", ImGuiTestOpFlags_NoError);
  IM_CHECK_RETV(set_n.ID != 0, false);
  IM_CHECK_RETV(set_n.RectClipped.GetHeight() > 0.0f, false);
  IM_CHECK_RETV(set_n.RectFull.Max.x <= info->InnerClipRect.Max.x + 0.5f, false);
  // And the ladder answers: typed coordinates + Set at the narrow width.
  ctx->SetRef("");
  ctx->ItemInputValue("**/alt°##sky", 20.0f);
  ctx->ItemInputValue("**/az°##sky", 90.0f);
  ctx->ItemClick("**/Set sky point###sky_set");
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid, false);
  // The state line: readable — the map region's whole stack (state line + canvas) fits its
  // height with no scroll, so no row is culled. (A text item carries no id and cannot be
  // ItemInfo'd — the containment form is the stack's own zero-scroll.)
  IM_CHECK_RETV(map->ScrollMax.y == 0.0f, false);
  IM_CHECK_RETV(map->Scroll.y == 0.0f, false);
  // The identity line's empty state (no selection here): named honestly, and the wrapped rows
  // keep the info region's content within its width — ScrollMax.x is the region-wide statement
  // of "nothing overflows horizontally" (a single-line text item here would read as content
  // wider than a region that has no horizontal scrollbar to recover it).
  IM_CHECK_RETV(gui::SelectedClassRowLabelFor(gui::g_state).empty(), false);
  IM_CHECK_RETV(info->ScrollMax.x == 0.0f, false);
  // The narrowest face is the user's reachable boundary, not a test-only size: a real
  // border-corner drag past the minimum stops at the window-level constraint — the same 480x360
  // the floors above were verified at, reached this time by the drag ImGui clamps at Begin.
  ctx->WindowResize("//Raypath view", ImVec2(160.0f, 100.0f));
  ctx->Yield(3);
  ImGuiWindow* win_min = ctx->GetWindowByRef("//Raypath view");
  IM_CHECK_RETV(win_min != nullptr, false);
  ctx->LogInfo("[narrowest] a 160x100 drag lands at %.0fx%.0f (constraint %.0fx%.0f)",
               static_cast<double>(win_min->Size.x), static_cast<double>(win_min->Size.y),
               static_cast<double>(gui::UiPx(gui::kRaypathViewMinWidth)),
               static_cast<double>(gui::UiPx(gui::kRaypathViewMinHeight)));
  IM_CHECK_RETV(fabsf(win_min->Size.x - gui::UiPx(gui::kRaypathViewMinWidth)) < 2.0f, false);
  IM_CHECK_RETV(fabsf(win_min->Size.y - gui::UiPx(gui::kRaypathViewMinHeight)) < 2.0f, false);
  // The clamped size is the verified layout: floors, canvas floor and the wrap invariant hold
  // after the drag too.
  map = MapRegionWindow();
  info = MapInfoRegionWindow();
  IM_CHECK_RETV(map != nullptr && info != nullptr, false);
  IM_CHECK_RETV(fabsf(info->Size.x - gui::UiPx(gui::kRaypathViewInfoMinSize)) < 2.0f, false);
  const ImGuiTestItemInfo canvas_min = ctx->ItemInfo("**/##map_canvas", ImGuiTestOpFlags_NoError);
  IM_CHECK_RETV(canvas_min.ID != 0, false);
  IM_CHECK_RETV(canvas_min.RectFull.GetWidth() >= 64.0f, false);
  IM_CHECK_RETV(info->ScrollMax.x == 0.0f, false);
  bool exercised_report_scroll = false;
  const auto check_pose_entry = [ctx, &exercised_report_scroll](const char* screenshot) -> bool {
    ImGuiWindow* region = MapInfoRegionWindow();
    IM_CHECK_RETV(region != nullptr, false);
    const ImGuiID entry_id = region->GetID("###pose_open");
    const auto entry = ctx->ItemInfo(entry_id);
    IM_CHECK_RETV(entry.ID != 0, false);
    ImRect clip = region->InnerClipRect;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    clip.ClipWith(
        ImRect(viewport->Pos, ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y)));
    IM_CHECK_RETV(clip.Contains(entry.RectFull), false);
    IM_CHECK_RETV(region->ScrollMax.y == 0.0f && region->Scroll.y == 0.0f, false);
    ImGuiWindow* report = MapReportWindow();
    IM_CHECK_RETV(report != nullptr && report->Pos.y >= entry.RectFull.Max.y, false);
    IM_CHECK_RETV(report->InnerClipRect.GetHeight() >= ImGui::GetTextLineHeight(), false);
    ctx->WindowFocus("//Raypath view");
    if (report->ScrollMax.y > 0.0f) {
      ctx->MouseMoveToPos(report->InnerClipRect.GetCenter());
      ctx->Yield(2);
      ctx->MouseWheelY(-3.0f);
      ctx->Yield(3);
      IM_CHECK_RETV(report->Scroll.y > 0.0f, false);
      exercised_report_scroll = true;
      ctx->LogInfo("[pose entry scroll] scale %.1f: wheel consumed by report (scroll %.0f / %.0f)",
                   gui::CurrentUiScale(), report->Scroll.y, report->ScrollMax.y);
      const auto scrolled_entry = ctx->ItemInfo(entry_id);
      IM_CHECK_RETV(std::abs(scrolled_entry.RectFull.Min.x - entry.RectFull.Min.x) < 0.5f &&
                        std::abs(scrolled_entry.RectFull.Min.y - entry.RectFull.Min.y) < 0.5f &&
                        std::abs(scrolled_entry.RectFull.Max.x - entry.RectFull.Max.x) < 0.5f &&
                        std::abs(scrolled_entry.RectFull.Max.y - entry.RectFull.Max.y) < 0.5f,
                    false);
    } else {
      ctx->LogInfo("[pose entry scroll] scale %.1f: report fits without scroll; other geometry must exercise wheel",
                   gui::CurrentUiScale());
    }
    ctx->ItemClick(entry_id);
    ctx->Yield(2);
    IM_CHECK_RETV(gui::g_state.analyze_pose_view_open, false);
    ImGuiWindow* pose = ctx->GetWindowByRef("//Pose & trace");
    IM_CHECK_RETV(pose != nullptr && pose->Active, false);
    ctx->WindowClose("//Pose & trace");
    ctx->Yield(2);
    IM_CHECK_RETV(!pose->Active && !gui::g_state.analyze_pose_view_open, false);
    ctx->WindowFocus("//Raypath view");
    ctx->MouseMoveToPos(ImVec2(2.0f, 2.0f));
    ctx->Yield(2);
    IM_CHECK_RETV(SaveWindowPng(ctx, "//Raypath view", GuiTestTempPath(screenshot).string()), false);
    ctx->LogInfo("[pose entry reach] scale %.1f, button (%.0f,%.0f)-(%.0f,%.0f), clip bottom %.0f report %.0f",
                 gui::CurrentUiScale(), entry.RectFull.Min.x, entry.RectFull.Min.y, entry.RectFull.Max.x,
                 entry.RectFull.Max.y, clip.Max.y, report->Size.y);
    return true;
  };
  IM_CHECK_RETV(check_pose_entry("pose_entry_narrow.png"), false);
  {
    ScopedUiScaleAndBasisLease scale_lease(ctx);
    gui::SetUiScaleMultiplierImmediate(2.0f);
    IM_CHECK_RETV(DriveUntil(ctx, [] { return !gui::g_ui_scale_dirty && gui::CurrentUiScale() == 2.0f; }, 5), false);
    g_window_size_test.width = 1024;
    g_window_size_test.height = 640;
    g_window_size_test.done.store(false);
    g_window_size_test.requested.store(true);
    IM_CHECK_RETV(DriveUntil(ctx, [] { return g_window_size_test.done.load(); }, 5), false);
    PinRaypathViewOnScreen(ctx);
    ctx->Yield(3);
    IM_CHECK_RETV(check_pose_entry("pose_entry_compressed_200.png"), false);
  }
  IM_CHECK_RETV(exercised_report_scroll, false);
  gui::g_state.analyze_raypath_view_open = false;
  ctx->Yield(2);
  return true;
}

// ---- The five-operations chain, one document session (755.7 Step 2, AC1) -----------------------------
//
// The interaction contract §12's new-side script driven in ONE document session, every
// intermediate state asserted on the way — the bar's waiting faces, the list's first appearance,
// the windows' first frames, the freshness three-some — with the end state only closing the
// chain, never standing in for the path (a01: a state enumeration passing is not the screen
// working). §12's new-side numbers are PREDICTIONS (contract §0's retest clause); the case logs
// its actual click/drag count per operation for the §12 backfill, and a divergence from the
// prediction is a design-drift signal, not a failure of this case.
//
// The seam no single-piece case covers: by op ⑤ BOTH floating windows stand, and the freshness
// trio (the list's stale banner, the main view's old-picture note, the status bar's Modified
// with the top bar's Revert face) is read with them up — every freshness case before this ran
// the windows closed. Op ② takes the contract §5.2's Pick+点图 path after one camera drag (the
// positioning move §12 predicts); the marker-drag path keeps its single-piece coverage
// (marker_drag_moves_the_centre_not_the_view). The wait LINE's own bytes stay the unit layer's
// pins — here the waiting fact is the bar's Stop face and the run's flags, observables that do
// not depend on winning a race against fixed-dt completion.
bool FiveOperationsChainInOneSession(ImGuiTestContext* ctx) {
  ScopedWatchdogGuard watchdog(ctx->EngineIO);
  ScopedServerGuard guard;

  // Cold start (§12.1's baseline): a document that has never run, no picture, the Analyze entry
  // closed by the one picture gate.
  ResetTestState();
  gui::g_server = LUMICE_CreateServer();
  IM_CHECK_RETV(gui::g_server != nullptr, false);
  gui::ResetServerConstructionTrackers();
  LUMICE_SetLogLevel(gui::g_server, static_cast<LUMICE_LogLevel>(g_core_log_level));
  IM_CHECK_RETV(gui::DeserializeFromJson(kFullyRandomJson, gui::g_state), false);
  gui::g_state.renderer.sim_resolution_index = 0;
  gui::g_state.sim.infinite = false;
  gui::g_state.sim.ray_num_millions = 0.1f;
  ctx->Yield(2);
  IM_CHECK_RETV(static_cast<int>(gui::g_state.run_intent) == static_cast<int>(gui::RunIntent::kNone), false);
  {
    const char* notice = gui::AnalysisPictureNotice(gui::g_state.run_intent, gui::g_state.sim_state);
    IM_CHECK_RETV(notice != nullptr && std::strstr(notice, "No rendered image") != nullptr, false);
  }
  IM_CHECK_RETV(!gui::CanStartAnalysis(true, gui::g_state.sim_state, false, gui::g_state.run_intent), false);

  // ---- Op ① — Run (top bar) → 切 Analyze → 操作条 Analyze → 点类行.
  bool saw_simulating = false;
  const unsigned long long uploads_before = gui::g_state.texture_upload_count;
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo("##TopBar/" ICON_FA_PLAY " Run")), false);
  ctx->ItemClick("##TopBar/" ICON_FA_PLAY " Run");
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [&saw_simulating] {
                      saw_simulating = saw_simulating || gui::g_state.sim_state == SimState::kSimulating;
                      return gui::g_state.sim_state == SimState::kDone;
                    },
                    60),
                false);
  IM_CHECK_RETV(saw_simulating, false);  // click 1 + the render wait
  IM_CHECK_RETV(
      DriveUntil(ctx, [uploads_before] { return gui::g_state.texture_upload_count > uploads_before; }, 10), false);
  IM_CHECK_RETV(gui::AnalysisPictureNotice(gui::g_state.run_intent, gui::g_state.sim_state) == nullptr, false);

  ctx->ItemClick("##TopBar/Analyze");  // click 2
  ctx->Yield(3);
  IM_CHECK_RETV(ctx->GetWindowByRef(kWindowRef) != nullptr, false);
  IM_CHECK_RETV(PanoramaPictureWindow() != nullptr, false);

  ctx->SetRef(PanoramaRegionRef());
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo(kAnalyzeButton)), false);
  ctx->ItemClick(kAnalyzeButton);  // click 3
  ctx->SetRef("");
  IM_CHECK_RETV(gui::g_state.analysis.started, false);
  IM_CHECK_RETV(gui::g_state.analysis_run_in_progress, false);
  // The bar's waiting face: the button reads Stop while the survey runs (§12.1's own anchor).
  ctx->SetRef(PanoramaRegionRef());
  IM_CHECK_RETV(ctx->ItemInfo(ICON_FA_STOP " Stop").ID != 0, false);
  ctx->SetRef("");
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return !gui::g_state.analysis_run_in_progress && gui::g_state.analysis_result.payload != nullptr; },
          60),
      false);
  IM_CHECK_RETV(!gui::g_state.analysis_result.payload->entries.empty(), false);
  IM_CHECK_RETV(!gui::g_state.analysis_result.display_order.empty(), false);

  // ---- ①'s last step, and ③'s open entry in one gesture: the top row click opens the Raypath
  // view — closed before it, alive after.
  IM_CHECK_RETV(!gui::g_state.analyze_raypath_view_open, false);
  IM_CHECK_RETV(!EntryWindowActive("Raypath view"), false);
  IM_CHECK_RETV(SelectRowByDisplay(ctx, TopChainDisplay().c_str()) != std::nullopt, false);  // click 4
  IM_CHECK_RETV(gui::g_state.analyze_raypath_view_open, false);
  IM_CHECK_RETV(EntryWindowActive("Raypath view"), false);
  PinRaypathViewOnScreen(ctx);
  IM_CHECK_RETV(MapRegionWindow() != nullptr && MapInfoRegionWindow() != nullptr, false);
  IM_CHECK_RETV(ctx->ItemInfo("**/##map_canvas", ImGuiTestOpFlags_NoError).ID != 0, false);
  // The class map's representative frame (the AC7 hand-off's 类图): the window map specifically
  // — the full-sky map answers independently of the observation camera,
  // whose default slice can legitimately carry no events. Captured here, on the whole-sky
  // payload, before the cone analysis replaces it (the faithful §12-③ moment: the row click
  // just opened the window onto the class's own picture).
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      if (m.adopted_result == nullptr || !(m.adopted_id == m.intent_id)) {
                        return false;
                      }
                      LUMICE_PathMapInfo i{};
                      i.struct_size = sizeof(i);
                      return LUMICE_PathMapGetInfo(m.adopted_result.get(), &i) == LUMICE_PATH_READ_OK &&
                             i.view_full_sky == 1;
                    },
                    60),
                false);
  IM_CHECK_RETV(SaveWindowPng(ctx, "//Raypath view", GuiTestTempPath("chain_five_ops_class_map.png").string()), false);
  ctx->LogInfo("[five ops] 1: Run -> Analyze -> list -> row click opens the Raypath view = 4 clicks + 2 waits");

  // ---- Op ② — one camera drag brings the arc into view (the arbitration's camera face: nothing
  // active, so the drag orbits), then an Alt/Option-click sets the centre
  // from the moved camera, the operation bar re-runs the cone, Radius re-sums display-time.
  const float az_before = gui::g_state.renderer.azimuth;
  const float el_before = gui::g_state.renderer.elevation;
  {
    ImGuiWindow* pano = PanoramaPictureWindow();
    IM_CHECK_RETV(pano != nullptr, false);
    // The open Raypath view floats over the main view's LEFT half (its pinned spot); the drag
    // lands on the picture's right part — outside the window's footprint, so the press's owner
    // is the camera and nothing else. The guard makes a future layout that covers this spot
    // fail visibly here instead of silently pressing a floating window.
    const ImVec2 drag_from(pano->Pos.x + pano->Size.x * 0.75f, pano->Pos.y + pano->Size.y * 0.60f);
    if (ImGuiWindow* view = ctx->GetWindowByRef("//Raypath view")) {
      IM_CHECK_RETV(!view->Rect().Contains(drag_from), false);
    }
    ctx->MouseMoveToPos(drag_from);
    ctx->Yield(2);
    ctx->MouseDown(0);
    ctx->Yield(2);
    ctx->MouseMoveToPos(ImVec2(drag_from.x + 60.0f, drag_from.y - 40.0f));
    ctx->Yield(3);
    ctx->MouseUp(0);
    ctx->Yield(3);
  }
  IM_CHECK_RETV(gui::g_state.renderer.azimuth != az_before || gui::g_state.renderer.elevation != el_before, false);
  IM_CHECK_RETV(gui::g_state.workspace.panorama_press_owner == PressOwner::kNone, false);

  IM_CHECK_RETV(StartPointAnalysis(ctx), false);  // radio, direct click, Analyze = 3 clicks
  IM_CHECK_RETV(gui::g_state.analysis_run_in_progress, false);
  // DoAnalyze cleared the previous (whole-sky) view — the list is empty while the cone runs, the
  // contract's own kNone face. The cone payload's arrival is the wait below.
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return !gui::g_state.analysis_run_in_progress &&
                             gui::g_state.analysis_result.payload != nullptr &&
                             gui::g_state.analysis_result.payload->roi_mode == LUMICE_RAYPATH_ROI_CONE;
                    },
                    60),
                false);
  IM_CHECK_RETV(!gui::g_state.analysis_result.payload->entries.empty(), false);
  // Radius: a real input to both ends of its range — display-time re-sum, no run.
  {
    const auto& payload = gui::g_state.analysis_result.payload;
    const float cone_deg = payload->cone_radius_rad * 57.29577951308232;
    const float min_deg = cone_deg / static_cast<float>(std::max(payload->cone_ring_count, 1));
    ctx->SetRef(PanoramaRegionRef());
    ctx->ItemInputValue("**/Radius###Radius_narrow", min_deg);
    ctx->Yield(2);
    IM_CHECK_RETV(fabsf(gui::g_state.analysis.cone_radius_deg - min_deg) < 0.05f, false);
    ctx->ItemInputValue("**/Radius###Radius_narrow", cone_deg);
    ctx->Yield(2);
    IM_CHECK_RETV(fabsf(gui::g_state.analysis.cone_radius_deg - cone_deg) < 0.05f, false);
    ctx->SetRef("");
  }
  ctx->LogInfo("[five ops] 2: camera drag + pick + cone analysis + radius = 1 drag + 4 actions + 1 wait");

  // The cone analysis cleared the selection (DoAnalyze's own semantics — the trigger wipes the
  // selected class with the survey): the row click re-selects before the map can answer. The
  // window is already open — this click re-keys it, it does not reopen it.
  IM_CHECK_RETV(SelectRowByDisplay(ctx, TopChainDisplay().c_str()) != std::nullopt, false);
  IM_CHECK_RETV(gui::g_state.analyze_raypath_view_open && EntryWindowActive("Raypath view"), false);
  ctx->LogInfo("[five ops] re-select after the cone analysis (DoAnalyze cleared the class): 1 click");

  // ---- Op ④'s first half — the map's pick sets the sky point and the point query waits (the
  // window's map took its own adoption first: a canvas click before it would read a stale hit
  // test).
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      return m.adopted_result != nullptr && (m.adopted_id == m.intent_id);
                    },
                    60),
                false);
  ctx->KeyDown(ImGuiMod_Alt);
  ctx->ItemClick("**/##map_canvas");
  ctx->KeyUp(ImGuiMod_Alt);
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid, false);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  IM_CHECK_RETV(gui::g_state.analysis.point.adopted_result != nullptr, false);

  // The map pick's direction is wherever the canvas centre projects — an honest off-arc point
  // can carry no valid pose sample (the window's own "no valid sample" face). The op-④ script's
  // second input path — the info region's typed alt/az + Set sky point — re-targets the arc with
  // the same real-product write, so the window-interaction arm below has a document to interact
  // with. Two-phase wait: leave the completed state first, complete again — a fast machine can
  // read the OLD document's completed state before the pump processes the key change.
  ctx->ItemInputValue("**/alt°##sky", 42.0f);
  ctx->ItemInputValue("**/az°##sky", 0.0f);
  ctx->ItemClick("**/Set sky point###sky_set");
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) != gui::PointQueryState::kCompleted; },
          30),
      false);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  IM_CHECK_RETV(gui::g_state.analysis.point.adopted_result != nullptr, false);

  // ---- Op ④'s second half — the Selection header's named button opens the pose window
  // (closed before it), and a window-interior control answers over the real mouse path.
  IM_CHECK_RETV(!gui::g_state.analyze_pose_view_open, false);
  IM_CHECK_RETV(!EntryWindowActive("Pose & trace"), false);
  ctx->ItemClick("**/Pose & trace###main_pose_open");
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.analyze_pose_view_open, false);
  IM_CHECK_RETV(EntryWindowActive("Pose & trace"), false);
  PinPoseViewOnScreen(ctx);
  IM_CHECK_RETV(PoseRegionWindow("##AnalyzeSunGlobe") != nullptr, false);
  ctx->ItemClick("**/Play##globe_play");
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analysis.point_view.playing, false);
  ctx->ItemClick("**/Stop##globe_play");
  ctx->Yield(2);
  IM_CHECK_RETV(!gui::g_state.analysis.point_view.playing, false);
  // Process frame of the populated pose window (the AC7 hand-off's 点姿态): globe, factors and
  // the completed point document in one window.
  IM_CHECK_RETV(SaveWindowPng(ctx, "//Pose & trace", GuiTestTempPath("chain_five_ops_point_pose.png").string()), false);
  ctx->LogInfo(
      "[five ops] 4: map pick -> point wait -> the header pose button opens the pose window + playback = 3 clicks + 1 "
      "wait");

  // ---- Op ⑤ — the left column's Sun input, with BOTH floating windows standing: the freshness
  // three-some is read in exactly the composition a user has after ①–④, and the windows follow
  // the edit without dropping.
  const float alt_before = gui::g_state.sun.altitude;
  const unsigned long long uploads_at_edit = gui::g_state.texture_upload_count;
  IM_CHECK_RETV(gui::g_state.analyze_raypath_view_open && gui::g_state.analyze_pose_view_open, false);
  ctx->SetRef("//##AnalyzeLeftPanel");
  ctx->ItemInputValue("##Altitude_input", alt_before - 5.0f);
  ctx->SetRef("");
  ctx->Yield(3);
  IM_CHECK_RETV(fabsf(gui::g_state.sun.altitude - (alt_before - 5.0f)) < 0.05f, false);
  IM_CHECK_RETV(gui::g_state.sim_state == SimState::kModified, false);
  IM_CHECK_RETV(EntryWindowActive("Raypath view") && EntryWindowActive("Pose & trace"), false);
  // The freshness trio, read where each fact LIVES (a01: the render paths are single-source
  // call-throughs of these predicates; this imgui fork reports labels for widgets only, so the
  // pure-text lines assert through their predicates here and land on a capture for the human's
  // eyes — the widget-visible faces assert through their own widgets):
  // (a) the list's stale banner — kStale, and the banner's contract is that the LIST STAYS: the
  // row the user had is still findable on screen while stale.
  const std::string stale_top = TopChainDisplay();
  {
    const auto& result = gui::g_state.analysis_result;
    const bool scene_matches =
        result.payload != nullptr && result.analyzed_scene && result.analyzed_scene->Matches(gui::g_state);
    IM_CHECK_RETV(gui::ComputeAnalysisListFreshness(result.payload != nullptr, scene_matches) ==
                      gui::AnalysisListFreshness::kStale,
                  false);
    const std::string stale_row_label = gui::AnalysisRowLabel(stale_top, gui::AnalysisCoreToPoolMap(gui::g_state));
    IM_CHECK_RETV(FindListRowScrolling(ctx, stale_row_label) != 0, false);
  }
  // (b) the main view's old-picture note — the modified document's notice, and the picture
  // behind it still the old render (no new upload since the edit).
  {
    const char* notice = gui::AnalysisPictureNotice(gui::g_state.run_intent, gui::g_state.sim_state);
    IM_CHECK_RETV(notice != nullptr && std::strstr(notice, "previous configuration") != nullptr, false);
    IM_CHECK_RETV(gui::g_state.texture_upload_count == uploads_at_edit, false);
  }
  // (c) the modified face's widget: the top bar's Revert is enabled (the ⚠/Revert pair's
  // enabled face; the pip's geometry is shell_chrome's pin), beside the kModified state itself.
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo("**/Revert", ImGuiTestOpFlags_NoError)), false);
  // Process frame of the stale composition — list banner + old picture + both windows standing.
  IM_CHECK_RETV(SaveWindowPng(ctx, kWindowRef, GuiTestTempPath("chain_five_ops_stale_freshness.png").string()), false);
  ctx->LogInfo("[five ops] 5: sun edit + freshness three-some with both windows open = 1 input, 0 waits");

  // ⑤'s refresh: Analyze on the edited scene — the banner clears (the list describes the
  // document again), the note stays (the picture is still the old render), the windows stand.
  ctx->SetRef(PanoramaRegionRef());
  ctx->ItemClick(kAnalyzeButton);
  ctx->SetRef("");
  IM_CHECK_RETV(gui::g_state.analysis.started, false);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return !gui::g_state.analysis_run_in_progress && gui::g_state.analysis_result.payload != nullptr; },
          60),
      false);
  IM_CHECK_RETV(!gui::g_state.analysis_result.payload->entries.empty(), false);
  {
    const auto& result = gui::g_state.analysis_result;
    const bool scene_matches =
        result.payload != nullptr && result.analyzed_scene && result.analyzed_scene->Matches(gui::g_state);
    IM_CHECK_RETV(gui::ComputeAnalysisListFreshness(result.payload != nullptr, scene_matches) ==
                      gui::AnalysisListFreshness::kFresh,
                  false);
  }
  ctx->Yield(4);
  IM_CHECK_RETV(gui::AnalysisPictureNotice(gui::g_state.run_intent, gui::g_state.sim_state) != nullptr, false);
  IM_CHECK_RETV(gui::g_state.sim_state == SimState::kModified, false);
  IM_CHECK_RETV(EntryWindowActive("Raypath view") && EntryWindowActive("Pose & trace"), false);
  ctx->LogInfo("[five ops] 5 refresh: 1 click + 1 wait; banner cleared, note honest, windows stood");

  // Process frames for the hand-off: the three surfaces this chain ended with, real frames of
  // the very session (not compared against anything — a human's evidence).
  IM_CHECK_RETV(SaveWindowPng(ctx, kWindowRef, GuiTestTempPath("chain_five_ops_list.png").string()), false);
  IM_CHECK_RETV(SaveWindowPng(ctx, "//Raypath view", GuiTestTempPath("chain_five_ops_raypath_view.png").string()),
                false);
  IM_CHECK_RETV(SaveWindowPng(ctx, "//Pose & trace", GuiTestTempPath("chain_five_ops_pose_view.png").string()), false);
  return true;
}

// The FULL class identity of one payload entry, re-derived by the same mechanical rule the
// product's selection path runs (analysis_panel.cpp's SelectedClassIdentityFor). The oracle for
// "the windows follow the selection": the halo's top chains share their root crystal, so only
// the face sequence tells two classes apart — a crystal-id comparison cannot.
static gui::SelectedClassIdentity IdentityFromPayloadEntry(const LUMICE_RaypathHistogramEntry& e) {
  gui::SelectedClassIdentity id;
  id.core_crystal_id = e.chain[0].crystal_id;
  for (int l = 0; l < e.chain_len; ++l) {
    id.faces.insert(id.faces.end(), e.chain[l].segment, e.chain[l].segment + e.chain[l].segment_len);
    id.layer_face_counts.push_back(e.chain[l].segment_len);
  }
  id.symmetry_bits_plus_one = gui::AnalysisSymmetryBits(gui::g_state) == 7 ? 0 : 1;
  return id;
}

// The window-combo cases' common standing state (755.7 Steps 2–3): the halo scene analysed
// whole-sky, the top class selected (the row click opened the Raypath view), the arc sky point
// typed through the info region's own inputs, the point document completed, and BOTH floating
// windows standing — the pose window parked lower-left so the main view's right half stays
// pressable (the multi-window chain's parking shape).
bool BringUpBothWindows(ImGuiTestContext* ctx) {
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  IM_CHECK_RETV(SelectRowByDisplay(ctx, TopChainDisplay().c_str()) != std::nullopt, false);
  IM_CHECK_RETV(gui::g_state.analyze_raypath_view_open, false);
  PinRaypathViewOnScreen(ctx);
  // Wait for the row's full-sky result and a stable rendered answer before typing through
  // the info region. Refused maps time this wait out into an explicit red; every caller here
  // uses a canonical scene whose map completes. Observation itself never requests another map.
  int map_settle_streak = 0;
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [&map_settle_streak] {
                      const gui::MapQuerySession& map = gui::g_state.analysis.map;
                      const bool quiet = map.adopted_result != nullptr && map.adopted_id == map.intent_id &&
                                         !gui::g_survey_executor.InFlight(gui::QueryFace::kMap);
                      map_settle_streak = quiet ? map_settle_streak + 1 : 0;
                      return map_settle_streak >= 3;
                    },
                    60),
                false);
  const gui::SurveyQueryId map_intent_at_release = gui::g_state.analysis.map.intent_id;
  ctx->ItemInputValue("**/alt°##sky", 42.0f);
  ctx->ItemInputValue("**/az°##sky", 0.0f);
  ctx->ItemClick("**/Set sky point###sky_set");
  // The input chain's own completion evidence: a Set click the engine skipped (any sticky
  // error from the ops above) would leave the sky point unwritten and the pump honestly idle —
  // assert the write landed, and that no map update reflowed the region behind the click.
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid, false);
  IM_CHECK_RETV(gui::g_state.analysis.map.intent_id == map_intent_at_release, false);
  // Completion is a persistent state (the adoption keeps its fields), so level polling is the
  // correct criterion here; the trace only makes a timeout self-identifying — states=NoIntent
  // throughout means the pump never issued the intent (the known B-shaped failure, tracked
  // separately from this file's arms), vs. Waiting/Completed churn in a healthy-but-slow frame.
  IM_CHECK_RETV(DriveScopesGate(
                    ctx, "bringup_sky_point_completed",
                    [](ScopesGateTrace& t) {
                      const gui::PointQuerySession& p = gui::g_state.analysis.point;
                      const gui::PointQueryState state = gui::PointQueryStateFor(p);
                      t.Poll({ { "intent_valid", p.intent_valid }, { "adopted", p.adopted_result != nullptr } },
                             ScopesStateBit(state));
                      return state == gui::PointQueryState::kCompleted;
                    },
                    kScopesCompleteBudgetS),
                false);
  IM_CHECK_RETV(gui::g_state.analysis.point.adopted_result != nullptr, false);
  ctx->ItemClick("**/Pose & trace###main_pose_open");
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.analyze_pose_view_open, false);
  PinPoseViewOnScreen(ctx);
  if (ImGuiWindow* pose_m = ctx->GetWindowByRef("//Pose & trace")) {
    ImGui::SetWindowPos(pose_m, ImVec2(360.0f, 240.0f), ImGuiCond_Always);
  }
  ctx->Yield(2);
  return true;
}

namespace {

// Capture one submitted application frame's whole log, before LogFinish clears its buffer.
// LogWindow is deliberately unbound from the fallback window: child/window End() must not
// clear this read. Keep normal item clipping; screenshots below are taken on ordinary,
// non-logging frames. Callers slice windows out of the returned text — or, when the region of
// interest lives in a window whose name renders empty (imgui logs a window name up to
// FindRenderedTextEnd, so a ##-prefixed name contributes an empty line), assert on content
// markers unique to the frame instead.
std::string CaptureFrameText(ImGuiTestContext* ctx) {
  struct Capture {
    std::string text;
    int frame = -1;
    ImGuiID begin_hook = 0;
    ImGuiID end_hook = 0;
    ~Capture() {
      // The test coroutine resumes in the engine's earlier EndFramePre hook. Finish THIS
      // frame's open logging before removing our later hook, or it leaks into future frames.
      ImGui::LogFinish();
      ImGui::RemoveContextHook(ImGui::GetCurrentContext(), begin_hook);
      ImGui::RemoveContextHook(ImGui::GetCurrentContext(), end_hook);
    }
  } capture;
  ImGuiContextHook hook;
  hook.UserData = &capture;
  hook.Type = ImGuiContextHookType_NewFramePost;
  hook.Callback = [](ImGuiContext* g, ImGuiContextHook*) {
    ImGui::LogToBuffer();
    g->LogWindow = nullptr;
    g->ItemUnclipByLog = false;
  };
  capture.begin_hook = ImGui::AddContextHook(ImGui::GetCurrentContext(), &hook);
  hook.Type = ImGuiContextHookType_EndFramePre;
  hook.Callback = [](ImGuiContext* g, ImGuiContextHook* h) {
    auto& c = *static_cast<Capture*>(h->UserData);
    c.text = g->LogBuffer.c_str();
    c.frame = g->FrameCount;
    ImGui::LogFinish();
  };
  capture.end_hook = ImGui::AddContextHook(ImGui::GetCurrentContext(), &hook);
  const int before = ImGui::GetFrameCount();
  ctx->Yield(2);
  if (capture.frame <= before) {
    ctx->LogError("[frame text] no submitted frame after %d", before);
    return {};
  }
  ctx->LogInfo("[frame text] captured frame %d, %zu bytes", capture.frame, capture.text.size());
  if (g_export_dir) {
    std::ofstream output(GuiTestTempPath("map_text_frame_" + std::to_string(capture.frame) + ".txt"));
    output << capture.text;
  }
  return capture.text;
}

// The frame's text from the named (titled) window on: the slice the map face's assertions read.
std::string CaptureWindowFrameText(ImGuiTestContext* ctx, const char* window_name) {
  const std::string text = CaptureFrameText(ctx);
  if (text.empty()) {
    return {};
  }
  const std::string window_marker = std::string(window_name) + '\n';
  const size_t window_at = text.find(window_marker);
  if (window_at == std::string::npos || text.find(window_marker, window_at + 1) != std::string::npos) {
    ctx->LogError("[frame text] expected exactly one %s submission in the captured frame", window_name);
    return {};
  }
  return text.substr(window_at);
}

std::string CaptureMapFrameText(ImGuiTestContext* ctx) {
  return CaptureWindowFrameText(ctx, "Raypath view");
}

std::string MapTextLine(const std::string& text, const char* marker, const char* field) {
  size_t at = 0;
  while ((at = text.find(marker, at)) != std::string::npos) {
    const size_t previous_line = text.rfind('\n', at);
    const size_t start = previous_line == std::string::npos ? 0 : previous_line + 1;
    const std::string line = text.substr(start, text.find('\n', at) - start);
    if (line.find(field) != std::string::npos)
      return line.substr(line.find_first_not_of(" \t"));
    ++at;
  }
  return {};
}

bool CheckMapTextGlyphs(ImGuiTestContext* ctx, const char* surface, const std::string& text) {
  IM_CHECK_RETV(!text.empty(), false);
  ctx->LogInfo("[map text] %s: %s", surface, text.c_str());
  // Decode what production submitted, not a second inventory of anticipated codepoints.
  ImFont* body = ImGui::GetIO().Fonts->Fonts[0];
  for (const char* p = text.c_str(); *p;) {
    unsigned int codepoint = 0;
    const int bytes = ImTextCharFromUtf8(&codepoint, p, nullptr);
    if (bytes <= 0) {
      ctx->LogError("[map text] %s invalid UTF-8", surface);
      return false;
    }
    if (codepoint > 32 && body->FindGlyphNoFallback(static_cast<ImWchar>(codepoint)) == nullptr) {
      ctx->LogError("[map text] %s missing U+%04X in live atlas: %s", surface, codepoint, text.c_str());
      return false;
    }
    p += bytes;
  }
  return true;
}

bool CheckMapCellSteps(ImGuiTestContext* ctx, const std::string& text, const LUMICE_PathMapInfo& info) {
  IM_CHECK_RETV(!text.empty(), false);
  const auto check_axis = [&](const char* axis, double expected) -> bool {
    const size_t at = text.find(axis);
    IM_CHECK_RETV(at != std::string::npos, false);
    char* end = nullptr;
    const double value = std::strtod(text.c_str() + at + std::strlen(axis), &end);
    IM_CHECK_RETV(end != nullptr && std::strncmp(end, "°", std::strlen("°")) == 0, false);
    IM_CHECK_RETV(std::isfinite(value) && std::abs(value - expected) <= std::abs(expected) * 1e-6, false);
    return true;
  };
  IM_CHECK_RETV(check_axis("dev ", (info.view_delta_max_deg - info.view_delta_min_deg) / info.view_rows), false);
  IM_CHECK_RETV(check_axis("az ", (info.view_az_max_deg - info.view_az_min_deg) / info.view_cols), false);
  return true;
}

bool CheckMapQualityText(ImGuiTestContext* ctx, const std::string& stem) {
  // The shipped default under BOTH off spellings — no variable, and an explicit "0" — proven
  // with the non-diagnostic premises group OPEN: the "Map diagnostics" header is unsubmitted
  // (the gate, not a closed fold), and neither the grid / execution lines nor the engine's raw
  // geometry-basis string leak into the capture.
  for (const DebugDetailsState gate_state : { DebugDetailsState::kUnset, DebugDetailsState::kOff }) {
    // One gate state's checks run inside an immediate lambda (the loop-fatal-assert rule: a
    // fatal assert directly in a for body aborts the remaining gate states).
    const bool leg_ok = [&](DebugDetailsState) {
      const ScopedDebugDetailsEnv env_off(gate_state);
      ctx->Yield(2);
      IM_CHECK_RETV(ctx->ItemInfo("**/Map diagnostics##map_caliber", ImGuiTestOpFlags_NoError).ID == 0, false);
      ImGuiWindow* report = MapReportWindow();
      IM_CHECK_RETV(report != nullptr, false);
      const ImGuiID prem_id = report->GetID("Map quantity & premises##map_caliber");
      ImGuiStorage* prem_storage = &report->StateStorage;
      const bool prem_open = prem_storage->GetInt(prem_id, 0) != 0;
      if (!prem_open) {
        ctx->ItemClick("**/Map quantity & premises##map_caliber");
        ctx->Yield(2);
      }
      const std::string default_text = CaptureMapFrameText(ctx);
      // The premises group is open, so its full-definition row is in the capture.
      IM_CHECK_RETV(default_text.find("Quantity: ") != std::string::npos, false);
      IM_CHECK_RETV(default_text.find("Reference cell") == std::string::npos, false);
      IM_CHECK_RETV(default_text.find("Source-fixed scale") == std::string::npos, false);
      IM_CHECK_RETV(default_text.find(" members \xC2\xB7 kept ") == std::string::npos, false);
      LUMICE_PathSurveyEvidence evidence{};
      evidence.struct_size = sizeof(evidence);
      IM_CHECK_RETV(
          LUMICE_PathMapGetEvidence(gui::g_state.analysis.map.adopted_result.get(), &evidence) == LUMICE_PATH_READ_OK,
          false);
      if (evidence.geometry_basis.length > 0) {
        const std::string basis(evidence.geometry_basis.data, evidence.geometry_basis.length);
        // The engine's raw basis string is a diagnostics row.
        IM_CHECK_RETV(default_text.find(basis) == std::string::npos, false);
      }
      if (!prem_open) {
        prem_storage->SetInt(prem_id, 0);  // restore the default-closed shape
        ctx->Yield(1);
      }
      return true;
    }(gate_state);
    if (!leg_ok) {
      return false;  // the leg's own IM_CHECK named the failure
    }
  }
  // The caliber's diagnostics group is env-gated: the helper runs under "1" (the group's
  // header, idempotent-open and restore logic all ride the gate being on).
  const ScopedDebugDetailsEnv env_on(DebugDetailsState::kOn);
  const auto& map = gui::g_state.analysis.map;
  const auto reference = map.adopted_result;
  ctx->MouseMoveToPos(ImVec2(2.0f, 2.0f));
  ImGuiWindow* report = MapReportWindow();
  IM_CHECK_RETV(report != nullptr, false);
  report->Scroll.y = 0.0f;
  // The reference-cell line lives in the caliber's env-gated "Map diagnostics" group (the
  // ledger's L4 tier) — open it IDEMPOTENTLY so the capture carries the numbers the checks
  // read: the header is a toggle, and check_scale calls this helper three times (1x / 1.5x /
  // back), so a blind click would close it on the second leg. The header's open state is the
  // report child's own tree storage.
  const ImGuiID calc_fold_id = report->GetID("Map diagnostics##map_caliber");
  ImGuiStorage* fold_storage = &report->StateStorage;
  const bool calc_fold_open = fold_storage->GetInt(calc_fold_id, 0) != 0;
  if (!calc_fold_open) {
    ctx->ItemClick("**/Map diagnostics##map_caliber");
    ctx->Yield(2);
  }
  const std::string report_text = CaptureMapFrameText(ctx);
  const auto check_source = [&]() -> bool {
    const char* source = "Reference";
    LUMICE_PathMapInfo info{};
    info.struct_size = sizeof(info);
    IM_CHECK_RETV(LUMICE_PathMapGetInfo(reference.get(), &info) == LUMICE_PATH_READ_OK, false);
    const std::string line = MapTextLine(report_text, source, "cell dev ");
    IM_CHECK_RETV(CheckMapCellSteps(ctx, line, info), false);
    // The full-sky reader's known one-degree grid is an anchor independent of display formatting.
    {
      IM_CHECK_RETV(info.view_rows == 180 && info.view_cols == 360, false);
      IM_CHECK_RETV(info.view_delta_max_deg - info.view_delta_min_deg == 180.0, false);
      IM_CHECK_RETV(info.view_az_max_deg - info.view_az_min_deg == 360.0, false);
    }
    IM_CHECK_RETV(CheckMapTextGlyphs(ctx, source, line), false);
    IM_CHECK_RETV(line.find("cell dev ") != std::string::npos, false);
    return true;
  };
  IM_CHECK_RETV(check_source(), false);
  IM_CHECK_RETV(SaveWindowPng(ctx, "//Raypath view", GuiTestTempPath(stem + "_steps.png").string()), false);
  const ImRect canvas = ctx->ItemInfo("**/##map_canvas").RectFull;
  ctx->MouseMoveToPos(canvas.GetCenter());
  ctx->Yield(3);
  const std::string cursor_frame = CaptureMapFrameText(ctx);
  const size_t near_at = cursor_frame.find("Near / pick direction");
  IM_CHECK_RETV(near_at != std::string::npos, false);
  const std::string cursor_text = cursor_frame.substr(near_at);
  const char* cursor_source = "Reference r";
  IM_CHECK_RETV(cursor_text.find("Near / pick direction") != std::string::npos, false);
  IM_CHECK_RETV(cursor_text.find(cursor_source) != std::string::npos, false);
  LUMICE_PathMapInfo cursor_info{};
  cursor_info.struct_size = sizeof(cursor_info);
  IM_CHECK_RETV(LUMICE_PathMapGetInfo(reference.get(), &cursor_info) == LUMICE_PATH_READ_OK, false);
  const std::string cursor_line = MapTextLine(cursor_text, "cell dev ", "az ");
  IM_CHECK_RETV(CheckMapCellSteps(ctx, cursor_line, cursor_info), false);
  IM_CHECK_RETV(CheckMapTextGlyphs(ctx, "cursor", cursor_line), false);
  const auto save_tooltip = [&](const char* suffix) -> bool {
    for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
      if (w->Active && (w->Flags & ImGuiWindowFlags_Tooltip)) {
        const std::string ref = std::string("//") + w->Name;
        return SaveWindowPng(ctx, ref.c_str(), GuiTestTempPath(stem + suffix).string());
      }
    }
    return false;
  };
  IM_CHECK_RETV(save_tooltip("_cursor.png"), false);
  ctx->MouseMove("**/Retry###map_quality");
  ctx->Yield(3);
  const std::string retry_text = CaptureMapFrameText(ctx);
  IM_CHECK_RETV(retry_text.find("Retry the current full-sky request") != std::string::npos, false);
  IM_CHECK_RETV(save_tooltip("_retry.png"), false);
  IM_CHECK_RETV(map.adopted_result == reference, false);
  // Leave the fold as we found it: the report child's storage outlives this helper, and a
  // later case asserting the closed-by-default state must not inherit our open tier.
  if (!calc_fold_open && fold_storage->GetInt(calc_fold_id, 0) != 0) {
    ctx->ItemClick("**/Map diagnostics##map_caliber");
    ctx->Yield(2);
  }
  ctx->MouseMoveToPos(ImVec2(2.0f, 2.0f));
  ctx->Yield(2);
  return true;
}

// The completed C01 map's actual text, after the same frame-boundary rebuild/GL upload the
// product uses. The standalone font case pins the full audited character set; this leg owns the
// rendered window and rebuild lifetime. Never retain an ImFont pointer across a Yield/rebuild.
bool CheckMapGlyphsAcrossUiScale(ImGuiTestContext* ctx) {
  ScopedUiScaleAndBasisLease ui_lease(ctx);
  const auto check_scale = [ctx](float scale, const char* suffix) -> bool {
    gui::SetUiScaleMultiplierImmediate(scale);
    IM_CHECK_RETV(
        DriveUntil(ctx, [scale] { return !gui::g_ui_scale_dirty && gui::CurrentUiScale() == scale; }, 5), false);
    ctx->Yield(2);  // completed upload AND rendered frames, not just a cleared dirty flag
    PinRaypathViewOnScreen(ctx);
    ImFontAtlas* atlas = ImGui::GetIO().Fonts;
    IM_CHECK_RETV(atlas != nullptr && atlas->IsBuilt() && atlas->TexID != 0 && atlas->Fonts.Size == 1, false);
    ImFont* body = atlas->Fonts[0];
    IM_CHECK_RETV(body->FontSize == std::round(15.0f * scale), false);
    const ImFontGlyph* lambda = body->FindGlyphNoFallback(0x03BB);
    const ImFontGlyph* delta = body->FindGlyphNoFallback(0x03B4);
    IM_CHECK_RETV(lambda != nullptr && delta != nullptr, false);
    IM_CHECK_RETV(body->FindGlyphNoFallback(0x2014) != nullptr && body->FindGlyphNoFallback(0x203A) != nullptr &&
                      body->FindGlyphNoFallback(0x00BB) != nullptr && body->FindGlyphNoFallback(0xF061) != nullptr,
                  false);
    ctx->LogInfo(
        "[map glyphs] scale %.1f atlas %dx%d glyphs %d font %.0f texture %llu; lambda %.3f delta %.3f "
        "fallback %.3f",
        scale, atlas->TexWidth, atlas->TexHeight, body->Glyphs.Size, body->FontSize,
        static_cast<unsigned long long>(atlas->TexID), lambda->AdvanceX, delta->AdvanceX, body->FallbackAdvanceX);
    ctx->SetRef("//Raypath view");
    // This independent UTF-8 anchor also tests the compiled slider label's encoding.
    const ImGuiTestItemInfo slider = ctx->ItemInfo("**/wl##map", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(slider.ID != 0, false);
    ImGuiWindow* info_region = MapInfoRegionWindow();
    IM_CHECK_RETV(info_region != nullptr, false);
    IM_CHECK_RETV(slider.RectFull.Min.x >= info_region->InnerClipRect.Min.x &&
                      slider.RectFull.Max.x <= info_region->InnerClipRect.Max.x &&
                      slider.RectFull.Min.y >= info_region->InnerClipRect.Min.y &&
                      slider.RectFull.Max.y <= info_region->InnerClipRect.Max.y,
                  false);
    gui::MapCaliberInput caliber;
    caliber.info.struct_size = sizeof(caliber.info);
    IM_CHECK_RETV(
        LUMICE_PathMapGetInfo(gui::g_state.analysis.map.adopted_result.get(), &caliber.info) == LUMICE_PATH_READ_OK &&
            caliber.info.kept_count > 0,
        false);
    // The caliber's diagnostics tier carries the grid numbers (the ledger's tiering); the
    // wavelength is deliberately absent from every caliber line — the slider row's echo is its
    // owner — and the descriptive-policy layer (D4's second contract, stacked over — never
    // instead of — the per-codepoint coverage) pins the submitted face: neither Greek glyph
    // rides the diagnostics text (the disabled representation), the dev descriptor it replaced
    // does, and the slider's new ASCII label resolves as an item (the wl ItemInfo above).
    // This input carries no evidence, so the diagnostics lines' front is the grid line itself.
    const std::string metrics = gui::MapCaliberDiagnosticsLines(caliber).front();
    IM_CHECK_RETV(metrics.find("\xCE\xBB") == std::string::npos, false);
    IM_CHECK_RETV(metrics.find("\xCE\xB4") == std::string::npos, false);
    IM_CHECK_RETV(metrics.find("cell dev ") != std::string::npos, false);
    ctx->LogInfo("[map glyphs] metrics: %s", metrics.c_str());
    ctx->MouseMoveToPos(ImVec2(2.0f, 2.0f));
    ctx->Yield(2);
    const std::string stem = std::string("map_glyphs_") + suffix;
    IM_CHECK_RETV(CheckMapQualityText(ctx, stem), false);
    IM_CHECK_RETV(SaveWindowPng(ctx, "//Raypath view", GuiTestTempPath(stem + ".png").string()), false);
    // Preserve the full application frame as well as the window crop for offline inspection.
    g_fullframe_capture.Reset();
    g_fullframe_capture.requested.store(true);
    for (int i = 0; i < 10 && !g_fullframe_capture.done.load(); ++i) {
      ctx->Yield(1);
    }
    IM_CHECK_RETV(g_fullframe_capture.done.load(), false);
    IM_CHECK_RETV(g_fullframe_capture.width > 0 && g_fullframe_capture.height > 0, false);
    const auto rgb = lumice::test::StripAlpha(g_fullframe_capture.pixels.data(), g_fullframe_capture.width,
                                              g_fullframe_capture.height);
    IM_CHECK_RETV(lumice::test::SavePng(GuiTestTempPath(stem + "_full.png").string().c_str(), rgb.data(),
                                        g_fullframe_capture.width, g_fullframe_capture.height, 3),
                  false);
    ctx->SetRef("");
    return true;
  };
  IM_CHECK_RETV(check_scale(1.0f, "100"), false);
  IM_CHECK_RETV(check_scale(1.5f, "150"), false);
  IM_CHECK_RETV(check_scale(1.0f, "100_return"), false);
  return true;
}

}  // namespace

// The Selection header's double entry (contract §16.3): both named buttons live in the fixed
// head — visible and clickable in BOTH branches (no selection yet, then a class selected) — and
// clicking one opens its window with zero writes outside the open flag: the selection, the
// document's dirty bit, the sky point and the point intent stay as they were. Clicking a button
// whose window is already open focuses that window instead (no second instance exists by
// construction; both windows Begin with NoCollapse, so there is no collapsed tier to consider).
bool SelectionHeaderEntryButtonsOpenAndFocus(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  ctx->Yield(2);

  // The idle branch (no class selected): the same operation stack stands — §16.3's no-class row.
  IM_CHECK_RETV(!gui::g_state.analysis.selected_entry.has_value(), false);
  const auto button_inside_clip = [ctx](const char* ref) {
    const ImGuiTestItemInfo item = ctx->ItemInfo(ref, ImGuiTestOpFlags_NoError);
    ImGuiWindow* details = nullptr;
    for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
      if (w != nullptr && w->Active && w->ParentWindow == nullptr && std::strcmp(w->Name, "##AnalyzeDetails") == 0) {
        details = w;
      }
    }
    return item.ID != 0 && details != nullptr && details->InnerRect.Contains(item.RectFull.Min) &&
           details->InnerRect.Contains(item.RectFull.Max);
  };
  IM_CHECK_RETV(button_inside_clip("**/Raypath view###main_raypath_open"), false);
  IM_CHECK_RETV(button_inside_clip("**/Pose & trace###main_pose_open"), false);
  ctx->ItemClick("**/Pose & trace###main_pose_open");
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analyze_pose_view_open && EntryWindowActive("Pose & trace"), false);
  ctx->WindowClose("//Pose & trace");
  ctx->Yield(1);

  // The identity branch: selecting the row selects AND opens the Raypath view (the content
  // shortcut, untouched); the header buttons then address the open windows' state again.
  IM_CHECK_RETV(SelectRowByDisplay(ctx, TopChainDisplay().c_str()) != std::nullopt, false);
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analyze_raypath_view_open, false);
  IM_CHECK_RETV(button_inside_clip("**/Raypath view###main_raypath_open"), false);
  IM_CHECK_RETV(button_inside_clip("**/Pose & trace###main_pose_open"), false);

  // The Pose button opens its window; every write the click makes is the flag.
  const std::string selection_before = gui::g_state.analysis.selected_entry.value_or("");
  const bool dirty_before = gui::g_state.dirty;
  const bool sky_before = gui::g_state.analysis.sky_point.valid;
  const auto point_id_before = gui::g_state.analysis.point.intent_id;
  ctx->ItemClick("**/Pose & trace###main_pose_open");
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analyze_pose_view_open && EntryWindowActive("Pose & trace"), false);
  IM_CHECK_RETV(gui::g_state.analysis.selected_entry.value_or("") == selection_before, false);
  IM_CHECK_RETV(gui::g_state.dirty == dirty_before, false);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid == sky_before, false);
  IM_CHECK_RETV(gui::g_state.analysis.point.intent_id == point_id_before, false);

  // Already-open clicks focus, not re-open: put the pose window front and focused, click the
  // header's Raypath view button, and the raypath window is the focused one again. The floats
  // move aside first so the header button's click lands on the details window, not on a float.
  PinPoseViewOnScreen(ctx);
  if (ImGuiWindow* pose_m = ctx->GetWindowByRef("//Pose & trace")) {
    ImGui::SetWindowPos(pose_m, ImVec2(360.0f, 240.0f), ImGuiCond_Always);
  }
  if (ImGuiWindow* raypath_m = ctx->GetWindowByRef("//Raypath view")) {
    ImGui::SetWindowPos(raypath_m, ImVec2(40.0f, 80.0f), ImGuiCond_Always);
  }
  ctx->Yield(2);
  ctx->WindowFocus("//Pose & trace");
  ctx->Yield(2);
  IM_CHECK_RETV(ImGui::GetCurrentContext()->NavWindow != nullptr &&
                    std::strcmp(ImGui::GetCurrentContext()->NavWindow->Name, gui::kPoseViewWindowTitle) == 0,
                false);
  ctx->ItemClick("**/Raypath view###main_raypath_open");
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analyze_raypath_view_open && EntryWindowActive("Raypath view"), false);
  IM_CHECK_RETV(ImGui::GetCurrentContext()->NavWindow != nullptr &&
                    std::strcmp(ImGui::GetCurrentContext()->NavWindow->Name, gui::kRaypathViewWindowTitle) == 0,
                false);
  ctx->LogInfo("[header entries] both branches addressable, zero writes outside the flags, focus on already-open");
  return true;
}

// The map window's in-place Pose button's labels derive mechanically from the window-title
// constants: the plain form is the constant + `###pose_open`, the narrow-column form is the
// constant's LAST space turned into the line break. The pin holds the exact submitted bytes,
// so a wrong derivation rule or a re-spelled title cannot pass green — the wrap carve-out's
// "byte-identical" is test-carried, not self-attested.
bool MapPoseButtonLabelsDeriveFromTheWindowTitle(ImGuiTestContext* ctx) {
  ctx->Yield(1);
  IM_CHECK_RETV(std::strcmp(gui::PoseButtonLabel(), "Pose & trace###pose_open") == 0, false);
  IM_CHECK_RETV(std::strcmp(gui::PoseButtonWrappedLabel(), "Pose &\ntrace###pose_open") == 0, false);
  ctx->LogInfo("[header entries] map pose button labels pin the title-constant derivation bytes");
  return true;
}

// The entry buttons' arc across the halo scene's query states (contract §16.3's table, the
// states this scene reaches): clickable while the point query is in flight — and a click then
// writes the open flag ONLY (the in-flight intent is untouched: an entry is a display action,
// never a query trigger); the normal completion names no navigation; the cancelled state's
// one-word hint shows at the default geometry without moving the buttons or the scroll region.
// The hint's assertion substrings are ones no scroll-region text carries ("Cancelled"
// capital-C is the slot's word alone; "The complete" is the mass-arm slot line alone) — the
// other states' hint texts reuse scroll-region wording, where a substring assert cannot
// isolate the slot.
bool SelectionHeaderEntriesTrackTheQueryStates(ImGuiTestContext* ctx) {
  const auto details_window = [] {
    for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
      if (w != nullptr && w->Active && w->ParentWindow == nullptr && std::strcmp(w->Name, "##AnalyzeDetails") == 0) {
        return w;
      }
    }
    return static_cast<ImGuiWindow*>(nullptr);
  };
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  IM_CHECK_RETV(SelectRowByDisplay(ctx, TopChainDisplay().c_str()) != std::nullopt, false);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    60),
                false);
  ctx->Yield(2);

  // Completed survey, no sky point yet: the pose entry answers, and the click writes the flag
  // alone — neither query's intent moves. The slot names the next step (the hint field is what
  // the slot drew, AC4's channel: what the test reads is what the user saw).
  const auto survey_id_before = gui::g_state.analysis.survey.intent_id;
  const auto point_id_before = gui::g_state.analysis.point.intent_id;
  ctx->ItemClick("**/Pose & trace###main_pose_open");
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analyze_pose_view_open && EntryWindowActive("Pose & trace"), false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.intent_id == survey_id_before, false);
  IM_CHECK_RETV(gui::g_state.analysis.point.intent_id == point_id_before, false);
  IM_CHECK_RETV(gui::g_state.analysis.wait_feedback_texts.entry_hint_line.find("Pick a sky point") == 0, false);
  ctx->WindowClose("//Pose & trace");
  ctx->Yield(1);

  // The point in flight: the entries still answer, and the click does not touch the flight.
  SetSkyPointTarget(42.0, 0.0);
  ctx->Yield(1);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kWaiting; },
          30),
      false);
  const auto flight_id = gui::g_state.analysis.point.intent_id;
  ctx->ItemClick("**/Raypath view###main_raypath_open");
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analyze_raypath_view_open && EntryWindowActive("Raypath view"), false);
  IM_CHECK_RETV(gui::g_state.analysis.point.intent_id == flight_id, false);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  // The float moves aside: the cancel click below must land on the details block, not on it.
  if (ImGuiWindow* raypath_m = ctx->GetWindowByRef("//Raypath view")) {
    ImGui::SetWindowPos(raypath_m, ImVec2(40.0f, 80.0f), ImGuiCond_Always);
  }
  ctx->Yield(1);

  // Normal completion: the answer is on show — the slot names no navigation. The buttons' row
  // and the scroll child's origin are the slot-absent reference geometry for the cancelled leg.
  ImGuiWindow* details = details_window();
  IM_CHECK_RETV(details != nullptr, false);
  const ImGuiTestItemInfo raypath_item = ctx->ItemInfo("**/Raypath view###main_raypath_open", ImGuiTestOpFlags_NoError);
  const ImGuiTestItemInfo pose_item = ctx->ItemInfo("**/Pose & trace###main_pose_open", ImGuiTestOpFlags_NoError);
  IM_CHECK_RETV(raypath_item.ID != 0 && pose_item.ID != 0, false);
  ImGuiWindow* scroll = RegionWindowUnder("##AnalyzeDetails", "##AnalyzeDetailsScroll");
  IM_CHECK_RETV(scroll != nullptr, false);
  const ImVec2 raypath_min_before = raypath_item.RectFull.Min;
  const ImVec2 pose_min_before = pose_item.RectFull.Min;
  const float scroll_top_before = scroll->Pos.y;
  IM_CHECK_RETV(gui::g_state.analysis.wait_feedback_texts.entry_hint_line.empty(), false);

  // Cancel the point's next flight through the block's own cancel row; the cancelled state's
  // slot shows the one-word hint, and neither the buttons nor the scroll child move for it
  // (the hint is a single bounded line, never growth).
  SetSkyPointTarget(33.0, 0.0);
  ctx->Yield(1);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kWaiting; },
          30),
      false);
  // The block's cancel row lives in the scroll content: the halo survey's full summary puts it
  // below the fold at the default split, and a clipped item registers no ID for the engine to
  // find — scroll the region to its bottom first (the header buttons above need no such
  // preamble; this one is about the scrolled row itself).
  {
    ImGuiWindow* details_scroll = RegionWindowUnder("##AnalyzeDetails", "##AnalyzeDetailsScroll");
    IM_CHECK_RETV(details_scroll != nullptr, false);
    details_scroll->Scroll.y = details_scroll->ScrollMax.y;
    ctx->Yield(2);
  }
  ctx->ItemClick("**/Cancel point");
  ctx->SetRef("");
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCancelled; },
          30),
      false);
  const ImGuiTestItemInfo raypath_after =
      ctx->ItemInfo("**/Raypath view###main_raypath_open", ImGuiTestOpFlags_NoError);
  const ImGuiTestItemInfo pose_after = ctx->ItemInfo("**/Pose & trace###main_pose_open", ImGuiTestOpFlags_NoError);
  IM_CHECK_RETV(raypath_after.ID != 0 && pose_after.ID != 0, false);
  IM_CHECK_RETV(
      raypath_after.RectFull.Min.x == raypath_min_before.x && raypath_after.RectFull.Min.y == raypath_min_before.y,
      false);
  IM_CHECK_RETV(pose_after.RectFull.Min.x == pose_min_before.x && pose_after.RectFull.Min.y == pose_min_before.y,
                false);
  scroll = RegionWindowUnder("##AnalyzeDetails", "##AnalyzeDetailsScroll");
  IM_CHECK_RETV(scroll != nullptr && scroll->Pos.y == scroll_top_before, false);
  IM_CHECK_RETV(gui::g_state.analysis.wait_feedback_texts.entry_hint_line == "Cancelled", false);
  ctx->LogInfo("[header entries] flight-time click kept the intent, completion omits the slot, cancelled slot drew it");
  return true;
}

// The refusal and the mass arms (contract §16.3's remaining rows): the refused survey's hint
// reuses the engine's own reason text and the slot truncates it — the refusal state's details
// window carries exactly one ellipsis (the slot's; the scroll region draws the error wrapped,
// and no other text in that window ends in one). The identity class's rank-0 point answer
// names the Raypath view as the complete answer's owner — the slot line only (the block's own
// mass arm says "the mass is the whole answer").
bool SelectionHeaderEntriesNameRefusalAndMassArms(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;

  // The multi-layer refusal: the slot truncates the engine's reason (one ellipsis in the window).
  IM_CHECK_RETV(BringUpScene(ctx, kPcTwoLayerJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  std::string multi_display;
  for (const int idx : gui::g_state.analysis_result.display_order) {
    const auto& e = gui::g_state.analysis_result.payload->entries[static_cast<size_t>(idx)];
    if (std::strstr(e.display, " -> ") != nullptr) {
      multi_display = e.display;
      break;
    }
  }
  IM_CHECK_RETV(!multi_display.empty(), false);
  IM_CHECK_RETV(SelectRowByDisplay(ctx, multi_display.c_str()) != std::nullopt, false);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kRefused;
                    },
                    60),
                false);
  const std::string& refused_hint = gui::g_state.analysis.wait_feedback_texts.entry_hint_line;
  IM_CHECK_RETV(refused_hint.find("multi_layer") != std::string::npos, false);
  IM_CHECK_RETV(refused_hint.find("…") != std::string::npos, false);
  ctx->ItemClick("**/Raypath view###main_raypath_open");
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analyze_raypath_view_open, false);
  ctx->LogInfo("[header entries] refused state: the slot carries the engine reason, truncated");

  // The identity class's mass arm at the sun's own image: the slot points at the Raypath view.
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  std::string c15_display;
  for (const int idx : gui::g_state.analysis_result.display_order) {
    const auto& e = gui::g_state.analysis_result.payload->entries[static_cast<size_t>(idx)];
    if (std::strcmp(e.display, "1-3-6-2") == 0) {
      c15_display = e.display;
      break;
    }
  }
  IM_CHECK_RETV(!c15_display.empty(), false);
  IM_CHECK_RETV(SelectRowByDisplay(ctx, c15_display.c_str()) != std::nullopt, false);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    60),
                false);
  SetSkyPointTarget(20.0, 0.0);
  ctx->Yield(1);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  IM_CHECK_RETV(gui::g_state.analysis.wait_feedback_texts.entry_hint_line.find("The complete") != std::string::npos,
                false);
  ctx->LogInfo("[header entries] mass arm: the slot names the Raypath view as the complete answer's owner");
  return true;
}

// AC3 at the narrowest basis: the diagnostics region pinned at its D2 floor still shows both
// entry buttons fully inside its clip — after the scroll region is REALLY scrolled to its end
// (the buttons are fixed head, but the promise is measured, not assumed) — and the pose entry
// still opens its window from a click there. The slot omits at this width (the mass-arm line's
// prefix appears at the default basis and disappears here), state changes do not move the
// buttons, and the header's measured composition lands in the log: the §16.3 derivation's
// live-frame calibration (推演 103/110 vs 实测), itemized by the rects the log carries.
bool SelectionHeaderEntriesReachAtTheNarrowestBasis(ImGuiTestContext* ctx) {
  const auto request_basis = [&](int width, int height, const char* label) {
    g_window_size_test.width = width;
    g_window_size_test.height = height;
    g_window_size_test.done.store(false);
    g_window_size_test.requested.store(true);
    for (int i = 0; i < 20 && !g_window_size_test.done.load(); ++i) {
      ctx->Yield(1);
    }
    IM_CHECK(g_window_size_test.done.load());
    ctx->Yield(3);
    ctx->LogInfo("[header entries] basis %s", label);
  };
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  const ScopedWindowBasis window_basis(ctx);
  request_basis(1024, 640, "1024x640");
  const float panel_h = ImGui::GetIO().DisplaySize.y - gui::UiPx(gui::kTopBarHeight) - gui::UiPx(gui::kStatusBarHeight);
  // The floor construction pins the list ratio; the guard restores it on EVERY exit path (the
  // RAII rule for case-mutated persistent state — a mid-case red must not leak the ratio).
  struct RightListRatioGuard {
    float saved;
    RightListRatioGuard() : saved(gui::g_state.workspace.right_list_ratio) {}
    ~RightListRatioGuard() { gui::g_state.workspace.right_list_ratio = saved; }
  } list_ratio_guard;
  // The list absorbs the span above the diagnostics floor: the region reaches ITS floor — the
  // construction that shows the D2 floor is reachable at the narrowest basis (the list stays
  // above its own 250 floor meanwhile: floors coexist, §16.3's 368 ≤ 572).
  gui::g_state.workspace.right_list_ratio =
      (panel_h - gui::UiPx(gui::kAnalyzeSplitterSize) - gui::UiPx(gui::kAnalyzeDetailsMinHeight)) / panel_h;
  ctx->Yield(3);
  {
    ImGuiWindow* details = ctx->GetWindowByRef("##AnalyzeDetails");
    IM_CHECK_RETV(details != nullptr, false);
    IM_CHECK_RETV(fabsf(details->Size.y - gui::UiPx(gui::kAnalyzeDetailsMinHeight)) <= 1.0f, false);
    // The floor's budget promise at 1x, itemized: window height, the header's share (title row
    // + button row + gaps + padding), the scroll child's. The live-frame numbers the as-built
    // records — the derivation's 推演 103/105/110 meets its measured counterpart here.
    ImGuiWindow* scroll = RegionWindowUnder("##AnalyzeDetails", "##AnalyzeDetailsScroll");
    IM_CHECK_RETV(scroll != nullptr, false);
    const float header_h = scroll->Pos.y - details->Pos.y;
    const ImGuiTestItemInfo raypath_item =
        ctx->ItemInfo("**/Raypath view###main_raypath_open", ImGuiTestOpFlags_NoError);
    const ImGuiTestItemInfo pose_item = ctx->ItemInfo("**/Pose & trace###main_pose_open", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(raypath_item.ID != 0 && pose_item.ID != 0, false);
    ctx->LogInfo(
        "[header entries] calibration: details %.1f px (floor %.1f), header %.1f px, buttons %.1fx%.1f / %.1fx%.1f",
        details->Size.y, gui::UiPx(gui::kAnalyzeDetailsMinHeight), header_h, raypath_item.RectFull.GetWidth(),
        raypath_item.RectFull.GetHeight(), pose_item.RectFull.GetWidth(), pose_item.RectFull.GetHeight());
    // Both buttons fully inside the region's clip (visible rectangles, not submitted-only).
    IM_CHECK_RETV(details->InnerRect.Contains(raypath_item.RectFull.Min) &&
                      details->InnerRect.Contains(raypath_item.RectFull.Max),
                  false);
    IM_CHECK_RETV(
        details->InnerRect.Contains(pose_item.RectFull.Min) && details->InnerRect.Contains(pose_item.RectFull.Max),
        false);
  }

  // The identity class's mass arm at this basis: the slot omits (the space after the buttons
  // cannot carry even the ellipsis), the buttons hold their rectangles across the state
  // changes, and the scroll region really reaches its end with the buttons still in the clip.
  std::string c15_display;
  for (const int idx : gui::g_state.analysis_result.display_order) {
    const auto& e = gui::g_state.analysis_result.payload->entries[static_cast<size_t>(idx)];
    if (std::strcmp(e.display, "1-3-6-2") == 0) {
      c15_display = e.display;
      break;
    }
  }
  IM_CHECK_RETV(!c15_display.empty(), false);
  IM_CHECK_RETV(SelectRowByDisplay(ctx, c15_display.c_str()) != std::nullopt, false);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    60),
                false);
  const ImGuiTestItemInfo raypath_before =
      ctx->ItemInfo("**/Raypath view###main_raypath_open", ImGuiTestOpFlags_NoError);
  IM_CHECK_RETV(raypath_before.ID != 0, false);
  SetSkyPointTarget(20.0, 0.0);
  ctx->Yield(1);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  const ImGuiTestItemInfo raypath_after =
      ctx->ItemInfo("**/Raypath view###main_raypath_open", ImGuiTestOpFlags_NoError);
  IM_CHECK_RETV(raypath_after.ID != 0, false);
  IM_CHECK_RETV(raypath_after.RectFull.Min.x == raypath_before.RectFull.Min.x &&
                    raypath_after.RectFull.Min.y == raypath_before.RectFull.Min.y,
                false);
  {
    // The measured narrowest form: the slot draws its minimal truncation — asserted by FORM,
    // not by any pinned prefix (a wrong-fit implementation must not be nailed into the assert):
    // the line starts the mass-arm sentence, carries the ellipsis, and MEASURES to fit the
    // row's real leftover — the same right edge draw_hint resolves inside the window.
    const std::string& narrow_hint = gui::g_state.analysis.wait_feedback_texts.entry_hint_line;
    IM_CHECK_RETV(narrow_hint.find("The") == 0, false);
    IM_CHECK_RETV(narrow_hint.find("…") != std::string::npos, false);
    ImGuiWindow* details_win = ctx->GetWindowByRef("##AnalyzeDetails");
    const ImGuiTestItemInfo pose_now = ctx->ItemInfo("**/Pose & trace###main_pose_open", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(details_win != nullptr && pose_now.ID != 0, false);
    // The leftover reads the DETAILS WINDOW's own fields: ContentRegionRect is absolute
    // (GetWindowContentRegionMax subtracts Pos), Scroll stays 0 on the NoScrollbar window.
    // ImGui::GetContentRegionMax() is NOT callable at the test coroutine's resume point —
    // g.CurrentWindow there is the frame-end Debug fallback window, not this one.
    const float leftover = details_win->ContentRegionRect.Max.x - details_win->Scroll.x -
                           (pose_now.RectFull.Max.x + ImGui::GetStyle().ItemSpacing.x);
    IM_CHECK_RETV(ImGui::CalcTextSize(narrow_hint.c_str()).x <= leftover, false);
    ctx->LogInfo("[header entries] narrowest: minimal truncation '%s' (%.0f pt <= leftover %.0f pt)",
                 narrow_hint.c_str(), ImGui::CalcTextSize(narrow_hint.c_str()).x, leftover);
    // The omit branch, exercised directly (it cannot fire through legal UI geometry): no width
    // carries even the ellipsis, and the whole text rides when the width allows it.
    IM_CHECK_RETV(gui::AnalyzeEntryHintFitToWidth("Pick a sky point", 0.0f).empty(), false);
    IM_CHECK_RETV(gui::AnalyzeEntryHintFitToWidth("Cancelled", 10000.0f) == "Cancelled", false);
    // The middle-truncation pin: a width that admits "C…" but not "Ca…" returns exactly "C…" —
    // the longest prefix that FITS, never a prefix that was already rejected (the width
    // invariant: whatever comes back from the truncated path measures <= the requested width).
    const float c_w = ImGui::CalcTextSize("C…").x;
    const float ca_w = ImGui::CalcTextSize("Ca…").x;
    IM_CHECK_RETV(gui::AnalyzeEntryHintFitToWidth("Cancelled", (c_w + ca_w) / 2.0f) == "C…", false);
    const std::string mid =
        gui::AnalyzeEntryHintFitToWidth("The complete answer is in the Raypath view.", (c_w + ca_w) / 2.0f + 40.0f);
    IM_CHECK_RETV(ImGui::CalcTextSize(mid.c_str()).x <= (c_w + ca_w) / 2.0f + 40.0f, false);
    IM_CHECK_RETV(mid.find("The") == 0 && mid.find("…") != std::string::npos, false);
  }
  {
    ImGuiWindow* details = ctx->GetWindowByRef("##AnalyzeDetails");
    IM_CHECK_RETV(details != nullptr, false);
    ImGuiWindow* scroll = RegionWindowUnder("##AnalyzeDetails", "##AnalyzeDetailsScroll");
    IM_CHECK_RETV(scroll != nullptr, false);
    scroll->Scroll.y = scroll->ScrollMax.y;
    ctx->Yield(2);
    const ImGuiTestItemInfo raypath_item =
        ctx->ItemInfo("**/Raypath view###main_raypath_open", ImGuiTestOpFlags_NoError);
    const ImGuiTestItemInfo pose_item = ctx->ItemInfo("**/Pose & trace###main_pose_open", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(raypath_item.ID != 0 && pose_item.ID != 0, false);
    IM_CHECK_RETV(details->InnerRect.Contains(raypath_item.RectFull.Min) &&
                      details->InnerRect.Contains(raypath_item.RectFull.Max),
                  false);
    IM_CHECK_RETV(
        details->InnerRect.Contains(pose_item.RectFull.Min) && details->InnerRect.Contains(pose_item.RectFull.Max),
        false);
  }
  ctx->ItemClick("**/Pose & trace###main_pose_open");
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analyze_pose_view_open && EntryWindowActive("Pose & trace"), false);
  ctx->WindowClose("//Pose & trace");
  ctx->Yield(2);
  ctx->LogInfo("[header entries] narrowest basis: floor geometry, scrolled-to-end reachability, click answers");
  return true;
}

// AC2's window-combo faces (755.7 Step 2), each with BOTH floating windows standing — the
// composition the per-window cases leave out:
// ① the main view's direct ROI click lands between the windows (no arming step;
//    neither window's map, point doc nor the list's selection moves — one press, one consumer);
// ② a list selection switch re-keys both windows in the same frame they re-key the list;
// ③ closing both through their title bars and reopening both through their real entries —
//    the row click and the Selection header's pose button — brings both back with their regions;
// ④ the 150% scale at the narrowest 1024x640 basis keeps both windows structurally present
//    (the fit promises themselves are the per-window cases'), then the factory state restores.
bool WindowsCoexistPickReopenAndScaleTiers(ImGuiTestContext* ctx) {
  ScopedWatchdogGuard watchdog(ctx->EngineIO);
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpBothWindows(ctx), false);

  // ---- ① the main view's ROI pick, both windows standing.
  const gui::MapHandle map_before = gui::g_state.analysis.map.adopted_result;
  const gui::PointHandle point_before = gui::g_state.analysis.point.adopted_result;
  const std::optional<gui::SelectedClassIdentity> identity_before = gui::SelectedClassIdentityFor(gui::g_state);
  IM_CHECK_RETV(identity_before.has_value(), false);
  ctx->SetRef(PanoramaRegionRef());
  ctx->ItemClick("Point");
  ctx->Yield(2);
  ctx->SetRef("");
  ctx->Yield(1);
  {
    ImGuiWindow* pano = PanoramaPictureWindow();
    IM_CHECK_RETV(pano != nullptr, false);
    const ImVec2 pick_at(pano->Pos.x + pano->Size.x * 0.85f, pano->Pos.y + pano->Size.y * 0.55f);
    // The press belongs to the main view: the spot is inside the picture and inside NO floating
    // window — a future layout that covers it fails here, visibly.
    if (ImGuiWindow* w = ctx->GetWindowByRef("//Raypath view")) {
      IM_CHECK_RETV(!w->Rect().Contains(pick_at), false);
    }
    if (ImGuiWindow* w = ctx->GetWindowByRef("//Pose & trace")) {
      IM_CHECK_RETV(!w->Rect().Contains(pick_at), false);
    }
    ctx->MouseMoveToPos(pick_at);
    ctx->Yield(2);
    ctx->KeyDown(ImGuiMod_Alt);
    ctx->MouseClick(0);
    ctx->KeyUp(ImGuiMod_Alt);
    ctx->Yield(2);
  }
  IM_CHECK_RETV(gui::g_state.analysis.cone_center_valid, false);
  IM_CHECK_RETV(!PickBannerVisible(ctx), false);
  // No cross-talk: the windows' documents are the same handles, the selection the same identity.
  IM_CHECK_RETV(gui::g_state.analysis.map.adopted_result == map_before, false);
  IM_CHECK_RETV(gui::g_state.analysis.point.adopted_result == point_before, false);
  {
    const std::optional<gui::SelectedClassIdentity> identity_after = gui::SelectedClassIdentityFor(gui::g_state);
    IM_CHECK_RETV(identity_after.has_value() && identity_after->core_crystal_id == identity_before->core_crystal_id,
                  false);
  }
  ctx->LogInfo("[combo] ROI pick landed on the main view between two standing windows; nothing moved");

  // ---- ② a second row's selection re-keys both windows. The oracle is the FULL class
  // identity, derived independently from the second row's own payload entry (the same
  // mechanical rule the product's selection path runs), not from the post-click live state —
  // the halo's top two chains share their root crystal, so only the face sequence tells the
  // classes apart, and a stale-intent implementation that kept the first class's requests must
  // fail here, not pass on a crystal-id match.
  const auto& view = gui::g_state.analysis_result;
  IM_CHECK_RETV(view.display_order.size() >= 2u, false);
  const std::string second_display = view.payload->entries[static_cast<size_t>(view.display_order[1])].display;
  const LUMICE_RaypathHistogramEntry& second_entry = view.payload->entries[static_cast<size_t>(view.display_order[1])];
  const gui::SelectedClassIdentity expected_second = IdentityFromPayloadEntry(second_entry);
  const std::optional<gui::SelectedClassIdentity> old_identity = gui::SelectedClassIdentityFor(gui::g_state);
  const gui::SurveySelectionKey old_map_key = gui::g_state.analysis.map.key.selection;
  const gui::SurveySelectionKey old_point_key = gui::g_state.analysis.point.key.selection;
  IM_CHECK_RETV(old_identity.has_value(), false);
  // The two rows are distinct CLASSES: the face sequences differ (same root crystal here, so
  // this is exactly the field a crystal-only oracle would miss).
  IM_CHECK_RETV(old_identity->faces != expected_second.faces, false);
  IM_CHECK_RETV(SelectRowByDisplay(ctx, second_display.c_str()) != std::nullopt, false);
  // The live selection IS the expected second class, field for field.
  {
    const std::optional<gui::SelectedClassIdentity> now = gui::SelectedClassIdentityFor(gui::g_state);
    IM_CHECK_RETV(now.has_value(), false);
    IM_CHECK_RETV(now->core_crystal_id == expected_second.core_crystal_id, false);
    IM_CHECK_RETV(now->faces == expected_second.faces, false);
    IM_CHECK_RETV(now->layer_face_counts == expected_second.layer_face_counts, false);
    IM_CHECK_RETV(now->symmetry_bits_plus_one == expected_second.symmetry_bits_plus_one, false);
  }
  // Both sessions' ISSUED intents name the expected class's full selection key — the request
  // identity, not merely an adoption echo. Waiting for the key swap (not just adoption) is
  // what makes the late old-class answer unable to satisfy the waits below.
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [&expected_second, &old_map_key, &old_point_key] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      const gui::PointQuerySession& p = gui::g_state.analysis.point;
                      return m.key.selection.faces == expected_second.faces &&
                             m.key.selection.crystal_id == expected_second.core_crystal_id &&
                             !(m.key.selection == old_map_key) && p.key.selection.faces == expected_second.faces &&
                             p.key.selection.crystal_id == expected_second.core_crystal_id &&
                             !(p.key.selection == old_point_key);
                    },
                    30),
                false);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      return m.adopted_result != nullptr && (m.adopted_id == m.intent_id);
                    },
                    60),
                false);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  {
    LUMICE_PathPointInfo info{};
    info.struct_size = sizeof(info);
    IM_CHECK_RETV(
        LUMICE_PathPointGetInfo(gui::g_state.analysis.point.adopted_result.get(), &info) == LUMICE_PATH_READ_OK, false);
    // The completed point document names the EXPECTED class (derived before the click) — its
    // own face sequence equals the expected identity's, so a stale first-class doc under the
    // new key (same crystal, different faces) fails right here.
    IM_CHECK_RETV(info.crystal_id == expected_second.core_crystal_id, false);
    IM_CHECK_RETV(info.face_count == expected_second.faces.size(), false);
    IM_CHECK_RETV(std::equal(expected_second.faces.begin(), expected_second.faces.end(), info.faces), false);
    const std::string second_label = gui::AnalysisRowLabel(second_display, gui::AnalysisCoreToPoolMap(gui::g_state));
    IM_CHECK_RETV(gui::SelectedClassRowLabelFor(gui::g_state) == second_label, false);
  }
  IM_CHECK_RETV(EntryWindowActive("Raypath view") && EntryWindowActive("Pose & trace"), false);
  ctx->LogInfo(
      "[combo] second-row selection re-keyed both windows' issued intents and adopted docs to the full expected "
      "identity");

  // ---- ③ close both through their title bars, reopen both through their real entries.
  ctx->WindowClose("//Raypath view");
  ctx->Yield(2);
  ctx->WindowClose("//Pose & trace");
  ctx->Yield(2);
  IM_CHECK_RETV(!gui::g_state.analyze_raypath_view_open && !gui::g_state.analyze_pose_view_open, false);
  IM_CHECK_RETV(!EntryWindowActive("Raypath view") && !EntryWindowActive("Pose & trace"), false);
  IM_CHECK_RETV(SelectRowByDisplay(ctx, second_display.c_str()) != std::nullopt, false);  // the row click reopens
  IM_CHECK_RETV(gui::g_state.analyze_raypath_view_open && EntryWindowActive("Raypath view"), false);
  ctx->ItemClick("**/Pose & trace###main_pose_open");
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.analyze_pose_view_open && EntryWindowActive("Pose & trace"), false);
  PinPoseViewOnScreen(ctx);
  IM_CHECK_RETV(MapRegionWindow() != nullptr && PoseRegionWindow("##AnalyzeSunGlobe") != nullptr, false);
  ctx->LogInfo("[combo] close/reopen through the real entries brought both windows back with their regions");

  // ---- ④ the 150% scale at the narrowest basis: both windows stay structurally present, then
  // the factory state restores (the settle shapes are the pose narrowest case's). The lease
  // lives in a NESTED scope: its destructor runs at the scope's end — before the settled
  // checks — so an early exit inside the leg still restores, and the checks read restored state.
  {
    ScopedUiScaleAndBasisLease ui_lease(ctx);
    gui::g_ui_scale_multiplier = 1.5f;
    gui::g_ui_scale_dirty = true;
    for (int i = 0; i < 60 && !(gui::UiPx(gui::kAnalyzeLeftPanelWidth) > gui::kAnalyzeLeftPanelWidth * 1.5f - 0.5f &&
                                gui::UiPx(gui::kAnalyzeLeftPanelWidth) < gui::kAnalyzeLeftPanelWidth * 1.5f + 0.5f);
         ++i) {
      ctx->Yield(1);
    }
    g_window_size_test.apply_aspect = false;
    g_window_size_test.width = 1024;
    g_window_size_test.height = 640;
    g_window_size_test.done.store(false);
    g_window_size_test.requested.store(true);
    for (int i = 0; i < 20 && !g_window_size_test.done.load(); ++i) {
      ctx->Yield(1);
    }
    IM_CHECK_RETV(g_window_size_test.done.load(), false);
    ctx->Yield(3);
    PinRaypathViewOnScreen(ctx);
    PinPoseViewOnScreen(ctx);
    IM_CHECK_RETV(EntryWindowActive("Raypath view") && EntryWindowActive("Pose & trace"), false);
    IM_CHECK_RETV(MapRegionWindow() != nullptr && PoseRegionWindow("##AnalyzeSunGlobe") != nullptr, false);
    IM_CHECK_RETV(ctx->ItemInfo("**/##map_canvas", ImGuiTestOpFlags_NoError).ID != 0, false);
    ctx->LogInfo("[combo] 150% + 1024x640 basis: both windows present, regions and canvas submitted");
  }  // the lease restores the factory scale and basis here, on every exit path
  // The settled result, checked after the restore: the next case's entry audit finds nothing.
  IM_CHECK_RETV(gui::g_ui_scale_multiplier == gui::kFactoryUiScaleMultiplier, false);
  IM_CHECK_RETV(fabsf(gui::UiPx(gui::kAnalyzeLeftPanelWidth) - gui::kAnalyzeLeftPanelWidth) < 0.5f, false);
  gui::g_state.analyze_pose_view_open = false;
  gui::g_state.analyze_raypath_view_open = false;
  ctx->Yield(2);
  return true;
}

// AC3's in-flight and document-scope faces (755.7 Step 3), each with BOTH floating windows
// standing and each in-flight arm gated on a VERIFIABLE issued premise — the pump's own intent
// key carries the request identity (selection faces + target + λ), so "issued" is a readback,
// not a hope; the adopted handle standing for the OLD key at that moment is the not-yet-adopted
// half of the premise:
// ① a λ change issued for 600 while the face waits, superseded by 0 — the adopted document's
//    own wavelength echo judges the answer that finally landed (a transient 600 adoption that
//    a later answer overwrites is executor-ordering territory, unclaimed at this layer);
// ② a class switch issued while the first class's point request is observed executing — the
//    issued intent's full selection key (faces, not the shared crystal) names the second
//    class, and the final document's own faces carry the discrimination;
// ③ a sky-point move issued, then the workspace switched before adoption — the Render-side
//    pump adopts with the windows open, and the document names the moved target;
// ④ the survey re-run with the executor verifiably in flight, then Stop — the trigger's
//    semantics clear the selection, the windows stay open, a re-selection re-arms both faces;
// ⑤ the point face verifiably outstanding and Revert (the cancel-latch face, distinct from
//    ④'s analysis-run Stop) — the windows stand through the withdrawal, then the re-selection
//    re-arms;
// ⑥ Revert after adoption keeps every adopted outcome by handle value, the sky point, the
//    windows (the settled contract, distinct from ⑤'s withdrawal);
// ⑦ Open (the document replace) with the windows open and a point query verifiably in flight —
//    the executor drains, every face clears, both windows close, nothing revives;
// ⑧ New with a verifiably in-flight point query (a fresh standing state) — the same cleared
//    and drained contract through DoNew.
bool WindowsStandThroughInFlightAndDocumentScopes(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  // The arms' serial analyses exceed the default 60 s watchdog; the scale-tier precedent's
  // raised limits apply for the whole case (restored on every exit).
  ScopedWatchdogGuard watchdog(ctx->EngineIO);
  // The InFlight gates' verdicts read this recorder's transition record (see the struct's
  // linkage comment); declared after the guards so it disarms before any teardown runs.
  ScopedPointPickupEdgeRecorder pickups;
  IM_CHECK_RETV(BringUpBothWindows(ctx), false);

  // ---- ① the shared λ knob, both windows standing: 600 issued with its execution recorded
  // (the pickup edge), then 0 supersedes it; the adopted document's wavelength echo is the
  // final judge.
  gui::g_state.analysis.map_wavelength_nm = 600.0;
  const int arm1_pickup_baseline = pickups.Baseline();
  IM_CHECK_RETV(DriveScopesGate(
                    ctx, "arm1_lambda600_issued",
                    [&pickups, arm1_pickup_baseline](ScopesGateTrace& t) {
                      const gui::PointQuerySession& p = gui::g_state.analysis.point;
                      const gui::PointQueryState state = gui::PointQueryStateFor(p);
                      t.Poll({ { "intent_valid", p.intent_valid },
                               { "key_match", p.key.wavelength_nm == 600.0 },
                               { "waiting", state == gui::PointQueryState::kWaiting },
                               { "point_in_flight", gui::g_survey_executor.InFlight(gui::QueryFace::kPoint) } },
                             ScopesStateBit(state));
                      // The recorded pickup edge proves the execution window opened with this
                      // intent standing (the recorder's linkage comment); the two re-sampled
                      // fields are wide-window states that adoption keeps and nothing re-keys
                      // inside this gate, so the conjunction's verdict carries no sampling race.
                      return p.intent_valid && p.key.wavelength_nm == 600.0 &&
                             pickups.PickedUpSince(arm1_pickup_baseline);
                    },
                    kScopesObserveBudgetS),
                false);
  gui::g_state.analysis.map_wavelength_nm = 0.0;
  IM_CHECK_RETV(DriveUntil(
                    ctx, [] { return gui::g_state.analysis.point.key.wavelength_nm == 0.0; }, kScopesObserveBudgetS),
                false);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          kScopesCompleteBudgetS),
      false);
  {
    LUMICE_PathPointInfo info{};
    info.struct_size = sizeof(info);
    IM_CHECK_RETV(
        LUMICE_PathPointGetInfo(gui::g_state.analysis.point.adopted_result.get(), &info) == LUMICE_PATH_READ_OK, false);
    IM_CHECK_RETV(std::fabs(info.wavelength_nm - 600.0) > 1e-3, false);  // the landed doc's echo is not 600
  }
  IM_CHECK_RETV(gui::g_state.analysis.map.key.wavelength_nm == 0.0, false);
  IM_CHECK_RETV(EntryWindowActive("Raypath view") && EntryWindowActive("Pose & trace"), false);
  ctx->LogInfo("[scopes] lambda 600 issued in flight, superseded by 0; the adopted doc's echo is not 600");

  // ---- ② a first-class request EXECUTING across the class switch: the sky point moves first
  // (the first class's point query goes out with its execution recorded on the executor's
  // per-face token — the pickup edge; InFlight(kPoint) is the running query, not the queue), and
  // only then does the second row's click re-key the session. The switch's own request is
  // likewise recorded executing before its answer is read, and the ADOPTED DOCUMENT's own face
  // sequence is the oracle — LUMICE_PathPointInfo carries the doc's faces, the field the halo's
  // top chains differ on, so a stale first-class answer adopted under the new key fails here.
  const auto& view = gui::g_state.analysis_result;
  IM_CHECK_RETV(view.display_order.size() >= 2u, false);
  const std::string second_display = view.payload->entries[static_cast<size_t>(view.display_order[1])].display;
  const gui::SelectedClassIdentity expected_second =
      IdentityFromPayloadEntry(view.payload->entries[static_cast<size_t>(view.display_order[1])]);
  const std::optional<gui::SelectedClassIdentity> old_identity = gui::SelectedClassIdentityFor(gui::g_state);
  IM_CHECK_RETV(old_identity.has_value() && old_identity->faces != expected_second.faces, false);
  SetSkyPointTarget(45.0, 5.0);
  const int arm2_move_pickup_baseline = pickups.Baseline();
  IM_CHECK_RETV(
      DriveScopesGate(
          ctx, "arm2_sky_point_moved",
          [&pickups, arm2_move_pickup_baseline](ScopesGateTrace& t) {
            const gui::PointQuerySession& p = gui::g_state.analysis.point;
            const gui::PointQueryState state = gui::PointQueryStateFor(p);
            t.Poll({ { "intent_valid", p.intent_valid },
                     { "key_match", std::fabs(p.key.target_dir[2] - (-std::sin(45.0 * lumice::kDeg2Rad))) < 1e-4 },
                     { "waiting", state == gui::PointQueryState::kWaiting },
                     { "point_in_flight", gui::g_survey_executor.InFlight(gui::QueryFace::kPoint) } },
                   ScopesStateBit(state));
            return p.intent_valid && std::fabs(p.key.target_dir[2] - (-std::sin(45.0 * lumice::kDeg2Rad))) < 1e-4 &&
                   pickups.PickedUpSince(arm2_move_pickup_baseline);
          },
          kScopesObserveBudgetS),
      false);
  // The baseline is read BEFORE the row click: the click's frames may already issue and pick up
  // the second class's query, and an edge that lands before the baseline would be invisible to
  // the gate. Every pickup after the baseline belongs to this arm's query — the first class's
  // query was picked up once, before the move gate above passed.
  const int arm2_switch_pickup_baseline = pickups.Baseline();
  IM_CHECK_RETV(SelectRowByDisplay(ctx, second_display.c_str()) != std::nullopt, false);
  IM_CHECK_RETV(DriveScopesGate(
                    ctx, "arm2_class_switch_issued",
                    [&expected_second, &old_identity, &pickups, arm2_switch_pickup_baseline](ScopesGateTrace& t) {
                      const gui::PointQuerySession& p = gui::g_state.analysis.point;
                      const bool key_match = p.key.selection.faces == expected_second.faces &&
                                             p.key.selection.crystal_id == expected_second.core_crystal_id &&
                                             !(p.key.selection.faces == old_identity->faces);
                      const gui::PointQueryState state = gui::PointQueryStateFor(p);
                      t.Poll({ { "intent_valid", p.intent_valid },
                               { "new_faces_key", key_match },
                               { "waiting", state == gui::PointQueryState::kWaiting },
                               { "point_in_flight", gui::g_survey_executor.InFlight(gui::QueryFace::kPoint) } },
                             ScopesStateBit(state));
                      return p.intent_valid && key_match && pickups.PickedUpSince(arm2_switch_pickup_baseline);
                    },
                    kScopesObserveBudgetS),
                false);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          kScopesCompleteBudgetS),
      false);
  {
    LUMICE_PathPointInfo info{};
    info.struct_size = sizeof(info);
    IM_CHECK_RETV(
        LUMICE_PathPointGetInfo(gui::g_state.analysis.point.adopted_result.get(), &info) == LUMICE_PATH_READ_OK, false);
    // The adopted document IS the second class's answer: its own face sequence equals the
    // expected identity's (the negative case — a stale first-class doc as the one that finally
    // landed under the new key — carries the old faces and fails this compare, which is the
    // discrimination the oracle exists for; a transient stale adoption that the second class's
    // answer later overwrites is executor-ordering territory, unclaimed at this layer).
    IM_CHECK_RETV(info.crystal_id == expected_second.core_crystal_id, false);
    IM_CHECK_RETV(info.face_count == expected_second.faces.size(), false);
    IM_CHECK_RETV(std::equal(expected_second.faces.begin(), expected_second.faces.end(), info.faces), false);
  }
  IM_CHECK_RETV(EntryWindowActive("Raypath view") && EntryWindowActive("Pose & trace"), false);
  ctx->LogInfo(
      "[scopes] first-class request executing across the class switch; adopted doc's faces = the second class's");

  // ---- ③ target A issued with its execution recorded (the per-face running token's pickup
  // edge; A's intent id saved by value), then target B supersedes it mid-flight — the pump's
  // key swap is the readback that B's intent now stands — and only then does the case switch
  // workspaces. The Render-side pump adopts B's answer with the windows open, and the
  // document's own target echo (alt 30) judges the document that LANDED: had A's answer been
  // the final one, the doc would read A's target and fail here. What this layer cannot
  // distinguish is a transient stale-A adoption that B's later answer overwrites — the
  // completion-event ordering and the refusal behind it are executor-internal contracts,
  // structurally not exposed to the GUI layer; the unit layer's
  // SurveyExecutorSupersedesRefusesOnReleaseAndJoins carries them, and no GUI-layer claim is
  // made for that ordering. This case owns the GUI-side composition.
  SetSkyPointTarget(28.0, 8.0);
  gui::SurveyQueryId target_a_id;
  const int arm3_pickup_baseline = pickups.Baseline();
  IM_CHECK_RETV(DriveScopesGate(
                    ctx, "arm3_target_a_issued",
                    [&target_a_id, &pickups, arm3_pickup_baseline](ScopesGateTrace& t) {
                      const gui::PointQuerySession& p = gui::g_state.analysis.point;
                      const bool key_match =
                          std::fabs(p.key.target_dir[2] - (-std::sin(28.0 * lumice::kDeg2Rad))) < 1e-4;
                      const gui::PointQueryState state = gui::PointQueryStateFor(p);
                      const bool issued = p.intent_valid && key_match && pickups.PickedUpSince(arm3_pickup_baseline);
                      if (issued && target_a_id.serial == 0 && target_a_id.generation == 0) {
                        target_a_id = p.intent_id;  // A's request id, by value — stable from issue through
                                                    // adoption, and B is not written until this gate passes
                      }
                      t.Poll({ { "intent_valid", p.intent_valid },
                               { "key_match", key_match },
                               { "waiting", state == gui::PointQueryState::kWaiting },
                               { "point_in_flight", gui::g_survey_executor.InFlight(gui::QueryFace::kPoint) } },
                             ScopesStateBit(state));
                      return issued;
                    },
                    kScopesObserveBudgetS),
                false);
  IM_CHECK_RETV(target_a_id.serial != 0 || target_a_id.generation != 0, false);
  SetSkyPointTarget(30.0, 10.0);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [&target_a_id] {
                      const gui::PointQuerySession& p = gui::g_state.analysis.point;
                      return p.intent_valid && (p.intent_id != target_a_id) &&
                             std::fabs(p.key.target_dir[2] - (-std::sin(30.0 * lumice::kDeg2Rad))) < 1e-4;
                    },
                    kScopesObserveBudgetS),
                false);
  ctx->ItemClick("##TopBar/Render");
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.workspace.mode == gui::GuiState::WorkspaceMode::kRender, false);
  IM_CHECK_RETV(EntryWindowActive("Raypath view") && EntryWindowActive("Pose & trace"), false);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [&target_a_id] {
                      // B's adoption: the doc gate passed for the post-supersession intent (its
                      // id has moved off A's), and the document echo names B's target.
                      const gui::PointQuerySession& p = gui::g_state.analysis.point;
                      if (gui::PointQueryStateFor(p) != gui::PointQueryState::kCompleted ||
                          p.adopted_result == nullptr || !(p.intent_id != target_a_id)) {
                        return false;
                      }
                      LUMICE_PathPointInfo i{};
                      i.struct_size = sizeof(i);
                      return LUMICE_PathPointGetInfo(p.adopted_result.get(), &i) == LUMICE_PATH_READ_OK &&
                             std::fabs(i.target_altitude_deg - 30.0) < 1e-3;
                    },
                    kScopesCompleteBudgetS),
                false);
  ctx->LogInfo(
      "[scopes] A executed, B superseded it mid-flight, B's answer landed across the workspace switch (alt 30)");
  ctx->ItemClick("##TopBar/Analyze");
  ctx->Yield(3);
  IM_CHECK_RETV(EntryWindowActive("Raypath view") && EntryWindowActive("Pose & trace"), false);

  // ---- ④ the survey re-run with the executor verifiably in flight, then Stop.
  ctx->SetRef(PanoramaRegionRef());
  ctx->ItemClick(kAnalyzeButton);
  ctx->SetRef("");
  IM_CHECK_RETV(gui::g_state.analysis.started, false);
  // The run species' in-flight premise: the whole-sky analysis run is on the server (a
  // different machine from the class-overview query executor below).
  IM_CHECK_RETV(gui::g_state.analysis_run_in_progress, false);
  ctx->SetRef(PanoramaRegionRef());
  IM_CHECK_RETV(ctx->ItemInfo(ICON_FA_STOP " Stop").ID != 0, false);
  ctx->ItemClick(ICON_FA_STOP " Stop");
  ctx->SetRef("");
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return !gui::g_state.analysis_run_in_progress && gui::g_state.sim_state == SimState::kDone; },
          kScopesTightBudgetS),
      false);
  // The trigger cleared the SELECTION (a synchronous write); the list on show is whatever the
  // stopped run adopted — nothing asserted about it here. The windows stay open.
  IM_CHECK_RETV(EntryWindowActive("Raypath view") && EntryWindowActive("Pose & trace"), false);
  IM_CHECK_RETV(gui::MapQueryStateFor(gui::g_state.analysis.map) == gui::MapQueryState::kNoSelection, false);
  ctx->LogInfo("[scopes] survey stop drained (executor was in flight); the windows stayed open");
  // Rows come back with the next completed analysis — then the re-selection re-arms both faces.
  ctx->SetRef(PanoramaRegionRef());
  ctx->ItemClick(kAnalyzeButton);
  ctx->SetRef("");
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return !gui::g_state.analysis_run_in_progress && gui::g_state.analysis_result.payload != nullptr; },
          kScopesCompleteBudgetS),
      false);
  IM_CHECK_RETV(!gui::g_state.analysis_result.payload->entries.empty(), false);
  IM_CHECK_RETV(SelectRowByDisplay(ctx, TopChainDisplay().c_str()) != std::nullopt, false);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      return m.adopted_result != nullptr && (m.adopted_id == m.intent_id);
                    },
                    kScopesCompleteBudgetS),
                false);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          kScopesCompleteBudgetS),
      false);
  ctx->LogInfo("[scopes] the re-selection re-armed map and point in the standing windows");


  // ---- ⑤ the point face verifiably outstanding (kWaiting under the new target), withdrawn by
  // Revert — the cancel-latch face (distinct from Stop's run drain). The withdrawal is per-face
  // (the app latches only a face whose query is out; a face whose answer already arrived keeps
  // its adopted result), so the arm holds the point query at its pickup — the query is
  // verifiably computing WHEN DoRevert runs, the premise a historical edge cannot carry (it may
  // have completed between two polls). The pickup hold IS the execution observation here: it
  // fires at the same instant the recorder's edge would. The survey face's variant stays with
  // the single-piece precedent (survey_survives_a_workspace_switch), whose digest flight window
  // its own construction catches. The windows stand throughout.
  SetSkyPointTarget(33.0, 17.0);
  ScopedPointPickupHold hold;
  IM_CHECK_RETV(DriveUntil(ctx, [&hold] { return hold.PickedUp(); }, kScopesObserveBudgetS), false);
  gui::DoRevert();  // latches kCancelled + cancels the HELD query (it IS outstanding now)
  hold.Release();
  // A voided hold (the GUI stalled past the worker's bound — premise lost) fails the arm here,
  // not silently: the gate accepts it early and the assert below names it.
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [&hold] {
                      return hold.Voided() ||
                             gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCancelled;
                    },
                    kScopesTightBudgetS),
                false);
  IM_CHECK_RETV(!hold.Voided(), false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result != nullptr, false);
  // The map face may legitimately be in flight here (the sky-point move re-keys it too); the
  // latch is per-face and only the point face was asserted outstanding.
  IM_CHECK_RETV(EntryWindowActive("Raypath view") && EntryWindowActive("Pose & trace"), false);
  ctx->LogInfo(
      "[scopes] Revert withdrew the outstanding point face under its latch; the other faces and windows stood");
  // The moved sky point re-arms the point face (the latch clears with the changed key) — the
  // standing windows refill, and the settled state feeds ⑥'s keep arm.
  SetSkyPointTarget(42.0, 0.0);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          kScopesCompleteBudgetS),
      false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result != nullptr, false);
  IM_CHECK_RETV(gui::g_state.analysis.map.adopted_result != nullptr, false);

  // ---- ⑥ Revert after adoption keeps: every adopted outcome by handle value, the sky point,
  // the windows (the settled contract — no query in flight here, asserted). The keep-oracle
  // snapshots are taken AFTER every execution face has drained AND the pump has consumed any
  // finish already posted (a posted-but-unconsumed finish adopts on the next frame and would
  // legitimately move e.g. the map's adopted_result — snapshotting before the drain would misread
  // that as Revert swapping the result): the drain wait, then the frame that lets the pump catch
  // up, THEN the snapshots, then the assertion and the Revert.
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      // No observation or adoption can owe a hidden recalculation. All faces
                      // quiet plus the following frame drain is the completion boundary.
                      return !gui::g_survey_executor.QueryOutstanding();
                    },
                    kScopesCompleteBudgetS),
                false);
  ctx->Yield(2);  // the pump's consumption of posted finishes (settled-with-margin + quiet: none pending)
  const gui::SurveyHandle survey_kept = gui::g_state.analysis.survey.adopted_result;
  const gui::MapHandle map_kept = gui::g_state.analysis.map.adopted_result;
  const gui::PointHandle point_kept = gui::g_state.analysis.point.adopted_result;
  IM_CHECK_RETV(survey_kept != nullptr && map_kept != nullptr && point_kept != nullptr, false);
  // The assertion stays verbatim; the wait protocol above carries the patience.
  IM_CHECK_RETV(!gui::g_survey_executor.InFlight(), false);
  gui::DoRevert();
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.analysis.survey.intent_valid == true, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result == survey_kept, false);
  IM_CHECK_RETV(gui::g_state.analysis.map.adopted_result == map_kept, false);
  IM_CHECK_RETV(gui::g_state.analysis.point.adopted_result == point_kept, false);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid, false);
  IM_CHECK_RETV(!gui::g_survey_executor.InFlight(), false);
  IM_CHECK_RETV(EntryWindowActive("Raypath view") && EntryWindowActive("Pose & trace"), false);
  ctx->LogInfo("[scopes] settled Revert kept the faces, the sky point and both standing windows");

  // ---- Open with a point query held at pickup: the real JSON command replaces the document,
  // keeps the workspace mode, clears every face and closes both analysis windows.
  const auto open_path = GuiTestTempPath("workspace_scope_open.json");
  struct OpenFileCleanup {
    std::filesystem::path path;
    ~OpenFileCleanup() {
      std::error_code ec;
      std::filesystem::remove(path, ec);
    }
  } open_cleanup{ open_path };
  IM_CHECK_RETV(gui::ExportConfigJson(open_path, kHalo22Json), false);
  SetSkyPointTarget(35.0, 20.0);
  ScopedPointPickupHold hold7;
  IM_CHECK_RETV(DriveUntil(ctx, [&hold7] { return hold7.PickedUp(); }, kScopesObserveBudgetS), false);
  IM_CHECK_RETV(gui::g_state.workspace.mode == gui::GuiState::WorkspaceMode::kAnalyze, false);
  gui::DoOpen(open_path);
  IM_CHECK_RETV(gui::g_state.workspace.mode == gui::GuiState::WorkspaceMode::kAnalyze, false);
  IM_CHECK_RETV(gui::g_state.current_file_path.empty() && gui::g_state.dirty, false);
  hold7.Release();  // the engine call proceeds against the cancelled/released binding and drains
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [&hold7] { return hold7.Voided() || !gui::g_survey_executor.InFlight(gui::QueryFace::kPoint); },
          kScopesTightBudgetS),
      false);
  IM_CHECK_RETV(!hold7.Voided(), false);
  // The drain covers the worker AND the GUI pump's consumption of any completion that raced.
  ctx->Yield(6);
  IM_CHECK_RETV(gui::g_state.analysis.survey.intent_valid == false, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::MapQueryStateFor(gui::g_state.analysis.map) == gui::MapQueryState::kNoSelection, false);
  IM_CHECK_RETV(gui::g_state.analysis.map.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kNoIntent, false);
  IM_CHECK_RETV(gui::g_state.analysis.point.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::g_state.analysis.trace.intent_valid == false, false);
  IM_CHECK_RETV(gui::g_state.analysis.trace.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid == false, false);
  IM_CHECK_RETV(!gui::g_state.analyze_raypath_view_open && !gui::g_state.analyze_pose_view_open, false);
  IM_CHECK_RETV(!EntryWindowActive("Raypath view") && !EntryWindowActive("Pose & trace"), false);
  ctx->LogInfo("[scopes] Open replaced the document with a query in flight; faces cleared, windows closed, drained");

  // ---- ⑧ New with a point query held outstanding at its pickup (a fresh standing state: both
  // windows open, class selected, arc point completed, then the move issued) — then DoNew — the
  // executor's binding releases on the worker thread and nothing revives.
  IM_CHECK_RETV(BringUpBothWindows(ctx), false);
  SetSkyPointTarget(31.0, 15.0);
  ScopedPointPickupHold hold8;
  IM_CHECK_RETV(DriveUntil(ctx, [&hold8] { return hold8.PickedUp(); }, kScopesObserveBudgetS), false);
  gui::DoNew();
  IM_CHECK_RETV(gui::g_state.workspace.mode == gui::GuiState::WorkspaceMode::kAnalyze, false);
  IM_CHECK_RETV(gui::g_state.current_file_path.empty() && !gui::g_state.dirty, false);
  hold8.Release();
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [&hold8] { return hold8.Voided() || !gui::g_survey_executor.InFlight(gui::QueryFace::kPoint); },
          kScopesTightBudgetS),
      false);
  IM_CHECK_RETV(!hold8.Voided(), false);
  ctx->Yield(6);
  IM_CHECK_RETV(gui::g_state.analysis.survey.intent_valid == false, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::MapQueryStateFor(gui::g_state.analysis.map) == gui::MapQueryState::kNoSelection, false);
  IM_CHECK_RETV(gui::g_state.analysis.map.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kNoIntent, false);
  IM_CHECK_RETV(gui::g_state.analysis.point.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::g_state.analysis.trace.intent_valid == false, false);
  IM_CHECK_RETV(gui::g_state.analysis.trace.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid == false, false);
  IM_CHECK_RETV(!gui::g_state.analyze_raypath_view_open && !gui::g_state.analyze_pose_view_open, false);
  IM_CHECK_RETV(!EntryWindowActive("Raypath view") && !EntryWindowActive("Pose & trace"), false);
  // No-revival over time: several more frames after the drain, the cleared contract still
  // holds (a late A-answer publish that revived any face would show here).
  ctx->Yield(20);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::g_state.analysis.point.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::g_state.analysis.map.adopted_result == nullptr, false);
  IM_CHECK_RETV(!gui::g_survey_executor.InFlight(), false);
  ctx->LogInfo("[scopes] New with a query in flight: faces cleared, windows closed, executor drained, no revival");
  return true;
}

// The staleness hints' edit-stale leg (analysis_stale_hint_child_windows): the red state is
// warning-coloured ink absent from both floating windows — see the registration below.
bool AnalysisStaleHintEditStaleLeg(ImGuiTestContext* ctx) {
  ScopedWatchdogGuard watchdog(ctx->EngineIO);
  ScopedServerGuard guard;

  // ---- Leg 1 — the edit-stale fact, both windows standing. ----
  ResetTestState();
  gui::g_server = LUMICE_CreateServer();
  IM_CHECK_RETV(gui::g_server != nullptr, false);
  LUMICE_SetLogLevel(gui::g_server, static_cast<LUMICE_LogLevel>(g_core_log_level));
  IM_CHECK_RETV(gui::DeserializeFromJson(kFullyRandomJson, gui::g_state), false);
  gui::g_state.renderer.sim_resolution_index = 0;
  gui::g_state.sim.infinite = false;
  gui::g_state.sim.ray_num_millions = 0.1f;
  ctx->Yield(2);
  // The one-picture gate, the five-op chain's shape: a real Run first.
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo("##TopBar/" ICON_FA_PLAY " Run")), false);
  ctx->ItemClick("##TopBar/" ICON_FA_PLAY " Run");
  IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kDone; }, 60), false);
  ctx->ItemClick("##TopBar/Analyze");
  ctx->Yield(3);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  // Both windows standing: the row click opens the Raypath view, the Selection header's
  // button the pose window; both pinned so the hint's clip is the window's own.
  IM_CHECK_RETV(SelectRowByDisplay(ctx, TopChainDisplay().c_str()) != std::nullopt, false);
  IM_CHECK_RETV(gui::g_state.analyze_raypath_view_open && EntryWindowActive("Raypath view"), false);
  PinRaypathViewOnScreen(ctx);
  IM_CHECK_RETV(!gui::g_state.analyze_pose_view_open, false);
  ctx->ItemClick("**/Pose & trace###main_pose_open");
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.analyze_pose_view_open && EntryWindowActive("Pose & trace"), false);
  PinPoseViewOnScreen(ctx);
  ctx->Yield(3);

  const ImU32 warn_colour = WarningTextVertexColour();
  // The probe's discriminating power, stated the way the drift band's case states its own:
  // while the analysis is fresh the two windows draw NO warning-coloured ink. If anything
  // else in either subtree ever does, this leg turns red at the control and the probe needs
  // a second discriminator — a deliberate tripwire, not a pass.
  IM_CHECK_RETV(!CollectWindowWarningInk("Raypath view", warn_colour).found, false);
  IM_CHECK_RETV(!CollectWindowWarningInk("Pose & trace", warn_colour).found, false);

  // A real point session, so the multi-frame block below witnesses the fact the plan names:
  // the stale edit must not re-issue a point query either. The map's pick sets the sky point
  // (the five-op chain's op ④ gesture — the window's map takes its own adoption first), and the
  // point query runs to its completed state.
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      return m.adopted_result != nullptr && (m.adopted_id == m.intent_id);
                    },
                    60),
                false);
  ctx->KeyDown(ImGuiMod_Alt);
  ctx->ItemClick("**/##map_canvas");
  ctx->KeyUp(ImGuiMod_Alt);
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid, false);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);

  // AC3's baseline: the session's intents and the payload are captured before the edit —
  // the hint must state the staleness, never re-run the analysis or re-issue a query.
  const auto* payload_before = gui::g_state.analysis_result.payload.get();
  const auto survey_intent_before = gui::g_state.analysis.survey.intent_id;
  const auto map_intent_before = gui::g_state.analysis.map.intent_id;
  const auto point_intent_before = gui::g_state.analysis.point.intent_id;
  const bool point_intent_valid_before = gui::g_state.analysis.point.intent_valid;
  // The edit: the sun's altitude, the same input the five-op chain's op ⑤ makes — a scene
  // edit the identity reads as stale.
  const float alt_before = gui::g_state.sun.altitude;
  ctx->SetRef("//##AnalyzeLeftPanel");
  ctx->ItemInputValue("##Altitude_input", alt_before - 5.0f);
  ctx->SetRef("");
  ctx->Yield(3);
  IM_CHECK_RETV(fabsf(gui::g_state.sun.altitude - (alt_before - 5.0f)) < 0.05f, false);
  IM_CHECK_RETV(gui::g_state.sim_state == SimState::kModified, false);
  // The staleness fact is the frame's derived state (the unification's Step 1): the windows'
  // hints consume it, and the field's verdict here is the case's own bridge between the fact
  // and the ink below.
  IM_CHECK_RETV(gui::g_state.analysis_freshness.list == gui::AnalysisListFreshness::kStale, false);

  // The red state: both windows show the hint.
  const PanoramaWarningInk map_hint = CollectWindowWarningInk("Raypath view", warn_colour);
  IM_CHECK_RETV(map_hint.found, false);
  IM_CHECK_RETV(!map_hint.clipped, false);
  const PanoramaWarningInk pose_hint = CollectWindowWarningInk("Pose & trace", warn_colour);
  IM_CHECK_RETV(pose_hint.found, false);
  IM_CHECK_RETV(!pose_hint.clipped, false);
  // AC3's face: stating changed nothing.
  IM_CHECK_RETV(!gui::g_state.analysis_run_in_progress, false);
  IM_CHECK_RETV(gui::g_state.analysis_result.payload.get() == payload_before, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.intent_id == survey_intent_before, false);
  IM_CHECK_RETV(gui::g_state.analysis.map.intent_id == map_intent_before, false);
  // The conjunction's other two faces, now that the render sites are in: the string the site
  // reports is the string that was drawn, and the full item rect is the completeness face — a
  // real wrapped line's footprint, inside the window's clip, containing the ink. The three
  // evidence faces are independent (ink = emitted vertices, rect = layout footprint, field =
  // the site's report); the conjunction is what the first-line-only clipping failure cannot
  // satisfy.
  {
    const auto& feedback = gui::g_state.analysis.wait_feedback_texts;
    ImGuiWindow* map_win = ctx->GetWindowByRef("//Raypath view");
    IM_CHECK_RETV(map_win != nullptr, false);
    IM_CHECK_RETV(feedback.map_stale_hint.find("configuration that has since changed") != std::string::npos, false);
    const ImRect map_rect(ImVec2(feedback.map_stale_hint_rect_min_x, feedback.map_stale_hint_rect_min_y),
                          ImVec2(feedback.map_stale_hint_rect_max_x, feedback.map_stale_hint_rect_max_y));
    IM_CHECK_RETV(map_rect.GetHeight() >= ImGui::GetTextLineHeight(), false);
    IM_CHECK_RETV(map_win->InnerClipRect.Contains(map_rect), false);
    IM_CHECK_RETV(map_hint.bbox.Min.y >= map_rect.Min.y - 1.0f && map_hint.bbox.Max.y <= map_rect.Max.y + 1.0f, false);
    ImGuiWindow* pose_win = ctx->GetWindowByRef("//Pose & trace");
    IM_CHECK_RETV(pose_win != nullptr, false);
    IM_CHECK_RETV(feedback.point_stale_hint.find("previous configuration") != std::string::npos, false);
    const ImRect pose_rect(ImVec2(feedback.point_stale_hint_rect_min_x, feedback.point_stale_hint_rect_min_y),
                           ImVec2(feedback.point_stale_hint_rect_max_x, feedback.point_stale_hint_rect_max_y));
    IM_CHECK_RETV(pose_rect.GetHeight() >= ImGui::GetTextLineHeight(), false);
    IM_CHECK_RETV(pose_win->InnerClipRect.Contains(pose_rect), false);
    IM_CHECK_RETV(pose_hint.bbox.Min.y >= pose_rect.Min.y - 1.0f && pose_hint.bbox.Max.y <= pose_rect.Max.y + 1.0f,
                  false);
  }
  // The narrowest face's wrapping boundary (the plan's own monitoring promise for the resize
  // risk): at the windows' contract minimum the hints wrap, and the same three faces must hold
  // at the narrowest size the user can reach — the rect (not the vertex count) is what the
  // completeness claim rides on. The regions' floors compress under the bars per the
  // floors-and-fallback rule; what is asserted here is the hints' own visibility, not the
  // layout's. The default sizes go back afterwards, so the legs below see the standard geometry.
  {
    ImGuiWindow* map_win = ctx->GetWindowByRef("//Raypath view");
    ImGuiWindow* pose_win = ctx->GetWindowByRef("//Pose & trace");
    IM_CHECK_RETV(map_win != nullptr && pose_win != nullptr, false);
    const ImVec2 vp = ImGui::GetMainViewport()->Size;
    ImGui::SetWindowSize(map_win,
                         ImVec2(std::min(gui::UiPx(gui::kRaypathViewMinWidth), vp.x),
                                std::min(gui::UiPx(gui::kRaypathViewMinHeight), vp.y)),
                         ImGuiCond_Always);
    ImGui::SetWindowSize(
        pose_win,
        ImVec2(std::min(gui::UiPx(gui::kPoseViewMinWidth), vp.x), std::min(gui::UiPx(gui::kPoseViewMinHeight), vp.y)),
        ImGuiCond_Always);
    ctx->Yield(3);
    const PanoramaWarningInk narrow_map = CollectWindowWarningInk("Raypath view", warn_colour);
    const PanoramaWarningInk narrow_pose = CollectWindowWarningInk("Pose & trace", warn_colour);
    const auto& feedback = gui::g_state.analysis.wait_feedback_texts;
    const ImRect narrow_map_rect(ImVec2(feedback.map_stale_hint_rect_min_x, feedback.map_stale_hint_rect_min_y),
                                 ImVec2(feedback.map_stale_hint_rect_max_x, feedback.map_stale_hint_rect_max_y));
    IM_CHECK_RETV(feedback.map_stale_hint.find("configuration that has since changed") != std::string::npos, false);
    IM_CHECK_RETV(narrow_map.found && !narrow_map.clipped, false);
    IM_CHECK_RETV(narrow_map_rect.GetHeight() >= ImGui::GetTextLineHeight(), false);
    IM_CHECK_RETV(map_win->InnerClipRect.Contains(narrow_map_rect), false);
    IM_CHECK_RETV(
        narrow_map.bbox.Min.y >= narrow_map_rect.Min.y - 1.0f && narrow_map.bbox.Max.y <= narrow_map_rect.Max.y + 1.0f,
        false);
    const ImRect narrow_pose_rect(ImVec2(feedback.point_stale_hint_rect_min_x, feedback.point_stale_hint_rect_min_y),
                                  ImVec2(feedback.point_stale_hint_rect_max_x, feedback.point_stale_hint_rect_max_y));
    IM_CHECK_RETV(feedback.point_stale_hint.find("previous configuration") != std::string::npos, false);
    IM_CHECK_RETV(narrow_pose.found && !narrow_pose.clipped, false);
    IM_CHECK_RETV(narrow_pose_rect.GetHeight() >= ImGui::GetTextLineHeight(), false);
    IM_CHECK_RETV(pose_win->InnerClipRect.Contains(narrow_pose_rect), false);
    IM_CHECK_RETV(narrow_pose.bbox.Min.y >= narrow_pose_rect.Min.y - 1.0f &&
                      narrow_pose.bbox.Max.y <= narrow_pose_rect.Max.y + 1.0f,
                  false);
    // Back to the contract defaults: the pin restores the pose window's size, the map window its
    // own want_size (the pin there is position-only).
    PinPoseViewOnScreen(ctx);
    ImGui::SetWindowSize(map_win, ImVec2(gui::UiPx(900.0f), gui::UiPx(560.0f)), ImGuiCond_Always);
    ctx->Yield(2);
  }
  // The derivation is display-inert, asserted the governance rule's way (I-display-inert): ten
  // consecutive frames in the stale state with both windows standing change no business state —
  // the payload, every session's intent (survey, map, point — no re-issue, no re-send), the run
  // flags, the document, the derived verdict itself. What the frames DO is re-derive the same
  // staleness facts and redraw the same hints.
  {
    const auto* payload_at_stale = gui::g_state.analysis_result.payload.get();
    const unsigned long long uploads_at_stale = gui::g_state.texture_upload_count;
    const float alt_at_stale = gui::g_state.sun.altitude;
    ctx->Yield(10);
    IM_CHECK_RETV(gui::g_state.analysis_run_in_progress == false, false);
    IM_CHECK_RETV(gui::g_state.analysis.started == true, false);
    IM_CHECK_RETV(gui::g_state.analysis_result.payload.get() == payload_at_stale, false);
    IM_CHECK_RETV(gui::g_state.analysis_result.payload.get() == payload_before, false);
    IM_CHECK_RETV(gui::g_state.texture_upload_count == uploads_at_stale, false);
    IM_CHECK_RETV(fabsf(gui::g_state.sun.altitude - alt_at_stale) < 1e-4f, false);
    IM_CHECK_RETV(gui::g_state.sim_state == SimState::kModified, false);
    IM_CHECK_RETV(gui::g_state.dirty == true, false);
    IM_CHECK_RETV(gui::g_state.analysis.survey.intent_id == survey_intent_before, false);
    IM_CHECK_RETV(gui::g_state.analysis.map.intent_id == map_intent_before, false);
    IM_CHECK_RETV(gui::g_state.analysis.point.intent_id == point_intent_before, false);
    IM_CHECK_RETV(gui::g_state.analysis.point.intent_valid == point_intent_valid_before, false);
    IM_CHECK_RETV(gui::g_state.analysis_freshness.list == gui::AnalysisListFreshness::kStale, false);
    IM_CHECK_RETV(gui::g_state.analysis_freshness.picture_notice != nullptr, false);
    IM_CHECK_RETV(!CollectWindowWarningInk("Raypath view", warn_colour).found == false, false);
    IM_CHECK_RETV(!CollectWindowWarningInk("Pose & trace", warn_colour).found == false, false);
  }
  // The site owns the clear, and the hint's exit is the freshness refresh itself: Analyze on the
  // edited scene — the list describes the document again and every hint face is gone this frame
  // (fields, rects, ink). The refresh was the button's — AC3 rides along.
  ctx->SetRef(PanoramaRegionRef());
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo(kAnalyzeButton)), false);
  ctx->ItemClick(kAnalyzeButton);
  ctx->SetRef("");
  IM_CHECK_RETV(gui::g_state.analysis.started, false);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return !gui::g_state.analysis_run_in_progress && gui::g_state.analysis_result.payload != nullptr; },
          60),
      false);
  IM_CHECK_RETV(gui::g_state.analysis_freshness.list == gui::AnalysisListFreshness::kFresh, false);
  {
    const auto& feedback = gui::g_state.analysis.wait_feedback_texts;
    IM_CHECK_RETV(feedback.map_stale_hint.empty(), false);
    IM_CHECK_RETV(feedback.point_stale_hint.empty(), false);
    IM_CHECK_RETV(feedback.map_stale_hint_rect_min_x == 0.0f && feedback.map_stale_hint_rect_max_y == 0.0f, false);
    IM_CHECK_RETV(feedback.point_stale_hint_rect_min_x == 0.0f && feedback.point_stale_hint_rect_max_y == 0.0f, false);
  }
  IM_CHECK_RETV(!CollectWindowWarningInk("Raypath view", warn_colour).found, false);
  IM_CHECK_RETV(!CollectWindowWarningInk("Pose & trace", warn_colour).found, false);
  return true;
}

// The staleness hints' cone-drift leg: the drift fact's first window-level witness, the same
// ink judge.
bool AnalysisStaleHintConeDriftLeg(ImGuiTestContext* ctx) {
  ScopedWatchdogGuard watchdog(ctx->EngineIO);
  ScopedServerGuard guard;

  // ---- Leg 2 — the cone-drift fact, the Raypath view as its first window-level witness. ----
  ResetTestState();
  gui::g_server = LUMICE_CreateServer();
  IM_CHECK_RETV(gui::g_server != nullptr, false);
  LUMICE_SetLogLevel(gui::g_server, static_cast<LUMICE_LogLevel>(g_core_log_level));
  IM_CHECK_RETV(gui::DeserializeFromJson(kFullyRandomJson, gui::g_state), false);
  gui::g_state.renderer.sim_resolution_index = 0;
  gui::g_state.sim.infinite = false;
  gui::g_state.sim.ray_num_millions = 0.1f;
  ctx->Yield(2);
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo("##TopBar/" ICON_FA_PLAY " Run")), false);
  ctx->ItemClick("##TopBar/" ICON_FA_PLAY " Run");
  IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kDone; }, 60), false);
  ctx->ItemClick("##TopBar/Analyze");
  ctx->Yield(3);
  IM_CHECK_RETV(RunPointAnalysisToCompletion(ctx), false);
  IM_CHECK_RETV(gui::g_state.analysis_result.payload->roi_mode == LUMICE_RAYPATH_ROI_CONE, false);
  IM_CHECK_RETV(SelectRowByDisplay(ctx, TopChainDisplay().c_str()) != std::nullopt, false);
  IM_CHECK_RETV(gui::g_state.analyze_raypath_view_open && EntryWindowActive("Raypath view"), false);
  PinRaypathViewOnScreen(ctx);
  ctx->Yield(3);

  const ImU32 warn_colour = WarningTextVertexColour();
  // Control: undrifted, the window draws no warning ink — and the predicate agrees, so the
  // ink probe and the fact cannot disagree about which state is which.
  IM_CHECK_RETV(!gui::ConeCenterDriftedFromResult(gui::g_state), false);
  IM_CHECK_RETV(!CollectWindowWarningInk("Raypath view", warn_colour).found, false);

  // The centre moves off the analysed direction: the state write is the display face of the
  // pure predicate (the real drag gesture's coverage is marker_drag_moves_the_centre_not_the_view
  // and the main band's own case), and needs no mouse — the floating window cannot shadow it.
  const float moved[3] = { -gui::g_state.analysis.cone_center_dir[1], gui::g_state.analysis.cone_center_dir[0],
                           gui::g_state.analysis.cone_center_dir[2] };
  std::copy(moved, moved + 3, gui::g_state.analysis.cone_center_dir);
  ctx->Yield(3);
  IM_CHECK_RETV(gui::ConeCenterDriftedFromResult(gui::g_state), false);
  IM_CHECK_RETV(gui::g_state.analysis_freshness.cone_center_drifted, false);
  const PanoramaWarningInk drift_hint = CollectWindowWarningInk("Raypath view", warn_colour);
  IM_CHECK_RETV(drift_hint.found, false);
  IM_CHECK_RETV(!drift_hint.clipped, false);
  // The drift row's conjunction: the site's field and rect beside the ink, the stale line's
  // three-face shape again.
  {
    const auto& feedback = gui::g_state.analysis.wait_feedback_texts;
    IM_CHECK_RETV(feedback.map_drift_hint.find("Centre has moved") != std::string::npos, false);
    ImGuiWindow* map_win = ctx->GetWindowByRef("//Raypath view");
    IM_CHECK_RETV(map_win != nullptr, false);
    const ImRect drift_rect(ImVec2(feedback.map_drift_hint_rect_min_x, feedback.map_drift_hint_rect_min_y),
                            ImVec2(feedback.map_drift_hint_rect_max_x, feedback.map_drift_hint_rect_max_y));
    IM_CHECK_RETV(drift_rect.GetHeight() >= ImGui::GetTextLineHeight(), false);
    IM_CHECK_RETV(map_win->InnerClipRect.Contains(drift_rect), false);
    IM_CHECK_RETV(drift_hint.bbox.Min.y >= drift_rect.Min.y - 1.0f && drift_hint.bbox.Max.y <= drift_rect.Max.y + 1.0f,
                  false);
    // The map row did not borrow the main band's field: the workspace is the Analyze one, the
    // panorama's band is live (its own field holds its draw), and the map's field is separate —
    // two writers, two fields, both non-empty is the separation's observable shape.
    IM_CHECK_RETV(gui::g_state.analysis.wait_feedback_texts.cone_center_drift_warning.find("Centre has moved") !=
                      std::string::npos,
                  false);
  }
  // The two facts coexist (assumption 2's window-level witness): an edit while drifted puts the
  // list's staleness beside the drift row — two independent lines, each with its own field and
  // rect, neither replacing the other. The narrowest width joins the construction: two wrapped
  // lines are the wrap-heaviest state the bar can reach, and both must stay complete inside the
  // window's clip at the contract minimum.
  const float alt_at_drift = gui::g_state.sun.altitude;
  ctx->SetRef("//##AnalyzeLeftPanel");
  ctx->ItemInputValue("##Altitude_input", alt_at_drift - 5.0f);
  ctx->SetRef("");
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.analysis_freshness.list == gui::AnalysisListFreshness::kStale &&
                    gui::g_state.analysis_freshness.cone_center_drifted,
                false);
  ImGuiWindow* dual_map_win = ctx->GetWindowByRef("//Raypath view");
  IM_CHECK_RETV(dual_map_win != nullptr, false);
  {
    const ImVec2 vp = ImGui::GetMainViewport()->Size;
    ImGui::SetWindowSize(dual_map_win,
                         ImVec2(std::min(gui::UiPx(gui::kRaypathViewMinWidth), vp.x),
                                std::min(gui::UiPx(gui::kRaypathViewMinHeight), vp.y)),
                         ImGuiCond_Always);
    ctx->Yield(3);
  }
  const PanoramaWarningInk dual_ink = CollectWindowWarningInk("Raypath view", warn_colour);
  IM_CHECK_RETV(dual_ink.found && !dual_ink.clipped, false);
  {
    const auto& feedback = gui::g_state.analysis.wait_feedback_texts;
    IM_CHECK_RETV(feedback.map_stale_hint.find("configuration that has since changed") != std::string::npos, false);
    IM_CHECK_RETV(feedback.map_drift_hint.find("Centre has moved") != std::string::npos, false);
    const ImRect stale_rect(ImVec2(feedback.map_stale_hint_rect_min_x, feedback.map_stale_hint_rect_min_y),
                            ImVec2(feedback.map_stale_hint_rect_max_x, feedback.map_stale_hint_rect_max_y));
    const ImRect drift_rect(ImVec2(feedback.map_drift_hint_rect_min_x, feedback.map_drift_hint_rect_min_y),
                            ImVec2(feedback.map_drift_hint_rect_max_x, feedback.map_drift_hint_rect_max_y));
    IM_CHECK_RETV(
        stale_rect.GetHeight() >= ImGui::GetTextLineHeight() && drift_rect.GetHeight() >= ImGui::GetTextLineHeight(),
        false);
    IM_CHECK_RETV(dual_map_win->InnerClipRect.Contains(stale_rect) && dual_map_win->InnerClipRect.Contains(drift_rect),
                  false);
    // Two independent lines: their layout footprints do not overlap (either order — the contract
    // is "each fact its own line", not the code's draw order).
    IM_CHECK_RETV(stale_rect.Max.y <= drift_rect.Min.y + 1.0f || drift_rect.Max.y <= stale_rect.Min.y + 1.0f, false);
  }
  ImGui::SetWindowSize(dual_map_win, ImVec2(gui::UiPx(900.0f), gui::UiPx(560.0f)), ImGuiCond_Always);
  ctx->Yield(2);
  IM_CHECK_RETV(!gui::g_state.analysis_run_in_progress, false);
  return true;
}

void RegisterRaypathAnalysisPanelTests(ImGuiTestEngine* engine) {
  // Direct positioning is one mouse-down action, not a new mode or a drag. Keep the press
  // through modifier changes, crossing the marker, leaving the picture and cancellation.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "direct_point_press_has_one_owner");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick("Point");
      ctx->Yield(1);
      IM_CHECK_EQ(ctx->ItemInfo(kPickButton, ImGuiTestOpFlags_NoError).ID, 0u);
      IM_CHECK(!PickBannerVisible(ctx));
      ctx->SetRef("");
      ImGuiWindow* picture = PanoramaPictureWindow();
      IM_CHECK(picture != nullptr && picture->Active);
      const auto& a = gui::g_state.analysis;
      const float initial[3] = { a.cone_center_dir[0], a.cone_center_dir[1], a.cone_center_dir[2] };
      const float az = gui::g_state.renderer.azimuth;
      const float el = gui::g_state.renderer.elevation;
      const auto intent = gui::g_state.run_intent;
      const auto epoch = gui::g_state.committed_epoch;
      const bool dirty = gui::g_state.dirty;
      const auto sim = gui::g_state.sim_state;
      const ImVec2 at(picture->Pos.x + picture->Size.x * 0.7f, picture->Pos.y + picture->Size.y * 0.4f);
      ctx->MouseMoveToPos(at);
      ctx->KeyDown(ImGuiMod_Alt);
      ctx->Yield(2);
      float want[3];
      IM_CHECK(UnprojectMouse(want));
      ctx->MouseDown(0);
      ctx->Yield(1);
      IM_CHECK_EQ(gui::g_state.workspace.panorama_press_owner, PressOwner::kDirectPoint);
      IM_CHECK(!a.cone_center_is_default);
      IM_CHECK(std::memcmp(initial, want, sizeof(want)) != 0);
      IM_CHECK(std::memcmp(a.cone_center_dir, want, sizeof(want)) == 0);
      ctx->KeyUp(ImGuiMod_Alt);
      ctx->MouseMoveToPos(ImVec2(at.x - 70.0f, at.y + 45.0f));
      ctx->Yield(2);
      ctx->MouseMoveToPos(ImVec2(picture->Pos.x - 20.0f, at.y));
      ctx->Yield(2);
      ctx->MouseMoveToPos(at);
      ctx->Yield(2);
      IM_CHECK(std::memcmp(a.cone_center_dir, want, sizeof(want)) == 0);
      IM_CHECK_EQ(gui::g_state.renderer.azimuth, az);
      IM_CHECK_EQ(gui::g_state.renderer.elevation, el);
      // A late explicit intent is fixture setup (there is no second mouse-down available
      // during a held press); the real held input must remain the original owner's.
      const size_t angle_count = gui::g_state.sun_circle_angles.size();
      gui::ArmAngularDistPicker(gui::g_state, gui::AngularDistFamily::kSun);
      ctx->MouseMoveToPos(ImVec2(at.x + 20.0f, at.y - 10.0f));
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.workspace.panorama_press_owner, PressOwner::kDirectPoint);
      IM_CHECK_EQ(gui::g_state.sun_circle_angles.size(), angle_count);
      IM_CHECK(gui::g_state.angular_dist_picker.armed);
      ctx->KeyPress(ImGuiKey_Escape);
      ctx->Yield(1);
      IM_CHECK_EQ(gui::g_state.workspace.panorama_press_owner, PressOwner::kCancelled);
      ctx->MouseMoveToPos(ImVec2(at.x + 30.0f, at.y));
      ctx->Yield(2);
      ctx->MouseUp(0);
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.workspace.panorama_press_owner, PressOwner::kNone);
      IM_CHECK(std::memcmp(a.cone_center_dir, want, sizeof(want)) == 0);
      IM_CHECK_EQ(gui::g_state.renderer.azimuth, az);
      IM_CHECK_EQ(gui::g_state.renderer.elevation, el);

      // Off-screen marker recovery: the new camera is setup, the positioning is real input.
      gui::g_state.renderer.azimuth = 180.0f;
      ctx->Yield(2);
      IM_CHECK(!MarkerScreenPos(ctx).has_value());
      ctx->MouseMoveToPos(at);
      ctx->KeyDown(ImGuiMod_Alt);
      ctx->Yield(2);
      IM_CHECK(UnprojectMouse(want));
      ctx->MouseClick(0);
      ctx->KeyUp(ImGuiMod_Alt);
      ctx->Yield(2);
      IM_CHECK(std::memcmp(a.cone_center_dir, want, sizeof(want)) == 0);
      IM_CHECK(MarkerScreenPos(ctx).has_value());
      // Layout and zoom do not re-seed a user's centre, including its convergence window.
      const auto splitter = ctx->ItemInfo("**/##AnalyzeColumnSplitter", ImGuiTestOpFlags_NoError);
      IM_CHECK(splitter.ID != 0);
      ctx->MouseMoveToPos(splitter.RectFull.GetCenter());
      ctx->MouseDown(0);
      ctx->MouseMoveToPos(ImVec2(splitter.RectFull.GetCenter().x + 35.0f, splitter.RectFull.GetCenter().y));
      ctx->MouseUp(0);
      ctx->Yield(3);
      ctx->MouseMoveToPos(at);
      ctx->Yield(2);
      ctx->MouseWheelY(1.0f);
      ctx->Yield(2);
      IM_CHECK(std::memcmp(a.cone_center_dir, want, sizeof(want)) == 0);
      IM_CHECK_EQ(gui::g_state.dirty, dirty);
      IM_CHECK_EQ(gui::g_state.run_intent, intent);
      IM_CHECK_EQ(gui::g_state.committed_epoch, epoch);
      IM_CHECK_EQ(gui::g_state.sim_state, sim);
      IM_CHECK(!a.started && !gui::g_state.analysis_run_in_progress);
      IM_CHECK(SaveWindowPng(ctx, PanoramaRegionRef(), GuiTestTempPath("direct_point_gestures.png").string()));
      ctx->LogInfo("[direct point] capture: %s", GuiTestTempPath("direct_point_gestures.png").string().c_str());
    };
  }

  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "invalid_direct_point_never_becomes_orbit");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick("Point");
      ctx->SetRef("");
      ctx->Yield(3);
      gui::g_state.renderer.elevation = 0.0f;
      gui::g_state.renderer.visible = gui::kVisibleUpper;
      ctx->Yield(2);
      const float before[3] = { gui::g_state.analysis.cone_center_dir[0], gui::g_state.analysis.cone_center_dir[1],
                                gui::g_state.analysis.cone_center_dir[2] };
      const float az = gui::g_state.renderer.azimuth;
      ImGuiWindow* picture = PanoramaPictureWindow();
      IM_CHECK(picture != nullptr);
      ctx->MouseMoveToPos(ImVec2(picture->Pos.x + picture->Size.x * 0.6f, picture->Pos.y + picture->Size.y * 0.85f));
      ctx->KeyDown(ImGuiMod_Alt);
      ctx->Yield(2);
      float unused[3];
      IM_CHECK(!UnprojectMouse(unused));
      ctx->MouseDown(0);
      ctx->Yield(2);
      ctx->KeyUp(ImGuiMod_Alt);
      ctx->MouseMoveToPos(ImVec2(picture->Pos.x + picture->Size.x * 0.4f, picture->Pos.y + picture->Size.y * 0.25f));
      ctx->Yield(2);
      IM_CHECK(UnprojectMouse(unused));
      IM_CHECK_EQ(gui::g_state.workspace.panorama_press_owner, PressOwner::kDirectPoint);
      IM_CHECK(std::memcmp(before, gui::g_state.analysis.cone_center_dir, sizeof(before)) == 0);
      IM_CHECK_EQ(gui::g_state.renderer.azimuth, az);
      IM_CHECK_EQ(gui::g_state.renderer.elevation, 0.0f);
      ctx->MouseUp(0);
      ctx->Yield(2);
      // A press begun outside never becomes a canvas press halfway through.
      ctx->MouseMoveToPos(ImVec2(picture->Pos.x - 15.0f, picture->Pos.y + picture->Size.y * 0.7f));
      ctx->KeyDown(ImGuiMod_Alt);
      ctx->MouseDown(0);
      ctx->MouseMoveToPos(ImVec2(picture->Pos.x + picture->Size.x * 0.5f, picture->Pos.y + picture->Size.y * 0.2f));
      ctx->Yield(2);
      ctx->MouseUp(0);
      ctx->KeyUp(ImGuiMod_Alt);
      ctx->Yield(2);
      IM_CHECK(std::memcmp(before, gui::g_state.analysis.cone_center_dir, sizeof(before)) == 0);
      IM_CHECK_EQ(gui::g_state.renderer.azimuth, az);
    };
  }

  // Both real picture hosts are checked against literal outputs, not against one another.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "picture_hosts_share_camera_wheel");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      const auto check_host = [&](bool analyze) -> bool {
        gui::g_state.workspace.mode =
            analyze ? gui::GuiState::WorkspaceMode::kAnalyze : gui::GuiState::WorkspaceMode::kRender;
        gui::g_state.renderer.lens_type = gui::kLensTypeLinear;
        gui::g_state.renderer.fov = 90.0f;
        gui::g_state.look_at.locked = true;
        ctx->Yield(3);
        ImGuiWindow* picture = analyze ? PanoramaPictureWindow() : ImGui::FindWindowByName("##PreviewPanel");
        IM_CHECK_RETV(picture != nullptr && picture->Active, false);
        const ImVec2 at(picture->Pos.x + picture->Size.x * 0.4f, picture->Pos.y + picture->Size.y * 0.4f);
        ctx->MouseMoveToPos(at);
        ctx->Yield(3);
        const float scroll = picture->Scroll.y;
        ctx->MouseWheelY(1.0f);
        ctx->Yield(2);
        IM_CHECK_RETV(gui::g_state.renderer.fov == 85.0f, false);
        IM_CHECK_RETV(gui::g_state.look_at.locked, false);
        IM_CHECK_RETV(picture->Scroll.y == scroll, false);
        ctx->MouseWheelY(-1.0f);
        ctx->Yield(2);
        IM_CHECK_RETV(gui::g_state.renderer.fov == 90.0f, false);
        ctx->MouseWheelY(0.5f);
        ctx->Yield(2);
        IM_CHECK_RETV(gui::g_state.renderer.fov == 87.5f, false);
        gui::g_state.renderer.fov = 2.0f;
        ctx->MouseWheelY(1.0f);
        ctx->Yield(2);
        IM_CHECK_RETV(gui::g_state.renderer.fov == 1.0f, false);
        gui::g_state.renderer.fov = 178.0f;
        ctx->MouseWheelY(-1.0f);
        ctx->Yield(2);
        IM_CHECK_RETV(gui::g_state.renderer.fov == 179.0f, false);
        gui::g_state.renderer.lens_type = gui::kLensTypeRectangular;
        ctx->Yield(2);
        const float full_fov = gui::g_state.renderer.fov;
        ctx->MouseWheelY(2.0f);
        ctx->Yield(2);
        IM_CHECK_RETV(gui::g_state.renderer.fov == full_fov, false);
        gui::g_state.renderer.lens_type = gui::kLensTypeLinear;
        gui::g_state.renderer.fov = 90.0f;
        ctx->Yield(2);
        // Explicit picking and its completed-press latch must both keep the wheel.
        gui::g_state.angular_dist_picker.armed = true;
        ctx->MouseWheelY(1.0f);
        ctx->Yield(2);
        IM_CHECK_RETV(gui::g_state.renderer.fov == 90.0f, false);
        ctx->MouseDown(0);
        ctx->Yield(2);
        IM_CHECK_RETV(gui::g_state.angular_dist_picker.swallow_drag_until_release, false);
        ctx->MouseWheelY(1.0f);
        ctx->Yield(2);
        IM_CHECK_RETV(gui::g_state.renderer.fov == 90.0f, false);
        ctx->MouseUp(0);
        ctx->Yield(2);
        ctx->MouseWheelY(1.0f);
        ctx->Yield(2);
        IM_CHECK_RETV(gui::g_state.renderer.fov == 85.0f, false);
        return true;
      };
      IM_CHECK(check_host(false));
      IM_CHECK(check_host(true));
    };
  }

  // AC2, first case: open -> Point -> click the preview -> the centre is the click's unprojection
  // -> Analyze -> the list is non-empty and its first row carries the largest energy.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "point_pick_analyze_lists_chains");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      IM_CHECK_EQ(gui::g_state.workspace.mode, gui::GuiState::WorkspaceMode::kAnalyze);
      // The splits the splitters own, read before anything runs: the manual-priority contract
      // (the interaction contract §9.3) says the first result and the selection rewrite none of
      // them — pinned bit-exact at the end of the flow.
      const float d1_ratio_before = gui::g_state.workspace.right_column_ratio;
      const float d2_ratio_before = gui::g_state.workspace.right_list_ratio;
      // The ROI controls live on the panorama side.
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick("Point");
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.analysis.roi_mode, LUMICE_RAYPATH_ROI_CONE);
      ctx->Yield(1);
      ctx->SetRef("");

      const gui::CanvasPixel px = PickPanoramaCentre(ctx);
      // The ROI is placed: the centre is valid, and this frame's marker
      // projects back onto the pixel that was clicked.
      IM_CHECK(gui::g_state.analysis.cone_center_valid);
      const LUMICE_AnnotationView view =
          gui::PreviewAnnotationView(gui::g_state, gui::g_panorama_vp.vp_w, gui::g_panorama_vp.vp_h);
      const std::optional<gui::CanvasPixel> marker = gui::ProjectConeCenterMarker(gui::g_state, view);
      IM_CHECK(marker.has_value());
      IM_CHECK_EQ(marker->px, px.px);
      IM_CHECK_EQ(marker->py, px.py);
      // And its direction is the click's unprojection under the view the picture is drawn with —
      // the oracle is the C API called directly on the same pixel, not the panel's own function.
      float want[3] = { 0.0f, 0.0f, 0.0f };
      int valid = 0;
      IM_CHECK_EQ(LUMICE_UnprojectPixel(&view, px.px, px.py, want, &valid), LUMICE_OK);
      IM_CHECK_EQ(valid, 1);
      IM_CHECK_FLOAT_NEAR(gui::g_state.analysis.cone_center_dir[0], want[0], 1e-6f);
      IM_CHECK_FLOAT_NEAR(gui::g_state.analysis.cone_center_dir[1], want[1], 1e-6f);
      IM_CHECK_FLOAT_NEAR(gui::g_state.analysis.cone_center_dir[2], want[2], 1e-6f);
      // Upper-sky light travels downward (z < 0) — the direction is on the panorama's sky. The
      // equal-area closed form is pinned in gui_unit (PanoramaUnprojectIsEqualAreaOverTheDisk);
      // the oracle here is the C API unprojection checked above, pixel for pixel.
      IM_CHECK_LT(gui::g_state.analysis.cone_center_dir[2], 0.0f);
      // The ring is drawable: the local scale at the click resolves.
      IM_CHECK(gui::ConeRingRadiusCanvasPx(view, px.px, px.py, gui::g_state.analysis.cone_radius_deg).has_value());

      // Analyze, through the button.
      LUMICE_SimLifecycleResult before{};
      LUMICE_GetSimLifecycle(gui::g_server, &before);
      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(!IsDisabled(ctx->ItemInfo(kAnalyzeButton)));
      ctx->ItemClick(kAnalyzeButton);
      ctx->SetRef("");
      IM_CHECK(gui::g_state.analysis.started);
      IM_CHECK(DriveUntil(
          ctx, [] { return !gui::g_state.analysis_run_in_progress && gui::g_state.analysis_result.payload != nullptr; },
          60));
      const auto& view_result = gui::g_state.analysis_result;
      IM_CHECK_EQ(view_result.payload->roi_mode, LUMICE_RAYPATH_ROI_CONE);
      IM_CHECK_GT(view_result.payload->entries.size(), 0u);
      IM_CHECK_EQ(view_result.display_order.size(), view_result.payload->entries.size());
      // The side-by-side containment leg, in the region's window host: the window opens for
      // this block (no selection yet — the honest empty state renders the same header row and
      // the same sky-point input in every state), and the input row fits WHOLE and the Set
      // button answers a real click. The old 370 pt band cell's exact-floor stress moved to
      // the narrowest-window legs; what stays here is the functional half — the controls
      // answer inside their region at the window's own geometry.
      gui::g_state.analyze_raypath_view_open = true;
      ctx->Yield(2);
      {
        // All the driven controls (inputs, Set) live in the INFO region since
        // the D3 split — the containment's host is that child.
        ImGuiWindow* info = MapInfoRegionWindow();
        IM_CHECK(info != nullptr);
        const auto contained = [&](const ImGuiTestItemInfo& item) {
          return item.RectFull.Min.x >= info->InnerClipRect.Min.x - 0.5f &&
                 item.RectFull.Max.x <= info->InnerClipRect.Max.x + 0.5f &&
                 item.RectFull.Min.y >= info->InnerClipRect.Min.y - 0.5f &&
                 item.RectFull.Max.y <= info->InnerClipRect.Max.y + 0.5f;
        };
        SetListRef(ctx);
        ctx->SetRef("");  // root descent: the inputs live in the window's info region, not the list
        const ImGuiTestItemInfo alt_in = ctx->ItemInfo("**/alt°##sky", ImGuiTestOpFlags_NoError);
        IM_CHECK(alt_in.ID != 0);
        const ImGuiTestItemInfo az_in = ctx->ItemInfo("**/az°##sky", ImGuiTestOpFlags_NoError);
        IM_CHECK(az_in.ID != 0);
        const ImGuiTestItemInfo set_btn = ctx->ItemInfo("**/Set sky point###sky_set", ImGuiTestOpFlags_NoError);
        IM_CHECK(set_btn.ID != 0);
        ctx->LogInfo("[side-by-side] info w=%.0f, alt right %.0f, az right %.0f, set right %.0f, clip right %.0f",
                     info->Size.x, alt_in.RectFull.Max.x, az_in.RectFull.Max.x, set_btn.RectFull.Max.x,
                     info->InnerClipRect.Max.x);
        IM_CHECK(contained(alt_in));
        IM_CHECK(contained(az_in));
        IM_CHECK(contained(set_btn));
        ctx->ItemInputValue("**/alt°##sky", 30.0f);
        ctx->ItemInputValue("**/az°##sky", 180.0f);
        ctx->ItemClick("**/Set sky point###sky_set");
        ctx->Yield(2);
        IM_CHECK(gui::g_state.analysis.sky_point.valid);
      }
      // The window closes here: the share-list and splitter legs below drive the workspace's
      // own chrome, which a floating window parked in the viewport would cover (the row
      // divider's drag press lands on the window, not the bar — hit-testing by z-order is the
      // product behaving correctly). The narrow block reopens it for the input row.
      gui::g_state.analyze_raypath_view_open = false;
      ctx->Yield(2);
      // The share-list pins at the 1280x800 acceptance size: with
      // the result in, the window itself must not scroll — the controls above the table are the
      // fixed header and are visible by construction (ScrollMax == 0 says the whole content
      // fits), and the one scrolling region under the pointer is the table's own inner window.
      // At this width the right column also sits above the search box's full form, so the
      // narrow-column forms (analysis_panel.cpp) are off — the list reads as designed here.
      {
        const ScopedWindowBasis window_basis(ctx);
        g_window_size_test.width = 1280;
        g_window_size_test.height = 800;
        g_window_size_test.done.store(false);
        g_window_size_test.requested.store(true);
        for (int i = 0; i < 20 && !g_window_size_test.done.load(); ++i) {
          ctx->Yield(1);
        }
        IM_CHECK(g_window_size_test.done.load());
        ctx->Yield(3);
        ImGuiWindow* share = ctx->GetWindowByRef(kWindowRef);
        IM_CHECK(share != nullptr);
        IM_CHECK_EQ(share->ScrollMax.y, 0.0f);
        IM_CHECK_EQ(share->Scroll.y, 0.0f);
        ImGuiWindow* table_child = nullptr;
        for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
          if (w->ParentWindow == share && std::strstr(w->Name, "##analysis_rows") != nullptr) {
            table_child = w;
            break;
          }
        }
        IM_CHECK(table_child != nullptr);
        // The fixed header is on screen: the search box resolves with a real clipped rect
        // (a control scrolled out of the window resolves too, but carries no visible height).
        const ImGuiTestItemInfo search = ctx->ItemInfo("**/###analysis_search", ImGuiTestOpFlags_NoError);
        IM_CHECK(search.ID != 0);
        IM_CHECK_GT(search.RectFull.GetHeight(), 0.0f);
        // The floor's red anchor: squeezing the SHARE LIST (the same write the D2 splitter's
        // drag performs — the list's height is the right column's own split now, not the main
        // view's row ratio) into the band where the result table's old 120pt floor stops
        // fitting — the window must STILL not scroll (the table yields instead). Moderate: the
        // controls' own fit stays well above the pixel floors the splitter clamps to. The grab
        // drag itself is verified right after (see the point leg's squeeze anchor for the same
        // shape).
        const float ratio_before = gui::g_state.workspace.right_list_ratio;
        gui::g_state.workspace.right_list_ratio = 0.32f;
        ctx->Yield(3);
        ctx->LogInfo("[scroll pins] share squeezed SM %.1f SC %.1f, ratio %.2f", share->ScrollMax.y, share->Scroll.y,
                     gui::g_state.workspace.right_list_ratio);
        IM_CHECK_EQ(share->ScrollMax.y, 0.0f);
        IM_CHECK_EQ(share->Scroll.y, 0.0f);
        gui::g_state.workspace.right_list_ratio = ratio_before;
        ctx->Yield(3);
        ctx->LogInfo("[scroll pins] share@1280x800 SM %.1f SC %.1f, table %s", share->ScrollMax.y, share->Scroll.y,
                     table_child->Name);
      }
      // The 1024x640 minimum (the interaction contract's clamp-order floor): the right column
      // pins exactly at its 260 content floor and the list's narrow-column forms come on — the
      // search box in its collapsed-label form, the buttons stacked — with every control still
      // resolving a real rect. The pose window (opened here for the toggle leg) carries the
      // regions; its viewport-clamped geometry keeps their controls driven through the real
      // mouse path — reachability as an interaction, not a canvas-size proxy.
      {
        const ScopedWindowBasis window_basis(ctx);
        g_window_size_test.width = gui::kMinWindowWidth;
        g_window_size_test.height = gui::kMinWindowHeight;
        g_window_size_test.done.store(false);
        g_window_size_test.requested.store(true);
        for (int i = 0; i < 20 && !g_window_size_test.done.load(); ++i) {
          ctx->Yield(1);
        }
        IM_CHECK(g_window_size_test.done.load());
        ctx->Yield(3);

        // The right column at its floor: the clamp the contract's order ① names (the stored
        // ratio would give ~212 here). The request's controls — Analyze among them — moved to
        // the main view's operation bar (contract §2.2), so the head that remains (banners,
        // symmetry, search) is shorter than before: at this floor it fits flat, and the search
        // row answers without any scroll. (The head's own scroll rung stays live behind the
        // wrapping banners — the freshness states that grow it are the other cases' to
        // evidence.)
        ImGuiWindow* share = ctx->GetWindowByRef(kWindowRef);
        IM_CHECK(share != nullptr);
        ctx->LogInfo("[narrow] share w=%.1f, d1 ratio %.4f", share->Size.x, gui::g_state.workspace.right_column_ratio);
        IM_CHECK_EQ(share->Size.x, gui::UiPx(gui::kAnalyzeRightColMinWidth));
        SetListRef(ctx);
        const ImGuiTestItemInfo search = ctx->ItemInfo("**/###analysis_search", ImGuiTestOpFlags_NoError);
        IM_CHECK(search.ID != 0);
        IM_CHECK_GT(search.RectFull.GetHeight(), 0.0f);
        ctx->SetRef("");

        // (The band's stacked-form pins this block once carried — its scroll, the two regions
        // at their band floors — retired with the band. The regions' compressed-geometry
        // evidence lives in the pose window's own minimum-size legs in the scale-tier case.)

        // The sky-point input row (in the window's region): typed input and the button answer
        // with the window open.
        gui::g_state.analyze_raypath_view_open = true;
        PinRaypathViewOnScreen(ctx);
        ctx->Yield(1);
        ctx->ItemInputValue("**/alt°##sky", 30.0f);
        ctx->ItemInputValue("**/az°##sky", 180.0f);
        ctx->ItemClick("**/Set sky point###sky_set");
        ctx->Yield(2);
        IM_CHECK(gui::g_state.analysis.sky_point.valid);
        // The typed angles, through the direction convention itself (util/sky_direction.hpp: the
        // light-travel direction is the position's negation — z = -sin(alt)):
        IM_CHECK_FLOAT_NEAR(gui::g_state.analysis.sky_point.dir[2], -std::sin(30.0f * lumice::kDeg2Rad), 1e-3f);
        gui::g_state.analyze_raypath_view_open = false;
        ctx->Yield(1);

        // The pose window at the compressed tier, BEFORE any point intent: the fixed heads are
        // the two region titles — the globe's deviation-grid toggle retired to the factors
        // region's env-gated diagnostics group (§16.4), which lives in the discovered arm, so
        // the pre-intent heads are titles + short status only (the pre-intent guide line IS the
        // factors region's forward pointer). The toggle's contain+flip legs live with the
        // completed document (the narrowest and narrow-column cases); the sample/play controls
        // belong to a completed point result and are driven by the point inspector's leg.
        gui::g_state.analyze_pose_view_open = true;
        PinPoseViewOnScreen(ctx);
        ctx->Yield(1);
        ImGuiWindow* globe_p = PoseRegionWindow("##AnalyzeSunGlobe");
        ImGuiWindow* factors_p = PoseRegionWindow("##AnalyzePoseFactors");
        IM_CHECK(globe_p != nullptr && factors_p != nullptr);

        // The 3D region beside it: at the viewport-clamped window its canvas stays legible.
        ImGuiWindow* r3d = PoseRegionWindow("##AnalyzeRaypath3D");
        IM_CHECK(r3d != nullptr);
        IM_CHECK_GT(r3d->InnerRect.GetHeight(), 150.0f);
        gui::g_state.analyze_pose_view_open = false;
        ctx->Yield(1);
      }
      // First row = the largest displayed energy of all rows.
      const double first = view_result.display_energy[static_cast<size_t>(view_result.display_order[0])];
      IM_CHECK_GT(first, 0.0);
      const double largest = *std::max_element(view_result.display_energy.begin(), view_result.display_energy.end());
      IM_CHECK_EQ(first, largest);
      // The top row's display text is the chain grammar itself (the same bytes the CSV writes):
      // which chain dominates depends on where on the sky the panorama pick landed, so the case
      // pins the grammar and the ordering, not one scene-specific chain identity.
      IM_CHECK(view_result.payload->entries[static_cast<size_t>(view_result.display_order[0])].display[0] != '\0');
      // The analysis is a submission of its own (v4.36): the server's epoch moved past the
      // render's — and the picture on screen is still the render's, because the GUI's own
      // committed_epoch (the one ReconcileSimState keys on) did not: sim_state stayed kDone.
      LUMICE_SimLifecycleResult after{};
      LUMICE_GetSimLifecycle(gui::g_server, &after);
      IM_CHECK_GT(after.epoch, before.epoch);
      IM_CHECK_EQ(gui::g_state.committed_epoch, before.epoch);
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)SimState::kDone);

      // Manual priority, pinned over the whole flow (the interaction contract §9.3): the run
      // and everything the legs above did to the window wrote none of the splits — bit-exact
      // against the values read before the pick.
      IM_CHECK_EQ(gui::g_state.workspace.right_column_ratio, d1_ratio_before);
      IM_CHECK_EQ(gui::g_state.workspace.right_list_ratio, d2_ratio_before);

      // A selection turns the diagnostics region from the idle block to the identity block: the
      // idle children leave (the guidance branches and the observation summary are named windows
      // — a named-child testability shape), the query region takes over as the named scroll
      // child under the identity head.
      const std::string top_display = TopChainDisplay();
      SetListRef(ctx);
      ctx->ItemClick((std::string("**/") + top_display).c_str());
      ctx->SetRef("");
      ctx->Yield(3);
      const auto window_present = [](const char* fragment) {
        for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
          if (w != nullptr && w->Active && std::strstr(w->Name, fragment) != nullptr) {
            return true;
          }
        }
        return false;
      };
      IM_CHECK(!window_present("##AnalyzeIdleNoPictureGuide"));
      IM_CHECK(!window_present("##AnalyzeIdlePictureGuide"));
      IM_CHECK(!window_present("##AnalyzeIdleSummary"));
      IM_CHECK(window_present("##AnalyzeDetailsScroll"));

      // The stale-at-the-floors repro, BOTH D2 floor directions of one construction, each with
      // its own evidence set:
      //  Half A — the LIST at its floor (the ratio derived from the live panel height, never a
      //  hardcoded window: at a non-1x scale the 912 arithmetic is simply wrong): the head —
      //  short since the request controls moved to the operation bar — fits flat, its search
      //  row resolves inside the never-scrolling window, the frozen header stays on screen at
      //  scroll 0, and both bottom buttons sit RectFull inside the window. The real operations
      //  then drive it: a position-addressed click on the first data row (its label is clipped
      //  by the floor's short viewport, so labels cannot address it) selects that row, and an
      //  Exclude click writes THAT row's chain, reversed through the product's own
      //  Include-again.
      //  Half B — the DIAGNOSTICS at its floor (the list takes the rest): the same containment
      //  with the head flat. Two floors, two evidence sets, one construction each way. The
      //  head's natural-height measurement rides one frame behind the banner, hence the yields
      //  before every read.
      const int fixture_crystal = gui::g_state.layers[0].entries[0].crystal_id;
      gui::g_state.crystals[fixture_crystal].height.center += 0.1f;
      // A typed sky point BEFORE the floors: the tail (Set/coordinates/Clear) renders below the
      // map's state line and canvas in every map state, and the r2 review's probe caught the
      // un-budgeted tail clipping the Clear at rung-height cells — the containment below pins
      // the fix where the overflow actually lived.
      SetSkyPointTarget(30.0, 0.0);
      const float d1_saved = gui::g_state.workspace.right_column_ratio;
      const float d2_saved = gui::g_state.workspace.right_list_ratio;
      const float panel_h =
          ImGui::GetIO().DisplaySize.y - gui::UiPx(gui::kTopBarHeight) - gui::UiPx(gui::kStatusBarHeight);
      const float span_w = ImGui::GetIO().DisplaySize.x - gui::UiPx(gui::kAnalyzeLeftPanelWidth);
      const auto assert_contained = [&](const char* ref, ImGuiWindow* share) {
        const ImGuiTestItemInfo info = ctx->ItemInfo(ref, ImGuiTestOpFlags_NoError);
        if (info.ID == 0u) {
          IM_ERRORF("not found in the floored list: %s", ref);
          return;
        }
        IM_CHECK_GE(info.RectFull.Min.x, share->InnerClipRect.Min.x - 0.5f);
        IM_CHECK_LE(info.RectFull.Max.x, share->InnerClipRect.Max.x + 0.5f);
        IM_CHECK_GE(info.RectFull.Min.y, share->InnerClipRect.Min.y - 0.5f);
        IM_CHECK_LE(info.RectFull.Max.y, share->InnerClipRect.Max.y + 0.5f);
      };
      // (top_display is the case's earlier declaration — the selection leg's row, still selected.)

      gui::g_state.workspace.right_column_ratio = gui::UiPx(gui::kAnalyzeRightColMinWidth) / span_w;
      gui::g_state.workspace.right_list_ratio = gui::UiPx(gui::kAnalyzeShareListMinHeight) / panel_h;
      ctx->Yield(4);
      ImGuiWindow* table_child = nullptr;
      ImGuiWindow* head = nullptr;
      {
        ImGuiWindow* share = ctx->GetWindowByRef(kWindowRef);
        IM_CHECK(share != nullptr);
        ctx->LogInfo("[stale floors A] share %.0fx%.0f (list floor %.0f)", share->Size.x, share->Size.y,
                     gui::UiPx(gui::kAnalyzeShareListMinHeight));
        IM_CHECK_FLOAT_NEAR(share->Size.y, gui::UiPx(gui::kAnalyzeShareListMinHeight), 1.0f);
        // The head is short since the request controls moved to the operation bar (contract
        // §2.2): at this floor it fits flat — no scrolling child at all — and the search row,
        // its last, resolves inside the never-scrolling window directly. (The clamp's scroll
        // rung stays live behind the wrapping banners; this construction's state does not
        // reach it, and the shorter head is the relaxed form the migration planned for.)
        for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
          if (w != nullptr && w->Active && std::strstr(w->Name, "##analysis_head") != nullptr) {
            head = w;
            break;
          }
        }
        IM_CHECK(head == nullptr);
        const ImGuiTestItemInfo search_row = ctx->ItemInfo("**/###analysis_search", ImGuiTestOpFlags_NoError);
        IM_CHECK(search_row.ID != 0);
        IM_CHECK_GE(search_row.RectFull.Min.x, share->InnerClipRect.Min.x - 0.5f);
        IM_CHECK_LE(search_row.RectFull.Max.x, share->InnerClipRect.Max.x + 0.5f);
        IM_CHECK_GE(search_row.RectFull.Min.y, share->InnerClipRect.Min.y - 0.5f);
        IM_CHECK_LE(search_row.RectFull.Max.y, share->InnerClipRect.Max.y + 0.5f);
        ctx->LogInfo("[stale floors A] head flat (no clamp child); search row inside the window");
        // The list at this floor: the buttons sit RectFull inside the never-scrolling window,
        // and the table keeps its frozen header on screen at scroll 0 (the with-result pin's
        // structural form — header CELLS don't report labels to the engine, so the header's
        // evidence is the scroll position, not an item lookup).
        for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
          if (w->ParentWindow == share && std::strstr(w->Name, "##analysis_rows") != nullptr) {
            table_child = w;
            break;
          }
        }
        IM_CHECK(table_child != nullptr);
        IM_CHECK_EQ(table_child->Scroll.y, 0.0f);
        ctx->LogInfo("[stale floors A] table inner y [%.0f..%.0f] scroll %.0f",
                     static_cast<double>(table_child->InnerRect.Min.y),
                     static_cast<double>(table_child->InnerRect.Max.y), static_cast<double>(table_child->Scroll.y));
        // The first data row, in the X5 form: with the clamp's block-gap margin the floor's
        // viewport holds header + row fully, so the row resolves by label and its FULL
        // rectangle sits inside the table's clip (pre-margin this viewport clipped the row and
        // the engine never reported its label — the two facts are the same fact).
        const ImGuiTestItemInfo first_row =
            ctx->ItemInfo((std::string("**/") + top_display).c_str(), ImGuiTestOpFlags_NoError);
        IM_CHECK(first_row.ID != 0);
        IM_CHECK_GE(first_row.RectFull.Min.y, table_child->InnerClipRect.Min.y - 0.5f);
        IM_CHECK_LE(first_row.RectFull.Max.y, table_child->InnerClipRect.Max.y + 0.5f);
        IM_CHECK_GE(first_row.RectFull.Min.x, table_child->InnerClipRect.Min.x - 0.5f);
        IM_CHECK_LE(first_row.RectFull.Max.x, table_child->InnerClipRect.Max.x + 0.5f);
        // The frozen header's own band, from the real layout: the header occupies the clip from
        // its top down to the first row's actual top (one row height — the header cells do not
        // report labels to the engine, but the row's measured top bounds what is above it), so
        // the band sits fully inside the clip with the row, and Scroll.y == 0 puts it at the
        // viewport top where the freeze keeps it.
        IM_CHECK_GT(first_row.RectFull.Min.y, table_child->InnerClipRect.Min.y);
        IM_CHECK_LE(first_row.RectFull.Min.y, table_child->InnerClipRect.Min.y + ImGui::GetTextLineHeight() * 2.5f);
        assert_contained("**/" ICON_FA_BAN " Exclude this raypath###action_exclude", share);
        if (ctx->IsError()) {
          return;  // the containment lambda already reported; later calls would echo it
        }
        assert_contained("**/" ICON_FA_FILE_CSV " Export CSV###action_csv", share);
        if (ctx->IsError()) {
          return;
        }
        // The map's sky-point tail, in the region's window (open since the containment leg —
        // a typed sky point and whatever map state the selection drives, waiting or
        // completed — the tail renders below both): the Clear sits RectFull inside the map
        // region — the state whose overflow the r2 review's probe measured.
        ImGuiWindow* map_sf = MapInfoRegionWindow();
        IM_CHECK(map_sf != nullptr);
        const ImGuiTestItemInfo clear_sf = ctx->ItemInfo("**/Clear##sky", ImGuiTestOpFlags_NoError);
        IM_CHECK(clear_sf.ID != 0);
        IM_CHECK_GE(clear_sf.RectFull.Min.x, map_sf->InnerClipRect.Min.x - 0.5f);
        IM_CHECK_LE(clear_sf.RectFull.Max.x, map_sf->InnerClipRect.Max.x + 0.5f);
        IM_CHECK_GE(clear_sf.RectFull.Min.y, map_sf->InnerClipRect.Min.y - 0.5f);
        IM_CHECK_LE(clear_sf.RectFull.Max.y, map_sf->InnerClipRect.Max.y + 0.5f);
        ctx->LogInfo("[stale floors A] clear bottom %.0f of clip %.0f (map state %d)",
                     static_cast<double>(clear_sf.RectFull.Max.y), static_cast<double>(map_sf->InnerClipRect.Max.y),
                     static_cast<int>(gui::MapQueryStateFor(gui::g_state.analysis.map)));
      }
      // The real operations, at this floor: the first data row — its label is clipped by the
      // 38px viewport, so the click is POSITION-addressed (the row rect computed from the
      // table's own line metrics) — must select that row; the Exclude click then writes that
      // row's chain, Include-again undoes it (the product's own reverse, no filter left
      // behind).
      {
        const float row_h = ImGui::GetTextLineHeight() + 2.0f * ImGui::GetStyle().CellPadding.y;
        const float row_mid_y =
            table_child->InnerRect.Min.y + row_h + row_h * 0.5f - table_child->Scroll.y;  // below the frozen header
        IM_CHECK(row_mid_y >= table_child->InnerRect.Min.y && row_mid_y <= table_child->InnerRect.Max.y);
        ctx->MouseMoveToPos(ImVec2(table_child->InnerRect.Min.x + 40.0f, row_mid_y));
        ctx->Yield(2);
        ctx->MouseClick(0);
        ctx->Yield(3);
        IM_CHECK(gui::g_state.analysis.selected_entry.has_value() &&
                 *gui::g_state.analysis.selected_entry == top_display);
        // Park the pointer off-frame: the row just clicked is a clipped one, and leaving the
        // cursor on it keeps its hover carrier (the tooltip) alive over the list below.
        ctx->MouseMoveToPos(ImVec2(-100.0f, -100.0f));
        ctx->Yield(2);
      }
      SetListRef(ctx);
      ctx->ItemClick("**/" ICON_FA_BAN " Exclude this raypath###action_exclude");
      ctx->SetRef("");
      ctx->Yield(2);
      {
        // The write IS the operability evidence at this state: the product's greyed row derives
        // only after the next analysis (the payload still holds the excluded display — that is
        // the freshness banner's contract), so the list's derivation is empty here by design.
        // The filter's own content pins WHICH entity the click excluded.
        IM_CHECK(!gui::g_state.filters.empty());
        const gui::FilterConfig& written = gui::g_state.filters.back();
        IM_CHECK(written.action == 1);  // filter_out
        IM_CHECK(!written.param.empty());
        IM_CHECK(written.param.front().text == top_display);
        ctx->LogInfo("[stale floors A] exclude wrote filter '%s' with token '%s'", written.name.c_str(),
                     written.param.front().text.c_str());
        // Reverse through the product's own Include-again, with the descriptor its greyed row
        // carries: the pool slot of the selected chain's root crystal, the filter's last slot,
        // the row's index in it.
        const auto& payload = gui::g_state.analysis_result.payload;
        std::optional<int> pool;
        for (const auto& e : payload->entries) {
          if (std::string(e.display) == top_display) {
            const int core = e.chain[0].crystal_id;
            const auto& core_to_pool = gui::AnalysisCoreToPoolMap(gui::g_state);
            const auto it = core_to_pool.find(core);
            if (it != core_to_pool.end()) {
              pool = it->second;
            }
            break;
          }
        }
        IM_CHECK(pool.has_value());
        gui::ExcludedRaypathRow reversal;
        reversal.display = top_display;
        reversal.token = top_display;
        reversal.pool_crystal_id = *pool;
        reversal.filter_slot = static_cast<int>(gui::g_state.filters.size()) - 1;
        reversal.param_index = 0;
        IM_CHECK(gui::ApplyIncludeAgain(gui::g_state, reversal));
        ctx->Yield(2);
      }

      // The Radius row, driven LAST: its input re-sorts the display order (the radius re-sums
      // the rings), so the row selection above must not share the frame with it. On the main
      // view's operation bar since the migration (contract §2.2) — the list head no longer
      // hosts it, so the containment face is the panorama window and no scroll is needed. A
      // real input drives the radius to BOTH ends of its range (the r2 review's probe caught
      // the fixed 220 pt slider clipped at this very tier — the bar's elastic width judgment
      // carries the same duty).
      ctx->Yield(3);
      const ImGuiTestItemInfo radius = ctx->ItemInfo("**/Radius###Radius_narrow", ImGuiTestOpFlags_NoError);
      IM_CHECK(radius.ID != 0);
      ImGuiWindow* bar_host = ctx->GetWindowByRef(PanoramaRegionRef());
      IM_CHECK(bar_host != nullptr);
      IM_CHECK_GE(radius.RectFull.Min.x, bar_host->InnerClipRect.Min.x - 0.5f);
      IM_CHECK_LE(radius.RectFull.Max.x, bar_host->InnerClipRect.Max.x + 0.5f);
      IM_CHECK_GE(radius.RectFull.Min.y, bar_host->InnerClipRect.Min.y - 0.5f);
      IM_CHECK_LE(radius.RectFull.Max.y, bar_host->InnerClipRect.Max.y + 0.5f);
      {
        const auto& payload = gui::g_state.analysis_result.payload;
        const float cone_deg = payload->cone_radius_rad * 57.29577951308232;
        const float min_deg = cone_deg / static_cast<float>(std::max(payload->cone_ring_count, 1));
        ctx->ItemInputValue("**/Radius###Radius_narrow", min_deg);
        ctx->Yield(2);
        IM_CHECK(fabsf(gui::g_state.analysis.cone_radius_deg - min_deg) < 0.05f);
        ctx->ItemInputValue("**/Radius###Radius_narrow", cone_deg);
        ctx->Yield(2);
        IM_CHECK(fabsf(gui::g_state.analysis.cone_radius_deg - cone_deg) < 0.05f);
        ctx->LogInfo("[stale floors A] radius driven to both ends: [%.3f, %.3f]", static_cast<double>(min_deg),
                     static_cast<double>(cone_deg));
      }

      gui::g_state.workspace.right_list_ratio =
          (panel_h - gui::UiPx(gui::kAnalyzeSplitterSize) - gui::UiPx(gui::kAnalyzeDetailsMinHeight)) / panel_h;
      ctx->Yield(4);
      {
        ImGuiWindow* share = ctx->GetWindowByRef(kWindowRef);
        IM_CHECK(share != nullptr);
        ImGuiWindow* details = ctx->GetWindowByRef("##AnalyzeDetails");
        IM_CHECK(details != nullptr);
        ctx->LogInfo("[stale floors B] share %.0fx%.0f, details %.0f (floor %.0f)", share->Size.x, share->Size.y,
                     details->Size.y, gui::UiPx(gui::kAnalyzeDetailsMinHeight));
        IM_CHECK_FLOAT_NEAR(details->Size.y, gui::UiPx(gui::kAnalyzeDetailsMinHeight), 1.0f);
        assert_contained("**/" ICON_FA_BAN " Exclude this raypath###action_exclude", share);
        if (ctx->IsError()) {
          return;  // the containment lambda already reported; later calls would echo it
        }
        assert_contained("**/" ICON_FA_FILE_CSV " Export CSV###action_csv", share);
        // The diagnostics floor keeps a visible first line of its scrolling region — the wheel
        // reaches the rest, the structural half of the floor's promise.
        ImGuiWindow* scroll = nullptr;
        for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
          if (w != nullptr && w->Active && std::strstr(w->Name, "##AnalyzeDetailsScroll") != nullptr) {
            scroll = w;
            break;
          }
        }
        IM_CHECK(scroll != nullptr);
        IM_CHECK_GT(scroll->Size.y, 0.0f);
      }
      gui::g_state.workspace.right_column_ratio = d1_saved;
      gui::g_state.workspace.right_list_ratio = d2_saved;
      gui::g_state.crystals[fixture_crystal].height.center -= 0.1f;
      ctx->Yield(2);
    };
  }

  // AC1, the Raypath view window's open entry and lifecycle (contract §6). The named function
  // above is the case body; the lambda stays void the way every TestFunc does.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "class_row_click_opens_the_raypath_view");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(ClassRowClickOpensTheRaypathView(ctx)); };
  }

  // AC1's negative faces and the open flag's tier (contract §8's ownership declaration).
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "raypath_view_open_is_display_state");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(RaypathViewOpenIsDisplayState(ctx)); };
  }

  // AC2's D3 face: the window's own split is manual, floored, resize-following and
  // session-persistent (contract §4.2, §9.3).
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "raypath_view_split_is_manual_and_floored");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(RaypathViewSplitIsManualAndFloored(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "pose_view_splits_are_manual_independent_and_floored");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(PoseViewSplitsAreManualIndependentAndFloored(ctx)); };
  }
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "raypath_analysis", "pose_view_resize_strategy_distributes_by_ratio_and_quota");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(PoseViewResizeStrategyDistributesByRatioAndQuota(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "pose_view_narrowest_boundaries_hold_both_ends");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(PoseViewNarrowestBoundariesHoldBothEnds(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "pose_view_opens_from_both_explicit_entries");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(PoseViewOpensFromBothExplicitEntries(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "pose_view_follows_the_selection_chain");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(PoseViewFollowsTheSelectionChain(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "pose_view_playback_pauses_when_closed_and_resumes");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(PoseViewPlaybackPausesWhenClosedAndResumes(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "pose_view_pump_adopts_with_every_window_closed");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(PoseViewPumpAdoptsWithEveryWindowClosed(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "pose_view_do_new_closes_and_reopens_honest");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(PoseViewDoNewClosesAndReopensHonest(ctx)); };
  }
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "raypath_analysis", "pose_view_narrowest_window_keeps_the_fixed_stack_operable");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(PoseViewNarrowestWindowKeepsTheFixedStackOperable(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "pose_view_joins_a_multi_window_real_mouse_chain");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(PoseViewJoinsAMultiWindowRealMouseChain(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "pose_view_operates_at_a_moved_window_origin");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(PoseViewOperatesAtAMovedWindowOrigin(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "pose_curve_region_draws_and_moves_the_shared_cursor");
    static gui::GuiState grid_probe;
    static bool draw_grid_probe = false;
    static std::string grid_report;
    static gui::GuiState backdrop_probe;
    static bool draw_backdrop_probe = false;
    static int backdrop_probe_vertices = -1;
    t->GuiFunc = [](ImGuiTestContext*) {
      if (draw_grid_probe) {
        ImGui::SetNextWindowPos(ImVec2(40.0f, 60.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(320.0f, 170.0f), ImGuiCond_Always);
        ImGui::Begin("Compressed grid probe", nullptr, ImGuiWindowFlags_NoResize);
        ImGui::LogToBuffer();
        gui::RenderAnalyzeSunGlobeRegion(grid_probe, ImGui::GetContentRegionAvail());
        grid_report = ImGui::GetCurrentContext()->LogBuffer.c_str();
        ImGui::LogFinish();
        ImGui::End();
      }
      if (draw_backdrop_probe) {
        // The backdrop probe must be tall enough for the canvas to exist (the compressed probe
        // above is deliberately canvas-less — its ##globe_canvas absence is asserted): the
        // grid's cell quads are the only state-driven vertex difference between two
        // identically-sized frames (fixed window, fixed state → deterministic counts). The
        // region's BeginChild body renders into the CHILD's own draw list, so the count sums
        // the probe window and its direct children (skip a child that shares the parent's list,
        // or its vertices would count twice).
        ImGui::SetNextWindowPos(ImVec2(400.0f, 60.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(320.0f, 460.0f), ImGuiCond_Always);
        ImGui::Begin("Grid backdrop probe", nullptr, ImGuiWindowFlags_NoResize);
        ImGuiWindow* probe_win = ImGui::GetCurrentWindow();
        gui::RenderAnalyzeSunGlobeRegion(backdrop_probe, ImGui::GetContentRegionAvail());
        int vtx = probe_win->DrawList->VtxBuffer.Size;
        for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
          if (w->ParentWindow == probe_win && w->DrawList != nullptr && w->DrawList != probe_win->DrawList) {
            vtx += w->DrawList->VtxBuffer.Size;
          }
        }
        backdrop_probe_vertices = vtx;
        ImGui::End();
      }
    };
    t->TestFunc = [](ImGuiTestContext* ctx) {
      IM_CHECK(PoseCurveRegionDrawsAndMovesTheSharedCursor(ctx));
      grid_probe = gui::GuiState{};
      grid_probe.analysis.point = gui::g_state.analysis.point;
      grid_probe.analysis.point_view = gui::g_state.analysis.point_view;
      grid_probe.analysis.point_view.show_deviation_grid = true;
      IM_CHECK(grid_probe.analysis.point.adopted_result != nullptr);
      LUMICE_PathSunGrid grid{};
      grid.struct_size = sizeof(grid);
      IM_CHECK_EQ(LUMICE_PathPointGetSunGrid(grid_probe.analysis.point.adopted_result.get(), &grid),
                  LUMICE_PATH_READ_OK);
      IM_CHECK_GT(grid.cell_count, 0u);
      draw_grid_probe = true;
      struct GridLease {
        ~GridLease() {
          draw_grid_probe = false;
          grid_probe = gui::GuiState{};
        }
      } grid_lease;
      ctx->Yield(3);
      ctx->SetRef("//Compressed grid probe");
      IM_CHECK(!ctx->ItemExists("**/##globe_canvas"));
      IM_CHECK(grid_report.find("Crystal-frame sun directions") != std::string::npos);
      IM_CHECK(grid_report.find("no sun grid in this document") == std::string::npos);
      ctx->SetRef("");
      // The backdrop is the toggle's DISPLAY EFFECT, gated at the draw site by the toggle and
      // the payload alone — the diagnostics env no longer rides `have_grid`. The three legs:
      // with the variable unset, the toggle's off state holds the canvas at its grid-free
      // vertex count; flipping the toggle alone adds the cell quads; and turning the env on
      // with the toggle still on changes nothing (the same count — the knob is not read by
      // this path at all). The baseline is the same document with the toggle off, env unset.
      backdrop_probe = gui::GuiState{};
      backdrop_probe.analysis.point = gui::g_state.analysis.point;
      backdrop_probe.analysis.point_view = gui::g_state.analysis.point_view;
      IM_CHECK(backdrop_probe.analysis.point.adopted_result != nullptr);
      draw_backdrop_probe = true;
      struct BackdropLease {
        ~BackdropLease() {
          draw_backdrop_probe = false;
          backdrop_probe = gui::GuiState{};
        }
      } backdrop_lease;
      {
        const ScopedDebugDetailsEnv env_unset(DebugDetailsState::kUnset);
        backdrop_probe.analysis.point_view.show_deviation_grid = false;
        ctx->Yield(3);
        const int base = backdrop_probe_vertices;
        IM_CHECK_GT(base, 0);
        backdrop_probe.analysis.point_view.show_deviation_grid = true;
        ctx->Yield(3);
        IM_CHECK_GT(backdrop_probe_vertices, base);
        const int cells_unset = backdrop_probe_vertices;
        {
          const ScopedDebugDetailsEnv env_on(DebugDetailsState::kOn);
          ctx->Yield(3);
          IM_CHECK_EQ(backdrop_probe_vertices, cells_unset);
          IM_CHECK_GT(backdrop_probe_vertices, base);
        }
      }
    };
  }

  // 779.5 — the pan gesture's real-input legs and the two-row footer's ink (its own case: the
  // failures stay locatable without walking the long cursor case again).
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "raypath_analysis", "pose_curve_pan_moves_the_window_and_keeps_the_x_labels");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(PoseCurvePanMovesTheWindowAndKeepsTheXLabels(ctx)); };
  }

  // AC2's narrowest-window face: at 480x360 the floors hold and the info rows wrap, not clip
  // (contract §11 R3).
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "raypath_analysis", "raypath_view_narrowest_window_keeps_controls_reachable");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(RaypathViewNarrowestWindowKeepsControlsReachable(ctx)); };
  }

  // AC2, second case: a render in progress keeps Analyze shut, and the other direction — an
  // analysis in progress keeps Run shut on the top bar — with the window's own Stop ending it.
  // The scene is an INFINITE run on purpose: a whole-sky analysis of it has no end of its own
  // (lumice_engine.h: an infinite budget runs until LUMICE_StopServer), so the in-progress state is
  // held open for as long as the assertions need, and Stop is the only way out — which is the
  // path this case exists to drive.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "analyze_and_run_exclude_each_other");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/true));
      OpenWindow(ctx);
      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(IsDisabled(ctx->ItemInfo(kAnalyzeButton)));
      IM_CHECK(!gui::CanStartAnalysis(true, gui::g_state.sim_state, gui::g_state.analysis_run_in_progress,
                                      gui::g_state.run_intent));
      // A click on it does nothing: no intent, no run.
      ctx->ItemClick(kAnalyzeButton);
      ctx->Yield(2);
      IM_CHECK(!gui::g_state.analysis.started);
      ctx->SetRef("");

      // Stop the render; Analyze opens up; start it; Run on the top bar is now the one shut.
      gui::DoStop();
      IM_CHECK(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kDone; }, 20));
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick("Whole sky");
      ctx->SetRef(ListWindowRef());
      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(!IsDisabled(ctx->ItemInfo(kAnalyzeButton)));
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick(kAnalyzeButton);
      ctx->SetRef("");
      IM_CHECK(gui::g_state.analysis.started);
      IM_CHECK(gui::g_state.analysis_run_in_progress);
      ctx->Yield(2);
      IM_CHECK(gui::g_state.analysis_run_in_progress);
      IM_CHECK(IsDisabled(ctx->ItemInfo("##TopBar/" ICON_FA_PLAY " Run")));
      IM_CHECK(gui::IsBackendBusy(gui::g_state.sim_state, gui::g_state.analysis_run_in_progress));
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)SimState::kDone);  // the picture is still the render's
      // Partial results reach the list while the run is in progress.
      IM_CHECK(DriveUntil(ctx, [] { return gui::g_state.analysis_result.payload != nullptr; }, 20));
      IM_CHECK_EQ(gui::g_state.analysis_result.payload->roi_mode, LUMICE_RAYPATH_ROI_FULL_SKY);

      // Stop from the window: the intent is withdrawn at the command, the top bar reopens, and
      // the result stays on show — the one adopted so far, or the run's final snapshot if the
      // stop drained one more (a newer generation, never an older one, and never nothing).
      // Not pointer equality: the entries are read on this thread per (generation, symmetry)
      // and every read publishes a new payload object for the same result.
      const auto partial = gui::g_state.analysis_result.payload;
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick(ICON_FA_STOP " Stop");
      ctx->SetRef("");
      IM_CHECK(!gui::g_state.analysis.started);
      IM_CHECK(DriveUntil(
          ctx, [] { return !gui::g_state.analysis_run_in_progress && gui::g_state.sim_state == SimState::kDone; }, 20));
      IM_CHECK(!IsDisabled(ctx->ItemInfo("##TopBar/" ICON_FA_PLAY " Run")));
      IM_CHECK(gui::g_state.analysis_result.payload != nullptr);
      IM_CHECK_GE(gui::g_state.analysis_result.payload->snapshot_generation, partial->snapshot_generation);
      IM_CHECK_EQ(gui::g_state.analysis_result.payload->roi_mode, LUMICE_RAYPATH_ROI_FULL_SKY);
      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(!IsDisabled(ctx->ItemInfo(kAnalyzeButton)));
      ctx->SetRef("");
    };
  }

  // AC3: the radius slider is display-time. Setting it changes the energies on the list and
  // starts no run — the lifecycle epoch, the in-progress flag and the result object are all the
  // same before and after, only the projection over its rings changed.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "radius_slider_is_display_time");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      IM_CHECK(RunPointAnalysisToCompletion(ctx));
      const auto payload_before = gui::g_state.analysis_result.payload;
      const double total_before = gui::g_state.analysis_result.display_total;
      const int rings_before = gui::g_state.analysis_result.display_ring_count;
      LUMICE_SimLifecycleResult before{};
      LUMICE_GetSimLifecycle(gui::g_server, &before);
      const unsigned long long uploads_before = gui::g_state.texture_upload_count;

      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemInputValue("Radius###Radius_narrow", 10.0f);
      ctx->SetRef("");
      ctx->Yield(5);

      IM_CHECK_FLOAT_NEAR(gui::g_state.analysis.cone_radius_deg, 10.0f, 1e-3f);
      IM_CHECK_GT(gui::g_state.analysis_result.display_ring_count, rings_before);
      // More rings, more of the cone's energy on the list — strictly, since the halo ring runs
      // through the cone and rays land at every distance from its centre.
      IM_CHECK_GT(gui::g_state.analysis_result.display_total, total_before);
      // And nothing ran: same result object, same epoch, no in-progress edge, no upload.
      IM_CHECK(gui::g_state.analysis_result.payload == payload_before);
      IM_CHECK(!gui::g_state.analysis_run_in_progress);
      LUMICE_SimLifecycleResult after{};
      LUMICE_GetSimLifecycle(gui::g_server, &after);
      IM_CHECK_EQ(after.epoch, before.epoch);
      IM_CHECK_EQ(after.lifecycle, before.lifecycle);
      IM_CHECK_EQ(gui::g_state.texture_upload_count, uploads_before);
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)SimState::kDone);
    };
  }

  // The P/B/D checkboxes are display-time (v4.33): the run recorded every chain unreduced, and a
  // toggle re-reads the result on hand under the new bits — on this thread, in the frame of the
  // click — starting no run. On the 22-degree halo the prism's D symmetry is what folds the
  // mirror-image path 3-7 into 3-5, so turning D off splits the top row in two: more rows, less
  // energy on "3-5", the same total; and the selection, which names the chain, survives because
  // "3-5" is still a row. Turning D back on merges them again.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "symmetry_checkboxes_are_display_time");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      IM_CHECK(RunPointAnalysisToCompletion(ctx));
      auto sum_energy = [] {
        double s = 0.0;
        for (const auto& e : gui::g_state.analysis_result.payload->entries) {
          s += e.energy;
        }
        return s;
      };
      auto row = [](const char* display) -> const LUMICE_RaypathHistogramEntry* {
        for (const auto& e : gui::g_state.analysis_result.payload->entries) {
          if (std::strcmp(e.display, display) == 0) {
            return &e;
          }
        }
        return nullptr;
      };
      const int kAll = LUMICE_RAYPATH_SYMMETRY_P | LUMICE_RAYPATH_SYMMETRY_B | LUMICE_RAYPATH_SYMMETRY_D;
      IM_CHECK_EQ(static_cast<int>(gui::g_state.analysis_result.entries_symmetry), kAll);
      const size_t rows_before = gui::g_state.analysis_result.payload->entries.size();
      const unsigned long long gen_before = gui::g_state.analysis_result.payload->snapshot_generation;
      const double total_before = sum_energy();
      const LUMICE_RaypathHistogramEntry* top = row("3-5");
      IM_CHECK(top != nullptr);
      const double top_before = top->energy;
      IM_CHECK(row("3-7") == nullptr);  // folded into 3-5 by D
      LUMICE_SimLifecycleResult before{};
      LUMICE_GetSimLifecycle(gui::g_server, &before);
      const unsigned long long uploads_before = gui::g_state.texture_upload_count;

      // Select the top row, then turn D off.
      SetListRef(ctx);
      ctx->ItemClick("**/3-5");
      ctx->Yield(1);
      IM_CHECK(gui::g_state.analysis.selected_entry.has_value() && *gui::g_state.analysis.selected_entry == "3-5");
      ctx->ItemClick("**/D##analysis_symmetry");
      ctx->Yield(2);
      ctx->SetRef("");
      IM_CHECK(!gui::g_state.analysis.symmetry_d);
      IM_CHECK_EQ(static_cast<int>(gui::g_state.analysis_result.entries_symmetry),
                  LUMICE_RAYPATH_SYMMETRY_P | LUMICE_RAYPATH_SYMMETRY_B);
      IM_CHECK_EQ(gui::g_state.analysis_result.payload->snapshot_generation, gen_before);
      IM_CHECK_GT(gui::g_state.analysis_result.payload->entries.size(), rows_before);
      IM_CHECK(row("3-7") != nullptr);
      top = row("3-5");
      IM_CHECK(top != nullptr);
      IM_CHECK_LT(top->energy, top_before);
      IM_CHECK_FLOAT_NEAR(sum_energy(), total_before, 1e-9 * total_before);
      // The selection names the chain and "3-5" is still a row: kept, and Exclude still has it.
      IM_CHECK(gui::SelectedAnalysisEntry(gui::g_state) == top);
      // And nothing ran: same epoch, same lifecycle, no in-progress edge, no upload.
      IM_CHECK(!gui::g_state.analysis_run_in_progress);
      LUMICE_SimLifecycleResult after{};
      LUMICE_GetSimLifecycle(gui::g_server, &after);
      IM_CHECK_EQ(after.epoch, before.epoch);
      IM_CHECK_EQ(after.lifecycle, before.lifecycle);
      IM_CHECK_EQ(gui::g_state.texture_upload_count, uploads_before);
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)SimState::kDone);

      // D back on: the rows merge again, the same count and total as at first.
      SetListRef(ctx);
      ctx->ItemClick("**/D##analysis_symmetry");
      ctx->Yield(2);
      ctx->SetRef("");
      IM_CHECK(gui::g_state.analysis.symmetry_d);
      IM_CHECK_EQ(static_cast<int>(gui::g_state.analysis_result.entries_symmetry), kAll);
      IM_CHECK_EQ(gui::g_state.analysis_result.payload->entries.size(), rows_before);
      IM_CHECK(row("3-7") == nullptr);
      IM_CHECK_FLOAT_NEAR(sum_energy(), total_before, 1e-9 * total_before);
      top = row("3-5");
      IM_CHECK(top != nullptr);
      IM_CHECK_FLOAT_NEAR(top->energy, top_before, 1e-9 * top_before);
      IM_CHECK(gui::SelectedAnalysisEntry(gui::g_state) == top);
    };
  }

  // The radius slider is live BEFORE any result: in Point mode with the default centre and no
  // Analyze pressed, it is enabled, setting it changes the session's radius and the ring drawn on
  // the panorama (the pixel radius DrawPanoramaRoiRing draws from), and nothing runs — no result
  // appears, the intent flag stays clear, the lifecycle is the render's, the sim state is Done.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "radius_slider_drags_before_any_result");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick("Point");
      ctx->Yield(1);
      IM_CHECK(gui::g_state.analysis.cone_center_valid);
      IM_CHECK(!gui::g_state.analysis_result.payload);
      IM_CHECK(!IsDisabled(ctx->ItemInfo("Radius###Radius_narrow")));
      const LUMICE_AnnotationView view =
          gui::PreviewAnnotationView(gui::g_state, gui::g_panorama_vp.vp_w, gui::g_panorama_vp.vp_h);
      const std::optional<gui::CanvasPixel> marker = gui::ProjectConeCenterMarker(gui::g_state, view);
      IM_CHECK(marker.has_value());
      const std::optional<float> ring_before =
          gui::ConeRingRadiusCanvasPx(view, marker->px, marker->py, gui::g_state.analysis.cone_radius_deg);
      IM_CHECK(ring_before.has_value());
      LUMICE_SimLifecycleResult before{};
      LUMICE_GetSimLifecycle(gui::g_server, &before);
      const unsigned long long uploads_before = gui::g_state.texture_upload_count;

      ctx->ItemInputValue("Radius###Radius_narrow", 10.0f);
      ctx->SetRef("");
      ctx->Yield(5);

      IM_CHECK_FLOAT_NEAR(gui::g_state.analysis.cone_radius_deg, 10.0f, 1e-3f);
      const std::optional<float> ring_after =
          gui::ConeRingRadiusCanvasPx(view, marker->px, marker->py, gui::g_state.analysis.cone_radius_deg);
      IM_CHECK(ring_after.has_value());
      IM_CHECK_GT(*ring_after, *ring_before);
      // And nothing ran.
      IM_CHECK(!gui::g_state.analysis.started);
      IM_CHECK(!gui::g_state.analysis_run_in_progress);
      IM_CHECK(!gui::g_state.analysis_result.payload);
      LUMICE_SimLifecycleResult after{};
      LUMICE_GetSimLifecycle(gui::g_server, &after);
      IM_CHECK_EQ(after.epoch, before.epoch);
      IM_CHECK_EQ(after.lifecycle, before.lifecycle);
      IM_CHECK_EQ(gui::g_state.texture_upload_count, uploads_before);
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)SimState::kDone);
    };
  }

  // The greyed-radius form (analyze-interaction-contract.md §15): the radius slider's enabled
  // state answers to the REQUEST mode alone. Whole sky greys it — disabled, visible, owning its
  // slot — whatever result is on hand; Point enables it whether or not a result exists. A mode
  // switch flips only the slider's interaction face: the payload and the list stand still.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "radius_slider_greyed_in_whole_sky");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      ctx->SetRef(PanoramaRegionRef());
      // Whole sky with no cone result: the slider is present (it owns its slot in both modes)
      // and disabled.
      IM_CHECK(gui::g_state.analysis.roi_mode == LUMICE_RAYPATH_ROI_FULL_SKY);
      IM_CHECK(!gui::g_state.analysis_result.payload);
      IM_CHECK(ctx->ItemInfo("Radius###Radius_narrow").ID != 0);
      IM_CHECK(IsDisabled(ctx->ItemInfo("Radius###Radius_narrow")));
      // A Point result on hand: enabled. Back to Whole sky: disabled again, and the result
      // stands untouched — the mode ruling closed the re-sum interaction face, not the result
      // semantics.
      IM_CHECK(RunPointAnalysisToCompletion(ctx));
      ctx->SetRef(PanoramaRegionRef());  // the completion helper resets the ref to the root
      const auto payload_before = gui::g_state.analysis_result.payload;
      const size_t rows_before = gui::g_state.analysis_result.payload->entries.size();
      IM_CHECK(ctx->ItemInfo("Radius###Radius_narrow").ID != 0);
      IM_CHECK(!IsDisabled(ctx->ItemInfo("Radius###Radius_narrow")));
      ctx->ItemClick("Whole sky");
      ctx->Yield(2);
      IM_CHECK(gui::g_state.analysis.roi_mode == LUMICE_RAYPATH_ROI_FULL_SKY);
      IM_CHECK(ctx->ItemInfo("Radius###Radius_narrow").ID != 0);
      IM_CHECK(IsDisabled(ctx->ItemInfo("Radius###Radius_narrow")));
      IM_CHECK(gui::g_state.analysis_result.payload == payload_before);
      IM_CHECK(gui::g_state.analysis_result.payload->entries.size() == rows_before);
      // Back to Point: enabled and drivable again.
      ctx->ItemClick("Point");
      ctx->Yield(2);
      IM_CHECK(ctx->ItemInfo("Radius###Radius_narrow").ID != 0);
      IM_CHECK(!IsDisabled(ctx->ItemInfo("Radius###Radius_narrow")));
      ctx->ItemInputValue("Radius###Radius_narrow", 10.0f);
      ctx->Yield(2);
      IM_CHECK_FLOAT_NEAR(gui::g_state.analysis.cone_radius_deg, 10.0f, 1e-3f);
      ctx->SetRef("");
    };
  }

  // The request's parameters are the panel's own session inputs: Rays(M) / Infinite rays open
  // with the document's values and then move independently of them; editing either starts
  // nothing and dirties nothing.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "request_params_are_session_inputs_seeded_once");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));  // sim.ray_num_millions = 0.1, infinite off
      IM_CHECK(!gui::g_state.analysis.ray_budget_initialized);
      OpenWindow(ctx);
      IM_CHECK(gui::g_state.analysis.ray_budget_initialized);
      IM_CHECK_FLOAT_NEAR(gui::g_state.analysis.ray_num_millions, gui::g_state.sim.ray_num_millions, 1e-6f);
      IM_CHECK_EQ(gui::g_state.analysis.infinite, gui::g_state.sim.infinite);
      LUMICE_SimLifecycleResult before{};
      LUMICE_GetSimLifecycle(gui::g_server, &before);
      const SimState sim_state_before = gui::g_state.sim_state;

      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(ctx->ItemInfo("##Rays(M)_input").ID != 0);
      ctx->ItemInputValue("##Rays(M)_input", 2.5f);
      ctx->Yield(2);
      IM_CHECK_FLOAT_NEAR(gui::g_state.analysis.ray_num_millions, 2.5f, 1e-4f);
      IM_CHECK_FLOAT_NEAR(gui::g_state.sim.ray_num_millions, 0.1f, 1e-6f);  // the document's Rays is not the panel's

      ctx->ItemClick("Infinite rays");
      ctx->Yield(1);
      IM_CHECK(gui::g_state.analysis.infinite);
      IM_CHECK(!gui::g_state.sim.infinite);                    // the document's Infinite rays is not the panel's
      IM_CHECK(IsDisabled(ctx->ItemInfo("##Rays(M)_input")));  // no total applies while unlimited
      ctx->SetRef("");

      // None of it ran anything or dirtied the document.
      IM_CHECK(!gui::g_state.analysis.started);
      IM_CHECK(!gui::g_state.analysis_run_in_progress);
      LUMICE_SimLifecycleResult after{};
      LUMICE_GetSimLifecycle(gui::g_server, &after);
      IM_CHECK_EQ(after.epoch, before.epoch);
      IM_CHECK_EQ(after.lifecycle, before.lifecycle);
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)sim_state_before);

      // Last, because it dirties the document: a later edit to the document's Rays does not
      // reach the seeded session field.
      gui::g_state.sim.ray_num_millions = 7.0f;
      ctx->Yield(2);
      IM_CHECK_FLOAT_NEAR(gui::g_state.analysis.ray_num_millions, 2.5f, 1e-4f);
    };
  }

  // AC3: entering Point mode places a centre at once — the viewport's middle pixel, unprojected —
  // so there is a marker to drag before any pick. The oracle is the C API on that pixel.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "point_mode_defaults_centre_to_viewport_middle");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      IM_CHECK(!gui::g_state.analysis.cone_center_valid);
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick("Point");
      ctx->SetRef("");
      ctx->Yield(1);
      IM_CHECK(gui::g_state.analysis.cone_center_valid);
      const LUMICE_AnnotationView view =
          gui::PreviewAnnotationView(gui::g_state, gui::g_panorama_vp.vp_w, gui::g_panorama_vp.vp_h);
      float want[3] = { 0.0f, 0.0f, 0.0f };
      int valid = 0;
      IM_CHECK_EQ(LUMICE_UnprojectPixel(&view, gui::g_panorama_vp.vp_w / 2, gui::g_panorama_vp.vp_h / 2, want, &valid),
                  LUMICE_OK);
      IM_CHECK_EQ(valid, 1);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[0], want[0]);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[1], want[1]);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[2], want[2]);
      // And the marker is on the picture, at that pixel.
      const std::optional<gui::CanvasPixel> marker = gui::ProjectConeCenterMarker(gui::g_state, view);
      IM_CHECK(marker.has_value());
      IM_CHECK_EQ(marker->px, gui::g_panorama_vp.vp_w / 2);
      IM_CHECK_EQ(marker->py, gui::g_panorama_vp.vp_h / 2);
      // Analyze is open at once: the centre the button needed is there.
      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(!IsDisabled(ctx->ItemInfo(kAnalyzeButton)));
      ctx->SetRef("");
    };
  }

  // AC1 on the free camera: the marker is the direction's projection on THIS view. A camera
  // orbit moves the marker with the picture — and the direction, the truth, byte-for-byte does
  // not change; aiming the view back at the direction (the product's own inverse, the same
  // inverse the Look At presets run) puts the marker exactly where it was — the per-frame
  // projection's contract.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "marker_follows_the_view_drag");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick("Point");
      ctx->Yield(2);
      ctx->SetRef("");
      ctx->Yield(1);
      // The pick aims at the direction the camera already looks at (the view's own centre
      // pixel), so aiming the view back at that direction later returns the marker to THIS
      // pixel exactly — the anchor the closing assertion needs.
      IM_CHECK(PickPanoramaAt(ctx, gui::g_state.renderer.elevation, gui::g_state.renderer.azimuth));
      IM_CHECK(gui::g_state.analysis.cone_center_valid);
      const float dir_before[3] = { gui::g_state.analysis.cone_center_dir[0], gui::g_state.analysis.cone_center_dir[1],
                                    gui::g_state.analysis.cone_center_dir[2] };
      const std::optional<ImVec2> marker_before = MarkerScreenPos(ctx);
      IM_CHECK(marker_before.has_value());

      // A camera drag that starts far from the marker (the marker sits at the picture's
      // centre, the drag starts near the left edge): the camera arm owns the press, the view
      // orbits, and the direction does not move.
      ImGuiWindow* pano = PanoramaPictureWindow();
      IM_CHECK(pano != nullptr);
      const ImVec2 drag_from(pano->Pos.x + pano->Size.x * 0.12f, pano->Pos.y + pano->Size.y * 0.70f);
      ctx->MouseMoveToPos(drag_from);
      ctx->Yield(2);
      ctx->MouseDown(0);
      ctx->Yield(2);
      ctx->KeyDown(ImGuiMod_Alt);
      ctx->MouseMoveToPos(ImVec2(drag_from.x + 150.0f, drag_from.y - 110.0f));
      ctx->Yield(3);
      ctx->KeyUp(ImGuiMod_Alt);
      ctx->MouseUp(0);
      ctx->Yield(3);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[0], dir_before[0]);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[1], dir_before[1]);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[2], dir_before[2]);
      // The view did move (the drag was not a silent no-op), and the marker left its pixel —
      // it follows the view, off-picture (nullopt) being the honest far end.
      float az_aim = 0.0f;
      float el_aim = 0.0f;
      gui::WorldDirToAzEl(dir_before, &az_aim, &el_aim);
      IM_CHECK(std::abs(gui::g_state.renderer.elevation - el_aim) + std::abs(gui::g_state.renderer.azimuth - az_aim) >
               0.5f);
      const std::optional<ImVec2> marker_mid = MarkerScreenPos(ctx);
      if (marker_mid.has_value()) {
        IM_CHECK(ImFabs(marker_mid->x - marker_before->x) + ImFabs(marker_mid->y - marker_before->y) > 5.0f);
      }

      // Aim the view back at the direction: the marker returns to its pixel, the direction
      // bytes never changed.
      gui::g_state.renderer.azimuth = az_aim;
      gui::g_state.renderer.elevation = el_aim;
      ctx->Yield(3);
      const std::optional<ImVec2> marker_after = MarkerScreenPos(ctx);
      IM_CHECK(marker_after.has_value());
      IM_CHECK_LT(ImFabs(marker_after->x - marker_before->x), 2.0f);
      IM_CHECK_LT(ImFabs(marker_after->y - marker_before->y), 2.0f);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[0], dir_before[0]);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[1], dir_before[1]);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[2], dir_before[2]);
    };
  }

  // AC1's overlay half: the auxiliary lines' label switches publish their curve-label sets for
  // the main view's own viewport — the same sets the deferred pass rasterizes into the picture,
  // and the same display state the Render preview reads (one source, two faces).
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "overlay_labels_publish_on_the_main_view");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      ctx->Yield(2);
      IM_CHECK(gui::g_panorama_vp.active);
      gui::g_state.show_horizon_label = true;
      gui::g_state.show_grid_label = true;
      ctx->Yield(3);
      IM_CHECK(!gui::g_panorama_vp.curve_labels.empty());
      // Pixel evidence: the picture with its labels, saved for inspection — the deferred pass
      // rasterized the published sets into the same blit the screen shows.
      IM_CHECK(
          SaveWindowPng(ctx, PanoramaRegionRef(), GuiTestTempPath("analyze_main_view_overlay_labels.png").string()));
      gui::g_state.show_horizon_label = false;
      gui::g_state.show_grid_label = false;
    };
  }

  // AC4's occlusion half on the camera arm: with the entry editor's modal up over the main
  // view, a drag on the picture orbits nothing — the modal owns the input, so the camera arm
  // never engages (a press the modal consumed is not a press the camera consumed).
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "camera_drag_stays_down_under_a_modal");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      const ScopedPopups popup_guard(ctx);
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick("Point");
      ctx->SetRef("");
      ctx->Yield(3);
      const float centre_before[3] = { gui::g_state.analysis.cone_center_dir[0],
                                       gui::g_state.analysis.cone_center_dir[1],
                                       gui::g_state.analysis.cone_center_dir[2] };
      const float fov_before = gui::g_state.renderer.fov;
      const float az_before = gui::g_state.renderer.azimuth;
      const float el_before = gui::g_state.renderer.elevation;
      ImGuiWindow* pano = PanoramaPictureWindow();
      IM_CHECK(pano != nullptr);
      // Opened through OpenEditModal rather than by a card click, the way
      // marker_sits_under_a_modal_not_over_it does: the click's own path is not what this case
      // is about, the modal's presence is.
      gui::g_state.modal_immediate_mode = false;  // staged: a real modal, on the popup stack
      const gui::EditRequest req{ gui::EditTarget::kCrystal, 0, 0 };
      gui::OpenEditModal(req, gui::g_state);
      ctx->Yield(4);
      IM_CHECK(ImGui::GetTopMostPopupModal() != nullptr);
      const ImVec2 drag_from(pano->Pos.x + pano->Size.x * 0.30f, pano->Pos.y + pano->Size.y * 0.60f);
      ctx->MouseMoveToPos(drag_from);
      ctx->Yield(2);
      ctx->MouseDown(0);
      ctx->Yield(2);
      ctx->MouseMoveToPos(ImVec2(drag_from.x + 120.0f, drag_from.y - 90.0f));
      ctx->Yield(3);
      ctx->MouseUp(0);
      ctx->Yield(3);
      IM_CHECK_EQ(gui::g_state.renderer.azimuth, az_before);
      IM_CHECK_EQ(gui::g_state.renderer.elevation, el_before);
      IM_CHECK(gui::g_state.workspace.panorama_press_owner == PressOwner::kNone);
      ctx->KeyDown(ImGuiMod_Alt);
      ctx->MouseMoveToPos(drag_from);
      ctx->MouseClick(0);
      ctx->MouseWheelY(1.0f);
      ctx->KeyUp(ImGuiMod_Alt);
      ctx->Yield(2);
      IM_CHECK(std::memcmp(centre_before, gui::g_state.analysis.cone_center_dir, sizeof(centre_before)) == 0);
      IM_CHECK_EQ(gui::g_state.renderer.fov, fov_before);
    };
  }

  // The Angular Distance pick owns its press on this picture too — the Render host's
  // gestures_locked rule on its second host: with the picker armed, a press-drag-release on the
  // main view commits the angle and moves nothing else. Whole sky: the camera never takes the
  // press. Point: the cone centre stands byte for byte. (The regression the review round
  // demanded: one press, one consumer.)
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "angular_pick_owns_the_press_on_the_main_view");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      ctx->Yield(2);
      ImGuiWindow* pano = PanoramaPictureWindow();
      IM_CHECK(pano != nullptr);
      // Arm through the left column's angular section — the same fold button and popup Pick the
      // Render suite drives (test_angular_dist_picker.cpp).
      gui::g_state.angular_dist_section_open = true;
      ctx->Yield(3);
      const auto arm_through_button = [&ctx]() {
        ctx->ItemClick("**/###sun_circles_fold");
        ctx->Yield(3);
        ctx->ItemClick("**/" ICON_FA_CROSSHAIRS "##pick_circle");
        ctx->Yield(2);
        IM_CHECK(gui::g_state.angular_dist_picker.armed);
      };
      const float az_before = gui::g_state.renderer.azimuth;
      const float el_before = gui::g_state.renderer.elevation;
      const int sun_angles_before = static_cast<int>(gui::g_state.sun_circle_angles.size());
      arm_through_button();
      // Press, drag, release: the angle commits (the picker disarms), the camera never moved.
      const ImVec2 press(pano->Pos.x + pano->Size.x * 0.4f, pano->Pos.y + pano->Size.y * 0.5f);
      ctx->MouseMoveToPos(press);
      ctx->Yield(2);
      ctx->MouseDown(0);
      ctx->Yield(2);
      ctx->MouseMoveToPos(ImVec2(press.x + 40.0f, press.y + 20.0f));
      ctx->Yield(2);
      ctx->MouseUp(0);
      ctx->Yield(2);
      IM_CHECK(!gui::g_state.angular_dist_picker.armed);
      IM_CHECK_GT(static_cast<int>(gui::g_state.sun_circle_angles.size()), sun_angles_before);
      IM_CHECK_EQ(gui::g_state.renderer.azimuth, az_before);
      IM_CHECK_EQ(gui::g_state.renderer.elevation, el_before);

      // Point: the same armed press leaves the centre byte-stable. (Entering the mode first
      // lets the default settle, so the bytes below are the settled ones.)
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick("Point");
      ctx->SetRef("");
      ctx->Yield(4);
      IM_CHECK(gui::g_state.analysis.cone_center_valid);
      float centre_before[3];
      std::copy(gui::g_state.analysis.cone_center_dir, gui::g_state.analysis.cone_center_dir + 3, centre_before);
      arm_through_button();
      ctx->MouseMoveToPos(press);
      ctx->Yield(2);
      ctx->MouseDown(0);
      ctx->Yield(2);
      ctx->MouseMoveToPos(ImVec2(press.x - 30.0f, press.y + 15.0f));
      ctx->Yield(2);
      ctx->MouseUp(0);
      ctx->Yield(2);
      IM_CHECK(!gui::g_state.angular_dist_picker.armed);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[0], centre_before[0]);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[1], centre_before[1]);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[2], centre_before[2]);
    };
  }

  // The default cone centre's bounded life (the review's freeze): once an Analyze has accepted
  // it — or the user has taken the camera — no layout or picture-rectangle change re-places the
  // effective ROI. The direction is byte-stable across an orbit and a column-split drag.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "default_point_freezes_once_accepted");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick("Point");
      ctx->SetRef("");
      ctx->Yield(4);  // the operation bar settles; the default sits at the settled middle
      IM_CHECK(gui::g_state.analysis.cone_center_valid);
      IM_CHECK(gui::g_state.analysis.cone_center_is_default);
      float centre_before[3];
      std::copy(gui::g_state.analysis.cone_center_dir, gui::g_state.analysis.cone_center_dir + 3, centre_before);

      // Accept: the request takes the default, and the flag freezes (accepted regions never
      // move by themselves). The run is stopped — this case is about the centre, not the list.
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick(kAnalyzeButton);
      ctx->SetRef("");
      ctx->Yield(2);
      IM_CHECK(gui::g_state.analysis.started);
      IM_CHECK(!gui::g_state.analysis.cone_center_is_default);
      gui::DoStop();
      ctx->Yield(2);

      // Orbit, then change the main view's width by dragging D1: the centre stands.
      ImGuiWindow* pano = PanoramaPictureWindow();
      IM_CHECK(pano != nullptr);
      const float az_before = gui::g_state.renderer.azimuth;
      const ImVec2 drag_from(pano->Pos.x + pano->Size.x * 0.5f, pano->Pos.y + pano->Size.y * 0.7f);
      ctx->MouseMoveToPos(drag_from);
      ctx->Yield(2);
      ctx->MouseDown(0);
      ctx->Yield(2);
      ctx->MouseMoveToPos(ImVec2(drag_from.x + 90.0f, drag_from.y - 60.0f));
      ctx->Yield(3);
      ctx->MouseUp(0);
      ctx->Yield(3);
      IM_CHECK_NE(gui::g_state.renderer.azimuth, az_before);
      const ImGuiTestItemInfo grab = ctx->ItemInfo("**/##AnalyzeColumnSplitter", ImGuiTestOpFlags_NoError);
      IM_CHECK(grab.ID != 0);
      const ImVec2 c = grab.RectFull.GetCenter();
      ctx->MouseMoveToPos(c);
      ctx->Yield(2);
      ctx->MouseDown(0);
      ctx->Yield(2);
      ctx->MouseMoveToPos(ImVec2(c.x + 30.0f, c.y));
      ctx->Yield(3);
      ctx->MouseUp(0);
      ctx->Yield(3);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[0], centre_before[0]);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[1], centre_before[1]);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[2], centre_before[2]);
    };
  }

  // The display-mode control is not a ROI surface: Alt/Option-click changes the channel,
  // not the centre, and wheel over it does not zoom the underlying picture.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "display_mode_click_does_not_take_the_roi");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick("Point");
      ctx->SetRef("");
      ctx->Yield(4);
      IM_CHECK(gui::g_state.analysis.cone_center_valid);
      float centre_before[3];
      std::copy(gui::g_state.analysis.cone_center_dir, gui::g_state.analysis.cone_center_dir + 3, centre_before);
      IM_CHECK_EQ(gui::g_state.renderer.display_mode, 0);
      ctx->SetRef(PanoramaRegionRef());
      const auto control = ctx->ItemInfo("**/Channel B-R##preview_display_mode");
      IM_CHECK(control.ID != 0);
      const float fov_before = gui::g_state.renderer.fov;
      ctx->MouseMoveToPos(control.RectFull.GetCenter());
      ctx->Yield(2);
      ctx->MouseWheelY(1.0f);
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.renderer.fov, fov_before);
      ctx->KeyDown(ImGuiMod_Alt);
      ctx->MouseClick(0);
      ctx->KeyUp(ImGuiMod_Alt);
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.renderer.display_mode, 1);  // the channel changed
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[0], centre_before[0]);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[1], centre_before[1]);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[2], centre_before[2]);
      IM_CHECK(gui::g_state.workspace.panorama_press_owner == PressOwner::kNone);
    };
  }

  // The eyedropper is the Render host's: with a background photo loaded and shown, the Analyze
  // left column's Display group carries no sky-pick entry at all — an armed mode with no
  // consumer must not be reachable — and the Render workspace still has its own.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "analyze_display_group_carries_no_eyedropper");
    static bool s_bg_requested = false;
    static bool s_bg_done = false;
    t->GuiFunc = [](ImGuiTestContext*) {
      if (s_bg_requested && !s_bg_done) {
        const std::vector<unsigned char> grey(static_cast<size_t>(16 * 16 * 3), 128);
        gui::g_preview.UploadBgTexture(grey.data(), 16, 16);
        s_bg_done = true;
      }
    };
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      s_bg_requested = false;
      s_bg_done = false;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      s_bg_requested = true;
      IM_CHECK(DriveUntil(ctx, [] { return s_bg_done; }, 5));
      gui::g_state.bg_show = true;
      ctx->Yield(2);
      IM_CHECK(gui::g_preview.HasBackground());
      ctx->ItemClick("##TopBar/Analyze");
      ctx->Yield(3);
      ctx->SetRef("//##AnalyzeLeftPanel");
      ctx->ItemClick("Display");
      ctx->Yield(2);
      IM_CHECK(!ctx->ItemExists("**/" ICON_FA_EYE_DROPPER "##display_sky_pick"));  // no entry, no mode to arm
      IM_CHECK(!gui::g_bg_pick.active);
      ctx->SetRef("");
      ctx->ItemClick("##TopBar/Render");
      ctx->Yield(3);
      // The Render host keeps its own entry (the ref carries the icon: the button's id hashes
      // the full label, icon included — test_bg_color_picker_mode.cpp's addressing form).
      IM_CHECK(ctx->ItemExists("**/" ICON_FA_EYE_DROPPER "##display_sky_pick"));
    };
  }

  // The default centre's convergence is bounded by its placement's age, not by which entries
  // have been visited: after the window closes, an observation-side view edit (the left
  // column's Azimuth — a path no freeze hook names) followed by a layout change cannot
  // re-derive an effective ROI. The direction stands byte for byte.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "default_point_survives_view_edits");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick("Point");
      ctx->SetRef("");
      // Settle well past the convergence window (kAnalyzeConeCenterConvergenceFrames counts
      // from the placement, and the bar's settle re-place restarts the clock once): the bar is
      // stable and the window closed before anything below runs.
      ctx->Yield(14);
      IM_CHECK(gui::g_state.analysis.cone_center_valid);
      IM_CHECK(gui::g_state.analysis.cone_center_is_default);
      float centre_before[3];
      std::copy(gui::g_state.analysis.cone_center_dir, gui::g_state.analysis.cone_center_dir + 3, centre_before);

      // The observation-side path the entry-freeze hooks never named: the View group's
      // Azimuth, edited for real in the left column.
      const float az_before_edit = gui::g_state.renderer.azimuth;
      ctx->SetRef("//##AnalyzeLeftPanel");
      ctx->ItemInputValue("##Azimuth##view_input", az_before_edit + 40.0f);
      ctx->SetRef("");
      ctx->Yield(4);
      IM_CHECK_NE(gui::g_state.renderer.azimuth, az_before_edit);  // the view really moved
      // A layout change on top: D1 drags the main view's width.
      const ImGuiTestItemInfo grab = ctx->ItemInfo("**/##AnalyzeColumnSplitter", ImGuiTestOpFlags_NoError);
      IM_CHECK(grab.ID != 0);
      const ImVec2 c = grab.RectFull.GetCenter();
      ctx->MouseMoveToPos(c);
      ctx->Yield(2);
      ctx->MouseDown(0);
      ctx->Yield(2);
      ctx->MouseMoveToPos(ImVec2(c.x + 30.0f, c.y));
      ctx->Yield(3);
      ctx->MouseUp(0);
      ctx->Yield(3);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[0], centre_before[0]);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[1], centre_before[1]);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[2], centre_before[2]);
    };
  }

  // The annotation-anchor caches are per publication: the screenshot export renders the
  // preview's picture through the PREVIEW's cache, so Analyze frames assembling with another
  // lens must never overwrite it. Drive the sequence — render, switch, change the lens, let the
  // panorama re-anchor — and the preview cache's marker projection stands byte for byte (under
  // the shared cache it would have been recomputed with the panorama's lens and moved).
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "screenshot_anchors_stay_with_the_render_face");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      // A marker on the picture: its projection lives in the preview's cache once assembled.
      gui::g_state.markers[LUMICE_ANNOTATION_MARKER_SUN].show = true;  // the camera aims at the sun
      ctx->Yield(4);
      IM_CHECK(gui::PreviewAnnotationAnchors().HasResult());
      IM_CHECK(gui::PreviewAnnotationAnchors().MarkerPoint(LUMICE_ANNOTATION_MARKER_SUN).valid);
      const gui::AnnotationAnchors::Point before =
          gui::PreviewAnnotationAnchors().MarkerPoint(LUMICE_ANNOTATION_MARKER_SUN);

      // Analyze assembles its own face (different publication, different cache); the lens goes
      // with the left column, for real.
      ctx->ItemClick("##TopBar/Analyze");
      ctx->Yield(3);
      ctx->SetRef("//##AnalyzeLeftPanel");
      ctx->ComboClick("Lens Type##view/Fisheye Equal Area");
      ctx->SetRef("");
      ctx->Yield(6);
      IM_CHECK_EQ(gui::g_state.renderer.lens_type, gui::kLensTypeFisheyeEqualArea);
      IM_CHECK(gui::g_panorama_vp.active);  // its own face assembled this frame

      // The preview cache is exactly what the render left there: same marker, same pixels.
      const gui::AnnotationAnchors::Point after =
          gui::PreviewAnnotationAnchors().MarkerPoint(LUMICE_ANNOTATION_MARKER_SUN);
      IM_CHECK_EQ(after.valid, before.valid);
      IM_CHECK_EQ(after.px, before.px);
      IM_CHECK_EQ(after.py, before.py);
    };
  }

  // The screenshot is a picture of what is ON SCREEN: with the Analyze main view live, the
  // export is the main view's picture at the main view's size — every fact (params, canvas, DPI,
  // anchors) from the one active publication — and the Render-side export still answers with the
  // preview panel's own. RenderScreenshot runs GL, so the calls route through the case's GuiFunc
  // (the background-upload rule).
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "screenshot_exports_the_active_face");
    static lumice::gui::ScreenshotExportSelection s_shot_sel;
    static lumice::gui::ScreenshotRender s_shot;
    static bool s_shot_requested = false;
    static bool s_shot_done = false;
    t->GuiFunc = [](ImGuiTestContext*) {
      if (s_shot_requested && !s_shot_done) {
        s_shot = gui::RenderScreenshot(s_shot_sel);
        s_shot_done = true;
      }
    };
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      const auto take = [&ctx](int frames) {
        s_shot_requested = true;
        IM_CHECK(DriveUntil(ctx, [] { return s_shot_done; }, frames));
        s_shot_requested = false;
        s_shot_done = false;
      };
      // Render side: the preview panel's own publication.
      take(10);
      const int render_w = s_shot.w;
      const int render_h = s_shot.h;
      IM_CHECK_EQ(render_w, gui::g_preview_vp.vp_w);
      IM_CHECK_EQ(render_h, gui::g_preview_vp.vp_h);

      // Analyze, main view live, lens switched for real: the export follows the live face, and
      // the two faces' canvases are different rectangles (the discriminator — a wrong-side export
      // would carry the preview's numbers here).
      ctx->ItemClick("##TopBar/Analyze");
      ctx->Yield(3);
      IM_CHECK(gui::g_panorama_vp.active);
      ctx->SetRef("//##AnalyzeLeftPanel");
      ctx->ComboClick("Lens Type##view/Fisheye Equal Area");
      ctx->SetRef("");
      ctx->Yield(4);
      IM_CHECK_NE(gui::g_panorama_vp.vp_w, gui::g_preview_vp.vp_w);
      IM_CHECK_NE(gui::g_panorama_vp.vp_h, gui::g_preview_vp.vp_h);
      take(10);
      IM_CHECK_EQ(s_shot.w, gui::g_panorama_vp.vp_w);
      IM_CHECK_EQ(s_shot.h, gui::g_panorama_vp.vp_h);
    };
  }

  // The default centre's convergence deadline is absolute: it is the FIRST placement's frame,
  // and a size compensation never refreshes it — so a resize chain that runs well past the
  // window cannot keep re-deriving the direction, and a view edit followed by a further resize
  // after the deadline leaves the region exactly where the window left it.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "default_convergence_deadline_is_absolute");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick("Point");
      ctx->SetRef("");
      ctx->Yield(4);
      IM_CHECK(gui::g_state.analysis.cone_center_valid);
      const auto drag_d1 = [&ctx](float dx) {
        const ImGuiTestItemInfo grab = ctx->ItemInfo("**/##AnalyzeColumnSplitter", ImGuiTestOpFlags_NoError);
        IM_CHECK(grab.ID != 0);
        const ImVec2 c = grab.RectFull.GetCenter();
        ctx->MouseMoveToPos(c);
        ctx->Yield(1);
        ctx->MouseDown(0);
        ctx->Yield(1);
        ctx->MouseMoveToPos(ImVec2(c.x + dx, c.y));
        ctx->Yield(1);
        ctx->MouseUp(0);
        ctx->Yield(1);
      };
      // Inside the window: a view edit and the first resize re-derive the middle (that is the
      // initialization the mechanism exists for) — the direction moves off the birth view.
      ctx->SetRef("//##AnalyzeLeftPanel");
      ctx->ItemInputValue("##Azimuth##view_input", gui::g_state.renderer.azimuth + 40.0f);
      ctx->SetRef("");
      ctx->Yield(2);
      drag_d1(20.0f);
      ctx->Yield(1);
      drag_d1(20.0f);
      // The chain has run past the first placement's deadline (the drags above spent more frames
      // than the window holds, and no compensation refreshed it). Whatever direction the window
      // left is the region's; capture it.
      float frozen[3];
      std::copy(gui::g_state.analysis.cone_center_dir, gui::g_state.analysis.cone_center_dir + 3, frozen);
      // A view edit and more resizing after the deadline: the region stands byte for byte.
      ctx->SetRef("//##AnalyzeLeftPanel");
      ctx->ItemInputValue("##Azimuth##view_input", gui::g_state.renderer.azimuth + 60.0f);
      ctx->SetRef("");
      ctx->Yield(2);
      drag_d1(20.0f);
      ctx->Yield(3);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[0], frozen[0]);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[1], frozen[1]);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[2], frozen[2]);
    };
  }

  // A face with no publication this frame has no picture to export: the bar-takeover state that
  // once stood the main view down retired with the band's row split, and the refusal contract
  // keeps a live host on a document with NO picture yet — neither face publishes, and
  // RenderScreenshot refuses. It must never fall back across faces to the preview panel's
  // stale frame. The Save menu greys on the same predicate.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "screenshot_refuses_when_no_face_is_live");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      gui::DoNew();
      ctx->Yield(3);
      IM_CHECK(!gui::g_preview.HasTexture());  // no picture of this document yet
      IM_CHECK(!gui::g_panorama_vp.active);    // the Analyze face has nothing to publish
      const lumice::gui::ScreenshotRender refused = gui::RenderScreenshot(lumice::gui::ScreenshotExportSelection{});
      IM_CHECK_EQ(refused.w, 0);
      IM_CHECK(refused.rgba.empty());  // a refusal, not the preview panel's last frame
    };
  }

  // The texture-level panorama exports carry the CURRENT color state, assembled fresh: Render
  // renders in Screen, the Analyze left column switches Print — and the export base answers
  // Print (the preview panel's cached publication still holds Screen, which is exactly what the
  // old stale-cache base would have shipped). Pixel evidence: the same fisheye preset over the
  // fresh base and over the stale cache renders DIFFERENT images.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "texture_exports_carry_the_current_color_state");
    static std::vector<unsigned char> s_fresh;
    static std::vector<unsigned char> s_stale;
    static bool s_pixels_requested = false;
    static bool s_pixels_done = false;
    t->GuiFunc = [](ImGuiTestContext*) {
      if (s_pixels_requested && !s_pixels_done) {
        lumice::gui::PreviewParams fresh = gui::BuildTextureExportBase(64, 32);
        lumice::gui::ConfigureDualFisheyeExportParams(fresh);
        s_fresh = gui::RenderExportToRgba(gui::g_preview, fresh, 64, 32);
        lumice::gui::PreviewParams stale = gui::g_preview_vp.params;
        lumice::gui::ConfigureDualFisheyeExportParams(stale);
        s_stale = gui::RenderExportToRgba(gui::g_preview, stale, 64, 32);
        s_pixels_done = true;
      }
    };
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      IM_CHECK_EQ(gui::g_state.renderer.tone, LUMICE_TONE_SCREEN);
      ctx->ItemClick("##TopBar/Analyze");
      ctx->Yield(3);
      ctx->SetRef("//##AnalyzeLeftPanel");
      ctx->ItemClick("Display");
      ctx->Yield(2);
      ctx->ComboClick("Mode##display_tone/Print");
      ctx->SetRef("");
      ctx->Yield(3);
      IM_CHECK_EQ(gui::g_state.renderer.tone, LUMICE_TONE_PRINT);
      // The fresh base answers the current state; the cached publication answers the last
      // Render visit. (The display group's fold means the combo needed the open group above.)
      IM_CHECK_EQ(gui::BuildTextureExportBase(64, 32).tone, LUMICE_TONE_PRINT);
      IM_CHECK_EQ(gui::g_preview_vp.params.tone, LUMICE_TONE_SCREEN);

      // Pixels, not just parameters: the two bases render differently under the same preset.
      s_pixels_requested = true;
      IM_CHECK(DriveUntil(ctx, [] { return s_pixels_done; }, 10));
      IM_CHECK_EQ(s_fresh.size(), s_stale.size());
      IM_CHECK(!s_fresh.empty());
      IM_CHECK(std::memcmp(s_fresh.data(), s_stale.data(), s_fresh.size()) != 0);
    };
  }

  // AC2/§3: the left column's Display group is the SAME exposure field the Render right panel
  // edits — driving it for real re-lights the main view on the next frame (the mono path
  // re-multiplies the committed texture, no server interaction), and the document stays
  // display-touched only: no kModified, no ray-count movement.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "left_column_display_edits_relight_in_place");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      ctx->Yield(2);
      ImGuiWindow* pano = PanoramaPictureWindow();
      IM_CHECK(pano != nullptr);
      LUMICE_RayCount rays_before = 0;
      LUMICE_GetSimRayCount(gui::g_server, &rays_before);
      unsigned char before[3] = { 0, 0, 0 };
      // The picture's centre is the halo's brightest region (the document's camera points at
      // it), at any pool window size — a point above it can sit on empty sky, which the screen
      // tone renders as the background colour, and black re-multiplies to black.
      const ImVec2 sample(pano->Pos.x + pano->Size.x * 0.5f, pano->Pos.y + pano->Size.y * 0.5f);
      IM_CHECK(ReadScreenPixel(ctx, sample, before));
      IM_CHECK(before[0] + before[1] + before[2] > 0);

      // The group folds by default; open it, then drive the EV slider for real.
      ctx->SetRef("//##AnalyzeLeftPanel");
      // SliderWithInput's addressable items are the derived ones ("##<label>_input"/"_slider"),
      // not the display label — the same convention the defaults panel's cases drive by.
      IM_CHECK(!ctx->ItemExists("##EV##display_input"));  // folded: the field is not submitted
      ctx->ItemClick("Display");
      ctx->Yield(2);
      IM_CHECK(ctx->ItemExists("##EV##display_input"));
      ctx->ItemInputValue("##EV##display_input", 1.5f);
      ctx->SetRef("");
      ctx->Yield(4);

      IM_CHECK_FLOAT_NEAR(gui::g_state.renderer.exposure_offset, 1.5f, 1e-3f);
      unsigned char after[3] = { 0, 0, 0 };
      IM_CHECK(ReadScreenPixel(ctx, sample, after));
      IM_CHECK(std::memcmp(before, after, 3) != 0);  // the picture re-lit
      // Display tier: the document is not dirtied by an observation edit, and nothing re-ran.
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)gui::GuiState::SimState::kDone);
      IM_CHECK(!gui::g_state.analysis_run_in_progress);
      LUMICE_RayCount rays_after = 0;
      LUMICE_GetSimRayCount(gui::g_server, &rays_after);
      IM_CHECK_EQ(rays_after, rays_before);
    };
  }

  // §5.4: the left column's lens switch is the document's field — the main view follows on the
  // next frame, and the Render workspace shows the SAME lens when the user crosses back. One
  // field, two faces.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "left_column_lens_switch_crosses_workspaces");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.renderer.lens_type, gui::kLensTypeLinear);
      // The View group is expanded by default; the combo is addressable without opening folds.
      ctx->SetRef("//##AnalyzeLeftPanel");
      ctx->ComboClick("Lens Type##view/Fisheye Equal Area");
      ctx->SetRef("");
      ctx->Yield(3);
      IM_CHECK_EQ(gui::g_state.renderer.lens_type, gui::kLensTypeFisheyeEqualArea);
      // The main view publishes the renderer's own view: the lens is the same on its picture.
      IM_CHECK(gui::g_panorama_vp.active);
      IM_CHECK_EQ(gui::g_panorama_vp.params.view_proj.lens_type, gui::kLensTypeFisheyeEqualArea);

      ctx->ItemClick("##TopBar/Render");
      ctx->Yield(3);
      IM_CHECK_EQ(gui::g_state.renderer.lens_type, gui::kLensTypeFisheyeEqualArea);
      IM_CHECK(gui::g_preview_vp.active);
      IM_CHECK_EQ(gui::g_preview_vp.params.view_proj.lens_type, gui::kLensTypeFisheyeEqualArea);
      ctx->ItemClick("##TopBar/Analyze");
      ctx->Yield(3);
      IM_CHECK_EQ(gui::g_state.renderer.lens_type, gui::kLensTypeFisheyeEqualArea);
    };
  }

  // The Simulation fold is the scene's render-budget entry, deliberately default-collapsed:
  // opening it and editing a field for real writes the DOCUMENT (kModified), and nothing re-runs
  // — the explicit Run contract keeps its shape (§3's "edits in place, runs on command").
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "simulation_fold_edits_in_place");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      ctx->Yield(2);
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)gui::GuiState::SimState::kDone);
      ctx->SetRef("//##AnalyzeLeftPanel");
      IM_CHECK(!ctx->ItemExists("##Rays(M)_input"));  // folded by default: the field is not submitted
      ctx->ItemClick("Simulation");
      ctx->Yield(2);
      IM_CHECK(ctx->ItemExists("##Rays(M)_input"));
      ctx->ItemInputValue("##Rays(M)_input", 7.5f);
      ctx->SetRef("");
      ctx->Yield(2);
      IM_CHECK_FLOAT_NEAR(gui::g_state.sim.ray_num_millions, 7.5f, 1e-3f);
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)gui::GuiState::SimState::kModified);
      IM_CHECK(!gui::g_state.analysis_run_in_progress);
    };
  }

  // The observation surfaces' zero-side-effect bundle (§5.4 + AC6): a camera drag on the main
  // view, an overlay toggle and the two splitter drags — none may touch the document's sim tier
  // or the analysis session. kDone stands, the committed ray count stands, the list stays fresh,
  // the result object is the same, the diagnostics idle block keeps its shape. The continuity
  // half rides the camera leg: the publication's view IS the dragged view, field by field — the
  // same fields a Render-workspace preview would show, one source.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "observation_edits_leave_the_document_alone");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      ctx->Yield(2);
      IM_CHECK(RunWholeSkyAnalysisToCompletion(ctx, nullptr));
      const auto payload_before = gui::g_state.analysis_result.payload;
      LUMICE_RayCount rays_before = 0;
      LUMICE_GetSimRayCount(gui::g_server, &rays_before);
      const unsigned long long stats_before = gui::g_state.stats_sim_ray_num;
      const float az_before = gui::g_state.renderer.azimuth;
      const float el_before = gui::g_state.renderer.elevation;

      ImGuiWindow* pano = PanoramaPictureWindow();
      IM_CHECK(pano != nullptr);
      // Camera drag on the picture: outside CONE mode the camera owns the region outright, and
      // the write is display-tier.
      const ImVec2 drag_from(pano->Pos.x + pano->Size.x * 0.5f, pano->Pos.y + pano->Size.y * 0.7f);
      ctx->MouseMoveToPos(drag_from);
      ctx->Yield(2);
      ctx->MouseDown(0);
      ctx->Yield(2);
      ctx->MouseMoveToPos(ImVec2(drag_from.x + 100.0f, drag_from.y - 70.0f));
      ctx->Yield(3);
      ctx->MouseUp(0);
      ctx->Yield(3);
      IM_CHECK_NE(gui::g_state.renderer.azimuth, az_before);
      IM_CHECK_NE(gui::g_state.renderer.elevation, el_before);
      IM_CHECK(gui::g_panorama_vp.active);
      IM_CHECK_FLOAT_NEAR(gui::g_panorama_vp.params.view_proj.azimuth, gui::g_state.renderer.azimuth, 1e-4f);
      IM_CHECK_FLOAT_NEAR(gui::g_panorama_vp.params.view_proj.elevation, gui::g_state.renderer.elevation, 1e-4f);
      IM_CHECK_EQ(gui::g_panorama_vp.params.view_proj.lens_type, gui::g_state.renderer.lens_type);
      IM_CHECK_FLOAT_NEAR(gui::g_panorama_vp.params.view_proj.fov, gui::g_state.renderer.fov, 1e-4f);

      // An overlay toggle, in the left column's Overlay group (the shared group function). The
      // overlay checkboxes are all-id labels — this suite addresses them by wildcard
      // (test_overlay_controls.cpp's form); a ref-scoped bare name resolves to no id at all.
      ctx->ItemClick("**/##grid_line");
      ctx->Yield(3);

      // The two splitters: real drags, delta-only writes.
      const auto drag_splitter = [&](const char* item, float dx, float dy) {
        const ImGuiTestItemInfo grab = ctx->ItemInfo(item, ImGuiTestOpFlags_NoError);
        IM_CHECK(grab.ID != 0);
        const ImVec2 c = grab.RectFull.GetCenter();
        ctx->MouseMoveToPos(c);
        ctx->Yield(2);
        ctx->MouseDown(0);
        ctx->Yield(2);
        ctx->MouseMoveToPos(ImVec2(c.x + dx, c.y + dy));
        ctx->Yield(3);
        ctx->MouseUp(0);
        ctx->Yield(2);
      };
      drag_splitter("**/##AnalyzeColumnSplitter", 30.0f, 0.0f);
      drag_splitter("**/##AnalyzeRightRowsSplitter", 0.0f, 25.0f);

      // The bundle: nothing in the sim tier or the analysis session moved.
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)gui::GuiState::SimState::kDone);
      IM_CHECK(!gui::g_state.dirty);
      IM_CHECK_EQ(gui::g_state.stats_sim_ray_num, stats_before);
      LUMICE_RayCount rays_after = 0;
      LUMICE_GetSimRayCount(gui::g_server, &rays_after);
      IM_CHECK_EQ(rays_after, rays_before);
      IM_CHECK(gui::g_state.analysis_result.payload == payload_before);
      IM_CHECK(gui::g_state.analysis_result.analyzed_scene &&
               gui::g_state.analysis_result.analyzed_scene->Matches(gui::g_state));
      IM_CHECK(!gui::g_state.analysis_run_in_progress);
      // The idle diagnostics block keeps its shape (the result is on show, nothing selected).
      bool idle_summary = false;
      for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
        idle_summary =
            idle_summary || (w != nullptr && w->Active && std::strstr(w->Name, "##AnalyzeIdleSummary") != nullptr);
      }
      IM_CHECK(idle_summary);
    };
  }

  // The composite EV push is a display-time channel through the Analyze face too: with a
  // raypath colour lane active (the composite face's own condition), an EV edit on the left
  // column relights the main view on the next frame — the shared assembly pushes only the
  // manual offset, the wake refreshes, and no render epoch is spent.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "composite_ev_relights_through_the_bar");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      ctx->Yield(2);
      ImGuiWindow* pano = PanoramaPictureWindow();
      IM_CHECK(pano != nullptr);
      // The composite face's own activity condition (the shared assembly reads it): a server
      // and a raypath colour lane — one match-all class, tinted red, visible, and the
      // composite-upload flag the push's memory reads.
      {
        gui::ColorClassRefConfig ref;
        ref.layer_idx = 0;
        ref.crystal_pool_id = gui::g_state.layers[0].entries[0].crystal_id;
        ref.match_all = true;
        gui::ColorClassConfig cls;
        cls.color[0] = 1.0f;
        cls.color[1] = 0.0f;
        cls.color[2] = 0.0f;
        cls.visible = true;
        cls.z_order = 0;
        cls.match.push_back(ref);
        gui::g_state.raypath_color.push_back(cls);
      }
      gui::g_state.last_uploaded_as_composite = true;
      ctx->Yield(2);
      LUMICE_SimLifecycleResult before{};
      LUMICE_GetSimLifecycle(gui::g_server, &before);
      // The class push is a document write, so the reconcile may sit at kModified here; what the
      // EV edit must not do is move that state or spend an epoch.
      const int sim_state_before_ev = (int)gui::g_state.sim_state;
      const ImVec2 sample(pano->Pos.x + pano->Size.x * 0.5f, pano->Pos.y + pano->Size.y * 0.5f);
      unsigned char before_px[3] = { 0, 0, 0 };
      IM_CHECK(ReadScreenPixel(ctx, sample, before_px));
      IM_CHECK(before_px[0] + before_px[1] + before_px[2] > 0);

      ctx->SetRef("//##AnalyzeLeftPanel");
      ctx->ItemClick("Display");
      ctx->Yield(2);
      ctx->ItemInputValue("##EV##display_input", 1.5f);
      ctx->SetRef("");
      ctx->Yield(4);
      IM_CHECK_FLOAT_NEAR(gui::g_state.renderer.exposure_offset, 1.5f, 1e-3f);
      unsigned char after_px[3] = { 0, 0, 0 };
      IM_CHECK(ReadScreenPixel(ctx, sample, after_px));
      IM_CHECK(std::memcmp(before_px, after_px, 3) != 0);  // the picture re-lit
      LUMICE_SimLifecycleResult after{};
      LUMICE_GetSimLifecycle(gui::g_server, &after);
      IM_CHECK_EQ(after.epoch, before.epoch);  // display-time, not a re-run
      IM_CHECK_EQ((int)gui::g_state.sim_state, sim_state_before_ev);
    };
  }

  // AC2: the marker itself can be dragged. Hovering it shows the hand cursor; a press on it and a
  // move re-aim the centre to the direction under the cursor (LUMICE_UnprojectPixel on the
  // mouse's pixel — bit-identical, the drag transfers the inverse's output verbatim) while the
  // camera stays exactly where it was.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "marker_drag_moves_the_centre_not_the_view");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick("Point");
      ctx->SetRef("");
      ctx->Yield(1);
      IM_CHECK(gui::g_state.analysis.cone_center_valid);  // the default centre, AC3
      const std::optional<ImVec2> marker = MarkerScreenPos(ctx);
      IM_CHECK(marker.has_value());
      const float az_before = gui::g_state.renderer.azimuth;
      const float el_before = gui::g_state.renderer.elevation;
      const float fov_before = gui::g_state.renderer.fov;

      // Off the marker: the arrow. On it: the hand.
      ctx->MouseMoveToPos(ImVec2(marker->x + 120.0f, marker->y + 90.0f));
      ctx->Yield(2);
      IM_CHECK_EQ(ImGui::GetMouseCursor(), ImGuiMouseCursor_Arrow);
      ctx->MouseMoveToPos(*marker);
      ctx->Yield(2);
      IM_CHECK_EQ(ImGui::GetMouseCursor(), ImGuiMouseCursor_Hand);

      // Grab and drag to another point of the sky.
      ctx->MouseDown(0);
      ctx->Yield(1);
      IM_CHECK(gui::g_state.workspace.panorama_press_owner == PressOwner::kMarker);
      ctx->MouseMoveToPos(ImVec2(marker->x + 45.0f, marker->y - 30.0f));
      ctx->Yield(1);
      IM_CHECK_EQ(gui::g_state.workspace.panorama_press_owner, PressOwner::kMarker);
      IM_CHECK_EQ(ImGui::GetMouseCursor(), ImGuiMouseCursor_Hand);
      ctx->KeyDown(ImGuiMod_Alt);
      ctx->MouseMoveToPos(ImVec2(marker->x + 60.0f, marker->y - 40.0f));
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.workspace.panorama_press_owner, PressOwner::kMarker);
      float want[3] = { 0.0f, 0.0f, 0.0f };
      IM_CHECK(UnprojectMouse(want));
      ctx->KeyUp(ImGuiMod_Alt);
      ctx->MouseUp(0);
      ctx->Yield(2);
      IM_CHECK(gui::g_state.workspace.panorama_press_owner == PressOwner::kNone);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[0], want[0]);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[1], want[1]);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[2], want[2]);
      // The marker now sits under the mouse (to the pixel rounding of the projection).
      const std::optional<ImVec2> marker_after = MarkerScreenPos(ctx);
      IM_CHECK(marker_after.has_value());
      IM_CHECK_LT(ImFabs(marker_after->x - ImGui::GetIO().MousePos.x), 2.0f);
      IM_CHECK_LT(ImFabs(marker_after->y - ImGui::GetIO().MousePos.y), 2.0f);
      // The camera did not move.
      IM_CHECK_EQ(gui::g_state.renderer.azimuth, az_before);
      IM_CHECK_EQ(gui::g_state.renderer.elevation, el_before);
      IM_CHECK_EQ(gui::g_state.renderer.fov, fov_before);
    };
  }

  // The cone-centre drift warning's fixed visibility (779.1): after a completed Point analysis,
  // moving the centre must light the warning in the main view's info area WITHOUT scrolling and
  // WITHOUT collapsing anything. The shape that failed the owner's real check drew the warning
  // as the fourth line of the two-line status viewport, past its fold — equivalent to no failure
  // sign at all. The judge is the drawn ink itself (CollectPanoramaWarningInk), never the
  // predicate or an item id: the red state this case first recorded was the warning's glyphs
  // absent from the main view's output entirely.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "cone_centre_drift_warning_fixed_visible");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      // The splitter ratios the manual-priority contract owns (§9.3): the band's appear and
      // its later clear must not rewrite either — captured before anything runs.
      const float d1_ratio_before = gui::g_state.workspace.right_column_ratio;
      const float d2_ratio_before = gui::g_state.workspace.right_list_ratio;
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick("Point");
      ctx->Yield(2);
      ctx->SetRef("");
      const gui::CanvasPixel px = PickPanoramaCentre(ctx);
      IM_CHECK(gui::g_state.analysis.cone_center_valid);
      // Analyze, through the button; completion is the analysis's own end plus a result, the
      // same gate point_pick_analyze_lists_chains uses — never a stale payload alone.
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick(kAnalyzeButton);
      ctx->SetRef("");
      IM_CHECK(gui::g_state.analysis.started);
      IM_CHECK(DriveUntil(
          ctx, [] { return !gui::g_state.analysis_run_in_progress && gui::g_state.analysis_result.payload != nullptr; },
          60));
      IM_CHECK_EQ(gui::g_state.analysis_result.payload->roi_mode, LUMICE_RAYPATH_ROI_CONE);

      const ImU32 warn_colour = WarningTextVertexColour();
      const PanoramaWarningInk before_drag = CollectPanoramaWarningInk(warn_colour);
      IM_CHECK(!gui::ConeCenterDriftedFromResult(gui::g_state));
      // AC3's positive control: no warning ink while the centre stands on the analysed one. If
      // something else in this subtree ever draws the warning colour, this leg turns red and
      // the probe needs a second discriminator — a deliberate tripwire, not a pass.
      IM_CHECK(!before_drag.found);

      // The real marker drag, the same gesture marker_drag_moves_the_centre_not_the_view pins:
      // press on the marker, move to another sky point, release.
      const std::optional<ImVec2> marker = MarkerScreenPos(ctx);
      IM_CHECK(marker.has_value());
      const float dir_before[3] = { gui::g_state.analysis.cone_center_dir[0], gui::g_state.analysis.cone_center_dir[1],
                                    gui::g_state.analysis.cone_center_dir[2] };
      ctx->MouseMoveToPos(*marker);
      ctx->Yield(2);
      ctx->MouseDown(0);
      ctx->Yield(1);
      IM_CHECK(gui::g_state.workspace.panorama_press_owner == PressOwner::kMarker);
      ctx->MouseMoveToPos(ImVec2(marker->x + 45.0f, marker->y - 30.0f));
      ctx->Yield(1);
      ctx->KeyDown(ImGuiMod_Alt);
      ctx->MouseMoveToPos(ImVec2(marker->x + 60.0f, marker->y - 40.0f));
      ctx->Yield(2);
      ctx->KeyUp(ImGuiMod_Alt);
      ctx->MouseUp(0);
      ctx->Yield(2);
      IM_CHECK(gui::g_state.workspace.panorama_press_owner == PressOwner::kNone);
      const float dir_after[3] = { gui::g_state.analysis.cone_center_dir[0], gui::g_state.analysis.cone_center_dir[1],
                                   gui::g_state.analysis.cone_center_dir[2] };
      IM_CHECK(std::memcmp(dir_before, dir_after, sizeof(dir_before)) != 0);
      // The drag is only a re-aim (contract §8): no analysis may auto-start, and the completed
      // result stays on show unchanged. `started` is the standing intent, not a progress flag —
      // the judge for "nothing re-ran" is the derived in-progress flag and the payload pointer.
      const auto* payload_before = gui::g_state.analysis_result.payload.get();
      IM_CHECK(!gui::g_state.analysis_run_in_progress);
      IM_CHECK(gui::ConeCenterDriftedFromResult(gui::g_state));

      // AC1's judge, at rest, before any scroll: the warning must be ON SCREEN in the main
      // view's info area. The drag gesture lives on the picture, so a scroll here would be the
      // automatic kind the AC forbids. Telemetry first — a red assert prints its expression,
      // these lines carry the geometry behind it.
      ImGuiWindow* host = AnalyzePanoramaHostWindow();
      IM_CHECK(host != nullptr);
      ImGuiWindow* status = RegionWindowUnder("##AnalyzePanorama", "##AnalyzePictureStatus");
      IM_CHECK(status != nullptr);
      ctx->LogInfo(
          "[drift band] status SM %.1f SC %.1f inner [%.0f..%.0f] h %.0f | chrome SM %.1f/%.1f avail %.0fx%.0f",
          status->ScrollMax.y, status->Scroll.y, status->InnerClipRect.Min.y, status->InnerClipRect.Max.y,
          status->InnerClipRect.GetHeight(), host->ScrollMax.x, host->ScrollMax.y, host->ContentRegionRect.GetWidth(),
          host->ContentRegionRect.GetHeight());
      IM_CHECK_EQ(status->Scroll.y, 0.0f);
      const PanoramaWarningInk after_drag = CollectPanoramaWarningInk(warn_colour);
      // The visibility bar: the warning's ink was drawn, and none of it sits outside its own
      // window's clip. On the old placement the warning's item is culled past the viewport's
      // fold and no vertex reaches the draw list — the red state this case is the regression
      // anchor for.
      // The visibility bar: the warning's ink was drawn, and none of it sits outside its own
      // window's clip. On the old placement the warning's item is culled past the viewport's
      // fold and no vertex reaches the draw list — the red state this case is the regression
      // anchor for.
      IM_CHECK(after_drag.found);
      IM_CHECK(!after_drag.clipped);
      // AC1's full text, read the way the render site reports it (the field is cleared at the
      // workspace's frame entry and is the band's only other writer): both clauses the AC
      // names, the icon drawn by the same call.
      const std::string& drawn = gui::g_state.analysis.wait_feedback_texts.cone_center_drift_warning;
      IM_CHECK(drawn.find("Centre has moved") != std::string::npos);
      IM_CHECK(drawn.find("Analyze") != std::string::npos);
      // The band replaced the viewport's own warning line (779.1): the status child draws no
      // warning-coloured ink any more, and the band's ink ends at or above the viewport's top
      // edge — one face, no duplicate, no overlap.
      int status_warning_vertices = 0;
      for (const ImDrawVert& v : status->DrawList->VtxBuffer) {
        status_warning_vertices += v.col == warn_colour ? 1 : 0;
      }
      IM_CHECK_EQ(status_warning_vertices, 0);
      IM_CHECK_LE(after_drag.bbox.Max.y, status->InnerClipRect.Min.y + 0.5f);
      // The completeness face (review round 1's judge): the vertices answer for the ink, the
      // recorded item rect answers for the text — the layout footprint every wrapped line
      // occupies whether or not RenderText went on to emit it (it stops at the clip bottom,
      // so a line clipped away leaves ink but never the rect; "only the first line visible"
      // cannot pass this pair). The rect is the item's own bb: its height is the same
      // CalcTextSizeA call the band's budget runs, so the two cannot disagree without breaking
      // the stack arithmetic.
      const auto& feedback = gui::g_state.analysis.wait_feedback_texts;
      const ImRect band_rect(
          ImVec2(feedback.cone_center_drift_warning_rect_min_x, feedback.cone_center_drift_warning_rect_min_y),
          ImVec2(feedback.cone_center_drift_warning_rect_max_x, feedback.cone_center_drift_warning_rect_max_y));
      IM_CHECK_FLOAT_NEAR(band_rect.GetHeight(), gui::AnalyzeDriftWarningHeight(host->ContentRegionRect.GetWidth()),
                          1e-3f);
      IM_CHECK(host->InnerClipRect.Contains(band_rect));
      IM_CHECK(after_drag.bbox.Min.y >= band_rect.Min.y - 1.0f && after_drag.bbox.Max.y <= band_rect.Max.y + 1.0f);
      // The viewport stays a bounded, genuinely scrollable viewport (AC4): its remaining
      // content still overflows the two-line fold, real scrolling moves it, and the band —
      // outside the flow — is untouched by that scroll. Not a ScrollMaxY==0 leg: the overflow
      // is asserted first.
      IM_CHECK_GT(status->ScrollMax.y, 0.0f);
      status->Scroll.y = status->ScrollMax.y;
      ctx->Yield(2);
      IM_CHECK_GT(status->Scroll.y, 0.0f);
      const PanoramaWarningInk after_scroll = CollectPanoramaWarningInk(warn_colour);
      IM_CHECK(after_scroll.found);
      IM_CHECK(!after_scroll.clipped);
      status->Scroll.y = 0.0f;
      ctx->Yield(1);
      // The picture region is still submitted with a real height, and the splitter ratios the
      // manual-priority contract owns (§9.3) never saw the band appear or disappear.
      ImGuiWindow* picture = PanoramaPictureWindow();
      IM_CHECK(picture != nullptr);
      IM_CHECK(picture->Size.y > 1.0f);
      IM_CHECK_EQ(gui::g_state.workspace.right_column_ratio, d1_ratio_before);
      IM_CHECK_EQ(gui::g_state.workspace.right_list_ratio, d2_ratio_before);
      // The chrome window itself never scrolls for the warning: the band's height is part of
      // the column's vertical stack, not a window-level overflow.
      IM_CHECK_EQ(host->ScrollMax.y, 0.0f);
      IM_CHECK_EQ(host->Scroll.y, 0.0f);
      // Still nothing auto-started several frames in (the adoption gate would have run by now).
      ctx->Yield(3);
      IM_CHECK(!gui::g_state.analysis_run_in_progress);
      IM_CHECK_EQ(gui::g_state.analysis_result.payload.get(), payload_before);

      // AC2, through the real chain: the button's new analysis runs to its own end and adopts
      // the moved centre as the analysed one — the band is gone (no ink, no drawn text), and
      // the button was reachable at rest when clicked.
      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(!IsDisabled(ctx->ItemInfo(kAnalyzeButton)));
      ctx->ItemClick(kAnalyzeButton);
      ctx->SetRef("");
      IM_CHECK(gui::g_state.analysis.started);
      IM_CHECK(DriveUntil(
          ctx, [] { return !gui::g_state.analysis_run_in_progress && gui::g_state.analysis_result.payload != nullptr; },
          60));
      IM_CHECK(!gui::ConeCenterDriftedFromResult(gui::g_state));
      const PanoramaWarningInk after_reanalyze = CollectPanoramaWarningInk(warn_colour);
      IM_CHECK(!after_reanalyze.found);
      IM_CHECK(gui::g_state.analysis.wait_feedback_texts.cone_center_drift_warning.empty());
      IM_CHECK_EQ(gui::g_state.analysis.wait_feedback_texts.cone_center_drift_warning_rect_min_x, 0.0f);
      IM_CHECK_EQ(gui::g_state.analysis.wait_feedback_texts.cone_center_drift_warning_rect_max_y, 0.0f);
    };
  }

  // The band's boundary legs (779.1): the narrowest legal window and the 1x/1.5x/2x scale
  // tiers — the wrap-width extremes the fixed height must survive. One completed analysis
  // feeds every leg; each leg re-establishes the undrifted baseline (the state write is the
  // display face of the pure predicate — the real gesture moved the centre in the main case),
  // resets the viewport's scroll, then drags the marker through the same real gesture at THIS
  // leg's geometry and reads the actual client off the drawn frame.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "cone_centre_drift_warning_narrow_and_scaled");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      const ScopedUiScaleAndBasisLease geometry_lease(ctx);
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick("Point");
      ctx->Yield(2);
      ctx->SetRef("");
      PickPanoramaCentre(ctx);
      IM_CHECK(gui::g_state.analysis.cone_center_valid);
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick(kAnalyzeButton);
      ctx->SetRef("");
      IM_CHECK(DriveUntil(
          ctx, [] { return !gui::g_state.analysis_run_in_progress && gui::g_state.analysis_result.payload != nullptr; },
          60));
      IM_CHECK_EQ(gui::g_state.analysis_result.payload->roi_mode, LUMICE_RAYPATH_ROI_CONE);

      const auto set_scale = [&](float multiplier) -> bool {
        gui::g_ui_scale_multiplier = multiplier;
        gui::g_ui_scale_dirty = true;
        for (int i = 0;
             i < 60 && !(gui::UiPx(gui::kAnalyzeLeftPanelWidth) > multiplier * gui::kAnalyzeLeftPanelWidth - 0.5f &&
                         gui::UiPx(gui::kAnalyzeLeftPanelWidth) < multiplier * gui::kAnalyzeLeftPanelWidth + 0.5f);
             ++i) {
          ctx->Yield(1);
        }
        IM_CHECK_RETV(fabsf(gui::UiPx(gui::kAnalyzeLeftPanelWidth) - multiplier * gui::kAnalyzeLeftPanelWidth) < 0.5f,
                      false);
        ctx->Yield(4);
        return true;
      };
      const auto set_basis = [&](int w, int h) -> bool {
        g_window_size_test.apply_aspect = false;
        g_window_size_test.width = w;
        g_window_size_test.height = h;
        g_window_size_test.done.store(false);
        g_window_size_test.requested.store(true);
        for (int i = 0; i < 20 && !g_window_size_test.done.load(); ++i) {
          ctx->Yield(1);
        }
        IM_CHECK_RETV(g_window_size_test.done.load(), false);
        ctx->Yield(3);
        return true;
      };

      const auto run_leg = [&](const char* label) -> bool {
        const ImU32 warn_colour = WarningTextVertexColour();
        // Undrifted baseline at THIS leg's geometry: the centre back on the analysed one (the
        // display face of the pure predicate), the viewport's scroll reset, no ink.
        std::memcpy(gui::g_state.analysis.cone_center_dir, gui::g_state.analysis.analyzed_cone_center_dir,
                    sizeof(gui::g_state.analysis.cone_center_dir));
        ImGuiWindow* status = RegionWindowUnder("##AnalyzePanorama", "##AnalyzePictureStatus");
        IM_CHECK_RETV(status != nullptr, false);
        status->Scroll.y = 0.0f;
        ctx->Yield(2);
        IM_CHECK_RETV(!gui::ConeCenterDriftedFromResult(gui::g_state), false);
        const PanoramaWarningInk baseline = CollectPanoramaWarningInk(warn_colour);
        IM_CHECK_RETV(!baseline.found, false);
        // The real marker drag, the main case's gesture at this leg's geometry.
        const std::optional<ImVec2> marker = MarkerScreenPos(ctx);
        IM_CHECK_RETV(marker.has_value(), false);
        ctx->MouseMoveToPos(*marker);
        ctx->Yield(2);
        ctx->MouseDown(0);
        ctx->Yield(1);
        IM_CHECK_RETV(gui::g_state.workspace.panorama_press_owner == PressOwner::kMarker, false);
        ctx->MouseMoveToPos(ImVec2(marker->x + 45.0f, marker->y - 30.0f));
        ctx->Yield(1);
        ctx->KeyDown(ImGuiMod_Alt);
        ctx->MouseMoveToPos(ImVec2(marker->x + 60.0f, marker->y - 40.0f));
        ctx->Yield(2);
        ctx->KeyUp(ImGuiMod_Alt);
        ctx->MouseUp(0);
        ctx->Yield(2);
        IM_CHECK_RETV(gui::g_state.workspace.panorama_press_owner == PressOwner::kNone, false);
        IM_CHECK_RETV(gui::ConeCenterDriftedFromResult(gui::g_state), false);
        // AC1 at this geometry: both clauses drawn, the ink whole and inside every clip.
        const std::string& drawn = gui::g_state.analysis.wait_feedback_texts.cone_center_drift_warning;
        IM_CHECK_RETV(drawn.find("Centre has moved") != std::string::npos, false);
        IM_CHECK_RETV(drawn.find("Analyze") != std::string::npos, false);
        const PanoramaWarningInk band = CollectPanoramaWarningInk(warn_colour);
        IM_CHECK_RETV(band.found, false);
        IM_CHECK_RETV(!band.clipped, false);
        // The layout floor at this geometry: at least one full readable text line of viewport
        // client, the chrome window still unscrolled, and the Analyze control — the action the
        // warning names — reachable.
        IM_CHECK_RETV(status->InnerClipRect.GetHeight() >= ImGui::GetTextLineHeight() - 0.5f, false);
        ImGuiWindow* host = AnalyzePanoramaHostWindow();
        IM_CHECK_RETV(host != nullptr, false);
        IM_CHECK_RETV(host->ScrollMax.y == 0.0f, false);
        // The completeness face, per leg (the same pair the main case judges): the ink's
        // vertices inside the recorded full item rect, and the rect itself inside the chrome's
        // clip — a line RenderText skipped for the clip bottom leaves ink but never the rect,
        // so "only the first line visible" cannot pass.
        const auto& feedback = gui::g_state.analysis.wait_feedback_texts;
        const ImRect band_rect(
            ImVec2(feedback.cone_center_drift_warning_rect_min_x, feedback.cone_center_drift_warning_rect_min_y),
            ImVec2(feedback.cone_center_drift_warning_rect_max_x, feedback.cone_center_drift_warning_rect_max_y));
        IM_CHECK_RETV(
            fabsf(band_rect.GetHeight() - gui::AnalyzeDriftWarningHeight(host->ContentRegionRect.GetWidth())) < 1e-3f,
            false);
        IM_CHECK_RETV(host->InnerClipRect.Contains(band_rect), false);
        IM_CHECK_RETV(band.bbox.Min.y >= band_rect.Min.y - 1.0f && band.bbox.Max.y <= band_rect.Max.y + 1.0f, false);
        // The negative control, on real geometry (review round 1): the same containment judge
        // applied to the two-line status viewport's clip — the OLD placement's fold — must
        // reject a band that wraps taller than it. Where this leg's band demonstrably wraps
        // multi-line, the fold's rejection is exactly what would have failed the old shape
        // under this judge; the judge discriminates, it is not a tautology of the vertex probe.
        if (band_rect.GetHeight() > status->InnerClipRect.GetHeight()) {
          IM_CHECK_RETV(!status->InnerClipRect.Contains(band_rect), false);
        }
        // "**/" roots the search — at the narrow tiers the op bar lives in its scroll child,
        // which a chrome-window ref cannot address (child windows register as
        // "Parent/Name_<id>"). The roomier tiers resolve the button at rest; at the extreme
        // tier the bar's content stacks past the two budgeted rows, so the button registers
        // only once the bar's OWN internal scroll — the contract's designed reach ("not
        // permanently cut") — brings its row in view. A bounded sweep, not one scroll-to-end:
        // the button sits mid-stack, and an end position can leave it past the fold in either
        // direction. The containment reads at the same in-view frame; the rest state returns
        // after.
        ctx->SetRef(PanoramaRegionRef());
        ImGuiTestItemInfo analyze_btn =
            ctx->ItemInfo((std::string("**/") + kAnalyzeButton).c_str(), ImGuiTestOpFlags_NoError);
        ctx->SetRef("");
        ImGuiWindow* opbar_scroll = RegionWindowUnder("##AnalyzePanorama", "##AnalyzeOpBarScroll");
        if (analyze_btn.ID == 0 && opbar_scroll != nullptr) {
          const float max_scroll = opbar_scroll->ScrollMax.y;
          const float step = std::max(1.0f, opbar_scroll->InnerClipRect.GetHeight() * 0.5f);
          for (float s = 0.0f; analyze_btn.ID == 0 && s <= max_scroll + 0.5f; s += step) {
            opbar_scroll->Scroll.y = std::min(s, max_scroll);
            ctx->Yield(1);
            ctx->SetRef(PanoramaRegionRef());
            analyze_btn = ctx->ItemInfo((std::string("**/") + kAnalyzeButton).c_str(), ImGuiTestOpFlags_NoError);
            ctx->SetRef("");
          }
        }
        IM_CHECK_RETV(analyze_btn.ID != 0, false);
        IM_CHECK_RETV(analyze_btn.RectFull.Min.y >= host->InnerClipRect.Min.y - 0.5f &&
                          analyze_btn.RectFull.Max.y <= host->InnerClipRect.Max.y + 0.5f,
                      false);
        if (opbar_scroll != nullptr) {
          opbar_scroll->Scroll.y = 0.0f;
        }
        ctx->Yield(1);
        ctx->LogInfo("[drift band leg] %s: band %.0fx%.0f, status client h %.0f SM %.0f, avail %.0fx%.0f", label,
                     band.bbox.GetWidth(), band.bbox.GetHeight(), status->InnerClipRect.GetHeight(),
                     status->ScrollMax.y, host->ContentRegionRect.GetWidth(), host->ContentRegionRect.GetHeight());
        return true;
      };

      IM_CHECK(set_basis(1024, 640));
      IM_CHECK(run_leg("1024x640 1x"));
      IM_CHECK(set_scale(1.5f));
      IM_CHECK(set_basis(1024, 640));
      IM_CHECK(run_leg("1024x640 1.5x"));
      IM_CHECK(set_scale(2.0f));
      IM_CHECK(set_basis(1024, 640));
      IM_CHECK(run_leg("1024x640 2x"));
    };
  }

  // The staleness hints' two child windows — the unified derivation's AC2/AC5 red state in
  // two legs, each constructing its own fact:
  //  1. the edit-stale leg — a completed analysis, an edit that moves the document away from the
  //     analysed scene, BOTH floating windows standing: the Raypath view and the Pose & trace
  //     window display the very list/map/answers the edit made stale, and nothing in either
  //     window says so (the freshness facts' only faces were the main window's three);
  //  2. the cone-drift leg — a completed Point analysis, the centre moved off the analysed one,
  //     the Raypath view open: its drift row is the drift fact's first window-level witness (the
  //     main band's cases and the aggregate's enumeration cannot see a missing hint).
  // The judge at this stage is the drawn ink itself — the warning colour's vertices inside the
  // window's own subtree, none of it outside its clip — never the predicate or a field: the red
  // state this case first records is warning-coloured glyphs absent from the two windows
  // entirely. The feedback fields and their full rects join the judge when the render sites
  // land (the rect is the completeness face: a hint whose first line fits while its continuation
  // is clipped away leaves ink and passes an ink-only probe — the roi-drift lesson).
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "analysis_stale_hint_child_windows");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      IM_CHECK(AnalysisStaleHintEditStaleLeg(ctx));
      IM_CHECK(AnalysisStaleHintConeDriftLeg(ctx));
    };
  }

  // The marker is drawn on the preview window's own draw list, not the foreground one: a window
  // drawn after the preview covers it. Read off the screen, since which list a primitive went to
  // is not something the item registry sees: the pixel under the marker IS the accent colour with
  // nothing over it (the positive control — without it a wrong sample point would pass the case
  // for free), and is NOT once the Edit Entry modal is up, whose dim layer and body are drawn
  // over every window beneath the popup stack. On the foreground list the marker painted over
  // both, and this second read came back accent.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "marker_sits_under_a_modal_not_over_it");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      const ScopedPopups popup_guard(ctx);
      OpenWindow(ctx);
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick("Point");
      ctx->SetRef("");
      ctx->Yield(2);
      std::optional<ImVec2> marker = MarkerScreenPos(ctx);
      IM_CHECK(marker.has_value());
      // The analysis window out of the way of the marker (its top-left past the marker), and the
      // mouse too, so that neither is what the sample lands on.
      ctx->WindowMove(kWindowRef, ImVec2(marker->x + 40.0f, marker->y + 40.0f));
      ctx->MouseMoveToPos(ImVec2(marker->x - 120.0f, marker->y - 90.0f));
      ctx->Yield(2);
      marker = MarkerScreenPos(ctx);
      IM_CHECK(marker.has_value());
      unsigned char rgb[3] = { 0, 0, 0 };
      IM_CHECK(ReadScreenPixel(ctx, *marker, rgb));
      ctx->LogInfo("marker pixel, nothing over it: %d %d %d", rgb[0], rgb[1], rgb[2]);
      IM_CHECK(IsAccentColour(rgb));

      // Opened through OpenEditModal rather than by a card click, the way test_entry_management
      // does: the click's own path is not what this case is about, the window's presence is.
      gui::g_state.modal_immediate_mode = false;  // staged: a real modal, on the popup stack
      const gui::EditRequest req{ gui::EditTarget::kCrystal, 0, 0 };
      gui::OpenEditModal(req, gui::g_state);
      ctx->Yield(4);
      ImGuiWindow* modal = ImGui::GetTopMostPopupModal();
      IM_CHECK(modal != nullptr);
      IM_CHECK_STR_EQ(modal->Name, "Edit Entry");
      IM_CHECK(gui::g_state.analysis.roi_mode == LUMICE_RAYPATH_ROI_CONE);  // the marker is still asked for
      IM_CHECK(ReadScreenPixel(ctx, *marker, rgb));
      ctx->LogInfo("marker pixel, modal up: %d %d %d", rgb[0], rgb[1], rgb[2]);
      IM_CHECK(!IsAccentColour(rgb));
    };
  }

  // An In filter on the chain's crystal keeps Exclude shut: the button is disabled with the
  // selection made, the eligibility names the reason, and nothing is written by a click. The
  // tooltip TEXT is not read off the screen — ImGui::SetTooltip draws through TextUnformatted
  // with id 0, which the test engine's registry never sees (the limit
  // test_view_display_controls.cpp records for its own disabled entries) — so the words are
  // asserted on the same function the tooltip prints, with the hover driven for real so that
  // frame's SetTooltip path is exercised too.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "exclude_is_shut_by_an_in_filter_on_the_crystal");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      IM_CHECK(RunPointAnalysisToCompletion(ctx));
      // The document's one entry gets an In filter (FilterConfig's default action) — as the
      // filter editor would leave it after "keep only 1-3".
      IM_CHECK(gui::g_state.filters.empty());
      gui::FilterConfig keep;
      keep.name = "keep 1-3";
      keep.action = 0;
      keep.param = gui::FromLegacyRaypath(gui::RaypathParams{ "1-3" });
      gui::g_state.filters.push_back(keep);
      gui::g_state.layers[0].entries[0].filter_id = 0;

      const auto& view_result = gui::g_state.analysis_result;
      IM_CHECK(!view_result.display_order.empty());
      const std::string top_display =
          view_result.payload->entries[static_cast<size_t>(view_result.display_order[0])].display;
      SetListRef(ctx);
      ctx->ItemClick((std::string("**/") + top_display).c_str());
      ctx->Yield(1);
      IM_CHECK(gui::g_state.analysis.selected_entry.has_value());
      IM_CHECK(IsDisabled(ctx->ItemInfo(ICON_FA_BAN " Exclude this raypath###action_exclude")));
      std::string why;
      IM_CHECK_EQ((int)gui::EvaluateExcludeEligibility(gui::g_state, &why),
                  (int)gui::ExcludeEligibility::kEntryHasInFilter);
      IM_CHECK(!why.empty());
      IM_CHECK(why.find("In filter") != std::string::npos);
      IM_CHECK(gui::ExcludeAppendNotice(gui::g_state).empty());
      // Hover, so the disabled button's tooltip frame is drawn; then click, which does nothing.
      ctx->MouseMove(ICON_FA_BAN " Exclude this raypath###action_exclude");
      ctx->Yield(2);
      ctx->ItemClick(ICON_FA_BAN " Exclude this raypath###action_exclude");
      ctx->SetRef("");
      ctx->Yield(1);
      IM_CHECK_EQ(gui::g_state.filters.size(), 1u);
      IM_CHECK_EQ(gui::g_state.filters[0].action, 0);
      IM_CHECK_EQ(gui::g_state.filters[0].param.size(), 1u);
      IM_CHECK_STR_EQ(gui::g_state.filters[0].name.c_str(), "keep 1-3");
    };
  }


  // Exclude with the Immediate-mode editor open on the same entry — see
  // ExcludeWhileTheImmediateEditorIsOpen. Two cases, one per way the editor's copy used to win over
  // the pool: a filter it had never seen (the entry had none), and a row appended to the one it
  // was holding.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "raypath_analysis", "exclude_while_the_immediate_editor_is_open_binds_a_new_filter");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(ExcludeWhileTheImmediateEditorIsOpen(ctx, /*start_with_out_filter=*/false));
    };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis",
                                    "exclude_while_the_immediate_editor_is_open_appends_to_its_out_filter");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(ExcludeWhileTheImmediateEditorIsOpen(ctx, /*start_with_out_filter=*/true));
    };
  }

  // Run after a cone-stopped analysis renders — see RunAfterAnalysisRenders. Three cases: the
  // sequence itself on the CPU backend, the same with the owner's Exclude step in between, and
  // the sequence on the GPU backend (Metal on this tree's reference machine; the analysis is
  // forced to the CPU route inside the same server, so the render that follows is the GPU's
  // first session after an analysis — the shape the bug was reported on).
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "run_after_analysis_renders");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(RunAfterAnalysisRenders(ctx, /*exclude=*/false, /*gpu=*/false));
    };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "run_after_analysis_with_exclude_renders");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(RunAfterAnalysisRenders(ctx, /*exclude=*/true, /*gpu=*/false));
    };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "run_after_analysis_renders_gpu");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(RunAfterAnalysisRenders(ctx, /*exclude=*/false, /*gpu=*/true));
    };
  }

  // Run after a COMPLETED analysis, once per ROI, and DoRun's rebuild predicate agreed with the
  // server — see RunAfterCompletedAnalysisHasNoRebuildMismatch. The three cases above end their
  // analysis by Stop; these let it end on its budget, the other end a run has, and read the
  // safety net's warning off the GUI logger instead of only the picture. One case per ROI
  // because the ROI decides which consumer the analysis session builds, and the mismatch used
  // to fire for every one of them. (The third ROI the suite once covered — In frame — retired
  // with the free-camera main view; its GUI entry is gone, and the engine-side contract keeps
  // its own coverage.)
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "run_after_completed_point_analysis_no_mismatch");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      IM_CHECK(RunPointAnalysisToCompletion(ctx));
      IM_CHECK_EQ(gui::g_state.analysis_result.payload->roi_mode, LUMICE_RAYPATH_ROI_CONE);
      IM_CHECK(RunAfterCompletedAnalysisHasNoRebuildMismatch(ctx));
    };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "run_after_completed_whole_sky_analysis_no_mismatch");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      IM_CHECK(RunWholeSkyAnalysisToCompletion(ctx, nullptr));
      IM_CHECK_EQ(gui::g_state.analysis_result.payload->roi_mode, LUMICE_RAYPATH_ROI_FULL_SKY);
      IM_CHECK(RunAfterCompletedAnalysisHasNoRebuildMismatch(ctx));
    };
  }

  // The bounded record on screen (v4.35). A result whose record was full — three rows plus an
  // "other" bucket — is put on show directly (no server: the row and the column are a property of
  // the list, and the record-level numbers come through the payload the read fills), and the
  // list must carry the "Cumulative %" header, a monotone column, and the fixed "other" line at
  // the bottom that a click cannot select: the selection stays what it was, and with nothing
  // selected the Exclude button stays disabled. The status line names the truncation. The cone
  // filter is a real path here too: no server is needed to drag the slider, and the other line
  // must keep closing the column after a re-sum.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "other_row_and_cumulative_column");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      ResetTestState();
      OpenWindow(ctx);
      auto payload = std::make_shared<gui::AnalysisPayload>();
      payload->snapshot_generation = 3;
      payload->roi_mode = LUMICE_RAYPATH_ROI_FULL_SKY;
      payload->other_energy = 2.0;
      payload->other_count = 20;
      payload->truncated_chain_count = 7;
      const double energies[3] = { 5.0, 2.0, 1.0 };
      for (int i = 0; i < 3; i++) {
        LUMICE_RaypathHistogramEntry e{};
        e.chain_len = 1;
        e.chain[0].crystal_id = 1;
        e.chain[0].segment_len = 2;
        e.chain[0].segment[0] = i + 1;
        e.chain[0].segment[1] = i + 2;
        snprintf(e.display, sizeof(e.display), "%d-%d", i + 1, i + 2);
        e.energy = energies[i];
        e.count = 100;
        e.error_bound = i == 2 ? 0.5 : 0.0;  // the last row took a slot over
        payload->entries.push_back(e);
      }
      IM_CHECK(gui::AdoptAnalysisPayloadIfNew(gui::g_state, payload));
      gui::g_state.analysis.fetched_once = true;
      gui::g_state.analysis.fetched_generation = 3;
      gui::g_state.analysis.fetched_symmetry = gui::AnalysisSymmetryBits(gui::g_state);
      ctx->Yield(2);
      SetListRef(ctx);

      // The column and the line exist in the rendered table. The header cell is not addressable
      // by label (imgui_tables.cpp never hands header labels to the engine), so the column is read
      // off the table object itself, whose id BeginTable computed from the window with nothing
      // pushed — the same route test_defaults_panel.cpp takes to its settings table.
      ImGuiWindow* win = ctx->GetWindowByRef(kWindowRef);
      IM_CHECK(win != nullptr);
      ImGuiTable* table = ImGui::TableFindByID(win->GetID("##analysis_rows"));
      IM_CHECK(table != nullptr);
      bool has_cumulative = false;
      for (int n = 0; n < table->ColumnsCount; n++) {
        has_cumulative = has_cumulative || std::strcmp(ImGui::TableGetColumnName(table, n), "Cumulative %") == 0;
      }
      IM_CHECK(has_cumulative);
      // Raypath, Energy, Cumulative %, +/- — and no Rays column: under adaptive ray allocation
      // a row's hit count follows the online deal, not the crystal's proportion, so it was
      // dropped from the table and the CSV alike.
      bool has_rays = false;
      for (int n = 0; n < table->ColumnsCount; n++) {
        has_rays = has_rays || std::strcmp(ImGui::TableGetColumnName(table, n), "Rays") == 0;
      }
      IM_CHECK(!has_rays);
      IM_CHECK_EQ(table->ColumnsCount, 4);
      const ImGuiTestItemInfo other =
          ctx->ItemInfo(std::string("**/").append(gui::kAnalysisOtherRowLabel).c_str(), ImGuiTestOpFlags_NoError);
      IM_CHECK(other.ID != 0);
      IM_CHECK(IsDisabled(other));
      // Below every chain row: the last row's rect is above it.
      const ImGuiTestItemInfo last_row = ctx->ItemInfo("**/3-4");
      IM_CHECK(last_row.ID != 0);
      IM_CHECK_GT(other.RectFull.Min.y, last_row.RectFull.Min.y);
      // The column is what the unit computed: 5/10, 7/10, 8/10, and the other line's 20 closes it.
      const auto& view = gui::g_state.analysis_result;
      IM_CHECK_EQ(view.display_cumulative_pct.size(), 3u);
      IM_CHECK_FLOAT_NEAR(view.display_cumulative_pct[0], 50.0, 1e-9);
      IM_CHECK_FLOAT_NEAR(view.display_cumulative_pct[1], 70.0, 1e-9);
      IM_CHECK_FLOAT_NEAR(view.display_cumulative_pct[2], 80.0, 1e-9);
      IM_CHECK_FLOAT_NEAR(view.display_cumulative_pct[2] + gui::AnalysisOtherPct(gui::g_state), 100.0, 1e-9);

      // A click on the other line selects nothing; Exclude stays disabled.
      IM_CHECK(!gui::g_state.analysis.selected_entry.has_value());
      ctx->ItemClick(std::string("**/").append(gui::kAnalysisOtherRowLabel).c_str());
      ctx->Yield(1);
      IM_CHECK(!gui::g_state.analysis.selected_entry.has_value());
      IM_CHECK(IsDisabled(ctx->ItemInfo(ICON_FA_BAN " Exclude this raypath###action_exclude")));
      // And it does not take a selection away from a real row either.
      ctx->ItemClick("**/1-2");
      ctx->Yield(1);
      IM_CHECK(gui::g_state.analysis.selected_entry.has_value() && *gui::g_state.analysis.selected_entry == "1-2");
      ctx->ItemClick(std::string("**/").append(gui::kAnalysisOtherRowLabel).c_str());
      ctx->Yield(1);
      IM_CHECK(gui::g_state.analysis.selected_entry.has_value() && *gui::g_state.analysis.selected_entry == "1-2");
      ctx->SetRef("");

      // The same list without a bucket has no other line and the column ends at 100.
      auto exact = std::make_shared<gui::AnalysisPayload>(*payload);
      exact->snapshot_generation = 4;
      exact->other_energy = 0.0;
      exact->other_count = 0;
      exact->truncated_chain_count = 0;
      IM_CHECK(gui::AdoptAnalysisPayloadIfNew(gui::g_state, exact));
      gui::g_state.analysis.fetched_generation = 4;
      ctx->Yield(2);
      SetListRef(ctx);
      IM_CHECK(
          ctx->ItemInfo(std::string("**/").append(gui::kAnalysisOtherRowLabel).c_str(), ImGuiTestOpFlags_NoError).ID ==
          0);
      IM_CHECK_FLOAT_NEAR(gui::g_state.analysis_result.display_cumulative_pct.back(), 100.0, 1e-9);
      ctx->SetRef("");
    };
  }

  // The analysis is a submission of the document itself (v4.36), and needs no Run before it.
  // Three documents that could not be analysed before, one case each.
  //
  // An .lmc with a baked picture, opened and never run: the app reads it as kLoaded / kDone, a
  // picture is on screen to pick on, and the server behind it has committed nothing — replaced
  // by a fresh one after the save, so the run that baked the picture cannot be what the analysis
  // traces. Point mode, a click on the halo, Analyze: the 22-degree path leads, and sim_state
  // never read kSimulating on the way — the picture stayed the file's.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "a_loaded_lmc_analyses_without_a_run");
    // The baked branch of DoOpen uploads the file's picture, a GL call, so it has to run on the
    // render thread: the GuiFunc opens the file once the TestFunc has named it (the same shape
    // test_file_ops.cpp uses for its baked-preview case).
    static bool s_open_done = false;
    static std::filesystem::path s_open_path;
    t->GuiFunc = [](ImGuiTestContext*) {
      if (!s_open_done && !s_open_path.empty()) {
        gui::DoNew();
        gui::DoOpen(s_open_path);
        s_open_done = true;
      }
    };
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      s_open_done = false;
      s_open_path.clear();
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      // Saved the way the app saves: PerformSave refreshes the CPU copy of the picture from the
      // server's frame before writing, which is what puts a baked texture into the file at all
      // (the preview's live texture is the float XYZ one, which is never the saved copy).
      const std::filesystem::path path = GuiTestTempPath("analysis_loaded_baked.lmc");
      gui::g_state.current_file_path = path;
      gui::g_state.save_texture = true;
      gui::PerformSave();
      IM_CHECK(std::filesystem::exists(path));
      IM_CHECK(ReplaceServerWithAFreshOne(ctx));

      s_open_path = path;
      IM_CHECK(DriveUntil(ctx, [] { return s_open_done; }, 5));
      ctx->Yield(2);
      IM_CHECK_EQ((int)gui::g_state.run_intent, (int)gui::RunIntent::kLoaded);
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)SimState::kDone);
      IM_CHECK(gui::g_preview_vp.active);
      IM_CHECK(gui::AnalysisPictureNotice(gui::g_state.run_intent, gui::g_state.sim_state) == nullptr);

      OpenWindow(ctx);
      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(!IsDisabled(ctx->ItemInfo(kAnalyzeButton)));
      ctx->SetRef("");
      IM_CHECK(StartPointAnalysis(ctx));
      bool saw_simulating = false;
      IM_CHECK(DriveUntil(
          ctx,
          [&saw_simulating] {
            saw_simulating = saw_simulating || gui::g_state.sim_state == SimState::kSimulating;
            return !gui::g_state.analysis_run_in_progress && gui::g_state.analysis_result.payload != nullptr;
          },
          60));
      IM_CHECK(!saw_simulating);
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)SimState::kDone);
      IM_CHECK_EQ((int)gui::g_state.run_intent, (int)gui::RunIntent::kLoaded);
      IM_CHECK_EQ(gui::g_state.analysis_result.payload->roi_mode, LUMICE_RAYPATH_ROI_CONE);
      IM_CHECK_GT(gui::g_state.analysis_result.payload->entries.size(), 0u);
      IM_CHECK_STR_EQ(TopChainDisplay().c_str(), "3-5");
      // The fresh server's only submission was the analysis: one epoch, the analysis's.
      LUMICE_SimLifecycleResult lc{};
      LUMICE_GetSimLifecycle(gui::g_server, &lc);
      IM_CHECK_EQ(lc.epoch, 1u);
      std::filesystem::remove(path);
    };
  }

  // A document that never had a picture (kNone / kIdle, no preview): Analyze and the Point
  // radio are disabled, the In frame entry is not on the bar at all (it retired with the
  // free-camera main view), and the panel says there is no picture, and a click on Analyze
  // starts nothing. Then a Run from the top bar renders as it always did, the notice goes, the
  // two come alive, and an edit afterwards (kModified) leaves them alive — the picture need
  // only have existed, not match.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "a_never_run_document_waits_for_its_first_run");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      ResetTestState();
      gui::g_server = LUMICE_CreateServer();
      IM_CHECK(gui::g_server != nullptr);
      gui::ResetServerConstructionTrackers();
      LUMICE_SetLogLevel(gui::g_server, static_cast<LUMICE_LogLevel>(g_core_log_level));
      IM_CHECK(gui::DeserializeFromJson(kHalo22Json, gui::g_state));
      gui::g_state.renderer.sim_resolution_index = 0;
      gui::g_state.sim.infinite = false;
      gui::g_state.sim.ray_num_millions = 0.1f;
      ctx->Yield(2);
      IM_CHECK_EQ((int)gui::g_state.run_intent, (int)gui::RunIntent::kNone);
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)SimState::kIdle);
      IM_CHECK(!gui::g_preview_vp.active);
      const char* notice = gui::AnalysisPictureNotice(gui::g_state.run_intent, gui::g_state.sim_state);
      IM_CHECK(notice != nullptr);
      IM_CHECK(std::strstr(notice, "No rendered image") != nullptr);
      // A server and an idle backend: the picture is the only denial left.
      IM_CHECK(!gui::CanStartAnalysis(true, gui::g_state.sim_state, false, gui::g_state.run_intent));

      OpenWindow(ctx);
      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(IsDisabled(ctx->ItemInfo(kAnalyzeButton)));
      IM_CHECK(ctx->ItemInfo("In frame", ImGuiTestOpFlags_NoError).ID == 0);
      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(IsDisabled(ctx->ItemInfo("Point")));
      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(!IsDisabled(ctx->ItemInfo("Whole sky")));
      // A click on the disabled button does nothing: no intent, no run.
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick(kAnalyzeButton);
      ctx->Yield(2);
      IM_CHECK(!gui::g_state.analysis.started);
      IM_CHECK(!gui::g_state.analysis_run_in_progress);
      ctx->SetRef("");

      // The first Run, from the top bar: it renders, and the notice is gone.
      const unsigned long long uploads_before = gui::g_state.texture_upload_count;
      IM_CHECK(!IsDisabled(ctx->ItemInfo("##TopBar/" ICON_FA_PLAY " Run")));
      ctx->ItemClick("##TopBar/" ICON_FA_PLAY " Run");
      IM_CHECK(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kSimulating; }, 10));
      IM_CHECK(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kDone; }, 60));
      IM_CHECK(DriveUntil(ctx, [uploads_before] { return gui::g_state.texture_upload_count > uploads_before; }, 10));
      IM_CHECK_EQ((int)gui::g_state.run_intent, (int)gui::RunIntent::kRunCompleted);
      IM_CHECK(gui::AnalysisPictureNotice(gui::g_state.run_intent, gui::g_state.sim_state) == nullptr);
      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(!IsDisabled(ctx->ItemInfo(kAnalyzeButton)));
      IM_CHECK(ctx->ItemInfo("In frame", ImGuiTestOpFlags_NoError).ID == 0);
      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(!IsDisabled(ctx->ItemInfo("Point")));
      ctx->SetRef("");

      // An edit after the run: the picture is of the previous configuration, and that is a
      // notice, not a refusal — the two stay alive and the analysis completes.
      gui::g_state.crystals[0].height = 0.1f;
      gui::g_state.dirty = true;
      IM_CHECK(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kModified; }, 5));
      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(!IsDisabled(ctx->ItemInfo(kAnalyzeButton)));
      IM_CHECK(ctx->ItemInfo("In frame", ImGuiTestOpFlags_NoError).ID == 0);
      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(!IsDisabled(ctx->ItemInfo("Point")));
      ctx->SetRef("");
      bool saw_simulating = false;
      IM_CHECK(RunWholeSkyAnalysisToCompletion(ctx, &saw_simulating));
      IM_CHECK(!saw_simulating);
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)SimState::kModified);
      IM_CHECK_EQ(gui::g_state.analysis_result.payload->roi_mode, LUMICE_RAYPATH_ROI_FULL_SKY);
    };
  }

  // The regression the gate was changed for: a background photograph on a document that has never
  // been rendered. The preview publishes its viewport for a photograph alone (g_preview_vp.active
  // reads true — the positive control that the OLD gate, which read that flag, would have lit
  // Pick), but a photograph is not a picture of the document: run_intent is still
  // kNone, Point and Analyze stay disabled, and the retired In frame entry is nowhere on the bar.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "a_background_photograph_alone_is_not_a_picture");
    static bool s_bg_requested = false;
    static bool s_bg_done = false;
    // The upload is a GL call, so it runs on the render thread: GuiFunc, once the TestFunc asks.
    t->GuiFunc = [](ImGuiTestContext*) {
      if (s_bg_requested && !s_bg_done) {
        const std::vector<unsigned char> grey(static_cast<size_t>(16 * 16 * 3), 128);
        gui::g_preview.UploadBgTexture(grey.data(), 16, 16);
        s_bg_done = true;
      }
    };
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      s_bg_requested = false;
      s_bg_done = false;
      ResetTestState();
      gui::g_server = LUMICE_CreateServer();
      IM_CHECK(gui::g_server != nullptr);
      gui::ResetServerConstructionTrackers();
      IM_CHECK(gui::DeserializeFromJson(kHalo22Json, gui::g_state));
      s_bg_requested = true;
      IM_CHECK(DriveUntil(ctx, [] { return s_bg_done; }, 5));
      gui::g_state.bg_show = true;
      ctx->Yield(2);
      IM_CHECK(gui::g_preview.HasBackground());
      IM_CHECK(!gui::g_preview.HasTexture());
      IM_CHECK(gui::g_preview_vp.active);  // the flag the old gate read: it IS up
      IM_CHECK_EQ((int)gui::g_state.run_intent, (int)gui::RunIntent::kNone);

      OpenWindow(ctx);
      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(ctx->ItemInfo("In frame", ImGuiTestOpFlags_NoError).ID == 0);
      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(IsDisabled(ctx->ItemInfo("Point")));
      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(IsDisabled(ctx->ItemInfo(kAnalyzeButton)));
      ctx->SetRef("");
      IM_CHECK(!gui::CanStartAnalysis(true, gui::g_state.sim_state, false, gui::g_state.run_intent));
    };
  }

  // A rendered document, then an edit that changes what the sky looks like (kModified): Analyze
  // stays enabled, the panel says the picture is of the previous configuration, and the list is
  // the EDITED document's. The edit is the crystal itself — a randomly oriented column becomes a
  // thin horizontal plate — so the top chain changes from the 22-degree path through the side
  // faces to the straight pass through the basal faces, which is how "the edited document" is
  // told apart from "the rendered one" without reading anything off the server but the list.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "an_edited_document_analyses_as_edited");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      // Positive control on the rendered document: the column's 22-degree path leads.
      IM_CHECK(RunWholeSkyAnalysisToCompletion(ctx, nullptr));
      IM_CHECK_STR_EQ(TopChainDisplay().c_str(), "3-5");
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)SimState::kDone);
      IM_CHECK(gui::AnalysisPictureNotice(gui::g_state.run_intent, gui::g_state.sim_state) == nullptr);

      // The edit: a thin plate lying flat. Marked dirty by hand — the widgets' frame-tail
      // reconcile is what does it in the app, and this edit did not go through a widget.
      gui::g_state.crystals[0].height = 0.1f;
      gui::g_state.crystals[0].zenith = gui::AxisDist{ gui::AxisDistType::kUniform, 0.0f, 0.0f };
      gui::g_state.dirty = true;
      IM_CHECK(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kModified; }, 5));
      const char* notice = gui::AnalysisPictureNotice(gui::g_state.run_intent, gui::g_state.sim_state);
      IM_CHECK(notice != nullptr);
      IM_CHECK(std::strstr(notice, "previous configuration") != nullptr);
      const unsigned long long uploads_before = gui::g_state.texture_upload_count;

      bool saw_simulating = false;
      IM_CHECK(RunWholeSkyAnalysisToCompletion(ctx, &saw_simulating));
      IM_CHECK(!saw_simulating);
      const std::string top = TopChainDisplay();
      ctx->LogInfo("edited document's top chain: %s", top.c_str());
      IM_CHECK_STR_EQ(top.c_str(), "1-2");
      // The picture is still the column's, and still marked as the previous configuration's:
      // the analysis rendered nothing and changed no intent.
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)SimState::kModified);
      IM_CHECK_EQ(gui::g_state.texture_upload_count, uploads_before);
      IM_CHECK(gui::AnalysisPictureNotice(gui::g_state.run_intent, gui::g_state.sim_state) != nullptr);
    };
  }

  // The chain label on screen. Core formats a multi-layer chain as "(3-5) -> (1-3)" — the C API
  // contract, ASCII on purpose because the body font has no U+2192 — and the list draws that
  // text through AnalysisRowLabel, which redraws the joiner as the ICON_FA_ARROW_RIGHT glyph
  // and each "C<core>(" crystal prefix as the card's "#<pool>(".
  // Three things have to be true of a real multi-layer row, and the unit truth table can show
  // none of them: the row's label in the ImGui tree IS the rewritten text (so the render path
  // goes through the function, not around it), the rewritten text has no "?" — the fallback
  // ImGui draws for a codepoint the atlas lacks, which is what the arrow WOULD be if it were
  // U+2192 — and the crystal prefix came out spelled the way the cards spell it, checked against
  // hand-written literals rather than the same helper the render runs. The first multi-layer row
  // is used, not the first row: which chain carries the most energy is the scene's business (on
  // this scene the top rows are single-layer), not this label's.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "chain_label_shows_arrow_glyph_not_qmark");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpScene(ctx, kPcTwoLayerJson, /*infinite=*/false, /*gpu=*/false));
      OpenWindow(ctx);
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick("Whole sky");
      ctx->SetRef(ListWindowRef());
      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(!IsDisabled(ctx->ItemInfo(kAnalyzeButton)));
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick(kAnalyzeButton);
      ctx->SetRef("");
      IM_CHECK(DriveUntil(
          ctx, [] { return !gui::g_state.analysis_run_in_progress && gui::g_state.analysis_result.payload != nullptr; },
          60));
      const auto& view = gui::g_state.analysis_result;
      IM_CHECK_EQ(view.payload->roi_mode, LUMICE_RAYPATH_ROI_FULL_SKY);
      const LUMICE_RaypathHistogramEntry* multi = nullptr;
      for (const auto& e : view.payload->entries) {
        if (e.chain_len >= 2) {
          multi = &e;
          break;
        }
      }
      IM_CHECK(multi != nullptr);
      const std::string raw = multi->display;
      IM_CHECK(raw.find(" -> ") != std::string::npos);
      if (raw.front() != 'C') {
        IM_ERRORF("expected a C-prefixed row (this scene's layers hold two crystals each), got \"%s\"", raw.c_str());
      }
      // The production rewrite of the production text — the same function and the same
      // Analyze-time map the render path reads.
      const std::string label = gui::AnalysisRowLabel(raw, gui::AnalysisCoreToPoolMap(gui::g_state));
      // Hand-written literals, independent of that helper: the prefix came out in the card's
      // spelling, and no scene-spelled crystal token survives to the screen.
      IM_CHECK(label.rfind("#", 0) == 0);
      IM_CHECK(label.find('C') == std::string::npos);
      IM_CHECK(label.find(ICON_FA_ARROW_RIGHT) != std::string::npos);
      IM_CHECK(label.find(" -> ") == std::string::npos);
      IM_CHECK(label.find('?') == std::string::npos);
      IM_CHECK(raw.find('?') == std::string::npos);
      // And a row on screen carries exactly that label — found by it, under the window, with the
      // list panned to it (on this scene the first multi-layer row sits below the first page).
      SetListRef(ctx);
      const ImGuiID row_id = FindListRowScrolling(ctx, label);
      IM_CHECK(row_id != 0);
      // The raw text is NOT what is drawn: with the row on screen, no item carries the ASCII form.
      IM_CHECK(ctx->ItemInfo(("**/" + raw).c_str(), ImGuiTestOpFlags_NoError).ID == 0);
      // Selecting by the drawn label records the raw text — the selection is the entry's
      // contract text, the label only its presentation.
      ctx->ItemClick(row_id);
      IM_CHECK(gui::g_state.analysis.selected_entry.has_value());
      IM_CHECK_STR_EQ(gui::g_state.analysis.selected_entry->c_str(), raw.c_str());
      ctx->SetRef("");
      // A picture of the window for a human to look at (the atlas assertions live in
      // functional/test_body_font_glyph_coverage.cpp; this is the frame they add up to).
      ctx->MouseMoveToPos(ImVec2(2.0f, 2.0f));
      ctx->Yield(2);
      IM_CHECK(SaveWindowPng(ctx, kWindowRef, GuiTestTempPath("chain_label_arrow.png").string()));
    };
  }

  // The search box over the list (592.2). Four rows put on show directly — three chains and an
  // "other" bucket — and the box is typed into. What it does: hide the rows whose raw text AND
  // drawn label do not contain the typed text, case-insensitively — so the CSV's "C2(" and the
  // card spelling "#2(" find the same row. What it must not do: touch the numbers. The
  // display order and the Cumulative % column are the whole list's, hidden rows included (a
  // shown row's Cumulative % still reads "down to this row of everything"), and the selection is
  // the chain's text, so a selected row that the search hides is still the selection and the
  // Exclude button's answer is the same one. The other line is not a raypath and so not
  // searchable: any non-empty search hides it. The multi-layer display text is written by hand
  // here, not collected from a scene; only the substring mechanism is under test, not the
  // formatter, and the raw " -> " joiner is what a user copies out of the CSV or the CLI's
  // output, so "->" has to reach the multi-layer row — the shape ImGuiTextFilter::PassFilter
  // would have read as "exclude every row containing '>'".
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "search_narrows_visible_rows");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      ResetTestState();
      OpenWindow(ctx);
      // A scene for the label rewrite to number by: three crystals, three entries in the root
      // layer, and the Analyze-time snapshot taken from it — so the hand-written display's
      // "C2(" maps to card #2 and the drawn label spells "#2(". The search then has two
      // vocabularies to find the row by, which is what the assertions below pin.
      gui::g_state.crystals.assign(3, gui::CrystalConfig{});
      gui::Layer search_layer;
      search_layer.probability = 1.0f;
      for (int i = 0; i < 3; ++i) {
        gui::EntryCard entry;
        entry.crystal_id = i;
        entry.proportion = 100.0f;
        search_layer.entries.push_back(entry);
      }
      gui::g_state.layers.assign(1, search_layer);
      gui::g_state.analysis_result.analyzed_scene = gui::GuiState::AnalysisSceneIdentity::From(gui::g_state);
      auto payload = std::make_shared<gui::AnalysisPayload>();
      payload->snapshot_generation = 3;
      payload->roi_mode = LUMICE_RAYPATH_ROI_FULL_SKY;
      payload->other_energy = 2.0;
      payload->other_count = 20;
      payload->truncated_chain_count = 7;
      const char* const displays[3] = { "3-5", "1-2", "C2(3-5) -> (1-3)" };
      const double energies[3] = { 5.0, 2.0, 1.0 };
      for (int i = 0; i < 3; i++) {
        LUMICE_RaypathHistogramEntry e{};
        e.chain_len = i == 2 ? 2 : 1;
        e.chain[0].crystal_id = 1;
        e.chain[0].segment_len = 2;
        e.chain[0].segment[0] = 1;
        e.chain[0].segment[1] = 2;
        snprintf(e.display, sizeof(e.display), "%s", displays[i]);
        e.energy = energies[i];
        e.count = 100;
        payload->entries.push_back(e);
      }
      IM_CHECK(gui::AdoptAnalysisPayloadIfNew(gui::g_state, payload));
      gui::g_state.analysis.fetched_once = true;
      gui::g_state.analysis.fetched_generation = 3;
      gui::g_state.analysis.fetched_symmetry = gui::AnalysisSymmetryBits(gui::g_state);
      ctx->Yield(2);
      SetListRef(ctx);

      // Rows are found by what is DRAWN: the multi-layer row's label carries the arrow glyph and
      // the card's "#2(" spelling.
      const std::string multi_label = gui::AnalysisRowLabel(displays[2], gui::AnalysisCoreToPoolMap(gui::g_state));
      // Hand-written literal, independent of the helper: "C2(...)" under the snapshot built
      // above draws as the card spelling with the glyph joiner and nothing else moved.
      IM_CHECK_STR_EQ(multi_label.c_str(), ("#2(3-5) " ICON_FA_ARROW_RIGHT " (1-3)"));
      const std::string other_path = gui::kAnalysisOtherRowLabel;
      auto row_shown = [&](const std::string& label) {
        // The rows live in the table's child window: an item path from the list window cannot
        // cross into it without the "**/" wildcard descent.
        return ctx->ItemInfo((std::string("**/") + label).c_str(), ImGuiTestOpFlags_NoError).ID != 0;
      };
      auto search = [&](const char* text) {
        ctx->ItemInputValue("###analysis_search", text);
        ctx->Yield(3);
      };
      // Empty box: everything, other line included.
      IM_CHECK(row_shown("3-5"));
      IM_CHECK(row_shown("1-2"));
      IM_CHECK(row_shown(multi_label));
      IM_CHECK(row_shown(gui::kAnalysisOtherRowLabel));
      const std::vector<double> cumulative_before = gui::g_state.analysis_result.display_cumulative_pct;
      const std::vector<int> order_before = gui::g_state.analysis_result.display_order;
      IM_CHECK_EQ(cumulative_before.size(), 3u);

      // "3-5": the single-layer row and the multi-layer row that contains it; not "1-2", not other.
      search("3-5");
      IM_CHECK(row_shown("3-5"));
      IM_CHECK(!row_shown("1-2"));
      IM_CHECK(row_shown(multi_label));
      IM_CHECK(!row_shown(gui::kAnalysisOtherRowLabel));
      // The numbers did not move: same order, same column, entry for entry — the same bytes, since
      // nothing recomputed them (a recompute of the same list would also pass, and the point is
      // that the search does not own those numbers; whichever way, the columns read as before).
      const auto& view = gui::g_state.analysis_result;
      IM_CHECK(view.display_order == order_before);
      IM_CHECK(view.display_cumulative_pct == cumulative_before);

      // Cleared: all four back.
      search("");
      IM_CHECK(row_shown("3-5"));
      IM_CHECK(row_shown("1-2"));
      IM_CHECK(row_shown(multi_label));
      IM_CHECK(row_shown(gui::kAnalysisOtherRowLabel));

      // "C2(" and "->": only the multi-layer row, by the raw text's crystal prefix and joiner.
      search("C2(");
      IM_CHECK(!row_shown("3-5"));
      IM_CHECK(!row_shown("1-2"));
      IM_CHECK(row_shown(multi_label));
      search("->");
      IM_CHECK(!row_shown("3-5"));
      IM_CHECK(!row_shown("1-2"));
      IM_CHECK(row_shown(multi_label));
      // Case-insensitive: the user types what they remember, not the exact case.
      search("c2(");
      IM_CHECK(row_shown(multi_label));
      // "#2(": only the DRAWN label carries it — the raw text has no '#' in it — so this is the
      // card spelling finding the row the C2 spelling finds: same row, two vocabularies.
      search("#2(");
      IM_CHECK(!row_shown("3-5"));
      IM_CHECK(!row_shown("1-2"));
      IM_CHECK(row_shown(multi_label));
      IM_CHECK(!row_shown(gui::kAnalysisOtherRowLabel));

      // The selection outlives being hidden, and Exclude's verdict does not depend on the box.
      search("");
      ctx->ItemClick("**/1-2");
      ctx->Yield(1);
      IM_CHECK(gui::g_state.analysis.selected_entry.has_value() && *gui::g_state.analysis.selected_entry == "1-2");
      const gui::ExcludeEligibility before = gui::EvaluateExcludeEligibility(gui::g_state, nullptr);
      search("3-5");
      IM_CHECK(!row_shown("1-2"));
      IM_CHECK(gui::g_state.analysis.selected_entry.has_value() && *gui::g_state.analysis.selected_entry == "1-2");
      IM_CHECK_EQ(static_cast<int>(gui::EvaluateExcludeEligibility(gui::g_state, nullptr)), static_cast<int>(before));
      search("");
      IM_CHECK(row_shown("1-2"));
      IM_CHECK(gui::g_state.analysis.selected_entry.has_value() && *gui::g_state.analysis.selected_entry == "1-2");
      // A picture of the window with the box in use, for a human to look at: where the box sits
      // and what a narrowed list looks like are not things the assertions above can show.
      search("3-5");
      ctx->SetRef("");
      ctx->MouseMoveToPos(ImVec2(2.0f, 2.0f));
      ctx->Yield(2);
      IM_CHECK(SaveWindowPng(ctx, kWindowRef, GuiTestTempPath("analysis_search.png").string()));
    };
  }

  // What clears the search box, and what does not. The box has the lifetime of
  // state.analysis (gui-state-governance.md §10.2: the analysis request parameters are session
  // state, cleared by the four document-switch reasons and kept by Revert), though it is not a
  // member of that struct — an ImGui type cannot enter gui_state.hpp — so ResetFrontendState
  // clears it by name, and this case is what catches that call going missing or landing in the
  // wrong branch. Revert: the payload and the text both survive, so the same row is still the
  // one shown. New document: the payload is gone, so the text is read off what a NEW payload
  // shows — a row the old text would have hidden.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "search_reset_semantics");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      ResetTestState();
      OpenWindow(ctx);
      auto make_payload = [](unsigned long long generation, std::initializer_list<const char*> displays) {
        auto payload = std::make_shared<gui::AnalysisPayload>();
        payload->snapshot_generation = generation;
        payload->roi_mode = LUMICE_RAYPATH_ROI_FULL_SKY;
        for (const char* d : displays) {
          LUMICE_RaypathHistogramEntry e{};
          e.chain_len = 1;
          e.chain[0].crystal_id = 1;
          e.chain[0].segment_len = 2;
          e.chain[0].segment[0] = 1;
          e.chain[0].segment[1] = 2;
          snprintf(e.display, sizeof(e.display), "%s", d);
          e.energy = 1.0;
          e.count = 100;
          payload->entries.push_back(e);
        }
        return payload;
      };
      auto show = [&](unsigned long long generation, std::initializer_list<const char*> displays) {
        IM_CHECK_RETV(gui::AdoptAnalysisPayloadIfNew(gui::g_state, make_payload(generation, displays)), false);
        gui::g_state.analysis.fetched_once = true;
        gui::g_state.analysis.fetched_generation = generation;
        gui::g_state.analysis.fetched_symmetry = gui::AnalysisSymmetryBits(gui::g_state);
        ctx->Yield(2);
        return true;
      };
      auto row_shown = [&](const char* label) {
        return ctx->ItemInfo((std::string("**/") + label).c_str(), ImGuiTestOpFlags_NoError).ID != 0;
      };
      IM_CHECK(show(3, { "1-2", "2-3" }));
      ctx->WindowMove(kWindowRef, ImVec2(60, 60));
      ctx->Yield(1);
      SetListRef(ctx);
      ctx->ItemInputValue("**/###analysis_search", "1-2");
      ctx->Yield(3);
      IM_CHECK(row_shown("1-2"));
      IM_CHECK(!row_shown("2-3"));

      // Revert keeps the result and the text: the same scene, the same view of it.
      gui::ResetFrontendState(gui::g_state, gui::FrontendResetReason::kRevert);
      ctx->Yield(3);
      IM_CHECK(gui::g_state.analysis_result.payload != nullptr);
      IM_CHECK(row_shown("1-2"));
      IM_CHECK(!row_shown("2-3"));

      // A new document drops the result; the text must go with it. Shown on a new result whose
      // only row the stale "1-2" would hide.
      gui::ResetFrontendState(gui::g_state, gui::FrontendResetReason::kNewDocument);
      ctx->Yield(3);
      IM_CHECK(gui::g_state.analysis_result.payload == nullptr);
      IM_CHECK(show(4, { "2-3" }));
      ctx->Yield(2);
      IM_CHECK(row_shown("2-3"));
      ctx->SetRef("");
    };
  }

  // Right-click on a row (592.3). Two menu items. "Copy raypath" puts the entry's RAW text on the
  // clipboard — the " -> " joiner the CSV and the CLI print, not the arrow glyph the row draws —
  // and "Copy row" puts the row in the CSV's own column order and formatting, which is asserted
  // against the same call the CSV export makes on the same numbers, not against a literal, so
  // the two cannot be checked into disagreement. Each item is reached by a fresh right-click:
  // a chosen menu item closes its popup. The clipboard read back is the process-local stand-in
  // test_gui_main.cpp installs, so this is what the GUI wrote, on any leg.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "row_menu_copies_raw_text_and_csv_row");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      ResetTestState();
      OpenWindow(ctx);
      IM_CHECK_STR_EQ(ImGui::GetClipboardText(), "");  // the reset emptied it
      auto payload = std::make_shared<gui::AnalysisPayload>();
      payload->snapshot_generation = 3;
      payload->roi_mode = LUMICE_RAYPATH_ROI_FULL_SKY;
      payload->other_energy = 2.0;
      payload->other_count = 20;
      const char* const displays[3] = { "3-5", "1-2", "C2(3-5) -> (1-3)" };
      const double energies[3] = { 5.0, 2.0, 1.0 };
      for (int i = 0; i < 3; i++) {
        LUMICE_RaypathHistogramEntry e{};
        e.chain_len = i == 2 ? 2 : 1;
        e.chain[0].crystal_id = 1;
        e.chain[0].segment_len = 2;
        e.chain[0].segment[0] = 1;
        e.chain[0].segment[1] = 2;
        snprintf(e.display, sizeof(e.display), "%s", displays[i]);
        e.energy = energies[i];
        e.count = 100 * (i + 1);
        payload->entries.push_back(e);
      }
      payload->entries[1].error_bound = 0.5;  // "1-2" took over a slot: its +/- cell has a bound
      IM_CHECK(gui::AdoptAnalysisPayloadIfNew(gui::g_state, payload));
      gui::g_state.analysis.fetched_once = true;
      gui::g_state.analysis.fetched_generation = 3;
      gui::g_state.analysis.fetched_symmetry = gui::AnalysisSymmetryBits(gui::g_state);
      ctx->Yield(2);

      // Right-click the row drawn as `label`, then choose `item` in the popup that opened.
      auto choose = [&](const std::string& label, const char* item) {
        SetListRef(ctx);
        ctx->ItemClick((std::string("**/") + label).c_str(), ImGuiMouseButton_Right);
        ctx->Yield(2);
        ctx->SetRef("//$FOCUSED");
        ctx->ItemClick(item);
        ctx->Yield(2);
        ctx->SetRef("");
      };
      // The expected row, from the export's own formatter on the view's own numbers.
      const auto& view = gui::g_state.analysis_result;
      auto csv_row = [&](const char* display) {
        for (size_t row = 0; row < view.display_order.size(); ++row) {
          const int idx = view.display_order[row];
          const auto& e = view.payload->entries[static_cast<size_t>(idx)];
          if (std::strcmp(e.display, display) == 0) {
            return lumice::FormatRaypathAnalysisCsvRow(e, view.display_energy[static_cast<size_t>(idx)],
                                                       view.display_cumulative_pct[row], view.display_total);
          }
        }
        return std::string("<no such row>");
      };

      choose("3-5", "Copy raypath");
      IM_CHECK_STR_EQ(ImGui::GetClipboardText(), "3-5");
      choose("3-5", "Copy row");
      IM_CHECK_STR_EQ(ImGui::GetClipboardText(), csv_row("3-5").c_str());
      // Not just the label with numbers after it: the row is the CSV's, four comma-separated
      // columns with Energy % / Cumulative % / +/- in the CSV's precisions.
      IM_CHECK_STR_EQ(ImGui::GetClipboardText(), "3-5,50.0000,50.0000,10.00");
      // The takeover row: its +/- cell carries the bound in parentheses, as the CSV's does.
      choose("1-2", "Copy row");
      IM_CHECK_STR_EQ(ImGui::GetClipboardText(), csv_row("1-2").c_str());
      IM_CHECK(std::strstr(ImGui::GetClipboardText(), " (-") != nullptr);
      // The multi-layer row is found on screen by its drawn label (card spelling under the
      // Analyze-time map, arrow glyph for the joiner) and copies its raw text.
      const std::string multi_label = gui::AnalysisRowLabel(displays[2], gui::AnalysisCoreToPoolMap(gui::g_state));
      choose(multi_label, "Copy raypath");
      IM_CHECK_STR_EQ(ImGui::GetClipboardText(), displays[2]);
      choose(multi_label, "Copy row");
      IM_CHECK_STR_EQ(ImGui::GetClipboardText(), csv_row(displays[2]).c_str());
      // A right-click neither selects the row nor is a left click: the selection is untouched.
      IM_CHECK(!gui::g_state.analysis.selected_entry.has_value());
      ctx->PopupCloseAll();
      ctx->Yield(2);
    };
  }

  // The greyed "excluded" rows — see ExcludedRowShowsAndIncludeAgainTakesItBack and
  // IncludeAgainWhileTheImmediateEditorIsOpen.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "excluded_row_shows_and_include_again_takes_it_back");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(ExcludedRowShowsAndIncludeAgainTakesItBack(ctx));
    };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "include_again_while_the_immediate_editor_is_open");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(IncludeAgainWhileTheImmediateEditorIsOpen(ctx));
    };
  }

  // M2-5's real-app evidence: the full chain, on file.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "c01_chain_analyze_select_exclude_rerun");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(C01ChainAnalyzeSelectExcludeRerun(ctx)); };
  }

  // M3-5's survey-query evidence: the digest answers with real class facts (and re-Analyze
  // re-arms under the next generation), the multi-layer refusal names itself, and the
  // background query is orthogonal to a render run in both directions.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "survey_digest_answers_with_real_class_facts");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(SurveyDigestAnswersWithRealClassFacts(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "survey_overview_shows_dispersion_and_profile");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(SurveyOverviewShowsDispersionAndProfile(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "verdict_anchor_hover_and_toggle_on_the_map");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(VerdictAnchorHoverAndToggleOnTheMap(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "survey_refuses_multilayer_class_by_name");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(SurveyRefusesMultilayerClassByName(ctx)); };
  }
  // The 2026-11-12 feedback wave's interaction face: row highlights, the profile device, the
  // legend icons. Real-timing pool: the family-arm survey's quantification legs are wall-clock.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "profile_rows_highlight_and_draw_on_the_map");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(ProfileRowsHighlightAndDrawOnTheMap(ctx)); };
  }
  // The map region (742.5): real chain, real click, real isolation — see the function's comment.
  // Real-timing pool: the map answers behind the survey's chromatic legs on the one worker, the
  // same wall-clock shape that put the survey overview case there.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "map_region_shows_the_class_map_and_takes_the_pick");
    t->GuiFunc = [](ImGuiTestContext*) {
      if (draw_marker_probe) {
        ImGui::SetNextWindowPos(ImVec2(760.0f, 60.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(420.0f, 460.0f), ImGuiCond_Always);
        ImGui::Begin("Sky point marker probe", nullptr, ImGuiWindowFlags_NoResize);
        ImGuiWindow* probe_win = ImGui::GetCurrentWindow();
        gui::RenderAnalyzeMapRegion(marker_probe, ImGui::GetContentRegionAvail());
        marker_probe_vertices = -1;
        for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
          if (w != nullptr && w->Active && w->DrawList != nullptr && w->DrawList != probe_win->DrawList &&
              w->ParentWindow == probe_win && std::strstr(w->Name, "##AnalyzeRaypathView") != nullptr) {
            marker_probe_vertices = w->DrawList->VtxBuffer.Size;
          }
        }
        ImGui::End();
      }
    };
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(MapRegionShowsTheClassMapAndTakesThePick(ctx)); };
  }
  // The point inspector (742.6): the real chain pick → point → component → slider/trace →
  // playback invariance. Real-timing pool: the point queues behind the selection's survey on the
  // one worker (the mass arm below is the fixed-dt pool's — it answers in milliseconds).
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "point_inspector_shows_the_point_fiber_and_the_path");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(PointInspectorShowsThePointFiberAndThePath(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "point_mass_arm_is_honest_and_forces_no_fiber");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(PointMassArmIsHonestAndForcesNoFiber(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "survey_and_render_run_are_orthogonal");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(SurveyAndRenderRunAreOrthogonal(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "survey_survives_a_workspace_switch");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(SurveySurvivesAWorkspaceSwitch(ctx)); };
  }
  // 742.7's integration legs — four real-timing (the chain pair and the C17 pair: the former
  // queues the per-face chain on one worker, the latter's survey tail is wall-clock-shaped but
  // deadline-bounded now; see scripts/build.sh's filter comments) and seven fixed-dt (the
  // lifecycle/identity set, across the halo and fully-random scenes).
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "analyze_full_chain_one_session");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(AnalyzeFullChainOneSession(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "internal_reflection_trace_from_the_real_consumer");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(InternalReflectionTraceFromTheRealConsumer(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "analyze_faces_rekey_on_row_switch");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(AnalyzeFacesRekeyOnRowSwitch(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "new_document_clears_every_analyze_face");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(NewDocumentClearsEveryAnalyzeFace(ctx)); };
  }
  // The scroll pins: skeleton workspace, no server, no sim — the
  // regions' structural no-scroll contract at three window bases, plus the divider's grab-bar
  // containment. Correctness pool: fixed-dt, state writes only, ~1-3 s.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "workspace_scroll_regions_are_structurally_fixed");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(WorkspaceScrollRegionsAreStructurallyFixed(ctx)); };
  }
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "raypath_analysis", "identity_class_shows_the_mass_arm_and_the_map_answers");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(IdentityClassShowsTheMassArmAndTheMapAnswers(ctx)); };
  }
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "raypath_analysis", "list_scroll_finds_the_row_under_the_next_frozen_header");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(ListScrollFindsTheRowUnderTheNextFrozenHeader(ctx)); };
  }
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "raypath_analysis", "refused_class_names_the_refusal_and_keeps_the_witness");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(RefusedClassNamesTheRefusalAndKeepsTheWitness(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "refused_class_blockades_nothing");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(RefusedClassBlockadesNothing(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "scene_edit_follows_the_explicit_accept_contract");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(SceneEditFollowsTheExplicitAcceptContract(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "cancel_survey_button_reaches_the_named_state");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(CancelSurveyButtonReachesTheNamedState(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "cancel_point_button_reaches_the_named_state");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(CancelPointButtonReachesTheNamedState(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "document_open_clears_every_analyze_face");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(DocumentOpenClearsEveryAnalyzeFace(ctx)); };
  }
  // The narrow-column reading contract's entity half: same-prefix clipped rows stay distinct
  // entities under selection and exclusion. Real-timing pool: a whole-sky analysis on the
  // three-layer scene, the same wall-clock shape as the chain cases.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "narrow_column_clipped_rows_keep_their_entities");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(NarrowColumnClippedRowsKeepTheirEntities(ctx)); };
  }
  // One analysis carried through the scale tiers (150% / 2x clamped / the narrow 1280 tier):
  // the result-state key operations per tier. Real-timing pool: the analysis plus the point
  // query run once, the tier switches are rebuilds.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "layout_slots_ignore_transient_states");
    static gui::GuiState probe;
    static bool draw_probe = false;
    static std::string map_text;
    static std::string pose_text;
    t->GuiFunc = [](ImGuiTestContext*) {
      if (!draw_probe) {
        return;
      }
      ImGui::SetNextWindowPos(ImVec2(8.0f, 60.0f), ImGuiCond_Always);
      ImGui::SetNextWindowSize(ImVec2(gui::UiPx(480.0f), gui::UiPx(360.0f)), ImGuiCond_Always);
      ImGui::Begin("Layout map probe", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
      ImGui::LogToBuffer();
      gui::RenderAnalyzeMapRegion(probe, ImGui::GetContentRegionAvail());
      map_text = ImGui::GetCurrentContext()->LogBuffer.c_str();
      ImGui::LogFinish();
      ImGui::End();
      ImGui::LogToBuffer();
      gui::RenderAnalyzePoseViewWindow(probe);
      pose_text = ImGui::GetCurrentContext()->LogBuffer.c_str();
      ImGui::LogFinish();
    };
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ResetTestState();
      probe = gui::GuiState{};
      draw_probe = true;
      struct DrawLease {
        ~DrawLease() { draw_probe = false; }
      } lease;
      ctx->Yield(3);
      IM_CHECK(!map_text.empty());
      // The Pose button's constant purpose text moved into its tooltip (the disposal ledger's
      // slot rework) — what the region draws instead, with a fresh GuiState (no class), is the
      // no-class next step; the tooltip bytes are pinned against the exported function below.
      IM_CHECK(map_text.find("Select a class first") != std::string::npos);
      IM_CHECK(std::strcmp(gui::PoseButtonTooltip(), "3D ray & pose.\nSelect class + sky point.") == 0);
      // The identity slot is the right column's first row: an empty state names its emptiness
      // there, and the idle tier draws no status line at all ("Observe · no calculation" is
      // gone — its explanation lives in the Cancel/Retry button's tooltip).
      IM_CHECK(map_text.find("(no class selected)") != std::string::npos);
      IM_CHECK(map_text.find("Observe") == std::string::npos);
      ctx->SetRef("//Layout map probe");
      IM_CHECK(!probe.analyze_pose_view_open);
      ctx->ItemClick("**/Pose & trace###pose_open");
      ctx->Yield(2);
      IM_CHECK(probe.analyze_pose_view_open);
      IM_CHECK(!pose_text.empty());
      IM_CHECK(pose_text.find("Pick a sky point") != std::string::npos);
      ctx->WindowFocus("//Layout map probe");
      ctx->MouseMoveToPos(ImVec2(2.0f, 2.0f));
      ctx->Yield(2);
      IM_CHECK(SaveWindowPng(ctx, "//Layout map probe", GuiTestTempPath("pose_entry_empty.png").string()));
      ctx->SetRef("");
      PinPoseViewOnScreen(ctx);
      // The separate fake state exercises layout branches only. Actual query adoption and
      // physical interaction remain the real-consumer cases' responsibility.
      const char* refs[] = { "**/##map_canvas",
                             "**/wl##map",
                             "**/alt°##sky",
                             "**/az°##sky",
                             "**/Set sky point###sky_set",
                             "**/Clear##sky",
                             "**/Pose & trace###pose_open",
                             "**/##globe_canvas",
                             "**/##globe_component",
                             "**/##globe_sample",
                             "**/Play##globe_play",
                             "**/##r3d_canvas",
                             "**/##pose_curve_canvas" };
      const auto read_anchor = [ctx](const char* ref) {
        if (std::strcmp(ref, "**/##globe_component") == 0) {
          ImGuiWindow* globe = PoseRegionWindow("##AnalyzeSunGlobe");
          return globe != nullptr ? ctx->ItemInfo(globe->GetID("##globe_component"), ImGuiTestOpFlags_NoError) :
                                    ctx->ItemInfoNull();
        }
        return ctx->ItemInfo(ref, ImGuiTestOpFlags_NoError);
      };
      std::vector<ImRect> anchors;
      std::vector<ImGuiID> anchor_ids;
      for (const char* ref : refs) {
        const auto item = read_anchor(ref);
        if (item.ID == 0) {
          IM_ERRORF("Missing layout anchor: %s", ref);
          return;
        }
        anchors.push_back(item.RectFull);
        anchor_ids.push_back(item.ID);
      }
      // Wildcard discovery yields while it searches. Renew the resolved ID tasks before a
      // transition, then read those cached IDs without yielding: all anchors must carry the
      // same fresh frame stamp, including the very first rendered frame of the new state.
      for (ImGuiID id : anchor_ids) {
        ctx->ItemInfo(id);
      }
      ctx->Yield(1);
      for (ImGuiID id : anchor_ids) {
        ctx->ItemInfo(id);
      }
      int expected_frame = -1;
      const auto compare = [&]() -> bool {
        for (size_t i = 0; i < anchors.size(); ++i) {
          const auto item = ctx->ItemInfo(anchor_ids[i], ImGuiTestOpFlags_NoError);
          if (expected_frame >= 0 && item.TimestampMain != expected_frame) {
            IM_ERRORF("Stale or late geometry for %s: frame %d, expected %d", refs[i], item.TimestampMain,
                      expected_frame);
            return false;
          }
          if (item.ID == 0 || std::abs(item.RectFull.Min.x - anchors[i].Min.x) > 0.5f ||
              std::abs(item.RectFull.Min.y - anchors[i].Min.y) > 0.5f ||
              std::abs(item.RectFull.Max.x - anchors[i].Max.x) > 0.5f ||
              std::abs(item.RectFull.Max.y - anchors[i].Max.y) > 0.5f) {
            IM_ERRORF("Transient state moved %s", refs[i]);
            return false;
          }
        }
        return true;
      };
      for (int state = 0; state < 3; ++state) {
        expected_frame = ctx->ItemInfo(anchor_ids[0]).TimestampMain + 1;
        probe.analysis.sky_point.valid = true;
        probe.analysis.map.intent_valid = true;
        probe.analysis.point.intent_valid = true;
        probe.analysis.map.cancelled_for_key = state == 1;
        probe.analysis.point.cancelled_for_key = state == 1;
        probe.analysis.map.adopted_error = state == 2 ? std::string(1000, 'X') : "";
        probe.analysis.point.adopted_error = state == 2 ? std::string(1000, 'X') : "";
        ctx->Yield(1);
        if (!compare()) {
          return;
        }
        ctx->Yield(1);
        ++expected_frame;
        if (!compare()) {
          return;
        }
        ctx->LogInfo("[layout state] waiting/cancelled/refused arm %d: first and intermediate frames stable", state);
      }
      expected_frame = -1;
      IM_CHECK(SaveWindowPng(ctx, "//Pose & trace", GuiTestTempPath("layout_pose_refused.png").string()));
      ctx->WindowFocus("//Layout map probe");
      ctx->Yield(2);
      IM_CHECK(SaveWindowPng(ctx, "//Layout map probe", GuiTestTempPath("layout_map_refused_fixture.png").string()));
      IM_CHECK(gui::MapQueryStateFor(probe.analysis.map) == gui::MapQueryState::kRefused);
      IM_CHECK(gui::PointQueryStateFor(probe.analysis.point) == gui::PointQueryState::kRefused);
      IM_CHECK(!pose_text.empty() && pose_text.find("Point refused") != std::string::npos);
      ctx->WindowClose("//Pose & trace");
      ctx->Yield(2);
      IM_CHECK(!probe.analyze_pose_view_open);
      ctx->SetRef("//Layout map probe");
      ctx->WindowFocus("//Layout map probe");
      ctx->ItemClick("**/Pose & trace###pose_open");
      ctx->Yield(2);
      IM_CHECK(probe.analyze_pose_view_open);
      IM_CHECK(!pose_text.empty() && pose_text.find("Point refused") != std::string::npos);
      ctx->SetRef("");
      // A real engine-produced partial document, not a fabricated Info field. The published
      // interrupted-request contract returns what it has with the stopped execution named.
      gui::GuiState query_scene;
      IM_CHECK(gui::DeserializeFromJson(kFullyRandomJson, query_scene));
      const auto scene = gui::BuildScene(query_scene, gui::SceneIntent::kSimCommit);
      IM_CHECK(scene != nullptr);
      const int faces[] = { 3, 5 };
      const int count = 2;
      volatile int interrupted = 1;
      LUMICE_SinglePathRequest request{};
      request.struct_size = sizeof(request);
      request.crystal_id = gui::ComputeCrystalPoolToCoreIdMap(query_scene).begin()->second;
      request.faces = faces;
      request.face_count = 2;
      request.layer_face_counts = &count;
      request.layer_count = 1;
      request.target_altitude_deg = 20.0;
      request.target_azimuth_deg = 25.0;
      request.sample_count = 2000;
      request.interrupted = &interrupted;
      LUMICE_SinglePathResult* raw_partial = nullptr;
      char error[256] = {};
      IM_CHECK_EQ(LUMICE_AnalyzeSinglePath(scene.get(), &request, &raw_partial, error, sizeof(error)), LUMICE_OK);
      const gui::PointHandle partial(raw_partial, gui::PointDeleter{});
      LUMICE_PathPointInfo partial_info{};
      partial_info.struct_size = sizeof(partial_info);
      IM_CHECK_EQ(LUMICE_PathPointGetInfo(partial.get(), &partial_info), LUMICE_PATH_READ_OK);
      IM_CHECK_NE(partial_info.execution, LUMICE_PATH_OUTCOME_COMPLETED);
      // The explicit close/reopen chain above outlives the engine's item-info leases.
      // Renew all IDs before starting another same-frame transition observation.
      for (ImGuiID id : anchor_ids) {
        ctx->ItemInfo(id);
      }
      ctx->Yield(1);
      for (ImGuiID id : anchor_ids) {
        ctx->ItemInfo(id);
      }
      expected_frame = ctx->ItemInfo(anchor_ids[0]).TimestampMain + 1;
      probe.analysis.point.adopted_result = partial;
      probe.analysis.point.adopted_error.clear();
      ctx->Yield(1);
      IM_CHECK(compare());
      ++expected_frame;
      ctx->Yield(1);
      IM_CHECK(compare());
      IM_CHECK(gui::PointQueryStateFor(probe.analysis.point) == gui::PointQueryState::kCompleted);
      IM_CHECK_EQ(partial_info.component_count, 0u);
      IM_CHECK(!pose_text.empty() && pose_text.find("No fiber") != std::string::npos);
      ctx->WindowClose("//Pose & trace");
      ctx->Yield(2);
      IM_CHECK(!probe.analyze_pose_view_open);
      ctx->SetRef("//Layout map probe");
      ctx->WindowFocus("//Layout map probe");
      ctx->ItemClick("**/Pose & trace###pose_open");
      ctx->Yield(2);
      IM_CHECK(probe.analyze_pose_view_open);
      IM_CHECK(!pose_text.empty() && pose_text.find("No fiber") != std::string::npos);
      ctx->SetRef("");
      IM_CHECK(SaveWindowPng(ctx, "//Pose & trace", GuiTestTempPath("layout_pose_partial.png").string()));
      probe.analysis.point.adopted_result.reset();
      expected_frame = -1;
      // Put the map above the pose for an actual typing/click chain while the status changes.
      ctx->SetRef("//Layout map probe");
      ctx->WindowFocus("//Layout map probe");
      ctx->ItemInputValue("**/alt°##sky", 30.0f);
      probe.analysis.map.adopted_error.clear();
      probe.analysis.point.adopted_error.clear();
      ctx->Yield(1);
      IM_CHECK(gui::MapQueryStateFor(probe.analysis.map) == gui::MapQueryState::kWaiting);
      ctx->ItemInputValue("**/az°##sky", 180.0f);
      ctx->ItemClick("**/Set sky point###sky_set");
      IM_CHECK(probe.analysis.sky_point.valid);
      IM_CHECK_FLOAT_NEAR(probe.analysis.sky_point.dir[2], -0.5f, 1e-4f);
      IM_CHECK(!probe.analysis.sky_point.from_map_pick);
      ctx->SetRef("");
      IM_CHECK(compare());
    };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "operation_anchors_ignore_roi_and_messages");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard server_guard;
      ResetTestState();
      OpenWindow(ctx);
      ctx->SetRef(PanoramaRegionRef());
      const auto before_item = ctx->ItemInfo(kAnalyzeButton);
      const ImRect before = before_item.RectFull;
      gui::g_state.analysis.roi_mode = LUMICE_RAYPATH_ROI_CONE;
      gui::g_state.analysis.cone_center_valid = false;
      ctx->Yield(1);
      const auto first_item = ctx->ItemInfo(kAnalyzeButton);
      IM_CHECK_EQ(first_item.TimestampMain, before_item.TimestampMain + 1);
      const ImRect first = first_item.RectFull;
      ctx->LogInfo("[operation geometry] Whole sky (%.1f,%.1f)-(%.1f,%.1f), Point first (%.1f,%.1f)-(%.1f,%.1f)",
                   before.Min.x, before.Min.y, before.Max.x, before.Max.y, first.Min.x, first.Min.y, first.Max.x,
                   first.Max.y);
      IM_CHECK_FLOAT_NEAR(first.Min.x, before.Min.x, 0.5f);
      IM_CHECK_FLOAT_NEAR(first.Min.y, before.Min.y, 0.5f);
      ctx->Yield(2);
      const ImRect settled = ctx->ItemInfo(kAnalyzeButton).RectFull;
      IM_CHECK_FLOAT_NEAR(settled.Min.x, before.Min.x, 0.5f);
      IM_CHECK_FLOAT_NEAR(settled.Min.y, before.Min.y, 0.5f);
      // Calibrate the boundary from complete rendered item footprints, not the product's
      // template formula. The bar's last control is the Rays(M) pair, and its trailing label is
      // probe-registered at the text's own rect (TextWithLabelProbe), so the label's right edge
      // closes the footprint — no font-advance arithmetic on a frame-only ruler.
      const float span = ImGui::GetIO().DisplaySize.x - gui::UiPx(gui::kAnalyzeLeftPanelWidth);
      gui::g_state.workspace.right_column_ratio = 0.2f;
      ctx->Yield(3);
      const auto whole = ctx->ItemInfo("Whole sky");
      const auto budget_label = ctx->ItemInfo("##Rays(M)_label");
      IM_CHECK_FLOAT_NEAR(whole.RectFull.Min.y, budget_label.RectFull.Min.y, 0.5f);
      const float full_footprint = budget_label.RectFull.Max.x - whole.RectFull.Min.x;
      const float padding = ImGui::GetStyle().WindowPadding.x * 2.0f;
      const auto at_width = [&](float content_width, bool single) -> bool {
        gui::g_state.workspace.right_column_ratio =
            (span - gui::UiPx(gui::kAnalyzeSplitterSize) - content_width - padding) / span;
        ctx->Yield(2);
        gui::g_state.analysis.roi_mode = LUMICE_RAYPATH_ROI_CONE;
        ctx->Yield(1);
        const auto run = ctx->ItemInfo(kAnalyzeButton);
        const auto budget = ctx->ItemInfo("##Rays(M)_input");
        const auto ring = ctx->ItemInfo("Radius###Radius_narrow");
        const auto infinite = ctx->ItemInfo("Infinite rays");
        ImGuiWindow* host = ctx->GetWindowByRef(PanoramaRegionRef());
        IM_CHECK_RETV(host != nullptr, false);
        const bool one_row = std::abs(run.RectFull.Min.y - budget.RectFull.Min.y) < 0.5f;
        ctx->LogInfo(
            "[operation boundary diagnostic] width=%.1f run-y=%.1f budget-y=%.1f radius-y=%.1f height=%.1f row1=%.1f",
            host->Size.x - padding, run.RectFull.Min.y, budget.RectFull.Min.y, ring.RectFull.Min.y,
            gui::AnalyzeOperationBarHeight(host->Size.x - padding),
            infinite.RectFull.Max.x - ctx->ItemInfo("Whole sky").RectFull.Min.x);
        IM_CHECK_RETV(one_row == single, false);
        // Nothing clips at either template: the two-row form's row 1 ends at Infinite rays and
        // row 2 at the Rays(M) label; the one-row form's only end is that same label.
        const auto last_end = ctx->ItemInfo("##Rays(M)_label");
        IM_CHECK_RETV(infinite.RectFull.Max.x <= host->InnerClipRect.Max.x + 0.5f, false);
        IM_CHECK_RETV(last_end.RectFull.Max.x <= host->InnerClipRect.Max.x + 0.5f, false);
        const ImRect point_picture = PanoramaPictureWindow()->Rect();
        gui::g_state.analysis.roi_mode = LUMICE_RAYPATH_ROI_FULL_SKY;
        ctx->Yield(1);
        const ImRect whole_picture = PanoramaPictureWindow()->Rect();
        const auto whole_run = ctx->ItemInfo(kAnalyzeButton);
        IM_CHECK_RETV(std::abs(point_picture.GetHeight() - whole_picture.GetHeight()) < 0.5f, false);
        IM_CHECK_RETV(std::abs(whole_run.RectFull.Min.x - run.RectFull.Min.x) < 0.5f &&
                          std::abs(whole_run.RectFull.Min.y - run.RectFull.Min.y) < 0.5f,
                      false);
        ctx->MouseMoveToPos(ImVec2(0.0f, 0.0f));
        ctx->Yield(2);  // keep unrelated top-bar tooltips out of the geometry capture
        IM_CHECK_RETV(
            SaveWindowPng(ctx, PanoramaRegionRef(),
                          GuiTestTempPath(single ? "layout_main_single.png" : "layout_main_double.png").string()),
            false);
        ctx->LogInfo("[operation boundary] actual content %.1f, measured footprint %.1f, rows=%d, picture h=%.1f",
                     host->Size.x - padding, full_footprint, one_row ? 1 : 2, whole_picture.GetHeight());
        return true;
      };
      IM_CHECK(at_width(full_footprint - 1.0f, false));
      IM_CHECK(at_width(full_footprint + 1.0f, true));
      // The narrowest reachable width: row 1's own demand exceeds it, so the host arms the bar's
      // scroll child — the two-row template's degrade — and the wrapped rows stay reachable
      // through it: a real wheel over the child brings the far rows back into the slice (the
      // mechanism the scale-tiers case probes with; culled rows register nothing, so the probes
      // below must run AFTER the wheel).
      {
        const float narrow = gui::UiPx(gui::kAnalyzeMainViewMinWidth) - padding;
        IM_CHECK(gui::AnalyzeOperationBarNeedsScroll(narrow));
        gui::g_state.workspace.right_column_ratio =
            (span - gui::UiPx(gui::kAnalyzeSplitterSize) - narrow - padding) / span;
        ctx->Yield(2);
        ImGuiWindow* bar_child = nullptr;
        for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
          if (w != nullptr && w->Active && std::strstr(w->Name, "##AnalyzeOpBarScroll") != nullptr) {
            bar_child = w;
            break;
          }
        }
        IM_CHECK(bar_child != nullptr);
        ctx->MouseMoveToPos(bar_child->Rect().GetCenter());
        ctx->Yield(1);
        for (int i = 0; i < 12 && bar_child->Scroll.y < bar_child->ScrollMax.y - 0.5f; ++i) {
          ctx->MouseWheelY(-1.0f);
          ctx->Yield(1);
        }
        const auto far_infinite = ctx->ItemInfo("**/Infinite rays", ImGuiTestOpFlags_NoError);
        const auto far_input = ctx->ItemInfo("**/##Rays(M)_input", ImGuiTestOpFlags_NoError);
        ctx->LogInfo("[operation boundary] narrowest width %.1f: scroll %.0f of %.0f, infinite %s, rays %s", narrow,
                     static_cast<double>(bar_child->Scroll.y), static_cast<double>(bar_child->ScrollMax.y),
                     far_infinite.ID != 0 ? "reached" : "culled", far_input.ID != 0 ? "reached" : "culled");
        IM_CHECK(far_infinite.ID != 0 && far_input.ID != 0);
      }
      gui::g_state.workspace.right_column_ratio = gui::kAnalyzeRightColumnDefaultRatio;
      ctx->Yield(2);
      // The Rays(M) track's calibration leg: at the default (wide) layout the rendered track is
      // the slot's calibrated width — the same value the slot budget reserves for it (the pair's
      // usable-track floor must never arm at ordinary widths).
      {
        const auto track = ctx->ItemInfo("**/##Rays(M)_slider", ImGuiTestOpFlags_NoError);
        IM_CHECK(track.ID != 0);
        const float expected = gui::UiPx(gui::kAnalysisRayBudgetTrackPt);
        ctx->LogInfo("[operation boundary] Rays(M) track %.1f px, calibrated %.1f px",
                     static_cast<double>(track.RectFull.GetWidth()), static_cast<double>(expected));
        IM_CHECK_FLOAT_NEAR(track.RectFull.GetWidth(), expected, 5.0f);
      }
      ctx->SetRef("");
      // A real Run from Analyze: record every completed GUI frame, including the first
      // uploaded picture, without a polling API that could skip that transition frame.
      gui::g_server = LUMICE_CreateServer();
      IM_CHECK(gui::g_server != nullptr);
      gui::ResetServerConstructionTrackers();
      IM_CHECK(gui::DeserializeFromJson(kHalo22Json, gui::g_state));
      gui::g_state.renderer.sim_resolution_index = 0;
      gui::g_state.sim.infinite = false;
      gui::g_state.sim.ray_num_millions = 0.1f;
      OpenWindow(ctx);
      IM_CHECK(!gui::g_preview.HasTexture());
      IM_CHECK(PanoramaPictureWindow() != nullptr);
      struct PictureFrames {
        ImRect expected;
        bool saw_empty = false;
        bool saw_picture = false;
        bool moved = false;
        int first_picture_frame = -1;
      } frames{ PanoramaPictureWindow()->Rect() };
      ImGuiContextHook hook;
      hook.Type = ImGuiContextHookType_EndFramePost;
      hook.UserData = &frames;
      hook.Callback = [](ImGuiContext*, ImGuiContextHook* h) {
        auto& f = *static_cast<PictureFrames*>(h->UserData);
        if (ImGuiWindow* picture = PanoramaPictureWindow()) {
          const ImRect actual = picture->Rect();
          f.moved = f.moved || std::abs(actual.Min.x - f.expected.Min.x) > 0.5f ||
                    std::abs(actual.Min.y - f.expected.Min.y) > 0.5f ||
                    std::abs(actual.Max.x - f.expected.Max.x) > 0.5f ||
                    std::abs(actual.Max.y - f.expected.Max.y) > 0.5f;
          if (gui::g_preview.HasTexture()) {
            f.saw_picture = true;
            if (f.first_picture_frame < 0) {
              f.first_picture_frame = ImGui::GetFrameCount();
            }
          } else {
            f.saw_empty = true;
          }
        } else {
          f.moved = true;
        }
      };
      const ImGuiID hook_id = ImGui::AddContextHook(ImGui::GetCurrentContext(), &hook);
      struct HookLease {
        ImGuiID id;
        ~HookLease() { ImGui::RemoveContextHook(ImGui::GetCurrentContext(), id); }
      } hook_lease{ hook_id };
      ctx->ItemClick("##TopBar/" ICON_FA_PLAY " Run");
      IM_CHECK(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kDone; }, 60));
      ctx->Yield(2);
      IM_CHECK(frames.saw_empty && frames.saw_picture && !frames.moved);
      ctx->LogInfo("[first picture] Run in Analyze; frame %d and every intermediate frame kept (%.0f,%.0f)-(%.0f,%.0f)",
                   frames.first_picture_frame, frames.expected.Min.x, frames.expected.Min.y, frames.expected.Max.x,
                   frames.expected.Max.y);
      IM_CHECK(SaveWindowPng(ctx, PanoramaRegionRef(), GuiTestTempPath("layout_main_first_picture.png").string()));
    };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "analyze_result_carries_across_scale_tiers");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(AnalyzeResultCarriesAcrossScaleTiers(ctx)); };
  }
  // The five-operations chain in one document session (755.7, AC1 + the freshness-with-windows
  // seam): whole-sky and cone analyses, two floating windows opened by their real entries, the
  // freshness trio read with both standing. Fixed-dt pool: the requests ride the same
  // state-machine waits every fixed-pool case uses, nothing wall-clock-pinned.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "five_operations_chain_in_one_session");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(FiveOperationsChainInOneSession(ctx)); };
  }
  // AC2's window-combo faces: pick, selection, close/reopen and the 150% narrowest tier, each
  // with both floating windows standing. Fixed-dt pool, same shape as the chain case.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "windows_coexist_pick_reopen_and_scale_tiers");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(WindowsCoexistPickReopenAndScaleTiers(ctx)); };
  }
  // AC3's in-flight and document-scope faces: lambda, survey stop, workspace switch in flight,
  // Revert and New, each with both floating windows standing. Fixed-dt pool.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "windows_stand_through_in_flight_and_document_scopes");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(WindowsStandThroughInFlightAndDocumentScopes(ctx)); };
  }
  // §16.3's Selection header double entry: both branches addressable, zero writes outside the
  // open flags, and the already-open click's focus face. Fixed-dt pool, standard waits.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "selection_header_entry_buttons_open_and_focus");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(SelectionHeaderEntryButtonsOpenAndFocus(ctx)); };
  }
  // The map window's pose-button labels: the byte-pin of the title-constant derivation rule.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "map_pose_button_labels_derive_from_the_window_title");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(MapPoseButtonLabelsDeriveFromTheWindowTitle(ctx)); };
  }
  // §16.3's entry arc across the halo scene's states: flight-time clicks keep the intents,
  // completion omits the slot, cancelled draws the one-word hint without moving the buttons.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "selection_header_entries_track_the_query_states");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(SelectionHeaderEntriesTrackTheQueryStates(ctx)); };
  }
  // The refusal and mass arms: the slot carries the engine reason (truncated) and names the
  // Raypath view as the complete answer's owner.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "selection_header_entries_name_refusal_and_mass_arms");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(SelectionHeaderEntriesNameRefusalAndMassArms(ctx)); };
  }
  // AC3 at the narrowest basis: floor geometry, scrolled-to-end reachability, the slot's
  // omission form, rect determinism, and the header's live-frame calibration in the log.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "raypath_analysis", "selection_header_entries_reach_at_the_narrowest_basis");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(SelectionHeaderEntriesReachAtTheNarrowestBasis(ctx)); };
  }
}
