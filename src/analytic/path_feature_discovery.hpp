#ifndef LUMICE_ANALYTIC_PATH_FEATURE_DISCOVERY_HPP_
#define LUMICE_ANALYTIC_PATH_FEATURE_DISCOVERY_HPP_

#include <array>
#include <cstdint>
#include <vector>

namespace lumice::analytic {

// A weighted push-forward measure, not pixels. Weights already contain quadrature
// mass and spectral coefficients. Rows are ordered by independent sample identity;
// members and wavelengths of the same draw share an identity for effective count.
struct WeightedSkySample {
  uint64_t sample_index = 0;
  std::array<double, 3> direction{};
  std::array<double, 3> xyz_weight{};
};

// Covariant derivatives in an explicit orthonormal tangent frame, in radians.
// Hessian order is 00, 01, 11. Values are per steradian of the observation kernel.
struct SphericalJet {
  double value = 0;
  std::array<double, 2> gradient{};
  std::array<double, 3> hessian{};
};
struct SphericalFieldQuery {
  std::array<double, 3> direction{};
  std::array<std::array<double, 3>, 2> basis{};
  double bandwidth_rad = 0;
};
struct SphericalFieldValue {
  std::array<SphericalJet, 3> xyz;
  std::array<SphericalJet, 2> xy;
  double effective_samples_y = 0;
  bool chromaticity_available = false;
};

// Normalized vMF convolution with kappa = 1 / bandwidth_rad^2. No angular
// truncation or projection; no product classification or convergence claim.
// An empty measure is a valid zero field, NOT proof of physically absent support.
// Invalid input or non-finite output returns false and clears the entire result.
bool EvaluateSphericalField(const std::vector<WeightedSkySample>& samples, const SphericalFieldQuery& query,
                            SphericalFieldValue* out);

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_PATH_FEATURE_DISCOVERY_HPP_
