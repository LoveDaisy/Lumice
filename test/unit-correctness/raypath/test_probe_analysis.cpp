#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstring>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "include/lumice_base.h"
#include "include/lumice_raypath.h"
#include "include/lumice_scene.h"
#include "raypath/probe_analysis.hpp"
#include "raypath/probe_json.hpp"
#include "survey_test_fixtures.hpp"

namespace lumice::raypath {
namespace {

constexpr int kLattice = 2000000;  // the survey probe's count (the schema ruler)

// A schema-4 harvest shaped by hand: the full-axis radial grid at 0.1 degrees with crafted values
// and counts. The reuse arm copies these verbatim; every consumer downstream (readings, the dark
// decomposition) reads them like a real survey document's.
// The grid's bin index layout (deltas 0.05 + 0.1k): below 219 outside the support [21.9, 40],
// 219 the weak-tail bin, 220-230 the edge's own events, 231-249 the sparse gap inside the support
// (the weak tail the decomposition must keep), 250-399 the positive tail, 400 and up outside.
constexpr int kEdgeBin = 220;

// The grid's centre at bin k, spelled as the layout spells it (two statements, no fused
// multiply-add): a one-expression form contracts differently under -march=native and lands one
// ulp off the layout's own centres, which would silently flip the reuse gate.
double GridCentreDeg(int k) {
  const double lo = static_cast<double>(k) * 0.1;
  return lo + 0.05;
}

SurveyWarmSource CraftedWarm(double wavelength_nm, int sample_count) {
  SurveyWarmSource warm;
  warm.from_survey_document = true;
  warm.index_present = true;
  warm.has_scope = true;
  warm.wavelength_nm = wavelength_nm;
  warm.wavelength_source = "default";
  warm.sun_altitude_deg = 0.0;  // RandomPrismScene's sun (0, 0)
  warm.sun_azimuth_deg = 0.0;
  warm.has_support = true;
  warm.intervals.push_back({ 21.9, 40.0, { 3, 5 } });
  warm.has_edge = true;
  warm.edge_deg = GridCentreDeg(kEdgeBin);  // the grid's own value at the edge
  warm.has_probe = true;
  warm.probe_bin_deg = 0.1;
  warm.probe_sample_count = sample_count;
  for (int k = 0; k < 1800; k++) {
    SurveyWarmSource::ProbeSample sample;
    sample.delta_deg = GridCentreDeg(k);
    if (k >= kEdgeBin && k < kEdgeBin + 11) {
      sample.k = 5;  // the edge's own events
      sample.value = 1.0;
    } else if (k >= kEdgeBin + 11 && k < 250) {
      sample.k = 0;  // the sparse gap inside the support: the weak-tail bins
      sample.value = 1e-6;
    } else if (k >= 250 && k < 400) {
      sample.k = 10;
      sample.value = 0.5;
    }
    warm.probe_samples.push_back(sample);
  }
  return warm;
}

ProbeRequest RadialRequest(const ConfigManager& config, double at_altitude, double at_azimuth) {
  ProbeRequest request;
  request.crystal_id = 1;
  request.path_layers = { { 3, 5 } };
  request.along = ProbeAlong::kRadial;
  request.at_altitude_deg = at_altitude;
  request.at_azimuth_deg = at_azimuth;
  request.sample_count = 20000;  // the tiny lattice the recompute arms run at
  (void)config;
  return request;
}

// The reuse arm: the scope matches, the grid is the warm grid, and the values (and the counts the
// dark decomposition classifies by) are the warm document's own — zero BandSum calls.
TEST(ProbeAnalysis, ReuseArmCopiesWarmValuesWhenScopeAndGridMatch) {
  const ConfigManager config = survey::RandomPrismScene();
  const SurveyWarmSource warm = CraftedWarm(550.0, kLattice);
  const double edge = warm.edge_deg;
  ProbeRequest request = RadialRequest(config, -30.0, 0.0);
  request.wavelengths_nm = { 550.0 };
  request.sample_count = kLattice;  // must match the warm probe's lattice for the reuse
  request.warm_survey = warm;

  ProbeResult result;
  const Error e = AnalyzePathProbe(config, request, &result);
  ASSERT_TRUE(e.Ok()) << e.message;
  ASSERT_EQ(result.profiles.size(), 1u);
  const ProbeProfile& profile = result.profiles.front();
  EXPECT_EQ(profile.source, "warm_reused");
  EXPECT_EQ(profile.evidence_state, "finite");
  ASSERT_EQ(profile.samples.size(), 1800u);
  for (size_t k = 0; k < profile.samples.size(); k++) {
    EXPECT_DOUBLE_EQ(profile.samples[k].value, warm.probe_samples[k].value);
    EXPECT_EQ(profile.samples[k].k, warm.probe_samples[k].k);
  }
  // The readings read off the crafted grid: the edge is the first non-empty bin, the peak the
  // argmax of the crafted values (the edge's own 1.0 beats the 0.5 tail).
  EXPECT_TRUE(profile.readings.has_edge);
  EXPECT_DOUBLE_EQ(profile.readings.edge_deg, edge);
  EXPECT_TRUE(profile.readings.has_peak);
  EXPECT_DOUBLE_EQ(profile.readings.peak_deg, edge);
  // The context is the warm document's, and the band-sum reading passes the scope gate.
  EXPECT_EQ(result.context.source, "warm");
  EXPECT_EQ(result.context.object.state, "certified");
  EXPECT_EQ(result.context.band_sum.state, "finite");
  // The dark decomposition: outside, the edge's own positive run, the weak tail stays its own
  // finite-zero class (never clamped into a written zero), the positive tail, outside.
  ASSERT_EQ(profile.dark_regions.size(), 6u);
  EXPECT_EQ(profile.dark_regions[0].kind, "outside_support");
  EXPECT_EQ(profile.dark_regions[1].kind, "inside_support_zero");
  EXPECT_EQ(profile.dark_regions[2].kind, "inside_support_positive");
  EXPECT_EQ(profile.dark_regions[3].kind, "inside_support_zero");
  EXPECT_GT(profile.dark_regions[3].bin_count, 0);  // the weak tail stays its own class
  EXPECT_EQ(profile.dark_regions[4].kind, "inside_support_positive");
  EXPECT_EQ(profile.dark_regions[5].kind, "outside_support");
}

// The scope mismatch family: the warm document's lambda differs, so the line recomputes and the
// context's band-sum reading names the cause.
TEST(ProbeAnalysis, ScopeMismatchRecomputesAndNamesTheCause) {
  const ConfigManager config = survey::RandomPrismScene();
  ProbeRequest request = RadialRequest(config, -30.0, 0.0);
  request.warm_survey = CraftedWarm(650.0, 20000);  // the warm map ran at 650, the query asks 550

  ProbeResult result;
  const Error e = AnalyzePathProbe(config, request, &result);
  ASSERT_TRUE(e.Ok()) << e.message;
  ASSERT_EQ(result.profiles.size(), 1u);
  EXPECT_EQ(result.profiles.front().source, "recomputed");
  EXPECT_EQ(result.context.band_sum.state, "unevaluated");
  EXPECT_EQ(result.context.band_sum.reason, "scope_value_mismatch");

  // The lattice is part of the same gate: a warm document computed at another n recomputes too.
  ProbeRequest other_lattice = RadialRequest(config, -30.0, 0.0);
  other_lattice.sample_count = 10000;
  other_lattice.warm_survey = CraftedWarm(550.0, 20000);
  ProbeResult lattice_result;
  ASSERT_TRUE(AnalyzePathProbe(config, other_lattice, &lattice_result).Ok());
  ASSERT_EQ(lattice_result.profiles.size(), 1u);
  EXPECT_EQ(lattice_result.profiles.front().source, "recomputed");
}

// The edge-normal form: the window anchors at the nearest of the support endpoints and the
// certified edge (the edge, here, at 7.75 degrees from the at point vs the endpoint's 8.1), and a
// context with neither is the named refusal — never a guessed anchor.
TEST(ProbeAnalysis, EdgeNormalAnchorsAtNearestEdgeAndRefusesWithoutOne) {
  const ConfigManager config = survey::RandomPrismScene();
  ProbeRequest request = RadialRequest(config, -30.0, 0.0);
  request.along = ProbeAlong::kEdgeNormal;
  // The at point sits 7.75 degrees from the certified edge (22.25 on the grid) and 8.1 from the
  // support's lower endpoint: the edge wins the anchor.
  request.warm_survey = CraftedWarm(550.0, 20000);
  request.warm_survey->edge_deg = 22.25;

  ProbeResult result;
  Error e = AnalyzePathProbe(config, request, &result);
  ASSERT_TRUE(e.Ok()) << e.message;
  ASSERT_EQ(result.profiles.size(), 1u);
  const ProbeProfile& profile = result.profiles.front();
  EXPECT_EQ(profile.evidence_state, "finite");
  EXPECT_EQ(profile.source, "recomputed");  // only the radial form reuses the warm map
  EXPECT_EQ(profile.anchor_basis, "caustic_edge_position");
  EXPECT_DOUBLE_EQ(profile.anchor_deg, 22.25);
  EXPECT_DOUBLE_EQ(profile.window_half_width_deg, 10.0);
  EXPECT_NEAR(profile.delta_min_deg, 12.25, 1e-9);
  EXPECT_NEAR(profile.delta_max_deg, 32.25, 1e-9);
  EXPECT_EQ(profile.samples.size(), 200u);

  // The no-edge arm: a support-less context refuses by name.
  ProbeRequest refused = RadialRequest(config, -30.0, 0.0);
  refused.along = ProbeAlong::kEdgeNormal;
  refused.sample_count = kLattice;  // never reached: the refusal precedes any BandSum
  SurveyWarmSource empty_warm;
  empty_warm.from_survey_document = true;
  empty_warm.index_present = true;
  empty_warm.has_scope = true;
  empty_warm.wavelength_nm = 550.0;
  refused.warm_survey = empty_warm;
  ProbeResult refused_result;
  e = AnalyzePathProbe(config, refused, &refused_result);
  ASSERT_TRUE(e.Ok()) << e.message;
  ASSERT_EQ(refused_result.profiles.size(), 1u);
  EXPECT_EQ(refused_result.profiles.front().evidence_state, "refused");
  EXPECT_EQ(refused_result.profiles.front().refused_mode, "no_edge_in_context");
}

// The arc form: a constant-delta arc swept over azimuth at the asked deviation; the degenerate
// poles refuse by name.
TEST(ProbeAnalysis, ArcFormSweepsAzimuthAtTheAskedDeviation) {
  const ConfigManager config = survey::RandomPrismScene();
  ProbeRequest request = RadialRequest(config, -30.0, 0.0);  // the sun sits at (0, 0): delta = 30
  request.along = ProbeAlong::kArc;
  // A crafted warm context: the arc form never reuses the warm map, but the context comes from
  // the document, so the test pays no inline delta partition (the CI budget's unit).
  request.warm_survey = CraftedWarm(550.0, 20000);

  ProbeResult result;
  Error e = AnalyzePathProbe(config, request, &result);
  ASSERT_TRUE(e.Ok()) << e.message;
  ASSERT_EQ(result.profiles.size(), 1u);
  const ProbeProfile& profile = result.profiles.front();
  EXPECT_EQ(profile.evidence_state, "finite");
  EXPECT_EQ(profile.source, "recomputed");
  EXPECT_EQ(profile.samples.size(), 200u);
  for (const auto& sample : profile.samples) {
    EXPECT_NEAR(sample.delta_deg, 30.0, 1e-9);
  }
  EXPECT_NEAR(profile.samples.front().azimuth_deg, -9.95, 1e-9);
  EXPECT_NEAR(profile.samples.back().azimuth_deg, 9.95, 1e-9);

  // The readings sit on the sweep coordinate (the azimuth here): a position on the arc, never
  // the constant-delta echo — the same rule the dark decomposition's bounds follow.
  ASSERT_TRUE(profile.readings.has_edge);
  ASSERT_TRUE(profile.readings.has_peak);
  EXPECT_GE(profile.readings.edge_deg, -10.0);
  EXPECT_LE(profile.readings.edge_deg, 10.0);
  EXPECT_GE(profile.readings.peak_deg, -10.0);
  EXPECT_LE(profile.readings.peak_deg, 10.0);

  // The degenerate pole: an arc through the sun itself is not a circle there.
  ProbeRequest degenerate = RadialRequest(config, 0.0, 0.0);
  degenerate.along = ProbeAlong::kArc;
  // The crafted context, like the first request: without it the inline arm runs the delta
  // partition (~13 s locally, worse on CI) and the default budget interrupts it mid-shell —
  // the refusal is deviation-based and needs no inline assembly.
  degenerate.warm_survey = CraftedWarm(550.0, 20000);
  ProbeResult degenerate_result;
  e = AnalyzePathProbe(config, degenerate, &degenerate_result);
  ASSERT_TRUE(e.Ok()) << e.message;
  ASSERT_EQ(degenerate_result.profiles.size(), 1u);
  EXPECT_EQ(degenerate_result.profiles.front().evidence_state, "refused");
  EXPECT_EQ(degenerate_result.profiles.front().refused_mode, "degenerate_arc_at_pole");
}

// The refusal shapes: an out-of-range at point, an axis that maps to no pose density (the inline
// arm assembles the context; the profile refuses), and the warm_index_absent degradation.
TEST(ProbeAnalysis, RefusalsNameThemselves) {
  const ConfigManager config = survey::RandomPrismScene();
  ProbeRequest bad_at = RadialRequest(config, 99.0, 0.0);
  ProbeResult result;
  const Error e = AnalyzePathProbe(config, bad_at, &result);
  EXPECT_FALSE(e.Ok());
  EXPECT_EQ(ErrorCodeName(e.code), std::string("invalid_target"));

  // The fixed-pose plate: the inline context assembles (one member, cheap), the profile refuses
  // because the axis maps to no pose density.
  const ConfigManager plate = survey::FixedPosePlateScene(1.0);
  ProbeRequest plate_request = RadialRequest(plate, 0.0, 60.0);
  plate_request.at_altitude_deg = 0.0;  // the plate's sun is at (20, 0); delta 20
  ProbeResult plate_result;
  const Error plate_error = AnalyzePathProbe(plate, plate_request, &plate_result);
  ASSERT_TRUE(plate_error.Ok()) << plate_error.message;
  EXPECT_EQ(plate_result.context.source, "inline");
  ASSERT_EQ(plate_result.profiles.size(), 1u);
  EXPECT_EQ(plate_result.profiles.front().evidence_state, "refused");
  EXPECT_EQ(plate_result.profiles.front().refused_mode, "measure_not_expressible");

  // The warm_index_absent degradation: a schema-4 document without its warm_index block degrades
  // visibly to the inline context and never claims the warm band-sum.
  const ConfigManager cheap = survey::FixedPosePlateScene(1.0);
  ProbeRequest degraded = RadialRequest(cheap, 0.0, 60.0);
  SurveyWarmSource absent;
  absent.from_survey_document = true;
  absent.index_present = false;
  degraded.warm_survey = absent;
  ProbeResult degraded_result;
  const Error degraded_error = AnalyzePathProbe(cheap, degraded, &degraded_result);
  ASSERT_TRUE(degraded_error.Ok()) << degraded_error.message;
  EXPECT_EQ(degraded_result.context.source, "warm");
  EXPECT_TRUE(degraded_result.context.warm_index_absent);
  EXPECT_EQ(degraded_result.context.band_sum.state, "unevaluated");
  EXPECT_EQ(degraded_result.context.band_sum.reason, "warm_index_absent");

  // The fourth refusal mode, inline_survey_refused (the edge-normal form's naming when the
  // inline context assembly returns an Error, probe_analysis.cpp's edge_refused branch), has no
  // drivable arm: every AssembleSurvey error is either pre-empted by AnalyzePathProbe's own
  // validation (crystal id, wavelength band, sample count, at point) or produced by the
  // crystal/path resolution that the probe repeats right after the context block — its own
  // ResolveCrystalSceneScope with the same arguments — and reports as an Error before any
  // profile is built (the same ResolveSingleLayerPath face rule and BuildFaceNormals conversion
  // guard both calls). The mode is the fail-closed name for a future divergence between those
  // two calls, not a state the engine surface can reach today.
}

// The chromatic lambda grid lives on the line: one profile per requested wavelength, the warm
// reuse only where the scope matches, and the red-edge width when both edges exist.
TEST(ProbeAnalysis, MultiLambdaGridLivesOnTheLineAndYieldsTheRedEdgeWidth) {
  const ConfigManager config = survey::RandomPrismScene();
  ProbeRequest request = RadialRequest(config, -30.0, 0.0);
  request.wavelengths_nm = { 550.0, 650.0 };
  request.sample_count = 20000;  // the 650 profile recomputes at the tiny lattice
  request.warm_survey = CraftedWarm(550.0, 20000);

  ProbeResult result;
  const Error e = AnalyzePathProbe(config, request, &result);
  ASSERT_TRUE(e.Ok()) << e.message;
  ASSERT_EQ(result.profiles.size(), 2u);
  EXPECT_EQ(result.profiles[0].source, "warm_reused");
  EXPECT_EQ(result.profiles[1].source, "recomputed");
  EXPECT_DOUBLE_EQ(result.profiles[1].wavelength_nm, 650.0);
  EXPECT_TRUE(result.has_red_edge_width);
  EXPECT_DOUBLE_EQ(result.red_edge_width_deg,
                   result.profiles[1].readings.edge_deg - result.profiles[0].readings.edge_deg);
}

// ---- the C API gate (the bridge's own additions: null args, the struct_size gate, named
// refusals — the point entry's same trio, on the probe's public face) ------------------------------

constexpr const char* kProbeCApiScene = R"({
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

struct ProbeSceneDeleter {
  void operator()(LUMICE_Scene* s) const { LUMICE_SceneDestroy(s); }
};
using ProbeScenePtr = std::unique_ptr<LUMICE_Scene, ProbeSceneDeleter>;

ProbeScenePtr MakeProbeCApiScene() {
  LUMICE_Scene* s = nullptr;
  EXPECT_EQ(LUMICE_SceneFromJson(kProbeCApiScene, &s), LUMICE_OK);
  return ProbeScenePtr(s);
}

struct ProbeReq {
  std::vector<int> faces{ 3, 5 };
  std::vector<int> layers{ 2 };
  LUMICE_PathProbeRequest c{};
  ProbeReq() {
    c.struct_size = sizeof(LUMICE_PathProbeRequest);
    c.crystal_id = 1;
    c.along = LUMICE_PATH_PROBE_ALONG_RADIAL;
    c.at_altitude_deg = -30.0;
    c.at_azimuth_deg = 0.0;
    c.faces = faces.data();
    c.face_count = static_cast<int>(faces.size());
    c.layer_face_counts = layers.data();
    c.layer_count = static_cast<int>(layers.size());
  }
};

TEST(ProbeCApi, NullArgumentsAreRejected) {
  const ProbeScenePtr scene = MakeProbeCApiScene();
  ProbeReq req;
  LUMICE_PathProbe* out = nullptr;
  EXPECT_EQ(LUMICE_AnalyzePathProbe(nullptr, &req.c, &out, nullptr, 0), LUMICE_ERR_NULL_ARG);
  EXPECT_EQ(LUMICE_AnalyzePathProbe(scene.get(), nullptr, &out, nullptr, 0), LUMICE_ERR_NULL_ARG);
  EXPECT_EQ(LUMICE_AnalyzePathProbe(scene.get(), &req.c, nullptr, nullptr, 0), LUMICE_ERR_NULL_ARG);
  LUMICE_PathProbeDestroy(nullptr);  // a no-op
}

TEST(ProbeCApi, StructSizeSmallerThanThisVersionIsRejected) {
  const ProbeScenePtr scene = MakeProbeCApiScene();
  ProbeReq req;
  // One byte below the smallest size this build reads — the reader gate's own anchor, not a
  // fixed offset from sizeof (appended fields grow sizeof, they never lower the floor).
  req.c.struct_size = offsetof(LUMICE_PathProbeRequest, interrupted) - 1;
  LUMICE_PathProbe* out = nullptr;
  char err[512];
  std::memset(err, 'x', sizeof(err));
  EXPECT_EQ(LUMICE_AnalyzePathProbe(scene.get(), &req.c, &out, err, sizeof(err)), LUMICE_ERR_INVALID_VALUE);
  EXPECT_EQ(out, nullptr);
  EXPECT_NE(std::string(err).find("struct_size"), std::string::npos) << err;
}

TEST(ProbeCApi, UnknownAlongFormIsRefusedByName) {
  const ProbeScenePtr scene = MakeProbeCApiScene();
  ProbeReq req;
  req.c.along = 99;
  LUMICE_PathProbe* out = nullptr;
  char err[512];
  std::memset(err, 'x', sizeof(err));
  EXPECT_NE(LUMICE_AnalyzePathProbe(scene.get(), &req.c, &out, err, sizeof(err)), LUMICE_OK);
  EXPECT_EQ(out, nullptr);
  EXPECT_NE(std::string(err).find("along"), std::string::npos) << err;
}

// The schema-5 wire shape, pinned at v1 (append-only from here): the top-level key order, the
// spec's key set and extent echo, the two evidence shapes, and the dark-region keys — plus the
// produced-once contract (a second ToJson call is byte-identical).
TEST(ProbeAnalysis, Schema5JsonShapeIsPinnedAndProducedOnce) {
  const ConfigManager config = survey::RandomPrismScene();

  ProbeRequest finite_request = RadialRequest(config, -30.0, 0.0);
  finite_request.wavelengths_nm = { 550.0 };
  finite_request.sample_count = 20000;
  finite_request.warm_survey = CraftedWarm(550.0, 20000);
  ProbeResult finite_result;
  ASSERT_TRUE(AnalyzePathProbe(config, finite_request, &finite_result).Ok());

  ProbeRequest refused_request = RadialRequest(config, -30.0, 0.0);
  refused_request.along = ProbeAlong::kEdgeNormal;
  // A warm document that certifies neither a support nor an edge: the form's named refusal,
  // without paying the inline arm's delta partition (the CI budget's unit).
  SurveyWarmSource empty_warm = CraftedWarm(550.0, 20000);
  empty_warm.intervals.clear();
  empty_warm.has_support = false;
  empty_warm.has_edge = false;
  refused_request.warm_survey = empty_warm;
  ProbeResult refused_result;
  ASSERT_TRUE(AnalyzePathProbe(config, refused_request, &refused_result).Ok());

  const std::string finite_json = ProbeToJson(finite_result, "test");
  EXPECT_EQ(finite_json, ProbeToJson(finite_result, "test"));  // produced once, byte-identical

  const nlohmann::ordered_json doc = nlohmann::ordered_json::parse(finite_json);
  EXPECT_EQ(doc["schema"], "lumice.path-probe");
  EXPECT_EQ(doc["schema_version"], 5);
  static const char* kTopKeys[] = { "schema", "schema_version", "generator", "conventions",
                                    "meta",   "context",        "profiles" };
  size_t k = 0;
  for (const auto& item : doc.items()) {
    if (k >= std::size(kTopKeys)) {
      ADD_FAILURE() << "more top-level keys than the pinned v1 set: " << item.key();
      break;
    }
    EXPECT_EQ(item.key(), kTopKeys[k++]);
  }
  EXPECT_EQ(k, std::size(kTopKeys));  // no derived on this request: one wavelength

  const nlohmann::ordered_json profile = doc.at("profiles").at(0);
  EXPECT_EQ(profile["along"], "radial");
  EXPECT_EQ(profile["source"], "warm_reused");
  static const char* kSpecKeys[] = { "delta_min_deg",    "delta_max_deg",        "azimuth_deg", "window_half_width_deg",
                                     "bin_deg",          "wedge_half_width_deg", "semantics",   "wavelength_nm",
                                     "refractive_index", "sample_count",         "lattice",     "deterministic",
                                     "representative",   "member_count" };
  size_t s = 0;
  for (const auto& item : profile["spec"].items()) {
    if (s >= std::size(kSpecKeys)) {
      ADD_FAILURE() << "more spec keys than the pinned v1 set: " << item.key();
      break;
    }
    EXPECT_EQ(item.key(), kSpecKeys[s++]);
  }
  EXPECT_EQ(s, std::size(kSpecKeys));
  EXPECT_DOUBLE_EQ(profile["spec"]["delta_min_deg"].get<double>(), 0.0);
  EXPECT_DOUBLE_EQ(profile["spec"]["delta_max_deg"].get<double>(), 180.0);
  EXPECT_EQ(profile["evidence"]["state"], "finite");
  EXPECT_EQ(profile["evidence"]["coverage"]["members_evaluated"], 1);
  EXPECT_EQ(profile["evidence"]["coverage"]["lattice_n"], 20000);
  ASSERT_FALSE(profile["samples"].empty());
  EXPECT_TRUE(profile.at("samples").at(0).contains("delta_deg"));
  EXPECT_TRUE(profile.at("samples").at(0).contains("azimuth_deg"));
  ASSERT_FALSE(profile["dark_regions"].empty());
  EXPECT_TRUE(profile.at("dark_regions").at("segments").at(0).contains("kind"));
  EXPECT_TRUE(profile.at("dark_regions").at("segments").at(0).contains("lo_deg"));
  EXPECT_TRUE(profile.at("dark_regions").at("segments").at(0).contains("hi_deg"));
  EXPECT_TRUE(profile.at("dark_regions").at("segments").at(0).contains("bin_count"));

  const nlohmann::ordered_json refused_doc = nlohmann::ordered_json::parse(ProbeToJson(refused_result, "test"));
  const nlohmann::ordered_json refused_profile = refused_doc.at("profiles").at(0);
  EXPECT_EQ(refused_profile["evidence"]["state"], "refused");
  EXPECT_EQ(refused_profile["evidence"]["mode"], "no_edge_in_context");
  EXPECT_FALSE(refused_profile.contains("samples"));
  // The refusal shell carries no source (nothing was reused or computed — the empty string is
  // a value the vocabulary never named) and its undetermined extent is null, not a written 0.
  EXPECT_FALSE(refused_profile.contains("source"));
  EXPECT_TRUE(refused_profile.at("spec").at("delta_min_deg").is_null());
  EXPECT_TRUE(refused_profile.at("spec").at("delta_max_deg").is_null());

  // The arc form's spec echoes its azimuth sweep's half width — "0 = the full axis" belongs to
  // the axis forms alone, and a spec that cannot reconstruct its own sweep is not a caliber.
  ProbeRequest arc_request = RadialRequest(config, -30.0, 0.0);
  arc_request.along = ProbeAlong::kArc;
  arc_request.wavelengths_nm = { 550.0 };
  arc_request.sample_count = 20000;
  arc_request.warm_survey = CraftedWarm(550.0, 20000);
  ProbeResult arc_result;
  ASSERT_TRUE(AnalyzePathProbe(config, arc_request, &arc_result).Ok());
  const nlohmann::ordered_json arc_doc = nlohmann::ordered_json::parse(ProbeToJson(arc_result, "test"));
  EXPECT_DOUBLE_EQ(arc_doc.at("profiles").at(0).at("spec").at("window_half_width_deg").get<double>(),
                   kProbeWindowHalfWidthDeg);
  EXPECT_EQ(arc_doc.at("profiles").at(0).at("evidence").at("state"), "finite");
}

}  // namespace
}  // namespace lumice::raypath
