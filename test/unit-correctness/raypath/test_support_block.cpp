// The schema3 support block (src/raypath/detail/schema3/support_block.{hpp,cpp}): the delta-axis
// facts per member — the schema1 reach in target-free form. The C05/C06 exact 120-degree
// complement as interval predicates, the endpoint onsets (every partition endpoint carries one,
// measured on both beta chains), the C06 hole edge as a constant-D_P closed curve with its
// n-continuation dispersion table (the corpus's 149.247@550 / +1.851(400-700) anchors), the
// fail-closed refusal row, and the family aggregate.
//
// symmetry_semantics: label — the 4-8-2-7-5 sibling is a PBD variant of the C06 family (the
// corpus's basal-face family), used for the sharing predicate.
//
// Provenance of the anchors: the partition intervals and the onsets are this tree's own measured
// values (the ABI test pins the same partition numbers); the dispersion anchors are the LI
// closed-form values the corpus records (149.247 / 148.645 / 150.496, +1.851) — the test
// reproduces them through the engine's own IceRefractiveIndex dispersion.

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "core/optics.hpp"
#include "raypath/detail/schema3/support_block.hpp"

namespace lumice::raypath::schema3 {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;

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

Tables Prism() {
  Tables t;
  analytic::CrystalShape shape;
  shape.kind = analytic::CrystalShapeKind::kPrism;
  shape.height = 1.0;
  for (int i = 0; i < 6; i++) {
    shape.face_distance[i] = 1.0;
  }
  EXPECT_EQ(analytic::BuildFaceNormals(shape, &t.normals, &t.polys), analytic::Status::kOk);
  return t;
}

// The engine's own dispersion is the declared n(lambda) authority.
double EngineIndex(double wavelength_nm) {
  return IceRefractiveIndex::Get(wavelength_nm);
}

MemberSupport RowOf(const Tables& t, const std::vector<int>& faces, double base_index,
                    const std::vector<double>& wavelengths = {}, const std::vector<double>& indices = {}) {
  int slots[analytic::kMaxFaceCount];
  const analytic::Status resolve =
      analytic::ResolveFaceSequence(t.normals, faces.data(), static_cast<int>(faces.size()), slots);
  if (resolve != analytic::Status::kOk) {
    ADD_FAILURE() << "face sequence rejected: status " << static_cast<int>(resolve);
    return MemberSupport{};
  }
  return MemberSupportOf(t.normals, t.polys, slots, static_cast<int>(faces.size()), base_index, faces, wavelengths,
                         indices);
}

// Every interval endpoint of a complete row carries an endpoint onset (the measured C05/C06
// shape: each critical cut IS an onset). A structural predicate — no absolute epsilon on any
// particular endpoint value.
void ExpectEndpointsCarryOnsets(const MemberSupport& row) {
  ASSERT_EQ(row.axis.context.coverage, PartitionContext::Coverage::kComplete);
  ASSERT_FALSE(row.axis.intervals.empty());
  ASSERT_FALSE(row.endpoint_onsets.empty());
  for (const analytic::DeviationInterval& interval : row.axis.intervals) {
    bool lower_covered = false;
    bool upper_covered = false;
    for (const analytic::CriticalOnset& onset : row.endpoint_onsets) {
      lower_covered = lower_covered || std::fabs(onset.value - interval.lower) <= analytic::kExtremumAtol;
      upper_covered = upper_covered || std::fabs(onset.value - interval.upper) <= analytic::kExtremumAtol;
    }
    EXPECT_TRUE(lower_covered) << "interval lower endpoint without an onset: " << interval.lower / kDeg;
    EXPECT_TRUE(upper_covered) << "interval upper endpoint without an onset: " << interval.upper / kDeg;
  }
}

// ---- the C05/C06 exact complement -------------------------------------------------------------

TEST(SupportBlock, C05C06ComplementAt120Degrees) {
  const Tables t = Beta();
  const double n550 = EngineIndex(550.0);
  const MemberSupport c05 = RowOf(t, { 4, 8, 7, 5 }, n550);
  const MemberSupport c06 = RowOf(t, { 4, 8, 1, 7, 5 }, n550);

  ASSERT_EQ(c05.axis.intervals.size(), 2u);
  ASSERT_EQ(c06.axis.intervals.size(), 2u);
  // The complement, as interval predicates: C05's upper edge IS C06's lower edge, and both are
  // the corpus's 120-degree cut (nine digits; the shared-edge comparison is the structural form).
  EXPECT_NEAR(c05.axis.intervals.back().upper / kDeg, 120.0, 1e-6);
  EXPECT_NEAR(c06.axis.intervals.front().lower / kDeg, 120.0, 1e-6);
  EXPECT_LE(std::fabs(c05.axis.intervals.back().upper - c06.axis.intervals.front().lower), 1e-9);
  // And every endpoint of both rows carries its onset.
  ExpectEndpointsCarryOnsets(c05);
  ExpectEndpointsCarryOnsets(c06);
}

// ---- the C06 hole edge: the constant closed curve with its dispersion table ---------------------

TEST(SupportBlock, C06HoleEdgeConstantCurveWithDispersion) {
  const Tables t = Beta();
  const std::vector<double> wavelengths = { 400.0, 550.0, 700.0 };
  std::vector<double> indices;
  for (const double wl : wavelengths) {
    indices.push_back(EngineIndex(wl));
  }
  const MemberSupport c06 = RowOf(t, { 4, 8, 1, 7, 5 }, EngineIndex(550.0), wavelengths, indices);

  // Exactly one constant closed curve: the basal TIR kink's closed-form circle (the corpus's
  // "常值闭式圆", the hole edge at ~150).
  ASSERT_EQ(c06.constant_curves.size(), 1u);
  const ConstantDeltaCurve& curve = c06.constant_curves[0];
  EXPECT_EQ(curve.weight_step, 2);
  EXPECT_NEAR(curve.d_p / kDeg, 149.246751931, 1e-6);  // measured, = the corpus's 149.247 @550
  ASSERT_EQ(curve.critical_d_p.size(), 3u);
  EXPECT_NEAR(curve.critical_d_p[0] / kDeg, 150.495978844, 1e-6);  // @400
  EXPECT_NEAR(curve.critical_d_p[1] / kDeg, 149.246751931, 1e-6);  // @550
  EXPECT_NEAR(curve.critical_d_p[2] / kDeg, 148.645349657, 1e-6);  // @700
  // The corpus's dispersion anchor: the blue edge is DEEPER (larger delta) than the red one,
  // by +1.851 deg across 400-700 (measured +1.8506 through the engine's own dispersion).
  const double dispersion = curve.critical_d_p[0] - curve.critical_d_p[2];
  EXPECT_NEAR(dispersion / kDeg, 1.851, 5e-4);
  EXPECT_GT(dispersion, 0.0);
}

// ---- the fail-closed refusal row ---------------------------------------------------------------

TEST(SupportBlock, RefusedPartitionIsAFailClosedRow) {
  const Tables t = Prism();
  // The real refusal the LI conclusions named: 3-1-5-7's crease touching dU_P.
  const MemberSupport refused = RowOf(t, { 3, 1, 5, 7 }, 1.3110129);
  EXPECT_EQ(refused.axis.context.coverage, PartitionContext::Coverage::kIncomplete);
  EXPECT_EQ(refused.axis.regime_slug, "slab_crease_touching");
  EXPECT_TRUE(refused.axis.intervals.empty());
  EXPECT_TRUE(refused.endpoint_onsets.empty());
  EXPECT_TRUE(refused.constant_curves.empty());
  EXPECT_FALSE(refused.axis.message.empty());
}

// ---- the family aggregate -----------------------------------------------------------------------

TEST(SupportBlock, AggregateFamilySharesIdenticalSupports) {
  const Tables t = Beta();
  const double n550 = EngineIndex(550.0);
  // The C06 family's two members (the literal and a PBD sibling): shared support.
  const MemberSupport c06a = RowOf(t, { 4, 8, 1, 7, 5 }, n550);
  const MemberSupport c06b = RowOf(t, { 4, 8, 2, 7, 5 }, n550);
  const FamilySupport family = AggregateFamily({ c06a, c06b });
  EXPECT_TRUE(family.shared);
  ASSERT_EQ(family.intervals.size(), 2u);
  EXPECT_NEAR(family.intervals.front().lower / kDeg, 120.0, 1e-6);

  // The negative control: C05 and C06 are different supports, the aggregate refuses.
  const MemberSupport c05 = RowOf(t, { 4, 8, 7, 5 }, n550);
  const FamilySupport mixed = AggregateFamily({ c05, c06a });
  EXPECT_FALSE(mixed.shared);
  EXPECT_TRUE(mixed.intervals.empty());
}

}  // namespace
}  // namespace lumice::raypath::schema3
