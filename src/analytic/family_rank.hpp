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
//   * The ordered-arc layer over the same scan (no second evaluation of the grid): every
//     crossing keeps its grid-edge reference; an edge with one invalid endpoint gains a midpoint
//     probe (a legal sign bracket inside recovers a crossing; an invalid midpoint records a
//     feasibility interruption); crossings chain through the cells that hold exactly two of
//     them (a 3+ cell subdivides once; an unresolved subcell breaks the chain with the reason
//     kept) — the ordered, per-link-justified `fold_chains`. A second pass classifies each
//     chain into image-form segments (arc / verified point-image / unknown) from the exact
//     second-order read of the SAME chain (Jet2 through the one templated optical chain — no
//     finite differences) and locates cusp candidates as sign-change zeros of dJ(v) on arc
//     segments only. A one-dimensional family records its scan as the ordered, validity-segmented
//     `track` instead — no fold machinery runs there.
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
// so3, jet, path_rank (the tolerance vocabulary's neighbour, read-only), core/math.

#include <algorithm>
#include <cmath>
#include <vector>

#include "analytic/path_evaluation.hpp"
#include "analytic/quantity_types.hpp"
#include "analytic/so3.hpp"
#include "core/math.hpp"

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
// The point-image claim's derived-bound cap (degrees of sky): the verification derives each
// candidate run's own constant-image bound from its reads (twice the worst image offset
// sigma1 x |J| / |grad J| over the run's points, plus the unit-sphere rounding scale) and
// classifies kPointImage when the measured sky diameter stays under that bound. This cap is
// the derivation's sanity ceiling, not the arbiter: an honest bound over located reads sits
// orders below it, and a bound past it means the run's evidence is too coarse for any point
// claim (the run reads kUnknown). Declared, not derived from any display quantity.
constexpr double kFoldPointImageFloorDeg = 1e-3;
// The point-image verification's on-locus refinement budget: at most this many gaps of a
// candidate run get their shared cell's subdivision (five evaluations plus reduced-precision
// bisections each) sampled into the diameter measure, evenly spaced along the run.
constexpr int kFoldRunRefineGaps = 16;
// The located fold's local rank-1 bar (the classification's qualification gate): a chain point
// reads as a fold only with sigma1 above kFamilyRankSigmaTolerance and sigma2/sigma1 under
// this. Real crossings bisect to ratios in the 1e-16..1e-6 band; a validity cliff's sign jump
// (which the edge scan bisects like a crossing) lands orders above — the bar separates the
// two with orders of margin on both sides.
constexpr double kFoldLocalRankRatio = 1e-4;
// The point-image LOCATOR's alignment slack: a chain point joins a candidate run when
// alignment >= 1 - this. Deliberately generous — the locator only proposes candidates; the
// diameter verification (the floor above, midpoint-refined) is the arbiter, so a generous
// locator costs a wasted verification on a slow arc, never a false point claim.
constexpr double kFoldPointImageLocatorSlack = 0.05;

// ---- the pose chart and the sky read --------------------------------------------------------------
//
// The single authority for the two formulas the routing layer reads on both sides of the kernel
// boundary (survey_assembly's local mirrors were consolidated here — one expression per formula,
// the kernel's own; do not re-derive them in a caller).

// R(a, t, r) = Rz(a - pi) Ry(-t) Rz(r), the production Euler convention (the module doc): the
// declared densities' zenith/roll extraction inverts exactly this form, and the corpus fold
// witnesses decompose onto it to rounding. `S` is double or Jet<2>.
template <class S>
void EulerPose(const S& azimuth, const S& zenith, const S& roll, S pose[9]) {
  S rz_az[9];
  {
    const S w[3] = { S(0.0), S(0.0), azimuth - math::kPiD };
    so3::Exp(w, rz_az);
  }
  S ry_t[9];
  {
    const S w[3] = { S(0.0), -zenith, S(0.0) };
    so3::Exp(w, ry_t);
  }
  S rz_r[9];
  {
    const S w[3] = { S(0.0), S(0.0), roll };
    so3::Exp(w, rz_r);
  }
  S tmp[9];
  so3::MatMul(rz_az, ry_t, tmp);
  so3::MatMul(tmp, rz_r, pose);
}

// The degree-taking entry of the same chart (survey-side poses arrive in degrees, PoseDensity's
// unit): a thin unit conversion onto EulerPose, the kernel expression untouched.
inline void EulerPoseDeg(double azimuth_deg, double zenith_deg, double roll_deg, double pose[9]) {
  EulerPose(azimuth_deg * math::kDegreeToRadD, zenith_deg * math::kDegreeToRadD, roll_deg * math::kDegreeToRadD, pose);
}

// The sky position of the image direction -outgoing: altitude above the world equator,
// azimuth atan2(y, x) — the corpus anchors' target fields are exactly these two.
inline void ImageAltAzDeg(const double outgoing[3], double* alt_deg, double* az_deg) {
  const double z = std::max(-1.0, std::min(1.0, -outgoing[2]));
  *alt_deg = std::asin(z) / math::kDegreeToRadD;
  *az_deg = std::atan2(-outgoing[1], -outgoing[0]) / math::kDegreeToRadD;
}

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

// One point of a one-dimensional family's ordered track (free_count == 1): the scan's own
// sample in parameter order — the SAME evaluation the rank scan already ran, recorded instead of
// dropped. No second optical judgement: the point is EvaluateSample's outgoing at its parameter.
// An invalid sample (the chain gates' verdict) keeps its place with valid == false and no
// direction — the track's segmentation counts on every grid position being present.
struct FamilyTrackPoint {
  double param_deg = 0.0;
  double pose[9] = {};
  double outgoing[3] = {};
  double image_alt_deg = 0.0;
  double image_az_deg = 0.0;
  double sigma1 = 0.0;
  bool valid = false;
};

// One maximal run of consecutive VALID track points (indices into FamilyTrackRecord::points,
// inclusive). The runs are the track's validity segmentation: between two runs lies an invalid
// gap (the chain gates' verdicts), and the track draws run by run — no line across a gap.
struct FamilyTrackSegment {
  int first = 0;
  int last = 0;
};

// The one-dimensional family's ordered track (free_count == 1 only; empty otherwise). The
// parameters run in the axis's own order (a periodic midpoint grid's order is the orbit
// parameter's, so the wrap sits between the last point and the first — `closed` records that
// the first and last points are grid-adjacent through the seam and both valid, the drawable
// loop's own evidence; no extra point is fabricated to close it).
struct FamilyTrackRecord {
  std::vector<FamilyTrackPoint> points;
  std::vector<FamilyTrackSegment> segments;
  bool closed = false;
};

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


// One located point of the fold locus (a two-dimensional family's rank-drop curve): the free
// parameters, the refined pose, the sky position of -outgoing (altitude above the world
// equator, azimuth atan2(y, x) of the image direction — sky_direction.hpp's world convention),
// the singular values there, and the orientation Jacobian's residual value after the bisection
// (0 to bracket resolution; |it| equals sigma1 * sigma2 by Lagrange, a consistency read).
//
// The edge reference is the ordered-arc work's addition (task arc-recovery-engine): the grid
// edge the crossing was located on, `(dim, i0, i1)` naming the edge's first vertex and `t` its
// 0..1 position along one spacing — the identity the chain walker needs (two crossings are
// adjacent exactly when a common grid cell holds exactly them). A crossing recovered by the
// cliff-edge midpoint probe (an edge with one invalid endpoint) says so in `from_cliff_probe`;
// its edge reference is the same grid edge. The classification reads (filled when the chain
// pass runs; negative `alignment` = not classified) hang on the same record: `alignment` is
// |tau_hat . v_hat| — the cosine between the chain's local parameter tangent and the kernel
// direction of the restricted Jacobian, 1 when the critical tangent sits inside the kernel
// (the image-velocity-zero reading a point-image run is verified from) — and `djv` is dJ(v),
// the fold transversality read (gradient of the signed Jacobian along the kernel direction;
// its sign is meaningful only against a continuously oriented kernel along the chain, which is
// how the chain pass computes it), with `djv_resolution` the point's own numeric bound.
struct FoldPointRecord {
  double params_deg[2] = { 0.0, 0.0 };  // [dim0, dim1] in FamilyDimension order below
  double pose[9] = {};
  double outgoing[3] = {};
  double image_alt_deg = 0.0;
  double image_az_deg = 0.0;
  double sigma1 = 0.0;
  double sigma2 = 0.0;
  double signed_jacobian = 0.0;
  int edge_dim = -1;  // 0 or 1: which free dimension varies along the locating edge
  int edge_i0 = -1;
  int edge_i1 = -1;
  double edge_t = 0.0;
  bool from_cliff_probe = false;
  double alignment = -1.0;
  double djv = 0.0;
  double djv_resolution = 0.0;
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

// ---- the ordered fold chain (the drawable critical arc) ---------------------------------------------
//
// The chain is the ordered-arc work's answer to what the union-find component only coarsens: an
// adjacency-ordered crossing sequence, each link carrying the grid cell that justifies it. The
// declared adjacency is UNCHANGED in kind (two crossings are adjacent exactly when one grid
// cell's boundary holds exactly the two of them); what is new is that the pairing is per cell —
// a cell holding three or four crossings pairs nothing (one level of bounded subdivision runs
// first; a subcell still holding 3+ crossings breaks the chain there) — so a drawn adjacency
// always has a named cell behind it, never a sky-distance guess.

// Why a chain ends (or breaks) at an end point. Data, never an error: a broken chain publishes
// its surviving fragments with the reason attached, per the ordered-arc contract (no silent
// bridging, no fabricated closure).
enum class FoldChainEnd {
  kOpen,                     // a degree-1 end: no cell continuation beyond it (the locus just stops there at
                             // this resolution)
  kClosedLoop,               // the chain closes onto its own first point through a justifying cell (adjacency
                             // evidence, never endpoint proximity)
  kFeasibilityInterruption,  // the cliff-edge midpoint probe found the invalid region: the valid
                             // region ends at this point (the probe's own mechanical verdict)
  kAmbiguousCell,            // a cell with 3+ crossings survived bounded subdivision unresolved;
                             // the fragments on both sides are kept, nothing pairs arbitrarily
};
const char* FoldChainEndName(FoldChainEnd end);

// One link of the chain: which cell joins the two crossings it sits between, and whether that
// cell paired them directly or through the one-level subdivision. `cell_i0` / `cell_i1` are the
// cell's lower corner (grid indices, canonical dimension order; a periodic wrap wraps here too).
struct FoldChainLink {
  int cell_i0 = 0;
  int cell_i1 = 0;
  bool through_subdivision = false;
};

// One maximal run of the chain's image form. `first` / `last` are INCLUSIVE indices into the
// chain's order[] (a partition of the chain: every order position belongs to exactly one
// segment; segments with no classification evidence at all come out as kUnknown — the honest
// third form, never silently folded into an arc or a point).
enum class FoldSegmentForm {
  kArc,         // the image moves along the segment (the resolvably nonzero image-velocity runs)
  kPointImage,  // verified constant image: alignment near 1 along the run AND the run's whole
                // sky diameter (midpoint-refined) inside the declared floor
  kUnknown,     // the numeric evidence cannot decide (alignment in the unresolved band, or a
                // diameter the evidence cannot separate from the floor)
};
const char* FoldSegmentFormName(FoldSegmentForm form);

struct FoldSegmentRecord {
  int first = 0;  // inclusive index into the owning chain's order[]
  int last = 0;   // inclusive
  FoldSegmentForm form = FoldSegmentForm::kArc;
  // kPointImage only: the run's constant image (the run's mean sky position) and its measured
  // sky diameter after midpoint refinement (the verification's own reading, not an assumption).
  double image_alt_deg = 0.0;
  double image_az_deg = 0.0;
  double image_diameter_deg = 0.0;
};

// One cusp candidate: a located transversality failure on the critical branch — the dJ(v)
// sign change between two chain points is first bisected on the parameter chord, then the
// chord zero is PROJECTED onto the locus (one Newton step on the constraint J = 0 along the
// same Jet2 read's gradient), and the event publishes at the projected point only if it
// passes the local rank-1 qualification and its |dJ(v)| sits under its own resolution (an
// ordinary fold neighbourhood projects to a resolvable dJ(v) and publishes nothing). The
// evidence level is the plan's own "candidate" bar: the failure of the normal-fold criterion
// at a locatable on-branch position does not by itself certify a generic cusp, and the record
// says so through its name. `order_index` is the chain order position the candidate sits at
// (the bisection ran between that point and its successor).
struct FoldCuspCandidate {
  int order_index = 0;
  double params_deg[2] = { 0.0, 0.0 };
  double pose[9] = {};
  double image_alt_deg = 0.0;
  double image_az_deg = 0.0;
  double djv_residual = 0.0;  // |dJ(v)| at the projected point (under the read's resolution —
                              // a bound, never a zero claim)
};

// One ordered fold chain: crossing indices in chain order (indices into RestrictedFamilyRank's
// crossings), each interior step justified by a link, both ends carrying their end reasons. A
// closed loop's `order` does not repeat the first point; `closed` says the last links to the
// first through `links.back()` (links.size() == order.size() then). Segments partition the
// order positions into image-form runs; cusp candidates sit only on arc segments (a point-image
// run never doubles as a fold or cusp set — the classification's own order).
struct FoldChainRecord {
  bool closed = false;
  std::vector<int> order;
  std::vector<FoldChainLink> links;
  FoldChainEnd end_a = FoldChainEnd::kOpen;  // at order.front()
  FoldChainEnd end_b = FoldChainEnd::kOpen;  // at order.back()
  std::vector<FoldSegmentRecord> segments;
  std::vector<FoldCuspCandidate> cusps;
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
  // The ordered chains over `crossings` (two-dimensional families): the drawable critical arcs.
  // The union-find `fold_components` above stays exactly what it was (the coarse sampling
  // connectivity, byte-compat readers); the chains are the finer per-cell-paired vocabulary the
  // ordered-arc contract consumes. A component may hold several chains (an unresolved ambiguity
  // breaks one locus into kept fragments) — the chains, not the components, are what draws.
  std::vector<FoldChainRecord> fold_chains;
  // The cliff-edge midpoint probe's honesty counters: how many one-valid-endpoint edges were
  // probed, and how many of those probes hit the invalid region (no crossing recovered, the
  // interval stays unresolved — declared data, like invalid_samples).
  int cliff_probed_edges = 0;
  int cliff_unresolved_edges = 0;
  // The one-dimensional family's ordered track (free_count == 1 only; the same scan's samples
  // recorded in parameter order, validity-segmented).
  FamilyTrackRecord track;
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
