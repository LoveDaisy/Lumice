#ifndef LUMICE_RAYPATH_DETAIL_QUANTITY_MC_ESS_H_
#define LUMICE_RAYPATH_DETAIL_QUANTITY_MC_ESS_H_

// The opt-in MC region ESS leg: the MC corroboration sampling's effective sample size per
// solar-radial delta band for THIS raypath class — the quantitative answer to "how noisy is
// the MC evidence in this region" (the corpus's additional-357 close-out named the missing
// leg: pixel variance / sum of squares / ESS were never measured). The mechanical chain is
// entirely the schema-3 side's as-built machinery under the survey's own scope: the survey
// assembly's input snapshot feeds a DiagnosticSampler (fixed seed, prefix-replay reproducible)
// and BuildDiagnosticMeasure fills the measure; the leg then buckets the measure's Y-channel
// weights into 5-degree delta bands and computes each band's Kish effective sample size over
// outer-draw groups — the grouping discipline shared with PresenceEss (one draw is one sampling
// act even when it contributed several member rows); the ratio itself is re-implemented here
// because PresenceEss is kernel-bound (it reads the attribution counts' kernel weights and
// returns one scalar for the whole measure).
//
// A sampling-quality measurement, never a corroboration verdict: the block carries no
// agrees/silent/conflicts word (the MC annotation vocabulary stays on the schema-3 side), and
// the deterministic legs (witness lattice, tint, TIR share) carry no ESS field — a
// deterministic quadrature has no ESS semantics, which is why this leg's `deterministic` is
// false by construction: the field exists to state the leg's own shape against theirs.
//
// Refusals (fail closed, never a defaulted number): the single-lambda spectrum the leg needs
// cannot be assembled from the scene's light (sampler_unavailable — a reference lambda outside
// the continuous band, or a discrete light with no slot at the reference wavelength), or the
// measure came back short of the requested draws (budget_exhausted — the builder sets its flag
// when the deadline stops the run before every requested draw completed; it checks before each
// draw and mid-draw, never after one completes, so a full measure never refuses and an empty
// one always does; a partial measure's band coverage would vary with machine speed, breaking
// the seeded reproducibility the coverage declares).

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "raypath/detail/input_assembly.hpp"
#include "raypath/results/quantity.hpp"

namespace lumice::analytic {
struct DiscoveryStop;  // the shared stop judgment (analytic/stop.hpp, via discovery.hpp)
}

namespace lumice::raypath::quantity {

// The leg's schema constants: the fixed seed and band grid. The public draw cap is shared
// with the C bridge through results/quantity.hpp.
inline constexpr long long kSurveyMcEssSeed = 20261022;
inline constexpr double kSurveyMcEssBandDeg = 5.0;
inline constexpr int kSurveyMcEssBandCount = 36;  // 5-degree bands over [0, 180]

struct McRegionEssInput {
  const InputSnapshot* snapshot = nullptr;  // the survey assembly's captured scope
  double wavelength_nm = 0.0;               // the survey's reference lambda (the leg's single lambda)
  double sun_dir[3] = { 0.0, 0.0, 1.0 };    // AT the sun (the negated SunIncidentDirection)
  int member_count = 0;                     // the class size (the declared member filter bound)
  long long draws = 0;                      // requested outer draws, in (0, kSurveyMcEssMaxDraws]
  int budget_ms = 0;                        // the survey request's budget, as the deadline guardrail
  // Cooperative stop riding into the sampler's draw loop beside the guardrail deadline (the
  // single judgment: the stop carries both the deadline and any cancel token).
  const analytic::DiscoveryStop* stop = nullptr;
  std::optional<std::chrono::steady_clock::time_point> query_deadline;
};

// Runs the sampler, the measure and the band aggregation, and returns the MC ESS record —
// finite with the bands, or the refused shape with the mode. The document-level leg stands
// alone: it never touches the chromatic block. Never throws, never writes a partial numeric
// shape.
McRegionEssResult EvaluateMcRegionEss(const McRegionEssInput& input);

}  // namespace lumice::raypath::quantity

#endif  // LUMICE_RAYPATH_DETAIL_QUANTITY_MC_ESS_H_
