// The Raypath Analysis window (src/gui/analysis_panel.cpp), driven through real frames, real
// clicks and a real server: the one layer that can see the wiring the unit cases cannot — the
// preview click reaching LUMICE_UnprojectPixel through the panel's own DPI path, the Analyze
// button reaching LUMICE_StartRaypathAnalysis and its result reaching the list through the
// poller, the slider leaving the lifecycle alone, and the top-bar mutual exclusion.
//
// The scene is the 22-degree halo: one randomly oriented prism under a 20-degree sun, the same
// fixture the C API and e2e layers use for the same run. The camera looks 22 degrees above the
// sun on its azimuth, so the frame's centre pixel sits ON the halo ring — the point a user would
// click to ask "what makes this bright arc" — and a cone around it collects real chains.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "IconsFontAwesome6.h"
#include "gui/analysis_panel.hpp"
#include "gui/analyze_map_view.hpp"
#include "gui/analyze_overview.hpp"
#include "gui/analyze_point_view.hpp"
#include "gui/edit_modals.hpp"
#include "gui/file_io.hpp"
#include "gui/gui_constants.hpp"
#include "gui/gui_logger.hpp"
#include "gui/log_sink.hpp"
#include "gui/panels.hpp"
#include "gui/raypath_segments.hpp"
#include "gui/sim_state_rules.hpp"
#include "gui/theme.hpp"
#include "imgui_internal.h"
#include "include/lumice_base.h"
#include "include/lumice_engine.h"
#include "include/lumice_render.h"
#include "test_gui_shared.hpp"
#include "util/raypath_analysis_display.hpp"
#include "util/sky_direction.hpp"

namespace {

using SimState = gui::GuiState::SimState;

// The analysis tool lives in the Analyze workspace now: the share list in the middle area's
// right column, the ROI controls above the panorama's picture. Both are TOP-LEVEL chrome windows
// (##AnalyzeShareList, ##AnalyzePanorama). The panorama's PICTURE is a same-named CHILD under the
// chrome window — BeginChild spells child names "Parent/Name_<id>", so FindWindowByName still
// lands on the chrome window — and every pixel the pick, the marker and the gestures measure is
// relative to that child's position, so the picture helpers below resolve it by ParentWindow.
// The refs are STATIC (ImGuiTestContext::SetRef stores the const char* itself) and ABSOLUTE
// (the leading "//" makes the engine resolve the first segment as a window name — without it a
// ref whose window name starts with "##" reads as an item-id suffix and every descent fails).
const char* ListWindowRef() {
  return "//##AnalyzeShareList";
}
void SetListRef(ImGuiTestContext* ctx) {
  ctx->SetRef(ListWindowRef());
}
const char* PanoramaRegionRef() {
  return "//##AnalyzePanorama";
}
const char* const kWindowRef = "//##AnalyzeShareList";
const char* const kAnalyzeButton = ICON_FA_PLAY " Analyze";
const char* const kPickButton = ICON_FA_CROSSHAIRS " Pick on the panorama";

const char* const kHalo22Json = R"({
  "crystal": [{"id": 1, "type": "prism", "shape": {"height": 1.2},
               "axis": {"zenith": {"type": "uniform", "mean": 90, "std": 360},
                        "azimuth": {"type": "uniform", "mean": 0, "std": 360}}}],
  "filter": [],
  "scene": {"light_source": {"type": "sun", "altitude": 20.0, "spectrum": "D65"},
            "ray_num": 100000, "max_hits": 7,
            "scattering": [{"prob": 0.0, "entries": [{"crystal": 1, "proportion": 10}]}]},
  "render": [{"id": 1, "lens": {"type": "linear", "fov": 60},
              "resolution": [64, 64], "view": {"elevation": 42}}]
})";

// The fully random prism — the C01 shape (raypath_feature_random_regular.json's crystal, the
// same physics as kHalo22Json with the zenith axis written the one way the survey's routing v1
// domain reads as the full-sphere random family: uniform, centred at the pole). This scene's
// dominant class routes to the full-dimensional arm, so its survey carries the chromatic legs
// the halo scene above is honestly refused — the dispersion-on-screen fixture.
const char* const kFullyRandomJson = R"({
  "crystal": [{"id": 1, "type": "prism", "shape": {"height": 1.2},
               "axis": {"zenith": {"type": "uniform", "mean": 0, "std": 360},
                        "azimuth": {"type": "uniform", "mean": 0, "std": 360},
                        "roll": {"type": "uniform", "mean": 0, "std": 360}}}],
  "filter": [],
  "scene": {"light_source": {"type": "sun", "altitude": 20.0, "spectrum": "D65"},
            "ray_num": 100000, "max_hits": 7,
            "scattering": [{"prob": 0.0, "entries": [{"crystal": 1, "proportion": 10}]}]},
  "render": [{"id": 1, "lens": {"type": "linear", "fov": 60},
              "resolution": [64, 64], "view": {"elevation": 42}}]
})";

// Plate + column in two scattering layers, ms_prob 0.3: the scene whose chains cross layers, so
// its list carries multi-layer rows — the ones whose `display` holds the " -> " joiner. Mirrors
// test/e2e/configs/raypath_analysis_pc_two_layer.json (the e2e layer reads the file; gui_test
// inlines its scenes, as kHalo22Json above).
const char* const kPcTwoLayerJson = R"({
  "crystal": [{"id": 1, "type": "prism", "shape": {"height": 0.3},
               "axis": {"zenith": {"type": "gauss", "mean": 90, "std": 0.5},
                        "azimuth": {"type": "uniform", "mean": 0, "std": 360},
                        "roll": {"type": "uniform", "mean": 0, "std": 360}}},
              {"id": 2, "type": "prism", "shape": {"height": 2.5},
               "axis": {"zenith": {"type": "gauss", "mean": 0, "std": 0.5},
                        "azimuth": {"type": "uniform", "mean": 0, "std": 360},
                        "roll": {"type": "uniform", "mean": 0, "std": 360}}}],
  "filter": [],
  "scene": {"light_source": {"type": "sun", "altitude": 20.0, "spectrum": "D65"},
            "ray_num": 200000, "max_hits": 7,
            "scattering": [{"prob": 0.3, "entries": [{"crystal": 1, "proportion": 10},
                                                     {"crystal": 2, "proportion": 10}]},
                           {"prob": 0.0, "entries": [{"crystal": 1, "proportion": 10},
                                                     {"crystal": 2, "proportion": 10}]}]},
  "render": [{"ev_mode": "absolute", "id": 1, "lens": {"type": "fisheye_equal_area", "fov": 120},
              "resolution": [64, 64], "view": {"elevation": 20}}]
})";

// Same shape as test_gui_sim_smoke.cpp's guard, for the same reason: IM_CHECK returns out of
// the case, and a server left running would be inherited by the next one.
struct ScopedServerGuard {
  ~ScopedServerGuard() {
    if (gui::g_server != nullptr) {
      gui::g_server_poller.Stop();
      LUMICE_StopServer(gui::g_server);
      LUMICE_DestroyServer(gui::g_server);
      gui::g_server = nullptr;
      gui::g_state.run_intent = gui::RunIntent::kNone;
      gui::g_state.analysis.started = false;
      gui::g_state.analysis_run_in_progress = false;
    }
    gui::g_state.use_gpu_backend = false;
    gui::ResetServerConstructionTrackers();
  }
};

template <typename Pred>
bool DriveUntil(ImGuiTestContext* ctx, Pred pred, int timeout_s) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_s);
  while (!pred()) {
    if (std::chrono::steady_clock::now() > deadline) {
      return false;
    }
    ctx->Yield();
  }
  return true;
}

// Server up, halo scene loaded, one finite render run to completion — the state Analyze needs.
// `infinite` leaves the render running instead, for the mutual-exclusion case and for the
// run-after-analysis cases (an unbounded analysis is the one the cone target ends). `gpu` asks
// for the GPU backend the way the Settings box does: DoRun reconstructs the server for it
// (Metal here; where no device is available ResolveGpuBackend falls back to CPU, as in
// test_color_window.cpp's GPU case), so the CPU server created below is only ever the seed.
bool BringUpScene(ImGuiTestContext* ctx, const char* scene_json, bool infinite, bool gpu) {
  ResetTestState();
  gui::g_server = LUMICE_CreateServer();
  IM_CHECK_RETV(gui::g_server != nullptr, false);
  gui::ResetServerConstructionTrackers();  // the seed is a CPU server; make the tracker say so
  LUMICE_SetLogLevel(gui::g_server, static_cast<LUMICE_LogLevel>(g_core_log_level));
  IM_CHECK_RETV(gui::DeserializeFromJson(scene_json, gui::g_state), false);
  gui::g_state.renderer.sim_resolution_index = 0;
  gui::g_state.sim.infinite = infinite;
  gui::g_state.sim.ray_num_millions = 0.1f;
  gui::g_state.use_gpu_backend = gpu;
  ctx->Yield(2);
  gui::DoRun(/*user_initiated=*/true);
  if (infinite) {
    IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kSimulating; }, 10), false);
  } else {
    IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kDone; }, 60), false);
    // A completed run has put a texture up, so the preview viewport the click is measured in is
    // live. (Mid-run it may not be yet — the first upload waits on the quality gate.)
    IM_CHECK_RETV(gui::g_preview_vp.active, false);
  }
  return true;
}

bool BringUpHaloScene(ImGuiTestContext* ctx, bool infinite, bool gpu = false) {
  return BringUpScene(ctx, kHalo22Json, infinite, gpu);
}

// Read the analysis window's live rectangle out of the default framebuffer and write it as a PNG,
// the way functional/test_theme_scan.cpp's ExportRegion does (same flip, same Retina scale, same
// clip to the framebuffer). Not compared against anything here — the file is for a human's eyes.
bool SaveWindowPng(ImGuiTestContext* ctx, const char* window_ref, const std::string& path) {
  ImGuiWindow* win = ctx->GetWindowByRef(window_ref);
  IM_CHECK_RETV(win != nullptr, false);
  const ImGuiIO& io = ImGui::GetIO();
  const float sx = io.DisplayFramebufferScale.x;
  const float sy = io.DisplayFramebufferScale.y;
  const float fb_w = io.DisplaySize.x * sx;
  const float fb_h = io.DisplaySize.y * sy;
  const ImVec2 vp_pos = ImGui::GetMainViewport()->Pos;
  const float x0 = std::max(0.0f, (win->Pos.x - vp_pos.x) * sx);
  const float y0 = std::max(0.0f, (win->Pos.y - vp_pos.y) * sy);
  const float x1 = std::min(fb_w, (win->Pos.x - vp_pos.x + win->Size.x) * sx);
  const float y1 = std::min(fb_h, (win->Pos.y - vp_pos.y + win->Size.y) * sy);
  g_fullframe_capture.Reset();
  g_fullframe_capture.rect_x = static_cast<int>(std::lround(x0));
  g_fullframe_capture.rect_y = static_cast<int>(std::lround(fb_h - y1));
  g_fullframe_capture.rect_w = static_cast<int>(std::lround(x1 - x0));
  g_fullframe_capture.rect_h = static_cast<int>(std::lround(y1 - y0));
  g_fullframe_capture.requested.store(true);
  for (int i = 0; i < 10 && !g_fullframe_capture.done.load(); ++i) {
    ctx->Yield(1);
  }
  IM_CHECK_RETV(g_fullframe_capture.done.load(), false);
  IM_CHECK_RETV(g_fullframe_capture.width > 0 && g_fullframe_capture.height > 0, false);
  const std::vector<unsigned char> rgb = lumice::test::StripAlpha(
      g_fullframe_capture.pixels.data(), g_fullframe_capture.width, g_fullframe_capture.height);
  IM_CHECK_RETV(
      lumice::test::SavePng(path.c_str(), rgb.data(), g_fullframe_capture.width, g_fullframe_capture.height, 3), false);
  ctx->LogInfo("[raypath_analysis] window %s -> %s (%dx%d)", window_ref, path.c_str(), g_fullframe_capture.width,
               g_fullframe_capture.height);
  return true;
}

// The result list is a ScrollY table, i.e. a child window, and only a row the table has on
// screen is findable by label (a clipped item never reports its label to the engine; the engine's
// own scroll-and-retry fallback pans the window given as ref, not the child inside it). So: pan
// the table's child a page at a time until the label resolves or the child is at its end.
// Returns the item's id, 0 when no row carries the label at any scroll position.
ImGuiID FindListRowScrolling(ImGuiTestContext* ctx, const std::string& label) {
  ImGuiWindow* analysis = ctx->GetWindowByRef(kWindowRef);
  IM_CHECK_RETV(analysis != nullptr, 0);
  ImGuiWindow* table_child = nullptr;
  for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
    if (w->ParentWindow == analysis && std::strstr(w->Name, "##analysis_rows") != nullptr) {
      table_child = w;
      break;
    }
  }
  IM_CHECK_RETV(table_child != nullptr, 0);
  const std::string path = "**/" + label;
  table_child->Scroll.y = 0.0f;
  ctx->Yield(2);
  while (true) {
    const ImGuiID id = ctx->ItemInfo(path.c_str(), ImGuiTestOpFlags_NoError).ID;
    if (id != 0) {
      return id;
    }
    if (table_child->Scroll.y >= table_child->ScrollMax.y) {
      return 0;
    }
    table_child->Scroll.y =
        std::min(table_child->ScrollMax.y, table_child->Scroll.y + table_child->InnerRect.GetHeight());
    ctx->Yield(2);
  }
}

void OpenWindow(ImGuiTestContext* ctx) {
  // "Opening the analysis tool" IS entering the Analyze workspace now: the switch's Analyze slot
  // brings up the frame whose share list and panorama ROI this suite drives. Idempotent — a case
  // already in Analyze just yields.
  if (gui::g_state.workspace.mode != gui::GuiState::WorkspaceMode::kAnalyze) {
    ctx->ItemClick("##TopBar/" ICON_FA_ROUTE " Analyze");
    ctx->Yield(3);
  }
  ctx->Yield(1);
}

// The panorama's PICTURE region. ##AnalyzePanorama exists twice: the top-level chrome window
// (the Region radios live there — FindWindowByName lands on it, its id being the name's own
// hash) and, inside it, the child the deferred pass blits the picture under.
ImGuiWindow* PanoramaPictureWindow() {
  for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
    if (w != nullptr && w->Active && w->ParentWindow != nullptr &&
        std::strstr(w->Name, "##AnalyzePanorama") != nullptr) {
      return w;
    }
  }
  return nullptr;
}

// A child region's live ImGuiWindow under the lower row (##AnalyzeRaypathView / ##AnalyzeRaypath3D):
// child windows register as "Parent/Name_<id>", so a ref cannot address them directly — resolve by
// the parent's exact top-level name and a name fragment (the same lookup shape
// PanoramaPictureWindow and FindListRowScrolling use).
ImGuiWindow* LowerRowRegionWindow(const char* name_fragment) {
  for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
    if (w != nullptr && w->Active && w->ParentWindow != nullptr &&
        std::strcmp(w->ParentWindow->Name, "##AnalyzeLowerRow") == 0 &&
        std::strstr(w->Name, name_fragment) != nullptr) {
      return w;
    }
  }
  return nullptr;
}

// Click the PANORAMA's picture centre with the pick armed; returns the canvas pixel the click
// landed on (the panorama's own canvas, g_panorama_vp). The pick's picture is the Analyze
// workspace's panorama now — the preview no longer hosts the cone gestures.
gui::CanvasPixel PickPanoramaCentre(ImGuiTestContext* ctx) {
  ImGuiWindow* pano = PanoramaPictureWindow();
  IM_CHECK_SILENT_RETV(pano != nullptr, gui::CanvasPixel{});
  const float px_pt = pano->Pos.x + pano->Size.x * 0.5f + pano->Size.x * 0.25f;  // a quarter right of centre
  const float py_pt = pano->Pos.y + pano->Size.y * 0.5f;
  ctx->MouseMoveToPos(ImVec2(px_pt, py_pt));
  ctx->Yield(2);
  // Where the click actually lands, from the mouse's own position: MouseMoveToPos can be a pixel
  // off the request, and the oracle must be built from the same pixel the panel saw.
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  const std::optional<gui::CanvasPixel> px =
      gui::PreviewPointToCanvasPixel(mouse.x - pano->Pos.x, mouse.y - pano->Pos.y, gui::g_panorama_vp.dpi_scale_x,
                                     gui::g_panorama_vp.dpi_scale_y, gui::g_panorama_vp.vp_w, gui::g_panorama_vp.vp_h);
  IM_CHECK_SILENT_RETV(px.has_value(), gui::CanvasPixel{});
  ctx->MouseClick(0);
  ctx->Yield(2);
  return *px;
}

// Click the panorama's picture AT a sky point (altitude/azimuth in degrees): the projected pixel,
// through the same annotation view the picture is drawn with — the click and the cone it places
// share one projection, so the cone centre is the point named, not a rounding away from it.
bool PickPanoramaAt(ImGuiTestContext* ctx, float alt_deg, float az_deg) {
  ImGuiWindow* picture = PanoramaPictureWindow();
  IM_CHECK_RETV(picture != nullptr, false);
  float dir[3] = { 0.0f, 0.0f, 0.0f };
  lumice::AltAzToDir(alt_deg, az_deg, dir);
  const LUMICE_AnnotationView view =
      gui::PanoramaAnnotationView(gui::g_state, gui::g_panorama_vp.vp_w, gui::g_panorama_vp.vp_h);
  float fx = 0.0f;
  float fy = 0.0f;
  int valid = 0;
  IM_CHECK_RETV(LUMICE_ProjectDirection(&view, dir, &fx, &fy, &valid) == LUMICE_OK && valid == 1, false);
  float x_pt = 0.0f;
  float y_pt = 0.0f;
  gui::CanvasPixelToPreviewPoint(static_cast<int>(std::lround(fx)), static_cast<int>(std::lround(fy)),
                                 gui::g_panorama_vp.dpi_scale_x, gui::g_panorama_vp.dpi_scale_y, &x_pt, &y_pt);
  ctx->MouseMoveToPos(ImVec2(picture->Pos.x + x_pt, picture->Pos.y + y_pt));
  ctx->Yield(2);
  ctx->MouseClick(0);
  ctx->Yield(2);
  return gui::g_state.analysis.cone_center_valid;
}

// The cone marker's position on screen this frame (the panel's own projection, through the DPI
// path the drawing uses), or nullopt when there is none.
std::optional<ImVec2> MarkerScreenPos(ImGuiTestContext* ctx) {
  (void)ctx;
  ImGuiWindow* w = PanoramaPictureWindow();
  if (w == nullptr) {
    return std::nullopt;
  }
  const LUMICE_AnnotationView view =
      gui::PanoramaAnnotationView(gui::g_state, gui::g_panorama_vp.vp_w, gui::g_panorama_vp.vp_h);
  const std::optional<gui::CanvasPixel> px = gui::ProjectConeCenterMarker(gui::g_state, view);
  if (!px.has_value()) {
    return std::nullopt;
  }
  float x_pt = 0.0f;
  float y_pt = 0.0f;
  gui::CanvasPixelToPreviewPoint(px->px, px->py, gui::g_panorama_vp.dpi_scale_x, gui::g_panorama_vp.dpi_scale_y, &x_pt,
                                 &y_pt);
  return ImVec2(w->Pos.x + x_pt, w->Pos.y + y_pt);
}

// The colour on screen at one ImGui-space point, read back off the default framebuffer through the
// harness's sub-region capture (the same read SaveWindowPng makes for a whole window). A 3x3 patch
// is captured and its middle pixel returned, so a half-pixel of rounding on a Retina scale cannot
// put the sample on a neighbour.
bool ReadScreenPixel(ImGuiTestContext* ctx, ImVec2 pos, unsigned char out_rgb[3]) {
  const ImGuiIO& io = ImGui::GetIO();
  const float sx = io.DisplayFramebufferScale.x;
  const float sy = io.DisplayFramebufferScale.y;
  const float fb_h = io.DisplaySize.y * sy;
  const ImVec2 vp_pos = ImGui::GetMainViewport()->Pos;
  const int cx = static_cast<int>(std::lround((pos.x - vp_pos.x) * sx));
  const int cy = static_cast<int>(std::lround((pos.y - vp_pos.y) * sy));  // top-down
  g_fullframe_capture.Reset();
  g_fullframe_capture.rect_x = cx - 1;
  g_fullframe_capture.rect_y = static_cast<int>(std::lround(fb_h)) - cy - 2;  // GL origin: bottom-left
  g_fullframe_capture.rect_w = 3;
  g_fullframe_capture.rect_h = 3;
  g_fullframe_capture.requested.store(true);
  for (int i = 0; i < 10 && !g_fullframe_capture.done.load(); ++i) {
    ctx->Yield(1);
  }
  IM_CHECK_RETV(g_fullframe_capture.done.load(), false);
  IM_CHECK_RETV(g_fullframe_capture.width == 3 && g_fullframe_capture.height == 3, false);
  const unsigned char* px = g_fullframe_capture.pixels.data() + (1 * 3 + 1) * 4;
  out_rgb[0] = px[0];
  out_rgb[1] = px[1];
  out_rgb[2] = px[2];
  return true;
}

// Whether an 8-bit colour is the accent colour the marker is drawn in, within a tolerance that
// covers the framebuffer's quantisation but not a dimmed or covered pixel (the modal's dim alone
// takes every channel down by 55%).
bool IsAccentColour(const unsigned char rgb[3]) {
  const ImVec4 accent = gui::AccentColor();
  const int want[3] = { static_cast<int>(std::lround(accent.x * 255.0f)),
                        static_cast<int>(std::lround(accent.y * 255.0f)),
                        static_cast<int>(std::lround(accent.z * 255.0f)) };
  for (int c = 0; c < 3; ++c) {
    if (std::abs(static_cast<int>(rgb[c]) - want[c]) > 12) {
      return false;
    }
  }
  return true;
}

// The direction LUMICE_UnprojectPixel gives for the mouse's CURRENT position on the preview — the
// oracle for "the centre is the direction under the cursor", built from the same pixel the panel
// saw (PickPreviewCentre's reasoning).
bool UnprojectMouse(float out[3]) {
  ImGuiWindow* w = PanoramaPictureWindow();
  if (w == nullptr) {
    return false;
  }
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  const std::optional<gui::CanvasPixel> px =
      gui::PreviewPointToCanvasPixel(mouse.x - w->Pos.x, mouse.y - w->Pos.y, gui::g_panorama_vp.dpi_scale_x,
                                     gui::g_panorama_vp.dpi_scale_y, gui::g_panorama_vp.vp_w, gui::g_panorama_vp.vp_h);
  if (!px.has_value()) {
    return false;
  }
  const LUMICE_AnnotationView view =
      gui::PanoramaAnnotationView(gui::g_state, gui::g_panorama_vp.vp_w, gui::g_panorama_vp.vp_h);
  int valid = 0;
  return LUMICE_UnprojectPixel(&view, px->px, px->py, out, &valid) == LUMICE_OK && valid == 1;
}

// Whether the pick banner is on screen: the banner child exists and was submitted last frame
// (a window that stopped being submitted stays in ImGui's window list, so existence alone says
// nothing — WasActive does). The banner is a CHILD of the share-list window, and BeginChild
// spells child names "Parent/Name_<id>" — a form the engine's window refs cannot address — so
// the pool is scanned for the name segment instead.
bool PickBannerVisible(ImGuiTestContext* ctx) {
  (void)ctx;
  for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
    if (w != nullptr && w->WasActive && std::strstr(w->Name, "##pick_banner") != nullptr) {
      return true;
    }
  }
  return false;
}

// Point mode, a centre picked on the panorama, Analyze pressed: the run is in progress on
// return. The ROI controls (region radios, the pick arming) sit on the PANORAMA side; Analyze
// sits with the list. The pick targets the halo's own centre — the direction the document's
// camera looks (22 degrees above the sun on its azimuth), the point the OLD default centre (the
// preview's middle pixel) sat on for free. The panorama's default centre is the fisheye's
// middle (the zenith), off the halo, so the point analyses that assert chain identities pick
// this direction explicitly: the 3-5 / 3-7 rows the fixture's assertions name live in this cone.
bool StartPointAnalysis(ImGuiTestContext* ctx) {
  ctx->SetRef(PanoramaRegionRef());
  ctx->ItemClick("Point");
  ctx->Yield(2);  // CONE's pick-button row is submitted the frame after the radio flips
  ctx->ItemClick(kPickButton);
  ctx->Yield(1);
  IM_CHECK_RETV(gui::g_state.analysis.pick_armed, false);
  ctx->SetRef("");
  IM_CHECK_RETV(PickPanoramaAt(ctx, gui::g_state.renderer.elevation, gui::g_state.renderer.azimuth), false);
  IM_CHECK_RETV(gui::g_state.analysis.cone_center_valid, false);
  // The Analyze button sits with the list sections.
  SetListRef(ctx);
  ctx->ItemClick(kAnalyzeButton);
  ctx->SetRef("");
  IM_CHECK_RETV(gui::g_state.analysis.started, false);
  return true;
}

// A Point analysis on a FINITE budget (the scene's, seeded into the panel): it ends by itself
// on that budget, the only end a run has apart from Stop.
bool RunPointAnalysisToCompletion(ImGuiTestContext* ctx) {
  IM_CHECK_RETV(StartPointAnalysis(ctx), false);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return !gui::g_state.analysis_run_in_progress && gui::g_state.analysis_result.payload != nullptr; },
          60),
      false);
  return true;
}

// A Point analysis on an UNLIMITED budget, ended from the window's own Stop once the list has
// rows: the in-progress flag falls, and the list stays — non-empty, never older than what was
// on show. This is the user's way of ending a cone analysis (there is no cone stop target).
bool RunPointAnalysisThenStop(ImGuiTestContext* ctx) {
  IM_CHECK_RETV(StartPointAnalysis(ctx), false);
  IM_CHECK_RETV(gui::g_state.analysis.infinite, false);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::g_state.analysis_result.payload != nullptr &&
                             !gui::g_state.analysis_result.payload->entries.empty();
                    },
                    30),
                false);
  const auto partial = gui::g_state.analysis_result.payload;
  SetListRef(ctx);
  IM_CHECK_RETV(ctx->ItemInfo(ICON_FA_STOP " Stop").ID != 0, false);
  ctx->ItemClick(ICON_FA_STOP " Stop");
  ctx->SetRef("");
  IM_CHECK_RETV(!gui::g_state.analysis.started, false);
  // The picture stays the render's, so sim_state returns to kDone once the async stop drains.
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return !gui::g_state.analysis_run_in_progress && gui::g_state.sim_state == SimState::kDone; }, 20),
      false);
  IM_CHECK_RETV(gui::g_state.analysis_result.payload != nullptr, false);
  IM_CHECK_RETV(!gui::g_state.analysis_result.payload->entries.empty(), false);
  IM_CHECK_RETV(gui::g_state.analysis_result.payload->snapshot_generation >= partial->snapshot_generation, false);
  return true;
}

// The owner's own sequence: Run, Analyze, Run again — and the second Run RENDERS. This is the
// one path through the panel that nothing above drives (every case so far ends on the analysis
// or on the slider), and the one that once went black: a cone-ROI analysis left a server-side
// flag up at the session switch, and the render session after it traced nothing — no batch, no
// frame, a preview stuck on "Simulating" until a backend switch rebuilt the server. That flag
// (the cone stop target's) no longer exists; what this case guards now is the session switch
// itself — that a stopped analysis leaves nothing behind that keeps the next render from
// producing. The server case that pins the mechanism is
// ServerAnalysisRun.RenderAfterStoppedAnalysisProducesAFrame; this one pins the user's view of
// it: sim_state reaches kDone and a texture goes up.
//
// The analysis is an UNLIMITED one, ended from the window's Stop (RunPointAnalysisThenStop): the
// render whose scene it inherits is an unbounded one, stopped by hand once it has put a picture
// up (the pick needs a live preview), so the panel's seeded budget is unlimited too. The run
// after the analysis is then made finite so that "it rendered" can be read as kDone rather than
// as "never ended".
//
// `exclude` adds the owner's step in between: select the top chain, press "Exclude this
// raypath", so the second Run commits a document with a filter in it. The bug does not need it
// (the server case has none), and it is driven here so the sequence as reported stays covered.
bool RunAfterAnalysisRenders(ImGuiTestContext* ctx, bool exclude, bool gpu) {
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/true, gpu), false);
  IM_CHECK_RETV(
      DriveUntil(ctx, [] { return gui::g_preview_vp.active && gui::g_state.texture_upload_count > 0; }, 30), false);
  gui::DoStop();
  IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kDone; }, 20), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunPointAnalysisThenStop(ctx), false);
  // The analysis was stopped, not completed (a stop is a reset, so the server reads idle), and
  // the list on show is the cone's.
  LUMICE_SimLifecycleResult after_analysis{};
  LUMICE_GetSimLifecycle(gui::g_server, &after_analysis);
  IM_CHECK_RETV(after_analysis.lifecycle == static_cast<int>(LUMICE_LIFECYCLE_IDLE), false);
  IM_CHECK_RETV(gui::g_state.analysis_result.payload->roi_mode == LUMICE_RAYPATH_ROI_CONE, false);

  if (exclude) {
    const auto& view_result = gui::g_state.analysis_result;
    IM_CHECK_RETV(!view_result.display_order.empty(), false);
    const int top = view_result.display_order[0];
    SetListRef(ctx);
    // The row is a Selectable under PushID(original index) inside the results table; the
    // wildcard finds it by label through the table's and the id's anonymous path segments.
    const std::string top_display = view_result.payload->entries[static_cast<size_t>(top)].display;
    const std::string row = std::string("**/") + top_display;
    ctx->ItemClick(row.c_str());
    ctx->Yield(1);
    IM_CHECK_RETV(gui::g_state.analysis.selected_entry.has_value(), false);
    IM_CHECK_RETV(*gui::g_state.analysis.selected_entry == top_display, false);
    IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo(ICON_FA_BAN " Exclude this raypath")), false);
    ctx->ItemClick(ICON_FA_BAN " Exclude this raypath");
    ctx->Yield(1);
    // The filter is in the document, an Out one, so the same crystal stays excludable: the
    // button is still enabled, and a second press of it for the same chain is the idempotent
    // path — driven here under real frames, and read back as "one filter, one row, unchanged".
    IM_CHECK_RETV(gui::g_state.filters.size() == 1u, false);
    IM_CHECK_RETV(gui::g_state.filters[0].action == 1, false);
    IM_CHECK_RETV(gui::g_state.filters[0].param.size() == 1u, false);
    IM_CHECK_RETV(gui::EvaluateExcludeEligibility(gui::g_state, nullptr) == gui::ExcludeEligibility::kOk, false);
    IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo(ICON_FA_BAN " Exclude this raypath")), false);
    ctx->ItemClick(ICON_FA_BAN " Exclude this raypath");
    ctx->SetRef("");
    ctx->Yield(1);
    IM_CHECK_RETV(gui::g_state.filters.size() == 1u, false);
    IM_CHECK_RETV(gui::g_state.filters[0].param.size() == 1u, false);
  }

  // The second Run, from the top bar, on a finite budget.
  gui::g_state.sim.infinite = false;
  gui::g_state.sim.ray_num_millions = 0.1f;
  const unsigned long long uploads_before = gui::g_state.texture_upload_count;
  const unsigned long long serial_before = gui::g_state.last_uploaded_texture_serial;
  ctx->Yield(1);
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo("##TopBar/" ICON_FA_PLAY " Run")), false);
  ctx->ItemClick("##TopBar/" ICON_FA_PLAY " Run");
  IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kSimulating; }, 10), false);
  // Both halves of "it rendered": the run ended, and its picture went up. The document is
  // dirty here (the budget edit above; the Exclude before it), and a dirty document's finished
  // run reads kModified, not kDone — the real main loop clears dirty on its 70 ms auto-commit
  // tick while simulating (main.cpp), a tick gui_test's loop only has under --main-loop-commit,
  // so it is cleared by hand at the same moment the app would.
  gui::g_state.dirty = false;
  IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kDone; }, 60), false);
  IM_CHECK_RETV(
      DriveUntil(ctx, [uploads_before] { return gui::g_state.texture_upload_count > uploads_before; }, 10), false);
  IM_CHECK_RETV(gui::g_state.last_uploaded_texture_serial != serial_before, false);
  return true;
}

// Replace the app's server with one that has never committed anything, so that whatever the
// next analysis traces cannot have come from a run this server saw.
bool ReplaceServerWithAFreshOne(ImGuiTestContext* ctx) {
  gui::g_server_poller.Stop();
  if (gui::g_server != nullptr) {
    LUMICE_StopServer(gui::g_server);
    LUMICE_DestroyServer(gui::g_server);
  }
  gui::g_server = LUMICE_CreateServer();
  IM_CHECK_RETV(gui::g_server != nullptr, false);
  gui::ResetServerConstructionTrackers();
  LUMICE_SetLogLevel(gui::g_server, static_cast<LUMICE_LogLevel>(g_core_log_level));
  ctx->Yield(1);
  LUMICE_SimLifecycleResult lc{};
  LUMICE_GetSimLifecycle(gui::g_server, &lc);
  IM_CHECK_RETV(lc.epoch == 0u, false);
  return true;
}

// Whole-sky mode, Analyze pressed, driven until the run ends on the panel's finite budget and
// the list is on show. `saw_simulating` reports whether sim_state ever read kSimulating on the
// way — the analysis must not be mistaken for a render, and that is only observable by sampling
// every frame rather than by looking at the end state.
bool RunWholeSkyAnalysisToCompletion(ImGuiTestContext* ctx, bool* saw_simulating) {
  ctx->SetRef(PanoramaRegionRef());
  ctx->ItemClick("Whole sky");
  SetListRef(ctx);
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo(kAnalyzeButton)), false);
  SetListRef(ctx);
  ctx->ItemClick(kAnalyzeButton);
  ctx->SetRef("");
  IM_CHECK_RETV(gui::g_state.analysis.started, false);
  IM_CHECK_RETV(!gui::g_state.analysis.infinite, false);
  bool simulating = false;
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [&simulating] {
                      simulating = simulating || gui::g_state.sim_state == SimState::kSimulating;
                      return !gui::g_state.analysis_run_in_progress && gui::g_state.analysis_result.payload != nullptr;
                    },
                    60),
                false);
  if (saw_simulating != nullptr) {
    *saw_simulating = simulating;
  }
  IM_CHECK_RETV(!gui::g_state.analysis_result.payload->entries.empty(), false);
  IM_CHECK_RETV(!gui::g_state.analysis_result.display_order.empty(), false);
  return true;
}

std::string TopChainDisplay() {
  const auto& view = gui::g_state.analysis_result;
  return view.payload->entries[static_cast<size_t>(view.display_order[0])].display;
}

// Write the sky-point intent for (altitude, azimuth) labels of the sky point itself: one named
// conversion for every anchor in this suite — the intent carries TRAVEL (the negated position),
// the sign convention spelled here once instead of five hand-written trig copies.
void SetSkyPointTarget(double alt_deg, double az_deg) {
  const double pos[3] = { std::cos(alt_deg * lumice::kDeg2Rad) * std::cos(az_deg * lumice::kDeg2Rad),
                          std::cos(alt_deg * lumice::kDeg2Rad) * std::sin(az_deg * lumice::kDeg2Rad),
                          std::sin(alt_deg * lumice::kDeg2Rad) };
  gui::g_state.analysis.sky_point.valid = true;
  for (int k = 0; k < 3; ++k) {
    gui::g_state.analysis.sky_point.dir[k] = static_cast<float>(-pos[k]);
  }
}

// Find and click the list row whose display spelling is exactly `display` (the scroll-to shape
// the multilayer leg established), returning the selection identity it arms.
std::optional<gui::SelectedClassIdentity> SelectRowByDisplay(ImGuiTestContext* ctx, const char* display) {
  std::string found;
  for (const int idx : gui::g_state.analysis_result.display_order) {
    const auto& e = gui::g_state.analysis_result.payload->entries[static_cast<size_t>(idx)];
    if (std::strcmp(e.display, display) == 0) {
      found = e.display;
      break;
    }
  }
  if (found.empty()) {
    return std::nullopt;
  }
  const std::string label = gui::AnalysisRowLabel(found, gui::AnalysisCoreToPoolMap(gui::g_state));
  SetListRef(ctx);
  const ImGuiID row = FindListRowScrolling(ctx, label);
  if (row == 0) {
    return std::nullopt;
  }
  ctx->ItemClick(row);
  ctx->SetRef("");
  return gui::SelectedClassIdentityFor(gui::g_state);
}

// In-frame mode, Analyze pressed, driven until the run ends on the panel's finite budget. The
// radio gates on the document having shown a picture (HasEverShownPicture — the same predicate
// the Analyze button reads), which the completed run the caller brought up satisfied; the
// Analyze workspace hosts no preview, so the frame the analysis reads is the document's own
// resolution (DoAnalyze's IN_FRAME fallback).
bool RunInFrameAnalysisToCompletion(ImGuiTestContext* ctx) {
  SetListRef(ctx);
  ctx->SetRef(PanoramaRegionRef());
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo("In frame")), false);
  ctx->SetRef(PanoramaRegionRef());
  ctx->ItemClick("In frame");
  SetListRef(ctx);
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo(kAnalyzeButton)), false);
  SetListRef(ctx);
  ctx->ItemClick(kAnalyzeButton);
  ctx->SetRef("");
  IM_CHECK_RETV(gui::g_state.analysis.started, false);
  IM_CHECK_RETV(!gui::g_state.analysis.infinite, false);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return !gui::g_state.analysis_run_in_progress && gui::g_state.analysis_result.payload != nullptr; },
          60),
      false);
  IM_CHECK_RETV(gui::g_state.analysis_result.payload->roi_mode == LUMICE_RAYPATH_ROI_IN_FRAME, false);
  return true;
}

// A sink of our own on the GUI logger for the duration of a case, so what DoRun logs can be read
// back. gui_test does not attach one by itself: g_imgui_log_sink exists only under --log-panel,
// and even then it is fed by the CORE log callback, not by the GUI logger DoRun writes to
// (test_log_panel.cpp attaches its own for the same reason). Restores the sink list on the way
// out — it is process-wide and gui_test is one process.
struct ScopedGuiLogCapture {
  std::shared_ptr<gui::ImGuiLogSink> sink;
  std::vector<spdlog::sink_ptr> prev_sinks;

  ScopedGuiLogCapture() : sink(std::make_shared<gui::ImGuiLogSink>()), prev_sinks(gui::GetGuiLogger().sinks()) {
    gui::GetGuiLogger().sinks().push_back(sink);
  }
  ~ScopedGuiLogCapture() { gui::GetGuiLogger().sinks() = prev_sinks; }

  bool Contains(const char* needle) const {
    bool found = false;
    sink->ForEachEntry(
        [&](size_t, const gui::LogEntry& e) { found = found || e.message.find(needle) != std::string::npos; });
    return found;
  }
};

const char* const kMismatchNeedle = "predict/actual mismatch";

// The Run after a COMPLETED analysis, from the top bar: it renders, and DoRun did not have to
// fall back on its safety net. The net is the "predict/actual mismatch" branch — the GUI
// predicted that the commit would reuse the previous session's consumers, the server rebuilt
// them (it never reuses an analysis session's), and the poller was stopped late. DoRun avoids
// it by reading the server's own session kind (LUMICE_GetSimLifecycle's session_kind) into its
// rebuild predicate before the commit; this is the one layer that can see the two agree, and it
// reads the warning's absence off a sink attached for the purpose. The capture is armed only
// for the second Run, so the analysis's own logging cannot mask a warning it never printed —
// and a probe line proves the sink is receiving before "absent" is read as "did not happen".
bool RunAfterCompletedAnalysisHasNoRebuildMismatch(ImGuiTestContext* ctx) {
  IM_CHECK_RETV(gui::g_state.sim_state == SimState::kDone, false);
  IM_CHECK_RETV(!gui::g_state.analysis_run_in_progress, false);
  LUMICE_SimLifecycleResult before_run{};
  LUMICE_GetSimLifecycle(gui::g_server, &before_run);
  IM_CHECK_RETV(before_run.session_kind == static_cast<int>(LUMICE_SESSION_ANALYSIS), false);

  ScopedGuiLogCapture capture;
  GUI_LOG_WARNING("[gui_test] log capture probe");
  IM_CHECK_RETV(capture.Contains("log capture probe"), false);

  const unsigned long long uploads_before = gui::g_state.texture_upload_count;
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo("##TopBar/" ICON_FA_PLAY " Run")), false);
  ctx->ItemClick("##TopBar/" ICON_FA_PLAY " Run");
  IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kSimulating; }, 10), false);
  IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kDone; }, 60), false);
  IM_CHECK_RETV(
      DriveUntil(ctx, [uploads_before] { return gui::g_state.texture_upload_count > uploads_before; }, 10), false);
  LUMICE_SimLifecycleResult after_run{};
  LUMICE_GetSimLifecycle(gui::g_server, &after_run);
  IM_CHECK_RETV(after_run.session_kind == static_cast<int>(LUMICE_SESSION_RENDER), false);
  IM_CHECK_RETV(!capture.Contains(kMismatchNeedle), false);
  return true;
}

// Exclude while the Immediate-mode editor is open on the crystal's own entry, Filter tab showing.
//
// The editor's buffers are a copy of the entry's crystal and filter taken when it opened, and in
// Immediate mode it writes them back into the pool every frame; the analysis window writes the same
// filter slot when Exclude is clicked, and main.cpp orders the two so the analysis window goes first
// (gui_test's frame loop keeps that order). So what this drives is the one write the editor must
// pull in rather than overwrite: the pool must hold the excluded row afterwards, the entry must
// still be bound to it, and the editor's own row list must show it — the user is looking at both.
//
// `start_with_out_filter` seeds the entry with a one-row Out filter first: with no filter the
// exclusion is a NEW slot the entry gets bound to; with one it is a row APPENDED to the slot the
// editor already holds. The editor loses each in its own way (unbinding the new slot, or restoring
// the old row set over the appended one), so both are driven.
bool ExcludeWhileTheImmediateEditorIsOpen(ImGuiTestContext* ctx, bool start_with_out_filter) {
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunPointAnalysisToCompletion(ctx), false);
  const auto& view_result = gui::g_state.analysis_result;
  IM_CHECK_RETV(!view_result.display_order.empty(), false);
  const LUMICE_RaypathHistogramEntry& top =
      view_result.payload->entries[static_cast<size_t>(view_result.display_order[0])];
  IM_CHECK_RETV(top.chain_len == 1, false);  // the single-layer scene; Exclude accepts only these
  const std::string top_display = top.display;
  const std::string top_row_text = gui::FormatSegmentRaypathText(top.chain[0]);

  std::vector<std::string> expected_rows;
  if (start_with_out_filter) {
    // Any face path but the one about to be excluded, so the append is a real second row.
    const std::string seed = (top_row_text == "1-3") ? "3-5" : "1-3";
    gui::FilterConfig out;
    out.name = "drop one";
    out.action = 1;  // filter_out
    out.param = gui::FromLegacyRaypath(gui::RaypathParams{ seed });
    gui::SetFilter(gui::g_state, gui::g_state.layers[0].entries[0], out);
    expected_rows.push_back(seed);
    ctx->Yield(2);
  }
  expected_rows.push_back(top_row_text);
  IM_CHECK_RETV(gui::g_state.filters.size() == (start_with_out_filter ? 1u : 0u), false);

  const ScopedPopups popup_guard(ctx);
  gui::g_state.modal_immediate_mode = true;
  const gui::EditRequest req{ gui::EditTarget::kFilter, 0, 0 };
  gui::OpenEditModal(req, gui::g_state);
  ctx->Yield(4);
  IM_CHECK_RETV(gui::IsEditModalOpen(), false);
  // Out of the analysis window's way, so the row and the button below are the items under the
  // mouse rather than the editor's title bar.
  ctx->WindowMove("//Edit Entry", ImVec2(760.0f, 40.0f));
  ctx->Yield(2);

  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + top_display).c_str());
  ctx->Yield(1);
  IM_CHECK_RETV(gui::g_state.analysis.selected_entry.has_value(), false);
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo(ICON_FA_BAN " Exclude this raypath")), false);
  ctx->ItemClick(ICON_FA_BAN " Exclude this raypath");
  ctx->SetRef("");
  // Two frames: the click's frame already ran the editor's pull and push once after the write;
  // one more shows whatever it left behind is stable rather than mid-flight.
  ctx->Yield(2);

  // The pool, as the next Run will read it.
  const auto& entry = gui::g_state.layers[0].entries[0];
  ctx->LogInfo("after Exclude: filters=%d entry.filter_id=%d", static_cast<int>(gui::g_state.filters.size()),
               entry.filter_id.has_value() ? *entry.filter_id : -1);
  IM_CHECK_RETV(entry.filter_id.has_value(), false);
  IM_CHECK_RETV(gui::g_state.filters.size() == 1u, false);  // bound to the one slot, no orphan
  const gui::FilterConfig& bound = gui::g_state.filters[static_cast<size_t>(*entry.filter_id)];
  IM_CHECK_RETV(bound.action == 1, false);
  {
    std::string got;
    for (const auto& row : bound.param) {
      got += "[" + row.text + "]";
    }
    ctx->LogInfo("pool rows: %s", got.c_str());
  }
  IM_CHECK_RETV(bound.param.size() == expected_rows.size(), false);
  for (const std::string& text : expected_rows) {
    const bool present =
        std::any_of(bound.param.begin(), bound.param.end(), [&](const gui::SummandText& r) { return r.text == text; });
    if (!present) {
      IM_ERRORF("pool filter lost row \"%s\"", text.c_str());
      return false;
    }
  }

  // The editor, as the user sees it: the same rows, in its own row list.
  const gui::EditModalBuffers buffers = gui::GetEditModalBuffers();
  {
    std::string got;
    for (const auto& row : buffers.filter_rows) {
      got += "[" + row + "]";
    }
    ctx->LogInfo("editor rows: %s", got.c_str());
  }
  IM_CHECK_RETV(buffers.filter_top.action == 1, false);
  IM_CHECK_RETV(buffers.filter_rows.size() == expected_rows.size(), false);
  for (const std::string& text : expected_rows) {
    const bool present =
        std::find(buffers.filter_rows.begin(), buffers.filter_rows.end(), text) != buffers.filter_rows.end();
    if (!present) {
      IM_ERRORF("editor row list lost row \"%s\"", text.c_str());
      return false;
    }
  }

  ctx->SetRef("//Edit Entry");
  ctx->ItemClick("Close##edit_modal");
  ctx->Yield(2);
  gui::g_state.modal_immediate_mode = false;
  return true;
}

// The halo document on the panels with no server behind it, and a hand-built whole-sky result
// on show as RefreshAnalysisEntries leaves one — the fixture for the excluded-row cases, whose
// subject is the list's derivation from the document's filters, not a run. `chains` are
// single-segment chains of the one crystal (scene id 0), with their energies.
bool ShowHaloDocumentWithResult(ImGuiTestContext* ctx, unsigned long long generation,
                                const std::vector<std::pair<const char*, double>>& chains) {
  auto payload = std::make_shared<gui::AnalysisPayload>();
  payload->snapshot_generation = generation;
  payload->roi_mode = LUMICE_RAYPATH_ROI_FULL_SKY;
  for (const auto& [display, energy] : chains) {
    LUMICE_RaypathHistogramEntry e{};
    e.chain_len = 1;
    e.chain[0].crystal_id = 0;
    e.chain[0].segment_len = 2;
    e.chain[0].segment[0] = display[0] - '0';
    e.chain[0].segment[1] = display[2] - '0';
    snprintf(e.display, sizeof(e.display), "%s", display);
    e.energy = energy;
    e.count = 1000;
    payload->entries.push_back(e);
  }
  IM_CHECK_RETV(gui::AdoptAnalysisPayloadIfNew(gui::g_state, payload), false);
  gui::g_state.analysis_result.entries_symmetry = gui::AnalysisSymmetryBits(gui::g_state);
  gui::g_state.analysis.fetched_once = true;
  gui::g_state.analysis.fetched_generation = generation;
  gui::g_state.analysis.fetched_symmetry = gui::AnalysisSymmetryBits(gui::g_state);
  ctx->Yield(2);
  return true;
}

// The path of the greyed row for `display`, and of its Include again button — the row is a
// selectable labelled ExcludedRowLabel under a PushID of the display text, the button a child of
// that id, so "**/<display>/<button>" resolves to it and to no other row's. The label is built
// the way RenderResultList builds it: rewritten under the live document's map.
std::string ExcludedRowPath(const std::string& display) {
  std::map<int, int> core_to_pool;
  for (const auto& [pool, core] : gui::ComputeCrystalPoolToCoreIdMap(gui::g_state)) {
    core_to_pool.emplace(core, pool);
  }
  return gui::ExcludedRowLabel(display, core_to_pool);
}

std::string IncludeAgainPath(const std::string& display) {
  return display + "/" + gui::kIncludeAgainButtonLabel;
}

// Exclude a chain, put a result on show that no longer has it, and read the greyed row off the
// screen: it is there with the share the chain had, the search box and the right-click copy
// reach it like any row, it is not a selection, and its Include again button erases the row
// from the filter — here the filter's only row, so the entry loses the filter — and takes the
// greyed row with it.
bool ExcludedRowShowsAndIncludeAgainTakesItBack(ImGuiTestContext* ctx) {
  ResetTestState();
  IM_CHECK_RETV(gui::DeserializeFromJson(kHalo22Json, gui::g_state), false);
  // Opened AFTER the deserialize, which replaces the whole GuiState — the workspace mode
  // included — so a workspace entered before it would be bounced back to Render and the list
  // would never draw.
  OpenWindow(ctx);
  IM_CHECK_RETV(gui::g_state.filters.empty(), false);
  IM_CHECK_RETV(ShowHaloDocumentWithResult(ctx, 1, { { "3-5", 3.0 }, { "1-3", 1.0 } }), false);
  ctx->WindowMove(kWindowRef, ImVec2(60, 60));
  ctx->Yield(1);
  SetListRef(ctx);
  IM_CHECK_RETV(ctx->ItemInfo((std::string("**/") + ExcludedRowPath("3-5")).c_str(), ImGuiTestOpFlags_NoError).ID == 0,
                false);

  // Exclude 3-5: the filter is written, the memory holds the 75% the row showed.
  ctx->ItemClick("**/3-5");
  ctx->Yield(1);
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo(ICON_FA_BAN " Exclude this raypath")), false);
  ctx->ItemClick(ICON_FA_BAN " Exclude this raypath");
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.filters.size() == 1u, false);
  IM_CHECK_RETV(gui::g_state.filters[0].param.size() == 1u, false);
  // Still a result row: shown as one, not greyed.
  IM_CHECK_RETV(ctx->ItemInfo("**/3-5").ID != 0, false);
  IM_CHECK_RETV(ctx->ItemInfo((std::string("**/") + ExcludedRowPath("3-5")).c_str(), ImGuiTestOpFlags_NoError).ID == 0,
                false);

  // The next result has no 3-5 (the filter took): the greyed row appears, under the result row
  // 1-3 and with the remembered share.
  ctx->SetRef("");
  IM_CHECK_RETV(ShowHaloDocumentWithResult(ctx, 2, { { "1-3", 1.0 } }), false);
  SetListRef(ctx);
  IM_CHECK_RETV(ctx->ItemInfo("**/3-5", ImGuiTestOpFlags_NoError).ID == 0, false);
  const ImGuiTestItemInfo greyed = ctx->ItemInfo((std::string("**/") + ExcludedRowPath("3-5")).c_str());
  IM_CHECK_RETV(greyed.ID != 0, false);
  const ImGuiTestItemInfo kept = ctx->ItemInfo("**/1-3");
  IM_CHECK_RETV(kept.ID != 0, false);
  IM_CHECK_RETV(greyed.RectFull.Min.y > kept.RectFull.Min.y, false);  // after the result rows
  const std::vector<gui::ExcludedRaypathRow> rows = gui::ComputeExcludedRaypathRows(gui::g_state);
  IM_CHECK_RETV(rows.size() == 1u, false);
  IM_CHECK_RETV(rows[0].was_pct.has_value(), false);
  IM_CHECK_RETV(std::fabs(*rows[0].was_pct - 75.0) < 1e-9, false);
  IM_CHECK_RETV(std::strcmp(gui::ExcludedRowWasText(rows[0].was_pct).c_str(), "was 75.00%") == 0, false);
  // A click on it selects nothing: the Exclude button stays shut.
  ctx->ItemClick((std::string("**/") + ExcludedRowPath("3-5")).c_str());
  ctx->Yield(1);
  IM_CHECK_RETV(!gui::g_state.analysis.selected_entry.has_value(), false);
  IM_CHECK_RETV(IsDisabled(ctx->ItemInfo(ICON_FA_BAN " Exclude this raypath")), false);

  // The search box reaches it: "1-3" hides it, "3-5" shows it alone, empty brings all back.
  SetListRef(ctx);
  ctx->ItemInputValue("###analysis_search", "1-3");
  ctx->Yield(3);
  IM_CHECK_RETV(ctx->ItemInfo((std::string("**/") + ExcludedRowPath("3-5")).c_str(), ImGuiTestOpFlags_NoError).ID == 0,
                false);
  IM_CHECK_RETV(ctx->ItemInfo("**/1-3").ID != 0, false);
  ctx->ItemInputValue("###analysis_search", "3-5");
  ctx->Yield(3);
  IM_CHECK_RETV(ctx->ItemInfo((std::string("**/") + ExcludedRowPath("3-5")).c_str()).ID != 0, false);
  IM_CHECK_RETV(ctx->ItemInfo("1-3", ImGuiTestOpFlags_NoError).ID == 0, false);
  ctx->ItemInputValue("###analysis_search", "");
  ctx->Yield(3);

  // The right-click menu reaches it: the raw text, and the row in its three-field form.
  auto choose = [&](const std::string& path, const char* item) {
    ctx->ItemClick((std::string("**/") + path.c_str()).c_str(), ImGuiMouseButton_Right);
    ctx->Yield(2);
    ctx->SetRef("//$FOCUSED");
    ctx->ItemClick((std::string("**/") + item).c_str());
    ctx->Yield(2);
    SetListRef(ctx);
  };
  choose(ExcludedRowPath("3-5"), "Copy raypath");
  IM_CHECK_RETV(std::strcmp(ImGui::GetClipboardText(), "3-5") == 0, false);
  choose(ExcludedRowPath("3-5"), "Copy row");
  IM_CHECK_RETV(std::strcmp(ImGui::GetClipboardText(), gui::ExcludedRowCopyText(rows[0]).c_str()) == 0, false);
  IM_CHECK_RETV(std::strcmp(ImGui::GetClipboardText(), "3-5,excluded,75.0000") == 0, false);

  // For a human's eyes: the greyed row under the result row, the button in its last column.
  ctx->SetRef("");
  ctx->MouseMoveToPos(ImVec2(2.0f, 2.0f));
  ctx->Yield(2);
  IM_CHECK_RETV(SaveWindowPng(ctx, kWindowRef, GuiTestTempPath("analysis_excluded_row.png").string()), false);

  // Include again: the filter's only row goes, so the entry drops the filter, the memory is
  // forgotten, and the greyed row is gone the next frame. 1-3 is untouched.
  SetListRef(ctx);
  IM_CHECK_RETV(ctx->ItemInfo((std::string("**/") + IncludeAgainPath("3-5")).c_str()).ID != 0, false);
  ctx->ItemClick((std::string("**/") + IncludeAgainPath("3-5")).c_str());
  ctx->Yield(2);
  IM_CHECK_RETV(!gui::g_state.layers[0].entries[0].filter_id.has_value(), false);
  IM_CHECK_RETV(gui::g_state.analysis.excluded_memory.empty(), false);
  IM_CHECK_RETV(ctx->ItemInfo((std::string("**/") + ExcludedRowPath("3-5")).c_str(), ImGuiTestOpFlags_NoError).ID == 0,
                false);
  IM_CHECK_RETV(ctx->ItemInfo("**/1-3").ID != 0, false);
  IM_CHECK_RETV(gui::ComputeExcludedRaypathRows(gui::g_state).empty(), false);
  ctx->SetRef("");
  return true;
}

// Include again while the Immediate-mode editor is open on the entry, Filter tab showing, on a
// filter of TWO rows so the filter itself stays — the branch the editor's pull is argued on
// (edit_modals.cpp PullSummandRows: theirs lost a row the buffer had not touched, so the buffer
// drops it too). The editor pushes its buffers into the pool every frame; if it did not pull the
// removal first it would write the row straight back. So: the pool holds one row afterwards,
// the entry is still bound to the slot, and the editor's own row list shows one row — the user
// is looking at both. The mirror of ExcludeWhileTheImmediateEditorIsOpen, frame for frame.
bool IncludeAgainWhileTheImmediateEditorIsOpen(ImGuiTestContext* ctx) {
  ResetTestState();
  IM_CHECK_RETV(gui::DeserializeFromJson(kHalo22Json, gui::g_state), false);
  // After the deserialize, which resets the whole GuiState (workspace mode included).
  OpenWindow(ctx);
  gui::FilterConfig out;
  out.name = "drop two";
  out.action = 1;  // filter_out
  out.param = gui::FromLegacyRaypath(gui::RaypathParams{ "3-5" });
  out.param.push_back(gui::FromLegacyRaypath(gui::RaypathParams{ "1-3" }).front());
  gui::SetFilter(gui::g_state, gui::g_state.layers[0].entries[0], out);
  IM_CHECK_RETV(gui::g_state.filters.size() == 1u, false);
  // A result without either chain: both are greyed rows.
  IM_CHECK_RETV(ShowHaloDocumentWithResult(ctx, 1, { { "2-4", 1.0 } }), false);
  ctx->WindowMove(kWindowRef, ImVec2(60, 60));
  ctx->Yield(1);
  SetListRef(ctx);
  IM_CHECK_RETV(ctx->ItemInfo((std::string("**/") + ExcludedRowPath("3-5")).c_str()).ID != 0, false);
  IM_CHECK_RETV(ctx->ItemInfo((std::string("**/") + ExcludedRowPath("1-3")).c_str()).ID != 0, false);
  ctx->SetRef("");

  const ScopedPopups popup_guard(ctx);
  gui::g_state.modal_immediate_mode = true;
  const gui::EditRequest req{ gui::EditTarget::kFilter, 0, 0 };
  gui::OpenEditModal(req, gui::g_state);
  ctx->Yield(4);
  IM_CHECK_RETV(gui::IsEditModalOpen(), false);
  // Out of the analysis window's way, so the button below is the item under the mouse rather
  // than the editor's title bar.
  ctx->WindowMove("//Edit Entry", ImVec2(760.0f, 40.0f));
  ctx->Yield(2);
  {
    const gui::EditModalBuffers before = gui::GetEditModalBuffers();
    IM_CHECK_RETV(before.filter_rows.size() == 2u, false);
  }

  ctx->ItemClick((std::string("**/") + IncludeAgainPath("3-5").c_str()).c_str());
  ctx->SetRef("");
  // Two frames: the click's frame already ran the editor's pull and push once after the write;
  // one more shows whatever it left behind is stable rather than mid-flight.
  ctx->Yield(2);

  // The pool, as the next Run will read it: one row, the entry still bound.
  const auto& entry = gui::g_state.layers[0].entries[0];
  ctx->LogInfo("after Include again: filters=%d entry.filter_id=%d", static_cast<int>(gui::g_state.filters.size()),
               entry.filter_id.has_value() ? *entry.filter_id : -1);
  IM_CHECK_RETV(entry.filter_id.has_value(), false);
  IM_CHECK_RETV(gui::g_state.filters.size() == 1u, false);
  const gui::FilterConfig& bound = gui::g_state.filters[static_cast<size_t>(*entry.filter_id)];
  IM_CHECK_RETV(bound.action == 1, false);
  IM_CHECK_RETV(bound.param.size() == 1u, false);
  IM_CHECK_RETV(std::strcmp(bound.param[0].text.c_str(), "1-3") == 0, false);

  // The editor, as the user sees it: the one row left, in its own row list.
  const gui::EditModalBuffers buffers = gui::GetEditModalBuffers();
  {
    std::string got;
    for (const auto& row : buffers.filter_rows) {
      got += "[" + row + "]";
    }
    ctx->LogInfo("editor rows: %s", got.c_str());
  }
  IM_CHECK_RETV(buffers.filter_rows.size() == 1u, false);
  IM_CHECK_RETV(std::strcmp(buffers.filter_rows[0].c_str(), "1-3") == 0, false);

  // And the list: 3-5 is no longer greyed, 1-3 still is.
  SetListRef(ctx);
  IM_CHECK_RETV(ctx->ItemInfo((std::string("**/") + ExcludedRowPath("3-5")).c_str(), ImGuiTestOpFlags_NoError).ID == 0,
                false);
  IM_CHECK_RETV(ctx->ItemInfo((std::string("**/") + ExcludedRowPath("1-3")).c_str()).ID != 0, false);
  ctx->SetRef("");

  ctx->SetRef("//Edit Entry");
  ctx->ItemClick("Close##edit_modal");
  ctx->Yield(2);
  gui::g_state.modal_immediate_mode = false;
  return true;
}

// The owner's chain on the C01 scene (the 22-degree halo fixture this suite shares with the
// e2e layer): Analyze, pick a row, Exclude it, and Run again — driven through real frames and
// real clicks, with the two captures a human reads: the list holding the selection, the
// exclusion, and the rerun's sky on the panorama. The rerun commits the document WITH the
// filter, so the picture after it is the excluded sky.
bool C01ChainAnalyzeSelectExcludeRerun(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  // Select the top row of the list, and keep the evidence of the selection on file.
  const std::string top_display = TopChainDisplay();
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + top_display).c_str());
  ctx->Yield(1);
  IM_CHECK_RETV(gui::g_state.analysis.selected_entry.has_value(), false);
  IM_CHECK_RETV(*gui::g_state.analysis.selected_entry == top_display, false);
  ctx->LogInfo("[c01 chain] analyzed %zu rows; selected top row '%s'",
               gui::g_state.analysis_result.payload->entries.size(), top_display.c_str());
  ctx->SetRef("");
  ctx->MouseMoveToPos(ImVec2(2.0f, 2.0f));
  ctx->Yield(2);
  IM_CHECK_RETV(SaveWindowPng(ctx, kWindowRef, GuiTestTempPath("analysis_c01_chain_list.png").string()), false);
  // The right column's selection card (the identity facts the row names), on file too.
  IM_CHECK_RETV(SaveWindowPng(ctx, "//##AnalyzeRightPanel", GuiTestTempPath("analysis_c01_chain_details.png").string()),
                false);

  // Exclude it: one Out filter on the document, bound to the entry.
  SetListRef(ctx);
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo(ICON_FA_BAN " Exclude this raypath")), false);
  ctx->MouseMove(ICON_FA_BAN " Exclude this raypath");
  ctx->Yield(2);
  ctx->ItemClick(ICON_FA_BAN " Exclude this raypath");
  ctx->SetRef("");
  ctx->Yield(1);
  IM_CHECK_RETV(gui::g_state.filters.size() == 1u, false);
  IM_CHECK_RETV(gui::g_state.filters[0].action == 1, false);
  IM_CHECK_RETV(gui::g_state.layers[0].entries[0].filter_id.has_value(), false);
  ctx->LogInfo("[c01 chain] excluded '%s' (filters=%d)", top_display.c_str(),
               static_cast<int>(gui::g_state.filters.size()));

  // Run again, from the top bar, on a finite budget: it renders the excluded sky.
  gui::g_state.sim.infinite = false;
  gui::g_state.sim.ray_num_millions = 0.1f;
  const unsigned long long uploads_before = gui::g_state.texture_upload_count;
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo("##TopBar/" ICON_FA_PLAY " Run")), false);
  ctx->ItemClick("##TopBar/" ICON_FA_PLAY " Run");
  IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kSimulating; }, 10), false);
  // A dirty document's finished run reads kModified; the real loop's commit tick clears it —
  // cleared by hand here, the moment the app would (RunAfterAnalysisRenders's reasoning).
  gui::g_state.dirty = false;
  IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kDone; }, 60), false);
  IM_CHECK_RETV(
      DriveUntil(ctx, [uploads_before] { return gui::g_state.texture_upload_count > uploads_before; }, 10), false);
  ctx->LogInfo("[c01 chain] rerendered after exclusion: uploads %llu -> %llu", uploads_before,
               gui::g_state.texture_upload_count);
  ctx->MouseMoveToPos(ImVec2(2.0f, 2.0f));
  ctx->Yield(2);
  IM_CHECK_RETV(SaveWindowPng(ctx, PanoramaRegionRef(), GuiTestTempPath("analysis_c01_chain_rerender.png").string()),
                false);
  return true;
}

// ---- M3's survey-query evidence cases (742.3 M3-5) --------------------------------------------------

// The completed digest on the halo fixture: a selection issues the background query, the
// worker's answer is adopted under its key, and the typed document carries the class's real
// facts — read back here through the same readers the region draws with, so the screenshot a
// human reads and the assertions below cannot both be mock. The re-Analyze half pins R6's
// generation gate: the bookkeeping dies with the selection, the next generation re-arms.
bool SurveyDigestAnswersWithRealClassFacts(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  const std::string top_display = TopChainDisplay();
  const unsigned long long generation = gui::g_state.analysis_result.payload->snapshot_generation;
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + top_display).c_str());
  ctx->SetRef("");
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    30),
                false);
  const gui::SurveyQuerySession& survey = gui::g_state.analysis.survey;
  IM_CHECK_RETV(survey.key.generation == generation, false);
  IM_CHECK_RETV(survey.cancelled_for_key == false, false);
  LUMICE_PathSurveyInfo info{};
  info.struct_size = sizeof(info);
  IM_CHECK_RETV(LUMICE_PathSurveyGetInfo(survey.adopted_result.get(), &info) == LUMICE_PATH_READ_OK, false);
  IM_CHECK_RETV(
      info.execution == LUMICE_PATH_OUTCOME_COMPLETED || info.execution == LUMICE_PATH_OUTCOME_DEADLINE_EXCEEDED,
      false);
  // The engine echoes the request's crystal back — the adopted document answers THIS key. (The
  // core id here is 0: the fixture's pool crystal maps to core id 0, and 0 is a valid echo.)
  IM_CHECK_RETV(static_cast<long long>(info.crystal_id) == survey.key.crystal_id, false);
  ctx->LogInfo("[survey digest] generation %llu, echo crystal %lld, members %zu, objects %zu, budget %d ms", generation,
               static_cast<long long>(info.crystal_id), info.member_count, info.object_count, info.budget_ms);
  // 742.4: the same document read into the overview model the region now draws. The halo
  // fixture's zenith axis is a uniform centred at 90° — the one spelling the survey's routing v1
  // domain does NOT read as the full-sphere random family — so its dominant class is honestly
  // REFUSED (measure_shape_outside_routing_domain), and the overview's refusal arm is what the
  // region shows: the named refusal, the witness's positive event, and the chromatic layer's
  // engine-worded absence. (The dispersion-on-screen face is the kFullyRandomJson case below.)
  {
    const gui::SurveyOverviewModel model = gui::BuildSurveyOverviewModel(*survey.adopted_result);
    IM_CHECK_RETV(model.valid, false);
    IM_CHECK_RETV(model.arm == gui::SurveyArm::kRefused, false);
    IM_CHECK_RETV(model.evidence_state == LUMICE_PATH_STATE_REFUSED, false);
    IM_CHECK_RETV(model.evidence_mode.find("measure_shape_outside_routing_domain") != std::string::npos, false);
    IM_CHECK_RETV(model.witness.available && model.witness.value_available, false);
    IM_CHECK_RETV(model.witness.kept_count > 0, false);
    IM_CHECK_RETV(!model.dispersion.available && !model.diagnose.available, false);
    IM_CHECK_RETV(model.chromatic_note.find("routing v1 domain") != std::string::npos, false);
    IM_CHECK_RETV(!model.conclusions.empty(), false);
    ctx->LogInfo("[survey digest] overview model: refused arm, mode '%s', witness kept %lld/%lld, conclusions %d",
                 model.evidence_mode.c_str(), static_cast<long long>(model.witness.kept_count),
                 static_cast<long long>(model.witness.sample_count), static_cast<int>(model.conclusions.size()));
  }
  IM_CHECK_RETV(SaveWindowPng(ctx, "//##AnalyzeRightPanel", GuiTestTempPath("analysis_survey_digest.png").string()),
                false);

  // Re-Analyze: the selection clears, the survey bookkeeping dies with it (R6 — no adopted
  // outcome survives into the next generation's region), and a straggler of the old generation
  // would find no intent to land under.
  SetListRef(ctx);
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo(kAnalyzeButton)), false);
  ctx->ItemClick(kAnalyzeButton);
  ctx->SetRef("");
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analysis.survey.intent_valid == false, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_error.empty(), false);
  ctx->LogInfo("[survey digest] re-analyze voided the old generation's intent and outcome");

  // The next generation answers again, under the new generation's id.
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::g_state.analysis_result.payload != nullptr &&
                             gui::g_state.sim_state != SimState::kSimulating;
                    },
                    60),
                false);
  const std::string next_display = TopChainDisplay();
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + next_display).c_str());
  ctx->SetRef("");
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    30),
                false);
  IM_CHECK_RETV(
      gui::g_state.analysis.survey.key.generation == gui::g_state.analysis_result.payload->snapshot_generation, false);
  ctx->LogInfo("[survey digest] re-armed under generation %llu", gui::g_state.analysis.survey.key.generation);
  return true;
}


// The map region's face (742.5, AC1/AC3/AC6): on the fully random prism, the selected class's
// reference map arrives through the real chain — share list selection -> survey and map queued
// on the one worker -> the full-sky map -> the auto-Fit -> the refinement window — and the
// region takes a real click as the sky point while the panorama's ROI state stands untouched.
// The assertions read the adopted result the picture is drawn from, so the screenshot a human
// reads and the checks below cannot both be mock.
bool MapRegionShowsTheClassMapAndTakesThePick(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  // The no-data state before any selection: the region asks for a class and keeps its pick
  // input available (the honest empty state, R7).
  IM_CHECK_RETV(gui::MapQueryStateFor(gui::g_state.analysis.map) == gui::MapQueryState::kNoSelection, false);

  const std::string top_display = TopChainDisplay();
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + top_display).c_str());
  ctx->SetRef("");
  // The kWaiting pin: the multi-line queue note is exactly the
  // fixed text stack the region must carry without scrolling — the waiting-text arm the
  // skeleton scroll-pin case cannot reach honestly without a server.
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::MapQueryStateFor(gui::g_state.analysis.map) == gui::MapQueryState::kWaiting; }, 10),
      false);
  {
    ImGuiWindow* region = LowerRowRegionWindow("##AnalyzeRaypathView");
    IM_CHECK_RETV(region != nullptr, false);
    IM_CHECK_RETV(region->ScrollMax.y == 0.0f, false);
    IM_CHECK_RETV(region->Scroll.y == 0.0f, false);
  }
  // The map answers behind the survey's chromatic legs (one worker, queue by face), then the
  // default view chain runs: the full-sky map auto-Fits and the refinement window answers —
  // the test waits for the WINDOW map specifically.
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      if (m.adopted_result == nullptr || !(m.adopted_id == m.intent_id)) {
                        return false;
                      }
                      LUMICE_PathMapInfo i{};
                      i.struct_size = sizeof(i);
                      return LUMICE_PathMapGetInfo(m.adopted_result.get(), &i) == LUMICE_PATH_READ_OK &&
                             i.view_full_sky == 0;
                    },
                    60),
                false);
  const gui::MapQuerySession& map = gui::g_state.analysis.map;
  LUMICE_PathMapInfo info{};
  info.struct_size = sizeof(info);
  IM_CHECK_RETV(LUMICE_PathMapGetInfo(map.adopted_result.get(), &info) == LUMICE_PATH_READ_OK, false);
  const std::optional<gui::SelectedClassIdentity> identity = gui::SelectedClassIdentityFor(gui::g_state);
  IM_CHECK_RETV(identity.has_value(), false);
  IM_CHECK_RETV(info.crystal_id == identity->core_crystal_id, false);
  IM_CHECK_RETV(info.kept_count > 0, false);  // the picture carries real events, not an empty echo
  ctx->LogInfo("[map view] window delta[%.1f,%.1f] az[%.1f,%.1f] %dx%d, members %d/%d, lambda %.1f, kept %lld",
               info.view_delta_min_deg, info.view_delta_max_deg, info.view_az_min_deg, info.view_az_max_deg,
               info.view_rows, info.view_cols, info.members_evaluated, info.member_count, info.wavelength_nm,
               info.kept_count);
  IM_CHECK_RETV(SaveWindowPng(ctx, "//##AnalyzeLowerRow", GuiTestTempPath("analyze_map_region.png").string()), false);

  // The pick on the real mouse path: a click on the canvas centre sets the sky point, and the
  // panorama's ROI state and the share list's selection stay exactly what they were (AC3 — the
  // two click semantics never share a write).
  const int roi_mode_before = gui::g_state.analysis.roi_mode;
  const bool roi_valid_before = gui::g_state.analysis.cone_center_valid;
  const float roi_dir_before[3] = { gui::g_state.analysis.cone_center_dir[0], gui::g_state.analysis.cone_center_dir[1],
                                    gui::g_state.analysis.cone_center_dir[2] };
  const bool pick_armed_before = gui::g_state.analysis.pick_armed;
  const std::string selected_before = gui::g_state.analysis.selected_entry.value_or("");
  ctx->ItemClick("**/##map_canvas");
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid, false);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.from_map_pick, false);
  IM_CHECK_RETV(gui::g_state.analysis.roi_mode == roi_mode_before, false);
  IM_CHECK_RETV(gui::g_state.analysis.cone_center_valid == roi_valid_before, false);
  IM_CHECK_RETV(gui::g_state.analysis.cone_center_dir[0] == roi_dir_before[0], false);
  IM_CHECK_RETV(gui::g_state.analysis.cone_center_dir[1] == roi_dir_before[1], false);
  IM_CHECK_RETV(gui::g_state.analysis.cone_center_dir[2] == roi_dir_before[2], false);
  IM_CHECK_RETV(gui::g_state.analysis.pick_armed == pick_armed_before, false);
  IM_CHECK_RETV(gui::g_state.analysis.selected_entry.value_or("") == selected_before, false);

  // The with-result pin: the caliber block is on screen and the
  // pick has set the sky point, so the point input is at its two-row shape — the region must
  // budget it exactly and never scroll (the reserve defect pinned the scroll to the bottom and
  // cut the header row off).
  {
    ImGuiWindow* region = LowerRowRegionWindow("##AnalyzeRaypathView");
    IM_CHECK_RETV(region != nullptr, false);
    ctx->LogInfo("[scroll pins] map with-result SM %.1f SC %.1f", region->ScrollMax.y, region->Scroll.y);
    IM_CHECK_RETV(region->ScrollMax.y == 0.0f, false);
    IM_CHECK_RETV(region->Scroll.y == 0.0f, false);
  }

  // The flip falsifier: a full-sky view puts the sun far off the disk's centre, so the halo
  // ring surrounds the sun MARKER only when the raster's row order and the anchors agree — the
  // one view where a vertical mirror of the picture is visible to the eye. Screenshot on file.
  // (The view arrives by the same write the Full sky button performs — the gesture click paths
  // are already covered by the pick above; the test engine's hover on this chrome layout is
  // addressed separately.)
  for (int k = 0; k < 3; ++k) {
    gui::g_state.analysis.map_view.centre_pos[k] = k == 2 ? 1.0f : 0.0f;
  }
  gui::g_state.analysis.map_view.radius_deg = gui::kMapFullSkyRadiusDeg;
  gui::g_state.analysis.map_view.user_moved = true;
  gui::MapViewChanged(&gui::g_state.analysis.map_view, ImGui::GetTime(), false, /*user_steered=*/true);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      if (m.adopted_result == nullptr || !(m.adopted_id == m.intent_id)) {
                        return false;
                      }
                      LUMICE_PathMapInfo i{};
                      i.struct_size = sizeof(i);
                      return LUMICE_PathMapGetInfo(m.adopted_result.get(), &i) == LUMICE_PATH_READ_OK &&
                             i.view_full_sky == 1;
                    },
                    30),
                false);
  ctx->Yield(2);
  IM_CHECK_RETV(SaveWindowPng(ctx, "//##AnalyzeLowerRow", GuiTestTempPath("analyze_map_region_full_sky.png").string()),
                false);
  return true;
}

// The scroll pins: the Analyze workspace's rolling regions are
// STRUCTURALLY non-scrolling — the map and 3D regions carry their fixed stacks (header row, state
// text, the two-row sky-point input) without a scrollbar, the divider strip contains its grab bar
// inside its own rect, and the share list never scrolls as a window. Asserted at the 1600x980
// default basis, the 1280x800 acceptance size and the 1024x640 minimum, against the worst
// fixed-content shapes reachable without a server: the regions' no-selection text stacks and the
// sky point set, which is what makes the input block two rows. The waiting-text arm (the map's
// multi-line queue note) rides the real-timing map leg below, where an intent is honest — set
// here without a server it would sit one pump frame from kRefused, and a state the app itself
// races away from is flake fuel, not a fixture.
bool WorkspaceScrollRegionsAreStructurallyFixed(ImGuiTestContext* ctx) {
  ResetTestState();
  OpenWindow(ctx);
  // The two-row input block: a set sky point draws the "sky point: ..." line and the Clear
  // button under the alt/az row — the height the map region must budget exactly.
  SetSkyPointTarget(42.0, 0.0);
  ctx->Yield(2);

  const auto request_basis = [&](int width, int height, const char* label) {
    g_window_size_test.width = width;
    g_window_size_test.height = height;
    g_window_size_test.done.store(false);
    g_window_size_test.requested.store(true);
    for (int i = 0; i < 20 && !g_window_size_test.done.load(); ++i) {
      ctx->Yield(1);
    }
    IM_CHECK(g_window_size_test.done.load());
    ctx->Yield(3);
    ctx->LogInfo("[scroll pins] basis %s", label);
  };
  const auto assert_regions_fixed = [&]() {
    ImGuiWindow* map = LowerRowRegionWindow("##AnalyzeRaypathView");
    ImGuiWindow* r3d = LowerRowRegionWindow("##AnalyzeRaypath3D");
    ImGuiWindow* divider = ctx->GetWindowByRef("//##AnalyzeRowsDivider");
    ImGuiWindow* share = ctx->GetWindowByRef(kWindowRef);
    ImGuiWindow* lower = ctx->GetWindowByRef("//##AnalyzeLowerRow");
    if (map != nullptr && r3d != nullptr && divider != nullptr && share != nullptr && lower != nullptr) {
      // The values first: a red assert prints its expression, this line pins the numbers. The
      // LOWER ROW's own scroll rides along: NoScrollbar only hides the bar, so a nonzero scroll
      // there would shift both children up while every child-side reading stays zero.
      ctx->LogInfo(
          "[scroll pins] map SM %.1f SC %.1f | 3d SM %.1f SC %.1f | divider SM %.1f/%.1f | share SM %.1f SC %.1f | "
          "lower SM %.1f SC %.1f",
          map->ScrollMax.y, map->Scroll.y, r3d->ScrollMax.y, r3d->Scroll.y, divider->ScrollMax.x, divider->ScrollMax.y,
          share->ScrollMax.y, share->Scroll.y, lower->ScrollMax.y, lower->Scroll.y);
      const ImGuiTestItemInfo title = ctx->ItemInfo("**/Raypath view", ImGuiTestOpFlags_NoError);
      if (title.ID != 0) {
        ctx->LogInfo("[scroll pins] header title rect top %.1f, region clip top %.1f", title.RectFull.Min.y,
                     map->InnerClipRect.Min.y);
      }
    }
    // The map and 3D regions: no scroll range, no scroll position — the header row and the
    // input block are reachable by construction, at every basis.
    const auto assert_region_no_scroll = [&](const char* fragment) {
      ImGuiWindow* region = LowerRowRegionWindow(fragment);
      IM_CHECK_RETV(region != nullptr, false);
      IM_CHECK_RETV(region->ScrollMax.y == 0.0f, false);
      IM_CHECK_RETV(region->Scroll.y == 0.0f, false);
      return true;
    };
    IM_CHECK_RETV(assert_region_no_scroll("##AnalyzeRaypathView"), false);
    IM_CHECK_RETV(assert_region_no_scroll("##AnalyzeRaypath3D"), false);
    // The divider: the 8pt strip never scrolls, and its grab bar stays inside the strip's own
    // rect — the line that rode ~2pt past the strip's bottom edge was the visible defect.
    IM_CHECK_RETV(divider != nullptr, false);
    IM_CHECK_RETV(divider->ScrollMax.x == 0.0f, false);
    IM_CHECK_RETV(divider->ScrollMax.y == 0.0f, false);
    // The strip is EXACTLY the 8pt its grab bar needs (the harness runs at UI scale 1.0):
    // ImGui clamps a top-level window's SetNextWindowSize up to style.WindowMinSize (32pt with
    // the default style), and a clamped strip's opaque background would cover the first rows of
    // both regions below it while every scroll reading above stays zero.
    IM_CHECK_RETV(divider->Rect().GetHeight() <= 8.5f, false);
    const ImGuiTestItemInfo grab = ctx->ItemInfo("**/##AnalyzeRowsSplitter", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(grab.ID != 0, false);
    IM_CHECK_RETV(grab.RectFull.Min.y >= divider->Rect().Min.y - 0.5f, false);
    IM_CHECK_RETV(grab.RectFull.Max.y <= divider->Rect().Max.y + 0.5f, false);
    IM_CHECK_RETV(grab.RectFull.Min.x >= divider->Rect().Min.x - 0.5f, false);
    IM_CHECK_RETV(grab.RectFull.Max.x <= divider->Rect().Max.x + 0.5f, false);
    // The share list: the window itself never scrolls (its one scrolling region — the result
    // table's own — is pinned in the point_pick_analyze_lists_chains leg with a result in).
    IM_CHECK_RETV(share != nullptr, false);
    IM_CHECK_RETV(share->ScrollMax.y == 0.0f, false);
    IM_CHECK_RETV(share->Scroll.y == 0.0f, false);
    return true;
  };

  const ScopedWindowBasis window_basis(ctx);
  IM_CHECK_RETV(assert_regions_fixed(), false);
  request_basis(1280, 800, "1280x800");
  IM_CHECK_RETV(assert_regions_fixed(), false);
  request_basis(1024, 640, "1024x640");
  IM_CHECK_RETV(assert_regions_fixed(), false);
  return true;
}


// the component, the slider/playback, the factors and the 3D path — on the fully random prism,
// whose 22-degree halo fiber the (42, 0) target sits on (the M0 probe's verified anchor for this
// scene geometry: one closed component, ~48 samples). Real-timing pool: the point queues behind
// the selection's survey on the one worker, the same wall-clock shape as the map leg above.
bool PointInspectorShowsThePointFiberAndThePath(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  const std::string top_display = TopChainDisplay();
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + top_display).c_str());
  ctx->SetRef("");
  // No sky point yet: the point block asks for one and nothing is outstanding (the honest
  // no-intent state, in the real app's frame).
  IM_CHECK_RETV(gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kNoIntent, false);
  IM_CHECK_RETV(gui::g_state.analysis.trace.intent_valid == false, false);

  // The real pick on the map canvas: whatever sky position the refined view centres on, the pick
  // must arm the point intent (the pick-to-point chain through the real mouse path).
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      return m.adopted_result != nullptr && (m.adopted_id == m.intent_id);
                    },
                    60),
                false);
  ctx->ItemClick("**/##map_canvas");
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid, false);
  IM_CHECK_RETV(gui::PointQueryStateFor(gui::g_state.analysis.point) != gui::PointQueryState::kNoIntent, false);

  // The verified on-fiber anchor (the pick above proved the mouse path; this state write pins the
  // document the rest of the chain asserts on — the 742.5 leg's "the same write the button
  // performs" shape): (42, 0) sits on this scene's halo fiber.
  SetSkyPointTarget(42.0, 0.0);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  const gui::PointQuerySession& point = gui::g_state.analysis.point;
  LUMICE_PathPointInfo info{};
  info.struct_size = sizeof(info);
  IM_CHECK_RETV(LUMICE_PathPointGetInfo(point.adopted_result.get(), &info) == LUMICE_PATH_READ_OK, false);
  const std::optional<gui::SelectedClassIdentity> identity = gui::SelectedClassIdentityFor(gui::g_state);
  IM_CHECK_RETV(identity.has_value(), false);
  IM_CHECK_RETV(info.crystal_id == identity->core_crystal_id, false);
  IM_CHECK_RETV(info.outcome == LUMICE_PATH_POINT_DISCOVERED, false);
  IM_CHECK_RETV(info.execution == LUMICE_PATH_OUTCOME_COMPLETED, false);
  // The target echo names the anchor's own labels (the one named conversion, verified on the
  // engine's echo — not the request's copy).
  IM_CHECK_RETV(std::fabs(info.target_altitude_deg - 42.0) < 1e-3, false);
  std::vector<gui::PointComponentRow> rows;
  const int components = gui::PointComponentRows(*point.adopted_result, &rows);
  IM_CHECK_RETV(components >= 1, false);
  const int valid_count = gui::PointValidSampleCount(*point.adopted_result, 0);
  IM_CHECK_RETV(valid_count > 1, false);
  ctx->LogInfo("[point view] components %d, samples %d, target (alt %.2f, az %.2f, dev %.2f), lambda %.1f", components,
               valid_count, info.target_altitude_deg, info.target_azimuth_deg, info.target_deviation_deg,
               info.wavelength_nm);
  IM_CHECK_RETV(SaveWindowPng(ctx, "//##AnalyzeRightPanel", GuiTestTempPath("analyze_point_detail.png").string()),
                false);

  // The fiber on the globe and the trace in the 3D region follow the cursor: step the slider to
  // the second valid sample (the slider drag path is the fixed-dt leg's; this leg pins the chain)
  // and the trace re-keys — the drawn polyline's key IS the current sample (AC4).
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::RayTraceSession& t = gui::g_state.analysis.trace;
                      return t.adopted_result != nullptr && t.intent_valid &&
                             t.key.sample ==
                                 gui::PointValidSampleIndexAt(*gui::g_state.analysis.point.adopted_result, 0, 0);
                    },
                    30),
                false);
  gui::g_state.analysis.point_view.sample_index = 1;
  gui::g_state.analysis.point_view.playing = false;
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::RayTraceSession& t = gui::g_state.analysis.trace;
                      if (!t.adopted_result || !t.intent_valid) {
                        return false;
                      }
                      const int stored =
                          gui::PointValidSampleIndexAt(*gui::g_state.analysis.point.adopted_result, 0, 1);
                      return t.key.sample == stored && gui::RayTraceDisplayStateFor(t, { t.key.point_id, 0, stored }) ==
                                                           gui::RayTraceDisplayState::kDrawn;
                    },
                    30),
                false);
  IM_CHECK_RETV(SaveWindowPng(ctx, "//##AnalyzeLowerRow", GuiTestTempPath("analyze_raypath_3d.png").string()), false);

  // The 3D region pin: title, the two-line description and the
  // legend are the fixed stack; the canvas absorbs the rest. Zero scroll at the default basis.
  {
    ImGuiWindow* region = LowerRowRegionWindow("##AnalyzeRaypath3D");
    IM_CHECK_RETV(region != nullptr, false);
    ctx->LogInfo("[scroll pins] 3d default SM %.1f SC %.1f", region->ScrollMax.y, region->Scroll.y);
    IM_CHECK_RETV(region->ScrollMax.y == 0.0f, false);
    IM_CHECK_RETV(region->Scroll.y == 0.0f, false);
  }
  // The squeezed-row red anchor: at the 0.85 clamp the lower row still fits the fixed stack and
  // the legend, while the canvas's old 40pt floor does not — the canvas must YIELD (scroll stays
  // zero) instead of pushing the stack out. The squeeze is a state write (the same write the
  // drag performs), because pre-fix the grab bar rides ~6pt outside its own 8pt strip and no
  // press can reach its centre — the drag itself is verified separately below, post-restore.
  {
    const float ratio_before = gui::g_state.workspace.middle_rows_ratio;
    gui::g_state.workspace.middle_rows_ratio = 0.85f;
    ctx->Yield(3);
    {
      ImGuiWindow* region = LowerRowRegionWindow("##AnalyzeRaypath3D");
      IM_CHECK_RETV(region != nullptr, false);
      ctx->LogInfo("[scroll pins] 3d squeezed SM %.1f SC %.1f", region->ScrollMax.y, region->Scroll.y);
      IM_CHECK_RETV(region->ScrollMax.y == 0.0f, false);
      IM_CHECK_RETV(region->Scroll.y == 0.0f, false);
    }
    gui::g_state.workspace.middle_rows_ratio = ratio_before;
    ctx->Yield(3);
    // The grab gesture, over the real mouse path: press the strip's bar, drag it down past the
    // 0.85 clamp, release — the ratio must follow the mouse. (Pre-fix this cannot pass: the bar
    // extends below the strip's rect and the press hovers the lower row window instead.)
    ImGuiWindow* divider = ctx->GetWindowByRef("//##AnalyzeRowsDivider");
    IM_CHECK_RETV(divider != nullptr, false);
    const ImGuiTestItemInfo grab = ctx->ItemInfo("**/##AnalyzeRowsSplitter", ImGuiTestOpFlags_NoError);
    IM_CHECK_RETV(grab.ID != 0, false);
    const ImVec2 centre = grab.RectFull.GetCenter();
    ctx->MouseMoveToPos(centre);
    ctx->Yield(2);
    ctx->MouseDown(0);
    ctx->Yield(2);
    ctx->MouseMoveToPos(ImVec2(centre.x, centre.y + 350.0f));
    ctx->Yield(3);
    ctx->MouseUp(0);
    ctx->Yield(2);
    IM_CHECK_RETV(gui::g_state.workspace.middle_rows_ratio > ratio_before + 0.3f, false);  // the drag wrote it
    gui::g_state.workspace.middle_rows_ratio = ratio_before;
    ctx->Yield(2);
  }
  // ---- The orbit rotation convention pin (doc/geometry/coordinate-convention.md §9.8) ----
  // The project-wide grab metaphor (Render main view + modal preview + this orbit camera): drag
  // right -> the crystal's NEAR face follows the hand RIGHT; drag down -> the near face follows
  // DOWN. Every quantity below comes out of gui::OrbitBasis — the same basis the renderer draws
  // with — and the landmark M = −fwd + 0.25·right sits a quarter-right of the view centre on the
  // camera-facing side, so the assertions are about the basis itself, decoupled from the scene.
  const float yaw0 = gui::g_state.analysis.point_view.orbit_yaw_deg;
  const float pitch0 = gui::g_state.analysis.point_view.orbit_pitch_deg;
  // The pin runs at the documented default pose — the §9.8 dominance-margin derivation (cot
  // pitch / 0.25) binds to it. If the defaults ever change, fail HERE, attributably, instead of
  // letting the dominance assertion below produce an unexplainable red.
  IM_CHECK_RETV(std::fabs(yaw0 - 35.0f) < 1e-4f && std::fabs(pitch0 - 22.0f) < 1e-4f, false);
  // Restores the pre-pin pose even when one of the assertions below returns early — cross-leg
  // display state must be restored in a destructor, not at a function tail.
  struct OrbitPoseRestore {
    float yaw;
    float pitch;
    ~OrbitPoseRestore() {
      gui::g_state.analysis.point_view.orbit_yaw_deg = yaw;
      gui::g_state.analysis.point_view.orbit_pitch_deg = pitch;
    }
  } pose_restore{ yaw0, pitch0 };
  double fwd0[3];
  double right0[3];
  double up0[3];
  gui::OrbitBasis(yaw0, pitch0, fwd0, right0, up0);
  const double landmark[3] = { -fwd0[0] + 0.25 * right0[0], -fwd0[1] + 0.25 * right0[1], -fwd0[2] + 0.25 * right0[2] };
  const auto screen_motion = [](const double m[3], const double a[3], const double b[3]) {
    return m[0] * (b[0] - a[0]) + m[1] * (b[1] - a[1]) + m[2] * (b[2] - a[2]);
  };

  // Drag right: the near face moves right (dsx > 0), the yaw axis dominates (a=0.25 puts the
  // dominance limit at cot(pitch)/a ≈ 9.9 at the default 22°; 4 leaves margin), and yaw itself
  // decreased.
  ctx->ItemDragWithDelta("**/##r3d_canvas", ImVec2(24.0f, 0.0f));
  ctx->Yield(2);
  const float yaw1 = gui::g_state.analysis.point_view.orbit_yaw_deg;
  const float pitch1 = gui::g_state.analysis.point_view.orbit_pitch_deg;
  double fwd1[3];
  double right1[3];
  double up1[3];
  gui::OrbitBasis(yaw1, pitch1, fwd1, right1, up1);
  const double dsx = screen_motion(landmark, right0, right1);
  const double dsy = screen_motion(landmark, up0, up1);
  ctx->LogInfo("[ray3d orbit] right drag: yaw %.3f -> %.3f, dsx %.6f, dsy %.6f", yaw0, yaw1, dsx, dsy);
  IM_CHECK_RETV(dsx > 0.0, false);                               // grab: the near face follows the hand to the right
  IM_CHECK_RETV(std::fabs(dsx) >= 4.0 * std::fabs(dsy), false);  // yaw-dominant, not a nod
  IM_CHECK_RETV(yaw1 < yaw0, false);                             // a right drag decreases yaw

  // Drag down: the near face moves down (dsy < 0) and the screen-x motion is EXACTLY zero — in
  // the z-up basis `right` depends on yaw only, so a pure pitch drag cannot move anything
  // sideways. This invariant fails on the transposed basis (its right depends on pitch), so it
  // doubles as the red/green discriminator, not just a direction check.
  ctx->ItemDragWithDelta("**/##r3d_canvas", ImVec2(0.0f, 24.0f));
  ctx->Yield(2);
  const float pitch2 = gui::g_state.analysis.point_view.orbit_pitch_deg;
  double fwd2[3];
  double right2[3];
  double up2[3];
  gui::OrbitBasis(yaw1, pitch2, fwd2, right2, up2);
  const double dsx_down = screen_motion(landmark, right1, right2);
  const double dsy_down = screen_motion(landmark, up1, up2);
  ctx->LogInfo("[ray3d orbit] down drag: pitch %.3f -> %.3f, dsx %.6f, dsy %.6f", pitch1, pitch2, dsx_down, dsy_down);
  IM_CHECK_RETV(dsy_down < 0.0, false);              // grab: the near face follows the hand downward
  IM_CHECK_RETV(std::fabs(dsx_down) < 1e-9, false);  // a pitch drag never moves anything sideways
  IM_CHECK_RETV(pitch2 < pitch1, false);             // a down drag decreases pitch
  // (the pose itself is restored by the OrbitPoseRestore destructor above — on every path)

  // Playback writes the sample index and NOTHING else (AC3, pinned on the real app's frame loop):
  // the sky point and the ROI state are bitwise what they were, the selection untouched.
  const float sky_before[3] = { gui::g_state.analysis.sky_point.dir[0], gui::g_state.analysis.sky_point.dir[1],
                                gui::g_state.analysis.sky_point.dir[2] };
  const int roi_before = gui::g_state.analysis.roi_mode;
  const std::string selected_before_str = gui::g_state.analysis.selected_entry.value_or("");
  gui::g_state.analysis.point_view.playing = true;
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::g_state.analysis.point_view.sample_index != 1;  // the cursor moved (wrap included)
                    },
                    30),
                false);
  gui::g_state.analysis.point_view.playing = false;
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.dir[0] == sky_before[0] &&
                    gui::g_state.analysis.sky_point.dir[1] == sky_before[1] &&
                    gui::g_state.analysis.sky_point.dir[2] == sky_before[2],
                false);
  IM_CHECK_RETV(gui::g_state.analysis.roi_mode == roi_before, false);
  IM_CHECK_RETV(gui::g_state.analysis.selected_entry.value_or("") == selected_before_str, false);
  ctx->LogInfo("[point view] playback stepped to sample %d; sky point and ROI untouched",
               gui::g_state.analysis.point_view.sample_index);
  return true;
}

// The rank-0 arm (AC5) on the corpus plate scene (C14's shape: the horizontal plate whose 1-2
// chain sends the sun's image straight through — the point at the sun itself is a point mass,
// not a fiber). Fixed-dt pool: the mass arm answers in milliseconds.
bool PointMassArmIsHonestAndForcesNoFiber(ImGuiTestContext* ctx) {
  static const char* const kPlateJson = R"({
    "crystal": [ { "id": 1, "type": "prism",
                   "shape": { "height": 0.3, "face_distance": [1,1,1,1,1,1] },
                   "axis": { "zenith": 0, "azimuth": {"type": "uniform", "mean": 0, "std": 360}, "roll": 0 } } ],
    "filter": [],
    "scene": { "light_source": { "type": "sun", "altitude": 20, "azimuth": 0, "diameter": 0.5,
                                 "spectrum": [ { "wavelength": 550, "weight": 1 } ] },
               "ray_num": 1000, "max_hits": 7,
               "scattering": [ { "prob": 0, "entries": [ { "crystal": 1, "proportion": 1 } ] } ] },
    "render": []
  })";
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kPlateJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  // The plate scene's basal row: the chain spelled 1-2 in the list (the sun's direct image).
  SetListRef(ctx);
  ctx->ItemClick("**/1-2");
  ctx->SetRef("");
  IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::SelectedClassIdentityFor(gui::g_state).has_value(); }, 10), false);
  // The point at the sun's own position: rank 0 — the direction-identity path.
  SetSkyPointTarget(20.0, 0.0);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          30),
      false);
  LUMICE_PathPointInfo info{};
  info.struct_size = sizeof(info);
  IM_CHECK_RETV(LUMICE_PathPointGetInfo(gui::g_state.analysis.point.adopted_result.get(), &info) == LUMICE_PATH_READ_OK,
                false);
  IM_CHECK_RETV(info.outcome == LUMICE_PATH_POINT_MASS, false);
  // No fiber is forced: no component rows, no ray intent, the trace stays hidden (AC5).
  std::vector<gui::PointComponentRow> rows;
  IM_CHECK_RETV(gui::PointComponentRows(*gui::g_state.analysis.point.adopted_result, &rows) == 0, false);
  IM_CHECK_RETV(gui::g_state.analysis.trace.intent_valid == false, false);
  LUMICE_PathPointMass mass{};
  mass.struct_size = sizeof(mass);
  IM_CHECK_RETV(LUMICE_PathPointGetMass(gui::g_state.analysis.point.adopted_result.get(), &mass) == LUMICE_PATH_READ_OK,
                false);
  // The mass IS the sun's image: the separation is the float intent roundtrip's own error (the
  // GUI's sky-point dir is float; the CLI's double path measures 4.5e-12 on the same scene).
  IM_CHECK_RETV(std::fabs(mass.target_separation_deg) < 1e-3, false);
  ctx->LogInfo("[point view] rank-0 mass at (alt %.3f, az %.3f), separation %.2e deg", mass.altitude_deg,
               mass.azimuth_deg, mass.target_separation_deg);
  IM_CHECK_RETV(SaveWindowPng(ctx, "//##AnalyzeRightPanel", GuiTestTempPath("analyze_point_mass.png").string()), false);
  return true;
}

// The dispersion-on-screen face (742.4, AC2): on the fully random prism — the C01 shape written
// the way the routing v1 domain reads as the full-sphere random family — the dominant class's
// overview carries the layered full-dimensional facts, and the chromatic block on screen holds
// the two-lambda min-deviation row with red inside blue and the closed-form certificate behind
// it. The assertions read the same model the region draws, so the screenshot a human reads and
// the checks below cannot both be mock; the picture is the evidence of the block being VISIBLE.
bool SurveyOverviewShowsDispersionAndProfile(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  const std::string top_display = TopChainDisplay();
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + top_display).c_str());
  ctx->SetRef("");
  // The survey's chromatic legs (the two-lambda tint power sums) are the cost here — give the
  // background query its minute.
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    60),
                false);
  const gui::SurveyQuerySession& survey = gui::g_state.analysis.survey;
  IM_CHECK_RETV(survey.adopted_result != nullptr, false);

  const gui::SurveyOverviewModel model = gui::BuildSurveyOverviewModel(*survey.adopted_result);
  IM_CHECK_RETV(model.valid, false);
  IM_CHECK_RETV(model.arm == gui::SurveyArm::kFullDimensional, false);
  IM_CHECK_RETV(model.dispersion.available, false);
  IM_CHECK_RETV(model.dispersion.min_row_available, false);
  // The red-inside-blue fact itself: the 650nm inner edge sits at a SMALLER deviation than the
  // 450nm one, and the displacement is the row's own spread.
  IM_CHECK_RETV(model.dispersion.value_red_deg < model.dispersion.value_blue_deg, false);
  IM_CHECK_RETV(model.dispersion.direction == "red_inner_blue_outer", false);
  IM_CHECK_RETV(std::fabs(model.dispersion.displacement_deg -
                          (model.dispersion.value_blue_deg - model.dispersion.value_red_deg)) < 1e-9,
                false);
  IM_CHECK_RETV(model.dispersion.certificate_available, false);
  IM_CHECK_RETV(model.probe.available && model.probe.peak_available, false);
  IM_CHECK_RETV(model.tint.available, false);
  ctx->LogInfo("[survey overview] dispersion %.3f/%.3f (Δ%.3f°, %s), probe %d rows, apex %.1f°",
               model.dispersion.value_red_deg, model.dispersion.value_blue_deg, model.dispersion.displacement_deg,
               model.dispersion.direction.c_str(), static_cast<int>(model.probe.sample_value.size()),
               model.dispersion.certificate_apex_deg);
  IM_CHECK_RETV(
      SaveWindowPng(ctx, "//##AnalyzeRightPanel", GuiTestTempPath("analysis_survey_overview_dispersion.png").string()),
      false);
  // The profile block, expanded: the reference curve on screen is the human-readable half of
  // the probe block's evidence (the samples and their gap rule are the model test's). The
  // header lives inside the details child, so the item is addressed through the child wildcard
  // (the GUI test engine-addressing rule for items inside a named child).
  ctx->ItemClick("**/Reference profile — 550 nm##ov_prof");
  ctx->Yield(2);
  // Bring the curve into the frame: the readings sit above it, and the panel's own height ends
  // right at them (the block ordering is readings first, curve behind). ScrollToItemY — the
  // programmatic scroll, not the wheel: the right panel declines user-wheel scrolling.
  ctx->ScrollToItemY("**/Internal-reflection share##ov_tir");
  ctx->Yield(2);
  IM_CHECK_RETV(
      SaveWindowPng(ctx, "//##AnalyzeRightPanel", GuiTestTempPath("analysis_survey_overview_profile.png").string()),
      false);
  return true;
}

// The engine's named refusal, on the two-layer scene: a multi-layer class is a legitimate
// selection and the survey answers it with multi_layer_unsupported — shown, not fabricated
// around (R5/R7; the multi-layer evidence the plan's M3-5 asks to be on file).
bool SurveyRefusesMultilayerClassByName(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kPcTwoLayerJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  // A row whose chain crosses layers: the display holds the joiner (this scene's reason to be
  // in the suite — its comment says so).
  std::string multi_display;
  for (const int idx : gui::g_state.analysis_result.display_order) {
    const auto& e = gui::g_state.analysis_result.payload->entries[static_cast<size_t>(idx)];
    if (std::strstr(e.display, " -> ") != nullptr) {
      multi_display = e.display;
      break;
    }
  }
  IM_CHECK_RETV(!multi_display.empty(), false);
  // The on-screen label is not the raw display: multi-layer rows are drawn through
  // AnalysisRowLabel over the Analyze-time core-to-pool map (the row_menu case's pattern), and
  // on this scene the first multi-layer row sits below the first page of the ScrollY table —
  // pan the table's child to it before clicking (FindListRowScrolling; a clipped row reports no
  // label at all). Selecting by the drawn label records the raw text.
  const std::string multi_label = gui::AnalysisRowLabel(multi_display, gui::AnalysisCoreToPoolMap(gui::g_state));
  SetListRef(ctx);
  const ImGuiID multi_row = FindListRowScrolling(ctx, multi_label);
  IM_CHECK_RETV(multi_row != 0, false);
  ctx->ItemClick(multi_row);
  ctx->SetRef("");
  ctx->Yield(3);
  {
    // Diagnostic, kept: the case exists to pin a named refusal, and these four lines tell a
    // fresh reader which link of issue→refuse→adopt broke if it ever reds again.
    const auto& s = gui::g_state.analysis.survey;
    ctx->LogInfo("[survey refusal] after issue: intent=%d in_flight=%d state=%d err='%s'", s.intent_valid ? 1 : 0,
                 gui::g_survey_executor.InFlight() ? 1 : 0, static_cast<int>(gui::SurveyOverviewStateFor(s)),
                 s.adopted_error.c_str());
  }
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kRefused;
                    },
                    30),
                false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_error.find("multi_layer_unsupported") != std::string::npos, false);
  ctx->LogInfo("[survey refusal] %s", gui::g_state.analysis.survey.adopted_error.c_str());
  IM_CHECK_RETV(SaveWindowPng(ctx, "//##AnalyzeRightPanel", GuiTestTempPath("analysis_survey_refusal.png").string()),
                false);
  return true;
}

// R6's concurrency stance, both directions, on the real server: a survey computing in the
// background neither blocks nor is blocked by a render run — the run starts and finishes under
// an in-flight survey with the survey state intact, and a selection made under a running run
// still gets its answer. No mutex story: both observations are the states themselves.
bool SurveyAndRenderRunAreOrthogonal(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  // Direction 1 — survey first, run under it.
  const std::string top_display = TopChainDisplay();
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + top_display).c_str());
  ctx->SetRef("");
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    30),
                false);
  const gui::SurveyHandle adopted_before = gui::g_state.analysis.survey.adopted_result;
  gui::g_state.sim.infinite = false;
  gui::g_state.sim.ray_num_millions = 0.1f;
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo("##TopBar/" ICON_FA_PLAY " Run")), false);
  ctx->ItemClick("##TopBar/" ICON_FA_PLAY " Run");
  IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kSimulating; }, 10), false);
  gui::g_state.dirty = false;  // the real loop's commit tick clears it (the c01 chain's note)
  IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kDone; }, 60), false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result == adopted_before, false);
  IM_CHECK_RETV(gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) == gui::SurveyOverviewState::kCompleted,
                false);
  ctx->LogInfo("[survey orthogonal] a full render run passed under an intact adopted survey");

  // Direction 2 — run first (unbounded), selection under it. The list is the kept analysis
  // result (a render commit keeps it on show, flagged stale); the survey answers anyway,
  // because it is bound to the frozen Analyze-time scene and never touches the server session.
  gui::g_state.sim.infinite = true;
  IM_CHECK_RETV(!IsDisabled(ctx->ItemInfo("##TopBar/" ICON_FA_PLAY " Run")), false);
  ctx->ItemClick("##TopBar/" ICON_FA_PLAY " Run");
  IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kSimulating; }, 10), false);
  const std::string other_display = TopChainDisplay();
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + other_display).c_str());
  ctx->SetRef("");
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [&] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                                 gui::SurveyOverviewState::kCompleted &&
                             gui::g_state.sim_state == SimState::kSimulating;
                    },
                    30),
                false);
  ctx->LogInfo("[survey orthogonal] a selection's survey completed under a running render");
  // The top-bar Stop ends the render run (the list window's own Stop belongs to the analysis
  // run and sits disabled under the render's Run↔Analyze mutual exclusion).
  ctx->ItemClick("##TopBar/" ICON_FA_STOP " Stop");
  ctx->SetRef("");
  IM_CHECK_RETV(DriveUntil(ctx, [] { return !gui::g_state.analysis.started; }, 10), false);
  return true;
}

// R6's teardown matrix, the rows the frame paths own: switching to the Render workspace cancels
// nothing and clears nothing (the query finishes into the mailbox; coming back adopts it), New
// clears the session and releases the executor's binding, Revert keeps the session with the
// kept selection and only withdraws an in-flight query.
bool SurveySurvivesAWorkspaceSwitch(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  const std::string top_display = TopChainDisplay();
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + top_display).c_str());
  ctx->SetRef("");
  // Switch away before the answer lands: the query keeps running (the Render frame pumps
  // nothing), and the switch itself is pure view — no dirty, no sim-state write.
  // The toggle is single-slot: from Analyze it reads bare "Render" (no icon prefix — the
  // product's kToRenderLabel), naming the destination.
  ctx->ItemClick("##TopBar/Render");
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.workspace.mode == gui::GuiState::WorkspaceMode::kRender, false);
  IM_CHECK_RETV(gui::g_state.dirty == false, false);

  // Back in the Analyze workspace: the pump adopts whatever finished while away.
  ctx->ItemClick("##TopBar/" ICON_FA_ROUTE " Analyze");
  ctx->Yield(2);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    30),
                false);
  ctx->LogInfo("[survey switch] the query finished across a workspace switch and was adopted");

  // New: the whole analysis session dies with the document — survey bookkeeping included — and
  // the executor's binding is released on the worker thread.
  ResetTestState();
  IM_CHECK_RETV(gui::g_state.analysis.survey.intent_valid == false, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result == nullptr, false);
  IM_CHECK_RETV(!gui::g_survey_executor.InFlight(), false);

  // Revert with a query IN FLIGHT: the withdrawal carries the cancel latch (the engine answers
  // a cancel with an OK-carrying partial document, and that answer must not land as this
  // selection's completed digest). Selecting and reverting back-to-back, with no Yield between,
  // means the query cannot have finished yet — the latch is what the overview then shows.
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  SetListRef(ctx);
  const std::string in_flight_display = TopChainDisplay();
  ctx->ItemClick((std::string("**/") + in_flight_display).c_str());
  gui::DoRevert();
  ctx->SetRef("");
  IM_CHECK_RETV(DriveUntil(ctx, [] { return !gui::g_survey_executor.InFlight(); }, 20), false);
  IM_CHECK_RETV(gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) == gui::SurveyOverviewState::kCancelled,
                false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_error.empty(), false);
  ctx->LogInfo("[survey switch] Revert withdrew an in-flight query under the cancel latch");
  // The kept selection is not re-issued under the latch; a DIFFERENT selection clears the latch
  // with its new intent and answers (the top row again would be the same key — the latch holds).
  const auto& kept_view = gui::g_state.analysis_result;
  const std::string other_display = kept_view.payload->entries[static_cast<size_t>(kept_view.display_order[1])].display;
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + other_display).c_str());
  ctx->SetRef("");
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    30),
                false);
  ctx->LogInfo("[survey switch] a changed selection re-armed after the reverted query");

  // Revert after adoption: rebuild the analysis state, adopt a survey, then revert — the
  // bookkeeping survives with the kept selection (same committed scene), nothing re-issues
  // under it, and the adopted digest stays on show (no latch was set — nothing was withdrawn).
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  SetListRef(ctx);
  const std::string revert_display = TopChainDisplay();
  ctx->ItemClick((std::string("**/") + revert_display).c_str());
  ctx->SetRef("");
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    30),
                false);
  const gui::SurveyHandle adopted_before_revert = gui::g_state.analysis.survey.adopted_result;
  gui::DoRevert();
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analysis.survey.intent_valid == true, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result == adopted_before_revert, false);
  IM_CHECK_RETV(!gui::g_survey_executor.InFlight(), false);
  ctx->LogInfo("[survey switch] Revert kept the session and the adopted outcome");
  return true;
}

// ---- 742.7's integration legs: the cross-slice combination evidence the per-slice legs leave
// out — one session driving the whole C01 chain, the internal-reflection path seen from its real
// consumer, and the new faces' lifecycle across row switches and document actions. The claims are
// session-level (one selection feeding every face coherently), not per-face re-pins: each face's
// own contract stays with the leg that introduced it.

// The continuous chain (v1-scope §4 C01): ONE selection, ONE session — the class overview (which
// must not grab a sky point for the user), the refined reference map, a real pick on the canvas,
// the point document, the fiber, the factors and the representative path — every echo naming the
// same selected class. Real-timing pool, the per-face legs' shape: the survey's chromatic legs,
// then the map refinement and the point queue behind it on the one worker.
bool AnalyzeFullChainOneSession(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  // Before any selection every face sits in its honest no-intent state — the baseline the
  // "the default chain never grabs a point" claim is read against.
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid == false, false);
  IM_CHECK_RETV(gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kNoIntent, false);
  IM_CHECK_RETV(gui::MapQueryStateFor(gui::g_state.analysis.map) == gui::MapQueryState::kNoSelection, false);
  IM_CHECK_RETV(gui::g_state.analysis.trace.intent_valid == false, false);

  const std::string top_display = TopChainDisplay();
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + top_display).c_str());
  ctx->SetRef("");
  const std::optional<gui::SelectedClassIdentity> identity = gui::SelectedClassIdentityFor(gui::g_state);
  IM_CHECK_RETV(identity.has_value(), false);
  // The overview settles (the chromatic legs are the wait), and the settled chain has STILL
  // written no sky point: the default overview never picks for the user (v1-scope B).
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    60),
                false);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid == false, false);
  IM_CHECK_RETV(gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kNoIntent, false);
  IM_CHECK_RETV(gui::g_state.analysis.trace.intent_valid == false, false);
  // The session-level dispersion readings (the 742.4 leg owns the full pin; the chain carries
  // them here): the dual-lambda inner edges, red inside blue, from the one real survey. Under
  // pool co-tenancy the survey's chromatic legs can honestly hit the 15 s budget — the partial
  // keeps the routing facts and drops the dispersion row; the leg pins the dispersion on the
  // survey's own terms (completed → the readings must be there) and names the partial otherwise.
  {
    const gui::SurveyOverviewModel model = gui::BuildSurveyOverviewModel(*gui::g_state.analysis.survey.adopted_result);
    IM_CHECK_RETV(model.valid && model.arm == gui::SurveyArm::kFullDimensional, false);
    ctx->LogInfo("[v1 chain] survey execution %d, budget %d ms, early-stopped %d, dispersion available %d",
                 model.execution, model.budget_ms, model.early_stopped ? 1 : 0, model.dispersion.available ? 1 : 0);
    if (model.dispersion.available) {
      IM_CHECK_RETV(model.dispersion.min_row_available, false);
      IM_CHECK_RETV(model.dispersion.value_red_deg < model.dispersion.value_blue_deg, false);
      IM_CHECK_RETV(model.dispersion.direction == "red_inner_blue_outer", false);
      IM_CHECK_RETV(model.tint.available, false);
      ctx->LogInfo("[v1 chain] dispersion %.3f/%.3f (delta %.3f deg, %s)", model.dispersion.value_red_deg,
                   model.dispersion.value_blue_deg, model.dispersion.displacement_deg,
                   model.dispersion.direction.c_str());
    } else {
      IM_CHECK_RETV(model.execution == LUMICE_PATH_OUTCOME_DEADLINE_EXCEEDED, false);
      ctx->LogInfo("[v1 chain] the survey stopped inside its budget — the dispersion row is the honest partial");
    }
  }
  IM_CHECK_RETV(SaveWindowPng(ctx, "//##AnalyzeRightPanel", GuiTestTempPath("analyze_v1_chain_overview.png").string()),
                false);

  // The map window answers behind the survey on the one worker (default chain: full sky, then
  // the auto-Fit, then the refinement), and its echo names the SAME class the overview
  // summarized — one identity feeding both faces.
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      if (m.adopted_result == nullptr || !(m.adopted_id == m.intent_id)) {
                        return false;
                      }
                      LUMICE_PathMapInfo i{};
                      i.struct_size = sizeof(i);
                      return LUMICE_PathMapGetInfo(m.adopted_result.get(), &i) == LUMICE_PATH_READ_OK &&
                             i.view_full_sky == 0;
                    },
                    60),
                false);
  {
    const gui::MapQuerySession& map = gui::g_state.analysis.map;
    LUMICE_PathMapInfo info{};
    info.struct_size = sizeof(info);
    IM_CHECK_RETV(LUMICE_PathMapGetInfo(map.adopted_result.get(), &info) == LUMICE_PATH_READ_OK, false);
    IM_CHECK_RETV(info.crystal_id == identity->core_crystal_id, false);
    IM_CHECK_RETV(info.kept_count > 0, false);
    ctx->LogInfo("[v1 chain] map lambda %.1f, kept %lld, window delta[%.1f,%.1f]", info.wavelength_nm, info.kept_count,
                 info.view_delta_min_deg, info.view_delta_max_deg);
  }
  // One pan, then one zoom-in view change (the gesture writes' shape — the 742.5 leg's
  // precedent): the map re-asks under the new centre and then the new window, and each
  // refinement lands while the session stays coherent.
  gui::g_state.analysis.map_view.centre_pos[0] += 0.1f;  // a pan off the mass centre
  gui::g_state.analysis.map_view.user_moved = true;
  gui::MapViewChanged(&gui::g_state.analysis.map_view, ImGui::GetTime(), false, /*user_steered=*/true);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      if (m.adopted_result == nullptr || !(m.adopted_id == m.intent_id)) {
                        return false;
                      }
                      LUMICE_PathMapInfo i{};
                      i.struct_size = sizeof(i);
                      return LUMICE_PathMapGetInfo(m.adopted_result.get(), &i) == LUMICE_PATH_READ_OK &&
                             i.view_full_sky == 0;
                    },
                    30),
                false);
  gui::g_state.analysis.map_view.radius_deg = 10.0f;
  gui::g_state.analysis.map_view.user_moved = true;
  gui::MapViewChanged(&gui::g_state.analysis.map_view, ImGui::GetTime(), false, /*user_steered=*/true);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      if (m.adopted_result == nullptr || !(m.adopted_id == m.intent_id)) {
                        return false;
                      }
                      LUMICE_PathMapInfo i{};
                      i.struct_size = sizeof(i);
                      return LUMICE_PathMapGetInfo(m.adopted_result.get(), &i) == LUMICE_PATH_READ_OK &&
                             i.view_full_sky == 0 && i.view_delta_max_deg < 12.0;
                    },
                    30),
                false);
  IM_CHECK_RETV(SaveWindowPng(ctx, "//##AnalyzeLowerRow", GuiTestTempPath("analyze_v1_chain_map.png").string()), false);

  // The real pick on the canvas centre: the point intent arms through the real mouse path (the
  // pick's outcome is the discovery arm's own honest answer at whatever spot the class's mass
  // centroid is — off-fiber spots answer an empty discovered document, which is the display's
  // named-empty state, not a failure). The document the chain then asserts on is pinned by the
  // anchor write below — the 742.6 leg's verified on-fiber target for this scene's halo fiber,
  // written through the same state the pick path writes ("the same write the button performs").
  ctx->ItemClick("**/##map_canvas");
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid, false);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.from_map_pick, false);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  // The pick's own document is a real terminal state and gets pinned as one: the discovery arm's
  // outcome is read back honestly (on-fiber or not), its component count logged — this step
  // carries the AC2 "an empty answer is still an answer" evidence at the combination level
  // (the anchor write below then re-keys to the deterministic document).
  {
    LUMICE_PathPointInfo pick_info{};
    pick_info.struct_size = sizeof(pick_info);
    IM_CHECK_RETV(
        LUMICE_PathPointGetInfo(gui::g_state.analysis.point.adopted_result.get(), &pick_info) == LUMICE_PATH_READ_OK,
        false);
    IM_CHECK_RETV(pick_info.outcome == LUMICE_PATH_POINT_DISCOVERED, false);
    std::vector<gui::PointComponentRow> pick_rows;
    const int pick_components = gui::PointComponentRows(*gui::g_state.analysis.point.adopted_result, &pick_rows);
    ctx->LogInfo("[v1 chain] the pick's own document: outcome DISCOVERED, %d component(s) at the centroid spot",
                 pick_components);
  }
  SetSkyPointTarget(42.0, 0.0);
  ctx->Yield(2);  // the pump re-arms under the new key before the wait reads the state
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  {
    const gui::PointQuerySession& point = gui::g_state.analysis.point;
    LUMICE_PathPointInfo info{};
    info.struct_size = sizeof(info);
    IM_CHECK_RETV(LUMICE_PathPointGetInfo(point.adopted_result.get(), &info) == LUMICE_PATH_READ_OK, false);
    IM_CHECK_RETV(info.crystal_id == identity->core_crystal_id, false);
    IM_CHECK_RETV(info.outcome == LUMICE_PATH_POINT_DISCOVERED, false);
    const gui::MapQuerySession& map = gui::g_state.analysis.map;
    LUMICE_PathMapInfo map_info{};
    map_info.struct_size = sizeof(map_info);
    IM_CHECK_RETV(LUMICE_PathMapGetInfo(map.adopted_result.get(), &map_info) == LUMICE_PATH_READ_OK, false);
    // One wavelength knob feeds both faces (the engine's reference echo, read back from each).
    IM_CHECK_RETV(info.wavelength_nm == map_info.wavelength_nm, false);
    // The caliber lines are the engine's own echo (the D65-vs-reference caliber the user reads).
    const std::vector<std::string> caliber = gui::PointCaliberLines(info);
    IM_CHECK_RETV(!caliber.empty(), false);
    std::vector<gui::PointComponentRow> rows;
    const int components = gui::PointComponentRows(*point.adopted_result, &rows);
    IM_CHECK_RETV(components >= 1, false);
    const int valid_count = gui::PointValidSampleCount(*point.adopted_result, 0);
    IM_CHECK_RETV(valid_count > 1, false);
    LUMICE_PathPointSample sample{};
    sample.struct_size = sizeof(sample);
    const int stored = gui::PointValidSampleIndexAt(*point.adopted_result, 0, 0);
    IM_CHECK_RETV(LUMICE_PathPointGetSample(point.adopted_result.get(), 0, static_cast<size_t>(stored), &sample) ==
                      LUMICE_PATH_READ_OK,
                  false);
    const std::vector<std::string> factors = gui::PointFactorLines(sample);
    IM_CHECK_RETV(!factors.empty(), false);
    ctx->LogInfo("[v1 chain] point alt %.2f az %.2f dev %.2f, lambda %.1f, %d component(s), %d valid samples",
                 info.target_altitude_deg, info.target_azimuth_deg, info.target_deviation_deg, info.wavelength_nm,
                 components, valid_count);
    for (const std::string& line : caliber) {
      ctx->LogInfo("[v1 chain] caliber: %s", line.c_str());
    }
  }

  // The representative path follows the same session's cursor: sample 0 drawn, a slider step
  // re-keys it, and the C01 chain's own shape comes back through the C readers (2 contacts —
  // entry face, exit face — 3 segments: the entry/internal/exit polyline).
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::RayTraceSession& t = gui::g_state.analysis.trace;
                      return t.adopted_result != nullptr && t.intent_valid &&
                             t.key.sample ==
                                 gui::PointValidSampleIndexAt(*gui::g_state.analysis.point.adopted_result, 0, 0);
                    },
                    30),
                false);
  gui::g_state.analysis.point_view.sample_index = 1;
  gui::g_state.analysis.point_view.playing = false;
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::RayTraceSession& t = gui::g_state.analysis.trace;
                      if (!t.adopted_result || !t.intent_valid) {
                        return false;
                      }
                      const int stored =
                          gui::PointValidSampleIndexAt(*gui::g_state.analysis.point.adopted_result, 0, 1);
                      return t.key.sample == stored && gui::RayTraceDisplayStateFor(t, { t.key.point_id, 0, stored }) ==
                                                           gui::RayTraceDisplayState::kDrawn;
                    },
                    30),
                false);
  {
    LUMICE_PathRayContacts contacts{};
    contacts.struct_size = sizeof(contacts);
    LUMICE_PathRayLegs legs{};
    legs.struct_size = sizeof(legs);
    IM_CHECK_RETV(LUMICE_RepresentativeRayGetContacts(gui::g_state.analysis.trace.adopted_result.get(), &contacts) ==
                      LUMICE_PATH_READ_OK,
                  false);
    IM_CHECK_RETV(
        LUMICE_RepresentativeRayGetLegs(gui::g_state.analysis.trace.adopted_result.get(), &legs) == LUMICE_PATH_READ_OK,
        false);
    IM_CHECK_RETV(contacts.contact_count == 2, false);
    IM_CHECK_RETV(legs.leg_count == 3, false);
  }
  IM_CHECK_RETV(SaveWindowPng(ctx, "//##AnalyzeLowerRow", GuiTestTempPath("analyze_v1_chain_trace.png").string()),
                false);
  IM_CHECK_RETV(SaveWindowPng(ctx, PanoramaRegionRef(), GuiTestTempPath("analyze_v1_chain_panorama.png").string()),
                false);
  ctx->LogInfo("[v1 chain] one session: overview, map, pick, point, fiber, factors, trace — identity %u",
               static_cast<unsigned>(identity->core_crystal_id));
  return true;
}

// The internal-reflection path from its real consumer (v1-scope §4: beyond C01's two-face
// transmission): the corpus's C03 chain `3-5-6-7` — the same scene's L0 list — selected as a
// real row, its own reference map refined, a real pick on the canvas, and the 3D region drawing
// the recovered polyline. The pin shape is the engine document's (4 contacts, 5 segments —
// entry, three internal, exit), read back through the C readers, not a screenshot's looks.
// Real-timing pool: the class survey's chromatic legs queue the map and the point on the one
// worker, the standing shape.
bool InternalReflectionTraceFromTheRealConsumer(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  // The C03 chain in this scene's L0 list (the corpus's internal-reflection representative; the
  // scene was chosen so this row exists — a red here means the scene choice broke, and says so).
  std::string c03_display;
  for (const int idx : gui::g_state.analysis_result.display_order) {
    const auto& e = gui::g_state.analysis_result.payload->entries[static_cast<size_t>(idx)];
    if (std::strcmp(e.display, "3-5-6-7") == 0) {
      c03_display = e.display;
      break;
    }
  }
  IM_CHECK_RETV(!c03_display.empty(), false);
  const std::string c03_label = gui::AnalysisRowLabel(c03_display, gui::AnalysisCoreToPoolMap(gui::g_state));
  SetListRef(ctx);
  const ImGuiID c03_row = FindListRowScrolling(ctx, c03_label);
  IM_CHECK_RETV(c03_row != 0, false);
  ctx->ItemClick(c03_row);
  ctx->SetRef("");
  const std::optional<gui::SelectedClassIdentity> identity = gui::SelectedClassIdentityFor(gui::g_state);
  IM_CHECK_RETV(identity.has_value(), false);

  // The class's own map refines (behind its survey on the one worker) and echoes the class.
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      if (m.adopted_result == nullptr || !(m.adopted_id == m.intent_id)) {
                        return false;
                      }
                      LUMICE_PathMapInfo i{};
                      i.struct_size = sizeof(i);
                      return LUMICE_PathMapGetInfo(m.adopted_result.get(), &i) == LUMICE_PATH_READ_OK &&
                             i.view_full_sky == 0;
                    },
                    60),
                false);
  // A real pick on the canvas centre — the real mouse path arms the intent (its own outcome is
  // the discovery arm's honest answer at the centroid spot). The document the leg asserts on is
  // then pinned to the (38, 180) anchor: the M0 probe's verified on-fiber target for this
  // scene's 3-5-6-7 fiber (two components, ~120 deg from the sun — the corpus's antisolar
  // concentration band; CLI probe on this exact geometry), written through the same state the
  // pick path writes. Not (-20, 180): that point IS the sun's antipode, where the engine
  // honestly refuses the target (kInvalidTarget) instead of answering.
  ctx->ItemClick("**/##map_canvas");
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid, false);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  // The pick's own document read back honestly (outcome + component count logged) — an empty
  // discovery at the centroid is a real terminal state, not a failure the leg papers over.
  {
    LUMICE_PathPointInfo pick_info{};
    pick_info.struct_size = sizeof(pick_info);
    IM_CHECK_RETV(
        LUMICE_PathPointGetInfo(gui::g_state.analysis.point.adopted_result.get(), &pick_info) == LUMICE_PATH_READ_OK,
        false);
    IM_CHECK_RETV(pick_info.outcome == LUMICE_PATH_POINT_DISCOVERED, false);
    std::vector<gui::PointComponentRow> pick_rows;
    const int pick_components = gui::PointComponentRows(*gui::g_state.analysis.point.adopted_result, &pick_rows);
    ctx->LogInfo("[v1 c03] the pick's own document: outcome DISCOVERED, %d component(s) at the centroid spot",
                 pick_components);
  }
  SetSkyPointTarget(38.0, 180.0);
  ctx->Yield(2);  // the pump re-arms under the new key before the wait reads the state
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  const gui::PointQuerySession& point = gui::g_state.analysis.point;
  LUMICE_PathPointInfo info{};
  info.struct_size = sizeof(info);
  IM_CHECK_RETV(LUMICE_PathPointGetInfo(point.adopted_result.get(), &info) == LUMICE_PATH_READ_OK, false);
  IM_CHECK_RETV(info.crystal_id == identity->core_crystal_id, false);
  IM_CHECK_RETV(info.outcome == LUMICE_PATH_POINT_DISCOVERED, false);
  std::vector<gui::PointComponentRow> rows;
  const int components = gui::PointComponentRows(*point.adopted_result, &rows);
  IM_CHECK_RETV(components >= 1, false);
  IM_CHECK_RETV(gui::PointValidSampleCount(*point.adopted_result, 0) > 0, false);
  ctx->LogInfo("[v1 c03] target (alt %.2f, az %.2f, dev %.2f), %d component(s)", info.target_altitude_deg,
               info.target_azimuth_deg, info.target_deviation_deg, components);

  // The 3D region draws the recovered path for the current sample; the document's shape is the
  // recovery's own pin: four contacts, five segments, kinds in the recovery's order.
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::RayTraceSession& t = gui::g_state.analysis.trace;
                      if (!t.adopted_result || !t.intent_valid) {
                        return false;
                      }
                      const int stored =
                          gui::PointValidSampleIndexAt(*gui::g_state.analysis.point.adopted_result, 0, 0);
                      return t.key.sample == stored && gui::RayTraceDisplayStateFor(t, { t.key.point_id, 0, stored }) ==
                                                           gui::RayTraceDisplayState::kDrawn;
                    },
                    30),
                false);
  {
    LUMICE_PathRayContacts contacts{};
    contacts.struct_size = sizeof(contacts);
    LUMICE_PathRayLegs legs{};
    legs.struct_size = sizeof(legs);
    IM_CHECK_RETV(LUMICE_RepresentativeRayGetContacts(gui::g_state.analysis.trace.adopted_result.get(), &contacts) ==
                      LUMICE_PATH_READ_OK,
                  false);
    IM_CHECK_RETV(
        LUMICE_RepresentativeRayGetLegs(gui::g_state.analysis.trace.adopted_result.get(), &legs) == LUMICE_PATH_READ_OK,
        false);
    IM_CHECK_RETV(contacts.contact_count == 4, false);
    IM_CHECK_RETV(legs.leg_count == 5, false);
    gui::RayTracePolyline line;
    IM_CHECK_RETV(BuildRayTracePolyline(*gui::g_state.analysis.trace.adopted_result, 1.1f, &line), false);
    IM_CHECK_RETV(line.seg_kinds.size() == 5, false);
    IM_CHECK_RETV(line.seg_kinds[0] == 0 && line.seg_kinds[1] == 1 && line.seg_kinds[2] == 1 &&
                      line.seg_kinds[3] == 1 && line.seg_kinds[4] == 2,
                  false);
  }
  IM_CHECK_RETV(SaveWindowPng(ctx, "//##AnalyzeLowerRow", GuiTestTempPath("analyze_v1_c03_trace.png").string()), false);
  ctx->LogInfo("[v1 c03] the internal-reflection path drawn in the 3D region: entry, 3 internal, exit");
  return true;
}

// The new faces' lifecycle across row switches (v1-scope E): select A, switch to B while A's
// queries are still in flight, and every face re-keys to B — the adopted outcomes name B, the
// lambda knob re-asks the map and the point under the same session rules. The halo scene keeps
// the worker costs inside the correctness pool's fixed-dt window (the workspace-switch leg's
// precedent); the claim is the rekeying, not the physics.
bool AnalyzeFacesRekeyOnRowSwitch(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  // A sky point intent, so the point face (and through it the ray face) participates in the
  // switch: an arbitrary named direction — the claim is the session, not the physics.
  SetSkyPointTarget(40.0, 0.0);
  const auto& view = gui::g_state.analysis_result;
  const std::string a_display = view.payload->entries[static_cast<size_t>(view.display_order[0])].display;
  const std::string b_display = view.payload->entries[static_cast<size_t>(view.display_order[1])].display;
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + a_display).c_str());
  ctx->Yield(1);
  IM_CHECK_RETV(gui::SelectedClassIdentityFor(gui::g_state).has_value(), false);
  // B lands while A's faces are still working — no wait for A.
  ctx->ItemClick((std::string("**/") + b_display).c_str());
  ctx->SetRef("");
  const std::optional<gui::SelectedClassIdentity> identity_b = gui::SelectedClassIdentityFor(gui::g_state);
  IM_CHECK_RETV(identity_b.has_value(), false);

  // B's survey and map adopt; the gates answer B's keys (A's outcomes structurally cannot pass).
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    60),
                false);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      if (m.adopted_result == nullptr || !(m.adopted_id == m.intent_id)) {
                        return false;
                      }
                      LUMICE_PathMapInfo i{};
                      i.struct_size = sizeof(i);
                      return LUMICE_PathMapGetInfo(m.adopted_result.get(), &i) == LUMICE_PATH_READ_OK &&
                             i.view_full_sky == 0;
                    },
                    60),
                false);
  {
    LUMICE_PathMapInfo info{};
    info.struct_size = sizeof(info);
    IM_CHECK_RETV(LUMICE_PathMapGetInfo(gui::g_state.analysis.map.adopted_result.get(), &info) == LUMICE_PATH_READ_OK,
                  false);
    IM_CHECK_RETV(info.crystal_id == identity_b->core_crystal_id, false);
  }
  // The point face settles terminal under B and names B in its echo.
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::PointQueryState st = gui::PointQueryStateFor(gui::g_state.analysis.point);
                      return st == gui::PointQueryState::kCompleted || st == gui::PointQueryState::kRefused;
                    },
                    60),
                false);
  if (gui::g_state.analysis.point.adopted_result != nullptr) {
    LUMICE_PathPointInfo info{};
    info.struct_size = sizeof(info);
    IM_CHECK_RETV(
        LUMICE_PathPointGetInfo(gui::g_state.analysis.point.adopted_result.get(), &info) == LUMICE_PATH_READ_OK, false);
    IM_CHECK_RETV(info.crystal_id == identity_b->core_crystal_id, false);
  }
  ctx->LogInfo("[v1 rekey] B's survey, map and point adopted under B's identity; A's era dropped");

  // The lambda knob re-asks the map AND the point (the one shared knob rides both keys) in
  // flight; the echo carries 450, and resetting back re-asks both under the engine reference —
  // read off the document first, so the reset never leans on a "not 450" accident.
  float reference_lambda = 0.0f;
  {
    LUMICE_PathMapInfo i{};
    i.struct_size = sizeof(i);
    IM_CHECK_RETV(LUMICE_PathMapGetInfo(gui::g_state.analysis.map.adopted_result.get(), &i) == LUMICE_PATH_READ_OK,
                  false);
    reference_lambda = i.wavelength_nm;
    IM_CHECK_RETV(reference_lambda != 450.0f, false);  // the scene premise this leg leans on
  }
  gui::g_state.analysis.map_wavelength_nm = 450.0f;
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      if (m.adopted_result == nullptr || !(m.adopted_id == m.intent_id)) {
                        return false;
                      }
                      LUMICE_PathMapInfo i{};
                      i.struct_size = sizeof(i);
                      return LUMICE_PathMapGetInfo(m.adopted_result.get(), &i) == LUMICE_PATH_READ_OK &&
                             i.view_full_sky == 0 && i.wavelength_nm == 450.0f;
                    },
                    60),
                false);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::PointQueryState st = gui::PointQueryStateFor(gui::g_state.analysis.point);
                      if (st != gui::PointQueryState::kCompleted) {
                        return false;
                      }
                      LUMICE_PathPointInfo i{};
                      i.struct_size = sizeof(i);
                      return LUMICE_PathPointGetInfo(gui::g_state.analysis.point.adopted_result.get(), &i) ==
                                 LUMICE_PATH_READ_OK &&
                             i.wavelength_nm == 450.0f;
                    },
                    60),
                false);
  gui::g_state.analysis.map_wavelength_nm = 0.0f;
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [reference_lambda] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      if (m.adopted_result == nullptr || !(m.adopted_id == m.intent_id)) {
                        return false;
                      }
                      LUMICE_PathMapInfo i{};
                      i.struct_size = sizeof(i);
                      return LUMICE_PathMapGetInfo(m.adopted_result.get(), &i) == LUMICE_PATH_READ_OK &&
                             i.view_full_sky == 0 && i.wavelength_nm == reference_lambda;
                    },
                    60),
                false);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [reference_lambda] {
                      const gui::PointQueryState st = gui::PointQueryStateFor(gui::g_state.analysis.point);
                      if (st != gui::PointQueryState::kCompleted) {
                        return false;
                      }
                      LUMICE_PathPointInfo i{};
                      i.struct_size = sizeof(i);
                      return LUMICE_PathPointGetInfo(gui::g_state.analysis.point.adopted_result.get(), &i) ==
                                 LUMICE_PATH_READ_OK &&
                             i.wavelength_nm == reference_lambda;
                    },
                    60),
                false);
  ctx->LogInfo("[v1 rekey] the lambda knob re-asked map and point at 450, then the reference %.1f again",
               reference_lambda);
  return true;
}

// The new faces die and survive with the document (v1-scope E, the cross-face claim the
// workspace-switch leg predates): New clears every analysis session — survey, map, point, ray,
// the sky-point intent with them, the executor left idle; Revert keeps the adopted outcomes and
// the intent, re-issuing nothing. The halo scene, fixed-dt pool — same reasons as the rekey leg.
bool NewDocumentClearsEveryAnalyzeFace(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  // Arm every face: a selection (survey + map), a sky point (point), and let the ray follow.
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + TopChainDisplay()).c_str());
  ctx->SetRef("");
  SetSkyPointTarget(40.0, 0.0);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    60),
                false);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::PointQueryState st = gui::PointQueryStateFor(gui::g_state.analysis.point);
                      return st == gui::PointQueryState::kCompleted || st == gui::PointQueryState::kRefused;
                    },
                    60),
                false);
  const gui::SurveyHandle survey_before = gui::g_state.analysis.survey.adopted_result;
  const gui::MapHandle map_before = gui::g_state.analysis.map.adopted_result;
  const gui::PointHandle point_before = gui::g_state.analysis.point.adopted_result;
  IM_CHECK_RETV(survey_before != nullptr && map_before != nullptr, false);
  IM_CHECK_RETV(point_before != nullptr || !gui::g_state.analysis.point.adopted_error.empty(), false);

  // New: the whole analysis session dies with the document — every face's bookkeeping included —
  // and the executor's binding is released on the worker thread.
  gui::DoNew();
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.analysis.survey.intent_valid == false, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::MapQueryStateFor(gui::g_state.analysis.map) == gui::MapQueryState::kNoSelection, false);
  IM_CHECK_RETV(gui::g_state.analysis.map.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kNoIntent, false);
  IM_CHECK_RETV(gui::g_state.analysis.point.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::g_state.analysis.trace.intent_valid == false, false);
  IM_CHECK_RETV(gui::g_state.analysis.trace.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid == false, false);
  IM_CHECK_RETV(!gui::g_survey_executor.InFlight(), false);
  ctx->LogInfo("[v1 new] New cleared survey, map, point, ray and the sky point; the executor is idle");

  // Revert keeps: rebuild the faces, then revert — every adopted outcome and the intent stay,
  // nothing re-issues (the analysis-tired per-reason rules, now across all four faces).
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + TopChainDisplay()).c_str());
  ctx->SetRef("");
  SetSkyPointTarget(40.0, 0.0);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    60),
                false);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::PointQueryState st = gui::PointQueryStateFor(gui::g_state.analysis.point);
                      return st == gui::PointQueryState::kCompleted || st == gui::PointQueryState::kRefused;
                    },
                    60),
                false);
  const gui::SurveyHandle survey_kept = gui::g_state.analysis.survey.adopted_result;
  const gui::MapHandle map_kept = gui::g_state.analysis.map.adopted_result;
  const gui::PointHandle point_kept = gui::g_state.analysis.point.adopted_result;
  // The ray face's precondition: the keep-assertion only carries meaning over an ARMED trace.
  // When the point document carries components, drive the trace to its adopted state and
  // capture the handle BY VALUE (a raw .get() address could be reused after a free — the
  // handle copy owns). When it carries none (the mass/refused shapes), the trace never arms:
  // the assertion degrades, explicitly, to "Revert keeps it un-armed".
  bool trace_armed = false;
  bool trace_intent_kept = false;
  gui::RepresentativeRayHandle trace_result_kept;
  {
    LUMICE_PathPointInfo pt_info{};
    pt_info.struct_size = sizeof(pt_info);
    const bool has_components =
        gui::g_state.analysis.point.adopted_result != nullptr &&
        LUMICE_PathPointGetInfo(gui::g_state.analysis.point.adopted_result.get(), &pt_info) == LUMICE_PATH_READ_OK &&
        pt_info.outcome == LUMICE_PATH_POINT_DISCOVERED && pt_info.component_count > 0;
    if (has_components) {
      IM_CHECK_RETV(DriveUntil(ctx, [] { return gui::g_state.analysis.trace.adopted_result != nullptr; }, 30), false);
      trace_armed = true;
    }
    trace_intent_kept = gui::g_state.analysis.trace.intent_valid;
    trace_result_kept = gui::g_state.analysis.trace.adopted_result;
    if (trace_armed) {
      IM_CHECK_RETV(trace_intent_kept && trace_result_kept != nullptr, false);
    } else {
      IM_CHECK_RETV(trace_intent_kept == false && trace_result_kept == nullptr, false);
      ctx->LogInfo(
          "[v1 revert] the point document has no components: the ray face stays un-armed, "
          "and the keep-assertion degrades to keeping it so");
    }
  }
  IM_CHECK_RETV(survey_kept != nullptr && map_kept != nullptr, false);
  gui::DoRevert();
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.analysis.survey.intent_valid == true, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result == survey_kept, false);
  IM_CHECK_RETV(gui::g_state.analysis.map.adopted_result == map_kept, false);
  IM_CHECK_RETV(gui::g_state.analysis.point.adopted_result == point_kept, false);
  IM_CHECK_RETV(gui::g_state.analysis.trace.intent_valid == trace_intent_kept, false);
  IM_CHECK_RETV(gui::g_state.analysis.trace.adopted_result == trace_result_kept, false);
  if (trace_armed) {
    ctx->LogInfo("[v1 revert] the armed ray face survived Revert by handle value");
  }
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid, false);
  IM_CHECK_RETV(!gui::g_survey_executor.InFlight(), false);
  ctx->LogInfo("[v1 revert] Revert kept every face's adopted outcome and the sky point; nothing re-issued");
  return true;
}

// The identity class (C15's `1-3-6-2`, every member rank-0) across all faces at once: the
// overview routes the identity arm's honest statement, the map still answers (the rank-0
// point-mass arm, m±err — not a refused or empty picture), and the point query at the sun's own
// image answers the MASS arm — no component rows, no ray intent, no fabricated fiber. The one
// session where "direction identity" is the answer, nothing else degrades with it (AC2's
// 不整页封锁 on this scene's own identity family). Fixed-dt pool: the rank-0 survey
// early-stops (certified, no chromatic legs) and the mass arm answers in milliseconds.
bool IdentityClassShowsTheMassArmAndTheMapAnswers(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  std::string c15_display;
  for (const int idx : gui::g_state.analysis_result.display_order) {
    const auto& e = gui::g_state.analysis_result.payload->entries[static_cast<size_t>(idx)];
    if (std::strcmp(e.display, "1-3-6-2") == 0) {
      c15_display = e.display;
      break;
    }
  }
  IM_CHECK_RETV(!c15_display.empty(), false);
  const std::string c15_label = gui::AnalysisRowLabel(c15_display, gui::AnalysisCoreToPoolMap(gui::g_state));
  SetListRef(ctx);
  const ImGuiID c15_row = FindListRowScrolling(ctx, c15_label);
  IM_CHECK_RETV(c15_row != 0, false);
  ctx->ItemClick(c15_row);
  ctx->SetRef("");
  const std::optional<gui::SelectedClassIdentity> identity = gui::SelectedClassIdentityFor(gui::g_state);
  IM_CHECK_RETV(identity.has_value(), false);

  // The overview's identity arm (all twelve members rank-0, certified) — the honest "direction
  // identity" statement, not a fabricated dispersion or fiber story.
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    60),
                false);
  {
    const gui::SurveyOverviewModel model = gui::BuildSurveyOverviewModel(*gui::g_state.analysis.survey.adopted_result);
    IM_CHECK_RETV(model.valid && model.arm == gui::SurveyArm::kIdentity, false);
    ctx->LogInfo("[v1 c15] overview arm identity, valid %d", model.valid ? 1 : 0);
  }
  // The map answers under the same selection (the rank-0 point-mass arm) and names the class.
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      return m.adopted_result != nullptr && (m.adopted_id == m.intent_id);
                    },
                    60),
                false);
  {
    LUMICE_PathMapInfo info{};
    info.struct_size = sizeof(info);
    IM_CHECK_RETV(LUMICE_PathMapGetInfo(gui::g_state.analysis.map.adopted_result.get(), &info) == LUMICE_PATH_READ_OK,
                  false);
    IM_CHECK_RETV(info.crystal_id == identity->core_crystal_id, false);
    IM_CHECK_RETV(info.kept_count > 0, false);
    ctx->LogInfo("[v1 c15] map answered: kept %lld, lambda %.1f", info.kept_count, info.wavelength_nm);
  }
  // The point query at the sun's own image (the rank-0 point): the MASS arm's named lines, no
  // component rows — and the ray face stays silent (the pump's guard never issues a trace for
  // the mass document; nothing fabricated replaces the fiber).
  SetSkyPointTarget(20.0, 0.0);
  ctx->Yield(2);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  {
    LUMICE_PathPointInfo info{};
    info.struct_size = sizeof(info);
    IM_CHECK_RETV(
        LUMICE_PathPointGetInfo(gui::g_state.analysis.point.adopted_result.get(), &info) == LUMICE_PATH_READ_OK, false);
    IM_CHECK_RETV(info.crystal_id == identity->core_crystal_id, false);
    IM_CHECK_RETV(info.outcome == LUMICE_PATH_POINT_MASS, false);
    std::vector<gui::PointComponentRow> rows;
    IM_CHECK_RETV(gui::PointComponentRows(*gui::g_state.analysis.point.adopted_result, &rows) == 0, false);
    LUMICE_PathPointMass mass{};
    mass.struct_size = sizeof(mass);
    IM_CHECK_RETV(
        LUMICE_PathPointGetMass(gui::g_state.analysis.point.adopted_result.get(), &mass) == LUMICE_PATH_READ_OK, false);
    const std::vector<std::string> mass_lines = gui::PointMassLines(mass);
    IM_CHECK_RETV(!mass_lines.empty(), false);
    IM_CHECK_RETV(gui::g_state.analysis.trace.intent_valid == false, false);
    IM_CHECK_RETV(gui::g_state.analysis.trace.adopted_result == nullptr, false);
    ctx->LogInfo("[v1 c15] point MASS at the sun image: %zu named line(s), no component, no trace intent",
                 mass_lines.size());
  }
  return true;
}

// The refused arm beside its independent facts (AC2's first sentence, the C17 shape):
// `3-1-5-7` — the corpus's open-arc family whose honest boundary IS the partition refusal —
// routes its members fine but refuses the delta axis's structural objects
// (`slab_crease_touching`), and the overview answers the refusal arm: the named refusal, the
// independent witness and the TIR share beside it, the chromatic block named unevaluated —
// only THAT item missing, everything else alive. Real-timing pool: this survey's witness/TIR
// tail is not bounded by the cooperative deadline (the as-built's escalation).
bool RefusedClassNamesTheRefusalAndKeepsTheWitness(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  const std::optional<gui::SelectedClassIdentity> identity = SelectRowByDisplay(ctx, "3-1-5-7");
  IM_CHECK_RETV(identity.has_value(), false);

  // The overview answers COMPLETED — the refusal is the document's arm, not a failed query —
  // with the refusal named, the witness and TIR alive beside it, and the chromatic block's
  // absence named rather than zeroed.
  // The budget follows the as-built's own observation band for this survey (40-100 s wall, the
  // witness/TIR tail outliving the cooperative deadline) plus margin — 60 s sat BELOW the band.
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    240),
                false);
  {
    const gui::SurveyOverviewModel model = gui::BuildSurveyOverviewModel(*gui::g_state.analysis.survey.adopted_result);
    IM_CHECK_RETV(model.valid && model.arm == gui::SurveyArm::kRefused, false);
    IM_CHECK_RETV(!model.evidence_mode.empty(), false);
    IM_CHECK_RETV(model.witness.available && model.witness.value_available, false);
    IM_CHECK_RETV(model.witness.kept_count > 0, false);
    IM_CHECK_RETV(model.tir.available, false);
    IM_CHECK_RETV(model.dispersion.available == false, false);
    IM_CHECK_RETV(!model.chromatic_note.empty(), false);
    ctx->LogInfo("[v1 c17] refused arm '%s': witness kept %lld, tir available, chromatic named '%s'",
                 model.evidence_mode.c_str(), model.witness.kept_count, model.chromatic_note.c_str());
  }
  IM_CHECK_RETV(SaveWindowPng(ctx, "//##AnalyzeRightPanel", GuiTestTempPath("analyze_v1_c17_refused.png").string()),
                false);
  return true;
}

// The independence half of the C17 claim, in its own session (each half short enough for the
// pool-context watchdog): a refused delta axis blockades neither the map face nor a point
// query on the same class — the map answers, and a sky-point query discovers (the corpus
// band, dev ~120 deg). Real-timing pool, same survey tail as the names leg.
bool RefusedClassBlockadesNothing(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  const std::optional<gui::SelectedClassIdentity> identity = SelectRowByDisplay(ctx, "3-1-5-7");
  IM_CHECK_RETV(identity.has_value(), false);
  // The budget follows the as-built's own observation band for this survey (40-100 s wall)
  // plus margin — 60 s sat BELOW the band.
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    240),
                false);

  // The map answers under the same selection (a refused delta axis does not blockade the map
  // face) and names the class — the FIRST adopted map is the evidence (the full-sky arrival
  // already proves the face answers; waiting for the refinement chain would buy nothing here
  // and is the pool-context watchdog budget's long pole).
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::MapQuerySession& m = gui::g_state.analysis.map;
                      return m.adopted_result != nullptr && (m.adopted_id == m.intent_id);
                    },
                    60),
                false);
  {
    LUMICE_PathMapInfo info{};
    info.struct_size = sizeof(info);
    IM_CHECK_RETV(LUMICE_PathMapGetInfo(gui::g_state.analysis.map.adopted_result.get(), &info) == LUMICE_PATH_READ_OK,
                  false);
    IM_CHECK_RETV(info.crystal_id == identity->core_crystal_id, false);
    IM_CHECK_RETV(info.kept_count > 0, false);
    ctx->LogInfo("[v1 c17] map answered: kept %lld", info.kept_count);
  }
  // A point query on the refused class still discovers — the corpus band target, the same
  // anchor the internal-reflection leg pins (probe: one component on this exact geometry).
  SetSkyPointTarget(38.0, 180.0);
  ctx->Yield(2);
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCompleted; },
          60),
      false);
  {
    LUMICE_PathPointInfo info{};
    info.struct_size = sizeof(info);
    IM_CHECK_RETV(
        LUMICE_PathPointGetInfo(gui::g_state.analysis.point.adopted_result.get(), &info) == LUMICE_PATH_READ_OK, false);
    IM_CHECK_RETV(info.crystal_id == identity->core_crystal_id, false);
    IM_CHECK_RETV(info.outcome == LUMICE_PATH_POINT_DISCOVERED, false);
    std::vector<gui::PointComponentRow> rows;
    const int components = gui::PointComponentRows(*gui::g_state.analysis.point.adopted_result, &rows);
    IM_CHECK_RETV(components >= 1, false);
    ctx->LogInfo("[v1 c17] point query independent of the survey refusal: discovered, %d component(s)", components);
  }
  return true;
}

// The document rules the per-slice legs leave out, part one (v1-scope E / AC3): editing the
// scene leaves the adopted outcomes in place (explicit accept — nothing clears or re-asks
// behind the user's back) and the next Analyze replaces them under the new archive. The halo
// scene, fixed-dt pool — the lifecycle legs' reasons; one survey's in-flight window each.
bool SceneEditFollowsTheExplicitAcceptContract(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  // Edit while a query is IN FLIGHT: the selection issued under the current archive keeps
  // running on the frozen snapshot and adopts — the edit kills nothing.
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + TopChainDisplay()).c_str());
  ctx->SetRef("");
  gui::g_state.crystals[0].height = 0.1f;
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    60),
                false);
  const gui::SurveyHandle in_flight_era = gui::g_state.analysis.survey.adopted_result;
  IM_CHECK_RETV(in_flight_era != nullptr, false);
  ctx->LogInfo("[v1 edit] the in-flight query adopted on its frozen snapshot across the edit");

  // Edit while ADOPTED: the outcomes stay adopted (explicit accept — the edit is not an update).
  gui::g_state.crystals[0].height = 1.2f;
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result == in_flight_era, false);
  IM_CHECK_RETV(gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) == gui::SurveyOverviewState::kCompleted,
                false);
  ctx->LogInfo("[v1 edit] the adopted outcome survives a document edit untouched");

  // The explicit update: re-Analyze clears the selection and voids the old generation's
  // bookkeeping (the digest leg's documented R6 shape — no adopted outcome survives into the
  // next generation's region), and re-selecting answers under the new archive.
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  ctx->Yield(2);
  IM_CHECK_RETV(gui::g_state.analysis.survey.intent_valid == false, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result == nullptr, false);
  ctx->LogInfo("[v1 edit] re-Analyze voided the old generation's selection and outcome");
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + TopChainDisplay()).c_str());
  ctx->SetRef("");
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [&era = in_flight_era] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                                 gui::SurveyOverviewState::kCompleted &&
                             gui::g_state.analysis.survey.adopted_result != era;
                    },
                    60),
                false);
  ctx->LogInfo("[v1 edit] re-selecting answered under the new archive's generation");
  return true;
}

// Part two: the Cancel survey / Cancel point buttons withdraw their own faces to the named
// cancelled states with the latch, a changed selection re-arms, and a document switch (the
// open path's mechanism) clears every face. The halo scene's seconds-scale survey provides the
// in-flight windows; fixed-dt pool.
bool CancelSurveyButtonReachesTheNamedState(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  // The fully random scene, deliberately: its dominant class routes full-dimensional, so the
  // survey's in-flight window is seconds wide (the halo scene's dominant class routes to the
  // fast refused arm — its kWaiting window is a click-race) and a cancel during the delta
  // phase posts back without waiting on the chromatic legs.
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  // The Cancel survey button: withdraw while the full-dimensional survey runs its delta phase,
  // and the named cancelled state latches until a new selection.
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + TopChainDisplay()).c_str());
  ctx->SetRef("");
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kWaiting;
                    },
                    20),
                false);
  ctx->ItemClick("**/Cancel survey");
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCancelled;
                    },
                    45),
                false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.cancelled_for_key, false);
  // The state flips when the pump adopts the cancellation; the worker clears its own in-flight
  // bookkeeping a frame or two later — wait for it bounded rather than asserting the race.
  IM_CHECK_RETV(DriveUntil(ctx, [] { return !gui::g_survey_executor.InFlight(); }, 20), false);
  ctx->LogInfo("[v1 cancel] Cancel survey withdrew the intent under the latch; the executor is idle");
  // A changed selection clears the latch and re-arms — the probe-verified fast identity chain
  // (`1-3-6-2` early-stops on this scene, no chromatic legs) proves the re-arm without making
  // the pool-context wall-clock budget the leg's long pole.
  IM_CHECK_RETV(SelectRowByDisplay(ctx, "1-3-6-2").has_value(), false);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    60),
                false);
  ctx->LogInfo("[v1 cancel] a changed selection cleared the latch and answered");
  return true;
}

// Part two: the Cancel point button while the point is QUEUED behind the dominant class's
// fresh survey (the wide window makes the queued state deterministic) — the withdraw names the
// point's cancelled state and the survey face is not superseded — and a document switch (the
// open path's mechanism) clears every face.
bool CancelPointButtonReachesTheNamedState(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpScene(ctx, kFullyRandomJson, /*infinite=*/false, /*gpu=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);

  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + TopChainDisplay()).c_str());
  ctx->SetRef("");
  SetSkyPointTarget(40.0, 0.0);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                                 gui::SurveyOverviewState::kWaiting &&
                             gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kWaiting;
                    },
                    20),
                false);
  ctx->ItemClick("**/Cancel point");
  IM_CHECK_RETV(
      DriveUntil(
          ctx, [] { return gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kCancelled; },
          20),
      false);
  IM_CHECK_RETV(gui::g_state.analysis.point.cancelled_for_key, false);
  IM_CHECK_RETV(gui::g_state.analysis.trace.intent_valid == false, false);
  IM_CHECK_RETV(gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) == gui::SurveyOverviewState::kWaiting, false);
  ctx->LogInfo("[v1 cancel] Cancel point withdrew the queued intent; the survey face kept its own wait");
  return true;
}

// A document switch — the open path's mechanism (the native dialog aside, the in-app boundary
// IS the deserialize) — clears every analysis face and the intent with them, with the executor
// left idle. The faces are settled before the switch (an in-flight query's withdrawal tail is
// the engine's cooperative-stop shape, the as-built's own escalation — not this leg's claim).
// The halo scene's fast refused-arm survey keeps the leg inside the fixed-dt pool's budget.
bool DocumentOpenClearsEveryAnalyzeFace(ImGuiTestContext* ctx) {
  ScopedServerGuard guard;
  IM_CHECK_RETV(BringUpHaloScene(ctx, /*infinite=*/false), false);
  OpenWindow(ctx);
  IM_CHECK_RETV(RunWholeSkyAnalysisToCompletion(ctx, nullptr), false);
  SetListRef(ctx);
  ctx->ItemClick((std::string("**/") + TopChainDisplay()).c_str());
  ctx->SetRef("");
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      return gui::SurveyOverviewStateFor(gui::g_state.analysis.survey) ==
                             gui::SurveyOverviewState::kCompleted;
                    },
                    60),
                false);
  SetSkyPointTarget(40.0, 0.0);
  ctx->Yield(2);
  IM_CHECK_RETV(DriveUntil(
                    ctx,
                    [] {
                      const gui::PointQueryState st = gui::PointQueryStateFor(gui::g_state.analysis.point);
                      return st == gui::PointQueryState::kCompleted || st == gui::PointQueryState::kRefused;
                    },
                    60),
                false);
  IM_CHECK_RETV(DriveUntil(ctx, [] { return !gui::g_survey_executor.InFlight(); }, 20), false);

  IM_CHECK_RETV(gui::DeserializeFromJson(kHalo22Json, gui::g_state), false);
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.analysis.survey.intent_valid == false, false);
  IM_CHECK_RETV(gui::g_state.analysis.survey.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::MapQueryStateFor(gui::g_state.analysis.map) == gui::MapQueryState::kNoSelection, false);
  IM_CHECK_RETV(gui::g_state.analysis.map.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::PointQueryStateFor(gui::g_state.analysis.point) == gui::PointQueryState::kNoIntent, false);
  IM_CHECK_RETV(gui::g_state.analysis.point.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::g_state.analysis.trace.intent_valid == false, false);
  IM_CHECK_RETV(gui::g_state.analysis.trace.adopted_result == nullptr, false);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid == false, false);
  IM_CHECK_RETV(!gui::g_survey_executor.InFlight(), false);
  ctx->LogInfo("[v1 open] a document switch cleared survey, map, point, ray and the sky point");
  return true;
}

}  // namespace

void RegisterRaypathAnalysisPanelTests(ImGuiTestEngine* engine) {
  // AC2, first case: open -> Point -> click the preview -> the centre is the click's unprojection
  // -> Analyze -> the list is non-empty and its first row carries the largest energy.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "point_pick_analyze_lists_chains");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      IM_CHECK_EQ(gui::g_state.workspace.mode, gui::GuiState::WorkspaceMode::kAnalyze);
      // The ROI controls live on the panorama side.
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick("Point");
      ctx->Yield(2);
      IM_CHECK_EQ(gui::g_state.analysis.roi_mode, LUMICE_RAYPATH_ROI_CONE);
      ctx->ItemClick(kPickButton);
      ctx->Yield(1);
      IM_CHECK(gui::g_state.analysis.pick_armed);
      ctx->SetRef("");

      const gui::CanvasPixel px = PickPanoramaCentre(ctx);
      // The ROI is placed: the pick is consumed, the centre is valid, and this frame's marker
      // projects back onto the pixel that was clicked.
      IM_CHECK(!gui::g_state.analysis.pick_armed);
      IM_CHECK(gui::g_state.analysis.cone_center_valid);
      const LUMICE_AnnotationView view =
          gui::PanoramaAnnotationView(gui::g_state, gui::g_panorama_vp.vp_w, gui::g_panorama_vp.vp_h);
      const std::optional<gui::CanvasPixel> marker = gui::ProjectConeCenterMarker(gui::g_state, view);
      IM_CHECK(marker.has_value());
      IM_CHECK_EQ(marker->px, px.px);
      IM_CHECK_EQ(marker->py, px.py);
      // And its direction is the click's unprojection under the view the picture is drawn with —
      // the oracle is the C API called directly on the same pixel, not the panel's own function.
      float want[3] = { 0.0f, 0.0f, 0.0f };
      int valid = 0;
      IM_CHECK_EQ(LUMICE_UnprojectPixel(&view, px.px, px.py, want, &valid), LUMICE_OK);
      IM_CHECK_EQ(valid, 1);
      IM_CHECK_FLOAT_NEAR(gui::g_state.analysis.cone_center_dir[0], want[0], 1e-6f);
      IM_CHECK_FLOAT_NEAR(gui::g_state.analysis.cone_center_dir[1], want[1], 1e-6f);
      IM_CHECK_FLOAT_NEAR(gui::g_state.analysis.cone_center_dir[2], want[2], 1e-6f);
      // Upper-sky light travels downward (z < 0) — the direction is on the panorama's sky. The
      // equal-area closed form is pinned in gui_unit (PanoramaUnprojectIsEqualAreaOverTheDisk);
      // the oracle here is the C API unprojection checked above, pixel for pixel.
      IM_CHECK_LT(gui::g_state.analysis.cone_center_dir[2], 0.0f);
      // The ring is drawable: the local scale at the click resolves.
      IM_CHECK(gui::ConeRingRadiusCanvasPx(view, px.px, px.py, gui::g_state.analysis.cone_radius_deg).has_value());

      // Analyze, through the button.
      LUMICE_SimLifecycleResult before{};
      LUMICE_GetSimLifecycle(gui::g_server, &before);
      SetListRef(ctx);
      IM_CHECK(!IsDisabled(ctx->ItemInfo(kAnalyzeButton)));
      ctx->ItemClick(kAnalyzeButton);
      ctx->SetRef("");
      IM_CHECK(gui::g_state.analysis.started);
      IM_CHECK(DriveUntil(
          ctx, [] { return !gui::g_state.analysis_run_in_progress && gui::g_state.analysis_result.payload != nullptr; },
          60));
      const auto& view_result = gui::g_state.analysis_result;
      IM_CHECK_EQ(view_result.payload->roi_mode, LUMICE_RAYPATH_ROI_CONE);
      IM_CHECK_GT(view_result.payload->entries.size(), 0u);
      IM_CHECK_EQ(view_result.display_order.size(), view_result.payload->entries.size());
      // The share-list pins at the 1280x800 acceptance size: with
      // the result in, the window itself must not scroll — the controls above the table are the
      // fixed header and are visible by construction (ScrollMax == 0 says the whole content
      // fits), and the one scrolling region under the pointer is the table's own inner window.
      // The listed basis, not smaller: the plan's §9.2 boundary leaves the 1024x640 minimum
      // outside the guarantee domain (the controls' information layout, not the scroll
      // structure, would be the thing to redesign there).
      {
        const ScopedWindowBasis window_basis(ctx);
        g_window_size_test.width = 1280;
        g_window_size_test.height = 800;
        g_window_size_test.done.store(false);
        g_window_size_test.requested.store(true);
        for (int i = 0; i < 20 && !g_window_size_test.done.load(); ++i) {
          ctx->Yield(1);
        }
        IM_CHECK(g_window_size_test.done.load());
        ctx->Yield(3);
        ImGuiWindow* share = ctx->GetWindowByRef(kWindowRef);
        IM_CHECK(share != nullptr);
        IM_CHECK_EQ(share->ScrollMax.y, 0.0f);
        IM_CHECK_EQ(share->Scroll.y, 0.0f);
        ImGuiWindow* table_child = nullptr;
        for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
          if (w->ParentWindow == share && std::strstr(w->Name, "##analysis_rows") != nullptr) {
            table_child = w;
            break;
          }
        }
        IM_CHECK(table_child != nullptr);
        // The fixed header is on screen: the search box resolves with a real clipped rect
        // (a control scrolled out of the window resolves too, but carries no visible height).
        const ImGuiTestItemInfo search = ctx->ItemInfo("**/###analysis_search", ImGuiTestOpFlags_NoError);
        IM_CHECK(search.ID != 0);
        IM_CHECK_GT(search.RectClipped.GetHeight(), 0.0f);
        // The floor's red anchor: squeezing the upper row (the same write the drag performs)
        // into the band where the result table's old 120pt floor stops fitting — the window
        // must STILL not scroll (the table yields instead). Moderate: the controls' own fit
        // stays inside the §9.2 boundary, well above the 0.15 clamp. The grab drag itself is
        // verified right after (see the point leg's squeeze anchor for the same shape).
        const float ratio_before = gui::g_state.workspace.middle_rows_ratio;
        gui::g_state.workspace.middle_rows_ratio = 0.32f;
        ctx->Yield(3);
        ctx->LogInfo("[scroll pins] share squeezed SM %.1f SC %.1f, ratio %.2f", share->ScrollMax.y, share->Scroll.y,
                     gui::g_state.workspace.middle_rows_ratio);
        IM_CHECK_EQ(share->ScrollMax.y, 0.0f);
        IM_CHECK_EQ(share->Scroll.y, 0.0f);
        gui::g_state.workspace.middle_rows_ratio = ratio_before;
        ctx->Yield(3);
        // The grab gesture over the real mouse path: drag the bar down, the ratio follows.
        ImGuiWindow* divider = ctx->GetWindowByRef("//##AnalyzeRowsDivider");
        IM_CHECK(divider != nullptr);
        const ImGuiTestItemInfo grab = ctx->ItemInfo("**/##AnalyzeRowsSplitter", ImGuiTestOpFlags_NoError);
        IM_CHECK(grab.ID != 0);
        const ImVec2 centre = grab.RectFull.GetCenter();
        ctx->MouseMoveToPos(centre);
        ctx->Yield(2);
        ctx->MouseDown(0);
        ctx->Yield(2);
        ctx->MouseMoveToPos(ImVec2(centre.x, centre.y + 120.0f));
        ctx->Yield(3);
        ctx->MouseUp(0);
        ctx->Yield(2);
        IM_CHECK_GT(gui::g_state.workspace.middle_rows_ratio, ratio_before + 0.05f);  // the drag wrote it
        gui::g_state.workspace.middle_rows_ratio = ratio_before;
        ctx->Yield(2);
        ctx->LogInfo("[scroll pins] share@1280x800 SM %.1f SC %.1f, table %s", share->ScrollMax.y, share->Scroll.y,
                     table_child->Name);
      }
      // First row = the largest displayed energy of all rows.
      const double first = view_result.display_energy[static_cast<size_t>(view_result.display_order[0])];
      IM_CHECK_GT(first, 0.0);
      const double largest = *std::max_element(view_result.display_energy.begin(), view_result.display_energy.end());
      IM_CHECK_EQ(first, largest);
      // The top row's display text is the chain grammar itself (the same bytes the CSV writes):
      // which chain dominates depends on where on the sky the panorama pick landed, so the case
      // pins the grammar and the ordering, not one scene-specific chain identity.
      IM_CHECK(view_result.payload->entries[static_cast<size_t>(view_result.display_order[0])].display[0] != '\0');
      // The analysis is a submission of its own (v4.36): the server's epoch moved past the
      // render's — and the picture on screen is still the render's, because the GUI's own
      // committed_epoch (the one ReconcileSimState keys on) did not: sim_state stayed kDone.
      LUMICE_SimLifecycleResult after{};
      LUMICE_GetSimLifecycle(gui::g_server, &after);
      IM_CHECK_GT(after.epoch, before.epoch);
      IM_CHECK_EQ(gui::g_state.committed_epoch, before.epoch);
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)SimState::kDone);
    };
  }

  // AC2, second case: a render in progress keeps Analyze shut, and the other direction — an
  // analysis in progress keeps Run shut on the top bar — with the window's own Stop ending it.
  // The scene is an INFINITE run on purpose: a whole-sky analysis of it has no end of its own
  // (lumice_engine.h: an infinite budget runs until LUMICE_StopServer), so the in-progress state is
  // held open for as long as the assertions need, and Stop is the only way out — which is the
  // path this case exists to drive.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "analyze_and_run_exclude_each_other");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/true));
      OpenWindow(ctx);
      SetListRef(ctx);
      IM_CHECK(IsDisabled(ctx->ItemInfo(kAnalyzeButton)));
      IM_CHECK(!gui::CanStartAnalysis(true, gui::g_state.sim_state, gui::g_state.analysis_run_in_progress,
                                      gui::g_state.run_intent));
      // A click on it does nothing: no intent, no run.
      ctx->ItemClick(kAnalyzeButton);
      ctx->Yield(2);
      IM_CHECK(!gui::g_state.analysis.started);
      ctx->SetRef("");

      // Stop the render; Analyze opens up; start it; Run on the top bar is now the one shut.
      gui::DoStop();
      IM_CHECK(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kDone; }, 20));
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick("Whole sky");
      ctx->SetRef(ListWindowRef());
      SetListRef(ctx);
      IM_CHECK(!IsDisabled(ctx->ItemInfo(kAnalyzeButton)));
      SetListRef(ctx);
      ctx->ItemClick(kAnalyzeButton);
      ctx->SetRef("");
      IM_CHECK(gui::g_state.analysis.started);
      IM_CHECK(gui::g_state.analysis_run_in_progress);
      ctx->Yield(2);
      IM_CHECK(gui::g_state.analysis_run_in_progress);
      IM_CHECK(IsDisabled(ctx->ItemInfo("##TopBar/" ICON_FA_PLAY " Run")));
      IM_CHECK(gui::IsBackendBusy(gui::g_state.sim_state, gui::g_state.analysis_run_in_progress));
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)SimState::kDone);  // the picture is still the render's
      // Partial results reach the list while the run is in progress.
      IM_CHECK(DriveUntil(ctx, [] { return gui::g_state.analysis_result.payload != nullptr; }, 20));
      IM_CHECK_EQ(gui::g_state.analysis_result.payload->roi_mode, LUMICE_RAYPATH_ROI_FULL_SKY);

      // Stop from the window: the intent is withdrawn at the command, the top bar reopens, and
      // the result stays on show — the one adopted so far, or the run's final snapshot if the
      // stop drained one more (a newer generation, never an older one, and never nothing).
      // Not pointer equality: the entries are read on this thread per (generation, symmetry)
      // and every read publishes a new payload object for the same result.
      const auto partial = gui::g_state.analysis_result.payload;
      SetListRef(ctx);
      ctx->ItemClick(ICON_FA_STOP " Stop");
      ctx->SetRef("");
      IM_CHECK(!gui::g_state.analysis.started);
      IM_CHECK(DriveUntil(
          ctx, [] { return !gui::g_state.analysis_run_in_progress && gui::g_state.sim_state == SimState::kDone; }, 20));
      IM_CHECK(!IsDisabled(ctx->ItemInfo("##TopBar/" ICON_FA_PLAY " Run")));
      IM_CHECK(gui::g_state.analysis_result.payload != nullptr);
      IM_CHECK_GE(gui::g_state.analysis_result.payload->snapshot_generation, partial->snapshot_generation);
      IM_CHECK_EQ(gui::g_state.analysis_result.payload->roi_mode, LUMICE_RAYPATH_ROI_FULL_SKY);
      SetListRef(ctx);
      IM_CHECK(!IsDisabled(ctx->ItemInfo(kAnalyzeButton)));
      ctx->SetRef("");
    };
  }

  // AC3: the radius slider is display-time. Setting it changes the energies on the list and
  // starts no run — the lifecycle epoch, the in-progress flag and the result object are all the
  // same before and after, only the projection over its rings changed.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "radius_slider_is_display_time");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      IM_CHECK(RunPointAnalysisToCompletion(ctx));
      const auto payload_before = gui::g_state.analysis_result.payload;
      const double total_before = gui::g_state.analysis_result.display_total;
      const int rings_before = gui::g_state.analysis_result.display_ring_count;
      LUMICE_SimLifecycleResult before{};
      LUMICE_GetSimLifecycle(gui::g_server, &before);
      const unsigned long long uploads_before = gui::g_state.texture_upload_count;

      SetListRef(ctx);
      ctx->ItemInputValue("Radius", 10.0f);
      ctx->SetRef("");
      ctx->Yield(5);

      IM_CHECK_FLOAT_NEAR(gui::g_state.analysis.cone_radius_deg, 10.0f, 1e-3f);
      IM_CHECK_GT(gui::g_state.analysis_result.display_ring_count, rings_before);
      // More rings, more of the cone's energy on the list — strictly, since the halo ring runs
      // through the cone and rays land at every distance from its centre.
      IM_CHECK_GT(gui::g_state.analysis_result.display_total, total_before);
      // And nothing ran: same result object, same epoch, no in-progress edge, no upload.
      IM_CHECK(gui::g_state.analysis_result.payload == payload_before);
      IM_CHECK(!gui::g_state.analysis_run_in_progress);
      LUMICE_SimLifecycleResult after{};
      LUMICE_GetSimLifecycle(gui::g_server, &after);
      IM_CHECK_EQ(after.epoch, before.epoch);
      IM_CHECK_EQ(after.lifecycle, before.lifecycle);
      IM_CHECK_EQ(gui::g_state.texture_upload_count, uploads_before);
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)SimState::kDone);
    };
  }

  // The P/B/D checkboxes are display-time (v4.33): the run recorded every chain unreduced, and a
  // toggle re-reads the result on hand under the new bits — on this thread, in the frame of the
  // click — starting no run. On the 22-degree halo the prism's D symmetry is what folds the
  // mirror-image path 3-7 into 3-5, so turning D off splits the top row in two: more rows, less
  // energy on "3-5", the same total; and the selection, which names the chain, survives because
  // "3-5" is still a row. Turning D back on merges them again.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "symmetry_checkboxes_are_display_time");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      IM_CHECK(RunPointAnalysisToCompletion(ctx));
      auto sum_energy = [] {
        double s = 0.0;
        for (const auto& e : gui::g_state.analysis_result.payload->entries) {
          s += e.energy;
        }
        return s;
      };
      auto row = [](const char* display) -> const LUMICE_RaypathHistogramEntry* {
        for (const auto& e : gui::g_state.analysis_result.payload->entries) {
          if (std::strcmp(e.display, display) == 0) {
            return &e;
          }
        }
        return nullptr;
      };
      const int kAll = LUMICE_RAYPATH_SYMMETRY_P | LUMICE_RAYPATH_SYMMETRY_B | LUMICE_RAYPATH_SYMMETRY_D;
      IM_CHECK_EQ(static_cast<int>(gui::g_state.analysis_result.entries_symmetry), kAll);
      const size_t rows_before = gui::g_state.analysis_result.payload->entries.size();
      const unsigned long long gen_before = gui::g_state.analysis_result.payload->snapshot_generation;
      const double total_before = sum_energy();
      const LUMICE_RaypathHistogramEntry* top = row("3-5");
      IM_CHECK(top != nullptr);
      const double top_before = top->energy;
      IM_CHECK(row("3-7") == nullptr);  // folded into 3-5 by D
      LUMICE_SimLifecycleResult before{};
      LUMICE_GetSimLifecycle(gui::g_server, &before);
      const unsigned long long uploads_before = gui::g_state.texture_upload_count;

      // Select the top row, then turn D off.
      SetListRef(ctx);
      ctx->ItemClick("**/3-5");
      ctx->Yield(1);
      IM_CHECK(gui::g_state.analysis.selected_entry.has_value() && *gui::g_state.analysis.selected_entry == "3-5");
      ctx->ItemClick("**/D##analysis_symmetry");
      ctx->Yield(2);
      ctx->SetRef("");
      IM_CHECK(!gui::g_state.analysis.symmetry_d);
      IM_CHECK_EQ(static_cast<int>(gui::g_state.analysis_result.entries_symmetry),
                  LUMICE_RAYPATH_SYMMETRY_P | LUMICE_RAYPATH_SYMMETRY_B);
      IM_CHECK_EQ(gui::g_state.analysis_result.payload->snapshot_generation, gen_before);
      IM_CHECK_GT(gui::g_state.analysis_result.payload->entries.size(), rows_before);
      IM_CHECK(row("3-7") != nullptr);
      top = row("3-5");
      IM_CHECK(top != nullptr);
      IM_CHECK_LT(top->energy, top_before);
      IM_CHECK_FLOAT_NEAR(sum_energy(), total_before, 1e-9 * total_before);
      // The selection names the chain and "3-5" is still a row: kept, and Exclude still has it.
      IM_CHECK(gui::SelectedAnalysisEntry(gui::g_state) == top);
      // And nothing ran: same epoch, same lifecycle, no in-progress edge, no upload.
      IM_CHECK(!gui::g_state.analysis_run_in_progress);
      LUMICE_SimLifecycleResult after{};
      LUMICE_GetSimLifecycle(gui::g_server, &after);
      IM_CHECK_EQ(after.epoch, before.epoch);
      IM_CHECK_EQ(after.lifecycle, before.lifecycle);
      IM_CHECK_EQ(gui::g_state.texture_upload_count, uploads_before);
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)SimState::kDone);

      // D back on: the rows merge again, the same count and total as at first.
      SetListRef(ctx);
      ctx->ItemClick("**/D##analysis_symmetry");
      ctx->Yield(2);
      ctx->SetRef("");
      IM_CHECK(gui::g_state.analysis.symmetry_d);
      IM_CHECK_EQ(static_cast<int>(gui::g_state.analysis_result.entries_symmetry), kAll);
      IM_CHECK_EQ(gui::g_state.analysis_result.payload->entries.size(), rows_before);
      IM_CHECK(row("3-7") == nullptr);
      IM_CHECK_FLOAT_NEAR(sum_energy(), total_before, 1e-9 * total_before);
      top = row("3-5");
      IM_CHECK(top != nullptr);
      IM_CHECK_FLOAT_NEAR(top->energy, top_before, 1e-9 * top_before);
      IM_CHECK(gui::SelectedAnalysisEntry(gui::g_state) == top);
    };
  }

  // The radius slider is live BEFORE any result: in Point mode with the default centre and no
  // Analyze pressed, it is enabled, setting it changes the session's radius and the ring drawn on
  // the panorama (the pixel radius DrawPanoramaRoiRing draws from), and nothing runs — no result
  // appears, the intent flag stays clear, the lifecycle is the render's, the sim state is Done.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "radius_slider_drags_before_any_result");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick("Point");
      ctx->Yield(1);
      IM_CHECK(gui::g_state.analysis.cone_center_valid);
      IM_CHECK(!gui::g_state.analysis_result.payload);
      SetListRef(ctx);
      IM_CHECK(!IsDisabled(ctx->ItemInfo("Radius")));
      const LUMICE_AnnotationView view =
          gui::PanoramaAnnotationView(gui::g_state, gui::g_panorama_vp.vp_w, gui::g_panorama_vp.vp_h);
      const std::optional<gui::CanvasPixel> marker = gui::ProjectConeCenterMarker(gui::g_state, view);
      IM_CHECK(marker.has_value());
      const std::optional<float> ring_before =
          gui::ConeRingRadiusCanvasPx(view, marker->px, marker->py, gui::g_state.analysis.cone_radius_deg);
      IM_CHECK(ring_before.has_value());
      LUMICE_SimLifecycleResult before{};
      LUMICE_GetSimLifecycle(gui::g_server, &before);
      const unsigned long long uploads_before = gui::g_state.texture_upload_count;

      ctx->ItemInputValue("Radius", 10.0f);
      ctx->SetRef("");
      ctx->Yield(5);

      IM_CHECK_FLOAT_NEAR(gui::g_state.analysis.cone_radius_deg, 10.0f, 1e-3f);
      const std::optional<float> ring_after =
          gui::ConeRingRadiusCanvasPx(view, marker->px, marker->py, gui::g_state.analysis.cone_radius_deg);
      IM_CHECK(ring_after.has_value());
      IM_CHECK_GT(*ring_after, *ring_before);
      // And nothing ran.
      IM_CHECK(!gui::g_state.analysis.started);
      IM_CHECK(!gui::g_state.analysis_run_in_progress);
      IM_CHECK(!gui::g_state.analysis_result.payload);
      LUMICE_SimLifecycleResult after{};
      LUMICE_GetSimLifecycle(gui::g_server, &after);
      IM_CHECK_EQ(after.epoch, before.epoch);
      IM_CHECK_EQ(after.lifecycle, before.lifecycle);
      IM_CHECK_EQ(gui::g_state.texture_upload_count, uploads_before);
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)SimState::kDone);
    };
  }

  // The request's parameters are the panel's own session inputs: Rays(M) / Infinite rays open
  // with the document's values and then move independently of them; editing either starts
  // nothing and dirties nothing.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "request_params_are_session_inputs_seeded_once");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));  // sim.ray_num_millions = 0.1, infinite off
      IM_CHECK(!gui::g_state.analysis.ray_budget_initialized);
      OpenWindow(ctx);
      IM_CHECK(gui::g_state.analysis.ray_budget_initialized);
      IM_CHECK_FLOAT_NEAR(gui::g_state.analysis.ray_num_millions, gui::g_state.sim.ray_num_millions, 1e-6f);
      IM_CHECK_EQ(gui::g_state.analysis.infinite, gui::g_state.sim.infinite);
      LUMICE_SimLifecycleResult before{};
      LUMICE_GetSimLifecycle(gui::g_server, &before);
      const SimState sim_state_before = gui::g_state.sim_state;

      SetListRef(ctx);
      IM_CHECK(ctx->ItemInfo("##Rays(M)_input").ID != 0);
      ctx->ItemInputValue("##Rays(M)_input", 2.5f);
      ctx->Yield(2);
      IM_CHECK_FLOAT_NEAR(gui::g_state.analysis.ray_num_millions, 2.5f, 1e-4f);
      IM_CHECK_FLOAT_NEAR(gui::g_state.sim.ray_num_millions, 0.1f, 1e-6f);  // the document's Rays is not the panel's

      ctx->ItemClick("Infinite rays");
      ctx->Yield(1);
      IM_CHECK(gui::g_state.analysis.infinite);
      IM_CHECK(!gui::g_state.sim.infinite);                    // the document's Infinite rays is not the panel's
      IM_CHECK(IsDisabled(ctx->ItemInfo("##Rays(M)_input")));  // no total applies while unlimited
      ctx->SetRef("");

      // None of it ran anything or dirtied the document.
      IM_CHECK(!gui::g_state.analysis.started);
      IM_CHECK(!gui::g_state.analysis_run_in_progress);
      LUMICE_SimLifecycleResult after{};
      LUMICE_GetSimLifecycle(gui::g_server, &after);
      IM_CHECK_EQ(after.epoch, before.epoch);
      IM_CHECK_EQ(after.lifecycle, before.lifecycle);
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)sim_state_before);

      // Last, because it dirties the document: a later edit to the document's Rays does not
      // reach the seeded session field.
      gui::g_state.sim.ray_num_millions = 7.0f;
      ctx->Yield(2);
      IM_CHECK_FLOAT_NEAR(gui::g_state.analysis.ray_num_millions, 2.5f, 1e-4f);
    };
  }

  // AC3: entering Point mode places a centre at once — the viewport's middle pixel, unprojected —
  // so there is a marker to drag before any pick. The oracle is the C API on that pixel.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "point_mode_defaults_centre_to_viewport_middle");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      IM_CHECK(!gui::g_state.analysis.cone_center_valid);
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick("Point");
      ctx->SetRef("");
      ctx->Yield(1);
      IM_CHECK(gui::g_state.analysis.cone_center_valid);
      IM_CHECK(!gui::g_state.analysis.pick_armed);
      const LUMICE_AnnotationView view =
          gui::PanoramaAnnotationView(gui::g_state, gui::g_panorama_vp.vp_w, gui::g_panorama_vp.vp_h);
      float want[3] = { 0.0f, 0.0f, 0.0f };
      int valid = 0;
      IM_CHECK_EQ(LUMICE_UnprojectPixel(&view, gui::g_panorama_vp.vp_w / 2, gui::g_panorama_vp.vp_h / 2, want, &valid),
                  LUMICE_OK);
      IM_CHECK_EQ(valid, 1);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[0], want[0]);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[1], want[1]);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[2], want[2]);
      // And the marker is on the picture, at that pixel.
      const std::optional<gui::CanvasPixel> marker = gui::ProjectConeCenterMarker(gui::g_state, view);
      IM_CHECK(marker.has_value());
      IM_CHECK_EQ(marker->px, gui::g_panorama_vp.vp_w / 2);
      IM_CHECK_EQ(marker->py, gui::g_panorama_vp.vp_h / 2);
      // Analyze is open at once: the centre the button needed is there.
      SetListRef(ctx);
      IM_CHECK(!IsDisabled(ctx->ItemInfo(kAnalyzeButton)));
      ctx->SetRef("");
    };
  }

  // AC1: the marker is the direction's projection on THIS view. The panorama's view change is
  // the hemisphere switch (no camera orbit on this surface): switching to the lower hemisphere
  // takes the upper-sky direction off the picture, and switching back puts the marker exactly
  // where it was — while the direction, the truth, does not change.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "marker_follows_the_view_drag");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick("Point");
      ctx->Yield(2);
      ctx->ItemClick(kPickButton);
      ctx->SetRef("");
      ctx->Yield(1);
      PickPanoramaCentre(ctx);
      IM_CHECK(gui::g_state.analysis.cone_center_valid);
      IM_CHECK(!gui::g_state.analysis.pick_armed);
      const float dir_before[3] = { gui::g_state.analysis.cone_center_dir[0], gui::g_state.analysis.cone_center_dir[1],
                                    gui::g_state.analysis.cone_center_dir[2] };
      const std::optional<ImVec2> marker_before = MarkerScreenPos(ctx);
      IM_CHECK(marker_before.has_value());
      IM_CHECK(gui::g_state.workspace.panorama_upper);

      // Switch the hemisphere: the picture re-projects around the flipped axis (the projector
      // clamps the now-behind-camera direction to this view's rim — visible, not meaningful).
      ctx->ItemClick("**/Lower##panorama_hemisphere");
      ctx->Yield(3);
      IM_CHECK(!gui::g_state.workspace.panorama_upper);

      // Switch back: the same direction, the same pixel — the per-frame projection's contract.
      ctx->ItemClick("**/Upper##panorama_hemisphere");
      ctx->Yield(3);
      IM_CHECK(gui::g_state.workspace.panorama_upper);
      const std::optional<ImVec2> marker_after = MarkerScreenPos(ctx);
      IM_CHECK(marker_after.has_value());
      IM_CHECK_LT(ImFabs(marker_after->x - marker_before->x), 2.0f);
      IM_CHECK_LT(ImFabs(marker_after->y - marker_before->y), 2.0f);
      // The direction is the truth and did not change.
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[0], dir_before[0]);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[1], dir_before[1]);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[2], dir_before[2]);
    };
  }

  // AC2: the marker itself can be dragged. Hovering it shows the hand cursor; a press on it and a
  // move re-aim the centre to the direction under the cursor (LUMICE_UnprojectPixel on the
  // mouse's pixel — bit-identical, the drag transfers the inverse's output verbatim) while the
  // camera stays exactly where it was.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "marker_drag_moves_the_centre_not_the_view");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick("Point");
      ctx->SetRef("");
      ctx->Yield(1);
      IM_CHECK(gui::g_state.analysis.cone_center_valid);  // the default centre, AC3
      const std::optional<ImVec2> marker = MarkerScreenPos(ctx);
      IM_CHECK(marker.has_value());
      const float az_before = gui::g_state.renderer.azimuth;
      const float el_before = gui::g_state.renderer.elevation;
      const float fov_before = gui::g_state.renderer.fov;

      // Off the marker: the arrow. On it: the hand.
      ctx->MouseMoveToPos(ImVec2(marker->x + 120.0f, marker->y + 90.0f));
      ctx->Yield(2);
      IM_CHECK_EQ(ImGui::GetMouseCursor(), ImGuiMouseCursor_Arrow);
      ctx->MouseMoveToPos(*marker);
      ctx->Yield(2);
      IM_CHECK_EQ(ImGui::GetMouseCursor(), ImGuiMouseCursor_Hand);

      // Grab and drag to another point of the sky.
      ctx->MouseDown(0);
      ctx->Yield(1);
      IM_CHECK(gui::g_state.analysis.cone_marker_dragging);
      ctx->MouseMoveToPos(ImVec2(marker->x + 45.0f, marker->y - 30.0f));
      ctx->Yield(1);
      IM_CHECK(gui::g_state.analysis.cone_marker_dragging);
      IM_CHECK_EQ(ImGui::GetMouseCursor(), ImGuiMouseCursor_Hand);
      float want[3] = { 0.0f, 0.0f, 0.0f };
      IM_CHECK(UnprojectMouse(want));
      ctx->MouseUp(0);
      ctx->Yield(2);
      IM_CHECK(!gui::g_state.analysis.cone_marker_dragging);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[0], want[0]);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[1], want[1]);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[2], want[2]);
      // The marker now sits under the mouse (to the pixel rounding of the projection).
      const std::optional<ImVec2> marker_after = MarkerScreenPos(ctx);
      IM_CHECK(marker_after.has_value());
      IM_CHECK_LT(ImFabs(marker_after->x - ImGui::GetIO().MousePos.x), 2.0f);
      IM_CHECK_LT(ImFabs(marker_after->y - ImGui::GetIO().MousePos.y), 2.0f);
      // The camera did not move.
      IM_CHECK_EQ(gui::g_state.renderer.azimuth, az_before);
      IM_CHECK_EQ(gui::g_state.renderer.elevation, el_before);
      IM_CHECK_EQ(gui::g_state.renderer.fov, fov_before);
    };
  }

  // AC4: the pick mode is visible for as long as it is on. The button arms it and the banner
  // appears; Esc disarms it and the banner goes; armed again, the click on the preview sets the
  // centre and disarms.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "pick_mode_shows_a_banner_until_consumed");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick("Point");
      ctx->Yield(2);  // CONE's pick-button row is submitted the frame after the radio flips
      IM_CHECK(!PickBannerVisible(ctx));
      ctx->ItemClick(kPickButton);
      ctx->Yield(2);
      IM_CHECK(gui::g_state.analysis.pick_armed);
      IM_CHECK(PickBannerVisible(ctx));
      ctx->SetRef("");
      // Esc, over the panorama's picture (where the click would land): banner and flag both
      // clear. The Analyze workspace hosts no preview panel to move the mouse into.
      ImGuiWindow* picture = PanoramaPictureWindow();
      IM_CHECK(picture != nullptr);
      ctx->MouseMoveToPos(ImVec2(picture->Pos.x + picture->Size.x * 0.75f, picture->Pos.y + picture->Size.y * 0.75f));
      ctx->KeyPress(ImGuiKey_Escape);
      ctx->Yield(2);
      IM_CHECK(!gui::g_state.analysis.pick_armed);
      IM_CHECK(!PickBannerVisible(ctx));
      // Armed again, the click consumes it — at a point AWAY from the default marker (a click on
      // the marker is the drag gesture, which consumes the pick as well; the pick path proper is
      // what this case drives).
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick(kPickButton);
      ctx->SetRef("");
      ctx->Yield(2);
      IM_CHECK(gui::g_state.analysis.pick_armed);
      IM_CHECK(PickBannerVisible(ctx));
      const std::optional<ImVec2> marker = MarkerScreenPos(ctx);
      IM_CHECK(marker.has_value());
      ctx->MouseMoveToPos(ImVec2(marker->x + 80.0f, marker->y + 60.0f));
      ctx->Yield(2);
      IM_CHECK_EQ(ImGui::GetMouseCursor(), ImGuiMouseCursor_Arrow);  // not the hand: not on the marker
      float want[3] = { 0.0f, 0.0f, 0.0f };
      IM_CHECK(UnprojectMouse(want));
      ctx->MouseClick(0);
      ctx->Yield(2);
      IM_CHECK(!gui::g_state.analysis.pick_armed);
      IM_CHECK(!PickBannerVisible(ctx));
      IM_CHECK(!gui::g_state.analysis.cone_marker_dragging);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[0], want[0]);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[1], want[1]);
      IM_CHECK_EQ(gui::g_state.analysis.cone_center_dir[2], want[2]);
    };
  }

  // The marker is drawn on the preview window's own draw list, not the foreground one: a window
  // drawn after the preview covers it. Read off the screen, since which list a primitive went to
  // is not something the item registry sees: the pixel under the marker IS the accent colour with
  // nothing over it (the positive control — without it a wrong sample point would pass the case
  // for free), and is NOT once the Edit Entry modal is up, whose dim layer and body are drawn
  // over every window beneath the popup stack. On the foreground list the marker painted over
  // both, and this second read came back accent.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "marker_sits_under_a_modal_not_over_it");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      const ScopedPopups popup_guard(ctx);
      OpenWindow(ctx);
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick("Point");
      ctx->SetRef("");
      ctx->Yield(2);
      std::optional<ImVec2> marker = MarkerScreenPos(ctx);
      IM_CHECK(marker.has_value());
      // The analysis window out of the way of the marker (its top-left past the marker), and the
      // mouse too, so that neither is what the sample lands on.
      ctx->WindowMove(kWindowRef, ImVec2(marker->x + 40.0f, marker->y + 40.0f));
      ctx->MouseMoveToPos(ImVec2(marker->x - 120.0f, marker->y - 90.0f));
      ctx->Yield(2);
      marker = MarkerScreenPos(ctx);
      IM_CHECK(marker.has_value());
      unsigned char rgb[3] = { 0, 0, 0 };
      IM_CHECK(ReadScreenPixel(ctx, *marker, rgb));
      ctx->LogInfo("marker pixel, nothing over it: %d %d %d", rgb[0], rgb[1], rgb[2]);
      IM_CHECK(IsAccentColour(rgb));

      // Opened through OpenEditModal rather than by a card click, the way test_entry_management
      // does: the click's own path is not what this case is about, the window's presence is.
      gui::g_state.modal_immediate_mode = false;  // staged: a real modal, on the popup stack
      const gui::EditRequest req{ gui::EditTarget::kCrystal, 0, 0 };
      gui::OpenEditModal(req, gui::g_state);
      ctx->Yield(4);
      ImGuiWindow* modal = ImGui::GetTopMostPopupModal();
      IM_CHECK(modal != nullptr);
      IM_CHECK_STR_EQ(modal->Name, "Edit Entry");
      IM_CHECK(gui::g_state.analysis.roi_mode == LUMICE_RAYPATH_ROI_CONE);  // the marker is still asked for
      IM_CHECK(ReadScreenPixel(ctx, *marker, rgb));
      ctx->LogInfo("marker pixel, modal up: %d %d %d", rgb[0], rgb[1], rgb[2]);
      IM_CHECK(!IsAccentColour(rgb));
    };
  }

  // An In filter on the chain's crystal keeps Exclude shut: the button is disabled with the
  // selection made, the eligibility names the reason, and nothing is written by a click. The
  // tooltip TEXT is not read off the screen — ImGui::SetTooltip draws through TextUnformatted
  // with id 0, which the test engine's registry never sees (the limit
  // test_view_display_controls.cpp records for its own disabled entries) — so the words are
  // asserted on the same function the tooltip prints, with the hover driven for real so that
  // frame's SetTooltip path is exercised too.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "exclude_is_shut_by_an_in_filter_on_the_crystal");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      IM_CHECK(RunPointAnalysisToCompletion(ctx));
      // The document's one entry gets an In filter (FilterConfig's default action) — as the
      // filter editor would leave it after "keep only 1-3".
      IM_CHECK(gui::g_state.filters.empty());
      gui::FilterConfig keep;
      keep.name = "keep 1-3";
      keep.action = 0;
      keep.param = gui::FromLegacyRaypath(gui::RaypathParams{ "1-3" });
      gui::g_state.filters.push_back(keep);
      gui::g_state.layers[0].entries[0].filter_id = 0;

      const auto& view_result = gui::g_state.analysis_result;
      IM_CHECK(!view_result.display_order.empty());
      const std::string top_display =
          view_result.payload->entries[static_cast<size_t>(view_result.display_order[0])].display;
      SetListRef(ctx);
      ctx->ItemClick((std::string("**/") + top_display).c_str());
      ctx->Yield(1);
      IM_CHECK(gui::g_state.analysis.selected_entry.has_value());
      IM_CHECK(IsDisabled(ctx->ItemInfo(ICON_FA_BAN " Exclude this raypath")));
      std::string why;
      IM_CHECK_EQ((int)gui::EvaluateExcludeEligibility(gui::g_state, &why),
                  (int)gui::ExcludeEligibility::kEntryHasInFilter);
      IM_CHECK(!why.empty());
      IM_CHECK(why.find("In filter") != std::string::npos);
      IM_CHECK(gui::ExcludeAppendNotice(gui::g_state).empty());
      // Hover, so the disabled button's tooltip frame is drawn; then click, which does nothing.
      ctx->MouseMove(ICON_FA_BAN " Exclude this raypath");
      ctx->Yield(2);
      ctx->ItemClick(ICON_FA_BAN " Exclude this raypath");
      ctx->SetRef("");
      ctx->Yield(1);
      IM_CHECK_EQ(gui::g_state.filters.size(), 1u);
      IM_CHECK_EQ(gui::g_state.filters[0].action, 0);
      IM_CHECK_EQ(gui::g_state.filters[0].param.size(), 1u);
      IM_CHECK_STR_EQ(gui::g_state.filters[0].name.c_str(), "keep 1-3");
    };
  }


  // Exclude with the Immediate-mode editor open on the same entry — see
  // ExcludeWhileTheImmediateEditorIsOpen. Two cases, one per way the editor's copy used to win over
  // the pool: a filter it had never seen (the entry had none), and a row appended to the one it
  // was holding.
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "raypath_analysis", "exclude_while_the_immediate_editor_is_open_binds_a_new_filter");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(ExcludeWhileTheImmediateEditorIsOpen(ctx, /*start_with_out_filter=*/false));
    };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis",
                                    "exclude_while_the_immediate_editor_is_open_appends_to_its_out_filter");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(ExcludeWhileTheImmediateEditorIsOpen(ctx, /*start_with_out_filter=*/true));
    };
  }

  // Run after a cone-stopped analysis renders — see RunAfterAnalysisRenders. Three cases: the
  // sequence itself on the CPU backend, the same with the owner's Exclude step in between, and
  // the sequence on the GPU backend (Metal on this tree's reference machine; the analysis is
  // forced to the CPU route inside the same server, so the render that follows is the GPU's
  // first session after an analysis — the shape the bug was reported on).
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "run_after_analysis_renders");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(RunAfterAnalysisRenders(ctx, /*exclude=*/false, /*gpu=*/false));
    };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "run_after_analysis_with_exclude_renders");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(RunAfterAnalysisRenders(ctx, /*exclude=*/true, /*gpu=*/false));
    };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "run_after_analysis_renders_gpu");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(RunAfterAnalysisRenders(ctx, /*exclude=*/false, /*gpu=*/true));
    };
  }

  // Run after a COMPLETED analysis, once per ROI, and DoRun's rebuild predicate agreed with the
  // server — see RunAfterCompletedAnalysisHasNoRebuildMismatch. The three cases above end their
  // analysis by Stop; these let it end on its budget, the other end a run has, and read the
  // safety net's warning off the GUI logger instead of only the picture. One case per ROI
  // because the ROI decides which consumer the analysis session builds, and the mismatch used
  // to fire for every one of them.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "run_after_completed_point_analysis_no_mismatch");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      IM_CHECK(RunPointAnalysisToCompletion(ctx));
      IM_CHECK_EQ(gui::g_state.analysis_result.payload->roi_mode, LUMICE_RAYPATH_ROI_CONE);
      IM_CHECK(RunAfterCompletedAnalysisHasNoRebuildMismatch(ctx));
    };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "run_after_completed_whole_sky_analysis_no_mismatch");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      IM_CHECK(RunWholeSkyAnalysisToCompletion(ctx, nullptr));
      IM_CHECK_EQ(gui::g_state.analysis_result.payload->roi_mode, LUMICE_RAYPATH_ROI_FULL_SKY);
      IM_CHECK(RunAfterCompletedAnalysisHasNoRebuildMismatch(ctx));
    };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "run_after_completed_in_frame_analysis_no_mismatch");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      IM_CHECK(RunInFrameAnalysisToCompletion(ctx));
      IM_CHECK(RunAfterCompletedAnalysisHasNoRebuildMismatch(ctx));
    };
  }

  // The bounded record on screen (v4.35). A result whose record was full — three rows plus an
  // "other" bucket — is put on show directly (no server: the row and the column are a property of
  // the list, and the record-level numbers come through the payload the read fills), and the
  // list must carry the "Cumulative %" header, a monotone column, and the fixed "other" line at
  // the bottom that a click cannot select: the selection stays what it was, and with nothing
  // selected the Exclude button stays disabled. The status line names the truncation. The cone
  // filter is a real path here too: no server is needed to drag the slider, and the other line
  // must keep closing the column after a re-sum.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "other_row_and_cumulative_column");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      ResetTestState();
      OpenWindow(ctx);
      auto payload = std::make_shared<gui::AnalysisPayload>();
      payload->snapshot_generation = 3;
      payload->roi_mode = LUMICE_RAYPATH_ROI_FULL_SKY;
      payload->other_energy = 2.0;
      payload->other_count = 20;
      payload->truncated_chain_count = 7;
      const double energies[3] = { 5.0, 2.0, 1.0 };
      for (int i = 0; i < 3; i++) {
        LUMICE_RaypathHistogramEntry e{};
        e.chain_len = 1;
        e.chain[0].crystal_id = 1;
        e.chain[0].segment_len = 2;
        e.chain[0].segment[0] = i + 1;
        e.chain[0].segment[1] = i + 2;
        snprintf(e.display, sizeof(e.display), "%d-%d", i + 1, i + 2);
        e.energy = energies[i];
        e.count = 100;
        e.error_bound = i == 2 ? 0.5 : 0.0;  // the last row took a slot over
        payload->entries.push_back(e);
      }
      IM_CHECK(gui::AdoptAnalysisPayloadIfNew(gui::g_state, payload));
      gui::g_state.analysis.fetched_once = true;
      gui::g_state.analysis.fetched_generation = 3;
      gui::g_state.analysis.fetched_symmetry = gui::AnalysisSymmetryBits(gui::g_state);
      ctx->Yield(2);
      SetListRef(ctx);

      // The column and the line exist in the rendered table. The header cell is not addressable
      // by label (imgui_tables.cpp never hands header labels to the engine), so the column is read
      // off the table object itself, whose id BeginTable computed from the window with nothing
      // pushed — the same route test_defaults_panel.cpp takes to its settings table.
      ImGuiWindow* win = ctx->GetWindowByRef(kWindowRef);
      IM_CHECK(win != nullptr);
      ImGuiTable* table = ImGui::TableFindByID(win->GetID("##analysis_rows"));
      IM_CHECK(table != nullptr);
      bool has_cumulative = false;
      for (int n = 0; n < table->ColumnsCount; n++) {
        has_cumulative = has_cumulative || std::strcmp(ImGui::TableGetColumnName(table, n), "Cumulative %") == 0;
      }
      IM_CHECK(has_cumulative);
      // Raypath, Energy, Cumulative %, +/- — and no Rays column: under adaptive ray allocation
      // a row's hit count follows the online deal, not the crystal's proportion, so it was
      // dropped from the table and the CSV alike.
      bool has_rays = false;
      for (int n = 0; n < table->ColumnsCount; n++) {
        has_rays = has_rays || std::strcmp(ImGui::TableGetColumnName(table, n), "Rays") == 0;
      }
      IM_CHECK(!has_rays);
      IM_CHECK_EQ(table->ColumnsCount, 4);
      const ImGuiTestItemInfo other =
          ctx->ItemInfo(std::string("**/").append(gui::kAnalysisOtherRowLabel).c_str(), ImGuiTestOpFlags_NoError);
      IM_CHECK(other.ID != 0);
      IM_CHECK(IsDisabled(other));
      // Below every chain row: the last row's rect is above it.
      const ImGuiTestItemInfo last_row = ctx->ItemInfo("**/3-4");
      IM_CHECK(last_row.ID != 0);
      IM_CHECK_GT(other.RectFull.Min.y, last_row.RectFull.Min.y);
      // The column is what the unit computed: 5/10, 7/10, 8/10, and the other line's 20 closes it.
      const auto& view = gui::g_state.analysis_result;
      IM_CHECK_EQ(view.display_cumulative_pct.size(), 3u);
      IM_CHECK_FLOAT_NEAR(view.display_cumulative_pct[0], 50.0, 1e-9);
      IM_CHECK_FLOAT_NEAR(view.display_cumulative_pct[1], 70.0, 1e-9);
      IM_CHECK_FLOAT_NEAR(view.display_cumulative_pct[2], 80.0, 1e-9);
      IM_CHECK_FLOAT_NEAR(view.display_cumulative_pct[2] + gui::AnalysisOtherPct(gui::g_state), 100.0, 1e-9);

      // A click on the other line selects nothing; Exclude stays disabled.
      IM_CHECK(!gui::g_state.analysis.selected_entry.has_value());
      ctx->ItemClick(std::string("**/").append(gui::kAnalysisOtherRowLabel).c_str());
      ctx->Yield(1);
      IM_CHECK(!gui::g_state.analysis.selected_entry.has_value());
      IM_CHECK(IsDisabled(ctx->ItemInfo(ICON_FA_BAN " Exclude this raypath")));
      // And it does not take a selection away from a real row either.
      ctx->ItemClick("**/1-2");
      ctx->Yield(1);
      IM_CHECK(gui::g_state.analysis.selected_entry.has_value() && *gui::g_state.analysis.selected_entry == "1-2");
      ctx->ItemClick(std::string("**/").append(gui::kAnalysisOtherRowLabel).c_str());
      ctx->Yield(1);
      IM_CHECK(gui::g_state.analysis.selected_entry.has_value() && *gui::g_state.analysis.selected_entry == "1-2");
      ctx->SetRef("");

      // The same list without a bucket has no other line and the column ends at 100.
      auto exact = std::make_shared<gui::AnalysisPayload>(*payload);
      exact->snapshot_generation = 4;
      exact->other_energy = 0.0;
      exact->other_count = 0;
      exact->truncated_chain_count = 0;
      IM_CHECK(gui::AdoptAnalysisPayloadIfNew(gui::g_state, exact));
      gui::g_state.analysis.fetched_generation = 4;
      ctx->Yield(2);
      SetListRef(ctx);
      IM_CHECK(
          ctx->ItemInfo(std::string("**/").append(gui::kAnalysisOtherRowLabel).c_str(), ImGuiTestOpFlags_NoError).ID ==
          0);
      IM_CHECK_FLOAT_NEAR(gui::g_state.analysis_result.display_cumulative_pct.back(), 100.0, 1e-9);
      ctx->SetRef("");
    };
  }

  // The analysis is a submission of the document itself (v4.36), and needs no Run before it.
  // Three documents that could not be analysed before, one case each.
  //
  // An .lmc with a baked picture, opened and never run: the app reads it as kLoaded / kDone, a
  // picture is on screen to pick on, and the server behind it has committed nothing — replaced
  // by a fresh one after the save, so the run that baked the picture cannot be what the analysis
  // traces. Point mode, a click on the halo, Analyze: the 22-degree path leads, and sim_state
  // never read kSimulating on the way — the picture stayed the file's.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "a_loaded_lmc_analyses_without_a_run");
    // The baked branch of DoOpen uploads the file's picture, a GL call, so it has to run on the
    // render thread: the GuiFunc opens the file once the TestFunc has named it (the same shape
    // test_file_ops.cpp uses for its baked-preview case).
    static bool s_open_done = false;
    static std::filesystem::path s_open_path;
    t->GuiFunc = [](ImGuiTestContext*) {
      if (!s_open_done && !s_open_path.empty()) {
        gui::DoNew();
        gui::DoOpen(s_open_path);
        s_open_done = true;
      }
    };
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      s_open_done = false;
      s_open_path.clear();
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      // Saved the way the app saves: PerformSave refreshes the CPU copy of the picture from the
      // server's frame before writing, which is what puts a baked texture into the file at all
      // (the preview's live texture is the float XYZ one, which is never the saved copy).
      const std::filesystem::path path = GuiTestTempPath("analysis_loaded_baked.lmc");
      gui::g_state.current_file_path = path;
      gui::g_state.save_texture = true;
      gui::PerformSave();
      IM_CHECK(std::filesystem::exists(path));
      IM_CHECK(ReplaceServerWithAFreshOne(ctx));

      s_open_path = path;
      IM_CHECK(DriveUntil(ctx, [] { return s_open_done; }, 5));
      ctx->Yield(2);
      IM_CHECK_EQ((int)gui::g_state.run_intent, (int)gui::RunIntent::kLoaded);
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)SimState::kDone);
      IM_CHECK(gui::g_preview_vp.active);
      IM_CHECK(gui::AnalysisPictureNotice(gui::g_state.run_intent, gui::g_state.sim_state) == nullptr);

      OpenWindow(ctx);
      SetListRef(ctx);
      IM_CHECK(!IsDisabled(ctx->ItemInfo(kAnalyzeButton)));
      ctx->SetRef("");
      IM_CHECK(StartPointAnalysis(ctx));
      bool saw_simulating = false;
      IM_CHECK(DriveUntil(
          ctx,
          [&saw_simulating] {
            saw_simulating = saw_simulating || gui::g_state.sim_state == SimState::kSimulating;
            return !gui::g_state.analysis_run_in_progress && gui::g_state.analysis_result.payload != nullptr;
          },
          60));
      IM_CHECK(!saw_simulating);
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)SimState::kDone);
      IM_CHECK_EQ((int)gui::g_state.run_intent, (int)gui::RunIntent::kLoaded);
      IM_CHECK_EQ(gui::g_state.analysis_result.payload->roi_mode, LUMICE_RAYPATH_ROI_CONE);
      IM_CHECK_GT(gui::g_state.analysis_result.payload->entries.size(), 0u);
      IM_CHECK_STR_EQ(TopChainDisplay().c_str(), "3-5");
      // The fresh server's only submission was the analysis: one epoch, the analysis's.
      LUMICE_SimLifecycleResult lc{};
      LUMICE_GetSimLifecycle(gui::g_server, &lc);
      IM_CHECK_EQ(lc.epoch, 1u);
      std::filesystem::remove(path);
    };
  }

  // A document that never had a picture (kNone / kIdle, no preview): Analyze and the In frame /
  // Point radios are all disabled, the panel says there is no picture, and a click on Analyze
  // starts nothing. Then a Run from the top bar renders as it always did, the notice goes, the
  // three come alive, and an edit afterwards (kModified) leaves them alive — the picture need
  // only have existed, not match.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "a_never_run_document_waits_for_its_first_run");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      ResetTestState();
      gui::g_server = LUMICE_CreateServer();
      IM_CHECK(gui::g_server != nullptr);
      gui::ResetServerConstructionTrackers();
      LUMICE_SetLogLevel(gui::g_server, static_cast<LUMICE_LogLevel>(g_core_log_level));
      IM_CHECK(gui::DeserializeFromJson(kHalo22Json, gui::g_state));
      gui::g_state.renderer.sim_resolution_index = 0;
      gui::g_state.sim.infinite = false;
      gui::g_state.sim.ray_num_millions = 0.1f;
      ctx->Yield(2);
      IM_CHECK_EQ((int)gui::g_state.run_intent, (int)gui::RunIntent::kNone);
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)SimState::kIdle);
      IM_CHECK(!gui::g_preview_vp.active);
      const char* notice = gui::AnalysisPictureNotice(gui::g_state.run_intent, gui::g_state.sim_state);
      IM_CHECK(notice != nullptr);
      IM_CHECK(std::strstr(notice, "No rendered image") != nullptr);
      // A server and an idle backend: the picture is the only denial left.
      IM_CHECK(!gui::CanStartAnalysis(true, gui::g_state.sim_state, false, gui::g_state.run_intent));

      OpenWindow(ctx);
      SetListRef(ctx);
      IM_CHECK(IsDisabled(ctx->ItemInfo(kAnalyzeButton)));
      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(IsDisabled(ctx->ItemInfo("In frame")));
      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(IsDisabled(ctx->ItemInfo("Point")));
      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(!IsDisabled(ctx->ItemInfo("Whole sky")));
      // A click on the disabled button does nothing: no intent, no run.
      SetListRef(ctx);
      ctx->ItemClick(kAnalyzeButton);
      ctx->Yield(2);
      IM_CHECK(!gui::g_state.analysis.started);
      IM_CHECK(!gui::g_state.analysis_run_in_progress);
      ctx->SetRef("");

      // The first Run, from the top bar: it renders, and the notice is gone.
      const unsigned long long uploads_before = gui::g_state.texture_upload_count;
      IM_CHECK(!IsDisabled(ctx->ItemInfo("##TopBar/" ICON_FA_PLAY " Run")));
      ctx->ItemClick("##TopBar/" ICON_FA_PLAY " Run");
      IM_CHECK(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kSimulating; }, 10));
      IM_CHECK(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kDone; }, 60));
      IM_CHECK(DriveUntil(ctx, [uploads_before] { return gui::g_state.texture_upload_count > uploads_before; }, 10));
      IM_CHECK_EQ((int)gui::g_state.run_intent, (int)gui::RunIntent::kRunCompleted);
      IM_CHECK(gui::AnalysisPictureNotice(gui::g_state.run_intent, gui::g_state.sim_state) == nullptr);
      SetListRef(ctx);
      IM_CHECK(!IsDisabled(ctx->ItemInfo(kAnalyzeButton)));
      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(!IsDisabled(ctx->ItemInfo("In frame")));
      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(!IsDisabled(ctx->ItemInfo("Point")));
      ctx->SetRef("");

      // An edit after the run: the picture is of the previous configuration, and that is a
      // notice, not a refusal — all three stay alive and the analysis completes.
      gui::g_state.crystals[0].height = 0.1f;
      gui::g_state.dirty = true;
      IM_CHECK(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kModified; }, 5));
      SetListRef(ctx);
      IM_CHECK(!IsDisabled(ctx->ItemInfo(kAnalyzeButton)));
      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(!IsDisabled(ctx->ItemInfo("In frame")));
      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(!IsDisabled(ctx->ItemInfo("Point")));
      ctx->SetRef("");
      bool saw_simulating = false;
      IM_CHECK(RunWholeSkyAnalysisToCompletion(ctx, &saw_simulating));
      IM_CHECK(!saw_simulating);
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)SimState::kModified);
      IM_CHECK_EQ(gui::g_state.analysis_result.payload->roi_mode, LUMICE_RAYPATH_ROI_FULL_SKY);
    };
  }

  // The regression the gate was changed for: a background photograph on a document that has never
  // been rendered. The preview publishes its viewport for a photograph alone (g_preview_vp.active
  // reads true — the positive control that the OLD gate, which read that flag, would have lit In
  // frame and Pick), but a photograph is not a picture of the document: run_intent is still
  // kNone, and In frame, Point and Analyze all stay disabled.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "a_background_photograph_alone_is_not_a_picture");
    static bool s_bg_requested = false;
    static bool s_bg_done = false;
    // The upload is a GL call, so it runs on the render thread: GuiFunc, once the TestFunc asks.
    t->GuiFunc = [](ImGuiTestContext*) {
      if (s_bg_requested && !s_bg_done) {
        const std::vector<unsigned char> grey(static_cast<size_t>(16 * 16 * 3), 128);
        gui::g_preview.UploadBgTexture(grey.data(), 16, 16);
        s_bg_done = true;
      }
    };
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      s_bg_requested = false;
      s_bg_done = false;
      ResetTestState();
      gui::g_server = LUMICE_CreateServer();
      IM_CHECK(gui::g_server != nullptr);
      gui::ResetServerConstructionTrackers();
      IM_CHECK(gui::DeserializeFromJson(kHalo22Json, gui::g_state));
      s_bg_requested = true;
      IM_CHECK(DriveUntil(ctx, [] { return s_bg_done; }, 5));
      gui::g_state.bg_show = true;
      ctx->Yield(2);
      IM_CHECK(gui::g_preview.HasBackground());
      IM_CHECK(!gui::g_preview.HasTexture());
      IM_CHECK(gui::g_preview_vp.active);  // the flag the old gate read: it IS up
      IM_CHECK_EQ((int)gui::g_state.run_intent, (int)gui::RunIntent::kNone);

      OpenWindow(ctx);
      SetListRef(ctx);
      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(IsDisabled(ctx->ItemInfo("In frame")));
      ctx->SetRef(PanoramaRegionRef());
      IM_CHECK(IsDisabled(ctx->ItemInfo("Point")));
      SetListRef(ctx);
      IM_CHECK(IsDisabled(ctx->ItemInfo(kAnalyzeButton)));
      ctx->SetRef("");
      IM_CHECK(!gui::CanStartAnalysis(true, gui::g_state.sim_state, false, gui::g_state.run_intent));
    };
  }

  // A rendered document, then an edit that changes what the sky looks like (kModified): Analyze
  // stays enabled, the panel says the picture is of the previous configuration, and the list is
  // the EDITED document's. The edit is the crystal itself — a randomly oriented column becomes a
  // thin horizontal plate — so the top chain changes from the 22-degree path through the side
  // faces to the straight pass through the basal faces, which is how "the edited document" is
  // told apart from "the rendered one" without reading anything off the server but the list.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "an_edited_document_analyses_as_edited");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpHaloScene(ctx, /*infinite=*/false));
      OpenWindow(ctx);
      // Positive control on the rendered document: the column's 22-degree path leads.
      IM_CHECK(RunWholeSkyAnalysisToCompletion(ctx, nullptr));
      IM_CHECK_STR_EQ(TopChainDisplay().c_str(), "3-5");
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)SimState::kDone);
      IM_CHECK(gui::AnalysisPictureNotice(gui::g_state.run_intent, gui::g_state.sim_state) == nullptr);

      // The edit: a thin plate lying flat. Marked dirty by hand — the widgets' frame-tail
      // reconcile is what does it in the app, and this edit did not go through a widget.
      gui::g_state.crystals[0].height = 0.1f;
      gui::g_state.crystals[0].zenith = gui::AxisDist{ gui::AxisDistType::kUniform, 0.0f, 0.0f };
      gui::g_state.dirty = true;
      IM_CHECK(DriveUntil(ctx, [] { return gui::g_state.sim_state == SimState::kModified; }, 5));
      const char* notice = gui::AnalysisPictureNotice(gui::g_state.run_intent, gui::g_state.sim_state);
      IM_CHECK(notice != nullptr);
      IM_CHECK(std::strstr(notice, "previous configuration") != nullptr);
      const unsigned long long uploads_before = gui::g_state.texture_upload_count;

      bool saw_simulating = false;
      IM_CHECK(RunWholeSkyAnalysisToCompletion(ctx, &saw_simulating));
      IM_CHECK(!saw_simulating);
      const std::string top = TopChainDisplay();
      ctx->LogInfo("edited document's top chain: %s", top.c_str());
      IM_CHECK_STR_EQ(top.c_str(), "1-2");
      // The picture is still the column's, and still marked as the previous configuration's:
      // the analysis rendered nothing and changed no intent.
      IM_CHECK_EQ((int)gui::g_state.sim_state, (int)SimState::kModified);
      IM_CHECK_EQ(gui::g_state.texture_upload_count, uploads_before);
      IM_CHECK(gui::AnalysisPictureNotice(gui::g_state.run_intent, gui::g_state.sim_state) != nullptr);
    };
  }

  // The chain label on screen. Core formats a multi-layer chain as "(3-5) -> (1-3)" — the C API
  // contract, ASCII on purpose because the body font has no U+2192 — and the list draws that
  // text through AnalysisRowLabel, which redraws the joiner as the ICON_FA_ARROW_RIGHT glyph
  // and each "C<core>(" crystal prefix as the card's "#<pool>(".
  // Three things have to be true of a real multi-layer row, and the unit truth table can show
  // none of them: the row's label in the ImGui tree IS the rewritten text (so the render path
  // goes through the function, not around it), the rewritten text has no "?" — the fallback
  // ImGui draws for a codepoint the atlas lacks, which is what the arrow WOULD be if it were
  // U+2192 — and the crystal prefix came out spelled the way the cards spell it, checked against
  // hand-written literals rather than the same helper the render runs. The first multi-layer row
  // is used, not the first row: which chain carries the most energy is the scene's business (on
  // this scene the top rows are single-layer), not this label's.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "chain_label_shows_arrow_glyph_not_qmark");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(BringUpScene(ctx, kPcTwoLayerJson, /*infinite=*/false, /*gpu=*/false));
      OpenWindow(ctx);
      ctx->SetRef(PanoramaRegionRef());
      ctx->ItemClick("Whole sky");
      ctx->SetRef(ListWindowRef());
      SetListRef(ctx);
      IM_CHECK(!IsDisabled(ctx->ItemInfo(kAnalyzeButton)));
      SetListRef(ctx);
      ctx->ItemClick(kAnalyzeButton);
      ctx->SetRef("");
      IM_CHECK(DriveUntil(
          ctx, [] { return !gui::g_state.analysis_run_in_progress && gui::g_state.analysis_result.payload != nullptr; },
          60));
      const auto& view = gui::g_state.analysis_result;
      IM_CHECK_EQ(view.payload->roi_mode, LUMICE_RAYPATH_ROI_FULL_SKY);
      const LUMICE_RaypathHistogramEntry* multi = nullptr;
      for (const auto& e : view.payload->entries) {
        if (e.chain_len >= 2) {
          multi = &e;
          break;
        }
      }
      IM_CHECK(multi != nullptr);
      const std::string raw = multi->display;
      IM_CHECK(raw.find(" -> ") != std::string::npos);
      if (raw.front() != 'C') {
        IM_ERRORF("expected a C-prefixed row (this scene's layers hold two crystals each), got \"%s\"", raw.c_str());
      }
      // The production rewrite of the production text — the same function and the same
      // Analyze-time map the render path reads.
      const std::string label = gui::AnalysisRowLabel(raw, gui::AnalysisCoreToPoolMap(gui::g_state));
      // Hand-written literals, independent of that helper: the prefix came out in the card's
      // spelling, and no scene-spelled crystal token survives to the screen.
      IM_CHECK(label.rfind("#", 0) == 0);
      IM_CHECK(label.find('C') == std::string::npos);
      IM_CHECK(label.find(ICON_FA_ARROW_RIGHT) != std::string::npos);
      IM_CHECK(label.find(" -> ") == std::string::npos);
      IM_CHECK(label.find('?') == std::string::npos);
      IM_CHECK(raw.find('?') == std::string::npos);
      // And a row on screen carries exactly that label — found by it, under the window, with the
      // list panned to it (on this scene the first multi-layer row sits below the first page).
      SetListRef(ctx);
      const ImGuiID row_id = FindListRowScrolling(ctx, label);
      IM_CHECK(row_id != 0);
      // The raw text is NOT what is drawn: with the row on screen, no item carries the ASCII form.
      IM_CHECK(ctx->ItemInfo(("**/" + raw).c_str(), ImGuiTestOpFlags_NoError).ID == 0);
      // Selecting by the drawn label records the raw text — the selection is the entry's
      // contract text, the label only its presentation.
      ctx->ItemClick(row_id);
      IM_CHECK(gui::g_state.analysis.selected_entry.has_value());
      IM_CHECK_STR_EQ(gui::g_state.analysis.selected_entry->c_str(), raw.c_str());
      ctx->SetRef("");
      // A picture of the window for a human to look at (the atlas assertions live in
      // functional/test_body_font_glyph_coverage.cpp; this is the frame they add up to).
      ctx->MouseMoveToPos(ImVec2(2.0f, 2.0f));
      ctx->Yield(2);
      IM_CHECK(SaveWindowPng(ctx, kWindowRef, GuiTestTempPath("chain_label_arrow.png").string()));
    };
  }

  // The search box over the list (592.2). Four rows put on show directly — three chains and an
  // "other" bucket — and the box is typed into. What it does: hide the rows whose raw text AND
  // drawn label do not contain the typed text, case-insensitively — so the CSV's "C2(" and the
  // card spelling "#2(" find the same row. What it must not do: touch the numbers. The
  // display order and the Cumulative % column are the whole list's, hidden rows included (a
  // shown row's Cumulative % still reads "down to this row of everything"), and the selection is
  // the chain's text, so a selected row that the search hides is still the selection and the
  // Exclude button's answer is the same one. The other line is not a raypath and so not
  // searchable: any non-empty search hides it. The multi-layer display text is written by hand
  // here, not collected from a scene; only the substring mechanism is under test, not the
  // formatter, and the raw " -> " joiner is what a user copies out of the CSV or the CLI's
  // output, so "->" has to reach the multi-layer row — the shape ImGuiTextFilter::PassFilter
  // would have read as "exclude every row containing '>'".
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "search_narrows_visible_rows");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      ResetTestState();
      OpenWindow(ctx);
      // A scene for the label rewrite to number by: three crystals, three entries in the root
      // layer, and the Analyze-time snapshot taken from it — so the hand-written display's
      // "C2(" maps to card #2 and the drawn label spells "#2(". The search then has two
      // vocabularies to find the row by, which is what the assertions below pin.
      gui::g_state.crystals.assign(3, gui::CrystalConfig{});
      gui::Layer search_layer;
      search_layer.probability = 1.0f;
      for (int i = 0; i < 3; ++i) {
        gui::EntryCard entry;
        entry.crystal_id = i;
        entry.proportion = 100.0f;
        search_layer.entries.push_back(entry);
      }
      gui::g_state.layers.assign(1, search_layer);
      gui::g_state.analysis_result.analyzed_scene = gui::GuiState::AnalysisSceneIdentity::From(gui::g_state);
      auto payload = std::make_shared<gui::AnalysisPayload>();
      payload->snapshot_generation = 3;
      payload->roi_mode = LUMICE_RAYPATH_ROI_FULL_SKY;
      payload->other_energy = 2.0;
      payload->other_count = 20;
      payload->truncated_chain_count = 7;
      const char* const displays[3] = { "3-5", "1-2", "C2(3-5) -> (1-3)" };
      const double energies[3] = { 5.0, 2.0, 1.0 };
      for (int i = 0; i < 3; i++) {
        LUMICE_RaypathHistogramEntry e{};
        e.chain_len = i == 2 ? 2 : 1;
        e.chain[0].crystal_id = 1;
        e.chain[0].segment_len = 2;
        e.chain[0].segment[0] = 1;
        e.chain[0].segment[1] = 2;
        snprintf(e.display, sizeof(e.display), "%s", displays[i]);
        e.energy = energies[i];
        e.count = 100;
        payload->entries.push_back(e);
      }
      IM_CHECK(gui::AdoptAnalysisPayloadIfNew(gui::g_state, payload));
      gui::g_state.analysis.fetched_once = true;
      gui::g_state.analysis.fetched_generation = 3;
      gui::g_state.analysis.fetched_symmetry = gui::AnalysisSymmetryBits(gui::g_state);
      ctx->Yield(2);
      SetListRef(ctx);

      // Rows are found by what is DRAWN: the multi-layer row's label carries the arrow glyph and
      // the card's "#2(" spelling.
      const std::string multi_label = gui::AnalysisRowLabel(displays[2], gui::AnalysisCoreToPoolMap(gui::g_state));
      // Hand-written literal, independent of the helper: "C2(...)" under the snapshot built
      // above draws as the card spelling with the glyph joiner and nothing else moved.
      IM_CHECK_STR_EQ(multi_label.c_str(), ("#2(3-5) " ICON_FA_ARROW_RIGHT " (1-3)"));
      const std::string other_path = gui::kAnalysisOtherRowLabel;
      auto row_shown = [&](const std::string& label) {
        // The rows live in the table's child window: an item path from the list window cannot
        // cross into it without the "**/" wildcard descent.
        return ctx->ItemInfo((std::string("**/") + label).c_str(), ImGuiTestOpFlags_NoError).ID != 0;
      };
      auto search = [&](const char* text) {
        ctx->ItemInputValue("###analysis_search", text);
        ctx->Yield(3);
      };
      // Empty box: everything, other line included.
      IM_CHECK(row_shown("3-5"));
      IM_CHECK(row_shown("1-2"));
      IM_CHECK(row_shown(multi_label));
      IM_CHECK(row_shown(gui::kAnalysisOtherRowLabel));
      const std::vector<double> cumulative_before = gui::g_state.analysis_result.display_cumulative_pct;
      const std::vector<int> order_before = gui::g_state.analysis_result.display_order;
      IM_CHECK_EQ(cumulative_before.size(), 3u);

      // "3-5": the single-layer row and the multi-layer row that contains it; not "1-2", not other.
      search("3-5");
      IM_CHECK(row_shown("3-5"));
      IM_CHECK(!row_shown("1-2"));
      IM_CHECK(row_shown(multi_label));
      IM_CHECK(!row_shown(gui::kAnalysisOtherRowLabel));
      // The numbers did not move: same order, same column, entry for entry — the same bytes, since
      // nothing recomputed them (a recompute of the same list would also pass, and the point is
      // that the search does not own those numbers; whichever way, the columns read as before).
      const auto& view = gui::g_state.analysis_result;
      IM_CHECK(view.display_order == order_before);
      IM_CHECK(view.display_cumulative_pct == cumulative_before);

      // Cleared: all four back.
      search("");
      IM_CHECK(row_shown("3-5"));
      IM_CHECK(row_shown("1-2"));
      IM_CHECK(row_shown(multi_label));
      IM_CHECK(row_shown(gui::kAnalysisOtherRowLabel));

      // "C2(" and "->": only the multi-layer row, by the raw text's crystal prefix and joiner.
      search("C2(");
      IM_CHECK(!row_shown("3-5"));
      IM_CHECK(!row_shown("1-2"));
      IM_CHECK(row_shown(multi_label));
      search("->");
      IM_CHECK(!row_shown("3-5"));
      IM_CHECK(!row_shown("1-2"));
      IM_CHECK(row_shown(multi_label));
      // Case-insensitive: the user types what they remember, not the exact case.
      search("c2(");
      IM_CHECK(row_shown(multi_label));
      // "#2(": only the DRAWN label carries it — the raw text has no '#' in it — so this is the
      // card spelling finding the row the C2 spelling finds: same row, two vocabularies.
      search("#2(");
      IM_CHECK(!row_shown("3-5"));
      IM_CHECK(!row_shown("1-2"));
      IM_CHECK(row_shown(multi_label));
      IM_CHECK(!row_shown(gui::kAnalysisOtherRowLabel));

      // The selection outlives being hidden, and Exclude's verdict does not depend on the box.
      search("");
      ctx->ItemClick("**/1-2");
      ctx->Yield(1);
      IM_CHECK(gui::g_state.analysis.selected_entry.has_value() && *gui::g_state.analysis.selected_entry == "1-2");
      const gui::ExcludeEligibility before = gui::EvaluateExcludeEligibility(gui::g_state, nullptr);
      search("3-5");
      IM_CHECK(!row_shown("1-2"));
      IM_CHECK(gui::g_state.analysis.selected_entry.has_value() && *gui::g_state.analysis.selected_entry == "1-2");
      IM_CHECK_EQ(static_cast<int>(gui::EvaluateExcludeEligibility(gui::g_state, nullptr)), static_cast<int>(before));
      search("");
      IM_CHECK(row_shown("1-2"));
      IM_CHECK(gui::g_state.analysis.selected_entry.has_value() && *gui::g_state.analysis.selected_entry == "1-2");
      // A picture of the window with the box in use, for a human to look at: where the box sits
      // and what a narrowed list looks like are not things the assertions above can show.
      search("3-5");
      ctx->SetRef("");
      ctx->MouseMoveToPos(ImVec2(2.0f, 2.0f));
      ctx->Yield(2);
      IM_CHECK(SaveWindowPng(ctx, kWindowRef, GuiTestTempPath("analysis_search.png").string()));
    };
  }

  // What clears the search box, and what does not. The box has the lifetime of
  // state.analysis (gui-state-governance.md §10.2: the analysis request parameters are session
  // state, cleared by the four document-switch reasons and kept by Revert), though it is not a
  // member of that struct — an ImGui type cannot enter gui_state.hpp — so ResetFrontendState
  // clears it by name, and this case is what catches that call going missing or landing in the
  // wrong branch. Revert: the payload and the text both survive, so the same row is still the
  // one shown. New document: the payload is gone, so the text is read off what a NEW payload
  // shows — a row the old text would have hidden.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "search_reset_semantics");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      ResetTestState();
      OpenWindow(ctx);
      auto make_payload = [](unsigned long long generation, std::initializer_list<const char*> displays) {
        auto payload = std::make_shared<gui::AnalysisPayload>();
        payload->snapshot_generation = generation;
        payload->roi_mode = LUMICE_RAYPATH_ROI_FULL_SKY;
        for (const char* d : displays) {
          LUMICE_RaypathHistogramEntry e{};
          e.chain_len = 1;
          e.chain[0].crystal_id = 1;
          e.chain[0].segment_len = 2;
          e.chain[0].segment[0] = 1;
          e.chain[0].segment[1] = 2;
          snprintf(e.display, sizeof(e.display), "%s", d);
          e.energy = 1.0;
          e.count = 100;
          payload->entries.push_back(e);
        }
        return payload;
      };
      auto show = [&](unsigned long long generation, std::initializer_list<const char*> displays) {
        IM_CHECK_RETV(gui::AdoptAnalysisPayloadIfNew(gui::g_state, make_payload(generation, displays)), false);
        gui::g_state.analysis.fetched_once = true;
        gui::g_state.analysis.fetched_generation = generation;
        gui::g_state.analysis.fetched_symmetry = gui::AnalysisSymmetryBits(gui::g_state);
        ctx->Yield(2);
        return true;
      };
      auto row_shown = [&](const char* label) {
        return ctx->ItemInfo((std::string("**/") + label).c_str(), ImGuiTestOpFlags_NoError).ID != 0;
      };
      IM_CHECK(show(3, { "1-2", "2-3" }));
      ctx->WindowMove(kWindowRef, ImVec2(60, 60));
      ctx->Yield(1);
      SetListRef(ctx);
      ctx->ItemInputValue("**/###analysis_search", "1-2");
      ctx->Yield(3);
      IM_CHECK(row_shown("1-2"));
      IM_CHECK(!row_shown("2-3"));

      // Revert keeps the result and the text: the same scene, the same view of it.
      gui::ResetFrontendState(gui::g_state, gui::FrontendResetReason::kRevert);
      ctx->Yield(3);
      IM_CHECK(gui::g_state.analysis_result.payload != nullptr);
      IM_CHECK(row_shown("1-2"));
      IM_CHECK(!row_shown("2-3"));

      // A new document drops the result; the text must go with it. Shown on a new result whose
      // only row the stale "1-2" would hide.
      gui::ResetFrontendState(gui::g_state, gui::FrontendResetReason::kNewDocument);
      ctx->Yield(3);
      IM_CHECK(gui::g_state.analysis_result.payload == nullptr);
      IM_CHECK(show(4, { "2-3" }));
      ctx->Yield(2);
      IM_CHECK(row_shown("2-3"));
      ctx->SetRef("");
    };
  }

  // Right-click on a row (592.3). Two menu items. "Copy raypath" puts the entry's RAW text on the
  // clipboard — the " -> " joiner the CSV and the CLI print, not the arrow glyph the row draws —
  // and "Copy row" puts the row in the CSV's own column order and formatting, which is asserted
  // against the same call the CSV export makes on the same numbers, not against a literal, so
  // the two cannot be checked into disagreement. Each item is reached by a fresh right-click:
  // a chosen menu item closes its popup. The clipboard read back is the process-local stand-in
  // test_gui_main.cpp installs, so this is what the GUI wrote, on any leg.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "row_menu_copies_raw_text_and_csv_row");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      ResetTestState();
      OpenWindow(ctx);
      IM_CHECK_STR_EQ(ImGui::GetClipboardText(), "");  // the reset emptied it
      auto payload = std::make_shared<gui::AnalysisPayload>();
      payload->snapshot_generation = 3;
      payload->roi_mode = LUMICE_RAYPATH_ROI_FULL_SKY;
      payload->other_energy = 2.0;
      payload->other_count = 20;
      const char* const displays[3] = { "3-5", "1-2", "C2(3-5) -> (1-3)" };
      const double energies[3] = { 5.0, 2.0, 1.0 };
      for (int i = 0; i < 3; i++) {
        LUMICE_RaypathHistogramEntry e{};
        e.chain_len = i == 2 ? 2 : 1;
        e.chain[0].crystal_id = 1;
        e.chain[0].segment_len = 2;
        e.chain[0].segment[0] = 1;
        e.chain[0].segment[1] = 2;
        snprintf(e.display, sizeof(e.display), "%s", displays[i]);
        e.energy = energies[i];
        e.count = 100 * (i + 1);
        payload->entries.push_back(e);
      }
      payload->entries[1].error_bound = 0.5;  // "1-2" took over a slot: its +/- cell has a bound
      IM_CHECK(gui::AdoptAnalysisPayloadIfNew(gui::g_state, payload));
      gui::g_state.analysis.fetched_once = true;
      gui::g_state.analysis.fetched_generation = 3;
      gui::g_state.analysis.fetched_symmetry = gui::AnalysisSymmetryBits(gui::g_state);
      ctx->Yield(2);

      // Right-click the row drawn as `label`, then choose `item` in the popup that opened.
      auto choose = [&](const std::string& label, const char* item) {
        SetListRef(ctx);
        ctx->ItemClick((std::string("**/") + label).c_str(), ImGuiMouseButton_Right);
        ctx->Yield(2);
        ctx->SetRef("//$FOCUSED");
        ctx->ItemClick(item);
        ctx->Yield(2);
        ctx->SetRef("");
      };
      // The expected row, from the export's own formatter on the view's own numbers.
      const auto& view = gui::g_state.analysis_result;
      auto csv_row = [&](const char* display) {
        for (size_t row = 0; row < view.display_order.size(); ++row) {
          const int idx = view.display_order[row];
          const auto& e = view.payload->entries[static_cast<size_t>(idx)];
          if (std::strcmp(e.display, display) == 0) {
            return lumice::FormatRaypathAnalysisCsvRow(e, view.display_energy[static_cast<size_t>(idx)],
                                                       view.display_cumulative_pct[row], view.display_total);
          }
        }
        return std::string("<no such row>");
      };

      choose("3-5", "Copy raypath");
      IM_CHECK_STR_EQ(ImGui::GetClipboardText(), "3-5");
      choose("3-5", "Copy row");
      IM_CHECK_STR_EQ(ImGui::GetClipboardText(), csv_row("3-5").c_str());
      // Not just the label with numbers after it: the row is the CSV's, four comma-separated
      // columns with Energy % / Cumulative % / +/- in the CSV's precisions.
      IM_CHECK_STR_EQ(ImGui::GetClipboardText(), "3-5,50.0000,50.0000,10.00");
      // The takeover row: its +/- cell carries the bound in parentheses, as the CSV's does.
      choose("1-2", "Copy row");
      IM_CHECK_STR_EQ(ImGui::GetClipboardText(), csv_row("1-2").c_str());
      IM_CHECK(std::strstr(ImGui::GetClipboardText(), " (-") != nullptr);
      // The multi-layer row is found on screen by its drawn label (card spelling under the
      // Analyze-time map, arrow glyph for the joiner) and copies its raw text.
      const std::string multi_label = gui::AnalysisRowLabel(displays[2], gui::AnalysisCoreToPoolMap(gui::g_state));
      choose(multi_label, "Copy raypath");
      IM_CHECK_STR_EQ(ImGui::GetClipboardText(), displays[2]);
      choose(multi_label, "Copy row");
      IM_CHECK_STR_EQ(ImGui::GetClipboardText(), csv_row(displays[2]).c_str());
      // A right-click neither selects the row nor is a left click: the selection is untouched.
      IM_CHECK(!gui::g_state.analysis.selected_entry.has_value());
      ctx->PopupCloseAll();
      ctx->Yield(2);
    };
  }

  // The greyed "excluded" rows — see ExcludedRowShowsAndIncludeAgainTakesItBack and
  // IncludeAgainWhileTheImmediateEditorIsOpen.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "excluded_row_shows_and_include_again_takes_it_back");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(ExcludedRowShowsAndIncludeAgainTakesItBack(ctx));
    };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "include_again_while_the_immediate_editor_is_open");
    t->TestFunc = [](ImGuiTestContext* ctx) {
      ScopedServerGuard guard;
      IM_CHECK(IncludeAgainWhileTheImmediateEditorIsOpen(ctx));
    };
  }

  // M2-5's real-app evidence: the full chain, on file.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "c01_chain_analyze_select_exclude_rerun");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(C01ChainAnalyzeSelectExcludeRerun(ctx)); };
  }

  // M3-5's survey-query evidence: the digest answers with real class facts (and re-Analyze
  // re-arms under the next generation), the multi-layer refusal names itself, and the
  // background query is orthogonal to a render run in both directions.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "survey_digest_answers_with_real_class_facts");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(SurveyDigestAnswersWithRealClassFacts(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "survey_overview_shows_dispersion_and_profile");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(SurveyOverviewShowsDispersionAndProfile(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "survey_refuses_multilayer_class_by_name");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(SurveyRefusesMultilayerClassByName(ctx)); };
  }
  // The map region (742.5): real chain, real click, real isolation — see the function's comment.
  // Real-timing pool: the map answers behind the survey's chromatic legs on the one worker, the
  // same wall-clock shape that put the survey overview case there.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "map_region_shows_the_class_map_and_takes_the_pick");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(MapRegionShowsTheClassMapAndTakesThePick(ctx)); };
  }
  // The point inspector (742.6): the real chain pick → point → component → slider/trace →
  // playback invariance. Real-timing pool: the point queues behind the selection's survey on the
  // one worker (the mass arm below is the fixed-dt pool's — it answers in milliseconds).
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "point_inspector_shows_the_point_fiber_and_the_path");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(PointInspectorShowsThePointFiberAndThePath(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "point_mass_arm_is_honest_and_forces_no_fiber");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(PointMassArmIsHonestAndForcesNoFiber(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "survey_and_render_run_are_orthogonal");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(SurveyAndRenderRunAreOrthogonal(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "survey_survives_a_workspace_switch");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(SurveySurvivesAWorkspaceSwitch(ctx)); };
  }
  // 742.7's integration legs — four real-timing (the chain pair and the C17 pair: the former
  // queues the per-face chain on one worker, the latter's survey carries the unbudgeted
  // witness/TIR tail; see scripts/build.sh's filter comments) and seven fixed-dt (the
  // lifecycle/identity set, across the halo and fully-random scenes).
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "analyze_full_chain_one_session");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(AnalyzeFullChainOneSession(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "internal_reflection_trace_from_the_real_consumer");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(InternalReflectionTraceFromTheRealConsumer(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "analyze_faces_rekey_on_row_switch");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(AnalyzeFacesRekeyOnRowSwitch(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "new_document_clears_every_analyze_face");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(NewDocumentClearsEveryAnalyzeFace(ctx)); };
  }
  // The scroll pins: skeleton workspace, no server, no sim — the
  // regions' structural no-scroll contract at three window bases, plus the divider's grab-bar
  // containment. Correctness pool: fixed-dt, state writes only, ~1-3 s.
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "workspace_scroll_regions_are_structurally_fixed");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(WorkspaceScrollRegionsAreStructurallyFixed(ctx)); };
  }
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "raypath_analysis", "identity_class_shows_the_mass_arm_and_the_map_answers");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(IdentityClassShowsTheMassArmAndTheMapAnswers(ctx)); };
  }
  {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "raypath_analysis", "refused_class_names_the_refusal_and_keeps_the_witness");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(RefusedClassNamesTheRefusalAndKeepsTheWitness(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "refused_class_blockades_nothing");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(RefusedClassBlockadesNothing(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "scene_edit_follows_the_explicit_accept_contract");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(SceneEditFollowsTheExplicitAcceptContract(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "cancel_survey_button_reaches_the_named_state");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(CancelSurveyButtonReachesTheNamedState(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "cancel_point_button_reaches_the_named_state");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(CancelPointButtonReachesTheNamedState(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "document_open_clears_every_analyze_face");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(DocumentOpenClearsEveryAnalyzeFace(ctx)); };
  }
}
