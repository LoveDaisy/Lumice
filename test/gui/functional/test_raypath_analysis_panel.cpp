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
  gui::MapViewChanged(&gui::g_state.analysis.map_view, ImGui::GetTime(), false);
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
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "survey_and_render_run_are_orthogonal");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(SurveyAndRenderRunAreOrthogonal(ctx)); };
  }
  {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "raypath_analysis", "survey_survives_a_workspace_switch");
    t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(SurveySurvivesAWorkspaceSwitch(ctx)); };
  }
}
