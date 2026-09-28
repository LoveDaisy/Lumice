#include "analytic/path_evaluation.hpp"

#include <cassert>
#include <cmath>
#include <limits>

#include "analytic/path_chain.hpp"
#include "core/crystal.hpp"

namespace lumice::analytic {

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

}  // namespace

int FaceNormalTable::SlotOf(int fn) const {
  for (int s = 0; s < slot_cnt; s++) {
    if (present[s] && face_number[s] == fn) {
      return s;
    }
  }
  return -1;
}

Status BuildFaceNormals(const LUMICE_ANALYTIC_Crystal& crystal, FaceNormalTable* out) {
  *out = FaceNormalTable{};
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

}  // namespace lumice::analytic
