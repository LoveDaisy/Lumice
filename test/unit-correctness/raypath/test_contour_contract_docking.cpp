// The docking of scrum 660's contour producers (src/analytic/dp_contour.hpp) onto the measure
// layer's frozen contract (scrum 661, measure_geometry_contract.hpp): the field-for-field
// mapping of the analytic-side mirror structs into the contract value types, then the measure
// primitives end to end — QuadratureIntensity over a producer orbit stream, SampleWeightProfile
// over a producer restricted curve (the M2 numeric anchor the measure layer had to suspend:
// "no kind-1 curve fixture exists until the geometry-layer port" — this file IS that fixture),
// and the contract header's two pre-registered integration gaps pinned as red-state negative
// controls (u fidelity, the A/T decomposition). The C11/C12 anchors of the task's AC3 ride here
// too: they need both sides in one TU.
//
// Test-local helpers (RhombicPlate, LabelOrbit, AssignTarget) are the same constructions as
// test_fiber_quadrature.cpp's, duplicated per the suite's fixture convention with the
// provenance inline.
//
// symmetry_semantics: label — LabelOrbit expands at the kLabel gating (P|B|D bits), the C12
// tint class vocabulary of LI #51.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string>
#include <vector>

#include "analytic/dp_contour.hpp"
#include "analytic/dp_focus.hpp"
#include "analytic/path_evaluation.hpp"
#include "analytic/pose_density.hpp"
#include "core/crystal.hpp"
#include "core/crystal_param.hpp"
#include "raypath/detail/measure/declared_density.hpp"
#include "raypath/detail/measure/fiber_quadrature.hpp"
#include "raypath/detail/measure/measure_geometry_contract.hpp"
#include "raypath/detail/measure/visibility_certificate.hpp"
#include "raypath/detail/measure/weight_profile.hpp"

namespace lumice::raypath {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kDeg = kPi / 180.0;

// LI conventions #22: the wavelength-pool ends, the index pair of every chromatic verdict.
constexpr double kNRed = 1.307;
constexpr double kNBlue = 1.317;

double Dot3(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

double Norm3(const double a[3]) {
  return std::sqrt(Dot3(a, a));
}

double AngleBetween(const double a[3], const double b[3]) {
  const double cross[3] = { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
  return std::atan2(std::sqrt(Dot3(cross, cross)), Dot3(a, b));
}

struct Tables {
  analytic::FaceNormalTable normals;
  analytic::FacePolygonTable polys;
};

// test_fiber_quadrature.cpp's RhombicPlate (LI HexPrism(a=1, h=2) at the engine scale).
Tables RhombicPlate() {
  Tables t;
  analytic::CrystalShape shape;
  shape.kind = analytic::CrystalShapeKind::kPrism;
  shape.height = 1.0;
  const double fd[6] = { 1.5, 1, 1, 1.5, 1, 1 };
  for (int i = 0; i < 6; i++) {
    shape.face_distance[i] = fd[i];
  }
  EXPECT_EQ(analytic::BuildFaceNormals(shape, &t.normals, &t.polys), analytic::Status::kOk);
  return t;
}

// The C11 plate: prism h = 0.3, all face distances 1 (raypath_feature_plate_target.json).
Tables C11Plate() {
  Tables t;
  analytic::CrystalShape shape;
  shape.kind = analytic::CrystalShapeKind::kPrism;
  shape.height = 0.3;
  for (int i = 0; i < 6; i++) {
    shape.face_distance[i] = 1.0;
  }
  EXPECT_EQ(analytic::BuildFaceNormals(shape, &t.normals, &t.polys), analytic::Status::kOk);
  return t;
}

void SunAt(double altitude_deg, double azimuth_deg, double s[3]) {
  const double alt = altitude_deg * kDeg;
  const double az = azimuth_deg * kDeg;
  s[0] = std::cos(alt) * std::cos(az);
  s[1] = std::cos(alt) * std::sin(az);
  s[2] = std::sin(alt);
}

analytic::PoseDensitySpec PlateSpec() {
  analytic::PoseDensitySpec spec;
  spec.family = analytic::PoseFamily::kPlate;
  spec.zenith_mean_deg = 0.0;
  spec.zenith_std_deg = 1.0;
  return spec;
}

Distribution NoRandom(double v) {
  return { DistributionType::kNoRandom, static_cast<float>(v), 0.0f };
}
Distribution Uniform(double center, double range) {
  return { DistributionType::kUniform, static_cast<float>(center), static_cast<float>(range) };
}

// test_fiber_quadrature.cpp's PlateMeasure: the C12 config's axis (zenith 0, uniform azimuth,
// roll 0) under its sun — the kSpinOrbit measure whose orbit IS the producer's circle.
UMarginal PlateMeasure() {
  double s[3];
  SunAt(9.0, 180.0, s);
  AxisDistribution axis;
  axis.azimuth_dist = Uniform(0.0, 360.0);
  axis.latitude_dist = NoRandom(90.0);
  axis.roll_dist = NoRandom(0.0);
  return MakeUMarginal(axis, s);
}

// The C11 config's axis under its sun (altitude 20, azimuth 0).
UMarginal C11PlateMeasure() {
  double s[3];
  SunAt(20.0, 0.0, s);
  AxisDistribution axis;
  axis.azimuth_dist = Uniform(0.0, 360.0);
  axis.latitude_dist = NoRandom(90.0);
  axis.roll_dist = NoRandom(0.0);
  return MakeUMarginal(axis, s);
}

// The mechanical mapping this file owns: analytic mirror struct -> contract value type,
// field for field. Drift on either side fails here.
FiberSampleStream ToContract(const analytic::OrbitFiberStream& stream) {
  FiberSampleStream out;
  out.samples.resize(stream.samples.size());
  for (size_t i = 0; i < stream.samples.size(); i++) {
    const analytic::OrbitFiberPoint& p = stream.samples[i];
    FiberSample& s = out.samples[i];
    s.u[0] = p.u[0];
    s.u[1] = p.u[1];
    s.u[2] = p.u[2];
    s.area = p.area;
    s.transmission = p.transmission;
    s.valid = p.valid;
    s.jet_degenerate = p.jet_degenerate;
    s.parameter = p.parameter;
    s.weight = p.weight;
  }
  // The mirrored routing enums map one-for-one; the assertions keep the vocabulary honest
  // (EXPECT: this is a mapping helper, the caller reads on).
  EXPECT_EQ(stream.binding, analytic::FiberMeasureBinding::kFiberParameter);
  EXPECT_EQ(stream.evidence, analytic::FiberEvidenceForm::kSampledExhaustive);
  out.binding = MeasureBinding::kFiberParameter;
  out.evidence = FiberSampleStream::EvidenceForm::kSampledExhaustive;
  out.source_name = stream.source_name;
  // The producer-side fields note / spin_degenerate / grid have no contract-side carrier in v1
  // (the struct carries none of them): they stop here by contract, not by omission — the same
  // carrier-gap discipline the chain mapping's note declares.
  return out;
}

CriticalSetCurve ToContract(const analytic::RestrictedFamilyCurve& curve) {
  CriticalSetCurve out;
  EXPECT_EQ(curve.existence, analytic::CurveExistence::kComputed);  // mapping helper: EXPECT
  out.existence = ExistenceState::kComputed;
  out.closed = curve.closed;
  out.u = curve.u;
  out.tangent = curve.tangent;
  out.d_p = curve.d_p;
  out.wavelengths_nm = curve.wavelengths_nm;
  out.critical_d_p = curve.critical_d_p;
  out.support_param = curve.support_param;
  // The producer-side fields indices / routed_nonfinite / note have no contract-side carrier in
  // v1: they stop here by contract, not by omission — the same carrier-gap discipline the chain
  // mapping's note declares.
  return out;
}

WeightSingularChain ToContract(const analytic::ChainCurve& chain) {
  WeightSingularChain out;
  out.is_gate_boundary = chain.is_gate_boundary;
  // Value-for-value across the mirrored enums, all four arms spelled (the mirrors' docking
  // claim: a drift on either side breaks THIS switch — -Wswitch on an added/renamed analytic
  // arm, an unread mapping here). kEscaped / kS4Declared are partition/report vocabulary the
  // contour producers never emit; their arms exist so the translation stays total, not because
  // a producer chain is expected to carry them.
  switch (chain.existence) {
    case analytic::CurveExistence::kComputed:
      out.existence = ExistenceState::kComputed;
      break;
    case analytic::CurveExistence::kEscaped:
      out.existence = ExistenceState::kEscaped;
      break;
    case analytic::CurveExistence::kWalkTruncated:
      out.existence = ExistenceState::kWalkTruncated;
      break;
    case analytic::CurveExistence::kS4Declared:
      out.existence = ExistenceState::kS4Declared;
      break;
  }
  // The walk message has no contract-side carrier in v1 (the struct carries no note field —
  // the same vocabulary gap as event's -1), so `note` stops here by contract, not by omission.
  out.u = chain.u;
  out.param = chain.param;
  out.kink = chain.kink;
  out.event = chain.event;
  out.closed = chain.closed;
  return out;
}

// ---- the orbit stream through the quadrature --------------------------------------------------------------

TEST(ContourDocking, OrbitStreamThroughQuadratureIntensity) {
  const Tables t = RhombicPlate();
  const std::vector<int> faces = { 1, 3, 4, 2 };
  int slots[analytic::kMaxFaceCount];
  ASSERT_EQ(analytic::ResolveFaceSequence(t.normals, faces.data(), static_cast<int>(faces.size()), slots),
            analytic::Status::kOk);
  double sun[3];
  SunAt(9.0, 180.0, sun);
  const int grid = 2048;
  const analytic::OrbitFiberStream stream =
      analytic::MakeOrbitFiberStream(t.normals, t.polys, slots, 4, PlateSpec(), sun, kNRed, grid);
  ASSERT_EQ(stream.samples.size(), static_cast<size_t>(grid));

  const UMarginal measure = PlateMeasure();
  ASSERT_EQ(measure.kind(), USupportKind::kSpinOrbit);
  const FiberQuadratureResult q = QuadratureIntensity(measure, ToContract(stream), 1e-9);

  // The producer-side self-sum over the SAME samples reading the SAME measure, mirroring the
  // declared routing of a kFiberParameter stream (fiber_quadrature.cpp): mu is the ORBIT
  // PARAMETER density — the binding's own contract — kept = A*T > 0 AND valid AND mu > 0. The
  // mapping and the routing are what this asserts; the physical value's independence is the LI
  // #51 tint anchors' job below.
  double expected = 0.0;
  int expected_kept = 0;
  for (const analytic::OrbitFiberPoint& p : stream.samples) {
    if (!(p.area * p.transmission > 0.0) || !p.valid) {
      continue;
    }
    double u_out[3];
    const double mu = measure.OrbitDensity(p.parameter, u_out);
    if (!(mu > 0.0)) {
      continue;
    }
    expected += mu * p.area * p.transmission * p.weight;
    expected_kept++;
  }
  EXPECT_EQ(q.total, grid);
  EXPECT_EQ(q.kept, expected_kept);
  EXPECT_EQ(q.in_support, expected_kept);  // under this binding in_support counts the kept-with-mu
  EXPECT_EQ(q.binding_mismatch, 0);
  EXPECT_NEAR(q.intensity, expected, 1e-15 * std::max(expected, 1e-9) + 1e-15);
}

// ---- the two pre-registered integration gaps, pinned as red-state controls ---------------------------------

TEST(ContourDocking, PlaceholderUStarvesTheCertificateEverywhere) {
  // The contract header's u-fidelity note, as a red state: a kFiberParameter stream is
  // quadrature-legal with a placeholder u (the quadrature reads only parameter/weight), but the
  // certificate reads u of EVERY sample for support membership under every binding — the same
  // stream with placeholder u answers no_in_support_samples, an unreadable symptom without the
  // pinned warning.
  const Tables t = RhombicPlate();
  const std::vector<int> faces = { 1, 3, 4, 2 };
  int slots[analytic::kMaxFaceCount];
  ASSERT_EQ(analytic::ResolveFaceSequence(t.normals, faces.data(), static_cast<int>(faces.size()), slots),
            analytic::Status::kOk);
  double sun[3];
  SunAt(9.0, 180.0, sun);
  const analytic::OrbitFiberStream stream =
      analytic::MakeOrbitFiberStream(t.normals, t.polys, slots, 4, PlateSpec(), sun, kNRed, 512);
  FiberSampleStream contract = ToContract(stream);
  const FiberQuadratureResult legal = QuadratureIntensity(PlateMeasure(), contract, 1e-9);
  EXPECT_GT(legal.kept, 0);  // the quadrature does not care about u under this binding

  for (FiberSample& s : contract.samples) {
    s.u[0] = 0.0;
    s.u[1] = 0.0;
    s.u[2] = 1.0;  // the placeholder the header warns about
  }
  const VisibilityCertificate dark = CertifyVisibility(PlateMeasure(), contract, nullptr, nullptr, 1e-9);
  EXPECT_EQ(dark.state, VisibilityState::kUnproven);
  EXPECT_STREQ(dark.reason, "no_in_support_samples");
  EXPECT_TRUE(std::isnan(dark.lit_fraction));
}

TEST(ContourDocking, ZeroAreaAndZeroTransmissionAreDifferentDarknesses) {
  // The A/T decomposition note, as a red state: an A = 0 corridor and a T = 0 gate are different
  // unlit reasons, and the certificate's flags discriminate them. The A = 0 arm occurs naturally
  // (the C12 blue member's census); the T = 0 arm does not (the corridor's exit-critical gate
  // mirrors the path's exit-Snell gate) — synthesized here, which is what a negative control is.
  const Tables t = RhombicPlate();
  double sun[3];
  SunAt(9.0, 180.0, sun);

  // The kind-1 curve the unlit verdict needs (a present contour with the measure positive on
  // it): the producer's restricted circle, mapped.
  const std::vector<int> faces = { 1, 3, 5, 2 };
  int slots[analytic::kMaxFaceCount];
  ASSERT_EQ(analytic::ResolveFaceSequence(t.normals, faces.data(), static_cast<int>(faces.size()), slots),
            analytic::Status::kOk);
  const analytic::RestrictedFamilyCurve circle = analytic::MakeRestrictedFamilyCurve(
      t.normals, t.polys, slots, 4, PlateSpec(), sun, kNBlue, nullptr, nullptr, 0, 128);
  const CriticalSetCurve kind1 = ToContract(circle);

  // In-support u values for both arms: the circle's own points.
  FiberSampleStream closed_corridor;  // A = 0, T > 0
  FiberSampleStream closed_gate;      // A > 0, T = 0
  closed_corridor.binding = MeasureBinding::kFiberParameter;
  closed_corridor.evidence = FiberSampleStream::EvidenceForm::kSampledExhaustive;
  closed_gate = closed_corridor;
  for (int i = 0; i < 32; i++) {
    const double* u = &kind1.u[3 * (4 * i)];
    FiberSample a{};
    a.u[0] = u[0];
    a.u[1] = u[1];
    a.u[2] = u[2];
    a.area = 0.0;
    a.transmission = 0.5;
    a.valid = false;  // A*T = 0: kept out
    a.parameter = kind1.support_param[4 * i];
    a.weight = kTwoPi / 32.0;
    closed_corridor.samples.push_back(a);
    FiberSample b = a;
    b.area = 0.1;
    b.transmission = 0.0;
    closed_gate.samples.push_back(b);
  }

  const UMarginal measure = PlateMeasure();
  const VisibilityCertificate corridor_dark = CertifyVisibility(measure, closed_corridor, &kind1, nullptr, 1e-9);
  EXPECT_EQ(corridor_dark.state, VisibilityState::kUnlit);
  EXPECT_TRUE(corridor_dark.saw_zero_area);
  EXPECT_FALSE(corridor_dark.saw_zero_transmission);

  const VisibilityCertificate gate_dark = CertifyVisibility(measure, closed_gate, &kind1, nullptr, 1e-9);
  EXPECT_EQ(gate_dark.state, VisibilityState::kUnlit);
  EXPECT_FALSE(gate_dark.saw_zero_area);
  EXPECT_TRUE(gate_dark.saw_zero_transmission);  // the two darknesses stay distinguishable
}

// ---- the M2 real-curve anchor ------------------------------------------------------------------------------

TEST(ContourDocking, RestrictedCircleWeightProfileMatchesTheClosedForm) {
  // The M2 anchor the measure layer had to suspend (fiber_quadrature.hpp's spec block: "no
  // kind-1 curve fixture exists until the geometry-layer port"): a REAL producer curve under a
  // zonal measure. The C12 plate family's restricted circle (latitude 9 degrees of body z, the
  // sun's zenith complement) under the full-sphere uniform measure (rho_u = 1/(4 pi), constant
  // on every circle): the profile total is rho * 2 pi * cos(lat) with the declared chord
  // deficit, and the exact closed form of the chord polygon itself.
  const Tables t = RhombicPlate();
  const std::vector<int> faces = { 1, 3, 4, 2 };
  int slots[analytic::kMaxFaceCount];
  ASSERT_EQ(analytic::ResolveFaceSequence(t.normals, faces.data(), static_cast<int>(faces.size()), slots),
            analytic::Status::kOk);
  double sun[3];
  SunAt(9.0, 180.0, sun);
  const int grid = 360;
  const analytic::RestrictedFamilyCurve circle = analytic::MakeRestrictedFamilyCurve(
      t.normals, t.polys, slots, 4, PlateSpec(), sun, kNRed, nullptr, nullptr, 0, grid);
  ASSERT_EQ(circle.u.size(), static_cast<size_t>(3 * grid));
  const CriticalSetCurve curve = ToContract(circle);
  EXPECT_TRUE(curve.closed);  // the canonical closed kind-1 curve

  AxisDistribution full;
  full.azimuth_dist = Uniform(0.0, 360.0);
  full.latitude_dist = Uniform(90.0, 360.0);
  full.roll_dist = Uniform(0.0, 360.0);
  const double pole_off[3] = { 0.0, 0.6, 0.8 };  // the sun must be off the pole
  const UMarginal uniform_measure = MakeUMarginal(full, pole_off);
  ASSERT_EQ(uniform_measure.kind(), USupportKind::kArea);

  const WeightProfile profile = SampleWeightProfile(uniform_measure, curve);
  ASSERT_EQ(profile.points.size(), static_cast<size_t>(grid));
  EXPECT_EQ(profile.in_support, grid);  // rho_u = 1/(4 pi) everywhere: the whole curve is covered
  for (const WeightProfileSample& p : profile.points) {
    EXPECT_NEAR(p.rho_u, 1.0 / (4.0 * kPi), 1e-9);
  }
  const double lat = 9.0 * kDeg;
  const double radius = std::cos(lat);
  // (a) the exact closed form of this chord polygon: rho * 2 N R sin(pi / N).
  EXPECT_NEAR(profile.total, (1.0 / (4.0 * kPi)) * 2.0 * grid * radius * std::sin(kPi / grid), 1e-14);
  // (b) the continuum closed form rho * 2 pi R with the declared O(h^2) chord deficit.
  EXPECT_NEAR(profile.total, (1.0 / (4.0 * kPi)) * kTwoPi * radius, 1e-5);
}

// ---- the C11/C12 anchors (AC3) -----------------------------------------------------------------------------

// test_fiber_quadrature.cpp's LabelOrbit: the L1/PBD label orbit of a representative (D6
// rotations+mirrors x B; ExpandRaypathByPeriod at the kLabel gating with P|B|D bits).
std::vector<std::vector<int>> LabelOrbit(const std::vector<int>& representative) {
  PrismCrystalParam prism;
  prism.h_ = { DistributionType::kNoRandom, 1.0f, 0.0f };
  for (auto& d : prism.d_) {
    d = { DistributionType::kNoRandom, 1.5f, 0.0f };
  }
  prism.d_[1] = { DistributionType::kNoRandom, 1.0f, 0.0f };
  prism.d_[2] = { DistributionType::kNoRandom, 1.0f, 0.0f };
  prism.d_[4] = { DistributionType::kNoRandom, 1.0f, 0.0f };
  prism.d_[5] = { DistributionType::kNoRandom, 1.0f, 0.0f };
  const GeometricSymmetry geom = DeriveGeometricSymmetry(prism);
  AxisDistribution axis;
  axis.azimuth_dist = Uniform(0.0, 360.0);
  axis.latitude_dist = NoRandom(90.0);
  axis.roll_dist = NoRandom(0.0);
  const SymmetryGating label = DeriveSymmetryGating(SymmetrySemantics::kLabel, geom, axis);
  const auto d = detail::DeriveDSymmetryParams(axis);
  std::vector<IdType> path(representative.begin(), representative.end());
  std::vector<std::vector<int>> members;
  for (const auto& m : ExpandRaypathByPeriod(path, sym::kSymP | sym::kSymB | sym::kSymD, d.sigma_a, d.d_applicable,
                                             label.p_applicable, label.b_applicable, kHexagonalFnPeriod, label.geom)) {
    std::vector<int> converted(m.begin(), m.end());
    if (std::find(members.begin(), members.end(), converted) == members.end()) {
      members.push_back(std::move(converted));
    }
  }
  return members;
}

// The two sky targets: spots at the sun's altitude, relative solar azimuth +-120 degrees. The
// measurement space of the assignment is the SPHERICAL angle to the spot position — the spot a
// member lights is -outgoing (the outgoing is propagation, the spot is where the beam goes).
void TargetSpot(double altitude_deg, double azimuth_deg, double spot[3]) {
  SunAt(altitude_deg, azimuth_deg, spot);
}

int AssignTarget(double altitude_deg, double sun_az_deg, const double outgoing[3], double spot[2][3]) {
  TargetSpot(altitude_deg, sun_az_deg + 120.0, spot[0]);
  TargetSpot(altitude_deg, sun_az_deg - 120.0, spot[1]);
  const double landing[3] = { -outgoing[0], -outgoing[1], -outgoing[2] };
  if (AngleBetween(landing, spot[0]) < 1e-6) {
    return 0;
  }
  if (AngleBetween(landing, spot[1]) < 1e-6) {
    return 1;
  }
  return -1;
}

// One member's energy through the PRODUCER path (the point of this anchor: the same numbers the
// handwritten grid produced, now through the productized producer).
struct MemberEnergy {
  double e = 0.0;
  double outgoing[3] = { 0.0, 0.0, 0.0 };
  bool any_valid = false;
};

MemberEnergy EvaluateMember(const Tables& t, const std::vector<int>& faces, double n, const double sun[3],
                            const UMarginal& measure, int grid) {
  MemberEnergy result;
  int slots[analytic::kMaxFaceCount];
  if (analytic::ResolveFaceSequence(t.normals, faces.data(), static_cast<int>(faces.size()), slots) !=
      analytic::Status::kOk) {
    return result;  // a face sequence this crystal cannot host: zero by construction
  }
  const analytic::OrbitFiberStream stream = analytic::MakeOrbitFiberStream(
      t.normals, t.polys, slots, static_cast<int>(faces.size()), PlateSpec(), sun, n, grid);
  for (const analytic::OrbitFiberPoint& p : stream.samples) {
    if (p.valid && !result.any_valid) {
      result.outgoing[0] = p.outgoing[0];
      result.outgoing[1] = p.outgoing[1];
      result.outgoing[2] = p.outgoing[2];
      result.any_valid = true;
    }
  }
  const FiberQuadratureResult q = QuadratureIntensity(measure, ToContract(stream), 1e-9);
  result.e = q.intensity;
  return result;
}

struct ClassTint {
  double ratio[2] = { 0.0, 0.0 };
  bool defined[2] = { false, false };
  int assigned[2] = { 0, 0 };
};

ClassTint ClassTintOverOrbit(const Tables& t, const std::vector<int>& representative, int grid,
                             const UMarginal& measure, double altitude_deg, double sun_az_deg) {
  ClassTint out;
  double sun[3];
  SunAt(altitude_deg, sun_az_deg, sun);
  double e_red[2] = { 0.0, 0.0 };
  double e_blue[2] = { 0.0, 0.0 };
  for (const std::vector<int>& member : LabelOrbit(representative)) {
    const MemberEnergy red = EvaluateMember(t, member, kNRed, sun, measure, grid);
    const MemberEnergy blue = EvaluateMember(t, member, kNBlue, sun, measure, grid);
    if (!red.any_valid && !blue.any_valid) {
      continue;  // an unlit member lights neither target
    }
    double spot[2][3];
    const int target = AssignTarget(altitude_deg, sun_az_deg, red.any_valid ? red.outgoing : blue.outgoing, spot);
    if (target < 0) {
      continue;
    }
    out.assigned[target]++;
    e_red[target] += red.e;
    e_blue[target] += blue.e;
  }
  for (int k = 0; k < 2; k++) {
    if (e_red[k] > 0.0) {
      out.ratio[k] = e_blue[k] / e_red[k];
      out.defined[k] = true;
    }
  }
  return out;
}

TEST(ContourDocking, C12TintReproducesTheLI51Authority) {
  // The C12 anchor through the producer path, tolerances verbatim from 661's verified tests
  // (white +-0.012, blue +-0.02 at 8192 — the plan's "do not tighten the authority's
  // tolerances"): the LI #51 kernel, E = (1/2 pi) int A T dtheta per member of the 24-member
  // L1/PBD orbit, tint = blue/red energy per target.
  // Sync duty: the authority values and tolerances below are duplicated with
  // test_fiber_quadrature.cpp's C12 block (there the handwritten grid, here the producer path —
  // the redundancy is the point); when LI's anchors or tolerances move, change BOTH.
  const Tables t = RhombicPlate();
  const UMarginal measure = PlateMeasure();
  const ClassTint white = ClassTintOverOrbit(t, { 1, 3, 4, 2 }, 8192, measure, 9.0, 180.0);
  ASSERT_TRUE(white.defined[0]);
  ASSERT_TRUE(white.defined[1]);
  EXPECT_GT(white.assigned[0], 0);
  EXPECT_GT(white.assigned[1], 0);
  EXPECT_NEAR(white.ratio[0], 1.010, 0.012) << "white class, target +120deg";
  EXPECT_NEAR(white.ratio[1], 1.010, 0.012) << "white class, target -120deg";

  const ClassTint blue = ClassTintOverOrbit(t, { 1, 3, 5, 2 }, 8192, measure, 9.0, 180.0);
  ASSERT_TRUE(blue.defined[0]);
  ASSERT_TRUE(blue.defined[1]);
  EXPECT_NEAR(blue.ratio[0], 1.525, 0.02) << "blue class, target +120deg";
  EXPECT_NEAR(blue.ratio[1], 1.525, 0.02) << "blue class, target -120deg";
  EXPECT_GT(blue.ratio[0], 1.12);
  EXPECT_GT(blue.ratio[1], 1.12);
}

TEST(ContourDocking, PositionsAreSphericalDistancesNotAzimuthDifferences) {
  // The measurement-space pin (the corpus's "azimuth difference != spherical distance" row):
  // the +-120-degree PARHELION spots sit 117.5998 deg (C12, sun altitude 9) and 108.937 deg
  // (C11, sun altitude 20) from the SUN as a spherical angle — the 120 is the azimuth
  // difference, a different space, and conflating them misplaces the feature by 2.4 deg here.
  const struct {
    double altitude;
    double expected_deg;
    const char* name;
  } rows[] = {
    { 9.0, 117.5998, "C12" },
    { 20.0, 108.937, "C11" },
  };
  for (const auto& row : rows) {
    double sun[3], spot[3];
    SunAt(row.altitude, 0.0, sun);
    SunAt(row.altitude, 120.0, spot);
    const double delta = AngleBetween(sun, spot) / kDeg;
    EXPECT_NEAR(delta, row.expected_deg, 5e-4) << row.name;
    EXPECT_NE(delta, 120.0) << row.name << ": the azimuth difference is not the spherical distance";
    // The C11 spot's own deflection equals the family's pinned D_P (108.937 deg = 1.9013146 rad,
    // the analytic test's constant-on-circle value): deflection and spot distance are the same
    // angle (cos delta = s . (-outgoing) = (-s) . outgoing). The analytic side pins the level
    // set; this pins the same number from the spot geometry — two doors, one value.
    if (std::string(row.name) == "C11") {
      EXPECT_NEAR(delta * kDeg, 1.901314641326, 1e-9);
    }
  }
}

TEST(ContourDocking, C11FamilyCollapseValidArcAndOutgoing) {
  // The C11 anchor's producer-side legs (FamilyPinned and the constant-on-circle D_P live in
  // the analytic test): the valid arc spans the corpus's 60 degrees, and EVERY valid sample's
  // outgoing points at the (20 deg, +-120 deg) target spot within 3.8e-12 — the family
  // collapses to one sky point.
  const Tables t = C11Plate();
  const std::vector<int> faces = { 1, 4, 5, 2 };
  int slots[analytic::kMaxFaceCount];
  ASSERT_EQ(analytic::ResolveFaceSequence(t.normals, faces.data(), static_cast<int>(faces.size()), slots),
            analytic::Status::kOk);
  double sun[3];
  SunAt(20.0, 0.0, sun);
  const int grid = 720;
  const analytic::OrbitFiberStream stream =
      analytic::MakeOrbitFiberStream(t.normals, t.polys, slots, 4, PlateSpec(), sun, 1.3110129, grid);

  double spot[2][3];
  TargetSpot(20.0, 120.0, spot[0]);
  TargetSpot(20.0, -120.0, spot[1]);

  int valid = 0;
  double landing_max = 0.0;  // max over valid samples of the NEAREST spot's distance
  for (const analytic::OrbitFiberPoint& p : stream.samples) {
    if (!p.valid) {
      continue;
    }
    valid++;
    // The spot a beam lands on is -outgoing.
    const double landing[3] = { -p.outgoing[0], -p.outgoing[1], -p.outgoing[2] };
    double nearest = 1e9;
    for (int k = 0; k < 2; k++) {
      const double diff[3] = { landing[0] - spot[k][0], landing[1] - spot[k][1], landing[2] - spot[k][2] };
      nearest = std::min(nearest, Norm3(diff));
    }
    landing_max = std::max(landing_max, nearest);
  }
  // The valid arc: 60 degrees at the grid's resolution (one cell = 0.5 deg).
  EXPECT_NEAR(valid * 360.0 / grid, 60.0, 1.0);
  // EVERY valid sample's landing sits on ONE of the two spots within the corpus's 3.8e-12 — the
  // pointwise form the AC declares. A min over samples would certify "at least one lucky
  // sample"; the fold identity claims all of them, so the statistic is max-of-nearest.
  printf("[c11-landing] max-over-valid |landing - nearest spot| = %.3e\n", landing_max);
  EXPECT_LE(landing_max, 3.8e-12);
}

TEST(ContourDocking, ChainMappingCarriesAWalkTruncatedChainAcrossTheBoundary) {
  // The mirror enum's docking claim needs a non-kComputed chain actually crossing ToContract:
  // the analytic side's kWalkTruncated chain (a walk refusal with an empty record — the same
  // construction ChainStatusMappingAndTrustTheStatus pins on the analytic side) mapped into the
  // contract's WeightSingularChain, the mechanical fields carried one-for-one and the refusal
  // status surviving the boundary. The walk message itself has no contract carrier (no note
  // field on the struct — declared in ToContract), so it is asserted on the analytic side only.
  analytic::BoundaryWalkRecord empty;
  const analytic::ChainCurve truncated = analytic::ChainFromBoundaryPieces(empty, analytic::WalkStatus::kStepsExhausted,
                                                                           "MAX_WALK_STEPS without a corner");
  ASSERT_EQ(truncated.existence, analytic::CurveExistence::kWalkTruncated);
  ASSERT_NE(truncated.note.find("MAX_WALK_STEPS"), std::string::npos);

  const WeightSingularChain out = ToContract(truncated);
  EXPECT_TRUE(out.is_gate_boundary);  // kind-2 flag passthrough: this mapping IS the boundary-walk path
  EXPECT_EQ(out.existence, ExistenceState::kWalkTruncated);
  EXPECT_TRUE(out.u.empty());
  EXPECT_TRUE(out.param.empty());
  EXPECT_TRUE(out.kink.empty());
  EXPECT_TRUE(out.event.empty());
  EXPECT_FALSE(out.closed);  // closure IS the kOk certificate; a refusal never closes
}

}  // namespace
}  // namespace lumice::raypath
