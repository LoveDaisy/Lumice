#ifndef UTIL_QUEUE_H_
#define UTIL_QUEUE_H_

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <utility>
#include <vector>

namespace lumice {

template <class T>
class Queue {
 public:
  T Get() {
    std::unique_lock lock(q_mutex_);
    if (shutdown_) {
      return T();
    }

    if (q_.empty()) {
      q_cv_.wait(lock, [=]() { return !q_.empty() || shutdown_; });  // Block
    }
    if (shutdown_) {
      return T();
    }

    T e = std::move(q_.front());
    q_.pop();
    return e;
  }

  // Non-blocking emptiness check. Used by the device-fused XYZ drain cadence
  // (scrum-312: third-clock decoupling) to flush the accumulation window when
  // the producer pauses. TOCTOU is benign here — a batch arriving right after
  // the check just drains one window later.
  bool Empty() {
    std::unique_lock lock(q_mutex_);
    return q_.empty();
  }

  // Removes every element still waiting and hands them to the caller in FIFO order.
  // Deliberately NOT Shutdown(): the queue stays open, a blocked Get() stays blocked,
  // and nothing about the queue's lifecycle changes — this only says "these particular
  // items are no longer wanted". Its one caller is the server's producer, which drops
  // the batches it queued at the GPU dispatch grain when the backend is dropped
  // mid-run and re-emits the same budget at the legacy grain; the caller is the one
  // that owns whatever accounting those items carried (see the server's
  // DiscardQueuedBatchesThenRefund).
  std::vector<T> DrainAll() {
    std::unique_lock lock(q_mutex_);
    std::vector<T> drained;
    drained.reserve(q_.size());
    while (!q_.empty()) {
      drained.push_back(std::move(q_.front()));
      q_.pop();
    }
    return drained;
  }

  template <class... Args>
  void Emplace(Args&&... args) {
    std::unique_lock lock(q_mutex_);
    if (shutdown_) {
      return;
    }
    q_.emplace(std::forward<Args>(args)...);
    q_cv_.notify_one();
  }

  // Enqueues every element of `items` (in order) under ONE lock acquisition and
  // wakes the waiters once, instead of one lock + one notify per element. A
  // no-op for an empty vector. This is what lets a producer hand over a burst
  // without paying the per-element wakeup: on a condvar with waiters coming and
  // going, each notify is a futex syscall, and on a virtualised scheduler (WSL2)
  // that syscall is what a producer running one wakeup per small item ends up
  // spending its whole thread on. `items` is left empty.
  void EmplaceMany(std::vector<T>& items) {
    if (items.empty()) {
      return;
    }
    std::unique_lock lock(q_mutex_);
    if (!shutdown_) {
      for (auto& e : items) {
        q_.emplace(std::move(e));
      }
      if (items.size() == 1) {
        q_cv_.notify_one();
      } else {
        q_cv_.notify_all();
      }
    }
    items.clear();
  }

  void Shutdown() {
    std::unique_lock<std::mutex> lock(q_mutex_);
    if (shutdown_) {
      return;
    }

    std::queue<T> tmp_q;
    q_.swap(tmp_q);
    shutdown_ = true;
    q_cv_.notify_all();
  }

  void Start() {
    std::unique_lock<std::mutex> lock(q_mutex_);
    shutdown_ = false;
    q_cv_.notify_all();
  }

 private:
  std::queue<T> q_;  // Just simple example
  std::mutex q_mutex_;
  std::condition_variable q_cv_;
  bool shutdown_ = false;
};

template <class T>
using QueuePtrU = std::unique_ptr<Queue<T>>;
template <class T>
using QueuePtrS = std::shared_ptr<Queue<T>>;

}  // namespace lumice

#endif  // UTIL_QUEUE_H_
