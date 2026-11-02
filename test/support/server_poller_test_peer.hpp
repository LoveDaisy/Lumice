#ifndef LUMICE_TEST_SUPPORT_SERVER_POLLER_TEST_PEER_HPP
#define LUMICE_TEST_SUPPORT_SERVER_POLLER_TEST_PEER_HPP

#include <chrono>
#include <mutex>

#include "gui/server_poller.hpp"

namespace lumice::gui {

// Observes real worker state without manufacturing it. Holding the publication lock parks
// PollOnce before GetDrainStatus, which still dereferences the server wrapper after publishing.
struct ServerPollerTestPeer {
  static std::unique_lock<std::mutex> HoldPublication(ServerPoller& poller) {
    return std::unique_lock<std::mutex>(poller.publish_mutex_);
  }

  static bool WaitForActive(ServerPoller& poller, bool paused) {
    std::unique_lock<std::mutex> lock(poller.mutex_);
    return poller.cv_.wait_for(lock, std::chrono::seconds(5), [&] {
      return poller.active_ && (!paused || poller.state_.load() == ServerPoller::State::kPaused);
    });
  }

  static bool Active(ServerPoller& poller) {
    std::lock_guard<std::mutex> lock(poller.mutex_);
    return poller.active_;
  }
};

}  // namespace lumice::gui

#endif  // LUMICE_TEST_SUPPORT_SERVER_POLLER_TEST_PEER_HPP
