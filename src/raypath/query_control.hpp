#ifndef LUMICE_RAYPATH_QUERY_CONTROL_HPP_
#define LUMICE_RAYPATH_QUERY_CONTROL_HPP_

// The one owner of deadline resolution for the raypath queries. Every window the four query
// faces install passes through here, in both policies:
//   - the legacy three-window policy (the S0 baseline, ruling 1): each leg gets its own start
//     and its own budget — the point/probe discovery stop, the report's discovery quality
//     window, the report enumeration's anti-hang cap, and the survey MC ESS leg's guardrail —
//     and nothing shares a parent deadline;
//   - the unified query policy (the new contract, first observable here): one absolute
//     deadline resolved once at entry; every leg clamps to it, and a leg started later never
//     gains budget, because a leg's stop derives from the query's point in time, never from
//     its own start.
// The resolution sites are enumerable by construction: this header's callers are the whole
// set, and a deadline constructed outside this file is a policy fork. What deliberately stays
// OUT: request-shape validation (an entry's own refusal — survey/report's budget_ms <= 0 is
// request validation, not deadline resolution), the C ABI's 0-to-default field translation
// (the bridge's contract, lumice_raypath.h's documented defaults), and stop-reason semantics
// (the carrier stays analytic's DiscoveryStop / SamplingBudget; outcomes are the result
// layer's vocabulary).

#include <chrono>
#include <optional>

#include "analytic/discovery.hpp"
#include "raypath/domain/outcome.hpp"  // ExecutionOutcome — the result layer's vocabulary

namespace lumice::raypath {

// Which window a legacy resolution installs. The window alone fixes the zero handling — the
// four kinds' documented 0-semantics differ, and the difference lives here, not at the calls.
enum class BudgetWindow {
  kPointDiscovery,   // point's discovery stop: 0 = no deadline (echoed 0); >0 = start + budget
  kProbeLambdas,     // probe's per-lambda boundary stop: 0 = the documented default, echoed resolved
  kMapMembers,       // map's per-member boundary stop: 0 = the documented default, echoed resolved
  kQualityDeadline,  // report's discovery quality window: the caller's absolute deadline, else start + budget
  kHangCap,          // report enumeration's anti-hang floor: start + the fixed cap, independent of any budget
  kLegGuardrail,     // survey's opt-in MC ESS leg: start + the survey request's budget, verbatim
};

// What a resolution reads. `start` defaults to now() at the call — point/probe/MC ESS/hang cap
// resolve at their leg's entry; the report's quality window resolves against its own earlier
// `begin` and passes it explicitly.
struct BudgetRequest {
  int budget_ms = 0;  // as the caller wrote it; the window's own rule reads the 0
  const volatile int* interrupted = nullptr;
  std::optional<std::chrono::steady_clock::time_point> absolute_deadline;  // the report C entry pins one
  std::optional<std::chrono::steady_clock::time_point> start;
};

// One legacy leg's resolved control: the stop the leg polls, plus the echo value the
// document's budget receipt reads back.
struct LegControl {
  analytic::DiscoveryStop stop;
  int resolved_ms = 0;
};

// Resolves one legacy leg's control. Call exactly once per leg per query: the per-window
// resolution counters are the runtime read the tests pin (one per leg per query, and a query
// face's run moves no other window's counter).
LegControl ResolveLeg(BudgetWindow window, const BudgetRequest& request);

// The unified query's single absolute deadline (the new contract). Resolved once per query:
// a positive budget is its span from entry, a caller-pinned absolute deadline wins outright,
// and otherwise the query runs unbounded — an explicit request on this path, never a silent
// default (the legacy path's defaults stay in the legacy windows).
struct QueryDeadline {
  std::chrono::steady_clock::time_point at = std::chrono::steady_clock::time_point::max();
  const volatile int* interrupted = nullptr;
  int resolved_ms = 0;

  // Every leg clamps to the query deadline. Deriving from `at` (never from a leg's own start)
  // is what makes "a later leg gains no budget" structural rather than policed.
  analytic::DiscoveryStop LegStop() const { return { at, interrupted }; }
};

QueryDeadline ResolveQueryDeadline(const BudgetRequest& request);

// Runtime reads: resolutions this process installed, per window and for the unified path.
// One query's run must move its own windows' counters by exactly the leg count and nothing
// else's — the counters exist to make "resolved once, no second resolution" observable, not
// to be production telemetry. Premise: ONE query executes at a time in the process — the
// per-window counts are decisive only without concurrent query executions (the faces run on
// the caller's thread; a future parallel executor owes these reads a per-query ledger).
long BudgetResolutionCount(BudgetWindow window);
long QueryDeadlineResolutionCount();
void ResetBudgetResolutionCountsForTest();

// Poll-ledger site ids the raypath faces own (range 16..31 — analytic/stop.hpp's split). A
// site's counter moving during a run is the evidence its stop point is real; the handshake
// tests wait on exactly these counters before cancelling.
inline constexpr int kPollSurveyT0 = 16;            // survey assembly, per member verdict
inline constexpr int kPollStructureMember = 17;     // class support, per member field+axis+partition
inline constexpr int kPollProbeLambdas = 18;        // probe, per-lambda boundary
inline constexpr int kPollPointLegs = 19;           // point discovery legs' boundaries
inline constexpr int kPollMcEssDraws = 20;          // survey MC ESS leg, per draw chunk
inline constexpr int kPollEnumerationMember = 21;   // report enumeration, per member
inline constexpr int kPollSurveyChromatic = 22;     // survey chromatic pair, at leg entry
inline constexpr int kPollSamplerDraws = 23;        // the diagnostic sampler's draw loops (survey MC ESS
                                                    // and report discovery — never both in one query)
inline constexpr int kPollMapMembers = 24;          // map query, per member
inline constexpr int kPollSurveyTirPass = 25;       // survey TIR share leg, per-lambda pass boundary
inline constexpr int kPollSurveyTirRows = 26;       // survey TIR share leg, the in-phase row-chunk boundary
                                                    // (row rebuild -> batch evaluation -> classification pipeline)
inline constexpr int kPollSurveyWitnessPhase = 27;  // survey witness leg, the post-lattice phase boundary
                                                    // (argmax + the pose's independent re-evaluation)

// ---- The poll-stage curation (the liveness snapshot's vocabulary) --------------------------
//
// The raw ledger's last site + total ticks (analytic/stop.hpp) are engine facts; this table is
// the ONE place that curates them into a user-readable stage and attributes each site to the
// query faces whose runs can fire it. Every consumer (the C ABI's poll snapshot, the GUI's
// wait-line text, the handshake tests' attribution assertions) reads this table — a second
// site-to-stage spelling anywhere else is a policy fork.
//
// Stage semantics: a stage says which kind of work the query was last seen inside. It is NOT a
// progress measure — poll counts count polling events, not remaining work, so there is no
// percentage and no "done" value anywhere in this vocabulary (the honest boundary the C header
// and the GUI text repeat). A quiet stretch between polls reads as the last reached stage.

// The curated stage of a site. Coarse by design — kernel sites fold into the leg they belong
// to, so internal re-polling or a site's renumbering never moves the ABI's vocabulary.
enum class PollStage {
  kNone,              // no poll yet (a fresh token), or an id outside the table
  kSurveyAssembly,    // the survey's T0 routing verdicts (assembly and family arms)
  kStructureSupport,  // per-member structure support (field + walk + partition + row)
  kChromaticLeg,      // the survey chromatic pair leg
  kEssSampling,       // the draw loops' budget judgment (the survey's opt-in MC ESS leg, the
                      // report's discovery legs — never both in one query)
  kTirShare,          // the survey TIR share leg (pass boundaries and in-phase row chunks)
  kWitness,           // the survey witness leg's post-lattice phase
  kEnumeration,       // the report enumeration's per-member anti-hang boundary
  kProbeLambdas,      // the probe's per-lambda boundaries
  kPointLegs,         // the point discovery legs' boundaries
  kMapMembers,        // the map's per-member band sums
  kFiberDiscovery,    // the point discovery's fiber phases (band lattice + pool clusters)
  kBoundaryWalk,      // the structure assembly's boundary walk (march steps + piece rounds)
  kLatticeScan,       // the 1024-sample event-lattice passes (tint/TIR/witness legs, the map's
                      // per-member band sums, the probe's per-lambda band sums)
};

// The query faces a site can fire under (a bit set over these — FacesOfSite's vocabulary).
// The point and probe faces run the survey's context-only assembly when no warm source
// answers, so the assembly's sites attribute to them too; that overlap is measured fact, not
// an accident to normalize away.
enum PollFaceBit {
  kPollFaceSurvey = 1 << 0,
  kPollFaceMap = 1 << 1,
  kPollFacePoint = 1 << 2,
  kPollFaceProbe = 1 << 3,
  kPollFaceReport = 1 << 4,
};

// One site's curated row. `faces` may be 0 with a stage still named: kPollKinkLattice's stage
// is real vocabulary, but no raypath query face passes a stop into WeightKinks today (the
// callers that would fire it all construct KinkOptions{}), so no face claims it — the first
// handshake test that sees it tick fails, pinning the attribution until a face honestly owns it.
struct PollSiteInfo {
  int site;
  PollStage stage;
  unsigned faces;  // an or-combination of PollFaceBit values
};

inline constexpr PollSiteInfo kPollSiteTable[] = {
  { kPollSurveyT0, PollStage::kSurveyAssembly, kPollFaceSurvey | kPollFacePoint | kPollFaceProbe },
  { kPollStructureMember, PollStage::kStructureSupport, kPollFaceSurvey | kPollFacePoint | kPollFaceProbe },
  { kPollProbeLambdas, PollStage::kProbeLambdas, kPollFaceProbe },
  { kPollPointLegs, PollStage::kPointLegs, kPollFacePoint },
  { kPollMcEssDraws, PollStage::kEssSampling, kPollFaceSurvey },  // declared; no call site today
  { kPollEnumerationMember, PollStage::kEnumeration, kPollFaceReport },
  { kPollSurveyChromatic, PollStage::kChromaticLeg, kPollFaceSurvey },
  { kPollSamplerDraws, PollStage::kEssSampling, kPollFaceSurvey | kPollFaceReport },
  { kPollMapMembers, PollStage::kMapMembers, kPollFaceMap },
  { kPollSurveyTirPass, PollStage::kTirShare, kPollFaceSurvey },
  { kPollSurveyTirRows, PollStage::kTirShare, kPollFaceSurvey },
  { kPollSurveyWitnessPhase, PollStage::kWitness, kPollFaceSurvey },
  { analytic::kPollBandLattice, PollStage::kFiberDiscovery, kPollFacePoint },
  { analytic::kPollPoolClusters, PollStage::kFiberDiscovery, kPollFacePoint },
  { analytic::kPollEventsLattice, PollStage::kLatticeScan, kPollFaceSurvey | kPollFaceMap | kPollFaceProbe },
  { analytic::kPollWalkSteps, PollStage::kBoundaryWalk, kPollFaceSurvey | kPollFacePoint | kPollFaceProbe },
  { analytic::kPollWalkPieces, PollStage::kBoundaryWalk, kPollFaceSurvey | kPollFacePoint | kPollFaceProbe },
  { analytic::kPollKinkLattice, PollStage::kLatticeScan, 0u },
};

// The curated stage of a poll site; an id outside the table (including a fresh token's -1)
// reads kNone. Linear scan over 18 rows — a snapshot read runs it once per call, not per ray.
inline PollStage StageOfSite(int site) {
  for (const PollSiteInfo& row : kPollSiteTable) {
    if (row.site == site) {
      return row.stage;
    }
  }
  return PollStage::kNone;
}

// The faces whose runs can fire `site`; an id outside the table reads 0 (no face claims it).
inline unsigned FacesOfSite(int site) {
  for (const PollSiteInfo& row : kPollSiteTable) {
    if (row.site == site) {
      return row.faces;
    }
  }
  return 0u;
}

// The attribution contract as a read (the tests pin it, the same way they pin the resolution
// counters): the first site in [0, kSiteCount) whose counter moved during `token`'s run
// without `faces` claiming it, or -1 when the run stayed inside its attribution. A nonzero
// return is either a table lie or a call site the table never saw — fix the table, never the
// assertion.
inline int FirstUnattributedPollSite(const analytic::StopToken& token, unsigned faces) {
  for (int site = 0; site < analytic::StopToken::kSiteCount; ++site) {
    if (token.PollCount(site) > 0 && (FacesOfSite(site) & faces) == 0u) {
      return site;
    }
  }
  return -1;
}

// The outcome a face records from its stop: completed unless its authority observed a reason.
// (The stop argument is the LAST thing the face polled; a cancel racing the final poll reads
// as completed — the run was over.)
inline ExecutionOutcome OutcomeOf(const analytic::DiscoveryStop& stop) {
  switch (stop.Why()) {
    case analytic::StopReason::kCancelled:
      return ExecutionOutcome::kCancelled;
    case analytic::StopReason::kDeadline:
      return ExecutionOutcome::kDeadlineExceeded;
    case analytic::StopReason::kNone:
      break;
  }
  return ExecutionOutcome::kCompleted;
}

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_QUERY_CONTROL_HPP_
