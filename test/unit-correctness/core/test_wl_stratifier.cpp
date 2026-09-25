#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "core/math.hpp"
#include "core/wl_stratifier.hpp"

namespace {

using lumice::RandomNumberGenerator;
using lumice::WavelengthStratifier;

std::vector<float> Draw(uint32_t seed, size_t n) {
  RandomNumberGenerator rng(seed);
  WavelengthStratifier s;
  std::vector<float> out(n);
  for (auto& u : out) {
    u = s.NextUnit(rng);
  }
  return out;
}

// Largest |count - n/bins| over the bins of [0, 1).
double WorstBinDeviation(const std::vector<float>& xs, size_t bins) {
  std::vector<int> count(bins, 0);
  for (float x : xs) {
    count[std::min(static_cast<size_t>(x * static_cast<float>(bins)), bins - 1)]++;
  }
  double worst = 0.0;
  for (int c : count) {
    worst = std::max(worst, std::abs(c - static_cast<double>(xs.size()) / static_cast<double>(bins)));
  }
  return worst;
}

TEST(WavelengthStratifier, EveryDrawStaysInTheUnitInterval) {
  size_t outside = 0;
  for (uint32_t seed = 1; seed <= 50; ++seed) {
    for (float u : Draw(seed, 1000)) {
      outside += (u < 0.0f || u >= 1.0f) ? 1 : 0;
    }
  }
  EXPECT_EQ(outside, 0u);
}

TEST(WavelengthStratifier, ConsecutiveDrawsCoverTheBandEvenly) {
  // The property the CPU path is after: 64 consecutive batches put 4 +- 1 wavelengths in
  // each 25 nm sixteenth of [380, 780], for every random shift (measured worst case over
  // 2e5 shifts: exactly 1). Independent uniform draws meet this bound with probability
  // ~1e-4, so a regression back to them fails here on essentially every seed.
  for (uint32_t seed = 1; seed <= 200; ++seed) {
    EXPECT_LE(WorstBinDeviation(Draw(seed, 64), 16), 1.0) << "seed " << seed;
  }
}

TEST(WavelengthStratifier, EachDrawIsUniformOverTheRandomShift) {
  // Unbiasedness does not rest on completing a cycle: the k-th draw alone is uniform
  // over the shift, for every k, so a run that stops after any number of batches still
  // integrates the spectrum without bias. Checked for several k over 20000 shifts.
  constexpr size_t kSeeds = 20000;
  constexpr size_t kBins = 10;
  for (size_t k : { 0u, 1u, 2u, 17u, 500u }) {
    std::vector<float> kth(kSeeds);
    for (size_t s = 0; s < kSeeds; ++s) {
      kth[s] = Draw(static_cast<uint32_t>(s + 1), k + 1)[k];
    }
    // Binomial sd per bin = sqrt(2000 * 0.9) ~ 42; 5 sd.
    EXPECT_LE(WorstBinDeviation(kth, kBins), 212.0) << "draw #" << k;
  }
}

TEST(WavelengthStratifier, DrawsFromTheRngOnlyOnce) {
  // One RNG draw per Run(), not per batch: the shift. Everything else the worker's RNG
  // decides must not be displaced by the wavelength schedule.
  RandomNumberGenerator used(7);
  RandomNumberGenerator reference(7);
  WavelengthStratifier s;
  for (int i = 0; i < 100; ++i) {
    s.NextUnit(used);
  }
  reference.GetUniform();
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(used.GetUniform(), reference.GetUniform());
  }
}

TEST(WavelengthStratifier, SameSeedReplaysTheSameSequence) {
  EXPECT_EQ(Draw(42, 300), Draw(42, 300));
  EXPECT_NE(Draw(42, 300), Draw(43, 300));
}

}  // namespace
