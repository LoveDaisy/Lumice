#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "include/lumice_base.h"
#include "include/lumice_raypath.h"
#include "include/lumice_scene.h"

namespace lumice::raypath::survey {
namespace {

// The C01 random prism (the corpus shape, in-memory) and the C15 plate, through the scene JSON
// the C API takes.
constexpr const char* kRandomPrismScene = R"({
  "crystal": [ { "id": 1, "type": "prism",
                 "shape": { "height": 1.0, "face_distance": [1,1,1,1,1,1] },
                 "axis": { "zenith": { "type": "uniform", "mean": 0, "std": 360 },
                           "azimuth": { "type": "uniform", "mean": 0, "std": 360 },
                           "roll": { "type": "uniform", "mean": 0, "std": 360 } } } ],
  "filter": [],
  "scene": { "light_source": { "type": "sun", "altitude": 0, "azimuth": 0, "diameter": 0.5, "spectrum": "D65" },
             "ray_num": 1000, "max_hits": 7,
             "scattering": [ { "prob": 0, "entries": [ { "crystal": 1, "proportion": 1 } ] } ] },
  "render": []
})";

constexpr const char* kPlateScene = R"({
  "crystal": [ { "id": 1, "type": "prism",
                 "shape": { "height": 1.5, "face_distance": [1,1,1,1,1,1] },
                 "axis": { "zenith": { "type": "gauss", "mean": 0, "std": 0.3 },
                           "azimuth": { "type": "uniform", "mean": 0, "std": 360 },
                           "roll": { "type": "uniform", "mean": 0, "std": 360 } } } ],
  "filter": [],
  "scene": { "light_source": { "type": "sun", "altitude": 20, "azimuth": 0, "diameter": 0.5, "spectrum": "D65" },
             "ray_num": 1000, "max_hits": 7,
             "scattering": [ { "prob": 0, "entries": [ { "crystal": 1, "proportion": 1 } ] } ] },
  "render": []
})";

struct SceneDeleter {
  void operator()(LUMICE_Scene* scene) const { LUMICE_SceneDestroy(scene); }
};
using ScenePtr = std::unique_ptr<LUMICE_Scene, SceneDeleter>;

struct MapDeleter {
  void operator()(LUMICE_PathMap* map) const { LUMICE_PathMapDestroy(map); }
};
using MapPtr = std::unique_ptr<LUMICE_PathMap, MapDeleter>;

LUMICE_PathMapRequest RingRequest() {
  LUMICE_PathMapRequest request{};
  request.struct_size = sizeof(LUMICE_PathMapRequest);
  request.crystal_id = 1;
  static const int faces[] = { 3, 5 };
  request.faces = faces;
  request.face_count = 2;
  static const int layer_counts[] = { 2 };
  request.layer_face_counts = layer_counts;
  request.layer_count = 1;
  request.full_sky = 0;
  request.centre_altitude_deg = -20.0;
  request.centre_azimuth_deg = 0.0;
  request.delta_span_deg = 6.0;
  request.azimuth_span_deg = 12.0;
  request.view_rows = 12;
  request.view_cols = 24;
  request.sample_count = 100000;
  return request;
}

char g_err[512];

// The happy path: request -> readers, all contracts on one small ring view.
TEST(MapCApi, RequestEvaluatesAndReadersCarryTheDocument) {
  ScenePtr scene;
  {
    LUMICE_Scene* raw = nullptr;
    ASSERT_EQ(LUMICE_SceneFromJson(kRandomPrismScene, &raw), LUMICE_OK);
    scene.reset(raw);
  }
  LUMICE_PathMapRequest request = RingRequest();
  LUMICE_PathMap* raw_map = nullptr;
  ASSERT_EQ(LUMICE_AnalyzePathMap(scene.get(), &request, &raw_map, g_err, sizeof(g_err)), LUMICE_OK) << g_err;
  MapPtr map(raw_map);

  // The info reader: state, class size, coverage, the view echo.
  LUMICE_PathMapInfo info{};
  info.struct_size = sizeof(info);
  ASSERT_EQ(LUMICE_PathMapGetInfo(map.get(), &info), LUMICE_PATH_READ_OK);
  EXPECT_EQ(info.state, LUMICE_PATH_STATE_FINITE);
  EXPECT_EQ(info.execution, LUMICE_PATH_OUTCOME_COMPLETED);
  EXPECT_EQ(info.member_count, 12);  // the PBD 3-5 expansion
  EXPECT_EQ(info.members_evaluated, 12);
  EXPECT_EQ(info.truncated, 0);
  EXPECT_EQ(info.view_rows, 12);
  EXPECT_EQ(info.view_cols, 24);
  EXPECT_EQ(info.pixel_count, 12u * 24u);
  EXPECT_EQ(info.view_full_sky, 0);
  EXPECT_DOUBLE_EQ(info.wavelength_nm, 550.0);
  EXPECT_NEAR(info.view_delta_min_deg, 17.0, 1e-9);
  EXPECT_NEAR(info.view_delta_max_deg, 23.0, 1e-9);
  const double frame_norm =
      std::sqrt(info.view_incident[0] * info.view_incident[0] + info.view_incident[1] * info.view_incident[1] +
                info.view_incident[2] * info.view_incident[2]);
  EXPECT_NEAR(frame_norm, 1.0, 1e-12);

  // The bulk pixel reader: row-major columns, status domain, borrowed storage.
  LUMICE_PathMapPixels pixels{};
  pixels.struct_size = sizeof(pixels);
  ASSERT_EQ(LUMICE_PathMapGetPixels(map.get(), &pixels), LUMICE_PATH_READ_OK);
  ASSERT_EQ(pixels.pixel_count, info.pixel_count);
  int statuses_ok = 0;
  for (size_t p = 0; p < pixels.pixel_count; p++) {
    if (pixels.status[p] != LUMICE_PATH_MAP_PIXEL_OK && pixels.status[p] != LUMICE_PATH_MAP_PIXEL_SINGULAR &&
        pixels.status[p] != LUMICE_PATH_MAP_PIXEL_POINT_MASS) {
      ADD_FAILURE() << "status out of the domain at " << p;
      continue;
    }
    if (pixels.status[p] == LUMICE_PATH_MAP_PIXEL_OK) {
      statuses_ok++;
      EXPECT_FALSE(std::isnan(pixels.values[p]));
    } else {
      EXPECT_TRUE(std::isnan(pixels.values[p]));
    }
  }
  EXPECT_GT(statuses_ok, 0);
  // A second read hands out the same pointers (borrowed storage, no recompute).
  LUMICE_PathMapPixels again{};
  again.struct_size = sizeof(again);
  ASSERT_EQ(LUMICE_PathMapGetPixels(map.get(), &again), LUMICE_PATH_READ_OK);
  EXPECT_EQ(again.values, pixels.values);
  EXPECT_EQ(again.status, pixels.status);

  // The quantity spec: the map's caliber vocabulary.
  LUMICE_PathQuantitySpec quantity{};
  quantity.struct_size = sizeof(quantity);
  ASSERT_EQ(LUMICE_PathMapGetQuantity(map.get(), &quantity), LUMICE_PATH_READ_OK);
  EXPECT_EQ(quantity.kind, LUMICE_PATH_QUANTITY_BAND_SUM);
  EXPECT_EQ(quantity.method, LUMICE_PATH_METHOD_CLASS_LATTICE_POWER_SUM);
  EXPECT_EQ(quantity.aggregation, LUMICE_PATH_AGGREGATION_PHYSICAL_CLASS);
  EXPECT_EQ(quantity.unit, LUMICE_PATH_UNIT_PER_SR);
  EXPECT_EQ(quantity.spectrum, LUMICE_PATH_SPECTRUM_REFERENCE_WAVELENGTH);
  EXPECT_EQ(quantity.source, LUMICE_PATH_SOURCE_SOLAR_CENTER);
  EXPECT_EQ(quantity.shape, LUMICE_PATH_GEOMETRY_NOMINAL);

  // The evidence reader: the coverage block.
  LUMICE_PathSurveyEvidence evidence{};
  evidence.struct_size = sizeof(evidence);
  ASSERT_EQ(LUMICE_PathMapGetEvidence(map.get(), &evidence), LUMICE_PATH_READ_OK);
  EXPECT_EQ(evidence.state, LUMICE_PATH_STATE_FINITE);
  EXPECT_EQ(evidence.coverage_available, 1);
  EXPECT_EQ(evidence.members_evaluated, 12);

  // No rank-0 member in this class: the mass arm answers NOT_AVAILABLE.
  LUMICE_PathMapPointMass mass{};
  mass.struct_size = sizeof(mass);
  EXPECT_EQ(LUMICE_PathMapGetPointMass(map.get(), &mass), LUMICE_PATH_READ_NOT_AVAILABLE);
}

// AC5's rank-0 shape over the C face: the plate identity class reads the mass arm, and its
// zero cells are a zero texture, not a refusal.
TEST(MapCApi, RankZeroClassReadsThePointMass) {
  ScenePtr scene;
  {
    LUMICE_Scene* raw = nullptr;
    ASSERT_EQ(LUMICE_SceneFromJson(kPlateScene, &raw), LUMICE_OK);
    scene.reset(raw);
  }
  LUMICE_PathMapRequest request{};
  request.struct_size = sizeof(LUMICE_PathMapRequest);
  request.crystal_id = 1;
  static const int faces[] = { 1, 3, 6, 2 };
  request.faces = faces;
  request.face_count = 4;
  static const int layer_counts[] = { 4 };
  request.layer_face_counts = layer_counts;
  request.layer_count = 1;
  request.full_sky = 1;
  request.view_rows = 18;
  request.view_cols = 36;
  request.sample_count = 100000;
  LUMICE_PathMap* raw_map = nullptr;
  ASSERT_EQ(LUMICE_AnalyzePathMap(scene.get(), &request, &raw_map, g_err, sizeof(g_err)), LUMICE_OK) << g_err;
  MapPtr map(raw_map);

  LUMICE_PathMapInfo info{};
  info.struct_size = sizeof(info);
  ASSERT_EQ(LUMICE_PathMapGetInfo(map.get(), &info), LUMICE_PATH_READ_OK);
  EXPECT_EQ(info.point_mass_available, 1);

  LUMICE_PathMapPointMass mass{};
  mass.struct_size = sizeof(mass);
  ASSERT_EQ(LUMICE_PathMapGetPointMass(map.get(), &mass), LUMICE_PATH_READ_OK);
  EXPECT_GT(mass.m, 0.0);
  EXPECT_EQ(mass.method_count, 1u);
  EXPECT_NE(mass.methods, nullptr);
  EXPECT_GE(mass.pixel, 0);
  EXPECT_LT(mass.pixel, static_cast<long long>(info.pixel_count));
}

// The natural rank-0 read order — Info (point_mass_available) then PointMass then Pixels —
// must hand out filled pixel columns however the caller orders the reads: each reader owns
// its projection and fills it once.
TEST(MapCApi, PointMassFirstReadOrderStillFillsThePixelColumns) {
  ScenePtr scene;
  {
    LUMICE_Scene* raw = nullptr;
    ASSERT_EQ(LUMICE_SceneFromJson(kPlateScene, &raw), LUMICE_OK);
    scene.reset(raw);
  }
  LUMICE_PathMapRequest request{};
  request.struct_size = sizeof(LUMICE_PathMapRequest);
  request.crystal_id = 1;
  static const int faces[] = { 1, 3, 6, 2 };
  request.faces = faces;
  request.face_count = 4;
  static const int layer_counts[] = { 4 };
  request.layer_face_counts = layer_counts;
  request.layer_count = 1;
  request.full_sky = 1;
  request.view_rows = 18;
  request.view_cols = 36;
  request.sample_count = 100000;
  LUMICE_PathMap* raw_map = nullptr;
  ASSERT_EQ(LUMICE_AnalyzePathMap(scene.get(), &request, &raw_map, g_err, sizeof(g_err)), LUMICE_OK) << g_err;
  MapPtr map(raw_map);
  LUMICE_PathMapInfo info{};
  info.struct_size = sizeof(info);
  ASSERT_EQ(LUMICE_PathMapGetInfo(map.get(), &info), LUMICE_PATH_READ_OK);
  ASSERT_EQ(info.point_mass_available, 1);
  LUMICE_PathMapPointMass mass{};
  mass.struct_size = sizeof(mass);
  ASSERT_EQ(LUMICE_PathMapGetPointMass(map.get(), &mass), LUMICE_PATH_READ_OK);
  LUMICE_PathMapPixels pixels{};
  pixels.struct_size = sizeof(pixels);
  ASSERT_EQ(LUMICE_PathMapGetPixels(map.get(), &pixels), LUMICE_PATH_READ_OK);
  EXPECT_EQ(pixels.pixel_count, info.pixel_count);
  EXPECT_NE(pixels.delta_deg, nullptr);
  EXPECT_NE(pixels.azimuth_deg, nullptr);
  EXPECT_NE(pixels.values, nullptr);
  EXPECT_NE(pixels.counts, nullptr);
  EXPECT_NE(pixels.status, nullptr);
}

// The named measure refusal rides the result: state refused + the mode; the numeric readers
// answer NOT_AVAILABLE (never a zero map).
TEST(MapCApi, MeasureRefusalRidesTheDocument) {
  static constexpr const char* kFixedPose = R"({
  "crystal": [ { "id": 1, "type": "prism",
                 "shape": { "height": 1.5, "face_distance": [1,1,1,1,1,1] },
                 "axis": { "zenith": 0,
                           "azimuth": { "type": "uniform", "mean": 0, "std": 360 },
                           "roll": { "type": "uniform", "mean": 0, "std": 360 } } } ],
  "filter": [],
  "scene": { "light_source": { "type": "sun", "altitude": 20, "azimuth": 0, "diameter": 0.5, "spectrum": "D65" },
             "ray_num": 1000, "max_hits": 7,
             "scattering": [ { "prob": 0, "entries": [ { "crystal": 1, "proportion": 1 } ] } ] },
  "render": []
})";
  ScenePtr scene;
  {
    LUMICE_Scene* raw = nullptr;
    ASSERT_EQ(LUMICE_SceneFromJson(kFixedPose, &raw), LUMICE_OK);
    scene.reset(raw);
  }
  LUMICE_PathMapRequest request{};
  request.struct_size = sizeof(LUMICE_PathMapRequest);
  request.crystal_id = 1;
  static const int faces[] = { 1, 3, 6, 2 };
  request.faces = faces;
  request.face_count = 4;
  static const int layer_counts[] = { 4 };
  request.layer_face_counts = layer_counts;
  request.layer_count = 1;
  request.full_sky = 1;
  request.view_rows = 4;
  request.view_cols = 8;
  LUMICE_PathMap* raw_map = nullptr;
  ASSERT_EQ(LUMICE_AnalyzePathMap(scene.get(), &request, &raw_map, g_err, sizeof(g_err)), LUMICE_OK) << g_err;
  MapPtr map(raw_map);
  LUMICE_PathMapInfo info{};
  info.struct_size = sizeof(info);
  ASSERT_EQ(LUMICE_PathMapGetInfo(map.get(), &info), LUMICE_PATH_READ_OK);
  EXPECT_EQ(info.state, LUMICE_PATH_STATE_REFUSED);
  const std::string reason(info.reason.data == nullptr ? "" : std::string(info.reason.data, info.reason.length));
  EXPECT_EQ(reason.substr(0, reason.find(':')), "measure_not_expressible") << "reason was: " << reason;
  LUMICE_PathMapPixels pixels{};
  pixels.struct_size = sizeof(pixels);
  EXPECT_EQ(LUMICE_PathMapGetPixels(map.get(), &pixels), LUMICE_PATH_READ_NOT_AVAILABLE);
  LUMICE_PathMapPointMass mass{};
  mass.struct_size = sizeof(mass);
  EXPECT_EQ(LUMICE_PathMapGetPointMass(map.get(), &mass), LUMICE_PATH_READ_NOT_AVAILABLE);
}

// The request refusals and reader contracts, on the C surface's own vocabulary.
TEST(MapCApi, RequestRefusalsAndReaderContracts) {
  ScenePtr scene;
  {
    LUMICE_Scene* raw = nullptr;
    ASSERT_EQ(LUMICE_SceneFromJson(kRandomPrismScene, &raw), LUMICE_OK);
    scene.reset(raw);
  }
  LUMICE_PathMap* map = nullptr;
  // struct_size and null args.
  LUMICE_PathMapRequest request = RingRequest();
  request.struct_size = 4;
  EXPECT_EQ(LUMICE_AnalyzePathMap(scene.get(), &request, &map, g_err, sizeof(g_err)), LUMICE_ERR_INVALID_VALUE);
  request = RingRequest();
  EXPECT_EQ(LUMICE_AnalyzePathMap(nullptr, &request, &map, g_err, sizeof(g_err)), LUMICE_ERR_NULL_ARG);
  EXPECT_EQ(LUMICE_AnalyzePathMap(scene.get(), nullptr, &map, g_err, sizeof(g_err)), LUMICE_ERR_NULL_ARG);
  // Multi-layer.
  request = RingRequest();
  static const int two_layers[] = { 3, 5, 1, 3 };
  static const int layer_counts[] = { 2, 2 };
  request.faces = two_layers;
  request.face_count = 4;
  request.layer_face_counts = layer_counts;
  request.layer_count = 2;
  EXPECT_EQ(LUMICE_AnalyzePathMap(scene.get(), &request, &map, g_err, sizeof(g_err)), LUMICE_ERR_INVALID_VALUE);
  EXPECT_EQ(std::string(g_err).compare(0, 23, "multi_layer_unsupported"), 0);
  // Unknown crystal.
  request = RingRequest();
  request.crystal_id = 7;
  EXPECT_EQ(LUMICE_AnalyzePathMap(scene.get(), &request, &map, g_err, sizeof(g_err)), LUMICE_ERR_INVALID_VALUE);
  EXPECT_EQ(std::string(g_err).compare(0, 18, "unknown_crystal_id"), 0);
  // The grid over the cell bound.
  request = RingRequest();
  request.view_rows = 1000;
  request.view_cols = 1000;
  EXPECT_EQ(LUMICE_AnalyzePathMap(scene.get(), &request, &map, g_err, sizeof(g_err)), LUMICE_ERR_INVALID_VALUE);
  // Two wavelengths: the per-lambda family is a later face.
  request = RingRequest();
  static const double lambdas[] = { 550.0, 650.0 };
  request.wavelengths_nm = lambdas;
  request.wavelength_count = 2;
  EXPECT_EQ(LUMICE_AnalyzePathMap(scene.get(), &request, &map, g_err, sizeof(g_err)), LUMICE_ERR_INVALID_VALUE);
  // NULL arrays with non-zero counts.
  request = RingRequest();
  request.faces = nullptr;
  EXPECT_EQ(LUMICE_AnalyzePathMap(scene.get(), &request, &map, g_err, sizeof(g_err)), LUMICE_ERR_NULL_ARG);
  ASSERT_EQ(map, nullptr) << "a failed entry must not leave a handle";

  // The reader size contract: a smaller struct_size is INVALID_SIZE and writes nothing.
  {
    LUMICE_PathMap* raw_map = nullptr;
    request = RingRequest();
    ASSERT_EQ(LUMICE_AnalyzePathMap(scene.get(), &request, &raw_map, g_err, sizeof(g_err)), LUMICE_OK);
    MapPtr owned(raw_map);
    LUMICE_PathMapInfo info{};
    info.struct_size = 8;
    EXPECT_EQ(LUMICE_PathMapGetInfo(owned.get(), &info), LUMICE_PATH_READ_INVALID_SIZE);
    info.struct_size = sizeof(info);
    EXPECT_EQ(LUMICE_PathMapGetInfo(nullptr, &info), LUMICE_PATH_READ_NULL_ARG);
  }
}

// The workspace face and the lifetime contract: the map outlives workspace and scene, and the
// deadline/cancel options ride the shared query shape.
TEST(MapCApi, WorkspaceFaceAndLifetime) {
  ScenePtr scene;
  {
    LUMICE_Scene* raw = nullptr;
    ASSERT_EQ(LUMICE_SceneFromJson(kRandomPrismScene, &raw), LUMICE_OK);
    scene.reset(raw);
  }
  LUMICE_PathWorkspace* workspace = nullptr;
  ASSERT_EQ(LUMICE_PathWorkspaceCreate(scene.get(), &workspace, g_err, sizeof(g_err)), LUMICE_OK);
  ASSERT_NE(workspace, nullptr);

  LUMICE_PathMapRequest request = RingRequest();
  LUMICE_PathMap* raw_map = nullptr;
  ASSERT_EQ(LUMICE_PathWorkspaceQueryMap(workspace, &request, nullptr, &raw_map, g_err, sizeof(g_err)), LUMICE_OK)
      << g_err;
  MapPtr map(raw_map);

  // The warm handle is not the map face's: a non-null warm option is refused.
  LUMICE_PathQueryOptions options{};
  options.struct_size = sizeof(options);
  options.deadline_ms = 0;
  options.warm = reinterpret_cast<const LUMICE_PathWarmData*>(1);
  LUMICE_PathMap* refused_map = nullptr;
  EXPECT_EQ(LUMICE_PathWorkspaceQueryMap(workspace, &request, &options, &refused_map, g_err, sizeof(g_err)),
            LUMICE_ERR_INVALID_VALUE);

  // Destroy workspace and scene: the map stays readable, every reader.
  LUMICE_PathWorkspaceDestroy(workspace);
  scene.reset();
  LUMICE_PathMapInfo info{};
  info.struct_size = sizeof(info);
  ASSERT_EQ(LUMICE_PathMapGetInfo(map.get(), &info), LUMICE_PATH_READ_OK);
  EXPECT_EQ(info.state, LUMICE_PATH_STATE_FINITE);
  LUMICE_PathMapPixels pixels{};
  pixels.struct_size = sizeof(pixels);
  ASSERT_EQ(LUMICE_PathMapGetPixels(map.get(), &pixels), LUMICE_PATH_READ_OK);
  LUMICE_PathQuantitySpec quantity{};
  quantity.struct_size = sizeof(quantity);
  ASSERT_EQ(LUMICE_PathMapGetQuantity(map.get(), &quantity), LUMICE_PATH_READ_OK);
  LUMICE_PathSurveyEvidence evidence{};
  evidence.struct_size = sizeof(evidence);
  ASSERT_EQ(LUMICE_PathMapGetEvidence(map.get(), &evidence), LUMICE_PATH_READ_OK);
  LUMICE_PathMapPointMass mass{};
  mass.struct_size = sizeof(mass);
  EXPECT_EQ(LUMICE_PathMapGetPointMass(map.get(), &mass), LUMICE_PATH_READ_NOT_AVAILABLE);
  LUMICE_PathMapDestroy(nullptr);  // NULL is a no-op
}

}  // namespace
}  // namespace lumice::raypath::survey
