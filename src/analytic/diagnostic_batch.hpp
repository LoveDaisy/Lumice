#ifndef LUMICE_ANALYTIC_DIAGNOSTIC_BATCH_HPP_
#define LUMICE_ANALYTIC_DIAGNOSTIC_BATCH_HPP_

#include <array>
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

}  // namespace lumice::analytic
#endif  // LUMICE_ANALYTIC_DIAGNOSTIC_BATCH_HPP_
