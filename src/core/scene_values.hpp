#ifndef LUMICE_CORE_SCENE_VALUES_HPP_
#define LUMICE_CORE_SCENE_VALUES_HPP_

#include <variant>
#include <vector>

#include "core/crystal_param.hpp"
#include "util/illuminant_data.hpp"
namespace lumice {

// Every field carries a default so that a partially filled `SunParam` is a well-defined value
// rather than an uninitialized read. The zeros are the same bytes `SunParam p{}` (the JSON
// decoder's starting point) already produced, so no default changed; what changed is that
// `SunParam s; s.altitude_ = x;` no longer leaves `azimuth_` holding whatever the stack held.
// That shape put a NaN into the annotation layer's sun direction, whose normalizer then fell
// back to the zenith and moved a whole family of circles — intermittently, since it depended
// on the previous frame's residue.
struct SunParam {
  float altitude_{};  // Degree
  float azimuth_{};   // Degree
  float diameter_{};  // Degree
};

struct WlParam {
  float wl_;
  float weight_;
};

using SpectrumConfig = std::variant<std::vector<WlParam>, IlluminantType>;

struct LightSourceConfig {
  SunParam param_;
  SpectrumConfig spectrum_;
};


using CrystalParam = std::variant<PrismCrystalParam, PyramidCrystalParam>;

struct CrystalConfig {
  IdType id_;
  CrystalParam param_;
  AxisDistribution axis_;
};


}  // namespace lumice

#endif  // LUMICE_CORE_SCENE_VALUES_HPP_
