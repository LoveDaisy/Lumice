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

// The v4.63 member-selection append: the request's extent gates the read. The legacy extent
// (what v4.62 published, through `interrupted` plus its trailing padding) must stay accepted
// with a poisoned appended field — an implementation that reads the field early would refuse
// the request and fail the test.
constexpr size_t kMapRequestSizeV462 = ((offsetof(LUMICE_PathMapRequest, interrupted) + sizeof(void*) + 7) / 8) * 8;
static_assert(kMapRequestSizeV462 < sizeof(LUMICE_PathMapRequest),
              "the appended field must grow sizeof beyond the legacy extent");
static_assert(kMapRequestSizeV462 == 120, "the legacy extent is a published ABI number; pin it");

// Analyzes the ring view with the given member encoding and returns the engine's member count;
// -1 on refusal (g_err carries the reason). The default encoding (0) keeps the all-members
// class the map has always evaluated.
int RingRequestMemberCount(LUMICE_Scene* scene, int symmetry_bits_plus_one) {
  LUMICE_PathMapRequest request = RingRequest();
  request.symmetry_bits_plus_one = symmetry_bits_plus_one;
  LUMICE_PathMap* raw_map = nullptr;
  if (LUMICE_AnalyzePathMap(scene, &request, &raw_map, g_err, sizeof(g_err)) != LUMICE_OK) {
    return -1;
  }
  MapPtr map(raw_map);
  LUMICE_PathMapInfo info{};
  info.struct_size = sizeof(info);
  if (LUMICE_PathMapGetInfo(map.get(), &info) != LUMICE_PATH_READ_OK) {
    return -1;
  }
  return static_cast<int>(info.member_count);
}

// Analyzes the ring view with the given encoding and serializes the pixels reader's raw
// columns (count, values, counts, status) into `blob` for bitwise comparison; returns the
// member count, -1 on refusal.
int AnalyzeRingIntoBlob(LUMICE_Scene* scene, int symmetry_bits_plus_one, bool legacy_extent,
                        std::vector<unsigned char>* blob) {
  LUMICE_PathMapRequest request = RingRequest();
  if (legacy_extent) {
    request.struct_size = kMapRequestSizeV462;
    request.symmetry_bits_plus_one = 999;  // beyond the extent: must never be read
  } else {
    request.symmetry_bits_plus_one = symmetry_bits_plus_one;
  }
  LUMICE_PathMap* raw_map = nullptr;
  if (LUMICE_AnalyzePathMap(scene, &request, &raw_map, g_err, sizeof(g_err)) != LUMICE_OK) {
    return -1;
  }
  MapPtr map(raw_map);
  LUMICE_PathMapInfo info{};
  info.struct_size = sizeof(info);
  if (LUMICE_PathMapGetInfo(map.get(), &info) != LUMICE_PATH_READ_OK) {
    return -1;
  }
  LUMICE_PathMapPixels pixels{};
  pixels.struct_size = sizeof(pixels);
  if (LUMICE_PathMapGetPixels(map.get(), &pixels) != LUMICE_PATH_READ_OK) {
    return -1;
  }
  const auto append = [blob](const void* data, size_t bytes) {
    const auto* begin = static_cast<const unsigned char*>(data);
    blob->insert(blob->end(), begin, begin + bytes);
  };
  append(&pixels.pixel_count, sizeof(pixels.pixel_count));
  append(pixels.values, pixels.pixel_count * sizeof(*pixels.values));
  append(pixels.counts, pixels.pixel_count * sizeof(*pixels.counts));
  append(pixels.status, pixels.pixel_count * sizeof(*pixels.status));
  return static_cast<int>(info.member_count);
}

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

// A real simulator list contains the complete one-face external-reflection path. The
// same request shape the GUI submits must produce a finite, non-empty reference map.
TEST(MapCApi, SingleFaceReflectionProducesPositiveMap) {
  LUMICE_Scene* raw_scene = nullptr;
  ASSERT_EQ(LUMICE_SceneFromJson(kRandomPrismScene, &raw_scene), LUMICE_OK);
  ScenePtr scene(raw_scene);
  LUMICE_PathMapRequest request = RingRequest();
  const int face = 3;
  const int count = 1;
  request.faces = &face;
  request.face_count = 1;
  request.layer_face_counts = &count;
  request.full_sky = 1;
  request.view_rows = 18;
  request.view_cols = 36;
  LUMICE_PathMap* raw_map = nullptr;
  ASSERT_EQ(LUMICE_AnalyzePathMap(scene.get(), &request, &raw_map, g_err, sizeof(g_err)), LUMICE_OK) << g_err;
  MapPtr map(raw_map);
  LUMICE_PathMapInfo info{};
  info.struct_size = sizeof(info);
  ASSERT_EQ(LUMICE_PathMapGetInfo(map.get(), &info), LUMICE_PATH_READ_OK);
  ASSERT_EQ(info.state, LUMICE_PATH_STATE_FINITE);
  EXPECT_GT(info.members_evaluated, 0);
  LUMICE_PathMapPixels pixels{};
  pixels.struct_size = sizeof(pixels);
  ASSERT_EQ(LUMICE_PathMapGetPixels(map.get(), &pixels), LUMICE_PATH_READ_OK);
  int positive = 0;
  for (size_t p = 0; p < pixels.pixel_count; ++p) {
    positive += pixels.status[p] == LUMICE_PATH_MAP_PIXEL_OK && pixels.values[p] > 0.0;
  }
  EXPECT_GT(positive, 0);
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

  // A fixed basal normal reflects to a non-solar mass. Do not send this through
  // the identity arm's sun placement or pretend the singular measure is per-sr.
  const int single_count = 1;
  request.face_count = 1;
  request.layer_face_counts = &single_count;
  raw_map = nullptr;
  ASSERT_EQ(LUMICE_AnalyzePathMap(scene.get(), &request, &raw_map, g_err, sizeof(g_err)), LUMICE_OK) << g_err;
  map.reset(raw_map);
  ASSERT_EQ(LUMICE_PathMapGetInfo(map.get(), &info), LUMICE_PATH_READ_OK);
  EXPECT_EQ(info.state, LUMICE_PATH_STATE_REFUSED);
  const std::string single_reason(info.reason.data, info.reason.length);
  EXPECT_EQ(single_reason.substr(0, single_reason.find(':')), "measure_not_expressible");
  EXPECT_EQ(LUMICE_PathMapGetPixels(map.get(), &pixels), LUMICE_PATH_READ_NOT_AVAILABLE);
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

// The member selection rides the request into the engine's expansion: the literal encoding
// evaluates one member, a partial set differs from the default class, and every accepted value
// yields a finite map.
TEST(MapCApi, MemberSelectionBitsCarryThroughTheRequest) {
  ScenePtr scene;
  {
    LUMICE_Scene* raw = nullptr;
    ASSERT_EQ(LUMICE_SceneFromJson(kRandomPrismScene, &raw), LUMICE_OK);
    scene.reset(raw);
  }
  const int literal = RingRequestMemberCount(scene.get(), 1);
  ASSERT_GT(literal, 0) << g_err;
  EXPECT_EQ(literal, 1);  // the literal 3-5 sequence, no symmetry expansion

  const int p_only = RingRequestMemberCount(scene.get(), 2);
  ASSERT_GT(p_only, 0) << g_err;
  EXPECT_NE(p_only, 12);  // a partial set is neither the literal nor the full class
  EXPECT_LT(p_only, 12);
}

// AC1's bitwise law on the C face: a zero field, an explicit all-members encoding and the legacy
// extent that predates the field produce byte-identical maps (same binary, same input).
TEST(MapCApi, DefaultExplicitFullAndLegacyExtentAreBitwiseIdentical) {
  ScenePtr scene;
  {
    LUMICE_Scene* raw = nullptr;
    ASSERT_EQ(LUMICE_SceneFromJson(kRandomPrismScene, &raw), LUMICE_OK);
    scene.reset(raw);
  }
  std::vector<unsigned char> zero_blob;
  const int zero_members = AnalyzeRingIntoBlob(scene.get(), 0, false, &zero_blob);
  ASSERT_GT(zero_members, 0) << g_err;
  std::vector<unsigned char> explicit_blob;
  const int explicit_members = AnalyzeRingIntoBlob(scene.get(), 8, false, &explicit_blob);
  ASSERT_EQ(explicit_members, zero_members);
  std::vector<unsigned char> legacy_blob;
  const int legacy_members = AnalyzeRingIntoBlob(scene.get(), 0, true, &legacy_blob);
  ASSERT_EQ(legacy_members, zero_members);
  EXPECT_EQ(zero_blob, explicit_blob);
  EXPECT_EQ(zero_blob, legacy_blob);
}

// The encoding's domain: values outside [0, 8] are refused with the named reason and no handle.
TEST(MapCApi, InvalidSymmetryBitsAreRefused) {
  ScenePtr scene;
  {
    LUMICE_Scene* raw = nullptr;
    ASSERT_EQ(LUMICE_SceneFromJson(kRandomPrismScene, &raw), LUMICE_OK);
    scene.reset(raw);
  }
  for (const int bad : { -1, 9 }) {
    LUMICE_PathMapRequest request = RingRequest();
    request.symmetry_bits_plus_one = bad;
    LUMICE_PathMap* map = nullptr;
    EXPECT_EQ(LUMICE_AnalyzePathMap(scene.get(), &request, &map, g_err, sizeof(g_err)), LUMICE_ERR_INVALID_VALUE)
        << "encoding " << bad;
    EXPECT_EQ(std::string(g_err).compare(0, 34, "invalid_argument: invalid physical"), 0) << g_err;
    EXPECT_EQ(map, nullptr);
  }
}

// The two entries' extent discipline: the legacy complete extent rides both with a poisoned
// appended field and keeps the all-members default; the new extent reads the field; anything
// below the legacy extent and not in the published set is refused.
TEST(MapCApi, LegacyExtentAndPoisonTailKeepTheAllMembersDefault) {
  ScenePtr scene;
  {
    LUMICE_Scene* raw = nullptr;
    ASSERT_EQ(LUMICE_SceneFromJson(kRandomPrismScene, &raw), LUMICE_OK);
    scene.reset(raw);
  }
  // The standalone entry: legacy extent, appended field poisoned.
  {
    std::vector<unsigned char> legacy_blob;
    const int legacy_members = AnalyzeRingIntoBlob(scene.get(), 0, true, &legacy_blob);
    ASSERT_EQ(legacy_members, 12) << g_err;  // the poison was never read: the full class ran
  }
  // The workspace entry: legacy accepted, new accepted, below-legacy refused.
  LUMICE_PathWorkspace* workspace = nullptr;
  ASSERT_EQ(LUMICE_PathWorkspaceCreate(scene.get(), &workspace, g_err, sizeof(g_err)), LUMICE_OK);
  {
    LUMICE_PathMapRequest legacy = RingRequest();
    legacy.struct_size = kMapRequestSizeV462;
    legacy.symmetry_bits_plus_one = 999;  // inside the request object but beyond the extent
    LUMICE_PathMap* raw_map = nullptr;
    ASSERT_EQ(LUMICE_PathWorkspaceQueryMap(workspace, &legacy, nullptr, &raw_map, g_err, sizeof(g_err)), LUMICE_OK)
        << g_err;
    MapPtr map(raw_map);
    LUMICE_PathMapInfo info{};
    info.struct_size = sizeof(info);
    ASSERT_EQ(LUMICE_PathMapGetInfo(map.get(), &info), LUMICE_PATH_READ_OK);
    EXPECT_EQ(info.member_count, 12);
  }
  {
    LUMICE_PathMap* raw_map = nullptr;
    LUMICE_PathMapRequest undersized = RingRequest();
    undersized.struct_size = kMapRequestSizeV462 - 8;
    EXPECT_EQ(LUMICE_PathWorkspaceQueryMap(workspace, &undersized, nullptr, &raw_map, g_err, sizeof(g_err)),
              LUMICE_ERR_INVALID_VALUE);
    EXPECT_EQ(raw_map, nullptr);
  }
  LUMICE_PathWorkspaceDestroy(workspace);
}

}  // namespace
}  // namespace lumice::raypath::survey
