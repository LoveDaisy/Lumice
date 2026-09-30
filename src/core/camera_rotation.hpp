#ifndef CORE_CAMERA_ROTATION_H_
#define CORE_CAMERA_ROTATION_H_

#include "config/render_config.hpp"
#include "core/geo3d.hpp"
#include "core/math.hpp"

namespace lumice {

// Camera rotation for a RenderConfig — matches RenderConsumer ctor.
inline Rotation MakeCameraRotation(const RenderConfig& cfg) {
  Rotation rot;
  float ax_z[3]{ 0, 0, 1 };
  float ax_y[3]{ 0, 1, 0 };
  rot.Chain({ ax_z, (-90.0f + cfg.view_.ro_) * math::kDegreeToRad })
      .Chain({ ax_y, (90.0f - cfg.view_.el_) * math::kDegreeToRad })
      .Chain({ ax_z, cfg.view_.az_ * math::kDegreeToRad });
  return rot;
}

}  // namespace lumice

#endif  // CORE_CAMERA_ROTATION_H_
