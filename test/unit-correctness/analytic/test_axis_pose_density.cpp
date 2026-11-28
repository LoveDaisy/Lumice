#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#include "analytic/axis_pose_density.hpp"
#include "analytic/pose_density.hpp"
#include "core/lat_lut.hpp"
#include "core/math.hpp"
#include "core/random.hpp"

namespace lumice::analytic {
namespace {

constexpr double kPi = math::kPiD;
constexpr double kDeg = math::kDegreeToRadD;
constexpr double kTurn = 2 * kPi;
constexpr double kHalfPi = kPi / 2;
constexpr DistributionType kTypes[] = { DistributionType::kNoRandom,  DistributionType::kUniform,
                                        DistributionType::kGaussian,  DistributionType::kZigzag,
                                        DistributionType::kLaplacian, DistributionType::kGaussianLegacy };

AxisDistribution Axis(DistributionType latitude = DistributionType::kGaussian, float center = 30, float spread = 12) {
  AxisDistribution d;
  d.latitude_dist = { latitude, center, spread };
  d.azimuth_dist = { DistributionType::kUniform, 0, 360 };
  d.roll_dist = d.azimuth_dist;
  return d;
}

// Independent production-chart matrix multiplication (not the density's chart inverse).
std::array<double, 9> Rotation(double az, double lat, double roll) {
  auto rz = [](double x) {
    return std::array<double, 9>{ std::cos(x), -std::sin(x), 0, std::sin(x), std::cos(x), 0, 0, 0, 1 };
  };
  auto multiply = [](const auto& a, const auto& b) {
    std::array<double, 9> c{};
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        for (int k = 0; k < 3; ++k) {
          c[3 * i + j] += a[3 * i + k] * b[3 * k + j];
        }
      }
    }
    return c;
  };
  const double theta = lat - kHalfPi;
  const std::array<double, 9> ry{ std::cos(theta), 0, std::sin(theta), 0, 1, 0, -std::sin(theta), 0, std::cos(theta) };
  return multiply(multiply(rz(az - kPi), ry), rz(roll));
}

// Independent fold via asin(sin), with the flip read from cos(raw), not a chart inversion.
double FoldLatitude(double raw) {
  return std::asin(std::sin(raw));
}

template <class F>
double GL(double lo, double hi, const F& f) {
  struct Rule {
    std::array<double, 128> x{}, w{};
    Rule() { GaussLegendre(128, x.data(), w.data()); }
  };
  static const Rule rule;
  double result = 0;
  for (size_t i = 0; i < rule.x.size(); ++i) {
    result += rule.w[i] * f((lo + hi) / 2 + (hi - lo) / 2 * rule.x[i]);
  }
  return result * (hi - lo) / 2;
}

// Reference quadrature is independent of the implementation's adaptive inverse-normal rule.
// Gaussian integrates the native standard-normal variable, splitting all fold/indicator kinks;
// other families integrate native U with dyadic Laplace tails and exact sine preimages.
template <class F>
double Native(const Distribution& d, const F& f, std::vector<double> extra_breaks = {}) {
  const double c = d.center * kDeg;
  const double s = d.spread * kDeg;
  if (d.type == DistributionType::kNoRandom) {
    return f(c);
  }
  for (int i = -300; i <= 300; ++i) {
    extra_breaks.push_back(i * kHalfPi);
  }
  if (d.type == DistributionType::kLaplacian) {
    double result = 0;
    // Both tails use the LOWER quantile, so quadrature never rounds 1-U to zero.
    for (double sign : { -1.0, 1.0 }) {
      std::vector<double> panels{ 0.0, 0.5 };
      for (int i = 1; i <= 48; ++i) {
        panels.push_back(std::ldexp(1.0, -i));
      }
      for (double b : extra_breaks) {
        if ((b - c) * sign > 0) {
          const double u = 0.5 * std::exp(-std::abs(b - c) / s);
          if (u > 1e-40 && u < 0.5) {
            panels.push_back(u);
          }
        }
      }
      std::sort(panels.begin(), panels.end());
      panels.erase(std::unique(panels.begin(), panels.end()), panels.end());
      for (size_t i = 1; i < panels.size(); ++i) {
        result += GL(panels[i - 1], panels[i], [&](double u) { return f(c - sign * s * std::log(2 * u)); });
      }
    }
    return result;
  }
  const bool normal = d.type == DistributionType::kGaussian || d.type == DistributionType::kGaussianLegacy;
  std::vector<double> points = normal ? std::vector<double>{ -12, 12 } : std::vector<double>{ 0, 1 };
  auto add = [&](double x) {
    if (x > points.front() && x < points[1]) {
      points.push_back(x);
    }
  };
  if (d.type == DistributionType::kZigzag) {
    add(0.25);
    add(0.5);
    add(0.75);
    extra_breaks.push_back(0);
  }
  for (double b : extra_breaks) {
    if (normal) {
      add((b - c) / s);
    } else if (d.type == DistributionType::kUniform) {
      add(0.5 + (b - c) / s);
    } else if (d.type == DistributionType::kLaplacian) {
      add(b < c ? 0.5 * std::exp((b - c) / s) : 1 - 0.5 * std::exp(-(b - c) / s));
    } else {
      for (double raw : { b, -b }) {
        const double sine = (raw - c) / s;
        if (std::abs(sine) < 1) {
          const double a = std::asin(sine);
          add(std::fmod(a / kTurn + 1, 1.0));
          add(std::fmod((kPi - a) / kTurn + 1, 1.0));
        }
      }
    }
  }
  std::sort(points.begin(), points.end());
  points.erase(std::unique(points.begin(), points.end()), points.end());
  double result = 0;
  for (size_t i = 1; i < points.size(); ++i) {
    result += GL(points[i - 1], points[i], [&](double u) {
      if (normal) {
        return f(c + s * u) * std::exp(-u * u / 2) / std::sqrt(kTurn);
      }
      double raw = c + s * (u - 0.5);
      if (d.type == DistributionType::kLaplacian) {
        raw = u < 0.5 ? c + s * std::log(2 * u) : c - s * std::log(2 * (1 - u));
      } else if (d.type == DistributionType::kZigzag) {
        raw = std::abs(c + s * std::sin(kTurn * u));
      }
      return f(raw);
    });
  }
  return result;
}

double RawPdf(const Distribution& d, double x) {
  const double c = d.center * kDeg;
  const double s = d.spread * kDeg;
  switch (d.type) {
    case DistributionType::kUniform:
      return std::abs(x - c) < s / 2 ? 1 / s : 0;
    case DistributionType::kLaplacian:
      return std::exp(-std::abs(x - c) / s) / (2 * s);
    case DistributionType::kZigzag: {
      double sum = 0;
      if (x >= 0) {
        for (double raw : { x, -x }) {
          const long double delta = static_cast<long double>(raw) - c;
          const long double discriminant =
              (static_cast<long double>(s) - std::abs(delta)) * (static_cast<long double>(s) + std::abs(delta));
          if (discriminant > 0) {
            sum += static_cast<double>(1 / (static_cast<long double>(kPi) * std::sqrt(discriminant)));
          }
        }
      }
      return sum;
    }
    default: {
      const double z = (x - c) / s;
      return std::exp(-z * z / 2) / (s * std::sqrt(kTurn));
    }
  }
}

double WrappedPdf(const Distribution& d, double x) {
  double sum = 0;
  for (int i = -64; i <= 64; ++i) {
    sum += RawPdf(d, x + i * kTurn);
  }
  return sum;
}

TEST(AxisPoseDensity, ValidationChartAndHonestyFlags) {
  for (auto type : kTypes) {
    auto d = Axis(type);
    EXPECT_STREQ(AxisPoseDensityError(d), "");
    d.roll_dist.type = static_cast<DistributionType>(99);
    EXPECT_NE(std::string(AxisPoseDensityError(d)).find("roll_dist.type"), std::string::npos);
    d = Axis(type);
    d.azimuth_dist.center = std::numeric_limits<float>::infinity();
    EXPECT_NE(std::string(AxisPoseDensityError(d)).find("azimuth_dist.center"), std::string::npos);
  }
  auto d = Axis();
  d.latitude_dist.spread = 0;
  EXPECT_THROW((void)AxisPoseDensity(d), std::invalid_argument);
  d = Axis();
  d.azimuth_dist = { DistributionType::kNoRandom, 20, 0 };
  d.roll_dist = { DistributionType::kGaussian, 15, 8 };
  const AxisPoseDensity density(d);
  const auto r = Rotation(20 * kDeg, 30 * kDeg, 15 * kDeg);
  EXPECT_NEAR(density.LogDensity(r).value, density.LogDensityAngles(20 * kDeg, 30 * kDeg, 15 * kDeg).value, 1e-13);
  EXPECT_EQ(density.Evaluate(Rotation(60 * kDeg, 30 * kDeg, 15 * kDeg)).value, 0);
  EXPECT_TRUE(density.LogDensityThirdRow(r[6], r[7], r[8]).Has(AxisDensityFlag::kAzimuthRequired));
  EXPECT_TRUE(density.LogDensity(Rotation(0, kHalfPi, 0)).Has(AxisDensityFlag::kUndefinedPole));
  EXPECT_TRUE(density.LogDensity(Rotation(20 * kDeg, kHalfPi - 1e-9, 0)).Has(AxisDensityFlag::kNearPole));
  auto bad = r;
  bad[0] += 0.01;
  EXPECT_TRUE(density.Evaluate(bad).Has(AxisDensityFlag::kInvalidPose));
  const AxisPoseDensity legacy(Axis(DistributionType::kGaussianLegacy, 85, 8));
  const double l1 = legacy.LogDensityAngles(0, kHalfPi - 1e-6, 0).value;
  const double l2 = legacy.LogDensityAngles(0, kHalfPi - 1e-9, 0).value;
  const double pole_tolerance = 16 * std::numeric_limits<double>::epsilon() / 1e-9;
  EXPECT_NEAR(l2 - l1, std::log(1000.0), pole_tolerance);
  EXPECT_TRUE(legacy.LogDensityAngles(0, kHalfPi - 1e-9, 0).Has(AxisDensityFlag::kNearPole));
  auto zig = Axis(DistributionType::kZigzag, 30, 10);
  const AxisPoseDensity cusp(zig);
  EXPECT_TRUE(cusp.LatitudeLogFactor(40 * kDeg).Has(AxisDensityFlag::kCusp));
  EXPECT_EQ(cusp.LatitudeLogFactor(40 * kDeg).value, std::numeric_limits<double>::infinity());
  EXPECT_EQ(cusp.LatitudeLogFactor(20 * kDeg).value, std::numeric_limits<double>::infinity());
  EXPECT_EQ(cusp.LatitudeFactor(40 * kDeg).value, std::numeric_limits<double>::infinity());
  // Half-open uniform endpoints must not double-count a wrapped latitude image.
  const AxisPoseDensity isotropic(Axis(DistributionType::kUniform, 0, 360));
  for (double phi : { -0.1, 0.0, 0.1 }) {
    EXPECT_NEAR(isotropic.EvaluateAngles(0, phi, 0).value, 1, 1e-13);
  }
}

TEST(AxisPoseDensity, TransformedSupportEndpointsKeepTheirIdentity) {
  auto d = Axis(DistributionType::kUniform, 120, 10);
  d.azimuth_dist = { DistributionType::kUniform, 0, 40 };
  const AxisPoseDensity folded(d);
  EXPECT_GT(folded.EvaluateAngles(160 * kDeg, 60 * kDeg, 0).value, 0);
  for (float center : { -300.0f, 0.0f, 300.0f }) {
    d.azimuth_dist = { DistributionType::kUniform, center, 40 };
    d.roll_dist = d.azimuth_dist;
    const AxisPoseDensity density(d);
    for (bool flip : { false, true }) {
      const double lower = (static_cast<double>(center) - 20 + (flip ? 180 : 0)) * kDeg;
      const double upper = (static_cast<double>(center) + 20 + (flip ? 180 : 0)) * kDeg;
      EXPECT_NEAR(density.AzimuthFactor(lower, flip).value, 9, 1e-12);
      EXPECT_NEAR(density.RollFactor(std::remainder(lower, kTurn), flip).value, 9, 1e-12);
      EXPECT_EQ(density.AzimuthFactor(upper, flip).value, 0);
      // Only represented endpoints are recognized; no neighborhood is clamped onto support.
      EXPECT_EQ(density.AzimuthFactor(lower - 1e-9, flip).value, 0);
      EXPECT_GT(density.AzimuthFactor(lower + 1e-9, flip).value, 0);
      EXPECT_GT(density.RollFactor(upper - 1e-9, flip).value, 0);
      EXPECT_EQ(density.RollFactor(upper + 1e-9, flip).value, 0);
    }
  }
  for (float center : { -350.0f, -30.0f, 30.0f, 350.0f }) {
    d.azimuth_dist = { DistributionType::kZigzag, center, 10 };
    d.roll_dist = d.azimuth_dist;
    const AxisPoseDensity density(d);
    for (bool flip : { false, true }) {
      for (double sign : { -1.0, 1.0 }) {
        const double endpoint = (std::abs(static_cast<double>(center) + sign * 10) + (flip ? 180 : 0)) * kDeg;
        const auto value = density.AzimuthLogFactor(endpoint, flip);
        EXPECT_EQ(value.value, std::numeric_limits<double>::infinity());
        EXPECT_TRUE(value.Has(AxisDensityFlag::kCusp));
        EXPECT_EQ(density.RollLogFactor(std::remainder(endpoint, kTurn), flip).value,
                  std::numeric_limits<double>::infinity());
        EXPECT_NE(density.AzimuthLogFactor(endpoint + 1e-9, flip).value, std::numeric_limits<double>::infinity());
      }
    }
  }
  for (float center : { 120.0f, 300.0f }) {
    const AxisPoseDensity latitude(Axis(DistributionType::kZigzag, center, 10));
    for (double sign : { -1.0, 1.0 }) {
      // Independent degree-space fold, not pi-phi reconstruction in the evaluator.
      const double raw_deg = std::remainder(static_cast<double>(center) + sign * 10, 360.0);
      const bool flip = raw_deg > 90;
      const double phi = (flip ? 180 - raw_deg : raw_deg) * kDeg;
      EXPECT_EQ(latitude.LatitudeLogFactor(phi, flip).value, std::numeric_limits<double>::infinity());
    }
  }
  const AxisPoseDensity latitude_uniform(Axis(DistributionType::kUniform, 120, 10));
  EXPECT_GT(latitude_uniform.LatitudeFactor(65 * kDeg, true).value, 0);
  EXPECT_EQ(latitude_uniform.LatitudeFactor(55 * kDeg, true).value, 0);
}

TEST(AxisPoseDensity, LargeFiniteAnglesArePeriodicOnBothSheets) {
  auto d = Axis(DistributionType::kGaussian, 170, 3);
  d.azimuth_dist = { DistributionType::kGaussian, 0, 20 };
  d.roll_dist = { DistributionType::kLaplacian, 30, 10 };
  const AxisPoseDensity density(d);
  for (double angle : { -std::numeric_limits<double>::max(), -1e20, 1e20, std::numeric_limits<double>::max() }) {
    const double reduced = std::remainder(angle, kTurn);
    for (bool flip : { false, true }) {
      EXPECT_NEAR(density.AzimuthLogFactor(angle, flip).value, density.AzimuthLogFactor(reduced, flip).value, 1e-12);
      EXPECT_NEAR(density.RollLogFactor(angle, flip).value, density.RollLogFactor(reduced, flip).value, 1e-12);
    }
    EXPECT_NEAR(density.LogDensityAngles(angle, 10 * kDeg, angle).value,
                density.LogDensityAngles(reduced, 10 * kDeg, reduced).value, 1e-12);
  }
}

TEST(AxisPoseDensity, ExactPoleReflectionHasNoCircularMean) {
  for (auto type : { DistributionType::kUniform, DistributionType::kGaussian, DistributionType::kGaussianLegacy,
                     DistributionType::kLaplacian, DistributionType::kZigzag }) {
    for (float center : { -270.0f, -90.0f, 90.0f, 270.0f }) {
      for (float spread : { 0.001f, 8.0f }) {
        SCOPED_TRACE(static_cast<int>(type));
        SCOPED_TRACE(center);
        SCOPED_TRACE(spread);
        auto d = Axis(type, center, spread);
        d.azimuth_dist = { DistributionType::kNoRandom, 0, 0 };
        d.roll_dist = { DistributionType::kGaussian, 20, 8 };
        const AxisPoseDensity density(d);
        EXPECT_NEAR(density.FlipProbability(), 0.5, 1e-14);
        EXPECT_TRUE(std::isnan(density.Variances().sigma2_az));
        EXPECT_TRUE(std::isnan(density.Variances().sigma2_roll));
        EXPECT_NE(density.MomentFlags() & static_cast<uint32_t>(AxisDensityFlag::kCircularMeanUndefined), 0u);
      }
    }
  }
  auto d = Axis(DistributionType::kGaussian, std::nextafter(90.0f, 0.0f), 0.001f);
  d.azimuth_dist = { DistributionType::kNoRandom, 0, 0 };
  d.roll_dist = d.azimuth_dist;
  const AxisPoseDensity asymmetric(d);
  EXPECT_FALSE(std::isnan(asymmetric.Variances().sigma2_az));
  EXPECT_FALSE(std::isnan(asymmetric.Variances().sigma2_roll));
}

TEST(AxisPoseDensity, PinnedWindowsApplyTheRelativeSheetCutoff) {
  for (float center : { 30.0f, 150.0f }) {
    auto d = Axis(DistributionType::kGaussian, center, 5);
    d.azimuth_dist = { DistributionType::kNoRandom, 0, 0 };
    d.roll_dist = d.azimuth_dist;
    const AxisPoseDensity density(d);
    const auto azimuth = density.AzimuthWindow();
    const auto roll = density.RollWindow();
    if (azimuth.intervals.size() != 1 || roll.intervals.size() != 1) {
      ADD_FAILURE() << "Expected one retained point per axis; azimuth=" << azimuth.intervals.size()
                    << ", roll=" << roll.intervals.size();
      continue;
    }
    EXPECT_NEAR(azimuth.intervals[0][0], center < 90 ? 0 : kPi, 1e-13);
    EXPECT_NEAR(roll.intervals[0][0], center < 90 ? 0 : kPi, 1e-13);
  }
  auto d = Axis(DistributionType::kGaussian, 30, 8);
  d.azimuth_dist = { DistributionType::kNoRandom, 0, 0 };
  d.roll_dist = d.azimuth_dist;
  const AxisPoseDensity both_sheets(d);
  EXPECT_EQ(both_sheets.AzimuthWindow().intervals.size(), 2u);
  EXPECT_EQ(both_sheets.RollWindow().intervals.size(), 2u);
}

TEST(AxisPoseDensity, TierOneFineLutTarget) {
  // Reproduce the LUT builder's kFine=4096 mass target, NOT its resampled 257-node CDF.
  // Compare interior bin centers, away from a uniform edge, Laplace corner or arcsine cusp.
  // The 0.1 degree Gaussian's finite-bin curvature is already ~0.8%, inside the declared 1%.
  constexpr int kFine = 4096;
  constexpr int kQuad = 1 << 16;
  const double bin_width = kPi / kFine;
  for (auto type : { DistributionType::kUniform, DistributionType::kGaussian, DistributionType::kZigzag,
                     DistributionType::kLaplacian }) {
    for (float width : { 0.1f, 3.0f, 20.0f }) {
      SCOPED_TRACE(static_cast<int>(type));
      SCOPED_TRACE(width);
      const auto d = Axis(type, 30, width);
      const AxisPoseDensity density(d);
      std::vector<double> bins(kFine, 0);
      double total = 0;
      for (int i = 0; i < kQuad; ++i) {
        const double u = (i + 0.5) / kQuad;
        double raw = (30 + width * (u - 0.5)) * kDeg;
        double weight = 1;
        if (type == DistributionType::kGaussian) {
          const double z = 24 * u - 12;
          raw = (30 + width * z) * kDeg;
          weight = std::exp(-z * z / 2);
        } else if (type == DistributionType::kLaplacian) {
          raw = (30 + (u < 0.5 ? width * std::log(2 * u) : -width * std::log(2 * (1 - u)))) * kDeg;
        } else if (type == DistributionType::kZigzag) {
          raw = std::abs(30 + width * std::sin(kTurn * u)) * kDeg;
        }
        const double phi = FoldLatitude(raw);
        const int bin = std::min(kFine - 1, static_cast<int>((kHalfPi - phi) / bin_width));
        const double mass = weight * std::cos(phi);
        bins[bin] += mass;
        total += mass;
      }
      // Laplace uses an off-center bin so its nondifferentiable peak is not a midpoint rule input.
      const double target = (30 + (type == DistributionType::kLaplacian ? 0.8 * width : 0)) * kDeg;
      const int bin = static_cast<int>((kHalfPi - target) / bin_width);
      const double phi = kHalfPi - (bin + 0.5) * bin_width;
      const double predicted = density.EvaluateAngles(0, phi, 0).value * std::cos(phi) * bin_width / 2;
      EXPECT_NEAR(predicted / (bins[bin] / total), 1, 0.01);
    }
  }
}

TEST(AxisPoseDensity, HaarNormalizationTypeMatrixAndPinnedSheets) {
  // Native-coordinate importance quadrature integrates the Haar factors. The denominator is
  // an independently assembled continuum image sum, not the density's own normalization.
  // All six types appear on EACH axis; the eight pin masks also exercise every free dimension.
  // The last six rows hit the maximum admitted width and rectified zigzag's two raw preimages,
  // where a nearest-image-only implementation would lose normalization.
  for (int row = 0; row < 20; ++row) {
    auto d = Axis(kTypes[row % 6], row >= 14 ? -10 : 30, row >= 14 ? 360 : 12);
    d.azimuth_dist = { kTypes[(row + 2) % 6], 20, row >= 14 ? 360.0f : 15.0f };
    d.roll_dist = { kTypes[(row + 4) % 6], 40, row >= 14 ? 360.0f : 10.0f };
    if (row >= 6 && row < 14) {
      const int mask = row - 6;
      if (mask & 1) {
        d.latitude_dist.type = DistributionType::kNoRandom;
      }
      if (mask & 2) {
        d.azimuth_dist.type = DistributionType::kNoRandom;
      }
      if (mask & 4) {
        d.roll_dist.type = DistributionType::kNoRandom;
      }
    }
    SCOPED_TRACE(row);
    const AxisPoseDensity density(d);
    EXPECT_EQ(density.MomentFlags() & static_cast<uint32_t>(AxisDensityFlag::kQuadratureAccuracy), 0u);
    const double probe_latitude =
        d.latitude_dist.center * kDeg + (d.latitude_dist.type == DistributionType::kNoRandom ? 0 : 0.07);
    double latitude_mass[2]{};
    for (int leaf = 0; leaf < 2; ++leaf) {
      if (d.latitude_dist.type == DistributionType::kNoRandom) {
        latitude_mass[leaf] = density.LatitudeFactor(probe_latitude, leaf != 0).value;
      } else {
        latitude_mass[leaf] = Native(d.latitude_dist, [&](double raw) {
          const double phi = FoldLatitude(raw);
          const double proposal = WrappedPdf(d.latitude_dist, phi) + WrappedPdf(d.latitude_dist, kPi - phi);
          return density.LatitudeFactor(phi, leaf != 0).value * std::cos(phi) / (2 * proposal);
        });
      }
      for (int axis = 0; axis < 2; ++axis) {
        const auto& marginal = axis == 0 ? d.azimuth_dist : d.roll_dist;
        const double integral = Native(marginal, [&](double raw) {
          const double angle = std::remainder(raw + leaf * kPi, kTurn);
          const double factor =
              axis == 0 ? density.AzimuthFactor(angle, leaf != 0).value : density.RollFactor(angle, leaf != 0).value;
          return marginal.type == DistributionType::kNoRandom ?
                     factor :
                     factor / (kTurn * WrappedPdf(marginal, std::remainder(angle - leaf * kPi, kTurn)));
        });
        EXPECT_NEAR(integral, 1, 1e-12);
        latitude_mass[leaf] *= integral;
      }
    }
    EXPECT_NEAR(latitude_mass[0] + latitude_mass[1], 1, 1e-12);
    // Actual pose entry agrees with the leaf assembly, including zero support constraints.
    for (bool flip : { false, true }) {
      const double a = 20 * kDeg + (flip ? kPi : 0) + (d.azimuth_dist.type == DistributionType::kNoRandom ? 0 : 0.13);
      const double b = 40 * kDeg + (flip ? kPi : 0) + (d.roll_dist.type == DistributionType::kNoRandom ? 0 : 0.17);
      double expected = 0;
      for (bool j : { false, true }) {
        expected += density.LatitudeFactor(probe_latitude, j).value * density.AzimuthFactor(a, j).value *
                    density.RollFactor(b, j).value;
      }
      EXPECT_NEAR(density.Evaluate(Rotation(a, probe_latitude, b)).value, expected, 1e-12 * std::max(1.0, expected));
    }
  }
}

TEST(AxisPoseDensity, TierTwoRealizedSamplerJointHistogram) {
  // Widths >=3 degrees: the realized 257-node LUT has a resolution floor of up to 180/256
  // degrees per node. Narrower end-to-end tests would judge interpolation, not the continuum law.
  // Four populated joint bins each have >2% mass (5% is >7 sigma at N=1e6). Off-diagonal
  // az/roll bins detect a missing +pi on EITHER coordinate, not just latitude folding.
  constexpr size_t kSamples = 1000000;
  for (auto type : { DistributionType::kGaussian, DistributionType::kGaussianLegacy }) {
    auto d = Axis(type, 85, 12);
    d.azimuth_dist = { DistributionType::kGaussian, 20, 12 };
    d.roll_dist = { DistributionType::kGaussian, -35, 12 };
    const AxisPoseDensity density(d);
    std::array<double, 8> expected{};
    const auto lat = d.latitude_dist;
    const bool area = type != DistributionType::kGaussianLegacy;
    const double total = Native(lat, [&](double raw) { return area ? std::abs(std::cos(raw)) : 1.0; });
    for (int bin = 0; bin < 8; ++bin) {
      const double reference = Native(lat,
                                      [&](double raw) {
                                        const bool flip = std::cos(raw) < 0;
                                        const int index = (FoldLatitude(raw) > 80 * kDeg ? 4 : 0) + (flip ? 3 : 0);
                                        return index == bin ? (area ? std::abs(std::cos(raw)) : 1.0) : 0.0;
                                      },
                                      { 80 * kDeg, 100 * kDeg }) /
                               total;
      if (bin % 4 == 0 || bin % 4 == 3) {
        // Integrate the ACTUAL density against Haar, using independent native-proposal
        // importance coordinates. Angular half-plane leakage is below 1e-13 here.
        expected[bin] =
            Native(lat,
                   [&](double raw) {
                     const double phi = FoldLatitude(raw);
                     if ((phi > 80 * kDeg) != (bin >= 4)) {
                       return 0.0;
                     }
                     const double proposal = WrappedPdf(lat, phi) + WrappedPdf(lat, kPi - phi);
                     return density.LatitudeFactor(phi, bin % 4 == 3).value * std::cos(phi) / (2 * proposal);
                   },
                   { 80 * kDeg, 100 * kDeg });
      }
      EXPECT_NEAR(expected[bin], reference, 1e-12);
    }
    EXPECT_NEAR(density.FlipProbability(), expected[3] + expected[7], 1e-12);
    std::array<size_t, 8> counts{};
    std::vector<float> samples(3 * kSamples);
    RandomNumberGenerator::GetInstance().SetSeed(0x187ad23u);
    const auto lut = BuildLatLut(lat);
    RandomSampler::SampleSphericalPointsSph(d, samples.data(), kSamples, &lut);
    for (size_t i = 0; i < kSamples; ++i) {
      const int bin = (samples[3 * i + 1] > 80 * kDeg ? 4 : 0) + (std::cos(samples[3 * i] - 20 * kDeg) < 0 ? 1 : 0) +
                      (std::cos(samples[3 * i + 2] + 35 * kDeg) < 0 ? 2 : 0);
      ++counts[bin];
    }
    for (int bin = 0; bin < 8; ++bin) {
      SCOPED_TRACE(bin);
      SCOPED_TRACE(static_cast<int>(type));
      if (expected[bin] > 0.02) {
        EXPECT_NEAR(static_cast<double>(counts[bin]) / kSamples / expected[bin], 1, 0.05);
      } else {
        EXPECT_LE(counts[bin], 2u);  // 90-degree angular tails at >=7.5 sigma.
      }
    }
  }
}

TEST(AxisPoseDensity, CenteredPushforwardVariances) {
  for (auto type : { DistributionType::kUniform, DistributionType::kGaussian, DistributionType::kZigzag,
                     DistributionType::kLaplacian, DistributionType::kGaussianLegacy }) {
    for (float center : { 30.0f, 87.0f }) {
      SCOPED_TRACE(static_cast<int>(type));
      SCOPED_TRACE(center);
      auto d = Axis(type, center, 8);
      d.azimuth_dist = { DistributionType::kGaussian, 170, 10 };
      d.roll_dist = { DistributionType::kLaplacian, -170, 8 };
      const AxisPoseDensity density(d);
      const bool area = type != DistributionType::kGaussianLegacy;
      auto weight = [&](double raw) { return area ? std::abs(std::cos(raw)) : 1.0; };
      const double total = Native(d.latitude_dist, weight);
      const double anchor = center * kDeg;
      const double first =
          Native(d.latitude_dist, [&](double raw) { return weight(raw) * (FoldLatitude(raw) - anchor); }) / total;
      const double second = Native(d.latitude_dist,
                                   [&](double raw) {
                                     const double delta = FoldLatitude(raw) - anchor;
                                     return weight(raw) * delta * delta;
                                   }) /
                            total;
      const auto variance = density.Variances();
      EXPECT_NEAR(variance.sigma2_lat, second - first * first, 2e-12 * std::max(1.0, second));
      const double flip =
          Native(d.latitude_dist, [&](double raw) { return std::cos(raw) < 0 ? weight(raw) : 0; }) / total;
      for (int axis = 0; axis < 2; ++axis) {
        const auto& marginal = axis == 0 ? d.azimuth_dist : d.roll_dist;
        const double mean = marginal.center * kDeg;  // symmetric base, flip probability <1/2
        const double reference = Native(marginal,
                                        [&](double x) {
                                          const double a = std::remainder(x - mean, kTurn);
                                          const double b = std::remainder(x + kPi - mean, kTurn);
                                          return (1 - flip) * a * a + flip * b * b;
                                        },
                                        { mean, mean - kPi, mean + kPi });
        EXPECT_NEAR(axis == 0 ? variance.sigma2_az : variance.sigma2_roll, reference, 3e-12);
      }
    }
  }
  const AxisPoseDensity uniform(Axis(DistributionType::kUniform, 90, 360));
  EXPECT_NEAR(uniform.Variances().sigma2_az, kPi * kPi / 3, 1e-14);
  EXPECT_NEAR(uniform.Variances().sigma2_roll, kPi * kPi / 3, 1e-14);
  const AxisPoseDensity narrow(Axis(DistributionType::kGaussian, 30, 0.001f));
  EXPECT_NEAR(narrow.Variances().sigma2_lat / std::pow(0.001f * kDeg, 2), 1, 2e-8);
  const AxisPoseDensity rectified_narrow(Axis(DistributionType::kZigzag, -30, 0.001f));
  EXPECT_NEAR(rectified_narrow.Variances().sigma2_lat / std::pow(0.001f * kDeg, 2), 0.5, 2e-8);
}

TEST(AxisPoseDensity, EquivalentLiFamiliesAndNonEquivalentSeam) {
  for (auto family :
       { PoseFamily::kRandom, PoseFamily::kColumn, PoseFamily::kPlate, PoseFamily::kParry, PoseFamily::kLowitz }) {
    const bool roll = family == PoseFamily::kParry || family == PoseFamily::kLowitz;
    const bool random = family == PoseFamily::kRandom;
    const double zenith = family == PoseFamily::kPlate || family == PoseFamily::kLowitz ? 40 : 60;
    const PoseDensitySpec spec{ family, random ? 0 : zenith, random ? 0.0 : 1.0, roll ? 20.0 : 0.0, roll ? 1.0 : 0.0 };
    const PoseDensity li(spec);
    auto d = random ? Axis(DistributionType::kUniform, 90, 360) : Axis(DistributionType::kGaussian, 90 - zenith, 1);
    if (roll) {
      d.roll_dist = { DistributionType::kGaussian, 20, 1 };
    }
    const AxisPoseDensity density(d);
    // Random has exactly uniform marginals. In the four localized families the off-pole latitude
    // images underflow, so LI's single zenith Gaussian and the production folded sum coincide
    // here. For Parry/Lowitz, probes are off the roll seam (other images underflow as well).
    // Pole-centered locked-roll configurations are NOT equivalent: folding adds a half-turn lobe.
    for (double offset : { -2.0, 0.0, 2.0 }) {
      const auto r = Rotation(0.7, (90 - zenith + offset) * kDeg, (20 + offset) * kDeg);
      const double expected = li.Evaluate(r[6], r[7], r[8]);
      EXPECT_NEAR(density.Evaluate(r).value / expected, 1, 1e-12);
    }
  }
  // Nearest-offset LI roll is not a sum of wrapped images at the seam: two equal images give
  // log(2), even though both LINEAR values underflow. This case is excluded from LI overlap.
  auto d = Axis(DistributionType::kGaussian, 30, 1);
  d.roll_dist = { DistributionType::kGaussian, 0, 1 };
  const AxisPoseDensity density(d);
  const double sigma = kDeg;
  const double nearest_log = std::log(kTurn / (sigma * std::sqrt(kTurn))) - kPi * kPi / (2 * sigma * sigma);
  EXPECT_NEAR(density.RollLogFactor(kPi).value - nearest_log, std::log(2.0), 1e-11);
}

TEST(AxisPoseDensity, UniformFastPathAndPoleTangentClosure) {
  const auto d = Axis(DistributionType::kGaussian, 87, 8);
  const AxisPoseDensity density(d);
  const double nan = std::numeric_limits<double>::quiet_NaN();
  for (double phi : { 0.1, 1.2, kHalfPi - 1e-9 }) {
    const auto r = Rotation(1.1, phi, 2.3);
    EXPECT_NEAR(density.LogDensity(r).value, density.LogDensityThirdRow(r[6], r[7], r[8]).value, 1e-13);
    // Independent general sum-of-images path: do not call the uniform fast-path accessors
    // on BOTH sides of an equivalence assertion.
    double general = 0;
    for (int leaf = 0; leaf < 2; ++leaf) {
      general += density.LatitudeFactor(phi, leaf != 0).value * (kTurn * WrappedPdf(d.azimuth_dist, 1.1 - leaf * kPi)) *
                 (kTurn * WrappedPdf(d.roll_dist, 2.3 - leaf * kPi));
    }
    EXPECT_NEAR(density.EvaluateAngles(nan, phi, nan).value / general, 1, 1e-13);
    EXPECT_FALSE(density.LogDensity(r).Has(AxisDensityFlag::kNearPole));
  }
  // At a pole the uniform-azimuth tangent direction has E[nn^T]=(I-zz^T)/2. This is a
  // limiting moment identity, NOT permission to fabricate the undefined chart's point value.
  EXPECT_NEAR(GL(0, kTurn, [](double a) { return std::cos(a) * std::cos(a) / kTurn; }), 0.5, 1e-13);
  EXPECT_NEAR(GL(0, kTurn, [](double a) { return std::sin(a) * std::sin(a) / kTurn; }), 0.5, 1e-13);
  EXPECT_NEAR(GL(0, kTurn, [](double a) { return std::sin(a) * std::cos(a) / kTurn; }), 0, 1e-13);
  EXPECT_TRUE(density.Evaluate(Rotation(0, kHalfPi, 0)).Has(AxisDensityFlag::kUndefinedPole));
}

TEST(AxisPoseDensity, HonestMarginalWindows) {
  auto d = Axis(DistributionType::kGaussian, 30, 2);
  d.roll_dist = { DistributionType::kGaussian, 170, 2 };
  const AxisPoseDensity density(d);
  const auto lat = density.LatitudeWindow();
  ASSERT_EQ(lat.intervals.size(), 1u);
  EXPECT_NEAR(lat.intervals[0][0], 6 * kDeg, 2e-12);
  EXPECT_NEAR(lat.intervals[0][1], 54 * kDeg, 2e-12);
  const auto roll = density.RollWindow();
  ASSERT_EQ(roll.intervals.size(), 2u);
  const double peak = density.RollLogFactor(170 * kDeg).value;
  EXPECT_NEAR(density.RollLogFactor(roll.intervals[0][1]).value, peak - 72, 2e-10);
  auto contains = [](const AxisDensityWindow& window, double point) {
    return std::any_of(window.intervals.begin(), window.intervals.end(),
                       [&](const auto& interval) { return interval[0] <= point && point <= interval[1]; });
  };
  // A tiny unflipped sheet is still above exp(-72), even when the flipped probability rounds
  // to one. Compute its weight independently rather than subtracting it from that rounded one.
  auto almost_flipped = Axis(DistributionType::kGaussian, 120, 3);
  almost_flipped.azimuth_dist = { DistributionType::kGaussian, 0, 1 };
  const AxisPoseDensity rare_sheet(almost_flipped);
  const double unflipped_mass =
      Native(almost_flipped.latitude_dist, [](double raw) { return std::cos(raw) > 0 ? std::cos(raw) : 0.0; }) /
      Native(almost_flipped.latitude_dist, [](double raw) { return std::abs(std::cos(raw)); });
  EXPECT_GT(std::log(unflipped_mass), -72);
  EXPECT_TRUE(contains(rare_sheet.AzimuthWindow(), 0));

  // An arbitrarily short secondary superlevel interval can lie wholly between mesh nodes.
  // For this one-degree Laplace latitude the distant images are <exp(-200); the folded mass
  // ratio has the independent closed form s*exp(-(pi/2-c)/s)/(2*cos(c)). Tune its log to -72
  // plus 0.0005 using float input parameters, not the implementation's marginal probabilities.
  auto short_lobe = Axis(DistributionType::kLaplacian, 20, 1);
  const double scale = kDeg;
  double lower = 10;
  double upper = 40;
  for (int i = 0; i < 48; ++i) {
    const double center = (lower + upper) / 2;
    const double log_ratio = std::log(scale / (2 * std::cos(center * kDeg))) - (kHalfPi - center * kDeg) / scale;
    if (log_ratio < -72 + 0.0005) {
      lower = center;
    } else {
      upper = center;
    }
  }
  short_lobe.latitude_dist.center = static_cast<float>((lower + upper) / 2);
  short_lobe.azimuth_dist = { DistributionType::kGaussian, 0.0625f, 1 };
  const AxisPoseDensity tiny_interval(short_lobe);
  const double gap = std::log(tiny_interval.FlipProbability()) + 72;
  ASSERT_GT(gap, 0);
  ASSERT_LT(gap, 0.001);
  EXPECT_TRUE(contains(tiny_interval.AzimuthWindow(), (0.0625 - 180) * kDeg));

  // A periodic maximum between the seam and the first mesh node must be refined too.
  auto seam = Axis(DistributionType::kNoRandom, 0, 0);
  seam.roll_dist = { DistributionType::kGaussian, 179.99f, 2 };
  const AxisPoseDensity seam_density(seam);
  const auto seam_window = seam_density.RollWindow();
  ASSERT_EQ(seam_window.intervals.size(), 2u);
  EXPECT_NEAR(seam_window.intervals[0][1], (seam.roll_dist.center + 24.0 - 360) * kDeg, 2e-13);
  EXPECT_NEAR(seam_window.intervals[1][0], (seam.roll_dist.center - 24.0) * kDeg, 2e-13);

  // Exact uniform edges matter: equal-height lobes can hide a zero-density gap shorter than
  // one mesh cell, leaving every sampled value on the same plateau.
  auto uniform_gap = Axis(DistributionType::kGaussianLegacy, 90, 1);
  uniform_gap.azimuth_dist = { DistributionType::kUniform, 0.3f, 179.99998f };
  const AxisPoseDensity separated_uniforms(uniform_gap);
  const auto gap_window = separated_uniforms.AzimuthWindow();
  EXPECT_FALSE(contains(gap_window, (static_cast<double>(uniform_gap.azimuth_dist.center) + 90) * kDeg));
  EXPECT_FALSE(contains(gap_window, (static_cast<double>(uniform_gap.azimuth_dist.center) - 90) * kDeg));
  EXPECT_TRUE(contains(gap_window, uniform_gap.azimuth_dist.center * kDeg));

  const AxisPoseDensity zig(Axis(DistributionType::kZigzag));
  EXPECT_TRUE(zig.LatitudeWindow().intervals.empty());
  EXPECT_NE(zig.LatitudeWindow().flags & static_cast<uint32_t>(AxisDensityFlag::kUnboundedPeak), 0u);
  const AxisPoseDensity legacy(Axis(DistributionType::kGaussianLegacy));
  EXPECT_TRUE(legacy.LatitudeWindow().intervals.empty());
  const AxisPoseDensity pinned(Axis(DistributionType::kNoRandom, 90, 0));
  EXPECT_TRUE(pinned.EvaluateAngles(0, kHalfPi, 0).Has(AxisDensityFlag::kUndefinedPole));
}

}  // namespace
}  // namespace lumice::analytic
