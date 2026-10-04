#include "analytic/entry_measure.hpp"

#include <cassert>
#include <cmath>
#include <utility>
#include <vector>

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

// An affine map x -> L x + t of the body frame, L row-major: where the unfolding has put the crystal.
struct Placement {
  double l[9] = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };
  double t[3] = { 0.0, 0.0, 0.0 };

  void Apply(const double x[3], double out[3]) const {
    for (int i = 0; i < 3; i++) {
      out[i] = l[i * 3 + 0] * x[0] + l[i * 3 + 1] * x[1] + l[i * 3 + 2] * x[2] + t[i];
    }
  }
  void Rotate(const double v[3], double out[3]) const {
    for (int i = 0; i < 3; i++) {
      out[i] = l[i * 3 + 0] * v[0] + l[i * 3 + 1] * v[1] + l[i * 3 + 2] * v[2];
    }
  }
  // Composes the reflection in the plane n . y = c (n unit) after this placement.
  void Reflect(const double n[3], double c) {
    double nl[3];  // n^T L
    for (int j = 0; j < 3; j++) {
      nl[j] = n[0] * l[0 * 3 + j] + n[1] * l[1 * 3 + j] + n[2] * l[2 * 3 + j];
    }
    const double nt = Dot3(n, t);
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        l[i * 3 + j] -= 2.0 * n[i] * nl[j];
      }
      t[i] += 2.0 * (c - nt) * n[i];
    }
  }
};

// Signed shoelace area of an (x, y)-packed polygon of `count` corners (LI _PolyBatch.area).
double PolygonArea(const std::vector<double>& p, int count) {
  double sum = 0.0;
  for (int i = 0; i < count; i++) {
    const int j = i + 1 < count ? i + 1 : 0;
    sum += p[2 * i] * p[2 * j + 1] - p[2 * i + 1] * p[2 * j];
  }
  return 0.5 * sum;
}

// LI _ensure_ccw: a projection may flip the ring's handedness; reverse it when its signed area is
// negative.
bool EnsureCcw(std::vector<double>* p, int count) {
  if (PolygonArea(*p, count) >= 0.0) {
    return false;
  }
  for (int i = 0, j = count - 1; i < j; i++, j--) {
    std::swap((*p)[2 * i], (*p)[2 * j]);
    std::swap((*p)[2 * i + 1], (*p)[2 * j + 1]);
  }
  return true;
}

// LI _clip_halfplane: one Sutherland-Hodgman step, keeping the left side of the directed edge a -> b.
// Per input corner i, in ring order: the corner itself when inside, then the crossing of edge
// i -> i+1 when that edge crosses the line.
int ClipHalfplane(const std::vector<double>& in, int count, const double a[2], const double b[2],
                  std::vector<double>* out, const std::vector<CorridorEdgeSource>* sources,
                  const CorridorEdgeSource& clipping_source, std::vector<CorridorEdgeSource>* out_sources) {
  if (out_sources) {
    out_sources->clear();
  }
  out->clear();
  const double ex = b[0] - a[0];
  const double ey = b[1] - a[1];
  for (int i = 0; i < count; i++) {
    const int j = i + 1 < count ? i + 1 : 0;
    const double px = in[2 * i];
    const double py = in[2 * i + 1];
    const double qx = in[2 * j];
    const double qy = in[2 * j + 1];
    const double s_p = ex * (py - a[1]) - ey * (px - a[0]);
    const double s_q = ex * (qy - a[1]) - ey * (qx - a[0]);
    const bool in_p = s_p >= 0.0;
    const bool in_q = s_q >= 0.0;
    if (in_p) {
      out->push_back(px);
      out->push_back(py);
      if (out_sources) {
        out_sources->push_back((*sources)[i]);
      }
    }
    if (in_p != in_q) {
      const double t = s_p / (s_p - s_q);
      out->push_back(px + t * (qx - px));
      out->push_back(py + t * (qy - py));
      if (out_sources) {
        out_sources->push_back(in_p ? clipping_source : (*sources)[i]);
      }
    }
  }
  return static_cast<int>(out->size() / 2);
}

}  // namespace

Corridor::Corridor(const FaceNormalTable& normals, const FacePolygonTable& polygons, const int* slots, int slot_count) {
  assert(slot_count >= 2);
  const int last = slot_count - 1;
  Placement placement;
  offsets_.push_back(0);
  auto append = [&](int slot) {
    for (int k = 0; k < polygons.corner_cnt[slot]; k++) {
      double p[3];
      placement.Apply(polygons.corner[slot][k], p);
      points_.insert(points_.end(), p, p + 3);
    }
    offsets_.push_back(static_cast<int>(points_.size() / 3));
  };
  // LI unfold_faces: bodies[j] is bodies[j-1] mirrored in its own face m_j, and the corridor takes
  // face m_j on bodies[j]. The mirror fixes that face's points, so they are placed with bodies[j-1]'s
  // map, before the reflection is composed.
  append(slots[0]);
  for (int k = 1; k < last; k++) {
    const int slot = slots[k];
    const int begin = offsets_.back();
    append(slot);
    const int end = offsets_.back();
    double n[3];
    placement.Rotate(normals.normal[slot], n);
    double c = 0.0;  // the plane offset, averaged over the face's placed corners
    for (int v = begin; v < end; v++) {
      c += Dot3(n, &points_[3 * static_cast<size_t>(v)]);
    }
    c /= (end - begin);
    placement.Reflect(n, c);
  }
  append(slots[last]);
  for (int i = 0; i < 3; i++) {
    entry_normal_[i] = normals.normal[slots[0]][i];
  }
  placement.Rotate(normals.normal[slots[last]], exit_normal_);
  // LI area_eps. A crystal edge is a polygon edge, so the shortest one is the table's.
  eps_ = kEntryMeasureEpsRel * polygons.min_edge_length * polygons.min_edge_length;
}

EntryMeasure Corridor::Evaluate(const double s_body[3], double refractive_index, CorridorDiagnostics* diagnostics) {
  EntryMeasure m;
  if (diagnostics) {
    *diagnostics = {};
    diagnostics->area_threshold = eps_;
  }
  // Entry gate: the incident side has no critical angle (LI entry_ok with cos_tc = 0, and cos_i > 0).
  const double cos_i = -Dot3(entry_normal_, s_body);
  if (!(cos_i > 0.0)) {
    m.status = EntryMeasureStatus::kEntryBackface;
    return m;
  }
  // LI refract_into_crystal.
  const double eta = 1.0 / refractive_index;
  const double discriminant = 1.0 - eta * eta * (1.0 - cos_i * cos_i);
  const double k = eta * cos_i - std::sqrt(discriminant);
  double d_in[3];
  for (int i = 0; i < 3; i++) {
    d_in[i] = eta * s_body[i] + k * entry_normal_[i];
  }
  const double cos_t = -Dot3(entry_normal_, d_in);
  // Exit gate (LI exit_ok): d_in . n~_b >= cos(theta_c), with theta_c from the call's index, the one
  // the ray actually crosses (as in LI).
  const double cos_critical = std::sqrt(1.0 - 1.0 / (refractive_index * refractive_index));
  if (!(Dot3(d_in, exit_normal_) >= cos_critical)) {
    m.status = EntryMeasureStatus::kExitCriticalAngle;
    return m;
  }

  // LI perp_bases of d_in: (u, w, d) right-handed, helper +z unless |d_z| >= 0.9, then +x.
  const double d_norm = std::sqrt(Dot3(d_in, d_in));
  const double d[3] = { d_in[0] / d_norm, d_in[1] / d_norm, d_in[2] / d_norm };
  const double helper_z[3] = { 0.0, 0.0, 1.0 };
  const double helper_x[3] = { 1.0, 0.0, 0.0 };
  double u[3];
  Cross3(d, std::fabs(d[2]) >= 0.9 ? helper_x : helper_z, u);
  const double u_norm = std::sqrt(Dot3(u, u));
  for (double& x : u) {
    x /= u_norm;
  }
  double w[3];
  Cross3(d, u, w);

  // LI corridor_intersection: project every polygon along d, then clip the first by each later one.
  auto project = [&](int poly, std::vector<double>* out, bool* reversed) {
    out->clear();
    for (int v = offsets_[poly]; v < offsets_[poly + 1]; v++) {
      const double* p = &points_[3 * static_cast<size_t>(v)];
      out->push_back(Dot3(p, u));
      out->push_back(Dot3(p, w));
    }
    const int count = offsets_[poly + 1] - offsets_[poly];
    *reversed = EnsureCcw(out, count);
    return count;
  };
  bool reversed = false;
  int count = project(0, &poly_, &reversed);
  std::vector<CorridorEdgeSource> sources;
  std::vector<CorridorEdgeSource> next_sources;
  if (diagnostics) {
    for (int i = 0; i < count; ++i) {
      sources.push_back({ 0, reversed ? (2 * count - 2 - i) % count : i });
    }
    for (int j = 0; j < 3; ++j) {
      diagnostics->projection_basis[0][j] = u[j];
      diagnostics->projection_basis[1][j] = w[j];
    }
    diagnostics->geometry_evaluated = true;
  }
  const int polygon_count = static_cast<int>(offsets_.size()) - 1;
  for (int poly = 1; poly < polygon_count; poly++) {
    const int clip_count = project(poly, &clip_, &reversed);
    for (int i = 0; i < clip_count; i++) {
      const int j = (i + 1) % clip_count;
      const CorridorEdgeSource source{ poly, reversed ? (2 * clip_count - 2 - i) % clip_count : i };
      count = ClipHalfplane(poly_, count, &clip_[2 * i], &clip_[2 * j], &next_, diagnostics ? &sources : nullptr,
                            source, diagnostics ? &next_sources : nullptr);
      if (diagnostics) {
        sources.swap(next_sources);
      }
      poly_.swap(next_);
    }
  }
  m.area_perp_internal = PolygonArea(poly_, count);
  if (diagnostics) {
    diagnostics->raw_area = m.area_perp_internal;
    diagnostics->edge_sources = std::move(sources);
    for (int i = 0; i < count; ++i) {
      diagnostics->vertices.push_back({ poly_[2 * i], poly_[2 * i + 1] });
    }
  }
  if (m.area_perp_internal <= eps_) {
    m.status = EntryMeasureStatus::kCorridorEmpty;
    return m;
  }
  m.status = EntryMeasureStatus::kOk;
  m.value = m.area_perp_internal * cos_i / cos_t;
  return m;
}

}  // namespace lumice::analytic
