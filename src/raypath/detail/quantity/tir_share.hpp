#ifndef LUMICE_RAYPATH_DETAIL_QUANTITY_TIR_SHARE_H_
#define LUMICE_RAYPATH_DETAIL_QUANTITY_TIR_SHARE_H_

// The TIR share leg: the class tint's per-sample average power decomposed by internal-reflection
// mechanism. The question it answers: of the kept events' transmission weight (w = A*T over the
// declared antipodal Fibonacci lattice, the class power-sum's own unit), what share sits on
// poses where EVERY internal interface of the chain is total-internal — the C15 near-TIR weight
// mechanism (blue's lower critical angle widens the TIR window) read as a class-level estimate.
// Per lambda the leg runs one BuildEvents pass (the tint's two-pass shape, same lattice size),
// re-poses every kept event (CandidatePose) and re-derives the chain through the diagnostic
// batch in row chunks (one cooperative poll per chunk boundary — the pass boundary and the
// chunk boundaries are the leg's stop points), then classifies each kept event by the batch's
// per-interface discriminant sign (negative = TIR, internal slots only — the batch row's
// interfaces[0] is the entry and interfaces[last] the exit, whose negative discriminants are
// gate failures, not TIR).
//
// The estimate reads the discovery-side weights on both sides of every share (numerator and
// denominator the same reader, the declared lattice's own unit) — the batch is only the
// classifier, and its entry/interface_product readings never enter the shares (the witness leg
// reports those under its own semantics; a share is an estimator, not a witness). A [0,1]
// share per lambda plus the per-interface decomposition — never a cross-lambda ratio (the
// tint's ratio is a different quantity on a different field family).
//
// Refusals (fail closed, never a defaulted number): the representative does not resolve
// (member_resolution), the path has no internal interface (no_internal_interfaces — the
// question is meaningless for that path shape, a refusal not a zero), a lambda pass keeps no
// weight (zero_band_weight — the tint's degenerate-band mode), or a kept event's batch row
// does not confirm a valid chain with one row per interface (batch_read_unavailable — the two
// readers disagree at a pose, which is a defect to investigate, never a row to drop).

#include <vector>

#include "analytic/path_evaluation.hpp"
#include "raypath/results/quantity.hpp"
#include "raypath/scene_to_analytic.hpp"

// The real namespace's forward declaration (a global-scope one would be shadowed by
// lumice::analytic in the field lookups below).
namespace lumice::analytic {
struct DiscoveryStop;  // the shared stop judgment (analytic/stop.hpp, via discovery.hpp)
}

namespace lumice::raypath::quantity {

// The pinned lattice size (schema, not tuning: the serialized block echoes it) — the class
// tint's own size, so the two power-sum legs read one shared lattice. The ladder that pinned
// the value and its acceptance lines are in the task's progress record.
inline constexpr int kSurveyTirShareSampleCount = 1000000;

struct TirShareInput {
  const analytic::FaceNormalTable* table = nullptr;
  const analytic::FacePolygonTable* polygons = nullptr;
  const analytic::CrystalShape* crystal = nullptr;   // the batch evaluation's own shape param
  const std::vector<int>* representative = nullptr;  // the requested literal face sequence
  double incident[3] = { 0.0, 0.0, -1.0 };           // the sun's propagation direction (SunIncidentDirection)
  int member_count = 0;                              // the class size the representative stands for
  int sample_count = kSurveyTirShareSampleCount;
  // Cooperative stop for the leg's whole run: the per-lambda pass boundary and the in-phase
  // row-chunk pipeline both poll it (an explicit cancel or the query's unified deadline). A
  // cut pass claims nothing — the leg refuses whole, never fabricating the full lattice's
  // shares from a prefix.
  const analytic::DiscoveryStop* stop = nullptr;
};

// Runs the two lambda passes and returns the TIR share record — finite with the shares, or the refused
// shape with the mode. The document-level leg stands alone: it never touches the chromatic
// block (whose arms carry their own legs and block-state rules). Never throws, never writes a
// partial numeric shape.
TirShareResult EvaluateTirShare(const TirShareInput& input);

}  // namespace lumice::raypath::quantity

#endif  // LUMICE_RAYPATH_DETAIL_QUANTITY_TIR_SHARE_H_
