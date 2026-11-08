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

// ---- The contribution curve (756): the pure model ----------------------------------------------------

TEST(AnalyzePointView, CurveClassifySplitsTheFlagQuadrants) {
  using K = PointCurveValueKind;
  // The gate decides availability; the value decides positive/zero only behind a true gate
  // (audit §3: a C caller never tests usability by comparing the value — the flag-clear cell
  // shows the BeginRead zero, the valid-fail cell shows storage's NaN copied through).
  EXPECT_EQ(PointCurveClassify(0.25, false), K::kUnavailable);
  EXPECT_EQ(PointCurveClassify(0.0, false), K::kUnavailable);  // the BeginRead zero is NOT data
  EXPECT_EQ(PointCurveClassify(std::nan(""), false), K::kUnavailable);
  EXPECT_EQ(PointCurveClassify(0.25, true), K::kPositive);
  EXPECT_EQ(PointCurveClassify(0.0, true), K::kExactZero);             // the legal zero (plate rho, C10 area)
  EXPECT_EQ(PointCurveClassify(std::nan(""), true), K::kUnavailable);  // NaN passthrough: not drawable
  EXPECT_EQ(PointCurveClassify(-1.0, true), K::kUnavailable);          // physically impossible: named, not drawn
}

TEST(AnalyzePointView, CurveAxisFitsOnlyPositiveValuesAcrossAllArms) {
  using K = PointCurveValueKind;
  PointContributionCurve curve;
  curve.points.resize(101);
  for (PointCurvePoint& point : curve.points) {
    point.rho = 2.0;
    point.rho_kind = K::kPositive;
    point.area = 1e200;  // unavailable values must not affect the union
    point.transmission_kind = K::kExactZero;
  }
  curve.points[0].rho = 1e-260;
  curve.points[1].product = 20.0;
  curve.points[1].product_kind = K::kPositive;
  const PointCurveAxisRange fit = PointCurveAxisFit(curve);
  ASSERT_TRUE(fit.valid);
  EXPECT_DOUBLE_EQ(fit.lo, 2.0);  // rank 2 of 102 positives
  EXPECT_DOUBLE_EQ(fit.hi, 20.0);
  const PointCurveOutsideCount outside = PointCurveCountOutside(curve, fit.lo, fit.hi);
  EXPECT_EQ(outside.points, 1);
  EXPECT_DOUBLE_EQ(outside.min, 1e-260);
  EXPECT_FALSE(outside.above);
  EXPECT_EQ(PointCurveOutsideLabel(outside), "1 points below axis · min 1e-260");

  // With more than 1% in the tiny tail, the relative noise floor wins instead of P1.
  curve.points[2].rho = 1e-260;
  EXPECT_DOUBLE_EQ(PointCurveAxisFit(curve).lo, 20.0 * 1e-15);
}

TEST(AnalyzePointView, CurveAxisPreservesNarrowSmallSamplesAndDegenerateInputs) {
  using K = PointCurveValueKind;
  PointContributionCurve curve;
  EXPECT_FALSE(PointCurveAxisFit(curve).valid);
  curve.points.resize(100);
  for (size_t i = 0; i < curve.points.size(); ++i) {
    curve.points[i].area = 2.0 + static_cast<double>(i) * 0.001;
    curve.points[i].area_kind = K::kPositive;
  }
  const PointCurveAxisRange narrow = PointCurveAxisFit(curve);
  EXPECT_DOUBLE_EQ(narrow.lo, 2.0);
  EXPECT_DOUBLE_EQ(narrow.hi, 2.099);
  EXPECT_EQ(PointCurveCountOutside(curve, narrow.lo, narrow.hi).points, 0);
  curve.points.resize(1);
  EXPECT_DOUBLE_EQ(PointCurveAxisFit(curve).lo, 2.0);
  EXPECT_DOUBLE_EQ(PointCurveAxisFit(curve).hi, 2.0);
  curve.points[0].area = 1e-320;  // underflow of the relative floor must not erase real positives
  EXPECT_DOUBLE_EQ(PointCurveAxisFit(curve).lo, 1e-320);
  curve.points[0].area_kind = K::kExactZero;
  EXPECT_FALSE(PointCurveAxisFit(curve).valid);
}

TEST(AnalyzePointView, CurveOutsideCountsPointsNotArmsAndKeepsHonestExtrema) {
  using K = PointCurveValueKind;
  PointContributionCurve curve;
  curve.points.resize(3);
  curve.points[0].rho_kind = curve.points[0].product_kind = K::kPositive;
  curve.points[0].rho = 1e-260;
  curve.points[0].product = 1e-100;
  curve.points[1].area_kind = K::kPositive;
  curve.points[1].area = 8.0;
  curve.points[2].rho_kind = K::kExactZero;
  curve.points[2].product = 1e-300;  // unavailable, not counted
  const PointCurveOutsideCount fit = PointCurveCountOutside(curve, 1.0, 8.0);
  EXPECT_EQ(fit.points, 1);
  EXPECT_DOUBLE_EQ(fit.min, 1e-260);
  EXPECT_DOUBLE_EQ(fit.max, 1e-100);
  EXPECT_FALSE(fit.above);
  const PointCurveOutsideCount zoom = PointCurveCountOutside(curve, 1.0, 4.0);
  EXPECT_EQ(zoom.points, 2);
  EXPECT_DOUBLE_EQ(zoom.min, 1e-260);
  EXPECT_DOUBLE_EQ(zoom.max, 8.0);
  EXPECT_TRUE(zoom.above);
  EXPECT_EQ(PointCurveOutsideLabel(zoom), "2 points outside axis · min 1e-260 · max 8");
  EXPECT_TRUE(PointCurveOutsideLabel({}).empty());
}

TEST(AnalyzePointView, CurveWindowZoomsAtAnchorClampsAndResetsOnAdoption) {
  const PointCurveAxisRange fit{ true, 1e-6, 1.0 };
  PointViewState view;
  const PointCurveAxisRange initial = PointCurveVisibleRange(fit, &view.curve_axis);
  EXPECT_DOUBLE_EQ(initial.lo, fit.lo);
  EXPECT_DOUBLE_EQ(initial.hi, fit.hi);
  const double initial_span = view.curve_axis.log_hi - view.curve_axis.log_lo;
  const PointCurveAxisRange zoomed = PointCurveVisibleRange(fit, &view.curve_axis, 1.0, 0.25);
  EXPECT_NEAR(view.curve_axis.log_hi - view.curve_axis.log_lo, initial_span / 1.12, 1e-12);
  EXPECT_NEAR(view.curve_axis.log_lo + 0.25 * (view.curve_axis.log_hi - view.curve_axis.log_lo), -4.5, 1e-12);
  EXPECT_GT(zoomed.lo, fit.lo);
  EXPECT_LT(zoomed.hi, fit.hi);
  PointCurveVisibleRange(fit, &view.curve_axis, 10000.0, 0.0);
  EXPECT_NEAR(view.curve_axis.log_hi - view.curve_axis.log_lo, 0.25, 1e-12);
  const PointCurveAxisRange restored = PointCurveVisibleRange(fit, &view.curve_axis, -10000.0);
  EXPECT_DOUBLE_EQ(restored.lo, fit.lo);
  EXPECT_DOUBLE_EQ(restored.hi, fit.hi);
  // Component changes reconcile in place, including a new span narrower than the zoom floor.
  const PointCurveAxisRange narrow{ true, 10.0, 11.0 };
  EXPECT_DOUBLE_EQ(PointCurveVisibleRange(narrow, &view.curve_axis).lo, 10.0);
  EXPECT_DOUBLE_EQ(PointCurveVisibleRange(narrow, &view.curve_axis, 1000.0).hi, 11.0);
  const PointCurveAxisRange one = PointCurveVisibleRange({ true, 2.0, 2.0 }, &view.curve_axis, 10.0);
  EXPECT_DOUBLE_EQ(one.lo, 2.0);
  EXPECT_DOUBLE_EQ(one.hi, 2.0);
  view.orbit_zoom = 2.0f;
  ResetPointViewStateForNewAdoption(&view);
  EXPECT_DOUBLE_EQ(view.curve_axis.log_lo, 0.0);
  EXPECT_DOUBLE_EQ(view.curve_axis.log_hi, 0.0);
  EXPECT_FLOAT_EQ(view.orbit_zoom, 2.0f);  // the 3D camera still survives
  EXPECT_FALSE(PointCurveVisibleRange({}, &view.curve_axis).valid);
}

TEST(AnalyzePointView, CurveWindowBreaksRunsAtBothBoundsWithoutDroppingSingletons) {
  using K = PointCurveValueKind;
  const K kinds[] = { K::kPositive, K::kPositive, K::kPositive, K::kPositive, K::kPositive, K::kExactZero };
  const double values[] = { 1.0, 1e-260, 2.0, 10.0, 4.0, 0.0 };
  const auto runs = PointCurvePositiveRuns(kinds, 6, values, 1.0, 4.0);
  ASSERT_EQ(runs.size(), 3U);
  EXPECT_EQ(runs[0].begin, 0);
  EXPECT_EQ(runs[0].end, 1);
  EXPECT_EQ(runs[1].begin, 2);
  EXPECT_EQ(runs[1].end, 3);
  EXPECT_EQ(runs[2].begin, 4);
  EXPECT_EQ(runs[2].end, 5);
}

TEST(AnalyzePointView, CurveLogTicksSpanDecadesWithSubTicks) {
  const std::vector<PointCurveLogTick> ticks = PointCurveLogTicks(1e-6, 1e-3);
  int majors = 0;
  int minors = 0;
  for (size_t i = 0; i < ticks.size(); ++i) {
    EXPECT_GT(ticks[i].value, i > 0 ? ticks[i - 1].value : 0.0);  // monotone increasing
    EXPECT_GE(ticks[i].value, 1e-6);
    EXPECT_LE(ticks[i].value, 1e-3);
    ticks[i].major ? ++majors : ++minors;
  }
  EXPECT_EQ(majors, 4);  // 1e-6, 1e-5, 1e-4, 1e-3
  EXPECT_EQ(minors, 6);  // the 2x/5x inside the band: 1e-3's own 2x and 5x sit above hi — clipped
  EXPECT_NEAR(ticks[0].value, 1e-6, 1e-30);
  EXPECT_TRUE(ticks[0].major);
  // A band degenerate to lo == hi widens to one decade: a single value draws inside a range.
  const std::vector<PointCurveLogTick> one = PointCurveLogTicks(2.5, 2.5);
  ASSERT_FALSE(one.empty());
  EXPECT_GE(one.size(), 2U);
  // Nothing positive to span answers empty — the renderer's honest no-range note.
  EXPECT_TRUE(PointCurveLogTicks(0.0, 1.0).empty());
  EXPECT_TRUE(PointCurveLogTicks(-1.0, 1.0).empty());
  EXPECT_TRUE(PointCurveLogTicks(5.0, 1.0).empty());
}

TEST(AnalyzePointView, CurveMappingHelpersAnswerSafelyOnAnEmptyCurve) {
  PointContributionCurve empty;
  EXPECT_EQ(PointCurveStoredAt(empty, 0), -1);
  EXPECT_EQ(PointCurveXAt(empty, 0), 0.0);
  EXPECT_EQ(PointCurveValidPositionNearX(empty, 1.0), -1);
  // Out-of-range positions on a real-shaped but empty-of-points curve answer the same way.
  empty.ok = true;
  empty.valid_positions = { 3 };
  empty.points.resize(5);
  empty.points[3].x_rad = 2.0;
  EXPECT_EQ(PointCurveStoredAt(empty, 0), 3);
  EXPECT_NEAR(PointCurveXAt(empty, 0), 2.0, 1e-12);
  EXPECT_EQ(PointCurveStoredAt(empty, 1), -1);
  EXPECT_EQ(PointCurveStoredAt(empty, -1), -1);
  EXPECT_EQ(PointCurveValidPositionNearX(empty, 100.0), 0);  // the only valid sample wins
  // Ties pick the lower valid position (strict less-than in the walk).
  empty.valid_positions = { 0, 4 };
  empty.points[0].x_rad = 0.0;
  empty.points[4].x_rad = 4.0;
  EXPECT_EQ(PointCurveValidPositionNearX(empty, 2.0), 0);
  EXPECT_EQ(PointCurveValidPositionNearX(empty, 2.01), 1);
}

TEST(AnalyzePointView, CurveLogLabelStepBoundsVisibleLabels) {
  // The live worst case (review round 1): a real fiber spanning ~213 decades on a ~200px plot
  // must collapse to a handful of evenly spaced labelled exponents, not 213 overlapping texts.
  const int step = PointCurveLogLabelStep(1.1287457567142152e-213, 1.6273993240820983, 200.0f, 15.0f);
  EXPECT_GE(step, 20);
  EXPECT_LE(step, 30);
  // A three-decade band labels every decade; degenerate and out-of-domain bands answer 1.
  EXPECT_EQ(PointCurveLogLabelStep(1e-6, 1e-3, 200.0f, 15.0f), 1);
  EXPECT_EQ(PointCurveLogLabelStep(2.5, 2.5, 200.0f, 15.0f), 1);
  EXPECT_EQ(PointCurveLogLabelStep(0.0, 1.0, 200.0f, 15.0f), 1);
  // A shorter plot labels fewer decades: the step grows as the plot shrinks.
  const int tall = PointCurveLogLabelStep(1e-6, 1e6, 300.0f, 15.0f);
  const int short_plot = PointCurveLogLabelStep(1e-6, 1e6, 60.0f, 15.0f);
  EXPECT_GE(short_plot, tall);
}

TEST(AnalyzePointView, CurvePositiveRunsKeepIsolatedPositivesAndBreakAtGaps) {
  using K = PointCurveValueKind;
  // P P U P: one 2-point run plus one isolated positive — nothing dropped.
  const K a[] = { K::kPositive, K::kPositive, K::kUnavailable, K::kPositive };
  const std::vector<PointCurveRun> ra = PointCurvePositiveRuns(a, 4);
  ASSERT_EQ(ra.size(), 2U);
  EXPECT_EQ(ra[0].begin, 0);
  EXPECT_EQ(ra[0].end, 2);
  EXPECT_EQ(ra[1].begin, 3);
  EXPECT_EQ(ra[1].end, 4);
  // U P U (the review's disappearing-contribution pattern): exactly one isolated run.
  const K b[] = { K::kUnavailable, K::kPositive, K::kUnavailable };
  const std::vector<PointCurveRun> rb = PointCurvePositiveRuns(b, 3);
  ASSERT_EQ(rb.size(), 1U);
  EXPECT_EQ(rb[0].begin, 1);
  EXPECT_EQ(rb[0].end, 2);
  // P U P: two isolated runs — a gap is never bridged.
  const K c[] = { K::kPositive, K::kUnavailable, K::kPositive };
  const std::vector<PointCurveRun> rc = PointCurvePositiveRuns(c, 3);
  ASSERT_EQ(rc.size(), 2U);
  EXPECT_EQ(rc[0].begin, 0);
  EXPECT_EQ(rc[1].begin, 2);
  // An exact zero breaks a run too: zeros live on the zero row, not on the log axis.
  const K d[] = { K::kPositive, K::kExactZero, K::kPositive };
  EXPECT_EQ(PointCurvePositiveRuns(d, 3).size(), 2U);
  // No positives, no runs; null kinds answer empty.
  const K e[] = { K::kUnavailable, K::kExactZero };
  EXPECT_EQ(PointCurvePositiveRuns(e, 2).size(), 0U);
  EXPECT_EQ(PointCurvePositiveRuns(nullptr, 3).size(), 0U);
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
    PumpPointQuery(state, executor, now);
    PumpRayTrace(state, executor);
    now += 0.05;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  PumpPointQuery(state, executor, now);
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
  PumpPointQuery(state, executor, 100.0);
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
  PumpPointQuery(state, executor, 100.0);
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
  PumpPointQuery(state, executor, 100.0);
  EXPECT_EQ(PointQueryStateFor(state.analysis.point), PointQueryState::kNoIntent);
  // With both, the intent issues.
  SetSkyPoint(state, 22.0, 0.0);
  PumpPointQuery(state, executor, 100.0);
  EXPECT_TRUE(state.analysis.point.intent_valid);
  // The intent dies with the sky point (the user cleared it): the session is dropped, adopted
  // outcome included.
  ASSERT_TRUE(
      PumpAllUntil(state, executor, 100.0, [&] { return state.analysis.point.adopted_result != nullptr; }, 60000));
  state.analysis.sky_point.valid = false;
  PumpPointQuery(state, executor, 100.0);
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
  PumpPointQuery(state, executor, 100.0);
  ASSERT_TRUE(state.analysis.point.intent_valid);
  const SurveyQueryId issued = state.analysis.point.intent_id;
  EXPECT_EQ(state.analysis.point.intent_start_s, 100.0);  // the wait display's epoch (A-layer)
  // The cancel button's rows (the details block's own shape): withdraw, latch, stop.
  executor.CancelInFlight(QueryFace::kPoint);
  state.analysis.point.cancelled_for_key = true;
  state.analysis.point.adopted_result.reset();
  state.analysis.point.adopted_error.clear();
  // The pump must NOT re-issue while the same key stands — the latch is the button's meaning.
  for (int i = 0; i < 5; ++i) {
    PumpPointQuery(state, executor, 100.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  EXPECT_TRUE(state.analysis.point.intent_valid);
  EXPECT_EQ(state.analysis.point.intent_id, issued);
  EXPECT_EQ(state.analysis.point.adopted_result, nullptr);
  EXPECT_EQ(PointQueryStateFor(state.analysis.point), PointQueryState::kCancelled);
  // A moved sky point starts fresh: the latch clears with the old intent, and the wait epoch
  // restarts with it (the rekey-restarts rule, plan §3.1 — a later clock, a younger stamp).
  SetSkyPoint(state, 21.9, 60.0);
  PumpPointQuery(state, executor, 130.0);
  EXPECT_TRUE(state.analysis.point.intent_valid);
  EXPECT_FALSE(state.analysis.point.cancelled_for_key);
  EXPECT_EQ(state.analysis.point.intent_start_s, 130.0);
  executor.ShutdownAndJoin();
}

// ---- The r1 revision's legs: the physics pin, the refusal mapping, the named refusals, and the
// partial document — the plan's Step 5/7 verification points over real engine documents. ----------

// C03's internal-reflection chain (3-5-6-7) at the M0-verified on-fiber target: the polyline's
// multi-segment assembly is pinned to the 742.2 recovery's own shape — four contacts, the
// entry/internal/internal/internal/exit kinds, one leg more than contacts — the oracle the plan
// names (742.2's verified pins), not a screenshot.
TEST(AnalyzePointView, InternalReflectionPolylineCarriesThePinShape) {
  LUMICE_Scene* raw_scene = nullptr;
  ASSERT_EQ(LUMICE_SceneFromJson(kPointSceneJson, &raw_scene), LUMICE_OK);
  std::unique_ptr<LUMICE_Scene, SceneDeleter> scene(raw_scene);
  SurveyExecutor executor;
  executor.SetScene(std::move(scene));

  GuiState state;
  state.analysis_result.payload = Rows(7, { "3-5-6-7" }, { { 3, 5, 6, 7 } });
  state.analysis_result.analyzed_scene = GuiState::AnalysisSceneIdentity::From(state);
  state.analysis.selected_entry = "3-5-6-7";
  SetSkyPoint(state, 38.0, 180.0);  // the C03 fiber anchor the M0 probe verified live
  const double now = 100.0;
  ASSERT_TRUE(
      PumpAllUntil(state, executor, now, [&] { return state.analysis.point.adopted_result != nullptr; }, 60000));
  LUMICE_PathPointInfo info{};
  info.struct_size = sizeof(info);
  ASSERT_EQ(LUMICE_PathPointGetInfo(state.analysis.point.adopted_result.get(), &info), LUMICE_PATH_READ_OK);
  ASSERT_EQ(info.outcome, LUMICE_PATH_POINT_DISCOVERED);
  ASSERT_EQ(info.face_count, 4);
  ASSERT_GE(info.component_count, 1U);
  ASSERT_GT(PointValidSampleCount(*state.analysis.point.adopted_result, 0), 0);
  ASSERT_TRUE(
      PumpAllUntil(state, executor, now, [&] { return state.analysis.trace.adopted_result != nullptr; }, 30000));
  RayTraceKey current;
  current.point_id = state.analysis.point.intent_id;
  current.component = 0;
  current.sample = state.analysis.trace.key.sample;
  EXPECT_EQ(RayTraceDisplayStateFor(state.analysis.trace, current), RayTraceDisplayState::kDrawn);
  // The pin shape: 4 contacts -> 5 polyline segments (entry + 3 internal + exit), kinds in the
  // recovery's own order, contacts = faces.
  RayTracePolyline line;
  ASSERT_TRUE(BuildRayTracePolyline(*state.analysis.trace.adopted_result, 1.1f, &line));
  EXPECT_EQ(line.seg_kinds.size(), 5U);
  EXPECT_EQ(line.points.size(), 30U);
  EXPECT_EQ(line.seg_kinds[0], 0);
  EXPECT_EQ(line.seg_kinds[1], 1);
  EXPECT_EQ(line.seg_kinds[2], 1);
  EXPECT_EQ(line.seg_kinds[3], 1);
  EXPECT_EQ(line.seg_kinds[4], 2);
  // The source readers agree: one contact per face of the sequence, legs one more.
  LUMICE_PathRayContacts contacts{};
  contacts.struct_size = sizeof(contacts);
  LUMICE_PathRayLegs legs{};
  legs.struct_size = sizeof(legs);
  ASSERT_EQ(LUMICE_RepresentativeRayGetContacts(state.analysis.trace.adopted_result.get(), &contacts),
            LUMICE_PATH_READ_OK);
  ASSERT_EQ(LUMICE_RepresentativeRayGetLegs(state.analysis.trace.adopted_result.get(), &legs), LUMICE_PATH_READ_OK);
  EXPECT_EQ(contacts.contact_count, 4U);
  EXPECT_EQ(legs.leg_count, 5U);
  // The polyline's own pairing — the invariant the 3D view's contact markers draw from: the odd
  // vertices ARE the recovery's contacts (index 0 and the last are the display extensions, which
  // must not masquerade as face contacts).
  for (size_t c = 0; c < contacts.contact_count; ++c) {
    const size_t idx = (c * 2 + 1) * 3;
    for (int k = 0; k < 3; ++k) {
      EXPECT_NEAR(line.points[idx + k], static_cast<float>(contacts.contacts[c].point[k]), 1e-3f);
    }
  }
  executor.ShutdownAndJoin();
}

// The refusal mapping: the corridor's own named refusal (the analytic suite's entry_backface pin
// pose, on the pin scene's sun) maps to the kRefused display state, and the polyline builder
// refuses to synthesize a curve from a refusal arm.
TEST(AnalyzePointView, RefusalPoseMapsToTheNamedRefusedState) {
  // The pin scene's own incident direction: sun at altitude 15, azimuth 0 (the analytic pins').
  constexpr const char* kRefusalSceneJson = R"({
    "crystal": [ { "id": 1, "type": "prism",
                   "shape": { "height": 1.0, "face_distance": [1,1,1,1,1,1] },
                   "axis": { "zenith": {"type": "uniform", "mean": 0, "std": 360},
                             "azimuth": {"type": "uniform", "mean": 0, "std": 360},
                             "roll": {"type": "uniform", "mean": 0, "std": 360} } } ],
    "filter": [],
    "scene": { "light_source": { "type": "sun", "altitude": 15, "azimuth": 0, "spectrum": "D65" },
               "ray_num": 1000, "max_hits": 7,
               "scattering": [ { "prob": 0, "entries": [ { "crystal": 1, "proportion": 1 } ] } ] },
    "render": []
  })";
  LUMICE_Scene* raw_scene = nullptr;
  ASSERT_EQ(LUMICE_SceneFromJson(kRefusalSceneJson, &raw_scene), LUMICE_OK);
  std::unique_ptr<LUMICE_Scene, SceneDeleter> scene(raw_scene);
  // The corpus's entry_backface pin pose (test_representative_ray.cpp's kBackface, the analytic
  // suite's own refusal matrix row).
  static const double kBackface[9] = { -0.5108181656453654, -0.8569178335521683,  -0.06896831291950242,
                                       0.6557541851097652,  -0.3365085527995817,  -0.6758316673578998,
                                       0.5559237810687107,  -0.39045335244173474, 0.7338221373120865 };
  static const int kFaces[2] = { 3, 5 };
  static const int kLayerCounts[1] = { 2 };
  LUMICE_RepresentativeRayRequest request{};
  request.struct_size = sizeof(request);
  request.crystal_id = 1;
  request.faces = kFaces;
  request.face_count = 2;
  request.layer_face_counts = kLayerCounts;
  request.layer_count = 1;
  request.pose = kBackface;
  LUMICE_RepresentativeRay* raw_ray = nullptr;
  char err[256] = {};
  ASSERT_EQ(LUMICE_AnalyzeRepresentativeRay(scene.get(), &request, &raw_ray, err, sizeof(err)), LUMICE_OK)
      << err;  // a corridor refusal rides the RESULT, not an error
  RepresentativeRayHandle ray(raw_ray, RepresentativeRayDeleter{});
  LUMICE_RepresentativeRayInfo rinfo{};
  rinfo.struct_size = sizeof(rinfo);
  ASSERT_EQ(LUMICE_RepresentativeRayGetInfo(ray.get(), &rinfo), LUMICE_PATH_READ_OK);
  EXPECT_EQ(rinfo.produced, 0);
  EXPECT_EQ(std::string(rinfo.reason.data, rinfo.reason.length), "entry_backface");
  // The display mapping: the session holding this handle reads kRefused, and the display
  // polyline builder refuses to synthesize anything from the refusal arm.
  RayTraceSession trace;
  trace.intent_valid = true;
  trace.key = RayTraceKey{ SurveyQueryId{ 1, 1 }, 0, 0 };
  trace.adopted_result = ray;
  const RayTraceKey same = trace.key;
  EXPECT_EQ(RayTraceDisplayStateFor(trace, same), RayTraceDisplayState::kRefused);
  RayTracePolyline line;
  EXPECT_FALSE(BuildRayTracePolyline(*ray, 1.1f, &line));
  EXPECT_TRUE(line.points.empty());
}

// The multi-layer selection: the point face's request-level refusal IS the answer (AC5), named
// like the survey face's, and the session holds it as a refusal — not a fabricated empty result.
TEST(AnalyzePointView, MultiLayerSelectionRefusesByName) {
  LUMICE_Scene* raw_scene = nullptr;
  ASSERT_EQ(LUMICE_SceneFromJson(kPointSceneJson, &raw_scene), LUMICE_OK);
  std::unique_ptr<LUMICE_Scene, SceneDeleter> scene(raw_scene);
  SurveyExecutor executor;
  executor.SetScene(std::move(scene));

  GuiState state;
  // The row's chain is ONE two-layer chain, spelled directly (the Rows helper writes single-layer
  // rows and the payload shares as const): identity faces {3,5} + {1,2}, counts {2,2}.
  {
    auto payload = std::make_shared<AnalysisPayload>();
    payload->snapshot_generation = 7;
    LUMICE_RaypathHistogramEntry row;
    std::memset(&row, 0, sizeof(row));
    row.chain[0].crystal_id = 1;
    row.chain[0].segment[0] = 3;
    row.chain[0].segment[1] = 5;
    row.chain[0].segment_len = 2;
    row.chain[1].crystal_id = 1;
    row.chain[1].segment[0] = 1;
    row.chain[1].segment[1] = 2;
    row.chain[1].segment_len = 2;
    row.chain_len = 2;
    std::snprintf(row.display, sizeof(row.display), "3-5 -> 1-2");
    payload->entries.push_back(row);
    state.analysis_result.payload = payload;
  }
  state.analysis_result.analyzed_scene = GuiState::AnalysisSceneIdentity::From(state);
  state.analysis.selected_entry = "3-5 -> 1-2";
  SetSkyPoint(state, 22.0, 0.0);
  const double now = 100.0;
  ASSERT_TRUE(PumpAllUntil(state, executor, now, [&] { return !state.analysis.point.adopted_error.empty(); }, 30000));
  EXPECT_EQ(PointQueryStateFor(state.analysis.point), PointQueryState::kRefused);
  EXPECT_NE(state.analysis.point.adopted_error.find("multi_layer"), std::string::npos);
  EXPECT_EQ(state.analysis.point.adopted_result, nullptr);
  // No trace follows a refusal: the ray pump drops its session without an adopted point.
  EXPECT_FALSE(state.analysis.trace.intent_valid);
  executor.ShutdownAndJoin();
}

// The partial document (the C17 shape: what arrived is kept and readable AS partial): a
// 1 ms-budget point query stops honestly — the document adopts (a partial IS an answer), its
// execution tag reads not-completed, and the caliber lines spell it.
//
// The query rides the workspace face — the one the GUI executor and the CLI both consume — where
// the deadline rides LUMICE_PathQueryOptions and propagates into the inline survey context. The
// direct LUMICE_AnalyzeSinglePath entry would also honor the 1 ms budget for the discovery legs,
// but its context is the legacy cancel-only shape: nothing cuts the inline survey there, so a
// 1 ms query pays a full uncut survey (~13 s on this fixture) for context the test never reads.
TEST(AnalyzePointView, PartialDocumentAdoptsAndReadsAsPartial) {
  LUMICE_Scene* raw_scene = nullptr;
  ASSERT_EQ(LUMICE_SceneFromJson(kPointSceneJson, &raw_scene), LUMICE_OK);
  std::unique_ptr<LUMICE_Scene, SceneDeleter> scene(raw_scene);
  LUMICE_PathWorkspace* raw_workspace = nullptr;
  char ws_err[512] = {};
  ASSERT_EQ(LUMICE_PathWorkspaceCreate(scene.get(), &raw_workspace, ws_err, sizeof(ws_err)), LUMICE_OK) << ws_err;
  std::unique_ptr<LUMICE_PathWorkspace, void (*)(LUMICE_PathWorkspace*)> workspace(raw_workspace,
                                                                                   LUMICE_PathWorkspaceDestroy);
  static const int kFaces[2] = { 3, 5 };
  static const int kLayerCounts[1] = { 2 };
  LUMICE_SinglePathRequest request{};
  request.struct_size = sizeof(request);
  request.crystal_id = 1;
  request.faces = kFaces;
  request.face_count = 2;
  request.layer_face_counts = kLayerCounts;
  request.layer_count = 1;
  request.target_altitude_deg = 22.0;
  request.target_azimuth_deg = 0.0;
  request.sample_count = kPointQuerySampleCount;
  request.sun_grid_lat_count = kPointSunGridLatCount;
  // The deadline rides the options on this face (the request body's deadline_ms is the direct
  // entry's field); 1 ms: the stop is the point — what arrives is the partial document.
  LUMICE_PathQueryOptions options{};
  options.struct_size = sizeof(options);
  options.deadline_ms = 1;
  LUMICE_SinglePathResult* raw_result = nullptr;
  char err[256] = {};
  ASSERT_EQ(LUMICE_PathWorkspaceQueryPoint(workspace.get(), &request, &options, &raw_result, err, sizeof(err)),
            LUMICE_OK)
      << err;
  PointHandle result(raw_result, PointDeleter{});
  LUMICE_PathPointInfo info{};
  info.struct_size = sizeof(info);
  ASSERT_EQ(LUMICE_PathPointGetInfo(result.get(), &info), LUMICE_PATH_READ_OK);
  EXPECT_NE(info.execution, LUMICE_PATH_OUTCOME_COMPLETED);
  // The session accepts a partial as its adopted answer (the honest four-state machinery reads
  // the document, whatever completeness it names), and the caliber lines say partial out loud.
  PointQuerySession point;
  point.intent_valid = true;
  point.intent_id = SurveyQueryId{ 1, 1 };
  point.adopted_result = result;
  EXPECT_EQ(PointQueryStateFor(point), PointQueryState::kCompleted);
  const std::vector<std::string> lines = PointCaliberLines(info);
  std::string joined;
  for (const std::string& l : lines) {
    joined += l + "\n";
  }
  EXPECT_NE(joined.find("(partial)"), std::string::npos);
}

// ---- The contribution curve over REAL engine documents (756, audit §5's contract) --------------------

// The axis/echo/classification contract on the C01 random prism (angle-Lebesgue measure, rho > 0):
// the axis is the increments' cumsum with x[0]=0, the valid walk agrees with the slider's own
// map, the classification invariants hold per point, the model's product is coherent with its
// own arms at all-positive points (a model-internal identity — the physics is the engine's),
// and the cursor mapping round-trips through the real x values.
TEST(AnalyzePointView, ContributionCurveBuildsFromTheRandomPrismDocument) {
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
  const LUMICE_SinglePathResult& result = *state.analysis.point.adopted_result;

  PointContributionCurve curve;
  ASSERT_TRUE(BuildPointContributionCurve(result, 0, &curve));
  ASSERT_TRUE(curve.ok);
  ASSERT_FALSE(curve.points.empty());
  // The echo: the component row's own words, verbatim on both faces.
  std::vector<PointComponentRow> rows;
  ASSERT_GE(PointComponentRows(result, &rows), 1);
  EXPECT_EQ(curve.kind, rows[0].kind);
  EXPECT_EQ(curve.total_points, rows[0].total_points);
  EXPECT_EQ(curve.available_points, rows[0].available_points);
  EXPECT_EQ(curve.integral_available, rows[0].integral_available);

  // The axis: x[0]=0, monotone non-negative, exactly N entries, equal to the increments' cumsum
  // read back from the component itself (N points over N-1 per-step increments).
  EXPECT_EQ(curve.points[0].x_rad, 0.0);
  LUMICE_PathPointComponent head{};
  head.struct_size = sizeof(head);
  ASSERT_EQ(LUMICE_PathPointGetComponent(&result, 0, &head), LUMICE_PATH_READ_OK);
  EXPECT_EQ(curve.points.size(), head.point_count);
  ASSERT_EQ(head.arclength_count + 1, head.point_count);
  double cumsum = 0.0;
  for (size_t i = 0; i < head.arclength_count; ++i) {
    cumsum += head.arclength_increments[i];
    EXPECT_NEAR(curve.points[i + 1].x_rad, cumsum, 1e-12) << "point " << i + 1;
    EXPECT_GE(curve.points[i + 1].x_rad, curve.points[i].x_rad) << "monotone at " << i + 1;
  }
  EXPECT_EQ(curve.seed_index, head.seed_index);

  // The valid walk: one truth with the slider's own map.
  EXPECT_EQ(curve.valid_positions, CollectValidSampleIndices(result, 0));

  // Per-point classification invariants (the flags decide availability; the arms stay coherent).
  bool any_positive_rho = false;
  bool any_positive_product = false;
  for (const PointCurvePoint& p : curve.points) {
    EXPECT_NE(p.rho_kind, PointCurveValueKind::kUnavailable);  // rho is produced unconditionally
    any_positive_rho = any_positive_rho || p.rho_kind == PointCurveValueKind::kPositive;
    if (p.product_kind == PointCurveValueKind::kPositive) {
      any_positive_product = true;
      EXPECT_TRUE(p.valid);
      EXPECT_NE(p.jacobian_available, 0);
      EXPECT_TRUE(p.product_kind == PointCurveClassify(p.product, p.valid && p.jacobian_available));
    }
    if (p.area_kind == PointCurveValueKind::kExactZero) {
      EXPECT_TRUE(p.valid);  // a zero behind a clear gate is the clear, never this classification
      EXPECT_EQ(p.area, 0.0);
    }
  }
  EXPECT_TRUE(any_positive_rho);
  EXPECT_TRUE(any_positive_product);

  // Model-internal coherence at all-positive points: the engine's product identity against the
  // arms the SAME document carries (not a physics recomputation — the engine's own arithmetic).
  for (const PointCurvePoint& p : curve.points) {
    if (p.rho_kind == PointCurveValueKind::kPositive && p.area_kind == PointCurveValueKind::kPositive &&
        p.transmission_kind == PointCurveValueKind::kPositive && p.k_kind == PointCurveValueKind::kPositive) {
      EXPECT_EQ(p.product_kind, PointCurveValueKind::kPositive);
      const double expect = p.rho * p.area * p.transmission * p.k_carea;
      EXPECT_NEAR(p.product, expect, std::abs(expect) * 1e-9 + 1e-300);
    }
  }

  // The cursor mapping round-trips through the real x values (sampled; midpoints biased toward
  // the lower neighbour land on it while the axis is strictly increasing there).
  const int valid_count = static_cast<int>(curve.valid_positions.size());
  const int step = std::max(1, valid_count / 8);
  for (int v = 0; v < valid_count; v += step) {
    EXPECT_EQ(PointCurveValidPositionNearX(curve, PointCurveXAt(curve, v)), v) << "position " << v;
    if (v + 1 < valid_count) {
      const double x_lo = PointCurveXAt(curve, v);
      const double x_hi = PointCurveXAt(curve, v + 1);
      if (x_hi > x_lo) {
        EXPECT_EQ(PointCurveValidPositionNearX(curve, x_lo + 0.25 * (x_hi - x_lo)), v);
      }
    }
  }
  // The random prism's declared measure registers no read-as-zero gap (J7's negative arm).
  LUMICE_PathMeasureRegistration reg{};
  reg.struct_size = sizeof(reg);
  ASSERT_EQ(LUMICE_PathPointGetMeasureRegistration(&result, &reg), LUMICE_PATH_READ_OK);
  EXPECT_EQ(reg.registered_gap, 0);
  executor.ShutdownAndJoin();
}

// The plate family (fixed axis, the C11 corpus shape): the angle-Lebesgue caliber's declared
// honesty — rho reads identically zero (the legal exact zero, never absence), the product
// inherits it, the other arms stay informative per their own flags, the measure registration's
// note is retrievable verbatim (J7's trigger), and the arc's seed sits mid-curve.
TEST(AnalyzePointView, ContributionCurveReadsThePlateMeasureHonestly) {
  constexpr const char* kPlateSceneJson = R"({
    "crystal": [ { "id": 1, "type": "prism",
                   "shape": { "height": 0.3, "face_distance": [1,1,1,1,1,1] },
                   "axis": { "zenith": 0,
                             "azimuth": { "type": "uniform", "mean": 0, "std": 360 },
                             "roll": 0 } } ],
    "filter": [],
    "scene": { "light_source": { "type": "sun", "altitude": 20, "azimuth": 0, "diameter": 0.5,
                                 "spectrum": [ { "wavelength": 550, "weight": 1 } ] },
               "ray_num": 1000, "max_hits": 7,
               "scattering": [ { "prob": 0, "entries": [ { "crystal": 1, "proportion": 1 } ] } ] },
    "render": []
  })";
  LUMICE_Scene* raw_scene = nullptr;
  ASSERT_EQ(LUMICE_SceneFromJson(kPlateSceneJson, &raw_scene), LUMICE_OK);
  std::unique_ptr<LUMICE_Scene, SceneDeleter> scene(raw_scene);
  SurveyExecutor executor;
  executor.SetScene(std::move(scene));

  GuiState state;
  state.analysis_result.payload = Rows(7, { "1-4-5-2" }, { { 1, 4, 5, 2 } });
  state.analysis_result.analyzed_scene = GuiState::AnalysisSceneIdentity::From(state);
  state.analysis.selected_entry = "1-4-5-2";
  SetSkyPoint(state, 20.0, 120.0);  // the C11 corpus anchor (20, 120)
  const double now = 100.0;
  ASSERT_TRUE(
      PumpAllUntil(state, executor, now, [&] { return state.analysis.point.adopted_result != nullptr; }, 60000));
  const LUMICE_SinglePathResult& result = *state.analysis.point.adopted_result;

  PointContributionCurve curve;
  ASSERT_TRUE(BuildPointContributionCurve(result, 0, &curve));
  ASSERT_TRUE(curve.ok);
  ASSERT_FALSE(curve.points.empty());
  EXPECT_GT(curve.points.back().x_rad, 0.0);  // an arc's axis spans a positive length
  EXPECT_GT(curve.seed_index, 0);             // the arc's seed sits mid-curve, not at the zero

  int positive_area = 0;
  int zero_area = 0;
  int positive_k = 0;
  int positive_t = 0;
  for (const PointCurvePoint& p : curve.points) {
    // The declared measure's rho: identically zero — the legal exact zero, never unavailable,
    // never absent (audit §4's plate row).
    EXPECT_EQ(p.rho_kind, PointCurveValueKind::kExactZero) << "rho at a plate point";
    // The product inherits the zero wherever its gate stands; it is never positive here.
    EXPECT_NE(p.product_kind, PointCurveValueKind::kPositive);
    if (p.product_kind == PointCurveValueKind::kExactZero) {
      EXPECT_TRUE(p.valid);
      EXPECT_NE(p.jacobian_available, 0);
    }
    // The other arms per their own flags (audit §4 live: T and K strictly positive on the arc,
    // A/a^2 zero exactly at the valid endpoints).
    positive_area += p.area_kind == PointCurveValueKind::kPositive ? 1 : 0;
    zero_area += p.area_kind == PointCurveValueKind::kExactZero ? 1 : 0;
    positive_k += p.k_kind == PointCurveValueKind::kPositive ? 1 : 0;
    positive_t += p.transmission_kind == PointCurveValueKind::kPositive ? 1 : 0;
  }
  EXPECT_GE(zero_area, 2);  // the arc's two valid endpoints carry the legal zero-area
  EXPECT_GE(positive_area, 1);
  EXPECT_GE(positive_k, 1);
  EXPECT_GE(positive_t, 1);

  // The measure registration answers its reader with a named support (J7's data source). The
  // registration's `registered_gap`/`note` pair carries the engine's v1 read-as-zero gap list,
  // which does not name this C11 shape (zenith Dirac at a pole + uniform spin) — the live fact
  // is that rho still reads identically zero (pinned above); the renderer's note trigger stays
  // the engine's own gap flag, never a GUI-side plate detector.
  LUMICE_PathMeasureRegistration reg{};
  reg.struct_size = sizeof(reg);
  ASSERT_EQ(LUMICE_PathPointGetMeasureRegistration(&result, &reg), LUMICE_PATH_READ_OK);
  EXPECT_GT(reg.support_kind.length, 0U);
  executor.ShutdownAndJoin();
}

// Bounded churn (AC6's resource bound, the session-level leg): five point moves pumped back to
// back leave exactly ONE standing intent — the last key's — and the answer that arrives is the
// last move's. The executor's single slot per face is the structural half; this pins the
// session's half.
TEST(AnalyzePointView, RapidPointMovesStayBoundedAtOneIntent) {
  LUMICE_Scene* raw_scene = nullptr;
  ASSERT_EQ(LUMICE_SceneFromJson(kPointSceneJson, &raw_scene), LUMICE_OK);
  std::unique_ptr<LUMICE_Scene, SceneDeleter> scene(raw_scene);
  SurveyExecutor executor;
  executor.SetScene(std::move(scene));

  GuiState state;
  state.analysis_result.payload = Rows(7, { "3-5" }, { { 3, 5 } });
  state.analysis_result.analyzed_scene = GuiState::AnalysisSceneIdentity::From(state);
  state.analysis.selected_entry = "3-5";
  for (int i = 0; i < 5; ++i) {
    SetSkyPoint(state, 21.5 + static_cast<double>(i), 0.0);
    PumpPointQuery(state, executor, 100.0);
  }
  EXPECT_TRUE(state.analysis.point.intent_valid);
  // The standing intent names the LAST move: its labels round-trip to altitude 21.5 + 4.
  std::vector<int> probe_faces;
  std::vector<int> probe_counts;
  const LUMICE_SinglePathRequest probe = MakePointRequestForKey(state.analysis.point.key, &probe_faces, &probe_counts);
  EXPECT_NEAR(probe.target_altitude_deg, 25.5, 1e-3);
  const double now = 100.0;
  ASSERT_TRUE(
      PumpAllUntil(state, executor, now, [&] { return state.analysis.point.adopted_result != nullptr; }, 60000));
  EXPECT_EQ(PointQueryStateFor(state.analysis.point), PointQueryState::kCompleted);
  // The mailbox is drained: the pump took the one finish there was.
  EXPECT_EQ(executor.TakeFinished(QueryFace::kPoint), std::nullopt);
  executor.ShutdownAndJoin();
}

// The frozen mesh source: the 3D region reads the DoAnalyze-captured crystal configs, never the
// live document — an edit after Analyze does not rebuild the mesh into a shape the trace no
// longer lies inside (AC4, the plan's risk 7).
TEST(AnalyzePointView, FrozenMeshSourceSurvivesADocumentEdit) {
  GuiState state;
  CrystalConfig config{};
  config.height.center = 1.25f;
  state.crystals.push_back(config);
  state.analysis.frozen_crystals = state.crystals;
  const CrystalConfig* frozen = FrozenCrystalForPool(state, 0);
  ASSERT_NE(frozen, nullptr);
  // An edit of the live document leaves the frozen copy exactly what the binding captured.
  state.crystals[0].height.center = 1.25f + 3.0f;
  const CrystalConfig* after_edit = FrozenCrystalForPool(state, 0);
  ASSERT_NE(after_edit, nullptr);
  EXPECT_EQ(after_edit->height.center, 1.25f);
  // Out-of-range slots and an empty snapshot read null — the renderer's honest note, not a
  // wrong mesh.
  EXPECT_EQ(FrozenCrystalForPool(state, static_cast<int>(state.crystals.size())), nullptr);
  EXPECT_EQ(FrozenCrystalForPool(state, -1), nullptr);
  GuiState empty;
  EXPECT_EQ(FrozenCrystalForPool(empty, 0), nullptr);
}

// The point wait line: the budget names the tier the request
// asks for (the face's own constant, spelled the way it always was) and the live elapsed
// suffix rides after it. One function, two render consumers — the fiber region and the details
// block cannot drift apart.
TEST(AnalyzePointView, PointWaitLineCarriesTheTierAndTheElapsed) {
  PointQuerySession point;
  point.intent_valid = true;
  point.intent_start_s = 7.0;
  const std::string line = PointWaitLineFor(point, 8.25);
  EXPECT_NE(line.find("Computing the point (2.0 s tier; queues behind the survey and map on the one worker)…"),
            std::string::npos);
  EXPECT_NE(line.find("(1.2 s)"), std::string::npos);
  EXPECT_EQ(line.substr(line.size() - 7), "(1.2 s)");
  // A later frame-time advances; a restarted epoch rolls the number back.
  EXPECT_NE(PointWaitLineFor(point, 9.5).find("(2.5 s)"), std::string::npos);
  point.intent_start_s = 10.0;
  EXPECT_NE(PointWaitLineFor(point, 9.5).find("(0.0 s)"), std::string::npos);
}

// The stage clause on the point line (751): the POINT face's own computing names the engine's
// last-known phase; another face's query renders the plain line.
TEST(AnalyzePointView, PointWaitLineCarriesTheStageOnlyForItsOwnFace) {
  PointQuerySession point;
  point.intent_valid = true;
  point.intent_start_s = 7.0;

  InflightPoll survey_running;  // the survey ahead in the queue: its stage is its own
  survey_running.in_flight = true;
  survey_running.face = QueryFace::kSurvey;
  survey_running.stage = LUMICE_PATH_POLL_STAGE_FIBER_DISCOVERY;
  survey_running.ticks = 4;
  EXPECT_EQ(PointWaitLineFor(point, 8.25, survey_running).find("discovering fibers"), std::string::npos);

  InflightPoll point_running;
  point_running.in_flight = true;
  point_running.face = QueryFace::kPoint;
  point_running.stage = LUMICE_PATH_POLL_STAGE_POINT_LEGS;
  point_running.ticks = 5;
  const std::string line = PointWaitLineFor(point, 8.25, point_running);
  EXPECT_NE(line.find(" — tracing point legs…"), std::string::npos);
  EXPECT_EQ(line.substr(line.size() - 7), "(1.2 s)");  // the elapsed suffix stays last
}

}  // namespace
}  // namespace lumice::gui
