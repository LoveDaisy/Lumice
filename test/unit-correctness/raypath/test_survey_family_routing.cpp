// The measure-to-family routing (gap1-family-routing.md, the promoted design note) at the
// survey assembly level: the measure extraction and the v1 domain gate as pure functions, the
// freezing ladder's shape per level, the survival filter's two sides, and the corpus
// calibrations the design note names — the C16 pair (the roll freeze survives as the Lowitz
// fold, the wide zenith freeze must die), the Parry circle's one-dimensional image at level 2,
// and the 0D single-pose arm. The C10-shape smoke uses the pinned mirror 1-2-1 (the
// restricted-family rank test's own anchor: the pole spin chart collapses it to the subsun at
// -sun altitude) — the assembly-level statement the ladder must reproduce.
//
// symmetry_semantics: none — every family is read off the measure, never a label.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <vector>

#include "raypath/detail/survey/survey_assembly.hpp"
#include "raypath/detail/survey/survey_family.hpp"
#include "raypath/detail/survey/survey_json.hpp"
#include "raypath/detail/survey/survey_tint.hpp"
#include "survey_test_fixtures.hpp"

namespace lumice::raypath::survey {
namespace {

SurveyRequest LiteralRequest(std::vector<int> faces) {
  SurveyRequest request;
  request.crystal_id = 1;
  request.path_layers = { std::move(faces) };
  request.symmetry_bits = 0;  // the literal member: the deterministic, per-member shape
  return request;
}

AxisDistribution AxisOf(Distribution latitude, Distribution azimuth, Distribution roll) {
  AxisDistribution axis;
  axis.latitude_dist = latitude;
  axis.azimuth_dist = azimuth;
  axis.roll_dist = roll;
  return axis;
}

const Distribution kFixed90{ DistributionType::kNoRandom, 90.0f, 0 };
const Distribution kFixed0{ DistributionType::kNoRandom, 0.0f, 0 };
const Distribution kUniformFull{ DistributionType::kUniform, 0.0f, 360.0f };

// ---- the measure extraction and the v1 domain gate (pure) ------------------------------------------

TEST(FamilyRouting, DefaultAxisIsTheSinglePoseAndReadsFixedEverywhere) {
  const FamilyMeasureReading reading = ReadFamilyMeasure(AxisDistribution{});
  EXPECT_TRUE(reading.ok);
  EXPECT_TRUE(reading.single_pose);
  EXPECT_EQ(reading.measure.zenith.kind, FamilyDimKind::kFixed);
  EXPECT_NEAR(reading.measure.zenith.center_deg, 0.0, 1e-12);  // latitude 90 -> zenith 0
  EXPECT_EQ(reading.measure.azimuth.kind, FamilyDimKind::kFixed);
  EXPECT_EQ(reading.measure.roll.kind, FamilyDimKind::kFixed);
}

// The loader's own default semantics, read through the loader itself (the router reads them;
// it never re-derives them): an axis block with only a zenith key seeds the azimuth and roll
// as the free uniform.
TEST(FamilyRouting, LoaderSeedsMissingAzimuthAndRollAsFreeUniform) {
  // Only the zenith key is written: the loader seeds the other two slots as the free uniform
  // (its own default semantics — the router reads them, it never re-derives them). The zenith
  // stays a gauss so the axis is one the routing v1 domain carries (an all-uniform axis is the
  // Haar one, which routes to the full-dimensional arm before this gate ever runs).
  const auto parsed = R"({
    "zenith": { "type": "gauss", "mean": 0.0, "std": 1.0 }
  })"_json;
  AxisDistribution axis;
  ASSERT_NO_THROW(from_json(parsed, axis));
  const FamilyMeasureReading reading = ReadFamilyMeasure(axis);
  EXPECT_TRUE(reading.ok);
  EXPECT_FALSE(reading.single_pose);
  EXPECT_EQ(reading.measure.zenith.kind, FamilyDimKind::kGauss);
  EXPECT_NEAR(reading.measure.zenith.center_deg, 0.0, 1e-12);
  EXPECT_EQ(reading.measure.azimuth.kind, FamilyDimKind::kUniformFull);
  EXPECT_EQ(reading.measure.roll.kind, FamilyDimKind::kUniformFull);
}

TEST(FamilyRouting, MeasureEchoCarriesCenterAndWidthInTheConcentrationLanguage) {
  // A Lowitz reading: latitude 90 +- 40 is zenith 0 +- 40; the roll gauss keeps its sigma.
  const FamilyMeasureReading reading = ReadFamilyMeasure(
      AxisOf({ DistributionType::kGaussian, 90.0f, 40.0f }, kUniformFull, { DistributionType::kGaussian, 0.0f, 0.3f }));
  ASSERT_TRUE(reading.ok);
  EXPECT_FALSE(reading.single_pose);
  EXPECT_EQ(reading.measure.zenith.kind, FamilyDimKind::kGauss);
  EXPECT_NEAR(reading.measure.zenith.center_deg, 0.0, 1e-12);
  EXPECT_NEAR(reading.measure.zenith.width_deg, 40.0, 1e-12);
  EXPECT_EQ(reading.measure.azimuth.kind, FamilyDimKind::kUniformFull);
  EXPECT_NEAR(reading.measure.azimuth.width_deg, 360.0, 1e-12);
  EXPECT_EQ(reading.measure.roll.kind, FamilyDimKind::kGauss);
  EXPECT_NEAR(reading.measure.roll.width_deg, 0.3, 1e-6);
}

// The zenith conversion's two poles: latitude 90 is zenith 0 (the plate), latitude 0 is
// zenith 90 (the column) — the density conversion's own formula, read at both ends.
TEST(FamilyRouting, ZenithConversionFlipsTheLatitudeSlot) {
  const FamilyMeasureReading plate =
      ReadFamilyMeasure(AxisOf({ DistributionType::kGaussian, 90.0f, 1.0f }, kUniformFull, kUniformFull));
  ASSERT_TRUE(plate.ok);
  EXPECT_NEAR(plate.measure.zenith.center_deg, 0.0, 1e-12);
  const FamilyMeasureReading column =
      ReadFamilyMeasure(AxisOf({ DistributionType::kGaussian, 0.0f, 1.0f }, kUniformFull, kUniformFull));
  ASSERT_TRUE(column.ok);
  EXPECT_NEAR(column.measure.zenith.center_deg, 90.0, 1e-12);
}

// The v1 domain gate, every named refusal on the AC3 list. The mode is the open string the
// document carries: the prefix plus the named shape, first offender in canonical order.
TEST(FamilyRouting, DomainGateNamesEachOutsideShape) {
  const auto mode_of = [](const AxisDistribution& axis) {
    const FamilyMeasureReading reading = ReadFamilyMeasure(axis);
    EXPECT_FALSE(reading.ok);
    return reading.refusal_mode;
  };
  const Distribution gauss{ DistributionType::kGaussian, 0.0f, 5.0f };
  const Distribution bounded{ DistributionType::kUniform, 10.0f, 30.0f };
  const Distribution zigzag{ DistributionType::kZigzag, 0.0f, 1.0f };
  const Distribution laplacian{ DistributionType::kLaplacian, 0.0f, 1.0f };
  const Distribution legacy{ DistributionType::kGaussianLegacy, 0.0f, 5.0f };

  EXPECT_EQ(mode_of(AxisOf(kFixed90, gauss, kUniformFull)), "measure_shape_outside_routing_domain: azimuth_gaussian");
  EXPECT_EQ(mode_of(AxisOf(kFixed90, bounded, kUniformFull)),
            "measure_shape_outside_routing_domain: azimuth_bounded_uniform");
  EXPECT_EQ(mode_of(AxisOf(kFixed90, kFixed0, kUniformFull)), "measure_shape_outside_routing_domain: azimuth_fixed");
  EXPECT_EQ(mode_of(AxisOf(bounded, kUniformFull, kUniformFull)),
            "measure_shape_outside_routing_domain: zenith_bounded_uniform");
  // A full-uniform zenith off the Haar axis (the roll is a gauss, so the axis is not
  // full-sphere uniform — and a Haar axis never reaches this gate, it routes full-dimensional).
  EXPECT_EQ(mode_of(AxisOf(kUniformFull, kUniformFull, gauss)),
            "measure_shape_outside_routing_domain: zenith_full_uniform");
  EXPECT_EQ(mode_of(AxisOf(kFixed90, kUniformFull, bounded)),
            "measure_shape_outside_routing_domain: roll_bounded_uniform");
  EXPECT_EQ(mode_of(AxisOf(zigzag, kUniformFull, kUniformFull)), "measure_shape_outside_routing_domain: zenith_zigzag");
  EXPECT_EQ(mode_of(AxisOf(kFixed90, kUniformFull, laplacian)), "measure_shape_outside_routing_domain: roll_laplacian");
  EXPECT_EQ(mode_of(AxisOf(kFixed90, legacy, kUniformFull)),
            "measure_shape_outside_routing_domain: azimuth_gaussian_legacy");
}

// The in-domain shapes that are NOT the single pose: the fixed zenith/roll of a pinned
// orientation (Parry-like) and the uniform roll of a spin family both pass.
TEST(FamilyRouting, DomainGateAdmitsFixedZenithAndRollAndUniformRoll) {
  const FamilyMeasureReading pinned = ReadFamilyMeasure(AxisOf(kFixed0, kUniformFull, kFixed0));
  EXPECT_TRUE(pinned.ok);
  EXPECT_FALSE(pinned.single_pose);
  const FamilyMeasureReading spin = ReadFamilyMeasure(AxisOf(kFixed90, kUniformFull, kUniformFull));
  EXPECT_TRUE(spin.ok);
  EXPECT_FALSE(spin.single_pose);
}

// The freeze candidates: gauss zenith/roll only, tightest first, zenith before roll on a tie
// (canonical, deterministic); uniforms and fixed dimensions never enter.
TEST(FamilyRouting, FreezeCandidatesSortTightestFirst) {
  const FamilyMeasureReading reading = ReadFamilyMeasure(
      AxisOf({ DistributionType::kGaussian, 90.0f, 40.0f }, kUniformFull, { DistributionType::kGaussian, 0.0f, 0.3f }));
  ASSERT_TRUE(reading.ok);
  const std::vector<FamilyFreezeCandidate> candidates = FreezeCandidates(reading.measure);
  ASSERT_EQ(candidates.size(), 2u);
  EXPECT_EQ(candidates[0].dim, analytic::FamilyDimension::kRoll);
  EXPECT_NEAR(candidates[0].width_deg, 0.3, 1e-6);
  EXPECT_EQ(candidates[1].dim, analytic::FamilyDimension::kZenith);
  EXPECT_NEAR(candidates[1].width_deg, 40.0, 1e-12);

  const FamilyMeasureReading tie = ReadFamilyMeasure(
      AxisOf({ DistributionType::kGaussian, 90.0f, 5.0f }, kUniformFull, { DistributionType::kGaussian, 0.0f, 5.0f }));
  const std::vector<FamilyFreezeCandidate> tie_candidates = FreezeCandidates(tie.measure);
  ASSERT_EQ(tie_candidates.size(), 2u);
  EXPECT_EQ(tie_candidates[0].dim, analytic::FamilyDimension::kZenith);
  EXPECT_EQ(tie_candidates[1].dim, analytic::FamilyDimension::kRoll);

  const FamilyMeasureReading none = ReadFamilyMeasure(AxisOf(kFixed90, kUniformFull, kUniformFull));
  EXPECT_TRUE(FreezeCandidates(none.measure).empty());
}

// ---- the assembly: the corpus calibrations ----------------------------------------------------------

// The C10 measure shape (a plate with a 0.3-degree zenith wobble) and the pinned mirror chain
// 1-2-1: level 0 is the support alone (three free dimensions — the rank entry's refusal
// domain, so the level owns the support and nothing else), level 1 freezes zenith and the
// {azimuth, roll} pole spin chart collapses the mirror to the sun's reflection (the
// restricted-family rank test's own anchor, -sun altitude), which survives its 0.3-degree
// blur against the spot floor.
TEST(FamilyRouting, C10ShapeFreezesZenithIntoASpinCollapsePoint) {
  const ConfigManager config = PlateScene(0.3, 0.3);
  SurveyDocument document;
  ASSERT_TRUE(AssembleSurvey(config, LiteralRequest({ 1, 2, 1 }), &document).Ok());
  EXPECT_FALSE(document.early_stopped);
  EXPECT_TRUE(document.unevaluated.empty());
  ASSERT_TRUE(document.family_routing.has_value());
  const FamilyRoutingBlock& block = *document.family_routing;

  // The measure echo: zenith gauss 0 +- 0.3, azimuth and roll the free uniform.
  EXPECT_EQ(block.measure.zenith.kind, FamilyDimKind::kGauss);
  EXPECT_NEAR(block.measure.zenith.width_deg, 0.3, 1e-6);
  EXPECT_EQ(block.measure.azimuth.kind, FamilyDimKind::kUniformFull);
  EXPECT_EQ(block.measure.roll.kind, FamilyDimKind::kUniformFull);

  // Level 0 owns the support and asks no rank (three free dimensions); level 1 freezes zenith.
  ASSERT_EQ(block.ladder.size(), 2u);
  EXPECT_EQ(block.ladder[0].level, 0);
  EXPECT_FALSE(block.ladder[0].rank_asked);
  EXPECT_TRUE(block.ladder[0].frozen_dims.empty());
  ASSERT_EQ(block.ladder[0].free_dims.size(), 3u);
  EXPECT_EQ(block.ladder[1].level, 1);
  ASSERT_TRUE(block.ladder[1].rank_asked);
  ASSERT_EQ(block.ladder[1].frozen_dims.size(), 1u);
  EXPECT_EQ(block.ladder[1].frozen_dims[0].dim, analytic::FamilyDimension::kZenith);
  EXPECT_NEAR(block.ladder[1].frozen_dims[0].width_deg, 0.3, 1e-6);
  EXPECT_EQ(block.ladder[1].generic_rank, 0);
  EXPECT_EQ(block.ladder[1].grid, kFamilyLadderGrid);

  // The cards: the level-0 support_set AND the level-1 collapse point (the reachability does
  // not collapse with the concentration — both are reported).
  const SurveyObject* support = nullptr;
  const SurveyObject* collapse = nullptr;
  for (const SurveyObject& card : document.objects) {
    if (card.kind == SurveyObjectKind::kSupportSet) {
      support = &card;
    }
    if (card.kind == SurveyObjectKind::kFamilyCollapsePoint) {
      collapse = &card;
    }
  }
  ASSERT_NE(support, nullptr);
  ASSERT_NE(collapse, nullptr);
  EXPECT_TRUE(support->support_set.has_value());
  ASSERT_TRUE(collapse->band.has_value());
  EXPECT_EQ(collapse->band->level, 1);
  EXPECT_NEAR(collapse->band->blur_deg, 0.6, 0.35);                // ~2x the frozen half-width (the mirror)
  EXPECT_LT(collapse->band->blur_deg, collapse->band->scale_deg);  // the survival verdict
  EXPECT_DOUBLE_EQ(collapse->band->scale_deg, kSurvivalSpotFloorDeg);
  ASSERT_EQ(collapse->band->frozen.size(), 1u);
  EXPECT_EQ(collapse->band->frozen[0].dim, "zenith");
  EXPECT_EQ(collapse->band->frozen[0].verdict, "narrow");
  // The anchor: the sun's reflection about the horizontal plane, -20 degrees altitude.
  EXPECT_NEAR(collapse->position_altitude_deg, -20.0, 0.05);
  EXPECT_NEAR(std::fabs(collapse->position_azimuth_deg), 0.0, 0.05);
  // The density carried the measure (a plate density), so the probe ran.
  EXPECT_EQ(document.probes.size(), 1u);
  EXPECT_EQ(document.evidence.state, EvidenceState::kFinite);
}

// The C16 Lowitz calibration (the design note's own row): roll narrow 0.3, zenith wide 40.
// Level 1 freezes roll and the {azimuth, zenith} family carries the Lowitz fold — the roll is
// NARROW against the fold's own extent, so the arc survives with its band. Level 2 freezes
// the wide zenith too, and the {azimuth} circle's image blurs far past its own scale: the
// candidate is killed and reported in the ladder level that proposed it (never a card), with
// the wide verdict and the clamped one-sided probe on record.
TEST(FamilyRouting, C16LowitzFoldSurvivesTheRollAndDiesUnderTheWideZenith) {
  const ConfigManager config = LowitzMeasureScene(40.0, 0.3);
  SurveyDocument document;
  ASSERT_TRUE(AssembleSurvey(config, LiteralRequest({ 3, 5 }), &document).Ok());
  ASSERT_TRUE(document.family_routing.has_value());
  const FamilyRoutingBlock& block = *document.family_routing;

  ASSERT_EQ(block.ladder.size(), 3u);
  EXPECT_FALSE(block.ladder[0].rank_asked);  // three free dimensions: the support alone
  ASSERT_TRUE(block.ladder[1].rank_asked);
  EXPECT_EQ(block.ladder[1].frozen_dims.size(), 1u);
  EXPECT_EQ(block.ladder[1].frozen_dims[0].dim, analytic::FamilyDimension::kRoll);
  EXPECT_EQ(block.ladder[1].generic_rank, 2);  // the regular image with the fold locus
  ASSERT_TRUE(block.ladder[2].rank_asked);
  ASSERT_EQ(block.ladder[2].frozen_dims.size(), 2u);
  EXPECT_EQ(block.ladder[2].frozen_dims[0].dim, analytic::FamilyDimension::kRoll);
  EXPECT_EQ(block.ladder[2].frozen_dims[1].dim, analytic::FamilyDimension::kZenith);
  EXPECT_EQ(block.ladder[2].generic_rank, 1);  // the one-dimensional circle

  // Level 1's survivors: the fold arcs (the family-form caustic cards), the roll narrow.
  const SurveyObject* fold = nullptr;
  const SurveyObject* domain = nullptr;
  for (const SurveyObject& card : document.objects) {
    if (card.kind == SurveyObjectKind::kCausticEdge && card.band.has_value() && card.band->level == 1) {
      fold = &card;
    }
    if (card.kind == SurveyObjectKind::kBrightDomain && card.band.has_value() && card.band->level == 1) {
      domain = &card;
    }
  }
  ASSERT_NE(fold, nullptr) << "the Lowitz fold must survive the roll freeze";
  ASSERT_TRUE(fold->band.has_value());
  ASSERT_EQ(fold->band->frozen.size(), 1u);
  EXPECT_EQ(fold->band->frozen[0].dim, "roll");
  EXPECT_EQ(fold->band->frozen[0].verdict, "narrow");
  EXPECT_LT(fold->band->blur_deg, fold->band->scale_deg);
  EXPECT_DOUBLE_EQ(fold->band->scale_deg, kSurvivalSpotFloorDeg);  // the cross-section reading
  ASSERT_TRUE(fold->family_origin.has_value());
  EXPECT_GT(fold->family_origin->crossing_count, 0);
  ASSERT_NE(domain, nullptr) << "the two-dimensional family's bright domain carries the band";
  EXPECT_GT(domain->band->scale_deg, kSurvivalSpotFloorDeg);  // the domain keeps the extent reading

  // Level 2 under the scale ruling (the design authority's decision on decision point 1: a
  // curve-like object's scale is its structural cross-section floor, never its overall extent
  // — the arc's length tolerating a transverse blur was the length-vs-thickness mismatch the
  // ruling closed): the wide zenith's blur 10.42 deg kills against the 1.0-deg floor, the
  // record lands in the ladder level that proposed it, and no level-2 card is produced.
  ASSERT_FALSE(block.ladder[2].killed.empty());
  const FamilyKilledCandidate* killed = nullptr;
  for (const FamilyKilledCandidate& candidate : block.ladder[2].killed) {
    if (candidate.reason == "survival_filter") {
      killed = &candidate;
    }
  }
  ASSERT_NE(killed, nullptr);
  EXPECT_EQ(killed->kind, "regular_image");
  for (const SurveyObject& card : document.objects) {
    EXPECT_FALSE(card.band.has_value() && card.band->level == 2);
  }
  // The support carries the density tail row (the width went into the ladder; the support's
  // own tail bound is a row).
  const SurveyObject* support = nullptr;
  for (const SurveyObject& card : document.objects) {
    if (card.kind == SurveyObjectKind::kSupportSet) {
      support = &card;
    }
  }
  ASSERT_NE(support, nullptr);
  ASSERT_TRUE(support->support_set.has_value());
  ASSERT_EQ(support->support_set->density_tails.size(), 1u);
  EXPECT_EQ(support->support_set->density_tails[0].dim, "zenith");
  EXPECT_NEAR(support->support_set->density_tails[0].lower_deg, 0.0, 1e-9);
  EXPECT_GT(support->support_set->density_tails[0].upper_deg, 100.0);  // the 12-sigma window
}

// The calibration ruler the design note spells for this shape, restored by the scale ruling
// (the former DISABLED_... ruler the pre-ruling estimator could not satisfy): the curve-like
// object's scale is the structural cross-section floor, and the wide zenith's blur 10.42 deg
// kills with room — the design row's "far past its own scale" made operational. The roll on
// the same record stays narrow: the two frozen dimensions of one object read differently
// (the narrow half of AC5's dimension-times-object property).
TEST(FamilyRouting, C16CalibrationKillUnderTheScaleEstimatorRuling) {
  const ConfigManager config = LowitzMeasureScene(40.0, 0.3);
  SurveyDocument document;
  ASSERT_TRUE(AssembleSurvey(config, LiteralRequest({ 3, 5 }), &document).Ok());
  const FamilyRoutingBlock& block = *document.family_routing;
  ASSERT_FALSE(block.ladder[2].killed.empty());
  const FamilyKilledCandidate* killed = nullptr;
  for (const FamilyKilledCandidate& candidate : block.ladder[2].killed) {
    if (candidate.reason == "survival_filter") {
      killed = &candidate;
    }
  }
  ASSERT_NE(killed, nullptr);
  EXPECT_EQ(killed->kind, "regular_image");
  EXPECT_NEAR(killed->blur_deg, 10.42, 0.30);
  EXPECT_DOUBLE_EQ(killed->scale_deg, kSurvivalSpotFloorDeg);
  EXPECT_GT(killed->blur_deg, 5.0 * kSurvivalSpotFloorDeg);  // the kill has room, not margin
  EXPECT_TRUE(killed->clamped);                              // the zenith center 0 minus 20 clamps one-sided
  bool zenith_wide = false;
  bool roll_narrow = false;
  for (const FamilyBandVerdict& verdict : killed->verdicts) {
    if (verdict.dim == "zenith") {
      zenith_wide = verdict.verdict == "wide";
    }
    if (verdict.dim == "roll") {
      roll_narrow = verdict.verdict == "narrow";
    }
  }
  EXPECT_TRUE(zenith_wide);
  EXPECT_TRUE(roll_narrow);
  for (const SurveyObject& card : document.objects) {
    EXPECT_FALSE(card.band.has_value() && card.band->level == 2);
  }
}

// The Parry measure (zenith 90 +- 1, roll 0 +- 0.3): level 1 freezes roll and level 2 both,
// leaving the {azimuth} Parry circle — a one-dimensional regular image that survives both its
// narrow freezes with the band at level 2.
TEST(FamilyRouting, ParryShapeCarriesTheOneDimensionalImageAtLevelTwo) {
  const ConfigManager config = ParryMeasureScene(1.0, 0.3);
  SurveyDocument document;
  ASSERT_TRUE(AssembleSurvey(config, LiteralRequest({ 3, 5 }), &document).Ok());
  ASSERT_TRUE(document.family_routing.has_value());
  ASSERT_EQ(document.family_routing->ladder.size(), 3u);

  const SurveyObject* regular = nullptr;
  for (const SurveyObject& card : document.objects) {
    if (card.kind == SurveyObjectKind::kRegularImage && card.band.has_value() && card.band->level == 2) {
      regular = &card;
    }
  }
  ASSERT_NE(regular, nullptr) << "the Parry circle is a surviving regular image at level 2";
  // The first sampled image of the Parry measure sits on the upper Parry arc, above the
  // horizon (sun at +20). The regression this pins: a second negation on the way into the
  // sky read (ImageAltAzDeg negates its outgoing input) flips the altitude to the antipode
  // (-38.6). The value is this scene's first-anchor reading, not a universal constant.
  EXPECT_NEAR(regular->position_altitude_deg, 38.596, 0.5);
  EXPECT_LT(regular->band->blur_deg, regular->band->scale_deg);
  for (const FamilyBandVerdict& verdict : regular->band->frozen) {
    EXPECT_EQ(verdict.verdict, "narrow") << verdict.dim;
  }
}

// The filter's kill side, the C10 shape's own mirror: the same pinned mirror chain on a plate
// with a WIDE zenith wobble (5 degrees) freezes the same level-1 spin family into the same
// collapse point, but the frozen width's blur (2 x the half-width, the mirror doubling) dwarfs
// the spot floor — the candidate is killed with its numbers in the ladder and no card is
// produced (the reachability stays on the support card; the object claim dies).
TEST(FamilyRouting, WideWobbleKillsTheCollapseCandidateItProposed) {
  const ConfigManager config = PlateScene(1.0, 5.0);
  SurveyDocument document;
  ASSERT_TRUE(AssembleSurvey(config, LiteralRequest({ 1, 2, 1 }), &document).Ok());
  ASSERT_TRUE(document.family_routing.has_value());
  const FamilyRoutingBlock& block = *document.family_routing;
  ASSERT_EQ(block.ladder.size(), 2u);
  const FamilyLadderLevel& level = block.ladder[1];
  EXPECT_EQ(level.objects_proposed, 1);
  ASSERT_EQ(level.killed.size(), 1u);
  const FamilyKilledCandidate& killed = level.killed.front();
  EXPECT_EQ(killed.kind, "family_collapse_point");
  EXPECT_EQ(killed.reason, "survival_filter");
  EXPECT_NEAR(killed.blur_deg, 5.0, 0.5);  // the mirror doubles the 2.5-degree half-width
  EXPECT_DOUBLE_EQ(killed.scale_deg, kSurvivalSpotFloorDeg);
  EXPECT_TRUE(killed.clamped);
  ASSERT_EQ(killed.verdicts.size(), 1u);
  EXPECT_EQ(killed.verdicts[0].dim, "zenith");
  EXPECT_EQ(killed.verdicts[0].verdict, "wide");
  for (const SurveyObject& card : document.objects) {
    EXPECT_NE(card.kind, SurveyObjectKind::kFamilyCollapsePoint);
  }
}

// The 0D arm: the all-fixed axis (the axis-default's own distribution) evaluates the one pose
// through the production chart — a collapse point per member with the band at level 0, the
// single-pose weight read directly (never a band sum), and the positional chromatic legs
// honestly unevaluated.
TEST(FamilyRouting, SinglePoseAxisComputesTheZeroDimensionalImageDirectly) {
  const ConfigManager config = SinglePoseScene();
  SurveyDocument document;
  // The pinned mirror 1-2-1: valid at the canonical pose (the 3-5 wedge is edge-on to the sun
  // there — its no_valid_image kill is the honest answer, not a failure of the arm).
  ASSERT_TRUE(AssembleSurvey(config, LiteralRequest({ 1, 2, 1 }), &document).Ok());
  EXPECT_FALSE(document.early_stopped);
  ASSERT_TRUE(document.family_routing.has_value());
  const FamilyRoutingBlock& block = *document.family_routing;
  ASSERT_EQ(block.ladder.size(), 1u);
  EXPECT_EQ(block.ladder[0].level, 0);
  EXPECT_FALSE(block.ladder[0].rank_asked);  // a rank over a point is not a question
  ASSERT_EQ(block.ladder[0].frozen_dims.size(), 3u);
  EXPECT_TRUE(block.ladder[0].free_dims.empty());

  const SurveyObject* collapse = nullptr;
  const SurveyObject* support = nullptr;
  for (const SurveyObject& card : document.objects) {
    if (card.kind == SurveyObjectKind::kFamilyCollapsePoint) {
      collapse = &card;
    }
    if (card.kind == SurveyObjectKind::kSupportSet) {
      support = &card;
    }
  }
  ASSERT_NE(support, nullptr);
  ASSERT_NE(collapse, nullptr);
  EXPECT_TRUE(collapse->has_position);
  // The horizontal basal mirror at the canonical pose: the sun's reflection, altitude -20.
  EXPECT_NEAR(collapse->position_altitude_deg, -20.0, 1e-6);
  EXPECT_NEAR(std::fabs(collapse->position_azimuth_deg), 0.0, 1e-6);
  ASSERT_TRUE(collapse->band.has_value());
  EXPECT_EQ(collapse->band->level, 0);
  EXPECT_DOUBLE_EQ(collapse->band->blur_deg, 0.0);
  // The direct weight on the same chain: the diagnostic batch's corridor is EMPTY for the
  // doubled-back basal chain (1-2-1's internal segment returns through the entry face — a
  // kernel-side corridor fact at every near-plate pose, not a pose accident), so the tint
  // refuses with its named mode rather than defaulting a number.
  ASSERT_TRUE(document.weight_tint.has_value());
  EXPECT_EQ(document.weight_tint->evidence.state, EvidenceState::kRefused);
  EXPECT_EQ(document.weight_tint->evidence.mode, "single_pose_weight_unavailable");
  EXPECT_EQ(document.chromatic.state, EvidenceState::kRefused);
  EXPECT_TRUE(document.probes.empty());
  ASSERT_TRUE(document.positional_dispersion.has_value());
  EXPECT_EQ(document.positional_dispersion->evidence.state, EvidenceState::kUnevaluated);

  // A chain whose corridor carries weight at the same pose (1-3: basal entry, side exit): the
  // single-pose read computes — the method slug named, both masses positive, never a band sum.
  SurveyDocument wedge_document;
  ASSERT_TRUE(AssembleSurvey(config, LiteralRequest({ 1, 3 }), &wedge_document).Ok());
  ASSERT_TRUE(wedge_document.family_routing.has_value());
  const SurveyObject* wedge_collapse = nullptr;
  for (const SurveyObject& card : wedge_document.objects) {
    if (card.kind == SurveyObjectKind::kFamilyCollapsePoint) {
      wedge_collapse = &card;
    }
  }
  ASSERT_NE(wedge_collapse, nullptr);
  EXPECT_TRUE(wedge_collapse->has_position);
  ASSERT_TRUE(wedge_document.weight_tint.has_value());
  EXPECT_EQ(wedge_document.weight_tint->evidence.state, EvidenceState::kFinite);
  EXPECT_EQ(wedge_document.weight_tint->method, std::string(kTintMethodSinglePoseDirect));
  EXPECT_GT(wedge_document.weight_tint->m_red, 0.0);
  EXPECT_GT(wedge_document.weight_tint->m_blue, 0.0);
  EXPECT_GT(wedge_document.weight_tint->ratio, 0.0);
  EXPECT_EQ(wedge_document.chromatic.state, EvidenceState::kFinite);

  // The edge-on 3-5 chain kills with no numbers at all: a no_valid_image kill carries no
  // blur/scale/clamped "measurement" — the group is absent from the serialized record, not
  // zeroed (the 0D arm sets its scale floor before the pose evaluates; it must not leak).
  SurveyDocument edge_document;
  ASSERT_TRUE(AssembleSurvey(config, LiteralRequest({ 3, 5 }), &edge_document).Ok());
  ASSERT_TRUE(edge_document.family_routing.has_value());
  ASSERT_EQ(edge_document.family_routing->ladder.size(), 1u);
  ASSERT_EQ(edge_document.family_routing->ladder[0].killed.size(), 1u);
  EXPECT_EQ(edge_document.family_routing->ladder[0].killed.front().reason, "no_valid_image");
  EXPECT_FALSE(edge_document.family_routing->ladder[0].killed.front().blur_measured);
  const auto edge_parsed = nlohmann::json::parse(SurveyDocumentToJson(edge_document, "test"));
  const auto& edge_killed = edge_parsed.at("family_routing").at("ladder").at(0).at("killed").at(0);
  EXPECT_FALSE(edge_killed.contains("blur_deg"));
  EXPECT_FALSE(edge_killed.contains("scale_deg"));
  EXPECT_FALSE(edge_killed.contains("clamped"));
}

// The fixed-pose plate (zenith and roll pinned, azimuth free): the fixed dimensions leave one
// free dimension at level 0, so the rank question is asked AT level 0 — and the density
// conversion cannot carry the axis, so the quantification legs stay honestly unevaluated
// while the ladder runs (the structure domain is wider than the density domain).
TEST(FamilyRouting, FixedZenithAndRollAskTheRankAtLevelZeroWithoutTheDensityLegs) {
  const ConfigManager config = FixedPosePlateScene(1.5);
  SurveyDocument document;
  ASSERT_TRUE(AssembleSurvey(config, LiteralRequest({ 3, 5 }), &document).Ok());
  EXPECT_FALSE(document.early_stopped);
  ASSERT_TRUE(document.family_routing.has_value());
  const FamilyRoutingBlock& block = *document.family_routing;
  ASSERT_EQ(block.ladder.size(), 1u);  // no gauss candidates: level 0 only
  EXPECT_TRUE(block.ladder[0].rank_asked);
  ASSERT_EQ(block.ladder[0].frozen_dims.size(), 2u);
  ASSERT_EQ(block.ladder[0].free_dims.size(), 1u);
  EXPECT_EQ(block.ladder[0].free_dims[0], analytic::FamilyDimension::kAzimuth);

  bool saw_regular = false;
  for (const SurveyObject& card : document.objects) {
    if (card.kind == SurveyObjectKind::kRegularImage) {
      saw_regular = card.band.has_value() && card.band->level == 0;
    }
  }
  EXPECT_TRUE(saw_regular) << "the plate's azimuth circle is a one-dimensional image";
  EXPECT_TRUE(document.probes.empty());
  EXPECT_EQ(document.chromatic.state, EvidenceState::kUnevaluated);
  ASSERT_TRUE(document.weight_tint.has_value());
  EXPECT_EQ(document.weight_tint->evidence.state, EvidenceState::kUnevaluated);
  EXPECT_NE(document.weight_tint->evidence.reason.find("no pose density"), std::string::npos);
}

// The outside-domain refusal: one refused support card with the named mode and the witness's
// positive fact, no family_routing key, no approximation into the domain.
TEST(FamilyRouting, OutsideDomainShapeRefusesWithTheNamedModeAndAWitness) {
  const ConfigManager config = ZigzagRollScene();
  SurveyDocument document;
  ASSERT_TRUE(AssembleSurvey(config, LiteralRequest({ 3, 5 }), &document).Ok());
  EXPECT_FALSE(document.early_stopped);
  EXPECT_FALSE(document.family_routing.has_value());
  ASSERT_EQ(document.objects.size(), 1u);
  const SurveyObject& card = document.objects.front();
  EXPECT_EQ(card.kind, SurveyObjectKind::kSupportSet);
  EXPECT_EQ(card.evidence.state, EvidenceState::kRefused);
  EXPECT_EQ(card.evidence.mode, "measure_shape_outside_routing_domain: roll_zigzag");
  ASSERT_TRUE(card.witness.has_value());
  EXPECT_EQ(card.witness->state, EvidenceState::kFinite);
  EXPECT_GT(card.witness->kept_count, 0);
  EXPECT_EQ(document.evidence.state, EvidenceState::kRefused);
  EXPECT_EQ(document.evidence.mode, "measure_shape_outside_routing_domain: roll_zigzag");
  EXPECT_TRUE(document.probes.empty());
  const auto parsed = nlohmann::json::parse(SurveyDocumentToJson(document, "test"));
  EXPECT_FALSE(parsed.contains("family_routing"));
  EXPECT_FALSE(parsed.contains("probes"));
  EXPECT_EQ(parsed.at("objects")[0].at("evidence").at("mode"), "measure_shape_outside_routing_domain: roll_zigzag");
}

// The serialized family document's shape: the measure echo, the ladder with its rank coverage
// and killed records, the band on the cards, and the conditional keys.
TEST(FamilyRouting, FamilyDocumentSerializesTheLadderAndBands) {
  const ConfigManager config = LowitzMeasureScene(40.0, 0.3);
  SurveyDocument document;
  ASSERT_TRUE(AssembleSurvey(config, LiteralRequest({ 3, 5 }), &document).Ok());
  const auto parsed = nlohmann::json::parse(SurveyDocumentToJson(document, "test"));
  ASSERT_TRUE(parsed.contains("family_routing"));
  const auto& family = parsed.at("family_routing");
  EXPECT_EQ(family.at("measure").at("zenith").at("kind"), "gauss");
  EXPECT_DOUBLE_EQ(family.at("measure").at("zenith").at("width_deg"), 40.0);
  EXPECT_EQ(family.at("measure").at("roll").at("kind"), "gauss");
  EXPECT_EQ(family.at("measure").at("azimuth").at("kind"), "uniform");
  ASSERT_EQ(family.at("ladder").size(), 3u);
  const auto& level0 = family.at("ladder").at(0);
  EXPECT_FALSE(level0.contains("rank"));  // three free dimensions: no rank question asked
  const auto& level1 = family.at("ladder").at(1);
  ASSERT_TRUE(level1.contains("rank"));
  EXPECT_EQ(level1.at("rank").at("generic_rank"), 2);
  EXPECT_EQ(level1.at("rank").at("grid"), kFamilyLadderGrid);
  ASSERT_EQ(level1.at("frozen_dims").size(), 1u);
  EXPECT_EQ(level1.at("frozen_dims").at(0).at("dim"), "roll");
  EXPECT_GT(level1.at("objects_proposed"), 0);
  const auto& level2 = family.at("ladder").at(2);
  // The wide zenith's level-2 kill serializes with its numbers in the ladder level that
  // proposed it (the hand-built fixture in test_survey_json_v4.cpp pins the full shape).
  ASSERT_TRUE(level2.contains("killed"));
  ASSERT_EQ(level2.at("killed").size(), 1u);
  EXPECT_EQ(level2.at("killed").at(0).at("kind"), "regular_image");
  EXPECT_NEAR(level2.at("killed").at(0).at("blur_deg").get<double>(), 10.42, 0.30);
  EXPECT_DOUBLE_EQ(level2.at("killed").at(0).at("scale_deg").get<double>(), kSurvivalSpotFloorDeg);
  EXPECT_EQ(level2.at("objects_proposed"), 1);
  EXPECT_EQ(family.at("evidence").at("state"), "finite");
  // The band lives on the surviving cards; the killed candidate's numbers live in the ladder.
  for (const auto& card : parsed.at("objects")) {
    if (card.contains("band")) {
      EXPECT_EQ(card.at("band").at("survival"), "pass");
      EXPECT_TRUE(card.at("band").contains("frozen"));
    }
  }
}

// The ladder arm's own no_valid_image kill (distinct from the 0D arm's same-named path): a
// rank-1 family whose image the rank scan sees (generic_rank 1) but whose 16-point extent
// probe set keeps fewer than two valid anchors — the candidate carries its kind (assigned
// before the anchors) and no position, and the kill records no measurement group. Scene: the
// Parry measure's level-2 {azimuth} circle on the 2-5 chain (basal entry, side exit) — the
// probe run in the task's progress record confirmed the anchors die there before this test
// pinned the serialized shape.
TEST(FamilyRouting, LadderKillsTheUnprobeableImageWithNoMeasurementGroup) {
  const ConfigManager config = ParryMeasureScene(1.0, 0.3);
  SurveyDocument document;
  ASSERT_TRUE(AssembleSurvey(config, LiteralRequest({ 2, 5 }), &document).Ok());
  ASSERT_TRUE(document.family_routing.has_value());
  ASSERT_EQ(document.family_routing->ladder.size(), 3u);
  const FamilyLadderLevel& level = document.family_routing->ladder[2];
  EXPECT_EQ(level.generic_rank, 1);
  ASSERT_FALSE(level.killed.empty());
  const FamilyKilledCandidate* killed = nullptr;
  for (const FamilyKilledCandidate& candidate : level.killed) {
    if (candidate.reason == "no_valid_image") {
      killed = &candidate;
    }
  }
  ASSERT_NE(killed, nullptr);
  EXPECT_EQ(killed->kind, "regular_image");
  EXPECT_FALSE(killed->has_position);
  EXPECT_FALSE(killed->blur_measured);
  EXPECT_TRUE(killed->verdicts.empty());
  // Serialized: kind and reason present, the position and the measurement group absent —
  // the same conditional-presence discipline the 0D edge-on kill pins.
  const auto parsed = nlohmann::json::parse(SurveyDocumentToJson(document, "test"));
  const auto& level2 = parsed.at("family_routing").at("ladder").at(2);
  ASSERT_TRUE(level2.contains("killed"));
  const nlohmann::json* record = nullptr;
  for (const auto& candidate : level2.at("killed")) {
    if (candidate.at("reason") == "no_valid_image") {
      record = &candidate;
    }
  }
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(record->at("kind"), "regular_image");
  EXPECT_FALSE(record->contains("position"));
  EXPECT_FALSE(record->contains("blur_deg"));
  EXPECT_FALSE(record->contains("scale_deg"));
  EXPECT_FALSE(record->contains("clamped"));
}

}  // namespace
}  // namespace lumice::raypath::survey
