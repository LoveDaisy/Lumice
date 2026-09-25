#ifndef LUMICE_GUI_SIM_STATE_RULES_HPP
#define LUMICE_GUI_SIM_STATE_RULES_HPP

// What the simulation lifecycle state permits, as data rather than as an inline `if` at each
// widget.
//
// Every predicate here answers one question — "given only the lifecycle state (and, for the save
// modal, whether a server exists), is this command available?" — and answers it from its
// arguments alone: no GuiState, no globals, no ImGui. That is what lets a test enumerate
// SimState's five values against each predicate instead of driving a window to reach one branch
// at a time; the call sites in app_panels.cpp then read `ImGui::BeginDisabled(IsBusy(...))`
// rather than restating the comparison.
//
// The extraction is behaviour-preserving by construction: each function body is the expression
// that stood at its call site verbatim. `CanRunFromModal` is the one that gained a dependency —
// it now spells its "no run in flight" half as `!IsBusy(state)`, which is the same predicate the
// top bar uses. That is deliberate and is what the call site's own comment asked for ("matches
// the top-bar Run button gating semantics; single-source would be nicer but the top bar's enable
// predicate is inlined and not exported"). It is NOT a merge of the two gates: the modal keeps
// its own named predicate, because it also requires a live server and the top bar does not.

#include "gui/gui_state.hpp"

namespace lumice::gui {

// A run is in flight.
inline bool IsSimulating(GuiState::SimState state) {
  return state == GuiState::SimState::kSimulating;
}

// An async Stop is still draining in the backend.
inline bool IsStopping(GuiState::SimState state) {
  return state == GuiState::SimState::kStopping;
}

// The backend is unavailable for a document-level command. Wider than "simulating": New / Open /
// Save must also stay shut while a Stop drains, or they would act on a backend mid-teardown.
inline bool IsBusy(GuiState::SimState state) {
  return IsSimulating(state) || IsStopping(state);
}

// The backend is unavailable for a document-level command OR for a render commit, counting the
// analysis run beside the render run. The two runs share one server and exclude each other at the
// C API (LUMICE_StartRaypathAnalysis / LUMICE_CommitScene each return LUMICE_ERR_SERVER while the
// other is in progress), and an analysis run is deliberately NOT a SimState value: sim_state
// answers "what does the picture on screen reflect", and an analysis changes nothing about the
// picture. So the top bar's Run / New / Open, and the modal's "Run first", take the analysis flag
// as a second argument rather than reading it off a sixth enum value. `analysis_in_progress` is
// GuiState::analysis_run_in_progress, derived each frame by SyncFromPoller.
inline bool IsBackendBusy(GuiState::SimState state, bool analysis_in_progress) {
  return IsBusy(state) || analysis_in_progress;
}

// The config has changed since the last run, so the on-screen preview no longer reflects it.
// Drives both the ⚠ + Revert affordance and the Save-Modified popup.
inline bool IsModified(GuiState::SimState state) {
  return state == GuiState::SimState::kModified;
}

// "Run first" in the Save-Modified popup: meaningful only when there is a live server to run on
// AND no run — render or analysis — is already in flight.
inline bool CanRunFromModal(bool has_server, GuiState::SimState state, bool analysis_in_progress) {
  return has_server && !IsBackendBusy(state, analysis_in_progress);
}

// The document has had a picture made of it at least once — a run started, completed or stopped,
// or an .lmc opened with its baked texture. kNone is the one intent that says it has not: a fresh
// document, New, a JSON import, an .lmc with no baked picture. Read off the intent rather than
// off the preview's own "is a texture bound" flag on purpose: that flag is also raised by a
// background photograph alone, which is not a picture of the document.
inline bool HasEverShownPicture(RunIntent intent) {
  return intent != RunIntent::kNone;
}

// The panel's Analyze button. An analysis submits the document on the panels itself
// (DoAnalyze builds the scene the way DoRun does and hands it to LUMICE_StartRaypathAnalysis), so
// it needs no picture that MATCHES: a loaded .lmc that was never run and an edited (kModified)
// document both analyse — the list describes the document as it was when Analyze was pressed
// (AnalysisPictureNotice below says when the picture on screen is not of that document;
// ComputeAnalysisListFreshness says when the document has since moved away from it). What it
// does need, besides a backend with nothing in flight (the C API's mutual exclusion, surfaced as
// a disabled button rather than as an error line after the click), is a document that has shown
// a picture at least once: analysing a scene the user has never seen rendered reads as a request
// out of nowhere, so a fresh / New / JSON-imported / blank-.lmc document waits for its first Run.
// The In frame and Point region radios read the same predicate, so the three cannot disagree on
// what "no picture yet" means.
inline bool CanStartAnalysis(bool has_server, GuiState::SimState state, bool analysis_in_progress, RunIntent intent) {
  return has_server && !IsBackendBusy(state, analysis_in_progress) && HasEverShownPicture(intent);
}

// The top bar's Continue button (LUMICE_ContinueRender: trace more rays into the picture on
// screen instead of starting over). Why it cannot run now, as the first reason that applies, or
// kNone. One function answers both "enabled?" (CanContinue) and "why not?" (the tooltip), so the
// two cannot drift apart.
//   kNoServer         — no backend.
//   kBusy             — a run (render or analysis) is in flight or a Stop is draining; a
//                       continuation starts a run, so it waits exactly as Run does.
//   kNoAccumulation   — the picture on screen is not the server's live accumulation of a render:
//                       nothing was run yet (kNone), or it came out of a file (kLoaded). Only a
//                       render that completed or was stopped leaves one (kRunCompleted / kStopped).
//   kAnalysisSession  — the server's session is a raypath analysis; its consumers hold a
//                       histogram, and the render accumulation it replaced is gone. The server's
//                       own session kind, read back through the poller — not the panel's
//                       analysis.started intent, which a Stop withdraws while the session stays.
//   kConfigChanged    — the document no longer describes what was accumulated: a re-sim field
//                       moved (MatchesCommitExceptRayBudget), or a construction-time property
//                       (backend, worker count) did, which a Run would rebuild the server for.
//                       The ray budget is exempt — it is what Continue reads, as the increment.
// Display-only edits (EV, overlays, the view) reach none of these, so they never block it.
enum class ContinueBlocker { kNone, kNoServer, kBusy, kNoAccumulation, kAnalysisSession, kConfigChanged };

inline ContinueBlocker WhyCannotContinue(bool has_server, GuiState::SimState state, bool analysis_in_progress,
                                         RunIntent intent, bool server_session_is_analysis, bool document_continuable) {
  if (!has_server) {
    return ContinueBlocker::kNoServer;
  }
  if (IsBackendBusy(state, analysis_in_progress)) {
    return ContinueBlocker::kBusy;
  }
  if (intent != RunIntent::kRunCompleted && intent != RunIntent::kStopped) {
    return ContinueBlocker::kNoAccumulation;
  }
  if (server_session_is_analysis) {
    return ContinueBlocker::kAnalysisSession;
  }
  if (!document_continuable) {
    return ContinueBlocker::kConfigChanged;
  }
  return ContinueBlocker::kNone;
}

inline bool CanContinue(bool has_server, GuiState::SimState state, bool analysis_in_progress, RunIntent intent,
                        bool server_session_is_analysis, bool document_continuable) {
  return WhyCannotContinue(has_server, state, analysis_in_progress, intent, server_session_is_analysis,
                           document_continuable) == ContinueBlocker::kNone;
}

// The disabled button's tooltip for each blocker; nullptr for kNone (the enabled button has its own).
inline const char* ContinueBlockerTooltip(ContinueBlocker blocker) {
  switch (blocker) {
    case ContinueBlocker::kNone:
      return nullptr;
    case ContinueBlocker::kNoServer:
      return "No simulation backend.";
    case ContinueBlocker::kBusy:
      return "A run is in progress. Continue adds rays once it has completed or been stopped.";
    case ContinueBlocker::kNoAccumulation:
      return "Nothing to continue yet \xe2\x80\x94 press Run first.";
    case ContinueBlocker::kAnalysisSession:
      return "The last run was a raypath analysis. Press Run to render again; Continue can then add rays to it.";
    case ContinueBlocker::kConfigChanged:
      return "The configuration changed since the last run, so more rays would not match the picture.\n"
             "Press Run to start over with the new configuration, or Revert the changes.";
  }
  return nullptr;
}

// The panel's notice that the picture on screen is not of the document on the panels — the text
// to show, or nullptr when there is nothing to say. Two cases and they cannot both hold: an
// intent of kNone (fresh / New / a JSON import / an .lmc with no baked picture) means no picture
// of this document was ever made, and it reconciles to kIdle, which dirty never lifts to
// kModified; kModified means there is a picture, of the configuration before the edit. A notice,
// not a refusal — though the kNone case is now also the one CanStartAnalysis refuses
// (HasEverShownPicture), so that line is read beside a disabled button. It says nothing about the
// LIST — whether the list still describes the document is a different question, answered by
// ComputeAnalysisListFreshness below from a comparison this function does not have.
inline const char* AnalysisPictureNotice(RunIntent intent, GuiState::SimState state) {
  if (intent == RunIntent::kNone) {
    return "No rendered image for this document yet \xe2\x80\x94 press Run once before analysing.";
  }
  if (IsModified(state)) {
    return "Image is from a previous configuration.";
  }
  return nullptr;
}

// Whether the analysis list on show still describes the document on the panels. kNone: there is
// no list (no payload adopted — before the first Analyze, or while one is in flight after
// DoAnalyze cleared the previous view). Otherwise fresh iff the document still matches the scene
// the result was captured from, stale iff it does not — whichever way it got there: an edit, a
// Run that committed one, a Revert that restored the previous commit. One comparison, evaluated
// each frame, is what makes those three the same case instead of three call sites.
//
// The caller computes both inputs — `has_result` = analysis_result.payload != nullptr,
// `scene_still_matches` = analysis_result.analyzed_scene && analyzed_scene->Matches(state) — so
// this stays a function of two bools a test can enumerate, like everything else in this file.
enum class AnalysisListFreshness { kNone, kFresh, kStale };

inline AnalysisListFreshness ComputeAnalysisListFreshness(bool has_result, bool scene_still_matches) {
  if (!has_result) {
    return AnalysisListFreshness::kNone;
  }
  return scene_still_matches ? AnalysisListFreshness::kFresh : AnalysisListFreshness::kStale;
}

}  // namespace lumice::gui

#endif  // LUMICE_GUI_SIM_STATE_RULES_HPP
