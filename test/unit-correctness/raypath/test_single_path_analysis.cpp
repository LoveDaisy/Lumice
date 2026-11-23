// The single-path analysis module (src/raypath/): the scene -> kernel conversions, the rank-0
// branch, the sun-direction grid against single evaluations, and the fibers' defining property —
// every point lands on the target. Oracles are restated here (the simulator's factory argument
// order, BuildCrystalRotation, the analyze --center direction formula) or are the kernel's own
// single-pose evaluator at an independently built pose, never the module's own helpers.
//
// symmetry_semantics: none — every case analyses one concrete face sequence (doc/interfaces/analytic-api.md
// section 3).

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>
#include <random>
#include <utility>
#include <vector>

#include "analytic/entry_measure.hpp"
#include "analytic/fiber_continuation.hpp"
#include "analytic/path_evaluation.hpp"
#include "analytic/path_fiber.hpp"
#include "analytic/so3.hpp"
#include "config/config_manager.hpp"
#include "core/crystal.hpp"
#include "core/optics.hpp"
#include "core/simulator.hpp"
#include "raypath/detail/measure/declared_density.hpp"
#include "raypath/detail/survey/config_fingerprint.hpp"
#include "raypath/prepared_geometry.hpp"
#include "raypath/representative_ray.hpp"
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

TEST(SurveyReuse, StructureAndReadingHaveSeparateScopeDependencies) {
  const ConfigManager config = Scene(Prism(1.2f));
  PathSelection selection;
  selection.crystal_id = 1;
  selection.path_layers = { { 3, 5 } };
  WarmCandidate candidate;
  candidate.crystal_id = 1;
  candidate.path_layers = selection.path_layers;
  candidate.location = ScopeLocation{ SelectionLocation::kStandalone, 0 };
  candidate.config_fingerprint =
      survey::RoutingConfigFingerprint(config, selection.crystal_id, selection.path_layers, selection.symmetry_bits);
  SurveyWarmSource& warm = candidate.facts;
  warm.family = WarmDocumentFamily::kSurvey;
  warm.index_present = true;
  warm.has_support = true;
  warm.intervals.push_back({ 21.9, 40.0, { 3, 5 } });
  warm.has_wavelength = true;
  warm.wavelength_nm = 550.0;
  warm.has_sun_altitude = true;
  warm.sun_altitude_deg = 20.0;
  warm.has_sun_azimuth = true;
  warm.sun_azimuth_deg = 0.0;
  const auto admit = [&] {
    std::optional<AdmittedWarmSource> admitted;
    EXPECT_TRUE(ValidateWarmCandidate(config, selection, candidate, &admitted).Ok());
    return admitted;
  };

  SurveyContextBlock context;
  context.source = ContextSource::kWarm;
  auto admitted = admit();
  ASSERT_TRUE(admitted);
  FillSurveyContext(*admitted, 22.1, 550.0, 20.0, 0.0, &context);
  EXPECT_EQ(context.object.state, survey::EvidenceState::kCertified);
  EXPECT_TRUE(context.target_inside_support);
  EXPECT_EQ(context.edge.state, survey::EvidenceState::kFinite);

  context = SurveyContextBlock{};
  context.source = ContextSource::kWarm;
  FillSurveyContext(*admitted, 22.1, 450.0, 20.0, 0.0, &context);
  EXPECT_EQ(context.object.reason, "scope_value_mismatch");
  EXPECT_EQ(context.edge.reason, "scope_value_mismatch");

  context = SurveyContextBlock{};
  context.source = ContextSource::kWarm;
  FillSurveyContext(*admitted, 41.0, 550.0, 30.0, 0.0, &context);
  EXPECT_EQ(context.object.state, survey::EvidenceState::kCertified);
  EXPECT_FALSE(context.target_inside_support);
  EXPECT_DOUBLE_EQ(context.object_distance_deg, 1.0);

  warm.has_sun_altitude = false;
  warm.has_probe = true;
  admitted = admit();
  ASSERT_TRUE(admitted);
  context = SurveyContextBlock{};
  context.source = ContextSource::kWarm;
  FillSurveyContext(*admitted, 22.1, 550.0, 20.0, 0.0, &context);
  EXPECT_EQ(context.object.state, survey::EvidenceState::kCertified);
  EXPECT_EQ(context.band_sum.reason, "scope_field_missing");
  warm.has_wavelength = false;
  admitted = admit();
  ASSERT_TRUE(admitted);
  context = SurveyContextBlock{};
  context.source = ContextSource::kWarm;
  FillSurveyContext(*admitted, 22.1, 550.0, 20.0, 0.0, &context);
  EXPECT_EQ(context.object.reason, "scope_field_missing");
  EXPECT_EQ(context.edge.reason, "scope_field_missing");
  EXPECT_EQ(context.band_sum.reason, "scope_field_missing");
}

TEST(SingleFacePoint, AntisolarTargetHasRegularReflectionFiber) {
  ConfigManager config = Scene(Prism(1.0f));
  auto& axis = config.crystals_.at(1).axis_;
  axis.latitude_dist = { DistributionType::kUniform, 90, 360 };
  axis.azimuth_dist = axis.roll_dist = { DistributionType::kUniform, 0, 360 };
  PointResult point;
  const auto error = AnalyzePathPointValue(config, Request({ 3 }, -20.0, 180.0), false, &point);
  ASSERT_TRUE(error.Ok()) << error.message;
  ASSERT_EQ(point.Data().OutcomeValue(), Outcome::kDiscovered);
  ASSERT_EQ(point.Data().Discovered().components.size(), 1u);
  const auto& component = point.Data().Discovered().components.front();
  EXPECT_EQ(component.kind, ComponentKind::kClosed);
  ASSERT_GT(component.points.size(), 4u);
  for (const auto& sample : component.points) {
    EXPECT_TRUE(sample.valid);
    EXPECT_GT(sample.entry_measure, 0.0);
    EXPECT_NEAR(sample.jacobian, 4.0, 1e-10);
    for (int k = 0; k < 3; ++k) {
      EXPECT_NEAR(sample.outgoing_direction[k], -point.Data().meta.incident_direction[k], 1e-8);
    }
  }
  RepresentativeRayRequest ray_request;
  ray_request.crystal_id = 1;
  ray_request.path_layers = { { 3 } };
  std::copy(component.points.front().pose, component.points.front().pose + 9, ray_request.pose);
  RepresentativeRayResult ray;
  ASSERT_TRUE(AnalyzeRepresentativeRay(config, ray_request, &ray).Ok());
  ASSERT_TRUE(ray.Data().Produced());
  EXPECT_EQ(ray.Data().Path().contacts.size(), 1u);
  ASSERT_EQ(ray.Data().Path().legs.size(), 2u);
  for (int k = 0; k < 3; ++k) {
    EXPECT_NEAR(ray.Data().Path().legs[1].direction[k], -point.Data().meta.incident_direction[k], 1e-8);
  }
}

TEST(SingleFacePoint, FixedNormalMassLandsAtReflectionNotSun) {
  ConfigManager config = Scene(Prism(1.0f));
  auto& axis = config.crystals_.at(1).axis_;
  axis.latitude_dist = Fixed(90.0f);  // zenith 0: basal normal stays world +z
  axis.azimuth_dist = axis.roll_dist = { DistributionType::kUniform, 0, 360 };
  PointResult point;
  ASSERT_TRUE(AnalyzePathPointValue(config, Request({ 1 }, -20.0, 0.0), false, &point).Ok());
  ASSERT_EQ(point.Data().OutcomeValue(), Outcome::kPointMass);
  EXPECT_NEAR(point.Data().Mass().altitude_deg, -20.0, 1e-12);
  EXPECT_NEAR(point.Data().Mass().azimuth_deg, 0.0, 1e-12);
  EXPECT_NEAR(point.Data().Mass().target_separation_deg, 0.0, 1e-12);
  EXPECT_FALSE(point.Data().Mass().orbit_factors.available);
  EXPECT_EQ(point.Data().Mass().orbit_factors.reason, "external_reflection_mass_not_integrated");
  axis.latitude_dist = { DistributionType::kGaussian, 90, 0 };
  ASSERT_TRUE(AnalyzePathPointValue(config, Request({ 1 }, -20.0, 0.0), false, &point).Ok());
  ASSERT_EQ(point.Data().OutcomeValue(), Outcome::kPointMass);  // zero-width Gaussian is fixed, not a density
  axis.latitude_dist = Fixed(90.0f);
  axis.azimuth_dist = Fixed(37.0f);
  // A frozen azimuth is outside the survey routing domain, but that numerical
  // limitation cannot invalidate a direct proof that this basal normal is fixed.
  ASSERT_TRUE(AnalyzePathPointValue(config, Request({ 1 }, -20.0, 0.0), false, &point).Ok());
  ASSERT_EQ(point.Data().OutcomeValue(), Outcome::kPointMass);
  EXPECT_NEAR(point.Data().Mass().altitude_deg, -20.0, 1e-12);
  axis.roll_dist = Fixed(19.0f);
  ASSERT_TRUE(AnalyzePathPointValue(config, Request({ 1 }, -20.0, 0.0), false, &point).Ok());
  ASSERT_EQ(point.Data().OutcomeValue(), Outcome::kPointMass);
  EXPECT_NEAR(point.Data().Mass().altitude_deg, -20.0, 1e-12);
  EXPECT_EQ(point.Data().Mass().orbit_factors.measure_support, "single_pose");
  const auto backface = AnalyzePathPointValue(config, Request({ 2 }, 20.0, 0.0), false, &point);
  EXPECT_EQ(backface.code, ErrorCode::kPathInfeasible);
  EXPECT_EQ(backface.message, "external_reflection_backface_on_actual_family");
}

TEST(SingleFacePoint, VaryingNormalFamilyIsNotMassAndHasNoFakeContinuousDensity) {
  ConfigManager config = Scene(Prism(1.0f));
  auto& axis = config.crystals_.at(1).axis_;
  axis.latitude_dist = Fixed(90.0f);
  axis.azimuth_dist = axis.roll_dist = { DistributionType::kUniform, 0, 360 };
  PointResult point;
  ASSERT_TRUE(AnalyzePathPointValue(config, Request({ 3 }, 20.0, 90.0), false, &point).Ok());
  ASSERT_EQ(point.Data().OutcomeValue(), Outcome::kDiscovered);
  ASSERT_FALSE(point.Data().Discovered().components.empty());
  const auto& sample = point.Data().Discovered().components.front().points.front();
  ASSERT_TRUE(sample.valid);
  EXPECT_GT(sample.entry_measure, 0.0);
  EXPECT_TRUE(std::isnan(sample.rho));
  EXPECT_TRUE(std::isnan(sample.factor_product));
  // The returned geometric pose really sends the incident direction to the target.
  const double* r = sample.pose;
  const double normal[3] = { r[0], r[3], r[6] };
  const auto& meta = point.Data().meta;
  double dot = 0.0;
  for (int i = 0; i < 3; ++i) {
    dot += meta.incident_direction[i] * normal[i];
  }
  for (int i = 0; i < 3; ++i) {
    EXPECT_NEAR(meta.incident_direction[i] - 2.0 * dot * normal[i], meta.target_direction[i], 1e-8);
  }
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
// The kernel's corners are the engine's refined to double from the double fields (BuildFaceNormals),
// so "equal" is to the engine's float rounding: 1e-6 of the crystal's size, where a swapped field
// moves a corner by a tenth of it.
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
        EXPECT_NEAR(polygons.corner[s][k][x], g.face_vtx[(s * kCrystalGeomMaxVtxPerFace + k) * 3 + x], 1e-6);
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
  EXPECT_TRUE(ResolveSingleLayerPath({ { 3 } }, table, &slots).Ok());
  EXPECT_EQ(slots.size(), 1u);
  EXPECT_EQ(ResolveSingleLayerPath({ {} }, table, &slots).code, ErrorCode::kInvalidPath);
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

  PointResult r;
  ASSERT_TRUE(AnalyzePathPointValue(Scene(Prism(1.0f)), Request({ 1, 2 }, 30.0, 90.0), false, &r).Ok());
  EXPECT_NEAR(r.Data().meta.target_direction[0], 0.0, 1e-15);
  EXPECT_NEAR(r.Data().meta.target_direction[1], -std::cos(30.0 * kDeg), 1e-15);
  EXPECT_NEAR(r.Data().meta.target_direction[2], -std::sin(30.0 * kDeg), 1e-15);
  EXPECT_NEAR(r.Data().meta.incident_direction[0], s[0], 0.0);
  // Great-circle distance between (20, 0) and (30, 90).
  const double expected = std::acos(std::sin(20.0 * kDeg) * std::sin(30.0 * kDeg));
  EXPECT_NEAR(r.Data().meta.target_deviation_deg, expected / kDeg, 1e-5);
  EXPECT_EQ(r.Data().meta.schema_version, kSchemaVersion);
  EXPECT_EQ(r.Data().meta.analytic_api_version, analytic::kApiVersion);
  EXPECT_EQ(r.Data().meta.wavelength_source, WavelengthSource::kConfigSingle);
  EXPECT_EQ(r.Data().meta.faces, (std::vector<int>{ 1, 2 }));
}

TEST(SinglePathConvert, RequestErrors) {
  const ConfigManager scene = Scene(Prism(1.0f));
  PointResult r;
  auto code = [&](SinglePathRequest q) { return AnalyzePathPointValue(scene, q, false, &r).code; };

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
  EXPECT_EQ(AnalyzePathPointValue(Scene(flat), Request({ 3, 5 }, 20.0, 25.0), false, &r).code,
            ErrorCode::kCrystalRejected);
  // On error the result is left empty.
  EXPECT_TRUE(r.Data().meta.faces.empty());
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
  PointResult r;
  const Error e = AnalyzePathPointValue(Scene(crystal), q, false, &r);
  ASSERT_TRUE(e.Ok()) << e.message;
  const SunSphereGrid& g = r.Data().sun_grid;
  ASSERT_EQ(g.lat_count, 30);
  ASSERT_EQ(g.lon_count, 60);
  ASSERT_EQ(g.deviation_rad.size(), 1800u);

  analytic::FaceNormalTable table;
  ASSERT_EQ(analytic::BuildFaceNormals(ConvertCrystal(crystal).shape, &table), analytic::Status::kOk);
  std::vector<int> slots;
  ASSERT_TRUE(ResolveSingleLayerPath(q.path_layers, table, &slots).Ok());
  const double* incident = r.Data().meta.incident_direction;
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
    const bool ok = analytic::EvaluatePath(table, slots.data(), static_cast<int>(slots.size()),
                                           r.Data().meta.refractive_index, incident, pose, &out);
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
    PointResult r;
    if (!AnalyzePathPointValue(Scene(Prism(1.0f)), q, false, &r).Ok()) {
      ADD_FAILURE() << faces[0] << "-" << faces[1] << " failed";
      continue;
    }
    EXPECT_EQ(r.Data().OutcomeValue(), Outcome::kPointMass);
    EXPECT_EQ(std::get_if<DiscoveredPoint>(&r.Data().payload), nullptr);  // discovery never ran
    EXPECT_LT(Angle(r.Data().Mass().direction, r.Data().meta.incident_direction), 1e-12);
    EXPECT_NEAR(r.Data().Mass().altitude_deg, 20.0, 1e-5);
    EXPECT_NEAR(r.Data().Mass().azimuth_deg, 0.0, 1e-5);
    EXPECT_NEAR(r.Data().Mass().target_separation_deg, r.Data().meta.target_deviation_deg, 1e-9);
    EXPECT_EQ(r.Data().sun_grid.lat_count, 10);  // the grid is still drawn
  }
  // The sun is a legal target for a point mass (no fiber needs defining there).
  PointResult r;
  ASSERT_TRUE(AnalyzePathPointValue(Scene(Prism(1.0f)), Request({ 1, 2 }, 20.0, 0.0), false, &r).Ok());
  EXPECT_EQ(r.Data().OutcomeValue(), Outcome::kPointMass);
  EXPECT_LT(r.Data().Mass().target_separation_deg, 1e-5);
}

// A 2-degree wedge between the entry and exit faces: nearly a plate, but the outgoing direction does
// move with the pose, so it must not be called rank 0. Measured margins of the deviation test the
// module uses: this path's deviation reaches 0.1..0.3 rad at some probe (a tolerance of 0.3 turns
// this case red), an exact plate's stays below 1e-13 rad (a tolerance of 1e-14 turns the case above
// red); the tolerance, 1e-10, sits between them.
TEST(SinglePathRankZero, ANearlyParallelPairIsNotAPointMass) {
  PointResult r;
  const Error e =
      AnalyzePathPointValue(Scene(Pyramid(1.0f, 0.5f, 0.0f, 2.0f, 28.0f)), Request({ 3, 16 }, 20.0, 25.0), false, &r);
  ASSERT_TRUE(e.Ok()) << e.message;
  EXPECT_EQ(r.Data().OutcomeValue(), Outcome::kDiscovered);
}

// ---- discovery and fibers ---------------------------------------------------------------------

void ExpectFibersLandOnTheTarget(const PointResult& r) {
  for (const FiberComponent& c : r.Data().Discovered().components) {
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
      EXPECT_LT(Angle(p.outgoing_direction, r.Data().meta.target_direction), 1e-8);
      double product = 1.0;
      for (double t : p.interface_transmittances) {
        product *= t;
      }
      EXPECT_NEAR(p.total_transmission, product, 1e-15);
      if (p.segment_directions.size() != 3 * (r.Data().meta.faces.size() + 1)) {
        ADD_FAILURE() << p.segment_directions.size() << " segment values for " << r.Data().meta.faces.size()
                      << " faces";
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
  PointResult r;
  ASSERT_TRUE(AnalyzePathPointValue(Scene(Prism(1.0f)), Request({ 3, 5 }, 20.0, 25.0), false, &r).Ok());
  EXPECT_EQ(r.Data().OutcomeValue(), Outcome::kDiscovered);
  ASSERT_FALSE(r.Data().Discovered().components.empty());
  EXPECT_EQ(r.Data().Discovered().discovery.admissible_count,
            r.Data().Discovered().discovery.dedup_merged + static_cast<int>(r.Data().Discovered().components.size()) +
                static_cast<int>(r.Data().Discovered().incomplete.size()));
  EXPECT_EQ(r.Data().Discovered().discovery.complete, r.Data().Discovered().incomplete.empty());
  ExpectFibersLandOnTheTarget(r);
}

// Inside the 22-degree halo's inner edge no pose sends 3-5 light: the answer is "none", and it is a
// successful, complete answer rather than an error.
TEST(SinglePathDiscovery, ADarkTargetIsCompleteWithNoComponent) {
  PointResult r;
  ASSERT_TRUE(AnalyzePathPointValue(Scene(Prism(1.0f)), Request({ 3, 5 }, 20.0, 8.0), false, &r).Ok());
  EXPECT_EQ(r.Data().OutcomeValue(), Outcome::kDiscovered);
  EXPECT_TRUE(r.Data().Discovered().components.empty());
  EXPECT_TRUE(r.Data().Discovered().discovery.complete);
  EXPECT_EQ(r.Data().Discovered().discovery.pool_count, 0);
  // And the reason is geometric: the target's deviation is below 3-5's minimum deviation.
  EXPECT_FALSE(r.Data().Discovered().reach.target_in_range);
  EXPECT_LT(r.Data().Discovered().reach.target_deviation_rad,
            r.Data().Discovered().reach.deviation_min_rad - r.Data().Discovered().reach.tolerance_rad);
}

// ---- reach: the two empty outcomes --------------------------------------------------------------

// 3-6-4-8 (four prism faces: a turn about the c-axis) has D depending only on the sun's latitude in
// the crystal, from 0 at the pole to exactly 120 degrees at the equator. At (20, 120), delta =
// 108.94 degrees: a prism of height 0.8 passes rays along part of that fiber, one of height 0.5
// passes none. At (20, 140), delta = 124.02 degrees exceeds every D: no pose of any crystal reaches
// it. Both empty results look alike in `components`; reach tells them apart, and — validity being
// direction-level — says the same thing about both heights.
TEST(SinglePathReach, SeparatesUnreachableFromBlockedByTheFiniteCrystal) {
  PointResult tall;
  PointResult flat;
  PointResult beyond;
  ASSERT_TRUE(AnalyzePathPointValue(Scene(Prism(0.8f)), Request({ 3, 6, 4, 8 }, 20.0, 120.0), false, &tall).Ok());
  ASSERT_TRUE(AnalyzePathPointValue(Scene(Prism(0.5f)), Request({ 3, 6, 4, 8 }, 20.0, 120.0), false, &flat).Ok());
  ASSERT_TRUE(AnalyzePathPointValue(Scene(Prism(0.5f)), Request({ 3, 6, 4, 8 }, 20.0, 140.0), false, &beyond).Ok());

  EXPECT_FALSE(tall.Data().Discovered().components.empty());
  EXPECT_TRUE(tall.Data().Discovered().reach.target_in_range);

  EXPECT_TRUE(flat.Data().Discovered().components.empty());
  EXPECT_TRUE(flat.Data().Discovered().reach.target_in_range);  // reachable, blocked by the finite crystal

  EXPECT_TRUE(beyond.Data().Discovered().components.empty());
  EXPECT_FALSE(beyond.Data().Discovered().reach.target_in_range);  // geometrically unreachable

  // Direction-level: the crystal's height does not enter the range.
  EXPECT_EQ(tall.Data().Discovered().reach.deviation_min_rad, flat.Data().Discovered().reach.deviation_min_rad);
  EXPECT_EQ(tall.Data().Discovered().reach.deviation_max_rad, flat.Data().Discovered().reach.deviation_max_rad);
  EXPECT_EQ(tall.Data().Discovered().reach.tolerance_rad, flat.Data().Discovered().reach.tolerance_rad);
}

// The probed range is made of attained values, and the tolerance reaches the exact range [0, 120 deg]
// that the probe itself approaches from inside at both ends (a cone point at the pole, a smooth
// maximum at the equator).
TEST(SinglePathReach, ToleranceCoversTheExactRangeOfARotationPath) {
  PointResult r;
  ASSERT_TRUE(AnalyzePathPointValue(Scene(Prism(1.0f)), Request({ 3, 6, 4, 8 }, 20.0, 120.0), false, &r).Ok());
  EXPECT_EQ(r.Data().Discovered().reach.probe_lat_count, kReachProbeLatCount);
  EXPECT_GT(r.Data().Discovered().reach.tolerance_rad, 0.0);
  EXPECT_GE(r.Data().Discovered().reach.deviation_min_rad, 0.0);
  EXPECT_LE(r.Data().Discovered().reach.deviation_max_rad, 2.0 * kPi / 3.0 + 1e-12);
  EXPECT_LE(r.Data().Discovered().reach.deviation_min_rad - r.Data().Discovered().reach.tolerance_rad, 0.0);
  EXPECT_GE(r.Data().Discovered().reach.deviation_max_rad + r.Data().Discovered().reach.tolerance_rad, 2.0 * kPi / 3.0);
  EXPECT_NEAR(r.Data().Discovered().reach.target_deviation_rad, r.Data().meta.target_deviation_deg * kDeg, 1e-12);
}

// A target on the anti-solar vertical (azimuth 180) at altitude a sits at delta = 160 - a degrees
// from a sun at 20: a target a fraction of the tolerance beyond the probed maximum is still in range,
// one two tolerances beyond is not.
TEST(SinglePathReach, TheToleranceBandIsInclusive) {
  const ConfigManager scene = Scene(Prism(1.0f));
  PointResult probe;
  ASSERT_TRUE(AnalyzePathPointValue(scene, Request({ 3, 6, 4, 8 }, 20.0, 120.0), false, &probe).Ok());
  const double max_deg = probe.Data().Discovered().reach.deviation_max_rad / kDeg;
  const double tol_deg = probe.Data().Discovered().reach.tolerance_rad / kDeg;
  for (const auto& [beyond_deg, in_range] : { std::pair{ 0.5 * tol_deg, true }, std::pair{ 2.0 * tol_deg, false } }) {
    PointResult r;
    if (!AnalyzePathPointValue(scene, Request({ 3, 6, 4, 8 }, 160.0 - max_deg - beyond_deg, 180.0), false, &r).Ok()) {
      ADD_FAILURE() << "analysis failed, beyond by " << beyond_deg << " deg";
      continue;
    }
    EXPECT_EQ(r.Data().Discovered().reach.target_in_range, in_range) << "beyond by " << beyond_deg << " deg";
  }
}

// Reach is geometry: neither the request's sun grid nor the sample count nor warm seeds move it.
TEST(SinglePathReach, IsIndependentOfGridAndSampling) {
  const ConfigManager scene = Scene(Prism(0.5f));
  SinglePathRequest a = Request({ 3, 6, 4, 8 }, 20.0, 120.0);
  SinglePathRequest b = a;
  b.sun_grid_lat_count = 90;
  b.sample_count = 1000;
  PointResult ra;
  PointResult rb;
  ASSERT_TRUE(AnalyzePathPointValue(scene, a, false, &ra).Ok());
  ASSERT_TRUE(AnalyzePathPointValue(scene, b, false, &rb).Ok());
  EXPECT_EQ(ra.Data().Discovered().reach.deviation_min_rad, rb.Data().Discovered().reach.deviation_min_rad);
  EXPECT_EQ(ra.Data().Discovered().reach.deviation_max_rad, rb.Data().Discovered().reach.deviation_max_rad);
  EXPECT_EQ(ra.Data().Discovered().reach.tolerance_rad, rb.Data().Discovered().reach.tolerance_rad);
  EXPECT_EQ(ra.Data().Discovered().reach.target_in_range, rb.Data().Discovered().reach.target_in_range);
}

// Seeds fed back are Gauss-Newton starts of their own clusters: a second call that has them cannot
// come back with fewer components, even from a sparser sample.
TEST(SinglePathDiscovery, WarmSeedsKeepEveryComponent) {
  const ConfigManager scene = Scene(Prism(1.0f));
  PointResult first;
  ASSERT_TRUE(AnalyzePathPointValue(scene, Request({ 3, 5, 6, 7 }, 18.0, 175.0), false, &first).Ok());
  ASSERT_FALSE(first.Data().Discovered().components.empty());  // otherwise there is nothing to keep
  SinglePathRequest q = Request({ 3, 5, 6, 7 }, 18.0, 175.0);
  q.sample_count = 1000;
  for (const FiberComponent& c : first.Data().Discovered().components) {
    q.warm_seeds.insert(q.warm_seeds.end(), c.seed, c.seed + 9);
  }
  PointResult second;
  ASSERT_TRUE(AnalyzePathPointValue(scene, q, false, &second).Ok());
  EXPECT_EQ(second.Data().meta.warm_seed_count, static_cast<int>(first.Data().Discovered().components.size()));
  EXPECT_EQ(second.Data().Discovered().discovery.extra_seed_count,
            static_cast<int>(first.Data().Discovered().components.size()));
  EXPECT_GE(second.Data().Discovered().components.size(), first.Data().Discovered().components.size());
  ExpectFibersLandOnTheTarget(second);
}

// ---- axis distribution -> band-sum pose density ----
// The five GUI presets, mirrored by hand from src/gui/axis_presets.hpp (raypath may not include the
// GUI) in the config file's external spelling: zenith is the c axis' zenith angle, stored internally
// as latitude = 90 - zenith (math.cpp from_json(AxisDistribution)).
AxisDistribution Axis(Distribution zenith_external, Distribution azimuth, Distribution roll) {
  AxisDistribution a;
  a.latitude_dist = zenith_external;
  a.latitude_dist.center = 90.0f - zenith_external.center;
  a.azimuth_dist = azimuth;
  a.roll_dist = roll;
  return a;
}

constexpr Distribution kFullTurn{ DistributionType::kUniform, 0.0f, 360.0f };
constexpr Distribution kLockedRoll{ DistributionType::kGaussian, 0.0f, 1.0f };

Distribution Gauss(float mean, float std) {
  return { DistributionType::kGaussian, mean, std };
}

TEST(AxisToPoseDensity, TheFivePresetsMapToTheirFamilies) {
  struct Case {
    const char* name;
    AxisDistribution axis;
    analytic::PoseFamily family;
    double zenith_mean;
    double zenith_std;
    double roll_std;
  };
  const Case cases[] = {
    { "random", Axis(kFullTurn, kFullTurn, kFullTurn), analytic::PoseFamily::kRandom, 0.0, 0.0, 0.0 },
    { "column", Axis(Gauss(90.0f, 1.0f), kFullTurn, kFullTurn), analytic::PoseFamily::kColumn, 90.0, 1.0, 0.0 },
    { "plate", Axis(Gauss(0.0f, 1.0f), kFullTurn, kFullTurn), analytic::PoseFamily::kPlate, 0.0, 1.0, 0.0 },
    { "parry", Axis(Gauss(90.0f, 1.0f), kFullTurn, kLockedRoll), analytic::PoseFamily::kParry, 90.0, 1.0, 1.0 },
    { "lowitz", Axis(Gauss(0.0f, 40.0f), kFullTurn, kLockedRoll), analytic::PoseFamily::kLowitz, 0.0, 40.0, 1.0 },
  };
  for (const Case& c : cases) {
    SCOPED_TRACE(c.name);
    const PoseDensityConversion got = ConvertAxisToPoseDensity(c.axis);
    EXPECT_TRUE(got.Ok()) << got.unsupported;
    EXPECT_EQ(got.spec.family, c.family);
    EXPECT_DOUBLE_EQ(got.spec.zenith_mean_deg, c.zenith_mean);
    EXPECT_DOUBLE_EQ(got.spec.zenith_std_deg, c.zenith_std);
    EXPECT_DOUBLE_EQ(got.spec.roll_mean_deg, 0.0);
    EXPECT_DOUBLE_EQ(got.spec.roll_std_deg, c.roll_std);
    EXPECT_STREQ(analytic::PoseDensityError(got.spec), "");
  }
}

// Every class the contract lists as not expressible in v1 (section 2.2) is refused, naming what it
// got; none falls back to a nearby family.
TEST(AxisToPoseDensity, InexpressibleAxesAreRefusedByName) {
  struct Case {
    AxisDistribution axis;
    const char* expected_fragment;
  };
  const Case cases[] = {
    { Axis({ DistributionType::kZigzag, 0.0f, 40.0f }, kFullTurn, kLockedRoll), "zenith of type zigzag" },
    { Axis({ DistributionType::kLaplacian, 90.0f, 1.0f }, kFullTurn, kFullTurn), "zenith of type laplacian" },
    { Axis({ DistributionType::kUniform, 90.0f, 20.0f }, kFullTurn, kFullTurn), "zenith of type uniform" },
    { Axis({ DistributionType::kGaussianLegacy, 90.0f, 1.0f }, kFullTurn, kFullTurn), "zenith of type gauss_legacy" },
    { Axis(Fixed(90.0f), kFullTurn, kFullTurn), "zenith of type fixed" },
    { Axis(Gauss(90.0f, 1.0f), { DistributionType::kUniform, 0.0f, 180.0f }, kFullTurn),
      "azimuth of type uniform over 180" },
    { Axis(Gauss(90.0f, 1.0f), Gauss(0.0f, 10.0f), kFullTurn), "azimuth of type gauss" },
    // A Gaussian roll with a uniform (even full-range) zenith.
    { Axis(kFullTurn, kFullTurn, kLockedRoll), "zenith of type uniform" },
    { Axis(Gauss(90.0f, 1.0f), kFullTurn, Fixed(0.0f)), "roll of type fixed" },
    { Axis(Gauss(90.0f, 1.0f), kFullTurn, { DistributionType::kUniform, 0.0f, 90.0f }),
      "roll of type uniform over 90" },
    { Axis(Gauss(90.0f, 1.0f), kFullTurn, { DistributionType::kZigzag, 0.0f, 5.0f }), "roll of type zigzag" },
    { Axis(Gauss(90.0f, 0.0f), kFullTurn, kFullTurn), "zenith_std_deg" },
  };
  for (const Case& c : cases) {
    SCOPED_TRACE(c.expected_fragment);
    const PoseDensityConversion got = ConvertAxisToPoseDensity(c.axis);
    EXPECT_FALSE(got.Ok());
    EXPECT_NE(got.unsupported.find(c.expected_fragment), std::string::npos) << got.unsupported;
  }
}

// ---- the L2 factor decomposition ------------------------------------------------------------------

// The per-pose K factor is the kernel's own Jacobian read at the same pose (a fresh
// NormalJacobianAt evaluation, the definition the trace's arrays record), k_carea is
// 1/(J + epsilon) with the declared epsilon, the factor product is the four named factors
// multiplied, and the component integral is the left-endpoint arclength sum the serializer's
// rule sentence states.
TEST(SinglePathFactors, KCareaMatchesTheKernelJacobianAndTheIntegralFollowsItsRule) {
  PointResult r;
  ASSERT_TRUE(AnalyzePathPointValue(Scene(Prism(1.0f)), Request({ 3, 5 }, 20.0, 25.0), false, &r).Ok());
  ASSERT_EQ(r.Data().OutcomeValue(), Outcome::kDiscovered);
  ASSERT_FALSE(r.Data().Discovered().components.empty());

  // The kernel map, built independently through the same conversions the module spells.
  const CrystalConversion c = ConvertCrystal(Prism(1.0f));
  analytic::FaceNormalTable table;
  analytic::FacePolygonTable polygons;
  ASSERT_EQ(analytic::BuildFaceNormals(c.shape, &table, &polygons), analytic::Status::kOk);
  std::vector<int> slots;
  ASSERT_TRUE(ResolveSingleLayerPath({ { 3, 5 } }, table, &slots).Ok());
  SunParam sun{ 20.0f, 0.0f, 0.5f };
  double incident[3];
  SunIncidentDirection(sun, incident);
  // The analysis's own index (the echo is the authority — a nearby-but-unequal n flips
  // availability at the near-TIR poses the fiber reaches).
  const analytic::IcePathMap map(table, slots.data(), static_cast<int>(slots.size()), r.Data().meta.refractive_index,
                                 incident);

  int checked = 0;
  for (const FiberComponent& component : r.Data().Discovered().components) {
    for (const PointDetail& p : component.points) {
      const analytic::NormalJacobian j = analytic::fiber_detail::NormalJacobianAt(map, p.pose);
      if (p.jacobian_available != j.available) {
        ADD_FAILURE() << "jacobian availability disagrees with a fresh kernel read at the same pose";
        continue;
      }
      if (!j.available) {
        EXPECT_TRUE(std::isnan(p.k_carea));
        EXPECT_TRUE(std::isnan(p.factor_product));
        continue;
      }
      EXPECT_NEAR(p.jacobian, j.value, 1e-12 * std::fabs(j.value) + 1e-15);
      EXPECT_NEAR(p.k_carea, 1.0 / (j.value + kFactorsEpsilon), 1e-12 * std::fabs(p.k_carea) + 1e-15);
      EXPECT_NEAR(p.singular_values[0] * p.singular_values[1], j.value, 1e-9 * std::fabs(j.value) + 1e-15);
      const double product = p.rho * p.entry_measure_a2 * p.total_transmission * p.k_carea;
      EXPECT_NEAR(p.factor_product, product, 1e-12 * std::fabs(product) + 1e-18);
      checked++;
    }
    // The integral recompute over the component's own arrays: segments whose first point
    // carries a usable product, left endpoints.
    double integral = 0.0;
    int used = 0;
    for (size_t i = 0; i + 1 < component.points.size(); i++) {
      if (!std::isfinite(component.points[i].factor_product)) {
        continue;
      }
      integral += component.points[i].factor_product * component.arclength_increments[i];
      used++;
    }
    if (used > 0) {
      EXPECT_TRUE(component.factors.integral_available);
      EXPECT_NEAR(component.factors.fiber_integral, integral, 1e-12 * std::fabs(integral) + 1e-18);
    }
    EXPECT_EQ(component.factors.total_points, static_cast<int>(component.points.size()));
    EXPECT_EQ(component.factors.available_points, checked);
  }
  EXPECT_GT(checked, 0);
}

// rho is the declared joint pose density at the pose's composed angles: on a fixed axis the
// angle-Lebesgue density is 0 a.e. (a point mass), and on a full-random axis it reduces to the
// latitude law over 1/(2 pi)^2 and is constant in azimuth at a fixed zenith.
TEST(SinglePathFactors, RhoIsTheDeclaredDensityDeclared) {
  // Fixed axis: the joint density is 0 everywhere off the Dirac slice, which is every traced pose.
  PointResult fixed;
  ASSERT_TRUE(AnalyzePathPointValue(Scene(Prism(1.0f)), Request({ 3, 5 }, 20.0, 25.0), false, &fixed).Ok());
  ASSERT_FALSE(fixed.Data().Discovered().components.empty());
  for (const FiberComponent& component : fixed.Data().Discovered().components) {
    for (const PointDetail& p : component.points) {
      EXPECT_EQ(p.rho, 0.0);
    }
  }
  EXPECT_FALSE(fixed.Data().Discovered().measure_registration.registered_gap);
  EXPECT_EQ(fixed.Data().Discovered().measure_registration.support_kind, "point");

  // Full-random axis: rho(phi) = rho_lat(phi) / (2 pi)^2, an oracle rebuilt here from the axis
  // the config carries (the module's own conversion is not reused).
  ConfigManager scene = Scene(Prism(1.0f));
  CrystalConfig& crystal = scene.crystals_.at(1);
  const Distribution kFullTurn{ DistributionType::kUniform, 0.0f, 360.0f };
  crystal.axis_.azimuth_dist = kFullTurn;
  crystal.axis_.latitude_dist = { DistributionType::kUniform, 90.0f, 180.0f };
  crystal.axis_.roll_dist = kFullTurn;
  PointResult r;
  ASSERT_TRUE(AnalyzePathPointValue(scene, Request({ 3, 5 }, 20.0, 25.0), false, &r).Ok());
  ASSERT_FALSE(r.Data().Discovered().components.empty());
  const LatitudeDensity lat = MakeLatitudeDensity(crystal.axis_.latitude_dist);
  double rho_min = std::numeric_limits<double>::infinity();
  double rho_max = 0.0;
  for (const FiberComponent& component : r.Data().Discovered().components) {
    for (const PointDetail& p : component.points) {
      const double phi = (90.0 - p.angles.zenith_deg) * kDeg;
      const double expected = lat.Evaluate(phi) / (2.0 * kPi * 2.0 * kPi);
      EXPECT_NEAR(p.rho, expected, 1e-12 * expected + 1e-18);
      rho_min = std::min(rho_min, p.rho);
      rho_max = std::max(rho_max, p.rho);
    }
  }
  EXPECT_GE(rho_max, 0.0);
  EXPECT_LT(rho_min, std::numeric_limits<double>::infinity());
  EXPECT_EQ(r.Data().Discovered().measure_registration.support_kind, "area");
  EXPECT_TRUE(r.Data().Discovered().measure_registration.fast_path);
  // The all-spread area axis is not a gap: both dirac predicates fall through, so the registration
  // stays silent (the green arm of the fixed-azimuth red test below).
  EXPECT_FALSE(r.Data().Discovered().measure_registration.registered_gap);
}

TEST(SinglePathFactors, LogDensityKeepsDeepTailsFiniteWhereLinearRhoUnderflows) {
  // A roll Gaussian centred 60 degrees away from where the {3,5} branches actually sit: every
  // branch point's linear rho underflows to a read-as-zero 0.0 while its log stays a finite
  // deep negative — the pinned member's honest dark, spelled in log space. The interpretive
  // pose names the recovered set's least-dark point (available — finiteness, not a threshold),
  // and a self reference answers an exactly zero ratio everywhere it answers at all.
  ConfigManager scene = Scene(Prism(1.0f));
  CrystalConfig& crystal = scene.crystals_.at(1);
  crystal.axis_.azimuth_dist = { DistributionType::kUniform, 0.0f, 360.0f };
  crystal.axis_.latitude_dist = { DistributionType::kGaussian, 0.0f, 1.0f };  // zenith 90 ± 1
  crystal.axis_.roll_dist = { DistributionType::kGaussian, 60.0f, 1.0f };
  auto request = Request({ 3, 5 }, 20.0, 25.0);
  request.ratio_reference_faces = { 3, 5 };  // self: identity carrier
  PointResult r;
  ASSERT_TRUE(AnalyzePathPointValue(scene, request, false, &r).Ok());
  const DiscoveredPoint& discovered = r.Data().Discovered();
  ASSERT_FALSE(discovered.components.empty());
  // Every slot spreads with everywhere-positive support (Gaussians, full-turn uniform), so a
  // linear rho of 0.0 here is UNDERFLOW, never an off-support zero — and the log must then be
  // finite (≤ log of the smallest subnormal ≈ −744), while any positive rho agrees with its log.
  int underflowed = 0;
  for (const FiberComponent& component : discovered.components) {
    for (const PointDetail& p : component.points) {
      if (!p.valid) {
        continue;
      }
      EXPECT_TRUE(std::isfinite(p.log_rho)) << "everywhere-positive support: no off-support −inf here";
      if (p.rho > 0.0) {
        EXPECT_NEAR(p.log_rho, std::log(p.rho), 1e-9 * std::fabs(p.log_rho) + 1e-9);
      } else {
        EXPECT_EQ(p.rho, 0.0);
        EXPECT_LE(p.log_rho, -700.0) << "a read-as-zero rho is the underflowed tail, not darkness";
        ++underflowed;
      }
      // Self reference: the transported pose is the pose, the reference density the member's
      // own, the ratio exactly zero (finite minus itself).
      EXPECT_EQ(p.ref_log_density, p.log_rho);
      EXPECT_EQ(p.log_ratio, 0.0);
    }
  }
  EXPECT_GT(underflowed, 0) << "the fixture's branches sit ~17..43 / ~-155 roll-degrees, far off the +60-degree anchor";
  const InterpretivePose& interpretive = discovered.interpretive;
  ASSERT_TRUE(interpretive.available);
  ASSERT_GE(interpretive.component_index, 0);
  ASSERT_GE(interpretive.point_index, 0);
  EXPECT_LT(interpretive.component_index, static_cast<int>(discovered.components.size()));
  EXPECT_LT(interpretive.point_index,
            static_cast<int>(discovered.components[interpretive.component_index].points.size()));
  EXPECT_EQ(interpretive.log_density,
            discovered.components[interpretive.component_index].points[interpretive.point_index].log_rho);
  EXPECT_TRUE(std::isfinite(interpretive.log_density));
}

TEST(SinglePathFactors, LogDensityAgreesWithTheLinearFormOnTheOverlap) {
  // A roll Gaussian centred on the {3,5} fixture's own branch (its rolls measure ~17°..43° at
  // this target — the 30° centre keeps that component inside the linear form's range while the
  // latitude slot stays flat-uniform, so positive linear rhos exist) — there the log form must
  // be the logarithm of the linear one (the two implementations agree on the overlap family;
  // the deep-tail arm is the test above).
  ConfigManager scene = Scene(Prism(1.0f));
  CrystalConfig& crystal = scene.crystals_.at(1);
  crystal.axis_.azimuth_dist = { DistributionType::kUniform, 0.0f, 360.0f };
  crystal.axis_.latitude_dist = { DistributionType::kUniform, 90.0f, 180.0f };
  crystal.axis_.roll_dist = { DistributionType::kGaussian, 30.0f, 1.0f };
  PointResult r;
  ASSERT_TRUE(AnalyzePathPointValue(scene, Request({ 3, 5 }, 20.0, 25.0), false, &r).Ok());
  int checked = 0;
  for (const FiberComponent& component : r.Data().Discovered().components) {
    for (const PointDetail& p : component.points) {
      if (!p.valid || p.rho <= 0.0) {
        continue;
      }
      EXPECT_NEAR(p.log_rho, std::log(p.rho), 1e-9 * std::fabs(p.log_rho) + 1e-9);
      ++checked;
    }
  }
  EXPECT_GT(checked, 0);
}

TEST(SinglePathFactors, RatioReferenceCarriesTheTransportedPoseAndItsTail) {
  // The Parry measure, {3,5} referenced against {5,7}: the first proper element mapping one onto
  // the other is P_2, whose compensation shifts roll by −120° and touches no other slot. This
  // fixture's branches sit tens of roll-degrees from the ±1° anchor (measured: ~17°..43° and
  // ~−155°), so both the member's own reads and the transported reads are deep tails — and the
  // two log forms keep every one of them finite, their per-pose difference huge and signed by
  // where each pose actually sits. No fake zero, no 0/0; the linear rho underflowed them all.
  ConfigManager scene = Scene(Prism(1.0f));
  CrystalConfig& crystal = scene.crystals_.at(1);
  crystal.axis_.azimuth_dist = { DistributionType::kUniform, 0.0f, 360.0f };
  crystal.axis_.latitude_dist = { DistributionType::kGaussian, 0.0f, 1.0f };
  crystal.axis_.roll_dist = { DistributionType::kGaussian, 0.0f, 1.0f };
  auto request = Request({ 3, 5 }, 20.0, 25.0);
  request.ratio_reference_faces = { 5, 7 };
  PointResult r;
  ASSERT_TRUE(AnalyzePathPointValue(scene, request, false, &r).Ok());
  const DiscoveredPoint& discovered = r.Data().Discovered();
  ASSERT_FALSE(discovered.components.empty());
  int compared = 0;
  int huge = 0;
  for (const FiberComponent& component : discovered.components) {
    for (const PointDetail& p : component.points) {
      if (!p.valid) {
        continue;  // an invalid member pose carries no world ray to correspond
      }
      EXPECT_TRUE(std::isfinite(p.log_rho)) << "the member's own tail stays readable";
      // A non-finite reference read would make the derived ratio meaningless: skip the row's
      // remaining derived checks (non-fatally — one bad row must not hide the others').
      if (!std::isfinite(p.ref_log_density) || !std::isfinite(p.log_ratio)) {
        ADD_FAILURE() << "the transported pose verified, its density must answer; ratio must be finite";
        continue;
      }
      EXPECT_DOUBLE_EQ(p.log_ratio, p.log_rho - p.ref_log_density);
      EXPECT_EQ(p.rho, 0.0) << "these tens-of-sigma tails underflow the linear form on this fixture";
      if (std::fabs(p.log_ratio) > 50.0) {
        ++huge;
      }
      ++compared;
    }
  }
  EXPECT_GT(compared, 0);
  EXPECT_GT(huge, 0) << "the −120° roll shift moves tens of sigmas: the ratio must say so";
  // H_0's transport preserves this measure (azimuth uniform, latitude and roll symmetric about
  // their centres): a reference the H_0 carrier reaches must read ratio zero — the
  // measure-retention theorem, asserted operationally on the ratio face. {3,7} is H_0's image
  // of {3,5}. The tolerance is the two evaluations' floating noise only (symmetric arguments
  // need not sum in the same order); the theorem itself is exact.
  request.ratio_reference_faces = { 3, 7 };
  PointResult h0;
  ASSERT_TRUE(AnalyzePathPointValue(scene, request, false, &h0).Ok());
  int h0_compared = 0;
  for (const FiberComponent& component : h0.Data().Discovered().components) {
    for (const PointDetail& p : component.points) {
      if (!p.valid || !std::isfinite(p.log_rho)) {
        continue;
      }
      EXPECT_NEAR(p.ref_log_density, p.log_rho, 1e-9 * std::fabs(p.log_rho) + 1e-9);
      EXPECT_NEAR(p.log_ratio, 0.0, 1e-9);
      ++h0_compared;
    }
  }
  EXPECT_GT(h0_compared, 0);
}

TEST(SinglePathFactors, RatioReferenceRefusalsNameTheirCause) {
  ConfigManager scene = Scene(Prism(1.0f));
  auto request = Request({ 3, 5 }, 20.0, 25.0);
  request.ratio_reference_faces = { 4, 1 };  // pri,base against pri,pri: no element maps the bands
  PointResult refused;
  const Error unmappable = AnalyzePathPointValue(scene, request, false, &refused);
  EXPECT_EQ(unmappable.code, ErrorCode::kInvalidArgument);
  EXPECT_NE(unmappable.message.find("no element fixing the target sky point"), std::string::npos);

  request.ratio_reference_faces = { 3 };  // a one-face sequence is not a path
  PointResult illegal;
  const Error illegal_path = AnalyzePathPointValue(scene, request, false, &illegal);
  EXPECT_EQ(illegal_path.code, ErrorCode::kInvalidArgument);
}

TEST(SinglePathFactors, RatioReferenceVerifiesTheTransportedCorridorBeforePublishing) {
  // The asymmetric crystal (d_[1]=0.3): a proper carrier still maps {3,5,7} onto {5,7,3}, but
  // the shape symmetry that made the transported corridor congruent to the member's own is
  // gone — a transported pose the reference's finite geometry refuses must keep its NaN
  // (unverified), never publish the density read as a verified ratio.
  PrismCrystalParam p;
  p.h_ = Fixed(1.2f);
  for (auto& d : p.d_) {
    d = Fixed(1.0f);
  }
  p.d_[1] = Fixed(0.3f);
  ConfigManager scene = Scene(p);
  CrystalConfig& crystal = scene.crystals_.at(1);
  crystal.axis_.azimuth_dist = { DistributionType::kUniform, 0.0f, 360.0f };
  crystal.axis_.latitude_dist = { DistributionType::kGaussian, 0.0f, 1.0f };
  crystal.axis_.roll_dist = { DistributionType::kGaussian, 0.0f, 1.0f };
  auto request = Request({ 3, 5, 7 }, 25.0, 120.0);
  request.sample_count = 10000;
  request.ratio_reference_faces = { 5, 7, 3 };
  PointResult r;
  ASSERT_TRUE(AnalyzePathPointValue(scene, request, false, &r).Ok());
  const SinglePathMetadata& meta = r.Data().meta;
  const DiscoveredPoint& discovered = r.Data().Discovered();
  ASSERT_FALSE(discovered.components.empty());
  // The production carrier and corridor, re-derived independently: the carrier by the same
  // deterministic sky-fixing rule, the corridor on the reference's face sequence — the two
  // halves of the leg's published-read contract.
  PreparedGeometry geometry;
  ASSERT_TRUE(PrepareGeometry(scene.crystals_.at(1), &geometry).Ok());
  const std::vector<IdType> member_faces(request.path_layers[0].begin(), request.path_layers[0].end());
  const std::vector<IdType> reference_ids(request.ratio_reference_faces.begin(), request.ratio_reference_faces.end());
  CrystalElement carrier;
  ASSERT_TRUE(FindLabelMappingElement(member_faces, reference_ids, meta.target_direction, meta.sun_azimuth_deg * kDeg,
                                      &carrier));
  // The corridor consumes table slot indices, not Lumice face numbers — the same resolution the
  // entry's own reference leg ran.
  std::vector<int> ref_slots;
  ASSERT_TRUE(ResolveSingleLayerPath({ request.ratio_reference_faces }, geometry.table, &ref_slots).Ok());
  analytic::Corridor reference_corridor(geometry.table, geometry.polygons, ref_slots.data(),
                                        static_cast<int>(ref_slots.size()));
  int valid = 0;
  int published = 0;
  int corridor_refused = 0;
  for (const FiberComponent& component : discovered.components) {
    for (const PointDetail& point : component.points) {
      if (!point.valid) {
        continue;  // an invalid member pose carries no world ray to correspond
      }
      ++valid;
      double transported[9];
      CompensatedPose(carrier, point.pose, meta.sun_azimuth_deg * kDeg, transported);
      // u = R^T s_hat with s_hat = -incident (SunInCrystal's convention, restated): the
      // body-frame incident the corridor reads is -u.
      double transported_incident[3];
      for (int i = 0; i < 3; ++i) {
        transported_incident[i] = transported[0 * 3 + i] * meta.incident_direction[0] +
                                  transported[1 * 3 + i] * meta.incident_direction[1] +
                                  transported[2 * 3 + i] * meta.incident_direction[2];
      }
      const bool corridor_ok = reference_corridor.Evaluate(transported_incident, meta.refractive_index).status ==
                               analytic::EntryMeasureStatus::kOk;
      if (!std::isnan(point.ref_log_density)) {
        EXPECT_TRUE(corridor_ok) << "a published reference read requires a traversable corridor";
        ++published;
      }
      if (!corridor_ok) {
        EXPECT_TRUE(std::isnan(point.ref_log_density));
        EXPECT_TRUE(std::isnan(point.log_ratio));
        ++corridor_refused;
      }
    }
  }
  EXPECT_GT(valid, 0) << "the scene recovers points, so the contract has rows to rule on";
  EXPECT_GT(corridor_refused, 0) << "this asymmetric fixture must contain the corridor gap the verification exists for";
  // The gap refused, the traversable remainder still answers (the same scene's congruent poses).
  EXPECT_GT(published, 0) << "the corridor check refuses poses, not the correspondence itself";
}

ConfigManager ZigzagRollScene() {
  // Zigzag roll endpoints declare +inf (the density law's own reading at its support edge); the
  // near-fixed GaussianLegacy latitude keeps every other read finite or −inf (off-support).
  PrismCrystalParam prism;
  prism.h_ = Fixed(1.2f);
  for (auto& d : prism.d_) {
    d = Fixed(1.0f);
  }
  ConfigManager scene = Scene(prism);
  CrystalConfig& crystal = scene.crystals_.at(1);
  crystal.axis_.azimuth_dist = { DistributionType::kUniform, 0.0f, 360.0f };
  crystal.axis_.latitude_dist = { DistributionType::kGaussianLegacy, 12.345f, 0.0001f };
  crystal.axis_.roll_dist = { DistributionType::kZigzag, 0.0f, 90.0f };
  return scene;
}

TEST(SinglePathFactors, MixedInfinityRatioKeepsItsSignedAnswer) {
  // Zigzag endpoints declare +inf; transported poses of the sibling land off-support (−inf) or
  // finite. Every state pair the subtraction can meet reads its declared answer: opposite
  // infinities keep their signed ratio, a finite member against a diverging reference reads the
  // negative infinity, like infinities stay incomparable (NaN, never 0) — the old
  // both-non-finite guard turned exactly the signed answers into NaN. Both seats of the same
  // correspondence are witnessed (the swapped numerator/denominator direction).
  auto request = Request({ 3 }, 20.0, 180.0);
  request.sample_count = 10000;
  request.ratio_reference_faces = { 6 };
  PointResult forward;
  ASSERT_TRUE(AnalyzePathPointValue(ZigzagRollScene(), request, false, &forward).Ok());
  int forward_signed = 0;
  for (const FiberComponent& component : forward.Data().Discovered().components) {
    for (const PointDetail& p : component.points) {
      if (!p.valid || p.log_rho != std::numeric_limits<double>::infinity() ||
          p.ref_log_density != -std::numeric_limits<double>::infinity()) {
        continue;  // the witnessed state pair: an on-endpoint member point, an off-support reference
      }
      EXPECT_EQ(p.log_ratio, std::numeric_limits<double>::infinity());
      ++forward_signed;
    }
  }
  EXPECT_GT(forward_signed, 0) << "+inf member against a −inf reference: the ratio is +inf, not NaN";
  // The swapped correspondence (the reference's member in the seat): the signed answers now
  // read the other way — a finite member against a diverging reference is −inf, and the like
  // infinities (member on an endpoint, reference too; member off-support, reference too) stay
  // incomparable NaN.
  auto swapped = Request({ 6 }, 20.0, 180.0);
  swapped.sample_count = 10000;
  swapped.ratio_reference_faces = { 3 };
  PointResult reverse;
  ASSERT_TRUE(AnalyzePathPointValue(ZigzagRollScene(), swapped, false, &reverse).Ok());
  int negative_signed = 0;
  int like_inf_incomparable = 0;
  for (const FiberComponent& component : reverse.Data().Discovered().components) {
    for (const PointDetail& p : component.points) {
      if (!p.valid) {
        continue;
      }
      if (std::isfinite(p.log_rho) && p.ref_log_density == std::numeric_limits<double>::infinity()) {
        EXPECT_EQ(p.log_ratio, -std::numeric_limits<double>::infinity());
        ++negative_signed;
      }
      const bool member_inf = !std::isfinite(p.log_rho);
      const bool ref_inf = !std::isfinite(p.ref_log_density) && !std::isnan(p.ref_log_density);
      if (member_inf && ref_inf && std::signbit(p.log_rho) == std::signbit(p.ref_log_density)) {
        EXPECT_TRUE(std::isnan(p.log_ratio)) << "like infinities subtract to incomparable, whatever the sign";
        ++like_inf_incomparable;
      }
    }
  }
  EXPECT_GT(negative_signed, 0) << "finite member against a +inf reference: the ratio is −inf";
  EXPECT_GT(like_inf_incomparable, 0) << "like infinities must read incomparable, not a signed answer";
}

// A fixed axis (every slot a Dirac): the linear rho answers 0 off the Dirac slice and the log
// answers −inf — the same statement in log vocabulary. With no ordinary density reading
// anywhere on the fiber, the interpretive pose is honestly absent: this search has not found
// one, not that none exists.
TEST(SinglePathFactors, ADiracSliceReadsMinusInfinityAndNamesNoInterpretivePose) {
  PointResult r;
  ASSERT_TRUE(AnalyzePathPointValue(Scene(Prism(1.0f)), Request({ 3, 5 }, 20.0, 25.0), false, &r).Ok());
  ASSERT_FALSE(r.Data().Discovered().components.empty());
  for (const FiberComponent& component : r.Data().Discovered().components) {
    for (const PointDetail& p : component.points) {
      EXPECT_EQ(p.log_rho, -std::numeric_limits<double>::infinity());
    }
  }
  EXPECT_FALSE(r.Data().Discovered().interpretive.available);
  EXPECT_EQ(r.Data().Discovered().interpretive.component_index, -1);
  EXPECT_EQ(r.Data().Discovered().interpretive.point_index, -1);
  EXPECT_TRUE(std::isnan(r.Data().Discovered().interpretive.log_density));
}

TEST(SinglePathFactors, ParryMinus52LightsTheCarryingMemberAndReadsTheDarkOneHonestly) {
  // The acceptance scene itself (prism height 1.2, regular; zenith gauss 90±1, azimuth full
  // turn, roll gauss 0±1; sun 20°/0°; target 36°/−52°; one-million seed events — the archived
  // discussion evidence's exact shape). The carrying member 4-2 recovers its lit arc and its
  // interpretive pose carries a positive rho on it; the dark member 4-1 keeps its own geometry
  // (the H_1 image arc, real finite crystals) while every density read stays an honest finite
  // deep tail — referenced against 4-2, the transported poses verify and the ratios answer,
  // none of it a fake zero. AC1's and AC4's engine halves on one scene.
  ConfigManager scene = Scene(Prism(1.2f));
  CrystalConfig& crystal = scene.crystals_.at(1);
  crystal.axis_.azimuth_dist = { DistributionType::kUniform, 0.0f, 360.0f };
  crystal.axis_.latitude_dist = { DistributionType::kGaussian, 0.0f, 1.0f };
  crystal.axis_.roll_dist = { DistributionType::kGaussian, 0.0f, 1.0f };
  PointResult lit;
  ASSERT_TRUE(AnalyzePathPointValue(scene, Request({ 4, 2 }, 36.0, -52.0), false, &lit).Ok());
  const InterpretivePose& lit_pose = lit.Data().Discovered().interpretive;
  ASSERT_TRUE(lit_pose.available);
  const PointDetail& peak = lit.Data().Discovered().components[lit_pose.component_index].points[lit_pose.point_index];
  EXPECT_TRUE(std::isfinite(peak.log_rho));
  EXPECT_GT(peak.rho, 1.0) << "the interpretive pose must sit on the arc's positive-rho body";
  EXPECT_NEAR(peak.log_rho, std::log(peak.rho), 1e-9 * std::fabs(peak.log_rho) + 1e-9);

  auto dark_request = Request({ 4, 1 }, 36.0, -52.0);
  dark_request.sample_count = 1000000;
  dark_request.ratio_reference_faces = { 4, 2 };
  PointResult dark;
  ASSERT_TRUE(AnalyzePathPointValue(scene, dark_request, false, &dark).Ok());
  ASSERT_FALSE(dark.Data().Discovered().components.empty()) << "the dark member keeps its own geometry";
  int dark_valid = 0;
  for (const FiberComponent& component : dark.Data().Discovered().components) {
    for (const PointDetail& p : component.points) {
      if (!p.valid) {
        continue;
      }
      EXPECT_EQ(p.rho, 0.0) << "the ±1° roll anchor never reaches these arcs: the linear form underflows";
      EXPECT_TRUE(std::isfinite(p.log_rho));
      EXPECT_LT(p.log_rho, -100.0);
      EXPECT_TRUE(std::isfinite(p.ref_log_density)) << "the transported pose verified against the lit member";
      EXPECT_TRUE(std::isfinite(p.log_ratio));
      ++dark_valid;
    }
  }
  EXPECT_GT(dark_valid, 0);
}

TEST(SinglePathFactors, InterpretivePoseRequiresAReachableFiniteEntry) {
  // A pose the normals read dense can still admit no ray: entry_measure is the finite
  // crystal's own fact (the cross-section that enters and meets every later face). The
  // selector's candidates need entry_measure > 0 — the winner must explain the clicked light
  // with a ray that can actually travel, never the densest unreachable pose.
  PrismCrystalParam prism;
  prism.h_ = Fixed(1.2f);
  for (int i = 0; i < 6; i++) {
    prism.d_[i] = Fixed(1.0f);
  }
  ConfigManager scene = Scene(prism);
  CrystalConfig& crystal = scene.crystals_.at(1);
  crystal.axis_.azimuth_dist = { DistributionType::kUniform, 0.0f, 360.0f };
  crystal.axis_.latitude_dist = { DistributionType::kUniform, 90.0f, 360.0f };  // full sphere
  crystal.axis_.roll_dist = { DistributionType::kUniform, 0.0f, 360.0f };
  SinglePathRequest q = Request({ 3, 5, 7 }, 25.0, 120.0);
  q.sample_count = 5000;
  PointResult r;
  ASSERT_TRUE(AnalyzePathPointValue(scene, q, false, &r).Ok());
  ASSERT_EQ(r.Data().OutcomeValue(), Outcome::kDiscovered);
  const InterpretivePose& pose = r.Data().Discovered().interpretive;
  if (pose.available) {
    // Whenever the search finds an interpretation at all, it is one a ray can walk.
    const PointDetail& winner = r.Data().Discovered().components[pose.component_index].points[pose.point_index];
    EXPECT_GT(winner.entry_measure, 0.0) << "the interpretive pose must sit on reachable finite geometry";
  }
  int reachable = 0;
  for (const FiberComponent& component : r.Data().Discovered().components) {
    for (const PointDetail& p : component.points) {
      if (p.valid && p.entry_measure > 0.0 && std::isfinite(p.log_rho)) {
        ++reachable;
      }
    }
  }
  EXPECT_GT(reachable, 0) << "the scenario holds reachable candidates for the selector";
}

// The rank-0 orbit integral reproduces the measure layer's own kFiberParameter quadrature on the
// LI #51 fixture construction (the rhombic plate, sun at 9/180, the wavelength-pool ends
// 1.307/1.317 — the literals test_fiber_quadrature.cpp freezes), summed here independently over
// the same midpoint grid: the hand sum reads A off the corridor and T off the chain at the
// identity pose with the sun at u (both depend on the pose only through u — the reduction the
// kernel's own body-frame evaluator states). The per-pose A/T kernels are already pinned against
// the LI snapshots by that file; this pins the point branch's wiring to the same machinery.
TEST(SinglePathFactors, OrbitIntegralReproducesTheQuadratureOnTheLI51Construction) {
  PrismCrystalParam plate;
  plate.h_ = Fixed(1.0f);
  const double face_distance[6] = { 1.5, 1.0, 1.0, 1.5, 1.0, 1.0 };
  for (int i = 0; i < 6; i++) {
    plate.d_[i] = Fixed(static_cast<float>(face_distance[i]));
  }
  ConfigManager scene = Scene(plate, 9.0f, { { 650.0f, 1.0f } });
  CrystalConfig& crystal = scene.crystals_.at(1);
  crystal.axis_.azimuth_dist = { DistributionType::kUniform, 0.0f, 360.0f };
  crystal.axis_.roll_dist = Fixed(0.0f);

  SinglePathRequest q;
  q.crystal_id = 1;
  q.path_layers = { { 1, 2 } };
  q.target_altitude_deg = 30.0;
  q.target_azimuth_deg = 90.0;
  q.sun_grid_lat_count = 0;
  PointResult r;
  ASSERT_TRUE(AnalyzePathPointValue(scene, q, false, &r).Ok());
  ASSERT_EQ(r.Data().OutcomeValue(), Outcome::kPointMass);
  const OrbitFactors& orbit = r.Data().Mass().orbit_factors;
  ASSERT_TRUE(orbit.available);
  ASSERT_EQ(orbit.measure_support, "spin_orbit");
  ASSERT_GT(orbit.kept_count, 0);

  // The independent hand sum over the same midpoint grid, at the analysis's own index (the echo
  // is the authority for n, the same rule the benches state).
  const CrystalConversion c = ConvertCrystal(plate);
  analytic::FaceNormalTable table;
  analytic::FacePolygonTable polygons;
  ASSERT_EQ(analytic::BuildFaceNormals(c.shape, &table, &polygons), analytic::Status::kOk);
  std::vector<int> slots;
  ASSERT_TRUE(ResolveSingleLayerPath({ { 1, 2 } }, table, &slots).Ok());
  const double n = r.Data().meta.refractive_index;
  double sun_hat[3];
  // The sun points AT the sun: the negation of the propagation direction.
  sun_hat[0] = -r.Data().meta.incident_direction[0];
  sun_hat[1] = -r.Data().meta.incident_direction[1];
  sun_hat[2] = -r.Data().meta.incident_direction[2];
  const UMarginal orbit_measure = MakeUMarginal(crystal.axis_, sun_hat);
  analytic::Corridor corridor(table, polygons, slots.data(), static_cast<int>(slots.size()));
  std::vector<double> segments(3 * (slots.size() + 1));
  std::vector<double> transmittances(slots.size());
  // Body-frame evaluation at the identity pose: the chain's A and T depend on the pose only
  // through u (PathContext::EvaluateAtSun's own reduction).
  const double kIdentity[9] = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };
  double hand = 0.0;
  int kept = 0;
  const double dtheta = 2.0 * kPi / kOrbitFactorSampleCount;
  for (int i = 0; i < kOrbitFactorSampleCount; i++) {
    const double theta = 2.0 * kPi * (i + 0.5) / kOrbitFactorSampleCount;
    double u[3];
    orbit_measure.SpinOrbitPoint(theta, u);
    const double incident_body[3] = { -u[0], -u[1], -u[2] };
    const double area = corridor.Evaluate(incident_body, n).value;
    analytic::PathOutputs out;
    out.segment_directions = segments.data();
    out.interface_transmittances = transmittances.data();
    const bool valid =
        analytic::EvaluatePath(table, slots.data(), static_cast<int>(slots.size()), n, incident_body, kIdentity, &out);
    if (!(area * out.fresnel_transmission > 0.0) || !valid) {
      continue;
    }
    hand += orbit_measure.SpinOrbitThetaDensity(theta) * area * out.fresnel_transmission * dtheta;
    kept++;
  }
  EXPECT_EQ(kept, orbit.kept_count);
  EXPECT_NEAR(orbit.integral, hand, 1e-12 * std::fabs(hand) + 1e-18);
}

// The registered-gap red arm: an area axis with a fixed azimuth is the declared-density layer's
// v1 read-as-zero gap (rho_u = 0 everywhere at the u-marginal), and the registration block must
// name it — the whole "a rho of 0 is unanswered, not dark" honesty contract rides on this
// declaration appearing exactly when the gap is in force (the point-support documents above
// carry the green arm: registered_gap false).
TEST(SinglePathFactors, AFixedAzimuthAreaAxisDeclaresItsRegisteredGap) {
  ConfigManager scene = Scene(Prism(1.0f));
  CrystalConfig& crystal = scene.crystals_.at(1);
  const Distribution kFullTurn{ DistributionType::kUniform, 0.0f, 360.0f };
  crystal.axis_.latitude_dist = { DistributionType::kUniform, 90.0f, 180.0f };
  crystal.axis_.azimuth_dist = Fixed(0.0f);  // the fixed azimuth is the gap's own trigger
  crystal.axis_.roll_dist = kFullTurn;
  PointResult r;
  ASSERT_TRUE(AnalyzePathPointValue(scene, Request({ 3, 5 }, 20.0, 25.0), false, &r).Ok());
  ASSERT_EQ(r.Data().OutcomeValue(), Outcome::kDiscovered);
  EXPECT_EQ(r.Data().Discovered().measure_registration.support_kind, "area");
  EXPECT_TRUE(r.Data().Discovered().measure_registration.registered_gap);
  ASSERT_FALSE(r.Data().Discovered().measure_registration.note.empty());
  EXPECT_NE(r.Data().Discovered().measure_registration.note.find("fixed azimuth"), std::string::npos)
      << r.Data().Discovered().measure_registration.note;
  EXPECT_NE(r.Data().Discovered().measure_registration.note.find("read-as-zero"), std::string::npos)
      << r.Data().Discovered().measure_registration.note;
}

// The registered-gap red arm's other dirac slot: a fixed zenith (latitude) with a spread,
// non-uniform azimuth and spread roll is the v1 gap list's second area gap — RegisterMeasure's
// transcription must name both slots, so each carries its own red arm.
TEST(SinglePathFactors, AFixedZenithAreaAxisDeclaresItsRegisteredGap) {
  ConfigManager scene = Scene(Prism(1.0f));
  CrystalConfig& crystal = scene.crystals_.at(1);
  const Distribution kFullTurn{ DistributionType::kUniform, 0.0f, 360.0f };
  crystal.axis_.latitude_dist = Fixed(
      45.0f);  // a fixed zenith away from the poles (Fixed(90) is the pole: the degenerate branch claims it first)
  crystal.axis_.azimuth_dist = { DistributionType::kUniform, 30.0f, 60.0f };  // spread, non-uniform
  crystal.axis_.roll_dist = kFullTurn;
  PointResult r;
  ASSERT_TRUE(AnalyzePathPointValue(scene, Request({ 3, 5 }, 20.0, 25.0), false, &r).Ok());
  ASSERT_EQ(r.Data().OutcomeValue(), Outcome::kDiscovered);
  EXPECT_EQ(r.Data().Discovered().measure_registration.support_kind, "area");
  EXPECT_TRUE(r.Data().Discovered().measure_registration.registered_gap);
  ASSERT_FALSE(r.Data().Discovered().measure_registration.note.empty());
  EXPECT_NE(r.Data().Discovered().measure_registration.note.find("fixed zenith"), std::string::npos)
      << r.Data().Discovered().measure_registration.note;
  EXPECT_NE(r.Data().Discovered().measure_registration.note.find("read-as-zero"), std::string::npos)
      << r.Data().Discovered().measure_registration.note;
}

// The fingerprint's hash is the contract's promise ("hex SHA256" in
// doc/raypath/raypath-cli-output.md §5); these known answers pin the hand-rolled implementation
// to the algorithm, which the differential gate tests alone cannot.
TEST(SinglePathFingerprint, Sha256MatchesTheKnownAnswers) {
  using lumice::raypath::survey::Sha256Hex;
  EXPECT_EQ(Sha256Hex(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  EXPECT_EQ(Sha256Hex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  // The NIST vectors cover both compression paths the production fingerprints take: the 56-byte
  // input straddling the padding boundary, and the 112-byte input exercising the cross-block
  // state carry — the projection dumps are always multi-block.
  EXPECT_EQ(Sha256Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  EXPECT_EQ(
      Sha256Hex("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnop"
                "qrstnopqrstu"),
      "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1");
}

ConfigManager NarrowColumnScene() {
  ConfigManager scene = Scene(Prism(1.0f));
  auto& axis = scene.crystals_.at(1).axis_;
  axis.latitude_dist = Gauss(0.0f, 0.25f);  // internal latitude, external zenith = 90 degrees
  axis.azimuth_dist = kFullTurn;
  axis.roll_dist = Gauss(0.0f, 5.0f);
  return scene;
}

std::vector<double> ComponentX(const FiberComponent& component) {
  std::vector<double> x{ 0.0 };
  for (double step : component.arclength_increments) {
    x.push_back(x.back() + step);
  }
  return x;
}

// C16's narrow orientation variant. The oracle samples a fixed uniform grid on the OFF edges;
// neither its nodes nor its half-height window use the adaptive criterion.
TEST(SinglePathPayload, NarrowPositivePeakGainsLocalResolutionAndAccuracy) {
  const ConfigManager scene = NarrowColumnScene();
  SinglePathRequest request = Request({ 3, 5 }, 40.0, 30.0);
  request.payload_evaluation_limit = 0;
  PointResult off;
  ASSERT_TRUE(AnalyzePathPointValue(scene, request, false, &off).Ok());
  ASSERT_EQ(off.Data().Discovered().components.size(), 1u);
  PointPayloadRefinementStats stats;
  request.payload_evaluation_limit = kPointPayloadEvaluationLimit;
  request.payload_stats = &stats;
  PointResult on;
  ASSERT_TRUE(AnalyzePathPointValue(scene, request, false, &on).Ok());
  ASSERT_EQ(on.Data().Discovered().components.size(), 1u);
  const auto& before = off.Data().Discovered().components[0];
  const auto& after = on.Data().Discovered().components[0];
  const auto x_off = ComponentX(before);
  const auto x_on = ComponentX(after);
  const auto& axis = scene.crystals_.at(1).axis_;
  const auto latitude = MakeLatitudeDensity(axis.latitude_dist);
  std::vector<double> reference_x;
  std::vector<double> reference_rho;
  for (size_t e = 0; e + 1 < before.points.size(); ++e) {
    double relative[9];
    double tangent[3];
    analytic::so3::MatTMul(before.points[e].pose, before.points[e + 1].pose, relative);
    analytic::so3::Log(relative, tangent);
    for (int k = 0; k < 256; ++k) {
      const double t = k / 256.0;
      const double scaled[3] = { tangent[0] * t, tangent[1] * t, tangent[2] * t };
      double step[9];
      double pose[9];
      analytic::so3::Exp(scaled, step);
      analytic::so3::MatMul(before.points[e].pose, step, pose);
      const auto angles = PoseToAngles(pose);
      reference_x.push_back(x_off[e] + t * before.arclength_increments[e]);
      reference_rho.push_back(DeclaredPoseDensity(latitude, axis, angles.azimuth_deg * kDeg,
                                                  (90.0 - angles.zenith_deg) * kDeg, angles.roll_deg * kDeg));
    }
  }
  const size_t peak_index = std::max_element(reference_rho.begin(), reference_rho.end()) - reference_rho.begin();
  const double peak = reference_rho[peak_index];
  ASSERT_GT(peak, 0.0);
  size_t first = peak_index;
  size_t last = peak_index;
  while (first > 0 && reference_rho[first - 1] >= peak / 2) {
    --first;
  }
  while (last + 1 < reference_rho.size() && reference_rho[last + 1] >= peak / 2) {
    ++last;
  }
  ASSERT_GT(first, 0u);
  ASSERT_LT(last + 1, reference_rho.size());
  auto crossing = [&](size_t i) {
    return reference_x[i] + (peak / 2 - reference_rho[i]) * (reference_x[i + 1] - reference_x[i]) /
                                (reference_rho[i + 1] - reference_rho[i]);
  };
  const double lo = crossing(first - 1);
  const double hi = crossing(last);
  ASSERT_GT(hi, lo);
  EXPECT_GE(last - first + 1, 17u);
  auto local_count = [&](const std::vector<double>& x) {
    return std::count_if(x.begin(), x.end(), [&](double at) { return at >= lo && at <= hi; });
  };
  EXPECT_LE(local_count(x_off), 3);
  EXPECT_GE(local_count(x_on), 9);
  double previous = lo;
  double max_gap = 0.0;
  for (double at : x_on) {
    if (at >= lo && at <= hi) {
      max_gap = std::max(max_gap, at - previous);
      previous = at;
    }
  }
  max_gap = std::max(max_gap, hi - previous);
  EXPECT_LE(max_gap, (hi - lo) / 8);
  auto error = [&](const FiberComponent& c, const std::vector<double>& x) {
    double worst = 0.0;
    for (size_t k = first; k <= last; ++k) {
      const double at = reference_x[k];
      const size_t i = std::upper_bound(x.begin(), x.end(), at) - x.begin() - 1;
      const double t = (at - x[i]) / (x[i + 1] - x[i]);
      const double value = c.points[i].rho * (1 - t) + c.points[i + 1].rho * t;
      worst = std::max(worst, std::fabs(value - reference_rho[k]) / peak);
    }
    return worst;
  };
  const double error_off = error(before, x_off);
  const double error_on = error(after, x_on);
  EXPECT_GT(error_off, 0.05);
  EXPECT_LE(error_on, 0.05);
  EXPECT_LE(error_on, error_off / 2);
  EXPECT_GT(stats.inserted_points, 0);
  EXPECT_LE(stats.rho_probes + stats.detail_evaluations, kPointPayloadEvaluationLimit);
}

// Each corpus row guards a different contract: closed anchors; reversed arc/seed ordering;
// singular-measure legal zero; and valid zero area. No assumption that storage count is trace count.
TEST(SinglePathPayload, OriginalAnchorsDistancesAndIntegralSurviveAcrossCorpusCases) {
  struct Case {
    ConfigManager scene;
    SinglePathRequest request;
  };
  ConfigManager random = Scene(Prism(1.0f));
  random.crystals_.at(1).axis_ = Axis({ DistributionType::kUniform, 90.0f, 180.0f }, kFullTurn, kFullTurn);
  ConfigManager plate = Scene(Prism(1.0f));
  plate.crystals_.at(1).axis_ = Axis(Fixed(0.0f), kFullTurn, Fixed(0.0f));
  ConfigManager thin_plate = plate;
  thin_plate.crystals_.at(1).param_ = Prism(0.8f);
  const Case cases[] = { { NarrowColumnScene(), Request({ 3, 5 }, 40.0, 30.0) },
                         { random, Request({ 3, 5, 6, 7 }, 20.0, 120.0) },
                         { plate, Request({ 1, 4, 5, 2 }, 20.0, 120.0) },
                         { thin_plate, Request({ 3, 6, 4, 8 }, 20.0, 120.0) } };
  int inserted = 0;
  for (const auto& test : cases) {
    auto request = test.request;
    request.payload_evaluation_limit = 0;
    PointResult off;
    if (!AnalyzePathPointValue(test.scene, request, false, &off).Ok()) {
      ADD_FAILURE() << "off query failed";
      continue;
    }
    request.payload_evaluation_limit = kPointPayloadEvaluationLimit;
    PointResult on;
    if (!AnalyzePathPointValue(test.scene, request, false, &on).Ok()) {
      ADD_FAILURE() << "on query failed";
      continue;
    }
    const auto& a = off.Data().Discovered().components;
    const auto& b = on.Data().Discovered().components;
    if (a.empty() || a.size() != b.size()) {
      ADD_FAILURE() << "nonempty component discovery must match";
      continue;
    }
    for (size_t c = 0; c < a.size(); ++c) {
      const auto& before = a[c];
      const auto& after = b[c];
      if (after.points.size() != after.arclength_increments.size() + 1 || before.points.empty()) {
        ADD_FAILURE() << "malformed component arrays";
        continue;
      }
      const auto x_off = ComponentX(before);
      const auto x_on = ComponentX(after);
      EXPECT_EQ(before.forward.pose_count, after.forward.pose_count);
      EXPECT_EQ(before.backward.pose_count, after.backward.pose_count);
      EXPECT_EQ(before.kind, after.kind);
      EXPECT_EQ(std::memcmp(before.points.front().pose, after.points.front().pose, sizeof(before.seed)), 0);
      EXPECT_EQ(std::memcmp(before.points.back().pose, after.points.back().pose, sizeof(before.seed)), 0);
      EXPECT_NEAR(x_on.back(), x_off.back(), 1e-12 * std::max(1.0, x_off.back()));
      EXPECT_NEAR(x_on[after.seed_index], x_off[before.seed_index], 1e-12);
      size_t anchor = 0;
      for (size_t i = 0; i < after.points.size(); ++i) {
        const auto& p = after.points[i];
        if (anchor < before.points.size() && std::memcmp(p.pose, before.points[anchor].pose, sizeof(p.pose)) == 0) {
          const auto& original = before.points[anchor];
          EXPECT_DOUBLE_EQ(p.residual_norm, original.residual_norm);
          EXPECT_DOUBLE_EQ(p.rho, original.rho);
          EXPECT_DOUBLE_EQ(p.jacobian, original.jacobian);
          EXPECT_EQ(p.jacobian_available, original.jacobian_available);
          EXPECT_DOUBLE_EQ(p.factor_product, original.factor_product);
          EXPECT_NEAR(x_on[i], x_off[anchor], 1e-12 * std::max(1.0, x_off[anchor]));
          ++anchor;
        } else {
          ++inserted;
          EXPECT_TRUE(std::isnan(p.residual_norm));
          EXPECT_TRUE(p.valid);
          EXPECT_TRUE(std::isfinite(p.rho));
          if (anchor == 0 || anchor >= before.points.size()) {
            ADD_FAILURE() << "inserted point outside original edges";
            continue;
          }
          const double distance = analytic::so3::Distance(before.points[anchor - 1].pose, p.pose) +
                                  analytic::so3::Distance(p.pose, before.points[anchor].pose);
          EXPECT_NEAR(distance, before.arclength_increments[anchor - 1], 1e-12);
        }
        if (i > 0) {
          EXPECT_NEAR(analytic::so3::Distance(after.points[i - 1].pose, p.pose), after.arclength_increments[i - 1],
                      1e-12);
        }
      }
      EXPECT_EQ(anchor, before.points.size());
      double integral = 0.0;
      int available = 0;
      for (size_t i = 0; i < after.points.size(); ++i) {
        if (std::isfinite(after.points[i].factor_product)) {
          ++available;
          if (i < after.arclength_increments.size()) {
            integral += after.points[i].factor_product * after.arclength_increments[i];
          }
        }
      }
      EXPECT_DOUBLE_EQ(integral, after.factors.fiber_integral);
      EXPECT_EQ(available, after.factors.available_points);
      EXPECT_EQ(after.points.size(), static_cast<size_t>(after.factors.total_points));
    }
  }
  EXPECT_GT(inserted, 0);
}

TEST(SinglePathPayload, BudgetIsQueryWideAndCancelledQueriesStartNoRefinement) {
  ConfigManager scene = Scene(Prism(1.0f));
  scene.crystals_.at(1).axis_ = Axis({ DistributionType::kUniform, 90.0f, 180.0f }, kFullTurn, kFullTurn);
  auto request = Request({ 3, 5, 6, 7 }, 20.0, 120.0);  // C03, two components sharing ONE quota
  PointPayloadRefinementStats stats;
  request.payload_stats = &stats;
  for (int limit : { 0, 1, 7, 110, kPointPayloadEvaluationLimit, kPointPayloadEvaluationLimit * 2 }) {
    request.payload_evaluation_limit = limit;
    PointResult result;
    if (!AnalyzePathPointValue(scene, request, false, &result).Ok()) {
      ADD_FAILURE() << "bounded query failed";
      continue;
    }
    EXPECT_EQ(result.Data().Discovered().components.size(), 2u);
    EXPECT_FALSE(result.Data().meta.interrupted);  // refinement quota is not a discovery interruption
    const int used = stats.rho_probes + stats.detail_evaluations;
    EXPECT_LE(used, std::min(limit, kPointPayloadEvaluationLimit));
    if (limit <= 7) {
      EXPECT_EQ(used, limit);
    }
    for (const auto& component : result.Data().Discovered().components) {
      EXPECT_EQ(component.points.size(), component.arclength_increments.size() + 1);
    }
  }
  volatile int interrupted = 1;
  request.interrupted = &interrupted;
  PointResult cancelled;
  ASSERT_TRUE(AnalyzePathPointValue(scene, request, false, &cancelled).Ok());
  EXPECT_TRUE(cancelled.Data().meta.interrupted);
  EXPECT_EQ(stats.rho_probes + stats.detail_evaluations, 0);
}

}  // namespace
}  // namespace lumice::raypath
