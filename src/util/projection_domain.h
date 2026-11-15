#ifndef LUMICE_UTIL_PROJECTION_DOMAIN_H_
#define LUMICE_UTIL_PROJECTION_DOMAIN_H_

// Pure numerical domain facts. No scene/config types: host GUI, CPU and device code consume
// the same floors without crossing an API boundary. MSL requires an explicit address space.
#ifdef __METAL_VERSION__
#define LUMICE_DOMAIN_CONSTANT constant constexpr
#else
#define LUMICE_DOMAIN_CONSTANT constexpr
#include <cmath>
#endif

namespace lumice {
namespace projection_domain {

LUMICE_DOMAIN_CONSTANT float kFisheyeEqualAreaMinCz = -1.0f + 1e-3f;
LUMICE_DOMAIN_CONSTANT float kFisheyeEquidistantMinCz = -1.0f + 1e-6f;
LUMICE_DOMAIN_CONSTANT float kFisheyeStereographicMinCz = -0.99996192f;
LUMICE_DOMAIN_CONSTANT float kRimSlack = 1.0f + 1e-5f;
LUMICE_DOMAIN_CONSTANT float kFisheyeEqualAreaMaxR2 = 2.0f * kRimSlack * kRimSlack;
LUMICE_DOMAIN_CONSTANT float kFisheyeEquidistantMaxR = 2.0f * kRimSlack;

// Float→int conversion-domain bounds for the pixel-coordinate casts in the single-lens branch
// of ProjectExitToPixel (projection_shared.h). Cast safety, NOT visibility: pixel bounds
// culling is the CALLER's job by that function's header contract, so these bounds never pull
// an off-canvas pixel into the canvas — they only keep the C++ float→int cast itself defined,
// which is UB once the truncated value leaves [INT_MIN, INT_MAX]. The forward formulas
// legitimately return pixel expressions far outside any canvas (kProjLinear's dx/dz is
// unbounded for near-horizontal rays), so the clamp's identity region must be the whole
// previously-defined conversion range, not a canvas:
//   - Every float32 with |v| >= 2^24 is an integer (ulp >= 2), so floor is exact and the cast
//     stays defined for ALL of (2^24, 2^31) — the UB region is NOT "|v| > 2^24", and 2^24
//     would be a behaviour-changing bound (it rewrites defined outputs in (2^24, 2^31)).
//   - kPixelCoordCastSafeMax = 2147483520 = 2^31 - 128: the largest representable float below
//     2^31 (ulp = 128 across [2^30, 2^31)); the next float up is 2^31 itself, whose truncation
//     is past INT_MAX. Do NOT use float(INT_MAX) as this bound: 2147483647 rounds UP to 2^31
//     in float32, i.e. the would-be bound is itself out of domain.
//   - kPixelCoordCastSafeMin = -2147483648 = INT_MIN: exactly representable with a defined
//     truncation; the next float down (-2147483904) is out of domain.
// Every value the cast already defined (floored results in [-2147483648, 2147483520]) passes
// through unchanged; only the previously-UB region is pinned to a bound.
LUMICE_DOMAIN_CONSTANT float kPixelCoordCastSafeMax = 2147483520.0f;
LUMICE_DOMAIN_CONSTANT float kPixelCoordCastSafeMin = -2147483648.0f;

#ifndef __METAL_VERSION__
// Host inverse and GUI forward share this derived bound; the device only needs its cz floor.
inline float FisheyeStereographicMaxR() {
  return std::tan(0.5f * std::acos(kFisheyeStereographicMinCz)) * kRimSlack;
}
#endif

}  // namespace projection_domain
}  // namespace lumice

#undef LUMICE_DOMAIN_CONSTANT
#endif
