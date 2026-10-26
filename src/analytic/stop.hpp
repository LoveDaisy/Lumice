#ifndef LUMICE_ANALYTIC_STOP_HPP_
#define LUMICE_ANALYTIC_STOP_HPP_

// The cooperative stop authority shared by every long loop in the analytic kernels and the
// raypath query faces above them: a one-way, thread-safe cancellation token with a poll
// ledger, and the reason vocabulary every stop point reports. One judgment object — the
// kernels' deadline + this token + the legacy volatile flag fold into a single Stopped()/Why()
// the loops read, so no polling site re-derives "should I stop" from its own inputs.
//
// The token is one-way by construction (no reset — an ABA between two queries sharing a token
// is unrepresentable), lock-free to read, and safe to RequestCancel from any thread while a
// borrower loop is inside a poll-free region; the loop observes the request at its next poll.
// It lives at this layer so the kernels can consume it without depending on raypath.
//
// The poll ledger (NotePoll/PollCount) is the coverage evidence and the handshake basis: a
// stop point is real exactly when its site's counter moves during a run, and a test that
// waits for a counter to move has proof the loop entered before it cancels — no sleeps, no
// pre-armed flags. Sites are small dense ints; this header owns the range split (kernels
// below 16, raypath 16 and up) and each module declares its own constants.

#include <atomic>
#include <cassert>

namespace lumice::analytic {

// Thread-safe one-way cancellation with a poll ledger.
class StopToken {
 public:
  // Any thread. One-way: the first call wins and sticks; later calls are no-ops.
  void RequestCancel() { cancelled_.store(true, std::memory_order_relaxed); }
  bool CancelRequested() const { return cancelled_.load(std::memory_order_relaxed); }

  // Called by polling sites only; `site` in [0, kSiteCount). Relaxed: the ledger is evidence,
  // not ordering — the loop's own next read of CancelRequested() is what acts on the request.
  // The assert is the guard rail for the site constants' growth: a 33rd site fails loudly in
  // every debug/test build instead of writing out of bounds silently in release.
  void NotePoll(int site) const {
    assert(site >= 0 && site < kSiteCount);
    polls_[static_cast<size_t>(site)].fetch_add(1, std::memory_order_relaxed);
  }
  long PollCount(int site) const { return polls_[static_cast<size_t>(site)].load(std::memory_order_relaxed); }

  static constexpr int kSiteCount = 32;

 private:
  std::atomic<bool> cancelled_{ false };
  mutable std::atomic<long> polls_[kSiteCount]{};
};

// Why a stop fired. The legacy volatile flag is an explicit cancel (the signal entry's
// documented semantics), so it reports kCancelled, not a third word; deadline exhaustion is
// its own value because the query outcome axis keeps them distinct. When both fired, the
// explicit intent outranks the timer.
enum class StopReason { kNone, kCancelled, kDeadline };

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_STOP_HPP_
