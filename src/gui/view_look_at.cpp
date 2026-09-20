#include "gui/view_look_at.hpp"

#include <algorithm>
#include <cmath>

#include "gui/annotation_anchors.hpp"  // GuiSunWorldDir — the GUI's one spelling of the sun
#include "gui/gui_state.hpp"           // kMarkerDisplayNames — the Overlay list's own labels

namespace lumice::gui {

namespace {

constexpr float kRad2Deg = 180.0f / 3.14159265358979323846f;

// The Horizon series, indexed by `id - kSunHorizon`. ONE table for both the label and the bearing
// offset: two switches keyed on the same id would carry an invariant ("these agree") that nothing
// checks until a user reads "Sun +90°" and the camera turns the other way.
//
// The offset is added to the bearing core returns for the sun-side horizon; it is the only thing
// the GUI contributes, and it is a constant, so there is no sign rule here to get wrong. The names
// are relative to the sun because the GUI pins the sun at azimuth 0 and offers no compass. The
// signed forms "+90°" / "-90°" are used instead of left/right: they match the number the user
// sees on the Azimuth slider after picking the entry, with no handedness to remember.
struct HorizonEntry {
  const char* name;
  float az_offset_deg;
};
constexpr HorizonEntry kHorizonSeries[] = {
  { "Toward sun", 0.0f },       // kSunHorizon
  { "Sun +90°", 90.0f },        // kHorizonSunPlus90
  { "Away from sun", 180.0f },  // kHorizonAntiSun
  { "Sun -90°", -90.0f },       // kHorizonSunMinus90
};
constexpr int kHorizonSeriesCount = static_cast<int>(sizeof(kHorizonSeries) / sizeof(kHorizonSeries[0]));
static_assert(kHorizonSeriesCount == static_cast<int>(LookAtId::kCount) - static_cast<int>(LookAtId::kSunHorizon),
              "every non-marker LookAtId must have a row in kHorizonSeries");

// The series entry for `id`, or nullptr for a marker or an out-of-range id.
const HorizonEntry* HorizonEntryFor(LookAtId id) {
  const int index = static_cast<int>(id) - static_cast<int>(LookAtId::kSunHorizon);
  if (index < 0 || index >= kHorizonSeriesCount) {
    return nullptr;
  }
  return &kHorizonSeries[index];
}

// Folds an azimuth into [-180, 180], the CLOSED interval the Azimuth slider accepts, and is the
// identity everywhere inside it. Both are load-bearing. The identity is what keeps the offset-0
// entry's answer the value it was before the series existed: core's bearing can come back as
// either +180 or -180 for the far side (the sign of a zero decides), and a wrap that folded one of
// those onto the other would be a change in behaviour with no change in camera. The fold is
// what a clamp cannot do: base -180 plus an offset of -90 is -270, and clamping that to the
// interval answers -180 — the entry "toward the sun" — where the bearing actually meant is +90.
float WrapAzDeg(float az_deg) {
  float r = std::fmod(az_deg, 360.0f);  // (-360, 360), sign of the input
  if (r > 180.0f) {
    r -= 360.0f;
  } else if (r < -180.0f) {
    r += 360.0f;
  }
  return r;
}

}  // namespace

const char* LookAtDisplayName(LookAtId id) {
  if (IsMarkerLookAt(id)) {
    return kMarkerDisplayNames[static_cast<int>(id)];
  }
  if (const HorizonEntry* entry = HorizonEntryFor(id)) {
    return entry->name;
  }
  return nullptr;
}

void WorldDirToAzEl(const float dir[3], float* az_deg, float* el_deg) {
  const float len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
  // A zero vector names no direction. Scaling by 0 leaves it at the origin, which falls into the
  // pole branch below and comes out as (0, 0) — finite, which is the property that matters, since
  // the only reason a caller reads these is to write them into a camera angle.
  const float inv = (len > 0.0f) ? (1.0f / len) : 0.0f;
  const float x = dir[0] * inv;
  const float y = dir[1] * inv;
  const float z = dir[2] * inv;

  // asin's domain, defended against the rounding that a normalize can leave behind (|z| can come
  // back at 1.0000001 for a vector that was exactly a pole).
  const float clamped_z = (z < -1.0f) ? -1.0f : ((z > 1.0f) ? 1.0f : z);
  *el_deg = std::asin(-clamped_z) * kRad2Deg;

  // At a pole there is no bearing to report, and atan2 must not be asked for one: it reads the SIGN
  // OF A ZERO, and negating a +0 gives a -0, so atan2(-0, -0) answers -pi rather than the 0 an
  // "undefined, pick anything" reading would expect. Left to that, the zenith and nadir presets
  // would swing the camera a half turn around the vertical on their way to a direction that does
  // not depend on the azimuth at all. Answering 0 explicitly is one of the infinitely many correct
  // answers, and it is the same one the overlay label placer anchors its own pole degeneracy at.
  if (x == 0.0f && y == 0.0f) {
    *az_deg = 0.0f;
    return;
  }
  *az_deg = std::atan2(-y, -x) * kRad2Deg;
}

bool ResolveLookAtAzEl(LookAtId id, float sun_altitude_deg, float* out_az_deg, float* out_el_deg) {
  if (!out_az_deg || !out_el_deg) {
    return false;
  }
  float sun_dir[3] = {};
  GuiSunWorldDir(sun_altitude_deg, sun_dir);

  float target[3] = {};
  if (IsMarkerLookAt(id)) {
    if (LUMICE_ResolveAnnotationMarkerDirection(static_cast<int>(id), sun_dir, target) != LUMICE_OK) {
      return false;
    }
    WorldDirToAzEl(target, out_az_deg, out_el_deg);
    return true;
  }

  const HorizonEntry* entry = HorizonEntryFor(id);
  if (entry == nullptr) {
    return false;
  }
  // The whole series is derived from this one call: core says where the sun-side horizon is, and
  // the GUI adds the entry's constant to that bearing. The elevation is read off the answer rather
  // than written as 0, so a future change to core's degenerate fallback is not silently overridden
  // here.
  if (LUMICE_ResolveSunHorizonDirection(sun_dir, target) != LUMICE_OK) {
    return false;
  }
  float base_az = 0.0f;
  float base_el = 0.0f;
  WorldDirToAzEl(target, &base_az, &base_el);
  *out_az_deg = WrapAzDeg(base_az + entry->az_offset_deg);
  *out_el_deg = base_el;
  return true;
}

bool ResolveLookAtPose(LookAtId id, float sun_altitude_deg, const FieldEditorConstraint& el_c,
                       const FieldEditorConstraint& az_c, float* out_az_deg, float* out_el_deg) {
  float az = 0.0f;
  float el = 0.0f;
  if (!ResolveLookAtAzEl(id, sun_altitude_deg, &az, &el)) {
    return false;
  }
  // has_numeric_domain guards a constraint that carries no interval — a combo or a bool row would
  // report [0, 0], and clamping to that would point the camera at the equirect centre for every
  // preset. Neither field this is called with is such a row, which is why the guard passes the
  // value through rather than failing: the interval is missing, not violated.
  *out_el_deg = el_c.has_numeric_domain ?
                    std::clamp(el, static_cast<float>(el_c.min_value), static_cast<float>(el_c.max_value)) :
                    el;
  *out_az_deg = az_c.has_numeric_domain ?
                    std::clamp(az, static_cast<float>(az_c.min_value), static_cast<float>(az_c.max_value)) :
                    az;
  return true;
}

}  // namespace lumice::gui
