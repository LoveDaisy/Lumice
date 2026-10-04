#include "core/product_sample_transform.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "core/lat_lut.hpp"
#include "core/shared/pcg_shared.h"

namespace lumice {

float TransformDistribution(const Distribution& dist, DistributionLatentDraw draw) {
  switch (dist.type) {
    case DistributionType::kUniform:
      return (draw.value - 0.5f) * dist.UniformFullRange() + dist.UniformCenter();
    case DistributionType::kGaussian:
    case DistributionType::kGaussianLegacy:
      return draw.value * dist.Std() + dist.Mean();
    case DistributionType::kZigzag:
      // Rectified arcsine: |A·sin(2πU) + B| where A is the amplitude and B the tilt offset.
      // The abs() is intentional: fold (flip=true) is unconditionally skipped — abs() guarantees
      // phi >= 0 for all kZigzag inputs regardless of the amplitude / tilt values.
      return std::abs(dist.Amplitude() * std::sin(draw.value * 2.0f * math::kPi) + dist.Tilt());
    case DistributionType::kLaplacian: {
      // Laplace inverse CDF: μ - b·sign(U-0.5)·ln(1-2|U-0.5|), returns degrees.
      float u = draw.value;
      float sign = (u < 0.5f) ? -1.0f : 1.0f;
      float arg = 1.0f - 2.0f * std::abs(u - 0.5f);
      arg = std::max(arg, std::numeric_limits<float>::min());  // Clamp to avoid ln(0).
      return dist.Location() - dist.Scale() * sign * std::log(arg);
    }
    case DistributionType::kNoRandom:
      return dist.Value();
    default:
      return 0.0f;
  }
}

LatitudeSample TransformFullSphereLatitude(float unit_uniform) {
  const float u = std::max(-1.0f, std::min(1.0f, unit_uniform * 2.0f - 1.0f));
  return { std::asin(u), false };
}

LatitudeSample TransformLatitudeLut(const LatLut& lut, LatitudeLutDraw draw) {
  const float theta_z = lm_pcg::invert_lat_lut(draw.quantile, lut.theta.data(), lut.cdf.data(), LatLut::kNodes);
  const uint32_t bin = lm_pcg::lat_lut_bin(theta_z, lut.theta.data(), LatLut::kNodes);
  return { math::kPi_2 - theta_z, draw.flip_uniform < lut.flip_prob[bin] };
}

LatitudeSample TransformLegacyLatitude(float degrees) {
  const auto normalized = detail::NormalizeLatitude(degrees * math::kDegreeToRad);
  return { normalized.first, normalized.second };
}

std::array<float, 3> ComposeAxisAngles(LatitudeSample latitude, float azimuth_deg, float roll_deg) {
  float lambda = azimuth_deg * math::kDegreeToRad;
  float roll = roll_deg * math::kDegreeToRad;
  if (latitude.flip) {
    lambda += math::kPi;
    roll += math::kPi;
  }
  return { lambda, latitude.radians, roll };
}

std::array<float, 2> TransformFullSpherePoint(float latitude_uniform, float longitude_uniform) {
  const float u = latitude_uniform * 2 - 1;
  const float lambda = longitude_uniform * 2 * math::kPi;
  return { lambda, std::asin(u) };
}

SphericalCapTransform MakeSphericalCapTransform(float lon_rad, float lat_rad, float radius_rad) {
  return { std::cos(radius_rad), std::cos(lon_rad), std::sin(lon_rad), std::cos(lat_rad), std::sin(lat_rad) };
}

std::array<float, 3> TransformSphericalCap(const SphericalCapTransform& cap, SphericalCapDraw draw) {
  float x = draw.radial_uniform;
  x += (1 - x) * cap.c_cap;
  const float r = std::sqrt(1.0f - x * x);
  const float u = draw.azimuth_uniform * 2 * math::kPi;
  const float y = std::cos(u) * r;
  const float z = std::sin(u) * r;
  return { cap.c_lon * cap.c_lat * x - cap.s_lon * y - cap.c_lon * cap.s_lat * z,
           cap.s_lon * cap.c_lat * x + cap.c_lon * y - cap.s_lon * cap.s_lat * z, cap.s_lat * x + cap.c_lat * z };
}

}  // namespace lumice
