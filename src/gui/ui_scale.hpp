#ifndef LUMICE_GUI_UI_SCALE_HPP
#define LUMICE_GUI_UI_SCALE_HPP

struct GLFWwindow;
struct ImGuiIO;

namespace lumice::gui {

// UI layout coordinates and framebuffer/device pixels are deliberately separate. On Windows and
// Linux GLFW window coordinates follow monitor scale, while macOS window coordinates remain points
// and only the font raster density follows the backing store.
struct UiScaleParams {
  float layout_scale = 1.0f;
  float raster_density = 1.0f;
};

inline UiScaleParams ResolveUiScaleParams(float monitor_scale, float user_multiplier) {
#if defined(__APPLE__)
  return { user_multiplier, monitor_scale };
#else
  return { monitor_scale * user_multiplier, 1.0f };
#endif
}

// Install style/font inputs without touching the backend texture or window. Startup uses this
// before the backend exists; the runtime entry below adds texture and geometry coordination.
UiScaleParams ApplyUiScaleInputs(ImGuiIO& io, float monitor_scale_x, float monitor_scale_y);

// Product and gui_test share this exact frame-boundary sequence. Returns false only when the GL
// backend cannot upload the rebuilt atlas; g_ui_scale_dirty then remains set for a later retry.
bool RebuildForUiScale(GLFWwindow* window, ImGuiIO& io, float monitor_scale_x, float monitor_scale_y);

}  // namespace lumice::gui

#endif  // LUMICE_GUI_UI_SCALE_HPP
