#ifndef LUMICE_ANALYTIC_DP_CONTOUR_HPP_
#define LUMICE_ANALYTIC_DP_CONTOUR_HPP_

// The contour layer (scrum 660.5): quadrature weights on the critical-set CURVE objects the
// earlier layers produce, and the producers that turn a fixed chain + a pose family into the
// M1/M2 feedstock the measure layer (scrum 661, src/raypath/detail/measure/) consumes.
// Deliberately NOT ported here (the task's scope line): LI phase2's level-set extraction, the
// coarea band and its 1/(8 pi^2) line integral — the whole-image rendering orchestration that
// feeds on them is excluded, and no contract consumer eats a level-set-at-delta.
//
// Three groups:
//   1. Declared quadrature rules for curves: parameter weights (the periodic midpoint grid of a
//      closed orbit; the trapezoid of an open arc; the kink-cut variant) and the chord-measure
//      line integral. Every rule ships its NAME and its declared convergence order; the
//      two-grid refinement difference |I_N - I_2N| is the declared error tier. The kink positions
//      are STRUCTURAL (a WalkerCorner, a KinkArc's first point) — never probed numerically.
//   2. The orbit fiber-stream producer: one member (fixed face sequence) of a family whose
//      sigma->0 support runs one circle about its family axis, sampled on a periodic midpoint
//      grid of Rz(theta) R0 poses. Every point carries A and T SEPARATELY (the corridor entry
//      measure; the path power with every internal reflectance) and the TRUE body-frame sun
//      direction u = R^T s_hat — the two pre-registered integration gaps of the measure contract
//      (an adapter that only has the merged w = A*T cannot discriminate an empty corridor from a
//      TIR gate; a placeholder u is quadrature-legal but certificate-dead) are closed at the
//      producer, by construction.
//   3. The restricted family curve (the contract's canonical closed kind-1 curve: the family's
//      latitude circle with per-wavelength D_P re-sampled by n-continuation) and the kind-2/kind-3
//      chain mappings (BoundaryWalkRecord / KinkCurve -> chain shape; pure transport, no
//      recomputation).
//
// Output structs are field-compatible ANALYTIC-side mirrors of the measure contract's value
// types (FiberSampleStream, CriticalSetCurve, WeightSingularChain — src/raypath/detail/measure/
// measure_geometry_contract.hpp). The layer direction forbids including the contract here
// (analytic is below raypath, cmake/lumice_layers.cmake + the layer-inversion rule), so the
// routing enums are mirrored with parallel names and the DOCKING TEST maps value-for-value —
// drift on either side goes red there. A production adapter (about thirty lines) is deliberately
// not written yet: no production consumer exists (the B report line is not started), and the
// docking test carries the mapping until one does (the trigger is recorded in the task's
// progress, not here).
//
// Internal header of the analytic kernel: nothing here is part of the C ABI (the v8 surface is
// 660.6's). Dependency direction: dp_contour -> dp_boundary / dp_weight_kink / dp_focus /
// dp_field / entry_measure / path_evaluation, all read-only.

#include <string>
#include <vector>

#include "analytic/dp_boundary.hpp"
#include "analytic/dp_field.hpp"
#include "analytic/dp_focus.hpp"
#include "analytic/dp_weight_kink.hpp"
#include "analytic/entry_measure.hpp"
#include "analytic/path_evaluation.hpp"
#include "analytic/pose_density.hpp"

namespace lumice::analytic {

// ---- declared quadrature rules ---------------------------------------------------------------------------

// The three rules. The declared order is for a SMOOTH integrand on each smooth piece; 0 means
// spectral (the periodic rule on a smooth periodic integrand).
enum class CurveRule {
  // Closed curve: the trapezoid over the parameter circle (wrap-around). On a uniform grid —
  // the orbit producer's midpoint grid — every weight is 2 pi / N, and the rule is the periodic
  // midpoint rule: exact for trigonometric polynomials of degree < N, geometric error decay for
  // every smooth periodic integrand. A kink (C^0 only) caps it at O(h^2).
  kPeriodicMidpoint,
  // Open curve: the trapezoid over the parameter interval. O(h^2), Euler-Maclaurin constants.
  kChordTrapezoid,
  // Kink marks cut the parameter domain at the marked points; each segment is integrated by the
  // open rule, restoring the declared order within segments (the kink cell's first-order
  // contamination is removed; the seam terms remain, with their Euler-Maclaurin constants).
  kKinkSegmented,
};
const char* CurveRuleName(CurveRule rule);
int DeclaredCurveRuleOrder(CurveRule rule);

// Per-point PARAMETER weights w_i with sum_i w_i f(theta_i) ~= integral f dtheta.
//   `params`: the parameter values, strictly ascending (kKinkSegmented also allows them
//   piecewise-ascending per segment? no — strictly ascending overall; a kink may sit ON a point,
//   which then bounds two segments and carries the sum of its two half-weights).
//   `closed`: the params cover one full 2 pi-period circle and the weight of the seam interval
//   [theta_{N-1}, theta_0 + 2 pi] wraps onto its endpoints (the only closed parameterization v1
//   produces: spin orbits and latitude circles — a non-2pi period would extend this signature).
//   `kink`: per-point marks; read only under kKinkSegmented (ignored, not validated, otherwise —
//   the rule choice itself is the caller's structural statement).
// A kink rule on a closed curve cuts at every marked point; the segments between consecutive
// kinks are integrated by the open rule, so a closed kink rule needs at least one mark (the cut
// makes the circle a list of open arcs). An empty `params` returns empty weights.
std::vector<double> SampleCurveWeights(CurveRule rule, const std::vector<double>& params, bool closed,
                                       const std::vector<char>& kink);

// The chord-measure line integral over a polyline on the sphere:
//   I = sum_i (f_i + f_{i+1}) / 2 * |u_{i+1} - u_i|   (closed curves wrap; `u` is 3N packed).
// This is the SAME declared rule the measure layer froze for its weight profile
// (weight_profile.hpp: the trapezoid line integral of rho_u over the curve's own polyline) —
// one rule, two sides of the layer boundary, pinned to the same closed forms by the tests.
// `f` is N values; sizes must match. NaN-free for finite inputs.
double ChordLineIntegral(const std::vector<double>& u, const std::vector<double>& f, bool closed);

// The declared error tier: |I_N - I_2N|. The naming of the convention is the product — the
// consumer integrates twice and reports the difference as the error band.
double RefinementErrorEstimate(double integral_n, double integral_2n);

// ---- the routing-enum mirrors (measure contract, field-compatible) ---------------------------------------

// Mirrors of the measure contract's MeasureBinding / FiberSampleStream::EvidenceForm /
// ExistenceState (layer direction forbids the include; the docking test maps value-for-value and
// fails on drift). Values and order match the contract's exactly.
enum class FiberMeasureBinding { kSolidAngle, kFiberParameter };
enum class FiberEvidenceForm { kStructural, kSampledExhaustive, kSampledPartial };
enum class CurveExistence { kComputed, kEscaped, kWalkTruncated, kS4Declared };

// ---- the orbit fiber-stream producer (M1 feedstock) ------------------------------------------------------

// One point of the orbit stream: the contract's FiberSample fields plus the world-frame outgoing
// (contract-neutral; the target-assignment and family-collapse anchors read it — the fold
// identity makes it theta-independent on a member's valid arc).
struct OrbitFiberPoint {
  double u[3] = { 0.0, 0.0, 0.0 };         // R^T s_hat: the TRUE body-frame sun direction (never a placeholder)
  double outgoing[3] = { 0.0, 0.0, 0.0 };  // world frame, propagation direction
  double area = 0.0;                       // A: the corridor entry measure (0 unless the corridor opens)
  double transmission = 0.0;               // T: the path power with every internal reflectance
  bool valid = false;                      // path valid AND A > 0 AND T > 0 (the kept convention)
  bool jet_degenerate = false;             // orbit-regularity: the sun at the family-axis pole
  double parameter = 0.0;                  // theta
  double weight = 0.0;                     // 2 pi / grid
};

struct OrbitFiberStream {
  std::vector<OrbitFiberPoint> samples;
  // The producer's routing declaration (the mirrored vocabulary; see the enum block above):
  // the weights are dtheta elements of the orbit's own parameterization, and the stream covers
  // that orbit to its declared resolution (`grid`), which is the kSampledExhaustive claim.
  FiberMeasureBinding binding = FiberMeasureBinding::kFiberParameter;
  FiberEvidenceForm evidence = FiberEvidenceForm::kSampledExhaustive;
  int grid = 0;
  // The sun sits at the family-axis pole: the orbit circle collapses to a point, every sample is
  // marked jet_degenerate and `note` says so (never silent). A spec with no family axis
  // (FamilyAxis false — random orientation, or a column off the pole) returns an EMPTY stream
  // with the reason in `note`: the sigma->0 support is not one circle, so there is no orbit to
  // sample. Both are producer verdicts, not errors.
  bool spin_degenerate = false;
  std::string note;
  const char* source_name = "orbit-spin-grid";
};

// One member's spin integral feedstock: a periodic midpoint grid of grid points over the
// Rz(theta) R0 poses, R0 the identity when the family axis is +e3 (the plate family — the
// physical configs of the anchors) and otherwise the minimal rotation taking the axis to e3.
// Every pose: EvaluatePath (T, outgoing) + Corridor::Evaluate (A) + u = R^T s_hat. A and T
// depend on the pose through u alone (the reduction theorem), so the R0 choice moves no value;
// the world-frame outgoing is evaluated at the actual posed rotation.
// `sun_hat` points AT the sun; the incident propagation is -sun_hat. The degeneracy threshold
// matches the measure layer's kDegenerateSunGeometry form (|s_z| > 1 - 1e-12,
// declared_density.hpp) so the two layers call the same geometry collapsed.
OrbitFiberStream MakeOrbitFiberStream(const FaceNormalTable& normals, const FacePolygonTable& polygons,
                                      const int* slots, int slot_count, const PoseDensitySpec& spec,
                                      const double sun_hat[3], double refractive_index, int grid);

// ---- the restricted family curve (the canonical closed kind-1 curve) ------------------------------------

// Field-compatible with the contract's CriticalSetCurve. The family's latitude circle: the
// circle of u about the family axis at the sun's polar angle, with D_P sampled along it at the
// base index (`d_p`, the exit-Snell closure routing of 660.3 — RoutedDeviation, the single
// authority) and re-sampled per wavelength (`critical_d_p` row k at `indices[k]`, the 660.4
// n-continuation convention: the index table is the caller's, full-precision literals in tests).
struct RestrictedFamilyCurve {
  bool closed = true;           // the latitude circle is a loop by construction
  std::vector<double> u;        // 3N
  std::vector<double> tangent;  // 3N, unit, the theta-derivative of the circle
  std::vector<double> d_p;      // N, routed D_P at the base index
  std::vector<double> wavelengths_nm;
  std::vector<double> indices;        // parallel to wavelengths_nm, the caller's per-wavelength n
  std::vector<double> critical_d_p;   // N * wavelengths_nm.size(), row-major (point i, index k)
  std::vector<double> support_param;  // N: theta, the orbit parameter of each point
  CurveExistence existence = CurveExistence::kComputed;
  // Points whose routed D_P was non-finite even under the closure routing (off the closure of
  // U_P): their d_p / critical_d_p entries are NaN, and this count makes them visible — the
  // circle is sampled where the field does not exist, which is data, not a silent zero.
  int routed_nonfinite = 0;
  std::string note;  // the degenerate/no-axis verdict, in the producer's own words
};

// `grid` points of one period, the same circle the orbit producer walks (built directly about
// the family axis; no poses are evaluated — the field is a function of u alone). A spec without
// a family axis or a pole sun returns the same verdicts as the orbit producer (empty / all
// degenerate is not applicable here — there is no orbit to restrict to), as an EMPTY curve with
// the reason in `note`.
RestrictedFamilyCurve MakeRestrictedFamilyCurve(const FaceNormalTable& normals, const FacePolygonTable& polygons,
                                                const int* slots, int slot_count, const PoseDensitySpec& spec,
                                                const double sun_hat[3], double base_index,
                                                const double* wavelengths_nm, const double* indices,
                                                int wavelength_count, int grid);

// ---- kind-2 / kind-3 chain mapping -----------------------------------------------------------------------

// Field-compatible with the contract's WeightSingularChain, plus the D_P values the same-source
// roundtrip reads (the contract chain carries no value field).
struct ChainCurve {
  bool is_gate_boundary = false;  // kind-2 (from a boundary walk) vs kind-3 (kink arcs)
  CurveExistence existence = CurveExistence::kComputed;
  std::vector<double> u;      // 3N
  std::vector<double> param;  // N: cumulative chord arclength from point 0
  std::vector<char> kink;     // N: corners (kind-2) / arc starts (kind-3) marked 1
  std::vector<int> event;     // N: -1 everywhere (v1; see the mapping notes below)
  bool closed = false;
  std::vector<double> values;  // N: D_P passthrough, copied from the source objects
  std::string note;            // the walk message / kink bookkeeping, when there is one
};

// The mapping table (every decision in one place — a different verdict changes THIS, not the
// data): WalkStatus kOk -> kComputed; EVERY other value -> kWalkTruncated (a walk refusal is a
// truncation of the walk; the contract's kEscaped is a partition-layer vocabulary this producer
// never emits) with `message` carried in `note`. `param` is cumulative chord arclength — the
// WeightProfileSample::s convention; kind-2/3 curves have no source parameterization of their
// own, this is the declared one. `event` is -1 everywhere in v1: schema1's pinned event words
// are ray-chain semantics and no LI authority maps them onto curve points — not invented here.
// `closed` is the walk's own closure certificate (kOk closes the loop). The record's u and
// values are COPIED — same source, no recomputation; a status != kOk with a non-empty record
// cannot arise from WalkBoundary (its mechanical invariant), and the mapping trusts the status
// and ignores the record in that case.
ChainCurve ChainFromBoundaryPieces(const BoundaryWalkRecord& record, WalkStatus status,
                                   const std::string& message = "");

// The kind-3 mapping: the curve's arcs in order, concatenated (each arc's first point is marked
// kink=1 — the TIR onset — so a consumer can re-split the components), values copied from the
// arcs. `closed` is true iff every arc is closed (the 3-1-6 single-circle case; a curve with no
// arcs is an empty chain with the curve's own status and note). The arc completeness vocabulary
// (KinkCurve::Complete, coverage) rides in `note`: existence is the WALK's status, the arcs'
// uncertified completeness is 660.3's honesty rule and is not flattened into existence.
ChainCurve ChainFromKinkArcs(const KinkCurve& curve);

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_DP_CONTOUR_HPP_
