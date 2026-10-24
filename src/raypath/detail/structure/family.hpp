#ifndef LUMICE_RAYPATH_DETAIL_STRUCTURE_FAMILY_HPP_
#define LUMICE_RAYPATH_DETAIL_STRUCTURE_FAMILY_HPP_

#include "analytic/family_rank.hpp"
#include "raypath/detail/structure/class_support.hpp"
#include "raypath/domain/measure.hpp"
#include "raypath/results/structure.hpp"
#include "raypath/results/survey_records.hpp"

namespace lumice::raypath::structure {

// The declared constants of the routing (schema, not tuning: the ladder block echoes them and
// the assertion benches read them back).
// The rank scan's per-free-dimension grid (715.1's own measurements: 1e6 Jet samples run
// sub-second, so 128^2 per member per level is bounded well under a second; the spacing the
// block reports is the resolution declaration).
inline constexpr int kFamilyLadderGrid = 128;
// The survival scale floor for a curve-like object (the design ruling's cross-section
// reading: a point object has no extent of its own to compare its blur against, and a curve —
// a fold arc, a one-dimensional image track — is that point's continuous extension, its
// zero-thickness limit, so its scale is the cross-section floor, never its overall length):
// 1.0 deg, calibrated by the corpus — C10's zenith 0.3 deg passes with margin (blur ~0.6 deg),
// C16's level-2 zenith 40 deg dies with room (blur ~10.4 deg against the floor).
inline constexpr double kSurvivalSpotFloorDeg = 1.0;
// The probe budget for the extended candidates' anchors (the blur evaluation points of a
// one-dimensional family's image track and the extent basis of a two-dimensional family's
// domain): at most 16 EvaluateFamilyPoint calls, a determinism and cost bound in one.
inline constexpr int kFamilyExtentProbeCount = 16;
// One freeze candidate: a gauss zenith/roll dimension with the width that ranks it.
struct FamilyFreezeCandidate {
  analytic::FamilyDimension dim = analytic::FamilyDimension::kRoll;  // never kAzimuth
  double center_deg = 0.0;  // the freeze value (the parameter centre), normalized legal
  double width_deg = 0.0;   // the measure's sigma — the filter's input, never the rank's
};

// The freeze candidates tightest-first (ties zenith before roll — canonical, deterministic).
std::vector<FamilyFreezeCandidate> FreezeCandidates(const survey::FamilyRoutingMeasure& measure);

struct FamilyStructureInput {
  const analytic::FaceNormalTable* table = nullptr;
  const std::vector<std::vector<int>>* members = nullptr;
  survey::FamilyRoutingMeasure measure;
  bool single_pose = false;
  double incident[3] = { 0.0, 0.0, -1.0 };
  double refractive_index = 0.0;
};
struct FamilyStructureResult {
  std::string refusal_mode;
  std::vector<structure::FamilyLadderLevel> ladder;
  std::vector<structure::FamilySurvivor> survivors;
  std::vector<survey::WarmSeed> seeds;
  bool rank_ran = false;
  std::array<double, 9> single_pose{};
};

FamilyStructureResult EvaluateFamilyStructure(const FamilyStructureInput& input, const ClassSupport& support);

}  // namespace lumice::raypath::structure
#endif
