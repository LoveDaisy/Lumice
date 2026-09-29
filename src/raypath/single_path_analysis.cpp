#include "raypath/single_path_analysis.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

#include "analytic/entry_measure.hpp"
#include "analytic/fiber_continuation.hpp"
#include "analytic/path_chain.hpp"
#include "analytic/so3.hpp"
#include "raypath/scene_to_analytic.hpp"
#include "util/sky_direction.hpp"

namespace lumice::raypath {

const char* const kCompletenessNote =
    "procedural, not a certificate: complete means only that every admissible candidate of this sample "
    "closed or became an arc, never that every component of the fiber was found; a denser sample can "
    "lose a component a sparser one found, so pass earlier component seeds back as warm seeds to keep "
    "them (LI phase-1 contract sections 9.5.6-9.5.7)";

const char* ErrorCodeName(ErrorCode code) {
  switch (code) {
    case ErrorCode::kOk:
      return "ok";
    case ErrorCode::kUnknownCrystalId:
      return "unknown_crystal_id";
    case ErrorCode::kMultiLayerUnsupported:
      return "multi_layer_unsupported";
    case ErrorCode::kInvalidPath:
      return "invalid_path";
    case ErrorCode::kFaceNotInCrystal:
      return "face_not_in_crystal";
    case ErrorCode::kWavelengthOutOfRange:
      return "wavelength_out_of_range";
    case ErrorCode::kInvalidTarget:
      return "invalid_target";
    case ErrorCode::kInvalidArgument:
      return "invalid_argument";
    case ErrorCode::kCrystalRejected:
      return "crystal_rejected";
    case ErrorCode::kPathInfeasible:
      return "path_infeasible";
  }
  return "unknown";
}

const char* WavelengthSourceName(WavelengthSource source) {
  switch (source) {
    case WavelengthSource::kDefault:
      return "default";
    case WavelengthSource::kConfigSingle:
      return "config";
    case WavelengthSource::kUser:
      return "user";
  }
  return "unknown";
}

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

constexpr double kPi = 3.14159265358979323846;
constexpr double kRad2Deg = 180.0 / kPi;
constexpr double kDeg2Rad = kPi / 180.0;
constexpr double kIdentity[9] = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };

// Below this sin(zenith) the azimuth and roll of a pose are not separately recoverable in double
// (their sum or difference is), so PoseToAngles folds the pair into azimuth.
constexpr double kPoleSine = 1e-12;

// Sun directions probed for rank 0 (the antipodal Fibonacci lattice discovery samples on).
constexpr int kRankProbeCount = 4096;

double WrapDeg(double a) {
  a = std::remainder(a, 360.0);  // [-180, 180]
  return a <= -180.0 ? a + 360.0 : a;
}

double Clamp1(double x) {
  return x < -1.0 ? -1.0 : (x > 1.0 ? 1.0 : x);
}

double AngleBetween(const double a[3], const double b[3]) {
  double cross[3];
  analytic::so3::Cross3(a, b, cross);
  return std::atan2(analytic::so3::Norm3(cross), analytic::so3::Dot3(a, b));
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
      w[i] = normal[i] / n * kPi;
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

// Everything that is fixed for one call: the crystal, the resolved path, the index, the sun.
class PathContext {
 public:
  PathContext(const analytic::FaceNormalTable& table, const analytic::FacePolygonTable& polygons,
              std::vector<int> slots, double refractive_index, const double incident[3])
      : table_(table), slots_(std::move(slots)), n_(refractive_index),
        incident_{ incident[0], incident[1], incident[2] }, corridor_(table_, polygons, slots_.data(), SlotCount()),
        segments_(3 * (slots_.size() + 1)), transmittances_(slots_.size()) {}

  int SlotCount() const { return static_cast<int>(slots_.size()); }

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
    *deviation = std::acos(Clamp1(analytic::so3::Dot3(out, incident)));
    return true;
  }

  double EntryMeasure(const double incident_body[3]) { return corridor_.Evaluate(incident_body, n_).value; }

  PointDetail Detail(const double pose[9], double residual_norm) {
    PointDetail d;
    for (int i = 0; i < 9; i++) {
      d.pose[i] = pose[i];
    }
    d.angles = PoseToAngles(pose);
    SunInCrystal(pose, incident_, d.sun_in_crystal);
    d.residual_norm = residual_norm;
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
    return d;
  }

  const std::vector<int>& Slots() const { return slots_; }
  const double* Incident() const { return incident_; }
  double Index() const { return n_; }

 private:
  const analytic::FaceNormalTable& table_;
  std::vector<int> slots_;
  double n_;
  double incident_[3];
  analytic::Corridor corridor_;
  std::vector<double> segments_;
  std::vector<double> transmittances_;
};

struct RankProbe {
  int valid_count = 0;
  bool rank_zero = false;
  double first_valid_u[3]{};
};

// Rank 0 = the outgoing direction does not move with the pose anywhere the path is realisable. That
// happens exactly when the deviation is 0 (or pi) at every valid pose: fix the sun's place u in the
// crystal and turn the crystal about the sun — the outgoing ray turns about the sun with it, so an
// outgoing direction that never moves must lie on the sun's axis; conversely an outgoing direction
// equal to +-incident everywhere is one direction. The test reads the deviation itself, whose
// rounding error stays at the double ulp scale, rather than the Jacobian's rank: an exact plate's
// Jacobian singular value measures 1e-10..1e-8 near grazing incidence, where the Snell square root
// amplifies rounding, which is the tracer's own 1e-8 rank gate. The tolerance is the kernel's for
// "this direction is that unit vector". Probed on the antipodal Fibonacci lattice discovery samples
// on; the deviation depends on the pose only through u.
RankProbe ProbeRank(const PathContext& ctx) {
  RankProbe probe;
  bool all_forward = true;   // deviation 0: outgoing == incident
  bool all_backward = true;  // deviation pi: outgoing == -incident
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
    const double incident[3] = { -u[0], -u[1], -u[2] };
    const double deviation = AngleBetween(out, incident);
    all_forward = all_forward && deviation <= analytic::kUnitTolerance;
    all_backward = all_backward && deviation >= kPi - analytic::kUnitTolerance;
    if (!all_forward && !all_backward) {
      return probe;  // rank_zero stays false
    }
  }
  probe.rank_zero = probe.valid_count > 0;
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
    out.points.push_back(ctx->Detail(t.poses.data() + 9 * static_cast<size_t>(k), t.residual_norms[k]));
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

void BuildSunGrid(int lat_count, PathContext* ctx, SunSphereGrid* grid) {
  grid->lat_count = lat_count;
  grid->lon_count = 2 * lat_count;
  const size_t cells = static_cast<size_t>(grid->lat_count) * static_cast<size_t>(grid->lon_count);
  grid->deviation_rad.assign(cells, std::numeric_limits<double>::quiet_NaN());
  grid->valid.assign(cells, 0);
  grid->entry_measure.assign(cells, 0.0);
  for (int i = 0; i < grid->lat_count; i++) {
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
DeviationRange ProbeDeviationRange(const PathContext& ctx, int lat_count) {
  SunSphereGrid layout;
  layout.lat_count = lat_count;
  layout.lon_count = 2 * lat_count;
  const int lon_count = layout.lon_count;
  const size_t cells = static_cast<size_t>(lat_count) * static_cast<size_t>(lon_count);
  std::vector<double> centre(3 * cells);
  std::vector<uint8_t> valid(cells, 0);
  DeviationRange range;
  for (int i = 0; i < lat_count; i++) {
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

ReachSummary ProbeReach(const PathContext& ctx, double target_deviation) {
  ReachSummary reach;
  reach.probe_lat_count = kReachProbeLatCount;
  reach.target_deviation_rad = target_deviation;
  const DeviationRange fine = ProbeDeviationRange(ctx, kReachProbeLatCount);
  const DeviationRange coarse = ProbeDeviationRange(ctx, kReachProbeLatCount / 2);
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
  reach.tolerance_rad =
      coarse.any ? kHalfOrderBound * std::max(std::fabs(fine.lo - coarse.lo), std::fabs(fine.hi - coarse.hi)) : kPi;
  reach.target_in_range = target_deviation >= reach.deviation_min_rad - reach.tolerance_rad &&
                          target_deviation <= reach.deviation_max_rad + reach.tolerance_rad;
  return reach;
}

}  // namespace

PoseAngles PoseToAngles(const double r[9]) {
  // R = Rz(a) Ry(-zenith) Rz(roll), a = azimuth - 180:
  //   third column = (-sin zenith cos a, -sin zenith sin a, cos zenith)
  //   third row    = ( sin zenith cos roll, -sin zenith sin roll, cos zenith)
  PoseAngles angles;
  const double sin_zenith = std::hypot(r[2], r[5]);
  angles.zenith_deg = std::atan2(sin_zenith, r[8]) * kRad2Deg;
  double a = 0.0;
  if (sin_zenith > kPoleSine) {
    a = std::atan2(-r[5], -r[2]);
    angles.roll_deg = WrapDeg(std::atan2(-r[7], r[6]) * kRad2Deg);
  } else {
    // zenith 0: R = Rz(a + roll); zenith 180: R = Rz(a) diag(-1, 1, -1) Rz(roll). Roll := 0.
    angles.degenerate = true;
    a = r[8] > 0.0 ? std::atan2(r[3], r[0]) : std::atan2(-r[3], -r[0]);
    angles.roll_deg = 0.0;
  }
  angles.azimuth_deg = WrapDeg(a * kRad2Deg + 180.0);
  return angles;
}

void SunSphereGridCellCentre(const SunSphereGrid& grid, int lat_index, int lon_index, double u[3]) {
  const double lat = (-90.0 + (lat_index + 0.5) * 180.0 / grid.lat_count) * kDeg2Rad;
  const double lon = (-180.0 + (lon_index + 0.5) * 360.0 / grid.lon_count) * kDeg2Rad;
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
  meta.target_deviation_deg = delta * kRad2Deg;
  meta.wavelength_nm = wavelength.wavelength_nm;
  meta.wavelength_source = wavelength.source;
  meta.refractive_index = wavelength.refractive_index;
  const analytic::ContinuationParams params;
  meta.sample_count = request.sample_count;
  meta.band_half_width_rad = analytic::kDefaultBandHalfWidth;
  meta.cluster_radius_rad = analytic::kDefaultClusterRadius;
  meta.distance_threshold_rad = params.closure_distance;
  meta.warm_seed_count = warm_count;

  PathContext ctx(table, polygons, slots, wavelength.refractive_index, meta.incident_direction);

  // ---- rank 0, and whether the path is realisable at all ----
  const RankProbe rank = ProbeRank(ctx);
  if (rank.valid_count == 0) {
    return { ErrorCode::kPathInfeasible, "no pose realises this face sequence (none of " +
                                             std::to_string(kRankProbeCount) + " probed sun directions is valid)" };
  }
  if (!rank.rank_zero && !(delta > 0.0 && delta < kPi)) {
    return { ErrorCode::kInvalidTarget,
             "the target coincides with the sun or its antipode; no fiber is defined there" };
  }

  if (request.sun_grid_lat_count > 0) {
    BuildSunGrid(request.sun_grid_lat_count, &ctx, &result.sun_grid);
  }

  if (rank.rank_zero) {
    result.outcome = Outcome::kPointMass;
    const double sun[3] = { -meta.incident_direction[0], -meta.incident_direction[1], -meta.incident_direction[2] };
    double pose[9];
    RotationTaking(rank.first_valid_u, sun, pose);
    const PointDetail probe = ctx.Detail(pose, 0.0);
    for (int i = 0; i < 3; i++) {
      result.point_mass.direction[i] = probe.outgoing_direction[i];
    }
    DirToAltAz(result.point_mass.direction, &result.point_mass.altitude_deg, &result.point_mass.azimuth_deg);
    result.point_mass.target_separation_deg =
        AngleBetween(result.point_mass.direction, meta.target_direction) * kRad2Deg;
    *out = std::move(result);
    return {};
  }

  // ---- discovery and the fibers ----
  result.outcome = Outcome::kDiscovered;
  result.reach = ProbeReach(ctx, delta);
  analytic::IceDiscovery discovery(table, polygons, ctx.Slots().data(), ctx.SlotCount(), ctx.Index(), ctx.Incident());
  const analytic::DiscoveryOutput found =
      discovery.Discover(meta.target_direction, request.sample_count, meta.band_half_width_rad,
                         request.warm_seeds.empty() ? nullptr : request.warm_seeds.data(), warm_count,
                         meta.cluster_radius_rad, meta.distance_threshold_rad, params);
  for (const analytic::DiscoveredComponent& c : found.components) {
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

}  // namespace lumice::raypath
