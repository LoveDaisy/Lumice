#ifndef LUMICE_ANALYTIC_PATH_FEATURE_DISCOVERY_HPP_
#define LUMICE_ANALYTIC_PATH_FEATURE_DISCOVERY_HPP_

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

namespace lumice::analytic {

// A weighted push-forward measure, not pixels. Weights already contain quadrature
// mass and spectral coefficients. Rows are ordered by independent sample identity;
// members and wavelengths of the same draw share an identity for effective count.
struct WeightedSkySample {
  uint64_t sample_index = 0;
  std::array<double, 3> direction{};
  std::array<double, 3> xyz_weight{};
  // When present, direction is a representative of the full [0, 2*pi) orbit
  // about this unit world axis, with NORMALIZED uniform angular measure. The
  // caller must establish that both its measure and physical weight are invariant
  // under this action. In particular, finite-width anisotropy is not Haar.
  std::optional<std::array<double, 3>> uniform_orbit_axis;
  // Opaque conditional-source identity, distinct from the outer statistical draw.
  // The caller retains the representative pose; rotate it about the same world
  // axis to reconstruct the family. Spectral/member rows may share sample_index.
  uint64_t source_token = 0;
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
  uint64_t outer_sample_count = 0;
  bool chromaticity_available = false;
};

struct FieldWorkBudget {
  uint64_t max_component_evaluations = 0;
  uint64_t component_evaluations = 0;
  std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::time_point::max();
  bool exhausted = false;
  bool Consume(uint64_t count);
};

enum class FieldEquation { kLogYPeak, kLogYRidge, kChromaticityX, kChromaticityY };
enum class FieldSolveStatus { kConverged, kInvalidInput, kNoSignal, kDegenerate, kIterationLimit, kBudgetExceeded };
struct FieldSolveOptions {
  FieldEquation equation = FieldEquation::kLogYRidge;
  double level = 0;
  double bandwidth_rad = 0;
  double tolerance_rad = 1e-8;
  double max_step_rad = 0;
  int max_iterations = 32;
};
struct FieldStationaryPoint {
  FieldSolveStatus status = FieldSolveStatus::kInvalidInput;
  SphericalFieldQuery query;
  SphericalFieldValue field;
  std::array<double, 2> normal{};
  std::array<double, 2> log_y_curvatures{};
  double correction_rad = 0;
  double travelled_rad = 0;
  int iterations = 0;
};

// Local numerical correction only. Converged says the declared equation was
// solved on THIS measure, not that an actual physical feature was discovered.
// No product strength, morphology, sample sufficiency or stability thresholds.
FieldStationaryPoint CorrectSphericalField(const std::vector<WeightedSkySample>& samples,
                                           const std::array<double, 3>& seed, const FieldSolveOptions& options,
                                           FieldWorkBudget* budget);

enum class FieldWalkStop { kClosed, kObservationCensored, kCorrectorFailed, kPointLimit, kInvalidInput };
struct FieldWalkOptions {
  FieldSolveOptions corrector;
  double step_rad = 0;
  double minimum_y = 0;
  int max_points = 0;
};
struct FieldCurve {
  std::vector<FieldStationaryPoint> points;
  FieldWalkStop stop = FieldWalkStop::kInvalidInput;
  // The failed/censored trial is retained separately, never appended as a
  // converged curve point. Observation censoring is not a physical source edge.
  FieldStationaryPoint terminal;
};
FieldCurve TraceSphericalField(const std::vector<WeightedSkySample>& samples, const std::array<double, 3>& seed,
                               const FieldWalkOptions& options, bool reverse, FieldWorkBudget* budget);

struct FieldBand {
  std::array<double, 2> levels{};
  std::array<std::vector<FieldStationaryPoint>, 2> boundaries;
  FieldSolveStatus status = FieldSolveStatus::kInvalidInput;
};
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
