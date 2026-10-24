#ifndef LUMICE_RAYPATH_RESULTS_STRUCTURE_HPP_
#define LUMICE_RAYPATH_RESULTS_STRUCTURE_HPP_

#include <string>
#include <vector>

#include "analytic/support_types.hpp"
#include "raypath/domain/measure_geometry.hpp"
namespace lumice::raypath::structure {

struct PartitionedAxis {
  PartitionContext context{};  // the contract value type; kIncomplete names the refusal in
                               // `regime_slug` below — the typed field keeps its kUnset sentinel
                               // (no kernel slug has a contract value; see the module docstring)
  std::string regime_slug;     // kernel EscapeRegimeName slug — the report-side datum (data)
  std::string message;         // the walk's or the escape's LI message text (stable prefixes)
  analytic::WalkStatus walk_status = analytic::WalkStatus::kOk;  // the kind-2 walk's own status
  bool walk_closed = false;                                      // the walk's closure certificate (kOk)
  std::vector<analytic::DeviationInterval> intervals;            // empty unless coverage == kComplete
};

struct ConstantDeltaCurve {
  double d_p = 0.0;                    // radians, at the base index
  int weight_step = 0;                 // the TIR onset's internal step k
  std::vector<double> wavelengths_nm;  // the caller's table (parallel to critical_d_p)
  std::vector<double> critical_d_p;    // the constant value per wavelength (NaN = not found there)
};

struct MemberSupport {
  std::vector<int> member;
  PartitionedAxis axis;                                  // intervals + partition state + slug (data)
  std::vector<analytic::CriticalOnset> endpoint_onsets;  // onsets AT an interval endpoint
  std::vector<ConstantDeltaCurve> constant_curves;       // closed constant-D_P circles inside
};

struct FamilySupport {
  bool shared = false;
  std::vector<analytic::DeviationInterval> intervals;
  std::vector<std::vector<int>> members;
  std::string phi_class_note;
};

struct SupportBlock {
  std::vector<MemberSupport> members;
  std::vector<FamilySupport> families;
};

}  // namespace lumice::raypath::structure

#endif  // LUMICE_RAYPATH_RESULTS_STRUCTURE_HPP_
