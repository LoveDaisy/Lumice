#ifndef LUMICE_ANALYTIC_DISCOVERY_HPP_
#define LUMICE_ANALYTIC_DISCOVERY_HPP_

// Component discovery for one fixed path and one target direction: the seeds of the fiber
// X_(P,d) = { R : F_P(R) = d }, each distinct component traced once and classified (LI
// docs/phase1-math-contract.md section 9.5, strategy reference-discovery-v1; LI discovery.py
// discover_components). Every step order and gate below is normative in that section, so that two
// backends given the same pool return the same components and counters:
//
//   sample (antipodal Fibonacci lattice of u = R^-1 s_hat) -> band |D_i - delta| <= b of the events
//   with w = A T > 0 -> candidate poses R_i = W F_i^T -> pool = extra seeds + band -> greedy geodesic
//   clustering (lowest unassigned index is the centre, distance strictly below r_c) -> one
//   representative per cluster -> Gauss-Newton -> admissibility gate -> dedup against accepted
//   curves (strictly below eta) -> forward trace, backward trace on an arc event -> classification.
//
// Two layers. The core (DiscoverOnPool, ClassifyAndTrace, NewtonCorrect, GeodesicCluster) does not
// know optics: it is a template over the continuation's Map (fiber_continuation.hpp) and an admission
// functor, so the unit tests drive it with LI's analytic maps as its conformance suite does. The
// ice-crystal layer (IceDiscovery) adds the lattice, the band, the finite-crystal entry-measure gate
// and the ice path map.
//
// `completeness` is procedural, never a certificate (section 9.5.6): complete means only that every
// admissible, non-folded candidate of this pool closed or stitched. Densification is not monotone
// (section 9.5.7): a denser call can lose a component a sparser one found, which is why extra seeds
// exist.

#include <cmath>
#include <utility>
#include <vector>

#include "analytic/entry_measure.hpp"
#include "analytic/fiber_continuation.hpp"
#include "analytic/path_evaluation.hpp"
#include "analytic/path_fiber.hpp"
#include "analytic/so3.hpp"

namespace lumice::analytic {

// Reference defaults of section 9.5.9 (strategy, not constants of the mathematics).
constexpr int kDefaultDiscoverySampleCount = 1000000;
constexpr double kDefaultBandHalfWidth = 0.2 * 3.14159265358979323846 / 180.0;  // 0.2 deg, in rad
constexpr double kDefaultClusterRadius = 0.3;                                   // rad, SO(3) geodesic
// The dedup threshold defaults to the continuation's closure_distance (section 9.5.9).

// Gauss-Newton of section 9.5.4 step 3 (LI discovery._newton_correct / _admissible_seed): at most
// 30 iterations, stop once |r| <= tau / 100, accept at tau.
constexpr int kNewtonIterations = 30;
constexpr double kNewtonStopFraction = 1e-2;

enum class ComponentKind { kClosed, kArc };

// The four incomplete counters of section 9.5.6, as a candidate's cause.
enum class IncompleteCause {
  kArcBackwardFailed,
  kArcBackwardClosedAnomaly,
  kUnnamedEvent,
  kNotConverged,
};

// A component: its corrected seed and its traces. `backward` is the trace with the orientation
// reversed, run for an arc only (closed: empty, PoseCount() == 0). The stitched arc of section 9.5.5
// follows from the two and is not stored.
struct DiscoveredComponent {
  ComponentKind kind = ComponentKind::kClosed;
  double seed[9]{};
  TraceResult forward;
  TraceResult backward;
};

// A candidate traced but not classified. `backward` was run iff the cause is kArcBackwardFailed or
// kArcBackwardClosedAnomaly.
struct IncompleteCandidate {
  IncompleteCause cause = IncompleteCause::kNotConverged;
  double seed[9]{};
  TraceResult forward;
  TraceResult backward;
};

struct DiscoveryOutput {
  std::vector<DiscoveredComponent> components;  // trace order
  std::vector<IncompleteCandidate> incomplete;  // trace order
  // Funnel counts (sections 9.5.3-9.5.4).
  int pool_count = 0;  // band events, extra seeds excluded
  int extra_seed_count = 0;
  int raw_cluster_count = 0;
  int admissible_count = 0;
  // Classification counters (section 9.5.6, LI DISCOVERY_EVENT_NAMES order).
  int dedup_merged = 0;
  int arc_stitched = 0;
  int arc_backward_failed = 0;
  int arc_backward_closed_anomaly = 0;
  int incomplete_unnamed_event = 0;
  int incomplete_not_converged = 0;

  bool Complete() const { return incomplete.empty(); }
};

// The five named events an arc may end on (LI discovery.ARC_EVENTS); rank_loss and
// topology_ambiguity are open degeneracies, not arc ends.
bool IsArcEvent(FiberReason reason);

// Section 9.5.4 step 1 over `count` row-major poses: clusters in creation order, members ascending.
std::vector<std::vector<int>> GeodesicCluster(const double* poses, int count, double radius);

// Section 9.5.4 step 2: the lowest-index extra seed (index < extra_count) if the cluster holds one,
// else the band member with the smallest offset, ties to the lowest index. `band_offsets[i -
// extra_count]` is band member i's |D_i - delta|.
int Representative(const std::vector<int>& cluster, int extra_count, const double* band_offsets);

// Smallest SO(3) geodesic distance from `r` to the `trace`'s stored poses; +inf when it has none.
double DistanceToCurve(const double r[9], const TraceResult& trace);

// Section 9.5.4 step 3 (LI _newton_correct): Gauss-Newton with the minimum-norm step
// R <- R exp(-A^T (A A^T)^-1 r) from `start`, stopping at the current pose once |r| <= stop. Writes
// the final pose to `out` and returns its residual norm (NaN once an iterate leaves the map's
// domain, where this library's direction is NaN).
template <class Map>
double NewtonCorrect(const Map& map, const TargetChart& chart, double stop, const double start[9], double out[9]) {
  double r[9];
  for (int i = 0; i < 9; i++) {
    r[i] = start[i];
  }
  auto residual_at = [&](const double pose[9], double res[2]) {
    double direction[3];
    map.Direction(pose, direction);
    fiber_detail::ChartResidual(chart, direction, res);
    return std::sqrt(res[0] * res[0] + res[1] * res[1]);
  };
  double norm = 0.0;
  bool finished = false;
  for (int iteration = 0; iteration < kNewtonIterations; iteration++) {
    double res[2];
    norm = residual_at(r, res);
    if (norm <= stop) {
      finished = true;
      break;
    }
    if (!std::isfinite(norm)) {
      break;  // every later iterate is NaN too; LI would iterate on to the same inadmissible end
    }
    double a[2][3];
    fiber_detail::LocalResidualJacobian(map, chart, r, a);
    // (A A^T) y = res, delta = -A^T y.
    const double m00 = so3::Dot3(a[0], a[0]);
    const double m01 = so3::Dot3(a[0], a[1]);
    const double m11 = so3::Dot3(a[1], a[1]);
    const double det = m00 * m11 - m01 * m01;
    const double y0 = (m11 * res[0] - m01 * res[1]) / det;
    const double y1 = (m00 * res[1] - m01 * res[0]) / det;
    const double delta[3] = { -(a[0][0] * y0 + a[1][0] * y1), -(a[0][1] * y0 + a[1][1] * y1),
                              -(a[0][2] * y0 + a[1][2] * y1) };
    double next[9];
    fiber_detail::ApplyCorrection(r, delta, next);
    for (int i = 0; i < 9; i++) {
      r[i] = next[i];
    }
  }
  if (!finished) {
    double res[2];
    norm = residual_at(r, res);
  }
  for (int i = 0; i < 9; i++) {
    out[i] = r[i];
  }
  return norm;
}

// Section 9.5.5 (LI _trace_and_classify): the forward trace with `params`, then — on an arc event —
// the backward trace with initial_tangent_sign negated; appends a component or an incomplete
// candidate and counts it. `trace(params)` runs one trace of `seed`; production passes TraceFiber
// (the overload below), and a test can substitute canned traces for the branches no real fiber
// reaches, as LI's suite does by patching trace_fiber.
template <class Trace>
void ClassifyAndTrace(const ContinuationParams& params, const double seed[9], const Trace& trace,
                      DiscoveryOutput* out) {
  TraceResult forward = trace(params);
  auto incomplete = [&](IncompleteCause cause, TraceResult backward) {
    IncompleteCandidate c;
    c.cause = cause;
    for (int i = 0; i < 9; i++) {
      c.seed[i] = seed[i];
    }
    c.forward = std::move(forward);
    c.backward = std::move(backward);
    out->incomplete.push_back(std::move(c));
  };
  if (forward.status == FiberStatus::kClosed) {
    DiscoveredComponent c;
    c.kind = ComponentKind::kClosed;
    for (int i = 0; i < 9; i++) {
      c.seed[i] = seed[i];
    }
    c.forward = std::move(forward);
    out->components.push_back(std::move(c));
    return;
  }
  if (forward.status != FiberStatus::kEventTerminated) {
    out->incomplete_not_converged++;
    incomplete(IncompleteCause::kNotConverged, TraceResult{});
    return;
  }
  if (!IsArcEvent(forward.reason)) {
    out->incomplete_unnamed_event++;
    incomplete(IncompleteCause::kUnnamedEvent, TraceResult{});
    return;
  }
  ContinuationParams reversed = params;
  reversed.initial_tangent_sign = -params.initial_tangent_sign;
  TraceResult backward = trace(reversed);
  if (backward.status == FiberStatus::kClosed) {
    // The forward trace of the same seed on the same one-dimensional fiber should have closed first.
    out->arc_backward_closed_anomaly++;
    incomplete(IncompleteCause::kArcBackwardClosedAnomaly, std::move(backward));
    return;
  }
  if (backward.status != FiberStatus::kEventTerminated || !IsArcEvent(backward.reason)) {
    out->arc_backward_failed++;
    incomplete(IncompleteCause::kArcBackwardFailed, std::move(backward));
    return;
  }
  out->arc_stitched++;
  DiscoveredComponent c;
  c.kind = ComponentKind::kArc;
  for (int i = 0; i < 9; i++) {
    c.seed[i] = seed[i];
  }
  c.forward = std::move(forward);
  c.backward = std::move(backward);
  out->components.push_back(std::move(c));
}

template <class Map>
void ClassifyAndTrace(const Map& map, const TargetChart& chart, const ContinuationParams& params, const double seed[9],
                      DiscoveryOutput* out) {
  ClassifyAndTrace(params, seed, [&](const ContinuationParams& p) { return TraceFiber(map, chart, seed, p); }, out);
}

// Sections 9.5.4-9.5.5 over a built pool: `pool_poses` holds `pool_size` row-major poses, the
// `extra_count` extra seeds first, then the band with its offsets `band_offsets`. `admit(raw, seed)`
// is the Gauss-Newton + admissibility step: true with the corrected pose in `seed` when admissible.
// Sets every count of `out` except pool_count and extra_seed_count, which the caller knows.
template <class Map, class Admit>
void DiscoverOnPool(const Map& map, const TargetChart& chart, const ContinuationParams& params,
                    const double* pool_poses, int pool_size, int extra_count, const double* band_offsets,
                    double cluster_radius, double distance_threshold, const Admit& admit, DiscoveryOutput* out) {
  const std::vector<std::vector<int>> clusters = GeodesicCluster(pool_poses, pool_size, cluster_radius);
  out->raw_cluster_count = static_cast<int>(clusters.size());
  for (const std::vector<int>& cluster : clusters) {
    const int representative = Representative(cluster, extra_count, band_offsets);
    double seed[9];
    if (!admit(pool_poses + 9 * static_cast<size_t>(representative), seed)) {
      continue;  // dropped silently; the cluster's other members are not tried in its place
    }
    out->admissible_count++;
    bool folded = false;
    for (const DiscoveredComponent& c : out->components) {
      if (DistanceToCurve(seed, c.forward) < distance_threshold ||
          DistanceToCurve(seed, c.backward) < distance_threshold) {
        folded = true;
        break;
      }
    }
    if (folded) {
      out->dedup_merged++;
      continue;
    }
    ClassifyAndTrace(map, chart, params, seed, out);
  }
}

// ---------------------------------------------------------------------------------------------
// The ice-crystal layer.
// ---------------------------------------------------------------------------------------------

// Point i of the n-point antipodal Fibonacci lattice (section 9.5.2, LI s2_store.store_lattice):
// u_i = -(sqrt(1 - z^2) cos theta, sqrt(1 - z^2) sin theta, z), z = 1 - (2 i + 1) / n,
// theta = pi (1 + sqrt 5)(i + 1/2), written in LI's evaluation order.
void LatticePoint(int n, int i, double u[3]);

// One kept band event: lattice index, deviation D = angle(phi, -u), u and the body-frame outgoing
// direction phi = Phi_P(-u).
struct BandEvent {
  int index = 0;
  double deviation = 0.0;
  double u[3]{};
  double phi[3]{};
};

// Deviation delta = angle(s, d), as LI's StoreSeeds.candidates computes it.
double TargetDeviation(const double incident_direction[3], const double target_direction[3]);

// Section 9.5.3: R = W F^T, W = [s_hat, e, s_hat x e], e = unit(d - (d . s_hat) s_hat),
// F = [u, f, u x f], f = unit(phi + cos(D) u); s_hat = -incident. Row-major.
void CandidatePose(const double incident_direction[3], const double target_direction[3], const BandEvent& event,
                   double r[9]);

// One crystal and one concrete path, lit from one direction: the fixed context of a discovery call.
// Holds the path map (path_fiber.hpp) and the corridor (entry_measure.hpp), so it is neither
// copyable nor movable. Not thread-safe (the corridor keeps clipping scratch); one per call.
class IceDiscovery {
 public:
  // `slots` resolved by ResolveFaceSequence (2..kMaxFaceCount), `incident_direction` a world unit
  // vector, `refractive_index` finite and positive.
  IceDiscovery(const FaceNormalTable& table, const FacePolygonTable& polygons, const int* slots, int slot_count,
               double refractive_index, const double incident_direction[3]);
  IceDiscovery(const IceDiscovery&) = delete;
  IceDiscovery& operator=(const IceDiscovery&) = delete;

  // Section 9.5.2-9.5.3: the kept events (w = A T > 0) of the n-point lattice whose deviation lies in
  // [delta - half_width, delta + half_width], in increasing (D, index). One streaming pass: only band
  // events are kept, and the entry measure is evaluated only for them (A does not change D, so the
  // band of the kept events is the kept events of the band, in the same order).
  std::vector<BandEvent> BuildBand(int sample_count, double delta, double half_width);

  // Section 9.5.4 step 3-4: Gauss-Newton from `raw`, then admissible iff the residual is at most
  // tau = residual_tolerance + relative_residual_tolerance, the path domain is valid (every
  // validity margin > 0) and the entry measure is positive.
  bool Admit(const TargetChart& chart, const ContinuationParams& params, const double raw[9], double seed[9]);

  // The whole pipeline on a given band (a parity fixture's, or BuildBand's). `extra_seeds` holds
  // `extra_count` row-major rotations.
  DiscoveryOutput DiscoverOnBand(const double target_direction[3], const std::vector<BandEvent>& band,
                                 const double* extra_seeds, int extra_count, double cluster_radius,
                                 double distance_threshold, const ContinuationParams& params);

  // BuildBand for the target's deviation, then DiscoverOnBand: one discovery call.
  DiscoveryOutput Discover(const double target_direction[3], int sample_count, double band_half_width,
                           const double* extra_seeds, int extra_count, double cluster_radius, double distance_threshold,
                           const ContinuationParams& params);

  const IcePathMap& Map() const { return map_; }

 private:
  const FaceNormalTable* table_;
  int slots_[kMaxFaceCount];
  int slot_count_;
  double refractive_index_;
  double incident_[3];
  IcePathMap map_;
  Corridor corridor_;
  std::vector<double> segments_;
  std::vector<double> transmittances_;
};

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_DISCOVERY_HPP_
