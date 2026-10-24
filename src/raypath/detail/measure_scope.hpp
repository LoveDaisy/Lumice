#ifndef LUMICE_RAYPATH_DETAIL_MEASURE_SCOPE_HPP_
#define LUMICE_RAYPATH_DETAIL_MEASURE_SCOPE_HPP_

#include <string>

#include "core/random.hpp"
#include "raypath/domain/measure.hpp"

namespace lumice::raypath::survey {

// The refusal mode prefix of the v1 domain gate (the mode is an open string; the named shape
// follows the colon).
inline constexpr const char* kMeasureOutsideDomainPrefix = "measure_shape_outside_routing_domain: ";

// The measure reading: the extraction plus the v1 domain gate, one call (the gate reads the
// same classification the echo reports, so the two can never disagree about what was read).
struct FamilyMeasureReading {
  bool ok = false;
  std::string refusal_mode;  // kMeasureOutsideDomainPrefix + the named shape, when !ok
  bool single_pose = false;  // all three dimensions fixed — the 0D arm (in the domain)
  FamilyRoutingMeasure measure;
};

FamilyMeasureReading ReadFamilyMeasure(const AxisDistribution& axis);


}  // namespace lumice::raypath::survey

#endif  // LUMICE_RAYPATH_DETAIL_MEASURE_SCOPE_HPP_
