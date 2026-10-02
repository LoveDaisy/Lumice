#ifndef LUMICE_ANALYTIC_DIAGNOSTIC_FIELD_HPP_
#define LUMICE_ANALYTIC_DIAGNOSTIC_FIELD_HPP_

// General per-pose diagnostic field over one concrete geometry/path. The public C ABI owns batch
// layout and storage; this kernel owns the numerical meaning and derivative availability.

#include <cstdint>
#include <string>
#include <vector>

#include "analytic/entry_measure.hpp"
#include "analytic/path_evaluation.hpp"

namespace lumice::analytic {

enum class DiagnosticPathStatus {
  kOk,
  kPathInfeasible,
  kRefractionCritical,
  kNonFinite,
};

enum class DiagnosticEntryStatus {
  kNotEvaluated,
  kOk,
  kEntryBackface,
  kExitCriticalAngle,
  kCorridorEmpty,
};

enum class DiagnosticInterfaceKind { kEntryTransmission, kInternalReflection, kExitTransmission };

struct DiagnosticRowInput {
  double refractive_index = 0.0;
  double incident_direction[3]{};
  double pose[9]{};
};

struct DiagnosticInterface {
  int face_number = 0;
  DiagnosticInterfaceKind kind = DiagnosticInterfaceKind::kEntryTransmission;
  double coefficient = 0.0;
  int pose_derivative_available = 0;
  int index_derivative_available = 0;
  double pose_gradient[3]{};
  double index_derivative = 0.0;
};

struct DiagnosticMargin {
  std::string name;
  int interface_index = 0;
  double value = 0.0;
  int pose_derivative_available = 0;
  int index_derivative_available = 0;
  double pose_gradient[3]{};
  double index_derivative = 0.0;
};

struct DiagnosticFieldResult {
  DiagnosticPathStatus path_status = DiagnosticPathStatus::kNonFinite;
  DiagnosticEntryStatus entry_status = DiagnosticEntryStatus::kNotEvaluated;
  double outgoing_direction[3]{};
  double entry_measure = 0.0;
  double fresnel_weight = 0.0;
  std::vector<DiagnosticInterface> interfaces;
  std::vector<DiagnosticMargin> domain_margins;
  std::vector<DiagnosticMargin> tir_margins;

  int direction_pose_jacobian_available = 0;
  int direction_pose_hessian_available = 0;
  int direction_index_derivative_available = 0;
  int entry_pose_gradient_available = 0;
  int entry_index_derivative_available = 0;
  double direction_pose_jacobian[9]{};
  double direction_pose_hessian[27]{};
  double direction_index_derivative[3]{};
  double entry_pose_gradient[3]{};
  double entry_index_derivative = 0.0;
};

// Prepared only for the duration of one public batch: common closed-form geometry, resolved face
// sequence and unfolded corridor are constructed once, with no public handle or global cache.
class DiagnosticField {
 public:
  DiagnosticField(const FaceNormalTable& normals, const FacePolygonTable& polygons, const int* faces, const int* slots,
                  int face_count);

  DiagnosticFieldResult Evaluate(const DiagnosticRowInput& input);

 private:
  struct Values;

  Values EvaluateValues(const DiagnosticRowInput& input);
  void FillDerivatives(const DiagnosticRowInput& input, const Values& base, DiagnosticFieldResult* out);

  const FaceNormalTable& normals_;
  std::vector<int> faces_;
  std::vector<int> slots_;
  Corridor corridor_;
};

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_DIAGNOSTIC_FIELD_HPP_
