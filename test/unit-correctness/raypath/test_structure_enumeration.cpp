// The schema3 enumeration driver (src/raypath/detail/schema3/structure_enumeration.hpp): the
// S1-S6 legs the v1 producers exist for, assembled into object records. Pinned here: the C09
// unlit shape through real producer feedstock (a family-pinned, fully-dark member on the C12
// plate — the corpus's cone-crystal fixture itself is registered for 666.4), the C11 family
// collapse identity fields (spherical delta vs the azimuth difference), the beta crystal's S2
// chain objects, the vocabulary coverage table (the ledger's machine face, Table A), A1's
// event-word registration, and the layer family aggregation.
//
// symmetry_semantics: label — the plate-family members are L1/PBD vocabulary (the C11/C12
// corpus configs).
//
// Provenance: the C12/C11 anchors (1.010/1.525 tint, the 108.937 deg spot) are pinned by the
// contour docking test through the same producer path; this file pins the ENUMERATION around
// them, not the physical values again.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "analytic/dp_contour.hpp"
#include "analytic/dp_focus.hpp"
#include "analytic/reflection_group.hpp"
#include "core/random.hpp"
#include "raypath/detail/measure/declared_density.hpp"
#include "raypath/detail/schema3/structure_enumeration.hpp"

namespace lumice::raypath::schema3 {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;

struct Tables {
  analytic::FaceNormalTable normals;
  analytic::FacePolygonTable polys;
};

// test_fiber_quadrature.cpp's RhombicPlate (the C12 crystal).
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

// test_dp_capi.cpp's Beta (the C05/C06 crystal).
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

Distribution NoRandom(double v) {
  return { DistributionType::kNoRandom, static_cast<float>(v), 0.0f };
}
Distribution Uniform(double center, double range) {
  return { DistributionType::kUniform, static_cast<float>(center), static_cast<float>(range) };
}

UMarginal PlateMeasure(double altitude_deg) {
  const double alt = altitude_deg * kDeg;
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

// The C12 enumeration input: the plate measure under the 9-degree sun.
EnumerationInput C12Input(const Tables& t) {
  EnumerationInput in;
  in.normals = &t.normals;
  in.polygons = &t.polys;
  in.base_index = 1.307;
  static const UMarginal measure = PlateMeasure(9.0);  // outlives the test process
  static const analytic::PoseDensitySpec spec = PlateSpec();
  in.measure = &measure;
  in.density = &spec;
  in.sun_dir[0] = std::cos(9.0 * kDeg);
  in.sun_dir[1] = 0.0;
  in.sun_dir[2] = std::sin(9.0 * kDeg);
  in.grid = 256;
  return in;
}

const StructureObjectRecord* FindKind(const std::vector<StructureObjectRecord>& objects, ObjectKind kind,
                                      int slot = -1) {
  for (const StructureObjectRecord& object : objects) {
    if (object.kind == kind && (slot < 0 || object.slot == slot)) {
      return &object;
    }
  }
  return nullptr;
}

// ---- the C09 unlit shape, through the restricted leg -----------------------------------------

TEST(StructureEnumeration, C09UnlitShapeOnAFullyDarkPinnedMember) {
  // The corpus's C09 (cone crystal 13-15-26-28) needs a fixture the tree does not carry —
  // registered for 666.4. The SHAPE is pinned here on a constructible member: family-pinned
  // (the restricted curve exists — the "contour present" leg), and fully dark on the declared
  // orbit ("no passage"): unlit with saw flags, and the candidate bucket (geometry in the light,
  // the light not in the geometry).
  const Tables t = RhombicPlate();
  const EnumerationInput in = C12Input(t);
  const MemberEnumeration one = EnumerateMember(in, { 2, 4, 5, 1 });
  const StructureObjectRecord* restricted = FindKind(one.objects, ObjectKind::kKind1Restricted);
  ASSERT_NE(restricted, nullptr) << "2-4-5-1 must be family-pinned on the plate (probe-pinned)";
  EXPECT_EQ(restricted->existence, ExistenceState::kComputed);
  EXPECT_FALSE(restricted->u.empty());
  EXPECT_EQ(restricted->visibility.state, VisibilityState::kUnlit);
  EXPECT_EQ(std::string(restricted->visibility.reason), "");  // unlit names no blocking condition
  EXPECT_EQ(BucketOf(*restricted), FeatureBucket::kCandidate);
  // The corroboration carry-invariant on a produced record.
  EXPECT_EQ(restricted->corroboration, CorroborationState::kUnchecked);
  EXPECT_FALSE(restricted->counterfactual.available);
}

// ---- the C11 family collapse: identity fields --------------------------------------------------

TEST(StructureEnumeration, C11FamilyCollapseIdentityFields) {
  // The corpus's C11 anchor (the 108.937-degree SPHERICAL spot deflection vs the 120-degree
  // azimuth difference) is pinned value-for-value in the contour docking test (two doors, one
  // value). The enumeration-level identity fields pinned here: the family-pinned restricted
  // object exists, and its u preimage is the family circle at the SUN'S latitude — the u-space
  // structure the fold identity acts on, not any azimuth-arithmetic circle.
  const Tables t = C11Plate();
  EnumerationInput in;
  in.normals = &t.normals;
  in.polygons = &t.polys;
  in.base_index = 1.3110129;
  static const UMarginal measure = PlateMeasure(20.0);
  static const analytic::PoseDensitySpec spec = PlateSpec();
  in.measure = &measure;
  in.density = &spec;
  in.sun_dir[0] = std::cos(20.0 * kDeg);
  in.sun_dir[1] = 0.0;
  in.sun_dir[2] = std::sin(20.0 * kDeg);
  in.grid = 720;
  const MemberEnumeration one = EnumerateMember(in, { 1, 4, 5, 2 });
  const StructureObjectRecord* restricted = FindKind(one.objects, ObjectKind::kKind1Restricted);
  ASSERT_NE(restricted, nullptr);
  ASSERT_GE(restricted->u.size(), 3u);
  for (size_t i = 2; i < restricted->u.size(); i += 3) {
    if (std::fabs(std::asin(restricted->u[i]) / kDeg - 20.0) > 1e-6) {
      ADD_FAILURE() << "u circle latitude off at point " << i / 3 << ": " << std::asin(restricted->u[i]) / kDeg;
    }
  }
}

// ---- the beta crystal's S2 chain objects -------------------------------------------------------

TEST(StructureEnumeration, BetaEnumerationCarriesS2ChainsAndKind1) {
  const Tables t = Beta();
  EnumerationInput in;
  in.normals = &t.normals;
  in.polygons = &t.polys;
  in.base_index = 1.3110129;
  const MemberEnumeration one = EnumerateMember(in, { 4, 8, 1, 7, 5 });
  // kind-2: the gate boundary walked closed.
  const StructureObjectRecord* gate = FindKind(one.objects, ObjectKind::kKind2);
  ASSERT_NE(gate, nullptr);
  EXPECT_EQ(gate->existence, ExistenceState::kComputed);
  EXPECT_FALSE(gate->u.empty());
  // kind-3: one object per weight kink (three internal steps on this chain), slots named.
  std::vector<int> slots_seen;
  for (const StructureObjectRecord& object : one.objects) {
    if (object.kind == ObjectKind::kKind3) {
      slots_seen.push_back(object.slot);
    }
  }
  ASSERT_EQ(slots_seen.size(), 3u);
  // s6_junction: opportunistic corners, u preimage only, no sky position claimed.
  const StructureObjectRecord* junction = FindKind(one.objects, ObjectKind::kS6Junction);
  ASSERT_NE(junction, nullptr);
  EXPECT_FALSE(junction->u.empty());
  EXPECT_FALSE(junction->has_sky_position);
  // walk_s spellings: NaN on everything a walk arclength does not apply to (the junction, the
  // closed gate curve) — the declared convention, pinned so no implicit 0.0 comes back.
  EXPECT_TRUE(std::isnan(junction->walk_s));
  EXPECT_TRUE(std::isnan(gate->walk_s));
  // kind-1: the critical-value structure, chromatic assessed with the declared snapshot.
  const StructureObjectRecord* kind1 = FindKind(one.objects, ObjectKind::kKind1);
  ASSERT_NE(kind1, nullptr);
  EXPECT_EQ(kind1->existence, ExistenceState::kComputed);
  EXPECT_TRUE(kind1->chromatic_assessed);
  EXPECT_DOUBLE_EQ(kind1->chromatic_thresholds.edge_min_shift_rad, 0.5 * kDeg);
  // No measure supplied: the visibility field ships its fail-closed default and the object
  // stays a candidate.
  EXPECT_EQ(kind1->visibility.state, VisibilityState::kUnproven);
  EXPECT_EQ(BucketOf(*kind1), FeatureBucket::kCandidate);
  // The support row rode the same assembly.
  EXPECT_EQ(one.support.axis.context.coverage, PartitionContext::Coverage::kComplete);
  EXPECT_EQ(one.support.constant_curves.size(), 1u);  // the basal kink's constant circle
}

// ---- the vocabulary coverage table (Table A's machine face) -------------------------------------

TEST(StructureEnumeration, CoverageTablePartitionsTheRegistry) {
  const Tables t = Beta();
  EnumerationInput in;
  in.normals = &t.normals;
  in.polygons = &t.polys;
  in.base_index = 1.3110129;
  const Schema3DiscoveryCore core = EnumerateLayer(in, { { 4, 8, 1, 7, 5 } });
  // Every registered kind appears exactly once across produced + declared.
  std::multiset<int> seen;
  for (const ObjectKind kind : core.coverage.produced) {
    seen.insert(static_cast<int>(kind));
  }
  for (const ObjectKind kind : core.coverage.declared_not_produced) {
    seen.insert(static_cast<int>(kind));
  }
  for (const ObjectKind kind : RegisteredObjectKinds()) {
    EXPECT_EQ(seen.count(static_cast<int>(kind)), 1u) << "kind not covered exactly once";
  }
  ASSERT_EQ(core.coverage.declared_reasons.size(), core.coverage.declared_not_produced.size());
  for (const std::string& reason : core.coverage.declared_reasons) {
    EXPECT_FALSE(reason.empty());
  }
  // The declared kinds never appear as objects; the produced ones do (this member produces all
  // five).
  for (const StructureObjectRecord& object : core.objects) {
    EXPECT_EQ(
        std::find(core.coverage.declared_not_produced.begin(), core.coverage.declared_not_produced.end(), object.kind),
        core.coverage.declared_not_produced.end())
        << "a declared-not-produced kind materialized an object";
  }
  EXPECT_NE(FindKind(core.objects, ObjectKind::kKind1), nullptr);
  EXPECT_NE(FindKind(core.objects, ObjectKind::kKind2), nullptr);
  EXPECT_NE(FindKind(core.objects, ObjectKind::kKind3), nullptr);
  EXPECT_NE(FindKind(core.objects, ObjectKind::kS6Junction), nullptr);
}

// ---- A1's event-word registration ---------------------------------------------------------------

TEST(StructureEnumeration, A1EventWordRegistration) {
  EXPECT_EQ(static_cast<ChainEventKind>(EventWordForObjectKind(ObjectKind::kKind3)), ChainEventKind::kTirBoundary);
  EXPECT_EQ(static_cast<ChainEventKind>(EventWordForObjectKind(ObjectKind::kKind2)), ChainEventKind::kPathInfeasible);
  EXPECT_EQ(static_cast<ChainEventKind>(EventWordForObjectKind(ObjectKind::kCorridorClosed)),
            ChainEventKind::kCorridorClosed);
  EXPECT_EQ(EventWordForObjectKind(ObjectKind::kKind1), -1);
  EXPECT_EQ(EventWordForObjectKind(ObjectKind::kKind1Restricted), -1);
  EXPECT_EQ(EventWordForObjectKind(static_cast<ObjectKind>(99)), -1);  // unknown: no word, no crash
}

// ---- the layer family aggregation -----------------------------------------------------------------

TEST(StructureEnumeration, LayerFamilyAggregationClustersIdenticalSupports) {
  const Tables t = Beta();
  EnumerationInput in;
  in.normals = &t.normals;
  in.polygons = &t.polys;
  in.base_index = 1.3110129;
  const Schema3DiscoveryCore core = EnumerateLayer(in, { { 4, 8, 1, 7, 5 }, { 4, 8, 2, 7, 5 }, { 4, 8, 7, 5 } });
  // Two families: the C06 pair shares its [120, 180] support; C05 stands alone.
  ASSERT_EQ(core.support.families.size(), 2u);
  size_t c06 = 0;
  size_t c05 = 0;
  for (const FamilySupport& family : core.support.families) {
    if (!family.shared) {
      ADD_FAILURE() << "an aggregated family must be shared";
      continue;
    }
    if (family.members.size() != 1u && family.members.size() != 2u) {
      ADD_FAILURE() << "unexpected cluster size: " << family.members.size();
      continue;
    }
    if (family.members.size() == 2) {
      c06++;
      EXPECT_NEAR(family.intervals.front().lower / kDeg, 120.0, 1e-6);
    } else if (family.members.size() == 1) {
      c05++;
      EXPECT_NEAR(family.intervals.back().upper / kDeg, 120.0, 1e-6);
    }
  }
  EXPECT_EQ(c06, 1u);
  EXPECT_EQ(c05, 1u);
  EXPECT_EQ(core.support.members.size(), 3u);
  // The Phi-class gate the clustering runs on: every family carries a note, and the C06 pair's
  // note is the kernel orbit's canonical member (read through the kernel's own authority — the
  // single ruler the gate compares).
  std::string c06_note;
  for (const FamilySupport& family : core.support.families) {
    if (family.phi_class_note.empty()) {
      ADD_FAILURE() << "a family without its Phi-class note";
      continue;
    }
    if (family.members.size() == 2u) {
      c06_note = family.phi_class_note;
    }
  }
  const std::vector<int> c06_member = { 4, 8, 1, 7, 5 };
  std::vector<std::vector<int>> orbit;
  ASSERT_TRUE(analytic::PbdOrbit(c06_member.data(), static_cast<int>(c06_member.size()), &orbit));
  ASSERT_FALSE(orbit.empty());
  std::string canonical;
  for (size_t i = 0; i < orbit[0].size(); i++) {
    if (i > 0) {
      canonical += "-";
    }
    canonical += std::to_string(orbit[0][i]);
  }
  EXPECT_EQ(c06_note, canonical);
  // The budget: the v1 default is unconstrained, and the sampling count is positive only when
  // the measure side ran (it did not here).
  EXPECT_EQ(core.budget.max_optical_evaluations, 0);
  EXPECT_EQ(core.budget.sampling_evaluations, 0);
}

// ---- the Phi-class note on objects and the build-site budget --------------------------------------

TEST(StructureEnumeration, PhiClassNoteCarriedAndBudgetCountedWhereBuilt) {
  const Tables t = RhombicPlate();
  const EnumerationInput in = C12Input(t);
  const std::vector<int> member = { 2, 4, 5, 1 };
  const Schema3DiscoveryCore core = EnumerateLayer(in, { member });
  // Every object carries its member's Phi-class note, and the note is the orbit's canonical
  // member per the kernel's own authority (the same ruler the clustering gate runs on).
  std::vector<std::vector<int>> orbit;
  ASSERT_TRUE(analytic::PbdOrbit(member.data(), static_cast<int>(member.size()), &orbit));
  ASSERT_FALSE(orbit.empty());
  std::string canonical;
  for (size_t i = 0; i < orbit[0].size(); i++) {
    if (i > 0) {
      canonical += "-";
    }
    canonical += std::to_string(orbit[0][i]);
  }
  ASSERT_FALSE(core.objects.empty());
  for (const StructureObjectRecord& object : core.objects) {
    EXPECT_EQ(object.phi_class_note, canonical);
  }
  // The budget counts the points where they were BUILT: one orbit stream for this member, the
  // actual sample count — not a formula's estimate (the formula under-counted the moment a
  // second consumer leg existed).
  int slots[analytic::kMaxFaceCount];
  ASSERT_EQ(analytic::ResolveFaceSequence(t.normals, member.data(), static_cast<int>(member.size()), slots),
            analytic::Status::kOk);
  const analytic::OrbitFiberStream expected = analytic::MakeOrbitFiberStream(
      t.normals, t.polys, slots, static_cast<int>(member.size()), *in.density, in.sun_dir, in.base_index, in.grid);
  EXPECT_EQ(core.budget.sampling_evaluations, static_cast<long long>(expected.samples.size()));
  EXPECT_GT(core.budget.sampling_evaluations, 0);
}

}  // namespace
}  // namespace lumice::raypath::schema3
