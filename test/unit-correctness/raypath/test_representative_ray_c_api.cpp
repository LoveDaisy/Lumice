#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "include/lumice_base.h"
#include "include/lumice_raypath.h"
#include "include/lumice_scene.h"

namespace lumice::raypath {
namespace {

// The C01 random prism through the scene JSON the C API takes, with the analytic pins' own sun
// (altitude 15, azimuth 0), so the corpus's pinned poses apply verbatim.
constexpr const char* kPrismScene = R"({
  "crystal": [ { "id": 1, "type": "prism",
                 "shape": { "height": 1.0, "face_distance": [1,1,1,1,1,1] },
                 "axis": { "zenith": { "type": "uniform", "mean": 0, "std": 360 },
                           "azimuth": { "type": "uniform", "mean": 0, "std": 360 },
                           "roll": { "type": "uniform", "mean": 0, "std": 360 } } } ],
  "filter": [],
  "scene": { "light_source": { "type": "sun", "altitude": 15, "azimuth": 0, "diameter": 0.5, "spectrum": "D65" },
             "ray_num": 1000, "max_hits": 7,
             "scattering": [ { "prob": 0, "entries": [ { "crystal": 1, "proportion": 1 } ] } ] },
  "render": []
})";

// The analytic suite's legal 3-5 pin pose (status kOk): the ray query consumes it verbatim.
const double kLegalPose[9] = { -0.0214067317528861,  -0.6662448270399101, -0.7454257724805471,
                               -0.35598205366005387, 0.7018108832951394,  -0.6170399189359362,
                               0.9342475738837273,   0.252149389313273,   -0.2521946790900098 };
// The pin pose the corridor refuses at entry (status kEntryBackface).
const double kBackfacePose[9] = { -0.5108181656453654, -0.8569178335521683,  -0.06896831291950242,
                                  0.6557541851097652,  -0.3365085527995817,  -0.6758316673578998,
                                  0.5559237810687107,  -0.39045335244173474, 0.7338221373120865 };

struct SceneDeleter {
  void operator()(LUMICE_Scene* scene) const { LUMICE_SceneDestroy(scene); }
};
using ScenePtr = std::unique_ptr<LUMICE_Scene, SceneDeleter>;

struct RayDeleter {
  void operator()(LUMICE_RepresentativeRay* ray) const { LUMICE_RepresentativeRayDestroy(ray); }
};
using RayPtr = std::unique_ptr<LUMICE_RepresentativeRay, RayDeleter>;

LUMICE_RepresentativeRayRequest RingRequest(const double* pose) {
  LUMICE_RepresentativeRayRequest request{};
  request.struct_size = sizeof(LUMICE_RepresentativeRayRequest);
  request.crystal_id = 1;
  static const int faces[] = { 3, 5 };
  request.faces = faces;
  request.face_count = 2;
  static const int layer_counts[] = { 2 };
  request.layer_face_counts = layer_counts;
  request.layer_count = 1;
  request.pose = pose;
  return request;
}

char g_err[512];

// The happy path: request -> readers, all contracts on one legal pose.
TEST(RepresentativeRayCApi, RequestEvaluatesAndReadersCarryThePath) {
  ScenePtr scene;
  {
    LUMICE_Scene* raw = nullptr;
    ASSERT_EQ(LUMICE_SceneFromJson(kPrismScene, &raw), LUMICE_OK);
    scene.reset(raw);
  }
  LUMICE_RepresentativeRayRequest request = RingRequest(kLegalPose);
  LUMICE_RepresentativeRay* raw_ray = nullptr;
  ASSERT_EQ(LUMICE_AnalyzeRepresentativeRay(scene.get(), &request, &raw_ray, g_err, sizeof(g_err)), LUMICE_OK) << g_err;
  RayPtr ray(raw_ray);

  // The info reader: produced, the identity echo, the measured corridor quantities.
  LUMICE_RepresentativeRayInfo info{};
  info.struct_size = sizeof(info);
  ASSERT_EQ(LUMICE_RepresentativeRayGetInfo(ray.get(), &info), LUMICE_PATH_READ_OK);
  EXPECT_EQ(info.produced, 1);
  EXPECT_EQ(info.face_count, 2);
  EXPECT_EQ(info.faces[0], 3);
  EXPECT_EQ(info.faces[1], 5);
  for (int i = 0; i < 9; i++) {
    EXPECT_DOUBLE_EQ(info.pose[i], kLegalPose[i]);
  }
  EXPECT_DOUBLE_EQ(info.wavelength_nm, 550.0);
  // The D65 scene names no single discrete wavelength: the reference rule's default answers.
  EXPECT_EQ(std::string(info.wavelength_source.data, info.wavelength_source.length), "default");
  EXPECT_GT(info.refractive_index, 1.0);
  EXPECT_GT(info.entry_measure, 0.0);
  // The evidence fields, spelled on the produced arm too: the corridor evaluated, passed its
  // own gate, and both measured quantities travel together.
  EXPECT_EQ(std::string(info.entry_status.data, info.entry_status.length), "ok");
  EXPECT_EQ(info.geometry_evaluated, 1);
  EXPECT_GT(info.area_perp_internal, info.area_threshold);
  EXPECT_GT(info.area_threshold, 0.0);
  EXPECT_EQ(std::string(info.reason.data == nullptr ? "" : info.reason.data, info.reason.length), "");

  // The bulk readers: row counts, unit legs, finite contacts, borrowed storage.
  LUMICE_PathRayContacts contacts{};
  contacts.struct_size = sizeof(contacts);
  ASSERT_EQ(LUMICE_RepresentativeRayGetContacts(ray.get(), &contacts), LUMICE_PATH_READ_OK);
  ASSERT_EQ(contacts.contact_count, 2u);
  LUMICE_PathRayLegs legs{};
  legs.struct_size = sizeof(legs);
  ASSERT_EQ(LUMICE_RepresentativeRayGetLegs(ray.get(), &legs), LUMICE_PATH_READ_OK);
  ASSERT_EQ(legs.leg_count, 3u);
  for (size_t k = 0; k < legs.leg_count; k++) {
    const double norm = std::sqrt(legs.legs[k].direction[0] * legs.legs[k].direction[0] +
                                  legs.legs[k].direction[1] * legs.legs[k].direction[1] +
                                  legs.legs[k].direction[2] * legs.legs[k].direction[2]);
    EXPECT_NEAR(norm, 1.0, 1e-12);
    EXPECT_TRUE(std::isfinite(contacts.contacts[k < 2 ? k : 1].point[0]));
    EXPECT_GT(contacts.contacts[k < 2 ? k : 1].transmittance, 0.0);
    EXPECT_LE(contacts.contacts[k < 2 ? k : 1].transmittance, 1.0);
  }
  // Leg 0 is the incident direction the info echoes; leg 2 the outgoing.
  for (int i = 0; i < 3; i++) {
    EXPECT_DOUBLE_EQ(legs.legs[0].direction[i], info.incident_direction[i]);
  }
  // A second read hands out the same pointers (borrowed storage, no recompute).
  LUMICE_PathRayContacts contacts_again{};
  contacts_again.struct_size = sizeof(contacts_again);
  ASSERT_EQ(LUMICE_RepresentativeRayGetContacts(ray.get(), &contacts_again), LUMICE_PATH_READ_OK);
  EXPECT_EQ(contacts_again.contacts, contacts.contacts);
  LUMICE_PathRayLegs legs_again{};
  legs_again.struct_size = sizeof(legs_again);
  ASSERT_EQ(LUMICE_RepresentativeRayGetLegs(ray.get(), &legs_again), LUMICE_PATH_READ_OK);
  EXPECT_EQ(legs_again.legs, legs.legs);
}

// The named refusal rides the result: state refused + the corridor's own name; the trajectory
// readers answer NOT_AVAILABLE (never zeros).
TEST(RepresentativeRayCApi, RefusalRidesTheResult) {
  ScenePtr scene;
  {
    LUMICE_Scene* raw = nullptr;
    ASSERT_EQ(LUMICE_SceneFromJson(kPrismScene, &raw), LUMICE_OK);
    scene.reset(raw);
  }
  LUMICE_RepresentativeRayRequest request = RingRequest(kBackfacePose);
  LUMICE_RepresentativeRay* raw_ray = nullptr;
  ASSERT_EQ(LUMICE_AnalyzeRepresentativeRay(scene.get(), &request, &raw_ray, g_err, sizeof(g_err)), LUMICE_OK) << g_err;
  RayPtr ray(raw_ray);
  LUMICE_RepresentativeRayInfo info{};
  info.struct_size = sizeof(info);
  ASSERT_EQ(LUMICE_RepresentativeRayGetInfo(ray.get(), &info), LUMICE_PATH_READ_OK);
  EXPECT_EQ(info.produced, 0);
  EXPECT_EQ(std::string(info.reason.data, info.reason.length), "entry_backface");
  EXPECT_EQ(std::string(info.entry_status.data, info.entry_status.length), "entry_backface");
  EXPECT_EQ(info.geometry_evaluated, 0);
  EXPECT_DOUBLE_EQ(info.entry_measure, 0.0);
  LUMICE_PathRayContacts contacts{};
  contacts.struct_size = sizeof(contacts);
  EXPECT_EQ(LUMICE_RepresentativeRayGetContacts(ray.get(), &contacts), LUMICE_PATH_READ_NOT_AVAILABLE);
  LUMICE_PathRayLegs legs{};
  legs.struct_size = sizeof(legs);
  EXPECT_EQ(LUMICE_RepresentativeRayGetLegs(ray.get(), &legs), LUMICE_PATH_READ_NOT_AVAILABLE);
}

// The request refusals and reader size contracts, on the C surface's own vocabulary.
TEST(RepresentativeRayCApi, RequestRefusalsAndReaderContracts) {
  ScenePtr scene;
  {
    LUMICE_Scene* raw = nullptr;
    ASSERT_EQ(LUMICE_SceneFromJson(kPrismScene, &raw), LUMICE_OK);
    scene.reset(raw);
  }
  LUMICE_RepresentativeRay* ray = nullptr;
  // struct_size and null args.
  LUMICE_RepresentativeRayRequest request = RingRequest(kLegalPose);
  request.struct_size = 4;
  EXPECT_EQ(LUMICE_AnalyzeRepresentativeRay(scene.get(), &request, &ray, g_err, sizeof(g_err)),
            LUMICE_ERR_INVALID_VALUE);
  request = RingRequest(kLegalPose);
  EXPECT_EQ(LUMICE_AnalyzeRepresentativeRay(nullptr, &request, &ray, g_err, sizeof(g_err)), LUMICE_ERR_NULL_ARG);
  EXPECT_EQ(LUMICE_AnalyzeRepresentativeRay(scene.get(), nullptr, &ray, g_err, sizeof(g_err)), LUMICE_ERR_NULL_ARG);
  // A NULL pose is a null arg, not a garbage rotation.
  request = RingRequest(nullptr);
  EXPECT_EQ(LUMICE_AnalyzeRepresentativeRay(scene.get(), &request, &ray, g_err, sizeof(g_err)), LUMICE_ERR_NULL_ARG);
  // Multi-layer.
  request = RingRequest(kLegalPose);
  static const int two_layers[] = { 3, 5, 1, 3 };
  static const int layer_counts[] = { 2, 2 };
  request.faces = two_layers;
  request.face_count = 4;
  request.layer_face_counts = layer_counts;
  request.layer_count = 2;
  EXPECT_EQ(LUMICE_AnalyzeRepresentativeRay(scene.get(), &request, &ray, g_err, sizeof(g_err)),
            LUMICE_ERR_INVALID_VALUE);
  EXPECT_EQ(std::string(g_err).compare(0, 23, "multi_layer_unsupported"), 0);
  // Unknown crystal.
  request = RingRequest(kLegalPose);
  request.crystal_id = 7;
  EXPECT_EQ(LUMICE_AnalyzeRepresentativeRay(scene.get(), &request, &ray, g_err, sizeof(g_err)),
            LUMICE_ERR_INVALID_VALUE);
  EXPECT_EQ(std::string(g_err).compare(0, 18, "unknown_crystal_id"), 0);
  // Two wavelengths: the per-lambda ray family is a later face.
  request = RingRequest(kLegalPose);
  static const double lambdas[] = { 550.0, 650.0 };
  request.wavelengths_nm = lambdas;
  request.wavelength_count = 2;
  EXPECT_EQ(LUMICE_AnalyzeRepresentativeRay(scene.get(), &request, &ray, g_err, sizeof(g_err)),
            LUMICE_ERR_INVALID_VALUE);
  // NULL arrays with non-zero counts.
  request = RingRequest(kLegalPose);
  request.faces = nullptr;
  EXPECT_EQ(LUMICE_AnalyzeRepresentativeRay(scene.get(), &request, &ray, g_err, sizeof(g_err)), LUMICE_ERR_NULL_ARG);
  ASSERT_EQ(ray, nullptr) << "a failed entry must not leave a handle";

  // The reader size contract: a smaller struct_size is INVALID_SIZE and writes nothing.
  {
    LUMICE_RepresentativeRay* raw_ray = nullptr;
    request = RingRequest(kLegalPose);
    ASSERT_EQ(LUMICE_AnalyzeRepresentativeRay(scene.get(), &request, &raw_ray, g_err, sizeof(g_err)), LUMICE_OK);
    RayPtr owned(raw_ray);
    LUMICE_RepresentativeRayInfo info{};
    info.struct_size = 8;
    EXPECT_EQ(LUMICE_RepresentativeRayGetInfo(owned.get(), &info), LUMICE_PATH_READ_INVALID_SIZE);
    info.struct_size = sizeof(info);
    EXPECT_EQ(LUMICE_RepresentativeRayGetInfo(nullptr, &info), LUMICE_PATH_READ_NULL_ARG);
    LUMICE_RepresentativeRayDestroy(nullptr);  // NULL is a no-op
  }
}

// The workspace face and the lifetime contract: the ray outlives workspace and scene.
TEST(RepresentativeRayCApi, WorkspaceFaceAndLifetime) {
  ScenePtr scene;
  {
    LUMICE_Scene* raw = nullptr;
    ASSERT_EQ(LUMICE_SceneFromJson(kPrismScene, &raw), LUMICE_OK);
    scene.reset(raw);
  }
  LUMICE_PathWorkspace* workspace = nullptr;
  ASSERT_EQ(LUMICE_PathWorkspaceCreate(scene.get(), &workspace, g_err, sizeof(g_err)), LUMICE_OK);
  ASSERT_NE(workspace, nullptr);

  LUMICE_RepresentativeRayRequest request = RingRequest(kLegalPose);
  LUMICE_RepresentativeRay* raw_ray = nullptr;
  ASSERT_EQ(LUMICE_PathWorkspaceQueryRepresentativeRay(workspace, &request, nullptr, &raw_ray, g_err, sizeof(g_err)),
            LUMICE_OK)
      << g_err;
  RayPtr ray(raw_ray);

  // The warm handle is not the ray face's: a non-null warm option is refused.
  LUMICE_PathQueryOptions options{};
  options.struct_size = sizeof(options);
  options.deadline_ms = 0;
  options.warm = reinterpret_cast<const LUMICE_PathWarmData*>(1);
  LUMICE_RepresentativeRay* refused_ray = nullptr;
  EXPECT_EQ(
      LUMICE_PathWorkspaceQueryRepresentativeRay(workspace, &request, &options, &refused_ray, g_err, sizeof(g_err)),
      LUMICE_ERR_INVALID_VALUE);

  // Destroy workspace and scene: the ray stays readable, every reader.
  LUMICE_PathWorkspaceDestroy(workspace);
  scene.reset();
  LUMICE_RepresentativeRayInfo info{};
  info.struct_size = sizeof(info);
  ASSERT_EQ(LUMICE_RepresentativeRayGetInfo(ray.get(), &info), LUMICE_PATH_READ_OK);
  EXPECT_EQ(info.produced, 1);
  LUMICE_PathRayContacts contacts{};
  contacts.struct_size = sizeof(contacts);
  ASSERT_EQ(LUMICE_RepresentativeRayGetContacts(ray.get(), &contacts), LUMICE_PATH_READ_OK);
  LUMICE_PathRayLegs legs{};
  legs.struct_size = sizeof(legs);
  ASSERT_EQ(LUMICE_RepresentativeRayGetLegs(ray.get(), &legs), LUMICE_PATH_READ_OK);
}

// The workspace form and the standalone form produce the same numbers on the same request.
TEST(RepresentativeRayCApi, WorkspaceAndStandaloneFormsAgree) {
  ScenePtr scene;
  {
    LUMICE_Scene* raw = nullptr;
    ASSERT_EQ(LUMICE_SceneFromJson(kPrismScene, &raw), LUMICE_OK);
    scene.reset(raw);
  }
  LUMICE_RepresentativeRayRequest request = RingRequest(kLegalPose);
  LUMICE_RepresentativeRay* standalone = nullptr;
  ASSERT_EQ(LUMICE_AnalyzeRepresentativeRay(scene.get(), &request, &standalone, g_err, sizeof(g_err)), LUMICE_OK);
  RayPtr owned_standalone(standalone);
  LUMICE_PathWorkspace* workspace = nullptr;
  ASSERT_EQ(LUMICE_PathWorkspaceCreate(scene.get(), &workspace, g_err, sizeof(g_err)), LUMICE_OK);
  LUMICE_RepresentativeRay* from_workspace = nullptr;
  ASSERT_EQ(
      LUMICE_PathWorkspaceQueryRepresentativeRay(workspace, &request, nullptr, &from_workspace, g_err, sizeof(g_err)),
      LUMICE_OK);
  RayPtr owned_workspace(from_workspace);
  LUMICE_PathWorkspaceDestroy(workspace);

  LUMICE_PathRayContacts a{};
  a.struct_size = sizeof(a);
  ASSERT_EQ(LUMICE_RepresentativeRayGetContacts(owned_standalone.get(), &a), LUMICE_PATH_READ_OK);
  LUMICE_PathRayContacts b{};
  b.struct_size = sizeof(b);
  ASSERT_EQ(LUMICE_RepresentativeRayGetContacts(owned_workspace.get(), &b), LUMICE_PATH_READ_OK);
  ASSERT_EQ(a.contact_count, b.contact_count);
  for (size_t k = 0; k < a.contact_count; k++) {
    for (int i = 0; i < 3; i++) {
      EXPECT_DOUBLE_EQ(a.contacts[k].point[i], b.contacts[k].point[i]);
    }
    EXPECT_DOUBLE_EQ(a.contacts[k].transmittance, b.contacts[k].transmittance);
  }
}

}  // namespace
}  // namespace lumice::raypath
