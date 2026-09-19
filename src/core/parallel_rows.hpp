#ifndef CORE_PARALLEL_ROWS_H_
#define CORE_PARALLEL_ROWS_H_

#include <cstddef>
#include <functional>

namespace lumice {

// Row-parallel dispatch for the per-pixel W*H loops that build render-domain and annotation
// masks (lens_proj_build.hpp, annotation_overlay.hpp).
//
// `body(row_begin, row_end)` receives a half-open band of image rows. Splitting by ROW is what
// makes the parallel result bit-identical to the serial one, and only under a premise the caller
// owes: every pixel writes its own slot and reads only its own inputs, with no cross-row
// reduction, no shared counter, and no order-sensitive shared buffer. A future angle field that
// introduces one of those invalidates the bit-identical guarantee, not merely the speedup.
//
// Below kParallelPixelThreshold (see the .cpp) the body runs inline on the calling thread:
// standing up a pool costs more than the loop saves at preview sizes, and these masks are built
// on the config-commit path where a small frame has to stay cheap.
//
// `thread_budget` is the number of cores this call may occupy, computed by the caller and passed
// explicitly on every call: there is no default, and this function never reads a core count
// itself. The budget is the machine's IDLE cores — physical cores minus the simulation workers
// the owning server keeps busy (server.cpp, render_thread_budget_) — because the pool built here
// competes with those workers for the same physical cores. The first version of this function
// asked for a full-core pool on every call; under the GUI's ~20 ms poll cadence that pool
// pre-empted the fixed-size worker pool and cost ~12 percentage points of simulation throughput
// on a 12-core machine, more than the loop it parallelized ever saved. A budget below 2 runs the
// body inline on the calling thread — no one-thread pool is stood up, since that is the serial
// loop plus a spawn for nothing.
//
// The pool is created and torn down per call rather than kept in a function-local static. Mask
// building runs a handful of times per commit (one RenderConsumer per renderer, at most
// LUMICE_MAX_CONFIG_RENDERERS of them, constructed serially) and once per annotation request, so
// the spawn cost is amortized against a multi-millisecond loop; a shared static would need a
// concurrency contract ThreadingPool does not document.
//
// Declaration-only on purpose: lens_proj_build.hpp is compiled by nvcc and by the ObjC++ Metal
// backend, and pulling <thread>/<future> into those translation units buys nothing and has no CI
// coverage on the Windows+CUDA combination (see AGENTS.md). The threading include stays in the
// .cpp.
void ParallelRows(int height, size_t pixel_count, int thread_budget, const std::function<void(int, int)>& body);

}  // namespace lumice

#endif  // CORE_PARALLEL_ROWS_H_
