#ifndef LUMICE_GUI_ANALYZE_SURVEY_HPP
#define LUMICE_GUI_ANALYZE_SURVEY_HPP

// The Analyze workspace's background query mechanism (742.3, plan R6; the map face added by
// 742.5): the one mechanism that turns a selected class — and now a chosen view — into an
// engine-typed result without the GUI thread ever computing. The machinery is a single worker
// thread that owns the frozen scene binding and runs at most one query at a time, whichever face
// it belongs to; the GUI side keeps the intent (R6's request id: the analysis generation the
// selection names, and a monotonic serial within it) and drains finished outcomes through an
// adoption gate so a late A never lands under B.
//
// Ownership, stated once. The worker thread owns the LUMICE_Scene and the LUMICE_PathWorkspace —
// SetScene / ReleaseScene / destruction all run on it, so a query never overlaps the destruction
// of what it reads. Finished results travel to the GUI in one mailbox slot per face and, once
// adopted, live in GuiState (survey.adopted_result / map.adopted_result): the GUI thread is then
// the only holder, so destruction (a ResetFrontendState branch, or a newer adoption) can never
// overlap a typed read — they are on the same thread by construction. The mailbox overwrite and
// the gate together bound retention at "the current intent's outcome per face + at most one
// in-flight per face": a worker can only run a query that was the current intent of its face
// when issued, so any outcome that fails the gate is a superseded zombie, and the newest
// finished slot can only ever be overwritten by an outcome that also fails the gate.

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

// The map face's handle, the same shape (742.5): a finished LUMICE_PathMap as a GUI-owned
// read-only handle living in GuiState once adopted — the GUI thread the only holder, so a typed
// read never races a destroy.
struct MapDeleter {
  void operator()(LUMICE_PathMap* map) const { LUMICE_PathMapDestroy(map); }
};
using MapHandle = std::shared_ptr<const LUMICE_PathMap>;

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

// The two query faces the executor runs. ONE worker runs at most one query of EITHER face (the
// workspace contract: queries on one workspace never overlap), but each face keeps its own
// pending slot and its own finished slot: an issue supersedes only its OWN face — a newer map
// view replaces an older map query and never cancels a standing survey (and vice versa; a
// cross-face cancel would starve the other face's valid intent, whose pump re-issues only on a
// key change). Cross-face the queries simply queue on the worker, survey slot first.
enum class QueryFace { kSurvey, kMap };

// The map face's session state — the survey session's sibling (same tier, same per-reason
// rules), with two deltas 742.5 owns. The key adds the view and the wavelength: a view or
// wavelength change is a new intent over the SAME selection, and a selection change still kills
// the whole session. And the adopted outcome is KEPT across a view supersession — the picture on
// show may sit one refinement behind the standing intent (what MapRefiningFor names), because a
// view change must not blank the region the user is currently steering; a selection change drops
// it (another class's map must not linger under a new selection).

// What an outstanding map intent was issued for: the selection key (the survey's own — one
// derivation, so map and survey cannot disagree about what class is on show) plus the view the
// display is showing (centre and angular radius; a request spans exactly that disk) plus the
// reference wavelength. centre_pos is the POSITION vector pointing at the view's sky centre —
// display-space semantics (it projects the picture); the request builder is where it becomes the
// engine's (altitude, azimuth) label. Equality is exact and bitwise: any gesture-frame change is
// a different view, and the pump's settle gate decides when that becomes a request.
struct MapIntentKey {
  SurveySelectionKey selection;
  float centre_pos[3] = { 0.0f, 0.0f, 1.0f };
  float radius_deg = 180.0f;   // >= kMapFullSkyRadiusDeg is the named all-sky view
  double wavelength_nm = 0.0;  // 0 = the engine's reference resolution
  bool operator==(const MapIntentKey& other) const {
    return selection == other.selection && centre_pos[0] == other.centre_pos[0] &&
           centre_pos[1] == other.centre_pos[1] && centre_pos[2] == other.centre_pos[2] &&
           radius_deg == other.radius_deg && wavelength_nm == other.wavelength_nm;
  }
  bool operator!=(const MapIntentKey& other) const { return !(*this == other); }
};

struct MapQuerySession {
  // An intent is outstanding: a map query was issued and neither cancelled nor answered yet.
  bool intent_valid = false;
  SurveyQueryId intent_id;  // meaningful only while intent_valid
  // The key the intent was issued for; the pump compares it against the live view+selection.
  MapIntentKey key;
  // The Revert row's latch (the map has no cancel button of its own — its queries are
  // sub-second; a withdrawn map intent comes only from a reset). Cleared with the intent: a
  // key change starts fresh.
  bool cancelled_for_key = false;
  // The newest gate-passed outcome: a completed map, or the engine's named refusal. Kept across
  // view supersession (the display decides what an older map is allowed to claim); dropped with
  // the whole session on a selection change. adopted_id says which intent it answers.
  MapHandle adopted_result;
  std::string adopted_error;
  SurveyQueryId adopted_id;
};

// The map region's four visible states — the survey vocabulary, over the map session (R7: the
// same honest separation of waiting / cancelled / refused / answered).
enum class MapQueryState { kNoSelection, kWaiting, kCompleted, kRefused, kCancelled };
MapQueryState MapQueryStateFor(const MapQuerySession& map);

// Whether the adopted map on show answers an OLDER intent than the one standing: a newer view
// (or wavelength) has been asked for and has not answered yet. The display shows the old picture
// anchored on the sky with a "refining" note — never as the newest answer (AC2).
bool MapRefiningFor(const MapQuerySession& map);

// Whether the adopted outcome describes a previous analysis generation: a re-Analyze has since
// been adopted into the share list, so the map on show is of the previous scene version and is
// labelled as such until its replacement arrives (AC5's stale-baseline rule).
bool MapStaleGenerationFor(const MapQuerySession& map, unsigned long long current_generation);

// ---- The executor ---------------------------------------------------------------------------------

class SurveyExecutor {
 public:
  // A finished query, drained by the GUI. `result` is null iff `ok` is false, in which case
  // `error` carries the engine's "<reason>: <detail>" text (a refusal the gate may still adopt —
  // it is a legitimate answer for a multi-layer selection). `face` says which query face
  // finished; each face drains its own outcomes through its own slot.
  struct Finished {
    SurveyQueryId id;
    QueryFace face = QueryFace::kSurvey;
    bool ok = false;
    std::string error;
    SurveyHandle result;
    MapHandle map_result;  // set iff face == kMap && ok
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
  // while a query reads them (any in-flight query, of either face, is cancelled first, and its
  // outcome drops in the gate because the intent died with the selection). Null scene is
  // ignored. GUI thread.
  void SetScene(std::unique_ptr<LUMICE_Scene, SceneDeleter> scene);

  // Issues `id` against the current binding on the SURVEY face. The request is rebound to
  // executor-owned copies of `faces`/`counts` (the C struct holds raw pointers, so the buffers
  // must outlive the query — the job owns them). Supersedes any in-flight SURVEY by cancelling
  // it (R6: 新意图取代旧) and replacing whatever survey is queued; a map query running or queued
  // is left alone (cross-face is queueing, not supersession — the face enum's comment). An id
  // that was never handed out by NextId is a caller bug.
  void IssueQuery(SurveyQueryId id, const LUMICE_PathSurveyRequest& request, std::vector<int> faces,
                  std::vector<int> counts);

  // The map face's sibling issue (742.5): same binding, same worker, same buffer ownership —
  // `faces`/`counts`/`wavelengths` move into the job. Supersedes any in-flight MAP and replaces
  // whatever map is queued; a survey query running or queued is left alone.
  void IssueMapQuery(SurveyQueryId id, const LUMICE_PathMapRequest& request, std::vector<int> faces,
                     std::vector<int> counts, std::vector<double> wavelengths);

  // Cancels the in-flight query OF THIS FACE (if it is the one running) and drops its queued
  // job (if any). The other face's query is untouched. The finished outcome of a cancelled query
  // may still arrive; the intent invalidation on the caller's side is what makes the gate drop
  // it. GUI thread.
  void CancelInFlight(QueryFace face);

  // Destroys the scene binding on the worker thread (document switches). Any in-flight query,
  // of either face, is cancelled first, and both faces' queued queries are dropped (a queued
  // query runs against nothing). After this, an issued query fails at workspace creation and the
  // refusal text says so — which only matters if the caller also kept the intent, and
  // ResetFrontendState never does (it clears the session with the binding). GUI thread.
  void ReleaseScene();

  // Takes the newest finished query OF THIS FACE, or nullopt. One slot per face: a newer finish
  // overwrites an older one of the same face that the GUI has not drained — sound because the
  // older one's intent was necessarily superseded before a newer query of the same face could
  // run (see the class comment). GUI thread.
  std::optional<Finished> TakeFinished(QueryFace face);

  // Whether a query is COMPUTING on `face` (picked up, not yet finished — the worker's own
  // registration; a queued-but-unpicked query reads false). Test/observability face — the GUI
  // never branches on it (the display state is intent + outcome, not machinery).
  bool InFlight(QueryFace face) const;

  // Whether ANY issued query is still owed an answer — computing or queued, either face. The
  // teardown decisions (the Revert row's "cancel what is out") branch on this, not on InFlight:
  // a queued query withdrawn by a reset must take its latch with it, exactly like a running one.
  bool QueryOutstanding() const;

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
