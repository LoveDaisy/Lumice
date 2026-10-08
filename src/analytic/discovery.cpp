#include "analytic/discovery.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

#include "analytic/path_chain.hpp"

namespace lumice::analytic {

namespace {

constexpr double kPi = 3.14159265358979323846;

double Clamp1(double x) {
  return x < -1.0 ? -1.0 : (x > 1.0 ? 1.0 : x);
}

void Normalize3(double v[3]) {
  const double n = so3::Norm3(v);
  for (int i = 0; i < 3; i++) {
    v[i] /= n;
  }
}

}  // namespace

bool IsArcEvent(FiberReason reason) {
  switch (reason) {
    case FiberReason::kTirBoundary:
    case FiberReason::kBranchBoundary:
    case FiberReason::kPathInfeasible:
    case FiberReason::kVisibilityBoundary:
    case FiberReason::kChartBoundary:
      return true;
    default:
      return false;
  }
}

std::vector<std::vector<int>> GeodesicCluster(const double* poses, int count, double radius) {
  std::vector<std::vector<int>> clusters;
  std::vector<char> assigned(static_cast<size_t>(count), 0);
  for (int centre = 0; centre < count; centre++) {
    if (assigned[centre]) {
      continue;
    }
    // Every index below `centre` is assigned, so `centre` is the lowest unassigned one. Membership is
    // the distance to the centre, not transitive.
    std::vector<int> members;
    const double* c = poses + 9 * static_cast<size_t>(centre);
    for (int j = centre; j < count; j++) {
      if (!assigned[j] && so3::Distance(c, poses + 9 * static_cast<size_t>(j)) < radius) {
        members.push_back(j);
        assigned[j] = 1;
      }
    }
    if (members.empty()) {
      // A centre is always within a positive radius of itself; only a non-positive radius or a NaN
      // pose leaves it out, and then it still forms its own cluster rather than looping forever.
      members.push_back(centre);
      assigned[centre] = 1;
    }
    clusters.push_back(std::move(members));
  }
  return clusters;
}

int Representative(const std::vector<int>& cluster, int extra_count, const double* band_offsets) {
  assert(!cluster.empty());
  if (cluster.front() < extra_count) {
    return cluster.front();  // members are ascending: the first is the lowest-index extra seed
  }
  int best = cluster.front();
  for (int i : cluster) {
    if (band_offsets[i - extra_count] < band_offsets[best - extra_count]) {
      best = i;
    }
  }
  return best;
}

double DistanceToCurve(const double r[9], const TraceResult& trace) {
  double best = std::numeric_limits<double>::infinity();
  const int n = trace.PoseCount();
  for (int k = 0; k < n; k++) {
    best = std::fmin(best, so3::Distance(r, trace.poses.data() + 9 * static_cast<size_t>(k)));
  }
  return best;
}

void LatticePoint(int n, int i, double u[3]) {
  // LI fibonacci_sphere: i + 0.5; z = 1 - 2 i / n; phi = pi (1 + sqrt 5) i; r = sqrt(max(0, 1 - z^2)).
  const double k = static_cast<double>(i) + 0.5;
  const double z = 1.0 - 2.0 * k / static_cast<double>(n);
  const double phi = kPi * (1.0 + std::sqrt(5.0)) * k;
  const double r = std::sqrt(std::fmax(0.0, 1.0 - z * z));
  u[0] = -(r * std::cos(phi));
  u[1] = -(r * std::sin(phi));
  u[2] = -z;
}

double TargetDeviation(const double incident_direction[3], const double target_direction[3]) {
  return std::acos(Clamp1(so3::Dot3(target_direction, incident_direction)));
}

void CandidatePose(const double incident_direction[3], const double target_direction[3], const BandEvent& event,
                   double r[9]) {
  const double sun[3] = { -incident_direction[0], -incident_direction[1], -incident_direction[2] };
  const double ds = so3::Dot3(target_direction, sun);
  double e[3] = { target_direction[0] - ds * sun[0], target_direction[1] - ds * sun[1],
                  target_direction[2] - ds * sun[2] };
  Normalize3(e);
  double se[3];
  so3::Cross3(sun, e, se);
  const double cos_d = std::cos(event.deviation);
  double f[3] = { event.phi[0] + cos_d * event.u[0], event.phi[1] + cos_d * event.u[1],
                  event.phi[2] + cos_d * event.u[2] };
  Normalize3(f);
  double uf[3];
  so3::Cross3(event.u, f, uf);
  const double* w_cols[3] = { sun, e, se };
  const double* f_cols[3] = { event.u, f, uf };
  // R_ij = sum_k W_ik F_jk, with W_ik = component i of W's column k.
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      r[i * 3 + j] = w_cols[0][i] * f_cols[0][j] + w_cols[1][i] * f_cols[1][j] + w_cols[2][i] * f_cols[2][j];
    }
  }
}

namespace {
// The resolved face slots, copied by value so the path map can point at the owner's copy from the
// moment it is constructed (no window in which it sees zeros).
std::array<int, kMaxFaceCount> CopySlots(const int* slots, int slot_count) {
  std::array<int, kMaxFaceCount> out{};
  for (int k = 0; k < slot_count && k < kMaxFaceCount; k++) {
    out[k] = slots[k];
  }
  return out;
}
}  // namespace

IceDiscovery::IceDiscovery(const FaceNormalTable& table, const FacePolygonTable& polygons, const int* slots,
                           int slot_count, double refractive_index, const double incident_direction[3])
    : table_(&table), slots_(CopySlots(slots, slot_count)), slot_count_(slot_count),
      refractive_index_(refractive_index),
      incident_{ incident_direction[0], incident_direction[1], incident_direction[2] },
      map_(table, slots_.data(), slot_count, refractive_index, incident_direction),
      corridor_(table, polygons, slots, slot_count), segments_(3 * (static_cast<size_t>(slot_count) + 1)),
      transmittances_(static_cast<size_t>(slot_count)) {
  assert(slot_count >= 2 && slot_count <= kMaxFaceCount);
}

bool IceDiscovery::EvaluateLatticePoint(int sample_count, int i, BandEvent* event, double* transmission) {
  event->index = i;
  LatticePoint(sample_count, i, event->u);
  // The fields depend on the pose only through u (section 9.5.2): at the identity pose the body and
  // world frames coincide and the incident propagation direction is -u.
  const double incident[3] = { -event->u[0], -event->u[1], -event->u[2] };
  const double identity[9] = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };
  PathOutputs detail{};
  detail.segment_directions = segments_.data();
  detail.interface_transmittances = transmittances_.data();
  if (!TracePathChain<double>(*table_, slots_.data(), slot_count_, refractive_index_, incident, identity, event->phi,
                              &detail, nullptr)) {
    return false;
  }
  event->deviation = std::acos(Clamp1(so3::Dot3(event->phi, incident)));
  *transmission = detail.fresnel_transmission;
  return true;
}

double IceDiscovery::EntryMeasureAt(const BandEvent& event) {
  const double incident[3] = { -event.u[0], -event.u[1], -event.u[2] };
  return corridor_.Evaluate(incident, refractive_index_).value;
}

namespace {
bool ByDeviationThenIndex(const BandEvent& a, const BandEvent& b) {
  return a.deviation < b.deviation || (a.deviation == b.deviation && a.index < b.index);
}
}  // namespace

std::vector<BandEvent> IceDiscovery::BuildBand(int sample_count, double delta, double half_width) {
  std::vector<BandEvent> band;
  const double lo = delta - half_width;
  const double hi = delta + half_width;
  for (int i = 0; i < sample_count; i++) {
    BandEvent event;
    double transmission = 0.0;
    if (!EvaluateLatticePoint(sample_count, i, &event, &transmission)) {
      continue;
    }
    if (!(event.deviation >= lo && event.deviation <= hi)) {
      continue;
    }
    // w = A T > 0.
    if (!(transmission > 0.0) || EntryMeasureAt(event) <= 0.0) {
      continue;
    }
    band.push_back(event);
  }
  std::sort(band.begin(), band.end(), ByDeviationThenIndex);
  return band;
}

std::vector<SampleEvent> IceDiscovery::BuildEvents(int sample_count) {
  std::vector<SampleEvent> events;
  for (int i = 0; i < sample_count; i++) {
    SampleEvent event;
    double transmission = 0.0;
    if (!EvaluateLatticePoint(sample_count, i, &event.event, &transmission)) {
      continue;
    }
    event.weight = EntryMeasureAt(event.event) * transmission;
    if (event.weight > 0.0) {
      events.push_back(event);
    }
  }
  std::sort(events.begin(), events.end(),
            [](const SampleEvent& a, const SampleEvent& b) { return ByDeviationThenIndex(a.event, b.event); });
  return events;
}

bool IceDiscovery::Admit(const TargetChart& chart, const ContinuationParams& params, const double raw[9],
                         double seed[9]) {
  const double tau = params.residual_tolerance + params.relative_residual_tolerance;
  const double norm = NewtonCorrect(map_, chart, tau * kNewtonStopFraction, raw, seed);
  if (!(norm <= tau)) {
    return false;
  }
  double outgoing[3];
  if (!TracePathChain<double>(*table_, slots_.data(), slot_count_, refractive_index_, incident_, seed, outgoing,
                              nullptr, nullptr)) {
    return false;
  }
  double s_body[3];
  for (int i = 0; i < 3; i++) {
    s_body[i] = seed[0 * 3 + i] * incident_[0] + seed[1 * 3 + i] * incident_[1] + seed[2 * 3 + i] * incident_[2];
  }
  return corridor_.Evaluate(s_body, refractive_index_).value > 0.0;
}

DiscoveryOutput IceDiscovery::DiscoverOnBand(const double target_direction[3], const std::vector<BandEvent>& band,
                                             const double* extra_seeds, int extra_count, double cluster_radius,
                                             double distance_threshold, const ContinuationParams& params) {
  DiscoveryOutput out;
  out.pool_count = static_cast<int>(band.size());
  out.extra_seed_count = extra_count;
  const double delta = TargetDeviation(incident_, target_direction);
  const size_t pool_size = static_cast<size_t>(extra_count) + band.size();
  std::vector<double> poses(9 * pool_size);
  std::vector<double> offsets(band.size());
  for (size_t i = 0; i < 9 * static_cast<size_t>(extra_count); i++) {
    poses[i] = extra_seeds[i];
  }
  for (size_t b = 0; b < band.size(); b++) {
    CandidatePose(incident_, target_direction, band[b], poses.data() + 9 * (static_cast<size_t>(extra_count) + b));
    offsets[b] = std::fabs(band[b].deviation - delta);
  }
  const TargetChart chart = MakeTargetChart(target_direction);
  auto admit = [this, &chart, &params](const double raw[9], double seed[9]) { return Admit(chart, params, raw, seed); };
  DiscoverOnPool(map_, chart, params, poses.data(), static_cast<int>(pool_size), extra_count, offsets.data(),
                 cluster_radius, distance_threshold, admit, &out);
  return out;
}

DiscoveryOutput IceDiscovery::Discover(const double target_direction[3], int sample_count, double band_half_width,
                                       const double* extra_seeds, int extra_count, double cluster_radius,
                                       double distance_threshold, const ContinuationParams& params) {
  const std::vector<BandEvent> band =
      BuildBand(sample_count, TargetDeviation(incident_, target_direction), band_half_width);
  return DiscoverOnBand(target_direction, band, extra_seeds, extra_count, cluster_radius, distance_threshold, params);
}

}  // namespace lumice::analytic
