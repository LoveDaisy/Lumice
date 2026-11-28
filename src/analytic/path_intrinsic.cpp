#include "analytic/path_intrinsic.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace lumice::analytic {

struct PathIntrinsic::Geometry {
  FaceNormalTable normals;
  FacePolygonTable polygons;
  std::vector<int> faces;
  std::vector<int> slots;
};

std::shared_ptr<const PathIntrinsic::Geometry> PathIntrinsic::BuildGeometry(const CrystalShape& shape,
                                                                            const std::vector<int>& faces,
                                                                            double refractive_index) {
  if (faces.empty() || faces.size() > kMaxFaceCount || !std::isfinite(refractive_index) || refractive_index <= 1.0) {
    throw std::invalid_argument("PathIntrinsic requires 1..64 faces and a finite refractive index > 1");
  }
  auto geometry = std::make_shared<Geometry>();
  if (BuildFaceNormals(shape, &geometry->normals, &geometry->polygons) != Status::kOk) {
    throw std::invalid_argument("PathIntrinsic requires a valid crystal shape");
  }
  geometry->faces = faces;
  geometry->slots.resize(faces.size());
  if (ResolveFaceSequence(geometry->normals, faces.data(), static_cast<int>(faces.size()), geometry->slots.data()) !=
      Status::kOk) {
    throw std::invalid_argument("PathIntrinsic requires present literal face numbers");
  }
  return geometry;
}

PathIntrinsic::PathIntrinsic(const CrystalShape& shape, const std::vector<int>& faces, double refractive_index)
    : PathIntrinsic(BuildGeometry(shape, faces, refractive_index), refractive_index) {}

PathIntrinsic::PathIntrinsic(std::shared_ptr<const Geometry> geometry, double refractive_index)
    : geometry_(std::move(geometry)), field_(geometry_->normals, geometry_->polygons, geometry_->slots.data(),
                                             static_cast<int>(geometry_->slots.size()), refractive_index),
      corridor_(geometry_->normals, geometry_->polygons, geometry_->slots.data(),
                static_cast<int>(geometry_->slots.size())) {
  const auto& fold = field_.fold();  // BuildFoldScreen owns the product, slab decision and axis.
  auto& out = decomposition_;
  std::copy_n(fold.fold_matrix, 9, out.fold_matrix.begin());
  std::copy_n(field_.EntryNormal(), 3, out.entry_normal.begin());
  WorldToBody(fold.fold_matrix, geometry_->normals.normal[geometry_->slots.back()], out.unfolded_exit_normal.data());
  out.fold_dot = fold.dot;
  out.slab = fold.degenerate;
  // A Householder product's error grows with the number of evaluated reflections (unit normals,
  // O(1) operands). 64 eps per interface bounds the short dot/product chains with ample headroom
  // over 10 eps * kappa_eval. Keep the slab authority's larger cosine budget, not a second test.
  out.cosine_tolerance = std::max(kFoldDotAtol, 64.0 * std::numeric_limits<double>::epsilon() * field_.SlotCount());
  // acos has absolute error O(delta_cos / sqrt(1-cos^2)); near 0/pi the reported angle therefore
  // degrades to O(sqrt(delta_cos)). Clamping prevents rounding outside [-1,1], not a classification.
  out.wedge_angle = std::acos(std::clamp(-fold.dot, -1.0, 1.0));

  // The product's parity is known from the path: one reflection for an external single-face
  // path, otherwise slot_count - 2. The authority reads the same det(M) M rotation for its axis.
  const int reflections = field_.SlotCount() == 1 ? 1 : field_.SlotCount() - 2;
  const double sign = reflections % 2 == 0 ? 1.0 : -1.0;
  const double rotation_cosine = 0.5 * (sign * (fold.fold_matrix[0] + fold.fold_matrix[4] + fold.fold_matrix[8]) - 1.0);
  out.identity = out.slab && sign > 0.0 && rotation_cosine >= 1.0 - out.cosine_tolerance;
  if (fold.has_axis && std::abs(rotation_cosine) < 1.0 - out.cosine_tolerance) {
    out.fixed_axis = std::array<double, 3>{ fold.axis[0], fold.axis[1], fold.axis[2] };
  }
}

const std::vector<int>& PathIntrinsic::faces() const {
  return geometry_->faces;
}

FieldSample PathIntrinsic::EvaluateU(const double u[3]) const {
  return field_.Sample(u);
}

FieldJet PathIntrinsic::DifferentiateU(const double u[3]) const {
  return field_.Differentiate(u);
}

const IntrinsicBoundary& PathIntrinsic::Boundary() const {
  if (!boundary_) {
    IntrinsicBoundary boundary;
    boundary.result = WalkBoundary(field_, BoundaryWalkOptions{}, &boundary.record);
    boundary_ = std::move(boundary);
  }
  return *boundary_;
}

const std::vector<KinkCurve>& PathIntrinsic::Kinks() const {
  if (!kinks_) {
    kinks_ = WeightKinks(field_, KinkOptions{});
  }
  return *kinks_;
}

EntryMeasure PathIntrinsic::EvaluateAreaU(const double u[3], CorridorDiagnostics* diagnostics) const {
  const double incident[3] = { -u[0], -u[1], -u[2] };
  return corridor_.Evaluate(incident, field_.RefractiveIndex(), diagnostics);
}

RecoveryStatus PathIntrinsic::RecoverRepresentativePath(const CorridorDiagnostics& diagnostics,
                                                        RepresentativePath* out) const {
  return corridor_.RecoverRepresentativePath(diagnostics, out);
}

}  // namespace lumice::analytic
