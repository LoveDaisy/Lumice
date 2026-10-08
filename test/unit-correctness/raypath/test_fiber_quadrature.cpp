// The M1 fiber quadrature (src/raypath/detail/measure/fiber_quadrature.hpp) against the LI #51
// authority (the spec block of fiber_quadrature.hpp restates the integral): the rhombic plate
// `1.5,1,1,1.5,1,1` (Lumice height 1.0 = LI HexPrism(a=1,h=2), areas 4x LI's), sun altitude 9
// azimuth 180, the ideal horizontal Rz family, the 24-member L1/PBD orbit, per-target energy
// sums at n = 1.307 / 1.317, tint ~ 1.010 (white 1-3-4-2) and ~ 1.525 (blue 1-3-5-2) at each
// target. Source: LI docs/raypath-diagnostic-reference.md "Ideal horizontal plates..." + the
// fixture NPZ's five per-class snapshot poses (values inlined below with their provenance).
//
// The mock cases pin the quadrature mechanics (kept-iff-positive, hand sums, both bindings);
// the anchor cases pin the numbers. 口径审计前置: the measurement space is spelled at each
// assertion — spherical separation 117.5998 degrees is NOT the azimuth difference (120), and the
// outgoing direction is the PROPAGATION vector, the negated sky-spot position.
//
// symmetry_semantics: label (the class orbit is the L1/PBD expansion — ExpandRaypathByPeriod
// with the kLabel gating; the physical L2 members are a subset, corpus row C12).

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

#include "analytic/discovery.hpp"
#include "analytic/entry_measure.hpp"
#include "analytic/path_evaluation.hpp"
#include "core/crystal.hpp"
#include "core/crystal_param.hpp"
#include "raypath/detail/measure/declared_density.hpp"
#include "raypath/detail/measure/fiber_quadrature.hpp"
#include "raypath/detail/measure/measure_geometry_contract.hpp"
#include "raypath/detail/measure/visibility_certificate.hpp"

namespace lumice::raypath {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kDeg = kPi / 180.0;

// LI #51 snapshots carry A in LI's hexagon-edge-1 normalisation; the engine crystal at
// face_distance 1 is half that edge, so engine areas are LI's / 4. The product-side constant
// was retired with the LI caliber fix (band_sum weights are the entry measure of the reference
// crystal itself now — see the 660.4 retirement commit), but these snapshot literals still
// live in the old normalisation: the conversion is a fixture concern of this test, kept here
// as a local literal rather than a product constant.
constexpr double kLiAreaPerEngineArea = 4.0;

// LI conventions #22: the wavelength-pool ends, parameters of every chromatic verdict.
constexpr double kNRed = 1.307;
constexpr double kNBlue = 1.317;

// The LI #51 fixture's snapshot poses (tests/data/raypath-diagnostic-reference/arrays.npz,
// arrays plate_white_snapshots_* / plate_blue_snapshots_*), inlined as literals with full
// precision so the cross-check needs no binary fixture: five effective poses per class, the
// body-frame sun u, the SO(3) pose, and A / T / outgoing at both indices. A is in LI's
// normalisation (hexagon edge a = 1); the engine crystal at face_distance 1 is half the edge, so
// engine areas are LI's / 4 (kLiAreaPerEngineArea).
struct SnapshotPose {
  double u[3];
  double rotation[9];
  double outgoing_red[3];
  double outgoing_blue[3];
  double a_red, t_red, a_blue, t_blue;
};

const SnapshotPose kWhiteSnapshots[5] = {
  { { -0.00037877372543881145, -0.9876882679661847, 0.15643446504023087 },
    { 0.0003834951875714743, 0.9999999264657179, 0.0, -0.9999999264657179, 0.0003834951875714743, 0.0, 0.0, 0.0, 1.0 },
    { -0.4938441702975685, 0.8553631939770864, -0.15643446504023073 },
    { -0.49384417029756866, 0.8553631939770865, -0.15643446504022973 },
    4.669320664564702e-06,
    0.15050017707625885,
    5.362413906246376e-06,
    0.38475584949814723 },
  { { -0.25575450686031775, -0.9540009907585068, 0.15643446504023087 },
    { 0.25894251895918013, 0.965892733110191, 0.0, -0.965892733110191, 0.25894251895918013, 0.0, 0.0, 0.0, 1.0 },
    { -0.4938441702975684, 0.8553631939770863, -0.15643446504023073 },
    { -0.49384417029756833, 0.8553631939770865, -0.15643446504022973 },
    0.006202217271514537,
    0.3892521181459411,
    0.0062022172715145395,
    0.38475584949814723 },
  { { -0.4937348237040728, -0.8554263159439771, 0.15643446504023087 },
    { 0.4998892903874614, 0.8660893125745868, 0.0, -0.8660893125745868, 0.4998892903874614, 0.0, 0.0, 0.0, 1.0 },
    { -0.4938441702975685, 0.8553631939770864, -0.15643446504023073 },
    { -0.49384417029756866, 0.8553631939770865, -0.15643446504022973 },
    0.01692703027073502,
    0.3892521181459411,
    0.01692703027073504,
    0.38475584949814723 },
  { { -0.698133238507492, -0.6986689054470782, 0.15643446504023087 },
    { 0.7068355571424735, 0.7073779012376424, 0.0, -0.7073779012376424, 0.7068355571424735, 0.0, 0.0, 0.0, 1.0 },
    { -0.4938441702975686, 0.8553631939770866, -0.15643446504023073 },
    { -0.4938441702975687, 0.8553631939770866, -0.15643446504022973 },
    0.023857100945506745,
    0.3892521181459411,
    0.025483056454328457,
    0.38475584949814723 },
  { { -0.8550473745015391, -0.49439078218156147, 0.15643446504023087 },
    { 0.8657056475795023, 0.5005534254693776, 0.0, -0.5005534254693776, 0.8657056475795023, 0.0, 0.0, 0.0, 1.0 },
    { -0.49384417029756866, 0.8553631939770864, -0.15643446504023073 },
    { -0.49384417029756894, 0.8553631939770865, -0.15643446504022973 },
    0.0001077682812556502,
    0.1511560608068196,
    0.00010892887022257419,
    0.38475584949814723 },
};

const SnapshotPose kBlueSnapshots[5] = {
  { { -0.7119261859876402, -0.6846089130683801, 0.15643446504023087 },
    { 0.7208004354477492, 0.6931426492853655, 0.0, -0.6931426492853655, 0.7208004354477492, 0.0, 0.0, 0.0, 1.0 },
    { -0.4938441702975693, -0.8553631939770864, -0.15643446504023073 },
    { -0.4938441702975692, -0.8553631939770863, -0.15643446504022973 },
    1.3262289069553542e-07,
    0.029078143722740535,
    1.3262289073451262e-07,
    0.03481995490324624 },
  { { -0.8172128039314571, -0.55469946028283, 0.15643446504023087 },
    { 0.8273994643280291, 0.5616138588297929, 0.0, -0.5616138588297929, 0.8273994643280291, 0.0, 0.0, 0.0, 1.0 },
    { -0.49384417029756883, -0.8553631939770863, -0.15643446504023073 },
    { -0.4938441702975688, -0.8553631939770863, -0.15643446504022973 },
    0.025848575071525527,
    0.06750450758529505,
    0.02410310327762131,
    0.0909267468944561 },
  { { -0.8990755077107546, -0.408890559419297, 0.15643446504023087 },
    { 0.9102825970072818, 0.4139874316759855, 0.0, -0.4139874316759855, 0.9102825970072818, 0.0, 0.0, 0.0, 1.0 },
    { -0.49384417029756894, -0.8553631939770864, -0.15643446504023073 },
    { -0.4938441702975691, -0.8553631939770863, -0.15643446504022973 },
    0.031690463965730345,
    0.05681464455040111,
    0.02882511650567192,
    0.07396845125753614 },
  { { -0.9551678522657182, -0.25136155661849335, 0.15643446504023087 },
    { 0.9670741396928669, 0.2544948100400112, 0.0, -0.2544948100400112, 0.9670741396928669, 0.0, 0.0, 0.0, 1.0 },
    { -0.49384417029756905, -0.8553631939770866, -0.15643446504023073 },
    { -0.4938441702975691, -0.8553631939770865, -0.15643446504022973 },
    0.013141363546524691,
    0.027111564374115424,
    0.010953071925060936,
    0.032258211021729655 },
  { { -0.9839482061068305, -0.0858730798721401, 0.15643446504023087 },
    { 0.9962132442648319, 0.08694349861454967, 0.0, -0.08694349861454967, 0.9962132442648319, 0.0, 0.0, 0.0, 1.0 },
    { -0.49384417029756894, -0.8553631939770858, -0.15643446504023073 },
    { -0.49384417029756894, -0.8553631939770858, -0.15643446504022973 },
    9.193768202280375e-05,
    0.020332111873999737,
    2.0024468614082646e-07,
    0.02361408734478624 },
};

// --- The scene's fixed geometry, single-sourced for the whole file. -------------------------

struct PlateTables {
  analytic::FaceNormalTable normals;
  analytic::FacePolygonTable polys;
};

const PlateTables& RhombicPlate() {
  static PlateTables t;
  static bool built = false;
  if (!built) {
    analytic::CrystalShape shape;
    shape.kind = analytic::CrystalShapeKind::kPrism;
    shape.height = 1.0;  // = LI HexPrism(a=1, h=2): h = 2a * height at a = 1
    const double fd[6] = { 1.5, 1, 1, 1.5, 1, 1 };
    for (int i = 0; i < 6; i++) {
      shape.face_distance[i] = fd[i];
    }
    const analytic::Status st = analytic::BuildFaceNormals(shape, &t.normals, &t.polys);
    EXPECT_EQ(st, analytic::Status::kOk);
    built = true;
  }
  return t;
}

// The sun position vector (pointing AT the sun): altitude 9, azimuth 180 —
// (cos alt cos az, cos alt sin az, + sin alt), the world convention of sky_direction.hpp.
void SunHat(double s[3]) {
  const double alt = 9.0 * kDeg;
  s[0] = std::cos(alt) * std::cos(180.0 * kDeg);
  s[1] = std::cos(alt) * std::sin(180.0 * kDeg);
  s[2] = std::sin(alt);
}

void Incident(double s[3]) {
  SunHat(s);
  for (int i = 0; i < 3; i++) {
    s[i] = -s[i];  // propagation: sun -> crystal
  }
}

AxisDistribution PlateAxis() {
  AxisDistribution a;  // the config's axis: zenith 0 (latitude 90), uniform azimuth, roll 0
  a.azimuth_dist = { DistributionType::kUniform, 0.0f, 360.0f };
  a.latitude_dist = { DistributionType::kNoRandom, 90.0f, 0.0f };
  a.roll_dist = { DistributionType::kNoRandom, 0.0f, 0.0f };
  return a;
}

UMarginal PlateMeasure() {
  double s[3];
  SunHat(s);
  return MakeUMarginal(PlateAxis(), s);
}

// The L1/PBD label orbit of a representative (LI symmetry.reflection_group.pbd_orbit: D6
// rotations+mirors x B; Lumice's ExpandRaypathByPeriod at the kLabel gating with P|B|D bits —
// P the C6 rotations, D the sigma_a mirror, B the basal swap).
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
  const AxisDistribution axis = PlateAxis();
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

// One member's spin integral through the production quadrature: the periodic midpoint grid over
// Rz(theta), each point evaluated by the module A/B machinery (corridor A + path power T), fed
// to QuadratureIntensity as a kFiberParameter stream (binding = dtheta, mu = the declared
// density along the orbit), exactly the #51 kernel E = (1/2 pi) integral A*T dtheta.
struct MemberEnergy {
  double e = 0.0;  // the intensity the quadrature returns (= E_m,n)
  double outgoing[3] = { 0.0, 0.0, 0.0 };
  bool any_valid = false;
};

MemberEnergy EvaluateMember(const PlateTables& t, const std::vector<int>& faces, double n, int grid) {
  MemberEnergy result;
  std::vector<int> slots(faces.size());
  const analytic::Status st =
      analytic::ResolveFaceSequence(t.normals, faces.data(), static_cast<int>(faces.size()), slots.data());
  if (st != analytic::Status::kOk) {
    return result;  // a face sequence this crystal cannot host: zero by construction
  }
  const int count = static_cast<int>(faces.size());
  analytic::Corridor corridor(t.normals, t.polys, slots.data(), count);

  double s_hat[3], incident[3];
  SunHat(s_hat);
  Incident(incident);

  FiberSampleStream stream;
  stream.binding = MeasureBinding::kFiberParameter;
  stream.evidence = FiberSampleStream::EvidenceForm::kSampledExhaustive;
  stream.source_name = "c12-spin-grid";
  stream.samples.resize(static_cast<size_t>(grid));
  std::vector<double> segments(static_cast<size_t>(count + 1) * 3);
  std::vector<double> transmittances(static_cast<size_t>(count));
  double first_outgoing[3] = { 0.0, 0.0, 0.0 };

  for (int i = 0; i < grid; i++) {
    const double theta = kTwoPi * (i + 0.5) / grid;
    const double c = std::cos(theta), s = std::sin(theta);
    const double pose[9] = { c, -s, 0.0, s, c, 0.0, 0.0, 0.0, 1.0 };  // Rz(theta), body -> world
    analytic::PathOutputs out;
    out.segment_directions = segments.data();
    out.interface_transmittances = transmittances.data();
    const bool valid = analytic::EvaluatePath(t.normals, slots.data(), count, n, incident, pose, &out);

    // u = R^T s_hat; the corridor's argument is the body-frame PROPAGATION direction R^T(-s_hat).
    double u[3] = { s_hat[0], s_hat[1], s_hat[2] };
    double s_body[3];
    for (int r = 0; r < 3; r++) {
      u[r] = pose[r] * s_hat[0] + pose[3 + r] * s_hat[1] + pose[6 + r] * s_hat[2];  // R^T = rows
      s_body[r] = -u[r];
    }
    const analytic::EntryMeasure entry = corridor.Evaluate(s_body, n);

    FiberSample& sample = stream.samples[static_cast<size_t>(i)];
    sample.u[0] = u[0];
    sample.u[1] = u[1];
    sample.u[2] = u[2];
    sample.area = entry.status == analytic::EntryMeasureStatus::kOk ? entry.value : 0.0;
    sample.transmission = valid ? out.fresnel_transmission : 0.0;
    sample.valid = valid && sample.area > 0.0 && sample.transmission > 0.0;
    sample.jet_degenerate = false;
    sample.parameter = theta;
    sample.weight = kTwoPi / grid;  // dtheta: the binding's element

    if (sample.valid) {
      if (!result.any_valid) {
        first_outgoing[0] = out.outgoing_direction[0];
        first_outgoing[1] = out.outgoing_direction[1];
        first_outgoing[2] = out.outgoing_direction[2];
        result.outgoing[0] = out.outgoing_direction[0];
        result.outgoing[1] = out.outgoing_direction[1];
        result.outgoing[2] = out.outgoing_direction[2];
        result.any_valid = true;
      } else {
        // The physical branch's fold identity: the outgoing is theta-INDEPENDENT (out(Rz(theta))
        // = M incident). Pin it on the last valid sample too.
        const double dot = first_outgoing[0] * out.outgoing_direction[0] +
                           first_outgoing[1] * out.outgoing_direction[1] +
                           first_outgoing[2] * out.outgoing_direction[2];
        EXPECT_GT(dot, 1.0 - 1e-9) << "member " << faces.front() << ": outgoing must not depend on theta";
      }
    }
  }

  const FiberQuadratureResult q = QuadratureIntensity(PlateMeasure(), stream, 1e-9);
  result.e = q.intensity;
  return result;
}

// The two sky targets: spots at the sun's altitude, relative solar azimuth +-120 degrees. The
// measurement space of the assignment is the SPHERICAL angle to the spot position (117.5998 deg
// from the sun — NOT the 120-degree azimuth difference; the outgoing is propagation, so the spot
// it lands on is -outgoing).
int AssignTarget(const double outgoing[3]) {
  const double alt = 9.0 * kDeg;
  double spot[2][3];
  for (int k = 0; k < 2; k++) {
    const double az = (k == 0 ? 180.0 + 120.0 : 180.0 - 120.0) * kDeg;
    spot[k][0] = std::cos(alt) * std::cos(az);
    spot[k][1] = std::cos(alt) * std::sin(az);
    spot[k][2] = std::sin(alt);
  }
  double best = -2.0;
  int target = -1;
  for (int k = 0; k < 2; k++) {
    const double dot = -outgoing[0] * spot[k][0] - outgoing[1] * spot[k][1] - outgoing[2] * spot[k][2];
    if (dot > best) {
      best = dot;
      target = k;
    }
  }
  return best > std::cos(2.0 * kDeg) ? target : -1;  // "other": neither target within 2 degrees
}

// ===========================================================================
// Mock mechanics: hand sums, kept-iff-positive, both bindings.
// ===========================================================================

FiberSample MockSample(double u0, double u1, double u2, double area, double transmission, double parameter,
                       double weight) {
  FiberSample s;
  s.u[0] = u0;
  s.u[1] = u1;
  s.u[2] = u2;
  s.area = area;
  s.transmission = transmission;
  s.valid = area * transmission > 0.0;
  s.parameter = parameter;
  s.weight = weight;
  return s;
}

TEST(FiberQuadrature, UniformDensityKnownProfileHandSum) {
  // Uniform spin measure (mu = 1/(2 pi) w.r.t. dtheta), a hand-set A*T profile on a 4-point
  // grid: intensity must be the hand sum sum mu * A * T * dtheta, and the DARK sample is KEPT
  // OUT (kept counts it out — the SampleEvent convention), not weighted zero.
  const UMarginal measure = PlateMeasure();
  FiberSampleStream stream;
  stream.binding = MeasureBinding::kFiberParameter;
  stream.evidence = FiberSampleStream::EvidenceForm::kSampledExhaustive;
  const double dtheta = kTwoPi / 4.0;
  stream.samples.push_back(MockSample(1, 0, 0, 0.5, 0.8, 0.0 * dtheta, dtheta));
  stream.samples.push_back(MockSample(0, 1, 0, 0.5, 0.8, 1.0 * dtheta, dtheta));
  stream.samples.push_back(MockSample(-1, 0, 0, 0.4, 0.0, 2.0 * dtheta, dtheta));  // T = 0: dark
  stream.samples.push_back(MockSample(0, -1, 0, 0.0, 0.6, 3.0 * dtheta, dtheta));  // A = 0: dark
  // The mock u points are NOT on the orbit — MuPositive/parameter binding reads the PARAMETER
  // for kFiberParameter streams, so the density is mu(theta) regardless of the u placeholder.
  const FiberQuadratureResult q = QuadratureIntensity(measure, stream, 1e-9);
  const double expected = 2.0 * (1.0 / kTwoPi) * 0.5 * 0.8 * dtheta;
  EXPECT_NEAR(q.intensity, expected, 1e-15);
  EXPECT_EQ(q.kept, 2);
  EXPECT_EQ(q.total, 4);
}

TEST(FiberQuadrature, SolidAngleBindingWeightsByTheAreaDensity) {
  // Full-sphere uniform measure: rho_u = 1/(4 pi); two lit samples with dOmega weights: the
  // intensity is (1/(4 pi)) * sum A*T*w.
  double sun[3];
  SunHat(sun);
  AxisDistribution axis;
  axis.azimuth_dist = { DistributionType::kUniform, 0.0f, 360.0f };
  axis.latitude_dist = { DistributionType::kUniform, 90.0f, 360.0f };
  axis.roll_dist = { DistributionType::kUniform, 0.0f, 360.0f };
  const UMarginal measure = MakeUMarginal(axis, sun);
  ASSERT_TRUE(measure.fast_path());
  FiberSampleStream stream;
  stream.binding = MeasureBinding::kSolidAngle;
  stream.evidence = FiberSampleStream::EvidenceForm::kSampledExhaustive;
  stream.samples.push_back(MockSample(0.6, 0.8, 0.0, 0.25, 0.5, 0.0, 1e-3));
  stream.samples.push_back(MockSample(0.36, 0.48, 0.8, 0.75, 0.5, 0.0, 2e-3));  // a generic
  // interior point (the pole itself sits on the zonal chart's coordinate boundary)
  const FiberQuadratureResult q = QuadratureIntensity(measure, stream, 1e-9);
  const double expected = (1.0 / (4.0 * kPi)) * (0.25 * 0.5 * 1e-3 + 0.75 * 0.5 * 2e-3);
  EXPECT_NEAR(q.intensity, expected, 1e-15);
  EXPECT_EQ(q.kept, 2);
  EXPECT_EQ(q.in_support, 2);
}

TEST(FiberQuadrature, BindingMismatchIsReportedNotSilent) {
  // A stream whose binding the measure cannot answer (dOmega weights on a spin orbit; a fiber
  // parameter on an area support) is dropped AND counted in binding_mismatch — a mis-bound stream
  // is distinguishable from a dark one ("coverage gaps keep their reason").
  const UMarginal spin = PlateMeasure();  // kSpinOrbit
  ASSERT_EQ(spin.kind(), USupportKind::kSpinOrbit);
  FiberSampleStream mismatched;
  mismatched.binding = MeasureBinding::kSolidAngle;  // the spin orbit has no dOmega density
  mismatched.samples.push_back(MockSample(1, 0, 0, 0.5, 0.8, 0.0, 0.1));
  const FiberQuadratureResult q = QuadratureIntensity(spin, mismatched, 1e-9);
  EXPECT_EQ(q.binding_mismatch, 1);
  EXPECT_EQ(q.kept, 0);
  EXPECT_EQ(q.total, 1);

  double sun[3];
  SunHat(sun);
  AxisDistribution area_axis;
  area_axis.azimuth_dist = { DistributionType::kUniform, 0.0f, 360.0f };
  area_axis.latitude_dist = { DistributionType::kUniform, 90.0f, 360.0f };
  area_axis.roll_dist = { DistributionType::kUniform, 0.0f, 360.0f };
  const UMarginal area = MakeUMarginal(area_axis, sun);  // kArea
  FiberSampleStream wrong_parameter;
  wrong_parameter.binding = MeasureBinding::kFiberParameter;  // the area support has no orbit
  wrong_parameter.samples.push_back(MockSample(0.6, 0.8, 0.0, 0.5, 0.8, 0.0, 0.1));
  const FiberQuadratureResult q2 = QuadratureIntensity(area, wrong_parameter, 1e-9);
  EXPECT_EQ(q2.binding_mismatch, 1);
  EXPECT_EQ(q2.kept, 0);
}

TEST(FiberQuadrature, OutOfRegistryBindingValueIsFailVisible) {
  // The registered table's a50 partner (the quadrature switch's fail-visible default): a
  // binding value OUTSIDE the registry counts in binding_mismatch instead of reading as
  // mu = 0 with the sample silently dropped (indistinguishable from dark).
  const UMarginal measure = PlateMeasure();
  FiberSampleStream stream;
  stream.binding = static_cast<MeasureBinding>(0x2A);  // not a registered value
  stream.samples.push_back(MockSample(1, 0, 0, 0.5, 0.8, 0.0, 0.1));
  const FiberQuadratureResult q = QuadratureIntensity(measure, stream, 1e-9);
  EXPECT_EQ(q.binding_mismatch, 1);
  EXPECT_EQ(q.kept, 0);
  EXPECT_EQ(q.total, 1);
  EXPECT_EQ(q.intensity, 0.0);
}

TEST(FiberQuadrature, SeamCrossingFiberStreamAgreesWithTheCertificate) {
  // The frozen-entry pair test (round 3's certified<->quadrature finding): on a seam-crossing
  // spin-orbit measure (azimuth Uniform(180 +- 20), support [160, 200] deg), a producer
  // emitting parameters in atan2's (-180, 180] convention lands the support's second half at
  // (-180, -160]. The density entries read those WRAPPED — the same authority the certificate's
  // membership leg (MuPositive) uses — so the quadrature must keep exactly the samples the
  // certificate counts in support, with both halves contributing; the unwrapped reading would
  // drop half the lit arc silently.
  double sun[3];
  SunHat(sun);
  AxisDistribution axis;
  axis.azimuth_dist = { DistributionType::kUniform, 180.0f, 40.0f };
  axis.latitude_dist = { DistributionType::kNoRandom, 90.0f, 0.0f };
  axis.roll_dist = { DistributionType::kNoRandom, 0.0f, 0.0f };
  const UMarginal measure = MakeUMarginal(axis, sun);
  ASSERT_EQ(measure.kind(), USupportKind::kSpinOrbit);

  const double dtheta = 5.0 * kDeg;
  // Tolerance note: the probes are built forward through SpinOrbitPoint and MuPositive recovers
  // the parameter via atan2 before rebuilding the orbit point, so the angular distance crosses
  // acos(dot ~ 1) whose error floor is sqrt(machine eps) ~ 1e-8 (acos square-root amplification,
  // the numerical-robustness convention) — 1e-6 matches the file's established MuPositive
  // tolerance and sits well above that floor. Platform rounding (x86_64 / MSVC vs ARM) must not
  // decide the verdict.
  const double kSupportTol = 1e-6;
  // Two points of the raw half (170, 165 deg) and two of the seam-imaged half (190 -> -170,
  // 185 -> -175 deg); the u points are built FROM each sample's own parameter, so any
  // convention mismatch between the density and the map would surface.
  const double params_deg[4] = { 170.0, 165.0, -170.0, -175.0 };
  FiberSampleStream stream;
  stream.binding = MeasureBinding::kFiberParameter;
  stream.evidence = FiberSampleStream::EvidenceForm::kSampledExhaustive;
  double u_known[3];
  for (int i = 0; i < 4; i++) {
    const double theta = params_deg[i] * kDeg;
    double u[3];
    measure.SpinOrbitPoint(theta, u);
    u_known[0] = u[0];
    u_known[1] = u[1];
    u_known[2] = u[2];
    stream.samples.push_back(MockSample(u[0], u[1], u[2], 1.0, 1.0, theta, dtheta));
    // Sanity: each sample sits on the orbit under the certificate's own membership leg.
    EXPECT_TRUE(measure.MuPositive(u, kSupportTol)) << "param " << params_deg[i];
  }
  (void)u_known;

  const FiberQuadratureResult q = QuadratureIntensity(measure, stream, kSupportTol);
  EXPECT_EQ(q.total, 4);
  EXPECT_EQ(q.kept, 4);
  EXPECT_EQ(q.in_support, 4);
  // Uniform(180, 40) carries density 1/(40 deg) per radian (the FULL range is the spread).
  EXPECT_NEAR(q.intensity, 4.0 * (1.0 / (40.0 * kDeg)) * dtheta, 1e-12);

  // The certificate's membership leg reads the same wrapped authority: the same stream is
  // all-lit in-support under exhaustive evidence, so it certifies.
  const VisibilityCertificate cert = CertifyVisibility(measure, stream, nullptr, nullptr, kSupportTol);
  EXPECT_EQ(cert.state, VisibilityState::kCertified);
}

TEST(FiberQuadrature, TintQuotientReportsUndefinedOnDarkDenominator) {
  FiberQuadratureResult blue, red;
  blue.intensity = 0.5;
  red.intensity = 0.0;
  const TintQuotient q = TintRatio(blue, red);
  EXPECT_FALSE(q.defined);
  EXPECT_EQ(q.ratio, 0.0);
  red.intensity = 0.4;
  const TintQuotient ok = TintRatio(blue, red);
  EXPECT_TRUE(ok.defined);
  EXPECT_NEAR(ok.ratio, 1.25, 1e-15);
}

// ===========================================================================
// The #51 anchor: snapshot poses first (they pin the frame wiring), then the class tints.
// ===========================================================================

// The snapshot check's per-pose evaluation (resolved slots, corridor A, path T, outgoing).
struct SnapshotCheck {
  double a_engine;
  double t;
  double outgoing[3];
};

SnapshotCheck EvaluateAt(const PlateTables& t, const std::vector<int>& faces, double n, const double pose[9],
                         const double u_expected[3]) {
  SnapshotCheck out{ 0.0, 0.0, { 0.0, 0.0, 0.0 } };
  std::vector<int> slots(faces.size());
  const analytic::Status st =
      analytic::ResolveFaceSequence(t.normals, faces.data(), static_cast<int>(faces.size()), slots.data());
  EXPECT_EQ(st, analytic::Status::kOk);
  if (st != analytic::Status::kOk) {
    return out;
  }
  const int count = static_cast<int>(faces.size());
  analytic::Corridor corridor(t.normals, t.polys, slots.data(), count);
  double s_hat[3], incident[3];
  SunHat(s_hat);
  Incident(incident);
  analytic::PathOutputs po;
  std::vector<double> segments(static_cast<size_t>(count + 1) * 3);
  std::vector<double> trans(static_cast<size_t>(count));
  po.segment_directions = segments.data();
  po.interface_transmittances = trans.data();
  const bool valid = analytic::EvaluatePath(t.normals, slots.data(), count, n, incident, pose, &po);
  // u = R^T s_hat must reproduce the fixture's body-frame sun direction.
  double u[3];
  for (int r = 0; r < 3; r++) {
    u[r] = pose[r] * s_hat[0] + pose[3 + r] * s_hat[1] + pose[6 + r] * s_hat[2];
    EXPECT_NEAR(u[r], u_expected[r], 1e-12);
  }
  double s_body[3] = { -u[0], -u[1], -u[2] };
  const analytic::EntryMeasure entry = corridor.Evaluate(s_body, n);
  out.a_engine = entry.status == analytic::EntryMeasureStatus::kOk ? entry.value : 0.0;
  out.t = valid ? po.fresnel_transmission : 0.0;
  out.outgoing[0] = po.outgoing_direction[0];
  out.outgoing[1] = po.outgoing_direction[1];
  out.outgoing[2] = po.outgoing_direction[2];
  EXPECT_TRUE(valid);
  return out;
}

TEST(FiberQuadratureC12, SnapshotPosesReproduceTheLI51Fixture) {
  // Five effective poses per class, A / T / outgoing at both indices (A rescaled from LI's
  // hexagon-edge-1 normalisation by kLiAreaPerEngineArea = 4). This pins the world-frame
  // convention, the face numbering and the unit scale BEFORE the integrals are trusted.
  const PlateTables& t = RhombicPlate();
  const std::vector<int> white = { 1, 3, 4, 2 };
  const std::vector<int> blue = { 1, 3, 5, 2 };
  for (const SnapshotPose& snap : kWhiteSnapshots) {
    const SnapshotCheck red = EvaluateAt(t, white, kNRed, snap.rotation, snap.u);
    const SnapshotCheck blue_idx = EvaluateAt(t, white, kNBlue, snap.rotation, snap.u);
    EXPECT_NEAR(red.a_engine, snap.a_red / kLiAreaPerEngineArea, 1e-9 * snap.a_red / kLiAreaPerEngineArea + 1e-15);
    EXPECT_NEAR(blue_idx.a_engine, snap.a_blue / kLiAreaPerEngineArea,
                1e-9 * snap.a_blue / kLiAreaPerEngineArea + 1e-15);
    EXPECT_NEAR(red.t, snap.t_red, 1e-9 * snap.t_red);
    EXPECT_NEAR(blue_idx.t, snap.t_blue, 1e-9 * snap.t_blue);
    for (int i = 0; i < 3; i++) {
      EXPECT_NEAR(red.outgoing[i], snap.outgoing_red[i], 1e-9);
      EXPECT_NEAR(blue_idx.outgoing[i], snap.outgoing_blue[i], 1e-9);
    }
  }
  for (const SnapshotPose& snap : kBlueSnapshots) {
    const SnapshotCheck red = EvaluateAt(t, blue, kNRed, snap.rotation, snap.u);
    const SnapshotCheck blue_idx = EvaluateAt(t, blue, kNBlue, snap.rotation, snap.u);
    EXPECT_NEAR(red.a_engine, snap.a_red / kLiAreaPerEngineArea, 1e-9 * snap.a_red / kLiAreaPerEngineArea + 1e-15);
    EXPECT_NEAR(blue_idx.a_engine, snap.a_blue / kLiAreaPerEngineArea,
                1e-9 * snap.a_blue / kLiAreaPerEngineArea + 1e-15);
    EXPECT_NEAR(red.t, snap.t_red, 1e-9 * snap.t_red);
    EXPECT_NEAR(blue_idx.t, snap.t_blue, 1e-9 * snap.t_blue);
    for (int i = 0; i < 3; i++) {
      EXPECT_NEAR(red.outgoing[i], snap.outgoing_red[i], 1e-9);
      EXPECT_NEAR(blue_idx.outgoing[i], snap.outgoing_blue[i], 1e-9);
    }
  }
}

TEST(FiberQuadratureC12, LabelOrbitHasTheTwentyFourMembers) {
  const std::vector<std::vector<int>> white = LabelOrbit({ 1, 3, 4, 2 });
  EXPECT_EQ(white.size(), 24u) << "the L1/PBD orbit of a generic 4-face sequence is 24 members";
  // The physical L2 members the acceptance record names are among them.
  std::vector<std::vector<int>> expected_l2 = { { 1, 3, 4, 2 }, { 1, 3, 8, 2 } };
  for (const auto& e : expected_l2) {
    EXPECT_NE(std::find(white.begin(), white.end(), e), white.end());
  }
  const std::vector<std::vector<int>> blue = LabelOrbit({ 1, 3, 5, 2 });
  EXPECT_EQ(blue.size(), 24u);
  EXPECT_NE(std::find(blue.begin(), blue.end(), std::vector<int>{ 1, 3, 7, 2 }), blue.end());
}

// The class tint, per target: sums over the 24-member orbit of the per-member spin energies at
// both indices, members assigned by their theta-independent outgoing.
struct ClassTint {
  double ratio[2] = { 0.0, 0.0 };
  bool defined[2] = { false, false };
  int assigned[2] = { 0, 0 };
  int unassigned = 0;
};

ClassTint ClassTintOverOrbit(const std::vector<int>& representative, int grid) {
  const PlateTables& t = RhombicPlate();
  ClassTint out;
  double e_red[2] = { 0.0, 0.0 };
  double e_blue[2] = { 0.0, 0.0 };
  for (const std::vector<int>& member : LabelOrbit(representative)) {
    const MemberEnergy red = EvaluateMember(t, member, kNRed, grid);
    const MemberEnergy blue = EvaluateMember(t, member, kNBlue, grid);
    if (!red.any_valid && !blue.any_valid) {
      continue;  // an unlit member lights neither target (lit_members of the #51 vocabulary)
    }
    const int target = AssignTarget(red.any_valid ? red.outgoing : blue.outgoing);
    if (target < 0) {
      out.unassigned++;
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

TEST(FiberQuadratureC12, WhiteClassTintMatchesTheLI51Anchor) {
  // #51: "each target has blue/red ratio about 1.010 for the white class" — per-target sums
  // over the 24-member L1/PBD orbit of 1-3-4-2, E = (1/2 pi) integral A*T dtheta, 8192-point
  // periodic midpoint grid (the fixture's fine grid; without its 1e-8 transition refinement the
  // grid bias joins the tolerance).
  const ClassTint tint = ClassTintOverOrbit({ 1, 3, 4, 2 }, 8192);
  ASSERT_TRUE(tint.defined[0]);
  ASSERT_TRUE(tint.defined[1]);
  // Both targets carry the class (the partition check of the #51 protocol).
  EXPECT_GT(tint.assigned[0], 0);
  EXPECT_GT(tint.assigned[1], 0);
  EXPECT_NEAR(tint.ratio[0], 1.010, 0.012) << "white class, target +120deg";
  EXPECT_NEAR(tint.ratio[1], 1.010, 0.012) << "white class, target -120deg";
}

TEST(FiberQuadratureC12, BlueClassTintMatchesTheLI51AnchorAndExceedsTheWhiteClass) {
  // #51: "about 1.525 for the blue class" (1-3-5-2), at each target — and the cross-chain
  // direction assertion of the corpus row: the blue class's tint exceeds the white class's.
  const ClassTint tint = ClassTintOverOrbit({ 1, 3, 5, 2 }, 8192);
  ASSERT_TRUE(tint.defined[0]);
  ASSERT_TRUE(tint.defined[1]);
  EXPECT_NEAR(tint.ratio[0], 1.525, 0.02) << "blue class, target +120deg";
  EXPECT_NEAR(tint.ratio[1], 1.525, 0.02) << "blue class, target -120deg";
  EXPECT_GT(tint.ratio[0], 1.12);  // direction: blue > white by far more than either tolerance
  EXPECT_GT(tint.ratio[1], 1.12);
}

TEST(FiberQuadratureC12, LiteralRepresentativeOfTheCarriedClassIsInfeasible) {
  // The negative control of the corpus row: the literal `3-5-6-8` is geometrically IMPOSSIBLE
  // on this crystal — its spin energy is zero at BOTH indices, so a tint computed from the
  // literal representative alone is 0/0 (undefined), while the orbit member `4-8-7-5` carries
  // the class. Tint cannot be read off the literal representative (or from geometric
  // coincidence); it is a per-member per-wavelength sum.
  const PlateTables& t = RhombicPlate();
  const MemberEnergy literal_red = EvaluateMember(t, { 3, 5, 6, 8 }, kNRed, 2048);
  const MemberEnergy literal_blue = EvaluateMember(t, { 3, 5, 6, 8 }, kNBlue, 2048);
  EXPECT_FALSE(literal_red.any_valid);
  EXPECT_FALSE(literal_blue.any_valid);
  EXPECT_EQ(literal_red.e, 0.0);
  EXPECT_EQ(literal_blue.e, 0.0);
  const TintQuotient q =
      TintRatio(FiberQuadratureResult{ literal_blue.e, 0, 0, 0 }, FiberQuadratureResult{ literal_red.e, 0, 0, 0 });
  EXPECT_FALSE(q.defined);  // the tint of the literal representative does not exist

  const MemberEnergy carrier_red = EvaluateMember(t, { 4, 8, 7, 5 }, kNRed, 2048);
  const MemberEnergy carrier_blue = EvaluateMember(t, { 4, 8, 7, 5 }, kNBlue, 2048);
  EXPECT_TRUE(carrier_red.any_valid || carrier_blue.any_valid);
  EXPECT_GT(carrier_red.e + carrier_blue.e, 0.0);
}

}  // namespace
}  // namespace lumice::raypath
