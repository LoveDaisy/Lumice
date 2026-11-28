#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "analytic/caustic_continuation.hpp"
#include "analytic/curve_profile.hpp"
#include "analytic/jet2.hpp"
#include "analytic/path_chain.hpp"
#include "core/optics.hpp"

namespace lumice::analytic {
namespace {

constexpr double kRad = math::kDegreeToRadD;
constexpr double kFactorTolerance = 2e-11;

CrystalShape Prism(double height = 1.0) {
  CrystalShape shape;
  shape.height = height;
  std::fill_n(shape.face_distance, 6, 1.0);
  return shape;
}

struct Fixture {
  CrystalShape shape;
  FaceNormalTable normals;
  FacePolygonTable polygons;
  std::vector<int> slots;
  PathIntrinsic path;
  double sun[3];

  Fixture(std::vector<int> faces = { 3, 5 }, double height = 1.0, double altitude = 20.0,
          double n = IceRefractiveIndex::Get(550.0))
      : shape(Prism(height)), slots(faces.size()), path(shape, faces, n) {
    EXPECT_EQ(BuildFaceNormals(shape, &normals, &polygons), Status::kOk);
    EXPECT_EQ(ResolveFaceSequence(normals, faces.data(), static_cast<int>(faces.size()), slots.data()), Status::kOk);
    AltAzDegToSky(altitude, 0.0, sun);
  }

  PoseCurveNode Node(double az, double zenith, double roll) const {
    PoseCurveNode node;
    const double param[3] = { az * kRad, zenith * kRad, roll * kRad };
    std::copy_n(param, 3, node.param);
    EulerPose(param[0], param[1], param[2], node.pose);
    WorldToBody(node.pose, sun, node.u);
    const double incident[3] = { -sun[0], -sun[1], -sun[2] };
    double directions[3 * (kMaxFaceCount + 1)], factors[kMaxFaceCount];
    PathOutputs out{};
    out.segment_directions = directions;
    out.interface_transmittances = factors;
    if (EvaluatePath(normals, slots.data(), static_cast<int>(slots.size()), path.field().RefractiveIndex(), incident,
                     node.pose, &out)) {
      for (int i = 0; i < 3; ++i) {
        node.sky[i] = -out.outgoing_direction[i];
      }
    } else {
      node.sky[2] = 1.0;
    }
    return node;
  }
};

AxisPoseDensity Density(bool non_gaussian = false) {
  AxisDistribution law;
  law.latitude_dist = { DistributionType::kNoRandom, 0.0f, 0.0f };
  law.azimuth_dist = { non_gaussian ? DistributionType::kLaplacian : DistributionType::kUniform, 270.0f,
                       non_gaussian ? 12.0f : 360.0f };
  law.roll_dist = { DistributionType::kUniform, 0.0f, 360.0f };
  return AxisPoseDensity(law);
}

// Direct authority comparison in world coordinates, independent of the profile's u/identity
// optical route. A finite corridor and all interface coefficients are checked, not only a product.
void CheckNode(const Fixture& f, const PoseCurveNode& node, const CurveProfileNode& value,
               const AxisPoseDensity& density) {
  ASSERT_TRUE(value.context_valid);
  std::array<double, 9> rotation;
  std::copy_n(node.pose, 9, rotation.begin());
  const auto rho = density.Evaluate(rotation), log_rho = density.LogDensity(rotation);
  EXPECT_EQ(value.rho.flags, rho.flags);
  EXPECT_EQ(value.log_rho.flags, log_rho.flags);
  if (std::isfinite(log_rho.value)) {
    EXPECT_NEAR(value.log_rho.value, log_rho.value, kFactorTolerance);
  } else if (std::isnan(log_rho.value)) {
    EXPECT_TRUE(std::isnan(value.log_rho.value));
  } else {
    EXPECT_EQ(value.log_rho.value, log_rho.value);
  }
  if (std::isfinite(rho.value)) {
    EXPECT_NEAR(value.rho.value, rho.value, kFactorTolerance * std::max(1.0, rho.value));
  } else {
    EXPECT_TRUE(std::isnan(value.rho.value));
  }
  const auto sample = f.path.EvaluateU(node.u);
  EXPECT_EQ(value.main.valid, sample.v_p);
  EXPECT_NEAR(value.main.transmission, sample.t_p, kFactorTolerance);
  const auto area = f.path.EvaluateAreaU(node.u);
  EXPECT_EQ(value.main.area.status, area.status);
  EXPECT_NEAR(value.main.area.value, area.value, kFactorTolerance);
  Corridor corridor(f.normals, f.polygons, f.slots.data(), static_cast<int>(f.slots.size()));
  const double incident_body[3] = { -node.u[0], -node.u[1], -node.u[2] };
  const auto direct_area = corridor.Evaluate(incident_body, f.path.field().RefractiveIndex());
  EXPECT_NEAR(value.main.area.value, direct_area.value, kFactorTolerance);
  const double incident[3] = { -f.sun[0], -f.sun[1], -f.sun[2] };
  double directions[3 * (kMaxFaceCount + 1)], factors[kMaxFaceCount];
  PathOutputs detail{};
  detail.segment_directions = directions;
  detail.interface_transmittances = factors;
  const bool valid = EvaluatePath(f.normals, f.slots.data(), static_cast<int>(f.slots.size()),
                                  f.path.field().RefractiveIndex(), incident, node.pose, &detail);
  EXPECT_EQ(value.main.valid, valid);
  if (valid) {
    bool ill_conditioned = value.main.grazing_accuracy_note;
    for (size_t i = 1; i + 1 < f.slots.size(); ++i) {
      ill_conditioned = ill_conditioned ||
                        std::abs(value.main.interfaces[i].discriminant) < CurveProfileOptions{}.grazing_discriminant;
    }
    if (!ill_conditioned) {
      EXPECT_NEAR(value.main.transmission, detail.fresnel_transmission, kFactorTolerance);
    }
    EXPECT_GE(value.main.transmission, 0.0);
    EXPECT_LE(value.main.transmission, 1.0);
    for (size_t i = 0; i < f.slots.size(); ++i) {
      EXPECT_TRUE(value.main.interfaces[i].factor_available);
      if (ill_conditioned) {
        // At Snell cliffs / internal TIR corners, two rotation evaluation routes differ
        // in d by roundoff; sqrt(d) degrades coefficient accuracy. The same-u authority
        // checks above still verify composition; cross-route checks here stay bounded.
        EXPECT_GE(value.main.interfaces[i].factor, 0.0);
        EXPECT_LE(value.main.interfaces[i].factor, 1.0);
      } else {
        EXPECT_NEAR(value.main.interfaces[i].factor, factors[i], kFactorTolerance);
      }
    }
  }
}

void CheckCurves(const Fixture& f, const CausticTraceResult& trace, const PoseManifold& manifold,
                 const AxisPoseDensity& density) {
  ASSERT_FALSE(trace.curves.empty());
  for (const auto& curve : trace.curves) {
    const auto profile = EvaluateCurveProfile(curve, f.path, density, manifold, f.sun);
    if (profile.nodes.size() != curve.nodes.size()) {
      ADD_FAILURE() << "Profile node count differs from curve";
      continue;
    }
    EXPECT_EQ(profile.coverage.budget_truncated, curve.coverage.budget_truncated);
    for (size_t i = 0; i < curve.nodes.size(); ++i) {
      CheckNode(f, curve.nodes[i], profile.nodes[i], density);
    }
  }
}

TEST(CurveProfile, GoalOneColumnApexCliffAndLowitzBranchAnchors) {
  const auto density = Density();
  for (double altitude : { 15.0, 20.0 }) {
    const Fixture f({ 3, 5 }, 1.0, altitude);
    for (const auto& manifold : { PoseManifold::Column(), PoseManifold::Lowitz() }) {
      const auto trace =
          TraceCausticLocus(f.normals, f.slots.data(), 2, f.path.field().RefractiveIndex(), f.sun, manifold);
      CheckCurves(f, trace, manifold, density);
      bool apex = false, cliff_tail = false, near_sun = false;
      for (const auto& curve : trace.curves) {
        for (const auto& node : curve.nodes) {
          double alt, az;
          SkyToAltAzDeg(node.sky, &alt, &az);
          if (manifold.axes[1].free) {
            near_sun = near_sun || (std::abs(az) > 25.0 && std::abs(alt - altitude) < 2.0);
          } else {
            const double expected =
                altitude + (2.0 * std::asin(f.path.field().RefractiveIndex() / 2.0) - math::kPiD / 3.0) / kRad;
            apex = apex || (std::abs(az) < 1.0 && std::abs(alt - expected) < 0.02);
            cliff_tail = cliff_tail || (std::abs(az) > 45.0 && alt < altitude && alt > 0.0);
          }
        }
      }
      if (manifold.axes[1].free) {
        EXPECT_TRUE(near_sun);
      } else {
        EXPECT_TRUE(apex);
        EXPECT_TRUE(cliff_tail);
      }
    }
  }
}

TEST(CurveProfile, MirrorCollapseParryNonGaussianNarrowCorridorAndTirTruncation) {
  const auto density = Density(true);
  const Fixture standard;
  PoseManifold mirror = PoseManifold::Column();
  mirror.axes[0] = PoseAxis::FreeDeg(25.713648, 25.913648);
  mirror.axes[2] = PoseAxis::FreeDeg(119.9, 120.1);
  const auto collapsed = TraceCausticLocus(standard.normals, standard.slots.data(), 2,
                                           standard.path.field().RefractiveIndex(), standard.sun, mirror);
  CheckCurves(standard, collapsed, mirror, density);
  bool midpoint = false;
  for (const auto& curve : collapsed.curves) {
    for (const auto& node : curve.nodes) {
      double alt, az;
      SkyToAltAzDeg(node.sky, &alt, &az);
      midpoint = midpoint || (std::abs(alt + 20.0) < 1e-6 && std::abs(az) < 1e-6);
    }
  }
  EXPECT_TRUE(midpoint);
  for (double height : { 1.0, 0.0001 }) {
    const Fixture f({ 3, 5 }, height);
    Corridor corridor(f.normals, f.polygons, f.slots.data(), 2);
    const auto trace = TraceFamily1D(f.normals, f.slots.data(), 2, f.path.field().RefractiveIndex(), f.sun,
                                     PoseManifold::Parry(), &corridor);
    CheckCurves(f, trace, PoseManifold::Parry(), density);
    if (height < 1.0) {
      int cutoffs = 0;
      for (const auto& curve : trace.curves) {
        for (const auto& event : curve.events) {
          cutoffs += event.kind == CurveEventKind::kCorridorCutoff ? 1 : 0;
        }
      }
      EXPECT_GE(cutoffs, 2);
    }
  }
  const Fixture tir({ 3, 1, 6 }, 0.8);
  Corridor corridor(tir.normals, tir.polygons, tir.slots.data(), 3);
  Family1DParams params;
  params.max_nodes_per_branch = 25;
  const auto truncated = TraceFamily1D(tir.normals, tir.slots.data(), 3, tir.path.field().RefractiveIndex(), tir.sun,
                                       PoseManifold::Parry(), &corridor, params);
  CheckCurves(tir, truncated, PoseManifold::Parry(), density);
  EXPECT_TRUE(truncated.coverage.budget_truncated);
}

TEST(CurveProfile, DirectJet2FoldCoefficientAndSameSeedCausticDispersion) {
  const Fixture f;
  const auto manifold = PoseManifold::Column();
  const auto traced =
      TraceCausticLocus(f.normals, f.slots.data(), 2, f.path.field().RefractiveIndex(), f.sun, manifold);
  PoseCurveNode apex;
  double best = std::numeric_limits<double>::infinity();
  int checked = 0;
  for (const auto& curve : traced.curves) {
    const auto profile = EvaluateCurveProfile(curve, f.path, Density(), manifold, f.sun);
    for (size_t index = 0; index < curve.nodes.size(); ++index) {
      const auto& node = curve.nodes[index];
      const auto& geometry = profile.nodes[index].geometry;
      if (geometry.status != ProfileGeometryStatus::kAvailable) {
        continue;
      }
      Jet2<2> pose[9];
      EulerPose(Jet2<2>::Variable(node.param[0], 0), Jet2<2>(node.param[1]), Jet2<2>::Variable(node.param[2], 1), pose);
      const Jet2<2> incident[3] = { -f.sun[0], -f.sun[1], -f.sun[2] };
      Jet2<2> outgoing[3];
      if (!TracePathChain(f.normals, f.slots.data(), 2, Jet2<2>(f.path.field().RefractiveIndex()), incident, pose,
                          outgoing, nullptr, nullptr)) {
        ADD_FAILURE() << "Tensor reference rejected an available caustic node";
        continue;
      }
      double a[3], b[3], x[3];
      for (int i = 0; i < 3; ++i) {
        a[i] = -outgoing[i].v[0];
        b[i] = -outgoing[i].v[1];
        x[i] = -outgoing[i].a;
      }
      const double aa = so3::Dot3(a, a), ab = so3::Dot3(a, b), bb = so3::Dot3(b, b);
      double kernel[2] = { ab, -aa };
      const double* image = a;
      if (bb > aa) {
        kernel[0] = bb;
        kernel[1] = -ab;
        image = b;
      }
      const double norm = std::hypot(kernel[0], kernel[1]);
      if (!(norm > 0.0)) {
        ADD_FAILURE() << "Reference kernel is degenerate";
        continue;
      }
      kernel[0] /= norm;
      kernel[1] /= norm;
      double normal[3];
      so3::Cross3(x, image, normal);
      const double normal_norm = so3::Norm3(normal);
      double beta = 0.0;
      for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 2; ++j) {
          for (int k = 0; k < 2; ++k) {
            beta -= 0.5 * normal[i] / normal_norm * outgoing[i].h[j][k] * kernel[j] * kernel[k];
          }
        }
      }
      // Tensor reference, not a singular-integrand contract. Avoid cliff-conditioned pointwise
      // comparisons: the separate optical anchor test still exercises every cliff-tail node.
      const auto sample = f.path.EvaluateU(node.u);
      if (sample.margins[3] > 1e-2 && std::abs(beta) > 1e-4) {
        EXPECT_NEAR(std::abs(geometry.lateral_beta) / std::abs(beta), 1.0, 2e-7);
        EXPECT_NEAR(geometry.fold_concentration * geometry.sigma1 * std::sqrt(std::abs(beta)), 1.0, 2e-7);
        ++checked;
      }
      double alt, az;
      SkyToAltAzDeg(node.sky, &alt, &az);
      if (alt > 40.0 && std::abs(az) < best) {
        best = std::abs(az);
        apex = node;
      }
    }
  }
  ASSERT_GT(checked, 10);
  ASSERT_LT(best, 1.0);
  const PathIntrinsic red(f.shape, { 3, 5 }, IceRefractiveIndex::Get(700.0));
  const PathIntrinsic blue(f.shape, { 3, 5 }, IceRefractiveIndex::Get(400.0));
  const auto shift = ProfileChromaticShift(red, blue, PoseCurveKind::kCaustic2D, manifold, f.sun, apex);
  ASSERT_TRUE(shift.available);
  EXPECT_GT(shift.angle_rad, 0.005);
  for (const auto* point : { &shift.red, &shift.blue }) {
    double alt, az;
    SkyToAltAzDeg(point->node.sky, &alt, &az);
    const double n = point == &shift.red ? red.field().RefractiveIndex() : blue.field().RefractiveIndex();
    const double expected = 20.0 + (2.0 * std::asin(n / 2.0) - math::kPiD / 3.0) / kRad;
    EXPECT_NEAR(alt, expected, 1e-3);
  }
}

TEST(CurveProfile, MultiLambdaTirTableAndDisplacementsAgainstDirectRetraces) {
  const Fixture f({ 3, 1, 6 }, 0.8);
  const auto manifold = PoseManifold::Parry();
  const PathIntrinsic red(f.shape, { 3, 1, 6 }, IceRefractiveIndex::Get(700.0));
  const PathIntrinsic blue(f.shape, { 3, 1, 6 }, IceRefractiveIndex::Get(400.0));
  const auto trace = TraceFamily1D(f.normals, f.slots.data(), 3, f.path.field().RefractiveIndex(), f.sun, manifold);
  bool found = false;
  for (const auto& curve : trace.curves) {
    const std::vector<const PathIntrinsic*> wavelengths = { &red, &f.path, &blue };
    const auto profile = EvaluateCurveProfile(curve, f.path, Density(), manifold, f.sun, wavelengths);
    for (size_t i = 0; i < curve.nodes.size(); ++i) {
      for (size_t w = 0; w < wavelengths.size(); ++w) {
        const auto& value = profile.nodes[i].wavelengths[w];
        const auto* path = wavelengths[w];
        double margins[2 * kMaxFaceCount];
        path->field().DomainMarginsAt(curve.nodes[i].u, margins);
        if (value.valid) {
          const auto state = margins[3] > 0.0 ? ProfileTirState::kTotal :
                             margins[3] < 0.0 ? ProfileTirState::kPartial :
                                                ProfileTirState::kCritical;
          EXPECT_EQ(value.interfaces[1].tir, state);
          EXPECT_NEAR(value.interfaces[1].discriminant, -margins[3], kFactorTolerance);
          const auto optical = path->EvaluateU(curve.nodes[i].u);
          EXPECT_NEAR(value.transmission, optical.t_p, kFactorTolerance);
          EXPECT_NEAR(value.area.value, optical.a_p, kFactorTolerance);
        }
      }
    }
    for (const auto& event : curve.events) {
      if (event.kind != CurveEventKind::kTirOnset) {
        continue;
      }
      auto seed = f.Node(event.param[0] / kRad, event.param[1] / kRad, event.param[2] / kRad);
      const auto tir = ProfileChromaticShift(red, blue, curve.kind, manifold, f.sun, seed, 1);
      if (!tir.available) {
        ADD_FAILURE() << "Red/blue TIR correction failed";
        continue;
      }
      EXPECT_GT(tir.angle_rad, 1e-4);
      const auto dispersion = ProfileChromaticShift(red, blue, curve.kind, manifold, f.sun, seed);
      if (!dispersion.available) {
        ADD_FAILURE() << "Red/blue position correction failed";
        continue;
      }
      // This basal-reflection path is a slab: fixed-pose position dispersion vanishes,
      // while its TIR-kink position moves. The caustic test pins nonzero position dispersion.
      EXPECT_LT(dispersion.angle_rad, kFactorTolerance);
      for (const auto* path : { &red, &blue }) {
        const auto direct =
            TraceFamily1D(f.normals, f.slots.data(), 3, path->field().RefractiveIndex(), f.sun, manifold);
        const auto& point = path == &red ? tir.red : tir.blue;
        double nearest = std::numeric_limits<double>::infinity();
        for (const auto& branch : direct.curves) {
          for (const auto& candidate : branch.events) {
            if (candidate.kind == CurveEventKind::kTirOnset) {
              nearest = std::min(nearest, so3::Distance(point.node.pose, candidate.pose));
            }
          }
        }
        EXPECT_LT(nearest, 5e-10);
        const auto& fixed = path == &red ? dispersion.red : dispersion.blue;
        EXPECT_NEAR(fixed.node.param[0], seed.param[0], 1e-13);
        const double incident[3] = { -f.sun[0], -f.sun[1], -f.sun[2] };
        double dirs[12], factors[3];
        PathOutputs detail{};
        detail.segment_directions = dirs;
        detail.interface_transmittances = factors;
        if (!EvaluatePath(f.normals, f.slots.data(), 3, path->field().RefractiveIndex(), incident, seed.pose,
                          &detail)) {
          ADD_FAILURE() << "Direct wavelength trace rejected a valid seed";
          continue;
        }
        for (int j = 0; j < 3; ++j) {
          EXPECT_NEAR(fixed.node.sky[j], -detail.outgoing_direction[j], kFactorTolerance);
        }
      }
      for (int j = 0; j < 3; ++j) {
        EXPECT_NEAR(tir.dx[static_cast<size_t>(j)], tir.blue.node.sky[j] - tir.red.node.sky[j], kFactorTolerance);
      }
      found = true;
      break;
    }
    if (found) {
      break;
    }
  }
  EXPECT_TRUE(found);
}

TEST(CurveProfile, RepresentativeRoundTripAndFreshFiberJacobian) {
  const Fixture f;
  const auto node = f.Node(237.7, 60.0, 0.0);
  const auto expanded = ExpandProfileNode(f.path, node);
  ASSERT_EQ(expanded.status, RecoveryStatus::kOk);
  ASSERT_EQ(expanded.path.contact_count, 2);
  ASSERT_EQ(expanded.directions.size(), 3u);
  double leg[3];
  for (int i = 0; i < 3; ++i) {
    leg[i] = expanded.path.contacts[1][i] - expanded.path.contacts[0][i];
  }
  const double norm = so3::Norm3(leg);
  ASSERT_GT(norm, 0.0);
  for (int i = 0; i < 3; ++i) {
    EXPECT_NEAR(leg[i] / norm, expanded.directions[1][static_cast<size_t>(i)], kFactorTolerance);
  }
  CorridorDiagnostics diag;
  f.path.EvaluateAreaU(node.u, &diag);
  RepresentativePath direct;
  ASSERT_EQ(f.path.RecoverRepresentativePath(diag, &direct), RecoveryStatus::kOk);
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 3; ++j) {
      EXPECT_NEAR(expanded.path.contacts[i][j], direct.contacts[i][j], kFactorTolerance);
    }
  }
  PoseCurve curve;
  curve.kind = PoseCurveKind::kSkyFiber;
  curve.nodes = { node };
  const auto profile = EvaluateCurveProfile(curve, f.path, Density(), {}, f.sun);
  ASSERT_EQ(profile.nodes[0].geometry.status, ProfileGeometryStatus::kAvailable);
  const double incident[3] = { -f.sun[0], -f.sun[1], -f.sun[2] };
  const auto direct_j =
      EvaluatePathDiagnostics(f.normals, f.slots.data(), 2, f.path.field().RefractiveIndex(), incident, node.pose);
  EXPECT_NEAR(profile.nodes[0].geometry.inverse_normal_jacobian * direct_j.jacobian.value, 1.0, kFactorTolerance);
}

TEST(CurveProfile, HotSeedMatchesColdDiscoveryAndReportsNonConvergence) {
  const Fixture f({ 3, 5 }, 1.0, 15.0, 1.31);
  const double incident[3] = { -f.sun[0], -f.sun[1], -f.sun[2] };
  const double target[3] = { -0.987460586147287, -0.010383253351062983, 0.15752389931532262 };
  IceDiscovery discovery(f.normals, f.polygons, f.slots.data(), 2, 1.31, incident);
  ProfileFiberOptions options;
  const auto cold = discovery.Discover(target, options.sample_count, options.band_half_width, nullptr, 0,
                                       options.cluster_radius, options.dedup_distance, options.continuation);
  ASSERT_FALSE(cold.components.empty());
  PoseCurveNode node;
  std::copy_n(cold.components[0].seed, 9, node.pose);
  WorldToBody(node.pose, f.sun, node.u);
  for (int i = 0; i < 3; ++i) {
    node.sky[i] = -target[i];
  }
  const auto hot = LinkProfileFiber(discovery, node, options);
  ASSERT_EQ(hot.status, ProfileFiberStatus::kConverged);
  ASSERT_GE(hot.hot_component, 0);
  const auto& component = hot.output.components[static_cast<size_t>(hot.hot_component)];
  auto length = [](const DiscoveredComponent& c) {
    double sum = 0.0;
    for (double v : c.forward.arclength_increments) {
      sum += v;
    }
    for (double v : c.backward.arclength_increments) {
      sum += v;
    }
    return sum;
  };
  EXPECT_NEAR(length(component), length(cold.components[0]), 3e-5);
  options.continuation.maximum_accepted_steps = 1;
  const auto failed = LinkProfileFiber(discovery, node, options);
  EXPECT_TRUE(failed.hot_seed_admitted);
  EXPECT_EQ(failed.status, ProfileFiberStatus::kNotConverged);
  EXPECT_EQ(failed.hot_component, -1);
  EXPECT_FALSE(failed.output.incomplete.empty());
}

TEST(CurveProfile, CausticTirCorrectorUsesDiscriminantAndHonestBudgets) {
  const Fixture f({ 3, 1, 5 }, 0.8);
  const auto manifold = PoseManifold::Column();
  const auto trace = TraceCausticLocus(f.normals, f.slots.data(), 3, f.path.field().RefractiveIndex(), f.sun, manifold);
  const PathIntrinsic blue(f.shape, { 3, 1, 5 }, IceRefractiveIndex::Get(400.0));
  bool checked = false;
  for (const auto& curve : trace.curves) {
    for (const auto& event : curve.events) {
      if (event.kind != CurveEventKind::kTirOnset) {
        continue;
      }
      const auto seed = f.Node(event.param[0] / kRad, event.param[1] / kRad, event.param[2] / kRad);
      const auto point = RetraceProfileSeed(blue, curve.kind, manifold, f.sun, seed, 1);
      if (point.status != ProfileSolveStatus::kConverged) {
        ADD_FAILURE() << "Caustic TIR correction failed";
        continue;
      }
      double margins[2 * kMaxFaceCount];
      blue.field().DomainMarginsAt(point.node.u, margins);
      EXPECT_NEAR(margins[3], 0.0, 2e-10);
      // Independent full trace at blue: compare the actual discriminant-located TIR event,
      // not a Fresnel slope, and not the corrector's own residual report.
      const auto direct =
          TraceCausticLocus(f.normals, f.slots.data(), 3, blue.field().RefractiveIndex(), f.sun, manifold);
      double distance = std::numeric_limits<double>::infinity();
      for (const auto& branch : direct.curves) {
        for (const auto& blue_event : branch.events) {
          if (blue_event.kind == CurveEventKind::kTirOnset) {
            distance = std::min(distance, so3::Distance(point.node.pose, blue_event.pose));
          }
        }
      }
      EXPECT_LT(distance, 2e-8);
      ProfileChromaticOptions limited;
      limited.max_travel = 1e-12;
      EXPECT_EQ(RetraceProfileSeed(blue, curve.kind, manifold, f.sun, seed, 1, limited).status,
                ProfileSolveStatus::kTravelLimit);
      limited = {};
      limited.maximum_iterations = 1;
      EXPECT_EQ(RetraceProfileSeed(blue, curve.kind, manifold, f.sun, seed, 1, limited).status,
                ProfileSolveStatus::kIterationLimit);
      checked = true;
      break;
    }
    if (checked) {
      break;
    }
  }
  EXPECT_TRUE(checked);
}

TEST(CurveProfile, HonestyFlagsDeepTailGrazingAndInvalidInputs) {
  const Fixture f;
  PoseCurve curve;
  curve.kind = PoseCurveKind::kFamily1D;
  curve.nodes = { f.Node(237.7, 90.0, 0.0) };
  AxisDistribution law;
  law.latitude_dist = { DistributionType::kNoRandom, 0.0f, 0.0f };
  law.roll_dist = { DistributionType::kNoRandom, 0.0f, 0.0f };
  law.azimuth_dist = { DistributionType::kGaussian, 0.0f, 0.1f };
  const AxisPoseDensity density(law);
  auto profile = EvaluateCurveProfile(curve, f.path, density, PoseManifold::Parry(), f.sun);
  EXPECT_EQ(profile.nodes[0].rho.value, 0.0);
  EXPECT_TRUE(std::isfinite(profile.nodes[0].log_rho.value));
  EXPECT_LT(profile.nodes[0].log_rho.value, -10000.0);
  CurveProfileOptions warning;
  warning.grazing_discriminant = 1.0;
  profile = EvaluateCurveProfile(curve, f.path, density, PoseManifold::Parry(), f.sun, {}, warning);
  EXPECT_TRUE(profile.nodes[0].main.grazing_accuracy_note);
  EXPECT_GE(profile.nodes[0].main.transmission, 0.0);
  EXPECT_LE(profile.nodes[0].main.transmission, 1.0);
  curve.nodes[0].u[0] += 0.01;
  profile = EvaluateCurveProfile(curve, f.path, density, PoseManifold::Parry(), f.sun);
  EXPECT_FALSE(profile.nodes[0].context_valid);
  EXPECT_EQ(profile.nodes[0].geometry.status, ProfileGeometryStatus::kInvalidNode);
  EXPECT_THROW(EvaluateCurveProfile(curve, f.path, density, PoseManifold::Parry(), f.sun, { nullptr }),
               std::invalid_argument);
  const auto invalid = RetraceProfileSeed(f.path, curve.kind, PoseManifold::Parry(), f.sun, curve.nodes[0]);
  EXPECT_EQ(invalid.status, ProfileSolveStatus::kInvalidInput);
}

}  // namespace
}  // namespace lumice::analytic
