#include "gui/analyze_map_view.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "gui/analysis_panel.hpp"  // SelectedClassIdentityFor / SurveySelectionKeyFor — the one identity chain
#include "gui/analyze_overview.hpp"
#include "gui/analyze_region_wheel.hpp"  // the wheel claim — one arbitration for both gesture regions
#include "gui/analyze_workspace.hpp"     // PumpAnalyzeQueries — the pumps this window's Render-mode frames run
#include "gui/app.hpp"
#include "gui/canvas_report_layout.hpp"
#include "gui/gl_common.h"
#include "gui/gui_constants.hpp"
#include "gui/gui_logger.hpp"
#include "gui/gui_state.hpp"
#include "gui/theme.hpp"
#include "imgui.h"
#include "util/sky_direction.hpp"

namespace lumice::gui {


namespace {

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

double Wrap360(double x) {
  x = std::fmod(x, 360.0);
  if (x < 0.0) {
    x += 360.0;
  }
  return x;
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

// Rotate `from` toward `to` by `angle_rad` about the (from×to) axis — Rodrigues. The angles'
// sanity (0 <= angle <= pi, unit vectors) is the callers'.
void RotateToward(const double from[3], const double to[3], double angle_rad, double out[3]) {
  double axis[3];
  Cross3(from, to, axis);
  Normalize3(axis);
  const double cos_a = std::cos(angle_rad);
  const double sin_a = std::sin(angle_rad);
  const double dot = Dot3(from, to);
  // out = from·cos + (axis×from)·sin + axis·(axis·from)·(1−cos); axis·from == 0 here.
  double axf[3];
  Cross3(axis, from, axf);
  for (int k = 0; k < 3; ++k) {
    out[k] = from[k] * cos_a + axf[k] * sin_a;
  }
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

}  // namespace

// ---- The caliber composer (pure; the unit layer holds it to the readback) -------------------------

std::vector<std::string> MapCaliberLines(const MapCaliberInput& in) {
  std::vector<std::string> lines;
  char line[512];
  const LUMICE_PathMapInfo& info = in.info;
  if (info.view_full_sky != 0) {
    std::snprintf(line, sizeof(line),
                  "λ %.1f nm · %d/%d members · n=%d lattice · view: full sky %d×%d · kept %lld events",
                  info.wavelength_nm, info.members_evaluated, info.member_count, info.sample_count, info.view_rows,
                  info.view_cols, info.kept_count);
  } else {
    std::snprintf(line, sizeof(line),
                  "λ %.1f nm · %d/%d members · n=%d lattice · view: δ [%.1f, %.1f]° az [%.1f, %.1f]° %d×%d · kept %lld "
                  "events",
                  info.wavelength_nm, info.members_evaluated, info.member_count, info.sample_count,
                  info.view_delta_min_deg, info.view_delta_max_deg, info.view_az_min_deg, info.view_az_max_deg,
                  info.view_rows, info.view_cols, info.kept_count);
  }
  lines.emplace_back(line);
  if (in.have_quantity) {
    std::snprintf(line, sizeof(line), "%s, %s, %s · sun %.1f°/%.1f° · %s", QuantityKindName(in.quantity.kind),
                  AggregationName(in.quantity.aggregation), UnitName(in.quantity.unit), info.sun_altitude_deg,
                  info.sun_azimuth_deg, info.shape_is_nominal != 0 ? "nominal geometry" : "scene geometry");
    lines.emplace_back(line);
  }
  if (in.have_evidence && in.evidence.geometry_basis.length > 0) {
    lines.emplace_back(in.evidence.geometry_basis.data, in.evidence.geometry_basis.length);
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
                  "Rank-0 arm: m = %.6g ± %.2g (%s) at the sun's cell%s — this class's sky image IS the mass; the "
                  "zero cells are not no-light.",
                  in.mass.m, in.mass.m_error, methods.c_str(), in.mass.pixel < 0 ? " (outside this view)" : "");
    lines.emplace_back(line);
  }
  return lines;
}

// ---- Pure view math -------------------------------------------------------------------------------

bool MapViewSettled(double now_s, const MapViewState& view) {
  return !view.gesture_active && (now_s - view.last_view_change_s) >= kMapSettleSeconds;
}

MapDiskProjector MakeMapDiskProjector(const float centre_pos[3], float radius_deg) {
  MapDiskProjector p;
  double c[3] = { centre_pos[0], centre_pos[1], centre_pos[2] };
  Normalize3(c);
  double up[3] = { 0.0, 0.0, 1.0 };
  if (std::fabs(c[2]) > 0.999) {
    // The prototype's fallback: a centre at the zenith (or nadir) has no zenith projection.
    up[0] = -1.0;
    up[1] = 0.0;
    up[2] = 0.0;
  } else {
    up[0] = -c[0] * c[2];
    up[1] = -c[1] * c[2];
    up[2] = 1.0 - c[2] * c[2];
    Normalize3(up);
  }
  double right[3];
  Cross3(c, up, right);
  for (int k = 0; k < 3; ++k) {
    p.centre[k] = c[k];
    p.up[k] = up[k];
    p.right[k] = right[k];
  }
  p.radius_rad = std::max(1e-6, static_cast<double>(radius_deg) * kDeg2RadD);
  return p;
}

bool MapDiskProject(const MapDiskProjector& projector, const double pos[3], double* out_x, double* out_y) {
  const double cos_th = Clamp1(Dot3(projector.centre, pos));
  if (cos_th < std::cos(projector.radius_rad)) {
    return false;  // outside the view disk
  }
  const double th = std::acos(cos_th);
  const double rho = std::sin(0.5 * th) / std::sin(0.5 * projector.radius_rad);  // normalized [0,1]
  double t[3] = { pos[0] - projector.centre[0] * cos_th, pos[1] - projector.centre[1] * cos_th,
                  pos[2] - projector.centre[2] * cos_th };
  const double tl = Len3(t);
  if (tl < 1e-12) {
    *out_x = 0.5;
    *out_y = 0.5;
    return true;
  }
  for (int k = 0; k < 3; ++k) {
    t[k] /= tl;
  }
  *out_x = 0.5 + 0.5 * rho * Dot3(t, projector.right);
  *out_y = 0.5 - 0.5 * rho * Dot3(t, projector.up);
  return true;
}

bool MapDiskUnproject(const MapDiskProjector& projector, double x, double y, double out_pos[3]) {
  const double px = (x - 0.5) * 2.0;  // disk plane: right positive
  const double py = (0.5 - y) * 2.0;  // disk plane: up positive
  const double rho = std::hypot(px, py);
  if (!(rho <= 1.0 + 1e-9)) {
    return false;  // the bounding square's corners — no sky under them
  }
  const double th = 2.0 * std::asin(std::min(1.0, rho * std::sin(0.5 * projector.radius_rad)));
  if (rho < 1e-12) {
    for (int k = 0; k < 3; ++k) {
      out_pos[k] = projector.centre[k];
    }
    return true;
  }
  double t[3] = { projector.right[0] * px / rho + projector.up[0] * py / rho,
                  projector.right[1] * px / rho + projector.up[1] * py / rho,
                  projector.right[2] * px / rho + projector.up[2] * py / rho };
  const double cos_th = std::cos(th);
  const double sin_th = std::sin(th);
  for (int k = 0; k < 3; ++k) {
    out_pos[k] = projector.centre[k] * cos_th + t[k] * sin_th;
  }
  return true;
}

bool MapPickPositionAtDiskCoords(const MapViewState& view, double x, double y, double out_pos[3]) {
  return MapDiskUnproject(MakeMapDiskProjector(view.centre_pos, view.radius_deg), x, y, out_pos);
}

void MapPanViewByDiskCoords(MapViewState* view, double dx, double dy) {
  // The prototype's drag rule: the sky point under the grab follows the cursor, so the new
  // centre is what was under (centre − delta). A miss (outside the disk) writes nothing.
  const MapDiskProjector proj = MakeMapDiskProjector(view->centre_pos, view->radius_deg);
  double pos[3];
  if (MapDiskUnproject(proj, 0.5 - dx, 0.5 - dy, pos)) {
    for (int k = 0; k < 3; ++k) {
      view->centre_pos[k] = static_cast<float>(pos[k]);
    }
  }
}

void MapZoomViewAtPoint(MapViewState* view, const double cursor_pos[3], float factor) {
  const double centre_d[3] = { view->centre_pos[0], view->centre_pos[1], view->centre_pos[2] };
  const double old_rad = static_cast<double>(view->radius_deg) * kDeg2RadD;
  const float new_rad_deg = std::clamp(view->radius_deg * factor, kMapMinRadiusDeg, kMapFullSkyRadiusDeg);
  view->radius_deg = new_rad_deg;
  const double new_rad = static_cast<double>(new_rad_deg) * kDeg2RadD;
  // The prototype's anchored zoom: slerp the centre toward the cursor's sky point by the
  // fraction that keeps that point's screen offset under the new radius.
  const double cos_th = Clamp1(Dot3(centre_d, cursor_pos));
  const double th = std::acos(cos_th);
  if (th < 1e-9) {
    return;  // the cursor sits at the centre: only the radius moved
  }
  const double rotate = th * (1.0 - new_rad / old_rad);
  double moved[3];
  RotateToward(centre_d, cursor_pos, rotate, moved);
  for (int k = 0; k < 3; ++k) {
    view->centre_pos[k] = static_cast<float>(moved[k]);
  }
}

void MapViewChanged(MapViewState* view, double now_s, bool gesture_active, bool user_steered) {
  view->last_view_change_s = now_s;
  view->gesture_active = gesture_active;
  if (user_steered) {
    view->user_moved = true;
  }
}

// ---- View -> request derivation --------------------------------------------------------------------

MapWindowSpec MapWindowSpecForView(const MapViewState& view, double sun_altitude_deg, double sun_azimuth_deg) {
  MapWindowSpec spec;
  if (view.radius_deg >= kMapFullSkyRadiusDeg - 0.01f) {
    return spec;  // full_sky true, grid zero — the engine's named all-sky view
  }
  spec.full_sky = false;
  const double c[3] = { view.centre_pos[0], view.centre_pos[1], view.centre_pos[2] };
  spec.centre_altitude_deg = std::asin(Clamp1(c[2])) * kRad2DegD;
  spec.centre_azimuth_deg = std::atan2(c[1], c[0]) * kRad2DegD;

  double sun_pos[3];
  SkyPosition(sun_altitude_deg, sun_azimuth_deg, sun_pos);
  const double rad = static_cast<double>(view.radius_deg) * kDeg2RadD;
  const double delta_c = std::acos(Clamp1(Dot3(c, sun_pos)));
  spec.delta_span_deg = 2.0 * static_cast<double>(view.radius_deg);
  // The disk's azimuth extent about the sun: the tangent-cone bound, full circle when the disk
  // reaches a pole of the sun frame (the azimuth coordinate degenerates there).
  if (delta_c - rad <= 0.0 || delta_c + rad >= kPiD) {
    spec.azimuth_span_deg = 360.0;
  } else {
    spec.azimuth_span_deg = 2.0 * std::asin(std::min(1.0, std::sin(rad) / std::sin(delta_c))) * kRad2DegD;
  }
  // The grid: kMapTargetCellsAcross across the delta span. Off the poles the azimuth columns
  // match the sky spacing at the centre's deviation (an az degree near the sun is a small sky
  // distance); a disk that contains a pole of the sun frame has no such reference deviation —
  // its azimuths span the full circle about the degenerate point — so its columns size like the
  // full sky's, one azimuth cell per delta cell (the quasi-uniform grid the all-sky default
  // takes).
  const double delta_cell_rad = 2.0 * rad / static_cast<double>(kMapTargetCellsAcross);
  int rows = std::max(1, static_cast<int>(std::ceil(2.0 * rad / delta_cell_rad)));
  const bool pole_inside = delta_c - rad <= 0.0 || delta_c + rad >= kPiD;
  double az_cell_sky_rad = delta_cell_rad;
  if (!pole_inside && std::sin(delta_c) > 1e-6) {
    az_cell_sky_rad = delta_cell_rad / std::sin(delta_c);
  }
  int cols = static_cast<int>(std::ceil(spec.azimuth_span_deg * kDeg2RadD / az_cell_sky_rad));
  cols = std::max(cols, 8);
  rows = std::max(rows, 1);
  if (static_cast<long long>(rows) * static_cast<long long>(cols) > LUMICE_PATH_MAP_MAX_PIXELS) {
    const double scale = std::sqrt(static_cast<double>(rows) * static_cast<double>(cols) /
                                   static_cast<double>(LUMICE_PATH_MAP_MAX_PIXELS));
    rows = std::max(1, static_cast<int>(std::floor(rows / scale)));
    cols = std::max(1, static_cast<int>(std::floor(cols / scale)));
  }
  spec.rows = rows;
  spec.cols = cols;
  return spec;
}

bool SkySunAnchorFor(const GuiState& state, SkySunAnchor* out) {
  // The adopted map's own echo is the best anchor (the picture's sun). Then the adopted
  // survey's scope (the digest's sun). The live document last — an edited scene's sun may
  // differ from what any adopted document described.
  if (state.analysis.map.adopted_result != nullptr) {
    LUMICE_PathMapInfo info{};
    info.struct_size = sizeof(info);
    if (LUMICE_PathMapGetInfo(state.analysis.map.adopted_result.get(), &info) == LUMICE_PATH_READ_OK) {
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
  // request against ITS sun, never the live document's (an edited-but-not-re-analyzed sun must
  // not steer the window geometry). Same source as the binding, captured at DoAnalyze.
  out->altitude_deg = state.analysis.frozen_sun_valid ? state.analysis.frozen_sun_altitude : state.sun.altitude;
  out->azimuth_deg = 0.0;  // the GUI's sun is always due north
  return true;
}

LUMICE_PathMapRequest MakeMapRequestForKey(const MapIntentKey& key, const SkySunAnchor& sun, std::vector<int>* faces,
                                           std::vector<int>* counts, std::vector<double>* wavelengths) {
  *faces = key.selection.faces;
  *counts = key.selection.layer_face_counts;
  MapViewState view;
  for (int k = 0; k < 3; ++k) {
    view.centre_pos[k] = key.centre_pos[k];
  }
  view.radius_deg = key.radius_deg;
  const MapWindowSpec spec = MapWindowSpecForView(view, sun.altitude_deg, sun.azimuth_deg);
  LUMICE_PathMapRequest request{};
  request.struct_size = sizeof(request);
  request.crystal_id = key.selection.crystal_id;
  request.faces = faces->data();
  request.face_count = static_cast<int>(faces->size());
  request.layer_face_counts = counts->data();
  request.layer_count = static_cast<int>(counts->size());
  request.full_sky = spec.full_sky ? 1 : 0;
  request.centre_altitude_deg = spec.centre_altitude_deg;
  request.centre_azimuth_deg = spec.centre_azimuth_deg;
  request.delta_span_deg = spec.delta_span_deg;
  request.azimuth_span_deg = spec.azimuth_span_deg;
  request.view_rows = spec.rows;
  request.view_cols = spec.cols;
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

MapFitResult FitMapViewFromPixels(const LUMICE_PathMapPixels& pixels, const LUMICE_PathMapInfo& info) {
  MapFitResult fit;
  double cx = 0.0;
  double cy = 0.0;
  double cz = 0.0;
  double total = 0.0;
  std::vector<double> weights;
  std::vector<double> distances;  // angular distance from the centroid, same order (filled later)
  weights.reserve(pixels.pixel_count);
  for (size_t i = 0; i < pixels.pixel_count; ++i) {
    const uint8_t status = pixels.status[i];
    if (status != LUMICE_PATH_MAP_PIXEL_OK && status != LUMICE_PATH_MAP_PIXEL_POINT_MASS) {
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
  }
  if (total <= 0.0) {
    return fit;  // ok=false: an honest all-zero map asks for no Fit
  }
  double centre[3] = { cx / total, cy / total, cz / total };
  Normalize3(centre);
  weights.clear();
  for (size_t i = 0; i < pixels.pixel_count; ++i) {
    const uint8_t status = pixels.status[i];
    if (status != LUMICE_PATH_MAP_PIXEL_OK && status != LUMICE_PATH_MAP_PIXEL_POINT_MASS) {
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

MapDisplayNorm MapDisplayNormFor(const LUMICE_PathMapPixels& pixels) {
  MapDisplayNorm norm;
  std::vector<double> positive;
  positive.reserve(pixels.pixel_count);
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

uint32_t MapCellColorRgba(double value, uint8_t status, const MapDisplayNorm& norm) {
  if (status == LUMICE_PATH_MAP_PIXEL_SINGULAR) {
    return 0u;  // NaN honesty: no colour where nothing was computed
  }
  if (status == LUMICE_PATH_MAP_PIXEL_POINT_MASS) {
    return IM_COL32(250, 184, 74, 255);  // the mass arm's fixed marker colour
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
  return IM_COL32(static_cast<int>(r * 255.0), static_cast<int>(g * 255.0), static_cast<int>(b * 255.0), 255);
}

// ---- The pump --------------------------------------------------------------------------------------

namespace {

// The default view chain's starting view (zenith-centred full sky): what a fresh selection's
// first request asks for, and what the view returns to when the selection changes.
void ResetMapViewForNewSelection(MapViewState* view) {
  view->centre_pos[0] = 0.0f;
  view->centre_pos[1] = 0.0f;
  view->centre_pos[2] = 1.0f;
  view->radius_deg = kMapFullSkyRadiusDeg;
  view->auto_fit_done = false;
  view->user_moved = false;
}

// The issuing half of the pump (the survey pump's IssueSurveyQuery shape): build the request for
// `key` and hand it to the executor with the id the pump already assigned. Buffers move into the
// executor's job.
void IssueMapIntent(GuiState& state, SurveyExecutor& executor, const SurveyQueryId& id, const MapIntentKey& key) {
  SkySunAnchor sun;
  SkySunAnchorFor(state, &sun);
  std::vector<int> faces;
  std::vector<int> counts;
  std::vector<double> wavelengths;
  const LUMICE_PathMapRequest request = MakeMapRequestForKey(key, sun, &faces, &counts, &wavelengths);
  executor.IssueMapQuery(id, request, std::move(faces), std::move(counts), std::move(wavelengths));
}

}  // namespace

void PumpMapQuery(GuiState& state, SurveyExecutor& executor, double now_s) {
  MapQuerySession& map = state.analysis.map;
  MapViewState& view = state.analysis.map_view;

  // 1. Drain: adopt the finished map iff it still answers the CURRENT intent and the intent
  //    stands (the survey gate's own rule — a cancelled intent accepts no outcome, and the
  //    engine answers a cancel with a truncated-but-OK map, exactly the document the user
  //    withdrew). A refusal is an answer for a standing intent.
  while (std::optional<SurveyExecutor::Finished> finished = executor.TakeFinished(QueryFace::kMap)) {
    if (map.intent_valid && !map.cancelled_for_key && finished->id == map.intent_id) {
      map.adopted_result = finished->ok ? finished->map_result : nullptr;
      map.adopted_error = finished->ok ? std::string() : std::move(finished->error);
      map.adopted_id = finished->id;
    }
  }

  // 2. The default view chain's auto-Fit: the first adopted map of this selection fits the
  //    picture once (the fitted view then asks its refinement through the settle gate below).
  //    Latched per selection — a failed Fit (an honest all-zero map) must not loop — and it
  //    stands down the moment the user has steered the view by hand: the chain serves a passive
  //    selection, never a yank of navigation the user has already begun.
  if (!view.auto_fit_done && !view.user_moved && map.adopted_result != nullptr) {
    view.auto_fit_done = true;
    LUMICE_PathMapInfo info{};
    info.struct_size = sizeof(info);
    LUMICE_PathMapPixels pixels{};
    pixels.struct_size = sizeof(pixels);
    if (LUMICE_PathMapGetInfo(map.adopted_result.get(), &info) == LUMICE_PATH_READ_OK &&
        LUMICE_PathMapGetPixels(map.adopted_result.get(), &pixels) == LUMICE_PATH_READ_OK) {
      const MapFitResult fit = FitMapViewFromPixels(pixels, info);
      if (fit.ok) {
        for (int k = 0; k < 3; ++k) {
          view.centre_pos[k] = fit.centre_pos[k];
        }
        view.radius_deg = fit.radius_deg;
        MapViewChanged(&view, now_s, false, /*user_steered=*/false);
        GUI_LOG_VERBOSE("[MapView] auto-Fit: radius {:.1f} deg", view.radius_deg);
      }
    }
  }

  // 3. The live selection through the ONE identity chain (a56 — the same derivation the survey
  //    pump consumes; map and survey cannot disagree about what class is on show).
  const std::optional<SelectedClassIdentity> identity = SelectedClassIdentityFor(state);
  if (!identity.has_value() || !state.analysis_result.payload) {
    if (map.intent_valid) {
      // The selection died: the intent dies with it, adopted outcome included — another
      // class's map must not linger under a new selection.
      executor.CancelInFlight(QueryFace::kMap);
      map = MapQuerySession{};
    }
    ResetMapViewForNewSelection(&view);
    return;
  }
  const SurveySelectionKey selection =
      SurveySelectionKeyFor(*identity, state.analysis_result.payload->snapshot_generation);

  // 4. A selection change (identity or analysis generation) kills the whole map session —
  //    picture included — and the view returns to the default chain's full sky: the default
  //    view chain is per selection, so the next class's first picture is its own whole-sky
  //    view, not the previous class's framing (whose auto-Fit would be a window map fitting
  //    itself). A view/wavelength change within one selection only supersedes the intent.
  if (map.intent_valid && !(map.key.selection == selection)) {
    executor.CancelInFlight(QueryFace::kMap);
    map = MapQuerySession{};
    ResetMapViewForNewSelection(&view);
  }

  MapIntentKey key;
  key.selection = selection;
  for (int k = 0; k < 3; ++k) {
    key.centre_pos[k] = view.centre_pos[k];
  }
  key.radius_deg = view.radius_deg;
  key.wavelength_nm = state.analysis.map_wavelength_nm;

  // 5. Issue or supersede — never mid-gesture or before the settle window (a wheel burst is one
  //    request, not one per notch).
  if (!map.intent_valid) {
    if (!MapViewSettled(now_s, view)) {
      return;
    }
    map = MapQuerySession{};
    map.intent_valid = true;
    map.key = key;
    map.intent_id = executor.NextId(selection.generation);
    map.intent_start_s = now_s;  // the wait display's epoch — this frame (plan §3.1)
    IssueMapIntent(state, executor, map.intent_id, key);
    return;
  }
  if (map.key != key) {
    if (!MapViewSettled(now_s, view)) {
      return;
    }
    executor.CancelInFlight(QueryFace::kMap);  // the map face's own supersession
    // The old PICTURE stays on show (MapRefiningFor names it); the old REFUSAL goes — it
    // answered a view that no longer stands.
    map.adopted_error.clear();
    map.cancelled_for_key = false;
    map.key = key;
    map.intent_id = executor.NextId(selection.generation);
    map.intent_start_s = now_s;  // a rekey restarts the wait clock (plan §3.1)
    IssueMapIntent(state, executor, map.intent_id, key);
  }
}

// ---- The raster ------------------------------------------------------------------------------------

namespace {

constexpr int kMapRasterMaxSize = 512;

// The region's cached picture: the raster of the adopted map under the view it was drawn for.
// Derived display state (never session semantics): invalidated by the map pointer or the view,
// regenerated at most once per frame. The GL texture is created once and re-uploaded in place.
struct MapRasterCache {
  // The rastered result's identity — the session's own adopted_id (the executor's serial is
  // monotonic and never reused), never the result's heap address: a replaced or reset session
  // frees the old map, and a same-sized new allocation can land on the same address, which
  // would pass an address key and show the previous class's raster under the new class's
  // caliber.
  SurveyQueryId adopted_id{};
  bool has_map = false;
  float centre[3] = { 0.0f, 0.0f, 0.0f };
  float radius_deg = 0.0f;
  int size = 0;
  bool valid = false;
  MapDisplayNorm norm;
  unsigned int texture = 0;
  int texture_size = 0;  // the allocation the GL object currently holds — the realloc's own record
  std::vector<uint8_t> rgba;
};

MapRasterCache g_map_raster;

void DestroyMapRasterTexture() {
  if (g_map_raster.texture != 0) {
    glDeleteTextures(1, &g_map_raster.texture);
    g_map_raster.texture = 0;
  }
  g_map_raster = MapRasterCache{};
}

void RasterizeMapDisk(const LUMICE_PathMap& map, const SurveyQueryId& adopted_id, const MapViewState& view,
                      int target_size) {
  LUMICE_PathMapInfo info{};
  info.struct_size = sizeof(info);
  LUMICE_PathMapPixels pixels{};
  pixels.struct_size = sizeof(pixels);
  if (LUMICE_PathMapGetInfo(&map, &info) != LUMICE_PATH_READ_OK ||
      LUMICE_PathMapGetPixels(&map, &pixels) != LUMICE_PATH_READ_OK) {
    return;  // the cache stays untouched — no "sized without a raster" half-state
  }
  const int size = std::clamp(target_size, 64, kMapRasterMaxSize);
  // The texture is (re)allocated at the raster's own size, not a fixed maximum: the quad draws
  // the texture's full [0,1] uv range, so a stale allocation would either clip the upload (an
  // out-of-bounds glTexSubImage2D is silently dropped — the picture would freeze on the old
  // texture) or show uninitialized storage beside the picture. The allocation size is the
  // cache's OWN record, compared against itself — never a field the caller pre-writes.
  if (g_map_raster.texture == 0 || g_map_raster.texture_size != size) {
    if (g_map_raster.texture == 0) {
      glGenTextures(1, &g_map_raster.texture);
    }
    glBindTexture(GL_TEXTURE_2D, g_map_raster.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    g_map_raster.texture_size = size;
  }
  g_map_raster.rgba.resize(static_cast<size_t>(size) * static_cast<size_t>(size) * 4u);
  const MapDiskProjector proj = MakeMapDiskProjector(view.centre_pos, view.radius_deg);
  if (!g_map_raster.norm.valid || !(g_map_raster.adopted_id == adopted_id)) {
    g_map_raster.norm = MapDisplayNormFor(pixels);
  }
  const double delta_min = info.view_delta_min_deg;
  const double delta_max = info.view_delta_max_deg;
  const double az_min = info.view_az_min_deg;
  const double az_max = info.view_az_max_deg;
  const double delta_cell = (delta_max - delta_min) / static_cast<double>(info.view_rows);
  const double az_cell = (az_max - az_min) / static_cast<double>(info.view_cols);
  uint8_t* out = g_map_raster.rgba.data();
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      double pos[3];
      uint32_t colour = 0;
      if (MapDiskUnproject(proj, (x + 0.5) / size, (y + 0.5) / size, pos)) {
        // The pixel's travel direction (the negated position), read through the result's own
        // frame — the map grid's one convention.
        const double d[3] = { -pos[0], -pos[1], -pos[2] };
        const double cos_delta = Clamp1(Dot3(d, info.view_incident));
        const double delta = std::acos(cos_delta) * kRad2DegD;
        double v[3] = { d[0] - cos_delta * info.view_incident[0], d[1] - cos_delta * info.view_incident[1],
                        d[2] - cos_delta * info.view_incident[2] };
        const double e1 = Dot3(v, info.view_e1);
        const double e2 = Dot3(v, info.view_e2);
        const double az = Wrap360(std::atan2(e2, e1) * kRad2DegD);
        if (delta >= delta_min - 1e-9 && delta <= delta_max + 1e-9) {
          // The azimuth wrapped into the window's own linear range — the bounds may start
          // anywhere (a 360-degree window centred on the pole spells azimuths about its own
          // at-point azimuth, not about 0), so the pixel's wrapped azimuth is re-anchored to
          // the window's az_min before the column lookup.
          const double az_lin = az_min + Wrap360(az - az_min);
          if (az_lin <= az_max + 1e-9) {
            const int row = std::clamp(static_cast<int>((delta - delta_min) / delta_cell), 0, info.view_rows - 1);
            const int col = std::clamp(static_cast<int>((az_lin - az_min) / az_cell), 0, info.view_cols - 1);
            const size_t idx =
                static_cast<size_t>(row) * static_cast<size_t>(info.view_cols) + static_cast<size_t>(col);
            colour = MapCellColorRgba(pixels.values[idx], pixels.status[idx], g_map_raster.norm);
          }
        }
      }
      const size_t o = (static_cast<size_t>(y) * static_cast<size_t>(size) + static_cast<size_t>(x)) * 4u;
      out[o + 0] = static_cast<uint8_t>((colour >> 0) & 0xFF);
      out[o + 1] = static_cast<uint8_t>((colour >> 8) & 0xFF);
      out[o + 2] = static_cast<uint8_t>((colour >> 16) & 0xFF);
      out[o + 3] = static_cast<uint8_t>((colour >> 24) & 0xFF);
    }
  }
  glBindTexture(GL_TEXTURE_2D, g_map_raster.texture);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size, size, GL_RGBA, GL_UNSIGNED_BYTE, out);
  glBindTexture(GL_TEXTURE_2D, 0);
  g_map_raster.adopted_id = adopted_id;
  g_map_raster.has_map = true;
  g_map_raster.centre[0] = view.centre_pos[0];
  g_map_raster.centre[1] = view.centre_pos[1];
  g_map_raster.centre[2] = view.centre_pos[2];
  g_map_raster.radius_deg = view.radius_deg;
  g_map_raster.size = size;
  g_map_raster.valid = true;
}

bool MapRasterFresh(const SurveyQueryId& adopted_id, const MapViewState& view, int target_size) {
  return g_map_raster.valid && g_map_raster.has_map && g_map_raster.adopted_id == adopted_id &&
         g_map_raster.size == target_size && g_map_raster.radius_deg == view.radius_deg &&
         g_map_raster.centre[0] == view.centre_pos[0] && g_map_raster.centre[1] == view.centre_pos[1] &&
         g_map_raster.centre[2] == view.centre_pos[2];
}

}  // namespace

void ShutdownMapView() {
  DestroyMapRasterTexture();
}

// ---- The renderer -----------------------------------------------------------------------------------

namespace {

constexpr int kOverlaySegments = 96;

// A screen point on the picture for a sky position, or nullopt outside the view disk.
bool ProjectToRect(const MapViewState& view, const double pos[3], const ImVec2& pic_min, float pic_px, double* out_x,
                   double* out_y) {
  const MapDiskProjector proj = MakeMapDiskProjector(view.centre_pos, view.radius_deg);
  double x = 0.0;
  double y = 0.0;
  if (!MapDiskProject(proj, pos, &x, &y)) {
    return false;
  }
  *out_x = pic_min.x + x * pic_px;
  *out_y = pic_min.y + y * pic_px;
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
IsoDeltaWalk WalkIsoDeltaCircle(ImDrawList* dl, const MapViewState& view, const ImVec2& pic_min, float pic_px,
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

void DrawFrameAnchors(ImDrawList* dl, const MapViewState& view, const LUMICE_PathMapInfo& info, const ImVec2& pic_min,
                      float pic_px, OverlayLegendState* legend) {
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

void DrawSurveyOverlays(ImDrawList* dl, GuiState& state, const MapViewState& view, const ImVec2& pic_min, float pic_px,
                        OverlayLegendState* legend) {
  SurveyQuerySession& survey = state.analysis.survey;
  if (survey.adopted_result == nullptr) {
    return;
  }
  // The payloads read through the ONE document->facts mapping (§5.1.8: the overview model is the
  // single organizer; the map overlays consume its object rows and dispersion block verbatim).
  const SurveyOverviewModel model = BuildSurveyOverviewModel(*survey.adopted_result);
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

void DrawSkyPointMarker(ImDrawList* dl, const MapViewState& view, const SkyPointIntent& intent, const ImVec2& pic_min,
                        float pic_px, float* out_marker_px, float* out_marker_py) {
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
  MapViewState& view = state.analysis.map_view;
  SkyPointIntent& intent = state.analysis.sky_point;
  const double now_s = ImGui::GetTime();
  bool gesture_this_frame = false;

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

  // The map's square spends only its geometric share. All asynchronous state prose lives
  // AFTER it in a bounded report; the first waiting frame has the same graph as completion.
  const float canvas_avail_w = ImGui::GetContentRegionAvail().x;
  const float canvas_avail_h = ImGui::GetContentRegionAvail().y;
  const float pic_px = std::max(
      1.0f, std::min(canvas_avail_w,
                     CanvasHeightBeforeReport(canvas_avail_h, 0.0f, UiPx(64.0f), ImGui::GetStyle().ItemSpacing.y)));
  const ImVec2 pic_min = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton("##map_canvas", ImVec2(pic_px, pic_px));
  const bool canvas_hovered = ImGui::IsItemHovered();
  const bool canvas_active = ImGui::IsItemActive();
  ImDrawList* dl = ImGui::GetWindowDrawList();

  if (map.adopted_result != nullptr) {
    const int target_size = static_cast<int>(std::min(pic_px, static_cast<float>(kMapRasterMaxSize)));
    if (!MapRasterFresh(map.adopted_id, view, target_size)) {
      RasterizeMapDisk(*map.adopted_result, map.adopted_id, view, target_size);
    }
    if (g_map_raster.valid) {
      // The quad stretches to the canvas the gestures measure against — the texture's own
      // resolution is display detail, never a second geometry. NO uv flip: the raster is
      // CPU-authored top-down (buffer row 0 is the disk's top, the y convention the projection
      // unit tests pin), and glTexSubImage2D lands row 0 at v=0 — so quad top must sample v=0.
      // (The flip here was the thumbnail blit's convention, whose content is GPU-authored
      // bottom-up; a CPU buffer under it displays mirrored, invisible on the az-symmetric
      // fitted view but opposite to every anchor on a full-sky one.)
      dl->AddImage(static_cast<ImTextureID>(static_cast<uintptr_t>(g_map_raster.texture)), pic_min,
                   ImVec2(pic_min.x + pic_px, pic_min.y + pic_px), ImVec2(0, 0), ImVec2(1, 1));
    }
  }
  // The disk frame: the view's own boundary (the picture's clip circle), plus the honest
  // no-data state when nothing has arrived yet.
  dl->AddCircle(ImVec2(pic_min.x + pic_px * 0.5f, pic_min.y + pic_px * 0.5f), pic_px * 0.5f,
                ImGui::GetColorU32(ImGuiCol_Separator), 0, 1.0f);

  // The frame anchors (sun / zenith / horizon) need the adopted map's own sun echo.
  LUMICE_PathMapInfo anchor_info{};
  anchor_info.struct_size = sizeof(anchor_info);
  const bool have_anchor_info = map.adopted_result != nullptr &&
                                LUMICE_PathMapGetInfo(map.adopted_result.get(), &anchor_info) == LUMICE_PATH_READ_OK;
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

  // ---- The gestures (the prototype's bindRp semantics; the ROI's state is never touched here) ----
  // The wheel's arbitration lives in the BeginChild comment above: this branch is the canvas
  // side (zoom, exclusive), the else-branch below is the region side (scroll, exclusive) —
  // and both consume only while a claim was standing at the frame's start, so the takeover
  // frame's event (already spent by the band's native scroll) is not spent twice.
  if (canvas_hovered && !view.point_dragging) {
    const ImGuiIO& io = ImGui::GetIO();
    if (wheel_standing && io.MouseWheel != 0.0f) {
      const double cx = (io.MousePos.x - pic_min.x) / pic_px;
      const double cy = (io.MousePos.y - pic_min.y) / pic_px;
      double pos[3];
      if (MapPickPositionAtDiskCoords(view, cx, cy, pos)) {
        const float factor = io.MouseWheel > 0.0f ? 1.0f / kMapZoomWheelFactor : kMapZoomWheelFactor;
        MapZoomViewAtPoint(&view, pos, factor);
        MapViewChanged(&view, now_s, true, /*user_steered=*/true);
        gesture_this_frame = true;
      }
    }
  } else if (wheel_standing && ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel != 0.0f) {
    // The region's own scroll, hand-routed (the window no longer takes the wheel natively):
    // the wheel outside the picture moves content and never the view. The step is the native
    // wheel scroll's own formula (five lines, capped at two thirds of the view), so the feel
    // matches every pane ImGui scrolls for itself.
    const float max_step = ImGui::GetWindowSize().y * 0.67f;
    const float step = std::floor(std::min(5.0f * ImGui::GetFontSize(), max_step));
    ImGui::SetScrollY(ImGui::GetScrollY() - ImGui::GetIO().MouseWheel * step);
  }
  if (canvas_active) {
    const ImGuiIO& io = ImGui::GetIO();
    // Press-time arbitration: the sky-point marker under the press grabs the drag; otherwise
    // the view pans. A press that never moves is a pick, decided at release.
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      float px = 0.0f;
      float py = 0.0f;
      if (marker_px >= 0.0f) {
        const float dx = io.MousePos.x - marker_px;
        const float dy = io.MousePos.y - marker_py;
        const float hit = UiPx(kAnalysisConeMarkerHitRadiusPt);
        view.point_dragging = dx * dx + dy * dy <= hit * hit;
      }
    }
    if (view.point_dragging) {
      const double cx = (io.MousePos.x - pic_min.x) / pic_px;
      const double cy = (io.MousePos.y - pic_min.y) / pic_px;
      double pos[3];
      if (MapPickPositionAtDiskCoords(view, cx, cy, pos)) {
        // The intent's travel direction is the negated position — the engine's at-point
        // convention (§12), the same units a point query fills.
        intent.valid = true;
        intent.from_map_pick = true;
        intent.dir[0] = static_cast<float>(-pos[0]);
        intent.dir[1] = static_cast<float>(-pos[1]);
        intent.dir[2] = static_cast<float>(-pos[2]);
      }
    } else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
      MapPanViewByDiskCoords(&view, static_cast<double>(io.MouseDelta.x) / pic_px,
                             static_cast<double>(io.MouseDelta.y) / pic_px);
      MapViewChanged(&view, now_s, true, /*user_steered=*/true);
      gesture_this_frame = true;
    }
  } else {
    // The release frame: end a marker drag (a marker press releases as a drag-end, never a
    // pick), and treat a still press on the bare canvas as a pick.
    const bool was_point_dragging = view.point_dragging;
    if (view.point_dragging) {
      view.point_dragging = false;
    }
    if (ImGui::IsItemDeactivated()) {
      const ImGuiIO& io = ImGui::GetIO();
      const ImVec2 drag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
      if (!was_point_dragging && drag.x * drag.x + drag.y * drag.y < 3.0f * 3.0f) {
        const double cx = (io.MousePos.x - pic_min.x) / pic_px;
        const double cy = (io.MousePos.y - pic_min.y) / pic_px;
        double pos[3];
        if (MapPickPositionAtDiskCoords(view, cx, cy, pos)) {
          intent.valid = true;
          intent.from_map_pick = true;
          intent.dir[0] = static_cast<float>(-pos[0]);
          intent.dir[1] = static_cast<float>(-pos[1]);
          intent.dir[2] = static_cast<float>(-pos[2]);
        }
      }
    }
  }
  // Esc disarms nothing here (the map has no armed pick), but a stray drag latch must not
  // outlive the press.
  view.gesture_active = gesture_this_frame || canvas_active;

  // The cursor readout: the sky label and its deviation from the sun, through the SAME
  // projection the picture was drawn with. It overlays the PICTURE (a backdrop plate on the
  // canvas's top-left) instead of occupying flow: rendered as flow it appeared only under the
  // pointer, and its appearing reflowed every control below it — the tail's buttons moved
  // under a moving mouse, and a click traveling toward them landed on a control that had
  // stepped aside (the map_region leg's Clear died mid-hover exactly so). Canvas feedback
  // belongs on the canvas.
  if (canvas_hovered && have_anchor_info) {
    const ImGuiIO& io = ImGui::GetIO();
    const double cx = (io.MousePos.x - pic_min.x) / pic_px;
    const double cy = (io.MousePos.y - pic_min.y) / pic_px;
    double pos[3];
    if (MapPickPositionAtDiskCoords(view, cx, cy, pos)) {
      const float alt = std::asin(std::clamp(pos[2], -1.0, 1.0)) * static_cast<float>(kRad2DegD);
      const float az = std::atan2(pos[1], pos[0]) * static_cast<float>(kRad2DegD);
      double sun_pos[3];
      SkyPosition(anchor_info.sun_altitude_deg, anchor_info.sun_azimuth_deg, sun_pos);
      const double delta =
          std::acos(Clamp1(pos[0] * sun_pos[0] + pos[1] * sun_pos[1] + pos[2] * sun_pos[2])) * kRad2DegD;
      char readout_a[64];
      char readout_b[64];
      std::snprintf(readout_a, sizeof(readout_a), "cursor: alt %.2f° az %.2f°", alt, az);
      std::snprintf(readout_b, sizeof(readout_b), "δ %.2f° from the sun", delta);
      ImDrawList* draw = ImGui::GetWindowDrawList();
      const float plate_pad = 5.0f;
      const float wrap_w = pic_px - 2.0f * plate_pad;
      const float line_h = ImGui::GetTextLineHeight();
      draw->AddRectFilled(ImVec2(pic_min.x, pic_min.y),
                          ImVec2(pic_min.x + pic_px, pic_min.y + 2.0f * plate_pad + 2.0f * line_h),
                          IM_COL32(16, 18, 22, 190));
      draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(pic_min.x + plate_pad, pic_min.y + plate_pad),
                    ImGui::GetColorU32(ImGuiCol_Text), readout_a, nullptr, wrap_w);
      draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(pic_min.x + plate_pad, pic_min.y + plate_pad + line_h), ImGui::GetColorU32(ImGuiCol_Text),
                    readout_b, nullptr, wrap_w);
    }
  }

  ImGui::BeginChild("##MapState", RemainingReportSize());
  // The state line (the four visible states + the two supersession notes). It reads the
  // picture's own query state, so it stays with the picture; the controls it names — the
  // wavelength, the view buttons, the sky-point input — live in the info region beside it.
  const MapQueryState st = MapQueryStateFor(map);
  const bool refining = MapRefiningFor(map);
  if (st == MapQueryState::kNoSelection) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped(
        "Select a class in the share list; its reference map takes this region. The sky-point input in the "
        "info panel beside it works without a map.");
    ImGui::PopStyleColor();
  } else if (st == MapQueryState::kCancelled) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped("Map cancelled for this selection (scene restored). Panning or zooming asks anew.");
    ImGui::PopStyleColor();
  } else if (st == MapQueryState::kRefused && !refining) {
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
    const std::string wait_line =
        MapWaitLineFor(map, g_survey_executor.QueryOutstanding(QueryFace::kSurvey), view.user_moved, ImGui::GetTime(),
                       g_survey_executor.InflightPollSnapshot());
    state.analysis.wait_feedback_texts.map_wait_line = wait_line;
    ImGui::TextWrapped("%s", wait_line.c_str());
    ImGui::PopStyleColor();
  } else if (refining) {
    // The refining note carries its own elapsed — the refinement intent's wait, same epoch the
    // pump stamps on every re-ask — and the poll's stage clause while the refinement's map
    // query itself is computing (751; face-matched, so a queued survey's stage never lands here).
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    const std::string note = MapRefiningNoteFor(map, ImGui::GetTime(), g_survey_executor.InflightPollSnapshot());
    state.analysis.wait_feedback_texts.map_refining_note = note;
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
  // The wavelength: an explicit value in [350, 900], or the engine's reference (0). The scratch
  // float shows the effective value either way; editing it asks the explicit wavelength — a key
  // change the pump turns into a new request (the engine has no warm map: every lambda is a
  // real re-evaluation).
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
  const bool lambda_same_line = false;
  // The wrapped case re-spends the FRESH line's whole width: the track shrinks so slider plus
  // its trailing label stay inside the region. The 60 pt floor keeps a usable track at every
  // width the info floor can produce; this child scrolls, so even the wrapped fallback stays
  // reachable.
  float lambda_track = UiPx(140.0f);
  if (!lambda_same_line) {
    lambda_track = std::min(lambda_track, ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("λ").x -
                                              ImGui::GetStyle().ItemInnerSpacing.x);
    lambda_track = std::max(lambda_track, UiPx(60.0f));
  }
  ImGui::SetNextItemWidth(lambda_track);
  if (ImGui::SliderFloat("λ##map", &lambda_scratch, 350.0f, 900.0f, "%.0f nm", ImGuiSliderFlags_AlwaysClamp)) {
    state.analysis.map_wavelength_nm = static_cast<double>(lambda_scratch);
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
  if (ImGui::SmallButton("Fit##map")) {
    if (map.adopted_result != nullptr) {
      LUMICE_PathMapInfo info{};
      info.struct_size = sizeof(info);
      LUMICE_PathMapPixels pixels{};
      pixels.struct_size = sizeof(pixels);
      if (LUMICE_PathMapGetInfo(map.adopted_result.get(), &info) == LUMICE_PATH_READ_OK &&
          LUMICE_PathMapGetPixels(map.adopted_result.get(), &pixels) == LUMICE_PATH_READ_OK) {
        const MapFitResult fit = FitMapViewFromPixels(pixels, info);
        if (fit.ok) {
          for (int k = 0; k < 3; ++k) {
            view.centre_pos[k] = fit.centre_pos[k];
          }
          view.radius_deg = fit.radius_deg;
          MapViewChanged(&view, now_s, false, /*user_steered=*/true);
        }
      }
    }
  }
  if (fits_on_line(full_sky_w)) {
    ImGui::SameLine();
  }
  if (ImGui::SmallButton("Full sky##map")) {
    view.centre_pos[0] = 0.0f;
    view.centre_pos[1] = 0.0f;
    view.centre_pos[2] = 1.0f;
    view.radius_deg = kMapFullSkyRadiusDeg;
    MapViewChanged(&view, now_s, false, /*user_steered=*/true);
  }

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
  char status[128];
  if (intent.valid) {
    float alt = 0.0f;
    float az = 0.0f;
    DirToAltAz(intent.dir, &alt, &az);
    std::snprintf(status, sizeof(status), "sky point: alt %.2f° az %.2f° (%s)", alt, az,
                  intent.from_map_pick ? "picked on the map" : "typed coordinates");
  } else {
    std::snprintf(status, sizeof(status), "no sky point set");
  }
  if (fits_on_line(ImGui::CalcTextSize("Clear").x + style_fp_x * 2.0f)) {
    ImGui::SameLine();
  }
  ImGui::BeginDisabled(!intent.valid);
  if (ImGui::SmallButton("Clear##sky")) {
    intent = SkyPointIntent{};
  }
  ImGui::EndDisabled();

  ImGui::BeginChild("##MapReport", RemainingReportSize());
  // The identity line (contract §6's status summary): the selected class named WHERE it is read.
  // The list that carries the same label can be off-screen while this window is open (Render
  // workspace, an occluding float), so the window re-states the list's own text — the one chain
  // (SelectedClassRowLabelFor, the same AnalysisRowLabel spelling the list shows), not a second
  // rendering of the selection and not a copy of the overview's report (§2.2: the survey facts
  // stay in the main window's diagnostics). Wrapped like every legend line below: a long chain
  // label folds, it does not clip. The Separator ends the line so the λ row's SameLine judgment
  // below reads a fresh line — fits_on_line judges against the LAST item's rect, and the
  // judgment it must not inherit is whatever the map region submitted before this child.
  const std::string identity_label = SelectedClassRowLabelFor(state);
  if (identity_label.empty()) {
    ImGui::TextDisabled("(no class selected)");
  } else {
    ImGui::TextWrapped("%s", identity_label.c_str());
  }
  ImGui::Separator();
  ImGui::TextWrapped("%s", status);
  if (state.analysis.map_wavelength_nm <= 0.0 && map.adopted_result == nullptr) {
    ImGui::TextWrapped("The engine-chosen reference wavelength is used until a map echoes it.");
  }
  // The caliber block: every fact read back through the result's own readers into the pure
  // composer (the unit layer holds it to the readback), then drawn (AC1).
  if (map.adopted_result != nullptr && have_anchor_info) {
    MapCaliberInput in;
    in.info = anchor_info;
    LUMICE_PathSurveyEvidence evidence{};
    evidence.struct_size = sizeof(evidence);
    in.have_evidence = LUMICE_PathMapGetEvidence(map.adopted_result.get(), &evidence) == LUMICE_PATH_READ_OK;
    in.evidence = evidence;
    LUMICE_PathQuantitySpec quantity{};
    quantity.struct_size = sizeof(quantity);
    in.have_quantity = LUMICE_PathMapGetQuantity(map.adopted_result.get(), &quantity) == LUMICE_PATH_READ_OK;
    in.quantity = quantity;
    if (anchor_info.point_mass_available != 0) {
      LUMICE_PathMapPointMass mass{};
      mass.struct_size = sizeof(mass);
      in.have_mass = LUMICE_PathMapGetPointMass(map.adopted_result.get(), &mass) == LUMICE_PATH_READ_OK;
      in.mass = mass;
      for (size_t i = 0; i < mass.method_count && i < kMapCaliberMaxMethods; ++i) {
        in.methods[i] = mass.methods[i];
      }
    }
    in.completed = MapQueryStateFor(map) == MapQueryState::kCompleted;
    // The report follows every operation. Its native scroll cannot overlap or reposition
    // the typed-coordinate controls, even while a long caliber or refusal arrives.
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    for (const std::string& line : MapCaliberLines(in)) {
      ImGui::TextWrapped("%s", line.c_str());
    }
    ImGui::PopStyleColor();
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
    // Every entry is the Bullet()+TextWrapped pair (the overview's rule: BulletText and
    // SameLine+Text size a single line and clip at the region's right edge — exactly where the
    // dispersion entry's honesty qualifier gets interesting at the info floor's 200 pt). The
    // wrap is the vertical scroll's own overflow channel: no horizontal scrollbar exists here.
    if (legend.anchor_sun) {
      ImGui::Bullet();
      ImGui::TextWrapped("sun marker — the delta axis's zero");
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
      // One wrapped item: the kind and its meaning fold together at the marker's indent — the
      // old SameLine+TextUnformatted pair pinned the explanation onto the kind's line, where the
      // info floor clipped its tail exactly when the words carry the shape's meaning.
      ImGui::TextWrapped("%s — the survey's located conclusion", kind.c_str());
    }
    if (legend.dispersion_red || legend.dispersion_blue) {
      ImGui::Bullet();
      if (legend.dispersion_red && legend.dispersion_blue) {
        ImGui::TextWrapped(
            "dispersion circles (red inside blue) — the measured minimum-deviation geometry at the two reference "
            "wavelengths; positions, not a whole-circle visibility or contribution claim");
      } else {
        // One circle's arc sits outside the view disk this frame: the entry names the one that
        // drew — the other half's absence is the honest state, not a dropped category.
        ImGui::TextWrapped(
            "%s dispersion circle — the measured minimum-deviation geometry at its reference wavelength; positions, "
            "not a whole-circle visibility or contribution claim",
            legend.dispersion_red ? "red" : "blue");
      }
      if (legend.dispersion_partial) {
        ImGui::TextWrapped("A circle partially outside the view draws only its in-view arc (\"arc in view\").");
      }
    }
    if (legend.sky_point) {
      ImGui::Bullet();
      ImGui::TextWrapped("sky-point marker — the target the point face queries");
    }
    ImGui::PopStyleColor();
  }

  ImGui::EndChild();  // ##MapReport
  ImGui::EndChild();  // ##AnalyzeRaypathInfo — the info region
}

void RenderAnalyzeRaypathViewWindow(GuiState& state) {
  if (!state.analyze_raypath_view_open) {
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
    feedback.map_refining_note.clear();
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
  if (!ImGui::Begin("Raypath view", &state.analyze_raypath_view_open,
                    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoScrollWithMouse)) {
    ImGui::End();
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
