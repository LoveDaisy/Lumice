#ifndef LUMICE_ANALYTIC_FIELD_TYPES_HPP_
#define LUMICE_ANALYTIC_FIELD_TYPES_HPP_

#include <array>
#include <cstdint>
#include <optional>
#include <vector>
namespace lumice::analytic {

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

enum class FieldEquation { kLogYPeak, kLogYRidge, kChromaticityX, kChromaticityY };

enum class FieldSolveStatus { kConverged, kInvalidInput, kNoSignal, kDegenerate, kIterationLimit, kBudgetExceeded };

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

enum class FieldWalkStop { kClosed, kObservationCensored, kCorrectorFailed, kPointLimit, kInvalidInput };

struct FieldCurve {
  std::vector<FieldStationaryPoint> points;
  FieldWalkStop stop = FieldWalkStop::kInvalidInput;
  // The failed/censored trial is retained separately, never appended as a
  // converged curve point. Observation censoring is not a physical source edge.
  FieldStationaryPoint terminal;
};

struct FieldBand {
  std::array<double, 2> levels{};
  std::array<std::vector<FieldStationaryPoint>, 2> boundaries;
  FieldSolveStatus status = FieldSolveStatus::kInvalidInput;
};

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_FIELD_TYPES_HPP_
