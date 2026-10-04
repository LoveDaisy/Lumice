#ifndef CORE_PRODUCT_SAMPLE_TRANSFORM_H_
#define CORE_PRODUCT_SAMPLE_TRANSFORM_H_

#include <array>

#include "core/math.hpp"

namespace lumice {

struct LatLut;

// A standard normal for Gaussian families, a unit uniform for the other random
// families. Fixed distributions ignore it. This is NOT a realized shape value.
struct DistributionLatentDraw {
  float value = 0.0f;
};
float TransformDistribution(const Distribution& dist, DistributionLatentDraw draw);

struct LatitudeSample {
  float radians = 0.0f;
  bool flip = false;
};
struct LatitudeLutDraw {
  float quantile;
  float flip_uniform;
};
LatitudeSample TransformFullSphereLatitude(float unit_uniform);
LatitudeSample TransformLatitudeLut(const LatLut& lut, LatitudeLutDraw draw);
LatitudeSample TransformLegacyLatitude(float degrees);
std::array<float, 3> ComposeAxisAngles(LatitudeSample latitude, float azimuth_deg, float roll_deg);
// Inner-to-outer Euler factors: Rz(roll), Ry(latitude-pi/2), Rz(azimuth-pi).
std::array<float, 3> ProductRotationAngles(float azimuth_rad, float latitude_rad, float roll_rad);
// The production full-sphere fast path uses longitude in [0, 2*pi], not [-pi, pi].
std::array<float, 2> TransformFullSpherePoint(float latitude_uniform, float longitude_uniform);

// Cache the trigonometry once per source, as SampleSphCapPoint always has.
struct SphericalCapTransform {
  float c_cap;
  float c_lon;
  float s_lon;
  float c_lat;
  float s_lat;
};
struct SphericalCapDraw {
  float radial_uniform;
  float azimuth_uniform;
};
SphericalCapTransform MakeSphericalCapTransform(float lon_rad, float lat_rad, float radius_rad);
std::array<float, 3> TransformSphericalCap(const SphericalCapTransform& cap, SphericalCapDraw draw);

}  // namespace lumice

#endif  // CORE_PRODUCT_SAMPLE_TRANSFORM_H_
