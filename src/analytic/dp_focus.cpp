#include "analytic/dp_focus.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

#include "analytic/discovery.hpp"
#include "analytic/reflection_group.hpp"

namespace lumice::analytic {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

double Deg(double rad) {
  return rad * 180.0 / kPi;
}

double Norm3(const double v[3]) {
  return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

double Dot3(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// angle(a, b) = atan2(|a x b|, a . b), the layer's own form (accurate at 0 and pi).
double AngleBetween(const double a[3], const double b[3]) {
  const double cross[3] = { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
  return std::atan2(Norm3(cross), Dot3(a, b));
}

// The wedge of a face sequence in degrees, atan2 form: the angle of n_a against -M^T n_b with M
// the fold matrix (LI geometry.wedge_angle_deg; FoldMatrixOf is the product's single authority —
// this reads it, it is not a second reflection-product loop). Rank 0 is decided by path_rank.cpp
// against its own copy of this formula at M = I, a frozen module-A adjacent contract.
double WedgeAngleDeg(const FaceNormalTable& table, const int* slots, int slot_count) {
  double m[9];
  FoldMatrixOf(table, slots, slot_count, m);
  const double* na = table.normal[slots[0]];
  const double* nb = table.normal[slots[slot_count - 1]];
  double unfolded[3];
  for (int i = 0; i < 3; i++) {
    unfolded[i] = -(m[0 * 3 + i] * nb[0] + m[1 * 3 + i] * nb[1] + m[2 * 3 + i] * nb[2]);
  }
  return Deg(AngleBetween(na, unfolded));
}

// The smallest validity margin of u, NaN included as "smallest" wherever it sits (a non-finite
// gate is exterior); callers compare against positive thresholds, and every comparison with NaN
// is false, which is the fail-closed answer. Note dp_field's LocateByValidityMargins orders with
// std::min, where a NaN survives only in the lead — this function is the stricter of the two on
// purpose: one NaN gate marks the whole point exterior.
double SmallestGate(const DeviationField& field, const double u[3]) {
  double margins[kMaxFaceCount + 2];
  const int count = field.ValidityMarginsAt(u, margins);
  double smallest = margins[0];
  for (int k = 1; k < count; k++) {
    if (std::isnan(margins[k]) || margins[k] < smallest) {
      smallest = margins[k];  // a NaN anywhere poisons the minimum from its position on
    }
  }
  return smallest;
}

}  // namespace

// ---- the density-side labels ---------------------------------------------------------------------------

ConfinedDimensions ConfinedDimensionsOf(const PoseDensitySpec& spec) {
  // LI confined_dimensions dispatches on the density's type: Haar none, a zenith Gaussian one
  // width, a roll-locked density two. Column and plate are one type there (they differ only in
  // the default mean, which this library takes as a field) — PoseFamily's kColumn/kPlate pair
  // mirrors that, and so does the dispatch below.
  ConfinedDimensions out;
  const double deg2rad = kPi / 180.0;
  if (spec.family == PoseFamily::kColumn || spec.family == PoseFamily::kPlate) {
    out.dimensions = 1;
    out.widths_rad.push_back(spec.zenith_std_deg * deg2rad);
  } else if (spec.family == PoseFamily::kParry || spec.family == PoseFamily::kLowitz) {
    out.dimensions = 2;
    out.widths_rad.push_back(spec.zenith_std_deg * deg2rad);
    out.widths_rad.push_back(spec.roll_std_deg * deg2rad);
  }
  return out;
}

bool FamilyAxis(const PoseDensitySpec& spec, double axis[3]) {
  const double deg2rad = kPi / 180.0;
  if (spec.family == PoseFamily::kColumn || spec.family == PoseFamily::kPlate) {
    // A zenith Gaussian at a pole leaves only the spin about c: e3. Off a pole (a column away
    // from vertical) the c-axis azimuth is free too: no single circle, no label. LI tests the
    // mean against the poles exactly, so does this (a 1e-16-off mean is a column, deliberately).
    if (spec.zenith_mean_deg != 0.0 && spec.zenith_mean_deg != 180.0) {
      return false;
    }
    axis[0] = 0.0;
    axis[1] = 0.0;
    axis[2] = 1.0;
    return true;
  }
  if (spec.family == PoseFamily::kParry || spec.family == PoseFamily::kLowitz) {
    // a = R_0^T e_z = Rz(-rho) Ry(zeta) e_z from the density's means: at a pole this is e3
    // whatever the roll (Lowitz), at Parry's (90, 0) it is body x.
    const double zeta = spec.zenith_mean_deg * deg2rad;
    const double rho = spec.roll_mean_deg * deg2rad;
    axis[0] = std::sin(zeta) * std::cos(rho);
    axis[1] = -std::sin(zeta) * std::sin(rho);
    axis[2] = std::cos(zeta);
    return true;
  }
  return false;  // random: the support is all of SO(3), no circle
}

bool FamilyPinned(const FaceNormalTable& table, const int* slots, int slot_count, const PoseDensitySpec& spec) {
  double axis[3];
  if (!FamilyAxis(spec, axis)) {
    return false;
  }
  if (IsRankZeroPath(table, slots, slot_count)) {
    return false;  // no field at all: the label is point_mass alone (LI family_pinned's rank check)
  }
  if (!(WedgeAngleDeg(table, slots, slot_count) <= kRankZeroWedgeToleranceDeg)) {
    return false;  // the wedge guard: a commuting-with-everything I with wedge 60 deg is not pinned
  }
  double m[9];
  FoldMatrixOf(table, slots, slot_count, m);
  return CommutesWithRotationAbout(m, axis);
}

// ---- the interior Newton search ------------------------------------------------------------------------

std::vector<InteriorCriticalPoint> InteriorCriticalPointsOf(const DeviationField& field,
                                                            const InteriorNewtonOptions& options) {
  // Seeds: the U_P points of the plain Fibonacci lattice (LI lattice_newton_critical_points).
  std::vector<double> seeds;
  for (int i = 0; i < options.lattice_n; i++) {
    double u[3];
    FibonacciAntipodePoint(options.lattice_n, i, u);
    if (InsideUp(field, u)) {
      seeds.push_back(u[0]);
      seeds.push_back(u[1]);
      seeds.push_back(u[2]);
    }
  }
  const int seed_count = static_cast<int>(seeds.size() / 3);
  if (seed_count == 0) {
    return {};
  }

  // Damped tangent-space Newton on grad D_P = 0 (LI _newton_batch's step, unrolled over seeds):
  // delta solves the Riemannian Hessian against the tangent gradient, is capped at max_step_rad,
  // the iterate renormalizes onto the sphere. A singular Hessian hands the seed to the NaN filter
  // the way jnp.linalg.solve's Linf/NaN there does.
  std::vector<double> ends(static_cast<size_t>(seed_count) * 3);
  for (int s = 0; s < seed_count; s++) {
    double u[3] = { seeds[3 * s], seeds[3 * s + 1], seeds[3 * s + 2] };
    for (int it = 0; it < options.iterations; it++) {
      const FieldJet jet = field.Differentiate(u);
      const double g[2] = { Dot3(jet.tangent_basis[0], jet.tangent_gradient),
                            Dot3(jet.tangent_basis[1], jet.tangent_gradient) };
      const double det = jet.hessian[0][0] * jet.hessian[1][1] - jet.hessian[0][1] * jet.hessian[1][0];
      if (!(det != 0.0) || !std::isfinite(det)) {
        u[0] = kNaN;
        break;
      }
      double delta[2] = { (-g[0] * jet.hessian[1][1] + g[1] * jet.hessian[0][1]) / det,
                          (-g[1] * jet.hessian[0][0] + g[0] * jet.hessian[1][0]) / det };
      const double norm = std::sqrt(delta[0] * delta[0] + delta[1] * delta[1]);
      if (!std::isfinite(norm)) {
        u[0] = kNaN;
        break;
      }
      const double scale = std::min(1.0, options.max_step_rad / std::max(norm, 1e-300));
      delta[0] *= scale;
      delta[1] *= scale;
      double moved[3];
      for (int c = 0; c < 3; c++) {
        moved[c] = u[c] + delta[0] * jet.tangent_basis[0][c] + delta[1] * jet.tangent_basis[1][c];
      }
      const double length = Norm3(moved);
      if (!std::isfinite(length) || length == 0.0) {
        u[0] = kNaN;
        break;
      }
      for (int c = 0; c < 3; c++) {
        u[c] = moved[c] / length;
      }
    }
    ends[3 * s] = u[0];
    ends[3 * s + 1] = u[1];
    ends[3 * s + 2] = u[2];
  }

  // The converged filter (LI's): finite, tangent gradient below tol, back inside U_P and off
  // dU_P (every gate above kBoundaryMarginAtol — a critical point of the smooth extension that
  // settled ON the boundary is the walk's loop extremum, not an interior one).
  struct Candidate {
    double position[3];
    double gradient_norm;
  };
  std::vector<Candidate> converged;
  for (int s = 0; s < seed_count; s++) {
    const double* u = &ends[3 * s];
    if (!std::isfinite(u[0]) || !std::isfinite(u[1]) || !std::isfinite(u[2])) {
      continue;
    }
    if (!(SmallestGate(field, u) > kBoundaryMarginAtol)) {
      continue;  // covers "not inside" and "on dU_P" in one comparison, NaN included
    }
    const FieldJet jet = field.Differentiate(u);
    const double norm = Norm3(jet.tangent_gradient);
    if (std::isfinite(norm) && norm < kCriticalGradientTol) {
      converged.push_back({ { u[0], u[1], u[2] }, norm });
    }
  }

  // Merge within kCriticalPointMergeRad, first-seen order (LI's unique loop).
  std::vector<Candidate> unique;
  for (const Candidate& candidate : converged) {
    bool seen = false;
    for (const Candidate& kept : unique) {
      if (AngleBetween(candidate.position, kept.position) <= kCriticalPointMergeRad) {
        seen = true;
        break;
      }
    }
    if (!seen) {
      unique.push_back(candidate);
    }
  }

  // Classify by the Riemannian Hessian's eigenvalues (symmetrized first, as LI symmetrizes: the
  // diagonal needs no symmetrizing, the off-diagonal averages its two entries).
  std::vector<InteriorCriticalPoint> points;
  points.reserve(unique.size());
  for (const Candidate& candidate : unique) {
    const FieldJet jet = field.Differentiate(candidate.position);
    const double a = jet.hessian[0][0];
    const double h01 = 0.5 * (jet.hessian[0][1] + jet.hessian[1][0]);
    const double c = jet.hessian[1][1];
    const double mean = 0.5 * (a + c);
    const double radius = std::sqrt(std::max(0.0, 0.25 * (a - c) * (a - c) + h01 * h01));
    // Ascending, eigvalsh's convention (LI reads the pair as a set; the parity fixtures read
    // values, so the order convention still matches).
    const double eigenvalues[2] = { mean - radius, mean + radius };
    InteriorCriticalPoint point;
    point.position[0] = candidate.position[0];
    point.position[1] = candidate.position[1];
    point.position[2] = candidate.position[2];
    point.value = jet.value;
    point.hessian_eigenvalues[0] = eigenvalues[0];
    point.hessian_eigenvalues[1] = eigenvalues[1];
    point.gradient_norm = candidate.gradient_norm;
    if (std::fabs(eigenvalues[0]) <= kDegenerateEigenvalueTol ||
        std::fabs(eigenvalues[1]) <= kDegenerateEigenvalueTol) {
      point.kind = CriticalKind::kDegenerate;
    } else {
      const int negative = (eigenvalues[0] < 0.0 ? 1 : 0) + (eigenvalues[1] < 0.0 ? 1 : 0);
      point.kind = negative == 0 ? CriticalKind::kMinimum :
                   negative == 1 ? CriticalKind::kSaddle :
                                   CriticalKind::kMaximum;
    }
    points.push_back(point);
  }
  std::sort(points.begin(), points.end(),
            [](const InteriorCriticalPoint& x, const InteriorCriticalPoint& y) { return x.value < y.value; });
  return points;
}

// ---- the onset table ------------------------------------------------------------------------------------

const char* OnsetLocationName(OnsetLocation location) {
  return location == OnsetLocation::kInterior ? "interior" : "boundary";
}

const char* OnsetSourceName(OnsetSource source) {
  switch (source) {
    case OnsetSource::kInteriorMinimum:
      return "interior_minimum";
    case OnsetSource::kInteriorMaximum:
      return "interior_maximum";
    case OnsetSource::kInteriorSaddle:
      return "interior_saddle";
    case OnsetSource::kInteriorDegenerate:
      return "interior_degenerate";
    case OnsetSource::kSlabAxis:
      return "slab_axis";
    case OnsetSource::kSlabCircle:
      return "slab_circle";
    case OnsetSource::kBoundaryExtremum:
      return "boundary_extremum";
    case OnsetSource::kCorner:
      return "corner";
  }
  return "corner";
}

const char* OnsetProfileName(OnsetProfile profile) {
  switch (profile) {
    case OnsetProfile::kFiniteJump:
      return "finite_jump";
    case OnsetProfile::kLogDivergence:
      return "log_divergence";
    case OnsetProfile::kInverseSqrtDivergence:
      return "inverse_sqrt_divergence";
    case OnsetProfile::kConePoint:
      return "cone_point";
    case OnsetProfile::kCrease:
      return "crease";
    case OnsetProfile::kBoundaryOnset:
      return "boundary_onset";
    case OnsetProfile::kDegenerate:
      return "degenerate";
  }
  return "degenerate";
}

bool ProfileIsJacobianFocusing(OnsetProfile profile) {
  return profile == OnsetProfile::kLogDivergence || profile == OnsetProfile::kInverseSqrtDivergence ||
         profile == OnsetProfile::kDegenerate;
}

namespace {

// The onset of a non-slab interior critical point from its Morse kind and eigenvalues (LI
// interior_onset): a minimum or maximum is a finite_jump carrying the measure limit
// 2 pi / sqrt(|det H|), a saddle a log divergence, anything else degenerate.
CriticalOnset InteriorOnset(const InteriorCriticalPoint& point) {
  CriticalOnset onset;
  onset.value = point.value;
  onset.location = OnsetLocation::kInterior;
  onset.gradient_norm = point.gradient_norm;
  switch (point.kind) {
    case CriticalKind::kMinimum:
    case CriticalKind::kMaximum:
      onset.source =
          point.kind == CriticalKind::kMinimum ? OnsetSource::kInteriorMinimum : OnsetSource::kInteriorMaximum;
      onset.profile = OnsetProfile::kFiniteJump;
      onset.has_measure_limit = true;
      onset.measure_limit =
          2.0 * kPi / std::sqrt(std::fabs(point.hessian_eigenvalues[0] * point.hessian_eigenvalues[1]));
      break;
    case CriticalKind::kSaddle:
      onset.source = OnsetSource::kInteriorSaddle;
      onset.profile = OnsetProfile::kLogDivergence;
      break;
    case CriticalKind::kDegenerate:
      onset.source = OnsetSource::kInteriorDegenerate;
      onset.profile = OnsetProfile::kDegenerate;
      break;
  }
  return onset;
}

// |grad D_P| CONE_PROBE_RAD off a slab axis (LI _cone_slope): the probe rides the tangent basis
// of the axis; the tangent gradient of the slab form is what the field layer differentiates.
double ConeSlope(const DeviationField& field, const double axis[3]) {
  double basis[2][3];
  TangentBasis(axis, basis);
  double probe[3];
  for (int c = 0; c < 3; c++) {
    probe[c] = std::cos(kConeProbeRad) * axis[c] + std::sin(kConeProbeRad) * basis[0][c];
  }
  const FieldJet jet = field.Differentiate(probe);
  return Norm3(jet.tangent_gradient);
}

// Where the slab circle u . n_M = 0 lies relative to U_P (LI _circle_location): the best (largest)
// smallest-gate over a dense sampling of the circle decides interior / boundary / exterior.
DomainLocation CircleLocation(const DeviationField& field, const double axis[3], int samples) {
  double basis[2][3];
  TangentBasis(axis, basis);
  double best = -kInf;
  for (int i = 0; i < samples; i++) {
    const double t = 2.0 * kPi * static_cast<double>(i) / static_cast<double>(samples);
    double u[3];
    for (int c = 0; c < 3; c++) {
      u[c] = std::cos(t) * basis[0][c] + std::sin(t) * basis[1][c];
    }
    const double smallest = SmallestGate(field, u);
    if (!std::isfinite(smallest)) {
      continue;  // a non-finite gate is exterior there, dropped from the max (LI isfinite filter)
    }
    best = std::max(best, smallest);
  }
  if (best > kZeroMarginAtol) {
    return DomainLocation::kInterior;
  }
  return best >= -kZeroMarginAtol ? DomainLocation::kBoundary : DomainLocation::kExterior;
}

// The slab onsets (LI _slab_onsets): each non-exterior axis point a cone_point at its D value
// (the slab form — exact where the chain loses sqrt(eps)), the circle an inverse-square-root
// divergence at the rotation's angle (a curve of maxima) or a crease at 0 for a mirror, with the
// gradient norms LI pins analytically (2 on both sides of a mirror crease, 0 on the rotation
// circle — the value diverges, its measure diverges, the slope itself vanishes).
std::vector<CriticalOnset> SlabOnsets(const DeviationField& field, const DegenerateFoldSet& fold_set,
                                      int circle_samples) {
  const double* m = field.fold().fold_matrix;
  const bool mirror = (m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6]) +
                       m[2] * (m[3] * m[7] - m[4] * m[6])) < 0.0;
  std::vector<CriticalOnset> onsets;
  for (int k = 0; k < fold_set.axis_point_count; k++) {
    const auto& axis_point = fold_set.axis_points[k];
    if (axis_point.location == DomainLocation::kExterior) {
      continue;
    }
    CriticalOnset onset;
    onset.value = field.DSlab(axis_point.position);
    onset.location =
        axis_point.location == DomainLocation::kInterior ? OnsetLocation::kInterior : OnsetLocation::kBoundary;
    onset.source = OnsetSource::kSlabAxis;
    onset.profile = OnsetProfile::kConePoint;
    onset.gradient_norm = ConeSlope(field, axis_point.position);
    onsets.push_back(onset);
  }
  if (fold_set.has_axis) {
    const DomainLocation where = CircleLocation(field, fold_set.axis, circle_samples);
    if (where != DomainLocation::kExterior) {
      // Rotation by theta_M: D = theta_M on the circle, a curve of maxima; mirror: D = 0, a crease.
      const double trace = m[0] + m[4] + m[8];
      CriticalOnset onset;
      onset.value = mirror ? 0.0 : std::acos(std::max(-1.0, std::min(1.0, 0.5 * (trace - 1.0))));
      onset.location = where == DomainLocation::kInterior ? OnsetLocation::kInterior : OnsetLocation::kBoundary;
      onset.source = OnsetSource::kSlabCircle;
      onset.profile = mirror ? OnsetProfile::kCrease : OnsetProfile::kInverseSqrtDivergence;
      onset.gradient_norm = mirror ? 2.0 : 0.0;
      onsets.push_back(onset);
    }
  }
  return onsets;
}

// Records of one value (within kExtremumAtol), location, source and profile merged, keeping the
// smallest gradient_norm and summing multiplicities (LI _merged). The sort keys mirror LI's: the
// merge pass runs on (value, location, source, profile), the output order is (value, location,
// source).
std::vector<CriticalOnset> MergeOnsets(std::vector<CriticalOnset> onsets) {
  const auto full_key_less = [](const CriticalOnset& x, const CriticalOnset& y) {
    return std::tie(x.value, x.location, x.source, x.profile) < std::tie(y.value, y.location, y.source, y.profile);
  };
  std::sort(onsets.begin(), onsets.end(), full_key_less);
  std::vector<CriticalOnset> out;
  for (const CriticalOnset& onset : onsets) {
    bool merged = false;
    for (CriticalOnset& kept : out) {
      if (std::fabs(kept.value - onset.value) <= kExtremumAtol && kept.location == onset.location &&
          kept.source == onset.source && kept.profile == onset.profile) {
        kept.gradient_norm = std::min(kept.gradient_norm, onset.gradient_norm);
        kept.multiplicity += onset.multiplicity;
        merged = true;
        break;
      }
    }
    if (!merged) {
      out.push_back(onset);
    }
  }
  std::sort(out.begin(), out.end(), [](const CriticalOnset& x, const CriticalOnset& y) {
    return std::tie(x.value, x.location, x.source) < std::tie(y.value, y.location, y.source);
  });
  return out;
}

}  // namespace

std::vector<CriticalOnset> FieldOnsets(const DeviationField& field, const DegenerateFoldSet* fold_set,
                                       const BoundaryLoopData& loop, const InteriorNewtonOptions& newton) {
  std::vector<CriticalOnset> onsets;
  if (field.fold().degenerate && fold_set != nullptr) {
    onsets = SlabOnsets(field, *fold_set, kFocusingCircleSamples);
  } else if (!field.fold().degenerate) {
    // The Newton set of the non-slab branch; the slab branch's interior points are the fold set's
    // axis members, already in SlabOnsets (their kind is degenerate but their onset is the cone
    // point, LI reports them once, there).
    for (const InteriorCriticalPoint& point : InteriorCriticalPointsOf(field, newton)) {
      onsets.push_back(InteriorOnset(point));
    }
  }

  // Every restricted extremum and every corner of the loop, with the gradient-based profile: a
  // vanishing (finite, <= kBoundaryGradientAtol) gradient is degenerate, anything else — including
  // an unbounded one at an exit-TIR end — is a boundary_onset carrying the norm (infinite there).
  struct BoundaryDatum {
    const double* position;
    double value;
    OnsetSource source;
  };
  std::vector<BoundaryDatum> boundary;
  for (const BoundaryCriticalPoint& point : loop.critical_points) {
    if (!point.corner) {
      boundary.push_back({ point.position, point.value, OnsetSource::kBoundaryExtremum });
    }
  }
  for (const LoopCorner& corner : loop.corners) {
    boundary.push_back({ corner.position, corner.value, OnsetSource::kCorner });
  }
  for (const BoundaryDatum& datum : boundary) {
    const FieldJet jet = field.Differentiate(datum.position);
    const double norm = Norm3(jet.tangent_gradient);
    const bool finite = std::isfinite(norm);
    CriticalOnset onset;
    onset.value = datum.value;
    onset.location = OnsetLocation::kBoundary;
    onset.source = datum.source;
    onset.profile = finite && norm <= kBoundaryGradientAtol ? OnsetProfile::kDegenerate : OnsetProfile::kBoundaryOnset;
    onset.gradient_norm = finite ? norm : kInf;
    onsets.push_back(onset);
  }
  return MergeOnsets(std::move(onsets));
}

bool GradientNormRange(const DeviationField& field, int lattice_n, double out[2]) {
  double smallest = kInf;
  double largest = -kInf;
  for (int i = 0; i < lattice_n; i++) {
    double u[3];
    FibonacciAntipodePoint(lattice_n, i, u);
    if (!InsideUp(field, u)) {
      continue;
    }
    const FieldJet jet = field.Differentiate(u);
    const double norm = Norm3(jet.tangent_gradient);
    if (!std::isfinite(norm)) {
      continue;
    }
    smallest = std::min(smallest, norm);
    largest = std::max(largest, norm);
  }
  if (!(largest >= 0.0)) {
    return false;  // nothing inside, or every norm non-finite
  }
  out[0] = smallest;
  out[1] = largest;
  return true;
}

// ---- classify --------------------------------------------------------------------------------------------

bool FocusingClassification::JacobianFocusing() const {
  for (const CriticalOnset& onset : onsets) {
    if (onset.JacobianFocusing()) {
      return true;
    }
  }
  return false;
}

std::string FocusingClassification::Mechanism() const {
  if (halo_map_rank == 0) {
    return "point_mass";
  }
  std::string mechanism;
  const char* parts[2] = { "jacobian", "dimension_collapse" };
  const bool on[2] = { JacobianFocusing(), DimensionCollapse() };
  for (int k = 0; k < 2; k++) {
    if (on[k]) {
      if (!mechanism.empty()) {
        mechanism += "+";
      }
      mechanism += parts[k];
    }
  }
  return mechanism.empty() ? "none" : mechanism;
}

FocusingClassification Classify(const FaceNormalTable& normals, const FacePolygonTable& polygons, const int* slots,
                                int slot_count, const PoseDensitySpec& density, double index,
                                const FocusingOptions& options) {
  FocusingClassification out;
  out.path = PathIdOf(normals, slots, slot_count);
  out.confined = ConfinedDimensionsOf(density);
  if (IsRankZeroPath(normals, slots, slot_count)) {
    // A rank-0 path is a point mass at the sun: no field, no onsets, no gradient range, and
    // family_pinned stays false (LI classify returns before family_pinned is even asked).
    out.halo_map_rank = 0;
    return out;
  }
  out.halo_map_rank = 2;
  out.family_pinned = FamilyPinned(normals, slots, slot_count, density);

  const DeviationField field(normals, polygons, slots, slot_count, index);
  const WalkResult walk = WalkBoundary(field, options.walk);
  if (walk.status != WalkStatus::kOk) {
    out.escaped = true;
    out.escape_status = walk.status;
    out.escape_message = std::string(WalkStatusName(walk.status)) + ": " + walk.message;
    return out;
  }
  DegenerateFoldSet fold_set;
  const DegenerateFoldSet* fold_set_ptr = nullptr;
  if (field.fold().degenerate) {
    fold_set = BuildDegenerateFoldSet(field, kFoldCircleSamples);
    fold_set_ptr = &fold_set;
  }
  out.onsets = FieldOnsets(field, fold_set_ptr, walk.loop, options.newton);
  double range[2];
  if (GradientNormRange(field, options.lattice_n, range)) {
    out.has_gradient_norm_range = true;
    out.gradient_norm_range[0] = range[0];
    out.gradient_norm_range[1] = range[1];
  }
  return out;
}

// ---- the wavelength-critical table ----------------------------------------------------------------------

WavelengthCriticalTable WavelengthCriticalTableOf(const FaceNormalTable& normals, const FacePolygonTable& polygons,
                                                  const int* slots, int slot_count, const PoseDensitySpec& density,
                                                  const std::vector<std::string>& labels,
                                                  const std::vector<double>& indices, const FocusingOptions& options) {
  WavelengthCriticalTable table;
  if (labels.empty()) {
    table.escaped = true;
    table.message = "indices is empty";
    return table;
  }
  if (labels.size() != indices.size()) {
    table.escaped = true;
    table.message = "labels and indices differ";
    return table;
  }
  std::vector<FocusingClassification> classifications;
  classifications.reserve(labels.size());
  for (size_t k = 0; k < labels.size(); k++) {
    FocusingClassification classification =
        Classify(normals, polygons, slots, slot_count, density, indices[k], options);
    if (classification.escaped) {
      // The walk refused at one index: no onset table exists to align (LI's walk error propagates
      // out of classify before the alignment ever runs).
      table.escaped = true;
      table.message = classification.escape_message;
      return table;
    }
    classifications.push_back(std::move(classification));
  }
  table.path = classifications[0].path;
  table.labels = labels;
  table.indices = indices;
  for (const FocusingClassification& classification : classifications) {
    if (classification.path != table.path) {
      table.escaped = true;
      table.message = "classifications disagree on the path: " + table.path + " vs " + classification.path;
      return table;
    }
  }
  for (const FocusingClassification& classification : classifications) {
    if (classification.onsets.size() != classifications[0].onsets.size()) {
      table.escaped = true;
      table.message = "onset counts differ across refractive indices";
      return table;
    }
  }
  for (size_t rank = 0; rank < classifications[0].onsets.size(); rank++) {
    const CriticalOnset& first = classifications[0].onsets[rank];
    for (const FocusingClassification& classification : classifications) {
      const CriticalOnset& onset = classification.onsets[rank];
      if (onset.location != first.location || onset.source != first.source || onset.profile != first.profile) {
        table.escaped = true;
        table.message = std::string("onset ") + std::to_string(rank) +
                        " differs across refractive indices: " + OnsetSourceName(first.source) + " vs " +
                        OnsetSourceName(onset.source);
        return table;
      }
    }
    WavelengthOnsetShift row;
    row.location = first.location;
    row.source = first.source;
    row.profile = first.profile;
    row.jacobian_focusing = first.JacobianFocusing();
    double smallest = kInf;
    double largest = -kInf;
    for (const FocusingClassification& classification : classifications) {
      const double value_deg = Deg(classification.onsets[rank].value);
      row.values_deg.push_back(value_deg);
      smallest = std::min(smallest, value_deg);
      largest = std::max(largest, value_deg);
    }
    row.displacement_deg = largest - smallest;
    table.onsets.push_back(std::move(row));
  }
  return table;
}

}  // namespace lumice::analytic
