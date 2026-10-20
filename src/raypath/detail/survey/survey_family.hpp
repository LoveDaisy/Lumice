#ifndef RAYPATH_DETAIL_SURVEY_FAMILY_H_
#define RAYPATH_DETAIL_SURVEY_FAMILY_H_

// The measure-to-family routing's survey side (gap1-family-routing.md, the promoted design
// note — this module is its consumer, never its second author). The routing runs when the T0
// certificate did not close the class and the axis is not the full-sphere random one: the
// measure is read in the concentration language (each dimension {fixed | gauss | uniform_full}
// x (center, width), the two default semantics being the config loader's own facts), the
// out-of-v1-domain shapes are refused with a named mode (never approximated — a bounded
// uniform is not silently swapped for a matching-width gauss), and the in-domain measures walk
// the ladder: level 0 is the actual support (the delta machine is
// orientation-distribution-independent, C16's recorded fact), levels 1..K freeze the k tightest
// gauss candidates (azimuth and uniforms never freeze — the rank entry's own refusal domain)
// and ask the kernel's restricted-family rank there, and the survival filter (structure
// proposes, weight disposes) decides per candidate whether the frozen widths' image blur stays
// inside the object's own angular scale. Survivors carry a band label into objects[]; killed
// candidates are reported in the ladder level that proposed them — every level reports, not
// just the deepest.
//
// The two-channel fact the module holds to: the STRUCTURE domain (this ladder) is wider than
// the density domain (the weight legs). A fixed zenith or roll is legal structure (level 0
// freezes it) but maps to no PoseDensity — its probe/chromatic/tint legs say so by name instead
// of silently skipping, and the witness leg still runs (a support refusal is an unknown, never
// a zero). The all-fixed axis is the 0D arm: the image is EvaluatePath at the one pose (the
// production Euler chart R(a, t, r) = Rz(a - pi) Ry(-t) Rz(r), family_rank.hpp's own), the
// weight the same single-pose read the witness discipline uses — never a band sum.

#include <string>
#include <vector>

#include "analytic/family_rank.hpp"
#include "analytic/path_evaluation.hpp"
#include "raypath/detail/survey/survey_document.hpp"
#include "raypath/scene_to_analytic.hpp"  // PoseDensityConversion

namespace lumice::raypath::survey {

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
// The refusal mode prefix of the v1 domain gate (the mode is an open string; the named shape
// follows the colon).
inline constexpr const char* kMeasureOutsideDomainPrefix = "measure_shape_outside_routing_domain: ";
// The tint method slug of the 0D arm's single-pose direct read (the method registry's third
// spelling, beside the kernel's point-mass names and the class power-sum).
inline constexpr const char* kTintMethodSinglePoseDirect = "single_pose_direct";

// One freeze candidate: a gauss zenith/roll dimension with the width that ranks it.
struct FamilyFreezeCandidate {
  analytic::FamilyDimension dim = analytic::FamilyDimension::kRoll;  // never kAzimuth
  double center_deg = 0.0;  // the freeze value (the parameter centre), normalized legal
  double width_deg = 0.0;   // the measure's sigma — the filter's input, never the rank's
};

// The measure reading: the extraction plus the v1 domain gate, one call (the gate reads the
// same classification the echo reports, so the two can never disagree about what was read).
struct FamilyMeasureReading {
  bool ok = false;
  std::string refusal_mode;  // kMeasureOutsideDomainPrefix + the named shape, when !ok
  bool single_pose = false;  // all three dimensions fixed — the 0D arm (in the domain)
  FamilyRoutingMeasure measure;
};

FamilyMeasureReading ReadFamilyMeasure(const AxisDistribution& axis);

// The freeze candidates tightest-first (ties zenith before roll — canonical, deterministic).
std::vector<FamilyFreezeCandidate> FreezeCandidates(const FamilyRoutingMeasure& measure);

// The family arm's assembly: fills out->family_routing, the level-0 support card (with the
// density tail rows), the ladder's surviving object cards and the arm's own legs (probe /
// chromatic / class tint when the density conversion carries the measure, the named refusals
// when it does not). Fail-closed mirror of the T1 arm: a refused or disagreeing support
// leaves one refused support_set card with the witness sub-block and no ladder cards.
struct FamilyArmInput {
  const analytic::FaceNormalTable* table = nullptr;
  const analytic::FacePolygonTable* polygons = nullptr;
  const analytic::CrystalShape* crystal = nullptr;  // the witness leg's own shape param
  const std::vector<std::vector<int>>* members = nullptr;
  const std::vector<int>* representative = nullptr;  // the request's literal face sequence
  FamilyRoutingMeasure measure;
  bool single_pose = false;
  const PoseDensityConversion* density = nullptr;  // the routing's own conversion
  double incident[3] = { 0.0, 0.0, -1.0 };         // the sun's propagation direction
  double refractive_index = 0.0;
  double wavelength_nm = 0.0;
};

struct FamilyArmOutcome {
  bool support_ok = false;   // false = the fail-closed shape below
  std::string refusal_mode;  // the support's refusal mode when !support_ok
};

FamilyArmOutcome AssembleFamilyRouting(const FamilyArmInput& input, SurveyDocument* out);

}  // namespace lumice::raypath::survey

#endif  // RAYPATH_DETAIL_SURVEY_FAMILY_H_
