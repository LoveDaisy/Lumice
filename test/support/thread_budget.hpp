#ifndef LUMICE_TEST_SUPPORT_THREAD_BUDGET_HPP_
#define LUMICE_TEST_SUPPORT_THREAD_BUDGET_HPP_

// The thread budget test code hands to the row-parallel W*H loops.
//
// ParallelRows, BuildVisibleMask, mask_detail::LevelSetMaskFromField, annotation::ComputeOverlay
// and the RenderConsumer constructor all take an explicit `int thread_budget` with no default
// (core/parallel_rows.hpp): in production the server computes it as the machine's idle cores —
// hardware_concurrency() minus its simulation workers — because those loops compete with the
// workers for the same physical cores, and a default would be exactly the value nobody reasoned
// about. Test code has no worker pool to leave cores for, and what it checks is that the serial
// and parallel paths produce the same bytes, not how they contend; so it passes the
// pre-budget unconstrained value, hardware_concurrency(), which keeps every existing test and
// benchmark on the same path it ran before the budget existed. A test that is ABOUT the budget
// (forced-serial vs parallel, the server's formula) passes its own explicit number instead.
//
// A constant rather than a function so the call sites read as a value, and `inline` so the one
// definition is shared by every TU that includes it. It lives in test/support/ for the same
// reason env_var.hpp does: its consumers span several CMake targets, all of which carry
// ${PROJ_TEST_DIR} on their include path — except lumice_testapi, which includes it by its
// same-directory spelling.

#include <thread>

namespace lumice::test {

inline const int kTestThreadBudget = static_cast<int>(std::thread::hardware_concurrency());

}  // namespace lumice::test

#endif  // LUMICE_TEST_SUPPORT_THREAD_BUDGET_HPP_
