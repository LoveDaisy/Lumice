// The channel-B-R display formula, asserted on the function itself.
//
// Same reason test_ink_transfer.cpp exists: the call sites are expensive to stand up (a consumer and
// a snapshot on the CLI side, a GL context on the GUI side), and what is left when both are stripped
// away is two floats in and one float out. test_render_consumer_channel_math.cpp asserts that the
// renderer USES this formula on the right inputs; this file asserts what the formula IS. The GLSL
// twin, `channelMathBrGray()` in the preview shader, is held to it by the preview/export/CLI parity
// tests under test/gui/parity/.

#include <gtest/gtest.h>

#include <algorithm>

#include "util/channel_math.hpp"

namespace lumice {
namespace {

// R == B is exactly mid grey, not "close to" it: 0.5 + 0.5 * 0 is exact in IEEE 754, and "no
// difference" being a single known value is what lets a user read mid grey as zero.
TEST(ChannelMath, EqualChannelsAreExactlyMidGrey) {
  for (float v : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f }) {
    EXPECT_EQ(ChannelMathBrGray(v, v), 0.5f) << v;
  }
}

// The two extremes land on the ends of the range and not past them.
TEST(ChannelMath, PureRedIsBlackAndPureBlueIsWhite) {
  EXPECT_EQ(ChannelMathBrGray(1.0f, 0.0f), 0.0f);
  EXPECT_EQ(ChannelMathBrGray(0.0f, 1.0f), 1.0f);
}

// The clamp: inputs outside [0, 1] (which a caller should not pass, but a gamut-clipped value can
// graze) must still come out as a legal grey.
TEST(ChannelMath, OutputIsClampedToUnitRange) {
  EXPECT_EQ(ChannelMathBrGray(2.0f, -1.0f), 0.0f);
  EXPECT_EQ(ChannelMathBrGray(-1.0f, 2.0f), 1.0f);
}

// "Bluer is lighter": for a fixed R, the output never decreases as B grows — and, symmetrically,
// never increases as R grows. Sampled densely enough to catch an inverted sign or a swapped argument.
TEST(ChannelMath, MonotoneInBlueAndAntitoneInRed) {
  for (float fixed : { 0.0f, 0.3f, 0.7f, 1.0f }) {
    float prev_b = -1.0f;
    float prev_r = 2.0f;
    for (int k = 0; k <= 100; ++k) {
      const float x = static_cast<float>(k) / 100.0f;
      const float by_b = ChannelMathBrGray(fixed, x);
      const float by_r = ChannelMathBrGray(x, fixed);
      EXPECT_GE(by_b, prev_b) << "r=" << fixed << " b=" << x;
      EXPECT_LE(by_r, prev_r) << "r=" << x << " b=" << fixed;
      prev_b = by_b;
      prev_r = by_r;
    }
  }
}

// The closed form, evaluated in double on a grid. What this catches is an operation-order or clamp
// slip, not whether the formula is the right one — that question was settled by the owner, and the
// renderer-level cross-check against an independent reference implementation of the whole chain
// lives in test_render_consumer_channel_math.cpp.
TEST(ChannelMath, MatchesTheClosedForm) {
  for (int i = 0; i <= 20; ++i) {
    for (int k = 0; k <= 20; ++k) {
      const double r = i / 20.0;
      const double b = k / 20.0;
      const double expected = std::clamp(0.5 + 0.5 * (b - r), 0.0, 1.0);
      EXPECT_NEAR(ChannelMathBrGray(static_cast<float>(r), static_cast<float>(b)), expected, 1e-6)
          << "r=" << r << " b=" << b;
    }
  }
}

}  // namespace
}  // namespace lumice
