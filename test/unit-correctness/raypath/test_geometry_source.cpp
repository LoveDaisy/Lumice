// The schema3 geometry adapter (src/raypath/detail/schema3/geometry_source.{hpp,cpp}): the
// production consumer the contour docking test carried its mappings for. The walk-status ->
// existence reading per kernel value, the mirrored-struct -> contract conversions, the escape
// regime slug table (the G3 interim: kernel slugs are data, the contract's typed field is not
// authoritative until the owner rules), the beta crystal's partition anchors through the C++
// direct path (the ABI test pins the same numbers — the adapter must not drift), and the G5
// u-fidelity verification (the contract's pre-registered integration gap: producer streams carry
// the TRUE body-frame sun direction, verified by the certificate's own membership predicate).
//
// symmetry_semantics: none — fixed face sequences and declared measures, no filter expansion.

#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "analytic/dp_boundary.hpp"
#include "analytic/dp_contour.hpp"
#include "analytic/dp_field.hpp"
#include "analytic/dp_partition.hpp"
#include "core/crystal_param.hpp"
#include "raypath/detail/measure/declared_density.hpp"
#include "raypath/detail/measure/visibility_certificate.hpp"
#include "raypath/detail/schema3/geometry_source.hpp"

namespace lumice::raypath::schema3 {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;
// LI conventions #22-ish: the 550 nm engine index (test_dp_capi.cpp's kN550) and the pool ends.
constexpr double kN550 = 1.3110129;
constexpr double kNRed = 1.307;
constexpr double kNBlue = 1.317;

struct Tables {
  analytic::FaceNormalTable normals;
  analytic::FacePolygonTable polys;
};

// test_dp_capi.cpp's Beta: prism h = 3, fd = [2, 1, 1, 2, 1, 1] (the C05/C06 crystal).
Tables Beta() {
  Tables t;
  analytic::CrystalShape shape;
  shape.kind = analytic::CrystalShapeKind::kPrism;
  shape.height = 3.0;
  const double fd[6] = { 2.0, 1.0, 1.0, 2.0, 1.0, 1.0 };
  for (int i = 0; i < 6; i++) {
    shape.face_distance[i] = fd[i];
  }
  EXPECT_EQ(analytic::BuildFaceNormals(shape, &t.normals, &t.polys), analytic::Status::kOk);
  return t;
}

// test_dp_capi.cpp's Prism: the hexagonal prism, all face distances 1.
Tables Prism(double height = 1.0) {
  Tables t;
  analytic::CrystalShape shape;
  shape.kind = analytic::CrystalShapeKind::kPrism;
  shape.height = height;
  for (int i = 0; i < 6; i++) {
    shape.face_distance[i] = 1.0;
  }
  EXPECT_EQ(analytic::BuildFaceNormals(shape, &t.normals, &t.polys), analytic::Status::kOk);
  return t;
}

analytic::DeviationField FieldOf(const Tables& t, const std::vector<int>& faces, double index,
                                 analytic::Status* resolve_status) {
  int slots[analytic::kMaxFaceCount];
  *resolve_status = analytic::ResolveFaceSequence(t.normals, faces.data(), static_cast<int>(faces.size()), slots);
  // The caller ASSERTs on *resolve_status before using the field (a failed resolve leaves the
  // slots garbage, and a field built on them segfaults — the fatal assert IS the guard).
  return analytic::DeviationField(t.normals, t.polys, slots, static_cast<int>(faces.size()), index);
}

Distribution NoRandom(double v) {
  return { DistributionType::kNoRandom, static_cast<float>(v), 0.0f };
}
Distribution Uniform(double center, double range) {
  return { DistributionType::kUniform, static_cast<float>(center), static_cast<float>(range) };
}

// The C12 plate measure: the declared axis whose u-support is the orbit the producer samples.
UMarginal PlateMeasure() {
  const double alt = 9.0 * kDeg;
  const double s[3] = { std::cos(alt), 0.0, std::sin(alt) };
  AxisDistribution axis;
  axis.azimuth_dist = Uniform(0.0, 360.0);
  axis.latitude_dist = NoRandom(90.0);
  axis.roll_dist = NoRandom(0.0);
  return MakeUMarginal(axis, s);
}

analytic::PoseDensitySpec PlateSpec() {
  analytic::PoseDensitySpec spec;
  spec.family = analytic::PoseFamily::kPlate;
  spec.zenith_mean_deg = 0.0;
  spec.zenith_std_deg = 1.0;
  return spec;
}

// ---- the walk-status -> existence reading (per kernel value) ---------------------------------

TEST(GeometrySource, WalkStatusMappingPerValue) {
  // The mapping's own authority is dp_contour's ruling; this row walk pins every kernel status's
  // destination so a new status cannot silently change sides.
  const std::vector<std::pair<analytic::WalkStatus, ExistenceState>> rows = {
    { analytic::WalkStatus::kOk, ExistenceState::kComputed },
    { analytic::WalkStatus::kStepsExhausted, ExistenceState::kWalkTruncated },
    { analytic::WalkStatus::kStartNoPoint, ExistenceState::kWalkTruncated },
    { analytic::WalkStatus::kStartCoversAll, ExistenceState::kWalkTruncated },
    { analytic::WalkStatus::kStartNoEdge, ExistenceState::kWalkTruncated },
    { analytic::WalkStatus::kCornerNotSimple, ExistenceState::kWalkTruncated },
    { analytic::WalkStatus::kNotClosed, ExistenceState::kWalkTruncated },
    { analytic::WalkStatus::kNotFinite, ExistenceState::kWalkTruncated },
    { analytic::WalkStatus::kBadOrientation, ExistenceState::kWalkTruncated },
  };
  for (const auto& row : rows) {
    EXPECT_EQ(ExistenceOfWalkStatus(row.first), row.second) << "status " << analytic::WalkStatusName(row.first);
  }
  // kEscaped is partition vocabulary: no walk status produces it.
  for (const auto& row : rows) {
    EXPECT_NE(ExistenceOfWalkStatus(row.first), ExistenceState::kEscaped);
  }
}

TEST(GeometrySource, CurveExistenceMappingIsTotal) {
  EXPECT_EQ(ExistenceOf(analytic::CurveExistence::kComputed), ExistenceState::kComputed);
  EXPECT_EQ(ExistenceOf(analytic::CurveExistence::kEscaped), ExistenceState::kEscaped);
  EXPECT_EQ(ExistenceOf(analytic::CurveExistence::kWalkTruncated), ExistenceState::kWalkTruncated);
  EXPECT_EQ(ExistenceOf(analytic::CurveExistence::kS4Declared), ExistenceState::kS4Declared);
}

// ---- the chain / curve / stream conversions --------------------------------------------------

TEST(GeometrySource, ChainMappingCarriesFieldsAndStopsAtTheContractVocabulary) {
  analytic::ChainCurve chain;
  chain.is_gate_boundary = false;  // kind-3
  chain.existence = analytic::CurveExistence::kWalkTruncated;
  chain.u = { 0.0, 0.0, 1.0, 0.0, 1.0, 0.0 };
  chain.param = { 0.0, 1.5 };
  chain.kink = { 1, 0 };
  chain.event = { -1, -1 };
  chain.closed = false;
  const WeightSingularChain out = ChainOf(chain);
  EXPECT_FALSE(out.is_gate_boundary);
  EXPECT_EQ(out.existence, ExistenceState::kWalkTruncated);
  ASSERT_EQ(out.u.size(), 6u);
  ASSERT_EQ(out.param.size(), 2u);
  ASSERT_EQ(out.kink.size(), 2u);
  ASSERT_EQ(out.event.size(), 2u);
  EXPECT_FALSE(out.closed);
}

// ---- the escape regime slug table (G3's interim evidence) -------------------------------------

TEST(GeometrySource, RegimeSlugMappingWalksBothTables) {
  // The contract's own registered value maps by name — the mechanism the future G3 ruling will
  // extend without touching this function.
  EscapeRegime out = EscapeRegime::kSlabCrease;
  EXPECT_TRUE(ContractRegimeOfSlug(EscapeRegimeName(EscapeRegime::kSlabCrease), &out));
  EXPECT_EQ(out, EscapeRegime::kSlabCrease);
  // Unknown slugs fail closed: false, out untouched.
  out = EscapeRegime::kSlabCrease;
  EXPECT_FALSE(ContractRegimeOfSlug("not_a_registered_slug", &out));
  EXPECT_EQ(out, EscapeRegime::kSlabCrease);
  // G3's evidence, pinned: the port names each refusal separately and NONE of its slugs is the
  // contract's registered "slab_crease" today — every kernel regime is unrepresentable in the
  // contract's typed field, which is why the slug flows as data at the object layer (the
  // escalation carries this row).
  const analytic::EscapeRegime kernel_regimes[] = {
    analytic::EscapeRegime::kNotDiskUnaudited,
    analytic::EscapeRegime::kNotDiskUnconverged,
    analytic::EscapeRegime::kNotDiskConfirmed,
    analytic::EscapeRegime::kNotDiskCorrected,
    analytic::EscapeRegime::kSlabCreaseContradiction,
    analytic::EscapeRegime::kSlabCreaseNotCarried,
    analytic::EscapeRegime::kSlabCreaseTouching,
    analytic::EscapeRegime::kSlabCreaseClosedRidge,
    analytic::EscapeRegime::kMultipleInteriorCriticalPoints,
    analytic::EscapeRegime::kLoopExtremaNotAlternating,
    analytic::EscapeRegime::kOddBoundaryCrossings,
    analytic::EscapeRegime::kInteriorCriticalPointNotSimple,
    analytic::EscapeRegime::kSublevelNotReachingBoundary,
  };
  ASSERT_EQ(RegisteredEscapeRegimes().size(), 1u);  // the contract table's current size
  for (const analytic::EscapeRegime regime : kernel_regimes) {
    const char* slug = analytic::EscapeRegimeName(regime);
    if (slug == nullptr) {
      ADD_FAILURE() << "kernel regime without a name: " << static_cast<int>(regime);
      continue;
    }
    EXPECT_STRNE(slug, "unknown_escape_regime") << "unnammed kernel regime " << static_cast<int>(regime);
    EXPECT_FALSE(ContractRegimeOfSlug(slug, nullptr))
        << "kernel slug '" << slug << "' unexpectedly matches a contract value; update the G3 row";
  }
}

// ---- the beta crystal partition anchors (the C++ direct path) ---------------------------------

TEST(GeometrySource, BetaPartitionAnchor4875MatchesTheAbiPin) {
  // test_dp_capi.cpp PartitionReplaysTheBetaAnchorAndTheMechanicalInvariants, through the C++
  // adapter: two intervals [0, 50.161740...] and [..., 120.000000...], the adjacency and the
  // component-count identities. The adapter must not drift from the ABI's own numbers.
  const Tables t = Beta();
  analytic::Status resolve = analytic::Status::kInvalidConfig;
  analytic::DeviationField field = FieldOf(t, { 4, 8, 7, 5 }, kN550, &resolve);
  ASSERT_EQ(resolve, analytic::Status::kOk);
  const PartitionedAxis axis = PartitionAxisOf(field);
  EXPECT_TRUE(axis.walk_closed);
  EXPECT_EQ(axis.context.coverage, PartitionContext::Coverage::kComplete);
  ASSERT_EQ(axis.intervals.size(), 2u);
  EXPECT_NEAR(axis.intervals[0].lower / kDeg, 0.0, 1e-9);
  EXPECT_NEAR(axis.intervals[0].upper / kDeg, 50.161740000307894, 1e-9);
  EXPECT_NEAR(axis.intervals[1].upper / kDeg, 120.00000000000001, 1e-9);
  for (size_t i = 0; i < axis.intervals.size(); i++) {
    EXPECT_EQ(axis.intervals[i].n_components, axis.intervals[i].n_closed + axis.intervals[i].n_open);
    if (i + 1 < axis.intervals.size()) {
      EXPECT_EQ(axis.intervals[i].upper, axis.intervals[i + 1].lower);
    }
  }
  // The C05 support's upper edge IS the 120-degree blade (the corpus's nine-digit value): the
  // support block's complementary edge with C06 hangs off this number.
  EXPECT_NEAR(axis.intervals.back().upper / kDeg, 120.0, 1e-6);
}

// The C06 chain's own partition: the display support's LOWER edge at the same 120-degree cut —
// the machine form of the C05/C06 exact complement. The partition's upper edge is the antipode
// (180 degrees); the corpus's display-support upper edge 179.69 is the MC-OBSERVABLE bound (the
// energy-bearing subset of the partition support — a 666.2 mc_evidence concern, not this
// block's), the same relationship as C05's corpus lower edge 0.54 against this partition's 0.
TEST(GeometrySource, BetaPartitionAnchor48175SharesThe120DegreeCut) {
  const Tables t = Beta();
  analytic::Status resolve = analytic::Status::kInvalidConfig;
  analytic::DeviationField field = FieldOf(t, { 4, 8, 1, 7, 5 }, kN550, &resolve);
  ASSERT_EQ(resolve, analytic::Status::kOk);
  const PartitionedAxis axis = PartitionAxisOf(field);
  EXPECT_TRUE(axis.walk_closed);
  EXPECT_EQ(axis.context.coverage, PartitionContext::Coverage::kComplete);
  ASSERT_EQ(axis.intervals.size(), 2u);
  // The support's lower edge = the shared cut (the corpus's nine-digit value, structural
  // predicate); the upper edge is the antipode.
  EXPECT_NEAR(axis.intervals.front().lower / kDeg, 120.0, 1e-6);
  EXPECT_NEAR(axis.intervals.front().upper / kDeg, 151.667407700, 1e-6);
  EXPECT_NEAR(axis.intervals.back().upper / kDeg, 180.0, 1e-9);
  for (size_t i = 0; i < axis.intervals.size(); i++) {
    EXPECT_EQ(axis.intervals[i].n_components, axis.intervals[i].n_closed + axis.intervals[i].n_open);
    if (i + 1 < axis.intervals.size()) {
      EXPECT_EQ(axis.intervals[i].upper, axis.intervals[i + 1].lower);
    }
  }
}

// ---- the partition's refusal forms --------------------------------------------------------------

TEST(GeometrySource, EscapeFormCarriesSlugAsDataWithNoIntervals) {
  // The LI-parity refusal shapes, on the REAL paths conclusions section 4 row 5 named: the
  // uniform prism's 3-1-5-7 (crease touching dU_P) escapes the partition — coverage incomplete,
  // the regime slug named, no intervals (PartitionResult's mechanical invariant), the slug has
  // no contract-side enum value (G3 pending), and the certificate consumes the context
  // fail-closed (partition_escape, unproven).
  const Tables t = Prism();
  analytic::Status resolve = analytic::Status::kInvalidConfig;
  analytic::DeviationField field = FieldOf(t, { 3, 1, 5, 7 }, kN550, &resolve);
  ASSERT_EQ(resolve, analytic::Status::kOk);
  const PartitionedAxis axis = PartitionAxisOf(field);
  EXPECT_TRUE(axis.walk_closed);
  EXPECT_EQ(axis.context.coverage, PartitionContext::Coverage::kIncomplete);
  EXPECT_EQ(axis.regime_slug, "slab_crease_touching");
  EXPECT_TRUE(axis.intervals.empty());
  EXPECT_FALSE(axis.message.empty());
  EscapeRegime matched = EscapeRegime::kSlabCrease;
  EXPECT_FALSE(ContractRegimeOfSlug(axis.regime_slug, &matched));  // G3: unrepresentable today
  // The certificate's consumption of the refusal: unproven with its own reason, whatever the
  // stream says.
  UMarginal measure = PlateMeasure();
  FiberSampleStream stream;
  stream.evidence = FiberSampleStream::EvidenceForm::kSampledExhaustive;
  for (int i = 0; i < 8; i++) {
    FiberSample s;
    measure.SpinOrbitPoint(0.8 * i, s.u);
    s.area = 0.5;
    s.transmission = 0.7;
    s.valid = true;
    s.weight = 0.1;
    stream.samples.push_back(s);
  }
  const VisibilityCertificate cert = CertifyVisibility(measure, stream, nullptr, &axis.context, 1e-6);
  EXPECT_EQ(cert.state, VisibilityState::kUnproven);
  EXPECT_EQ(std::string(cert.reason), "partition_escape");
}

// ---- G5: the producer feeds certificate-grade u (the contract's pre-registered gap) ------------

TEST(GeometrySource, OrbitStreamCarriesTrueBodyFrameU) {
  const Tables t = Prism();
  // The C12-shaped plate orbit on the rhombic plate (the docking test's crystal).
  Tables plate;
  analytic::CrystalShape shape;
  shape.kind = analytic::CrystalShapeKind::kPrism;
  shape.height = 1.0;
  const double fd[6] = { 1.5, 1, 1, 1.5, 1, 1 };
  for (int i = 0; i < 6; i++) {
    shape.face_distance[i] = fd[i];
  }
  EXPECT_EQ(analytic::BuildFaceNormals(shape, &plate.normals, &plate.polys), analytic::Status::kOk);
  double sun[3];
  const double alt = 9.0 * kDeg;
  sun[0] = std::cos(alt);
  sun[1] = 0.0;
  sun[2] = std::sin(alt);
  const std::vector<int> faces = { 1, 3, 4, 2 };
  int slots[analytic::kMaxFaceCount];
  ASSERT_EQ(analytic::ResolveFaceSequence(plate.normals, faces.data(), static_cast<int>(faces.size()), slots),
            analytic::Status::kOk);
  const analytic::OrbitFiberStream orbit =
      analytic::MakeOrbitFiberStream(plate.normals, plate.polys, slots, 4, PlateSpec(), sun, kNRed, 256);
  ASSERT_EQ(orbit.samples.size(), 256u);
  const UMarginal measure = PlateMeasure();
  ASSERT_EQ(measure.kind(), USupportKind::kSpinOrbit);
  int in_support = 0;
  for (const analytic::OrbitFiberPoint& p : orbit.samples) {
    const double u2 = p.u[0] * p.u[0] + p.u[1] * p.u[1] + p.u[2] * p.u[2];
    EXPECT_NEAR(u2, 1.0, 1e-12) << "u not unit at theta=" << p.parameter;
    if (measure.MuPositive(p.u, 1e-6)) {
      in_support++;  // the certificate's own membership predicate: placeholder u fails ALL of these
    }
  }
  // G5's answer: the producer feeds the TRUE body-frame sun direction (a placeholder fails every
  // sample; the measured roundtrip worst case is ~1.5e-8 rad). The tolerance is 1e-6, not
  // 1e-9, and that is a MEASURED finding this test pins: MuPositive's angle_to is acos of a dot
  // product at ~1, whose resolution floor is sqrt(ulp) ~= 1.5e-8 rad — a tolerance below that
  // rejects genuine on-orbit samples on rounding dice (5/256 at 1e-9, measured). The adapter's
  // certificate calls therefore pass orbit tolerances at 1e-6; the finding is registered in the
  // task's G ledger (the acos-floor criterion belongs to doc/numerical-robustness.md's rule 1).
  EXPECT_EQ(in_support, 256) << "a producer u off the declared orbit is the G5 defect shape";
}

}  // namespace
}  // namespace lumice::raypath::schema3
