// CouldFaceExist — whether a face can bound any crystal a config draws — and the warning a scene
// gets when one of its filters names a face its crystal never has. Matching itself is untouched:
// such a filter still matches nothing through that face; the point is that it is no longer silent.
//
// The shape is the one the three-crystal e2e fixtures carried: face_distance [2, 1, 2, 1, 2, 1]
// leaves faces 3, 5 and 7 zero-wide. Before raypath reduction followed the crystal's own symmetry,
// a filter on 3-5 there matched rays on 4-6 by a full-hexagon rotation; since then it matches none.

#include <gtest/gtest.h>

#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

#include "config/config_manager.hpp"
#include "config/crystal_config.hpp"
#include "core/crystal.hpp"
#include "support/log_capture.hpp"

extern std::string config_file_name;

namespace lumice {
namespace {

Distribution Fixed(float v) {
  return Distribution{ DistributionType::kNoRandom, v, 0.0f };
}

Distribution Uniform(float center, float full_range) {
  return Distribution{ DistributionType::kUniform, center, full_range };
}

PrismCrystalParam Prism(const float (&d)[6]) {
  PrismCrystalParam p;
  p.h_ = Fixed(1.0f);
  for (int i = 0; i < 6; i++) {
    p.d_[i] = Fixed(d[i]);
  }
  return p;
}

TEST(CouldFaceExist, AlternatingDistancesDropEveryOtherPrismFace) {
  const auto p = Prism({ 2, 1, 2, 1, 2, 1 });
  for (IdType face : { 3, 5, 7 }) {
    EXPECT_FALSE(CouldFaceExist(p, face)) << face;
  }
  for (IdType face : { 1, 2, 4, 6, 8 }) {
    EXPECT_TRUE(CouldFaceExist(p, face)) << face;
  }
}

TEST(CouldFaceExist, ARegularPrismHasEveryFace) {
  const auto p = Prism({ 1, 1, 1, 1, 1, 1 });
  for (IdType face = 1; face <= 8; face++) {
    EXPECT_TRUE(CouldFaceExist(p, face)) << face;
  }
}

TEST(CouldFaceExist, AUniformRangeCountsIfAnyCornerHasTheFace) {
  // Face 3's own distance drawn from [1.5, 2.5]: at 1.5 it is narrower than its neighbours' corner
  // (1 + 1) and has area, so some draws have it.
  auto p = Prism({ 2, 1, 2, 1, 2, 1 });
  p.d_[0] = Uniform(2.0f, 1.0f);
  EXPECT_TRUE(CouldFaceExist(p, 3));
  // Entirely past the corner: no draw has it.
  p.d_[0] = Uniform(2.5f, 0.5f);
  EXPECT_FALSE(CouldFaceExist(p, 3));
  // A neighbour that can grow past 1 widens the corner and lets the face through.
  p.d_[1] = Uniform(1.5f, 2.0f);
  EXPECT_TRUE(CouldFaceExist(p, 3));
}

TEST(CouldFaceExist, UnboundedDistributionsAreNotJudged) {
  auto p = Prism({ 2, 1, 2, 1, 2, 1 });
  p.d_[0] = Distribution{ DistributionType::kGaussian, 2.0f, 0.01f };
  EXPECT_TRUE(CouldFaceExist(p, 3));
}

TEST(CouldFaceExist, APyramidWithoutAnUpperConeHasNoUpperConeFaces) {
  PyramidCrystalParam p;
  p.h_pyr_u_ = Fixed(0.0f);
  p.h_prs_ = Fixed(1.0f);
  p.h_pyr_l_ = Fixed(0.5f);
  for (auto& d : p.d_) {
    d = Fixed(1.0f);
  }
  EXPECT_FALSE(CouldFaceExist(p, 13));
  EXPECT_TRUE(CouldFaceExist(p, 23));
  EXPECT_TRUE(CouldFaceExist(p, 1));
  EXPECT_TRUE(CouldFaceExist(p, 3));
  // A full upper cone reaches its apex and cuts basal face 1 away.
  p.h_pyr_u_ = Fixed(1.0f);
  EXPECT_TRUE(CouldFaceExist(p, 13));
  EXPECT_FALSE(CouldFaceExist(p, 1));
}

// The scene-level half: a filter bound to such a crystal is reported once per missing face, and a
// filter on faces the crystal has stays quiet.
nlohmann::json SceneWithFilter(const nlohmann::json& raypath) {
  std::ifstream f(config_file_name);
  nlohmann::json j;
  f >> j;
  j["crystal"] =
      nlohmann::json::array({ { { "id", 1 },
                                { "type", "prism" },
                                { "shape", { { "height", 1.0 }, { "face_distance", { 2, 1, 2, 1, 2, 1 } } } } } });
  j["filter"] =
      nlohmann::json::array({ { { "id", 1 }, { "type", "raypath" }, { "raypath", raypath }, { "symmetry", "PBD" } } });
  j["scene"]["scattering"] =
      nlohmann::json::array({ { { "prob", 0.0 }, { "entries", { { { "crystal", 1 }, { "filter", 1 } } } } } });
  return j;
}

TEST(FilterFacesTheCrystalLacks, AreWarnedAtSceneParse) {
  std::string text;
  {
    test::LogCapture capture;
    (void)SceneWithFilter({ 3, 5 }).get<ConfigManager>();
    text = capture.Text();
  }
  EXPECT_NE(text.find("names face 3"), std::string::npos) << text;
  EXPECT_NE(text.find("names face 5"), std::string::npos) << text;
  EXPECT_EQ(test::CountOccurrences(text, "which crystal 1 never has"), 2u) << text;
}

TEST(FilterFacesTheCrystalLacks, AFilterOnPresentFacesStaysQuiet) {
  std::string text;
  {
    test::LogCapture capture;
    (void)SceneWithFilter({ 4, 6 }).get<ConfigManager>();
    text = capture.Text();
  }
  EXPECT_EQ(text.find("never has"), std::string::npos) << text;
}

}  // namespace
}  // namespace lumice
