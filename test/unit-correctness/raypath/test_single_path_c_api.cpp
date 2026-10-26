// lumice_raypath.h's single-path entry point (src/server/c_api_raypath.cpp): argument checks, the
// struct_size gate, the request -> module mapping (layers, unset wavelength, default sample count,
// warm JSON), error reporting through err_buf, and the snprintf-style buffer contract of
// LUMICE_SinglePathResultToJson. The analysis itself is test_single_path_analysis.cpp's subject and
// the JSON's shape test_single_path_json.cpp's; here only what the bridge adds.
//
// symmetry_semantics: none — every case analyses one concrete face sequence (doc/interfaces/analytic-api.md
// section 3).

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "config/config_manager.hpp"
#include "include/lumice_base.h"
#include "include/lumice_raypath.h"
#include "include/lumice_scene.h"
#include "raypath/detail/survey/config_fingerprint.hpp"
#include "raypath/single_path_json.hpp"
#include "raypath/survey_reuse.hpp"
#include "server/c_api_scene_internal.hpp"  // SceneRoot (test-only exposure)
#include "server/server.hpp"                // ParseConfigManager
#include "util/logger.hpp"

namespace {

// A plain prism under a 20-degree sun; the D65 spectrum leaves the wavelength to the default.
constexpr const char* kScene = R"({
  "crystal": [ { "id": 1, "type": "prism", "shape": { "height": 1.0 } } ],
  "filter": [],
  "scene": {
    "light_source": { "type": "sun", "altitude": 20.0, "spectrum": "D65" },
    "ray_num": 1000,
    "max_hits": 7,
    "scattering": [ { "prob": 0.0, "entries": [ { "crystal": 1, "proportion": 1 } ] } ]
  },
  "render": []
})";

struct SceneDeleter {
  void operator()(LUMICE_Scene* s) const { LUMICE_SceneDestroy(s); }
};
struct ResultDeleter {
  void operator()(LUMICE_SinglePathResult* r) const { LUMICE_SinglePathResultDestroy(r); }
};
using ScenePtr = std::unique_ptr<LUMICE_Scene, SceneDeleter>;
using ResultPtr = std::unique_ptr<LUMICE_SinglePathResult, ResultDeleter>;

ScenePtr MakeScene(const char* json = kScene) {
  LUMICE_Scene* s = nullptr;
  EXPECT_EQ(LUMICE_SceneFromJson(json, &s), LUMICE_OK);
  return ScenePtr(s);
}

// Owns the arrays a LUMICE_SinglePathRequest points into.
struct Req {
  std::vector<int> faces;
  std::vector<int> layers;
  std::string warm;
  LUMICE_SinglePathRequest c{};

  Req(std::vector<int> f, std::vector<int> layer_counts, double alt = 20.0, double az = 25.0)
      : faces(std::move(f)), layers(std::move(layer_counts)) {
    c.struct_size = sizeof(LUMICE_SinglePathRequest);
    c.crystal_id = 1;
    c.target_altitude_deg = alt;
    c.target_azimuth_deg = az;
    c.sample_count = 50000;
    c.sun_grid_lat_count = 4;
    Bind();
  }
  void Bind() {
    c.faces = faces.data();
    c.face_count = static_cast<int>(faces.size());
    c.layer_face_counts = layers.data();
    c.layer_count = static_cast<int>(layers.size());
    c.warm_json = warm.empty() ? nullptr : warm.data();
    c.warm_json_len = warm.size();
  }
};

struct Outcome {
  LUMICE_ErrorCode code;
  ResultPtr result;
  std::string error;
};

Outcome Analyse(const LUMICE_Scene* scene, const Req& req) {
  LUMICE_SinglePathResult* raw = reinterpret_cast<LUMICE_SinglePathResult*>(&raw);  // must be overwritten
  char err[512];
  std::memset(err, 'x', sizeof(err));
  const LUMICE_ErrorCode code = LUMICE_AnalyzeSinglePath(scene, &req.c, &raw, err, sizeof(err));
  return { code, ResultPtr(raw), std::string(err) };
}

std::string Json(const LUMICE_SinglePathResult* r) {
  size_t len = 0;
  EXPECT_EQ(LUMICE_SinglePathResultToJson(r, nullptr, 0, &len), LUMICE_OK);
  std::string out(len + 1, '\0');
  size_t len2 = 0;
  EXPECT_EQ(LUMICE_SinglePathResultToJson(r, out.data(), out.size(), &len2), LUMICE_OK);
  EXPECT_EQ(len, len2);
  out.resize(len);
  return out;
}

TEST(SinglePathCApi, TypedReadersUseResultOwnedStorageAndBoundedExtents) {
  const ScenePtr scene = MakeScene();
  Req request({ 3, 5 }, { 2 });
  request.c.sample_count = 100;
  const Outcome outcome = Analyse(scene.get(), request);
  ASSERT_EQ(outcome.code, LUMICE_OK) << outcome.error;
  ASSERT_NE(outcome.result, nullptr);
  const auto json = nlohmann::json::parse(Json(outcome.result.get()));

  LUMICE_PathPointInfo info{};
  info.struct_size = sizeof(info);
  ASSERT_EQ(LUMICE_PathPointGetInfo(outcome.result.get(), &info), LUMICE_PATH_READ_OK);
  EXPECT_EQ(info.crystal_id, json.at("meta").at("crystal").at("id"));
  EXPECT_EQ(info.face_count, request.faces.size());
  EXPECT_EQ(info.component_count, json.at("components").size());
  EXPECT_EQ(info.execution, LUMICE_PATH_OUTCOME_COMPLETED);
  EXPECT_EQ(std::string(info.crystal_kind.data, info.crystal_kind.length), "prism");
  EXPECT_EQ(info.sun_diameter_deg, json.at("meta").at("sun").at("diameter_deg"));
  EXPECT_EQ(info.target_deviation_deg, json.at("meta").at("target").at("deviation_deg"));
  EXPECT_GT(info.band_half_width_rad, 0.0);
  EXPECT_GT(info.cluster_radius_rad, 0.0);
  LUMICE_PathShapeScalar shape{};
  shape.struct_size = sizeof(shape);
  ASSERT_EQ(LUMICE_PathPointGetShape(outcome.result.get(), 0, &shape), LUMICE_PATH_READ_OK);
  EXPECT_GT(shape.name.length, 0u);
  LUMICE_PathPointMass mass{};
  mass.struct_size = sizeof(mass);
  EXPECT_EQ(LUMICE_PathPointGetMass(outcome.result.get(), &mass), LUMICE_PATH_READ_INVALID_KIND);
  LUMICE_PathPointDiscovery discovery{};
  discovery.struct_size = sizeof(discovery);
  ASSERT_EQ(LUMICE_PathPointGetDiscovery(outcome.result.get(), &discovery), LUMICE_PATH_READ_OK);
  EXPECT_EQ(discovery.pool_count, json.at("discovery").at("pool_count"));
  LUMICE_PathPointReach reach{};
  reach.struct_size = sizeof(reach);
  ASSERT_EQ(LUMICE_PathPointGetReach(outcome.result.get(), &reach), LUMICE_PATH_READ_OK);
  EXPECT_GT(reach.probe_lat_count, 0);
  LUMICE_PathMeasureRegistration registration{};
  registration.struct_size = sizeof(registration);
  ASSERT_EQ(LUMICE_PathPointGetMeasureRegistration(outcome.result.get(), &registration), LUMICE_PATH_READ_OK);
  EXPECT_GT(registration.support_kind.length, 0u);
  LUMICE_PathContext context{};
  context.struct_size = sizeof(context);
  ASSERT_EQ(LUMICE_PathPointGetContext(outcome.result.get(), &context), LUMICE_PATH_READ_OK);
  EXPECT_NE(context.object_state, 0);
  LUMICE_PathIncomplete incomplete{};
  incomplete.struct_size = sizeof(incomplete);
  EXPECT_EQ(LUMICE_PathPointGetIncomplete(outcome.result.get(), info.incomplete_count, &incomplete),
            LUMICE_PATH_READ_INVALID_INDEX);

  LUMICE_PathSunGrid grid{};
  grid.struct_size = sizeof(grid);
  ASSERT_EQ(LUMICE_PathPointGetSunGrid(outcome.result.get(), &grid), LUMICE_PATH_READ_OK);
  EXPECT_EQ(grid.cell_count, static_cast<size_t>(grid.lat_count * grid.lon_count));
  if (grid.cell_count > 0) {
    EXPECT_NE(grid.valid, nullptr);
    EXPECT_NE(grid.entry_measure, nullptr);
  }
  LUMICE_PathPointComponent component{};
  component.struct_size = sizeof(component);
  if (info.component_count > 0) {
    ASSERT_EQ(LUMICE_PathPointGetComponent(outcome.result.get(), 0, &component), LUMICE_PATH_READ_OK);
    EXPECT_EQ(component.point_count, json.at("components")[0].at("points").size());
    if (component.point_count > 0) {
      LUMICE_PathPointSample sample{};
      sample.struct_size = sizeof(sample);
      ASSERT_EQ(LUMICE_PathPointGetSample(outcome.result.get(), 0, 0, &sample), LUMICE_PATH_READ_OK);
      EXPECT_EQ(sample.pose[0], json.at("components")[0].at("points")[0].at("pose")[0]);
      EXPECT_GT(sample.segment_direction_count, 0u);
    }
  }
  component.point_count = 123;
  EXPECT_EQ(LUMICE_PathPointGetComponent(outcome.result.get(), info.component_count, &component),
            LUMICE_PATH_READ_INVALID_INDEX);
  EXPECT_EQ(component.point_count, 0u);
  EXPECT_EQ(component.struct_size, sizeof(component));

  alignas(LUMICE_PathPointInfo) unsigned char future[sizeof(LUMICE_PathPointInfo) + 8];
  std::memset(future, 0x5a, sizeof(future));
  auto* extended = reinterpret_cast<LUMICE_PathPointInfo*>(future);
  extended->struct_size = sizeof(future);
  ASSERT_EQ(LUMICE_PathPointGetInfo(outcome.result.get(), extended), LUMICE_PATH_READ_OK);
  for (size_t i = sizeof(LUMICE_PathPointInfo); i < sizeof(future); ++i) {
    EXPECT_EQ(future[i], 0x5a);
  }
  std::memset(future, 0x5a, sizeof(future));
  extended->struct_size = sizeof(LUMICE_PathPointInfo) - 1;
  EXPECT_EQ(LUMICE_PathPointGetInfo(outcome.result.get(), extended), LUMICE_PATH_READ_INVALID_SIZE);
  for (size_t i = sizeof(size_t); i < sizeof(future); ++i) {
    EXPECT_EQ(future[i], 0x5a);
  }
}

TEST(SinglePathCApi, WorkspacePointWithoutContextDoesNotPublishContextView) {
  ScenePtr scene = MakeScene();
  LUMICE_PathWorkspace* raw_workspace = nullptr;
  ASSERT_EQ(LUMICE_PathWorkspaceCreate(scene.get(), &raw_workspace, nullptr, 0), LUMICE_OK);
  std::unique_ptr<LUMICE_PathWorkspace, decltype(&LUMICE_PathWorkspaceDestroy)> workspace(raw_workspace,
                                                                                          LUMICE_PathWorkspaceDestroy);
  scene.reset();

  Req request({ 3, 5 }, { 2 });
  request.c.sample_count = 100;
  LUMICE_PathQueryOptions options{};
  options.struct_size = sizeof(options);
  options.deadline_ms = 15000;
  options.include_context = 0;
  LUMICE_SinglePathResult* raw_result = nullptr;
  char error[256]{};
  ASSERT_EQ(LUMICE_PathWorkspaceQueryPoint(workspace.get(), &request.c, &options, &raw_result, error, sizeof(error)),
            LUMICE_OK)
      << error;
  ResultPtr result(raw_result);
  workspace.reset();

  LUMICE_PathContext context{};
  context.struct_size = sizeof(context);
  EXPECT_EQ(LUMICE_PathPointGetContext(result.get(), &context), LUMICE_PATH_READ_NOT_AVAILABLE);
  EXPECT_EQ(context.struct_size, sizeof(context));
  EXPECT_EQ(context.source, 0);
  EXPECT_EQ(context.object_state, 0);
  const auto document = nlohmann::json::parse(Json(result.get()));
  EXPECT_EQ(document.at("survey_context").at("object").at("reason"), "not_requested");
  EXPECT_EQ(document.at("survey_context").at("edge").at("reason"), "not_requested");
  EXPECT_EQ(document.at("survey_context").at("band_sum").at("reason"), "not_requested");
}

TEST(SinglePathCApi, CancelledSurveyHasTypedExecutionOutcome) {
  const ScenePtr scene = MakeScene();
  LUMICE_PathWorkspace* workspace = nullptr;
  ASSERT_EQ(LUMICE_PathWorkspaceCreate(scene.get(), &workspace, nullptr, 0), LUMICE_OK);
  LUMICE_PathCancelToken* token = nullptr;
  ASSERT_EQ(LUMICE_PathCancelTokenCreate(&token), LUMICE_OK);
  LUMICE_PathCancelTokenRequest(token);
  const int faces[] = { 3, 5 };
  const int counts[] = { 2 };
  LUMICE_PathSurveyRequest request{};
  request.struct_size = sizeof(request);
  request.crystal_id = 1;
  request.faces = faces;
  request.face_count = 2;
  request.layer_face_counts = counts;
  request.layer_count = 1;
  request.symmetry_bits_plus_one = 1;
  LUMICE_PathQueryOptions options{};
  options.struct_size = sizeof(options);
  options.deadline_ms = 15000;
  options.cancel = token;
  LUMICE_PathSurvey* result = nullptr;
  char error[256] = {};
  const auto code = LUMICE_PathWorkspaceQuerySurvey(workspace, &request, &options, &result, error, sizeof(error));
  LUMICE_PathCancelTokenDestroy(token);
  LUMICE_PathWorkspaceDestroy(workspace);
  ASSERT_EQ(code, LUMICE_OK) << error;
  LUMICE_PathSurveyInfo info{};
  info.struct_size = sizeof(info);
  ASSERT_EQ(LUMICE_PathSurveyGetInfo(result, &info), LUMICE_PATH_READ_OK);
  EXPECT_EQ(info.execution, LUMICE_PATH_OUTCOME_CANCELLED);
  LUMICE_PathSurveyDestroy(result);
}

TEST(SinglePathCApi, SurveyAndProbeTypedReadersRespectClaimStates) {
  const ScenePtr scene = MakeScene();
  const int faces[] = { 3, 5 };
  const int counts[] = { 2 };
  LUMICE_PathSurveyRequest survey{};
  survey.struct_size = sizeof(survey);
  survey.crystal_id = 1;
  survey.faces = faces;
  survey.face_count = 2;
  survey.layer_face_counts = counts;
  survey.layer_count = 1;
  survey.symmetry_bits_plus_one = 1;
  survey.wavelength_nm = 550;
  survey.mc_ess_draws = 32;
  LUMICE_PathSurvey* raw_survey = nullptr;
  char err[256] = {};
  ASSERT_EQ(LUMICE_AnalyzePathSurvey(scene.get(), &survey, &raw_survey, err, sizeof(err)), LUMICE_OK) << err;
  std::unique_ptr<LUMICE_PathSurvey, void (*)(LUMICE_PathSurvey*)> survey_result(raw_survey, LUMICE_PathSurveyDestroy);
  LUMICE_PathSurveyInfo info{};
  info.struct_size = sizeof(info);
  ASSERT_EQ(LUMICE_PathSurveyGetInfo(survey_result.get(), &info), LUMICE_PATH_READ_OK);
  EXPECT_EQ(info.schema_version, 4);
  EXPECT_EQ(info.crystal_id, 1);
  EXPECT_EQ(info.execution, LUMICE_PATH_OUTCOME_COMPLETED);
  EXPECT_GT(info.object_count, 0u);
  LUMICE_PathSurveyEvidence evidence{};
  evidence.struct_size = sizeof(evidence);
  ASSERT_EQ(LUMICE_PathSurveyGetEvidence(survey_result.get(), LUMICE_PATH_EVIDENCE_DOCUMENT, 0, &evidence),
            LUMICE_PATH_READ_OK);
  EXPECT_EQ(evidence.state, info.evidence_state);
  EXPECT_EQ(LUMICE_PathSurveyGetEvidence(survey_result.get(), 0, 0, &evidence), LUMICE_PATH_READ_INVALID_KIND);
  EXPECT_EQ(evidence.state, 0);
  ASSERT_EQ(LUMICE_PathSurveyGetEvidence(survey_result.get(), LUMICE_PATH_EVIDENCE_OBJECT, 0, &evidence),
            LUMICE_PATH_READ_OK);
  if (info.family_available) {
    EXPECT_EQ(LUMICE_PathSurveyGetEvidence(survey_result.get(), LUMICE_PATH_EVIDENCE_FAMILY, 0, &evidence),
              LUMICE_PATH_READ_OK);
  }
  if (info.tint_available) {
    EXPECT_EQ(LUMICE_PathSurveyGetEvidence(survey_result.get(), LUMICE_PATH_EVIDENCE_TINT, 0, &evidence),
              LUMICE_PATH_READ_OK);
    LUMICE_PathQuantitySpec quantity{};
    quantity.struct_size = sizeof(quantity);
    EXPECT_EQ(LUMICE_PathSurveyGetTintQuantity(survey_result.get(), &quantity), LUMICE_PATH_READ_OK);
    EXPECT_EQ(quantity.kind, LUMICE_PATH_QUANTITY_TINT_RATIO);
  }
  LUMICE_PathSurveyScope scope{};
  scope.struct_size = sizeof(scope);
  ASSERT_EQ(LUMICE_PathSurveyGetScope(survey_result.get(), &scope), LUMICE_PATH_READ_OK);
  EXPECT_TRUE(scope.request_wavelength_given);
  EXPECT_EQ(scope.request_wavelength_nm, 550.0);
  EXPECT_GE(scope.geometry_kind, 1);
  ASSERT_EQ(scope.request_layer_count, 1u);
  LUMICE_PathFaceSequence request_layer{};
  request_layer.struct_size = sizeof(request_layer);
  ASSERT_EQ(LUMICE_PathSurveyGetRequestLayer(survey_result.get(), 0, &request_layer), LUMICE_PATH_READ_OK);
  ASSERT_EQ(request_layer.face_count, 2u);
  EXPECT_EQ(request_layer.faces[0], 3);
  if (info.member_count > 0) {
    LUMICE_PathFaceSequence member{};
    member.struct_size = sizeof(member);
    EXPECT_EQ(LUMICE_PathSurveyGetMember(survey_result.get(), 0, &member), LUMICE_PATH_READ_OK);
  }
  if (scope.routing_member_count > 0) {
    LUMICE_PathSurveyMemberVerdict verdict{};
    verdict.struct_size = sizeof(verdict);
    EXPECT_EQ(LUMICE_PathSurveyGetMemberVerdict(survey_result.get(), 0, &verdict), LUMICE_PATH_READ_OK);
    EXPECT_NE(verdict.evidence_state, 0);
  }
  if (info.unevaluated_count > 0) {
    LUMICE_PathSurveyUnevaluated unevaluated{};
    unevaluated.struct_size = sizeof(unevaluated);
    ASSERT_EQ(LUMICE_PathSurveyGetUnevaluated(survey_result.get(), 0, &unevaluated), LUMICE_PATH_READ_OK);
    if (unevaluated.kind_count > 0) {
      int kind = 0;
      EXPECT_EQ(LUMICE_PathSurveyGetUnevaluatedKind(survey_result.get(), 0, 0, &kind), LUMICE_PATH_READ_OK);
      EXPECT_NE(kind, 0);
    }
  }
  LUMICE_PathSurveyObject object{};
  object.struct_size = sizeof(object);
  ASSERT_EQ(LUMICE_PathSurveyGetObject(survey_result.get(), 0, &object), LUMICE_PATH_READ_OK);
  EXPECT_GE(object.kind, LUMICE_PATH_SURVEY_SOURCE_IMAGE);
  EXPECT_LE(object.kind, LUMICE_PATH_SURVEY_REGULAR_IMAGE);
  EXPECT_NE(object.state, 0);
  EXPECT_EQ(LUMICE_PathSurveyGetObject(survey_result.get(), info.object_count, &object),
            LUMICE_PATH_READ_INVALID_INDEX);
  EXPECT_EQ(object.kind, 0);

  LUMICE_PathFamilyInfo family{};
  family.struct_size = sizeof(family);
  const auto family_status = LUMICE_PathSurveyGetFamily(survey_result.get(), &family);
  EXPECT_EQ(family_status, info.family_available ? LUMICE_PATH_READ_OK : LUMICE_PATH_READ_NOT_AVAILABLE);
  if (family_status == LUMICE_PATH_READ_OK) {
    LUMICE_PathFamilyDimension dim{};
    dim.struct_size = sizeof(dim);
    ASSERT_EQ(LUMICE_PathSurveyGetFamilyDimension(survey_result.get(), 0, &dim), LUMICE_PATH_READ_OK);
    EXPECT_GT(dim.kind.length, 0u);
    EXPECT_EQ(LUMICE_PathSurveyGetFamilyDimension(survey_result.get(), 3, &dim), LUMICE_PATH_READ_INVALID_INDEX);
  }
  if (family_status == LUMICE_PATH_READ_OK && family.ladder_count > 0) {
    LUMICE_PathFamilyLevel level{};
    level.struct_size = sizeof(level);
    ASSERT_EQ(LUMICE_PathSurveyGetFamilyLevel(survey_result.get(), 0, &level), LUMICE_PATH_READ_OK);
    if (level.frozen_dim_count > 0) {
      LUMICE_PathFamilyFrozenDim frozen{};
      frozen.struct_size = sizeof(frozen);
      EXPECT_EQ(LUMICE_PathSurveyGetFamilyFrozenDim(survey_result.get(), 0, 0, &frozen), LUMICE_PATH_READ_OK);
    }
    if (level.free_dim_count > 0) {
      LUMICE_PathText free_dim{};
      EXPECT_EQ(LUMICE_PathSurveyGetFamilyFreeDim(survey_result.get(), 0, 0, &free_dim), LUMICE_PATH_READ_OK);
      EXPECT_GT(free_dim.length, 0u);
    }
    if (level.killed_count > 0) {
      LUMICE_PathFamilyKilled killed{};
      killed.struct_size = sizeof(killed);
      ASSERT_EQ(LUMICE_PathSurveyGetFamilyKilled(survey_result.get(), 0, 0, &killed), LUMICE_PATH_READ_OK);
      if (killed.verdict_count > 0) {
        LUMICE_PathBandVerdict verdict{};
        verdict.struct_size = sizeof(verdict);
        EXPECT_EQ(LUMICE_PathSurveyGetFamilyKilledVerdict(survey_result.get(), 0, 0, 0, &verdict), LUMICE_PATH_READ_OK);
      }
    }
  }
  LUMICE_PathTint tint{};
  tint.struct_size = sizeof(tint);
  EXPECT_EQ(LUMICE_PathSurveyGetTint(survey_result.get(), &tint),
            info.tint_available ? LUMICE_PATH_READ_OK : LUMICE_PATH_READ_NOT_AVAILABLE);
  if (tint.reading_available) {
    EXPECT_GT(tint.sample_count, 0);
  }
  LUMICE_PathTirShare tir{};
  tir.struct_size = sizeof(tir);
  EXPECT_EQ(LUMICE_PathSurveyGetTirShare(survey_result.get(), &tir),
            info.tir_available ? LUMICE_PATH_READ_OK : LUMICE_PATH_READ_NOT_AVAILABLE);
  if (tir.interface_count > 0) {
    LUMICE_PathTirInterface row{};
    row.struct_size = sizeof(row);
    EXPECT_EQ(LUMICE_PathSurveyGetTirInterface(survey_result.get(), 0, &row), LUMICE_PATH_READ_OK);
    EXPECT_GT(row.face, 0);
  }
  LUMICE_PathMcEss ess{};
  ess.struct_size = sizeof(ess);
  EXPECT_EQ(LUMICE_PathSurveyGetMcEss(survey_result.get(), &ess),
            info.ess_available ? LUMICE_PATH_READ_OK : LUMICE_PATH_READ_NOT_AVAILABLE);
  if (ess.band_count > 0) {
    LUMICE_PathMcEssBand band{};
    band.struct_size = sizeof(band);
    EXPECT_EQ(LUMICE_PathSurveyGetMcEssBand(survey_result.get(), 0, &band), LUMICE_PATH_READ_OK);
    EXPECT_GE(band.draw_count, 0);
  }
  LUMICE_PathWarmSeed seed{};
  seed.struct_size = sizeof(seed);
  EXPECT_EQ(LUMICE_PathSurveyGetWarmSeed(survey_result.get(), info.warm_seed_count, &seed),
            LUMICE_PATH_READ_INVALID_INDEX);
  LUMICE_PathChromatic chromatic{};
  chromatic.struct_size = sizeof(chromatic);
  ASSERT_EQ(LUMICE_PathSurveyGetChromatic(survey_result.get(), &chromatic), LUMICE_PATH_READ_OK);
  EXPECT_NE(chromatic.state, 0);
  LUMICE_PathDispersion dispersion{};
  dispersion.struct_size = sizeof(dispersion);
  EXPECT_EQ(LUMICE_PathSurveyGetDispersion(survey_result.get(), &dispersion),
            chromatic.dispersion_available ? LUMICE_PATH_READ_OK : LUMICE_PATH_READ_NOT_AVAILABLE);
  if (dispersion.followed_count > 0) {
    LUMICE_PathFollowedOnset onset{};
    onset.struct_size = sizeof(onset);
    EXPECT_EQ(LUMICE_PathSurveyGetFollowedOnset(survey_result.get(), 0, &onset), LUMICE_PATH_READ_OK);
    EXPECT_GT(onset.value_count, 0u);
  }
  LUMICE_PathDiagnose diagnose{};
  diagnose.struct_size = sizeof(diagnose);
  EXPECT_EQ(LUMICE_PathSurveyGetDiagnose(survey_result.get(), &diagnose),
            chromatic.diagnose_available ? LUMICE_PATH_READ_OK : LUMICE_PATH_READ_NOT_AVAILABLE);
  if (diagnose.note_count > 0) {
    LUMICE_PathText note{};
    EXPECT_EQ(LUMICE_PathSurveyGetDiagnoseNote(survey_result.get(), 0, &note), LUMICE_PATH_READ_OK);
    EXPECT_GT(note.length, 0u);
  }
  if (diagnose.feature_count > 0) {
    LUMICE_PathDiagnoseFeature feature{};
    feature.struct_size = sizeof(feature);
    EXPECT_EQ(LUMICE_PathSurveyGetDiagnoseFeature(survey_result.get(), 0, &feature), LUMICE_PATH_READ_OK);
  }
  if (info.probe_count > 0) {
    LUMICE_PathSurveyProbe built_in{};
    built_in.struct_size = sizeof(built_in);
    ASSERT_EQ(LUMICE_PathSurveyGetProbe(survey_result.get(), 0, &built_in), LUMICE_PATH_READ_OK);
    LUMICE_PathQuantitySpec built_in_quantity{};
    built_in_quantity.struct_size = sizeof(built_in_quantity);
    ASSERT_EQ(LUMICE_PathSurveyGetProbeQuantity(survey_result.get(), 0, &built_in_quantity), LUMICE_PATH_READ_OK);
    EXPECT_EQ(built_in_quantity.kind, LUMICE_PATH_QUANTITY_BAND_SUM);
    EXPECT_GT(built_in.lattice.length, 0u);
    EXPECT_GT(built_in.refractive_index, 1.0);
    if (built_in_quantity.scope_member_count > 0) {
      LUMICE_PathFaceSequence claim_member{};
      claim_member.struct_size = sizeof(claim_member);
      EXPECT_EQ(LUMICE_PathSurveyGetClaimMember(survey_result.get(), LUMICE_PATH_EVIDENCE_PROBE, 0, 0, &claim_member),
                LUMICE_PATH_READ_OK);
    }
    if (built_in.sample_row_count > 0) {
      LUMICE_PathProbeSample row{};
      row.struct_size = sizeof(row);
      EXPECT_EQ(LUMICE_PathSurveyGetProbeSample(survey_result.get(), 0, 0, &row), LUMICE_PATH_READ_OK);
    }
  }
  const auto check_survey_object = [&](size_t index) {
    LUMICE_PathSurveyObject typed{};
    typed.struct_size = sizeof(typed);
    ASSERT_EQ(LUMICE_PathSurveyGetObject(survey_result.get(), index, &typed), LUMICE_PATH_READ_OK);
    LUMICE_PathQuantitySpec object_quantity{};
    object_quantity.struct_size = sizeof(object_quantity);
    const auto quantity_status = LUMICE_PathSurveyGetObjectQuantity(survey_result.get(), index, &object_quantity);
    EXPECT_TRUE(quantity_status == LUMICE_PATH_READ_OK || quantity_status == LUMICE_PATH_READ_NOT_AVAILABLE);
    if (quantity_status == LUMICE_PATH_READ_OK) {
      EXPECT_GT(object_quantity.kind, 0);
      if (object_quantity.scope_member_count > 0) {
        LUMICE_PathFaceSequence claim_member{};
        claim_member.struct_size = sizeof(claim_member);
        EXPECT_EQ(
            LUMICE_PathSurveyGetClaimMember(survey_result.get(), LUMICE_PATH_EVIDENCE_OBJECT, index, 0, &claim_member),
            LUMICE_PATH_READ_OK);
      }
    }
    if (typed.member_count > 0) {
      LUMICE_PathFaceSequence member{};
      member.struct_size = sizeof(member);
      EXPECT_EQ(LUMICE_PathSurveyGetObjectMember(survey_result.get(), index, 0, &member), LUMICE_PATH_READ_OK);
    }
    if (typed.kind == LUMICE_PATH_SURVEY_SUPPORT_SET) {
      LUMICE_PathSurveySupport support{};
      support.struct_size = sizeof(support);
      if (typed.state == LUMICE_PATH_STATE_FINITE || typed.state == LUMICE_PATH_STATE_CERTIFIED) {
        ASSERT_EQ(LUMICE_PathSurveyGetSupport(survey_result.get(), index, &support), LUMICE_PATH_READ_OK);
        EXPECT_EQ(support.interval_count, typed.support_interval_count);
        if (support.endpoint_count > 0) {
          LUMICE_PathSurveyEndpoint endpoint{};
          endpoint.struct_size = sizeof(endpoint);
          EXPECT_EQ(LUMICE_PathSurveyGetSupportEndpoint(survey_result.get(), index, 0, &endpoint), LUMICE_PATH_READ_OK);
        }
        if (support.density_tail_count > 0) {
          LUMICE_PathSurveyTail tail{};
          tail.struct_size = sizeof(tail);
          EXPECT_EQ(LUMICE_PathSurveyGetSupportTail(survey_result.get(), index, 0, &tail), LUMICE_PATH_READ_OK);
        }
      }
      if (typed.support_interval_count > 0) {
        LUMICE_PathSurveyInterval interval{};
        interval.struct_size = sizeof(interval);
        EXPECT_EQ(LUMICE_PathSurveyGetSupportInterval(survey_result.get(), index, 0, &interval), LUMICE_PATH_READ_OK);
        EXPECT_LE(interval.lower_deg, interval.upper_deg);
      }
      LUMICE_PathWitness witness{};
      witness.struct_size = sizeof(witness);
      EXPECT_EQ(LUMICE_PathSurveyGetWitness(survey_result.get(), index, &witness),
                typed.witness_available ? LUMICE_PATH_READ_OK : LUMICE_PATH_READ_NOT_AVAILABLE);
    } else if (typed.kind == LUMICE_PATH_SURVEY_SOURCE_IMAGE) {
      LUMICE_PathSurveySourceCertificate certificate{};
      certificate.struct_size = sizeof(certificate);
      const auto status = LUMICE_PathSurveyGetSourceCertificate(survey_result.get(), index, &certificate);
      EXPECT_TRUE(status == LUMICE_PATH_READ_OK || status == LUMICE_PATH_READ_NOT_AVAILABLE);
    } else if (typed.kind == LUMICE_PATH_SURVEY_CAUSTIC_EDGE) {
      LUMICE_PathSurveyEdge edge{};
      edge.struct_size = sizeof(edge);
      const auto status = LUMICE_PathSurveyGetEdge(survey_result.get(), index, &edge);
      EXPECT_TRUE(status == LUMICE_PATH_READ_OK || status == LUMICE_PATH_READ_NOT_AVAILABLE);
    } else if (typed.kind == LUMICE_PATH_SURVEY_BRIGHT_DOMAIN) {
      LUMICE_PathSurveyBright bright{};
      bright.struct_size = sizeof(bright);
      const auto status = LUMICE_PathSurveyGetBright(survey_result.get(), index, &bright);
      EXPECT_TRUE(status == LUMICE_PATH_READ_OK || status == LUMICE_PATH_READ_NOT_AVAILABLE);
    }
    if (typed.band_available) {
      LUMICE_PathFamilyOrigin origin{};
      origin.struct_size = sizeof(origin);
      ASSERT_EQ(LUMICE_PathSurveyGetFamilyOrigin(survey_result.get(), index, &origin), LUMICE_PATH_READ_OK);
      if (origin.frozen_verdict_count > 0) {
        LUMICE_PathBandVerdict verdict{};
        verdict.struct_size = sizeof(verdict);
        EXPECT_EQ(LUMICE_PathSurveyGetObjectBandVerdict(survey_result.get(), index, 0, &verdict), LUMICE_PATH_READ_OK);
      }
    }
  };
  for (size_t index = 0; index < info.object_count; ++index) {
    check_survey_object(index);
  }

  LUMICE_PathProbeRequest probe{};
  probe.struct_size = sizeof(probe);
  probe.crystal_id = 1;
  probe.faces = faces;
  probe.face_count = 2;
  probe.layer_face_counts = counts;
  probe.layer_count = 1;
  probe.at_altitude_deg = 20;
  probe.at_azimuth_deg = 25;
  const double wavelengths[] = { 450, 650 };
  probe.wavelengths_nm = wavelengths;
  probe.wavelength_count = 2;
  probe.sample_count = 100;
  LUMICE_PathProbe* raw_probe = nullptr;
  ASSERT_EQ(LUMICE_AnalyzePathProbe(scene.get(), &probe, &raw_probe, err, sizeof(err)), LUMICE_OK) << err;
  std::unique_ptr<LUMICE_PathProbe, void (*)(LUMICE_PathProbe*)> probe_result(raw_probe, LUMICE_PathProbeDestroy);
  LUMICE_PathProbeInfo probe_info{};
  probe_info.struct_size = sizeof(probe_info);
  ASSERT_EQ(LUMICE_PathProbeGetInfo(probe_result.get(), &probe_info), LUMICE_PATH_READ_OK);
  ASSERT_EQ(probe_info.profile_count, 2u);
  size_t probe_length = 0;
  ASSERT_EQ(LUMICE_PathProbeToJson(probe_result.get(), nullptr, 0, &probe_length), LUMICE_OK);
  std::string probe_json(probe_length + 1, '\0');
  ASSERT_EQ(LUMICE_PathProbeToJson(probe_result.get(), probe_json.data(), probe_json.size(), nullptr), LUMICE_OK);
  probe_json.resize(probe_length);
  const auto probe_document = nlohmann::json::parse(probe_json);
  EXPECT_EQ(probe_info.red_edge_width_available, probe_document.contains("derived"));
  if (probe_info.red_edge_width_available) {
    EXPECT_EQ(std::string(probe_info.red_edge_rule.data, probe_info.red_edge_rule.length),
              probe_document.at("derived").at("rule"));
    EXPECT_EQ(probe_info.red_edge_width_deg, probe_document.at("derived").at("red_edge_width_deg"));
  } else {
    EXPECT_EQ(probe_info.red_edge_rule.data, nullptr);
  }
  LUMICE_PathProbeWavelength wavelength{};
  wavelength.struct_size = sizeof(wavelength);
  ASSERT_EQ(LUMICE_PathProbeGetWavelength(probe_result.get(), 0, &wavelength), LUMICE_PATH_READ_OK);
  EXPECT_GE(wavelength.nm, 350.0);
  LUMICE_PathContext probe_context{};
  probe_context.struct_size = sizeof(probe_context);
  ASSERT_EQ(LUMICE_PathProbeGetContext(probe_result.get(), &probe_context), LUMICE_PATH_READ_OK);
  EXPECT_NE(probe_context.object_state, 0);
  LUMICE_PathShapeScalar probe_shape{};
  probe_shape.struct_size = sizeof(probe_shape);
  EXPECT_EQ(LUMICE_PathProbeGetShape(probe_result.get(), 0, &probe_shape), LUMICE_PATH_READ_OK);
  LUMICE_PathProbeProfile profile{};
  profile.struct_size = sizeof(profile);
  ASSERT_EQ(LUMICE_PathProbeGetProfile(probe_result.get(), 0, &profile), LUMICE_PATH_READ_OK);
  LUMICE_PathQuantitySpec profile_quantity{};
  profile_quantity.struct_size = sizeof(profile_quantity);
  ASSERT_EQ(LUMICE_PathProbeGetQuantity(probe_result.get(), 0, &profile_quantity), LUMICE_PATH_READ_OK);
  EXPECT_EQ(profile_quantity.kind, LUMICE_PATH_QUANTITY_BAND_SUM);
  LUMICE_PathSurveyEvidence profile_evidence{};
  profile_evidence.struct_size = sizeof(profile_evidence);
  ASSERT_EQ(LUMICE_PathProbeGetEvidence(probe_result.get(), 0, &profile_evidence), LUMICE_PATH_READ_OK);
  EXPECT_EQ(profile_evidence.state, profile.state);
  EXPECT_NE(profile.state, 0);
  EXPECT_GT(profile.semantics.length, 0u);
  EXPECT_GT(profile.lattice.length, 0u);
  if (profile.dark_region_count > 0) {
    LUMICE_PathProbeDarkRegion region{};
    region.struct_size = sizeof(region);
    ASSERT_EQ(LUMICE_PathProbeGetDarkRegion(probe_result.get(), 0, 0, &region), LUMICE_PATH_READ_OK);
    EXPECT_GE(region.kind, LUMICE_PATH_DARK_INSIDE_POSITIVE);
    EXPECT_LE(region.kind, LUMICE_PATH_DARK_UNKNOWN);
  }
  LUMICE_PathProbeSample sample{};
  sample.struct_size = sizeof(sample);
  EXPECT_EQ(LUMICE_PathProbeGetSample(probe_result.get(), 0, profile.sample_row_count, &sample),
            profile.state == LUMICE_PATH_STATE_FINITE || profile.state == LUMICE_PATH_STATE_CERTIFIED ?
                LUMICE_PATH_READ_INVALID_INDEX :
                LUMICE_PATH_READ_NOT_AVAILABLE);
  if (profile.sample_row_count > 0) {
    EXPECT_EQ(LUMICE_PathProbeGetSample(probe_result.get(), 0, 0, &sample), LUMICE_PATH_READ_OK);
    EXPECT_TRUE(sample.azimuth_available);
    EXPECT_GE(sample.k, 0);
  }
}

TEST(SinglePathCApi, RedEdgeRuleSharesTypedAndWireOwner) {
  const auto fixture = std::filesystem::path(LUMICE_WARM_FIXTURE_DIR).parent_path().parent_path() /
                       "e2e/configs/raypath_feature_plate_gauss.json";
  std::ifstream input(fixture);
  ASSERT_TRUE(input.is_open()) << fixture;
  const std::string scene_text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  const ScenePtr scene = MakeScene(scene_text.c_str());
  const int faces[] = { 3, 5 };
  const int counts[] = { 2 };
  const double wavelengths[] = { 450, 550, 650 };
  LUMICE_PathProbeRequest request{};
  request.struct_size = sizeof(request);
  request.crystal_id = 1;
  request.faces = faces;
  request.face_count = 2;
  request.layer_face_counts = counts;
  request.layer_count = 1;
  request.at_altitude_deg = 20;
  request.at_azimuth_deg = 25;
  request.wavelengths_nm = wavelengths;
  request.wavelength_count = 3;
  request.sample_count = 0;
  LUMICE_PathProbe* raw = nullptr;
  char err[256] = {};
  ASSERT_EQ(LUMICE_AnalyzePathProbe(scene.get(), &request, &raw, err, sizeof(err)), LUMICE_OK) << err;
  std::unique_ptr<LUMICE_PathProbe, void (*)(LUMICE_PathProbe*)> result(raw, LUMICE_PathProbeDestroy);
  LUMICE_PathProbeInfo info{};
  info.struct_size = sizeof(info);
  ASSERT_EQ(LUMICE_PathProbeGetInfo(result.get(), &info), LUMICE_PATH_READ_OK);
  EXPECT_TRUE(info.red_edge_width_available);
  if (info.red_edge_width_available) {
    size_t length = 0;
    ASSERT_EQ(LUMICE_PathProbeToJson(result.get(), nullptr, 0, &length), LUMICE_OK);
    std::string text(length + 1, '\0');
    ASSERT_EQ(LUMICE_PathProbeToJson(result.get(), text.data(), text.size(), nullptr), LUMICE_OK);
    text.resize(length);
    const auto document = nlohmann::json::parse(text);
    EXPECT_EQ(std::string(info.red_edge_rule.data, info.red_edge_rule.length), document.at("derived").at("rule"));
    EXPECT_EQ(info.red_edge_width_deg, document.at("derived").at("red_edge_width_deg"));
  }
}

TEST(SinglePathCApi, NullArgumentsAreRejected) {
  const ScenePtr scene = MakeScene();
  const Req req({ 3, 5 }, { 2 });
  LUMICE_SinglePathResult* out = nullptr;
  EXPECT_EQ(LUMICE_AnalyzeSinglePath(nullptr, &req.c, &out, nullptr, 0), LUMICE_ERR_NULL_ARG);
  EXPECT_EQ(LUMICE_AnalyzeSinglePath(scene.get(), nullptr, &out, nullptr, 0), LUMICE_ERR_NULL_ARG);
  EXPECT_EQ(LUMICE_AnalyzeSinglePath(scene.get(), &req.c, nullptr, nullptr, 0), LUMICE_ERR_NULL_ARG);
  Req no_faces({ 3, 5 }, { 2 });
  no_faces.c.faces = nullptr;
  EXPECT_EQ(Analyse(scene.get(), no_faces).code, LUMICE_ERR_NULL_ARG);
  EXPECT_EQ(LUMICE_SinglePathResultToJson(nullptr, nullptr, 0, nullptr), LUMICE_ERR_NULL_ARG);
  LUMICE_SinglePathResultDestroy(nullptr);  // a no-op
}

TEST(SinglePathCApi, StructSizeSmallerThanThisVersionIsRejected) {
  const ScenePtr scene = MakeScene();
  Req req({ 3, 5 }, { 2 });
  // One byte below the smallest size this build reads — the reader gate's own anchor, not a
  // fixed offset from sizeof (appended fields grow sizeof, they never lower the floor).
  req.c.struct_size = offsetof(LUMICE_SinglePathRequest, warm_json_len) + sizeof(size_t) - 1;
  const Outcome o = Analyse(scene.get(), req);
  EXPECT_EQ(o.code, LUMICE_ERR_INVALID_VALUE);
  EXPECT_EQ(o.result, nullptr);
  EXPECT_NE(o.error.find("struct_size"), std::string::npos) << o.error;
}

TEST(SinglePathCApi, AnalysesAndSerializesOnce) {
  const ScenePtr scene = MakeScene();
  const Outcome o = Analyse(scene.get(), Req({ 3, 5 }, { 2 }));
  ASSERT_EQ(o.code, LUMICE_OK) << o.error;
  ASSERT_NE(o.result, nullptr);
  EXPECT_EQ(o.error, "");  // cleared on entry, untouched on success
  const nlohmann::json doc = nlohmann::json::parse(Json(o.result.get()));
  EXPECT_EQ(doc["outcome"], "discovered");
  EXPECT_FALSE(doc["components"].empty());
  LUMICE_PathPointSample pose{};
  pose.struct_size = sizeof(pose);
  ASSERT_EQ(LUMICE_PathPointGetSample(o.result.get(), 0, 0, &pose), LUMICE_PATH_READ_OK);
  EXPECT_EQ(pose.zenith_deg, doc["components"][0]["points"][0]["angles"]["zenith_deg"]);
  EXPECT_EQ(doc["meta"]["faces"], nlohmann::json({ 3, 5 }));
  // Unset wavelength with a D65 spectrum: the default, and it says so.
  EXPECT_EQ(doc["meta"]["wavelength"]["source"], "default");
  EXPECT_EQ(doc["meta"]["discovery_settings"]["sample_count"], 50000);
  EXPECT_EQ(doc["sun_grid"]["lat_count"], 4);
  EXPECT_EQ(doc["generator"]["lumice"], LUMICE_GetVersionString());

  // Truncation: always NUL-terminated, *out_len the full length.
  char small[8];
  size_t len = 0;
  ASSERT_EQ(LUMICE_SinglePathResultToJson(o.result.get(), small, sizeof(small), &len), LUMICE_OK);
  EXPECT_EQ(std::strlen(small), sizeof(small) - 1);
  EXPECT_GT(len, sizeof(small));
}

TEST(SinglePathCApi, ZeroSampleCountTakesTheDefaultAndAUserWavelengthIsRecorded) {
  const ScenePtr scene = MakeScene();
  Req req({ 1, 2 }, { 2 }, 30.0, 90.0);  // rank 0: no discovery, so the default count costs nothing
  req.c.sample_count = 0;
  req.c.wavelength_nm = 480.0;
  const Outcome o = Analyse(scene.get(), req);
  ASSERT_EQ(o.code, LUMICE_OK) << o.error;
  const nlohmann::json doc = nlohmann::json::parse(Json(o.result.get()));
  EXPECT_EQ(doc["outcome"], "point_mass");
  EXPECT_EQ(doc["meta"]["discovery_settings"]["sample_count"], 1000000);
  EXPECT_EQ(doc["meta"]["wavelength"]["source"], "user");
  EXPECT_DOUBLE_EQ(doc["meta"]["wavelength"]["nm"].get<double>(), 480.0);
  LUMICE_PathPointInfo info{};
  info.struct_size = sizeof(info);
  ASSERT_EQ(LUMICE_PathPointGetInfo(o.result.get(), &info), LUMICE_PATH_READ_OK);
  EXPECT_EQ(info.outcome, LUMICE_PATH_POINT_MASS);
  EXPECT_EQ(std::string(info.wavelength_source.data, info.wavelength_source.length), "user");
  LUMICE_PathPointMass mass{};
  mass.struct_size = sizeof(mass);
  ASSERT_EQ(LUMICE_PathPointGetMass(o.result.get(), &mass), LUMICE_PATH_READ_OK);
  EXPECT_EQ(mass.orbit_available, doc["point_mass"]["orbit_factors"]["available"].get<bool>());
  if (mass.orbit_point_count > 0) {
    LUMICE_PathOrbitPoint point{};
    point.struct_size = sizeof(point);
    EXPECT_EQ(LUMICE_PathPointGetOrbitPoint(o.result.get(), 0, &point), LUMICE_PATH_READ_OK);
  }
  LUMICE_PathOrbitPoint none{};
  none.struct_size = sizeof(none);
  EXPECT_EQ(LUMICE_PathPointGetOrbitPoint(o.result.get(), mass.orbit_point_count, &none),
            mass.orbit_available ? LUMICE_PATH_READ_INVALID_INDEX : LUMICE_PATH_READ_NOT_AVAILABLE);
}

TEST(SinglePathCApi, RefusalsNameTheirReasonInErrBuf) {
  const ScenePtr scene = MakeScene();
  struct Row {
    const char* what;
    Req req;
    const char* reason;
  };
  std::vector<Row> rows;
  rows.push_back({ "two layers", Req({ 3, 5, 1, 3 }, { 2, 2 }), "multi_layer_unsupported: " });
  rows.push_back({ "counts do not add up", Req({ 3, 5 }, { 3 }), "invalid_path: " });
  rows.push_back({ "no layer", Req({}, {}), "invalid_path: " });
  Req unknown({ 3, 5 }, { 2 });
  unknown.c.crystal_id = 7;
  rows.push_back({ "unknown crystal", unknown, "unknown_crystal_id: " });
  Req far_wavelength({ 3, 5 }, { 2 });  // not "far": windows.h (pulled by the server headers
  // this TU includes for the warm-harvest pin) still defines the 16-bit-era far/near macros on
  // MSVC, and the bare identifier expands to nothing there.
  far_wavelength.c.wavelength_nm = 1000.0;
  rows.push_back({ "wavelength", far_wavelength, "wavelength_out_of_range: " });
  Req warm_not_json({ 3, 5 }, { 2 });
  warm_not_json.warm = "not json";
  rows.push_back({ "warm not json", warm_not_json, "invalid_argument: warm seeds" });
  Req warm_version({ 3, 5 }, { 2 });
  warm_version.warm = R"({"schema_version": 99, "components": []})";
  rows.push_back({ "warm schema version", warm_version, "invalid_argument: warm seeds" });
  Req warm_short({ 3, 5 }, { 2 });
  warm_short.warm = R"({"schema_version": 1, "components": [ {"seed": [1, 0, 0, 0, 1, 0, 0, 0]} ]})";
  rows.push_back({ "warm seed of 8", warm_short, "invalid_argument: warm seeds" });
  for (auto& row : rows) {
    row.req.Bind();
    const Outcome o = Analyse(scene.get(), row.req);
    EXPECT_EQ(o.code, LUMICE_ERR_INVALID_VALUE) << row.what;
    EXPECT_EQ(o.result, nullptr) << row.what;
    EXPECT_EQ(o.error.rfind(row.reason, 0), 0u) << row.what << ": " << o.error;
  }
}

TEST(SinglePathCApi, WarmJsonFromAnEarlierResultIsAccepted) {
  const ScenePtr scene = MakeScene();
  const Outcome first = Analyse(scene.get(), Req({ 3, 5 }, { 2 }));
  ASSERT_EQ(first.code, LUMICE_OK) << first.error;
  Req again({ 3, 5 }, { 2 });
  again.warm = Json(first.result.get());
  again.Bind();
  const Outcome second = Analyse(scene.get(), again);
  ASSERT_EQ(second.code, LUMICE_OK) << second.error;
  const nlohmann::json a = nlohmann::json::parse(again.warm);
  const nlohmann::json b = nlohmann::json::parse(Json(second.result.get()));
  EXPECT_EQ(b["meta"]["discovery_settings"]["warm_seed_count"], a["components"].size() + a["incomplete"].size());
}

// The schema-1 seed sheet through --warm keeps the context honestly inline: "not a survey
// document" is a different absence from "a survey document without its warm_index block", and
// must never be declared as the warm_index_absent degradation.
TEST(SinglePathCApi, Schema1WarmSheetKeepsTheContextInline) {
  const ScenePtr scene = MakeScene();
  const Outcome first = Analyse(scene.get(), Req({ 3, 5 }, { 2 }));
  ASSERT_EQ(first.code, LUMICE_OK) << first.error;
  Req again({ 3, 5 }, { 2 });
  again.warm = Json(first.result.get());
  again.Bind();
  const Outcome second = Analyse(scene.get(), again);
  ASSERT_EQ(second.code, LUMICE_OK) << second.error;
  const nlohmann::json doc = nlohmann::json::parse(Json(second.result.get()));
  EXPECT_GT(doc["meta"]["discovery_settings"]["warm_seed_count"], 0);  // the seeds were consumed
  const nlohmann::json& context = doc["survey_context"];
  EXPECT_EQ(context["source"], "inline");
  EXPECT_EQ(context["warm_index_absent"], false);
  EXPECT_EQ(context["band_sum"]["state"], "unevaluated");
  EXPECT_EQ(context["band_sum"]["reason"], "inline_context_runs_no_probe");
}

// ---- the --warm survey contract (schema 4) ---------------------------------------------------------

// A survey C API scene whose full-random axis routes the T1 full-dimensional arm (the support,
// the probe and the warm_index with no held pose — `seeds: []`). One production, many probes.
#ifndef LUMICE_WARM_FIXTURE_DIR
#error "LUMICE_WARM_FIXTURE_DIR must name test/fixtures/raypath-warm (test/CMakeLists.txt)"
#endif

// Byte-exact warm fixtures under test/fixtures/raypath-warm — historical writer output, never
// hand-edited; see that directory's PROVENANCE.md for the regeneration commands.
static std::string LoadWarmAsset(const char* name) {
  std::ifstream in(std::string(LUMICE_WARM_FIXTURE_DIR) + "/" + name);
  if (!in) {
    ADD_FAILURE() << "missing warm fixture: " << name;
    return "{}";
  }
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

class WarmContract : public ::testing::Test {
 protected:
  static constexpr const char* kWitnessScene = R"({
    "crystal": [ { "id": 1, "type": "prism", "shape": { "height": 1.2 },
                   "axis": { "zenith": { "type": "uniform", "mean": 90, "std": 360 },
                             "azimuth": { "type": "uniform", "mean": 0, "std": 360 } } } ],
    "filter": [],
    "scene": {
      "light_source": { "type": "sun", "altitude": 20.0, "spectrum": "D65" },
      "ray_num": 1000, "max_hits": 7,
      "scattering": [ { "prob": 0.0, "entries": [ { "crystal": 1, "proportion": 1 } ] } ]
    },
    "render": []
  })";

  // The full-dimensional warm fixtures are byte-exact CLI products of the two scene configs
  // beside them (regenerate with the commands in the fixtures' PROVENANCE.md): producing that
  // survey runs the delta partition — seconds of CPU the unit layer must not pay every run.
  // The writer-integration side stays live-covered by the cheap refused arm above (kWitnessScene,
  // produced in-process in ~40 ms) and the schema-1 sheet test.
  static const std::string& RandomScene() {
    static const std::string s = LoadWarmAsset("warm_random_scene.json");
    return s;
  }
  static const std::string& RandomSurvey() {
    static const std::string s = LoadWarmAsset("warm_random_survey.json");
    return s;
  }
  static const std::string& Sun33Scene() {
    static const std::string s = LoadWarmAsset("warm_sun33_scene.json");
    return s;
  }
  static const std::string& Sun33Survey() {
    static const std::string s = LoadWarmAsset("warm_sun33_survey.json");
    return s;
  }

  // One production per scene text, cached: the T1 arms' 2M-event probes are the schema's own
  // cost, and the five probes below consume the same two documents.
  static const std::string& CachedSurvey(const char* scene_json) {
    static std::map<std::string, std::string> cache;
    auto it = cache.find(scene_json);
    if (it == cache.end()) {
      std::string text;
      SurveyInto(&text, scene_json);
      it = cache.emplace(scene_json, std::move(text)).first;
    }
    return it->second;
  }

  static void SurveyInto(std::string* text, const char* scene_json = kWitnessScene) {
    LUMICE_Scene* raw = nullptr;
    ASSERT_EQ(LUMICE_SceneFromJson(scene_json, &raw), LUMICE_OK);
    const ScenePtr scene(raw);
    LUMICE_PathSurveyRequest s{};
    s.struct_size = sizeof(s);
    s.crystal_id = 1;
    static const int faces[] = { 3, 5 };
    s.faces = faces;
    s.face_count = 2;
    static const int layers[] = { 2 };
    s.layer_face_counts = layers;
    s.layer_count = 1;
    s.budget_ms = 15000;
    LUMICE_PathSurvey* doc = nullptr;
    ASSERT_EQ(LUMICE_AnalyzePathSurvey(scene.get(), &s, &doc, nullptr, 0), LUMICE_OK);
    const std::unique_ptr<LUMICE_PathSurvey, void (*)(LUMICE_PathSurvey*)> owned(doc, LUMICE_PathSurveyDestroy);
    size_t len = 0;
    LUMICE_PathSurveyToJson(doc, nullptr, 0, &len);
    std::string out(len + 1, '\0');
    LUMICE_PathSurveyToJson(doc, out.data(), out.size(), &len);
    out.resize(len);
    *text = std::move(out);
  }
};

// The closed loop: the survey's warm_index travels back through the point query — seeds consumed
// (zero held on this arm: an honest `seeds: []`), the gates all pass, and the context reads the
// document's structure: the target inside the support, the edge distance, and the band-sum reading
// consumed under the passing scope gate.
TEST_F(WarmContract, ClosedLoopConsumesTheIndexAndAnswersTheContext) {
  const std::string& survey_text = RandomSurvey();
  const nlohmann::json doc = nlohmann::json::parse(survey_text);
  ASSERT_TRUE(doc.contains("warm_index"));
  EXPECT_EQ(doc["warm_index"]["seeds"], nlohmann::json::array());
  EXPECT_FALSE(doc["warm_index"]["config_fingerprint"].get<std::string>().empty());
  EXPECT_FALSE(doc["warm_index"]["reuse_scope"].get<std::string>().empty());
  EXPECT_FALSE(doc["warm_index"]["produced_for"].get<std::string>().empty());

  const ScenePtr scene = MakeScene(RandomScene().c_str());
  Req q({ 3, 5 }, { 2 });
  q.warm = survey_text;
  q.Bind();
  const Outcome o = Analyse(scene.get(), q);
  ASSERT_EQ(o.code, LUMICE_OK) << o.error;
  EXPECT_EQ(nlohmann::json::parse(Json(o.result.get()))["meta"]["discovery_settings"]["warm_seed_count"], 0);
  const nlohmann::json context = nlohmann::json::parse(Json(o.result.get()))["survey_context"];
  EXPECT_EQ(context["source"], "warm");
  EXPECT_EQ(context["warm_index_absent"], false);
  EXPECT_EQ(context["object"]["state"], "certified");
  EXPECT_TRUE(context["object"]["members"].size() > 0);
  EXPECT_EQ(context["band_sum"]["state"], "finite");
  EXPECT_GT(context["band_sum"]["value"].get<double>(), 0.0);
  EXPECT_DOUBLE_EQ(context["band_sum"]["bin_deg"].get<double>(), 0.1);
  // The nearest sample sits within half a bin of the query's own deviation (the same ruler).
  const double delta_deg = nlohmann::json::parse(Json(o.result.get()))["meta"]["target"]["deviation_deg"].get<double>();
  EXPECT_LT(std::fabs(context["band_sum"]["delta_deg"].get<double>() - delta_deg), 0.05 + 1e-9);
}

// The structure gates go red on a config field inside the routing projection: the crystal
// geometry, the measure (whose echo is absent on this arm — only the fingerprint can catch it),
// and the scope the expansion ran under. Each probe changes exactly one axis and expects the
// refusal to name the warm index.
TEST_F(WarmContract, RoutingAxisMutationsAreRefusedByTheGates) {
  const std::string& survey_text = RandomSurvey();

  // Crystal geometry: height 1.0 -> 1.1.
  const char* taller = R"({
    "crystal": [ { "id": 1, "type": "prism", "shape": { "height": 1.1 },
                   "axis": { "zenith": { "type": "uniform", "mean": 0, "std": 360 },
                             "azimuth": { "type": "uniform", "mean": 0, "std": 360 },
                             "roll": { "type": "uniform", "mean": 0, "std": 360 } } } ],
    "filter": [],
    "scene": {
      "light_source": { "type": "sun", "altitude": 20.0, "spectrum": "D65" },
      "ray_num": 1000, "max_hits": 7,
      "scattering": [ { "prob": 0.0, "entries": [ { "crystal": 1, "proportion": 1 } ] } ]
    },
    "render": []
  })";
  {
    const ScenePtr scene = MakeScene(taller);
    Req q({ 3, 5 }, { 2 });
    q.warm = survey_text;
    q.Bind();
    const Outcome o = Analyse(scene.get(), q);
    EXPECT_EQ(o.code, LUMICE_ERR_INVALID_VALUE) << o.error;
    EXPECT_NE(o.error.find("warm index"), std::string::npos) << o.error;
  }
  // Measure: roll 360 -> 90 (no family_routing echo on this arm; the fingerprint is the one gate
  // that can see it).
  const char* narrow_roll = R"({
    "crystal": [ { "id": 1, "type": "prism", "shape": { "height": 1.0 },
                   "axis": { "zenith": { "type": "uniform", "mean": 0, "std": 360 },
                             "azimuth": { "type": "uniform", "mean": 0, "std": 360 },
                             "roll": { "type": "uniform", "mean": 0, "std": 90 } } } ],
    "filter": [],
    "scene": {
      "light_source": { "type": "sun", "altitude": 20.0, "spectrum": "D65" },
      "ray_num": 1000, "max_hits": 7,
      "scattering": [ { "prob": 0.0, "entries": [ { "crystal": 1, "proportion": 1 } ] } ]
    },
    "render": []
  })";
  {
    const ScenePtr scene = MakeScene(narrow_roll);
    Req q({ 3, 5 }, { 2 });
    q.warm = survey_text;
    q.Bind();
    const Outcome o = Analyse(scene.get(), q);
    EXPECT_EQ(o.code, LUMICE_ERR_INVALID_VALUE) << o.error;
    EXPECT_NE(o.error.find("warm index"), std::string::npos) << o.error;
  }
  // The path (the request echo's own gate).
  {
    const ScenePtr scene = MakeScene(RandomScene().c_str());
    Req q({ 3, 6 }, { 2 });
    q.warm = survey_text;
    q.Bind();
    const Outcome o = Analyse(scene.get(), q);
    EXPECT_EQ(o.code, LUMICE_ERR_INVALID_VALUE) << o.error;
    EXPECT_NE(o.error.find("warm index"), std::string::npos) << o.error;
  }
}

// The reuse domain's positive face: the same warm document with ONLY the sun moved still passes
// every structure gate (sun is not a routing axis), and the band-sum reading falls to the named
// scope mismatch instead of being consumed.
TEST_F(WarmContract, MovedSunPassesTheStructureGatesAndNamesTheScopeMismatch) {
  const std::string& survey_text = RandomSurvey();
  const char* moved_sun = R"({
    "crystal": [ { "id": 1, "type": "prism", "shape": { "height": 1.0 },
                   "axis": { "zenith": { "type": "uniform", "mean": 0, "std": 360 },
                             "azimuth": { "type": "uniform", "mean": 0, "std": 360 },
                             "roll": { "type": "uniform", "mean": 0, "std": 360 } } } ],
    "filter": [],
    "scene": {
      "light_source": { "type": "sun", "altitude": 45.0, "spectrum": "D65" },
      "ray_num": 1000, "max_hits": 7,
      "scattering": [ { "prob": 0.0, "entries": [ { "crystal": 1, "proportion": 1 } ] } ]
    },
    "render": []
  })";
  const ScenePtr scene = MakeScene(moved_sun);
  Req q({ 3, 5 }, { 2 });
  q.warm = survey_text;
  q.Bind();
  const Outcome o = Analyse(scene.get(), q);
  ASSERT_EQ(o.code, LUMICE_OK) << o.error;
  const nlohmann::json context = nlohmann::json::parse(Json(o.result.get()))["survey_context"];
  EXPECT_EQ(context["source"], "warm");
  EXPECT_EQ(context["object"]["state"], "certified");  // the structure reused across the sun move
  EXPECT_EQ(context["band_sum"]["state"], "unevaluated");
  EXPECT_EQ(context["band_sum"]["reason"], "scope_value_mismatch");
}

// The degradation probes, on mutated documents: a truncated scope receipt names the missing
// field, and a stripped warm_index block degrades to the inline context (visible, not a refusal,
// not silent).
TEST_F(WarmContract, FieldMissingAndBlockAbsentDegradeWithNamedDeclarations) {
  const std::string& survey_text = RandomSurvey();
  {
    nlohmann::json doc = nlohmann::json::parse(survey_text);
    doc["scope"].erase("sun_azimuth_deg");
    const ScenePtr scene = MakeScene(RandomScene().c_str());
    Req q({ 3, 5 }, { 2 });
    q.warm = doc.dump();
    q.Bind();
    const Outcome o = Analyse(scene.get(), q);
    ASSERT_EQ(o.code, LUMICE_OK) << o.error;
    const nlohmann::json context = nlohmann::json::parse(Json(o.result.get()))["survey_context"];
    EXPECT_EQ(context["band_sum"]["state"], "unevaluated");
    EXPECT_EQ(context["band_sum"]["reason"], "scope_field_missing");
    EXPECT_EQ(context["object"]["state"], "certified");
    EXPECT_EQ(context["edge"]["state"], "finite");
  }
  {
    nlohmann::json doc = nlohmann::json::parse(survey_text);
    doc["scope"].erase("wavelength_nm");
    const ScenePtr scene = MakeScene(RandomScene().c_str());
    Req q({ 3, 5 }, { 2 });
    q.warm = doc.dump();
    q.Bind();
    const Outcome o = Analyse(scene.get(), q);
    ASSERT_EQ(o.code, LUMICE_OK) << o.error;
    const nlohmann::json context = nlohmann::json::parse(Json(o.result.get()))["survey_context"];
    EXPECT_EQ(context["object"]["reason"], "scope_field_missing");
    EXPECT_EQ(context["edge"]["reason"], "scope_field_missing");
    EXPECT_FALSE(context["object"].contains("target_inside_support"));
    EXPECT_EQ(context["band_sum"]["reason"], "scope_field_missing");
  }
  {
    // The block-absent degradation on the cheap refused arm (the full-dimensional inline context
    // runs on the dev-machine bench layer, not here): the stripped block degrades visibly to the
    // inline context, whose structure answers honestly — this arm's support is refused, so the
    // membership stays unevaluated with the named reason, never a silent zero.
    const std::string& witness_text = CachedSurvey(kWitnessScene);
    nlohmann::json doc = nlohmann::json::parse(witness_text);
    doc.erase("warm_index");
    const ScenePtr scene = MakeScene(kWitnessScene);
    Req q({ 3, 5 }, { 2 });
    q.warm = doc.dump();
    q.Bind();
    const Outcome o = Analyse(scene.get(), q);
    ASSERT_EQ(o.code, LUMICE_OK) << o.error;
    const nlohmann::json context = nlohmann::json::parse(Json(o.result.get()))["survey_context"];
    EXPECT_EQ(context["source"], "warm");
    EXPECT_EQ(context["warm_index_absent"], true);
    EXPECT_EQ(context["object"]["state"], "unevaluated");
    EXPECT_EQ(context["object"]["reason"], "survey_carries_no_support");
    EXPECT_EQ(context["band_sum"]["reason"], "warm_index_absent");
    EXPECT_EQ(nlohmann::json::parse(Json(o.result.get()))["meta"]["discovery_settings"]["warm_seed_count"], 0);
  }
}

// The empty-support red arm: a hand-edited (or truncated) document whose support_set carries an
// empty interval list used to pass the shape checks, set has_support, and crash the context on a
// null dereference. Now it reads as no support: the query answers, naming the cause, and never
// dereferences an interval that is not there.
TEST_F(WarmContract, EmptyIntervalListDegradesLikeNoSupport) {
  const std::string& survey_text = RandomSurvey();
  nlohmann::json doc = nlohmann::json::parse(survey_text);
  ASSERT_TRUE(doc.contains("objects") && !doc["objects"].empty());
  doc["objects"][0]["support_set"]["intervals"] = nlohmann::json::array();
  const ScenePtr scene = MakeScene(RandomScene().c_str());
  Req q({ 3, 5 }, { 2 });
  q.warm = doc.dump();
  q.Bind();
  const Outcome o = Analyse(scene.get(), q);
  ASSERT_EQ(o.code, LUMICE_OK) << o.error;
  const nlohmann::json context = nlohmann::json::parse(Json(o.result.get()))["survey_context"];
  EXPECT_EQ(context["object"]["state"], "unevaluated");
  EXPECT_EQ(context["object"]["reason"], "survey_carries_no_support");
  EXPECT_EQ(context["edge"]["state"], "unevaluated");
  EXPECT_EQ(context["edge"]["reason"], "survey_carries_no_support");
  // The unevaluated blocks carry no value fields (see §3.12): nothing reads as a certified 0.
  EXPECT_FALSE(context["object"].contains("distance_deg"));
  EXPECT_FALSE(context["edge"].contains("distance_deg"));
}

// The dual-arm oracle: the JSON-arm harvest (through DecodeWarmCandidate inside the bridge) and the
// in-memory harvest (inside the analysis's inline assembly) must read the same blocks of the
// same document — here the same config runs both arms, and the structural answers must come out
// field-for-field identical. Only band_sum differs by design (the warm reading exists, the
// inline one names why it does not).
TEST_F(WarmContract, BothHarvestArmsReadTheSameBlocks) {
  // On the cheap refused arm (the full-dimensional pair costs a delta partition per side and
  // runs on the dev-machine bench layer): the JSON-arm harvest (through DecodeWarmCandidate inside
  // the bridge) and the in-memory harvest (inside the analysis's inline assembly) must read the
  // same blocks of the same document — here the same config runs both arms, and the structural
  // answers must come out field-for-field identical.
  const std::string& survey_text = CachedSurvey(kWitnessScene);
  const ScenePtr scene = MakeScene(kWitnessScene);
  Req warm({ 3, 5 }, { 2 });
  warm.warm = survey_text;
  warm.Bind();
  const Outcome warm_out = Analyse(scene.get(), warm);
  ASSERT_EQ(warm_out.code, LUMICE_OK) << warm_out.error;
  Req inline_req({ 3, 5 }, { 2 });
  const Outcome inline_out = Analyse(scene.get(), inline_req);
  ASSERT_EQ(inline_out.code, LUMICE_OK) << inline_out.error;
  const nlohmann::json warm_doc = nlohmann::json::parse(Json(warm_out.result.get()))["survey_context"];
  const nlohmann::json inline_doc = nlohmann::json::parse(Json(inline_out.result.get()))["survey_context"];
  EXPECT_EQ(warm_doc["source"], "warm");
  EXPECT_EQ(inline_doc["source"], "inline");
  EXPECT_EQ(warm_doc["object"], inline_doc["object"]) << warm_doc.dump(1) << " vs " << inline_doc.dump(1);
  EXPECT_EQ(warm_doc["edge"], inline_doc["edge"]);
}

// The scope gate's bit-equality premise, on a value no double spells exactly: the document's sun
// and the query's sun travel the same parse path, so 33.3 arrives bit-identical on both sides
// and the reading is consumed. A parse path that broke that identity would fail this as a
// scope_value_mismatch on a same-config query.
TEST_F(WarmContract, NonExactlyRepresentableSunStillConsumesTheReading) {
  const std::string& survey_text = Sun33Survey();
  const nlohmann::json doc = nlohmann::json::parse(survey_text);
  // The config stores the sun as float: both sides promote the same 33.3f, which is exactly the
  // bit-level same-source identity the scope gate compares (a double 33.3 literal would differ
  // in the last bits and rightly fail the gate).
  ASSERT_DOUBLE_EQ(doc["scope"]["sun_altitude_deg"].get<double>(), static_cast<double>(33.3f));
  const ScenePtr scene = MakeScene(Sun33Scene().c_str());
  Req q({ 3, 5 }, { 2 });
  q.warm = survey_text;
  q.Bind();
  const Outcome o = Analyse(scene.get(), q);
  ASSERT_EQ(o.code, LUMICE_OK) << o.error;
  const nlohmann::json context = nlohmann::json::parse(Json(o.result.get()))["survey_context"];
  EXPECT_EQ(context["object"]["state"], "certified");  // the structure gates pass on the same config
  EXPECT_EQ(context["band_sum"]["state"], "finite");
  EXPECT_GT(context["band_sum"]["value"].get<double>(), 0.0);
}

TEST_F(WarmContract, ChangedLambdaNamesTheScopeMismatch) {
  const double edge_450 =
      2.0 * std::asin(1.3156924295898642 * std::sin(std::acos(-1.0) / 6.0)) * 180.0 / std::acos(-1.0) - 60.0;
  EXPECT_NEAR(edge_450, 22.27163802869244, 1e-6);
  EXPECT_LT(21.9161264398004, 22.1);
  EXPECT_LT(22.1, edge_450);

  const std::string& survey_text = RandomSurvey();
  const ScenePtr scene = MakeScene(RandomScene().c_str());
  Req q({ 3, 5 }, { 2 });
  q.c.wavelength_nm = 450.0;
  q.c.target_altitude_deg = -2.1;
  q.c.target_azimuth_deg = 0.0;
  q.warm = survey_text;
  q.Bind();
  const Outcome o = Analyse(scene.get(), q);
  ASSERT_EQ(o.code, LUMICE_OK) << o.error;
  const nlohmann::json context = nlohmann::json::parse(Json(o.result.get()))["survey_context"];
  EXPECT_NEAR(nlohmann::json::parse(Json(o.result.get()))["meta"]["target"]["deviation_deg"].get<double>(), 22.1, 1e-5);
  EXPECT_EQ(context["source"], "warm");
  EXPECT_EQ(context["object"]["state"], "unevaluated");
  EXPECT_EQ(context["object"]["reason"], "scope_value_mismatch");
  EXPECT_FALSE(context["object"].contains("target_inside_support"));
  EXPECT_EQ(context["edge"]["state"], "unevaluated");
  EXPECT_EQ(context["edge"]["reason"], "scope_value_mismatch");
  EXPECT_EQ(context["band_sum"]["state"], "unevaluated");
  EXPECT_EQ(context["band_sum"]["reason"], "scope_value_mismatch");
}

// The seed retention's positive arm: a scene whose T0 routing refuses the delta partition hangs
// the witness argmax pose on the refused card, the warm_index carries it, and a point query
// consumes it as a discovery seed.
TEST_F(WarmContract, WitnessArgmaxSeedTravelsBackAsAWarmSeed) {
  const std::string& survey_text = CachedSurvey(kWitnessScene);
  const nlohmann::json doc = nlohmann::json::parse(survey_text);
  ASSERT_EQ(doc["warm_index"]["seeds"].size(), doc["warm_index"]["seed_sources"].size());
  // kWitnessScene is deterministic and deliberately routes the refused-support arm whose routing
  // holds a pose: an empty seeds block can only mean an upstream change dropped the retention —
  // the regression this test exists to catch — so assert, never skip.
  ASSERT_TRUE(doc["warm_index"]["seeds"].is_array());
  ASSERT_FALSE(doc["warm_index"]["seeds"].empty());
  for (const auto& source : doc["warm_index"]["seed_sources"]) {
    EXPECT_FALSE(source.get<std::string>().empty());
  }
  const ScenePtr scene = MakeScene(kWitnessScene);
  Req q({ 3, 5 }, { 2 });
  q.warm = survey_text;
  q.Bind();
  const Outcome o = Analyse(scene.get(), q);
  ASSERT_EQ(o.code, LUMICE_OK) << o.error;
  EXPECT_EQ(nlohmann::json::parse(Json(o.result.get()))["meta"]["discovery_settings"]["warm_seed_count"],
            doc["warm_index"]["seeds"].size());
}

// The warm harvest's JSON arm, pinned on a real document: DecodeWarmCandidate reads the probe
// samples' k / k_eff and the lattice count off the asset — the probe tests' crafted in-memory
// sources never exercise this reader path.
TEST_F(WarmContract, WarmHarvestCarriesTheKappaFields) {
  const ScenePtr scene = MakeScene(RandomScene().c_str());
  lumice::ConfigManager config;
  ASSERT_FALSE(static_cast<bool>(lumice::ParseConfigManager(SceneRoot(scene.get()),
                                                            "WarmContract.WarmHarvestCarriesTheKappaFields",
                                                            lumice::GetGlobalLogger(), nullptr, &config)));
  namespace rp = lumice::raypath;
  rp::PathSelection selection;
  selection.crystal_id = 1;
  selection.path_layers = { { 3, 5 } };
  rp::WarmCandidate candidate;
  ASSERT_TRUE(rp::DecodeWarmCandidate(RandomSurvey(), &candidate).Ok());
  std::optional<rp::AdmittedWarmSource> admitted;
  const rp::Error e = rp::ValidateWarmCandidate(config, selection, candidate, &admitted);
  ASSERT_TRUE(e.Ok()) << e.message;
  const auto& harvest = admitted->Facts();
  EXPECT_TRUE(harvest.index_present);
  EXPECT_TRUE(harvest.has_probe);
  EXPECT_TRUE(harvest.has_probe_azimuth);
  EXPECT_DOUBLE_EQ(harvest.probe_azimuth_deg, 0.0);
  EXPECT_EQ(harvest.probe_sample_count, 2000000);
  ASSERT_FALSE(harvest.probe_samples.empty());
  long long total_k = 0;
  double max_k_eff = 0.0;
  for (const auto& sample : harvest.probe_samples) {
    total_k += sample.k;
    max_k_eff = std::max(max_k_eff, sample.k_eff);
  }
  EXPECT_GT(total_k, 0);
  EXPECT_GT(max_k_eff, 0.0);

  nlohmann::json legacy = nlohmann::json::parse(RandomSurvey());
  legacy["probes"][0]["spec"].erase("azimuth_deg");
  legacy["generator"]["lumice"] = "4.7.4-dev";
  rp::WarmCandidate older_candidate;
  ASSERT_TRUE(rp::DecodeWarmCandidate(legacy.dump(), &older_candidate).Ok());
  ASSERT_TRUE(rp::ValidateWarmCandidate(config, selection, older_candidate, &admitted).Ok());
  const auto& older = admitted->Facts();
  EXPECT_TRUE(older.has_probe_azimuth);
  EXPECT_DOUBLE_EQ(older.probe_azimuth_deg, 0.0);

  legacy["generator"]["lumice"] = "unknown-producer";
  rp::WarmCandidate unknown_candidate;
  ASSERT_TRUE(rp::DecodeWarmCandidate(legacy.dump(), &unknown_candidate).Ok());
  ASSERT_TRUE(rp::ValidateWarmCandidate(config, selection, unknown_candidate, &admitted).Ok());
  const auto& unknown = admitted->Facts();
  EXPECT_FALSE(unknown.has_probe_azimuth);
}

// The warm path's first gate: an id that is not in the config used to reach the fingerprint's
// FatalAbort before any existence check (the warm gates compare the document's echo, never the
// id against the config) — the refusal must keep the engine's unknown-crystal semantics.
TEST(SinglePathCapi, UnknownCrystalIdWithAWarmTextIsRefusedNotAborted) {
  const ScenePtr scene = MakeScene();
  Req q({ 3, 5 }, { 2 });
  q.c.crystal_id = 999;
  q.warm = "{}";  // any warm text: the existence gate fires before the text is parsed
  q.Bind();
  const Outcome o = Analyse(scene.get(), q);
  EXPECT_NE(o.code, LUMICE_OK);
  ASSERT_NE(o.error.find("no crystal entry with id 999"), std::string::npos) << o.error;
}

}  // namespace
