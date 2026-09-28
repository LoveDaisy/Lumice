#include "analytic/path_evaluation.hpp"

#include <cmath>
#include <limits>

#include "core/crystal.hpp"
#include "core/shared/optics_shared.h"

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

// v_W = R v_B with R row-major.
void BodyToWorld(const double r[9], const double v[3], double out[3]) {
  for (int i = 0; i < 3; i++) {
    out[i] = r[i * 3 + 0] * v[0] + r[i * 3 + 1] * v[1] + r[i * 3 + 2] * v[2];
  }
}

// v_B = R^T v_W.
void WorldToBody(const double r[9], const double v[3], double out[3]) {
  for (int i = 0; i < 3; i++) {
    out[i] = r[0 * 3 + i] * v[0] + r[1 * 3 + i] * v[1] + r[2 * 3 + i] * v[2];
  }
}

// Unpolarised reflectance of an interface in HitSurface's variables: `cos_i` the incidence cosine
// (> 0), `rr` the relative index along the ray (n_incident / n_transmitted) and `discriminant` =
// 1 - rr^2 (1 - cos_i^2) = cos_t^2. GetReflectRatio's `delta` is discriminant / cos_i^2
// (optics.cpp HitSurface), clamped at 0 as HitSurface clamps it, which makes a total reflection
// R = 1 exactly.
double Reflectance(double cos_i, double rr, double discriminant) {
  const double delta = discriminant / (cos_i * cos_i);
  return lm_optics::GetReflectRatioT<double>(delta > 0.0 ? delta : 0.0, rr);
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
  const double n = refractive_index;
  const int last = slot_count - 1;
  double* seg = out->segment_directions;
  double* trans = out->interface_transmittances;
  double fresnel = 1.0;

  // Entry: refraction into the crystal, normal toward the incident medium is the outward normal.
  // Margin expressions are LI's _path_domain / refract_smooth, term for term.
  double normal[3];
  BodyToWorld(pose, table.normal[slots[0]], normal);
  const double entry_rr = 1.0 / n;
  const double entry_cos = -Dot3(normal, incident_direction);
  const double entry_disc = 1.0 - entry_rr * entry_rr * (1.0 - entry_cos * entry_cos);
  bool valid = entry_cos > 0.0 && entry_disc > 0.0;  // also false on NaN

  double dir[3];
  if (valid) {
    WorldToBody(pose, incident_direction, seg);
    const double k = entry_rr * entry_cos - std::sqrt(entry_disc);
    for (int i = 0; i < 3; i++) {
      dir[i] = entry_rr * incident_direction[i] + k * normal[i];
    }
    WorldToBody(pose, dir, seg + 3);
    trans[0] = 1.0 - Reflectance(entry_cos, entry_rr, entry_disc);
    fresnel *= trans[0];
  }

  // Internal reflections: the ray must reach each face from inside; TIR or not, it reflects.
  for (int k = 1; valid && k < last; k++) {
    BodyToWorld(pose, table.normal[slots[k]], normal);
    const double cos_i = Dot3(normal, dir);
    const double disc = 1.0 - n * n * (1.0 - cos_i * cos_i);  // = -(LI's internal TIR discriminant)
    if (!(cos_i > 0.0) || !std::isfinite(disc)) {
      valid = false;
      break;
    }
    for (int i = 0; i < 3; i++) {
      dir[i] -= 2.0 * cos_i * normal[i];
    }
    WorldToBody(pose, dir, seg + 3 * (k + 1));
    trans[k] = Reflectance(cos_i, n, disc);
    fresnel *= trans[k];
  }

  // Exit: refraction out of the crystal, normal toward the incident (inner) medium is -outward.
  if (valid) {
    BodyToWorld(pose, table.normal[slots[last]], normal);
    const double exit_cos = Dot3(normal, dir);
    const double exit_disc = 1.0 - n * n * (1.0 - exit_cos * exit_cos);
    valid = exit_cos > 0.0 && exit_disc > 0.0;
    if (valid) {
      const double k = n * exit_cos - std::sqrt(exit_disc);
      for (int i = 0; i < 3; i++) {
        out->outgoing_direction[i] = n * dir[i] - k * normal[i];
      }
      WorldToBody(pose, out->outgoing_direction, seg + 3 * (last + 1));
      trans[last] = 1.0 - Reflectance(exit_cos, n, exit_disc);
      fresnel *= trans[last];
    }
  }

  if (!valid) {
    for (int i = 0; i < 3; i++) {
      out->outgoing_direction[i] = 0.0;
    }
    for (int i = 0; i < 3 * (slot_count + 1); i++) {
      seg[i] = 0.0;
    }
    for (int i = 0; i < slot_count; i++) {
      trans[i] = 0.0;
    }
    fresnel = 0.0;
  }
  out->fresnel_transmission = fresnel;
  return valid;
}

}  // namespace lumice::analytic
