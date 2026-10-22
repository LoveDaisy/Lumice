#include <gtest/gtest.h>

#include <cstring>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "include/lumice_base.h"
#include "include/lumice_raypath.h"
#include "include/lumice_scene.h"

namespace lumice::raypath::survey {
namespace {

// The same plate scene the assembly test builds, through the scene JSON the C API takes — the
// corpus C14 numbers (plate prism, zenith Gaussian 0±0.3, sun 20/0, D65).
constexpr const char* kSceneJson = R"({
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

struct SceneDeleter {
  void operator()(LUMICE_Scene* scene) const { LUMICE_SceneDestroy(scene); }
};
using ScenePtr = std::unique_ptr<LUMICE_Scene, SceneDeleter>;

class SurveyCApi : public ::testing::Test {
 protected:
  void SetUp() override {
    LUMICE_Scene* raw = nullptr;
    ASSERT_EQ(LUMICE_SceneFromJson(kSceneJson, &raw), LUMICE_OK) << "the C14 plate scene must parse";
    scene_.reset(raw);
  }
  ScenePtr scene_;
};

TEST_F(SurveyCApi, StructSizeTooSmallIsRefused) {
  LUMICE_PathSurveyRequest request{};
  request.struct_size = 4;  // smaller than any version's struct
  request.crystal_id = 1;
  static int faces[] = { 1, 2 };
  static int counts[] = { 2 };
  request.faces = faces;
  request.face_count = 2;
  request.layer_face_counts = counts;
  request.layer_count = 1;
  LUMICE_PathSurvey* result = nullptr;
  char err[512] = {};
  EXPECT_EQ(LUMICE_AnalyzePathSurvey(scene_.get(), &request, &result, err, sizeof(err)), LUMICE_ERR_INVALID_VALUE);
  EXPECT_EQ(result, nullptr);
  EXPECT_NE(std::string(err).find("struct_size 4 is smaller than this version's LUMICE_PathSurveyRequest"),
            std::string::npos);
}

TEST_F(SurveyCApi, UnknownCrystalIsRefusedWithTheStableName) {
  LUMICE_PathSurveyRequest request{};
  request.struct_size = sizeof(request);
  request.crystal_id = 42;
  static int faces[] = { 1, 2 };
  static int counts[] = { 2 };
  request.faces = faces;
  request.face_count = 2;
  request.layer_face_counts = counts;
  request.layer_count = 1;
  LUMICE_PathSurvey* result = nullptr;
  char err[512] = {};
  EXPECT_EQ(LUMICE_AnalyzePathSurvey(scene_.get(), &request, &result, err, sizeof(err)), LUMICE_ERR_INVALID_VALUE);
  EXPECT_EQ(result, nullptr);
  EXPECT_STREQ(err, "unknown_crystal_id: unknown crystal id");
}

TEST_F(SurveyCApi, HappyPathReturnsTheSchema4Document) {
  LUMICE_PathSurveyRequest request{};
  request.struct_size = sizeof(request);
  request.crystal_id = 1;
  static int faces[] = { 1, 2 };
  static int counts[] = { 2 };
  request.faces = faces;
  request.face_count = 2;
  request.layer_face_counts = counts;
  request.layer_count = 1;
  request.wavelength_nm = 550.0;
  request.symmetry_bits_plus_one = 0;  // the PBD default
  LUMICE_PathSurvey* raw = nullptr;
  char err[512] = {};
  ASSERT_EQ(LUMICE_AnalyzePathSurvey(scene_.get(), &request, &raw, err, sizeof(err)), LUMICE_OK);
  std::unique_ptr<LUMICE_PathSurvey, void (*)(LUMICE_PathSurvey*)> result(raw, LUMICE_PathSurveyDestroy);

  size_t len = 0;
  ASSERT_EQ(LUMICE_PathSurveyToJson(result.get(), nullptr, 0, &len), LUMICE_OK);
  std::string text(len + 1, '\0');
  ASSERT_EQ(LUMICE_PathSurveyToJson(result.get(), text.data(), text.size(), &len), LUMICE_OK);
  text.resize(len);

  const auto document = nlohmann::json::parse(text);
  EXPECT_EQ(document.at("schema"), "lumice.path-survey");
  EXPECT_EQ(document.at("schema_version"), 4);
  ASSERT_EQ(document.at("objects").size(), 1u);
  EXPECT_EQ(document.at("objects")[0].at("kind"), "source_image");
  EXPECT_EQ(document.at("objects")[0].at("evidence").at("state"), "certified");
  EXPECT_FALSE(document.at("objects")[0].at("evidence").at("method").get<std::string>().empty());
  EXPECT_DOUBLE_EQ(document.at("objects")[0].at("position").at("altitude_deg").get<double>(), 20.0);
  EXPECT_DOUBLE_EQ(document.at("objects")[0].at("position").at("azimuth_deg").get<double>(), 0.0);
  // The echo records the request's wavelength, given, and the symmetry bits after encoding.
  EXPECT_DOUBLE_EQ(document.at("request").at("wavelength_nm").get<double>(), 550.0);
  EXPECT_EQ(document.at("request").at("symmetry_bits"), 7);
}

TEST_F(SurveyCApi, LiteralSymmetryAndNullArgContracts) {
  LUMICE_PathSurveyRequest request{};
  request.struct_size = sizeof(request);
  request.crystal_id = 1;
  static int faces[] = { 1, 2 };
  static int counts[] = { 2 };
  request.faces = faces;
  request.face_count = 2;
  request.layer_face_counts = counts;
  request.layer_count = 1;
  request.symmetry_bits_plus_one = 1;  // the literal sequence
  LUMICE_PathSurvey* raw = nullptr;
  char err[512] = {};
  ASSERT_EQ(LUMICE_AnalyzePathSurvey(scene_.get(), &request, &raw, err, sizeof(err)), LUMICE_OK);
  std::unique_ptr<LUMICE_PathSurvey, void (*)(LUMICE_PathSurvey*)> result(raw, LUMICE_PathSurveyDestroy);
  size_t len = 0;
  ASSERT_EQ(LUMICE_PathSurveyToJson(result.get(), nullptr, 0, &len), LUMICE_OK);
  std::string text(len + 1, '\0');
  ASSERT_EQ(LUMICE_PathSurveyToJson(result.get(), text.data(), text.size(), &len), LUMICE_OK);
  text.resize(len);
  const auto document = nlohmann::json::parse(text);
  EXPECT_EQ(document.at("request").at("symmetry_bits"), 0);
  ASSERT_EQ(document.at("objects")[0].at("members").size(), 1u);

  EXPECT_EQ(LUMICE_PathSurveyToJson(nullptr, nullptr, 0, nullptr), LUMICE_ERR_NULL_ARG);
  LUMICE_PathSurveyDestroy(nullptr);  // a no-op, not a crash
  EXPECT_EQ(LUMICE_AnalyzePathSurvey(nullptr, &request, &raw, err, sizeof(err)), LUMICE_ERR_NULL_ARG);
}

// The appended mc_ess_draws reads only when the caller's struct carries it: a v4.54-shaped
// request (struct_size stops at symmetry_bits_plus_one) with an out-of-range count in its
// tail must succeed — the gate ignores the tail, so the poison never reaches the engine's
// validation (a gate that read it would turn the call invalid_argument). A full-size request
// is validated: past the cap is invalid_argument, and a valid count on an identity-arm path
// succeeds with no mc_region_ess key (the arm does not run the leg).
TEST_F(SurveyCApi, McEssDrawsReadThroughTheStructSizeGate) {
  static int faces[] = { 1, 2 };
  static int counts[] = { 2 };
  LUMICE_PathSurveyRequest request{};
  request.crystal_id = 1;
  request.faces = faces;
  request.face_count = 2;
  request.layer_face_counts = counts;
  request.layer_count = 1;

  const size_t kV454Size = offsetof(LUMICE_PathSurveyRequest, symmetry_bits_plus_one) + sizeof(int);
  request.struct_size = kV454Size;
  request.symmetry_bits_plus_one = 0;
  request.mc_ess_draws = LUMICE_PATH_SURVEY_MAX_MC_ESS_DRAWS + 1;  // the discriminating poison
  LUMICE_PathSurvey* raw = nullptr;
  char err[512] = {};
  ASSERT_EQ(LUMICE_AnalyzePathSurvey(scene_.get(), &request, &raw, err, sizeof(err)), LUMICE_OK);
  LUMICE_PathSurveyDestroy(raw);

  request.struct_size = sizeof(request);
  request.mc_ess_draws = LUMICE_PATH_SURVEY_MAX_MC_ESS_DRAWS + 1;
  EXPECT_EQ(LUMICE_AnalyzePathSurvey(scene_.get(), &request, &raw, err, sizeof(err)), LUMICE_ERR_INVALID_VALUE);
  EXPECT_EQ(raw, nullptr);

  request.mc_ess_draws = 200;  // valid, but the plate early-stops before the leg's arms
  ASSERT_EQ(LUMICE_AnalyzePathSurvey(scene_.get(), &request, &raw, err, sizeof(err)), LUMICE_OK);
  std::unique_ptr<LUMICE_PathSurvey, void (*)(LUMICE_PathSurvey*)> result(raw, LUMICE_PathSurveyDestroy);
  size_t len = 0;
  ASSERT_EQ(LUMICE_PathSurveyToJson(result.get(), nullptr, 0, &len), LUMICE_OK);
  std::string text(len + 1, '\0');
  ASSERT_EQ(LUMICE_PathSurveyToJson(result.get(), text.data(), text.size(), &len), LUMICE_OK);
  const auto document = nlohmann::json::parse(text);
  EXPECT_FALSE(document.contains("mc_region_ess"));
}

}  // namespace
}  // namespace lumice::raypath::survey
