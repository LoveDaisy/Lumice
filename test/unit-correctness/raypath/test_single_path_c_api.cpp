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
  Req far({ 3, 5 }, { 2 });
  far.c.wavelength_nm = 1000.0;
  rows.push_back({ "wavelength", far, "wavelength_out_of_range: " });
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

// The dual-arm oracle: the JSON-arm harvest (through ParseWarmStart inside the bridge) and the
// in-memory harvest (inside the analysis's inline assembly) must read the same blocks of the
// same document — here the same config runs both arms, and the structural answers must come out
// field-for-field identical. Only band_sum differs by design (the warm reading exists, the
// inline one names why it does not).
TEST_F(WarmContract, BothHarvestArmsReadTheSameBlocks) {
  // On the cheap refused arm (the full-dimensional pair costs a delta partition per side and
  // runs on the dev-machine bench layer): the JSON-arm harvest (through ParseWarmStart inside
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

// The lambda side of the scope gate (MovedSunPasses... probes the sun only): a changed lambda is
// also a scope_value_mismatch, and it must not touch the structural gates.
TEST_F(WarmContract, ChangedLambdaNamesTheScopeMismatch) {
  const std::string& survey_text = RandomSurvey();
  const ScenePtr scene = MakeScene(RandomScene().c_str());
  Req q({ 3, 5 }, { 2 });
  q.c.wavelength_nm = 480.0;  // the document's D65 default resolved to 550
  q.warm = survey_text;
  q.Bind();
  const Outcome o = Analyse(scene.get(), q);
  ASSERT_EQ(o.code, LUMICE_OK) << o.error;
  const nlohmann::json context = nlohmann::json::parse(Json(o.result.get()))["survey_context"];
  EXPECT_EQ(context["source"], "warm");
  EXPECT_EQ(context["object"]["state"], "certified");  // the structure gates do not see lambda
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

// The warm harvest's JSON arm, pinned on a real document: ParseWarmStart reads the probe
// samples' k / k_eff and the lattice count off the asset — the probe tests' crafted in-memory
// sources never exercise this reader path.
TEST_F(WarmContract, WarmHarvestCarriesTheKappaFields) {
  const ScenePtr scene = MakeScene(RandomScene().c_str());
  lumice::ConfigManager config;
  ASSERT_FALSE(static_cast<bool>(lumice::ParseConfigManager(SceneRoot(scene.get()),
                                                            "WarmContract.WarmHarvestCarriesTheKappaFields",
                                                            lumice::GetGlobalLogger(), nullptr, &config)));
  namespace rp = lumice::raypath;
  rp::WarmGateContext gate;
  gate.crystal_id = 1;
  gate.path_layers = { { 3, 5 } };
  gate.config_fingerprint =
      rp::survey::RoutingConfigFingerprint(config, 1, gate.path_layers, rp::survey::kDefaultSymmetryBits);
  std::vector<double> seeds;
  rp::SurveyWarmSource harvest;
  const rp::Error e = rp::ParseWarmStart(RandomSurvey(), gate, &seeds, &harvest);
  ASSERT_TRUE(e.Ok()) << e.message;
  EXPECT_TRUE(harvest.index_present);
  EXPECT_TRUE(harvest.has_probe);
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
