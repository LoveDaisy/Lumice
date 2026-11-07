#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "gui/analyze_map_renderer.hpp"
#include "gui/preview_renderer.hpp"
#include "include/lumice_raypath.h"
#include "include/lumice_scene.h"
#include "nlohmann/json.hpp"
#include "test_gui_shared.hpp"

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;
using Direction = std::array<double, 3>;

// Independent double geometry: no shared GLSL, BuildViewMatrix, production index helper or
// colour helper. x/y are continuous GL pixel coordinates, measured from the lower-left.
std::optional<Direction> OracleDirection(const gui::ViewProjection& v, double x, double y, int w, int h, bool far) {
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
        const double t = -b + (far ? 1.0 : -1.0) * std::sqrt(disc);
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

bool ProjectionWitness(ImGuiTestContext* ctx) {
  ResetTestState();
  g_probe = {};
  const std::string path = std::string(LUMICE_E2E_CONFIG_DIR) + "/raypath_feature_random_regular.json";
  std::ifstream input(path);
  nlohmann::json config;
  input >> config;
  config["scene"]["light_source"]["altitude"] = 20.0;
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
  for (size_t i = 0; i < detail_source.pixels.pixel_count; ++i)
    positives += detail_source.pixels.values[i] > 0.0;
  IM_CHECK_RETV(positives > 0, false);
  // Handles and borrowed columns remain valid across Scene destruction.
  scene.reset();
  size_t verified = 0, detail_seen = 0, far_seen = 0, boundary_only = 0;
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
          IM_CHECK_RETV(g_probe.probe.near.size() == static_cast<size_t>(w) * h, false);
          for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
              const size_t pixel = static_cast<size_t>(y) * w + x;
              for (int root = 0; root < (lens == gui::kLensTypeGlobe ? 2 : 1); ++root) {
                const auto d = OracleDirection(g_probe.view, x + 0.5, y + 0.5, w, h, root != 0);
                const auto& actual = root ? g_probe.probe.far[pixel] : g_probe.probe.near[pixel];
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
  IM_CHECK_RETV(max_color_error <= 1e-6, false);
  IM_CHECK_RETV(RawHash(full_source.pixels) == full_source.hash && RawHash(detail_source.pixels) == detail_source.hash,
                false);
  ctx->LogInfo(
      "[frozen-map] verified=%zu detail=%zu far=%zu boundary-only=%zu max-color-error=%.12g raw hashes unchanged",
      verified, detail_seen, far_seen, boundary_only, max_color_error);
  g_probe = {};
  ctx->Yield(1);
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
}
