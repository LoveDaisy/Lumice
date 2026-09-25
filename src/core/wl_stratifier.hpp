#ifndef CORE_WL_STRATIFIER_H_
#define CORE_WL_STRATIFIER_H_

#include <cmath>

#include "core/math.hpp"

namespace lumice {

// Wavelength draw for a standard (continuous) illuminant on the CPU path, where one
// physics batch (128 rays by default) traces at one wavelength.
//
// The k-th batch takes x_k = frac(phi + k * alpha) with alpha = golden ratio - 1 and phi
// drawn once from the worker's RNG: a randomly shifted Kronecker sequence. Every x_k is
// on its own uniform on [0, 1) because phi is, so the estimator is unbiased for any
// number of batches — no "complete a cycle" requirement a permuted-strata scheme would
// have. What changes against independent draws is the joint distribution: any run of
// consecutive batches covers the band evenly (three-gap theorem), so the part of the
// image a whole batch lands on together — the direct-transmission sun spot, and the
// frame total — no longer inherits the scatter of a few thousand independent
// wavelengths. Measured on three D65 scenes at 1e6 rays: whole-image pixel variance
// 0.22-0.31x of independent draws, frame-total variance 0.0002-0.001x, per-ray cost zero.
// Per-ray independent wavelengths bought the same whole-image variance and a frame
// total 10x worse, which is why the batch keeps a single wavelength here.
//
// Scope is one Simulator::Run(): Run() re-seeds its RNG under a fixed seed, and a
// stratifier created there draws phi from that RNG on first use, so a fixed-seed session
// replays the same wavelengths.
class WavelengthStratifier {
 public:
  // The next batch's position in [0, 1). Draws from `rng` once, on the first call only.
  float NextUnit(RandomNumberGenerator& rng) {
    if (!started_) {
      x_ = static_cast<double>(rng.GetUniform());
      started_ = true;
    } else {
      x_ += kAlpha;
      x_ -= std::floor(x_);
    }
    // x_ < 1 in double can still round up to 1.0f.
    float u = static_cast<float>(x_);
    return u < 1.0f ? u : kBelowOne;
  }

 private:
  static constexpr double kAlpha = 0.6180339887498949;  // golden ratio - 1
  static constexpr float kBelowOne = 0.99999994f;       // largest float below 1
  double x_ = 0.0;
  bool started_ = false;
};

}  // namespace lumice

#endif  // CORE_WL_STRATIFIER_H_
