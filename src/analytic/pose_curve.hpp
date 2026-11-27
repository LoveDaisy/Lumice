#ifndef LUMICE_ANALYTIC_POSE_CURVE_HPP_
#define LUMICE_ANALYTIC_POSE_CURVE_HPP_

// Unified pose-manifold specification and 1D curve data model for the analytic kernel:
//   - PoseManifold parameterizes 0D/1D/2D/3D submanifolds of SO(3) in the production Euler chart
//     R(az, zenith, roll) = Rz(az - pi) Ry(-zenith) Rz(roll) (body -> world, row-major, radians
//     internally).
//   - PoseCurve is the common 1D output shared across 1D families (kFamily1D), 2D caustic loci
//     (kCaustic2D), rank-deficient 2D families (kCollapsed2D), and 3D sky fibers (kSkyFiber).
//   - PoseCurveFromFiber adapts fiber_continuation::TraceResult into PoseCurveKind::kSkyFiber.
//
// Internal analytic header: not part of the C ABI.

#include <algorithm>
#include <cmath>
#include <vector>

#include "analytic/family_rank.hpp"
#include "analytic/fiber_continuation.hpp"
#include "analytic/jet.hpp"
#include "analytic/path_evaluation.hpp"
#include "analytic/so3.hpp"
#include "core/math.hpp"

namespace lumice::analytic {

// Production Euler chart R(az, zenith, roll) = Rz(az - pi) Ry(-zenith) Rz(roll) and sky read
// ImageAltAzDeg are owned by family_rank.hpp (the single authority: EulerPose / EulerPoseDeg /
// ImageAltAzDeg). This header provides only the genuinely new inverses and thin sky-direction
// wrappers over those authorities.

// Inverse decomposition of a body -> world rotation onto (azimuth, zenith, roll) in radians:
// azimuth in [0, 2*pi), zenith in [0, pi], roll in [0, 2*pi). At the poles (zenith near 0 or pi),
// roll is set to 0 and azimuth absorbs the combined z-spin.
inline void PoseToEulerRad(const double pose[9], double param[3]) {
  constexpr double kTwoPi = 2.0 * math::kPiD;
  auto wrap_two_pi = [](double v) {
    double r = std::fmod(v, kTwoPi);
    if (r < 0.0) {
      r += kTwoPi;
    }
    return r;
  };
  const double sin_z = std::hypot(pose[6], pose[7]);
  const double zenith = std::atan2(sin_z, pose[8]);
  param[1] = zenith;
  if (sin_z > 1e-12) {
    param[0] = wrap_two_pi(std::atan2(pose[5], pose[2]));
    param[2] = wrap_two_pi(std::atan2(-pose[7], pose[6]));
  } else if (pose[8] >= 0.0) {
    // zenith ~ 0: pose[0] = -cos(az + roll), pose[3] = -sin(az + roll).
    param[0] = wrap_two_pi(std::atan2(-pose[3], -pose[0]));
    param[2] = 0.0;
  } else {
    // zenith ~ pi: pose[0] = cos(az - roll), pose[3] = sin(az - roll).
    param[0] = wrap_two_pi(std::atan2(pose[3], pose[0]));
    param[2] = 0.0;
  }
}

// Converts a unit sky direction (sky = -outgoing) to altitude and azimuth in degrees, forwarding
// to family_rank.hpp's ImageAltAzDeg authority.
inline void SkyToAltAzDeg(const double sky[3], double* alt_deg, double* az_deg) {
  const double outgoing[3] = { -sky[0], -sky[1], -sky[2] };
  ImageAltAzDeg(outgoing, alt_deg, az_deg);
}

// Converts (altitude_deg, azimuth_deg) to a unit sky direction.
inline void AltAzDegToSky(double alt_deg, double az_deg, double sky[3]) {
  const double alt = alt_deg * math::kDegreeToRadD;
  const double az = az_deg * math::kDegreeToRadD;
  sky[0] = std::cos(alt) * std::cos(az);
  sky[1] = std::cos(alt) * std::sin(az);
  sky[2] = std::sin(alt);
}

// One axis of a PoseManifold, stored in radians internally.
struct PoseAxis {
  bool free = false;
  double value = 0.0;     // radians (used when !free)
  double min_val = 0.0;   // radians (used when free)
  double max_val = 0.0;   // radians (used when free)
  bool periodic = false;  // wrap [min_val, max_val) when free

  static PoseAxis PinnedRad(double rad) {
    PoseAxis ax;
    ax.free = false;
    ax.value = rad;
    ax.min_val = rad;
    ax.max_val = rad;
    ax.periodic = false;
    return ax;
  }

  static PoseAxis PinnedDeg(double deg) { return PinnedRad(deg * math::kDegreeToRadD); }

  static PoseAxis FreeRad(double min_rad, double max_rad, bool is_periodic = false) {
    PoseAxis ax;
    ax.free = true;
    ax.value = 0.5 * (min_rad + max_rad);
    ax.min_val = min_rad;
    ax.max_val = max_rad;
    ax.periodic = is_periodic;
    return ax;
  }

  static PoseAxis FreeDeg(double min_deg, double max_deg, bool is_periodic = false) {
    return FreeRad(min_deg * math::kDegreeToRadD, max_deg * math::kDegreeToRadD, is_periodic);
  }

  double Span() const { return max_val - min_val; }
};

// Submanifold of SO(3) in the production Euler chart (azimuth, zenith, roll), supporting 0D..3D.
struct PoseManifold {
  static constexpr int kAxisAzimuth = 0;
  static constexpr int kAxisZenith = 1;
  static constexpr int kAxisRoll = 2;

  PoseAxis axes[3] = {
    PoseAxis::FreeRad(0.0, 2.0 * math::kPiD, true),
    PoseAxis::FreeRad(0.0, math::kPiD, false),
    PoseAxis::FreeRad(0.0, 2.0 * math::kPiD, true),
  };

  int Dimension() const {
    int dim = 0;
    for (const PoseAxis& ax : axes) {
      if (ax.free) {
        dim++;
      }
    }
    return dim;
  }

  int FreeAxes(int out_axes[3]) const {
    int count = 0;
    for (int i = 0; i < 3; i++) {
      if (axes[i].free) {
        out_axes[count++] = i;
      }
    }
    return count;
  }

  bool IsValid() const {
    for (int i = 0; i < 3; i++) {
      const PoseAxis& ax = axes[i];
      if (!std::isfinite(ax.value) || !std::isfinite(ax.min_val) || !std::isfinite(ax.max_val)) {
        return false;
      }
      if (ax.free && !(ax.max_val > ax.min_val)) {
        return false;
      }
    }
    const PoseAxis& z = axes[kAxisZenith];
    if (z.free) {
      if (z.min_val < -1e-12 || z.max_val > math::kPiD + 1e-12 || z.periodic) {
        return false;
      }
    } else {
      if (z.value < -1e-12 || z.value > math::kPiD + 1e-12) {
        return false;
      }
    }
    return true;
  }

  // Standard ice-crystal orientation families:
  // Plate: horizontal basal faces (zenith = 0, roll = 0), free azimuth (1D).
  static PoseManifold Plate() {
    PoseManifold m;
    m.axes[kAxisAzimuth] = PoseAxis::FreeRad(0.0, 2.0 * math::kPiD, true);
    m.axes[kAxisZenith] = PoseAxis::PinnedRad(0.0);
    m.axes[kAxisRoll] = PoseAxis::PinnedRad(0.0);
    return m;
  }

  // Column: horizontal c-axis (zenith = 90 deg), free azimuth and roll (2D).
  static PoseManifold Column() {
    PoseManifold m;
    m.axes[kAxisAzimuth] = PoseAxis::FreeRad(0.0, 2.0 * math::kPiD, true);
    m.axes[kAxisZenith] = PoseAxis::PinnedDeg(90.0);
    m.axes[kAxisRoll] = PoseAxis::FreeRad(0.0, 2.0 * math::kPiD, true);
    return m;
  }

  // Parry: horizontal c-axis (zenith = 90 deg) with fixed roll (default 0 deg), free azimuth (1D).
  static PoseManifold Parry(double roll_deg = 0.0) {
    PoseManifold m;
    m.axes[kAxisAzimuth] = PoseAxis::FreeRad(0.0, 2.0 * math::kPiD, true);
    m.axes[kAxisZenith] = PoseAxis::PinnedDeg(90.0);
    m.axes[kAxisRoll] = PoseAxis::PinnedDeg(roll_deg);
    return m;
  }

  // Lowitz: fixed roll (default 0 deg), free azimuth [0, 2pi) and zenith [0, pi] (2D).
  static PoseManifold Lowitz(double roll_deg = 0.0) {
    PoseManifold m;
    m.axes[kAxisAzimuth] = PoseAxis::FreeRad(0.0, 2.0 * math::kPiD, true);
    m.axes[kAxisZenith] = PoseAxis::FreeRad(0.0, math::kPiD, false);
    m.axes[kAxisRoll] = PoseAxis::PinnedDeg(roll_deg);
    return m;
  }

  // TiltedSpin: fixed zenith, free azimuth and roll (2D).
  static PoseManifold TiltedSpin(double zenith_deg) {
    PoseManifold m;
    m.axes[kAxisAzimuth] = PoseAxis::FreeRad(0.0, 2.0 * math::kPiD, true);
    m.axes[kAxisZenith] = PoseAxis::PinnedDeg(zenith_deg);
    m.axes[kAxisRoll] = PoseAxis::FreeRad(0.0, 2.0 * math::kPiD, true);
    return m;
  }

  // Maps a RestrictedFamilySpec (family_rank.hpp) onto a full-range PoseManifold: kFixed ->
  // PinnedDeg(center_deg), kFree -> full periodic [0, 2*pi) for azimuth/roll or [0, pi] for zenith.
  static PoseManifold FromRestrictedFamilySpec(const RestrictedFamilySpec& spec) {
    PoseManifold m;
    m.axes[kAxisAzimuth] = (spec.azimuth.mode == FamilyDimensionMode::kFixed) ?
                               PoseAxis::PinnedDeg(spec.azimuth.center_deg) :
                               PoseAxis::FreeRad(0.0, 2.0 * math::kPiD, true);
    m.axes[kAxisZenith] = (spec.zenith.mode == FamilyDimensionMode::kFixed) ?
                              PoseAxis::PinnedDeg(spec.zenith.center_deg) :
                              PoseAxis::FreeRad(0.0, math::kPiD, false);
    m.axes[kAxisRoll] = (spec.roll.mode == FamilyDimensionMode::kFixed) ?
                            PoseAxis::PinnedDeg(spec.roll.center_deg) :
                            PoseAxis::FreeRad(0.0, 2.0 * math::kPiD, true);
    return m;
  }
};

enum class PoseCurveKind {
  kFamily1D,
  kCaustic2D,
  kCollapsed2D,
  kSkyFiber,
};

enum class CurveSegmentKind {
  kArc,
  kPointImage,
};

enum class CurveEventKind {
  kOpticalBoundary,
  kDomainBoundary,
  kCorridorCutoff,
  kTirOnset,
  kCusp,
  kStationaryPoint,
  kLocusSingularity,
};

struct PoseCurveNode {
  double param[3]{};    // Euler coordinates (azimuth, zenith, roll) in radians
  double pose[9]{};     // row-major body -> world rotation
  double u[3]{};        // body-frame unit direction toward sun: R^T sun_dir
  double sky[3]{};      // world-frame unit halo image direction: -outgoing
  double tangent[3]{};  // unit tangent in parameter space (or SO(3) tangent for kSkyFiber)
  double sky_speed = 0.0;
  double sigma1 = 0.0;
  double sigma2 = 0.0;
  double arclength = 0.0;
};

struct CurveSegment {
  int start_idx = 0;  // inclusive node index
  int end_idx = 0;    // inclusive node index
  CurveSegmentKind kind = CurveSegmentKind::kArc;
  bool corridor_open = true;
};

struct CurveEvent {
  CurveEventKind kind = CurveEventKind::kOpticalBoundary;
  double arclength = 0.0;
  double param[3]{};
  double pose[9]{};
  double sky[3]{};
  // Failing optical margin index (kOpticalBoundary), internal reflection step index (kTirOnset),
  // Euler axis index (kDomainBoundary), or -1 if N/A.
  int detail_index = -1;
};

struct CurveCoverage {
  int seed_grid_n = 0;
  bool closed = false;
  bool budget_truncated = false;
  int unresolved_singularities = 0;
  int failed_seeds = 0;
};

struct PoseCurve {
  PoseCurveKind kind = PoseCurveKind::kFamily1D;
  std::vector<PoseCurveNode> nodes;
  std::vector<CurveSegment> segments;
  std::vector<CurveEvent> events;
  CurveCoverage coverage;
};

// Adapts an existing fiber_continuation TraceResult into PoseCurveKind::kSkyFiber.
// `sun_dir` is the unit vector toward the sun; `target_sky` is the unit sky direction of the fiber
// (i.e. -target_outgoing).
inline PoseCurve PoseCurveFromFiber(const TraceResult& fiber, const double sun_dir[3], const double target_sky[3]) {
  PoseCurve curve;
  curve.kind = PoseCurveKind::kSkyFiber;
  curve.coverage.closed = (fiber.status == FiberStatus::kClosed);
  curve.coverage.budget_truncated =
      (fiber.reason == FiberReason::kStepBudget || fiber.reason == FiberReason::kEvaluationBudget ||
       fiber.reason == FiberReason::kArclengthBudget);

  double sky_unit[3] = { target_sky[0], target_sky[1], target_sky[2] };
  const double sky_norm = so3::Norm3(sky_unit);
  if (sky_norm > 0.0) {
    sky_unit[0] /= sky_norm;
    sky_unit[1] /= sky_norm;
    sky_unit[2] /= sky_norm;
  }

  const int n = fiber.PoseCount();
  curve.nodes.reserve(static_cast<size_t>(n));
  double s = 0.0;
  for (int i = 0; i < n; i++) {
    if (i > 0 && static_cast<size_t>(i - 1) < fiber.arclength_increments.size()) {
      s += fiber.arclength_increments[static_cast<size_t>(i - 1)];
    }
    PoseCurveNode node;
    const size_t idx = static_cast<size_t>(i);
    for (int k = 0; k < 9; k++) {
      node.pose[k] = fiber.poses[9 * idx + static_cast<size_t>(k)];
    }
    PoseToEulerRad(node.pose, node.param);
    WorldToBody(node.pose, sun_dir, node.u);
    so3::Copy3(sky_unit, node.sky);
    for (int k = 0; k < 3; k++) {
      node.tangent[k] = fiber.tangents[3 * idx + static_cast<size_t>(k)];
    }
    node.sky_speed = 0.0;
    if (idx < fiber.jacobian_available.size() && fiber.jacobian_available[idx] != 0 &&
        2 * idx + 1 < fiber.singular_values.size()) {
      node.sigma1 = fiber.singular_values[2 * idx + 0];
      node.sigma2 = fiber.singular_values[2 * idx + 1];
    }
    node.arclength = s;
    curve.nodes.push_back(node);
  }

  if (n > 0) {
    CurveSegment seg;
    seg.start_idx = 0;
    seg.end_idx = n - 1;
    seg.kind = CurveSegmentKind::kPointImage;
    seg.corridor_open = true;
    curve.segments.push_back(seg);

    if (fiber.status == FiberStatus::kEventTerminated) {
      CurveEvent ev;
      const PoseCurveNode& last = curve.nodes.back();
      ev.arclength = last.arclength;
      for (int k = 0; k < 3; k++) {
        ev.param[k] = last.param[k];
        ev.sky[k] = last.sky[k];
      }
      for (int k = 0; k < 9; k++) {
        ev.pose[k] = last.pose[k];
      }
      ev.detail_index = -1;
      if (fiber.reason == FiberReason::kRankLoss || fiber.reason == FiberReason::kTopologyAmbiguity) {
        ev.kind = CurveEventKind::kLocusSingularity;
      } else {
        ev.kind = CurveEventKind::kOpticalBoundary;
      }
      curve.events.push_back(ev);
    }
  }

  return curve;
}

}  // namespace lumice::analytic

#endif  // LUMICE_ANALYTIC_POSE_CURVE_HPP_
