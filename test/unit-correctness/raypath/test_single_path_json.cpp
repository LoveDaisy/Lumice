// The JSON form of a single-path result (src/raypath/single_path_json.hpp) and its warm-seed reader:
// the key set a shell and doc/raypath/raypath-cli-output.md rely on, null for NaN, the two outcomes' mutually
// exclusive fields, the bit-exact seed round trip and the grid's 9-digit rounding. The key table
// below is the second copy of doc/raypath/raypath-cli-output.md's field table; a field added to one is added
// to the other.
//
// symmetry_semantics: none — every case analyses one concrete face sequence (doc/interfaces/analytic-api.md
// section 3).

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "config/config_manager.hpp"
#include "raypath/single_path_analysis.hpp"
#include "raypath/single_path_json.hpp"
#include "raypath/survey_report.hpp"
#include "raypath/survey_reuse.hpp"

namespace lumice::raypath {
namespace {

constexpr int kSamples = 200000;
constexpr int kGridRows = 8;

ConfigManager PrismScene() {
  PrismCrystalParam p;
  p.h_ = { DistributionType::kNoRandom, 1.0f, 0.0f };
  for (auto& d : p.d_) {
    d = { DistributionType::kNoRandom, 1.0f, 0.0f };
  }
  ConfigManager c;
  CrystalConfig crystal;
  crystal.id_ = 1;
  crystal.param_ = p;
  c.crystals_.emplace(1, crystal);
  c.scene_.light_source_.param_ = SunParam{ 20.0f, 0.0f, 0.5f };
  c.scene_.light_source_.spectrum_ = std::vector<WlParam>{ { 550.0f, 1.0f } };
  return c;
}

PointResult Analyse(std::vector<int> faces, double alt, double az) {
  SinglePathRequest r;
  r.crystal_id = 1;
  r.path_layers = { std::move(faces) };
  r.target_altitude_deg = alt;
  r.target_azimuth_deg = az;
  r.sample_count = kSamples;
  r.sun_grid_lat_count = kGridRows;
  PointResult out;
  const Error e = AnalyzePathPointValue(PrismScene(), r, true, &out);
  EXPECT_TRUE(e.Ok()) << e.message;
  return out;
}

// The 22-degree-halo path at a point on its ring: closed components, the discovered outcome.
const PointResult& Halo() {
  static const PointResult r = Analyse({ 3, 5 }, 20.0, 25.0);
  return r;
}

nlohmann::json Doc(const PointResult& r) {
  return nlohmann::json::parse(ToJson(r, "test-version"));
}

// Every field the result struct carries, as the JSON pointer it is written at. Paths into lists use
// the first element; the cases below make sure that element exists.
const char* const kDiscoveredKeys[] = {
  "/schema_version",
  "/generator/lumice",
  "/generator/analytic_api_version",
  "/conventions/frames",
  "/conventions/directions",
  "/conventions/target_azimuth",
  "/conventions/pose",
  "/conventions/angles",
  "/conventions/sun_in_crystal",
  "/conventions/units",
  "/conventions/null",
  "/conventions/sun_grid",
  "/conventions/completeness",
  "/meta/crystal/id",
  "/meta/crystal/kind",
  "/meta/crystal/shape/0/name",
  "/meta/crystal/shape/0/value",
  "/meta/crystal/shape/0/distribution",
  "/meta/crystal/shape/0/spread",
  "/meta/crystal/shape_is_nominal",
  "/meta/faces",
  "/meta/sun/altitude_deg",
  "/meta/sun/azimuth_deg",
  "/meta/sun/diameter_deg",
  "/meta/sun/incident_direction",
  "/meta/target/altitude_deg",
  "/meta/target/azimuth_deg",
  "/meta/target/direction",
  "/meta/target/deviation_deg",
  "/meta/wavelength/nm",
  "/meta/wavelength/source",
  "/meta/wavelength/refractive_index",
  "/meta/discovery_settings/sample_count",
  "/meta/discovery_settings/band_half_width_rad",
  "/meta/discovery_settings/cluster_radius_rad",
  "/meta/discovery_settings/distance_threshold_rad",
  "/meta/discovery_settings/warm_seed_count",
  "/outcome",
  "/components/0/kind",
  "/components/0/seed",
  "/components/0/forward/status",
  "/components/0/forward/reason",
  "/components/0/forward/pose_count",
  "/components/0/seed_index",
  "/components/0/arclength_increments",
  "/components/0/points/0/pose",
  "/components/0/points/0/angles/zenith_deg",
  "/components/0/points/0/angles/azimuth_deg",
  "/components/0/points/0/angles/roll_deg",
  "/components/0/points/0/angles/degenerate",
  "/components/0/points/0/sun_in_crystal",
  "/components/0/points/0/residual_norm",
  "/components/0/points/0/valid",
  "/components/0/points/0/outgoing_direction",
  "/components/0/points/0/segment_directions",
  "/components/0/points/0/interface_transmittances",
  "/components/0/points/0/total_transmission",
  "/components/0/points/0/entry_measure",
  "/components/0/points/0/rho",
  "/components/0/points/0/entry_measure_a2",
  "/components/0/points/0/jacobian/value",
  "/components/0/points/0/jacobian/singular_values",
  "/components/0/points/0/jacobian/available",
  "/components/0/points/0/k_carea",
  "/components/0/points/0/factor_product",
  "/components/0/factors/epsilon",
  "/components/0/factors/fiber_integral",
  "/components/0/factors/fiber_integral_rule",
  "/components/0/factors/completeness/available_points",
  "/components/0/factors/completeness/total_points",
  "/components/0/factors/completeness/note",
  "/measure_registration/support_kind",
  "/measure_registration/fast_path",
  "/measure_registration/registered_gap",
  "/incomplete",
  "/discovery/complete",
  "/discovery/pool_count",
  "/discovery/extra_seed_count",
  "/discovery/raw_cluster_count",
  "/discovery/admissible_count",
  "/discovery/dedup_merged",
  "/discovery/arc_stitched",
  "/discovery/arc_backward_failed",
  "/discovery/arc_backward_closed_anomaly",
  "/discovery/incomplete_unnamed_event",
  "/discovery/incomplete_not_converged",
  "/reach/target_in_range",
  "/reach/target_deviation_rad",
  "/reach/deviation_min_rad",
  "/reach/deviation_max_rad",
  "/reach/tolerance_rad",
  "/reach/probe_lat_count",
  "/survey_context/source",
  "/survey_context/warm_index_absent",
  "/survey_context/object/state",
  "/survey_context/edge/state",
  "/survey_context/band_sum/state",
  "/sun_grid/lat_count",
  "/sun_grid/lon_count",
  "/sun_grid/deviation_rad",
  "/sun_grid/valid",
  "/sun_grid/entry_measure",
};

TEST(SinglePathJson, EveryFieldOfADiscoveredResultHasItsKey) {
  ASSERT_FALSE(Halo().Data().Discovered().components.empty());
  ASSERT_FALSE(Halo().Data().Discovered().components[0].points.empty());
  const nlohmann::json doc = Doc(Halo());
  for (const char* key : kDiscoveredKeys) {
    EXPECT_TRUE(doc.contains(nlohmann::json::json_pointer(key))) << key;
  }
  EXPECT_EQ(doc["schema_version"], kSchemaVersion);
  EXPECT_EQ(doc["generator"]["lumice"], "test-version");
  EXPECT_EQ(doc["outcome"], "discovered");
  EXPECT_EQ(doc["meta"]["wavelength"]["source"], "config");
  EXPECT_EQ(doc["meta"]["crystal"]["shape"][0]["distribution"], "fixed");
  EXPECT_EQ(doc["meta"]["faces"], nlohmann::json({ 3, 5 }));
  EXPECT_FALSE(doc.contains("point_mass"));
  EXPECT_TRUE(doc["conventions"].contains("reach"));
  // reach is written right after discovery (fields are only appended).
  const auto ordered = nlohmann::ordered_json::parse(ToJson(Halo(), "test-version"));
  auto after_discovery = ordered.find("discovery");
  ASSERT_NE(after_discovery, ordered.end());
  EXPECT_EQ((++after_discovery).key(), "reach");
  // A prism carries no wedge angles.
  EXPECT_FALSE(doc["meta"]["crystal"].contains("upper_wedge_deg"));
}

// The `directions` convention sentence states the light-travel semantics — that
// target_direction is the propagation of light arriving FROM the target sky point and that a
// direction's displayed point is the one it comes from. The wording is free-text (not a
// parsing contract), but a drift back toward a "crystal -> observer" phrasing reads as if the
// field pointed AT the sky point, which is exactly the misreading the sentence exists to
// prevent; the substrings below are what pin it.
TEST(SinglePathJson, DirectionsConventionStatesTheLightTravelSemantics) {
  const nlohmann::json doc = Doc(Halo());
  const std::string directions = doc["conventions"]["directions"];
  EXPECT_NE(directions.find("light-travel convention"), std::string::npos);
  EXPECT_NE(directions.find("arriving from the target sky point"), std::string::npos);
  EXPECT_NE(directions.find("asin(-d.z)"), std::string::npos);
  EXPECT_NE(directions.find("atan2(y, x) - 180"), std::string::npos);
}

TEST(SinglePathJson, AClosedComponentHasNoBackwardTrace) {
  const nlohmann::json doc = Doc(Halo());
  int closed = 0;
  for (const auto& c : doc["components"]) {
    if (c["kind"] == "closed") {
      closed++;
      EXPECT_FALSE(c.contains("backward"));
    } else {
      EXPECT_TRUE(c.contains("backward"));
    }
    EXPECT_EQ(c["points"].size(), c["arclength_increments"].size() + 1);
  }
  EXPECT_GT(closed, 0);
}

TEST(SinglePathJson, SunGridIsRowsByTwiceRowsWithNullWhereNotValid) {
  const nlohmann::json grid = Doc(Halo())["sun_grid"];
  EXPECT_EQ(grid["lat_count"], kGridRows);
  EXPECT_EQ(grid["lon_count"], 2 * kGridRows);
  const size_t cells = static_cast<size_t>(kGridRows) * 2 * kGridRows;
  ASSERT_EQ(grid["deviation_rad"].size(), cells);
  ASSERT_EQ(grid["valid"].size(), cells);
  ASSERT_EQ(grid["entry_measure"].size(), cells);
  int valid = 0;
  int invalid = 0;
  for (size_t i = 0; i < cells; i++) {
    if (grid["valid"][i] == 1) {
      valid++;
      EXPECT_TRUE(grid["deviation_rad"][i].is_number()) << i;
    } else {
      invalid++;
      EXPECT_TRUE(grid["deviation_rad"][i].is_null()) << i;
    }
  }
  // Both kinds of cell exist for this path, so both branches above were exercised.
  EXPECT_GT(valid, 0);
  EXPECT_GT(invalid, 0);
}

// Rounded, not merely printed short: every grid number reads back unchanged through 9 significant
// digits, while the unrounded deviations of this grid do not (so the rounding is what is measured).
TEST(SinglePathJson, SunGridArraysKeepNineSignificantDigits) {
  const nlohmann::json grid = Doc(Halo())["sun_grid"];
  auto nine_digit = [](double x) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.9g", x);
    return std::strtod(buf, nullptr) == x;
  };
  for (const char* key : { "deviation_rad", "entry_measure" }) {
    for (const auto& x : grid[key]) {
      if (x.is_number()) {
        EXPECT_TRUE(nine_digit(x.get<double>())) << key << " " << x;
      }
    }
  }
  int longer = 0;
  for (double x : Halo().Data().sun_grid.deviation_rad) {
    longer += std::isfinite(x) && !nine_digit(x) ? 1 : 0;
  }
  EXPECT_GT(longer, 0);
}

TEST(SinglePathJson, RankZeroWritesThePointMassAndNoDiscovery) {
  const PointResult r = Analyse({ 1, 2 }, 30.0, 90.0);
  ASSERT_EQ(r.Data().OutcomeValue(), Outcome::kPointMass);
  const nlohmann::json doc = Doc(r);
  EXPECT_EQ(doc["outcome"], "point_mass");
  for (const char* key : { "/point_mass/direction", "/point_mass/altitude_deg", "/point_mass/azimuth_deg",
                           "/point_mass/target_separation_deg", "/point_mass/orbit_factors/available",
                           "/point_mass/orbit_factors/measure_support", "/meta/faces", "/sun_grid/valid" }) {
    EXPECT_TRUE(doc.contains(nlohmann::json::json_pointer(key))) << key;
  }
  EXPECT_EQ(doc["point_mass"]["direction"].size(), 3u);
  for (const char* key : { "components", "incomplete", "discovery", "reach", "measure_registration" }) {
    EXPECT_FALSE(doc.contains(key)) << key;
  }
  // A point-mass output is still a valid warm-seed source: it simply holds none.
  std::vector<double> seeds;
  EXPECT_TRUE(ParseWarmSeeds(ToJson(r, "v"), &seeds).Ok());
  EXPECT_TRUE(seeds.empty());
}

// A fixed-axis scene's declared measure is a point mass: the orbit factor block carries the
// honest unavailable state (named reason, the support spelled) and no integral — never a
// defaulted number. Exactly one of the two block shapes is written.
TEST(SinglePathJson, OrbitFactorsSpellTheUnavailableStateWhenTheSupportIsNotASpinOrbit) {
  const PointResult r = Analyse({ 1, 2 }, 30.0, 90.0);
  ASSERT_EQ(r.Data().OutcomeValue(), Outcome::kPointMass);
  ASSERT_FALSE(r.Data().Mass().orbit_factors.available);
  const nlohmann::json doc = Doc(r);
  const auto& orbit = doc["point_mass"]["orbit_factors"];
  EXPECT_EQ(orbit["available"], false);
  EXPECT_EQ(orbit["measure_support"], "point");
  EXPECT_EQ(orbit["reason"], "measure_support_not_spin_orbit");
  EXPECT_FALSE(orbit.contains("integral"));
  EXPECT_FALSE(orbit.contains("points"));
}

// The spin-orbit block's field set: the per-theta factor readings, the quadrature integral with
// its rule, and the kept count. The density of a uniform-azimuth spin orbit is 1/(2 pi) at every
// parameter — the spec's own constant.
TEST(SinglePathJson, OrbitFactorsOnASpinOrbitCarryTheGridAndTheIntegral) {
  ConfigManager scene = PrismScene();
  CrystalConfig& crystal = scene.crystals_.at(1);
  crystal.axis_.azimuth_dist = { DistributionType::kUniform, 0.0f, 360.0f };
  SinglePathRequest q;
  q.crystal_id = 1;
  q.path_layers = { { 1, 2 } };
  q.target_altitude_deg = 30.0;
  q.target_azimuth_deg = 90.0;
  q.sun_grid_lat_count = 0;
  PointResult r;
  ASSERT_TRUE(AnalyzePathPointValue(scene, q, true, &r).Ok());
  ASSERT_EQ(r.Data().OutcomeValue(), Outcome::kPointMass);
  ASSERT_TRUE(r.Data().Mass().orbit_factors.available);
  const nlohmann::json doc = Doc(r);
  const auto& orbit = doc["point_mass"]["orbit_factors"];
  EXPECT_EQ(orbit["available"], true);
  EXPECT_EQ(orbit["measure_support"], "spin_orbit");
  EXPECT_EQ(orbit["sample_count"], kOrbitFactorSampleCount);
  EXPECT_EQ(orbit["points"].size(), kOrbitFactorSampleCount);
  EXPECT_TRUE(orbit.contains("integral"));
  EXPECT_TRUE(orbit.contains("integral_rule"));
  EXPECT_TRUE(orbit.contains("kept_count"));
  EXPECT_FALSE(orbit.contains("reason"));
  for (const auto& p : orbit["points"]) {
    EXPECT_NEAR(p["rho_theta"].get<double>(), 1.0 / (2.0 * 3.14159265358979323846), 1e-15);
    EXPECT_TRUE(p.contains("entry_measure_a2"));
    EXPECT_TRUE(p.contains("total_transmission"));
    EXPECT_TRUE(p.contains("valid"));
  }
}

TEST(SinglePathJson, SeedsRoundTripBitExactly) {
  const PointResult& r = Halo();
  std::vector<double> expected;
  for (const auto& c : r.Data().Discovered().components) {
    expected.insert(expected.end(), c.seed, c.seed + 9);
  }
  for (const auto& c : r.Data().Discovered().incomplete) {
    expected.insert(expected.end(), c.seed, c.seed + 9);
  }
  ASSERT_FALSE(expected.empty());
  std::vector<double> seeds;
  const Error e = ParseWarmSeeds(ToJson(r, "v"), &seeds);
  ASSERT_TRUE(e.Ok()) << e.message;
  ASSERT_EQ(seeds.size(), expected.size());
  EXPECT_EQ(std::memcmp(seeds.data(), expected.data(), expected.size() * sizeof(double)), 0);
}

TEST(SinglePathJson, NanIsWrittenAsNull) {
  PointData data;
  data.meta.target_deviation_deg = std::numeric_limits<double>::quiet_NaN();
  FiberComponent c;
  c.points.emplace_back();
  c.points[0].residual_norm = std::numeric_limits<double>::infinity();
  data.Discovered().components.push_back(c);
  const PointResult r(FinalizePointData(std::move(data)));
  const nlohmann::json doc = Doc(r);
  EXPECT_TRUE(doc["meta"]["target"]["deviation_deg"].is_null());
  EXPECT_TRUE(doc["components"][0]["points"][0]["residual_norm"].is_null());
  EXPECT_TRUE(doc["components"][0]["points"][0]["total_transmission"].is_number());
}

TEST(SinglePathJson, WarmSeedReaderRejectsWhatIsNotThisFormat) {
  const std::string good = ToJson(Halo(), "v");
  auto code_of = [](const std::string& text) {
    std::vector<double> seeds;
    const Error e = ParseWarmSeeds(text, &seeds);
    EXPECT_TRUE(e.Ok() || !e.message.empty());
    EXPECT_TRUE(e.Ok() || seeds.empty());
    return e.code;
  };
  EXPECT_EQ(code_of(good), ErrorCode::kOk);
  EXPECT_EQ(code_of("not json"), ErrorCode::kInvalidArgument);
  EXPECT_EQ(code_of("[1, 2]"), ErrorCode::kInvalidArgument);
  EXPECT_EQ(code_of(R"({"components": []})"), ErrorCode::kInvalidArgument);

  nlohmann::json doc = nlohmann::json::parse(good);
  nlohmann::json other = doc;
  other["schema_version"] = kSchemaVersion + 1;
  EXPECT_EQ(code_of(other.dump()), ErrorCode::kInvalidArgument);

  nlohmann::json short_seed = doc;
  short_seed["components"][0]["seed"].erase(8);
  EXPECT_EQ(code_of(short_seed.dump()), ErrorCode::kInvalidArgument);

  nlohmann::json text_seed = doc;
  text_seed["components"][0]["seed"][0] = "1";
  EXPECT_EQ(code_of(text_seed.dump()), ErrorCode::kInvalidArgument);

  nlohmann::json no_seed = doc;
  no_seed["components"][0].erase("seed");
  EXPECT_EQ(code_of(no_seed.dump()), ErrorCode::kInvalidArgument);
}

TEST(SinglePathJson, WarmCandidateDecoderPreservesNullRowsAndRejectsUnknownSchema) {
  nlohmann::json doc = {
    { "schema", "lumice.path-survey" },
    { "schema_version", kSurveySchemaVersion },
    { "request", { { "crystal_id", 1 }, { "path_layers", { { 3, 5 } } } } },
    { "scope", { { "scene_layer", nullptr } } },
    { "warm_index",
      { { "seeds", nlohmann::json::array() },
        { "seed_sources", nlohmann::json::array() },
        { "reuse_scope", "edited" },
        { "produced_for", "edited" },
        { "config_fingerprint", "edited" } } },
    { "probes",
      { { { "samples",
            { { { "delta_deg", 22.0 }, { "value", nullptr }, { "k", 0 }, { "k_eff", nullptr } },
              { { "delta_deg", 22.1 }, { "value", 0.0 }, { "k", 2 }, { "k_eff", 1.0 } } } } } } }
  };
  WarmCandidate candidate;
  ASSERT_TRUE(DecodeWarmCandidate(doc.dump(), &candidate).Ok());
  EXPECT_EQ(candidate.origin, WarmOrigin::kImportedJson);
  ASSERT_EQ(candidate.facts.probe_samples.size(), 2u);
  EXPECT_TRUE(std::isnan(candidate.facts.probe_samples[0].value));
  EXPECT_DOUBLE_EQ(candidate.facts.probe_samples[0].k_eff, 0.0);
  EXPECT_DOUBLE_EQ(candidate.facts.probe_samples[1].value, 0.0);

  WarmCandidate seed_sheet;
  EXPECT_EQ(DecodeWarmCandidate(R"({"schema_version":4294967297,"components":[]})", &seed_sheet).code,
            ErrorCode::kInvalidArgument);
  doc["request"]["path_layers"][0][0] = std::uint64_t{ 4294967299ULL };
  EXPECT_EQ(DecodeWarmCandidate(doc.dump(), &candidate).code, ErrorCode::kInvalidArgument);
  doc["request"]["path_layers"][0][0] = 3;
  doc["objects"] = nlohmann::json::array({ nlohmann::json::object() });
  doc["objects"][0]["members"] = nlohmann::json::array({ nlohmann::json::array({ std::uint64_t{ 4294967299ULL } }) });
  EXPECT_EQ(DecodeWarmCandidate(doc.dump(), &candidate).code, ErrorCode::kInvalidArgument);
  doc.erase("objects");
  doc["probes"][0]["spec"] = { { "sample_count", std::uint64_t{ 4294967298ULL } } };
  EXPECT_EQ(DecodeWarmCandidate(doc.dump(), &candidate).code, ErrorCode::kInvalidArgument);
  doc["probes"][0].erase("spec");
  doc["probes"][0]["samples"][1]["k"] = std::uint64_t{ 4294967298ULL };
  EXPECT_EQ(DecodeWarmCandidate(doc.dump(), &candidate).code, ErrorCode::kInvalidArgument);
  doc["probes"][0]["samples"][1]["k"] = 2;
  doc["probes"][0]["samples"][0]["delta_deg"] = "bad";
  EXPECT_EQ(DecodeWarmCandidate(doc.dump(), &candidate).code, ErrorCode::kInvalidArgument);
  doc["probes"][0]["samples"][0]["delta_deg"] = 22.0;
  doc["schema_version"] = kSurveySchemaVersion + 1;
  EXPECT_EQ(DecodeWarmCandidate(doc.dump(), &candidate).code, ErrorCode::kInvalidArgument);
  doc["schema_version"] = kSurveySchemaVersion;
  doc["schema"] = "unknown";
  EXPECT_EQ(DecodeWarmCandidate(doc.dump(), &candidate).code, ErrorCode::kInvalidArgument);
  doc["schema"] = "lumice.path-survey";
  doc["scope"]["geometry_kind"] = "unknown";
  EXPECT_EQ(DecodeWarmCandidate(doc.dump(), &candidate).code, ErrorCode::kInvalidArgument);
  doc["scope"].erase("geometry_kind");
  doc["scope"]["wavelength_nm"] = "bad";
  EXPECT_EQ(DecodeWarmCandidate(doc.dump(), &candidate).code, ErrorCode::kInvalidArgument);
  doc["scope"].erase("wavelength_nm");
  doc.erase("warm_index");
  ASSERT_TRUE(DecodeWarmCandidate(doc.dump(), &candidate).Ok());
  EXPECT_FALSE(candidate.facts.index_present);
  PathSelection selection;
  selection.crystal_id = 1;
  selection.path_layers = { { 3, 5 } };
  std::optional<AdmittedWarmSource> admitted;
  ASSERT_TRUE(ValidateWarmCandidate(PrismScene(), selection, candidate, &admitted).Ok());
  ASSERT_TRUE(admitted.has_value());
  EXPECT_EQ(admitted->Origin(), WarmOrigin::kImportedJson);
  doc["warm_index"] = { { "seeds", nlohmann::json::array() } };
  ASSERT_TRUE(DecodeWarmCandidate(doc.dump(), &candidate).Ok());
  EXPECT_FALSE(candidate.facts.index_present);
  ASSERT_TRUE(ValidateWarmCandidate(PrismScene(), selection, candidate, &admitted).Ok());
  ASSERT_TRUE(admitted.has_value());
  EXPECT_FALSE(admitted->Facts().index_present);
  EXPECT_TRUE(admitted->Seeds().empty());
  doc["warm_index"]["seeds"] = "bad";
  EXPECT_EQ(DecodeWarmCandidate(doc.dump(), &candidate).code, ErrorCode::kInvalidArgument);

  WarmCandidate typed;
  typed.seeds.assign(9, 0.0);
  typed.seeds[0] = std::numeric_limits<double>::quiet_NaN();
  EXPECT_EQ(ValidateWarmCandidate(PrismScene(), selection, typed, &admitted).code, ErrorCode::kInvalidArgument);
}

TEST(SinglePathJson, UnknownTypedSchemaCannotFreeze) {
  PointData data;
  data.meta.schema_version = kSchemaVersion + 1;
  EXPECT_THROW(PointResult(FinalizePointData(std::move(data))), std::invalid_argument);
}

TEST(SinglePathJson, TheConstructionSlotIsNotPublishable) {
  EXPECT_THROW(ToJson(PointResult{}, "test"), std::invalid_argument);
}

}  // namespace
}  // namespace lumice::raypath
