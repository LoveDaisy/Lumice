// The dispatch rule of ParallelRows (core/parallel_rows.hpp), stated on the function itself rather
// than through a mask builder: which calls run the body inline on the calling thread, which stand
// up a pool, and — either way — that the bands the body receives tile [0, height) exactly once.
//
// The rule has three inline gates (frame below the pixel threshold, a single row, thread budget
// below 2) and this file exercises the third one directly, because it is the one a caller sets
// per call: a server with no idle cores passes 0 and must get the serial loop, not a one-thread
// pool. The other two gates are covered here only as far as the budget does not override them —
// a large budget on a small frame is still inline.
//
// "Inline" is observed as the body running on the calling thread, exactly once, over the whole
// range; "parallel" as the body running on at least one thread that is not the caller. Whether
// the parallel result matches the serial one byte-for-byte is the mask builders' contract and is
// held by their own tests; this file is about the dispatch decision.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "core/parallel_rows.hpp"

namespace lumice {
namespace {

// Above kParallelPixelThreshold (65536) with room to spare, and tall enough that every worker of
// a large pool gets a band.
constexpr int kBigHeight = 512;
constexpr size_t kBigPixels = static_cast<size_t>(kBigHeight) * 512u;
static_assert(kBigPixels > 65536u, "must clear ParallelRows' pixel threshold");
constexpr int kSmallHeight = 64;
constexpr size_t kSmallPixels = static_cast<size_t>(kSmallHeight) * 64u;
static_assert(kSmallPixels < 65536u, "must sit below ParallelRows' pixel threshold");

struct Observation {
  std::vector<std::pair<int, int>> bands;
  std::vector<std::thread::id> threads;
};

Observation Dispatch(int height, size_t pixel_count, int thread_budget) {
  Observation obs;
  std::mutex m;
  ParallelRows(height, pixel_count, thread_budget, [&](int begin, int end) {
    std::lock_guard<std::mutex> lock(m);
    obs.bands.emplace_back(begin, end);
    obs.threads.push_back(std::this_thread::get_id());
  });
  return obs;
}

// The bands tile [0, height) exactly once: sorted by start, contiguous, no gap and no overlap.
void ExpectTiles(const Observation& obs, int height) {
  auto bands = obs.bands;
  std::sort(bands.begin(), bands.end());
  ASSERT_FALSE(bands.empty());
  EXPECT_EQ(bands.front().first, 0);
  EXPECT_EQ(bands.back().second, height);
  for (size_t i = 1; i < bands.size(); ++i) {
    EXPECT_EQ(bands[i].first, bands[i - 1].second)
        << "band " << i << " does not start where band " << i - 1 << " ended";
  }
  for (const auto& [b, e] : bands) {
    EXPECT_LT(b, e);
  }
}

void ExpectInlineOnCaller(const Observation& obs, int height) {
  ASSERT_EQ(obs.bands.size(), 1u) << "an inline dispatch hands the body the whole range in one call";
  EXPECT_EQ(obs.bands[0], std::make_pair(0, height));
  EXPECT_EQ(obs.threads[0], std::this_thread::get_id()) << "an inline dispatch runs on the calling thread";
}

TEST(ParallelRows, BudgetOfZeroRunsInlineOnALargeFrame) {
  const Observation obs = Dispatch(kBigHeight, kBigPixels, /*thread_budget=*/0);
  ExpectInlineOnCaller(obs, kBigHeight);
}

TEST(ParallelRows, BudgetOfOneRunsInlineNotAOneThreadPool) {
  const Observation obs = Dispatch(kBigHeight, kBigPixels, /*thread_budget=*/1);
  ExpectInlineOnCaller(obs, kBigHeight);
}

TEST(ParallelRows, NegativeBudgetRunsInline) {
  // A caller that computed hw − workers without the max(0, ·) clamp must still get the serial
  // loop rather than a pool construction with a negative size.
  const Observation obs = Dispatch(kBigHeight, kBigPixels, /*thread_budget=*/-3);
  ExpectInlineOnCaller(obs, kBigHeight);
}

TEST(ParallelRows, BudgetOfTwoOnALargeFrameGoesParallelAndTilesTheRange) {
  const Observation obs = Dispatch(kBigHeight, kBigPixels, /*thread_budget=*/2);
  ExpectTiles(obs, kBigHeight);
  const auto me = std::this_thread::get_id();
  const bool any_other_thread = std::any_of(obs.threads.begin(), obs.threads.end(), [&](auto id) { return id != me; });
  EXPECT_TRUE(any_other_thread) << "a budget of 2 on a frame above the threshold must stand up a pool";
  // The pool was sized to the budget, not to the machine: at most `budget` distinct threads ran
  // bands. (A pool of 2 hands out 2 bands, one per worker; the count is checked as an upper
  // bound rather than an exact 2 so the assertion does not encode ThreadingPool's slicing.)
  std::vector<std::thread::id> ids = obs.threads;
  std::sort(ids.begin(), ids.end());
  ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
  EXPECT_LE(ids.size(), 2u) << "more distinct threads ran bands than the budget allows";
}

TEST(ParallelRows, LargeBudgetOnASmallFrameStaysInline) {
  // The budget admits parallelism; the pixel threshold still says the frame is too small for a
  // pool to pay for itself. The gates compose as AND-for-parallel, OR-for-inline.
  const Observation obs = Dispatch(kSmallHeight, kSmallPixels, /*thread_budget=*/64);
  ExpectInlineOnCaller(obs, kSmallHeight);
}

TEST(ParallelRows, SingleRowStaysInlineWhateverTheBudget) {
  const Observation obs = Dispatch(/*height=*/1, kBigPixels, /*thread_budget=*/64);
  ExpectInlineOnCaller(obs, 1);
}

TEST(ParallelRows, NonPositiveHeightNeverCallsTheBody) {
  EXPECT_TRUE(Dispatch(0, kBigPixels, 64).bands.empty());
  EXPECT_TRUE(Dispatch(-1, kBigPixels, 64).bands.empty());
}

}  // namespace
}  // namespace lumice
