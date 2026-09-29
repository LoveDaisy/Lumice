// The single-path analysis module (src/raypath/): the scene -> kernel conversions, the rank-0
// branch, the sun-direction grid against single evaluations, and the fibers' defining property —
// every point lands on the target. Oracles are restated here (the simulator's factory argument
// order, BuildCrystalRotation, the analyze --center direction formula) or are the kernel's own
// single-pose evaluator at an independently built pose, never the module's own helpers.
//
// symmetry_semantics: none — every case analyses one concrete face sequence (doc/analytic-api.md
// section 3).

#include <gtest/gtest.h>

#include <cmath>
#include <random>
#include <vector>

#include "analytic/path_evaluation.hpp"
#include "analytic/so3.hpp"
#include "config/config_manager.hpp"
#include "core/crystal.hpp"
#include "core/optics.hpp"
#include "core/simulator.hpp"
#include "raypath/scene_to_analytic.hpp"
#include "raypath/single_path_analysis.hpp"

namespace lumice::raypath {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;

// Small enough to keep every discovery case well under a second; the kernel's own tests pin the
// reference-default behaviour, these only need the components a 22-degree-halo target has.
constexpr int kSamples = 200000;

Distribution Fixed(float v) {
  return { DistributionType::kNoRandom, v, 0.0f };
}

PrismCrystalParam Prism(float height) {
  PrismCrystalParam p;
  p.h_ = Fixed(height);
  for (auto& d : p.d_) {
    d = Fixed(1.0f);
  }
  return p;
}

PyramidCrystalParam Pyramid(float prism_h, float upper_h, float lower_h, float upper_wedge, float lower_wedge) {
  PyramidCrystalParam p;
  p.h_prs_ = Fixed(prism_h);
  p.h_pyr_u_ = Fixed(upper_h);
  p.h_pyr_l_ = Fixed(lower_h);
  p.wedge_angle_u_ = upper_wedge;
  p.wedge_angle_l_ = lower_wedge;
  for (auto& d : p.d_) {
    d = Fixed(1.0f);
  }
  return p;
}

ConfigManager Scene(const CrystalParam& param, float sun_altitude = 20.0f,
                    std::vector<WlParam> spectrum = { { 550.0f, 1.0f } }) {
  ConfigManager c;
  CrystalConfig crystal;
  crystal.id_ = 1;
  crystal.param_ = param;
  c.crystals_.emplace(1, crystal);
  c.scene_.light_source_.param_ = SunParam{ sun_altitude, 0.0f, 0.5f };
  c.scene_.light_source_.spectrum_ = std::move(spectrum);
  return c;
}

SinglePathRequest Request(std::vector<int> faces, double alt, double az) {
  SinglePathRequest r;
  r.crystal_id = 1;
  r.path_layers = { std::move(faces) };
  r.target_altitude_deg = alt;
  r.target_azimuth_deg = az;
  r.sample_count = kSamples;
  r.sun_grid_lat_count = 0;
  return r;
}

double Angle(const double a[3], const double b[3]) {
  double c[3];
  analytic::so3::Cross3(a, b, c);
  return std::atan2(analytic::so3::Norm3(c), analytic::so3::Dot3(a, b));
}

// ---- conversions ----------------------------------------------------------------------------

TEST(SinglePathConvert, TakesTheCentreSlotOfEveryDistribution) {
  PrismCrystalParam p = Prism(1.2f);
  p.h_ = { DistributionType::kUniform, 0.5f, 0.4f };  // interval [0.3, 0.7]
  p.d_[2] = { DistributionType::kGaussian, 1.3f, 0.2f };
  p.d_[4] = { DistributionType::kLaplacian, 0.9f, 0.1f };
  const CrystalConversion c = ConvertCrystal(p);
  EXPECT_EQ(c.kind, "prism");
  EXPECT_EQ(c.shape.kind, analytic::CrystalShapeKind::kPrism);
  EXPECT_FLOAT_EQ(c.shape.height, 0.5f);
  EXPECT_FLOAT_EQ(c.shape.face_distance[0], 1.0f);
  EXPECT_FLOAT_EQ(c.shape.face_distance[2], 1.3f);
  EXPECT_FLOAT_EQ(c.shape.face_distance[4], 0.9f);
  EXPECT_TRUE(c.shape_is_nominal);
  ASSERT_EQ(c.scalars.size(), 7u);
  EXPECT_EQ(c.scalars[0].name, "height");
  EXPECT_EQ(c.scalars[0].distribution, DistributionType::kUniform);
  EXPECT_FLOAT_EQ(c.scalars[0].spread, 0.4f);
  EXPECT_EQ(c.scalars[3].name, "face_distance[2]");
  EXPECT_EQ(c.scalars[3].distribution, DistributionType::kGaussian);

  EXPECT_FALSE(ConvertCrystal(Prism(1.2f)).shape_is_nominal);
}

// The pyramid's fields reach the kernel in the simulator's factory order (simulator.cpp
// CrystalMaker: CreatePyramid(wedge_u, wedge_l, h_pyr_u, h_prs, h_pyr_l, d)): with every height and
// wedge distinct, a swapped field moves some corner, so equal corners slot by slot pin the mapping.
TEST(SinglePathConvert, PyramidFieldsReachTheSimulatorsFactoryArguments) {
  PyramidCrystalParam p = Pyramid(0.8f, 0.3f, 0.6f, 28.0f, 40.0f);
  p.d_[1] = Fixed(1.2f);
  const CrystalConversion c = ConvertCrystal(p);
  EXPECT_EQ(c.kind, "pyramid");
  EXPECT_EQ(c.scalars[0].name, "prism_h");
  EXPECT_EQ(c.scalars[1].name, "upper_h");
  EXPECT_EQ(c.scalars[2].name, "lower_h");
  analytic::FaceNormalTable table;
  analytic::FacePolygonTable polygons;
  ASSERT_EQ(analytic::BuildFaceNormals(c.shape, &table, &polygons), analytic::Status::kOk);

  float dist[6];
  for (int i = 0; i < 6; i++) {
    dist[i] = p.d_[i].center;
  }
  const Crystal engine = Crystal::CreatePyramid(p.wedge_angle_u_, p.wedge_angle_l_, p.h_pyr_u_.center, p.h_prs_.center,
                                                p.h_pyr_l_.center, dist);
  const CrystalGeom& g = engine.CfGeom();
  ASSERT_EQ(table.slot_cnt, g.face_cnt);
  int present = 0;
  for (int s = 0; s < g.face_cnt; s++) {
    EXPECT_EQ(table.present[s], g.face_present[s]) << "slot " << s;
    if (!g.face_present[s] || table.present[s] != g.face_present[s]) {
      continue;
    }
    present++;
    if (polygons.corner_cnt[s] != g.face_vtx_cnt[s]) {
      ADD_FAILURE() << "slot " << s << ": " << polygons.corner_cnt[s] << " vs " << g.face_vtx_cnt[s] << " corners";
      continue;
    }
    for (int k = 0; k < g.face_vtx_cnt[s]; k++) {
      for (int x = 0; x < 3; x++) {
        EXPECT_EQ(polygons.corner[s][k][x], g.face_vtx[(s * kCrystalGeomMaxVtxPerFace + k) * 3 + x]);
      }
    }
  }
  EXPECT_EQ(present, 20);  // both cones partial: every slot is present
}

TEST(SinglePathConvert, WavelengthRangeAndSources) {
  LightSourceConfig light;
  light.spectrum_ = std::vector<WlParam>{ { 450.0f, 1.0f }, { 650.0f, 1.0f } };
  WavelengthChoice w;
  ASSERT_TRUE(ResolveWavelength(light, std::nullopt, &w).Ok());
  EXPECT_EQ(w.source, WavelengthSource::kDefault);
  EXPECT_EQ(w.wavelength_nm, kDefaultWavelengthNm);
  EXPECT_EQ(w.refractive_index, IceRefractiveIndex::Get(kDefaultWavelengthNm));

  light.spectrum_ = std::vector<WlParam>{ { 480.0f, 1.0f } };
  ASSERT_TRUE(ResolveWavelength(light, std::nullopt, &w).Ok());
  EXPECT_EQ(w.source, WavelengthSource::kConfigSingle);
  EXPECT_EQ(w.wavelength_nm, 480.0);

  for (double nm : { 350.0, 900.0 }) {
    EXPECT_TRUE(ResolveWavelength(light, nm, &w).Ok()) << nm;
    EXPECT_EQ(w.source, WavelengthSource::kUser);
    EXPECT_GT(w.refractive_index, 1.2);
  }
  for (double nm : { 349.0, 901.0, std::nan("") }) {
    EXPECT_EQ(ResolveWavelength(light, nm, &w).code, ErrorCode::kWavelengthOutOfRange) << nm;
  }
}

TEST(SinglePathConvert, PathResolution) {
  analytic::FaceNormalTable table;
  ASSERT_EQ(analytic::BuildFaceNormals(ConvertCrystal(Prism(1.0f)).shape, &table), analytic::Status::kOk);
  std::vector<int> slots;
  EXPECT_TRUE(ResolveSingleLayerPath({ { 3, 5 } }, table, &slots).Ok());
  EXPECT_EQ(slots.size(), 2u);
  EXPECT_EQ(ResolveSingleLayerPath({}, table, &slots).code, ErrorCode::kInvalidPath);
  EXPECT_EQ(ResolveSingleLayerPath({ { 3 } }, table, &slots).code, ErrorCode::kInvalidPath);
  EXPECT_EQ(ResolveSingleLayerPath({ std::vector<int>(65, 3) }, table, &slots).code, ErrorCode::kInvalidPath);
  EXPECT_EQ(ResolveSingleLayerPath({ { 3, 5 }, { 1, 2 } }, table, &slots).code, ErrorCode::kMultiLayerUnsupported);
  const Error missing = ResolveSingleLayerPath({ { 3, 13 } }, table, &slots);
  EXPECT_EQ(missing.code, ErrorCode::kFaceNotInCrystal);
  EXPECT_NE(missing.message.find("13"), std::string::npos);
  EXPECT_TRUE(slots.empty());
  EXPECT_EQ(ResolveSingleLayerPath({ { 3, 99 } }, table, &slots).code, ErrorCode::kFaceNotInCrystal);
}

// Sun at (alpha, 0) sends its light along (-cos alpha, 0, -sin alpha) (SampleRayDir's cap centre);
// the target follows `analyze --center`: x = -cos(alt)cos(az), y = -cos(alt)sin(az), z = -sin(alt).
TEST(SinglePathConvert, SunAndTargetDirections) {
  SunParam sun{ 20.0f, 0.0f, 0.5f };
  double s[3];
  SunIncidentDirection(sun, s);
  EXPECT_NEAR(s[0], -std::cos(20.0 * kDeg), 1e-7);
  EXPECT_NEAR(s[1], 0.0, 1e-15);
  EXPECT_NEAR(s[2], -std::sin(20.0 * kDeg), 1e-7);
  EXPECT_NEAR(analytic::so3::Norm3(s), 1.0, 1e-15);

  SinglePathResult r;
  ASSERT_TRUE(AnalyzeSinglePath(Scene(Prism(1.0f)), Request({ 1, 2 }, 30.0, 90.0), &r).Ok());
  EXPECT_NEAR(r.meta.target_direction[0], 0.0, 1e-15);
  EXPECT_NEAR(r.meta.target_direction[1], -std::cos(30.0 * kDeg), 1e-15);
  EXPECT_NEAR(r.meta.target_direction[2], -std::sin(30.0 * kDeg), 1e-15);
  EXPECT_NEAR(r.meta.incident_direction[0], s[0], 0.0);
  // Great-circle distance between (20, 0) and (30, 90).
  const double expected = std::acos(std::sin(20.0 * kDeg) * std::sin(30.0 * kDeg));
  EXPECT_NEAR(r.meta.target_deviation_deg, expected / kDeg, 1e-5);
  EXPECT_EQ(r.meta.schema_version, kSchemaVersion);
  EXPECT_EQ(r.meta.analytic_api_version, analytic::kApiVersion);
  EXPECT_EQ(r.meta.wavelength_source, WavelengthSource::kConfigSingle);
  EXPECT_EQ(r.meta.faces, (std::vector<int>{ 1, 2 }));
}

TEST(SinglePathConvert, RequestErrors) {
  const ConfigManager scene = Scene(Prism(1.0f));
  SinglePathResult r;
  auto code = [&](SinglePathRequest q) { return AnalyzeSinglePath(scene, q, &r).code; };

  SinglePathRequest q = Request({ 3, 5 }, 20.0, 25.0);
  q.crystal_id = 7;
  EXPECT_EQ(code(q), ErrorCode::kUnknownCrystalId);
  q = Request({ 3, 5 }, 20.0, 25.0);
  q.sample_count = 0;
  EXPECT_EQ(code(q), ErrorCode::kInvalidArgument);
  q.sample_count = kMaxSampleCount + 1;
  EXPECT_EQ(code(q), ErrorCode::kInvalidArgument);
  q = Request({ 3, 5 }, 20.0, 25.0);
  q.sun_grid_lat_count = kMaxSunGridLatCount + 1;
  EXPECT_EQ(code(q), ErrorCode::kInvalidArgument);
  q = Request({ 3, 5 }, 20.0, 25.0);
  q.warm_seeds.assign(8, 0.0);
  EXPECT_EQ(code(q), ErrorCode::kInvalidArgument);
  q.warm_seeds.assign(9, 0.0);  // the zero matrix is no rotation
  EXPECT_EQ(code(q), ErrorCode::kInvalidArgument);
  EXPECT_EQ(code(Request({ 3, 5 }, 91.0, 0.0)), ErrorCode::kInvalidTarget);
  EXPECT_EQ(code(Request({ 3, 5 }, std::nan(""), 0.0)), ErrorCode::kInvalidTarget);
  // The target on the sun: a fiber needs a deviation strictly between 0 and pi.
  EXPECT_EQ(code(Request({ 3, 5 }, 20.0, 0.0)), ErrorCode::kInvalidTarget);
  EXPECT_EQ(code(Request({ 3, 5 }, 20.0, 25.0)), ErrorCode::kOk);
  q = Request({ 3, 5 }, 20.0, 25.0);
  q.wavelength_nm = 1000.0;
  EXPECT_EQ(code(q), ErrorCode::kWavelengthOutOfRange);
  EXPECT_EQ(code(Request({ 3, 13 }, 20.0, 25.0)), ErrorCode::kFaceNotInCrystal);
  q = Request({ 3, 5 }, 20.0, 25.0);
  q.path_layers.push_back({ 1, 2 });
  EXPECT_EQ(code(q), ErrorCode::kMultiLayerUnsupported);
  // A repeated face is reached from outside at every pose: no pose realises it.
  EXPECT_EQ(code(Request({ 3, 3 }, 20.0, 25.0)), ErrorCode::kPathInfeasible);

  // Opposite face distances summing to zero: the engine builds no crystal.
  PrismCrystalParam flat = Prism(1.0f);
  flat.d_[0] = Fixed(-1.0f);
  EXPECT_EQ(AnalyzeSinglePath(Scene(flat), Request({ 3, 5 }, 20.0, 25.0), &r).code, ErrorCode::kCrystalRejected);
  // On error the result is left empty.
  EXPECT_TRUE(r.meta.faces.empty());
}

// ---- pose angles ----------------------------------------------------------------------------

TEST(SinglePathPoseAngles, RoundTripThroughBuildCrystalRotation) {
  for (double zenith : { 0.0, 1e-3, 30.0, 90.0, 150.0, 179.999, 180.0 }) {
    for (double azimuth : { -179.0, -45.0, 0.0, 60.0, 180.0 }) {
      for (double roll : { -170.0, 0.0, 25.0, 179.0 }) {
        const Rotation rot =
            BuildCrystalRotation(static_cast<float>(azimuth * kDeg), static_cast<float>((90.0 - zenith) * kDeg),
                                 static_cast<float>(roll * kDeg));
        double r[9];
        for (int i = 0; i < 9; i++) {
          r[i] = rot.GetMat()[i];
        }
        const PoseAngles a = PoseToAngles(r);
        // Rebuild from the recovered angles and compare the matrices: at the poles only the sum
        // or difference of azimuth and roll is defined, and the matrix is what both must agree on.
        const Rotation back = BuildCrystalRotation(static_cast<float>(a.azimuth_deg * kDeg),
                                                   static_cast<float>((90.0 - a.zenith_deg) * kDeg),
                                                   static_cast<float>(a.roll_deg * kDeg));
        for (int i = 0; i < 9; i++) {
          EXPECT_NEAR(back.GetMat()[i], r[i], 2e-6) << zenith << " " << azimuth << " " << roll << " [" << i << "]";
        }
        EXPECT_NEAR(a.zenith_deg, zenith, 1e-3);
        if (zenith > 0.1 && zenith < 179.9) {
          EXPECT_FALSE(a.degenerate);
          EXPECT_NEAR(std::remainder(a.azimuth_deg - azimuth, 360.0), 0.0, 1e-3);
          EXPECT_NEAR(std::remainder(a.roll_deg - roll, 360.0), 0.0, 1e-3);
        }
        EXPECT_GT(a.azimuth_deg, -180.0);
        EXPECT_LE(a.azimuth_deg, 180.0);
      }
    }
  }
  const double identity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
  const PoseAngles pole = PoseToAngles(identity);
  EXPECT_TRUE(pole.degenerate);
  EXPECT_EQ(pole.roll_deg, 0.0);
  EXPECT_EQ(pole.zenith_deg, 0.0);
}

// ---- the sun-direction grid -------------------------------------------------------------------

// Each cell's deviation and validity equal the kernel's single-pose evaluation at a pose that puts
// the sun at that cell's u — built here with a random extra turn about the sun, which must not
// matter.
TEST(SinglePathSunGrid, MatchesSingleEvaluationsAtIndependentPoses) {
  const PyramidCrystalParam crystal = Pyramid(0.8f, 0.3f, 0.2f, 28.0f, 28.0f);
  SinglePathRequest q = Request({ 13, 15, 26, 28 }, 20.0, 40.0);
  q.sun_grid_lat_count = 30;
  q.sample_count = 1000;
  SinglePathResult r;
  const Error e = AnalyzeSinglePath(Scene(crystal), q, &r);
  ASSERT_TRUE(e.Ok()) << e.message;
  const SunSphereGrid& g = r.sun_grid;
  ASSERT_EQ(g.lat_count, 30);
  ASSERT_EQ(g.lon_count, 60);
  ASSERT_EQ(g.deviation_rad.size(), 1800u);

  analytic::FaceNormalTable table;
  ASSERT_EQ(analytic::BuildFaceNormals(ConvertCrystal(crystal).shape, &table), analytic::Status::kOk);
  std::vector<int> slots;
  ASSERT_TRUE(ResolveSingleLayerPath(q.path_layers, table, &slots).Ok());
  const double* incident = r.meta.incident_direction;
  const double sun[3] = { -incident[0], -incident[1], -incident[2] };
  std::vector<double> segments(3 * (slots.size() + 1));
  std::vector<double> transmittances(slots.size());

  std::mt19937 rng(20260929);
  std::uniform_int_distribution<int> lat(0, g.lat_count - 1);
  std::uniform_int_distribution<int> lon(0, g.lon_count - 1);
  std::uniform_real_distribution<double> spin(-kPi, kPi);
  int valid = 0;
  for (int trial = 0; trial < 200; trial++) {
    const int i = lat(rng);
    const int j = lon(rng);
    double u[3];
    SunSphereGridCellCentre(g, i, j, u);
    // R0 u = sun (Rodrigues about u x sun), then a turn about the sun.
    double axis[3];
    analytic::so3::Cross3(u, sun, axis);
    const double sine = analytic::so3::Norm3(axis);
    const double angle = std::atan2(sine, analytic::so3::Dot3(u, sun));
    double w0[3];
    for (int k = 0; k < 3; k++) {
      w0[k] = axis[k] / sine * angle;
    }
    double r0[9];
    analytic::so3::Exp(w0, r0);
    const double theta = spin(rng);
    const double w1[3] = { sun[0] * theta, sun[1] * theta, sun[2] * theta };
    double r1[9];
    analytic::so3::Exp(w1, r1);
    double pose[9];
    analytic::so3::MatMul(r1, r0, pose);

    analytic::PathOutputs out{};
    out.segment_directions = segments.data();
    out.interface_transmittances = transmittances.data();
    const bool ok = analytic::EvaluatePath(table, slots.data(), static_cast<int>(slots.size()), r.meta.refractive_index,
                                           incident, pose, &out);
    const size_t cell = static_cast<size_t>(i) * g.lon_count + j;
    if (ok != (g.valid[cell] != 0)) {
      ADD_FAILURE() << "cell " << i << "," << j << ": grid validity " << int(g.valid[cell]) << ", evaluation " << ok;
      continue;
    }
    if (!ok) {
      EXPECT_TRUE(std::isnan(g.deviation_rad[cell]));
      continue;
    }
    valid++;
    EXPECT_NEAR(g.deviation_rad[cell], Angle(out.outgoing_direction, incident), 1e-9) << "cell " << i << "," << j;
    EXPECT_GE(g.entry_measure[cell], 0.0);
  }
  EXPECT_GT(valid, 10);  // the comparison ran on real cells, not only on invalid ones
}

// ---- rank 0 ---------------------------------------------------------------------------------

// A parallel face pair is a plate: the outgoing ray is the incident one at every pose, so there is
// no fiber to trace — only the one sky point, the sun itself.
TEST(SinglePathRankZero, AParallelFacePairIsAPointMass) {
  for (const std::vector<int>& faces : { std::vector<int>{ 1, 2 }, std::vector<int>{ 3, 6 } }) {
    SinglePathRequest q = Request(faces, 20.0, 25.0);
    q.sun_grid_lat_count = 10;
    SinglePathResult r;
    if (!AnalyzeSinglePath(Scene(Prism(1.0f)), q, &r).Ok()) {
      ADD_FAILURE() << faces[0] << "-" << faces[1] << " failed";
      continue;
    }
    EXPECT_EQ(r.outcome, Outcome::kPointMass);
    EXPECT_TRUE(r.components.empty());
    EXPECT_TRUE(r.incomplete.empty());
    EXPECT_EQ(r.discovery.pool_count, 0);  // discovery never ran
    EXPECT_LT(Angle(r.point_mass.direction, r.meta.incident_direction), 1e-12);
    EXPECT_NEAR(r.point_mass.altitude_deg, 20.0, 1e-5);
    EXPECT_NEAR(r.point_mass.azimuth_deg, 0.0, 1e-5);
    EXPECT_NEAR(r.point_mass.target_separation_deg, r.meta.target_deviation_deg, 1e-9);
    EXPECT_EQ(r.sun_grid.lat_count, 10);  // the grid is still drawn
  }
  // The sun is a legal target for a point mass (no fiber needs defining there).
  SinglePathResult r;
  ASSERT_TRUE(AnalyzeSinglePath(Scene(Prism(1.0f)), Request({ 1, 2 }, 20.0, 0.0), &r).Ok());
  EXPECT_EQ(r.outcome, Outcome::kPointMass);
  EXPECT_LT(r.point_mass.target_separation_deg, 1e-5);
}

// A 2-degree wedge between the entry and exit faces: nearly a plate, but the outgoing direction does
// move with the pose, so it must not be called rank 0. Measured margins of the deviation test the
// module uses: this path's deviation reaches 0.1..0.3 rad at some probe (a tolerance of 0.3 turns
// this case red), an exact plate's stays below 1e-13 rad (a tolerance of 1e-14 turns the case above
// red); the tolerance, 1e-10, sits between them.
TEST(SinglePathRankZero, ANearlyParallelPairIsNotAPointMass) {
  SinglePathResult r;
  const Error e = AnalyzeSinglePath(Scene(Pyramid(1.0f, 0.5f, 0.0f, 2.0f, 28.0f)), Request({ 3, 16 }, 20.0, 25.0), &r);
  ASSERT_TRUE(e.Ok()) << e.message;
  EXPECT_EQ(r.outcome, Outcome::kDiscovered);
}

// ---- discovery and fibers ---------------------------------------------------------------------

void ExpectFibersLandOnTheTarget(const SinglePathResult& r) {
  for (const FiberComponent& c : r.components) {
    if (c.points.empty()) {
      ADD_FAILURE() << "a component with no points";
      continue;
    }
    EXPECT_EQ(c.arclength_increments.size(), c.points.size() - 1);
    // The seed sits where the component says it does.
    const PointDetail& seed = c.points[c.seed_index];
    for (int i = 0; i < 9; i++) {
      EXPECT_EQ(seed.pose[i], c.seed[i]);
    }
    EXPECT_TRUE(seed.valid);
    EXPECT_GT(seed.entry_measure, 0.0);  // discovery only admits seeds with a positive entry measure
    for (const PointDetail& p : c.points) {
      if (!p.valid) {
        continue;  // an arc's end pose sits on its boundary event
      }
      EXPECT_LT(Angle(p.outgoing_direction, r.meta.target_direction), 1e-8);
      double product = 1.0;
      for (double t : p.interface_transmittances) {
        product *= t;
      }
      EXPECT_NEAR(p.total_transmission, product, 1e-15);
      if (p.segment_directions.size() != 3 * (r.meta.faces.size() + 1)) {
        ADD_FAILURE() << p.segment_directions.size() << " segment values for " << r.meta.faces.size() << " faces";
        continue;
      }
      // The first segment is the sun's light in the crystal frame: -u.
      for (int k = 0; k < 3; k++) {
        EXPECT_NEAR(p.segment_directions[k], -p.sun_in_crystal[k], 1e-12);
      }
    }
    if (c.kind == ComponentKind::kClosed) {
      EXPECT_EQ(c.forward.status, TraceStatus::kClosed);
      EXPECT_EQ(c.seed_index, 0);
      EXPECT_LT(analytic::so3::Distance(c.points.front().pose, c.points.back().pose), 1e-6);
    } else {
      EXPECT_EQ(c.backward.pose_count + c.forward.pose_count - 1, static_cast<int>(c.points.size()));
    }
  }
}

TEST(SinglePathDiscovery, TwentyTwoDegreeFibersLandOnTheTarget) {
  SinglePathResult r;
  ASSERT_TRUE(AnalyzeSinglePath(Scene(Prism(1.0f)), Request({ 3, 5 }, 20.0, 25.0), &r).Ok());
  EXPECT_EQ(r.outcome, Outcome::kDiscovered);
  ASSERT_FALSE(r.components.empty());
  EXPECT_EQ(r.discovery.admissible_count,
            r.discovery.dedup_merged + static_cast<int>(r.components.size()) + static_cast<int>(r.incomplete.size()));
  EXPECT_EQ(r.discovery.complete, r.incomplete.empty());
  ExpectFibersLandOnTheTarget(r);
}

// Inside the 22-degree halo's inner edge no pose sends 3-5 light: the answer is "none", and it is a
// successful, complete answer rather than an error.
TEST(SinglePathDiscovery, ADarkTargetIsCompleteWithNoComponent) {
  SinglePathResult r;
  ASSERT_TRUE(AnalyzeSinglePath(Scene(Prism(1.0f)), Request({ 3, 5 }, 20.0, 8.0), &r).Ok());
  EXPECT_EQ(r.outcome, Outcome::kDiscovered);
  EXPECT_TRUE(r.components.empty());
  EXPECT_TRUE(r.discovery.complete);
  EXPECT_EQ(r.discovery.pool_count, 0);
}

// Seeds fed back are Gauss-Newton starts of their own clusters: a second call that has them cannot
// come back with fewer components, even from a sparser sample.
TEST(SinglePathDiscovery, WarmSeedsKeepEveryComponent) {
  const ConfigManager scene = Scene(Prism(1.0f));
  SinglePathResult first;
  ASSERT_TRUE(AnalyzeSinglePath(scene, Request({ 3, 5, 6, 7 }, 18.0, 175.0), &first).Ok());
  ASSERT_FALSE(first.components.empty());  // otherwise there is nothing to keep
  SinglePathRequest q = Request({ 3, 5, 6, 7 }, 18.0, 175.0);
  q.sample_count = 1000;
  for (const FiberComponent& c : first.components) {
    q.warm_seeds.insert(q.warm_seeds.end(), c.seed, c.seed + 9);
  }
  SinglePathResult second;
  ASSERT_TRUE(AnalyzeSinglePath(scene, q, &second).Ok());
  EXPECT_EQ(second.meta.warm_seed_count, static_cast<int>(first.components.size()));
  EXPECT_EQ(second.discovery.extra_seed_count, static_cast<int>(first.components.size()));
  EXPECT_GE(second.components.size(), first.components.size());
  ExpectFibersLandOnTheTarget(second);
}

}  // namespace
}  // namespace lumice::raypath
