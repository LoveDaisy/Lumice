#pragma once

#include <algorithm>
#include <cmath>

namespace lumice {

// The one convention every direction in the C API uses (lumice_render.h, the marker family and the
// raypath-analysis cone centre): a unit vector is the direction light TRAVELS, so the sky point
// it comes FROM sits at altitude = asin(-z) (the zenith is z = -1) and its azimuth is measured as
// the sun's is — the sun at azimuth 0 sits at lon 180, i.e. az = atan2(y, x) - 180.
//
// HOW THE CONSUMERS FIT TOGETHER (doc/coordinate-convention.md "Direction-vector semantics" is
// the single authority; this is the working summary). A sky point's POSITION vector in the world
// frame (+z the zenith, azimuth counter-clockwise from +x) is the NEGATION of what AltAzToDir
// returns: position(alt, az) = (cos·cos az, cos·sin az, +sin alt), and AltAzToDir hands back the
// opposite — the propagation of light arriving FROM that point. That negation is not a footnote,
// it is the whole reason these helpers fill the fields they fill:
//
//   - RaypathHistogramConsumer::ConeMembership dots `cone_center` against the outgoing ray
//     PROPAGATION `w` (toward wherever the ray is heading). A ray's DISPLAYED sky point — the
//     label the user sees it land on — is the point it comes from, position = -w. So
//     dot(AltAzToDir(P), w) = cos(angle between P and the displayed point): AltAzToDir(P) is the
//     CORRECT fill for a cone meant to select what the user names as P. Negating it would select
//     P's antipode.
//   - The single-path kernel (LUMICE_AnalyzeSinglePath's `target_direction`, the analytic
//     kernel's fiber target) consumes the same light-travel convention: it seeks exits whose
//     propagation equals the vector handed in, and an exit displaying at P propagates along
//     AltAzToDir(P). The LI parity fixtures carry target vectors in exactly this form.
//
// Both halves of the conversion live here, in src/util/, because they are the ONE place the
// GUI's Point-mode centre (analysis_panel.cpp) and the CLI's `--center <alt>,<az>` agree on
// what "altitude 43, azimuth 0" means. Pure and stateless, no core or config type in sight —
// the shape AGENTS.md admits under src/util/ — so both sides read it without a second copy,
// and `inline` so the shared build's hidden visibility has no symbol to hide.

// Public on purpose, not tucked in a detail namespace: the CLI (`--radius <deg>` -> radians for
// the request), the CSV formatter (radians -> degrees for the header echo) and the GUI all need
// the same two factors, and a `_detail` name that three other files reach into is a promise the
// name no longer keeps.
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDeg2Rad = kPi / 180.0f;
constexpr float kRad2Deg = 180.0f / kPi;

// Direction light travels -> (altitude, azimuth) in degrees of the sky point it comes from —
// which for an OUTGOING ray direction is exactly the sky point the ray is displayed at.
// Altitude in [-90, 90]; azimuth wrapped into (-180, 180]. `dir` need not be normalized beyond
// |z| <= 1 (z is clamped for the asin).
template <class T>
inline void DirToAltAzT(const T dir[3], T* alt_deg, T* az_deg) {
  const T rad2deg = static_cast<T>(180) / static_cast<T>(3.14159265358979323846);
  const T z = std::max(T(-1), std::min(T(1), dir[2]));
  *alt_deg = std::asin(-z) * rad2deg;
  T az = std::atan2(dir[1], dir[0]) * rad2deg - T(180);
  while (az > T(180)) {
    az -= T(360);
  }
  while (az < T(-180)) {
    az += T(360);
  }
  *az_deg = az;
}

inline void DirToAltAz(const float dir[3], float* alt_deg, float* az_deg) {
  DirToAltAzT<float>(dir, alt_deg, az_deg);
}

inline void DirToAltAz(const double dir[3], double* alt_deg, double* az_deg) {
  DirToAltAzT<double>(dir, alt_deg, az_deg);
}

// The inverse: (altitude, azimuth) in degrees of a sky point -> the unit direction light from it
// travels. Read DirToAltAz backwards:
//   altitude = asin(-z)          -> z = -sin(alt)
//   azimuth  = atan2(y, x) - 180 -> x = -cos(alt)cos(az), y = -cos(alt)sin(az)
// Equivalently: the NEGATION of the point's position vector (cos·cos az, cos·sin az, +sin alt).
// This is the fill for every light-travel-convention consumer — the cone centre above, the
// single-path `target_direction`, the sun's own `incident_direction` (scene_to_analytic.cpp's
// SunIncidentDirection) — NOT a "pointing at the sky point" vector. A caller who wants the
// position vector (a pointing/toward vector, e.g. to compare against directions a ray heads
// toward) wants the negation of this function's output.
// The same formula the annotation overlay's altitude curves use (core/annotation_overlay.cpp).
// One formula for both precisions: the float overload is what the C API's cone centre and the GUI
// use; the double one feeds liblumice_analytic's kernel, which rejects a direction that is not unit
// to 1e-10 — a float-rounded one is not.
template <class T>
inline void AltAzToDirT(T alt_deg, T az_deg, T dir[3]) {
  const T deg2rad = static_cast<T>(3.14159265358979323846) / static_cast<T>(180);
  const T alt = alt_deg * deg2rad;
  const T az = az_deg * deg2rad;
  const T cos_alt = std::cos(alt);
  dir[0] = -cos_alt * std::cos(az);
  dir[1] = -cos_alt * std::sin(az);
  dir[2] = -std::sin(alt);
}

inline void AltAzToDir(float alt_deg, float az_deg, float dir[3]) {
  AltAzToDirT<float>(alt_deg, az_deg, dir);
}

inline void AltAzToDir(double alt_deg, double az_deg, double dir[3]) {
  AltAzToDirT<double>(alt_deg, az_deg, dir);
}

}  // namespace lumice
