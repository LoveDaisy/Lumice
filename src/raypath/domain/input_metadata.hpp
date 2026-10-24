#ifndef LUMICE_RAYPATH_DOMAIN_INPUT_METADATA_HPP_
#define LUMICE_RAYPATH_DOMAIN_INPUT_METADATA_HPP_

#include <string>

#include "core/distribution.hpp"

namespace lumice::raypath {

constexpr double kDefaultWavelengthNm = 550.0;

enum class WavelengthSource { kDefault, kConfigSingle, kUser };
const char* WavelengthSourceName(WavelengthSource source);

// One crystal shape scalar as the kernel received it: `value` is the distribution's centre slot,
// which is the value itself (fixed), the interval midpoint (uniform), the mean (gauss) or the
// location (laplacian).
struct NominalShapeScalar {
  std::string name;  // the config key: height, prism_h, upper_h, lower_h, face_distance[i]
  double value = 0.0;
  DistributionType distribution = DistributionType::kNoRandom;
  double spread = 0.0;  // the distribution's second slot (0 for a fixed value)
};

}  // namespace lumice::raypath

#endif  // LUMICE_RAYPATH_DOMAIN_INPUT_METADATA_HPP_
