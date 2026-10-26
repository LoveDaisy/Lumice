#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstring>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>

#include "include/lumice_base.h"
#include "include/lumice_raypath.h"
#include "include/lumice_scene.h"
#include "raypath/detail/json_values.hpp"
#include "raypath/query_control.hpp"

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

template <typename Result>
nlohmann::json ResultDocument(const Result* result,
                              LUMICE_ErrorCode (*to_json)(const Result*, char*, size_t, size_t*)) {
  size_t length = 0;
  EXPECT_EQ(to_json(result, nullptr, 0, &length), LUMICE_OK);
  std::string text(length + 1, '\0');
  EXPECT_EQ(to_json(result, text.data(), text.size(), nullptr), LUMICE_OK);
  text.resize(length);
  auto document = nlohmann::json::parse(text);
  document.erase("timing");
  if (document.contains("mc_evidence") && document["mc_evidence"].contains("spectral_verification")) {
    document["mc_evidence"]["spectral_verification"].erase("seconds");
  }
  return document;
}

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

  std::vector<std::thread> readers;
  std::vector<std::string> copies(8);
  for (auto& copy : copies) {
    readers.emplace_back([&result, &copy, len] {
      size_t full_length = 0;
      std::string buffer(len + 1, '\0');
      if (LUMICE_PathSurveyToJson(result.get(), buffer.data(), buffer.size(), &full_length) == LUMICE_OK &&
          full_length == len) {
        copy.assign(buffer.data(), full_length);
      }
    });
  }
  for (auto& reader : readers) {
    reader.join();
  }
  for (const auto& copy : copies) {
    EXPECT_EQ(copy, text);
  }
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
  LUMICE_PathSurveyScope scope{};
  scope.struct_size = sizeof(scope);
  ASSERT_EQ(LUMICE_PathSurveyGetScope(result.get(), &scope), LUMICE_PATH_READ_OK);
  EXPECT_EQ(scope.member_kind, LUMICE_PATH_MEMBER_LITERAL);

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

TEST_F(SurveyCApi, WorkspaceAndLegacyQueriesAgreeOnAllFourFaces) {
  LUMICE_PathWorkspace* raw_workspace = nullptr;
  char err[512] = {};
  ASSERT_EQ(LUMICE_PathWorkspaceCreate(scene_.get(), &raw_workspace, err, sizeof(err)), LUMICE_OK);
  std::unique_ptr<LUMICE_PathWorkspace, void (*)(LUMICE_PathWorkspace*)> workspace(raw_workspace,
                                                                                   LUMICE_PathWorkspaceDestroy);
  static int faces[] = { 1, 2 };
  static int counts[] = { 2 };
  LUMICE_PathQueryOptions options{};
  options.struct_size = sizeof(options);
  options.deadline_ms = 15000;
  options.include_context = 1;

  LUMICE_PathSurveyRequest survey{};
  survey.struct_size = sizeof(survey);
  survey.crystal_id = 1;
  survey.faces = faces;
  survey.face_count = 2;
  survey.layer_face_counts = counts;
  survey.layer_count = 1;
  survey.budget_ms = 15000;
  LUMICE_PathSurvey* legacy_survey = nullptr;
  LUMICE_PathSurvey* workspace_survey = nullptr;
  ASSERT_EQ(LUMICE_AnalyzePathSurvey(scene_.get(), &survey, &legacy_survey, err, sizeof(err)), LUMICE_OK) << err;
  ASSERT_EQ(LUMICE_PathWorkspaceQuerySurvey(workspace.get(), &survey, &options, &workspace_survey, err, sizeof(err)),
            LUMICE_OK)
      << err;
  EXPECT_EQ(ResultDocument(legacy_survey, LUMICE_PathSurveyToJson),
            ResultDocument(workspace_survey, LUMICE_PathSurveyToJson));
  LUMICE_PathSurveyDestroy(legacy_survey);
  LUMICE_PathSurveyDestroy(workspace_survey);
  workspace.reset();
  ASSERT_EQ(LUMICE_PathWorkspaceCreate(scene_.get(), &raw_workspace, err, sizeof(err)), LUMICE_OK);
  workspace.reset(raw_workspace);

  LUMICE_SinglePathRequest point{};
  point.struct_size = sizeof(point);
  point.crystal_id = 1;
  point.faces = faces;
  point.face_count = 2;
  point.layer_face_counts = counts;
  point.layer_count = 1;
  point.target_altitude_deg = 20;
  point.target_azimuth_deg = 25;
  point.sample_count = 100;
  point.deadline_ms = 15000;
  LUMICE_SinglePathResult* legacy_point = nullptr;
  LUMICE_SinglePathResult* workspace_point = nullptr;
  ASSERT_EQ(LUMICE_AnalyzeSinglePath(scene_.get(), &point, &legacy_point, err, sizeof(err)), LUMICE_OK) << err;
  ASSERT_EQ(LUMICE_PathWorkspaceQueryPoint(workspace.get(), &point, &options, &workspace_point, err, sizeof(err)),
            LUMICE_OK)
      << err;
  EXPECT_EQ(ResultDocument(legacy_point, LUMICE_SinglePathResultToJson),
            ResultDocument(workspace_point, LUMICE_SinglePathResultToJson));
  LUMICE_SinglePathResultDestroy(legacy_point);
  LUMICE_SinglePathResultDestroy(workspace_point);

  LUMICE_PathProbeRequest probe{};
  probe.struct_size = sizeof(probe);
  probe.crystal_id = 1;
  probe.faces = faces;
  probe.face_count = 2;
  probe.layer_face_counts = counts;
  probe.layer_count = 1;
  probe.at_altitude_deg = 20;
  probe.at_azimuth_deg = 25;
  probe.sample_count = 100;
  probe.budget_ms = 15000;
  LUMICE_PathProbe* legacy_probe = nullptr;
  LUMICE_PathProbe* workspace_probe = nullptr;
  ASSERT_EQ(LUMICE_AnalyzePathProbe(scene_.get(), &probe, &legacy_probe, err, sizeof(err)), LUMICE_OK) << err;
  ASSERT_EQ(LUMICE_PathWorkspaceQueryProbe(workspace.get(), &probe, &options, &workspace_probe, err, sizeof(err)),
            LUMICE_OK)
      << err;
  EXPECT_EQ(ResultDocument(legacy_probe, LUMICE_PathProbeToJson),
            ResultDocument(workspace_probe, LUMICE_PathProbeToJson));
  LUMICE_PathProbeDestroy(legacy_probe);
  LUMICE_PathProbeDestroy(workspace_probe);

  LUMICE_PathFeatureReportRequest report{};
  report.struct_size = sizeof(report);
  report.crystal_id = 1;
  report.faces = faces;
  report.face_count = 2;
  report.layer_face_counts = counts;
  report.layer_count = 1;
  report.sample_count = 64;
  report.budget_ms = 15000;
  LUMICE_PathFeatureReport* legacy_report = nullptr;
  LUMICE_PathFeatureReport* workspace_report = nullptr;
  ASSERT_EQ(LUMICE_AnalyzePathFeatureReport(scene_.get(), &report, &legacy_report, err, sizeof(err)), LUMICE_OK) << err;
  ASSERT_EQ(LUMICE_PathWorkspaceQueryReport(workspace.get(), &report, &options, &workspace_report, err, sizeof(err)),
            LUMICE_OK)
      << err;
  const auto legacy_report_doc = ResultDocument(legacy_report, LUMICE_PathFeatureReportToJson);
  const auto workspace_report_doc = ResultDocument(workspace_report, LUMICE_PathFeatureReportToJson);
  EXPECT_TRUE(legacy_report_doc == workspace_report_doc)
      << nlohmann::json::diff(legacy_report_doc, workspace_report_doc).dump().substr(0, 1600);
  LUMICE_PathFeatureReportDestroy(legacy_report);
  LUMICE_PathFeatureReportDestroy(workspace_report);
}

TEST_F(SurveyCApi, WorkspacePointHonorsRequestInterruptionFlag) {
  LUMICE_PathWorkspace* raw_workspace = nullptr;
  ASSERT_EQ(LUMICE_PathWorkspaceCreate(scene_.get(), &raw_workspace, nullptr, 0), LUMICE_OK);
  std::unique_ptr<LUMICE_PathWorkspace, void (*)(LUMICE_PathWorkspace*)> workspace(raw_workspace,
                                                                                   LUMICE_PathWorkspaceDestroy);
  static int faces[] = { 3, 5 };
  static int counts[] = { 2 };
  volatile int interrupted = 1;
  LUMICE_SinglePathRequest request{};
  request.struct_size = sizeof(request);
  request.crystal_id = 1;
  request.faces = faces;
  request.face_count = 2;
  request.layer_face_counts = counts;
  request.layer_count = 1;
  request.target_altitude_deg = 20;
  request.target_azimuth_deg = 25;
  request.sample_count = 1000000;
  request.interrupted = &interrupted;
  LUMICE_PathQueryOptions options{};
  options.struct_size = sizeof(options);
  options.deadline_ms = 15000;
  options.include_context = 0;
  LUMICE_SinglePathResult* raw_result = nullptr;
  char err[256] = {};
  ASSERT_EQ(LUMICE_PathWorkspaceQueryPoint(workspace.get(), &request, &options, &raw_result, err, sizeof(err)),
            LUMICE_OK)
      << err;
  std::unique_ptr<LUMICE_SinglePathResult, void (*)(LUMICE_SinglePathResult*)> result(raw_result,
                                                                                      LUMICE_SinglePathResultDestroy);
  EXPECT_TRUE(ResultDocument(result.get(), LUMICE_SinglePathResultToJson)["meta"]["interrupted"].get<bool>());
}

TEST_F(SurveyCApi, CCancelTokenStopsAnEnteredQuery) {
  LUMICE_PathWorkspace* raw_workspace = nullptr;
  ASSERT_EQ(LUMICE_PathWorkspaceCreate(scene_.get(), &raw_workspace, nullptr, 0), LUMICE_OK);
  std::unique_ptr<LUMICE_PathWorkspace, void (*)(LUMICE_PathWorkspace*)> workspace(raw_workspace,
                                                                                   LUMICE_PathWorkspaceDestroy);
  LUMICE_PathCancelToken* raw_cancel = nullptr;
  ASSERT_EQ(LUMICE_PathCancelTokenCreate(&raw_cancel), LUMICE_OK);
  std::unique_ptr<LUMICE_PathCancelToken, void (*)(LUMICE_PathCancelToken*)> cancel(raw_cancel,
                                                                                    LUMICE_PathCancelTokenDestroy);
  // The identity path {1, 2} can finish before the cancelling thread is scheduled.
  // Use the real discovery lattice, without the context's early-refusal shortcut.
  static int faces[] = { 3, 5 };
  static int counts[] = { 2 };
  LUMICE_SinglePathRequest request{};
  request.struct_size = sizeof(request);
  request.crystal_id = 1;
  request.faces = faces;
  request.face_count = 2;
  request.layer_face_counts = counts;
  request.layer_count = 1;
  request.target_altitude_deg = 20;
  request.target_azimuth_deg = 25;
  request.sample_count = 100000000;
  LUMICE_PathQueryOptions options{};
  options.struct_size = sizeof(options);
  options.deadline_ms = 2000;
  options.cancel = cancel.get();
  options.include_context = 0;
  const auto before = QueryDeadlineResolutionCount();
  LUMICE_SinglePathResult* raw_result = nullptr;
  LUMICE_ErrorCode code = LUMICE_ERR_UNKNOWN;
  std::thread worker([&] {
    char err[256] = {};
    code = LUMICE_PathWorkspaceQueryPoint(workspace.get(), &request, &options, &raw_result, err, sizeof(err));
  });
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(1);
  while (QueryDeadlineResolutionCount() == before && std::chrono::steady_clock::now() < until) {
    std::this_thread::yield();
  }
  const bool entered = QueryDeadlineResolutionCount() > before;
  LUMICE_PathCancelTokenRequest(cancel.get());
  worker.join();
  ASSERT_TRUE(entered);
  ASSERT_EQ(code, LUMICE_OK);
  std::unique_ptr<LUMICE_SinglePathResult, void (*)(LUMICE_SinglePathResult*)> result(raw_result,
                                                                                      LUMICE_SinglePathResultDestroy);
  size_t length = 0;
  ASSERT_EQ(LUMICE_SinglePathResultToJson(result.get(), nullptr, 0, &length), LUMICE_OK);
  std::string text(length + 1, '\0');
  ASSERT_EQ(LUMICE_SinglePathResultToJson(result.get(), text.data(), text.size(), nullptr), LUMICE_OK);
  const auto document = nlohmann::json::parse(text);
  EXPECT_TRUE(document.at("meta").at("interrupted").get<bool>());
}

TEST_F(SurveyCApi, WorkspaceRejectsUnpublishedRequestExtentsAndConflictingWarm) {
  LUMICE_PathWorkspace* raw = nullptr;
  char err[256] = {};
  ASSERT_EQ(LUMICE_PathWorkspaceCreate(scene_.get(), &raw, err, sizeof(err)), LUMICE_OK);
  std::unique_ptr<LUMICE_PathWorkspace, void (*)(LUMICE_PathWorkspace*)> workspace(raw, LUMICE_PathWorkspaceDestroy);
  LUMICE_PathSurveyRequest request{};
  request.struct_size = 68;
  LUMICE_PathSurvey* result = reinterpret_cast<LUMICE_PathSurvey*>(0x1);
  EXPECT_EQ(LUMICE_PathWorkspaceQuerySurvey(workspace.get(), &request, nullptr, &result, err, sizeof(err)),
            LUMICE_ERR_INVALID_VALUE);
  EXPECT_EQ(result, nullptr);
  EXPECT_NE(std::string(err).find("published complete extent"), std::string::npos);
  LUMICE_PathQueryOptions options{};
  options.struct_size = sizeof(options) - 1;
  request.struct_size = sizeof(request);
  EXPECT_EQ(LUMICE_PathWorkspaceQuerySurvey(workspace.get(), &request, &options, &result, err, sizeof(err)),
            LUMICE_ERR_INVALID_VALUE);
}

TEST_F(SurveyCApi, WorkspaceQueriesOwnResultsAndTypedWarmOutlivesSurvey) {
  static int faces[] = { 1, 2 };
  static int counts[] = { 2 };
  char err[512] = {};
  LUMICE_PathWorkspace* raw_workspace = nullptr;
  ASSERT_EQ(LUMICE_PathWorkspaceCreate(scene_.get(), &raw_workspace, err, sizeof(err)), LUMICE_OK) << err;
  std::unique_ptr<LUMICE_PathWorkspace, void (*)(LUMICE_PathWorkspace*)> workspace(raw_workspace,
                                                                                   LUMICE_PathWorkspaceDestroy);
  scene_.reset();

  LUMICE_PathQueryOptions options{};
  options.struct_size = sizeof(options);
  options.deadline_ms = 15000;
  options.include_context = 1;
  LUMICE_PathSurveyRequest survey{};
  survey.struct_size = sizeof(survey);
  survey.crystal_id = 1;
  survey.faces = faces;
  survey.face_count = 2;
  survey.layer_face_counts = counts;
  survey.layer_count = 1;
  survey.wavelength_nm = 550;
  LUMICE_PathSurvey* raw_survey = nullptr;
  ASSERT_EQ(LUMICE_PathWorkspaceQuerySurvey(workspace.get(), &survey, &options, &raw_survey, err, sizeof(err)),
            LUMICE_OK)
      << err;
  std::unique_ptr<LUMICE_PathSurvey, void (*)(LUMICE_PathSurvey*)> result(raw_survey, LUMICE_PathSurveyDestroy);
  const long encode_before_export = detail::JsonEncodeCalls();
  size_t survey_length = 0;
  ASSERT_EQ(LUMICE_PathSurveyToJson(result.get(), nullptr, 0, &survey_length), LUMICE_OK);
  EXPECT_EQ(detail::JsonEncodeCalls(), encode_before_export + 1);
  std::string survey_text(survey_length + 1, '\0');
  ASSERT_EQ(LUMICE_PathSurveyToJson(result.get(), survey_text.data(), survey_text.size(), nullptr), LUMICE_OK);
  EXPECT_EQ(detail::JsonEncodeCalls(), encode_before_export + 1);
  const long decode_before_import = detail::JsonDecodeCalls();
  LUMICE_PathWarmData* imported = nullptr;
  ASSERT_EQ(LUMICE_PathWarmDataFromJson(survey_text.data(), survey_length, &imported, err, sizeof(err)), LUMICE_OK)
      << err;
  EXPECT_EQ(detail::JsonDecodeCalls(), decode_before_import + 1);
  LUMICE_PathWarmData* raw_warm = nullptr;
  ASSERT_EQ(LUMICE_PathWarmDataFromSurvey(result.get(), &raw_warm), LUMICE_OK);
  std::unique_ptr<LUMICE_PathWarmData, void (*)(LUMICE_PathWarmData*)> warm(raw_warm, LUMICE_PathWarmDataDestroy);
  result.reset();
  const long before_encode = detail::JsonEncodeCalls();
  const long before_decode = detail::JsonDecodeCalls();
  options.warm = warm.get();

  LUMICE_SinglePathRequest point{};
  point.struct_size = sizeof(point);
  point.crystal_id = 1;
  point.faces = faces;
  point.face_count = 2;
  point.layer_face_counts = counts;
  point.layer_count = 1;
  point.target_altitude_deg = 20;
  point.target_azimuth_deg = 25;
  point.sample_count = 100;
  LUMICE_SinglePathResult* raw_point = nullptr;
  ASSERT_EQ(LUMICE_PathWorkspaceQueryPoint(workspace.get(), &point, &options, &raw_point, err, sizeof(err)), LUMICE_OK)
      << err;
  std::unique_ptr<LUMICE_SinglePathResult, void (*)(LUMICE_SinglePathResult*)> point_result(
      raw_point, LUMICE_SinglePathResultDestroy);
  point.warm_json = "{}";
  point.warm_json_len = 2;
  EXPECT_EQ(LUMICE_PathWorkspaceQueryPoint(workspace.get(), &point, &options, &raw_point, err, sizeof(err)),
            LUMICE_ERR_INVALID_VALUE);
  EXPECT_NE(std::string(err).find("both typed warm and warm_json"), std::string::npos);
  point.warm_json = nullptr;
  point.warm_json_len = 0;

  LUMICE_PathProbeRequest probe{};
  probe.struct_size = sizeof(probe);
  probe.crystal_id = 1;
  probe.faces = faces;
  probe.face_count = 2;
  probe.layer_face_counts = counts;
  probe.layer_count = 1;
  probe.at_altitude_deg = 20;
  probe.at_azimuth_deg = 25;
  probe.sample_count = 100;
  LUMICE_PathProbe* raw_probe = nullptr;
  options.warm = imported;
  ASSERT_EQ(LUMICE_PathWorkspaceQueryProbe(workspace.get(), &probe, &options, &raw_probe, err, sizeof(err)), LUMICE_OK)
      << err;
  LUMICE_PathProbeDestroy(raw_probe);
  LUMICE_PathWarmDataDestroy(imported);
  options.warm = warm.get();
  ASSERT_EQ(LUMICE_PathWorkspaceQueryProbe(workspace.get(), &probe, &options, &raw_probe, err, sizeof(err)), LUMICE_OK)
      << err;
  std::unique_ptr<LUMICE_PathProbe, void (*)(LUMICE_PathProbe*)> probe_result(raw_probe, LUMICE_PathProbeDestroy);
  options.warm = nullptr;

  LUMICE_PathFeatureReportRequest report{};
  report.struct_size = sizeof(report);
  report.crystal_id = 1;
  report.faces = faces;
  report.face_count = 2;
  report.layer_face_counts = counts;
  report.layer_count = 1;
  report.sample_count = 64;
  LUMICE_PathFeatureReport* raw_report = nullptr;
  ASSERT_EQ(LUMICE_PathWorkspaceQueryReport(workspace.get(), &report, &options, &raw_report, err, sizeof(err)),
            LUMICE_OK)
      << err;
  std::unique_ptr<LUMICE_PathFeatureReport, void (*)(LUMICE_PathFeatureReport*)> report_result(
      raw_report, LUMICE_PathFeatureReportDestroy);
  EXPECT_EQ(detail::JsonEncodeCalls(), before_encode);
  EXPECT_EQ(detail::JsonDecodeCalls(), before_decode);

  LUMICE_PathSurvey* raw_concurrent_survey = nullptr;
  ASSERT_EQ(
      LUMICE_PathWorkspaceQuerySurvey(workspace.get(), &survey, &options, &raw_concurrent_survey, err, sizeof(err)),
      LUMICE_OK)
      << err;
  std::unique_ptr<LUMICE_PathSurvey, void (*)(LUMICE_PathSurvey*)> concurrent_survey(raw_concurrent_survey,
                                                                                     LUMICE_PathSurveyDestroy);
  workspace.reset();
  warm.reset();
  const long before_concurrent_encode = detail::JsonEncodeCalls();
  std::array<bool, 8> read_ok{};
  std::vector<std::thread> readers;
  for (size_t i = 0; i < read_ok.size(); ++i) {
    readers.emplace_back([&, i] {
      LUMICE_PathPointInfo point_info{};
      point_info.struct_size = sizeof(point_info);
      LUMICE_PathProbeInfo probe_info{};
      probe_info.struct_size = sizeof(probe_info);
      LUMICE_PathSurveyInfo survey_info{};
      survey_info.struct_size = sizeof(survey_info);
      LUMICE_PathReportInfo report_info{};
      report_info.struct_size = sizeof(report_info);
      size_t length = 0;
      read_ok[i] = LUMICE_PathPointGetInfo(point_result.get(), &point_info) == LUMICE_PATH_READ_OK &&
                   LUMICE_PathProbeGetInfo(probe_result.get(), &probe_info) == LUMICE_PATH_READ_OK &&
                   LUMICE_PathSurveyGetInfo(concurrent_survey.get(), &survey_info) == LUMICE_PATH_READ_OK &&
                   LUMICE_PathReportGetInfo(report_result.get(), &report_info) == LUMICE_PATH_READ_OK &&
                   LUMICE_SinglePathResultToJson(point_result.get(), nullptr, 0, &length) == LUMICE_OK && length > 0 &&
                   LUMICE_PathProbeToJson(probe_result.get(), nullptr, 0, &length) == LUMICE_OK && length > 0 &&
                   LUMICE_PathSurveyToJson(concurrent_survey.get(), nullptr, 0, &length) == LUMICE_OK && length > 0 &&
                   LUMICE_PathFeatureReportToJson(report_result.get(), nullptr, 0, &length) == LUMICE_OK && length > 0;
    });
  }
  for (auto& reader : readers) {
    reader.join();
  }
  for (bool ok : read_ok) {
    EXPECT_TRUE(ok);
  }
  EXPECT_EQ(detail::JsonEncodeCalls(), before_concurrent_encode + 4);
  size_t length = 0;
  ASSERT_EQ(LUMICE_SinglePathResultToJson(point_result.get(), nullptr, 0, &length), LUMICE_OK);
  EXPECT_GT(length, 0u);
  ASSERT_EQ(LUMICE_PathProbeToJson(probe_result.get(), nullptr, 0, &length), LUMICE_OK);
  EXPECT_GT(length, 0u);
  ASSERT_EQ(LUMICE_PathFeatureReportToJson(report_result.get(), nullptr, 0, &length), LUMICE_OK);
  EXPECT_GT(length, 0u);
}

TEST_F(SurveyCApi, TypedBorrowedReadsSurviveWorkspaceAndConcurrentJson) {
  LUMICE_PathWorkspace* raw_workspace = nullptr;
  char err[256] = {};
  ASSERT_EQ(LUMICE_PathWorkspaceCreate(scene_.get(), &raw_workspace, err, sizeof(err)), LUMICE_OK) << err;
  std::unique_ptr<LUMICE_PathWorkspace, void (*)(LUMICE_PathWorkspace*)> workspace(raw_workspace,
                                                                                   LUMICE_PathWorkspaceDestroy);
  static int faces[] = { 1, 2 };
  static int counts[] = { 2 };
  LUMICE_PathSurveyRequest request{};
  request.struct_size = sizeof(request);
  request.crystal_id = 1;
  request.faces = faces;
  request.face_count = 2;
  request.layer_face_counts = counts;
  request.layer_count = 1;
  request.wavelength_nm = 550;
  LUMICE_PathSurvey* raw_result = nullptr;
  ASSERT_EQ(LUMICE_PathWorkspaceQuerySurvey(workspace.get(), &request, nullptr, &raw_result, err, sizeof(err)),
            LUMICE_OK)
      << err;
  std::unique_ptr<LUMICE_PathSurvey, void (*)(LUMICE_PathSurvey*)> result(raw_result, LUMICE_PathSurveyDestroy);
  LUMICE_PathSurveyInfo original{};
  original.struct_size = sizeof(original);
  ASSERT_EQ(LUMICE_PathSurveyGetInfo(result.get(), &original), LUMICE_PATH_READ_OK);
  ASSERT_GT(original.object_count, 0u);
  LUMICE_PathSurveyObject object{};
  object.struct_size = sizeof(object);
  ASSERT_EQ(LUMICE_PathSurveyGetObject(result.get(), 0, &object), LUMICE_PATH_READ_OK);
  ASSERT_NE(object.method.data, nullptr);
  ASSERT_GT(object.method.length, 0u);
  const std::string method(object.method.data, object.method.length);
  size_t json_length = 0;
  ASSERT_EQ(LUMICE_PathSurveyToJson(result.get(), nullptr, 0, &json_length), LUMICE_OK);
  scene_.reset();
  workspace.reset();

  std::vector<LUMICE_PathSurveyInfo> reads(8);
  std::vector<LUMICE_PathReadStatus> statuses(reads.size());
  std::vector<size_t> lengths(reads.size());
  std::vector<std::thread> threads;
  for (size_t i = 0; i < reads.size(); ++i) {
    reads[i].struct_size = sizeof(reads[i]);
    threads.emplace_back([&, i] {
      statuses[i] = LUMICE_PathSurveyGetInfo(result.get(), &reads[i]);
      (void)LUMICE_PathSurveyToJson(result.get(), nullptr, 0, &lengths[i]);
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  for (size_t i = 0; i < reads.size(); ++i) {
    EXPECT_EQ(statuses[i], LUMICE_PATH_READ_OK);
    EXPECT_EQ(reads[i].object_count, original.object_count);
    EXPECT_EQ(reads[i].execution, original.execution);
    EXPECT_EQ(lengths[i], json_length);
  }
  EXPECT_EQ(std::string(object.method.data, object.method.length), method);
}

}  // namespace
}  // namespace lumice::raypath::survey
