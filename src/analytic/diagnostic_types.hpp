#ifndef LUMICE_ANALYTIC_DIAGNOSTIC_TYPES_HPP_
#define LUMICE_ANALYTIC_DIAGNOSTIC_TYPES_HPP_

#include <array>
#include <cstdint>
#include <vector>
namespace lumice::analytic {

enum class Status {
  kOk,
  kInvalidValue,   // a value the caller got wrong: non-finite, non-unit, unused crystal field set, ...
  kInvalidConfig,  // a crystal the engine's closed-form validity gate rejects
};

enum class CrystalShapeKind { kPrism, kPyramid };

struct CrystalShape {
  CrystalShapeKind kind = CrystalShapeKind::kPrism;
  double height = 0.0;
  double face_distance[6]{};
  double upper_h = 0.0;
  double lower_h = 0.0;
  double upper_wedge_deg = 0.0;
  double lower_wedge_deg = 0.0;
};

enum class ChainFailure {
  kNone,
  kPathInfeasible,  // an incidence cosine <= 0: the ray does not enter / reach / leave the face
  kTirBoundary,     // an entry or exit Snell discriminant <= 0
  kNonFinite,       // a margin is not finite
};

enum class EntryMeasureStatus {
  kOk,
  kEntryBackface,      // the incident ray does not enter the entry face
  kExitCriticalAngle,  // the internal ray cannot leave through the exit face
  kCorridorEmpty,      // no line of that internal direction crosses every face polygon
};

struct EntryMeasure {
  double value = 0.0;  // A_P, absolute area in the crystal's length unit squared; 0 unless kOk
  EntryMeasureStatus status = EntryMeasureStatus::kEntryBackface;
  double area_perp_internal = 0.0;  // the corridor footprint perpendicular to the internal direction
};

struct CorridorEdgeSource {
  int path_index = 0;
  int edge_index = 0;  // original corner -> next corner in FacePolygonTable
};

struct CorridorDiagnostics {
  bool geometry_evaluated = false;  // false when an optical gate stopped evaluation
  double raw_area = 0;
  double area_threshold = 0;
  std::array<std::array<double, 3>, 2> projection_basis{};
  std::vector<std::array<double, 2>> vertices;
  // One outgoing edge per vertex. Incoming/outgoing sources identify a local
  // intersection, not a proof of complete source-domain topology.
  std::vector<CorridorEdgeSource> edge_sources;
};

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

enum class InterfaceSolveStatus {
  kConverged,
  kInvalidInput,
  kUnavailable,
  kNoSupport,
  kDegenerate,
  kIterationLimit,
  kBudgetExceeded
};

struct InterfaceStationaryPoint {
  InterfaceSolveStatus status = InterfaceSolveStatus::kInvalidInput;
  DiagnosticInputRow source;
  DiagnosticOutputRow value;
  std::vector<std::array<double, 9>> accepted_poses;
  double travelled_rad = 0;
  uint64_t path_evaluations = 0;
};

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

struct DeviationStationaryPoint {
  InterfaceSolveStatus status = InterfaceSolveStatus::kInvalidInput;
  DiagnosticInputRow source;
  DiagnosticOutputRow value;
  bool deviation_available = false;  // Complete angle/correction/curvature snapshot.
  double deviation_rad = 0;
  std::array<double, 2> objective_curvatures{};
  double hessian_error = 0;
  double correction_rad = 0;
  uint64_t path_evaluations = 0;
};

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_DIAGNOSTIC_TYPES_HPP_
