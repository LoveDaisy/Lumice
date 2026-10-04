#include <gtest/gtest.h>

#include <cmath>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>

#include "analytic/path_feature_discovery.hpp"

namespace lumice::analytic {
namespace {

constexpr double kPi = 3.14159265358979323846;

SphericalFieldQuery Query() {
  return { { 0, 0, 1 }, { { { 1, 0, 0 }, { 0, 1, 0 } } }, 0.5 };
}

TEST(PathFeatureField, NormalizedKernelAndCovariantHessian) {
  const std::vector<WeightedSkySample> samples = { { 0, { 0, 0, 1 }, { 2, 3, 5 } } };
  SphericalFieldValue out;
  ASSERT_TRUE(EvaluateSphericalField(samples, Query(), &out));
  // Integral exp(k*cos(theta)) over the sphere = 4*pi*sinh(k)/k.
  const double peak = 4 / (2 * kPi * (1 - std::exp(-8)));
  EXPECT_NEAR(out.xyz[1].value, 3 * peak, 1e-14);
  EXPECT_DOUBLE_EQ(out.xyz[1].gradient[0], 0);
  EXPECT_DOUBLE_EQ(out.xyz[1].gradient[1], 0);
  EXPECT_NEAR(out.xyz[1].hessian[0], -12 * peak, 1e-13);
  EXPECT_NEAR(out.xyz[1].hessian[2], -12 * peak, 1e-13);
  EXPECT_DOUBLE_EQ(out.xyz[1].hessian[1], 0);
  EXPECT_DOUBLE_EQ(out.effective_samples_y, 1);
}

TEST(PathFeatureField, OffAxisDerivativesAndBasisRotation) {
  const std::vector<WeightedSkySample> samples = { { 0, { .6, 0, .8 }, { 1, 2, 4 } } };
  auto query = Query();
  SphericalFieldValue out;
  ASSERT_TRUE(EvaluateSphericalField(samples, query, &out));
  const double f = 2 * 4 / (2 * kPi * (1 - std::exp(-8))) * std::exp(-.8);
  EXPECT_NEAR(out.xyz[1].gradient[0], f * 2.4, 1e-14);
  EXPECT_NEAR(out.xyz[1].hessian[0], f * (2.4 * 2.4 - 3.2), 1e-14);
  EXPECT_NEAR(out.xyz[1].hessian[2], -f * 3.2, 1e-14);
  const double u = std::sqrt(.5);
  query.basis = { { { u, u, 0 }, { -u, u, 0 } } };
  SphericalFieldValue rotated;
  ASSERT_TRUE(EvaluateSphericalField(samples, query, &rotated));
  EXPECT_NEAR(rotated.xyz[1].gradient[0], u * out.xyz[1].gradient[0], 1e-14);
  EXPECT_NEAR(rotated.xyz[1].gradient[1], -u * out.xyz[1].gradient[0], 1e-14);
  EXPECT_NEAR(rotated.xyz[1].hessian[1], .5 * (out.xyz[1].hessian[2] - out.xyz[1].hessian[0]), 1e-14);
}

TEST(PathFeatureField, SameColourCanHaveStrongLuminanceGradient) {
  const std::vector<WeightedSkySample> samples = { { 0, { 0, 0, 1 }, { 2, 3, 5 } },
                                                   { 1, { .6, 0, .8 }, { 4, 6, 10 } } };
  SphericalFieldValue out;
  ASSERT_TRUE(EvaluateSphericalField(samples, Query(), &out));
  EXPECT_GT(out.xyz[1].gradient[0], 1);
  EXPECT_TRUE(out.chromaticity_available);
  EXPECT_NEAR(out.xy[0].value, .2, 1e-15);
  EXPECT_NEAR(out.xy[1].value, .3, 1e-15);
  for (const auto& jet : out.xy) {
    for (double value : jet.gradient) {
      EXPECT_NEAR(value, 0, 1e-14);
    }
    for (double value : jet.hessian) {
      EXPECT_NEAR(value, 0, 1e-14);
    }
  }
}

TEST(PathFeatureField, BothChromaticCoordinatesAndQuotientHessian) {
  // Equal z fractions but different x/y: a scalar blue fraction would miss it.
  const std::vector<WeightedSkySample> samples = { { 0, { 0, 0, 1 }, { 2, 4, 4 } }, { 1, { .6, 0, .8 }, { 4, 2, 4 } } };
  SphericalFieldValue out;
  ASSERT_TRUE(EvaluateSphericalField(samples, Query(), &out));
  EXPECT_GT(out.xy[0].gradient[0], .1);
  EXPECT_NEAR(out.xy[0].gradient[0], -out.xy[1].gradient[0], 1e-14);
  // Direct scalar formula in a geodesic, independent of the jet quotient code.
  const auto x = [](double theta) {
    const double a = std::exp(4 * std::cos(theta));
    const double b = std::exp(4 * (.6 * std::sin(theta) + .8 * std::cos(theta)));
    return (.2 * a + .4 * b) / (a + b);
  };
  const double step = 1e-4;
  EXPECT_NEAR(out.xy[0].gradient[0], (x(step) - x(-step)) / (2 * step), 1e-8);
  EXPECT_NEAR(out.xy[0].hessian[0], (x(step) - 2 * x(0) + x(-step)) / (step * step), 1e-7);
}

TEST(PathFeatureField, CorrelatedSpectralRowsDoNotInflateEffectiveCount) {
  std::vector<WeightedSkySample> samples = { { 4, { 0, 0, 1 }, { 1, 1, 1 } },
                                             { 4, { 0, 0, 1 }, { 1, 1, 1 } },
                                             { 7, { 0, 0, 1 }, { 1, 2, 1 } } };
  SphericalFieldValue out;
  ASSERT_TRUE(EvaluateSphericalField(samples, Query(), &out));
  EXPECT_DOUBLE_EQ(out.effective_samples_y, 2);
  for (auto& row : samples) {
    for (double& weight : row.xyz_weight) {
      weight *= 1e-160;
    }
  }
  ASSERT_TRUE(EvaluateSphericalField(samples, Query(), &out));
  EXPECT_DOUBLE_EQ(out.effective_samples_y, 2);
}

TEST(PathFeatureField, EmptyIsUnobservedAndInvalidInputClearsOutput) {
  SphericalFieldValue out;
  ASSERT_TRUE(EvaluateSphericalField({}, Query(), &out));
  EXPECT_FALSE(out.chromaticity_available);
  EXPECT_DOUBLE_EQ(out.xyz[1].value, 0);
  const std::vector<WeightedSkySample> samples = { { 0, { 0, 0, 1 }, { 1, 1, 1 } } };
  ASSERT_TRUE(EvaluateSphericalField(samples, Query(), &out));
  auto invalid = Query();
  invalid.basis[1] = invalid.basis[0];
  EXPECT_FALSE(EvaluateSphericalField(samples, invalid, &out));
  EXPECT_FALSE(out.chromaticity_available);
  EXPECT_DOUBLE_EQ(out.xyz[1].value, 0);
  invalid = Query();
  invalid.bandwidth_rad = std::numeric_limits<double>::quiet_NaN();
  EXPECT_FALSE(EvaluateSphericalField(samples, invalid, &out));
  EXPECT_FALSE(EvaluateSphericalField({ { 0, { 0, 0, 0 }, { 1, 1, 1 } } }, Query(), &out));
  EXPECT_FALSE(EvaluateSphericalField({ { 0, { 0, 0, 1 }, { -1, 1, 1 } } }, Query(), &out));
  EXPECT_FALSE(
      EvaluateSphericalField({ { 1, { 0, 0, 1 }, { 1, 1, 1 } }, { 0, { 0, 0, 1 }, { 1, 1, 1 } } }, Query(), &out));
}

// Independent trapezoidal angular integration of explicit rotating directions.
// It uses neither Bessel functions nor the product point-kernel implementation.
SphericalJet AngularOracle(double c, const SphericalFieldQuery& query, int count) {
  SphericalJet out;
  const double k = 1 / (query.bandwidth_rad * query.bandwidth_rad);
  const double normalization = k / (2 * kPi * -std::expm1(-2 * k)) / count;
  for (int i = 0; i < count; ++i) {
    const double phi = 2 * kPi * (i + .5) / count;
    const double radius = std::sqrt((1 - c) * (1 + c));
    const std::array<double, 3> d{ radius * std::cos(phi), radius * std::sin(phi), c };
    double dot = 0;
    double a = 0;
    double b = 0;
    for (int j = 0; j < 3; ++j) {
      dot += d[j] * query.direction[j];
      a += d[j] * query.basis[0][j];
      b += d[j] * query.basis[1][j];
    }
    const double w = normalization * std::exp(k * (dot - 1));
    out.value += w;
    out.gradient[0] += k * w * a;
    out.gradient[1] += k * w * b;
    out.hessian[0] += w * (k * k * a * a - k * dot);
    out.hessian[1] += w * k * k * a * b;
    out.hessian[2] += w * (k * k * b * b - k * dot);
  }
  return out;
}

TEST(PathFeatureField, UniformOrbitMatchesIndependentAngularJets) {
  // Includes the Bessel branch junction, narrow kernels, orbit/query poles,
  // and broad kernels. Error is measured on the actual derivative scale.
  for (double h : { .005, .07, std::sqrt(.64 / 50.0), .2, 2.0, 20.0 }) {
    for (double c : { -.6, 0.0, .6, 1.0 }) {
      for (double theta : { 0.0, .8, 1.2, kPi }) {
        const SphericalFieldQuery query{ { std::sin(theta), 0, std::cos(theta) },
                                         { { { std::cos(theta), 0, -std::sin(theta) }, { 0, 1, 0 } } },
                                         h };
        const WeightedSkySample row{
          0, { std::sqrt((1 - c) * (1 + c)), 0, c }, { 1, 1, 1 }, std::array<double, 3>{ 0, 0, 1 }, 42
        };
        SphericalFieldValue result;
        if (!EvaluateSphericalField({ row }, query, &result)) {
          ADD_FAILURE() << "valid uniform orbit rejected";
          return;
        }
        const auto expected = AngularOracle(c, query, 32768);
        const double scale = std::max(expected.value, 1e-250);
        EXPECT_NEAR(result.xyz[1].value, expected.value, scale * 2e-10);
        for (int j = 0; j < 2; ++j) {
          EXPECT_NEAR(result.xyz[1].gradient[j], expected.gradient[j], scale / h * 2e-8);
        }
        for (int j = 0; j < 3; ++j) {
          EXPECT_NEAR(result.xyz[1].hessian[j], expected.hessian[j], scale / (h * h) * 2e-8);
        }
      }
    }
  }
}

TEST(PathFeatureField, OrbitIsNormalizedAndPreservesRotationalInvariance) {
  const WeightedSkySample row{ 9, { .8, 0, .6 }, { 2, 3, 5 }, std::array<double, 3>{ 0, 0, 1 }, 73 };
  double integral = 0;
  constexpr int kCount = 4096;
  for (int i = 0; i < kCount; ++i) {
    const double z = -1 + 2.0 * (i + .5) / kCount;
    const double r = std::sqrt((1 - z) * (1 + z));
    const SphericalFieldQuery query{ { r, 0, z }, { { { z, 0, -r }, { 0, 1, 0 } } }, .12 };
    SphericalFieldValue out;
    if (!EvaluateSphericalField({ row }, query, &out)) {
      ADD_FAILURE();
      return;
    }
    integral += out.xyz[1].value * 4 * kPi / kCount;
    auto rotated = query;
    rotated.direction = { 0, r, z };
    rotated.basis = { { { 0, z, -r }, { -1, 0, 0 } } };
    SphericalFieldValue other;
    if (!EvaluateSphericalField({ row }, rotated, &other)) {
      ADD_FAILURE();
      return;
    }
    EXPECT_NEAR(out.xyz[1].value, other.xyz[1].value, 1e-13);
    EXPECT_NEAR(out.xyz[1].gradient[0], other.xyz[1].gradient[0], 1e-12);
    EXPECT_NEAR(out.xyz[1].hessian[0], other.xyz[1].hessian[0], 1e-11);
  }
  EXPECT_NEAR(integral, 3, 1e-7);
}

TEST(PathFeatureField, OrbitSubdivisionDoesNotManufactureOuterEvidence) {
  const WeightedSkySample row{ 4, { .8, 0, .6 }, { 2, 3, 5 }, std::array<double, 3>{ 0, 0, 1 }, 73 };
  const WeightedSkySample other{ 5, { 0, 0, 1 }, { 1, 2, 3 } };
  SphericalFieldValue original;
  ASSERT_TRUE(EvaluateSphericalField({ row, other }, Query(), &original));
  std::vector<WeightedSkySample> split(32, row);
  for (auto& part : split) {
    for (auto& weight : part.xyz_weight) {
      weight /= 32;
    }
  }
  split.push_back(other);
  SphericalFieldValue out;
  ASSERT_TRUE(EvaluateSphericalField(split, Query(), &out));
  EXPECT_EQ(out.outer_sample_count, 2u);
  EXPECT_EQ(original.outer_sample_count, out.outer_sample_count);
  EXPECT_NEAR(original.effective_samples_y, out.effective_samples_y, 1e-13);
  EXPECT_NEAR(original.xyz[1].value, out.xyz[1].value, 1e-13);
  EXPECT_NEAR(original.xyz[1].hessian[0], out.xyz[1].hessian[0], 1e-12);
  EXPECT_EQ(split[0].source_token, 73u);
  // Integrating the spin is NOT a valid replacement for an anisotropic measure.
  auto point = row;
  point.uniform_orbit_axis.reset();
  SphericalFieldValue orbit;
  SphericalFieldValue anisotropic;
  auto query = Query();
  query.direction = { 0, .8, .6 };
  query.basis = { { { 1, 0, 0 }, { 0, .6, -.8 } } };
  ASSERT_TRUE(EvaluateSphericalField({ row }, query, &orbit));
  ASSERT_TRUE(EvaluateSphericalField({ point }, query, &anisotropic));
  EXPECT_GT(std::abs(orbit.xyz[1].value - anisotropic.xyz[1].value), .1);
  point.uniform_orbit_axis = std::array<double, 3>{ 0, 0, 2 };
  EXPECT_FALSE(EvaluateSphericalField({ point }, query, &out));
  EXPECT_EQ(out.outer_sample_count, 0u);
}

TEST(PathFeatureField, ContinuousPeakAndRidgeAreNumericalNotPhysicalClaims) {
  const std::vector<WeightedSkySample> point{ { 0, { 0, 0, 1 }, { 1, 2, 3 } } };
  FieldSolveOptions options{ FieldEquation::kLogYPeak, 0, .1, 1e-10, .05, 32 };
  auto peak = CorrectSphericalField(point, { .1, 0, std::sqrt(.99) }, options, nullptr);
  ASSERT_EQ(peak.status, FieldSolveStatus::kConverged);
  EXPECT_NEAR(peak.query.direction[0], 0, 1e-10);
  EXPECT_NEAR(peak.query.direction[2], 1, 1e-14);
  EXPECT_LT(peak.log_y_curvatures[1], 0);
  const std::vector<WeightedSkySample> ring{ { 0, { 1, 0, 0 }, { 1, 2, 3 }, std::array<double, 3>{ 0, 0, 1 }, 31 } };
  options.equation = FieldEquation::kLogYRidge;
  const auto ridge = CorrectSphericalField(ring, { std::sqrt(.99), 0, .1 }, options, nullptr);
  ASSERT_EQ(ridge.status, FieldSolveStatus::kConverged);
  EXPECT_NEAR(ridge.query.direction[2], 0, 1e-10);
  EXPECT_LT(ridge.log_y_curvatures[0], -90);
  EXPECT_NEAR(ridge.log_y_curvatures[1], 0, 1e-10);
  // A single outer draw can yield a perfectly solved ring; that is not evidence
  // of a converged physical integral or a positive-mass atom.
  EXPECT_DOUBLE_EQ(ridge.field.effective_samples_y, 1);
  options.max_iterations = 1;
  const auto short_run = CorrectSphericalField(ring, { std::sqrt(.99), 0, .1 }, options, nullptr);
  EXPECT_EQ(short_run.status, FieldSolveStatus::kIterationLimit);
  EXPECT_GT(short_run.correction_rad, options.tolerance_rad);
  EXPECT_NEAR(short_run.query.direction[2], .1, 1e-15);
  EXPECT_EQ(CorrectSphericalField({}, { 1, 0, 0 }, options, nullptr).status, FieldSolveStatus::kNoSignal);
}

TEST(PathFeatureField, ContinuousChromaticityAndRealInnerBudgetStops) {
  const double angle = .2;
  const std::vector<WeightedSkySample> samples{ { 0, { std::sin(angle), 0, std::cos(angle) }, { 4, 2, 4 } },
                                                { 1, { -std::sin(angle), 0, std::cos(angle) }, { 2, 4, 4 } } };
  const FieldSolveOptions options{ FieldEquation::kChromaticityX, .3, .15, 1e-10, .05, 32 };
  const auto root = CorrectSphericalField(samples, { .05, 0, std::sqrt(1 - .05 * .05) }, options, nullptr);
  ASSERT_EQ(root.status, FieldSolveStatus::kConverged);
  EXPECT_NEAR(root.query.direction[0], 0, 1e-10);
  EXPECT_NEAR(root.field.xy[0].value, .3, 1e-10);
  EXPECT_NEAR(root.field.xy[1].value, .3, 1e-10);
  FieldWorkBudget budget;
  budget.max_component_evaluations = 1;
  const auto stopped = CorrectSphericalField(samples, { 0, 0, 1 }, options, &budget);
  EXPECT_EQ(stopped.status, FieldSolveStatus::kBudgetExceeded);
  EXPECT_TRUE(budget.exhausted);
  EXPECT_EQ(budget.component_evaluations, 0u);
  budget = {};
  budget.max_component_evaluations = 1000;
  budget.deadline = std::chrono::steady_clock::now();
  SphericalFieldValue out;
  EXPECT_FALSE(EvaluateSphericalField(samples, Query(), &out, &budget));
  EXPECT_TRUE(budget.exhausted);
  EXPECT_EQ(budget.component_evaluations, 0u);
  budget = {};
  budget.max_component_evaluations = 512;
  const std::vector<WeightedSkySample> many(2048, samples.front());
  EXPECT_FALSE(EvaluateSphericalField(many, Query(), &out, &budget));
  EXPECT_TRUE(budget.exhausted);
  EXPECT_EQ(budget.component_evaluations, 512u);
  EXPECT_DOUBLE_EQ(out.xyz[1].value, 0);
}

TEST(PathFeatureField, ContinuousWalkKeepsClosureCensoringAndBudgetDistinct) {
  const std::vector<WeightedSkySample> ring{ { 0, { 1, 0, 0 }, { 1, 2, 3 }, std::array<double, 3>{ 0, 0, 1 }, 31 } };
  FieldWalkOptions options{ { FieldEquation::kLogYRidge, 0, .1, 1e-10, .05, 32 }, .05, 0, 140 };
  const auto closed = TraceSphericalField(ring, { 1, 0, 0 }, options, false, nullptr);
  ASSERT_EQ(closed.stop, FieldWalkStop::kClosed);
  EXPECT_GT(closed.points.size(), 120u);
  for (const auto& point : closed.points) {
    EXPECT_EQ(point.status, FieldSolveStatus::kConverged);
    EXPECT_NEAR(point.query.direction[2], 0, 1e-10);
  }
  options.minimum_y = 2 * closed.points.front().field.xyz[1].value;
  const auto censored = TraceSphericalField(ring, { 1, 0, 0 }, options, false, nullptr);
  EXPECT_EQ(censored.stop, FieldWalkStop::kObservationCensored);
  EXPECT_TRUE(censored.points.empty());
  EXPECT_EQ(censored.terminal.status, FieldSolveStatus::kConverged);
  options.minimum_y = 0;
  options.max_points = 3;
  const auto partial = TraceSphericalField(ring, { 1, 0, 0 }, options, true, nullptr);
  EXPECT_EQ(partial.stop, FieldWalkStop::kPointLimit);
  EXPECT_EQ(partial.points.size(), 3u);
  EXPECT_LT(partial.points[1].query.direction[1] * closed.points[1].query.direction[1], 0);
  FieldWorkBudget budget;
  budget.max_component_evaluations = 2;
  const auto stopped = TraceSphericalField(ring, { 1, 0, 0 }, options, false, &budget);
  EXPECT_EQ(stopped.stop, FieldWalkStop::kCorrectorFailed);
  EXPECT_EQ(stopped.terminal.status, FieldSolveStatus::kBudgetExceeded);
  EXPECT_EQ(stopped.points.size(), 2u);
}

TEST(PathFeatureField, IndependentPhysicalCloudFixture) {
  std::ifstream in(std::string(LUMICE_DIAGNOSTIC_FIXTURE_DIR) + "/weighted-sky.json");
  ASSERT_TRUE(in.is_open());
  const auto fixture = nlohmann::json::parse(in);
  ASSERT_EQ(fixture.at("symmetry_semantics"), "none");
  std::vector<WeightedSkySample> samples;
  for (const auto& row : fixture.at("samples")) {
    samples.push_back({ row.at("sample_index").get<uint64_t>(), row.at("direction").get<std::array<double, 3>>(),
                        row.at("xyz_weight").get<std::array<double, 3>>() });
  }
  EXPECT_GT(samples.size(), 0u);
  EXPECT_GT(fixture.at("queries").size(), 0u);
  for (const auto& row : fixture.at("queries")) {
    const SphericalFieldQuery query{ row.at("direction").get<std::array<double, 3>>(),
                                     row.at("basis").get<std::array<std::array<double, 3>, 2>>(),
                                     row.at("bandwidth_rad").get<double>() };
    SphericalFieldValue result;
    if (!EvaluateSphericalField(samples, query, &result)) {
      ADD_FAILURE() << "valid physical cloud rejected";
      return;
    }
    for (int c = 0; c < 5; ++c) {
      const auto& expected = c < 3 ? row.at("xyz").at(c) : row.at("xy").at(c - 3);
      const auto& jet = c < 3 ? result.xyz[c] : result.xy[c - 3];
      const double actual[] = { jet.value,      jet.gradient[0], jet.gradient[1],
                                jet.hessian[0], jet.hessian[1],  jet.hessian[2] };
      for (int k = 0; k < 6; ++k) {
        const double value = expected.at(k).get<double>();
        EXPECT_NEAR(actual[k], value, 1e-10 * std::abs(value) + 1e-13) << "channel " << c << " term " << k;
      }
    }
    EXPECT_NEAR(result.effective_samples_y, row.at("effective_samples_y").get<double>(), 1e-12);
  }
}

}  // namespace
}  // namespace lumice::analytic
