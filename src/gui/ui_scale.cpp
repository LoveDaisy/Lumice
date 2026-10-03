#include "gui/ui_scale.hpp"

#include <imgui.h>
#include <imgui_impl_opengl3.h>

#include "gui/app.hpp"
#include "gui/gui_logger.hpp"
#include "gui/theme.hpp"

namespace lumice::gui {

UiScaleParams ApplyUiScaleInputs(ImGuiIO& io, float monitor_scale_x, float monitor_scale_y) {
  IM_ASSERT(monitor_scale_x > 0.0f);
  if (monitor_scale_y != monitor_scale_x) {
    GUI_LOG_WARNING("[GUI] UI scale: anisotropic content scale {}x{}; using x", monitor_scale_x, monitor_scale_y);
  }
  const UiScaleParams params = ResolveUiScaleParams(monitor_scale_x, g_ui_scale_multiplier);
  ApplyVisualLanguage(io, params.layout_scale, params.raster_density);
  GUI_LOG_INFO("[GUI] UI scale: monitor {} x user {} -> layout {} raster {}", monitor_scale_x, g_ui_scale_multiplier,
               params.layout_scale, params.raster_density);
  return params;
}

bool RebuildForUiScale(GLFWwindow* window, ImGuiIO& io, float monitor_scale_x, float monitor_scale_y) {
  const unsigned int scale_revision = WindowContentScaleRevision();
  const float user_multiplier = g_ui_scale_multiplier;
  const UiScaleParams params = ApplyUiScaleInputs(io, monitor_scale_x, monitor_scale_y);
  ImGui_ImplOpenGL3_DestroyFontsTexture();
  if (!ImGui_ImplOpenGL3_CreateFontsTexture()) {
    GUI_LOG_ERROR("[GUI] UI scale: failed to upload rebuilt font atlas");
    return false;
  }

  const AspectPreset active_preset = g_state.aspect_preset;
  const bool portrait = g_state.aspect_portrait;
  const float background_ratio = active_preset == AspectPreset::kMatchBg ? g_preview.GetBgAspect() : 0.0f;

  ApplyWindowGeometryForScale(window, params.layout_scale);
  if (active_preset != AspectPreset::kFree) {
    // A synchronous GLFW callback may have observed the floor correction. Restore the user's
    // intent before and after the ratio resize; programmatic changes never mean "Free".
    g_state.aspect_preset = active_preset;
    ApplyAspectRatio(window, active_preset, portrait, background_ratio);
    g_state.aspect_preset = active_preset;
  }
  // Resizing/repositioning can synchronously trigger another monitor notification. Complete only
  // the inputs captured by this rebuild; a newer input must survive for the next frame.
  g_ui_scale_dirty = WindowContentScaleRevision() != scale_revision || g_ui_scale_multiplier != user_multiplier;
  return true;
}

}  // namespace lumice::gui
