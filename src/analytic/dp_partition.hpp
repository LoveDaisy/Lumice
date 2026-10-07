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

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_DP_PARTITION_HPP_
