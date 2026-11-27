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
bool IndependentSignedJacobian2D(const FaceNormalTable& table, const int* slots, int slot_count, double n,
                                 const double sun_dir[3], const PoseManifold& manifold, const double param[3],
                                 double* j_out, double* sigma1_out = nullptr, double* sigma2_out = nullptr,
                                 double* internal_disc_out = nullptr) {
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

TEST(CausticContinuation, EulerChartParityAndPoseManifoldConstructors) {
  // Verify EulerPoseRad matches family_rank.hpp's EulerPose (so3::Exp form) to machine precision
  // and round-trips through PoseToEulerRad.
  const double test_angles[][3] = {
    { 0.0, 0.0, 0.0 }, { 37.5, 59.75, 142.25 }, { 237.7, 90.0, 0.0 }, { 315.0, 135.0, 270.0 }, { 120.0, 180.0, 0.0 },
  };
  for (const auto& a : test_angles) {
    const double az = a[0] * kDegToRad;
    const double z = a[1] * kDegToRad;
    const double r = a[2] * kDegToRad;
    double pose_direct[9];
    double pose_exp[9];
    EulerPoseRad(az, z, r, pose_direct);
    EulerPose(az, z, r, pose_exp);
    for (int i = 0; i < 9; i++) {
      EXPECT_NEAR(pose_direct[i], pose_exp[i], 1e-14) << "entry " << i;
    }
    EXPECT_TRUE(ValidateRotation(pose_direct));

    double recovered[3];
    PoseToEulerRad(pose_direct, recovered);
    double pose_rt[9];
    EulerPoseRad(recovered[0], recovered[1], recovered[2], pose_rt);
    EXPECT_LT(so3::Distance(pose_direct, pose_rt), 1e-13);
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
  EulerPoseDegRadOut(237.7, 60.0, 0.0, seed_pose);
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
        EXPECT_LE(std::fabs(j_ind), 1e-11) << "node " << i << " |J|=" << std::fabs(j_ind);
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
        EXPECT_LE(std::fabs(j_ind), 1e-11) << "sun_alt=" << sun_alt_deg << " |J|=" << std::fabs(j_ind);
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
      if (!((ok_same && std::fabs(j_same) < 1e-9) || (ok_flip && std::fabs(j_flip) < 1e-9))) {
        ADD_FAILURE() << "No symmetric partner on J=0 for (az=" << az_deg << ", z=" << z_deg << ")";
        continue;
      }

      const double target_az_deg = sym_az_deg;
      const double target_z_deg = (ok_same && std::fabs(j_same) < 1e-9) ? z_deg : sym_z_deg;

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
  }

  // Case D: Euler pole spin chart TiltedSpin(0.0) on {3, 5} -> generator rank 1 -> kCollapsed2D.
  {
    int slots[2];
    const int faces[2] = { 3, 5 };
    f.Resolve(faces, 2, slots);
    const CausticTraceResult res = TraceCausticLocus(f.normals, slots, 2, n, sun_dir, PoseManifold::TiltedSpin(0.0));
    EXPECT_EQ(res.generic_rank, 1);
    ASSERT_FALSE(res.curves.empty());
    for (const PoseCurve& c : res.curves) {
      EXPECT_EQ(c.kind, PoseCurveKind::kCollapsed2D);
      EXPECT_GT(c.nodes.size(), 10u);
    }
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

}  // namespace
}  // namespace lumice::analytic
