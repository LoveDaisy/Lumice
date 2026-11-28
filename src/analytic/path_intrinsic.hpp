#ifndef LUMICE_ANALYTIC_PATH_INTRINSIC_HPP_
#define LUMICE_ANALYTIC_PATH_INTRINSIC_HPP_

#include <array>
#include <memory>
#include <optional>
#include <vector>

#include "analytic/dp_weight_kink.hpp"

namespace lumice::analytic {

// Cached algebra of a literal path. All decisions use cosines, never wedge_angle.
struct PathDecomposition {
  std::array<double, 9> fold_matrix{};  // row-major M, from FoldMatrixOf via BuildFoldScreen
  std::array<double, 3> entry_normal{};
  std::array<double, 3> unfolded_exit_normal{};  // M^T n_b
  double fold_dot = 0.0;                         // n_a . M^T n_b, from BuildFoldScreen
  // acos(-fold_dot), radians, reporting only. Near 0/pi its absolute accuracy degrades to
  // O(sqrt(cosine_tolerance)); predicates never compare this reported angle.
  double wedge_angle = 0.0;
  bool slab = false;
  bool identity = false;  // slab and proper M with rotation cosine >= 1 - cosine_tolerance
  // For an improper M, the authority's axis is its -1 eigenvector (the +1 axis of -M).
  // Empty near either endpoint of the rotation cosine of det(M) M, including mirrors.
  std::optional<std::array<double, 3>> fixed_axis;
  double cosine_tolerance = 0.0;
};

// Both the consumer loop (including along-edge extrema) and rich rings/pieces/corners. A refusal
// retains the authority's status/message and empty data, never a partially certified boundary.
struct IntrinsicBoundary {
  WalkResult result;
  BoundaryWalkRecord record;
};

// One owned (shape, literal faces, index) value, independent of sun and pose density. Geometry and
// algebra are built once; Boundary()/Kinks() cache the existing walks at their default settings on
// first use. No terrain search is performed. Invalid construction inputs throw std::invalid_argument.
//
// Copies share only immutable geometry; their fields, corridor scratch and cached walk results are
// independent values. Moving preserves the geometry addresses borrowed by DeviationField. As with
// DeviationField, concurrent calls on ONE value require external synchronization (clipping scratch
// and lazy caches); separate copies may be used concurrently. References remain valid until the
// value is assigned or destroyed.
class PathIntrinsic {
 public:
  PathIntrinsic(const CrystalShape& shape, const std::vector<int>& faces, double refractive_index);

  const std::vector<int>& faces() const;
  const PathDecomposition& decomposition() const { return decomposition_; }
  const DeviationField& field() const { return field_; }

  // Unit body-frame source direction u; incident propagation is -u. These forward the field's
  // existing FieldSample and FieldJet without changing gates, closure conventions or derivatives.
  FieldSample EvaluateU(const double u[3]) const;
  FieldJet DifferentiateU(const double u[3]) const;
  const IntrinsicBoundary& Boundary() const;
  const std::vector<KinkCurve>& Kinks() const;

  // A_P and recovery use the same cached corridor. Diagnostics passed to recovery must come from
  // EvaluateAreaU on this path and direction, just as Corridor's own recovery contract requires.
  EntryMeasure EvaluateAreaU(const double u[3], CorridorDiagnostics* diagnostics = nullptr) const;
  RecoveryStatus RecoverRepresentativePath(const CorridorDiagnostics& diagnostics, RepresentativePath* out) const;

 private:
  struct Geometry;
  static std::shared_ptr<const Geometry> BuildGeometry(const CrystalShape& shape, const std::vector<int>& faces,
                                                       double refractive_index);
  PathIntrinsic(std::shared_ptr<const Geometry> geometry, double refractive_index);

  std::shared_ptr<const Geometry> geometry_;
  DeviationField field_;
  mutable Corridor corridor_;
  PathDecomposition decomposition_;
  mutable std::optional<IntrinsicBoundary> boundary_;
  mutable std::optional<std::vector<KinkCurve>> kinks_;
};

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_PATH_INTRINSIC_HPP_
