#ifndef LUMICE_ANALYTIC_PATH_FEATURE_DISCOVERY_HPP_
#define LUMICE_ANALYTIC_PATH_FEATURE_DISCOVERY_HPP_

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

#include "analytic/field_types.hpp"

namespace lumice::analytic {

// A weighted push-forward measure, not pixels. Weights already contain quadrature
// mass and spectral coefficients. Rows are ordered by independent sample identity;
// members and wavelengths of the same draw share an identity for effective count.


// Covariant derivatives in an explicit orthonormal tangent frame, in radians.
// Hessian order is 00, 01, 11. Values are per steradian of the observation kernel.


struct FieldWorkBudget {
  uint64_t max_component_evaluations = 0;
  uint64_t component_evaluations = 0;
  std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::time_point::max();
  bool exhausted = false;
  bool Consume(uint64_t count);
};


struct FieldSolveOptions {
  FieldEquation equation = FieldEquation::kLogYRidge;
  double level = 0;
  double bandwidth_rad = 0;
  double tolerance_rad = 1e-8;
  double max_step_rad = 0;
  int max_iterations = 32;
};


// Local numerical correction only. Converged says the declared equation was
// solved on THIS measure, not that an actual physical feature was discovered.
// No product strength, morphology, sample sufficiency or stability thresholds.
FieldStationaryPoint CorrectSphericalField(const std::vector<WeightedSkySample>& samples,
                                           const std::array<double, 3>& seed, const FieldSolveOptions& options,
                                           FieldWorkBudget* budget);


struct FieldWalkOptions {
  FieldSolveOptions corrector;
  double step_rad = 0;
  double minimum_y = 0;
  int max_points = 0;
};

FieldCurve TraceSphericalField(const std::vector<WeightedSkySample>& samples, const std::array<double, 3>& seed,
                               const FieldWalkOptions& options, bool reverse, FieldWorkBudget* budget);


// Two level boundaries of ONE fixed observation, locally transverse to a
// supplied centre curve. This is a field-value range, never an error interval
// or the envelope of different bandwidths. End caps belong to the caller's
// declared local search window, not to a physical source boundary.
FieldBand CorrectSphericalFieldBand(const std::vector<WeightedSkySample>& samples,
                                    const std::vector<FieldStationaryPoint>& centre,
                                    const std::array<double, 2>& levels, const FieldSolveOptions& options,
                                    FieldWorkBudget* budget);

// Normalized vMF convolution with kappa = 1 / bandwidth_rad^2. No angular
// truncation or projection; no product classification or convergence claim.
// Uniform orbits are integrated analytically, not expanded into optical rows.
// No quadrature nodes are counted as independent outer samples.
// An empty measure is a valid zero field, NOT proof of physically absent support.
// Invalid input or non-finite output returns false and clears the entire result.
bool EvaluateSphericalField(const std::vector<WeightedSkySample>& samples, const SphericalFieldQuery& query,
                            SphericalFieldValue* out, FieldWorkBudget* budget = nullptr);

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_PATH_FEATURE_DISCOVERY_HPP_
