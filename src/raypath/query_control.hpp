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
inline constexpr int kPollSurveyT0 = 16;           // survey assembly, per member verdict
inline constexpr int kPollStructureMember = 17;    // class support, per member field+axis+partition
inline constexpr int kPollProbeLambdas = 18;       // probe, per-lambda boundary
inline constexpr int kPollPointLegs = 19;          // point discovery legs' boundaries
inline constexpr int kPollMcEssDraws = 20;         // survey MC ESS leg, per draw chunk
inline constexpr int kPollEnumerationMember = 21;  // report enumeration, per member
inline constexpr int kPollSamplerDraws = 23;       // the diagnostic sampler's draw loops (survey MC ESS
                                                   // and report discovery — never both in one query)

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
