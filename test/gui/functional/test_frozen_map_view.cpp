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
  // The same physical operator, an explicit fine window about the lower edge of C01.
  request.full_sky = 0;
  request.centre_altitude_deg = -2.0;
  request.delta_span_deg = 4.0;
  request.azimuth_span_deg = 8.0;
  request.view_rows = 200;
  request.view_cols = 400;
  LUMICE_PathMap* detail = nullptr;
  IM_CHECK_RETV(LUMICE_AnalyzePathMap(scene.get(), &request, &detail, error, sizeof(error)) == LUMICE_OK, false);
  g_probe.map.detail_result = gui::MapHandle(detail, gui::MapDeleter{});
  g_probe.map.detail_id = gui::g_survey_executor.NextId(1770);
  gui::g_state.analysis.map = g_probe.map;  // named consumption fixture; no standing calculation intent
  Source full_source{ g_probe.map.adopted_result }, detail_source{ g_probe.map.detail_result };
  IM_CHECK_RETV(Read(full_source) && Read(detail_source), false);
  IM_CHECK_RETV(full_source.info.members_evaluated == full_source.info.member_count, false);
  IM_CHECK_RETV(detail_source.info.members_evaluated == detail_source.info.member_count, false);
  IM_CHECK_RETV(detail_source.pixels.pixel_count == 80000, false);
  size_t positives = 0;
  double first_positive_delta = 180.0, min_positive = 1.0;
  for (size_t i = 0; i < detail_source.pixels.pixel_count; ++i) {
    if (detail_source.pixels.values[i] > 0.0) {
      ++positives;
      first_positive_delta = std::min(first_positive_delta, detail_source.pixels.delta_deg[i]);
      min_positive = std::min(min_positive, detail_source.pixels.values[i]);
    }
  }
  const double closed_edge = (2.0 * std::asin(detail_source.info.refractive_index * 0.5) / kDeg) - 60.0;
  const double delta_cell =
      (detail_source.info.view_delta_max_deg - detail_source.info.view_delta_min_deg) / detail_source.info.view_rows;
  IM_CHECK_RETV(std::abs(first_positive_delta - 21.91) < 1e-9, false);
  IM_CHECK_RETV(
      closed_edge >= first_positive_delta - delta_cell * 0.5 && closed_edge < first_positive_delta + delta_cell * 0.5,
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
    ctx->LogInfo("[C16 quantity] full weak %.17g; fine minimum positive %.17g", weak, min_positive);
  }
  IM_CHECK_RETV(positives > 0, false);
  ExportSource(full_source, column ? "frozen_map_C16_full" : "frozen_map_C01_full");
  ExportSource(detail_source, column ? "frozen_map_C16_fine" : "frozen_map_C01_fine");
  // Handles and borrowed columns remain valid across Scene destruction.
  scene.reset();
  size_t verified = 0, detail_seen = 0, far_seen = 0, boundary_only = 0, raw_checked = 0;
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
      // Additional fine-edge arm through the actual product shader, not a coarse upscale.
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
                const auto fine = OracleIndex(detail_source, *d);
                const Source& source = fine ? detail_source : full_source;
                const auto index = fine ? fine : OracleIndex(full_source, *d);
                if (!index)
                  continue;
                const int slot = fine ? 2 : 1;
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
                detail_seen += fine.has_value();
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
  IM_CHECK_RETV(verified > 100000 && detail_seen > 1000 && far_seen > 1000, false);
  IM_CHECK_RETV(max_color_error <= 1e-6 && raw_checked > 1000, false);
  IM_CHECK_RETV(RawHash(full_source.pixels) == full_source.hash && RawHash(detail_source.pixels) == detail_source.hash,
                false);
  ctx->LogInfo(
      "[frozen-map] verified=%zu detail=%zu far=%zu boundary-only=%zu max-color-error=%.12g raw hashes unchanged",
      verified, detail_seen, far_seen, boundary_only, max_color_error);
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
  // A fixed all-sky lens has no camera zoom freedom in either host.
  ctx->ItemClick("**/Full sky##map");
  ctx->Yield(3);
  IM_CHECK_RETV(gui::g_state.renderer.lens_type == gui::kLensTypeRectangular && views_match(), false);
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
  int prefix = 0, full_budget = 0;
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
      full_budget = budget;
      full = std::move(candidate);
      break;
    }
  }
  IM_CHECK_RETV(full != nullptr && prefix > 0, false);
  const auto frozen_frame = info_for(full);
  request.full_sky = 0;
  request.centre_altitude_deg = -22.0;
  request.delta_span_deg = 4.0;
  request.azimuth_span_deg = 8.0;
  request.view_rows = 200;
  request.view_cols = 400;
  const auto matching_detail = [&](double azimuth) {
    int low = 1, high = 15000, budget = full_budget;
    const double delta = 22.0 * kDeg, phi = azimuth * kDeg;
    Direction travel{};
    for (int k = 0; k < 3; ++k)
      travel[k] = std::cos(delta) * frozen_frame.view_incident[k] +
                  std::sin(delta) * (std::cos(phi) * frozen_frame.view_e1[k] + std::sin(phi) * frozen_frame.view_e2[k]);
    request.centre_altitude_deg = std::asin(-travel[2]) / kDeg;
    request.centre_azimuth_deg = std::atan2(-travel[1], -travel[0]) / kDeg;
    for (int attempt = 0; attempt < 14 && low <= high; ++attempt) {
      request.budget_ms = budget;
      auto candidate = query(request);
      if (!candidate)
        return gui::MapHandle{};
      const auto info = info_for(candidate);
      if (info.truncated && info.members_evaluated == prefix)
        return candidate;
      if (info.members_evaluated < prefix)
        low = budget + 1;
      else
        high = budget - 1;
      budget = low + (high - low) / 2;
    }
    return gui::MapHandle{};
  };
  auto detail = matching_detail(0.0);
  IM_CHECK_RETV(detail != nullptr, false);
  const auto original_detail_request = request;
  g_probe.map.adopted_result = full;
  g_probe.map.adopted_id = gui::g_survey_executor.NextId(1771);
  g_probe.map.detail_result = detail;
  g_probe.map.detail_id = gui::g_survey_executor.NextId(1771);
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
  IM_CHECK_RETV(g_probe.frame.compatible && g_probe.frame.partial && g_probe.frame.members_evaluated == prefix, false);
  IM_CHECK_RETV(centre_slot() == 2.0f, false);
  const auto detail_read =
      gui::ReadFrozenMapDirection(g_probe.map, Direction{ -std::cos(22 * kDeg), 0, std::sin(22 * kDeg) }.data());
  IM_CHECK_RETV(detail_read && detail_read->source == detail && detail_read->info.members_evaluated == prefix, false);
  IM_CHECK_RETV(capture(-60.0f) && centre_slot() == 1.0f && g_probe.frame.partial, false);
  auto reference_read = gui::ReadFrozenMapDirection(g_probe.map, Direction{ -0.5, 0, std::sqrt(0.75) }.data());
  IM_CHECK_RETV(reference_read && reference_read->source == full && reference_read->info.members_evaluated == prefix,
                false);
  // Replace the one detail with a disjoint cap, using the same real completed prefix.
  auto next = matching_detail(90.0);
  IM_CHECK_RETV(next != nullptr && next != detail, false);
  g_probe.map.detail_result = next;
  g_probe.map.detail_id = gui::g_survey_executor.NextId(1771);
  gui::g_state.analysis.map = g_probe.map;
  detail.reset();
  IM_CHECK_RETV(capture(-22.0f) && centre_slot() == 1.0f && g_probe.frame.partial, false);
  g_probe.map.source_mode = 2;
  gui::g_state.analysis.map = g_probe.map;
  IM_CHECK_RETV(capture(-22.0f) && centre_slot() == 0.0f, false);
  IM_CHECK_RETV(
      !gui::ReadFrozenMapDirection(g_probe.map, Direction{ -std::cos(22 * kDeg), 0, std::sin(22 * kDeg) }.data()),
      false);
  // Both mismatch directions are real C results, not fabricated metadata edits.
  request = original_detail_request;
  request.budget_ms = 15000;
  auto complete_detail = query(request);
  IM_CHECK_RETV(complete_detail && !info_for(complete_detail).truncated, false);
  g_probe.map.detail_result = complete_detail;
  g_probe.map.detail_id = gui::g_survey_executor.NextId(1771);
  g_probe.map.source_mode = 0;
  gui::g_state.analysis.map = g_probe.map;
  next.reset();
  IM_CHECK_RETV(capture(-22.0f) && !g_probe.frame.compatible && centre_slot() == 1.0f && g_probe.frame.partial, false);
  request.full_sky = 1;
  request.view_rows = 0;
  request.view_cols = 0;
  auto complete_full = query(request);
  IM_CHECK_RETV(complete_full && !info_for(complete_full).truncated, false);
  g_probe.map.adopted_result = complete_full;
  g_probe.map.adopted_id = gui::g_survey_executor.NextId(1771);
  gui::g_state.analysis.map = g_probe.map;
  full.reset();
  request.full_sky = 0;
  request.view_rows = 200;
  request.view_cols = 400;
  detail = matching_detail(0.0);
  IM_CHECK_RETV(detail != nullptr, false);
  g_probe.map.detail_result = detail;
  g_probe.map.detail_id = gui::g_survey_executor.NextId(1771);
  gui::g_state.analysis.map = g_probe.map;
  complete_detail.reset();
  scene.reset();
  IM_CHECK_RETV(capture(-22.0f) && !g_probe.frame.compatible && centre_slot() == 1.0f && !g_probe.frame.partial, false);
  g_probe.map.source_mode = 2;
  gui::g_state.analysis.map = g_probe.map;
  IM_CHECK_RETV(capture(-22.0f) && centre_slot() == 2.0f && g_probe.frame.partial, false);
  IM_CHECK_RETV(capture(-60.0f) && centre_slot() == 0.0f, false);
  ctx->LogInfo(
      "[partial-prefix] real K=%d, full budget=%d ms; same-prefix composition, latest replacement and both mismatches "
      "verified",
      prefix, full_budget);
  gui::g_state.analysis.map = {};
  g_probe = {};
  ctx->Yield(1);
  return true;
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
    g_pressure.failed = !gui::PrepareFrozenMapSource(g_pressure.map.adopted_result, g_pressure.map.adopted_id, false);
    g_pressure.adoptions.push_back(gui::LastFrozenMapAdoption());
    if (!g_pressure.failed)
      g_pressure.failed = !gui::PrepareFrozenMapSource(g_pressure.map.detail_result, g_pressure.map.detail_id, true);
    g_pressure.adoptions.push_back(gui::LastFrozenMapAdoption());
    gui::g_state.analysis.map = g_pressure.map;
    g_pressure.prepared = true;
  }
  if (g_pressure.replace) {
    g_pressure.replace = false;
    const auto old = std::weak_ptr<const LUMICE_PathMap>(g_pressure.map.detail_result);
    g_pressure.failed = !gui::PrepareFrozenMapSource(g_pressure.candidate, g_pressure.candidate_id, true);
    g_pressure.adoptions.push_back(gui::LastFrozenMapAdoption());
    if (!g_pressure.failed) {
      g_pressure.map.detail_result = std::move(g_pressure.candidate);
      g_pressure.map.detail_id = g_pressure.candidate_id;
      g_pressure.map.intent_id = g_pressure.candidate_id;
      g_pressure.map.answered_id = g_pressure.candidate_id;
      gui::g_state.analysis.map = g_pressure.map;
      g_pressure.old_released = old.expired();
    }
    g_pressure.replaced = true;
  }
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
    const auto request = lumice::test::MaximumMapRequest(slot);
    char error[512]{};
    LUMICE_PathMap* raw = nullptr;
    if (LUMICE_AnalyzePathMap(scene.get(), &request, &raw, error, sizeof(error)) != LUMICE_OK) {
      ctx->LogError("Maximum source query failed: %s", error);
      return gui::MapHandle{};
    }
    return gui::MapHandle(raw, gui::MapDeleter{});
  };
  g_pressure.map.adopted_result = query(0);
  g_pressure.map.detail_result = query(1);
  IM_CHECK_RETV(g_pressure.map.adopted_result && g_pressure.map.detail_result, false);
  g_pressure.map.adopted_id = gui::g_survey_executor.NextId(1772);
  g_pressure.map.detail_id = gui::g_survey_executor.NextId(1772);
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
  g_pressure.map.intent_id = g_pressure.map.detail_id;
  g_pressure.map.answered_id = g_pressure.map.detail_id;
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
  IM_CHECK_RETV(LUMICE_PathMapGetPixels(g_pressure.map.detail_result.get(), &dense) == LUMICE_PATH_READ_OK, false);
  IM_CHECK_RETV(dense.pixel_count == LUMICE_PATH_MAP_MAX_PIXELS, false);
  size_t positive = 0;
  for (size_t i = 0; i < dense.pixel_count; ++i)
    positive += dense.values[i] > 0.0 && std::isfinite(dense.values[i]);
  IM_CHECK_RETV(positive == dense.pixel_count, false);
  g_pressure.candidate = query(2);
  g_pressure.candidate_id = gui::g_survey_executor.NextId(1772);
  IM_CHECK_RETV(g_pressure.candidate != nullptr, false);
  g_pressure.replace = true;
  ctx->Yield(3);
  IM_CHECK_RETV(g_pressure.replaced && !g_pressure.failed && g_pressure.old_released, false);
  scene.reset();
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
  ImGui::SetWindowPos(window, ImVec2(origin.x + 400, origin.y + 45), ImGuiCond_Always);
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
    double input = 0, present = 0, observe_ms = 0;
    gui::ViewProjection expected;
    bool moved = false;
  };
  std::vector<Sample> samples;
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
    sample.pointer = ImVec2(centre.x + 35.0f * std::cos(angle), centre.y + 23.0f * std::sin(angle));
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
    auto& sample = samples.back();
    const auto cost = gui::LastMapObservationTiming();
    if (sample.expected.lens_type != gui::kLensTypeGlobe) {
      sample.expected = gui::BuildPreviewViewProjFromRenderer(gui::g_state.renderer);
      sample.moved = !same(sample.expected, gui::LastFrozenMapView()) &&
                     std::abs(ImGui::GetIO().MousePos.x - sample.pointer.x) < 0.1f &&
                     std::abs(ImGui::GetIO().MousePos.y - sample.pointer.y) < 0.1f;
      sample.observe_ms = (end - cost.began_seconds) * 1000.0;
    }
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
                    { "present_s", sample.present },
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
  const double max_observe = observation.empty() ? 1e9 : *std::max_element(observation.begin(), observation.end());
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
    { "adoptions", adoptions },
    { "input_frames", latencies.size() },
    { "latency_p95_ms", p95 },
    { "latency_max_ms", max_latency },
    { "observation_max_upper_ms", max_observe },
    { "old_detail_released", g_pressure.old_released },
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
  t = IM_REGISTER_TEST(engine, "perf_test", "partial_prefix_frozen_composition");
  t->GuiFunc = [](ImGuiTestContext*) {
    if (g_probe.requested) {
      g_probe.requested = false;
      g_probe.frame = gui::DrawFrozenMap(g_probe.map, g_probe.view, g_probe.width, g_probe.height, &g_probe.probe);
      g_probe.done = true;
    }
  };
  t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(PartialPrefixWitness(ctx)); };
  t = IM_REGISTER_TEST(engine, "perf_test", "maximum_frozen_map_input_to_present");
  t->GuiFunc = [](ImGuiTestContext*) { PressureGui(); };
  t->TestFunc = [](ImGuiTestContext* ctx) { IM_CHECK(MaximumTimingWitness(ctx)); };
}
