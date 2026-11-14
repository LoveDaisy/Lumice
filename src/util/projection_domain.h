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
