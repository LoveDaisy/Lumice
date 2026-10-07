#include "raypath/detail/measure/declared_density.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "analytic/pose_density.hpp"

namespace lumice::raypath {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kHalfPi = kPi * 0.5;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kDegToRad = kPi / 180.0;

// The wrapped (torus) density of a slot law at x: sum over k of the law at x + 2 pi k. Compact
// supports make this a finite sum; the unbounded ones decay superexponentially (gauss) or
// exponentially (laplacian) and the cutoff below leaves them exact to double precision for every
// law the engine can configure.
double WrappedSlotDensity(const Distribution& slot, double x_rad, bool* dirac, double* dirac_at) {
  if (SlotIsDirac(slot, dirac_at)) {
    *dirac = true;
    return 0.0;
  }
  *dirac = false;
  // The k-window: |2 pi k| beyond 40 scales is e^{-800} for the narrowest gaussian and e^{-40}
  // for the widest laplacian anyone can write with a float spread; both are 0 in double. Premise:
  // `spread` is a config-bounded degree value (a float), so `reach / kTwoPi` stays far inside
  // int range — no overflow guard is taken here.
  const double reach = 40.0 * std::fabs(static_cast<double>(slot.spread)) * kDegToRad + kTwoPi;
  const int kmax = static_cast<int>(reach / kTwoPi) + 1;
  double sum = 0.0;
  for (int k = -kmax; k <= kmax; k++) {
    sum += SlotDensityValue(slot, x_rad + kTwoPi * k);
  }
  return sum;
}

// --- Small rotation helpers (row-major, applied to a vector): the exact factors of
// R = Rz(lambda - pi) Ry(phi - pi/2) Rz(roll) that the u map needs. ---

void RotateAboutZ(double v[3], double angle) {
  const double c = std::cos(angle), s = std::sin(angle);
  const double x = v[0] * c - v[1] * s;
  const double y = v[0] * s + v[1] * c;
  v[0] = x;
  v[1] = y;
}

void RotateAboutY(double v[3], double angle) {
  const double c = std::cos(angle), s = std::sin(angle);
  const double x = v[0] * c + v[2] * s;
  const double z = -v[0] * s + v[2] * c;
  v[0] = x;
  v[2] = z;
}

double Clamp(double v, double lo, double hi) {
  return std::min(std::max(v, lo), hi);
}

// detail::NormalizeLatitude's branch split as pure functions of the latitude value: the folded
// latitude and whether the fold flipped (src/core/math.cpp:341).
void FoldLatitude(double lat_rad, double* folded, bool* flip) {
  double theta = kHalfPi - lat_rad;
  theta = std::fmod(theta, kTwoPi);
  if (theta < 0.0) {
    theta += kTwoPi;
  }
  *flip = theta > kPi;
  if (*flip) {
    theta = kTwoPi - theta;
  }
  *folded = kHalfPi - theta;
}

// The two preimage families of a folded latitude (Table 2): branch A (no flip) L = phi + 2 pi k,
// branch B (flip) L = pi - phi + 2 pi k.
double FoldedProposalDensity(const Distribution& p, double phi_rad, bool branch_b) {
  const double offset = branch_b ? kPi - phi_rad : phi_rad;
  if (p.type == DistributionType::kNoRandom || p.spread == 0.0f) {
    return 0.0;  // a Dirac proposal has no density; the Dirac-of-Dirac path is handled by kind
  }
  const double reach = 40.0 * std::fabs(static_cast<double>(p.spread)) * kDegToRad + kTwoPi;
  const int kmax = static_cast<int>(reach / kTwoPi) + 1;
  double sum = 0.0;
  for (int k = -kmax; k <= kmax; k++) {
    sum += SlotDensityValue(p, offset + kTwoPi * k);
  }
  return sum;
}

// Composite Gauss-Legendre integral of f over [a, b]: `panels` uniform panels of GL order `order`.
// The analytic library's own GaussLegendre (pose_density.hpp, Newton on P_n) is the single node
// source; this wrapper only composites it.
template <class F>
double CompositeGaussLegendre(F&& f, double a, double b, int panels, int order) {
  std::vector<double> x(static_cast<size_t>(order)), w(static_cast<size_t>(order));
  analytic::GaussLegendre(order, x.data(), w.data());
  const double h = (b - a) / panels;
  double total = 0.0;
  for (int p = 0; p < panels; p++) {
    const double lo = a + p * h;
    for (int i = 0; i < order; i++) {
      total += w[i] * f(lo + (x[i] + 1.0) * 0.5 * h);
    }
  }
  return total * 0.5 * h;
}

// Interior breakpoints of a slot law's density on the real line (radians): the kinks and support
// edges the latitude and zonal quadratures split at so their panels see smooth integrands.
// kZigzag's own fold kinks are NOT listed (their closed form is a finite union that depends on
// the amplitude/tilt arithmetic); its quadrature converges algebraically and the tests say so.
std::vector<double> SlotDensityBreaks(const Distribution& slot) {
  std::vector<double> breaks;
  const double c = static_cast<double>(slot.center) * kDegToRad;
  switch (slot.type) {
    case DistributionType::kUniform: {
      const double half = static_cast<double>(slot.spread) * 0.5 * kDegToRad;
      breaks.push_back(c - half);
      breaks.push_back(c + half);
      break;
    }
    case DistributionType::kLaplacian:
      breaks.push_back(c);  // the |x - mu| kink
      break;
    default:
      break;
  }
  return breaks;
}

// The compact-support law's edges, when the type has them (uniform, zigzag): the zigzag's are
// also integrable 1/sqrt SPIKES (the arcsine edges), which a plain panel rule undershoots badly
// — the caller sqrt-substitutes at them.
std::vector<double> SupportEdges(const Distribution& slot, bool* spiky) {
  *spiky = false;
  std::vector<double> edges;
  const double c = static_cast<double>(slot.center) * kDegToRad;
  const double sp = static_cast<double>(slot.spread) * kDegToRad;
  switch (slot.type) {
    case DistributionType::kUniform:
      edges.push_back(c - sp * 0.5);
      edges.push_back(c + sp * 0.5);
      break;
    case DistributionType::kZigzag: {
      *spiky = true;
      edges.push_back(std::max(0.0, std::fabs(c) - sp));  // support of |A sin t + B|
      edges.push_back(std::fabs(c) + sp);
      break;
    }
    default:
      break;
  }
  return edges;
}

// Both fold-image families of one preimage value x, inside the latitude range (-pi/2, pi/2):
// branch A phi = x + 2 pi k, branch B phi = pi - x + 2 pi k (the two preimages of the folded
// latitude, FoldLatitude). The k range is DERIVED from x's magnitude — no config-bounded window:
// an extreme support edge (|edge| beyond one full turn) still lands its image in range through
// the k that the inequality admits, and the strict range filter drops the rest.
std::vector<double> FoldImagesOf(double x) {
  std::vector<double> images;
  const double bases[2] = { x, kPi - x };
  for (double base : bases) {
    const int k_lo = static_cast<int>(std::floor((-kHalfPi - base) / kTwoPi));
    const int k_hi = static_cast<int>(std::ceil((kHalfPi - base) / kTwoPi));
    for (int k = k_lo; k <= k_hi; k++) {
      const double phi = base + kTwoPi * k;
      if (phi > -kHalfPi && phi < kHalfPi) {
        images.push_back(phi);
      }
    }
  }
  std::sort(images.begin(), images.end());
  images.erase(std::unique(images.begin(), images.end(), [](double a, double b) { return std::fabs(a - b) < 1e-12; }),
               images.end());
  return images;
}

// The fold-image SPIKES of p_fold inside (-pi/2, pi/2): the phi-values whose branch-A or
// branch-B preimage sits on the proposal's support edge. kZigzag's edges are integrable 1/sqrt
// spikes that a plain panel rule undershoots badly — the quadratures below split at these
// phi-values (as panel EDGES, where the arcsine weight or a sqrt substitution absorbs them).
std::vector<double> FoldImageSpikes(const Distribution& slot) {
  bool spiky = false;
  const std::vector<double> edges = SupportEdges(slot, &spiky);
  if (!spiky) {
    return {};
  }
  std::vector<double> spikes;
  for (double e : edges) {
    for (double phi : FoldImagesOf(e)) {
      spikes.push_back(phi);
    }
  }
  std::sort(spikes.begin(), spikes.end());
  spikes.erase(std::unique(spikes.begin(), spikes.end(), [](double a, double b) { return std::fabs(a - b) < 1e-12; }),
               spikes.end());
  return spikes;
}

// The fold images of the slot's own density BREAKS (SlotDensityBreaks: support edges and kinks):
// a kinked law's branch-B image (kLaplacian's |x - mu| mirrored by the fold) is a real kink of
// p_fold/rho_phi, so the quadratures split at it too; a uniform's images are value-continuous
// (equal-density copies meet) and splitting them is harmless. Empty for laws without breaks.
std::vector<double> FoldImageKinks(const Distribution& slot) {
  std::vector<double> kinks;
  for (double b : SlotDensityBreaks(slot)) {
    for (double phi : FoldImagesOf(b)) {
      kinks.push_back(phi);
    }
  }
  std::sort(kinks.begin(), kinks.end());
  kinks.erase(std::unique(kinks.begin(), kinks.end(), [](double a, double b) { return std::fabs(a - b) < 1e-12; }),
              kinks.end());
  return kinks;
}

// Panel integral of f over (a, b), split at `cuts`, with the sqrt-substitution s -> s^2 applied
// at any cut that is one of `spikes` (f's integrable 1/sqrt edges): the substitution removes the
// singularity exactly, a plain panel rule would undershoot it badly (the Z measurement: ~24% on
// tilt 30 / amplitude 20). `spikes` must be sorted; interior smooth panels take composite GL.
template <class F>
double IntegrateWithFoldImages(F&& f, const std::vector<double>& cuts, const std::vector<double>& spikes, double a,
                               double b, int gl_panels_per_half = 8, int gl_order = 16) {
  auto is_spike = [&spikes](double x) {
    return std::any_of(spikes.begin(), spikes.end(), [x](double s) { return std::fabs(x - s) < 1e-12; });
  };
  std::vector<double> edges{ a };
  for (double c : cuts) {
    if (c > a && c < b) {
      edges.push_back(c);
    }
  }
  edges.push_back(b);
  std::sort(edges.begin(), edges.end());
  edges.erase(std::unique(edges.begin(), edges.end(), [](double u, double v) { return std::fabs(u - v) < 1e-12; }),
              edges.end());
  const int n = 256;
  double total = 0.0;
  for (size_t i = 0; i + 1 < edges.size(); i++) {
    const double lo = edges[i], hi = edges[i + 1];
    if (!(hi > lo)) {
      continue;
    }
    const double mid = 0.5 * (lo + hi);
    if (is_spike(lo)) {
      const double smax = std::sqrt(mid - lo);
      const double ds = smax / n;
      for (int j = 0; j < n; j++) {
        const double sq = (j + 0.5) * ds;
        total += f(lo + sq * sq) * 2.0 * sq * ds;
      }
    } else {
      total += CompositeGaussLegendre(f, lo, mid, gl_panels_per_half, gl_order);
    }
    if (is_spike(hi)) {
      const double smax = std::sqrt(hi - mid);
      const double ds = smax / n;
      for (int j = 0; j < n; j++) {
        const double sq = (j + 0.5) * ds;
        total += f(hi - sq * sq) * 2.0 * sq * ds;
      }
    } else {
      total += CompositeGaussLegendre(f, mid, hi, gl_panels_per_half, gl_order);
    }
  }
  return total;
}

}  // namespace

// ---------------------------------------------------------------------------
// Table 1: slot densities.
// ---------------------------------------------------------------------------

bool SlotIsDirac(const Distribution& slot, double* at_rad) {
  if (at_rad != nullptr) {
    *at_rad = SlotDegenerateValue(slot) * kDegToRad;
  }
  return slot.type == DistributionType::kNoRandom || slot.spread == 0.0f;
}

double SlotDegenerateValue(const Distribution& slot) {
  // BuildDistributionDrawPlan answers kConstant for spread == 0 (src/core/sample_transform.cpp:54)
  // and TransformDistribution then evaluates the law's constant: |0*sin + B| = |B| for zigzag,
  // center for everyone else.
  if (slot.type == DistributionType::kZigzag) {
    return std::fabs(static_cast<double>(slot.center));
  }
  return static_cast<double>(slot.center);
}

double ZigzagDensityValue(double amplitude_rad, double tilt_rad, double y_rad) {
  // y = |A sin t + B| with t = 2 pi U uniform on one period. Preimages of y > 0: the solutions of
  // A sin t + B = +y and = -y; each contributes 1/(2 pi A |cos t|). Returns 0 at y <= 0 (the fold
  // point 0 itself is a boundary; its density diverges from one side only when B = +-A).
  if (amplitude_rad <= 0.0 || y_rad <= 0.0) {
    return 0.0;
  }
  double sum = 0.0;
  const double targets[2] = { y_rad, -y_rad };
  for (double target : targets) {
    const double s = (target - tilt_rad) / amplitude_rad;  // required sin t
    if (s < -1.0 || s > 1.0) {
      continue;
    }
    const double cos_sq = std::max(1.0 - s * s, 0.0);
    const double inv_cos = 1.0 / std::sqrt(cos_sq);  // |cos t| at both solutions
    // Two solutions per period (t0 and pi - t0), coinciding at |s| = 1.
    sum += (cos_sq > 0.0 ? 2.0 : 1.0) * inv_cos;
  }
  return sum / (kTwoPi * amplitude_rad);
}

double SlotDensityValue(const Distribution& slot, double x_rad) {
  const double c = static_cast<double>(slot.center) * kDegToRad;
  const double s = static_cast<double>(slot.spread) * kDegToRad;
  switch (slot.type) {
    case DistributionType::kNoRandom:
      return 0.0;
    case DistributionType::kUniform: {
      // HALF-OPEN [c - R/2, c + R/2): the wrapped (torus) sum then covers the seam — the k = 0
      // copy excludes x = c + R/2 exactly where the k = -1 copy includes it. An open box would
      // leave a measure-zero puncture that a preimage landing exactly on the seam reads as zero
      // density (bit the general-path preimage evaluation at a symmetric probe).
      const double half = s * 0.5;
      return (x_rad >= c - half && x_rad < c + half) ? 1.0 / s : 0.0;
    }
    case DistributionType::kGaussian:
    case DistributionType::kGaussianLegacy: {
      if (s <= 0.0) {
        return 0.0;
      }
      const double d = (x_rad - c) / s;
      return std::exp(-0.5 * d * d) / (s * std::sqrt(kTwoPi));
    }
    case DistributionType::kLaplacian: {
      if (s <= 0.0) {
        return 0.0;
      }
      return std::exp(-std::fabs(x_rad - c) / s) / (2.0 * s);
    }
    case DistributionType::kZigzag:
      return ZigzagDensityValue(s, c, x_rad);
    default:
      return 0.0;
  }
}

// ---------------------------------------------------------------------------
// Table 2: the folded latitude law.
// ---------------------------------------------------------------------------

LatitudeDensity MakeLatitudeDensity(const Distribution& lat_slot) {
  LatitudeDensity law;
  law.proposal_ = lat_slot;
  if (lat_slot.type == DistributionType::kNoRandom) {
    // The kNoRandom branch takes the raw center UNFOLDED (src/core/random.cpp:172).
    law.kind_ = LatitudeLawKind::kDiracUnfolded;
    law.point_ = static_cast<double>(lat_slot.center) * kDegToRad;
    return law;
  }
  if (lat_slot.spread == 0.0f) {
    // A degenerate non-noRandom latitude still routes to its sampling path (SelectLatPath keys on
    // the type): the LUT families land in BuildLatLut's zero-mass guard, a Dirac at the FOLDED
    // mean (src/core/lat_lut.cpp:130-136); the legacy path folds explicitly.
    double folded = 0.0;
    bool flip = false;
    FoldLatitude(SlotDegenerateValue(lat_slot) * kDegToRad, &folded, &flip);
    law.kind_ = LatitudeLawKind::kDiracFolded;
    law.point_ = folded;
    return law;
  }
  if (lat_slot.type == DistributionType::kGaussianLegacy) {
    law.kind_ = LatitudeLawKind::kFoldedNoArea;  // folded, no Jacobian, integrates to 1
    law.norm_ = 1.0;
    return law;
  }
  // kUniform / kGaussian / kZigzag / kLaplacian: the area-measure target. The full-sphere config
  // (uniform [90 +- 180]) takes its own sampler branch but declares the SAME continuum law (the
  // folded uniform area measure IS cos(phi)/2) — one law serves both, and the test pins that
  // identity numerically.
  law.kind_ = LatitudeLawKind::kFoldedArea;
  {
    // Z = integral of p_fold * cos over [-pi/2, pi/2], on panels split at the density's kinks
    // and support edges AND at their fold images (branch-B kinks of kinked laws; the fold-image
    // SPIKES of the zigzag family's arcsine edges, where the sqrt-substitution removes the
    // integrable divergence exactly — a plain panel rule undershoots it badly, measured 24% on
    // tilt 30 / amplitude 20).
    auto integrand = [&lat_slot](double phi) {
      return (FoldedProposalDensity(lat_slot, phi, false) + FoldedProposalDensity(lat_slot, phi, true)) * std::cos(phi);
    };
    std::vector<double> cuts = SlotDensityBreaks(lat_slot);
    for (double b : FoldImageKinks(lat_slot)) {
      cuts.push_back(b);
    }
    for (double b : FoldImageSpikes(lat_slot)) {
      cuts.push_back(b);
    }
    std::sort(cuts.begin(), cuts.end());
    cuts.erase(std::unique(cuts.begin(), cuts.end(), [](double a, double b) { return std::fabs(a - b) < 1e-12; }),
               cuts.end());
    law.norm_ = IntegrateWithFoldImages(integrand, cuts, FoldImageSpikes(lat_slot), -kHalfPi, kHalfPi);
  }
  if (!(law.norm_ > 0.0)) {
    // Unreachable for spread > 0 (the fold preserves mass and cos > 0 on the open interval);
    // the guard keeps a pathological config from producing NaNs downstream.
    law.kind_ = LatitudeLawKind::kDiracFolded;
    double folded = 0.0;
    bool flip = false;
    FoldLatitude(static_cast<double>(lat_slot.center) * kDegToRad, &folded, &flip);
    law.point_ = folded;
    law.norm_ = 1.0;
  }
  return law;
}

LatitudeDensity MakeFullSphereLatitude() {
  LatitudeDensity law;
  law.kind_ = LatitudeLawKind::kFullSphere;
  law.norm_ = 1.0;  // integral of cos(phi)/2 over [-pi/2, pi/2]
  return law;
}

double LatitudeDensity::Evaluate(double phi_rad) const {
  switch (kind_) {
    case LatitudeLawKind::kFullSphere:
      return (phi_rad > -kHalfPi && phi_rad < kHalfPi) ? std::cos(phi_rad) * 0.5 : 0.0;
    case LatitudeLawKind::kFoldedArea: {
      if (phi_rad <= -kHalfPi || phi_rad >= kHalfPi) {
        return 0.0;
      }
      const double p =
          FoldedProposalDensity(proposal_, phi_rad, false) + FoldedProposalDensity(proposal_, phi_rad, true);
      return p * std::cos(phi_rad) / norm_;
    }
    case LatitudeLawKind::kFoldedNoArea: {
      if (phi_rad <= -kHalfPi || phi_rad >= kHalfPi) {
        return 0.0;
      }
      return FoldedProposalDensity(proposal_, phi_rad, false) + FoldedProposalDensity(proposal_, phi_rad, true);
    }
    case LatitudeLawKind::kDiracUnfolded:
    case LatitudeLawKind::kDiracFolded:
      return 0.0;  // a point mass has no density; point() carries it
  }
  return 0.0;
}

double LatitudeDensity::FlipProbability(double phi_rad) const {
  switch (kind_) {
    case LatitudeLawKind::kFullSphere:
    case LatitudeLawKind::kDiracUnfolded:
    case LatitudeLawKind::kDiracFolded:
      return 0.0;
    case LatitudeLawKind::kFoldedArea:
    case LatitudeLawKind::kFoldedNoArea: {
      if (phi_rad <= -kHalfPi || phi_rad >= kHalfPi) {
        return 0.0;
      }
      const double pa = FoldedProposalDensity(proposal_, phi_rad, false);
      const double pb = FoldedProposalDensity(proposal_, phi_rad, true);
      const double total = pa + pb;
      return total > 0.0 ? pb / total : 0.0;
    }
  }
  return 0.0;
}

double LatitudeDensity::TotalMass() const {
  // The same fold-aware panel split the Z normalization uses: the plain rule underreads a
  // kZigzag proposal's arcsine spikes badly (the Z measurement: ~24%) and a kinked law's
  // branch-B fold image is a real kink — the accessor is honest for EVERY folded law, there is
  // no "spike-free" precondition. (The Dirac kinds' Evaluate is identically 0, so the split
  // machinery reads their 0 mass unchanged.)
  std::vector<double> cuts = SlotDensityBreaks(proposal_);
  for (double b : FoldImageKinks(proposal_)) {
    cuts.push_back(b);
  }
  for (double b : FoldImageSpikes(proposal_)) {
    cuts.push_back(b);
  }
  std::sort(cuts.begin(), cuts.end());
  cuts.erase(std::unique(cuts.begin(), cuts.end(), [](double a, double b) { return std::fabs(a - b) < 1e-12; }),
             cuts.end());
  return IntegrateWithFoldImages([this](double phi) { return Evaluate(phi); }, cuts, FoldImageSpikes(proposal_),
                                 -kHalfPi, kHalfPi, 32, 16);
}

// ---------------------------------------------------------------------------
// Table 3: the joint pose law.
// ---------------------------------------------------------------------------

double DeclaredPoseDensity(const AxisDistribution& axis, double lambda_out_rad, double phi_rad, double roll_out_rad) {
  // The full-sphere branch is config-level and never flips: plain slot product with cos(phi)/2.
  const LatitudeDensity lat =
      axis.IsFullSphereUniform() ? MakeFullSphereLatitude() : MakeLatitudeDensity(axis.latitude_dist);
  return DeclaredPoseDensity(lat, axis, lambda_out_rad, phi_rad, roll_out_rad);
}

double DeclaredPoseDensity(const LatitudeDensity& lat, const AxisDistribution& axis, double lambda_out_rad,
                           double phi_rad, double roll_out_rad) {
  if (lat.kind() == LatitudeLawKind::kDiracUnfolded || lat.kind() == LatitudeLawKind::kDiracFolded) {
    return 0.0;  // singular in phi; the mass is carried by UMarginal's support kinds
  }
  double az_at = 0.0, roll_at = 0.0;
  bool az_dirac = false, roll_dirac = false;
  const double az_d = WrappedSlotDensity(axis.azimuth_dist, lambda_out_rad, &az_dirac, &az_at);
  const double roll_d = WrappedSlotDensity(axis.roll_dist, roll_out_rad, &roll_dirac, &roll_at);
  if (az_dirac || roll_dirac) {
    return 0.0;  // singular in that slot; same rule as phi
  }
  const double f = lat.FlipProbability(phi_rad);
  const double rho_phi = lat.Evaluate(phi_rad);
  const double shifted_az = WrappedSlotDensity(axis.azimuth_dist, lambda_out_rad - kPi, &az_dirac, &az_at);
  const double shifted_roll = WrappedSlotDensity(axis.roll_dist, roll_out_rad - kPi, &roll_dirac, &roll_at);
  return rho_phi * ((1.0 - f) * az_d * roll_d + f * shifted_az * shifted_roll);
}

// ---------------------------------------------------------------------------
// Table 4: the u-marginal.
// ---------------------------------------------------------------------------

namespace {

// The torus pose-joint at a preimage: the wrapped az law mixed by the flip probability (Table
// 3's formula on the pose torus — this is what the u-marginal of the SAMPLER integrates). The
// ROLL factor is supplied by the caller: for a spread roll it is the wrapped roll law at the
// branch's raw value; a Dirac roll pins the composed slice (its delta mass evaluates to 1 on the
// branch that reaches that slice and 0 on the other — DensitySolidAngleGeneral sums both slices).
double PoseTorusDensity(const LatitudeDensity& lat, const Distribution& az, double lambda_out, double phi,
                        bool flip_branch, double roll_factor) {
  if (lat.kind() != LatitudeLawKind::kFoldedArea && lat.kind() != LatitudeLawKind::kFoldedNoArea &&
      lat.kind() != LatitudeLawKind::kFullSphere) {
    return 0.0;
  }
  if (!(roll_factor > 0.0)) {
    return 0.0;
  }
  bool dirac = false;
  double at = 0.0;
  const double az_d = WrappedSlotDensity(az, flip_branch ? lambda_out - kPi : lambda_out, &dirac, &at);
  if (dirac) {
    // A Dirac azimuth zeroes every preimage term: the general path never solves the
    // fixed-azimuth (phi, roll) preimage — the REGISTERED read-as-zero gap in the header's
    // v1 gap list (zero = "unanswered", not "dark").
    return 0.0;
  }
  const double f = lat.FlipProbability(phi);
  const double branch_weight = flip_branch ? f : (1.0 - f);
  return lat.Evaluate(phi) * branch_weight * az_d * roll_factor;
}

// Gauss-Chebyshev pieces for the zonal m(c): the phi-set where c lies inside the arcsine range.
// Returns the pieces' total: m(c) = integral of rho_phi(phi) / (pi sqrt((c - c_lo)(c_hi - c)))
// over the valid interval, split at the slot's own density breaks. A DIRAC latitude contributes
// the kernel at its point alone (the arcsine ring: fixed-zenith family with free azimuth/roll).
double ZonalMarginalAt(const LatitudeDensity& lat, const Distribution& proposal, double c, double sigma) {
  if (lat.kind() == LatitudeLawKind::kDiracUnfolded || lat.kind() == LatitudeLawKind::kDiracFolded) {
    // m(c) = ArcsineKernel(c; phi_0): the point mass picks the kernel's value at phi_0, nonzero
    // on the open interval only (at phi_0 = +-pi/2 the interval is empty — the axis-at-pole
    // corner the support classification routes elsewhere).
    const double phi0 = lat.point();
    const double d_hi = std::cos(phi0 - sigma) - c;
    const double d_lo = c + std::cos(phi0 + sigma);
    if (d_hi <= 0.0 || d_lo <= 0.0) {
      return 0.0;
    }
    return 1.0 / (kPi * std::sqrt(d_lo * d_hi));
  }
  const double gamma0 = std::acos(Clamp(c, -1.0, 1.0));  // arccos(c), in (0, pi)
  // phi in (sigma - gamma0, sigma + gamma0) [from c < cos(phi - sigma)] AND
  // phi in (-sigma - (pi - gamma0), -sigma + (pi - gamma0)) [from c > -cos(phi + sigma)].
  double lo = std::max(sigma - gamma0, -sigma - kPi + gamma0);
  double hi = std::min(sigma + gamma0, -sigma + kPi - gamma0);
  lo = std::max(lo, -kHalfPi);
  hi = std::min(hi, kHalfPi);
  if (!(hi > lo)) {
    return 0.0;
  }
  // Arcsine kernel as a function of phi on (lo, hi): 1/(pi sqrt((c - c_lo(phi))(c_hi(phi) - c)))
  // with c_lo = -cos(phi + sigma), c_hi = cos(phi - sigma).
  auto kernel = [&](double phi) {
    const double d_hi = std::cos(phi - sigma) - c;
    const double d_lo = c + std::cos(phi + sigma);
    if (d_hi <= 0.0 || d_lo <= 0.0) {
      return 0.0;
    }
    return 1.0 / (kPi * std::sqrt(d_lo * d_hi));
  };
  // Split at the slot's own breaks AND their fold images (branch-B kinks of kinked laws), plus
  // the fold-image SPIKES of spiky support edges (kZigzag's arcsine edges) — a spike must sit on
  // a panel EDGE, where the arcsine weight absorbs it exactly (rho_phi's 1/sqrt cancels the
  // weight's sqrt and g stays smooth); left inside g it degrades the rule to algebraic order at
  // best and underreads unboundedly at worst (the MakeLatitudeDensity measurement: 24%). The
  // degenerate coincidence of a spike with the kernel's own singular endpoint (a measure-zero
  // config alignment) makes m(c) genuinely log-divergent — the same honest-degenerate precedent
  // as ZigzagDensityValue's +inf support endpoint.
  std::vector<double> cuts;
  for (double b : SlotDensityBreaks(proposal)) {
    if (b > lo && b < hi) {
      cuts.push_back(b);
    }
  }
  for (double b : FoldImageKinks(proposal)) {
    if (b > lo && b < hi) {
      cuts.push_back(b);
    }
  }
  for (double s : FoldImageSpikes(proposal)) {
    if (s > lo && s < hi) {
      cuts.push_back(s);
    }
  }
  std::sort(cuts.begin(), cuts.end());
  // Each piece then has at most the two sqrt-singular endpoints (where c meets c_lo/c_hi, and
  // any spike parked on an edge) and is otherwise smooth. On each piece the integrand is exactly
  // g(phi)/sqrt((phi - a)(b - phi)) with g smooth — the kernel's own 1/sqrt factors ARE the
  // Chebyshev weight — so the rule integrates g at the Chebyshev nodes and multiplies by the
  // weight's integral pi. (Evaluating the FULL integrand at the nodes would apply the weight
  // twice — the factorization below is the load-bearing step.)
  std::vector<double> edges;
  edges.push_back(lo);
  edges.insert(edges.end(), cuts.begin(), cuts.end());
  edges.push_back(hi);
  double total = 0.0;
  for (size_t i = 0; i + 1 < edges.size(); i++) {
    const double a = edges[i], b = edges[i + 1];
    if (!(b > a)) {
      continue;
    }
    const int n = 64;
    const double mid = 0.5 * (a + b), half = 0.5 * (b - a);
    double sum = 0.0;
    for (int j = 0; j < n; j++) {
      const double t = std::cos((j + 0.5) * kPi / n);
      const double phi = mid + half * t;
      const double smooth_part = lat.Evaluate(phi) * kernel(phi) * std::sqrt((phi - a) * (b - phi));
      sum += smooth_part;
    }
    // int_a^b g(phi)/sqrt((phi-a)(b-phi)) dphi = pi * Chebyshev mean of g.
    total += kPi * sum / n;
  }
  return total;
}

}  // namespace

UMarginal MakeUMarginal(const AxisDistribution& axis, const double sun_dir[3]) {
  UMarginal m;
  m.axis_ = axis;
  m.lat_ = axis.IsFullSphereUniform() ? MakeFullSphereLatitude() : MakeLatitudeDensity(axis.latitude_dist);
  const double norm = std::sqrt(sun_dir[0] * sun_dir[0] + sun_dir[1] * sun_dir[1] + sun_dir[2] * sun_dir[2]);
  m.sun_[0] = sun_dir[0] / norm;
  m.sun_[1] = sun_dir[1] / norm;
  m.sun_[2] = sun_dir[2] / norm;
  m.sigma_ = std::asin(Clamp(m.sun_[2], -1.0, 1.0));
  m.lambda_s_ = std::atan2(m.sun_[1], m.sun_[0]);
  m.fast_ = axis.IsAzRotationallySymmetric() && axis.IsRollRotationallySymmetric();

  const bool lat_dirac =
      m.lat_.kind() == LatitudeLawKind::kDiracUnfolded || m.lat_.kind() == LatitudeLawKind::kDiracFolded;
  double dummy = 0.0;
  const bool az_dirac = SlotIsDirac(axis.azimuth_dist, &dummy);
  const bool roll_dirac = SlotIsDirac(axis.roll_dist, &dummy);
  // The axis-at-pole corner: with the axis Dirac AT +-pi/2 the azimuth and the roll act about the
  // SAME body axis, so two spread slots have a one-dimensional image whose density is their
  // convolution — reported degenerate rather than answered with either law alone.
  const bool lat_at_pole = lat_dirac && std::fabs(std::fabs(m.lat_.point()) - kHalfPi) < 1e-12;
  const bool axis_corner = lat_at_pole && !az_dirac && !roll_dirac;
  if (axis_corner) {
    m.kind_ = USupportKind::kDegenerateAxisGeometry;
    m.fast_ = false;
    return m;
  }
  const int spread_slots = (lat_dirac ? 0 : 1) + (az_dirac ? 0 : 1) + (roll_dirac ? 0 : 1);
  if (spread_slots == 0) {
    m.kind_ = USupportKind::kPoint;
  } else if (std::fabs(m.sun_[2]) > 1.0 - 1e-12) {
    // Sun at a pole: cos(sigma) = 0 collapses the azimuth degree of freedom (Table 4).
    m.kind_ = USupportKind::kDegenerateSunGeometry;
    m.fast_ = false;
  } else if (spread_slots == 1) {
    if (!az_dirac) {
      m.kind_ = USupportKind::kSpinOrbit;
    } else if (!roll_dirac) {
      m.kind_ = USupportKind::kRollOrbit;
    } else {
      m.kind_ = USupportKind::kLatitudeOrbit;
    }
  } else {
    m.kind_ = USupportKind::kArea;
  }
  // The fast path requires two spread slots (az and roll full-turn uniform), so a classification
  // of orbit/point can never carry it; keep the flag honest.
  if (m.kind_ != USupportKind::kArea) {
    m.fast_ = false;
  }
  return m;
}

bool UMarginal::MuPositive(const double u[3], double angular_tol_rad) const {
  double probe[3] = { u[0], u[1], u[2] };
  const double n = std::sqrt(probe[0] * probe[0] + probe[1] * probe[1] + probe[2] * probe[2]);
  if (!(n > 0.0)) {
    return false;
  }
  for (int i = 0; i < 3; i++) {
    probe[i] /= n;
  }
  auto angle_to = [&](const double v[3]) {
    return std::acos(Clamp(probe[0] * v[0] + probe[1] * v[1] + probe[2] * v[2], -1.0, 1.0));
  };
  switch (kind_) {
    case USupportKind::kPoint: {
      double p[3];
      Point(p);
      return angle_to(p) <= angular_tol_rad;
    }
    case USupportKind::kSpinOrbit: {
      // On the orbit circle AND the azimuth law positive at the preimage draw.
      const double theta = SpinOrbitParameter(probe);
      double orbit[3];
      SpinOrbitPoint(theta, orbit);
      if (angle_to(orbit) > angular_tol_rad) {
        return false;
      }
      bool dirac = false;
      double at = 0.0;
      const double wrapped = WrappedSlotDensity(axis_.azimuth_dist, theta, &dirac, &at);
      return !dirac && wrapped > 0.0;
    }
    case USupportKind::kRollOrbit:
    case USupportKind::kLatitudeOrbit: {
      // Parameter inversion is not wired for these kinds in v1 (no consumer; pre-registered in
      // the header's gap list): membership is distance-to-orbit only. The orbit kinds' parameter
      // densities are still exposed through OrbitDensity for producers that have the parameter.
      double v0[3];
      OrbitAnchor(v0);
      if (kind_ == USupportKind::kRollOrbit) {
        // Circle about the body z axis through the anchor.
        const double cos_beta = v0[2];  // angular radius beta from +z: cos(beta) = v0_z
        const double cos_angle = probe[2];
        return std::fabs(std::acos(Clamp(cos_angle, -1.0, 1.0)) - std::acos(Clamp(cos_beta, -1.0, 1.0))) <=
               angular_tol_rad;
      }
      // Latitude orbit: circle about the y axis through the anchor.
      const double cos_beta = v0[1];
      const double cos_angle = probe[1];
      return std::fabs(std::acos(Clamp(cos_angle, -1.0, 1.0)) - std::acos(Clamp(cos_beta, -1.0, 1.0))) <=
             angular_tol_rad;
    }
    case USupportKind::kArea:
      return DensitySolidAngle(probe) > 0.0;
    case USupportKind::kDegenerateSunGeometry:
    case USupportKind::kDegenerateAxisGeometry:
      return false;
  }
  return false;
}

double UMarginal::DensitySolidAngle(const double u[3]) const {
  if (kind_ != USupportKind::kArea) {
    return 0.0;
  }
  if (fast_) {
    // Zonal: rho_u = m(u_z)/(2 pi), m the arcsine-mixed latitude law (Table 4 fast path).
    return ZonalMarginalAt(lat_, axis_.latitude_dist, Clamp(u[2], -1.0, 1.0), sigma_) / kTwoPi;
  }
  return DensitySolidAngleGeneral(u);
}

double UMarginal::DensitySolidAngleGeneral(const double u[3]) const {
  if (kind_ != USupportKind::kArea) {
    return 0.0;
  }
  // The general path: the roll-torus integral of the preimage sum. Two registered read-as-zero
  // families (the header's gap list): a Dirac latitude with non-uniform azimuth/roll (the fast
  // path covers the symmetric members; the Dirac-latitude preimage solve is a v2 addition), and
  // a Dirac AZIMUTH with latitude AND roll spread (every preimage term is zeroed by
  // PoseTorusDensity's Dirac guard; the (phi, roll) preimage under a fixed azimuth is v2).
  // Both read as zero everywhere — "unanswered", not "dark".
  if (lat_.kind() == LatitudeLawKind::kDiracUnfolded || lat_.kind() == LatitudeLawKind::kDiracFolded) {
    return 0.0;
  }
  double roll_at = 0.0;
  if (SlotIsDirac(axis_.roll_dist, &roll_at)) {
    // A Dirac roll lands on TWO composed slices: the raw value (no-flip branch) and the raw
    // value + pi (flip branch); each slice sees exactly one branch, the delta's unit mass.
    const double r0 = roll_at * kDegToRad;
    return PreimageSum(u, r0, false) + PreimageSum(u, r0 + kPi, true);
  }
  // Spread roll: the slice sum as a function of r has sqrt-type singularities at the fold
  // onsets — where the preimage radicand cos^2(sigma) - wy(r)^2 crosses zero, wy(r) =
  // A sin(r + delta) being the y-component of Rz(r) u. Those onsets have a CLOSED form
  // (|A sin| = cos(sigma) at r + delta = k*pi +- asin(cos(sigma)/A)), and each support window
  // is integrated with the substitution r = end +- s^2 that removes the singularity exactly
  // (verified against the exact zonal answers on the fast family; plain midpoint on the raw
  // integrand converges too slowly to be honest about).
  const double amp = std::sqrt(u[0] * u[0] + u[1] * u[1]);
  const double cos_sigma = std::sqrt(std::max(1.0 - sun_[2] * sun_[2], 0.0));
  if (!(amp > cos_sigma) || !(cos_sigma > 0.0)) {
    // The preimage radicand never crosses zero: no folds, plain midpoint over one period.
    const int panels = 512;
    double total = 0.0;
    const double dt = kTwoPi / panels;
    for (int i = 0; i < panels; i++) {
      const double r = (i + 0.5) * dt;
      total += PreimageSum(u, r, false) + PreimageSum(u, r, true);
    }
    return total * dt;
  }
  const double delta = std::atan2(u[1], u[0]);
  const double half = std::asin(Clamp(cos_sigma / amp, -1.0, 1.0));
  double total = 0.0;
  for (int k = 0; k < 2; k++) {
    const double center = -delta + k * kPi;  // unwrapped on purpose: everything is 2 pi periodic
    const double a = center - half;
    const double b = center + half;
    const double mid = 0.5 * (a + b);
    const int n = 256;
    const double smax = std::sqrt(mid - a);
    const double ds = smax / n;
    for (int i = 0; i < n; i++) {
      const double s = (i + 0.5) * ds;
      const double r = a + s * s;
      total += (PreimageSum(u, r, false) + PreimageSum(u, r, true)) * 2.0 * s * ds;
    }
    const double smax2 = std::sqrt(b - mid);
    const double ds2 = smax2 / n;
    for (int i = 0; i < n; i++) {
      const double s = (i + 0.5) * ds2;
      const double r = b - s * s;
      total += (PreimageSum(u, r, false) + PreimageSum(u, r, true)) * 2.0 * s * ds2;
    }
  }
  return total;
}

void UMarginal::SpinOrbitPoint(double theta_rad, double u_out[3]) const {
  double phi0 = 0.0, roll0 = 0.0;
  DiracLatRoll(&phi0, &roll0);
  double v[3] = { sun_[0], sun_[1], sun_[2] };
  RotateAboutZ(v, kPi - theta_rad);
  RotateAboutY(v, kHalfPi - phi0);
  RotateAboutZ(v, -roll0);
  u_out[0] = v[0];
  u_out[1] = v[1];
  u_out[2] = v[2];
}

double UMarginal::SpinOrbitThetaDensity(double theta_rad) const {
  if (kind_ != USupportKind::kSpinOrbit) {
    // The orbit laws answer their own support kind; guards mirror DensitySolidAngle's (a caller
    // reading this on an area/point/degenerate measure must not get a plausible-looking number
    // that is the density of nothing).
    return 0.0;
  }
  double at = 0.0;
  if (SlotIsDirac(axis_.azimuth_dist, &at)) {
    return 0.0;  // a Dirac azimuth has no orbit density; the support kind is not kSpinOrbit
  }
  // The orbit parameter is a CIRCLE quantity (u(theta) = u(theta + 2 pi)), so the marginal
  // density is the WRAPPED az law — the same authority MuPositive's membership leg reads. The
  // unwrapped slot value would read zero on the seam image of any support crossing +-pi (e.g.
  // Uniform(180 +- 20), a mainstream config: a producer emitting parameters in atan2's
  // (-pi, pi] convention lands half its lit arc at (-pi, -0.6 pi)) and a kFiberParameter
  // quadrature would drop those samples silently while CertifyVisibility counts them in
  // support. Uniform[0, 360] (the C12 anchor) agrees a.e. under both readings.
  bool dirac_dummy = false;
  double at_dummy = 0.0;
  return WrappedSlotDensity(axis_.azimuth_dist, theta_rad, &dirac_dummy, &at_dummy);
}

double UMarginal::OrbitDensity(double parameter_rad, double u_out[3]) const {
  if (kind_ != USupportKind::kSpinOrbit && kind_ != USupportKind::kRollOrbit && kind_ != USupportKind::kLatitudeOrbit) {
    // Orbit laws answer orbit kinds (guards mirror DensitySolidAngle's); Point/SpinOrbitPoint
    // are the maps for the other kinds.
    return 0.0;
  }
  double v[3];
  double phi0 = 0.0, roll0 = 0.0, az0 = 0.0;
  DiracLatRoll(&phi0, &roll0);
  DiracAz(&az0);
  if (kind_ == USupportKind::kRollOrbit) {
    v[0] = sun_[0];
    v[1] = sun_[1];
    v[2] = sun_[2];
    RotateAboutZ(v, kPi - az0);
    RotateAboutY(v, kHalfPi - phi0);
    RotateAboutZ(v, -parameter_rad);
    if (u_out != nullptr) {
      u_out[0] = v[0];
      u_out[1] = v[1];
      u_out[2] = v[2];
    }
    // The roll parameter is a circle quantity (Rz(-r) has period 2 pi): WRAPPED law, same seam
    // argument as SpinOrbitThetaDensity's.
    bool dirac_dummy = false;
    double at_dummy = 0.0;
    return WrappedSlotDensity(axis_.roll_dist, parameter_rad, &dirac_dummy, &at_dummy);
  }
  if (kind_ == USupportKind::kLatitudeOrbit) {
    v[0] = sun_[0];
    v[1] = sun_[1];
    v[2] = sun_[2];
    RotateAboutZ(v, kPi - az0);
    RotateAboutY(v, kHalfPi - parameter_rad);
    RotateAboutZ(v, -roll0);
    if (u_out != nullptr) {
      u_out[0] = v[0];
      u_out[1] = v[1];
      u_out[2] = v[2];
    }
    // The latitude parameter is NOT a circle quantity: the folded law already sums the fold
    // preimages and lives on [-pi/2, pi/2] — the raw (unwrapped) evaluation is the correct one.
    return lat_.Evaluate(parameter_rad);
  }
  // kSpinOrbit: parameter = theta (the azimuth draw), a circle quantity — WRAPPED law.
  SpinOrbitPoint(parameter_rad, v);
  if (u_out != nullptr) {
    u_out[0] = v[0];
    u_out[1] = v[1];
    u_out[2] = v[2];
  }
  bool dirac_dummy = false;
  double at_dummy = 0.0;
  return WrappedSlotDensity(axis_.azimuth_dist, parameter_rad, &dirac_dummy, &at_dummy);
}

void UMarginal::Point(double u_out[3]) const {
  double phi0 = 0.0, roll0 = 0.0, az0 = 0.0;
  DiracLatRoll(&phi0, &roll0);
  DiracAz(&az0);
  double v[3] = { sun_[0], sun_[1], sun_[2] };
  RotateAboutZ(v, kPi - az0);
  RotateAboutY(v, kHalfPi - phi0);
  RotateAboutZ(v, -roll0);
  u_out[0] = v[0];
  u_out[1] = v[1];
  u_out[2] = v[2];
}

// --- private helpers (declared in the class via the friend block; kept file-local here) ---

void UMarginal::DiracLatRoll(double* phi0, double* roll0) const {
  *phi0 = lat_.point();  // the Dirac latitude's value (both Dirac kinds store it in point_)
  double at = 0.0;
  SlotIsDirac(axis_.roll_dist, &at);
  *roll0 = at;
}

void UMarginal::DiracAz(double* az0) const {
  double at = 0.0;
  SlotIsDirac(axis_.azimuth_dist, &at);
  *az0 = at;
}

double UMarginal::SpinOrbitParameter(const double u[3]) const {
  // Invert SpinOrbitPoint: undo roll, undo Ry, read the sun-circle longitude.
  double phi0 = 0.0, roll0 = 0.0;
  DiracLatRoll(&phi0, &roll0);
  double v[3] = { u[0], u[1], u[2] };
  RotateAboutZ(v, roll0);
  RotateAboutY(v, phi0 - kHalfPi);
  const double psi = std::atan2(v[1], v[0]);
  return lambda_s_ + kPi - psi;
}

double UMarginal::OrbitAnchor(double u_out[3]) const {
  double phi0 = 0.0, roll0 = 0.0, az0 = 0.0;
  DiracLatRoll(&phi0, &roll0);
  DiracAz(&az0);
  double v[3] = { sun_[0], sun_[1], sun_[2] };
  RotateAboutZ(v, kPi - az0);
  if (kind_ == USupportKind::kRollOrbit) {
    RotateAboutY(v, kHalfPi - phi0);
    // (no roll rotation: the anchor is where r = 0)
  } else {
    RotateAboutZ(v, -roll0);  // latitude orbit anchor at phi = ... uses the roll only
  }
  u_out[0] = v[0];
  u_out[1] = v[1];
  u_out[2] = v[2];
  return 0.0;
}

double UMarginal::PreimageSum(const double u0[3], double roll_out, bool flip_branch) const {
  // Table 4's general path at one roll slice and one flip branch: the intersections of the sun's
  // latitude circle {w = Rz(pi - lambda) s_hat} with the y-rotation orbit of Rz(roll_out) u0.
  double x[3] = { u0[0], u0[1], u0[2] };
  RotateAboutZ(x, roll_out);
  const double sin_sigma = sun_[2];
  const double wy = x[1];
  const double radicand = 1.0 - sin_sigma * sin_sigma - wy * wy;
  if (!(radicand >= 0.0)) {
    return 0.0;
  }
  const double wx_mag = std::sqrt(radicand);
  const double cos_sigma = std::sqrt(std::max(1.0 - sin_sigma * sin_sigma, 0.0));
  // The roll factor of this branch at this slice: the wrapped roll law at the branch's raw value
  // (no-flip: the slice itself; flip: the slice minus pi), or the delta's unit mass for a Dirac
  // roll whose slice the caller pinned to exactly this branch.
  double roll_at = 0.0;
  double roll_factor = 0.0;
  if (SlotIsDirac(axis_.roll_dist, &roll_at)) {
    const double raw = flip_branch ? roll_out - kPi : roll_out;
    roll_factor = std::fabs(raw - roll_at * kDegToRad) < 1e-9 ? 1.0 : 0.0;
  } else {
    bool dirac_dummy = false;
    double at_dummy = 0.0;
    roll_factor = WrappedSlotDensity(axis_.roll_dist, flip_branch ? roll_out - kPi : roll_out, &dirac_dummy, &at_dummy);
  }
  if (!(roll_factor > 0.0)) {
    return 0.0;
  }
  double total = 0.0;
  for (int sign = 0; sign < 2; sign++) {
    if (sign == 1 && !(wx_mag > 0.0)) {
      break;  // tangent preimage: the two sign branches coincide, count it once
    }
    const double wx = sign == 0 ? wx_mag : -wx_mag;
    const double w[3] = { wx, wy, sin_sigma };
    const double psi = std::atan2(w[1], w[0]);        // w's longitude
    const double lambda_out = lambda_s_ + kPi - psi;  // Rz(pi - lambda) s_hat = w
    // phi from Ry(pi/2 - phi) w = x. The y-rotation acts in the (x, z) plane of w's and x's
    // coordinates: beta's cosine is w.x*x.x + w.z*x.z — the y-components are INVARIANT under the
    // rotation and must NOT enter (w . x as a whole is not the rotation angle's cosine unless
    // w happens to be perpendicular to the y axis). sin(beta) = w.z*x.x - w.x*x.z.
    const double cos_beta = w[0] * x[0] + w[2] * x[2];
    const double sin_beta = w[2] * x[0] - w[0] * x[2];
    const double beta = std::atan2(sin_beta, cos_beta);
    const double phi = kHalfPi - beta;
    if (phi <= -kHalfPi || phi >= kHalfPi) {
      continue;
    }
    // The Jacobian |cos(sigma) cos(psi)| (Table 4).
    const double jac = std::fabs(cos_sigma * std::cos(psi));
    if (!(jac > 1e-15)) {
      continue;  // degenerate preimage: w at a pole of the sun circle kills lambda's freedom
    }
    total += PoseTorusDensity(lat_, axis_.azimuth_dist, lambda_out, phi, flip_branch, roll_factor) / jac;
  }
  return total;
}

}  // namespace lumice::raypath
