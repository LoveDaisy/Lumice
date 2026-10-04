#include <cmath>

#include "core/color_util.hpp"
#include "core/geo3d.hpp"
#include "core/wl_stratifier.hpp"
#include "gtest/gtest.h"
#include "raypath/product_input_assembly.hpp"
#include "util/illuminant.hpp"

namespace {
namespace ns = lumice;
namespace rp = lumice::raypath;

TEST(ProductInputSpectrum, DiscreteWeightsAreChargedOnceAndMonoKeepsChromaticity) {
  ns::LightSourceConfig light{};
  light.spectrum_ = std::vector<ns::WlParam>{ { 450.f, 2.f }, { 550.f, .3f }, { 650.f, 4.f } };
  rp::AssembledSpectrum output;
  ASSERT_TRUE(rp::AssembleDiscreteSpectrum(light, &output).Ok());
  ASSERT_EQ(output.rows.size(), 3u);
  const auto& config = std::get<std::vector<ns::WlParam>>(light.spectrum_);
  for (size_t i = 0; i < config.size(); ++i) {
    const auto& row = output.rows[i];
    const int index = static_cast<int>(config[i].wl_) - ns::kCmfMinWavelength;
    EXPECT_EQ(row.coefficient[0], static_cast<double>(config[i].weight_) * ns::kCmfX[index]);
    EXPECT_EQ(row.coefficient[1], static_cast<double>(config[i].weight_) * ns::kCmfY[index]);
    EXPECT_EQ(row.coefficient[2], static_cast<double>(config[i].weight_) * ns::kCmfZ[index]);
    EXPECT_EQ(row.measure_mass, 1);
  }
  ASSERT_TRUE(rp::AssembleSampledSpectrum(light, { 450.f, 0, "host discrete slot 0" }, &output).Ok());
  EXPECT_EQ(output.rows.front().source_weight, 2);
  EXPECT_FALSE(rp::AssembleSampledSpectrum(light, { 450.f, 1, "wrong slot" }, &output).Ok());
  EXPECT_TRUE(output.rows.empty());
  light.spectrum_ = std::vector<ns::WlParam>{ { 550.f, 2.f } };
  ASSERT_TRUE(rp::AssembleDiscreteSpectrum(light, &output).Ok());
  ASSERT_EQ(output.rows.size(), 1u);
  const auto c = output.rows.front().coefficient;
  const double sum = c[0] + c[1] + c[2];
  EXPECT_NEAR(c[0] / sum, (7 * c[0]) / (7 * sum), 1e-15);
  EXPECT_NEAR(c[1] / sum, (7 * c[1]) / (7 * sum), 1e-15);
}

TEST(ProductInputSpectrum, ContinuousSampleDoesNotBecomeQuadratureOrConsumeRng) {
  ns::LightSourceConfig light{};
  light.spectrum_ = ns::IlluminantType::kD65;
  ns::RandomNumberGenerator rng(55), reference(55);
  ns::WavelengthStratifier clock;
  const float shift = reference.GetUniform();
  const float wl = 380.f + 400.f * clock.NextUnit(rng);
  EXPECT_EQ(wl, 380.f + 400.f * shift);
  rp::AssembledSpectrum out;
  ASSERT_TRUE(rp::AssembleSampledSpectrum(light, { wl, std::nullopt, "host batch 0" }, &out).Ok());
  EXPECT_FALSE(out.quadrature.has_value());
  EXPECT_EQ(out.rows.front().measure_mass, 1);
  EXPECT_EQ(out.rows.front().source_weight, ns::GetIlluminantSpd(ns::IlluminantType::kD65, wl));
  EXPECT_EQ(rng.GetUniform(), reference.GetUniform());
}

TEST(ProductInputSpectrum, QuadratureUsesProbabilityMassNotNanometers) {
  ns::LightSourceConfig light{};
  light.spectrum_ = ns::IlluminantType::kE;
  const rp::SpectrumQuadrature q{
    { { 430.f, .25 }, { 530.f, .25 }, { 630.f, .25 }, { 730.f, .25 } }, "four midpoints", 4, std::nullopt
  };
  rp::AssembledSpectrum out;
  ASSERT_TRUE(rp::AssembleSpectrumQuadrature(light, q, &out).Ok());
  ASSERT_TRUE(out.quadrature.has_value());
  EXPECT_FALSE(out.quadrature->estimated_error.has_value());
  double mass = 0;
  for (const auto& row : out.rows) {
    mass += row.measure_mass * row.source_weight;
    EXPECT_EQ(row.source_weight, 1);
    EXPECT_EQ(row.coefficient[1], .25 * ns::kCmfY[static_cast<int>(row.wavelength_nm) - ns::kCmfMinWavelength]);
  }
  EXPECT_EQ(mass, 1);
  auto invalid = q;
  invalid.evaluation_budget = 3;
  EXPECT_FALSE(rp::AssembleSpectrumQuadrature(light, invalid, &out).Ok());
  EXPECT_TRUE(out.rows.empty());
}

TEST(ProductInputSource, ActualCapDrawMatchesProductionAndKeepsDomain) {
  ns::SunParam sun{ 21.f, 33.f, 8.f };
  auto& rng = ns::RandomNumberGenerator::GetInstance();
  rng.SetSeed(732);
  ns::RandomNumberGenerator replay(732);
  rp::ProductSourceSample sample{ { replay.GetUniform(), replay.GetUniform() }, "source draw 0" };
  rp::AssembledSource out;
  ASSERT_TRUE(rp::AssembleSource(sun, sample, &out).Ok());
  float actual[3];
  ns::SampleSphCapPoint(213.f, -21.f, 4.f, actual);
  double dot = 0;
  for (int j = 0; j < 3; ++j) {
    EXPECT_EQ(actual[j], out.product_direction[j]);
    dot += out.center_direction[j] * out.incident_direction[j];
  }
  EXPECT_GE(dot, std::cos(4.0 * 3.141592653589793 / 180));
  EXPECT_EQ(out.domain.diameter_, 8.f);
  EXPECT_EQ(rng.GetUniform(), replay.GetUniform());
  EXPECT_TRUE(ns::analytic::ValidateUnitVector(out.incident_direction.data()));
}
}  // namespace
