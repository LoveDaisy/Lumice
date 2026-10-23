// The orientation-ensemble halves of the P and B raypath symmetries (detail::IsPApplicable /
// detail::IsBApplicable), checked against the truth table for the five axis families the GUI
// offers as presets, and at the edges of each rule.
//
// Truth table (P = invariant under a 60° roll shift; B = zenith symmetric about 90° AND azimuth
// invariant under a 180° shift):
//   random  P yes  B yes
//   plate   P yes  B no    — face 1 always faces up
//   column  P yes  B yes
//   parry   P no   B yes   — roll locked
//   lowitz  P no   B no    — roll locked, c-axis near vertical
//
// The families are spelled as wire JSON and parsed by core's own from_json, so the zenith -> latitude
// conversion the predicates depend on is part of what is checked. The numbers mirror
// src/gui/axis_presets.hpp::kAxisPresets (not included: test code must not reach into src/gui/);
// if that table changes, revisit this one.

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "core/crystal.hpp"
#include "core/math.hpp"

namespace lumice {
namespace {

AxisDistribution ParseAxis(const char* text) {
  AxisDistribution axis;
  from_json(nlohmann::json::parse(text), axis);
  return axis;
}

constexpr const char* kRandom =
    R"({"zenith": {"type": "uniform", "mean": 90, "std": 360},
        "azimuth": {"type": "uniform", "mean": 0, "std": 360},
        "roll": {"type": "uniform", "mean": 0, "std": 360}})";
constexpr const char* kPlate =
    R"({"zenith": {"type": "gauss", "mean": 0, "std": 1},
        "azimuth": {"type": "uniform", "mean": 0, "std": 360},
        "roll": {"type": "uniform", "mean": 0, "std": 360}})";
constexpr const char* kColumn =
    R"({"zenith": {"type": "gauss", "mean": 90, "std": 1},
        "azimuth": {"type": "uniform", "mean": 0, "std": 360},
        "roll": {"type": "uniform", "mean": 0, "std": 360}})";
constexpr const char* kParry =
    R"({"zenith": {"type": "gauss", "mean": 90, "std": 1},
        "azimuth": {"type": "uniform", "mean": 0, "std": 360},
        "roll": {"type": "gauss", "mean": 0, "std": 1}})";
constexpr const char* kLowitz =
    R"({"zenith": {"type": "gauss", "mean": 0, "std": 40},
        "azimuth": {"type": "uniform", "mean": 0, "std": 360},
        "roll": {"type": "gauss", "mean": 0, "std": 1}})";

TEST(RaypathEnsembleApplicability, PresetFamiliesMatchTheTruthTable) {
  struct Row {
    const char* name;
    const char* axis;
    bool p;
    bool b;
  };
  const Row rows[] = {
    { "random", kRandom, true, true }, { "plate", kPlate, true, false },    { "column", kColumn, true, true },
    { "parry", kParry, false, true },  { "lowitz", kLowitz, false, false },
  };
  for (const auto& row : rows) {
    const AxisDistribution axis = ParseAxis(row.axis);
    EXPECT_EQ(detail::IsPApplicable(axis), row.p) << row.name;
    EXPECT_EQ(detail::IsBApplicable(axis), row.b) << row.name;
  }
}

TEST(RaypathEnsembleApplicability, PReadsRollOnly) {
  EXPECT_TRUE(detail::IsPApplicableParams(DistributionType::kUniform, 360.0f));
  EXPECT_FALSE(detail::IsPApplicableParams(DistributionType::kUniform, 300.0f));
  EXPECT_FALSE(detail::IsPApplicableParams(DistributionType::kUniform, 60.0f));
  EXPECT_FALSE(detail::IsPApplicableParams(DistributionType::kGaussian, 360.0f));
  EXPECT_FALSE(detail::IsPApplicableParams(DistributionType::kNoRandom, 0.0f));
  EXPECT_FALSE(detail::IsPApplicableParams(DistributionType::kZigzag, 360.0f));

  // Azimuth does not enter: a column with a Gaussian azimuth still has P.
  AxisDistribution axis = ParseAxis(kColumn);
  axis.azimuth_dist = Distribution{ DistributionType::kGaussian, 0.0f, 30.0f };
  EXPECT_TRUE(detail::IsPApplicable(axis));
}

TEST(RaypathEnsembleApplicability, BNeedsZenithSymmetricAbout90) {
  const auto az = DistributionType::kUniform;
  // Latitude (= 90 - zenith) centred on 0, for every type symmetric about its own centre.
  for (auto t : { DistributionType::kNoRandom, DistributionType::kUniform, DistributionType::kGaussian,
                  DistributionType::kGaussianLegacy, DistributionType::kLaplacian }) {
    EXPECT_TRUE(detail::IsBApplicableParams(az, 360.0f, t, 0.0f, 20.0f)) << static_cast<int>(t);
    EXPECT_FALSE(detail::IsBApplicableParams(az, 360.0f, t, 1e-3f, 20.0f)) << static_cast<int>(t);
    EXPECT_FALSE(detail::IsBApplicableParams(az, 360.0f, t, 90.0f, 20.0f)) << static_cast<int>(t);
  }
  // A full-turn uniform latitude is symmetric about every point, wherever it is centred.
  EXPECT_TRUE(detail::IsBApplicableParams(az, 360.0f, DistributionType::kUniform, 37.0f, 360.0f));
  // kZigzag folds |A sin + B|: not decidable from its centre, answered conservatively.
  EXPECT_FALSE(detail::IsBApplicableParams(az, 360.0f, DistributionType::kZigzag, 0.0f, 20.0f));
}

TEST(RaypathEnsembleApplicability, BNeedsAzimuthInvariantUnderAHalfTurn) {
  // A column's zenith is symmetric about 90°, but with a non-uniform azimuth the crystal's two
  // ends are not equally likely to point toward the sun: 3-1 and 3-2 differ.
  AxisDistribution axis = ParseAxis(kColumn);
  ASSERT_TRUE(detail::IsBApplicable(axis));
  axis.azimuth_dist = Distribution{ DistributionType::kGaussian, 0.0f, 30.0f };
  EXPECT_FALSE(detail::IsBApplicable(axis));
  axis.azimuth_dist = Distribution{ DistributionType::kUniform, 0.0f, 180.0f };
  EXPECT_FALSE(detail::IsBApplicable(axis));
  axis.azimuth_dist = Distribution{ DistributionType::kNoRandom, 30.0f, 0.0f };
  EXPECT_FALSE(detail::IsBApplicable(axis));
}

TEST(RaypathEnsembleApplicability, BIgnoresRoll) {
  AxisDistribution axis = ParseAxis(kParry);
  EXPECT_TRUE(detail::IsBApplicable(axis));
  axis.roll_dist = Distribution{ DistributionType::kNoRandom, 17.0f, 0.0f };
  EXPECT_TRUE(detail::IsBApplicable(axis));
}

// ---- Source B of the physical P gating: the gluing lemma (detail::GluingRetainsHalfTurn) ----
// Equal pole leaves (zenith density symmetric about 0°/180° over a folding latitude path) with a
// full-turn uniform azimuth make the measure invariant under the 180° rotation, whatever the roll.

TEST(RaypathEnsembleApplicability, GluingNeedsOnlyTheZenithAndAzimuth) {
  const AxisDistribution lowitz = ParseAxis(kLowitz);
  EXPECT_TRUE(detail::GluingRetainsHalfTurn(lowitz));
  // The wire zenith 180° centre (core latitude −90°) is the same pole pair.
  AxisDistribution down = lowitz;
  down.latitude_dist.center = -90.0f;
  EXPECT_TRUE(detail::GluingRetainsHalfTurn(down));
  // Every folding latitude type glues from its own centre slot: Laplacian, a pole-centred
  // uniform interval (its far half folds back), and the legacy no-Jacobian Gaussian.
  AxisDistribution lap = lowitz;
  lap.latitude_dist = Distribution{ DistributionType::kLaplacian, 90.0f, 5.0f };
  EXPECT_TRUE(detail::GluingRetainsHalfTurn(lap));
  AxisDistribution uni = lowitz;
  uni.latitude_dist = Distribution{ DistributionType::kUniform, 90.0f, 8.0f };
  EXPECT_TRUE(detail::GluingRetainsHalfTurn(uni));
  AxisDistribution legacy = lowitz;
  legacy.latitude_dist = Distribution{ DistributionType::kGaussianLegacy, 90.0f, 5.0f };
  EXPECT_TRUE(detail::GluingRetainsHalfTurn(legacy));
  // Roll plays no part — the leaves feed the roll density, not the other way (design prediction 1:
  // a roll centred at 30° keeps the half-turn).
  AxisDistribution roll30 = lowitz;
  roll30.roll_dist = Distribution{ DistributionType::kGaussian, 30.0f, 1.0f };
  EXPECT_TRUE(detail::GluingRetainsHalfTurn(roll30));
}

TEST(RaypathEnsembleApplicability, GluingNegativeControls) {
  const AxisDistribution lowitz = ParseAxis(kLowitz);
  // Zenith centre 20°: the two leaves weigh P(zenith raw > 0) = Φ(centre/σ) ≠ 1/2 — the corpus's
  // control axis, where the roll petals come out 49880/15656 instead of even.
  AxisDistribution control = lowitz;
  control.latitude_dist.center = 70.0f;
  EXPECT_FALSE(detail::GluingRetainsHalfTurn(control));
  // A non-uniform azimuth weights the leaves per sky position.
  AxisDistribution az_gauss = lowitz;
  az_gauss.azimuth_dist = Distribution{ DistributionType::kGaussian, 0.0f, 30.0f };
  EXPECT_FALSE(detail::GluingRetainsHalfTurn(az_gauss));
  // A zenith uniform over [0°,180°] never crosses a pole — nothing folds (prediction 2).
  AxisDistribution band = ParseAxis(kColumn);
  band.latitude_dist = Distribution{ DistributionType::kUniform, 0.0f, 180.0f };
  EXPECT_FALSE(detail::GluingRetainsHalfTurn(band));
  // The full-sphere uniform routes to the fold-free asin path: symmetric, but nothing glues.
  EXPECT_FALSE(detail::GluingRetainsHalfTurn(ParseAxis(kRandom)));
  // A locked zenith feeds one leaf only.
  AxisDistribution locked = lowitz;
  locked.latitude_dist = Distribution{ DistributionType::kNoRandom, 90.0f, 0.0f };
  EXPECT_FALSE(detail::GluingRetainsHalfTurn(locked));
  // kZigzag is answered conservatively, like the B channel.
  AxisDistribution zig = lowitz;
  zig.latitude_dist = Distribution{ DistributionType::kZigzag, 90.0f, 5.0f };
  EXPECT_FALSE(detail::GluingRetainsHalfTurn(zig));
}

}  // namespace
}  // namespace lumice
