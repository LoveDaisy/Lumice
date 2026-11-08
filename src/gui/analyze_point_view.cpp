#include "gui/analyze_point_view.hpp"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <optional>
#include <tuple>
#include <utility>

#include "app.hpp"                       // g_survey_executor — the cancel button reaches the one executor
#include "gui/analysis_panel.hpp"        // RowLabelTextTooltip — the component rows' hover identity
#include "gui/analyze_region_wheel.hpp"  // the wheel claim — one arbitration for both gesture regions
#include "gui/canvas_report_layout.hpp"
#include "gui/crystal_preview.hpp"  // CrystalParamHash — the mesh cache's change key
#include "gui/crystal_renderer.hpp"
#include "gui/gui_constants.hpp"
#include "gui/gui_logger.hpp"
#include "gui/gui_state.hpp"
#include "gui/semantic_colors.hpp"
#include "gui/theme.hpp"
#include "imgui.h"
#include "include/lumice_editor.h"  // LUMICE_GetCrystalMesh — the crystal's true-scale wireframe
#include "util/sky_direction.hpp"

namespace lumice::gui {
namespace {

std::string PathTextToString(const LUMICE_PathText& text) {
  if (text.data == nullptr || text.length == 0) {
    return std::string();
  }
  return std::string(text.data, text.length);
}

// The regions' text wraps, never clips: the cells' widths run from the side-by-side floors down
// to the ③ split, below several of these lines' full extent, and ImGui's TextDisabled sizes one
// clipped line (no DisabledTextWrapped exists; the color pair is by hand).
void DisabledWrappedText(const char* fmt, ...) {
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
  va_list args;
  va_start(args, fmt);
  ImGui::TextWrappedV(fmt, args);
  va_end(args);
  ImGui::PopStyleColor();
}

// A trace end's display words: the status verbatim, the reason in parentheses when the engine
// names one (an open arc's ends are boundary events — the reason IS the fact, R5's honesty).
std::string TraceEndText(const LUMICE_PathText& status, const LUMICE_PathText& reason) {
  std::string text = PathTextToString(status);
  if (!text.empty() && reason.data != nullptr && reason.length > 0) {
    text += " (";
    text.append(reason.data, reason.length);
    text += ")";
  }
  return text;
}

// The execution tag's display words (LUMICE_PATH_OUTCOME_*; anything else stays unnamed-honest).
const char* ExecutionText(int execution) {
  switch (execution) {
    case LUMICE_PATH_OUTCOME_COMPLETED:
      return "completed";
    case LUMICE_PATH_OUTCOME_CANCELLED:
      return "cancelled (partial)";
    case LUMICE_PATH_OUTCOME_DEADLINE_EXCEEDED:
      return "deadline exceeded (partial)";
    default:
      return "unknown";
  }
}

constexpr int kMaxRenderComponents = 64;

struct CurveArm {
  double PointCurvePoint::*value;
  PointCurveValueKind PointCurvePoint::*kind;
};
constexpr CurveArm kCurveArmFields[] = {
  { &PointCurvePoint::rho, &PointCurvePoint::rho_kind },
  { &PointCurvePoint::area, &PointCurvePoint::area_kind },
  { &PointCurvePoint::transmission, &PointCurvePoint::transmission_kind },
  { &PointCurvePoint::k_carea, &PointCurvePoint::k_kind },
  { &PointCurvePoint::product, &PointCurvePoint::product_kind },
};

}  // namespace

// ---- Display state ---------------------------------------------------------------------------------

void ResetPointViewStateForNewAdoption(PointViewState* view) {
  view->component_index = 0;
  view->sample_index = 0;
  view->playing = false;
  view->last_play_step_s = 0.0;
  view->curve_axis = {};
  // The toggles and the camera survive: they are the user's display preferences for THIS
  // session, not per-document state.
}

bool AdvancePointPlayback(PointViewState* view, int valid_count, double now_s) {
  if (!view->playing || valid_count <= 0) {
    return false;
  }
  if (view->sample_index < 0 || view->sample_index >= valid_count) {
    view->sample_index = 0;  // a stale cursor restarts the walk instead of reading out of range
    view->last_play_step_s = now_s;
    return true;
  }
  if (now_s - view->last_play_step_s < kPointPlaybackStepSeconds) {
    return false;
  }
  view->sample_index = (view->sample_index + 1) % valid_count;
  view->last_play_step_s = now_s;
  return true;
}

// ---- Request derivation -----------------------------------------------------------------------------

LUMICE_SinglePathRequest MakePointRequestForKey(const PointIntentKey& key, std::vector<int>* faces,
                                                std::vector<int>* counts) {
  *faces = key.selection.faces;
  *counts = key.selection.layer_face_counts;
  LUMICE_SinglePathRequest request{};
  request.struct_size = sizeof(request);
  request.crystal_id = key.selection.crystal_id;
  request.faces = faces->data();
  request.face_count = static_cast<int>(faces->size());
  request.layer_face_counts = counts->data();
  request.layer_count = static_cast<int>(counts->size());
  // The sky point's labels, through the ONE named conversion (util/sky_direction.hpp): the
  // intent carries the light-travel direction, the engine target fields carry the (altitude,
  // azimuth) labels of the sky point it comes from — the map coordinate input's inverse trip.
  float alt = 0.0f;
  float az = 0.0f;
  DirToAltAz(key.target_dir, &alt, &az);
  request.target_altitude_deg = alt;
  request.target_azimuth_deg = az;
  request.wavelength_nm = key.wavelength_nm;  // 0.0 = the engine's reference resolution
  request.sample_count = kPointQuerySampleCount;
  request.sun_grid_lat_count = kPointSunGridLatCount;
  request.warm_json = nullptr;
  request.warm_json_len = 0;
  request.deadline_ms = kPointQueryBudgetMs;
  request.interrupted = nullptr;  // the executor's cancel token is the one stop channel
  return request;
}

LUMICE_RepresentativeRayRequest MakeRayRequestForSample(const SelectedClassIdentity& identity, const double pose[9],
                                                        double wavelength_nm, std::vector<int>* faces,
                                                        std::vector<int>* counts, std::vector<double>* wavelengths) {
  *faces = identity.faces;
  *counts = identity.layer_face_counts;
  if (wavelength_nm > 0.0) {
    *wavelengths = { wavelength_nm };
  } else {
    wavelengths->clear();
  }
  LUMICE_RepresentativeRayRequest request{};
  request.struct_size = sizeof(request);
  request.crystal_id = identity.core_crystal_id;
  request.faces = faces->data();
  request.face_count = static_cast<int>(faces->size());
  request.layer_face_counts = counts->data();
  request.layer_count = static_cast<int>(counts->size());
  request.pose = pose;  // rebound to the job's own buffer by the executor
  request.wavelengths_nm = wavelengths->empty() ? nullptr : wavelengths->data();
  request.wavelength_count = static_cast<int>(wavelengths->size());
  return request;
}

// ---- Reading the adopted point document -------------------------------------------------------------

int PointComponentRows(const LUMICE_SinglePathResult& result, std::vector<PointComponentRow>* rows) {
  rows->clear();
  LUMICE_PathPointInfo info{};
  info.struct_size = sizeof(info);
  if (LUMICE_PathPointGetInfo(&result, &info) != LUMICE_PATH_READ_OK) {
    return 0;
  }
  if (info.outcome != LUMICE_PATH_POINT_DISCOVERED) {
    return 0;  // the mass arm renders its own block
  }
  const int count = static_cast<int>(std::min<size_t>(info.component_count, kMaxRenderComponents));
  for (int i = 0; i < count; ++i) {
    LUMICE_PathPointComponent component{};
    component.struct_size = sizeof(component);
    if (LUMICE_PathPointGetComponent(&result, static_cast<size_t>(i), &component) != LUMICE_PATH_READ_OK) {
      continue;
    }
    PointComponentRow row;
    row.kind = PathTextToString(component.kind);
    row.forward_text = TraceEndText(component.forward_status, component.forward_reason);
    row.backward_text = TraceEndText(component.backward_status, component.backward_reason);
    row.point_count = static_cast<int>(component.point_count);
    row.integral_available = component.integral_available != 0;
    row.available_points = static_cast<int>(component.available_points);
    row.total_points = static_cast<int>(component.total_points);
    row.fiber_integral = component.fiber_integral;
    rows->push_back(std::move(row));
  }
  return static_cast<int>(rows->size());
}

std::vector<int> CollectValidSampleIndices(const LUMICE_SinglePathResult& result, int component);

// The two point queries above the one map build: the count and the cursor mapping are the
// valid-index map's own size and lookup (ONE authority for the valid->stored walk — the
// renderers and the pump hold the map itself; these wrappers are the tests' and the
// 742.7 handover's per-call shape).
int PointValidSampleCount(const LUMICE_SinglePathResult& result, int component) {
  return static_cast<int>(CollectValidSampleIndices(result, component).size());
}

int PointValidSampleIndexAt(const LUMICE_SinglePathResult& result, int component, int valid_position) {
  if (valid_position < 0) {
    return -1;
  }
  const std::vector<int> indices = CollectValidSampleIndices(result, component);
  return valid_position < static_cast<int>(indices.size()) ? indices[static_cast<size_t>(valid_position)] : -1;
}

std::vector<int> CollectValidSampleIndices(const LUMICE_SinglePathResult& result, int component) {
  std::vector<int> indices;
  LUMICE_PathPointInfo info{};
  info.struct_size = sizeof(info);
  if (LUMICE_PathPointGetInfo(&result, &info) != LUMICE_PATH_READ_OK || info.outcome != LUMICE_PATH_POINT_DISCOVERED ||
      component < 0 || component >= static_cast<int>(info.component_count)) {
    return indices;
  }
  LUMICE_PathPointComponent head{};
  head.struct_size = sizeof(head);
  if (LUMICE_PathPointGetComponent(&result, static_cast<size_t>(component), &head) != LUMICE_PATH_READ_OK) {
    return indices;
  }
  indices.reserve(head.point_count);
  LUMICE_PathPointSample sample{};
  sample.struct_size = sizeof(sample);
  for (size_t i = 0; i < head.point_count; ++i) {
    if (LUMICE_PathPointGetSample(&result, static_cast<size_t>(component), i, &sample) == LUMICE_PATH_READ_OK &&
        sample.valid != 0) {
      indices.push_back(static_cast<int>(i));
    }
  }
  return indices;
}

std::vector<std::string> PointFactorLines(const LUMICE_PathPointSample& sample) {
  std::vector<std::string> lines;
  char buf[192];
  // Orientation: the pose's own angle words, the degeneracy named (R4 — a spin-degenerate pose
  // says so; the angles do not pretend to locate it).
  std::snprintf(buf, sizeof(buf), "pose: zenith %.2f deg, azimuth %.2f deg, roll %.2f deg%s", sample.zenith_deg,
                sample.azimuth_deg, sample.roll_deg, sample.angles_degenerate != 0 ? " (degenerate)" : "");
  lines.emplace_back(buf);
  std::snprintf(buf, sizeof(buf), "rho = %.6g rad^-3", sample.rho);
  lines.emplace_back(buf);
  std::snprintf(buf, sizeof(buf), "A_P = %.6g, A/a^2 = %.6g", sample.entry_measure, sample.entry_measure_a2);
  lines.emplace_back(buf);
  std::snprintf(buf, sizeof(buf), "T (total transmission) = %.6g", sample.total_transmission);
  lines.emplace_back(buf);
  if (sample.jacobian_available != 0) {
    std::snprintf(buf, sizeof(buf), "J (jacobian) = %.6g (sigma %.4g, %.4g)", sample.jacobian,
                  sample.singular_values[0], sample.singular_values[1]);
    lines.emplace_back(buf);
    std::snprintf(buf, sizeof(buf), "K = 1/(J+eps) = %.6g", sample.k_carea);
    lines.emplace_back(buf);
    std::snprintf(buf, sizeof(buf), "rho*A/a^2*T*K = %.6g", sample.factor_product);
    lines.emplace_back(buf);
  } else {
    // The named-unavailable states (R4): no zero stands in for a quantity the engine did not
    // produce, and the product that would use it is unavailable with it.
    lines.emplace_back("J (jacobian): unavailable");
    lines.emplace_back("K = 1/(J+eps): unavailable");
    lines.emplace_back("rho*A/a^2*T*K: unavailable");
  }
  const int interfaces = static_cast<int>(sample.interface_count);
  for (int i = 0; i < interfaces; ++i) {
    std::snprintf(buf, sizeof(buf), "interface %d/%d transmittance = %.6g (conditional on this path)", i + 1,
                  interfaces, sample.interface_transmittances[i]);
    lines.emplace_back(buf);
  }
  return lines;
}

std::vector<std::string> PointCaliberLines(const LUMICE_PathPointInfo& info) {
  std::vector<std::string> lines;
  char buf[256];
  std::snprintf(buf, sizeof(buf), "target: alt %.3f deg, az %.3f deg (deviation %.3f deg)", info.target_altitude_deg,
                info.target_azimuth_deg, info.target_deviation_deg);
  lines.emplace_back(buf);
  std::string faces_text;
  for (size_t i = 0; i < info.face_count; ++i) {
    if (!faces_text.empty()) {
      faces_text += "-";
    }
    faces_text += std::to_string(info.faces[i]);
  }
  std::snprintf(buf, sizeof(buf), "member faces %s, crystal %d, %s", faces_text.c_str(), info.crystal_id,
                PathTextToString(info.crystal_kind).c_str());
  lines.emplace_back(buf);
  const std::string source = PathTextToString(info.wavelength_source);
  std::snprintf(buf, sizeof(buf), "wavelength %.2f nm (%s), n = %.6f", info.wavelength_nm,
                source.empty() ? "source unknown" : source.c_str(), info.refractive_index);
  lines.emplace_back(buf);
  std::snprintf(buf, sizeof(buf), "discovery sample_count %d, budget %d ms, execution: %s", info.sample_count,
                info.budget_ms, ExecutionText(info.execution));
  lines.emplace_back(buf);
  return lines;
}

std::vector<std::string> PointMassLines(const LUMICE_PathPointMass& mass) {
  std::vector<std::string> lines;
  char buf[256];
  std::snprintf(buf, sizeof(buf), "point mass at alt %.3f deg, az %.3f deg (target separation %.3e deg)",
                mass.altitude_deg, mass.azimuth_deg, mass.target_separation_deg);
  lines.emplace_back(buf);
  if (mass.orbit_available != 0) {
    std::snprintf(buf, sizeof(buf), "orbit integral over %s: %.6g (%d/%d samples kept)",
                  PathTextToString(mass.measure_support).c_str(), mass.orbit_integral, mass.orbit_kept_count,
                  mass.orbit_sample_count);
    lines.emplace_back(buf);
  } else {
    std::snprintf(buf, sizeof(buf), "orbit integral: unavailable (%s)", PathTextToString(mass.orbit_reason).c_str());
    lines.emplace_back(buf);
  }
  return lines;
}

// ---- The contribution curve (756, audit §5's consumption contract) -----------------------------------

PointCurveValueKind PointCurveClassify(double value, bool gated) {
  if (!gated) {
    // The gate decides availability (audit §3: the flag-clear cell's C zero is the clear, not
    // data — a value comparison here would read that zero as a legal zero).
    return PointCurveValueKind::kUnavailable;
  }
  if (!std::isfinite(value) || value < 0.0) {
    // Not drawable: storage's NaN copied through a half-open gate, or a physically impossible
    // negative — named unavailable, never drawn, never patched (J4).
    return PointCurveValueKind::kUnavailable;
  }
  return value == 0.0 ? PointCurveValueKind::kExactZero : PointCurveValueKind::kPositive;
}

bool BuildPointContributionCurve(const LUMICE_SinglePathResult& result, int component, PointContributionCurve* out) {
  *out = PointContributionCurve{};
  LUMICE_PathPointInfo info{};
  info.struct_size = sizeof(info);
  if (LUMICE_PathPointGetInfo(&result, &info) != LUMICE_PATH_READ_OK || info.outcome != LUMICE_PATH_POINT_DISCOVERED ||
      component < 0 || component >= static_cast<int>(info.component_count)) {
    return false;
  }
  LUMICE_PathPointComponent head{};
  head.struct_size = sizeof(head);
  if (LUMICE_PathPointGetComponent(&result, static_cast<size_t>(component), &head) != LUMICE_PATH_READ_OK) {
    return false;
  }
  out->ok = true;
  out->kind = PathTextToString(head.kind);
  out->seed_index = head.seed_index;
  out->integral_available = head.integral_available != 0;
  out->available_points = static_cast<int>(head.available_points);
  out->total_points = static_cast<int>(head.total_points);
  out->fiber_integral = head.fiber_integral;
  const size_t n = head.point_count;
  out->points.resize(n);
  // The axis first (audit §5.1): x[0] = 0, x[i+1] = x[i] + increments[i] — N points over N-1
  // per-step increments, each point keeping its ORIGINAL index and x whatever its availability.
  // A count short of the point walk leaves the axis flat from there (monotone non-negative by
  // construction); the renderer guards a zero-span axis with its honest note.
  double x = 0.0;
  for (size_t i = 0; i < n; ++i) {
    out->points[static_cast<size_t>(i)].x_rad = x;
    if (i + 1 < n && i < head.arclength_count) {
      x += head.arclength_increments[i];
    }
  }
  out->valid_positions.reserve(n);
  LUMICE_PathPointSample sample{};
  sample.struct_size = sizeof(sample);
  for (size_t i = 0; i < n; ++i) {
    PointCurvePoint& p = out->points[i];
    if (LUMICE_PathPointGetSample(&result, static_cast<size_t>(component), i, &sample) != LUMICE_PATH_READ_OK) {
      continue;  // the honest hole: every arm stays kUnavailable, the point stays unreachable
    }
    p.valid = sample.valid != 0;
    p.jacobian_available = sample.jacobian_available != 0;
    p.rho = sample.rho;
    p.area = sample.entry_measure_a2;
    p.transmission = sample.total_transmission;
    p.k_carea = sample.k_carea;
    p.product = sample.factor_product;
    p.rho_kind = PointCurveClassify(sample.rho, /*gated=*/true);  // rho is produced unconditionally
    p.area_kind = PointCurveClassify(sample.entry_measure_a2, p.valid);
    p.transmission_kind = PointCurveClassify(sample.total_transmission, p.valid);
    p.k_kind = PointCurveClassify(sample.k_carea, p.jacobian_available);
    p.product_kind = PointCurveClassify(sample.factor_product, p.valid && p.jacobian_available);
    if (p.valid) {
      out->valid_positions.push_back(static_cast<int>(i));
    }
  }
  return true;
}

int PointCurveStoredAt(const PointContributionCurve& curve, int valid_position) {
  if (!curve.ok || valid_position < 0 || valid_position >= static_cast<int>(curve.valid_positions.size())) {
    return -1;
  }
  return curve.valid_positions[static_cast<size_t>(valid_position)];
}

double PointCurveXAt(const PointContributionCurve& curve, int valid_position) {
  const int stored = PointCurveStoredAt(curve, valid_position);
  return stored < 0 ? 0.0 : curve.points[static_cast<size_t>(stored)].x_rad;
}

int PointCurveValidPositionNearX(const PointContributionCurve& curve, double x_rad) {
  int best = -1;
  double best_distance = 0.0;
  for (size_t v = 0; v < curve.valid_positions.size(); ++v) {
    const double distance = std::abs(curve.points[static_cast<size_t>(curve.valid_positions[v])].x_rad - x_rad);
    if (best < 0 || distance < best_distance) {
      best = static_cast<int>(v);
      best_distance = distance;
    }
  }
  return best;
}

PointCurveAxisRange PointCurveAxisFit(const PointContributionCurve& curve) {
  std::vector<double> positives;
  positives.reserve(curve.points.size() * std::size(kCurveArmFields));
  for (const PointCurvePoint& point : curve.points) {
    for (const CurveArm& arm : kCurveArmFields) {
      if (point.*arm.kind == PointCurveValueKind::kPositive) {
        positives.push_back(point.*arm.value);
      }
    }
  }
  if (positives.empty()) {
    return {};
  }
  const auto extrema = std::minmax_element(positives.begin(), positives.end());
  const double lo = *extrema.first;
  const double hi = *extrema.second;
  if (lo >= hi / kCurveAxisNarrowRatio) {
    return { true, lo, hi };
  }
  const size_t rank = static_cast<size_t>(std::ceil(kCurveAxisLowerQuantile * positives.size())) - 1;
  std::nth_element(positives.begin(), positives.begin() + rank, positives.end());
  return { true, std::max(positives[rank], hi * kCurveAxisNoiseRatio), hi };
}

PointCurveAxisRange PointCurveVisibleRange(const PointCurveAxisRange& fit, PointCurveZoom* zoom, double wheel,
                                           double anchor) {
  if (!fit.valid) {
    return {};
  }
  const double fit_lo = std::log10(fit.lo);
  if (fit.hi == fit.lo) {
    zoom->log_lo = fit_lo;
    zoom->log_hi = fit_lo + 1.0;
    return fit;  // one value has no smaller data window; the plot still gives it a full decade
  }
  const double fit_hi = std::log10(fit.hi);
  const double full_span = fit_hi - fit_lo;
  double span = zoom->log_hi - zoom->log_lo;
  if (!(span > 0.0)) {
    zoom->log_lo = fit_lo;
    span = full_span;
  }
  span = std::clamp(span, std::min(0.25, full_span), full_span);
  zoom->log_lo = std::clamp(zoom->log_lo, fit_lo, fit_hi - span);
  if (wheel != 0.0) {
    anchor = std::clamp(anchor, 0.0, 1.0);
    const double new_span =
        std::clamp(span / std::pow(kCurveAxisZoomFactor, wheel), std::min(0.25, full_span), full_span);
    zoom->log_lo = std::clamp(zoom->log_lo + anchor * (span - new_span), fit_lo, fit_hi - new_span);
    span = new_span;
  }
  // log subtraction and a wheel-in/out round trip can leave the full span one ULP short.
  // Snap only the window, never the sample values, so the exact fit boundary stays included.
  if (full_span - span <= full_span * 1e-12) {
    zoom->log_lo = fit_lo;
    zoom->log_hi = fit_hi;
    return fit;
  }
  zoom->log_hi = zoom->log_lo + span;
  // Keep exact fit endpoints: a log/pow round trip must not hide a boundary value as "outside".
  return { true, zoom->log_lo <= fit_lo ? fit.lo : std::pow(10.0, zoom->log_lo),
           zoom->log_hi >= fit_hi ? fit.hi : std::pow(10.0, zoom->log_hi) };
}

PointCurveOutsideCount PointCurveCountOutside(const PointContributionCurve& curve, double lo, double hi) {
  PointCurveOutsideCount outside;
  for (const PointCurvePoint& point : curve.points) {
    bool hidden = false;
    for (const CurveArm& arm : kCurveArmFields) {
      const double value = point.*arm.value;
      if (point.*arm.kind != PointCurveValueKind::kPositive || (value >= lo && value <= hi)) {
        continue;
      }
      if (outside.points == 0 && !hidden) {
        outside.min = value;
        outside.max = value;
      }
      outside.min = std::min(outside.min, value);
      outside.max = std::max(outside.max, value);
      outside.above = outside.above || value > hi;
      hidden = true;
    }
    outside.points += hidden ? 1 : 0;
  }
  return outside;
}

std::string PointCurveOutsideLabel(const PointCurveOutsideCount& outside) {
  if (outside.points == 0) {
    return {};
  }
  char text[160];
  if (outside.above) {
    std::snprintf(text, sizeof(text), "%d points outside axis · min %.3g · max %.3g", outside.points, outside.min,
                  outside.max);
  } else {
    std::snprintf(text, sizeof(text), "%d points below axis · min %.3g", outside.points, outside.min);
  }
  return text;
}

std::vector<PointCurveLogTick> PointCurveLogTicks(double lo, double hi) {
  std::vector<PointCurveLogTick> ticks;
  if (!(lo > 0.0) || !(hi > 0.0) || lo > hi) {
    return ticks;
  }
  if (hi <= lo) {
    hi = lo * 10.0;  // a single value still draws inside one decade, not on an edge
  }
  const int first_exp = static_cast<int>(std::floor(std::log10(lo)));
  const int last_exp = static_cast<int>(std::ceil(std::log10(hi)));
  for (int e = first_exp; e <= last_exp; ++e) {
    const double decade = std::pow(10.0, static_cast<double>(e));
    for (const double mantissa : { 1.0, 2.0, 5.0 }) {
      const double value = decade * mantissa;
      if (value < lo || value > hi) {
        continue;
      }
      ticks.push_back(PointCurveLogTick{ value, mantissa == 1.0 });
    }
  }
  return ticks;
}

int PointCurveLogLabelStep(double lo, double hi, float plot_h_px, float line_h_px) {
  if (!(lo > 0.0) || !(hi > 0.0) || hi <= lo) {
    return 1;
  }
  const int decades = std::max(1, static_cast<int>(std::ceil(std::log10(hi) - std::log10(lo))));
  const float usable_line = std::max(1.0f, line_h_px * 1.5f);
  const int max_labels = std::max(1, static_cast<int>(std::floor(plot_h_px / usable_line)));
  return std::max(1, static_cast<int>(std::ceil(static_cast<double>(decades) / static_cast<double>(max_labels))));
}

std::vector<PointCurveRun> PointCurvePositiveRuns(const PointCurveValueKind* kinds, size_t count, const double* values,
                                                  double lo, double hi) {
  std::vector<PointCurveRun> runs;
  if (kinds == nullptr) {
    return runs;
  }
  const auto drawable = [&](size_t index) {
    return kinds[index] == PointCurveValueKind::kPositive &&
           (values == nullptr || (values[index] >= lo && values[index] <= hi));
  };
  size_t i = 0;
  while (i < count) {
    if (!drawable(i)) {
      ++i;
      continue;
    }
    const size_t begin = i;
    while (i < count && drawable(i)) {
      ++i;
    }
    runs.push_back(PointCurveRun{ static_cast<int>(begin), static_cast<int>(i) });
  }
  return runs;
}

// ---- The u-globe -------------------------------------------------------------------------------------

bool MakeSunGlobeProjector(const LUMICE_SinglePathResult& result, int component, SunGlobeProjector* out) {
  const int stored = PointValidSampleIndexAt(result, component, 0);
  if (stored < 0) {
    return false;
  }
  LUMICE_PathPointSample sample{};
  sample.struct_size = sizeof(sample);
  if (LUMICE_PathPointGetSample(&result, static_cast<size_t>(component), static_cast<size_t>(stored), &sample) !=
      LUMICE_PATH_READ_OK) {
    return false;
  }
  // The projector's basis: the centre is the component's first valid u; the reference up picks the
  // world +z when the centre is not parallel to it (the map's own fallback rule), else +x. The
  // centre/up/right triple is orthonormal by construction (Gram-Schmidt, then a re-cross).
  for (int k = 0; k < 3; ++k) {
    out->centre[k] = sample.sun_in_crystal[k];
  }
  static constexpr double kRefZ[3] = { 0.0, 0.0, 1.0 };
  static constexpr double kRefX[3] = { 1.0, 0.0, 0.0 };
  const double* reference = std::abs(out->centre[2]) < 0.999 ? kRefZ : kRefX;
  double up[3];
  const double dot = out->centre[0] * reference[0] + out->centre[1] * reference[1] + out->centre[2] * reference[2];
  for (int k = 0; k < 3; ++k) {
    up[k] = reference[k] - dot * out->centre[k];
  }
  const double up_norm = std::sqrt(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]);
  for (int k = 0; k < 3; ++k) {
    up[k] /= up_norm;
    out->up[k] = up[k];
  }
  out->right[0] = up[1] * out->centre[2] - up[2] * out->centre[1];
  out->right[1] = up[2] * out->centre[0] - up[0] * out->centre[2];
  out->right[2] = up[0] * out->centre[1] - up[1] * out->centre[0];
  return true;
}

void SunGlobeProject(const SunGlobeProjector& projector, const double u[3], double* out_x, double* out_y) {
  // Lambert azimuthal equal-area about the centre, the map's rho = 2 sin(theta/2) family,
  // normalized so the disk's rim is the antipode: rho in [0,1] over theta in [0, pi]. Screen up is
  // the projector's up, screen right the projector's right (the map's own handedness). The
  // antipode's azimuth is undefined (every lateral component vanishes) — atan2's (0,0) = 0 fixes
  // the deterministic rim point, so the projection is a total function of u.
  const double dot = u[0] * projector.centre[0] + u[1] * projector.centre[1] + u[2] * projector.centre[2];
  const double cos_theta = std::max(-1.0, std::min(1.0, dot));
  const double theta = std::acos(cos_theta);
  const double rho = 0.5 * 2.0 * std::sin(theta / 2.0);  // in [0,1]; 1 = the antipode (the rim)
  if (rho < 1e-12) {
    *out_x = 0.5;
    *out_y = 0.5;
    return;
  }
  const double x = u[0] * projector.right[0] + u[1] * projector.right[1] + u[2] * projector.right[2];
  const double y = u[0] * projector.up[0] + u[1] * projector.up[1] + u[2] * projector.up[2];
  const double azimuth = std::atan2(y, x);
  // Normalized disk coordinates: (0.5, 0.5) is the centre, the rim 0.5 from it; y down.
  *out_x = 0.5 + 0.5 * rho * std::cos(azimuth);
  *out_y = 0.5 - 0.5 * rho * std::sin(azimuth);
}

uint32_t SunGridCellColorRgba(double deviation_rad, uint8_t valid) {
  if (valid == 0 || !(deviation_rad >= 0.0) || !(deviation_rad <= 3.15)) {
    return IM_COL32(0, 0, 0, 0);  // invalid (or non-finite) cells paint nothing
  }
  const double t = std::min(1.0, deviation_rad / 3.14159265358979323846);
  // The map's dark-to-light ramp family, re-anchored on angles: dark at delta 0, light at pi.
  const int v = static_cast<int>(38.0 + 200.0 * t);
  return IM_COL32(v, v, std::min(255, v + 12), 235);
}

// ---- The representative-ray display -----------------------------------------------------------------

bool BuildRayTracePolyline(const LUMICE_RepresentativeRay& ray, float extension, RayTracePolyline* out) {
  out->points.clear();
  out->seg_kinds.clear();
  LUMICE_PathRayContacts contacts{};
  contacts.struct_size = sizeof(contacts);
  LUMICE_PathRayLegs legs{};
  legs.struct_size = sizeof(legs);
  if (LUMICE_RepresentativeRayGetContacts(&ray, &contacts) != LUMICE_PATH_READ_OK ||
      LUMICE_RepresentativeRayGetLegs(&ray, &legs) != LUMICE_PATH_READ_OK || contacts.contact_count == 0 ||
      legs.leg_count != contacts.contact_count + 1) {
    return false;
  }
  // The contacts' own spread sets the display scale (the extension is relative to it). The
  // readers hand back doubles; the display polyline is float — one explicit narrowing each.
  float spread = 0.0f;
  for (size_t i = 0; i < contacts.contact_count; ++i) {
    for (size_t j = i + 1; j < contacts.contact_count; ++j) {
      const float dx = static_cast<float>(contacts.contacts[j].point[0] - contacts.contacts[i].point[0]);
      const float dy = static_cast<float>(contacts.contacts[j].point[1] - contacts.contacts[i].point[1]);
      const float dz = static_cast<float>(contacts.contacts[j].point[2] - contacts.contacts[i].point[2]);
      spread = std::max(spread, std::sqrt(dx * dx + dy * dy + dz * dz));
    }
  }
  if (spread <= 0.0f) {
    spread = 1.0f;
  }
  const float ext = std::max(0.1f, extension) * spread;
  auto push_point = [&out](const float p[3]) {
    out->points.push_back(p[0]);
    out->points.push_back(p[1]);
    out->points.push_back(p[2]);
  };
  auto to_float = [](const double v[3]) {
    return std::array<float, 3>{ static_cast<float>(v[0]), static_cast<float>(v[1]), static_cast<float>(v[2]) };
  };
  // Entry leg: from outside along the incident direction (negative travel), to the first contact.
  const std::array<float, 3> c0 = to_float(contacts.contacts[0].point);
  const std::array<float, 3> leg0 = to_float(legs.legs[0].direction);
  const float entry_start[3] = { c0[0] - leg0[0] * ext, c0[1] - leg0[1] * ext, c0[2] - leg0[2] * ext };
  push_point(entry_start);
  push_point(c0.data());
  out->seg_kinds.push_back(0);
  // Internal legs: contact k to contact k+1.
  for (size_t k = 0; k + 1 < contacts.contact_count; ++k) {
    const std::array<float, 3> a = to_float(contacts.contacts[k].point);
    const std::array<float, 3> b = to_float(contacts.contacts[k + 1].point);
    push_point(a.data());
    push_point(b.data());
    out->seg_kinds.push_back(1);
  }
  // Exit leg: last contact onwards along the outgoing direction.
  const std::array<float, 3> cl = to_float(contacts.contacts[contacts.contact_count - 1].point);
  const std::array<float, 3> legl = to_float(legs.legs[legs.leg_count - 1].direction);
  const float exit_end[3] = { cl[0] + legl[0] * ext, cl[1] + legl[1] * ext, cl[2] + legl[2] * ext };
  push_point(cl.data());
  push_point(exit_end);
  out->seg_kinds.push_back(2);
  return true;
}

void RayTracePointToCrystalFrame(const double pose[9], float point[3]) {
  // World -> body: the transpose of the row-major body->world rotation.
  const float x = point[0];
  const float y = point[1];
  const float z = point[2];
  point[0] = static_cast<float>(pose[0] * x + pose[3] * y + pose[6] * z);
  point[1] = static_cast<float>(pose[1] * x + pose[4] * y + pose[7] * z);
  point[2] = static_cast<float>(pose[2] * x + pose[5] * y + pose[8] * z);
}

RayTraceDisplayState RayTraceDisplayStateFor(const RayTraceSession& trace, const RayTraceKey& current_key) {
  if (!trace.intent_valid) {
    return RayTraceDisplayState::kHidden;
  }
  if (trace.cancelled_for_key) {
    return RayTraceDisplayState::kHidden;
  }
  if (trace.adopted_result != nullptr && trace.key == current_key) {
    LUMICE_RepresentativeRayInfo info{};
    info.struct_size = sizeof(info);
    if (LUMICE_RepresentativeRayGetInfo(trace.adopted_result.get(), &info) != LUMICE_PATH_READ_OK) {
      return RayTraceDisplayState::kError;
    }
    return info.produced != 0 ? RayTraceDisplayState::kDrawn : RayTraceDisplayState::kRefused;
  }
  if (!trace.adopted_error.empty() && trace.key == current_key) {
    return RayTraceDisplayState::kError;
  }
  // The current key's answer has not arrived (or an older one is on hand) — the named computing
  // state, never an older pose's polyline (AC4).
  return RayTraceDisplayState::kComputing;
}

// ---- The pumps ---------------------------------------------------------------------------------------

namespace {

// The point face's issuing half (the map pump's shape): build the request for `key` and hand it
// to the executor with the id the pump already assigned. Buffers move into the executor's job.
void IssuePointIntent(SurveyExecutor& executor, const SurveyQueryId& id, const PointIntentKey& key) {
  std::vector<int> faces;
  std::vector<int> counts;
  const LUMICE_SinglePathRequest request = MakePointRequestForKey(key, &faces, &counts);
  executor.IssuePointQuery(id, request, std::move(faces), std::move(counts));
}

}  // namespace

void PumpPointQuery(GuiState& state, SurveyExecutor& executor, double now_s) {
  PointQuerySession& point = state.analysis.point;
  PointViewState& view = state.analysis.point_view;

  // 1. Drain: adopt the finished point iff it still answers the CURRENT intent and the intent
  //    stands (the survey gate's own rule — a cancelled intent accepts no outcome). A refusal is
  //    an answer for a standing intent (a multi-layer selection's named refusal IS the answer).
  //    A completed document's ADOPTION resets the display cursor: the component/sample selection
  //    belonged to the document it read.
  while (std::optional<SurveyExecutor::Finished> finished = executor.TakeFinished(QueryFace::kPoint)) {
    if (point.intent_valid && !point.cancelled_for_key && finished->id == point.intent_id) {
      const bool adoption_changed = finished->ok && finished->point_result != point.adopted_result;
      point.adopted_result = finished->ok ? finished->point_result : nullptr;
      point.adopted_error = finished->ok ? std::string() : std::move(finished->error);
      if (adoption_changed) {
        ResetPointViewStateForNewAdoption(&view);
      }
    }
  }

  // 2. The live selection through the ONE identity chain (a56 — the same derivation the survey
  //    and map pumps consume), plus the sky-point intent and the shared wavelength knob. A point
  //    intent exists only when all three do.
  const std::optional<SelectedClassIdentity> identity = SelectedClassIdentityFor(state);
  if (!identity.has_value() || !state.analysis_result.payload || !state.analysis.sky_point.valid) {
    if (point.intent_valid) {
      // The selection or the sky point died: the intent dies with it, adopted outcome included —
      // the point block's answer must not linger without its question (AC5).
      executor.CancelInFlight(QueryFace::kPoint);
      point = PointQuerySession{};
      ResetPointViewStateForNewAdoption(&view);
    }
    return;
  }
  PointIntentKey key;
  key.selection = SurveySelectionKeyFor(*identity, state.analysis_result.payload->snapshot_generation);
  for (int k = 0; k < 3; ++k) {
    key.target_dir[k] = state.analysis.sky_point.dir[k];
  }
  key.wavelength_nm = state.analysis.map_wavelength_nm;

  // 3. Issue or supersede. A key change DROPS the adopted outcome — a newer sky point's answer
  //    must not linger under the new target (AC5's display-side elimination; unlike the map
  //    there is no honest "older picture anchored on the sky" claim here). The cancel latch
  //    keeps this branch from re-issuing while the same key stands (the button's meaning).
  if (!point.intent_valid || !(point.key == key)) {
    executor.CancelInFlight(QueryFace::kPoint);
    point = PointQuerySession{};
    point.intent_valid = true;
    point.key = key;
    point.intent_id = executor.NextId(key.selection.generation);
    point.intent_start_s = now_s;  // the wait display's epoch — a moved point restarts it (plan §3.1)
    IssuePointIntent(executor, point.intent_id, key);
  }
}

void PumpRayTrace(GuiState& state, SurveyExecutor& executor) {
  RayTraceSession& trace = state.analysis.trace;

  // 1. Drain: adopt the finished ray iff it still answers the CURRENT intent and the intent
  //    stands (the same gate every face runs). A refusal rides INSIDE the handle (Info.produced
  //    0 + the named reason) — an adopted handle either way.
  while (std::optional<SurveyExecutor::Finished> finished = executor.TakeFinished(QueryFace::kRay)) {
    if (trace.intent_valid && !trace.cancelled_for_key && finished->id == trace.intent_id) {
      trace.adopted_result = finished->ok ? finished->ray_result : nullptr;
      trace.adopted_error = finished->ok ? std::string() : std::move(finished->error);
    }
  }

  // 2. The desired key: the CURRENT point adoption + the selected component + the selected
  //    valid sample. No point adoption (or the mass arm, or no valid sample) means no ray
  //    intent — the session is dropped, the 3D view shows its honest state.
  const PointQuerySession& point = state.analysis.point;
  const PointViewState& view = state.analysis.point_view;
  const std::optional<SelectedClassIdentity> identity = SelectedClassIdentityFor(state);
  if (!identity.has_value() || !point.adopted_result) {
    if (trace.intent_valid) {
      executor.CancelInFlight(QueryFace::kRay);
      trace = RayTraceSession{};
    }
    return;
  }
  LUMICE_PathPointInfo info{};
  info.struct_size = sizeof(info);
  if (LUMICE_PathPointGetInfo(point.adopted_result.get(), &info) != LUMICE_PATH_READ_OK ||
      info.outcome != LUMICE_PATH_POINT_DISCOVERED || info.component_count == 0) {
    if (trace.intent_valid) {
      executor.CancelInFlight(QueryFace::kRay);
      trace = RayTraceSession{};
    }
    return;
  }
  const int component = std::clamp(view.component_index, 0, static_cast<int>(info.component_count) - 1);
  // One scan for both the count and the cursor's stored row (the map the renderers share too).
  const std::vector<int> valid_indices = CollectValidSampleIndices(*point.adopted_result, component);
  if (valid_indices.empty()) {
    if (trace.intent_valid) {
      executor.CancelInFlight(QueryFace::kRay);
      trace = RayTraceSession{};
    }
    return;
  }
  const int stored =
      valid_indices[static_cast<size_t>(std::clamp(view.sample_index, 0, static_cast<int>(valid_indices.size()) - 1))];
  LUMICE_PathPointSample sample{};
  sample.struct_size = sizeof(sample);
  if (LUMICE_PathPointGetSample(point.adopted_result.get(), static_cast<size_t>(component), static_cast<size_t>(stored),
                                &sample) != LUMICE_PATH_READ_OK) {
    if (trace.intent_valid) {
      executor.CancelInFlight(QueryFace::kRay);
      trace = RayTraceSession{};
    }
    return;
  }
  RayTraceKey key;
  key.point_id = point.intent_id;
  key.component = component;
  key.sample = stored;

  // 3. Issue or supersede (the ray face's queue-replacement rule — the QueryFace comment).
  if (!trace.intent_valid || !(trace.key == key)) {
    executor.CancelInFlight(QueryFace::kRay);
    trace = RayTraceSession{};
    trace.intent_valid = true;
    trace.key = key;
    trace.intent_id = executor.NextId(point.key.selection.generation);
    std::vector<int> faces;
    std::vector<int> counts;
    std::vector<double> wavelengths;
    const LUMICE_RepresentativeRayRequest request = MakeRayRequestForSample(
        *identity, sample.pose, state.analysis.map_wavelength_nm, &faces, &counts, &wavelengths);
    executor.IssueRayQuery(trace.intent_id, request, std::move(faces), std::move(counts));
  }
}

// ---- Renderer-shared helpers -------------------------------------------------------------------------

namespace {

std::optional<LUMICE_PathPointInfo> ReadPointInfo(const LUMICE_SinglePathResult& result) {
  LUMICE_PathPointInfo info{};
  info.struct_size = sizeof(info);
  if (LUMICE_PathPointGetInfo(&result, &info) != LUMICE_PATH_READ_OK) {
    return std::nullopt;
  }
  return info;
}

// One text line in every state. Detailed reasons remain in the report; the short state is
// still visible when that report is scrolled to a long legend or the last factor.
void RenderPointStatus(const PointQuerySession& point) {
  const char* label = "No target";
  bool partial = false;
  switch (PointQueryStateFor(point)) {
    case PointQueryState::kNoIntent:
      break;
    case PointQueryState::kWaiting:
      label = "Waiting";
      break;
    case PointQueryState::kCancelled:
      label = "Cancelled";
      break;
    case PointQueryState::kRefused:
      label = "Refused";
      break;
    case PointQueryState::kCompleted: {
      const auto info = ReadPointInfo(*point.adopted_result);
      partial = info && info->execution != LUMICE_PATH_OUTCOME_COMPLETED;
      label = partial ? "Partial" : "Ready";
      break;
    }
  }
  ImGui::PushStyleColor(ImGuiCol_Text, partial ? WarningTextColor() : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::TextUnformatted(label);
  ImGui::PopStyleColor();
}

// The sample the display currently points at: (component, stored index, the sample itself, the
// component's valid count). nullopt outside the discovered arm with a usable selection.
struct CurrentSample {
  int component = 0;
  int stored = 0;
  int valid_count = 0;
  LUMICE_PathPointSample sample{};
};

// The sample the display currently points at: (component, stored index, the sample itself, the
// component's valid count). nullopt outside the discovered arm with a usable selection. Takes
// the component's valid-index map (CollectValidSampleIndices) — built once per renderer frame.
std::optional<CurrentSample> CurrentSampleOf(const LUMICE_SinglePathResult& result, const PointViewState& view,
                                             const std::vector<int>& valid_indices) {
  const std::optional<LUMICE_PathPointInfo> info = ReadPointInfo(result);
  if (!info.has_value() || info->outcome != LUMICE_PATH_POINT_DISCOVERED || info->component_count == 0 ||
      valid_indices.empty()) {
    return std::nullopt;
  }
  CurrentSample out;
  out.component = std::clamp(view.component_index, 0, static_cast<int>(info->component_count) - 1);
  out.valid_count = static_cast<int>(valid_indices.size());
  out.stored = valid_indices[static_cast<size_t>(std::clamp(view.sample_index, 0, out.valid_count - 1))];
  out.sample.struct_size = sizeof(out.sample);
  if (LUMICE_PathPointGetSample(&result, static_cast<size_t>(out.component), static_cast<size_t>(out.stored),
                                &out.sample) != LUMICE_PATH_READ_OK) {
    return std::nullopt;
  }
  return out;
}

// The mesh cache: one crystal's wireframe, rebuilt when the pool slot or the config's param hash
// moves. LUMICE_CrystalMesh owns its storage by value (lumice_editor.h: fixed-size caller
// arrays, no destroy) — the by-value cache is the whole ownership story; overwriting sheds the
// old copy with the struct. The mesh is TRUE-SCALE crystal frame — no AABB normalization, no
// axis swap: the trace's contacts are true-scale world/crystal quantities and the two must
// overlay (AC4).
struct CachedCrystalMesh {
  int pool = -1;
  int hash = 0;
  bool valid = false;
  LUMICE_CrystalMesh mesh{};
};

CachedCrystalMesh& PointMeshCache() {
  static CachedCrystalMesh cache;
  return cache;
}

// Uploaded geometry is a pure function of the complete display state: frozen config hash,
// pool, pose[9], crystal_frame, centre[3], fit. In particular centre/fit can stay unchanged
// while a symmetric crystal rotates, so neither pose nor frame may be omitted from the key.
using PointMeshUploadKey = std::tuple<int, int, std::array<double, 9>, bool, std::array<float, 3>, float>;
struct PointCrystalRenderer {
  CrystalRenderer renderer;
  std::optional<PointMeshUploadKey> uploaded;
  bool ready = false;
};

PointCrystalRenderer& PointRenderer() {
  static PointCrystalRenderer renderer;
  return renderer;
}

Ray3DFrameInfo g_ray3d_frame;

std::array<float, 3> ToRay3DScene(const float display[3], const Ray3DFrameInfo& frame) {
  return { (display[0] - frame.centre[0]) / frame.fit, (display[1] - frame.centre[1]) / frame.fit,
           (display[2] - frame.centre[2]) / frame.fit };
}

ImVec2 ProjectSceneToPixel(const float display[3], const Ray3DFrameInfo& frame) {
  const auto p = ToRay3DScene(display, frame);
  const float* m = frame.mvp;
  const float w = m[3] * p[0] + m[7] * p[1] + m[11] * p[2] + m[15];
  // All content is inside the unit bounding sphere. The camera's focal half-height stays
  // above 0.3, so its eye distance exceeds 0.3 / tan(15deg) > 1 and perspective w is positive.
  const float x = (m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12]) / w;
  const float y = (m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13]) / w;
  return ImVec2(frame.canvas_min.x + (x + 1.0f) * 0.5f * frame.canvas_size.x,
                frame.canvas_min.y + (1.0f - y) * 0.5f * frame.canvas_size.y);
}

// The crystal param in the analysis geometry's OWN caliber (scene_to_analytic's Nominal rule):
// every shape distribution flattened to its centre — the mesh on show is the mesh the queries ran
// against, not a fresh sample of the document's randomized shape.
LUMICE_Distribution NominalDistribution(const ShapeDist& d) {
  return LUMICE_Distribution{ static_cast<int>(d.type), d.center, 0.0f };
}

bool BuildPointCrystalMesh(const CrystalConfig& cr, LUMICE_CrystalMesh* out) {
  LUMICE_CrystalParam param{};
  param.type = (cr.type == CrystalType::kPrism) ? 0 : 1;
  if (cr.type == CrystalType::kPrism) {
    param.height = NominalDistribution(cr.height);
  } else {
    param.prism_h = NominalDistribution(cr.prism_h);
    param.upper_h = NominalDistribution(cr.upper_h);
    param.lower_h = NominalDistribution(cr.lower_h);
    param.upper_wedge_angle = cr.upper_alpha;
    param.lower_wedge_angle = cr.lower_alpha;
  }
  for (int i = 0; i < 6; i++) {
    param.face_distance[i] = NominalDistribution(cr.face_distance[i]);
  }
  FillSyncGroupArray(cr, param.sync_group);
  *out = {};
  return LUMICE_GetCrystalMesh(&param, /*sample_seed=*/0, out) == LUMICE_OK;
}

void RotateBodyToWorld(const double pose[9], const float v[3], float out[3]) {
  out[0] = static_cast<float>(pose[0] * v[0] + pose[1] * v[1] + pose[2] * v[2]);
  out[1] = static_cast<float>(pose[3] * v[0] + pose[4] * v[1] + pose[5] * v[2]);
  out[2] = static_cast<float>(pose[6] * v[0] + pose[7] * v[1] + pose[8] * v[2]);
}

ImU32 SegmentColor(int kind, float alpha) {
  const ImU32 color = kRayTraceSegmentColors[kind == 0 ? 0 : kind == 1 ? 1 : 2];
  return (color & ~IM_COL32_A_MASK) | (static_cast<ImU32>(255 * alpha) << IM_COL32_A_SHIFT);
}

}  // namespace

const Ray3DFrameInfo& LastRay3DFrameInfo() {
  return g_ray3d_frame;
}

// The 3D view's orbit basis in the display frame (declared in the hpp for the
// gui_test pin to consume the same basis the renderer draws with; the convention is doc'd in
// doc/geometry/coordinate-convention.md §9.8).
void OrbitBasis(float yaw_deg, float pitch_deg, double fwd[3], double right[3], double up[3]) {
  const double yaw = static_cast<double>(yaw_deg) * (3.14159265358979323846 / 180.0);
  const double pitch = std::clamp(static_cast<double>(pitch_deg), -88.0, 88.0) * (3.14159265358979323846 / 180.0);
  // z-up spherical coordinates about the scene's display convention (the crystal's c-axis, +z,
  // is up on screen): yaw sweeps the horizontal plane, pitch rises from it. `fwd` is the view
  // direction, from the viewer INTO the scene — the eye and the visible near face are on the
  // −fwd side (with up = right x fwd below, the basis satisfies right x up = −fwd).
  fwd[0] = std::cos(pitch) * std::cos(yaw);
  fwd[1] = std::cos(pitch) * std::sin(yaw);
  fwd[2] = std::sin(pitch);
  // right = fwd x worldUp, up = right x fwd. With |pitch| <= 88° the clamp keeps cos(pitch)
  // >= 0.035, so `right` normalizes to exactly (sin yaw, −cos yaw, 0) — a function of the yaw
  // alone, which is what makes a pure pitch drag unable to move anything on screen sideways
  // (the gui_test pin asserts that as an exact invariant). The pitch clamp is also what
  // prevents this cross product's one real singularity (fwd parallel to +z at pitch ±90°); the
  // `n < 1e-12` branch below is an unreachable defense against the double-rounded cosine,
  // kept so the normalizer never divides by zero even if the clamp were ever widened.
  right[0] = fwd[1] * 1.0 - fwd[2] * 0.0;
  right[1] = fwd[2] * 0.0 - fwd[0] * 1.0;
  right[2] = 0.0;
  const double n = std::sqrt(right[0] * right[0] + right[1] * right[1]);
  if (n < 1e-12) {
    right[0] = 1.0;
    right[1] = 0.0;
  } else {
    right[0] /= n;
    right[1] /= n;
  }
  up[0] = right[1] * fwd[2] - right[2] * fwd[1];
  up[1] = right[2] * fwd[0] - right[0] * fwd[2];
  up[2] = right[0] * fwd[1] - right[1] * fwd[0];
}

const CrystalConfig* FrozenCrystalForPool(const GuiState& state, int pool) {
  if (pool < 0 || pool >= static_cast<int>(state.analysis.frozen_crystals.size())) {
    return nullptr;
  }
  return &state.analysis.frozen_crystals[static_cast<size_t>(pool)];
}

// ---- The sun-direction globe -------------------------------------------------------------------------

void RenderAnalyzeSunGlobeRegion(GuiState& state, const ImVec2& size) {
  ImGui::BeginChild("##AnalyzeSunGlobe", size, ImGuiChildFlags_Borders);
  PointQuerySession& point = state.analysis.point;
  PointViewState& view = state.analysis.point_view;
  // SameLine is a judgment, not a default (the map region's rule): the side-by-side cell sits at
  // its own content floor, the stacked form can be narrower, and a clipped toggle or readout is
  // an unreachable fact — successors wrap when the line cannot hold them.
  const auto fits_on_line = [&](float next_w) {
    const float line_right = ImGui::GetContentRegionMax().x + ImGui::GetWindowPos().x;
    return ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + next_w <= line_right;
  };

  // Every result branch writes only the report after the fixed component/canvas/sample stack.
  const auto render_state_area = [](auto&& draw_lines) { draw_lines(); };

  // ---- The fixed interaction stack (R4): the controls a hand must reach are submitted outside
  // any scrolling region at every cell the window's minimum admits — the title and the
  // deviation-grid toggle here; the combo, the canvas, the weight bar and the slider/play row
  // in the discovered arm below. ----
  ImGui::TextUnformatted("Sun direction globe");
  const float dev_grid_w =
      ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize("Deviation grid").x;
  // A unit wider than the row is not fixed by wrapping (the wrapped checkbox's label would
  // still clip — its name is the toggle's only identity). Below that the join is a judgment;
  // at the over-width shapes the toggle keeps its id with an empty display label and the name
  // moves to wrapped text under it (one id across the forms, "###" anchored).
  const bool dev_grid_fits = dev_grid_w <= ImGui::GetContentRegionAvail().x;
  if (dev_grid_fits && fits_on_line(dev_grid_w)) {
    ImGui::SameLine();
  }
  if (dev_grid_fits) {
    ImGui::Checkbox("Deviation grid###globe_dev_grid", &view.show_deviation_grid);
  } else {
    ImGui::Checkbox("###globe_dev_grid", &view.show_deviation_grid);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped("Deviation grid");
    ImGui::PopStyleColor();
  }

  const auto info = point.adopted_result ? ReadPointInfo(*point.adopted_result) : std::nullopt;
  std::vector<PointComponentRow> rows;
  const int component_total = info ? PointComponentRows(*point.adopted_result, &rows) : 0;
  int component = component_total > 0 ? std::clamp(view.component_index, 0, component_total - 1) : 0;
  std::string labels;
  for (int i = 0; i < component_total; ++i) {
    char item[96];
    std::snprintf(item, sizeof(item), "component %d: %s, %d pts%s", i + 1, rows[static_cast<size_t>(i)].kind.c_str(),
                  rows[static_cast<size_t>(i)].point_count,
                  rows[static_cast<size_t>(i)].integral_available ? "" : ", integral unavailable");
    labels += item;
    labels += '\0';
  }
  if (component_total == 0) {
    labels = std::string("No component\0", 13);
  }
  labels += '\0';
  ImGui::BeginDisabled(component_total <= 1);
  ImGui::SetNextItemWidth(-1.0f);
  if (ImGui::Combo("##globe_component", &component, labels.c_str())) {
    view.component_index = component;
    view.sample_index = 0;
    view.playing = false;
  }
  ImGui::EndDisabled();
  const std::vector<int> valid_indices =
      component_total > 0 ? CollectValidSampleIndices(*point.adopted_result, component) : std::vector<int>{};
  const int valid_count = static_cast<int>(valid_indices.size());
  if (valid_count > 0) {
    AdvancePointPlayback(&view, valid_count, ImGui::GetTime());
  }
  const float play_w =
      std::max(ImGui::CalcTextSize("Stop").x, ImGui::CalcTextSize("Play").x) + ImGui::GetStyle().FramePadding.x * 2.0f;
  RenderPointStatus(point);
  const ImVec2 available = ImGui::GetContentRegionAvail();
  const bool shared_line = available.x - play_w - ImGui::GetStyle().ItemSpacing.x >= UiPx(80.0f);
  const float controls_h = ImGui::GetFrameHeightWithSpacing() * (shared_line ? 1.0f : 2.0f);
  const float graph_side = std::min(
      available.x, CanvasHeightBeforeReport(available.y, controls_h, UiPx(64.0f), ImGui::GetStyle().ItemSpacing.y));
  const ImVec2 graph_min = ImGui::GetCursorScreenPos();
  ImDrawList* graph_draw_list = ImGui::GetWindowDrawList();
  if (graph_side > 0.0f) {
    ImGui::InvisibleButton("##globe_canvas", ImVec2(graph_side, graph_side));
    graph_draw_list->AddCircle(ImVec2(graph_min.x + graph_side * 0.5f, graph_min.y + graph_side * 0.5f),
                               std::max(1.0f, graph_side * 0.5f - UiPx(4.0f)), ImGui::GetColorU32(ImGuiCol_Border));
  }
  int pos = valid_count > 0 ? std::clamp(view.sample_index, 0, valid_count - 1) : 0;
  ImGui::BeginDisabled(valid_count == 0);
  ImGui::SetNextItemWidth(shared_line ? available.x - play_w - ImGui::GetStyle().ItemSpacing.x : available.x);
  if (ImGui::SliderInt("##globe_sample", &pos, 0, std::max(0, valid_count - 1), valid_count > 0 ? "%d" : "—")) {
    view.sample_index = pos;
    view.playing = false;
  }
  if (shared_line) {
    ImGui::SameLine();
  }
  if (ImGui::Button(view.playing ? "Stop##globe_play" : "Play##globe_play", ImVec2(play_w, 0.0f))) {
    view.playing = !view.playing;
    view.last_play_step_s = ImGui::GetTime();
  }
  ImGui::EndDisabled();
  ImGui::BeginChild("##AnalyzeGlobeState", RemainingReportSize(), ImGuiChildFlags_Borders);
  const auto render_content = [&] {
    const PointQueryState st = PointQueryStateFor(point);
    if (st == PointQueryState::kNoIntent) {
      render_state_area([&] {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        ImGui::TextWrapped("Crystal-frame sun directions — a pose projection, not the full SO(3).");
        if (!state.analysis.sky_point.valid) {
          ImGui::TextWrapped(
              "Pick a sky point on the raypath view (or type coordinates there); the fiber of poses "
              "that light it takes this region.");
        } else {
          ImGui::TextWrapped("The fiber takes this region once a class is selected.");
        }
        ImGui::PopStyleColor();
      });
      return;
    }
    if (st == PointQueryState::kWaiting || st == PointQueryState::kCancelled || st == PointQueryState::kRefused) {
      render_state_area([&] {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        ImGui::TextWrapped("Crystal-frame sun directions — a pose projection, not the full SO(3).");
        switch (st) {
          case PointQueryState::kWaiting: {
            const std::string wait_line =
                PointWaitLineFor(point, ImGui::GetTime(), g_survey_executor.InflightPollSnapshot());
            state.analysis.wait_feedback_texts.point_wait_line_fiber = wait_line;
            ImGui::TextWrapped("%s", wait_line.c_str());
            break;
          }
          case PointQueryState::kCancelled:
            ImGui::TextWrapped(
                "Point cancelled for this selection. Moving the sky point — or re-picking the class — "
                "asks anew.");
            break;
          default:
            ImGui::TextWrapped("Point refused: %s", point.adopted_error.c_str());
            break;
        }
        ImGui::PopStyleColor();
      });
      return;
    }

    // Completed. Either arm reads through the document's own Info.
    if (!info.has_value()) {
      render_state_area([&] { DisabledWrappedText("The adopted point document did not answer its Info reader."); });
      return;
    }
    if (info->outcome == LUMICE_PATH_POINT_MASS) {
      render_state_area([&] {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        ImGui::TextWrapped("Crystal-frame sun directions — a pose projection, not the full SO(3).");
        ImGui::TextWrapped("A rank-0 point mass has no fiber to project — the details carry the mass numbers.");
        ImGui::PopStyleColor();
      });
      return;
    }

    // The discovered arm: the selected component's fiber, the current sample marker, the weight
    // bar, the slider and playback. The partial-document note rides EVERY state area this arm can
    // show — including the empty-payload early returns below: a deadline stopped before the
    // component walk can legitimately return zero components, and a window open on its own (the
    // diagnostics region may be off screen or on another workspace) must not present an
    // unfinished document as a definite physical absence.
    const bool partial_document = info->execution != LUMICE_PATH_OUTCOME_COMPLETED;
    const auto draw_partial_note = [&] {
      if (partial_document) {
        ImGui::PushStyleColor(ImGuiCol_Text, WarningTextColor());
        ImGui::TextWrapped(
            "Partial document (%s) — what arrived is on show; emptiness here is the run's "
            "stop, not a physical absence.",
            ExecutionText(info->execution));
        ImGui::PopStyleColor();
      }
    };
    if (component_total == 0) {
      LUMICE_PathPointReach reach{};
      reach.struct_size = sizeof(reach);
      const bool have_reach = LUMICE_PathPointGetReach(point.adopted_result.get(), &reach) == LUMICE_PATH_READ_OK;
      render_state_area([&] {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        ImGui::TextWrapped("Crystal-frame sun directions — a pose projection, not the full SO(3).");
        draw_partial_note();
        if (have_reach && !reach.target_in_range) {
          ImGui::TextWrapped(
              "No fiber here: the target's deviation is outside the range of this path's deflection field "
              "(the reach probe excludes it at its resolution).");
        } else {
          ImGui::TextWrapped(
              "No fiber component at this point: the finite crystal passes no pose along the queried "
              "deflection.");
        }
        ImGui::PopStyleColor();
      });
      return;
    }
    if (valid_count == 0) {
      render_state_area([&] {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        ImGui::TextWrapped("Crystal-frame sun directions — a pose projection, not the full SO(3).");
        draw_partial_note();
        ImGui::TextWrapped("This component carries no valid sample — nothing to project.");
        ImGui::PopStyleColor();
      });
      return;
    }
    // Payload availability is independent of whether this geometry can show the canvas.
    LUMICE_PathSunGrid grid{};
    grid.struct_size = sizeof(grid);
    const bool have_grid = view.show_deviation_grid &&
                           LUMICE_PathPointGetSunGrid(point.adopted_result.get(), &grid) == LUMICE_PATH_READ_OK &&
                           grid.cell_count > 0;
    if (graph_side > 0.0f) {
      SunGlobeProjector projector;
      const bool have_projector = MakeSunGlobeProjector(*point.adopted_result, component, &projector);

      const float side = graph_side;
      ImDrawList* dl = graph_draw_list;
      const ImVec2 canvas_min = graph_min;
      const ImVec2 centre_px(canvas_min.x + side * 0.5f, canvas_min.y + side * 0.5f);
      const float radius_px = side * 0.5f - UiPx(4.0f);
      auto to_screen = [&](double x, double y) {
        return ImVec2(static_cast<float>(centre_px.x + (x - 0.5) * 2.0 * radius_px),
                      static_cast<float>(centre_px.y + (y - 0.5) * 2.0 * radius_px));
      };
      // The disk: rim + interior, dark — the map's own picture convention (the cells carry the data).
      dl->AddCircleFilled(centre_px, radius_px, IM_COL32(16, 18, 22, 255));
      dl->AddCircle(centre_px, radius_px, IM_COL32(70, 74, 82, 200), 0, 1.0f);

      // The deviation backdrop: only when the document carries a grid (AC2's named-absence rule) and
      // the toggle stands.
      if (have_grid) {
        const double deg_to_rad = 3.14159265358979323846 / 180.0;
        for (int i = 0; i < grid.lat_count; ++i) {
          const double lat = (-90.0 + (i + 0.5) * 180.0 / grid.lat_count) * deg_to_rad;
          for (int j = 0; j < grid.lon_count; ++j) {
            const double lon = (-180.0 + (j + 0.5) * 360.0 / grid.lon_count) * deg_to_rad;
            const size_t cell = static_cast<size_t>(i) * static_cast<size_t>(grid.lon_count) + static_cast<size_t>(j);
            if (cell >= grid.cell_count || grid.valid[cell] == 0) {
              continue;
            }
            const double u[3] = { std::cos(lat) * std::cos(lon), std::cos(lat) * std::sin(lon), std::sin(lat) };
            double gx = 0.0;
            double gy = 0.0;
            SunGlobeProject(projector, u, &gx, &gy);
            const ImVec2 p = to_screen(gx, gy);
            const float half = std::max(1.5f, radius_px / 48.0f);
            dl->AddRectFilled(ImVec2(p.x - half, p.y - half), ImVec2(p.x + half, p.y + half),
                              SunGridCellColorRgba(grid.deviation_rad[cell], grid.valid[cell]));
          }
        }
      }

      // The fiber: the selected component's VALID samples, in arclength order, as contiguous runs —
      // broken where an invalid sample sits (AC2: this component's own valid samples, nothing joined).
      if (have_projector) {
        std::vector<ImVec2> run;
        auto flush_run = [&]() {
          if (run.size() >= 2) {
            dl->AddPolyline(run.data(), static_cast<int>(run.size()), ImGui::ColorConvertFloat4ToU32(AccentColor()), 0,
                            2.0f);
          }
          run.clear();
        };
        LUMICE_PathPointSample probe{};
        probe.struct_size = sizeof(probe);
        for (size_t v = 0; v < valid_indices.size(); ++v) {
          // A gap in stored indices means an invalid sample sat between: the run breaks there.
          if (v > 0 && valid_indices[v] != valid_indices[v - 1] + 1) {
            flush_run();
          }
          if (LUMICE_PathPointGetSample(point.adopted_result.get(), static_cast<size_t>(component),
                                        static_cast<size_t>(valid_indices[v]), &probe) != LUMICE_PATH_READ_OK) {
            flush_run();
            continue;
          }
          double gx = 0.0;
          double gy = 0.0;
          SunGlobeProject(projector, probe.sun_in_crystal, &gx, &gy);
          run.push_back(to_screen(gx, gy));
        }
        flush_run();
      }

      // The current sample marker (inside the disk; outside means a stale projector — the marker
      // simply sits at the rim, still honest about the direction it names).
      if (have_projector) {
        if (const std::optional<CurrentSample> current = CurrentSampleOf(*point.adopted_result, view, valid_indices);
            current.has_value()) {
          double gx = 0.0;
          double gy = 0.0;
          SunGlobeProject(projector, current->sample.sun_in_crystal, &gx, &gy);
          const ImVec2 p = to_screen(gx, gy);
          dl->AddCircleFilled(p, UiPx(4.0f), ImGui::ColorConvertFloat4ToU32(AccentColor()));
          dl->AddCircle(p, UiPx(6.5f), IM_COL32(255, 255, 255, 210), 0, 1.5f);
        }
      }
      // (The old per-sample weight bar retired here (756): its quantity — the per-sample product —
      // is the curve region's product arm now, on the true arclength axis instead of the sample
      // index; the same window drawing it twice was a duplication, not a redundancy.)
    }
    // ---- The state area (R4's scroll set): the description, the partial note, the
    // grid-absence note, the weight note and the sample readout — all reachable by scrolling,
    // none deleted. The readout's old fits_on_line join is gone with the form: the state area is
    // where these lines live at every width. ----
    const std::optional<CurrentSample> current = CurrentSampleOf(*point.adopted_result, view, valid_indices);
    render_state_area([&] {
      DisabledWrappedText("Crystal-frame sun directions — a pose projection, not the full SO(3).");
      if (partial_document) {
        ImGui::PushStyleColor(ImGuiCol_Text, WarningTextColor());
        ImGui::TextWrapped("Partial document (%s) — the fiber on show is what arrived.",
                           ExecutionText(info->execution));
        ImGui::PopStyleColor();
      }
      if (!have_grid && view.show_deviation_grid) {
        DisabledWrappedText("(no sun grid in this document — the backdrop is absent, not zero)");
      }
      if (current.has_value()) {
        char readout[128];
        std::snprintf(readout, sizeof(readout), "sample %d/%d — zenith %.1f, az %.1f, roll %.1f%s",
                      std::clamp(view.sample_index, 0, valid_count - 1) + 1, valid_count, current->sample.zenith_deg,
                      current->sample.azimuth_deg, current->sample.roll_deg,
                      current->sample.angles_degenerate != 0 ? " (degenerate)" : "");
        DisabledWrappedText("%s", readout);
      }
    });
  };
  render_content();
  ImGui::EndChild();
  ImGui::EndChild();
}

// ---- The raypath 3D view -----------------------------------------------------------------------------

void RenderAnalyzeRay3DRegion(GuiState& state, const ImVec2& size) {
  g_ray3d_frame.valid = false;
  // The canvas and frame controls never scroll with result prose. The report scrolls
  // natively; only the actual canvas registers the orbit wheel claim.
  ImGui::BeginChild("##AnalyzeRaypath3D", size, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollWithMouse);
  PointQuerySession& point = state.analysis.point;
  RayTraceSession& trace = state.analysis.trace;
  PointViewState& view = state.analysis.point_view;
  // SameLine is a judgment, not a default (the map region's rule): the frame radios' row exceeds
  // the side-by-side cell at its arm boundary, and a clipped radio is an unreachable control.
  const auto fits_on_line = [&](float next_w) {
    const float line_right = ImGui::GetContentRegionMax().x + ImGui::GetWindowPos().x;
    return ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + next_w <= line_right;
  };

  ImGui::TextUnformatted("Raypath 3D");
  int frame = view.crystal_frame ? 1 : 0;
  const auto radio_w = [&](const char* label) {
    return ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(label).x;
  };
  if (fits_on_line(radio_w("World"))) {
    ImGui::SameLine();
  }
  ImGui::RadioButton("World##r3d", &frame, 0);
  if (fits_on_line(radio_w("Crystal"))) {
    ImGui::SameLine();
  }
  ImGui::RadioButton("Crystal##r3d", &frame, 1);
  view.crystal_frame = frame == 1;
  RenderPointStatus(point);
  const ImVec2 remaining = ImGui::GetContentRegionAvail();
  const float graph_h = CanvasHeightBeforeReport(remaining.y, 0.0f, std::max(UiPx(64.0f), remaining.y * 0.2f),
                                                 ImGui::GetStyle().ItemSpacing.y);
  const ImVec2 canvas_size(remaining.x, graph_h);
  const ImVec2 graph_min = ImGui::GetCursorScreenPos();
  ImDrawList* graph_draw_list = ImGui::GetWindowDrawList();
  bool graph_hovered = false;
  bool graph_active = false;
  if (graph_h > 0.0f) {
    ImGui::InvisibleButton("##r3d_canvas", canvas_size, ImGuiButtonFlags_MouseButtonLeft);
    graph_hovered = ImGui::IsItemHovered();
    graph_active = ImGui::IsItemActive();
    graph_draw_list->AddRectFilled(graph_min, ImVec2(graph_min.x + canvas_size.x, graph_min.y + graph_h),
                                   IM_COL32(16, 18, 22, 255));
  }
  AnalyzeRegionWheelClaim(graph_hovered, AnalyzeRay3DWheelOwnerId());
  ImGui::BeginChild("##Ray3DReport", RemainingReportSize());
  const auto render_content = [&] {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped(
        "One pose's real path in the crystal: drag orbits, wheel zooms. Same physical state, two "
        "views.");
    ImGui::PopStyleColor();

    const PointQueryState st = PointQueryStateFor(point);
    if (st == PointQueryState::kNoIntent) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
      ImGui::TextWrapped("Pick a class and a sky point; the current pose's crystal-interior path takes this region.");
      ImGui::PopStyleColor();
      return;
    }
    if (st == PointQueryState::kWaiting || st == PointQueryState::kCancelled || st == PointQueryState::kRefused) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
      switch (st) {
        case PointQueryState::kWaiting:
          ImGui::TextWrapped("The point is computing — the path follows it.");
          break;
        case PointQueryState::kCancelled:
          ImGui::TextWrapped("Point cancelled — no path on show.");
          break;
        default:
          ImGui::TextWrapped("Point refused — no path on show.");
          break;
      }
      ImGui::PopStyleColor();
      return;
    }
    const std::optional<LUMICE_PathPointInfo> info = ReadPointInfo(*point.adopted_result);
    if (!info.has_value()) {
      DisabledWrappedText("The adopted point document did not answer its Info reader.");
      return;
    }
    if (info->outcome == LUMICE_PATH_POINT_MASS) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
      ImGui::TextWrapped(
          "A rank-0 point mass has no crystal-interior path — nothing to draw here (the details carry "
          "the mass numbers).");
      ImGui::PopStyleColor();
      return;
    }

    // The partial-document note, the globe's sibling: the path on show is what a stopped run
    // delivered.
    if (info->execution != LUMICE_PATH_OUTCOME_COMPLETED) {
      ImGui::PushStyleColor(ImGuiCol_Text, WarningTextColor());
      ImGui::TextWrapped("Partial document (%s) — the path on show is what arrived.", ExecutionText(info->execution));
      ImGui::PopStyleColor();
    }
    if (info->component_count == 0) {
      DisabledWrappedText("No fiber component — no crystal-interior path to draw.");
      return;
    }
    const std::vector<int> valid_indices = CollectValidSampleIndices(
        *point.adopted_result, std::clamp(view.component_index, 0, static_cast<int>(info->component_count) - 1));
    const std::optional<CurrentSample> current = CurrentSampleOf(*point.adopted_result, view, valid_indices);
    if (!current.has_value()) {
      DisabledWrappedText("No valid sample on show — nothing to draw.");
      return;
    }
    RayTraceKey current_key;
    current_key.point_id = point.intent_id;
    current_key.component = current->component;
    current_key.sample = current->stored;
    const RayTraceDisplayState display_state = RayTraceDisplayStateFor(trace, current_key);

    // The crystal's mesh: the FROZEN binding's config (captured at DoAnalyze), in the analysis
    // geometry's nominal caliber, true scale. The pool slot comes from the snapshot's own
    // core->pool map; the mesh source is the frozen copy, so an edit of the live document after
    // Analyze cannot rebuild the mesh into a shape the trace no longer lies inside (AC4).
    const std::map<int, int> core_to_pool = AnalysisCoreToPoolMap(state);
    const auto pool_it = core_to_pool.find(info->crystal_id);
    if (pool_it == core_to_pool.end() || pool_it->second < 0) {
      DisabledWrappedText("The analyzed crystal is not in the snapshot's pool map — no mesh to draw it with.");
      return;
    }
    const int pool = pool_it->second;
    const CrystalConfig* frozen = FrozenCrystalForPool(state, pool);
    if (frozen == nullptr) {
      DisabledWrappedText("The frozen binding carries no crystal for this pool slot — no mesh to draw it with.");
      return;
    }
    CachedCrystalMesh& cache = PointMeshCache();
    const int hash = CrystalParamHash(*frozen);
    // The key moves even on a failed build: a persistently-unbuildable config is one failed
    // attempt per key change, not one LUMICE_GetCrystalMesh call per frame.
    if (cache.pool != pool || cache.hash != hash) {
      cache.pool = pool;
      cache.hash = hash;
      cache.valid = BuildPointCrystalMesh(*frozen, &cache.mesh);
    }
    if (!cache.valid || cache.mesh.vertex_count <= 0) {
      DisabledWrappedText("The crystal mesh came back empty for this configuration.");
      return;
    }
    RayTracePolyline line;
    if (display_state == RayTraceDisplayState::kDrawn &&
        !BuildRayTracePolyline(*trace.adopted_result, /*extension=*/1.1f, &line)) {
      DisabledWrappedText("The path's readers answered empty — nothing to draw.");
      return;
    }

    // The frame switch is a display transform (AC4): world view rotates the body-frame mesh to the
    // pose's world; crystal view rotates the world-frame path back. One physical state, two views.
    const double* pose = current->sample.pose;
    auto to_display = [&](const float v[3], float out[3]) {
      if (view.crystal_frame) {
        out[0] = v[0];
        out[1] = v[1];
        out[2] = v[2];
      } else {
        RotateBodyToWorld(pose, v, out);
      }
    };
    auto point_to_display = [&](const float p[3], float out[3]) {
      if (view.crystal_frame) {
        float tmp[3] = { p[0], p[1], p[2] };
        RayTracePointToCrystalFrame(pose, tmp);
        out[0] = tmp[0];
        out[1] = tmp[1];
        out[2] = tmp[2];
      } else {
        out[0] = p[0];
        out[1] = p[1];
        out[2] = p[2];
      }
    };

    // Content bounds over everything on show (mesh + trace), for the fit and the orbit centre.
    float lo[3] = { 0.0f, 0.0f, 0.0f };
    float hi[3] = { 0.0f, 0.0f, 0.0f };
    bool have_bounds = false;
    auto accumulate = [&](const float p[3]) {
      if (!have_bounds) {
        for (int k = 0; k < 3; ++k) {
          lo[k] = hi[k] = p[k];
        }
        have_bounds = true;
        return;
      }
      for (int k = 0; k < 3; ++k) {
        lo[k] = std::min(lo[k], p[k]);
        hi[k] = std::max(hi[k], p[k]);
      }
    };
    for (int v = 0; v < cache.mesh.vertex_count; ++v) {
      float d[3];
      to_display(&cache.mesh.vertices[v * 3], d);
      accumulate(d);
    }
    // The crystal's own centre, in the same display frame: the sun arrow's anchor (the crystal-frame
    // view's u direction radiates from the CRYSTAL, not from a content box the display extensions
    // inflate).
    float mesh_lo[3] = { 0.0f, 0.0f, 0.0f };
    float mesh_hi[3] = { 0.0f, 0.0f, 0.0f };
    for (int k = 0; k < 3; ++k) {
      mesh_lo[k] = mesh_hi[k] = lo[k];
    }
    {
      bool have_mesh_bounds = false;
      for (int v = 0; v < cache.mesh.vertex_count; ++v) {
        float d[3];
        to_display(&cache.mesh.vertices[v * 3], d);
        if (!have_mesh_bounds) {
          for (int k = 0; k < 3; ++k) {
            mesh_lo[k] = mesh_hi[k] = d[k];
          }
          have_mesh_bounds = true;
          continue;
        }
        for (int k = 0; k < 3; ++k) {
          mesh_lo[k] = std::min(mesh_lo[k], d[k]);
          mesh_hi[k] = std::max(mesh_hi[k], d[k]);
        }
      }
    }
    const float mesh_centre[3] = { (mesh_lo[0] + mesh_hi[0]) * 0.5f, (mesh_lo[1] + mesh_hi[1]) * 0.5f,
                                   (mesh_lo[2] + mesh_hi[2]) * 0.5f };
    for (size_t i = 0; i + 2 < line.points.size(); i += 3) {
      const float p[3] = { line.points[i], line.points[i + 1], line.points[i + 2] };
      float d[3];
      point_to_display(p, d);
      accumulate(d);
    }
    if (!have_bounds) {
      ImGui::TextDisabled("Nothing on show.");
      return;
    }
    const float centre[3] = { (lo[0] + hi[0]) * 0.5f, (lo[1] + hi[1]) * 0.5f, (lo[2] + hi[2]) * 0.5f };
    float fit = 0.0f;
    auto accumulate_radius = [&](const float p[3]) {
      const float dx = p[0] - centre[0];
      const float dy = p[1] - centre[1];
      const float dz = p[2] - centre[2];
      fit = std::max(fit, std::sqrt(dx * dx + dy * dy + dz * dz));
    };
    for (int v = 0; v < cache.mesh.vertex_count; ++v) {
      float d[3];
      to_display(&cache.mesh.vertices[v * 3], d);
      accumulate_radius(d);
    }
    for (size_t i = 0; i + 2 < line.points.size(); i += 3) {
      const float p[3] = { line.points[i], line.points[i + 1], line.points[i + 2] };
      float d[3];
      point_to_display(p, d);
      accumulate_radius(d);
    }
    fit = std::max(fit, 1e-4f);

    const ImVec2 avail = canvas_size;
    const ImVec2 canvas_min = graph_min;
    const float canvas_h = canvas_size.y;
    if (canvas_h > 0.0f && avail.x > 0.0f) {
      const ImVec2 canvas_max(canvas_min.x + avail.x, canvas_min.y + canvas_h);
      const bool active = graph_active;
      // Consume only the standing canvas claim; the report retains native wheel scrolling.
      if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        // Grab convention — the project's one drag metaphor (Render main view inside-out and Globe
        // alike, the modal preview trackball, this orbit camera; doc/geometry/coordinate-convention.md
        // §9.8): the content's near face follows the hand. Right drag → yaw decreases; down drag →
        // pitch decreases.
        view.orbit_yaw_deg -= delta.x * 0.5f;
        view.orbit_pitch_deg = std::clamp(view.orbit_pitch_deg - delta.y * 0.5f, -88.0f, 88.0f);
      }
      if (AnalyzeRegionWheelStanding() && graph_hovered && std::abs(ImGui::GetIO().MouseWheel) > 0.0f) {
        view.orbit_zoom =
            std::clamp(view.orbit_zoom * (ImGui::GetIO().MouseWheel > 0.0f ? 1.12f : 1.0f / 1.12f), 0.25f, 8.0f);
      }

      // Canvas failures must not suppress the independent status/legend report below.
      const auto render_canvas = [&] {
        PointCrystalRenderer& gl = PointRenderer();
        const ImVec2 fb_scale = ImGui::GetIO().DisplayFramebufferScale;
        const float pixel_w = std::max(1.0f, canvas_size.x * fb_scale.x);
        const float pixel_h = std::max(1.0f, canvas_size.y * fb_scale.y);
        const float downscale = std::min(1.0f, 1024.0f / std::max(pixel_w, pixel_h));
        const int width = std::max(1, static_cast<int>(std::lround(pixel_w * downscale)));
        const int height = std::max(1, static_cast<int>(std::lround(pixel_h * downscale)));
        // Init records its dimensions even on failure. Retry only on a size change, rather
        // than compiling shaders and allocating partial FBOs again on every failed frame.
        if (gl.renderer.Width() != width || gl.renderer.Height() != height) {
          gl.renderer.Destroy();
          gl.uploaded.reset();
          gl.ready = gl.renderer.Init(width, height);
          if (!gl.ready) {
            gl.renderer.Destroy();
          }
        }
        if (!gl.ready) {
          DisabledWrappedText("The crystal renderer could not initialize — no 3D canvas available.");
          return;
        }

        Ray3DFrameInfo& rendered = g_ray3d_frame;
        rendered.canvas_min = canvas_min;
        rendered.canvas_size = canvas_size;
        std::copy_n(centre, 3, rendered.centre);
        rendered.fit = fit;
        std::copy_n(pose, 9, rendered.pose);
        rendered.pool = pool;
        rendered.crystal_frame = view.crystal_frame;
        rendered.frame_count = ImGui::GetFrameCount();
        rendered.texture_id = gl.renderer.GetTextureId();
        std::array<double, 9> pose_key;
        std::copy_n(pose, 9, pose_key.begin());
        const PointMeshUploadKey upload_key{
          pool, hash, pose_key, view.crystal_frame, { centre[0], centre[1], centre[2] }, fit
        };
        if (!gl.uploaded.has_value() || *gl.uploaded != upload_key) {
          LUMICE_CrystalMesh display_mesh = cache.mesh;
          for (int v = 0; v < display_mesh.vertex_count; ++v) {
            float display[3];
            to_display(&cache.mesh.vertices[v * 3], display);
            const auto scene = ToRay3DScene(display, rendered);
            std::copy(scene.begin(), scene.end(), &display_mesh.vertices[v * 3]);
          }
          // A normal follows the display rotation, but never the centring or uniform scale.
          for (int n = 0; n < display_mesh.edge_count * 2; ++n) {
            to_display(&cache.mesh.edge_face_normals[n * 3], &display_mesh.edge_face_normals[n * 3]);
          }
          gl.renderer.UpdateMesh(display_mesh.vertices, display_mesh.vertex_count, display_mesh.edges,
                                 display_mesh.edge_count, display_mesh.triangles, display_mesh.triangle_count,
                                 display_mesh.edge_face_normals);
          gl.uploaded = upload_key;
        }
        double fwd[3];
        double right[3];
        double up[3];
        OrbitBasis(view.orbit_yaw_deg, view.orbit_pitch_deg, fwd, right, up);
        float view_rotation[16] = {};
        for (int k = 0; k < 3; ++k) {
          view_rotation[k * 4] = static_cast<float>(right[k]);
          view_rotation[k * 4 + 1] = static_cast<float>(up[k]);
          view_rotation[k * 4 + 2] = static_cast<float>(-fwd[k]);
        }
        view_rotation[15] = 1.0f;
        constexpr float kIdentity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
        // The mesh and overlay are now in a unit bounding sphere. Zoom is its focal-plane
        // half-height; compensate portrait aspect to preserve the old short-side fit.
        // At zoom=1, a focal-plane radius spans 42% of the short side, matching the old fit.
        // A positive stand-off keeps the eye outside the unit sphere at every zoom without a
        // clamp plateau; wheel magnification remains strictly monotone across the full range.
        constexpr float kFitHalfHeight = 0.5f / 0.42f;
        constexpr float kCameraStandOff = 0.3f;
        const float zoom =
            (kCameraStandOff + (kFitHalfHeight - kCameraStandOff) / std::clamp(view.orbit_zoom, 0.25f, 8.0f)) *
            std::max(1.0f, static_cast<float>(height) / width);
        gl.renderer.Render(kIdentity, zoom, CrystalStyle::kXRay, view_rotation);
        // Use the ACTUAL integer FBO dimensions on both sides. Its aspect only differs from the
        // logical canvas by pixel quantization; both texture and overlay undergo the same stretch.
        CrystalRenderer::ComputeMvp(kIdentity, zoom, width, height, rendered.mvp, view_rotation);
        rendered.valid = true;
        ImDrawList* dl = graph_draw_list;
        dl->PushClipRect(canvas_min, canvas_max, true);
        // AddImage is the Image quad without a second layout item: the existing InvisibleButton
        // remains the sole canvas interaction target, at its existing ##r3d_canvas identity.
        dl->AddImage(static_cast<ImTextureID>(rendered.texture_id), canvas_min, canvas_max, ImVec2(0, 1), ImVec2(1, 0));
        const auto project = [&](const float p[3]) { return ProjectSceneToPixel(p, rendered); };
        for (size_t s = 0; s < line.seg_kinds.size(); ++s) {
          const float* p0 = &line.points[s * 6];
          const float* p1 = p0 + 3;
          float d0[3];
          float d1[3];
          point_to_display(p0, d0);
          point_to_display(p1, d1);
          dl->AddLine(project(d0), project(d1), SegmentColor(line.seg_kinds[s], 1.0f), 2.0f);
        }
        // The contacts: one marker per REAL contact of the recovery. The polyline's point layout is
        // [entry_start, c0, c0, c1, ..., cl, exit_end] — odd indices 1..2N-1 are the N contacts, index
        // 0 the entry extension's start and the LAST index the exit extension's end, both this TU's
        // display synthesis and marked by nothing.
        for (size_t idx = 1; idx + 1 < line.points.size() / 3; idx += 2) {
          const float* p = &line.points[idx * 3];
          float d[3];
          point_to_display(p, d);
          dl->AddCircle(project(d), UiPx(2.5f), kRayTraceContactColor, 0, 1.0f);
        }
        // The crystal-frame sun direction: one short arrow along u radiating from the crystal's own
        // centre (the crystal view's own anchor; the world view's entry leg already carries the
        // incident). The tip rides the content fit's scale, the anchor does not.
        if (display_state == RayTraceDisplayState::kDrawn && view.crystal_frame) {
          const float u[3] = { static_cast<float>(current->sample.sun_in_crystal[0]),
                               static_cast<float>(current->sample.sun_in_crystal[1]),
                               static_cast<float>(current->sample.sun_in_crystal[2]) };
          const float tip[3] = { mesh_centre[0] + u[0] * fit * 0.6f, mesh_centre[1] + u[1] * fit * 0.6f,
                                 mesh_centre[2] + u[2] * fit * 0.6f };
          dl->AddLine(project(mesh_centre), project(tip), ImGui::ColorConvertFloat4ToU32(AccentColor()), 1.5f);
        }
        dl->PopClipRect();
      };
      render_canvas();
    }
    // The current pose's crystal remains visible even when its trace has no drawable path.
    switch (display_state) {
      case RayTraceDisplayState::kHidden:
        DisabledWrappedText("No representative path asked for yet.");
        return;
      case RayTraceDisplayState::kComputing:
        DisabledWrappedText("Computing the representative path for this sample…");
        return;
      case RayTraceDisplayState::kError:
        ImGui::TextUnformatted("Path query failed:");
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        ImGui::TextWrapped("%s", trace.adopted_error.c_str());
        ImGui::PopStyleColor();
        return;
      case RayTraceDisplayState::kRefused: {
        LUMICE_RepresentativeRayInfo rinfo{};
        rinfo.struct_size = sizeof(rinfo);
        LUMICE_RepresentativeRayGetInfo(trace.adopted_result.get(), &rinfo);
        ImGui::TextUnformatted("No legal path for this pose:");
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        ImGui::TextWrapped("reason: %s | entry status: %s%s%s | corridor area %.3e (threshold %.3e)",
                           PathTextToString(rinfo.reason).c_str(), PathTextToString(rinfo.entry_status).c_str(),
                           rinfo.chain_failure.length > 0 ? " | chain: " : "",
                           rinfo.chain_failure.length > 0 ? PathTextToString(rinfo.chain_failure).c_str() : "",
                           rinfo.area_perp_internal, rinfo.area_threshold);
        ImGui::PopStyleColor();
        return;
      }
      case RayTraceDisplayState::kDrawn:
        break;
    }

    // Each legend unit owns a swatch item and its label; test the whole unit before SameLine.
    const float swatch = UiPx(9.0f);
    const char* labels[] = { "entry", "internal", "exit" };
    for (int kind = 0; kind < 3; ++kind) {
      const float unit_w = swatch + ImGui::GetStyle().ItemSpacing.x + ImGui::CalcTextSize(labels[kind]).x;
      if (kind > 0 && fits_on_line(unit_w)) {
        ImGui::SameLine();
      }
      const ImVec2 pos = ImGui::GetCursorScreenPos();
      ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + swatch, pos.y + swatch), SegmentColor(kind, 1.0f));
      ImGui::Dummy(ImVec2(swatch, ImGui::GetTextLineHeight()));
      ImGui::SameLine();
      ImGui::TextDisabled("%s", labels[kind]);
    }
    if (current->sample.interface_count >= 2) {
      char transmittance[96];
      std::snprintf(transmittance, sizeof(transmittance), "face transmittance: entry %.3f, exit %.3f",
                    current->sample.interface_transmittances[0],
                    current->sample.interface_transmittances[current->sample.interface_count - 1]);
      if (fits_on_line(ImGui::CalcTextSize(transmittance).x)) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", transmittance);
      } else {
        ImGui::NewLine();  // end the labels' line — the wrapped readout owns a fresh line, not the tail
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        ImGui::TextWrapped("%s", transmittance);
        ImGui::PopStyleColor();
      }
    }
  };
  render_content();
  ImGui::EndChild();
  ImGui::EndChild();
}

// ---- The Pose & trace window (contract §7) -----------------------------------------------------------

namespace {

// The curve's arm count: one authoritative constant for the whole Pose & trace section (legend,
// per-arm readouts, canvas series and the per-arm colors all index by it). It lives at namespace
// scope — a function-local constexpr used as an array bound inside a nested lambda reads as a
// captured runtime value to MSVC (C2131), while clang folds it, so the two compilers disagree
// unless the constant is unambiguously namespace-scope.
constexpr int kCurveArms = 5;

// D4 — the Pose & trace window's vertical strip between the 3D region and the right column
// (contract §4.2): the D3 splitter's recipe (invisible button + hover/active line + the one
// affordance line + a delta-only write from the DISPLAYED boundary — never from the raw stored
// value, whose first-stretch crawl through the clamp band the workspace splitter comment
// records). Dragging right widens the 3D region (the right column's share shrinks), matching
// D1/D3's direction convention. `shown_right_w` is the frame's final derived width (the same
// value the layout spent), `denom_w` the denominator the stored ratio divides.
void PoseViewColumnSplitter(const char* id, float height, float shown_right_w, float* ratio, float denom_w,
                            float right_floor_px, float left_floor_px) {
  const float width = UiPx(kAnalyzeSplitterSize);
  ImGui::InvisibleButton(id, ImVec2(width, height));
  const bool hovered = ImGui::IsItemHovered();
  const bool active = ImGui::IsItemActive();
  const float mouse_dx = ImGui::GetIO().MouseDelta.x;
  if (active && mouse_dx != 0.0f && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    const float new_right =
        std::clamp(shown_right_w - mouse_dx, right_floor_px, std::max(right_floor_px, denom_w - left_floor_px));
    *ratio = new_right / std::max(1.0f, denom_w);
  }
  if (hovered || active) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
  }
  const ImU32 line_col = ImGui::GetColorU32(active  ? ImGuiCol_SeparatorActive :
                                            hovered ? ImGuiCol_SeparatorHovered :
                                                      ImGuiCol_Separator);
  ImGui::GetWindowDrawList()->AddLine(ImVec2(ImGui::GetItemRectMin().x + width * 0.5f, ImGui::GetItemRectMin().y),
                                      ImVec2(ImGui::GetItemRectMin().x + width * 0.5f, ImGui::GetItemRectMax().y),
                                      line_col, 1.0f);
}

// D5 — the right column's horizontal strip between the globe region and the factors region:
// the row splitter's recipe and direction (dragging down grows the globe, the
// MiddleRowsSplitter's convention), the same delta-only write from the displayed boundary.
// Independent of D4 by construction — the two splitters write different fields and drag
// neither's geometry (§4.2's 独立不联动).
void PoseViewRowSplitter(const char* id, float width, float shown_globe_h, float* ratio, float denom_h,
                         float globe_floor_px, float factors_floor_px) {
  const float height = UiPx(kAnalyzeSplitterSize);
  ImGui::InvisibleButton(id, ImVec2(width, height));
  const bool hovered = ImGui::IsItemHovered();
  const bool active = ImGui::IsItemActive();
  const float mouse_dy = ImGui::GetIO().MouseDelta.y;
  if (active && mouse_dy != 0.0f && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    const float new_globe =
        std::clamp(shown_globe_h + mouse_dy, globe_floor_px, std::max(globe_floor_px, denom_h - factors_floor_px));
    *ratio = new_globe / std::max(1.0f, denom_h);
  }
  if (hovered || active) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
  }
  const ImU32 line_col = ImGui::GetColorU32(active  ? ImGuiCol_SeparatorActive :
                                            hovered ? ImGuiCol_SeparatorHovered :
                                                      ImGuiCol_Separator);
  ImGui::GetWindowDrawList()->AddLine(ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMin().y + height * 0.5f),
                                      ImVec2(ImGui::GetItemRectMax().x, ImGui::GetItemRectMin().y + height * 0.5f),
                                      line_col, 1.0f);
}

// The contribution-curve region (contract §7's 右列下, 756): the five named factors and their
// product along the SELECTED COMPONENT's real SO(3) arclength — one shared log axis, the
// engine's own numbers (no recomputation, no per-arm normalization), each arm honest in the
// audit §3 three-state sense. Fixed stack: the title (identity + span + integral echo), the
// caliber line (angle-Lebesgue, rad — the J7 constant, never inferred), the legend, the canvas
// (five polylines broken at unavailable points, the zero row for exact zeros, the seed mark,
// the cursor), the readout line (the current sample; the hovered sample overrides while it
// lasts). Below: the region's own scroll — the partial note, the measure registration's note
// verbatim when its gap flag stands (J7), and the current sample's full factor readouts (the
// factors region's face, retained in-window). Every pre-result state is the honest ladder the
// factors region held — a line naming where the answer lives, never a painted zero. Click or
// drag on the canvas moves the SAME selection the slider, globe and 3D trace follow
// (view.sample_index) and stops the playback; the sky-point intent is untouched by construction.
void RenderPoseCurveRegion(GuiState& state, const ImVec2& size) {
  ImGui::BeginChild("##AnalyzePoseCurve", size, ImGuiChildFlags_Borders);
  PointQuerySession& point = state.analysis.point;
  PointViewState& view = state.analysis.point_view;
  const auto disabled_line = [](const char* fmt, auto... args) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped(fmt, args...);
    ImGui::PopStyleColor();
  };
  ImGui::SeparatorText("Contribution curve");

  RenderPointStatus(point);
  const ImVec2 available = ImGui::GetContentRegionAvail();
  const float graph_h = CanvasHeightBeforeReport(available.y, 0.0f, std::max(UiPx(96.0f), available.y * 0.4f),
                                                 ImGui::GetStyle().ItemSpacing.y);
  const ImVec2 graph_size(available.x, graph_h);
  const ImVec2 graph_min = ImGui::GetCursorScreenPos();
  ImDrawList* graph_draw_list = ImGui::GetWindowDrawList();
  bool graph_hovered = false;
  bool graph_active = false;
  if (graph_h > 0.0f) {
    ImGui::InvisibleButton("##pose_curve_canvas", graph_size, ImGuiButtonFlags_MouseButtonLeft);
    graph_hovered = ImGui::IsItemHovered();
    graph_active = ImGui::IsItemActive();
    graph_draw_list->AddRectFilled(graph_min, ImVec2(graph_min.x + graph_size.x, graph_min.y + graph_h),
                                   IM_COL32(16, 18, 22, 255));
  }
  AnalyzeRegionWheelStanding();  // latch before this region writes its claim
  AnalyzeRegionWheelClaim(graph_hovered, AnalyzeCurveWheelOwnerId());
  ImGui::BeginChild("##PoseCurveScroll", RemainingReportSize());
  const auto render_content = [&] {
    const PointQueryState st = PointQueryStateFor(point);
    if (st == PointQueryState::kNoIntent) {
      disabled_line("The contribution curve takes this region once a point result is in.");
      return;
    }
    if (st == PointQueryState::kWaiting) {
      disabled_line("The point is computing — the curve follows it.");
      return;
    }
    if (st == PointQueryState::kCancelled) {
      disabled_line("Point cancelled — the curve waits for a new query.");
      return;
    }
    if (st == PointQueryState::kRefused) {
      disabled_line("Point refused — the curve waits for a supported target.");
      return;
    }
    const std::optional<LUMICE_PathPointInfo> info = ReadPointInfo(*point.adopted_result);
    if (!info.has_value()) {
      DisabledWrappedText("The adopted point document did not answer its Info reader.");
      return;
    }
    if (info->outcome == LUMICE_PATH_POINT_MASS) {
      disabled_line("A rank-0 point mass has no fiber curve — the details carry the mass numbers.");
      return;
    }
    std::vector<PointComponentRow> rows;
    const int component_total = PointComponentRows(*point.adopted_result, &rows);
    if (component_total == 0) {
      LUMICE_PathPointReach reach{};
      reach.struct_size = sizeof(reach);
      const bool have_reach = LUMICE_PathPointGetReach(point.adopted_result.get(), &reach) == LUMICE_PATH_READ_OK;
      if (info->execution != LUMICE_PATH_OUTCOME_COMPLETED) {
        ImGui::PushStyleColor(ImGuiCol_Text, WarningTextColor());
        ImGui::TextWrapped(
            "Partial document (%s) — what arrived is on show; emptiness here is the run's "
            "stop, not a physical absence.",
            ExecutionText(info->execution));
        ImGui::PopStyleColor();
      }
      if (have_reach && !reach.target_in_range) {
        disabled_line("No fiber here: the target's deviation is outside the range of this path's deflection field.");
      } else {
        disabled_line("No fiber component at this point — nothing to draw a curve along.");
      }
      return;
    }
    const int component = std::clamp(view.component_index, 0, component_total - 1);
    PointContributionCurve curve;
    BuildPointContributionCurve(*point.adopted_result, component, &curve);
    if (!curve.ok || curve.valid_positions.empty()) {
      if (info->execution != LUMICE_PATH_OUTCOME_COMPLETED) {
        ImGui::PushStyleColor(ImGuiCol_Text, WarningTextColor());
        ImGui::TextWrapped("Partial document (%s) — the curve on show is what arrived.",
                           ExecutionText(info->execution));
        ImGui::PopStyleColor();
      }
      disabled_line("This component carries no valid sample — nothing to draw a curve along.");
      return;
    }
    const int valid_count = static_cast<int>(curve.valid_positions.size());
    const size_t n = curve.points.size();

    // ---- The fixed stack's title: identity, span, integral echo — the component's own words,
    // the completeness named where it is incomplete. ----
    const double span_rad = n > 0 ? curve.points[n - 1].x_rad : 0.0;
    // The echo slot: the engine's integral answer and its completeness pair — "incomplete" named
    // where the counts differ (available_points counts points, not segments — audit §1; the GUI
    // echoes, it never renormalizes).
    char echo[80];
    if (curve.integral_available) {
      std::snprintf(echo, sizeof(echo), "int %.3g (%d/%d)%s", curve.fiber_integral, curve.available_points,
                    curve.total_points, curve.available_points < curve.total_points ? " · incomplete" : "");
    } else {
      std::snprintf(echo, sizeof(echo), "int unavailable (%d/%d)%s", curve.available_points, curve.total_points,
                    curve.available_points < curve.total_points ? " · incomplete" : "");
    }
    char title[192];
    if (curve.seed_index == 0) {
      // Closed loop: the axis zero IS the seed (first and last poses coincide — audit §5.1).
      std::snprintf(title, sizeof(title), "%s · %d pts · loop %.3f rad · %s", curve.kind.c_str(), static_cast<int>(n),
                    span_rad, echo);
    } else {
      // Arc: the zero sits at the backward far-end boundary event; the seed rides mid-curve.
      const double seed_x = curve.seed_index >= 0 && curve.seed_index < static_cast<int>(n) ?
                                curve.points[static_cast<size_t>(curve.seed_index)].x_rad :
                                0.0;
      std::snprintf(title, sizeof(title), "%s · %d pts · arc %.3f rad · seed %.3f · %s", curve.kind.c_str(),
                    static_cast<int>(n), span_rad, seed_x, echo);
    }
    disabled_line("%s", title);
    // J7's constant: the caliber is DECLARED, never inferred — on plate-family scenes rho (and
    // the product and the integral with it) reads identically zero in this convention, while the
    // other arms stay informative per their own flags (audit §4).
    disabled_line("angle-Lebesgue · arclength axis in rad");

    // ---- The legend: one swatch + short symbol per arm, the units in the factor readouts' own
    // vocabulary; an arm with no positive value names the fact (J4's legend naming). SameLine is
    // a judgment (the 3D legend's rule): an entry wraps below when it no longer fits. ----
    const ImU32 arm_colors[kCurveArms] = {
      IM_COL32(96, 165, 250, 255),   // rho — blue
      IM_COL32(251, 146, 60, 255),   // A/a^2 — orange
      IM_COL32(74, 222, 128, 255),   // T — green
      IM_COL32(232, 121, 249, 255),  // K — fuchsia
      IM_COL32(250, 250, 250, 255),  // product — the headline quantity, brightest
    };
    const char* arm_labels[kCurveArms] = { "rho rad^-3", "A/a^2", "T total", "K 1/(J+eps)", "rho*A/a^2*T*K" };
    const auto value_of = [&](int arm, size_t i) -> double { return curve.points[i].*kCurveArmFields[arm].value; };
    const auto kind_of = [&](int arm, size_t i) { return curve.points[i].*kCurveArmFields[arm].kind; };
    int positive_seen[kCurveArms] = { 0, 0, 0, 0, 0 };
    int zero_seen[kCurveArms] = { 0, 0, 0, 0, 0 };
    for (size_t i = 0; i < n; ++i) {
      for (int arm = 0; arm < kCurveArms; ++arm) {
        if (kind_of(arm, i) == PointCurveValueKind::kPositive) {
          ++positive_seen[arm];
        } else if (kind_of(arm, i) == PointCurveValueKind::kExactZero) {
          ++zero_seen[arm];
        }
      }
    }
    {
      const float swatch = UiPx(9.0f);
      // The fit is judged where the entry would LAND — the previous entry's text rect end (the
      // cursor has already dropped to the next line by then; a cursor-position check would read
      // the fresh line's left edge and glue every entry onto one overflowing row).
      const float region_max_x = ImGui::GetWindowPos().x + ImGui::GetContentRegionMax().x;
      for (int arm = 0; arm < kCurveArms; ++arm) {
        std::string label = arm_labels[arm];
        if (positive_seen[arm] == 0) {
          label += zero_seen[arm] > 0 ? " (0 throughout)" : " (unavailable)";
        }
        const float text_w = ImGui::CalcTextSize(label.c_str()).x;
        bool fits = false;
        if (arm > 0) {
          const float prev_end = ImGui::GetItemRectMax().x;  // the previous entry's text, screen coords
          fits = prev_end + ImGui::GetStyle().ItemSpacing.x + swatch + ImGui::GetStyle().ItemSpacing.x + text_w <=
                 region_max_x;
        }
        if (arm > 0 && fits) {
          ImGui::SameLine();
        }
        const ImVec2 sp = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(sp, ImVec2(sp.x + swatch, sp.y + swatch), arm_colors[arm]);
        ImGui::Dummy(ImVec2(swatch, swatch));
        ImGui::SameLine();
        ImGui::TextUnformatted(label.c_str());
      }
    }

    // ---- The readout's words: one arm's value at its honest state — the number, "0" for the
    // exact zero, "n/a" for unavailable. The curve holds the engine's number; this only spells it.
    const auto arm_readout = [&](int arm, size_t i) -> std::string {
      const PointCurveValueKind kind = kind_of(arm, i);
      if (kind == PointCurveValueKind::kUnavailable) {
        return "n/a";
      }
      if (kind == PointCurveValueKind::kExactZero) {
        return "0";
      }
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%.3g", value_of(arm, i));
      return buf;
    };
    const auto sample_readout = [&](int position) -> std::string {
      const int stored = PointCurveStoredAt(curve, position);
      if (stored < 0) {
        return std::string();
      }
      const PointCurvePoint& p = curve.points[static_cast<size_t>(stored)];
      char head[64];
      std::snprintf(head, sizeof(head), "sample %d/%d @ %.3f rad", position + 1, valid_count, p.x_rad);
      std::string out = head;
      const char* names[kCurveArms] = { "rho", "A/a^2", "T", "K", "product" };
      for (int arm = 0; arm < kCurveArms; ++arm) {
        out += " · ";
        out += names[arm];
        out += " ";
        out += arm_readout(arm, static_cast<size_t>(stored));
      }
      return out;
    };

    const ImVec2 avail = graph_size;
    int hover_position = -1;
    if (graph_size.y > 0.0f) {
      const float canvas_h = graph_size.y;
      ImDrawList* dl = graph_draw_list;
      const ImVec2 canvas_min = graph_min;
      const ImVec2 canvas_max(canvas_min.x + avail.x, canvas_min.y + canvas_h);
      const bool canvas_hovered = graph_hovered;
      const bool canvas_active = graph_active;
      const double x_hi = n > 0 ? curve.points[n - 1].x_rad : 0.0;
      // One shared fit over all positive arms, followed by the session's log-domain window.
      const PointCurveAxisRange fit = PointCurveAxisFit(curve);
      const float line_h = ImGui::GetTextLineHeight();
      const float summary_wrap = std::max(1.0f, avail.x - UiPx(12.0f));
      const auto summary_for = [&](const PointCurveAxisRange& range) {
        return range.valid ? PointCurveOutsideLabel(PointCurveCountOutside(curve, range.lo, range.hi)) : "";
      };
      const auto footer_height = [&](const std::string& text) {
        return std::max(UiPx(28.0f), UiPx(16.0f) + ImGui::CalcTextSize(text.c_str(), nullptr, false, summary_wrap).y);
      };
      const PointCurveAxisRange before_wheel = PointCurveVisibleRange(fit, &view.curve_axis);
      const std::string before_summary = summary_for(before_wheel);
      const float before_plot_h = std::max(1.0f, canvas_h - UiPx(10.0f) - footer_height(before_summary));
      const double mouse_anchor = 1.0 - (ImGui::GetIO().MousePos.y - canvas_min.y - UiPx(10.0f)) / before_plot_h;
      const double anchor = mouse_anchor >= 0.0 && mouse_anchor <= 1.0 ? mouse_anchor : 0.5;
      const double wheel = AnalyzeRegionWheelStanding() && canvas_hovered ? ImGui::GetIO().MouseWheel : 0.0;
      const PointCurveAxisRange visible = PointCurveVisibleRange(fit, &view.curve_axis, wheel, anchor);
      const double v_min = visible.lo;
      const double v_max = visible.hi;
      const bool have_range = visible.valid && x_hi > 0.0;
      const std::string outside_label = wheel == 0.0 ? before_summary : summary_for(visible);
      const float pad_b = footer_height(outside_label);
      const float rough_plot_h = std::max(1.0f, canvas_h - UiPx(10.0f) - pad_b);
      const int label_step = have_range ? PointCurveLogLabelStep(v_min, v_max, rough_plot_h, line_h) : 1;
      float widest_lab = 0.0f;
      if (have_range) {
        for (const PointCurveLogTick& tick : PointCurveLogTicks(v_min, v_max)) {
          const int exp10 = static_cast<int>(std::lround(std::log10(tick.value)));
          if (!tick.major || exp10 % label_step != 0) {
            continue;
          }
          char lab[16];
          std::snprintf(lab, sizeof(lab), "%.0e", tick.value);
          widest_lab = std::max(widest_lab, ImGui::CalcTextSize(lab).x);
        }
      }
      const float pad_l = std::max(UiPx(34.0f), widest_lab + UiPx(10.0f));  // decade labels live left of the plot
      const float pad_r = UiPx(10.0f);
      const float pad_t = UiPx(10.0f);  // the seed mark rides above the plot
      // The footer leaves the zero row intact and wraps the summary inside the same canvas.
      const ImVec2 plot_min(canvas_min.x + pad_l, canvas_min.y + pad_t);
      const ImVec2 plot_max(canvas_max.x - pad_r, canvas_max.y - pad_b);
      const float plot_w = std::max(1.0f, plot_max.x - plot_min.x);
      const float plot_h = std::max(1.0f, plot_max.y - plot_min.y);
      const auto x_to_px = [&](double x) {
        const double t = x_hi > 0.0 ? x / x_hi : 0.0;
        return plot_min.x + static_cast<float>(t * plot_w);
      };
      const auto x_at_px = [&](float px) {
        const float t = std::clamp((px - plot_min.x) / plot_w, 0.0f, 1.0f);
        return static_cast<double>(t) * x_hi;
      };

      dl->AddRectFilled(canvas_min, canvas_max, IM_COL32(16, 18, 22, 255));
      dl->AddRect(canvas_min, canvas_max, IM_COL32(70, 74, 82, 160));

      const auto v_to_py = [&](double v) {
        const double log_lo = have_range ? std::log10(v_min) : 0.0;
        double log_hi = have_range ? std::log10(v_max) : 0.0;
        if (log_hi <= log_lo) {
          log_hi = log_lo + 1.0;  // a single decade, so one value draws inside a range
        }
        const double t = (std::log10(v) - log_lo) / (log_hi - log_lo);
        return plot_min.y + static_cast<float>((1.0 - std::clamp(t, 0.0, 1.0)) * plot_h);
      };
      if (have_range) {
        const bool minors_fit =
            label_step == 1 && plot_h / std::max(1.0, std::log10(v_max) - std::log10(v_min)) >= UiPx(12.0f);
        for (const PointCurveLogTick& tick : PointCurveLogTicks(v_min, v_max)) {
          const int exp10 = static_cast<int>(std::lround(std::log10(tick.value)));
          const bool labelled = tick.major && exp10 % label_step == 0;
          if (tick.major && !labelled) {
            continue;  // an unlabelled decade would only crowd the plot at this density
          }
          const float py = v_to_py(tick.value);
          if (tick.major) {
            dl->AddLine(ImVec2(plot_min.x, py), ImVec2(plot_max.x, py), IM_COL32(60, 64, 72, 110), 1.0f);
            char lab[16];
            std::snprintf(lab, sizeof(lab), "%.0e", tick.value);
            const ImVec2 ts = ImGui::CalcTextSize(lab);
            dl->AddText(ImVec2(plot_min.x - ts.x - UiPx(5.0f), py - ts.y * 0.5f),
                        ImGui::GetColorU32(ImGuiCol_TextDisabled), lab);
          } else if (minors_fit) {
            dl->AddLine(ImVec2(plot_min.x - UiPx(3.0f), py), ImVec2(plot_min.x, py), IM_COL32(90, 94, 102, 140), 1.0f);
          }
        }
      }
      // Fit the honest tail summary beside the x labels, leaving the zero markers untouched.
      const ImVec2 summary_size = ImGui::CalcTextSize(outside_label.c_str(), nullptr, false, summary_wrap);
      const float summary_x = canvas_max.x - UiPx(6.0f) - summary_size.x;
      if (!outside_label.empty()) {
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(summary_x, plot_max.y + UiPx(13.0f)),
                    ImGui::GetColorU32(ImGuiCol_TextDisabled), outside_label.c_str(), nullptr, summary_wrap);
      }
      // The plot's frame, the x labels and the axis unit (the caliber line names rad; the axis
      // shows where its numbers sit). The summary owns its strip; omit colliding x labels.
      const auto x_label = [&](float x, float y, const char* text) {
        if (outside_label.empty() || x + ImGui::CalcTextSize(text).x + UiPx(5.0f) < summary_x) {
          dl->AddText(ImVec2(x, y), ImGui::GetColorU32(ImGuiCol_TextDisabled), text);
        }
      };
      dl->AddRect(plot_min, plot_max, IM_COL32(90, 94, 102, 120), 0.0f, 0, 1.0f);
      if (x_hi > 0.0) {
        char lab[24];
        const char* fmt = x_hi < 10.0 ? "%.2f" : (x_hi < 100.0 ? "%.1f" : "%.0f");
        const float axis_lab_y = plot_max.y + UiPx(13.0f);
        std::snprintf(lab, sizeof(lab), fmt, 0.0);
        x_label(plot_min.x - ImGui::CalcTextSize(lab).x * 0.5f, axis_lab_y, lab);
        std::snprintf(lab, sizeof(lab), fmt, x_hi * 0.5);
        x_label(x_to_px(x_hi * 0.5) - ImGui::CalcTextSize(lab).x * 0.5f, axis_lab_y, lab);
        std::snprintf(lab, sizeof(lab), fmt, x_hi);
        x_label(plot_max.x - ImGui::CalcTextSize(lab).x, axis_lab_y, lab);
      }
      {
        const char* unit = "rad";
        dl->AddText(ImVec2(canvas_max.x - ImGui::CalcTextSize(unit).x - UiPx(6.0f), canvas_min.y + UiPx(4.0f)),
                    ImGui::GetColorU32(ImGuiCol_TextDisabled), unit);
      }

      // The zero row: the exact zeros sit here, in their arm's colour — never mapped onto the
      // log axis, never patched with an epsilon (J4).
      if (x_hi > 0.0) {
        const float zero_y = plot_max.y + UiPx(5.0f);
        dl->AddLine(ImVec2(plot_min.x, zero_y), ImVec2(plot_max.x, zero_y), IM_COL32(60, 64, 72, 90), 1.0f);
        for (int arm = 0; arm < kCurveArms; ++arm) {
          for (size_t i = 0; i < n; ++i) {
            if (kind_of(arm, i) != PointCurveValueKind::kExactZero) {
              continue;
            }
            const float px = x_to_px(curve.points[i].x_rad);
            dl->AddRectFilled(ImVec2(px - UiPx(1.5f), zero_y - UiPx(2.0f)),
                              ImVec2(px + UiPx(1.5f), zero_y + UiPx(2.0f)), arm_colors[arm]);
          }
        }
        // The seed mark: the title names its arclength; the canvas shows where it sits (an arc's
        // seed rides mid-curve — its axis zero is the backward far end, not the seed).
        if (curve.seed_index > 0 && curve.seed_index < static_cast<int>(n)) {
          const float seed_px = x_to_px(curve.points[static_cast<size_t>(curve.seed_index)].x_rad);
          const ImU32 accent = ImGui::ColorConvertFloat4ToU32(AccentColor());
          dl->AddTriangleFilled(ImVec2(seed_px - UiPx(4.0f), plot_min.y), ImVec2(seed_px + UiPx(4.0f), plot_min.y),
                                ImVec2(seed_px, plot_min.y + UiPx(5.0f)), accent);
          dl->AddText(ImVec2(seed_px + UiPx(5.0f), plot_min.y), ImGui::GetColorU32(ImGuiCol_TextDisabled), "seed");
        }
        // The five arms: drawn run by run over PointCurvePositiveRuns — unavailable gaps and
        // exact-zero stretches break a run and are never bridged (AC3), and a run of ONE point
        // is an isolated positive contribution drawn as a marker, never dropped (AC6: discrete
        // data stays visible while the legend keeps claiming the arm).
        for (int arm = 0; arm < kCurveArms; ++arm) {
          std::vector<PointCurveValueKind> kinds(static_cast<size_t>(n));
          std::vector<double> values(n);
          for (size_t i = 0; i < n; ++i) {
            kinds[i] = kind_of(arm, i);
            values[i] = value_of(arm, i);
          }
          const float thickness = arm == 4 ? 2.0f : 1.5f;
          for (const PointCurveRun& run : PointCurvePositiveRuns(kinds.data(), n, values.data(), v_min, v_max)) {
            if (run.end - run.begin >= 2) {
              std::vector<ImVec2> line;
              line.reserve(static_cast<size_t>(run.end - run.begin));
              for (int i = run.begin; i < run.end; ++i) {
                line.push_back(ImVec2(x_to_px(curve.points[static_cast<size_t>(i)].x_rad),
                                      v_to_py(value_of(arm, static_cast<size_t>(i)))));
              }
              dl->AddPolyline(line.data(), static_cast<int>(line.size()), arm_colors[arm], 0, thickness);
            } else {
              const ImVec2 p(x_to_px(curve.points[static_cast<size_t>(run.begin)].x_rad),
                             v_to_py(value_of(arm, static_cast<size_t>(run.begin))));
              dl->AddCircleFilled(p, UiPx(2.5f), arm_colors[arm]);
            }
          }
        }
        // The cursor: the CURRENT sample's arclength — the same selection the slider, the globe
        // marker and the 3D trace follow (AC4's one selection).
        const int cursor_stored = PointCurveStoredAt(curve, std::clamp(view.sample_index, 0, valid_count - 1));
        if (cursor_stored >= 0) {
          const float cpx = x_to_px(curve.points[static_cast<size_t>(cursor_stored)].x_rad);
          dl->AddLine(ImVec2(cpx, plot_min.y), ImVec2(cpx, plot_max.y), ImGui::ColorConvertFloat4ToU32(AccentColor()),
                      1.5f);
        }
      } else {
        const char* note = "no positive value on this component — the log axis has no range";
        const ImVec2 ts = ImGui::CalcTextSize(note);
        dl->AddText(ImVec2(canvas_min.x + (avail.x - ts.x) * 0.5f, canvas_min.y + (canvas_h - ts.y) * 0.5f),
                    ImGui::GetColorU32(ImGuiCol_TextDisabled), note);
      }

      // The canvas writes the ONE selection: click or drag maps the mouse's x to the nearest
      // VALID sample (invalid points are unreachable, the slider's domain) and stops the
      // playback — the slider's own write discipline, no second cursor state (J3).
      if (x_hi > 0.0) {
        if (canvas_hovered) {
          hover_position = PointCurveValidPositionNearX(curve, x_at_px(ImGui::GetIO().MousePos.x));
        }
        const auto write_cursor_from_mouse = [&]() {
          const int v = PointCurveValidPositionNearX(curve, x_at_px(ImGui::GetIO().MousePos.x));
          if (v < 0) {
            return;
          }
          // A resolved click stops the playback even when the index does not change — clicking
          // the pose already on screen must pin it where it is, not let playback run on (J3).
          if (v != view.sample_index) {
            view.sample_index = v;
          }
          view.playing = false;
        };
        if ((canvas_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) ||
            (canvas_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left))) {
          write_cursor_from_mouse();
        }
      }
      // The readout line: the hovered sample while the hand is on the canvas, otherwise the
      // current one — a real ImGui text row (wraps, is test-addressable; not a drawlist painting).
      if (hover_position >= 0) {
        ImGui::TextWrapped("hover: %s", sample_readout(hover_position).c_str());
      } else {
        ImGui::TextWrapped("%s", sample_readout(std::clamp(view.sample_index, 0, valid_count - 1)).c_str());
      }
      // ---- The scroll region: the partial note, the measure registration's note verbatim when
      // the engine's gap flag stands (J7 — the GUI never paraphrases the support semantics), and
      // the current sample's full factor readouts (the factors face, retained in-window). ----
      if (info->execution != LUMICE_PATH_OUTCOME_COMPLETED) {
        ImGui::PushStyleColor(ImGuiCol_Text, WarningTextColor());
        ImGui::TextWrapped("Partial document (%s) — the curve on show is what arrived.",
                           ExecutionText(info->execution));
        ImGui::PopStyleColor();
      }
      LUMICE_PathMeasureRegistration reg{};
      reg.struct_size = sizeof(reg);
      if (LUMICE_PathPointGetMeasureRegistration(point.adopted_result.get(), &reg) == LUMICE_PATH_READ_OK &&
          reg.registered_gap != 0 && reg.note.data != nullptr && reg.note.length > 0) {
        disabled_line("measure registration: %.*s", static_cast<int>(reg.note.length), reg.note.data);
      }
      if (const std::optional<CurrentSample> current =
              CurrentSampleOf(*point.adopted_result, view, curve.valid_positions);
          current.has_value()) {
        disabled_line("Current sample:");
        for (const std::string& line_text : PointFactorLines(current->sample)) {
          ImGui::Bullet();
          ImGui::TextWrapped("%s", line_text.c_str());
        }
      }
      return;
    }
    // The degraded form: no canvas — the readout and the lines render direct in the region, the
    // region's own scrollbar carries them (the content budget's answer at a cell too short for
    // the two-segment form).
    if (info->execution != LUMICE_PATH_OUTCOME_COMPLETED) {
      ImGui::PushStyleColor(ImGuiCol_Text, WarningTextColor());
      ImGui::TextWrapped("Partial document (%s) — the curve on show is what arrived.", ExecutionText(info->execution));
      ImGui::PopStyleColor();
    }
    ImGui::TextWrapped("%s", sample_readout(std::clamp(view.sample_index, 0, valid_count - 1)).c_str());
    LUMICE_PathMeasureRegistration reg{};
    reg.struct_size = sizeof(reg);
    if (LUMICE_PathPointGetMeasureRegistration(point.adopted_result.get(), &reg) == LUMICE_PATH_READ_OK &&
        reg.registered_gap != 0 && reg.note.data != nullptr && reg.note.length > 0) {
      disabled_line("measure registration: %.*s", static_cast<int>(reg.note.length), reg.note.data);
    }
    if (const std::optional<CurrentSample> current =
            CurrentSampleOf(*point.adopted_result, view, curve.valid_positions);
        current.has_value()) {
      disabled_line("Current sample:");
      for (const std::string& line_text : PointFactorLines(current->sample)) {
        ImGui::Bullet();
        ImGui::TextWrapped("%s", line_text.c_str());
      }
    }
  };
  render_content();
  ImGui::EndChild();
  ImGui::EndChild();
}

}  // namespace

void RenderAnalyzePoseViewWindow(GuiState& state) {
  if (!state.analyze_pose_view_open) {
    return;
  }
  // The pumps run at RenderWorkspacePanels' entry (every GUI frame — windows open or not); this
  // window keeps only its own wait-feedback hygiene: on the Render-workspace frames it hosts
  // the fiber region (the sole writer of point_wait_line_fiber), so a field's content is
  // exactly what the user saw THIS frame — the site that can draw it owns the clear. The
  // details field's writer is the main window's diagnostics block (an Analyze-frame-only face);
  // on the Render frames where the pose window is its own host the details field simply stays
  // at its last Analyze-frame value — stale-honest, the same rule every window-closed field
  // follows — and this window does not touch it. On an Analyze frame the workspace entry has
  // already pumped and cleared; the globe region below is then that frame's only fiber-field
  // writer.
  if (state.workspace.mode != GuiState::WorkspaceMode::kAnalyze) {
    state.analysis.wait_feedback_texts.point_wait_line_fiber.clear();
  }
  // The Raypath view window's size recipe (755.4, verbatim): FirstUseEver at the contract's
  // suggested default, re-applied on a UI-scale change (WindowResizeCondForScale, theme.hpp);
  // the default clamped to the viewport — at the 2x scale the suggested 960x720 pt is
  // 1920x1440 px, past the shortest supported window's own height. The window-level minimum
  // re-clamps the live size at EVERY Begin, so the frames of the user's own border drag cannot
  // carry the window below the narrowest layout the split floors and the wrap ladder were
  // verified at (the minimum rose with the curve region — the D5 column's two full forms; the
  // default keeps the D5 clamp band a usable drag range above that).
  static float s_sized_for_scale = 0.0f;
  const ImGuiCond size_cond = WindowResizeCondForScale(s_sized_for_scale, ImGuiCond_FirstUseEver);
  const ImVec2 want_size = ImVec2(UiPx(960.0f), UiPx(720.0f));
  const ImVec2 vp_size = ImGui::GetMainViewport()->Size;
  const ImVec2 min_size =
      ImVec2(std::min(UiPx(kPoseViewMinWidth), vp_size.x), std::min(UiPx(kPoseViewMinHeight), vp_size.y));
  ImGui::SetNextWindowSizeConstraints(min_size, ImVec2(FLT_MAX, FLT_MAX));
  ImGui::SetNextWindowSize(ImVec2(std::min(want_size.x, vp_size.x), std::min(want_size.y, vp_size.y)), size_cond);
  // NoDocking: a free-form surface, not shell layout (§7's boundary note); NoCollapse: a
  // collapsed title bar hides the pose faces without freeing their queries — the honest off
  // state is closed, via the same flag the title bar's X writes through `p_open`.
  if (!ImGui::Begin("Pose & trace", &state.analyze_pose_view_open,
                    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoScrollWithMouse)) {
    ImGui::End();
    return;
  }
  // ---- D4 — the 3D | right-column split (contract §4.2), the Raypath view's D3 recipe: the
  // stored ratio is the split's identity (manual priority, §9.3 — no query result or selection
  // ever writes it); the frame derives pixels and clamps BOTH sides every frame — the right
  // column at its floor AND the 3D region at its own (either side may be the one pushed toward
  // its bound by a drag to an end or a resize under a kept ratio); only when the two floors
  // cannot share the span does the ③ proportional fallback apply. ----
  const float splitter_w = UiPx(kAnalyzeSplitterSize);
  const ImVec2 avail = VisibleContentSize();
  const float avail_w = std::max(0.0f, avail.x);
  const float right_floor = UiPx(kPoseViewRightFloor);
  const float left_floor = UiPx(kPoseViewLeftFloor);
  const float denom_w = std::max(0.0f, avail_w - splitter_w);
  float right_w = state.analyze_pose_view_right_ratio * denom_w;
  if (denom_w >= right_floor + left_floor) {
    right_w = std::clamp(right_w, right_floor, denom_w - left_floor);
  } else {
    right_w = std::max(0.0f, denom_w * (right_floor / (right_floor + left_floor)));
  }
  const float left_w = std::max(0.0f, denom_w - right_w);

  // The body's five elements are placed by ABSOLUTE cursor (the derived geometry and the
  // placement are the same arithmetic — the SameLine row flow would put the right column's
  // vertical stack back at the window's left edge on the line below the 3D row, under the 3D
  // region's own child).
  const ImVec2 body_min = ImGui::GetCursorScreenPos();
  RenderAnalyzeRay3DRegion(state, ImVec2(left_w, avail.y));
  ImGui::SetCursorScreenPos(ImVec2(body_min.x + left_w, body_min.y));
  PoseViewColumnSplitter("##PoseViewSplitter", avail.y, right_w, &state.analyze_pose_view_right_ratio, denom_w,
                         right_floor, left_floor);

  // ---- The right column: D5's globe | factors split, the row-splitter recipe. The column
  // spends its height on the globe region, the strip and the factors region; the two
  // ItemSpacings between the three elements are part of the budget the stored ratio divides.
  // Same floors-and-fallback rule as D4. ----
  const float strip_h = UiPx(kAnalyzeSplitterSize);
  const float v_spacing = ImGui::GetStyle().ItemSpacing.y;
  const float denom_h = std::max(0.0f, avail.y - strip_h - v_spacing * 2.0f);
  const float globe_floor = UiPx(kPoseViewGlobeFloor);
  const float factors_floor = UiPx(kPoseViewFactorsFloor);
  float globe_h = state.analyze_pose_view_globe_ratio * denom_h;
  if (denom_h >= globe_floor + factors_floor) {
    globe_h = std::clamp(globe_h, globe_floor, denom_h - factors_floor);
  } else {
    globe_h = std::max(0.0f, denom_h * (globe_floor / (globe_floor + factors_floor)));
  }
  const float factors_h = std::max(0.0f, denom_h - globe_h);
  const float col_x = body_min.x + left_w + splitter_w;

  ImGui::SetCursorScreenPos(ImVec2(col_x, body_min.y));
  RenderAnalyzeSunGlobeRegion(state, ImVec2(right_w, globe_h));
  ImGui::SetCursorScreenPos(ImVec2(col_x, body_min.y + globe_h + v_spacing));
  PoseViewRowSplitter("##PoseViewRowsSplitter", right_w, globe_h, &state.analyze_pose_view_globe_ratio, denom_h,
                      globe_floor, factors_floor);
  ImGui::SetCursorScreenPos(ImVec2(col_x, body_min.y + globe_h + v_spacing + strip_h + v_spacing));
  RenderPoseCurveRegion(state, ImVec2(right_w, factors_h));
  ImGui::End();
}

// ---- The point-level details block -------------------------------------------------------------------

void RenderPointDetailsBlock(GuiState& state, const SelectedClassIdentity& identity) {
  PointQuerySession& point = state.analysis.point;
  PointViewState& view = state.analysis.point_view;

  ImGui::Separator();
  // This title and the Raypath view's Pose & trace button ensure the same window is open.
  // Both write display state only: neither starts a query nor touches a document field.
  // Sky-point writers (map pick, marker drag, coordinate Set) remain separate from navigation:
  // setting a sky point never pops the window open. With navigation now also in the map TU,
  // the negative contract is input-test guarded, not enforced by TU separation:
  // pose_view_opens_from_both_explicit_entries and map_region_shows_the_class_map_and_takes_the_pick.
  if (ImGui::Selectable("Point detail")) {
    state.analyze_pose_view_open = true;
  }
  if (identity.layer_face_counts.size() > 1) {
    // The request-level refusal the point face will name for this selection, spelled before it
    // is asked for (the survey block states the same fact for its own face).
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped(
        "Multi-layer chains are refused by the point face (multi_layer_unsupported); the existing "
        "retreat — keep only this chain in the filters and re-run — stays the way through.");
    ImGui::PopStyleColor();
    return;
  }
  const PointQueryState st = PointQueryStateFor(point);
  if (st == PointQueryState::kNoIntent) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped(
        "Pick a sky point on the raypath view — the point's components, factors and path appear "
        "here.");
    ImGui::PopStyleColor();
    return;
  }
  if (st == PointQueryState::kWaiting) {
    const std::string wait_line = PointWaitLineFor(point, ImGui::GetTime(), g_survey_executor.InflightPollSnapshot());
    state.analysis.wait_feedback_texts.point_wait_line_details = wait_line;
    ImGui::TextWrapped("%s", wait_line.c_str());
    if (ImGui::SmallButton("Cancel point")) {
      // R6's cancel row, the survey's shape: withdraw the intent, latch it against this key, and
      // stop the playback a withdrawn query could keep feeding.
      g_survey_executor.CancelInFlight(QueryFace::kPoint);
      point.cancelled_for_key = true;
      point.adopted_result.reset();
      point.adopted_error.clear();
      view.playing = false;
    }
    return;
  }
  if (st == PointQueryState::kCancelled) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped(
        "Point cancelled for this selection. Moving the sky point — or re-picking the class — asks "
        "anew.");
    ImGui::PopStyleColor();
    return;
  }
  if (st == PointQueryState::kRefused) {
    ImGui::TextUnformatted("Point refused:");
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped("%s", point.adopted_error.c_str());
    if (point.adopted_error.find("multi_layer") != std::string::npos) {
      ImGui::TextWrapped(
          "Multi-layer chains are not supported by the point face. The existing retreat stands: keep "
          "only this chain in the filters and re-run the analysis.");
    }
    ImGui::PopStyleColor();
    return;
  }

  // Completed: the document's own echo first (the engine's words, AC1), then the arm.
  const std::optional<LUMICE_PathPointInfo> info = ReadPointInfo(*point.adopted_result);
  if (!info.has_value()) {
    DisabledWrappedText("The adopted point document did not answer its Info reader.");
    return;
  }
  if (info->execution != LUMICE_PATH_OUTCOME_COMPLETED) {
    ImGui::PushStyleColor(ImGuiCol_Text, WarningTextColor());
    ImGui::TextWrapped("Partial document: %s — what arrived is on show, honestly incomplete.",
                       ExecutionText(info->execution));
    ImGui::PopStyleColor();
  }
  for (const std::string& line_text : PointCaliberLines(*info)) {
    ImGui::Bullet();
    ImGui::TextWrapped("%s", line_text.c_str());
  }

  if (info->outcome == LUMICE_PATH_POINT_MASS) {
    LUMICE_PathPointMass mass{};
    mass.struct_size = sizeof(mass);
    if (LUMICE_PathPointGetMass(point.adopted_result.get(), &mass) == LUMICE_PATH_READ_OK) {
      ImGui::Spacing();
      ImGui::TextWrapped("Rank-0 point mass (no fiber, no playback — the mass is the whole answer):");
      for (const std::string& line_text : PointMassLines(mass)) {
        ImGui::Bullet();
        ImGui::TextWrapped("%s", line_text.c_str());
      }
    } else {
      ImGui::TextDisabled("The mass block did not answer its reader.");
    }
    return;
  }

  std::vector<PointComponentRow> rows;
  const int component_total = PointComponentRows(*point.adopted_result, &rows);
  ImGui::Spacing();
  if (component_total == 0) {
    LUMICE_PathPointReach reach{};
    reach.struct_size = sizeof(reach);
    const bool have_reach = LUMICE_PathPointGetReach(point.adopted_result.get(), &reach) == LUMICE_PATH_READ_OK;
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    if (have_reach && !reach.target_in_range) {
      ImGui::TextWrapped(
          "No component: the target's deviation lies outside this path's deflection range (reach "
          "probe, tolerance %.2e rad).",
          reach.tolerance_rad);
    } else {
      ImGui::TextWrapped(
          "No component at this point (the finite crystal passes no pose; the discovery's own "
          "completeness note is below).");
    }
    ImGui::PopStyleColor();
    LUMICE_PathPointDiscovery discovery{};
    discovery.struct_size = sizeof(discovery);
    if (LUMICE_PathPointGetDiscovery(point.adopted_result.get(), &discovery) == LUMICE_PATH_READ_OK &&
        discovery.completeness_note.length > 0) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
      ImGui::TextWrapped("%s", PathTextToString(discovery.completeness_note).c_str());
      ImGui::PopStyleColor();
    }
    return;
  }

  // The component list (AC2) as EVIDENCE rows: kind, ends, coverage and integral availability,
  // with the label's hover tooltip for a clipped tail. The rows no longer select — the
  // component selector's one interaction owner is the pose window's globe combo (contract
  // §2.2 pins the list here as evidence, §7 pins the selection in the window; one function,
  // one owner). The render cap note still names itself below.
  LUMICE_PathPointInfo count_info{};
  count_info.struct_size = sizeof(count_info);
  const size_t true_component_count =
      LUMICE_PathPointGetInfo(point.adopted_result.get(), &count_info) == LUMICE_PATH_READ_OK ?
          count_info.component_count :
          component_total;
  ImGui::TextUnformatted("Components:");
  for (int i = 0; i < component_total; ++i) {
    const PointComponentRow& row = rows[static_cast<size_t>(i)];
    char label[192];
    std::snprintf(label, sizeof(label), "%d: %s, %d pts, ends %s / %s%s", i + 1, row.kind.c_str(), row.point_count,
                  row.forward_text.c_str(), row.backward_text.c_str(),
                  row.integral_available ? "" : ", integral unavailable");
    ImGui::Bullet();
    ImGui::TextWrapped("%s", label);
    RowLabelTextTooltip(std::string(label));
  }
  const int component = std::clamp(view.component_index, 0, component_total - 1);
  {
    const PointComponentRow& row = rows[static_cast<size_t>(component)];
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    if (row.integral_available) {
      ImGui::TextWrapped("fiber integral %.6g (covers %d/%d points' arclength)", row.fiber_integral,
                         row.available_points, row.total_points);
    } else {
      ImGui::TextWrapped("fiber integral: unavailable (%d/%d points usable)", row.available_points, row.total_points);
    }
    if (static_cast<int>(true_component_count) > component_total) {
      ImGui::TextWrapped("showing %d of %zu components (the list's render cap).", component_total,
                         true_component_count);
    }
    ImGui::PopStyleColor();
  }

  // The selected component's sample evidence (contract §10's diagnostic row): a component
  // carrying no valid sample says so HERE — the answer-and-evidence fact this block owns. The
  // per-sample FACTORS themselves draw in the Pose & trace window (##AnalyzePoseFactors).
  const std::vector<int> detail_valid_indices = CollectValidSampleIndices(*point.adopted_result, component);
  if (detail_valid_indices.empty()) {
    ImGui::TextDisabled("No valid sample on show for this component.");
  }
}

}  // namespace lumice::gui
