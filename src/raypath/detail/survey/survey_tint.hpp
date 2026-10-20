#ifndef RAYPATH_DETAIL_SURVEY_TINT_H_
#define RAYPATH_DETAIL_SURVEY_TINT_H_

// The chromatic layer's weight-tint leg on the identity arm. A rank-0 path maps every pose to
// the sun direction, so the class's sky object has one position and the only chromatic question
// left is the weight: how the transmission tilts across the fixed 650/450 index pair. The
// answer is the point-mass branch of analytic::BandSum (band_sum.hpp section 5) run twice on
// the same lattice size — once per lambda through its refractive index — with an empty pixel
// table (the mass m is what the ratio reads; a point mass placed into pixels is a rendering
// concern this leg does not have). Under a non-uniform density the kernel's deterministic
// psi-average computes both masses over the same discretization nodes, so the ratio's
// quadrature error cancels to first order; under the random family both masses are lattice
// means over the same kept event multiset.
//
// Refusals (fail closed, never a defaulted number): the axis maps to no pose density
// (density_not_expressible — the fixed-pose degenerate is an honest refusal, not a physical
// impossibility), the kernel does not see the point mass (not_rank_zero — the caller's T0
// verdict and the kernel's own rank check disagree), or a band's mass degenerates (the ratio
// would be no number, and a silent inf/null must not reach the document —
// documented-unreachable: a kept-event mass is positive whenever the rank-0 quadrature ran,
// so this guard exists for the degenerate-band shape, not for a reachable input).

#include <vector>

#include "analytic/band_sum.hpp"
#include "analytic/path_evaluation.hpp"
#include "raypath/detail/survey/survey_document.hpp"
#include "raypath/scene_to_analytic.hpp"

namespace lumice::raypath::survey {

// The tint leg's caliber constant (schema, not tuning: the serialized block echoes it and
// the assertion bench reads it back). The wavelength pair itself is NOT defined here — it is
// the chromatic layer's single authority in survey_document.hpp (kChromaticRedNm /
// kChromaticBlueNm), shared with the positional-dispersion leg; only the lattice size is
// tint-local.
inline constexpr int kSurveyTintSampleCount = 1000000;  // the band_sum lattice size (its `n`)

// The tint method slug of the 0D arm's single-pose direct read (the method registry's third
// spelling, beside the kernel's point-mass names and the class power-sum).
inline constexpr const char* kTintMethodSinglePoseDirect = "single_pose_direct";

struct TintInput {
  const analytic::FaceNormalTable* table = nullptr;
  const analytic::FacePolygonTable* polygons = nullptr;
  const std::vector<int>* representative = nullptr;  // the requested literal face sequence
  const PoseDensityConversion* density = nullptr;    // the caller's gated conversion; !Ok() ⇒ refused
  double incident[3] = { 0.0, 0.0, -1.0 };           // the sun's propagation direction (SunIncidentDirection)
  int member_count = 0;                              // the class size the representative stands for
  int sample_count = kSurveyTintSampleCount;
};

// Computes the two point-mass band sums and fills out->weight_tint; sets the chromatic block's
// own state (the block mirrors its only leg on this arm). Refusals keep the document honest —
// a refusal mode, never a defaulted ratio.
void AssembleWeightTint(const TintInput& input, SurveyDocument* out);

// The tint leg's caliber constant on the T1 arm (schema: the serialized block echoes it; the
// ladder that pinned the value and its acceptance lines are in the task's progress record).
inline constexpr int kSurveyClassTintSampleCount = 1000000;

// The T1 arm's class tint: the direction the class's transmission WEIGHT tilts across the fixed
// pair, read as a class power sum over the declared source. Two BuildEvents calls — once per
// lambda through its refractive index, same lattice size, one shared lattice (the declared
// Haar measure's own sampler) — and the ratio is the blue sum over the red sum: sum of the
// kept events' w = A*T, divided by the lattice size on both sides, so the per-sample average
// powers carry it. This is the C15/C06 near-TIR weight mechanism at class level, a different
// reading (and a different field family) from the identity arm's point-mass masses; the two
// calibers must not be compared. The caller is the T1 arm's full-dimensional branch (the
// declared family is the random one); the leg refuses, never defaults: an unresolvable
// representative (member_resolution), or a degenerate band (zero_band_weight — no silent
// division by zero reaches the document). A refusal here does not flip the chromatic block:
// on this arm the block has other legs with real output, and the sub-block carries its own
// state (the block-state rule: finite once any leg produced output).
struct ClassTintInput {
  const analytic::FaceNormalTable* table = nullptr;
  const analytic::FacePolygonTable* polygons = nullptr;
  const std::vector<int>* representative = nullptr;  // the requested literal face sequence
  double incident[3] = { 0.0, 0.0, -1.0 };           // the sun's propagation direction (SunIncidentDirection)
  int member_count = 0;                              // the class size the representative stands for
  int sample_count = kSurveyClassTintSampleCount;
};

void AssembleClassTint(const ClassTintInput& input, SurveyDocument* out);

// The 0D arm's weight tint: the single-pose direct read (w = A*T at each lambda of the fixed
// pair, the diagnostic batch's own re-evaluation — the witness's reading discipline), never a
// band sum (the design's own sentence). The caller is the family arm's 0D branch (the all-fixed
// axis, one pose from the production Euler chart).
struct SinglePoseTintInput {
  const analytic::CrystalShape* crystal = nullptr;   // the batch evaluation's own shape param
  const std::vector<int>* representative = nullptr;  // the requested literal face sequence
  double pose[9] = {};                               // the axis's single pose
  double incident[3] = { 0.0, 0.0, -1.0 };           // the sun's propagation direction
  int member_count = 0;                              // the class size the card stands for
};

// Fills out->weight_tint and mirrors the chromatic block's own state (the tint is the 0D arm's
// only chromatic leg). Refusals keep the document honest — single_pose_weight_unavailable, the
// weight is unavailable, never defaulted.
void AssembleSinglePoseTint(const SinglePoseTintInput& input, SurveyDocument* out);

}  // namespace lumice::raypath::survey

#endif  // RAYPATH_DETAIL_SURVEY_TINT_H_
