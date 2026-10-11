#ifndef LUMICE_GUI_VIEW_LOOK_AT_TYPES_HPP
#define LUMICE_GUI_VIEW_LOOK_AT_TYPES_HPP

#include "include/lumice_render.h"

namespace lumice::gui {

enum class LookAtId : int {
  kZenith = LUMICE_ANNOTATION_MARKER_ZENITH,
  kNadir = LUMICE_ANNOTATION_MARKER_NADIR,
  kSun = LUMICE_ANNOTATION_MARKER_SUN,
  kSubsun = LUMICE_ANNOTATION_MARKER_SUBSUN,
  kAnthelion = LUMICE_ANNOTATION_MARKER_ANTHELION,
  kAntisolar = LUMICE_ANNOTATION_MARKER_ANTISOLAR,
  kSunHorizon = LUMICE_ANNOTATION_MARKER_COUNT,
  kHorizonSunPlus90 = LUMICE_ANNOTATION_MARKER_COUNT + 1,
  kHorizonAntiSun = LUMICE_ANNOTATION_MARKER_COUNT + 2,
  kHorizonSunMinus90 = LUMICE_ANNOTATION_MARKER_COUNT + 3,
  kCount = LUMICE_ANNOTATION_MARKER_COUNT + 4,
};

inline bool IsValidLookAtId(LookAtId id) {
  return static_cast<int>(id) >= 0 && static_cast<int>(id) < static_cast<int>(LookAtId::kCount);
}

struct LookAtSession {
  LookAtId target = LookAtId::kSun;
  bool locked = false;
};

}  // namespace lumice::gui

#endif  // LUMICE_GUI_VIEW_LOOK_AT_TYPES_HPP
