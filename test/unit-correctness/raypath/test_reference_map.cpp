#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

#include "analytic/stop.hpp"
#include "core/optics.hpp"  // IceRefractiveIndex
#include "raypath/detail/input_scope.hpp"
#include "raypath/detail/quantity/probe_line.hpp"
#include "raypath/detail/quantity/reference_map.hpp"
#include "raypath/map_analysis.hpp"
#include "raypath/prepared_geometry.hpp"
#include "raypath/scene_to_analytic.hpp"
#include "raypath/single_path_analysis.hpp"  // kMaxSampleCount
#include "raypath/workspace.hpp"
#include "survey_test_fixtures.hpp"

namespace lumice::raypath {
namespace {

constexpr double kPi = 3.14159265358979323846;

// The C01 class: the random prism's PBD 3-5 expansion (the M1 probe's 12 members), resolved
// through the same authority the face uses.
struct ClassFixture {
  ConfigManager config;
  std::vector<std::vector<int>> members;
  analytic::FaceNormalTable table;
  analytic::FacePolygonTable polygons;
  PoseDensityConversion density;
  double incident[3]{};
  double refractive_index = 0.0;

  static ClassFixture Make() {
    ClassFixture fix;
    fix.config = survey::RandomPrismScene();
    PathSelection selection;
    selection.crystal_id = 1;
    selection.path_layers = { { 3, 5 } };
    selection.symmetry_bits = 7;
    survey::CrystalSceneScope scope;
    if (!survey::ResolveCrystalSceneScope(fix.config, selection, &scope).Ok()) {
      ADD_FAILURE() << "class scope refused";
      return fix;
    }
    fix.members = std::move(scope.members);
    fix.density = std::move(scope.density);
    const auto configured = fix.config.crystals_.find(1);
    PreparedGeometry geometry;
    if (!PrepareGeometry(configured->second, &geometry).Ok()) {
      ADD_FAILURE() << "geometry rejected";
      return fix;
    }
    fix.table = geometry.table;
    fix.polygons = geometry.polygons;
    SunIncidentDirection(fix.config.scene_.light_source_.param_, fix.incident);
    WavelengthChoice choice;
    if (!ResolveWavelength(fix.config.scene_.light_source_, std::nullopt, &choice).Ok()) {
      ADD_FAILURE() << "wavelength refused";
      return fix;
    }
    fix.refractive_index = choice.refractive_index;
    return fix;
  }
};

// A small window over the C01 ring: 0.25-degree cells, the view the cheap invariant tests run.
survey::MapViewDescription RingView(const ClassFixture& fix) {
  survey::MapViewDescription view;
  view.delta_min_deg = 20.0;
  view.delta_max_deg = 26.0;
  view.az_min_deg = -6.0;
  view.az_max_deg = 6.0;
  view.rows = 24;
  view.cols = 48;
  const survey::ProbeFrame frame = survey::MakeProbeFrame(fix.incident);
  for (int k = 0; k < 3; k++) {
    view.incident[k] = frame.incident[k];
    view.e1[k] = frame.e1[k];
    view.e2[k] = frame.e2[k];
  }
  return view;
}

// INV1 (AC2): the class map IS the sum of the member maps — same lattice, same cells, values
// summed member-by-member bit for bit. The reconciliation is the class-sum's own identity (the
// plan states it is not an independent physical oracle).
TEST(ReferenceMap, ClassSumEqualsSumOfMemberMapsBitwise) {
  const ClassFixture fix = ClassFixture::Make();
  ASSERT_EQ(fix.members.size(), static_cast<size_t>(12));
  const survey::MapViewDescription view = RingView(fix);
  const int n = 200000;

  const survey::ReferenceMapData class_map = survey::EvaluateReferenceMap(
      fix.table, fix.polygons, fix.members, fix.density, survey::MakeProbeFrame(fix.incident), view, 550.0,
      fix.refractive_index, fix.incident, n);
  ASSERT_EQ(class_map.members_evaluated, 12);
  ASSERT_FALSE(class_map.truncated);

  std::vector<double> summed_values(class_map.pixels.size(), 0.0);
  std::vector<int> summed_counts(class_map.pixels.size(), 0);
  long long kept = 0;
  for (const std::vector<int>& member : fix.members) {
    const std::vector<std::vector<int>> one{ member };
    const survey::ReferenceMapData single =
        survey::EvaluateReferenceMap(fix.table, fix.polygons, one, fix.density, survey::MakeProbeFrame(fix.incident),
                                     view, 550.0, fix.refractive_index, fix.incident, n);
    if (single.members_evaluated != 1) {
      ADD_FAILURE() << "member " << member.front() << " did not evaluate";
      continue;
    }
    kept += single.kept_count;
    for (size_t p = 0; p < summed_values.size(); p++) {
      if (std::isnan(single.pixels[p].value)) {
        continue;  // singular cells stay NaN in both readings
      }
      summed_values[p] += single.pixels[p].value;
      summed_counts[p] += single.pixels[p].k;
    }
  }
  for (size_t p = 0; p < class_map.pixels.size(); p++) {
    if (std::isnan(class_map.pixels[p].value)) {
      EXPECT_TRUE(std::isnan(summed_values[p]));
      continue;
    }
    EXPECT_EQ(class_map.pixels[p].value, summed_values[p]) << "pixel " << p;
    EXPECT_EQ(class_map.pixels[p].k, summed_counts[p]) << "pixel " << p;
  }
  EXPECT_EQ(class_map.kept_count, kept);
}

// The negative control (plan Step 2): the class map is NOT the representative's map times the
// member count — on this asymmetric class the members' maps differ, so any "representative x N"
// implementation reads a different grid.
TEST(ReferenceMap, RepresentativeTimesNIsNotTheClassSum) {
  const ClassFixture fix = ClassFixture::Make();
  const survey::MapViewDescription view = RingView(fix);
  const int n = 200000;

  const survey::ReferenceMapData class_map = survey::EvaluateReferenceMap(
      fix.table, fix.polygons, fix.members, fix.density, survey::MakeProbeFrame(fix.incident), view, 550.0,
      fix.refractive_index, fix.incident, n);
  const std::vector<std::vector<int>> representative_only{ fix.members.front() };
  const survey::ReferenceMapData representative = survey::EvaluateReferenceMap(
      fix.table, fix.polygons, representative_only, fix.density, survey::MakeProbeFrame(fix.incident), view, 550.0,
      fix.refractive_index, fix.incident, n);
  int differing = 0;
  for (size_t p = 0; p < class_map.pixels.size(); p++) {
    if (std::isnan(class_map.pixels[p].value) || std::isnan(representative.pixels[p].value)) {
      continue;
    }
    if (class_map.pixels[p].value != 12.0 * representative.pixels[p].value) {
      differing++;
    }
  }
  EXPECT_GT(differing, 0) << "the class map degenerated to representative x N";
}

// INV2 (AC5, C15): the identity class's sky image is the point mass — the pixel arm stays the
// honest zero texture and the mass arm carries m / Omega at the sun's cell. Zero pixels with a
// mass arm present are never "no light".
TEST(ReferenceMap, RankZeroClassCarriesPointMassNotTexture) {
  const ConfigManager config = survey::PlateScene(1.5, 0.3);
  PathSelection selection;
  selection.crystal_id = 1;
  selection.path_layers = { { 1, 3, 6, 2 } };
  selection.symmetry_bits = 7;
  survey::CrystalSceneScope scope;
  ASSERT_TRUE(survey::ResolveCrystalSceneScope(config, selection, &scope).Ok());
  const auto configured = config.crystals_.find(1);
  PreparedGeometry geometry;
  ASSERT_TRUE(PrepareGeometry(configured->second, &geometry).Ok());
  ASSERT_TRUE(scope.density.Ok());

  survey::MapViewDescription view;
  view.full_sky = true;
  view.delta_min_deg = 0.0;
  view.delta_max_deg = 180.0;
  view.az_min_deg = 0.0;
  view.az_max_deg = 360.0;
  view.rows = 36;
  view.cols = 72;
  double incident[3];
  SunIncidentDirection(config.scene_.light_source_.param_, incident);
  const survey::ProbeFrame real_frame = survey::MakeProbeFrame(incident);
  for (int k = 0; k < 3; k++) {
    view.incident[k] = real_frame.incident[k];
    view.e1[k] = real_frame.e1[k];
    view.e2[k] = real_frame.e2[k];
  }

  const survey::ReferenceMapData map =
      survey::EvaluateReferenceMap(geometry.table, geometry.polygons, scope.members, scope.density, real_frame, view,
                                   550.0, lumice::IceRefractiveIndex::Get(550.0), incident, 100000);
  ASSERT_EQ(map.members_evaluated, static_cast<int>(scope.members.size()));
  ASSERT_TRUE(map.point_mass.has_value()) << "the identity class produced no mass arm";
  EXPECT_GT(map.point_mass->m, 0.0);
  EXPECT_FALSE(map.point_mass->methods.empty());
  EXPECT_EQ(map.point_mass->methods.size(), static_cast<size_t>(1));  // one density family, one method
  EXPECT_GE(map.point_mass->pixel, 0);
  // The mass cell carries m / Omega (the band-sum pixel convention: the cell's solid angle is
  // d_delta * d_az * sin(centre delta) — the sun cell sits at the first row's centre delta).
  const double row_deg = 180.0 / view.rows;
  const double col_deg = 360.0 / view.cols;
  const double centre_delta_deg = 0.5 * row_deg;
  const double omega = row_deg * (kPi / 180.0) * col_deg * (kPi / 180.0) * std::sin(centre_delta_deg * (kPi / 180.0));
  bool sun_cell_ok = false;
  for (size_t p = 0; p < map.pixels.size(); p++) {
    if (static_cast<long long>(p) == map.point_mass->pixel) {
      EXPECT_EQ(map.pixels[p].status, survey::MapPixelSample::Status::kPointMass);
      EXPECT_DOUBLE_EQ(map.pixels[p].value, map.point_mass->m / omega);
      sun_cell_ok = true;
    } else if (map.pixels[p].status == survey::MapPixelSample::Status::kOk) {
      EXPECT_EQ(map.pixels[p].value, 0.0) << "rank-0 mass smeared into pixel " << p;
    }
  }
  EXPECT_TRUE(sun_cell_ok);
  // The mass is the kept events' lattice mean — the arm holds events even though the pixel
  // arm stays the zero texture.
  EXPECT_GT(map.kept_count, 0);
}

// INV2's mixed shape: a constructed class with one rank-0 member and one band member (the
// corpus has no natural mix) — each member lands in exactly one arm: the pixels equal the band
// member's own map bit for bit, the mass equals the rank-0 member's own mass.
TEST(ReferenceMap, MixedClassSplitsArmsPerMember) {
  const ConfigManager config = survey::PlateScene(1.5, 0.3);
  const auto configured = config.crystals_.find(1);
  PreparedGeometry geometry;
  ASSERT_TRUE(PrepareGeometry(configured->second, &geometry).Ok());
  const PoseDensityConversion density = ConvertAxisToPoseDensity(configured->second.axis_);
  ASSERT_TRUE(density.Ok());
  double incident[3];
  SunIncidentDirection(config.scene_.light_source_.param_, incident);
  const survey::ProbeFrame frame = survey::MakeProbeFrame(incident);
  const survey::MapViewDescription view = [&] {
    survey::MapViewDescription v;
    v.delta_min_deg = 20.0;
    v.delta_max_deg = 26.0;
    v.az_min_deg = -6.0;
    v.az_max_deg = 6.0;
    v.rows = 12;
    v.cols = 24;
    for (int k = 0; k < 3; k++) {
      v.incident[k] = frame.incident[k];
      v.e1[k] = frame.e1[k];
      v.e2[k] = frame.e2[k];
    }
    return v;
  }();
  const int n = 100000;
  const double index = lumice::IceRefractiveIndex::Get(550.0);

  const std::vector<std::vector<int>> mixed{ { 1, 3, 6, 2 }, { 3, 5 } };
  const survey::ReferenceMapData both = survey::EvaluateReferenceMap(geometry.table, geometry.polygons, mixed, density,
                                                                     frame, view, 550.0, index, incident, n);
  const std::vector<std::vector<int>> rank0_only{ { 1, 3, 6, 2 } };
  const survey::ReferenceMapData mass_arm = survey::EvaluateReferenceMap(
      geometry.table, geometry.polygons, rank0_only, density, frame, view, 550.0, index, incident, n);
  const std::vector<std::vector<int>> band_only{ { 3, 5 } };
  const survey::ReferenceMapData band_arm = survey::EvaluateReferenceMap(
      geometry.table, geometry.polygons, band_only, density, frame, view, 550.0, index, incident, n);

  ASSERT_TRUE(both.point_mass.has_value());
  ASSERT_TRUE(mass_arm.point_mass.has_value());
  EXPECT_DOUBLE_EQ(both.point_mass->m, mass_arm.point_mass->m);
  EXPECT_EQ(both.point_mass->pixel, mass_arm.point_mass->pixel);
  for (size_t p = 0; p < both.pixels.size(); p++) {
    if (both.pixels[p].status == survey::MapPixelSample::Status::kPointMass || std::isnan(both.pixels[p].value)) {
      continue;
    }
    EXPECT_EQ(both.pixels[p].value, band_arm.pixels[p].value) << "rank-0 member leaked into pixel " << p;
    EXPECT_EQ(both.pixels[p].k, band_arm.pixels[p].k);
  }
  EXPECT_GT(both.kept_count, 0);  // the band member's events are the map's events
}

// INV3 (AC3): the same request reproduces the map bit for bit (the lattice is deterministic);
// a different view re-runs the class on its own grid — the support moves with the window and a
// finer grid resolves more non-empty cells (never an upscaling of the old raster).
TEST(ReferenceMap, DeterministicRerunsAndViewsReallyRecompute) {
  const ClassFixture fix = ClassFixture::Make();
  const survey::ProbeFrame frame = survey::MakeProbeFrame(fix.incident);
  const survey::MapViewDescription view = RingView(fix);
  const int n = 100000;

  const survey::ReferenceMapData first = survey::EvaluateReferenceMap(
      fix.table, fix.polygons, fix.members, fix.density, frame, view, 550.0, fix.refractive_index, fix.incident, n);
  const survey::ReferenceMapData second = survey::EvaluateReferenceMap(
      fix.table, fix.polygons, fix.members, fix.density, frame, view, 550.0, fix.refractive_index, fix.incident, n);
  ASSERT_EQ(first.pixels.size(), second.pixels.size());
  for (size_t p = 0; p < first.pixels.size(); p++) {
    EXPECT_EQ(first.pixels[p].value, second.pixels[p].value) << "not bit-reproducible at " << p;
    EXPECT_EQ(first.pixels[p].k, second.pixels[p].k);
  }

  // The window shifts inward: the support's leading edge moves with it.
  survey::MapViewDescription shifted = view;
  shifted.delta_min_deg = 22.0;
  shifted.delta_max_deg = 26.0;
  const survey::ReferenceMapData shifted_map = survey::EvaluateReferenceMap(
      fix.table, fix.polygons, fix.members, fix.density, frame, shifted, 550.0, fix.refractive_index, fix.incident, n);
  int first_row_nonempty = -1;
  int shifted_row_nonempty = -1;
  for (int r = 0; r < view.rows && first_row_nonempty < 0; r++) {
    for (int c = 0; c < view.cols; c++) {
      if (first.pixels[static_cast<size_t>(r) * view.cols + c].k > 0) {
        first_row_nonempty = r;
        break;
      }
    }
  }
  for (int r = 0; r < shifted.rows && shifted_row_nonempty < 0; r++) {
    for (int c = 0; c < shifted.cols; c++) {
      if (shifted_map.pixels[static_cast<size_t>(r) * shifted.cols + c].k > 0) {
        shifted_row_nonempty = r;
        break;
      }
    }
  }
  ASSERT_GE(first_row_nonempty, 0);
  ASSERT_GE(shifted_row_nonempty, 0);
  // The leading edge sits at the closed-form edge in both windows; shifting the window by 2
  // degrees moves the first non-empty row by ~2 degrees / the row height.
  EXPECT_NEAR(first_row_nonempty * (view.delta_max_deg - view.delta_min_deg) / view.rows + view.delta_min_deg, 21.9,
              1.0);
  EXPECT_NEAR(
      shifted_row_nonempty * (shifted.delta_max_deg - shifted.delta_min_deg) / shifted.rows + shifted.delta_min_deg,
      21.9, 1.0);
}

// INV4 (AC4): a stop keeps the completed member prefix and marks truncation — a pre-cancelled
// token stops before the first member; a 1 ms budget cannot finish the first member's lattice,
// and the face reports the interrupted outcome. Neither run presents itself as the class.
TEST(ReferenceMap, StopKeepsCompletedPrefixAndNamesTheStop) {
  const ClassFixture fix = ClassFixture::Make();
  const survey::ProbeFrame frame = survey::MakeProbeFrame(fix.incident);
  const survey::MapViewDescription view = RingView(fix);

  analytic::StopToken token;
  token.RequestCancel();
  const analytic::DiscoveryStop stop{ std::chrono::steady_clock::time_point::max(), nullptr, &token };
  const survey::ReferenceMapData cancelled =
      survey::EvaluateReferenceMap(fix.table, fix.polygons, fix.members, fix.density, frame, view, 550.0,
                                   fix.refractive_index, fix.incident, 100000, &stop);
  EXPECT_EQ(cancelled.members_evaluated, 0);
  EXPECT_TRUE(cancelled.truncated);
  EXPECT_TRUE(cancelled.member_kept_counts.empty());
  for (const survey::MapPixelSample& pixel : cancelled.pixels) {
    if (pixel.status == survey::MapPixelSample::Status::kOk) {
      EXPECT_EQ(pixel.value, 0.0);
    }
  }

  // The face's budget path: a 1 ms budget cuts inside the first member's 2M lattice.
  MapRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 5 } };
  request.symmetry_bits = 7;
  request.full_sky = false;
  request.centre_altitude_deg = -20.0;
  request.centre_azimuth_deg = 0.0;
  request.delta_span_deg = 6.0;
  request.azimuth_span_deg = 12.0;
  request.view_rows = 12;
  request.view_cols = 24;
  request.sample_count = kDefaultMapSampleCount;
  request.budget_ms = 1;
  MapResult result;
  ASSERT_TRUE(AnalyzePathMap(fix.config, request, &result).Ok());
  const MapData& data = result.Data();
  ASSERT_EQ(ClaimState(data.claim), survey::EvidenceState::kFinite)
      << "a stopped map is a finite claim about its prefix";
  EXPECT_EQ(data.meta.interrupted, true);
  EXPECT_EQ(data.meta.execution, ExecutionOutcome::kDeadlineExceeded);
  const auto* finite = std::get_if<Finite<MapValues>>(&data.claim);
  ASSERT_NE(finite, nullptr);
  EXPECT_TRUE(finite->value.map.truncated);
}

// INV6 (D5): the refusal matrix — each unsupported shape names its own mode, never a zero map
// or a silent substitution.
TEST(ReferenceMap, RefusalMatrixNamesEveryMode) {
  const ClassFixture fix = ClassFixture::Make();
  MapResult result;

  // Multi-layer: the same named refusal the point/probe faces spell.
  MapRequest multi;
  multi.crystal_id = 1;
  multi.path_layers = { { 3, 5 }, { 1, 3 } };
  const Error multi_error = AnalyzePathMap(fix.config, multi, &result);
  EXPECT_EQ(multi_error.code, ErrorCode::kMultiLayerUnsupported);

  // The measure not expressible: the fixed-pose plate (an axis no pose family reads).
  MapRequest fixed_pose;
  fixed_pose.crystal_id = 1;
  fixed_pose.path_layers = { { 1, 3, 6, 2 } };
  fixed_pose.full_sky = true;
  fixed_pose.view_rows = 4;
  fixed_pose.view_cols = 8;
  const ConfigManager fixed_config = survey::FixedPosePlateScene(1.5);
  ASSERT_TRUE(AnalyzePathMap(fixed_config, fixed_pose, &result).Ok());
  EXPECT_EQ(ClaimState(result.Data().claim), survey::EvidenceState::kRefused);
  const auto* refused = std::get_if<Refused>(&result.Data().claim);
  ASSERT_NE(refused, nullptr);
  // The mode's prefix is the contract; the detail after the colon is the conversion
  // authority's own naming of the axis it could not read.
  EXPECT_EQ(refused->reason.substr(0, refused->reason.find(':')), "measure_not_expressible");
  EXPECT_NE(refused->reason.find(':'), std::string::npos);

  // The request-shape refusals.
  MapRequest bad;
  bad.crystal_id = 1;
  bad.path_layers = { { 3, 5 } };
  bad.view_rows = 1;
  bad.view_cols = 1;
  bad.full_sky = false;
  bad.centre_altitude_deg = -20.0;
  bad.delta_span_deg = 0.0;  // a non-positive span
  bad.azimuth_span_deg = 4.0;
  EXPECT_EQ(AnalyzePathMap(fix.config, bad, &result).code, ErrorCode::kInvalidArgument);
  bad.delta_span_deg = 4.0;
  bad.centre_altitude_deg = 95.0;  // outside [-90, 90]
  EXPECT_EQ(AnalyzePathMap(fix.config, bad, &result).code, ErrorCode::kInvalidTarget);
  bad.centre_altitude_deg = -20.0;
  bad.sample_count = kMaxSampleCount + 1;
  EXPECT_EQ(AnalyzePathMap(fix.config, bad, &result).code, ErrorCode::kInvalidArgument);
  bad.sample_count = 0;
  bad.view_rows = 1000;
  bad.view_cols = 1000;  // over the cell bound
  EXPECT_EQ(AnalyzePathMap(fix.config, bad, &result).code, ErrorCode::kInvalidArgument);
  bad.view_rows = 4;
  bad.view_cols = 4;
  bad.wavelengths_nm = { 550.0, 650.0 };  // the per-lambda family is a later face
  EXPECT_EQ(AnalyzePathMap(fix.config, bad, &result).code, ErrorCode::kInvalidArgument);
  bad.wavelengths_nm = { 200.0 };  // outside the index table
  EXPECT_EQ(AnalyzePathMap(fix.config, bad, &result).code, ErrorCode::kWavelengthOutOfRange);
}

// INV8 (AC5): a cell covering the sun direction or its antipode is singular — NaN with an
// explicit status, never a written zero, and the status is view-determined (identical across
// members by construction, read once from the grid).
TEST(ReferenceMap, SingularCellsAreNaNNotZero) {
  const ClassFixture fix = ClassFixture::Make();
  const survey::ProbeFrame frame = survey::MakeProbeFrame(fix.incident);
  survey::MapViewDescription view;
  view.full_sky = true;
  view.delta_min_deg = 0.0;
  view.delta_max_deg = 180.0;
  view.az_min_deg = 0.0;
  view.az_max_deg = 360.0;
  view.rows = 36;
  view.cols = 72;
  for (int k = 0; k < 3; k++) {
    view.incident[k] = frame.incident[k];
    view.e1[k] = frame.e1[k];
    view.e2[k] = frame.e2[k];
  }
  const survey::ReferenceMapData map =
      survey::EvaluateReferenceMap(fix.table, fix.polygons, fix.members, fix.density, frame, view, 550.0,
                                   fix.refractive_index, fix.incident, 100000);
  int singular = 0;
  for (const survey::MapPixelSample& pixel : map.pixels) {
    if (pixel.status == survey::MapPixelSample::Status::kSingular) {
      singular++;
      EXPECT_TRUE(std::isnan(pixel.value)) << "a singular cell carried a number";
    } else {
      EXPECT_FALSE(std::isnan(pixel.value)) << "a non-singular cell carried NaN";
    }
  }
  EXPECT_GT(singular, 0) << "an all-sky view must contain the sun / antipode cells";
}

// INV7 (AC2): the face's document reads its own caliber and coverage — kind, method,
// aggregation, unit, spectrum/source/shape, the reference wavelength, the member count and the
// completion boundary.
TEST(ReferenceMap, FaceDocumentCarriesCaliberAndCoverage) {
  const ClassFixture fix = ClassFixture::Make();
  MapRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 5 } };
  request.symmetry_bits = 7;
  request.full_sky = false;
  request.centre_altitude_deg = -20.0;
  request.centre_azimuth_deg = 0.0;
  request.delta_span_deg = 6.0;
  request.azimuth_span_deg = 12.0;
  request.view_rows = 12;
  request.view_cols = 24;
  request.sample_count = 100000;
  MapResult result;
  ASSERT_TRUE(AnalyzePathMap(fix.config, request, &result).Ok());
  const MapData& data = result.Data();
  ASSERT_EQ(ClaimState(data.claim), survey::EvidenceState::kFinite);
  const auto* finite = std::get_if<Finite<MapValues>>(&data.claim);
  ASSERT_NE(finite, nullptr);
  const QuantitySpec& spec = finite->value.quantity;
  EXPECT_EQ(spec.kind, QuantityKind::kBandSum);
  EXPECT_EQ(spec.method, QuantityMethod::kClassLatticePowerSum);
  EXPECT_EQ(spec.aggregation, MemberAggregation::kPhysicalClass);
  EXPECT_EQ(spec.unit, QuantityUnit::kPerSteradian);
  EXPECT_EQ(spec.spectrum, SpectralObservation::kReferenceWavelength);
  EXPECT_EQ(spec.source, SourceObservation::kSolarCenter);
  EXPECT_EQ(spec.shape, GeometryKind::kNominal);
  ASSERT_EQ(spec.wavelengths_nm.size(), static_cast<size_t>(1));
  EXPECT_DOUBLE_EQ(spec.wavelengths_nm.front(), 550.0);
  const survey::ReferenceMapData& map = finite->value.map;
  EXPECT_EQ(map.member_count, 12);
  EXPECT_EQ(map.members_evaluated, 12);
  EXPECT_EQ(finite->coverage.members_evaluated, 12);
  EXPECT_DOUBLE_EQ(map.view.delta_min_deg, 17.0);
  EXPECT_DOUBLE_EQ(map.view.delta_max_deg, 23.0);
  EXPECT_EQ(static_cast<int>(map.pixels.size()), 12 * 24);
  EXPECT_FALSE(map.truncated);
  EXPECT_FALSE(map.point_mass.has_value());
}

// AC6 (the direction anchor): the class map's first non-empty cell contains the closed-form
// minimum-deviation edge `2*asin(n/2) - 60` — no event lies below it, so the first non-empty
// cell is the edge's cell; the containment is exact in the cell, with no extra slack. The
// probe-consistency leg: the shared kernel's own line probe at the same lambda/frame reads its
// first non-empty bin across the map's first non-empty cell (one implementation, two triggers,
// one reading).
TEST(ReferenceMap, ClassMapEdgeContainsClosedFormMinimumDeviation) {
  const ClassFixture fix = ClassFixture::Make();
  const survey::ProbeFrame frame = survey::MakeProbeFrame(fix.incident);
  survey::MapViewDescription view;
  view.delta_min_deg = 20.0;
  view.delta_max_deg = 24.0;
  view.az_min_deg = -4.0;
  view.az_max_deg = 4.0;
  view.rows = 200;
  view.cols = 400;
  for (int k = 0; k < 3; k++) {
    view.incident[k] = frame.incident[k];
    view.e1[k] = frame.e1[k];
    view.e2[k] = frame.e2[k];
  }
  const int n = 500000;
  const survey::ReferenceMapData map = survey::EvaluateReferenceMap(
      fix.table, fix.polygons, fix.members, fix.density, frame, view, 550.0, fix.refractive_index, fix.incident, n);
  ASSERT_FALSE(map.truncated);
  const double closed_form_deg = 2.0 * std::asin(fix.refractive_index * 0.5) * 180.0 / kPi - 60.0;

  // The first non-empty cell in the delta direction (at the window's centre azimuth column).
  const int centre_col = view.cols / 2;
  int first_row = -1;
  for (int r = 0; r < view.rows; r++) {
    if (map.pixels[static_cast<size_t>(r) * view.cols + centre_col].k > 0) {
      first_row = r;
      break;
    }
  }
  ASSERT_GE(first_row, 0) << "the inner-edge window read empty";
  const double row_height = (view.delta_max_deg - view.delta_min_deg) / view.rows;
  const double cell_lo = view.delta_min_deg + first_row * row_height;
  const double cell_hi = cell_lo + row_height;
  EXPECT_LE(closed_form_deg, cell_hi + 1e-9)
      << "light below the closed-form minimum deviation: the edge anchor is broken";
  EXPECT_GE(closed_form_deg, cell_lo - 1e-9)
      << "the first non-empty cell sits above the closed-form edge: the lattice missed the ring";

  // The probe consistency leg: the same kernel's line at the window's centre azimuth, same
  // lambda and lattice, reads an edge bin across the map's first non-empty cell (a consistency
  // check, not an independent oracle — the closed form above is the independent anchor).
  const survey::ProbeLineLayout layout =
      survey::AxisLineLayout(view.delta_min_deg, view.delta_max_deg, 0.5 * (view.az_min_deg + view.az_max_deg));
  const survey::ProbeLineProfile line =
      survey::EvaluateProbeLine(fix.table, fix.polygons, fix.members.front(), fix.density, frame, layout, 550.0,
                                fix.refractive_index, fix.incident, n);
  ASSERT_FALSE(line.truncated);
  double probe_edge_deg = -1.0;
  for (const survey::ProbeLineSample& sample : line.samples) {
    if (sample.k > 0) {
      probe_edge_deg = sample.delta_deg;
      break;
    }
  }
  ASSERT_GT(probe_edge_deg, 0.0);
  EXPECT_NEAR(probe_edge_deg, 0.5 * (cell_lo + cell_hi), 2.0 * survey::kProbeBinDeg);
}

// The workspace's fifth face: the shared geometry slot serves the map without a rebuild, the
// cancel token rides into the member loop, and the ledger's structure count does not move (the
// map runs no structure).
TEST(ReferenceMap, WorkspaceFaceReusesGeometryAndHonorsCancel) {
  const ConfigManager config = survey::RandomPrismScene();
  std::unique_ptr<AnalysisWorkspace> workspace;
  ASSERT_TRUE(AnalysisWorkspace::Create(config, &workspace).Ok());

  MapRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 5 } };
  request.symmetry_bits = 7;
  request.full_sky = false;
  request.centre_altitude_deg = -20.0;
  request.centre_azimuth_deg = 0.0;
  request.delta_span_deg = 6.0;
  request.azimuth_span_deg = 12.0;
  request.view_rows = 8;
  request.view_cols = 16;
  request.sample_count = 50000;

  MapResult first;
  ASSERT_TRUE(workspace->QueryMap(request, &first).Ok());
  const long builds_after_first = workspace->GeometryBuildCount();
  const long structures_after_first = workspace->StructureRunCount();
  MapResult second;
  ASSERT_TRUE(workspace->QueryMap(request, &second).Ok());
  EXPECT_EQ(workspace->GeometryBuildCount(), builds_after_first) << "the geometry slot rebuilt";
  EXPECT_EQ(workspace->StructureRunCount(), structures_after_first) << "the map moved the structure ledger";
  // Same request, same numbers: the two documents read identical pixels.
  const auto& a = std::get<Finite<MapValues>>(first.Data().claim).value.map.pixels;
  const auto& b = std::get<Finite<MapValues>>(second.Data().claim).value.map.pixels;
  ASSERT_EQ(a.size(), b.size());
  for (size_t p = 0; p < a.size(); p++) {
    EXPECT_EQ(a[p].value, b[p].value);
  }

  // The workspace's cancel token: a pre-cancelled query truncates at the member boundary.
  analytic::StopToken token;
  token.RequestCancel();
  MapResult cancelled;
  ASSERT_TRUE(workspace->QueryMap(request, &cancelled, &token).Ok());
  const auto& claim = cancelled.Data().claim;
  const auto* finite = std::get_if<Finite<MapValues>>(&claim);
  ASSERT_NE(finite, nullptr);
  EXPECT_TRUE(finite->value.map.truncated);
  EXPECT_EQ(finite->value.map.members_evaluated, 0);
  EXPECT_EQ(cancelled.Data().meta.execution, ExecutionOutcome::kCancelled);
}

}  // namespace
}  // namespace lumice::raypath
