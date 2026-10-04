#include <cmath>

#include "core/geo3d.hpp"
#include "core/lat_lut.hpp"
#include "core/product_sample_transform.hpp"
#include "core/shape_sample.hpp"
#include "core/shared/lat_path_selection.hpp"
#include "gtest/gtest.h"

namespace {
namespace ns = lumice;

TEST(ProductSampleTransform, RawDistributionValuesHaveIndependentOracles) {
  EXPECT_EQ(ns::TransformDistribution({ ns::DistributionType::kNoRandom, -3.f, 0.f }, {}), -3.f);
  EXPECT_EQ(ns::TransformDistribution({ ns::DistributionType::kUniform, 4.f, 8.f }, { .25f }), 2.f);
  EXPECT_EQ(ns::TransformDistribution({ ns::DistributionType::kGaussian, 4.f, 3.f }, { -2.f }), -2.f);
  EXPECT_EQ(ns::TransformDistribution({ ns::DistributionType::kGaussianLegacy, 4.f, 3.f }, { -2.f }), -2.f);
  EXPECT_NEAR(ns::TransformDistribution({ ns::DistributionType::kZigzag, 0.f, 30.f }, { .75f }), 30.f, 1e-5f);
  EXPECT_NEAR(ns::TransformDistribution({ ns::DistributionType::kLaplacian, 4.f, 2.f }, { .25f }),
              4.f - 2.f * std::log(2.f), 1e-6f);
  double sum = 0, sq = 0;
  for (int i = 0; i < 10000; ++i) {
    const float v = ns::TransformDistribution({ ns::DistributionType::kZigzag, 0.f, 30.f }, { (i + .5f) / 10000.f });
    sum += v;
    sq += v * v;
  }
  EXPECT_NEAR(std::sqrt(sq / 10000 - std::pow(sum / 10000, 2)), 9.23275, 1e-4);
}

TEST(ProductSampleTransform, LegacyCrossingFlipsAzimuthAndRoll) {
  const auto latitude = ns::TransformLegacyLatitude(120.f);
  EXPECT_TRUE(latitude.flip);
  EXPECT_NEAR(latitude.radians, 60.f * ns::math::kDegreeToRad, 1e-6f);
  const auto angles = ns::ComposeAxisAngles(latitude, 10.f, 20.f);
  EXPECT_EQ(angles[0], 10.f * ns::math::kDegreeToRad + ns::math::kPi);
  EXPECT_EQ(angles[2], 20.f * ns::math::kDegreeToRad + ns::math::kPi);
}

TEST(ProductSampleTransform, ProductionDrawScheduleMatchesExplicitReplay) {
  // Both RNGs run the same standard library. Absolute normal-distribution pins
  // are not portable between libc++, libstdc++ and MSVC.
  const ns::DistributionType types[]{ ns::DistributionType::kNoRandom, ns::DistributionType::kUniform,
                                      ns::DistributionType::kGaussian, ns::DistributionType::kGaussianLegacy,
                                      ns::DistributionType::kZigzag,   ns::DistributionType::kLaplacian };
  auto& production = ns::RandomNumberGenerator::GetInstance();
  production.SetSeed(9873);
  ns::RandomNumberGenerator replay(9873);
  for (auto type : types) {
    ns::AxisDistribution axis;
    axis.latitude_dist = { type, 100.f, 30.f };
    axis.azimuth_dist = { ns::DistributionType::kUniform, 0.f, 360.f };
    axis.roll_dist = { ns::DistributionType::kZigzag, 0.f, 30.f };
    const auto path = ns::lat_path::SelectLatPath(axis);
    const auto* lut = ns::GetSharedLatLut(axis.latitude_dist);
    for (int i = 0; i < 8; ++i) {
      ns::LatitudeSample latitude;
      if (path.kind == ns::lat_path::LatPathKind::kLutInverseCdf) {
        const float xi = replay.GetUniform();
        const float flip = replay.GetUniform();
        latitude = ns::TransformLatitudeLut(*lut, { xi, flip });
      } else if (type == ns::DistributionType::kGaussianLegacy) {
        latitude = ns::TransformLegacyLatitude(replay.Get(axis.latitude_dist));
      } else {
        latitude = { replay.Get(axis.latitude_dist) * ns::math::kDegreeToRad, false };
      }
      const float az = replay.Get(axis.azimuth_dist);
      const float roll = replay.Get(axis.roll_dist);
      const auto expected = ns::ComposeAxisAngles(latitude, az, roll);
      float actual[3];
      ns::RandomSampler::SampleSphericalPointsSph(axis, actual, 1, lut);
      for (int j = 0; j < 3; ++j)
        EXPECT_EQ(actual[j], expected[j]);
    }
  }
  EXPECT_EQ(production.GetUniform(), replay.GetUniform());
}

TEST(ProductSampleTransform, SphereAndFiniteCapKeepTwoDrawsEvenAtZeroRadius) {
  auto& production = ns::RandomNumberGenerator::GetInstance();
  production.SetSeed(293);
  ns::RandomNumberGenerator replay(293);
  const float lat = replay.GetUniform();
  const float lon = replay.GetUniform();
  float sphere[2];
  ns::RandomSampler::SampleSphericalPointsSph(sphere, 1, 2);
  const auto expected = ns::TransformFullSpherePoint(lat, lon);
  EXPECT_EQ(sphere[0], expected[0]);
  EXPECT_EQ(sphere[1], expected[1]);
  for (float radius : { 0.f, 4.f }) {
    const float radial = replay.GetUniform();
    const float azimuth = replay.GetUniform();
    const auto cap = ns::MakeSphericalCapTransform(213.f * ns::math::kDegreeToRad, -21.f * ns::math::kDegreeToRad,
                                                   radius * ns::math::kDegreeToRad);
    const auto point = ns::TransformSphericalCap(cap, { radial, azimuth });
    float actual[3];
    ns::SampleSphCapPoint(213.f, -21.f, radius, actual);
    for (int j = 0; j < 3; ++j)
      EXPECT_EQ(actual[j], point[j]);
  }
  EXPECT_EQ(production.GetUniform(), replay.GetUniform());
}
TEST(ProductShapeSample, LeaderRecordReplaysWithoutRngOrFollowerRescaling) {
  ns::PrismCrystalParam p;
  p.h_ = { ns::DistributionType::kUniform, -2.f, .5f };
  for (auto& d : p.d_)
    d = { ns::DistributionType::kUniform, 100.f, 12.f };
  p.sync_group_[ns::kShapeScalarHeight] = 8;
  p.sync_group_[ns::kShapeScalarFace0] = 8;
  const auto plan = ns::BuildShapeDrawPlan(p);
  ns::RandomNumberGenerator rng(764), production(764);
  const auto values = ns::DrawShapeLeaders(rng, plan);
  ns::ShapeSample sample;
  ASSERT_EQ(ns::RealizeShape(plan, values, &sample), ns::ShapeSampleStatus::kOk);
  EXPECT_EQ(values.size, 6u);
  EXPECT_EQ(sample.raw[0], sample.raw[ns::kShapeScalarFace0]);
  EXPECT_LT(sample.raw[0], 0.f);
  EXPECT_EQ(sample.consumed[0], -sample.raw[0]);
  EXPECT_EQ(sample.consumed[ns::kShapeScalarFace0], sample.raw[0]);
  float distances[6];
  EXPECT_EQ(ns::SamplePrismShapeScalars(production, p, distances), sample.consumed[0]);
  for (int i = 0; i < 6; ++i)
    EXPECT_EQ(distances[i], sample.consumed[ns::kShapeScalarFace0 + i]);
  EXPECT_EQ(rng.GetUniform(), production.GetUniform());
  auto bad = values;
  bad.size--;
  EXPECT_EQ(ns::RealizeShape(plan, bad, &sample), ns::ShapeSampleStatus::kMissingLeader);
  bad = values;
  bad.values[1] = bad.values[0];
  EXPECT_EQ(ns::RealizeShape(plan, bad, &sample), ns::ShapeSampleStatus::kDuplicateLeader);
  bad = values;
  bad.values[0].slot = ns::kShapeScalarFace0;
  EXPECT_EQ(ns::RealizeShape(plan, bad, &sample), ns::ShapeSampleStatus::kInvalidLeader);
}

TEST(ProductShapeSample, PyramidLeaderOrderAndCrossKindConsumption) {
  ns::PyramidCrystalParam p;
  p.h_pyr_u_ = { ns::DistributionType::kNoRandom, -.25f, 0.f };
  p.h_prs_ = { ns::DistributionType::kNoRandom, 0.f, 0.f };
  p.h_pyr_l_ = { ns::DistributionType::kNoRandom, .4f, 0.f };
  for (auto& d : p.d_)
    d = { ns::DistributionType::kNoRandom, 1.f, 0.f };
  p.sync_group_[ns::kShapeScalarUpperH] = 9;
  p.sync_group_[ns::kShapeScalarFace2] = 9;
  ns::RandomNumberGenerator rng(43), ref(43);
  const auto plan = ns::BuildShapeDrawPlan(p);
  const auto values = ns::DrawShapeLeaders(rng, plan);
  EXPECT_EQ(values.values[0].slot, ns::kShapeScalarUpperH);
  EXPECT_EQ(values.values[1].slot, ns::kShapeScalarPrismH);
  EXPECT_EQ(values.values[2].slot, ns::kShapeScalarLowerH);
  ns::ShapeSample sample;
  ASSERT_EQ(ns::RealizeShape(plan, values, &sample), ns::ShapeSampleStatus::kOk);
  EXPECT_EQ(sample.consumed[ns::kShapeScalarUpperH], .25f);
  EXPECT_EQ(sample.consumed[ns::kShapeScalarFace2], -.25f);
  EXPECT_EQ(sample.consumed[ns::kShapeScalarPrismH], 0.f);
  EXPECT_EQ(rng.GetUniform(), ref.GetUniform());
}

}  // namespace
