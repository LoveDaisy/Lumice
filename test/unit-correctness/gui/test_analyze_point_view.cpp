#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "gui/analysis_panel.hpp"
#include "gui/analysis_result.hpp"
#include "gui/analyze_point_view.hpp"
#include "gui/analyze_survey.hpp"
#include "gui/file_io.hpp"  // SceneDeleter's complete type — the map tests' include
#include "gui/gui_state.hpp"
#include "include/lumice_base.h"
#include "include/lumice_editor.h"
#include "include/lumice_raypath.h"
#include "include/lumice_scene.h"
#include "util/sky_direction.hpp"

namespace lumice::gui {
namespace {

constexpr double kPiD = 3.14159265358979323846;
constexpr double kD2R = kPiD / 180.0;

// The position vector of the sky label (alt, az) in degrees — the tests' own §12 spelling.
void SkyPositionOf(double alt_deg, double az_deg, double pos[3]) {
  const double alt = alt_deg * kD2R;
  const double az = az_deg * kD2R;
  pos[0] = std::cos(alt) * std::cos(az);
  pos[1] = std::cos(alt) * std::sin(az);
  pos[2] = std::sin(alt);
}

// ---- Request derivation (pure) ----------------------------------------------------------------------

TEST(AnalyzePointView, PointRequestCarriesIdentityTargetLambdaAndTier) {
  PointIntentKey key;
  key.selection.crystal_id = 3;
  key.selection.faces = { 3, 5 };
  key.selection.layer_face_counts = { 2 };
  key.selection.generation = 11;
  // The intent carries the TRAVEL direction of the sky point labelled (22, 0); the request
  // builder converts it through DirToAltAz — the labels come back, independently checked.
  const double pos[3] = { std::cos(22.0 * kD2R), 0.0, std::sin(22.0 * kD2R) };
  for (int k = 0; k < 3; ++k) {
    key.target_dir[k] = static_cast<float>(-pos[k]);  // travel = the negated position
  }
  key.wavelength_nm = 589.0;

  std::vector<int> faces;
  std::vector<int> counts;
  const LUMICE_SinglePathRequest request = MakePointRequestForKey(key, &faces, &counts);
  EXPECT_EQ(request.struct_size, sizeof(request));
  EXPECT_EQ(request.crystal_id, 3);
  EXPECT_EQ(request.face_count, 2);
  ASSERT_EQ(faces.size(), 2U);
  EXPECT_EQ(faces[0], 3);
  EXPECT_EQ(faces[1], 5);
  ASSERT_EQ(counts.size(), 1U);
  EXPECT_EQ(counts[0], 2);
  // The labels of the sky point the intent named: altitude asin(z), azimuth atan2(y,x)-180 —
  // derived here against the §12 position formula, not by calling the helper back.
  EXPECT_NEAR(request.target_altitude_deg, 22.0, 1e-3);
  EXPECT_NEAR(std::fmod(request.target_azimuth_deg + 360.0 + 180.0, 360.0) - 180.0, 0.0, 1e-3);
  EXPECT_EQ(request.wavelength_nm, 589.0);
  EXPECT_EQ(request.sample_count, kPointQuerySampleCount);
  EXPECT_EQ(request.sun_grid_lat_count, kPointSunGridLatCount);
  EXPECT_EQ(request.deadline_ms, kPointQueryBudgetMs);
  EXPECT_EQ(request.warm_json, nullptr);
  // The zero-λ intent is the engine's reference resolution: carried as 0.0, no buffer games.
  key.wavelength_nm = 0.0;
  const LUMICE_SinglePathRequest reference = MakePointRequestForKey(key, &faces, &counts);
  EXPECT_EQ(reference.wavelength_nm, 0.0);
}

TEST(AnalyzePointView, RayRequestCarriesFacesPoseAndLambda) {
  SelectedClassIdentity identity;
  identity.core_crystal_id = 2;
  identity.faces = { 4, 6 };
  identity.layer_face_counts = { 2 };
  const double pose[9] = { 1, 0, 0, 0, 0, -1, 0, 1, 0 };
  std::vector<int> faces;
  std::vector<int> counts;
  std::vector<double> wavelengths;

  const LUMICE_RepresentativeRayRequest explicit_lambda =
      MakeRayRequestForSample(identity, pose, 650.0, &faces, &counts, &wavelengths);
  EXPECT_EQ(explicit_lambda.struct_size, sizeof(explicit_lambda));
  EXPECT_EQ(explicit_lambda.crystal_id, 2);
  EXPECT_EQ(explicit_lambda.face_count, 2);
  EXPECT_EQ(explicit_lambda.layer_count, 1);
  EXPECT_EQ(explicit_lambda.pose, pose);  // rebound by the executor to its job-owned buffer
  ASSERT_EQ(wavelengths.size(), 1U);
  EXPECT_EQ(wavelengths[0], 650.0);
  EXPECT_EQ(explicit_lambda.wavelength_count, 1);

  const LUMICE_RepresentativeRayRequest reference =
      MakeRayRequestForSample(identity, pose, 0.0, &faces, &counts, &wavelengths);
  EXPECT_TRUE(wavelengths.empty());
  EXPECT_EQ(reference.wavelengths_nm, nullptr);
  EXPECT_EQ(reference.wavelength_count, 0);
}

TEST(AnalyzePointView, PointIntentKeyEqualityIsExact) {
  PointIntentKey key;
  key.selection.generation = 5;
  key.selection.faces = { 3, 5 };
  key.target_dir[0] = 0.5f;
  key.target_dir[2] = -0.5f;
  key.wavelength_nm = 550.0;
  PointIntentKey same = key;
  EXPECT_TRUE(key == same);
  // A moved sky point is a new intent; so is a re-selection and a wavelength change.
  same.target_dir[1] = 0.25f;
  EXPECT_TRUE(key != same);
  same = key;
  same.selection.generation = 6;
  EXPECT_TRUE(key != same);
  same = key;
  same.wavelength_nm = 650.0;
  EXPECT_TRUE(key != same);
}

// ---- State machines (pure; the drawn/refused arms take real handles in the composition tests) -------

TEST(AnalyzePointView, PointQueryStateTruthTable) {
  PointQuerySession point;
  EXPECT_EQ(PointQueryStateFor(point), PointQueryState::kNoIntent);
  point.intent_valid = true;
  point.intent_id = SurveyQueryId{ 7, 1 };
  EXPECT_EQ(PointQueryStateFor(point), PointQueryState::kWaiting);
  point.cancelled_for_key = true;
  EXPECT_EQ(PointQueryStateFor(point), PointQueryState::kCancelled);
  point.cancelled_for_key = false;
  point.adopted_error = "multi_layer_unsupported: 2 layers";
  EXPECT_EQ(PointQueryStateFor(point), PointQueryState::kRefused);
  // The completed arm reads a real handle — the composition tests cover it over a live adoption.
}

TEST(AnalyzePointView, PlaybackStepsAndWrapsAndStops) {
  PointViewState view;
  view.playing = true;
  EXPECT_FALSE(AdvancePointPlayback(&view, 5, 0.0));  // no step before the interval
  EXPECT_FALSE(AdvancePointPlayback(&view, 5, kPointPlaybackStepSeconds * 0.9));
  EXPECT_TRUE(AdvancePointPlayback(&view, 5, kPointPlaybackStepSeconds + 0.01));
  EXPECT_EQ(view.sample_index, 1);
  EXPECT_FALSE(AdvancePointPlayback(&view, 5, kPointPlaybackStepSeconds + 0.02));  // too soon
  view.last_play_step_s = 0.0;
  for (int i = 0; i < 4; ++i) {
    EXPECT_TRUE(AdvancePointPlayback(&view, 5, (i + 1) * (kPointPlaybackStepSeconds + 0.01)));
  }
  EXPECT_EQ(view.sample_index, 0);  // wrapped
  // A stale cursor restarts instead of reading out of range.
  view.sample_index = 99;
  EXPECT_TRUE(AdvancePointPlayback(&view, 5, 10.0 * (kPointPlaybackStepSeconds + 0.01)));
  EXPECT_EQ(view.sample_index, 0);
  // Not playing: nothing moves.
  view.playing = false;
  EXPECT_FALSE(AdvancePointPlayback(&view, 5, 100.0));
  EXPECT_EQ(view.sample_index, 0);
  // Empty component: nothing moves.
  view.playing = true;
  EXPECT_FALSE(AdvancePointPlayback(&view, 0, 200.0));
}

// A hand-built projector — no result needed: the projection holds itself to anchors derived from
// the definition (centre at the middle, a 90-degree point half way to the rim, the antipode on
// the rim at the deterministic azimuth).
TEST(AnalyzePointView, GlobeProjectionMatchesHandDerivedAnchors) {
  SunGlobeProjector p;
  p.centre[0] = 0.0;
  p.centre[1] = 0.0;
  p.centre[2] = 1.0;
  // right = centre x refUp? No — the definition: up = ref - (ref·centre)centre, right = up x centre.
  // With centre +z and ref +z the fallback reference is +x: up = +x - 0 = +x; right = up x centre
  // = x̂ × ẑ = -ŷ. Spell it from the maker's rule instead of trusting the fields:
  for (int k = 0; k < 3; ++k) {
    p.up[k] = k == 0 ? 1.0 : 0.0;  // +x
  }
  p.right[0] = p.up[1] * p.centre[2] - p.up[2] * p.centre[1];
  p.right[1] = p.up[2] * p.centre[0] - p.up[0] * p.centre[2];
  p.right[2] = p.up[0] * p.centre[1] - p.up[1] * p.centre[0];  // = x̂ × ẑ = -ŷ

  double x = 0.0;
  double y = 0.0;
  const double centre[3] = { 0.0, 0.0, 1.0 };
  SunGlobeProject(p, centre, &x, &y);
  EXPECT_NEAR(x, 0.5, 1e-12);
  EXPECT_NEAR(y, 0.5, 1e-12);
  // A direction 90 degrees from the centre along the screen right: rho = sin(45°)·... = 2sin(π/4)/2
  // = 0.7071 — its disk radius is half of that.
  const double side[3] = { 0.0, -1.0, 0.0 };  // along right = -ŷ
  SunGlobeProject(p, side, &x, &y);
  const double rho90 = std::sin(kPiD / 4.0);
  EXPECT_NEAR(x, 0.5 + 0.5 * rho90, 1e-12);
  EXPECT_NEAR(y, 0.5, 1e-12);
  // Along the screen up (+x): same radius, y flips (screen y is down).
  const double up_dir[3] = { 1.0, 0.0, 0.0 };
  SunGlobeProject(p, up_dir, &x, &y);
  EXPECT_NEAR(x, 0.5, 1e-12);
  EXPECT_NEAR(y, 0.5 - 0.5 * rho90, 1e-12);
  // The antipode: on the rim, at the deterministic azimuth (atan2(0,0) = 0 → along +right).
  const double antipode[3] = { 0.0, 0.0, -1.0 };
  SunGlobeProject(p, antipode, &x, &y);
  EXPECT_NEAR(x, 1.0, 1e-12);
  EXPECT_NEAR(y, 0.5, 1e-12);
  // Total function: any direction lands inside the bounding square.
  const double somewhere[3] = { 0.3, -0.4, -0.6 };
  SunGlobeProject(p, somewhere, &x, &y);
  EXPECT_GE(x, -1e-9);
  EXPECT_LE(x, 1.0 + 1e-9);
  EXPECT_GE(y, -1e-9);
  EXPECT_LE(y, 1.0 + 1e-9);
}

TEST(AnalyzePointView, SunGridCellColorPaintsNothingWhereInvalid) {
  // IM_COL32 packing spelled by hand ((a<<24)|(b<<16)|(g<<8)|r) — this TU does not pull imgui.
  const auto alpha_of = [](uint32_t c) { return (c >> 24) & 0xFFu; };
  const auto red_of = [](uint32_t c) { return c & 0xFFu; };
  EXPECT_EQ(SunGridCellColorRgba(0.5, 0), 0u);
  EXPECT_EQ(SunGridCellColorRgba(std::nan(""), 1), 0u);
  // Valid cells ramp monotonically with the deviation, fully opaque.
  const uint32_t dark = SunGridCellColorRgba(0.01, 1);
  const uint32_t light = SunGridCellColorRgba(3.0, 1);
  EXPECT_EQ(alpha_of(dark), 235u);
  EXPECT_GT(red_of(light), red_of(dark));
}

TEST(AnalyzePointView, FactorLinesSpellUnavailableNotZero) {
  LUMICE_PathPointSample sample{};
  sample.struct_size = sizeof(sample);
  sample.zenith_deg = 30.0;
  sample.azimuth_deg = -45.0;
  sample.roll_deg = 10.0;
  sample.angles_degenerate = 0;
  sample.rho = 0.0125;
  sample.entry_measure = 0.35;
  sample.entry_measure_a2 = 0.35;
  sample.total_transmission = 0.95;
  sample.jacobian_available = 0;  // the engine did not produce it — the lines must say so
  const double interfaces_t[2] = { 0.977, 0.978 };
  sample.interface_transmittances = interfaces_t;
  sample.interface_count = 2;

  const std::vector<std::string> lines = PointFactorLines(sample);
  std::string joined;
  for (const std::string& l : lines) {
    joined += l;
    joined += "\n";
  }
  EXPECT_NE(joined.find("unavailable"), std::string::npos);
  EXPECT_NE(joined.find("conditional on this path"), std::string::npos);
  EXPECT_EQ(joined.find("rho*A/a^2*T*K = "), std::string::npos);  // no product number without J
  EXPECT_NE(joined.find("interface 1/2 transmittance = 0.977"), std::string::npos);
  EXPECT_EQ(joined.find("(degenerate)"), std::string::npos);  // not degenerate: the marker stays off

  sample.angles_degenerate = 1;
  const std::vector<std::string> degenerate = PointFactorLines(sample);
  std::string joined_deg;
  for (const std::string& l : degenerate) {
    joined_deg += l;
    joined_deg += "\n";
  }
  EXPECT_NE(joined_deg.find("(degenerate)"), std::string::npos);
  sample.angles_degenerate = 0;

  sample.jacobian_available = 1;
  sample.jacobian = 0.5;
  sample.singular_values[0] = 0.4;
  sample.singular_values[1] = 0.1;
  sample.k_carea = 2.0;
  sample.factor_product = 0.0083;
  const std::vector<std::string> with_j = PointFactorLines(sample);
  std::string joined2;
  for (const std::string& l : with_j) {
    joined2 += l;
    joined2 += "\n";
  }
  EXPECT_NE(joined2.find("K = 1/(J+eps) = 2"), std::string::npos);
  EXPECT_NE(joined2.find("rho*A/a^2*T*K = 0.0083"), std::string::npos);
}

TEST(AnalyzePointView, CaliberLinesEchoTheInfoVerbatim) {
  LUMICE_PathPointInfo info{};
  info.struct_size = sizeof(info);
  info.target_altitude_deg = 22.5;
  info.target_azimuth_deg = -30.0;
  info.target_deviation_deg = 22.51;
  info.crystal_id = 4;
  static const int faces[2] = { 3, 5 };
  info.faces = faces;
  info.face_count = 2;
  info.wavelength_nm = 550.0;
  const char* source = "explicit";
  info.wavelength_source = LUMICE_PathText{ source, std::strlen(source) };
  const char* kind = "prism";
  info.crystal_kind = LUMICE_PathText{ kind, std::strlen(kind) };
  info.refractive_index = 1.3116;
  info.sample_count = 1000000;
  info.budget_ms = 2000;
  info.execution = LUMICE_PATH_OUTCOME_COMPLETED;

  const std::vector<std::string> lines = PointCaliberLines(info);
  std::string joined;
  for (const std::string& l : lines) {
    joined += l;
    joined += "\n";
  }
  EXPECT_NE(joined.find("alt 22.500 deg, az -30.000 deg (deviation 22.510 deg)"), std::string::npos);
  EXPECT_NE(joined.find("member faces 3-5, crystal 4, prism"), std::string::npos);
  EXPECT_NE(joined.find("wavelength 550.00 nm (explicit)"), std::string::npos);
  EXPECT_NE(joined.find("sample_count 1000000, budget 2000 ms, execution: completed"), std::string::npos);
}

TEST(AnalyzePointView, MassLinesSpellTheOrbitWords) {
  LUMICE_PathPointMass mass{};
  mass.struct_size = sizeof(mass);
  mass.altitude_deg = 20.0;
  mass.azimuth_deg = 0.0;
  mass.target_separation_deg = 1e-12;
  const char* support = "spin_orbit";
  mass.measure_support = LUMICE_PathText{ support, std::strlen(support) };
  mass.orbit_available = 1;
  mass.orbit_integral = 0.097;
  mass.orbit_sample_count = 1024;
  mass.orbit_kept_count = 1024;
  std::string joined;
  for (const std::string& l : PointMassLines(mass)) {
    joined += l + "\n";
  }
  EXPECT_NE(joined.find("spin_orbit"), std::string::npos);
  EXPECT_NE(joined.find("1024/1024"), std::string::npos);
  mass.orbit_available = 0;
  const char* reason = "measure_support_not_spin_orbit";
  mass.orbit_reason = LUMICE_PathText{ reason, std::strlen(reason) };
  joined.clear();
  for (const std::string& l : PointMassLines(mass)) {
    joined += l + "\n";
  }
  EXPECT_NE(joined.find("unavailable (measure_support_not_spin_orbit)"), std::string::npos);
}

TEST(AnalyzePointView, CrystalFrameTransformIsTheInverseRotation) {
  // The pose: a 90-degree rotation about world x (row-major body->world). The crystal-frame
  // transform is its transpose; applying it, then applying it to the transposed pose, round-trips.
  const double c = std::cos(kPiD / 2.0);
  const double s = std::sin(kPiD / 2.0);
  const double pose[9] = { 1, 0, 0, 0, c, -s, 0, s, c };
  const float body_point[3] = { 0.25f, -0.5f, 0.75f };
  float p[3] = { body_point[0], body_point[1], body_point[2] };
  RayTracePointToCrystalFrame(pose, p);
  // R^T by hand for R = [[1,0,0],[0,0,-1],[0,1,0]]: R^T = [[1,0,0],[0,0,1],[0,-1,0]].
  EXPECT_NEAR(p[0], body_point[0], 1e-6);
  EXPECT_NEAR(p[1], body_point[2], 1e-6);
  EXPECT_NEAR(p[2], -body_point[1], 1e-6);
  // Roundtrip: the transpose of a rotation is its inverse.
  double pose_t[9];
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      pose_t[i * 3 + j] = pose[j * 3 + i];
    }
  }
  RayTracePointToCrystalFrame(pose_t, p);
  EXPECT_NEAR(p[0], body_point[0], 1e-6);
  EXPECT_NEAR(p[1], body_point[1], 1e-6);
  EXPECT_NEAR(p[2], body_point[2], 1e-6);
}

// ---- The pumps, over a real executor and a real scene -----------------------------------------------

// The C01 corpus scene (doc/cases/raypath-analysis-cases.md): the random prism at sun 0/0 — the
// 22-degree halo's fiber is the verified live anchor (the M0 probe queried it through the CLI).
constexpr const char* kPointSceneJson = R"({
  "crystal": [ { "id": 1, "type": "prism",
                 "shape": { "height": 1.0, "face_distance": [1,1,1,1,1,1] },
                 "axis": { "zenith": { "type": "uniform", "mean": 0, "std": 360 },
                           "azimuth": { "type": "uniform", "mean": 0, "std": 360 },
                           "roll": { "type": "uniform", "mean": 0, "std": 360 } } } ],
  "filter": [],
  "scene": { "light_source": { "type": "sun", "altitude": 0, "azimuth": 0, "spectrum": "D65" },
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

// The sky point intent for the label (alt, az): travel = the negated position, through the same
// named conversion the map's coordinate input uses.
void SetSkyPoint(GuiState& state, double alt_deg, double az_deg) {
  const double pos[3] = { std::cos(alt_deg * kD2R) * std::cos(az_deg * kD2R),
                          std::cos(alt_deg * kD2R) * std::sin(az_deg * kD2R), std::sin(alt_deg * kD2R) };
  state.analysis.sky_point.valid = true;
  for (int k = 0; k < 3; ++k) {
    state.analysis.sky_point.dir[k] = static_cast<float>(-pos[k]);
  }
}

// The headless frame loop: the two pumps under test (point, then the ray trace it feeds). The
// survey and map pumps stay out — a selection with a payload makes the survey pump issue a full
// survey (its chromatic legs run ~15 s on this class, queueing first on the one worker), which is
// the real app's ordering but not what these tests are about.
template <typename Pred>
bool PumpAllUntil(GuiState& state, SurveyExecutor& executor, double now, Pred pred, int timeout_ms) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (!pred()) {
    if (std::chrono::steady_clock::now() > deadline) {
      return false;
    }
    PumpPointQuery(state, executor);
    PumpRayTrace(state, executor);
    now += 0.05;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  PumpPointQuery(state, executor);
  PumpRayTrace(state, executor);
  return true;
}

TEST(AnalyzePointView, PointPumpAdoptsEchoesAndMovesHonestly) {
  LUMICE_Scene* raw_scene = nullptr;
  ASSERT_EQ(LUMICE_SceneFromJson(kPointSceneJson, &raw_scene), LUMICE_OK);
  std::unique_ptr<LUMICE_Scene, SceneDeleter> scene(raw_scene);
  SurveyExecutor executor;
  executor.SetScene(std::move(scene));

  GuiState state;
  state.analysis_result.payload = Rows(7, { "3-5" }, { { 3, 5 } });
  state.analysis_result.analyzed_scene = GuiState::AnalysisSceneIdentity::From(state);
  state.analysis.selected_entry = "3-5";
  SetSkyPoint(state, 22.0, 0.0);
  const double now = 100.0;

  // The intent issues and the C01 halo point adopts (a real engine document, ~2 s tier).
  ASSERT_TRUE(
      PumpAllUntil(state, executor, now, [&] { return state.analysis.point.adopted_result != nullptr; }, 60000));
  EXPECT_EQ(PointQueryStateFor(state.analysis.point), PointQueryState::kCompleted);
  const SurveyQueryId first_id = state.analysis.point.intent_id;
  // The document's echo: the labels the intent named, the faces the row spelled, completed.
  LUMICE_PathPointInfo info{};
  info.struct_size = sizeof(info);
  ASSERT_EQ(LUMICE_PathPointGetInfo(state.analysis.point.adopted_result.get(), &info), LUMICE_PATH_READ_OK);
  EXPECT_NEAR(info.target_altitude_deg, 22.0, 1e-3);
  EXPECT_NEAR(std::fmod(info.target_azimuth_deg + 540.0, 360.0) - 180.0, 0.0, 1e-3);
  EXPECT_EQ(info.face_count, 2);
  EXPECT_EQ(info.faces[0], 3);
  EXPECT_EQ(info.faces[1], 5);
  EXPECT_EQ(info.outcome, LUMICE_PATH_POINT_DISCOVERED);
  EXPECT_EQ(info.execution, LUMICE_PATH_OUTCOME_COMPLETED);
  // The reading helpers over the real document: rows, valid-sample mapping, a consistent cursor.
  std::vector<PointComponentRow> rows;
  const int components = PointComponentRows(*state.analysis.point.adopted_result, &rows);
  ASSERT_GE(components, 1);
  EXPECT_FALSE(rows[0].kind.empty());
  EXPECT_GT(rows[0].point_count, 0);
  const int valid_count = PointValidSampleCount(*state.analysis.point.adopted_result, 0);
  EXPECT_GT(valid_count, 0);
  const int stored = PointValidSampleIndexAt(*state.analysis.point.adopted_result, 0, valid_count - 1);
  ASSERT_GE(stored, 0);
  LUMICE_PathPointSample sample{};
  sample.struct_size = sizeof(sample);
  ASSERT_EQ(
      LUMICE_PathPointGetSample(state.analysis.point.adopted_result.get(), 0, static_cast<size_t>(stored), &sample),
      LUMICE_PATH_READ_OK);
  EXPECT_NE(sample.valid, 0);

  // Moving the sky point is a new intent; the old answer DROPS (a newer target's answer must not
  // linger under the new one — AC5), and the gate kills the superseded zombie when it arrives.
  SetSkyPoint(state, 21.9, 60.0);
  PumpPointQuery(state, executor);
  EXPECT_NE(state.analysis.point.intent_id, first_id);
  EXPECT_EQ(state.analysis.point.adopted_result, nullptr);
  EXPECT_EQ(PointQueryStateFor(state.analysis.point), PointQueryState::kWaiting);
  ASSERT_TRUE(
      PumpAllUntil(state, executor, now, [&] { return state.analysis.point.adopted_result != nullptr; }, 60000));
  executor.ShutdownAndJoin();
}

TEST(AnalyzePointView, RayTraceFollowsTheSampleAndDiesWithThePoint) {
  LUMICE_Scene* raw_scene = nullptr;
  ASSERT_EQ(LUMICE_SceneFromJson(kPointSceneJson, &raw_scene), LUMICE_OK);
  std::unique_ptr<LUMICE_Scene, SceneDeleter> scene(raw_scene);
  SurveyExecutor executor;
  executor.SetScene(std::move(scene));

  GuiState state;
  state.analysis_result.payload = Rows(7, { "3-5" }, { { 3, 5 } });
  state.analysis_result.analyzed_scene = GuiState::AnalysisSceneIdentity::From(state);
  state.analysis.selected_entry = "3-5";
  SetSkyPoint(state, 22.0, 0.0);
  const double now = 100.0;

  ASSERT_TRUE(
      PumpAllUntil(state, executor, now, [&] { return state.analysis.point.adopted_result != nullptr; }, 60000));
  // The ray intent follows the adopted point's first valid sample and adopts (C01's pairing is
  // the 742.2 pin: the pose realizes the queried faces).
  ASSERT_TRUE(
      PumpAllUntil(state, executor, now, [&] { return state.analysis.trace.adopted_result != nullptr; }, 30000));
  const RayTraceKey key = state.analysis.trace.key;
  EXPECT_EQ(key.component, 0);
  const int valid_count = PointValidSampleCount(*state.analysis.point.adopted_result, 0);
  EXPECT_EQ(key.sample, PointValidSampleIndexAt(*state.analysis.point.adopted_result, 0, 0));
  RayTraceKey current;
  current.point_id = state.analysis.point.intent_id;
  current.component = 0;
  current.sample = key.sample;
  EXPECT_EQ(RayTraceDisplayStateFor(state.analysis.trace, current), RayTraceDisplayState::kDrawn);
  // The polyline: entry + internal + exit over a two-face path, straight from the readers.
  RayTracePolyline line;
  ASSERT_TRUE(BuildRayTracePolyline(*state.analysis.trace.adopted_result, 1.1f, &line));
  EXPECT_EQ(line.seg_kinds.size(), 3U);
  EXPECT_EQ(line.points.size(), 18U);
  EXPECT_EQ(line.seg_kinds[0], 0);
  EXPECT_EQ(line.seg_kinds[1], 1);
  EXPECT_EQ(line.seg_kinds[2], 2);
  // A stale key never draws (AC4): ask for another sample — the state is the named computing
  // until the fresh trace arrives, and the drawn polyline below re-keys to the new sample.
  current.sample = key.sample + 1 < valid_count ? key.sample + 1 : key.sample - 1;
  state.analysis.point_view.sample_index = 1 < valid_count ? 1 : 0;
  if (current.sample != state.analysis.trace.key.sample) {
    EXPECT_EQ(RayTraceDisplayStateFor(state.analysis.trace, current), RayTraceDisplayState::kComputing);
  }
  ASSERT_TRUE(PumpAllUntil(
      state, executor, now,
      [&] {
        current.point_id = state.analysis.point.intent_id;
        current.component = 0;
        current.sample =
            PointValidSampleIndexAt(*state.analysis.point.adopted_result, 0,
                                    std::clamp(state.analysis.point_view.sample_index, 0, valid_count - 1));
        return state.analysis.trace.key == current && state.analysis.trace.adopted_result != nullptr;
      },
      30000));
  // Selection death kills both sessions — no adopted point, no lingering trace (AC5).
  state.analysis.selected_entry.reset();
  PumpPointQuery(state, executor);
  PumpRayTrace(state, executor);
  EXPECT_EQ(PointQueryStateFor(state.analysis.point), PointQueryState::kNoIntent);
  EXPECT_FALSE(state.analysis.trace.intent_valid);
  EXPECT_EQ(state.analysis.trace.adopted_result, nullptr);
  EXPECT_EQ(RayTraceDisplayStateFor(state.analysis.trace, current), RayTraceDisplayState::kHidden);
  executor.ShutdownAndJoin();
}

TEST(AnalyzePointView, PointNeedsBothSelectionAndSkyPoint) {
  LUMICE_Scene* raw_scene = nullptr;
  ASSERT_EQ(LUMICE_SceneFromJson(kPointSceneJson, &raw_scene), LUMICE_OK);
  std::unique_ptr<LUMICE_Scene, SceneDeleter> scene(raw_scene);
  SurveyExecutor executor;
  executor.SetScene(std::move(scene));

  GuiState state;
  state.analysis_result.payload = Rows(7, { "3-5" }, { { 3, 5 } });
  state.analysis_result.analyzed_scene = GuiState::AnalysisSceneIdentity::From(state);
  state.analysis.selected_entry = "3-5";
  // No sky point yet: no intent, even with a selection.
  PumpPointQuery(state, executor);
  EXPECT_EQ(PointQueryStateFor(state.analysis.point), PointQueryState::kNoIntent);
  // With both, the intent issues.
  SetSkyPoint(state, 22.0, 0.0);
  PumpPointQuery(state, executor);
  EXPECT_TRUE(state.analysis.point.intent_valid);
  // The intent dies with the sky point (the user cleared it): the session is dropped, adopted
  // outcome included.
  ASSERT_TRUE(
      PumpAllUntil(state, executor, 100.0, [&] { return state.analysis.point.adopted_result != nullptr; }, 60000));
  state.analysis.sky_point.valid = false;
  PumpPointQuery(state, executor);
  EXPECT_EQ(PointQueryStateFor(state.analysis.point), PointQueryState::kNoIntent);
  EXPECT_EQ(state.analysis.point.adopted_result, nullptr);
  executor.ShutdownAndJoin();
}

TEST(AnalyzePointView, PointCancelLatchHoldsAgainstThePump) {
  LUMICE_Scene* raw_scene = nullptr;
  ASSERT_EQ(LUMICE_SceneFromJson(kPointSceneJson, &raw_scene), LUMICE_OK);
  std::unique_ptr<LUMICE_Scene, SceneDeleter> scene(raw_scene);
  SurveyExecutor executor;
  executor.SetScene(std::move(scene));

  GuiState state;
  state.analysis_result.payload = Rows(7, { "3-5" }, { { 3, 5 } });
  state.analysis_result.analyzed_scene = GuiState::AnalysisSceneIdentity::From(state);
  state.analysis.selected_entry = "3-5";
  SetSkyPoint(state, 22.0, 0.0);
  PumpPointQuery(state, executor);
  ASSERT_TRUE(state.analysis.point.intent_valid);
  const SurveyQueryId issued = state.analysis.point.intent_id;
  // The cancel button's rows (the details block's own shape): withdraw, latch, stop.
  executor.CancelInFlight(QueryFace::kPoint);
  state.analysis.point.cancelled_for_key = true;
  state.analysis.point.adopted_result.reset();
  state.analysis.point.adopted_error.clear();
  // The pump must NOT re-issue while the same key stands — the latch is the button's meaning.
  for (int i = 0; i < 5; ++i) {
    PumpPointQuery(state, executor);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  EXPECT_TRUE(state.analysis.point.intent_valid);
  EXPECT_EQ(state.analysis.point.intent_id, issued);
  EXPECT_EQ(state.analysis.point.adopted_result, nullptr);
  EXPECT_EQ(PointQueryStateFor(state.analysis.point), PointQueryState::kCancelled);
  // A moved sky point starts fresh: the latch clears with the old intent.
  SetSkyPoint(state, 21.9, 60.0);
  PumpPointQuery(state, executor);
  EXPECT_TRUE(state.analysis.point.intent_valid);
  EXPECT_FALSE(state.analysis.point.cancelled_for_key);
  executor.ShutdownAndJoin();
}

}  // namespace
}  // namespace lumice::gui
