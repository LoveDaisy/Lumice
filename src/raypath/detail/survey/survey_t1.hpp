#ifndef RAYPATH_DETAIL_SURVEY_T1_H_
#define RAYPATH_DETAIL_SURVEY_T1_H_

// The T1 full-dimensional arm of the survey (routing.md: a full-dimensional pose support means
// the direction map is a submersion — the class's structure facts are the delta-axis machine's).
// One axis assembly per class member (the schema3 machine's own — AssembleAxis / SupportRowOf,
// a single authority), the members clustered by SameSupport into the class-level support, then
// the three object cards: support_set (intervals + endpoint mechanism labels + caliber),
// caustic_edge (the min-deviation fold, certified closed-form on a two-face chain), and the
// bright_domain skeleton (its profile readings arrive with the probe leg). A refused or
// disagreeing member set fails closed: one refused support_set card carries the mode, and no
// downstream card is produced.

#include <string>
#include <vector>

#include "analytic/path_evaluation.hpp"
#include "analytic/pose_density.hpp"
#include "raypath/detail/structure/class_support.hpp"  // MemberSupport (the returned rows' type)
#include "raypath/survey_result.hpp"

namespace lumice::raypath::survey {

struct T1Outcome {
  bool ok = false;
  std::string refusal_mode;  // the open refusal mode when !ok (a walk status or partition slug)
};

// The class-level delta support, shared by the full-dimensional and the family arms (the
// partition is orientation-distribution independent — C16's recorded fact, so the family arm
// reads the same machine, not a second one). One axis assembly per member, fail-closed on any
// walk refusal, escape or member disagreement; on success appends the support_set card to
// out->objects and returns the assemblies/rows for the caller's own downstream cards. On
// refusal appends nothing (the caller owns the fail-closed card shape).
// Runs the class's delta machine over `members` (the resolved class) at `refractive_index`,
// with `representative` (the requested face sequence, for the two-face-chain premise) and
// `wavelength_nm` (the coverage declaration's wavelength). Appends the three cards to
// out->objects on success, the one refused support_set card on refusal.
T1Outcome AssembleT1Objects(const analytic::FaceNormalTable& table, const analytic::FacePolygonTable& polygons,
                            const std::vector<std::vector<int>>& members, const std::vector<int>& representative,
                            double refractive_index, double wavelength_nm, SurveyData* out);

// The chromatic layer's v1 legs on the representative: the positional-dispersion table (the
// kernel's wavelength-critical table at the fixed 650/450 pair — every onset row the table
// carried, plus the min-deviation row's own scalars when the class has one) and the diagnose
// sub-block (the kernel's Diagnose — a separate kernel path, its own sub-block, never merged
// into the dispersion fields). The weight-tint leg is the caller's (the T1 arm's class
// power-sum). Refusals keep their leg's own evidence; the block-level evidence turns finite
// once any leg ran.
void AssembleChromaticLayer(const analytic::FaceNormalTable& table, const analytic::FacePolygonTable& polygons,
                            const std::vector<int>& representative, const analytic::PoseDensitySpec& density,
                            int class_size, SurveyData* out);

}  // namespace lumice::raypath::survey

#endif  // RAYPATH_DETAIL_SURVEY_T1_H_
