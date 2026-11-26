#include "analytic/family_rank.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

#include "analytic/discovery.hpp"
#include "analytic/jet.hpp"
#include "analytic/jet2.hpp"
#include "analytic/path_chain.hpp"
#include "analytic/so3.hpp"
#include "core/math.hpp"

namespace lumice::analytic {

namespace {

constexpr double kDegToRad = math::kDegreeToRadD;
constexpr double kRadToDeg = 1.0 / math::kDegreeToRadD;

// The pose chart (EulerPose) and the sky read (ImageAltAzDeg) live in the module header — the
// single authority both this kernel and the survey assembly consume.

// ---- the grid -----------------------------------------------------------------------------------

// One free dimension's sampling axis: the periodic midpoint grid (the orbit producer's) for
// azimuth and roll, the inclusive uniform grid over [0, 180] for zenith. count >= 2 (the entry's
// validation), so the interval spacing never divides by zero.
struct GridAxis {
  FamilyDimension dim = FamilyDimension::kAzimuth;
  int count = 0;
  bool periodic = true;
  double spacing_deg = 0.0;

  double ValueDeg(int i) const { return periodic ? 360.0 * (i + 0.5) / count : 180.0 * i / (count - 1); }
};

// The spec's free dimensions in canonical (zenith, azimuth, roll) order, with their axes.
void FreeAxes(const RestrictedFamilySpec& spec, int grid, GridAxis axes[2], int* free_count) {
  *free_count = 0;
  const FamilyDimensionSpec* dims[3] = { &spec.zenith, &spec.azimuth, &spec.roll };
  const FamilyDimension names[3] = { FamilyDimension::kZenith, FamilyDimension::kAzimuth, FamilyDimension::kRoll };
  for (int i = 0; i < 3; i++) {
    if (dims[i]->mode == FamilyDimensionMode::kFree) {
      GridAxis& axis = axes[(*free_count)++];
      axis.dim = names[i];
      axis.count = grid;
      axis.periodic = names[i] != FamilyDimension::kZenith;
      axis.spacing_deg = axis.periodic ? 360.0 / grid : 180.0 / (grid - 1);
    }
  }
}

// The value of every dimension at a scan point: the free ones from params (canonical slot
// order, degrees), the frozen ones at their centers.
void DimensionDegrees(const RestrictedFamilySpec& spec, const GridAxis axes[2], int free_count,
                      const double params_deg[2], double degrees_out[3]) {
  double degrees[3] = { spec.zenith.center_deg, spec.azimuth.center_deg, spec.roll.center_deg };
  const FamilyDimension names[3] = { FamilyDimension::kZenith, FamilyDimension::kAzimuth, FamilyDimension::kRoll };
  for (int k = 0; k < free_count; k++) {
    for (int d = 0; d < 3; d++) {
      if (axes[k].dim == names[d]) {
        degrees[d] = params_deg[k];
      }
    }
  }
  degrees_out[0] = degrees[0];
  degrees_out[1] = degrees[1];
  degrees_out[2] = degrees[2];
}

// ---- the per-sample evaluation --------------------------------------------------------------------

struct Sample {
  bool valid = false;
  double outgoing[3] = {};
  double rows[2][3] = {};  // d(outgoing)/d(free dim k), already image-tangent
  double sigma1 = 0.0;
  double sigma2 = 0.0;
  int rank_class = 0;
  double signed_jacobian = 0.0;  // free_count == 2 only
};

// One Jet<2> chain trace at free-dimension DEGREES (canonical slot order): the free dimensions
// seeded as chart variables, the frozen ones constants. The chain runs in kFirstFailure, so a
// gate rejection returns false — the invalid region — and every accepted outgoing is finite.
bool EvaluateSample(const FaceNormalTable& normals, const int* slots, int slot_count, const double sun_hat[3],
                    double refractive_index, int free_count, const GridAxis axes[2], const RestrictedFamilySpec& spec,
                    const double params_deg[2], Sample* out) {
  double degrees[3];
  DimensionDegrees(spec, axes, free_count, params_deg, degrees);
  const FamilyDimension names[3] = { FamilyDimension::kZenith, FamilyDimension::kAzimuth, FamilyDimension::kRoll };
  Jet<2> angle[3];
  int slot = 0;
  for (int d = 0; d < 3; d++) {
    bool is_free = false;
    for (int k = 0; k < free_count; k++) {
      is_free = is_free || axes[k].dim == names[d];
    }
    if (is_free) {
      angle[d] = Jet<2>::Variable(degrees[d] * kDegToRad, slot);
      slot++;
    } else {
      angle[d] = Jet<2>(degrees[d] * kDegToRad);
    }
  }
  Jet<2> pose[9];
  EulerPose(angle[1], angle[0], angle[2], pose);
  const Jet<2> incident[3] = { Jet<2>(-sun_hat[0]), Jet<2>(-sun_hat[1]), Jet<2>(-sun_hat[2]) };
  Jet<2> outgoing[3];
  if (!TracePathChain(normals, slots, slot_count, Jet<2>(refractive_index), incident, pose, outgoing, nullptr,
                      nullptr)) {
    return false;
  }
  for (int i = 0; i < 3; i++) {
    out->outgoing[i] = outgoing[i].a;
    for (int k = 0; k < free_count; k++) {
      out->rows[k][i] = outgoing[i].v[k];
    }
  }
  if (free_count == 2) {
    // The two rows are tangent to the image sphere at the outgoing (|outgoing| = 1 by
    // construction), so the Gram-eigenvalue pair IS the restricted singular-value pair.
    const so3::TwoByThreeSvd svd = so3::SvdTwoByThree(out->rows[0], out->rows[1]);
    out->sigma1 = svd.sigma1;
    out->sigma2 = svd.sigma2;
    // The orientation Jacobian in a tangent basis: every basis TangentBasis builds is
    // right-handed about the outgoing (basis[0] x basis[1] = d), so the SIGN is basis-choice
    // independent and its flip is the fold. |det| = sigma1 * sigma2 by Lagrange.
    double basis[2][3];
    so3::TangentBasis(out->outgoing, basis);
    const double m00 = so3::Dot3(basis[0], out->rows[0]);
    const double m01 = so3::Dot3(basis[0], out->rows[1]);
    const double m10 = so3::Dot3(basis[1], out->rows[0]);
    const double m11 = so3::Dot3(basis[1], out->rows[1]);
    out->signed_jacobian = m00 * m11 - m01 * m10;
  } else {
    out->sigma1 = so3::Norm3(out->rows[0]);
    out->sigma2 = 0.0;
  }
  out->rank_class = out->sigma2 >= kFamilyRankSigmaTolerance ? 2 : (out->sigma1 >= kFamilyRankSigmaTolerance ? 1 : 0);
  out->valid = true;
  return true;
}

// ---- the union-find of the grid structures -----------------------------------------------------------

struct DisjointSet {
  std::vector<int> parent;

  explicit DisjointSet(int count) : parent(static_cast<size_t>(count)) {
    for (int i = 0; i < count; i++) {
      parent[static_cast<size_t>(i)] = i;
    }
  }

  int Find(int x) {
    while (parent[static_cast<size_t>(x)] != x) {
      parent[static_cast<size_t>(x)] = parent[static_cast<size_t>(parent[static_cast<size_t>(x)])];
      x = parent[static_cast<size_t>(x)];
    }
    return x;
  }

  void Union(int a, int b) {
    a = Find(a);
    b = Find(b);
    if (a != b) {
      parent[static_cast<size_t>(b)] = a;
    }
  }
};

// The next index along `axis` in `direction`, or -1 at a non-periodic bound.
int StepIndex(const GridAxis& axis, int i, int direction) {
  int j = i + direction;
  if (axis.periodic) {
    if (j < 0) {
      j += axis.count;
    } else if (j >= axis.count) {
      j -= axis.count;
    }
    return j;
  }
  return (j < 0 || j >= axis.count) ? -1 : j;
}

// ---- the second-order fold reads (Jet2 through the SAME chain) -------------------------------------
//
// The classification pass reads, at each chain point, the exact gradient and Hessian of the
// signed Jacobian through the one templated optical chain (jet2.hpp is the second-order tier,
// dp_field.hpp's precedent — no finite differences anywhere, no second optical mapping). One
// Jet2<2> evaluation per chain point; its value parts double as the first-order read.

struct FoldJet2Read {
  bool valid = false;
  double outgoing[3] = {};
  double rows[2][3] = {};  // d(outgoing)/d(free dim k), value parts
  double sigma1 = 0.0;
  double sigma2 = 0.0;
  double signed_jacobian = 0.0;
  double grad_jacobian[2] = {};     // dJ/d(param_k), exact through the chain (order-2 information)
  double hess_jacobian[2][2] = {};  // always zero: J's Hessian is third-order chain information
                                    // this read does not carry; never consumed
};

bool EvaluateSampleJet2(const FaceNormalTable& normals, const int* slots, int slot_count, const double sun_hat[3],
                        double refractive_index, const GridAxis axes[2], const RestrictedFamilySpec& spec,
                        const double params_deg[2], FoldJet2Read* out) {
  double degrees[3];
  DimensionDegrees(spec, axes, 2, params_deg, degrees);
  const FamilyDimension names[3] = { FamilyDimension::kZenith, FamilyDimension::kAzimuth, FamilyDimension::kRoll };
  Jet2<2> angle[3];
  int slot = 0;
  for (int d = 0; d < 3; d++) {
    bool is_free = false;
    for (int k = 0; k < 2; k++) {
      is_free = is_free || axes[k].dim == names[d];
    }
    if (is_free) {
      angle[d] = Jet2<2>::Variable(degrees[d] * kDegToRad, slot);
      slot++;
    } else {
      angle[d] = Jet2<2>(degrees[d] * kDegToRad);
    }
  }
  Jet2<2> pose[9];
  EulerPose(angle[1], angle[0], angle[2], pose);
  const Jet2<2> incident[3] = { Jet2<2>(-sun_hat[0]), Jet2<2>(-sun_hat[1]), Jet2<2>(-sun_hat[2]) };
  Jet2<2> outgoing[3];
  if (!TracePathChain(normals, slots, slot_count, Jet2<2>(refractive_index), incident, pose, outgoing, nullptr,
                      nullptr)) {
    return false;
  }
  for (int i = 0; i < 3; i++) {
    out->outgoing[i] = outgoing[i].a;
    for (int k = 0; k < 2; k++) {
      out->rows[k][i] = outgoing[i].v[k];
    }
  }
  const so3::TwoByThreeSvd svd = so3::SvdTwoByThree(out->rows[0], out->rows[1]);
  out->sigma1 = svd.sigma1;
  out->sigma2 = svd.sigma2;
  // The restricted Jacobian's tangent-basis matrix as an ORDER-ONE Jet2 function of the
  // parameters: an entry's value is the outgoing's first derivative (rows) and its gradient the
  // outgoing's second derivative (h). Jet2 does not carry the third-order part, so the entries'
  // own Hessian is left zero — only J's value and gradient are consumed below, and J is degree
  // one in the matrix, so its gradient needs exactly this order. The tangent basis is built
  // from the outgoing value and treated as constant: J is the determinant of the restricted
  // Jacobian in a right-handed basis, invariant under every basis the code builds (det R = 1),
  // so the basis's own parameter dependence contributes nothing to dJ.
  double basis[2][3];
  so3::TangentBasis(out->outgoing, basis);
  Jet2<2> jm[2][2];
  for (int k = 0; k < 2; k++) {
    for (int l = 0; l < 2; l++) {
      Jet2<2> entry(0.0);
      for (int i = 0; i < 3; i++) {
        entry.a += basis[k][i] * outgoing[i].v[l];
        for (int q = 0; q < 2; q++) {
          entry.v[q] += basis[k][i] * outgoing[i].h[l][q];
        }
      }
      jm[k][l] = entry;
    }
  }
  const Jet2<2> j = jm[0][0] * jm[1][1] - jm[0][1] * jm[1][0];
  out->signed_jacobian = j.a;
  for (int k = 0; k < 2; k++) {
    out->grad_jacobian[k] = j.v[k];
    // The expression's Hessian is not carried at this order (the entries' zero Hessians above);
    // recorded as zeros so the field can never masquerade as information.
    for (int l = 0; l < 2; l++) {
      out->hess_jacobian[k][l] = 0.0;
    }
  }
  out->valid = true;
  return true;
}

// The kernel direction of the restricted Jacobian at a fold read (the null direction of the
// 2x2 tangent-basis matrix M): the perpendicular of the stronger row — at a fold sigma2 ~ 0,
// the rows are parallel and either perpendicular is the kernel up to scale; taking the
// stronger row keeps the construction defined when the weaker row rounds to zero. Normalized;
// false when the stronger row is itself degenerate (rank ~ 0 — not a fold read). Takes the
// plain value-part rows so the first-order samples (midpoint verification) share it with the
// Jet2 reads.
bool KernelDirection(const double rows[2][3], const double outgoing[3], double v[2]) {
  double basis[2][3];
  so3::TangentBasis(outgoing, basis);
  double m[2][2];
  for (int k = 0; k < 2; k++) {
    for (int l = 0; l < 2; l++) {
      m[k][l] = so3::Dot3(basis[k], rows[l]);
    }
  }
  const double n0 = m[0][0] * m[0][0] + m[0][1] * m[0][1];
  const double n1 = m[1][0] * m[1][0] + m[1][1] * m[1][1];
  const int r = n0 >= n1 ? 0 : 1;
  const double norm = std::sqrt(m[r][0] * m[r][0] + m[r][1] * m[r][1]);
  if (!(norm > 0.0)) {
    return false;
  }
  v[0] = m[r][1] / norm;
  v[1] = -m[r][0] / norm;
  return true;
}

// The neighbour parameters' per-dimension difference in canonical degree coordinates: a
// periodic dimension's difference wraps to the short way across the seam. The DELTA is the
// quantity the extent accumulation consumes (the un-normalized parameter displacement — the
// review's separation of the parameter displacement from the unit tangent: a normalized helper
// must never double as a length source, every non-degenerate adjacency would read exactly one
// degree); the tangent helper normalizes it.
void ChainParamDelta(const GridAxis axes[2], const double prev[2], const double next[2], double d[2]) {
  for (int k = 0; k < 2; k++) {
    d[k] = next[k] - prev[k];
    if (axes[k].periodic) {
      if (d[k] > 180.0) {
        d[k] -= 360.0;
      } else if (d[k] < -180.0) {
        d[k] += 360.0;
      }
    }
  }
}

// The unit tangent of the chain in canonical parameter coordinates (degrees — a uniform unit
// factor per dimension leaves every direction comparison unchanged, so the degrees need no
// conversion; the per-dimension differences must agree with the kernel's per-RADIAN components
// only up to ONE common factor, which they do). `prev`/`next` are the neighbour parameters.
void ChainTangent(const GridAxis axes[2], const double prev[2], const double next[2], double tau[2]) {
  ChainParamDelta(axes, prev, next, tau);
  const double norm = std::sqrt(tau[0] * tau[0] + tau[1] * tau[1]);
  if (norm > 0.0) {
    tau[0] /= norm;
    tau[1] /= norm;
  }
}

// The image sky direction of a located point: -outgoing, the unit vector ImageAltAzDeg reads
// its alt/az from. All image-form geometry (diameters, means) runs on these unit vectors —
// no trig round trip, no azimuth-wrap case.
void ImageSkyDir(const double outgoing[3], double s[3]) {
  for (int i = 0; i < 3; i++) {
    s[i] = -outgoing[i];
  }
}

// The angular distance of two image directions, degrees.
double SkyAngleDeg(const double a[3], const double b[3]) {
  const double cross[3] = { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
  const double norm = std::sqrt(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]);
  const double dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  return std::atan2(norm, dot) / kDegToRad;
}

// The point's transversality resolution: the bound carries (a) the located point's own J
// residual (what the bisection left — the same units dJ(v) inherits per unit parameter through
// the gradient scale; the Hessian scale of the residual's curvature propagation is third-order
// chain information this read does not carry, and the residual itself dominates for a located
// crossing), and (b) a rounding-scale relative floor on the gradient (the Jet2 chain here is a
// few hundred operations, ~1e-13 relative accumulation; relative to the derivatives, never an
// absolute epsilon).
double DjvResolution(const FoldJet2Read& read) {
  const double g[2] = { read.grad_jacobian[0], read.grad_jacobian[1] };
  const double g_norm = std::sqrt(g[0] * g[0] + g[1] * g[1]);
  return std::fabs(read.signed_jacobian) + 1e-13 * g_norm;
}

// Left rotation of a vector by `rotation` positions (C++17 — no template lambdas): the closed
// chain's seam-adjacency fix-up before its linear run detection.
template <class T>
void RotateVector(std::vector<T>* values, int rotation) {
  const int n = static_cast<int>(values->size());
  std::vector<T> rotated(static_cast<size_t>(n));
  for (int k = 0; k < n; k++) {
    rotated[static_cast<size_t>(k)] = (*values)[static_cast<size_t>((k + rotation) % n)];
  }
  values->swap(rotated);
}

// One cell's one-level subdivision grid: the nine evaluation points in unit-cell coordinates
// (the four corners are grid samples, the rest are evaluated) and the twelve sub-edges over
// them. `side` names which full boundary edge a boundary half sits on (0 = the dim0 edge at
// v = 0, 1 = at v = 1, 2 = the dim1 edge at u = 0, 3 = at u = 1; -1 interior). Shared by the
// ambiguous-cell pairing and the point-image verification's on-locus refinement.
constexpr double kCellUnit[9][2] = { { 0.0, 0.0 }, { 0.5, 0.0 }, { 1.0, 0.0 }, { 0.0, 0.5 }, { 0.5, 0.5 },
                                     { 1.0, 0.5 }, { 0.0, 1.0 }, { 0.5, 1.0 }, { 1.0, 1.0 } };
struct CellSubEdge {
  int a;
  int b;
  int dim;
  int side;
};
constexpr CellSubEdge kCellSubEdges[12] = {
  { 0, 1, 0, 0 }, { 1, 2, 0, 0 }, { 3, 4, 0, -1 }, { 4, 5, 0, -1 }, { 6, 7, 0, 1 }, { 7, 8, 0, 1 },
  { 0, 3, 1, 2 }, { 3, 6, 1, 2 }, { 1, 4, 1, -1 }, { 4, 7, 1, -1 }, { 2, 5, 1, 3 }, { 5, 8, 1, 3 },
};

}  // namespace

// ---- validation and routing ------------------------------------------------------------------------

const char* FamilySpecError(const RestrictedFamilySpec& spec) {
  static const char* const kNotFinite[3] = { "the fixed zenith center must be finite",
                                             "the fixed azimuth center must be finite",
                                             "the fixed roll center must be finite" };
  static const char* const kOutOfRange[3] = { "the fixed zenith center is outside [0, 180]",
                                              "the fixed azimuth center is outside [0, 360)",
                                              "the fixed roll center is outside [0, 360)" };
  static const char* const kFreeCenter[3] = { "a free zenith takes no center: it must be exactly 0",
                                              "a free azimuth takes no center: it must be exactly 0",
                                              "a free roll takes no center: it must be exactly 0" };
  const FamilyDimensionSpec* const dims[3] = { &spec.zenith, &spec.azimuth, &spec.roll };
  const double max_center[3] = { 180.0, 360.0, 360.0 };  // exclusive: zenith [0, 180], others [0, 360)
  for (int i = 0; i < 3; i++) {
    if (dims[i]->mode == FamilyDimensionMode::kFixed) {
      if (!std::isfinite(dims[i]->center_deg)) {
        return kNotFinite[i];
      }
      if (dims[i]->center_deg < 0.0 || dims[i]->center_deg >= max_center[i]) {
        return kOutOfRange[i];
      }
    } else if (dims[i]->center_deg != 0.0) {
      return kFreeCenter[i];
    }
  }
  return "";
}

const char* FamilyRankStatusName(FamilyRankStatus status) {
  switch (status) {
    case FamilyRankStatus::kOk:
      return "ok";
    case FamilyRankStatus::kRefusedSinglePose:
      return "refused: all dimensions fixed (a single pose — EvaluatePath is the entry)";
    case FamilyRankStatus::kRefusedFullSo3:
      return "refused: all dimensions free (full SO(3) — Classify is the entry)";
    case FamilyRankStatus::kRefusedAzimuthFrozen:
      return "refused: azimuth fixed while another dimension is free (outside the routing v1 domain)";
    case FamilyRankStatus::kRefusedInvalidSpec:
      return "refused: malformed family spec";
    case FamilyRankStatus::kRefusedGridOutOfBounds:
      return "refused: the grid count is outside its bounds";
  }
  return "unknown";
}

const char* FoldChainEndName(FoldChainEnd end) {
  switch (end) {
    case FoldChainEnd::kOpen:
      return "open";
    case FoldChainEnd::kClosedLoop:
      return "closed_loop";
    case FoldChainEnd::kFeasibilityInterruption:
      return "feasibility_interruption";
    case FoldChainEnd::kAmbiguousCell:
      return "ambiguous_cell";
    case FoldChainEnd::kStopRequested:
      return "stop_requested";
  }
  return "unknown";
}

const char* FoldSegmentFormName(FoldSegmentForm form) {
  switch (form) {
    case FoldSegmentForm::kArc:
      return "arc";
    case FoldSegmentForm::kPointImage:
      return "point_image";
    case FoldSegmentForm::kUnknown:
      return "unknown";
  }
  return "unknown";
}

// The shared refusal routing of both entries; kOk or the refusal.
FamilyRankStatus RouteFamilySpec(const RestrictedFamilySpec& spec, int* free_count) {
  *free_count = (spec.zenith.mode == FamilyDimensionMode::kFree ? 1 : 0) +
                (spec.azimuth.mode == FamilyDimensionMode::kFree ? 1 : 0) +
                (spec.roll.mode == FamilyDimensionMode::kFree ? 1 : 0);
  if (FamilySpecError(spec)[0] != '\0') {
    return FamilyRankStatus::kRefusedInvalidSpec;
  }
  if (*free_count == 0) {
    return FamilyRankStatus::kRefusedSinglePose;
  }
  if (*free_count == 3) {
    return FamilyRankStatus::kRefusedFullSo3;
  }
  if (spec.azimuth.mode == FamilyDimensionMode::kFixed) {
    // *free_count is 1 or 2 here, so another dimension is free: outside the routing v1 domain.
    return FamilyRankStatus::kRefusedAzimuthFrozen;
  }
  return FamilyRankStatus::kOk;
}

FamilyPointRecord EvaluateFamilyPoint(const FaceNormalTable& normals, const int* slots, int slot_count,
                                      const RestrictedFamilySpec& spec, const double sun_hat[3],
                                      double refractive_index, const double params_deg[2]) {
  FamilyPointRecord out;
  int free_count = 0;
  out.status = RouteFamilySpec(spec, &free_count);
  if (out.status != FamilyRankStatus::kOk) {
    return out;
  }
  GridAxis axes[2];
  int axes_count = 0;
  FreeAxes(spec, 2, axes, &axes_count);
  Sample sample;
  if (!EvaluateSample(normals, slots, slot_count, sun_hat, refractive_index, free_count, axes, spec, params_deg,
                      &sample)) {
    return out;  // the chain gates' verdict: invalid data, status stays kOk
  }
  out.valid = true;
  for (int i = 0; i < 3; i++) {
    out.outgoing[i] = sample.outgoing[i];
  }
  out.sigma1 = sample.sigma1;
  out.sigma2 = sample.sigma2;
  out.rank_class = sample.rank_class;
  out.signed_jacobian = sample.signed_jacobian;
  return out;
}

RestrictedFamilyRank RankRestrictedFamily(const FaceNormalTable& normals, const int* slots, int slot_count,
                                          const RestrictedFamilySpec& spec, const double sun_hat[3],
                                          double refractive_index, int grid, const DiscoveryStop* stop) {
  RestrictedFamilyRank out;
  out.grid = grid;
  int free_count = 0;
  out.status = RouteFamilySpec(spec, &free_count);
  if (out.status != FamilyRankStatus::kOk) {
    return out;
  }
  if (grid < kFamilyMinGridPerDimension || grid > kFamilyMaxGridPerDimension) {
    out.status = FamilyRankStatus::kRefusedGridOutOfBounds;
    return out;
  }
  // The query's cooperative stop (the shared-budget contract: the recovery work joins the same
  // stop, no second budget): polled at the bounded work-unit boundaries only — a scan row, an
  // edge batch, a cell pairing, a chain walk, a classification read — never inside the
  // numerics. A poll ticks kPollFamilyCurve (the liveness evidence) and returns true when the
  // query wants out; every true marks the result.
  const auto stopped = [&]() {
    if (stop == nullptr) {
      return false;
    }
    if (stop->token != nullptr) {
      stop->token->NotePoll(kPollFamilyCurve);
    }
    if (stop->Stopped()) {
      out.stop_requested = true;
      return true;
    }
    return false;
  };

  GridAxis axes[2];
  FreeAxes(spec, grid, axes, &out.free_count);
  for (int k = 0; k < out.free_count; k++) {
    out.free_dimensions[k] = axes[k].dim;
    out.grid_spacing_deg[k] = axes[k].spacing_deg;
  }
  const int n0 = axes[0].count;
  const int n1 = out.free_count == 2 ? axes[1].count : 1;
  out.samples = n0 * n1;

  // The product scan.
  std::vector<Sample> samples(static_cast<size_t>(out.samples));
  for (int i0 = 0; i0 < n0; i0++) {
    if (stopped()) {
      break;  // the rows past the cut were never evaluated: the counters below read what ran
    }
    for (int i1 = 0; i1 < n1; i1++) {
      const double params[2] = { axes[0].ValueDeg(i0), axes[1].ValueDeg(i1) };
      EvaluateSample(normals, slots, slot_count, sun_hat, refractive_index, out.free_count, axes, spec, params,
                     &samples[static_cast<size_t>(i0 * n1 + i1)]);
    }
  }
  bool have_sigma = false;
  for (const Sample& sample : samples) {
    if (!sample.valid) {
      out.invalid_samples++;
      continue;
    }
    out.valid_samples++;
    out.generic_rank = std::max(out.generic_rank, sample.rank_class);
    if (!have_sigma) {
      out.min_sigma1 = sample.sigma1;
      out.min_sigma2 = sample.sigma2;
      have_sigma = true;
    } else {
      out.min_sigma1 = std::min(out.min_sigma1, sample.sigma1);
      out.min_sigma2 = std::min(out.min_sigma2, sample.sigma2);
    }
    if (sample.rank_class == 2) {
      out.rank2_samples++;
    } else if (sample.rank_class == 1) {
      out.rank1_samples++;
    } else {
      out.rank0_samples++;
    }
  }
  if (out.free_count == 1) {
    out.min_sigma2 = 0.0;  // not a quantity of a one-dimensional family
  }

  // The fold locus (two-dimensional families only): the orientation Jacobian's sign change
  // across a grid edge, bisected along the edge. An edge's parameter runs FORWARD from its first
  // vertex's value by one spacing — a periodic wrap just passes through 360, which the chart's
  // trigonometry absorbs; the record's params fold back into [0, 360).
  if (out.free_count == 2) {
    struct EdgeRef {
      int dim;  // which dimension varies along the edge
      int i0;
      int i1;
    };
    const int dim0_edges = axes[0].periodic ? n0 : n0 - 1;
    const int total_edges = dim0_edges * n1 + n0 * n1;  // + the dim1 edges (always periodic)
    std::vector<int> edge_crossing(static_cast<size_t>(total_edges), -1);
    const auto edge_key = [&](const EdgeRef& edge) {
      return edge.dim == 0 ? edge.i0 * n1 + edge.i1 : dim0_edges * n1 + edge.i0 * n1 + edge.i1;
    };

    // Bisect the signed Jacobian's sign change along the parameter segment from `lo_params` to
    // `hi_params` (degrees, canonical order — full edges and sub-brackets share the form), from
    // `a_sign`'s side. Returns false when a probe falls into the invalid region (no located
    // crossing; the refinement fell into the invalid region). `t_out` is the bracket-relative
    // position of the located root.
    const auto bisect_edge = [&](const double lo_params[2], const double hi_params[2], double a_sign, double* t_out,
                                 Sample* located) -> bool {
      double t_lo = 0.0;
      double t_hi = 1.0;
      for (int step = 0; step < kFoldBisectionSteps; step++) {
        const double t = 0.5 * (t_lo + t_hi);
        const double probe[2] = { lo_params[0] + t * (hi_params[0] - lo_params[0]),
                                  lo_params[1] + t * (hi_params[1] - lo_params[1]) };
        Sample mid;
        if (!EvaluateSample(normals, slots, slot_count, sun_hat, refractive_index, 2, axes, spec, probe, &mid)) {
          return false;  // the refinement fell into the invalid region: no located crossing
        }
        if (mid.signed_jacobian * a_sign > 0.0) {
          t_lo = t;
        } else {
          t_hi = t;
        }
      }
      const double t = 0.5 * (t_lo + t_hi);
      const double refined[2] = { lo_params[0] + t * (hi_params[0] - lo_params[0]),
                                  lo_params[1] + t * (hi_params[1] - lo_params[1]) };
      if (!EvaluateSample(normals, slots, slot_count, sun_hat, refractive_index, 2, axes, spec, refined, located)) {
        return false;
      }
      *t_out = t;
      return true;
    };

    // Append one located fold point from an evaluated sample at the root `lo + t*(hi-lo)`
    // (degrees, canonical order) on the edge `ref`, with its bracket-relative position `t` (an
    // interior refinement crossing carries dim -1 and t 0). Returns the crossing's index.
    const auto record_crossing = [&](const EdgeRef& ref, bool from_cliff_probe, const double lo_params[2],
                                     const double hi_params[2], double t, const Sample& located) -> int {
      const double refined[2] = { lo_params[0] + t * (hi_params[0] - lo_params[0]),
                                  lo_params[1] + t * (hi_params[1] - lo_params[1]) };
      FoldPointRecord record;
      for (int k = 0; k < 2; k++) {
        record.params_deg[k] = axes[k].periodic ? std::fmod(refined[k], 360.0) : refined[k];
      }
      double degrees[3];
      DimensionDegrees(spec, axes, 2, refined, degrees);
      EulerPose(degrees[1] * kDegToRad, degrees[0] * kDegToRad, degrees[2] * kDegToRad, record.pose);
      for (int i = 0; i < 3; i++) {
        record.outgoing[i] = located.outgoing[i];
      }
      ImageAltAzDeg(record.outgoing, &record.image_alt_deg, &record.image_az_deg);
      record.sigma1 = located.sigma1;
      record.sigma2 = located.sigma2;
      record.signed_jacobian = located.signed_jacobian;
      record.edge_dim = ref.dim;
      record.edge_i0 = ref.i0;
      record.edge_i1 = ref.i1;
      record.edge_t = t;
      record.from_cliff_probe = from_cliff_probe;
      out.crossings.push_back(record);
      return static_cast<int>(out.crossings.size()) - 1;
    };

    // Locate this edge's crossing and append its record, or do nothing. Returns the crossing
    // index (>= 0) or -1.
    const auto locate_crossing = [&](const EdgeRef& edge) -> int {
      const int j0 = edge.dim == 0 ? StepIndex(axes[0], edge.i0, 1) : edge.i0;
      const int j1 = edge.dim == 1 ? StepIndex(axes[1], edge.i1, 1) : edge.i1;
      if (j0 < 0 || j1 < 0) {
        return -1;
      }
      const Sample& a = samples[static_cast<size_t>(edge.i0 * n1 + edge.i1)];
      const Sample& b = samples[static_cast<size_t>(j0 * n1 + j1)];
      if (!a.valid || !b.valid || a.signed_jacobian * b.signed_jacobian >= 0.0) {
        return -1;
      }
      double base[2] = { axes[0].ValueDeg(edge.i0), axes[1].ValueDeg(edge.i1) };
      double hi[2] = { base[0], base[1] };
      hi[edge.dim] += axes[edge.dim].spacing_deg;
      double t = 0.0;
      Sample located;
      if (!bisect_edge(base, hi, a.signed_jacobian, &t, &located)) {
        return -1;
      }
      return record_crossing(edge, false, base, hi, t, located);
    };

    // The cliff-edge midpoint probe (the ordered-arc work's recovery of the feasibility-cliff
    // loci the both-valid-edges rule loses): an edge with exactly one valid endpoint probes its
    // midpoint once. A valid midpoint with a sign bracket against the valid endpoint recovers
    // the crossing; an invalid midpoint records the unresolved edge (data — no crossing is
    // invented, and nothing is bridged across the blank).
    const auto probe_cliff_edge = [&](const EdgeRef& edge) {
      const int j0 = edge.dim == 0 ? StepIndex(axes[0], edge.i0, 1) : edge.i0;
      const int j1 = edge.dim == 1 ? StepIndex(axes[1], edge.i1, 1) : edge.i1;
      if (j0 < 0 || j1 < 0) {
        return;
      }
      const Sample& a = samples[static_cast<size_t>(edge.i0 * n1 + edge.i1)];
      const Sample& b = samples[static_cast<size_t>(j0 * n1 + j1)];
      const bool a_valid = a.valid;
      const bool b_valid = b.valid;
      if (a_valid == b_valid) {
        return;  // zero valid endpoints (nothing to bracket from) or both valid (the scan above owns it)
      }
      out.cliff_probed_edges++;
      double base[2] = { axes[0].ValueDeg(edge.i0), axes[1].ValueDeg(edge.i1) };
      double hi[2] = { base[0], base[1] };
      hi[edge.dim] += axes[edge.dim].spacing_deg;
      const double mid_params[2] = { base[0] + 0.5 * (hi[0] - base[0]), base[1] + 0.5 * (hi[1] - base[1]) };
      Sample mid;
      if (!EvaluateSample(normals, slots, slot_count, sun_hat, refractive_index, 2, axes, spec, mid_params, &mid)) {
        out.cliff_unresolved_edges++;
        return;
      }
      const Sample& valid_end = a_valid ? a : b;
      if (mid.signed_jacobian * valid_end.signed_jacobian >= 0.0) {
        return;  // no bracket toward this neighbour: nothing located, nothing claimed
      }
      // The sub-bracket always runs FROM the valid endpoint (bisect_edge's sign convention
      // compares probes against the bracket's lo end — the invalid midpoint must never sit
      // there): when the valid endpoint is the edge's second vertex the bracket is the
      // reversed [hi -> mid], and the root's full-edge position mirrors the bracket-relative
      // one.
      double t = 0.0;
      Sample located;
      double lo_params[2];
      double hi_params[2];
      for (int k = 0; k < 2; k++) {
        lo_params[k] = a_valid ? base[k] : hi[k];
        hi_params[k] = a_valid ? mid_params[k] : mid_params[k];
      }
      if (!bisect_edge(lo_params, hi_params, valid_end.signed_jacobian, &t, &located)) {
        out.cliff_unresolved_edges++;
        return;
      }
      const double t_abs = a_valid ? 0.5 * t : 1.0 - 0.5 * t;
      // The record interpolates on the FULL edge frame (base -> hi) at the root's absolute
      // position; the reversed bracket only steered the bisection's sign convention.
      const int crossing = record_crossing(edge, true, base, hi, t_abs, located);
      edge_crossing[static_cast<size_t>(edge_key(edge))] = crossing;
    };

    for (int i0 = 0; i0 < dim0_edges; i0++) {
      if (stopped()) {
        break;  // the edges past the cut were never located: the chains read the located ones
      }
      for (int i1 = 0; i1 < n1; i1++) {
        const EdgeRef edge = { 0, i0, i1 };
        const int crossing = locate_crossing(edge);
        if (crossing >= 0) {
          edge_crossing[static_cast<size_t>(edge_key(edge))] = crossing;
        }
      }
    }
    for (int i0 = 0; i0 < n0; i0++) {
      if (stopped()) {
        break;
      }
      for (int i1 = 0; i1 < n1; i1++) {
        const EdgeRef edge = { 1, i0, i1 };
        const int crossing = locate_crossing(edge);
        if (crossing >= 0) {
          edge_crossing[static_cast<size_t>(edge_key(edge))] = crossing;
        }
      }
    }

    // Connectivity at grid resolution: crossings on the four edges of a common cell are one
    // component (edges sharing only a vertex always bound a common cell, so cells suffice).
    const int crossing_count = static_cast<int>(out.crossings.size());
    if (crossing_count > 0) {
      DisjointSet set(crossing_count);
      for (int i0 = 0; i0 < n0; i0++) {
        for (int i1 = 0; i1 < n1; i1++) {
          const int i0_up = StepIndex(axes[0], i0, 1);
          const int i1_up = StepIndex(axes[1], i1, 1);
          if (i0_up < 0 || i1_up < 0) {
            continue;  // the zenith interval's last row has no upper cells
          }
          const EdgeRef cell[4] = { { 0, i0, i1 }, { 0, i0, i1_up }, { 1, i0, i1 }, { 1, i0_up, i1 } };
          int members[4];
          int member_count = 0;
          for (const EdgeRef& edge : cell) {
            const int crossing = edge_crossing[static_cast<size_t>(edge_key(edge))];
            if (crossing >= 0) {
              members[member_count++] = crossing;
            }
          }
          for (int m = 1; m < member_count; m++) {
            set.Union(members[0], members[m]);
          }
        }
      }
      // Components in first-crossing order, each with its smallest-sigma2 representative.
      std::vector<int> first_of_root(static_cast<size_t>(crossing_count), -1);
      for (int c = 0; c < crossing_count; c++) {
        const int root = set.Find(c);
        if (first_of_root[static_cast<size_t>(root)] < 0) {
          first_of_root[static_cast<size_t>(root)] = c;
        }
      }
      for (int c = 0; c < crossing_count; c++) {
        if (first_of_root[static_cast<size_t>(set.Find(c))] != c) {
          continue;
        }
        FoldComponentRecord component;
        component.first_crossing = c;
        component.representative = c;
        for (int d = c; d < crossing_count; d++) {
          if (set.Find(d) == set.Find(c)) {
            component.crossing_count++;
            if (out.crossings[static_cast<size_t>(d)].sigma2 <
                out.crossings[static_cast<size_t>(component.representative)].sigma2) {
              component.representative = d;
            }
          }
        }
        out.fold_components.push_back(component);
      }
    }

    // ---- the cliff probes: the feasibility-cliff loci the both-valid-edges rule loses --------
    for (int i0 = 0; i0 < dim0_edges; i0++) {
      if (stopped()) {
        break;  // the un-probed edges stay unprobed: the counters read what ran
      }
      for (int i1 = 0; i1 < n1; i1++) {
        probe_cliff_edge(EdgeRef{ 0, i0, i1 });
      }
    }
    for (int i0 = 0; i0 < n0; i0++) {
      if (stopped()) {
        break;
      }
      for (int i1 = 0; i1 < n1; i1++) {
        probe_cliff_edge(EdgeRef{ 1, i0, i1 });
      }
    }

    // ---- the ordered chains -----------------------------------------------------------------
    // Cell occupancy: which crossings each cell's boundary holds (at most one per edge — one
    // crossing per edge by the located scans). Cell (I, J) spans the dim0 interval [I, I+1]
    // and the dim1 interval [J, J+1]; it exists when its upper dim0 neighbor does (the zenith
    // interval's last row has none) and both indices are in range.
    const int dim0_cells = dim0_edges;
    const int cell_total = dim0_cells * n1;
    const auto cell_exists = [&](int I, int J) {
      return I >= 0 && I < dim0_cells && J >= 0 && J < n1 && StepIndex(axes[0], I, 1) >= 0;
    };
    // The (up to two) cells a crossing sits on, as flat cell indices.
    const auto cells_of_crossing = [&](int c, int cells_out[2]) -> int {
      const FoldPointRecord& p = out.crossings[static_cast<size_t>(c)];
      if (p.edge_dim < 0) {
        return 0;  // an interior refinement crossing pairs through its sub-cells' links only
      }
      int count = 0;
      if (p.edge_dim == 0) {
        if (cell_exists(p.edge_i0, p.edge_i1)) {
          cells_out[count++] = p.edge_i0 * n1 + p.edge_i1;
        }
        const int down = StepIndex(axes[1], p.edge_i1, -1);
        if (cell_exists(p.edge_i0, down)) {
          cells_out[count++] = p.edge_i0 * n1 + down;
        }
      } else {
        if (cell_exists(p.edge_i0, p.edge_i1)) {
          cells_out[count++] = p.edge_i0 * n1 + p.edge_i1;
        }
        const int left = StepIndex(axes[0], p.edge_i0, -1);
        if (cell_exists(left, p.edge_i1)) {
          cells_out[count++] = left * n1 + p.edge_i1;
        }
      }
      return count;
    };
    std::vector<std::array<int, 4>> cell_members(static_cast<size_t>(cell_total));
    std::vector<int> cell_counts(static_cast<size_t>(cell_total), 0);
    const int located_count = static_cast<int>(out.crossings.size());
    for (int c = 0; c < located_count; c++) {
      int cells[2];
      const int count = cells_of_crossing(c, cells);
      for (int k = 0; k < count; k++) {
        std::array<int, 4>& members = cell_members[static_cast<size_t>(cells[k])];
        int& cell_count = cell_counts[static_cast<size_t>(cells[k])];
        if (cell_count < 4) {
          members[static_cast<size_t>(cell_count++)] = c;
        }
      }
    }

    // Pairing: a cell with exactly two crossings pairs them (the chain link); a cell with
    // three or four runs one level of bounded subdivision. Adjacency degree is at most two
    // (each crossing sits on at most two cells; each cell pairs it with at most one neighbor).
    struct LinkEntry {
      int to = -1;
      FoldChainLink link;
    };
    int ambiguous_total = 0;
    for (int I = 0; I < dim0_cells; I++) {
      for (int J = 0; J < n1; J++) {
        if (cell_exists(I, J) && cell_counts[static_cast<size_t>(I * n1 + J)] >= 3) {
          ambiguous_total++;
        }
      }
    }
    // Sized for the interior refinement crossings the subdivisions may still append (at most
    // one per interior sub-edge, four sub-edges per cell).
    std::vector<std::vector<LinkEntry>> adjacency(static_cast<size_t>(located_count + 4 * ambiguous_total));
    const auto add_link = [&](int a, int b, int I, int J, bool through_subdivision) {
      FoldChainLink link;
      link.cell_i0 = I;
      link.cell_i1 = J;
      link.through_subdivision = through_subdivision;
      adjacency[static_cast<size_t>(a)].push_back({ b, link });
      adjacency[static_cast<size_t>(b)].push_back({ a, link });
    };
    std::vector<bool> ambiguous_cell(static_cast<size_t>(cell_total), false);

    // The nine points of one cell's one-level subdivision: the four corners from the scan's
    // own samples, the four edge midpoints and the center evaluated here (five new
    // evaluations). False when any of the nine is invalid (the cell touches the invalid
    // region). Shared by the ambiguous-cell pairing and the point-image verification.
    const auto eval_cell_nine = [&](int I, int J, bool point_valid[9], double point_j[9],
                                    Sample point_sample[9]) -> bool {
      const int gu0 = StepIndex(axes[0], I, 1);
      const int gv1 = StepIndex(axes[1], J, 1);
      if (gu0 < 0 || gv1 < 0) {
        return false;
      }
      const int corner_grid[9][2] = { { I, J },   { -1, -1 }, { gu0, J }, { -1, -1 },  { -1, -1 },
                                      { -1, -1 }, { I, gv1 }, { -1, -1 }, { gu0, gv1 } };
      for (int idx = 0; idx < 9; idx++) {
        if (corner_grid[idx][0] >= 0) {
          point_sample[idx] = samples[static_cast<size_t>(corner_grid[idx][0] * n1 + corner_grid[idx][1])];
          point_valid[idx] = point_sample[idx].valid;
          point_j[idx] = point_sample[idx].signed_jacobian;
          if (!point_valid[idx]) {
            return false;
          }
        } else {
          const double p[2] = { axes[0].ValueDeg(I) + kCellUnit[idx][0] * axes[0].spacing_deg,
                                axes[1].ValueDeg(J) + kCellUnit[idx][1] * axes[1].spacing_deg };
          if (!EvaluateSample(normals, slots, slot_count, sun_hat, refractive_index, 2, axes, spec, p,
                              &point_sample[idx])) {
            return false;
          }
          point_valid[idx] = true;
          point_j[idx] = point_sample[idx].signed_jacobian;
        }
      }
      return true;
    };

    // One bounded on-locus refinement of a cell: every sign-flipped sub-edge is bisected to a
    // reduced precision (a diameter/alignment read does not need the 52-step closure) and every
    // located locus point is returned with its parameters. These points ARE on the branch —
    // unlike the parameter chord's midpoints, which sit off it (both chord endpoints are J
    // zeros). False when any new evaluation is invalid.
    constexpr int kRefineBisectionSteps = 20;
    const auto refine_cell_locus = [&](int I, int J, std::vector<Sample>* refined_samples,
                                       std::vector<std::array<double, 2>>* refined_params,
                                       std::vector<int>* refined_edges = nullptr) -> bool {
      bool point_valid[9] = {};
      double point_j[9] = {};
      Sample point_sample[9];
      if (!eval_cell_nine(I, J, point_valid, point_j, point_sample)) {
        return false;
      }
      for (int e = 0; e < 12; e++) {
        const CellSubEdge& edge = kCellSubEdges[e];
        if (point_j[edge.a] * point_j[edge.b] >= 0.0) {
          continue;  // no sign change on this half
        }
        const double pa[2] = { axes[0].ValueDeg(I) + kCellUnit[edge.a][0] * axes[0].spacing_deg,
                               axes[1].ValueDeg(J) + kCellUnit[edge.a][1] * axes[1].spacing_deg };
        const double pb[2] = { axes[0].ValueDeg(I) + kCellUnit[edge.b][0] * axes[0].spacing_deg,
                               axes[1].ValueDeg(J) + kCellUnit[edge.b][1] * axes[1].spacing_deg };
        double t_lo = 0.0;
        double t_hi = 1.0;
        bool ok = true;
        for (int step = 0; step < kRefineBisectionSteps; step++) {
          const double t = 0.5 * (t_lo + t_hi);
          const double probe[2] = { pa[0] + t * (pb[0] - pa[0]), pa[1] + t * (pb[1] - pa[1]) };
          Sample mid;
          if (!EvaluateSample(normals, slots, slot_count, sun_hat, refractive_index, 2, axes, spec, probe, &mid)) {
            ok = false;
            break;
          }
          if (mid.signed_jacobian * point_j[edge.a] > 0.0) {
            t_lo = t;
          } else {
            t_hi = t;
          }
        }
        if (!ok) {
          return false;
        }
        const double t = 0.5 * (t_lo + t_hi);
        const double probe[2] = { pa[0] + t * (pb[0] - pa[0]), pa[1] + t * (pb[1] - pa[1]) };
        Sample located;
        if (!EvaluateSample(normals, slots, slot_count, sun_hat, refractive_index, 2, axes, spec, probe, &located)) {
          return false;
        }
        refined_samples->push_back(located);
        refined_params->push_back({ probe[0], probe[1] });
        if (refined_edges != nullptr) {
          refined_edges->push_back(e);
        }
      }
      return true;
    };

    // The continuity verification of a two-crossing cell's pairing (review: the most common
    // cell must not pair blindly): the cell's on-locus subdivision (the same nine points and
    // twelve sub-edges, reduced-precision bisections, nothing appended) must CONNECT the two
    // boundary crossings through its sub-cell pairings — a feasibility strip crossing the cell
    // can cut the locus into two unrelated pieces that each touch the boundary once, and only
    // the subdivision's own adjacency can tell them apart. Unverifiable (an invalid probe, a
    // missing re-found root, an unresolved subcell) refuses the pairing: the cell marks
    // ambiguous and the chains break with the reason kept — never a fabricated link.
    const auto verify_cell_pair = [&](int I, int J, int a, int b) -> bool {
      std::vector<Sample> refined_samples;
      std::vector<std::array<double, 2>> refined_params;
      std::vector<int> refined_edges;
      if (!refine_cell_locus(I, J, &refined_samples, &refined_params, &refined_edges)) {
        return false;
      }
      const int m = static_cast<int>(refined_samples.size());
      if (m < 2) {
        return false;
      }
      // Identify the two terminals among the located points (their full edges).
      const FoldPointRecord& pa = out.crossings[static_cast<size_t>(a)];
      const FoldPointRecord& pb = out.crossings[static_cast<size_t>(b)];
      const auto edge_matches = [&](const FoldPointRecord& p, const CellSubEdge& edge) {
        if (edge.side < 0) {
          return false;
        }
        const EdgeRef full = edge.side == 0 ? EdgeRef{ 0, I, J } :
                             edge.side == 1 ? EdgeRef{ 0, I, StepIndex(axes[1], J, 1) } :
                             edge.side == 2 ? EdgeRef{ 1, I, J } :
                                              EdgeRef{ 1, StepIndex(axes[0], I, 1), J };
        return full.dim == p.edge_dim && full.i0 == p.edge_i0 && full.i1 == p.edge_i1;
      };
      int node_a = -1;
      int node_b = -1;
      for (int i = 0; i < m; i++) {
        if (edge_matches(pa, kCellSubEdges[static_cast<size_t>(refined_edges[static_cast<size_t>(i)])])) {
          node_a = i;
        }
        if (edge_matches(pb, kCellSubEdges[static_cast<size_t>(refined_edges[static_cast<size_t>(i)])])) {
          node_b = i;
        }
      }
      if (node_a < 0 || node_b < 0 || node_a == node_b) {
        return false;
      }
      // Sub-cell occupancy over the located points, pairings unioned (a subcell with 3+
      // members stays unresolved and refuses the verification).
      int sub_members[4][4];
      int sub_counts[4] = { 0, 0, 0, 0 };
      const auto add_to_subcell = [&](int su, int sv, int node) {
        int& count = sub_counts[su * 2 + sv];
        if (count < 4) {
          sub_members[su * 2 + sv][count++] = node;
        }
      };
      for (int i = 0; i < m; i++) {
        const CellSubEdge& edge = kCellSubEdges[static_cast<size_t>(refined_edges[static_cast<size_t>(i)])];
        if (edge.dim == 0) {
          const int row = edge.a / 3;
          const int su = (edge.a % 3 == 0) ? 0 : 1;
          if (row == 0) {
            add_to_subcell(su, 0, i);
          } else if (row == 2) {
            add_to_subcell(su, 1, i);
          } else {
            add_to_subcell(su, 0, i);
            add_to_subcell(su, 1, i);
          }
        } else {
          const int column = edge.a % 3;
          const int sv = edge.a / 3;
          if (column == 0) {
            add_to_subcell(0, sv, i);
          } else if (column == 2) {
            add_to_subcell(1, sv, i);
          } else {
            add_to_subcell(0, sv, i);
            add_to_subcell(1, sv, i);
          }
        }
      }
      DisjointSet set(m);
      for (int s = 0; s < 4; s++) {
        if (sub_counts[s] == 2) {
          set.Union(sub_members[s][0], sub_members[s][1]);
        } else if (sub_counts[s] >= 3) {
          return false;
        }
      }
      return set.Find(node_a) == set.Find(node_b);
    };

    // One level of bounded subdivision of an ambiguous cell: the four edge midpoints and the
    // center are evaluated, the twelve sub-edges are scanned for sign changes, and the four
    // sub-cells pair what they hold exactly two of. A sub-crossing on a boundary half maps onto
    // the cell's existing full-edge crossing (the same unique root — bisection converges to the
    // one sign change inside any bracket of it); an interior sub-crossing appends to the
    // crossings (edge fields -1). Returns false — pairing nothing, the cell marked ambiguous —
    // when any new evaluation is invalid, one full edge shows two roots (the one-edge-two-
    // crossings case the contract declares out of scope), or a subcell still holds three or
    // more: nothing pairs, the fragments break with kAmbiguousCell.
    const auto subdivide_cell = [&](int I, int J) -> bool {
      const int gu0 = StepIndex(axes[0], I, 1);
      const int gv1 = StepIndex(axes[1], J, 1);
      if (gu0 < 0 || gv1 < 0) {
        return false;
      }
      bool point_valid[9] = {};
      double point_j[9] = {};
      Sample point_sample[9];
      if (!eval_cell_nine(I, J, point_valid, point_j, point_sample)) {
        return false;  // the cell touches (or probed) the invalid region: unresolved, nothing pairs
      }
      // Each side's full edge identity: 0 = the dim0 edge at v = 0, 1 = at v = 1, 2 = the dim1
      // edge at u = 0, 3 = at u = 1.
      const EdgeRef side_full_edge[4] = { EdgeRef{ 0, I, J }, EdgeRef{ 0, I, gv1 }, EdgeRef{ 1, I, J },
                                          EdgeRef{ 1, gu0, J } };
      int full_edge_roots[4] = { 0, 0, 0, 0 };
      int sub_crossing[12];
      for (int e = 0; e < 12; e++) {
        sub_crossing[e] = -1;
        const CellSubEdge& edge = kCellSubEdges[e];
        if (point_j[edge.a] * point_j[edge.b] >= 0.0) {
          continue;  // no sign change on this half
        }
        const double pa[2] = { axes[0].ValueDeg(I) + kCellUnit[edge.a][0] * axes[0].spacing_deg,
                               axes[1].ValueDeg(J) + kCellUnit[edge.a][1] * axes[1].spacing_deg };
        const double pb[2] = { axes[0].ValueDeg(I) + kCellUnit[edge.b][0] * axes[0].spacing_deg,
                               axes[1].ValueDeg(J) + kCellUnit[edge.b][1] * axes[1].spacing_deg };
        double t = 0.0;
        Sample located;
        if (!bisect_edge(pa, pb, point_j[edge.a], &t, &located)) {
          return false;
        }
        if (edge.side >= 0) {
          if (full_edge_roots[edge.side] > 0) {
            return false;  // two roots on one full edge — the out-of-scope case, unresolved
          }
          full_edge_roots[edge.side]++;
          const int orig = edge_crossing[static_cast<size_t>(edge_key(side_full_edge[edge.side]))];
          if (orig < 0) {
            return false;  // a boundary root without the full-edge crossing: unresolved
          }
          sub_crossing[e] = orig;
        } else {
          sub_crossing[e] = record_crossing(EdgeRef{ -1, -1, -1 }, false, pa, pb, t, located);
        }
      }
      // Sub-cell occupancy: sub-cell (su, sv) holds the sub-crossings on its four boundary
      // halves; exactly two pair (through_subdivision), three or more stay unresolved.
      int sub_members[4][4];
      int sub_counts[4] = { 0, 0, 0, 0 };
      const auto add_to_subcell = [&](int su, int sv, int crossing) {
        int& count = sub_counts[su * 2 + sv];
        if (count < 4) {
          sub_members[su * 2 + sv][count++] = crossing;
        }
      };
      for (int e = 0; e < 12; e++) {
        if (sub_crossing[e] < 0) {
          continue;
        }
        const CellSubEdge& edge = kCellSubEdges[e];
        if (edge.dim == 0) {
          const int row = edge.a / 3;                // the unit row: 0, 1, 2
          const int su = (edge.a % 3 == 0) ? 0 : 1;  // left half or right half
          if (row == 0) {
            add_to_subcell(su, 0, sub_crossing[e]);
          } else if (row == 2) {
            add_to_subcell(su, 1, sub_crossing[e]);
          } else {
            add_to_subcell(su, 0, sub_crossing[e]);
            add_to_subcell(su, 1, sub_crossing[e]);
          }
        } else {
          const int column = edge.a % 3;  // the unit column: 0, 1, 2
          const int sv = edge.a / 3;      // lower half or upper half
          if (column == 0) {
            add_to_subcell(0, sv, sub_crossing[e]);
          } else if (column == 2) {
            add_to_subcell(1, sv, sub_crossing[e]);
          } else {
            add_to_subcell(0, sv, sub_crossing[e]);
            add_to_subcell(1, sv, sub_crossing[e]);
          }
        }
      }
      for (int su = 0; su < 2; su++) {
        for (int sv = 0; sv < 2; sv++) {
          const int count = sub_counts[su * 2 + sv];
          if (count == 2) {
            add_link(sub_members[su * 2 + sv][0], sub_members[su * 2 + sv][1], I, J, true);
          } else if (count >= 3) {
            return false;
          }
        }
      }
      return true;
    };

    bool pairing_cut = false;
    for (int I = 0; I < dim0_cells && !pairing_cut; I++) {
      for (int J = 0; J < n1; J++) {
        if (!cell_exists(I, J)) {
          continue;
        }
        // The pairing is the recovery's dominant new work (the verification's nine points and
        // reduced bisections, the subdivision's): one poll per cell, the bounded unit boundary.
        if (stopped()) {
          pairing_cut = true;  // the cells past the cut stay unpaired: the walk reads the paired ones
          break;
        }
        const size_t cell = static_cast<size_t>(I * n1 + J);
        const int count = cell_counts[cell];
        if (count == 2) {
          if (verify_cell_pair(I, J, cell_members[cell][0], cell_members[cell][1])) {
            add_link(cell_members[cell][0], cell_members[cell][1], I, J, false);
          } else {
            ambiguous_cell[cell] = true;
          }
        } else if (count >= 3) {
          if (!subdivide_cell(I, J)) {
            ambiguous_cell[cell] = true;
          }
        }
      }
    }

    // ---- the image-form classification of one chain -----------------------------------------
    // Per order position: one Jet2 read (the exact second-order read of the SAME chain — the
    // value parts, the restricted Jacobian's kernel direction, and the signed Jacobian's exact
    // gradient and Hessian), the chain tangent's alignment with the kernel, the transversality
    // read dJ(v) against a continuously oriented kernel, and the TRUE-tangent image speed —
    // the classification's arbiter. Within runs of aligned points (the locator only), windows
    // whose motion evidence stays consistent with a constant image at the window's own
    // positioning resolution are verified by their measured sky diameter with one midpoint
    // refinement level; what passes is a kPointImage segment, what resolvably moves is an arc,
    // the undecidable band and the unevaluated positions say kUnknown. Cusp candidates are
    // located sign-change zeros of dJ(v), searched on arc segments only.
    const auto classify_chain = [&](FoldChainRecord* chain) {
      const int n = static_cast<int>(chain->order.size());
      if (n == 0) {
        return;
      }
      std::vector<FoldJet2Read> reads(static_cast<size_t>(n));
      std::vector<double> vhat(static_cast<size_t>(n) * 2, 0.0);
      std::vector<double> tauhat(static_cast<size_t>(n) * 2, 0.0);
      std::vector<double> align(static_cast<size_t>(n), -1.0);
      std::vector<double> djv(static_cast<size_t>(n), 0.0);
      // The true-tangent image-speed read (the classification's own instrument, the review's
      // criterion): the J = 0 level curve's direction pushed through the restricted Jacobian
      // rows, and the speed's own resolution — the positioning ambiguity plus the rounding
      // floor. Zero speed means the critical set's own tangent lies in the map kernel: the
      // image cannot move along the branch THERE, whatever the finite chord does.
      std::vector<double> image_speed(static_cast<size_t>(n), 0.0);
      std::vector<double> image_speed_res(static_cast<size_t>(n), 0.0);
      std::vector<char> read_ok(static_cast<size_t>(n), 0);
      for (int k = 0; k < n; k++) {
        if (stopped()) {
          break;  // the positions past the cut keep their place unclassified (kUnknown below)
        }
        FoldJet2Read& read = reads[static_cast<size_t>(k)];
        FoldPointRecord& point = out.crossings[static_cast<size_t>(chain->order[static_cast<size_t>(k)])];
        if (!EvaluateSampleJet2(normals, slots, slot_count, sun_hat, refractive_index, axes, spec, point.params_deg,
                                &read)) {
          continue;
        }
        // The local rank-1 qualification (issue: transversality must gate, not just record): a
        // read qualifies as a fold point only with sigma1 above the module's own rank bar AND
        // the singular ratio under the declared located-fold bar — a real crossing bisects to
        // sigma2/sigma1 in the 1e-16..1e-6 band, while a validity cliff's sign jump lands
        // orders above (its "crossing" is a discontinuity, not a fold). Unqualified points
        // keep their chain place with unclassified reads; no cusp logic consumes them.
        if (read.sigma1 < kFamilyRankSigmaTolerance || read.sigma2 > read.sigma1 * kFoldLocalRankRatio) {
          continue;
        }
        double v[2];
        if (!KernelDirection(read.rows, read.outgoing, v)) {
          continue;  // rank ~ 0 at this point: no fold read, the position stays unclassified
        }
        read_ok[static_cast<size_t>(k)] = 1;
        // The chain tangent from the neighbours (a closed chain wraps; an open chain's ends go
        // one-sided by clamping).
        const int kp = chain->closed ? (k - 1 + n) % n : std::max(k - 1, 0);
        const int kn = chain->closed ? (k + 1) % n : std::min(k + 1, n - 1);
        const FoldPointRecord& pp = out.crossings[static_cast<size_t>(chain->order[static_cast<size_t>(kp)])];
        const FoldPointRecord& pn = out.crossings[static_cast<size_t>(chain->order[static_cast<size_t>(kn)])];
        double tau[2];
        ChainTangent(axes, pp.params_deg, pn.params_deg, tau);
        const double tau_norm = std::sqrt(tau[0] * tau[0] + tau[1] * tau[1]);
        if (!(tau_norm > 0.0)) {
          read_ok[static_cast<size_t>(k)] = 0;  // degenerate spacing: no direction, no read
          continue;
        }
        tauhat[static_cast<size_t>(k) * 2] = tau[0] / tau_norm;
        tauhat[static_cast<size_t>(k) * 2 + 1] = tau[1] / tau_norm;
        // Orient the kernel along the chain (continuity against the previous point's kernel)
        // so the SIGN of dJ(v) is meaningful along the chain; the alignment takes |.| and is
        // orientation-free.
        if (k > 0 && read_ok[static_cast<size_t>(k - 1)]) {
          const double d =
              v[0] * vhat[static_cast<size_t>(k - 1) * 2] + v[1] * vhat[static_cast<size_t>(k - 1) * 2 + 1];
          if (d < 0.0) {
            v[0] = -v[0];
            v[1] = -v[1];
          }
        }
        vhat[static_cast<size_t>(k) * 2] = v[0];
        vhat[static_cast<size_t>(k) * 2 + 1] = v[1];
        align[static_cast<size_t>(k)] =
            std::fabs(tauhat[static_cast<size_t>(k) * 2] * v[0] + tauhat[static_cast<size_t>(k) * 2 + 1] * v[1]);
        djv[static_cast<size_t>(k)] = read.grad_jacobian[0] * v[0] + read.grad_jacobian[1] * v[1];
        // The speed read (orientation-free — the |rows . tau_true| magnitude, never a signed
        // chain quantity): tau_true is the J = 0 level curve's own direction, perpendicular to
        // the gradient in the parameter plane.
        const double g =
            std::sqrt(read.grad_jacobian[0] * read.grad_jacobian[0] + read.grad_jacobian[1] * read.grad_jacobian[1]);
        if (g > 0.0) {
          const double tau_true[2] = { -read.grad_jacobian[1] / g, read.grad_jacobian[0] / g };
          double w[3] = { 0.0, 0.0, 0.0 };
          for (int c = 0; c < 3; c++) {
            for (int p = 0; p < 2; p++) {
              w[c] += read.rows[p][c] * tau_true[p];
            }
          }
          image_speed[static_cast<size_t>(k)] = std::sqrt(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
        }
        image_speed_res[static_cast<size_t>(k)] =
            std::fabs(read.signed_jacobian) + kFoldImageSpeedRounding * read.sigma1;
        point.alignment = align[static_cast<size_t>(k)];
        point.djv = djv[static_cast<size_t>(k)];
        point.djv_resolution = DjvResolution(read);
      }
      // Candidate runs (locator only — the diameter verification is the arbiter). Closed
      // chains rotate so no run straddles the seam (a closed chain has no distinguished end;
      // the rotation is semantically neutral and keeps every index below linear).
      std::vector<char> candidate(static_cast<size_t>(n), 0);
      for (int k = 0; k < n; k++) {
        candidate[static_cast<size_t>(k)] =
            read_ok[static_cast<size_t>(k)] && align[static_cast<size_t>(k)] >= 1.0 - kFoldPointImageLocatorSlack ? 1 :
                                                                                                                    0;
      }
      if (chain->closed && candidate[static_cast<size_t>(0)] && candidate[static_cast<size_t>(n - 1)]) {
        int rotation = -1;
        for (int k = 0; k < n; k++) {
          if (!candidate[static_cast<size_t>(k)]) {
            rotation = k;
            break;
          }
        }
        if (rotation > 0) {
          RotateVector(&chain->order, rotation);
          RotateVector(&chain->links, rotation);
          RotateVector(&reads, rotation);
          RotateVector(&vhat, rotation);
          RotateVector(&tauhat, rotation);
          RotateVector(&align, rotation);
          RotateVector(&djv, rotation);
          RotateVector(&image_speed, rotation);
          RotateVector(&image_speed_res, rotation);
          RotateVector(&read_ok, rotation);
          RotateVector(&candidate, rotation);
        }
      }
      struct Run {
        int first;
        int last;
      };
      std::vector<Run> runs;
      // The verification's own readings, keyed by each run's first position (the assembly
      // reads them back when it builds the segments — one evidence, one publication). The
      // form itself always reads the split's own array; the stats ride only where a strict
      // sub-run was actually verified.
      std::vector<double> run_diameter(static_cast<size_t>(n), 0.0);
      std::vector<double> run_bound(static_cast<size_t>(n), -1.0);
      std::vector<char> run_verified(static_cast<size_t>(n), 0);
      {
        int k = 0;
        while (k < n) {
          if (candidate[static_cast<size_t>(k)]) {
            int j = k;
            while (j < n && candidate[static_cast<size_t>(j)]) {
              j++;
            }
            if (j - k >= 2) {
              runs.push_back({ k, j - 1 });
            }
            k = j;
          } else {
            k++;
          }
        }
      }
      // The point-image windows: the classification's arbiter is the critical set's TRUE
      // tangent read (the chord alignment above is the locator's approximation, never the
      // verdict — a finite chord misaligns through ordinary curvature, which is not image
      // motion). Within each candidate run, a window [s..e] claims a constant image only
      // while its motion evidence stays consistent with the claim at the window's own
      // resolution:
      //   motion: extent(s..e) x max resolvable speed(s..e) <= B_pos(s..e)
      // A speed counts only above its own resolution (image_speed_res: the located point's
      // positioning ambiguity |J| plus the Jet2 rounding floor on the rows); extent is the
      // sum of the UNWRAPPED per-adjacency parameter displacements (the real parameter arc —
      // a normalized tangent never doubles as a length source, every non-degenerate adjacency
      // would otherwise read exactly one degree); B_pos is the window's positioning bound:
      // twice the worst image offset sigma1 x |J| / grad_min over the window's points, plus
      // the unit-sphere rounding floor. The motion bound compares against the POSITIONING
      // bound deliberately, not the instrument-inclusive bound the diameter check uses below:
      // a drift absorbed only by the refinement's own noise budget is not a collapse the
      // instrument resolved (the near-degenerate counterexample's per-gap drift ~7e-7 deg
      // against a ~1e-12 positioning bound reads arc; the exact mirror identity's speed sits
      // under its resolution, the drift bound stays zero, and the whole core verifies as the
      // point-image segment the plan's own fact statement describes — the middle collapses,
      // the extended ends read arc by their own resolvable speed).
      // B_pos past the claim ceiling grounds nothing: the position reads kUnknown. Windows
      // shorter than two positions are no segment. The greedy scan grows the longest window
      // from each position and restarts after it — deterministic, and every position ends
      // inside a verified window or left at arc/unknown by its own evidence.
      std::vector<FoldSegmentForm> form(static_cast<size_t>(n), FoldSegmentForm::kArc);
      for (int k = 0; k < n; k++) {
        if (!read_ok[static_cast<size_t>(k)]) {
          form[static_cast<size_t>(k)] = FoldSegmentForm::kUnknown;
        }
      }
      const auto verify_window = [&](int run_first, int run_last) {
        const Run run{ run_first, run_last };
        std::vector<std::array<double, 3>> dirs;
        dirs.reserve(static_cast<size_t>(run.last - run.first) * 2 + 1);
        for (int k = run.first; k <= run.last; k++) {
          const FoldPointRecord& point = out.crossings[static_cast<size_t>(chain->order[static_cast<size_t>(k)])];
          std::array<double, 3> dir;
          ImageSkyDir(point.outgoing, dir.data());
          dirs.push_back(dir);
        }
        bool mids_ok = true;
        // The on-locus refinement ("加密一致性"): the parameter chord's midpoint is NOT on the
        // branch (both chord endpoints are J zeros), so the finer sampling takes the shared
        // link cell's on-locus subdivision — its interior sub-crossings sit on the branch — at
        // up to kFoldRunRefineGaps gaps evenly spaced along the window. Each refined point
        // joins the diameter measure and re-reads its kernel alignment against the gap's
        // chord direction (the gross locator consistency — the refined midpoints' own
        // 20-step-reduced bisection locates them an order coarser than the crossings' 52-step
        // one, so no tangency verdict rides them; a gross misalignment still cuts the
        // verification to kUnknown).
        int gap_count = run.last - run.first;
        const int refine_gaps = std::min(gap_count, kFoldRunRefineGaps);
        std::vector<Sample> refined_samples;
        std::vector<std::array<double, 2>> refined_params;
        for (int g = 0; g < refine_gaps && mids_ok; g++) {
          // A cut refinement is an unfinished verification: the window's verdict stays kUnknown.
          if (stopped()) {
            mids_ok = false;
            break;
          }
          const int k = refine_gaps == 1 ? run.first : run.first + g * (run.last - run.first) / (refine_gaps - 1);
          if (k >= run.last) {
            continue;
          }
          const FoldChainLink& link = chain->links[static_cast<size_t>(k)];
          refined_samples.clear();
          refined_params.clear();
          if (!refine_cell_locus(link.cell_i0, link.cell_i1, &refined_samples, &refined_params)) {
            mids_ok = false;
            break;
          }
          const FoldPointRecord& pa = out.crossings[static_cast<size_t>(chain->order[static_cast<size_t>(k)])];
          const FoldPointRecord& pb = out.crossings[static_cast<size_t>(chain->order[static_cast<size_t>(k + 1)])];
          double tau[2];
          ChainTangent(axes, pa.params_deg, pb.params_deg, tau);
          const double tau_norm = std::sqrt(tau[0] * tau[0] + tau[1] * tau[1]);
          if (!(tau_norm > 0.0)) {
            mids_ok = false;
            break;
          }
          for (size_t r = 0; r < refined_samples.size(); r++) {
            double v[2];
            if (!KernelDirection(refined_samples[r].rows, refined_samples[r].outgoing, v)) {
              mids_ok = false;
              break;
            }
            const double refined_align = std::fabs((tau[0] / tau_norm) * v[0] + (tau[1] / tau_norm) * v[1]);
            if (refined_align < 1.0 - kFoldPointImageLocatorSlack) {
              mids_ok = false;
              break;
            }
            std::array<double, 3> dir;
            ImageSkyDir(refined_samples[r].outgoing, dir.data());
            dirs.push_back(dir);
          }
        }
        FoldSegmentForm verdict = FoldSegmentForm::kUnknown;
        double verdict_diameter = 0.0;
        double verdict_bound = -1.0;
        if (mids_ok) {
          double diameter = 0.0;
          for (size_t i = 0; i < dirs.size(); i++) {
            for (size_t j = i + 1; j < dirs.size(); j++) {
              diameter = std::max(diameter, SkyAngleDeg(dirs[i].data(), dirs[j].data()));
            }
          }
          // The declared bound and the direct consistency check. UNITS: the Jet2 variables
          // are the parameters in RADIANS (degrees enter through kDegToRad), so grad_jacobian
          // is dJ/d(radian) and |J| / |grad J| is a RADIAN distance — converted to the
          // record's degree frame at the computation site, never compared raw against a
          // *_deg bar (review: an unconverted distance reads ~57x too small against a degree
          // bar). The bound covers (a) the window's points' positioning offsets, (b) the
          // refined midpoints' own (the run's minimum gradient carries theirs), (c) the
          // unit-sphere rounding floor, and (d) the refinement's own positioning bracket —
          // the midpoints are located by the reduced 20-step bisection of half-spacing
          // sub-edges, an order coarser than the crossings' 52-step one; their image spread
          // enters the measured diameter, so the declared bound must cover what the diameter
          // measures. The gates: the bound past the claim ceiling reads kUnknown (the
          // evidence cannot ground any point claim); the measured diameter past the ceiling
          // or past the window's OWN declared bound reads arc — a measured spread the
          // declared resolution excludes is a resolvably moving image (the review's direct
          // consistency check, never replaced by the motion-span prediction).
          double grad_min = 0.0;
          bool have_grad = false;
          for (int k = run.first; k <= run.last; k++) {
            const FoldJet2Read& r = reads[static_cast<size_t>(k)];
            const double g =
                std::sqrt(r.grad_jacobian[0] * r.grad_jacobian[0] + r.grad_jacobian[1] * r.grad_jacobian[1]);
            if (!have_grad || g < grad_min) {
              grad_min = g;
              have_grad = true;
            }
          }
          double derived_bound = 0.0;
          for (int k = run.first; k <= run.last; k++) {
            const FoldJet2Read& r = reads[static_cast<size_t>(k)];
            // |J| / |grad J| is a radian distance; the record's frame is degrees.
            const double off_deg = grad_min > 0.0 ? std::fabs(r.signed_jacobian) / grad_min * kRadToDeg : 0.0;
            derived_bound = std::max(derived_bound, 2.0 * r.sigma1 * off_deg);
          }
          for (const Sample& r : refined_samples) {
            // The refined midpoints carry no gradient of their own (a Sample, not a Jet2
            // read): the window's own minimum gradient carries theirs, as before.
            const double off_deg = grad_min > 0.0 ? std::fabs(r.signed_jacobian) / grad_min * kRadToDeg : 0.0;
            derived_bound = std::max(derived_bound, 2.0 * r.sigma1 * off_deg);
          }
          derived_bound += 1e-12;
          const double refine_bracket_deg = axes[0].spacing_deg * 0.5 / (1 << 20);
          double sigma1_max = 0.0;
          for (int k = run.first; k <= run.last; k++) {
            sigma1_max = std::max(sigma1_max, reads[static_cast<size_t>(k)].sigma1);
          }
          derived_bound += 2.0 * sigma1_max * refine_bracket_deg;
          if (derived_bound > kFoldPointImageFloorDeg) {
            verdict = FoldSegmentForm::kUnknown;  // the window's own evidence cannot ground a point claim
          } else if (diameter > kFoldPointImageFloorDeg) {
            verdict = FoldSegmentForm::kArc;
          } else if (diameter > derived_bound) {
            // The measured spread exceeds the window's own declared resolution: a
            // resolvably moving image, whatever the absolute size.
            verdict = FoldSegmentForm::kArc;
          } else {
            // Constant within the window's own declared resolution, the measured span
            // inside the evidence's reach, the motion span inside the positioning bound
            // (the scan's own condition): the point-image claim, bound published with it.
            verdict = FoldSegmentForm::kPointImage;
          }
          verdict_diameter = diameter;
          verdict_bound = derived_bound;
        }
        for (int k = run.first; k <= run.last; k++) {
          form[static_cast<size_t>(k)] = verdict;
        }
        // The verification's own readings ride to the assembly (review: the published values
        // must BE the classified ones, not a re-derivation over a coarser sample set).
        run_diameter[static_cast<size_t>(run.first)] = verdict_diameter;
        run_bound[static_cast<size_t>(run.first)] = verdict_bound;
        run_verified[static_cast<size_t>(run.first)] = 1;
      };
      for (const Run& run : runs) {
        int s = run.first;
        while (s + 1 <= run.last) {
          double extent_deg = 0.0;
          double max_speed = 0.0;
          double sigma1_max = 0.0;
          double jac_max = 0.0;
          double grad_min = 0.0;
          bool have_grad = false;
          int best_e = -1;
          bool cap_blind = false;
          for (int e = s; e <= run.last; e++) {
            const FoldJet2Read& r = reads[static_cast<size_t>(e)];
            sigma1_max = std::max(sigma1_max, r.sigma1);
            jac_max = std::max(jac_max, std::fabs(r.signed_jacobian));
            const double g =
                std::sqrt(r.grad_jacobian[0] * r.grad_jacobian[0] + r.grad_jacobian[1] * r.grad_jacobian[1]);
            if (g > 0.0 && (!have_grad || g < grad_min)) {
              grad_min = g;
              have_grad = true;
            }
            if (image_speed[static_cast<size_t>(e)] > image_speed_res[static_cast<size_t>(e)]) {
              max_speed = std::max(max_speed, image_speed[static_cast<size_t>(e)]);
            }
            if (e > s) {
              const FoldPointRecord& pp = out.crossings[static_cast<size_t>(chain->order[static_cast<size_t>(e - 1)])];
              const FoldPointRecord& pn = out.crossings[static_cast<size_t>(chain->order[static_cast<size_t>(e)])];
              double d[2];
              ChainParamDelta(axes, pp.params_deg, pn.params_deg, d);
              extent_deg += std::sqrt(d[0] * d[0] + d[1] * d[1]);
              const double pos_bound = have_grad && grad_min > 0.0 ? 2.0 * sigma1_max * jac_max / grad_min + 1e-12 :
                                                                     kFoldPointImageFloorDeg * 2.0;
              if (pos_bound > kFoldPointImageFloorDeg) {
                cap_blind = true;  // this window start's own evidence cannot ground any point claim
                break;
              }
              const double span_deg = extent_deg * kDegToRad * max_speed * kRadToDeg;
              if (span_deg <= pos_bound) {
                best_e = e;  // the longest consistent window so far; keep growing
              } else if (best_e >= 0) {
                break;  // the window stopped growing; verify the last consistent prefix
              }
            }
          }
          if (best_e >= 0) {
            verify_window(s, best_e);  // best_e >= s + 1: a first qualification needs two positions
            s = best_e + 1;
          } else {
            if (cap_blind) {
              form[static_cast<size_t>(s)] = FoldSegmentForm::kUnknown;
            }
            s++;
          }
        }
      }
      // The cusp candidates: located sign-change zeros of dJ(v) on arc positions only. The
      // locator bisects the parameter chord between two adjacent chain points (the located
      // spacing — the chord is the bracket, not a claim about the curve's shape).
      const auto locate_cusp = [&](int ka, int kb) -> bool {
        const FoldPointRecord& pa = out.crossings[static_cast<size_t>(chain->order[static_cast<size_t>(ka)])];
        const FoldPointRecord& pb = out.crossings[static_cast<size_t>(chain->order[static_cast<size_t>(kb)])];
        double lo[2] = { pa.params_deg[0], pa.params_deg[1] };
        double hi[2] = { pb.params_deg[0], pb.params_deg[1] };
        for (int k = 0; k < 2; k++) {
          if (axes[k].periodic) {
            const double d = hi[k] - lo[k];
            if (d > 180.0) {
              hi[k] -= 360.0;
            } else if (d < -180.0) {
              hi[k] += 360.0;
            }
          }
        }
        const double reference[2] = { vhat[static_cast<size_t>(ka) * 2], vhat[static_cast<size_t>(ka) * 2 + 1] };
        // One PROJECTED transversality read at the parameter point `p` (degrees): evaluate the
        // Jet2 read, project it onto the locus (Newton on J = 0 along the same read's exact
        // gradient), and return dJ(v) AT THE PROJECTED POINT with the chain's kernel
        // orientation. The chord zero of the raw dJ(v) is generally OFF the branch (both chord
        // endpoints are J zeros; the chord's interior is not), so bisection reads the
        // transversality of the branch itself — the review's own locator (continuation /
        // joint solving on the restored critical branch), not a parameter-chord zero. UNITS:
        // the Jet2 variables are the parameters in RADIANS, so grad_jacobian is dJ/d(radian);
        // both the Newton step and the first-order distance come out in radians and are
        // converted to the degree frame at the site they touch params_deg or a *_deg bar —
        // never compared raw (the review's unit audit: an unconverted distance reads ~57x too
        // small against a degree bar). The projection's own gate is the first-order distance
        // |J| / |grad J| in degrees under kFoldCuspOnLocusDistanceDeg (the module's finest
        // declared geometric scale; the review's chord-zero artifact measured 1.35e-3 deg out,
        // full-rank — this bar and the rank gate both refuse it). A probe whose projection
        // fails (an invalid sample, no convergent projection) leaves the interval unknown:
        // false, no event invented.
        const auto projected_djv = [&](double p[2], FoldJet2Read* read_out) -> bool {
          FoldJet2Read read;
          double v[2];
          if (!EvaluateSampleJet2(normals, slots, slot_count, sun_hat, refractive_index, axes, spec, p, &read) ||
              !KernelDirection(read.rows, read.outgoing, v)) {
            return false;
          }
          const auto off_locus_distance_deg = [&read]() {
            const double g_norm = std::sqrt(read.grad_jacobian[0] * read.grad_jacobian[0] +
                                            read.grad_jacobian[1] * read.grad_jacobian[1]);
            return g_norm > 0.0 ? std::fabs(read.signed_jacobian) / g_norm * kRadToDeg :
                                  std::numeric_limits<double>::infinity();
          };
          for (int step = 0; step < 4 && off_locus_distance_deg() > kFoldCuspOnLocusDistanceDeg; step++) {
            const double g_norm_sq =
                read.grad_jacobian[0] * read.grad_jacobian[0] + read.grad_jacobian[1] * read.grad_jacobian[1];
            if (!(g_norm_sq > 0.0)) {
              return false;  // no gradient: the projection cannot move
            }
            // The Newton step is a radian displacement; the parameter lives in degrees.
            const double newton_deg = read.signed_jacobian / g_norm_sq * kRadToDeg;
            p[0] -= newton_deg * read.grad_jacobian[0];
            p[1] -= newton_deg * read.grad_jacobian[1];
            if (!EvaluateSampleJet2(normals, slots, slot_count, sun_hat, refractive_index, axes, spec, p, &read) ||
                !KernelDirection(read.rows, read.outgoing, v)) {
              return false;
            }
          }
          if (off_locus_distance_deg() > kFoldCuspOnLocusDistanceDeg) {
            return false;
          }
          // The projected position must BE a fold point — a generic full-rank position is
          // refused, never published as a candidate.
          if (read.sigma1 < kFamilyRankSigmaTolerance || read.sigma2 > read.sigma1 * kFoldLocalRankRatio) {
            return false;
          }
          if (v[0] * reference[0] + v[1] * reference[1] < 0.0) {
            v[0] = -v[0];
            v[1] = -v[1];
          }
          *read_out = read;
          return true;
        };
        double t_lo = 0.0;
        double t_hi = 1.0;
        bool ok = true;
        double probe[2] = { 0.0, 0.0 };
        FoldJet2Read read{};
        for (int step = 0; step < kFoldBisectionSteps; step++) {
          const double t = 0.5 * (t_lo + t_hi);
          double at[2] = { lo[0] + t * (hi[0] - lo[0]), lo[1] + t * (hi[1] - lo[1]) };
          if (!projected_djv(at, &read)) {
            ok = false;
            break;
          }
          probe[0] = at[0];
          probe[1] = at[1];
          // The projected transversality's sign against the bracket's own reference read.
          double v[2];
          if (!KernelDirection(read.rows, read.outgoing, v)) {
            ok = false;
            break;
          }
          if (v[0] * reference[0] + v[1] * reference[1] < 0.0) {
            v[0] = -v[0];
            v[1] = -v[1];
          }
          const double d = read.grad_jacobian[0] * v[0] + read.grad_jacobian[1] * v[1];
          if (d * djv[static_cast<size_t>(ka)] > 0.0) {
            t_lo = t;
          } else {
            t_hi = t;
          }
        }
        if (!ok) {
          return false;
        }
        // The final probe is the bisection midpoint's projected position — ON the locus (the
        // projection's own bar) with its transversality read there. The publish gate: the
        // residual genuinely unresolvable at that point (a residual above the read's own
        // resolution means this interval's sign change never met the branch — no event).
        {
          double at[2] = { lo[0] + 0.5 * (t_lo + t_hi) * (hi[0] - lo[0]),
                           lo[1] + 0.5 * (t_lo + t_hi) * (hi[1] - lo[1]) };
          if (!projected_djv(at, &read)) {
            return false;
          }
          probe[0] = at[0];
          probe[1] = at[1];
        }
        double v[2];
        if (!KernelDirection(read.rows, read.outgoing, v)) {
          return false;
        }
        if (v[0] * reference[0] + v[1] * reference[1] < 0.0) {
          v[0] = -v[0];
          v[1] = -v[1];
        }
        const double d = read.grad_jacobian[0] * v[0] + read.grad_jacobian[1] * v[1];
        if (std::fabs(d) > DjvResolution(read)) {
          return false;
        }
        FoldCuspCandidate located;
        located.order_index = ka;
        for (int k = 0; k < 2; k++) {
          located.params_deg[k] = axes[k].periodic ? std::fmod(probe[k] + 360.0, 360.0) : probe[k];
        }
        double degrees[3];
        DimensionDegrees(spec, axes, 2, probe, degrees);
        EulerPose(degrees[1] * kDegToRad, degrees[0] * kDegToRad, degrees[2] * kDegToRad, located.pose);
        ImageAltAzDeg(read.outgoing, &located.image_alt_deg, &located.image_az_deg);
        located.djv_residual = std::fabs(d);
        chain->cusps.push_back(located);
        return true;
      };
      int k = 0;
      while (k < n) {
        int j = k;
        while (j < n && form[static_cast<size_t>(j)] == form[static_cast<size_t>(k)]) {
          j++;
        }
        if (form[static_cast<size_t>(k)] == FoldSegmentForm::kArc) {
          for (int p = k; p + 1 < j; p++) {
            if (!read_ok[static_cast<size_t>(p)] || !read_ok[static_cast<size_t>(p + 1)]) {
              continue;
            }
            // A located search is a bounded unit of its own (a 52-step Jet2 bisection): the
            // shared stop gates each attempt, an unfinished one publishes nothing.
            if (stopped()) {
              break;
            }
            // The sign change is only meaningful between two RESOLVABLE reads: a |dJ(v)| below
            // its own resolution is an unresolved degenerate region (the mirror branch's
            // transition zone), never a locatable cusp — no marker is generated there.
            const FoldPointRecord& qa = out.crossings[static_cast<size_t>(chain->order[static_cast<size_t>(p)])];
            const FoldPointRecord& qb = out.crossings[static_cast<size_t>(chain->order[static_cast<size_t>(p + 1)])];
            if (std::fabs(djv[static_cast<size_t>(p)]) <= qa.djv_resolution ||
                std::fabs(djv[static_cast<size_t>(p + 1)]) <= qb.djv_resolution) {
              continue;
            }
            if (djv[static_cast<size_t>(p)] * djv[static_cast<size_t>(p + 1)] < 0.0) {
              locate_cusp(p, p + 1);
            }
          }
        }
        FoldSegmentRecord segment;
        segment.first = k;
        segment.last = j - 1;
        // The published values ARE the verification's own readings (review: one evidence, one
        // publication — no re-derivation over a coarser sample set at the assembly). The form
        // reads the split's array; a verified strict sub-run's first position carries its own
        // diameter and bound, an unverified (default-arc / unread) run carries the defaults.
        segment.form = form[static_cast<size_t>(k)];
        segment.image_diameter_deg = run_verified[static_cast<size_t>(k)] ? run_diameter[static_cast<size_t>(k)] : 0.0;
        segment.image_bound_deg = run_verified[static_cast<size_t>(k)] ? run_bound[static_cast<size_t>(k)] : -1.0;
        if (segment.form == FoldSegmentForm::kPointImage) {
          double mean[3] = { 0.0, 0.0, 0.0 };
          int mean_count = 0;
          for (int p = k; p < j; p++) {
            const FoldPointRecord& point = out.crossings[static_cast<size_t>(chain->order[static_cast<size_t>(p)])];
            double dir[3];
            ImageSkyDir(point.outgoing, dir);
            for (int c = 0; c < 3; c++) {
              mean[c] += dir[c];
            }
            mean_count++;
          }
          const double mean_norm = std::sqrt(mean[0] * mean[0] + mean[1] * mean[1] + mean[2] * mean[2]);
          if (mean_norm > 0.0 && mean_count > 0) {
            for (int c = 0; c < 3; c++) {
              mean[c] /= mean_norm;
            }
            // ImageAltAzDeg reads -outgoing; the mean is already the image direction, so its
            // negation plays outgoing.
            const double as_outgoing[3] = { -mean[0], -mean[1], -mean[2] };
            ImageAltAzDeg(as_outgoing, &segment.image_alt_deg, &segment.image_az_deg);
          }
        }
        chain->segments.push_back(segment);
        k = j;
      }
      // A closed chain's seam adjacency (its last and first positions are linked): the cusp
      // search above runs on linear segments only, so the seam pair is checked here — under
      // the same resolvable-signs gate as the linear pairs.
      if (chain->closed && n >= 2 && form[static_cast<size_t>(n - 1)] == FoldSegmentForm::kArc &&
          form[static_cast<size_t>(0)] == FoldSegmentForm::kArc && read_ok[static_cast<size_t>(n - 1)] &&
          read_ok[static_cast<size_t>(0)] &&
          std::fabs(djv[static_cast<size_t>(n - 1)]) >
              out.crossings[static_cast<size_t>(chain->order[static_cast<size_t>(n - 1)])].djv_resolution &&
          std::fabs(djv[static_cast<size_t>(0)]) >
              out.crossings[static_cast<size_t>(chain->order[static_cast<size_t>(0)])].djv_resolution &&
          djv[static_cast<size_t>(n - 1)] * djv[static_cast<size_t>(0)] < 0.0) {
        locate_cusp(n - 1, 0);
      }
    };

    const auto end_reason_at = [&](int c) -> FoldChainEnd {
      int cells[2];
      const int count = cells_of_crossing(c, cells);
      for (int k = 0; k < count; k++) {
        if (ambiguous_cell[static_cast<size_t>(cells[k])]) {
          return FoldChainEnd::kAmbiguousCell;
        }
      }
      if (out.crossings[static_cast<size_t>(c)].from_cliff_probe) {
        return FoldChainEnd::kFeasibilityInterruption;
      }
      // An uncut scan earns kOpen (the locus genuinely stops at this resolution); a scan the
      // stop cut never earned that claim — the grid beyond the cut was never looked at.
      return out.stop_requested ? FoldChainEnd::kStopRequested : FoldChainEnd::kOpen;
    };

    // The walk: open chains from degree-1 crossings (smallest index first), then closed loops
    // from the remaining degree-2 crossings, then isolated crossings as single-point chains.
    // Deterministic: crossing indices are scan order and every iteration runs in index order.
    // The shared stop gates each chain start (no chain begins after a cut) and each walk step
    // (a cut walk publishes the chain it holds, its walking end kStopRequested — the adjacency
    // beyond was never examined), and stops the pass.
    const int total_crossings = static_cast<int>(out.crossings.size());
    adjacency.resize(static_cast<size_t>(total_crossings));
    std::vector<bool> visited(static_cast<size_t>(total_crossings), false);
    bool walk_pass_cut = false;
    for (int start = 0; start < total_crossings && !walk_pass_cut; start++) {
      if (visited[static_cast<size_t>(start)] || adjacency[static_cast<size_t>(start)].size() != 1) {
        continue;
      }
      if (stopped()) {
        break;  // no chain starts after the cut
      }
      FoldChainRecord chain;
      chain.closed = false;
      int current = start;
      int prev = -1;
      bool step_cut = false;
      while (true) {
        visited[static_cast<size_t>(current)] = true;
        chain.order.push_back(current);
        int next = -1;
        FoldChainLink next_link;
        for (const LinkEntry& entry : adjacency[static_cast<size_t>(current)]) {
          if (entry.to == prev || visited[static_cast<size_t>(entry.to)]) {
            continue;
          }
          next = entry.to;
          next_link = entry.link;
          break;
        }
        if (next < 0) {
          break;
        }
        // The poll sits BEFORE the link commits: a walk unit is the ATOMIC (link + target
        // point) pair, so a cut here ends the chain at `current` with the links ==
        // order.size() - 1 invariant intact — a dangling link to a point that never joined
        // the order would fail the frozen boundary's topology check (the review's probe:
        // the old order turned every walk-phase cancel into a freeze exception).
        if (stopped()) {
          step_cut = true;
          break;
        }
        chain.links.push_back(next_link);
        prev = current;
        current = next;
      }
      chain.end_a = end_reason_at(chain.order.front());
      chain.end_b = step_cut ? FoldChainEnd::kStopRequested : end_reason_at(chain.order.back());
      classify_chain(&chain);
      out.fold_chains.push_back(std::move(chain));
      walk_pass_cut = step_cut;
    }
    for (int start = 0; start < total_crossings; start++) {
      if (walk_pass_cut || visited[static_cast<size_t>(start)]) {
        continue;
      }
      if (stopped()) {
        break;  // no chain starts after the cut
      }
      if (adjacency[static_cast<size_t>(start)].empty()) {
        visited[static_cast<size_t>(start)] = true;
        FoldChainRecord chain;
        chain.order.push_back(start);
        chain.end_a = end_reason_at(start);
        chain.end_b = chain.end_a;
        classify_chain(&chain);
        out.fold_chains.push_back(std::move(chain));
        continue;
      }
      // A closed loop: walk until the walk returns to its start through a justifying link.
      FoldChainRecord chain;
      int current = start;
      int prev = -1;
      bool closed = false;
      bool step_cut = false;
      FoldChainLink closing_link;
      while (true) {
        visited[static_cast<size_t>(current)] = true;
        chain.order.push_back(current);
        int next = -1;
        FoldChainLink next_link;
        for (const LinkEntry& entry : adjacency[static_cast<size_t>(current)]) {
          if (entry.to == prev || visited[static_cast<size_t>(entry.to)]) {
            continue;
          }
          next = entry.to;
          next_link = entry.link;
          break;
        }
        if (next < 0) {
          for (const LinkEntry& entry : adjacency[static_cast<size_t>(current)]) {
            if (entry.to == start && chain.order.size() >= 3) {
              closed = true;
              closing_link = entry.link;
              break;
            }
          }
          break;
        }
        // The same atomic (link + target point) commit: the poll precedes the link, a cut
        // walk publishes links == order.size() - 1 and never a closed loop.
        if (stopped()) {
          step_cut = true;
          break;
        }
        chain.links.push_back(next_link);
        prev = current;
        current = next;
      }
      chain.closed = closed;
      if (closed) {
        chain.links.push_back(closing_link);
        chain.end_a = FoldChainEnd::kClosedLoop;
        chain.end_b = FoldChainEnd::kClosedLoop;
      } else {
        chain.end_a = end_reason_at(chain.order.front());
        chain.end_b = step_cut ? FoldChainEnd::kStopRequested : end_reason_at(chain.order.back());
      }
      classify_chain(&chain);
      out.fold_chains.push_back(std::move(chain));
      walk_pass_cut = step_cut;
    }
  }

  // The collapse components: four-connected class-0 samples over the grid (a periodic
  // dimension's wrap connects through it).
  if (out.rank0_samples > 0) {
    DisjointSet set(out.samples);
    for (int i0 = 0; i0 < n0; i0++) {
      for (int i1 = 0; i1 < n1; i1++) {
        const Sample& here = samples[static_cast<size_t>(i0 * n1 + i1)];
        if (!here.valid || here.rank_class != 0) {
          continue;
        }
        const int j0 = StepIndex(axes[0], i0, 1);
        const int j1 = out.free_count == 2 ? StepIndex(axes[1], i1, 1) : -1;
        const int neighbors[2][2] = { { j0, i1 }, { i0, j1 } };
        for (const auto& neighbor : neighbors) {
          if (neighbor[0] < 0 || neighbor[1] < 0) {
            continue;
          }
          const Sample& there = samples[static_cast<size_t>(neighbor[0] * n1 + neighbor[1])];
          if (there.valid && there.rank_class == 0) {
            set.Union(i0 * n1 + i1, neighbor[0] * n1 + neighbor[1]);
          }
        }
      }
    }
    std::vector<int> first_of_root(static_cast<size_t>(out.samples), -1);
    for (int s = 0; s < out.samples; s++) {
      const Sample& sample = samples[static_cast<size_t>(s)];
      if (!sample.valid || sample.rank_class != 0) {
        continue;
      }
      const int root = set.Find(s);
      if (first_of_root[static_cast<size_t>(root)] < 0) {
        first_of_root[static_cast<size_t>(root)] = s;
      }
    }
    for (int s = 0; s < out.samples; s++) {
      const Sample& sample = samples[static_cast<size_t>(s)];
      if (!sample.valid || sample.rank_class != 0 || first_of_root[static_cast<size_t>(set.Find(s))] != s) {
        continue;
      }
      // One component: its size and its smallest-sigma1 representative.
      CollapseRecord record;
      int representative = s;
      for (int d = s; d < out.samples; d++) {
        const Sample& other = samples[static_cast<size_t>(d)];
        if (!other.valid || other.rank_class != 0 || set.Find(d) != set.Find(s)) {
          continue;
        }
        record.sample_count++;
        if (other.sigma1 < samples[static_cast<size_t>(representative)].sigma1) {
          representative = d;
        }
      }
      const double params[2] = { axes[0].ValueDeg(representative / n1),
                                 out.free_count == 2 ? axes[1].ValueDeg(representative % n1) : 0.0 };
      for (int k = 0; k < out.free_count; k++) {
        record.params_deg[k] = axes[k].periodic ? std::fmod(params[k], 360.0) : params[k];
      }
      double degrees[3];
      DimensionDegrees(spec, axes, out.free_count, params, degrees);
      EulerPose(degrees[1] * kDegToRad, degrees[0] * kDegToRad, degrees[2] * kDegToRad, record.pose);
      const Sample& rep = samples[static_cast<size_t>(representative)];
      for (int i = 0; i < 3; i++) {
        record.outgoing[i] = rep.outgoing[i];
      }
      ImageAltAzDeg(record.outgoing, &record.image_alt_deg, &record.image_az_deg);
      record.sigma1 = rep.sigma1;
      out.collapse_components.push_back(record);
    }
  }

  // The one-dimensional family's ordered track: the scan's own samples recorded in parameter
  // order, validity-segmented (no fold machinery runs on a one-dimensional family; this is a
  // recording of what the scan already evaluated, not a second evaluation).
  if (out.free_count == 1) {
    FamilyTrackRecord& track = out.track;
    track.points.resize(static_cast<size_t>(n0));
    for (int i0 = 0; i0 < n0; i0++) {
      const Sample& sample = samples[static_cast<size_t>(i0)];
      FamilyTrackPoint& point = track.points[static_cast<size_t>(i0)];
      point.param_deg = axes[0].ValueDeg(i0);
      if (!sample.valid) {
        continue;
      }
      point.valid = true;
      const double params[2] = { axes[0].ValueDeg(i0), 0.0 };
      double degrees[3];
      DimensionDegrees(spec, axes, 1, params, degrees);
      EulerPose(degrees[1] * kDegToRad, degrees[0] * kDegToRad, degrees[2] * kDegToRad, point.pose);
      for (int i = 0; i < 3; i++) {
        point.outgoing[i] = sample.outgoing[i];
      }
      ImageAltAzDeg(point.outgoing, &point.image_alt_deg, &point.image_az_deg);
      point.sigma1 = sample.sigma1;
    }
    int k = 0;
    while (k < n0) {
      if (track.points[static_cast<size_t>(k)].valid) {
        int j = k;
        while (j < n0 && track.points[static_cast<size_t>(j)].valid) {
          j++;
        }
        FamilyTrackSegment segment;
        segment.first = k;
        segment.last = j - 1;
        track.segments.push_back(segment);
        k = j;
      } else {
        k++;
      }
    }
    track.closed = axes[0].periodic && n0 > 0 && track.points.front().valid && track.points.back().valid;
  }

  return out;
}

}  // namespace lumice::analytic
