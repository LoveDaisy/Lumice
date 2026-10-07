#include "analytic/dp_field.hpp"

#include <cmath>

#include "analytic/jet2.hpp"

namespace lumice::analytic {
namespace {

double Dot3(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void Cross3(const double a[3], const double b[3], double out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

void MatVec(const double m[9], const double v[3], double out[3]) {
  for (int i = 0; i < 3; i++) {
    out[i] = m[i * 3 + 0] * v[0] + m[i * 3 + 1] * v[1] + m[i * 3 + 2] * v[2];
  }
}

// angle(a, b) in atan2 form — the deviation measure of dp_field (arccos of the dot loses
// sqrt(eps) next to 0 and pi, which slab paths reach on whole boundary arcs).
double AngleBetween(const double a[3], const double b[3]) {
  double cross[3];
  Cross3(a, b, cross);
  return std::atan2(std::sqrt(Dot3(cross, cross)), Dot3(a, b));
}

// D_P's form: angle(Phi_P(-u), -u).
double Deviation(const double phi[3], const double u[3]) {
  double minus_u[3] = { -u[0], -u[1], -u[2] };
  return AngleBetween(phi, minus_u);
}

double Determinant(const double m[9]) {
  return m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6]) + m[2] * (m[3] * m[7] - m[4] * m[6]);
}

}  // namespace

// ---- fold pre-screen --------------------------------------------------------------------------------

void FoldMatrixOf(const FaceNormalTable& table, const int* slots, int slot_count, double out[9]) {
  for (int i = 0; i < 9; i++) {
    out[i] = i % 4 == 0 ? 1.0 : 0.0;
  }
  // One Householder reflection S(n) = I - 2 n n^T per internal face, left-multiplied onto the
  // running product in path order (LI geometry.fold_matrix; a prefix call with slot_count = k + 1
  // yields R_k, the fold of the first k - 1 reflections).
  for (int k = 1; k < slot_count - 1; k++) {
    const double* n = table.normal[slots[k]];
    double s[9];
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        s[i * 3 + j] = (i == j ? 1.0 : 0.0) - 2.0 * n[i] * n[j];
      }
    }
    double product[9];
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        product[i * 3 + j] =
            s[i * 3 + 0] * out[0 * 3 + j] + s[i * 3 + 1] * out[1 * 3 + j] + s[i * 3 + 2] * out[2 * 3 + j];
      }
    }
    for (int i = 0; i < 9; i++) {
      out[i] = product[i];
    }
  }
}

FoldScreen BuildFoldScreen(const FaceNormalTable& table, const int* slots, int slot_count) {
  FoldScreen screen;
  // M = S(n_k) ... S(n_1), one reflection per internal face, left-multiplied in path order (LI
  // geometry.fold_matrix) — the product's one authority is FoldMatrixOf.
  FoldMatrixOf(table, slots, slot_count, screen.fold_matrix);
  // n_a . M^T n_b with the body normals of the entry and exit faces.
  double m_t[9];
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      m_t[i * 3 + j] = screen.fold_matrix[j * 3 + i];
    }
  }
  const double* n_b = table.normal[slots[slot_count - 1]];
  double n_b_tilde[3];
  MatVec(m_t, n_b, n_b_tilde);
  screen.dot = Dot3(table.normal[slots[0]], n_b_tilde);
  screen.degenerate = std::fabs(std::fabs(screen.dot) - 1.0) <= kFoldDotAtol;

  // The fixed axis: none for M = I (LI fold_axis's own test, 1e-12 entrywise).
  double eye_distance = 0.0;
  for (int i = 0; i < 9; i++) {
    eye_distance = std::max(eye_distance, std::fabs(screen.fold_matrix[i] - (i % 4 == 0 ? 1.0 : 0.0)));
  }
  if (eye_distance <= 1e-12) {
    screen.has_axis = false;
    return screen;
  }
  screen.has_axis = true;
  // A mirror (det < 0) is asked for its -1 axis: -M is the rotation about that same axis.
  const double sign = Determinant(screen.fold_matrix) > 0.0 ? 1.0 : -1.0;
  double r[9];
  for (int i = 0; i < 9; i++) {
    r[i] = sign * screen.fold_matrix[i];
  }
  // +1 axis of the rotation r: the antisymmetric part is 2 sin(theta) [axis].
  double w[3] = { r[2 * 3 + 1] - r[1 * 3 + 2], r[0 * 3 + 2] - r[2 * 3 + 0], r[1 * 3 + 0] - r[0 * 3 + 1] };
  const double w_norm = std::sqrt(Dot3(w, w));
  if (w_norm > 1e-12) {
    for (int i = 0; i < 3; i++) {
      screen.axis[i] = w[i] / w_norm;
    }
    return screen;
  }
  // theta = pi: r is symmetric with r = 2 a a^T - I, so a_k^2 = (r_kk + 1) / 2 (largest diagonal
  // first, for the conditioning of the off-diagonal division below) and r_kj = 2 a_k a_j.
  int k = 0;
  for (int i = 1; i < 3; i++) {
    if (r[i * 3 + i] > r[k * 3 + k]) {
      k = i;
    }
  }
  const double a_k = std::sqrt(std::max(0.5 * (r[k * 3 + k] + 1.0), 0.0));
  screen.axis[k] = a_k;
  for (int j = 0; j < 3; j++) {
    if (j != k) {
      screen.axis[j] = a_k > 0.0 ? 0.5 * r[k * 3 + j] / a_k : 0.0;
    }
  }
  const double norm = std::sqrt(Dot3(screen.axis, screen.axis));
  for (int i = 0; i < 3; i++) {
    screen.axis[i] /= norm;
  }
  return screen;
}

// ---- margins ----------------------------------------------------------------------------------------

template <class S>
int DomainMargins(const ChainInterfaceDiagnostics<S>& interfaces, int slot_count, S out[2 * kMaxFaceCount]) {
  int count = 0;
  for (int k = 0; k < slot_count; k++) {
    out[count++] = interfaces.incidence[k];
    // The chain stores 1 - n^2 (1 - cos^2); LI's TIR discriminant is its negation. This negation
    // is the single conversion point path_chain.hpp's ChainDomain names; entry and exit keep the
    // chain's (LI's) Snell sign already.
    out[count++] = k > 0 && k < slot_count - 1 ? -interfaces.discriminant[k] : interfaces.discriminant[k];
  }
  return count;
}

// Explicit instantiation for the two scalar types the field runs.
template int DomainMargins<double>(const ChainInterfaceDiagnostics<double>&, int, double[2 * kMaxFaceCount]);
template int DomainMargins<Jet2<4>>(const ChainInterfaceDiagnostics<Jet2<4>>&, int, Jet2<4>[2 * kMaxFaceCount]);

// (e1, e2) at unit u: cross with the least-aligned coordinate axis, so the basis never degenerates
// (|u x axis| = sqrt(1 - u_axis^2) >= sqrt(2/3); LI tangent_basis).
void TangentBasis(const double u[3], double basis[2][3]) {
  int k = 0;
  for (int i = 1; i < 3; i++) {
    if (std::fabs(u[i]) < std::fabs(u[k])) {
      k = i;
    }
  }
  double axis[3] = { 0.0, 0.0, 0.0 };
  axis[k] = 1.0;
  double e1[3];
  Cross3(u, axis, e1);
  const double norm = std::sqrt(Dot3(e1, e1));
  for (int i = 0; i < 3; i++) {
    basis[0][i] = e1[i] / norm;
  }
  Cross3(u, basis[0], basis[1]);
}

int ValidityMargins(const double* domain_margins, int margin_count, double out[kMaxFaceCount + 2]) {
  int count = 0;
  for (int i = 0; i < margin_count; i++) {
    // The validity subset is every position except the internal TIR slots (IsValidityMarginPosition,
    // the layout's single statement). Rebuilt from the layout, never restated (LI
    // validity_margin_indices).
    if (IsValidityMarginPosition(i, margin_count)) {
      out[count++] = domain_margins[i];
    }
  }
  return count;
}

DomainLocation LocateByValidityMargins(const double* validity_margins, int count) {
  double smallest = validity_margins[0];
  for (int i = 1; i < count; i++) {
    smallest = std::min(smallest, validity_margins[i]);
  }
  if (!std::isfinite(smallest)) {
    return DomainLocation::kExterior;
  }
  if (std::fabs(smallest) <= kBoundaryMarginAtol) {
    return DomainLocation::kBoundary;
  }
  return smallest > 0.0 ? DomainLocation::kInterior : DomainLocation::kExterior;
}

bool OnUpClosure(const double* validity_margins, int count) {
  for (int i = 0; i < count; i++) {
    if (!(validity_margins[i] >= -kViolationAtol)) {
      return false;  // a NaN margin is off the closure too: fail closed on it
    }
  }
  return true;
}

RoutedDeviationStatus RoutedDeviation(const FieldSample& sample, bool slab_path, double* out) {
  if (std::isfinite(sample.d_value)) {
    *out = sample.d_value;
    return RoutedDeviationStatus::kOk;
  }
  if (!slab_path && OnUpClosure(sample.validity_margins, sample.validity_count)) {
    *out = sample.d_p_exit_limit;
    return RoutedDeviationStatus::kOk;
  }
  return RoutedDeviationStatus::kNotFinite;
}

// ---- the field --------------------------------------------------------------------------------------

DeviationField::DeviationField(const FaceNormalTable& normals, const FacePolygonTable& polygons, const int* slots,
                               int slot_count, double refractive_index)
    : table_(&normals), slot_count_(slot_count), refractive_index_(refractive_index),
      fold_(BuildFoldScreen(normals, slots, slot_count)), corridor_(normals, polygons, slots, slot_count) {
  for (int i = 0; i < slot_count; i++) {
    slots_[i] = slots[i];
  }
}

double DeviationField::DSlab(const double u[3]) const {
  double mu[3];
  MatVec(fold_.fold_matrix, u, mu);
  return AngleBetween(mu, u);
}

int DeviationField::ValidityMarginsAt(const double u[3], double out[kMaxFaceCount + 2]) const {
  // The same kEvaluateAll chain evaluation Sample runs, stopped before everything past the
  // margins: no deviation, no corridor — no mutable member is touched (the class docstring's
  // thread-safety paragraph). The batch paths of the partition read the gates here and nowhere
  // else.
  const double identity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
  const double incident[3] = { -u[0], -u[1], -u[2] };
  ChainInterfaceDiagnostics<double> interfaces;
  double outgoing[3];
  TracePathChain<double, ChainEvaluation::kEvaluateAll>(*table_, slots_, slot_count_, refractive_index_, incident,
                                                        identity, outgoing, nullptr, nullptr, &interfaces);
  double margins[2 * kMaxFaceCount];
  const int margin_count = DomainMargins(interfaces, slot_count_, margins);
  return ValidityMargins(margins, margin_count, out);
}

int DeviationField::DomainMarginsAt(const double u[3], double out[2 * kMaxFaceCount]) const {
  // The same corridor-free chain evaluation ValidityMarginsAt runs, answering the full vector: the
  // walk's gate bookkeeping ranks the internal TIR diagnostics too, which the validity subset
  // never carries. Still no mutable member (the class docstring's thread-safety paragraph).
  const double identity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
  const double incident[3] = { -u[0], -u[1], -u[2] };
  ChainInterfaceDiagnostics<double> interfaces;
  double outgoing[3];
  TracePathChain<double, ChainEvaluation::kEvaluateAll>(*table_, slots_, slot_count_, refractive_index_, incident,
                                                        identity, outgoing, nullptr, nullptr, &interfaces);
  return DomainMargins(interfaces, slot_count_, out);
}

FieldSample DeviationField::SampleOptical(const double u[3]) const {
  FieldSample sample;
  // One chain evaluation at the identity pose (the field is a function of the body frame):
  // incident = -u, everything else follows from it. The chain's ChainDomain output is not read:
  // the layer's margins come from the interface diagnostics through DomainMargins — the single
  // conversion point of the TIR sign — and the domain's own validity packing would be a second
  // derivation of the same gates (nullptr below says so at the call site).
  const double identity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
  const double incident[3] = { -u[0], -u[1], -u[2] };
  ChainInterfaceDiagnostics<double> interfaces;
  double outgoing[3];
  double segments[3 * (kMaxFaceCount + 1)];
  double transmittances[kMaxFaceCount];
  PathOutputs detail{ {}, 0.0, segments, transmittances };
  const bool valid = TracePathChain<double, ChainEvaluation::kEvaluateAll>(
      *table_, slots_, slot_count_, refractive_index_, incident, identity, outgoing, &detail, nullptr, &interfaces);

  sample.d_p = Deviation(outgoing, u);
  sample.v_p = valid;
  sample.t_p = valid ? detail.fresnel_transmission : 0.0;
  sample.margin_count = DomainMargins(interfaces, slot_count_, sample.margins);
  sample.validity_count = ValidityMargins(sample.margins, sample.margin_count, sample.validity_margins);

  sample.d_value = fold_.degenerate ? DSlab(u) : sample.d_p;

  const int last = slot_count_ - 1;
  // d_p_grazing: the exit root dropped after the fact — NaN wherever the direction already is
  // (disc < 0), exactly the situation it exists to sit next to. The exit face's outward normal:
  // the transmitted direction is n d - (n c - sqrt(disc)) N, so subtracting sqrt(disc) N again
  // leaves n d - n c N, the disc -> 0+ limit.
  const double root = std::sqrt(std::max(interfaces.discriminant[last], 0.0));
  const double* n_exit = table_->normal[slots_[last]];
  const double grazing[3] = { outgoing[0] - root * n_exit[0], outgoing[1] - root * n_exit[1],
                              outgoing[2] - root * n_exit[2] };
  sample.d_p_grazing = Deviation(grazing, u);
  // d_p_exit_limit: the same limit recomputed with no root anywhere, from the last internal
  // direction (the entry's refracted ray when there is no internal reflection — segment `last`)
  // and the exit incidence cosine. The pose is the identity, so the body-frame segments are the
  // world-frame directions.
  const double* d_int = detail.segment_directions + 3 * last;
  const double d_exit[3] = { refractive_index_ * d_int[0] - refractive_index_ * interfaces.incidence[last] * n_exit[0],
                             refractive_index_ * d_int[1] - refractive_index_ * interfaces.incidence[last] * n_exit[1],
                             refractive_index_ * d_int[2] -
                                 refractive_index_ * interfaces.incidence[last] * n_exit[2] };
  sample.d_p_exit_limit = Deviation(d_exit, u);

  sample.location = LocateByValidityMargins(sample.validity_margins, sample.validity_count);
  return sample;
}

FieldSample DeviationField::Sample(const double u[3]) const {
  // SampleOptical plus the entry measure — the corridor is the only mutable-member touch, kept
  // here and out of the walks' entry.
  FieldSample sample = SampleOptical(u);
  const double s_body[3] = { -u[0], -u[1], -u[2] };
  const EntryMeasure entry = corridor_.Evaluate(s_body, refractive_index_);
  sample.a_p = entry.value;
  return sample;
}

namespace {

// angle(a, b) = atan2(|a x b|, a . b) in jets — the same formula as the double path's
// AngleBetween, so the jet is the derivative of the value the layer reports, not of a lookalike.
// |a x b| crosses zero at angle 0 and pi; Sqrt there is non-finite in any forward mode — LI's JAX
// norm behaves the same. The value stays exact; consumers stay off the crease.
template <int N>
Jet2<N> AngleJet(const Jet2<N> a[3], const Jet2<N> b[3]) {
  Jet2<N> cross[3] = { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
  Jet2<N> cross_sq = cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2];
  Jet2<N> dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  return Atan2(Sqrt(cross_sq), dot);
}

}  // namespace

FieldJet DeviationField::Differentiate(const double u[3]) const {
  using J = Jet2<4>;
  FieldJet jet;
  // The identity pose as constants; the four dual directions are u_0, u_1, u_2 (slots 0..2) and
  // the refractive index (slot 3) — the ∂/∂n of LI index_derivatives_batch.
  const J pose[9] = { J(1.0), J(0.0), J(0.0), J(0.0), J(1.0), J(0.0), J(0.0), J(0.0), J(1.0) };
  const J index = J::Variable(refractive_index_, 3);
  J u_jet[3] = { J::Variable(u[0], 0), J::Variable(u[1], 1), J::Variable(u[2], 2) };
  const J incident[3] = { -u_jet[0], -u_jet[1], -u_jet[2] };
  J outgoing[3];
  ChainInterfaceDiagnostics<J> interfaces;
  TracePathChain<J, ChainEvaluation::kEvaluateAll>(*table_, slots_, slot_count_, index, incident, pose, outgoing,
                                                   nullptr, nullptr, &interfaces);

  // The field's value jet: the slab form for a degenerate fold (no n in it — d_p_dn comes out 0
  // on its own, LI's "0 for a slab"), the chain deviation atan2(|phi x (-u)|, phi . (-u))
  // otherwise.
  Jet2<4> minus_u[3] = { -u_jet[0], -u_jet[1], -u_jet[2] };
  J mu[3];
  for (int i = 0; i < 3; i++) {
    mu[i] = fold_.fold_matrix[i * 3 + 0] * u_jet[0] + fold_.fold_matrix[i * 3 + 1] * u_jet[1] +
            fold_.fold_matrix[i * 3 + 2] * u_jet[2];
  }
  const J value = fold_.degenerate ? AngleJet(mu, u_jet) : AngleJet(outgoing, minus_u);

  jet.value = value.a;
  jet.d_p_dn = value.v[3];

  // Ambient gradient and Hessian in the u slots; the tangent projection and the second
  // fundamental form term give the S^2 objects (FieldJet's docstring).
  double gradient[3];
  double hessian[3][3];
  for (int i = 0; i < 3; i++) {
    gradient[i] = value.v[i];
    for (int j = 0; j < 3; j++) {
      hessian[i][j] = value.h[i][j];
    }
  }
  const double radial = Dot3(gradient, u);
  for (int i = 0; i < 3; i++) {
    jet.tangent_gradient[i] = gradient[i] - radial * u[i];
  }
  TangentBasis(u, jet.tangent_basis);
  const double* e1 = jet.tangent_basis[0];
  const double* e2 = jet.tangent_basis[1];
  for (int a = 0; a < 2; a++) {
    const double* ea = a == 0 ? e1 : e2;
    for (int b = 0; b < 2; b++) {
      const double* eb = b == 0 ? e1 : e2;
      double sum = 0.0;
      for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
          sum += ea[i] * (hessian[i][j] - (i == j ? radial : 0.0)) * eb[j];
        }
      }
      jet.hessian[a][b] = sum;
    }
  }

  // The domain margin vector and its index derivative from the same run.
  J margins[2 * kMaxFaceCount];
  jet.margin_count = DomainMargins(interfaces, slot_count_, margins);
  for (int i = 0; i < jet.margin_count; i++) {
    jet.margins[i] = margins[i].a;
    jet.margins_dn[i] = margins[i].v[3];
  }
  return jet;
}

void DeviationField::MarginsWithGradient(const double u[3], MarginJet* out) const {
  // One Jet<3> chain evaluation — the same chain body the double path runs, with u seeded as the
  // three dual directions and the refractive index a plain constant (LI margins_jacobian jacfwd
  // in u alone; d/dn is FieldJet's fourth direction and the focusing layer's, not the walk's).
  using J = Jet<3>;
  const J pose[9] = { J(1.0), J(0.0), J(0.0), J(0.0), J(1.0), J(0.0), J(0.0), J(0.0), J(1.0) };
  J u_jet[3] = { J::Variable(u[0], 0), J::Variable(u[1], 1), J::Variable(u[2], 2) };
  const J incident[3] = { -u_jet[0], -u_jet[1], -u_jet[2] };
  J refractive_index = refractive_index_;
  J outgoing[3];
  ChainInterfaceDiagnostics<J> interfaces;
  TracePathChain<J, ChainEvaluation::kEvaluateAll>(*table_, slots_, slot_count_, refractive_index, incident, pose,
                                                   outgoing, nullptr, nullptr, &interfaces);
  J margins[2 * kMaxFaceCount];
  out->margin_count = DomainMargins(interfaces, slot_count_, margins);
  for (int i = 0; i < out->margin_count; i++) {
    out->margins[i] = margins[i].a;
    for (int k = 0; k < 3; k++) {
      out->gradient[i][k] = margins[i].v[k];
    }
  }
}

}  // namespace lumice::analytic
