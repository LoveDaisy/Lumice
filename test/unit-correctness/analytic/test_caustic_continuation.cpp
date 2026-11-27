// Unit tests for PoseManifold / PoseCurve (src/analytic/pose_curve.hpp) and 1D/2D continuous
// caustic continuation (src/analytic/caustic_continuation.{hpp,cpp}).

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "analytic/caustic_continuation.hpp"
#include "analytic/entry_measure.hpp"
#include "analytic/family_rank.hpp"
#include "analytic/fiber_continuation.hpp"
#include "analytic/jet.hpp"
#include "analytic/path_chain.hpp"
#include "analytic/path_evaluation.hpp"
#include "analytic/path_fiber.hpp"
#include "analytic/pose_curve.hpp"
#include "analytic/so3.hpp"
#include "core/math.hpp"
#include "core/optics.hpp"

namespace lumice::analytic {
namespace {

constexpr double kPi = math::kPiD;
constexpr double kDegToRad = math::kDegreeToRadD;
constexpr double kRadToDeg = 1.0 / math::kDegreeToRadD;

LUMICE_ANALYTIC_Crystal RegularPrism(double height = 1.0) {
  LUMICE_ANALYTIC_Crystal c{};
  c.kind = LUMICE_ANALYTIC_CRYSTAL_PRISM;
  c.height = height;
  for (int i = 0; i < 6; i++) {
    c.face_distance[i] = 1.0;
  }
  return c;
}

struct PrismFixture {
  FaceNormalTable normals;
  FacePolygonTable polygons;

  explicit PrismFixture(double height = 1.0) {
    EXPECT_EQ(BuildFaceNormals(RegularPrism(height), &normals, &polygons), Status::kOk);
  }

  void Resolve(const int* faces, int count, int* slots) const {
    ASSERT_EQ(ResolveFaceSequence(normals, faces, count, slots), Status::kOk);
  }
};

// Independently re-evaluates J(q) = x . (x_1 x x_2) at a 2D manifold node using Jet<2>.
// Optionally also returns the desingularized residual J_reg = entry_disc * exit_disc * J
// (whose gradient stays O(1) even as exit_disc -> 1e-8) and min(entry_disc, exit_disc).
bool IndependentSignedJacobian2D(const FaceNormalTable& table, const int* slots, int slot_count, double n,
                                 const double sun_dir[3], const PoseManifold& manifold, const double param[3],
                                 double* j_out, double* sigma1_out = nullptr, double* sigma2_out = nullptr,
                                 double* internal_disc_out = nullptr, double* j_reg_out = nullptr,
                                 double* min_snell_disc_out = nullptr) {
  int free_ax[3]{};
  if (manifold.FreeAxes(free_ax) != 2) {
    return false;
  }
  Jet<2> ang[3];
  for (int k = 0; k < 3; k++) {
    if (k == free_ax[0]) {
      ang[k] = Jet<2>::Variable(param[k], 0);
    } else if (k == free_ax[1]) {
      ang[k] = Jet<2>::Variable(param[k], 1);
    } else {
      ang[k] = Jet<2>(param[k]);
    }
  }
  Jet<2> pose[9];
  EulerPose(ang[0], ang[1], ang[2], pose);  // family_rank.hpp's independent EulerPose via so3::Exp
  const Jet<2> incident[3] = { Jet<2>(-sun_dir[0]), Jet<2>(-sun_dir[1]), Jet<2>(-sun_dir[2]) };
  Jet<2> outgoing[3];
  ChainInterfaceDiagnostics<Jet<2>> diag;
  if (!TracePathChain<Jet<2>, ChainEvaluation::kFirstFailure>(table, slots, slot_count, Jet<2>(n), incident, pose,
                                                              outgoing, nullptr, nullptr, &diag)) {
    return false;
  }
  double sky[3];
  double x1[3];
  double x2[3];
  for (int i = 0; i < 3; i++) {
    sky[i] = -outgoing[i].a;
    x1[i] = -outgoing[i].v[0];
    x2[i] = -outgoing[i].v[1];
  }
  double cross[3];
  so3::Cross3(x1, x2, cross);
  *j_out = so3::Dot3(sky, cross);
  if (j_reg_out != nullptr) {
    const double w = (slot_count >= 2) ? (diag.discriminant[0].a * diag.discriminant[slot_count - 1].a) : 1.0;
    *j_reg_out = w * (*j_out);
  }
  if (min_snell_disc_out != nullptr) {
    *min_snell_disc_out =
        (slot_count >= 2) ? std::min(diag.discriminant[0].a, diag.discriminant[slot_count - 1].a) : 1.0;
  }
  if (sigma1_out != nullptr || sigma2_out != nullptr) {
    const so3::TwoByThreeSvd svd = so3::SvdTwoByThree(x1, x2);
    if (sigma1_out != nullptr) {
      *sigma1_out = svd.sigma1;
    }
    if (sigma2_out != nullptr) {
      *sigma2_out = svd.sigma2;
    }
  }
  if (internal_disc_out != nullptr) {
    for (int k = 1; k < slot_count - 1; k++) {
      internal_disc_out[k] = diag.discriminant[k].a;
    }
  }
  return true;
}

double WrapAngleDeg(double deg) {
  double r = std::fmod(deg, 360.0);
  if (r < 0.0) {
    r += 360.0;
  }
  return r;
}

double AngleDiffDeg(double a_deg, double b_deg) {
  double d = std::fmod(a_deg - b_deg + 180.0, 360.0);
  if (d < 0.0) {
    d += 360.0;
  }
  return d - 180.0;
}

// =================================================================================================
// 1. PoseManifold, Euler chart parity, and PoseCurveFromFiber adapter
// =================================================================================================

TEST(CausticContinuation, EulerChartRoundTripAndPoseManifoldConstructors) {
  // Verify EulerPose (family_rank.hpp's single authority) round-trips through PoseToEulerRad
  // across generic poses and Euler poles.
  const double test_angles[][3] = {
    { 0.0, 0.0, 0.0 }, { 37.5, 59.75, 142.25 }, { 237.7, 90.0, 0.0 }, { 315.0, 135.0, 270.0 }, { 120.0, 180.0, 0.0 },
  };
  for (const auto& a : test_angles) {
    const double az = a[0] * kDegToRad;
    const double z = a[1] * kDegToRad;
    const double r = a[2] * kDegToRad;
    double pose[9];
    EulerPose(az, z, r, pose);
    EXPECT_TRUE(ValidateRotation(pose));

    double recovered[3];
    PoseToEulerRad(pose, recovered);
    double pose_rt[9];
    EulerPose(recovered[0], recovered[1], recovered[2], pose_rt);
    EXPECT_LT(so3::Distance(pose, pose_rt), 1e-13);
  }

  // Standard family convenience constructors.
  const PoseManifold plate = PoseManifold::Plate();
  EXPECT_TRUE(plate.IsValid());
  EXPECT_EQ(plate.Dimension(), 1);
  EXPECT_TRUE(plate.axes[PoseManifold::kAxisAzimuth].free);
  EXPECT_FALSE(plate.axes[PoseManifold::kAxisZenith].free);
  EXPECT_NEAR(plate.axes[PoseManifold::kAxisZenith].value, 0.0, 1e-15);

  const PoseManifold column = PoseManifold::Column();
  EXPECT_TRUE(column.IsValid());
  EXPECT_EQ(column.Dimension(), 2);
  EXPECT_NEAR(column.axes[PoseManifold::kAxisZenith].value, 0.5 * kPi, 1e-15);

  const PoseManifold parry = PoseManifold::Parry(15.0);
  EXPECT_TRUE(parry.IsValid());
  EXPECT_EQ(parry.Dimension(), 1);
  EXPECT_NEAR(parry.axes[PoseManifold::kAxisRoll].value, 15.0 * kDegToRad, 1e-15);

  const PoseManifold lowitz = PoseManifold::Lowitz(0.0);
  EXPECT_TRUE(lowitz.IsValid());
  EXPECT_EQ(lowitz.Dimension(), 2);
  EXPECT_FALSE(lowitz.axes[PoseManifold::kAxisZenith].periodic);

  const PoseManifold spin = PoseManifold::TiltedSpin(35.0);
  EXPECT_TRUE(spin.IsValid());
  EXPECT_EQ(spin.Dimension(), 2);
  EXPECT_NEAR(spin.axes[PoseManifold::kAxisZenith].value, 35.0 * kDegToRad, 1e-15);

  // RestrictedFamilySpec -> PoseManifold adapter preserves fixed centers and free ranges.
  RestrictedFamilySpec rspec;
  rspec.zenith = { FamilyDimensionMode::kFixed, 90.0 };
  rspec.azimuth = { FamilyDimensionMode::kFree, 0.0 };
  rspec.roll = { FamilyDimensionMode::kFixed, 12.5 };
  const PoseManifold mapped = PoseManifold::FromRestrictedFamilySpec(rspec);
  EXPECT_TRUE(mapped.IsValid());
  EXPECT_EQ(mapped.Dimension(), 1);
  EXPECT_NEAR(mapped.axes[PoseManifold::kAxisZenith].value, 90.0 * kDegToRad, 1e-15);
  EXPECT_NEAR(mapped.axes[PoseManifold::kAxisRoll].value, 12.5 * kDegToRad, 1e-15);
}

TEST(CausticContinuation, PoseCurveFromFiberAdaptsTraceResult) {
  const PrismFixture f;
  int slots[2];
  const int faces[2] = { 3, 5 };
  f.Resolve(faces, 2, slots);
  const double n = IceRefractiveIndex::Get(550.0);

  double sun_dir[3];
  AltAzDegToSky(20.0, 0.0, sun_dir);
  const double incident[3] = { -sun_dir[0], -sun_dir[1], -sun_dir[2] };

  // Construct a valid seed pose on {3, 5} and trace a real sky fiber from it.
  double seed_pose[9];
  EulerPoseDeg(237.7, 60.0, 0.0, seed_pose);
  double seg_dirs[9]{};
  double trans[2]{};
  PathOutputs po{};
  po.segment_directions = seg_dirs;
  po.interface_transmittances = trans;
  ASSERT_TRUE(EvaluatePath(f.normals, slots, 2, n, incident, seed_pose, &po));
  const double target_out[3] = { po.outgoing_direction[0], po.outgoing_direction[1], po.outgoing_direction[2] };
  const double target_sky[3] = { -target_out[0], -target_out[1], -target_out[2] };

  const IcePathMap map(f.normals, slots, 2, n, incident);
  const TargetChart chart = MakeTargetChart(target_out);
  ContinuationParams cp;
  cp.maximum_accepted_steps = 200;
  const TraceResult fiber = TraceFiber(map, chart, seed_pose, cp);
  ASSERT_GT(fiber.PoseCount(), 5);

  const PoseCurve curve = PoseCurveFromFiber(fiber, sun_dir, target_sky);
  EXPECT_EQ(curve.kind, PoseCurveKind::kSkyFiber);
  ASSERT_EQ(static_cast<int>(curve.nodes.size()), fiber.PoseCount());
  ASSERT_EQ(curve.segments.size(), 1u);
  EXPECT_EQ(curve.segments[0].kind, CurveSegmentKind::kPointImage);
  EXPECT_EQ(curve.coverage.closed, fiber.status == FiberStatus::kClosed);

  for (size_t i = 0; i < curve.nodes.size(); i++) {
    const PoseCurveNode& node = curve.nodes[i];
    EXPECT_DOUBLE_EQ(node.sky_speed, 0.0);
    EXPECT_LT(so3::AngleBetween(node.sky, target_sky), 1e-12);
    double u_check[3];
    WorldToBody(node.pose, sun_dir, u_check);
    EXPECT_LT(so3::AngleBetween(node.u, u_check), 1e-12);
    if (i > 0) {
      EXPECT_GT(node.arclength, curve.nodes[i - 1].arclength);
    }
  }

  // Chart-boundary termination maps explicitly to kDomainBoundary with detail_index = -1.
  TargetChart tight_chart = MakeTargetChart(target_out);
  tight_chart.minimum_dot = 1.0 - 1e-8;
  const TraceResult chart_exit = TraceFiber(map, tight_chart, seed_pose, cp);
  ASSERT_EQ(chart_exit.status, FiberStatus::kEventTerminated);
  ASSERT_EQ(chart_exit.reason, FiberReason::kChartBoundary);
  const PoseCurve chart_curve = PoseCurveFromFiber(chart_exit, sun_dir, target_sky);
  ASSERT_EQ(chart_curve.events.size(), 1u);
  EXPECT_EQ(chart_curve.events[0].kind, CurveEventKind::kDomainBoundary);
  EXPECT_EQ(chart_curve.events[0].detail_index, -1);
}

// =================================================================================================
// 2. C16 Prism 3-5 Column: Upper & Lower Tangent Arcs, Apex, Cliff Tail, Mirror Point-Image
// =================================================================================================

TEST(CausticContinuation, C16ColumnTangentArcsAndMirrorPointImage) {
  const PrismFixture f;
  int slots[2];
  const int faces[2] = { 3, 5 };
  f.Resolve(faces, 2, slots);
  const double n = IceRefractiveIndex::Get(550.0);
  double sun_dir[3];
  AltAzDegToSky(20.0, 0.0, sun_dir);

  const PoseManifold column = PoseManifold::Column();
  CausticContinuationParams params;
  const CausticTraceResult res = TraceCausticLocus(f.normals, slots, 2, n, sun_dir, column, nullptr, params);

  EXPECT_FALSE(res.stop_requested);
  EXPECT_EQ(res.generic_rank, 2);
  ASSERT_GE(res.curves.size(), 2u);

  // Exact closed-form upper tangent arc apex: sun_alt + D_min(60 deg prism, n(550)).
  const double d_min_deg = (2.0 * std::asin(n * 0.5) - kPi / 3.0) * kRadToDeg;
  const double expected_apex_deg = 20.0 + d_min_deg;  // ~41.9159 deg

  double best_upper_apex_deg = -90.0;
  double upper_max_abs_az_deg = 0.0;
  double upper_min_alt_deg = 90.0;
  bool found_upper_arc = false;
  bool found_lower_arc = false;
  bool found_mirror_point_image = false;
  int total_checked_nodes = 0;

  for (const PoseCurve& curve : res.curves) {
    EXPECT_EQ(curve.kind, PoseCurveKind::kCaustic2D);
    if (curve.nodes.empty() || curve.segments.empty()) {
      ADD_FAILURE() << "Empty curve nodes or segments";
      continue;
    }

    // Segments must contiguously partition [0, nodes.size() - 1].
    EXPECT_EQ(curve.segments.front().start_idx, 0);
    EXPECT_EQ(curve.segments.back().end_idx, static_cast<int>(curve.nodes.size()) - 1);
    for (size_t s = 1; s < curve.segments.size(); s++) {
      EXPECT_EQ(curve.segments[s].start_idx, curve.segments[s - 1].end_idx);
    }

    // Check |J| <= 1e-11 on every interior continued node via independent Jet<2> evaluation.
    for (size_t i = 0; i < curve.nodes.size(); i++) {
      const PoseCurveNode& node = curve.nodes[i];
      if (i > 0) {
        EXPECT_GE(node.arclength, curve.nodes[i - 1].arclength);
      }
      if (i > 0 && i + 1 < curve.nodes.size()) {
        double j_ind = 0.0;
        if (!IndependentSignedJacobian2D(f.normals, slots, 2, n, sun_dir, column, node.param, &j_ind)) {
          ADD_FAILURE() << "Independent evaluation invalid at node " << i;
          continue;
        }
        EXPECT_LE(std::fabs(j_ind), kRawInteriorJCap) << "node " << i << " |J|=" << std::fabs(j_ind);
        total_checked_nodes++;
      }
    }

    // Check arc and point-image segments.
    for (const CurveSegment& seg : curve.segments) {
      if (seg.kind == CurveSegmentKind::kPointImage) {
        // Mirror / constant-image branch sits at (-20 deg, 0 deg).
        double max_dist_from_mirror_deg = 0.0;
        double mirror_sky[3];
        AltAzDegToSky(-20.0, 0.0, mirror_sky);
        for (int i = seg.start_idx; i <= seg.end_idx; i++) {
          const double d = so3::AngleBetween(curve.nodes[static_cast<size_t>(i)].sky, mirror_sky) * kRadToDeg;
          max_dist_from_mirror_deg = std::max(max_dist_from_mirror_deg, d);
        }
        if (max_dist_from_mirror_deg < 1e-3 && seg.end_idx - seg.start_idx >= 5) {
          found_mirror_point_image = true;
          // Constant-image mirror segments must emit zero spurious kCusp events.
          const double s_lo = curve.nodes[static_cast<size_t>(seg.start_idx)].arclength;
          const double s_hi = curve.nodes[static_cast<size_t>(seg.end_idx)].arclength;
          int mirror_cusps = 0;
          for (const CurveEvent& ev : curve.events) {
            if (ev.kind == CurveEventKind::kCusp && ev.arclength >= s_lo - 1e-9 && ev.arclength <= s_hi + 1e-9) {
              mirror_cusps++;
            }
          }
          EXPECT_EQ(mirror_cusps, 0) << "Constant-image mirror emitted spurious kCusp events";
        }
      } else {
        double seg_max_alt = -90.0;
        double seg_min_alt = 90.0;
        double seg_max_abs_az = 0.0;
        double apex_near_az0_alt = -90.0;
        for (int i = seg.start_idx; i <= seg.end_idx; i++) {
          double alt = 0.0;
          double az = 0.0;
          SkyToAltAzDeg(curve.nodes[static_cast<size_t>(i)].sky, &alt, &az);
          seg_max_alt = std::max(seg_max_alt, alt);
          seg_min_alt = std::min(seg_min_alt, alt);
          seg_max_abs_az = std::max(seg_max_abs_az, std::fabs(az));
          if (std::fabs(az) < 2.0) {
            apex_near_az0_alt = std::max(apex_near_az0_alt, alt);
          }
        }
        if (apex_near_az0_alt > 40.0) {
          found_upper_arc = true;
          // Find the discrete node closest to az = 0 (minimum altitude at the gull-wing apex) and
          // also check 3-point quadratic interpolation at az = 0.
          int best_i = seg.start_idx;
          double min_abs_az = 1e9;
          for (int i = seg.start_idx; i <= seg.end_idx; i++) {
            double alt = 0.0;
            double az = 0.0;
            SkyToAltAzDeg(curve.nodes[static_cast<size_t>(i)].sky, &alt, &az);
            if (std::fabs(az) < min_abs_az) {
              min_abs_az = std::fabs(az);
              best_i = i;
            }
          }
          double alt_node = 0.0;
          double az_node = 0.0;
          SkyToAltAzDeg(curve.nodes[static_cast<size_t>(best_i)].sky, &alt_node, &az_node);
          best_upper_apex_deg = alt_node;
          if (best_i > seg.start_idx && best_i < seg.end_idx) {
            double y[3];
            double x[3];
            for (int k = 0; k < 3; k++) {
              SkyToAltAzDeg(curve.nodes[static_cast<size_t>(best_i - 1 + k)].sky, &y[k], &x[k]);
            }
            const double l0 = (-x[1]) * (-x[2]) / ((x[0] - x[1]) * (x[0] - x[2]));
            const double l1 = (-x[0]) * (-x[2]) / ((x[1] - x[0]) * (x[1] - x[2]));
            const double l2 = (-x[0]) * (-x[1]) / ((x[2] - x[0]) * (x[2] - x[1]));
            const double alt_quad_at_0 = l0 * y[0] + l1 * y[1] + l2 * y[2];
            EXPECT_NEAR(alt_quad_at_0, expected_apex_deg, 1e-4);
          }
          upper_max_abs_az_deg = std::max(upper_max_abs_az_deg, seg_max_abs_az);
          upper_min_alt_deg = std::min(upper_min_alt_deg, seg_min_alt);
          // Both tails of the upper tangent arc must terminate cleanly at kOpticalBoundary right
          // on the Snell cliff (never prematurely at kLocusSingularity).
          int upper_opt_bnds = 0;
          int upper_sings = 0;
          const double inc[3] = { -sun_dir[0], -sun_dir[1], -sun_dir[2] };
          for (const CurveEvent& ev : curve.events) {
            if (ev.kind == CurveEventKind::kOpticalBoundary) {
              upper_opt_bnds++;
              EXPECT_GE(ev.detail_index, 0);
              double out_dir[3];
              ChainDomain dom;
              double mbuf[kMaxDomainMargins]{};
              dom.margins = mbuf;
              const bool ok = TracePathChain<double>(f.normals, slots, 2, n, inc, ev.pose, out_dir, nullptr, &dom);
              const OpticalMarginVerdict v = ClassifyChainMargins(ok, dom, 2);
              EXPECT_LT(v.min_effective_margin, 1e-4) << "Upper tangent arc tail stopped far from optical boundary";
            } else if (ev.kind == CurveEventKind::kLocusSingularity) {
              upper_sings++;
            }
          }
          EXPECT_EQ(upper_opt_bnds, 2) << "Both upper tangent arc tails must terminate at kOpticalBoundary";
          EXPECT_EQ(upper_sings, 0) << "Upper tangent arc must not falsely terminate at kLocusSingularity";
        }
        if (seg_min_alt < 0.0 && seg_max_alt > -15.0) {
          found_lower_arc = true;
        }
      }
    }
  }

  EXPECT_GT(total_checked_nodes, 100);
  EXPECT_TRUE(found_upper_arc) << "Upper tangent arc not recovered";
  EXPECT_TRUE(found_lower_arc) << "Lower tangent arc not recovered";
  EXPECT_TRUE(found_mirror_point_image) << "C16 constant-image mirror branch not segmented as kPointImage";
  EXPECT_NEAR(best_upper_apex_deg, 41.916, 1e-2);
  EXPECT_NEAR(best_upper_apex_deg, expected_apex_deg, 1e-3);
  // Full cliff-hugging tail of the upper tangent arc reaches far out in azimuth (> 45 deg) and
  // dips down below 20 deg altitude (near the grazing exit optical cliff).
  EXPECT_GT(upper_max_abs_az_deg, 45.0) << "Upper tangent arc cliff tail truncated prematurely";
  EXPECT_LT(upper_min_alt_deg, 20.0) << "Upper tangent arc cliff tail did not reach low altitude";
}

// =================================================================================================
// 3. Lowitz 3-5 (sun 15 deg & 20 deg): Main + Near-Sun Branches & Antisymmetry about az = 270 deg
// =================================================================================================

TEST(CausticContinuation, LowitzMainAndNearSunBranchesAndAntisymmetry) {
  const PrismFixture f;
  int slots[2];
  const int faces[2] = { 3, 5 };
  f.Resolve(faces, 2, slots);
  const double n = IceRefractiveIndex::Get(550.0);
  const PoseManifold lowitz = PoseManifold::Lowitz(0.0);

  for (const double sun_alt_deg : { 15.0, 20.0 }) {
    double sun_dir[3];
    AltAzDegToSky(sun_alt_deg, 0.0, sun_dir);

    CausticContinuationParams params;
    const CausticTraceResult res = TraceCausticLocus(f.normals, slots, 2, n, sun_dir, lowitz, nullptr, params);
    if (res.generic_rank != 2 || res.curves.size() < 2u) {
      ADD_FAILURE() << "Lowitz sun_alt=" << sun_alt_deg << " rank=" << res.generic_rank
                    << " curves=" << res.curves.size();
      continue;
    }

    // Collect all interior nodes to verify |J| <= 1e-11 and branch coverage.
    std::vector<PoseCurveNode> all_nodes;
    bool has_main_fold = false;
    bool has_left_side_branch = false;
    bool has_right_side_branch = false;

    for (const PoseCurve& curve : res.curves) {
      EXPECT_EQ(curve.kind, PoseCurveKind::kCaustic2D);
      double c_min_az = 180.0;
      double c_max_az = -180.0;
      double c_max_alt = -90.0;
      for (size_t i = 1; i + 1 < curve.nodes.size(); i++) {
        const PoseCurveNode& node = curve.nodes[i];
        double j_ind = 0.0;
        if (!IndependentSignedJacobian2D(f.normals, slots, 2, n, sun_dir, lowitz, node.param, &j_ind)) {
          ADD_FAILURE() << "Independent evaluation invalid at sun_alt=" << sun_alt_deg << " node " << i;
          continue;
        }
        EXPECT_LE(std::fabs(j_ind), kRawInteriorJCap) << "sun_alt=" << sun_alt_deg << " |J|=" << std::fabs(j_ind);
        all_nodes.push_back(node);

        double alt = 0.0;
        double az = 0.0;
        SkyToAltAzDeg(node.sky, &alt, &az);
        c_min_az = std::min(c_min_az, az);
        c_max_az = std::max(c_max_az, az);
        c_max_alt = std::max(c_max_alt, alt);
      }
      if (c_min_az < -15.0 && c_max_az > 15.0 && c_max_alt > sun_alt_deg + 20.0) {
        has_main_fold = true;
      }
      if (c_min_az > 25.0 && std::fabs(c_max_alt - sun_alt_deg) < 2.0) {
        has_right_side_branch = true;
      }
      if (c_max_az < -25.0 && std::fabs(c_max_alt - sun_alt_deg) < 2.0) {
        has_left_side_branch = true;
      }
    }

    EXPECT_TRUE(has_main_fold) << "Main Lowitz fold branch missing for sun_alt=" << sun_alt_deg;
    EXPECT_TRUE(has_left_side_branch && has_right_side_branch)
        << "Symmetric Lowitz side branches missing for sun_alt=" << sun_alt_deg;

    // Verify exact antisymmetry about azimuth = 270 deg:
    // For face sequence 3-5 at roll = 0, reflecting (az, zenith) -> (540 deg - az, 180 deg - zenith)
    // maps J(q) -> -J(q) (or +J(q) on the locus J = 0) and reflects sky azimuth az_sky -> -az_sky
    // while keeping sky altitude invariant! Check that every continued point's mirror partner is
    // also a valid J = 0 point (independent white-box check) AND is covered by a traced curve.
    int verified_symmetric = 0;
    for (size_t idx = 0; idx < all_nodes.size(); idx += std::max<size_t>(1, all_nodes.size() / 40)) {
      const PoseCurveNode& node = all_nodes[idx];
      const double az_deg = WrapAngleDeg(node.param[PoseManifold::kAxisAzimuth] * kRadToDeg);
      const double z_deg = node.param[PoseManifold::kAxisZenith] * kRadToDeg;
      const double sym_az_deg = WrapAngleDeg(540.0 - az_deg);
      const double sym_z_deg = 180.0 - z_deg;

      // Test both candidate mirror maps: (540 - az, z) and (540 - az, 180 - z).
      double param_same_z[3] = { sym_az_deg * kDegToRad, z_deg * kDegToRad, 0.0 };
      double param_flip_z[3] = { sym_az_deg * kDegToRad, sym_z_deg * kDegToRad, 0.0 };
      double j_same = 1.0;
      double j_flip = 1.0;
      const bool ok_same = IndependentSignedJacobian2D(f.normals, slots, 2, n, sun_dir, lowitz, param_same_z, &j_same);
      const bool ok_flip = IndependentSignedJacobian2D(f.normals, slots, 2, n, sun_dir, lowitz, param_flip_z, &j_flip);
      const bool same_on_locus = ok_same && std::fabs(j_same) < 1e-9;
      const bool flip_on_locus = ok_flip && std::fabs(j_flip) < 1e-9;
      if (!(same_on_locus || flip_on_locus)) {
        ADD_FAILURE() << "No symmetric partner on J=0 for (az=" << az_deg << ", z=" << z_deg << ")";
        continue;
      }

      const double target_az_deg = sym_az_deg;
      const double target_z_deg = same_on_locus ? z_deg : sym_z_deg;

      // Verify the symmetric partner is covered by our traced curves within 2 deg in parameter space.
      double min_param_dist_deg = 1e9;
      for (const PoseCurveNode& other : all_nodes) {
        const double o_az = WrapAngleDeg(other.param[PoseManifold::kAxisAzimuth] * kRadToDeg);
        const double o_z = other.param[PoseManifold::kAxisZenith] * kRadToDeg;
        const double d = std::hypot(AngleDiffDeg(o_az, target_az_deg), o_z - target_z_deg);
        min_param_dist_deg = std::min(min_param_dist_deg, d);
      }
      EXPECT_LT(min_param_dist_deg, 2.5) << "Symmetric partner (az=" << target_az_deg << ", z=" << target_z_deg
                                         << ") not recovered by continuation (sun_alt=" << sun_alt_deg << ")";
      verified_symmetric++;
    }
    EXPECT_GT(verified_symmetric, 10);
  }
}

// =================================================================================================
// 4. Parallel Collapsed Paths (kCollapsed2D) and Near-Degenerate Point-Image vs Arc Separation
// =================================================================================================

TEST(CausticContinuation, ParallelPathsAndPoleSpinClassifyAsCollapsed2D) {
  const PrismFixture f;
  const double n = IceRefractiveIndex::Get(550.0);
  double sun_dir[3];
  AltAzDegToSky(20.0, 0.0, sun_dir);

  // Case A: Basal parallel slab {1, 2} on Column -> rank 0 collapse at the sun.
  {
    int slots[2];
    const int faces[2] = { 1, 2 };
    f.Resolve(faces, 2, slots);
    const CausticTraceResult res = TraceCausticLocus(f.normals, slots, 2, n, sun_dir, PoseManifold::Column());
    EXPECT_EQ(res.generic_rank, 0);
    ASSERT_EQ(res.curves.size(), 1u);
    EXPECT_EQ(res.curves[0].kind, PoseCurveKind::kCollapsed2D);
    ASSERT_EQ(res.curves[0].segments.size(), 1u);
    EXPECT_EQ(res.curves[0].segments[0].kind, CurveSegmentKind::kPointImage);
    EXPECT_LT(so3::AngleBetween(res.curves[0].nodes[0].sky, sun_dir) * kRadToDeg, 1e-6);
  }

  // Case B: Opposite parallel prism faces {3, 6} on Lowitz -> rank 0 collapse at the sun.
  {
    int slots[2];
    const int faces[2] = { 3, 6 };
    f.Resolve(faces, 2, slots);
    const CausticTraceResult res = TraceCausticLocus(f.normals, slots, 2, n, sun_dir, PoseManifold::Lowitz(0.0));
    EXPECT_EQ(res.generic_rank, 0);
    ASSERT_EQ(res.curves.size(), 1u);
    EXPECT_EQ(res.curves[0].kind, PoseCurveKind::kCollapsed2D);
    EXPECT_EQ(res.curves[0].segments[0].kind, CurveSegmentKind::kPointImage);
    EXPECT_LT(so3::AngleBetween(res.curves[0].nodes[0].sky, sun_dir) * kRadToDeg, 1e-6);
  }

  // Case C: Parallel-equivalent internal path {3, 5, 6, 8} (two reflections on 5,6 rotate entry 3
  // to exit 8) and basal-mirror path {3, 1, 6} on Column -> rank 1 kCollapsed2D.
  {
    int slots[4];
    const int faces[4] = { 3, 5, 6, 8 };
    f.Resolve(faces, 4, slots);
    const CausticTraceResult res = TraceCausticLocus(f.normals, slots, 4, n, sun_dir, PoseManifold::Column());
    EXPECT_EQ(res.generic_rank, 1);
    ASSERT_FALSE(res.curves.empty());
    for (const PoseCurve& c : res.curves) {
      EXPECT_EQ(c.kind, PoseCurveKind::kCollapsed2D);
    }
  }
  {
    int slots[3];
    const int faces[3] = { 3, 1, 6 };
    f.Resolve(faces, 3, slots);
    const CausticTraceResult res = TraceCausticLocus(f.normals, slots, 3, n, sun_dir, PoseManifold::Column());
    EXPECT_EQ(res.generic_rank, 1);
    ASSERT_FALSE(res.curves.empty());
    for (const PoseCurve& c : res.curves) {
      EXPECT_EQ(c.kind, PoseCurveKind::kCollapsed2D);
    }
    // Independent 2D coverage check: every valid pose on the original 2D Column manifold (including
    // poses like (az = 132 deg, roll = 70 deg) that are infeasible at the best-margin roll slice)
    // must map close to the returned rank-1 sky image.
    const double inc[3] = { -sun_dir[0], -sun_dir[1], -sun_dir[2] };
    auto check_pose_covered = [&](double az_deg, double roll_deg) {
      double pose[9];
      EulerPoseDeg(az_deg, 90.0, roll_deg, pose);
      double out_dir[3];
      if (!TracePathChain<double>(f.normals, slots, 3, n, inc, pose, out_dir, nullptr, nullptr)) {
        return;
      }
      const double sky[3] = { -out_dir[0], -out_dir[1], -out_dir[2] };
      double best_deg = 180.0;
      for (const PoseCurve& c : res.curves) {
        for (const PoseCurveNode& nd : c.nodes) {
          best_deg = std::min(best_deg, so3::AngleBetween(sky, nd.sky) * kRadToDeg);
        }
      }
      EXPECT_LT(best_deg, 3.0) << "Rank-1 image missed valid pose (az=" << az_deg << ", roll=" << roll_deg << ")";
    };
    check_pose_covered(132.0, 70.0);
    for (int ai = 0; ai < 36; ai++) {
      for (int ri = 0; ri < 18; ri++) {
        check_pose_covered(10.0 * ai + 2.0, 20.0 * ri + 5.0);
      }
    }
  }

  // Case D: Euler pole spin chart TiltedSpin(0.0) on {3, 5} -> generator rank loss explicitly
  // flagged via kDomainBoundary on kAxisZenith alongside the collapsed 1D image.
  {
    int slots[2];
    const int faces[2] = { 3, 5 };
    f.Resolve(faces, 2, slots);
    const CausticTraceResult res = TraceCausticLocus(f.normals, slots, 2, n, sun_dir, PoseManifold::TiltedSpin(0.0));
    EXPECT_EQ(res.generic_rank, 1);
    ASSERT_FALSE(res.curves.empty());
    int zenith_pole_events = 0;
    for (const PoseCurve& c : res.curves) {
      EXPECT_EQ(c.kind, PoseCurveKind::kCollapsed2D);
      EXPECT_GT(c.nodes.size(), 10u);
      for (const CurveEvent& ev : c.events) {
        if (ev.kind == CurveEventKind::kDomainBoundary && ev.detail_index == PoseManifold::kAxisZenith) {
          zenith_pole_events++;
        }
      }
    }
    EXPECT_GT(zenith_pole_events, 0) << "Euler pole singularity must emit kDomainBoundary on kAxisZenith";
  }

  // Case E: TiltedSpin(89.99) breaks the mirror identity on {3, 5}, so no kPointImage segment.
  {
    int slots[2];
    const int faces[2] = { 3, 5 };
    f.Resolve(faces, 2, slots);
    const CausticTraceResult res = TraceCausticLocus(f.normals, slots, 2, n, sun_dir, PoseManifold::TiltedSpin(89.99));
    EXPECT_EQ(res.generic_rank, 2);
    for (const PoseCurve& c : res.curves) {
      for (const CurveSegment& seg : c.segments) {
        EXPECT_EQ(seg.kind, CurveSegmentKind::kArc);
      }
    }
  }
}

// =================================================================================================
// 5. Parry 3-5 and Plate 3-5 1D Manifolds via TraceFamily1D
// =================================================================================================

TEST(CausticContinuation, ParryAndPlate1DManifoldsAndParhelionStationaryPoint) {
  const PrismFixture f;
  int slots[2];
  const int faces[2] = { 3, 5 };
  f.Resolve(faces, 2, slots);
  const double n = IceRefractiveIndex::Get(550.0);
  double sun_dir[3];
  AltAzDegToSky(20.0, 0.0, sun_dir);

  // Parry 3-5: ordered trajectory with optical boundary endpoints.
  {
    const CausticTraceResult res = TraceFamily1D(f.normals, slots, 2, n, sun_dir, PoseManifold::Parry(0.0));
    EXPECT_EQ(res.generic_rank, 1);
    ASSERT_FALSE(res.curves.empty());
    int optical_boundaries = 0;
    for (const PoseCurve& curve : res.curves) {
      EXPECT_EQ(curve.kind, PoseCurveKind::kFamily1D);
      EXPECT_GT(curve.nodes.size(), 5u);
      for (size_t i = 1; i < curve.nodes.size(); i++) {
        EXPECT_GT(curve.nodes[i].arclength, curve.nodes[i - 1].arclength);
        EXPECT_LT(so3::AngleBetween(curve.nodes[i].sky, curve.nodes[i - 1].sky) * kRadToDeg, 10.0);
      }
      for (const CurveEvent& ev : curve.events) {
        if (ev.kind == CurveEventKind::kOpticalBoundary) {
          optical_boundaries++;
          EXPECT_GE(ev.detail_index, 0);
        }
      }
    }
    EXPECT_GE(optical_boundaries, 2);
  }

  // Plate 3-5 (parhelion): ordered trajectory, optical boundary endpoints, and exact stationary
  // point at the parhelion inner edge (Bravais effective-index minimum deviation).
  {
    const CausticTraceResult res = TraceFamily1D(f.normals, slots, 2, n, sun_dir, PoseManifold::Plate());
    EXPECT_EQ(res.generic_rank, 1);
    ASSERT_FALSE(res.curves.empty());

    const double sin_h = std::sin(20.0 * kDegToRad);
    const double cos_h = std::cos(20.0 * kDegToRad);
    const double n_eff = std::sqrt((n * n - sin_h * sin_h) / (cos_h * cos_h));
    const double expected_parhelion_az_deg = (2.0 * std::asin(n_eff * 0.5) - kPi / 3.0) * kRadToDeg;

    int optical_boundaries = 0;
    int stationary_points = 0;
    for (const PoseCurve& curve : res.curves) {
      EXPECT_EQ(curve.kind, PoseCurveKind::kFamily1D);
      for (const PoseCurveNode& node : curve.nodes) {
        double alt = 0.0;
        double az = 0.0;
        SkyToAltAzDeg(node.sky, &alt, &az);
        // Every parhelion ray from a horizontal plate sits on the parhelic circle at alt == sun_alt.
        EXPECT_NEAR(alt, 20.0, 1e-9);
      }
      for (const CurveEvent& ev : curve.events) {
        if (ev.kind == CurveEventKind::kOpticalBoundary) {
          optical_boundaries++;
          EXPECT_GE(ev.detail_index, 0);
        } else if (ev.kind == CurveEventKind::kStationaryPoint) {
          stationary_points++;
          double alt = 0.0;
          double az = 0.0;
          SkyToAltAzDeg(ev.sky, &alt, &az);
          EXPECT_NEAR(alt, 20.0, 1e-6);
          EXPECT_NEAR(std::fabs(az), expected_parhelion_az_deg, 1e-4);
        }
      }
    }
    EXPECT_GE(optical_boundaries, 2);
    EXPECT_GE(stationary_points, 1) << "Parhelion inner-edge stationary point not detected";
  }
}

// =================================================================================================
// 6. Internal TIR Onset & Finite Corridor Cutoff without Prematurely Terminating Continuation
// =================================================================================================

TEST(CausticContinuation, InternalTirOnsetAndCorridorCutoffDoNotStopGeometricContinuation) {
  const PrismFixture f(0.8);
  const double n = IceRefractiveIndex::Get(550.0);
  double sun_dir[3];
  AltAzDegToSky(20.0, 0.0, sun_dir);

  // Path {3, 1, 6}: internal reflection on basal face 1 crosses TIR critical angle AND finite
  // crystal corridor A_P turns off while optical path remains geometrically valid.
  int slots[3];
  const int faces[3] = { 3, 1, 6 };
  f.Resolve(faces, 3, slots);
  Corridor corridor(f.normals, f.polygons, slots, 3);

  // 1D manifold: Parry(0.0) with {3, 1, 6}.
  {
    const CausticTraceResult res1d =
        TraceFamily1D(f.normals, slots, 3, n, sun_dir, PoseManifold::Parry(0.0), &corridor);
    ASSERT_FALSE(res1d.curves.empty());
    int tir_events = 0;
    int cutoff_events = 0;
    bool has_open_seg = false;
    bool has_closed_seg = false;
    for (const PoseCurve& curve : res1d.curves) {
      for (const CurveSegment& seg : curve.segments) {
        has_open_seg = has_open_seg || seg.corridor_open;
        has_closed_seg = has_closed_seg || !seg.corridor_open;
      }
      for (const CurveEvent& ev : curve.events) {
        if (ev.kind == CurveEventKind::kTirOnset) {
          tir_events++;
          EXPECT_EQ(ev.detail_index, 1);
          // Independently verify internal reflection discriminant at the bisected TIR onset is ~0.
          const double inc[3] = { -sun_dir[0], -sun_dir[1], -sun_dir[2] };
          double out_dir[3];
          ChainInterfaceDiagnostics<double> diag;
          if (!TracePathChain<double>(f.normals, slots, 3, n, inc, ev.pose, out_dir, nullptr, nullptr, &diag)) {
            ADD_FAILURE() << "TracePathChain invalid at 1D TIR onset";
            continue;
          }
          EXPECT_NEAR(diag.discriminant[1], 0.0, 1e-7);
        } else if (ev.kind == CurveEventKind::kCorridorCutoff) {
          cutoff_events++;
          EXPECT_EQ(ev.detail_index, -1);
        }
      }
    }
    EXPECT_GT(tir_events, 0) << "Expected kTirOnset on {3, 1, 6} Parry 1D";
    EXPECT_GT(cutoff_events, 0) << "Expected kCorridorCutoff on {3, 1, 6} Parry 1D";
    EXPECT_TRUE(has_open_seg && has_closed_seg) << "Geometric continuation must span both open and closed corridor";
  }

  // 2D manifold: Column() with {3, 1, 5} (60-deg prism with internal basal reflection on face 1,
  // generic rank 2) and finite Corridor -> exact kTirOnset AND kCorridorCutoff on J = 0.
  {
    int slots315[3];
    const int faces315[3] = { 3, 1, 5 };
    f.Resolve(faces315, 3, slots315);
    Corridor corridor315(f.normals, f.polygons, slots315, 3);

    const CausticTraceResult res2d =
        TraceCausticLocus(f.normals, slots315, 3, n, sun_dir, PoseManifold::Column(), &corridor315);
    EXPECT_EQ(res2d.generic_rank, 2);
    ASSERT_FALSE(res2d.curves.empty());
    int tir_events = 0;
    int cutoff_events = 0;
    bool has_open_seg = false;
    bool has_closed_seg = false;
    for (const PoseCurve& curve : res2d.curves) {
      for (const CurveSegment& seg : curve.segments) {
        has_open_seg = has_open_seg || seg.corridor_open;
        has_closed_seg = has_closed_seg || !seg.corridor_open;
      }
      for (const CurveEvent& ev : curve.events) {
        if (ev.kind == CurveEventKind::kTirOnset) {
          tir_events++;
          EXPECT_EQ(ev.detail_index, 1);
          double j_ind = 0.0;
          double discs[kMaxFaceCount]{};
          if (!IndependentSignedJacobian2D(f.normals, slots315, 3, n, sun_dir, PoseManifold::Column(), ev.param, &j_ind,
                                           nullptr, nullptr, discs)) {
            ADD_FAILURE() << "Independent evaluation invalid at 2D TIR onset";
            continue;
          }
          EXPECT_LE(std::fabs(j_ind), 1e-10);
          EXPECT_NEAR(discs[1], 0.0, 1e-6);
        } else if (ev.kind == CurveEventKind::kCorridorCutoff) {
          cutoff_events++;
          double j_ind = 0.0;
          if (!IndependentSignedJacobian2D(f.normals, slots315, 3, n, sun_dir, PoseManifold::Column(), ev.param,
                                           &j_ind)) {
            ADD_FAILURE() << "Independent evaluation invalid at 2D corridor cutoff";
            continue;
          }
          EXPECT_LE(std::fabs(j_ind), 1e-10);
        }
      }
    }
    EXPECT_GT(tir_events, 0) << "Expected kTirOnset on {3, 1, 5} Column 2D";
    EXPECT_GT(cutoff_events, 0) << "Expected kCorridorCutoff on {3, 1, 5} Column 2D";
    EXPECT_TRUE(has_open_seg && has_closed_seg)
        << "2D caustic continuation must continue geometrically past corridor cutoff";
  }

  // Multi-internal-reflection path {3, 5, 6, 7, 3} on Column: verifies multi-step kTirOnset tracking.
  {
    int slots5[5];
    const int faces5[5] = { 3, 5, 6, 7, 3 };
    f.Resolve(faces5, 5, slots5);
    const CausticTraceResult res5 = TraceCausticLocus(f.normals, slots5, 5, n, sun_dir, PoseManifold::Column());
    EXPECT_EQ(res5.generic_rank, 2);
    ASSERT_FALSE(res5.curves.empty());
    int tir_events = 0;
    for (const PoseCurve& curve : res5.curves) {
      for (const CurveEvent& ev : curve.events) {
        if (ev.kind == CurveEventKind::kTirOnset) {
          tir_events++;
          if (ev.detail_index < 1 || ev.detail_index > 3) {
            ADD_FAILURE() << "Unexpected internal step index " << ev.detail_index;
            continue;
          }
          double j_ind = 0.0;
          double discs[kMaxFaceCount]{};
          if (!IndependentSignedJacobian2D(f.normals, slots5, 5, n, sun_dir, PoseManifold::Column(), ev.param, &j_ind,
                                           nullptr, nullptr, discs)) {
            ADD_FAILURE() << "Independent evaluation invalid at 5-face TIR onset";
            continue;
          }
          EXPECT_LE(std::fabs(j_ind), 1e-10);
          EXPECT_NEAR(discs[ev.detail_index], 0.0, 1e-6);
        }
      }
    }
    EXPECT_GT(tir_events, 0) << "Expected kTirOnset events on {3, 5, 6, 7, 3} Column 2D";
  }
}

// =================================================================================================
// 7. Tight Bounded PoseManifold (Compact-Support Bounds) -> Clean kDomainBoundary Stops
// =================================================================================================

TEST(CausticContinuation, TightBoundedManifoldStopsAtDomainBoundary) {
  const PrismFixture f;
  int slots[2];
  const int faces[2] = { 3, 5 };
  f.Resolve(faces, 2, slots);
  const double n = IceRefractiveIndex::Get(550.0);
  double sun_dir[3];
  AltAzDegToSky(20.0, 0.0, sun_dir);

  // 2D Lowitz with compact-support zenith box [45 deg, 75 deg] and non-periodic azimuth [200 deg, 260 deg]
  // enclosing the upper Lowitz witness (zenith ~59.8 deg, az ~237.7 deg).
  {
    PoseManifold bounded;
    bounded.axes[PoseManifold::kAxisAzimuth] = PoseAxis::FreeDeg(200.0, 260.0, false);
    bounded.axes[PoseManifold::kAxisZenith] = PoseAxis::FreeDeg(45.0, 75.0, false);
    bounded.axes[PoseManifold::kAxisRoll] = PoseAxis::PinnedDeg(0.0);

    const CausticTraceResult res = TraceCausticLocus(f.normals, slots, 2, n, sun_dir, bounded);
    EXPECT_EQ(res.generic_rank, 2);
    ASSERT_FALSE(res.curves.empty());

    int domain_events = 0;
    for (const PoseCurve& curve : res.curves) {
      EXPECT_FALSE(curve.coverage.closed);
      for (const PoseCurveNode& node : curve.nodes) {
        EXPECT_GE(node.param[PoseManifold::kAxisAzimuth], 200.0 * kDegToRad - 1e-10);
        EXPECT_LE(node.param[PoseManifold::kAxisAzimuth], 260.0 * kDegToRad + 1e-10);
        EXPECT_GE(node.param[PoseManifold::kAxisZenith], 45.0 * kDegToRad - 1e-10);
        EXPECT_LE(node.param[PoseManifold::kAxisZenith], 75.0 * kDegToRad + 1e-10);
      }
      for (const CurveEvent& ev : curve.events) {
        if (ev.kind == CurveEventKind::kDomainBoundary) {
          domain_events++;
          if (ev.detail_index != PoseManifold::kAxisAzimuth && ev.detail_index != PoseManifold::kAxisZenith) {
            ADD_FAILURE() << "Unexpected domain boundary axis " << ev.detail_index;
            continue;
          }
          const PoseAxis& ax = bounded.axes[ev.detail_index];
          const double d_bound = std::min(std::fabs(ev.param[ev.detail_index] - ax.min_val),
                                          std::fabs(ev.param[ev.detail_index] - ax.max_val));
          EXPECT_LT(d_bound, 1e-7) << "Domain boundary event did not land on exact bound";
          double j_ind = 0.0;
          if (!IndependentSignedJacobian2D(f.normals, slots, 2, n, sun_dir, bounded, ev.param, &j_ind)) {
            ADD_FAILURE() << "Independent evaluation invalid at domain boundary";
            continue;
          }
          EXPECT_LE(std::fabs(j_ind), 1e-10);
        }
      }
    }
    EXPECT_GE(domain_events, 2) << "Expected domain boundary stops on compact-support 2D box";
  }

  // 1D Plate with compact azimuth window inside the valid parhelion interval -> 2 kDomainBoundary events.
  {
    const CausticTraceResult full_plate = TraceFamily1D(f.normals, slots, 2, n, sun_dir, PoseManifold::Plate());
    ASSERT_FALSE(full_plate.curves.empty());
    const PoseCurve& c = full_plate.curves[0];
    ASSERT_GT(c.nodes.size(), 10u);
    const double t_lo = c.nodes[c.nodes.size() / 4].param[PoseManifold::kAxisAzimuth];
    const double t_hi = c.nodes[(3 * c.nodes.size()) / 4].param[PoseManifold::kAxisAzimuth];
    if (t_hi > t_lo + 0.05) {
      PoseManifold tight_plate = PoseManifold::Plate();
      tight_plate.axes[PoseManifold::kAxisAzimuth] = PoseAxis::FreeRad(t_lo, t_hi, false);
      const CausticTraceResult res1d = TraceFamily1D(f.normals, slots, 2, n, sun_dir, tight_plate);
      ASSERT_EQ(res1d.curves.size(), 1u);
      int bnd_cnt = 0;
      for (const CurveEvent& ev : res1d.curves[0].events) {
        if (ev.kind == CurveEventKind::kDomainBoundary) {
          bnd_cnt++;
          EXPECT_EQ(ev.detail_index, PoseManifold::kAxisAzimuth);
        }
      }
      EXPECT_EQ(bnd_cnt, 2);
    }
  }
}

// =================================================================================================
// 8. Cooperative Cancellation via DiscoveryStop
// =================================================================================================

TEST(CausticContinuation, CooperativeStopHonored) {
  const PrismFixture f;
  int slots[2];
  const int faces[2] = { 3, 5 };
  f.Resolve(faces, 2, slots);
  const double n = IceRefractiveIndex::Get(550.0);
  double sun_dir[3];
  AltAzDegToSky(20.0, 0.0, sun_dir);

  StopToken token;
  token.RequestCancel();
  DiscoveryStop stop;
  stop.token = &token;

  const CausticTraceResult r1 =
      TraceFamily1D(f.normals, slots, 2, n, sun_dir, PoseManifold::Parry(0.0), nullptr, {}, &stop);
  EXPECT_TRUE(r1.stop_requested);
  EXPECT_TRUE(r1.curves.empty());

  const CausticTraceResult r2 =
      TraceCausticLocus(f.normals, slots, 2, n, sun_dir, PoseManifold::Column(), nullptr, {}, &stop);
  EXPECT_TRUE(r2.stop_requested);
  EXPECT_TRUE(r2.curves.empty());
}

// =================================================================================================
// 9. Edge-Case Regressions: Narrow 1D Gap, Exact Stationary Node, Truncated Loop, Small Rank-2,
//    Param Validation, and Exact max_curves Budget Honesty
// =================================================================================================

TEST(CausticContinuation, EdgeCaseRegressions) {
  const PrismFixture f;
  const double n = IceRefractiveIndex::Get(550.0);

  // 9a. Narrow optical gap inside an all-valid coarse 1D periodic scan: basal reflection {1},
  //     free azimuth, zenith 20.000005 deg, roll 0 deg, sun (20 deg, 0.481705 deg). Must recover
  //     both sides of the gap and stitch across the periodic seam without discarding half the loop.
  {
    int slots1[1];
    const int faces1[1] = { 1 };
    f.Resolve(faces1, 1, slots1);
    double sun_gap[3];
    AltAzDegToSky(20.0, 0.481705, sun_gap);
    PoseManifold m;
    m.axes[PoseManifold::kAxisAzimuth] = PoseAxis::FreeRad(0.0, 2.0 * kPi, true);
    m.axes[PoseManifold::kAxisZenith] = PoseAxis::PinnedDeg(20.000005);
    m.axes[PoseManifold::kAxisRoll] = PoseAxis::PinnedDeg(0.0);
    const CausticTraceResult res = TraceFamily1D(f.normals, slots1, 1, n, sun_gap, m);
    ASSERT_EQ(res.curves.size(), 1u);
    const PoseCurve& c = res.curves[0];
    EXPECT_FALSE(c.coverage.closed);
    EXPECT_FALSE(c.coverage.budget_truncated);
    EXPECT_GT(c.nodes.back().arclength, 6.2) << "Narrow optical gap discarded valid poses";
    int opt_bnds = 0;
    for (const CurveEvent& ev : c.events) {
      if (ev.kind == CurveEventKind::kOpticalBoundary) {
        opt_bnds++;
      }
    }
    EXPECT_EQ(opt_bnds, 2);
  }

  // 9b. Exact node hit on parhelion stationary point: Plate {3, 5} compact interval +-0.016 rad
  //     centered directly on the minimum with max_step = 0.004 must still emit kStationaryPoint.
  {
    int slots35[2];
    const int faces35[2] = { 3, 5 };
    f.Resolve(faces35, 2, slots35);
    double sun20[3];
    AltAzDegToSky(20.0, 0.0, sun20);
    const CausticTraceResult full_plate = TraceFamily1D(f.normals, slots35, 2, n, sun20, PoseManifold::Plate());
    double stat_az = -1.0;
    for (const PoseCurve& c : full_plate.curves) {
      for (const CurveEvent& ev : c.events) {
        if (ev.kind == CurveEventKind::kStationaryPoint) {
          stat_az = ev.param[PoseManifold::kAxisAzimuth];
          break;
        }
      }
    }
    ASSERT_GE(stat_az, 0.0);
    PoseManifold centered = PoseManifold::Plate();
    centered.axes[PoseManifold::kAxisAzimuth] = PoseAxis::FreeRad(stat_az - 0.016, stat_az + 0.016, false);
    Family1DParams fp;
    fp.max_step = 0.004;
    const CausticTraceResult res = TraceFamily1D(f.normals, slots35, 2, n, sun20, centered, nullptr, fp);
    ASSERT_EQ(res.curves.size(), 1u);
    int stat_cnt = 0;
    for (const CurveEvent& ev : res.curves[0].events) {
      if (ev.kind == CurveEventKind::kStationaryPoint) {
        stat_cnt++;
      }
    }
    EXPECT_EQ(stat_cnt, 1) << "Exact node hit on stationary point suppressed kStationaryPoint";
  }

  // 9c. Budget-truncated periodic 1D trace must NOT be marked closed.
  {
    int slots1[1];
    const int faces1[1] = { 1 };
    f.Resolve(faces1, 1, slots1);
    double sun20[3];
    AltAzDegToSky(20.0, 0.0, sun20);
    Family1DParams fp;
    fp.max_nodes_per_branch = 2;
    const CausticTraceResult res = TraceFamily1D(f.normals, slots1, 1, n, sun20, PoseManifold::Plate(), nullptr, fp);
    ASSERT_EQ(res.curves.size(), 1u);
    EXPECT_TRUE(res.curves[0].coverage.budget_truncated);
    EXPECT_FALSE(res.curves[0].coverage.closed);
    EXPECT_FALSE(res.coverage.closed);
  }

  // 9d. Small, well-conditioned rank-2 map (n = 1.000000001 on 3-5, sigma1 ~ 2.9e-8,
  //     sigma2/sigma1 ~ 0.15 >> rank_tol) must classify as generic_rank == 2, not rank 0.
  {
    int slots35[2];
    const int faces35[2] = { 3, 5 };
    f.Resolve(faces35, 2, slots35);
    double sun20[3];
    AltAzDegToSky(20.0, 0.0, sun20);
    PoseManifold tiny_box;
    tiny_box.axes[PoseManifold::kAxisAzimuth] = PoseAxis::FreeDeg(-1.0, 1.0, false);
    tiny_box.axes[PoseManifold::kAxisZenith] = PoseAxis::PinnedDeg(90.0);
    tiny_box.axes[PoseManifold::kAxisRoll] = PoseAxis::FreeDeg(-1.0, 1.0, false);
    const CausticTraceResult res = TraceCausticLocus(f.normals, slots35, 2, 1.000000001, sun20, tiny_box);
    EXPECT_EQ(res.generic_rank, 2);
  }

  // 9e. Invalid step parameters are rejected cleanly (never hanging in retry loop).
  {
    int slots35[2];
    const int faces35[2] = { 3, 5 };
    f.Resolve(faces35, 2, slots35);
    double sun20[3];
    AltAzDegToSky(20.0, 0.0, sun20);

    CausticContinuationParams bad_neg;
    bad_neg.min_step = -1e-6;
    EXPECT_FALSE(ValidateParams(bad_neg));
    const CausticTraceResult r_bad =
        TraceCausticLocus(f.normals, slots35, 2, n, sun20, PoseManifold::Column(), nullptr, bad_neg);
    EXPECT_TRUE(r_bad.curves.empty());

    CausticContinuationParams bad_order;
    bad_order.min_step = 0.1;
    bad_order.max_step = 0.05;
    EXPECT_FALSE(ValidateParams(bad_order));

    Family1DParams bad_1d;
    bad_1d.min_step = 0.0;
    EXPECT_FALSE(ValidateParams(bad_1d));
    const CausticTraceResult r_bad1d =
        TraceFamily1D(f.normals, slots35, 2, n, sun20, PoseManifold::Plate(), nullptr, bad_1d);
    EXPECT_TRUE(r_bad1d.curves.empty());
  }

  // 9f. Reaching exact curve count max_curves == 3 on Lowitz 3-5 at 20 deg must NOT mark
  //     budget_truncated = true (all remaining seeds are duplicates of the 3 traced curves).
  {
    int slots35[2];
    const int faces35[2] = { 3, 5 };
    f.Resolve(faces35, 2, slots35);
    double sun20[3];
    AltAzDegToSky(20.0, 0.0, sun20);

    CausticContinuationParams p3;
    p3.max_curves = 3;
    const CausticTraceResult r3 =
        TraceCausticLocus(f.normals, slots35, 2, n, sun20, PoseManifold::Lowitz(0.0), nullptr, p3);
    EXPECT_EQ(r3.curves.size(), 3u);
    EXPECT_FALSE(r3.coverage.budget_truncated);

    CausticContinuationParams p2;
    p2.max_curves = 2;
    const CausticTraceResult r2 =
        TraceCausticLocus(f.normals, slots35, 2, n, sun20, PoseManifold::Lowitz(0.0), nullptr, p2);
    EXPECT_EQ(r2.curves.size(), 2u);
    EXPECT_TRUE(r2.coverage.budget_truncated);
  }

  // 9g. 1D periodic trace must NOT close across an interior grazing optical gap missed by step
  //     endpoints: basal reflection {1}, zenith 20.000005 deg, roll 0 deg, sun (20 deg, 0.6 deg).
  {
    int slots1[1];
    const int faces1[1] = { 1 };
    f.Resolve(faces1, 1, slots1);
    double sun_gap[3];
    AltAzDegToSky(20.0, 0.6, sun_gap);
    PoseManifold m;
    m.axes[PoseManifold::kAxisAzimuth] = PoseAxis::FreeRad(0.0, 2.0 * kPi, true);
    m.axes[PoseManifold::kAxisZenith] = PoseAxis::PinnedDeg(20.000005);
    m.axes[PoseManifold::kAxisRoll] = PoseAxis::PinnedDeg(0.0);
    const CausticTraceResult res = TraceFamily1D(f.normals, slots1, 1, n, sun_gap, m);
    ASSERT_EQ(res.curves.size(), 1u);
    EXPECT_FALSE(res.curves[0].coverage.closed);
    EXPECT_FALSE(res.coverage.closed);
    int opt_bnds = 0;
    for (const CurveEvent& ev : res.curves[0].events) {
      if (ev.kind == CurveEventKind::kOpticalBoundary) {
        opt_bnds++;
      }
    }
    EXPECT_EQ(opt_bnds, 2) << "1D trace closed across an optically invalid interior gap";
  }

  // 9h. Stitching preserves backward boundary events when the seed lands on a domain boundary:
  //     Column {3, 5} at 20 deg, azimuth [220, 224] deg, periodic roll [-180, 180).
  {
    int slots35[2];
    const int faces35[2] = { 3, 5 };
    f.Resolve(faces35, 2, slots35);
    double sun20[3];
    AltAzDegToSky(20.0, 0.0, sun20);
    PoseManifold m;
    m.axes[PoseManifold::kAxisAzimuth] = PoseAxis::FreeDeg(220.0, 224.0, false);
    m.axes[PoseManifold::kAxisZenith] = PoseAxis::PinnedDeg(90.0);
    m.axes[PoseManifold::kAxisRoll] = PoseAxis::FreeDeg(-180.0, 180.0, true);
    const CausticTraceResult res = TraceCausticLocus(f.normals, slots35, 2, n, sun20, m);
    ASSERT_FALSE(res.curves.empty());
    bool hit_220 = false;
    bool hit_224 = false;
    for (const PoseCurve& c : res.curves) {
      for (const CurveEvent& ev : c.events) {
        if (ev.kind == CurveEventKind::kDomainBoundary && ev.detail_index == PoseManifold::kAxisAzimuth) {
          const double az_deg = ev.param[PoseManifold::kAxisAzimuth] * kRadToDeg;
          if (std::fabs(az_deg - 220.0) < 1e-2) {
            hit_220 = true;
          }
          if (std::fabs(az_deg - 224.0) < 1e-2) {
            hit_224 = true;
          }
        }
      }
    }
    EXPECT_TRUE(hit_220) << "Missing 220 deg domain boundary event after stitching";
    EXPECT_TRUE(hit_224) << "Missing 224 deg domain boundary event after stitching";
  }

  // 9i. Continuous poses, consistent arclength/tangent, and max_curves enforcement on rank-1
  //     collapsed Column {3, 1, 6}.
  {
    int slots316[3];
    const int faces316[3] = { 3, 1, 6 };
    f.Resolve(faces316, 3, slots316);
    double sun20[3];
    AltAzDegToSky(20.0, 0.0, sun20);

    const CausticTraceResult res = TraceCausticLocus(f.normals, slots316, 3, n, sun20, PoseManifold::Column());
    EXPECT_EQ(res.generic_rank, 1);
    ASSERT_FALSE(res.curves.empty());
    for (const PoseCurve& c : res.curves) {
      for (size_t i = 1; i < c.nodes.size(); i++) {
        const PoseCurveNode& a = c.nodes[i - 1];
        const PoseCurveNode& b = c.nodes[i];
        // Consecutive poses in SO(3) must stay continuous (never teleport 150 deg across rolls).
        double trace = 0.0;
        for (int k = 0; k < 9; k++) {
          trace += a.pose[k] * b.pose[k];
        }
        const double so3_angle_deg = std::acos(std::clamp(0.5 * (trace - 1.0), -1.0, 1.0)) * kRadToDeg;
        EXPECT_LT(so3_angle_deg, 20.0) << "Discontinuous SO(3) pose jump in rank-1 collapsed curve";
        const double d_az = AngleDiffDeg(b.param[PoseManifold::kAxisAzimuth] * kRadToDeg,
                                         a.param[PoseManifold::kAxisAzimuth] * kRadToDeg) *
                            kDegToRad;
        const double d_roll =
            AngleDiffDeg(b.param[PoseManifold::kAxisRoll] * kRadToDeg, a.param[PoseManifold::kAxisRoll] * kRadToDeg) *
            kDegToRad;
        const double ds_param = std::hypot(d_az, d_roll);
        EXPECT_NEAR(b.arclength - a.arclength, ds_param, 1e-4);
      }
    }

    // Nonperiodic [180, 540] deg with max_curves = 1 must truncate cleanly at 1 curve.
    PoseManifold nonper;
    nonper.axes[PoseManifold::kAxisAzimuth] = PoseAxis::FreeDeg(180.0, 540.0, false);
    nonper.axes[PoseManifold::kAxisZenith] = PoseAxis::PinnedDeg(90.0);
    nonper.axes[PoseManifold::kAxisRoll] = PoseAxis::FreeDeg(-180.0, 180.0, true);
    CausticContinuationParams p1;
    p1.max_curves = 1;
    const CausticTraceResult r_lim = TraceCausticLocus(f.normals, slots316, 3, n, sun20, nonper, nullptr, p1);
    EXPECT_EQ(r_lim.curves.size(), 1u);
    EXPECT_TRUE(r_lim.coverage.budget_truncated);
  }

  // 9j. Compact Euler pole TiltedSpin(0 deg) on face {3}, az [100, 120] deg, roll [0, 20] deg:
  //     must cover full attainable az + roll in [100, 140] deg (including pose (100, 0) deg)
  //     with all poses staying within both bounds.
  {
    int slots3[1];
    const int faces3[1] = { 3 };
    f.Resolve(faces3, 1, slots3);
    double sun20[3];
    AltAzDegToSky(20.0, 0.0, sun20);
    PoseManifold pole;
    pole.axes[PoseManifold::kAxisAzimuth] = PoseAxis::FreeDeg(100.0, 120.0, false);
    pole.axes[PoseManifold::kAxisZenith] = PoseAxis::PinnedDeg(0.0);
    pole.axes[PoseManifold::kAxisRoll] = PoseAxis::FreeDeg(0.0, 20.0, false);
    const CausticTraceResult res = TraceCausticLocus(f.normals, slots3, 1, n, sun20, pole);
    EXPECT_EQ(res.generic_rank, 1);
    ASSERT_FALSE(res.curves.empty());

    // All emitted poses must stay within both axis bounds.
    bool saw_pole_ev = false;
    for (const PoseCurve& c : res.curves) {
      for (const CurveEvent& ev : c.events) {
        if (ev.kind == CurveEventKind::kDomainBoundary && ev.detail_index == PoseManifold::kAxisZenith) {
          saw_pole_ev = true;
        }
      }
      for (const PoseCurveNode& nd : c.nodes) {
        const double az_d = nd.param[PoseManifold::kAxisAzimuth] * kRadToDeg;
        const double r_d = nd.param[PoseManifold::kAxisRoll] * kRadToDeg;
        EXPECT_GE(az_d, 100.0 - 1e-6);
        EXPECT_LE(az_d, 120.0 + 1e-6);
        EXPECT_GE(r_d, 0.0 - 1e-6);
        EXPECT_LE(r_d, 20.0 + 1e-6);
      }
    }
    EXPECT_TRUE(saw_pole_ev);

    // Check independent sky coverage for both corner poses (100, 0) and (120, 20).
    const double inc[3] = { -sun20[0], -sun20[1], -sun20[2] };
    for (const std::pair<double, double>& pt : { std::make_pair(100.0, 0.0), std::make_pair(120.0, 20.0) }) {
      double pose[9];
      EulerPoseDeg(pt.first, 0.0, pt.second, pose);
      double out_dir[3];
      if (!TracePathChain<double>(f.normals, slots3, 1, n, inc, pose, out_dir, nullptr, nullptr)) {
        ADD_FAILURE() << "Corner pose (" << pt.first << ", " << pt.second << ") unexpectedly invalid";
        continue;
      }
      const double sky_ref[3] = { -out_dir[0], -out_dir[1], -out_dir[2] };
      double min_ang_deg = 180.0;
      for (const PoseCurve& c : res.curves) {
        for (const PoseCurveNode& nd : c.nodes) {
          min_ang_deg = std::min(min_ang_deg, so3::AngleBetween(sky_ref, nd.sky) * kRadToDeg);
        }
      }
      EXPECT_LT(min_ang_deg, 1.0) << "Compact Euler pole missed sky direction for (" << pt.first << ", " << pt.second
                                  << ")";
    }
  }

  // 9k. Fine 1D step resolving the parhelion near-zero neighborhood with > 4 nodes must still
  //     emit kStationaryPoint: Plate {3, 5}, +-1.6e-6 rad centered on minimum, step = 1e-7.
  {
    int slots35[2];
    const int faces35[2] = { 3, 5 };
    f.Resolve(faces35, 2, slots35);
    double sun20[3];
    AltAzDegToSky(20.0, 0.0, sun20);
    const CausticTraceResult full_plate = TraceFamily1D(f.normals, slots35, 2, n, sun20, PoseManifold::Plate());
    double stat_az = -1.0;
    for (const PoseCurve& c : full_plate.curves) {
      for (const CurveEvent& ev : c.events) {
        if (ev.kind == CurveEventKind::kStationaryPoint) {
          stat_az = ev.param[PoseManifold::kAxisAzimuth];
          break;
        }
      }
    }
    ASSERT_GE(stat_az, 0.0);
    PoseManifold fine = PoseManifold::Plate();
    fine.axes[PoseManifold::kAxisAzimuth] = PoseAxis::FreeRad(stat_az - 1.6e-6, stat_az + 1.6e-6, false);
    Family1DParams fp;
    fp.min_step = 1e-7;
    fp.max_step = 1e-7;
    const CausticTraceResult res = TraceFamily1D(f.normals, slots35, 2, n, sun20, fine, nullptr, fp);
    ASSERT_EQ(res.curves.size(), 1u);
    int stat_cnt = 0;
    for (const CurveEvent& ev : res.curves[0].events) {
      if (ev.kind == CurveEventKind::kStationaryPoint) {
        stat_cnt++;
      }
    }
    EXPECT_EQ(stat_cnt, 1) << "Fine 1D step suppressed parhelion kStationaryPoint";
  }

  // 9l. Partial-chain internal incidence failure must NOT be mislabeled as a Snell failure:
  //     Column {3, 1, 6} at (az = 0 deg, roll = 0 deg), sun 20 deg fails at internal face margin 2.
  {
    int slots316[3];
    const int faces316[3] = { 3, 1, 6 };
    f.Resolve(faces316, 3, slots316);
    double sun20[3];
    AltAzDegToSky(20.0, 0.0, sun20);
    const double inc[3] = { -sun20[0], -sun20[1], -sun20[2] };
    double pose[9];
    EulerPoseDeg(0.0, 90.0, 0.0, pose);
    double out_dir[3];
    ChainDomain dom;
    double mbuf[kMaxDomainMargins]{};
    dom.margins = mbuf;
    const bool ok = TracePathChain<double>(f.normals, slots316, 3, n, inc, pose, out_dir, nullptr, &dom);
    EXPECT_FALSE(ok);
    const OpticalMarginVerdict v = ClassifyChainMargins(ok, dom, 3);
    EXPECT_FALSE(v.valid);
    EXPECT_EQ(v.failing_index, 2);
    EXPECT_FALSE(v.failing_is_snell) << "Internal incidence failure was misclassified as Snell failure";
  }

  // 9m. Sun-azimuth rotation invariance on C16 Column {3, 5} at sun (alt=20 deg, az=5 deg):
  //     cliff-adjacent mixed-cell refinement must recover all 4 optical endpoints and both lower
  //     cliff tails, including (az=163.122062 deg, roll=135.482907 deg).
  {
    int slots35[2];
    const int faces35[2] = { 3, 5 };
    f.Resolve(faces35, 2, slots35);
    double sun_rot[3];
    AltAzDegToSky(20.0, 5.0, sun_rot);
    const CausticTraceResult res = TraceCausticLocus(f.normals, slots35, 2, n, sun_rot, PoseManifold::Column());
    EXPECT_EQ(res.generic_rank, 2);

    int total_opt_bnds = 0;
    double min_dist_target_deg = 180.0;
    const double target_az = 163.122062;
    const double target_roll = 135.482907;
    for (const PoseCurve& c : res.curves) {
      for (const CurveEvent& ev : c.events) {
        if (ev.kind == CurveEventKind::kOpticalBoundary) {
          total_opt_bnds++;
        }
      }
      for (size_t i = 0; i + 1 < c.nodes.size(); i++) {
        const double a_az = c.nodes[i].param[PoseManifold::kAxisAzimuth] * kRadToDeg;
        const double a_r = c.nodes[i].param[PoseManifold::kAxisRoll] * kRadToDeg;
        const double b_az = c.nodes[i + 1].param[PoseManifold::kAxisAzimuth] * kRadToDeg;
        const double b_r = c.nodes[i + 1].param[PoseManifold::kAxisRoll] * kRadToDeg;
        const double sx = AngleDiffDeg(b_az, a_az);
        const double sy = AngleDiffDeg(b_r, a_r);
        const double px = AngleDiffDeg(target_az, a_az);
        const double py = AngleDiffDeg(target_roll, a_r);
        const double s2 = sx * sx + sy * sy;
        const double t = (s2 > 0.0) ? std::clamp((px * sx + py * sy) / s2, 0.0, 1.0) : 0.0;
        min_dist_target_deg = std::min(min_dist_target_deg, std::hypot(px - t * sx, py - t * sy));
      }
    }
    EXPECT_GE(total_opt_bnds, 4) << "Sun azimuth 5 deg lost C16 lower optical endpoints";
    EXPECT_LT(min_dist_target_deg, 1.0) << "Sun azimuth 5 deg missed lower C16 tail pose";
  }

  // 9n. Cliff-march corridor cutoff: C16 Column {3, 5} with prism heights 0.4461242675781 and
  //     0.4461253675781 has positive A_P at the penultimate nodes and empty A_P at both optical
  //     endpoints of the upper tangent arc -> must emit kCorridorCutoff on true segment splits
  //     near both ends, match direct Corridor::Evaluate on each side, and keep raw |J| <= cap.
  for (double short_h : { 0.4461242675781, 0.4461253675781 }) {
    const PrismFixture f_short(short_h);
    int slots35[2];
    const int faces35[2] = { 3, 5 };
    f_short.Resolve(faces35, 2, slots35);
    double sun20[3];
    AltAzDegToSky(20.0, 0.0, sun20);
    Corridor corr(f_short.normals, f_short.polygons, slots35, 2);
    const CausticTraceResult res =
        TraceCausticLocus(f_short.normals, slots35, 2, n, sun20, PoseManifold::Column(), &corr);
    bool found_upper_with_cutoffs = false;
    for (const PoseCurve& c : res.curves) {
      double max_alt = -90.0;
      for (const PoseCurveNode& nd : c.nodes) {
        double alt = 0.0;
        double az = 0.0;
        SkyToAltAzDeg(nd.sky, &alt, &az);
        max_alt = std::max(max_alt, alt);
      }
      if (max_alt > 40.0) {
        int cutoffs = 0;
        for (const CurveEvent& ev : c.events) {
          if (ev.kind == CurveEventKind::kCorridorCutoff) {
            cutoffs++;
            // Cutoff must sit on an actual segment boundary separating open and closed sides.
            bool at_seg_split = false;
            for (size_t s = 0; s + 1 < c.segments.size(); s++) {
              const double s_split = c.nodes[static_cast<size_t>(c.segments[s].end_idx)].arclength;
              if (std::fabs(s_split - ev.arclength) < 1e-7 &&
                  c.segments[s].corridor_open != c.segments[s + 1].corridor_open) {
                at_seg_split = true;
                break;
              }
            }
            EXPECT_TRUE(at_seg_split) << "Cliff kCorridorCutoff at h=" << short_h << " s=" << ev.arclength
                                      << " did not split segment corridor_open states";
          }
        }
        EXPECT_GE(cutoffs, 2) << "Upper arc cliff tails missed kCorridorCutoff events at h=" << short_h;
        if (c.segments.size() < 3u) {
          ADD_FAILURE() << "Expected open middle segment flanked by closed cliff segments at h=" << short_h;
          continue;
        }
        EXPECT_FALSE(c.segments.front().corridor_open);
        EXPECT_TRUE(c.segments[1].corridor_open);
        EXPECT_FALSE(c.segments.back().corridor_open);
        // Verify direct Corridor::Evaluate agrees with segment corridor_open on both sides of the
        // front and back cliff splits (midpoint of each terminal segment and interior of open seg).
        auto eval_open_at_pose = [&](const double pose[9]) {
          const double inc[3] = { -sun20[0], -sun20[1], -sun20[2] };
          double s_body[3];
          WorldToBody(pose, inc, s_body);
          const EntryMeasure em = corr.Evaluate(s_body, n);
          return em.status == EntryMeasureStatus::kOk && em.value > 0.0;
        };
        EXPECT_FALSE(eval_open_at_pose(c.nodes.front().pose));
        EXPECT_FALSE(eval_open_at_pose(c.nodes.back().pose));
        const int mid_open_idx = (c.segments[1].start_idx + c.segments[1].end_idx) / 2;
        EXPECT_TRUE(eval_open_at_pose(c.nodes[static_cast<size_t>(mid_open_idx)].pose));
        // Verify raw |J| <= kRawInteriorJCap on every interior node including cliff cutoff splits.
        for (size_t i = 1; i + 1 < c.nodes.size(); i++) {
          double j_ind = 0.0;
          if (!IndependentSignedJacobian2D(f_short.normals, slots35, 2, n, sun20, PoseManifold::Column(),
                                           c.nodes[i].param, &j_ind)) {
            ADD_FAILURE() << "Independent evaluation invalid at short-prism node " << i;
            continue;
          }
          EXPECT_LE(std::fabs(j_ind), kRawInteriorJCap)
              << "Short-prism (h=" << short_h << ") interior node " << i << " |J|=" << j_ind;
        }
        found_upper_with_cutoffs = (cutoffs >= 2);
      }
    }
    EXPECT_TRUE(found_upper_with_cutoffs) << "Missing upper arc cutoffs at h=" << short_h;
  }

  // 9o. Compact-bounds Lowitz cusp preservation: Lowitz {3, 5} at 20 deg bounded to +-0.02 deg
  //     around the genuine cusp (197.81628321, 13.831146705) deg must emit kCusp.
  {
    int slots35[2];
    const int faces35[2] = { 3, 5 };
    f.Resolve(faces35, 2, slots35);
    double sun20[3];
    AltAzDegToSky(20.0, 0.0, sun20);
    PoseManifold compact = PoseManifold::Lowitz(0.0);
    compact.axes[PoseManifold::kAxisAzimuth] = PoseAxis::FreeDeg(197.81628321 - 0.02, 197.81628321 + 0.02, false);
    compact.axes[PoseManifold::kAxisZenith] = PoseAxis::FreeDeg(13.831146705 - 0.02, 13.831146705 + 0.02, false);
    const CausticTraceResult res = TraceCausticLocus(f.normals, slots35, 2, n, sun20, compact);
    ASSERT_FALSE(res.curves.empty());
    int cusp_cnt = 0;
    for (const PoseCurve& c : res.curves) {
      for (const CurveEvent& ev : c.events) {
        if (ev.kind == CurveEventKind::kCusp) {
          cusp_cnt++;
        }
      }
    }
    EXPECT_EQ(cusp_cnt, 1) << "Compact +-0.02 deg bounds suppressed a genuine Lowitz cusp";
  }

  // 9p. Narrow valid 1D branch between invalid scan samples: face {2}, zenith 20.000005 deg,
  //     roll 0 deg, sun (20 deg, 0.6 deg) at default 360 scan samples must recover [180.558, 180.642] deg.
  {
    int slots2[1];
    const int faces2[1] = { 2 };
    f.Resolve(faces2, 1, slots2);
    double sun_narrow[3];
    AltAzDegToSky(20.0, 0.6, sun_narrow);
    PoseManifold m;
    m.axes[PoseManifold::kAxisAzimuth] = PoseAxis::FreeRad(0.0, 2.0 * kPi, true);
    m.axes[PoseManifold::kAxisZenith] = PoseAxis::PinnedDeg(20.000005);
    m.axes[PoseManifold::kAxisRoll] = PoseAxis::PinnedDeg(0.0);
    const CausticTraceResult res = TraceFamily1D(f.normals, slots2, 1, n, sun_narrow, m);
    ASSERT_EQ(res.curves.size(), 1u);
    EXPECT_FALSE(res.curves[0].coverage.closed);
    const double az_lo = res.curves[0].nodes.front().param[PoseManifold::kAxisAzimuth] * kRadToDeg;
    const double az_hi = res.curves[0].nodes.back().param[PoseManifold::kAxisAzimuth] * kRadToDeg;
    EXPECT_NEAR(az_lo, 180.558, 0.01);
    EXPECT_NEAR(az_hi, 180.642, 0.01);
    int opt_bnds = 0;
    for (const CurveEvent& ev : res.curves[0].events) {
      if (ev.kind == CurveEventKind::kOpticalBoundary) {
        opt_bnds++;
      }
    }
    EXPECT_EQ(opt_bnds, 2);
  }

  // 9q. Rank-one collapsed node sigma1 must match independent 2D manifold Jet<2> SVD even when
  //     the representative roll slides (Column {3, 1, 6} at 20 deg).
  {
    int slots316[3];
    const int faces316[3] = { 3, 1, 6 };
    f.Resolve(faces316, 3, slots316);
    double sun20[3];
    AltAzDegToSky(20.0, 0.0, sun20);
    const PoseManifold column = PoseManifold::Column();
    const CausticTraceResult res = TraceCausticLocus(f.normals, slots316, 3, n, sun20, column);
    EXPECT_EQ(res.generic_rank, 1);
    ASSERT_FALSE(res.curves.empty());
    for (const PoseCurve& c : res.curves) {
      for (size_t i = 0; i < c.nodes.size(); i += std::max<size_t>(1, c.nodes.size() / 20)) {
        const PoseCurveNode& nd = c.nodes[i];
        double j_ind = 0.0;
        double s1_ind = 0.0;
        double s2_ind = 0.0;
        if (!IndependentSignedJacobian2D(f.normals, slots316, 3, n, sun20, column, nd.param, &j_ind, &s1_ind,
                                         &s2_ind)) {
          ADD_FAILURE() << "Independent Jet<2> invalid on rank-1 node " << i;
          continue;
        }
        EXPECT_NEAR(nd.sigma1, s1_ind, 1e-6) << "Rank-1 collapsed node sigma1 deviated from 2D manifold SVD";
        EXPECT_NEAR(nd.sigma2, s2_ind, 1e-6);
      }
    }
  }

  // 9r. Collapsed rank-one continuation must obey params.max_steps_per_branch and report truncation.
  {
    int slots316[3];
    const int faces316[3] = { 3, 1, 6 };
    f.Resolve(faces316, 3, slots316);
    double sun20[3];
    AltAzDegToSky(20.0, 0.0, sun20);
    CausticContinuationParams p_one;
    p_one.max_steps_per_branch = 1;
    const CausticTraceResult res =
        TraceCausticLocus(f.normals, slots316, 3, n, sun20, PoseManifold::Column(), nullptr, p_one);
    EXPECT_EQ(res.generic_rank, 1);
    ASSERT_FALSE(res.curves.empty());
    EXPECT_TRUE(res.coverage.budget_truncated);
    for (const PoseCurve& c : res.curves) {
      EXPECT_LE(c.nodes.size(), 3u) << "Collapsed branch ignored max_steps_per_branch=1";
      EXPECT_TRUE(c.coverage.budget_truncated);
    }
  }

  // 9s. Rank-one segment/interval deduplication: Column face {2} at 20 deg, az [120, 240] deg,
  //     roll [0, 1] deg must emit 1 curve (no replay) and max_curves=1 must NOT falsely truncate.
  {
    int slots2[1];
    const int faces2[1] = { 2 };
    f.Resolve(faces2, 1, slots2);
    double sun20[3];
    AltAzDegToSky(20.0, 0.0, sun20);
    PoseManifold m;
    m.axes[PoseManifold::kAxisAzimuth] = PoseAxis::FreeDeg(120.0, 240.0, false);
    m.axes[PoseManifold::kAxisZenith] = PoseAxis::PinnedDeg(90.0);
    m.axes[PoseManifold::kAxisRoll] = PoseAxis::FreeDeg(0.0, 1.0, false);

    const CausticTraceResult r_def = TraceCausticLocus(f.normals, slots2, 1, n, sun20, m);
    EXPECT_EQ(r_def.generic_rank, 1);
    EXPECT_EQ(r_def.curves.size(), 1u);
    EXPECT_FALSE(r_def.coverage.budget_truncated);

    CausticContinuationParams p1;
    p1.max_curves = 1;
    const CausticTraceResult r_one = TraceCausticLocus(f.normals, slots2, 1, n, sun20, m, nullptr, p1);
    EXPECT_EQ(r_one.curves.size(), 1u);
    EXPECT_FALSE(r_one.coverage.budget_truncated);
  }

  // 9t. Periodic span validation: a compact angular interval marked periodic (span != 2*pi) must
  //     be rejected by PoseManifold::IsValid() before tracing.
  {
    PoseManifold bad_per = PoseManifold::Plate();
    bad_per.axes[PoseManifold::kAxisAzimuth] = PoseAxis::FreeDeg(100.0, 120.0, true);
    EXPECT_FALSE(bad_per.IsValid());

    int slots1[1];
    const int faces1[1] = { 1 };
    f.Resolve(faces1, 1, slots1);
    double sun20[3];
    AltAzDegToSky(20.0, 0.0, sun20);
    const CausticTraceResult res = TraceFamily1D(f.normals, slots1, 1, n, sun20, bad_per);
    EXPECT_TRUE(res.curves.empty());
  }

  // 9u. Double internal TIR onset inside a single 1D step: Parry {3, 1, 6}, roll 0 deg,
  //     sun altitude 32.027351 deg has internal_disc[1] crossing - -> + -> - inside [178.8, 181.7] deg.
  {
    int slots316[3];
    const int faces316[3] = { 3, 1, 6 };
    f.Resolve(faces316, 3, slots316);
    double sun_tir[3];
    AltAzDegToSky(32.027351, 0.0, sun_tir);
    const CausticTraceResult res = TraceFamily1D(f.normals, slots316, 3, n, sun_tir, PoseManifold::Parry(0.0));
    int tir_in_window = 0;
    for (const PoseCurve& c : res.curves) {
      for (const CurveEvent& ev : c.events) {
        if (ev.kind == CurveEventKind::kTirOnset && ev.detail_index == 1) {
          const double az_d = WrapAngleDeg(ev.param[PoseManifold::kAxisAzimuth] * kRadToDeg);
          if (az_d > 178.0 && az_d < 182.0) {
            tir_in_window++;
          }
        }
      }
    }
    EXPECT_GE(tir_in_window, 2) << "1D step skipped double internal TIR onset";
  }

  // 9v. Double corridor cutoff inside a single 1D step: Parry {3, 5}, sun 20 deg,
  //     prism height 0.00572556886455 opens and closes inside [268.9, 271.8] deg.
  {
    const PrismFixture f_thin(0.00572556886455);
    int slots35[2];
    const int faces35[2] = { 3, 5 };
    f_thin.Resolve(faces35, 2, slots35);
    double sun20[3];
    AltAzDegToSky(20.0, 0.0, sun20);
    Corridor corr(f_thin.normals, f_thin.polygons, slots35, 2);
    const CausticTraceResult res = TraceFamily1D(f_thin.normals, slots35, 2, n, sun20, PoseManifold::Parry(0.0), &corr);
    int cutoffs = 0;
    int open_segs = 0;
    for (const PoseCurve& c : res.curves) {
      for (const CurveEvent& ev : c.events) {
        if (ev.kind == CurveEventKind::kCorridorCutoff) {
          cutoffs++;
        }
      }
      for (const CurveSegment& seg : c.segments) {
        if (seg.corridor_open) {
          open_segs++;
        }
      }
    }
    EXPECT_GE(cutoffs, 2) << "1D step skipped double corridor cutoff";
    EXPECT_GE(open_segs, 1) << "1D step failed to split narrow open corridor segment";
  }

  // 9w. Euler-pole collapsed curve must report 2D manifold Jacobian sigma1 (not 1D slice speed):
  //     TiltedSpin(0 deg) on {3, 5}, sun 20 deg.
  {
    int slots35[2];
    const int faces35[2] = { 3, 5 };
    f.Resolve(faces35, 2, slots35);
    double sun20[3];
    AltAzDegToSky(20.0, 0.0, sun20);
    const PoseManifold pole = PoseManifold::TiltedSpin(0.0);
    const CausticTraceResult res = TraceCausticLocus(f.normals, slots35, 2, n, sun20, pole);
    EXPECT_EQ(res.generic_rank, 1);
    ASSERT_FALSE(res.curves.empty());
    for (const PoseCurve& c : res.curves) {
      for (size_t i = 0; i < c.nodes.size(); i += std::max<size_t>(1, c.nodes.size() / 12)) {
        double j_ind = 0.0;
        double s1_ind = 0.0;
        double s2_ind = 0.0;
        if (!IndependentSignedJacobian2D(f.normals, slots35, 2, n, sun20, pole, c.nodes[i].param, &j_ind, &s1_ind,
                                         &s2_ind)) {
          ADD_FAILURE() << "Independent Jet<2> invalid at pole node " << i;
          continue;
        }
        EXPECT_NEAR(c.nodes[i].sigma1, s1_ind, 1e-5) << "Euler-pole node reported 1D speed instead of 2D sigma1";
        EXPECT_NEAR(c.nodes[i].sigma2, s2_ind, 1e-5);
      }
    }
  }

  // 9x. Bounded Euler pole with total spin > 360 deg must NOT invent a kDomainBoundary strictly
  //     inside both axis bounds: zenith=0, az=[140, 350] deg, roll=[80, 280] deg, {3, 5}, sun 20 deg.
  {
    int slots35[2];
    const int faces35[2] = { 3, 5 };
    f.Resolve(faces35, 2, slots35);
    double sun20[3];
    AltAzDegToSky(20.0, 0.0, sun20);
    PoseManifold m;
    m.axes[PoseManifold::kAxisAzimuth] = PoseAxis::FreeDeg(140.0, 350.0, false);
    m.axes[PoseManifold::kAxisZenith] = PoseAxis::PinnedDeg(0.0);
    m.axes[PoseManifold::kAxisRoll] = PoseAxis::FreeDeg(80.0, 280.0, false);
    const CausticTraceResult res = TraceCausticLocus(f.normals, slots35, 2, n, sun20, m);
    EXPECT_EQ(res.generic_rank, 1);
    ASSERT_FALSE(res.curves.empty());
    for (const PoseCurve& c : res.curves) {
      for (const CurveEvent& ev : c.events) {
        if (ev.kind == CurveEventKind::kDomainBoundary && ev.detail_index == PoseManifold::kAxisAzimuth) {
          const double az_d = ev.param[PoseManifold::kAxisAzimuth] * kRadToDeg;
          const bool at_bound = (std::fabs(az_d - 140.0) < 1e-3) || (std::fabs(az_d - 350.0) < 1e-3);
          EXPECT_TRUE(at_bound) << "Bounded pole emitted interior kDomainBoundary(az) at az=" << az_d;
        }
      }
    }
  }

  // 9y. Ultra-tight compact Lowitz cusp (+-0.00020 deg around (197.81628321, 13.831146705) deg)
  //     must still emit kCusp while C16 mirror keeps zero cusps even at residual_tol=1e-16.
  {
    int slots35[2];
    const int faces35[2] = { 3, 5 };
    f.Resolve(faces35, 2, slots35);
    double sun20[3];
    AltAzDegToSky(20.0, 0.0, sun20);
    PoseManifold tight = PoseManifold::Lowitz(0.0);
    tight.axes[PoseManifold::kAxisAzimuth] = PoseAxis::FreeDeg(197.81628321 - 0.00020, 197.81628321 + 0.00020, false);
    tight.axes[PoseManifold::kAxisZenith] = PoseAxis::FreeDeg(13.831146705 - 0.00020, 13.831146705 + 0.00020, false);
    const CausticTraceResult res = TraceCausticLocus(f.normals, slots35, 2, n, sun20, tight);
    ASSERT_FALSE(res.curves.empty());
    int cusp_cnt = 0;
    for (const PoseCurve& c : res.curves) {
      for (const CurveEvent& ev : c.events) {
        if (ev.kind == CurveEventKind::kCusp) {
          cusp_cnt++;
        }
      }
    }
    EXPECT_EQ(cusp_cnt, 1) << "Tight +-0.00020 deg bounds suppressed genuine Lowitz cusp";

    // Also verify C16 mirror emits zero cusps at residual_tol = 1e-16.
    CausticContinuationParams p_tight;
    p_tight.residual_tol = 1e-16;
    const CausticTraceResult c16_tight =
        TraceCausticLocus(f.normals, slots35, 2, n, sun20, PoseManifold::Column(), nullptr, p_tight);
    for (const PoseCurve& c : c16_tight.curves) {
      for (const CurveSegment& seg : c.segments) {
        if (seg.kind == CurveSegmentKind::kPointImage) {
          const double s0 = c.nodes[static_cast<size_t>(seg.start_idx)].arclength;
          const double s1 = c.nodes[static_cast<size_t>(seg.end_idx)].arclength;
          for (const CurveEvent& ev : c.events) {
            if (ev.kind == CurveEventKind::kCusp) {
              EXPECT_FALSE(ev.arclength >= s0 - 1e-9 && ev.arclength <= s1 + 1e-9)
                  << "C16 mirror emitted spurious cusp at residual_tol=1e-16";
            }
          }
        }
      }
    }
  }

  // 9z. 1D corridor-split node budget enforcement & single-node closed corridor state:
  //     Parry {3, 1, 6}, height 0.8, sun 20 deg.
  {
    const PrismFixture f08(0.8);
    int slots316[3];
    const int faces316[3] = { 3, 1, 6 };
    f08.Resolve(faces316, 3, slots316);
    double sun20[3];
    AltAzDegToSky(20.0, 0.0, sun20);
    Corridor corr(f08.normals, f08.polygons, slots316, 3);

    Family1DParams p21;
    p21.max_nodes_per_branch = 21;
    const CausticTraceResult r21 =
        TraceFamily1D(f08.normals, slots316, 3, n, sun20, PoseManifold::Parry(0.0), &corr, p21);
    ASSERT_FALSE(r21.curves.empty());
    for (const PoseCurve& c : r21.curves) {
      EXPECT_LE(c.nodes.size(), 21u) << "Corridor split exceeded max_nodes_per_branch=21";
    }

    Family1DParams p1;
    p1.max_nodes_per_branch = 1;
    const CausticTraceResult r1 =
        TraceFamily1D(f08.normals, slots316, 3, n, sun20, PoseManifold::Parry(0.0), &corr, p1);
    ASSERT_FALSE(r1.curves.empty());
    bool saw_closed_single = false;
    for (const PoseCurve& c : r1.curves) {
      EXPECT_EQ(c.nodes.size(), 1u);
      if (c.segments.size() == 1u) {
        double s_body[3];
        WorldToBody(c.nodes[0].pose, sun20, s_body);
        s_body[0] = -s_body[0];
        s_body[1] = -s_body[1];
        s_body[2] = -s_body[2];
        const EntryMeasure em = corr.Evaluate(s_body, n);
        const bool expected_open = (em.status == EntryMeasureStatus::kOk && em.value > 0.0);
        EXPECT_EQ(c.segments[0].corridor_open, expected_open);
        if (!expected_open) {
          saw_closed_single = true;
        }
      }
    }
    EXPECT_TRUE(saw_closed_single);
  }

  // 9aa. Continuous Corridor::OverlapMargin + ultra-narrow 1D double corridor cutoff:
  //      Parry {3, 5}, roll 0 deg, sun 20 deg, prism height 0.0001 has A_P = 0 at 269.90 and
  //      270.10 deg and A_P > 0 at 270.00 deg inside a single default step.
  {
    const PrismFixture f_ultra(0.0001);
    int slots35[2];
    const int faces35[2] = { 3, 5 };
    f_ultra.Resolve(faces35, 2, slots35);
    double sun20[3];
    AltAzDegToSky(20.0, 0.0, sun20);
    Corridor corr(f_ultra.normals, f_ultra.polygons, slots35, 2);

    auto eval_margin_at = [&](double az_deg) {
      double pose[9];
      EulerPoseDeg(az_deg, 90.0, 0.0, pose);
      const double inc[3] = { -sun20[0], -sun20[1], -sun20[2] };
      double s_body[3];
      WorldToBody(pose, inc, s_body);
      return corr.OverlapMargin(s_body, n);
    };
    EXPECT_LT(eval_margin_at(269.90), 0.0);
    EXPECT_GT(eval_margin_at(270.00), 0.0);
    EXPECT_LT(eval_margin_at(270.10), 0.0);

    // One-face external reflection OverlapMargin: positive when cos_i > 0, NaN on backface.
    int slots1[1];
    const int faces1[1] = { 1 };
    f_ultra.Resolve(faces1, 1, slots1);
    Corridor corr_ext(f_ultra.normals, f_ultra.polygons, slots1, 1);
    const double s_down[3] = { 0.0, 0.0, -1.0 };
    const double s_up[3] = { 0.0, 0.0, 1.0 };
    EXPECT_GT(corr_ext.OverlapMargin(s_down, n), 0.5);
    EXPECT_TRUE(std::isnan(corr_ext.OverlapMargin(s_up, n)));

    const CausticTraceResult res =
        TraceFamily1D(f_ultra.normals, slots35, 2, n, sun20, PoseManifold::Parry(0.0), &corr);
    int cutoffs_near_270 = 0;
    int open_segs_near_270 = 0;
    for (const PoseCurve& c : res.curves) {
      for (const CurveEvent& ev : c.events) {
        if (ev.kind == CurveEventKind::kCorridorCutoff) {
          const double az_d = WrapAngleDeg(ev.param[PoseManifold::kAxisAzimuth] * kRadToDeg);
          if (std::fabs(AngleDiffDeg(az_d, 270.0)) < 1.0) {
            cutoffs_near_270++;
          }
        }
      }
      for (const CurveSegment& seg : c.segments) {
        if (seg.corridor_open) {
          const double az_lo = WrapAngleDeg(c.nodes[static_cast<size_t>(seg.start_idx)].param[0] * kRadToDeg);
          const double az_hi = WrapAngleDeg(c.nodes[static_cast<size_t>(seg.end_idx)].param[0] * kRadToDeg);
          if (std::fabs(AngleDiffDeg(az_lo, 270.0)) < 1.0 && std::fabs(AngleDiffDeg(az_hi, 270.0)) < 1.0) {
            open_segs_near_270++;
          }
        }
      }
    }
    EXPECT_GE(cutoffs_near_270, 2) << "1D continuous-margin probe missed ultra-narrow corridor cutoffs";
    EXPECT_GE(open_segs_near_270, 1) << "1D continuous-margin probe failed to split ultra-narrow open segment";
  }

  // 9ab. 2D rank-2 double corridor cutoff inside one accepted step: Column {3, 5}, sun 20 deg,
  //      prism height 0.0034822892640643 opens around (az=269.94966 deg, roll=330.95808 deg)
  //      inside a closed-to-closed step.
  {
    const PrismFixture f_2d_thin(0.0034822892640643);
    int slots35[2];
    const int faces35[2] = { 3, 5 };
    f_2d_thin.Resolve(faces35, 2, slots35);
    double sun20[3];
    AltAzDegToSky(20.0, 0.0, sun20);
    Corridor corr(f_2d_thin.normals, f_2d_thin.polygons, slots35, 2);
    const CausticTraceResult res =
        TraceCausticLocus(f_2d_thin.normals, slots35, 2, n, sun20, PoseManifold::Column(), &corr);
    EXPECT_EQ(res.generic_rank, 2);
    int cutoffs_near_target = 0;
    bool found_open_near_target = false;
    for (const PoseCurve& c : res.curves) {
      for (const CurveEvent& ev : c.events) {
        if (ev.kind == CurveEventKind::kCorridorCutoff) {
          const double az_d = WrapAngleDeg(ev.param[PoseManifold::kAxisAzimuth] * kRadToDeg);
          const double r_d = WrapAngleDeg(ev.param[PoseManifold::kAxisRoll] * kRadToDeg);
          if (std::fabs(AngleDiffDeg(az_d, 269.95)) < 2.0 && std::fabs(AngleDiffDeg(r_d, 330.96)) < 5.0) {
            cutoffs_near_target++;
          }
        }
      }
      for (const CurveSegment& seg : c.segments) {
        if (!seg.corridor_open) {
          continue;
        }
        for (int idx = seg.start_idx; idx <= seg.end_idx; idx++) {
          const double az_d = WrapAngleDeg(c.nodes[static_cast<size_t>(idx)].param[0] * kRadToDeg);
          const double r_d = WrapAngleDeg(c.nodes[static_cast<size_t>(idx)].param[2] * kRadToDeg);
          if (std::fabs(AngleDiffDeg(az_d, 269.95)) < 2.0 && std::fabs(AngleDiffDeg(r_d, 330.96)) < 5.0) {
            found_open_near_target = true;
          }
        }
      }
    }
    EXPECT_GE(cutoffs_near_target, 2) << "2D rank-2 step skipped double corridor cutoff";
    EXPECT_TRUE(found_open_near_target) << "2D rank-2 step failed to split narrow open corridor segment";
  }

  // 9ac. 2D rank-1 collapsed fiber double TIR onset inside one step: {3, 1, 6}, zenith=90 deg,
  //      roll in [-0.2, 0.2] deg, sun altitude 32.027351 deg exercises the unified TIR detector on
  //      TraceContinuousRank1AxisAligned.
  {
    int slots316[3];
    const int faces316[3] = { 3, 1, 6 };
    f.Resolve(faces316, 3, slots316);
    double sun_tir[3];
    AltAzDegToSky(32.027351, 0.0, sun_tir);
    PoseManifold m;
    m.axes[PoseManifold::kAxisAzimuth] = PoseAxis::FreeRad(0.0, 2.0 * kPi, true);
    m.axes[PoseManifold::kAxisZenith] = PoseAxis::PinnedDeg(90.0);
    m.axes[PoseManifold::kAxisRoll] = PoseAxis::FreeDeg(-0.2, 0.2, false);
    const CausticTraceResult res = TraceCausticLocus(f.normals, slots316, 3, n, sun_tir, m);
    EXPECT_EQ(res.generic_rank, 1);
    int tir_in_window = 0;
    for (const PoseCurve& c : res.curves) {
      for (const CurveEvent& ev : c.events) {
        if (ev.kind == CurveEventKind::kTirOnset && ev.detail_index == 1) {
          const double az_d = WrapAngleDeg(ev.param[PoseManifold::kAxisAzimuth] * kRadToDeg);
          if (az_d > 178.0 && az_d < 182.0) {
            tir_in_window++;
          }
        }
      }
    }
    EXPECT_GE(tir_in_window, 2) << "Rank-1 collapsed step skipped double internal TIR onset";
  }

  // 9ad. Node-budget truncation must never leave events past curve.nodes.back().arclength:
  //      Parry {3, 1, 6}, roll 0 deg, sun 20 deg, height 0.4, max_nodes_per_branch = 25.
  {
    const PrismFixture f04(0.4);
    int slots316[3];
    const int faces316[3] = { 3, 1, 6 };
    f04.Resolve(faces316, 3, slots316);
    double sun20[3];
    AltAzDegToSky(20.0, 0.0, sun20);
    Corridor corr(f04.normals, f04.polygons, slots316, 3);
    Family1DParams p25;
    p25.max_nodes_per_branch = 25;
    const CausticTraceResult res =
        TraceFamily1D(f04.normals, slots316, 3, n, sun20, PoseManifold::Parry(0.0), &corr, p25);
    ASSERT_FALSE(res.curves.empty());
    for (const PoseCurve& c : res.curves) {
      EXPECT_LE(c.nodes.size(), 25u);
      const double max_s = c.nodes.back().arclength;
      for (const CurveEvent& ev : c.events) {
        EXPECT_GE(ev.arclength, -1e-12);
        EXPECT_LE(ev.arclength, max_s + 1e-12)
            << "Truncated branch emitted event at s=" << ev.arclength << " past final node s=" << max_s;
      }
    }
  }

  // 9ae. Basal-reflection {1} grazing-gap fixture (zenith=20.000005 deg, roll=0 deg,
  //      sun=(20 deg, 0.6 deg)): (a) with unit-height prism corridor, the clipped step entering
  //      the optical gap must still emit its kCorridorCutoff (~180.50-180.51 deg) and close the
  //      terminal segment through 180.557778 deg alongside the right-side cutoff (~180.698 deg);
  //      (b) without corridor and max_nodes_per_branch=70, periodic seam stitching must enforce
  //      the per-branch node budget and retain only segments/events within the returned arclength.
  {
    int slots1[1];
    const int faces1[1] = { 1 };
    f.Resolve(faces1, 1, slots1);
    double sun_gap[3];
    AltAzDegToSky(20.0, 0.6, sun_gap);
    PoseManifold m;
    m.axes[PoseManifold::kAxisAzimuth] = PoseAxis::FreeRad(0.0, 2.0 * kPi, true);
    m.axes[PoseManifold::kAxisZenith] = PoseAxis::PinnedDeg(20.000005);
    m.axes[PoseManifold::kAxisRoll] = PoseAxis::PinnedDeg(0.0);

    // (a) Corridor cutoffs on both sides of the grazing gap.
    Corridor corr(f.normals, f.polygons, slots1, 1);
    const CausticTraceResult res_corr = TraceFamily1D(f.normals, slots1, 1, n, sun_gap, m, &corr);
    ASSERT_EQ(res_corr.curves.size(), 1u);
    const PoseCurve& c_corr = res_corr.curves[0];
    EXPECT_FALSE(c_corr.coverage.closed);
    int left_cutoffs = 0;
    int right_cutoffs = 0;
    for (const CurveEvent& ev : c_corr.events) {
      if (ev.kind == CurveEventKind::kCorridorCutoff) {
        const double az_d = WrapAngleDeg(ev.param[PoseManifold::kAxisAzimuth] * kRadToDeg);
        if (az_d > 180.4 && az_d < 180.56) {
          left_cutoffs++;
        } else if (az_d > 180.64 && az_d < 180.8) {
          right_cutoffs++;
        }
      }
    }
    EXPECT_GE(left_cutoffs, 1) << "Missing corridor cutoff on the clipped left side of the grazing gap";
    EXPECT_GE(right_cutoffs, 1) << "Missing corridor cutoff on the right side of the grazing gap";
    ASSERT_GE(c_corr.segments.size(), 3u);
    EXPECT_FALSE(c_corr.segments.front().corridor_open);
    EXPECT_FALSE(c_corr.segments.back().corridor_open);

    // (b) Periodic seam stitching honors max_nodes_per_branch = 70.
    Family1DParams p70;
    p70.max_nodes_per_branch = 70;
    const CausticTraceResult res_cap = TraceFamily1D(f.normals, slots1, 1, n, sun_gap, m, nullptr, p70);
    ASSERT_EQ(res_cap.curves.size(), 1u);
    const PoseCurve& c_cap = res_cap.curves[0];
    EXPECT_LE(c_cap.nodes.size(), 70u);
    EXPECT_TRUE(c_cap.coverage.budget_truncated);
    EXPECT_TRUE(res_cap.coverage.budget_truncated);
    const double max_s = c_cap.nodes.back().arclength;
    for (const CurveEvent& ev : c_cap.events) {
      EXPECT_GE(ev.arclength, -1e-12);
      EXPECT_LE(ev.arclength, max_s + 1e-12);
    }
    for (const CurveSegment& seg : c_cap.segments) {
      EXPECT_GE(seg.start_idx, 0);
      EXPECT_LT(seg.end_idx, static_cast<int>(c_cap.nodes.size()));
    }
  }

  // 9af. Valid ultra-fine 1D step (min_step = max_step = 1e-14) must advance and reach
  //      max_nodes_per_branch = 3 immediately without looping until an external deadline.
  {
    int slots1[1];
    const int faces1[1] = { 1 };
    f.Resolve(faces1, 1, slots1);
    double sun20[3];
    AltAzDegToSky(20.0, 0.0, sun20);
    PoseManifold m;
    m.axes[PoseManifold::kAxisAzimuth] = PoseAxis::FreeRad(0.0, 0.001, false);
    m.axes[PoseManifold::kAxisZenith] = PoseAxis::PinnedDeg(0.0);
    m.axes[PoseManifold::kAxisRoll] = PoseAxis::PinnedDeg(0.0);

    Family1DParams p_fine;
    p_fine.min_step = 1e-14;
    p_fine.max_step = 1e-14;
    p_fine.max_nodes_per_branch = 3;
    const CausticTraceResult res_fine = TraceFamily1D(f.normals, slots1, 1, n, sun20, m, nullptr, p_fine);
    EXPECT_FALSE(res_fine.stop_requested);
    ASSERT_EQ(res_fine.curves.size(), 1u);
    EXPECT_EQ(res_fine.curves[0].nodes.size(), 3u);
    EXPECT_TRUE(res_fine.curves[0].coverage.budget_truncated);
  }

  // 9ag. Seed deduplication in a compact 2D domain must not merge distinct parallel caustic
  //      branches separated by < 1e-3 rad: C16 Column 3-5, sun alt 20 deg, az [25.77, 25.83] deg,
  //      roll [120.02, 120.05] deg must recover BOTH branches reaching both roll bounds.
  {
    int slots35[2];
    const int faces35[2] = { 3, 5 };
    f.Resolve(faces35, 2, slots35);
    double sun20[3];
    AltAzDegToSky(20.0, 0.0, sun20);
    PoseManifold m;
    m.axes[PoseManifold::kAxisAzimuth] = PoseAxis::FreeDeg(25.77, 25.83, false);
    m.axes[PoseManifold::kAxisZenith] = PoseAxis::PinnedDeg(90.0);
    m.axes[PoseManifold::kAxisRoll] = PoseAxis::FreeDeg(120.02, 120.05, false);

    const CausticTraceResult res = TraceCausticLocus(f.normals, slots35, 2, n, sun20, m);
    ASSERT_GE(res.curves.size(), 2u) << "Compact-domain dedup merged two distinct parallel caustic branches";
    int branches_spanning_roll = 0;
    for (const PoseCurve& c : res.curves) {
      double min_roll = 1e9;
      double max_roll = -1e9;
      for (const PoseCurveNode& nd : c.nodes) {
        const double rd = nd.param[PoseManifold::kAxisRoll] * kRadToDeg;
        min_roll = std::min(min_roll, rd);
        max_roll = std::max(max_roll, rd);
      }
      if (min_roll <= 120.021 && max_roll >= 120.049) {
        branches_spanning_roll++;
      }
    }
    EXPECT_GE(branches_spanning_roll, 2) << "Both parallel branches must reach both roll bounds [120.02, 120.05] deg";

    // Setting dedup_tol = 0 must also preserve both branches without any hidden distance shortcut.
    CausticContinuationParams p_nodedup;
    p_nodedup.dedup_tol = 0.0;
    const CausticTraceResult res_nodedup = TraceCausticLocus(f.normals, slots35, 2, n, sun20, m, nullptr, p_nodedup);
    EXPECT_GE(res_nodedup.curves.size(), 2u);
  }

  // 9ah. Compact mirror branch around az [25.713648, 25.913648] deg, roll [119.9, 120.1] deg
  //      (C16 Column 3-5, sun alt 20 deg): the constant-image mirror branch must be segmented
  //      entirely as kPointImage and emit zero spurious kCusp events.
  {
    int slots35[2];
    const int faces35[2] = { 3, 5 };
    f.Resolve(faces35, 2, slots35);
    double sun20[3];
    AltAzDegToSky(20.0, 0.0, sun20);
    PoseManifold m;
    m.axes[PoseManifold::kAxisAzimuth] = PoseAxis::FreeDeg(25.713648, 25.913648, false);
    m.axes[PoseManifold::kAxisZenith] = PoseAxis::PinnedDeg(90.0);
    m.axes[PoseManifold::kAxisRoll] = PoseAxis::FreeDeg(119.9, 120.1, false);

    const CausticTraceResult res = TraceCausticLocus(f.normals, slots35, 2, n, sun20, m);
    ASSERT_FALSE(res.curves.empty());
    bool found_mirror_point_image = false;
    for (const PoseCurve& c : res.curves) {
      double max_sky_spread = 0.0;
      for (size_t i = 1; i < c.nodes.size(); i++) {
        max_sky_spread = std::max(max_sky_spread, so3::AngleBetween(c.nodes[0].sky, c.nodes[i].sky));
      }
      if (max_sky_spread < 1e-6) {
        for (const CurveEvent& ev : c.events) {
          EXPECT_NE(ev.kind, CurveEventKind::kCusp)
              << "Constant-image mirror branch emitted spurious kCusp at s=" << ev.arclength;
        }
        EXPECT_FALSE(c.segments.empty());
        for (const CurveSegment& seg : c.segments) {
          EXPECT_EQ(seg.kind, CurveSegmentKind::kPointImage)
              << "Constant-image mirror branch must be segmented entirely as kPointImage";
        }
        found_mirror_point_image = true;
      }
    }
    EXPECT_TRUE(found_mirror_point_image);
  }
}

}  // namespace
}  // namespace lumice::analytic
