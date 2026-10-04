#ifndef LUMICE_ANALYTIC_DIAGNOSTIC_BATCH_HPP_
#define LUMICE_ANALYTIC_DIAGNOSTIC_BATCH_HPP_

#include <array>
#include <chrono>
#include <cstdint>
#include <vector>

#include "analytic/entry_measure.hpp"
#include "analytic/path_chain.hpp"

namespace lumice::analytic {

struct DiagnosticInputRow {
  CrystalShape crystal;
  std::array<double, 9> pose{};
  std::array<double, 3> incident{};
  double refractive_index = 0;
  uint64_t source_token = 0;
};
struct DiagnosticInterface {
  bool reached = false;
  double incidence = 0;
  double discriminant = 0;  // negative means TIR; internal slots only
  std::array<double, 3> discriminant_pose_gradient{};
  bool pose_gradient_available = false;
  double discriminant_index_derivative = 0;
  double index_derivative_error = 0;
  bool index_derivative_available = false;
  double factor = 0;
  bool factor_available = false;
};
struct DiagnosticOutputRow {
  uint64_t source_token = 0;
  Status input_status = Status::kInvalidValue;
  uint64_t path_evaluations = 0;
  ChainFailure optical_failure = ChainFailure::kNone;
  bool path_valid = false;
  std::array<double, 3> outgoing{};
  // Right-trivialized body rotation, outgoing Cartesian component then axis.
  std::array<std::array<double, 3>, 3> direction_pose_jacobian{};
  bool direction_jacobian_available = false;
  std::array<double, 3> direction_index_derivative{};
  double direction_index_error = 0;
  bool direction_index_available = false;
  double interface_product = 0;
  EntryMeasure entry;
  bool entry_available = false;
  CorridorDiagnostics corridor;
  std::vector<DiagnosticInterface> interfaces;
};
struct DiagnosticBatchOptions {
  bool derivatives = false;
  bool corridor_lineage = false;
  double index_step = 2e-5;
};
// Shared path syntax/options are call errors; invalid geometry, missing faces,
// n/pose/source and optical domain are per-row outcomes. No Scene or product
// classification. Optional native n derivatives report two-step discrepancies,
// not a product acceptance threshold or an integration error bound.
bool EvaluateDiagnosticBatch(const std::vector<int>& faces, const std::vector<DiagnosticInputRow>& rows,
                             const DiagnosticBatchOptions& options, std::vector<DiagnosticOutputRow>* out);

// A local conditional interface equation on SO(3), not a joint caustic or an
// observed colour boundary. The caller owns whether this pose chart belongs
// to its declared source measure. Each accepted iterate must retain A*T > 0.
enum class InterfaceSolveStatus {
  kConverged,
  kInvalidInput,
  kUnavailable,
  kNoSupport,
  kDegenerate,
  kIterationLimit,
  kBudgetExceeded
};
struct InterfaceSolveOptions {
  int internal_slot = 0;
  double residual_tolerance = 1e-10;
  double max_step_rad = .05;
  int max_iterations = 32;
  bool require_positive_entry = true;
};
struct InterfaceStationaryPoint {
  InterfaceSolveStatus status = InterfaceSolveStatus::kInvalidInput;
  DiagnosticInputRow source;
  DiagnosticOutputRow value;
  std::vector<std::array<double, 9>> accepted_poses;
  double travelled_rad = 0;
  uint64_t path_evaluations = 0;
};
InterfaceStationaryPoint CorrectInterfaceEvent(const std::vector<int>& faces, const DiagnosticInputRow& source,
                                               const InterfaceSolveOptions& options, uint64_t max_path_evaluations,
                                               std::chrono::steady_clock::time_point deadline);

enum class InterfaceWalkStop {
  kClosed,
  kAreaThreshold,
  kGeometricContact,
  kOpticalGate,
  kCorrectorFailed,
  kPointLimit,
  kBudgetExceeded,
  kInvalidInput
};
struct InterfaceEventBracket {
  InterfaceWalkStop kind = InterfaceWalkStop::kInvalidInput;
  InterfaceStationaryPoint positive;
  InterfaceStationaryPoint nonpositive;
  double source_width_rad = 0;
};
struct InterfaceCurve {
  std::vector<InterfaceStationaryPoint> points;
  std::vector<InterfaceEventBracket> events;
  InterfaceWalkStop stop = InterfaceWalkStop::kInvalidInput;
  uint64_t path_evaluations = 0;
};
// Walk one direction of a conditional interface curve in the body-incident S2
// quotient. Product-area and raw-geometric-contact brackets remain different
// predicates. Brackets give local source ranges, not exact contact certificates.
InterfaceCurve TraceInterfaceCurve(const std::vector<int>& faces, const DiagnosticInputRow& seed, int slot,
                                   double step_rad, double event_resolution_rad, int max_points, bool reverse,
                                   uint64_t max_path_evaluations, std::chrono::steady_clock::time_point deadline);

// A local minimum of scattering deviation on the incident-direction quotient
// of SO(3). This chart is valid only when the caller's support allows every
// orientation. Shape, wavelength and incident ray remain fixed. Positive
// finite entry measure is required at every numerical stencil and iterate;
// touching an active domain gate is unavailable, never an unconstrained root.
struct DeviationSolveOptions {
  double tolerance_rad = 1e-8;
  double max_step_rad = .1;
  int max_iterations = 32;
};
struct DeviationStationaryPoint {
  InterfaceSolveStatus status = InterfaceSolveStatus::kInvalidInput;
  DiagnosticInputRow source;
  DiagnosticOutputRow value;
  double deviation_rad = 0;
  std::array<double, 2> objective_curvatures{};
  double hessian_error = 0;
  double correction_rad = 0;
  uint64_t path_evaluations = 0;
};
DeviationStationaryPoint CorrectDeviationMinimum(const std::vector<int>& faces, const DiagnosticInputRow& source,
                                                 const DeviationSolveOptions& options, uint64_t max_path_evaluations,
                                                 std::chrono::steady_clock::time_point deadline);

}  // namespace lumice::analytic
#endif  // LUMICE_ANALYTIC_DIAGNOSTIC_BATCH_HPP_
