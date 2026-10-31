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
//
// The liveness snapshot (LastPolledSite/TotalPolls) rides the same NotePoll writes: the last
// site polled and the total poll count across all sites. It is evidence, not ordering — a
// reader may see the tick count move before the last-site store lands, so the pair is "the
// last known state", never an atomic phase transition. LastPolledSite is the data source of
// the poll-stage snapshot the raypath layer curates (query_control.hpp's StageOfSite); it
// never clears, so a pause between polls reads as the last reached stage, not as idle.

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
    // The liveness snapshot's two writes, same relaxed evidence discipline: last_site_ is the
    // active stage's data source, total_polls_ the monotonic tick (fetch_add only, so readers
    // across threads never see it decrease — that monotonicity is structural, not enforced).
    last_site_.store(site, std::memory_order_relaxed);
    total_polls_.fetch_add(1, std::memory_order_relaxed);
  }
  long PollCount(int site) const { return polls_[static_cast<size_t>(site)].load(std::memory_order_relaxed); }

  // The liveness snapshot (read from any thread): the last polled site (-1 before the first
  // poll — never a valid site id, so "no poll yet" and "site 0" stay distinct) and the total
  // poll count across all sites. The stage grouping over these two reads is the raypath
  // layer's curation (query_control.hpp), not this header's concern.
  int LastPolledSite() const { return last_site_.load(std::memory_order_relaxed); }
  long TotalPolls() const { return total_polls_.load(std::memory_order_relaxed); }

  static constexpr int kSiteCount = 32;

 private:
  std::atomic<bool> cancelled_{ false };
  mutable std::atomic<long> polls_[kSiteCount]{};
  mutable std::atomic<int> last_site_{ -1 };
  mutable std::atomic<long> total_polls_{ 0 };
};

// Why a stop fired. The legacy volatile flag is an explicit cancel (the signal entry's
// documented semantics), so it reports kCancelled, not a third word; deadline exhaustion is
// its own value because the query outcome axis keeps them distinct. When both fired, the
// explicit intent outranks the timer.
enum class StopReason { kNone, kCancelled, kDeadline };

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_STOP_HPP_
