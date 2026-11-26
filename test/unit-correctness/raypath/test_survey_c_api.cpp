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
  // The poll snapshot's live half: sampled from the caller's seat while the query runs on the
  // worker — ticks never decrease across samples, and a nonzero tick count names a stage this
  // request's face (point, no context) can be in.
  long sampled_ticks = 0;
  while (QueryDeadlineResolutionCount() == before && std::chrono::steady_clock::now() < until) {
    LUMICE_PathPollSnapshot live{};
    live.struct_size = sizeof(live);
    if (LUMICE_PathCancelTokenPoll(cancel.get(), &live) == LUMICE_OK) {
      EXPECT_GE(live.ticks, sampled_ticks) << "the tick count went backwards across samples";
      sampled_ticks = live.ticks;
      if (live.ticks > 0) {
        EXPECT_TRUE(live.stage == LUMICE_PATH_POLL_STAGE_POINT_LEGS ||
                    live.stage == LUMICE_PATH_POLL_STAGE_FIBER_DISCOVERY)
            << "stage " << static_cast<int>(live.stage) << " is outside the point face's no-context domain";
      }
    }
    std::this_thread::yield();
  }
  const bool entered = QueryDeadlineResolutionCount() > before;
  LUMICE_PathCancelTokenRequest(cancel.get());
  worker.join();
  ASSERT_TRUE(entered);
  ASSERT_EQ(code, LUMICE_OK);
  // After the query returns the token stays readable and frozen — a stalled snapshot is NOT a
  // completion claim (there is no done value; the caller knows the query settled by its return).
  LUMICE_PathPollSnapshot settled{};
  settled.struct_size = sizeof(settled);
  ASSERT_EQ(LUMICE_PathCancelTokenPoll(cancel.get(), &settled), LUMICE_OK);
  EXPECT_GE(settled.ticks, sampled_ticks);
  EXPECT_GT(settled.ticks, 0) << "the discovery lattice ran but never ticked the ledger";
  std::unique_ptr<LUMICE_SinglePathResult, void (*)(LUMICE_SinglePathResult*)> result(raw_result,
                                                                                      LUMICE_SinglePathResultDestroy);
  size_t length = 0;
  ASSERT_EQ(LUMICE_SinglePathResultToJson(result.get(), nullptr, 0, &length), LUMICE_OK);
  std::string text(length + 1, '\0');
  ASSERT_EQ(LUMICE_SinglePathResultToJson(result.get(), text.data(), text.size(), nullptr), LUMICE_OK);
  const auto document = nlohmann::json::parse(text);
  EXPECT_TRUE(document.at("meta").at("interrupted").get<bool>());
}

// The poll snapshot's refusal half: a fresh token idles (NONE/0 — the documented idle shape),
// NULLs are named refusals, an undersized extent is refused with NOTHING written, and a larger
// future tail is accepted (the typed readers' size discipline).
TEST_F(SurveyCApi, PollSnapshotIdlesOnAFreshTokenAndRefusesBadCalls) {
  LUMICE_PathCancelToken* raw_cancel = nullptr;
  ASSERT_EQ(LUMICE_PathCancelTokenCreate(&raw_cancel), LUMICE_OK);
  std::unique_ptr<LUMICE_PathCancelToken, void (*)(LUMICE_PathCancelToken*)> cancel(raw_cancel,
                                                                                    LUMICE_PathCancelTokenDestroy);
  LUMICE_PathPollSnapshot snap{};
  snap.struct_size = sizeof(snap);
  ASSERT_EQ(LUMICE_PathCancelTokenPoll(cancel.get(), &snap), LUMICE_OK);
  EXPECT_EQ(snap.stage, LUMICE_PATH_POLL_STAGE_NONE);
  EXPECT_EQ(snap.ticks, 0);

  EXPECT_EQ(LUMICE_PathCancelTokenPoll(nullptr, &snap), LUMICE_ERR_NULL_ARG);
  EXPECT_EQ(LUMICE_PathCancelTokenPoll(cancel.get(), nullptr), LUMICE_ERR_NULL_ARG);

  struct FutureTail : LUMICE_PathPollSnapshot {
    char extra[16];
  };
  FutureTail future{};
  future.struct_size = sizeof(FutureTail);
  ASSERT_EQ(LUMICE_PathCancelTokenPoll(cancel.get(), &future), LUMICE_OK);
  EXPECT_EQ(future.stage, LUMICE_PATH_POLL_STAGE_NONE);
  EXPECT_EQ(future.ticks, 0);

  snap.stage = LUMICE_PATH_POLL_STAGE_WITNESS;  // sentinels a refusal must not touch
  snap.ticks = 42;
  snap.struct_size = sizeof(snap) - 1;
  EXPECT_EQ(LUMICE_PathCancelTokenPoll(cancel.get(), &snap), LUMICE_ERR_INVALID_VALUE);
  EXPECT_EQ(snap.stage, LUMICE_PATH_POLL_STAGE_WITNESS);  // zero-write on the refused extent
  EXPECT_EQ(snap.ticks, 42);
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

TEST_F(SurveyCApi, ContextScopePreservesAutomaticExplicitAndLegacyContracts) {
  // C01's random regular prism avoids the narrow plate's unrelated psi-average tint
  // quadrature. This identity traversal still produces complete, positive survey facts;
  // nontrivial members and independently recomputed profiles are checked by Workspace.
  auto scene_json = nlohmann::json::parse(kSceneJson);
  scene_json["crystal"][0]["shape"]["height"] = 1.0;
  scene_json["crystal"][0]["axis"]["zenith"] = { { "type", "uniform" }, { "mean", 0 }, { "std", 360 } };
  scene_json["scene"]["light_source"]["altitude"] = 0;
  LUMICE_Scene* raw_scene = nullptr;
  ASSERT_EQ(LUMICE_SceneFromJson(scene_json.dump().c_str(), &raw_scene), LUMICE_OK);
  scene_.reset(raw_scene);
  const int faces[] = { 1, 2 };
  const int counts[] = { 2 };
  LUMICE_SinglePathRequest point{};
  point.struct_size = sizeof(point);
  point.crystal_id = 1;
  point.faces = faces;
  point.face_count = 2;
  point.layer_face_counts = counts;
  point.layer_count = 1;
  point.target_altitude_deg = 36;
  point.target_azimuth_deg = 52;
  point.sample_count = 100;
  LUMICE_PathProbeRequest probe{};
  probe.struct_size = sizeof(probe);
  probe.crystal_id = 1;
  probe.faces = faces;
  probe.face_count = 2;
  probe.layer_face_counts = counts;
  probe.layer_count = 1;
  probe.at_altitude_deg = 36;
  probe.at_azimuth_deg = 52;
  probe.sample_count = 100;
  LUMICE_PathQueryOptions options{};
  options.struct_size = sizeof(options);
  options.include_context = 1;
  char err[512]{};
  const size_t kOldPointExtent = offsetof(LUMICE_SinglePathRequest, interrupted) + sizeof(void*);
  const size_t kOldProbeExtent = offsetof(LUMICE_PathProbeRequest, interrupted) + sizeof(void*);
  std::array<std::string, 2> survey_json;
  std::vector<std::unique_ptr<LUMICE_PathWarmData, decltype(&LUMICE_PathWarmDataDestroy)>> warm_sources;
  const auto check_producer = [&](int producer_scope) {
    SCOPED_TRACE(producer_scope);
    ASSERT_EQ(warm_sources.size(), static_cast<size_t>(producer_scope));
    LUMICE_PathWorkspace* raw_workspace = nullptr;
    ASSERT_EQ(LUMICE_PathWorkspaceCreate(scene_.get(), &raw_workspace, err, sizeof(err)), LUMICE_OK);
    std::unique_ptr<LUMICE_PathWorkspace, decltype(&LUMICE_PathWorkspaceDestroy)> workspace(
        raw_workspace, LUMICE_PathWorkspaceDestroy);
    LUMICE_PathSurveyRequest survey{};
    survey.struct_size = sizeof(survey);
    survey.crystal_id = 1;
    survey.faces = faces;
    survey.face_count = 2;
    survey.layer_face_counts = counts;
    survey.layer_count = 1;
    survey.symmetry_bits_plus_one = producer_scope;
    LUMICE_PathSurvey* source = nullptr;
    ASSERT_EQ(LUMICE_PathWorkspaceQuerySurvey(workspace.get(), &survey, nullptr, &source, err, sizeof(err)), LUMICE_OK)
        << err;
    const auto source_doc = ResultDocument(source, LUMICE_PathSurveyToJson);
    EXPECT_EQ(source_doc["request"]["symmetry_bits"], producer_scope == 0 ? 7 : 0);
    ASSERT_EQ(source_doc["objects"].size(), 1u);
    EXPECT_EQ(source_doc["objects"][0]["kind"], "source_image");
    EXPECT_EQ(source_doc["evidence"]["state"], "certified");
    const auto& tint = source_doc["chromatic"]["weight_tint"];
    EXPECT_EQ(tint["evidence"]["state"], "finite");
    EXPECT_EQ(tint["sample_count"], 1000000);
    EXPECT_GT(tint["m_red"].get<double>(), 0.0);
    EXPECT_GT(tint["m_blue"].get<double>(), 0.0);
    survey_json[producer_scope] = source_doc.dump();
    LUMICE_PathWarmData* raw_warm = nullptr;
    ASSERT_EQ(LUMICE_PathWarmDataFromSurvey(source, &raw_warm), LUMICE_OK);
    LUMICE_PathSurveyDestroy(source);
    std::unique_ptr<LUMICE_PathWarmData, decltype(&LUMICE_PathWarmDataDestroy)> warm(raw_warm,
                                                                                     LUMICE_PathWarmDataDestroy);
    const auto check_consumer = [&](int consumer_scope) {
      SCOPED_TRACE(consumer_scope);
      // The physical automatic consumer also proves that old complete extents ignore the
      // poisoned tail and default to physical scope: it hits only the physical producer.
      point.struct_size = consumer_scope == 0 ? kOldPointExtent : sizeof(point);
      probe.struct_size = consumer_scope == 0 ? kOldProbeExtent : sizeof(probe);
      point.context_symmetry_bits_plus_one = consumer_scope == 0 ? 99 : consumer_scope;
      probe.context_symmetry_bits_plus_one = consumer_scope == 0 ? 99 : consumer_scope;
      options.warm = nullptr;
      LUMICE_SinglePathResult* point_result = nullptr;
      ASSERT_EQ(LUMICE_PathWorkspaceQueryPoint(workspace.get(), &point, &options, &point_result, err, sizeof(err)),
                LUMICE_OK)
          << err;
      const auto point_doc = ResultDocument(point_result, LUMICE_SinglePathResultToJson);
      EXPECT_EQ(point_doc["survey_context"]["source"], producer_scope == consumer_scope ? "warm" : "inline");
      LUMICE_SinglePathResultDestroy(point_result);
      LUMICE_PathProbe* probe_result = nullptr;
      ASSERT_EQ(LUMICE_PathWorkspaceQueryProbe(workspace.get(), &probe, &options, &probe_result, err, sizeof(err)),
                LUMICE_OK)
          << err;
      const auto probe_doc = ResultDocument(probe_result, LUMICE_PathProbeToJson);
      EXPECT_EQ(probe_doc["context"]["source"], producer_scope == consumer_scope ? "warm" : "inline");
      LUMICE_PathProbeDestroy(probe_result);
      point.struct_size = sizeof(point);
      probe.struct_size = sizeof(probe);
      point.context_symmetry_bits_plus_one = consumer_scope;
      probe.context_symmetry_bits_plus_one = consumer_scope;
      options.warm = warm.get();
      const auto expected = producer_scope == consumer_scope ? LUMICE_OK : LUMICE_ERR_INVALID_VALUE;
      EXPECT_EQ(LUMICE_PathWorkspaceQueryPoint(workspace.get(), &point, &options, &point_result, err, sizeof(err)),
                expected)
          << err;
      if (expected == LUMICE_OK) {
        EXPECT_EQ(ResultDocument(point_result, LUMICE_SinglePathResultToJson)["survey_context"]["source"], "warm");
      } else {
        EXPECT_EQ(point_result, nullptr);
        EXPECT_NE(std::string(err).find("config fingerprint gate"), std::string::npos);
      }
      LUMICE_SinglePathResultDestroy(point_result);
      EXPECT_EQ(LUMICE_PathWorkspaceQueryProbe(workspace.get(), &probe, &options, &probe_result, err, sizeof(err)),
                expected)
          << err;
      if (expected == LUMICE_OK) {
        EXPECT_EQ(ResultDocument(probe_result, LUMICE_PathProbeToJson)["context"]["source"], "warm");
      } else {
        EXPECT_EQ(probe_result, nullptr);
        EXPECT_NE(std::string(err).find("config fingerprint gate"), std::string::npos);
      }
      LUMICE_PathProbeDestroy(probe_result);
    };
    for (int consumer_scope : { 0, 1 }) {
      check_consumer(consumer_scope);
    }
    if (producer_scope == 1) {
      // A compatible automatic literal slot must not hide incompatible explicit physical input.
      options.warm = warm_sources.front().get();
      LUMICE_SinglePathResult* point_result = nullptr;
      LUMICE_PathProbe* probe_result = nullptr;
      EXPECT_EQ(LUMICE_PathWorkspaceQueryPoint(workspace.get(), &point, &options, &point_result, err, sizeof(err)),
                LUMICE_ERR_INVALID_VALUE);
      EXPECT_EQ(point_result, nullptr);
      EXPECT_NE(std::string(err).find("config fingerprint gate"), std::string::npos);
      LUMICE_SinglePathResultDestroy(point_result);
      EXPECT_EQ(LUMICE_PathWorkspaceQueryProbe(workspace.get(), &probe, &options, &probe_result, err, sizeof(err)),
                LUMICE_ERR_INVALID_VALUE);
      EXPECT_EQ(probe_result, nullptr);
      EXPECT_NE(std::string(err).find("config fingerprint gate"), std::string::npos);
      LUMICE_PathProbeDestroy(probe_result);
    }
    warm_sources.push_back(std::move(warm));
  };
  for (int producer_scope : { 0, 1 }) {
    check_producer(producer_scope);
  }
  ASSERT_EQ(warm_sources.size(), 2u);

  // ABI boundaries are independent of the producer matrix. All other arguments are valid.
  // Legacy old extents reuse the real physical source. Intermediate prefixes deliberately
  // carry the literal source: reaching the fingerprint gate proves the poison was ignored AND
  // the default was physical, without running an otherwise unconsumed numerical solve.
  LUMICE_PathWorkspace* raw_workspace = nullptr;
  ASSERT_EQ(LUMICE_PathWorkspaceCreate(scene_.get(), &raw_workspace, err, sizeof(err)), LUMICE_OK);
  std::unique_ptr<LUMICE_PathWorkspace, decltype(&LUMICE_PathWorkspaceDestroy)> workspace(raw_workspace,
                                                                                          LUMICE_PathWorkspaceDestroy);
  options.warm = nullptr;
  point.context_symmetry_bits_plus_one = probe.context_symmetry_bits_plus_one = 99;
  const auto check_extent = [&](size_t point_extent, size_t probe_extent, bool old_complete) {
    SCOPED_TRACE(point_extent);
    point.struct_size = point_extent;
    probe.struct_size = probe_extent;
    const auto& json = survey_json[old_complete ? 0 : 1];
    point.warm_json = probe.warm_json = json.data();
    point.warm_json_len = probe.warm_json_len = json.size();
    LUMICE_SinglePathResult* point_result = nullptr;
    LUMICE_PathProbe* probe_result = nullptr;
    const auto expected = old_complete ? LUMICE_OK : LUMICE_ERR_INVALID_VALUE;
    ASSERT_EQ(LUMICE_AnalyzeSinglePath(scene_.get(), &point, &point_result, err, sizeof(err)), expected) << err;
    if (old_complete) {
      EXPECT_EQ(ResultDocument(point_result, LUMICE_SinglePathResultToJson)["survey_context"]["source"], "warm");
    } else {
      EXPECT_EQ(point_result, nullptr);
      EXPECT_NE(std::string(err).find("config fingerprint gate"), std::string::npos) << "point err: " << err;
    }
    LUMICE_SinglePathResultDestroy(point_result);
    ASSERT_EQ(LUMICE_AnalyzePathProbe(scene_.get(), &probe, &probe_result, err, sizeof(err)), expected) << err;
    if (old_complete) {
      EXPECT_EQ(ResultDocument(probe_result, LUMICE_PathProbeToJson)["context"]["source"], "warm");
    } else {
      EXPECT_EQ(probe_result, nullptr);
      EXPECT_NE(std::string(err).find("config fingerprint gate"), std::string::npos) << "probe err: " << err;
    }
    LUMICE_PathProbeDestroy(probe_result);
    point.warm_json = probe.warm_json = nullptr;
    point.warm_json_len = probe.warm_json_len = 0;
    if (!old_complete) {
      EXPECT_EQ(LUMICE_PathWorkspaceQueryPoint(workspace.get(), &point, &options, &point_result, err, sizeof(err)),
                LUMICE_ERR_INVALID_VALUE);
      EXPECT_EQ(point_result, nullptr);
      EXPECT_NE(std::string(err).find("published complete extent"), std::string::npos);
      LUMICE_SinglePathResultDestroy(point_result);
      EXPECT_EQ(LUMICE_PathWorkspaceQueryProbe(workspace.get(), &probe, &options, &probe_result, err, sizeof(err)),
                LUMICE_ERR_INVALID_VALUE);
      EXPECT_EQ(probe_result, nullptr);
      EXPECT_NE(std::string(err).find("published complete extent"), std::string::npos);
      LUMICE_PathProbeDestroy(probe_result);
    }
  };
  check_extent(kOldPointExtent, kOldProbeExtent, true);
  check_extent(kOldPointExtent + 1, kOldProbeExtent + 1, false);
  // The "intermediate" anchor is one byte below the context field's published complete extent
  // (the bridge rounds a field end up to the scalar alignment), not sizeof-1: a later pure
  // append (v4.63) grew the structs past the v4.62 complete extent, so sizeof-1 now lands
  // ABOVE the context gate, where the poison below is legitimately validated and refused. One
  // byte below the gate the poison stays genuinely unread, while both floors (point v4.50,
  // probe v4.61) are cleared and the extent is still not a published complete one — the
  // workspace refusal and the legacy warm path are both exercised exactly as before.
  constexpr auto complete_prefix = [](size_t field_end) {
    return (field_end + 7) / 8 * 8;  // the scalar-alignment rounding the bridge's gates apply
  };
  check_extent(complete_prefix(offsetof(LUMICE_SinglePathRequest, context_symmetry_bits_plus_one) + sizeof(int)) - 1,
               complete_prefix(offsetof(LUMICE_PathProbeRequest, context_symmetry_bits_plus_one) + sizeof(int)) - 1,
               false);

  // A complete extent must actually validate the new field, in both entry families.
  point.struct_size = sizeof(point);
  probe.struct_size = sizeof(probe);
  LUMICE_SinglePathResult* point_result = nullptr;
  LUMICE_PathProbe* probe_result = nullptr;
  EXPECT_EQ(LUMICE_PathWorkspaceQueryPoint(workspace.get(), &point, &options, &point_result, err, sizeof(err)),
            LUMICE_ERR_INVALID_VALUE);
  EXPECT_EQ(point_result, nullptr);
  EXPECT_STREQ(err, "invalid_argument: context_symmetry_bits_plus_one must be 0 or 1");
  LUMICE_SinglePathResultDestroy(point_result);
  EXPECT_EQ(LUMICE_AnalyzeSinglePath(scene_.get(), &point, &point_result, err, sizeof(err)), LUMICE_ERR_INVALID_VALUE);
  EXPECT_EQ(point_result, nullptr);
  EXPECT_STREQ(err, "invalid_argument: context_symmetry_bits_plus_one must be 0 or 1");
  LUMICE_SinglePathResultDestroy(point_result);
  EXPECT_EQ(LUMICE_PathWorkspaceQueryProbe(workspace.get(), &probe, &options, &probe_result, err, sizeof(err)),
            LUMICE_ERR_INVALID_VALUE);
  EXPECT_EQ(probe_result, nullptr);
  EXPECT_STREQ(err, "invalid_argument: context_symmetry_bits_plus_one must be 0 or 1");
  LUMICE_PathProbeDestroy(probe_result);
  EXPECT_EQ(LUMICE_AnalyzePathProbe(scene_.get(), &probe, &probe_result, err, sizeof(err)), LUMICE_ERR_INVALID_VALUE);
  EXPECT_EQ(probe_result, nullptr);
  EXPECT_STREQ(err, "invalid_argument: context_symmetry_bits_plus_one must be 0 or 1");
  LUMICE_PathProbeDestroy(probe_result);
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

// The ordered-curve readers' round trip (task arc-recovery-engine): a column population's
// survey produces family_curves; the five readers read back the same identity, point order,
// segment forms and events the document declares — every count consistent, every index in
// range, the point-image rows carrying their constant-image readings, the borrowed member
// array surviving a ToJson — and the JSON projection's family_curves table matches the
// reader's count. A too-small row struct is refused like every other reader's.
TEST_F(SurveyCApi, CurveReadersRoundTripTheColumnDocument) {
  constexpr const char* kColumnJson = R"({
    "crystal": [ { "id": 1, "type": "prism",
                   "shape": { "height": 0.3, "face_distance": [1,1,1,1,1,1] },
                   "axis": { "zenith": { "type": "gauss", "mean": 90, "std": 0.3 },
                             "azimuth": { "type": "uniform", "mean": 0, "std": 360 },
                             "roll": { "type": "uniform", "mean": 0, "std": 360 } } } ],
    "filter": [],
    "scene": { "light_source": { "type": "sun", "altitude": 20, "azimuth": 0, "diameter": 0.5, "spectrum": "D65" },
               "ray_num": 1000, "max_hits": 7,
               "scattering": [ { "prob": 0, "entries": [ { "crystal": 1, "proportion": 1 } ] } ] },
    "render": []
  })";
  LUMICE_Scene* raw_scene = nullptr;
  ASSERT_EQ(LUMICE_SceneFromJson(kColumnJson, &raw_scene), LUMICE_OK) << "the column scene must parse";
  std::unique_ptr<LUMICE_Scene, SceneDeleter> column_scene(raw_scene);
  static int faces[] = { 3, 5 };
  static int counts[] = { 2 };
  LUMICE_PathSurveyRequest request{};
  request.struct_size = sizeof(request);
  request.crystal_id = 1;
  request.faces = faces;
  request.face_count = 2;
  request.layer_face_counts = counts;
  request.layer_count = 1;
  request.wavelength_nm = 550;
  // The literal requested sequence: one member's ladder (the physical P|B|D class would run
  // eight members' scans for the same curve shape — the round trip needs one real document,
  // not eight).
  request.symmetry_bits_plus_one = 1;
  LUMICE_PathSurvey* raw_result = nullptr;
  char err[512] = {};
  ASSERT_EQ(LUMICE_AnalyzePathSurvey(column_scene.get(), &request, &raw_result, err, sizeof(err)), LUMICE_OK) << err;
  std::unique_ptr<LUMICE_PathSurvey, void (*)(LUMICE_PathSurvey*)> result(raw_result, LUMICE_PathSurveyDestroy);

  LUMICE_PathSurveyCurveInfo info{};
  info.struct_size = sizeof(info);
  ASSERT_EQ(LUMICE_PathSurveyGetCurves(result.get(), &info), LUMICE_PATH_READ_OK);
  ASSERT_GT(info.curve_count, 0u) << "the column population produced no ordered curves";
  // A too-small info struct is refused.
  LUMICE_PathSurveyCurveInfo small{};
  small.struct_size = 4;
  EXPECT_EQ(LUMICE_PathSurveyGetCurves(result.get(), &small), LUMICE_PATH_READ_INVALID_SIZE);

  int point_image_rows = 0;
  size_t first_curve_member_count = 0;
  int edge_dim_of_first = -2;
  // One curve's full read, the per-row asserts inside a lambda the loop calls (a fatal assert
  // in the loop body itself would silently skip every row after the first failure).
  const auto read_curve = [&](size_t c) {
    LUMICE_PathSurveyCurve curve{};
    curve.struct_size = sizeof(curve);
    ASSERT_EQ(LUMICE_PathSurveyGetCurve(result.get(), c, &curve), LUMICE_PATH_READ_OK);
    ASSERT_GT(curve.member_count, 0u);
    ASSERT_NE(curve.member, nullptr);
    if (c == 0) {
      first_curve_member_count = curve.member_count;
      LUMICE_PathSurveyCurvePoint p0{};
      p0.struct_size = sizeof(p0);
      if (LUMICE_PathSurveyGetCurvePoint(result.get(), 0, 0, &p0) == LUMICE_PATH_READ_OK) {
        edge_dim_of_first = p0.edge_dim;
      }
    }
    EXPECT_TRUE(curve.kind == LUMICE_PATH_CURVE_KIND_FOLD_CHAIN || curve.kind == LUMICE_PATH_CURVE_KIND_REGULAR_TRACK);
    EXPECT_TRUE(curve.end_a == LUMICE_PATH_CURVE_END_OPEN || curve.end_a == LUMICE_PATH_CURVE_END_CLOSED_LOOP ||
                curve.end_a == LUMICE_PATH_CURVE_END_FEASIBILITY_INTERRUPTION ||
                curve.end_a == LUMICE_PATH_CURVE_END_AMBIGUOUS_CELL ||
                curve.end_a == LUMICE_PATH_CURVE_END_STOP_REQUESTED);
    EXPECT_TRUE(curve.point_count > 0u);
    const auto read_point = [&](size_t p) {
      LUMICE_PathSurveyCurvePoint point{};
      point.struct_size = sizeof(point);
      ASSERT_EQ(LUMICE_PathSurveyGetCurvePoint(result.get(), c, p, &point), LUMICE_PATH_READ_OK);
      EXPECT_TRUE(std::isfinite(point.params_deg[0]) && std::isfinite(point.params_deg[1]));
      EXPECT_TRUE(std::isfinite(point.image_altitude_deg) && std::isfinite(point.image_azimuth_deg));
    };
    for (size_t p = 0; p < curve.point_count; ++p) {
      read_point(p);
    }
    const auto read_segment = [&](size_t s) {
      LUMICE_PathSurveyCurveSegment segment{};
      segment.struct_size = sizeof(segment);
      ASSERT_EQ(LUMICE_PathSurveyGetCurveSegment(result.get(), c, s, &segment), LUMICE_PATH_READ_OK);
      EXPECT_GE(segment.first, 0);
      EXPECT_GE(segment.last, segment.first);
      EXPECT_LT(static_cast<size_t>(segment.last), curve.point_count);
      EXPECT_TRUE(segment.form == LUMICE_PATH_CURVE_SEGMENT_FORM_ARC ||
                  segment.form == LUMICE_PATH_CURVE_SEGMENT_FORM_POINT_IMAGE ||
                  segment.form == LUMICE_PATH_CURVE_SEGMENT_FORM_UNKNOWN);
      if (segment.form == LUMICE_PATH_CURVE_SEGMENT_FORM_POINT_IMAGE) {
        EXPECT_EQ(segment.image_available, 1);
        EXPECT_GE(segment.image_diameter_deg, 0.0);
        point_image_rows++;
      }
    };
    for (size_t s = 0; s < curve.segment_count; ++s) {
      read_segment(s);
    }
    const auto read_cusp = [&](size_t e) {
      LUMICE_PathSurveyCurveCusp cusp{};
      cusp.struct_size = sizeof(cusp);
      ASSERT_EQ(LUMICE_PathSurveyGetCurveCusp(result.get(), c, e, &cusp), LUMICE_PATH_READ_OK);
      EXPECT_GE(cusp.order_index, 0);
      EXPECT_LT(static_cast<size_t>(cusp.order_index), curve.point_count);
      EXPECT_GE(cusp.djv_residual, 0.0);
    };
    for (size_t e = 0; e < curve.cusp_count; ++e) {
      read_cusp(e);
    }
  };
  for (size_t c = 0; c < info.curve_count; ++c) {
    read_curve(c);
  }
  EXPECT_GT(point_image_rows, 0) << "the column's mirror collapse never reads as a point-image run";
  // The evidence-fidelity contract (the review's Major: the adjacency source must survive the
  // freeze): every fold chain's link count matches its declared topology, a link row reads
  // back with a legal cell corner pair, the points carry their grid-edge sources, and the
  // JSON projection carries the same link table and edge fields.
  {
    LUMICE_PathSurveyCurve first{};
    first.struct_size = sizeof(first);
    ASSERT_EQ(LUMICE_PathSurveyGetCurve(result.get(), 0, &first), LUMICE_PATH_READ_OK);
    if (first.kind == LUMICE_PATH_CURVE_KIND_FOLD_CHAIN) {
      const size_t expected_links = first.point_count - (first.closed != 0 ? 0u : 1u);
      EXPECT_EQ(first.link_count, expected_links);
      LUMICE_PathSurveyCurveLink link{};
      link.struct_size = sizeof(link);
      ASSERT_EQ(LUMICE_PathSurveyGetCurveLink(result.get(), 0, 0, &link), LUMICE_PATH_READ_OK);
      EXPECT_GE(link.cell_i0, 0);
      EXPECT_GE(link.cell_i1, 0);
      LUMICE_PathSurveyCurvePoint edge_point{};
      edge_point.struct_size = sizeof(edge_point);
      ASSERT_EQ(LUMICE_PathSurveyGetCurvePoint(result.get(), 0, 0, &edge_point), LUMICE_PATH_READ_OK);
      EXPECT_GE(edge_point.edge_dim, -1);
      EXPECT_LE(edge_point.edge_dim, 1);
    } else {
      EXPECT_EQ(first.link_count, 0u) << "a track carries no fold-chain links";
      LUMICE_PathSurveyCurveLink no_link{};
      no_link.struct_size = sizeof(no_link);
      EXPECT_EQ(LUMICE_PathSurveyGetCurveLink(result.get(), 0, 0, &no_link), LUMICE_PATH_READ_INVALID_INDEX);
    }
    const auto document = ResultDocument(result.get(), LUMICE_PathSurveyToJson);
    ASSERT_TRUE(document.contains("family_curves"));
    const auto& curve_json = document["family_curves"][0];
    ASSERT_TRUE(curve_json.contains("links"));
    EXPECT_EQ(curve_json["links"].size(), first.link_count);
    ASSERT_TRUE(curve_json["points"][0].contains("edge_dim"));
    EXPECT_EQ(curve_json["points"][0]["edge_dim"].get<int>(), edge_dim_of_first);
  }
  // The borrowed member array survives a derived JSON (the same borrow contract every other
  // reader declares).
  (void)ResultDocument(result.get(), LUMICE_PathSurveyToJson);
  LUMICE_PathSurveyCurve after{};
  after.struct_size = sizeof(after);
  ASSERT_EQ(LUMICE_PathSurveyGetCurve(result.get(), 0, &after), LUMICE_PATH_READ_OK);
  ASSERT_NE(after.member, nullptr);
  EXPECT_EQ(after.member_count, first_curve_member_count);
}

}  // namespace
}  // namespace lumice::raypath::survey
