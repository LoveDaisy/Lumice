#include "core/parallel_rows.hpp"

#include "util/threading_pool.hpp"

namespace lumice {

namespace {

// 256x256. Below this the serial loop is under a millisecond on every machine this runs on, and
// the pool spin-up dominates. The threshold is a cost cut-off, not a correctness boundary: the
// two paths compute the same bytes either side of it.
constexpr size_t kParallelPixelThreshold = 65536;

}  // namespace

void ParallelRows(int height, size_t pixel_count, int thread_budget, const std::function<void(int, int)>& body) {
  if (height <= 0) {
    return;
  }
  // Three reasons to stay on the calling thread, all cost cut-offs rather than correctness
  // boundaries (see the header): the frame is too small for a pool to pay for itself, there is
  // only one row to split, or the caller's idle-core budget cannot seat two workers.
  if (pixel_count < kParallelPixelThreshold || height < 2 || thread_budget < 2) {
    body(0, height);
    return;
  }
  auto pool = ThreadingPool::CreatePool(thread_budget);
  if (!pool || pool->GetPoolSize() < 2) {
    body(0, height);
    return;
  }
  // CommitRangeSliceJobs hands each worker a disjoint [idx1, idx2) band covering [0, height).
  const bool committed = pool->CommitRangeSliceJobsAndWait(0, height, [&](int, int begin, int end) {
    if (begin < end) {
      body(begin, end);
    }
  });
  if (!committed) {
    // A pool that refused the commit (shutdown mid-flight) may have run some bands already.
    // Re-running the whole range is safe because every band writes only its own pixels' outputs
    // from its own inputs, which is the same premise the parallel split rests on.
    body(0, height);
  }
}

}  // namespace lumice
