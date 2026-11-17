#include "gui/analyze_point_view.hpp"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <optional>
#include <tuple>
#include <utility>

#include "IconsFontAwesome6.h"           // the staleness hint's leading glyph, the banner's family
#include "app.hpp"                       // g_survey_executor — the cancel button reaches the one executor
#include "gui/analysis_panel.hpp"        // RowLabelTextTooltip — the component rows' hover identity
#include "gui/analyze_overview.hpp"      // FoldOpen — the answer host's fold group headers
#include "gui/analyze_region_wheel.hpp"  // the wheel claim — one arbitration for both gesture regions
#include "gui/analyze_ui_widgets.hpp"  // DisabledWrappedText / WarningTextWrapped / DrawLegendIconSlot / AnalyzeSplitter
#include "gui/canvas_report_layout.hpp"
#include "gui/crystal_preview.hpp"  // CrystalParamHash — the mesh cache's change key
#include "gui/crystal_renderer.hpp"
#include "gui/gui_constants.hpp"
#include "gui/gui_logger.hpp"
#include "gui/gui_state.hpp"
#include "gui/overlay_labels.hpp"  // detail::ClampLabelPosToViewport — the one shared label clamp
#include "gui/semantic_colors.hpp"
#include "gui/theme.hpp"
#include "gui/tooltip.hpp"
#include "imgui.h"
#include "include/lumice_editor.h"  // LUMICE_GetCrystalMesh — the crystal's true-scale wireframe
#include "util/env_knobs.hpp"       // env::AnalyzeDebugDetails — the diagnostics groups' env gate
#include "util/sky_direction.hpp"

namespace lumice::gui {
namespace {

std::string PathTextToString(const LUMICE_PathText& text) {
  if (text.data == nullptr || text.length == 0) {
    return std::string();
  }
  return std::string(text.data, text.length);
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

// Register a passive text item's COMPLETE layout bounds without a second layout or hit target.
// Text has no ID; the test engine otherwise sees only emitted glyphs (and misses clipped tails).
void RegisterPointTextItem(const char* label) {
  ImGuiContext& g = *ImGui::GetCurrentContext();
  // ImGui upgrades must re-check LastItemData restoration and ItemAdd's non-layout side effects.
  if (!g.TestEngineHookItems) {
    return;
  }
  const ImGuiLastItemData saved = g.LastItemData;
  ImGui::ItemAdd(saved.Rect, ImGui::GetID(label), nullptr, ImGuiItemFlags_NoNav);
  IMGUI_TEST_ENGINE_ITEM_INFO(g.LastItemData.ID, label, g.LastItemData.StatusFlags);
  g.LastItemData = saved;
}

// One policy value drives both the numerical floor and its help icon's text (owner 2026-11-17:
// Full widens to 14 decades; both values stay adjustable after hands-on).
constexpr int CurveRangeDecades(PointCurveRangeMode mode) {
  return mode == PointCurveRangeMode::kFull ? 14 : 5;
}

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
  view->curve_range_mode = PointCurveRangeMode::kFit;
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

// The default layer's half (contract §16.1's ladder, §16.4's factors region): the pose itself
// and the four factor conclusions — or their named-unavailable forms (R4: no zero stands in
// for a quantity the engine did not produce). The per-interface rows below are T's optical
// premises, the Interface transmission fold's rows; PointFactorLines is the one concatenation
// (the tests' consumer — the production readers take the halves directly).
std::vector<std::string> PointFactorHeadLines(const LUMICE_PathPointSample& sample) {
  std::vector<std::string> lines;
  char buf[192];
  // Orientation: the pose's own angle words, the degeneracy named (R4 — a spin-degenerate pose
  // says so; the angles do not pretend to locate it).
  std::snprintf(buf, sizeof(buf), "pose: zenith %.2f°, azimuth %.2f°, roll %.2f°%s", sample.zenith_deg,
                sample.azimuth_deg, sample.roll_deg, sample.angles_degenerate != 0 ? " (degenerate)" : "");
  lines.emplace_back(buf);
  std::snprintf(buf, sizeof(buf), "axis density = %.6g rad^-3", sample.rho);
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
    std::snprintf(buf, sizeof(buf), "axis density × A/a^2 × T × K = %.6g", sample.factor_product);
    lines.emplace_back(buf);
  } else {
    lines.emplace_back("J (jacobian): unavailable");
    lines.emplace_back("K = 1/(J+eps): unavailable");
    lines.emplace_back("axis density × A/a^2 × T × K: unavailable");
  }
  return lines;
}

std::vector<std::string> PointFactorInterfaceLines(const LUMICE_PathPointSample& sample) {
  std::vector<std::string> lines;
  char buf[192];
  const int interfaces = static_cast<int>(sample.interface_count);
  for (int i = 0; i < interfaces; ++i) {
    std::snprintf(buf, sizeof(buf), "interface %d/%d transmittance = %.6g (conditional on this path)", i + 1,
                  interfaces, sample.interface_transmittances[i]);
    lines.emplace_back(buf);
  }
  return lines;
}

std::vector<std::string> PointFactorLines(const LUMICE_PathPointSample& sample) {
  std::vector<std::string> lines = PointFactorHeadLines(sample);
  for (std::string& line : PointFactorInterfaceLines(sample)) {
    lines.push_back(std::move(line));
  }
  return lines;
}

std::vector<std::string> PointCaliberLines(const LUMICE_PathPointInfo& info) {
  std::vector<std::string> lines;
  char buf[256];
  std::snprintf(buf, sizeof(buf), "target: alt %.3f°, az %.3f° (deviation %.3f°)", info.target_altitude_deg,
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
  std::snprintf(buf, sizeof(buf), "point mass at alt %.3f°, az %.3f° (target separation %.3e°)", mass.altitude_deg,
                mass.azimuth_deg, mass.target_separation_deg);
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

PointCurveAxisRange PointCurveAxisFit(const PointContributionCurve& curve, PointCurveRangeMode mode) {
  double lo = std::numeric_limits<double>::max();
  double hi = 0.0;
  for (const PointCurvePoint& point : curve.points) {
    for (const CurveArm& arm : kCurveArmFields) {
      const double value = point.*arm.value;
      if (point.*arm.kind == PointCurveValueKind::kPositive && value > 0.0 && std::isfinite(value)) {
        lo = std::min(lo, value);
        hi = std::max(hi, value);
      }
    }
  }
  if (!(hi > 0.0)) {
    return {};
  }
  const double floor_ratio = std::pow(10.0, -CurveRangeDecades(mode));
  return { true, std::max(lo, hi * floor_ratio), hi };
}

std::vector<PointCurveLogTick> PointCurveLogTicks(double lo, double hi) {
  std::vector<PointCurveLogTick> ticks;
  if (!(lo > 0.0) || !(hi > 0.0) || lo > hi || !std::isfinite(hi)) {
    return ticks;
  }
  const double log_hi = std::log10(hi) + (hi == lo ? 1.0 : 0.0);
  const int first_exp = static_cast<int>(std::floor(std::log10(lo)));
  const int last_exp = static_cast<int>(std::ceil(log_hi));
  for (int e = first_exp; e <= last_exp; ++e) {
    const long double decade = std::pow(10.0L, e);
    for (const double mantissa : { 1.0, 2.0, 5.0 }) {
      const long double candidate = decade * mantissa;
      if (candidate < lo || candidate > std::numeric_limits<double>::max()) {
        continue;
      }
      const double value = static_cast<double>(candidate);
      if ((hi > lo && value > hi) || (hi == lo && std::log10(value) > log_hi)) {
        continue;
      }
      if (!ticks.empty() && ticks.back().value >= value) {
        continue;  // distinct log ticks can round to the same subnormal value
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

std::vector<PointCurveRun> PointCurvePositiveRuns(const PointCurveValueKind* kinds, size_t count) {
  std::vector<PointCurveRun> runs;
  if (kinds == nullptr) {
    return runs;
  }
  size_t i = 0;
  while (i < count) {
    if (kinds[i] != PointCurveValueKind::kPositive) {
      ++i;
      continue;
    }
    const size_t begin = i;
    while (i < count && kinds[i] == PointCurveValueKind::kPositive) {
      ++i;
    }
    runs.push_back(PointCurveRun{ static_cast<int>(begin), static_cast<int>(i) });
  }
  return runs;
}

namespace {
// Avoid log(a)-log(b) cancellation for a narrow band, and ratio overflow/underflow at the tails.
double CurveLogDistance(double value, double origin) {
  const double relative = (value - origin) / origin;
  if (std::abs(relative) < 0.5) {
    return std::log1p(relative) / std::log(10.0);
  }
  return std::log10(value) - std::log10(origin);
}

double CurveLogY(double value, const PointCurveAxisRange& range) {
  const double span = CurveLogDistance(range.hi, range.lo);
  return CurveLogDistance(value, range.lo) / (span > 0.0 ? span : 1.0);
}
}  // namespace

bool PointCurveClipSegment(double x0, double value0, double x1, double value1, const PointCurveAxisRange& range,
                           PointCurveSegment* out) {
  if (!out || !range.valid || !(range.lo > 0.0) || range.hi < range.lo || !std::isfinite(range.hi) || !(value0 > 0.0) ||
      !(value1 > 0.0) || !std::isfinite(value0) || !std::isfinite(value1) || !std::isfinite(x0) || !std::isfinite(x1)) {
    return false;
  }
  const double y0 = CurveLogY(value0, range);
  const double y1 = CurveLogY(value1, range);
  if ((y0 < 0.0 && y1 < 0.0) || (y0 > 1.0 && y1 > 1.0)) {
    return false;
  }
  double enter = 0.0, leave = 1.0;
  const double dy = y1 - y0;
  if (dy != 0.0) {
    const double at_bottom = -y0 / dy;
    const double at_top = (1.0 - y0) / dy;
    enter = std::max(enter, std::min(at_bottom, at_top));
    leave = std::min(leave, std::max(at_bottom, at_top));
  }
  if (enter > leave) {
    return false;
  }
  const double clipped_y0 = enter > 0.0 ? (dy > 0.0 ? 0.0 : 1.0) : std::clamp(y0, 0.0, 1.0);
  const double clipped_y1 = leave < 1.0 ? (dy > 0.0 ? 1.0 : 0.0) : std::clamp(y1, 0.0, 1.0);
  *out = { x0 + (x1 - x0) * enter, clipped_y0, x0 + (x1 - x0) * leave, clipped_y1 };
  return true;
}

void DrawPointCurveArms(ImDrawList* dl, const PointContributionCurve& curve, const PointCurveAxisRange& range,
                        const ImVec2& plot_min, const ImVec2& plot_max) {
  if (!range.valid || curve.points.empty() || !(curve.points.back().x_rad > 0.0) || plot_max.x <= plot_min.x ||
      plot_max.y <= plot_min.y) {
    return;
  }
  const double x_hi = curve.points.back().x_rad;
  const auto pixel = [&](double x, double y) {
    return ImVec2(plot_min.x + static_cast<float>((x / x_hi) * (plot_max.x - plot_min.x)),
                  plot_max.y - static_cast<float>(y * (plot_max.y - plot_min.y)));
  };
  dl->PushClipRect(plot_min, plot_max, true);
  std::vector<PointCurveValueKind> kinds(curve.points.size());
  for (size_t arm = 0; arm < std::size(kCurveArmFields); ++arm) {
    const auto field = kCurveArmFields[arm];
    for (size_t i = 0; i < curve.points.size(); ++i) {
      kinds[i] = curve.points[i].*field.kind;
    }
    for (const auto& run : PointCurvePositiveRuns(kinds.data(), kinds.size())) {
      if (run.end - run.begin == 1) {
        const auto& p = curve.points[run.begin];
        const double value = p.*field.value;
        if (value >= range.lo && value <= range.hi) {
          dl->AddCircleFilled(pixel(p.x_rad, CurveLogY(value, range)), UiPx(2.5f), kPointCurveArmColors[arm]);
        }
      } else {
        for (int i = run.begin + 1; i < run.end; ++i) {
          const auto& a = curve.points[i - 1];
          const auto& b = curve.points[i];
          PointCurveSegment segment{};
          if (PointCurveClipSegment(a.x_rad, a.*field.value, b.x_rad, b.*field.value, range, &segment)) {
            dl->AddLine(pixel(segment.x0, segment.y0), pixel(segment.x1, segment.y1), kPointCurveArmColors[arm],
                        arm == 4 ? 2.0f : 1.5f);
          }
        }
      }
    }
  }
  dl->PopClipRect();
}

void RenderPointCurveRangeControl(PointCurveRangeMode* mode) {
  const float cell_w =
      std::max(ImGui::CalcTextSize("Fit").x, ImGui::CalcTextSize("Full").x) + 2.0f * ImGui::GetStyle().FramePadding.x;
  for (int segment = 0; segment < 2; ++segment) {
    const auto candidate = segment == 0 ? PointCurveRangeMode::kFit : PointCurveRangeMode::kFull;
    const bool current = *mode == candidate;
    if (current) {
      PushCurrentSegmentStyle();
    }
    if (ImGui::Button(segment == 0 ? "Fit##curve_range" : "Full##curve_range", ImVec2(cell_w, 0)) && !current) {
      *mode = candidate;
    }
    if (current) {
      ImGui::PopStyleColor(4);
    }
    if (segment == 0) {
      ImGui::SameLine(0.0f, 0.0f);
    }
  }
  // The two-tier rule: the pair's shared concept rides one associated info icon instead of the
  // buttons' hover (doc/gui/gui-visual-language.md); the floors stay the policy values.
  InfoIcon("curve_range_help", "Log-axis floor, decades below the largest positive value:\nFit %d, Full %d.",
           CurveRangeDecades(PointCurveRangeMode::kFit), CurveRangeDecades(PointCurveRangeMode::kFull));
}

void DrawPointCurveLogAxis(ImDrawList* dl, const PointCurveAxisRange& visible, const ImVec2& plot_min,
                           const ImVec2& plot_max) {
  if (!visible.valid || plot_max.y <= plot_min.y) {
    return;
  }
  const double v_min = visible.lo, v_max = visible.hi;
  const float plot_h = plot_max.y - plot_min.y;
  const float line_h = ImGui::GetTextLineHeight();
  const int label_step = PointCurveLogLabelStep(v_min, v_max, plot_h, line_h);
  bool have_labelled_major = false;
  const auto v_to_py = [&](double value) {
    return plot_max.y - static_cast<float>(CurveLogY(value, visible) * plot_h);
  };
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
      have_labelled_major = true;
      dl->AddLine(ImVec2(plot_min.x, py), ImVec2(plot_max.x, py), IM_COL32(60, 64, 72, 110), 1.0f);
      char lab[16];
      std::snprintf(lab, sizeof(lab), "%.0e", tick.value);
      const ImVec2 ts = ImGui::CalcTextSize(lab);
      dl->AddText(ImVec2(plot_min.x - ts.x - UiPx(5.0f), py - ts.y * 0.5f), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                  lab);
    } else if (minors_fit) {
      dl->AddLine(ImVec2(plot_min.x - UiPx(3.0f), py), ImVec2(plot_min.x, py), IM_COL32(90, 94, 102, 140), 1.0f);
    }
  }
  // The boundary labels' drawing (the gutter rule above): the window's own edges at the
  // plot's top and bottom frame — the top one always fits (its upper half rides pad_t),
  // the bottom one only while the plot can host both without overlap. A reference that
  // fits beats none; nothing is squeezed over an existing label (§3.2).
  if (!have_labelled_major) {
    const auto boundary_lab = [&](double v, float py) {
      char lab[24];
      std::snprintf(lab, sizeof(lab), "%.3g", v);
      const ImVec2 ts = ImGui::CalcTextSize(lab);
      dl->AddText(ImVec2(plot_min.x - ts.x - UiPx(5.0f), py - ts.y * 0.5f), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                  lab);
    };
    boundary_lab(v_max, plot_min.y);
    if (plot_h >= line_h * 2.0f) {
      boundary_lab(v_min, plot_max.y);
    }
  }
}

void DrawPointCurveSeed(ImDrawList* dl, const PointContributionCurve& curve, const ImVec2& plot_min,
                        const ImVec2& plot_max) {
  if (curve.seed_index <= 0 || curve.seed_index >= static_cast<int>(curve.points.size()) ||
      !(curve.points.back().x_rad > 0.0)) {
    return;
  }
  const float px = plot_min.x + static_cast<float>(curve.points[curve.seed_index].x_rad / curve.points.back().x_rad *
                                                   (plot_max.x - plot_min.x));
  dl->AddTriangleFilled(ImVec2(px - UiPx(4), plot_min.y), ImVec2(px + UiPx(4), plot_min.y),
                        ImVec2(px, plot_min.y + UiPx(5)), ImGui::ColorConvertFloat4ToU32(AccentColor()));
  dl->AddText(ImVec2(px + UiPx(5), plot_min.y), ImGui::GetColorU32(ImGuiCol_TextDisabled), "seed");
}

// ---- The u-globe -------------------------------------------------------------------------------------

// The projector's basis rule, shared by both view modes so they cannot drift: `centre` as given;
// the reference up picks the world +z when the centre is not parallel to it (the map's own
// fallback rule), else +x; then up is orthogonalized against the centre and right = up × centre
// re-crosses the triple orthonormal.
void BuildSunGlobeBasis(const double centre[3], SunGlobeProjector* out) {
  for (int k = 0; k < 3; ++k) {
    out->centre[k] = centre[k];
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
}

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
  // The Fit view's centre is the component's first valid u.
  BuildSunGlobeBasis(sample.sun_in_crystal, out);
  return true;
}

SunGlobeProjector MakeSunGlobeFullProjector() {
  // The Full view's centre: the crystal +x, an equatorial fixed direction (owner-picked within
  // the "fixed equatorial direction" budget — the same latitude band the common refraction /
  // reflection fibres sweep, so the band stays off the rim). The shared basis rule puts +z (the
  // c-axis) on screen-up from it: |centre.z| = 0 < 0.999 references +z, up orthogonalizes to
  // exactly +z, right = up × centre = +y.
  static constexpr double kFixedCentre[3] = { 1.0, 0.0, 0.0 };
  SunGlobeProjector out;
  BuildSunGlobeBasis(kFixedCentre, &out);
  return out;
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

namespace {

bool SunGridCellDrawable(double deviation_rad, uint8_t valid) {
  return valid != 0 && deviation_rad >= 0.0 && deviation_rad <= 3.15;
}

size_t SunGridDrawCount(const LUMICE_PathSunGrid& grid) {
  if (!grid.valid || !grid.deviation_rad || grid.lat_count <= 0 || grid.lon_count <= 0) {
    return 0;
  }
  return std::min(grid.cell_count, static_cast<size_t>(grid.lat_count) * static_cast<size_t>(grid.lon_count));
}

constexpr const char* kSunGridLegendTitle = "Deviation δ · auto";

}  // namespace

SunGridRange SunGridDisplayRange(const LUMICE_PathSunGrid& grid) {
  SunGridRange range;
  const size_t count = SunGridDrawCount(grid);
  for (size_t cell = 0; cell < count; ++cell) {
    const double delta = grid.deviation_rad[cell];
    if (!SunGridCellDrawable(delta, grid.valid[cell])) {
      continue;
    }
    if (!range.available) {
      range = { true, delta, delta };
    } else {
      range.min_rad = std::min(range.min_rad, delta);
      range.max_rad = std::max(range.max_rad, delta);
    }
  }
  return range;
}

uint32_t SunGridCellColorRgba(double deviation_rad, uint8_t valid, const SunGridRange& range) {
  if (!SunGridCellDrawable(deviation_rad, valid) || !range.available || !SunGridCellDrawable(range.min_rad, 1) ||
      !SunGridCellDrawable(range.max_rad, 1) || range.max_rad < range.min_rad) {
    return 0;
  }
  const double t = range.min_rad == range.max_rad ?
                       0.5 :
                       std::clamp((deviation_rad - range.min_rad) / (range.max_rad - range.min_rad), 0.0, 1.0);
  // Sequential violet: all three channels increase, hence luminance is strictly ordered.
  // The existing violet data family, not the solar gold or any health/status colour.
  return IM_COL32(static_cast<int>(92 + 127 * t), static_cast<int>(55 + 129 * t), static_cast<int>(135 + 115 * t), 235);
}

void DrawSunGridBackdrop(ImDrawList* dl, const LUMICE_PathSunGrid& grid, const SunGlobeProjector& projector,
                         const SunGridRange& range, const ImVec2& centre, float radius) {
  const size_t count = SunGridDrawCount(grid);
  constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
  for (size_t cell = 0; cell < count; ++cell) {
    const ImU32 color = SunGridCellColorRgba(grid.deviation_rad[cell], grid.valid[cell], range);
    if (color == 0) {
      continue;
    }
    const double lat = (-90.0 + (cell / grid.lon_count + 0.5) * 180.0 / grid.lat_count) * kDegToRad;
    const double lon = (-180.0 + (cell % grid.lon_count + 0.5) * 360.0 / grid.lon_count) * kDegToRad;
    const double u[3] = { std::cos(lat) * std::cos(lon), std::cos(lat) * std::sin(lon), std::sin(lat) };
    double x, y;
    SunGlobeProject(projector, u, &x, &y);
    const ImVec2 p(static_cast<float>(centre.x + (x - .5) * 2 * radius),
                   static_cast<float>(centre.y + (y - .5) * 2 * radius));
    const float half = std::max(1.5f, radius / 48.0f);
    dl->AddRectFilled(ImVec2(p.x - half, p.y - half), ImVec2(p.x + half, p.y + half), color);
  }
}

float SunGridLegendColumnWidth() {
  // The column's fixed width claim on the canvas row: wide enough for the common "12.3° – 45.6°"
  // range on one line at the default font, narrow enough to leave the disk the row's majority.
  // Longer extreme-value texts wrap to further lines (the footprint is consumed vertically —
  // the band absorbs the row's slack, never the text).
  return UiPx(96.0f);
}

// The range text's own endpoint ladder (the 6..17 significant-digit climb until lo and hi read
// apart, or the conversion itself collides) — shared with the band's end ticks so the ticks and
// the text cannot disagree about the endpoints they name. Also the single owner of the rad→deg
// display conversion (kRadToDeg, this file's one copy).
constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;

void SunGridDegEndpoints(const SunGridRange& range, char* lo, size_t lo_size, char* hi, size_t hi_size) {
  for (int precision = 6; precision <= 17; ++precision) {
    std::snprintf(lo, lo_size, "%.*g", precision, range.min_rad * kRadToDeg);
    std::snprintf(hi, hi_size, "%.*g", precision, range.max_rad * kRadToDeg);
    if (range.min_rad == range.max_rad || std::strcmp(lo, hi) != 0) {
      break;
    }
  }
}

std::string SunGridRangeText(const SunGridRange& range) {
  if (!range.available) {
    return "No valid deviation data";
  }
  char lo[64], hi[64];
  SunGridDegEndpoints(range, lo, sizeof(lo), hi, sizeof(hi));
  if (range.min_rad != range.max_rad && std::strcmp(lo, hi) == 0) {
    // Adjacent doubles can collide in the degree conversion itself, not just in formatting.
    // Keep a nonzero interval honest with an origin + offsets, rather than inventing extrema.
    char span[64];
    std::snprintf(span, sizeof(span), "%.17g", (range.max_rad - range.min_rad) * kRadToDeg);
    return std::string(lo) + "° + [0, " + span + "]°";
  }
  return range.min_rad == range.max_rad ? std::string(lo) + "° (constant)" : std::string(lo) + "° – " + hi + "°";
}

void RenderSunGridLegend(const SunGridRange& range, float row_height) {
  // The vertical column at the canvas's right edge: header, the band row, the range text. The
  // end ticks ride the band's right edge (the high one top-aligned, the low one pinned to the
  // band's bottom) but are SUBMITTED only when the row's remainder can host their stack — the
  // range text carries both endpoints verbatim, so a short row loses the tick labels, then the
  // ramp, never its numbers. The column manages the cursor EXPLICITLY: a wrapped text ends
  // ImGui's item flow back at the window's line start, so a SameLine'd column of wrapped texts
  // would scatter its tail down the window's left edge — every item is placed at the column's
  // x by hand, wrapped at the column's width.
  //
  // The bound is the mechanism, measured before anything submits: budget = the row's remainder
  // after the header and the range text. budget >= tick stack submits band + ticks (band =
  // min(budget, cap), the column bounded by the row); 0 <= budget < stack submits band alone
  // (band = budget); budget < 0 — even the words exceed the row — yields the whole legend, the
  // canvas's own short-host rule. Every submitted branch satisfies column_height <= row_height
  // by construction; nothing can slide under the slider row's later-submitted frame.
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const float wrap = std::max(1.0f, SunGridLegendColumnWidth());
  const float spacing = ImGui::GetStyle().ItemSpacing.y;
  const ImVec2 column = ImGui::GetCursorScreenPos();
  const float wrap_x = ImGui::GetCursorPosX() + wrap;  // window-local
  const float band_w = UiPx(12.0f);
  const float band_gap = ImGui::GetStyle().ItemSpacing.x;
  // The ticks render at the band's right edge, so their measured width uses the width they
  // actually have there — measuring from the column origin would undercount the second line a
  // long endpoint string wraps into and could overlap the two ticks.
  const float tick_wrap = std::max(1.0f, wrap - band_w - band_gap);
  const auto text_h = [&](const char* text, float avail) { return ImGui::CalcTextSize(text, nullptr, false, avail).y; };
  const auto submit_column = [&](float title_h, float band_row_h, float band_h, bool show_ticks, const char* hi,
                                 const char* lo, const char* range_text, float range_h) {
    ImGui::PushTextWrapPos(wrap_x);
    float y = column.y;
    // Place BEFORE each item: ImGui's ItemSize pulls the flow cursor back to the window's line
    // start after every item (wrapped texts and buttons alike), so the column re-pins its own
    // cursor ahead of each member — nothing can fall out of the column to the line below.
    const auto place = [&](float item_h) {
      ImGui::SetCursorScreenPos(ImVec2(column.x, y));
      y += item_h + spacing;
    };
    place(title_h);
    DisabledWrappedText("%s", kSunGridLegendTitle);
    RegisterPointTextItem("##sun_grid_title");
    const ImVec2 band_pos(column.x, y);
    y += band_row_h + spacing;
    // A zero-height band submits no item at all: an empty hit target would only register as a
    // phantom "present" widget to the tests and the item registry.
    if (band_h > 0.0f) {
      ImGui::SetCursorScreenPos(band_pos);
      ImGui::InvisibleButton("##sun_grid_band", ImVec2(band_w, band_h));
      const bool constant = range.min_rad == range.max_rad;
      const int steps = constant ? 1 : 64;
      for (int i = 0; i < steps; ++i) {
        // Segment i counts from the TOP: the last segment (the ramp's high end) sits at the top,
        // so the delta walk runs high → low as y grows.
        const double delta =
            constant ? range.min_rad : range.max_rad - (range.max_rad - range.min_rad) * i / (steps - 1);
        dl->AddRectFilled(ImVec2(band_pos.x, band_pos.y + band_h * i / steps),
                          ImVec2(band_pos.x + band_w, band_pos.y + band_h * (i + 1) / steps),
                          SunGridCellColorRgba(delta, 1, range));
      }
    }
    if (show_ticks) {
      ImGui::SetCursorScreenPos(ImVec2(band_pos.x + band_w + band_gap, band_pos.y));
      DisabledWrappedText("%s°", hi);
      RegisterPointTextItem("##sun_grid_hi_tick");
      const float lo_y = band_pos.y + std::max(band_h - text_h(lo, tick_wrap), text_h(hi, tick_wrap) + 2.0f);
      ImGui::SetCursorScreenPos(ImVec2(band_pos.x + band_w + band_gap, lo_y));
      DisabledWrappedText("%s°", lo);
      RegisterPointTextItem("##sun_grid_lo_tick");
    }
    place(range_h);
    DisabledWrappedText("%s", range_text);
    RegisterPointTextItem("##sun_grid_range");
    ImGui::PopTextWrapPos();
    // The column's end: the caller resumes the flow from here (the slider row below the canvas).
    ImGui::SetCursorScreenPos(ImVec2(column.x, y));
  };
  const float title_h = text_h(kSunGridLegendTitle, wrap);
  if (!range.available) {
    const std::string empty_text = SunGridRangeText(range);
    const float body_h = text_h(empty_text.c_str(), wrap);
    if (title_h + spacing + body_h > row_height) {
      return;  // a row too short for even the words yields the legend (the short-host rule)
    }
    submit_column(title_h, 0.0f, 0.0f, false, "", "", empty_text.c_str(), body_h);
    return;
  }
  char lo[64], hi[64];
  SunGridDegEndpoints(range, lo, sizeof(lo), hi, sizeof(hi));
  char hi_tick[72];
  std::snprintf(hi_tick, sizeof(hi_tick), "%s°", hi);
  char lo_tick[72];
  std::snprintf(lo_tick, sizeof(lo_tick), "%s°", lo);
  const float hi_h = text_h(hi_tick, tick_wrap);
  const float lo_h = text_h(lo_tick, tick_wrap);
  const float tick_stack = hi_h + 2.0f + lo_h;
  const std::string range_text = SunGridRangeText(range);
  const float range_h = text_h(range_text.c_str(), wrap);
  const float budget = row_height - title_h - spacing - range_h - spacing;
  if (budget < 0.0f) {
    return;  // even the header and the numbers exceed the row — yield, never overlap the row below
  }
  const bool show_ticks = budget >= tick_stack;
  const float band_h = std::clamp(budget, 0.0f, UiPx(120.0f));
  const float band_row_h = show_ticks ? std::max(band_h, tick_stack) : band_h;
  submit_column(title_h, band_row_h, band_h, show_ticks, hi, lo, range_text.c_str(), range_h);
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
  // The per-segment cumulative Fresnel relative energies, straight from the contacts' own
  // factors (T at entry/exit, R at internal reflections — the payload's semantics verbatim, no
  // complement). N contacts -> N+1 segments, one energy each.
  std::vector<double> factors(contacts.contact_count);
  for (size_t i = 0; i < contacts.contact_count; ++i) {
    factors[i] = contacts.contacts[i].transmittance;
  }
  out->seg_energy = RayTraceCumulativeEnergies(factors.data(), factors.size());
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

// ---- The trace display's pure energy and facing rules (declared in the hpp) -------------------------

// The rounding band a Fresnel interface factor must sit in to be usable: the engine's Fresnel
// math runs in double precision and overshoots [0,1] only by rounding magnitude, so in-band
// overshoot clamps back; beyond the band the value is not a rounding artifact and the energy
// claim stops (the prefix stays, the suffix draws neutral).
constexpr double kRayTraceFactorBand = 1e-6;

std::vector<RayTraceSegmentEnergy> RayTraceCumulativeEnergies(const double* factors, size_t count) {
  // E[0] = 1 (the entry leg, before the first interface), then E[k+1] = E[k] * q[k] — every
  // interface consumed exactly once, the last segment including the exit factor. An internal
  // reflection with R = 1 keeps the energy; an exact zero ends it (a legal declared zero). The
  // accumulation stays DOUBLE: narrowing per step would underflow a long chain of small factors
  // (0.02^27 ~ 1.3e-46) into a fabricated exact zero while the honest value is still positive.
  std::vector<RayTraceSegmentEnergy> out;
  out.reserve(count + 1);
  double energy = 1.0;
  bool valid = true;
  out.push_back(RayTraceSegmentEnergy{ energy, valid });
  for (size_t k = 0; k < count; ++k) {
    const double q = factors != nullptr ? factors[k] : 0.0;
    if (valid && std::isfinite(q) && q >= -kRayTraceFactorBand && q <= 1.0 + kRayTraceFactorBand) {
      const double clamped = std::min(1.0, std::max(0.0, q));
      energy = energy * clamped;
    } else {
      // Unusable factor: everything from the segment after it on carries no energy claim. The
      // stored value is the last honest prefix — unused while invalid, kept for inspection.
      valid = false;
    }
    out.push_back(RayTraceSegmentEnergy{ energy, valid });
  }
  return out;
}

float RayTraceEnergyWidth(double energy) {
  // The fixed sqrt monotone visual mapping over the positive range (the plan's chosen gamma: a
  // linear map presses small positives into sub-pixel lines, buckets lose the segment order).
  // E <= 0 (or non-finite) answers 0 — no ink; the contact markers stay. The mapping runs in
  // double and narrows only at the returned width.
  if (!(energy > 0.0)) {
    return 0.0f;
  }
  const double e = std::min(1.0, energy);
  return static_cast<float>(UiPx(kRayTraceWidthMinUi) +
                            (UiPx(kRayTraceWidthMaxUi) - UiPx(kRayTraceWidthMinUi)) * std::sqrt(e));
}

std::string RayTraceEnergyLabelText(const RayTraceSegmentEnergy& energy) {
  if (!energy.valid) {
    return "n/a";
  }
  char buf[32];
  // %.3g keeps tiny non-zero values honest (1e-9 -> "1e-07%") without fabricating digits for
  // the common range; an exact zero reads as "0%".
  std::snprintf(buf, sizeof(buf), "%.3g%%", static_cast<double>(energy.value) * 100.0);
  return buf;
}

bool RayTraceSegmentOnCanvas(const ImVec2& a, const ImVec2& b, const ImVec2& canvas_min, const ImVec2& canvas_max,
                             double* out_t0, double* out_t1) {
  // Liang-Barsky against the canvas rect. Degenerate (zero-length) segments answer the point's
  // own containment.
  double t0 = 0.0;
  double t1 = 1.0;
  const double dx = static_cast<double>(b.x) - a.x;
  const double dy = static_cast<double>(b.y) - a.y;
  const auto bound = [&](double p, double q) {
    if (std::abs(p) < 1e-12) {
      return q >= 0.0;
    }
    const double r = q / p;
    if (p < 0.0) {
      if (r > t1) {
        return false;
      }
      if (r > t0) {
        t0 = r;
      }
    } else {
      if (r < t0) {
        return false;
      }
      if (r < t1) {
        t1 = r;
      }
    }
    return true;
  };
  const bool visible = bound(-dx, static_cast<double>(a.x) - canvas_min.x) && bound(dx, canvas_max.x - a.x) &&
                       bound(-dy, static_cast<double>(a.y) - canvas_min.y) && bound(dy, canvas_max.y - a.y);
  if (out_t0 != nullptr) {
    *out_t0 = t0;
  }
  if (out_t1 != nullptr) {
    *out_t1 = t1;
  }
  return visible;
}

RayTraceContactFaceNormal RayTraceContactFaceNormalFor(const LUMICE_CrystalMesh& mesh, int face_number) {
  RayTraceContactFaceNormal out;
  if (face_number < 0) {
    return out;  // kInvalidId — the unknown vocabulary, never matched against the table
  }
  for (int f = 0; f < mesh.face_count; ++f) {
    if (mesh.face_numbers_by_face[f] != face_number) {
      continue;
    }
    const float* n = &mesh.face_normals[static_cast<size_t>(f) * 3];
    const float norm = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (std::isfinite(norm) && norm > 0.5f) {  // unit-length by contract; half of it is degenerate
      out.known = true;
      out.normal_body[0] = n[0];
      out.normal_body[1] = n[1];
      out.normal_body[2] = n[2];
    }
    break;  // the first table hit decides — the numbering is unique per face by contract
  }
  return out;
}

bool RayTraceContactFacesEye(const float normal_display[3], const float contact_scene[3], const float eye_scene[3]) {
  // dot(normal, eye - contact) in the scene metric: the normal is a display-frame direction, the
  // eye/contact pair scene-frame points — scene is display translated and uniformly positively
  // scaled (fit >= 1e-4), and neither moves the dot's sign.
  const double dx = static_cast<double>(eye_scene[0]) - contact_scene[0];
  const double dy = static_cast<double>(eye_scene[1]) - contact_scene[1];
  const double dz = static_cast<double>(eye_scene[2]) - contact_scene[2];
  const double dot = static_cast<double>(normal_display[0]) * dx + static_cast<double>(normal_display[1]) * dy +
                     static_cast<double>(normal_display[2]) * dz;
  return dot > 0.0;  // exact zero is tangent — hollow (the plan's tie rule)
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

// The trace key for the sample the display currently points at — the ONE constructor for the
// identity both the 3D region's gate and the status bar's transient read consume (a56: the
// key's field semantics have one implementation, so the two readers cannot drift apart; the
// guard SEQUENCES around it stay each site's own, because the region renders per-arm facts
// while the bar only needs the no-valid-sample vs state split).
RayTraceKey RayTraceKeyFor(const SurveyQueryId& point_id, const CurrentSample& current) {
  RayTraceKey key;
  key.point_id = point_id;
  key.component = current.component;
  key.sample = current.stored;
  return key;
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

// The homogeneous clip coordinates of a scene point, row-major over the column-major MVP — the
// same math TraceOracleProject (the gui_test's oracle) mirrors; keep the two in lockstep.
std::array<float, 4> ToRay3DClip(const float scene[3], const Ray3DFrameInfo& frame) {
  std::array<float, 4> clip = {};
  for (int row = 0; row < 4; ++row) {
    clip[row] = frame.mvp[12 + row];
    for (int k = 0; k < 3; ++k) {
      clip[row] += frame.mvp[k * 4 + row] * scene[k];
    }
  }
  return clip;
}

// Pixel position of an in-frustum clip point. Callers clip first: with the mesh-only fit the
// trace's display extensions can sit behind the camera, and a w <= 0 point must never divide.
ImVec2 RayTraceClipToPixel(const float clip[4], const Ray3DFrameInfo& frame) {
  const float w = clip[3];
  const float x = clip[0] / w;
  const float y = clip[1] / w;
  return ImVec2(frame.canvas_min.x + (x + 1.0f) * 0.5f * frame.canvas_size.x,
                frame.canvas_min.y + (1.0f - y) * 0.5f * frame.canvas_size.y);
}

// Clips the scene segment against the camera's near plane (w >= near_w — the same plane
// ComputeMvp builds the GL frustum with) and answers the visible part's pixel endpoints. False
// when the whole segment sits behind the near plane. Clip coordinates are affine along the
// segment, so interpolating them at the crossing parameter gives the exact intersection.
bool RayTraceClipSegmentToPixel(const float scene_a[3], const float scene_b[3], float near_w,
                                const Ray3DFrameInfo& frame, ImVec2* pa, ImVec2* pb) {
  const std::array<float, 4> ca = ToRay3DClip(scene_a, frame);
  const std::array<float, 4> cb = ToRay3DClip(scene_b, frame);
  if (ca[3] < near_w && cb[3] < near_w) {
    return false;
  }
  float t0 = 0.0f;
  float t1 = 1.0f;
  if (ca[3] < near_w) {
    t0 = (near_w - ca[3]) / (cb[3] - ca[3]);
  }
  if (cb[3] < near_w) {
    t1 = (near_w - ca[3]) / (cb[3] - ca[3]);
  }
  float a[4] = {};
  float b[4] = {};
  for (int k = 0; k < 4; ++k) {
    a[k] = ca[k] + (cb[k] - ca[k]) * t0;
    b[k] = ca[k] + (cb[k] - ca[k]) * t1;
  }
  *pa = RayTraceClipToPixel(a, frame);
  *pb = RayTraceClipToPixel(b, frame);
  return true;
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

// A fixed-size, read-only compass of the CURRENT display frame. Consume the very rotation
// passed to GL, not a second yaw/pitch convention or the crystal's physical pose.
void DrawRay3DAxes(ImDrawList* dl, const float rotation[16], Ray3DFrameInfo& frame) {
  const float length = UiPx(kRay3DAxisLengthUi);
  const float half = length + ImGui::GetTextLineHeight() * .65f + UiPx(4.0f);
  const float inset = UiPx(4.0f);
  ImRect visible(frame.canvas_min,
                 ImVec2(frame.canvas_min.x + frame.canvas_size.x, frame.canvas_min.y + frame.canvas_size.y));
  visible.ClipWithFull(ImRect(dl->GetClipRectMin(), dl->GetClipRectMax()));
  frame.axes_drawn = visible.GetWidth() >= half * 2 + inset * 2 && visible.GetHeight() >= half * 2 + inset * 2;
  if (!frame.axes_drawn) {
    return;  // compressed canvas: no clipped letters or invented directions
  }
  frame.axes_max = ImVec2(visible.Max.x - inset, visible.Max.y - inset);
  frame.axes_min = ImVec2(frame.axes_max.x - half * 2, frame.axes_max.y - half * 2);
  frame.axes_origin = ImVec2(frame.axes_max.x - half, frame.axes_max.y - half);
  // No plate (2026-11-17 feedback: the old IM_COL32(16,18,22) square read as a black box glued
  // on the canvas). The arrows and letters draw straight over the GL image — the same surface
  // every other overlay ink sits on; the compass's reserved footprint still feeds the labels'
  // avoidance, and the owner judges the look.
  std::vector<ImRect> letters;
  for (int axis = 0; axis < 3; ++axis) {
    const ImVec2 delta(length * rotation[axis * 4], -length * rotation[axis * 4 + 1]);
    const ImVec2 tip(frame.axes_origin.x + delta.x, frame.axes_origin.y + delta.y);
    frame.axes_tips[axis] = tip;
    const float norm = std::hypot(delta.x, delta.y);
    const ImU32 color = kRay3DAxisColors[axis];
    ImVec2 direction(0, -1);
    if (norm > UiPx(0.5f)) {
      direction = ImVec2(delta.x / norm, delta.y / norm);
      dl->AddLine(frame.axes_origin, tip, color, UiPx(1.5f));
      const float head = std::min(UiPx(4.0f), norm * 0.5f);
      const ImVec2 base(tip.x - direction.x * head, tip.y - direction.y * head);
      dl->AddTriangleFilled(tip, ImVec2(base.x - direction.y * head * .5f, base.y + direction.x * head * .5f),
                            ImVec2(base.x + direction.y * head * .5f, base.y - direction.x * head * .5f), color);
    } else {
      dl->AddCircleFilled(tip, UiPx(2.0f), color);  // truly edge-on: retain identity, not a fake arrow
    }
    const char text[] = { static_cast<char>('X' + axis), '\0' };
    const ImVec2 size = ImGui::CalcTextSize(text);
    const float gap = UiPx(3.0f) + size.y * .5f;
    ImVec2 pos(tip.x + direction.x * gap - size.x * .5f, tip.y + direction.y * gap - size.y * .5f);
    // Keep complete letter footprints apart even when two axes project to the same ray.
    // Three letters and eight perimeter slots suffice without altering the arrow directions.
    const ImVec2 initial = pos;
    for (int slot = 0; slot <= 8; ++slot) {
      if (slot > 0) {
        const float angle = (slot - 1) * 3.14159265f / 4.0f;
        pos = ImVec2(frame.axes_origin.x + (half - size.x) * std::cos(angle) - size.x * .5f,
                     frame.axes_origin.y + (half - size.y) * std::sin(angle) - size.y * .5f);
      } else {
        pos = initial;
      }
      pos.x = std::clamp(pos.x, frame.axes_min.x + inset, frame.axes_max.x - inset - size.x);
      pos.y = std::clamp(pos.y, frame.axes_min.y + inset, frame.axes_max.y - inset - size.y);
      const ImRect rect(pos, ImVec2(pos.x + size.x, pos.y + size.y));
      if (std::none_of(letters.begin(), letters.end(), [&](const ImRect& r) { return r.Overlaps(rect); })) {
        letters.push_back(rect);
        break;
      }
    }
    frame.axes_label_min[axis] = pos;
    frame.axes_label_max[axis] = ImVec2(pos.x + size.x, pos.y + size.y);
    dl->AddText(pos, color, text);
  }
}

ImU32 SegmentColor(int kind, float alpha) {
  const ImU32 color = kRayTraceSegmentColors[kind == 0 ? 0 : kind == 1 ? 1 : 2];
  return (color & ~IM_COL32_A_MASK) | (static_cast<ImU32>(255 * alpha) << IM_COL32_A_SHIFT);
}

// The Ray3D region's one conceptual help text (the two-tier tooltip rule): the usage note and
// the display semantics' legend — the width mapping's honesty note (visual sqrt scaling, no
// absorption — the ratio numbers carry the exact values), the contact shapes' vocabulary, and
// the unusable-factor boundary. Status reasons and real errors stay in the report: they are
// facts of a state, not concepts.
constexpr const char* kRay3DHelpText =
    "One pose's real path in the crystal: drag orbits, wheel zooms. Same physical state, two "
    "views.\n\n"
    "Width follows cumulative Fresnel relative energy (no absorption); visual scaling — the "
    "ratio numbers, when shown, carry the exact values. Contacts: solid = facing the camera, "
    "hollow = back-facing or facing unknown. A segment past an unusable interface factor draws "
    "neutral (n/a).";

// The Ray3D report's fixed budget: two text lines over the child's real chrome (its padding and
// border), computed from the LIVE font every call — never from content, last frame's height or
// the trace state. The canvas eats whatever this does not reserve; narrower columns scroll their
// longer wraps (an error's full text stays reachable by native scrolling).
float Ray3DReportQuota() {
  const ImGuiStyle& style = ImGui::GetStyle();
  return 2.0f * ImGui::GetTextLineHeightWithSpacing() + 2.0f * style.WindowPadding.y + UiPx(6.0f);
}

// The report's stable source label: the component/sample identity the shown content belongs to
// (1-based, the component list's and the curve readout's vocabulary). It is the honesty marker
// that lets the report hold still across a sample switch's transient: the retained line names
// its own sample, never masquerading as the cursor's.
std::string Ray3DReportSourceLine(int component, int sample_position, int valid_count) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "Component %d \xc2\xb7 sample %d/%d", component + 1, sample_position, valid_count);
  return std::string(buf);
}

// The canvas legend's drawing vocabulary — ONE source for the footprint decision, the Fresnel
// ladder's exclusion extents and the ink emission (a56: the ink can never drift from the rects
// the avoidance reserves by).
constexpr const char* kRay3DLegendLabels[] = { "entry", "internal", "exit" };
constexpr float kRay3DLegendSwatchUi = 9.0f;
constexpr float kRay3DLegendSwatchGapUi = 5.0f;

}  // namespace

// The point session's one-line state word (the hpp comment carries the contract). The mapping's
// one definition lives at namespace scope so the main window's hint and the TU-local status row
// read the same words; the completed read happens here so the word and its partial flag cannot
// disagree.
const char* PointStatusLabelFor(const PointQuerySession& point, bool* partial, LUMICE_PathPointInfo* completed_info) {
  if (partial != nullptr) {
    *partial = false;
  }
  if (completed_info != nullptr) {
    *completed_info = LUMICE_PathPointInfo{};
    completed_info->struct_size = sizeof(*completed_info);
  }
  const char* label = "No target";
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
      const bool is_partial = info && info->execution != LUMICE_PATH_OUTCOME_COMPLETED;
      if (partial != nullptr) {
        *partial = is_partial;
      }
      if (completed_info != nullptr && info.has_value()) {
        *completed_info = *info;
      }
      label = is_partial ? "Partial" : "Ready";
      break;
    }
  }
  return label;
}

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

// ---- The sun-direction globe
// -----------------------------------------------------------------------------------------

// The globe view control's full footprint: two same-width cells (the wider of the two labels,
// frame padding each side) joined with zero spacing — the width budget the title band's
// fits-on-line check reserves, single-sourced with the control so the check cannot undercount
// the Full−Fit label difference again.
float SunGlobeViewControlWidth() {
  return 2.0f * std::max(ImGui::CalcTextSize("Fit").x, ImGui::CalcTextSize("Full").x) +
         4.0f * ImGui::GetStyle().FramePadding.x;
}

// The globe view's fixed two-cell control (the curve range control's shape, a different life:
// this mode survives adoptions where the curve's refits — see SunGlobeViewMode). Clicking the
// current cell is a no-op; the pair's shared concept rides the region title's info icon.
void RenderSunGlobeViewControl(SunGlobeViewMode* mode) {
  const float cell_w =
      std::max(ImGui::CalcTextSize("Fit").x, ImGui::CalcTextSize("Full").x) + 2.0f * ImGui::GetStyle().FramePadding.x;
  for (int segment = 0; segment < 2; ++segment) {
    const auto candidate = segment == 0 ? SunGlobeViewMode::kFit : SunGlobeViewMode::kFull;
    const bool current = *mode == candidate;
    if (current) {
      PushCurrentSegmentStyle();
    }
    if (ImGui::Button(segment == 0 ? "Fit##globe_view" : "Full##globe_view", ImVec2(cell_w, 0)) && !current) {
      *mode = candidate;
    }
    if (current) {
      ImGui::PopStyleColor(4);
    }
    if (segment == 0) {
      ImGui::SameLine(0.0f, 0.0f);
    }
  }
}

void RenderAnalyzeSunGlobeRegion(GuiState& state, const ImVec2& size) {
  ImGui::BeginChild("##AnalyzeSunGlobe", size, ImGuiChildFlags_Borders);
  PointQuerySession& point = state.analysis.point;
  PointViewState& view = state.analysis.point_view;

  // Every result branch writes only the report after the fixed component/canvas/sample stack.
  const auto render_state_area = [](auto&& draw_lines) { draw_lines(); };

  // ---- The fixed interaction stack (R4): the controls a hand must reach are submitted outside
  // any scrolling region at every cell the window's minimum admits — the title with the view
  // control and its info icon here (the deviation-grid toggle lives in the factors region's
  // user-facing diagnostics group, §16.4); the combo, canvas, colour legend and slider/play row
  // in the discovered arm below. ----
  ImGui::TextUnformatted("Sun direction globe");
  // The view control joins the title while the line admits it; the shared concept text rides
  // one info icon (the two-tier rule) — the six per-arm copies of the old concept sentence are
  // retired with it.
  const auto title_band_fits = [&](float next_w) {
    const float line_right = ImGui::GetContentRegionMax().x + ImGui::GetWindowPos().x;
    return ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + next_w <= line_right;
  };
  if (title_band_fits(SunGlobeViewControlWidth())) {
    ImGui::SameLine();
  }
  RenderSunGlobeViewControl(&view.sun_globe_view_mode);
  if (title_band_fits(InfoIconAdvance())) {
    ImGui::SameLine();
  }
  InfoIcon("globe_view_help",
           "Crystal-frame sun directions — a pose projection, not the full SO(3).\n"
           "Full: a fixed crystal-frame basis (centre +x, up +z — the c-axis vertical on "
           "screen); the same arc draws the same map at every picked point.\n"
           "Fit: centred on this component's first valid sample — the view follows the point.\n"
           "The deviation colour scale is auto-ranged per document; colours do not compare "
           "across results.");

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
  // Read once, before allocating the canvas: the legend's width claim on the canvas row is
  // known before the row is laid out. Only the discovered arm displays terrain; waiting/
  // withdrawn/mass documents show no old scale.
  LUMICE_PathSunGrid grid{};
  grid.struct_size = sizeof(grid);
  const bool have_grid =
      view.show_deviation_grid && valid_count > 0 && info && info->outcome != LUMICE_PATH_POINT_MASS &&
      PointQueryStateFor(point) == PointQueryState::kCompleted &&
      LUMICE_PathPointGetSunGrid(point.adopted_result.get(), &grid) == LUMICE_PATH_READ_OK && grid.cell_count > 0;
  const SunGridRange grid_range = have_grid ? SunGridDisplayRange(grid) : SunGridRange{};
  const float play_w =
      std::max(ImGui::CalcTextSize("Stop").x, ImGui::CalcTextSize("Play").x) + ImGui::GetStyle().FramePadding.x * 2.0f;
  const ImVec2 available = ImGui::GetContentRegionAvail();
  const bool shared_line = available.x - play_w - ImGui::GetStyle().ItemSpacing.x >= UiPx(80.0f);
  // The vertical legend left the height budget with its old horizontal layout: the column rides
  // the canvas row's right edge and its band absorbs the row's slack, so only the slider/play
  // row reserves height above the canvas. The column's WIDTH, by contrast, has first claim on
  // the row — the square canvas is what remains.
  const float controls_h = ImGui::GetFrameHeightWithSpacing() * (shared_line ? 1.0f : 2.0f);
  const float legend_w = have_grid ? SunGridLegendColumnWidth() + ImGui::GetStyle().ItemSpacing.x : 0.0f;
  const float graph_side =
      std::min(available.x - legend_w,
               CanvasHeightBeforeReport(available.y, controls_h, UiPx(64.0f), ImGui::GetStyle().ItemSpacing.y));
  const ImVec2 graph_min = ImGui::GetCursorScreenPos();
  ImDrawList* graph_draw_list = ImGui::GetWindowDrawList();
  if (graph_side > 0.0f) {
    ImGui::InvisibleButton("##globe_canvas", ImVec2(graph_side, graph_side));
    graph_draw_list->AddCircle(ImVec2(graph_min.x + graph_side * 0.5f, graph_min.y + graph_side * 0.5f),
                               std::max(1.0f, graph_side * 0.5f - UiPx(4.0f)), ImGui::GetColorU32(ImGuiCol_Border));
    if (have_grid) {
      ImGui::SameLine();
      RenderSunGridLegend(grid_range, graph_side);
    }
    // The legend column manages its own cursor; the row's end is the canvas's bottom either way
    // — the slider resumes from there, never from wherever the column's last wrapped line left
    // the flow.
    ImGui::SetCursorScreenPos(ImVec2(graph_min.x, graph_min.y + graph_side + ImGui::GetStyle().ItemSpacing.y));
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
      // Empty by design: query progress and refusal reasons have one owner, the window's fixed
      // footer — the pose projection's concept text rides the title's info icon.
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
        WarningTextWrapped(
            "Partial document (%s) — what arrived is on show; emptiness here is the run's "
            "stop, not a physical absence.",
            ExecutionText(info->execution));
      }
    };
    if (component_total == 0) {
      LUMICE_PathPointReach reach{};
      reach.struct_size = sizeof(reach);
      const bool have_reach = LUMICE_PathPointGetReach(point.adopted_result.get(), &reach) == LUMICE_PATH_READ_OK;
      render_state_area([&] {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
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
        draw_partial_note();
        ImGui::TextWrapped("This component carries no valid sample — nothing to project.");
        ImGui::PopStyleColor();
      });
      return;
    }
    if (graph_side > 0.0f) {
      // Full's fixed basis is a total function of nothing — no document read can fail it; Fit
      // anchors on the component's first valid sample (its ok=false arm is unreachable here,
      // the no-valid-sample state above already returned).
      SunGlobeProjector projector;
      bool have_projector = false;
      if (view.sun_globe_view_mode == SunGlobeViewMode::kFull) {
        projector = MakeSunGlobeFullProjector();
        have_projector = true;
      } else {
        have_projector = MakeSunGlobeProjector(*point.adopted_result, component, &projector);
      }

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
        DrawSunGridBackdrop(dl, grid, projector, grid_range, centre_px, radius_px);
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
    // ---- The state area (R4's scroll set): the description, the partial note, the weight note
    // and the sample readout — all reachable by scrolling, none deleted. The grid-absence note
    // moved with the toggle to the factors region's diagnostics group (§16.4); the
    // readout's old fits_on_line join is gone with the form: the state area is where these
    // lines live at every width. ----
    const std::optional<CurrentSample> current = CurrentSampleOf(*point.adopted_result, view, valid_indices);
    render_state_area([&] {
      if (partial_document) {
        WarningTextWrapped("Partial document (%s) — the fiber on show is what arrived.",
                           ExecutionText(info->execution));
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
  g_ray3d_frame.axes_drawn = false;
  g_ray3d_frame.legend_drawn = false;
  state.analysis.ray3d_fresnel_labels.clear();
  state.analysis.ray3d_fresnel_label_rects.clear();
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
  if (fits_on_line(ImGui::CalcTextSize("Reset View").x + ImGui::GetStyle().FramePadding.x * 2.0f)) {
    ImGui::SameLine();
  }
  if (ImGui::Button("Reset View##r3d_reset")) {
    view.ResetOrbitOrientation();
  }
  // The Fresnel toggle: a title-band session preference (moved out of the report head — the
  // report's fixed two-line budget carries status/qualifying facts only). The same survival
  // set as the other toggles (kept across sample/component switches, point adoptions and
  // window reopen; reset only by the whole-state new-document paths; never persisted, never
  // in a query key); default on — the ratio numbers are the trace's exact-value face. It sits
  // in the fixed control band, so toggling it moves no canvas geometry.
  const char* fresnel_item = "Show Fresnel ratios##r3d_fresnel";
  const float fresnel_w =
      ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(fresnel_item).x;
  if (fits_on_line(fresnel_w)) {
    ImGui::SameLine();
  }
  ImGui::Checkbox(fresnel_item, &view.show_fresnel_labels);
  // The region's one help carrier: usage note + display semantics (kRay3DHelpText). The shared
  // icon joins this line when it fits and takes its own fresh line at the wrap boundary — the
  // same complete-footprint rule as every control above, no compressed hit target.
  if (fits_on_line(InfoIconAdvance())) {
    InfoIcon("r3d_help", kRay3DHelpText);
  } else {
    InfoIconLine("r3d_help", kRay3DHelpText);
  }
  const ImVec2 remaining = ImGui::GetContentRegionAvail();
  const float graph_h =
      CanvasHeightBeforeReport(remaining.y, 0.0f, Ray3DReportQuota(), ImGui::GetStyle().ItemSpacing.y);
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
    const PointQueryState st = PointQueryStateFor(point);
    if (st == PointQueryState::kNoIntent) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
      ImGui::TextWrapped("Pick a class and a sky point; the current pose's crystal-interior path takes this region.");
      ImGui::PopStyleColor();
      return;
    }
    if (st == PointQueryState::kWaiting || st == PointQueryState::kCancelled || st == PointQueryState::kRefused) {
      DisabledWrappedText("No path on show.");
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
      WarningTextWrapped("Partial document (%s) — the path on show is what arrived.", ExecutionText(info->execution));
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
    const RayTraceKey current_key = RayTraceKeyFor(point.intent_id, *current);
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
    // The request's face sequence (the Info reader's own): one face number per contact by
    // contract. A failed read, a count mismatch or a null sequence falls back to the
    // facing-unknown vocabulary below — every real contact keeps its marker either way.
    LUMICE_RepresentativeRayInfo ray_info{};
    ray_info.struct_size = sizeof(ray_info);
    const bool faces_usable =
        display_state == RayTraceDisplayState::kDrawn &&
        LUMICE_RepresentativeRayGetInfo(trace.adopted_result.get(), &ray_info) == LUMICE_PATH_READ_OK &&
        ray_info.face_count == static_cast<int>(line.seg_kinds.size() - 1) && ray_info.faces != nullptr;
    // The labels' render feedback: one slot per segment, filled only where a label actually
    // drew this frame (the toggle's leg reads positions from the primitives and content here),
    // with 5 rect/placement floats beside each.
    state.analysis.ray3d_fresnel_labels.assign(line.seg_kinds.size(), std::string{});
    state.analysis.ray3d_fresnel_label_rects.assign(line.seg_kinds.size() * 5, 0.0f);

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

    // The frozen mesh is the ONLY framing anchor (2026-11-12 feedback #7): centre and the
    // normalization radius come from the mesh alone — the trace's display extensions (1.1x the
    // contacts' own spread, well outside the crystal) no longer inflate either. The old content
    // fit counted the polyline: a pose with light on show shrank its crystal, the waiting frame
    // filled it back — the reported size jump between the two states of ONE pose. The polyline
    // may now reach beyond the unit bounding sphere; the overlay below clips against the
    // camera's near plane instead of growing the fit to hide it.
    float lo[3] = { 0.0f, 0.0f, 0.0f };
    float hi[3] = { 0.0f, 0.0f, 0.0f };
    {
      bool first = true;
      for (int v = 0; v < cache.mesh.vertex_count; ++v) {
        float d[3];
        to_display(&cache.mesh.vertices[v * 3], d);
        if (first) {
          for (int k = 0; k < 3; ++k) {
            lo[k] = hi[k] = d[k];
          }
          first = false;
          continue;
        }
        for (int k = 0; k < 3; ++k) {
          lo[k] = std::min(lo[k], d[k]);
          hi[k] = std::max(hi[k], d[k]);
        }
      }
    }
    const float centre[3] = { (lo[0] + hi[0]) * 0.5f, (lo[1] + hi[1]) * 0.5f, (lo[2] + hi[2]) * 0.5f };
    float fit = 0.0f;
    for (int v = 0; v < cache.mesh.vertex_count; ++v) {
      float d[3];
      to_display(&cache.mesh.vertices[v * 3], d);
      const float dx = d[0] - centre[0];
      const float dy = d[1] - centre[1];
      const float dz = d[2] - centre[2];
      fit = std::max(fit, std::sqrt(dx * dx + dy * dy + dz * dz));
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
        // Zoom is the focal-plane half-height shared by Render, ComputeMvp and the frame info
        // (the overlay's near-plane clip and the frame tests' camera both derive from it);
        // compensate portrait aspect to preserve the old short-side fit. At zoom=1, a
        // focal-plane radius spans 42% of the short side, matching the old fit. A positive
        // stand-off keeps the eye outside the unit sphere at every zoom without a clamp
        // plateau; wheel magnification remains strictly monotone across the full range.
        constexpr float kFitHalfHeight = 0.5f / 0.42f;
        constexpr float kCameraStandOff = 0.3f;
        const float zoom =
            (kCameraStandOff + (kFitHalfHeight - kCameraStandOff) / std::clamp(view.orbit_zoom, 0.25f, 8.0f)) *
            std::max(1.0f, static_cast<float>(height) / width);

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
        rendered.zoom = zoom;
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
        gl.renderer.Render(kIdentity, zoom, CrystalStyle::kXRay, view_rotation);
        // Use the ACTUAL integer FBO dimensions on both sides. Its aspect only differs from the
        // logical canvas by pixel quantization; both texture and overlay undergo the same stretch.
        CrystalRenderer::ComputeMvp(kIdentity, zoom, width, height, rendered.mvp, view_rotation);
        rendered.valid = true;
        ImDrawList* dl = graph_draw_list;
        dl->PushClipRect(canvas_min, canvas_max, true);
        ImDrawListSplitter axes_layer;
        axes_layer.Split(dl, 2);
        axes_layer.SetCurrentChannel(dl, 1);
        DrawRay3DAxes(dl, view_rotation, rendered);
        axes_layer.SetCurrentChannel(dl, 0);
        // AddImage is the Image quad without a second layout item: the existing InvisibleButton
        // remains the sole canvas interaction target, at its existing ##r3d_canvas identity.
        dl->AddImage(static_cast<ImTextureID>(rendered.texture_id), canvas_min, canvas_max, ImVec2(0, 1), ImVec2(1, 0));
        // The overlay draws in scene coordinates with the REAL camera: the eye sits on the
        // -fwd axis at ComputeDist, the near plane is ComputeNearPlane's — both the same
        // formulas ComputeMvp builds the frustum from. With the mesh-only fit the trace's
        // display extensions reach beyond the unit bounding sphere and can cross the near
        // plane at high zoom, so every primitive clips first: a segment draws its visible
        // part only, a behind-eye point never projects.
        const float near_w = CrystalRenderer::ComputeNearPlane(zoom);
        const float eye_dist = CrystalRenderer::ComputeDist(zoom);
        const float eye_scene[3] = { static_cast<float>(-eye_dist * fwd[0]), static_cast<float>(-eye_dist * fwd[1]),
                                     static_cast<float>(-eye_dist * fwd[2]) };
        const auto scene_of = [&](const float p[3]) {
          float d[3];
          point_to_display(p, d);
          return ToRay3DScene(d, rendered);
        };
        // The canvas legend's reserved footprint: computed and decided BEFORE the per-segment
        // label ladder runs — the ladder's hard exclusion reads it live (the compass's mirror;
        // the compass is live for the same structural reason, DrawRay3DAxes runs on channel 1
        // ahead of this loop). Only a drawn trace wants it — a static colour legend over an
        // empty canvas would read as a path being present. Complete footprints or nothing: a
        // clipped partial legend lies about what it names (the compass's own gate). The ink
        // emission itself stays after the contacts and sun arrow; the rects ride the frame.
        const bool legend_wanted = display_state == RayTraceDisplayState::kDrawn;
        if (legend_wanted) {
          const float swatch_side = UiPx(kRay3DLegendSwatchUi);
          const float swatch_gap_x = UiPx(kRay3DLegendSwatchGapUi);
          const float unit_gap_y = UiPx(3.0f);
          const float legend_inset = UiPx(4.0f);
          const char* const(&legend_labels)[3] = kRay3DLegendLabels;
          const float legend_line_h = ImGui::GetTextLineHeight();
          const float legend_h = 3.0f * legend_line_h + 2.0f * unit_gap_y;
          float legend_w = 0.0f;
          for (const char* label : legend_labels) {
            legend_w = std::max(legend_w, swatch_side + swatch_gap_x + ImGui::CalcTextSize(label).x);
          }
          rendered.legend_drawn =
              canvas_size.x >= legend_w + legend_inset * 2.0f && canvas_size.y >= legend_h + legend_inset * 2.0f;
          if (rendered.legend_drawn) {
            const ImVec2 legend_min(canvas_min.x + legend_inset, canvas_min.y + legend_inset);
            rendered.legend_min = legend_min;
            rendered.legend_max = ImVec2(legend_min.x + legend_w, legend_min.y + legend_h);
            for (int kind = 0; kind < 3; ++kind) {
              const ImVec2 unit_min(legend_min.x,
                                    legend_min.y + static_cast<float>(kind) * (legend_line_h + unit_gap_y));
              const float text_w = ImGui::CalcTextSize(legend_labels[kind]).x;
              rendered.legend_unit_min[kind] = unit_min;
              rendered.legend_unit_max[kind] =
                  ImVec2(unit_min.x + swatch_side + swatch_gap_x + text_w, unit_min.y + legend_line_h);
            }
          }
        }
        std::vector<ImRect> placed_label_rects;  // the labels' placed text rects, in order
        for (size_t s = 0; s < line.seg_kinds.size(); ++s) {
          const float* p0 = &line.points[s * 6];
          const float* p1 = p0 + 3;
          const auto s0 = scene_of(p0);
          const auto s1 = scene_of(p1);
          ImVec2 pa;
          ImVec2 pb;
          if (!RayTraceClipSegmentToPixel(s0.data(), s1.data(), near_w, rendered, &pa, &pb)) {
            continue;  // the whole segment sits behind the near plane — nothing to draw
          }
          const RayTraceSegmentEnergy& energy = line.seg_energy[s];
          // Width follows the cumulative Fresnel relative energy (the sqrt visual mapping); a
          // segment past an unusable interface factor draws a fixed neutral line — no energy
          // claim — and an exact zero draws no ink at all: the contact markers carry those
          // spots, the labels (when shown) read 0%.
          const bool draw_ink = !energy.valid || energy.value > 0.0;
          if (draw_ink) {
            const float width = energy.valid ? RayTraceEnergyWidth(energy.value) : UiPx(kRayTraceWidthNeutralUi);
            dl->AddLine(pa, pb, SegmentColor(line.seg_kinds[s], 1.0f), width);
          }
          if (view.show_fresnel_labels) {
            // The ratio label rides the VISIBLE part's midpoint, offset off the line and
            // clamped into the canvas — the anchor stays a real visible position. Visibility
            // is the REAL segment-rect clip (an AABB overlap is only necessary: a segment
            // skimming past a corner must take no fabricated edge slot).
            double vis_t0 = 0.0;
            double vis_t1 = 1.0;
            const ImVec2 canvas_max(canvas_min.x + canvas_size.x, canvas_min.y + canvas_size.y);
            const bool on_canvas = RayTraceSegmentOnCanvas(pa, pb, canvas_min, canvas_max, &vis_t0, &vis_t1);
            if (on_canvas && vis_t1 > vis_t0) {
              const std::string text = RayTraceEnergyLabelText(energy);
              const ImVec2 text_size = ImGui::CalcTextSize(text.c_str());
              const ImVec2 mid(pa.x + static_cast<float>((pb.x - pa.x) * (vis_t0 + vis_t1) * 0.5),
                               pa.y + static_cast<float>((pb.y - pa.y) * (vis_t0 + vis_t1) * 0.5));
              ImVec2 dir(pb.x - pa.x, pb.y - pa.y);
              const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
              if (len > 1e-3f) {
                dir.x /= len;
                dir.y /= len;
              } else {
                dir.x = 0.0f;
                dir.y = -1.0f;
              }
              // The collision ladder: labels stay individually readable (AC3) — a placed text
              // rect may overlap NO already-placed rect. Candidates: the default offset, then
              // the opposite side, then stepped distances on both sides. A label every
              // candidate of which still collides takes the least-colliding candidate and a
              // short leader line back to its segment's visible midpoint — the ratio is never
              // hidden, and the leader keeps the segment identity the plan forbids losing.
              int placement = 2;
              int candidate_index = 0;
              bool placed = false;
              float best_overlap = -1.0f;
              ImVec2 best_anchor;
              ImVec2 anchor;
              for (const float dist : { UiPx(7.0f), UiPx(15.0f), UiPx(23.0f) }) {
                for (const float side : { 1.0f, -1.0f }) {
                  ImVec2 candidate(mid.x + dir.y * side * dist, mid.y - dir.x * side * dist);
                  // The ladder's per-candidate clamp is the ONE shared label clamp
                  // (util/label_viewport_clamp.hpp via the ImVec2 wrapper in overlay_labels.hpp)
                  // over the canvas rectangle INTERSECTED WITH the draw list's effective clip —
                  // the visible-intersection viewport the map's readout labels clamp to
                  // (contract §16.1.3: this window writes no second clamp). The wrapper takes
                  // the viewport as given (it never intersects); the intersection is this
                  // site's responsibility. Behavior deltas against the hand copy, per the
                  // plan's AC6 list: the y inset 1 pt becomes the shared 2 pt, the degenerate
                  // axis is left untouched instead of pinned to the lower edge, and the
                  // viewport is the intersection — which the as-built layout always makes
                  // equal to the canvas (the visible-budget rule sizes every region inside
                  // each ancestor clip), so the clause is defense in depth.
                  const ImVec2 clip_min = dl->GetClipRectMin();
                  const ImVec2 clip_max = dl->GetClipRectMax();
                  const float vis_min_x = std::max(canvas_min.x, clip_min.x);
                  const float vis_min_y = std::max(canvas_min.y, clip_min.y);
                  const float vis_max_x = std::min(canvas_max.x, clip_max.x);
                  const float vis_max_y = std::min(canvas_max.y, clip_max.y);
                  candidate = detail::ClampLabelPosToViewport(candidate, text_size, vis_min_x, vis_min_y,
                                                              vis_max_x - vis_min_x, vis_max_y - vis_min_y);
                  // The compass is a HARD exclusion, including the least-overlap fallback.
                  // A colliding candidate moves into the left viewport before label scoring;
                  // unlike a finite penalty, this cannot lose to a crowded label cluster.
                  if (rendered.axes_drawn &&
                      ImRect(candidate, ImVec2(candidate.x + text_size.x, candidate.y + text_size.y))
                          .Overlaps(ImRect(rendered.axes_min, rendered.axes_max))) {
                    candidate = detail::ClampLabelPosToViewport(candidate, text_size, vis_min_x, vis_min_y,
                                                                rendered.axes_min.x - vis_min_x, vis_max_y - vis_min_y);
                  }
                  // The legend is the left edge's hard exclusion, the compass's mirror: a
                  // colliding candidate moves right of the reserved footprint before scoring —
                  // also before the least-overlap fallback, so the degraded form cannot land
                  // back on the legend either. The band's right edge yields to the compass's
                  // bound too, so while the band fits the label the two exclusions commute — a
                  // candidate colliding with either zone cannot land in the other. Below that
                  // fit the shared clamp's degenerate branch leaves the axis untouched (the
                  // same honest degradation the compass exclusion has always had); that domain
                  // is unreachable at every legal canvas — the path is mesh-anchored at the
                  // centre while both zones sit in corners ~240 px out — so the premise rides
                  // on the canvas gates' joint, not on this site.
                  if (rendered.legend_drawn &&
                      ImRect(candidate, ImVec2(candidate.x + text_size.x, candidate.y + text_size.y))
                          .Overlaps(ImRect(rendered.legend_min, rendered.legend_max))) {
                    const float legend_band_right =
                        rendered.axes_drawn ? std::min(vis_max_x, rendered.axes_min.x - text_size.x) : vis_max_x;
                    candidate = detail::ClampLabelPosToViewport(candidate, text_size, rendered.legend_max.x, vis_min_y,
                                                                legend_band_right - rendered.legend_max.x,
                                                                vis_max_y - vis_min_y);
                  }
                  const ImRect candidate_rect(candidate, ImVec2(candidate.x + text_size.x, candidate.y + text_size.y));
                  float overlap = 0.0f;
                  for (const ImRect& placed_rect : placed_label_rects) {
                    ImRect common = candidate_rect;
                    common.ClipWithFull(placed_rect);
                    if (common.Min.x < common.Max.x && common.Min.y < common.Max.y) {
                      overlap += common.GetWidth() * common.GetHeight();
                    }
                  }
                  if (overlap <= 0.0f) {
                    anchor = candidate;
                    placement = candidate_index == 0 ? 0 : 1;  // 0 = the default offset, 1 = shifted
                    placed = true;
                    break;
                  }
                  if (best_overlap < 0.0f || overlap < best_overlap) {
                    best_overlap = overlap;
                    best_anchor = candidate;
                  }
                  ++candidate_index;
                }
                if (placed) {
                  break;
                }
              }
              if (!placed && rendered.axes_drawn) {
                // The corner reservation can collapse multiple near-segment candidates to
                // one slot. Search the remaining visible canvas before accepting overlap.
                const ImVec2 clip_min = dl->GetClipRectMin();
                const ImVec2 clip_max = dl->GetClipRectMax();
                const float inset = UiPx(2.0f);
                float nearest = std::numeric_limits<float>::max();
                for (float y = std::max(canvas_min.y, clip_min.y) + inset;
                     y + text_size.y + inset <= std::min(canvas_max.y, clip_max.y); y += text_size.y + inset) {
                  for (float x = std::max(canvas_min.x, clip_min.x) + inset;
                       x + text_size.x + inset <= std::min(canvas_max.x, clip_max.x); x += text_size.x + inset) {
                    const ImRect rect(ImVec2(x, y), ImVec2(x + text_size.x, y + text_size.y));
                    if (rect.Overlaps(ImRect(rendered.axes_min, rendered.axes_max)) ||
                        (rendered.legend_drawn && rect.Overlaps(ImRect(rendered.legend_min, rendered.legend_max))) ||
                        std::any_of(placed_label_rects.begin(), placed_label_rects.end(),
                                    [&](const ImRect& other) { return other.Overlaps(rect); })) {
                      continue;
                    }
                    const float distance = std::hypot(x - mid.x, y - mid.y);
                    if (distance < nearest) {
                      nearest = distance;
                      anchor = ImVec2(x, y);
                      placed = true;
                    }
                  }
                }
              }
              if (!placed) {
                anchor = best_anchor;  // the leader fallback — the least-colliding slot plus a leader
              }
              if (placement != 0) {
                // The leader: a short line from the text's near edge to the visible midpoint —
                // every non-default placement keeps its segment identity explicit (proximity
                // alone cannot carry it once the ladder has moved the text).
                ImVec2 from(anchor.x + text_size.x * 0.5f, anchor.y + text_size.y * 0.5f);
                ImVec2 to(mid);
                ImVec2 step(to.x - from.x, to.y - from.y);
                const float gap = UiPx(3.0f);
                const float step_len = std::sqrt(step.x * step.x + step.y * step.y);
                if (step_len > gap) {
                  step.x *= gap / step_len;
                  step.y *= gap / step_len;
                  to.x -= step.x;
                  to.y -= step.y;
                }
                dl->AddLine(from, to, kRayTraceLabelColor, 1.0f);
              }
              placed_label_rects.push_back(ImRect(anchor, ImVec2(anchor.x + text_size.x, anchor.y + text_size.y)));
              float* rect_feedback = &state.analysis.ray3d_fresnel_label_rects[s * 5];
              rect_feedback[0] = placed_label_rects.back().Min.x;
              rect_feedback[1] = placed_label_rects.back().Min.y;
              rect_feedback[2] = placed_label_rects.back().Max.x;
              rect_feedback[3] = placed_label_rects.back().Max.y;
              rect_feedback[4] = static_cast<float>(placement);
              dl->AddText(anchor, kRayTraceLabelColor, text.c_str());
              state.analysis.ray3d_fresnel_labels[s] = text;
            }
          }
        }
        // The contacts: one marker per REAL contact of the recovery. The polyline's point layout is
        // [entry_start, c0, c0, c1, ..., cl, exit_end] — odd indices 1..2N-1 are the N contacts, index
        // 0 the entry extension's start and the LAST index the exit extension's end, both this TU's
        // display synthesis and marked by nothing. A contact whose face looks at the real camera
        // (the face's outward normal, display-rotated, dotted against contact->eye in the scene
        // metric — never a global fwd approximation) draws solid; back-facing and facing-unknown
        // draw hollow. The legend carries the vocabulary; unknown never masquerades as back-facing.
        for (size_t idx = 1; idx + 1 < line.points.size() / 3; idx += 2) {
          const size_t contact_index = (idx - 1) / 2;
          const float* p = &line.points[idx * 3];
          const auto s = scene_of(p);
          const std::array<float, 4> clip = ToRay3DClip(s.data(), rendered);
          if (clip[3] < near_w) {
            continue;  // a contact marker never projects past the near plane
          }
          bool solid = false;
          if (faces_usable) {
            const RayTraceContactFaceNormal face =
                RayTraceContactFaceNormalFor(cache.mesh, ray_info.faces[contact_index]);
            if (face.known) {
              float n_display[3];
              to_display(face.normal_body, n_display);
              solid = RayTraceContactFacesEye(n_display, s.data(), eye_scene);
            }
          }
          const ImVec2 pos = RayTraceClipToPixel(clip.data(), rendered);
          if (solid) {
            dl->AddCircleFilled(pos, UiPx(2.5f), kRayTraceContactSolidColor);
          } else {
            dl->AddCircle(pos, UiPx(2.5f), kRayTraceContactColor, 0, 1.0f);
          }
        }
        // The crystal-frame sun direction: one short arrow along u radiating from the crystal's own
        // centre (the crystal view's own anchor; the world view's entry leg already carries the
        // incident). The tip rides the mesh fit's scale, the anchor does not.
        if (display_state == RayTraceDisplayState::kDrawn && view.crystal_frame) {
          const float u[3] = { static_cast<float>(current->sample.sun_in_crystal[0]),
                               static_cast<float>(current->sample.sun_in_crystal[1]),
                               static_cast<float>(current->sample.sun_in_crystal[2]) };
          const float tip[3] = { centre[0] + u[0] * fit * 0.6f, centre[1] + u[1] * fit * 0.6f,
                                 centre[2] + u[2] * fit * 0.6f };
          // centre/tip are ALREADY display points (the crystal view's mesh centre, and the
          // tip built from the body-frame sun direction in that same view): normalize
          // directly. scene_of would apply the frame switch's point transform a SECOND time
          // (an extra inverse rotation that bends the arrow off the true sun direction).
          const auto a0 = ToRay3DScene(centre, rendered);
          const auto a1 = ToRay3DScene(tip, rendered);
          ImVec2 pa;
          ImVec2 pb;
          if (RayTraceClipSegmentToPixel(a0.data(), a1.data(), near_w, rendered, &pa, &pb)) {
            dl->AddLine(pa, pb, ImGui::ColorConvertFloat4ToU32(AccentColor()), 1.5f);
          }
        }
        // The segment legend's INK: emitted at the reserved rects computed before the label
        // ladder ran (the footprint itself decided there, kDrawn-gated — see the hoisted block
        // above the per-segment loop). The colour vocabulary lives beside the light it names
        // (2026-11-17 feedback), not in the report.
        if (rendered.legend_drawn) {
          const float swatch_side = UiPx(kRay3DLegendSwatchUi);
          const float swatch_gap_x = UiPx(kRay3DLegendSwatchGapUi);
          const char* const(&legend_labels)[3] = kRay3DLegendLabels;
          for (int kind = 0; kind < 3; ++kind) {
            const ImVec2 unit_min = rendered.legend_unit_min[kind];
            const float unit_h = rendered.legend_unit_max[kind].y - unit_min.y;
            const float swatch_y = unit_min.y + (unit_h - swatch_side) * 0.5f;
            dl->AddRectFilled(ImVec2(unit_min.x, swatch_y), ImVec2(unit_min.x + swatch_side, swatch_y + swatch_side),
                              SegmentColor(kind, 1.0f));
            dl->AddText(ImVec2(unit_min.x + swatch_side + swatch_gap_x, unit_min.y),
                        ImGui::GetColorU32(ImGuiCol_TextDisabled), legend_labels[kind]);
          }
        }
        axes_layer.Merge(dl);
        dl->PopClipRect();
      };
      render_canvas();
    }
    // The current pose's crystal remains visible even when its trace has no drawable path.
    // Transient vs persistent (2026-11-17 feedback): computing and hidden never cycle through
    // the report — the status bar owns the transient words. Computing shows the RETAINED
    // record (the identity-checked source line and, for a retained failure, its details) so
    // the report's content and height hold still while the trace re-arms; without a retained
    // source in THIS domain, the honest empty line. Hidden means the session itself was
    // dropped — a reset or a withdrawn intent, exactly the source-invalidating events — so it
    // shows the honest empty line, never a stale report. Error/refused are the current key's
    // persistent facts: the live details render here AND are captured into the record, so the
    // next sample switch's transient retains them as the named previous result. The drawn arm
    // captures its own source line the same way.
    const auto capture_record = [&](Ray3DReportRecord::Kind kind, const char* detail_head, std::string detail_body) {
      Ray3DReportRecord& record = state.analysis.ray3d_report_record;
      record.valid = true;
      record.point_id = point.intent_id;
      record.component = current->component;
      record.kind = kind;
      record.source_line = Ray3DReportSourceLine(
          current->component, std::clamp(view.sample_index, 0, current->valid_count - 1) + 1, current->valid_count);
      record.detail_head = detail_head == nullptr ? std::string() : std::string(detail_head);
      record.detail_body = std::move(detail_body);
    };
    const auto render_record = [&](const Ray3DReportRecord& record) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
      ImGui::TextWrapped("%s", record.source_line.c_str());
      ImGui::PopStyleColor();
      RegisterPointTextItem("##r3d_source");
      if (!record.detail_head.empty()) {
        ImGui::TextUnformatted(record.detail_head.c_str());
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        ImGui::TextWrapped("%s", record.detail_body.c_str());
        ImGui::PopStyleColor();
      }
    };
    switch (display_state) {
      case RayTraceDisplayState::kHidden:
        DisabledWrappedText("No representative path asked for yet.");
        return;
      case RayTraceDisplayState::kComputing: {
        const Ray3DReportRecord& record = state.analysis.ray3d_report_record;
        if (record.valid && record.point_id == point.intent_id && record.component == current->component) {
          render_record(record);  // the retained form — stale-honest, source-named
        } else {
          DisabledWrappedText("No representative path shown yet.");
        }
        return;
      }
      case RayTraceDisplayState::kError: {
        capture_record(Ray3DReportRecord::Kind::kError, "Path query failed:", trace.adopted_error);
        render_record(state.analysis.ray3d_report_record);
        return;
      }
      case RayTraceDisplayState::kRefused: {
        LUMICE_RepresentativeRayInfo rinfo{};
        rinfo.struct_size = sizeof(rinfo);
        LUMICE_RepresentativeRayGetInfo(trace.adopted_result.get(), &rinfo);
        std::string refused_detail =
            "reason: " + PathTextToString(rinfo.reason) + " | entry status: " + PathTextToString(rinfo.entry_status);
        if (rinfo.chain_failure.length > 0) {
          refused_detail += " | chain: " + PathTextToString(rinfo.chain_failure);
        }
        char area_buf[96];
        std::snprintf(area_buf, sizeof(area_buf), " | corridor area %.3e (threshold %.3e)", rinfo.area_perp_internal,
                      rinfo.area_threshold);
        refused_detail += area_buf;
        capture_record(Ray3DReportRecord::Kind::kRefused, "No legal path for this pose:", std::move(refused_detail));
        render_record(state.analysis.ray3d_report_record);
        return;
      }
      case RayTraceDisplayState::kDrawn:
        capture_record(Ray3DReportRecord::Kind::kDrawn, nullptr, std::string());
        render_record(state.analysis.ray3d_report_record);
        return;
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
constexpr int kCurveArms = static_cast<int>(std::size(kCurveArmFields));

// Below the smallest honest plot the canvas keeps a recoverable note and click-to-select.
constexpr float kPoseViewCurveMinPlot = 12.0f;

// Curve-local display: shared log axis, real arclength, three-state availability, seed and the
// shared selected-pose cursor. The fixed header owns Fit/Full. The companion owns identity,
// integral/completeness, caliber, symbolic legend and partial limitation; hover lives only in
// the canvas/tooltip, while the factors region owns current-pose explanations.
void RenderPoseCurveRegion(GuiState& state, const ImVec2& size) {
  ImGui::BeginChild("##AnalyzePoseCurve", size, ImGuiChildFlags_Borders);
  PointQuerySession& point = state.analysis.point;
  PointViewState& view = state.analysis.point_view;
  // Fixed header: the range switch does not participate in either scrolling child.
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted("Contribution curve");
  ImGui::SameLine();
  RenderPointCurveRangeControl(&view.curve_range_mode);

  // ---- D7 — the curve region's canvas | companion split (§16.4): the canvas's share of the
  // span below the fixed head. The sentinel share (never dragged) grants the companion its
  // fixed line quota and the canvas eats the whole delta — the resize strategy's
  // elastic|quota pair, the quota a budget of LINES (live-measured height), never content
  // height (§15: lines beyond it scroll, none is swallowed). A drag writes the dragged share,
  // and from then on the stored share governs — user intent overrides the strategy — clamped
  // at the canvas's real-slice floor and the companion's floor (the quota compresses from its
  // floor first, the shrink ladder's reverse symmetry). Below the canvas floor's own reach the
  // canvas yields entirely: the region's honest scroll form, recoverable by growing back. ----
  const ImVec2 available = ImGui::GetContentRegionAvail();
  const float strip_7 = UiPx(kAnalyzeSplitterSize);
  const float v_spacing = ImGui::GetStyle().ItemSpacing.y;
  const float denom_7 = std::max(0.0f, available.y - strip_7 - v_spacing * 2.0f);
  const float canvas_floor_7 = UiPx(kPoseViewCurveCanvasFloor);
  const float companion_floor_7 = UiPx(kPoseViewCurveCompanionFloor);
  const float companion_quota_7 =
      static_cast<float>(kPoseViewCurveCompanionLines) * ImGui::GetTextLineHeightWithSpacing();
  float graph_h = 0.0f;
  if (denom_7 >= canvas_floor_7) {
    if (state.analyze_pose_view_curve_canvas_ratio > kPoseViewCurveCanvasRatioUndragged) {
      const float upper = std::max(canvas_floor_7, denom_7 - companion_floor_7);
      graph_h = std::clamp(state.analyze_pose_view_curve_canvas_ratio * denom_7, canvas_floor_7, upper);
    } else {
      graph_h = std::max(canvas_floor_7, denom_7 - companion_quota_7);
    }
  }
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
  if (graph_h > 0.0f) {
    AnalyzeSplitter("##PoseCurveSplitter", SplitterDragAxis::kUpDown, available.x, graph_h, denom_7, canvas_floor_7,
                    denom_7 - companion_floor_7, canvas_floor_7 / std::max(1.0f, denom_7),
                    &state.analyze_pose_view_curve_canvas_ratio);
  }
  ImGui::BeginChild("##PoseCurveScroll", RemainingReportSize());
  const auto render_content = [&] {
    const PointQueryState st = PointQueryStateFor(point);
    if (st == PointQueryState::kNoIntent) {
      DisabledWrappedText("The contribution curve takes this region once a point result is in.");
      return;
    }
    if (st != PointQueryState::kCompleted) {
      return;  // Query lifecycle text lives only in the window footer.
    }
    const std::optional<LUMICE_PathPointInfo> info = ReadPointInfo(*point.adopted_result);
    if (!info.has_value()) {
      DisabledWrappedText("The adopted point document did not answer its Info reader.");
      return;
    }
    if (info->outcome == LUMICE_PATH_POINT_MASS) {
      DisabledWrappedText("A rank-0 point mass has no fiber curve — the details carry the mass numbers.");
      return;
    }
    std::vector<PointComponentRow> rows;
    const int component_total = PointComponentRows(*point.adopted_result, &rows);
    if (component_total == 0) {
      LUMICE_PathPointReach reach{};
      reach.struct_size = sizeof(reach);
      const bool have_reach = LUMICE_PathPointGetReach(point.adopted_result.get(), &reach) == LUMICE_PATH_READ_OK;
      if (info->execution != LUMICE_PATH_OUTCOME_COMPLETED) {
        WarningTextWrapped(
            "Partial document (%s) — what arrived is on show; emptiness here is the run's "
            "stop, not a physical absence.",
            ExecutionText(info->execution));
      }
      if (have_reach && !reach.target_in_range) {
        DisabledWrappedText(
            "No fiber here: the target's deviation is outside the range of this path's deflection field.");
      } else {
        DisabledWrappedText("No fiber component at this point — nothing to draw a curve along.");
      }
      return;
    }
    const int component = std::clamp(view.component_index, 0, component_total - 1);
    PointContributionCurve curve;
    BuildPointContributionCurve(*point.adopted_result, component, &curve);
    if (!curve.ok || curve.valid_positions.empty()) {
      if (info->execution != LUMICE_PATH_OUTCOME_COMPLETED) {
        WarningTextWrapped("Partial document (%s) — the curve on show is what arrived.",
                           ExecutionText(info->execution));
      }
      DisabledWrappedText("This component carries no valid sample — nothing to draw a curve along.");
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
    DisabledWrappedText("%s", title);
    RegisterPointTextItem("##curve_identity");
    // J7's constant: the caliber is DECLARED, never inferred — on plate-family scenes the axis
    // density (the `rho` field, and the product and the integral with it) reads identically zero
    // in this convention, while the other arms stay informative per their own flags (audit §4).
    DisabledWrappedText("angle-Lebesgue · arclength axis in rad");
    RegisterPointTextItem("##curve_caliber");

    // ---- The legend: one swatch + short symbol per arm, the units in the factor readouts' own
    // vocabulary; an arm with no positive value names the fact (J4's legend naming). SameLine is
    // a judgment (the 3D legend's rule): an entry wraps below when it no longer fits. ----
    const ImU32* const arm_colors = kPointCurveArmColors;  // the one palette (the tests read it too)
    const char* arm_labels[kCurveArms] = { "ρ", "A/a²", "T", "K", "ρ·A/a²·T·K" };
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
        DrawLegendIconSlot([&] { DrawSwatchSquare(arm_colors[arm], swatch); }, swatch, swatch);
        ImGui::TextUnformatted(label.c_str());
        char probe[32];
        std::snprintf(probe, sizeof(probe), "##curve_legend_%d", arm);
        RegisterPointTextItem(probe);
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
      const char* const* names = arm_labels;
      for (int arm = 0; arm < kCurveArms; ++arm) {
        out += "\n";
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
      const PointCurveAxisRange visible = PointCurveAxisFit(curve, view.curve_range_mode);
      const float line_h = ImGui::GetTextLineHeight();
      const float summary_wrap = std::max(1.0f, avail.x - UiPx(12.0f));
      const double v_min = visible.lo;
      const double v_max = visible.hi;
      const bool have_range = visible.valid && x_hi > 0.0;
      const float pad_b = UiPx(5.0f) + line_h;
      const float rough_plot_h = std::max(1.0f, canvas_h - UiPx(10.0f) - pad_b);
      const float real_plot_h = canvas_h - UiPx(10.0f) - pad_b;
      const bool plot_fits = real_plot_h >= UiPx(kPoseViewCurveMinPlot);
      const int label_step = have_range ? PointCurveLogLabelStep(v_min, v_max, rough_plot_h, line_h) : 1;
      float widest_lab = 0.0f;
      bool have_labelled_major = false;
      if (have_range) {
        for (const PointCurveLogTick& tick : PointCurveLogTicks(v_min, v_max)) {
          const int exp10 = static_cast<int>(std::lround(std::log10(tick.value)));
          if (!tick.major || exp10 % label_step != 0) {
            continue;
          }
          have_labelled_major = true;
          char lab[16];
          std::snprintf(lab, sizeof(lab), "%.0e", tick.value);
          widest_lab = std::max(widest_lab, ImGui::CalcTextSize(lab).x);
        }
        if (!have_labelled_major) {
          // A narrow data window can sit strictly between decade majors — no labelled tick, no
          // numeric reference. The window's own boundary values become the axis's labels (§3.2),
          // and the left gutter must host them exactly like the decade labels they stand in for.
          for (const double boundary : { v_min, v_max }) {
            char lab[24];
            std::snprintf(lab, sizeof(lab), "%.3g", boundary);
            widest_lab = std::max(widest_lab, ImGui::CalcTextSize(lab).x);
          }
        }
      }
      const float pad_l = std::max(UiPx(34.0f), widest_lab + UiPx(10.0f));  // decade labels live left of the plot
      const float pad_r = UiPx(10.0f);
      const float pad_t = UiPx(10.0f);  // the seed mark rides above the plot
      // Only the X labels remain below the plot; the seed and rad unit sit above.
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
      const auto v_to_py = [&](double v) { return plot_max.y - static_cast<float>(CurveLogY(v, visible) * plot_h); };
      if (plot_fits && have_range) {
        DrawPointCurveLogAxis(dl, visible, plot_min, plot_max);
      }
      // X labels own their row regardless of the data range.
      const auto x_label = [&](float x, float y, const char* text) {
        dl->AddText(ImVec2(x, y), ImGui::GetColorU32(ImGuiCol_TextDisabled), text);
      };
      if (plot_fits) {
        dl->AddRect(plot_min, plot_max, kPointCurvePlotBorderColor, 0.0f, 0, 1.0f);
        if (x_hi > 0.0) {
          char lab[24];
          const char* fmt = x_hi < 10.0 ? "%.2f" : (x_hi < 100.0 ? "%.1f" : "%.0f");
          const float axis_lab_y = plot_max.y + UiPx(5.0f);
          std::snprintf(lab, sizeof(lab), fmt, 0.0);
          x_label(plot_min.x - ImGui::CalcTextSize(lab).x * 0.5f, axis_lab_y, lab);
          std::snprintf(lab, sizeof(lab), fmt, x_hi * 0.5);
          x_label(x_to_px(x_hi * 0.5) - ImGui::CalcTextSize(lab).x * 0.5f, axis_lab_y, lab);
          std::snprintf(lab, sizeof(lab), fmt, x_hi);
          x_label(plot_max.x - ImGui::CalcTextSize(lab).x, axis_lab_y, lab);
        }
      }
      {
        const char* unit = "rad";
        dl->AddText(ImVec2(canvas_max.x - ImGui::CalcTextSize(unit).x - UiPx(6.0f), canvas_min.y + UiPx(4.0f)),
                    ImGui::GetColorU32(ImGuiCol_TextDisabled), unit);
      }

      if (plot_fits && have_range) {
        DrawPointCurveSeed(dl, curve, plot_min, plot_max);
        DrawPointCurveArms(dl, curve, visible, plot_min, plot_max);
        // The cursor: the CURRENT sample's arclength — the same selection the slider, the globe
        // marker and the 3D trace follow (AC4's one selection).
        const int cursor_stored = PointCurveStoredAt(curve, std::clamp(view.sample_index, 0, valid_count - 1));
        if (cursor_stored >= 0) {
          const float cpx = x_to_px(curve.points[static_cast<size_t>(cursor_stored)].x_rad);
          dl->AddLine(ImVec2(cpx, plot_min.y), ImVec2(cpx, plot_max.y), ImGui::ColorConvertFloat4ToU32(AccentColor()),
                      1.5f);
        }
      } else if (!have_range) {
        const char* note = "no positive value on this component — the log axis has no range";
        const ImVec2 ts = ImGui::CalcTextSize(note);
        dl->AddText(ImVec2(canvas_min.x + (avail.x - ts.x) * 0.5f, canvas_min.y + (canvas_h - ts.y) * 0.5f),
                    ImGui::GetColorU32(ImGuiCol_TextDisabled), note);
      } else {
        // No inverted plot or cropped labels: growing the splitter/window restores the plot.
        // Selection remains available through the width-defined X mapping below.
        const char* note = "the canvas is too short for the curve plot — grow it back with the splitter or the window";
        const ImVec2 ts = ImGui::CalcTextSize(note, nullptr, false, summary_wrap);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(canvas_min.x + UiPx(8.0f), canvas_min.y + (canvas_h - ts.y) * 0.5f),
                    ImGui::GetColorU32(ImGuiCol_TextDisabled), note, nullptr, summary_wrap);
      }

      // The canvas writes the ONE selection: click or drag maps the mouse's x to the nearest
      // VALID sample (invalid points are unreachable, the slider's domain) and stops the
      // playback — the slider's own write discipline, no second cursor state (J3). The write
      // SURVIVES the degraded form: the x mapping is width-defined (it never touches the
      // inverted dimension) and the selection is the one global state the globe, the 3D trace
      // and the factors follow — the D7 floor's contract is that the canvas answers a click at
      // its 40pt real-slice floor, where the plot ink cannot exist but the selection still can.
      if (x_hi > 0.0) {
        if (plot_fits && have_range && canvas_hovered && ImGui::GetIO().MousePos.x >= plot_min.x &&
            ImGui::GetIO().MousePos.x <= plot_max.x && ImGui::GetIO().MousePos.y >= plot_min.y &&
            ImGui::GetIO().MousePos.y <= plot_max.y) {
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
      if (hover_position >= 0) {
        const int stored = PointCurveStoredAt(curve, hover_position);
        const float px = x_to_px(curve.points[stored].x_rad);
        dl->PushClipRect(plot_min, plot_max, true);
        dl->AddLine(ImVec2(px, plot_min.y), ImVec2(px, plot_max.y), ImGui::GetColorU32(ImGuiCol_TextDisabled, 0.55f),
                    1.0f);
        for (int arm = 0; arm < kCurveArms; ++arm) {
          const double v = value_of(arm, stored);
          if (kind_of(arm, stored) == PointCurveValueKind::kPositive && v >= v_min && v <= v_max) {
            dl->AddCircleFilled(ImVec2(px, v_to_py(v)), UiPx(3.5f), arm_colors[arm]);
          }
        }
        dl->PopClipRect();
        ImGui::SetTooltip("%s\nLeft click or drag to select a pose.", sample_readout(hover_position).c_str());
      }
      // ---- The scroll region's tail: the partial note. The measure registration's note and
      // the current sample's factor readouts moved to the factors region (§16.4's content
      // owner) — the companion carries the curve's own reading only. ----
      if (info->execution != LUMICE_PATH_OUTCOME_COMPLETED) {
        WarningTextWrapped("Partial document (%s) — the curve on show is what arrived.",
                           ExecutionText(info->execution));
      }
      return;
    }
    // No canvas: the companion's identity, caliber, legend and limitation remain scrollable.
    if (info->execution != LUMICE_PATH_OUTCOME_COMPLETED) {
      WarningTextWrapped("Partial document (%s) — the curve on show is what arrived.", ExecutionText(info->execution));
    }
  };
  render_content();
  ImGui::EndChild();
  ImGui::EndChild();
}

// The factors region — the right column's lower member (contract §16.4): the current pose's
// factor readouts bound to view.sample_index — the ONE selection the slider, the globe, the 3D
// trace and the curve canvas all write — plus the measure registration's note (a limitation
// reads beside the numbers it limits, §16.4's default-readable rule) and the two §16.1 fold
// groups. The curve canvas's hover tooltip is temporary display only
// only; nothing here follows the hover — CurrentSampleOf reads view.sample_index, which hover
// never writes.
void RenderPoseFactorsRegion(GuiState& state, const ImVec2& size) {
  ImGui::BeginChild("##AnalyzePoseFactors", size, ImGuiChildFlags_Borders);
  PointQuerySession& point = state.analysis.point;
  PointViewState& view = state.analysis.point_view;
  ImGui::TextUnformatted("Current pose factors");
  ImGui::BeginChild("##PoseFactorsScroll", RemainingReportSize());
  const auto render_content = [&] {
    const PointQueryState st = PointQueryStateFor(point);
    if (st == PointQueryState::kNoIntent) {
      DisabledWrappedText("The current pose's factors take this region once a point result is in.");
      return;
    }
    if (st != PointQueryState::kCompleted) {
      return;  // Query lifecycle text lives only in the window footer.
    }
    const std::optional<LUMICE_PathPointInfo> info = ReadPointInfo(*point.adopted_result);
    if (!info.has_value()) {
      DisabledWrappedText("The adopted point document did not answer its Info reader.");
      return;
    }
    if (info->outcome == LUMICE_PATH_POINT_MASS) {
      DisabledWrappedText("A rank-0 point mass has no per-pose factors — the details carry the mass numbers.");
      return;
    }
    std::vector<PointComponentRow> rows;
    const int component_total = PointComponentRows(*point.adopted_result, &rows);
    if (component_total == 0) {
      LUMICE_PathPointReach reach{};
      reach.struct_size = sizeof(reach);
      const bool have_reach = LUMICE_PathPointGetReach(point.adopted_result.get(), &reach) == LUMICE_PATH_READ_OK;
      if (info->execution != LUMICE_PATH_OUTCOME_COMPLETED) {
        WarningTextWrapped(
            "Partial document (%s) — what arrived is on show; emptiness here is the run's "
            "stop, not a physical absence.",
            ExecutionText(info->execution));
      }
      if (have_reach && !reach.target_in_range) {
        DisabledWrappedText(
            "No fiber here: the target's deviation is outside the range of this path's deflection field.");
      } else {
        DisabledWrappedText("No fiber component at this point — there are no pose factors to read.");
      }
      return;
    }
    const int component = std::clamp(view.component_index, 0, component_total - 1);
    const std::vector<int> valid_indices = CollectValidSampleIndices(*point.adopted_result, component);
    if (valid_indices.empty()) {
      if (info->execution != LUMICE_PATH_OUTCOME_COMPLETED) {
        WarningTextWrapped("Partial document (%s) — the factors on show are what arrived.",
                           ExecutionText(info->execution));
      }
      DisabledWrappedText("This component carries no valid sample — the pose factors have nothing to point at.");
      return;
    }
    if (info->execution != LUMICE_PATH_OUTCOME_COMPLETED) {
      WarningTextWrapped("Partial document (%s) — the factors on show are what arrived.",
                         ExecutionText(info->execution));
    }
    const std::optional<CurrentSample> current = CurrentSampleOf(*point.adopted_result, view, valid_indices);
    if (!current.has_value()) {
      DisabledWrappedText("The current sample could not be read from the adopted document.");
      return;
    }
    // ---- The default layer (§16.1's ladder): the factor conclusions, bound to the ONE
    // selection; the measure registration's note beside them (a limitation reads where it
    // limits, §16.4's default-readable rule). ----
    DisabledWrappedText("Current sample:");
    for (const std::string& line_text : PointFactorHeadLines(current->sample)) {
      ImGui::Bullet();
      ImGui::TextWrapped("%s", line_text.c_str());
    }
    LUMICE_PathMeasureRegistration reg{};
    reg.struct_size = sizeof(reg);
    if (LUMICE_PathPointGetMeasureRegistration(point.adopted_result.get(), &reg) == LUMICE_PATH_READ_OK &&
        reg.registered_gap != 0 && reg.note.data != nullptr && reg.note.length > 0) {
      DisabledWrappedText("measure registration: %.*s", static_cast<int>(reg.note.length), reg.note.data);
    }
    // ---- The two §16.1 fold groups: A1's label constants, one spelling (a56); the expansion
    // state lives in the pose window's ImGui storage — session-sticky, no GuiState field, the
    // same convention every answer host follows. Both render in every state: the diagnostics
    // group is not env-gated (its deviation-grid toggle is a user-facing control; only the
    // Class/Point/Map diagnostics groups stay behind LUMICE_ANALYZE_DEBUG_DETAILS). The
    // toggle's display effect — the globe's backdrop — is gated at the globe's own draw site
    // (RenderAnalyzeSunGlobeRegion's have_grid), by the toggle and the payload alone. ----
    if (FoldOpen(kPoseTransmissionLabel, "pose_factors")) {
      // The per-interface transmittances: T's optical premises — the ladder's evidence rows,
      // not the default layer's conclusions.
      for (const std::string& line_text : PointFactorInterfaceLines(current->sample)) {
        ImGui::Bullet();
        ImGui::TextWrapped("%s", line_text.c_str());
      }
    }
    if (FoldOpen(kPoseDiagnosticsLabel, "pose_factors")) {
      // The deviation-grid toggle: a calculation aid retired here from the globe's fixed stack
      // (§16.4); its reader — the globe's backdrop — keeps its own region. The requested-but-
      // absent note travels with the toggle: it is the toggle request's fact, not the globe's.
      const float dev_grid_w =
          ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize("Deviation grid").x;
      if (dev_grid_w <= ImGui::GetContentRegionAvail().x) {
        ImGui::Checkbox("Deviation grid###pose_factors_dev_grid", &view.show_deviation_grid);
      } else {
        ImGui::Checkbox("###pose_factors_dev_grid", &view.show_deviation_grid);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        ImGui::TextWrapped("Deviation grid");
        ImGui::PopStyleColor();
      }
      if (view.show_deviation_grid) {
        LUMICE_PathSunGrid grid{};
        grid.struct_size = sizeof(grid);
        const bool have_grid =
            LUMICE_PathPointGetSunGrid(point.adopted_result.get(), &grid) == LUMICE_PATH_READ_OK && grid.cell_count > 0;
        if (!have_grid) {
          DisabledWrappedText("(no sun grid in this document — the backdrop is absent, not zero)");
        }
      }
    }
  };
  render_content();
  ImGui::EndChild();
  ImGui::EndChild();
}

// The trace face's display state for the CURRENT selection, read for the status bar's transient
// segment. nullopt = no valid current point/sample — no trace fact to state, the point's own
// words own the bar. Otherwise the same named state the 3D region's gate computes: the pump is
// the session's writer, this is a read over the same public state, and the key comes from the
// ONE shared constructor (RayTraceKeyFor) the region's gate uses — the report and the bar
// cannot disagree about which state the trace is in. The derivation deliberately is NOT
// extracted into one shared CurrentSampleForSelection for both sites: the region's walk
// interleaves a distinct honest render per failure arm (unreadable Info / mass / no components
// / no valid sample / unreadable sample), which a combined optional helper would collapse into
// one generic failure. The shared layer is the four primitives both sites call, plus the key
// constructor above; the guard sequences stay each site's own.
std::optional<RayTraceDisplayState> TraceDisplayStateForSelection(const GuiState& state) {
  const PointQuerySession& point = state.analysis.point;
  if (!point.adopted_result || PointQueryStateFor(point) != PointQueryState::kCompleted) {
    return std::nullopt;
  }
  const std::optional<LUMICE_PathPointInfo> info = ReadPointInfo(*point.adopted_result);
  if (!info.has_value() || info->outcome != LUMICE_PATH_POINT_DISCOVERED || info->component_count == 0) {
    return std::nullopt;
  }
  const std::vector<int> valid_indices = CollectValidSampleIndices(
      *point.adopted_result,
      std::clamp(state.analysis.point_view.component_index, 0, static_cast<int>(info->component_count) - 1));
  const std::optional<CurrentSample> current =
      CurrentSampleOf(*point.adopted_result, state.analysis.point_view, valid_indices);
  if (!current.has_value()) {
    return std::nullopt;
  }
  return RayTraceDisplayStateFor(state.analysis.trace, RayTraceKeyFor(point.intent_id, *current));
}

}  // namespace

void RenderAnalyzePoseViewWindow(GuiState& state) {
  if (!state.analyze_pose_view_open) {
    return;
  }
  // The pumps run at RenderWorkspacePanels' entry (every GUI frame — windows open or not); this
  // window keeps only its own wait-feedback hygiene: on the Render-workspace frames it hosts
  // the footer (the sole writer of point_wait_line_fiber) and the staleness hint (the
  // sole writer of point_stale_hint), so a field's content is exactly what the user saw THIS
  // frame — the site that can draw it owns the clear. The details field's writer is the main
  // window's diagnostics block (an Analyze-frame-only face); on the Render frames where the
  // pose window is its own host the details field simply stays at its last Analyze-frame value
  // — stale-honest, the same rule every window-closed field follows — and this window does not
  // touch it. On an Analyze frame the workspace entry has already pumped and cleared; the footer
  // below is then that frame's only fiber-field writer.
  if (state.workspace.mode != GuiState::WorkspaceMode::kAnalyze) {
    WaitFeedbackTexts& feedback = state.analysis.wait_feedback_texts;
    feedback.point_wait_line_fiber.clear();
    feedback.point_stale_hint.clear();
    feedback.point_stale_hint_rect_min_x = 0.0f;
    feedback.point_stale_hint_rect_min_y = 0.0f;
    feedback.point_stale_hint_rect_max_x = 0.0f;
    feedback.point_stale_hint_rect_max_y = 0.0f;
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
  if (!ImGui::Begin(kPoseViewWindowTitle, &state.analyze_pose_view_open,
                    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoScrollWithMouse)) {
    ImGui::End();
    return;
  }
  // The staleness hint's fixed bar: the point answers describe the analysis payload, so the
  // list's staleness against the live document states itself here — notice-only (no button, no
  // action: the refresh is the user's explicit Analyze, the contract's no-auto-rerun rule). The
  // bar is drawn FIRST and only lowers the body: the VisibleContentSize() the D4 split reads
  // below is taken after it, so the six-element layout's height budget shrinks by the bar's real
  // footprint — the absolute cursor layout shifts down as one piece (body_min sits below the
  // bar), no placement is rewritten. The point-level facts are none of the bar's business: the
  // pump re-keys a changed point intent, and the centre's drift does not change any payload the
  // window shows. The field reports the string this site drew and its FULL item rect (the
  // completeness face beside the ink), per-window like every render-feedback field.
  WaitFeedbackTexts& hint_feedback = state.analysis.wait_feedback_texts;
  if (state.analysis_freshness.list == AnalysisListFreshness::kStale) {
    // The field carries the string the draw drew, icon included — what the test layer reads is
    // what was drawn (the band's field contract, one shape per face).
    const char* const stale_text = ICON_FA_TRIANGLE_EXCLAMATION
        " Answers are from the analysis of a previous configuration \xe2\x80\x94 press Analyze to refresh.";
    WarningTextWrapped("%s", stale_text);
    const ImVec2 rect_min = ImGui::GetItemRectMin();
    const ImVec2 rect_max = ImGui::GetItemRectMax();
    hint_feedback.point_stale_hint_rect_min_x = rect_min.x;
    hint_feedback.point_stale_hint_rect_min_y = rect_min.y;
    hint_feedback.point_stale_hint_rect_max_x = rect_max.x;
    hint_feedback.point_stale_hint_rect_max_y = rect_max.y;
    hint_feedback.point_stale_hint = stale_text;
  }
  // ---- D4 — the 3D | right-column split (contract §16.4), the Raypath view's D3 recipe: the
  // stored ratio is the split's identity (manual priority, §9.3 — no query result or selection
  // ever writes it); the frame derives pixels and clamps BOTH sides every frame — the right
  // column at its floor AND the left column at its own (either side may be the one pushed
  // toward its bound by a drag to an end or a resize under a kept ratio); only when the two
  // floors cannot share the span does the ③ proportional fallback apply. Both columns are
  // elastic members of the resize strategy (each hosts canvases over fixed controls), so the
  // resize delta distributes by the stored ratio — the recipe's own semantics, no new path. ----
  const float splitter_w = UiPx(kAnalyzeSplitterSize);
  const AnalyzeStatusBarLayout status_layout = ReserveAnalyzeStatusBar();
  const ImVec2 avail = status_layout.body_size;
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

  // The body's six elements are placed by ABSOLUTE cursor (the derived geometry and the
  // placement are the same arithmetic — the SameLine row flow would put the right column's
  // vertical stack back at the window's left edge on the line below the 3D row, under the 3D
  // region's own child).
  const ImVec2 body_min = ImGui::GetCursorScreenPos();

  // ---- The LEFT column (§16.4): D5's 3D | curve split, the row-splitter recipe. The column's
  // height denominator is its own — D5 and D6 each consume the same visible height without
  // sharing a ratio — and the two ItemSpacings between the three elements are part of the
  // budget the stored ratio divides. Same floors-and-fallback rule as D4/D6; dragging D5
  // writes D5's field and never touches the others (§16.4's 互不写入). ----
  const float strip_h = UiPx(kAnalyzeSplitterSize);
  const float v_spacing = ImGui::GetStyle().ItemSpacing.y;
  const float denom_h = std::max(0.0f, avail.y - strip_h - v_spacing * 2.0f);
  const float left_3d_floor = UiPx(kPoseViewLeft3dFloor);
  const float curve_floor = UiPx(kPoseViewCurveFloor);
  float left_3d_h = state.analyze_pose_view_left_row_ratio * denom_h;
  if (denom_h >= left_3d_floor + curve_floor) {
    left_3d_h = std::clamp(left_3d_h, left_3d_floor, denom_h - curve_floor);
  } else {
    left_3d_h = std::max(0.0f, denom_h * (left_3d_floor / (left_3d_floor + curve_floor)));
  }
  const float left_curve_h = std::max(0.0f, denom_h - left_3d_h);

  RenderAnalyzeRay3DRegion(state, ImVec2(left_w, left_3d_h));
  ImGui::SetCursorScreenPos(ImVec2(body_min.x + left_w, body_min.y));
  // The drag writes from the displayed boundary the frame derived (right_w), bounded where the
  // layout's own clamp stops; the overflow pin is what the clamp's lo==hi band writes (the
  // dialect equivalence: analyze_ui_widgets.hpp's white-box note).
  AnalyzeSplitter("##PoseViewSplitter", SplitterDragAxis::kLeftRight, avail.y, right_w, denom_w, right_floor,
                  denom_w - left_floor, right_floor / std::max(1.0f, denom_w), &state.analyze_pose_view_right_ratio);
  ImGui::SetCursorScreenPos(ImVec2(body_min.x, body_min.y + left_3d_h + v_spacing));
  AnalyzeSplitter("##PoseViewLeftRowSplitter", SplitterDragAxis::kUpDown, left_w, left_3d_h, denom_h, left_3d_floor,
                  denom_h - curve_floor, left_3d_floor / std::max(1.0f, denom_h),
                  &state.analyze_pose_view_left_row_ratio);
  ImGui::SetCursorScreenPos(ImVec2(body_min.x, body_min.y + left_3d_h + v_spacing + strip_h + v_spacing));
  RenderPoseCurveRegion(state, ImVec2(left_w, left_curve_h));

  // ---- The right column: D6's globe | factors split (§16.4), the row-splitter recipe. The
  // boundary the drag has always written keeps its semantics — the globe region's share of the
  // column's height, never re-meaned into a 3D share; the lower member is now the factors
  // region. Same floors-and-fallback rule as D4. ----
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
  AnalyzeSplitter("##PoseViewRowsSplitter", SplitterDragAxis::kUpDown, right_w, globe_h, denom_h, globe_floor,
                  denom_h - factors_floor, globe_floor / std::max(1.0f, denom_h), &state.analyze_pose_view_globe_ratio);
  ImGui::SetCursorScreenPos(ImVec2(col_x, body_min.y + globe_h + v_spacing + strip_h + v_spacing));
  RenderPoseFactorsRegion(state, ImVec2(right_w, factors_h));
  const PointQuerySession& point = state.analysis.point;
  bool partial = false;
  std::string status = PointStatusLabelFor(point, &partial);
  switch (PointQueryStateFor(point)) {
    case PointQueryState::kWaiting: {
      const std::string wait_line = PointWaitLineFor(point, ImGui::GetTime(), g_survey_executor.InflightPollSnapshot());
      status += " — " + wait_line;
      state.analysis.wait_feedback_texts.point_wait_line_fiber = wait_line;
      break;
    }
    case PointQueryState::kCancelled:
      status += " — moving the sky point or re-picking the class asks anew.";
      break;
    case PointQueryState::kRefused:
      status += " — " + point.adopted_error;
      break;
    default:
      break;
  }
  // The trace's transient words ride the same single-line bar, AFTER the point's own words
  // (point first; the segment exists only while the point answer is in and the current sample
  // is valid — the helper's nullopt arms). Computing and not-requested are the transients the
  // report no longer cycles; drawn/error/refused are answers — the report owns those, and the
  // bar says nothing about the trace. The point's partial flag and wait field keep their own
  // writers: this append never overwrites them.
  if (const std::optional<RayTraceDisplayState> trace_state = TraceDisplayStateForSelection(state)) {
    if (*trace_state == RayTraceDisplayState::kComputing) {
      status += " — representative path computing";
    } else if (*trace_state == RayTraceDisplayState::kHidden) {
      status += " — representative path not requested";
    }
  }
  RenderAnalyzeStatusBar(status_layout, status, partial || PointQueryStateFor(point) == PointQueryState::kRefused);
  ImGui::End();
}

// ---- The point-level details block -------------------------------------------------------------------

namespace {

// The completed document's common prefix reads: the Info (or the unreadable-document line) and
// the partial-completion limit. Both arms (the main window's summary and the Raypath view's
// complete answer) open with these — one spelling, two hosts (D2: the same read-back, never a
// second wording). Returns false after drawing the unreadable line; the caller stops.
bool DrawPointCompletedHead(const PointQuerySession& point, LUMICE_PathPointInfo* info) {
  const std::optional<LUMICE_PathPointInfo> read = ReadPointInfo(*point.adopted_result);
  if (!read.has_value()) {
    DisabledWrappedText("The adopted point document did not answer its Info reader.");
    return false;
  }
  *info = *read;
  if (info->execution != LUMICE_PATH_OUTCOME_COMPLETED) {
    WarningTextWrapped("Partial document: %s — what arrived is on show, honestly incomplete.",
                       ExecutionText(info->execution));
  }
  return true;
}

// The no-component arm's conclusions: the reach's out-of-range fact or the finite-crystal
// statement, then the discovery's own completeness note. An L1 pair — both hosts keep it
// verbatim (the 覆盖律: 有限排除/完备性限定不得被折叠掉).
void DrawPointNoComponentConclusions(const PointQuerySession& point) {
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
}

// The main window's completed arm — the summary projection (D5's 口径): the target line, the
// rank-0 answer in full, the component count with its availability limits and the selected
// component's no-valid-sample fact. The per-component evidence rows, the fiber integral, the
// member/λ premise lines and the execution counts live in the Raypath view's complete answer.
void DrawPointSummaryCompleted(GuiState& state, const LUMICE_PathPointInfo& info) {
  PointQuerySession& point = state.analysis.point;
  PointViewState& view = state.analysis.point_view;

  const std::vector<std::string> caliber = PointCaliberLines(info);
  if (!caliber.empty()) {  // the target line: the point's coordinates and its deviation
    ImGui::Bullet();
    ImGui::TextWrapped("%s", caliber[0].c_str());
  }

  if (info.outcome == LUMICE_PATH_POINT_MASS) {
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
    // The caliber detail (member faces, execution tier) is omitted here — same pointer leg.
    DisabledWrappedText("The complete answer is in the %s.", kRaypathViewWindowTitle);
    return;
  }

  std::vector<PointComponentRow> rows;
  const int component_total = PointComponentRows(*point.adopted_result, &rows);
  if (component_total == 0) {
    DrawPointNoComponentConclusions(point);
    DisabledWrappedText("The complete answer is in the %s.", kRaypathViewWindowTitle);
    return;
  }
  // The component summary: the count, the kinds, and the availability limits — an L1 fact set
  // (an unavailable integral is an incomplete answer, not a missing one; no valid sample is
  // not no-light). The kind/ends/integral evidence rows live in the complete answer.
  std::string kinds;
  int unavailable = 0;
  for (const PointComponentRow& row : rows) {
    if (!kinds.empty()) {
      kinds += ", ";
    }
    kinds += row.kind;
    if (!row.integral_available) {
      ++unavailable;
    }
  }
  ImGui::Bullet();
  ImGui::TextWrapped("%d component(s): %s.", component_total, kinds.c_str());
  if (unavailable > 0) {
    DisabledWrappedText("%d of them: fiber integral unavailable (the points did not cover an arclength).", unavailable);
  }
  const int component = std::clamp(view.component_index, 0, component_total - 1);
  const std::vector<int> detail_valid_indices = CollectValidSampleIndices(*point.adopted_result, component);
  if (detail_valid_indices.empty()) {
    ImGui::TextDisabled("No valid sample on show for this component.");
  }
  // §16.2's pointer rule, the point summary's own leg: the evidence rows, integrals and
  // premise lines are omitted here, and the omission names its home — the same spelling the
  // survey summary closes with (one constant, one sentence).
  DisabledWrappedText("The complete answer is in the %s.", kRaypathViewWindowTitle);
}

}  // namespace

void RenderPointDetailsBlock(GuiState& state, const SelectedClassIdentity& identity) {
  PointQuerySession& point = state.analysis.point;
  PointViewState& view = state.analysis.point_view;

  // The scroll region's content starts with this block's first fact (§16.3: the main window's
  // window-entry buttons live in the fixed head above, and the block itself carries no entry —
  // the retired in-report link's contract, display-writes-only navigation with sky-point
  // writers kept separate, now lives with the entry buttons that own it).
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

  // Completed: the summary projection reads the document's own readers (D2) — the common head
  // (Info + partial limit) then the L1 facts the main window owes even with both windows closed
  // (contract §16.2's 关窗场景: rank-0 数值、reach/完备性、无有效样本).
  LUMICE_PathPointInfo info{};
  if (!DrawPointCompletedHead(point, &info)) {
    return;
  }
  DrawPointSummaryCompleted(state, info);
}

// ---- The Raypath view's sky-point answer (contract §16.2) --------------------------------------------

void RenderPointAnswerBlock(GuiState& state) {
  PointQuerySession& point = state.analysis.point;
  PointViewState& view = state.analysis.point_view;
  // The adopted document displays whenever one is in hand — including the in-flight window of
  // a re-query, where it IS the answer the ledger's distinction row promises (the input group's
  // target is the intent, the answer's target is the adopted document). The four states and
  // their cancel actions stay in the main window; a withdrawn intent resets the session, so no
  // document means this section draws nothing.
  if (point.adopted_result == nullptr) {
    return;
  }
  LUMICE_PathPointInfo info{};
  if (!DrawPointCompletedHead(point, &info)) {
    return;
  }

  // Default layer: the target and λ/n premises (§16.2: 天空点答案带 target、λ、必要量纲),
  // then the arm's answer.
  const std::vector<std::string> caliber = PointCaliberLines(info);
  if (caliber.size() > 2) {
    for (const size_t i : { size_t(0), size_t(2) }) {
      ImGui::Bullet();
      ImGui::TextWrapped("%s", caliber[i].c_str());
    }
  }

  std::vector<PointComponentRow> rows;
  const int component_total =
      info.outcome == LUMICE_PATH_POINT_MASS ? 0 : PointComponentRows(*point.adopted_result, &rows);
  bool has_components = false;
  if (info.outcome == LUMICE_PATH_POINT_MASS) {
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
  } else if (component_total == 0) {
    DrawPointNoComponentConclusions(point);
  } else {
    has_components = true;
    // The component summary at the default layer: the count, the kinds, the availability
    // limits and the selected component's no-valid-sample fact. The evidence rows are the
    // Physics group's.
    std::string kinds;
    int unavailable = 0;
    for (const PointComponentRow& row : rows) {
      if (!kinds.empty()) {
        kinds += ", ";
      }
      kinds += row.kind;
      if (!row.integral_available) {
        ++unavailable;
      }
    }
    ImGui::Bullet();
    ImGui::TextWrapped("%d component(s): %s.", component_total, kinds.c_str());
    if (unavailable > 0) {
      DisabledWrappedText("%d of them: fiber integral unavailable (the points did not cover an arclength).",
                          unavailable);
    }
    const int component = std::clamp(view.component_index, 0, component_total - 1);
    const std::vector<int> detail_valid_indices = CollectValidSampleIndices(*point.adopted_result, component);
    if (detail_valid_indices.empty()) {
      ImGui::TextDisabled("No valid sample on show for this component.");
    }
  }

  // The fold pairs, the SAME structure on every arm — the complete-answer promise means every
  // premise line has a home on every document shape (the member-faces line is
  // PointCaliberLines' unconditional evidence row; the component evidence is the component
  // arm's extra content inside the same group; the diagnostics group is env-gated as a whole).
  if (FoldOpen(kPointEvidenceLabel, "pt_answer")) {
    if (caliber.size() > 1) {  // the member faces / crystal / kind premise line
      DisabledWrappedText("%s", caliber[1].c_str());
    }
    if (has_components) {
      // The component evidence rows: kind, ends, coverage and integral availability — the
      // ledger's Physics tier. The rows are read-only (the component selector's one
      // interaction owner is the pose window's globe combo); each row's hover tooltip names
      // its full text. The render cap's truncation note stays AT the truncated list. The
      // integral row keeps its value and its availability verdict — the covers/points-usable
      // counts inside the parentheses are implementation coverage numbers and moved to the
      // diagnostics group.
      const size_t true_component_count = static_cast<size_t>(info.component_count);
      ImGui::TextDisabled("Components");
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
      const PointComponentRow& selected = rows[static_cast<size_t>(component)];
      if (selected.integral_available) {
        DisabledWrappedText("Selected component's fiber integral %.6g.", selected.fiber_integral);
      } else {
        DisabledWrappedText("Selected component's fiber integral: unavailable.");
      }
      if (true_component_count > static_cast<size_t>(component_total)) {
        DisabledWrappedText("+ %zu more component(s) — beyond this view's render cap.",
                            true_component_count - static_cast<size_t>(component_total));
      }
    }
  }
  // The diagnostics group, env-gated as a whole (header and body): the discovery execution
  // triple, the per-component coverage counts and the valid/total sample ratio. The default
  // partial/no-valid-sample limits stay at the default layer regardless of this group.
  if (env::AnalyzeDebugDetails() && FoldOpen(kPointDiagnosticsLabel, "pt_answer")) {
    if (caliber.size() > 3) {  // discovery sample_count / budget / execution
      DisabledWrappedText("%s", caliber[3].c_str());
    }
    if (has_components) {
      const int component = std::clamp(view.component_index, 0, component_total - 1);
      const PointComponentRow& selected = rows[static_cast<size_t>(component)];
      if (selected.integral_available) {
        DisabledWrappedText("Selected component's fiber integral covers %d/%d points' arclength.",
                            selected.available_points, selected.total_points);
      } else {
        DisabledWrappedText("Selected component's fiber integral: %d/%d points usable.", selected.available_points,
                            selected.total_points);
      }
      const std::vector<int> valid_indices = CollectValidSampleIndices(*point.adopted_result, component);
      DisabledWrappedText("Selected component: %d valid sample(s) of %d.", static_cast<int>(valid_indices.size()),
                          rows[static_cast<size_t>(component)].point_count);
    }
  }
}

}  // namespace lumice::gui
