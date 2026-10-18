#ifndef RAYPATH_SURVEY_TEST_FIXTURES_H_
#define RAYPATH_SURVEY_TEST_FIXTURES_H_

// In-memory scenes shared by the survey unit tests (the corpus configs' shapes without the
// corpus files, which never enter the tree). One scene, one builder, every test that needs the
// shape reads it from here — the duplicated ~20-line builders this header replaces are how a
// fixture fix lands in one test file and not the other.

#include "config/config_manager.hpp"
#include "core/crystal_param.hpp"
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

}  // namespace lumice::raypath::survey

#endif  // RAYPATH_SURVEY_TEST_FIXTURES_H_
