#include "analytic/external_reflection.hpp"

#include <cmath>

#include "core/shared/optics_shared.h"

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

double Norm3(const double v[3]) {
  return std::sqrt(Dot3(v, v));
}

}  // namespace

ExternalReflectionFace::ExternalReflectionFace(const FaceNormalTable& normals, const FacePolygonTable& polygons,
                                               int slot)
    : corner_count_(polygons.corner_cnt[slot]) {
  for (int component = 0; component < 3; ++component) {
    normal_[component] = normals.normal[slot][component];
  }
  if (corner_count_ < 3) {
    return;
  }
  // Match the product geometry's fan triangulation. This remains well-defined even if a future
  // closed-form corner set is slightly non-planar near a degenerate shape boundary.
  const double* origin = polygons.corner[slot][0];
  for (int corner = 1; corner + 1 < corner_count_; ++corner) {
    double edge0[3];
    double edge1[3];
    for (int component = 0; component < 3; ++component) {
      edge0[component] = polygons.corner[slot][corner][component] - origin[component];
      edge1[component] = polygons.corner[slot][corner + 1][component] - origin[component];
    }
    double cross[3];
    Cross3(edge0, edge1, cross);
    area_ += 0.5 * Norm3(cross);
  }
}

ExternalReflectionResult ExternalReflectionFace::Evaluate(const double incident_body[3],
                                                          double refractive_index) const {
  ExternalReflectionResult out;
  out.topology_signature = static_cast<uint64_t>(corner_count_);
  out.incidence_cosine = -Dot3(normal_, incident_body);
  const double relative_index = 1.0 / refractive_index;
  out.tir_discriminant = 1.0 - relative_index * relative_index * (1.0 - out.incidence_cosine * out.incidence_cosine);
  if (!std::isfinite(area_) || !std::isfinite(out.incidence_cosine) || !std::isfinite(out.tir_discriminant) ||
      !std::isfinite(relative_index)) {
    return out;
  }
  if (!(out.incidence_cosine > 0.0)) {
    out.status = ExternalReflectionStatus::kEntryBackface;
    return out;
  }
  if (!(area_ > 0.0)) {
    out.status = ExternalReflectionStatus::kDegenerateFace;
    return out;
  }
  const double incident_dot_normal = -out.incidence_cosine;
  for (int component = 0; component < 3; ++component) {
    out.outgoing_direction[component] = incident_body[component] - 2.0 * incident_dot_normal * normal_[component];
  }
  out.entry_measure = area_ * out.incidence_cosine;
  const double delta = out.tir_discriminant / (out.incidence_cosine * out.incidence_cosine);
  out.reflectance = lm_optics::GetReflectRatioT<double>(delta > 0.0 ? delta : 0.0, relative_index);
  if (!std::isfinite(out.entry_measure) || !std::isfinite(out.reflectance)) {
    out = ExternalReflectionResult{};
    return out;
  }
  out.status = ExternalReflectionStatus::kOk;
  return out;
}

}  // namespace lumice::analytic
