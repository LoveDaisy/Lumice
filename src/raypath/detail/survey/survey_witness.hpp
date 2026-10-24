#ifndef RAYPATH_DETAIL_SURVEY_WITNESS_H_
#define RAYPATH_DETAIL_SURVEY_WITNESS_H_

// The refused-support arm's witness leg (the evidence asymmetry principle: one positive witness
// proves existence, and a refused partition never reads as zero). The corpus's 3157 is the
// direct evidence it exists for: every member's partition refuses (slab_crease_touching) while
// the declared-source events carry positive weight. The leg samples the declared source itself
// — the antipodal Fibonacci lattice over SO(3), the same sampler the discovery layer owns —
// takes the max-weight event, builds its candidate pose and re-derives the forward chain by the
// diagnostic batch (a second, independent read of the entry measure and the interface product).
// The payload hangs on the SAME refused support card: the card's evidence stays refused, the
// witness sub-block is finite with its own coverage — layered declarations, neither overriding
// the other. The argmax event makes the witness an EXISTENCE statement (one positive-weight
// pose exists), never a distribution statistic; the corpus's target-specific numbers are a
// different protocol and are not asserted here.
//
// Refusals (fail closed, never a defaulted number): the representative does not resolve
// (member_resolution — the same defensive mode the sibling legs use), the lattice keeps no
// event, or the batch evaluation does not confirm the positive chain (path valid, entry > 0,
// interface product > 0) — one mode, no_declared_source_witness, for the last two: the leg
// asked the declared source for one positive witness and did not get one.

#include <string>
#include <vector>

#include "analytic/discovery.hpp"
#include "raypath/scene_to_analytic.hpp"
#include "raypath/survey_result.hpp"

namespace lumice::raypath::survey {

// The witness lattice size (schema, not tuning: the serialized block echoes it). Small on
// purpose — the gate is kept_count > 0, an existence question; the ladder that pinned the
// value and its acceptance line are in the task's progress record.
inline constexpr int kSurveyWitnessSampleCount = 500000;

struct WitnessInput {
  const analytic::FaceNormalTable* table = nullptr;
  const analytic::FacePolygonTable* polygons = nullptr;
  const analytic::CrystalShape* crystal = nullptr;   // the batch evaluation's own shape param
  const std::vector<int>* representative = nullptr;  // the requested literal face sequence
  double incident[3] = { 0.0, 0.0, -1.0 };           // the sun's propagation direction (SunIncidentDirection)
  double refractive_index = 0.0;
  double wavelength_nm = 0.0;  // the coverage declaration's wavelength
  int member_count = 0;        // the class size the card stands for
  int sample_count = kSurveyWitnessSampleCount;
  // The query-face context mode: the lattice leg is T3 quantification, skipped (no witness
  // sub-block at all) when the assembly runs context-only.
  bool context_only = false;
};

// Runs the lattice, the candidate pose and the batch re-evaluation, and fills card->witness —
// finite with the readings, or the refused shape with the mode. Never throws, never writes a
// partial numeric shape.
void AssembleWitness(const WitnessInput& input, SurveyObject* card);

// The refused-support arm's objects discipline: the refusing arm either replaces the cards the
// arm produced before its refusal (kReplaceObjects) or appends the refused card to them
// (kAppendToObjects) — spelled at each call, never a positional bool.
enum class RefusedSupportObjects { kReplaceObjects, kAppendToObjects };

// The refused-support card's shared assembly — the one constructor of the "support refused, the
// witness's positive fact on the SAME card, the downstream chromatic legs unevaluated" shape the
// arm-level refusal paths build (the T1 arm's refused support, the family arm's refused support,
// the measure's out-of-domain refusal). Caller-owned differences arrive explicitly: the texts
// are spelled by the caller (the three refusals' wording differs deliberately — never unified
// here), and `objects_discipline` carries each arm's own objects discipline. The caller keeps
// everything above the document level (block evidence, escape bookkeeping, the return verdict).
void AssembleRefusedSupport(const WitnessInput& witness_input, const std::vector<std::vector<int>>& members,
                            const std::string& refusal_mode, const std::string& chromatic_reason,
                            const std::string& budget_note, RefusedSupportObjects objects_discipline,
                            SurveyResult* out);

}  // namespace lumice::raypath::survey

#endif  // RAYPATH_DETAIL_SURVEY_WITNESS_H_
