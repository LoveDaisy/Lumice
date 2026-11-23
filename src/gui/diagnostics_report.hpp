#ifndef LUMICE_GUI_DIAGNOSTICS_REPORT_HPP
#define LUMICE_GUI_DIAGNOSTICS_REPORT_HPP

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "gui/diagnostics_platform.hpp"

namespace lumice::gui {
namespace diagnostics {

// ---------------------------------------------------------------------------
// What this module is.
//
// The fault-scene recorder behind the GUI's
// "Export Diagnostics..." entry. It holds three kinds of observation apart —
// plan §3.1's three time tenses:
//
//   1. Export-time environment (OS / CPU / RAM / the live GL context / the
//      server's actual route) — collected by diagnostics_platform.* at the
//      moment the user exports.
//   2. The current application/display state at export time.
//   3. The upload scene — frozen at the moment each preview-texture upload
//      ran, by the upload functions themselves (preview_renderer.cpp) plus the
//      caller that set the context (app.cpp).
//
// The recorder is a passive OBSERVER of the upload chain. It owns no
// acceptance, retry, recovery or display decision: that judgment belongs to
// the upload chain's acceptance fix (a separate parallel change), which will
// replace the fact-producing side of this same production point with its
// structured result when it lands. What must not happen is a second module
// re-deriving "did the upload succeed" — there is one judgment, and this is
// not it.
//
// Threading: every collector entry point below runs on the GUI main thread
// (the only thread that owns a GL context or touches GuiState). There is no
// lock on purpose; calling from any other thread is a contract violation.
// ---------------------------------------------------------------------------

// Which upload entry produced an event. One-to-one with PreviewRenderer's
// three sim-texture entry points (the background image upload is NOT a sim
// upload and never produces events).
enum class UploadPath {
  kXyz,             // UploadXyzTexture — float16 halves via the PBO ring
  kSrgbComposited,  // UploadTexture — 8-bit sky-included (legacy .lmc)
  kSrgbRadiance,    // UploadRadianceTexture — 8-bit radiance-only
};

// Where the uploaded pixels came from — plan §3.1's live/reopen distinction.
enum class UploadProvenance {
  kLive,    // the poller's payload for this session's run
  kReopen,  // a .lmc texture section, uploaded by ResetFrontendState(kOpenBaked)
};

// Document origin, recorded when the document changes. A diagnostic reader
// must be able to tell "this failure belongs to the document that was open
// then" from "this is what is open now" — file paths are private, so the
// association travels as anonymous serials (document_serial below).
enum class DocumentOrigin {
  kNew,                // New document
  kImported,           // .json config import (no texture section)
  kReopened,           // .lmc open with a texture section
  kReopenedNoTexture,  // .lmc open whose texture section was absent
};

// What the upload action concluded, translated mechanically from the upload
// chain's own structured result (UploadResult) by app.cpp's completion
// bracket — these are OBSERVED control-flow facts, not judgments: the
// acceptance, retry and recovery decisions stay with the upload chain, and
// the report only says what happened.
enum class UploadOutcome {
  kCompleted,             // the accepting attempt committed the texture state
  kRejectedInvalidInput,  // null data / non-positive size / unbound texture (no GL call made)
  kFailedFrameRetained,   // failed; the previous frame provably stands
  kFailedDisplayRevoked,  // failed through a conservative revoke; the display is cleared
};

// The GL error flag at the upload chain's own check for this attempt — or
// the fact that the attempt made no checkable GL call (AC3's three states:
// checked-and-clean / reported-error / unchecked-or-no-record). The chain's
// paths read the flag at their own stage boundaries (per strip, post-define,
// post-upload); the recorder adds no reads of its own — detailed error
// checking belongs to the upload chain.
enum class GlErrorCheckState {
  kNone,        // checked; GL_NO_ERROR
  kReported,    // checked; the code below carries what glPaidError said
  kNotChecked,  // this path performs no check
};

// The display-commit fact, attached by the CALLER after the upload call
// returns (it knows what it committed). kUnknown means the caller did not
// report — e.g. the reopen path, whose display outcome is carried by
// current_document.display instead.
enum class DisplayCommit {
  kUnknown,
  kAdvanced,     // caller advanced its displayed-frame state after this call
  kNotAdvanced,  // caller left its displayed-frame state alone
};

// The view/sim settings frozen alongside an upload event or an export. This
// is a deliberate whitelist, NOT a GuiState projection: file paths, host
// names, user naming and everything else privacy-excluded (plan §3.3) have no
// field here to hide in.
struct FrozenViewSettings {
  int resolution_tier = 0;  // kSimResolutions value (512/1024/2048/4096)
  int lens_type = 0;        // LensType enum value (gui_constants.hpp)
  float fov = 0.0f;         // degrees
  float ray_num_millions = 0.0f;
  int max_hits = 0;
  bool ray_num_infinite = false;
  bool ray_allocation_adaptive = false;
};

// The caller-side half of an upload event: who asked for the upload and with
// what pixels. Frozen BEFORE the upload call, synchronously on the main
// thread, so the event cannot be back-filled by later edits (plan §3.1: the
// incident's settings and the current settings are two different readings).
struct UploadEventContext {
  UploadProvenance provenance = UploadProvenance::kLive;
  int source_width = 0;
  int source_height = 0;
  FrozenViewSettings settings;
  // Live only. `run_epoch` is the SOURCE PIXELS' production epoch (the
  // payload's own stamp) — not the snapshot bundle's current epoch, which
  // advances on every publish and lets a carry-forward payload be re-uploaded
  // by a mode change: billing an old frame to a new run is exactly the
  // misattribution the frozen context exists to prevent. `bundle_epoch` is
  // that current epoch, recorded beside it so the two can disagree visibly.
  // `texture_serial` is the exact-once serial the upload fired under.
  // Reopen uploads carry none of these (there is no run).
  std::optional<unsigned long long> run_epoch;
  std::optional<unsigned long long> bundle_epoch;
  std::optional<unsigned long long> texture_serial;
  // The display adapter the server session ran on AT UPLOAD TIME, frozen
  // beside the event — the export-time route section cannot answer for a
  // failure that happened under a reconstructed server or a different
  // backend. nullopt on reopen uploads (no session produced their pixels).
  std::optional<std::string> active_backend_at_upload;
};

// The renderer-side half: what the upload function observed. Produced inside
// preview_renderer.cpp at each exit of the two upload bodies.
struct UploadFacts {
  UploadPath path = UploadPath::kXyz;
  UploadOutcome outcome = UploadOutcome::kCompleted;
  int requested_width = 0;
  int requested_height = 0;
  // XYZ only: the PBO capacity this upload used, and the float16 scale it
  // quantized with (meaningful only once quantization ran).
  std::optional<size_t> pbo_bytes;
  std::optional<float> xyz_half_scale;
  GlErrorCheckState gl_error_state = GlErrorCheckState::kNotChecked;
  // The code the upload chain's own failing-stage read returned, serialized
  // as hex. On the fence-wait stage the value may be the sync STATUS rather
  // than a GL error — the stage name below says which, and the raw value is
  // reported as received, not reinterpreted.
  std::optional<unsigned int> gl_error_code;
  // Where the attempt stopped: the failing stage's name from the upload
  // chain's own stage taxonomy ("fence-wait"/"pbo-allocate"/"pbo-map"/
  // "pbo-unmap"/"texture-define"/"tex-update"/"fence-create"/
  // "storage-mismatch"/"cpu-allocation"). With a reported error it names the
  // stage whose read found the flag; set on every non-invalid failure — a
  // stage can fail without leaving an error flag, and that fact is report
  // content, not an absence.
  std::optional<std::string> error_stage;
  // The XYZ attempt committed while its staging slot's fence creation failed
  // (glFenceSync returned null): the frame itself is displayed, but the next
  // write to the slot runs unguarded. Kept apart from gl_error — the failure
  // may stand behind an error flag or not; the fact is the null return, and
  // it is recorded before the event closes so it cannot escape the report.
  // A completed outcome carrying this fact is still retained as a failure.
  std::optional<bool> fence_create_failed;
  // The texture's measured level-0 state, translated from the upload chain's
  // own witness (the success paths query it before committing; the
  // storage-mismatch revoke carries its disagreeing measurement back). Not
  // copied from the CPU mirror, not from the requested values — when the
  // witness did not run, actual_queried stays false (plan §3.2).
  bool actual_queried = false;
  int actual_width = 0;
  int actual_height = 0;
  std::optional<unsigned int> actual_internal_format;
};

// One frozen upload attempt: context + facts + timestamp + the caller's
// display-commit fact.
struct UploadEvent {
  UploadEventContext context;
  UploadFacts facts;
  std::chrono::system_clock::time_point occurred_at = std::chrono::system_clock::now();
  DisplayCommit display_commit = DisplayCommit::kUnknown;
  // Stamped by the collector; the identity of the document this attempt
  // belonged to, never a file path.
  unsigned long long document_serial = 0;
  // Stamped by the collector: the attempt's identity within this session.
  // Timestamps cannot serve — two uploads inside one clock tick are a real
  // state, and first_failure/last_failure/last_upload may be copies of one
  // attempt that must count (and be updated) as one.
  unsigned long long event_serial = 0;
  // Stamped by the collector: the server instance the attempt ran under.
  // Epochs restart at 0 on every reconstruction, so this generation is what
  // keeps a pre-swap run distinguishable from a post-swap one.
  unsigned long long server_generation = 0;
};

// The retention contract (plan §3.2's small fixed-capacity history): the
// session's first failure (latched — successes never overwrite it), the most
// recent failure, and the most recent attempt of any outcome. Older attempts
// fall out of these three summaries; total_event_count is what makes the
// report able to say how many (DroppedEventCount above). Nothing larger than
// the three summaries is ever held — no pixel buffers, no full-frame copies.
struct UploadHistory {
  std::optional<UploadEvent> first_failure;
  std::optional<UploadEvent> last_failure;
  std::optional<UploadEvent> last_upload;
  // Every closed attempt since process start, retained or not.
  unsigned long long total_event_count = 0;
};

// An attempt counts as a failure when the chain itself reported one: a
// rejection, a failing stage, a GL error standing after the chain's check —
// or a completed frame whose fence creation failed (the next write to the
// slot runs unguarded; the fact rides fence_create_failed and must not be
// erased from the retention by the success it rode in on).
bool IsUploadFailure(const UploadEvent& event);

// The number of recorded attempts this history no longer represents, i.e.
// total_event_count minus the distinct attempts the three slots hold (a
// single attempt may occupy two slots: a first failure is usually also the
// last failure). The report must be able to say how much history it did NOT
// keep — "retained three summaries" without a dropped count would read as a
// complete record.
unsigned long long DroppedEventCount(const UploadHistory& history);

// ---------------------------------------------------------------------------
// The recorder. Production access is through Diagnostics(); tests may default-
// construct their own instances.
// ---------------------------------------------------------------------------
class DiagnosticsCollector {
 public:
  // Opens the pending event slot with the caller's frozen context. The next
  // CompleteUpload closes it. Calling BeginUpload while a slot is still open
  // discards the stale slot without recording it — a begun context never
  // became an attempt, so there is no event to count; production never does
  // this, the calls bracket one upload invocation, synchronously.
  void BeginUpload(const UploadEventContext& context);

  // Closes the pending slot with the renderer-side facts. Without a pending
  // slot this is a no-op: uploads driven without a context (existing tests,
  // future call sites) are honestly "no record", not synthesized events.
  void CompleteUpload(const UploadFacts& facts);

  // The caller's display-commit fact for the event just closed. Applied to
  // every retained slot holding that same attempt — Retain stores value
  // copies (first/last failure and last upload can all be one attempt), and
  // a fact learned after the retention must reach every copy. Attempts are
  // identified by their freeze timestamp; the just-closed event is the one
  // this applies to. No-op when nothing has been recorded yet.
  void AttachDisplayCommit(DisplayCommit commit);

  // The server instance was torn down and rebuilt inside the same document
  // (backend/worker construction properties changed): the fresh server
  // restarts its epoch authority at 0, so run epochs from before the swap
  // would collide with the same numbers after it. Bumping the anonymous
  // server generation gives every retained event a namespace that survives
  // the reuse. Deliberately separate from OnDocumentReset: reconstruction
  // keeps the document; the two counters are independent axes.
  void OnServerReconstructed();

  unsigned long long server_generation() const { return server_generation_; }


  // The document changed (ResetFrontendState, non-kRevert reasons). Bumps the
  // anonymous document serial and re-arms the "current document has seen an
  // upload attempt" fact. History is retained — but every retained event
  // carries the serial it belonged to, so an old failure can never pose as
  // the current document's (plan §3.2).
  void OnDocumentReset(DocumentOrigin origin);

  unsigned long long document_serial() const { return document_serial_; }
  DocumentOrigin current_document_origin() const { return current_origin_; }
  // Whether any upload attempt has been recorded for the CURRENT document.
  // The current context must be able to say "no upload happened here" even
  // though history holds older events.
  bool current_document_has_upload() const { return current_document_has_upload_; }

  const UploadHistory& history() const { return history_; }

  // Test isolation only.
  void ResetForTest();

 private:
  void Retain(const UploadEvent& event);

  UploadHistory history_;
  unsigned long long document_serial_ = 1;    // document 1 = the initial document
  unsigned long long server_generation_ = 1;  // server instance 1 = the first
  DocumentOrigin current_origin_ = DocumentOrigin::kNew;
  bool current_document_has_upload_ = false;
  bool upload_pending_ = false;
  UploadEvent pending_;
  // The serial of the most recently closed event — the identity
  // AttachDisplayCommit matches against, so a post-close caller fact reaches
  // every retained copy of that attempt.
  unsigned long long last_closed_serial_ = 0;
  // Monotonic attempt counter feeding event_serial.
  unsigned long long next_event_serial_ = 0;
};

// The process-wide recorder (main thread only).
DiagnosticsCollector& Diagnostics();

// The settings whitelist, read from a GuiState's fields at a moment worth
// freezing. Scalar parameters rather than a GuiState& so this header stays
// linkable without the full GUI state; the signature is itself the whitelist —
// the caller cannot freeze a field that is not a parameter. app.cpp is the
// one caller that maps GuiState onto it.
FrozenViewSettings SnapshotFrozenSettings(int resolution_tier, int lens_type, float fov, float ray_num_millions,
                                          int max_hits, bool ray_num_infinite, bool ray_allocation_adaptive);

// ---------------------------------------------------------------------------
// Report assembly — pure over its input, no GL, no OS queries, no globals
// (unit-tested with synthetic inputs; app.cpp gathers the real ones).
// ---------------------------------------------------------------------------

struct LogLine {
  std::string level;    // spdlog level name, e.g. "info", "warning"
  std::string message;  // one stored line, no trailing newline
};

struct EnvironmentInfo {
  std::optional<std::string> app_version;       // LUMICE_GetVersionString
  std::optional<std::string> engine_isa_level;  // LUMICE_GetEngineIsaLevel
};

// Export-time route facts. All three describe the CURRENT server session —
// a reopened document's picture came from a file and may predate this
// session, which is exactly what the assembly's note says out loud.
struct SimRouteInfo {
  std::optional<std::string> preferred_backend;   // what the GUI asked the server to run on
  std::optional<std::string> active_backend;      // LUMICE_GetActiveBackend at export
  std::optional<bool> backend_fell_back_mid_run;  // LUMICE_GetBackendFallbackFlag at export
};

struct CurrentDocumentInfo {
  unsigned long long document_serial = 0;
  // The server instance's anonymous generation at export time — the same
  // namespace the retained events carry. A server reconstruction restarts
  // the epoch authority at 0 inside one document, so this field is what lets
  // a reader tell whether a retained failure belongs to the CURRENT run or
  // to a previous server whose epochs this one reuses. 0 = not gathered.
  unsigned long long server_generation = 0;
  DocumentOrigin origin = DocumentOrigin::kNew;
  bool has_texture = false;
  std::string texture_mode;  // "xyz" / "srgb_radiance" / "srgb_composited" / "" when no texture
  int display_width = 0;
  int display_height = 0;
  std::optional<unsigned long long> committed_epoch;
  std::optional<unsigned long long> last_uploaded_texture_serial;
  unsigned long long texture_upload_count = 0;
  FrozenViewSettings settings_at_export;
};

struct DiagnosticsExportInput {
  std::chrono::system_clock::time_point exported_at = std::chrono::system_clock::now();
  EnvironmentInfo environment;
  PlatformInfo platform_info;  // diagnostics_platform.hpp; gathered at export
  GlInfo gl_info;              // ditto — the live context's display-adapter identity
  SimRouteInfo route;
  CurrentDocumentInfo current;
  UploadHistory history;
  bool current_document_has_upload = false;
};

// Builds the JSON document. `log_tail` is the explicitly opted-in raw log
// snapshot: pass nullptr when the user did not opt in (the key is then ABSENT,
// not empty — a absent key and an empty ring mean different things). The
// output never embeds the export destination path (plan §3.3).
std::string BuildDiagnosticsJson(const DiagnosticsExportInput& input, const std::vector<LogLine>* log_tail);

// ISO 8601 UTC with milliseconds — the one timestamp spelling every time
// field in the report uses.
std::string FormatIso8601Utc(std::chrono::system_clock::time_point tp);

}  // namespace diagnostics
}  // namespace lumice::gui

#endif  // LUMICE_GUI_DIAGNOSTICS_REPORT_HPP
