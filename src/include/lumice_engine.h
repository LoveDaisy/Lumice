#ifndef LUMICE_ENGINE_H_
#define LUMICE_ENGINE_H_

// The simulation engine: a server's lifecycle, committing a scene, result frames and their
// readers, the raypath analysis run, and trace-backend selection.

#include "lumice_base.h"
#include "lumice_render.h"
#include "lumice_scene.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============== Server State ===============
typedef enum LUMICE_ServerState_ {
  LUMICE_SERVER_IDLE,
  LUMICE_SERVER_RUNNING,
  LUMICE_SERVER_NOT_READY,
} LUMICE_ServerState;

// =============== Simulation Lifecycle ===============
// Explicit single-source lifecycle truth (replaces disambiguation via bare
// LUMICE_SERVER_IDLE + has_valid_data + stats>0). LUMICE_QueryServerState is a
// projection of this enum: RUNNING -> RUNNING, IDLE|COMPLETED -> IDLE.
//   - COMPLETED = a finite run drained clean (includes zero-output / all-filter-
//     rejected convergence).
//   - IDLE      = never run, or reset (post-Stop) with no data consumed.
//   - RUNNING   = pending work / workers active. Infinite runs stay RUNNING
//     forever (never COMPLETED); Stop returns them to IDLE.
typedef enum LUMICE_SimLifecycle_ {
  LUMICE_LIFECYCLE_IDLE = 0,
  LUMICE_LIFECYCLE_RUNNING,
  LUMICE_LIFECYCLE_COMPLETED,
} LUMICE_SimLifecycle;

// Which kind of run the CURRENT session is. Orthogonal to LUMICE_SimLifecycle (whether
// that session is running) and to LUMICE_BACKEND_* (where it runs). Written only by the
// two calls that (re)start a run — LUMICE_CommitScene (-> RENDER) and
// LUMICE_StartRaypathAnalysis (-> ANALYSIS); LUMICE_StopSimulation leaves it alone, so a
// stopped analysis still reads ANALYSIS until the next commit. It is the server's own
// "was the previous session an analysis" term of the consumer-reuse decision
// LUMICE_CommitScene makes, exposed so a client predicting that decision reads the same
// value instead of keeping a shadow of it.
typedef enum LUMICE_SessionKind_ {
  LUMICE_SESSION_RENDER = 0,
  LUMICE_SESSION_ANALYSIS = 1,
} LUMICE_SessionKind;

// {lifecycle, epoch, session_kind} snapshot of the backend lifecycle truth.
//   lifecycle:    one of LUMICE_SimLifecycle.
//   epoch:        monotonic generation counter, ++ on each run start: every successful
//                 LUMICE_CommitScene, LUMICE_StartRaypathAnalysis and
//                 LUMICE_ContinueRender (the one start that resets no accumulator).
//                 0 before any successful commit. Read back after a synchronous
//                 commit to learn the just-minted epoch.
//   session_kind: one of LUMICE_SessionKind — the kind of the session that is
//                 current at the time of the call, NOT "the kind at the last
//                 commit". Sampled BEFORE a LUMICE_CommitScene it tells you what
//                 that commit is about to replace; sampled after, it is RENDER.
// The three reads are not one atomic snapshot of each other (they never were for
// the first two); each is individually current.
typedef struct LUMICE_SimLifecycleResult_ {
  int lifecycle;
  unsigned long long epoch;
  int session_kind;
} LUMICE_SimLifecycleResult;

// {drained_epoch, current_epoch} snapshot of the CONSUMER-side drain contract.
// The current epoch's data is fully drained iff drained_epoch == current_epoch.
//
// Why this is not folded into the lifecycle above: LUMICE_LIFECYCLE_COMPLETED /
// LUMICE_SERVER_IDLE are derived from producer-side predicates only (no
// simulator busy, no pending scenes, scene generation done). None of them asks
// whether the consumer thread has finished draining its queue, so the
// accumulated statistics (LUMICE_FrameGetStats) can still be a PARTIAL total at
// the instant the server first reports IDLE — measured as orientation_num 19616
// vs 20000, a whole number of dispatch grains short. A reader that needs final
// totals must wait for THIS signal, not for IDLE.
//   drained_epoch:  highest epoch whose data the consumer has fully drained.
//                   Monotonic; 0 before any epoch has drained.
//   current_epoch:  == LUMICE_SimLifecycleResult.epoch, sampled in the same call
//                   so the two are compared without a second round trip.
// An infinite run never drains (production never ends), which is the same shape
// as its lifecycle never reaching COMPLETED. Stopping a server does NOT publish
// a drain: LUMICE_StopServer discards whatever is still queued, so "stopped" is
// deliberately distinguishable from "drained".
typedef struct LUMICE_DrainResult_ {
  unsigned long long drained_epoch;
  unsigned long long current_epoch;
} LUMICE_DrainResult;

// Summary of how many raypath-color assignments the CORE dropped for the most
// recent committed config, because some capacity was exceeded. Used by the GUI
// to surface a modal warning that coloring will be truncated (the filter /
// geometry / raypath tracing path is UNAFFECTED — only color assignment
// degrades). All counts are per committed config, not cumulative across runs.
//
// Two surfacing timings, by field:
//   component_overflow_count is set SYNCHRONOUSLY inside CommitConfig (color
//     predicates past the 64-bit ComponentTable budget were assigned kNoBit).
//     Backend-independent (host-side gate table). Read once in the DoRun path.
//   The remaining three are GPU-ONLY device buffer-layout caps that fire on the
//     worker thread's FIRST batch (asynchronous), so the GUI must POLL them each
//     tick rather than read them once at DoRun. Populated via the server's
//     ConsumeData path; the CPU backend has no such caps and reports 0:
//       symmetry_group_overflow_count — kColorMaxGroupsPerSlot (per gate slot)
//       or_summand_overflow_count     — kDeviceFilterMaxOrClauses (per color group)
//       color_class_overflow_count    — kMaxColorClassesDevice (per session)
typedef struct LUMICE_ColorOverflowInfo_ {
  int component_overflow_count;
  int symmetry_group_overflow_count;
  int or_summand_overflow_count;
  int color_class_overflow_count;
} LUMICE_ColorOverflowInfo;

// =============== Result Structs ===============
typedef struct LUMICE_RenderResult_ {
  int renderer_id;
  int img_width;
  int img_height;
  const unsigned char* img_buffer;  // Read-only view into the LUMICE_ResultFrame it was obtained
                                    // from. Valid until that frame is released with
                                    // LUMICE_ReleaseResultFrame().
                                    // Sentinel: img_buffer == NULL
  // Composite-only auto-EV anchor. Populated by LUMICE_FrameGetComposite —
  // MEANINGFUL ONLY on the composite path. LUMICE_FrameGetRender (mono/full-spectrum)
  // leaves this at 0 and consumers must ignore it there. Composite path: P99 over the
  // union of NON-ZERO UNEXPOSED (raw lane) Y values across every participating color
  // class (the anchor the GUI's auto-EV feeds into ComputeEvAuto for composite display).
  // 0 on the composite path means no participating class carried any positive Y this
  // snapshot (all-black composite / all classes hidden). See doc/ev-pipeline-architecture.md
  // §2.4 for why this field is composite-only.
  float composite_p99_y;
} LUMICE_RenderResult;

typedef struct LUMICE_RawXyzResult_ {
  int renderer_id;
  int img_width;
  int img_height;
  const float* xyz_buffer;   // Read-only XYZ float data, 3 floats per pixel. A view into the
                             // LUMICE_ResultFrame it was obtained from; valid until that frame
                             // is released with LUMICE_ReleaseResultFrame().
                             // Sentinel: xyz_buffer == NULL
  float snapshot_intensity;  // Per-pixel landed intensity (landed_ray_weights / (kNormScale * total_pixels))
  float intensity_factor;    // Per-renderer intensity factor (2^EV)
  int has_valid_data;        // Non-zero once simulation has produced data (reset on CommitConfig/Stop)
  unsigned long long snapshot_generation;  // Increments on each new snapshot; compare to detect data changes
  int effective_pixels;                    // Non-zero pixel count (for stats display)
  // Total spectral energy the light source EMITTED into this snapshot: the sum over every
  // simulated batch of (per-ray emission weight x rays emitted). Raw total, NOT divided by
  // kNormScale * total_pixels the way snapshot_intensity above is.
  //
  // Distinct from snapshot_intensity in what it measures, not just in scaling: this counts
  // what went IN, that counts what came out and landed on a pixel. They differ by everything
  // that removes a ray -- filters, absorption, rays that miss the lens. This is the quantity
  // the renderer normalizes by, which is what makes its output scale absolute; a consumer can
  // reproduce that scale as
  //     scale = intensity_factor * kNormScale * total_pixels / emitted_energy
  // and two scenes normalized this way are directly comparable in brightness.
  //
  // Occupies alignment padding that already existed before `epoch`, so sizeof(LUMICE_RawXyzResult)
  // is unchanged and `epoch` keeps its offset.
  float emitted_energy;
  unsigned long long epoch;  // Lifecycle epoch at snapshot time (committed_epoch_); 1.5 display keying
  // The session's EXPOSURE ANCHOR: the P99 of the sky's radiance, in Y per steradian.
  //
  // Measured on a fixed, full-sky, equal-area buffer that this renderer's lens, fov, view
  // pose, `visible` clip and output resolution do not touch. It therefore describes the
  // SCENE, not this view of it — which is the point: a P99 taken over a renderer's own
  // output buffer moves by up to a couple of stops when you merely change the lens looking
  // at the same sky, and two consumers that each take their own such P99 disagree by a
  // constant gain that no one can see in either one alone.
  //
  // IDENTICAL ON EVERY ROW of one LUMICE_FrameGetRawXyz call, by construction. It is a
  // frame-level scalar computed exactly once per snapshot and broadcast into each row
  // because that is where a caller is already looking; it is NOT a per-renderer quantity,
  // and reading it as one (e.g. comparing rows to detect a difference) is meaningless.
  //
  // 0 means the sky carried no positive Y this snapshot (nothing simulated yet, or every
  // ray filtered away) — the same "no samples" convention every other P99 in this API uses.
  float anchor_l99_sky;
  // The solid angle, in steradians, that ONE pixel on THIS renderer's optical axis subtends.
  //
  // The unit bridge between the two quantities above it: `anchor_l99_sky` is a radiance (per
  // steradian) while `xyz_buffer` holds a radiance integrated over each pixel's own solid angle,
  // so converting between them takes exactly this factor. With it, the relative-mode scale the
  // renderer applied is
  //     scale = intensity_factor * TargetWhiteToLinear(135) / (axis_solid_angle * anchor_l99_sky)
  // which is the relative-branch counterpart of the `emitted_energy` formula documented above.
  //
  // UNLIKE `anchor_l99_sky`, this IS per-renderer: it is a property of this row's lens, fov,
  // overlap and output resolution and differs from row to row. A consumer holding a buffer of its
  // own — a preview that resamples this one, say — must use the solid angle of the buffer it is
  // actually exposing, not this row's, unless they are the same buffer.
  //
  // On-axis specifically, not an average: away from the axis the per-pixel solid angle varies by
  // the projection's relative illumination, and normalizing at the axis is what lets a viewer
  // apply that shape separately (it is 1 at the frame centre by construction).
  //
  // 0 only for a degenerate calibration (a zero-size or zero-scale view), matching the "no
  // measurement" convention of the fields above.
  float axis_solid_angle;
} LUMICE_RawXyzResult;

typedef struct LUMICE_StatsResult_ {
  LUMICE_RayCount ray_seg_num;
  LUMICE_RayCount sim_ray_num;
  LUMICE_RayCount crystal_num;  // Distinct crystal GEOMETRIES sampled this run.
  // Distinct crystal ORIENTATIONS sampled this run. A separate quantity, not a
  // rescaling of crystal_num: the shape and the axis of a crystal setting are
  // independent, and the commonest halo configuration — a fixed shape under a
  // random axis — yields one geometry and a great many orientations. Expect this
  // to be far larger than crystal_num: orientation is redrawn per ray with no
  // reuse, while geometry is reused across a batch of rays.
  //
  // NOT comparable across backends, and not usable as a parity assertion: the
  // CPU and GPU routes sample at different densities and that difference is
  // precisely what the number exposes.
  LUMICE_RayCount orientation_num;
  // Sentinel: all zeros (sim_ray_num == 0)
} LUMICE_StatsResult;

// =============== Server Configuration ===============
typedef struct LUMICE_ServerConfig_ {
  int num_workers;        // CPU worker count (0 = automatic: the physical core count on
                          // Linux/macOS, the full logical core count on Windows, each capped
                          // per platform — see ServerImpl::AutomaticWorkerBaseAndCap() in server.cpp
                          // for the values and the measurements behind them). A value > 0 is honoured verbatim and is
                          // NOT subject to that cap. On the CPU route these are the render
                          // workers (which also run an analysis). On the GPU/Metal/CUDA route
                          // the render engine stays a single Simulator (N engines would contend
                          // one GPU; see doc/gpu-single-engine-implementation.md) and this
                          // count sizes the server's standing CPU ANALYSIS POOL — the workers
                          // LUMICE_StartRaypathAnalysis runs on. BEHAVIOUR CHANGE: before that
                          // pool existed the field was ignored on the GPU route; a caller that
                          // passed a non-zero value there now gets an analysis pool of that
                          // size (the render is unaffected either way).
  unsigned int sim_seed;  // Deterministic seed for the worker RNG. 0 = random (default).
  int preferred_backend;  // LUMICE_BACKEND_CPU (0, multi-worker), LUMICE_BACKEND_METAL
                          // (1, single engine on Apple) or LUMICE_BACKEND_CUDA (2, future).
                          // Fixes the route at construction; the worker count follows it.
                          // env LUMICE_TRACE_BACKEND overrides. The GUI reconstructs the
                          // server when the backend selection changes.
} LUMICE_ServerConfig;

// =============== Server Lifecycle ===============
LUMICE_API LUMICE_Server* LUMICE_CreateServer(void);
LUMICE_API LUMICE_Server* LUMICE_CreateServerEx(const LUMICE_ServerConfig* config);
LUMICE_API void LUMICE_DestroyServer(LUMICE_Server* server);

// ---------- Commit ----------
// Commit `scene` to `server`. This is the ONE and only commit entry point of the API: v4.12
// removed the three legacy LUMICE_Config commit functions, and every configuration — however it
// was authored — reaches the core through here.
//
// Commit is DELIBERATELY decoupled from serialization: this takes a handle, never a JSON string
// or a file path. To commit JSON, first build a handle with LUMICE_SceneFromJson /
// LUMICE_SceneFromJsonFile, then pass it here.
//
// `scene` is neither consumed nor destroyed: it is read as const, the server keeps no reference
// to it, and the caller still owns it and must eventually call LUMICE_SceneDestroy. The same
// handle may be committed repeatedly (edit via Add*/Set*, commit again).
//
// `out_reused` is OPTIONAL (may be NULL). When non-NULL it receives 1 if the server reused the
// existing consumers/renderers across this commit (no renderer-layout change), 0 if they were
// rebuilt.
//
// Errors: LUMICE_ERR_NULL_ARG (NULL server or scene); otherwise whatever the core commit rejects
// the scene with — LUMICE_ERR_INVALID_CONFIG
// / _MISSING_FIELD / _INVALID_VALUE / _INVALID_JSON for a configuration the core refuses, and
// LUMICE_ERR_SERVER for a server-side failure. On any error *out_reused is left untouched. Note
// that no whole-scene re-validation happens here: each Add*/Set* call already validated its own
// input, so what this can still surface is cross-field/semantic rejection from the core.
LUMICE_API LUMICE_ErrorCode LUMICE_CommitScene(LUMICE_Server* server, const LUMICE_Scene* scene, int* out_reused);

// =============== Raypath Color Display-Time Setter ===============
// Display-time appearance of one color class (mutable without re-simulation): the RGB
// color, the visible/solo toggles. Structural fields (match[]/combine) live on
// LUMICE_ColorClass and require re-simulation to change (LUMICE_CommitScene).
// WARNING (same footgun as LUMICE_ColorClass): visible/solo are applied verbatim, so a
// zero-initialized LUMICE_ColorClassDisplay{} has visible==0 (INVISIBLE). Callers MUST set
// visible=1 explicitly for every class they want shown, or the next composite hides them.
typedef struct LUMICE_ColorClassDisplay_ {
  float color[3];
  int visible;
  int solo;
} LUMICE_ColorClassDisplay;

// Update display-time appearance of the committed color classes WITHOUT restarting the
// simulation. Colors, visibility, solo, z-order, composite mode — none of these touch the
// accumulator, the epoch, or the consumer set. The compositor re-runs on the SAME
// already-accumulated per-class Y-lanes and produces new pixel output on the next
// frame acquired with LUMICE_AcquireResultFrame.
//
// classes[i] targets committed color class i (physical index in raypath_color[]; the
// server's active class table). class_count MUST equal the current raypath_color_count of
// the committed config, otherwise LUMICE_ERR_INVALID_CONFIG is returned — a count mismatch
// signals the caller changed member structure and must re-commit the config.
//
// z_order: OPTIONAL — pass NULL to leave existing z-order unchanged. When non-NULL, z_order
// MUST be a permutation of [0, class_count): z_order[i] is the NEW drawing rank of class i
// (the ranks are the integers 0..class_count-1 in some order — the natural output of a GUI
// drag-reorder). The compositor sorts ascending, so rank 0 (the LOWEST rank) draws first and
// therefore lands on top for painter mode / wins dominant ties (first-drawn wins). A z_order
// that is not a valid
// permutation (out-of-range or duplicate rank, e.g. {0,0,1}) returns LUMICE_ERR_INVALID_CONFIG
// and leaves all state unchanged (all-or-nothing).
//
// mode: composite mode (LUMICE_COLOR_MODE_DOMINANT / _ADDITIVE / _PAINTER). Values outside
// this range return LUMICE_ERR_INVALID_VALUE.
//
// Thread safety: display-time only; safe relative to OTHER display-time readers
// (LUMICE_AcquireResultFrame, LUMICE_GetSimLifecycle, etc.). NOT thread-safe with a concurrent
// LUMICE_CommitScene — the existing single-owner commit rule
// (doc/capi-lifecycle-architecture.md §4) still applies to this setter.
LUMICE_API LUMICE_ErrorCode LUMICE_SetRaypathColors(LUMICE_Server* server, const LUMICE_ColorClassDisplay* classes,
                                                    int class_count, const int* z_order, int mode);

// Display-time EV for the composite (raypath_color) path only.
// `ev_total` is applied as `2^ev_total` inside the composite bake — a single scalar shared
// by every participating color class (per-class renormalization stays structurally excluded;
// the mono / non-composite path is unaffected).
//
// No accumulator reset / no epoch bump / no sim restart — the setter just flips the internal
// snapshot_dirty_ flag, so the next acquired result frame rebuilds the composite with the new EV.
// Callers that already keep the poller running (a live sim, or a display-time refresh triggered
// by other setters) get the new brightness on the next poll; callers that stopped the poller must
// wake it (mirrors the LUMICE_SetRaypathColors + poller-wake pattern used by the GUI).
//
// Thread safety: display-time only; safe relative to other display-time readers
// (LUMICE_AcquireResultFrame, LUMICE_GetSimLifecycle, LUMICE_SetRaypathColors, etc.). NOT
// thread-safe with concurrent LUMICE_CommitScene (same single-owner rule as the rest of the
// display-time surface).
LUMICE_API LUMICE_ErrorCode LUMICE_SetCompositeExposure(LUMICE_Server* server, float ev_total);

// Display-time background colour for the composite (raypath_color) path only. `background_linear` is a caller-owned
// array of 3 floats, ADDITIVE **linear** RGB — the same convention the render config's `background` carries internally
// (see doc/configuration.md: JSON/picker values are sRGB, the struct side is linear). A caller holding a picker's sRGB
// triple must pre-convert it with lumice::SrgbToLinearRgb (src/util/color_space.hpp, an inline header function — see
// LUMICE_XyzToSrgbUint8WithBackground for the same note) before calling. The value is added inside the composite bake
// to every pixel the lens actually images — outside the image circle, and in the hemisphere `visible` excludes, nothing
// is painted, matching what the mono path does with the committed config's background.
//
// The mono / non-composite path is unaffected: it keeps taking its background from the committed
// scene. Pushing the SAME colour through both is what makes toggling raypath colour on and off
// leave the background pixels unchanged.
//
// No accumulator reset / no epoch bump / no sim restart — the setter just flips the internal
// snapshot_dirty_ flag, so the next acquired result frame rebuilds the composite with the new
// background. Callers that already keep the poller running get it on the next poll; callers that
// stopped the poller must wake it (mirrors the LUMICE_SetCompositeExposure + poller-wake pattern
// used by the GUI). All-zero (the server default) is an algebraic no-op, so a caller that never
// calls this sees byte-identical composites.
//
// Returns LUMICE_ERR_NULL_ARG if `server` or `background_linear` is NULL.
//
// Thread safety: display-time only; safe relative to other display-time readers
// (LUMICE_AcquireResultFrame, LUMICE_GetSimLifecycle, LUMICE_SetCompositeExposure, etc.). NOT
// thread-safe with concurrent LUMICE_CommitScene (same single-owner rule as the rest of the
// display-time surface).
LUMICE_API LUMICE_ErrorCode LUMICE_SetCompositeBackground(LUMICE_Server* server, const float* background_linear);

// Per-color-class empty-arc detector. For each committed color class, reports
// whether the class has any non-zero pixel in its snapshot Y-lane on any active RenderConsumer
// — i.e. whether it has captured any rays yet. Intended for GUI empty-arc warnings when a
// physical filter has silently blocked all rays that would have matched the class predicate.
//
// out_flags is a caller-owned buffer of length class_count. On success, out_flags[i] = 1 iff
// class i has signal, 0 otherwise. class_count MUST equal the current raypath_color_count of
// the committed config, otherwise LUMICE_ERR_INVALID_CONFIG. class_count == 0 is a valid no-op
// (returns LUMICE_OK; out_flags is not touched).
//
// Reads the frozen snapshot state (no DoSnapshot trigger); callers relying on freshness should
// acquire a result frame first. O(W*H * class_count *
// consumers) scan; intended for infrequent polls (commit-debounce cadence, ~1 Hz), not per
// render frame.
LUMICE_API LUMICE_ErrorCode LUMICE_GetColorClassSignal(LUMICE_Server* server, int* out_flags, int class_count);

// =============== Results ===============
// See doc/capi-lifecycle-architecture.md §5 for sentinel contract.
// Unified pattern: (server, out, max_count) -> LUMICE_ErrorCode, sentinel-terminated.
// Sentinel is written at out[count] only when count < max_count.
// When the array is full (count == max_count), no sentinel slot is written;
// callers relying on sentinel iteration must value-initialize the array
// (e.g., Type arr[N + 1]{}) and pass max_count = N. Minimum array size:
// max_count + 1 for sentinel iteration, max_count for direct index access.

// ---------- Result frame (opaque handle) ----------
// Every result read goes through a frame. Acquire one, read whatever kinds of result you
// need out of it, release it:
//
//     LUMICE_ResultFrame* frame = NULL;
//     if (LUMICE_AcquireResultFrame(server, &frame) == LUMICE_OK) {
//       LUMICE_RawXyzResult xyz[2] = {0};
//       LUMICE_FrameGetRawXyz(frame, xyz, 1);
//       /* xyz[0].xyz_buffer stays valid until the Release below */
//       LUMICE_ReleaseResultFrame(frame);
//     }
//
// WHY a handle rather than plain getters: the buffers these results point at are owned by
// the server, and the server keeps producing new snapshots. Without a handle a reader holds
// a pointer with no share of its lifetime — the next snapshot frees or rewrites the memory
// under it, which is a use-after-free, not merely a stale read. Holding a frame is what
// makes the borrow safe, and it is why the read functions below take a frame instead of a
// server.
//
// Two frames are independent: acquiring a second one does not affect the first, and a
// caller may hold as many as it likes.
//
// Materializes a pending snapshot (like the getters it replaces), then writes a new frame
// handle to *out_frame. The caller owns that handle and MUST eventually pass it to
// LUMICE_ReleaseResultFrame. Returns LUMICE_ERR_NULL_ARG if `server` or `out_frame` is NULL.
// On success *out_frame is never NULL, even before the first snapshot — such a frame simply
// reads as "no results" (every FrameGet* writes its sentinel / all-zero struct).
LUMICE_API LUMICE_ErrorCode LUMICE_AcquireResultFrame(LUMICE_Server* server, LUMICE_ResultFrame** out_frame);

// Release a frame handle. NULL-safe no-op (same contract as LUMICE_DestroyServer). Each
// handle must be Released exactly once; releasing the same handle twice is undefined
// behavior (this mirrors LUMICE_DestroyServer and every other handle in this API — there is
// no double-free sentinel).
//
// Failure mode if you forget: the frame — and only that frame — leaks. It cannot corrupt
// memory or affect any other reader, because a frame is immutable and separately
// reference-counted; and a leak is what ASan/LSan/valgrind already report, so no
// project-specific machinery is needed to find one. After the Release, every pointer read
// out of that frame is dangling; copy what you still need first.
LUMICE_API void LUMICE_ReleaseResultFrame(LUMICE_ResultFrame* frame);

// Read functions. Same (out, max_count) array shape and same sentinel contract as the
// server-taking getters, so only the first argument differs. Any two reads from the SAME
// frame describe the same snapshot generation by construction — no separate "combined"
// getter is needed to pair them. All return LUMICE_ERR_NULL_ARG on a NULL frame/out.

// Raw XYZ float data + intensity scalars (xyz_buffer == NULL sentinel).
LUMICE_API LUMICE_ErrorCode LUMICE_FrameGetRawXyz(const LUMICE_ResultFrame* frame, LUMICE_RawXyzResult* out,
                                                  int max_count);

// Per-raypath composite sRGB images, one per colored renderer (img_buffer == NULL sentinel).
// Empty (out[0] sentinel) when no `raypath_color` is configured — the mono path below is
// unaffected. composite_p99_y is meaningful here (see its field docs).
LUMICE_API LUMICE_ErrorCode LUMICE_FrameGetComposite(const LUMICE_ResultFrame* frame, LUMICE_RenderResult* out,
                                                     int max_count);

// Mono / full-spectrum sRGB uint8 images (img_buffer == NULL sentinel). composite_p99_y is
// left at 0 on this path and must be ignored.
LUMICE_API LUMICE_ErrorCode LUMICE_FrameGetRender(const LUMICE_ResultFrame* frame, LUMICE_RenderResult* out,
                                                  int max_count);

// Simulation statistics. Single value, so no max_count: writes an all-zero struct when the
// frame carries no stats (e.g. acquired before the first snapshot).
LUMICE_API LUMICE_ErrorCode LUMICE_FrameGetStats(const LUMICE_ResultFrame* frame, LUMICE_StatsResult* out);

// Cheap O(1) live accumulated sim ray count — no snapshot, no render, no XYZ copy.
// For progress polling (e.g. the --benchmark drain loop) that needs sim_ray_num
// every iteration but not a rendered image. Unlike acquiring a result frame, this
// does NOT trigger DoSnapshot/PostSnapshot, and it reads the running counter
// directly, so it needs no external snapshot driver to stay fresh.
// Writes 0 if no StatsConsumer (or none produced yet).
LUMICE_API LUMICE_ErrorCode LUMICE_GetSimRayCount(LUMICE_Server* server, LUMICE_RayCount* out);

// =============== State & Control ===============
LUMICE_API LUMICE_ErrorCode LUMICE_QueryServerState(LUMICE_Server* server, LUMICE_ServerState* out);

// Read the explicit simulation lifecycle + current epoch + session kind (single-source
// truth). LUMICE_QueryServerState is a projection of this. After a synchronous commit,
// call this to read back the just-minted epoch (no commit-signature change). Before a
// commit, `session_kind` tells you whether the session it replaces was an analysis — the
// term of the consumer-reuse decision a caller cannot otherwise observe.
LUMICE_API LUMICE_ErrorCode LUMICE_GetSimLifecycle(LUMICE_Server* server, LUMICE_SimLifecycleResult* out);

// Read the consumer-side drain status (see LUMICE_DrainResult for the contract).
// Cheap O(1) atomic read — same cost class as LUMICE_GetSimRayCount: no snapshot,
// no render, no lock. Safe to call every poll iteration.
//
// Intended use: a reader that wants FINAL accumulated totals polls until
//   out.drained_epoch == out.current_epoch
// and only then acquires a result frame. Waiting for LUMICE_SERVER_IDLE alone is
// not sufficient and never was.
LUMICE_API LUMICE_ErrorCode LUMICE_GetDrainStatus(LUMICE_Server* server, LUMICE_DrainResult* out);

// Synchronous readback of the
// most recent commit's color-classification overflow counters (see the
// LUMICE_ColorOverflowInfo doc block above). The GUI DoRun path calls this
// after CommitConfigStruct returns OK; a non-zero component_overflow_count
// triggers a modal "coloring degraded" prompt. LUMICE_OK on success;
// LUMICE_ERR_NULL_ARG if server or out is null.
LUMICE_API LUMICE_ErrorCode LUMICE_GetColorOverflowInfo(LUMICE_Server* server, LUMICE_ColorOverflowInfo* out);

// Has the GPU backend stopped running partway through the current run?
// Writes 1 to *out_fell_back once this server's GPU single-engine route has dropped
// its trace backend (a device/PSO failure mid-run), never obtained one, or holds one
// the run's config cannot use (no renderers, more than the backend serves, a
// compatibility miss), and tracing has continued on the legacy CPU path; 0
// otherwise. Always 0 on the CPU route, and 0
// again after the next LUMICE_StartServer. The condition is discovered asynchronously
// by the worker, so poll this each GUI tick the way LUMICE_GetColorOverflowInfo is
// polled — it is a single cheap point read. Beyond the slowdown, the fallback also
// changes what the frame looks like for the first seconds (already-queued GPU-sized
// batches carry one wavelength each on the CPU path). LUMICE_OK on success;
// LUMICE_ERR_NULL_ARG if server or out_fell_back is null.
LUMICE_API LUMICE_ErrorCode LUMICE_GetBackendFallbackFlag(LUMICE_Server* server, int* out_fell_back);

LUMICE_API void LUMICE_StopServer(LUMICE_Server* server);

// Continue the committed RENDER: trace more rays into the accumulation the last run left behind
// (v4.44). LUMICE_CommitScene always starts from zero; this is the other way to start a run.
//
// What carries over: everything a commit would reset — the render planes (raw XYZ and the images
// made from them), sim_ray_num and the other statistics, emitted_energy, and the exposure anchor
// (anchor_l99_sky) — so every result read afterwards describes the earlier rays and the new ones
// together, and the picture does not jump in brightness beyond what more samples themselves
// change. The scene is the committed one, unchanged; only the budget is new.
//
// The new rays are NEW random samples: each continuation traces a random stream of its own, also
// on a server created with a fixed `sim_seed` (which would otherwise restart its stream and trace
// the same rays again). A fixed-seed server stays reproducible — the same sequence of commits and
// continuations traces the same rays.
//
// `additional_ray_num` is an INCREMENT, total across wavelengths — "this many more", not a new
// total (unlike LUMICE_SceneSetSimParams' `ray_num`, which is the whole run's budget). `infinite`
// non-zero ignores it and runs until LUMICE_StopServer.
//
// Lifecycle: works after a render run COMPLETED (a finite budget ran out) or was stopped (IDLE).
// On success the lifecycle reads RUNNING and the epoch has advanced by one, exactly as after a
// commit; poll LUMICE_GetSimLifecycle / LUMICE_GetDrainStatus as for any run, and the run ends
// COMPLETED (finite) or stays RUNNING until stopped (infinite). A completed run whose last
// batches are still being consumed is drained first, so no traced ray is dropped — unconditionally:
// if that drain does not finish within an internal bound (a normal consumer pass is orders of
// magnitude faster), the call fails rather than proceeding and discarding them (see Errors below).
//
// Errors (a rejected call changes nothing): LUMICE_ERR_NULL_ARG (NULL server);
// LUMICE_ERR_INVALID_VALUE (`infinite` == 0 with `additional_ray_num` == 0); LUMICE_ERR_SERVER
// when there is nothing to continue — no render was ever committed, or the current session is a
// raypath analysis (LUMICE_StartRaypathAnalysis; a render must be committed again first), a run
// is in progress, or a just-completed run's last batches did not finish draining within the
// internal wait bound (retry shortly; this is the "no traced ray is dropped" guarantee failing
// safe rather than silently).
LUMICE_API LUMICE_ErrorCode LUMICE_ContinueRender(LUMICE_Server* server, int infinite,
                                                  LUMICE_RayCount additional_ray_num);

// =============== Raypath Analysis Run ===============
// The other kind of run a server can carry (doc/raypath-analysis-panel.md): no image, a
// histogram of COMPLETE raypath chains — which crystal, and which face sequence through it,
// on every scattering layer a ray traversed — with the energy each chain delivered into a
// region of interest, sorted by that energy. Same lifecycle as a render run, one request
// structure in, one result kind out of the same LUMICE_ResultFrame.
//
// Lifecycle, stated once here:
//   1. Build the scene to analyse — LUMICE_SceneFromJson / _FromJsonFile, or the ConfigScratch
//      route — exactly as for LUMICE_CommitScene. No commit is needed first (v4.36): the
//      analysis is a submission of its own, and a server that has never committed anything
//      analyses just the same.
//   2. If a render is in progress, LUMICE_StopServer or wait for it to complete — an analysis
//      cannot start over a render in progress (LUMICE_ERR_SERVER), and a render commit cannot
//      start over an analysis in progress (LUMICE_ERR_SERVER). Neither silently interrupts the
//      other.
//   3. LUMICE_StartRaypathAnalysis with the scene and a request. The run traces that scene on
//      the CPU (see below), to the scene's ray_num budget — an "infinite" budget runs until
//      LUMICE_StopServer, exactly as a render would — or, for a cone ROI with a stop target,
//      until that many rays have landed in the cone, whichever comes first. The scene is read
//      at the call and deep-copied; the handle stays the caller's, as with LUMICE_CommitScene.
//   4. Poll LUMICE_GetSimLifecycle / LUMICE_GetDrainStatus as for a render; read the result
//      through LUMICE_AcquireResultFrame + LUMICE_FrameGetRaypathAnalysisInfo /
//      LUMICE_FrameGetRaypathAnalysis. Partial results are readable while the run is in
//      progress, like a render's, and are final once the epoch reports drained. The run
//      advances the lifecycle epoch like a commit does: it is a submission, and a frame of it
//      is never mistaken for the last render's.
//   5. The next LUMICE_CommitScene is a render run again: the analysis-session properties
//      below are withdrawn, and its frames carry no histogram. What the analysis submitted
//      does not carry over into it — a commit after an analysis behaves exactly as a commit
//      after the previous commit would have.
//
// CPU, always. The chain ids the histogram is built from exist on the legacy CPU path only
// (v1 — doc/raypath-analysis-panel.md §2 ruling 1), so the analysis run forces that route
// for its duration, ahead of BOTH LUMICE_SetPreferredBackend and the LUMICE_TRACE_BACKEND
// environment override. It is a session property, not a fallback: the preference is left
// as it was and the next render honours it; LUMICE_GetBackendFallbackFlag stays 0; and
// LUMICE_GetActiveBackend reads LUMICE_BACKEND_CPU for as long as the session lasts. The
// server logs one INFO line saying so when the run starts.

// Which rays count.
#define LUMICE_RAYPATH_ROI_FULL_SKY 0  // every outgoing ray
#define LUMICE_RAYPATH_ROI_IN_FRAME 1  // rays that land inside `frame_view` (lens, view, visible, front)
#define LUMICE_RAYPATH_ROI_CONE 2      // rays within `cone_radius_rad` of `cone_center`, binned by angular distance

// The symmetry a READ of the result reduces under (LUMICE_FrameGetRaypathAnalysisInfo /
// LUMICE_FrameGetRaypathAnalysis, v4.33): a bit set of these, 0..7, the same FilterConfig
// symmetry the filter grammar uses. 0 is a legal value — no reduction, every recorded face
// sequence its own row. There is no "default" sentinel: the run records unreduced, and each read
// says what it wants.
#define LUMICE_RAYPATH_SYMMETRY_P 1
#define LUMICE_RAYPATH_SYMMETRY_B 2
#define LUMICE_RAYPATH_SYMMETRY_D 4

// `infinite` value meaning "use the committed scene's own ray_num / infinite" (v4.32). Sits
// outside the boolean domain {0, 1}: a zero-initialized request must not silently ask for it —
// `infinite = 0, ray_num = 0` is a request for zero rays, a degenerate but honest budget.
#define LUMICE_RAYPATH_RAY_BUDGET_SCENE_DEFAULT (-1)

// What one result entry can hold, sized so that NO chain a run can produce is cut: a chain has
// one segment per scattering layer the ray traversed, and a config has at most
// LUMICE_MAX_CONFIG_SCATTER_LAYERS of those; a segment is the face sequence through one crystal,
// at most the scene's max_hits, whose ceiling is core's kMaxHits = 64 (pinned by static_assert
// at the C boundary, so the two cannot drift apart silently). Symmetry reduction relabels faces
// and never shortens or lengthens a sequence. A chain past either cap therefore only comes from
// malformed data; it is TRUNCATED in the entry (the fields still describe a self-consistent
// prefix) and the server logs one WARN per frame read, rather than overrunning anything.
#define LUMICE_MAX_RAYPATH_CHAIN_LAYERS LUMICE_MAX_CONFIG_SCATTER_LAYERS
#define LUMICE_MAX_RAYPATH_SEGMENT_LEN 64
// Ring cap for the cone ROI. NOT a truncation: a request asking for more rings than this is
// rejected by LUMICE_StartRaypathAnalysis with LUMICE_ERR_INVALID_VALUE, because a truncated ring
// split would silently change what the entries mean.
#define LUMICE_MAX_RAYPATH_CONE_RINGS 32
// Upper bound on LUMICE_RaypathAnalysisRequest::chain_capacity (v4.43). NOT a truncation either: a
// request above it is rejected with LUMICE_ERR_INVALID_VALUE. The bound is a memory budget, not a
// correctness limit — the record costs about 220 bytes per chain per simulation worker on the
// producer side (workers × chain_capacity × 220 B) and roughly as much again once on the
// consumer side, so at this cap a run on ten workers commits on the order of 4-5 GB, which is a
// figure a caller should have to ask for explicitly and which also stops a mistyped extra digit
// from being honoured. With `--workers` raised past the automatic count, multiply accordingly.
#define LUMICE_MAX_RAYPATH_CHAIN_CAPACITY (1 << 20)
// Longest `display` text an untruncated entry can need, including the terminating NUL. Derived
// from the types rather than from what scenes produce: per layer, "C" (1) + a uint16 id (5) +
// "(" + 64 uint16 faces joined by "-" (64*5 + 63 = 383) + ")" + the joining " -> " (4) = 395,
// times LUMICE_MAX_RAYPATH_CHAIN_LAYERS = 3160; rounded up. Longer text (only possible past the
// caps above) is truncated with the same WARN as the arrays.
#define LUMICE_RAYPATH_DISPLAY_MAX 3200

typedef struct LUMICE_RaypathAnalysisRequest_ {
  int roi_mode;  // LUMICE_RAYPATH_ROI_*

  // LUMICE_RAYPATH_ROI_IN_FRAME only: the frame whose lens / view / visible / front decide
  // membership — the same struct the annotation anchors take, so a GUI panel passes the view it
  // is already drawing. `width`/`height` are the canvas the membership test is made on.
  LUMICE_AnnotationView frame_view;

  // LUMICE_RAYPATH_ROI_CONE only. `cone_center` is a world direction in the convention every
  // direction in this API uses (the direction light TRAVELS: altitude = asin(-z), the zenith is
  // z = -1 — LUMICE_UnprojectPixel below returns one). Need not be normalized; a zero vector is
  // rejected. `cone_radius_rad` must be positive. `cone_ring_count` splits [0, radius] into
  // that many equal angular-distance rings, 1..LUMICE_MAX_RAYPATH_CONE_RINGS. There is no
  // per-cone stop (removed in v4.34): the run's length is the ray budget below, or
  // LUMICE_StopServer, in every ROI mode.
  float cone_center[3];
  float cone_radius_rad;
  int cone_ring_count;

  // No symmetry here (v4.33): the run records every chain unreduced, and the symmetry is a
  // parameter of the READ — see LUMICE_FrameGetRaypathAnalysis.

  // ADDED v4.32: THIS run's own ray budget, independent of the committed scene's ray_num /
  // infinite (LUMICE_SceneSetSimParams) and in the same representation: `infinite` 1 means
  // unlimited (the run ends on LUMICE_StopServer), 0 means
  // `ray_num` is the total across every wavelength (distributed per wavelength as the scene's own
  // budget is). LUMICE_RAYPATH_RAY_BUDGET_SCENE_DEFAULT ignores `ray_num` and traces the scene's
  // own budget — the behaviour before v4.32. Any other `infinite` is rejected with
  // LUMICE_ERR_INVALID_VALUE.
  int infinite;
  LUMICE_RayCount ray_num;

  // ADDED (v4.43). How many distinct finest chains THIS run's record keeps exact — the same
  // number sizes the per-worker interning table (what the trace can tell apart) and the server's
  // histogram (what the read can list), so the two bounds are only ever both exact or both not.
  // 0 = the default (16384, the compiled-in calibration for the GUI's multi-scatter scenes);
  // 1..LUMICE_MAX_RAYPATH_CHAIN_CAPACITY = that many chains; anything else is rejected with
  // LUMICE_ERR_INVALID_VALUE. Raise it for a `--symmetry none` read of a scene with more distinct
  // chains than the default holds (the run's `record_full_hits` / truncated_chain_count says
  // when that happened); the cost is memory, see LUMICE_MAX_RAYPATH_CHAIN_CAPACITY.
  int chain_capacity;
} LUMICE_RaypathAnalysisRequest;

// One scattering layer of a chain: the crystal (its config id) and the face sequence the ray
// took through it, reduced under the read's symmetry, root-first in the entry's `chain` array.
typedef struct LUMICE_RaypathChainSegment_ {
  int crystal_id;
  int segment[LUMICE_MAX_RAYPATH_SEGMENT_LEN];
  int segment_len;  // faces actually written into `segment` (<= the cap; see the truncation note)
} LUMICE_RaypathChainSegment;

// One chain and what it delivered. The sentinel of LUMICE_FrameGetRaypathAnalysis is
// `count == 0`: every real entry counted at least one ray.
typedef struct LUMICE_RaypathHistogramEntry_ {
  LUMICE_RaypathChainSegment chain[LUMICE_MAX_RAYPATH_CHAIN_LAYERS];
  int chain_len;  // layers actually written into `chain` (>= 1 for a real entry)

  // The chain as text (v4.33 format). Faces joined by "-". A layer that holds more than one
  // crystal in the scene names its crystal as "C<id>" (the config id, as in `chain`); a layer
  // that holds one crystal does not, since the position already says which crystal. With more
  // than one layer every layer is parenthesised and layers are joined by " -> ", root first:
  //   "3-5"                        one layer, the layer's only crystal
  //   "C1(3-5)"                    one layer, a layer with several crystals
  //   "(3-5) -> (1-3)"             two single-crystal layers
  //   "C1(1-3) -> C4(3-5)"         two multi-crystal layers
  // This is a byte copy of the ONE implementation of that format (the server's
  // FormatRaypathChainDisplay); a consumer that needs the same text prints this field rather than
  // re-assembling it from `chain`, so the CLI and every GUI agree on it by construction. The
  // arrow is ASCII on purpose — the same bytes in every consumer, and the GUI's font has no
  // U+2192. NUL-terminated.
  char display[LUMICE_RAYPATH_DISPLAY_MAX];

  double energy;          // sum over counted rays of Y(wavelength) * weight
  LUMICE_RayCount count;  // number of counted rays

  // LUMICE_RAYPATH_ROI_CONE only: `energy` split by angular-distance ring from the cone centre,
  // `ring_count` == the request's cone_ring_count and sum(ring_energy) == energy. ring_count is 0
  // in the other modes.
  double ring_energy[LUMICE_MAX_RAYPATH_CONE_RINGS];
  int ring_count;

  // ADDED v4.35. How much of `energy` (and of `ring_energy`, and proportionally of `count`) may
  // belong to some other chain: the server holds a bounded number of rows, and a chain arriving
  // with no row while every row is taken takes over the lowest row — energy, count and rings —
  // and records what it took over here (Space-Saving). The chain's true energy lies within
  // [energy - error_bound, energy]; a row that never took a slot over has 0. Under a symmetry
  // that merges rows it is the sum over the merged rows, so a row standing for m recorded chains
  // is uncertain by at most m × (total energy / row capacity).
  double error_bound;
} LUMICE_RaypathHistogramEntry;

// What a frame says about its analysis result as a whole, before any entry is read.
typedef struct LUMICE_RaypathAnalysisInfo_ {
  int present;            // 1 iff this frame is an analysis frame; every other field is 0 when it is not
  int roi_mode;           // echo of the request's LUMICE_RAYPATH_ROI_*
  int entry_count;        // total entries in the frame — what a full read of LUMICE_FrameGetRaypathAnalysis returns
  int cone_ring_count;    // echo of the request (CONE), else 0
  float cone_radius_rad;  // echo of the request (CONE), else 0
  // The server's snapshot counter this frame was published under — the same
  // LUMICE_RawXyzResult::snapshot_generation, which an analysis frame cannot supply (its raw-XYZ
  // row is the sentinel). Strictly increases with every snapshot the server takes; two acquires of
  // the same frame read the same value. THIS is the "is there a new result" signal: `present`
  // above is true on every frame of the session, not once. 0 when present == 0. (v4.30)
  unsigned long long snapshot_generation;

  // ADDED v4.35: what the bounded record could not keep as rows, all 0 when present == 0 and all
  // 0 for a run that fit — the shape of every small scene. `other_energy` / `other_count` are the
  // rays whose chain the producer's interning table had no room for, as ONE bucket that is never
  // an entry and never reduced: the sum of every entry's `energy` plus `other_energy` is the
  // energy of every counted ray, under every symmetry, and likewise for `count`. A consumer
  // listing the entries shows this as one more line ("other") so the percentages add up.
  // `truncated_chain_count` is how many times a ray's chain hit the full record (summed over the
  // producers and the run) — arrivals, not distinct chains, since a chain that was turned away is
  // not remembered and counts again when it comes back; it says how often the cut hit, and bounds
  // the distinct chains the bucket stands for from above. `max_row_error` is the largest `error_bound` over the entries
  // under THIS symmetry (merging rows adds their errors), i.e. how uncertain the least certain
  // entry is; 0 means no row ever took a slot over and every entry is exact.
  double other_energy;
  LUMICE_RayCount other_count;
  // Clamped from the internal size_t counter to INT_MAX rather than truncated by the narrowing
  // cast — a run would need to overflow a chain's arrival count past ~2.1 billion for this to
  // read anything other than the true count, but the clamp keeps that case a saturated (still
  // meaningful) lower bound instead of a wrapped, misleading one.
  int truncated_chain_count;
  double max_row_error;
} LUMICE_RaypathAnalysisInfo;

// Start an analysis run on `scene` (lifecycle above; v4.36 — the scene is the call's own, no
// prior LUMICE_CommitScene is needed). Returns LUMICE_ERR_NULL_ARG for a NULL server / scene /
// request; LUMICE_ERR_INVALID_VALUE for an unknown roi_mode, a CONE request with a non-positive
// radius, a zero centre, a ring count outside 1..LUMICE_MAX_RAYPATH_CONE_RINGS, an IN_FRAME
// request whose frame_view has an unknown lens_type / visible, or an `infinite` outside its three
// spellings; for a scene the server cannot use, what LUMICE_CommitScene returns for the same scene
// (LUMICE_ERR_MISSING_FIELD / LUMICE_ERR_INVALID_JSON / LUMICE_ERR_INVALID_CONFIG), with nothing
// stopped or replaced; LUMICE_ERR_SERVER when a render run is in progress (AC1 — stop it first).
// Calling it while an ANALYSIS run is in progress restarts the analysis with the new scene and
// request.
LUMICE_API LUMICE_ErrorCode LUMICE_StartRaypathAnalysis(LUMICE_Server* server, const LUMICE_Scene* scene,
                                                        const LUMICE_RaypathAnalysisRequest* request);

// Frame-level view of the analysis result under `chain_id_symmetry` (a LUMICE_RAYPATH_SYMMETRY_*
// bit set, 0..7). Every field but `entry_count` is independent of the symmetry; `entry_count` is
// the number of rows LUMICE_FrameGetRaypathAnalysis returns for the SAME symmetry, so a caller
// sizing an array from it passes the same value to both. Writes present = 0 (and zeros) for a
// frame that is not an analysis frame — a render frame, or a frame acquired before the first
// snapshot. Returns LUMICE_ERR_NULL_ARG on a NULL frame / out; LUMICE_ERR_INVALID_VALUE for a
// symmetry outside 0..7 (nothing is written).
LUMICE_API LUMICE_ErrorCode LUMICE_FrameGetRaypathAnalysisInfo(const LUMICE_ResultFrame* frame, int chain_id_symmetry,
                                                               LUMICE_RaypathAnalysisInfo* out);

// The entries under `chain_id_symmetry` (0..7, as above): the frame's recorded chains with each
// layer's face sequence reduced under it — with the D parameters of THAT layer's crystal, the
// same rule a filter on that crystal canonicalises by — and chains that meet on one reduced form
// merged into one row whose `energy`, `count` and `ring_energy` are the sums. The sums over all
// rows are the same at every symmetry; the row count never grows as bits are added. Computed
// on this call, from the frame's recorded (unreduced) result, so two reads of one frame under two
// symmetries are two views of the same run.
//
// Energy descending (ties by `display` ascending, so two runs order equal energies alike). Same
// (out, max_count) array shape and sentinel contract as LUMICE_FrameGetRawXyz, the sentinel being
// an entry with `count == 0` — written at out[count] only when count < max_count, so an array of
// max_count + 1 value-initialized entries is the shape for sentinel iteration.
// LUMICE_FrameGetRaypathAnalysisInfo's entry_count (for the same symmetry) says how many there
// are in total. The entries are COPIED into `out` (no pointer into the frame), so they outlive
// the frame; the frame still has to be held for the duration of this call. Returns
// LUMICE_ERR_NULL_ARG on a NULL frame / out; LUMICE_ERR_INVALID_VALUE for a symmetry outside 0..7
// (nothing is written).
LUMICE_API LUMICE_ErrorCode LUMICE_FrameGetRaypathAnalysis(const LUMICE_ResultFrame* frame, int chain_id_symmetry,
                                                           LUMICE_RaypathHistogramEntry* out, int max_count);

// =============== Preferred Trace Backend ===============
// Stable backend identifiers. Future backends (e.g. CUDA) append new positive
// values; 0 stays CPU so default zero-init = legacy behavior.
#define LUMICE_BACKEND_CPU 0
#define LUMICE_BACKEND_METAL 1
#define LUMICE_BACKEND_CUDA 2

// Set preferred trace backend for this server.
//   backend = LUMICE_BACKEND_CPU   (default): legacy CPU path.
//   backend = LUMICE_BACKEND_METAL          : request Metal; falls back to CPU
//                                             if incompatible or unavailable
//                                             on this platform.
// Takes effect on the next simulation start (after LUMICE_CommitScene). The
// env-var LUMICE_TRACE_BACKEND, when set, overrides this preference:
//   - "cpu_backend"            forces CPU unconditionally (ignores this pref).
//   - "metal"                  forces Metal (Apple) regardless of this pref.
//   - unset / "" / "legacy"    defers to this API preference.
// (i.e. an empty or "legacy" env-var no longer forces CPU once this pref is
//  set to LUMICE_BACKEND_METAL — use "cpu_backend" to hard-pin CPU in CI.)
// On non-Apple platforms LUMICE_BACKEND_METAL is silently treated as CPU.
LUMICE_API void LUMICE_SetPreferredBackend(LUMICE_Server* server, int backend);

// Query whether a trace backend is available on this machine at runtime.
//   backend = LUMICE_BACKEND_CPU   : always returns 1.
//   backend = LUMICE_BACKEND_METAL : returns 1 iff Apple build AND a Metal
//                                    device is present at runtime; 0 otherwise
//                                    (non-Apple, or Mac without Metal device).
//   other values                   : returns 0.
// Result is cached after the first call; safe to call from any thread and from
// per-frame GUI code.
// To add a new backend (e.g. CUDA): append LUMICE_BACKEND_CUDA above and add a
// matching branch here; CPU / Metal semantics are unchanged.
LUMICE_API int LUMICE_IsBackendAvailable(int backend);

// The backend this server's simulation ACTUALLY runs on, as opposed to the one
// LUMICE_SetPreferredBackend asked for. Writes LUMICE_BACKEND_CPU / _METAL / _CUDA. The two differ in
// exactly two situations, and this call is what makes both observable: during an analysis run
// (LUMICE_StartRaypathAnalysis — always CPU, the session forces it, and the answer is CPU from the
// moment the run starts rather than from its first batch), and on a GPU route whose backend was
// lost or never obtained (LUMICE_GetBackendFallbackFlag says which). Otherwise it is what the last
// LUMICE_CommitScene resolved to, and LUMICE_BACKEND_CPU before any run. Cheap; safe to poll.
// Returns LUMICE_ERR_NULL_ARG if server or out_backend is NULL.
LUMICE_API LUMICE_ErrorCode LUMICE_GetActiveBackend(LUMICE_Server* server, int* out_backend);

// Query whether a server built with `preferred_backend` would take the GPU
// single-engine route (worker_count=1) on this machine. Unlike
// LUMICE_IsBackendAvailable (which only reports device presence), this also honors
// the `LUMICE_TRACE_BACKEND` env override, which wins over `preferred_backend` — so
// e.g. `LUMICE_TRACE_BACKEND=cuda` with preferred_backend=CPU returns 1 (iff an
// eligible CUDA device exists). Same resolution the server uses to size worker_count.
// Intended for the CLI `--benchmark` dual-pass: the GPU route is single-engine, so
// its "single" (warmup) vs "multi" (steady) passes are NOT parallel — callers use
// this to collapse the GPU benchmark to one steady pass. Returns 1 (GPU route) or 0.
LUMICE_API int LUMICE_WillUseGpuRoute(int preferred_backend);

#ifdef __cplusplus
}
#endif

#endif  // LUMICE_ENGINE_H_
