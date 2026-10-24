#include "raypath/single_path_analysis.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

#include "analytic/entry_measure.hpp"
#include "analytic/fiber_continuation.hpp"
#include "analytic/path_chain.hpp"
#include "analytic/path_rank.hpp"
#include "analytic/so3.hpp"
#include "raypath/detail/measure/declared_density.hpp"
#include "raypath/detail/measure/fiber_quadrature.hpp"
#include "raypath/detail/survey/config_fingerprint.hpp"
#include "raypath/detail/survey/survey_assembly.hpp"
#include "raypath/detail/survey/survey_document.hpp"
#include "raypath/scene_to_analytic.hpp"
#include "util/sky_direction.hpp"

namespace lumice::raypath {

const char* const kCompletenessNote =
    "procedural, not a certificate: complete means only that every admissible candidate of this sample "
    "closed or became an arc, never that every component of the fiber was found; a denser sample can "
    "lose a component a sparser one found, so pass earlier component seeds back as warm seeds to keep "
    "them (LI phase-1 contract sections 9.5.6-9.5.7)";

const char* ComponentKindName(ComponentKind kind) {
  return kind == ComponentKind::kArc ? "arc" : "closed";
}

const char* TraceStatusName(TraceStatus status) {
  switch (status) {
    case TraceStatus::kClosed:
      return "closed";
    case TraceStatus::kEventTerminated:
      return "event_terminated";
    case TraceStatus::kNumericalFailure:
      return "numerical_failure";
    case TraceStatus::kBudgetExhausted:
      return "budget_exhausted";
  }
  return "unknown";
}

const char* TraceReasonName(TraceReason reason) {
  switch (reason) {
    case TraceReason::kClosedLoop:
      return "closed_loop";
    case TraceReason::kTirBoundary:
      return "tir_boundary";
    case TraceReason::kBranchBoundary:
      return "branch_boundary";
    case TraceReason::kPathInfeasible:
      return "path_infeasible";
    case TraceReason::kVisibilityBoundary:
      return "visibility_boundary";
    case TraceReason::kChartBoundary:
      return "chart_boundary";
    case TraceReason::kRankLoss:
      return "rank_loss";
    case TraceReason::kTopologyAmbiguity:
      return "topology_ambiguity";
    case TraceReason::kCorrectorFailure:
      return "corrector_failure";
    case TraceReason::kLinearSolveFailure:
      return "linear_solve_failure";
    case TraceReason::kNonFinite:
      return "non_finite";
    case TraceReason::kStepUnderflow:
      return "step_underflow";
    case TraceReason::kInvalidNumericalInput:
      return "invalid_numerical_input";
    case TraceReason::kStepBudget:
      return "step_budget";
    case TraceReason::kArclengthBudget:
      return "arclength_budget";
    case TraceReason::kEvaluationBudget:
      return "evaluation_budget";
  }
  return "unknown";
}

const char* IncompleteCauseName(IncompleteCause cause) {
  switch (cause) {
    case IncompleteCause::kArcBackwardFailed:
      return "arc_backward_failed";
    case IncompleteCause::kArcBackwardClosedAnomaly:
      return "arc_backward_closed_anomaly";
    case IncompleteCause::kUnnamedEvent:
      return "unnamed_event";
    case IncompleteCause::kNotConverged:
      return "not_converged";
  }
  return "unknown";
}

namespace {

constexpr double kIdentity[9] = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };

// Below this sin(zenith) the azimuth and roll of a pose are not separately recoverable in double
// (their sum or difference is), so PoseToAngles folds the pair into azimuth.
constexpr double kPoleSine = 1e-12;

// Sun directions probed for rank 0 (the antipodal Fibonacci lattice discovery samples on).
constexpr int kRankProbeCount = 4096;

// The declared measure's support kinds, spelled for the registration block (the v1 gap list's own
// vocabulary; declared_density.hpp carries no name table, so this is the one wire spelling).
const char* USupportKindName(USupportKind kind) {
  switch (kind) {
    case USupportKind::kPoint:
      return "point";
    case USupportKind::kSpinOrbit:
      return "spin_orbit";
    case USupportKind::kRollOrbit:
      return "roll_orbit";
    case USupportKind::kLatitudeOrbit:
      return "latitude_orbit";
    case USupportKind::kArea:
      return "area";
    case USupportKind::kDegenerateSunGeometry:
      return "degenerate_sun_geometry";
    case USupportKind::kDegenerateAxisGeometry:
      return "degenerate_axis_geometry";
  }
  return "unknown";
}

// The declared measure's registration for the per-pose rho read: the u-marginal's support kind and
// the registered read-as-zero gaps (declared_density.hpp's v1 gap list) a consumer must know about
// before treating a rho of 0 as dark. The joint pose density itself is closed-form at every pose;
// the gaps live one reduction up, at the u-marginal a solid-angle consumer would read.
MeasureRegistration RegisterMeasure(const UMarginal& marginal, const AxisDistribution& axis) {
  MeasureRegistration out;
  out.support_kind = USupportKindName(marginal.kind());
  out.fast_path = marginal.fast_path();
  double at = 0.0;
  const bool az_dirac = SlotIsDirac(axis.azimuth_dist, &at);
  const bool lat_dirac = SlotIsDirac(axis.latitude_dist, &at);
  if (marginal.kind() == USupportKind::kDegenerateSunGeometry) {
    out.registered_gap = true;
    out.note = "sun at a body pole: the declared measure answers nothing here (kDegenerateSunGeometry)";
  } else if (marginal.kind() == USupportKind::kDegenerateAxisGeometry) {
    out.registered_gap = true;
    out.note =
        "axis Dirac at a pole with azimuth and roll both spread: not answered before v2 "
        "(kDegenerateAxisGeometry)";
  } else if (marginal.kind() == USupportKind::kArea && az_dirac) {
    out.registered_gap = true;
    out.note =
        "registered read-as-zero gap: a fixed azimuth with zenith and roll both spread reads "
        "rho_u = 0 everywhere at the u-marginal (declared_density.hpp v1 gap list)";
  } else if (marginal.kind() == USupportKind::kArea && lat_dirac) {
    out.registered_gap = true;
    out.note =
        "registered read-as-zero gap: a fixed zenith with non-uniform azimuth and spread roll reads "
        "rho_u = 0 everywhere at the u-marginal (declared_density.hpp v1 gap list)";
  }
  return out;
}


double WrapDeg(double a) {
  a = std::remainder(a, 360.0);  // [-180, 180]
  return a <= -180.0 ? a + 360.0 : a;
}

// A rotation R with R u = s, for unit u and s. Which one does not matter to any caller: the path's
// outgoing direction is equivariant under a rotation about s, and every caller only needs a pose
// whose sun sits at u.
void RotationTaking(const double u[3], const double s[3], double r[9]) {
  double axis[3];
  analytic::so3::Cross3(u, s, axis);
  const double sine = analytic::so3::Norm3(axis);
  const double cosine = analytic::so3::Dot3(u, s);
  double w[3] = { 0.0, 0.0, 0.0 };
  if (sine > 1e-12) {
    const double angle = std::atan2(sine, cosine);
    for (int i = 0; i < 3; i++) {
      w[i] = axis[i] / sine * angle;
    }
  } else if (cosine < 0.0) {
    // Antiparallel: a half-turn about any axis normal to u.
    const double ref[3] = { std::fabs(u[0]) < 0.9 ? 1.0 : 0.0, std::fabs(u[0]) < 0.9 ? 0.0 : 1.0, 0.0 };
    double normal[3];
    analytic::so3::Cross3(u, ref, normal);
    const double n = analytic::so3::Norm3(normal);
    for (int i = 0; i < 3; i++) {
      w[i] = normal[i] / n * math::kPiD;
    }
  }
  analytic::so3::Exp(w, r);
}

// u = R^T s_hat, s_hat = -incident: the sun direction in the crystal frame.
void SunInCrystal(const double pose[9], const double incident[3], double u[3]) {
  for (int k = 0; k < 3; k++) {
    u[k] = -(pose[0 * 3 + k] * incident[0] + pose[1 * 3 + k] * incident[1] + pose[2 * 3 + k] * incident[2]);
  }
}

// Everything that is fixed for one call: the crystal, the resolved path, the index, the sun, and
// the orientation measure the declared pose density reads.
class PathContext {
 public:
  PathContext(const analytic::FaceNormalTable& table, const analytic::FacePolygonTable& polygons,
              std::vector<int> slots, double refractive_index, const double incident[3], const AxisDistribution& axis)
      : table_(table), slots_(std::move(slots)), n_(refractive_index),
        incident_{ incident[0], incident[1], incident[2] }, axis_(axis),
        lat_(axis.IsFullSphereUniform() ? MakeFullSphereLatitude() : MakeLatitudeDensity(axis.latitude_dist)),
        corridor_(table_, polygons, slots_.data(), SlotCount()), segments_(3 * (slots_.size() + 1)),
        transmittances_(slots_.size()) {}

  int SlotCount() const { return static_cast<int>(slots_.size()); }

  // The declared joint pose density (w.r.t. dlambda dphi droll, radians) at the pose's composed
  // angles: the config's own orientation angles are the composed ones (azimuth = lambda_out,
  // zenith = pi/2 - phi_out, roll = roll_out — the same chart both specs spell).
  double PoseDensity(const double pose[9]) const {
    const PoseAngles angles = PoseToAngles(pose);
    return DeclaredPoseDensity(lat_, axis_, angles.azimuth_deg * math::kDegreeToRadD,
                               (90.0 - angles.zenith_deg) * math::kDegreeToRadD, angles.roll_deg * math::kDegreeToRadD);
  }

  // Body-frame evaluation with the sun at u (pose = identity, incident = -u): what discovery's band
  // evaluates (discovery.cpp BuildBand). Returns validity; `out` the outgoing direction.
  bool EvaluateAtSun(const double u[3], double out[3]) const {
    const double incident[3] = { -u[0], -u[1], -u[2] };
    return analytic::TracePathChain<double>(table_, slots_.data(), SlotCount(), n_, incident, kIdentity, out, nullptr,
                                            nullptr);
  }

  // D_P(u): the deviation between the incident and outgoing directions with the sun at u, as
  // discovery's band measures it (discovery.cpp BuildBand). Returns validity; `deviation` is set
  // only when valid. The one definition of D that the sun grid and the reach probe both read.
  bool DeviationAtSun(const double u[3], double* deviation) const {
    double out[3];
    if (!EvaluateAtSun(u, out)) {
      return false;
    }
    const double incident[3] = { -u[0], -u[1], -u[2] };
    *deviation = std::acos(math::Clamp1(analytic::so3::Dot3(out, incident)));
    return true;
  }

  double EntryMeasure(const double incident_body[3]) { return corridor_.Evaluate(incident_body, n_).value; }

  PointDetail Detail(const double pose[9], double residual_norm) {
    return Detail(pose, residual_norm, false, 0.0, nullptr);
  }

  // The factor-decomposition overload: the continuation's own per-pose Jacobian read (NaN-honest
  // — unavailable poses carry NaN, never a plausible 0), then k_carea and the factor product.
  PointDetail Detail(const double pose[9], double residual_norm, bool jacobian_available, double jacobian,
                     const double* singular_values) {
    PointDetail d;
    for (int i = 0; i < 9; i++) {
      d.pose[i] = pose[i];
    }
    d.angles = PoseToAngles(pose);
    SunInCrystal(pose, incident_, d.sun_in_crystal);
    d.residual_norm = residual_norm;
    d.rho = PoseDensity(pose);
    d.jacobian_available = jacobian_available;
    if (jacobian_available) {
      d.jacobian = jacobian;
      if (singular_values != nullptr) {
        d.singular_values[0] = singular_values[0];
        d.singular_values[1] = singular_values[1];
      }
      d.k_carea = 1.0 / (jacobian + kFactorsEpsilon);
    } else {
      constexpr double nan = std::numeric_limits<double>::quiet_NaN();
      d.jacobian = nan;
      d.singular_values[0] = nan;
      d.singular_values[1] = nan;
      d.k_carea = nan;
    }
    analytic::PathOutputs outputs{};
    outputs.segment_directions = segments_.data();
    outputs.interface_transmittances = transmittances_.data();
    d.valid = analytic::EvaluatePath(table_, slots_.data(), SlotCount(), n_, incident_, pose, &outputs);
    if (d.valid) {
      for (int i = 0; i < 3; i++) {
        d.outgoing_direction[i] = outputs.outgoing_direction[i];
      }
      d.segment_directions = segments_;
      d.interface_transmittances = transmittances_;
      d.total_transmission = outputs.fresnel_transmission;
      const double incident_body[3] = { -d.sun_in_crystal[0], -d.sun_in_crystal[1], -d.sun_in_crystal[2] };
      d.entry_measure = EntryMeasure(incident_body);
    }
    d.entry_measure_a2 = d.entry_measure;  // a = the crystal length unit = 1 in this repo's configs
    if (d.valid && d.jacobian_available) {
      d.factor_product = d.rho * d.entry_measure_a2 * d.total_transmission * d.k_carea;
    } else {
      d.factor_product = std::numeric_limits<double>::quiet_NaN();
    }
    return d;
  }

  const std::vector<int>& Slots() const { return slots_; }
  const analytic::FaceNormalTable& Table() const { return table_; }
  const double* Incident() const { return incident_; }
  double Index() const { return n_; }

 private:
  const analytic::FaceNormalTable& table_;
  std::vector<int> slots_;
  double n_;
  double incident_[3];
  AxisDistribution axis_;
  LatitudeDensity lat_;
  analytic::Corridor corridor_;
  std::vector<double> segments_;
  std::vector<double> transmittances_;
};

struct RankProbe {
  int valid_count = 0;
  bool rank_zero = false;
  double first_valid_u[3]{};
};

// Rank 0 is the analytic library's criterion (analytic/path_rank.hpp: fold matrix I and zero wedge,
// LI docs/band-sum-contract.md section 5), the one the band sum's point mass reads too. It is a
// property of the geometry; what the probe adds is whether the path is realisable at all and a sun
// direction where it is, sampled on the antipodal Fibonacci lattice discovery samples on (the
// fields depend on the pose only through u). The rank-0 set equals "the deviation is 0 at every
// valid probed pose" on every realisable path of up to four faces on the prism and three on the
// pyramid (test_path_rank.cpp). A deviation of pi at every pose would need M = -I (three mutually
// perpendicular reflecting faces), which no closed-form crystal has.
RankProbe ProbeRank(const PathContext& ctx) {
  RankProbe probe;
  for (int i = 0; i < kRankProbeCount; i++) {
    double u[3];
    analytic::LatticePoint(kRankProbeCount, i, u);
    double out[3];
    if (!ctx.EvaluateAtSun(u, out)) {
      continue;
    }
    if (probe.valid_count++ == 0) {
      for (int k = 0; k < 3; k++) {
        probe.first_valid_u[k] = u[k];
      }
    }
  }
  probe.rank_zero = probe.valid_count > 0 && analytic::IsRankZeroPath(ctx.Table(), ctx.Slots().data(), ctx.SlotCount());
  return probe;
}

TraceEnd EndOf(const analytic::TraceResult& trace) {
  return { trace.status, trace.reason, trace.PoseCount() };
}

FiberComponent ToComponent(const analytic::DiscoveredComponent& c, PathContext* ctx) {
  FiberComponent out;
  out.kind = c.kind;
  for (int i = 0; i < 9; i++) {
    out.seed[i] = c.seed[i];
  }
  out.forward = EndOf(c.forward);
  const int nf = c.forward.PoseCount();
  auto append = [&](const analytic::TraceResult& t, int k) {
    out.points.push_back(ctx->Detail(t.poses.data() + 9 * static_cast<size_t>(k), t.residual_norms[k],
                                     t.jacobian_available[static_cast<size_t>(k)] != 0,
                                     t.normal_jacobian[static_cast<size_t>(k)],
                                     t.singular_values.data() + 2 * static_cast<size_t>(k)));
  };
  if (c.kind == ComponentKind::kArc) {
    out.backward = EndOf(c.backward);
    const int nb = c.backward.PoseCount();
    // Backward reversed, its seed (pose 0) left to the forward trace.
    for (int k = nb - 1; k >= 1; k--) {
      append(c.backward, k);
    }
    for (int k = nb - 2; k >= 0; k--) {
      out.arclength_increments.push_back(c.backward.arclength_increments[k]);
    }
    out.seed_index = nb >= 1 ? nb - 1 : 0;
  }
  for (int k = 0; k < nf; k++) {
    append(c.forward, k);
  }
  out.arclength_increments.insert(out.arclength_increments.end(), c.forward.arclength_increments.begin(),
                                  c.forward.arclength_increments.end());
  // The component's factor integral: left-endpoint rectangular sum of factor_product * arclength
  // over consecutive point pairs whose first point carries a usable product (finite — NaN marks
  // an unusable read). Available points counted for the completeness declaration.
  out.factors.total_points = static_cast<int>(out.points.size());
  for (const PointDetail& p : out.points) {
    if (std::isfinite(p.factor_product)) {
      out.factors.available_points++;
    }
  }
  const size_t segments = out.arclength_increments.size();
  for (size_t i = 0; i < segments; i++) {
    const double product = out.points[i].factor_product;
    if (!std::isfinite(product)) {
      continue;
    }
    out.factors.fiber_integral += product * out.arclength_increments[i];
    out.factors.integral_available = true;
  }
  return out;
}

IncompleteComponent ToIncomplete(const analytic::IncompleteCandidate& c) {
  IncompleteComponent out;
  out.cause = c.cause;
  for (int i = 0; i < 9; i++) {
    out.seed[i] = c.seed[i];
  }
  out.forward = EndOf(c.forward);
  const bool backward_run =
      c.cause == IncompleteCause::kArcBackwardFailed || c.cause == IncompleteCause::kArcBackwardClosedAnomaly;
  if (backward_run) {
    out.backward = EndOf(c.backward);
  } else {
    out.backward.pose_count = 0;
  }
  return out;
}

void BuildSunGrid(int lat_count, PathContext* ctx, SunSphereGrid* grid, const analytic::DiscoveryStop* stop) {
  grid->lat_count = lat_count;
  grid->lon_count = 2 * lat_count;
  const size_t cells = static_cast<size_t>(grid->lat_count) * static_cast<size_t>(grid->lon_count);
  grid->deviation_rad.assign(cells, std::numeric_limits<double>::quiet_NaN());
  grid->valid.assign(cells, 0);
  grid->entry_measure.assign(cells, 0.0);
  for (int i = 0; i < grid->lat_count; i++) {
    if (stop != nullptr && stop->Stopped()) {
      break;  // the stop is the document's meta.interrupted; the unwritten cells stay NaN/invalid
    }
    for (int j = 0; j < grid->lon_count; j++) {
      double u[3];
      SunSphereGridCellCentre(*grid, i, j, u);
      double deviation = 0.0;
      if (!ctx->DeviationAtSun(u, &deviation)) {
        continue;
      }
      const size_t cell = static_cast<size_t>(i) * static_cast<size_t>(grid->lon_count) + static_cast<size_t>(j);
      const double incident[3] = { -u[0], -u[1], -u[2] };
      grid->deviation_rad[cell] = deviation;
      grid->valid[cell] = 1;
      grid->entry_measure[cell] = ctx->EntryMeasure(incident);
    }
  }
}

// Bisection steps along one grid edge: the edge is at most one cell (0.5 deg at 360 rows) long, so
// 50 halvings leave ~1e-17 rad, whose square root (D's behaviour at the boundary) is ~3e-9 rad.
constexpr int kReachBisectionSteps = 50;

struct DeviationRange {
  bool any = false;
  double lo = 0.0;
  double hi = 0.0;
  void Add(double d) {
    lo = any ? std::min(lo, d) : d;
    hi = any ? std::max(hi, d) : d;
    any = true;
  }
};

// The rank-0 branch's factor decomposition: the declared measure's spin orbit, evaluated at
// midpoint samples, integrated by the measure layer's own quadrature (the kFiberParameter
// binding's v1 semantics — the LI #51 tint kernel's integral form). A measure whose support is
// not a spin orbit gets the honest unavailable state, never a defaulted integral.
OrbitFactors BuildOrbitFactors(const UMarginal& marginal, PathContext& ctx, const double sun_hat[3],
                               const analytic::DiscoveryStop* stop) {
  OrbitFactors orbit;
  orbit.measure_support = USupportKindName(marginal.kind());
  if (marginal.kind() != USupportKind::kSpinOrbit) {
    orbit.available = false;
    orbit.reason = "measure_support_not_spin_orbit";
    return orbit;
  }
  orbit.available = true;
  orbit.sample_count = kOrbitFactorSampleCount;
  FiberSampleStream stream;
  stream.binding = MeasureBinding::kFiberParameter;
  stream.evidence = FiberSampleStream::EvidenceForm::kSampledExhaustive;
  stream.source_name = "raypath point orbit factors";
  stream.samples.resize(static_cast<size_t>(orbit.sample_count));
  const double dtheta = 2.0 * math::kPiD / orbit.sample_count;
  for (int i = 0; i < orbit.sample_count; i++) {
    if (stop != nullptr && stop->Stopped()) {
      // A partial grid's integral would claim a coverage it does not have: the honest shape is
      // the unavailable state naming the stop.
      orbit.available = false;
      orbit.reason = "interrupted";
      orbit.points.clear();
      orbit.integral = 0.0;
      orbit.kept_count = 0;
      return orbit;
    }
    const double theta = 2.0 * math::kPiD * (i + 0.5) / orbit.sample_count;
    double u[3];
    marginal.SpinOrbitPoint(theta, u);
    double pose[9];
    RotationTaking(u, sun_hat, pose);
    const PointDetail d = ctx.Detail(pose, 0.0);
    OrbitFactorPoint point;
    point.theta_rad = theta;
    point.rho_theta = marginal.SpinOrbitThetaDensity(theta);
    point.entry_measure_a2 = d.entry_measure_a2;
    point.total_transmission = d.total_transmission;
    point.valid = d.valid;
    orbit.points.push_back(point);
    FiberSample& sample = stream.samples[static_cast<size_t>(i)];
    for (int c = 0; c < 3; c++) {
      sample.u[c] = u[c];
    }
    sample.area = d.entry_measure;
    sample.transmission = d.total_transmission;
    sample.valid = d.valid;
    sample.parameter = theta;
    sample.weight = dtheta;
  }
  const FiberQuadratureResult q = QuadratureIntensity(marginal, stream, /*angular_tol_rad=*/0.0);
  orbit.integral = q.intensity;
  orbit.kept_count = q.kept;
  return orbit;
}

// The last valid point on the segment from a valid u_valid towards an invalid u_invalid (normalised
// chords; both are one grid cell apart). Adds D there.
void AddBoundaryDeviation(const PathContext& ctx, const double u_valid[3], const double u_invalid[3],
                          DeviationRange* range) {
  double a[3] = { u_valid[0], u_valid[1], u_valid[2] };
  double b[3] = { u_invalid[0], u_invalid[1], u_invalid[2] };
  double best = 0.0;
  ctx.DeviationAtSun(a, &best);
  for (int step = 0; step < kReachBisectionSteps; step++) {
    double m[3] = { a[0] + b[0], a[1] + b[1], a[2] + b[2] };
    const double norm = analytic::so3::Norm3(m);
    for (double& c : m) {
      c /= norm;
    }
    double deviation = 0.0;
    if (ctx.DeviationAtSun(m, &deviation)) {
      best = deviation;
      std::copy(m, m + 3, a);
    } else {
      std::copy(m, m + 3, b);
    }
  }
  range->Add(best);
}

// Range of D on the reach probe at `lat_count` rows: valid cell centres plus the bisected validity
// boundary on every valid/invalid neighbour edge (latitude neighbours, and longitude neighbours
// across the periodic seam).
DeviationRange ProbeDeviationRange(const PathContext& ctx, int lat_count, const analytic::DiscoveryStop* stop) {
  SunSphereGrid layout;
  layout.lat_count = lat_count;
  layout.lon_count = 2 * lat_count;
  const int lon_count = layout.lon_count;
  const size_t cells = static_cast<size_t>(lat_count) * static_cast<size_t>(lon_count);
  std::vector<double> centre(3 * cells);
  std::vector<uint8_t> valid(cells, 0);
  DeviationRange range;
  for (int i = 0; i < lat_count; i++) {
    if (stop != nullptr && stop->Stopped()) {
      break;
    }
    for (int j = 0; j < lon_count; j++) {
      const size_t cell = static_cast<size_t>(i) * static_cast<size_t>(lon_count) + static_cast<size_t>(j);
      double* u = centre.data() + 3 * cell;
      SunSphereGridCellCentre(layout, i, j, u);
      double deviation = 0.0;
      if (ctx.DeviationAtSun(u, &deviation)) {
        valid[cell] = 1;
        range.Add(deviation);
      }
    }
  }
  auto edge = [&](size_t p, size_t q) {
    if (valid[p] != valid[q]) {
      const size_t v = valid[p] ? p : q;
      const size_t w = valid[p] ? q : p;
      AddBoundaryDeviation(ctx, centre.data() + 3 * v, centre.data() + 3 * w, &range);
    }
  };
  for (int i = 0; i < lat_count; i++) {
    for (int j = 0; j < lon_count; j++) {
      const size_t cell = static_cast<size_t>(i) * static_cast<size_t>(lon_count) + static_cast<size_t>(j);
      edge(cell, static_cast<size_t>(i) * static_cast<size_t>(lon_count) + static_cast<size_t>((j + 1) % lon_count));
      if (i + 1 < lat_count) {
        edge(cell, cell + static_cast<size_t>(lon_count));
      }
    }
  }
  return range;
}

ReachSummary ProbeReach(const PathContext& ctx, double target_deviation, const analytic::DiscoveryStop* stop) {
  ReachSummary reach;
  reach.probe_lat_count = kReachProbeLatCount;
  reach.target_deviation_rad = target_deviation;
  const DeviationRange fine = ProbeDeviationRange(ctx, kReachProbeLatCount, stop);
  const DeviationRange coarse = ProbeDeviationRange(ctx, kReachProbeLatCount / 2, stop);
  if (!fine.any) {
    reach.deviation_min_rad = std::numeric_limits<double>::quiet_NaN();
    reach.deviation_max_rad = std::numeric_limits<double>::quiet_NaN();
    reach.tolerance_rad = std::numeric_limits<double>::quiet_NaN();
    reach.target_in_range = true;
    return reach;
  }
  reach.deviation_min_rad = fine.lo;
  reach.deviation_max_rad = fine.hi;
  // A coarse probe that sees nothing valid has no change to offer: nothing is excluded.
  // Half-order convergence bound (single_path_analysis.hpp, ReachSummary): shortfall <= c / (sqrt 2 - 1).
  constexpr double kHalfOrderBound = 2.41421356237309504880;  // 1 + sqrt 2
  reach.tolerance_rad = coarse.any ?
                            kHalfOrderBound * std::max(std::fabs(fine.lo - coarse.lo), std::fabs(fine.hi - coarse.hi)) :
                            math::kPiD;
  reach.target_in_range = target_deviation >= reach.deviation_min_rad - reach.tolerance_rad &&
                          target_deviation <= reach.deviation_max_rad + reach.tolerance_rad;
  return reach;
}

}  // namespace

// ---- the survey context ---------------------------------------------------------------------------

// The schema-4 value model's harvest: the facts the context reads, read once off the assembled
// document (the warm reader's JSON arm mirrors this — the two must read the same blocks, the
// dual-arm oracle test pins them field-for-field). Members are the card's first layer: the
// support partition ran on the entry faces, and both arms read that same layer, so a future
// multi-layer document changes the answer on both sides together or not at all. A support_set
// with zero intervals harvests nothing — has_support stays false and the context answers
// survey_carries_no_support, the same shape as a document without support at all.
SurveyWarmSource HarvestSurveyDocument(const survey::SurveyDocument& doc) {
  SurveyWarmSource out;
  out.early_stopped = doc.early_stopped;
  out.has_wavelength = std::isfinite(doc.wavelength_nm) && doc.wavelength_nm > 0.0;
  out.wavelength_nm = doc.wavelength_nm;
  out.wavelength_source = doc.wavelength_source;
  out.has_sun_altitude = doc.sun_altitude_deg.has_value() && std::isfinite(*doc.sun_altitude_deg);
  out.sun_altitude_deg = doc.sun_altitude_deg.value_or(0.0);
  out.has_sun_azimuth = doc.sun_azimuth_deg.has_value() && std::isfinite(*doc.sun_azimuth_deg);
  out.sun_azimuth_deg = doc.sun_azimuth_deg.value_or(0.0);
  for (const survey::SurveyObject& card : doc.objects) {
    if (card.support_set.has_value()) {
      for (const survey::SurveyObject::Interval& interval : card.support_set->intervals) {
        SurveyWarmSource::SupportInterval out_interval;
        out_interval.lower_deg = interval.lower_deg;
        out_interval.upper_deg = interval.upper_deg;
        out_interval.members = card.members.empty() ? std::vector<int>{} : card.members.front();
        out.intervals.push_back(std::move(out_interval));
      }
    }
    if (card.caustic_edge.has_value()) {
      out.has_edge = true;
      out.edge_deg = card.caustic_edge->position_deg;
    }
  }
  out.has_support = !out.intervals.empty();
  if (!doc.probes.empty()) {
    const survey::SurveyProbe& probe = doc.probes.front();
    out.has_probe = true;
    out.has_probe_azimuth = probe.spec.azimuth_deg.has_value() && std::isfinite(*probe.spec.azimuth_deg);
    out.probe_azimuth_deg = probe.spec.azimuth_deg.value_or(0.0);
    out.probe_bin_deg = probe.spec.bin_deg;
    out.probe_sample_count = static_cast<int>(probe.spec.sample_count);
    for (const survey::SurveyProbeSample& sample : probe.samples) {
      out.probe_samples.push_back({ sample.delta_deg, sample.value, sample.k, sample.k_eff });
    }
  }
  out.has_measure_echo = doc.family_routing.has_value();
  if (out.has_measure_echo) {
    const auto read_dim = [](const survey::FamilyDimMeasure& dim, SurveyWarmSource::MeasureDimEcho* e) {
      e->kind = survey::FamilyDimKindName(dim.kind);
      e->center_deg = dim.center_deg;
      e->width_deg = dim.width_deg;
    };
    read_dim(doc.family_routing->measure.zenith, &out.measure_zenith);
    read_dim(doc.family_routing->measure.azimuth, &out.measure_azimuth);
    read_dim(doc.family_routing->measure.roll, &out.measure_roll);
  }
  return out;
}

SurveyReuseDecision EvaluateSurveyReuse(const SurveyWarmSource& warm, double wavelength_nm, double sun_altitude_deg,
                                        double sun_azimuth_deg) {
  SurveyReuseDecision decision;
  if (warm.early_stopped) {
    decision.structure.reason = "survey_early_stopped_no_delta_axis";
  } else if (!warm.has_support) {
    decision.structure.reason = "survey_carries_no_support";
  } else if (!warm.has_wavelength) {
    decision.structure.reason = "scope_field_missing";
  } else if (warm.wavelength_nm != wavelength_nm) {
    decision.structure.reason = "scope_value_mismatch";
  } else {
    decision.structure.warm = &warm;
  }
  if (!warm.has_wavelength || !warm.has_sun_altitude || !warm.has_sun_azimuth) {
    decision.reading_reason = "scope_field_missing";
  } else if (warm.wavelength_nm != wavelength_nm || warm.sun_altitude_deg != sun_altitude_deg ||
             warm.sun_azimuth_deg != sun_azimuth_deg) {
    decision.reading_reason = "scope_value_mismatch";
  }
  return decision;
}

bool NearestStructureAnchor(const SurveyStructureView& structure, double deviation_deg, double* position_deg,
                            const char** basis) {
  if (!structure) {
    return false;
  }
  const SurveyWarmSource& warm = *structure.warm;
  bool has = false;
  double best = 0.0;
  const char* best_basis = "";
  for (const SurveyWarmSource::SupportInterval& interval : warm.intervals) {
    for (const double end : { interval.lower_deg, interval.upper_deg }) {
      if (!has || std::fabs(deviation_deg - end) < std::fabs(deviation_deg - best)) {
        best = end;
        best_basis = "support_interval_endpoint";
        has = true;
      }
    }
  }
  if (warm.has_edge && (!has || std::fabs(deviation_deg - warm.edge_deg) < std::fabs(deviation_deg - best))) {
    best = warm.edge_deg;
    best_basis = "caustic_edge_position";
    has = true;
  }
  if (has) {
    *position_deg = best;
    *basis = best_basis;
  }
  return has;
}

// The context's answers, read off a harvest (warm document or inline assembly — the same shape).
void FillSurveyContext(const SurveyWarmSource& warm, double target_deviation_deg, double wavelength_nm,
                       double sun_altitude_deg, double sun_azimuth_deg, SurveyContextBlock* out) {
  const SurveyReuseDecision reuse = EvaluateSurveyReuse(warm, wavelength_nm, sun_altitude_deg, sun_azimuth_deg);
  const double delta = target_deviation_deg;
  const SurveyWarmSource::SupportInterval* containing = nullptr;
  const SurveyWarmSource::SupportInterval* closest = nullptr;
  double closest_distance = std::numeric_limits<double>::infinity();
  if (reuse.structure) {
    for (const SurveyWarmSource::SupportInterval& interval : warm.intervals) {
      if (delta >= interval.lower_deg && delta <= interval.upper_deg) {
        containing = &interval;
        break;
      }
      const double d = delta < interval.lower_deg ? interval.lower_deg - delta : delta - interval.upper_deg;
      if (d < closest_distance) {
        closest_distance = d;
        closest = &interval;
      }
    }
  }
  if (!reuse.structure) {
    out->object.state = "unevaluated";
    out->object.reason = reuse.structure.reason;
    out->edge.state = "unevaluated";
    out->edge.reason = reuse.structure.reason;
  } else {
    // The partition containment is a certified structural fact; the nearest reading inherits it.
    const SurveyWarmSource::SupportInterval& hit = containing != nullptr ? *containing : *closest;
    out->object.state = "certified";
    out->target_inside_support = containing != nullptr;
    out->object_kind = "support_set";
    out->object_members = hit.members;
    out->object_distance_deg = containing != nullptr ? 0.0 : closest_distance;
    // A certified support carries endpoints, so the shared walk always names an anchor here.
    double edge_position = 0.0;
    const char* edge_basis = "";
    NearestStructureAnchor(reuse.structure, delta, &edge_position, &edge_basis);
    out->edge.state = "finite";
    out->edge_distance_deg = std::fabs(delta - edge_position);
    out->edge_basis = edge_basis;
  }

  // The band-sum reading: warm documents only, under the scope gate's exact-equality comparison
  // (both sides resolved the same way, so equal configs give bit-equal doubles here).
  if (out->source == "inline") {
    out->band_sum.state = "unevaluated";
    out->band_sum.reason = "inline_context_runs_no_probe";
  } else if (out->warm_index_absent) {
    out->band_sum.state = "unevaluated";
    out->band_sum.reason = "warm_index_absent";
  } else if (!warm.has_probe) {
    out->band_sum.state = "unevaluated";
    out->band_sum.reason = "warm_document_has_no_probe";
  } else if (reuse.reading_reason != nullptr) {
    out->band_sum.state = "unevaluated";
    out->band_sum.reason = reuse.reading_reason;
  } else {
    const SurveyWarmSource::ProbeSample* best = nullptr;
    double best_distance = std::numeric_limits<double>::infinity();
    for (const SurveyWarmSource::ProbeSample& sample : warm.probe_samples) {
      if (std::isnan(sample.value)) {
        continue;  // an undefined reading is no reading: the pick spans the defined ones
      }
      const double d = std::fabs(delta - sample.delta_deg);
      if (d < best_distance) {
        best_distance = d;
        best = &sample;
      }
    }
    if (best == nullptr) {
      out->band_sum.state = "unevaluated";
      out->band_sum.reason = "warm_document_has_no_probe";
    } else {
      out->band_sum.state = "finite";
      out->band_sum_value = best->value;
      out->band_sum_delta_deg = best->delta_deg;
      out->band_sum_bin_deg = warm.probe_bin_deg;
      out->band_sum_caliber = "the warm document's solar-radial band-sum profile at wavelength " +
                              std::to_string(warm.wavelength_nm) + " nm (" + warm.wavelength_source + "), sun at " +
                              std::to_string(warm.sun_altitude_deg) + "/" + std::to_string(warm.sun_azimuth_deg) +
                              ", survey line azimuth " +
                              (warm.has_probe_azimuth ? std::to_string(warm.probe_azimuth_deg) : "unknown") +
                              " — reference-line reading, not target-sky intensity";
    }
  }
}

namespace {

// The survey context for this query: warm harvest when a schema-4 document passed the gates, the
// context-only T0-T2 assembly otherwise (and also under the warm_index_absent degradation, which
// still declares itself in the block).
void BuildSurveyContext(const ConfigManager& config, const SinglePathRequest& request, const SinglePathMetadata& meta,
                        SurveyContextBlock* out) {
  if (request.warm_survey.has_value()) {
    out->source = "warm";
    out->warm_index_absent = !request.warm_survey->index_present;
  } else {
    out->source = "inline";
  }
  const SurveyWarmSource* warm =
      request.warm_survey.has_value() && request.warm_survey->index_present ? &*request.warm_survey : nullptr;
  SurveyWarmSource inline_harvest;
  if (warm == nullptr) {
    SurveyRequest survey_request;
    survey_request.crystal_id = request.crystal_id;
    survey_request.path_layers = request.path_layers;
    survey_request.symmetry_bits = survey::kDefaultSymmetryBits;
    survey_request.wavelength_nm = meta.wavelength_nm;
    survey::SurveyDocument doc;
    const Error e = survey::AssembleSurvey(config, survey_request, &doc, survey::SurveyAssemblyOptions{ true });
    if (!e.Ok()) {
      out->object.state = "unevaluated";
      out->object.reason = "inline_survey_refused";
      out->edge.state = "unevaluated";
      out->edge.reason = "inline_survey_refused";
      out->band_sum.state = "unevaluated";
      out->band_sum.reason = "inline_survey_refused";
      return;
    }
    inline_harvest = HarvestSurveyDocument(doc);
    warm = &inline_harvest;
  }
  FillSurveyContext(*warm, meta.target_deviation_deg, meta.wavelength_nm, meta.sun_altitude_deg, meta.sun_azimuth_deg,
                    out);
}

}  // namespace

PoseAngles PoseToAngles(const double r[9]) {
  // R = Rz(a) Ry(-zenith) Rz(roll), a = azimuth - 180:
  //   third column = (-sin zenith cos a, -sin zenith sin a, cos zenith)
  //   third row    = ( sin zenith cos roll, -sin zenith sin roll, cos zenith)
  PoseAngles angles;
  const double sin_zenith = std::hypot(r[2], r[5]);
  angles.zenith_deg = std::atan2(sin_zenith, r[8]) * math::kRadToDegreeD;
  double a = 0.0;
  if (sin_zenith > kPoleSine) {
    a = std::atan2(-r[5], -r[2]);
    angles.roll_deg = WrapDeg(std::atan2(-r[7], r[6]) * math::kRadToDegreeD);
  } else {
    // zenith 0: R = Rz(a + roll); zenith 180: R = Rz(a) diag(-1, 1, -1) Rz(roll). Roll := 0.
    angles.degenerate = true;
    a = r[8] > 0.0 ? std::atan2(r[3], r[0]) : std::atan2(-r[3], -r[0]);
    angles.roll_deg = 0.0;
  }
  angles.azimuth_deg = WrapDeg(a * math::kRadToDegreeD + 180.0);
  return angles;
}

void SunSphereGridCellCentre(const SunSphereGrid& grid, int lat_index, int lon_index, double u[3]) {
  const double lat = (-90.0 + (lat_index + 0.5) * 180.0 / grid.lat_count) * math::kDegreeToRadD;
  const double lon = (-180.0 + (lon_index + 0.5) * 360.0 / grid.lon_count) * math::kDegreeToRadD;
  u[0] = std::cos(lat) * std::cos(lon);
  u[1] = std::cos(lat) * std::sin(lon);
  u[2] = std::sin(lat);
}

Error AnalyzeSinglePath(const ConfigManager& config, const SinglePathRequest& request, SinglePathResult* out) {
  *out = SinglePathResult{};

  // ---- inputs ----
  const auto crystal_it = config.crystals_.find(request.crystal_id);
  if (crystal_it == config.crystals_.end()) {
    return { ErrorCode::kUnknownCrystalId,
             "no crystal entry with id " + std::to_string(request.crystal_id) + " in the config" };
  }
  if (request.sample_count < 1 || request.sample_count > kMaxSampleCount) {
    return { ErrorCode::kInvalidArgument, "sample count must be in [1, " + std::to_string(kMaxSampleCount) + "]; got " +
                                              std::to_string(request.sample_count) };
  }
  if (request.sun_grid_lat_count < 0 || request.sun_grid_lat_count > kMaxSunGridLatCount) {
    return { ErrorCode::kInvalidArgument, "sun-grid latitude count must be in [0, " +
                                              std::to_string(kMaxSunGridLatCount) + "]; got " +
                                              std::to_string(request.sun_grid_lat_count) };
  }
  if (request.warm_seeds.size() % 9 != 0) {
    return { ErrorCode::kInvalidArgument, "warm seeds must be 9 numbers (a row-major rotation) each" };
  }
  const int warm_count = static_cast<int>(request.warm_seeds.size() / 9);
  for (int i = 0; i < warm_count; i++) {
    if (!analytic::ValidateRotation(request.warm_seeds.data() + 9 * static_cast<size_t>(i))) {
      return { ErrorCode::kInvalidArgument, "warm seed " + std::to_string(i) + " is not a rotation" };
    }
  }
  if (!std::isfinite(request.target_altitude_deg) || !std::isfinite(request.target_azimuth_deg) ||
      request.target_altitude_deg < -90.0 || request.target_altitude_deg > 90.0) {
    return { ErrorCode::kInvalidTarget, "the target altitude must be finite and in [-90, 90], the azimuth finite" };
  }

  SinglePathResult result;
  SinglePathMetadata& meta = result.meta;
  meta.analytic_api_version = analytic::kApiVersion;
  meta.crystal_id = request.crystal_id;

  const LightSourceConfig& light = config.scene_.light_source_;
  WavelengthChoice wavelength;
  if (Error e = ResolveWavelength(light, request.wavelength_nm, &wavelength); !e.Ok()) {
    return e;
  }

  const CrystalConversion crystal = ConvertCrystal(crystal_it->second.param_);
  analytic::FaceNormalTable table;
  analytic::FacePolygonTable polygons;
  switch (analytic::BuildFaceNormals(crystal.shape, &table, &polygons)) {
    case analytic::Status::kOk:
      break;
    case analytic::Status::kInvalidConfig:
      return { ErrorCode::kCrystalRejected,
               "the engine builds no crystal from entry " + std::to_string(request.crystal_id) + "'s nominal shape" };
    case analytic::Status::kInvalidValue:
      return { ErrorCode::kCrystalRejected, "crystal entry " + std::to_string(request.crystal_id) +
                                                "'s nominal shape has a value the kernel cannot take" };
  }

  std::vector<int> slots;
  if (Error e = ResolveSingleLayerPath(request.path_layers, table, &slots); !e.Ok()) {
    return e;
  }

  meta.crystal_kind = crystal.kind;
  meta.shape = crystal.scalars;
  meta.upper_wedge_deg = crystal.upper_wedge_deg;
  meta.lower_wedge_deg = crystal.lower_wedge_deg;
  meta.shape_is_nominal = crystal.shape_is_nominal;
  meta.faces = request.path_layers.front();
  meta.sun_altitude_deg = light.param_.altitude_;
  meta.sun_azimuth_deg = light.param_.azimuth_;
  meta.sun_diameter_deg = light.param_.diameter_;
  SunIncidentDirection(light.param_, meta.incident_direction);
  meta.target_altitude_deg = request.target_altitude_deg;
  meta.target_azimuth_deg = request.target_azimuth_deg;
  AltAzToDir(request.target_altitude_deg, request.target_azimuth_deg, meta.target_direction);
  const double delta = analytic::TargetDeviation(meta.incident_direction, meta.target_direction);
  meta.target_deviation_deg = delta * math::kRadToDegreeD;
  meta.wavelength_nm = wavelength.wavelength_nm;
  meta.wavelength_source = wavelength.source;
  meta.refractive_index = wavelength.refractive_index;
  const analytic::ContinuationParams params;
  // The budget, resolved once at entry; polled at loop boundaries only. A stopped run serializes
  // its partial document with meta.interrupted — an honest early exit, not a failure.
  const analytic::DiscoveryStop stop{ request.budget_ms > 0 ? std::chrono::steady_clock::now() +
                                                                  std::chrono::milliseconds(request.budget_ms) :
                                                              std::chrono::steady_clock::time_point::max(),
                                      request.interrupted };
  const auto stopped = [&stop]() { return stop.Stopped(); };
  meta.budget_ms = request.budget_ms > 0 ? request.budget_ms : 0;
  meta.sample_count = request.sample_count;
  meta.band_half_width_rad = analytic::kDefaultBandHalfWidth;
  meta.cluster_radius_rad = analytic::kDefaultClusterRadius;
  meta.distance_threshold_rad = params.closure_distance;
  meta.warm_seed_count = warm_count;

  PathContext ctx(table, polygons, slots, wavelength.refractive_index, meta.incident_direction,
                  crystal_it->second.axis_);

  // The declared measure: one u-marginal for the registration and (rank 0) the orbit integral.
  const double sun_hat[3] = { -meta.incident_direction[0], -meta.incident_direction[1], -meta.incident_direction[2] };
  const UMarginal marginal = MakeUMarginal(crystal_it->second.axis_, sun_hat);
  result.measure_registration = RegisterMeasure(marginal, crystal_it->second.axis_);

  // ---- rank 0, and whether the path is realisable at all ----
  const RankProbe rank = ProbeRank(ctx);
  if (rank.valid_count == 0) {
    return { ErrorCode::kPathInfeasible, "no pose realises this face sequence (none of " +
                                             std::to_string(kRankProbeCount) + " probed sun directions is valid)" };
  }
  if (!rank.rank_zero && !(delta > 0.0 && delta < math::kPiD)) {
    return { ErrorCode::kInvalidTarget,
             "the target coincides with the sun or its antipode; no fiber is defined there" };
  }

  if (request.sun_grid_lat_count > 0) {
    BuildSunGrid(request.sun_grid_lat_count, &ctx, &result.sun_grid, &stop);
    if (stopped()) {
      meta.interrupted = true;
    }
  }

  // The survey context defers (SurveyContextOf / EnsureSurveyContext): the inline branch's
  // context-only T0-T2 assembly IS the delta partition — measured as the analysis's dominant
  // cost — so it must not tax consumers that never read the context. The bridge's one-shot
  // serialization computes and caches it; the deadline gate stays on the discovery legs only.
  result.context_config = std::make_shared<const ConfigManager>(config);
  result.context_request = request;

  if (rank.rank_zero) {
    result.outcome = Outcome::kPointMass;
    const double sun[3] = { sun_hat[0], sun_hat[1], sun_hat[2] };
    double pose[9];
    RotationTaking(rank.first_valid_u, sun, pose);
    const PointDetail probe = ctx.Detail(pose, 0.0);
    for (int i = 0; i < 3; i++) {
      result.point_mass.direction[i] = probe.outgoing_direction[i];
    }
    DirToAltAz(result.point_mass.direction, &result.point_mass.altitude_deg, &result.point_mass.azimuth_deg);
    result.point_mass.target_separation_deg =
        analytic::so3::AngleBetween(result.point_mass.direction, meta.target_direction) * math::kRadToDegreeD;
    result.point_mass.orbit_factors = BuildOrbitFactors(marginal, ctx, sun_hat, &stop);
    if (stopped()) {
      meta.interrupted = true;
    }
    *out = std::move(result);
    return {};
  }

  // ---- discovery and the fibers ----
  result.outcome = Outcome::kDiscovered;
  result.reach = ProbeReach(ctx, delta, &stop);
  if (stopped()) {
    meta.interrupted = true;
  }
  analytic::IceDiscovery discovery(table, polygons, ctx.Slots().data(), ctx.SlotCount(), ctx.Index(), ctx.Incident());
  const analytic::DiscoveryOutput found =
      discovery.Discover(meta.target_direction, request.sample_count, meta.band_half_width_rad,
                         request.warm_seeds.empty() ? nullptr : request.warm_seeds.data(), warm_count,
                         meta.cluster_radius_rad, meta.distance_threshold_rad, params, &stop);
  if (stopped()) {
    // The honest stop marker is meta.interrupted; discovery.complete below keeps its procedural
    // candidate-conversion semantics — a band stopped mid-way may still read complete, because
    // the lattice points it never explored never became candidates.
    meta.interrupted = true;
  }
  for (const analytic::DiscoveredComponent& c : found.components) {
    if (stopped()) {
      meta.interrupted = true;
      break;  // the unconverted components stay unconverted: the document is partial, marked so
    }
    result.components.push_back(ToComponent(c, &ctx));
  }
  for (const analytic::IncompleteCandidate& c : found.incomplete) {
    result.incomplete.push_back(ToIncomplete(c));
  }
  DiscoverySummary& s = result.discovery;
  s.complete = found.Complete();
  s.pool_count = found.pool_count;
  s.extra_seed_count = found.extra_seed_count;
  s.raw_cluster_count = found.raw_cluster_count;
  s.admissible_count = found.admissible_count;
  s.dedup_merged = found.dedup_merged;
  s.arc_stitched = found.arc_stitched;
  s.arc_backward_failed = found.arc_backward_failed;
  s.arc_backward_closed_anomaly = found.arc_backward_closed_anomaly;
  s.incomplete_unnamed_event = found.incomplete_unnamed_event;
  s.incomplete_not_converged = found.incomplete_not_converged;
  *out = std::move(result);
  return {};
}


SurveyContextBlock SurveyContextOf(const SinglePathResult& result) {
  if (result.survey_context_ready) {
    return result.survey_context;
  }
  SurveyContextBlock out;
  if (result.context_config != nullptr) {
    BuildSurveyContext(*result.context_config, result.context_request, result.meta, &out);
  }
  return out;
}

void EnsureSurveyContext(SinglePathResult* result) {
  if (result->survey_context_ready) {
    return;
  }
  result->survey_context = SurveyContextOf(*result);
  result->survey_context_ready = true;
  result->context_config.reset();
  result->context_request = SinglePathRequest{};
}

}  // namespace lumice::raypath
