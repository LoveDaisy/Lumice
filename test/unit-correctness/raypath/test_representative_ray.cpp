// The representative-ray query (src/raypath/representative_ray.hpp): the typed assembly over the
// analytic kernel's corridor and chain. The kernel's own geometry is pinned by the analytic suite
// (test_entry_measure.cpp); what this file pins is the query's own surface — the request
// validation, the identity echo, the refusal matrix (the corridor's and chain's own vocabulary,
// on LI-pin poses), the cross-result consistency with the same pose's PointDetail, and the
// completion boundary.
//
// Legal poses are taken from a point query's own output (the plan's pose-source rule: no
// fabricated poses), and the refusal poses are the corpus pin poses of the analytic suite.

#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

#include "analytic/entry_measure.hpp"
#include "analytic/path_evaluation.hpp"
#include "config/config_manager.hpp"
#include "core/crystal.hpp"
#include "raypath/representative_ray.hpp"
#include "raypath/scene_to_analytic.hpp"
#include "raypath/single_path_analysis.hpp"

namespace lumice::raypath {
namespace {

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

ConfigManager Scene(float sun_altitude, float sun_azimuth = 0.0f) {
  ConfigManager c;
  CrystalConfig crystal;
  crystal.id_ = 1;
  crystal.param_ = Prism(1.0f);
  c.crystals_.emplace(1, crystal);
  c.scene_.light_source_.param_ = SunParam{ sun_altitude, sun_azimuth, 0.5f };
  std::vector<WlParam> spectrum = { WlParam{ 550.0f, 1.0f } };
  c.scene_.light_source_.spectrum_ = std::move(spectrum);
  return c;
}

RepresentativeRayRequest RayRequest(std::vector<int> faces, const double pose[9]) {
  RepresentativeRayRequest r;
  r.crystal_id = 1;
  r.path_layers = { std::move(faces) };
  for (int i = 0; i < 9; i++) {
    r.pose[i] = pose[i];
  }
  return r;
}

// A first valid pose of the discovered 3-5 fiber: the pose the ray query consumes, with the
// point detail it must agree with.
struct PoseSource {
  double pose[9];
  PointDetail detail;
};

bool FirstValidPose(const PointResult& point, PoseSource* out) {
  for (const FiberComponent& component : point.Data().Discovered().components) {
    for (const PointDetail& detail : component.points) {
      if (detail.valid && detail.entry_measure > 0.0) {
        for (int i = 0; i < 9; i++) {
          out->pose[i] = detail.pose[i];
        }
        out->detail = detail;
        return true;
      }
    }
  }
  return false;
}

// The C01 shape: the discovered fiber's first valid pose produces the bound path, and every
// quantity the two products share agrees.
TEST(RepresentativeRay, ProducesTheBoundPathFromAPointQueryPose) {
  const ConfigManager scene = Scene(20.0f);
  PointResult point;
  SinglePathRequest point_request;
  point_request.crystal_id = 1;
  point_request.path_layers = { { 3, 5 } };
  point_request.target_altitude_deg = 20.0;
  point_request.target_azimuth_deg = 25.0;
  point_request.sample_count = 200000;
  point_request.sun_grid_lat_count = 0;
  ASSERT_TRUE(AnalyzePathPointValue(scene, point_request, false, &point).Ok());
  ASSERT_EQ(point.Data().OutcomeValue(), Outcome::kDiscovered);
  PoseSource source;
  ASSERT_TRUE(FirstValidPose(point, &source)) << "the halo fiber carried no valid pose";

  RepresentativeRayResult ray;
  ASSERT_TRUE(AnalyzeRepresentativeRay(scene, RayRequest({ 3, 5 }, source.pose), &ray).Ok()) << "on the ray query";
  ASSERT_TRUE(ray.Data().Produced());
  const RepresentativeRayValues& path = ray.Data().Path();

  // The identity: two faces, so two contacts and three legs, and every echoed conversion.
  EXPECT_EQ(ray.Data().meta.faces, (std::vector<int>{ 3, 5 }));
  EXPECT_EQ(path.contacts.size(), 2u);
  EXPECT_EQ(path.legs.size(), 3u);
  EXPECT_EQ(ray.Data().meta.crystal_kind, "prism");
  EXPECT_EQ(ray.Data().meta.wavelength_source, WavelengthSource::kConfigSingle);
  EXPECT_DOUBLE_EQ(ray.Data().meta.refractive_index, point.Data().meta.refractive_index);

  // The optics: the same pose's point detail and the ray agree number for number.
  EXPECT_DOUBLE_EQ(path.total_transmission, source.detail.total_transmission);
  EXPECT_DOUBLE_EQ(path.contacts[0].transmittance, source.detail.interface_transmittances[0]);
  EXPECT_DOUBLE_EQ(path.contacts[1].transmittance, source.detail.interface_transmittances[1]);
  EXPECT_DOUBLE_EQ(path.entry_measure, source.detail.entry_measure);
  const double incident[3] = { ray.Data().meta.incident_direction[0], ray.Data().meta.incident_direction[1],
                               ray.Data().meta.incident_direction[2] };
  for (int i = 0; i < 3; i++) {
    EXPECT_DOUBLE_EQ(path.legs[0].direction[i], incident[i]);
    EXPECT_DOUBLE_EQ(path.legs[2].direction[i], source.detail.outgoing_direction[i]);
    // The internal leg is the point detail's own body-frame segment, rotated to world.
    const double expected = source.detail.pose[0 * 3 + i] * source.detail.segment_directions[3 + 0] +
                            source.detail.pose[1 * 3 + i] * source.detail.segment_directions[3 + 1] +
                            source.detail.pose[2 * 3 + i] * source.detail.segment_directions[3 + 2];
    EXPECT_DOUBLE_EQ(path.legs[1].direction[i], expected);
  }

  // The geometry, in the world frame: the entry contact sits on the entry face's plane (the
  // kernel's own containment is pinned by the analytic suite; this pins the frame transform).
  PreparedGeometry geometry;
  ASSERT_TRUE(PrepareGeometry(scene.crystals_.at(1), &geometry).Ok());
  const int entry_slot = geometry.table.SlotOf(3);
  ASSERT_GE(entry_slot, 0);
  double n_world[3];
  for (int i = 0; i < 3; i++) {
    n_world[i] = source.pose[0 * 3 + i] * geometry.table.normal[entry_slot][0] +
                 source.pose[1 * 3 + i] * geometry.table.normal[entry_slot][1] +
                 source.pose[2 * 3 + i] * geometry.table.normal[entry_slot][2];
  }
  double offset = 0.0;
  for (int v = 0; v < geometry.polygons.corner_cnt[entry_slot]; v++) {
    offset += geometry.table.normal[entry_slot][0] * geometry.polygons.corner[entry_slot][v][0] +
              geometry.table.normal[entry_slot][1] * geometry.polygons.corner[entry_slot][v][1] +
              geometry.table.normal[entry_slot][2] * geometry.polygons.corner[entry_slot][v][2];
  }
  offset /= geometry.polygons.corner_cnt[entry_slot];
  double residual = 0.0;
  for (int i = 0; i < 3; i++) {
    residual += n_world[i] * path.contacts[0].point[i];
  }
  EXPECT_NEAR(residual - offset, 0.0, 1e-6 * geometry.polygons.min_edge_length);

  // Repeated reads are the same frozen value, byte for byte.
  const RepresentativeRayValues& again = ray.Data().Path();
  EXPECT_EQ(again.contacts.data(), path.contacts.data());
  EXPECT_EQ(again.legs.data(), path.legs.data());
}

// One refusal-matrix row, per call: the asserts live here so one row's early failure returns
// from the row, not out of the test past the other rows.
void RunRefusalRow(const ConfigManager& scene, const std::vector<int>& faces, const double* pose, const char* reason) {
  SCOPED_TRACE(reason);
  RepresentativeRayResult ray;
  ASSERT_TRUE(AnalyzeRepresentativeRay(scene, RayRequest(faces, pose), &ray).Ok());
  ASSERT_FALSE(ray.Data().Produced());
  const RepresentativeRayRefusal& refusal = ray.Data().Refusal();
  EXPECT_EQ(refusal.reason, reason);
  EXPECT_EQ(refusal.entry_status, reason) << "the corridor's own status name";
  EXPECT_TRUE(refusal.chain_failure.empty());
  EXPECT_DOUBLE_EQ(refusal.entry_measure, 0.0);
  if (std::string(reason) == "corridor_empty") {
    EXPECT_TRUE(refusal.geometry_evaluated);
    EXPECT_LE(refusal.area_perp_internal, refusal.area_threshold);
  } else {
    EXPECT_FALSE(refusal.geometry_evaluated);
  }
}

// The refusal matrix, on the corpus's pinned poses: whichever authority refuses names the
// refusal, and no trajectory numbers travel with it. The scene sun (altitude 15, azimuth 0) is
// the analytic pins' own incident direction.
TEST(RepresentativeRay, RefusalMatrixNamesTheAuthorityThatSpoke) {
  const ConfigManager scene = Scene(15.0f);
  struct Row {
    std::vector<int> faces;
    const double* pose;
    const char* reason;
  };
  static const double kBackface[9] = { -0.5108181656453654, -0.8569178335521683,  -0.06896831291950242,
                                       0.6557541851097652,  -0.3365085527995817,  -0.6758316673578998,
                                       0.5559237810687107,  -0.39045335244173474, 0.7338221373120865 };
  static const double kExitCritical[9] = { 0.5021590044719326,  0.775587735994188,   -0.38249182736781095,
                                           -0.2676411234139845, -0.2812000596610773, -0.9215718938337404,
                                           -0.8223165833700897, 0.5651461671302535,  0.06637203095073863 };
  static const double kCorridorEmpty[9] = { 0.3373647742329948,  -0.8217959637781687, -0.45916925313508417,
                                            -0.6007478036546372, 0.18758651077411415, -0.7771186379052573,
                                            0.724766918057695,   0.5380173741558014,  -0.4304070394355164 };
  const Row rows[] = {
    { { 3, 5 }, kBackface, "entry_backface" },
    { { 3, 5 }, kExitCritical, "exit_critical_angle" },
    { { 3, 5, 6, 7 }, kCorridorEmpty, "corridor_empty" },
  };
  for (const Row& row : rows) {
    RunRefusalRow(scene, row.faces, row.pose, row.reason);
  }
}

// The request's own refusals: the shared vocabulary, never a result.
TEST(RepresentativeRay, RequestRefusals) {
  const ConfigManager scene = Scene(20.0f);
  const double identity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
  RepresentativeRayResult ray;
  auto code = [&](RepresentativeRayRequest q) { return AnalyzeRepresentativeRay(scene, q, &ray).code; };

  RepresentativeRayRequest q = RayRequest({ 3, 5 }, identity);
  q.crystal_id = 7;
  EXPECT_EQ(code(q), ErrorCode::kUnknownCrystalId);
  q = RayRequest({ 3, 5 }, identity);
  q.pose[0] = 2.0;  // no longer a rotation
  EXPECT_EQ(code(q), ErrorCode::kInvalidArgument);
  EXPECT_EQ(code(RayRequest({}, identity)), ErrorCode::kInvalidPath);
  EXPECT_EQ(code(RayRequest({ 3, 13 }, identity)), ErrorCode::kFaceNotInCrystal);
  q = RayRequest({ 3, 5 }, identity);
  q.path_layers.push_back({ 1, 2 });
  EXPECT_EQ(code(q), ErrorCode::kMultiLayerUnsupported);
  q = RayRequest({ 3, 5 }, identity);
  q.wavelength_nm = 1000.0;
  EXPECT_EQ(code(q), ErrorCode::kWavelengthOutOfRange);
}

// The completion boundary: inconsistent payloads cannot freeze.
TEST(RepresentativeRay, CompletionBoundaryRejectsInconsistentPayloads) {
  RepresentativeRayData data;
  data.meta.faces = { 3, 5 };
  RepresentativeRayValues& path = data.payload.emplace<RepresentativeRayValues>();
  path.contacts.resize(2);
  path.legs.resize(2);  // must be face_count + 1
  EXPECT_THROW(FinalizeRepresentativeRayData(std::move(data)), std::invalid_argument);

  RepresentativeRayData wrong_total;
  wrong_total.meta.faces = { 3, 5 };
  RepresentativeRayValues& bad = wrong_total.payload.emplace<RepresentativeRayValues>();
  bad.contacts.resize(2);
  bad.contacts[0].transmittance = 0.5;
  bad.contacts[1].transmittance = 0.5;
  bad.legs.resize(3);
  bad.total_transmission = 0.9;
  EXPECT_THROW(FinalizeRepresentativeRayData(std::move(wrong_total)), std::invalid_argument);

  RepresentativeRayData unnamed;
  unnamed.meta.faces = { 3, 5 };
  unnamed.payload.emplace<RepresentativeRayRefusal>();
  EXPECT_THROW(FinalizeRepresentativeRayData(std::move(unnamed)), std::invalid_argument);

  RepresentativeRayData area_claim;
  area_claim.meta.faces = { 3, 5 };
  area_claim.payload.emplace<RepresentativeRayRefusal>().reason = "corridor_empty";
  std::get<RepresentativeRayRefusal>(area_claim.payload).entry_measure = 0.5;  // a refusal claims no area
  EXPECT_THROW(FinalizeRepresentativeRayData(std::move(area_claim)), std::invalid_argument);

  // And the construction slot itself is not publishable.
  RepresentativeRayData unfinished;
  unfinished.meta.faces = { 3, 5 };
  EXPECT_THROW(RepresentativeRayResult(std::move(unfinished)), std::invalid_argument);
}

}  // namespace
}  // namespace lumice::raypath
