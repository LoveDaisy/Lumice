#include "gui/angular_dist_picker.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "gui/angular_dist_rules.hpp"
#include "gui/annotation_anchors.hpp"  // GuiSunWorldDir
#include "gui/overlay_labels.hpp"      // PixelToWorldDir

namespace lumice::gui {

void AngularDistFamilyCenterDir(AngularDistFamily family, const ViewProjection& view_proj, float sun_altitude_deg,
                                float out[3]) {
  if (family == AngularDistFamily::kSun) {
    GuiSunWorldDir(sun_altitude_deg, out);
    return;
  }
  // The shader's `view_axis = -u_view_matrix[2]`: the third column of the column-major matrix,
  // negated, from the same BuildViewMatrix call PreviewRenderer::Render uploads.
  float vm[9];
  BuildViewMatrix(view_proj.elevation, view_proj.azimuth, view_proj.roll, vm);
  out[0] = -vm[6];
  out[1] = -vm[7];
  out[2] = -vm[8];
}

std::optional<float> AngularDistAtCanvasPixel(AngularDistFamily family, const LUMICE_AnnotationView& view,
                                              const ViewProjection& view_proj, float sun_altitude_deg, int px, int py) {
  if (px < 0 || py < 0 || px >= view.width || py >= view.height) {
    return std::nullopt;
  }
  // Is it sky on the picture: the one authority that knows `visible` and `front`.
  float c_api_dir[3] = { 0.0f, 0.0f, 0.0f };
  int sky = 0;
  if (LUMICE_UnprojectPixel(&view, px, py, c_api_dir, &sky) != LUMICE_OK || sky == 0) {
    return std::nullopt;
  }

  // Which direction the shader draws there: the pixel's CENTRE in the shader's centre-origin, y-up
  // coordinates (`pos = v_ndc * u_resolution * 0.5`), through the shader's own inverse.
  const auto res_x = static_cast<float>(view.width);
  const auto res_y = static_cast<float>(view.height);
  const float sx = static_cast<float>(px) + 0.5f - res_x * 0.5f;
  const float sy = res_y * 0.5f - (static_cast<float>(py) + 0.5f);
  float vm[9];
  BuildViewMatrix(view_proj.elevation, view_proj.azimuth, view_proj.roll, vm);
  float d[3] = { 0.0f, 0.0f, 0.0f };
  bool valid = false;
  PixelToWorldDir(sx, sy, res_x, res_y, view_proj.lens_type, view_proj.fov, vm, &d[0], &d[1], &d[2], &valid);
  if (!valid) {
    return std::nullopt;
  }

  float c[3];
  AngularDistFamilyCenterDir(family, view_proj, sun_altitude_deg, c);
  // The shader's own expression: acos(clamp(dot(world_dir, centre), -1, 1)) * DEG.
  const float dot = std::max(-1.0f, std::min(1.0f, d[0] * c[0] + d[1] * c[1] + d[2] * c[2]));
  constexpr float kRad2Deg = 57.29577951308232f;
  return std::acos(dot) * kRad2Deg;
}

std::vector<float>& AngularDistFamilyAngles(GuiState& state, AngularDistFamily family) {
  return family == AngularDistFamily::kSun ? state.sun_circle_angles : state.view_dist_angles;
}

void ArmAngularDistPicker(GuiState& state, AngularDistFamily family) {
  state.angular_dist_picker.armed = true;
  state.angular_dist_picker.family = family;
  // One click-taking mode at a time (GuiState::AngularDistPicker). The eyedropper's flag is not
  // GuiState's, so the caller that owns it clears it.
  state.analysis.pick_armed = false;
  if (family == AngularDistFamily::kSun) {
    state.show_sun_circles_line = true;
  } else {
    state.show_view_dist_line = true;
  }
}

bool CommitAngularDistPick(std::vector<float>& angles, float angle_deg) {
  if (AngularDistCirclesAtLimit(angles.size())) {
    return false;
  }
  const float a = ClampAngularDistCircleAngle(angle_deg);
  if (AngularDistCircleAlreadyPresent(angles, a)) {
    return false;
  }
  angles.push_back(a);
  std::sort(angles.begin(), angles.end());
  return true;
}

void InjectAngularDistPickPreview(AngularDistFamily family, float angle_deg, OverlayDecoration& overlay) {
  if (family == AngularDistFamily::kSun) {
    overlay.angular_dist_deg.push_back(angle_deg);
    overlay.show_sun_circles = true;
  } else {
    overlay.view_dist_deg.push_back(angle_deg);
    overlay.show_view_dist = true;
  }
}

std::string FormatAngularDistPickReadout(float angle_deg) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.1f\xc2\xb0", angle_deg);
  return buf;
}

}  // namespace lumice::gui
