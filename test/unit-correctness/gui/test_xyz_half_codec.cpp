// xyz_half_codec.hpp — the one float16 encoding both the live GL upload and the .lmc v6 texture
// section go through. Two propositions, each held against an oracle that shares no line with the
// header:
//   1. FloatToHalfBits / HalfBitsToFloat ARE IEEE 754 binary16 with round-to-nearest-even. The
//      oracle is the half format's definition itself: every one of the 65536 bit patterns is
//      decoded by ldexp, and float->half is checked against "the nearest of those 65536 values,
//      ties to the even mantissa" found by search — on every exact half value, on every midpoint
//      between adjacent halves, and on a million random floats across the range.
//   2. quantize ∘ dequantize ∘ quantize == quantize, bit for bit, with the scale recomputed from
//      the dequantized data — the fixed point the reopened-.lmc path rests on (a v6 file is
//      dequantized to float, then re-quantized by the same upload path a live frame takes, and
//      must land on the same half bits under the same scale). Held on frame-shaped data: sparse,
//      30 stops of dynamic range, a single hot spot.
// Header-only and GL-free, so this is unit_correctness_test rather than gui_unit_test.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <random>
#include <vector>

#include "gui/xyz_half_codec.hpp"

namespace lumice::gui {
namespace {

uint32_t Bits(float f) {
  uint32_t u = 0;
  std::memcpy(&u, &f, sizeof(u));
  return u;
}

// The half format, by definition, from its fields.
float ReferenceHalfValue(uint16_t h) {
  const int sign = (h & 0x8000u) ? -1 : 1;
  const int exp = (h >> 10) & 0x1F;
  const int mant = h & 0x3FF;
  if (exp == 31) {
    return mant == 0 ? sign * std::numeric_limits<float>::infinity() : std::numeric_limits<float>::quiet_NaN();
  }
  if (exp == 0) {
    return static_cast<float>(sign) * std::ldexp(static_cast<float>(mant), -24);
  }
  return static_cast<float>(sign) * std::ldexp(static_cast<float>(1024 + mant), exp - 25);
}

// All finite non-negative half values, ascending, paired with their bit patterns.
struct HalfTable {
  std::vector<float> values;  // ascending
  std::vector<uint16_t> bits;
  HalfTable() {
    for (uint32_t h = 0; h < 0x7C00u; ++h) {  // 0x7C00 is +Inf; everything below is finite
      values.push_back(ReferenceHalfValue(static_cast<uint16_t>(h)));
      bits.push_back(static_cast<uint16_t>(h));
    }
    // Non-negative finite halves are already in ascending bit order == ascending value order.
    for (size_t i = 1; i < values.size(); ++i) {
      if (!(values[i] > values[i - 1])) {
        ADD_FAILURE() << "half table premise broken at " << i;
      }
    }
  }
};

// Nearest half to |f|, ties to even mantissa, then the sign folded in. NaN -> any quiet NaN;
// |f| >= 65520 -> Inf (the midpoint between 65504 and the next would-be half, rounding to even).
uint16_t ReferenceFloatToHalf(const HalfTable& t, float f) {
  if (std::isnan(f)) {
    return 0x7E00u;
  }
  const uint16_t sign = std::signbit(f) ? 0x8000u : 0u;
  const float a = std::fabs(f);
  if (a >= 65520.0f) {
    return static_cast<uint16_t>(sign | 0x7C00u);
  }
  // First half value >= a.
  const auto it = std::lower_bound(t.values.begin(), t.values.end(), a);
  const size_t hi = static_cast<size_t>(it - t.values.begin());
  if (hi == 0) {
    return sign;  // a == 0
  }
  if (hi == t.values.size()) {
    return static_cast<uint16_t>(sign | t.bits.back());  // a in (65504, 65520): rounds down
  }
  const size_t lo = hi - 1;
  // Exact midpoint test in double: both endpoints and a are floats, so the arithmetic is exact.
  const double d_lo = static_cast<double>(a) - static_cast<double>(t.values[lo]);
  const double d_hi = static_cast<double>(t.values[hi]) - static_cast<double>(a);
  size_t pick = 0;
  if (d_lo < d_hi) {
    pick = lo;
  } else if (d_hi < d_lo) {
    pick = hi;
  } else {
    pick = (t.bits[lo] & 1u) == 0 ? lo : hi;  // tie: even mantissa
  }
  return static_cast<uint16_t>(sign | t.bits[pick]);
}

TEST(XyzHalfCodec, EveryHalfBitPatternDecodesToItsDefinedValue) {
  for (uint32_t h = 0; h <= 0xFFFFu; ++h) {
    const float got = HalfBitsToFloat(static_cast<uint16_t>(h));
    const float want = ReferenceHalfValue(static_cast<uint16_t>(h));
    if (std::isnan(want)) {
      EXPECT_TRUE(std::isnan(got)) << "half 0x" << std::hex << h;
    } else {
      EXPECT_EQ(Bits(got), Bits(want)) << "half 0x" << std::hex << h;
    }
  }
}

TEST(XyzHalfCodec, EveryFiniteHalfValueRoundTripsThroughFloatToItsOwnBits) {
  for (uint32_t h = 0; h <= 0xFFFFu; ++h) {
    if (((h >> 10) & 0x1Fu) == 31u) {
      continue;  // Inf / NaN covered below
    }
    const uint16_t hh = static_cast<uint16_t>(h);
    // -0 and +0 are distinct bit patterns and both must survive.
    EXPECT_EQ(FloatToHalfBits(HalfBitsToFloat(hh)), hh) << "half 0x" << std::hex << h;
  }
}

TEST(XyzHalfCodec, FloatToHalfRoundsToNearestEvenAtEveryMidpointAndOnRandomFloats) {
  const HalfTable t;
  // Non-fatal per row, and the flood is capped: a systematic rounding defect would otherwise
  // print a hundred thousand identical lines, while the first twenty already name the pattern.
  int failures = 0;
  const auto check = [&](float x, const char* what, size_t i) {
    const uint16_t got = FloatToHalfBits(x);
    const uint16_t want = ReferenceFloatToHalf(t, x);
    if (got != want) {
      ADD_FAILURE() << what << " " << i << ": float " << x << " -> 0x" << std::hex << got << ", want 0x" << want;
      ++failures;
    }
  };
  constexpr int kMaxReported = 20;
  // Every midpoint between adjacent non-negative finite halves, both signs. Midpoints are exactly
  // representable in float for the normal range (one more bit than half), and for subnormals too.
  for (size_t i = 0; i + 1 < t.values.size() && failures < kMaxReported; ++i) {
    const float mid =
        static_cast<float>((static_cast<double>(t.values[i]) + static_cast<double>(t.values[i + 1])) / 2.0);
    check(mid, "midpoint above half index", i);
    check(-mid, "negative midpoint above half index", i);
    // And one float ulp to either side, which must break the tie the obvious way.
    check(std::nextafter(mid, 0.0f), "just below midpoint", i);
    check(std::nextafter(mid, std::numeric_limits<float>::infinity()), "just above midpoint", i);
  }
  // The overflow edge: 65504 stays, 65520 (the midpoint to the absent next half) rounds to Inf,
  // anything between rounds down to 65504.
  EXPECT_EQ(FloatToHalfBits(65504.0f), 0x7BFFu);
  EXPECT_EQ(FloatToHalfBits(std::nextafter(65520.0f, 0.0f)), 0x7BFFu);
  EXPECT_EQ(FloatToHalfBits(65520.0f), 0x7C00u);
  EXPECT_EQ(FloatToHalfBits(1.0e9f), 0x7C00u);
  EXPECT_EQ(FloatToHalfBits(-1.0e9f), 0xFC00u);
  EXPECT_EQ(FloatToHalfBits(std::numeric_limits<float>::infinity()), 0x7C00u);
  EXPECT_EQ((FloatToHalfBits(std::numeric_limits<float>::quiet_NaN()) & 0x7C00u), 0x7C00u);
  EXPECT_NE((FloatToHalfBits(std::numeric_limits<float>::quiet_NaN()) & 0x03FFu), 0u);
  // Signed zeros and the smallest subnormal's neighbourhood.
  EXPECT_EQ(FloatToHalfBits(0.0f), 0x0000u);
  EXPECT_EQ(FloatToHalfBits(-0.0f), 0x8000u);
  EXPECT_EQ(FloatToHalfBits(std::ldexp(1.0f, -25)), 0x0000u);  // exactly half the smallest subnormal: to even (0)
  EXPECT_EQ(FloatToHalfBits(std::nextafter(std::ldexp(1.0f, -25), 1.0f)), 0x0001u);
  EXPECT_EQ(FloatToHalfBits(std::ldexp(3.0f, -25)), 0x0002u);  // 1.5 quanta: to even (2)

  // A million random floats over the range a scaled frame occupies (0 .. 2^15 plus a tail past
  // 65504) and over the subnormal band, log-uniform so every binade gets samples.
  std::mt19937 rng(20260919u);
  std::uniform_real_distribution<double> log_exp(-30.0, 17.0);
  std::uniform_real_distribution<double> mant(1.0, 2.0);
  for (int i = 0; i < 1'000'000 && failures < kMaxReported; ++i) {
    const float f = static_cast<float>(std::ldexp(mant(rng), static_cast<int>(std::floor(log_exp(rng)))));
    check((i & 1) ? -f : f, "random sample", static_cast<size_t>(i));
  }
}

// Frame-shaped data: mostly zero, the non-zero values log-uniform over 30 stops, one hot spot that
// sets the scale. This is the distribution the encoding-probe measured on real renders.
std::vector<float> FrameLikeXyz(size_t n, uint32_t seed, float hot) {
  std::mt19937 rng(seed);
  std::uniform_real_distribution<float> coin(0.0f, 1.0f);
  std::uniform_real_distribution<double> log_e(-25.0, 5.0);
  std::vector<float> xyz(n, 0.0f);
  for (size_t i = 0; i < n; ++i) {
    if (coin(rng) < 0.4f) {
      xyz[i] = static_cast<float>(std::exp2(log_e(rng)));
    }
  }
  xyz[n / 3] = hot;
  return xyz;
}

TEST(XyzHalfCodec, ScaleMapsTheBrightestComponentToExactlyTwoToTheFifteenth) {
  const std::vector<float> xyz = FrameLikeXyz(30'000, 1u, 4.6e4f);
  const float scale = ComputeXyzHalfScale(xyz.data(), xyz.size());
  std::vector<uint16_t> half(xyz.size());
  QuantizeXyzToHalf(xyz.data(), xyz.size(), scale, half.data());
  EXPECT_EQ(half[xyz.size() / 3], 0x7800u);  // 2^15 as a half: exponent 15+15=30, mantissa 0
  for (uint16_t h : half) {
    EXPECT_LE(h & 0x7FFFu, 0x7800u) << "no component may quantize above the brightest one";
  }
  // Non-max components: relative error within half's rounding half-width for normals.
  std::vector<float> back(xyz.size());
  DequantizeHalfToXyz(half.data(), half.size(), scale, back.data());
  for (size_t i = 0; i < xyz.size(); ++i) {
    if (xyz[i] / scale >= std::ldexp(1.0f, -14)) {  // half-normal range
      EXPECT_LE(std::fabs(back[i] - xyz[i]), std::ldexp(1.0f, -11) * xyz[i]) << "component " << i;
    } else {
      EXPECT_LE(std::fabs(back[i] - xyz[i]), std::ldexp(1.0f, -25) * scale) << "subnormal component " << i;
    }
  }
}

TEST(XyzHalfCodec, AllZeroFrameQuantizesToZeroWithoutDividingByZero) {
  const std::vector<float> zeros(64, 0.0f);
  EXPECT_EQ(ComputeXyzHalfScale(zeros.data(), zeros.size()), 1.0f);
  EXPECT_EQ(ComputeXyzHalfScale(zeros.data(), 0), 1.0f);
  std::vector<uint16_t> half(zeros.size(), 0xFFFFu);
  QuantizeXyzToHalf(zeros.data(), zeros.size(), ComputeXyzHalfScale(zeros.data(), zeros.size()), half.data());
  for (uint16_t h : half) {
    EXPECT_EQ(h, 0u);
  }
}

// The fixed point. Quantize a frame; dequantize it; recompute the scale from the result (this is
// what the upload path does to a reopened file — it is handed floats and knows nothing about
// where they came from); quantize again. Same scale, same bits, on several frames including hot
// spots that are not powers of two and one that sits exactly on a half midpoint after scaling.
TEST(XyzHalfCodec, QuantizeDequantizeQuantizeIsAFixedPointUnderTheRecomputedScale) {
  const float hots[] = { 4.6e4f, 65.0f, 0.7f, 3.3333e-3f, 1.0f, 12345.678f };
  uint32_t seed = 7u;
  for (float hot : hots) {
    SCOPED_TRACE(hot);
    const std::vector<float> xyz = FrameLikeXyz(50'000, seed++, hot);
    const float s1 = ComputeXyzHalfScale(xyz.data(), xyz.size());
    std::vector<uint16_t> h1(xyz.size());
    QuantizeXyzToHalf(xyz.data(), xyz.size(), s1, h1.data());

    std::vector<float> back(xyz.size());
    DequantizeHalfToXyz(h1.data(), h1.size(), s1, back.data());
    const float s2 = ComputeXyzHalfScale(back.data(), back.size());
    ASSERT_EQ(Bits(s2), Bits(s1)) << "the recomputed scale moved";

    std::vector<uint16_t> h2(xyz.size());
    QuantizeXyzToHalf(back.data(), back.size(), s2, h2.data());
    ASSERT_EQ(std::memcmp(h1.data(), h2.data(), h1.size() * sizeof(uint16_t)), 0) << "the requantized bits moved";

    // And the dequantized floats are a fixed point too (Save of a reopened document writes the
    // same section back).
    std::vector<float> back2(xyz.size());
    DequantizeHalfToXyz(h2.data(), h2.size(), s2, back2.data());
    ASSERT_EQ(std::memcmp(back.data(), back2.data(), back.size() * sizeof(float)), 0);
  }
}

// Every half value, scaled by an awkward scale, must survive the same round trip — this is the
// exhaustive form of the proposition above, independent of the frame distribution.
TEST(XyzHalfCodec, EveryHalfValueIsAFixedPointUnderAnAwkwardScale) {
  const float scales[] = { 1.0f, 1.2345678e-3f, 3.0f / 32768.0f, 7.77e5f, 9.999e-9f };
  for (float scale : scales) {
    SCOPED_TRACE(scale);
    for (uint32_t h = 0; h < 0x7C00u; ++h) {
      const uint16_t hh = static_cast<uint16_t>(h);
      float back = 0.0f;
      DequantizeHalfToXyz(&hh, 1, scale, &back);
      uint16_t again = 0;
      QuantizeXyzToHalf(&back, 1, scale, &again);
      ASSERT_EQ(again, hh) << "half 0x" << std::hex << h;
    }
  }
}

}  // namespace
}  // namespace lumice::gui
