// The visibility certificate (src/raypath/detail/measure/visibility_certificate.hpp): the
// four-state routing against mock measures and mock streams, every leg pinned separately — the
// C09 "contour present, no passage" recomputation, the escape/truncated fail-closed routing, the
// evidence-form gate, and the A/T boundary discrimination. The measures are the real declared
// densities (the plate family of the C12 config and a zonal column), the streams are synthetic.
//
// symmetry_semantics: none — measures and mock streams, not face paths.

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "raypath/detail/measure/declared_density.hpp"
#include "raypath/detail/measure/measure_geometry_contract.hpp"
#include "raypath/detail/measure/visibility_certificate.hpp"

namespace lumice::raypath {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;

Distribution NoRandom(double v) {
  return { DistributionType::kNoRandom, static_cast<float>(v), 0.0f };
}
Distribution Uniform(double center, double range) {
  return { DistributionType::kUniform, static_cast<float>(center), static_cast<float>(range) };
}
Distribution Gauss(double mean, double std) {
  return { DistributionType::kGaussian, static_cast<float>(mean), static_cast<float>(std) };
}

// The plate family measure of the C12 scene (sun altitude 9 degrees on the x-z plane meridian):
// the u-support is the spin orbit, and the test builds its samples ON that orbit.
UMarginal PlateMeasure() {
  const double alt = 9.0 * kDeg;
  const double s[3] = { std::cos(alt), 0.0, std::sin(alt) };
  AxisDistribution axis;
  axis.azimuth_dist = Uniform(0.0, 360.0);
  axis.latitude_dist = NoRandom(90.0);
  axis.roll_dist = NoRandom(0.0);
  return MakeUMarginal(axis, s);
}

FiberSample OnOrbitSample(const UMarginal& measure, double theta, double area, double transmission, double weight) {
  FiberSample s;
  measure.SpinOrbitPoint(theta, s.u);
  s.area = area;
  s.transmission = transmission;
  s.valid = area * transmission > 0.0;
  s.parameter = theta;
  s.weight = weight;
  return s;
}

CriticalSetCurve OrbitKind1Curve(const UMarginal& measure, int points) {
  // A kind-1 contour ON the measure's support (C09's shape: the contour is inside the ensemble's
  // declared orientations, so "unlit" is about the passage, not the support).
  CriticalSetCurve curve;
  curve.existence = ExistenceState::kComputed;
  curve.u.resize(3 * static_cast<size_t>(points));
  for (int i = 0; i < points; i++) {
    measure.SpinOrbitPoint(2.0 * kPi * i / points, &curve.u[3 * i]);
  }
  return curve;
}

TEST(VisibilityCertificate, AllLitExhaustiveIsCertified) {
  const UMarginal measure = PlateMeasure();
  FiberSampleStream stream;
  stream.evidence = FiberSampleStream::EvidenceForm::kSampledExhaustive;
  stream.binding = MeasureBinding::kFiberParameter;
  for (int i = 0; i < 16; i++) {
    stream.samples.push_back(OnOrbitSample(measure, 2.0 * kPi * i / 16, 0.5, 0.8, 1.0 / 16.0));
  }
  const VisibilityCertificate cert = CertifyVisibility(measure, stream, nullptr, nullptr, 1e-6);
  EXPECT_EQ(cert.state, VisibilityState::kCertified);
  EXPECT_TRUE(cert.jets_ok);
  EXPECT_NEAR(cert.lit_fraction, 1.0, 1e-12);
  EXPECT_EQ(std::string(cert.reason), "");
}

TEST(VisibilityCertificate, MixedLitIsPartialWithMeasureWeightedFraction) {
  const UMarginal measure = PlateMeasure();
  FiberSampleStream stream;
  stream.evidence = FiberSampleStream::EvidenceForm::kSampledExhaustive;
  for (int i = 0; i < 10; i++) {
    // Weights: 3 lit samples carry 0.25 total, 7 dark carry 0.75 — lit_fraction must be the
    // WEIGHTED fraction (0.25), not the count fraction (0.3): the measure the evidence covers.
    const bool lit = i < 3;
    stream.samples.push_back(OnOrbitSample(measure, 0.4 * i, lit ? 0.5 : 0.0, 0.8, lit ? 1.0 / 12.0 : 3.0 / 28.0));
  }
  const VisibilityCertificate cert = CertifyVisibility(measure, stream, nullptr, nullptr, 1e-6);
  EXPECT_EQ(cert.state, VisibilityState::kPartial);
  EXPECT_NEAR(cert.lit_fraction, 0.25, 1e-12);
}

TEST(VisibilityCertificate, C09ContourPresentNoPassageIsUnlit) {
  // THE C09 recomputation: a kind-1 curve present on the support, mu > 0, and every fiber
  // candidate dark (A*T == 0) under exhaustive evidence — the precise "the isophot exists, the
  // light does not pass" verdict. A = 0 and T = 0 candidates both count as dark, and the result
  // names which boundary it saw.
  const UMarginal measure = PlateMeasure();
  FiberSampleStream stream;
  stream.evidence = FiberSampleStream::EvidenceForm::kSampledExhaustive;
  for (int i = 0; i < 12; i++) {
    const bool corridor = i % 2 == 0;
    // Half the candidates: corridor closed (A = 0, T > 0); half: transmission gate (T = 0, A > 0).
    stream.samples.push_back(
        OnOrbitSample(measure, 2.0 * kPi * i / 12, corridor ? 0.0 : 0.4, corridor ? 0.7 : 0.0, 1.0 / 12.0));
  }
  const CriticalSetCurve curve = OrbitKind1Curve(measure, 12);
  const VisibilityCertificate cert = CertifyVisibility(measure, stream, &curve, nullptr, 1e-6);
  EXPECT_EQ(cert.state, VisibilityState::kUnlit);
  EXPECT_TRUE(cert.saw_zero_area);
  EXPECT_TRUE(cert.saw_zero_transmission);
}

TEST(VisibilityCertificate, AllDarkWithoutContourIsUnprovenNotUnlit) {
  // The negative control of C09: darkness alone does not assert the object's presence — without
  // the kind-1 curve the verdict defers (a silent unlit here would read as "the feature is there
  // but dark", which nothing vouches).
  const UMarginal measure = PlateMeasure();
  FiberSampleStream stream;
  stream.evidence = FiberSampleStream::EvidenceForm::kSampledExhaustive;
  for (int i = 0; i < 8; i++) {
    stream.samples.push_back(OnOrbitSample(measure, 0.8 * i, 0.0, 0.5, 0.1));
  }
  const VisibilityCertificate cert = CertifyVisibility(measure, stream, nullptr, nullptr, 1e-6);
  EXPECT_EQ(cert.state, VisibilityState::kUnproven);
  EXPECT_EQ(std::string(cert.reason), "no_kind1_curve");
}

TEST(VisibilityCertificate, AllDarkUnderSampledPartialDefers) {
  // Even WITH the contour, spot evidence cannot certify "no passage anywhere" — the universal
  // statement needs the declared coverage form.
  const UMarginal measure = PlateMeasure();
  FiberSampleStream stream;
  stream.evidence = FiberSampleStream::EvidenceForm::kSampledPartial;
  for (int i = 0; i < 8; i++) {
    stream.samples.push_back(OnOrbitSample(measure, 0.8 * i, 0.0, 0.5, 0.1));
  }
  const CriticalSetCurve curve = OrbitKind1Curve(measure, 8);
  const VisibilityCertificate cert = CertifyVisibility(measure, stream, &curve, nullptr, 1e-6);
  EXPECT_EQ(cert.state, VisibilityState::kUnproven);
  EXPECT_EQ(std::string(cert.reason), "sampled_partial_evidence");
}

TEST(VisibilityCertificate, EscapeAndTruncatedRouteFailClosed) {
  const UMarginal measure = PlateMeasure();
  FiberSampleStream stream;
  stream.evidence = FiberSampleStream::EvidenceForm::kStructural;
  for (int i = 0; i < 8; i++) {
    stream.samples.push_back(OnOrbitSample(measure, 0.8 * i, 0.5, 0.7, 0.1));
  }
  PartitionContext escaped;
  escaped.coverage = PartitionContext::Coverage::kIncomplete;
  escaped.escape_regime = EscapeRegime::kSlabCrease;
  EXPECT_EQ(CertifyVisibility(measure, stream, nullptr, &escaped, 1e-6).state, VisibilityState::kUnproven);

  CriticalSetCurve truncated;
  truncated.existence = ExistenceState::kWalkTruncated;
  const VisibilityCertificate cert = CertifyVisibility(measure, stream, &truncated, nullptr, 1e-6);
  EXPECT_EQ(cert.state, VisibilityState::kUnproven);
  EXPECT_EQ(std::string(cert.reason), "kind1_walk_truncated");

  CriticalSetCurve escaped_curve;
  escaped_curve.existence = ExistenceState::kEscaped;
  EXPECT_EQ(CertifyVisibility(measure, stream, &escaped_curve, nullptr, 1e-6).reason, std::string("kind1_escaped"));
}

TEST(VisibilityCertificate, UnknownPartitionDoesNotBlockCertified) {
  // A per-point visibility certificate is local: the v1 producer shape (no partition run yet)
  // can still certify its own samples — the object's bucket placement consumes the partition
  // separately. A NULL partition is the same statement.
  const UMarginal measure = PlateMeasure();
  FiberSampleStream stream;
  stream.evidence = FiberSampleStream::EvidenceForm::kSampledExhaustive;
  for (int i = 0; i < 8; i++) {
    stream.samples.push_back(OnOrbitSample(measure, 0.8 * i, 0.5, 0.7, 0.1));
  }
  PartitionContext unknown;
  unknown.coverage = PartitionContext::Coverage::kUnknown;
  EXPECT_EQ(CertifyVisibility(measure, stream, nullptr, &unknown, 1e-6).state, VisibilityState::kCertified);
  EXPECT_EQ(CertifyVisibility(measure, stream, nullptr, nullptr, 1e-6).state, VisibilityState::kCertified);
}

TEST(VisibilityCertificate, OffSupportSamplesAreNotEvidence) {
  // Samples outside the declared measure are not evidence about THIS object: all of them dark
  // with no in-support sample routes to unproven (no_in_support_samples), never to unlit — and a
  // stream whose ONLY lit sample is off-support stays unproven too.
  const UMarginal measure = PlateMeasure();
  FiberSampleStream stream;
  stream.evidence = FiberSampleStream::EvidenceForm::kSampledExhaustive;
  for (int i = 0; i < 8; i++) {
    FiberSample s;
    const double lat = 40.0 * kDeg;  // far off the 9-degree orbit
    s.u[0] = std::cos(lat) * std::cos(0.3 * i);
    s.u[1] = std::cos(lat) * std::sin(0.3 * i);
    s.u[2] = std::sin(lat);
    s.area = 0.5;
    s.transmission = 0.7;
    s.valid = true;
    s.weight = 0.1;
    stream.samples.push_back(s);
  }
  const CriticalSetCurve curve = OrbitKind1Curve(measure, 8);
  const VisibilityCertificate cert = CertifyVisibility(measure, stream, &curve, nullptr, 1e-6);
  EXPECT_EQ(cert.state, VisibilityState::kUnproven);
  EXPECT_EQ(std::string(cert.reason), "no_in_support_samples");
}

TEST(VisibilityCertificate, DegenerateSunMeasureRoutesUnproven) {
  const double pole_sun[3] = { 0.0, 1e-13, 1.0 };
  AxisDistribution axis;
  axis.azimuth_dist = Uniform(0.0, 360.0);
  axis.latitude_dist = Gauss(60.0, 5.0);
  axis.roll_dist = Uniform(0.0, 360.0);
  const UMarginal measure = MakeUMarginal(axis, pole_sun);
  ASSERT_EQ(measure.kind(), USupportKind::kDegenerateSunGeometry);
  FiberSampleStream stream;
  stream.evidence = FiberSampleStream::EvidenceForm::kStructural;
  const VisibilityCertificate cert = CertifyVisibility(measure, stream, nullptr, nullptr, 1e-6);
  EXPECT_EQ(cert.state, VisibilityState::kUnproven);
  EXPECT_EQ(std::string(cert.reason), "degenerate_measure");
}

TEST(VisibilityCertificate, RegisteredStatesWalk) {
  ASSERT_EQ(RegisteredVisibilityStates().size(), 4);
  EXPECT_STREQ(VisibilityStateName(VisibilityState::kCertified), "certified");
  EXPECT_STREQ(VisibilityStateName(VisibilityState::kUnlit), "unlit");
}

}  // namespace
}  // namespace lumice::raypath
