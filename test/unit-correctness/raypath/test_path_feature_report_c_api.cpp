// The public target-free report bridge: append-only request validation, opaque immutable result,
// and the shared length-query/fetch JSON buffer contract.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstring>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "include/lumice_base.h"
#include "include/lumice_raypath.h"
#include "include/lumice_scene.h"

namespace {

constexpr const char* kSceneJson = R"({
  "crystal": [{
    "id": 1, "type": "prism", "shape": {"height": 1},
    "axis": {
      "zenith": {"type": "uniform", "mean": 0, "std": 360},
      "azimuth": {"type": "uniform", "mean": 0, "std": 360},
      "roll": {"type": "uniform", "mean": 0, "std": 360}
    }
  }],
  "filter": [],
  "scene": {
    "light_source": {"type": "sun", "altitude": 0, "azimuth": 0, "spectrum": "D65"},
    "ray_num": 1000, "max_hits": 7,
    "scattering": [{"prob": 0, "entries": [{"crystal": 1, "proportion": 1}]}]
  },
  "render": []
})";

constexpr const char* kMultiSceneJson = R"({
  "crystal": [{
    "id": 1, "type": "prism", "shape": {"height": 1},
    "axis": {
      "zenith": {"type": "uniform", "mean": 0, "std": 360},
      "azimuth": {"type": "uniform", "mean": 0, "std": 360},
      "roll": {"type": "uniform", "mean": 0, "std": 360}
    }
  }],
  "filter": [],
  "scene": {
    "light_source": {"type": "sun", "altitude": 20, "azimuth": 0, "spectrum": "D65"},
    "ray_num": 1000, "max_hits": 7,
    "scattering": [
      {"prob": 1, "entries": [{"crystal": 1, "proportion": 1}]},
      {"prob": 0, "entries": [{"crystal": 1, "proportion": 1}]}
    ]
  },
  "render": []
})";

struct SceneDeleter {
  void operator()(LUMICE_Scene* scene) const { LUMICE_SceneDestroy(scene); }
};
struct ReportDeleter {
  void operator()(LUMICE_PathFeatureReport* report) const { LUMICE_PathFeatureReportDestroy(report); }
};
using ScenePtr = std::unique_ptr<LUMICE_Scene, SceneDeleter>;
using ReportPtr = std::unique_ptr<LUMICE_PathFeatureReport, ReportDeleter>;

ScenePtr MakeScene(const char* json = kSceneJson) {
  LUMICE_Scene* scene = nullptr;
  EXPECT_EQ(LUMICE_SceneFromJson(json, &scene), LUMICE_OK);
  return ScenePtr(scene);
}

struct Request {
  int faces[3] = { 3, 1, 5 };
  int layers[1] = { 3 };
  LUMICE_PathFeatureReportRequest c{};

  Request() {
    c.struct_size = sizeof(c);
    c.crystal_id = 1;
    c.faces = faces;
    c.face_count = 3;
    c.layer_face_counts = layers;
    c.layer_count = 1;
    c.sample_count = 64;
  }
};

struct Outcome {
  LUMICE_ErrorCode code;
  ReportPtr report;
  std::string error;
};

Outcome Analyse(const LUMICE_Scene* scene, const LUMICE_PathFeatureReportRequest* request) {
  LUMICE_PathFeatureReport* raw = reinterpret_cast<LUMICE_PathFeatureReport*>(&raw);
  char error[512];
  std::memset(error, 'x', sizeof(error));
  const LUMICE_ErrorCode code = LUMICE_AnalyzePathFeatureReport(scene, request, &raw, error, sizeof(error));
  return { code, ReportPtr(raw), std::string(error) };
}

std::string Json(const LUMICE_PathFeatureReport* report) {
  size_t length = 0;
  EXPECT_EQ(LUMICE_PathFeatureReportToJson(report, nullptr, 0, &length), LUMICE_OK);
  std::string text(length + 1, '\0');
  size_t fetched = 0;
  EXPECT_EQ(LUMICE_PathFeatureReportToJson(report, text.data(), text.size(), &fetched), LUMICE_OK);
  EXPECT_EQ(fetched, length);
  text.resize(length);
  return text;
}

TEST(PathFeatureReportCApi, NullArgumentsAndPointersAreRejected) {
  const ScenePtr scene = MakeScene();
  Request request;
  LUMICE_PathFeatureReport* out = nullptr;
  EXPECT_EQ(LUMICE_AnalyzePathFeatureReport(nullptr, &request.c, &out, nullptr, 0), LUMICE_ERR_NULL_ARG);
  EXPECT_EQ(LUMICE_AnalyzePathFeatureReport(scene.get(), nullptr, &out, nullptr, 0), LUMICE_ERR_NULL_ARG);
  EXPECT_EQ(LUMICE_AnalyzePathFeatureReport(scene.get(), &request.c, nullptr, nullptr, 0), LUMICE_ERR_NULL_ARG);
  request.c.faces = nullptr;
  EXPECT_EQ(Analyse(scene.get(), &request.c).code, LUMICE_ERR_NULL_ARG);
  EXPECT_EQ(LUMICE_PathFeatureReportToJson(nullptr, nullptr, 0, nullptr), LUMICE_ERR_NULL_ARG);
  LUMICE_PathFeatureReportDestroy(nullptr);
}

TEST(PathFeatureReportCApi, StructSizeAndLayerShapeAreValidated) {
  const ScenePtr scene = MakeScene();
  Request short_request;
  short_request.c.struct_size = offsetof(LUMICE_PathFeatureReportRequest, sample_count);
  Outcome outcome = Analyse(scene.get(), &short_request.c);
  EXPECT_EQ(outcome.code, LUMICE_ERR_INVALID_VALUE);
  EXPECT_NE(outcome.error.find("struct_size"), std::string::npos);

  Request multi_layer;
  multi_layer.layers[0] = 2;
  outcome = Analyse(scene.get(), &multi_layer.c);
  EXPECT_EQ(outcome.code, LUMICE_ERR_INVALID_VALUE);
  EXPECT_NE(outcome.error.find("layer face counts"), std::string::npos);

  Request empty_path;
  empty_path.c.face_count = 0;
  empty_path.c.faces = nullptr;
  outcome = Analyse(scene.get(), &empty_path.c);
  EXPECT_EQ(outcome.code, LUMICE_ERR_INVALID_VALUE);
  EXPECT_NE(outcome.error.find("non-empty layer"), std::string::npos);

  Request no_layers;
  no_layers.c.layer_count = 0;
  outcome = Analyse(scene.get(), &no_layers.c);
  EXPECT_EQ(outcome.code, LUMICE_ERR_INVALID_VALUE);
  EXPECT_NE(outcome.error.find("at least one non-empty layer"), std::string::npos);
}

TEST(PathFeatureReportCApi, SerializesOnceAndKeepsTheResultImmutable) {
  const ScenePtr scene = MakeScene();
  Request request;
  const Outcome outcome = Analyse(scene.get(), &request.c);
  ASSERT_EQ(outcome.code, LUMICE_OK) << outcome.error;
  ASSERT_NE(outcome.report, nullptr);
  EXPECT_EQ(outcome.error, "");
  const std::string first = Json(outcome.report.get());
  EXPECT_EQ(Json(outcome.report.get()), first);
  const nlohmann::json doc = nlohmann::json::parse(first);
  EXPECT_EQ(doc["schema"], "lumice.path-feature-report");
  EXPECT_EQ(doc["schema_version"], 2);
  EXPECT_EQ(doc["meta"]["sample_count"], 64);
  EXPECT_EQ(doc["wavelengths"].size(), 2u);
  EXPECT_EQ(doc["scene_measure"]["spectrum_nodes"].size(), 8u);
  EXPECT_EQ(doc["scene_measure"]["spectrum_nodes"][0]["source"], "scene_illuminant_uniform_380_780");

  char small[8];
  size_t full_length = 0;
  ASSERT_EQ(LUMICE_PathFeatureReportToJson(outcome.report.get(), small, sizeof(small), &full_length), LUMICE_OK);
  EXPECT_EQ(std::strlen(small), sizeof(small) - 1);
  EXPECT_EQ(full_length, first.size());
}

TEST(PathFeatureReportCApi, V1SizeRetainsTheNamedLegacySpectrumCompatibilityMode) {
  const ScenePtr scene = MakeScene();
  Request request;
  request.c.struct_size = offsetof(LUMICE_PathFeatureReportRequest, sample_count) + sizeof(int);
  const Outcome outcome = Analyse(scene.get(), &request.c);
  ASSERT_EQ(outcome.code, LUMICE_OK) << outcome.error;
  const nlohmann::json doc = nlohmann::json::parse(Json(outcome.report.get()));
  ASSERT_EQ(doc["scene_measure"]["spectrum_nodes"].size(), 2u);
  EXPECT_EQ(doc["scene_measure"]["spectrum_nodes"][0]["source"], "legacy_reference_endpoint");
}

TEST(PathFeatureReportCApi, PhysicalMemberMaskAndMultiLayerIdsReachTheSceneMeasure) {
  {
    const ScenePtr scene = MakeScene();
    Request request;
    request.c.member_selection = LUMICE_PATH_FEATURE_MEMBERS_PHYSICAL_MASK;
    request.c.physical_member_mask = uint64_t{ 1 } << 1;
    const Outcome outcome = Analyse(scene.get(), &request.c);
    ASSERT_EQ(outcome.code, LUMICE_OK) << outcome.error;
    const nlohmann::json doc = nlohmann::json::parse(Json(outcome.report.get()));
    EXPECT_EQ(doc["scene_measure"]["member_chains"].size(), 1u);
    EXPECT_EQ(doc["physical_l2_members"].size(), 1u);
  }

  const ScenePtr scene = MakeScene(kMultiSceneJson);
  Request request;
  int faces[4] = { 3, 5, 3, 5 };
  int layer_counts[2] = { 2, 2 };
  int layer_crystals[2] = { 1, 1 };
  request.c.faces = faces;
  request.c.face_count = 4;
  request.c.layer_face_counts = layer_counts;
  request.c.layer_count = 2;
  request.c.layer_crystal_ids = layer_crystals;
  request.c.layer_crystal_id_count = 2;
  request.c.member_selection = LUMICE_PATH_FEATURE_MEMBERS_CONCRETE;
  request.c.scene_measure_sample_count = 64;
  const Outcome outcome = Analyse(scene.get(), &request.c);
  ASSERT_EQ(outcome.code, LUMICE_OK) << outcome.error;
  const nlohmann::json doc = nlohmann::json::parse(Json(outcome.report.get()));
  EXPECT_EQ(doc["meta"]["layer_crystal_ids"], nlohmann::json({ 1, 1 }));
  EXPECT_EQ(doc["meta"]["requested_path_layers"], nlohmann::json({ { 3, 5 }, { 3, 5 } }));
  EXPECT_EQ(doc["scene_measure"]["member_chains"].size(), 1u);
  EXPECT_EQ(doc["scene_measure"]["evaluated_row_count"], 8 * 64);
}

TEST(PathFeatureReportCApi, AcceptsExplicitWavelengthsAndRejectsInvalidCounts) {
  const ScenePtr scene = MakeScene();
  Request request;
  const double wavelengths[2] = { 500.0, 600.0 };
  const double weights[2] = { 0.25, 0.75 };
  request.c.wavelengths_nm = wavelengths;
  request.c.wavelength_weights = weights;
  request.c.wavelength_count = 2;
  Outcome outcome = Analyse(scene.get(), &request.c);
  ASSERT_EQ(outcome.code, LUMICE_OK) << outcome.error;
  const nlohmann::json doc = nlohmann::json::parse(Json(outcome.report.get()));
  EXPECT_DOUBLE_EQ(doc["wavelengths"][0]["nm"].get<double>(), 500.0);
  EXPECT_DOUBLE_EQ(doc["wavelengths"][1]["weight"].get<double>(), 0.75);

  request.c.wavelengths_nm = nullptr;
  outcome = Analyse(scene.get(), &request.c);
  EXPECT_EQ(outcome.code, LUMICE_ERR_NULL_ARG);
}

TEST(PathFeatureReportCApi, RejectsCombinedMemberWavelengthAndSampleWorkAboveThePublicBudget) {
  const ScenePtr scene = MakeScene();
  Request request;
  std::vector<double> wavelengths(LUMICE_PATH_FEATURE_REPORT_MAX_WAVELENGTH_COUNT, 550.0);
  request.c.wavelengths_nm = wavelengths.data();
  request.c.wavelength_count = static_cast<int>(wavelengths.size());
  request.c.sample_count = LUMICE_PATH_FEATURE_REPORT_MAX_SAMPLE_COUNT;

  const Outcome outcome = Analyse(scene.get(), &request.c);
  EXPECT_EQ(outcome.code, LUMICE_ERR_INVALID_VALUE);
  EXPECT_EQ(outcome.report, nullptr);
  EXPECT_NE(outcome.error.find("sample-evaluation budget"), std::string::npos);
}

}  // namespace
