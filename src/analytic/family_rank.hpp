#ifndef LUMICE_ANALYTIC_FAMILY_RANK_HPP_
#define LUMICE_ANALYTIC_FAMILY_RANK_HPP_

// The rank + critical structure of a face sequence on a RESTRICTED pose family: the map from a
// caller-chosen set of free orientation dimensions (the family) to the sky image, its per-point
// rank class, the family's generic rank, and the rank-drop set's connected components (the fold
// locus and the collapse components). This is the ONE kernel-side primitive of the
// measure-to-family routing (the ladder of freezing the concentrated dimensions, one level at a
// time, and asking what image structure survives each freeze) — the piece that decides whether a
// family's contribution is a regular two-dimensional patch, a fold arc, a one-dimensional ring,
// or a collapse point.
//
// LUMICE-BORN, not an LI port — the deliberate contrast with dp_focus.hpp's "JAX is the
// authority": no LI counterpart exists (LI's rank vocabulary is the full-SO3 halo_map_rank of
// Classify and the one-dimensional latitude-circle entries; a two-dimensional restricted family
// has no LI entry), so the specification authority is Lumice's own
// doc/raypath/gap1-family-routing.md (the promoted design note). The registered discipline
// (doc/interfaces/analytic-api.md section 9): if LI's articles ever need this primitive, a
// temporary parallel JAX version may cross-check against it through parity fixtures — named by
// their calibre, derived from this specification, never independently re-derived — until the
// binding consumes liblumice_analytic and the JAX side retires.
//
// The input vocabulary is STRUCTURE-side, deliberately not PoseDensitySpec (that is the
// weight-side vocabulary): each orientation dimension is {fixed(center) | free} with no width —
// a width never enters a rank question. The routing v1 domain (gap1-family-routing.md): azimuth
// is the dimension the ladder never freezes (it stays free unless everything is frozen), so the
// accepted shapes are azimuth-free families of one dimension ({azimuth}, with zenith and roll
// pinned at their centers — the Parry circle) or two ({azimuth, zenith} — Lowitz after freezing
// roll; {azimuth, roll} — the spin chart after freezing zenith), and every other shape is a
// NAMED refusal, never a silent approximation.
//
// Semantics (the angles are degrees, PoseDensitySpec's unit):
//   * The pose chart is the production Euler convention R(a, t, r) = Rz(a - pi) Ry(-t) Rz(r)
//     (body -> world, row-major), the one the declared densities read (zenith t = arccos(e3),
//     roll r = atan2(-e2, e1) of the pose's third row) — so a family center given in the
//     density's coordinates lands on the poses that density concentrates about.
//   * Free dimensions are chart coordinates seeded as Jet variables through so3.hpp's Rodrigues
//     (the Taylor branch is Jet-friendly by design), so the image Jacobian is the exact
//     derivative of the SAME chain (path_chain.hpp TracePathChain, kFirstFailure — no second
//     hand-derived derivative); fixed dimensions sit at their centers.
//   * Per point: singular values sigma1 >= sigma2 of the free-dimension Jacobian of the outgoing
//     direction restricted to the image tangent plane (the rows are tangent to the image sphere
//     by construction, so the restriction is the Gram eigenvalues — so3::SvdTwoByThree's form,
//     the same relative-precision discipline as FiberResult's singular values). sigma2 >=
//     kFamilyRankSigmaTolerance is class 2 (a regular image patch), sigma1 >= tolerance > sigma2
//     class 1 (a fold or a one-dimensional image), sigma1 below it class 0 (a collapse). The
//     tolerance is a Lumice-born judgement (no LI authority to copy) at the continuation
//     layer's singular_value_tolerance calibre — the in-house regularity bar.
//   * The family's GENERIC rank is the maximum class over the valid samples. The critical
//     structure is the rank-drop set: for a two-dimensional family, the fold locus is located by
//     the sign change of the image-orientation Jacobian across grid edges (a fold reverses
//     orientation; the sign is basis-independent — every tangent basis the code builds is
//     right-handed about the outgoing direction) and refined by bisection along the edge to
//     kFoldBisectionSteps; the crossings are grouped into connected components at grid
//     resolution. Collapse components (class-0 samples, four-connected on the grid) are recorded
//     with a representative each.
//   * A pole chart degeneration is DATA, not an error: at zenith 0 the (azimuth, roll) chart
//     tangents are the same physical spin generator, the per-point sigma2 is structurally ~0 and
//     the family correctly reports a lower rank (the C10 spin chart reports its collapse or
//     one-dimensional image; no NaN, no silently skipped sample).
//   * A sample whose chain gates reject (infeasible pose, TIR) is invalid data — counted, never
//     an error, the same treatment as RestrictedFamilyCurve's routed_nonfinite. A family with
//     no valid sample reports generic_rank -1.
//   * Deterministic: a pure product grid, no random numbers anywhere.
//
// The declared-error tier of the sampled structures is the two-grid protocol (dp_contour's
// |I_N - I_2N| discipline): a consumer integrates nothing here, so the declared quantity is the
// fold representative's IMAGE POSITION shift between a run at grid N and a run at 2N, reported
// by the consumer from two calls — the spacing each run reports (grid_spacing_deg) is the
// resolution declaration. The unit tests pin its measured value. What the scan does NOT certify:
// the fold locus is sampled at grid resolution and refined per edge — a curve branch that never
// crosses a grid edge, or two crossings of one edge, are not found; the recorded coverage
// (crossing counts, spacing) is the honesty about that, mirroring the corpus's own scope note.
//
// Internal header of the analytic kernel: nothing here is part of the C ABI. The result structs
// are kept POD-flat (escape-as-data statuses, no handles, no cross-call state) so a future
// exposure is a pure additive v9 — the reopen conditions are registered in analytic-api.md
// section 9. Dependency direction: family_rank -> path_evaluation (the table and chain),
// so3, jet, path_rank (the tolerance vocabulary's neighbour, read-only).

#include <vector>

#include "analytic/path_evaluation.hpp"

namespace lumice::analytic {

// The rank-class singular-value bar (sigma >= this counts as nonzero): the continuation layer's
// singular_value_tolerance calibre. A pinned family's collapsed rows sit at ~1e-15 (exact
// cancellation to rounding), a generic image's at O(0.1), so the bar separates them by many
// orders of magnitude in both directions.
constexpr double kFamilyRankSigmaTolerance = 1e-8;
// The grid count per free dimension, a call error beyond the bound (the largest product grid
// stays ~1e6 samples, sub-second in Jet arithmetic — refusing is cheaper than truncating
// silently). The minimum 2 keeps the zenith interval's two endpoints representable.
constexpr int kFamilyMinGridPerDimension = 2;
constexpr int kFamilyMaxGridPerDimension = 1024;
// The fold-locus bisection along a grid edge, the closure crossing's iteration count calibre
// (fiber_continuation.hpp kClosureCrossingBisectionSteps): 52 halvings take the bracket far
// below double resolution for every legal spacing.
constexpr int kFoldBisectionSteps = 52;

// One orientation dimension of the family.
enum class FamilyDimensionMode {
  kFixed,  // pinned at center_deg (a free dimension's center must be exactly 0, the C ABI's
           // unused-field convention, doc/interfaces/analytic-api.md section 4.1)
  kFree,
};
struct FamilyDimensionSpec {
  FamilyDimensionMode mode = FamilyDimensionMode::kFree;
  double center_deg = 0.0;
};

// The family: which of {zenith, azimuth, roll} is free, and where the frozen ones sit. Valid
// centers: zenith [0, 180], azimuth [0, 360), roll [0, 360).
struct RestrictedFamilySpec {
  FamilyDimensionSpec zenith;
  FamilyDimensionSpec azimuth;
  FamilyDimensionSpec roll;
};

// Empty when the spec is well-formed (every field finite, every fixed center in its range, every
// free center exactly 0), else a sentence naming the offending field — PoseDensityError's shape.
const char* FamilySpecError(const RestrictedFamilySpec& spec);

// Every refusal is data, not an error code.
enum class FamilyRankStatus {
  kOk,
  // All three dimensions fixed: one pose. EvaluatePath at R(azimuth, zenith, roll) is the entry;
  // a rank over a point is not a question this module answers (the AC's all-frozen anchor).
  kRefusedSinglePose,
  // All three free: the whole of SO(3) — Classify's domain (dp_focus.hpp), named so the caller
  // goes to the existing entry instead of a second full-SO3 judgement.
  kRefusedFullSo3,
  // Azimuth fixed while another dimension is free: outside the routing's v1 domain (the ladder
  // never freezes azimuth); refused rather than silently approximated, widen the domain in the
  // spec document first.
  kRefusedAzimuthFrozen,
  // A malformed spec (FamilySpecError's conditions).
  kRefusedInvalidSpec,
  // The grid count outside [kFamilyMinGridPerDimension, kFamilyMaxGridPerDimension] — a caller
  // bug, not a verdict.
  kRefusedGridOutOfBounds,
};
const char* FamilyRankStatusName(FamilyRankStatus status);

enum class FamilyDimension {
  kZenith,
  kAzimuth,
  kRoll,
};

// One located point of the fold locus (a two-dimensional family's rank-drop curve): the free
// parameters, the refined pose, the sky position of -outgoing (altitude above the world
// equator, azimuth atan2(y, x) of the image direction — sky_direction.hpp's world convention),
// the singular values there, and the orientation Jacobian's residual value after the bisection
// (0 to bracket resolution; |it| equals sigma1 * sigma2 by Lagrange, a consistency read).
struct FoldPointRecord {
  double params_deg[2] = { 0.0, 0.0 };  // [dim0, dim1] in FamilyDimension order below
  double pose[9] = {};
  double outgoing[3] = {};
  double image_alt_deg = 0.0;
  double image_az_deg = 0.0;
  double sigma1 = 0.0;
  double sigma2 = 0.0;
  double signed_jacobian = 0.0;
};

// One connected component of the fold locus, at grid resolution (crossings on edges of a common
// grid cell are connected — the declared approximation). `representative` indexes crossings and
// names the component's smallest-sigma2 point.
struct FoldComponentRecord {
  int first_crossing = 0;
  int crossing_count = 0;
  int representative = 0;
};

// One connected component of the collapsed samples (class 0, four-connected on the grid), with
// its representative (the smallest-sigma1 sample of the component). A pinned family's whole
// domain is one component whose image is a single sky point.
struct CollapseRecord {
  double params_deg[2] = { 0.0, 0.0 };
  double pose[9] = {};
  double outgoing[3] = {};
  double image_alt_deg = 0.0;
  double image_az_deg = 0.0;
  double sigma1 = 0.0;
  int sample_count = 0;
};

// One point evaluation of the same semantics — the probe primitive the survival filter (the
// ladder's "frozen width x local sensitivity" question) and the anchor tests need at points the
// caller names, independent of any grid. `params_deg` is in the spec's canonical free-dimension
// order (zenith, azimuth, roll, filtered). The refusals of RankRestrictedFamily apply with the
// same statuses (the grid bounds excepted — no grid here).
struct FamilyPointRecord {
  FamilyRankStatus status = FamilyRankStatus::kOk;
  bool valid = false;  // the chain gates' verdict at the pose (data, not error)
  double outgoing[3] = {};
  double sigma1 = 0.0;
  double sigma2 = 0.0;
  int rank_class = 0;
  double signed_jacobian = 0.0;  // free_count == 2 only
};

FamilyPointRecord EvaluateFamilyPoint(const FaceNormalTable& normals, const int* slots, int slot_count,
                                      const RestrictedFamilySpec& spec, const double sun_hat[3],
                                      double refractive_index, const double params_deg[2]);

struct RestrictedFamilyRank {
  FamilyRankStatus status = FamilyRankStatus::kOk;
  // The free dimensions in canonical (zenith, azimuth, roll) order; free_count is 1 or 2 for
  // kOk, and indexes params_deg.
  FamilyDimension free_dimensions[2] = { FamilyDimension::kAzimuth, FamilyDimension::kRoll };
  int free_count = 0;
  // The family's generic rank (the maximum class over the valid samples); -1 when no sample is
  // valid (the path has no image on this family — data, like an empty curve with a note).
  int generic_rank = -1;
  int grid = 0;
  int samples = 0;
  int valid_samples = 0;
  int invalid_samples = 0;  // the chain gates' rejection count (data, not error)
  int rank2_samples = 0;
  int rank1_samples = 0;
  int rank0_samples = 0;
  // The smallest singular values over the valid samples — the margin readout for the rank-class
  // threshold (min_sigma2 is 0 unless free_count == 2).
  double min_sigma1 = 0.0;
  double min_sigma2 = 0.0;
  // The scan's resolution declaration, per free dimension (the two-grid declared-error tier
  // reads these).
  double grid_spacing_deg[2] = { 0.0, 0.0 };
  // The fold locus (two-dimensional families only — a one-dimensional family's critical
  // structure is the EXISTING one-dimensional entries' vocabulary (dp_focus.hpp FamilyPinned,
  // dp_contour.hpp MakeRestrictedFamilyCurve), and this module never forks a second one).
  std::vector<FoldPointRecord> crossings;
  std::vector<FoldComponentRecord> fold_components;
  std::vector<CollapseRecord> collapse_components;
};

// The rank + critical structure of `slots` on the family `spec`, under the sun `sun_hat` (points
// AT the sun, dp_contour's convention; the incident propagation is -sun_hat) at
// `refractive_index`. `grid` is the per-free-dimension sample count: a periodic midpoint grid
// (2 pi (i + 0.5) / grid, the orbit producer's) for azimuth and roll, an inclusive uniform grid
// over [0, 180] for zenith. Deterministic; no allocation beyond the result's vectors.
RestrictedFamilyRank RankRestrictedFamily(const FaceNormalTable& normals, const int* slots, int slot_count,
                                          const RestrictedFamilySpec& spec, const double sun_hat[3],
                                          double refractive_index, int grid);

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_FAMILY_RANK_HPP_
