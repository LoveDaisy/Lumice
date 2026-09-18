#ifndef CORE_COLOR_UTIL_H_
#define CORE_COLOR_UTIL_H_

#include <cstddef>

#include "util/color_data.hpp"

namespace lumice {

// Public constants previously declared in server/render.hpp — re-declared here
// so core/ TUs need not include server headers.
inline constexpr int kCmfMinWavelength = 360;
inline constexpr int kCmfMaxWavelength = 830;

// Display brightness baseline: maps the physically-derived per-pixel radiance
// to a visually reasonable [0,1] range at EV=0. The average illuminated pixel
// appears at ~5% brightness, so bright halo features (~20x average) approach
// full white. Independent of resolution and FOV — use EV (intensity_factor)
// for user-controlled brightness adjustment.
//
// The value was originally calibrated against a denominator of LANDED energy
// and is now applied against EMITTED energy (see RenderConsumer::ExposureScale).
// It is unchanged, and deliberately so: the two denominators differ by the
// landed fraction, which for a full-sky field of view measures ~0.98, i.e. under
// 0.03 stop — smaller than the calibration's own precision. Re-deriving the
// constant would move every reference image for less than the eye can see.
inline constexpr float kNormScale = 0.08f;

// Scatter-add `v[i]` weighted by the CIE 1931 color-matching functions at
// the given wavelength into the XYZ image. When xy == nullptr, samples write
// in-order to xyz[3*i .. 3*i+2]; otherwise xy[i] is the flat pixel index.
// No bounds check — caller must clamp xy[] to [0, W*H).
//
// `Acc` is the accumulator's element type, float or double, and which one a
// caller hands in is a statement about its chain length. A per-ray `+=` into a
// float32 sum is exact only while the addend is large against the sum's ulp;
// a pixel that collects 1e7 rays has a sum near 1e6 against addends near 0.1,
// where every add rounds by up to half an ulp in the SAME direction (round-
// half-even on a fixed bit pattern is a bias, not noise), and X, Y and Z bias
// by different amounts, so the ratio drifts — a monochromatic sub-sun turned
// from yellow-green to cyan between 10M and 30M rays that way. A running sum
// that grows with the ray budget therefore takes double (RenderConsumer's
// internal_xyz_); a buffer that is reset per batch or per session — the
// CpuTraceBackend parity image, the per-batch product here — may stay float.
// The per-ray product itself stays a float multiply on either path so the two
// accumulator types add the same addends and differ only in where they round.
//
// Inline-defined so that scatter_accum.hpp and server/render.cpp can both
// include this header without an ODR collision.
template <typename Acc>
inline void SpectrumToXyz(float wl, const float* v, const int* xy, Acc* xyz, size_t num = 1) {
  int wl_key = static_cast<int>(wl + 0.5f);
  if (wl_key < kCmfMinWavelength || wl_key > kCmfMaxWavelength) {
    return;
  }

  for (size_t i = 0; i < num; i++) {
    size_t idx = xy == nullptr ? i * 3 : static_cast<size_t>(xy[i]) * 3;
    xyz[idx + 0] += kCmfX[wl_key - kCmfMinWavelength] * v[i];
    xyz[idx + 1] += kCmfY[wl_key - kCmfMinWavelength] * v[i];
    xyz[idx + 2] += kCmfZ[wl_key - kCmfMinWavelength] * v[i];
  }
}

// Single-ray Y-only helper (task-336.2 component lanes).
// Returns kCmfY[wl] * w for an in-range wavelength; 0 otherwise.
// Deliberately mirrors SpectrumToXyz's wavelength clipping (rounding + range check)
// so per-component Y lanes accumulate a strict Y-slice of the same batch.
inline float SpectrumToYSingle(float wl, float w) {
  int wl_key = static_cast<int>(wl + 0.5f);
  if (wl_key < kCmfMinWavelength || wl_key > kCmfMaxWavelength) {
    return 0.0f;
  }
  return kCmfY[wl_key - kCmfMinWavelength] * w;
}

// Per-ray variant — each ray i carries its own wavelength wl_per_ray[i].
// Used by the Metal/DR-3 path where the photon's lifetime wavelength tag is
// derived from a host-uploaded wavelength pool (see metal_trace_backend.mm's
// ComputeWlPool). xy / xyz semantics mirror SpectrumToXyz above, `Acc` included.
// Out-of-range wavelengths are silently skipped per ray.
template <typename Acc>
inline void SpectrumToXyzPerRay(const float* wl_per_ray, const float* v, const int* xy, Acc* xyz, size_t num) {
  for (size_t i = 0; i < num; i++) {
    int wl_key = static_cast<int>(wl_per_ray[i] + 0.5f);
    if (wl_key < kCmfMinWavelength || wl_key > kCmfMaxWavelength) {
      continue;
    }
    int idx_cmf = wl_key - kCmfMinWavelength;
    size_t pidx = xy == nullptr ? i * 3 : static_cast<size_t>(xy[i]) * 3;
    xyz[pidx + 0] += kCmfX[idx_cmf] * v[i];
    xyz[pidx + 1] += kCmfY[idx_cmf] * v[i];
    xyz[pidx + 2] += kCmfZ[idx_cmf] * v[i];
  }
}

}  // namespace lumice

#endif  // CORE_COLOR_UTIL_H_
