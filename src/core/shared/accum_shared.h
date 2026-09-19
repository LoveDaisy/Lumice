// accum_shared.h — single-source XYZ accumulation kernel
//
// Surface contract
//   AccumXyzToPixel(buf, pix_flat, cmf_x, cmf_y, cmf_z, weight)
//     Atomically adds weight * (cmf_x, cmf_y, cmf_z) to a 3-channel XYZ
//     pixel at `pix_flat` inside a W*H*3 buffer.
//
// Backend variants
//   MSL (`__METAL_VERSION__`):   buf is `device atomic_float*`
//   CUDA (`__CUDACC__`):         buf is `double*`       (atomicAdd intrinsic,
//                                                       native since sm_60)
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
// cuda_trace_backend.cu's EmitToDeviceXyz. Folding the fp32 plane into a
// double plane once per batch on device was measured correct but cost 22% of
// throughput on a 2048×1024 plane (a full-plane pass on the stream's critical
// path every batch), so the CUDA plane is simply double: the per-exit atomic
// widens, nothing is added per batch, and the drain converts to float on
// device before the D2H. The cross-batch reduction on the host side is not
// here: RenderConsumer folds each window into a double running sum
// (server/render.cpp), which replaced the float Neumaier pair this header used
// to carry. Compensated float summation was measured to be no answer to an
// unbounded chain — its compensation term is itself a float running sum and
// drifts the same way, 4e-3 relative after 1e7 constant addends — so a new
// long-chain accumulator takes a double, not a `NeumaierAdd`.
//
// Note: the parameter type cannot be unified across all three backends because
// MSL requires the `device atomic_float*` qualifier for atomic_fetch_add.
// Function body logic is identical across the three variants.

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
__device__ inline void AccumXyzToPixel(double* xyz_buf, uint32_t pix_flat, float cmf_x, float cmf_y, float cmf_z,
                                       float weight) {
  uint32_t base = pix_flat * 3u;
  // The product stays a float product (same rounding as Metal / host); only the
  // running sum is double.
  atomicAdd(xyz_buf + base + 0u, static_cast<double>(cmf_x * weight));
  atomicAdd(xyz_buf + base + 1u, static_cast<double>(cmf_y * weight));
  atomicAdd(xyz_buf + base + 2u, static_cast<double>(cmf_z * weight));
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
