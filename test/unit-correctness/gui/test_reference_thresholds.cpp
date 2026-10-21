// The threshold resolver (test/support/reference_thresholds.hpp) — the mechanical evidence that
// the gate TUs' run-time threshold lookup really reads _thresholds.json, plus its failure shapes.
//
// Two propositions carry the acceptance:
//   * json edits move the threshold: writing a value resolves that value, rewriting the same
//     fixture resolves the NEW value — the json decides, not a source literal (none is left).
//   * every malformed shape — missing file, bad JSON, missing group/scene/threshold, non-numeric
//     threshold — resolves to ok=false with a non-empty reason, which the harness turns into a
//     loud red rather than a silent default.
//
// The last case smokes the tracked json through the compile-time LUMICE_TEST_REF_DIR without
// asserting any of its numbers — copying a threshold here would recreate exactly the second
// source the resolver removed. The fixture values below are self-contained test data.

#include <gtest/gtest.h>

#include <cmath>
#include <fstream>
#include <string>

#include "support/reference_thresholds.hpp"

namespace {

constexpr const char* kOneGroup = R"({
  "groups": {
    "lens_proj": {
      "scenes": {
        "rectangular": {"threshold": 31.25}
      }
    }
  }
})";

constexpr const char* kOneGroupRewritten = R"({
  "groups": {
    "lens_proj": {
      "scenes": {
        "rectangular": {"threshold": 12.5}
      }
    }
  }
})";

std::string WriteFixture(const char* name, const char* content) {
  const std::string path = std::string(::testing::TempDir()) + name;
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << content;
  return path;
}

}  // namespace

TEST(ReferenceThresholds, ResolvesTheValueItReads) {
  const std::string path = WriteFixture("thresholds_read.json", kOneGroup);
  const lumice::test::ThresholdResolution r =
      lumice::test::ResolveReferenceThreshold(path.c_str(), "lens_proj", "rectangular");
  ASSERT_TRUE(r.ok) << r.error;
  EXPECT_DOUBLE_EQ(r.threshold, 31.25);
}

TEST(ReferenceThresholds, JsonEditMovesTheThreshold) {
  const std::string path = WriteFixture("thresholds_edit.json", kOneGroup);
  const lumice::test::ThresholdResolution before =
      lumice::test::ResolveReferenceThreshold(path.c_str(), "lens_proj", "rectangular");
  ASSERT_TRUE(before.ok) << before.error;

  WriteFixture("thresholds_edit.json", kOneGroupRewritten);
  const lumice::test::ThresholdResolution after =
      lumice::test::ResolveReferenceThreshold(path.c_str(), "lens_proj", "rectangular");
  ASSERT_TRUE(after.ok) << after.error;
  EXPECT_DOUBLE_EQ(after.threshold, 12.5);
}

TEST(ReferenceThresholds, MissingFileFailsLoudly) {
  const std::string path = std::string(::testing::TempDir()) + "thresholds_absent.json";
  std::remove(path.c_str());
  const lumice::test::ThresholdResolution r =
      lumice::test::ResolveReferenceThreshold(path.c_str(), "lens_proj", "rectangular");
  EXPECT_FALSE(r.ok);
  EXPECT_NE(r.error.find(path), std::string::npos) << r.error;
}

TEST(ReferenceThresholds, MalformedJsonFails) {
  const std::string path = WriteFixture("thresholds_bad.json", "{oops");
  const lumice::test::ThresholdResolution r =
      lumice::test::ResolveReferenceThreshold(path.c_str(), "lens_proj", "rectangular");
  EXPECT_FALSE(r.ok);
  EXPECT_FALSE(r.error.empty());
}

TEST(ReferenceThresholds, MissingGroupOrSceneFails) {
  const std::string path = WriteFixture("thresholds_keys.json", kOneGroup);
  const auto missing_group = lumice::test::ResolveReferenceThreshold(path.c_str(), "modal_layout", "crystal_prism");
  EXPECT_FALSE(missing_group.ok);
  EXPECT_NE(missing_group.error.find("modal_layout"), std::string::npos) << missing_group.error;
  const auto missing_scene = lumice::test::ResolveReferenceThreshold(path.c_str(), "lens_proj", "linear");
  EXPECT_FALSE(missing_scene.ok);
  EXPECT_NE(missing_scene.error.find("linear"), std::string::npos) << missing_scene.error;
}

TEST(ReferenceThresholds, MissingOrNonNumericThresholdFails) {
  const std::string path = WriteFixture("thresholds_no_value.json", R"({
  "groups": {"g": {"scenes": {"no_field": {}, "wrong_type": {"threshold": "40.0"}}}}
})");
  const auto missing = lumice::test::ResolveReferenceThreshold(path.c_str(), "g", "no_field");
  EXPECT_FALSE(missing.ok);
  EXPECT_FALSE(missing.error.empty());
  const auto wrong_type = lumice::test::ResolveReferenceThreshold(path.c_str(), "g", "wrong_type");
  EXPECT_FALSE(wrong_type.ok);
  EXPECT_FALSE(wrong_type.error.empty());
}

TEST(ReferenceThresholds, TrackedJsonResolves) {
  const lumice::test::ThresholdResolution r =
      lumice::test::ResolveReferenceThreshold(LUMICE_TEST_REF_DIR "/_thresholds.json", "lens_proj", "rectangular");
  ASSERT_TRUE(r.ok) << r.error;
  EXPECT_TRUE(std::isfinite(r.threshold));
  EXPECT_GT(r.threshold, 0.0);
}
