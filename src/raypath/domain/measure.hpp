#ifndef LUMICE_RAYPATH_DOMAIN_MEASURE_HPP_
#define LUMICE_RAYPATH_DOMAIN_MEASURE_HPP_

#include "raypath/domain/scope.hpp"

namespace lumice::raypath::survey {

// The routing vocabulary of one orientation dimension. A width NEVER enters a rank question —
// it feeds only the freeze order and the survival filter.
enum class FamilyDimKind { kFixed, kGauss, kUniformFull };

inline const char* FamilyDimKindName(FamilyDimKind kind) {
  switch (kind) {
    case FamilyDimKind::kFixed:
      return "fixed";
    case FamilyDimKind::kGauss:
      return "gauss";
    case FamilyDimKind::kUniformFull:
      return "uniform";
  }
  return "uniform";
}

struct FamilyDimMeasure {
  FamilyDimKind kind = FamilyDimKind::kUniformFull;
  double center_deg = 0.0;  // the fixed value / gauss mean / the uniform's own midpoint echo
  double width_deg = 0.0;   // the gauss sigma (the freeze candidate's width); 0 for fixed; the
                            // uniform's full range as a raw echo — a uniform is never a freeze
                            // candidate, so its width is display only
};

// The measure the router read, in ROUTING coordinates (zenith = 90 - latitude, the density
// conversion's own formula), so the echo and the kernel's family dimensions speak one name.
struct FamilyRoutingMeasure {
  MeasureScope scope;
  FamilyDimMeasure zenith;
  FamilyDimMeasure azimuth;
  FamilyDimMeasure roll;
};


}  // namespace lumice::raypath::survey

#endif  // LUMICE_RAYPATH_DOMAIN_MEASURE_HPP_
