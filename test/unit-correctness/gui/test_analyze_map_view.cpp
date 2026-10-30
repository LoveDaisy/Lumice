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
#include "gui/analyze_map_view.hpp"
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

// ---- The disk projection (the prototype's contract, held to independent anchors) -------------------

TEST(AnalyzeMapView, ZenithFullSkyProjectsTheSunByTheHandDerivedSpot) {
  // Zenith-centred full sky: the zenith is the disk centre, and the sun at (alt 20, az 0)
  // sits 70 degrees out — at rho = sin(35°)/sin(90°) of the half-radius, in the fallback basis's
  // own screen down-direction (the basis is (−1,0,0) up by the prototype's rule; spelled here,
  // not read from the implementation).
  const float centre[3] = { 0.0f, 0.0f, 1.0f };
  const MapDiskProjector proj = MakeMapDiskProjector(centre, 180.0f);
  double sun[3];
  SkyPositionOf(20.0, 0.0, sun);
  double x = 0.0;
  double y = 0.0;
  ASSERT_TRUE(MapDiskProject(proj, sun, &x, &y));
  const double rho = std::sin(35.0 * kD2R);
  EXPECT_NEAR(x, 0.5, 1e-12);
  EXPECT_NEAR(y, 0.5 + 0.5 * rho, 1e-12);
  // And the zenith itself is the centre.
  double zenith[3] = { 0.0, 0.0, 1.0 };
  ASSERT_TRUE(MapDiskProject(proj, zenith, &x, &y));
  EXPECT_NEAR(x, 0.5, 1e-12);
  EXPECT_NEAR(y, 0.5, 1e-12);
}

TEST(AnalyzeMapView, DiskRoundtripsAndRejectsTheCorners) {
  // The centre here is off the world axis, so the basis is the zenith projection — and the
  // roundtrip must hold for directions across the view disk.
  double centre_d[3];
  SkyPositionOf(40.0, 30.0, centre_d);
  const float centre[3] = { static_cast<float>(centre_d[0]), static_cast<float>(centre_d[1]),
                            static_cast<float>(centre_d[2]) };
  const MapDiskProjector proj = MakeMapDiskProjector(centre, 25.0f);
  const double kOffsets[][2] = { { 0, 0 },   { 10, 0 },    { 0, 15 }, { -8, 12 },
                                 { 18, -6 }, { -20, -10 }, { 5, 5 },  { 0, 24.9 } };
  for (const auto& offset : kOffsets) {
    // A direction at the given (toward-zenith, toward-east) offsets from the centre, spelled by
    // the spherical offset formula the prototype's basis induces: the tangent plane steps.
    double base[3];
    SkyPositionOf(40.0, 30.0, base);
    double up[3] = { -base[0] * base[2], -base[1] * base[2], 1.0 - base[2] * base[2] };
    const double n = std::sqrt(DotOf(up, up));
    for (int k = 0; k < 3; ++k) {
      up[k] /= n;
    }
    double right[3] = { base[1] * up[2] - base[2] * up[1], base[2] * up[0] - base[0] * up[2],
                        base[0] * up[1] - base[1] * up[0] };
    const double a = offset[0] * kD2R;
    const double b = offset[1] * kD2R;
    double pos[3];
    for (int k = 0; k < 3; ++k) {
      pos[k] = std::cos(a) * (std::cos(b) * base[k] + std::sin(b) * right[k]) + std::sin(a) * up[k];
    }
    double x = 0.0;
    double y = 0.0;
    if (!MapDiskProject(proj, pos, &x, &y)) {
      ADD_FAILURE() << "projection refused an in-disk offset " << offset[0] << "," << offset[1];
      continue;
    }
    double back[3];
    if (!MapDiskUnproject(proj, x, y, back)) {
      ADD_FAILURE() << "unproject refused its own projection at offset " << offset[0] << "," << offset[1];
      continue;
    }
    // The projector's centre is float-quantized (the view state's own precision), so the
    // roundtrip carries a ~1e-7 rad ceiling — far under anything a pixel can express.
    EXPECT_NEAR(AngleBetween(pos, back), 0.0, 1e-6) << "offset " << offset[0] << "," << offset[1];
    // Past the view radius, the projection refuses.
    double far_pos[3];
    for (int k = 0; k < 3; ++k) {
      far_pos[k] = std::cos(30.0 * kD2R) * base[k] + std::sin(30.0 * kD2R) * up[k];
    }
    if (offset[0] == 0 && offset[1] == 0) {
      double fx = 0.0;
      double fy = 0.0;
      EXPECT_FALSE(MapDiskProject(proj, far_pos, &fx, &fy));
    }
  }
  // The bounding square's corners are outside the disk.
  double out[3];
  EXPECT_FALSE(MapDiskUnproject(proj, 0.02, 0.02, out));
  EXPECT_FALSE(MapDiskUnproject(proj, 0.98, 0.98, out));
}

TEST(AnalyzeMapView, PanMakesTheGrabbedSkyPointFollowTheCursor) {
  MapViewState view;
  double centre[3];
  SkyPositionOf(40.0, 30.0, centre);
  for (int k = 0; k < 3; ++k) {
    view.centre_pos[k] = static_cast<float>(centre[k]);
  }
  view.radius_deg = 25.0f;
  // A drag to the right by a fifth of the disk: the new centre is the sky point that was LEFT of
  // the centre by that step (the grab follows the cursor) — spelled through the same unproject.
  const MapDiskProjector proj = MakeMapDiskProjector(view.centre_pos, view.radius_deg);
  double expected[3];
  ASSERT_TRUE(MapDiskUnproject(proj, 0.5 - 0.1, 0.5, expected));
  MapPanViewByDiskCoords(&view, 0.1, 0.0);
  double got[3] = { view.centre_pos[0], view.centre_pos[1], view.centre_pos[2] };
  // The expected direction itself came through a float-centred unproject, and the result is
  // float-stored: a ~1e-4 rad band is what two quantizations actually promise.
  EXPECT_NEAR(AngleBetween(got, expected), 0.0, 1e-3);
}

TEST(AnalyzeMapView, ZoomClampsAndAnchorsAtTheCentre) {
  MapViewState view;
  double centre[3];
  SkyPositionOf(10.0, 10.0, centre);
  for (int k = 0; k < 3; ++k) {
    view.centre_pos[k] = static_cast<float>(centre[k]);
  }
  view.radius_deg = 3.0f;
  // Zoom at the centre moves only the radius.
  double at_centre[3] = { view.centre_pos[0], view.centre_pos[1], view.centre_pos[2] };
  MapZoomViewAtPoint(&view, at_centre, 1.0f / kMapZoomWheelFactor);
  EXPECT_NEAR(view.radius_deg, 3.0f / kMapZoomWheelFactor, 1e-5);
  for (int k = 0; k < 3; ++k) {
    EXPECT_NEAR(view.centre_pos[k], at_centre[k], 1e-5);
  }
  // The radius clamps at both ends.
  for (int i = 0; i < 40; ++i) {
    MapZoomViewAtPoint(&view, at_centre, 1.0f / kMapZoomWheelFactor);
  }
  EXPECT_EQ(view.radius_deg, kMapMinRadiusDeg);
  for (int i = 0; i < 40; ++i) {
    MapZoomViewAtPoint(&view, at_centre, kMapZoomWheelFactor);
  }
  EXPECT_EQ(view.radius_deg, kMapFullSkyRadiusDeg);
}

// ---- The view -> window derivation (the spans derived by hand here) --------------------------------

TEST(AnalyzeMapView, WindowSpecBoundsTheDiskInTheSunFrame) {
  MapViewState view;
  double centre[3];
  SkyPositionOf(40.0, 0.0, centre);  // 20 degrees from the sun (alt 20, az 0)
  for (int k = 0; k < 3; ++k) {
    view.centre_pos[k] = static_cast<float>(centre[k]);
  }
  view.radius_deg = 10.0f;
  const MapWindowSpec spec = MapWindowSpecForView(view, 20.0, 0.0);
  EXPECT_FALSE(spec.full_sky);
  EXPECT_NEAR(spec.centre_altitude_deg, 40.0, 1e-5);  // the float-quantized centre
  EXPECT_NEAR(spec.centre_azimuth_deg, 0.0, 1e-5);
  EXPECT_NEAR(spec.delta_span_deg, 20.0, 1e-9);
  // The tangent-cone bound: 2·asin(sin 10° / sin 20°), the hand spelling of the cap's azimuth
  // extent in the sun frame.
  const double az_span = 2.0 * std::asin(std::sin(10.0 * kD2R) / std::sin(20.0 * kD2R)) * kR2D;
  EXPECT_NEAR(spec.azimuth_span_deg, az_span, 1e-4);  // the centre's float quantization
  EXPECT_GT(spec.rows, 0);
  EXPECT_GT(spec.cols, 0);
  EXPECT_LE(static_cast<long long>(spec.rows) * static_cast<long long>(spec.cols),
            static_cast<long long>(LUMICE_PATH_MAP_MAX_PIXELS));
}

TEST(AnalyzeMapView, DiskAtAPoleTakesTheFullAzimuthCircle) {
  MapViewState view;
  double centre[3];
  SkyPositionOf(20.0, 0.0, centre);  // the disk of radius 30 contains the sun frame's pole
  for (int k = 0; k < 3; ++k) {
    view.centre_pos[k] = static_cast<float>(centre[k]);
  }
  view.radius_deg = 30.0f;
  const MapWindowSpec spec = MapWindowSpecForView(view, 20.0, 0.0);
  EXPECT_NEAR(spec.azimuth_span_deg, 360.0, 1e-9);
  // And a full-sky view stays the engine's named all-sky view with the zero grid.
  MapViewState full;
  const MapWindowSpec full_spec = MapWindowSpecForView(full, 20.0, 0.0);
  EXPECT_TRUE(full_spec.full_sky);
  EXPECT_EQ(full_spec.rows, 0);
  EXPECT_EQ(full_spec.cols, 0);
}

TEST(AnalyzeMapView, RequestCarriesClassViewLambdaAndTheConstants) {
  MapIntentKey key;
  key.selection.crystal_id = 3;
  key.selection.faces = { 3, 5 };
  key.selection.layer_face_counts = { 2 };
  key.selection.generation = 7;
  double centre[3];
  SkyPositionOf(40.0, 0.0, centre);
  for (int k = 0; k < 3; ++k) {
    key.centre_pos[k] = static_cast<float>(centre[k]);
  }
  key.radius_deg = 10.0f;
  key.wavelength_nm = 0.0;  // the reference: no explicit buffer

  std::vector<int> faces;
  std::vector<int> counts;
  std::vector<double> wavelengths;
  const SkySunAnchor sun{ 20.0, 0.0 };
  const LUMICE_PathMapRequest request = MakeMapRequestForKey(key, sun, &faces, &counts, &wavelengths);
  EXPECT_EQ(request.crystal_id, 3);
  EXPECT_EQ(request.face_count, 2);
  EXPECT_EQ(faces, (std::vector<int>{ 3, 5 }));
  EXPECT_EQ(counts, (std::vector<int>{ 2 }));
  EXPECT_EQ(request.full_sky, 0);
  EXPECT_NEAR(request.centre_altitude_deg, 40.0, 1e-5);
  EXPECT_EQ(request.sample_count, kMapQuerySampleCount);
  EXPECT_EQ(request.budget_ms, LUMICE_PATH_MAP_DEFAULT_BUDGET_MS);
  EXPECT_EQ(request.wavelengths_nm, nullptr);
  EXPECT_EQ(request.wavelength_count, 0);

  // An explicit wavelength rides its own buffer.
  key.wavelength_nm = 650.0;
  const LUMICE_PathMapRequest red = MakeMapRequestForKey(key, sun, &faces, &counts, &wavelengths);
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

  // Refining: an adopted map stands while a NEWER intent is unanswered — never under a cancelled
  // intent, never with the intent answered, never without an adopted map at all.
  map.intent_id = SurveyQueryId{ 3, 1 };
  map.adopted_id = SurveyQueryId{ 3, 1 };
  EXPECT_FALSE(MapRefiningFor(map));
  map.intent_id = SurveyQueryId{ 3, 2 };
  EXPECT_TRUE(MapRefiningFor(map));
  map.cancelled_for_key = true;
  EXPECT_FALSE(MapRefiningFor(map));
  map.cancelled_for_key = false;
  map.adopted_result.reset();
  EXPECT_FALSE(MapRefiningFor(map));
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

// The headless frame loop: both pumps, at a clock the loop ADVANCES (a frame every ~50 ms, like
// the real app's — the settle gate consumes it, so a view change settles after a few frames).
template <typename Pred>
bool PumpBothUntil(GuiState& state, SurveyExecutor& executor, double now, Pred pred, int timeout_ms) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (!pred()) {
    if (std::chrono::steady_clock::now() > deadline) {
      return false;
    }
    PumpSurveyQuery(state, executor);
    PumpMapQuery(state, executor, now);
    now += 0.05;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  PumpSurveyQuery(state, executor);
  PumpMapQuery(state, executor, now);
  return true;
}

}  // namespace

TEST(AnalyzeMapView, PumpIssuesAdoptsAndRefinesThroughTheSettleGate) {
  LUMICE_Scene* raw_scene = nullptr;
  ASSERT_EQ(LUMICE_SceneFromJson(kMapSceneJson, &raw_scene), LUMICE_OK);
  std::unique_ptr<LUMICE_Scene, SceneDeleter> scene(raw_scene);
  SurveyExecutor executor;
  executor.SetScene(std::move(scene));

  GuiState state;
  state.analysis_result.payload = Rows(7, { "1-2", "1-3-2" }, { { 1, 2 }, { 1, 3, 2 } });
  state.analysis_result.analyzed_scene = GuiState::AnalysisSceneIdentity::From(state);
  state.analysis.selected_entry = "1-2";
  const double now = 100.0;  // the view starts settled: last_view_change 0, kMapSettleSeconds met

  // The first intent is the full sky (the default view chain); the arrival adopts.
  ASSERT_TRUE(PumpBothUntil(state, executor, now, [&] { return state.analysis.map.adopted_result != nullptr; }, 30000));
  const SurveyQueryId full_id = state.analysis.map.intent_id;
  EXPECT_EQ(state.analysis.map.key.radius_deg, kMapFullSkyRadiusDeg);
  EXPECT_EQ(MapQueryStateFor(state.analysis.map), MapQueryState::kCompleted);
  EXPECT_FALSE(MapRefiningFor(state.analysis.map));
  // The default view chain's auto-Fit fired once for this selection: the view moved off the
  // zenith and the refinement intent stands after the settle window.
  ASSERT_TRUE(PumpBothUntil(state, executor, now, [&] { return state.analysis.map.intent_id != full_id; }, 30000));
  EXPECT_TRUE(MapRefiningFor(state.analysis.map));  // the old picture shows while refining
  EXPECT_LT(state.analysis.map_view.radius_deg, kMapFullSkyRadiusDeg);

  // The refinement arrives: the picture on show answers the standing intent.
  ASSERT_TRUE(PumpBothUntil(
      state, executor, now, [&] { return state.analysis.map.adopted_id == state.analysis.map.intent_id; }, 30000));
  EXPECT_FALSE(MapRefiningFor(state.analysis.map));
  LUMICE_PathMapInfo info{};
  info.struct_size = sizeof(info);
  ASSERT_EQ(LUMICE_PathMapGetInfo(state.analysis.map.adopted_result.get(), &info), LUMICE_PATH_READ_OK);
  EXPECT_EQ(info.view_full_sky, 0);  // a real window map, not a mock
  executor.ShutdownAndJoin();
}

TEST(AnalyzeMapView, PumpWaitsForTheSettleBeforeIssuing) {
  LUMICE_Scene* raw_scene = nullptr;
  ASSERT_EQ(LUMICE_SceneFromJson(kMapSceneJson, &raw_scene), LUMICE_OK);
  std::unique_ptr<LUMICE_Scene, SceneDeleter> scene(raw_scene);
  SurveyExecutor executor;
  executor.SetScene(std::move(scene));

  GuiState state;
  state.analysis_result.payload = Rows(7, { "1-2" }, { { 1, 2 } });
  state.analysis_result.analyzed_scene = GuiState::AnalysisSceneIdentity::From(state);
  state.analysis.selected_entry = "1-2";
  const double gesture_time = 200.0;
  MapViewState& view = state.analysis.map_view;
  // A gesture frame changes the view but the request waits: mid-gesture nothing issues, the
  // settle window holds, and the burst ends as ONE request for the final view.
  double zoom_target[3] = { 0.0, 0.0, 1.0 };
  MapZoomViewAtPoint(&view, zoom_target, 1.0f / kMapZoomWheelFactor);
  MapViewChanged(&view, gesture_time, true);
  PumpMapQuery(state, executor, gesture_time);
  EXPECT_FALSE(state.analysis.map.intent_valid);  // mid-gesture: nothing issued
  MapViewChanged(&view, gesture_time, false);     // the gesture ends
  PumpMapQuery(state, executor, gesture_time);
  EXPECT_FALSE(state.analysis.map.intent_valid);  // still inside the settle window
  PumpMapQuery(state, executor, gesture_time + kMapSettleSeconds + 0.01);
  EXPECT_TRUE(state.analysis.map.intent_valid);  // one request for the whole burst
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
  state.analysis.selected_entry = "1-2";
  ASSERT_TRUE(
      PumpBothUntil(state, executor, 100.0, [&] { return state.analysis.map.adopted_result != nullptr; }, 30000));
  ASSERT_TRUE(state.analysis.map.adopted_result != nullptr);

  // Another class: the adopted map is gone with the selection's session — no lingering picture.
  // (The same-pump call has not re-issued yet: the auto-Fit left the view inside its settle
  // window at this clock, so the honest state is the pre-issue one.)
  state.analysis.selected_entry = "1-3-2";
  PumpMapQuery(state, executor, 100.0);
  EXPECT_FALSE(state.analysis.map.adopted_result != nullptr);
  EXPECT_FALSE(state.analysis.map.intent_valid);
  ASSERT_TRUE(
      PumpBothUntil(state, executor, 100.0, [&] { return state.analysis.map.adopted_result != nullptr; }, 30000));
  EXPECT_NE(state.analysis.map.key.selection.faces, (std::vector<int>{ 1, 2 }));  // the new class
  executor.ShutdownAndJoin();
}

// A hand on the view disarms the default chain: the first adopted map must not yank a view the
// user is already driving (r2 Major: auto-Fit used to fire on the first adoption whatever the
// user had done in the queueing window).
TEST(AnalyzeMapView, UserSteeredViewDisarmsTheAutoFit) {
  LUMICE_Scene* raw_scene = nullptr;
  ASSERT_EQ(LUMICE_SceneFromJson(kMapSceneJson, &raw_scene), LUMICE_OK);
  std::unique_ptr<LUMICE_Scene, SceneDeleter> scene(raw_scene);
  SurveyExecutor executor;
  executor.SetScene(std::move(scene));

  GuiState state;
  state.analysis_result.payload = Rows(7, { "1-2" }, { { 1, 2 } });
  state.analysis_result.analyzed_scene = GuiState::AnalysisSceneIdentity::From(state);
  state.analysis.selected_entry = "1-2";
  // The user pans and zooms while the first map is still in flight.
  MapViewState& view = state.analysis.map_view;
  double centre[3];
  SkyPositionOf(40.0, 30.0, centre);
  for (int k = 0; k < 3; ++k) {
    view.centre_pos[k] = static_cast<float>(centre[k]);
  }
  view.radius_deg = 30.0f;
  view.user_moved = true;
  ASSERT_TRUE(
      PumpBothUntil(state, executor, 100.0, [&] { return state.analysis.map.adopted_result != nullptr; }, 30000));
  // The arrival changed nothing: the view stands where the user put it, and no refinement
  // intent superseded it.
  double got[3] = { view.centre_pos[0], view.centre_pos[1], view.centre_pos[2] };
  EXPECT_NEAR(AngleBetween(got, centre), 0.0, 1e-6);
  EXPECT_NEAR(view.radius_deg, 30.0f, 1e-5);
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
  state.analysis.selected_entry = "1-2";
  state.analysis.roi_mode = LUMICE_RAYPATH_ROI_CONE;
  state.analysis.cone_center_valid = true;
  state.analysis.cone_center_dir[0] = 0.25f;
  state.analysis.pick_armed = true;

  ASSERT_TRUE(
      PumpBothUntil(state, executor, 100.0, [&] { return state.analysis.map.adopted_result != nullptr; }, 30000));
  // A map pick (the renderer's write, spelled here): the intent moves, the ROI state does not.
  double pos[3];
  ASSERT_TRUE(MapPickPositionAtDiskCoords(state.analysis.map_view, 0.5, 0.4, pos));
  state.analysis.sky_point.valid = true;
  state.analysis.sky_point.from_map_pick = true;
  state.analysis.sky_point.dir[0] = static_cast<float>(-pos[0]);
  state.analysis.sky_point.dir[1] = static_cast<float>(-pos[1]);
  state.analysis.sky_point.dir[2] = static_cast<float>(-pos[2]);

  EXPECT_EQ(state.analysis.roi_mode, LUMICE_RAYPATH_ROI_CONE);
  EXPECT_TRUE(state.analysis.cone_center_valid);
  EXPECT_NEAR(state.analysis.cone_center_dir[0], 0.25f, 1e-9);
  EXPECT_TRUE(state.analysis.pick_armed);
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

}  // namespace
}  // namespace lumice::gui
