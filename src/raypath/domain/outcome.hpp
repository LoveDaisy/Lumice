#ifndef LUMICE_RAYPATH_DOMAIN_OUTCOME_HPP_
#define LUMICE_RAYPATH_DOMAIN_OUTCOME_HPP_

// The execution outcome axis: the four-outcome vocabulary a result's meta carries. It belongs
// to the RESULT layer (a pure value vocabulary — the value headers include nothing but this),
// while its derivation (OutcomeOf, from the stop authority's reason) stays with the budget/
// stop owner in query_control.hpp.

namespace lumice::raypath {

enum class ExecutionOutcome { kCompleted, kCancelled, kDeadlineExceeded };

inline const char* ExecutionOutcomeName(ExecutionOutcome outcome) {
  switch (outcome) {
    case ExecutionOutcome::kCompleted:
      return "completed";
    case ExecutionOutcome::kCancelled:
      return "cancelled";
    case ExecutionOutcome::kDeadlineExceeded:
      return "deadline_exceeded";
  }
  return "unknown_execution_outcome";
}

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_DOMAIN_OUTCOME_HPP_
