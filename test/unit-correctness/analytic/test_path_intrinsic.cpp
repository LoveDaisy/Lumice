#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include "analytic/path_intrinsic.hpp"
#include "analytic/so3.hpp"

namespace lumice::analytic {
namespace {

CrystalShape Prism(double height = 1.0) {
  CrystalShape shape;
  shape.height = height;
  std::fill_n(shape.face_distance, 6, 1.0);
  return shape;
}

CrystalShape Pyramid(double wedge) {
  auto shape = Prism();
  shape.kind = CrystalShapeKind::kPyramid;
  shape.upper_h = 0.5;
  shape.lower_h = 0.5;
  shape.upper_wedge_deg = wedge;
  shape.lower_wedge_deg = wedge;
  return shape;
}

// Separate evaluations may contract differently on FMA architectures. This is a wrapper
// consistency budget, not an accuracy claim for the underlying field: 64 eps * path length
// for O(1) chains, scaled by the magnitude of a derivative when it is ill-conditioned.
void ExpectScalar(double actual, double expected, int count) {
  if (std::isnan(expected)) {
    EXPECT_TRUE(std::isnan(actual));
  } else if (std::isinf(expected)) {
    EXPECT_EQ(actual, expected);
  } else {
    EXPECT_NEAR(actual, expected,
                64.0 * std::numeric_limits<double>::epsilon() * count * std::max(1.0, std::abs(expected)));
  }
}

void ExpectSample(const FieldSample& actual, const FieldSample& expected, int count) {
  ExpectScalar(actual.d_p, expected.d_p, count);
  ExpectScalar(actual.d_value, expected.d_value, count);
  ExpectScalar(actual.d_p_grazing, expected.d_p_grazing, count);
  ExpectScalar(actual.d_p_exit_limit, expected.d_p_exit_limit, count);
  ExpectScalar(actual.a_p, expected.a_p, count);
  ExpectScalar(actual.t_p, expected.t_p, count);
  EXPECT_EQ(actual.v_p, expected.v_p);
  EXPECT_EQ(actual.location, expected.location);
  ASSERT_EQ(actual.margin_count, expected.margin_count);
  ASSERT_EQ(actual.validity_count, expected.validity_count);
  for (int k = 0; k < actual.margin_count; ++k) {
    ExpectScalar(actual.margins[k], expected.margins[k], count);
  }
  for (int k = 0; k < actual.validity_count; ++k) {
    ExpectScalar(actual.validity_margins[k], expected.validity_margins[k], count);
  }
}

void ExpectJet(const FieldJet& actual, const FieldJet& expected, int count) {
  ExpectScalar(actual.value, expected.value, count);
  ExpectScalar(actual.d_p_dn, expected.d_p_dn, count);
  for (int i = 0; i < 3; ++i) {
    ExpectScalar(actual.tangent_gradient[i], expected.tangent_gradient[i], count);
    for (int j = 0; j < 2; ++j) {
      ExpectScalar(actual.tangent_basis[j][i], expected.tangent_basis[j][i], count);
    }
  }
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 2; ++j) {
      ExpectScalar(actual.hessian[i][j], expected.hessian[i][j], count);
    }
  }
  ASSERT_EQ(actual.margin_count, expected.margin_count);
  for (int k = 0; k < actual.margin_count; ++k) {
    ExpectScalar(actual.margins[k], expected.margins[k], count);
    ExpectScalar(actual.margins_dn[k], expected.margins_dn[k], count);
  }
}

TEST(PathIntrinsic, FieldAndDecompositionMatchAuthoritiesAcrossCorpusScenes) {
  struct Scene {
    double height;
    std::vector<int> faces;
  };
  // C16 column, C14 plate, C02 TIR-weight corridor and C03 multiple internal reflections.
  const Scene scenes[] = { { 1.0, { 3, 5 } }, { 0.3, { 1, 2 } }, { 2.0, { 3, 1, 5 } }, { 1.0, { 3, 5, 6, 7 } } };
  for (const auto& scene : scenes) {
    for (double n : { 1.307, 1.317 }) {
      const auto shape = Prism(scene.height);
      PathIntrinsic path(shape, scene.faces, n);
      FaceNormalTable normals;
      FacePolygonTable polygons;
      if (BuildFaceNormals(shape, &normals, &polygons) != Status::kOk) {
        ADD_FAILURE() << "corpus crystal construction failed";
        continue;
      }
      const int count = static_cast<int>(scene.faces.size());
      int slots[kMaxFaceCount];
      if (ResolveFaceSequence(normals, scene.faces.data(), count, slots) != Status::kOk) {
        ADD_FAILURE() << "corpus literal face resolution failed";
        continue;
      }
      DeviationField direct(normals, polygons, slots, count, n);
      const auto screen = BuildFoldScreen(normals, slots, count);
      double matrix[9];
      FoldMatrixOf(normals, slots, count, matrix);
      const auto& algebra = path.decomposition();
      EXPECT_EQ(path.faces(), scene.faces);
      EXPECT_EQ(algebra.slab, screen.degenerate);
      ExpectScalar(algebra.fold_dot, screen.dot, count);
      ExpectScalar(algebra.wedge_angle, std::acos(std::clamp(-screen.dot, -1.0, 1.0)), count);
      for (int i = 0; i < 9; ++i) {
        ExpectScalar(algebra.fold_matrix[i], matrix[i], count);
        ExpectScalar(algebra.fold_matrix[i], screen.fold_matrix[i], count);
      }
      for (int i = 0; i < 3; ++i) {
        ExpectScalar(algebra.entry_normal[i], normals.normal[slots[0]][i], count);
        double unfolded = 0.0;
        for (int j = 0; j < 3; ++j) {
          unfolded += matrix[j * 3 + i] * normals.normal[slots[count - 1]][j];
        }
        ExpectScalar(algebra.unfolded_exit_normal[i], unfolded, count);
      }
      // Deterministic sphere grid includes interior, boundary-adjacent and exterior directions.
      int interior = 0;
      for (int iz = 0; iz < 9; ++iz) {
        const double z = -0.96 + 0.24 * iz;
        for (int ia = 0; ia < 24; ++ia) {
          const double az = (ia + 0.17) * 2.0 * std::acos(-1.0) / 24.0;
          const double radius = std::sqrt(1.0 - z * z);
          const double u[3] = { radius * std::cos(az), radius * std::sin(az), z };
          const auto sample = direct.Sample(u);
          interior += sample.location == DomainLocation::kInterior ? 1 : 0;
          ExpectSample(path.EvaluateU(u), sample, count);
          ExpectJet(path.DifferentiateU(u), direct.Differentiate(u), count);
          ExpectScalar(path.EvaluateAreaU(u).value, sample.a_p, count);
        }
      }
      EXPECT_GT(interior, 0);
      if (scene.faces == std::vector<int>{ 3, 1, 5 }) {
        // C02's basal-reflection onset is u_z^2 = n^2 - 1. Probe both sides of TIR, keeping
        // entry/exit gates and finite corridor positive rather than comparing two exterior NaNs.
        for (double offset : { -1e-7, 0.0, 1e-7 }) {
          const double z = -std::sqrt(n * n - 1.0) * (1.0 + offset);
          const double radius = std::sqrt(1.0 - z * z);
          const double az = -75.0 * std::acos(-1.0) / 180.0;
          const double u[3] = { radius * std::cos(az), radius * std::sin(az), z };
          const auto sample = direct.Sample(u);
          EXPECT_TRUE(sample.v_p);
          EXPECT_GT(sample.a_p, 0.0);
          EXPECT_LT(std::abs(sample.margins[3]), 1e-6);
          ExpectSample(path.EvaluateU(u), sample, count);
          ExpectJet(path.DifferentiateU(u), direct.Differentiate(u), count);
        }
      }
    }
  }
}

TEST(PathIntrinsic, SlabWedgeIdentityAndAxisTruthTable) {
  struct Row {
    std::vector<int> faces;
    bool slab;
    bool identity;
    bool axis;
  };
  const Row rows[] = { { { 3, 5 }, false, false, false },     { { 1, 2 }, true, true, false },
                       { { 1, 3, 6, 2 }, true, true, false }, { { 3, 1, 6 }, true, false, false },
                       { { 1, 3, 4, 2 }, true, false, true }, { { 1, 1, 3, 2 }, true, false, false },
                       { { 1 }, true, false, false },         { { 1, 3, 4, 1, 2 }, true, false, true } };
  for (const auto& row : rows) {
    PathIntrinsic path(Prism(), row.faces, 1.31);
    const auto& d = path.decomposition();
    EXPECT_EQ(d.slab, row.slab);
    EXPECT_EQ(d.identity, row.identity);
    EXPECT_EQ(d.fixed_axis.has_value(), row.axis);
    if (d.fixed_axis) {
      const auto& a = *d.fixed_axis;
      EXPECT_NEAR(so3::Dot3(a.data(), a.data()), 1.0, 1e-12);
      double transformed[3];
      BodyToWorld(d.fold_matrix.data(), a.data(), transformed);
      const double eigenvalue = row.faces.size() % 2 == 0 ? 1.0 : -1.0;
      for (int i = 0; i < 3; ++i) {
        EXPECT_NEAR(transformed[i], eigenvalue * a[i], 1e-12);
      }
    }
  }
}

TEST(PathIntrinsic, CosineBoundaryAdjacentPredicatesAndNearEndpointAxes) {
  // Opposite upper/lower cone normals at nearly equal slopes put the dot on both sides of the
  // slab cosine budget. Offsets stay clear of the threshold (no FMA-dependent expectation),
  // and within the engine's supported 0.1..89.9 degree cone range.
  for (const auto& row : { std::pair<double, bool>{ 1e-5, true }, { 0.01, false } }) {
    auto shape = Pyramid(35.0);
    shape.lower_wedge_deg += row.first;
    const PathIntrinsic wedge(shape, { 13, 26 }, 1.31);
    const auto& d = wedge.decomposition();
    EXPECT_EQ(d.slab, row.second);
    EXPECT_EQ(d.identity, row.second);  // no internal reflection, M = I
    const double gap = 1.0 - std::abs(d.fold_dot);
    if (row.second) {
      EXPECT_LT(gap, 0.1 * d.cosine_tolerance);
    } else {
      EXPECT_GT(gap, 10.0 * d.cosine_tolerance);
    }
  }
  // An almost antiparallel upper/lower pair gives a near-zero rotation; a nearly orthogonal
  // pair of upper cone normals gives a near-pi rotation. Both are proper M, not mirrors.
  const double orthogonal_wedge = std::atan(1.0 / std::sqrt(2.0)) * 180.0 / std::acos(-1.0);
  auto near_endpoint = Pyramid(orthogonal_wedge + 1e-5);
  near_endpoint.lower_wedge_deg += 1e-5;
  for (const auto& faces : { std::vector<int>{ 1, 13, 26, 2 }, std::vector<int>{ 1, 13, 15, 2 } }) {
    const PathIntrinsic path(near_endpoint, faces, 1.31);
    const auto& d = path.decomposition();
    const double cosine = 0.5 * (d.fold_matrix[0] + d.fold_matrix[4] + d.fold_matrix[8] - 1.0);
    EXPECT_GT(std::abs(cosine), 1.0 - 0.1 * d.cosine_tolerance);
    EXPECT_FALSE(d.fixed_axis.has_value());
  }
  // Three cone normals 120 degrees apart are orthogonal when tan(wedge) = 1/sqrt(2).
  // Their three reflections compose to -I; a small perturbation must not expose its arbitrary axis.
  for (double offset : { 0.0, 1e-5 }) {
    const PathIntrinsic path(Pyramid(orthogonal_wedge + offset), { 1, 13, 15, 17, 2 }, 1.31);
    const auto& d = path.decomposition();
    const double cosine = 0.5 * (-d.fold_matrix[0] - d.fold_matrix[4] - d.fold_matrix[8] - 1.0);
    EXPECT_GT(cosine, 1.0 - 0.1 * d.cosine_tolerance);
    EXPECT_FALSE(d.fixed_axis.has_value());
    EXPECT_FALSE(d.identity);
  }
}

TEST(PathIntrinsic, CachedKinksKeepResidualsAndHonestCoverage) {
  // C02 has a closed-form basal TIR kink; C18's nonorthogonal side reflection must stay marched.
  for (const auto& faces : { std::vector<int>{ 3, 1, 5 }, std::vector<int>{ 3, 5, 7 } }) {
    PathIntrinsic path(Prism(2.0), faces, 1.31);
    const auto& curves = path.Kinks();
    if (curves.size() != 1u) {
      ADD_FAILURE() << "expected one internal TIR kink";
      continue;
    }
    EXPECT_EQ(&curves, &path.Kinks());
    EXPECT_EQ(curves[0].coverage,
              faces[1] == 1 ? KinkCoverage::kClosedFormAuthority : KinkCoverage::kMarchedUncertified);
    EXPECT_EQ(curves[0].status, WalkStatus::kOk);
    EXPECT_TRUE(curves[0].Complete());  // explicitly NOT a completeness certificate for the march
    EXPECT_FALSE(curves[0].arcs.empty());
    size_t points = 0;
    for (const auto& arc : curves[0].arcs) {
      EXPECT_EQ(arc.points.size(), 3 * arc.values.size());
      for (size_t i = 0; i + 2 < arc.points.size(); i += 3) {
        double margins[2 * kMaxFaceCount];
        path.field().DomainMarginsAt(&arc.points[i], margins);
        EXPECT_NEAR(margins[curves[0].margin], 0.0, 1e-12);
        double validity[kMaxFaceCount + 2];
        const int count = path.field().ValidityMarginsAt(&arc.points[i], validity);
        EXPECT_TRUE(OnUpClosure(validity, count));
        ++points;
      }
    }
    EXPECT_GT(points, 10u);
  }
}

TEST(PathIntrinsic, BoundaryKeepsAuthorityRecordAndCachesIt) {
  PathIntrinsic path(Prism(), { 3, 5 }, 1.31);  // C16, independent of column pose density
  const auto& boundary = path.Boundary();
  EXPECT_EQ(&boundary, &path.Boundary());
  BoundaryWalkRecord record;
  const auto direct = WalkBoundary(path.field(), BoundaryWalkOptions{}, &record);
  ASSERT_EQ(boundary.result.status, direct.status);
  ASSERT_EQ(boundary.result.status, WalkStatus::kOk);
  EXPECT_EQ(boundary.result.message, direct.message);
  ASSERT_EQ(boundary.record.pieces.size(), record.pieces.size());
  ASSERT_EQ(boundary.record.corners.size(), record.corners.size());
  ASSERT_EQ(boundary.result.loop.critical_points.size(), direct.loop.critical_points.size());
  ASSERT_FALSE(boundary.record.pieces.empty());
  EXPECT_FALSE(boundary.result.loop.critical_points.empty());
  for (size_t k = 0; k < record.pieces.size(); ++k) {
    const auto& actual = boundary.record.pieces[k];
    const auto& expected = record.pieces[k];
    EXPECT_EQ(actual.margin, expected.margin);
    if (actual.points.size() != expected.points.size() || actual.values.size() != expected.values.size()) {
      ADD_FAILURE() << "boundary piece sizes differ from authority";
      continue;
    }
    for (size_t i = 0; i < actual.points.size(); ++i) {
      ExpectScalar(actual.points[i], expected.points[i], 2);
    }
    for (size_t i = 0; i < actual.values.size(); ++i) {
      ExpectScalar(actual.values[i], expected.values[i], 2);
    }
  }
  // Consecutive repeated faces are valid literal input, but admit no optical interior. The walk's
  // named refusal must be cached as a refusal, not converted into a successful empty boundary.
  const PathIntrinsic impossible(Prism(), { 3, 3 }, 1.31);
  const auto& refused = impossible.Boundary();
  EXPECT_EQ(refused.result.status, WalkStatus::kStartNoPoint);
  EXPECT_FALSE(refused.result.message.empty());
  EXPECT_TRUE(refused.result.loop.critical_points.empty());
  EXPECT_TRUE(refused.record.pieces.empty());
  EXPECT_TRUE(refused.record.corners.empty());
  EXPECT_EQ(&refused, &impossible.Boundary());
}

TEST(PathIntrinsic, CorridorRecoveryRoundTripAndOwnedValueLifetime) {
  // C01/C16 3-5 at a known positive finite-corridor pose, from EntryMeasureRecovery.
  const double pose[9] = { -0.0214067317528861,  -0.6662448270399101, -0.7454257724805471,
                           -0.35598205366005387, 0.7018108832951394,  -0.6170399189359362,
                           0.9342475738837273,   0.252149389313273,   -0.2521946790900098 };
  const double incident[3] = { -0.9659258262890683, 0.0, -0.25881904510252074 };
  double u[3];
  WorldToBody(pose, incident, u);
  for (double& v : u) {
    v = -v;
  }
  // The original value and its constructor arguments die before either the copy or move is used.
  PathIntrinsic path = [] {
    PathIntrinsic original(Prism(), { 3, 5 }, 1.31);
    original.Boundary();
    original.Kinks();
    PathIntrinsic copy(original);
    EXPECT_NE(&copy.Boundary(), &original.Boundary());
    return PathIntrinsic(std::move(copy));
  }();
  CorridorDiagnostics diagnostics;
  const auto area = path.EvaluateAreaU(u, &diagnostics);
  ASSERT_EQ(area.status, EntryMeasureStatus::kOk);
  EXPECT_GT(area.value, 0.0);
  EXPECT_NEAR(area.value, path.EvaluateU(u).a_p, 1e-12);
  RepresentativePath recovered;
  ASSERT_EQ(path.RecoverRepresentativePath(diagnostics, &recovered), RecoveryStatus::kOk);
  ASSERT_EQ(recovered.contact_count, 2);
  const auto& field = path.field();
  const double body_incident[3] = { -u[0], -u[1], -u[2] };
  const double identity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
  double segments[9];
  double transmission[2];
  PathOutputs output{};
  output.segment_directions = segments;
  output.interface_transmittances = transmission;
  ASSERT_TRUE(EvaluatePath(field.table(), field.slots(), 2, field.RefractiveIndex(), body_incident, identity, &output));
  double leg[3];
  for (int i = 0; i < 3; ++i) {
    leg[i] = recovered.contacts[1][i] - recovered.contacts[0][i];
  }
  const double length = std::sqrt(so3::Dot3(leg, leg));
  ASSERT_GT(length, 0.0);
  for (int i = 0; i < 3; ++i) {
    EXPECT_NEAR(leg[i] / length, segments[3 + i], 1e-10);
  }
  // Refract the recovered geometric leg at the actual exit normal, independently of EvaluatePath.
  const double* exit_normal = field.table().normal[field.slots()[1]];
  const double cosine = so3::Dot3(exit_normal, leg) / length;
  const double n = field.RefractiveIndex();
  const double disc = 1.0 - n * n * (1.0 - cosine * cosine);
  ASSERT_GT(disc, 0.0);
  for (int i = 0; i < 3; ++i) {
    const double outgoing = n * leg[i] / length + (std::sqrt(disc) - n * cosine) * exit_normal[i];
    EXPECT_NEAR(-outgoing, -output.outgoing_direction[i], 1e-10);  // same body-frame sky point
  }
  EXPECT_EQ(path.RecoverRepresentativePath(CorridorDiagnostics{}, &recovered), RecoveryStatus::kNotEvaluated);
}

TEST(PathIntrinsic, RejectsInvalidConstructionBeforeCallingBorrowingAuthorities) {
  EXPECT_THROW(PathIntrinsic(Prism(), {}, 1.31), std::invalid_argument);
  EXPECT_THROW(PathIntrinsic(Prism(), std::vector<int>(kMaxFaceCount + 1, 3), 1.31), std::invalid_argument);
  EXPECT_THROW(PathIntrinsic(Prism(), { 99 }, 1.31), std::invalid_argument);
  EXPECT_THROW(PathIntrinsic(Prism(), { 3, 5 }, 1.0), std::invalid_argument);
  EXPECT_THROW(PathIntrinsic(Prism(), { 3, 5 }, std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
  auto invalid_shape = Prism();
  invalid_shape.height = std::numeric_limits<double>::infinity();
  EXPECT_THROW(PathIntrinsic(invalid_shape, { 3, 5 }, 1.31), std::invalid_argument);
}

}  // namespace
}  // namespace lumice::analytic
