#ifndef LUMICE_ANALYTIC_DIAGNOSTIC_BATCH_HPP_
#define LUMICE_ANALYTIC_DIAGNOSTIC_BATCH_HPP_

#include <array>
#include <chrono>
#include <cstdint>
#include <vector>

#include "analytic/diagnostic_types.hpp"
#include "analytic/entry_measure.hpp"
#include "analytic/path_chain.hpp"

namespace lumice::analytic {


struct DiagnosticBatchOptions {
  bool derivatives = false;
  bool corridor_lineage = false;
  double index_step = 2e-5;
};
// Shared path syntax/options are call errors; invalid geometry, missing faces,
// n/pose/source and optical domain are per-row outcomes. No Scene or product
// classification. Optional native n derivatives report two-step discrepancies,
// not a product acceptance threshold or an integration error bound.
bool ValidateDiagnosticPath(const int* faces, size_t face_count);
bool EvaluateDiagnosticBatch(const std::vector<int>& faces, const std::vector<DiagnosticInputRow>& rows,
                             const DiagnosticBatchOptions& options, std::vector<DiagnosticOutputRow>* out);

// A local conditional interface equation on SO(3), not a joint caustic or an
// observed colour boundary. The caller owns whether this pose chart belongs
// to its declared source measure. Each accepted iterate must retain A*T > 0.

struct InterfaceSolveOptions {
  int internal_slot = 0;
  double residual_tolerance = 1e-10;
  double max_step_rad = .05;
  int max_iterations = 32;
  bool require_positive_entry = true;
};

InterfaceStationaryPoint CorrectInterfaceEvent(const std::vector<int>& faces, const DiagnosticInputRow& source,
                                               const InterfaceSolveOptions& options, uint64_t max_path_evaluations,
                                               std::chrono::steady_clock::time_point deadline);


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

DeviationStationaryPoint CorrectDeviationMinimum(const std::vector<int>& faces, const DiagnosticInputRow& source,
                                                 const DeviationSolveOptions& options, uint64_t max_path_evaluations,
                                                 std::chrono::steady_clock::time_point deadline);

}  // namespace lumice::analytic
#endif  // LUMICE_ANALYTIC_DIAGNOSTIC_BATCH_HPP_
