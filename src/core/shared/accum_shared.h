// accum_shared.h — single-source XYZ accumulation kernel
//
// Surface contract
//   AccumXyzToPixel(buf, pix_flat, cmf_x, cmf_y, cmf_z, weight)
//     Atomically adds weight * (cmf_x, cmf_y, cmf_z) to a 3-channel XYZ
//     pixel at `pix_flat` inside a W*H*3 float buffer.
//
// Backend variants
//   MSL (`__METAL_VERSION__`):   buf is `device atomic_float*`
//   CUDA (`__CUDACC__`):         buf is `float*`        (atomicAdd intrinsic)
//   Host C++:                    buf is `float*`        (plain +=, used by
//                                                       parity oracles; not
//                                                       the cross-batch path)
//
// Every variant adds into a float32 buffer, and every caller bounds the chain
// that buffer ever holds. Metal drains (reads back and zeroes) its plane every
// batch. CUDA keeps its plane alive across a whole drain window, so the chain
// is bounded one level down instead: the simulator has the backend fold the
// fp32 plane into a device double plane and zero the fp32 side every
// Simulator::kXyzFoldEveryBatches (8) batches, and the drain folds the residue
// on the same kernel (fold_xyz_plane_kernel in cuda_trace_backend.cu) — so no
// fp32 chain here is longer than 8 batches, whatever the window length. The
// host oracle's buffer lives one session. The cross-batch reduction on the
// host side is not here either: RenderConsumer folds each window into a double
// running sum (server/render.cpp), which replaced the float Neumaier pair this
// header used to carry. Compensated float summation was measured to be no
// answer to an unbounded chain — its compensation term is itself a float
// running sum and drifts the same way, 4e-3 relative after 1e7 constant addends
// — so a new long-chain accumulator takes a double, not a `NeumaierAdd`.
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
__device__ inline void AccumXyzToPixel(float* xyz_buf, uint32_t pix_flat, float cmf_x, float cmf_y, float cmf_z,
                                       float weight) {
  uint32_t base = pix_flat * 3u;
  atomicAdd(xyz_buf + base + 0u, cmf_x * weight);
  atomicAdd(xyz_buf + base + 1u, cmf_y * weight);
  atomicAdd(xyz_buf + base + 2u, cmf_z * weight);
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
