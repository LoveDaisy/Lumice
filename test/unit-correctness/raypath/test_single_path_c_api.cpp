// lumice.h's single-path entry point (src/server/c_api_raypath.cpp): argument checks, the
// struct_size gate, the request -> module mapping (layers, unset wavelength, default sample count,
// warm JSON), error reporting through err_buf, and the snprintf-style buffer contract of
// LUMICE_SinglePathResultToJson. The analysis itself is test_single_path_analysis.cpp's subject and
// the JSON's shape test_single_path_json.cpp's; here only what the bridge adds.
//
// symmetry_semantics: none — every case analyses one concrete face sequence (doc/analytic-api.md
// section 3).

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
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

}  // namespace
