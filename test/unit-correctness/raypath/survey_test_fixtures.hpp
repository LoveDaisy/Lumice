#ifndef RAYPATH_SURVEY_TEST_FIXTURES_H_
#define RAYPATH_SURVEY_TEST_FIXTURES_H_

// In-memory scenes shared by the survey unit tests (the corpus configs' shapes without the
// corpus files, which never enter the tree). One scene, one builder, every test that needs the
// shape reads it from here — the duplicated ~20-line builders this header replaces are how a
// fixture fix lands in one test file and not the other.

#include "config/config_manager.hpp"
#include "core/crystal_param.hpp"
#include "core/miller_wedge.hpp"  // ConvertMillerIndexToWedgeAngle: the conversion the config loader runs
#include "util/illuminant_data.hpp"

namespace lumice::raypath::survey {

// The C14/C15 shape: a hexagonal plate prism, c axis at the zenith (config latitude slot's 90)
// with a Gaussian wobble, azimuth and roll uniform over 360. `height` and the wobble `std`
// are the two knobs the corpus variants differ in (C14: 0.3/0.3, C15: 1.5/0.3). The axis lives
// in the config's latitude slot, where 90 IS zenith 0.
inline ConfigManager PlateScene(double height, double wobble_std) {
  PrismCrystalParam prism;
  prism.h_ = { DistributionType::kNoRandom, static_cast<float>(height), 0 };
  for (auto& distance : prism.d_) {
    distance = { DistributionType::kNoRandom, 1, 0 };
  }
  CrystalConfig crystal{};
  crystal.id_ = 1;
  crystal.param_ = prism;
  crystal.axis_.latitude_dist = { DistributionType::kGaussian, 90, static_cast<float>(wobble_std) };
  crystal.axis_.azimuth_dist = crystal.axis_.roll_dist = { DistributionType::kUniform, 0, 360 };
  ConfigManager config;
  config.crystals_.emplace(1, crystal);
  config.scene_.light_source_.param_ = { 20, 0, 0 };
  config.scene_.light_source_.spectrum_ = IlluminantType::kD65;
  ScatteringSetting entry{};
  entry.crystal_ = crystal;
  entry.crystal_proportion_ = 1;
  config.scene_.ms_.push_back({ 0, { entry } });
  return config;
}

// The C01 shape: a regular hexagonal column, every axis slot uniform over the full sphere
// (random orientation), sun on the horizon.
inline ConfigManager RandomPrismScene() {
  PrismCrystalParam prism;
  prism.h_ = { DistributionType::kNoRandom, 1.0f, 0 };
  for (auto& distance : prism.d_) {
    distance = { DistributionType::kNoRandom, 1, 0 };
  }
  CrystalConfig crystal{};
  crystal.id_ = 1;
  crystal.param_ = prism;
  // Full-sphere uniform: the internal latitude's center is 90 (the config's zenith-0 slot
  // spelling), and IsFullSphereUniform requires exactly that center with a full 360 range.
  crystal.axis_.latitude_dist = { DistributionType::kUniform, 90, 360 };
  crystal.axis_.azimuth_dist = crystal.axis_.roll_dist = { DistributionType::kUniform, 0, 360 };
  ConfigManager config;
  config.crystals_.emplace(1, crystal);
  config.scene_.light_source_.param_ = { 0, 0, 0 };
  config.scene_.light_source_.spectrum_ = IlluminantType::kD65;
  ScatteringSetting entry{};
  entry.crystal_ = crystal;
  entry.crystal_proportion_ = 1;
  config.scene_.ms_.push_back({ 0, { entry } });
  return config;
}

// The C17 shape: the same regular prism as RandomPrismScene (h 1, fd all 1, full-sphere
// uniform) with the sun at `altitude` degrees (the corpus snapshot has 15). Its touching
// four-face chains (3-1-5-7 family) are the refused-support arm's shape.
inline ConfigManager RegularPrismSceneWithSun(double altitude) {
  ConfigManager config = RandomPrismScene();
  config.scene_.light_source_.param_ = { static_cast<float>(altitude), 0, 0 };
  return config;
}

// The C15 fixed-pose control: the same plate with the zenith and roll pinned to scalars (the
// corpus's "zenith 0, roll 0" variant). The axis maps to no pose density — the honest-refusal
// arm's shape.
inline ConfigManager FixedPosePlateScene(double height) {
  PrismCrystalParam prism;
  prism.h_ = { DistributionType::kNoRandom, static_cast<float>(height), 0 };
  for (auto& distance : prism.d_) {
    distance = { DistributionType::kNoRandom, 1, 0 };
  }
  CrystalConfig crystal{};
  crystal.id_ = 1;
  crystal.param_ = prism;
  crystal.axis_.latitude_dist = { DistributionType::kNoRandom, 90, 0 };
  crystal.axis_.azimuth_dist = { DistributionType::kUniform, 0, 360 };
  crystal.axis_.roll_dist = { DistributionType::kNoRandom, 0, 0 };
  ConfigManager config;
  config.crystals_.emplace(1, crystal);
  config.scene_.light_source_.param_ = { 20, 0, 0 };
  config.scene_.light_source_.spectrum_ = IlluminantType::kD65;
  ScatteringSetting entry{};
  entry.crystal_ = crystal;
  entry.crystal_proportion_ = 1;
  config.scene_.ms_.push_back({ 0, { entry } });
  return config;
}

// The C09 shape: the corpus's asymmetric cone pyramid on a full-sphere uniform axis, sun at 15
// degrees. The wedge angles are the corpus's Miller indices ([1,0,1] upper / [2,0,3] lower)
// through the engine's own converter — the same conversion the config loader runs on the JSON
// snapshot. Its literal 13-15-26-28 chain is the support-set (hole + upper-edge) case.
inline ConfigManager AsymmetricPyramidScene() {
  PyramidCrystalParam pyramid;
  pyramid.h_prs_ = { DistributionType::kNoRandom, 0.5f, 0 };
  pyramid.h_pyr_u_ = { DistributionType::kNoRandom, 0.25f, 0 };
  pyramid.h_pyr_l_ = { DistributionType::kNoRandom, 0.6f, 0 };
  const float distances[6] = { 1.0f, 1.1f, 0.9f, 1.0f, 1.2f, 0.95f };
  for (int k = 0; k < 6; k++) {
    pyramid.d_[k] = { DistributionType::kNoRandom, distances[k], 0 };
  }
  const MillerConversionResult upper_wedge = ConvertMillerIndexToWedgeAngle(1, 0, 1, 3);  // the corpus's [1,0,1]
  const MillerConversionResult lower_wedge = ConvertMillerIndexToWedgeAngle(2, 0, 3, 3);  // the corpus's [2,0,3]
  EXPECT_EQ(upper_wedge.state, MillerConversionState::kValid);
  EXPECT_EQ(lower_wedge.state, MillerConversionState::kValid);
  pyramid.wedge_angle_u_ = upper_wedge.wedge_angle_deg;
  pyramid.wedge_angle_l_ = lower_wedge.wedge_angle_deg;
  CrystalConfig crystal{};
  crystal.id_ = 1;
  crystal.param_ = pyramid;
  crystal.axis_.latitude_dist = { DistributionType::kUniform, 90, 360 };
  crystal.axis_.azimuth_dist = crystal.axis_.roll_dist = { DistributionType::kUniform, 0, 360 };
  ConfigManager config;
  config.crystals_.emplace(1, crystal);
  config.scene_.light_source_.param_ = { 15, 0, 0 };
  config.scene_.light_source_.spectrum_ = IlluminantType::kD65;
  ScatteringSetting entry{};
  entry.crystal_ = crystal;
  entry.crystal_proportion_ = 1;
  config.scene_.ms_.push_back({ 0, { entry } });
  return config;
}


// The C16 Lowitz measure: a regular prism whose c-axis sits near the vertical with a WIDE
// zenith gauss (the corpus's zenith0/std40) and a NARROW roll gauss (0.3) — the routing's
// calibration pair (the roll freezes first, the wide zenith must not survive its own freeze).
inline ConfigManager LowitzMeasureScene(double zenith_std, double roll_std) {
  PrismCrystalParam prism;
  prism.h_ = { DistributionType::kNoRandom, 1.0f, 0 };
  for (auto& distance : prism.d_) {
    distance = { DistributionType::kNoRandom, 1, 0 };
  }
  CrystalConfig crystal{};
  crystal.id_ = 1;
  crystal.param_ = prism;
  // The latitude slot holds latitude; zenith 0 (the c-axis vertical) is latitude 90.
  crystal.axis_.latitude_dist = { DistributionType::kGaussian, 90, static_cast<float>(zenith_std) };
  crystal.axis_.azimuth_dist = { DistributionType::kUniform, 0, 360 };
  crystal.axis_.roll_dist = { DistributionType::kGaussian, 0, static_cast<float>(roll_std) };
  ConfigManager config;
  config.crystals_.emplace(1, crystal);
  config.scene_.light_source_.param_ = { 20, 0, 0 };
  config.scene_.light_source_.spectrum_ = IlluminantType::kD65;
  ScatteringSetting entry{};
  entry.crystal_ = crystal;
  entry.crystal_proportion_ = 1;
  config.scene_.ms_.push_back({ 0, { entry } });
  return config;
}

// The Parry measure: the c-axis near HORIZONTAL (zenith 90 = latitude 0) with a narrow zenith
// gauss and a narrow roll gauss — the ladder freezes roll then zenith, and level 2's
// {azimuth} family is the Parry circle itself (a one-dimensional regular image).
inline ConfigManager ParryMeasureScene(double zenith_std, double roll_std) {
  ConfigManager config = LowitzMeasureScene(zenith_std, roll_std);
  auto crystal = config.crystals_.find(1);
  crystal->second.axis_.latitude_dist = { DistributionType::kGaussian, 0, static_cast<float>(zenith_std) };
  // The scene entry carries its own copy of the crystal; keep the two in step.
  for (auto& setting : config.scene_.ms_.front().setting_) {
    setting.crystal_.axis_.latitude_dist = crystal->second.axis_.latitude_dist;
  }
  return config;
}

// The all-fixed axis: the single pose (the axis-default semantics — an axis block left out
// entirely produces exactly this distribution). The 0D arm's shape.
inline ConfigManager SinglePoseScene() {
  PrismCrystalParam prism;
  prism.h_ = { DistributionType::kNoRandom, 1.0f, 0 };
  for (auto& distance : prism.d_) {
    distance = { DistributionType::kNoRandom, 1, 0 };
  }
  CrystalConfig crystal{};
  crystal.id_ = 1;
  crystal.param_ = prism;
  crystal.axis_.latitude_dist = { DistributionType::kNoRandom, 90, 0 };
  crystal.axis_.azimuth_dist = { DistributionType::kNoRandom, 0, 0 };
  crystal.axis_.roll_dist = { DistributionType::kNoRandom, 0, 0 };
  ConfigManager config;
  config.crystals_.emplace(1, crystal);
  config.scene_.light_source_.param_ = { 20, 0, 0 };
  config.scene_.light_source_.spectrum_ = IlluminantType::kD65;
  ScatteringSetting entry{};
  entry.crystal_ = crystal;
  entry.crystal_proportion_ = 1;
  config.scene_.ms_.push_back({ 0, { entry } });
  return config;
}

// An outside-domain shape: a zigzag roll (a distribution type the routing vocabulary does not
// read) — the refused-measure arm's shape.
inline ConfigManager ZigzagRollScene() {
  ConfigManager config = LowitzMeasureScene(0.3, 0.3);
  config.crystals_.find(1)->second.axis_.roll_dist = { DistributionType::kZigzag, 0, 1.0f };
  for (auto& setting : config.scene_.ms_.front().setting_) {
    setting.crystal_.axis_.roll_dist = { DistributionType::kZigzag, 0, 1.0f };
  }
  return config;
}

}  // namespace lumice::raypath::survey

#endif  // RAYPATH_SURVEY_TEST_FIXTURES_H_
