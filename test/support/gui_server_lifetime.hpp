#ifndef LUMICE_TEST_SUPPORT_GUI_SERVER_LIFETIME_HPP
#define LUMICE_TEST_SUPPORT_GUI_SERVER_LIFETIME_HPP

#include "gui/app.hpp"

namespace lumice::test {

// The owner-thread prefix for test teardown/reset, before stopping the poller or touching
// its server. A paused poller does not imply that DoStop's async server call has returned.
inline void JoinGuiServerTasksForTeardown() {
  gui::JoinPendingStop();
  gui::JoinPendingCalibration();
}

}  // namespace lumice::test

#endif  // LUMICE_TEST_SUPPORT_GUI_SERVER_LIFETIME_HPP
