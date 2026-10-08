#ifndef LUMICE_ANALYTIC_DP_PARTITION_HPP_
#define LUMICE_ANALYTIC_DP_PARTITION_HPP_

// The delta-axis partition of D_P on U_P and its component counts — the completeness
// certificate's data (LI dp_field.certificate, tasks 52.6/52.7/52.8 of LI scrum
// wave3-pull-forward). The level set {D_P = delta} in U_P is a union of closed loops and of open
// arcs whose ends lie on dU_P; its topology can change only at a critical value (an interior one,
// one of D_P restricted to a smooth piece of dU_P, or a corner's), and between consecutive
// critical values the counts follow from the critical data alone — every arc end is a boundary
// crossing, every closed loop bounds a disk around the one interior extremum. Everything that
// reasoning does not cover is an explicit escape (EscapeRegime below, LI TopologyEscape): none of
// it is resolved silently, and the report side consumes escapes as data (fail-closed).
//
// JAX is the authority, this is the derived implementation (scrum schema3-geometry-port):
// constants, thresholds, message texts and judgement order are LI's, copied verbatim and marked
// with their LI names — no re-calibration here. The chart audit deliberately re-implements the
// chart of LI's scripts/verify_dp_field_intervals.py (shared gate authority, unshared code): that
// independence between the two chains is what makes the audit worth anything, and neither side's
// chart construction may be edited without the other.
//
// Internal header of the analytic kernel: nothing here is part of the C ABI (the report surface is
// a later subtask's).

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "analytic/dp_field.hpp"

namespace lumice::analytic {

// ---- constants (LI certificate.py / field.py / boundary.py, verbatim) --------------------------------

// Two critical values within this (rad) are one (LI boundary.EXTREMUM_ATOL: above the D_P error
// on the loop ~1e-8, below any extremum the sampling resolves). The single authority of the
// partition's merge behaviour; consumers read it from here.
constexpr double kExtremumAtol = 1e-7;
// The ring that decides on which side of a boundary extremum D_P is lower, and its directions
// (LI certificate.SIDE_RING_RAD / SIDE_RING_DIRECTIONS).
constexpr double kSideRingRad = 1e-5;
constexpr int kSideRingDirections = 64;
// The crease-cluster evidence thresholds of the degenerate fold set (LI field.CREASE_CONTACT_MARGIN /
// CREASE_TOUCHING_ARC_RAD): a crease sample whose binding gate is within the contact margin of
// zero counts as touching dU_P, and a run of such samples hugging dU_P over more than the arc
// figure is a tangency, not a transversal crossing. Both are pinned on the one positive fixture
// (4-8-7-5, 7200 samples) to its NON-triggering side only; the triggering side is covered by
// synthetic tests. Re-calibrating them is forbidden (they are LI's settled values, not knobs).
constexpr double kCreaseContactMargin = 1e-2;
constexpr double kCreaseTouchingArcRad = 0.1;
// The crease sampling density of the fold set (LI degenerate_fold_set's default).
constexpr int kFoldCircleSamples = 7200;
// The lattice the component counts are taken on (LI domain_topology's default).
constexpr int kTopologyLatticeN = 20000;
// The chart resolutions of the component-count audit, coarse to fine (LI certificate.AUDIT_LADDER,
// settled on 3-5-6-7: both levels adjudicate one component). An empty ladder turns the audit off —
// the rollback switch: a plural count then keeps its lattice value and escapes the old way.
constexpr int kDefaultAuditLadder[] = { 801, 1601 };
constexpr int kDefaultAuditLadderCount = 2;

// ---- the escape hatch (AC1: an open enumeration, one name per regime) --------------------------------

// Every regime the simple disk / single-extremum reasoning refuses (LI TopologyEscape's raise
// sites, one enum value each — a regime is never silently merged into another). Appending one is
// a source change (this is a kernel-internal header, not ABI); the name table below and the
// reachability tests grow with it.
enum class EscapeRegime {
  kNotDiskUnaudited,         // U_P not a disk and no audit ran (a hand-built topology or an empty ladder)
  kNotDiskUnconverged,       // the chart grids disagree or breached their premise: nothing established
  kNotDiskConfirmed,         // lattice and grids agree on a plural count
  kNotDiskCorrected,         // the grids agree on a count the lattice got wrong, and it is still not a disk
  kSlabCreaseContradiction,  // a fraction claiming interior arcs the crease sampling does not hold
  kSlabCreaseNotCarried,     // the blade value is not carried as a strict local maximum of the walk
  kSlabCreaseTouching,       // the crease touches or runs along dU_P (a non-transversal contact)
  kSlabCreaseClosedRidge,    // an interior crease arc never reaches dU_P
  kMultipleInteriorCriticalPoints,
  kLoopExtremaNotAlternating,
  kOddBoundaryCrossings,
  kInteriorCriticalPointNotSimple,  // a saddle, or a degenerate point of a path with no slab form
  kSublevelNotReachingBoundary,     // the interior extremum's sublevel component is not shown to reach dU_P first
};

// The regime's stable slug (`not_disk_unconverged`, ...): report-side identifiers and test
// assertions match on this string, so it is a name, not a description.
const char* EscapeRegimeName(EscapeRegime regime);

// ---- the audit of the lattice component counts (task 52.7) ------------------------------------------

enum class AuditVerdict { kConfirmed, kCorrected, kUnconverged };
// LI's verdict strings, stable for the report side.
const char* AuditVerdictName(AuditVerdict verdict);

// The chart-grid audit's evidence: one count per grid of the ladder, -1 where that grid's mask
// breached the chart premise (a valid node on the pushed rim). The lattice_* fields keep the
// audited k-NN counts next to the verdict, so a correction stays visible instead of silent. An
// audit trail only: what escapes is decided by the adjudicated counts through DomainTopology.
struct ChartAudit {
  std::vector<int> grids;
  std::vector<int> domain_counts;
  std::vector<int> complement_counts;
  int lattice_domain_count = 0;
  int lattice_complement_count = 0;
  AuditVerdict verdict = AuditVerdict::kConfirmed;
};

// Component counts of U_P and of its complement, adjudicated: the counts come from the
// lattice_n-point Fibonacci lattice's k-NN graph, and when a count is plural the chart-grid audit
// runs and — if it converges — its counts replace the lattice's here (`grid_audit` keeps both).
// Without an audit the fields are the lattice counts, unchanged.
struct DomainTopology {
  int lattice_n = 0;
  int domain_components = 0;
  int complement_components = 0;
  bool has_grid_audit = false;
  ChartAudit grid_audit;

  bool IsDisk() const { return domain_components == 1 && complement_components == 1; }
};

// The verdict roll-up of the two counts' audit statuses, worst first: unconverged (the grids
// disagree, or a mask breached the premise) over corrected (the grids agree on a count the
// lattice got wrong) over confirmed (the grids agree with the lattice). Pure — the test's
// table-driven subject.
AuditVerdict AuditVerdictRollup(const int* domain_counts, const int* complement_counts, int count, int lattice_domain,
                                int lattice_complement);

// The 4-connected (domain, complement) counts of a chart mask, false on a rim breach. The domain
// count labels the valid mask. The complement count labels the invalid mask with every component
// holding an off-chart node merged into one (U_P lies in the open hemisphere u . n_a > 0, so the
// pushed rim ring is invalid and connected, and an invalid region reaching it joins the one
// far-hemisphere component; an invalid component with no off-chart node is an island, counted
// separately). A VALID node on the pushed rim breaches that premise and voids the counts — the
// rim would merge what it touches — rather than counting through it (fail closed). The synthetic
// masks' subject (no field involved).
bool ChartComponentCounts(const unsigned char* valid, const unsigned char* off_chart, int grid, int* domain,
                          int* complement);

// Component counts of U_P and of its complement of one field (LI domain_topology): the
// `lattice_n`-point Fibonacci lattice, valid through DeviationField::ValidityMarginsAt (the one
// gate authority), k-NN (k = 8) connected components per side with edges longer than 3x the
// median dropped; a plural count pays the audit's cost and takes its verdict when converged.
// `ladder` == nullptr selects kDefaultAuditLadder; a non-null empty ladder (ladder_count == 0)
// turns the audit off — the rollback switch, degraded to the pre-52.7 semantics.
DomainTopology DomainTopologyOf(const DeviationField& field, int lattice_n, const int* ladder, int ladder_count);

// ---- the degenerate fold set (task 52.8's evidence) ---------------------------------------------------

// Morse kind of a critical point (LI InteriorCriticalPoint.kind's vocabulary). The partition
// consumes the kind; the saddle and degenerate labels are what it refuses.
enum class CriticalKind { kMinimum, kMaximum, kSaddle, kDegenerate };

// An interior critical point of D_P (a zero of its S^2 gradient inside U_P, or a member of a
// slab path's closed-form critical set). `value` is D_P in radians. `hessian_eigenvalues` and
// `gradient_norm` carry what the focusing layer's onset table reads off the point (LI
// InteriorCriticalPoint's fields; the partition itself consumes only the kind) — for a slab
// branch member they are NaN and 0 (LI's degenerate branch), the Newton branch fills both.
struct InteriorCriticalPoint {
  double position[3] = {};
  double value = 0.0;
  CriticalKind kind = CriticalKind::kMinimum;
  double hessian_eigenvalues[2] = { std::numeric_limits<double>::quiet_NaN(),
                                    std::numeric_limits<double>::quiet_NaN() };
  double gradient_norm = 0.0;
};

// The slab critical set of a degenerate-fold path — {+-n_M} U {u . n_M = 0} — and where it lies
// relative to U_P (LI DegenerateFoldSet). `axis_points` are +-n_M with their location;
// `circle_interior_fraction` is the fraction of a dense sampling of the crease (the great circle
// u . n_M = 0) inside U_P — 0 when the circle only grazes it, 22.76% on the beta crystal's
// 4-8-7-5. The `crease_*` fields are that sampling's cluster evidence, resolution-limited —
// evidence rather than proof (the chart-audit standard): `crease_interior_arcs` is the number of
// maximal interior runs of the crease (0 exactly when the fraction is 0; a fraction claiming
// otherwise is a contradiction the partition refuses), `crease_closed_ridge` says some interior
// run never comes within kCreaseContactMargin of dU_P (level loops around it are not the boundary
// walk's to count), and `crease_touching_arc` says the crease hugs dU_P over more than
// kCreaseTouchingArcRad — a tangency or coincidence, not a transversal crossing.
struct DegenerateFoldSet {
  bool has_axis = false;  // false exactly for M = I, whose set is degenerate: no points, fraction 0
  double axis[3] = {};    // n_M: eigenvalue +1 of a rotation, -1 of a mirror
  struct AxisPoint {
    double position[3] = {};
    DomainLocation location = DomainLocation::kExterior;
  };
  AxisPoint axis_points[2];
  int axis_point_count = 0;  // 0 without an axis, else 2 (+-n_M)
  double circle_interior_fraction = 0.0;
  int crease_interior_arcs = 0;
  bool crease_closed_ridge = false;
  bool crease_touching_arc = false;
};

// Maximal circular runs of a boolean mask as (start index, length), wrapping runs included (LI
// _circular_runs). A run crossing index 0 is ONE run — the wrap-merge lesson of 52.8's review:
// a naive linear pass on the raw mask splits it in two and miscounts both halves. Table-driven
// test subject.
std::vector<std::pair<int, int>> CircularRuns(const unsigned char* mask, int n);

// Locates the slab critical set of a degenerate fold relative to U_P and clusters its crease
// sampling (LI degenerate_fold_set): +-n_M through ValidityMarginsAt, the crease circle
// `circle_samples` points of the tangent basis at n_M, inside = binding gate above
// kBoundaryMarginAtol, the cluster fields from circular runs of that mask. A screen without an
// axis yields the empty set.
DegenerateFoldSet BuildDegenerateFoldSet(const DeviationField& field, int circle_samples);

// The interior members of a slab path's critical set (LI interior_critical_points' degenerate
// branch): the axis points located interior, value taken in the slab form (exact where the chain
// loses sqrt(eps)), kind degenerate — the ring probe of the partition decides their side. The
// non-slab branch (lattice Newton) belongs to the focusing layer, not here; a caller with a
// non-degenerate path passes its own (usually empty) interior.
std::vector<InteriorCriticalPoint> SlabInteriorCriticalPoints(const DeviationField& field,
                                                              const DegenerateFoldSet& fold_set);

// ---- the boundary loop as the partition consumes it (filled by the walk, 660.3) -----------------------

// A local extremum of D_P restricted to dU_P: inside a smooth piece (corner false) or at a
// corner. `strict` is false when the extremum is a plateau run of the loop (several samples
// equal within kExtremumAtol), not a strict local extremum: the slab-crease gate accepts only
// strict members as its blade evidence (a plateau at the blade value is the signature of a crease
// touching, not crossing, dU_P). LI BoundaryCriticalPoint.
struct BoundaryCriticalPoint {
  double position[3] = {};
  double value = 0.0;
  CriticalKind kind = CriticalKind::kMinimum;
  bool strict = true;
  bool corner = false;
};

// A corner of dU_P (LI Corner): position and the corner's D_P value; the margin bookkeeping of
// the walk's own record stays on the walk side, the partition consumes the value.
struct LoopCorner {
  double position[3] = {};
  double value = 0.0;
};

// dU_P as one closed walk, the partition's consumption face of it (LI BoundaryLoop, the subset
// the certificate reads). `critical_points` is in WALK ORDER (the alternation check is a real
// order property); `first_point` is the walk's first sample point (a plateau loop's ring probe
// is taken there — any one of its points would do, the walk's first is the natural one).
// `has_plateau` marks a loop of constant D_P (a mirror slab's crease circle as dU_P itself: no
// isolated extremum, the constant value carried by `plateau_value` alone).
//
// This type lives on the CONSUMER side by design: the boundary walk (660.3) is its producer and
// includes this header — the data contract belongs to the semantics that read it, which is not a
// layer inversion but the settled include direction of the interface.
struct BoundaryLoopData {
  std::vector<BoundaryCriticalPoint> critical_points;
  std::vector<LoopCorner> corners;
  double first_point[3] = {};
  bool has_plateau = false;
  double plateau_value = 0.0;
};

// ---- the slab-crease three-check gate (task 52.8) ------------------------------------------------------

// The three checks that let a crease through U_P take the generic partition (LI _slab_crease_gates):
// (0) the fold set's own sampling holds interior arcs of the crease (a fraction claiming arcs
// that are not there is a contradiction); (a) the boundary walk carries the blade value — D_P at
// any crease point, d_slab of a tangent basis vector of the fold axis — as a STRICT local
// maximum (only transversal crease ends produce one; a plateau at the blade is a tangency
// signature); (2a) the crease does not hug dU_P over an arc; (2b) no interior crease arc closes
// without touching dU_P. Returns true when all hold (the crease's transversal ends are then
// ordinary loop extrema and the generic mechanism applies); on failure returns false with the
// failing regime and its LI message text (report-side grep compatibility). Run only for a fold
// set with circle_interior_fraction > 0.
bool SlabCreaseGates(const DeviationField& field, const DegenerateFoldSet& fold_set, const BoundaryLoopData& loop,
                     EscapeRegime* regime, std::string* message);

// ---- the partition -----------------------------------------------------------------------------------

// (lower, upper) in radians with the constant counts of {D_P = delta} for lower < delta < upper
// (LI DeviationInterval). n_components = n_closed + n_open always.
struct DeviationInterval {
  double lower = 0.0;
  double upper = 0.0;
  int n_components = 0;
  int n_closed = 0;
  int n_open = 0;
};

// IntervalPartition's result: either the partition or the escape that refused it. The escape is
// DATA, not an exception — a fail-closed answer the report side consumes explicitly. Mechanical
// invariant (asserted by the tests, load-bearing for the consumers): `escaped` implies
// `intervals` is EMPTY and `message` carries the failing check's LI text — an ignored escape
// therefore fails loudly downstream as no intervals at all, not as stale ones.
struct PartitionResult {
  bool escaped = false;
  EscapeRegime regime = EscapeRegime::kNotDiskUnaudited;  // meaningful when escaped
  std::string message;                                    // LI's raise text, stable prefixes
  std::vector<DeviationInterval> intervals;
};

// The partition of [min D_P, max D_P] with the counts of every interval (LI interval_partition).
// `interior` is the caller's interior critical point set (the slab members for a degenerate fold —
// SlabInteriorCriticalPoints; the lattice-Newton set of the focusing layer otherwise); `fold_set`
// null for a non-degenerate path; `loop` the boundary walk's record (660.3's BoundaryLoopData);
// `topology` from DomainTopologyOf. The reasoning: between consecutive critical values (interior,
// loop extrema, corners, a plateau's constant — merged within kExtremumAtol) the counts are
// constant; every open arc end is a boundary crossing so n_open is half the crossings of every
// monotone stretch of the loop; with one interior extremum a closed loop exists exactly between
// its value and the loop extremum its sublevel component provably reaches first (the ring probe
// of the degenerate case decides min/max, and the reach is checked on a small ring at the
// touching extremum). Everything outside that reasoning escapes with its own regime and LI's
// message text — never silently.
PartitionResult IntervalPartition(const DeviationField& field, const std::vector<InteriorCriticalPoint>& interior,
                                  const DegenerateFoldSet* fold_set, const BoundaryLoopData& loop,
                                  const DomainTopology& topology);

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_DP_PARTITION_HPP_
