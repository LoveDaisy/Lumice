#ifndef DIAGNOSTIC_PROTOTYPE_H_
#define DIAGNOSTIC_PROTOTYPE_H_
#include <stddef.h>
#include <stdint.h>

#include "lumice_analytic_core.h"
#ifdef __cplusplus
extern "C" {
#endif
#if defined(_WIN32)
#define DIAGNOSTIC_PROTOTYPE_API __declspec(dllexport)
#else
#define DIAGNOSTIC_PROTOTYPE_API __attribute__((visibility("default")))
#endif
// Deliberately unpublished consumption probe; no product labels or Scene input.
typedef struct DiagnosticPrototypeSource {
  LUMICE_ANALYTIC_Crystal crystal;
  double pose[9];
  double incident[3];
  double refractive_index;
  uint64_t token;
} DiagnosticPrototypeSource;
typedef struct DiagnosticPrototypeSample {
  uint64_t sample_index;
  uint64_t source_token;
  double direction[3];
  double xyz_weight[3];
  int has_orbit;
  double orbit_axis[3];
} DiagnosticPrototypeSample;
typedef struct DiagnosticPrototypeInterface {
  int reached, factor_available, pose_derivative_available, index_derivative_available;
  double incidence, discriminant, factor, pose_gradient[3], index_derivative, index_error;
} DiagnosticPrototypeInterface;
typedef struct DiagnosticPrototypeOptics {
  DiagnosticPrototypeSource source;
  int solve_status, input_status, path_valid, optical_failure, entry_available, geometry_evaluated;
  double outgoing[3], area, raw_area, area_threshold, interface_product;
  double direction_pose_jacobian[9], direction_index_derivative[3], direction_index_error;
  int direction_pose_available, direction_index_available, interface_count;
  DiagnosticPrototypeInterface interfaces[64];
} DiagnosticPrototypeOptics;
typedef struct DiagnosticPrototypeFieldPoint {
  int status;
  double direction[3], tangent_basis[6], bandwidth_rad;
  // Five covariant jets: X,Y,Z,x,y; each value,g0,g1,H00,H01,H11.
  double jets[30];
  int xy_available;
  double effective_samples_y, correction_rad, log_y_curvatures[2];
} DiagnosticPrototypeFieldPoint;
typedef struct DiagnosticPrototypeResult {
  size_t optical_count, field_count;
  const DiagnosticPrototypeOptics* optical;
  const DiagnosticPrototypeFieldPoint* field;
  uint64_t path_evaluations, component_evaluations;
  int termination;
  void* storage;
} DiagnosticPrototypeResult;
DIAGNOSTIC_PROTOTYPE_API int DiagnosticPrototypeEvaluate(const int* faces, int face_count,
                                                         const DiagnosticPrototypeSource* rows, size_t row_count,
                                                         DiagnosticPrototypeResult* out);
DIAGNOSTIC_PROTOTYPE_API int DiagnosticPrototypeWalkEvent(const int* faces, int face_count,
                                                          const DiagnosticPrototypeSource* source, int slot,
                                                          int reverse, int max_points, uint64_t max_evaluations,
                                                          int budget_ms, DiagnosticPrototypeResult* out);
DIAGNOSTIC_PROTOTYPE_API int DiagnosticPrototypeWalkField(const DiagnosticPrototypeSample* samples, size_t count,
                                                          const double seed[3], int equation, double level,
                                                          double bandwidth_rad, int max_points,
                                                          uint64_t max_evaluations, int budget_ms,
                                                          DiagnosticPrototypeResult* out);
DIAGNOSTIC_PROTOTYPE_API void DiagnosticPrototypeRelease(DiagnosticPrototypeResult* result);
#ifdef __cplusplus
}
#endif
#endif
