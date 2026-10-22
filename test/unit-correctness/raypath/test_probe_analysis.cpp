#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

#include "raypath/probe_analysis.hpp"
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

  // The degenerate pole: an arc through the sun itself is not a circle there.
  ProbeRequest degenerate = RadialRequest(config, 0.0, 0.0);
  degenerate.along = ProbeAlong::kArc;
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

}  // namespace
}  // namespace lumice::raypath
