#ifndef RAYPATH_DETAIL_SURVEY_WITNESS_H_
#define RAYPATH_DETAIL_SURVEY_WITNESS_H_

#include "raypath/detail/quantity/witness.hpp"
#include "raypath/survey_result.hpp"

namespace lumice::raypath::survey {
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
