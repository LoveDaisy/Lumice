#ifndef LUMICE_GUI_FORWARD_PROJECTION_HPP_
#define LUMICE_GUI_FORWARD_PROJECTION_HPP_

#include <algorithm>
#include <cmath>

#include "gui/gui_constants.hpp"
#include "util/projection_domain.h"

namespace lumice::gui {

// The two historical entry contracts differ in rejection/axis/layout policy, not lens math.
// Screen coordinates are y-up; the unclipped inverse-mirror entry flips full-sky y afterwards.
enum class ForwardPolicy { kScreen, kUnclippedMirror };
struct ForwardPixel {
  float px = 0;
  float py = 0;
  bool valid = false;
};
constexpr float kBehindCameraEps = 1e-5f;

// Unit direction already in camera space (or world space for fixed full-sky lenses).
// No rectangular viewport rejection: a caller drawing segments must let its draw clip handle
// their intersections with the viewport, even when an endpoint lies beyond the rectangle.
inline ForwardPixel ProjectLocalDirection(const float d[3], float width, float height, int lens, float fov,
                                          ForwardPolicy policy) {
  constexpr float kPi = 3.14159265358979323846f;
  const bool screen = policy == ForwardPolicy::kScreen;
  if (!(width > 0 && height > 0 && fov > 0) || !std::isfinite(d[0]) || !std::isfinite(d[1]) || !std::isfinite(d[2]))
    return {};
  const float hf = fov * (0.5f * kPi / 180.0f);
  const float radius = std::min(width, height) * 0.5f;
  const float dual_radius = std::min(width * 0.5f, height) * 0.5f;
  ForwardPixel result{};
  if (lens == kLensTypeLinear || lens == kLensTypeGlobe) {
    if (lens == kLensTypeLinear ? d[2] >= (screen ? -kBehindCameraEps : 0.0f) : d[2] <= 1.0f / kGlobeCameraD)
      return {};
    const float denom = lens == kLensTypeLinear ? -d[2] : kGlobeCameraD - d[2];
    const float focal = radius / std::tan(hf);
    result = { d[0] / denom * focal, d[1] / denom * focal, true };
  } else if ((lens >= kLensTypeFisheyeEqualArea && lens <= kLensTypeFisheyeStereographic) ||
             lens == kLensTypeFisheyeOrthographic) {
    const float theta = std::acos(std::clamp(-d[2], -1.0f, 1.0f));
    const float rho = std::hypot(d[0], d[1]);
    // Preserve the retired mirror's axis convention, including its non-unique antipode.
    if (!screen && rho < 1e-8f)
      return { 0, 0, true };
    float r = 0;
    if (lens == kLensTypeFisheyeEqualArea) {
      r = std::sin(theta * 0.5f) / std::sin(hf * 0.5f);
    } else if (lens == kLensTypeFisheyeEquidist) {
      r = theta / hf;
    } else if (lens == kLensTypeFisheyeStereographic) {
      if (-d[2] < projection_domain::kFisheyeStereographicMinCz)
        return {};
      r = std::tan(theta * 0.5f) / std::tan(hf * 0.5f);
    } else {
      // Orthographic is not injective beyond the equator. The mirror additionally keeps its
      // historical FOV gate; the screen entry keeps its hemisphere epsilon and half-pixel rim.
      if (screen ? d[2] > kBehindCameraEps : theta > hf)
        return {};
      r = std::sin(theta) / std::sin(hf);
      if (screen && r > 1.0f + 0.5f / radius)
        return {};
    }
    // FOV defines the calibration circle, NOT the sky domain. Equal-area/equidistant extend
    // to the antipode (undefined azimuth there); engine trace floors do not shrink this ideal
    // inverse domain. Stereographic's divergent tail is bounded by the shared numerical floor.
    if (screen && rho < 1e-8f && d[2] > 0)
      return {};
    if (rho < 1e-8f)
      return { 0, 0, true };
    const float scale = radius * r / rho;
    result = { d[0] * scale, d[1] * scale, true };
  } else if (lens == kLensTypeRectangular) {
    const float lat = std::asin(std::clamp(-d[2], -1.0f, 1.0f));
    if (!screen && std::abs(lat) >= kPi * 0.5f - 1e-6f)
      return {};
    const float lon = screen && d[0] * d[0] + d[1] * d[1] < 1e-6f ? 0.0f : std::atan2(-d[1], -d[0]);
    const float scale = 2.0f * dual_radius / kPi;
    result = { lon * scale, -lat * scale, true };
  } else if ((lens >= kLensTypeDualFisheyeEqualArea && lens <= kLensTypeDualFisheyeStereographic) ||
             lens == kLensTypeDualFisheyeOrthographic) {
    const bool left = screen ? d[2] < 0.0f : d[2] <= 0.0f;
    const float theta = std::acos(std::clamp(std::abs(d[2]), 0.0f, 1.0f));
    const float phi = screen && std::sin(theta) <= 1e-6f ? 0.0f : std::atan2(-d[0], left ? d[1] : -d[1]);
    float r = 0;
    if (lens == kLensTypeDualFisheyeEqualArea)
      r = std::sin(theta * 0.5f) / std::sin(kPi * 0.25f);
    else if (lens == kLensTypeDualFisheyeEquidist)
      r = theta / (kPi * 0.5f);
    else if (lens == kLensTypeDualFisheyeStereographic)
      r = std::tan(theta * 0.5f);
    else
      r = std::sin(theta);
    if (!screen && r > 1.0f)
      return {};
    result = { (left ? -dual_radius : dual_radius) + r * dual_radius * std::cos(phi), r * dual_radius * std::sin(phi),
               true };
  }
  if (!std::isfinite(result.px) || !std::isfinite(result.py))
    return {};
  return result;
}

}  // namespace lumice::gui
#endif
