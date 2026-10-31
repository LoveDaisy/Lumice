#ifndef LUMICE_GUI_PANORAMA_VIEWPORT_HPP
#define LUMICE_GUI_PANORAMA_VIEWPORT_HPP

// The Analyze workspace's panorama publish arithmetic, in one place.
//
// RenderScenePanorama (analyze_workspace.cpp) is the one site in the tree that hands an ImGui
// window rectangle to a GL viewport: the deferred pass blits the reprojected sky into the
// panorama child window's rect on the main window's framebuffer (RunSharedFrameRenderPass in
// export_fbo_renderer.cpp). With ImGuiConfigFlags_ViewportsEnable on (src/gui/main.cpp), the
// child window's GetWindowPos() — like every ImGui window coordinate — is an OS-screen absolute
// coordinate: the main viewport's origin plus the window-local offset (the MainVpPos idiom in
// app_panels.cpp is the same fact used in the other direction). A GL viewport, by contrast,
// counts main-window-local pixels. The conversion between the two spaces — subtract the main
// viewport's origin, scale by the window's per-axis DPI, flip Y for OpenGL's bottom-up axis —
// is this header's ONE function. The publish site passes the quantities in and keeps no
// formula of its own.
//
// Deliberately free of ImGui / GL / GLFW dependencies: the quantities arrive as plain numbers,
// so the function can be exercised without any context — in particular with a NONZERO
// main-viewport origin, which no ImGui-context-based test can produce (the gui_test harness
// cannot enable ViewportsEnable, and a flipped harness crashes). The nonzero-origin cases in
// test/unit-correctness/gui/test_panorama_viewport.cpp are the red/green nail that holds this
// conversion to its contract.

namespace lumice::gui {

// The panorama child window's rectangle in ImGui's absolute OS-screen coordinate space
// (points): the top-left corner as GetWindowPos() reports it, plus the extent. Under
// ViewportsEnable the corner carries the main viewport's origin offset; with multi-viewport
// off it is already window-local and the origin below reads (0, 0) — which is why the same
// formula serves both configurations.
struct PanoramaScreenRect {
  float x = 0.0f;
  float y = 0.0f;
  float w = 0.0f;
  float h = 0.0f;
};

// The main viewport's origin, in the same absolute OS-screen space as PanoramaScreenRect —
// ImGui::GetMainViewport()->Pos unpacked. The one quantity that turns the absolute rect into
// the window-local geometry the GL viewport needs.
struct PanoramaScreenPoint {
  float x = 0.0f;
  float y = 0.0f;
};

// The main OS window's two sizes: ImGui's DisplaySize in points (the space the rect above
// lives in, and the space the bottom-up flip measures against) and GLFW's framebuffer size in
// pixels (the space the GL viewport counts in). Their per-axis ratios are the DPI scales.
struct PanoramaSurface {
  float display_w = 0.0f;  // ImGuiIO::DisplaySize.x, points
  float display_h = 0.0f;  // ImGuiIO::DisplaySize.y, points
  int fb_w = 0;            // glfwGetFramebufferSize width, pixels
  int fb_h = 0;            // glfwGetFramebufferSize height, pixels
};

// The publish: the GL viewport rect the deferred blit fills — framebuffer pixels, x from the
// window's left edge and y from its BOTTOM (OpenGL is bottom-up) — and the per-axis
// point-to-pixel scales the pick/gesture path needs to map cursor points onto the blit's
// pixels.
struct PanoramaGlViewport {
  int vp_x = 0;
  int vp_y = 0;
  int vp_w = 0;
  int vp_h = 0;
  float dpi_scale_x = 1.0f;
  float dpi_scale_y = 1.0f;
};

// The conversion proper. P1, the property that makes the picture stick to the child window
// while the OS window moves: the output is a function of the window-LOCAL geometry only —
// moving the window translates `rect` and `main_vp_origin` together, the difference is
// untouched, and the published viewport cannot move. P2, the compatibility leg: with the main
// viewport at (0, 0) (multi-viewport off — every existing document, every gui_test, every
// committed reference image) the arithmetic degenerates to the pre-fix publication bitwise.
// The nonzero-origin cases in test_panorama_viewport.cpp hold P1; the (0, 0) case holds P2;
// the fractional-DPI case holds the truncation and the bottom-up Y convention.
inline PanoramaGlViewport PanoramaChildRectToGlViewport(const PanoramaScreenRect& rect,
                                                        const PanoramaScreenPoint& main_vp_origin,
                                                        const PanoramaSurface& surface) {
  PanoramaGlViewport out;
  const float dpi_x = static_cast<float>(surface.fb_w) / surface.display_w;
  const float dpi_y = static_cast<float>(surface.fb_h) / surface.display_h;
  // Window-local points: strip the main viewport's origin off the absolute corner before the
  // GL viewport counts pixels. (The pre-fix formula stopped here at `rect` itself — the defect
  // the nonzero-origin nail caught.)
  const float local_x = rect.x - main_vp_origin.x;
  const float local_y = rect.y - main_vp_origin.y;
  out.vp_x = static_cast<int>(local_x * dpi_x);
  // OpenGL Y is bottom-up (the preview panel's own comment): the region's bottom edge measured
  // from the window's bottom. DisplaySize is already window-local, so the flip consumes the
  // localized geometry too.
  out.vp_y = static_cast<int>((surface.display_h - (local_y + rect.h)) * dpi_y);
  out.vp_w = static_cast<int>(rect.w * dpi_x);
  out.vp_h = static_cast<int>(rect.h * dpi_y);
  out.dpi_scale_x = dpi_x;
  out.dpi_scale_y = dpi_y;
  return out;
}

}  // namespace lumice::gui

#endif  // LUMICE_GUI_PANORAMA_VIEWPORT_HPP
