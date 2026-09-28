// Single-source pure-math Fresnel reflection ratio.
// Shared by host C++ (src/core/optics.cpp), MSL kernel
// (src/core/metal/lumice_trace.metal), and (future) CUDA kernel.
//
// Surface contract: scalar value semantics only.
#ifndef LM_OPTICS_SHARED_H_
#define LM_OPTICS_SHARED_H_

#include "lm_shims.h"

namespace lm_optics {

// Unpolarized Fresnel reflection ratio.
//   delta = (1 - rr^2)/cos^2(theta) + rr^2  (positive => transmitted; <=0 => TIR)
//   rr    = relative refractive index along the ray direction
// Caller is responsible for clamping delta >= 0 before invoking.
//
// One body, two precisions. `float` is what every backend's trace instantiates (through the
// GetReflectRatio wrapper below — the call sites never name the template), and its arithmetic is
// the float expression this function has always been. `double` is instantiated on the host only, by
// liblumice_analytic (src/analytic/path_evaluation.cpp), whose tolerances are set in double
// (doc/analytic-api.md section 5.4); giving it its own copy of the formula would put a second
// implementation of this primitive in the tree.
template <typename T>
LM_FN T GetReflectRatioT(T delta, T rr) {
  T d_sqrt = LM_SQRT(delta);
  T Rs = (rr - d_sqrt) / (rr + d_sqrt);  // NOLINT(readability-identifier-naming) Fresnel notation
  Rs *= Rs;
  T Rp = (T(1) - rr * d_sqrt) / (T(1) + rr * d_sqrt);  // NOLINT(readability-identifier-naming) Fresnel notation
  Rp *= Rp;
  return (Rs + Rp) * T(0.5f);  // a float literal: MSL has no double, and 0.5 is exact in both
}

LM_FN float GetReflectRatio(float delta, float rr) {
  return GetReflectRatioT<float>(delta, rr);
}

}  // namespace lm_optics

#endif  // LM_OPTICS_SHARED_H_
