// Single-source pure-math polygon-slab face traversal.
// Shared by host C++ (src/core/optics.cpp PropagateSlab), MSL kernel
// (src/core/metal/lumice_trace.metal trace_layer_kernel) and CUDA kernel
// (src/core/backend/cuda_trace_backend.cu trace_single_ms_kernel).
//
// Background: scrum-#295.7 fixed a Möller-Trumbore + absolute-ε face-miss bug
// in the CUDA path by porting to legacy `PropagateSlab` (optics.cpp). The
// fix lived as three independent inline copies across CUDA/Metal/CPU; this
// header collapses them to a single source so the absolute-ε anti-pattern
// (doc/numerical-robustness.md约定 2) cannot silently regress in any backend.
//
// Surface contract: scalar value semantics only. No pointer or address-space
// parameters — caller reads face data (normal + plane constant) and passes
// scalars. See lm_shims.h for the cross-language qualifier shims.
//
// Epsilon coupling (kept in sync by convention, not by mechanism):
//   - This header's `kSlabEps` (1e-5f) is the denominator gate inside SlabFaceT.
//   - Metal `kFloatEps` (lumice_trace.metal:315) and CPU `math::kFloatEps`
//     (core/math.hpp) are numerically equal (1e-5f) and continue to drive each
//     backend's post-loop accept threshold (`eps_thr`). If this value is ever
//     re-tuned, **all three constants must move together**.
//
// From-face strategy (caller responsibility):
//   - All three backends classify an outward-going child AT BIRTH and never
//     let it consume a slab result. CPU and Metal test the source-face
//     denominator sign where the child is born (denom_src = d·n_src > 0.0f —
//     a pure sign test, no epsilon; optics.cpp PropagateSlab and
//     lumice_trace.metal trace_layer_kernel) and record the child as an exit
//     with its position unchanged. Metal evaluates the same quantity in its
//     factored form from the Fresnel inputs (reflection child r·n = −cosθ;
//     refraction child f·n = sd·cosθ, sign = cosθ's) to keep the source
//     normal's live range out of the child loop — an occupancy register step
//     (threadgroup 512 → 448) otherwise. CUDA does the same classification
//     constructively: entry external-reflect children exit unconditionally
//     and refracted exits gate on cos_exit > 0.0f (cuda_trace_backend.cu).
//     The CPU/Metal and CUDA predicates are deliberately identical in shape
//     — a sign test with no epsilon on any side — so no (0, ε] band can
//     re-open where an outward child slips back into the slab. denom_src ==
//     0.0f counts as inward: a deliberate tie-break on the same boundary as
//     CUDA's cos_exit > 0.0f (measure-zero band; the tie-break direction
//     matches CUDA's by construction — both gate exits on `> 0` — while
//     near-zero values may differ by ~1 ulp across backends, factored form
//     vs full dot product), not a defect to "fix" asymmetrically.
//   - What remains inside the slab loops is mechanical belt-and-braces, not
//     semantics: CUDA's explicit `fi == from_poly` skip, and Metal/CPU's
//     post-loop `eps_thr` threshold for the source face. For an INWARD child
//     (denom_src ≤ 0 < kSlabEps) the source plane is not a slab candidate in
//     the first place — SlabFaceT's denominator gate excludes it — so those
//     guards only backstop float noise. This equivalence premise (outward
//     children never consume a slab result) is what makes the two in-loop
//     strategies interchangeable.
//   - Convexity: the slab's "face with the minimum valid t is the exit face"
//     invariant additionally presupposes a ray that STARTS INSIDE a convex
//     crystal whose every polygon plane bounds a real (non-degenerate) face.
//     A birth point a few ulp inside its source plane plus a grazing outward
//     denominator inflates t = |δ|/denom past any fixed guard, and the convex
//     half-space property hands positive t to other faces as well — which is
//     exactly why outward children must be classified before the slab and not
//     inside it. If a non-convex crystal type is ever wired through (see
//     CreateConcavePyramidMesh in geo3d, currently NOT wired to the
//     config/Crystal factory), outward rays CAN legitimately re-hit and the
//     birth classification — not just the in-loop guards — must be revisited.
//
// Convex-crystal invariant: the face with the minimum valid t (denom > eps)
// is guaranteed to be the exit face. There is always at least one such face
// for a ray strictly inside the convex hull.
#ifndef LM_TRAVERSAL_SHARED_H_
#define LM_TRAVERSAL_SHARED_H_

#include "lm_shims.h"

namespace lm_traversal {

// Denominator/face gate epsilon. Float32-loose value chosen to absorb
// near-parallel rounding without rejecting any legitimate exit face on the
// configured crystal scales. See header comment for cross-backend coupling.
LM_CONSTANT float kSlabEps = 1e-5f;

// Per-face polygon-slab intersection t value.
//   Returns t if the ray is leaving this face's half-space
//     (dir·normal > kSlabEps), where t = -(org·n + fd) / (dir·n).
//   Returns 1e30f otherwise (face is parallel to the ray or the ray is
//     entering its half-space — not a candidate exit face).
// The 1e30f sentinel makes `if (t < t_best)` naturally skip non-candidates
// without requiring a separate `denom > eps` check at the call site.
//
// Caller responsibilities:
//   - outward-child birth classification before the slab (see header note on
//     the from-face strategy)
//   - min-t tracking across faces
//   - post-loop accept threshold (e.g. eps_thr relaxation for TIR-edge cases)
// 10 scalar params (3 dir + 3 origin + 3 normal + plane const) — scalar-only
// contract precludes a struct wrapper, so silence the size linter explicitly.
// NOLINTNEXTLINE(readability-function-size)
LM_FN float SlabFaceT(float dx, float dy, float dz,  // ray direction
                      float px, float py, float pz,  // ray origin
                      float nx, float ny, float nz,  // face outward normal
                      float fd) {                    // plane constant (p·n + fd = 0)
  float denom = dx * nx + dy * ny + dz * nz;
  if (denom <= kSlabEps) {
    return 1.0e30f;
  }
  return -(px * nx + py * ny + pz * nz + fd) / denom;
}

}  // namespace lm_traversal

#endif  // LM_TRAVERSAL_SHARED_H_
