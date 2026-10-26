#ifndef LUMICE_RAYPATH_RESULTS_STRUCTURE_HPP_
#define LUMICE_RAYPATH_RESULTS_STRUCTURE_HPP_

#include <string>
#include <vector>

#include "analytic/quantity_types.hpp"
#include "analytic/support_types.hpp"
#include "raypath/domain/measure.hpp"
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

inline const char* DimName(analytic::FamilyDimension dim) {
  switch (dim) {
    case analytic::FamilyDimension::kZenith:
      return "zenith";
    case analytic::FamilyDimension::kAzimuth:
      return "azimuth";
    case analytic::FamilyDimension::kRoll:
      return "roll";
  }
  return "roll";
}

enum class FamilyImageKind { kCollapsePoint, kCausticEdge, kRegularImage, kBrightDomain };

struct FamilyFrozenDim {
  analytic::FamilyDimension dim = analytic::FamilyDimension::kRoll;
  double center_deg = 0.0;
  double width_deg = 0.0;
};

struct FamilyBandVerdict {
  std::string dim;
  std::string verdict;
};

struct FamilyKilledCandidate {
  std::vector<int> member;
  FamilyImageKind kind = FamilyImageKind::kCollapsePoint;
  bool has_position = false;
  double position_altitude_deg = 0.0;
  double position_azimuth_deg = 0.0;
  bool blur_measured = false;
  double blur_deg = 0.0;
  double scale_deg = 0.0;
  bool clamped = false;
  std::string reason;
  std::vector<FamilyBandVerdict> verdicts;
};

struct FamilyLadderLevel {
  int level = 0;
  std::vector<FamilyFrozenDim> frozen_dims;
  std::vector<analytic::FamilyDimension> free_dims;
  bool rank_asked = false;
  int generic_rank = -1;
  int grid = 0;
  double grid_spacing_deg[2] = { 0.0, 0.0 };
  int samples = 0;
  int valid_samples = 0;
  int rank_refused_members = 0;
  int objects_proposed = 0;
  int objects_killed = 0;
  std::vector<FamilyKilledCandidate> killed;
};

struct FamilySurvivor {
  FamilyImageKind kind = FamilyImageKind::kCollapsePoint;
  std::vector<int> member;
  bool has_position = false;
  double position_altitude_deg = 0.0;
  double position_azimuth_deg = 0.0;
  int level = 0;
  std::vector<FamilyBandVerdict> frozen;
  double blur_deg = 0.0;
  double scale_deg = 0.0;
  bool clamped = false;
  int sample_count = 0;
  int crossing_count = 0;
  bool has_domain = false;
  double domain_lower_deg = 0.0;
  double domain_upper_deg = 0.0;
};

}  // namespace lumice::raypath::structure

#endif  // LUMICE_RAYPATH_RESULTS_STRUCTURE_HPP_
