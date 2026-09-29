#include "analytic/path_evaluation.hpp"

#include <cassert>
#include <cmath>
#include <limits>

#include "analytic/path_chain.hpp"
#include "core/crystal.hpp"
#include "core/def.hpp"

namespace lumice::analytic {

static_assert(static_cast<size_t>(kMaxFaceCount) == kMaxHits, "the face-sequence bound is the simulator's");

namespace {

bool AllFinite(const double* v, int n) {
  for (int i = 0; i < n; i++) {
    if (!std::isfinite(v[i])) {
      return false;
    }
  }
  return true;
}

// A double that survives the cast to the engine's float factory arguments.
bool FitsFloat(double v) {
  return std::isfinite(v) && std::fabs(v) <= static_cast<double>(std::numeric_limits<float>::max());
}

double Dot3(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// How far, relative to the crystal's size, a float closed-form corner may sit from a plane it lies
// on: the engine rounds each corner coordinate to float (about 6e-8 relative), and its own vertex
// merge works at 5 float epsilons of scale (geo3d_closedform.cpp GapToleranceForScale).
constexpr double kCornerPlaneTolerance = 1e-5;

double Det3(const double a[3], const double b[3], const double c[3]) {
  return a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) + a[2] * (b[0] * c[1] - b[1] * c[0]);
}

// A face-polygon corner in double: the engine's float corner decides which planes meet there (the
// face `slot` itself and every present face whose plane passes within `tolerance`), and the corner
// is the intersection of the best-conditioned triple of them (the largest |det| of normals that
// includes `slot`), solved in double from the double normals and `offsets`. Topology stays the
// engine's float decision (doc/analytic-api.md section 4.1); only the coordinates are refined, from
// about 1e-7 relative to the double rounding of the closed form. A corner with fewer than three
// independent planes (no such corner exists on a closed-form crystal) keeps the float value.
void RefineCorner(const FaceNormalTable& table, const double* offsets, int slot, double tolerance,
                  const double corner[3], double out[3]) {
  int on[kMaxFaceSlots];
  int on_count = 0;
  for (int t = 0; t < table.slot_cnt; t++) {
    if (t != slot && table.present[t] && std::fabs(Dot3(table.normal[t], corner) - offsets[t]) <= tolerance) {
      on[on_count++] = t;
    }
  }
  double best = 0.0;
  int best_i = -1;
  int best_j = -1;
  for (int i = 0; i < on_count; i++) {
    for (int j = i + 1; j < on_count; j++) {
      const double d = std::fabs(Det3(table.normal[slot], table.normal[on[i]], table.normal[on[j]]));
      if (d > best) {
        best = d;
        best_i = on[i];
        best_j = on[j];
      }
    }
  }
  for (int c = 0; c < 3; c++) {
    out[c] = corner[c];
  }
  if (best_i < 0 || best < 1e-6) {
    return;
  }
  // Cramer's rule on N x = o, rows the three normals.
  const double* n0 = table.normal[slot];
  const double* n1 = table.normal[best_i];
  const double* n2 = table.normal[best_j];
  const double o[3] = { offsets[slot], offsets[best_i], offsets[best_j] };
  const double det = Det3(n0, n1, n2);
  for (int c = 0; c < 3; c++) {
    double col[3][3] = { { n0[0], n0[1], n0[2] }, { n1[0], n1[1], n1[2] }, { n2[0], n2[1], n2[2] } };
    for (int r = 0; r < 3; r++) {
      col[r][c] = o[r];
    }
    out[c] = Det3(col[0], col[1], col[2]) / det;
  }
}

}  // namespace

Status BuildFaceNormals(const CrystalShape& shape, FaceNormalTable* out, FacePolygonTable* polygons) {
  LUMICE_ANALYTIC_Crystal crystal{};
  crystal.kind =
      shape.kind == CrystalShapeKind::kPyramid ? LUMICE_ANALYTIC_CRYSTAL_PYRAMID : LUMICE_ANALYTIC_CRYSTAL_PRISM;
  crystal.height = shape.height;
  for (int i = 0; i < 6; i++) {
    crystal.face_distance[i] = shape.face_distance[i];
  }
  crystal.upper_h = shape.upper_h;
  crystal.lower_h = shape.lower_h;
  crystal.upper_wedge_deg = shape.upper_wedge_deg;
  crystal.lower_wedge_deg = shape.lower_wedge_deg;
  return BuildFaceNormals(crystal, out, polygons);
}

int FaceNormalTable::SlotOf(int fn) const {
  for (int s = 0; s < slot_cnt; s++) {
    if (present[s] && face_number[s] == fn) {
      return s;
    }
  }
  return -1;
}

Status BuildFaceNormals(const LUMICE_ANALYTIC_Crystal& crystal, FaceNormalTable* out, FacePolygonTable* polygons) {
  *out = FaceNormalTable{};
  if (polygons != nullptr) {
    *polygons = FacePolygonTable{};
  }
  const double scalars[] = { crystal.height,           crystal.upper_h,          crystal.lower_h,
                             crystal.upper_wedge_deg,  crystal.lower_wedge_deg,  crystal.face_distance[0],
                             crystal.face_distance[1], crystal.face_distance[2], crystal.face_distance[3],
                             crystal.face_distance[4], crystal.face_distance[5] };
  for (double v : scalars) {
    if (!FitsFloat(v)) {
      return Status::kInvalidValue;
    }
  }
  float dist[6];
  for (int i = 0; i < 6; i++) {
    dist[i] = static_cast<float>(crystal.face_distance[i]);
  }

  Crystal engine;
  double a1 = 0.0;
  double a2 = 0.0;
  if (crystal.kind == LUMICE_ANALYTIC_CRYSTAL_PRISM) {
    if (crystal.upper_h != 0.0 || crystal.lower_h != 0.0 || crystal.upper_wedge_deg != 0.0 ||
        crystal.lower_wedge_deg != 0.0) {
      return Status::kInvalidValue;
    }
    engine = Crystal::CreatePrism(static_cast<float>(std::fabs(crystal.height)), dist);
  } else if (crystal.kind == LUMICE_ANALYTIC_CRYSTAL_PYRAMID) {
    engine = Crystal::CreatePyramid(
        static_cast<float>(crystal.upper_wedge_deg), static_cast<float>(crystal.lower_wedge_deg),
        static_cast<float>(std::fabs(crystal.upper_h)), static_cast<float>(std::fabs(crystal.height)),
        static_cast<float>(std::fabs(crystal.lower_h)), dist);
    // Only read for the cone slots, and only when the engine kept that cone — which already implies
    // a legal wedge, so tan() here is finite and non-zero.
    a1 = ClosedFormConeSlopeFromWedgeDeg(crystal.upper_wedge_deg);
    a2 = ClosedFormConeSlopeFromWedgeDeg(crystal.lower_wedge_deg);
  } else {
    return Status::kInvalidValue;
  }

  const CrystalGeom& g = engine.CfGeom();
  if (g.face_cnt == 0) {
    return Status::kInvalidConfig;
  }
  out->slot_cnt = g.face_cnt;
  for (int s = 0; s < g.face_cnt; s++) {
    out->face_number[s] = g.face_number[s];
    out->present[s] = g.face_present[s];
    if (!g.face_present[s]) {
      continue;
    }
    // Coupled derivations: the simulator's prism takes these slots' normals from the kHexFace*
    // direction tables (geo3d_closedform.cpp), not from this plane table. The two agree to within
    // an ulp, and test_path_evaluation.cpp holds the result to the star directions at 1e-15 — a
    // change to either derivation must keep that test green (doc/analytic-api.md section 5.4).
    double plane[4];
    ClosedFormHexFacePlane(s, a1, a2, 0.0, 0.0, plane);
    const double mag = std::sqrt(plane[0] * plane[0] + plane[1] * plane[1] + plane[2] * plane[2]);
    for (int k = 0; k < 3; k++) {
      out->normal[s][k] = plane[k] / mag;
    }
  }
  if (polygons != nullptr) {
    static_assert(kMaxFaceCorners == kCrystalGeomMaxVtxPerFace, "one face polygon must fit the engine's layout");
    // The plane offsets o_s (n_s . x <= o_s) in double from the caller's double inputs, by the
    // engine's own closed-form formulas: side and cone faces by ClosedFormHexFacePlane, the basal
    // cut by ClosedFormPyramidBasalHeights (the prism's is z = +-h/2, ComputeClosedFormPrism).
    double offsets[kMaxFaceSlots]{};
    const double height = std::fabs(crystal.height);
    double z_top = 0.5 * height;
    double z_bot = -0.5 * height;
    if (crystal.kind == LUMICE_ANALYTIC_CRYSTAL_PYRAMID) {
      ClosedFormPyramidBasalHeights(crystal.upper_wedge_deg, crystal.lower_wedge_deg, std::fabs(crystal.upper_h),
                                    height, std::fabs(crystal.lower_h), crystal.face_distance, &z_top, &z_bot);
    }
    double scale = 0.0;
    for (int s = 0; s < g.face_cnt; s++) {
      if (!g.face_present[s]) {
        continue;
      }
      if (s == 0) {
        offsets[s] = z_top;
      } else if (s == 1) {
        offsets[s] = -z_bot;
      } else {
        double plane[4];
        ClosedFormHexFacePlane(s, a1, a2, 0.5 * height, crystal.face_distance[(s - 2) % 6], plane);
        offsets[s] = -plane[3] / std::sqrt(plane[0] * plane[0] + plane[1] * plane[1] + plane[2] * plane[2]);
      }
      for (int k = 0; k < g.face_vtx_cnt[s]; k++) {
        for (int c = 0; c < 3; c++) {
          scale = std::fmax(scale, std::fabs(g.face_vtx[(s * kCrystalGeomMaxVtxPerFace + k) * 3 + c]));
        }
      }
    }
    // A float corner lies on its planes to a few float ulps of the crystal's size.
    const double corner_tolerance = kCornerPlaneTolerance * scale;
    double min_edge = std::numeric_limits<double>::infinity();
    for (int s = 0; s < g.face_cnt; s++) {
      if (!g.face_present[s]) {
        continue;
      }
      const int cnt = g.face_vtx_cnt[s];
      polygons->corner_cnt[s] = cnt;
      for (int k = 0; k < cnt; k++) {
        double corner[3];
        for (int c = 0; c < 3; c++) {
          corner[c] = g.face_vtx[(s * kCrystalGeomMaxVtxPerFace + k) * 3 + c];
        }
        RefineCorner(*out, offsets, s, corner_tolerance, corner, polygons->corner[s][k]);
      }
      for (int k = 0; k < cnt; k++) {
        const double* p = polygons->corner[s][k];
        const double* q = polygons->corner[s][(k + 1) % cnt];
        const double e[3] = { q[0] - p[0], q[1] - p[1], q[2] - p[2] };
        min_edge = std::fmin(min_edge, std::sqrt(Dot3(e, e)));
      }
    }
    polygons->min_edge_length = min_edge;
  }
  return Status::kOk;
}

Status ResolveFaceSequence(const FaceNormalTable& table, const int* faces, int face_count, int* slots_out) {
  if (face_count < 2) {
    return Status::kInvalidValue;
  }
  for (int k = 0; k < face_count; k++) {
    const int slot = table.SlotOf(faces[k]);
    if (slot < 0) {
      return Status::kInvalidValue;
    }
    slots_out[k] = slot;
  }
  return Status::kOk;
}

bool ValidateUnitVector(const double v[3]) {
  if (!AllFinite(v, 3)) {
    return false;
  }
  return std::fabs(std::sqrt(Dot3(v, v)) - 1.0) <= kUnitTolerance;
}

bool ValidateRotation(const double r[9]) {
  if (!AllFinite(r, 9)) {
    return false;
  }
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      // (R^T R)_ij = column i . column j
      double m = r[0 * 3 + i] * r[0 * 3 + j] + r[1 * 3 + i] * r[1 * 3 + j] + r[2 * 3 + i] * r[2 * 3 + j];
      if (std::fabs(m - (i == j ? 1.0 : 0.0)) > kRotationTolerance) {
        return false;
      }
    }
  }
  const double det =
      r[0] * (r[4] * r[8] - r[5] * r[7]) - r[1] * (r[3] * r[8] - r[5] * r[6]) + r[2] * (r[3] * r[7] - r[4] * r[6]);
  return det > 0.0;
}

bool EvaluatePath(const FaceNormalTable& table, const int* slots, int slot_count, double refractive_index,
                  const double incident_direction[3], const double pose[9], PathOutputs* out) {
  assert(slot_count >= 2);
  for (int k = 0; k < slot_count; k++) {
    assert(slots[k] >= 0 && slots[k] < table.slot_cnt && table.present[slots[k]]);
  }
  const bool valid = TracePathChain<double>(table, slots, slot_count, refractive_index, incident_direction, pose,
                                            out->outgoing_direction, out, nullptr);
  if (!valid) {
    for (int i = 0; i < 3; i++) {
      out->outgoing_direction[i] = 0.0;
    }
    for (int i = 0; i < 3 * (slot_count + 1); i++) {
      out->segment_directions[i] = 0.0;
    }
    for (int i = 0; i < slot_count; i++) {
      out->interface_transmittances[i] = 0.0;
    }
    out->fresnel_transmission = 0.0;
  }
  return valid;
}

int BranchMarginCount(int face_count) {
  return face_count + 2;
}

std::string BranchMarginName(int index, int face_count) {
  const int last = BranchMarginCount(face_count) - 1;
  assert(index >= 0 && index <= last);
  if (index == 0) {
    return "entry_incidence_cosine";
  }
  if (index == 1) {
    return "entry_snell_discriminant";
  }
  if (index == last - 1) {
    return "exit_incidence_cosine";
  }
  if (index == last) {
    return "exit_snell_discriminant";
  }
  return "internal_" + std::to_string(index - 1) + "_incidence_cosine";
}

}  // namespace lumice::analytic
