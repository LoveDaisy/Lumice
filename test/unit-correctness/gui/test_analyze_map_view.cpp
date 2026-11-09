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

  const std::vector<std::string> lines = MapCaliberLines(in);
  ASSERT_GE(lines.size(), 3u);
  EXPECT_NE(lines[0].find("550.0 nm"), std::string::npos);
  EXPECT_NE(lines[0].find("9/12 members"), std::string::npos);
  EXPECT_NE(lines[0].find("n=500000"), std::string::npos);
  EXPECT_NE(lines[0].find("12345 events"), std::string::npos);
  EXPECT_NE(lines[0].find("[3.5, 48.5]"), std::string::npos);
  EXPECT_NE(lines[0].find("[-100.0, 260.0]"), std::string::npos);
  EXPECT_NE(lines[0].find("182\u00d71440"), std::string::npos) << "grid dims echo the Info's own";
  EXPECT_NE(lines[1].find("band-sum"), std::string::npos);
  EXPECT_NE(lines[1].find("physical class"), std::string::npos);
  EXPECT_NE(lines[1].find("per sr"), std::string::npos);
  EXPECT_NE(lines[1].find("scene geometry"), std::string::npos);
  EXPECT_EQ(lines[2], kBasis);  // the engine's geometry string, byte for byte

  // A nominal shape says so; a full-sky view names the all-sky grid.
  in.info.shape_is_nominal = 1;
  in.info.view_full_sky = 1;
  in.info.view_rows = 180;
  in.info.view_cols = 360;
  const std::vector<std::string> full = MapCaliberLines(in);
  EXPECT_NE(full[0].find("full sky 180\u00d7360"), std::string::npos);
  EXPECT_NE(full[1].find("nominal geometry"), std::string::npos);
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
  std::vector<std::string> lines = MapCaliberLines(in);
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
  lines = MapCaliberLines(in);
  zero_line = false;
  for (const std::string& l : lines) {
    zero_line = zero_line || l.find("finite zero") != std::string::npos;
  }
  EXPECT_TRUE(zero_line);
  // The same document before its intent is answered draws no finite-zero claim.
  in.completed = false;
  lines = MapCaliberLines(in);
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
  lines = MapCaliberLines(in);
  bool mass_line = false;
  for (const std::string& l : lines) {
    mass_line = mass_line ||
                (l.find("Rank-0 arm") != std::string::npos && l.find("3.14159") != std::string::npos &&
                 l.find("0.012") != std::string::npos && l.find("psi_average, lattice_mean") != std::string::npos &&
                 l.find("not no-light") != std::string::npos);
  }
  EXPECT_TRUE(mass_line);
  in.mass.pixel = -1;
  lines = MapCaliberLines(in);
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

}  // namespace
}  // namespace lumice::gui
