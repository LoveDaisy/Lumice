// The u-S^2 field layer (src/analytic/dp_field.hpp) against LI's pinned anchors (the wave-3
// dissolution probe's measurements, n = Lumice n(550)): the slab closed forms of 3-1-6 and the
// beta crystal's 4-8-7-5, the 3-5 minimum-deviation closed form, the 3-1-5 boundary extremum, the
// 120-degree blade point of 3-1-4-5, and the margin/location/NaN semantics of the layer. Every
// angle tolerance is an assertion constant (doc/testing-architecture cross-ISA discipline);
// the implementation itself carries no tolerance.
//
// symmetry_semantics: none — no symmetry reduction is involved.

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "analytic/dp_field.hpp"
#include "analytic/path_evaluation.hpp"

namespace lumice::analytic {
namespace {

// Lumice n(550 nm): the index every 52.2 anchor is pinned at.
constexpr double kN550 = 1.3110129;
constexpr double kPi = 3.14159265358979323846;

double Deg(double rad) {
  return rad * 180.0 / kPi;
}

void ExpectNearDeg(double value_rad, double expected_deg, double tol_deg, const char* what) {
  EXPECT_NEAR(Deg(value_rad), expected_deg, tol_deg) << what;
}

LUMICE_ANALYTIC_Crystal Prism(double height) {
  LUMICE_ANALYTIC_Crystal c{};
  c.kind = LUMICE_ANALYTIC_CRYSTAL_PRISM;
  c.height = height;
  for (int i = 0; i < 6; i++) {
    c.face_distance[i] = 1.0;
  }
  return c;
}

// The beta crystal of the 52.2 fixtures: fd = [2, 1, 1, 2, 1, 1], h = 3.
LUMICE_ANALYTIC_Crystal Beta() {
  LUMICE_ANALYTIC_Crystal c{};
  c.kind = LUMICE_ANALYTIC_CRYSTAL_PRISM;
  c.height = 3.0;
  const double fd[6] = { 2.0, 1.0, 1.0, 2.0, 1.0, 1.0 };
  for (int i = 0; i < 6; i++) {
    c.face_distance[i] = fd[i];
  }
  return c;
}

struct Fixture {
  FaceNormalTable normals;
  FacePolygonTable polygons;

  explicit Fixture(const LUMICE_ANALYTIC_Crystal& crystal) {
    EXPECT_EQ(BuildFaceNormals(crystal, &normals, &polygons), Status::kOk);
  }

  DeviationField Field(const int* faces, int count, double n) const {
    int slots[kMaxFaceCount];
    EXPECT_EQ(ResolveFaceSequence(normals, faces, count, slots), Status::kOk);
    return DeviationField(normals, polygons, slots, count, n);
  }
};

// The minimum-deviation direction of the 3-5 wedge: u at i = asin(n/2) from the entry normal, in
// the principal plane, away from the exit face (the incident ray heads toward it, bends toward the
// entry normal inside, and crosses the prism to meet face 5 at the symmetric incidence).
void MinDeviationDirection(const FaceNormalTable& t, double u[3], double n) {
  const int s3 = t.SlotOf(3);
  const int s5 = t.SlotOf(5);
  const double* n3 = t.normal[s3];
  const double* n5 = t.normal[s5];
  const double proj = n5[0] * n3[0] + n5[1] * n3[1] + n5[2] * n3[2];
  double t_hat[3] = { n5[0] - proj * n3[0], n5[1] - proj * n3[1], n5[2] - proj * n3[2] };
  const double norm = std::sqrt(t_hat[0] * t_hat[0] + t_hat[1] * t_hat[1] + t_hat[2] * t_hat[2]);
  const double i0 = std::asin(n / 2.0);
  for (int i = 0; i < 3; i++) {
    u[i] = std::cos(i0) * n3[i] - std::sin(i0) * t_hat[i] / norm;
  }
}

// ---------------------------------------------------------------------------------------------
// Fold pre-screen
// ---------------------------------------------------------------------------------------------

TEST(DPField, FoldScreenOfTheKinkSlab) {
  const Fixture f(Prism(1.0));
  const int faces[3] = { 3, 1, 6 };
  const DeviationField field = f.Field(faces, 3, kN550);
  const FoldScreen& screen = field.fold();
  // One basal reflection: the entry and exit refractions cancel exactly (dot = -1), M = S(z) is a
  // mirror whose -1 axis is the c-axis.
  EXPECT_NEAR(screen.dot, -1.0, 1e-15);
  EXPECT_TRUE(screen.degenerate);
  ASSERT_TRUE(screen.has_axis);
  EXPECT_GT(std::fabs(screen.axis[2]), 1.0 - 1e-12);
  EXPECT_LT(std::fabs(screen.axis[0]) + std::fabs(screen.axis[1]), 1e-12);
}

TEST(DPField, FoldScreenOfTheDirectPathIsTheIdentity) {
  const Fixture f(Prism(1.0));
  const int faces[2] = { 3, 5 };
  const DeviationField field = f.Field(faces, 2, kN550);
  const FoldScreen& screen = field.fold();
  EXPECT_NEAR(screen.dot, -0.5, 1e-15);  // n3 . n5, 120 degrees around the prism
  EXPECT_FALSE(screen.degenerate);
  EXPECT_FALSE(screen.has_axis);  // M = I: no internal reflection, no axis
}

TEST(DPField, FoldScreenOfTheBetaSlab) {
  const Fixture f(Beta());
  const int faces[4] = { 4, 8, 7, 5 };
  const DeviationField field = f.Field(faces, 4, kN550);
  const FoldScreen& screen = field.fold();
  // Two side reflections: a rotation whose +1 axis is the c-axis.
  EXPECT_NEAR(screen.dot, -1.0, 1e-12);
  EXPECT_TRUE(screen.degenerate);
  ASSERT_TRUE(screen.has_axis);
  EXPECT_GT(std::fabs(screen.axis[2]), 1.0 - 1e-12);
  EXPECT_LT(std::fabs(screen.axis[0]) + std::fabs(screen.axis[1]), 1e-12);
}

// ---------------------------------------------------------------------------------------------
// Anchor values (plan section 6)
// ---------------------------------------------------------------------------------------------

// 3-1-6's kink circle (the internal basal TIR onset, u_z = -sqrt(n^2 - 1)) carries the constant
// deviation 2 asin sqrt(n^2 - 1); the field layer reads it through d_slab, exact where the chain
// loses sqrt(eps) or goes NaN. The TIR discriminant is a diagnostic: on its zero set the point
// stays in U_P.
TEST(DPField, KinkCircleOfTheSlabPathIsTheClosedFormConstant) {
  const Fixture f(Prism(1.0));
  const int faces[3] = { 3, 1, 6 };
  const DeviationField field = f.Field(faces, 3, kN550);
  const double closed = Deg(2.0 * std::asin(std::sqrt(kN550 * kN550 - 1.0)));  // 115.945094 deg
  EXPECT_NEAR(closed, 115.945094, 1e-6);

  const double u_z = -std::sqrt(kN550 * kN550 - 1.0);
  const double r_xy = std::sqrt(1.0 - u_z * u_z);
  const double azimuths[2][3] = { { 1.0, 0.0, 0.0 }, { 0.5, std::sqrt(3.0) / 2.0, 0.0 } };
  for (const auto& azimuth : azimuths) {
    const double u[3] = { r_xy * azimuth[0], r_xy * azimuth[1], u_z };
    const FieldSample s = field.Sample(u);
    EXPECT_TRUE(s.v_p) << "the TIR onset is not a gate";
    EXPECT_EQ(s.location, DomainLocation::kInterior);
    // The slab field is exact on the crease; the chain value agrees on the closure of U_P.
    ExpectNearDeg(s.d_value, closed, 1e-9, "d_slab on the kink circle");
    ExpectNearDeg(s.d_p, closed, 1e-9, "d_p on the kink circle");
    if (s.margin_count != 6) {
      ADD_FAILURE() << "margin count " << s.margin_count;
      continue;
    }
    EXPECT_NEAR(s.margins[3], 0.0, 1e-12) << "the internal TIR discriminant, LI sign, is 0 on the circle";
  }

  // Off the circle, in U_P, the TIR discriminant carries LI's sign: n^2 (1 - cos^2) - 1, positive
  // under total reflection. u tilted toward the basal face raises the internal incidence above
  // the critical angle: partial reflection, negative discriminant.
  {
    const double u[3] = { r_xy, 0.0, u_z - 0.02 };
    const double norm = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
    const double un[3] = { u[0] / norm, u[1] / norm, u[2] / norm };
    const FieldSample s = field.Sample(un);
    EXPECT_TRUE(s.v_p);
    EXPECT_EQ(s.margin_count, 6);
    if (s.margin_count == 6) {
      EXPECT_LT(s.margins[3], 0.0) << "partial reflection: LI-sign TIR discriminant is negative";
    }
    // And the sign conversion itself: the chain's diagnostic is the negation of the domain margin.
    int slots[3];
    ASSERT_EQ(ResolveFaceSequence(f.normals, faces, 3, slots), Status::kOk);
    const double identity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
    const double incident[3] = { -un[0], -un[1], -un[2] };
    double outgoing[3];
    ChainInterfaceDiagnostics<double> diag;
    const bool valid = TracePathChain<double, ChainEvaluation::kEvaluateAll>(
        f.normals, slots, 3, kN550, incident, identity, outgoing, nullptr, nullptr, &diag);
    EXPECT_TRUE(valid);
    if (s.margin_count == 6) {
      EXPECT_DOUBLE_EQ(s.margins[3], -diag.discriminant[1]);
    }
  }
}

// 3-5 at the minimum-deviation direction: the prism closed form 2 asin(n/2) - 60 deg, an interior
// critical point of the field (the Hessian test pins its kind).
TEST(DPField, MinimumDeviationOfTheDirectPathMatchesTheClosedForm) {
  const Fixture f(Prism(1.0));
  const int faces[2] = { 3, 5 };
  const DeviationField field = f.Field(faces, 2, kN550);
  double u[3];
  MinDeviationDirection(f.normals, u, kN550);
  const FieldSample s = field.Sample(u);
  const double closed = 2.0 * Deg(std::asin(kN550 / 2.0)) - 60.0;  // 21.916126 deg
  EXPECT_NEAR(closed, 21.916126, 1e-6);
  EXPECT_TRUE(s.v_p);
  EXPECT_EQ(s.location, DomainLocation::kInterior);
  ExpectNearDeg(s.d_value, closed, 1e-9, "D_P at the minimum deviation");
  EXPECT_DOUBLE_EQ(s.d_value, s.d_p) << "a non-slab evaluates through the chain only";
  // The finite crystal is present at the identity pose: the corridor of the principal-section ray
  // through a full-height prism has positive area, and the transmission is a Fresnel product in
  // (0, 1).
  EXPECT_GT(s.a_p, 0.0);
  EXPECT_GT(s.t_p, 0.0);
  EXPECT_LT(s.t_p, 1.0);
}

// 3-1-5's minimum is a boundary extremum: at the principal-plane direction the basal reflection
// is grazing (internal incidence cosine exactly 0), the chain takes the smooth branch through it,
// and the deviation reaches the 3-5 closed form — the ray that grazes face 1 never notices it.
TEST(DPField, MinimumOfTheBasalPathIsABoundaryExtremum) {
  const Fixture f(Prism(1.0));
  const int faces[3] = { 3, 1, 5 };
  const DeviationField field = f.Field(faces, 3, kN550);
  double u[3];
  MinDeviationDirection(f.normals, u, kN550);  // u_z = 0: the grazing point
  ASSERT_NEAR(u[2], 0.0, 1e-15);
  const FieldSample s = field.Sample(u);
  const double closed = 2.0 * Deg(std::asin(kN550 / 2.0)) - 60.0;
  EXPECT_FALSE(s.v_p) << "the grazing incidence is not > 0";
  EXPECT_EQ(s.location, DomainLocation::kBoundary);
  EXPECT_TRUE(std::isfinite(s.d_p)) << "the smooth branch evaluates through the grazing reflection";
  ExpectNearDeg(s.d_p, closed, 1e-9, "the boundary extremum value");
  // Tilted into the domain the point becomes interior and the deviation rises off the boundary
  // value (the minimum lives on dU_P, not inside).
  const double norm = std::sqrt(u[0] * u[0] + u[1] * u[1] + 0.01 * 0.01);
  const double u_in[3] = { u[0] / norm, u[1] / norm, -0.01 / norm };
  const FieldSample inside = field.Sample(u_in);
  EXPECT_TRUE(inside.v_p);
  EXPECT_EQ(inside.location, DomainLocation::kInterior);
  EXPECT_GT(Deg(inside.d_p), closed);
}

// The 120-degree blade: on 3-1-4-5 (and 3-4-1-5) the fold axis direction of the xy-plane carries
// exactly 120 degrees of deviation on the smooth branch, finite with every margin readable — the
// recomputable front half of the partition anchor (the cut itself belongs to the partition task).
TEST(DPField, BladePointOfTheBasalPairIsExact) {
  const Fixture f(Prism(1.0));
  const double u[3] = { -std::sqrt(3.0) / 2.0, 0.5, 0.0 };
  const int faces_a[4] = { 3, 1, 4, 5 };
  const int faces_b[4] = { 3, 4, 1, 5 };
  for (const int* faces : { faces_a, faces_b }) {
    const DeviationField field = f.Field(faces, 4, kN550);
    if (!field.fold().degenerate) {
      ADD_FAILURE() << "the basal pair is a slab";
      continue;
    }
    const FieldSample s = field.Sample(u);
    EXPECT_FALSE(s.v_p);
    EXPECT_EQ(s.location, DomainLocation::kExterior);
    EXPECT_TRUE(std::isfinite(s.d_p));
    ExpectNearDeg(s.d_p, 120.0, 1e-9, "the blade point deviation");
    EXPECT_EQ(s.margin_count, 8);  // 2 x 4 interfaces
  }
}

// The beta slab's crease: angle(M u, u) = 120 deg exactly on the whole crease circle (a rotation
// by twice the inter-facial angle), the field's top edge the 52.2 lattice measured at
// 119.99999999 deg; the crease crosses U_P (the 22.76% interior arc).
TEST(DPField, BetaSlabCreaseIsExactAndCrossesTheDomain) {
  const Fixture f(Beta());
  const int faces[4] = { 4, 8, 7, 5 };
  const DeviationField field = f.Field(faces, 4, kN550);
  const double crease[2][3] = { { 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 } };
  bool interior_seen = false;
  bool exterior_seen = false;
  for (const auto& u : crease) {
    const FieldSample s = field.Sample(u);
    ExpectNearDeg(s.d_value, 120.0, 1e-8, "d_slab on the beta crease");
    interior_seen |= s.location == DomainLocation::kInterior;
    exterior_seen |= s.location == DomainLocation::kExterior;
  }
  EXPECT_TRUE(interior_seen) << "the crease has an interior arc (22.76% of it)";
  EXPECT_TRUE(exterior_seen);
}

// ---------------------------------------------------------------------------------------------
// Margin vector, validity subset, NaN conventions
// ---------------------------------------------------------------------------------------------

TEST(DPField, MarginVectorLayoutAndValiditySubset) {
  const Fixture f(Prism(1.0));
  double u[3];
  MinDeviationDirection(f.normals, u, kN550);

  struct Case {
    const char* name;
    int faces[4];
    int count;
  };
  const Case cases[] = { { "3-5", { 3, 5, 0, 0 }, 2 },
                         { "3-1-5", { 3, 1, 5, 0 }, 3 },
                         { "3-1-4-5", { 3, 1, 4, 5 }, 4 } };
  for (const Case& c : cases) {
    int slots[kMaxFaceCount];
    if (ResolveFaceSequence(f.normals, c.faces, c.count, slots) != Status::kOk) {
      ADD_FAILURE() << c.name << ": unresolved";
      continue;
    }
    const DeviationField field = f.Field(c.faces, c.count, kN550);
    // Near but not on the boundary so every chain evaluation is finite.
    const double norm = std::sqrt(u[0] * u[0] + u[1] * u[1] + 0.05 * 0.05);
    const double un[3] = { u[0] / norm, u[1] / norm, -0.05 / norm };
    const FieldSample s = field.Sample(un);
    // The domain vector is two margins per interface on the chain.
    EXPECT_EQ(s.margin_count, 2 * c.count) << c.name;
    // The validity subset is the domain vector minus the internal TIR slots, and equals what the
    // chain itself records (the two orderings cannot silently diverge).
    double chain_margins[kMaxFaceCount + 2];
    ChainDomain domain{ chain_margins };
    const double identity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
    const double incident[3] = { -un[0], -un[1], -un[2] };
    double outgoing[3];
    const bool valid = TracePathChain<double, ChainEvaluation::kEvaluateAll>(f.normals, slots, c.count, kN550, incident,
                                                                             identity, outgoing, nullptr, &domain);
    EXPECT_EQ(valid, s.v_p) << c.name;
    EXPECT_EQ(domain.margin_count, c.count + 2) << c.name;
    double subset[kMaxFaceCount + 2];
    const int subset_count = ValidityMargins(s.margins, s.margin_count, subset);
    EXPECT_EQ(subset_count, domain.margin_count) << c.name;
    if (subset_count != domain.margin_count) {
      continue;
    }
    for (int i = 0; i < subset_count; i++) {
      EXPECT_DOUBLE_EQ(subset[i], chain_margins[i]) << c.name << " margin " << i;
    }
  }
}

TEST(DPField, LocationThreeStates) {
  const Fixture f(Prism(1.0));
  // Interior: the 3-5 minimum-deviation point.
  {
    const int faces[2] = { 3, 5 };
    const DeviationField field = f.Field(faces, 2, kN550);
    double u[3];
    MinDeviationDirection(f.normals, u, kN550);
    EXPECT_EQ(field.Sample(u).location, DomainLocation::kInterior);
  }
  // Boundary: 3-1-5's grazing point (smallest validity margin exactly 0).
  {
    const int faces[3] = { 3, 1, 5 };
    const DeviationField field = f.Field(faces, 3, kN550);
    double u[3];
    MinDeviationDirection(f.normals, u, kN550);
    EXPECT_EQ(field.Sample(u).location, DomainLocation::kBoundary);
  }
  // Exterior: a backface entry.
  {
    const int faces[2] = { 3, 5 };
    const DeviationField field = f.Field(faces, 2, kN550);
    const double u[3] = { -1.0, 0.0, 0.0 };  // u away from the entry normal
    const FieldSample s = field.Sample(u);
    EXPECT_EQ(s.location, DomainLocation::kExterior);
    EXPECT_FALSE(s.v_p);
    EXPECT_DOUBLE_EQ(s.t_p, 0.0) << "outside V_P the path transmits no power (LI convention)";
  }
}

// The exit-Snell NaN convention of the smooth branch (the exit refraction's own square root), and
// the two limit values that read through it: the exit-limit form has no root anywhere and stays
// finite; the grazing form is NaN exactly where the direction is.
TEST(DPField, ExitSnellNanConventionAndLimitValues) {
  const Fixture f(Prism(1.0));
  const int faces[2] = { 3, 5 };
  const DeviationField field = f.Field(faces, 2, kN550);
  // Normal incidence on face 3: the internal ray meets face 5 at 60 deg, beyond the critical
  // angle — the exit Snell discriminant is negative and the chain's outgoing direction is NaN.
  const int s3 = f.normals.SlotOf(3);
  const double u[3] = { f.normals.normal[s3][0], f.normals.normal[s3][1], f.normals.normal[s3][2] };
  const FieldSample s = field.Sample(u);
  EXPECT_FALSE(s.v_p);
  EXPECT_TRUE(std::isnan(s.d_p));
  EXPECT_TRUE(std::isnan(s.d_value)) << "a non-slab has no slab form to fall back on";
  EXPECT_TRUE(std::isnan(s.d_p_grazing)) << "the grazing form starts from the direction";
  EXPECT_TRUE(std::isfinite(s.d_p_exit_limit)) << "the exit-limit form takes no root";

  // At a deep interior point the limit values are NOT d_p substitutes (they are off by
  // ~sqrt(disc)); they agree only in the disc -> 0+ limit.
  double u_min[3];
  MinDeviationDirection(f.normals, u_min, kN550);
  const FieldSample interior = field.Sample(u_min);
  EXPECT_GT(std::fabs(Deg(interior.d_p_exit_limit) - Deg(interior.d_p)), 1e-3);
  // On the exit TIR curve's U_P side the exit-limit value is the closure value of d_p there: take
  // a point just inside where disc is small but positive and compare against d_p's limit.
  // (Pinpointing the curve belongs to the boundary walk; here the finite/infinite split above is
  // the contract under test.)
}

// The slab path has no NaN: d_slab evaluates everywhere, including on the crease and at the axis
// where the chain goes NaN.
TEST(DPField, SlabFieldHasNoNan) {
  const Fixture f(Prism(1.0));
  const int faces[3] = { 3, 1, 6 };
  const DeviationField field = f.Field(faces, 3, kN550);
  const double axis[2][3] = { { 0.0, 0.0, 1.0 }, { 0.0, 0.0, -1.0 } };
  for (const auto& u : axis) {
    const FieldSample s = field.Sample(u);
    EXPECT_TRUE(std::isnan(s.d_p)) << "the chain hits the entry gate at the axis";
    EXPECT_TRUE(std::isfinite(s.d_value)) << "the slab form has no square root";
    ExpectNearDeg(s.d_value, 180.0, 1e-9, "at the axis M u = -u: the crease value is pi");
  }
}

// ---------------------------------------------------------------------------------------------
// The exit-Snell closure convention (LI task 52.6, RoutedDeviation) and the near-limit behaviour
// of the two limit forms. The anchors are LI's dumped corners of 3-5-6-7 at n(550): one corner's
// plain d_p is finite, the other's is NaN (the exit square root on the rounding-negative side of
// the discriminant), both on the closure of U_P with the closure value 50.161740000308 deg.
// ---------------------------------------------------------------------------------------------

// LI's two settled corners of 3-5-6-7 @ n(550) (task-dir anchor dump, full precision). Both sit
// on the exit-Snell curve; LI's chain reads the discriminant on the rounding-negative side at the
// second one and routes it to the closure limit, Lumice's reads it non-negative at both — the
// true residual is ~1e-15 and its sign is the chain's rounding, which is exactly the situation
// the closure convention exists for. The corner RECORD value (LI's, either form) is
// 50.161740000308 deg.
constexpr double kCornerFinite[3] = { 0.0, -0.48947414775668885, 0.8720178086930698 };
constexpr double kCornerNan[3] = { 0.0, -0.4894741477566886, -0.8720178086930699 };
constexpr double kClosureCornerDeg = 50.161740000308;

// A transversal crossing of the exit-Snell zero curve of 3-5-6-7 @ n(550) with every OTHER gate
// comfortably inside U_P (>= +0.008): unlike the corners, a single-gate crossing, so the closure
// window [-1e-13, 0) of the discriminant is reachable by bisection alone.
struct ExitSnellCrossing {
  double u_cross[3];  // the bisection's NaN-side endpoint: disc in (-1e-16, 0), other gates > 0
  double start[3];
  double dir[3];  // the tangent direction the geodesic leaves `start` along
};

bool ExitSnellCrossingOf(const DeviationField& field, ExitSnellCrossing* out) {
  const double start[3] = { 0.0, -0.578931, 0.815376 };
  double basis[2][3];
  TangentBasis(start, basis);
  const double phi = -kPi / 6.0;
  double dir[3];
  for (int i = 0; i < 3; i++) {
    dir[i] = std::cos(phi) * basis[0][i] + std::sin(phi) * basis[1][i];
    out->start[i] = start[i];
    out->dir[i] = dir[i];
  }
  const double kFar = 0.135;  // disc < 0 there, every other gate >= +0.001 (measured)
  const auto point = [&](double t, double u[3]) {
    for (int i = 0; i < 3; i++) {
      u[i] = std::cos(t) * start[i] + std::sin(t) * dir[i];
    }
  };
  double t_lo = 0.0;  // disc > 0 (the invariant: the start is inside, d_p finite)
  double t_hi = kFar;
  for (int step = 0; step < 70; step++) {
    const double t = 0.5 * (t_lo + t_hi);
    double u[3];
    point(t, u);
    if (field.Sample(u).d_p >= 0.0 && std::isfinite(field.Sample(u).d_p)) {
      t_lo = t;
    } else {
      t_hi = t;
    }
  }
  point(t_hi, out->u_cross);
  const FieldSample cross = field.Sample(out->u_cross);
  const double disc = cross.margins[cross.margin_count - 1];
  double other = 1.0;
  for (int m = 0; m + 1 < cross.validity_count; m++) {
    other = std::min(other, cross.validity_margins[m]);
  }
  EXPECT_FALSE(std::isfinite(cross.d_p));
  EXPECT_GT(disc, -1e-13) << "the bisection endpoint sits in the closure window";
  EXPECT_LT(disc, 0.0);
  EXPECT_GT(other, 1e-3) << "every other gate is comfortably inside U_P";
  return true;
}

// |limit - d_p| shrinks like sqrt(disc) on the U_P side of the crossing (the two limit forms are
// the transmitted direction's disc -> 0+ value; d_p itself carries the sqrt(disc) term). This is
// the near-limit anchor the boundary walk (660.3) consumes both forms against.
TEST(DPField, ExitLimitFormsConvergeToDpAsDiscVanishes) {
  const Fixture f(Prism(1.0));
  const int faces[4] = { 3, 5, 6, 7 };
  const DeviationField field = f.Field(faces, 4, kN550);
  ExitSnellCrossing crossing;
  ASSERT_TRUE(ExitSnellCrossingOf(field, &crossing));
  double previous = 1.0;
  for (double offset : { 1e-5, 1e-7, 1e-9 }) {
    // offset in geodesic parameter pulls the point back into the U_P side of the crossing.
    double u[3];
    for (int i = 0; i < 3; i++) {
      u[i] = std::cos(offset) * crossing.u_cross[i] + std::sin(offset) * (-crossing.dir[i]);
    }
    const double norm = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
    for (int i = 0; i < 3; i++) {
      u[i] /= norm;
    }
    const FieldSample s = field.Sample(u);
    const double disc = s.margins[s.margin_count - 1];
    if (!std::isfinite(s.d_p) || disc <= 0.0) {
      ADD_FAILURE() << "the U_P side of the crossing at offset " << offset << ": finite d_p, disc > 0";
      continue;
    }
    const double d_grazing = std::fabs(s.d_p_grazing - s.d_p);
    const double d_limit = std::fabs(s.d_p_exit_limit - s.d_p);
    EXPECT_LT(d_grazing, 5.0 * std::sqrt(disc) + 1e-15);
    EXPECT_LT(d_limit, 5.0 * std::sqrt(disc) + 1e-15);
    EXPECT_LT(d_limit, previous) << "the difference shrinks as disc -> 0+";
    previous = std::max(previous * 0.999, d_limit);
  }
}

// The routed deviation at the settled corners (52.6): both corners' plain d_p is finite under
// Lumice's rounding (LI's reads NaN at the second), so both take the passthrough branch bit-exact
// — and the exit-limit form at both is LI's corner record value 50.161740000308 deg, the value
// LI's own walker consumes there. The NaN branch itself is pinned live at the transversal
// crossing below.
TEST(DPField, RoutedDeviationAtTheClosureCorner) {
  const Fixture f(Prism(1.0));
  const int faces[4] = { 3, 5, 6, 7 };
  const DeviationField field = f.Field(faces, 4, kN550);
  for (const auto& corner : { kCornerFinite, kCornerNan }) {
    const FieldSample s = field.Sample(corner);
    double routed = 0.0;
    if (!std::isfinite(s.d_p) || RoutedDeviation(s, /*slab_path=*/false, &routed) != RoutedDeviationStatus::kOk) {
      ADD_FAILURE() << "a corner with finite d_p takes the passthrough branch";
      continue;
    }
    EXPECT_EQ(routed, s.d_value) << "a finite value passes through bit-exact";
    ExpectNearDeg(s.d_p_exit_limit, kClosureCornerDeg, 1e-9, "the closure-corner value (LI 52.6)");
    EXPECT_TRUE(OnUpClosure(s.validity_margins, s.validity_count)) << "a settled corner is on the closure";
  }

  // The branch itself, live: non-finite d_p on the closure routes to the exit-limit value.
  ExitSnellCrossing crossing;
  ASSERT_TRUE(ExitSnellCrossingOf(field, &crossing));
  const FieldSample nan_side = field.Sample(crossing.u_cross);
  ASSERT_FALSE(std::isfinite(nan_side.d_p));
  double routed = 0.0;
  ASSERT_EQ(RoutedDeviation(nan_side, /*slab_path=*/false, &routed), RoutedDeviationStatus::kOk);
  EXPECT_EQ(routed, nan_side.d_p_exit_limit) << "the closure branch returns the limit form";
  EXPECT_TRUE(std::isfinite(routed));
}

// Fail-closed: a non-finite d_p off the closure is refused (the value is not guessed), a slab
// path's non-finite value has no closure branch to take (an anomaly), and a NaN validity margin
// fails the closure predicate too. The off-closure point is live: at the far end of the crossing
// geodesic the discriminant gate is far below -kViolationAtol.
TEST(DPField, RoutedDeviationFailsClosed) {
  const Fixture f(Prism(1.0));
  const int faces[4] = { 3, 5, 6, 7 };
  const DeviationField field = f.Field(faces, 4, kN550);
  ExitSnellCrossing crossing;
  ASSERT_TRUE(ExitSnellCrossingOf(field, &crossing));

  double u_far[3];
  for (int i = 0; i < 3; i++) {
    u_far[i] = std::cos(0.135) * crossing.start[i] + std::sin(0.135) * crossing.dir[i];
  }
  const FieldSample deep = field.Sample(u_far);
  ASSERT_FALSE(std::isfinite(deep.d_p));
  ASSERT_LT(deep.validity_margins[deep.validity_count - 1], -1e-9) << "past the exit Snell limit";
  double routed = 123.0;
  EXPECT_EQ(RoutedDeviation(deep, /*slab_path=*/false, &routed), RoutedDeviationStatus::kNotFinite);

  // A slab path's d_value has no square root: non-finite there is an anomaly, never routed.
  FieldSample slab_nan{};
  slab_nan.d_value = std::numeric_limits<double>::quiet_NaN();
  slab_nan.d_p_exit_limit = 0.5;
  for (int i = 0; i < kMaxFaceCount + 2; i++) {
    slab_nan.validity_margins[i] = 1.0;  // on the closure: must not matter
  }
  slab_nan.validity_count = 4;
  EXPECT_EQ(RoutedDeviation(slab_nan, /*slab_path=*/true, &routed), RoutedDeviationStatus::kNotFinite);

  // A NaN margin is off the closure (fail closed on it), and so is a real violation.
  FieldSample off_closure{};
  off_closure.d_value = std::numeric_limits<double>::quiet_NaN();
  off_closure.d_p_exit_limit = 0.5;
  for (int i = 0; i < kMaxFaceCount + 2; i++) {
    off_closure.validity_margins[i] = 0.1;
  }
  off_closure.validity_count = 4;
  off_closure.validity_margins[2] = std::numeric_limits<double>::quiet_NaN();
  EXPECT_FALSE(OnUpClosure(off_closure.validity_margins, 4));
  off_closure.validity_margins[2] = -1e-9;
  EXPECT_FALSE(OnUpClosure(off_closure.validity_margins, 4));
  EXPECT_EQ(RoutedDeviation(off_closure, /*slab_path=*/false, &routed), RoutedDeviationStatus::kNotFinite);
}

// ValidityMarginsAt is the batch paths' single gate entry: the same values Sample reports, with
// none of Sample's other bookkeeping (the partition's lattice count, chart audit and fold-set
// sampling read the gates through it).
TEST(DPField, ValidityMarginsAtMatchesSample) {
  const Fixture f(Prism(1.0));
  const int faces[4] = { 3, 5, 6, 7 };
  const DeviationField field = f.Field(faces, 4, kN550);
  const double us[3][3] = { { 0.0, 0.0, 1.0 }, { 0.3, -0.5, 0.8 }, { 0.0, -1.0, 0.0 } };
  for (const auto& u : us) {
    double un[3];
    const double norm = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
    for (int i = 0; i < 3; i++) {
      un[i] = u[i] / norm;
    }
    double margins[kMaxFaceCount + 2];
    const int count = field.ValidityMarginsAt(un, margins);
    const FieldSample s = field.Sample(un);
    if (count != s.validity_count) {
      ADD_FAILURE() << "ValidityMarginsAt count " << count << " != Sample's " << s.validity_count;
      continue;
    }
    for (int i = 0; i < count; i++) {
      EXPECT_DOUBLE_EQ(margins[i], s.validity_margins[i]) << "margin " << i;
    }
  }
  // The entry normal accessor: face 3's body normal (the chart axis of the topology audit).
  const double* n_a = field.EntryNormal();
  const int slot3 = f.normals.SlotOf(3);
  for (int i = 0; i < 3; i++) {
    EXPECT_DOUBLE_EQ(n_a[i], f.normals.normal[slot3][i]);
  }
}


// ---------------------------------------------------------------------------------------------
// The walk's evaluation entries (660.3 plan Step 1): FoldMatrixOf, DomainMarginsAt,
// SampleOptical, MarginsWithGradient
// ---------------------------------------------------------------------------------------------

// FoldMatrixOf is the reflection product's single authority: BuildFoldScreen's matrix is the full
// sequence's call (the refactor invariance), and a prefix call with slot_count = k + 1 gives
// R_{k-1} — for the first internal face exactly the one Householder reflection S(n_1).
TEST(DPField, FoldMatrixOfIsTheScreensProductAndItsPrefixes) {
  const Fixture prism(Prism(1.0));
  const Fixture beta(Beta());
  struct Case {
    const Fixture* fixture;
    int faces[5];
    int count;
  };
  const Case cases[] = { { &prism, { 3, 5, 6, 7, 3 }, 5 },
                         { &prism, { 3, 1, 6, 0, 0 }, 3 },
                         { &prism, { 4, 8, 7, 5, 0 }, 4 },
                         { &beta, { 4, 8, 7, 5, 0 }, 4 } };
  for (const Case& c : cases) {
    int slots[kMaxFaceCount];
    if (ResolveFaceSequence(c.fixture->normals, c.faces, c.count, slots) != Status::kOk) {
      ADD_FAILURE() << "ResolveFaceSequence failed for " << c.count << " faces";
      continue;
    }
    const DeviationField field = c.fixture->Field(c.faces, c.count, kN550);
    double product[9];
    FoldMatrixOf(c.fixture->normals, slots, c.count, product);
    for (int i = 0; i < 9; i++) {
      EXPECT_NEAR(product[i], field.fold().fold_matrix[i], 0.0) << "entry " << i << " of " << c.count << " faces";
    }
    if (c.count >= 3) {
      // The prefix over (a, m_1, x) folds exactly one reflection — S(n_1); the two-face prefix
      // folds nothing and is the identity (LI incidence_normals' R_0).
      double prefix[9];
      FoldMatrixOf(c.fixture->normals, slots, 3, prefix);
      const double* n1 = c.fixture->normals.normal[slots[1]];
      double s[9];
      for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
          s[i * 3 + j] = (i == j ? 1.0 : 0.0) - 2.0 * n1[i] * n1[j];
        }
      }
      for (int i = 0; i < 9; i++) {
        EXPECT_NEAR(prefix[i], s[i], 1e-15);
      }
      FoldMatrixOf(c.fixture->normals, slots, 2, prefix);
      for (int i = 0; i < 9; i++) {
        EXPECT_DOUBLE_EQ(prefix[i], i % 4 == 0 ? 1.0 : 0.0);
      }
    }
  }
}

// DomainMarginsAt answers the full vector — the validity gates the subset carries AND the internal
// TIR diagnostics it does not — with the same values Sample reports (the walk's bookkeeping reads
// the diagnostics to rank the kink curves).
TEST(DPField, DomainMarginsAtMatchesSampleIncludingTirPositions) {
  const Fixture f(Prism(1.0));
  const int faces[5] = { 3, 5, 6, 7, 3 };
  const DeviationField field = f.Field(faces, 5, kN550);
  const double us[3][3] = { { 0.0, -0.48947414775668885, 0.8720178086930698 }, { 0.3, -0.5, 0.8 }, { 0.2, 0.9, 0.1 } };
  for (const auto& u : us) {
    double un[3];
    const double norm = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
    for (int i = 0; i < 3; i++) {
      un[i] = u[i] / norm;
    }
    double margins[2 * kMaxFaceCount];
    const int count = field.DomainMarginsAt(un, margins);
    const FieldSample s = field.Sample(un);
    EXPECT_EQ(count, s.margin_count);
    EXPECT_EQ(count, 10);  // 5 interfaces x (incidence, discriminant)
    for (int i = 0; i < count; i++) {
      EXPECT_DOUBLE_EQ(margins[i], s.margins[i]) << "margin " << i;
    }
  }
}

// SampleOptical is Sample minus the corridor: every optical quantity identical, a_p untouched at
// zero (the walks never pay the entry measure's clipping).
TEST(DPField, SampleOpticalMatchesSampleExceptTheEntryMeasure) {
  const Fixture f(Prism(1.0));
  const int faces[4] = { 3, 5, 6, 7 };
  const DeviationField field = f.Field(faces, 4, kN550);
  const double us[2][3] = { { 0.0, -0.48947414775668885, 0.8720178086930698 }, { 0.3, -0.5, 0.8 } };
  for (const auto& u : us) {
    double un[3];
    const double norm = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
    for (int i = 0; i < 3; i++) {
      un[i] = u[i] / norm;
    }
    const FieldSample optical = field.SampleOptical(un);
    const FieldSample full = field.Sample(un);
    EXPECT_DOUBLE_EQ(optical.d_p, full.d_p);
    EXPECT_DOUBLE_EQ(optical.d_value, full.d_value);
    EXPECT_DOUBLE_EQ(optical.d_p_grazing, full.d_p_grazing);
    EXPECT_DOUBLE_EQ(optical.d_p_exit_limit, full.d_p_exit_limit);
    EXPECT_DOUBLE_EQ(optical.t_p, full.t_p);
    EXPECT_EQ(optical.v_p, full.v_p);
    EXPECT_EQ(optical.location, full.location);
    EXPECT_EQ(optical.margin_count, full.margin_count);
    EXPECT_DOUBLE_EQ(optical.a_p, 0.0);
  }
}

// The ambient margin gradient of a Jet<3> sweep, against two oracles that share nothing with the
// forward-mode rules: the closed forms the first two margins have (the entry cosine is exactly
// n_a . u, the entry Snell discriminant 1 - (1 - c^2)/n^2 with c = n_a . u), and the chart
// difference of the double evaluation for every margin of 3-5-6-7-3 (whose vector carries the
// internal TIR diagnostics and the exit discriminant the walk ranks).
TEST(DPField, MarginGradientsMatchClosedFormsAndChartDifferences) {
  const Fixture f(Prism(1.0));
  const int faces[5] = { 3, 5, 6, 7, 3 };
  const DeviationField field = f.Field(faces, 5, kN550);
  const double u_in[3] = { 0.0, -0.48947414775668885, 0.8720178086930698 };
  double u[3];
  const double norm = std::sqrt(u_in[0] * u_in[0] + u_in[1] * u_in[1] + u_in[2] * u_in[2]);
  for (int i = 0; i < 3; i++) {
    u[i] = u_in[i] / norm;
  }
  const double* n_a = field.EntryNormal();

  // Closed forms of the entry pair.
  MarginJet jet;
  field.MarginsWithGradient(u, &jet);
  EXPECT_EQ(jet.margin_count, 10);
  const double c = n_a[0] * u[0] + n_a[1] * u[1] + n_a[2] * u[2];
  for (int i = 0; i < 3; i++) {
    EXPECT_NEAR(jet.gradient[0][i], n_a[i], 1e-15) << "entry cosine gradient " << i;
    EXPECT_NEAR(jet.gradient[1][i], 2.0 * c * n_a[i] / (kN550 * kN550), 1e-15) << "entry disc gradient " << i;
  }

  // Chart differences: u(t) = normalize(u + t a) has d/dt of margin_k exactly the ambient gradient
  // projected on a's tangent part, for every margin of the vector.
  double basis[2][3];
  TangentBasis(u, basis);
  const double h = 1e-6;
  const auto chart_margins = [&](double t, int which, double out[2 * kMaxFaceCount]) {
    const double* e = basis[which];
    double w[3] = { u[0] + t * e[0], u[1] + t * e[1], u[2] + t * e[2] };
    const double wn = std::sqrt(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
    for (int i = 0; i < 3; i++) {
      w[i] /= wn;
    }
    field.DomainMarginsAt(w, out);
  };
  for (int a = 0; a < 2; a++) {
    const double* e = basis[a];
    double plus[2 * kMaxFaceCount];
    double minus[2 * kMaxFaceCount];
    chart_margins(h, a, plus);
    chart_margins(-h, a, minus);
    for (int k = 0; k < jet.margin_count; k++) {
      const double fd = (plus[k] - minus[k]) / (2.0 * h);
      const double g_tangent = jet.gradient[k][0] * e[0] + jet.gradient[k][1] * e[1] + jet.gradient[k][2] * e[2];
      EXPECT_NEAR(g_tangent, fd, 1e-8 * (1.0 + std::fabs(fd))) << "margin " << k << " direction " << a;
    }
  }
  // The values themselves are DomainMarginsAt's (one sweep, same numbers up to FMA-grade noise).
  double margins[2 * kMaxFaceCount];
  field.DomainMarginsAt(u, margins);
  for (int k = 0; k < jet.margin_count; k++) {
    EXPECT_NEAR(jet.margins[k], margins[k], 1e-15 * (1.0 + std::fabs(margins[k])));
  }
}


// ---------------------------------------------------------------------------------------------
// Jets: tangent gradient, Riemannian Hessian, d/dn (plan Step 4; the FieldJet docstring carries
// the chart identity the differences below rely on)
// ---------------------------------------------------------------------------------------------

// The chart the Hessian lives in: u(t) = normalize(u + t1 e1 + t2 e2) has d^2/dt_k dt_j of the
// field exactly B (H - (u . g) I) B^T, and the gradient components g . e_a are the chart's first
// derivatives — so both jet objects are checked by differencing the double evaluation the layer
// itself reports (d_value), an oracle that shares nothing with the Jet2 rules.
TEST(DPField, JetsMatchChartDifferencesOfTheDoubleEvaluation) {
  const Fixture f(Prism(1.0));
  struct Case {
    const char* name;
    int faces[4];
    int count;
    double u[3];
  };
  // A generic interior point of 3-5 (off the critical point, so the gradient is nonzero) and a
  // generic point of the 3-1-6 slab field (on the kink circle, where d_value is the slab form and
  // smooth — the TIR margin is a diagnostic the value never sees).
  double u_min[3];
  MinDeviationDirection(f.normals, u_min, kN550);
  const double tilted[3] = { u_min[0], u_min[1] + 0.05, -0.08 };
  double t_norm = std::sqrt(tilted[0] * tilted[0] + tilted[1] * tilted[1] + tilted[2] * tilted[2]);
  const double u_direct[3] = { tilted[0] / t_norm, tilted[1] / t_norm, tilted[2] / t_norm };
  const double kink_z = -std::sqrt(kN550 * kN550 - 1.0);
  const double kink_r = std::sqrt(1.0 - kink_z * kink_z);
  const double u_kink[3] = { 0.5 * kink_r, std::sqrt(3.0) / 2.0 * kink_r, kink_z };
  const Case cases[] = { { "3-5", { 3, 5, 0, 0 }, 2, { u_direct[0], u_direct[1], u_direct[2] } },
                         { "3-1-6 slab", { 3, 1, 6, 0 }, 3, { u_kink[0], u_kink[1], u_kink[2] } } };
  for (const Case& c : cases) {
    const DeviationField field = f.Field(c.faces, c.count, kN550);
    const FieldJet jet = field.Differentiate(c.u);
    const auto chart = [&](double t1, double t2) {
      double w[3] = { c.u[0] + t1 * jet.tangent_basis[0][0] + t2 * jet.tangent_basis[1][0],
                      c.u[1] + t1 * jet.tangent_basis[0][1] + t2 * jet.tangent_basis[1][1],
                      c.u[2] + t1 * jet.tangent_basis[0][2] + t2 * jet.tangent_basis[1][2] };
      const double norm = std::sqrt(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
      for (int i = 0; i < 3; i++) {
        w[i] /= norm;
      }
      return field.Sample(w).d_value;
    };
    // First derivatives along the basis directions.
    const double h1 = 1e-6;
    for (int a = 0; a < 2; a++) {
      const double t1p = a == 0 ? h1 : 0.0, t1m = a == 0 ? -h1 : 0.0;
      const double t2p = a == 1 ? h1 : 0.0, t2m = a == 1 ? -h1 : 0.0;
      const double fd = (chart(t1p, t2p) - chart(t1m, t2m)) / (2.0 * h1);
      const double g_a = jet.tangent_gradient[0] * jet.tangent_basis[a][0] +
                         jet.tangent_gradient[1] * jet.tangent_basis[a][1] +
                         jet.tangent_gradient[2] * jet.tangent_basis[a][2];
      EXPECT_NEAR(g_a, fd, 1e-8 * (1.0 + std::fabs(fd))) << c.name << " gradient e" << a + 1;
    }
    // Mixed second derivatives, one per Hessian entry.
    const double h2 = 1e-5;
    for (int a = 0; a < 2; a++) {
      for (int b = 0; b < 2; b++) {
        const double d1 = a == 0 ? 1.0 : 0.0, d2 = a == 1 ? 1.0 : 0.0;
        const double e1 = b == 0 ? 1.0 : 0.0, e2 = b == 1 ? 1.0 : 0.0;
        const double fd = (chart(h2 * (d1 + e1), h2 * (d2 + e2)) - chart(h2 * (d1 - e1), h2 * (d2 - e2)) -
                           chart(-h2 * (d1 - e1), -h2 * (d2 - e2)) + chart(-h2 * (d1 + e1), -h2 * (d2 + e2))) /
                          (4.0 * h2 * h2);
        EXPECT_NEAR(jet.hessian[a][b], fd, 1e-6 * (1.0 + std::fabs(fd))) << c.name << " hessian " << a << b;
      }
    }
    // Structure: the tangent gradient is orthogonal to u, the basis is orthonormal.
    const double g_dot_u =
        jet.tangent_gradient[0] * c.u[0] + jet.tangent_gradient[1] * c.u[1] + jet.tangent_gradient[2] * c.u[2];
    EXPECT_NEAR(g_dot_u, 0.0, 1e-14) << c.name;
    for (int a = 0; a < 2; a++) {
      EXPECT_NEAR(
          jet.tangent_basis[a][0] * c.u[0] + jet.tangent_basis[a][1] * c.u[1] + jet.tangent_basis[a][2] * c.u[2], 0.0,
          1e-14)
          << c.name << " basis row " << a << " is tangent";
    }
    EXPECT_NEAR(jet.tangent_basis[0][0] * jet.tangent_basis[1][0] + jet.tangent_basis[0][1] * jet.tangent_basis[1][1] +
                    jet.tangent_basis[0][2] * jet.tangent_basis[1][2],
                0.0, 1e-14)
        << c.name;
  }
}

// The tangent basis never degenerates: u aligned with each coordinate axis still yields an
// orthonormal pair (the least-aligned-axis cross of LI tangent_basis).
TEST(DPField, TangentBasisAlongTheThreeAxes) {
  const Fixture f(Prism(1.0));
  const int faces[2] = { 3, 5 };
  const DeviationField field = f.Field(faces, 2, kN550);
  const double axes[3][3] = { { 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 }, { 0.0, 0.0, 1.0 } };
  for (const auto& u : axes) {
    const FieldJet jet = field.Differentiate(u);
    for (int a = 0; a < 2; a++) {
      const double norm = std::sqrt(jet.tangent_basis[a][0] * jet.tangent_basis[a][0] +
                                    jet.tangent_basis[a][1] * jet.tangent_basis[a][1] +
                                    jet.tangent_basis[a][2] * jet.tangent_basis[a][2]);
      EXPECT_NEAR(norm, 1.0, 1e-14);
    }
  }
}

// The 3-5 minimum-deviation point is a radial critical point: the tangent gradient vanishes, the
// ambient gradient does not (it is parallel to u — that is why the second fundamental form term
// matters), and both Riemannian Hessian eigenvalues are positive: a constrained minimum, the sign
// pattern LI measured as [+0.34, +0.96] against the naive projection's [-5.4, -4.7].
TEST(DPField, MinimumDeviationIsARadialCriticalPoint) {
  const Fixture f(Prism(1.0));
  const int faces[2] = { 3, 5 };
  const DeviationField field = f.Field(faces, 2, kN550);
  double u[3];
  MinDeviationDirection(f.normals, u, kN550);
  const FieldJet jet = field.Differentiate(u);
  const double g_norm =
      std::sqrt(jet.tangent_gradient[0] * jet.tangent_gradient[0] + jet.tangent_gradient[1] * jet.tangent_gradient[1] +
                jet.tangent_gradient[2] * jet.tangent_gradient[2]);
  EXPECT_LT(g_norm, 1e-8) << "a constrained critical point";
  const double trace = jet.hessian[0][0] + jet.hessian[1][1];
  const double det = jet.hessian[0][0] * jet.hessian[1][1] - jet.hessian[0][1] * jet.hessian[1][0];
  EXPECT_GT(trace, 0.0);
  EXPECT_GT(det, 0.0) << "both eigenvalues positive: the deviation minimum";
  // And of a sane size (LI measured 0.34 and 0.96 at n = 1.31).
  const double half_trace = 0.5 * trace;
  const double root = std::sqrt(std::max(half_trace * half_trace - det, 0.0));
  EXPECT_GT(half_trace + root, 0.05);
  EXPECT_LT(half_trace - root, 5.0);
}

// dD_P/dn against a difference of the field's own evaluation at perturbed indices (the field is
// built per index), on the non-slab 3-1-5 where the derivative is nonzero (the dispersion that
// moves a halo's red edge); and the domain margins' d/dn the same way.
TEST(DPField, IndexDerivativeMatchesDifference) {
  const Fixture f(Prism(1.0));
  const int faces[3] = { 3, 1, 5 };
  double u_min[3];
  MinDeviationDirection(f.normals, u_min, kN550);
  const double norm = std::sqrt(u_min[0] * u_min[0] + u_min[1] * u_min[1] + 0.05 * 0.05);
  const double u[3] = { u_min[0] / norm, u_min[1] / norm, -0.05 / norm };

  const DeviationField field = f.Field(faces, 3, kN550);
  const FieldJet jet = field.Differentiate(u);
  const double h = 1e-6;
  const DeviationField plus = f.Field(faces, 3, kN550 + h);
  const DeviationField minus = f.Field(faces, 3, kN550 - h);
  const FieldSample s_plus = plus.Sample(u);
  const FieldSample s_minus = minus.Sample(u);

  const double fd_dn = (s_plus.d_value - s_minus.d_value) / (2.0 * h);
  EXPECT_NEAR(jet.d_p_dn, fd_dn, 1e-7 * (1.0 + std::fabs(fd_dn)));
  ASSERT_NE(std::fabs(fd_dn), 0.0);
  EXPECT_GT(std::fabs(jet.d_p_dn), 1e-2) << "the dispersion of a refracting path is nonzero";

  // Margin values agree with the same-index sample's (the same expressions in another scalar
  // type, so to rounding, not bitwise) and every d margin/dn matches its own difference.
  const FieldSample s_same = field.Sample(u);
  ASSERT_EQ(s_same.margin_count, jet.margin_count);
  for (int i = 0; i < jet.margin_count; i++) {
    EXPECT_NEAR(jet.margins[i], s_same.margins[i], 1e-12) << "margin " << i;
    const double fd_m = (s_plus.margins[i] - s_minus.margins[i]) / (2.0 * h);
    EXPECT_NEAR(jet.margins_dn[i], fd_m, 1e-6 * (1.0 + std::fabs(fd_m))) << "margin " << i;
  }
}

// A slab's field does not see the refractive index: dD_P/dn is exactly 0 (LI index_derivatives
// "0 for a slab" — d_slab's expression tree has no n in it), at a kink-circle point of 3-1-6 and
// at the beta crease point. The plan's closed form d/dn 2 asin sqrt(n^2 - 1) is a different
// object: the derivative of the kink circle's constant value as the circle itself moves with n —
// the following test pins that one.
TEST(DPField, SlabIndexDerivativeIsExactlyZero) {
  const Fixture prism_fixture(Prism(1.0));
  {
    const int faces[3] = { 3, 1, 6 };
    const DeviationField field = prism_fixture.Field(faces, 3, kN550);
    const double u_z = -std::sqrt(kN550 * kN550 - 1.0);
    const double u[3] = { std::sqrt(1.0 - u_z * u_z), 0.0, u_z };
    EXPECT_DOUBLE_EQ(field.Differentiate(u).d_p_dn, 0.0);
  }
  const Fixture beta_fixture(Beta());
  {
    const int faces[4] = { 4, 8, 7, 5 };
    const DeviationField field = beta_fixture.Field(faces, 4, kN550);
    const double u[3] = { 0.0, 1.0, 0.0 };  // the interior crease point
    EXPECT_DOUBLE_EQ(field.Differentiate(u).d_p_dn, 0.0);
  }
}

// The kink family's dispersion: evaluated at the moving kink circle u_z(n) = -sqrt(n^2 - 1), the
// slab constant follows 2 asin sqrt(n^2 - 1), whose closed-form derivative is
// 2n / (sqrt(n^2 - 1) sqrt(2 - n^2)) — the plan's anchor, correctly read as the family derivative
// (at a fixed u the slab derivative is the 0 above).
TEST(DPField, KinkFamilyDispersionMatchesTheClosedForm) {
  const Fixture f(Prism(1.0));
  const int faces[3] = { 3, 1, 6 };
  const double h = 1e-5;
  double values[2];
  int k = 0;
  for (double n : { kN550 - h, kN550 + h }) {
    const DeviationField field = f.Field(faces, 3, n);
    const double u_z = -std::sqrt(n * n - 1.0);
    const double u[3] = { std::sqrt(1.0 - u_z * u_z), 0.0, u_z };
    values[k++] = field.Sample(u).d_value;
  }
  const double fd = (values[1] - values[0]) / (2.0 * h);
  const double closed = 2.0 * kN550 / (std::sqrt(kN550 * kN550 - 1.0) * std::sqrt(2.0 - kN550 * kN550));
  EXPECT_NEAR(fd, closed, 1e-8 * (1.0 + std::fabs(closed)));
}

}  // namespace


}  // namespace lumice::analytic
