// The local-class resolution (src/raypath/detail/local_class.hpp) against the three acceptance
// scenarios restated as pure algebra: the Parry −52° pair partitioning under {identity, H_0},
// the column tangent arc's twelve members collapsing to one orbit named 3-5 (stable along the
// arc), and the C12 plate's singleton classes on both sun sides — plus the sky-action semantics
// (the mirror twins merge exactly on the sun's vertical plane) and the row-scope clamp (a
// partial-bit row keeps its own members).
//
// symmetry_semantics: kPhysical — the row scope is the physical regime's member list; identity
// and naming are theorems here, so every expectation below is an exact set, no sampling anywhere.

#include <gtest/gtest.h>

#include <vector>

#include "core/crystal.hpp"
#include "core/crystal_param.hpp"
#include "raypath/detail/local_class.hpp"
#include "raypath/detail/physical_member_scope.hpp"

namespace lumice::raypath {
namespace {

// A prism crystal row scope: `face_distances` spells the shape (the C12 plate's rhombic
// distances derive their own two-fold/mirror symmetry through the production path).
PhysicalMemberScope MakeRow(const AxisDistribution& axis, const float* face_distances,
                            const std::vector<int>& representative, uint8_t symmetry_bits) {
  PhysicalMemberRequest request;
  request.scene_identity = "local-class-test";
  request.layer_index = 0;
  PrismCrystalParam prism;
  prism.h_ = { DistributionType::kNoRandom, 1.0f, 0 };
  for (int i = 0; i < 6; ++i) {
    prism.d_[i] = { DistributionType::kNoRandom, face_distances[i], 0 };
  }
  CrystalConfig crystal;
  crystal.id_ = 1;
  crystal.param_ = prism;
  crystal.axis_ = axis;
  request.crystal = crystal;
  request.representative = representative;
  request.symmetry_bits = symmetry_bits;
  request.semantics = SymmetrySemantics::kPhysical;
  PhysicalMemberScope scope;
  if (!ResolvePhysicalMemberScope(request, &scope).Ok()) {
    ADD_FAILURE() << "row scope resolution failed";
  }
  return scope;
}

std::vector<std::vector<int>> MemberSequences(const std::vector<LocalClass>& classes) {
  std::vector<std::vector<int>> out;
  for (const LocalClass& klass : classes) {
    out.push_back(klass.representative);
  }
  return out;
}

TEST(LocalClass, ParryMinus52SplitsTheFullOrbitIntoTheTwoHalfTurnClasses) {
  // The Parry axis: zenith gauss 90±1 (latitude 0±1), azimuth uniform, roll gauss 0±1.
  AxisDistribution axis;
  axis.azimuth_dist = Distribution{ DistributionType::kUniform, 0.0f, 360.0f };
  axis.latitude_dist = Distribution{ DistributionType::kGaussian, 0.0f, 1.0f };
  axis.roll_dist = Distribution{ DistributionType::kGaussian, 0.0f, 1.0f };
  // The row's member list is the PHYSICAL class the analysis list carries: the expansion runs
  // under the physical gating (the locked roll keeps no c-axis rotation), so four members.
  const SymmetryGating gating = DeriveSymmetryGating(SymmetrySemantics::kPhysical, kFullHexagonalSymmetry, axis);
  const auto d = detail::DeriveDSymmetryParams(axis);
  const auto members = ExpandRaypathByPeriod({ 4, 1 }, sym::kSymP | sym::kSymB | sym::kSymD, d.sigma_a, d.d_applicable,
                                             gating.p_retained, gating.b_applicable, kHexagonalFnPeriod, gating.geom);
  ASSERT_EQ(members.size(), 4u);
  const float regular[6] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
  PhysicalMemberScope row = MakeRow(axis, regular, { 4, 1 }, sym::kSymP | sym::kSymB | sym::kSymD);
  row.members.clear();
  for (const auto& member : members) {
    row.members.emplace_back(member.begin(), member.end());
  }
  LocalClassRequest request;
  request.target_altitude_deg = 36.0;
  request.target_azimuth_deg = -52.0;
  request.sun_azimuth_deg = 0.0;
  std::vector<LocalClass> classes;
  ASSERT_TRUE(ResolveLocalClasses(row, request, &classes).Ok());
  // The locked roll kills the c-axis rotations; the improper elements leave every ±52° class.
  // The retained sky-fixing set is {identity, H_0}, whose orbits are exactly the two pairs.
  ASSERT_EQ(classes.size(), 2u);
  EXPECT_EQ(classes[0].representative, (std::vector<int>{ 4, 1 }));
  EXPECT_EQ(classes[1].representative, (std::vector<int>{ 4, 2 }));
  ASSERT_EQ(classes[0].member_indices.size(), 2u);
  ASSERT_EQ(classes[1].member_indices.size(), 2u);
  // The transport facts: the representative carries itself by the identity, its sibling by H_0
  // (looked up by sequence — member_indices is sorted, so positions are not the expansion order).
  const CrystalElement identity{};
  const CrystalElement h0{ CrystalElement::Family::kHorizontalHalfTurn, 0 };
  for (size_t i = 0; i < classes[1].member_indices.size(); ++i) {
    const auto& member = row.members[classes[1].member_indices[i]];
    if (member == std::vector<int>({ 4, 2 })) {
      EXPECT_EQ(classes[1].to_representative[i], identity);
    } else if (member == std::vector<int>({ 8, 1 })) {
      EXPECT_EQ(classes[1].to_representative[i], h0);
    }
  }
  // The +52° side partitions identically (proper elements fix every sky direction); which class
  // carries the light there is a discovery fact, not this layer's.
  request.target_azimuth_deg = 52.0;
  std::vector<LocalClass> plus_classes;
  ASSERT_TRUE(ResolveLocalClasses(row, request, &plus_classes).Ok());
  EXPECT_EQ(MemberSequences(plus_classes), MemberSequences(classes));
}

TEST(LocalClass, ColumnArcTwelveMembersAreOneClassNamedThreeFiveStably) {
  // The C16 column axis: zenith gauss 90±0.3, azimuth uniform, roll uniform 360.
  AxisDistribution axis;
  axis.azimuth_dist = Distribution{ DistributionType::kUniform, 0.0f, 360.0f };
  axis.latitude_dist = Distribution{ DistributionType::kGaussian, 0.0f, 0.3f };
  axis.roll_dist = Distribution{ DistributionType::kUniform, 0.0f, 360.0f };
  const SymmetryGating gating = DeriveSymmetryGating(SymmetrySemantics::kPhysical, kFullHexagonalSymmetry, axis);
  const auto d = detail::DeriveDSymmetryParams(axis);
  const auto members = ExpandRaypathByPeriod({ 3, 5 }, sym::kSymP | sym::kSymB | sym::kSymD, d.sigma_a, d.d_applicable,
                                             gating.p_retained, gating.b_applicable, kHexagonalFnPeriod, gating.geom);
  ASSERT_EQ(members.size(), 12u);
  const float regular[6] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
  PhysicalMemberScope row = MakeRow(axis, regular, { 3, 5 }, sym::kSymP | sym::kSymB | sym::kSymD);
  row.members.clear();
  for (const auto& member : members) {
    row.members.emplace_back(member.begin(), member.end());
  }
  // Two points along the lower tangent arc: one class, one representative, both times.
  for (const double azimuth : { -30.0, -26.0 }) {
    LocalClassRequest request;
    request.target_altitude_deg = 20.0;
    request.target_azimuth_deg = azimuth;
    request.sun_azimuth_deg = 0.0;
    std::vector<LocalClass> classes;
    if (!ResolveLocalClasses(row, request, &classes).Ok()) {
      ADD_FAILURE() << "resolution failed at azimuth " << azimuth;
      continue;
    }
    if (classes.size() != 1u) {
      ADD_FAILURE() << "expected one class at azimuth " << azimuth << ", got " << classes.size();
      continue;
    }
    EXPECT_EQ(classes[0].representative, (std::vector<int>{ 3, 5 }));
    EXPECT_EQ(classes[0].member_indices.size(), 12u);
    // The transport of 3-7 to 3-5 is the composite half-turn — the element no bit names.
    for (size_t i = 0; i < classes[0].member_indices.size(); ++i) {
      const auto& member = row.members[classes[0].member_indices[i]];
      if (member == std::vector<int>({ 3, 7 })) {
        const CrystalElement h0{ CrystalElement::Family::kHorizontalHalfTurn, 0 };
        EXPECT_EQ(classes[0].to_representative[i], h0);
      }
    }
  }
}

TEST(LocalClass, PlateSidesAreSingletonsOnBothSidesAndMergeOnlyOnTheSunPlane) {
  // The C12 plate: zenith gauss 0±1 (latitude 90±1), azimuth uniform, roll gauss 0±5; the
  // rhombic face distances keep a two-fold rotation and the mirrors at 0 and 3.
  AxisDistribution axis;
  axis.azimuth_dist = Distribution{ DistributionType::kUniform, 0.0f, 360.0f };
  axis.latitude_dist = Distribution{ DistributionType::kGaussian, 90.0f, 1.0f };
  axis.roll_dist = Distribution{ DistributionType::kGaussian, 0.0f, 5.0f };
  const float rhombic[6] = { 1.5f, 1.0f, 1.0f, 1.5f, 1.0f, 1.0f };
  // The PRODUCTION scope, unmodified: the row's own bit selection (B|D — the canonical AC3
  // input keeps P out of the row) resolves its own members through the axis's real physical
  // gating, which answers b_applicable FALSE here (the pole-centred zenith is not symmetric
  // about 90°): the row is the σ_0 mirror pair, two members. The test never edits them. NOTE
  // on AC3's wording: with the retained inventory now closed under composition, a FULL-bit row
  // on this axis would carry the P_3 half-turn (a proper element, sky-fixing everywhere) and
  // its ±120° classes would be two-member orbits, not singletons — see
  // PlateAxisFullBitRowOffPlaneCarriesTheHalfTurnOrbits. The singleton statement below is the
  // no-P row's answer, the acceptance scenario's own input; the full-bit restatement rides the
  // owner dispute.
  PhysicalMemberScope row = MakeRow(axis, rhombic, { 1, 3, 4, 2 }, sym::kSymB | sym::kSymD);
  ASSERT_EQ(row.members.size(), 2u);
  // The scene sun sits at azimuth 180; the ±120 sides are off the sun's vertical plane, so the
  // mirror drops out of G_q and every class is a singleton — the algebraic statement behind
  // "each side is carried by one member".
  LocalClassRequest request;
  request.target_altitude_deg = 36.0;
  request.target_azimuth_deg = 120.0;
  request.sun_azimuth_deg = 180.0;
  std::vector<LocalClass> classes;
  ASSERT_TRUE(ResolveLocalClasses(row, request, &classes).Ok());
  ASSERT_EQ(classes.size(), 2u);
  EXPECT_EQ(classes[0].representative, (std::vector<int>{ 1, 3, 4, 2 }));
  EXPECT_EQ(classes[1].representative, (std::vector<int>{ 1, 3, 8, 2 }));
  request.target_azimuth_deg = -120.0;
  std::vector<LocalClass> minus_classes;
  ASSERT_TRUE(ResolveLocalClasses(row, request, &minus_classes).Ok());
  EXPECT_EQ(MemberSequences(minus_classes), MemberSequences(classes));
  // On the sun's plane the mirror fixes the sky: the mirror twins merge into one class of two.
  request.target_azimuth_deg = 180.0;
  std::vector<LocalClass> on_plane;
  ASSERT_TRUE(ResolveLocalClasses(row, request, &on_plane).Ok());
  ASSERT_EQ(on_plane.size(), 1u);
  EXPECT_EQ(on_plane[0].representative, (std::vector<int>{ 1, 3, 4, 2 }));
  ASSERT_EQ(on_plane[0].member_indices.size(), 2u);  // 1-3-4-2 with its mirror twin 1-3-8-2
  // A literal row (symmetry bits 0) is its own single class.
  PhysicalMemberScope literal = MakeRow(axis, rhombic, { 1, 3, 4, 2 }, 0);
  std::vector<LocalClass> literal_classes;
  ASSERT_TRUE(ResolveLocalClasses(literal, request, &literal_classes).Ok());
  ASSERT_EQ(literal_classes.size(), 1u);
  EXPECT_EQ(literal_classes[0].representative, (std::vector<int>{ 1, 3, 4, 2 }));
}

TEST(LocalClass, PlateAxisFullBitRowOffPlaneCarriesTheHalfTurnOrbits) {
  // The corrected algebra's full-bit answer on the SAME axis and shape, unmodified production
  // scope: the pole-centred latitude glues P_3 (proper — it fixes every sky direction, the sun
  // plane is irrelevant), so off the sun's plane the row partitions into TWO two-member
  // classes, the P_3 orbits {1-3-4-2, 1-6-7-2} and {1-3-8-2, 1-6-5-2} — not the singletons
  // AC3's wording was derived against. The acceptance conflict rides the owner dispute; this
  // test pins what the complete preservation group actually answers.
  AxisDistribution axis;
  axis.azimuth_dist = Distribution{ DistributionType::kUniform, 0.0f, 360.0f };
  axis.latitude_dist = Distribution{ DistributionType::kGaussian, 90.0f, 1.0f };
  axis.roll_dist = Distribution{ DistributionType::kGaussian, 0.0f, 5.0f };
  const float rhombic[6] = { 1.5f, 1.0f, 1.0f, 1.5f, 1.0f, 1.0f };
  PhysicalMemberScope row = MakeRow(axis, rhombic, { 1, 3, 4, 2 }, sym::kSymP | sym::kSymB | sym::kSymD);
  ASSERT_EQ(row.members.size(), 4u);
  LocalClassRequest request;
  request.target_altitude_deg = 36.0;
  request.target_azimuth_deg = 120.0;
  request.sun_azimuth_deg = 180.0;
  std::vector<LocalClass> classes;
  ASSERT_TRUE(ResolveLocalClasses(row, request, &classes).Ok());
  ASSERT_EQ(classes.size(), 2u);
  EXPECT_EQ(classes[0].representative, (std::vector<int>{ 1, 3, 4, 2 }));
  ASSERT_EQ(classes[0].member_indices.size(), 2u);
  EXPECT_EQ(classes[1].representative, (std::vector<int>{ 1, 3, 8, 2 }));
  ASSERT_EQ(classes[1].member_indices.size(), 2u);
  // The −120° side answers identically (proper elements fix it the same way).
  request.target_azimuth_deg = -120.0;
  std::vector<LocalClass> minus_classes;
  ASSERT_TRUE(ResolveLocalClasses(row, request, &minus_classes).Ok());
  EXPECT_EQ(MemberSequences(minus_classes), MemberSequences(classes));
}

TEST(LocalClass, PlateAxisOnPlaneFormsOneClassAndCarriesEveryMemberByALabelVerifiedElement) {
  // The production scope, unmodified: the plate axis (zenith pole-centred, roll gauss 0±5)
  // under full P|B|D bits — the physical gating keeps {identity, P_3, σ_0} plus the composition
  // closure's σ_3 = σ_0·P_3, whose canonical reflection centre names nothing. On the sun's
  // vertical plane all four retained elements fix the sky point, so the row's four members are
  // ONE orbit-class named by the authority — and every member's carrier must map it onto the
  // representative label-for-label (the transport facts' contract, the one a silent identity
  // default used to break when the closure missed σ_3).
  AxisDistribution axis;
  axis.azimuth_dist = Distribution{ DistributionType::kUniform, 0.0f, 360.0f };
  axis.latitude_dist = Distribution{ DistributionType::kGaussian, 90.0f, 1.0f };
  axis.roll_dist = Distribution{ DistributionType::kGaussian, 0.0f, 5.0f };
  const float rhombic[6] = { 1.5f, 1.0f, 1.0f, 1.5f, 1.0f, 1.0f };
  PhysicalMemberScope row = MakeRow(axis, rhombic, { 1, 3, 4, 2 }, sym::kSymP | sym::kSymB | sym::kSymD);
  ASSERT_EQ(row.members.size(), 4u) << "the production scope: P_3 shifts and the σ_0 mirrors";
  LocalClassRequest request;
  request.target_altitude_deg = 36.0;
  request.target_azimuth_deg = 180.0;  // the sun's vertical plane: every retained element fixes q
  request.sun_azimuth_deg = 180.0;
  std::vector<LocalClass> classes;
  ASSERT_TRUE(ResolveLocalClasses(row, request, &classes).Ok());
  ASSERT_EQ(classes.size(), 1u) << "the closed retained set makes the row one orbit on this q";
  EXPECT_EQ(classes[0].representative, (std::vector<int>{ 1, 3, 4, 2 }));
  ASSERT_EQ(classes[0].member_indices.size(), 4u);
  for (size_t i = 0; i < classes[0].member_indices.size(); ++i) {
    const auto& member = row.members[classes[0].member_indices[i]];
    std::vector<int> image = member;
    for (int& face : image) {
      face = static_cast<int>(classes[0].to_representative[i].ApplyToFace(static_cast<IdType>(face)));
    }
    EXPECT_EQ(image, classes[0].representative)
        << "member " << member[0] << "-" << member[1] << "-" << member[2] << "-" << member[3];
  }
}

}  // namespace
}  // namespace lumice::raypath
