#ifndef INCLUDE_SERVER_H_
#define INCLUDE_SERVER_H_

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "config/color_class_table.hpp"
#include "core/backend/backend_kind.hpp"
#include "core/def.hpp"  // ColorDegradeCounts (task-color-degrade-gui-surfacing)
#include "server/component_compositor.hpp"
#include "server/result_types.hpp"  // Error, the Result types, ResultFrame
#include "util/logger.hpp"


namespace lumice {

struct ConfigManager;

// The one owner of "a JSON document becomes a ConfigManager, or a return code". Every entry point
// that takes a scene document parses through here — the server's CommitConfig (a render) and
// StartRaypathAnalysis (an analysis), and the C API's LUMICE_AnalyzeSinglePath — so the four
// failure shapes map onto the Error vocabulary in exactly one place: nlohmann::json::out_of_range
// → MissingField, any other json exception → InvalidJson, std::exception → InvalidConfig, anything
// else → InvalidConfig. `validate` runs inside the same try on the parsed document (CommitConfig
// builds its colour tables there, which throw std::invalid_argument on a config error); nullptr for
// no extra step. On failure `*out` is untouched: the parse lands in a local first and is only
// moved into `out` once every step has passed. `caller` prefixes the line logged to `logger` so
// the entry points stay distinguishable in a log.
Error ParseConfigManager(const nlohmann::json& config_json, const char* caller, Logger& logger,
                         const std::function<void(const ConfigManager&)>& validate, ConfigManager* out);

// =============== GPU route query ===============
/**
 * @brief Would a server built with @p preferred_backend take the GPU single-engine route?
 * @details Single source of truth for the routing decision (env `LUMICE_TRACE_BACKEND`
 *          override wins over @p preferred_backend, CUDA gated on device availability),
 *          mirroring CreateBackend. `ServerImpl`'s constructor uses the same function to
 *          decide `worker_count` (GPU route => 1). Callers outside the server (e.g. the CLI
 *          `Lumice benchmark` dual-pass, which must skip the meaningless "single" warmup pass for
 *          the single-engine GPU route) should query this rather than re-deriving the logic.
 */
// `force_cpu` is the analysis run's session property (Server::StartRaypathAnalysis): when
// true the answer is CPU before the env override or the preference is even consulted,
// mirroring CreateBackend's `force_cpu` so the two routing decisions cannot split.
bool ResolveGpuRoute(BackendKind preferred_backend, Logger& logger, bool force_cpu = false);

/**
 * @brief per-batch dispatch grain `GenerateScene` should actually use.
 * @details `ResolveGpuRoute` answers a *preference*; whether the Simulator on the other
 *          side of the queue is still driving a TraceBackend is a separate, runtime fact
 *          (Simulator::BackendActive()). When the two disagree — GPU route selected, no
 *          backend running — the legacy CPU path is being fed GPU-sized batches, and that
 *          path samples ONE host wavelength per batch, so a 262144-ray batch paints the
 *          frame with a single wavelength. Shrink to the legacy grain in exactly that case;
 *          leave every other case (including a GPU route that is healthy) byte-identical.
 *          Pure — no lock, no I/O, no thread — so the judgement is unit-testable without a
 *          real backend. Parameter order: the NOMINAL cap first, the fallback cap second
 *          (both `size_t`; the compiler cannot catch a swap).
 * @param gpu_route      ResolveGpuRoute's verdict for this server.
 * @param backend_active Simulator::BackendActive() for the simulator consuming these batches.
 * @param nominal_cap    Grain the route asked for (env override, or the per-backend default).
 * @param fallback_cap   Grain the legacy CPU path is sized for.
 * @return `min(nominal_cap, fallback_cap)` iff the route is GPU and the backend is gone;
 *         `nominal_cap` otherwise. Never raises the grain (an explicit
 *         `LUMICE_DISPATCH_RAY_NUM` smaller than the legacy default stays honoured).
 */
size_t EffectiveDispatchCap(bool gpu_route, bool backend_active, size_t nominal_cap, size_t fallback_cap);

// =============== Server Status ===============
/**
 * @brief Server status enumeration
 */
enum class ServerStatus {
  kIdle,     ///< Idle (completed or not started)
  kRunning,  ///< Running (processing)
  kError     ///< Error state
};

// =============== Simulation Lifecycle ===============
/**
 * @brief Explicit single-source simulation lifecycle truth.
 * @details Replaces the historical side-signal disambiguation (bare kIdle +
 *          has_ever_consumed_ + stats>0). Derived in one place
 *          (ServerImpl::GetSimLifecycle); ServerStatus / QueryServerState become
 *          projections of it. See doc/gui-preview-lifecycle-architecture.md §4/§5.
 *          - kCompleted = a finite run drained clean (includes the zero-output /
 *            all-filter-rejected convergence, capi-lifecycle §3.6).
 *          - kIdle      = never run, or reset (post-Stop) with no data consumed.
 *          - kRunning   = pending work / workers active. Infinite runs stay here
 *            forever (never kCompleted); Stop returns them to kIdle.
 */
enum class SimLifecycle {
  kIdle,       ///< Not run, or reset and not yet consumed (incl. after Stop)
  kRunning,    ///< Pending work / workers active
  kCompleted,  ///< Finite run drained clean (incl. zero-output convergence)
};

// =============== Session Kind ===============
/**
 * @brief Which kind of run the CURRENT session is.
 * @details Orthogonal to SimLifecycle (whether that session is running) and to
 *          BackendKind (where it runs). Written by exactly the two entry points that
 *          (re)start a run — CommitConfig (→ kRender) and StartRaypathAnalysis
 *          (→ kAnalysis) — and by nothing else; a Stop() leaves it alone, so a stopped
 *          analysis still reads kAnalysis until the next CommitConfig. Read back via
 *          Server::GetSessionKind(). This is the one authority on "was the previous
 *          session an analysis": CommitConfig's consumer-reuse decision reads it, and
 *          a client that wants to predict that decision must read the same value rather
 *          than keep a shadow of its own.
 */
enum class SessionKind {
  kRender,    ///< A render run (CommitConfig); the default before any run
  kAnalysis,  ///< A raypath-analysis run (StartRaypathAnalysis)
};

// =============== Server ===============
class ServerImpl;

/**
 * @brief Server interface for ice halo simulation
 * @details The Server class provides a high-level interface for running ice halo simulations.
 *          It uses a server-consumer architecture with multi-threaded processing.
 *          The server starts running immediately after construction.
 */
class Server {
 public:
  /**
   * @brief Construct a new Server with default worker count
   * @note The server starts running immediately after construction
   */
  Server();

  /**
   * @brief Construct a new Server
   * @param num_workers CPU worker count (0 = automatic: the physical core count on Linux/macOS,
   *        the full logical core count on Windows, each capped per platform — see
   *        ServerImpl::AutomaticWorkerBaseAndCap() in server.cpp). A value > 0 is honoured verbatim, above that
   *        cap included. On the CPU route these are the render workers, which also run an
   *        analysis. On the GPU/Metal/CUDA route the render engine is always a single
   *        Simulator (doc/gpu-single-engine-implementation.md) and this count sizes the standing
   *        CPU analysis pool instead —
   *        the workers a StartRaypathAnalysis session runs on. The route is fixed at
   *        construction (see preferred_backend).
   * @param sim_seed Deterministic RNG seed. 0 = random; non-zero clamps the CPU group (render
   *        workers on the CPU route, the analysis pool on the GPU route) to 1 worker.
   * @param preferred_backend BackendKind::kCpu (multi-worker), kMetal (single engine)
   *        or kCuda (future). An env LUMICE_TRACE_BACKEND override takes precedence.
   *        The GUI reconstructs the server when the backend selection changes.
   * @note The server starts running immediately after construction
   */
  explicit Server(int num_workers, uint32_t sim_seed = 0, BackendKind preferred_backend = BackendKind::kCpu);

  /**
   * @brief Commit configuration from string
   * @param config_str JSON configuration string
   * @return Error object indicating success or failure
   * @note The configuration format should follow V3 configuration schema
   * @see configuration.md for configuration format details
   * @example
   *   auto err = server.CommitConfig(config_str);
   *   if (err) {  // Check if error occurred
   *     std::cerr << "Error: " << err.message << std::endl;
   *     return;
   *   }
   */
  Error CommitConfig(const std::string& config_str);

  /**
   * @brief Commit configuration from parsed JSON object (skips string parse overhead)
   * @param config_json Parsed JSON object
   * @return Error object indicating success or failure
   */
  Error CommitConfig(const nlohmann::json& config_json, bool* out_reused = nullptr);

  /**
   * @brief Start an ANALYSIS run — the other kind of run this server's one lifecycle
   *        carries (doc/raypath-analysis-panel.md §3): no image, a histogram of
   *        complete raypath chains over the committed scene.
   * @details Same Stop → rebuild consumers → Start sequence as CommitConfig, on the
   *          scene already committed; only the consumer set differs (a
   *          RaypathHistogramConsumer + a StatsConsumer). Three session properties
   *          follow from it and hold until the next successful CommitConfig, which
   *          switches the server back to a render session:
   *          - every Simulator carries chain ids (SetAnalysisChainId);
   *          - the trace backend is forced to the legacy CPU path regardless of
   *            SetPreferredBackend and LUMICE_TRACE_BACKEND (SetAnalysisForceCpu); the
   *            preference itself is left untouched, so a later render session still
   *            honours it;
   *          - the run ends on its ray budget (RaypathAnalysisRequest::ray_num_) or on
   *            Stop(), in every ROI mode; a Stop() publishes the histogram accumulated up
   *            to it before resetting, so the partial result stays readable through
   *            AcquireResultFrame() (the run then reads as kIdle, not kCompleted).
   *          The result is read through AcquireResultFrame():
   *          ResultFrame::raypath_histogram_result_.
   * @param scene_json The document to analyse, in the same JSON grammar CommitConfig takes
   *        (crystal / filter / scene / render). Parsed before anything is stopped: a document
   *        this call rejects changes nothing.
   * @param request The ROI and the ray budget.
   * @return Error::ServerError when a RENDER run is in progress (GetSimLifecycle() ==
   *         kRunning in a render session): AC1 of the analysis run — the caller must Stop()
   *         first or wait for the render to complete; this call never interrupts it silently.
   *         Calling it while an ANALYSIS run is in progress is allowed and restarts the
   *         analysis with the new request and scene. A rejected `scene_json` returns what
   *         CommitConfig would return for it — MissingField / InvalidJson / InvalidConfig.
   */
  Error StartRaypathAnalysis(const nlohmann::json& scene_json, const RaypathAnalysisRequest& request);

  /**
   * @brief Continue the committed render: trace `additional_ray_num` more rays INTO the
   *        accumulation the last run left behind, instead of starting a new one.
   * @details The one run start that is not a reset. Nothing CommitConfig resets is touched —
   *          the render planes, the emitted-energy and ray-count totals, the exposure anchor,
   *          the adaptive ray-allocation tally — so every quantity read afterwards describes
   *          the previous rays and the new ones together. The new rays are a fresh random
   *          stream: each continuation hands the workers a seed of its own
   *          (Simulator::SetContinuationIndex), which is what keeps a fixed-seed server from
   *          tracing the same rays a second time. It does advance the lifecycle epoch, like
   *          every other run start: the drain signal and "is this frame of the current run"
   *          are both keyed on it, and an unchanged epoch would read as already drained.
   *          Works from either way a render run ends — completed on its budget, or Stop()ped.
   * @param additional_ray_num Rays to add, total across wavelengths (the same unit as the
   *        scene's ray_num); kInfSize runs until Stop().
   * @return Error::ServerError when there is nothing to continue — the current session is an
   *         analysis, no render was ever committed, a run is in progress, or a just-completed
   *         run's last batches did not finish draining within an internal bound (retry shortly:
   *         this is "no traced ray is dropped" failing safe rather than silently, code review
   *         round 1 Major #1); Error::InvalidValue for a zero budget. A rejected call changes
   *         nothing.
   * @note Same implicit rule as CommitConfig: this mutates session state
   *       (continuation_serial_, active_scene_, scene_generation_) with no internal
   *       serialization of its own, so the caller must not invoke this concurrently with another
   *       call to ContinueRun or CommitConfig (code review round 2, Minor #1) — exactly the
   *       existing single-writer assumption every C API mutator here already relies on.
   */
  Error ContinueRun(size_t additional_ray_num);

  /**
   * @brief The trace backend this server's Simulator ACTUALLY runs on, as opposed to the
   *        one SetPreferredBackend asked for.
   * @details The two differ in exactly two situations, and this is the read that makes
   *          both observable: an analysis session (always kCpu — the session forces it,
   *          structurally, so the answer does not wait for Run() to re-enter), and a GPU
   *          route whose backend was lost or never obtained (see BackendFellBack). In a
   *          render session it is the value the Simulator published at its last Run()
   *          entry, kCpu before any run. Cheap; safe to poll.
   */
  BackendKind GetActiveBackend() const;

  /**
   * @brief Acquire a share of the most recent result frame.
   * @return Never null. Materializes a pending snapshot first, so the frame is as fresh
   *         as the getters it replaces; an empty frame before the first snapshot (or on a
   *         terminated server) reads as "no results".
   * @note This is THE result entry point: every kind of result (mono render, composite,
   *       raw XYZ, stats) comes off one frame, so any two of them are same-generation by
   *       construction. The returned pointers stay valid for as long as the caller keeps
   *       its share — later snapshots cannot disturb them.
   */
  std::shared_ptr<const ResultFrame> AcquireResultFrame();

  /**
   * @brief Cheap O(1) live accumulated sim ray count (no snapshot / no render).
   * @return Running StatsConsumer ray count; 0 if none. For progress polling
   *         (e.g. the `Lumice benchmark` drain loop) that needs sim_ray_num every
   *         iteration but not a rendered image. See task-317.
   */
  size_t GetLiveSimRayCount();

  /**
   * @brief Stop the server
   * @note Stops processing but keeps the server alive. Can be restarted by committing new config.
   * @note No Run() method exists because server starts running immediately after construction.
   */
  void Stop();

  /**
   * @brief Terminate the server
   * @note Stops processing and prepares for destruction. Server cannot be used after termination.
   */
  void Terminate();

  void SetLogLevel(LogLevel level);

  /**
   * @brief Set preferred trace backend for this server.
   * @param backend 0 = CPU (default), 1 = Metal (Apple-only; silent CPU
   *                fallback elsewhere or when the active config is not
   *                backend-compatible). Mirrors the public C API constants
   *                LUMICE_BACKEND_CPU / LUMICE_BACKEND_METAL.
   * @note Takes effect on the next Simulator::Run() entry (after CommitConfig
   *       restart). env-var LUMICE_TRACE_BACKEND, when set, overrides this.
   */
  void SetPreferredBackend(BackendKind backend);

  /**
   * @brief Get server status
   * @return Current server status
   */
  ServerStatus GetStatus() const;

  /**
   * @brief Get the explicit simulation lifecycle (single-source truth).
   * @return kRunning / kCompleted / kIdle (see SimLifecycle).
   * @note Authoritative derivation; GetStatus()/QueryServerState are projections.
   */
  SimLifecycle GetSimLifecycle() const;

  /**
   * @brief Which kind of run the current session is (see SessionKind).
   * @return kRender before any run, after every CommitConfig, and on a terminated server;
   *         kAnalysis from StartRaypathAnalysis until the next CommitConfig — a Stop()
   *         does not reset it. Cheap O(1) atomic read, same shape as GetActiveBackend().
   * @note Read it BEFORE the CommitConfig whose reuse decision you want to predict: that
   *       commit is what flips it back to kRender, so the value read after it no longer
   *       says what the previous session was.
   */
  SessionKind GetSessionKind() const;

  /**
   * @brief Current lifecycle epoch (monotonic generation counter).
   * @return committed_epoch_; ++ on each reset-causing CommitConfig. 0 before any
   *         successful commit. Read back after a synchronous commit to learn the
   *         just-minted epoch (no commit-signature change; see plan §2 decision 3).
   */
  uint64_t CommittedEpoch() const;

  /**
   * @brief Highest epoch the CONSUMER has fully drained.
   * @return drained_epoch_; the current epoch is drained iff this equals
   *         CommittedEpoch(). Cheap O(1) atomic read — no snapshot, no lock.
   * @note Deliberately separate from GetStatus()/GetSimLifecycle(), whose kIdle
   *       verdict is derived from producer-side predicates only and therefore
   *       does NOT imply that the accumulators hold this epoch's final totals.
   *       Never reset: a new epoch outruns it, so the equality test reads
   *       "not drained yet" without a reset site anyone could forget.
   */
  uint64_t DrainedEpoch() const;

  /**
   * @brief Check if server is idle
   * @return true if server is idle (no processing), false if processing
   * @note Convenience method, equivalent to GetStatus() == ServerStatus::kIdle
   */
  bool IsIdle() const;

  /**
   * @brief Display-time update of the committed color classes' appearance (task-342.2).
   * @param classes     Per-class appearance patch (color, visible, solo).
   * @param class_count Must equal the currently active color-class count; mismatch =
   *                    Error::InvalidConfig (caller must re-commit the config to change
   *                    member structure).
   * @param z_order     Optional (nullptr = leave unchanged). When non-null, must be a
   *                    permutation of [0, class_count): z_order[i] is the new drawing rank of
   *                    class i (compositor sorts ascending — lower rank / rank 0 = drawn first,
   *                    hence on top / wins painter and dominant ties). A non-permutation returns
   *                    Error::InvalidConfig.
   * @param mode        Composite mode (dominant/additive/painter).
   * @return Error::Success on success. Never restarts the simulation — accumulator, epoch,
   *         and consumers stay put. Only the next acquired result frame re-composites.
   */
  Error SetRaypathColors(const ColorClassDisplay* classes, int class_count, const int* z_order, CompositeMode mode);

  /**
   * @brief Per-color-class empty-arc detector (task-342.3 AC4).
   * @param out_flags   Caller-owned buffer of length class_count. On success, each byte is set
   *                    to 1 iff the corresponding class has any non-zero pixel in its snapshot
   *                    Y-lane on any active RenderConsumer, 0 otherwise.
   * @param class_count Must equal the currently active color-class count; mismatch =
   *                    Error::InvalidConfig.
   * @return Error::Success on success. Reads the frozen snapshot (no DoSnapshot trigger);
   *         callers relying on freshness should acquire a result frame first.
   *         O(W*H * class_count * consumers) scan; intended for infrequent GUI polls
   *         (commit-debounce cadence), not per-render-frame.
   */
  Error GetColorClassSignals(uint8_t* out_flags, int class_count);

  /**
   * @brief task-gui-feedback-affordances Step 5 (AC1): number of color predicates
   *        that hit `kNoBit` in the most recent BuildColorGateTable call
   *        (component-bit budget, kMaxBits=64, exhausted). Written synchronously
   *        inside CommitConfig; read by LUMICE_GetColorOverflowInfo so the GUI
   *        DoRun path can pop a "coloring degraded" modal without waiting for
   *        the first backend batch to land.
   * @return 0 iff no predicate was dropped; else the drop count.
   */
  size_t GetLastColorComponentOverflowCount() const;

  /**
   * @brief task-color-degrade-gui-surfacing: GPU-only raypath-color drop tally
   *        (symmetry-group / OR-summand / color-class device caps). Unlike the
   *        component count above, these fire on the backend's FIRST batch and
   *        are published ASYNCHRONOUSLY via ConsumeData, so the GUI polls this
   *        each tick (LUMICE_GetColorOverflowInfo) rather than reading it once
   *        at DoRun. Reset to zero synchronously on CommitConfig. CPU backend
   *        has no such caps and always reports all-zeros.
   * @return all-zero iff nothing was dropped; else the per-cap drop counts.
   */
  ColorDegradeCounts GetLastColorDegradeCounts() const;

  /**
   * @brief Has the GPU single-engine route lost its TraceBackend mid-run?
   * @details True once this server's one Simulator has dropped its backend for the rest
   *          of the current Run() (a BackendUnavailableError), or never obtained
   *          one despite the GPU route being selected. Always false on the CPU route —
   *          there is no backend to lose — and false again after the next Start(), which
   *          re-enters Run() and re-resolves the backend. Like GetLastColorDegradeCounts
   *          this is discovered asynchronously by the worker, so the GUI polls it
   *          (LUMICE_GetBackendFallbackFlag); the fallback otherwise surfaces only as a
   *          core WARN log line. Cheap point read — safe to call every GUI tick.
   * @return true iff the GPU route is currently running on the legacy CPU path.
   */
  bool BackendFellBack() const;

  /**
   * @brief task-345.3: display-time EV multiplier for the composite path only.
   * @param ev_total Total EV (manual + auto) to apply as 2^ev_total inside DoSnapshot Phase 2.
   * @return Error::Success. Mono path is untouched — only the composite result carries the
   *         resulting brightness change. Flips snapshot_dirty_ so the next acquired result frame
   *         re-bakes the composite; no epoch bump, no accumulator reset.
   */
  Error SetCompositeExposure(float ev_total);

  /**
   * @brief Display-time background colour for the composite path only.
   * @param rgb Three ADDITIVE linear-RGB components, added inside DoSnapshot Phase 2 to every
   *        pixel the lens actually images (RenderConsumer's per-pixel domain mask — the same one
   *        the mono path's background honours, so the two paths agree outside the image circle).
   * @return Error::Success. Mono path is untouched — it keeps taking its background from the
   *         committed RenderConfig; only the composite result carries this one. Flips
   *         snapshot_dirty_ so the next acquired result frame re-bakes the composite; no epoch
   *         bump, no accumulator reset.
   */
  Error SetCompositeBackground(const float rgb[3]);

 private:
  std::shared_ptr<ServerImpl> impl_;
};

}  // namespace lumice

#endif  // INCLUDE_SERVER_H_
