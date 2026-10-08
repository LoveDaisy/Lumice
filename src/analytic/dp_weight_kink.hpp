#ifndef LUMICE_ANALYTIC_DP_WEIGHT_KINK_HPP_
#define LUMICE_ANALYTIC_DP_WEIGHT_KINK_HPP_

// Weight kinks: the TIR onsets C_k = {internal_k_tir_discriminant = 0} in the closure of U_P (LI
// dp_field.weight_kink, ported step for step for scrum 660.3). The third kind of critical line of
// the field layer, next to the critical points of D_P (dp_field) and the boundary dU_P
// (dp_boundary). A partial internal reflection keeps the point in U_P: the internal step k's
// Fresnel R_k is 1 on the total side and drops continuously, with an unbounded normal derivative,
// on the partial side. C_k is therefore a KINK of the path's weight, not of its support, and it
// moves with n while the gates of a slab path do not. Nothing here touches dU_P or the topology
// certificate of dp_partition.
//
// Two ways to find C_k (the same split as dp_boundary's curve kinds):
//  - closed form: when m_k = R_{k-1}^T n_k (the step's unfolded incidence normal,
//    dp_boundary::IncidenceNormals) is orthogonal to the entry normal n_a, the entry refraction
//    keeps the tangential component, incidence_cosine = -(m_k . u) / n and disc_k = n^2 - 1 -
//    (m_k . u)^2: C_k inside the incidence gate is the small circle m_k . u = -sqrt(n^2 - 1),
//    clipped to U_P by bisection on its angle. It is the AUTHORITY for every path whose m_k
//    passes the check (kGreatCircleAtol, plus the residual on the circle); on a single-mirror
//    slab D_P = 2 arcsin |m . u| is constant on it. For n^2 - 1 >= 1 the circle is empty by
//    construction (disc_k >= 0 on all of S^2) — said in the note, no NaN comparison.
//  - marched: otherwise, predictor-corrector along the zero set of disc_k with
//    WalkZeroSet, from lattice seeds near the zero set, both ways until a gate of U_P stops the
//    walk or it closes. Seeds within a few steps of a walked arc are dropped, the rest start new
//    arcs; the arcs found are NOT certified to be all of C_k (one closed loop is no proof of
//    completeness) — that honesty is the coverage field's content.
//
// THE COMPLETENESS DECLARATION IS A FIRST-CLASS FIELD (AC2, conclusions section 4 open item 4):
// `coverage` says which of the two ways produced the curve — kClosedFormAuthority (the circle,
// clipped; complete by construction) or kMarchedUncertified (the walk; structurally not a
// completeness claim) — `failed_seeds` counts the marched seeds whose walk refused (the others
// are still walked; Complete() is the no-failure roll-up, NOT a completeness certificate), `note`
// carries the first refusal's message, and each arc's `end_gates` name where an open arc stopped.
// A truncated walk (kStepsExhausted) is a refusal the seed loop counts, never a silent cap.
//
// Arc points lie in the closure of U_P, and on a chain whose C_k coincides algebraically with the
// exit-Snell zero set (3-5-6-7: the internal-1 onset IS the exit-Snell boundary piece) the exit
// refraction's square root reads a rounding-negative discriminant over whole stretches of the
// arc: those points' values are the closure limit of the exit-Snell convention (RoutedDeviation);
// a point off the closure fails the arc (fail closed, LI raises).
//
// Internal header of the analytic kernel: nothing here is part of the C ABI. Dependency
// direction: this header -> dp_boundary -> dp_field.

#include <string>
#include <vector>

#include "analytic/dp_boundary.hpp"

namespace lumice::analytic {

// ---- constants (LI weight_kink.py, verbatim) --------------------------------------------------------------

// Samples of a closed-form circle before its U_P arcs are cut by bisection (0.1 deg apart; LI
// CIRCLE_SAMPLES).
constexpr int kCircleSamples = 3600;
// Bisection on the circle angle stops below this (rad; ARC_END_ATOL).
constexpr double kArcEndAtol = 1e-13;
// The closed form is accepted when the discriminant on the circle is within this of zero (LI
// CIRCLE_RESIDUAL_ATOL).
constexpr double kCircleResidualAtol = 1e-12;
// Marched walk: lattice seeds with |disc_k| below this start a walk (the discriminant is O(1)
// over S^2; LI SEED_BAND).
constexpr double kSeedBand = 0.02;
// A seed within this many walk steps of an arc already walked starts nothing new (LI
// SEED_COVERED_STEPS).
constexpr double kSeedCoveredSteps = 4.0;

// ---- the curves --------------------------------------------------------------------------------------------

// Which way a curve was found — the completeness declaration's first half (AC2).
enum class KinkCoverage {
  kClosedFormAuthority,  // the small circle m . u = -sqrt(n^2 - 1), clipped to U_P: complete
  kMarchedUncertified,   // the walk: the arcs found are not certified to be all of C_k
};

// One arc of C_k inside the closure of U_P (LI KinkArc): `points` (3N) in curve order, `values`
// D_P there (radians, the closure convention of the module docstring), `closed` for a whole loop
// inside U_P (then `end_gates` is {-1, -1}), otherwise `end_gates` names the gate that stops the
// arc at its first and last point (-1: no gate).
struct KinkArc {
  std::vector<double> points;
  std::vector<double> values;
  bool closed = false;
  int end_gates[2] = { -1, -1 };
};

// C_k: the TIR onset of internal step `step` at refractive index `index` (LI KinkCurve).
// `normal` is m_k for the closed form (has_normal false when marched). `arcs` may be empty: the
// onset misses U_P (every pose of the path reflects totally, or none does, at that step; `note`
// says so when n >= sqrt 2 takes the onset off S^2). `failed_seeds` counts the marched seeds
// whose walk refused; the other seeds are still walked, and `note` carries the first message
// (reported, not hidden). `status` is the closed form's own refusal (an arc value off the
// closure of U_P — LI lets that raise out of weight_kinks; here it is data): a marched seed's
// refusal is counted in failed_seeds instead and never sets it.
struct KinkCurve {
  int step = 0;
  int margin = -1;
  double index = 0.0;
  KinkCoverage coverage = KinkCoverage::kMarchedUncertified;
  bool has_normal = false;
  double normal[3] = {};
  std::vector<KinkArc> arcs;
  std::string note;
  int failed_seeds = 0;
  WalkStatus status = WalkStatus::kOk;

  // No seed walk failed. NOT a completeness certificate: the marched arcs are uncertified either
  // way (the module docstring's honesty rule — LI KinkCurve.complete).
  bool Complete() const { return failed_seeds == 0; }
  // sigma_k: the width max - min of D_P on C_k (radians); quiet-NaN without arcs, 0 on a
  // single-mirror slab (LI spread).
  double Spread() const;
};

// The knobs of the kink search with LI's defaults (lattice_n of the marched seeding, the shared
// walk step and budget — LI weight_kinks' lattice_n and walk_zero_set's step).
struct KinkOptions {
  int lattice_n = 20000;
  double step = kWalkStepRad;
  int max_walk_steps = kMaxWalkSteps;
};

// The both-ways walk of one marched seed, as an injectable seam: LI's failure-counting test
// monkeypatches _walk_both_ways; the port makes the seam an explicit callable with opaque
// context. The built-in implementation walks both ways and fills the arc; an injected one may
// return a refusal to exercise the counting.
struct KinkSeedArc {
  WalkStatus status = WalkStatus::kOk;  // != kOk: this seed's walk refused (counted, reported)
  std::string message;                  // LI's error text
  KinkArc arc;
};
using KinkBothWaysFn = KinkSeedArc (*)(const BoundaryWalker& walker, const double start[3], int margin,
                                       const KinkOptions& options, void* user);

// The built-in both-ways walk (what a null `both_ways` runs): forward with stop_at the seed, then
// backward from it; an open arc's two ends name their stopping gates. Exposed so an injected
// wrapper can delegate to it between its own refusals (the port of LI's monkeypatch-then-call
// pattern).
KinkSeedArc KinkWalkBothWays(const BoundaryWalker& walker, const double start[3], int margin,
                             const KinkOptions& options, void* user);

// One KinkCurve per internal reflection of the sequence, in step order (empty for a-b paths; LI
// weight_kinks): the closed form where m_k passes the check, the march otherwise.
std::vector<KinkCurve> WeightKinks(const DeviationField& field, const KinkOptions& options,
                                   KinkBothWaysFn both_ways = nullptr, void* user = nullptr);

// C_k of internal step `step` by the walk alone, whatever its normal — the closed form's
// cross-check (LI marched_kink).
KinkCurve MarchedKink(const DeviationField& field, int step, const KinkOptions& options,
                      KinkBothWaysFn both_ways = nullptr, void* user = nullptr);

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_DP_WEIGHT_KINK_HPP_
