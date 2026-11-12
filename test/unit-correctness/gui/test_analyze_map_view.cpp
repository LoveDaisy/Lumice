#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "gui/analysis_panel.hpp"
#include "gui/analysis_result.hpp"
#include "gui/analyze_map_view.hpp"
#include "gui/analyze_overview.hpp"
#include "gui/analyze_survey.hpp"
#include "gui/file_io.hpp"  // SceneDeleter's complete type — the survey tests' include
#include "gui/gui_state.hpp"
#include "gui/preview_renderer.hpp"  // ViewProjection's full shape — the anchor tests' camera
#include "imgui.h"
#include "include/lumice_base.h"
#include "include/lumice_raypath.h"
#include "include/lumice_scene.h"
#include "util/sky_direction.hpp"

namespace lumice::gui {
namespace {

constexpr double kPiD = 3.14159265358979323846;
constexpr double kD2R = kPiD / 180.0;
constexpr double kR2D = 180.0 / kPiD;

// The position vector of the sky label (alt, az) in degrees — the tests' own §12 spelling.
void SkyPositionOf(double alt_deg, double az_deg, double pos[3]) {
  const double alt = alt_deg * kD2R;
  const double az = az_deg * kD2R;
  pos[0] = std::cos(alt) * std::cos(az);
  pos[1] = std::cos(alt) * std::sin(az);
  pos[2] = std::sin(alt);
}

double DotOf(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

double AngleBetween(const double a[3], const double b[3]) {
  return std::acos(std::max(-1.0, std::min(1.0, DotOf(a, b))));
}

TEST(AnalyzeMapView, RequestCarriesClassViewLambdaAndTheConstants) {
  MapIntentKey key;
  key.selection.crystal_id = 3;
  key.selection.faces = { 3, 5 };
  key.selection.layer_face_counts = { 2 };
  key.selection.generation = 7;
  key.wavelength_nm = 0.0;  // the reference: no explicit buffer

  std::vector<int> faces;
  std::vector<int> counts;
  std::vector<double> wavelengths;
  const LUMICE_PathMapRequest request = MakeMapRequestForKey(key, &faces, &counts, &wavelengths);
  EXPECT_EQ(request.crystal_id, 3);
  EXPECT_EQ(request.face_count, 2);
  EXPECT_EQ(faces, (std::vector<int>{ 3, 5 }));
  EXPECT_EQ(counts, (std::vector<int>{ 2 }));
  EXPECT_EQ(request.full_sky, 1);
  EXPECT_DOUBLE_EQ(request.centre_altitude_deg, 0.0);
  EXPECT_DOUBLE_EQ(request.centre_azimuth_deg, 0.0);
  EXPECT_DOUBLE_EQ(request.delta_span_deg, 0.0);
  EXPECT_DOUBLE_EQ(request.azimuth_span_deg, 0.0);
  EXPECT_EQ(request.view_rows, 0);
  EXPECT_EQ(request.view_cols, 0);
  EXPECT_EQ(request.sample_count, kMapQuerySampleCount);
  EXPECT_EQ(request.budget_ms, LUMICE_PATH_MAP_DEFAULT_BUDGET_MS);
  EXPECT_EQ(request.wavelengths_nm, nullptr);
  EXPECT_EQ(request.wavelength_count, 0);

  // An explicit wavelength rides its own buffer.
  key.wavelength_nm = 650.0;
  const LUMICE_PathMapRequest red = MakeMapRequestForKey(key, &faces, &counts, &wavelengths);
  ASSERT_EQ(red.wavelength_count, 1);
  ASSERT_NE(red.wavelengths_nm, nullptr);
  EXPECT_DOUBLE_EQ(red.wavelengths_nm[0], 650.0);
}

// ---- Fit, normalization, colour --------------------------------------------------------------------

namespace {

// A synthetic result's view: the frame the tests spell by hand (the map's own (delta, azimuth)
// read of every synthetic cell), so a Fit's or a norm's inputs are fully known.
struct SyntheticGrid {
  LUMICE_PathMapInfo info{};
  std::vector<double> delta;
  std::vector<double> azimuth;
  std::vector<double> values;
  std::vector<int> counts;
  std::vector<uint8_t> status;
  LUMICE_PathMapPixels pixels{};

  static SyntheticGrid Make(int rows, int cols, double cell_deg) {
    SyntheticGrid g;
    g.info.struct_size = sizeof(g.info);
    g.info.view_incident[0] = -1.0;
    g.info.view_e1[2] = 1.0;
    g.info.view_e2[1] = 1.0;
    g.info.view_rows = rows;
    g.info.view_cols = cols;
    g.info.view_full_sky = 0;
    g.info.view_delta_min_deg = 0.0;
    g.info.view_delta_max_deg = rows * cell_deg;
    g.info.view_az_min_deg = 0.0;
    g.info.view_az_max_deg = cols * cell_deg;
    for (int r = 0; r < rows; ++r) {
      for (int c = 0; c < cols; ++c) {
        g.delta.push_back((r + 0.5) * cell_deg);
        g.azimuth.push_back((c + 0.5) * cell_deg);
        g.values.push_back(0.0);
        g.counts.push_back(0);
        g.status.push_back(LUMICE_PATH_MAP_PIXEL_OK);
      }
    }
    g.pixels.struct_size = sizeof(g.pixels);
    g.pixels.delta_deg = g.delta.data();
    g.pixels.azimuth_deg = g.azimuth.data();
    g.pixels.values = g.values.data();
    g.pixels.counts = g.counts.data();
    g.pixels.status = g.status.data();
    g.pixels.pixel_count = g.delta.size();
    return g;
  }

  size_t At(int row, int col) const {
    return static_cast<size_t>(row) * static_cast<size_t>(info.view_cols) + static_cast<size_t>(col);
  }
};

}  // namespace

TEST(AnalyzeMapView, FitFindsTheWeightedCentroidAndTheMassRadius) {
  // Two lit cells: a heavy one and a light one a known angular distance away (the cells' sky
  // positions spelled through the test's own frame reading).
  SyntheticGrid g = SyntheticGrid::Make(40, 40, 1.0);
  const auto cell_pos = [&](double delta_deg, double az_deg, double pos[3]) {
    const double d = delta_deg * kD2R;
    const double a = az_deg * kD2R;
    // The test's own frame: incident (−1,0,0), e1 (0,0,1), e2 (0,1,0).
    const double travel[3] = { -std::cos(d), std::sin(d) * std::sin(a), std::sin(d) * std::cos(a) };
    pos[0] = -travel[0];
    pos[1] = -travel[1];
    pos[2] = -travel[2];
  };
  // The cells' CENTRES sit at (row + 0.5, col + 0.5) degrees — what the pixel columns echo and
  // what the fit reads.
  double heavy[3];
  cell_pos(20.5, 0.5, heavy);
  double light[3];
  cell_pos(20.5, 30.5, light);
  g.values[g.At(20, 0)] = 9.0;
  g.counts[g.At(20, 0)] = 100;
  g.values[g.At(20, 30)] = 1.0;
  g.counts[g.At(20, 30)] = 10;
  const MapFitResult fit = FitMapViewFromPixels(g.pixels, g.info);
  ASSERT_TRUE(fit.ok);
  // The centroid leans 9:1 toward the heavy cell.
  double centroid[3] = { (9.0 * heavy[0] + 1.0 * light[0]) / 10.0, (9.0 * heavy[1] + 1.0 * light[1]) / 10.0,
                         (9.0 * heavy[2] + 1.0 * light[2]) / 10.0 };
  const double cn = std::sqrt(DotOf(centroid, centroid));
  for (int k = 0; k < 3; ++k) {
    centroid[k] /= cn;
  }
  const double got[3] = { fit.centre_pos[0], fit.centre_pos[1], fit.centre_pos[2] };
  EXPECT_NEAR(AngleBetween(got, centroid), 0.0, 1e-6);  // the fit result is float-quantized
  // radius95: the heavy cell covers 90% alone, so the radius reaches the light cell — measured
  // from the FIT CENTROID (the prototype's radius95 is a distance from the centre it just
  // found), times the prototype's 1.4 headroom, clamped.
  EXPECT_NEAR(fit.radius_deg, AngleBetween(centroid, light) * kR2D * 1.4, 1e-6);
}

TEST(AnalyzeMapView, FitRejectsAnHonestAllZeroMap) {
  SyntheticGrid g = SyntheticGrid::Make(20, 20, 1.0);
  const MapFitResult fit = FitMapViewFromPixels(g.pixels, g.info);
  EXPECT_FALSE(fit.ok);
}

TEST(AnalyzeMapView, NormIsTheRobustMaximumOfThePositiveValues) {
  SyntheticGrid g = SyntheticGrid::Make(10, 100, 1.0);  // 1000 cells
  for (int i = 0; i < 1000; ++i) {
    g.values[static_cast<size_t>(i)] = static_cast<double>(i + 1);  // 1..1000
  }
  g.status[g.At(0, 0)] = LUMICE_PATH_MAP_PIXEL_SINGULAR;  // excluded: no value at all
  g.values[g.At(0, 0)] = 0.0;
  const MapDisplayNorm norm = MapDisplayNormFor(g.pixels);
  ASSERT_TRUE(norm.valid);
  // The positives are now {2..1000} (999 of them); the 99th percentile is the 990th smallest,
  // 991.
  EXPECT_NEAR(norm.vmax, 991.0, 1e-9);

  // No positive value anywhere: the honest all-zero texture asks for no normalization.
  SyntheticGrid z = SyntheticGrid::Make(4, 4, 1.0);
  EXPECT_FALSE(MapDisplayNormFor(z.pixels).valid);
}

TEST(AnalyzeMapView, CellColorHonorsTheStatuses) {
  MapDisplayNorm norm;
  norm.valid = true;
  norm.vmax = 10.0;
  EXPECT_EQ(MapCellColorRgba(0.0, LUMICE_PATH_MAP_PIXEL_SINGULAR, norm), 0u);
  const uint32_t mass = MapCellColorRgba(3.0, LUMICE_PATH_MAP_PIXEL_POINT_MASS, norm);
  EXPECT_EQ((mass >> 24) & 0xFF, 0xFF);  // opaque, the fixed mass colour
  const uint32_t zero = MapCellColorRgba(0.0, LUMICE_PATH_MAP_PIXEL_OK, norm);
  EXPECT_EQ((zero >> 24) & 0xFF, 0xFF);  // a finite zero is a claim: opaque dark, not transparent
  const uint32_t top = MapCellColorRgba(10.0, LUMICE_PATH_MAP_PIXEL_OK, norm);
  EXPECT_GT(
      static_cast<int>((top >> 0) & 0xFF) + static_cast<int>((top >> 8) & 0xFF) + static_cast<int>((top >> 16) & 0xFF),
      700);  // the maximum sits near white
}

// ---- The caliber composer (the readback the region draws, held field by field) ---------------------

TEST(AnalyzeMapView, CaliberLinesCarryTheReaderFieldsVerbatim) {
  MapCaliberInput in;
  in.info.struct_size = sizeof(in.info);
  in.info.wavelength_nm = 550.0;
  in.info.members_evaluated = 9;
  in.info.member_count = 12;
  in.info.sample_count = 500000;
  in.info.kept_count = 12345;
  in.info.view_full_sky = 0;
  in.info.view_delta_min_deg = 3.5;
  in.info.view_delta_max_deg = 48.5;
  in.info.view_az_min_deg = -100.0;
  in.info.view_az_max_deg = 260.0;
  in.info.view_rows = 182;
  in.info.view_cols = 1440;
  in.info.sun_altitude_deg = 20.0;
  in.info.sun_azimuth_deg = 0.0;
  in.info.shape_is_nominal = 0;
  in.have_quantity = true;
  in.quantity.kind = LUMICE_PATH_QUANTITY_BAND_SUM;
  in.quantity.aggregation = LUMICE_PATH_AGGREGATION_PHYSICAL_CLASS;
  in.quantity.unit = LUMICE_PATH_UNIT_PER_SR;
  in.have_evidence = true;
  static const char kBasis[] = "nominal crystal shape at the distribution centres";
  in.evidence.geometry_basis = LUMICE_PathText{ kBasis, sizeof(kBasis) - 1 };
  in.completed = true;

  // The tiers: DEFAULT = the short quantity name with its unit and the scene-vs-nominal limit;
  // PHYSICS = the full definition, the sun premise and the engine's basis verbatim; CALCULATION
  // = the grid and execution numbers. The lambda is in NO line — the slider row's echo is its
  // one owner.
  const std::vector<std::string> default_lines = MapCaliberDefaultLines(in);
  ASSERT_GE(default_lines.size(), 2u);
  EXPECT_NE(default_lines[0].find("band-sum"), std::string::npos);
  EXPECT_NE(default_lines[0].find("per sr"), std::string::npos);
  EXPECT_EQ(default_lines[0].find("physical class"), std::string::npos) << "the aggregation is a Physics fact";
  EXPECT_NE(default_lines[1].find("Scene geometry"), std::string::npos)
      << "a non-nominal shape names the limit at the default tier";

  const std::vector<std::string> physics = MapCaliberPhysicsLines(in);
  ASSERT_GE(physics.size(), 4u);
  EXPECT_NE(physics[0].find("band-sum"), std::string::npos);
  EXPECT_NE(physics[0].find("physical class"), std::string::npos);
  EXPECT_NE(physics[0].find("per sr"), std::string::npos);
  EXPECT_NE(physics[1].find("Sun at altitude 20.0"), std::string::npos);
  EXPECT_NE(physics[2].find("Scene geometry"), std::string::npos);
  EXPECT_EQ(physics[3], kBasis);  // the engine's geometry string, byte for byte

  const std::vector<std::string> calc = MapCaliberCalculationLines(in);
  ASSERT_GE(calc.size(), 3u);
  for (const std::string& l : calc) {
    EXPECT_EQ(l.find("550.0 nm"), std::string::npos) << "the lambda echo is deleted, not moved";
  }
  EXPECT_NE(calc[0].find("n=500000"), std::string::npos);
  EXPECT_NE(calc[0].find("[3.5, 48.5]"), std::string::npos);
  EXPECT_NE(calc[0].find("[-100.0, 260.0]"), std::string::npos);
  EXPECT_NE(calc[0].find("182\u00d71440"), std::string::npos) << "grid dims echo the Info's own";
  EXPECT_NE(calc[1].find("9/12 members"), std::string::npos);
  EXPECT_NE(calc[1].find("12345 events"), std::string::npos);
  EXPECT_NE(calc[2].find("Source-fixed scale"), std::string::npos);

  // A nominal shape says so (its limit rides Physics); a full-sky view names the all-sky grid.
  in.info.shape_is_nominal = 1;
  in.info.view_full_sky = 1;
  in.info.view_rows = 180;
  in.info.view_cols = 360;
  const std::vector<std::string> nominal_default = MapCaliberDefaultLines(in);
  for (const std::string& l : nominal_default) {
    EXPECT_EQ(l.find("Scene geometry"), std::string::npos) << "nominal needs no default limit";
  }
  const std::vector<std::string> nominal_physics = MapCaliberPhysicsLines(in);
  EXPECT_NE(nominal_physics[2].find("Nominal geometry"), std::string::npos);
  const std::vector<std::string> full = MapCaliberCalculationLines(in);
  EXPECT_NE(full[0].find("full sky 180\u00d7360"), std::string::npos);
}

TEST(AnalyzeMapView, CaliberLinesSpellTruncationFiniteZeroAndRank0) {
  MapCaliberInput in;
  in.info.struct_size = sizeof(in.info);
  in.info.wavelength_nm = 550.0;
  in.info.member_count = 12;
  in.info.members_evaluated = 5;
  in.info.sample_count = 500000;
  in.info.kept_count = 42;
  in.info.truncated = 1;
  in.completed = true;
  std::vector<std::string> lines = MapCaliberDefaultLines(in);
  ASSERT_GE(lines.size(), 1u);
  // Truncation is named and counts its prefix — never passed off as the whole class.
  bool truncated_line = false;
  for (const std::string& l : lines) {
    truncated_line = truncated_line || l.find("completed prefix of 5 of 12") != std::string::npos;
  }
  EXPECT_TRUE(truncated_line);
  // A finite zero needs a completed, untruncated zero-kept map: not this one.
  bool zero_line = false;
  for (const std::string& l : lines) {
    zero_line = zero_line || l.find("finite zero") != std::string::npos;
  }
  EXPECT_FALSE(zero_line);

  in.info.truncated = 0;
  in.info.kept_count = 0;
  lines = MapCaliberDefaultLines(in);
  zero_line = false;
  for (const std::string& l : lines) {
    zero_line = zero_line || l.find("finite zero") != std::string::npos;
  }
  EXPECT_TRUE(zero_line);
  // The same document before its intent is answered draws no finite-zero claim.
  in.completed = false;
  lines = MapCaliberDefaultLines(in);
  zero_line = false;
  for (const std::string& l : lines) {
    zero_line = zero_line || l.find("finite zero") != std::string::npos;
  }
  EXPECT_FALSE(zero_line);
  in.completed = true;

  // The rank-0 arm: the mass, its error, the method names and the zero-cells honesty, with the
  // out-of-view case named instead of hidden.
  in.have_mass = true;
  in.mass.m = 3.14159;
  in.mass.m_error = 0.012;
  in.mass.pixel = 7;
  static const char* kMethods[] = { "psi_average", "lattice_mean" };
  in.methods[0] = kMethods[0];
  in.methods[1] = kMethods[1];
  lines = MapCaliberDefaultLines(in);
  bool mass_line = false;
  for (const std::string& l : lines) {
    mass_line = mass_line ||
                (l.find("Rank-0 arm") != std::string::npos && l.find("3.14159") != std::string::npos &&
                 l.find("0.012") != std::string::npos && l.find("psi_average, lattice_mean") != std::string::npos &&
                 l.find("not no-light") != std::string::npos);
  }
  EXPECT_TRUE(mass_line);
  in.mass.pixel = -1;
  lines = MapCaliberDefaultLines(in);
  bool outside_line = false;
  for (const std::string& l : lines) {
    outside_line =
        outside_line || (l.find("Rank-0 arm") != std::string::npos && l.find("outside this view") != std::string::npos);
  }
  EXPECT_TRUE(outside_line);
}

// ---- The session-state rules -----------------------------------------------------------------------

TEST(AnalyzeMapView, MapSessionStateTruthTable) {
  // The state rules read the session's flags, never a handle's contents — the adopted-map
  // double below is a non-null pointer with a no-op deleter.
  const auto map_double = [] {
    return MapHandle(reinterpret_cast<const LUMICE_PathMap*>(0x1), [](const LUMICE_PathMap*) {});
  };
  MapQuerySession map;
  EXPECT_EQ(MapQueryStateFor(map), MapQueryState::kNoSelection);
  map.intent_valid = true;
  EXPECT_EQ(MapQueryStateFor(map), MapQueryState::kWaiting);
  map.cancelled_for_key = true;
  EXPECT_EQ(MapQueryStateFor(map), MapQueryState::kCancelled);
  map.cancelled_for_key = false;
  map.adopted_error = "multi_layer_unsupported: it says so";
  EXPECT_EQ(MapQueryStateFor(map), MapQueryState::kRefused);
  map.adopted_error.clear();
  map.adopted_result = map_double();
  EXPECT_EQ(MapQueryStateFor(map), MapQueryState::kCompleted);

  // Supersession: an adopted map stands while a NEWER intent is unanswered — never under a cancelled
  // intent, never with the intent answered, never without an adopted map at all.
  map.intent_id = SurveyQueryId{ 3, 1 };
  map.adopted_id = SurveyQueryId{ 3, 1 };
  EXPECT_FALSE(MapSupersededFor(map));
  map.intent_id = SurveyQueryId{ 3, 2 };
  EXPECT_TRUE(MapSupersededFor(map));
  map.cancelled_for_key = true;
  EXPECT_FALSE(MapSupersededFor(map));
  map.cancelled_for_key = false;
  map.adopted_result.reset();
  EXPECT_FALSE(MapSupersededFor(map));
}

// ---- The pump, over a real executor and a real scene -----------------------------------------------

namespace {

constexpr const char* kMapSceneJson = R"({
  "crystal": [ { "id": 1, "type": "prism",
                 "shape": { "height": 0.3, "face_distance": [1,1,1,1,1,1] },
                 "axis": { "zenith": { "type": "gauss", "mean": 0, "std": 0.3 },
                           "azimuth": { "type": "uniform", "mean": 0, "std": 360 },
                           "roll": { "type": "uniform", "mean": 0, "std": 360 } } } ],
  "filter": [],
  "scene": { "light_source": { "type": "sun", "altitude": 20, "azimuth": 0, "diameter": 0.5, "spectrum": "D65" },
             "ray_num": 1000, "max_hits": 7,
             "scattering": [ { "prob": 0, "entries": [ { "crystal": 1, "proportion": 1 } ] } ] },
  "render": []
})";

std::shared_ptr<const AnalysisPayload> Rows(unsigned long long generation, const std::vector<std::string>& displays,
                                            const std::vector<std::vector<int>>& face_sets) {
  auto payload = std::make_shared<AnalysisPayload>();
  payload->snapshot_generation = generation;
  payload->entries.reserve(displays.size());
  for (size_t i = 0; i < displays.size(); ++i) {
    LUMICE_RaypathHistogramEntry row;
    std::memset(&row, 0, sizeof(row));
    row.chain[0].crystal_id = 1;
    const auto& faces = face_sets[i];
    for (size_t f = 0; f < faces.size() && f < LUMICE_MAX_RAYPATH_SEGMENT_LEN; ++f) {
      row.chain[0].segment[f] = faces[f];
    }
    row.chain[0].segment_len = static_cast<int>(faces.size());
    row.chain_len = 1;
    std::snprintf(row.display, sizeof(row.display), "%s", displays[i].c_str());
    payload->entries.push_back(row);
  }
  return payload;
}

// SetScene builds the real workspace independently of the survey face. These cases consume
// only maps; advancing the frame clock still exercises the real settle gate. Keep the clock
// across waits and stop at the observed adoption, without an extra pump changing its identity.
template <typename Pred>
bool PumpMapUntil(GuiState& state, SurveyExecutor& executor, double& now, Pred pred, int timeout_ms) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (!pred()) {
    if (std::chrono::steady_clock::now() > deadline) {
      return false;
    }
    PumpMapQuery(state, executor, now);
    if (!state.analysis.map.adopted_error.empty()) {
      ADD_FAILURE() << state.analysis.map.adopted_error;
      return false;
    }
    now += 0.05;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  return true;
}

// The request's fallback anchor must describe the same frozen scene as the executor, not
// GuiState's unrelated live-document defaults (nor a survey computed just to obtain its sun).
void FreezeMapSun(GuiState& state) {
  state.analysis.frozen_sun_valid = true;
  state.analysis.frozen_sun_altitude = 20.0f;
}

void ExpectAdoptedMap(const GuiState& state, const std::vector<int>& faces, bool full_sky) {
  const MapQuerySession& map = state.analysis.map;
  EXPECT_FALSE(state.analysis.survey.intent_valid);
  ASSERT_NE(map.adopted_result, nullptr) << map.adopted_error;
  EXPECT_TRUE(map.adopted_error.empty());
  EXPECT_EQ(map.adopted_id, map.intent_id);
  EXPECT_EQ(map.adopted_id.generation, 7u);
  EXPECT_EQ(map.key.selection.crystal_id, 1);
  EXPECT_EQ(map.key.selection.faces, faces);
  EXPECT_EQ(map.key.selection.layer_face_counts, (std::vector<int>{ static_cast<int>(faces.size()) }));
  LUMICE_PathMapInfo info{};
  info.struct_size = sizeof(info);
  ASSERT_EQ(LUMICE_PathMapGetInfo(map.adopted_result.get(), &info), LUMICE_PATH_READ_OK);
  EXPECT_EQ(info.execution, LUMICE_PATH_OUTCOME_COMPLETED);
  EXPECT_EQ(info.state, LUMICE_PATH_STATE_FINITE);
  EXPECT_EQ(info.crystal_id, 1);
  ASSERT_NE(info.faces, nullptr);
  EXPECT_EQ(std::vector<int>(info.faces, info.faces + info.face_count), faces);
  EXPECT_DOUBLE_EQ(info.sun_altitude_deg, 20.0);
  EXPECT_DOUBLE_EQ(info.sun_azimuth_deg, 0.0);
  EXPECT_EQ(info.sample_count, kMapQuerySampleCount);
  EXPECT_GT(info.member_count, 0);
  EXPECT_EQ(info.members_evaluated, info.member_count);
  EXPECT_GT(info.kept_count, 0);
  EXPECT_EQ(info.truncated, 0);
  EXPECT_EQ(info.view_full_sky, full_sky ? 1 : 0);
  LUMICE_PathMapPixels pixels{};
  pixels.struct_size = sizeof(pixels);
  ASSERT_EQ(LUMICE_PathMapGetPixels(map.adopted_result.get(), &pixels), LUMICE_PATH_READ_OK);
  ASSERT_GT(pixels.pixel_count, 0u);
  EXPECT_EQ(pixels.pixel_count, info.pixel_count);
  ASSERT_NE(pixels.values, nullptr);
  ASSERT_NE(pixels.status, nullptr);
  // A valid zero-valued window is possible; the full-sky fit fixture must actually carry light.
  if (full_sky) {
    bool positive = false;
    for (size_t i = 0; i < pixels.pixel_count; ++i) {
      positive = positive || (std::isfinite(pixels.values[i]) && pixels.values[i] > 0.0);
    }
    EXPECT_TRUE(positive);
  }
}

}  // namespace

TEST(AnalyzeMapView, ObservationNeverIssuesAndRetryRepeatsFullSky) {
  LUMICE_Scene* raw_scene = nullptr;
  ASSERT_EQ(LUMICE_SceneFromJson(kMapSceneJson, &raw_scene), LUMICE_OK);
  SurveyExecutor executor;
  executor.SetScene(std::unique_ptr<LUMICE_Scene, SceneDeleter>(raw_scene));
  GuiState state;
  state.analysis_result.payload = Rows(7, { "1-2" }, { { 1, 2 } });
  state.analysis_result.analyzed_scene = GuiState::AnalysisSceneIdentity::From(state);
  FreezeMapSun(state);
  state.analysis.selected_entry = "1-2";
  double now = 100.0;
  ASSERT_TRUE(PumpMapUntil(state, executor, now, [&] { return state.analysis.map.adopted_result != nullptr; }, 30000));
  ASSERT_NO_FATAL_FAILURE(ExpectAdoptedMap(state, { 1, 2 }, true));
  const auto full = state.analysis.map.adopted_result;
  const auto full_id = state.analysis.map.intent_id;
  const auto renderer = state.renderer;
  for (int i = 0; i < 20; ++i) {
    state.renderer.fov = 1.0f + i;
    state.renderer.azimuth = i * 17.0f;
    PumpMapQuery(state, executor, now + 1.0 + i);
  }
  EXPECT_EQ(state.analysis.map.intent_id, full_id);
  EXPECT_EQ(state.analysis.map.adopted_result, full);
  EXPECT_FALSE(executor.QueryOutstanding(QueryFace::kMap));
  EXPECT_FALSE(MapSupersededFor(state.analysis.map));
  // The first adoption itself never changed the actual observation camera.
  EXPECT_EQ(renderer.fov, GuiState{}.renderer.fov);
  RetryMap(state, executor, now);
  EXPECT_EQ(state.analysis.map.intent_id, full_id);  // idle Retry is inert
  CancelMap(state, executor);
  for (int i = 0; i < 20; ++i)
    PumpMapQuery(state, executor, now + i);
  EXPECT_TRUE(state.analysis.map.cancelled_for_key);
  EXPECT_EQ(state.analysis.map.intent_id, full_id);
  EXPECT_EQ(state.analysis.map.adopted_result, full);
  RetryMap(state, executor, now);
  const auto retry_id = state.analysis.map.intent_id;
  EXPECT_NE(retry_id, full_id);
  EXPECT_FALSE(state.analysis.map.cancelled_for_key);
  state.renderer.fov = 80.0f;
  state.renderer.azimuth = 97.0f;
  ASSERT_TRUE(PumpMapUntil(state, executor, now, [&] { return state.analysis.map.adopted_id == retry_id; }, 30000));
  ASSERT_NO_FATAL_FAILURE(ExpectAdoptedMap(state, { 1, 2 }, true));
  EXPECT_FLOAT_EQ(state.renderer.fov, 80.0f);
  EXPECT_FLOAT_EQ(state.renderer.azimuth, 97.0f);
  EXPECT_FALSE(MapSupersededFor(state.analysis.map));
  executor.ShutdownAndJoin();
}

TEST(AnalyzeMapView, DeterministicLateGateAndMailboxPeak) {
  struct Gate {
    std::mutex mu;
    std::condition_variable cv;
    int entered = 0, allowed = 0;
    void Pickup(QueryFace face) {
      if (face != QueryFace::kMap)
        return;
      std::unique_lock<std::mutex> lock(mu);
      const int phase = ++entered;
      cv.notify_all();
      cv.wait(lock, [&] { return allowed >= phase; });
    }
    bool Wait(int phase) {
      std::unique_lock<std::mutex> lock(mu);
      return cv.wait_for(lock, std::chrono::seconds(15), [&] { return entered >= phase; });
    }
    bool At(int phase) {
      std::lock_guard<std::mutex> lock(mu);
      return entered >= phase;
    }
    void Release(int phase) {
      std::lock_guard<std::mutex> lock(mu);
      allowed = phase;
      cv.notify_all();
    }
  } gate;
  struct Ledger {
    std::mutex mu;
    std::vector<std::weak_ptr<const LUMICE_PathMap>> handles;
    size_t peak = 0;
    void Observe(const SurveyExecutor::Finished& result) {
      if (!result.map_result)
        return;
      std::lock_guard<std::mutex> lock(mu);
      handles.push_back(result.map_result);
      const size_t live = static_cast<size_t>(
          std::count_if(handles.begin(), handles.end(), [](const auto& h) { return !h.expired(); }));
      peak = std::max(peak, live);
    }
    size_t Peak() {
      std::lock_guard<std::mutex> lock(mu);
      return peak;
    }
  } ledger;
  SurveyExecutor executor;
  struct Teardown {
    Gate& gate;
    SurveyExecutor& executor;
    ~Teardown() {
      gate.Release(1 << 30);
      executor.SetPickupGateForTest(nullptr);
      executor.SetCompletionObserverForTest(nullptr);
    }
  } teardown{ gate, executor };
  LUMICE_Scene* raw_scene = nullptr;
  ASSERT_EQ(LUMICE_SceneFromJson(kMapSceneJson, &raw_scene), LUMICE_OK);
  executor.SetScene(std::unique_ptr<LUMICE_Scene, SceneDeleter>(raw_scene));
  executor.SetPickupGateForTest([&](QueryFace face) { gate.Pickup(face); });
  executor.SetCompletionObserverForTest([&](const auto& result) { ledger.Observe(result); });
  GuiState state;
  state.analysis_result.payload = Rows(7, { "1-2" }, { { 1, 2 } });
  state.analysis_result.analyzed_scene = GuiState::AnalysisSceneIdentity::From(state);
  state.analysis.selected_entry = "1-2";
  FreezeMapSun(state);
  double now = 100.0;
  PumpMapQuery(state, executor, now);
  ASSERT_TRUE(gate.Wait(1));
  const auto old = state.analysis.map.intent_id;
  state.analysis.map_wavelength_nm = 650.0;
  PumpMapQuery(state, executor, now);
  const auto current = state.analysis.map.intent_id;
  ASSERT_NE(old, current);
  gate.Release(1);
  ASSERT_TRUE(PumpMapUntil(state, executor, now, [&] { return gate.At(2); }, 15000));
  EXPECT_EQ(state.analysis.map.adopted_result, nullptr);
  EXPECT_TRUE(state.analysis.map.adopted_error.empty());
  gate.Release(2);
  ASSERT_TRUE(PumpMapUntil(state, executor, now, [&] { return state.analysis.map.adopted_id == current; }, 15000));
  const auto* reference = state.analysis.map.adopted_result.get();
  ASSERT_NE(reference, nullptr);
  LUMICE_PathMapInfo info{};
  info.struct_size = sizeof(info);
  ASSERT_EQ(LUMICE_PathMapGetInfo(reference, &info), LUMICE_PATH_READ_OK);
  EXPECT_DOUBLE_EQ(info.wavelength_nm, 650.0);
  EXPECT_LE(ledger.Peak(), 2u);  // one adopted source plus one completion
  RecordProperty("map_peak_live_result_handles", static_cast<int>(ledger.Peak()));
  executor.ShutdownAndJoin();
}

TEST(AnalyzeMapView, SelectionChangeKillsTheWholeMapSession) {
  LUMICE_Scene* raw_scene = nullptr;
  ASSERT_EQ(LUMICE_SceneFromJson(kMapSceneJson, &raw_scene), LUMICE_OK);
  std::unique_ptr<LUMICE_Scene, SceneDeleter> scene(raw_scene);
  SurveyExecutor executor;
  executor.SetScene(std::move(scene));

  GuiState state;
  state.analysis_result.payload = Rows(7, { "1-2", "1-3-2" }, { { 1, 2 }, { 1, 3, 2 } });
  state.analysis_result.analyzed_scene = GuiState::AnalysisSceneIdentity::From(state);
  FreezeMapSun(state);
  state.analysis.selected_entry = "1-2";
  double now = 100.0;
  ASSERT_TRUE(PumpMapUntil(state, executor, now, [&] { return state.analysis.map.adopted_result != nullptr; }, 30000));
  ASSERT_NO_FATAL_FAILURE(ExpectAdoptedMap(state, { 1, 2 }, true));
  const SurveyQueryId old_id = state.analysis.map.adopted_id;

  // Another class: the adopted map is gone with the selection's session — no lingering picture.
  // Auto-Fit just changed the view: the same clock is still inside its settle window.
  state.analysis.selected_entry = "1-3-2";
  PumpMapQuery(state, executor, now);
  EXPECT_FALSE(state.analysis.map.adopted_result != nullptr);
  EXPECT_TRUE(state.analysis.map.intent_valid);
  ASSERT_TRUE(PumpMapUntil(state, executor, now, [&] { return state.analysis.map.adopted_result != nullptr; }, 30000));
  ASSERT_NO_FATAL_FAILURE(ExpectAdoptedMap(state, { 1, 3, 2 }, true));
  EXPECT_NE(state.analysis.map.adopted_id, old_id);
  executor.ShutdownAndJoin();
}

TEST(AnalyzeMapView, IdentityChangesClearTheSourceAndCancellationNeverAutoRetries) {
  SurveyExecutor executor;
  GuiState state;
  state.analysis_result.payload = Rows(7, { "1-2" }, { { 1, 2 } });
  state.analysis_result.analyzed_scene = GuiState::AnalysisSceneIdentity::From(state);
  state.analysis.selected_entry = "1-2";
  PumpMapQuery(state, executor, 100.0);
  const auto old = state.analysis.map.intent_id;
  CancelMap(state, executor);
  PumpMapQuery(state, executor, 1000.0);
  EXPECT_EQ(state.analysis.map.intent_id, old);
  EXPECT_TRUE(state.analysis.map.cancelled_for_key);
  RetryMap(state, executor, 1001.0);
  EXPECT_NE(state.analysis.map.intent_id, old);
  EXPECT_FALSE(state.analysis.map.cancelled_for_key);
  const auto retry = state.analysis.map.intent_id;
  state.analysis.map_wavelength_nm = 650.0;
  PumpMapQuery(state, executor, 1002.0);
  EXPECT_NE(state.analysis.map.intent_id, retry);
  EXPECT_EQ(state.analysis.map.key.wavelength_nm, 650.0);
  EXPECT_EQ(state.analysis.map.adopted_result, nullptr);
  executor.ShutdownAndJoin();
}

// The window geometry's sun anchor of last resort is the FROZEN scene's sun, never the live
// document's: the engine evaluates the request against the binding it was issued for, and an
// edited-but-not-re-analyzed sun must not steer the window (r2 Major).
TEST(AnalyzeMapView, SunAnchorFallsBackToTheFrozenSunNotTheLiveOne) {
  GuiState state;
  state.sun.altitude = 10.0f;  // the live document: edited since
  state.analysis.frozen_sun_valid = true;
  state.analysis.frozen_sun_altitude = 35.0f;
  SkySunAnchor anchor;
  ASSERT_TRUE(SkySunAnchorFor(state, &anchor));
  EXPECT_NEAR(anchor.altitude_deg, 35.0, 1e-6);
  EXPECT_NEAR(anchor.azimuth_deg, 0.0, 1e-6);
  // Without the capture (a session from before the field existed), the live sun remains the
  // honest fallback.
  state.analysis.frozen_sun_valid = false;
  ASSERT_TRUE(SkySunAnchorFor(state, &anchor));
  EXPECT_NEAR(anchor.altitude_deg, 10.0, 1e-6);
}

// The click-semantics isolation (AC3), at the state level: the map pump and a map pick write
// the map's own session and the sky-point intent — never the panorama's ROI state. The
// gesture-level pin (a real click changes nothing on the panorama) lives in gui_test.
TEST(AnalyzeMapView, MapFaceNeverTouchesTheRoiState) {
  LUMICE_Scene* raw_scene = nullptr;
  ASSERT_EQ(LUMICE_SceneFromJson(kMapSceneJson, &raw_scene), LUMICE_OK);
  std::unique_ptr<LUMICE_Scene, SceneDeleter> scene(raw_scene);
  SurveyExecutor executor;
  executor.SetScene(std::move(scene));

  GuiState state;
  state.analysis_result.payload = Rows(7, { "1-2" }, { { 1, 2 } });
  state.analysis_result.analyzed_scene = GuiState::AnalysisSceneIdentity::From(state);
  FreezeMapSun(state);
  state.analysis.selected_entry = "1-2";
  state.analysis.roi_mode = LUMICE_RAYPATH_ROI_CONE;
  state.analysis.cone_center_valid = true;
  state.analysis.cone_center_dir[0] = 0.6f;
  state.analysis.cone_center_dir[1] = 0.0f;
  state.analysis.cone_center_dir[2] = 0.8f;

  double now = 100.0;
  ASSERT_TRUE(PumpMapUntil(state, executor, now, [&] { return state.analysis.map.adopted_result != nullptr; }, 30000));
  ASSERT_NO_FATAL_FAILURE(ExpectAdoptedMap(state, { 1, 2 }, true));
  // A map pick (the renderer's write, spelled here): the intent moves, the ROI state does not.
  double pos[3];
  SkyPositionOf(23.0, 37.0, pos);
  state.analysis.sky_point.valid = true;
  state.analysis.sky_point.from_map_pick = true;
  state.analysis.sky_point.dir[0] = static_cast<float>(-pos[0]);
  state.analysis.sky_point.dir[1] = static_cast<float>(-pos[1]);
  state.analysis.sky_point.dir[2] = static_cast<float>(-pos[2]);

  EXPECT_EQ(state.analysis.roi_mode, LUMICE_RAYPATH_ROI_CONE);
  EXPECT_TRUE(state.analysis.cone_center_valid);
  EXPECT_FLOAT_EQ(state.analysis.cone_center_dir[0], 0.6f);
  EXPECT_FLOAT_EQ(state.analysis.cone_center_dir[1], 0.0f);
  EXPECT_FLOAT_EQ(state.analysis.cone_center_dir[2], 0.8f);
  executor.ShutdownAndJoin();
}

// The pick's label roundtrip: the travel direction the pick writes reads back as the sky label
// the user clicked — the §12 negation at the pick boundary, held at both ends.
TEST(AnalyzeMapView, SkyPointIntentRoundtripsThroughTheLabel) {
  const double kLabels[][2] = { { 20, 0 }, { 45, 90 }, { -30, 200 }, { 0, 180 }, { 75, 315 } };
  for (const auto& label : kLabels) {
    float dir[3];
    AltAzToDir(static_cast<float>(label[0]), static_cast<float>(label[1]), dir);
    float alt = 0.0f;
    float az = 0.0f;
    DirToAltAz(dir, &alt, &az);
    EXPECT_NEAR(alt, label[0], 1e-4) << "alt of (" << label[0] << "," << label[1] << ")";
    double gap = std::fabs(az - label[1]);
    while (gap > 180.0) {
      gap = 360.0 - gap;
    }
    EXPECT_NEAR(gap, 0.0, 1e-4) << "az of (" << label[0] << "," << label[1] << ")";
  }
}

// The wait line's three texts: the queue is the truth the wait
// names — behind the survey the prediction says so, the steered view asks for its own answer —
// and the direct branch at the interaction tier. All three carry the live elapsed suffix.
TEST(AnalyzeMapView, MapWaitLineNamesTheQueueAndCarriesTheElapsed) {
  MapQuerySession map;
  map.intent_valid = true;
  map.intent_start_s = 5.0;
  const double now = 6.5;
  const std::string elapsed = " (1.5 s)";

  const std::string queued_default = MapWaitLineFor(map, /*survey_outstanding=*/true, now);
  EXPECT_NE(queued_default.find("queues behind this class's survey"), std::string::npos);
  EXPECT_NE(queued_default.find("the all-sky map"), std::string::npos);
  EXPECT_EQ(queued_default.substr(queued_default.size() - elapsed.size()), elapsed);

  const std::string direct = MapWaitLineFor(map, /*survey_outstanding=*/false, now);
  EXPECT_NE(direct.find("camera movement does not recalculate"), std::string::npos);
  EXPECT_EQ(direct.substr(direct.size() - elapsed.size()), elapsed);

  // A later frame-time advances the number; a restarted epoch (the rekey rule) rolls it back.
  EXPECT_NE(MapWaitLineFor(map, true, now + 1.0).find(" (2.5 s)"), std::string::npos);
  map.intent_start_s = 8.0;
  EXPECT_NE(MapWaitLineFor(map, true, now).find(" (0.0 s)"), std::string::npos);
}

// The stage clause on the map line (751): only the MAP face's own computing names a stage —
// while the survey ahead runs, the queue text stands alone (another face's stage must not
// hang on this line); the direct branch carries the engine's last-known phase.
TEST(AnalyzeMapView, MapWaitLineCarriesTheStageOnlyForItsOwnFace) {
  MapQuerySession map;
  map.intent_valid = true;
  map.intent_start_s = 5.0;
  const double now = 6.5;

  InflightPoll survey_running;  // the queue's truth: the SURVEY face is the one computing
  survey_running.in_flight = true;
  survey_running.face = QueryFace::kSurvey;
  survey_running.stage = LUMICE_PATH_POLL_STAGE_CHROMATIC_LEG;
  survey_running.ticks = 3;
  const std::string queued = MapWaitLineFor(map, true, now, survey_running);
  EXPECT_NE(queued.find("queues behind this class's survey"), std::string::npos);
  EXPECT_EQ(queued.find("running the chromatic pair"), std::string::npos)
      << "another face's stage must not hang on the map's queue line";

  InflightPoll map_running;
  map_running.in_flight = true;
  map_running.face = QueryFace::kMap;
  map_running.stage = LUMICE_PATH_POLL_STAGE_MAP_MEMBERS;
  map_running.ticks = 9;
  const std::string direct = MapWaitLineFor(map, /*survey_outstanding=*/false, now, map_running);
  EXPECT_NE(direct.find(" — summing the map members (1.5 s)"), std::string::npos);
}

// The supersession note carries the stage clause too (751, same mechanism): while the Retry's
// map query itself is computing, its last-known phase rides the note.
TEST(AnalyzeMapView, MapSupersedingNoteCarriesTheStageWhileTheMapComputes) {
  MapQuerySession map;
  map.intent_valid = true;
  map.intent_start_s = 3.0;
  InflightPoll poll;
  poll.in_flight = true;
  poll.face = QueryFace::kMap;
  poll.stage = LUMICE_PATH_POLL_STAGE_MAP_MEMBERS;
  const std::string note = MapSupersedingNoteFor(map, 4.0, poll);
  EXPECT_NE(note.find("retrying the full-sky request"), std::string::npos);
  EXPECT_NE(note.find("summing the map members"), std::string::npos);
  EXPECT_NE(note.find("(1.0 s)"), std::string::npos);
}

// The supersession note carries its own elapsed — the Retry intent's wait, the same epoch the
// pump stamps on every re-ask.
TEST(AnalyzeMapView, MapSupersedingNoteCarriesTheElapsed) {
  MapQuerySession map;
  map.intent_valid = true;
  map.intent_start_s = 3.0;
  const std::string note = MapSupersedingNoteFor(map, 4.0);
  EXPECT_NE(note.find("Frozen map remains"), std::string::npos);
  EXPECT_NE(note.find("(1.0 s)"), std::string::npos);
}


// The overlay tooltip's grouping (the Step-4 presentation contract): identical keys fold into
// one entry with a count, anything differing on ANY observed field (kind, state, coordinates,
// reason) stays a separate entry, first-seen order holds, and nothing drops. The rows' shapes
// are the observed probes' (the fully-random prism's unpositioned rows never reach this layer;
// the plate scene's source image and the synthetic same-key rows do).
TEST(AnalyzeMapView, OverlayHitGroupsFoldIdenticalKeysAndKeepDistinctSources) {
  SurveyObjectRow a;
  a.kind_name = "source image";
  a.state = LUMICE_PATH_STATE_CERTIFIED;
  a.state_name = "certified";
  a.position_available = true;
  a.position_altitude_deg = 20.0;
  a.position_azimuth_deg = 0.0;
  SurveyObjectRow a_again = a;  // same key on every field: folds
  SurveyObjectRow b = a;
  b.kind_name = "support set";  // different kind at the same position: separate
  SurveyObjectRow c = a;
  c.reason = "scope_value_mismatch";  // different reason at the same position: separate
  SurveyObjectRow d = a;
  d.position_azimuth_deg = 0.5;  // different coordinates: separate
  SurveyObjectRow e = a;
  e.family_level = 2;  // different family evidence, same every drawn word: separate — the fold
                       // claims the SOURCES are identical, so every carried field counts
  SurveyObjectRow f = a;
  f.blur_deg = 1.25;            // different blur evidence, same every drawn word: separate
  SurveyObjectRow a_third = a;  // a third same-key row: the count follows

  const std::vector<const SurveyObjectRow*> hits = { &a, &a_again, &b, &c, &d, &e, &f, &a_third };
  std::vector<OverlayHitGroup> groups;
  GroupOverlayHits(hits, &groups);
  ASSERT_EQ(groups.size(), 6u);
  EXPECT_EQ(groups[0].representative, &a);
  EXPECT_EQ(groups[0].count, 3);
  EXPECT_EQ(groups[1].representative, &b);
  EXPECT_EQ(groups[1].count, 1);
  EXPECT_EQ(groups[2].representative, &c);
  EXPECT_EQ(groups[3].representative, &d);
  EXPECT_EQ(groups[4].representative, &e);
  EXPECT_EQ(groups[5].representative, &f);
  // And nothing dropped: the counts sum to the hits.
  int total = 0;
  for (const OverlayHitGroup& g : groups) {
    total += g.count;
  }
  EXPECT_EQ(total, 8);
}

// The marker vocabulary: a concluded claim (certified/finite) draws solid, the refused and
// unevaluated states draw outline only — the marker never presents an unevaluated row as
// answered — and distinct kinds take distinct shapes.
TEST(AnalyzeMapView, OverlayMarkerStyleSolidOnlyForConcludedClaims) {
  const OverlayMarkerStyle certified = OverlayMarkerStyleFor("source image", LUMICE_PATH_STATE_CERTIFIED);
  const OverlayMarkerStyle finite = OverlayMarkerStyleFor("support set", LUMICE_PATH_STATE_FINITE);
  const OverlayMarkerStyle refused = OverlayMarkerStyleFor("bright domain", LUMICE_PATH_STATE_REFUSED);
  const OverlayMarkerStyle unevaluated = OverlayMarkerStyleFor("caustic edge", LUMICE_PATH_STATE_UNEVALUATED);
  EXPECT_TRUE(certified.filled);
  EXPECT_TRUE(finite.filled);
  EXPECT_FALSE(refused.filled);
  EXPECT_FALSE(unevaluated.filled);
  EXPECT_NE(certified.shape, finite.shape);
  EXPECT_NE(certified.shape, refused.shape);
  // An unknown kind still gets the honest fallback marker, not a crash or the sun's look.
  const OverlayMarkerStyle fallback = OverlayMarkerStyleFor("some future kind", LUMICE_PATH_STATE_CERTIFIED);
  EXPECT_EQ(fallback.shape, OverlayMarkerShape::kFallbackDiamond);
}

// ---- The verdict anchor (contract §16.1's A2 pilot) --------------------------------------------------
//
// The anchor's draw-list assertions run on a headless frame: the context, its CPU-built default
// font atlas and one NewFrame are the whole rig — the background draw list records exactly what
// the canvas would draw, and the assertions read it through the SHARED palette constants (the
// colour buckets) and the vertices' extents, never a second projection. Where two rings must be
// told apart in one frame, they take the palette's two colours: the routing maps the diagnose's
// own colour field, so a blue diagnose and a red diagnose of the same geometry are the two
// buckets — the same trick that keeps the label's glyph vertices harmless (they carry their
// curve's colour; the x-extent reading below survives them because the label's patch sits inside
// the ring's own).

class MapDeviceDrawRig : public ::testing::Test {
 protected:
  void SetUp() override {
    ctx_ = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(600.0f, 600.0f);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();
    unsigned char* pixels = nullptr;
    int atlas_w = 0;
    int atlas_h = 0;
    io.Fonts->GetTexDataAsAlpha8(&pixels, &atlas_w, &atlas_h);  // builds the atlas, CPU-only
    ImGui::NewFrame();
  }
  void TearDown() override {
    if (ImGui::GetCurrentContext() != nullptr) {
      ImGui::EndFrame();
    }
    ImGui::DestroyContext(ctx_);
  }

  // The frame's camera: a 180° equidistant fisheye aimed at the sun (its own altitude and
  // azimuth), so the iso-delta circles land concentric about the canvas centre and their screen
  // radii scale linearly with the delta — the relation the ring-ratio test reads.
  static ViewProjection SunCamera() {
    ViewProjection vp;
    vp.lens_type = kLensTypeFisheyeEquidist;
    vp.fov = 180.0f;
    vp.elevation = 20.0f;
    vp.azimuth = 0.0f;
    vp.roll = 0.0f;
    vp.visible = kVisibleFull;
    vp.front = false;
    return vp;
  }

  static SurveyDiagnoseView Diagnose(const char* kind, const char* colour, double delta_deg, bool coverage) {
    SurveyDiagnoseView d;
    d.available = true;
    d.verdict_kind = kind;
    d.color = colour;
    d.visible = 1;
    d.position_available = true;
    d.position_deg = delta_deg;
    d.coverage_complete = coverage;
    return d;
  }

  static VerdictAnchorInput Input(const ViewProjection& vp, bool shown, bool hover) {
    VerdictAnchorInput in;
    in.camera = &vp;
    in.width_px = 600;
    in.height_px = 600;
    in.width_pt = 600.0f;
    in.height_pt = 600.0f;
    in.pic_min_x = 0.0f;
    in.pic_min_y = 0.0f;
    in.pic_px = 600.0f;
    in.sun.altitude_deg = 20.0;
    in.sun.azimuth_deg = 0.0;
    in.shown = shown;
    in.hover = hover;
    return in;
  }

  // The colour bucket's index count: every primitive whose six vertices carry the colour — the
  // anchor's line quads and its label's glyphs together (the backing rect's ink is a different
  // colour and stays out of every bucket).
  static int IndexCountForColour(ImU32 colour) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    int count = 0;
    for (int i = 0; i + 5 < dl->IdxBuffer.Size; i += 6) {
      bool matches = true;
      for (int k = 0; k < 6; ++k) {
        if (dl->VtxBuffer[static_cast<int>(dl->IdxBuffer[i + k])].col != colour) {
          matches = false;
          break;
        }
      }
      if (matches) {
        count += 6;
      }
    }
    return count;
  }

  // The colour bucket's vertices' x-extent: for a full ring that is its diameter along x (the
  // 96-sample walk puts vertices within a half-segment of both extremes, a sub-0.1% shave), and
  // the label's patch — centered on the ring's topmost point, halfwidth well inside the radius —
  // cannot inflate it. The vertical extents are deliberately not read: the label rides above
  // the ring's top, exactly where a y-extent would be polluted.
  static void XExtentForColour(ImU32 colour, float* min_x, float* max_x) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    *min_x = FLT_MAX;
    *max_x = -FLT_MAX;
    for (int i = 0; i < dl->VtxBuffer.Size; ++i) {
      const ImDrawVert& v = dl->VtxBuffer[i];
      if (v.col != colour) {
        continue;
      }
      *min_x = std::min(*min_x, v.pos.x);
      *max_x = std::max(*max_x, v.pos.x);
    }
  }

  // One colour's whole vertex bounding box, both axes — the readout label patch's own read: the
  // backing ink is unique in the frame, so its box IS the patch rect (text + backing together).
  static void RectForColour(ImU32 colour, float* min_x, float* min_y, float* max_x, float* max_y) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    *min_x = FLT_MAX;
    *min_y = FLT_MAX;
    *max_x = -FLT_MAX;
    *max_y = -FLT_MAX;
    for (int i = 0; i < dl->VtxBuffer.Size; ++i) {
      const ImDrawVert& v = dl->VtxBuffer[i];
      if (v.col != colour) {
        continue;
      }
      *min_x = std::min(*min_x, v.pos.x);
      *min_y = std::min(*min_y, v.pos.y);
      *max_x = std::max(*max_x, v.pos.x);
      *max_y = std::max(*max_y, v.pos.y);
    }
  }

  // The FONT-glyph vertices of one colour: the same bucket's vertices minus the geometry
  // primitives — AddLine/AddRect/AddCircle all stamp the atlas's shared white pixel, font glyphs
  // carry their own atlas coordinates. The label's text is the one glyph consumer of the anchor
  // colour, so this box IS the text rect (ring and glyphs share the bucket; the white-pixel
  // discriminator is what tells them apart).
  static void GlyphRectForColour(ImU32 colour, float* min_x, float* min_y, float* max_x, float* max_y) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImVec2 white = ImGui::GetIO().Fonts->TexUvWhitePixel;
    *min_x = FLT_MAX;
    *min_y = FLT_MAX;
    *max_x = -FLT_MAX;
    *max_y = -FLT_MAX;
    for (int i = 0; i < dl->VtxBuffer.Size; ++i) {
      const ImDrawVert& v = dl->VtxBuffer[i];
      if (v.col != colour) {
        continue;
      }
      if (std::fabs(v.uv.x - white.x) < 1e-3f && std::fabs(v.uv.y - white.y) < 1e-3f) {
        continue;  // a geometry primitive's white-pixel vertex, not a glyph
      }
      *min_x = std::min(*min_x, v.pos.x);
      *min_y = std::min(*min_y, v.pos.y);
      *max_x = std::max(*max_x, v.pos.x);
      *max_y = std::max(*max_y, v.pos.y);
    }
  }

  ImGuiContext* ctx_ = nullptr;
};

// The verdict-anchor fixture shares the rig wholesale.
class VerdictAnchorDraw : public MapDeviceDrawRig {};

// The honesty gate on a synthetic diagnose — the injection seam the SurveyDiagnoseView plain
// struct provides, and the only tier that can hold the drawn legs: the engine reaches a
// coverage-incomplete, position-carrying verdict only through a stop or a cost guard, shapes no
// deterministic fixture produces (contract §16.1.3 records the vocabulary and the honesty
// grammar these tests assert).
// Each absence leg draws nothing (bucket counts zero), the coverage-incomplete leg still draws
// (dashed — the drawn form's discriminability is the next test), and the legend's verdict bit
// follows what actually drew.
TEST_F(VerdictAnchorDraw, HonestyGateLegs) {
  const ViewProjection vp = SunCamera();
  const SurveyDiagnoseView pilot = Diagnose("edge", "blue", 40.0, true);
  const SurveyDiagnoseView not_available = [] {
    SurveyDiagnoseView d;
    d.available = false;  // the refused / not-run document: no diagnose rows anywhere
    return d;
  }();
  const SurveyDiagnoseView tint = Diagnose("tint", "blue", 40.0, true);  // outside the closed kind set
  SurveyDiagnoseView positionless = Diagnose("edge", "blue", 40.0, true);
  positionless.position_available = false;
  SurveyDiagnoseView self_invisible = Diagnose("edge", "blue", 40.0, true);
  self_invisible.visible = 0;
  SurveyDiagnoseView self_invisible_partial = Diagnose("edge", "blue", 40.0, false);
  self_invisible_partial.visible = 0;  // the precedence: absence wins over the dashed leg
  const SurveyDiagnoseView unpainted_colour = Diagnose("edge", "white", 40.0, true);

  struct Leg {
    const char* what;
    const SurveyDiagnoseView* d;
  };
  const Leg absent[] = { { "unavailable document", &not_available },
                         { "kind outside the closed set", &tint },
                         { "no spatial payload", &positionless },
                         { "verdict names itself not visible", &self_invisible },
                         { "not visible with incomplete coverage", &self_invisible_partial },
                         { "colour with no palette entry", &unpainted_colour } };
  for (const Leg& leg : absent) {
    OverlayLegendState legend;
    VerdictAnchorInput in = Input(vp, /*shown=*/true, /*hover=*/false);
    in.legend = &legend;
    DrawVerdictAnchor(ImGui::GetBackgroundDrawList(), *leg.d, in);
    EXPECT_EQ(IndexCountForColour(kChromaticColourBlue), 0) << leg.what;
    EXPECT_EQ(IndexCountForColour(kChromaticColourRed), 0) << leg.what;
    EXPECT_FALSE(legend.verdict_anchor) << leg.what;
  }
  // And the density gate: a fully anchorable verdict with neither switch standing draws nothing.
  OverlayLegendState legend;
  VerdictAnchorInput in = Input(vp, /*shown=*/false, /*hover=*/false);
  in.legend = &legend;
  DrawVerdictAnchor(ImGui::GetBackgroundDrawList(), pilot, in);
  EXPECT_EQ(IndexCountForColour(kChromaticColourBlue), 0);
  EXPECT_FALSE(legend.verdict_anchor);
}

// The density gate's two tiers: the hover alone anchors, the toggle alone anchors — and the
// closed kind set's second member (gate_edge) anchors through the same path. One frame, two
// colour buckets: the hover tier draws blue, the toggle tier's gate_edge red. The legend bit is
// pinned here too (the drawn → bit-set coupling, unit-tier, not only through the functional
// chain).
TEST_F(VerdictAnchorDraw, HoverAndToggleEachAnchor) {
  const ViewProjection vp = SunCamera();
  OverlayLegendState legend;
  VerdictAnchorInput in = Input(vp, /*shown=*/false, /*hover=*/true);
  in.legend = &legend;
  DrawVerdictAnchor(ImGui::GetBackgroundDrawList(), Diagnose("edge", "blue", 40.0, true), in);
  EXPECT_GT(IndexCountForColour(kChromaticColourBlue), 0);
  EXPECT_TRUE(legend.verdict_anchor);
  in = Input(vp, /*shown=*/true, /*hover=*/false);
  DrawVerdictAnchor(ImGui::GetBackgroundDrawList(), Diagnose("gate_edge", "red", 60.0, true), in);
  EXPECT_GT(IndexCountForColour(kChromaticColourRed), 0);
}

// The switch standing but the ring wholly outside the frustum: nothing draws and no legend bit
// lights — the density gate's complement on the drawing side. What this rig CANNOT reach is the
// intermediate regime the lines_submitted gate also covers (samples in view but zero submitted
// pairs: a dashed arc wholly inside its off-phase windows, or the measure-zero tangent ring
// whose single in-view sample forms no pair) — that regime has no robust fixture in this
// projection, and the gate it protects reads the one "actually drew" counter so a future caller
// cannot pick a walked-but-not-drawn carrier.
TEST_F(VerdictAnchorDraw, OutOfViewRingDrawsNothingAndEarnsNoLegend) {
  const ViewProjection vp = SunCamera();
  OverlayLegendState legend;
  VerdictAnchorInput in = Input(vp, /*shown=*/true, /*hover=*/false);
  in.legend = &legend;
  // δ=150 about the sun on a 180° fisheye aimed at the sun: every sample sits beyond the
  // frustum's 90° reach, so the whole walk is out of view.
  DrawVerdictAnchor(ImGui::GetBackgroundDrawList(), Diagnose("edge", "blue", 150.0, true), in);
  EXPECT_EQ(IndexCountForColour(kChromaticColourBlue), 0);
  EXPECT_FALSE(legend.verdict_anchor);
}

// The colour routing reads the verdict's own colour through the shared palette: a red verdict's
// anchor lands in the red bucket and nowhere else.
TEST_F(VerdictAnchorDraw, ColourRoutingUsesTheSharedPalette) {
  const ViewProjection vp = SunCamera();
  VerdictAnchorInput in = Input(vp, /*shown=*/true, /*hover=*/false);
  DrawVerdictAnchor(ImGui::GetBackgroundDrawList(), Diagnose("edge", "red", 40.0, true), in);
  EXPECT_GT(IndexCountForColour(kChromaticColourRed), 0);
  EXPECT_EQ(IndexCountForColour(kChromaticColourBlue), 0);
}

// The ring geometry, with no second projection: two rings drawn about the same sun — deltas 40
// and 80 — must land at x-extents whose ratio is the deltas' own (the equidistant lens's linear
// radius law), inside one frame and two colour buckets.
TEST_F(VerdictAnchorDraw, RingsLandAtTheRatiosTheDeltasName) {
  const ViewProjection vp = SunCamera();
  VerdictAnchorInput in = Input(vp, /*shown=*/true, /*hover=*/false);
  ImDrawList* dl = ImGui::GetBackgroundDrawList();
  DrawVerdictAnchor(dl, Diagnose("edge", "blue", 40.0, true), in);
  DrawVerdictAnchor(dl, Diagnose("edge", "red", 80.0, true), in);
  EXPECT_GT(IndexCountForColour(kChromaticColourBlue), 0);
  EXPECT_GT(IndexCountForColour(kChromaticColourRed), 0);
  float blue_min_x = 0.0f;
  float blue_max_x = 0.0f;
  float red_min_x = 0.0f;
  float red_max_x = 0.0f;
  XExtentForColour(kChromaticColourBlue, &blue_min_x, &blue_max_x);
  XExtentForColour(kChromaticColourRed, &red_min_x, &red_max_x);
  ASSERT_GT(blue_max_x, blue_min_x);
  ASSERT_GT(red_max_x, red_min_x);
  const float blue_extent = blue_max_x - blue_min_x;
  const float red_extent = red_max_x - red_min_x;
  EXPECT_NEAR(red_extent / blue_extent, 2.0f, 0.02f) << blue_extent << " vs " << red_extent;
  // And one ring's own sanity: δ=40 on a 180° equidistant canvas of 600pt is ~133pt of radius —
  // not a degenerate dot, not off-canvas.
  EXPECT_GT(blue_extent, 100.0f);
  EXPECT_LT(blue_extent, 600.0f);
}

// The coverage leg's drawn form: the same delta in the two palette colours — solid (blue) and
// coverage-incomplete (red) — draws the same ring twice, the dashed one with strictly fewer
// submitted primitives. The label's glyphs ride each bucket equally, so the ratio band is wide.
TEST_F(VerdictAnchorDraw, IncompleteCoverageDrawsTheDashedForm) {
  const ViewProjection vp = SunCamera();
  VerdictAnchorInput in = Input(vp, /*shown=*/true, /*hover=*/false);
  ImDrawList* dl = ImGui::GetBackgroundDrawList();
  DrawVerdictAnchor(dl, Diagnose("edge", "blue", 40.0, /*coverage=*/true), in);
  DrawVerdictAnchor(dl, Diagnose("edge", "red", 40.0, /*coverage=*/false), in);
  const int solid = IndexCountForColour(kChromaticColourBlue);
  const int dashed = IndexCountForColour(kChromaticColourRed);
  EXPECT_GT(solid, 0);
  EXPECT_GT(dashed, 0);
  EXPECT_LT(dashed, solid);
  EXPECT_GT(dashed, solid / 3);
  // Same delta, same ring: the two forms' x-extents agree.
  float blue_min_x = 0.0f;
  float blue_max_x = 0.0f;
  float red_min_x = 0.0f;
  float red_max_x = 0.0f;
  XExtentForColour(kChromaticColourBlue, &blue_min_x, &blue_max_x);
  XExtentForColour(kChromaticColourRed, &red_min_x, &red_max_x);
  ASSERT_GT(blue_max_x, blue_min_x);
  ASSERT_GT(red_max_x, red_min_x);
  EXPECT_NEAR((blue_max_x - blue_min_x) / (red_max_x - red_min_x), 1.0f, 0.01f);
}

// The readout label's viewport clamp: a readout anchored near the canvas edge must keep its
// whole patch — backing AND glyphs — inside the canvas rectangle. The unclamped draw centred the
// text on the walk's topmost in-disk sample with no inset: an edge-close anchor cropped the word
// on the canvas edge (the owner's screenshot cut "verdict" to "dict"). The backing ink
// (16,17,20,170 — DrawIsoDeltaReadoutLabel's own literal) is unique in the frame, so its vertex
// box IS the patch rect; the glyph bucket (the anchor's colour) must sit inside it, text and
// backing moving together. Two independent pokes — the canvas's right edge (the owner's cut) and
// its top edge — plus the non-zero-origin face: the rectangle the patch must respect is the
// canvas's own [pic_min, pic_min + size], not the screen's.
//
// Camera premise (independent of the production projection): the rig's 180° equidistant fisheye
// puts a direction at angular distance θ from the view axis at screen radius img_radius·θ/90° —
// the sun at (alt 20, az 88) sits ~81° off the axis near the canvas's right edge, and the sun at
// (alt 85, az 0) ~65° off it high on the canvas; the δ rings about them keep in-disk samples
// whose topmost one anchors the label within a half-text-width of the named edge.
TEST_F(VerdictAnchorDraw, EdgeAnchorKeepsTheLabelInsideTheCanvas) {
  ImDrawList* dl = ImGui::GetBackgroundDrawList();
  const ViewProjection horizon_vp = [] {
    ViewProjection vp = SunCamera();
    vp.elevation = 0.0f;  // aiming at the horizon: the high sun rides near the canvas's top edge
    return vp;
  }();

  struct Poke {
    const char* what;
    const ViewProjection* vp;
    double sun_alt;
    double sun_az;
    double delta;
    float pic_min_x;
    float pic_min_y;
    bool glyph_wrap_assertable;  // the origin leg's canvas deliberately exceeds the screen
                                 // viewport (that is what makes the non-zero origin visible to
                                 // the clamp) — there the screen clip, not the canvas, bounds
                                 // glyph emission, so the wrap fact is unassertable in this rig
  };
  const ViewProjection sun_vp = SunCamera();
  const Poke pokes[] = {
    { "right-edge anchor", &sun_vp, 20.0, 88.0, 20.0, 0.0f, 0.0f, true },
    { "top-edge anchor", &horizon_vp, 85.0, 0.0, 5.0, 0.0f, 0.0f, true },
    { "right-edge anchor, non-zero canvas origin", &sun_vp, 20.0, 88.0, 20.0, 100.0f, 50.0f, false },
  };
  for (const Poke& poke : pokes) {
    VerdictAnchorInput in = Input(*poke.vp, /*shown=*/true, /*hover=*/false);
    in.sun.altitude_deg = poke.sun_alt;
    in.sun.azimuth_deg = poke.sun_az;
    in.pic_min_x = poke.pic_min_x;
    in.pic_min_y = poke.pic_min_y;
    dl->VtxBuffer.resize(0);
    dl->IdxBuffer.resize(0);
    dl->CmdBuffer.resize(0);
    dl->_VtxCurrentIdx = 0;  // the quads' index values come from this — a partial reset reads OOB
    dl->AddDrawCmd();        // this rig's between-leg reset: PrimReserve assumes a live command
    DrawVerdictAnchor(dl, Diagnose("edge", "blue", poke.delta, true), in);
    if (IndexCountForColour(kChromaticColourBlue) <= 0) {
      ADD_FAILURE() << poke.what << ": ring premise — the anchor must draw for the label to exist";
      continue;
    }
    float min_x = 0.0f;
    float min_y = 0.0f;
    float max_x = 0.0f;
    float max_y = 0.0f;
    RectForColour(kReadoutBackingInk, &min_x, &min_y, &max_x, &max_y);
    if (max_x <= min_x) {
      ADD_FAILURE() << poke.what << ": the label backing must have drawn";
      continue;
    }
    const float canvas_l = in.pic_min_x;
    const float canvas_t = in.pic_min_y;
    const float canvas_r = in.pic_min_x + in.width_pt;
    const float canvas_b = in.pic_min_y + in.height_pt;
    EXPECT_GE(min_x, canvas_l) << poke.what << ": patch crosses the canvas left edge";
    EXPECT_LE(max_x, canvas_r) << poke.what << ": patch crosses the canvas right edge";
    EXPECT_GE(min_y, canvas_t) << poke.what << ": patch crosses the canvas top edge";
    EXPECT_LE(max_y, canvas_b) << poke.what << ": patch crosses the canvas bottom edge";
    if (poke.glyph_wrap_assertable) {
      // The glyphs ride the backing (text and backing move together): the label's text rect sits
      // inside the backing's box, and the backing wraps it with the pad — the patch moved, never
      // shrank. The glyph box is the quad box, an advance narrower than CalcTextSize by the last
      // glyph's own side bearing — hence the slack in the pad equality.
      float gmin_x = 0.0f;
      float gmin_y = 0.0f;
      float gmax_x = 0.0f;
      float gmax_y = 0.0f;
      GlyphRectForColour(kChromaticColourBlue, &gmin_x, &gmin_y, &gmax_x, &gmax_y);
      if (gmax_x <= gmin_x) {
        ADD_FAILURE() << poke.what << ": glyphs must have drawn";
        continue;
      }
      EXPECT_GE(gmin_x, min_x - 0.5f) << poke.what << ": glyph left of backing";
      EXPECT_LE(gmax_x, max_x + 0.5f) << poke.what << ": glyph right of backing";
      EXPECT_GE(gmin_y, min_y - 0.5f) << poke.what << ": glyph above backing";
      EXPECT_LE(gmax_y, max_y + 0.5f) << poke.what << ": glyph below backing";
      EXPECT_NEAR(max_x - min_x, (gmax_x - gmin_x) + 6.0f, 4.0f) << poke.what << ": backing wraps the text";
    }
  }
}

// ---- The reference profile's hover device (§16.1.3's extension) --------------------------------------
//
// The same headless rig as the verdict anchor's. The positioning assertions read the drawn point
// against THE TEST'S OWN projection of THE TEST'S OWN sky math — AltAz→world, the walk basis, the
// φ = π − az azimuth mapping, the equidistant law and the canvas mapping, all spelled locally
// below — never a readback of the production conversion (an independent anchor is two
// implementations of one spec, not one implementation reading itself). The anchor is pinned to
// the rig's SunCamera (elevation 20, azimuth 0, roll 0); the asymmetric-sun legs are what keep it
// honest (a mirrored azimuth mapping or a swapped basis vector would land the point 180° or 90°
// away, far outside the tolerance).

class ProfileHoverDraw : public MapDeviceDrawRig {
 protected:
  static SurveyProbeView Probe(bool az_available, double az_deg, double edge_deg, double peak_deg, int state = 0) {
    SurveyProbeView p;
    p.available = true;
    p.state = state;  // 0 = the finite default: the honesty gate only refuses LUMICE_PATH_STATE_REFUSED
    p.azimuth_available = az_available;
    p.azimuth_deg = az_deg;
    p.edge_available = true;
    p.edge_deg = edge_deg;
    p.peak_available = true;
    p.peak_deg = peak_deg;
    p.separation_available = true;
    p.separation_deg = peak_deg - edge_deg;
    return p;
  }

  static ProfileHoverInput Input(const ViewProjection& vp, double sun_alt, double sun_az, SurveyProfileHover hover) {
    ProfileHoverInput in;
    in.camera = &vp;
    in.width_px = 600;
    in.height_px = 600;
    in.width_pt = 600.0f;
    in.height_pt = 600.0f;
    in.pic_min_x = 0.0f;
    in.pic_min_y = 0.0f;
    in.pic_px = 600.0f;
    in.sun.altitude_deg = sun_alt;
    in.sun.azimuth_deg = sun_az;
    in.hover = hover;
    return in;
  }

  // The colour bucket's vertex bounding box's centre — the drawn point's own read (the marker is
  // an evenly-tessellated fan about its centre; the rose bucket carries no other primitive).
  static void RoseCentre(double* cx, double* cy) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    float min_x = FLT_MAX;
    float min_y = FLT_MAX;
    float max_x = -FLT_MAX;
    float max_y = -FLT_MAX;
    for (int i = 0; i < dl->VtxBuffer.Size; ++i) {
      const ImDrawVert& v = dl->VtxBuffer[i];
      if (v.col != kProfileReadingColour) {
        continue;
      }
      min_x = std::min(min_x, v.pos.x);
      min_y = std::min(min_y, v.pos.y);
      max_x = std::max(max_x, v.pos.x);
      max_y = std::max(max_y, v.pos.y);
    }
    *cx = (min_x + max_x) * 0.5;
    *cy = (min_y + max_y) * 0.5;
  }

  // The test's own projection: sky point (δ on the probe's radial line at az) → screen, with the
  // view matrix and the equidistant law inlined for the rig's SunCamera. Returns screen pt coords.
  static std::array<double, 2> ExpectedScreenPoint(double sun_alt, double sun_az, double delta_deg,
                                                   double probe_az_deg) {
    double sun[3];
    SkyPositionOf(sun_alt, sun_az, sun);
    double u[3] = { 0.0, 0.0, 1.0 };
    double dot = u[0] * sun[0] + u[1] * sun[1] + u[2] * sun[2];
    if (std::fabs(dot) > 0.9) {
      u[0] = 1.0;
      u[1] = 0.0;
      u[2] = 0.0;
      dot = u[0] * sun[0] + u[1] * sun[1] + u[2] * sun[2];
    }
    for (int k = 0; k < 3; ++k) {
      u[k] -= dot * sun[k];
    }
    double un = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
    for (int k = 0; k < 3; ++k) {
      u[k] /= un;
    }
    double v[3] = { sun[1] * u[2] - sun[2] * u[1], sun[2] * u[0] - sun[0] * u[2], sun[0] * u[1] - sun[1] * u[0] };
    const double phi = kPiD - probe_az_deg * kD2R;  // the mapping the device claims
    const double delta = delta_deg * kD2R;
    double p[3];
    for (int k = 0; k < 3; ++k) {
      p[k] = std::cos(delta) * sun[k] + std::sin(delta) * (std::cos(phi) * u[k] + std::sin(phi) * v[k]);
    }
    // travel = −p, the rig's view (elevation 20, azimuth 0, roll 0): right/up/forward spelled at
    // those values; the equidistant fisheye: screen radius = img_radius·θ/half_fov.
    const double travel[3] = { -p[0], -p[1], -p[2] };
    const double e = 20.0 * kD2R;
    const double right[3] = { 0.0, -1.0, 0.0 };
    const double up[3] = { std::sin(e), 0.0, -std::cos(e) };
    const double fwd[3] = { std::cos(e), 0.0, std::sin(e) };
    const double vd[3] = { right[0] * travel[0] + right[1] * travel[1] + right[2] * travel[2],
                           up[0] * travel[0] + up[1] * travel[1] + up[2] * travel[2],
                           fwd[0] * travel[0] + fwd[1] * travel[1] + fwd[2] * travel[2] };
    const double theta = std::acos(std::clamp(-vd[2], -1.0, 1.0));
    const double rr = 300.0 * theta / (kPiD / 2.0);
    const double phi_s = std::atan2(vd[1], vd[0]);
    const double sx = rr * std::cos(phi_s);
    const double sy = rr * std::sin(phi_s);
    return { (sx / 600.0 + 0.5) * 600.0, (0.5 - sy / 600.0) * 600.0 };
  }

  static int RoseCount() {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    int count = 0;
    for (int i = 0; i < dl->VtxBuffer.Size; ++i) {
      if (dl->VtxBuffer[i].col == kProfileReadingColour) {
        ++count;
      }
    }
    return count;
  }
};

// The device's ONE positioning fact: the drawn point sits at the sky position the probe's own
// azimuth names — (δ on the radial line, the φ = π − az mapping) — computed by the test's own
// math above, on an asymmetric sun so neither a mirrored mapping nor a stale azimuth can pass.
TEST_F(ProfileHoverDraw, PointLandsAtTheProbeAzimuthsSkyPosition) {
  const ViewProjection vp = SunCamera();
  struct Leg {
    const char* what;
    double sun_alt;
    double sun_az;
    double delta;
    double probe_az;
  };
  const Leg legs[] = {
    { "aimed sun, built-in azimuth", 20.0, 0.0, 22.0, 0.0 },
    { "aimed sun, non-trivial azimuth", 20.0, 0.0, 22.0, 90.0 },
    { "asymmetric sun, built-in azimuth", 35.0, 55.0, 18.0, 0.0 },
    { "asymmetric sun, non-trivial azimuth", 35.0, 55.0, 30.0, 200.0 },
  };
  for (const Leg& leg : legs) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->VtxBuffer.resize(0);
    dl->IdxBuffer.resize(0);
    dl->CmdBuffer.resize(0);
    dl->_VtxCurrentIdx = 0;
    dl->AddDrawCmd();
    DrawProfileHoverDevice(dl, Probe(true, leg.probe_az, leg.delta, leg.delta + 10.0),
                           Input(vp, leg.sun_alt, leg.sun_az, SurveyProfileHover::kEdge));
    if (RoseCount() <= 0) {
      ADD_FAILURE() << leg.what << ": the point must draw";
      continue;
    }
    double cx = 0.0;
    double cy = 0.0;
    RoseCentre(&cx, &cy);
    const auto expect = ExpectedScreenPoint(leg.sun_alt, leg.sun_az, leg.delta, leg.probe_az);
    EXPECT_NEAR(cx, expect[0], 2.5) << leg.what << ": point x against the test's own projection";
    EXPECT_NEAR(cy, expect[1], 2.5) << leg.what << ": point y against the test's own projection";
  }
}

// The separation device's shape: the radial INTERVAL between the two readings, endpoints marked
// — the drawn rose bucket must span both endpoints (the test's own projection) and stay a
// bounded sliver along the line, never a ring or an annulus (a ring at either delta would blow
// the sliver's size band).
TEST_F(ProfileHoverDraw, SeparationIntervalSpansTheTwoReadings) {
  const ViewProjection vp = SunCamera();
  ImDrawList* dl = ImGui::GetBackgroundDrawList();
  const SurveyProbeView probe = Probe(true, 0.0, 25.0, 40.0);
  DrawProfileHoverDevice(dl, probe, Input(vp, 20.0, 0.0, SurveyProfileHover::kSeparation));
  ASSERT_GT(RoseCount(), 0) << "the interval must draw";
  float min_x = FLT_MAX;
  float min_y = FLT_MAX;
  float max_x = -FLT_MAX;
  float max_y = -FLT_MAX;
  for (int i = 0; i < dl->VtxBuffer.Size; ++i) {
    const ImDrawVert& vtx = dl->VtxBuffer[i];
    if (vtx.col != kProfileReadingColour) {
      continue;
    }
    min_x = std::min(min_x, vtx.pos.x);
    min_y = std::min(min_y, vtx.pos.y);
    max_x = std::max(max_x, vtx.pos.x);
    max_y = std::max(max_y, vtx.pos.y);
  }
  const auto edge_pt = ExpectedScreenPoint(20.0, 0.0, 25.0, 0.0);
  const auto peak_pt = ExpectedScreenPoint(20.0, 0.0, 40.0, 0.0);
  EXPECT_GE(min_x, std::min(edge_pt[0], peak_pt[0]) - 8.0) << "edge endpoint inside the bucket's box";
  EXPECT_LE(max_x, std::max(edge_pt[0], peak_pt[0]) + 8.0) << "no extent beyond the two endpoints";
  EXPECT_GE(min_y, std::min(edge_pt[1], peak_pt[1]) - 8.0) << "edge endpoint inside the bucket's box";
  EXPECT_LE(max_y, std::max(edge_pt[1], peak_pt[1]) + 8.0) << "no extent beyond the two endpoints";
  // The sliver band: a 15° radial interval at the equidistant scale is ~50pt of extent, the two
  // endpoint dots ~9pt each — a ring (diameter ~250pt) or an annulus sits far outside.
  EXPECT_LT(std::max(max_x - min_x, max_y - min_y), 90.0f) << "the device is an interval, not a ring";
}

// The honesty gates, in the draw's short-circuit order: no standing, a refused probe, an unknown
// azimuth, the named reading absent, an endpoint off the view — each draws nothing. Not drawing
// is the honest state; the row's own words carry the absence.
TEST_F(ProfileHoverDraw, HonestyGateLegs) {
  const ViewProjection vp = SunCamera();
  ImDrawList* dl = ImGui::GetBackgroundDrawList();
  struct Leg {
    const char* what;
    SurveyProfileHover hover;
    bool az_available;
    double probe_az;
    double edge;
    double peak;
    int state;
    double sun_alt;
    double sun_az;
  };
  const Leg legs[] = {
    { "no standing", SurveyProfileHover::kNone, true, 0.0, 22.0, 32.0, 0, 20.0, 0.0 },
    { "refused probe", SurveyProfileHover::kEdge, true, 0.0, 22.0, 32.0, LUMICE_PATH_STATE_REFUSED, 20.0, 0.0 },
    { "unknown azimuth", SurveyProfileHover::kEdge, false, 0.0, 22.0, 32.0, 0, 20.0, 0.0 },
    { "edge reading absent", SurveyProfileHover::kEdge, true, 0.0, 22.0, 32.0, 0, 20.0, 0.0 },
    { "separation missing one endpoint", SurveyProfileHover::kSeparation, true, 0.0, 22.0, 32.0, 0, 20.0, 0.0 },
    { "endpoint beyond the frustum", SurveyProfileHover::kEdge, true, 0.0, 150.0, 160.0, 0, 20.0, 0.0 },
  };
  for (const Leg& leg : legs) {
    dl->VtxBuffer.resize(0);
    dl->IdxBuffer.resize(0);
    dl->CmdBuffer.resize(0);
    dl->_VtxCurrentIdx = 0;
    dl->AddDrawCmd();
    SurveyProbeView probe = Probe(leg.az_available, leg.probe_az, leg.edge, leg.peak, leg.state);
    if (std::string(leg.what) == "edge reading absent") {
      probe.edge_available = false;
    }
    if (std::string(leg.what) == "separation missing one endpoint") {
      probe.peak_available = false;
    }
    DrawProfileHoverDevice(dl, probe, Input(vp, leg.sun_alt, leg.sun_az, leg.hover));
    EXPECT_EQ(RoseCount(), 0) << leg.what << ": nothing must draw";
  }
}

}  // namespace
}  // namespace lumice::gui
