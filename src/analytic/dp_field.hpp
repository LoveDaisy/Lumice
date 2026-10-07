#ifndef LUMICE_ANALYTIC_DP_FIELD_HPP_
#define LUMICE_ANALYTIC_DP_FIELD_HPP_

// The deviation field of one fixed face sequence on the body-frame sphere (LI dp_field.field,
// docs/phase2.md section 1): u is the source direction in the crystal frame, the incoming ray is
// -u, and D_P(u) = angle(Phi_P(-u), -u) at the identity pose — the field does not see the sun
// direction, only the crystal. By the u-S^2 reduction every geometric and optical weight of the
// path (the entry measure A_P, the transmission T_P, the validity gates, V_P, D_P) is a function
// of u alone; rho sees the full pose and belongs to the measure layer, not here.
//
// One chain evaluation per point (path_chain.hpp's TracePathChain in kEvaluateAll mode — LI
// trace_path: the smooth branch keeps evaluating outside the gates, the exit refraction going NaN
// beyond its Snell limit), so this layer never re-derives a gate or a refraction. The internal TIR
// discriminant joins the domain margin vector in LI's sign n^2 (1 - cos^2) - 1 here, converted
// from the chain's negated convention at the one point below (path_chain.hpp ChainDomain records
// the decision). U_P is where every validity margin is positive; the internal TIR discriminants
// stay in the vector as diagnostics and bound nothing (a partial reflection keeps the point in
// U_P with a smaller Fresnel weight).
//
// Internal header of the analytic kernel: nothing here is part of the C ABI (the report surface is
// a later subtask's). Consumers: the U_P partition, the boundary and weight-kink walks, the
// focusing/chromatic classification — the field layer is their single data source.

#include "analytic/entry_measure.hpp"
#include "analytic/path_chain.hpp"
#include "analytic/path_evaluation.hpp"

namespace lumice::analytic {

// |n_a . M^T n_b| within this of 1 is a slab (LI FOLD_DOT_ATOL; the dot is exactly -1.0 in
// float64 on the fixtures, and the nearest non-slab value on the prism is 0.5, so this is not a
// tuning knob).
constexpr double kFoldDotAtol = 1e-9;
// A point is on dU_P rather than inside when its smallest validity margin is within this of zero
// (LI BOUNDARY_MARGIN_ATOL).
constexpr double kBoundaryMarginAtol = 1e-10;
// A validity margin below -kViolationAtol violates U_P; rounding of a margin that only touches
// zero (a square such as the exit Snell discriminant = entry incidence cosine^2 on 3-5-6-7-3) is
// ~2e-16 and must not fail the closure test. LI boundary.VIOLATION_ATOL, pinned at 100x that
// rounding (the observed negative extreme of a Newton-settled corner is -1e-15); the coupling
// "the closure threshold reuses the violation constant" is LI's own decision, carried here.
constexpr double kViolationAtol = 1e-13;

// Where u sits relative to U_P (LI location): interior, boundary (smallest validity margin within
// kBoundaryMarginAtol of zero), or exterior.
enum class DomainLocation { kInterior, kBoundary, kExterior };

// The field's first- and second-order jets at one u, from one Jet2<4> chain evaluation with
// (u_0, u_1, u_2, n) as the four dual directions — the field layer's ∂/∂u and ∂/∂n in one sweep
// (LI index_derivatives_batch: dD_P/dn is the direction dispersion of the field, 0 for a slab;
// d disc_k/dn says which wavelength reflects totally on which side of a TIR onset).
//   value:            the field as evaluated (d_slab for a degenerate fold, d_p otherwise)
//   tangent_gradient: the S^2 gradient g - (g . u) u (ambient gradient projected; orthogonal to u)
//   tangent_basis:    (e1, e2), the cross with the least-aligned coordinate axis (LI tangent_basis)
//   hessian:          the Riemannian Hessian B (H - (u . g) I) B^T in that basis. The (u . g)
//                     term is the sphere's second fundamental form and is not optional: at the 3-5
//                     minimum-deviation point the ambient gradient is radial, not zero, and
//                     dropping the term flips both eigenvalue signs (LI measured [-5.4, -4.7]
//                     naive against [+0.34, +0.96] corrected). It equals the Hessian of the field
//                     in the chart u(t) = normalize(u + t1 e1 + t2 e2), which is the form the
//                     difference oracle tests.
struct FieldJet {
  double value = 0.0;
  double tangent_gradient[3] = {};
  double tangent_basis[2][3] = {};
  double hessian[2][2] = {};
  double d_p_dn = 0.0;
  double margins[2 * kMaxFaceCount] = {};
  double margins_dn[2 * kMaxFaceCount] = {};
  int margin_count = 0;
};

// The fold pre-screen of a face sequence (LI FoldScreen): with n_a the entry normal and
// n~_b = M^T n_b the unfolded exit normal (M the fold matrix, the reflections of the internal
// faces left-multiplied in path order), |n_a . n~_b| = 1 means the entry and exit refractions
// cancel (a slab): Phi_P(-u) = -M u on the closure of U_P and D_P(u) = angle(M u, u) (d_slab),
// whose critical set is fixed by M alone. Such a path has no interior fold unless that set meets
// the interior of U_P.
struct FoldScreen {
  double dot = 0.0;                                       // n_a . M^T n_b
  bool degenerate = false;                                // |dot| = 1 within kFoldDotAtol: a slab
  double fold_matrix[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };  // row-major M
  bool has_axis = false;                                  // false exactly for M = I (no internal reflection)
  double axis[3] = {};  // the fixed axis n_M: eigenvalue +1 of a rotation, -1 of a mirror
};

// Builds the screen from the resolved slots. The axis of an orthogonal M comes from the
// antisymmetric part (2 sin(theta) [axis]); at theta = pi that vanishes and the closed form
// M = 2 a a^T - I gives it instead; a mirror is asked for its -1 axis by negating M first. A
// fully degenerate M = -I (every direction a fixed axis) returns +x, as arbitrary as LI's
// eigenvector there.
FoldScreen BuildFoldScreen(const FaceNormalTable& table, const int* slots, int slot_count);

// The prefix fold matrix M = S(n_k) ... S(n_1) over `slots`'s internal faces (slots 1 ..
// slot_count - 2), left-multiplied in path order, row-major into `out` (LI geometry.fold_matrix).
// The single authority of the product: BuildFoldScreen's matrix is this at the full sequence, and
// the boundary walk's incidence normals read its prefixes (R_{k-1}^T n_k) and its transpose at
// the exit (M^T n_b) — no second reflection-product loop anywhere else (a56).
void FoldMatrixOf(const FaceNormalTable& table, const int* slots, int slot_count, double out[9]);

// The domain margin vector in LI's order and sign (LI optics.domain_margin_names): entry
// (incidence cosine, Snell discriminant), each internal reflection (incidence cosine, TIR
// discriminant n^2 (1 - cos^2) - 1, positive = total), exit (incidence cosine, Snell
// discriminant). `slot_count` interfaces -> 2 * slot_count margins. The TIR negation from the
// chain's convention happens here, at this one function — the conversion point path_chain.hpp's
// ChainDomain names. Valid for any S the chain runs (double for values, Jet2<4> for derivatives).
template <class S>
int DomainMargins(const ChainInterfaceDiagnostics<S>& interfaces, int slot_count, S out[2 * kMaxFaceCount]);

// The gates of U_P: the domain margins at the validity subset positions (LI
// validity_margin_indices — every position except the internal TIR slots, in the same order).
// Returns how many were written (slot_count + 2).
int ValidityMargins(const double* domain_margins, int margin_count, double out[kMaxFaceCount + 2]);

// Position i of the domain margin vector is a validity gate (LI validity_margin_indices): every
// position except the internal TIR slots — odd positions of the internal interfaces. The single
// statement of the subset's layout: ValidityMargins reads it for the values, the boundary walk's
// active list for the indices, so the two can never drift apart.
inline bool IsValidityMarginPosition(int i, int margin_count) {
  return i % 2 == 0 || i == 1 || i == margin_count - 1;
}

// DomainLocation of u given its validity margins (LI location): a non-finite smallest margin is
// exterior; within kBoundaryMarginAtol of zero is boundary; the sign decides otherwise.
DomainLocation LocateByValidityMargins(const double* validity_margins, int count);

// (e1, e2) at unit u: cross with the least-aligned coordinate axis, so the basis never degenerates
// (|u x axis| = sqrt(1 - u_axis^2) >= sqrt(2/3); LI tangent_basis). The basis of the ring probes
// and the crease circle of the partition, and of the boundary walk to come.
void TangentBasis(const double u[3], double basis[2][3]);

// One point of the field: the five quantities of the layer plus the domain bookkeeping, from one
// chain evaluation. `d_p` is the chain's deviation in atan2 form (arccos loses sqrt(eps) next to
// D = 0 and pi, which slab paths reach on whole arcs) — NaN beyond the exit Snell limit, the
// smooth branch's own boundary. `d_value` is the field as this package evaluates it: d_slab for a
// degenerate fold (exact where the chain loses sqrt(eps) or goes NaN on the crease), d_p
// otherwise. `d_p_grazing` / `d_p_exit_limit` are the exit-TIR-curve limit values (the same
// transmitted-direction limit disc -> 0+, two float paths: the grazing form starts from the
// direction — already NaN wherever disc < 0 — and the exit-limit form recomputes without any
// root, so it stays finite as long as the chain up to the exit does); they exist to read values
// ON that curve, at a deep interior point they are off d_p by ~sqrt(disc), and d_slab paths do
// not need them. `t_p` is the chain's Fresnel product, 0 outside V_P (LI
// fresnel_transmission_path). `a_p` is the finite-crystal entry measure at the identity pose
// (0 unless the corridor exists).
struct FieldSample {
  double d_p = 0.0;
  double d_value = 0.0;
  double d_p_grazing = 0.0;
  double d_p_exit_limit = 0.0;
  double a_p = 0.0;
  double t_p = 0.0;
  bool v_p = false;
  DomainLocation location = DomainLocation::kExterior;
  double margins[2 * kMaxFaceCount] = {};
  int margin_count = 0;
  double validity_margins[kMaxFaceCount + 2] = {};
  int validity_count = 0;
};

// True when every validity margin is at or above -kViolationAtol: u is in the closure of U_P as
// far as rounding can tell (a Newton-settled corner sits at gate values ~±1e-16). The predicate
// half of the exit-Snell closure convention (LI boundary.Walker.violated's complement).
bool OnUpClosure(const double* validity_margins, int count);

// The routed deviation of a sample (LI boundary.Walker.d, the exit-Snell closure convention of LI
// task 52.6). Three-way, first match wins:
//   1. a finite `d_value` passes through unchanged — the healthy path is bit-identical, the
//      router never re-evaluates what the chain already answered (slab paths live here: d_slab
//      has no square root);
//   2. a non-finite d_p of a non-slab path whose gates pass OnUpClosure returns
//      `d_p_exit_limit` (the disc -> 0+ closure value: the exit square root reads the
//      rounding-negative side of the exit Snell discriminant exactly at a settled corner or a
//      kink arc coincident with the exit-Snell zero set);
//   3. anything else — non-finite off the closure, or a non-finite value of a slab path (an
//      anomaly: d_slab has no root to round) — is kNotFinite, and the caller must fail closed
//      (the value is not guessed; LI raises RuntimeError there).
// `slab_path` is the field's fold().degenerate, passed explicitly. The consumers are the boundary
// and weight-kink walks (660.3); the partition's ring probes stay strictly inside U_P and read
// `d_value` directly.
enum class RoutedDeviationStatus { kOk, kNotFinite };
RoutedDeviationStatus RoutedDeviation(const FieldSample& sample, bool slab_path, double* out);

// The domain margin vector and its ambient u-gradient in one struct (LI margins_jacobian's pair):
// the same margin vector DomainMarginsAt reports, next to d(margin)/d(u_i) from a Jet<3> sweep.
// Ambient, not tangent-projected — projection belongs to the consumer that knows its own metric
// use (the walk's tangent gradient; the chart differences of the tests).
struct MarginJet {
  double margins[2 * kMaxFaceCount] = {};
  double gradient[2 * kMaxFaceCount][3] = {};
  int margin_count = 0;
};

// The fixed face sequence on one crystal as a field: holds the resolved slots, the fold screen
// and the corridor, and evaluates points. One chain evaluation per point, no allocation once
// constructed (the corridor's clipping scratch grows once). Batch (vmap) shapes wait for a real
// consumer; a single-point entry is the object the partition and the walks take.
//
// Lifetime: the constructor borrows `normals` and `polygons` by pointer — both tables must outlive
// every use of the field (the entry measure and every chain evaluation dereference them).
//
// Thread safety: Sample and Differentiate on ONE instance are not thread-safe — the corridor's
// clipping scratch is mutable member state. Parallel consumers build one field per thread (the
// constructor is cheap: the fold screen and the corridor polygons are small); ValidityMarginsAt is
// the one evaluation entry that touches no mutable member and is safe to call concurrently.
class DeviationField {
 public:
  // `slots` as ResolveFaceSequence returns them, 2 <= slot_count <= kMaxFaceCount; `polygons` the
  // same crystal's corner polygons (BuildFaceNormals's second output) — the entry measure reads
  // them, and so must outlive this object (class docstring); `refractive_index` the wavelength's.
  DeviationField(const FaceNormalTable& normals, const FacePolygonTable& polygons, const int* slots, int slot_count,
                 double refractive_index);

  const FoldScreen& fold() const { return fold_; }
  double RefractiveIndex() const { return refractive_index_; }
  int SlotCount() const { return slot_count_; }
  // The construction inputs, for the walk layers (the boundary walk's identity detection reads
  // the face numbers, its incidence normals the table's slots — one field, one sequence).
  const FaceNormalTable& table() const { return *table_; }
  const int* slots() const { return slots_; }
  // The entry face's body normal n_a: the axis of the orthographic chart the topology audit
  // projects U_P onto (U_P lies in its open hemisphere, the entry incidence gate).
  const double* EntryNormal() const { return table_->normal[slots_[0]]; }

  // The validity margins of u with no other bookkeeping: one kEvaluateAll chain evaluation, the
  // domain margin vector, the validity subset — nothing else (no corridor: no mutable member is
  // touched, the call is const and thread-safe). This is the single gate-reading entry of the
  // batch paths (the lattice count, the chart audit and the fold-set sampling of the partition):
  // "u is in U_P" is `every margin > 0` here and nowhere else.
  int ValidityMarginsAt(const double u[3], double out[kMaxFaceCount + 2]) const;

  // The FULL domain margin vector of u (LI margin_vector's layout: validity gates and the internal
  // TIR diagnostics together, LI's TIR sign) with no other bookkeeping — like ValidityMarginsAt
  // in touching no mutable member, but answering the diagnostics too. The boundary walk's gate
  // bookkeeping and the kink walks read margins the validity subset does not carry (the internal
  // TIR discriminants are their subject); returns 2 * slot_count.
  int DomainMarginsAt(const double u[3], double out[2 * kMaxFaceCount]) const;

  // The slab field angle(M u, u) (LI d_slab); meaningful for a degenerate fold, where it equals
  // d_p on the closure of U_P.
  double DSlab(const double u[3]) const;

  FieldSample Sample(const double u[3]) const;

  // Everything Sample evaluates except the entry measure: one chain evaluation's five quantities
  // and domain bookkeeping, with `a_p` left at 0 because no corridor is touched — no mutable
  // member, const and thread-safe like ValidityMarginsAt. The walks' evaluation entry: a boundary
  // or kink walk asks for thousands of values and gradients and never for A_P (rho, which needs
  // it, belongs to the measure layer), so it never pays the corridor's clipping.
  FieldSample SampleOptical(const double u[3]) const;

  // The jets of the field at u (FieldJet): one Jet2<4> evaluation of the same chain, so value,
  // gradients and Hessian come from one expression tree — no second derivation of anything
  // Sample computes.
  FieldJet Differentiate(const double u[3]) const;

  // The domain margin vector with its AMBIENT u-gradient, from one Jet<3> chain evaluation (LI
  // margins_jacobian, jacfwd first order): the values in `margins` and d(margin)/d(u_i) in
  // `gradient`, the ambient gradient of the R^3 embedding. The sphere's tangent projection is the
  // consumer's step (LI boundary._tangent), deliberately not done here; the refractive index is a
  // plain constant of the sweep (no d/dn — that is FieldJet's fourth direction, the focusing
  // layer's). The walks' corrector, corner Newton and most-violated ranking all read this.
  void MarginsWithGradient(const double u[3], MarginJet* out) const;

 private:
  const FaceNormalTable* table_;
  int slots_[kMaxFaceCount];
  int slot_count_;
  double refractive_index_;
  FoldScreen fold_;
  // Mutable for Corridor::Evaluate's clipping scratch (its own design: a pose evaluation is
  // allocation-free once the scratch has grown, at the cost of member state).
  mutable Corridor corridor_;
};

// The U_P membership predicate (every validity margin strictly positive) — the one definition the
// "here and nowhere else" sentence on ValidityMarginsAt points at: the boundary walk's start-point
// search and the kink curves' clipping read this, not per-consumer copies (a56).
bool InsideUp(const DeviationField& field, const double u[3]);

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_DP_FIELD_HPP_
