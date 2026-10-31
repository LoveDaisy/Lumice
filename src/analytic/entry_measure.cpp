#include "analytic/entry_measure.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "analytic/so3.hpp"

namespace lumice::analytic {

namespace {

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
    const double nt = so3::Dot3(n, t);
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
      real_points_.insert(real_points_.end(), polygons.corner[slot][k], polygons.corner[slot][k] + 3);
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
      c += so3::Dot3(n, &points_[3 * static_cast<size_t>(v)]);
    }
    c /= (end - begin);
    plane_normals_.insert(plane_normals_.end(), n, n + 3);
    plane_offsets_.push_back(c);
    placement.Reflect(n, c);
  }
  append(slots[last]);
  for (int i = 0; i < 3; i++) {
    entry_normal_[i] = normals.normal[slots[0]][i];
  }
  placement.Rotate(normals.normal[slots[last]], exit_normal_);
  // LI area_eps. A crystal edge is a polygon edge, so the shortest one is the table's.
  eps_ = kEntryMeasureEpsRel * polygons.min_edge_length * polygons.min_edge_length;

  // The recovery's record, over the polygons just placed: each polygon's plane in the unfolded
  // frame (polygon 0 is the entry face, placed by the identity), the real face's plane it folds
  // back to, and — once, over every present face — the crystal's own half-spaces the recovered
  // path is contained by.
  plane_normals_.insert(plane_normals_.begin(), entry_normal_, entry_normal_ + 3);
  double entry_offset = 0.0;
  for (int v = offsets_[0]; v < offsets_[1]; v++) {
    entry_offset += so3::Dot3(entry_normal_, &points_[3 * static_cast<size_t>(v)]);
  }
  entry_offset /= offsets_[1] - offsets_[0];
  plane_offsets_.insert(plane_offsets_.begin(), entry_offset);
  double exit_plane[3];
  placement.Rotate(normals.normal[slots[last]], exit_plane);
  plane_normals_.insert(plane_normals_.end(), exit_plane, exit_plane + 3);
  double exit_offset = 0.0;
  for (int v = offsets_[last]; v < offsets_[last + 1]; v++) {
    exit_offset += so3::Dot3(exit_plane, &points_[3 * static_cast<size_t>(v)]);
  }
  exit_offset /= offsets_[last + 1] - offsets_[last];
  plane_offsets_.push_back(exit_offset);
  for (int k = 0; k < slot_count; k++) {
    const int slot = slots[k];
    real_normals_.insert(real_normals_.end(), normals.normal[slot], normals.normal[slot] + 3);
    double offset = 0.0;
    const int count = offsets_[k + 1] - offsets_[k];
    for (int v = 0; v < count; v++) {
      offset += so3::Dot3(normals.normal[slot], &real_points_[3 * static_cast<size_t>(offsets_[k] + v)]);
    }
    real_offsets_.push_back(offset / count);
  }
  for (int slot = 0; slot < normals.slot_cnt; slot++) {
    if (!normals.present[slot]) {
      continue;
    }
    crystal_normals_.insert(crystal_normals_.end(), normals.normal[slot], normals.normal[slot] + 3);
    double offset = 0.0;
    for (int v = 0; v < polygons.corner_cnt[slot]; v++) {
      offset += so3::Dot3(normals.normal[slot], polygons.corner[slot][v]);
    }
    crystal_offsets_.push_back(offset / polygons.corner_cnt[slot]);
  }
  min_edge_ = polygons.min_edge_length;
}

EntryMeasure Corridor::Evaluate(const double s_body[3], double refractive_index, CorridorDiagnostics* diagnostics) {
  EntryMeasure m;
  if (diagnostics) {
    *diagnostics = {};
    diagnostics->area_threshold = eps_;
  }
  // Entry gate: the incident side has no critical angle (LI entry_ok with cos_tc = 0, and cos_i > 0).
  const double cos_i = -so3::Dot3(entry_normal_, s_body);
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
  const double cos_t = -so3::Dot3(entry_normal_, d_in);
  // Exit gate (LI exit_ok): d_in . n~_b >= cos(theta_c), with theta_c from the call's index, the one
  // the ray actually crosses (as in LI).
  const double cos_critical = std::sqrt(1.0 - 1.0 / (refractive_index * refractive_index));
  if (!(so3::Dot3(d_in, exit_normal_) >= cos_critical)) {
    m.status = EntryMeasureStatus::kExitCriticalAngle;
    return m;
  }

  // LI perp_bases of d_in: (u, w, d) right-handed, helper +z unless |d_z| >= 0.9, then +x.
  const double d_norm = std::sqrt(so3::Dot3(d_in, d_in));
  const double d[3] = { d_in[0] / d_norm, d_in[1] / d_norm, d_in[2] / d_norm };
  const double helper_z[3] = { 0.0, 0.0, 1.0 };
  const double helper_x[3] = { 1.0, 0.0, 0.0 };
  double u[3];
  so3::Cross3(d, std::fabs(d[2]) >= 0.9 ? helper_x : helper_z, u);
  const double u_norm = std::sqrt(so3::Dot3(u, u));
  for (double& x : u) {
    x /= u_norm;
  }
  double w[3];
  so3::Cross3(d, u, w);

  // LI corridor_intersection: project every polygon along d, then clip the first by each later one.
  auto project = [&](int poly, std::vector<double>* out, bool* reversed) {
    out->clear();
    for (int v = offsets_[poly]; v < offsets_[poly + 1]; v++) {
      const double* p = &points_[3 * static_cast<size_t>(v)];
      out->push_back(so3::Dot3(p, u));
      out->push_back(so3::Dot3(p, w));
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

namespace {

// The recovery's tolerances, scale-relative to the crystal (doc/geometry/numerical-robustness.md
// #2): a length bound derived from the same min_edge_length the area gate's eps is, sitting above
// the float closed-form corners' own planarity noise (the geometry both the corridor and the
// verification read — per-corner ~FLT_EPSILON * size, so margins down to ~1e-7 * edge are still
// float-consistent) and orders of magnitude below the feature sizes the area gate resolves. What
// it must catch — a wrong fold-back transform, a wrong face order, a grazing breakdown — violates
// by the distance to a wrong plane or polygon, i.e. by crystal-scale amounts, so the bound keeps
// its detection power with room to spare.
constexpr double kRecoverTolRel = 1e-6;

// Reflection of x in the plane (n unit, offset c): the fold-back's one primitive, applied with the
// corridor's own recorded planes.
void ReflectInPlane(const double n[3], double c, double x[3]) {
  const double d = so3::Dot3(n, x) - c;
  for (int i = 0; i < 3; i++) {
    x[i] -= 2.0 * d * n[i];
  }
}

// The signed distance of a point (already on the polygon's plane) to the nearest edge line of the
// convex corner ring `pts` (3 per corner, `count` corners): positive inside. The winding reference
// is the ring's own first triple against the plane normal, so no caller-side orientation rule
// exists to get wrong. A degenerate (zero-length) edge makes the ring unusable and is reported as
// an unlimited negative margin — the caller refuses named.
double ConvexPolygonMargin(const double* pts, int count, const double n[3], const double q[3]) {
  if (count < 3) {
    return -1.0;
  }
  double e0[3] = { pts[3] - pts[0], pts[4] - pts[1], pts[5] - pts[2] };
  double v0[3] = { pts[6] - pts[0], pts[7] - pts[1], pts[8] - pts[2] };
  double ref[3];
  so3::Cross3(e0, v0, ref);
  const double ref_sign = so3::Dot3(ref, n) >= 0.0 ? 1.0 : -1.0;
  double margin = std::numeric_limits<double>::infinity();
  for (int i = 0; i < count; i++) {
    const double* a = pts + 3 * i;
    const double* b = pts + 3 * ((i + 1) % count);
    const double e[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] };
    const double v[3] = { q[0] - a[0], q[1] - a[1], q[2] - a[2] };
    const double edge_length = std::sqrt(so3::Dot3(e, e));
    if (!(edge_length > 0.0)) {
      return -1.0;
    }
    double c[3];
    so3::Cross3(e, v, c);
    // Cross-per-edge-length: the point's signed distance to this edge's line, in the ring's own
    // winding direction.
    margin = std::min(margin, ref_sign * so3::Dot3(c, n) / edge_length);
  }
  return margin;
}

}  // namespace

const char* RecoveryStatusName(RecoveryStatus status) {
  switch (status) {
    case RecoveryStatus::kOk:
      return "ok";
    case RecoveryStatus::kNotEvaluated:
      return "not_evaluated";
    case RecoveryStatus::kDegenerate:
      return "degenerate_corridor";
  }
  return "unknown";
}

const char* EntryMeasureStatusName(EntryMeasureStatus status) {
  switch (status) {
    case EntryMeasureStatus::kOk:
      return "ok";
    case EntryMeasureStatus::kEntryBackface:
      return "entry_backface";
    case EntryMeasureStatus::kExitCriticalAngle:
      return "exit_critical_angle";
    case EntryMeasureStatus::kCorridorEmpty:
      return "corridor_empty";
  }
  return "unknown";
}

RecoveryStatus Corridor::RecoverRepresentativePath(const CorridorDiagnostics& diagnostics,
                                                   RepresentativePath* out) const {
  *out = {};
  const int polygon_count = static_cast<int>(offsets_.size()) - 1;
  // The diagnostics are the intersection this corridor evaluated: without its geometry there is
  // nothing to recover, and a corridor the area gate refused is not a legal one even though the
  // clipped ring's vertices are still recorded. Both gates are the corridor's own.
  if (!diagnostics.geometry_evaluated || diagnostics.vertices.size() < 3 ||
      diagnostics.vertices.size() != diagnostics.edge_sources.size() ||
      !(diagnostics.raw_area > diagnostics.area_threshold)) {
    return RecoveryStatus::kNotEvaluated;
  }

  // The internal direction from the projection basis (u, w): Evaluate built them orthonormal with
  // w = d x u, so d = u x w is the normalized internal direction the intersection was cut along.
  const std::array<double, 3>& u = diagnostics.projection_basis[0];
  const std::array<double, 3>& w = diagnostics.projection_basis[1];
  double d[3];
  so3::Cross3(u.data(), w.data(), d);

  // The interior point: the intersection polygon is convex (clipping a convex ring keeps it
  // convex), so its vertex average is interior. With the internal direction it defines the one
  // straight line of the unfolded picture.
  double c_u = 0.0;
  double c_w = 0.0;
  for (const std::array<double, 2>& v : diagnostics.vertices) {
    c_u += v[0];
    c_w += v[1];
  }
  c_u /= static_cast<double>(diagnostics.vertices.size());
  c_w /= static_cast<double>(diagnostics.vertices.size());
  double origin[3];
  for (int i = 0; i < 3; i++) {
    origin[i] = c_u * u[i] + c_w * w[i];
  }

  // The unfolded line meets every ghost plane in order; a non-increasing or unbounded meeting
  // (a plane parallel to the line) is a named refusal, not a clamp.
  double t[kMaxFaceCount];
  for (int k = 0; k < polygon_count; k++) {
    const double* n = &plane_normals_[3 * static_cast<size_t>(k)];
    const double denom = so3::Dot3(n, d);
    if (!(std::fabs(denom) > 0.0)) {
      return RecoveryStatus::kDegenerate;
    }
    t[k] = (plane_offsets_[k] - so3::Dot3(n, origin)) / denom;
    if (k > 0 && !(t[k] > t[k - 1])) {
      return RecoveryStatus::kDegenerate;
    }
  }

  // Fold each ghost intersection back through the reflection chain: the polygon of face k was
  // placed with the map P_{k-1} = rho_{k-1} o ... o rho_1 (the polygons' own planes), so the
  // original point is P_{k-1}^{-1}(x_k) — the reflections applied last-placed first.
  double pts[kMaxFaceCount][3];
  for (int k = 0; k < polygon_count; k++) {
    for (int i = 0; i < 3; i++) {
      pts[k][i] = origin[i] + t[k] * d[i];
    }
    for (int j = k - 1; j >= 1; j--) {
      ReflectInPlane(&plane_normals_[3 * static_cast<size_t>(j)], plane_offsets_[j], pts[k]);
    }
  }

  // Independent verification on the original crystal — the same checks the unit tests run, run
  // here so a numerically degenerate corridor refuses named instead of emitting a trajectory.
  const double tol = kRecoverTolRel * min_edge_;
  for (int k = 0; k < polygon_count; k++) {
    // I0 plane membership: the folded point sits on its original finite face's plane.
    const double plane_residual =
        std::fabs(so3::Dot3(&real_normals_[3 * static_cast<size_t>(k)], pts[k]) - real_offsets_[k]);
    if (!(plane_residual <= tol)) {
      return RecoveryStatus::kDegenerate;
    }
    // I1 finite-face containment: inside the original polygon, in the polygon's own margins.
    const double* ring = &real_points_[3 * static_cast<size_t>(offsets_[k])];
    const int count = offsets_[k + 1] - offsets_[k];
    if (ConvexPolygonMargin(ring, count, &real_normals_[3 * static_cast<size_t>(k)], pts[k]) < -tol) {
      return RecoveryStatus::kDegenerate;
    }
  }
  // I2 in-crystal propagation: every leg's midpoint satisfies every present face's inward
  // half-space (the convex-crystal containment test, walked over the whole crystal, not just the
  // sequence's faces).
  for (int k = 0; k + 1 < polygon_count; k++) {
    const double mid[3] = { 0.5 * (pts[k][0] + pts[k + 1][0]), 0.5 * (pts[k][1] + pts[k + 1][1]),
                            0.5 * (pts[k][2] + pts[k + 1][2]) };
    const size_t face_count = crystal_offsets_.size();
    for (size_t f = 0; f < face_count; f++) {
      if (so3::Dot3(&crystal_normals_[3 * f], mid) - crystal_offsets_[f] > tol) {
        return RecoveryStatus::kDegenerate;
      }
    }
  }

  for (int k = 0; k < polygon_count; k++) {
    for (int i = 0; i < 3; i++) {
      out->contacts[k][i] = pts[k][i];
    }
  }
  out->contact_count = polygon_count;
  return RecoveryStatus::kOk;
}

}  // namespace lumice::analytic
