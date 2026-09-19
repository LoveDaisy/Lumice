// accum_shared.h — single-source XYZ accumulation kernel
//
// Surface contract
//   AccumXyzToPixel(buf, pix_flat, cmf_x, cmf_y, cmf_z, weight)
//     Atomically adds weight * (cmf_x, cmf_y, cmf_z) to a 3-channel XYZ
//     pixel at `pix_flat` inside a W*H*3 float buffer.
//   CUDA only: AccumXyzToPixel(buf, acc, pix_flat, ...) — a second, double
//     W*H*3 plane `acc` that the float plane spills into (see below); the
//     pixel's value is buf + acc.
//
// Backend variants
//   MSL (`__METAL_VERSION__`):   buf is `device atomic_float*`
//   CUDA (`__CUDACC__`):         buf is `float*`, acc is `double*`
//                                                       (atomicAdd intrinsics)
//   Host C++:                    buf is `float*`        (plain +=, used by
//                                                       parity oracles; not
//                                                       the cross-batch path)
//
// The Metal and host variants add into a float32 buffer that lives ONE batch
// (Metal drains per batch) or one session (host oracle) — a bounded chain. The
// CUDA variant is the exception because its plane is not bounded that way: the
// third-clock drain (Simulator::kDefaultXyzDrainBatches) keeps the device plane
// alive for a whole window of batches, and a per-exit fp32 chain that long
// measured a window-length-dependent drift of up to +0.40% on a narrow-field
// scene against the host's double ledger (16 batches → −0.135%, 1 → −0.008%) —
// the pixel-level twin of the landed-weight scalar defect described in
// cuda_trace_backend.cu's EmitToDeviceXyz. The fix is a ratio-gated spill: the
// fp32 atomicAdd returns the pixel's previous sum, and once that sum is more
// than kXyzSpillRatio times the addend the pixel is moved (atomicExch to 0,
// then a double atomicAdd of what was there) into the double plane. The fp32
// plane therefore never holds more than ~kXyzSpillRatio addends' worth per
// pixel, every fp32 add rounds by at most that sum's half-ulp — a relative
// error of kXyzSpillRatio·2⁻²⁴ per addend, 10⁻⁵ at 128, independent of how
// long the window is — and the double plane is only ever touched at pixels
// that are hot, so its cache footprint is that of the hot pixels rather than of
// the whole plane. Two alternatives were built and measured on the 2048×1024
// throughput scene before this shape was chosen: folding the whole fp32 plane
// into a double plane once per batch (correct, −22% throughput — a full-plane
// pass on the stream's critical path every batch), and accumulating every exit
// straight into a double plane (correct, −14% at 2048×1024 and neutral at
// ≤1024×512 — the 2× plane no longer sits comfortably in L2). The cross-batch
// reduction on the host side is not here: RenderConsumer folds each window
// into a double running sum (server/render.cpp), which replaced the float
// Neumaier pair this header used to carry. Compensated float summation was
// measured to be no answer to an unbounded chain — its compensation term is
// itself a float running sum and drifts the same way, 4e-3 relative after 1e7
// constant addends — so a new long-chain accumulator takes a double, not a
// `NeumaierAdd`.
//
// Note: the parameter type cannot be unified across all three backends because
// MSL requires the `device atomic_float*` qualifier for atomic_fetch_add.
// The per-channel add is identical across the three variants; CUDA adds the
// spill gate around it.

#ifndef LM_ACCUM_SHARED_H_
#define LM_ACCUM_SHARED_H_

#if defined(__METAL_VERSION__)
// ===== MSL =====
#include <metal_stdlib>

inline void AccumXyzToPixel(device metal::atomic_float* xyz_buf, uint32_t pix_flat, float cmf_x, float cmf_y,
                            float cmf_z, float weight) {
  uint32_t base = pix_flat * 3u;
  metal::atomic_fetch_add_explicit(xyz_buf + base + 0u, cmf_x * weight, metal::memory_order_relaxed);
  metal::atomic_fetch_add_explicit(xyz_buf + base + 1u, cmf_y * weight, metal::memory_order_relaxed);
  metal::atomic_fetch_add_explicit(xyz_buf + base + 2u, cmf_z * weight, metal::memory_order_relaxed);
}

#elif defined(__CUDACC__)
// ===== CUDA =====
// Spill gate: a pixel whose fp32 sum already exceeds this many times the addend
// moves to the double plane. Bounds the fp32 rounding error per addend at
// kXyzSpillRatio·2⁻²⁴ (see the header comment); the race slack — several
// threads observing the gate open before one of them empties the pixel — only
// widens that bound by a small factor, never loses mass (atomicExch returns
// exactly what was there, and an add landing after it stays in the fp32 plane
// for the next check).
constexpr float kXyzSpillRatio = 128.0f;

__device__ inline void AccumXyzChannel(float* buf, double* acc, float v) {
  if (v <= 0.0f) {
    return;  // nothing to add (a zero CMF sample); also keeps the gate's ratio well-defined
  }
  const float old = atomicAdd(buf, v);
  if (old >= kXyzSpillRatio * v) {
    const float moved = atomicExch(buf, 0.0f);
    atomicAdd(acc, static_cast<double>(moved));
  }
}

__device__ inline void AccumXyzToPixel(float* xyz_buf, double* xyz_acc, uint32_t pix_flat, float cmf_x, float cmf_y,
                                       float cmf_z, float weight) {
  uint32_t base = pix_flat * 3u;
  AccumXyzChannel(xyz_buf + base + 0u, xyz_acc + base + 0u, cmf_x * weight);
  AccumXyzChannel(xyz_buf + base + 1u, xyz_acc + base + 1u, cmf_y * weight);
  AccumXyzChannel(xyz_buf + base + 2u, xyz_acc + base + 2u, cmf_z * weight);
}

#else
// ===== Host C++ =====
#include <cstdint>

inline void AccumXyzToPixel(float* xyz_buf, std::uint32_t pix_flat, float cmf_x, float cmf_y, float cmf_z,
                            float weight) {
  std::uint32_t base = pix_flat * 3u;
  xyz_buf[base + 0u] += cmf_x * weight;
  xyz_buf[base + 1u] += cmf_y * weight;
  xyz_buf[base + 2u] += cmf_z * weight;
}

#endif

#endif  // LM_ACCUM_SHARED_H_
