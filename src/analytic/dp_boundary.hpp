#ifndef LUMICE_ANALYTIC_DP_BOUNDARY_HPP_
#define LUMICE_ANALYTIC_DP_BOUNDARY_HPP_

// dU_P: the boundary loop of the valid domain, its corners and the restricted critical points of
// D_P (LI dp_field.boundary, ported step for step for scrum 660.3). U_P = {u : every gate > 0}
// (dp_field), so dU_P is made of arcs of the zero sets of single gates meeting at corners. Only
// gates are walked, cut at, or recorded at corners: an internal reflection's TIR discriminant is a
// diagnostic of the domain margin vector and never part of dU_P (a partial reflection keeps the
// point in U_P, weighted by Fresnel R). "Margin" below means a gate.
//
// The boundary is found by WALKING it: from one boundary point, march along the zero set of the
// active margin with U_P on the left (tangent grad m x u), stop at the first point where another
// margin turns negative (bisection, then a two-margin tangent-plane Newton), pick the one margin
// through the corner whose own zero set continues the boundary, and go on until the walk is back
// at its first corner. A closed walk is the CERTIFICATE that the loop is complete — closure itself
// is this module's completeness claim (AC2: WalkStatus kOk means the loop closed, everything else
// is a named truncation with no loop data). Closure is credited on distance alone (the 52.4
// lesson, LI walk_zero_set verbatim): the walk is back within one step of its seed after at least
// two steps of accumulated arc. A heading test would reject the exact return (chord and dot both
// zero), which a great-circle loop whose perimeter is an integer multiple of the step (1-2-1:
// 2 pi / step = 1440) makes every lap; the arc bound rules out the departure transient instead.
// When D_P is constant along the whole loop (a mirror slab's crease circle as dU_P itself), the
// loop has no isolated extremum: critical_points is empty and the constant value is recorded as
// the plateau.
//
// Two curve kinds, by margin: an incidence cosine whose unfolded normal m = R_{k-1}^T n_k is
// orthogonal to the entry normal is linear in u, its zero set the great circle m . u = 0, walked
// in closed form (a rotation about the oriented m); this is CHECKED per path at construction (the
// normal condition and the margin on sampled circle points), a margin failing either check is
// marched. Every other margin is marched: tangent predictor, Newton corrector along the tangent
// gradient back onto the zero set.
//
// Margin identities are removed before walking, never assumed absent: three consecutive internal
// reflections off side faces whose azimuths step by the same +-60 degrees make the first and third
// incidence cosines the same function (the third step's margins are dropped and reported in the
// record); a margin that vanishes along a whole piece of the walk is recorded as coincident with
// that piece and does not stop it there.
//
// Corners carry every margin that vanishes there, which two of them bound U_P (the incoming and
// outgoing pieces) and which of the others are tangent to a bounding piece versus transversal.
// D_P restricted to the loop is sampled along each piece (the exit-Snell closure convention of
// RoutedDeviation — a non-finite plain value ON the closure reads d_p_exit_limit, off it the walk
// fails closed); local extrema are refined by golden-section search on the piece (derivative-free:
// grad D_P diverges on the exit-TIR curve, where D_P itself is finite), corner values are kept as
// they are.
//
// JAX is the authority, this is the derived implementation (scrum schema3-geometry-port):
// constants, thresholds, closure criterion and judgement order are LI's, copied verbatim and
// marked with their LI names — no re-calibration here. LI's five RuntimeError / three ValueError
// raise sites of the walk become the WalkStatus enumeration with LI's message texts (stable
// prefixes), so an exception semantics never crosses a future C ABI.
//
// Internal header of the analytic kernel: nothing here is part of the C ABI. Dependency
// direction: dp_weight_kink -> dp_boundary (this header) -> dp_field; the partition's
// BoundaryLoopData (dp_partition.hpp) is the consumer face this module produces.

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "analytic/dp_field.hpp"
#include "analytic/dp_partition.hpp"

namespace lumice::analytic {

// ---- constants (LI boundary.py, verbatim) --------------------------------------------------------------
// (kViolationAtol / kExtremumAtol live in dp_field.hpp / dp_partition.hpp — not duplicated here.)

// March step along a boundary piece (rad; LI WALK_STEP_RAD = radians(0.25)).
constexpr double kWalkStepRad = 0.25 * 3.14159265358979323846 / 180.0;
// Distance from a corner at which the candidate outgoing pieces are probed (rad; CORNER_PROBE_RAD).
constexpr double kCornerProbeRad = 1e-5;
// A margin at or below this in size vanishes at a point (ZERO_MARGIN_ATOL).
constexpr double kZeroMarginAtol = 1e-9;
// A margin within this of zero along a whole piece is coincident with it (COINCIDENT_ATOL).
constexpr double kCoincidentAtol = 1e-12;
// A vanishing margin whose tangent gradient is below this touches zero without changing sign:
// never an edge (TOUCHING_GRADIENT_ATOL).
constexpr double kTouchingGradientAtol = 1e-8;
// |m . n_a| below this makes an incidence cosine linear in u — a great circle (GREAT_CIRCLE_ATOL).
constexpr double kGreatCircleAtol = 1e-12;
// A side-face triple's two incidence normals within this are one margin (IDENTITY_NORMAL_ATOL).
constexpr double kIdentityNormalAtol = 1e-9;
// Two gradients whose unit tangent parts have |cross| below this are parallel — tangent curves
// (TANGENT_SINE_ATOL).
constexpr double kTangentSineAtol = 1e-6;
// The walk is closed when it meets its first corner again within this (rad; CORNER_CLOSE_RAD).
constexpr double kCornerCloseRad = 1e-6;
// Step budget of one zero-set walk (MAX_WALK_STEPS); the truncation a small override exposes is
// AC2's declared truncation, not a silent cap.
constexpr int kMaxWalkSteps = 40000;

// ---- margins by index ----------------------------------------------------------------------------------

// The domain margin vector's position names, LI domain_margin_names' vocabulary:
// entry_incidence_cosine, entry_snell_discriminant, internal_k_incidence_cosine,
// internal_k_tir_discriminant, exit_incidence_cosine, exit_snell_discriminant. The walk's and the
// tests' identifier — margin indices are the currency, names are for messages and assertions.
std::string MarginName(int slot_count, int margin);
constexpr int kEntryIncidence = 0;
constexpr int kEntrySnell = 1;
// internal step k: incidence at 2k, TIR discriminant at 2k + 1; exit pair at 2 * (slot_count - 1).
inline int ExitSnellOf(int slot_count) {
  return 2 * (slot_count - 1) + 1;
}

// The path id of a face sequence ("3-5-6-7", LI path_id_of) for the walk's message texts.
std::string PathIdOf(const FaceNormalTable& table, const int* slots, int slot_count);

// ---- margin identities and great circles ----------------------------------------------------------------

// m of every incidence cosine (LI incidence_normals): n_a (slot 0), R_{k-1}^T n_k (internal slot
// k), M^T n_b (the last slot) — one per slot of the sequence, from the fold product's single
// authority (FoldMatrixOf). The identity check and the kink closed form both read these.
void IncidenceNormals(const FaceNormalTable& table, const int* slots, int slot_count, double out[kMaxFaceCount][3]);

// Margins that equal an earlier one as functions (LI identical_margins): the +-60-degree
// side-face triples. Returns {dropped, kept} margin-index pairs — the incidence cosine and the
// TIR discriminant of the third reflection of each triple map to the first's (chains map to the
// head). The triple is found by face number and confirmed on the crystal's own incidence normals
// (within kIdentityNormalAtol); a side face off its regular azimuth breaks the identity and keeps
// both margins.
std::vector<std::pair<int, int>> IdenticalMargins(const FaceNormalTable& table, const int* slots, int slot_count);

// The incidence cosines whose zero set is a great circle, as {margin, oriented unit normal} pairs
// (LI great_circle_margins): accepted when |m . n_a| <= kGreatCircleAtol (the entry margin
// unconditionally — its m IS n_a) AND the margin is <= kCoincidentAtol in size at `samples`
// points of m . u = 0 — the run-time check of the derivation. The normal is oriented into U_P
// (the margin's sign at u = m).
std::vector<std::pair<int, std::array<double, 3>>> GreatCircleMargins(const DeviationField& field, int samples);

// ---- the walker -----------------------------------------------------------------------------------------

// The curve steppers and margin bookkeeping of one (crystal, faces, index) — LI boundary.Walker.
// Read-only after construction: the steppers take any margin index (WalkZeroSet walks the zero
// set of one — a gate of dU_P or a TIR discriminant), and a walk's orientation is an argument of
// the walk, never walker state. Holds its field by reference; the field must outlive the walker.
class BoundaryWalker {
 public:
  explicit BoundaryWalker(const DeviationField& field);

  const DeviationField& field() const { return field_; }
  int slot_count() const { return slot_count_; }
  int margin_count() const { return margin_count_; }

  // The gates of U_P with identities removed (LI Walker.active), as margin indices in domain
  // order. entry_snell_discriminant stays a gate but never vanishes for n > 1, so it never
  // becomes a piece.
  const std::vector<int>& active() const { return active_; }
  // The identity map of IdenticalMargins: 0 when the margin is kept, else the kept margin (0 is
  // unambiguous — kept indices are positive, the entry margin is never dropped).
  int KeptOf(int margin) const { return kept_of_[margin]; }
  // The {dropped, kept} pairs themselves (LI identical_margins' dict), for the walk record.
  const std::vector<std::pair<int, int>>& identical() const { return identical_; }

  // The closed-form circles of GreatCircleMargins (LI Walker.circles).
  bool HasCircle(int margin) const { return has_circle_[margin]; }
  const double* CircleNormal(int margin) const { return circle_normal_[margin]; }

  // -- evaluation
  int Margins(const double u[3], double out[2 * kMaxFaceCount]) const { return field_.DomainMarginsAt(u, out); }
  void MarginsJacobian(const double u[3], MarginJet* out) const { field_.MarginsWithGradient(u, out); }
  // The ambient gradient of `margin` at u projected on u's tangent plane (LI _tangent): the
  // corrector's direction and the ranking distance's denominator.
  void TangentGradient(const double u[3], int margin, double out[3]) const;

  // D_P at a point of the closure of U_P (LI Walker.d): the exit-Snell convention — a finite
  // value passes through, a non-finite value of a non-slab path ON the closure is d_p_exit_limit,
  // anything else is kNotFinite and the caller fails closed (LI raises RuntimeError there).
  // RoutedDeviation is the single router; the closure check reads the full validity vector —
  // equivalent to LI's active-minus-identical verdict because a dropped margin equals its kept
  // twin as a function (to 1e-14), so the two pass and fail together.
  RoutedDeviationStatus D(const double u[3], double* out) const;
  // D_P at a point of the exit TIR curve, the exit root dropped (LI Walker.d_on_exit_tir =
  // d_p_grazing): the golden section's value on an exit-Snell piece.
  double DOnExitTir(const double u[3]) const;

  // -- curve steppers
  // Project u onto the zero set of `margin`, ending on its non-negative side (in place).
  void Correct(double u[3], int margin) const;
  // Unit tangent of the zero set of `margin` at u with U_P (margin > 0) on the left.
  void Direction(const double u[3], int margin, double out[3]) const;
  // `step` along the zero set of `margin` from u, the way `tangent` points.
  void Advance(const double u[3], int margin, const double tangent[3], double step, double out[3]) const;
  // Two-margin tangent-plane Newton on m_a = m_b = 0 (in place; left as is when the curves are
  // tangent there — the det / step / best-residual gates of LI refine_corner).
  void RefineCorner(double u[3], int a, int b) const;
  // The active margins violated at u (value < -kViolationAtol, NaN included — fail closed), minus
  // `excluded`; returns how many were written.
  int Violated(const double u[3], const int* excluded, int excluded_count, int out[kMaxFaceCount + 2]) const;
  // The margin of `names` farthest outside at u, in signed distance m / |grad m| (LI
  // most_violated). Precondition: count >= 1 — an empty list has no answer (the callers reach it
  // only with a non-empty violated set; FindStartPoint fail-closes on the shell where that fails).
  int MostViolated(const double u[3], const int* names, int count) const;
  // The active margins other than `margin` whose value is within kCoincidentAtol of zero at u.
  int CoincidentWith(const double u[3], int margin, int out[kMaxFaceCount + 2]) const;

 private:
  // The tangent projection of an already-evaluated ambient gradient (the public TangentGradient
  // without the second sweep).
  void TangentOf(const double u[3], const MarginJet& jet, int margin, double out[3]) const;

  const DeviationField& field_;
  int slot_count_ = 0;
  int margin_count_ = 0;
  std::vector<std::pair<int, int>> identical_;
  int kept_of_[2 * kMaxFaceCount] = {};
  std::vector<int> active_;
  bool has_circle_[2 * kMaxFaceCount] = {};
  double circle_normal_[2 * kMaxFaceCount][3] = {};
};

// ---- the walks ------------------------------------------------------------------------------------------

// Every way a walk refuses, one enum value per LI raise site (a regime is never silently merged):
// kOk is the closed loop — the completeness certificate itself. The message carries LI's raise
// text (stable prefixes; the numbers are diagnostics).
enum class WalkStatus {
  kOk,
  kStepsExhausted,   // RuntimeError: MAX_WALK_STEPS without a corner or a closure
  kStartNoPoint,     // ValueError: U_P has no point on the lattice
  kStartCoversAll,   // ValueError: U_P covers the whole lattice — no boundary
  kStartNoEdge,      // ValueError: no lattice point next to the boundary — also the
                     // start-bisection shell fail-close (the message text distinguishes)
  kCornerNotSimple,  // RuntimeError: a corner's outgoing margin is not unique
  kNotClosed,        // RuntimeError: 1000 pieces without closing
  kNotFinite,        // RuntimeError: D_P off the closure of U_P (fail closed)
  kBadOrientation,   // ValueError: orientation not in {1, -1}
};

const char* WalkStatusName(WalkStatus status);

// The knobs of a walk with LI's defaults (LI walk_boundary's lattice_n / step; max_walk_steps is
// the same constant, overridable so the truncation is testable without touching product
// constants).
struct BoundaryWalkOptions {
  int lattice_n = 20000;
  double step = kWalkStepRad;
  int max_walk_steps = kMaxWalkSteps;
};

// One march along the zero set of `margin` from `start` (a point on it) until a gate of U_P stops
// it or it is back at `stop_at` (LI walk_zero_set; `stop_at` null walks to the first corner only).
// `orientation` +1 walks with the margin's positive side on the left (the way dU_P is walked),
// -1 the other way; anything else is kBadOrientation. The points run `start` first and the corner
// last when one is met; the closure credit is the distance-only double criterion (module
// docstring). `coincident` holds the margins vanishing along the walked piece.
struct ZeroSetWalk {
  WalkStatus status = WalkStatus::kOk;
  std::string message;
  std::vector<double> points;  // 3N, start first (corner last when met)
  bool met_corner = false;
  double corner[3] = {};
  std::vector<int> coincident;
};

ZeroSetWalk WalkZeroSet(const BoundaryWalker& walker, const double start[3], int margin, double orientation,
                        const double* stop_at, const BoundaryWalkOptions& options);

// One smooth arc of dU_P: the zero set of `margin` from points[0] to points[-1], U_P on the left
// (LI BoundaryPiece). `great_circle` says the arc was walked in closed form (`circle_normal` then
// set); `coincident` lists the margins vanishing all along it; `values` is D_P at the points
// (radians, the closure convention). Internal-contract: declared here so the unit tests can
// assert the walk's own bookkeeping (pieces, corner adjacency) — not part of any API surface.
struct BoundaryPiece {
  int margin = -1;
  bool great_circle = false;
  double circle_normal[3] = {};
  std::vector<double> points;  // 3N
  std::vector<double> values;  // N
  std::vector<int> coincident;
};

// A non-smooth point of dU_P (LI Corner). `margins`: every gate vanishing there
// (|m| <= kZeroMarginAtol, in domain order); `incoming` / `outgoing`: the two that bound U_P there
// (walk order); `tangent` / `transversal`: the other vanishing margins whose curve is tangent to
// a bounding one (they touch the corner without cutting U_P) versus crossing both bounding
// curves; `coincident`: vanishing margins whose zero set contains a bounding piece; `residual` is
// the largest |m| over `margins`.
struct WalkerCorner {
  double position[3] = {};
  double value = 0.0;
  double residual = 0.0;
  std::vector<int> margins;
  int incoming = -1;
  int outgoing = -1;
  std::vector<int> tangent;
  std::vector<int> transversal;
  std::vector<int> coincident;
};

// The full walk record (LI BoundaryLoop, the rich half): pieces and corners in walk order
// (pieces[i] runs from corners[i - 1] to corners[i]), the identity and great-circle bookkeeping,
// and the consumer face in `data` (dp_partition's BoundaryLoopData — critical points in WALK
// ORDER, corners, the first sample point, the plateau). Internal-contract like BoundaryPiece.
struct BoundaryWalkRecord {
  std::vector<BoundaryPiece> pieces;
  std::vector<WalkerCorner> corners;
  std::vector<std::pair<int, int>> identical;  // {dropped, kept}, LI identical_margins
  std::vector<int> great_circles;              // margins with a closed-form circle
  BoundaryLoopData data;
};

// WalkBoundary's answer: either the loop's consumer face or the refusal. Mechanical invariant
// (load-bearing, asserted by the tests): status != kOk implies `loop` is EMPTY — no half-walk is
// ever delivered. `record`, when non-null, additionally receives the rich bookkeeping (on kOk
// only) — the tests' seam.
struct WalkResult {
  WalkStatus status = WalkStatus::kOk;
  std::string message;
  BoundaryLoopData loop;
};

// Walk dU_P once around (module docstring) and collect pieces, corners and the restricted extrema
// of D_P (LI walk_boundary): find a start point by lattice bisection, walk to the first corner (a
// smooth loop closes on itself as one piece), then assemble piece by piece — probe each vanishing
// margin's continuation, walk to the next corner, stop back at the first within
// kCornerCloseRad — and finally glue the last point to the first corner exactly and take the
// corner records. A loop of constant D_P comes back with empty critical_points and its value as
// the plateau.
WalkResult WalkBoundary(const DeviationField& field, const BoundaryWalkOptions& options,
                        BoundaryWalkRecord* record = nullptr);

// ---- the plateau extrema of a cyclic sequence -------------------------------------------------------------

// Local extrema of a cyclic value sequence as {index, kind, run length} — runs of equal values
// (within `atol`) counted once, a run crossing the seam merged as ONE run whose length counts
// both tails (LI _plateau_extrema, the 52.8 wrap-merge lesson carried in value order — a
// different semantics than dp_partition's boolean CircularRuns, deliberately not unified). A
// sequence that is one single run has no isolated extremum: `is_plateau` is set and its constant
// returned. Table-driven test subject; the loop assembly's internal step.
struct PlateauExtremum {
  int index = 0;
  CriticalKind kind = CriticalKind::kMinimum;
  int run_length = 0;
};

std::vector<PlateauExtremum> PlateauExtremaOf(const double* values, int count, double atol, bool* is_plateau,
                                              double* plateau_value);

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_DP_BOUNDARY_HPP_
