#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#if defined(__APPLE__) || defined(__linux__)
#include <sys/resource.h>
#endif

#include "gui/analysis_panel.hpp"
#include "gui/analyze_map_renderer.hpp"
#include "gui/analyze_overview.hpp"
#include "gui/gl_common.h"
#include "gui/preview_renderer.hpp"
#include "imgui_internal.h"
#include "include/lumice_raypath.h"
#include "include/lumice_scene.h"
#include "nlohmann/json.hpp"
#include "support/map_stress_fixture.hpp"
#include "test_gui_shared.hpp"

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;
using Direction = std::array<double, 3>;

// Independent double geometry: no shared GLSL, BuildViewMatrix, production index helper or
// colour helper. x/y are continuous GL pixel coordinates, measured from the lower-left.
std::optional<Direction> OracleDirection(const gui::ViewProjection& v, double x, double y, int w, int h,
                                         bool far_root) {
  x -= w * 0.5;
  y -= h * 0.5;
  const double hf = v.fov * kDeg * 0.5;
  const double radius = std::min(w, h) * 0.5;
  Direction d{};
  const int lens = v.lens_type;
  if (lens == gui::kLensTypeRectangular) {
    const double scale = std::min(w * 0.5, static_cast<double>(h)) / kPi;
    const double alt = y / scale;
    const double az = x / scale;
    if (std::abs(alt) > kPi * 0.5)
      return std::nullopt;
    d = { -std::cos(alt) * std::cos(az), -std::cos(alt) * std::sin(az), -std::sin(alt) };
  } else if (gui::LensIsFullSky(lens)) {
    const double r = std::min(w * 0.5, static_cast<double>(h)) * 0.5;
    const bool left = std::hypot(x + r, y) <= r;
    const double xx = x + (left ? r : -r);
    const double rho = std::hypot(xx, y) / r;
    if (rho > 1.0)
      return std::nullopt;
    const double phi = std::atan2(-y, xx);
    double theta = rho * kPi * 0.5;
    if (lens == gui::kLensTypeDualFisheyeEqualArea)
      theta = 2.0 * std::asin(rho / std::sqrt(2.0));
    if (lens == gui::kLensTypeDualFisheyeStereographic)
      theta = 2.0 * std::atan(rho);
    if (lens == gui::kLensTypeDualFisheyeOrthographic)
      theta = std::asin(rho);
    d = { -std::sin(theta) * std::sin(phi), (left ? 1.0 : -1.0) * std::sin(theta) * std::cos(phi),
          (left ? -1.0 : 1.0) * std::cos(theta) };
  } else {
    Direction local{};
    if (lens == gui::kLensTypeLinear || lens == gui::kLensTypeGlobe) {
      const double f = radius / std::tan(hf);
      const double n = std::sqrt(x * x + y * y + f * f);
      local = { x / n, y / n, -f / n };
      if (lens == gui::kLensTypeGlobe) {
        const double b = 4.0 * local[2];
        const double disc = b * b - 15.0;
        if (disc < 0.0)
          return std::nullopt;
        const double t = -b + (far_root ? 1.0 : -1.0) * std::sqrt(disc);
        local = { t * local[0], t * local[1], 4.0 + t * local[2] };
      }
    } else {
      const double rho = std::hypot(x, y) / radius;
      double theta = 0.0;
      if (lens == gui::kLensTypeFisheyeEqualArea) {
        const double s = rho * std::sin(hf * 0.5);
        if (s > 1.0)
          return std::nullopt;
        theta = 2.0 * std::asin(s);
      } else if (lens == gui::kLensTypeFisheyeEquidist) {
        theta = rho * hf;
        if (theta >= kPi)
          return std::nullopt;
      } else if (lens == gui::kLensTypeFisheyeStereographic) {
        theta = 2.0 * std::atan(rho * std::tan(hf * 0.5));
      } else {
        const double s = rho * std::sin(hf);
        if (s > 1.0)
          return std::nullopt;
        theta = std::asin(s);
      }
      const double phi = std::atan2(y, x);
      local = { std::sin(theta) * std::cos(phi), std::sin(theta) * std::sin(phi), -std::cos(theta) };
    }
    const double a = v.azimuth * kDeg, e = v.elevation * kDeg, r = v.roll * kDeg;
    const Direction right = { std::sin(a), -std::cos(a), 0 };
    const Direction up = { std::sin(e) * std::cos(a), std::sin(e) * std::sin(a), -std::cos(e) };
    const Direction back = { std::cos(e) * std::cos(a), std::cos(e) * std::sin(a), std::sin(e) };
    for (int k = 0; k < 3; ++k)
      d[k] = (local[0] * std::cos(r) - local[1] * std::sin(r)) * right[k] +
             (local[0] * std::sin(r) + local[1] * std::cos(r)) * up[k] + local[2] * back[k];
  }
  if ((v.visible == gui::kVisibleUpper && d[2] > 0.0) || (v.visible == gui::kVisibleLower && d[2] < 0.0))
    return std::nullopt;
  if (v.front) {
    const double a = v.azimuth * kDeg, e = v.elevation * kDeg;
    if (d[0] * std::cos(e) * std::cos(a) + d[1] * std::cos(e) * std::sin(a) + d[2] * std::sin(e) > 0.0)
      return std::nullopt;
  }
  return d;
}

struct Source {
  gui::MapHandle handle;
  LUMICE_PathMapInfo info{};
  LUMICE_PathMapPixels pixels{};
  double vmax = 0.0;
  uint64_t hash = 0;
};
void ExportSource(const Source& source, const std::string& name) {
  if (!g_export_dir)
    return;
  const auto write = [&](const char* suffix, const void* bytes, size_t count) {
    std::ofstream output(GuiTestTempPath(name + suffix), std::ios::binary);
    output.write(static_cast<const char*>(bytes), static_cast<std::streamsize>(count));
  };
  write(".f64", source.pixels.values, source.pixels.pixel_count * sizeof(double));
  write(".i32", source.pixels.counts, source.pixels.pixel_count * sizeof(int));
  write(".u8", source.pixels.status, source.pixels.pixel_count);
}

uint64_t RawHash(const LUMICE_PathMapPixels& p) {
  uint64_t hash = 1469598103934665603ULL;
  const auto bytes = [&](const void* data, size_t count) {
    const auto* b = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < count; ++i) {
      hash ^= b[i];
      hash *= 1099511628211ULL;
    }
  };
  bytes(p.values, p.pixel_count * sizeof(double));
  bytes(p.counts, p.pixel_count * sizeof(int));
  bytes(p.status, p.pixel_count);
  bytes(p.delta_deg, p.pixel_count * sizeof(double));
  bytes(p.azimuth_deg, p.pixel_count * sizeof(double));
  return hash;
}
bool Read(Source& s) {
  s.info.struct_size = sizeof(s.info);
  s.pixels.struct_size = sizeof(s.pixels);
  if (!s.handle || LUMICE_PathMapGetInfo(s.handle.get(), &s.info) != LUMICE_PATH_READ_OK ||
      LUMICE_PathMapGetPixels(s.handle.get(), &s.pixels) != LUMICE_PATH_READ_OK)
    return false;
  std::vector<double> positives;
  for (size_t i = 0; i < s.pixels.pixel_count; ++i)
    if (std::isfinite(s.pixels.values[i]) && s.pixels.values[i] > 0.0)
      positives.push_back(s.pixels.values[i]);
  std::sort(positives.begin(), positives.end());
  if (!positives.empty())
    s.vmax = positives[static_cast<size_t>(std::ceil(0.99 * positives.size())) - 1];
  s.hash = RawHash(s.pixels);
  return true;
}
std::array<double, 2> FrameCoordinates(const Source& s, const Direction& d) {
  double z = 0, x = 0, y = 0;
  for (int k = 0; k < 3; ++k) {
    z += d[k] * s.info.view_incident[k];
    x += d[k] * s.info.view_e1[k];
    y += d[k] * s.info.view_e2[k];
  }
  const double az = std::atan2(y, x) / kDeg;
  return { std::atan2(std::hypot(x, y), z) / kDeg,
           s.info.view_az_min_deg + std::fmod(std::fmod(az - s.info.view_az_min_deg, 360.0) + 360.0, 360.0) };
}
std::optional<size_t> OracleIndex(const Source& s, const Direction& d) {
  const auto p = FrameCoordinates(s, d);
  const auto& i = s.info;
  if (p[0] < i.view_delta_min_deg || p[0] > i.view_delta_max_deg || p[1] > i.view_az_max_deg)
    return std::nullopt;
  const int row = std::clamp(static_cast<int>(std::floor((p[0] - i.view_delta_min_deg) * i.view_rows /
                                                         (i.view_delta_max_deg - i.view_delta_min_deg))),
                             0, i.view_rows - 1);
  const int col = std::clamp(
      static_cast<int>(std::floor((p[1] - i.view_az_min_deg) * i.view_cols / (i.view_az_max_deg - i.view_az_min_deg))),
      0, i.view_cols - 1);
  return static_cast<size_t>(row) * i.view_cols + col;
}

struct Harness {
  gui::MapQuerySession map;
  gui::ViewProjection view;
  int width = 640, height = 480;
  bool requested = false, done = false;
  gui::MapDisplayFrame frame;
  gui::MapDisplayProbe probe;
};
Harness g_probe;

bool ProjectionWitness(ImGuiTestContext* ctx, bool column = false) {
  ResetTestState();
  g_probe = {};
  const std::string path = std::string(LUMICE_E2E_CONFIG_DIR) +
                           (column ? "/raypath_feature_column.json" : "/raypath_feature_random_regular.json");
  std::ifstream input(path);
  nlohmann::json config;
  input >> config;
  config["scene"]["light_source"]["altitude"] = 20.0;
  // The catalogued column variant with free roll, matching the frozen directed-map baseline.
  if (column)
    config["crystal"][0]["axis"]["roll"] = { { "type", "uniform" }, { "mean", 0 }, { "std", 360 } };
  LUMICE_Scene* raw_scene = nullptr;
  IM_CHECK_RETV(LUMICE_SceneFromJson(config.dump().c_str(), &raw_scene) == LUMICE_OK, false);
  std::unique_ptr<LUMICE_Scene, decltype(&LUMICE_SceneDestroy)> scene(raw_scene, LUMICE_SceneDestroy);
  const int faces[] = { 3, 5 };
  const int count = 2;
  LUMICE_PathMapRequest request{};
  request.struct_size = sizeof(request);
  request.crystal_id = 1;
  request.faces = faces;
  request.face_count = 2;
  request.layer_face_counts = &count;
  request.layer_count = 1;
  request.full_sky = 1;
  request.sample_count = 500000;
  char error[512]{};
  LUMICE_PathMap* full = nullptr;
  IM_CHECK_RETV(LUMICE_AnalyzePathMap(scene.get(), &request, &full, error, sizeof(error)) == LUMICE_OK, false);
  g_probe.map.adopted_result = gui::MapHandle(full, gui::MapDeleter{});
  g_probe.map.adopted_id = gui::g_survey_executor.NextId(1770);
  gui::g_state.analysis.map = g_probe.map;  // named consumption fixture; no standing calculation intent
  Source full_source{ g_probe.map.adopted_result };
  IM_CHECK_RETV(Read(full_source), false);
  IM_CHECK_RETV(full_source.info.view_full_sky && full_source.info.members_evaluated == full_source.info.member_count,
                false);
  if (column) {
    double weak = std::numeric_limits<double>::infinity();
    size_t weak_index = 0;
    for (size_t i = 0; i < full_source.pixels.pixel_count; ++i)
      if (full_source.pixels.values[i] > 0.0 && full_source.pixels.values[i] < weak) {
        weak = full_source.pixels.values[i];
        weak_index = i;
      }
    IM_CHECK_RETV(weak > 0.0 && weak < std::numeric_limits<double>::min(), false);
    const double delta = full_source.pixels.delta_deg[weak_index] * kDeg,
                 az = full_source.pixels.azimuth_deg[weak_index] * kDeg;
    Direction direction{};
    for (int k = 0; k < 3; ++k)
      direction[k] =
          std::cos(delta) * full_source.info.view_incident[k] +
          std::sin(delta) * (std::cos(az) * full_source.info.view_e1[k] + std::sin(az) * full_source.info.view_e2[k]);
    const auto weak_read = gui::ReadFrozenMapDirection(g_probe.map, direction.data());
    IM_CHECK_RETV(weak_read && weak_read->source == full_source.handle && weak_read->index == weak_index &&
                      weak_read->pixels.values[weak_index] == weak,
                  false);
    ctx->LogInfo("[C16 quantity] full weak %.17g", weak);
  }
  ExportSource(full_source, column ? "frozen_map_C16_full" : "frozen_map_C01_full");
  // Handles and borrowed columns remain valid across Scene destruction.
  scene.reset();
  size_t verified = 0, far_seen = 0, boundary_only = 0, raw_checked = 0;
  double max_color_error = 0.0;
  for (int lens = 0; lens < gui::kLensTypeCount; ++lens) {
    for (int aspect = 0; aspect < 2; ++aspect) {
      g_probe.width = aspect ? 960 : 640;
      g_probe.view = {};
      g_probe.view.lens_type = lens;
      g_probe.view.fov = lens == gui::kLensTypeGlobe ? 30.0f : 80.0f;
      g_probe.view.elevation = 20.0f;
      g_probe.view.azimuth = 37.0f;
      g_probe.view.roll = gui::EffectiveRollForLens(lens, 21.0f);
      g_probe.view.visible = gui::kVisibleFull;
      g_probe.view.globe_back_fade = lens == gui::kLensTypeGlobe ? 0.5f : 0.0f;
      // Zoomed full-sky cells still use the original grid through the actual product shader.
      for (int arm = 0; arm < (lens == 0 || lens == 2 ? 2 : 1); ++arm) {
        if (arm) {
          g_probe.view.fov = 4.0f;
          g_probe.view.elevation = -2.0f;
          g_probe.view.azimuth = 0.0f;
          g_probe.view.roll = 0.0f;
        }
        const auto verify_frame = [&]() -> bool {
          g_probe.done = false;
          g_probe.requested = true;
          ctx->Yield(2);
          IM_CHECK_RETV(g_probe.done && g_probe.frame.texture != 0, false);
          const int w = g_probe.frame.width, h = g_probe.frame.height;
          IM_CHECK_RETV(g_probe.probe.near_cells.size() == static_cast<size_t>(w) * h, false);
          for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
              const size_t pixel = static_cast<size_t>(y) * w + x;
              for (int root = 0; root < (lens == gui::kLensTypeGlobe ? 2 : 1); ++root) {
                const auto d = OracleDirection(g_probe.view, x + 0.5, y + 0.5, w, h, root != 0);
                const auto& actual = root ? g_probe.probe.far_cells[pixel] : g_probe.probe.near_cells[pixel];
                if (!d)
                  continue;  // boundary validity has a separate exact limb witness
                const Source& source = full_source;
                const auto index = OracleIndex(source, *d);
                if (!index)
                  continue;
                const int slot = 1;
                if (actual[0] != slot || actual[1] != static_cast<float>(*index) ||
                    actual[2] != source.pixels.status[*index]) {
                  // Independent local screen-distance ruler: only a sub-quarter-pixel strip
                  // around a source/grid boundary can excuse float-vs-double index selection.
                  const auto coords = FrameCoordinates(source, *d);
                  const auto dx = OracleDirection(g_probe.view, x + 1.0, y + 0.5, w, h, root != 0);
                  const auto dy = OracleDirection(g_probe.view, x + 0.5, y + 1.0, w, h, root != 0);
                  if (!dx || !dy) {
                    ++boundary_only;
                    continue;
                  }
                  const auto cx = FrameCoordinates(source, *dx), cy = FrameCoordinates(source, *dy);
                  const double dd =
                      (source.info.view_delta_max_deg - source.info.view_delta_min_deg) / source.info.view_rows;
                  const double da = (source.info.view_az_max_deg - source.info.view_az_min_deg) / source.info.view_cols;
                  const double rd = (coords[0] - source.info.view_delta_min_deg) / dd;
                  const double ra = (coords[1] - source.info.view_az_min_deg) / da;
                  const double grad_d = 2.0 * std::hypot(cx[0] - coords[0], cy[0] - coords[0]);
                  const double grad_a = 2.0 * std::hypot(std::remainder(cx[1] - coords[1], 360.0),
                                                         std::remainder(cy[1] - coords[1], 360.0));
                  const double distance = std::min(std::abs(rd - std::round(rd)) * dd / std::max(grad_d, 1e-12),
                                                   std::abs(ra - std::round(ra)) * da / std::max(grad_a, 1e-12));
                  if (distance > 0.25) {
                    ctx->LogError(
                        "lens %d %dx%d root %d pixel %d,%d expected %d/%zu/%u actual %.0f/%.0f/%.0f distance %.6g px",
                        lens, w, h, root, x, y, slot, *index, static_cast<unsigned>(source.pixels.status[*index]),
                        actual[0], actual[1], actual[2], distance);
                    return false;
                  }
                  ++boundary_only;
                  continue;
                }
                if (x % 17 == 0 && y % 13 == 0) {
                  const auto reading = gui::ReadFrozenMapDirection(g_probe.map, d->data());
                  if (!reading || reading->source != source.handle || reading->index != *index ||
                      reading->pixels.counts[reading->index] != source.pixels.counts[*index] ||
                      reading->pixels.status[reading->index] != source.pixels.status[*index] ||
                      std::memcmp(&reading->pixels.values[reading->index], &source.pixels.values[*index],
                                  sizeof(double)) != 0) {
                    ctx->LogError("Raw reader does not name the displayed source/cell at %d,%d root %d", x, y, root);
                    return false;
                  }
                  ++raw_checked;
                }
                ++verified;
                far_seen += root;
                if (!root && lens != gui::kLensTypeGlobe && source.pixels.status[*index] == LUMICE_PATH_MAP_PIXEL_OK) {
                  const double value = source.pixels.values[*index];
                  const double t = std::pow(source.vmax > 0.0 ? std::clamp(value / source.vmax, 0.0, 1.0) : 0.0, 0.6);
                  const double a[3] = { 0.055, 0.065, 0.100 }, b[3] = { 0.30, 0.48, 0.72 }, c[3] = { 0.94, 0.96, 1.0 };
                  for (int k = 0; k < 3; ++k) {
                    const double expected =
                        t < 0.5 ? a[k] + (b[k] - a[k]) * 2.0 * t : b[k] + (c[k] - b[k]) * 2.0 * (t - 0.5);
                    max_color_error = std::max(max_color_error, std::abs(g_probe.probe.color[pixel][k] - expected));
                  }
                }
              }
            }
          }
          return true;
        };
        if (!verify_frame())
          return false;
      }
    }
  }
  // Oversized display pressure changes only the raster sampling. The logical camera and
  // original cells stay unchanged; no source or query is replaced to fit the display budget.
  const auto source_id = g_probe.map.adopted_id;
  g_probe.width = 3200;
  g_probe.height = 2400;
  g_probe.requested = true;
  g_probe.done = false;
  ctx->Yield(2);
  IM_CHECK_RETV(g_probe.done && g_probe.frame.texture && g_probe.frame.display_sampled, false);
  IM_CHECK_RETV(g_probe.frame.width <= 1600 && g_probe.frame.height <= 1200, false);
  IM_CHECK_RETV(g_probe.map.adopted_id == source_id, false);
  const Direction sky_anchor = { -0.7351478631379983, -0.5539736496920532, -0.39073112848927377 };
  const auto original_index = OracleIndex(full_source, sky_anchor);
  const auto raw_direction = gui::ReadFrozenMapDirection(g_probe.map, sky_anchor.data());
  IM_CHECK_RETV(original_index && raw_direction && raw_direction->source == full_source.handle &&
                    raw_direction->index == *original_index,
                false);
  IM_CHECK_RETV(verified > 100000 && far_seen > 1000, false);
  IM_CHECK_RETV(max_color_error <= 1e-6 && raw_checked > 1000, false);
  IM_CHECK_RETV(RawHash(full_source.pixels) == full_source.hash, false);
  ctx->LogInfo("[frozen-map] verified=%zu far=%zu boundary-only=%zu max-color-error=%.12g raw hashes unchanged",
               verified, far_seen, boundary_only, max_color_error);
  gui::g_state.analysis.map = {};
  g_probe = {};
  ctx->Yield(1);
  return true;
}
bool g_upload_preview = false;
void UploadInputFixture() {
  if (!g_upload_preview)
    return;
  g_upload_preview = false;
  std::vector<unsigned char> rgb(64 * 32 * 3);
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 64; ++x) {
      const size_t i = (y * 64 + x) * 3;
      rgb[i] = static_cast<unsigned char>(x * 4);
      rgb[i + 1] = static_cast<unsigned char>(y * 8);
      rgb[i + 2] = static_cast<unsigned char>((x + 3 * y) % 64 * 4);
    }
  gui::g_preview.UploadTexture(rgb.data(), 64, 32);
}

bool SharedInputWitness(ImGuiTestContext* ctx) {
  ResetTestState();
  g_probe = {};
  gui::g_state.renderer.lens_type = gui::kLensTypeLinear;
  gui::g_state.renderer.fov = 80.0f;
  gui::g_state.renderer.azimuth = 37.0f;
  gui::g_state.renderer.elevation = 23.0f;
  gui::g_state.renderer.roll = 21.0f;
  gui::g_state.renderer.visible = gui::kVisibleFull;
  gui::g_state.analyze_raypath_view_open = true;
  g_upload_preview = true;
  ctx->Yield(4);
  IM_CHECK_RETV(gui::g_preview_vp.active, false);
  ImGuiWindow* window = ctx->GetWindowByRef("//Raypath view");
  IM_CHECK_RETV(window != nullptr, false);
  ImGui::SetWindowSize(window, ImVec2(480, 360), ImGuiCond_Always);
  const auto window_origin = ImGui::GetMainViewport()->Pos;
  ImGui::SetWindowPos(window, ImVec2(window_origin.x + 1110, window_origin.y + 580), ImGuiCond_Always);
  ctx->Yield(3);
  auto counter = std::make_shared<std::atomic<int>>(0);
  gui::g_survey_executor.SetPickupGateForTest([counter](gui::QueryFace face) {
    if (face == gui::QueryFace::kMap)
      counter->fetch_add(1);
  });
  struct ClearGate {
    ~ClearGate() { gui::g_survey_executor.SetPickupGateForTest(nullptr); }
  } clear_gate;
  const auto views_match = [&]() {
    const auto& main = gui::g_preview_vp.params.view_proj;
    const auto& map = gui::LastFrozenMapView();
    return main.lens_type == map.lens_type && main.fov == map.fov && main.elevation == map.elevation &&
           main.azimuth == map.azimuth && main.roll == map.roll && main.visible == map.visible &&
           main.front == map.front;
  };
  IM_CHECK_RETV(views_match(), false);
  const auto origin = ImGui::GetMainViewport()->Pos;
  const auto& vp = gui::g_preview_vp;
  const ImVec2 main_center(origin.x + (vp.vp_x + vp.vp_w * 0.5f) / vp.dpi_scale_x,
                           origin.y + ImGui::GetIO().DisplaySize.y - (vp.vp_y + vp.vp_h * 0.5f) / vp.dpi_scale_y);
  const auto drag = [&](const ImVec2& from, const ImVec2& delta) {
    ctx->MouseMoveToPos(from);
    ctx->Yield(2);
    ctx->MouseDown(0);
    ctx->Yield(1);
    ctx->MouseMoveToPos(ImVec2(from.x + delta.x, from.y + delta.y));
    ctx->Yield(2);
    ctx->MouseUp(0);
    ctx->Yield(3);
  };
  const float az_before_main = gui::g_state.renderer.azimuth;
  drag(main_center, ImVec2(30, 10));
  IM_CHECK_RETV(gui::g_state.renderer.azimuth != az_before_main, false);
  IM_CHECK_RETV(views_match(), false);
  ctx->SetRef("//Raypath view");
  const auto map_item = ctx->ItemInfo("**/##map_canvas");
  IM_CHECK_RETV(map_item.ID != 0, false);
  const ImVec2 map_center = map_item.RectFull.GetCenter();
  const float az_before_map = gui::g_state.renderer.azimuth;
  drag(map_center, ImVec2(-20, 8));
  IM_CHECK_RETV(gui::g_state.renderer.azimuth != az_before_map, false);
  IM_CHECK_RETV(views_match(), false);
  const float fov_before = gui::g_state.renderer.fov;
  ctx->MouseMoveToPos(map_center);
  ctx->Yield(2);
  ctx->MouseWheelY(1.0f);
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.renderer.fov == fov_before - 5.0f, false);
  IM_CHECK_RETV(views_match(), false);
  const float az_before_pick = gui::g_state.renderer.azimuth;
  const float el_before_pick = gui::g_state.renderer.elevation;
  ctx->KeyDown(ImGuiMod_Alt);
  ctx->MouseClick(0);
  ctx->KeyUp(ImGuiMod_Alt);
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.valid && gui::g_state.analysis.sky_point.from_map_pick, false);
  IM_CHECK_RETV(gui::g_state.renderer.azimuth == az_before_pick && gui::g_state.renderer.elevation == el_before_pick,
                false);
  const std::array<float, 3> point = { gui::g_state.analysis.sky_point.dir[0], gui::g_state.analysis.sky_point.dir[1],
                                       gui::g_state.analysis.sky_point.dir[2] };
  // The marker owns the next held press; turning off Alt cannot reclassify it as a camera drag.
  drag(map_center, ImVec2(12, 4));
  IM_CHECK_RETV(gui::g_state.analysis.sky_point.dir[0] != point[0] ||
                    gui::g_state.analysis.sky_point.dir[1] != point[1] ||
                    gui::g_state.analysis.sky_point.dir[2] != point[2],
                false);
  IM_CHECK_RETV(gui::g_state.renderer.azimuth == az_before_pick && gui::g_state.renderer.elevation == el_before_pick,
                false);
  IM_CHECK_RETV(views_match(), false);
  const float fov_before_report = gui::g_state.renderer.fov;
  ImGuiWindow* report = nullptr;
  for (ImGuiWindow* candidate : ImGui::GetCurrentContext()->Windows)
    if (candidate->Active && candidate->RootWindow == window && std::strstr(candidate->Name, "##MapReport"))
      report = candidate;
  if (report) {
    ctx->MouseMoveToPos(report->InnerClipRect.GetCenter());
    ctx->Yield(2);
    ctx->MouseWheelY(-2.0f);
    ctx->Yield(3);
    IM_CHECK_RETV(report->ScrollMax.y > 0.0f && report->Scroll.y > 0.0f, false);
  } else
    return false;
  IM_CHECK_RETV(gui::g_state.renderer.fov == fov_before_report, false);
  // A fixed all-sky lens has no camera zoom freedom in either host. The lens arrives as a
  // direct product-state write (the shared presets are retired).
  gui::g_state.renderer.lens_type = gui::kLensTypeRectangular;
  gui::g_state.renderer.visible = gui::kVisibleFull;
  gui::g_state.renderer.front = false;
  ctx->Yield(3);
  IM_CHECK_RETV(views_match(), false);
  const float fixed_fov = gui::g_state.renderer.fov;
  ctx->MouseMoveToPos(map_center);
  ctx->Yield(2);
  ctx->MouseWheelY(2.0f);
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.renderer.fov == fixed_fov, false);
  ctx->WindowClose("//Raypath view");
  ctx->Yield(3);
  gui::g_state.analyze_raypath_view_open = true;
  ctx->Yield(30);  // beyond the retired settle interval
  IM_CHECK_RETV(counter->load() == 0 && !gui::g_survey_executor.QueryOutstanding(gui::QueryFace::kMap), false);
  IM_CHECK_RETV(views_match(), false);
  gui::g_state.analyze_raypath_view_open = false;
  ctx->Yield(2);
  return true;
}

bool PartialPrefixWitness(ImGuiTestContext* ctx) {
  ResetTestState();
  g_probe = {};
  LUMICE_Scene* raw_scene = nullptr;
  const std::string path = std::string(LUMICE_E2E_CONFIG_DIR) + "/raypath_feature_random_regular.json";
  IM_CHECK_RETV(LUMICE_SceneFromJsonFile(path.c_str(), &raw_scene) == LUMICE_OK, false);
  std::unique_ptr<LUMICE_Scene, decltype(&LUMICE_SceneDestroy)> scene(raw_scene, LUMICE_SceneDestroy);
  const int faces[] = { 3, 5 }, counts[] = { 2 };
  LUMICE_PathMapRequest request{};
  request.struct_size = sizeof(request);
  request.crystal_id = 1;
  request.faces = faces;
  request.face_count = 2;
  request.layer_face_counts = counts;
  request.layer_count = 1;
  request.sample_count = 500000;
  const auto query = [&](const LUMICE_PathMapRequest& req) {
    char error[512]{};
    LUMICE_PathMap* raw = nullptr;
    if (LUMICE_AnalyzePathMap(scene.get(), &req, &raw, error, sizeof(error)) != LUMICE_OK) {
      ctx->LogError("Partial fixture request failed: %s", error);
      return gui::MapHandle{};
    }
    return gui::MapHandle(raw, gui::MapDeleter{});
  };
  const auto info_for = [](const gui::MapHandle& map) {
    LUMICE_PathMapInfo info{};
    info.struct_size = sizeof(info);
    if (map)
      LUMICE_PathMapGetInfo(map.get(), &info);
    return info;
  };
  gui::MapHandle full;
  int prefix = 0;
  request.full_sky = 1;
  // A real deadline produces the prefix. The value of K is observed, not prescribed from
  // scheduling luck. Only this real-timing fixture searches; no GUI action auto-retries.
  for (const int budget : { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8000 }) {
    request.budget_ms = budget;
    auto candidate = query(request);
    if (!candidate)
      return false;
    const auto info = info_for(candidate);
    if (info.truncated && info.members_evaluated > 0 && info.members_evaluated < info.member_count) {
      prefix = info.members_evaluated;
      full = std::move(candidate);
      break;
    }
  }
  IM_CHECK_RETV(full != nullptr && prefix > 0, false);
  g_probe.map.adopted_result = full;
  g_probe.map.adopted_id = gui::g_survey_executor.NextId(1771);
  gui::g_state.analysis.map = g_probe.map;
  g_probe.width = 640;
  g_probe.height = 480;
  g_probe.view = {};
  g_probe.view.lens_type = gui::kLensTypeLinear;
  g_probe.view.fov = 4.0f;
  g_probe.view.elevation = -22.0f;
  const auto capture = [&](float altitude) {
    g_probe.view.elevation = altitude;
    g_probe.requested = true;
    g_probe.done = false;
    ctx->Yield(2);
    return g_probe.done && g_probe.frame.texture != 0;
  };
  const auto centre_slot = [&]() {
    return g_probe.probe.near_cells[static_cast<size_t>(g_probe.height / 2) * g_probe.width + g_probe.width / 2][0];
  };
  IM_CHECK_RETV(capture(-22.0f), false);
  IM_CHECK_RETV(g_probe.frame.partial && g_probe.frame.members_evaluated == prefix, false);
  IM_CHECK_RETV(centre_slot() == 1.0f, false);
  const auto reading =
      gui::ReadFrozenMapDirection(g_probe.map, Direction{ -std::cos(22 * kDeg), 0, std::sin(22 * kDeg) }.data());
  IM_CHECK_RETV(reading && reading->source == full && reading->info.members_evaluated == prefix, false);
  IM_CHECK_RETV(capture(-60.0f) && centre_slot() == 1.0f && g_probe.frame.partial, false);
  request.budget_ms = 15000;
  auto complete = query(request);
  IM_CHECK_RETV(complete && !info_for(complete).truncated, false);
  g_probe.map.adopted_result = complete;
  g_probe.map.adopted_id = gui::g_survey_executor.NextId(1771);
  gui::g_state.analysis.map = g_probe.map;
  scene.reset();
  IM_CHECK_RETV(capture(-22.0f) && centre_slot() == 1.0f && !g_probe.frame.partial, false);
  const auto complete_read =
      gui::ReadFrozenMapDirection(g_probe.map, Direction{ -std::cos(22 * kDeg), 0, std::sin(22 * kDeg) }.data());
  IM_CHECK_RETV(complete_read && complete_read->source == complete &&
                    complete_read->info.members_evaluated == complete_read->info.member_count,
                false);
  ctx->LogInfo("[partial-prefix] real K=%d, full-sky partial display and complete replacement verified", prefix);
  gui::g_state.analysis.map = {};
  g_probe = {};
  ctx->Yield(1);
  return true;
}

size_t ModelCapacityBytes(const gui::SurveyOverviewModel& model) {
  const auto text = [](const std::string& value) { return value.capacity() + 1; };
  const auto texts = [&](const std::vector<std::string>& values) {
    size_t bytes = values.capacity() * sizeof(std::string);
    for (const auto& value : values)
      bytes += text(value);
    return bytes;
  };
  size_t bytes = sizeof(model) + text(model.evidence_mode) + text(model.wavelength_source) +
                 text(model.chromatic_note) + texts(model.conclusions);
  const auto& d = model.dispersion;
  bytes +=
      text(d.red_label) + text(d.blue_label) + text(d.direction) + text(d.evidence_basis) + text(d.certificate_formula);
  bytes += d.followed.capacity() * sizeof(gui::SurveyFollowedOnsetView);
  for (const auto& row : d.followed)
    bytes += text(row.source) + text(row.profile) + text(row.direction);
  bytes += text(model.diagnose.verdict_kind) + text(model.diagnose.color) + text(model.diagnose.evidence_basis) +
           texts(model.diagnose.notes);
  bytes += text(model.probe.mode) +
           (model.probe.sample_delta_deg.capacity() + model.probe.sample_value.capacity()) * sizeof(double);
  bytes += text(model.family.refusal_mode) + model.family.levels.capacity() * sizeof(gui::SurveyFamilyLevelView);
  for (const auto& level : model.family.levels)
    bytes += texts(level.frozen_dims) + texts(level.free_dims);
  bytes += text(model.tint.direction) + text(model.tint.caliber) + text(model.tir.mode) +
           model.objects.capacity() * sizeof(gui::SurveyObjectRow);
  for (const auto& row : model.objects)
    bytes += text(row.kind_name) + text(row.state_name) + text(row.reason);
  return bytes;
}
size_t MapGuiCapacityBytes(ImGuiWindow* root) {
  size_t bytes = 0;
  for (ImGuiWindow* window : ImGui::GetCurrentContext()->Windows) {
    bool belongs = false;
    for (auto* parent = window; parent; parent = parent->ParentWindow)
      if (parent == root)
        belongs = true;
    if (!belongs)
      continue;
    bytes += sizeof(*window) + window->NameBufLen + window->IDStack.Capacity * sizeof(ImGuiID) +
             window->StateStorage.Data.Capacity * sizeof(ImGuiStoragePair);
    const auto* list = window->DrawList;
    bytes += sizeof(*list) + list->VtxBuffer.Capacity * sizeof(ImDrawVert) +
             list->IdxBuffer.Capacity * sizeof(ImDrawIdx) + list->CmdBuffer.Capacity * sizeof(ImDrawCmd) +
             list->_Path.Capacity * sizeof(ImVec2) + list->_ClipRectStack.Capacity * sizeof(ImVec4) +
             list->_TextureIdStack.Capacity * sizeof(ImTextureID);
    bytes += list->_Splitter._Channels.Capacity * sizeof(ImDrawChannel);
    for (const auto& channel : list->_Splitter._Channels)
      bytes += channel._CmdBuffer.Capacity * sizeof(ImDrawCmd) + channel._IdxBuffer.Capacity * sizeof(ImDrawIdx);
  }
  return bytes;
}

struct PressureHarness {
  gui::MapQuerySession map;
  gui::MapHandle candidate;
  gui::SurveyHandle survey;
  gui::SurveyQueryId candidate_id;
  std::vector<gui::MapDisplayFrame> adoptions;
  bool prepare = false, prepared = false, replace = false, replaced = false, failed = false, old_released = false;
  std::string renderer, version;
};
PressureHarness g_pressure;
double NativeSeconds() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void PressureGui() {
  UploadInputFixture();
  if (g_pressure.prepare) {
    g_pressure.prepare = false;
    g_pressure.renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    g_pressure.version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    g_pressure.failed = !gui::PrepareFrozenMapSource(g_pressure.map.adopted_result, g_pressure.map.adopted_id);
    g_pressure.adoptions.push_back(gui::LastFrozenMapAdoption());
    gui::g_state.analysis.map = g_pressure.map;
    g_pressure.prepared = true;
  }
  if (g_pressure.replace) {
    g_pressure.replace = false;
    const auto old = std::weak_ptr<const LUMICE_PathMap>(g_pressure.map.adopted_result);
    g_pressure.failed = !gui::PrepareFrozenMapSource(g_pressure.candidate, g_pressure.candidate_id);
    g_pressure.adoptions.push_back(gui::LastFrozenMapAdoption());
    if (!g_pressure.failed) {
      g_pressure.map.adopted_result = std::move(g_pressure.candidate);
      g_pressure.map.adopted_id = g_pressure.candidate_id;
      g_pressure.map.intent_id = g_pressure.candidate_id;
      g_pressure.map.answered_id = g_pressure.candidate_id;
      gui::g_state.analysis.map = g_pressure.map;
      g_pressure.old_released = old.expired();
    }
    g_pressure.replaced = true;
  }
}

// A real C01 map + survey, observed through the product window. Expected curve samples use
// independent double sky geometry, not the GUI forward; the indexed triangles and their own
// command clips are the evidence of ink (legend flags and point counts cannot prove this).
bool OverlayCoverageWitness(ImGuiTestContext* ctx) {
  ResetTestState();
  g_pressure = {};
  std::ifstream input(std::string(LUMICE_E2E_CONFIG_DIR) + "/raypath_feature_random_regular.json");
  nlohmann::json config;
  input >> config;
  config["scene"]["light_source"]["altitude"] = 20.0;
  IM_CHECK_RETV(gui::DeserializeFromJson(config.dump(), gui::g_state), false);
  LUMICE_Scene* raw_scene = nullptr;
  IM_CHECK_RETV(LUMICE_SceneFromJson(config.dump().c_str(), &raw_scene) == LUMICE_OK, false);
  std::unique_ptr<LUMICE_Scene, decltype(&LUMICE_SceneDestroy)> scene(raw_scene, LUMICE_SceneDestroy);
  const int faces[] = { 3, 5 }, counts[] = { 2 };
  LUMICE_PathMapRequest request{};
  request.struct_size = sizeof(request);
  request.crystal_id = 1;
  request.faces = faces;
  request.face_count = 2;
  request.layer_face_counts = counts;
  request.layer_count = 1;
  request.full_sky = 1;
  request.sample_count = 500000;
  char error[512]{};
  LUMICE_PathMap* map = nullptr;
  IM_CHECK_RETV(LUMICE_AnalyzePathMap(scene.get(), &request, &map, error, sizeof(error)) == LUMICE_OK, false);
  g_pressure.map.adopted_result = gui::MapHandle(map, gui::MapDeleter{});
  g_pressure.map.adopted_id = gui::g_survey_executor.NextId(1785);
  LUMICE_PathSurveyRequest survey_request{};
  survey_request.struct_size = sizeof(survey_request);
  survey_request.crystal_id = 1;
  survey_request.faces = faces;
  survey_request.face_count = 2;
  survey_request.layer_face_counts = counts;
  survey_request.layer_count = 1;
  LUMICE_PathSurvey* survey = nullptr;
  IM_CHECK_RETV(LUMICE_AnalyzePathSurvey(scene.get(), &survey_request, &survey, error, sizeof(error)) == LUMICE_OK,
                false);
  g_pressure.survey = gui::SurveyHandle(survey, gui::SurveyDeleter{});
  gui::g_state.analysis.survey.adopted_result = g_pressure.survey;
  gui::g_state.analysis.survey.intent_id = gui::g_survey_executor.NextId(1785);
  const auto model = gui::BuildSurveyOverviewModel(*g_pressure.survey, gui::g_state.analysis.FrozenPointSunPremise());
  IM_CHECK_RETV(model.dispersion.available && model.dispersion.min_row_available && model.sun_available, false);
  // No standing selection/query intent: the adopted, physically computed results are immutable.
  g_pressure.prepare = true;
  auto& renderer = gui::g_state.renderer;
  renderer.lens_type = gui::kLensTypeFisheyeEquidist;
  renderer.fov = 140.0f;
  renderer.azimuth = 80.0f;
  renderer.elevation = 20.0f;
  renderer.roll = 0.0f;
  renderer.visible = gui::kVisibleFull;
  renderer.front = false;
  gui::g_state.analyze_raypath_view_info_ratio = 0.2f;
  gui::g_state.analyze_raypath_view_open = true;
  ctx->Yield(3);
  IM_CHECK_RETV(g_pressure.prepared && !g_pressure.failed, false);
  ImGuiWindow* window = ctx->GetWindowByRef("//Raypath view");
  IM_CHECK_RETV(window != nullptr, false);
  const ImVec2 old_pos = window->Pos, old_size = window->Size;
  struct Restore {
    ImGuiWindow* window;
    ImVec2 pos, size;
    ~Restore() {
      ImGui::SetWindowPos(window, pos, ImGuiCond_Always);
      ImGui::SetWindowSize(window, size, ImGuiCond_Always);
    }
  } restore{ window, old_pos, old_size };
  const auto origin = ImGui::GetMainViewport()->Pos;
  ImGui::SetWindowPos(window, ImVec2(origin.x + 20, origin.y + 45), ImGuiCond_Always);
  ImGui::SetWindowSize(window, ImVec2(1300, 650), ImGuiCond_Always);
  ctx->Yield(3);
  ctx->SetRef("//Raypath view");
  const auto canvas = ctx->ItemInfo("**/##map_canvas");
  IM_CHECK_RETV(canvas.ID != 0, false);
  const auto rect = canvas.RectFull;
  const auto timing = gui::LastMapObservationTiming();
  const auto view = gui::LastFrozenMapView();
  // Read the actual texture referenced by this window's image command on the GL thread.
  // This is the displayed base map, not a made-up fill or a second query/render fixture.
  std::vector<unsigned char> base_pixels;
  g_frame_observer.before_swap = [&] {
    for (ImGuiWindow* region : ImGui::GetCurrentContext()->Windows) {
      if (!region->Active || region->RootWindow != window)
        continue;
      const auto* dl = region->DrawList;
      for (const auto& cmd : dl->CmdBuffer) {
        if (cmd.UserCallback || cmd.ElemCount != 6 || cmd.TextureId == ImGui::GetIO().Fonts->TexID)
          continue;
        const auto& vertex = dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset]];
        if (std::abs(vertex.pos.x - rect.Min.x) < 0.1f && std::abs(vertex.pos.y - rect.Min.y) < 0.1f)
          base_pixels =
              lumice::test::ReadTexturePixels(static_cast<unsigned int>(cmd.TextureId), timing.width, timing.height);
      }
    }
  };
  g_fullframe_capture.Reset();
  g_fullframe_capture.requested.store(true);
  ctx->Yield(2);
  g_frame_observer = {};
  IM_CHECK_RETV(!base_pixels.empty() && g_fullframe_capture.done.load(), false);
  IM_CHECK_RETV(lumice::test::SavePng(GuiTestTempPath("tilted_equidistant_overlay.png").string().c_str(),
                                      g_fullframe_capture.pixels.data(), g_fullframe_capture.width,
                                      g_fullframe_capture.height, 4),
                false);
  const double radius = std::min(timing.width, timing.height) * 0.5;
  const double sx = rect.GetWidth() / timing.width, sy = rect.GetHeight() / timing.height;
  const double a = view.azimuth * kDeg, e = view.elevation * kDeg;
  const auto project = [&](const Direction& position) -> std::optional<ImVec2> {
    const double x = -std::sin(a) * position[0] + std::cos(a) * position[1];
    const double y =
        -std::sin(e) * std::cos(a) * position[0] - std::sin(e) * std::sin(a) * position[1] + std::cos(e) * position[2];
    const double z =
        std::cos(e) * std::cos(a) * position[0] + std::cos(e) * std::sin(a) * position[1] + std::sin(e) * position[2];
    const double theta = std::atan2(std::hypot(x, y), z);
    if (theta >= kPi)
      return std::nullopt;
    const double scale = radius * theta / (view.fov * kDeg * 0.5) / std::max(std::hypot(x, y), 1e-15);
    return ImVec2(static_cast<float>(rect.GetCenter().x + x * scale * sx),
                  static_cast<float>(rect.GetCenter().y - y * scale * sy));
  };
  // Collect only triangles actually indexed by a draw command, keeping its effective clip.
  struct Triangle {
    ImVec2 p[3];
    ImVec4 clip;
    ImU32 color;
  };
  std::vector<Triangle> triangles;
  for (ImGuiWindow* region : ImGui::GetCurrentContext()->Windows) {
    if (!region->Active || region->RootWindow != window)
      continue;
    const auto* dl = region->DrawList;
    for (const auto& cmd : dl->CmdBuffer) {
      if (cmd.UserCallback)
        continue;
      for (unsigned i = 0; i + 2 < cmd.ElemCount; i += 3) {
        Triangle triangle{};
        triangle.clip = cmd.ClipRect;
        for (unsigned k = 0; k < 3; ++k) {
          const auto& vertex = dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + i + k]];
          triangle.p[k] = vertex.pos;
          if ((vertex.col & IM_COL32_A_MASK) != 0)
            triangle.color = vertex.col;
        }
        triangles.push_back(triangle);
      }
    }
  }
  const auto distance = [](ImVec2 p, ImVec2 p0, ImVec2 p1) {
    const double dx = p1.x - p0.x, dy = p1.y - p0.y;
    const double t = std::clamp(((p.x - p0.x) * dx + (p.y - p0.y) * dy) / std::max(dx * dx + dy * dy, 1e-20), 0.0, 1.0);
    return std::hypot(p.x - p0.x - t * dx, p.y - p0.y - t * dy);
  };
  const double alt = model.sun_altitude_deg * kDeg, az = model.sun_azimuth_deg * kDeg;
  const Direction sun = { std::cos(alt) * std::cos(az), std::cos(alt) * std::sin(az), std::sin(alt) };
  // C01's altitude 20 degrees makes the meridional tangent and its azimuthal tangent explicit.
  const Direction up = { -std::sin(alt) * std::cos(az), -std::sin(alt) * std::sin(az), std::cos(alt) };
  const Direction side = { std::sin(az), -std::cos(az), 0 };
  Source source{ g_pressure.map.adopted_result };
  IM_CHECK_RETV(Read(source) && source.vmax > 0.0, false);
  const ImU32 colors[] = { IM_COL32(200, 205, 215, 60), gui::kChromaticColourRed, gui::kChromaticColourBlue };
  const double deltas[] = { 0, model.dispersion.value_red_deg, model.dispersion.value_blue_deg };
  for (int curve = 0; curve < 3; ++curve) {
    const auto check_curve = [&]() -> bool {
      int checked = 0, inside = 0, outside = 0, behind = 0, data = 0;
      std::optional<ImVec2> previous;
      for (int s = 0; s <= 96; ++s) {
        const double phi = 2.0 * kPi * s / 96;
        Direction p{};
        for (int k = 0; k < 3; ++k)
          p[k] = curve == 0 ? (k == 0 ? std::cos(phi) :
                               k == 1 ? std::sin(phi) :
                                        0.0) :
                              std::cos(deltas[curve] * kDeg) * sun[k] +
                                  std::sin(deltas[curve] * kDeg) * (std::cos(phi) * up[k] + std::sin(phi) * side[k]);
        const auto point = project(p);
        if (previous && point) {
          // Intersect the segment with the canvas instead of dropping edge-crossing pairs.
          double lo = 0.0, hi = 1.0;
          const double dx = point->x - previous->x, dy = point->y - previous->y;
          const auto cut = [&](double start, double delta, double lower, double upper) {
            if (std::abs(delta) < 1e-12)
              return start >= lower && start <= upper;
            double t0 = (lower - start) / delta, t1 = (upper - start) / delta;
            if (t0 > t1)
              std::swap(t0, t1);
            lo = std::max(lo, t0);
            hi = std::min(hi, t1);
            return lo <= hi;
          };
          if (cut(previous->x, dx, rect.Min.x, rect.Max.x) && cut(previous->y, dy, rect.Min.y, rect.Max.y)) {
            for (double fraction : { 0.25, 0.5, 0.75 }) {
              const double t = lo + (hi - lo) * fraction;
              const ImVec2 expected(static_cast<float>(previous->x + t * dx), static_cast<float>(previous->y + t * dy));
              double nearest = 1e9;
              for (const auto& triangle : triangles) {
                if (triangle.color != colors[curve] || expected.x < triangle.clip.x || expected.x > triangle.clip.z ||
                    expected.y < triangle.clip.y || expected.y > triangle.clip.w)
                  continue;
                for (int edge = 0; edge < 3; ++edge)
                  nearest = std::min(nearest, distance(expected, triangle.p[edge], triangle.p[(edge + 1) % 3]));
              }
              if (nearest > 2.0) {
                ctx->LogError("curve %d segment %d ink missing at %.3f,%.3f distance %.3f", curve, s, expected.x,
                              expected.y, nearest);
                return false;
              }
              const double px = (expected.x - rect.GetCenter().x) / sx;
              const double py = (rect.GetCenter().y - expected.y) / sy;
              const double theta = std::hypot(px, py) / radius * view.fov * kDeg * 0.5;
              inside += std::hypot(px, py) < radius;
              outside += std::hypot(px, py) > radius + 2;
              behind += theta > kPi * 0.5 + 0.01;
              const auto direction = OracleDirection(view, px + timing.width * 0.5, py + timing.height * 0.5,
                                                     timing.width, timing.height, false);
              const auto read_data = [&]() -> bool {
                IM_CHECK_RETV(direction.has_value(), false);
                const auto index = OracleIndex(source, *direction);
                const int col = std::clamp(static_cast<int>(px + timing.width * 0.5), 0, timing.width - 1);
                const int row = std::clamp(static_cast<int>(timing.height * 0.5 - py), 0, timing.height - 1);
                const size_t offset = (static_cast<size_t>(row) * timing.width + col) * 4;
                data += index && source.pixels.status[*index] == LUMICE_PATH_MAP_PIXEL_OK &&
                        (base_pixels[offset] || base_pixels[offset + 1] || base_pixels[offset + 2]);
                return true;
              };
              if (!read_data())
                return false;
              ++checked;
            }
          }
        }
        previous = point;
      }
      ctx->LogInfo(
          "[overlay coverage] curve=%d points=%d inside=%d outside=%d behind=%d data=%d canvas=%dx%d "
          "camera=140/80/20/0",
          curve, checked, inside, outside, behind, data, timing.width, timing.height);
      IM_CHECK_RETV(checked > 0 && inside > 0 && outside > 0 && behind > 0 && data > 0, false);
      return true;
    };
    if (!check_curve())
      return false;
  }
  gui::g_state.analyze_raypath_view_open = false;
  gui::g_state.analysis = {};
  g_pressure = {};
  ctx->Yield(2);
  return true;
}

bool MaximumTimingWitness(ImGuiTestContext* ctx) {
  ResetTestState();
  IM_CHECK_RETV(!g_enable_fixed_dt, false);
  g_pressure = {};
  g_probe = {};
  std::ifstream input(std::string(LUMICE_E2E_CONFIG_DIR) + "/raypath_feature_random_regular.json");
  nlohmann::json config;
  input >> config;
  IM_CHECK_RETV(gui::DeserializeFromJson(config.dump(), gui::g_state), false);
  LUMICE_Scene* raw_scene = nullptr;
  IM_CHECK_RETV(LUMICE_SceneFromJson(config.dump().c_str(), &raw_scene) == LUMICE_OK, false);
  std::unique_ptr<LUMICE_Scene, decltype(&LUMICE_SceneDestroy)> scene(raw_scene, LUMICE_SceneDestroy);
  const auto query = [&](int slot) {
    auto request = lumice::test::MaximumMapRequest(slot);
    request.full_sky = 1;  // replacement also exercises the sole full-sky source
    char error[512]{};
    LUMICE_PathMap* raw = nullptr;
    if (LUMICE_AnalyzePathMap(scene.get(), &request, &raw, error, sizeof(error)) != LUMICE_OK) {
      ctx->LogError("Maximum source query failed: %s", error);
      return gui::MapHandle{};
    }
    return gui::MapHandle(raw, gui::MapDeleter{});
  };
  g_pressure.map.adopted_result = query(0);
  IM_CHECK_RETV(g_pressure.map.adopted_result, false);
  g_pressure.map.adopted_id = gui::g_survey_executor.NextId(1772);
  // Seed a named consumer binding from the real C map identity, not an invented MC share.
  auto payload = std::make_shared<gui::AnalysisPayload>();
  payload->snapshot_generation = 1772;
  LUMICE_RaypathHistogramEntry row{};
  row.chain_len = 1;
  row.chain[0].crystal_id = 1;
  row.chain[0].segment_len = 2;
  row.chain[0].segment[0] = 3;
  row.chain[0].segment[1] = 5;
  std::snprintf(row.display, sizeof(row.display), "3-5");
  payload->entries.push_back(row);
  gui::g_state.analysis_result.payload = payload;
  gui::g_state.analysis_result.analyzed_scene = gui::GuiState::AnalysisSceneIdentity::From(gui::g_state);
  gui::g_state.analysis.selected_entry = "3-5";
  const auto identity = gui::SelectedClassIdentityFor(gui::g_state);
  IM_CHECK_RETV(identity.has_value(), false);
  const auto selection = gui::SurveySelectionKeyFor(*identity, 1772);
  g_pressure.map.intent_valid = true;
  g_pressure.map.key.selection = selection;
  g_pressure.map.intent_id = g_pressure.map.adopted_id;
  g_pressure.map.answered_id = g_pressure.map.adopted_id;
  gui::g_state.analysis.map = g_pressure.map;
  LUMICE_PathSurveyRequest survey_request{};
  survey_request.struct_size = sizeof(survey_request);
  const int faces[] = { 3, 5 }, counts[] = { 2 };
  survey_request.crystal_id = 1;
  survey_request.faces = faces;
  survey_request.face_count = 2;
  survey_request.layer_face_counts = counts;
  survey_request.layer_count = 1;
  LUMICE_PathSurvey* raw_survey = nullptr;
  char error[512]{};
  IM_CHECK_RETV(LUMICE_AnalyzePathSurvey(scene.get(), &survey_request, &raw_survey, error, sizeof(error)) == LUMICE_OK,
                false);
  g_pressure.survey = gui::SurveyHandle(raw_survey, gui::SurveyDeleter{});
  auto& survey = gui::g_state.analysis.survey;
  survey.intent_valid = true;
  survey.key = selection;
  survey.intent_id = gui::g_survey_executor.NextId(1772);
  survey.adopted_result = g_pressure.survey;
  gui::g_state.renderer.lens_type = gui::kLensTypeGlobe;
  gui::g_state.renderer.fov = 30.0f;
  gui::g_state.renderer.elevation = 0.0f;
  gui::g_state.renderer.azimuth = 180.0f;
  gui::g_state.renderer.roll = 0.0f;
  gui::g_state.renderer.visible = gui::kVisibleFull;
  gui::g_state.renderer.front = false;
  gui::g_state.renderer.globe_back_fade = 0.5f;
  // Preparation starts before the first GetPixels: do not pre-materialize C columns here.
  g_pressure.prepare = true;
  g_upload_preview = true;
  ctx->Yield(3);
  IM_CHECK_RETV(g_pressure.prepared && !g_pressure.failed, false);
  LUMICE_PathMapPixels dense{};
  dense.struct_size = sizeof(dense);
  IM_CHECK_RETV(LUMICE_PathMapGetPixels(g_pressure.map.adopted_result.get(), &dense) == LUMICE_PATH_READ_OK, false);
  IM_CHECK_RETV(dense.pixel_count == LUMICE_PATH_MAP_MAX_PIXELS, false);
  size_t positive = 0;
  for (size_t i = 0; i < dense.pixel_count; ++i)
    positive += dense.values[i] > 0.0 && std::isfinite(dense.values[i]);
  IM_CHECK_RETV(positive > 0, false);  // full sky includes finite-zero cells as well as positive bands
  g_pressure.candidate = query(2);
  g_pressure.candidate_id = gui::g_survey_executor.NextId(1772);
  IM_CHECK_RETV(g_pressure.candidate != nullptr, false);
  g_pressure.replace = true;
  ctx->Yield(3);
  IM_CHECK_RETV(g_pressure.replaced && !g_pressure.failed && g_pressure.old_released, false);
  scene.reset();
  const ScopedWindowBasis native_basis(ctx);
  const auto framebuffer_scale = ImGui::GetIO().DisplayFramebufferScale;
  g_window_size_test.width = std::max(gui::kInitWindowWidth, static_cast<int>(std::ceil(2560.0 / framebuffer_scale.x)));
  g_window_size_test.height =
      std::max(gui::kInitWindowHeight, static_cast<int>(std::ceil(1600.0 / framebuffer_scale.y)));
  g_window_size_test.done.store(false);
  g_window_size_test.requested.store(true);
  for (int frame = 0; frame < 30 && !g_window_size_test.done.load(); ++frame)
    ctx->Yield(1);
  IM_CHECK_RETV(g_window_size_test.done.load(), false);
  ctx->Yield(3);
  gui::g_state.analyze_raypath_view_open = true;
  ctx->Yield(3);
  ImGuiWindow* window = ctx->GetWindowByRef("//Raypath view");
  IM_CHECK_RETV(window != nullptr, false);
  const ImVec2 old_pos = window->Pos, old_size = window->Size;
  struct RestoreWindow {
    ImGuiWindow* window;
    ImVec2 pos, size;
    ~RestoreWindow() {
      ImGui::SetWindowPos(window, pos, ImGuiCond_Always);
      ImGui::SetWindowSize(window, size, ImGuiCond_Always);
    }
  } restore_window{ window, old_pos, old_size };
  const auto origin = ImGui::GetMainViewport()->Pos;
  ImGui::SetWindowPos(window, ImVec2(origin.x + 8, origin.y + 45), ImGuiCond_Always);
  ImGui::SetWindowSize(window, ImVec2(1180, 730), ImGuiCond_Always);
  ctx->SetRef("//Raypath view");
  for (int adjustment = 0; adjustment < 4; ++adjustment) {
    ctx->Yield(2);
    const auto canvas = ctx->ItemInfo("**/##map_canvas");
    if (canvas.ID == 0)
      return false;
    const auto timing = gui::LastMapObservationTiming();
    const double sx = timing.width / canvas.RectFull.GetWidth(), sy = timing.height / canvas.RectFull.GetHeight();
    const double dw = (1600 - timing.width) / sx / (1.0 - gui::g_state.analyze_raypath_view_info_ratio);
    const double dh = (1200 - timing.height) / sy;
    if (std::abs(dw) < 0.25 && std::abs(dh) < 0.25)
      break;
    ImGui::SetWindowSize(window,
                         ImVec2(window->Size.x + static_cast<float>(dw), window->Size.y + static_cast<float>(dh)),
                         ImGuiCond_Always);
  }
  ctx->Yield(3);
  const auto timing = gui::LastMapObservationTiming();
  ctx->LogInfo("[maximum geometry] canvas %dx%d; viewport %.0fx%.0f; native %.0fx%.0f", timing.width, timing.height,
               ImGui::GetMainViewport()->Size.x, ImGui::GetMainViewport()->Size.y, ImGui::GetIO().DisplaySize.x,
               ImGui::GetIO().DisplaySize.y);
  IM_CHECK_RETV(timing.width <= 1600 && timing.height <= 1200 && timing.width >= 1598 && timing.height >= 1198, false);
  const auto legend = gui::LastMapOverlayLegend();
  IM_CHECK_RETV(legend.anchor_sun || legend.anchor_horizon || legend.dispersion_red, false);
  const auto canvas = ctx->ItemInfo("**/##map_canvas");
  const ImVec2 centre = canvas.RectFull.GetCenter();
  ctx->MouseMoveToPos(centre);
  ctx->Yield(2);
  ctx->MouseDown(0);
  ctx->Yield(2);
  struct Sample {
    int ordinal = 0;
    ImVec2 pointer{};
    double input = 0, consumed = 0, present = 0, observe_ms = 0;
    gui::ViewProjection expected;
    bool moved = false;
  };
  std::vector<Sample> samples;
  std::vector<double> frame_observation;
  size_t gui_capacity_peak = 0;
  nlohmann::json frame_trace = nlohmann::json::array();
  int issued = 0;
  bool measuring = true;
  const auto same = [](const gui::ViewProjection& a, const gui::ViewProjection& b) {
    return a.lens_type == b.lens_type && a.fov == b.fov && a.elevation == b.elevation && a.azimuth == b.azimuth &&
           a.roll == b.roll;
  };
  const auto original_run_flags = ctx->RunFlags;
  ctx->RunFlags |= ImGuiTestRunFlags_EnableRawInputs;
  struct ClearObserver {
    ImGuiTestContext* ctx;
    ImGuiTestRunFlags flags;
    ~ClearObserver() {
      g_frame_observer = {};
      ctx->RunFlags = flags;
    }
  } clear_observer{ ctx, original_run_flags };
  g_frame_observer.before_new_frame = [&] {
    if (!measuring || issued >= 124)
      return;
    const float angle = issued * 0.071f;
    Sample sample;
    sample.ordinal = issued++;
    // ImGui::UpdateMouseInputs floors logical coordinates. Admit that exact integer event
    // rather than matching a fractional request against a different IO coordinate.
    sample.pointer =
        ImVec2(std::floor(centre.x + 35.0f * std::cos(angle)), std::floor(centre.y + 23.0f * std::sin(angle)));
    sample.input = NativeSeconds();
    samples.push_back(sample);
    auto& context = *ImGui::GetCurrentContext();
    const int first_event = context.InputEventsQueue.Size;
    ImGui::GetIO().AddMousePosEvent(sample.pointer.x, sample.pointer.y);
    ImGui::GetIO().AddMouseButtonEvent(0, true);
    // The engine marks raw TestFunc events this way too. These events arrive in the
    // pre-NewFrame harness hook, so tag only this admitted batch, not backend/user events.
    for (int i = first_event; i < context.InputEventsQueue.Size; ++i)
      context.InputEventsQueue[i].AddedByTestEngine = true;
  };
  g_frame_observer.before_swap = [&] {
    if (!measuring || samples.empty())
      return;
    glFinish();  // actual map+overlay+main+ImGui GPU completion, not a CPU setter timestamp
    const double end = NativeSeconds();
    gui_capacity_peak = std::max(gui_capacity_peak, MapGuiCapacityBytes(window));
    const auto cost = gui::LastMapObservationTiming();
    const double elapsed = (end - cost.began_seconds) * 1000.0;
    frame_observation.push_back(elapsed);
    const auto actual = gui::BuildPreviewViewProjFromRenderer(gui::g_state.renderer);
    const auto& io = ImGui::GetIO();
    // Match the event actually consumed by IO, not simply the newest queued event. The
    // queue may defer admission across a frame; that waiting time remains in input->present.
    for (auto& sample : samples)
      if (sample.consumed == 0 && std::abs(io.MousePos.x - sample.pointer.x) < 0.1f &&
          std::abs(io.MousePos.y - sample.pointer.y) < 0.1f) {
        sample.consumed = end;
        sample.expected = actual;
        sample.moved = !same(actual, gui::LastFrozenMapView());
        sample.observe_ms = elapsed;
        break;
      }
    if (frame_trace.size() < 160)
      frame_trace.push_back({ { "io_x", io.MousePos.x },
                              { "io_y", io.MousePos.y },
                              { "delta_x", io.MouseDelta.x },
                              { "delta_y", io.MouseDelta.y },
                              { "down", io.MouseDown[0] },
                              { "owner", static_cast<int>(gui::g_state.workspace.map_press_owner) },
                              { "active_id", ImGui::GetCurrentContext()->ActiveId },
                              { "pose_az", actual.azimuth },
                              { "pose_el", actual.elevation },
                              { "drawn_az", gui::LastFrozenMapView().azimuth },
                              { "drawn_el", gui::LastFrozenMapView().elevation } });
  };
  g_frame_observer.after_swap = [&] {
    if (!measuring)
      return;
    const double now = NativeSeconds();
    for (auto& sample : samples)
      if (sample.present == 0 && sample.moved && same(sample.expected, gui::LastFrozenMapView()) &&
          same(sample.expected, gui::g_preview_vp.params.view_proj))
        sample.present = now;
  };
  const double deadline = NativeSeconds() + 20.0;
  while ((samples.size() < 124 || samples.back().present == 0) && NativeSeconds() < deadline)
    ctx->Yield(1);
  measuring = false;
  g_frame_observer = {};
  ctx->RunFlags = original_run_flags;
  ctx->MouseUp(0);
  ctx->Yield(2);
  std::vector<double> latencies, observation;
  nlohmann::json raw = nlohmann::json::array(), adoptions = nlohmann::json::array();
  for (const auto& sample : samples) {
    if (sample.moved && sample.present > 0)
      latencies.push_back((sample.present - sample.input) * 1000.0);
    observation.push_back(sample.observe_ms);
    raw.push_back({ { "ordinal", sample.ordinal },
                    { "input_s", sample.input },
                    { "consumed_s", sample.consumed },
                    { "present_s", sample.present },
                    { "requested_x", sample.pointer.x },
                    { "requested_y", sample.pointer.y },
                    { "moved", sample.moved },
                    { "observation_upper_ms", sample.observe_ms } });
  }
  for (const auto& record : g_pressure.adoptions)
    adoptions.push_back({ { "serial", record.source_adopted_id.serial },
                          { "ms", record.adoption_ms },
                          { "source_gpu_bytes", record.source_gpu_bytes },
                          { "source_gpu_peak_bytes", record.source_gpu_peak_bytes },
                          { "sort_peak_bytes", record.sort_peak_bytes },
                          { "staging_peak_bytes", record.staging_peak_bytes } });
  std::sort(latencies.begin(), latencies.end());
  const double p95 = latencies.empty() ? 1e9 : latencies[static_cast<size_t>(std::ceil(latencies.size() * 0.95)) - 1];
  const double max_latency = latencies.empty() ? 1e9 : latencies.back();
  const double max_observe =
      frame_observation.empty() ? 1e9 : *std::max_element(frame_observation.begin(), frame_observation.end());
  nlohmann::json report = {
    { "scope", "maximum C-result consumer fixture; actual shared frame + visible-window input/swap path" },
    { "endpoint",
      "pre-NewFrame event admission to first matching shared-camera frame after glFinish + glfwSwapBuffers return; not "
      "photon arrival" },
    { "renderer", g_pressure.renderer },
    { "gl_version", g_pressure.version },
    { "visible", g_enable_visible },
    { "vsync", g_enable_vsync },
    { "frame_limit", g_enable_frame_limit },
    { "fixed_dt", g_enable_fixed_dt },
    { "map_width", timing.width },
    { "map_height", timing.height },
    { "source_cells", dense.pixel_count },
    { "dense_positive_cells", positive },
    { "map_gui_capacity_peak_bytes", gui_capacity_peak },
    { "overlay_model_capacity_bytes", ModelCapacityBytes(gui::BuildSurveyOverviewModel(
                                          *g_pressure.survey, gui::g_state.analysis.FrozenPointSunPremise())) },
    { "adoptions", adoptions },
    { "input_frames", latencies.size() },
    { "latency_p95_ms", p95 },
    { "latency_max_ms", max_latency },
    { "observation_max_upper_ms", max_observe },
    { "old_source_released", g_pressure.old_released },
    { "frame_trace", frame_trace },
    { "samples", raw }
  };
  report["peak_rss_bytes"] = nullptr;
#if defined(__APPLE__) || defined(__linux__)
  struct rusage usage {};
  if (getrusage(RUSAGE_SELF, &usage) == 0) {
#if defined(__APPLE__)
    report["peak_rss_bytes"] = usage.ru_maxrss;
#else
    report["peak_rss_bytes"] = static_cast<unsigned long long>(usage.ru_maxrss) * 1024;
#endif
  }
#endif
  report["rss_scope"] = "whole process incl. physical-query temporaries/driver/UI; separate from net map payload";
  std::ofstream(GuiTestTempPath("frozen_map_maximum_timing.json")) << report.dump(2) << '\n';
  ctx->LogInfo("[frozen-map-timing] frames=%zu p95=%.6g ms max=%.6g ms observation-upper=%.6g ms", latencies.size(),
               p95, max_latency, max_observe);
  IM_CHECK_RETV(latencies.size() >= 120, false);
  IM_CHECK_RETV(p95 <= 33.4 && max_latency <= 100.0 && max_observe <= 8.0, false);
  for (const auto& record : g_pressure.adoptions)
    if (record.adoption_ms > 100.0)
      return false;
  gui::g_state.analyze_raypath_view_open = false;
  gui::g_state.analysis = {};
  gui::g_state.analysis_result = {};
  g_pressure = {};
  ctx->Yield(2);
  return true;
}

}  // namespace

void RegisterFrozenMapTests(ImGuiTestEngine* engine) {
  ImGuiTest* t = IM_REGISTER_TEST(engine, "frozen_map", "independent_projection_cells_and_raw_values");
  t->GuiFunc = [](ImGuiTestContext*) {
    if (g_probe.requested) {
      g_probe.requested = false;
      g_probe.frame = gui::DrawFrozenMap(g_probe.map, g_probe.view, g_probe.width, g_probe.height, &g_probe.probe);
      g_probe.done = true;
    }
  };
  t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(ProjectionWitness(ctx)); };
  t = IM_REGISTER_TEST(engine, "frozen_map", "shared_camera_real_input_and_observation_only");
  t->GuiFunc = [](ImGuiTestContext*) { UploadInputFixture(); };
  t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(SharedInputWitness(ctx)); };
  t = IM_REGISTER_TEST(engine, "frozen_map", "independent_column_strong_weak_and_raw_values");
  t->GuiFunc = [](ImGuiTestContext*) {
    if (g_probe.requested) {
      g_probe.requested = false;
      g_probe.frame = gui::DrawFrozenMap(g_probe.map, g_probe.view, g_probe.width, g_probe.height, &g_probe.probe);
      g_probe.done = true;
    }
  };
  t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(ProjectionWitness(ctx, true)); };
  t = IM_REGISTER_TEST(engine, "perf_test", "partial_prefix_frozen_full_sky");
  t->GuiFunc = [](ImGuiTestContext*) {
    if (g_probe.requested) {
      g_probe.requested = false;
      g_probe.frame = gui::DrawFrozenMap(g_probe.map, g_probe.view, g_probe.width, g_probe.height, &g_probe.probe);
      g_probe.done = true;
    }
  };
  t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(PartialPrefixWitness(ctx)); };
  t = IM_REGISTER_TEST(engine, "frozen_map", "tilted_equidistant_overlay_coverage");
  t->GuiFunc = [](ImGuiTestContext*) { PressureGui(); };
  t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(OverlayCoverageWitness(ctx)); };
  t = IM_REGISTER_TEST(engine, "perf_test", "maximum_frozen_map_input_to_present");
  t->GuiFunc = [](ImGuiTestContext*) { PressureGui(); };
  t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(MaximumTimingWitness(ctx)); };
}
