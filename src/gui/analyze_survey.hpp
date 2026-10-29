#ifndef LUMICE_GUI_ANALYZE_SURVEY_HPP
#define LUMICE_GUI_ANALYZE_SURVEY_HPP

// The Analyze workspace's background survey query (742.3, plan R6): the one mechanism that turns
// a selected class in the share list into an engine-typed survey summary without the GUI thread
// ever computing. The machinery is a single worker thread that owns the frozen scene binding and
// runs at most one LUMICE_PathWorkspaceQuerySurvey at a time; the GUI side keeps the intent (R6's
// request id: the analysis generation the selection names, and a monotonic serial within it) and
// drains finished outcomes through an adoption gate so a late A never lands under B.
//
// Ownership, stated once. The worker thread owns the LUMICE_Scene and the LUMICE_PathWorkspace —
// SetScene / ReleaseScene / destruction all run on it, so a query never overlaps the destruction
// of what it reads. The finished LUMICE_PathSurvey travels to the GUI in a single-slot mailbox
// and, once adopted, lives in GuiState (survey.adopted_result below): the GUI thread is then the
// only holder, so destruction (a ResetFrontendState branch, or a newer adoption) can never
// overlap a typed read — they are on the same thread by construction. The mailbox overwrite and
// the gate together bound retention at "the current selection's outcome + at most one in-flight":
// a worker can only run a query that was the current intent when issued, so any outcome that
// fails the gate is a superseded zombie, and the newest finished slot can only ever be
// overwritten by an outcome that also fails the gate.

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "include/lumice_base.h"
#include "include/lumice_raypath.h"

namespace lumice::gui {

struct SceneDeleter;  // gui/file_io.hpp (ScenePtr) — forward-declared to keep this header off
                      // file_io's include chain; call sites already have it complete.

// A finished survey as a GUI-owned read-only handle. The deleter is type-erased by shared_ptr,
// so GuiState can hold the member without including anything beyond the forward declaration.
struct SurveyDeleter {
  void operator()(LUMICE_PathSurvey* survey) const { LUMICE_PathSurveyDestroy(survey); }
};
using SurveyHandle = std::shared_ptr<const LUMICE_PathSurvey>;

// R6's request id: the analysis generation the selected row belongs to (the payload's
// snapshot_generation — a server counter that only grows, so generations never repeat) and a
// serial the executor assigns monotonically as intents are issued. Equality is the whole gate.
struct SurveyQueryId {
  unsigned long long generation = 0;
  unsigned long long serial = 0;
  bool operator==(const SurveyQueryId& other) const { return generation == other.generation && serial == other.serial; }
  bool operator!=(const SurveyQueryId& other) const { return !(*this == other); }
};

// What an outstanding intent was issued FOR: the selection key the pump compares level-triggered
// (a selection that changes shape or symmetry is a new intent, not a re-read of the old one).
// Derived, like SelectedClassIdentity under it — never a second stored selection (R10).
struct SurveySelectionKey {
  unsigned long long generation = 0;
  int crystal_id = 0;
  std::vector<int> faces;
  std::vector<int> layer_face_counts;
  int symmetry_bits_plus_one = 0;
  bool operator==(const SurveySelectionKey& other) const {
    return generation == other.generation && crystal_id == other.crystal_id && faces == other.faces &&
           layer_face_counts == other.layer_face_counts && symmetry_bits_plus_one == other.symmetry_bits_plus_one;
  }
  bool operator!=(const SurveySelectionKey& other) const { return !(*this == other); }
};

// The survey query's session state — a member of RaypathAnalysisSession, so it follows the
// analysis tool's per-reason resets for free (ResetFrontendState keeps it across Revert, clears
// it on every document switch; DoNew replaces the whole GuiState). Every field here is session
// tier: it is the tool's own bookkeeping about one background query, never document state.
struct SurveyQuerySession {
  // An intent is outstanding: a query was issued and neither cancelled nor answered yet. False
  // before the first selection and after the cancel button (R6: 意图失效) or the selection
  // clearing — the pump owns all three transitions.
  bool intent_valid = false;
  SurveyQueryId intent_id;  // meaningful only while intent_valid
  // The selection the intent was issued for; the pump compares it against the live selection.
  SurveySelectionKey key;
  // The cancel button's latch: the intent was withdrawn on purpose, and the pump must not
  // re-issue while the same selection stands (otherwise the button could not mean "cancel").
  // Cleared with the intent — a selection change starts fresh.
  bool cancelled_for_key = false;
  // The adopted outcome for intent_id. Exactly one of the two is set once the gate has passed a
  // finished query: `adopted_result` for a completed survey, `adopted_error` for the engine's
  // named refusal ("<reason>: <detail>" — multi_layer_unsupported et al., which IS the answer
  // for a multi-layer selection, R5). Both empty = still waiting.
  SurveyHandle adopted_result;
  std::string adopted_error;
};

// The overview region's display state, derived each frame (pure, unit-tested without a frame).
// kCancelled exists so the cancel button's effect is visible (R7): "cancelled" is not "waiting"
// and not "no selection".
enum class SurveyOverviewState { kNoSelection, kWaiting, kCompleted, kRefused, kCancelled };
SurveyOverviewState SurveyOverviewStateFor(const SurveyQuerySession& survey);

// ---- The executor ---------------------------------------------------------------------------------

class SurveyExecutor {
 public:
  // A finished query, drained by the GUI. `result` is null iff `ok` is false, in which case
  // `error` carries the engine's "<reason>: <detail>" text (a refusal the gate may still adopt —
  // it is a legitimate answer for a multi-layer selection).
  struct Finished {
    SurveyQueryId id;
    bool ok = false;
    std::string error;
    SurveyHandle result;
  };

  SurveyExecutor();
  ~SurveyExecutor();  // ShutdownAndJoin, as a safety net; call it explicitly at teardown.

  SurveyExecutor(const SurveyExecutor&) = delete;
  SurveyExecutor& operator=(const SurveyExecutor&) = delete;

  // Assigns the next request id for `generation` (R6's 代内序号; strictly monotonic across the
  // process, so a cancelled intent's id can never be reused by a re-issued one). GUI thread.
  SurveyQueryId NextId(unsigned long long generation);

  // Replaces the frozen scene binding (R6's input freeze, DoAnalyze's retention point): the
  // worker destroys the previous scene/workspace and builds the workspace for `scene` — never
  // while a query reads them (any in-flight query is cancelled first, and its outcome drops in
  // the gate because the intent died with the selection). Null scene is ignored. GUI thread.
  void SetScene(std::unique_ptr<LUMICE_Scene, SceneDeleter> scene);

  // Issues `id` against the current binding. The request is rebound to executor-owned copies of
  // `faces`/`counts` (the C struct holds raw pointers, so the buffers must outlive the query —
  // the job owns them). Supersedes any in-flight query by cancelling it (R6: 新意图取代旧) and
  // replacing whatever is queued. An id that was never handed out by NextId is a caller bug.
  void IssueQuery(SurveyQueryId id, const LUMICE_PathSurveyRequest& request, std::vector<int> faces,
                  std::vector<int> counts);

  // Cancels the in-flight query (if any) and drops the queued job (if any). The finished outcome
  // of the cancelled query may still arrive; the intent invalidation on the caller's side is what
  // makes the gate drop it. GUI thread.
  void CancelInFlight();

  // Destroys the scene binding on the worker thread (document switches). Any in-flight query is
  // cancelled first. After this, an issued query fails at workspace creation and the refusal
  // text says so — which only matters if the caller also kept the intent, and ResetFrontendState
  // never does (it clears the session with the binding). GUI thread.
  void ReleaseScene();

  // Takes the newest finished query, or nullopt. Single slot: a newer finish overwrites an
  // older one that the GUI has not drained — sound because the older one's intent was
  // necessarily superseded before a newer query could run (see the class comment). GUI thread.
  std::optional<Finished> TakeFinished();

  // Whether a query is between issue and finish. Test/observability face — the GUI never
  // branches on it (the display state is intent + outcome, not machinery).
  bool InFlight() const;

  // Cancels in-flight, drops queued, and joins the worker (which destroys the binding on its own
  // thread). The app's teardown calls this before the LUMICE global cleanup; every later call is
  // a no-op. GUI thread.
  void ShutdownAndJoin();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace lumice::gui

#endif  // LUMICE_GUI_ANALYZE_SURVEY_HPP
