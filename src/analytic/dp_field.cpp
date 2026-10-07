#include "analytic/dp_field.hpp"

#include <cmath>

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

FoldScreen BuildFoldScreen(const FaceNormalTable& table, const int* slots, int slot_count) {
  FoldScreen screen;
  // M = S(n_k) ... S(n_1), one reflection per internal face, left-multiplied in path order (LI
  // geometry.fold_matrix).
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
        product[i * 3 + j] = s[i * 3 + 0] * screen.fold_matrix[0 * 3 + j] +
                             s[i * 3 + 1] * screen.fold_matrix[1 * 3 + j] +
                             s[i * 3 + 2] * screen.fold_matrix[2 * 3 + j];
      }
    }
    for (int i = 0; i < 9; i++) {
      screen.fold_matrix[i] = product[i];
    }
  }
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

int ValidityMargins(const double* domain_margins, int margin_count, double out[kMaxFaceCount + 2]) {
  int count = 0;
  for (int i = 0; i < margin_count; i++) {
    // The validity subset is every position except the internal TIR slots — odd positions of the
    // internal interfaces, i.e. positions 3, 5, ... (position 1 is the entry Snell discriminant,
    // a gate). Rebuilt from the layout, never restated (LI validity_margin_indices).
    if (i % 2 == 0 || i == 1 || i == margin_count - 1) {
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

FieldSample DeviationField::Sample(const double u[3]) const {
  FieldSample sample;
  // One chain evaluation at the identity pose (the field is a function of the body frame):
  // incident = -u, everything else follows from it.
  const double identity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
  const double incident[3] = { -u[0], -u[1], -u[2] };
  double margins[kMaxFaceCount + 2];
  ChainDomain domain{ margins };
  ChainInterfaceDiagnostics<double> interfaces;
  double outgoing[3];
  double segments[3 * (kMaxFaceCount + 1)];
  double transmittances[kMaxFaceCount];
  PathOutputs detail{ {}, 0.0, segments, transmittances };
  const bool valid = TracePathChain<double, ChainEvaluation::kEvaluateAll>(
      *table_, slots_, slot_count_, refractive_index_, incident, identity, outgoing, &detail, &domain, &interfaces);

  sample.d_p = Deviation(outgoing, u);
  sample.v_p = valid;
  sample.t_p = valid ? detail.fresnel_transmission : 0.0;
  sample.margin_count = DomainMargins(interfaces, slot_count_, sample.margins);
  const int validity_count = ValidityMargins(sample.margins, sample.margin_count, sample.validity_margins);

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

  // A_P at the identity pose: s_body = R^T s = -u.
  const double s_body[3] = { -u[0], -u[1], -u[2] };
  const EntryMeasure entry = corridor_.Evaluate(s_body, refractive_index_);
  sample.a_p = entry.value;
  sample.location = LocateByValidityMargins(sample.validity_margins, validity_count);
  return sample;
}

}  // namespace lumice::analytic
