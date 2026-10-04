#include <gtest/gtest.h>

#include <cmath>
#include <limits>

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

TEST(PathFeatureField, SameColourIsNotALuminanceOrChromaticityEdge) {
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

}  // namespace
}  // namespace lumice::analytic
