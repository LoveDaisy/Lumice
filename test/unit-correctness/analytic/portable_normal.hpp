// Standard normal deviates that are the same on every standard library.
//
// std::normal_distribution's algorithm is implementation-defined: libc++ (macOS), libstdc++ (Linux)
// and the MSVC STL draw different values from the same std::mt19937_64 stream, so a test seeded
// identically samples different poses on each CI leg (PR #444: three legs failed on poses macOS
// never visited). std::mt19937_64's output sequence is specified by the standard, so Box-Muller on
// its raw bits gives the same deviates everywhere, up to the last-bit differences of each libm's
// log / cos.

#ifndef LUMICE_TEST_ANALYTIC_PORTABLE_NORMAL_HPP_
#define LUMICE_TEST_ANALYTIC_PORTABLE_NORMAL_HPP_

#include <cmath>
#include <random>

namespace lumice::analytic {

// Drop-in for std::normal_distribution<double> (mean 0, standard deviation 1): g(rng).
class PortableNormal {
 public:
  double operator()(std::mt19937_64& rng) {
    if (has_spare_) {
      has_spare_ = false;
      return spare_;
    }
    constexpr double kTwoPi = 6.283185307179586476925286766559;
    const double u1 = Uniform01Open(rng);  // (0, 1]: log(u1) is finite
    const double u2 = Uniform01Open(rng);
    const double radius = std::sqrt(-2.0 * std::log(u1));
    spare_ = radius * std::sin(kTwoPi * u2);
    has_spare_ = true;
    return radius * std::cos(kTwoPi * u2);
  }

 private:
  // The top 53 bits as a double in (0, 1].
  static double Uniform01Open(std::mt19937_64& rng) {
    return (static_cast<double>(rng() >> 11) + 1.0) * (1.0 / 9007199254740992.0);
  }

  double spare_ = 0.0;
  bool has_spare_ = false;
};

}  // namespace lumice::analytic

#endif  // LUMICE_TEST_ANALYTIC_PORTABLE_NORMAL_HPP_
