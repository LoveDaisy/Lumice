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
#include <cstring>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "include/lumice_base.h"
#include "include/lumice_raypath.h"
#include "include/lumice_scene.h"

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
  req.c.struct_size = sizeof(LUMICE_SinglePathRequest) - sizeof(size_t);
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

// ---- the --warm survey contract (schema 4) ---------------------------------------------------------

// A survey C API scene whose full-random axis routes the T1 full-dimensional arm (the support,
// the probe and the warm_index with no held pose — `seeds: []`). One production, many probes.
class WarmContract : public ::testing::Test {
 protected:
  static constexpr const char* kRandomScene = R"({
    "crystal": [ { "id": 1, "type": "prism", "shape": { "height": 1.0 },
                   "axis": { "zenith": { "type": "uniform", "mean": 0, "std": 360 },
                             "azimuth": { "type": "uniform", "mean": 0, "std": 360 },
                             "roll": { "type": "uniform", "mean": 0, "std": 360 } } } ],
    "filter": [],
    "scene": {
      "light_source": { "type": "sun", "altitude": 20.0, "spectrum": "D65" },
      "ray_num": 1000,
      "max_hits": 7,
      "scattering": [ { "prob": 0.0, "entries": [ { "crystal": 1, "proportion": 1 } ] } ]
    },
    "render": []
  })";

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

  static void SurveyInto(std::string* text, const char* scene_json = kRandomScene) {
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
  const std::string& survey_text = CachedSurvey(kRandomScene);
  const nlohmann::json doc = nlohmann::json::parse(survey_text);
  ASSERT_TRUE(doc.contains("warm_index"));
  EXPECT_EQ(doc["warm_index"]["seeds"], nlohmann::json::array());
  EXPECT_FALSE(doc["warm_index"]["config_fingerprint"].get<std::string>().empty());
  EXPECT_FALSE(doc["warm_index"]["reuse_scope"].get<std::string>().empty());
  EXPECT_FALSE(doc["warm_index"]["produced_for"].get<std::string>().empty());

  const ScenePtr scene = MakeScene(kRandomScene);
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
  const std::string& survey_text = CachedSurvey(kRandomScene);

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
    const ScenePtr scene = MakeScene(kRandomScene);
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
  const std::string& survey_text = CachedSurvey(kRandomScene);
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
  const std::string& survey_text = CachedSurvey(kRandomScene);
  {
    nlohmann::json doc = nlohmann::json::parse(survey_text);
    doc["scope"].erase("sun_azimuth_deg");
    const ScenePtr scene = MakeScene(kRandomScene);
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
    nlohmann::json doc = nlohmann::json::parse(survey_text);
    doc.erase("warm_index");
    const ScenePtr scene = MakeScene(kRandomScene);
    Req q({ 3, 5 }, { 2 });
    q.warm = doc.dump();
    q.Bind();
    const Outcome o = Analyse(scene.get(), q);
    ASSERT_EQ(o.code, LUMICE_OK) << o.error;
    const nlohmann::json context = nlohmann::json::parse(Json(o.result.get()))["survey_context"];
    EXPECT_EQ(context["source"], "warm");
    EXPECT_EQ(context["warm_index_absent"], true);
    // The degradation still runs the inline context: the structure answers, the reading does not.
    EXPECT_EQ(context["object"]["state"], "certified");
    EXPECT_EQ(context["band_sum"]["reason"], "warm_index_absent");
    EXPECT_EQ(nlohmann::json::parse(Json(o.result.get()))["meta"]["discovery_settings"]["warm_seed_count"], 0);
  }
}

// The seed retention's positive arm: a scene whose T0 routing refuses the delta partition hangs
// the witness argmax pose on the refused card, the warm_index carries it, and a point query
// consumes it as a discovery seed.
TEST_F(WarmContract, WitnessArgmaxSeedTravelsBackAsAWarmSeed) {
  // The tall fixed-axis prism routes the refused-support arm (the witness argmax's own arm).
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
  const std::string& survey_text = CachedSurvey(kWitnessScene);
  const nlohmann::json doc = nlohmann::json::parse(survey_text);
  ASSERT_EQ(doc["warm_index"]["seeds"].size(), doc["warm_index"]["seed_sources"].size());
  if (!doc["warm_index"]["seeds"].is_array() || doc["warm_index"]["seeds"].empty()) {
    GTEST_SKIP() << "this scene's routing held no pose (the retention's negative arm)";
  }
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

}  // namespace
