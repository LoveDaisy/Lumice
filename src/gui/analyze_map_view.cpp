#include "gui/analyze_map_view.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "IconsFontAwesome6.h"     // the staleness hint's leading glyph, the banner's family
#include "gui/analysis_panel.hpp"  // SelectedClassIdentityFor / SurveySelectionKeyFor — the one identity chain
#include "gui/analyze_map_renderer.hpp"
#include "gui/analyze_overview.hpp"
#include "gui/analyze_point_view.hpp"    // RenderPointAnswerBlock — the sky-point answer host
#include "gui/analyze_region_wheel.hpp"  // the wheel claim — one arbitration for both gesture regions
#include "gui/analyze_ui_widgets.hpp"  // DisabledWrappedText / WarningTextWrapped / DrawLegendIconSlot / AnalyzeSplitter
#include "gui/analyze_workspace.hpp"   // PumpAnalyzeQueries — the pumps this window's Render-mode frames run
#include "gui/app.hpp"
#include "gui/canvas_report_layout.hpp"
#include "gui/gl_common.h"
#include "gui/gui_constants.hpp"
#include "gui/gui_logger.hpp"
#include "gui/gui_state.hpp"
#include "gui/preview_renderer.hpp"
#include "gui/theme.hpp"
#include "gui/tooltip.hpp"
#include "gui/view_look_at.hpp"
#include "imgui.h"
#include "util/env_knobs.hpp"             // env::AnalyzeDebugDetails — the diagnostics groups' env gate
#include "util/label_viewport_clamp.hpp"  // the ONE viewport clamp — the readout label's shared rule
#include "util/sky_direction.hpp"

namespace lumice::gui {


namespace {

// The semantic palette's promotion (analyze_map_view.hpp kChromaticColour*) must be VALUE-identical
// to the dispersion pair's original literals — the dispersions' pixels are a visual-reference
// surface, and a wrong promotion would retune the whole picture at once. Pinned against the
// literals at the promotion site: retuning one side breaks this at compile time, the two-consumer
// change the constant's comment demands.
static_assert(kChromaticColourRed == IM_COL32(242, 108, 92, 235), "the promoted red must equal its literal");
static_assert(kChromaticColourBlue == IM_COL32(96, 145, 240, 235), "the promoted blue must equal its literal");
static_assert(kReadoutBackingInk == IM_COL32(16, 17, 20, 170), "the promoted backing ink must equal its literal");
static_assert(kProfileReadingColour == IM_COL32(244, 114, 182, 255), "the promoted rose must equal its literal");
static_assert(kSurveyArcInk == IM_COL32(200, 205, 215, 215), "the promoted arc ink must equal its literal");
static_assert(kPointMassColour == IM_COL32(250, 184, 74, 255), "the promoted mass colour must equal its literal");

// The family arcs' inks (task arc-recovery-engine): the fold arc takes the caustic family's own
// hue (the arc replaces that marker), the point-image run the collapse family's, the cusp
// candidates a hotter variant of the arc hue — one semantics one entry in the legend.
constexpr ImU32 kFamilyArcInk = IM_COL32(255, 176, 92, 235);
constexpr ImU32 kFamilyPointInk = IM_COL32(198, 160, 255, 235);
constexpr ImU32 kFamilyCuspInk = IM_COL32(255, 120, 80, 235);

OverlayLegendState g_map_legend;
MapObservationTiming g_map_observation_timing;
double ObservationClockSeconds() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

constexpr double kPiD = 3.14159265358979323846;
constexpr double kDeg2RadD = kPiD / 180.0;
constexpr double kRad2DegD = 180.0 / kPiD;

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
// (the §12 display rule). One implementation here, consumed by the raster and the
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
// members, kept events — plus the source-fixed-scale note. The wavelength is NOT in any line:
// the wl row's same-control echo is the reading's one owner (a repeated echo is deleted, not
// moved).

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
  if (in.survey_scope_differs) {
    // The in-place scope note (the partial tick sets): the map's member set and the survey
    // overlay's are different collections — the note rides the adopted map's own bits, so a
    // late frame cannot relabel the old picture with the current ticks.
    lines.emplace_back(
        "Partial selection: the map sums the ticked P/B/D member set; the survey answer and its overlay markers stay "
        "the row's literal representative.");
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
  return lines;
}

// The caliber's diagnostics group: the engine's raw geometry_basis explanation string plus the
// grid and execution facts — rendered only while LUMICE_ANALYZE_DEBUG_DETAILS is set. The
// readable scene/nominal caliber stays in the evidence group above, so the raw basis string
// hides nothing a default or evidence line does not already say in user words.
std::vector<std::string> MapCaliberDiagnosticsLines(const MapCaliberInput& in) {
  std::vector<std::string> lines;
  char line[512];
  const LUMICE_PathMapInfo& info = in.info;
  if (in.have_evidence && in.evidence.geometry_basis.length > 0) {
    lines.emplace_back(in.evidence.geometry_basis.data, in.evidence.geometry_basis.length);
  }
  if (info.view_full_sky != 0) {
    std::snprintf(line, sizeof(line), "Reference cell dev %.7g° × az %.7g° · view: full sky %d×%d · n=%d lattice",
                  (info.view_delta_max_deg - info.view_delta_min_deg) / info.view_rows,
                  (info.view_az_max_deg - info.view_az_min_deg) / info.view_cols, info.view_rows, info.view_cols,
                  info.sample_count);
  } else {
    std::snprintf(line, sizeof(line),
                  "Reference cell dev %.7g° × az %.7g° · view: dev [%.1f, %.1f]° az [%.1f, %.1f]° %d×%d · n=%d lattice",
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
  // The map's true member selection rides explicitly (1..8): the GUI always states the set it
  // wants, the 0 default stays the older callers' encoding (bitwise-equal for the full class).
  request.symmetry_bits_plus_one = key.symmetry_bits + 1;
  return request;
}

// ---- Normalization, colour -------------------------------------------------------------------------

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
  const uint8_t symmetry_bits = AnalysisSymmetryBits(state);
  SurveySelectionKey selection;
  if (selected) {
    selection = SurveySelectionKeyFor(*identity, state.analysis_result.payload->snapshot_generation);
  }
  // Invalidate before draining: no old capture/lambda may acquire the new labels, even for a
  // frame. The map's true symmetry bits are part of the data identity — a partial-to-partial
  // change retires the old map even though the binary survey identity (and often the row face
  // sequence) did not move. Camera fields are deliberately absent from this effect chain.
  if (map.intent_valid && (!selected || map.key.selection != selection || map.key.symmetry_bits != symmetry_bits ||
                           map.key.wavelength_nm != state.analysis.map_wavelength_nm)) {
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
    map.key.symmetry_bits = symmetry_bits;
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

// ---- The inner-edge selection (817.2) ---------------------------------------------------------------

bool InnerEdgeSelectionActive(const GuiState& state) {
  const auto& s = state.analysis;
  // The binding is the adopted identity itself: the document handle it was made under, still
  // the survey's adopted result, and still the survey's current intent id. A class switch, a
  // new generation or a lost document breaks it without any explicit clear.
  return s.inner_edge_selected && !s.inner_edge_document.expired() &&
         s.inner_edge_document.lock() == s.survey.adopted_result && s.inner_edge_id == s.survey.intent_id;
}

void SelectInnerEdgeObject(GuiState& state) {
  auto& s = state.analysis;
  s.inner_edge_selected = true;
  s.inner_edge_document = s.survey.adopted_result;
  s.inner_edge_id = s.survey.intent_id;
}

// ---- The family curve selection (task arc-recovery-engine) -------------------------------------------

bool FamilyCurveSelectionActive(const GuiState& state) {
  const auto& s = state.analysis;
  // The inner-edge binding verbatim: the adopted document handle it was made under, still the
  // survey's adopted result, still its current intent id.
  return s.family_curve_selected && !s.family_curve_document.expired() &&
         s.family_curve_document.lock() == s.survey.adopted_result && s.family_curve_id == s.survey.intent_id;
}

void SelectFamilyCurve(GuiState& state, const FamilyCurvePick& pick, std::vector<int> sources) {
  auto& s = state.analysis;
  s.family_curve_selected = true;
  s.family_curve_document = s.survey.adopted_result;
  s.family_curve_id = s.survey.intent_id;
  s.family_curve_index = pick.curve_index;
  s.family_curve_pick_kind = static_cast<int>(pick.kind);
  s.family_curve_point_index = pick.point_index;
  s.family_curve_segment_index = pick.segment_index;
  s.family_curve_cusp_index = pick.cusp_index;
  s.family_curve_sources = std::move(sources);
  s.family_curve_source_slot = 0;  // the committed curve is the nearest hit's — slot zero
}

void CycleFamilyCurveSource(GuiState& state, int step) {
  auto& s = state.analysis;
  if (s.family_curve_sources.empty()) {
    return;
  }
  const int n = static_cast<int>(s.family_curve_sources.size());
  s.family_curve_source_slot = ((s.family_curve_source_slot + step) % n + n) % n;
  s.family_curve_index = s.family_curve_sources[static_cast<size_t>(s.family_curve_source_slot)];
  // The switched source was not the clicked one: its witness resets to the curve's own first
  // point (a legal preimage of THAT curve — never the clicked event's, which belongs to another
  // source). The user refines on the map if they want a specific event of it.
  s.family_curve_pick_kind = static_cast<int>(FamilyCurvePickKind::kPoint);
  s.family_curve_point_index = 0;
  s.family_curve_segment_index = -1;
  s.family_curve_cusp_index = -1;
}

void ClearFamilyCurveSelection(GuiState& state) {
  auto& s = state.analysis;
  s.family_curve_selected = false;
  s.family_curve_document.reset();
  s.family_curve_index = -1;
  s.family_curve_pick_kind = 0;
  s.family_curve_point_index = -1;
  s.family_curve_segment_index = -1;
  s.family_curve_cusp_index = -1;
  s.family_curve_sources.clear();
  s.family_curve_source_slot = 0;
}

float PointSegmentDistanceSq(float px, float py, float ax, float ay, float bx, float by) {
  const float dx = bx - ax;
  const float dy = by - ay;
  const float len_sq = dx * dx + dy * dy;
  float t = 0.0f;
  if (len_sq > 0.0f) {
    t = ((px - ax) * dx + (py - ay) * dy) / len_sq;
    t = std::clamp(t, 0.0f, 1.0f);
  }
  const float cx = ax + t * dx;
  const float cy = ay + t * dy;
  const float ex = px - cx;
  const float ey = py - cy;
  return ex * ex + ey * ey;
}

bool ArcStrokeHit(const ReferenceArcStroke& stroke, float px, float py, float tolerance_sq) {
  for (size_t i = 0; i + 3 < stroke.segments.size(); i += 4) {
    if (PointSegmentDistanceSq(px, py, stroke.segments[i], stroke.segments[i + 1], stroke.segments[i + 2],
                               stroke.segments[i + 3]) <= tolerance_sq) {
      return true;
    }
  }
  return false;
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
                   double* out_y, bool clip_viewport = true) {
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
  const auto screen = ProjectWorldDirToScreen(view.camera, travel, view.width, view.height, clip_viewport);
  if (screen[0] == kOverlaySentinel)
    return false;
  *out_x = pic_min.x + (screen[0] / view.width + 0.5f) * view.width_pt;
  *out_y = pic_min.y + (0.5f - screen[1] / view.height) * view.height_pt;
  return true;
}

// Clip the whole segment, not its endpoints' membership: dropping an outside endpoint also
// drops the visible tail. Account nominal canvas geometry here, just as the point walk did:
// ancestor clip remains a draw-command concern, while the readout independently clamps into
// that effective clip (including when its nominal anchor is hidden behind it).
bool ClipCanvasSegment(const MapCanvasProjection& view, const ImVec2& origin, double& x0, double& y0, double& x1,
                       double& y1) {
  double lo = 0, hi = 1;
  const double dx = x1 - x0, dy = y1 - y0;
  const auto cut = [&](double start, double delta, double lower, double upper) {
    if (lower > upper)
      return false;
    if (delta == 0)
      return start >= lower && start <= upper;
    double t0 = (lower - start) / delta, t1 = (upper - start) / delta;
    if (t0 > t1)
      std::swap(t0, t1);
    lo = std::max(lo, t0);
    hi = std::min(hi, t1);
    return lo <= hi;
  };
  if (!cut(x0, dx, origin.x, origin.x + view.width_pt) || !cut(y0, dy, origin.y, origin.y + view.height_pt))
    return false;
  x1 = x0 + hi * dx;
  y1 = y0 + hi * dy;
  x0 += lo * dx;
  y0 += lo * dy;
  return true;
}

// One projected pair's screen continuity — the projection's own discontinuities, not a
// per-sample property. The dual-fisheye family (a full-sky lens — no view transform, the world
// travel direction goes straight in) splits the sky into two disks by the travel direction's z
// sign: consecutive samples on opposite sides of the split belong to different disks, and the
// segment between them does not exist on the picture. The rectangular equirect wraps at
// longitude ±π and anchors the pole itself to longitude 0 — a pair straddling the seam, touching
// the anchored pole, or with the pole passed between its two samples screens farther apart than
// the samples' own bounded step can explain, and the chord between them is likewise not the
// path. One predicate, two consumers —
// the iso-delta walk (a break starts a fresh polyline at the next in-disk pair) and the profile
// device (a break voids the all-or-nothing interval). The single-fisheye family and the globe
// project into one contiguous region with a behind-camera sentinel, where a projected pair is
// always continuous.
bool ProjectionContinuous(int lens_type, double travel_prev_z, double travel_next_z, const double travel_prev[3],
                          const double travel_next[3]) {
  if (lens_type == kLensTypeDualFisheyeEqualArea || lens_type == kLensTypeDualFisheyeEquidist ||
      lens_type == kLensTypeDualFisheyeStereographic || lens_type == kLensTypeDualFisheyeOrthographic) {
    return (travel_prev_z < 0.0) == (travel_next_z < 0.0);
  }
  if (lens_type == kLensTypeRectangular) {
    // The travel directions' own longitudes, spelled as ProjectRectangular spells them; the
    // pole anchor's epsilon is that branch's own (the squared x-y norm below it).
    const double prev_pole = travel_prev[0] * travel_prev[0] + travel_prev[1] * travel_prev[1] < 1e-6;
    const double next_pole = travel_next[0] * travel_next[0] + travel_next[1] * travel_next[1] < 1e-6;
    if (prev_pole || next_pole) {
      return false;  // the anchored pole column: a neighbour pairs with it across a whole-canvas jump
    }
    const double lon_a = std::atan2(-travel_prev[1], -travel_prev[0]);
    const double lon_b = std::atan2(-travel_next[1], -travel_next[0]);
    // A swing past a quarter turn is beyond any same-side pair's own span at these sampling
    // densities (a pair whose path keeps farther from the pole than roughly its own separation
    // swings under a quarter turn), so it marks one of the projection's own breaks: the ±π seam
    // straddled (a represented swing past π), or the pole passed BETWEEN the two samples — the
    // meridian's two sides read half a canvas apart while the path between them clips the pole
    // column. Both are breaks: under-draw a near-pole graze, never draw a chord the projection
    // does not carry. The walk's own pole passages land on a sample (the radial basis is
    // zenith-referenced and kOverlaySegments is even, so the passage sits at φ = 0/π) and the
    // anchor rule above fires there; the profile interval's δ grid does not align — that
    // straddle is exactly what this bound catches.
    return std::fabs(lon_a - lon_b) <= kPiD / 2.0;
  }
  return true;
}

// One drawn segment of the iso-deviation circle's walk: whether any point landed in the disk,
// and the topmost (smallest screen y) of them — the label anchor the curve's readout sits at.
struct IsoDeltaWalk {
  int drawn = 0;            // the CURRENT contiguous polyline run (a disk exit breaks the run and resets
                            // it — the polyline's segmentation, not the walked amount)
  int lines_submitted = 0;  // AddLine calls actually emitted — the ONE "actually drew" fact the
                            // legend gates and the label read. The walked sample count is
                            // samples_in_disk (same value every counter here used to
                            // double-carry); a dashed walk submits only its on-phase pairs, so
                            // walked > 0 with nothing drawn is a real state the gates must not
                            // light.
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
// projected, split where the disk leaves the view. `dashed` is the verdict anchor's weakened
// form (the coverage-incomplete honesty, §16.1's visual grammar): every dash-cycle's second half
// of samples skips the segment, by SAMPLE INDEX (the circle's own parameter) so the pattern is
// deterministic and view-independent — a run broken by the view disk does not restart the phase.
// The solid form (dashed=false, every existing call) emits the same AddLine sequence it always
// did, byte for byte.
// The radial basis about a sky direction — the one construction every sun-centred device shares:
// u is the zenith axis projected onto the plane normal to `sun_pos` (the x axis as the fallback
// when the zenith is too close to the sun), v = sun_pos × u. This is the same basis the engine's
// probe frame builds from the incident direction (MakeProbeFrame, whose incident is this sun
// position's negation): e1 = u and e2 = −v, identically, for every sun position — the profile
// device's azimuth mapping (φ = π − az) rests on this identity, so it must have exactly ONE
// spelling here.
void SkyRadialBasis(const double sun_pos[3], double u[3], double v[3]) {
  u[0] = 0.0;
  u[1] = 0.0;
  u[2] = 1.0;
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
  Cross3(sun_pos, u, v);
}

// The sky direction at deviation `delta_deg` from the sun, at the walk's own circle parameter
// `phi` in the basis above — the iso-deviation circle's one parameterization, shared by the
// dispersion circles, the verdict anchor and the profile device.
void SkyPointAboutSun(const double sun_pos[3], const double u[3], const double v[3], double delta_deg, double phi_rad,
                      double out[3]) {
  const double delta = delta_deg * kDeg2RadD;
  const double cos_d = std::cos(delta);
  const double sin_d = std::sin(delta);
  const double cos_p = std::cos(phi_rad);
  const double sin_p = std::sin(phi_rad);
  for (int k = 0; k < 3; ++k) {
    out[k] = cos_d * sun_pos[k] + sin_d * (cos_p * u[k] + sin_p * v[k]);
  }
}

// A profile device's screen point: the projection verdict travels with the coordinates — the
// device's honesty gates read it (a point off the view draws nothing).
struct ProfileScreenPoint {
  bool projected = false;
  double x = 0.0;
  double y = 0.0;
};

// One profile reading's point marker (the vocabulary's point device as this feature draws it):
// a dark rim under the rose fill, so the point stays legible over both bright curves and the
// dark raster — the readout backing's ink family, not a new grey.
void DrawProfilePointMarker(ImDrawList* dl, const ProfileScreenPoint& pt) {
  if (!pt.projected) {
    return;
  }
  const ImVec2 c(static_cast<float>(pt.x), static_cast<float>(pt.y));
  dl->AddCircle(c, UiPx(5.0f), IM_COL32(16, 17, 20, 200), 0, UiPx(1.4f));
  dl->AddCircleFilled(c, UiPx(4.0f), kProfileReadingColour);
}

IsoDeltaWalk WalkIsoDeltaCircle(ImDrawList* dl, const MapCanvasProjection& view, const ImVec2& pic_min, float pic_px,
                                const double sun_pos[3], double delta_deg, ImU32 colour, float thickness,
                                bool dashed = false, std::vector<float>* segment_sink = nullptr) {
  constexpr int kDashCycleSegments = 4;  // one dash phase = 4 samples: 4 drawn, then 4 skipped
                                         // (the %2 alternation makes the period 8) — a run broken
                                         // by the view disk does not restart the phase
  double u[3];
  double v[3];
  SkyRadialBasis(sun_pos, u, v);
  IsoDeltaWalk walk;
  double last_x = 0.0;
  double last_y = 0.0;
  double prev_travel_z = 0.0;
  double prev_travel[3] = { 0.0, 0.0, 0.0 };
  for (int s = 0; s <= kOverlaySegments; ++s) {
    const double phi = 2.0 * kPiD * s / kOverlaySegments;
    double p[3];
    SkyPointAboutSun(sun_pos, u, v, delta_deg, phi, p);
    double x = 0.0;
    double y = 0.0;
    if (!ProjectToRect(view, p, pic_min, pic_px, &x, &y, false)) {
      walk.drawn = 0;  // the lens domain broke; off-canvas endpoints remain available for clipping
      continue;
    }
    const bool in_canvas =
        x >= pic_min.x && x <= pic_min.x + view.width_pt && y >= pic_min.y && y <= pic_min.y + view.height_pt;
    walk.samples_in_disk += in_canvas;
    const double travel[3] = { -p[0], -p[1], -p[2] };  // the projection's own input (ProjectToRect negates)
    const bool same_disk =
        s == 0 || ProjectionContinuous(view.camera.lens_type, prev_travel_z, travel[2], prev_travel, travel);
    prev_travel_z = travel[2];
    for (int k = 0; k < 3; ++k) {
      prev_travel[k] = travel[k];
    }
    if (walk.drawn > 0 && same_disk && (!dashed || (s / kDashCycleSegments) % 2 == 0)) {
      double x0 = last_x, y0 = last_y, x1 = x, y1 = y;
      if (ClipCanvasSegment(view, pic_min, x0, y0, x1, y1)) {
        dl->AddLine(ImVec2(static_cast<float>(x0), static_cast<float>(y0)),
                    ImVec2(static_cast<float>(x1), static_cast<float>(y1)), colour, thickness);
        ++walk.lines_submitted;
        if (segment_sink != nullptr) {
          // The click surface records exactly what was submitted — the clipped, continuous
          // segments only, never the ideal circle.
          segment_sink->push_back(static_cast<float>(x0));
          segment_sink->push_back(static_cast<float>(y0));
          segment_sink->push_back(static_cast<float>(x1));
          segment_sink->push_back(static_cast<float>(y1));
        }
        if (!walk.have_top || std::min(y0, y1) < walk.top_y) {
          walk.have_top = true;
          walk.top_x = y0 < y1 ? x0 : x1;
          walk.top_y = std::min(y0, y1);
        }
      }
    }
    if (!same_disk) {
      walk.drawn = 0;  // a projection break (disk flip or seam/pole jump) ends the run, the same
                       // way an off-view sample does — the next in-disk pair starts fresh
    }
    last_x = x;
    last_y = y;
    ++walk.drawn;
    if (in_canvas && (!walk.have_top || y < walk.top_y)) {
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
    if (ProjectToRect(view, p1, pic_min, pic_px, &x1, &y1, false) &&
        ProjectToRect(view, p2, pic_min, pic_px, &x2, &y2, false) && ClipCanvasSegment(view, pic_min, x1, y1, x2, y2)) {
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

void DrawMapReadoutLabels(ImDrawList* dl, const std::vector<MapReadoutLabel>& labels, float x, float y, float width,
                          float height) {
  if (labels.empty()) {
    return;
  }
  const ImVec2 clip_min = dl->GetClipRectMin();
  const ImVec2 clip_max = dl->GetClipRectMax();
  const LabelViewport visible{ std::max(x, clip_min.x), std::max(y, clip_min.y),
                               std::max(0.0f, std::min(x + width, clip_max.x) - std::max(x, clip_min.x)),
                               std::max(0.0f, std::min(y + height, clip_max.y) - std::max(y, clip_min.y)) };
  const float pad = UiPx(3.0f);
  const float gap = UiPx(4.0f);
  const float inset = kLabelViewportInsetPx;
  const float max_width = visible.w - 2.0f * inset;
  const float max_height = visible.h - 2.0f * inset;
  struct Patch {
    LabelPos pos;
    LabelSize size;
    float wrap = 0.0f;
  };
  std::vector<Patch> patches;
  // Try the complete, single-line labels near their own anchors first. The shared clamp owns
  // edge placement; the caller rejects its degenerate axis rather than pretending it fits.
  for (const auto& label : labels) {
    const ImVec2 text_size = ImGui::CalcTextSize(label.text.c_str());
    const LabelSize size{ text_size.x + 2.0f * pad, text_size.y };
    bool placed = false;
    if (size.w < max_width && size.h < max_height) {
      const float step = size.h + gap;
      const int rows = static_cast<int>(max_height / step) + 1;
      for (int row = 0; row <= rows && !placed; ++row) {
        for (const float direction : { 1.0f, -1.0f }) {
          const LabelPos candidate = ClampLabelPosToViewport(
              { label.anchor_x - size.w * 0.5f, label.anchor_y - size.h + direction * row * step }, size, visible);
          const bool overlaps = std::any_of(patches.begin(), patches.end(), [&](const Patch& p) {
            return candidate.x < p.pos.x + p.size.w + gap && candidate.x + size.w + gap > p.pos.x &&
                   candidate.y < p.pos.y + p.size.h + gap && candidate.y + size.h + gap > p.pos.y;
          });
          if (!overlaps) {
            patches.push_back({ candidate, size });
            placed = true;
            break;
          }
        }
      }
    }
    if (!placed) {
      break;
    }
  }
  bool stacked = patches.size() != labels.size();
  if (stacked) {
    // A narrow map gets one wrapped reading stack, never a smaller font or a missing colour.
    patches.clear();
    const float wrap = std::max(1.0f, max_width - 2.0f * pad);
    float top = visible.y + inset + ImGui::GetTextLineHeight() + gap;
    for (const auto& label : labels) {
      const ImVec2 ts = ImGui::CalcTextSize(label.text.c_str(), nullptr, false, wrap);
      patches.push_back({ { visible.x + inset, top }, { ts.x + 2.0f * pad, ts.y }, wrap });
      top += ts.y + gap;
    }
    if (max_width <= 2.0f * pad || top > visible.y + visible.h - inset) {
      // Even the stack cannot fit (e.g. a splitter compressed to its floor). Advertise the
      // loss of inline room explicitly; the normal wrapped tooltip keeps EVERY full reading.
      // This is display-only and has no item/click ownership to steal from the map gestures.
      const char* summary = "Readouts: hover for all";
      const ImVec2 ts = ImGui::CalcTextSize(summary, nullptr, false, wrap);
      if (ts.x + 2.0f * pad < max_width && ts.y < max_height) {
        const ImVec2 p(visible.x + inset, visible.y + inset);
        dl->AddRectFilled(p, ImVec2(p.x + ts.x + 2.0f * pad, p.y + ts.y), kReadoutBackingInk);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(p.x + pad, p.y), ImGui::GetColorU32(ImGuiCol_Text),
                    summary, nullptr, wrap);
        if (ImGui::IsMouseHoveringRect(p, ImVec2(p.x + ts.x + 2.0f * pad, p.y + ts.y)) && ImGui::BeginTooltip()) {
          WrappedTooltipText("Map too small for inline readouts. Enlarge it to restore labels.");
          for (const auto& label : labels) {
            WrappedTooltipText("%s", label.text.c_str());
          }
          ImGui::EndTooltip();
        }
      }
      return;
    }
  }
  // Leaders go below ALL patches, so a later curve or connector cannot overwrite an earlier
  // label. Each leader ends at the curve's original sample; neither walk nor physics moves.
  for (size_t i = 0; labels.size() > 1 && i < labels.size(); ++i) {
    const auto& p = patches[i];
    const auto& label = labels[i];
    const ImVec2 anchor(label.anchor_x, label.anchor_y);
    const ImVec2 end(std::clamp(label.anchor_x, p.pos.x, p.pos.x + p.size.w),
                     std::clamp(label.anchor_y, p.pos.y, p.pos.y + p.size.h));
    dl->AddLine(anchor, end, IM_COL32(16, 17, 20, 240), UiPx(3.0f));
    dl->AddLine(anchor, end, label.colour, UiPx(1.0f));
    dl->AddCircleFilled(anchor, UiPx(2.0f), label.colour);
  }
  if (stacked) {
    dl->AddText(ImVec2(visible.x + inset + pad, visible.y + inset), ImGui::GetColorU32(ImGuiCol_Text),
                "Readouts (stacked)");
  }
  for (size_t i = 0; i < labels.size(); ++i) {
    const auto& p = patches[i];
    dl->AddRectFilled(ImVec2(p.pos.x, p.pos.y), ImVec2(p.pos.x + p.size.w, p.pos.y + p.size.h), kReadoutBackingInk);
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(p.pos.x + pad, p.pos.y), labels[i].colour,
                labels[i].text.c_str(), nullptr, p.wrap);
  }
}

// The marker vocabulary: a shape and a colour per kind (the engine's six kinds plus the
// fallback), solid for a concluded claim (certified/finite) and outline-only otherwise — the
// refused/unevaluated rows must not read as answered.
// The engine kind name this file's caustic logic compares against — ONE spelling here: the
// visibility predicate, the cluster collection, the style table, the popup's absence read and
// the display-vocabulary mapping all test the same wire string, so a rename touches one line.
constexpr char kCausticEdgeKindName[] = "caustic edge";

// The ONE points-row visibility rule (the header's contract): the positioned caustic rows read
// their OWN group's gate — the Point images parent neither shows nor buries them (the pile's
// declutter switch must not ride the images' parent) — and the points group's rows take the
// parent gate plus the three positioned image kinds' sub-gates. The draw loop, the hit
// collection and the legend all read the row through this predicate — hidden kinds draw no
// marker, join no hover group and earn no legend entry from the very same check.
bool PointMarkerVisible(const SurveyObjectRow& row, const SurveyLayerGates& gates) {
  if (row.kind_name == kCausticEdgeKindName) {
    // Positioned rows only: the T1 arm's same-kind card carries a deviation-axis position and
    // never a sky position (survey_t1 assembles SetEdge without SetPosition, so has_position
    // stays false). The draw loop answers THIS predicate first and skips !position_available
    // rows right after, so the unpositioned T1 card draws nothing either way and its Sharp
    // edges route stands. Should the engine ever give that card a sky position, THIS branch is
    // what would silently re-route it into this group's switch.
    return gates.caustic.graphics;
  }
  if (!gates.points.graphics) {
    return false;
  }
  if (row.kind_name == "source image") {
    return gates.points_kinds.source_image;
  }
  if (row.kind_name == "regular image") {
    return gates.points_kinds.regular_image;
  }
  if (row.kind_name == "family collapse") {
    return gates.points_kinds.family_collapse;
  }
  return true;  // kinds outside the sub-gate vocabulary follow the parent gate alone
}

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
  } else if (kind_name == kCausticEdgeKindName) {
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

bool OverlayClusterAllConcluded(const std::vector<int>& states) {
  for (const int s : states) {
    if (s != LUMICE_PATH_STATE_CERTIFIED && s != LUMICE_PATH_STATE_FINITE) {
      return false;
    }
  }
  return true;
}

void GroupOverlayScreenClusters(const std::vector<OverlayScreenPoint>& points, float radius,
                                std::vector<std::vector<size_t>>* clusters) {
  clusters->clear();
  const size_t n = points.size();
  std::vector<bool> assigned(n, false);
  const float r2 = radius * radius;
  for (size_t i = 0; i < n; ++i) {
    if (assigned[i]) {
      continue;
    }
    assigned[i] = true;
    clusters->emplace_back(1, i);  // the seed: the cluster's anchor (a real representative)
    std::vector<size_t>& cluster = clusters->back();
    // Complete-linkage growth in input order: a candidate joins only when it lies within the
    // radius of EVERY current member. The diameter stays bounded, so a chain of far-apart
    // representatives can never fold into one spanning cluster — the anti-case the transitive
    // closure would fail.
    for (size_t j = i + 1; j < n; ++j) {
      if (assigned[j]) {
        continue;
      }
      bool fits = true;
      for (const size_t m : cluster) {
        const float dx = points[j].x - points[m].x;
        const float dy = points[j].y - points[m].y;
        if (dx * dx + dy * dy > r2) {
          fits = false;
          break;
        }
      }
      if (fits) {
        assigned[j] = true;
        cluster.push_back(j);
      }
    }
  }
}

// The display grouping's hover: `record_noun` names what the count line counts — the plain
// rows keep "object", a caustic pile's members are "representative record"s (the cluster's
// ×N reads the same count; the noun is the map's display vocabulary, not an engine claim).
void RenderSurveyObjectTooltip(const std::vector<const SurveyObjectRow*>& hits, const char* record_noun) {
  if (hits.empty() || !ImGui::BeginTooltip()) {
    return;
  }
  const SurveyObjectRow& first = *hits.front();
  ImGui::TextUnformatted("Survey objects");
  ImGui::TextDisabled("alt %.2f° az %.2f° · %zu %s%s", first.position_altitude_deg, first.position_azimuth_deg,
                      hits.size(), record_noun, hits.size() == 1 ? "" : "s");
  std::vector<OverlayHitGroup> groups;
  GroupOverlayHits(hits, &groups);
  for (const OverlayHitGroup& group : groups) {
    const SurveyObjectRow& row = *group.representative;
    ImGui::Text("%s · %s", row.kind_name.c_str(), row.state_name.c_str());
    if (group.count > 1) {
      ImGui::SameLine();
      ImGui::TextDisabled("×%d identical", group.count);
    }
    // Keep the evidence fields visible where two grouped entries differ.
    char pos_line[96];
    if (row.family_level >= 0 && row.blur_deg > 0.0) {
      std::snprintf(pos_line, sizeof(pos_line), "alt %.2f° az %.2f° · level %d · blur %.2f°", row.position_altitude_deg,
                    row.position_azimuth_deg, row.family_level, row.blur_deg);
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
      WrappedTooltipText("%s", row.reason.c_str());
    }
  }
  ImGui::EndTooltip();
}

namespace {

// The map's display vocabulary for a positioned caustic row (the two-arm kind-name split): the
// row's stored kind stays "caustic edge" — the engine's one wire kind, shared with the T1 arm's
// deviation-axis reference card — while the marker THIS map draws for it is a family
// representative. The standing words (the short label and the legend's entry) say what the ink
// is; the hover reads the raw row back. One mapping, two consumers; "Sharp edges" keeps the T1
// curves' own name, and the T1 card itself never draws a marker (it carries no sky position).
const char* OverlayKindDisplayName(const std::string& kind_name) {
  if (kind_name == kCausticEdgeKindName) {
    return "caustic representative";
  }
  return kind_name.c_str();
}

struct SurveyOverlayCache {
  std::weak_ptr<const LUMICE_PathSurvey> source;
  SurveyQueryId id;
  bool point_sun_premise = false;
  SurveyOverviewModel model;
  SurveyOverlayFacts facts;
};
SurveyOverlayCache g_survey_overlay_cache;

// One submitted curve ink's click surface (task arc-recovery-engine; frame-transient like the
// reference arcs' strokes — never stored): the pick identity it would commit and the screen
// segment it submitted (a == b for the markers' point surfaces). The stationary-release test
// reads the frame's surfaces at the press position; ink nothing covered cannot be picked.
struct FamilyCurveHitSurface {
  FamilyCurvePick pick;
  ImVec2 a;
  ImVec2 b;
};

// The one refresh per frame path (idempotent): a stale identity rebuilds BOTH pure reads — the
// overview model and the layer facts — once, from the immutable document. The popup's empty-state
// notes and the arcs read this same entry, so the facts exist even on a frame whose master gate
// suppresses every draw (a toggle edits display preferences; it never re-reads the document).
const SurveyOverlayCache& RefreshSurveyOverlayCache(const SurveyQuerySession& survey, bool point_sun_premise) {
  auto& cache = g_survey_overlay_cache;
  if (cache.source.lock() != survey.adopted_result || cache.id != survey.intent_id ||
      cache.point_sun_premise != point_sun_premise) {
    // The document is immutable. Build once per adopted identity, not once per observation; the
    // premise rides the identity (a freeze capture — it only changes with a new analyze, which
    // changes the identity anyway, but the compare keeps the cache honest to its own key).
    cache = {};
    if (survey.adopted_result != nullptr) {
      cache.model = BuildSurveyOverviewModel(*survey.adopted_result, point_sun_premise);
      cache.point_sun_premise = point_sun_premise;
      cache.facts = BuildSurveyOverlayFacts(*survey.adopted_result);
      cache.source = survey.adopted_result;
      cache.id = survey.intent_id;
    }
  }
  return cache;
}

// The family curve pick's identity header (task arc-recovery-engine): the curve's kind words
// and its literal member/level — the 820 ruling's theoretical caliber, never an actual-measure
// interpretation.
void RenderFamilyCurveHeader(const SurveyOverlayFacts::FamilyCurveFact& curve) {
  const char* kind = curve.kind == LUMICE_PATH_CURVE_KIND_REGULAR_TRACK ? "regular track" : "fold arc";
  std::string member;
  for (size_t i = 0; i < curve.member.size(); ++i) {
    if (i > 0) {
      member += "-";
    }
    member += std::to_string(curve.member[static_cast<size_t>(i)]);
  }
  ImGui::Text("%s · level %d · member %s", kind, curve.level, member.c_str());
}

// One witness line pair: the parameter preimage, the sky position, and the pose (the engine's
// own rows — recomputable from exactly these parameters, the plan's witness contract).
void RenderFamilyCurvePoseWitness(const double params_deg[2], const double pose[9], double alt_deg, double az_deg) {
  ImGui::TextDisabled("params %.3f° %.3f° · alt %.3f° az %.3f°", params_deg[0], params_deg[1], alt_deg, az_deg);
  ImGui::TextDisabled("pose (recomputable from the params):");
  for (int r = 0; r < 3; ++r) {
    ImGui::TextDisabled("  %+.6f %+.6f %+.6f", pose[r * 3], pose[r * 3 + 1], pose[r * 3 + 2]);
  }
}

// The family curve pick's tooltip (task arc-recovery-engine): the hovered event's own witness,
// per kind — a chain point reads the line's nearest endpoint, a point-image run reads the
// collapsed marker's segment (the run's constant image, its measured diameter, and one legal
// preimage of the run), a cusp candidate reads the located event's own refined pose (never the
// neighbouring crossing's). Nothing here re-asks the engine.
void RenderFamilyCurveTooltip(const SurveyOverlayFacts::FamilyCurveFact& curve, const FamilyCurvePick& pick) {
  if (!ImGui::BeginTooltip()) {
    return;
  }
  switch (pick.kind) {
    case FamilyCurvePickKind::kPoint: {
      ImGui::TextUnformatted("Family curve");
      RenderFamilyCurveHeader(curve);
      if (pick.point_index >= 0 && pick.point_index < static_cast<int>(curve.points.size())) {
        const SurveyOverlayFacts::FamilyCurveFact::Point& p = curve.points[static_cast<size_t>(pick.point_index)];
        RenderFamilyCurvePoseWitness(p.params_deg, p.pose, p.alt_deg, p.az_deg);
      }
      break;
    }
    case FamilyCurvePickKind::kPointImage: {
      ImGui::TextUnformatted("Point image");
      RenderFamilyCurveHeader(curve);
      if (pick.segment_index >= 0 && pick.segment_index < static_cast<int>(curve.segments.size())) {
        const SurveyOverlayFacts::FamilyCurveFact::Segment& s = curve.segments[static_cast<size_t>(pick.segment_index)];
        ImGui::TextDisabled("this run of the branch collapses to one image");
        ImGui::TextDisabled("constant image alt %.3f° az %.3f° · measured diameter %.2e°", s.image_alt_deg,
                            s.image_az_deg, s.image_diameter_deg);
        // A legal preimage of the run (its first point): the constant image is the run's mean —
        // the pose witness comes from a real locus point, not from the mean.
        if (s.first >= 0 && s.first < static_cast<int>(curve.points.size())) {
          const SurveyOverlayFacts::FamilyCurveFact::Point& p = curve.points[static_cast<size_t>(s.first)];
          ImGui::TextDisabled("witness preimage (the run's first locus point):");
          RenderFamilyCurvePoseWitness(p.params_deg, p.pose, p.alt_deg, p.az_deg);
        }
      }
      break;
    }
    case FamilyCurvePickKind::kCusp: {
      ImGui::TextUnformatted("Cusp candidate");
      RenderFamilyCurveHeader(curve);
      if (pick.cusp_index >= 0 && pick.cusp_index < static_cast<int>(curve.cusps.size())) {
        const SurveyOverlayFacts::FamilyCurveFact::Cusp& c = curve.cusps[static_cast<size_t>(pick.cusp_index)];
        ImGui::TextDisabled("located transversality failure — a candidate, not a certified cusp");
        ImGui::TextDisabled("|dJ(v)| residual bound %.2e", c.djv_residual);
        RenderFamilyCurvePoseWitness(c.params_deg, c.pose, c.alt_deg, c.az_deg);
      }
      break;
    }
  }
  ImGui::EndTooltip();
}

void DrawSurveyOverlays(ImDrawList* dl, GuiState& state, const MapCanvasProjection& view, const ImVec2& pic_min,
                        float pic_px, OverlayLegendState* legend, const SurveyLayerGates& gates,
                        int selected_edge_object_index, std::vector<ReferenceArcStroke>* stroke_sink,
                        std::vector<FamilyCurveHitSurface>* curve_hit_sink) {
  SurveyQuerySession& survey = state.analysis.survey;
  if (survey.adopted_result == nullptr) {
    return;
  }
  // The payloads read through the ONE document->facts mapping (§5.1.8: the overview model is the
  // single organizer; the map overlays consume its object rows and dispersion block verbatim).
  // The refresh is idempotent and already ran at the region's entry — this read cannot rebuild.
  const SurveyOverlayCache& cache = RefreshSurveyOverlayCache(survey, state.analysis.FrozenPointSunPremise());
  const SurveyOverviewModel& model = cache.model;
  const SurveyOverlayFacts& facts = cache.facts;
  // The reference arcs (reach bounds, sharp edges, TIR turns): the frame's first analysis ink,
  // under everything the other groups draw. Their readout labels join the frame's one label set.
  std::vector<MapReadoutLabel> labels;
  ReferenceArcInput arcs;
  arcs.camera = &view.camera;
  arcs.width_px = view.width;
  arcs.height_px = view.height;
  arcs.width_pt = view.width_pt;
  arcs.height_pt = view.height_pt;
  arcs.pic_min_x = pic_min.x;
  arcs.pic_min_y = pic_min.y;
  arcs.pic_px = pic_px;
  SkySunAnchorFor(state, &arcs.sun);
  arcs.reach = gates.reach;
  arcs.edge = gates.edge;
  arcs.tir = gates.tir;
  arcs.legend = legend;
  arcs.labels_sink = &labels;
  // The selected object's feedback and this frame's click surfaces (817.2): the selection reads,
  // the sink fills — neither recomputes anything.
  arcs.selected_edge_object_index = selected_edge_object_index;
  arcs.stroke_sink = stroke_sink;
  // The arcs' FIXED reference calibers, read off the cached model: the survey's own scope
  // wavelength names the reach/edge bounds (whatever the map's λ slider shows), and the turn
  // pairs quote the chromatic pair's wavelength words — a turn's two deltas are that pair's
  // values, not the scope wavelength's.
  if (model.scope_available) {
    arcs.reference_lambda_available = true;
    arcs.reference_lambda_nm = static_cast<float>(model.wavelength_nm);
  }
  if (model.dispersion.available && !model.dispersion.red_label.empty() && !model.dispersion.blue_label.empty()) {
    arcs.tir_lambda_text = model.dispersion.red_label + "/" + model.dispersion.blue_label;
  }
  DrawReferenceArcs(dl, facts, arcs);
  // The family arm's ordered curves (task arc-recovery-engine): the drawable critical arcs and
  // regular tracks. The DRAW unit is the curve's verified adjacency, not the image-form runs:
  // a fold chain draws every consecutive ordered pair (each is a cell-justified link — the
  // chain's order IS the link sequence) plus the closing pair when closed; only a link whose
  // both ends sit inside point-image runs draws nothing (the run IS its marker), and a regular
  // track keeps its run-by-run lines (the runs are validity segments — real gaps). A cusp
  // candidate is a bounded tick on its arc. The seam rule is display-level, per link: a
  // submitted line breaks when either endpoint leaves the view or the two projections jump more
  // than a view radius (the physical wrap's own evidence) — never bridged. The pick channel is
  // the hover (the nearest submitted ink's own event witness — point, point-image run or cusp),
  // and every submitted ink records its click surface for the stationary-click selection.
  int family_curve_entries = 0;
  // The hover state is filled during the draw and consumed after the markers' tooltip: the
  // map window must actually be hovered (a popup or another window over the picture swallows
  // the hover — otherwise the curve tooltip would eat the next click), and the marker tooltip
  // owns the pointer whenever a marker is under it (one tooltip per frame).
  const SurveyOverlayFacts::FamilyCurveFact* curve_hovered = nullptr;
  FamilyCurvePick curve_hovered_pick;
  float curve_hovered_dist = 1e30f;
  if (gates.curves.graphics) {
    // The ink draws whenever the group is on; only the PICK records against the pointer, and
    // only while the map window is actually hovered (a popup or another window over the
    // picture must not spawn a curve tooltip — it would eat the next click). The click
    // surfaces record whenever the ink submits — the stationary-release test reads them from
    // the frame that was on screen when the press landed.
    const bool pointer_pickable = ImGui::IsWindowHovered();
    float hovered_dist = 1e30f;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    auto consider_hover = [&](const FamilyCurvePick& pick, float d) {
      if (pointer_pickable && d < hovered_dist) {
        hovered_dist = d;
        curve_hovered = &facts.family_curves[static_cast<size_t>(pick.curve_index)];
        curve_hovered_pick = pick;
      }
    };
    auto submit_surface = [&](const FamilyCurvePick& pick, const ImVec2& a, const ImVec2& b) {
      if (curve_hit_sink != nullptr) {
        curve_hit_sink->push_back({ pick, a, b });
      }
    };
    for (size_t ci = 0; ci < facts.family_curves.size(); ++ci) {
      const SurveyOverlayFacts::FamilyCurveFact& curve = facts.family_curves[ci];
      const int curve_index = static_cast<int>(ci);
      bool drew_any = false;
      // The point-image markers first (one per collapsed run, at its verified constant image).
      for (size_t si = 0; si < curve.segments.size(); ++si) {
        const SurveyOverlayFacts::FamilyCurveFact::Segment& segment = curve.segments[si];
        if (segment.form != LUMICE_PATH_CURVE_SEGMENT_FORM_POINT_IMAGE) {
          continue;
        }
        double pos[3];
        SkyPosition(segment.image_alt_deg, segment.image_az_deg, pos);
        double x = 0.0;
        double y = 0.0;
        if (!ProjectToRect(view, pos, pic_min, pic_px, &x, &y)) {
          continue;
        }
        const ImVec2 center(static_cast<float>(x), static_cast<float>(y));
        dl->AddCircleFilled(center, UiPx(4.0f), kFamilyPointInk);
        dl->AddCircle(center, UiPx(4.0f), kFamilyArcInk, 0, UiPx(1.2f));
        drew_any = true;
        FamilyCurvePick pick;
        pick.curve_index = curve_index;
        pick.kind = FamilyCurvePickKind::kPointImage;
        pick.segment_index = static_cast<int>(si);
        submit_surface(pick, center, center);
        const float dx = mouse.x - center.x;
        const float dy = mouse.y - center.y;
        consider_hover(pick, dx * dx + dy * dy);
      }
      const bool is_track = curve.kind == LUMICE_PATH_CURVE_KIND_REGULAR_TRACK;
      // Which order positions sit inside a point-image run (fold chains): the run's marker
      // owns the line there — a link with both ends inside draws nothing, a transition link
      // (collapsed end to extended end) draws to the collapsed end's own locus point.
      std::vector<char> collapsed(curve.points.size(), 0);
      for (const SurveyOverlayFacts::FamilyCurveFact::Segment& segment : curve.segments) {
        if (segment.form == LUMICE_PATH_CURVE_SEGMENT_FORM_POINT_IMAGE) {
          for (int i = segment.first; i <= segment.last && i < static_cast<int>(collapsed.size()); ++i) {
            collapsed[static_cast<size_t>(i)] = 1;
          }
        }
      }
      // The line submitter (the seam rule per submitted pair; the hover picks the nearer
      // endpoint's witness).
      const auto draw_pair = [&](int a_index, int b_index) -> bool {
        const int n = static_cast<int>(curve.points.size());
        if (a_index < 0 || b_index < 0 || a_index >= n || b_index >= n) {
          return false;
        }
        const SurveyOverlayFacts::FamilyCurveFact::Point& pa = curve.points[static_cast<size_t>(a_index)];
        const SurveyOverlayFacts::FamilyCurveFact::Point& pb = curve.points[static_cast<size_t>(b_index)];
        double pos[3];
        double ax = 0.0;
        double ay = 0.0;
        double bx = 0.0;
        double by = 0.0;
        SkyPosition(pa.alt_deg, pa.az_deg, pos);
        const bool a_ok = ProjectToRect(view, pos, pic_min, pic_px, &ax, &ay);
        SkyPosition(pb.alt_deg, pb.az_deg, pos);
        const bool b_ok = ProjectToRect(view, pos, pic_min, pic_px, &bx, &by);
        if (!a_ok || !b_ok) {
          return false;  // an endpoint off the view: the link breaks, never bridges
        }
        const float dx = static_cast<float>(bx) - static_cast<float>(ax);
        const float dy = static_cast<float>(by) - static_cast<float>(ay);
        if (dx * dx + dy * dy >= pic_px * pic_px) {
          return false;  // the wrap's own evidence: the seam breaks the line
        }
        const ImVec2 a(static_cast<float>(ax), static_cast<float>(ay));
        const ImVec2 b(static_cast<float>(bx), static_cast<float>(by));
        dl->AddLine(a, b, kFamilyArcInk, UiPx(1.4f));
        FamilyCurvePick pick;
        pick.curve_index = curve_index;
        pick.kind = FamilyCurvePickKind::kPoint;
        const float da = (mouse.x - a.x) * (mouse.x - a.x) + (mouse.y - a.y) * (mouse.y - a.y);
        const float db = (mouse.x - b.x) * (mouse.x - b.x) + (mouse.y - b.y) * (mouse.y - b.y);
        pick.point_index = da <= db ? a_index : b_index;
        submit_surface(pick, a, b);
        consider_hover(pick, PointSegmentDistanceSq(mouse.x, mouse.y, a.x, a.y, b.x, b.y));
        return true;
      };
      if (is_track) {
        // The regular track: run by run — the runs are the validity segmentation, the real
        // gaps between them stay unbridged.
        for (const SurveyOverlayFacts::FamilyCurveFact::Segment& segment : curve.segments) {
          for (int i = segment.first; i < segment.last; ++i) {
            if (draw_pair(i, i + 1)) {
              drew_any = true;
            }
          }
        }
      } else {
        // The fold chain: every consecutive ordered pair is a verified link (the chain's order
        // IS the link sequence — form boundaries are not topology breaks), and a closed chain
        // adds the closing pair through its justifying cell.
        const int n = static_cast<int>(curve.points.size());
        for (int i = 0; i + 1 < n; ++i) {
          if (collapsed[static_cast<size_t>(i)] && collapsed[static_cast<size_t>(i + 1)]) {
            continue;  // the run's interior: the marker owns it
          }
          if (draw_pair(i, i + 1)) {
            drew_any = true;
          }
        }
        if (curve.closed && n >= 3) {
          if (!(collapsed[static_cast<size_t>(n - 1)] && collapsed[0])) {
            if (draw_pair(n - 1, 0)) {
              drew_any = true;
            }
          }
        }
      }
      // The cusp candidates: bounded ticks on their arcs (a candidate, never a certified cusp).
      for (size_t ei = 0; ei < curve.cusps.size(); ++ei) {
        const SurveyOverlayFacts::FamilyCurveFact::Cusp& cusp = curve.cusps[ei];
        double pos[3];
        SkyPosition(cusp.alt_deg, cusp.az_deg, pos);
        double x = 0.0;
        double y = 0.0;
        if (!ProjectToRect(view, pos, pic_min, pic_px, &x, &y)) {
          continue;
        }
        const ImVec2 center(static_cast<float>(x), static_cast<float>(y));
        dl->AddCircle(center, UiPx(2.5f), kFamilyCuspInk, 0, UiPx(1.6f));
        drew_any = true;
        FamilyCurvePick pick;
        pick.curve_index = curve_index;
        pick.kind = FamilyCurvePickKind::kCusp;
        pick.cusp_index = static_cast<int>(ei);
        submit_surface(pick, center, center);
        const float dx = mouse.x - center.x;
        const float dy = mouse.y - center.y;
        consider_hover(pick, dx * dx + dy * dy);
      }
      if (drew_any) {
        family_curve_entries += 1;
      }
    }
    if (legend != nullptr && family_curve_entries > 0) {
      legend->family_curves = true;
      legend->family_curve_count = family_curve_entries;
    }
    curve_hovered_dist = hovered_dist;
  }
  // Positional objects: the survey's own located conclusions, drawn where the document puts
  // them — one marker per object, styled by kind and claim state; the frame's hits collect for
  // ONE organized tooltip (every object under the pointer, grouped by the observation key),
  // never a stack of same-text tooltips. The points group's gate: off, neither markers nor their
  // tooltip collects (a hidden object does not answer the pointer).
  std::vector<const SurveyObjectRow*> hits;
  const float marker_r = UiPx(6.0f);
  // One shape vocabulary for both passes below (the per-row loop and the caustic cluster pass).
  const auto draw_overlay_marker = [&](const OverlayMarkerStyle& style, float cx, float cy) {
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
  };
  // The positioned caustic rows collect for the cluster pass below — the pile at a mirror
  // fixed point is dozens of rows the eye cannot separate, so the frame submits ONE marker per
  // indistinguishable pile plus an ×N count. The rows themselves are untouched: the grouping is
  // this frame's display derivation over the already-read-back rows (no query, no document
  // write, nothing cached across frames). Non-caustic rows keep the one-marker-per-row pass.
  std::vector<std::pair<const SurveyObjectRow*, OverlayScreenPoint>> caustic_points;
  // The legend's one note-kind rule, shared by both passes (a legend describes the drawn
  // picture; each drawn marker's kind earns at most one first-draw-order entry).
  const auto legend_note_kind = [&](const std::string& kind) {
    if (legend == nullptr) {
      return;
    }
    bool known = false;
    for (const std::string& k : legend->object_kinds) {
      if (k == kind) {
        known = true;
        break;
      }
    }
    if (!known) {
      legend->object_kinds.push_back(kind);
    }
  };
  for (const SurveyObjectRow& row : model.objects) {
    if (!PointMarkerVisible(row, gates) || !row.position_available) {
      continue;
    }
    // The ordered curves supersede their cards' aggregate representative markers (the same
    // semantic drawn twice): a card carrying a curve stays hidden while the curves group is on;
    // turning the group off returns the marker — the card's own fact is never dropped from the
    // document or the tooltip.
    if (gates.curves.graphics && row.object_index >= 0) {
      bool superseded = false;
      for (const SurveyOverlayFacts::FamilyCurveFact& curve : facts.family_curves) {
        if (curve.object_index == row.object_index) {
          superseded = true;
          break;
        }
      }
      if (superseded) {
        continue;
      }
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
    if (row.kind_name == kCausticEdgeKindName) {
      caustic_points.push_back({ &row, OverlayScreenPoint{ cx, cy } });
      continue;
    }
    const OverlayMarkerStyle style = OverlayMarkerStyleFor(row.kind_name, row.state);
    draw_overlay_marker(style, cx, cy);
    legend_note_kind(row.kind_name);
    if (gates.points.labels) {
      // The labels box's own consumer: the kind's short standing name beside its marker (the
      // plan's 辨识所需短名 — the recognisable-structure face). Hiding labels keeps the marker
      // and the hover details, never the reverse.
      const ImVec2 label_pos(cx + marker_r + UiPx(3.0f), cy - ImGui::GetTextLineHeight() * 0.5f);
      dl->AddText(label_pos, ImGui::GetColorU32(ImGuiCol_TextDisabled), row.kind_name.c_str());
    }
    if (ImGui::IsMouseHoveringRect(ImVec2(cx - marker_r, cy - marker_r), ImVec2(cx + marker_r, cy + marker_r))) {
      hits.push_back(&row);
    }
  }
  // The caustic representatives' cluster pass: the ONE grouping feeds marker, count badge,
  // label, legend and hover alike. The anchor is a real representative's projected point (the
  // seed row's own position — never an average, so no new sky point is invented, and a ±180°
  // view edge's two visible copies of one site stay two markers). A pile whose members are not
  // all concluded keeps the conservative outline; the count never reads as confidence.
  if (!caustic_points.empty()) {
    std::vector<OverlayScreenPoint> pts;
    pts.reserve(caustic_points.size());
    for (const auto& entry : caustic_points) {
      pts.push_back(entry.second);
    }
    std::vector<std::vector<size_t>> clusters;
    GroupOverlayScreenClusters(pts, marker_r, &clusters);
    for (const std::vector<size_t>& cluster : clusters) {
      const SurveyObjectRow& anchor = *caustic_points[cluster.front()].first;
      const OverlayScreenPoint at = caustic_points[cluster.front()].second;
      std::vector<int> states;
      states.reserve(cluster.size());
      for (const size_t m : cluster) {
        states.push_back(caustic_points[m].first->state);
      }
      OverlayMarkerStyle style = OverlayMarkerStyleFor(anchor.kind_name, anchor.state);
      style.filled = OverlayClusterAllConcluded(states);
      draw_overlay_marker(style, at.x, at.y);
      legend_note_kind(anchor.kind_name);
      if (gates.caustic.labels) {
        const ImVec2 label_pos(at.x + marker_r + UiPx(3.0f), at.y - ImGui::GetTextLineHeight() * 0.5f);
        dl->AddText(label_pos, ImGui::GetColorU32(ImGuiCol_TextDisabled), OverlayKindDisplayName(anchor.kind_name));
      }
      if (cluster.size() > 1) {
        // The pile's count: part of the composite marker (the labels bit does not touch it —
        // hiding the standing names must not hide how many rows the pile folds). A pile on the
        // canvas's own edge meridian — the mirror fixed points sit at ±180° — shifts its badge
        // inside the canvas instead of letting the canvas cut it; the marker itself stays at
        // the representative's true position.
        char badge[16];
        std::snprintf(badge, sizeof(badge), "×%zu", cluster.size());
        const ImVec2 ts = ImGui::CalcTextSize(badge);
        ImVec2 bmin(at.x - ts.x * 0.5f, at.y + marker_r + UiPx(1.0f));
        bmin.x = std::clamp(bmin.x, pic_min.x + UiPx(1.0f), pic_min.x + pic_px - ts.x - UiPx(1.0f));
        dl->AddRectFilled(bmin, ImVec2(bmin.x + ts.x, bmin.y + ts.y), kReadoutBackingInk);
        dl->AddText(bmin, ImGui::GetColorU32(ImGuiCol_Text), badge);
      }
      if (ImGui::IsMouseHoveringRect(ImVec2(at.x - marker_r, at.y - marker_r),
                                     ImVec2(at.x + marker_r, at.y + marker_r))) {
        for (const size_t m : cluster) {
          hits.push_back(caustic_points[m].first);
        }
      }
    }
  }
  // ONE tooltip per frame for everything under the pointer: the position header, then one line
  // per observation key (identical keys fold into an ×N count — a display grouping of the
  // model's rows; nothing drops, different sources stay separate). A caustic pile's hover says
  // what its count counts: representative records, not engine-merged objects.
  const char* record_noun = "object";
  if (!hits.empty()) {
    bool all_caustic = true;
    for (const SurveyObjectRow* row : hits) {
      if (row->kind_name != kCausticEdgeKindName) {
        all_caustic = false;
        break;
      }
    }
    if (all_caustic) {
      record_noun = "representative record";
    }
  }
  RenderSurveyObjectTooltip(hits, record_noun);
  // The family curve pick: the nearest submitted segment's witness under the pointer —
  // rendered only when no marker is under the pointer (the marker tooltip owns it then; one
  // tooltip per frame). The tolerance is the pick's own: a 7pt radius around the submitted
  // line.
  if (hits.empty() && curve_hovered != nullptr && curve_hovered_dist <= UiPx(7.0f) * UiPx(7.0f)) {
    RenderFamilyCurveTooltip(*curve_hovered, curve_hovered_pick);
  }
  // The chromatic group's gate wraps BOTH red-blue ink sources — the dispersion pair and the
  // verdict anchor draw together or not at all (a half-open chromatic group would leave one
  // semantic colour claiming the other's absence). The neutral layers above are untouched by it;
  // the group's labels bit gates the readouts, never the ink.
  if (gates.chromatic.graphics && model.dispersion.available && model.dispersion.min_row_available) {
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
      { model.dispersion.value_red_deg, kChromaticColourRed, &model.dispersion.red_label,
        legend != nullptr ? &legend->dispersion_red : &legend_bits_sink[0] },
      { model.dispersion.value_blue_deg, kChromaticColourBlue, &model.dispersion.blue_label,
        legend != nullptr ? &legend->dispersion_blue : &legend_bits_sink[1] },
    };
    for (const Curve& curve : curves) {
      // One walk draws the circle and reports its topmost in-disk sample — the label anchor (the
      // readout the eye meets). A partially visible circle says so: the drawn arc is the
      // in-view part, not the whole circle. The legend and the label read the SUBMITTED line
      // count: a legend describes the drawn picture, and a walk that only grazed the disk
      // (samples in, but no two consecutive) draws no curve and earns no entry.
      const IsoDeltaWalk walk =
          WalkIsoDeltaCircle(dl, view, pic_min, pic_px, sun_pos, curve.delta_deg, curve.colour, 1.4f);
      if (legend != nullptr && walk.lines_submitted > 0) {
        *curve.legend_bit = true;
        if (walk.partial()) {
          legend->dispersion_partial = true;
        }
      }
      if (walk.lines_submitted > 0 && gates.chromatic.labels) {
        char line[112];
        std::snprintf(line, sizeof(line), "%s · %.3f°%s", curve.label->c_str(), curve.delta_deg,
                      walk.partial() ? " · arc in view" : "");
        labels.push_back({ static_cast<float>(walk.top_x), static_cast<float>(walk.top_y), curve.colour, line });
      }
    }
  }
  // Qualified verdicts join the same chromatic group gate and readout collection as dispersion.
  // Honesty, the closed kind set and colour routing remain the anchor draw's own; its readout
  // label obeys the same labels toggle (the anchor's ink draws, its text waits).
  if (gates.chromatic.graphics) {
    VerdictAnchorInput verdict;
    verdict.camera = &view.camera;
    verdict.width_px = view.width;
    verdict.height_px = view.height;
    verdict.width_pt = view.width_pt;
    verdict.height_pt = view.height_pt;
    verdict.pic_min_x = pic_min.x;
    verdict.pic_min_y = pic_min.y;
    verdict.pic_px = pic_px;
    SkySunAnchorFor(state, &verdict.sun);
    verdict.labels = &labels;
    verdict.label_wanted = gates.chromatic.labels;
    verdict.legend = legend;
    DrawVerdictAnchor(dl, model.diagnose, verdict);
  }
  DrawMapReadoutLabels(dl, labels, pic_min.x, pic_min.y, view.width_pt, view.height_pt);
  // The reference profile's hover device (§16.1.3's extension): the hovered profile row's
  // reading, drawn on the radial line the probe sampled — the SURVEY document's own sun echo and
  // its own azimuth frame (the probe's premise; not the map's anchor fallback, which may carry
  // another generation's sun). Consumes LAST frame's profile row standing. Under the Overlays
  // gate by construction: this whole pass runs behind it, so the default frame adds no profile
  // ink. The profile device never joins the persistent set. No legend entry: the device
  // is hover-transient and the driving row is its explanation (the contract's sync records it).
  if (state.analysis.profile_row_hover != SurveyProfileHover::kNone && model.sun_available) {
    ProfileHoverInput profile;
    profile.camera = &view.camera;
    profile.width_px = view.width;
    profile.height_px = view.height;
    profile.width_pt = view.width_pt;
    profile.height_pt = view.height_pt;
    profile.pic_min_x = pic_min.x;
    profile.pic_min_y = pic_min.y;
    profile.pic_px = pic_px;
    profile.sun.altitude_deg = model.sun_altitude_deg;
    profile.sun.azimuth_deg = model.sun_azimuth_deg;
    profile.hover = state.analysis.profile_row_hover;
    DrawProfileHoverDevice(dl, model.probe, profile);
  }
}

void DrawSkyPointMarker(ImDrawList* dl, const MapCanvasProjection& view, const SkyPointIntent& intent,
                        const ImVec2& pic_min, float pic_px, float* out_marker_px, float* out_marker_py) {
  // The marker rides the intent's VALIDITY, not its source: every path that writes a valid
  // intent — a map pick, marker drag, the typed-coordinate Set sky point button — draws the
  // same ink (AC1's style unity holds by construction: one draw site, one shape). A typed
  // coordinate is a first-class citizen of the map: it names a sky direction that projects
  // exactly like a picked one.
  if (!intent.valid) {
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
  DrawSkyPointIndicator(dl, cx, cy);
  *out_marker_px = cx;
  *out_marker_py = cy;
}

// The adopted point query's rank-0 image on the map (AC3): a dedicated marker in the rank-0
// mass colour, drawn from the ADOPTED POINT RESULT only — the position is never borrowed from a
// survey or map handle, and the user's sky-point marker keeps its own ink and its own place
// (the image never moves the target). The image rides the EXISTING Point images group gate
// (gates.points.graphics) beside the Overlays master: no parallel switch — closing the group
// removes the marker and its legend naming, the answer's text still carries the position (a
// hidden layer is never a no-image claim). The marker is the image's ONE carrier: the review
// chain retired the "reuse the matched map's mass cell" branch — the raster's point-sampled
// shader can drop a sliver cell entirely (the delta-0 pole ring is exactly such a cell) while
// the map submits fine, so a document-side cell record never proves on-screen ink, and the
// legend would name a picture that carries none. Where the map's own amber cell DOES show, the
// diamond sits on it — same rank-0 semantics, same colour, one expression. The image's position
// must project into this frame's view: the legend records what the frame actually drew, never a
// document-side potential object.
void DrawPointImageMarker(GuiState& state, const MapCanvasProjection& view, const ImVec2& pic_min, float pic_px,
                          bool points_graphics_on, OverlayLegendState* legend) {
  // The Point images group's graphics gate: one group, one visibility rule (the layer plan's
  // AC1; the master Overlays gate is the caller's).
  if (!points_graphics_on) {
    return;
  }
  const PointQuerySession& point = state.analysis.point;
  if (PointQueryStateFor(point) != PointQueryState::kCompleted || point.adopted_result == nullptr) {
    return;
  }
  LUMICE_PathPointInfo info{};
  info.struct_size = sizeof(info);
  if (LUMICE_PathPointGetInfo(point.adopted_result.get(), &info) != LUMICE_PATH_READ_OK ||
      info.outcome != LUMICE_PATH_POINT_MASS) {
    return;
  }
  LUMICE_PathPointMass mass{};
  mass.struct_size = sizeof(mass);
  if (LUMICE_PathPointGetMass(point.adopted_result.get(), &mass) != LUMICE_PATH_READ_OK) {
    return;
  }
  double travel[3];
  AltAzToDir(mass.altitude_deg, mass.azimuth_deg, travel);
  const double pos[3] = { -travel[0], -travel[1], -travel[2] };
  double x = 0.0;
  double y = 0.0;
  // The visibility rule: off-view means off-frame, the legend stays silent whatever the
  // document carries.
  if (!ProjectToRect(view, pos, pic_min, pic_px, &x, &y)) {
    return;
  }
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImU32 ink = static_cast<ImU32>(kPointMassColour);
  const float r = UiPx(5.0f);
  const ImVec2 c(static_cast<float>(x), static_cast<float>(y));
  // One filled diamond, ONE convex quad in the screen-clockwise winding (top → right → bottom
  // → left) — AddConvexPolyFilled's AA edge direction assumes it; two mirrored triangles would
  // hand the left half a counter-clockwise order and render an asymmetric marker (the recorded
  // ImGui AA fill trap).
  dl->AddQuadFilled(ImVec2(c.x, c.y - r), ImVec2(c.x + r, c.y), ImVec2(c.x, c.y + r), ImVec2(c.x - r, c.y), ink);
  dl->AddCircle(c, r + UiPx(1.5f), IM_COL32(16, 17, 20, 255), 0, UiPx(1.2f));
  if (legend != nullptr) {
    legend->point_image = true;
  }
}

}  // namespace

// The legend keys' caliber words, ONE spelling per draw pass: the scope wavelength for the
// reach/edge bounds, unknown spelled when the document carries no scope — never the map's λ.
void RecordArcLambdaTexts(OverlayLegendState& legend, const ReferenceArcInput& in) {
  if (in.reference_lambda_available) {
    char lambda[32];
    std::snprintf(lambda, sizeof(lambda), "%.1f nm", static_cast<double>(in.reference_lambda_nm));
    legend.arc_lambda_text = lambda;
  } else {
    legend.arc_lambda_text = "wavelength unknown";
  }
}

LightSideTick LightSideTickFor(const ViewProjection* camera, int width_px, int height_px, float width_pt,
                               float height_pt, float pic_min_x, float pic_min_y, float pic_px, const SkySunAnchor& sun,
                               double delta_deg) {
  LightSideTick out;
  MapCanvasProjection view;
  view.camera = *camera;
  view.width = width_px;
  view.height = height_px;
  view.width_pt = width_pt;
  view.height_pt = height_pt;
  const ImVec2 pic_min(pic_min_x, pic_min_y);
  double sun_pos[3];
  SkyPosition(sun.altitude_deg, sun.azimuth_deg, sun_pos);
  double u[3];
  double v[3];
  SkyRadialBasis(sun_pos, u, v);
  // The anchor: the circle's own topmost in-disk sample (the walk's own parameterization and
  // continuity rules — the label anchor's precedent), found here so the tick is self-contained.
  auto project = [&](double delta, double phi, double* x, double* y, double travel[3]) {
    double p[3];
    SkyPointAboutSun(sun_pos, u, v, delta, phi, p);
    for (int k = 0; k < 3; ++k) {
      travel[k] = -p[k];
    }
    return ProjectToRect(view, p, pic_min, pic_px, x, y);
  };
  bool have_top = false;
  double top_x = 0.0;
  double top_y = 0.0;
  double top_phi = 0.0;
  double prev_travel_z = 0.0;
  double prev_travel[3] = { 0.0, 0.0, 0.0 };
  for (int sample = 0; sample <= kOverlaySegments; ++sample) {
    const double phi = 2.0 * kPiD * sample / kOverlaySegments;
    double x = 0.0;
    double y = 0.0;
    double travel[3] = { 0.0, 0.0, 0.0 };
    if (!project(delta_deg, phi, &x, &y, travel)) {
      continue;
    }
    const bool in_canvas =
        x >= pic_min.x && x <= pic_min.x + view.width_pt && y >= pic_min.y && y <= pic_min.y + view.height_pt;
    const bool same_disk =
        sample == 0 || ProjectionContinuous(view.camera.lens_type, prev_travel_z, travel[2], prev_travel, travel);
    prev_travel_z = travel[2];
    for (int k = 0; k < 3; ++k) {
      prev_travel[k] = travel[k];
    }
    if (in_canvas && same_disk && (!have_top || y < top_y)) {
      have_top = true;
      top_x = x;
      top_y = y;
      top_phi = phi;
    }
  }
  if (!have_top) {
    return out;  // the circle left the view: no anchor, no tick (the label's own honesty)
  }
  // The tip: the same circle azimuth, one step OUTWARD — a larger deviation, the light side ON
  // THE SPHERE — stepped up until the projected segment reads. A broken run (a dual-fisheye
  // disk split or a seam jump between anchor and tip) or an off-view tip skips that step; the
  // words carry the side when no projection carries the geometry.
  for (const double eps : { 0.5, 1.0, 2.0, 4.0 }) {
    double ax = 0.0;
    double ay = 0.0;
    double anchor_travel[3] = { 0.0, 0.0, 0.0 };
    double tx = 0.0;
    double ty = 0.0;
    double tip_travel[3] = { 0.0, 0.0, 0.0 };
    if (!project(delta_deg, top_phi, &ax, &ay, anchor_travel) ||
        !project(delta_deg + eps, top_phi, &tx, &ty, tip_travel)) {
      continue;
    }
    if (!ProjectionContinuous(view.camera.lens_type, anchor_travel[2], tip_travel[2], anchor_travel, tip_travel)) {
      continue;
    }
    const float dx = static_cast<float>(tx - ax);
    const float dy = static_cast<float>(ty - ay);
    if (dx * dx + dy * dy < UiPx(8.0f) * UiPx(8.0f)) {
      continue;  // too short to read; try the next step up
    }
    out.drawn = true;
    out.anchor_x = ax;
    out.anchor_y = ay;
    out.tip_x = tx;
    out.tip_y = ty;
    out.eps_deg = eps;
    return out;
  }
  return out;
}

void DrawReferenceArcs(ImDrawList* dl, const SurveyOverlayFacts& facts, const ReferenceArcInput& in) {
  if (!in.reach.graphics && !in.edge.graphics && !in.tir.graphics) {
    return;
  }
  MapCanvasProjection view;
  // The mirror of ReferenceArcInput's projection quintet (see its comment): a new
  // MapCanvasProjection field must be copied here AND carried by the input struct, or this
  // rebuilt view silently defaults while the dispersion circles move.
  view.camera = *in.camera;
  view.width = in.width_px;
  view.height = in.height_px;
  view.width_pt = in.width_pt;
  view.height_pt = in.height_pt;
  const ImVec2 pic_min(in.pic_min_x, in.pic_min_y);
  double sun_pos[3];
  SkyPosition(in.sun.altitude_deg, in.sun.azimuth_deg, sun_pos);
  // One submitted stroke's readout: the owner word names the sharper owner when two share the
  // stroke (the legend carries the second name); a group's labels bit shows its readouts and
  // never its ink.
  auto push_label = [&](bool label_wanted, double top_x, double top_y, double delta_deg, const char* owner_word,
                        bool partial) {
    if (!label_wanted || in.labels_sink == nullptr) {
      return;
    }
    char line[128];
    std::snprintf(line, sizeof(line), "%s · %.3f°%s", owner_word, delta_deg, partial ? " · arc in view" : "");
    in.labels_sink->push_back({ static_cast<float>(top_x), static_cast<float>(top_y), kSurveyArcInk, line });
  };
  // One submitted stroke: the geometry (a delta about the sun), its weakened form, the owners
  // the legend must name, and the coincidence identity the selection consumes. The reach/edge
  // shared stroke merges owners onto one submission — one walk, both names — never two walks at
  // the same delta (alpha would stack). `owned_by_edge` is the DISPLAY ownership the legend and
  // labels consume (it follows the gates); `carries_edge_identity` is the object identity, which
  // does NOT.
  struct ArcSubmission {
    double delta_deg;
    bool dashed;
    bool owned_by_reach;
    bool owned_by_edge;
    size_t edge_object_index = 0;
    bool carries_edge_identity = false;
  };
  std::vector<ArcSubmission> submissions;
  // The support bounds' candidate list and the coincidence association, both read from the facts
  // BEFORE any gate: a coincident edge card's stroke is the NEAREST support bound (the merge's
  // own nearest rule among the document's own numbers), and that pairing rides the bound
  // whichever gate shows it — hiding an owner changes visibility, never the object identity.
  // Both sides of the pairing are the document's own delivered numbers; no numeric comparison
  // with a tolerance is re-derived here.
  std::vector<double> support_bounds;
  if (!facts.reach_full_range) {
    for (const SurveyOverlayFacts::ReachInterval& interval : facts.reach_intervals) {
      for (const double bound : { interval.lower_deg, interval.upper_deg }) {
        if (bound <= 0.0 || bound >= 180.0) {
          continue;  // the degenerate deltas are the anchors, not arcs
        }
        support_bounds.push_back(bound);
      }
    }
  }
  std::vector<std::pair<size_t, double>> edge_identities;  // (edge card object index, its bound)
  for (const SurveyOverlayFacts::EdgeArc& edge : facts.edges) {
    if (!edge.coincident_with_support || support_bounds.empty()) {
      continue;
    }
    double nearest = 0.0;
    double best = 0.0;
    bool have_nearest = false;
    for (const double bound : support_bounds) {
      const double d = std::fabs(bound - edge.position_deg);
      if (!have_nearest || d < best) {
        nearest = bound;  // the value, not the loop copy's address
        best = d;
        have_nearest = true;
      }
    }
    edge_identities.emplace_back(edge.object_index, nearest);
  }
  auto identity_for_bound = [&](double bound) -> const std::pair<size_t, double>* {
    for (auto& identity : edge_identities) {
      if (identity.second == bound) {
        return &identity;
      }
    }
    return nullptr;
  };
  // The reach bounds first: the merged intervals' own endpoints, in the one neutral ink.
  // Degenerate deltas submit nothing (0 is the sun's own anchor, already drawn; 180 is the
  // antipodal point) — a bound that IS an anchor carries no second stroke.
  if (in.reach.graphics) {
    if (facts.reach_full_range) {
      // The full [0, 180] range states itself in one text line anchored at the sun — a circle
      // pair would paint the whole sky as ink and say nothing the words do not.
      double lx = 0.0;
      double ly = 0.0;
      if (ProjectToRect(view, sun_pos, pic_min, in.pic_px, &lx, &ly)) {
        if (in.reach.labels && in.labels_sink != nullptr) {
          in.labels_sink->push_back(
              { static_cast<float>(lx), static_cast<float>(ly), kSurveyArcInk, "support · full 0–180° range" });
        }
        if (in.legend != nullptr) {
          in.legend->reach_bounds = true;
          in.legend->reach_bound_count = 1;
          in.legend->reach_full_range_text = true;
          RecordArcLambdaTexts(*in.legend, in);
        }
      }
    } else {
      for (const SurveyOverlayFacts::ReachInterval& interval : facts.reach_intervals) {
        for (const double bound : { interval.lower_deg, interval.upper_deg }) {
          if (bound <= 0.0 || bound >= 180.0) {
            continue;  // the degenerate deltas are the anchors, not arcs
          }
          submissions.push_back({ bound, false, true, false });
          // The bound's identity tag (the association read before the gates): the shared
          // stroke's object identity, even while the edge gate shows nothing.
          if (const auto* identity = identity_for_bound(bound)) {
            submissions.back().carries_edge_identity = true;
            submissions.back().edge_object_index = identity->first;
          }
        }
      }
    }
  }
  // The sharp edges: a card declaring coincidence with support takes the NEARER interval
  // endpoint's submission as its own stroke (the layer plan's shared stroke — one walk, both
  // owners' names); only while BOTH gates are on. Each gate alone walks its own geometry; a
  // non-coincident edge never merges, however close on screen.
  if (in.edge.graphics) {
    for (const SurveyOverlayFacts::EdgeArc& edge : facts.edges) {
      bool merged = false;
      if (edge.coincident_with_support && in.reach.graphics && !facts.reach_full_range) {
        ArcSubmission* nearest = nullptr;
        double best = 0.0;
        for (ArcSubmission& s : submissions) {
          if (!s.owned_by_reach) {
            continue;
          }
          const double d = std::fabs(s.delta_deg - edge.position_deg);
          if (nearest == nullptr || d < best) {
            nearest = &s;
            best = d;
          }
        }
        if (nearest != nullptr) {
          nearest->owned_by_edge = true;
          nearest->edge_object_index = edge.object_index;
          merged = true;
        }
      }
      if (!merged) {
        submissions.push_back({ edge.position_deg, !edge.concluded, false, true, edge.object_index });
        submissions.back().carries_edge_identity = true;
      }
    }
  }
  // One walk per submission — the whole point of the merge above. The legend and the labels read
  // the SUBMITTED line count (a walk that grazed the disk draws no stroke and earns no entry),
  // the dispersion pair's own precedents. The evidence level and the reference λ ride the
  // legend's explanations and the layer control, not the arc text. The selected object's stroke
  // draws its feedback in the same pass: thicker ink (the selection's one visual emphasis), the
  // readout worded as the object's with the light-outside fact and its fixed λ, and the outward
  // hint tick at the label anchor — the circle's far side from the sun IS the light side (the
  // support starts at this bound), so the tick names the side the words claim.
  for (const ArcSubmission& s : submissions) {
    // The selected object's stroke, by the coincidence association: whichever gate is showing
    // the associated bound, the stroke is the object's.
    const bool selected = in.selected_edge_object_index >= 0 && s.carries_edge_identity &&
                          s.edge_object_index == static_cast<size_t>(in.selected_edge_object_index);
    std::vector<float> segments;
    const IsoDeltaWalk walk =
        WalkIsoDeltaCircle(dl, view, pic_min, in.pic_px, sun_pos, s.delta_deg, kSurveyArcInk,
                           selected ? UiPx(3.0f) : 1.4f, s.dashed, in.stroke_sink != nullptr ? &segments : nullptr);
    if (walk.lines_submitted == 0) {
      continue;
    }
    if (in.stroke_sink != nullptr) {
      in.stroke_sink->push_back(
          { s.carries_edge_identity, s.owned_by_edge, s.owned_by_reach, s.edge_object_index, std::move(segments) });
    }
    // The label OWNER obeys the labels switches, not the geometry ownership alone: the edge
    // word only when the edge owns the stroke AND its labels are on; the support word when the
    // reach's labels are on; both off, the stroke draws unlabeled — a shared stroke's text can
    // never speak through an owner whose labels are switched off. The selected object's label
    // worded as the object's (the same labels bit owns it).
    const bool edge_label = s.owned_by_edge && in.edge.labels;
    const bool reach_label = s.owned_by_reach && in.reach.labels;
    if ((edge_label || reach_label) && selected && in.labels_sink != nullptr) {
      char line[128];
      if (in.reference_lambda_available) {
        std::snprintf(line, sizeof(line), "inner edge — light outside · %.0f nm ref",
                      static_cast<double>(in.reference_lambda_nm));
      } else {
        std::snprintf(line, sizeof(line), "inner edge — light outside");
      }
      in.labels_sink->push_back(
          { static_cast<float>(walk.top_x), static_cast<float>(walk.top_y), kSurveyArcInk, line });
      // The outward hint tick, built in SKY space (LightSideTickFor): the tip is the same
      // circle azimuth at a larger deviation — the light side on the sphere — projected through
      // the shared chain. A screen-space sun-to-anchor direction would be wrong exactly where
      // the wording matters: under the dual-fisheye family the projected circle is not centred
      // on the projected sun (anchor and sun can sit in different disks), and the tick would
      // point into the dark side.
      const LightSideTick tick = LightSideTickFor(in.camera, in.width_px, in.height_px, in.width_pt, in.height_pt,
                                                  in.pic_min_x, in.pic_min_y, in.pic_px, in.sun, s.delta_deg);
      if (tick.drawn) {
        dl->AddLine(ImVec2(static_cast<float>(tick.anchor_x), static_cast<float>(tick.anchor_y)),
                    ImVec2(static_cast<float>(tick.tip_x), static_cast<float>(tick.tip_y)), kSurveyArcInk, UiPx(2.0f));
        dl->AddCircleFilled(ImVec2(static_cast<float>(tick.tip_x), static_cast<float>(tick.tip_y)), UiPx(2.0f),
                            kSurveyArcInk);
      }
    } else {
      push_label(edge_label || reach_label, walk.top_x, walk.top_y, s.delta_deg, edge_label ? "edge" : "support",
                 walk.partial());
    }
    if (in.legend != nullptr) {
      if (s.owned_by_reach) {
        in.legend->reach_bounds = true;
        in.legend->reach_bound_count += 1;
      }
      if (s.owned_by_edge) {
        in.legend->edge_arcs = true;
        if (s.owned_by_reach) {
          in.legend->edge_shared = true;
        }
      }
      RecordArcLambdaTexts(*in.legend, in);
    }
  }
  // The internal-reflection turns: each drawable feature draws its pair of representative deltas
  // in the same neutral ink and carries ONE paired readout (the pair is the feature's own fact —
  // two positions, one event), anchored at whichever of the two submitted circles sits higher.
  // The RED-BLUE group owns the chromatic claims; no stroke is shared across the two groups —
  // simultaneous gates each draw their own ink, the legend accounts separately.
  if (in.tir.graphics) {
    for (const SurveyOverlayFacts::TirTurn& turn : facts.tir_turns) {
      const IsoDeltaWalk red =
          WalkIsoDeltaCircle(dl, view, pic_min, in.pic_px, sun_pos, turn.delta_red_deg, kSurveyArcInk, 1.4f);
      const IsoDeltaWalk blue =
          WalkIsoDeltaCircle(dl, view, pic_min, in.pic_px, sun_pos, turn.delta_blue_deg, kSurveyArcInk, 1.4f);
      if (red.lines_submitted == 0 && blue.lines_submitted == 0) {
        continue;  // the pair left the view disk: nothing drew, no entry, no readout
      }
      const bool red_carries = blue.lines_submitted == 0 || (red.lines_submitted > 0 && red.top_y <= blue.top_y);
      const IsoDeltaWalk& carrier = red_carries ? red : blue;
      if (carrier.lines_submitted > 0) {
        if (in.tir.labels && in.labels_sink != nullptr) {
          const std::string step_word = turn.step >= 0 ? "TIR step " + std::to_string(turn.step) : std::string("TIR");
          char line[128];
          std::snprintf(line, sizeof(line), "%s · %.3f°/%.3f°%s", step_word.c_str(), turn.delta_red_deg,
                        turn.delta_blue_deg, carrier.partial() ? " · arc in view" : "");
          in.labels_sink->push_back(
              { static_cast<float>(carrier.top_x), static_cast<float>(carrier.top_y), kSurveyArcInk, line });
        }
        if (in.legend != nullptr) {
          in.legend->tir_turns = true;
          in.legend->tir_turn_count += 1;
          in.legend->tir_lambda_text = in.tir_lambda_text;
        }
      }
    }
  }
}

void DrawSkyPointIndicator(ImDrawList* dl, float cx, float cy) {
  const float r = UiPx(6.0f);
  // The same marker has a dark under-stroke and a light core on every input path. Keep the
  // projected centre and the separate hit-radius owner unchanged.
  const ImU32 rim = IM_COL32(16, 17, 20, 255);
  dl->AddCircle(ImVec2(cx, cy), r, rim, 0, 4.0f);
  dl->AddLine(ImVec2(cx - 2.0f * r, cy), ImVec2(cx - r - 1.0f, cy), rim, 3.6f);
  dl->AddLine(ImVec2(cx + r + 1.0f, cy), ImVec2(cx + 2.0f * r, cy), rim, 3.6f);
  dl->AddLine(ImVec2(cx, cy - 2.0f * r), ImVec2(cx, cy - r - 1.0f), rim, 3.6f);
  dl->AddLine(ImVec2(cx, cy + r + 1.0f), ImVec2(cx, cy + 2.0f * r), rim, 3.6f);
  dl->AddCircle(ImVec2(cx, cy), r, IM_COL32(255, 255, 255, 235), 0, 1.6f);
  dl->AddLine(ImVec2(cx - 2.0f * r, cy), ImVec2(cx - r - 1.0f, cy), IM_COL32(255, 255, 255, 200), 1.2f);
  dl->AddLine(ImVec2(cx + r + 1.0f, cy), ImVec2(cx + 2.0f * r, cy), IM_COL32(255, 255, 255, 200), 1.2f);
  dl->AddLine(ImVec2(cx, cy - 2.0f * r), ImVec2(cx, cy - r - 1.0f), IM_COL32(255, 255, 255, 200), 1.2f);
  dl->AddLine(ImVec2(cx, cy + r + 1.0f), ImVec2(cx, cy + 2.0f * r), IM_COL32(255, 255, 255, 200), 1.2f);
}

void DrawVerdictAnchor(ImDrawList* dl, const SurveyDiagnoseView& diagnose, const VerdictAnchorInput& in) {
  // One analysis-ink gate. Row hover cannot bypass Overlays or suppress a qualified anchor.
  if (!in.overlays) {
    return;
  }
  // The honesty gate, in contract §16.1.3's order, each leg short-circuiting. An unavailable
  // diagnosis (a refused or not-run document) renders no diagnose rows at the text layer either
  // — the chromatic layer's own limit line is that state's honesty, the anchor adds nothing.
  if (!diagnose.available) {
    return;
  }
  // The closed trigger kind set (contract §16.1.3): the edge family are the verdict kinds that
  // carry a position — the set is closed by the payload's own structure (dp_chromatic's VerdictOf
  // is the only place has_position is set, exactly for these two kinds), not by an open
  // "anything with a position" predicate (the mapping must not fork from what the documents can
  // actually carry).
  const bool kind_anchored = diagnose.verdict_kind == "edge" || diagnose.verdict_kind == "gate_edge";
  if (!kind_anchored || !diagnose.position_available || diagnose.visible == 0) {
    // No spatial payload, or the verdict names itself not visible — anchoring it would read the
    // map against the row's own words. Not drawing is the honest state; the text layer's limit
    // lines and the legend's drawn-only rule carry the absence.
    return;
  }
  // The coverage leg: an incomplete-coverage verdict still names its position, so the anchor
  // draws — weakened, dashed (the limited computation stated in the visual grammar, the text
  // layer's own qualifier line carrying the same limit in words). Judge the absence legs FIRST:
  // a stop-requested verdict is kindless and positionless anyway, so a coverage-incomplete,
  // self-invisible edge verdict resolves to absence here, not to a dashed ghost.
  const bool dashed = !diagnose.coverage_complete;
  ImU32 colour = 0;
  if (diagnose.color == "blue") {
    colour = kChromaticColourBlue;
  } else if (diagnose.color == "red") {
    colour = kChromaticColourRed;
  } else {
    return;  // no palette entry, no anchor — the edge family's colours are blue/red by construction
  }
  MapCanvasProjection view;
  // The mirror of VerdictAnchorInput's projection quintet (see its comment): a new
  // MapCanvasProjection field must be copied here AND carried by the input struct, or this
  // rebuilt view silently defaults while the dispersion circles move.
  view.camera = *in.camera;
  view.width = in.width_px;
  view.height = in.height_px;
  view.width_pt = in.width_pt;
  view.height_pt = in.height_pt;
  const ImVec2 pic_min(in.pic_min_x, in.pic_min_y);
  double sun_pos[3];
  SkyPosition(in.sun.altitude_deg, in.sun.azimuth_deg, sun_pos);
  // One walk draws the circle and reports its topmost in-disk sample — the label anchor, the
  // dispersion readout's own precedent. The legend bit reads the SUBMITTED line count: a legend
  // describes the drawn picture, and a dashed circle that walked in-disk may still have skipped
  // every one of its pairs.
  const IsoDeltaWalk walk =
      WalkIsoDeltaCircle(dl, view, pic_min, in.pic_px, sun_pos, diagnose.position_deg, colour, 1.4f, dashed);
  if (walk.lines_submitted > 0) {
    if (in.legend != nullptr) {
      in.legend->verdict_anchor = true;
      in.legend->verdict_anchor_colour = colour;
      in.legend->verdict_anchor_dashed = dashed;
    }
    // The readout label rides the labels toggle (label_wanted): off, the anchor's ink draws and
    // its text waits — labels annotate the ink, they do not gate it, and the labels-off frame
    // must not leak the label through this draw's solo path.
    if (in.label_wanted) {
      char line[96];
      std::snprintf(line, sizeof(line), "verdict · %.3f°%s", diagnose.position_deg,
                    walk.partial() ? " · arc in view" : "");
      const MapReadoutLabel label{ static_cast<float>(walk.top_x), static_cast<float>(walk.top_y), colour, line };
      if (in.labels != nullptr) {
        in.labels->push_back(label);
      } else {
        DrawMapReadoutLabels(dl, { label }, in.pic_min_x, in.pic_min_y, in.width_pt, in.height_pt);
      }
    }
  }
}

// The reference profile's hover device (§16.1.3's extension). The spatial premise, derived once
// and resting on ONE basis construction (SkyRadialBasis, shared with the dispersion walk): the
// engine's probe frame is MakeProbeFrame(incident) with incident = SunIncidentDirection =
// AltAzToDir(sun) = −SkyPosition(sun) — so the probe's basis about the incident coincides with
// this file's walk basis about the sun (e1 = u, e2 = −v, identically for every sun position, the
// same fallback included), and ProbeDirection's outgoing propagation at (δ, az) negates to the
// sky point the map walk parameterizes at φ = π − az. The built-in survey probe's azimuth is the
// representative wedge's own value (azimuth_available carries it; the probe's value formula makes
// the uniform-density profile azimuth-free, so the drawn line is the sample line, not a claim
// about every azimuth). The point device marks the named reading's position on that line; the
// separation device draws the radial INTERVAL between the two readings, sampled along the line —
// never an annulus between the two iso-deviation circles. All-or-nothing visibility: a reading
// whose position leaves the view draws nothing (no bridging across a projection break, no
// partial interval without a qualifier it cannot carry — the row's own words are the device's
// only text).
void DrawProfileHoverDevice(ImDrawList* dl, const SurveyProbeView& probe, const ProfileHoverInput& in) {
  if (in.hover == SurveyProfileHover::kNone) {
    return;
  }
  if (!probe.available || probe.state == LUMICE_PATH_STATE_REFUSED || !probe.azimuth_available) {
    return;  // the honesty gates: no readings claimed, or the line azimuth unknown — no geometry
  }
  bool edge_needed = false;
  bool peak_needed = false;
  switch (in.hover) {
    case SurveyProfileHover::kEdge:
      edge_needed = true;
      break;
    case SurveyProfileHover::kPeak:
      peak_needed = true;
      break;
    case SurveyProfileHover::kSeparation:
      edge_needed = true;
      peak_needed = true;
      break;
    case SurveyProfileHover::kNone:
      return;
  }
  if ((edge_needed && !probe.edge_available) || (peak_needed && !probe.peak_available)) {
    return;  // the named reading is absent — not drawing is the honest state
  }
  MapCanvasProjection view;
  // The mirror of ProfileHoverInput's projection quintet (see its comment): a new
  // MapCanvasProjection field must be copied here AND carried by the input struct, or this
  // rebuilt view silently defaults while the dispersion circles move.
  view.camera = *in.camera;
  view.width = in.width_px;
  view.height = in.height_px;
  view.width_pt = in.width_pt;
  view.height_pt = in.height_pt;
  const ImVec2 pic_min(in.pic_min_x, in.pic_min_y);
  double sun_pos[3];
  SkyPosition(in.sun.altitude_deg, in.sun.azimuth_deg, sun_pos);
  double u[3];
  double v[3];
  SkyRadialBasis(sun_pos, u, v);
  // The probe azimuth's one mapping into the walk's circle parameter (the derivation above).
  const double phi = kPiD - probe.azimuth_deg * kDeg2RadD;
  auto device_point = [&](double delta_deg) {
    double p[3];
    SkyPointAboutSun(sun_pos, u, v, delta_deg, phi, p);
    double x = 0.0;
    double y = 0.0;
    ProfileScreenPoint out;
    out.projected = ProjectToRect(view, p, pic_min, in.pic_px, &x, &y);
    out.x = x;
    out.y = y;
    return out;
  };
  if (in.hover == SurveyProfileHover::kSeparation) {
    // The radial interval between the two readings, walked along the profile line at a step the
    // map's own scale reads smoothly; any sample off the view voids the whole device (the
    // all-or-nothing rule above). The rule is STRUCTURAL, not incidental: the whole bounded
    // sample sequence projects FIRST, and only a fully-projecting interval submits anything — a
    // loop that drew as it projected would leave an unqualified prefix on the draw list when a
    // later sample fell off the view (submitted commands cannot be unsubmitted). The endpoints
    // carry the point device's own marker.
    const double edge = probe.edge_deg;
    const double peak = probe.peak_deg;
    const double lo = std::min(edge, peak);
    const double hi = std::max(edge, peak);
    const int steps = std::min(64, std::max(2, static_cast<int>(std::ceil((hi - lo) * 2.0))));  // ~0.5° resolution
    std::vector<ProfileScreenPoint> samples;
    samples.reserve(static_cast<size_t>(steps) + 1);
    double prev_travel_z = 0.0;
    double prev_travel[3] = { 0.0, 0.0, 0.0 };
    for (int s = 0; s <= steps; ++s) {
      const double delta = lo + (hi - lo) * static_cast<double>(s) / static_cast<double>(steps);
      double p[3];
      SkyPointAboutSun(sun_pos, u, v, delta, phi, p);
      const double travel[3] = { -p[0], -p[1], -p[2] };
      if (s > 0 && !ProjectionContinuous(view.camera.lens_type, prev_travel_z, travel[2], prev_travel, travel)) {
        return;  // the interval spans one of the projection's own breaks (the dual-fisheye disk
                 // split, the rectangular seam or pole anchor): the bridged chord does not exist
                 // on the picture, and the all-or-nothing rule keeps the list untouched
      }
      prev_travel_z = travel[2];
      for (int k = 0; k < 3; ++k) {
        prev_travel[k] = travel[k];
      }
      double x = 0.0;
      double y = 0.0;
      ProfileScreenPoint out;
      out.projected = ProjectToRect(view, p, pic_min, in.pic_px, &x, &y);
      out.x = x;
      out.y = y;
      if (!out.projected) {
        return;  // one sample off the view: the whole device stays undrawn, the draw list untouched
      }
      samples.push_back(out);
    }
    for (size_t s = 1; s < samples.size(); ++s) {
      dl->AddLine(ImVec2(static_cast<float>(samples[s - 1].x), static_cast<float>(samples[s - 1].y)),
                  ImVec2(static_cast<float>(samples[s].x), static_cast<float>(samples[s].y)), kProfileReadingColour,
                  UiPx(2.0f));
    }
    DrawProfilePointMarker(dl, samples.front());
    DrawProfilePointMarker(dl, samples.back());
    return;
  }
  DrawProfilePointMarker(dl, device_point(in.hover == SurveyProfileHover::kEdge ? probe.edge_deg : probe.peak_deg));
}

void RenderAnalyzeMapRegion(GuiState& state, const ImVec2& size) {
  g_map_observation_timing.began_seconds = ObservationClockSeconds();
  // D3 — the reference-map | info split (contract §4.2), drawn by this function: the window's
  // content area splits into the LEFT region (the raster, its gestures and its state line,
  // under the region's own name ##AnalyzeRaypathView) and the RIGHT region (the wavelength,
  // the caliber block, the sky-point input and the legend — its own child,
  // ##AnalyzeRaypathInfo, native wheel scroll), with the draggable strip between. The stored
  // ratio is the split's identity (manual priority, §9.3 — no query result or selection ever
  // writes it); the frame derives pixels and clamps them at the two floors: ① the info region
  // stops at its floor and the map absorbs the rest, ② the map stops at its floor and both
  // pin, ③ — only when both floors cannot fit — the floors share the span proportionally and
  // the regions' internal scrolls take over (the map's manual route, the info's native wheel).
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
  if (Checkbox("Overlays##map", &overlays)) {
    storage->SetBool(overlay_key, overlays);
  }
  // The Layers entry joins the checkbox's line: the master gate and the group face are one
  // header row, and the canvas keeps the line the old header had (the checkbox ~90 pt + the
  // button ~50 pt fit the narrowest map column the floors can produce).
  ImGui::SameLine();
  // The six layer groups' session switches, beside the master gate (the layer plan's control
  // face): each row's two boxes are the Render habit's graphics/labels split — one group, one
  // graphics box and one labels box, one storage key pair per group. Storage keys ride the
  // master gate's own T-session lifetime: class switches, adopted results and window close/open
  // keep them; a new process resumes the factory defaults (all on — a group with no payload
  // draws nothing anyway, so the defaults never fabricate ink). The master gate stays the ONE
  // off switch: with it off no preference is disabled or forgotten, the groups stay editable
  // and simply draw nothing.
  struct LayerKeyPair {
    const char* base;          // "ov_<group>": the graphics key; + "_l": the labels key
    const char* display_name;  // the table row's visible name — ONE copy, consumed by the row
                               // and by the popup's width demand below
    SurveyLayerGate* gate;
    const char* graphics_label;  // "##<base>_graphics": the table box's hidden item label
    const char* labels_label;    // "##<base>_labels": ditto, for the labels box
    ImGuiID graphics_key;        // resolved ONCE, in THIS child's ID scope — the popup's writes
    ImGuiID labels_key;          // below reuse these very IDs (ImGui::GetID inside a popup hashes
  } key_pairs[7];                // in the popup's own scope, which would fork the keys: the read
                                 // here and the write there would never meet)
  SurveyLayerGates gates;
  {
    const struct {
      const char* base;
      const char* display_name;
      SurveyLayerGate* gate;
    const char* graphics_label;
    const char* labels_label;
    } named[7] = {
      { "ov_reach", "Support range", &gates.reach, "##ov_reach_graphics", "##ov_reach_labels" },
      { "ov_edge", "Sharp edges", &gates.edge, "##ov_edge_graphics", "##ov_edge_labels" },
      { "ov_curves", "Fold arcs", &gates.curves, "##ov_curves_graphics", "##ov_curves_labels" },
      { "ov_points", "Point images", &gates.points, "##ov_points_graphics", "##ov_points_labels" },
      { "ov_caustic", "Caustic representatives", &gates.caustic, "##ov_caustic_graphics", "##ov_caustic_labels" },
      { "ov_chromatic", "Red-blue shift", &gates.chromatic, "##ov_chromatic_graphics", "##ov_chromatic_labels" },
      { "ov_tir", "Reflection turns", &gates.tir, "##ov_tir_graphics", "##ov_tir_labels" }
    };
    for (int i = 0; i < 7; ++i) {
      key_pairs[i].base = named[i].base;
      key_pairs[i].display_name = named[i].display_name;
      key_pairs[i].gate = named[i].gate;
      key_pairs[i].graphics_label = named[i].graphics_label;
      key_pairs[i].labels_label = named[i].labels_label;
      const std::string labels_key = std::string(named[i].base) + "_l";
      key_pairs[i].graphics_key = ImGui::GetID(named[i].base);
      key_pairs[i].labels_key = ImGui::GetID(labels_key.c_str());
      key_pairs[i].gate->graphics = storage->GetBool(key_pairs[i].graphics_key, true);
      key_pairs[i].gate->labels = storage->GetBool(key_pairs[i].labels_key, true);
    }
  }
  // The points group's per-kind sub-gates (AC4): regular and collapsed images keep independent
  // visibility inside the group — two names that stay separately switchable whatever overlaps on
  // screen. Same region-scope ID discipline as the group keys: resolve here once, the popup's
  // rows reuse the precomputed IDs.
  struct PointKindKey {
    const char* key;
    bool* value;
    const char* label;
    const char* item_id;  // the sub-row's graphics box's label (a hidden id: the Name column
                          // carries the visible text, the Render-table habit)
    ImGuiID id;
  } point_kind_keys[3] = {
    { "ov_points_k_src", &gates.points_kinds.source_image, "source image", "##ov_points_k_src", 0 },
    { "ov_points_k_reg", &gates.points_kinds.regular_image, "regular image", "##ov_points_k_reg", 0 },
    { "ov_points_k_col", &gates.points_kinds.family_collapse, "family collapse", "##ov_points_k_col", 0 }
  };
  for (PointKindKey& k : point_kind_keys) {
    k.id = ImGui::GetID(k.key);
    *k.value = storage->GetBool(k.id, true);
  }
  if (ImGui::SmallButton("Layers##map_layers")) {
    ImGui::OpenPopup("map_layer_switches");
  }
  if (ImGui::IsItemHovered()) {
    SetWrappedTooltip(
        "The survey's reference layers, per group: graphics and labels. Opening this never "
        "recomputes; switches filter what is already drawn.");
  }
  InfoIcon("map_overlays_help",
           "The survey's reference layers: support-range bounds, sharp edges, positioned object "
           "markers, caustic representatives, the red-blue dispersion pair with its colour-verdict "
           "anchor, and visible internal-reflection turns. The caustic markers are the family "
           "representatives' own sky positions (the map folds a screen-level pile into one marker "
           "plus a count); the sharp-edge curves are deviation-axis references — a different "
           "reading despite the engine's shared kind name. Hover Profile edge, Profile peak or "
           "Peak–edge separation in the class answer to locate the reading on its reference "
           "profile (not an all-sky peak). Overlays must be on for these temporary markers. Sun, "
           "horizon and your sky point stay visible.");
  // The layer popup opens in EVERY data state and with the master gate off (the entry's own
  // premise): with no adopted document the rows name the absence instead of disabling themselves,
  // and the switches stay editable — the preferences are display state, not query state.
  //
  // The popup's size is stated, not whatever auto-fit lands on: the table's outer size has to be
  // an explicit number or an auto-resize window would lock onto whatever its first frame's
  // content-region report was (the table would size to the popup's then-current avail and the
  // two would pin each other). Width runs from the LIVE font's demand — the longest row name plus
  // the two fixed switch columns — so the popup is exactly as wide as its content at any scale;
  // height takes the ten content rows and yields to the host viewport's work rect when the two
  // disagree, at which point the table scrolls (its header row frozen) instead of the popup
  // growing past what is visible. Widths and heights are all clamped against the viewport that
  // hosts THIS window — a detached map's monitor — never against the map column, which the plan
  // forbids as a width source.
  //
  // Only the SetNext* calls sit under the IsPopupOpen gate: they write data the NEXT Begin
  // consumes — while the popup is closed that next Begin would be some unrelated window, and
  // the constraints would land on it. The demand arithmetic above them is pure style queries.
  const char* region_shading_name = "Region shading";
  const ImGuiViewport* layer_vp = ImGui::GetWindowViewport();
  const ImGuiStyle& layer_st = ImGui::GetStyle();
  const float check_w = ImGui::GetFrameHeight();
  const float graphics_w = std::max(check_w, ImGui::CalcTextSize("Graphics").x + layer_st.CellPadding.x * 2.0f);
  const float labels_w = std::max(check_w, ImGui::CalcTextSize("Labels").x + layer_st.CellPadding.x * 2.0f);
  float name_w = ImGui::CalcTextSize(region_shading_name).x;
  for (const LayerKeyPair& k : key_pairs) {
    name_w = std::max(name_w, ImGui::CalcTextSize(k.display_name).x);
  }
  for (const PointKindKey& k : point_kind_keys) {
    name_w = std::max(name_w, ImGui::CalcTextSize(k.label).x);
  }
  const float name_col = name_w + layer_st.IndentSpacing + layer_st.CellPadding.x * 2.0f;
  const float demand_w = name_col + graphics_w + labels_w + layer_st.WindowPadding.x * 2.0f;
  // Two INDEPENDENT quantities, per the plan's "width from the full column demand and the
  // workspace's upper bound": the cap derives from the viewport alone and must never be lifted
  // by the content's own demand — a cap raised to fit the content left the labels column
  // clipped past the visible edge on a viewport narrower than the popup (the table has no
  // horizontal scroll to recover it). Past the cap the popup narrows, the name column — the
  // only stretching one — absorbs the shrink by wrapping, and the fixed switch columns keep
  // their budget.
  const float layer_max_w = std::max(graphics_w + labels_w, layer_vp->Size.x - layer_st.WindowPadding.x * 2.0f);
  // The height cap reserves one scrollbar: past it the TABLE scrolls, and its scrollbar must
  // not push the popup's own total past the work rect. The viewport's usable rect is Pos..Pos+Size
  // (ImGuiViewport carries no WorkRect — the main window's menu/chrome live outside the viewport).
  const float layer_max_h =
      std::max(check_w * 3.0f, layer_vp->Size.y - layer_st.WindowPadding.y * 2.0f - layer_st.ScrollbarSize);
  if (ImGui::IsPopupOpen("map_layer_switches")) {
    ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(layer_max_w, layer_max_h));
    ImGui::SetNextWindowSize(
        ImVec2(std::min(demand_w, layer_max_w), 0.0f));  // height stays auto-fit under the constraints
  }
  if (ImGui::BeginPopup("map_layer_switches")) {
    const SurveyOverlayCache& cache =
        RefreshSurveyOverlayCache(state.analysis.survey, state.analysis.FrozenPointSunPremise());
    const SurveyOverviewModel& model = cache.model;
    const SurveyOverlayFacts& facts = cache.facts;
    struct LayerRow {
      const char* name;
      LayerKeyPair key;     // the region-scope storage IDs, the live gate, and the boxes' labels
      const char* absence;  // why this group cannot draw on THIS document; nullptr = has payload
    };
    const char* no_document = model.valid ? nullptr : "no document";
    const char* reach_absence =
        model.valid ? (facts.reach_intervals.empty() ? "no support intervals" : nullptr) : no_document;
    const char* edge_absence = model.valid ? (facts.edges.empty() ? "no positioned edge" : nullptr) : no_document;
    bool points_available = false;
    for (const SurveyObjectRow& row : model.objects) {
      if (row.position_available) {
        points_available = true;
        break;
      }
    }
    const char* points_absence = model.valid ? (points_available ? nullptr : "no positioned objects") : no_document;
    bool caustic_available = false;
    for (const SurveyObjectRow& row : model.objects) {
      if (row.position_available && row.kind_name == kCausticEdgeKindName) {
        caustic_available = true;
        break;
      }
    }
    const char* caustic_absence = model.valid ? (caustic_available ? nullptr : "no positioned caustic") : no_document;
    const bool chromatic_available =
        (model.dispersion.available && model.dispersion.min_row_available) ||
        (model.diagnose.available && model.diagnose.position_available && model.diagnose.visible != 0);
    const char* chromatic_absence =
        model.valid ? (chromatic_available ? nullptr : "no chromatic positions") : no_document;
    const char* tir_absence = model.valid ? (facts.tir_turns.empty() ? "no visible turns" : nullptr) : no_document;
    const char* curves_absence =
        model.valid ? (facts.family_curves.empty() ? "no family curves" : nullptr) : no_document;
    const LayerRow rows[7] = {
      { key_pairs[0].display_name, key_pairs[0], reach_absence },
      { key_pairs[1].display_name, key_pairs[1], edge_absence },
      { key_pairs[2].display_name, key_pairs[2], curves_absence },
      { key_pairs[3].display_name, key_pairs[3], points_absence },
      { key_pairs[4].display_name, key_pairs[4], caustic_absence },
      { key_pairs[5].display_name, key_pairs[5], chromatic_absence },
      { key_pairs[6].display_name, key_pairs[6], tir_absence },
    };
    // The layer table: one record per row, the name column the ONLY stretching one (the Render
    // overlay table's rule — the switches state the width they need, the names get the rest),
    // the two switch columns headed in visible text so a box's meaning is readable on the frame
    // and not a hover discovery. ScrollY keeps the header row frozen whenever the content
    // outgrows the cap above; RowBg separates the sub-rows from their parent the way the indent
    // alone cannot at a glance.
    constexpr ImGuiTableFlags kLayerTableFlags =
        ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
    const float row_pitch = std::max(ImGui::GetTextLineHeightWithSpacing(), ImGui::GetFrameHeight());
    const float header_h = ImGui::GetFrameHeight() + layer_st.CellPadding.y * 2.0f;
    // The height the table actually needs, counted from THIS frame's rows: a group row rides
    // TWO text lines when it carries an absence (the note wraps under the name), the three kind
    // sub-rows ride one each, and the region-shading row rides its name plus its readable
    // reason. Estimating a fixed row count instead would shrink the table below its content
    // whenever documents are missing — the rows would scroll under a popup that had room for
    // them all (the frame-text capture caught exactly that on the no-document state).
    int text_lines = 0;
    for (const LayerRow& row : rows) {
      text_lines += 1 + (row.absence != nullptr ? 1 : 0);
    }
    text_lines += 3 + 2;  // the three kind sub-rows; region shading = name + reason
    // The popup's total is this table plus the window padding both ends; the constraint above
    // caps that total, so the table's own cap is the total minus the padding it rides in.
    const float table_cap = std::max(header_h, layer_max_h - layer_st.WindowPadding.y * 2.0f);
    const float table_h = std::min(header_h + static_cast<float>(text_lines) * row_pitch, table_cap);
    if (ImGui::BeginTable("##layer_table", 3, kLayerTableFlags, ImVec2(ImGui::GetContentRegionAvail().x, table_h))) {
      ImGui::TableSetupColumn("##layer_name", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Graphics", ImGuiTableColumnFlags_WidthFixed, graphics_w);
      ImGui::TableSetupColumn("Labels", ImGuiTableColumnFlags_WidthFixed, labels_w);
      // ScrollY alone does NOT freeze the header: the freeze count is set only by
      // TableSetupScrollFreeze (ImGui's FreezeRowsRequest defaults to 0), and the popup-rect
      // capture caught the header scrolled away before this line existed.
      ImGui::TableSetupScrollFreeze(0, 1);
      ImGui::TableHeadersRow();
      // One row per record. The storage keys are the REGION-scope IDs precomputed at the entry
      // read — the popup's own GetID would hash in the popup's scope and fork the keys (the bug
      // this structure exists to prevent); the boxes' item IDs come from their "##" labels alone.
      const auto switches_cell = [&](LayerKeyPair key) {
        ImGui::TableSetColumnIndex(1);
        bool g = key.gate->graphics;
        if (Checkbox(key.graphics_label, &g)) {
          key.gate->graphics = g;
          storage->SetBool(key.graphics_key, g);
        }
        if (ImGui::IsItemHovered()) {
          SetWrappedTooltip("Draw this group's ink.");
        }
        ImGui::TableSetColumnIndex(2);
        bool l = key.gate->labels;
        if (Checkbox(key.labels_label, &l)) {
          key.gate->labels = l;
          storage->SetBool(key.labels_key, l);
        }
        if (ImGui::IsItemHovered()) {
          SetWrappedTooltip("Show this group's readout labels. Hiding labels never hides the ink.");
        }
      };
      const auto name_cell = [&](const char* name, const char* absence, bool indented) {
        ImGui::TableSetColumnIndex(0);
        if (indented) {
          ImGui::SetCursorPosX(ImGui::GetCursorPosX() + layer_st.IndentSpacing);
        }
        ImGui::TextWrapped("%s", name);
        if (absence != nullptr) {
          // The absence rides the SAME record, wrapped under the name — not a SameLine sibling
          // the eye has to chase across the switch columns.
          DisabledWrappedText("%s", absence);
        }
      };
      const auto no_labels_cell = [] {
        // The em dash reads as "rides the parent gate, no labels key of its own" — a non-item
        // placeholder, and the one hover that explains it stays per-cell, not per-row noise.
        ImGui::TableSetColumnIndex(2);
        ImGui::TextDisabled("—");
        if (ImGui::IsItemHovered()) {
          SetWrappedTooltip("Labels follow the parent group's box — kinds have no labels key of their own.");
        }
      };
      for (const LayerRow& row : rows) {
        ImGui::TableNextRow();
        name_cell(row.name, row.absence, /*indented=*/false);
        switches_cell(row.key);
        // The point kinds' sub-rows follow their parent immediately: three graphics-only
        // switches, one indent step, the dash in the labels column (hidden kinds draw no
        // marker, join no hover group and earn no legend entry — one predicate on the draw
        // side sees to all three).
        if (row.key.gate == &gates.points) {
          for (const PointKindKey& k : point_kind_keys) {
            ImGui::TableNextRow();
            name_cell(k.label, /*absence=*/nullptr, /*indented=*/true);
            ImGui::TableSetColumnIndex(1);
            bool v = *k.value;
            if (Checkbox(k.item_id, &v)) {
              *k.value = v;
              storage->SetBool(k.id, v);
            }
            if (ImGui::IsItemHovered()) {
              SetWrappedTooltip("Show this kind's markers within Point images.");
            }
            no_labels_cell();
          }
        }
      }
      // The region-shading row: a separate switch by design (the plan's 正交 rule), and an
      // honest absence today — no regional-flux payload exists, so the switch stands disabled
      // with that reason READABLE on the row (the hover only adds the angular-distance caveat).
      ImGui::TableNextRow();
      name_cell(region_shading_name, "unavailable: no regional-flux payload", /*indented=*/false);
      ImGui::TableSetColumnIndex(1);
      ImGui::BeginDisabled();
      bool region_shading = false;
      Checkbox("##layer_region", &region_shading);
      ImGui::EndDisabled();
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        SetWrappedTooltip("The turn references above are angular-distance references, not an area claim.");
      }
      no_labels_cell();
      ImGui::EndTable();
    }
    ImGui::EndPopup();
  }

  const float canvas_avail_w = ImGui::GetContentRegionAvail().x;
  const float canvas_avail_h = ImGui::GetContentRegionAvail().y;
  const float pic_px = std::max(1.0f, canvas_avail_w);
  const float pic_h = std::max(1.0f, canvas_avail_h);
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
  if (MapQueryStateFor(map) == MapQueryState::kNoSelection) {
    const char* guidance =
        "Select a class in the share list; its reference map takes this region. The sky-point input in the "
        "info panel beside it works without a map.";
    const float wrap_w = std::max(1.0f, pic_px - UiPx(24.0f));
    const ImVec2 text_size = ImGui::CalcTextSize(guidance, nullptr, false, wrap_w);
    const ImVec2 text_min(pic_min.x + (pic_px - text_size.x) * 0.5f,
                          pic_min.y + std::max(0.0f, (pic_h - text_size.y) * 0.5f));
    dl->PushClipRect(pic_min, ImVec2(pic_min.x + pic_px, pic_min.y + pic_h), true);
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), text_min, ImGui::GetColorU32(ImGuiCol_TextDisabled), guidance,
                nullptr, wrap_w);
    dl->PopClipRect();
  }
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
  // The inner-edge selection's map half (817.2): the selected object's stroke feedback draws
  // through the same overlay pass, and the frame's submitted strokes collect as this frame's
  // click surfaces (consumed by the stationary-release test below; frame-transient, never
  // stored). The cache read is idempotent and already warm — no rebuild, no re-read.
  std::vector<ReferenceArcStroke> arc_strokes;
  // The curve inks' click surfaces (task arc-recovery-engine): filled by the overlay pass, read
  // by the stationary-release selection — frame-transient, never stored.
  std::vector<FamilyCurveHitSurface> curve_hit_surfaces;
  int selected_edge_object_index = -1;
  if (InnerEdgeSelectionActive(state)) {
    const SurveyOverlayCache& sel_cache =
        RefreshSurveyOverlayCache(state.analysis.survey, state.analysis.FrozenPointSunPremise());
    if (sel_cache.model.valid && sel_cache.model.inner_edge.qualified) {
      selected_edge_object_index = sel_cache.model.inner_edge.edge_object_index;
    }
  }
  if (overlays) {
    DrawSurveyOverlays(dl, state, view, pic_min, pic_px, &legend, gates, selected_edge_object_index, &arc_strokes,
                       &curve_hit_surfaces);
  }

  float marker_px = -1.0f;
  float marker_py = -1.0f;
  DrawSkyPointMarker(dl, view, intent, pic_min, pic_px, &marker_px, &marker_py);
  if (marker_px >= 0.0f) {
    legend.sky_point = true;
  }
  // The adopted point query's rank-0 image (analysis ink): under the Overlays master gate AND
  // the existing Point images group's graphics gate — one group, one visibility rule.
  if (overlays) {
    DrawPointImageMarker(state, view, pic_min, pic_px, gates.points.graphics, &legend);
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
  // The press provenance snapshot + the drag latch: written while a press is held, so the
  // release frame — which the workspace entry reaches with map_press_owner already reset — can
  // still tell a plain camera press from one the marker/pick/direct-point paths own, and a
  // press that EVER dragged from one that never did. The latch consumes the same predicate the
  // camera orbit consumes (IsMouseDragging during the hold, where the button is down and it
  // works); the Escape-cancelled press lands here as kCancelled on the frame after the key,
  // killing the pending selection.
  if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    state.analysis.map_canvas_press_owner = static_cast<int>(owner);
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
      state.analysis.map_canvas_dragged_beyond_threshold = true;
    }
  }
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    const int press_owner = state.analysis.map_canvas_press_owner;
    const bool dragged = state.analysis.map_canvas_dragged_beyond_threshold;
    state.analysis.map_canvas_press_owner = 0;
    state.analysis.map_canvas_dragged_beyond_threshold = false;
    // The stationary-click structure selection (817.2): a plain camera press that never dragged,
    // tested at the press position against the strokes THIS frame submitted — a part of the
    // circle no line covered cannot be picked, the master gate and the edge graphics gate leave
    // nothing to test when off, and the marker/pick/direct-point presses never reach here (their
    // owners are not kCamera). A miss commits nothing: the existing point-selection features are
    // untouched, and an existing selection is not cleared.
    if (press_owner == static_cast<int>(Owner::kCamera) && !dragged && !explicit_pick) {
      const SurveyOverlayCache& hit_cache =
          RefreshSurveyOverlayCache(state.analysis.survey, state.analysis.FrozenPointSunPremise());
      const ImVec2 press_pos = io.MouseClickedPos[ImGuiMouseButton_Left];
      // The curve inks first (task arc-recovery-engine): one click, one selection — a click
      // that lands on drawn curve ink commits THAT curve's pick (the nearest surface within
      // the pick tolerance; every curve sharing the click joins the overlap cycle) and never
      // reaches the inner-edge strokes below. A miss commits nothing, and an existing
      // selection is not cleared.
      const float curve_tolerance_sq = UiPx(7.0f) * UiPx(7.0f);
      const FamilyCurveHitSurface* best_surface = nullptr;
      float best_dist = curve_tolerance_sq;
      std::vector<std::pair<int, float>> source_dists;
      for (const FamilyCurveHitSurface& surface : curve_hit_surfaces) {
        const bool point_surface = surface.a.x == surface.b.x && surface.a.y == surface.b.y;
        const float d = point_surface ? (press_pos.x - surface.a.x) * (press_pos.x - surface.a.x) +
                                            (press_pos.y - surface.a.y) * (press_pos.y - surface.a.y) :
                                        PointSegmentDistanceSq(press_pos.x, press_pos.y, surface.a.x, surface.a.y,
                                                               surface.b.x, surface.b.y);
        if (d > curve_tolerance_sq) {
          continue;
        }
        bool known = false;
        for (auto& entry : source_dists) {
          if (entry.first == surface.pick.curve_index) {
            entry.second = std::min(entry.second, d);
            known = true;
            break;
          }
        }
        if (!known) {
          source_dists.push_back({ surface.pick.curve_index, d });
        }
        if (d < best_dist) {
          best_dist = d;
          best_surface = &surface;
        }
      }
      if (best_surface != nullptr) {
        std::sort(source_dists.begin(), source_dists.end(),
                  [](const auto& lhs, const auto& rhs) { return lhs.second < rhs.second; });
        std::vector<int> sources;
        sources.reserve(source_dists.size());
        for (const auto& entry : source_dists) {
          sources.push_back(entry.first);
        }
        SelectFamilyCurve(state, best_surface->pick, std::move(sources));
      } else if (hit_cache.model.valid && hit_cache.model.inner_edge.qualified) {
        const float tolerance_sq = UiPx(6.0f) * UiPx(6.0f);
        for (const ReferenceArcStroke& stroke : arc_strokes) {
          // The C01 stroke by its coincidence association — the shared stroke is the object's
          // while either owner's gate shows it (the plan's pickable rule); hiding an owner
          // never re-keys the identity.
          const bool is_c01_stroke =
              stroke.carries_edge_identity &&
              stroke.edge_object_index == static_cast<size_t>(hit_cache.model.inner_edge.edge_object_index);
          if (is_c01_stroke && ArcStrokeHit(stroke, press_pos.x, press_pos.y, tolerance_sq)) {
            SelectInnerEdgeObject(state);
            state.analysis.inner_edge_scroll_hint = true;
            break;
          }
        }
      }
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
        WrappedTooltipText("%s · alt %.4f° az %.4f°", label, alt, az);
        const auto cell = ReadFrozenMapDirection(map, travel);
        if (cell) {
          const auto i = cell->index;
          const auto& info = cell->info;
          ImGui::Text("Reference r%zu c%zu · status %u", i / info.view_cols, i % info.view_cols,
                      static_cast<unsigned>(cell->pixels.status[i]));
          ImGui::Text("%.17g per sr · count %d", cell->pixels.values[i], cell->pixels.counts[i]);
          ImGui::Text("cell dev %.7g° × az %.7g° · %d/%d · wl %.1f · n=%d",
                      (info.view_delta_max_deg - info.view_delta_min_deg) / info.view_rows,
                      (info.view_az_max_deg - info.view_az_min_deg) / info.view_cols, info.members_evaluated,
                      info.member_count, info.wavelength_nm, info.sample_count);
        } else
          WrappedTooltipText("Outside selected source coverage — unknown, not zero");
      };
      if (near_sample) {
        const double d[3] = { (*near_sample)[0], (*near_sample)[1], (*near_sample)[2] };
        show_reading("Near / pick direction", d);
      } else
        WrappedTooltipText("Near side clipped — no point selection here");
      if (far_sample) {
        ImGui::Separator();
        show_reading("Far-side hint — separate direction, not a sum", far_sample->data());
      }
      ImGui::EndTooltip();
    }
  }

  ImGui::EndChild();  // ##AnalyzeRaypathView — the map region

  // ---- D3: the draggable strip between the two regions (contract §4.2) ----
  // The same recipe the workspace splitters run (invisible button + hover/active line +
  // delta-only write from the displayed boundary): the drag moves the stored ratio, the layout
  // re-derives the pixels each frame and clamps them at the floors, so restoring the window
  // size restores the split. Dragging right widens the map (the info region's share shrinks),
  // matching D1's direction convention.
  ImGui::SameLine(0.0f, 0.0f);
  // The drag writes from the displayed boundary (the layout's clamped info width), bounded
  // where the layout's own clamp stops; the overflow pin is what the clamp's lo==hi band
  // writes (analyze_ui_widgets.hpp's white-box note carries the dialect equivalence).
  AnalyzeSplitter("##RaypathViewSplitter", SplitterDragAxis::kLeftRight, size.y, info_w,
                  std::max(1.0f, avail_w - splitter_w), info_floor, avail_w - splitter_w - map_floor,
                  info_floor / std::max(1.0f, avail_w - splitter_w), &state.analyze_raypath_view_info_ratio);
  ImGui::SameLine(0.0f, 0.0f);

  // ---- The info region: the picture's companion facts (contract §6's right side) ----
  // The wavelength, the caliber block, the sky-point input and (Step 4)
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
      SetWrappedTooltip("Cancel only this map calculation; keep the current frozen map.");
    else if (quality_retry)
      SetWrappedTooltip("Retry the current full-sky request, independent of the observation camera.");
    else
      SetWrappedTooltip(
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
  const float lambda_label_w = ImGui::CalcTextSize("wl").x + ImGui::GetStyle().ItemInnerSpacing.x;
  const float lambda_row_w = ImGui::GetContentRegionAvail().x;
  const float joined_track_w = lambda_row_w - lambda_label_w - reference_w - ImGui::GetStyle().ItemSpacing.x;
  // Yield the elastic track before wrapping the fixed reference action. At the compressed
  // viewport this recovers a full row for the report without hiding any control or its label.
  const float track_budget = joined_track_w >= UiPx(60.0f) ? joined_track_w : lambda_row_w - lambda_label_w;
  const float lambda_track = std::max(UiPx(60.0f), std::min(UiPx(140.0f), track_budget));
  ImGui::SetNextItemWidth(lambda_track);
  if (ImGui::SliderFloat("wl##map", &lambda_scratch, 350.0f, 900.0f, "%.0f nm", ImGuiSliderFlags_AlwaysClamp)) {
    state.analysis.map_wavelength_nm = static_cast<double>(lambda_scratch);
  }
  // The λ reading's one owner is this control's own echo; its tooltip names the source rule
  // (the ledger: 真实来源进 λ tooltip — an explicit value asks itself, the engine's reference
  // answers until one is set). Editing asks a key change the pump turns into a real recompute.
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SetWrappedTooltip(
        state.analysis.map_wavelength_nm > 0.0 ?
            "The wavelength this map answers at — an explicit value; editing it asks a new computation." :
            "The wavelength this map answers at — the engine's reference; editing the slider asks an explicit one.");
  }
  if (fits_on_line(reference_w)) {
    ImGui::SameLine();
  }
  ImGui::BeginDisabled(state.analysis.map_wavelength_nm <= 0.0);
  if (ImGui::SmallButton("reference##map_lambda")) {
    state.analysis.map_wavelength_nm = 0.0;
  }
  ImGui::EndDisabled();
  // The view steering verbs are retired (2026-11-13 owner call: the pictures share one synced
  // view and both respond to the mouse, so one-shot Fit/Full-sky presets had no work left);
  // framing now happens on the pictures themselves — wheel to zoom out to the full sky, drag
  // to re-aim (§6.3 of the camera contract keeps the retirement record).

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
      SetWrappedTooltip(intent.from_map_pick ? "Set from a map pick." : "Set from typed coordinates.");
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
    SetWrappedTooltip("%s", PoseButtonTooltip());
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

  // The family curve selection's readout (task arc-recovery-engine): the picked curve's own
  // theoretical witness — the 820 ruling's caliber split (the arc's literal member, the
  // reference λ/ideal frozen family, the recomputable pose; never an actual-measure
  // interpretation). The overlap cycle walks every curve that shared the click; "Set sky point
  // at this image" hands the witness's own sky position to the existing point flow (the
  // fixed-member explanation and Pose & Trace answer at the same sky position — a separate
  // caliber, asked beside this one, not over it). Renders only while the selection is live
  // (identity-bound); no selection, no space.
  if (FamilyCurveSelectionActive(state)) {
    const SurveyOverlayCache& sel_cache =
        RefreshSurveyOverlayCache(state.analysis.survey, state.analysis.FrozenPointSunPremise());
    const int curve_index = state.analysis.family_curve_index;
    if (sel_cache.model.valid && curve_index >= 0 &&
        curve_index < static_cast<int>(sel_cache.facts.family_curves.size())) {
      const SurveyOverlayFacts::FamilyCurveFact& curve =
          sel_cache.facts.family_curves[static_cast<size_t>(curve_index)];
      ImGui::Separator();
      ImGui::TextUnformatted("Family curve selection");
      const int source_count = static_cast<int>(state.analysis.family_curve_sources.size());
      if (source_count > 1) {
        if (ImGui::SmallButton("<##curve_source")) {
          CycleFamilyCurveSource(state, -1);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(">##curve_source")) {
          CycleFamilyCurveSource(state, 1);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("source %d/%d at this position", state.analysis.family_curve_source_slot + 1, source_count);
        if (ImGui::IsItemHovered()) {
          SetWrappedTooltip("Several curves share this position — switch the selection between them.");
        }
      }
      RenderFamilyCurveHeader(curve);
      if (sel_cache.model.scope_available) {
        ImGui::TextDisabled("reference λ %.1f nm · the ideal frozen family's own critical arc",
                            sel_cache.model.wavelength_nm);
      }
      // The picked event's own witness (the cycle reset reads the switched curve's first point).
      FamilyCurvePick pick;
      pick.curve_index = curve_index;
      pick.kind = static_cast<FamilyCurvePickKind>(state.analysis.family_curve_pick_kind);
      pick.point_index = state.analysis.family_curve_point_index;
      pick.segment_index = state.analysis.family_curve_segment_index;
      pick.cusp_index = state.analysis.family_curve_cusp_index;
      double w_params[2] = { 0.0, 0.0 };
      double w_pose[9] = {};
      double w_alt = 0.0;
      double w_az = 0.0;
      bool have_witness = false;
      switch (pick.kind) {
        case FamilyCurvePickKind::kPoint:
          if (pick.point_index >= 0 && pick.point_index < static_cast<int>(curve.points.size())) {
            const SurveyOverlayFacts::FamilyCurveFact::Point& p = curve.points[static_cast<size_t>(pick.point_index)];
            std::copy(p.params_deg, p.params_deg + 2, w_params);
            std::copy(p.pose, p.pose + 9, w_pose);
            w_alt = p.alt_deg;
            w_az = p.az_deg;
            have_witness = true;
          }
          break;
        case FamilyCurvePickKind::kPointImage:
          if (pick.segment_index >= 0 && pick.segment_index < static_cast<int>(curve.segments.size())) {
            const SurveyOverlayFacts::FamilyCurveFact::Segment& s =
                curve.segments[static_cast<size_t>(pick.segment_index)];
            ImGui::TextDisabled("point image: this run of the branch collapses to one image");
            ImGui::TextDisabled("constant image alt %.3f° az %.3f° · measured diameter %.2e°", s.image_alt_deg,
                                s.image_az_deg, s.image_diameter_deg);
            if (s.first >= 0 && s.first < static_cast<int>(curve.points.size())) {
              const SurveyOverlayFacts::FamilyCurveFact::Point& p =
                  curve.points[static_cast<size_t>(s.first)];  // a legal preimage of the run
              std::copy(p.params_deg, p.params_deg + 2, w_params);
              std::copy(p.pose, p.pose + 9, w_pose);
              w_alt = p.alt_deg;
              w_az = p.az_deg;
              have_witness = true;
            }
          }
          break;
        case FamilyCurvePickKind::kCusp:
          if (pick.cusp_index >= 0 && pick.cusp_index < static_cast<int>(curve.cusps.size())) {
            const SurveyOverlayFacts::FamilyCurveFact::Cusp& c = curve.cusps[static_cast<size_t>(pick.cusp_index)];
            ImGui::TextDisabled("cusp candidate (not certified) · |dJ(v)| residual bound %.2e", c.djv_residual);
            std::copy(c.params_deg, c.params_deg + 2, w_params);
            std::copy(c.pose, c.pose + 9, w_pose);
            w_alt = c.alt_deg;
            w_az = c.az_deg;
            have_witness = true;
          }
          break;
      }
      if (have_witness) {
        RenderFamilyCurvePoseWitness(w_params, w_pose, w_alt, w_az);
        if (ImGui::SmallButton("Set sky point at this image###curve_pick")) {
          float dir[3];
          AltAzToDir(static_cast<float>(w_alt), static_cast<float>(w_az), dir);
          state.analysis.sky_point.valid = true;
          state.analysis.sky_point.from_map_pick = false;
          state.analysis.sky_point.dir[0] = dir[0];
          state.analysis.sky_point.dir[1] = dir[1];
          state.analysis.sky_point.dir[2] = dir[2];
        }
        if (ImGui::IsItemHovered()) {
          SetWrappedTooltip(
              "Asks the existing point flow at this image position — the fixed-member explanation "
              "and Pose & Trace answer there (a separate caliber from this theoretical witness).");
        }
        ImGui::SameLine();
      }
      if (ImGui::SmallButton("Clear##curve_pick")) {
        ClearFamilyCurveSelection(state);
      }
    } else {
      ClearFamilyCurveSelection(state);  // the facts cannot resolve it: nothing to show
    }
  }

  ImGui::BeginChild("##MapReport", RemainingReportSize());
  // The class answer (contract §16.2): the adopted survey document, drawn by the
  // complete-answer renderer — the same default layer and fold groups the renderer gives every
  // host, read back from the SAME adopted result the main window's summary reads (no second
  // model, no second session; opening or closing this window changes no query lifetime). The
  // four-state shell stays in the main window (§16.2's split): while the survey computes, is
  // cancelled or refused, this section draws nothing here — the answer region carries answers.
  if (state.analysis.survey.adopted_result) {
    const SurveyOverviewModel survey_model =
        BuildSurveyOverviewModel(*state.analysis.survey.adopted_result, state.analysis.FrozenPointSunPremise());
    ImGui::TextDisabled("Class answer");
    // The profile rows' read (§16.1.3): this class-answer host owns the standing's overwrite
    // every frame (hover or explicit none); the canvas consumes it next frame. The verdict and
    // Position rows keep their highlight feedback local, without a session standing. The
    // inner-edge entry's wiring (817.2): the selection's liveness, the arc layers' visibility
    // fact this frame (the master gate and the edge graphics gate), and the click the entry's
    // surface may have taken (the scroll request consumed above).
    SurveyRowHoverReadout row_readout;
    SurveyInnerEdgeInteraction inner_edge;
    inner_edge.selected = InnerEdgeSelectionActive(state);
    // The C01 stroke's visibility: the shared stroke draws while either owner's gate shows it
    // — the note's "not drawn there" is exact only under this condition.
    inner_edge.map_layer_draws = overlays && (gates.reach.graphics || gates.edge.graphics);
    inner_edge.entry_request_scroll = state.analysis.inner_edge_scroll_hint;
    RenderSurveyOverviewModel(survey_model, &row_readout, &inner_edge);
    state.analysis.profile_row_hover = row_readout.profile;
    if (inner_edge.select_requested) {
      SelectInnerEdgeObject(state);  // the text entry commits against the same adopted identity
    }
    state.analysis.inner_edge_scroll_hint = false;  // this render was the sole reader
    ImGui::Separator();
  } else {
    // The class answer's absent frame (no adopted document): no rows render, so the overwrite
    // has no site — this branch is the reset's owner instead. Without it a document's last
    // hover standing would outlive the rows that carried it.
    state.analysis.profile_row_hover = SurveyProfileHover::kNone;
  }
  // The sky-point answer (contract §16.2): the adopted point document read in full, sorted
  // into its own fold pair. Sits after the class answer, before the map's own report — the
  // ledger's answer-region order. Same gate as the class answer: an adopted document in hand
  // draws its content (including across a re-query's in-flight window — the four-state shell
  // and the cancel actions stay in the main window, and the input group's target line carries
  // the intent/adopted distinction).
  if (state.analysis.point.adopted_result) {
    ImGui::TextDisabled("Sky point answer");
    RenderPointAnswerBlock(state);
    ImGui::Separator();
  }
  ImGui::TextWrapped("Stripes = unknown, purple = singular (not zero).");
  // The identity line and the sky-point echo used to sit here; both moved up the fixed slots
  // (identity to the column's first row, the target beside the input group) and the report no
  // longer repeats either. The old reference-cell line moved into the caliber's env-gated
  // diagnostics fold (the same numbers, the ledger's L4 tier); the source-fixed-scale sentence
  // moved with it. The Separator keeps the picture-facts above apart from the answer block
  // below.
  ImGui::Separator();
  if (state.analysis.map_wavelength_nm <= 0.0 && map.adopted_result == nullptr) {
    // The λ's unrechoiced state stays default (the ledger: 尚未回显的未知性默认) — the map has
    // not answered at any wavelength yet, and the slider's tooltip names the source rule.
    ImGui::TextWrapped("The engine-chosen reference wavelength is used until a map echoes it.");
  }
  // The caliber block: every fact read back through the result's own readers into the pure
  // composers (the unit layer holds them to the readback), then drawn — the ledger's tiers:
  // the short quantity and its limits default, the full definition and premises behind
  // "Map quantity & premises", the grid and execution numbers behind the env-gated "Map
  // diagnostics". The report follows every operation: its native scroll cannot overlap or
  // reposition the typed-coordinate controls, even while a long caliber or refusal arrives.
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
    in.map_symmetry_bits = map.key.symmetry_bits;
    in.survey_scope_differs = map.key.symmetry_bits > 0 && map.key.symmetry_bits < 7;
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    for (const std::string& line : MapCaliberDefaultLines(in)) {
      ImGui::TextWrapped("%s", line.c_str());
    }
    ImGui::PopStyleColor();
    if (FoldOpen(kMapCaliberPremisesLabel, "map_caliber")) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
      for (const std::string& line : MapCaliberPhysicsLines(in)) {
        ImGui::TextWrapped("%s", line.c_str());
      }
      ImGui::PopStyleColor();
    }
    // The diagnostics group is env-gated as a whole (header and body): with the env off
    // neither the header nor the raw basis/grid/execution lines render, and the group's old
    // expansion state survives behind the absent header for env-on to restore.
    if (env::AnalyzeDebugDetails() && FoldOpen(kMapCaliberDiagnosticsLabel, "map_caliber")) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
      for (const std::string& line : MapCaliberDiagnosticsLines(in)) {
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
      DrawLegendIconSlot(
          [legend_dl = ImGui::GetWindowDrawList(), p = ImGui::GetCursorScreenPos()]() {
            // The sun's own marker at icon scale: the gold fill and the fainter ring, the map's
            // proportions (UiPx(4) fill, UiPx(6) ring) shrunk into the slot.
            const float r = UiPx(4.0f);
            const ImVec2 c(p.x + ImGui::GetFrameHeight() * 0.5f - UiPx(3.0f), p.y + ImGui::GetTextLineHeight() * 0.5f);
            legend_dl->AddCircleFilled(c, r * 0.8f, IM_COL32(255, 231, 168, 255));
            legend_dl->AddCircle(c, r, IM_COL32(255, 231, 168, 128), 0, 1.0f);
          },
          ImGui::GetFrameHeight(), 0.0f, 0.0f);
      ImGui::TextWrapped("sun marker");
    }
    if (legend.anchor_zenith) {
      ImGui::Bullet();
      DrawLegendIconSlot(
          [legend_dl = ImGui::GetWindowDrawList(), p = ImGui::GetCursorScreenPos()]() {
            // The cross's own two lines, the map's colour and span.
            const float a = UiPx(3.0f);
            const ImVec2 c(p.x + ImGui::GetFrameHeight() * 0.5f - UiPx(3.0f), p.y + ImGui::GetTextLineHeight() * 0.5f);
            legend_dl->AddLine(ImVec2(c.x - a, c.y), ImVec2(c.x + a, c.y), IM_COL32(200, 205, 215, 110), 1.0f);
            legend_dl->AddLine(ImVec2(c.x, c.y - a), ImVec2(c.x, c.y + a), IM_COL32(200, 205, 215, 110), 1.0f);
          },
          ImGui::GetFrameHeight(), 0.0f, 0.0f);
      ImGui::TextWrapped("zenith cross");
    }
    if (legend.anchor_horizon) {
      ImGui::Bullet();
      DrawLegendIconSlot(
          [legend_dl = ImGui::GetWindowDrawList(), p = ImGui::GetCursorScreenPos()]() {
            // The horizon's own circle, in the horizon's hue. The map draws it at alpha 60 — a
            // large reference circle reads there, an icon-size one does not; the legend icon takes
            // the zenith's alpha (the hue is the correspondence, the alpha the icon's legibility).
            const ImVec2 c(p.x + ImGui::GetFrameHeight() * 0.5f - UiPx(3.0f), p.y + ImGui::GetTextLineHeight() * 0.5f);
            legend_dl->AddCircle(c, UiPx(4.0f), IM_COL32(200, 205, 215, 110), 0, 1.2f);
          },
          ImGui::GetFrameHeight(), 0.0f, 0.0f);
      ImGui::TextWrapped("horizon circle");
    }
    for (const std::string& kind : legend.object_kinds) {
      const OverlayMarkerStyle style = OverlayMarkerStyleFor(kind, LUMICE_PATH_STATE_CERTIFIED);
      ImGui::Bullet();
      // The kind's own marker shape, drawn beside the words (the legend and the picture agree).
      DrawLegendIconSlot(
          [legend_dl = ImGui::GetWindowDrawList(), &style]() {
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
          },
          ImGui::GetFrameHeight(), 0.0f, 0.0f);
      // The legend's words use the map's display vocabulary (the caustic marker's entry says
      // what the ink is); the icon reads the raw kind's shape, the same mapping the label uses.
      ImGui::TextWrapped("%s", OverlayKindDisplayName(kind));
    }
    // The three arc layers' entries: one ink, named by owner (the layers differ by name, never by
    // hue — the map's colour vocabulary stays one semantics one entry). The full-range form drew
    // text, so its entry carries text and no arc icon.
    const auto arc_icon = []() {
      DrawLegendIconSlot(
          [legend_dl = ImGui::GetWindowDrawList(), p = ImGui::GetCursorScreenPos()]() {
            const ImVec2 c(p.x + ImGui::GetFrameHeight() * 0.5f - UiPx(3.0f), p.y + ImGui::GetTextLineHeight() * 0.5f);
            legend_dl->AddCircle(c, UiPx(4.0f), kSurveyArcInk, 0, UiPx(1.2f));
          },
          ImGui::GetFrameHeight(), 0.0f, 0.0f);
    };
    // The keys name the FIXED caliber each layer quotes (the survey's own scope wavelength for
    // the bounds and edges, the chromatic pair's words for the turns) — the map's λ slider never
    // masquerades as these references' caliber, and an unknown scope says so.
    if (legend.reach_bounds) {
      ImGui::Bullet();
      if (legend.reach_full_range_text) {
        ImGui::TextWrapped("support range — full 0–180° (text, no bounds) · survey %s", legend.arc_lambda_text.c_str());
      } else {
        arc_icon();
        ImGui::TextWrapped("support range — %d bound%s · survey %s", legend.reach_bound_count,
                           legend.reach_bound_count == 1 ? "" : "s", legend.arc_lambda_text.c_str());
      }
    }
    if (legend.edge_arcs) {
      ImGui::Bullet();
      arc_icon();
      ImGui::TextWrapped("sharp edge%s · survey %s",
                         legend.edge_shared ? " — one stroke shared with the support range" : "",
                         legend.arc_lambda_text.c_str());
    }
    if (legend.tir_turns) {
      ImGui::Bullet();
      arc_icon();
      ImGui::TextWrapped("internal-reflection turns — %d turn pair%s · chromatic pair %s", legend.tir_turn_count,
                         legend.tir_turn_count == 1 ? "" : "s",
                         legend.tir_lambda_text.empty() ? "wavelength unknown" : legend.tir_lambda_text.c_str());
    }
    if (legend.family_curves) {
      ImGui::Bullet();
      // The curve's own icon: a short line in the arc's hue (the marker-family colour the arc
      // carries forward), not the neutral arc ring the reference layers share.
      DrawLegendIconSlot(
          [legend_dl = ImGui::GetWindowDrawList(), p = ImGui::GetCursorScreenPos()]() {
            const ImVec2 c(p.x + ImGui::GetFrameHeight() * 0.5f - UiPx(3.0f), p.y + ImGui::GetTextLineHeight() * 0.5f);
            legend_dl->AddLine(ImVec2(c.x - UiPx(4.0f), c.y), ImVec2(c.x + UiPx(4.0f), c.y), kFamilyArcInk, UiPx(1.4f));
          },
          ImGui::GetFrameHeight(), 0.0f, 0.0f);
      ImGui::TextWrapped("family fold arcs — %d curve%s · hover for member & pose", legend.family_curve_count,
                         legend.family_curve_count == 1 ? "" : "s");
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
    if (legend.verdict_anchor) {
      ImGui::Bullet();
      DrawLegendIconSlot(
          [legend_dl = ImGui::GetWindowDrawList(), p = ImGui::GetCursorScreenPos(), &legend]() {
            // The anchor's own circle in the palette entry that actually drew — dashed when the
            // drawn form was (the walk's own dash phase, 4 samples on, 4 off, so the icon reads the
            // same visual grammar the anchor drew).
            const ImVec2 c(p.x + ImGui::GetFrameHeight() * 0.5f - UiPx(3.0f), p.y + ImGui::GetTextLineHeight() * 0.5f);
            const float r = UiPx(4.0f);
            if (legend.verdict_anchor_dashed) {
              constexpr int kSamples = 24;
              ImVec2 prev(0.0f, 0.0f);
              for (int s = 0; s <= kSamples; ++s) {
                const float a = 2.0f * static_cast<float>(kPiD) * s / kSamples;
                const ImVec2 pt(c.x + r * std::cos(a), c.y + r * std::sin(a));
                if (s > 0 && (s / 4) % 2 == 0) {
                  legend_dl->AddLine(prev, pt, legend.verdict_anchor_colour, UiPx(1.2f));
                }
                prev = pt;
              }
            } else {
              legend_dl->AddCircle(c, r, legend.verdict_anchor_colour, 0, UiPx(1.2f));
            }
          },
          ImGui::GetFrameHeight(), 0.0f, 0.0f);
      ImGui::TextWrapped("verdict anchor");
    }
    if (legend.sky_point) {
      ImGui::Bullet();
      ImGui::TextWrapped("sky-point marker");
    }
    if (legend.point_image) {
      ImGui::Bullet();
      ImGui::TextWrapped("point image (rank-0 answer)");
    }
    // The long dictionary: the entries' meanings and honesty qualifications, behind one
    // closed-by-default fold (the ledger's Legend explanations group).
    if (FoldOpen(kMapLegendExplanationsLabel, "map_legend")) {
      if (legend.anchor_sun) {
        ImGui::TextWrapped("sun marker — zero deviation");
      }
      if (legend.reach_bounds) {
        ImGui::TextWrapped(
            "support range — the delivered support intervals' bounds, angular-distance references at the survey's own "
            "wavelength: where the class can light, not a glowing ring; a full 0–180° range states itself in text");
      }
      if (legend.edge_arcs) {
        ImGui::TextWrapped(
            "sharp edge — the edge card's position at the deviation it names; dashed when the claim has not concluded; "
            "a stroke shared with the support range carries both owners' names here");
      }
      if (legend.tir_turns) {
        ImGui::TextWrapped(
            "internal-reflection turns — the visible TIR discriminant feature's representative angular distances at "
            "the chromatic pair's two wavelengths; the step number names the reflection step along the representative "
            "path, not a crystal face; a reference pair, not a whole-circle colour claim");
      }
      if (!legend.object_kinds.empty()) {
        ImGui::TextWrapped("each kind's marker — the survey's located conclusion, at the position the card names");
      }
      if (legend.dispersion_red || legend.dispersion_blue) {
        ImGui::TextWrapped(
            "dispersion circles — the measured minimum-deviation geometry at the reference wavelengths; positions, "
            "not a whole-circle visibility or contribution claim");
      }
      if (legend.verdict_anchor) {
        ImGui::TextWrapped(
            "verdict anchor — the class answer's colour verdict at the deviation it names; "
            "dashed when its coverage is incomplete");
      }
      if (legend.sky_point) {
        ImGui::TextWrapped("sky-point marker — the target the point face queries");
      }
      if (legend.point_image) {
        ImGui::TextWrapped(
            "point image — the adopted point query's rank-0 answer, at the position the answer names; on a matched "
            "map the mass cell itself carries the ink and no second symbol draws. The target marker stays the "
            "user's own — the image never moves it");
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
    // The window's closed frames: the class answer's rows do not render, so the hover standing's
    // overwrite has no site — this seam owns the reset, and the first frame after a reopen reads
    // false instead of whatever the mouse was on when the window closed.
    state.analysis.profile_row_hover = SurveyProfileHover::kNone;
    if (state.workspace.map_press_owner != GuiState::AnalyzeWorkspaceSession::PanoramaPressOwner::kNone)
      state.workspace.map_press_owner = GuiState::AnalyzeWorkspaceSession::PanoramaPressOwner::kCancelled;
    return;
  }
  // The pumps run at RenderWorkspacePanels' entry since 755.5 (every GUI frame, windows open or
  // not); this window keeps only its own wait-feedback hygiene: on the Render-workspace frames
  // it is the map fields' — and the staleness hints' — only render site, so a field's content is
  // exactly what the user saw THIS frame — the site that can draw it owns the clear. On an
  // Analyze frame the workspace entry already pumped and cleared; the footer below is
  // then that frame's only map-wait-field writer.
  if (state.workspace.mode != GuiState::WorkspaceMode::kAnalyze) {
    WaitFeedbackTexts& feedback = state.analysis.wait_feedback_texts;
    feedback.map_wait_line.clear();
    feedback.map_superseding_note.clear();
    feedback.map_stale_hint.clear();
    feedback.map_drift_hint.clear();
    feedback.map_stale_hint_rect_min_x = 0.0f;
    feedback.map_stale_hint_rect_min_y = 0.0f;
    feedback.map_stale_hint_rect_max_x = 0.0f;
    feedback.map_stale_hint_rect_max_y = 0.0f;
    feedback.map_drift_hint_rect_min_x = 0.0f;
    feedback.map_drift_hint_rect_min_y = 0.0f;
    feedback.map_drift_hint_rect_max_x = 0.0f;
    feedback.map_drift_hint_rect_max_y = 0.0f;
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
    // The Begin-false frame (the window fully clipped): neither region renders, so the hover
    // standing's overwrite has no site here either — the same reset the closed seam owns.
    state.analysis.profile_row_hover = SurveyProfileHover::kNone;
    ImGui::End();
    if (state.workspace.map_press_owner != GuiState::AnalyzeWorkspaceSession::PanoramaPressOwner::kNone)
      state.workspace.map_press_owner = GuiState::AnalyzeWorkspaceSession::PanoramaPressOwner::kCancelled;
    return;
  }
  // The staleness hints' fixed bar: the map describes the analysis payload, so both facts that
  // stale it state themselves here — the list's freshness against the live document, and (Point
  // mode) the cone centre's drift from the analysed one — each fact its own line, appearing and
  // disappearing independently, notice-only (no button, no action: the refresh is the user's
  // explicit Analyze, the contract's no-auto-rerun rule). The bar sits above the region and only
  // lowers it: VisibleContentSize() below is taken after these lines, so the D3 split's span
  // shrinks by the bar's real footprint and no stored ratio moves — the same "appearing changes
  // the content's height only" geometry contract the main view's band follows. Each line
  // reports the string it drew and its FULL item rect (the completeness face beside the ink the
  // vertices carry) into this site's own fields — per-window, never the main band's: a field's
  // single-writer contract follows the render site, not the wording.
  WaitFeedbackTexts& hint_feedback = state.analysis.wait_feedback_texts;
  if (state.analysis_freshness.list == AnalysisListFreshness::kStale) {
    // The field carries the string the draw drew, icon included — what the test layer reads is
    // what was drawn (the band's field contract, one shape per face).
    const char* const stale_text = ICON_FA_TRIANGLE_EXCLAMATION
        " This map is from a configuration that has since changed \xe2\x80\x94 press Analyze to refresh.";
    WarningTextWrapped("%s", stale_text);
    const ImVec2 rect_min = ImGui::GetItemRectMin();
    const ImVec2 rect_max = ImGui::GetItemRectMax();
    hint_feedback.map_stale_hint_rect_min_x = rect_min.x;
    hint_feedback.map_stale_hint_rect_min_y = rect_min.y;
    hint_feedback.map_stale_hint_rect_max_x = rect_max.x;
    hint_feedback.map_stale_hint_rect_max_y = rect_max.y;
    hint_feedback.map_stale_hint = stale_text;
  }
  if (state.analysis_freshness.cone_center_drifted) {
    const char* const drift_text = ConeCenterDriftWarningText();
    WarningTextWrapped("%s", drift_text);
    const ImVec2 rect_min = ImGui::GetItemRectMin();
    const ImVec2 rect_max = ImGui::GetItemRectMax();
    hint_feedback.map_drift_hint_rect_min_x = rect_min.x;
    hint_feedback.map_drift_hint_rect_min_y = rect_min.y;
    hint_feedback.map_drift_hint_rect_max_x = rect_max.x;
    hint_feedback.map_drift_hint_rect_max_y = rect_max.y;
    hint_feedback.map_drift_hint = drift_text;
  }
  // Reserve the common footer before the body submits any child. Its state cannot change geometry.
  const AnalyzeStatusBarLayout status_layout = ReserveAnalyzeStatusBar();
  RenderAnalyzeMapRegion(state, status_layout.body_size);
  const MapQuerySession& map = state.analysis.map;
  const MapQueryState query_state = MapQueryStateFor(map);
  // Waiting requires no adopted result; supersession requires a retained result. They cannot
  // coincide: a retained-map retry reads Completed (or Refused), then takes the note below.
  const bool superseded = MapSupersededFor(map);
  std::string status;
  bool warning = false;
  switch (query_state) {
    case MapQueryState::kNoSelection:
      status = "No selection";
      break;
    case MapQueryState::kCancelled:
      status = "Cancelled — frozen map remains; Retry repeats the full-sky request. Observation never retries.";
      break;
    case MapQueryState::kRefused:
      status = "Refused — " + map.adopted_error;
      warning = true;
      break;
    case MapQueryState::kWaiting: {
      const std::string wait_line = MapWaitLineFor(map, g_survey_executor.QueryOutstanding(QueryFace::kSurvey),
                                                   ImGui::GetTime(), g_survey_executor.InflightPollSnapshot());
      status = "Waiting — " + wait_line;
      state.analysis.wait_feedback_texts.map_wait_line = wait_line;
      break;
    }
    case MapQueryState::kCompleted:
      status = "Ready";
      break;
  }
  if (superseded) {
    const std::string note = MapSupersedingNoteFor(map, ImGui::GetTime(), g_survey_executor.InflightPollSnapshot());
    status = "Superseded — " + note;
    state.analysis.wait_feedback_texts.map_superseding_note = note;
  }
  // Partial coverage and supersession/cancellation are independent facts. Read the same source
  // Info as the frozen display, not a second completion latch or the right column's point query.
  LUMICE_PathMapInfo info{};
  info.struct_size = sizeof(info);
  if (map.adopted_result && LUMICE_PathMapGetInfo(map.adopted_result.get(), &info) == LUMICE_PATH_READ_OK &&
      MapInfoIsPartial(info)) {
    const std::string partial = "Partial map: " + std::to_string(info.members_evaluated) + "/" +
                                std::to_string(info.member_count) + " completed members";
    status = query_state == MapQueryState::kCompleted && !superseded ? partial : partial + " — " + status;
    warning = true;
  }
  RenderAnalyzeStatusBar(status_layout, status, warning);
  ImGui::End();
}

}  // namespace lumice::gui
