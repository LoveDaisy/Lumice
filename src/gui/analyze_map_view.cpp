#include "gui/analyze_map_view.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "gui/analysis_panel.hpp"  // SelectedClassIdentityFor / SurveySelectionKeyFor — the one identity chain
#include "gui/analyze_map_renderer.hpp"
#include "gui/analyze_overview.hpp"
#include "gui/analyze_point_view.hpp"    // RenderPointAnswerBlock — the sky-point answer host
#include "gui/analyze_region_wheel.hpp"  // the wheel claim — one arbitration for both gesture regions
#include "gui/analyze_workspace.hpp"     // PumpAnalyzeQueries — the pumps this window's Render-mode frames run
#include "gui/app.hpp"
#include "gui/canvas_report_layout.hpp"
#include "gui/gl_common.h"
#include "gui/gui_constants.hpp"
#include "gui/gui_logger.hpp"
#include "gui/gui_state.hpp"
#include "gui/preview_renderer.hpp"
#include "gui/theme.hpp"
#include "gui/view_look_at.hpp"
#include "imgui.h"
#include "util/sky_direction.hpp"

namespace lumice::gui {


namespace {

OverlayLegendState g_map_legend;
MapObservationTiming g_map_observation_timing;
double ObservationClockSeconds() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

constexpr double kPiD = 3.14159265358979323846;
constexpr double kDeg2RadD = kPiD / 180.0;
constexpr double kRad2DegD = 180.0 / kPiD;

double Clamp1(double v) {
  return std::max(-1.0, std::min(1.0, v));
}

double Dot3(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

double Len3(const double a[3]) {
  return std::sqrt(Dot3(a, a));
}

void Cross3(const double a[3], const double b[3], double out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

void Normalize3(double a[3]) {
  const double n = Len3(a);
  if (n > 1e-12) {
    a[0] /= n;
    a[1] /= n;
    a[2] /= n;
  }
}

// The POSITION vector of the sky label (alt, az) in degrees — the §12 position spelling.
void SkyPosition(double alt_deg, double az_deg, double pos[3]) {
  const double alt = alt_deg * kDeg2RadD;
  const double az = az_deg * kDeg2RadD;
  const double cos_alt = std::cos(alt);
  pos[0] = cos_alt * std::cos(az);
  pos[1] = cos_alt * std::sin(az);
  pos[2] = std::sin(alt);
}

// The sky position a map cell displays: the header's cell-direction formula (travel), negated
// (the §12 display rule). One implementation here, consumed by the raster, the Fit and the
// anchors — never a second spelling.
void CellPosition(const LUMICE_PathMapInfo& info, double delta_deg, double az_deg, double out_pos[3]) {
  const double delta = delta_deg * kDeg2RadD;
  const double az = az_deg * kDeg2RadD;
  const double cos_d = std::cos(delta);
  const double sin_d = std::sin(delta);
  const double cos_a = std::cos(az);
  const double sin_a = std::sin(az);
  double d[3];
  for (int k = 0; k < 3; ++k) {
    d[k] = cos_d * info.view_incident[k] + sin_d * (cos_a * info.view_e1[k] + sin_a * info.view_e2[k]);
  }
  out_pos[0] = -d[0];
  out_pos[1] = -d[1];
  out_pos[2] = -d[2];
}

// The quantity spec's published enums, spelled for the caliber line (presentation of the C
// header's own constants — the engine's vocabulary, not a second one).
const char* QuantityKindName(int kind) {
  switch (kind) {
    case LUMICE_PATH_QUANTITY_BAND_SUM:
      return "band-sum";
    default:
      return "quantity";
  }
}

const char* AggregationName(int aggregation) {
  switch (aggregation) {
    case LUMICE_PATH_AGGREGATION_PHYSICAL_CLASS:
      return "physical class";
    case LUMICE_PATH_AGGREGATION_REPRESENTATIVE:
      return "representative";
    case LUMICE_PATH_AGGREGATION_MEMBER:
      return "member";
    default:
      return "aggregation";
  }
}

const char* UnitName(int unit) {
  switch (unit) {
    case LUMICE_PATH_UNIT_PER_SR:
      return "per sr";
    case LUMICE_PATH_UNIT_DIMENSIONLESS:
      return "dimensionless";
    default:
      return "unit";
  }
}

// The in-place Pose button's two labels derive from the window-title constant (the display
// spelling has one source). The narrow-column form is the constant's LAST space turned into the
// line break — the unambiguous derivation for the only multi-space spelling; the byte-pin test
// reads the exact submitted strings through the accessors below.
const std::string kPoseButtonLabelStorage = std::string(kPoseViewWindowTitle) + "###pose_open";
const std::string kPoseButtonWrappedLabelStorage = [] {
  std::string wrapped = kPoseViewWindowTitle;
  const size_t last_space = wrapped.find_last_of(' ');
  if (last_space != std::string::npos)
    wrapped[last_space] = '\n';
  return wrapped + "###pose_open";
}();

}  // namespace

const char* PoseButtonLabel() {
  return kPoseButtonLabelStorage.c_str();
}

const char* PoseButtonWrappedLabel() {
  return kPoseButtonWrappedLabelStorage.c_str();
}

// The Pose button's tooltip: the constant purpose text that used to sit beside the button as
// always-on prose (the disposition ledger — a fixed explanation must not occupy operation
// area). A named function, not an inline literal, so the relocated text keeps its byte pin in
// the tests the same way the two labels above do.
const char* PoseButtonTooltip() {
  return "3D ray & pose.\nSelect class + sky point.";
}

// ---- The caliber composer (pure; the unit layer holds it to the readback) ---------------------------
//
// Three read-backs of one adopted map, one per information tier (the disposition ledger): the
// DEFAULT lines answer "what am I looking at" — the short quantity name with its unit, the
// scene-vs-nominal limit when the answer is not the nominal shape, and the truncation /
// finite-zero / rank-0-mass facts; the PHYSICS lines carry the full quantity definition, the
// sun premise and the engine's own geometry basis (byte for byte); the CALCULATION lines carry
// the grid and execution numbers a check reads — reference cell, view window, lattice,
// members, kept events — plus the source-fixed-scale note. The λ is NOT in any line: the λ
// row's same-control echo is the reading's one owner (a repeated echo is deleted, not moved).

std::vector<std::string> MapCaliberDefaultLines(const MapCaliberInput& in) {
  std::vector<std::string> lines;
  char line[512];
  const LUMICE_PathMapInfo& info = in.info;
  if (in.have_quantity) {
    std::snprintf(line, sizeof(line), "%s, %s.", QuantityKindName(in.quantity.kind), UnitName(in.quantity.unit));
    lines.emplace_back(line);
  }
  if (in.have_quantity && info.shape_is_nominal == 0) {
    // The nominal-vs-scene limit is a default fact (the ledger: 名义几何≠冻结场景的限定默认) —
    // the map answers the edited scene, and the reader must know that without opening a fold.
    lines.emplace_back("Scene geometry — the map answers the edited scene, not the nominal shape.");
  }
  if (info.truncated != 0) {
    std::snprintf(line, sizeof(line),
                  "Truncated at the budget: the picture is the completed prefix of %d of %d members — not the whole "
                  "class.",
                  info.members_evaluated, info.member_count);
    lines.emplace_back(line);
  }
  if (info.kept_count == 0 && info.truncated == 0 && in.completed) {
    lines.emplace_back("No events in this view — a finite zero, not a refusal and not missing data.");
  }
  if (in.have_mass) {
    std::string methods;
    for (size_t i = 0; i < kMapCaliberMaxMethods; ++i) {
      if (in.methods[i] == nullptr) {
        break;
      }
      if (i > 0) {
        methods += ", ";
      }
      methods += in.methods[i];
    }
    std::snprintf(line, sizeof(line),
                  "Rank-0 arm: m = %.6g ± %.2g (%s) at the sun's cell%s — mass is separate from any band arm; "
                  "zero cells are not no-light.",
                  in.mass.m, in.mass.m_error, methods.c_str(), in.mass.pixel < 0 ? " (outside this view)" : "");
    lines.emplace_back(line);
  }
  return lines;
}

std::vector<std::string> MapCaliberPhysicsLines(const MapCaliberInput& in) {
  std::vector<std::string> lines;
  char line[512];
  const LUMICE_PathMapInfo& info = in.info;
  if (in.have_quantity) {
    std::snprintf(line, sizeof(line), "Quantity: %s, %s, %s.", QuantityKindName(in.quantity.kind),
                  AggregationName(in.quantity.aggregation), UnitName(in.quantity.unit));
    lines.emplace_back(line);
    std::snprintf(line, sizeof(line), "Sun at altitude %.1f°, azimuth %.1f°.", info.sun_altitude_deg,
                  info.sun_azimuth_deg);
    lines.emplace_back(line);
    if (info.shape_is_nominal != 0) {
      lines.emplace_back("Nominal geometry — the map answers the crystal's nominal shape.");
    } else {
      lines.emplace_back("Scene geometry.");
    }
  }
  if (in.have_evidence && in.evidence.geometry_basis.length > 0) {
    lines.emplace_back(in.evidence.geometry_basis.data, in.evidence.geometry_basis.length);
  }
  return lines;
}

std::vector<std::string> MapCaliberCalculationLines(const MapCaliberInput& in) {
  std::vector<std::string> lines;
  char line[512];
  const LUMICE_PathMapInfo& info = in.info;
  if (info.view_full_sky != 0) {
    std::snprintf(line, sizeof(line), "Reference cell δ %.7g° × az %.7g° · view: full sky %d×%d · n=%d lattice",
                  (info.view_delta_max_deg - info.view_delta_min_deg) / info.view_rows,
                  (info.view_az_max_deg - info.view_az_min_deg) / info.view_cols, info.view_rows, info.view_cols,
                  info.sample_count);
  } else {
    std::snprintf(line, sizeof(line),
                  "Reference cell δ %.7g° × az %.7g° · view: δ [%.1f, %.1f]° az [%.1f, %.1f]° %d×%d · n=%d lattice",
                  (info.view_delta_max_deg - info.view_delta_min_deg) / info.view_rows,
                  (info.view_az_max_deg - info.view_az_min_deg) / info.view_cols, info.view_delta_min_deg,
                  info.view_delta_max_deg, info.view_az_min_deg, info.view_az_max_deg, info.view_rows, info.view_cols,
                  info.sample_count);
  }
  lines.emplace_back(line);
  std::snprintf(line, sizeof(line), "%d/%d members · kept %lld events", info.members_evaluated, info.member_count,
                info.kept_count);
  lines.emplace_back(line);
  lines.emplace_back("Source-fixed scale; cells do not interpolate.");
  return lines;
}

bool SkySunAnchorFor(const GuiState& state, SkySunAnchor* out) {
  // The adopted map's own echo is the best anchor (the picture's sun). Then the adopted
  // survey's scope (the digest's sun). The live document last — an edited scene's sun may
  // differ from what any adopted document described.
  const auto& source = state.analysis.map.adopted_result;
  if (source != nullptr) {
    LUMICE_PathMapInfo info{};
    info.struct_size = sizeof(info);
    if (LUMICE_PathMapGetInfo(source.get(), &info) == LUMICE_PATH_READ_OK) {
      out->altitude_deg = info.sun_altitude_deg;
      out->azimuth_deg = info.sun_azimuth_deg;
      return true;
    }
  }
  if (state.analysis.survey.adopted_result != nullptr) {
    LUMICE_PathSurveyScope scope{};
    scope.struct_size = sizeof(scope);
    if (LUMICE_PathSurveyGetScope(state.analysis.survey.adopted_result.get(), &scope) == LUMICE_PATH_READ_OK &&
        scope.sun_altitude_available != 0) {
      out->altitude_deg = scope.sun_altitude_deg;
      out->azimuth_deg = scope.sun_azimuth_deg;
      return true;
    }
  }
  // Last resort: the sun the frozen scene binding was built with — the engine evaluates the
  // request against ITS sun, never the live document's. Keep the picture's anchors on that
  // same binding, frozen at DoAnalyze.
  out->altitude_deg = state.analysis.frozen_sun_valid ? state.analysis.frozen_sun_altitude : state.sun.altitude;
  out->azimuth_deg = 0.0;  // the GUI's sun is always due north
  return true;
}

LUMICE_PathMapRequest MakeMapRequestForKey(const MapIntentKey& key, std::vector<int>* faces, std::vector<int>* counts,
                                           std::vector<double>* wavelengths) {
  *faces = key.selection.faces;
  *counts = key.selection.layer_face_counts;
  LUMICE_PathMapRequest request{};
  request.struct_size = sizeof(request);
  request.crystal_id = key.selection.crystal_id;
  request.faces = faces->data();
  request.face_count = static_cast<int>(faces->size());
  request.layer_face_counts = counts->data();
  request.layer_count = static_cast<int>(counts->size());
  request.full_sky = 1;
  if (key.wavelength_nm > 0.0) {
    *wavelengths = { key.wavelength_nm };
    request.wavelengths_nm = wavelengths->data();
    request.wavelength_count = 1;
  }
  request.sample_count = kMapQuerySampleCount;
  request.budget_ms = LUMICE_PATH_MAP_DEFAULT_BUDGET_MS;
  return request;
}

// ---- Fit, normalization, colour --------------------------------------------------------------------

MapFitResult FitMapViewFromPixels(const LUMICE_PathMapPixels& pixels, const LUMICE_PathMapInfo& info,
                                  size_t* temporary_bytes) {
  if (temporary_bytes)
    *temporary_bytes = 0;
  MapFitResult fit;
  double cx = 0.0;
  double cy = 0.0;
  double cz = 0.0;
  double total = 0.0;
  std::vector<double> weights;
  std::vector<double> distances;  // angular distance from the centroid, same order (filled later)
  size_t positive_count = 0;
  for (size_t i = 0; i < pixels.pixel_count; ++i) {
    const uint8_t status = pixels.status[i];
    if (status != LUMICE_PATH_MAP_PIXEL_OK) {
      continue;
    }
    const double v = pixels.values[i];
    if (!std::isfinite(v) || v <= 0.0) {
      continue;
    }
    double pos[3];
    CellPosition(info, pixels.delta_deg[i], pixels.azimuth_deg[i], pos);
    cx += pos[0] * v;
    cy += pos[1] * v;
    cz += pos[2] * v;
    total += v;
    ++positive_count;
  }
  if (total <= 0.0) {
    return fit;  // ok=false: an honest all-zero map asks for no Fit
  }
  double centre[3] = { cx / total, cy / total, cz / total };
  Normalize3(centre);
  if (Len3(centre) < 1e-12)
    return fit;
  weights.reserve(positive_count);
  distances.reserve(positive_count);
  for (size_t i = 0; i < pixels.pixel_count; ++i) {
    const uint8_t status = pixels.status[i];
    if (status != LUMICE_PATH_MAP_PIXEL_OK) {
      continue;
    }
    const double v = pixels.values[i];
    if (!std::isfinite(v) || v <= 0.0) {
      continue;
    }
    double pos[3];
    CellPosition(info, pixels.delta_deg[i], pixels.azimuth_deg[i], pos);
    const double cos_a = Clamp1(Dot3(pos, centre));
    weights.push_back(v);
    distances.push_back(std::acos(cos_a));
  }
  // 95% of the value mass, by ascending distance (the prototype's radius95).
  std::vector<size_t> order(weights.size());
  if (temporary_bytes)
    *temporary_bytes = (weights.capacity() + distances.capacity()) * sizeof(double) + order.capacity() * sizeof(size_t);
  for (size_t i = 0; i < order.size(); ++i) {
    order[i] = i;
  }
  std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return distances[a] < distances[b]; });
  double r95 = 0.0;
  double acc = 0.0;
  for (const size_t idx : order) {
    acc += weights[idx];
    r95 = distances[idx];
    if (acc >= 0.95 * total) {
      break;
    }
  }
  const double radius_deg = std::clamp(r95 * kRad2DegD * 1.4, 3.0, 180.0);
  fit.ok = true;
  for (int k = 0; k < 3; ++k) {
    fit.centre_pos[k] = static_cast<float>(centre[k]);
  }
  fit.radius_deg = static_cast<float>(radius_deg);
  return fit;
}

MapDisplayNorm MapDisplayNormFor(const LUMICE_PathMapPixels& pixels, size_t* temporary_bytes) {
  MapDisplayNorm norm;
  std::vector<double> positive;
  positive.reserve(pixels.pixel_count);
  if (temporary_bytes)
    *temporary_bytes = positive.capacity() * sizeof(double);
  for (size_t i = 0; i < pixels.pixel_count; ++i) {
    if (pixels.status[i] == LUMICE_PATH_MAP_PIXEL_SINGULAR) {
      continue;
    }
    const double v = pixels.values[i];
    if (std::isfinite(v) && v > 0.0) {
      positive.push_back(v);
    }
  }
  if (positive.empty()) {
    return norm;  // valid=false: the honest all-zero texture
  }
  std::sort(positive.begin(), positive.end());
  const size_t idx =
      std::min(positive.size() - 1, static_cast<size_t>(std::ceil(0.99 * static_cast<double>(positive.size()))) - 1);
  norm.valid = true;
  norm.vmax = positive[idx];
  return norm;
}

std::array<double, 4> MapCellColor(double value, uint8_t status, const MapDisplayNorm& norm) {
  if (status == LUMICE_PATH_MAP_PIXEL_SINGULAR) {
    return { 0.0, 0.0, 0.0, 0.0 };  // NaN honesty: no finite colour
  }
  if (status == LUMICE_PATH_MAP_PIXEL_POINT_MASS) {
    return { 250.0 / 255.0, 184.0 / 255.0, 74.0 / 255.0, 1.0 };
  }
  double t = 0.0;
  if (norm.valid && norm.vmax > 0.0) {
    t = std::clamp(value / norm.vmax, 0.0, 1.0);
  }
  t = std::pow(t, 0.6);
  // Dark slate -> ice blue -> white, two linear stops.
  const double stop_r[2] = { 0.055, 0.30 };
  const double stop_g[2] = { 0.065, 0.48 };
  const double stop_b[2] = { 0.100, 0.72 };
  double r = 0.0;
  double g = 0.0;
  double b = 0.0;
  if (t < 0.5) {
    const double s = t / 0.5;
    r = stop_r[0] + (stop_r[1] - stop_r[0]) * s;
    g = stop_g[0] + (stop_g[1] - stop_g[0]) * s;
    b = stop_b[0] + (stop_b[1] - stop_b[0]) * s;
  } else {
    const double s = (t - 0.5) / 0.5;
    r = stop_r[1] + (0.94 - stop_r[1]) * s;
    g = stop_g[1] + (0.96 - stop_g[1]) * s;
    b = stop_b[1] + (1.00 - stop_b[1]) * s;
  }
  return { r, g, b, 1.0 };
}

uint32_t MapCellColorRgba(double value, uint8_t status, const MapDisplayNorm& norm) {
  const auto color = MapCellColor(value, status, norm);
  return IM_COL32(static_cast<int>(color[0] * 255.0), static_cast<int>(color[1] * 255.0),
                  static_cast<int>(color[2] * 255.0), static_cast<int>(color[3] * 255.0));
}

// ---- The pump --------------------------------------------------------------------------------------

namespace {

// The issuing half of the pump (the survey pump's IssueSurveyQuery shape): build the request for
// `key` and hand it to the executor with the id the pump already assigned. Buffers move into the
// executor's job.
void IssueMapIntent(SurveyExecutor& executor, const SurveyQueryId& id, const MapIntentKey& key) {
  std::vector<int> faces;
  std::vector<int> counts;
  std::vector<double> wavelengths;
  const LUMICE_PathMapRequest request = MakeMapRequestForKey(key, &faces, &counts, &wavelengths);
  executor.IssueMapQuery(id, request, std::move(faces), std::move(counts), std::move(wavelengths));
}

}  // namespace

void PumpMapQuery(GuiState& state, SurveyExecutor& executor, double now_s, MapSourcePreparer prepare) {
  MapQuerySession& map = state.analysis.map;
  const auto identity = SelectedClassIdentityFor(state);
  const bool selected = identity.has_value() && state.analysis_result.payload != nullptr;
  SurveySelectionKey selection;
  if (selected) {
    selection = SurveySelectionKeyFor(*identity, state.analysis_result.payload->snapshot_generation);
  }
  // Invalidate before draining: no old capture/lambda may acquire the new labels, even for a
  // frame. Camera fields are deliberately absent from this effect chain.
  if (map.intent_valid &&
      (!selected || map.key.selection != selection || map.key.wavelength_nm != state.analysis.map_wavelength_nm)) {
    executor.CancelInFlight(QueryFace::kMap);
    map = MapQuerySession{};
  }
  while (auto finished = executor.TakeFinished(QueryFace::kMap, /*defer_map_ack=*/true)) {
    if (!map.intent_valid || map.cancelled_for_key || finished->id != map.intent_id) {
      continue;
    }
    map.answered_id = finished->id;
    LUMICE_PathMapInfo info{};
    info.struct_size = sizeof(info);
    if (!finished->ok || !finished->map_result ||
        LUMICE_PathMapGetInfo(finished->map_result.get(), &info) != LUMICE_PATH_READ_OK ||
        info.state != LUMICE_PATH_STATE_FINITE) {
      map.adopted_error = finished->error;
      if (map.adopted_error.empty()) {
        map.adopted_error =
            info.reason.length ? std::string(info.reason.data, info.reason.length) : "Map result is unavailable";
      }
      continue;  // a refusal never replaces an existing finite source with zero
    }
    if (prepare && !prepare(finished->map_result, finished->id)) {
      map.adopted_error = "Display preparation failed; previous finite map kept. Retry the full-sky request.";
      continue;
    }
    map.adopted_error.clear();
    map.adopted_result = std::move(finished->map_result);
    map.adopted_id = finished->id;
  }
  executor.CompleteMapDrain();  // all temporary results and previous-source readers have left scope
  if (selected && !map.intent_valid) {
    map.intent_valid = true;
    map.key.selection = selection;
    map.key.wavelength_nm = state.analysis.map_wavelength_nm;
    map.intent_id = executor.NextId(selection.generation);
    map.intent_start_s = now_s;
    IssueMapIntent(executor, map.intent_id, map.key);  // once, all sky
  }
}

void RetryMap(GuiState& state, SurveyExecutor& executor, double now_s) {
  auto& map = state.analysis.map;
  if (!map.intent_valid || (!map.cancelled_for_key && map.adopted_error.empty())) {
    return;
  }
  executor.CancelInFlight(QueryFace::kMap);
  map.cancelled_for_key = false;
  map.adopted_error.clear();
  map.intent_id = executor.NextId(map.key.selection.generation);
  map.intent_start_s = now_s;
  IssueMapIntent(executor, map.intent_id, map.key);
}

void CancelMap(GuiState& state, SurveyExecutor& executor) {
  executor.CancelInFlight(QueryFace::kMap);
  state.analysis.map.cancelled_for_key = true;
}

MapObservationTiming LastMapObservationTiming() {
  return g_map_observation_timing;
}

OverlayLegendState LastMapOverlayLegend() {
  return g_map_legend;
}

bool InitMapView() {
  return InitializeFrozenMapDisplay();
}

void ShutdownMapView() {
  DestroyFrozenMapDisplay();
  g_map_legend = {};
}

// ---- The renderer -----------------------------------------------------------------------------------

namespace {

struct MapCanvasProjection {
  ViewProjection camera;
  int width = 1;
  int height = 1;
  float width_pt = 1.0f;
  float height_pt = 1.0f;
};
constexpr int kOverlaySegments = 96;

bool ProjectToRect(const MapCanvasProjection& view, const double pos[3], const ImVec2& pic_min, float, double* out_x,
                   double* out_y) {
  if ((view.camera.visible == kVisibleUpper && pos[2] < 0.0) ||
      (view.camera.visible == kVisibleLower && pos[2] > 0.0)) {
    return false;
  }
  const float travel[3] = { static_cast<float>(-pos[0]), static_cast<float>(-pos[1]), static_cast<float>(-pos[2]) };
  if (view.camera.front) {
    float matrix[9];
    BuildViewMatrix(view.camera.elevation, view.camera.azimuth, view.camera.roll, matrix);
    if (travel[0] * matrix[6] + travel[1] * matrix[7] + travel[2] * matrix[8] > 0.0f)
      return false;
  }
  const auto screen = ProjectWorldDirToScreen(view.camera, travel, view.width, view.height);
  if (screen[0] == kOverlaySentinel)
    return false;
  *out_x = pic_min.x + (screen[0] / view.width + 0.5f) * view.width_pt;
  *out_y = pic_min.y + (0.5f - screen[1] / view.height) * view.height_pt;
  return true;
}

// One drawn segment of the iso-deviation circle's walk: whether any point landed in the disk,
// and the topmost (smallest screen y) of them — the label anchor the curve's readout sits at.
struct IsoDeltaWalk {
  int drawn = 0;            // the CURRENT contiguous polyline run (a disk exit breaks the run and resets
                            // it — the polyline's segmentation, not the walked amount)
  int drawn_total = 0;      // segments submitted across the WHOLE walk: the "actually drew this
                            // curve" fact the legend reads (the final run's length would read
                            // zero for a circle that left the disk mid-way and never returned)
  int samples_in_disk = 0;  // the circle's own samples that landed in the view disk (of kOverlaySegments+1)
  bool have_top = false;
  double top_x = 0.0;
  double top_y = 0.0;
  // The circle is PARTIALLY visible: some samples projected in-disk, some did not. The label's
  // honesty qualifier reads this (the drawn arc is not the whole circle).
  bool partial() const { return samples_in_disk > 0 && samples_in_disk <= kOverlaySegments; }
};

// The iso-deviation circle about the sun (the survey dispersion's curve geometry): the circle's
// one parameterization — orthogonal basis about the sun, kOverlaySegments samples — sampled,
// projected, split where the disk leaves the view.
IsoDeltaWalk WalkIsoDeltaCircle(ImDrawList* dl, const MapCanvasProjection& view, const ImVec2& pic_min, float pic_px,
                                const double sun_pos[3], double delta_deg, ImU32 colour, float thickness) {
  double u[3] = { 0.0, 0.0, 1.0 };
  double dot = Dot3(u, sun_pos);
  if (std::fabs(dot) > 0.9) {
    u[0] = 1.0;
    u[1] = 0.0;
    u[2] = 0.0;
    dot = Dot3(u, sun_pos);
  }
  for (int k = 0; k < 3; ++k) {
    u[k] -= dot * sun_pos[k];
  }
  Normalize3(u);
  double v[3];
  Cross3(sun_pos, u, v);
  const double delta = delta_deg * kDeg2RadD;
  const double cos_d = std::cos(delta);
  const double sin_d = std::sin(delta);
  IsoDeltaWalk walk;
  double last_x = 0.0;
  double last_y = 0.0;
  for (int s = 0; s <= kOverlaySegments; ++s) {
    const double phi = 2.0 * kPiD * s / kOverlaySegments;
    const double p[3] = { cos_d * sun_pos[0] + sin_d * (std::cos(phi) * u[0] + std::sin(phi) * v[0]),
                          cos_d * sun_pos[1] + sin_d * (std::cos(phi) * u[1] + std::sin(phi) * v[1]),
                          cos_d * sun_pos[2] + sin_d * (std::cos(phi) * u[2] + std::sin(phi) * v[2]) };
    double x = 0.0;
    double y = 0.0;
    if (!ProjectToRect(view, p, pic_min, pic_px, &x, &y)) {
      walk.drawn = 0;  // the run broke; the next in-disk point starts a fresh polyline
      continue;
    }
    ++walk.samples_in_disk;
    if (walk.drawn > 0) {
      dl->AddLine(ImVec2(static_cast<float>(last_x), static_cast<float>(last_y)),
                  ImVec2(static_cast<float>(x), static_cast<float>(y)), colour, thickness);
    }
    last_x = x;
    last_y = y;
    ++walk.drawn;
    ++walk.drawn_total;
    if (!walk.have_top || y < walk.top_y) {
      walk.have_top = true;
      walk.top_x = x;
      walk.top_y = y;
    }
  }
  return walk;
}

void DrawFrameAnchors(ImDrawList* dl, const MapCanvasProjection& view, const LUMICE_PathMapInfo& info,
                      const ImVec2& pic_min, float pic_px, OverlayLegendState* legend) {
  // The sun disc (the delta axis's zero — the anchor every readout quotes), the zenith cross and
  // the horizon circle. All from the result's own sun echo.
  double sun_pos[3];
  SkyPosition(info.sun_altitude_deg, info.sun_azimuth_deg, sun_pos);
  double sx = 0.0;
  double sy = 0.0;
  if (ProjectToRect(view, sun_pos, pic_min, pic_px, &sx, &sy)) {
    dl->AddCircleFilled(ImVec2(static_cast<float>(sx), static_cast<float>(sy)), UiPx(4.0f),
                        IM_COL32(255, 231, 168, 255));
    dl->AddCircle(ImVec2(static_cast<float>(sx), static_cast<float>(sy)), UiPx(6.0f), IM_COL32(255, 231, 168, 128), 0,
                  1.0f);
    if (legend != nullptr) {
      legend->anchor_sun = true;
    }
  }
  double zenith[3] = { 0.0, 0.0, 1.0 };
  double zx = 0.0;
  double zy = 0.0;
  if (ProjectToRect(view, zenith, pic_min, pic_px, &zx, &zy)) {
    const float a = UiPx(3.0f);
    const float cx = static_cast<float>(zx);
    const float cy = static_cast<float>(zy);
    dl->AddLine(ImVec2(cx - a, cy), ImVec2(cx + a, cy), IM_COL32(200, 205, 215, 110), 1.0f);
    dl->AddLine(ImVec2(cx, cy - a), ImVec2(cx, cy + a), IM_COL32(200, 205, 215, 110), 1.0f);
    if (legend != nullptr) {
      legend->anchor_zenith = true;
    }
  }
  bool horizon_drawn = false;
  for (int s = 0; s < kOverlaySegments; ++s) {
    const double a1 = 2.0 * kPiD * s / kOverlaySegments;
    const double a2 = 2.0 * kPiD * (s + 1) / kOverlaySegments;
    double p1[3];
    double p2[3];
    SkyPosition(0.0, a1 * kRad2DegD, p1);
    SkyPosition(0.0, a2 * kRad2DegD, p2);
    double x1 = 0.0;
    double y1 = 0.0;
    double x2 = 0.0;
    double y2 = 0.0;
    if (ProjectToRect(view, p1, pic_min, pic_px, &x1, &y1) && ProjectToRect(view, p2, pic_min, pic_px, &x2, &y2)) {
      dl->AddLine(ImVec2(static_cast<float>(x1), static_cast<float>(y1)),
                  ImVec2(static_cast<float>(x2), static_cast<float>(y2)), IM_COL32(200, 205, 215, 60), 1.0f);
      horizon_drawn = true;
    }
  }
  if (legend != nullptr && horizon_drawn) {
    legend->anchor_horizon = true;
  }
}

}  // namespace

// The marker vocabulary: a shape and a colour per kind (the engine's six kinds plus the
// fallback), solid for a concluded claim (certified/finite) and outline-only otherwise — the
// refused/unevaluated rows must not read as answered.
OverlayMarkerStyle OverlayMarkerStyleFor(const std::string& kind_name, int state) {
  OverlayMarkerStyle s;
  s.filled = state == LUMICE_PATH_STATE_CERTIFIED || state == LUMICE_PATH_STATE_FINITE;
  if (kind_name == "source image") {
    s.shape = OverlayMarkerShape::kCircle;
    s.colour = IM_COL32(255, 231, 168, 255);  // the sun's own image: the anchor gold
  } else if (kind_name == "regular image") {
    s.shape = OverlayMarkerShape::kCircle;
    s.colour = IM_COL32(140, 230, 200, 255);
  } else if (kind_name == "support set") {
    s.shape = OverlayMarkerShape::kSquare;
    s.colour = IM_COL32(140, 196, 255, 255);
  } else if (kind_name == "caustic edge") {
    s.shape = OverlayMarkerShape::kTriangle;
    s.colour = IM_COL32(255, 176, 92, 255);
  } else if (kind_name == "bright domain") {
    s.shape = OverlayMarkerShape::kDiamond;
    s.colour = IM_COL32(140, 196, 255, 255);
  } else if (kind_name == "family collapse") {
    s.shape = OverlayMarkerShape::kTriangleDown;
    s.colour = IM_COL32(198, 160, 255, 255);
  } else {
    s.shape = OverlayMarkerShape::kFallbackDiamond;
    s.colour = IM_COL32(170, 178, 190, 255);
  }
  return s;
}

// The observation key's one comparison: identical on EVERY field the row carries — kind, state,
// position, reason AND the evidence fields (family_level, blur_deg). "×N identical" is a claim
// about the SOURCES, so the comparison is the row's whole content: two rows differing in any
// carried field are different sources and stay separate entries. An earlier draft keyed on the
// tooltip's own text fields only, and two rows with the same words but different family or blur
// evidence folded as "identical" — a sameness the display could not prove and the sources did
// not share (AC4's no-silent-drop line).
bool OverlayRowsSameKey(const SurveyObjectRow& a, const SurveyObjectRow& b) {
  return a.kind_name == b.kind_name && a.state == b.state && a.state_name == b.state_name &&
         a.position_available == b.position_available && a.position_altitude_deg == b.position_altitude_deg &&
         a.position_azimuth_deg == b.position_azimuth_deg && a.family_level == b.family_level &&
         a.blur_deg == b.blur_deg && a.reason == b.reason;
}

void GroupOverlayHits(const std::vector<const SurveyObjectRow*>& hits, std::vector<OverlayHitGroup>* out) {
  out->clear();
  for (const SurveyObjectRow* hit : hits) {
    bool folded = false;
    for (OverlayHitGroup& group : *out) {
      if (OverlayRowsSameKey(*group.representative, *hit)) {
        ++group.count;
        folded = true;
        break;
      }
    }
    if (!folded) {
      out->push_back(OverlayHitGroup{ hit, 1 });
    }
  }
}

namespace {

struct SurveyOverlayCache {
  std::weak_ptr<const LUMICE_PathSurvey> source;
  SurveyQueryId id;
  SurveyOverviewModel model;
};
SurveyOverlayCache g_survey_overlay_cache;

void DrawSurveyOverlays(ImDrawList* dl, GuiState& state, const MapCanvasProjection& view, const ImVec2& pic_min,
                        float pic_px, OverlayLegendState* legend) {
  SurveyQuerySession& survey = state.analysis.survey;
  if (survey.adopted_result == nullptr) {
    return;
  }
  // The payloads read through the ONE document->facts mapping (§5.1.8: the overview model is the
  // single organizer; the map overlays consume its object rows and dispersion block verbatim).
  auto& cache = g_survey_overlay_cache;
  if (cache.source.lock() != survey.adopted_result || cache.id != survey.intent_id) {
    // The document is immutable. Build once per adopted identity, not once per observation.
    cache = {};
    cache.model = BuildSurveyOverviewModel(*survey.adopted_result);
    cache.source = survey.adopted_result;
    cache.id = survey.intent_id;
  }
  const SurveyOverviewModel& model = cache.model;
  // Positional objects: the survey's own located conclusions, drawn where the document puts
  // them — one marker per object, styled by kind and claim state; the frame's hits collect for
  // ONE organized tooltip (every object under the pointer, grouped by the observation key),
  // never a stack of same-text tooltips.
  std::vector<const SurveyObjectRow*> hits;
  const float marker_r = UiPx(6.0f);
  for (const SurveyObjectRow& row : model.objects) {
    if (!row.position_available) {
      continue;
    }
    double pos[3];
    SkyPosition(row.position_altitude_deg, row.position_azimuth_deg, pos);
    double x = 0.0;
    double y = 0.0;
    if (!ProjectToRect(view, pos, pic_min, pic_px, &x, &y)) {
      continue;
    }
    const float cx = static_cast<float>(x);
    const float cy = static_cast<float>(y);
    const OverlayMarkerStyle style = OverlayMarkerStyleFor(row.kind_name, row.state);
    const ImVec2 pts[4] = { ImVec2(cx, cy - marker_r), ImVec2(cx + marker_r, cy), ImVec2(cx, cy + marker_r),
                            ImVec2(cx - marker_r, cy) };
    switch (style.shape) {
      case OverlayMarkerShape::kCircle:
        if (style.filled) {
          dl->AddCircleFilled(ImVec2(cx, cy), marker_r * 0.8f, style.colour);
        } else {
          dl->AddCircle(ImVec2(cx, cy), marker_r * 0.8f, style.colour, 0, 1.4f);
        }
        break;
      case OverlayMarkerShape::kSquare:
        if (style.filled) {
          dl->AddRectFilled(ImVec2(cx - marker_r * 0.75f, cy - marker_r * 0.75f),
                            ImVec2(cx + marker_r * 0.75f, cy + marker_r * 0.75f), style.colour);
        } else {
          dl->AddRect(ImVec2(cx - marker_r * 0.75f, cy - marker_r * 0.75f),
                      ImVec2(cx + marker_r * 0.75f, cy + marker_r * 0.75f), style.colour, 0.0f, 0, 1.4f);
        }
        break;
      case OverlayMarkerShape::kTriangle:
        if (style.filled) {
          dl->AddTriangleFilled(pts[0], pts[1], pts[2], style.colour);
        } else {
          dl->AddTriangle(pts[0], pts[1], pts[2], style.colour, 1.4f);
        }
        break;
      case OverlayMarkerShape::kTriangleDown:
        if (style.filled) {
          dl->AddTriangleFilled(pts[0], pts[3], pts[2], style.colour);
        } else {
          dl->AddTriangle(pts[0], pts[3], pts[2], style.colour, 1.4f);
        }
        break;
      case OverlayMarkerShape::kDiamond:
      case OverlayMarkerShape::kFallbackDiamond:
        if (style.filled) {
          dl->AddConvexPolyFilled(pts, 4, style.colour);
        } else {
          dl->AddPolyline(pts, 4, style.colour, ImDrawFlags_Closed, 1.4f);
        }
        break;
    }
    if (legend != nullptr) {
      bool known = false;
      for (const std::string& k : legend->object_kinds) {
        if (k == row.kind_name) {
          known = true;
          break;
        }
      }
      if (!known) {
        legend->object_kinds.push_back(row.kind_name);
      }
    }
    if (ImGui::IsMouseHoveringRect(ImVec2(cx - marker_r, cy - marker_r), ImVec2(cx + marker_r, cy + marker_r))) {
      hits.push_back(&row);
    }
  }
  // ONE tooltip per frame for everything under the pointer: the position header, then one line
  // per observation key (identical keys fold into an ×N count — a display grouping of the
  // model's rows; nothing drops, different sources stay separate).
  if (!hits.empty() && ImGui::BeginTooltip()) {
    const SurveyObjectRow& first = *hits.front();
    ImGui::TextUnformatted("Survey objects");
    ImGui::TextDisabled("alt %.2f° az %.2f° · %zu object%s", first.position_altitude_deg, first.position_azimuth_deg,
                        hits.size(), hits.size() == 1 ? "" : "s");
    std::vector<OverlayHitGroup> groups;
    GroupOverlayHits(hits, &groups);
    for (const OverlayHitGroup& group : groups) {
      const SurveyObjectRow& row = *group.representative;
      ImGui::Text("%s · %s", row.kind_name.c_str(), row.state_name.c_str());
      if (group.count > 1) {
        ImGui::SameLine();
        ImGui::TextDisabled("×%d identical", group.count);
      }
      // The evidence fields the fold key honors, spelled on the position line: a family arm's
      // level tag and a blur readout, when the row carries them — the differences between two
      // entries are then readable where the entries differ, not only in their count.
      char pos_line[96];
      if (row.family_level >= 0 && row.blur_deg > 0.0) {
        std::snprintf(pos_line, sizeof(pos_line), "alt %.2f° az %.2f° · level %d · blur %.2f°",
                      row.position_altitude_deg, row.position_azimuth_deg, row.family_level, row.blur_deg);
      } else if (row.family_level >= 0) {
        std::snprintf(pos_line, sizeof(pos_line), "alt %.2f° az %.2f° · level %d", row.position_altitude_deg,
                      row.position_azimuth_deg, row.family_level);
      } else if (row.blur_deg > 0.0) {
        std::snprintf(pos_line, sizeof(pos_line), "alt %.2f° az %.2f° · blur %.2f°", row.position_altitude_deg,
                      row.position_azimuth_deg, row.blur_deg);
      } else {
        std::snprintf(pos_line, sizeof(pos_line), "alt %.2f° az %.2f°", row.position_altitude_deg,
                      row.position_azimuth_deg);
      }
      ImGui::TextDisabled("%s", pos_line);
      if (!row.reason.empty()) {
        ImGui::TextWrapped("%s", row.reason.c_str());
      }
    }
    ImGui::EndTooltip();
  }
  // The chromatic dispersion's inner-edge pair: the engine's own delta values, drawn as
  // iso-deviation circles about the sun, red inside blue (the C01 alignment face).
  if (model.dispersion.available && model.dispersion.min_row_available) {
    SkySunAnchor sun;
    SkySunAnchorFor(state, &sun);
    double sun_pos[3];
    SkyPosition(sun.altitude_deg, sun.azimuth_deg, sun_pos);
    struct Curve {
      double delta_deg;
      ImU32 colour;
      const std::string* label;
      bool* legend_bit;  // the legend reads what the WALK actually drew, per curve — not the
                         // survey payload's availability (a circle entirely outside the view
                         // disk draws nothing and earns no entry; a legend describes the drawn
                         // picture, and the drawn picture is view-dependent frame to frame)
    };
    bool legend_bits_sink[2] = { false, false };  // legend==nullptr callers still need an
                                                  // addressable bit slot; writes to the sink
                                                  // are the frame's no-legend path
    const Curve curves[2] = {
      { model.dispersion.value_red_deg, IM_COL32(242, 108, 92, 235), &model.dispersion.red_label,
        legend != nullptr ? &legend->dispersion_red : &legend_bits_sink[0] },
      { model.dispersion.value_blue_deg, IM_COL32(96, 145, 240, 235), &model.dispersion.blue_label,
        legend != nullptr ? &legend->dispersion_blue : &legend_bits_sink[1] },
    };
    for (const Curve& curve : curves) {
      // One walk draws the circle and reports its topmost drawn point — the label anchor (the
      // readout the eye meets). A partially visible circle says so: the drawn arc is the
      // in-view part, not the whole circle.
      const IsoDeltaWalk walk =
          WalkIsoDeltaCircle(dl, view, pic_min, pic_px, sun_pos, curve.delta_deg, curve.colour, 1.4f);
      if (legend != nullptr && walk.drawn_total > 0) {
        *curve.legend_bit = true;
        if (walk.partial()) {
          legend->dispersion_partial = true;
        }
      }
      if (walk.drawn_total > 0) {
        char line[112];
        std::snprintf(line, sizeof(line), "%s · %.3f°%s", curve.label->c_str(), curve.delta_deg,
                      walk.partial() ? " · arc in view" : "");
        const ImVec2 ts = ImGui::CalcTextSize(line);
        dl->AddRectFilled(
            ImVec2(static_cast<float>(walk.top_x) - ts.x * 0.5f - 3.0f, static_cast<float>(walk.top_y) - ts.y),
            ImVec2(static_cast<float>(walk.top_x) + ts.x * 0.5f + 3.0f, static_cast<float>(walk.top_y)),
            IM_COL32(16, 17, 20, 170));
        dl->AddText(ImVec2(static_cast<float>(walk.top_x) - ts.x * 0.5f, static_cast<float>(walk.top_y) - ts.y),
                    curve.colour, line);
      }
    }
  }
}

void DrawSkyPointMarker(ImDrawList* dl, const MapCanvasProjection& view, const SkyPointIntent& intent,
                        const ImVec2& pic_min, float pic_px, float* out_marker_px, float* out_marker_py) {
  if (!intent.valid || !intent.from_map_pick) {
    return;
  }
  const double pos[3] = { -static_cast<double>(intent.dir[0]), -static_cast<double>(intent.dir[1]),
                          -static_cast<double>(intent.dir[2]) };
  double x = 0.0;
  double y = 0.0;
  if (!ProjectToRect(view, pos, pic_min, pic_px, &x, &y)) {
    return;
  }
  const float cx = static_cast<float>(x);
  const float cy = static_cast<float>(y);
  const float r = UiPx(6.0f);
  dl->AddCircle(ImVec2(cx, cy), r, IM_COL32(255, 255, 255, 235), 0, 1.6f);
  dl->AddLine(ImVec2(cx - 2.0f * r, cy), ImVec2(cx - r - 1.0f, cy), IM_COL32(255, 255, 255, 200), 1.2f);
  dl->AddLine(ImVec2(cx + r + 1.0f, cy), ImVec2(cx + 2.0f * r, cy), IM_COL32(255, 255, 255, 200), 1.2f);
  dl->AddLine(ImVec2(cx, cy - 2.0f * r), ImVec2(cx, cy - r - 1.0f), IM_COL32(255, 255, 255, 200), 1.2f);
  dl->AddLine(ImVec2(cx, cy + r + 1.0f), ImVec2(cx, cy + 2.0f * r), IM_COL32(255, 255, 255, 200), 1.2f);
  *out_marker_px = cx;
  *out_marker_py = cy;
}

}  // namespace

void RenderAnalyzeMapRegion(GuiState& state, const ImVec2& size) {
  g_map_observation_timing.began_seconds = ObservationClockSeconds();
  // D3 — the reference-map | info split (contract §4.2), drawn by this function: the window's
  // content area splits into the LEFT region (the raster, its gestures and its state line,
  // under the region's own name ##AnalyzeRaypathView) and the RIGHT region (the wavelength,
  // the view buttons, the caliber block, the sky-point input and the legend — its own child,
  // ##AnalyzeRaypathInfo, native wheel scroll), with the draggable strip between. The stored
  // ratio is the split's identity (manual priority, §9.3 — no query result or selection ever
  // writes it); the frame derives pixels and clamps them at the two floors: ① the info region
  // stops at its floor and the map absorbs the rest, ② the map stops at its floor and both
  // pin, ③ — only when both floors cannot fit — the floors share the span proportionally and
  // the regions' internal scrolls take over (the map's manual route, the info's native wheel).
  const float style_spacing_x = ImGui::GetStyle().ItemSpacing.x;
  const float splitter_w = UiPx(kAnalyzeSplitterSize);
  const float avail_w = std::max(0.0f, size.x);
  const float map_floor = UiPx(kRaypathViewMapMinSize);
  const float info_floor = UiPx(kRaypathViewInfoMinSize);
  float info_w = state.analyze_raypath_view_info_ratio * std::max(0.0f, avail_w - splitter_w);
  if (avail_w >= map_floor + info_floor + splitter_w) {
    info_w = std::clamp(info_w, info_floor, avail_w - splitter_w - map_floor);
  } else {
    info_w = std::max(0.0f, (avail_w - splitter_w) * (info_floor / (info_floor + map_floor)));
  }
  const float map_w = std::max(0.0f, avail_w - splitter_w - info_w);

  // Wheel ownership is arbitrated by CLAIMING the wheel key for the whole time the pointer is
  // over the map region — not by the child flag alone. The flag only removes this child from
  // the native dispatch, and ImGui's wheeling-window resolution then bubbles the same event
  // up the parent chain: the host window scrolled natively while the gestures below zoomed
  // off the very same wheel — the double consumption had just moved up a level (the native
  // dispatch runs in NewFrame, before any rendering, so nothing set during rendering of the
  // same frame can stop it). The claim is what the dispatch reads — and the CONSUME side is
  // gated on the claim standing at the frame's START: on the takeover frame (the host
  // scrolled this region under a stationary pointer) no claim was standing yet, the dispatch
  // has already spent the event, and the gestures must not spend it again. The rule and its
  // transition cases live in analyze_region_wheel.hpp (the 3D orbit is the other claimant).
  // NoScrollWithMouse stays as the map region's own guard: its scroll path is the manual
  // route, nothing else. The info region takes the wheel natively — it registers no claim, so
  // the same event scrolls it when the pointer is over it.
  ImGui::BeginChild("##AnalyzeRaypathView", ImVec2(map_w, size.y), ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_NoScrollWithMouse);
  const bool wheel_standing = AnalyzeRegionWheelStanding();
  AnalyzeRegionWheelClaim(ImGui::IsWindowHovered(), AnalyzeMapWheelOwnerId());
  MapQuerySession& map = state.analysis.map;
  SkyPointIntent& intent = state.analysis.sky_point;
  const double now_s = ImGui::GetTime();

  // The header/state rows' join judgment, shared by both regions (the lambda reads the
  // CURRENT window's layout at call time). SameLine is a judgment here, not a default: every
  // successor joins the line only while it still fits — a clipped toggle or slider is an
  // unreachable control exactly where the control exists (the contract's content-adaptation
  // default: wrap, don't clip). The judgment reads the LAST ITEM's right edge against the
  // content region's right edge: after ItemSize commits a widget the cursor drops to the next
  // line, so GetContentRegionAvail().x there would report the whole fresh line's width and
  // every check would pass while the SameLine it guards still overflows.
  const auto fits_on_line = [&](float next_w) {
    const float line_right = ImGui::GetContentRegionMax().x + ImGui::GetWindowPos().x;
    return ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + next_w <= line_right;
  };
  // The header row opens with the overlay toggle — the window's own title carries the name
  // this row's old text label repeated. The toggle is the row's FIRST item, so it takes a fresh
  // line by construction (the fits_on_line judgment reads the LAST item's rect; with no
  // predecessor there is nothing to join — its old guard here would have read whatever widget
  // the parent window submitted before this child, a stale rect from outside the region).
  ImGuiStorage* storage = ImGui::GetStateStorage();
  const ImGuiID overlay_key = ImGui::GetID("overlays");
  bool overlays = storage->GetBool(overlay_key, true);
  if (ImGui::Checkbox("Overlays##map", &overlays)) {
    storage->SetBool(overlay_key, overlays);
  }

  const float canvas_avail_w = ImGui::GetContentRegionAvail().x;
  const float canvas_avail_h = ImGui::GetContentRegionAvail().y;
  const float pic_px = std::max(1.0f, canvas_avail_w);
  const float pic_h =
      std::max(1.0f, CanvasHeightBeforeReport(canvas_avail_h, 0.0f, UiPx(64.0f), ImGui::GetStyle().ItemSpacing.y));
  const ImVec2 pic_min = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton("##map_canvas", ImVec2(pic_px, pic_h));
  const bool canvas_hovered = ImGui::IsItemHovered();
  const bool canvas_active = ImGui::IsItemActive();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  auto dpi = ImGui::GetIO().DisplayFramebufferScale;
  // A detached map may live on another monitor. Its GLFW framebuffer, not the main
  // viewport's global IO scale, owns the conversion from this window's points to pixels.
  if (const auto* viewport = ImGui::GetWindowViewport(); viewport && viewport->PlatformHandle) {
    auto* host = static_cast<GLFWwindow*>(viewport->PlatformHandle);
    int width = 0, height = 0, fb_width = 0, fb_height = 0;
    glfwGetWindowSize(host, &width, &height);
    glfwGetFramebufferSize(host, &fb_width, &fb_height);
    if (width > 0 && height > 0 && fb_width > 0 && fb_height > 0)
      dpi = ImVec2(static_cast<float>(fb_width) / width, static_cast<float>(fb_height) / height);
  }
  MapCanvasProjection view;
  // The published main picture is this frame's pre-gesture view. Taking that same snapshot
  // avoids a one-frame fork when its input handler already wrote the next frame's camera.
  view.camera = g_panorama_vp.active ? g_panorama_vp.params.view_proj :
                g_preview_vp.active  ? g_preview_vp.params.view_proj :
                                       BuildPreviewViewProjFromRenderer(state.renderer);
  view.width = std::max(1, static_cast<int>(std::round(pic_px * dpi.x)));
  view.height = std::max(1, static_cast<int>(std::round(pic_h * dpi.y)));
  view.width_pt = pic_px;
  view.height_pt = pic_h;
  g_map_observation_timing.width = view.width;
  g_map_observation_timing.height = view.height;
  const auto display = DrawFrozenMap(map, view.camera, view.width, view.height);
  if (display.texture) {
    dl->AddImage(static_cast<ImTextureID>(static_cast<uintptr_t>(display.texture)), pic_min,
                 ImVec2(pic_min.x + pic_px, pic_min.y + pic_h), ImVec2(0, 1), ImVec2(1, 0));
  }
  dl->AddRect(pic_min, ImVec2(pic_min.x + pic_px, pic_min.y + pic_h), ImGui::GetColorU32(ImGuiCol_Separator));
  if (display.display_sampled) {
    dl->AddText(pic_min, ImGui::GetColorU32(ImGuiCol_Text), "Display sampled; raw cells unchanged");
  }

  // The frame anchors (sun / zenith / horizon) need the adopted map's own sun echo.
  LUMICE_PathMapInfo anchor_info{};
  anchor_info.struct_size = sizeof(anchor_info);
  const auto& caliber_source = map.adopted_result;
  const bool have_anchor_info =
      caliber_source && LUMICE_PathMapGetInfo(caliber_source.get(), &anchor_info) == LUMICE_PATH_READ_OK;
  // What this frame drew — the legend's input (the legend describes the drawn picture; absent
  // categories draw no entry). Filled by the anchor/overlay/marker draws below, read by the
  // info region's legend section.
  OverlayLegendState legend;
  if (have_anchor_info) {
    DrawFrameAnchors(dl, view, anchor_info, pic_min, pic_px, &legend);
  }
  if (overlays) {
    DrawSurveyOverlays(dl, state, view, pic_min, pic_px, &legend);
  }

  float marker_px = -1.0f;
  float marker_py = -1.0f;
  DrawSkyPointMarker(dl, view, intent, pic_min, pic_px, &marker_px, &marker_py);
  if (marker_px >= 0.0f) {
    legend.sky_point = true;
  }

  g_map_legend = legend;

  LUMICE_AnnotationView pick_view = PreviewAnnotationView(state, view.width, view.height);
  pick_view.lens_type = view.camera.lens_type;
  pick_view.lens_fov = view.camera.fov;
  pick_view.view_azimuth = view.camera.azimuth;
  pick_view.view_elevation = view.camera.elevation;
  pick_view.view_roll = view.camera.roll;
  pick_view.visible = view.camera.visible;
  pick_view.front = view.camera.front ? 1 : 0;
  const ImGuiIO& io = ImGui::GetIO();
  auto pick_cursor = [&]() -> std::optional<std::array<float, 3>> {
    const auto pixel = PreviewPointToCanvasPixel(io.MousePos.x - pic_min.x, io.MousePos.y - pic_min.y, dpi.x, dpi.y,
                                                 view.width, view.height);
    if (!pixel)
      return std::nullopt;
    std::array<float, 3> travel{};
    int valid = 0;
    if (LUMICE_UnprojectPixel(&pick_view, pixel->px, pixel->py, travel.data(), &valid) != LUMICE_OK || !valid)
      return std::nullopt;
    return travel;
  };
  auto move_point = [&]() {
    if (const auto travel = pick_cursor()) {
      intent.valid = true;
      intent.from_map_pick = true;
      std::copy(travel->begin(), travel->end(), intent.dir);
    }
  };
  using Owner = GuiState::AnalyzeWorkspaceSession::PanoramaPressOwner;
  auto& owner = state.workspace.map_press_owner;
  const bool explicit_pick = state.angular_dist_picker.armed || state.angular_dist_picker.swallow_drag_until_release ||
                             g_bg_pick.active || g_bg_pick.swallow_drag_until_release;
  const float mx = io.MousePos.x - marker_px;
  const float my = io.MousePos.y - marker_py;
  const float hit = UiPx(kAnalysisConeMarkerHitRadiusPt);
  const bool marker_hover = marker_px >= 0.0f && mx * mx + my * my <= hit * hit;
  if (canvas_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && owner == Owner::kNone) {
    owner = explicit_pick ? Owner::kExplicitPick :
            io.KeyAlt     ? Owner::kDirectPoint :
            marker_hover  ? Owner::kMarker :
                            Owner::kCamera;
    if (owner == Owner::kDirectPoint)
      move_point();
  }
  if (canvas_active && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    if (owner == Owner::kMarker && canvas_hovered)
      move_point();
    if (owner == Owner::kCamera && !LensIsFullSky(view.camera.lens_type) &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
      ApplyCameraOrbitDrag(state, io.MouseDelta, view.width, view.height);
    }
  }
  if (wheel_standing && canvas_hovered && !explicit_pick && (owner == Owner::kNone || owner == Owner::kCamera) &&
      io.MouseWheel != 0.0f) {
    ApplyCameraWheelZoom(state, io.MouseWheel);
  } else if (wheel_standing && !canvas_hovered && ImGui::IsWindowHovered() && io.MouseWheel != 0.0f) {
    const float step = std::floor(std::min(5.0f * ImGui::GetFontSize(), ImGui::GetWindowSize().y * 0.67f));
    ImGui::SetScrollY(ImGui::GetScrollY() - io.MouseWheel * step);
  }
  if (state.angular_dist_picker.armed && !g_bg_pick.active && canvas_hovered &&
      (owner == Owner::kExplicitPick || (owner == Owner::kNone && !ImGui::IsMouseDown(ImGuiMouseButton_Left)))) {
    PreviewParams params;
    params.view_proj = view.camera;
    ApplyAngularDistPickGesture(state, params, ImVec2(io.MousePos.x - pic_min.x, io.MousePos.y - pic_min.y), dpi.x,
                                dpi.y, view.width, view.height);
  }
  if (canvas_hovered) {
    const auto near_sample = pick_cursor();
    std::optional<std::array<double, 3>> far_sample;
    if (const auto pixel = PreviewPointToCanvasPixel(io.MousePos.x - pic_min.x, io.MousePos.y - pic_min.y, dpi.x, dpi.y,
                                                     view.width, view.height))
      far_sample = FrozenMapFarTravel(view.camera, view.width, view.height, pixel->px, pixel->py);
    if ((near_sample || far_sample) && ImGui::BeginTooltip()) {
      const auto show_reading = [&](const char* label, const double travel[3]) {
        const float direction[3] = { static_cast<float>(travel[0]), static_cast<float>(travel[1]),
                                     static_cast<float>(travel[2]) };
        float alt = 0.0f, az = 0.0f;
        DirToAltAz(direction, &alt, &az);
        ImGui::Text("%s · alt %.4f° az %.4f°", label, alt, az);
        const auto cell = ReadFrozenMapDirection(map, travel);
        if (cell) {
          const auto i = cell->index;
          const auto& info = cell->info;
          ImGui::Text("Reference r%zu c%zu · status %u", i / info.view_cols, i % info.view_cols,
                      static_cast<unsigned>(cell->pixels.status[i]));
          ImGui::Text("%.17g per sr · count %d", cell->pixels.values[i], cell->pixels.counts[i]);
          ImGui::Text("cell δ %.7g° × az %.7g° · %d/%d · λ %.1f · n=%d",
                      (info.view_delta_max_deg - info.view_delta_min_deg) / info.view_rows,
                      (info.view_az_max_deg - info.view_az_min_deg) / info.view_cols, info.members_evaluated,
                      info.member_count, info.wavelength_nm, info.sample_count);
        } else
          ImGui::TextUnformatted("Outside selected source coverage — unknown, not zero");
      };
      if (near_sample) {
        const double d[3] = { (*near_sample)[0], (*near_sample)[1], (*near_sample)[2] };
        show_reading("Near / pick direction", d);
      } else
        ImGui::TextUnformatted("Near side clipped — no point selection here");
      if (far_sample) {
        ImGui::Separator();
        show_reading("Far-side hint — separate direction, not a sum", far_sample->data());
      }
      ImGui::EndTooltip();
    }
  }

  ImGui::BeginChild("##MapState", RemainingReportSize());
  // The state line (the four visible states + the two supersession notes). It reads the
  // picture's own query state, so it stays with the picture; the controls it names — the
  // wavelength, the view buttons, the sky-point input — live in the info region beside it.
  if (display.partial)
    ImGui::TextWrapped("Partial map: %d/%d completed members", display.members_evaluated, display.member_count);
  const MapQueryState st = MapQueryStateFor(map);
  const bool superseded = MapSupersededFor(map);
  if (st == MapQueryState::kNoSelection) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped(
        "Select a class in the share list; its reference map takes this region. The sky-point input in the "
        "info panel beside it works without a map.");
    ImGui::PopStyleColor();
  } else if (st == MapQueryState::kCancelled) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped(
        "Map cancelled. Frozen map remains; Retry repeats the full-sky request. Observation never retries.");
    ImGui::PopStyleColor();
  } else if (st == MapQueryState::kRefused && !superseded) {
    ImGui::TextUnformatted("Map refused:");
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped("%s", map.adopted_error.c_str());
    ImGui::PopStyleColor();
  } else if (st == MapQueryState::kWaiting && map.adopted_result == nullptr) {
    // The queue is the truth the wait names (plan risk 4): the selection's survey — whose
    // chromatic legs run ~15 s on a full-dimensional class — goes first on the one worker, and
    // this map rides behind it. Only a map asked in that window can honestly say so. The line
    // is the pure function's output and is reported to the render-feedback field: what the
    // test reads is what was drawn (AC4).
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    const std::string wait_line = MapWaitLineFor(map, g_survey_executor.QueryOutstanding(QueryFace::kSurvey),
                                                 ImGui::GetTime(), g_survey_executor.InflightPollSnapshot());
    state.analysis.wait_feedback_texts.map_wait_line = wait_line;
    ImGui::TextWrapped("%s", wait_line.c_str());
    ImGui::PopStyleColor();
  } else if (superseded) {
    // The supersession note carries the Retry intent's elapsed wait and the poll's stage
    // clause while the map
    // query itself is computing (751; face-matched, so a queued survey's stage never lands here).
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    const std::string note = MapSupersedingNoteFor(map, ImGui::GetTime(), g_survey_executor.InflightPollSnapshot());
    state.analysis.wait_feedback_texts.map_superseding_note = note;
    ImGui::TextWrapped("%s", note.c_str());
    ImGui::PopStyleColor();
  }

  ImGui::EndChild();
  ImGui::EndChild();  // ##AnalyzeRaypathView — the map region

  // ---- D3: the draggable strip between the two regions (contract §4.2) ----
  // The same recipe the workspace splitters run (invisible button + hover/active line +
  // delta-only write from the displayed boundary): the drag moves the stored ratio, the layout
  // re-derives the pixels each frame and clamps them at the floors, so restoring the window
  // size restores the split. Dragging right widens the map (the info region's share shrinks),
  // matching D1's direction convention.
  {
    const float strip_h = size.y;
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::InvisibleButton("##RaypathViewSplitter", ImVec2(splitter_w, strip_h));
    const bool strip_hovered = ImGui::IsItemHovered();
    const bool strip_active = ImGui::IsItemActive();
    const float mouse_dx = ImGui::GetIO().MouseDelta.x;
    if (strip_active && mouse_dx != 0.0f && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
      // Delta from the displayed boundary (the clamped split), never from the raw stored
      // value: at a window whose layout is clamped away from the stored ratio the two differ,
      // and a raw write spends the first stretch of the drag crawling through the clamp band.
      const float shown_info_w = std::clamp(info_w, info_floor, std::max(info_floor, avail_w - splitter_w - map_floor));
      const float new_info_w =
          std::clamp(shown_info_w - mouse_dx, info_floor, std::max(info_floor, avail_w - splitter_w - map_floor));
      state.analyze_raypath_view_info_ratio = new_info_w / std::max(1.0f, avail_w - splitter_w);
    }
    if (strip_hovered || strip_active) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    const ImU32 strip_col = ImGui::GetColorU32(strip_active  ? ImGuiCol_SeparatorActive :
                                               strip_hovered ? ImGuiCol_SeparatorHovered :
                                                               ImGuiCol_Separator);
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(ImGui::GetItemRectMin().x + splitter_w * 0.5f, ImGui::GetItemRectMin().y),
        ImVec2(ImGui::GetItemRectMin().x + splitter_w * 0.5f, ImGui::GetItemRectMax().y), strip_col, 1.0f);
    ImGui::SameLine(0.0f, 0.0f);
  }

  // ---- The info region: the picture's companion facts (contract §6's right side) ----
  // The wavelength, the two view buttons, the caliber block, the sky-point input and (Step 4)
  // the legend. It scrolls natively: the wheel over it is not claimed by any region, so
  // ImGui's own dispatch scrolls it — the reach whose existence does not depend on measuring
  // the content that decides the measure (the same ladder rung the old single-column region
  // reached through its manual route).
  ImGui::BeginChild("##AnalyzeRaypathInfo", ImVec2(info_w, size.y), ImGuiChildFlags_Borders);
  // The identity line, the right column's FIRST row (the disposition ledger's slot 1): the map's
  // one echo of the selection, worded by the list's own source (SelectedClassRowLabelFor — the
  // same spelling the list shows). The main window can be occluded while this window is open,
  // so the column names the chain being looked at before anything else; the long report below
  // no longer repeats it (its old in-report copy moved here, not duplicated).
  const std::string identity_label = SelectedClassRowLabelFor(state);
  if (identity_label.empty()) {
    ImGui::TextDisabled("(no class selected)");
  } else {
    ImGui::TextWrapped("%s", identity_label.c_str());
  }
  const bool quality_busy =
      map.intent_valid && !map.cancelled_for_key && map.answered_id != map.intent_id && map.adopted_id != map.intent_id;
  const bool quality_retry = map.cancelled_for_key || !map.adopted_error.empty();
  const char* quality_label = quality_busy ? "Cancel###map_quality" : "Retry###map_quality";
  const float quality_width = std::max(ImGui::CalcTextSize("Retry").x, ImGui::CalcTextSize("Cancel").x) +
                              2.0f * ImGui::GetStyle().FramePadding.x;
  ImGui::BeginDisabled(!quality_busy && !quality_retry);
  if (ImGui::Button(quality_label, ImVec2(quality_width, ImGui::GetFrameHeight()))) {
    if (quality_busy)
      CancelMap(state, g_survey_executor);
    else
      RetryMap(state, g_survey_executor, now_s);
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    if (quality_busy)
      ImGui::SetTooltip("Cancel only this map calculation; keep the current frozen map.");
    else if (quality_retry)
      ImGui::SetTooltip("Retry the current full-sky request, independent of the observation camera.");
    else
      ImGui::SetTooltip(
          "Observation never recomputes: the map answers only an explicit full-sky request, independent of the "
          "observation camera.");
  }
  // The disposition ledger's slot is ONE row — action and state side by side ("Cancel/Retry＋
  // 状态"). The row's height is the button's at every tier, so the stable-space contract (§15:
  // a state change must not move the rows below) holds without an empty stand-in line; the
  // idle tier draws the button alone (its static explanation in the tooltip; the old constant
  // "Observe · no calculation" is deleted — a nothing-happened line is presentation noise).
  // Asynchronous prose belongs in the scrolling report below.
  if (quality_busy || quality_retry) {
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextUnformatted(quality_busy ? "Computing full sky" : "Retry full sky");
    ImGui::PopStyleColor();
  }

  // The wavelength: an explicit value in [350, 900], or the engine's reference (0). The scratch
  // float shows the effective value either way; editing it asks the explicit wavelength — a key
  // change the pump turns into a new request (the engine has no warm map: every lambda is a
  // real re-evaluation). One ledger slot ("λ / reference"): the track yields to the reference
  // button while both fit one line — the 60 pt floor keeps a usable track at every width the
  // info floor can produce — and the button wraps below at whatever cannot fit.
  float lambda_scratch = 550.0f;
  if (map.adopted_result != nullptr) {
    LUMICE_PathMapInfo info{};
    info.struct_size = sizeof(info);
    if (LUMICE_PathMapGetInfo(map.adopted_result.get(), &info) == LUMICE_PATH_READ_OK) {
      lambda_scratch = static_cast<float>(info.wavelength_nm);
    }
  }
  if (state.analysis.map_wavelength_nm > 0.0) {
    lambda_scratch = static_cast<float>(state.analysis.map_wavelength_nm);
  }
  const float reference_w = ImGui::CalcTextSize("reference").x + ImGui::GetStyle().FramePadding.x * 2.0f;
  float lambda_track = UiPx(140.0f);
  lambda_track = std::min(lambda_track, ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("λ").x -
                                            ImGui::GetStyle().ItemInnerSpacing.x);
  lambda_track = std::max(lambda_track, UiPx(60.0f));
  ImGui::SetNextItemWidth(lambda_track);
  if (ImGui::SliderFloat("λ##map", &lambda_scratch, 350.0f, 900.0f, "%.0f nm", ImGuiSliderFlags_AlwaysClamp)) {
    state.analysis.map_wavelength_nm = static_cast<double>(lambda_scratch);
  }
  // The λ reading's one owner is this control's own echo; its tooltip names the source rule
  // (the ledger: 真实来源进 λ tooltip — an explicit value asks itself, the engine's reference
  // answers until one is set). Editing asks a key change the pump turns into a real recompute.
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip(
        state.analysis.map_wavelength_nm > 0.0 ?
            "The wavelength this map answers at — an explicit value; editing it asks a new computation." :
            "The wavelength this map answers at — the engine's reference; editing the slider asks an explicit one.");
  }
  ImGui::BeginDisabled(state.analysis.map_wavelength_nm <= 0.0);
  if (ImGui::SmallButton("reference##map_lambda")) {
    state.analysis.map_wavelength_nm = 0.0;
  }
  ImGui::EndDisabled();
  // The view buttons: the map's two steering verbs, beside the wavelength they steer with.
  // The row joins only while it fits — the info floor's narrowest width wraps them onto their
  // own line rather than clip a view button.
  const float fit_w = ImGui::CalcTextSize("Fit##map").x + ImGui::GetStyle().FramePadding.x * 2.0f;
  const float full_sky_w = ImGui::CalcTextSize("Full sky##map").x + ImGui::GetStyle().FramePadding.x * 2.0f;
  if (fits_on_line(fit_w + style_spacing_x + full_sky_w)) {
    ImGui::SameLine();
  }
  ImGui::BeginDisabled(!display.fit.ok);
  if (ImGui::SmallButton("Fit##map")) {
    const auto& fit = display.fit;
    UnlockLookAt(state);
    if (2.0f * fit.radius_deg > LUMICE_MaxFov(static_cast<LUMICE_LensType>(LUMICE_LENS_TYPE_FISHEYE_EQUIDISTANT))) {
      SelectLensType(state, kLensTypeRectangular);
      state.renderer.visible = kVisibleFull;
      state.renderer.front = false;
    } else {
      SelectLensType(state, kLensTypeFisheyeEquidist);
      state.renderer.fov = 2.0f * fit.radius_deg;
      state.renderer.elevation = std::asin(std::clamp(fit.centre_pos[2], -1.0f, 1.0f)) * static_cast<float>(kRad2DegD);
      state.renderer.azimuth = std::atan2(fit.centre_pos[1], fit.centre_pos[0]) * static_cast<float>(kRad2DegD);
      state.renderer.roll = 0.0f;
    }
    ClampRendererViewInvariants(state.renderer);
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip(
        "Fit sampled map — changes BOTH pictures using the frozen full-sky reference."
        "\nPartial samples are not a full-class support certificate. No map calculation.");
  if (fits_on_line(full_sky_w))
    ImGui::SameLine();
  if (ImGui::SmallButton("Full sky##map")) {
    UnlockLookAt(state);
    SelectLensType(state, kLensTypeRectangular);
    state.renderer.visible = kVisibleFull;
    state.renderer.front = false;
    ClampRendererViewInvariants(state.renderer);
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Shared full-sky preset — changes BOTH pictures, keeps the frozen map.");
  // The steering group's one "shared view" mark (the disposition ledger): both verbs move BOTH
  // pictures, and the column says so once beside them — the tooltips carry the per-verb rest.
  if (fits_on_line(ImGui::CalcTextSize("· shared view").x)) {
    ImGui::SameLine();
  }
  ImGui::TextDisabled("· shared view");

  // ---- The sky-point input: always available (AC3: no map required to name a target) ----------
  ImGui::Separator();
  ImGuiStorage* sky_storage = ImGui::GetStateStorage();
  const ImGuiID alt_key = ImGui::GetID("sky_input_alt");
  const ImGuiID az_key = ImGui::GetID("sky_input_az");
  float input_alt = sky_storage->GetFloat(alt_key, 0.0f);
  float input_az = sky_storage->GetFloat(az_key, 0.0f);
  // The row wraps when the cell cannot hold it — the side-by-side cell is at its content floor
  // and the stacked form can be narrower than that floor legitimately (the main view's own soft
  // floor minus padding sits below the row's width), and a forced SameLine would clip the Set
  // button exactly where typing is the only way to name a target.
  // Labeled widgets draw their label right of the frame, so each successor's footprint includes
  // the label's own advance.
  const float style_inner_x = ImGui::GetStyle().ItemInnerSpacing.x;
  // The row's fresh-line width, captured here for the Set button's form ladder below: a label
  // wider than THIS cannot fit any line the cell can produce, and wrapping is no rescue.
  const float sky_row_full_w = ImGui::GetContentRegionAvail().x;
  // The inputs are elastic on the same rule as the λ track: a fixed 90 pt input ends past the
  // clip at the narrowest legal cells (③ split under 2x scale — ~76 pt for this row), and this
  // child never scrolls. The 40 pt floor keeps a typeable field at every width the ③ formula
  // can produce; the label rides right of the frame and its advance is part of the footprint.
  const float alt_label_w = ImGui::CalcTextSize("alt°").x;
  const float alt_track =
      std::max(UiPx(40.0f), std::min(UiPx(90.0f), ImGui::GetContentRegionAvail().x - alt_label_w - style_inner_x));
  ImGui::SetNextItemWidth(alt_track);
  if (ImGui::InputFloat("alt°##sky", &input_alt, 0.0f, 0.0f, "%.1f")) {
    sky_storage->SetFloat(alt_key, input_alt);
  }
  const float az_label_w = ImGui::CalcTextSize("az°").x;
  const bool az_same_line = fits_on_line(UiPx(90.0f) + style_inner_x + az_label_w);
  if (az_same_line) {
    ImGui::SameLine();
  }
  float az_track = UiPx(90.0f);
  if (!az_same_line) {
    az_track =
        std::max(UiPx(40.0f), std::min(UiPx(90.0f), ImGui::GetContentRegionAvail().x - az_label_w - style_inner_x));
  }
  ImGui::SetNextItemWidth(az_track);
  if (ImGui::InputFloat("az°##sky", &input_az, 0.0f, 0.0f, "%.1f")) {
    sky_storage->SetFloat(az_key, input_az);
  }
  // The Set button's forms (the Exclude/CSV ladder, one "###" id across them): at the
  // compressed tiers' narrowest cells the full label exceeds even a fresh line — the verb
  // carries the action and the typed-coordinate flow around it carries the meaning. The join
  // stays a judgment on whichever label will render.
  const float style_fp_x = ImGui::GetStyle().FramePadding.x;
  const float set_full_w = ImGui::CalcTextSize("Set sky point").x + style_fp_x * 2.0f;
  const bool set_narrow = set_full_w > sky_row_full_w;
  const float set_w = set_narrow ? ImGui::CalcTextSize("Set###sky_set").x + style_fp_x * 2.0f : set_full_w;
  if (fits_on_line(set_w)) {
    ImGui::SameLine();
  }
  // No tooltip on the verb form: a tooltip at the cursor swallows the very click that was
  // aiming at the button beneath it, and the typed-coordinate inputs this button serves sit
  // directly above it — the verb next to them is its own explanation.
  const bool set_clicked = ImGui::SmallButton(set_narrow ? "Set###sky_set" : "Set sky point###sky_set");
  if (set_clicked) {
    float dir[3];
    AltAzToDir(input_alt, input_az, dir);
    intent.valid = true;
    intent.from_map_pick = false;
    intent.dir[0] = dir[0];
    intent.dir[1] = dir[1];
    intent.dir[2] = dir[2];
  }
  if (fits_on_line(ImGui::CalcTextSize("Clear").x + style_fp_x * 2.0f)) {
    ImGui::SameLine();
  }
  ImGui::BeginDisabled(!intent.valid);
  if (ImGui::SmallButton("Clear##sky")) {
    intent = SkyPointIntent{};
  }
  ImGui::EndDisabled();
  // The current target, adjacent to the inputs that name it (the disposition ledger's slot 5):
  // it is what the point face queries — distinct from the fields above, which hold whatever the
  // user is typing until Set is pressed, so a still-computing older target reads as the intent
  // it is, never as the new input. The pick/typed source words live in this line's tooltip (the
  // input group's), and the report's second echo of the target is deleted with the move.
  // Wrapped like every prose line here: the narrowest info floor folds the row, it does not
  // clip it.
  if (intent.valid) {
    float target_alt = 0.0f;
    float target_az = 0.0f;
    DirToAltAz(intent.dir, &target_alt, &target_az);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped("target: alt %.2f° az %.2f°", target_alt, target_az);
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(intent.from_map_pick ? "Set from a map pick." : "Set from typed coordinates.");
    }
  } else {
    // The row is reserved at every tier (the stable-space contract: setting a first target
    // must not move the slots below it — the probe test holds the Pose button's rect across
    // exactly that transition). No label in the empty tier: the widgets above already say the
    // empty state.
    ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeight()));
  }

  // Navigation is display-only, like the main window's Selection header entry buttons (§16.3):
  // every state's entry into the pose window sits outside the growing report, available before
  // a class/point or drawable result exists. Only geometry chooses the label's line break;
  // query states never move this operation stack.
  ImGui::Separator();
  const float pose_label_w = ImGui::CalcTextSize(kPoseViewWindowTitle).x + style_fp_x * 2.0f;
  const char* pose_label =
      pose_label_w <= ImGui::GetContentRegionAvail().x ? PoseButtonLabel() : PoseButtonWrappedLabel();
  if (ImGui::Button(pose_label)) {
    state.analyze_pose_view_open = true;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", PoseButtonTooltip());
  }
  // The constant purpose text moved into the tooltip (the disposition ledger's slot 6 — a fixed
  // explanation must not occupy operation area); the column carries instead only a next step
  // when one exists: no class, then no point. With both present the stack stays quiet. The
  // wording is the compact-without-demotion form (the ladder's L1/L2 rule): one line at the
  // narrowest info floor, the longer where-withal riding the button's tooltip.
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
  if (identity_label.empty()) {
    ImGui::TextWrapped("Select a class first.");
  } else if (!intent.valid) {
    ImGui::TextWrapped("Set a sky point above.");
  }
  ImGui::PopStyleColor();

  ImGui::BeginChild("##MapReport", RemainingReportSize());
  // The class answer (contract §16.2): the adopted survey document, drawn by the
  // complete-answer renderer — the same default layer and fold groups the renderer gives every
  // host, read back from the SAME adopted result the main window's summary reads (no second
  // model, no second session; opening or closing this window changes no query lifetime). The
  // four-state shell stays in the main window (§16.2's split): while the survey computes, is
  // cancelled or refused, this section draws nothing here — the answer region carries answers.
  if (state.analysis.survey.adopted_result) {
    const SurveyOverviewModel survey_model = BuildSurveyOverviewModel(*state.analysis.survey.adopted_result);
    ImGui::TextDisabled("Class answer");
    RenderSurveyOverviewModel(survey_model);
    ImGui::Separator();
  }
  // The sky-point answer (contract §16.2): the completed point document read in full, sorted
  // into its own fold pair. Sits after the class answer, before the map's own report — the
  // ledger's answer-region order. Like the class answer: nothing here outside the completed
  // state; the four-state shell and the cancel actions stay in the main window.
  if (state.analysis.point.adopted_result) {
    ImGui::TextDisabled("Sky point answer");
    RenderPointAnswerBlock(state);
    ImGui::Separator();
  }
  ImGui::TextWrapped("Stripes = unknown, purple = singular (not zero).");
  // The identity line and the sky-point echo used to sit here; both moved up the fixed slots
  // (identity to the column's first row, the target beside the input group) and the report no
  // longer repeats either. The old reference-cell line moved into the caliber's Calculation
  // fold (the same numbers, the ledger's L4 tier); the source-fixed-scale sentence moved with
  // it. The Separator keeps the picture-facts above apart from the answer block below.
  ImGui::Separator();
  if (state.analysis.map_wavelength_nm <= 0.0 && map.adopted_result == nullptr) {
    // The λ's unrechoiced state stays default (the ledger: 尚未回显的未知性默认) — the map has
    // not answered at any wavelength yet, and the slider's tooltip names the source rule.
    ImGui::TextWrapped("The engine-chosen reference wavelength is used until a map echoes it.");
  }
  // The caliber block: every fact read back through the result's own readers into the pure
  // composers (the unit layer holds them to the readback), then drawn — the ledger's tiers:
  // the short quantity and its limits default, the full definition and premises behind
  // Physics details, the grid and execution numbers behind Calculation details. The report
  // follows every operation: its native scroll cannot overlap or reposition the
  // typed-coordinate controls, even while a long caliber or refusal arrives.
  if (caliber_source && have_anchor_info) {
    ImGui::TextDisabled("Reference caliber");
    MapCaliberInput in;
    in.info = anchor_info;
    LUMICE_PathSurveyEvidence evidence{};
    evidence.struct_size = sizeof(evidence);
    in.have_evidence = LUMICE_PathMapGetEvidence(caliber_source.get(), &evidence) == LUMICE_PATH_READ_OK;
    in.evidence = evidence;
    LUMICE_PathQuantitySpec quantity{};
    quantity.struct_size = sizeof(quantity);
    in.have_quantity = LUMICE_PathMapGetQuantity(caliber_source.get(), &quantity) == LUMICE_PATH_READ_OK;
    in.quantity = quantity;
    if (anchor_info.point_mass_available != 0) {
      LUMICE_PathMapPointMass mass{};
      mass.struct_size = sizeof(mass);
      in.have_mass = LUMICE_PathMapGetPointMass(caliber_source.get(), &mass) == LUMICE_PATH_READ_OK;
      in.mass = mass;
      for (size_t i = 0; i < mass.method_count && i < kMapCaliberMaxMethods; ++i) {
        in.methods[i] = mass.methods[i];
      }
    }
    in.completed = true;  // the named finite source completed, independently of a newer intent
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    for (const std::string& line : MapCaliberDefaultLines(in)) {
      ImGui::TextWrapped("%s", line.c_str());
    }
    ImGui::PopStyleColor();
    if (FoldOpen(kSurveyPhysicsDetailsLabel, "map_caliber")) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
      for (const std::string& line : MapCaliberPhysicsLines(in)) {
        ImGui::TextWrapped("%s", line.c_str());
      }
      ImGui::PopStyleColor();
    }
    if (FoldOpen(kSurveyCalculationDetailsLabel, "map_caliber")) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
      for (const std::string& line : MapCaliberCalculationLines(in)) {
        ImGui::TextWrapped("%s", line.c_str());
      }
      ImGui::PopStyleColor();
    }
  }

  // The legend: one entry per category THIS frame actually drew, each with its one-line
  // meaning (the legend describes the drawn picture — a category absent from the frame draws
  // no entry). The honesty wording on the dispersion entry is the contract's: the circles are
  // the measured minimum-deviation geometry, not a whole-circle visibility claim, and a
  // partially visible circle says which part the picture carries.
  ImGui::Separator();
  ImGui::TextDisabled("Legend");
  {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    // The short keys are the default tier (the ledger: 默认短形状/颜色键，长词典进折叠): each
    // entry names the drawn category and its marker shape — the meanings and honesty qualifiers
    // ride the Legend explanations group below, with the two short-form limits that must stay
    // readable at the keys (positions are not whole-circle visibility; a partial circle names
    // its arc). Every entry is the Bullet()+TextWrapped pair (BulletText and SameLine+Text size
    // a single line and clip at the region's right edge); the wrap is the vertical scroll's own
    // overflow channel.
    if (legend.anchor_sun) {
      ImGui::Bullet();
      ImGui::TextWrapped("sun marker");
    }
    if (legend.anchor_zenith) {
      ImGui::Bullet();
      ImGui::TextWrapped("zenith cross");
    }
    if (legend.anchor_horizon) {
      ImGui::Bullet();
      ImGui::TextWrapped("horizon circle");
    }
    for (const std::string& kind : legend.object_kinds) {
      const OverlayMarkerStyle style = OverlayMarkerStyleFor(kind, LUMICE_PATH_STATE_CERTIFIED);
      ImGui::Bullet();
      // The kind's own marker shape, drawn beside the words (the legend and the picture agree).
      ImDrawList* legend_dl = ImGui::GetWindowDrawList();
      const ImVec2 p = ImGui::GetCursorScreenPos();
      const float my = p.y + ImGui::GetTextLineHeight() * 0.5f;
      const float mx = p.x + ImGui::GetFrameHeight() * 0.5f - UiPx(3.0f);
      const float r = UiPx(4.0f);
      const ImVec2 pts[4] = { ImVec2(mx, my - r), ImVec2(mx + r, my), ImVec2(mx, my + r), ImVec2(mx - r, my) };
      switch (style.shape) {
        case OverlayMarkerShape::kCircle:
          legend_dl->AddCircleFilled(ImVec2(mx, my), r * 0.8f, style.colour);
          break;
        case OverlayMarkerShape::kSquare:
          legend_dl->AddRectFilled(ImVec2(mx - r * 0.75f, my - r * 0.75f), ImVec2(mx + r * 0.75f, my + r * 0.75f),
                                   style.colour);
          break;
        case OverlayMarkerShape::kTriangle:
          legend_dl->AddTriangleFilled(pts[0], pts[1], pts[2], style.colour);
          break;
        case OverlayMarkerShape::kTriangleDown:
          legend_dl->AddTriangleFilled(pts[0], pts[3], pts[2], style.colour);
          break;
        case OverlayMarkerShape::kDiamond:
        case OverlayMarkerShape::kFallbackDiamond:
          legend_dl->AddConvexPolyFilled(pts, 4, style.colour);
          break;
      }
      ImGui::Dummy(ImVec2(ImGui::GetFrameHeight(), 0.0f));
      ImGui::SameLine(0.0f, 0.0f);
      ImGui::TextWrapped("%s", kind.c_str());
    }
    if (legend.dispersion_red || legend.dispersion_blue) {
      ImGui::Bullet();
      if (legend.dispersion_red && legend.dispersion_blue) {
        ImGui::TextWrapped("dispersion circles (red inside blue) — positions, not visibility");
      } else {
        // One circle's arc sits outside the view disk this frame: the entry names the one that
        // drew — the other half's absence is the honest state, not a dropped category.
        ImGui::TextWrapped("%s dispersion circle — positions, not visibility", legend.dispersion_red ? "red" : "blue");
      }
      if (legend.dispersion_partial) {
        ImGui::TextWrapped("partial circles draw their in-view arc (\"arc in view\").");
      }
    }
    if (legend.sky_point) {
      ImGui::Bullet();
      ImGui::TextWrapped("sky-point marker");
    }
    // The long dictionary: the entries' meanings and honesty qualifications, behind one
    // closed-by-default fold (the ledger's Legend explanations group).
    if (FoldOpen(kMapLegendExplanationsLabel, "map_legend")) {
      if (legend.anchor_sun) {
        ImGui::TextWrapped("sun marker — the delta axis's zero");
      }
      if (!legend.object_kinds.empty()) {
        ImGui::TextWrapped("each kind's marker — the survey's located conclusion, at the position the card names");
      }
      if (legend.dispersion_red || legend.dispersion_blue) {
        ImGui::TextWrapped(
            "dispersion circles — the measured minimum-deviation geometry at the reference wavelengths; positions, "
            "not a whole-circle visibility or contribution claim");
      }
      if (legend.sky_point) {
        ImGui::TextWrapped("sky-point marker — the target the point face queries");
      }
    }
    ImGui::PopStyleColor();
  }

  ImGui::EndChild();  // ##MapReport
  ImGui::EndChild();  // ##AnalyzeRaypathInfo — the info region
  g_map_observation_timing.cpu_finished_seconds = ObservationClockSeconds();
}

void RenderAnalyzeRaypathViewWindow(GuiState& state) {
  PruneFrozenMapDisplay(state.analysis.map);
  if (g_survey_overlay_cache.source.lock() != state.analysis.survey.adopted_result ||
      g_survey_overlay_cache.id != state.analysis.survey.intent_id)
    g_survey_overlay_cache = {};
  if (!state.analyze_raypath_view_open) {
    if (state.workspace.map_press_owner != GuiState::AnalyzeWorkspaceSession::PanoramaPressOwner::kNone)
      state.workspace.map_press_owner = GuiState::AnalyzeWorkspaceSession::PanoramaPressOwner::kCancelled;
    return;
  }
  // The pumps run at RenderWorkspacePanels' entry since 755.5 (every GUI frame, windows open or
  // not); this window keeps only its own wait-feedback hygiene: on the Render-workspace frames
  // it is the map fields' only render site, so a field's content is exactly what the user saw
  // THIS frame — the site that can draw it owns the clear. On an Analyze frame the workspace
  // entry already pumped and cleared; this window's map region is then that frame's only
  // map-field writer.
  if (state.workspace.mode != GuiState::WorkspaceMode::kAnalyze) {
    WaitFeedbackTexts& feedback = state.analysis.wait_feedback_texts;
    feedback.map_wait_line.clear();
    feedback.map_superseding_note.clear();
  }
  // The Colors window's size recipe (the four-tier table's free-form row): FirstUseEver at the
  // contract's suggested default, re-applied on a UI-scale change (WindowResizeCondForScale,
  // theme.hpp) so a scale bump grows the window with its UiPx()-derived content instead of
  // leaving it pinned at the old pixel size. The default is clamped to the viewport: at the 2x
  // scale the suggested 900x560 pt is 1800x1120 px — past the shortest supported window's own
  // height — and a first appearance larger than the viewport hides its own edges behind the
  // screen; the free-form tier's two-axis resize starts from what fits.
  static float s_sized_for_scale = 0.0f;
  const ImGuiCond size_cond = WindowResizeCondForScale(s_sized_for_scale, ImGuiCond_FirstUseEver);
  const ImVec2 want_size = ImVec2(UiPx(900.0f), UiPx(560.0f));
  const ImVec2 vp_size = ImGui::GetMainViewport()->Size;
  // The window-level minimum (the contract's §11 R3 narrowest face, UiPx-scaled like every other
  // geometry constant): constraints have no Cond — ImGui re-clamps the live size into this bound
  // at EVERY Begin, so the frames of the user's own border drag cannot carry the window below the
  // narrowest layout the split floors and the wrap ladder were verified at. Without it, 480x360
  // is only a size the tests can set; with it, it is the smallest size the user can reach (the
  // same fact, one mechanism). Clamped to the viewport like the default size: a screen smaller
  // than the floor shows the largest prefix of it rather than a window whose far edge is stuck
  // off-screen.
  const ImVec2 min_size =
      ImVec2(std::min(UiPx(kRaypathViewMinWidth), vp_size.x), std::min(UiPx(kRaypathViewMinHeight), vp_size.y));
  ImGui::SetNextWindowSizeConstraints(min_size, ImVec2(FLT_MAX, FLT_MAX));
  ImGui::SetNextWindowSize(ImVec2(std::min(want_size.x, vp_size.x), std::min(want_size.y, vp_size.y)), size_cond);
  // NoDocking: the window is a free-form surface, not shell layout (the contract's §7 boundary
  // note); NoCollapse: a collapsed title bar hides the map without freeing its queries — the
  // honest off state is closed, via the same flag the title bar's X writes through `p_open`.
  if (!ImGui::Begin(kRaypathViewWindowTitle, &state.analyze_raypath_view_open,
                    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoScrollWithMouse)) {
    ImGui::End();
    if (state.workspace.map_press_owner != GuiState::AnalyzeWorkspaceSession::PanoramaPressOwner::kNone)
      state.workspace.map_press_owner = GuiState::AnalyzeWorkspaceSession::PanoramaPressOwner::kCancelled;
    return;
  }
  // The body IS the map region (the whole capability moved out of the depth band in one piece:
  // the raster and its gestures, the overlays, the state line, the caliber block, the sky-point
  // tail). The region's own child keeps its name — the tests' addressing anchors to it. Its
  // honest no-selection state (below, in the region) is the window's empty state.
  RenderAnalyzeMapRegion(state, VisibleContentSize());
  ImGui::End();
}

}  // namespace lumice::gui
