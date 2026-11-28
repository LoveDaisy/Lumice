#include "analytic/axis_pose_density.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>

#include "analytic/pose_density.hpp"
#include "core/math.hpp"

namespace lumice::analytic {
namespace {

constexpr double kPi = math::kPiD;
constexpr double kHalfPi = kPi / 2;
constexpr double kTurn = 2 * kPi;
constexpr double kDeg = math::kDegreeToRadD;
constexpr double kEps = std::numeric_limits<double>::epsilon();
constexpr double kRoundoff = 64 * kEps;
constexpr double kNegInf = -std::numeric_limits<double>::infinity();
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

uint32_t Flag(AxisDensityFlag flag) {
  return static_cast<uint32_t>(flag);
}

AxisDensityValue Invalid(AxisDensityFlag flag) {
  return { kNaN, Flag(flag) };
}

AxisDensityValue Exp(AxisDensityValue result) {
  result.value = std::exp(result.value);
  return result;
}

double Wrap(double x) {
  return std::remainder(x, kTurn);
}

struct Folded {
  double latitude;
  bool flip;
};

Folded Fold(double x) {
  x = Wrap(x);
  if (x > kHalfPi) {
    return { kPi - x, true };
  }
  if (x < -kHalfPi) {
    return { -kPi - x, true };
  }
  return { x, false };
}

bool Uniform(const Distribution& d) {
  return d.type == DistributionType::kUniform && d.spread == 360.0f;
}

double LogAdd(double a, double b) {
  if (a == kNegInf) {
    return b;
  }
  if (b == kNegInf) {
    return a;
  }
  if (std::isinf(a) || std::isinf(b)) {
    return std::max(a, b);
  }
  return std::max(a, b) + std::log1p(std::exp(-std::abs(a - b)));
}

// Phi^-1 on the LOWER half only, avoiding 1-U cancellation in either Gaussian tail.
// Bracketed Newton on erfc is independent of a rational quantile approximation.
double NormalQuantile(double u) {
  double lower = -40;
  double upper = 0;
  double z = -std::sqrt(-2 * std::log(u));
  for (int i = 0; i < 64; ++i) {
    const double cdf = 0.5 * std::erfc(-z / std::sqrt(2.0));
    if (cdf < u) {
      lower = z;
    } else {
      upper = z;
    }
    const double step = (cdf - u) * std::sqrt(kTurn) * std::exp(0.5 * z * z);
    if (std::abs(step) <= 4 * kEps * std::max(1.0, std::abs(z))) {
      return z;
    }
    const double next = z - step;
    z = next > lower && next < upper ? next : 0.5 * (lower + upper);
  }
  return z;
}

struct Rules {
  std::array<double, 16> x16{}, w16{};
  std::array<double, 32> x32{}, w32{};
  Rules() {
    GaussLegendre(16, x16.data(), w16.data());
    GaussLegendre(32, x32.data(), w32.data());
  }
};

const Rules& Rule() {
  static const Rules rule;
  return rule;
}

template <size_t N, class F>
double Quadrature(double lo, double hi, const std::array<double, N>& x, const std::array<double, N>& w, const F& f,
                  double* absolute = nullptr) {
  double result = 0;
  double magnitude = 0;
  const double h = (hi - lo) / 2;
  for (size_t i = 0; i < N; ++i) {
    const double term = w[i] * f(lo + h * (1 + x[i]));
    result += term;
    magnitude += std::abs(term);
  }
  if (absolute) {
    *absolute = h * magnitude;
  }
  return h * result;
}

template <class F>
double Adaptive(double lo, double hi, const F& f, uint32_t& flags, double accuracy) {
  struct Panel {
    double lo, hi, value, error;
    int depth;
    bool operator<(const Panel& other) const { return error < other.error; }
  };
  const auto& rule = Rule();
  double magnitude = 0;
  const double fine = Quadrature(lo, hi, rule.x32, rule.w32, f, &magnitude);
  const double coarse = Quadrature(lo, hi, rule.x16, rule.w16, f);
  // One error-estimate budget for the WHOLE native interval, never one copy per child.
  // Absolute mass scaling preserves tiny but real fold sheets without an absolute floor.
  const double tolerance = accuracy * std::max({ magnitude, std::abs(coarse), hi - lo });
  std::priority_queue<Panel> panels;
  double error = std::abs(fine - coarse);
  double value = fine;
  panels.push({ lo, hi, fine, error, 0 });
  for (int splits = 0; error > tolerance; ++splits) {
    const auto parent = panels.top();
    if (parent.depth == 64 || splits == 4096) {
      flags |= Flag(AxisDensityFlag::kQuadratureAccuracy);
      break;
    }
    panels.pop();
    const double mid = parent.lo + (parent.hi - parent.lo) / 2;
    double child_value = 0;
    double child_error = 0;
    for (const auto& bounds : { std::array<double, 2>{ parent.lo, mid }, { mid, parent.hi } }) {
      const double v = Quadrature(bounds[0], bounds[1], rule.x32, rule.w32, f);
      const double e = std::abs(v - Quadrature(bounds[0], bounds[1], rule.x16, rule.w16, f));
      panels.push({ bounds[0], bounds[1], v, e, parent.depth + 1 });
      child_value += v;
      child_error += e;
    }
    value += child_value - parent.value;
    error = std::max(0.0, error - parent.error) + child_error;
  }
  return value;
}

// Integrate a function of the RAW draw in the sampling-native probability coordinate. Break
// points are raw angle values: folds, rectification corners, and wrapped-deviation seams.
template <class F>
double NativeExpectation(const Distribution& d, const std::vector<double>& breaks, const F& f, uint32_t& flags) {
  const double c = d.center * kDeg;
  const double s = d.spread * kDeg;
  if (d.type == DistributionType::kNoRandom) {
    return f(c);
  }
  double result = 0;
  const bool gaussian = d.type == DistributionType::kGaussian || d.type == DistributionType::kGaussianLegacy;
  const int branches = gaussian || d.type == DistributionType::kLaplacian ? 2 : 1;
  for (int branch = 0; branch < branches; ++branch) {
    const double sign = branch == 0 ? -1.0 : 1.0;
    const double hi = branches == 2 ? 0.5 : 1.0;
    std::vector<double> points{ 0.0, hi };
    auto add = [&](double u) {
      // Do not create subnormal-sized quadrature panels. The [0,first] tail remains
      // integrated; only additional breakpoints below 1e-40 probability are omitted.
      if (u >= 1e-40 && u < hi) {
        points.push_back(u);
      }
    };
    if (d.type == DistributionType::kZigzag) {
      points.insert(points.end(), { 0.25, 0.5, 0.75 });
    }
    for (double b : breaks) {
      if (gaussian) {
        const double z = (b - c) / s;
        if (z * sign >= 0) {
          add(0.5 * std::erfc(std::abs(z) / std::sqrt(2.0)));
        }
      } else if (d.type == DistributionType::kLaplacian) {
        if ((b - c) * sign >= 0) {
          add(0.5 * std::exp(-std::abs(b - c) / s));
        }
      } else if (d.type == DistributionType::kUniform) {
        add((b - c) / s + 0.5);
      } else {
        // Rectification has preimages at both b and -b, and its corner at raw zero.
        for (double raw : { b, -b }) {
          const double t = (raw - c) / s;
          if (std::abs(t) <= 1) {
            const double a = std::asin(t);
            for (double phase : { a, kPi - a }) {
              double u = std::fmod(phase / kTurn + 1, 1.0);
              add(u);
            }
          }
        }
      }
    }
    std::sort(points.begin(), points.end());
    points.erase(std::unique(points.begin(), points.end()), points.end());
    auto integrand = [&](double u) {
      double raw;
      if (gaussian) {
        raw = c - sign * s * NormalQuantile(u);
      } else if (d.type == DistributionType::kLaplacian) {
        raw = c - sign * s * std::log(2 * u);
      } else if (d.type == DistributionType::kUniform) {
        raw = c + s * (u - 0.5);
      } else {
        raw = std::abs(c + s * std::sin(kTurn * u));
      }
      return f(raw);
    };
    for (size_t i = 1; i < points.size(); ++i) {
      const double accuracy = 16 * kEps * std::max(1.0, (1 + std::abs(c)) / s);
      result += Adaptive(points[i - 1], points[i], integrand, flags, accuracy);
    }
  }
  return result;
}

// All finite kinks with non-negligible probability: beyond 40 sigma/scales the omitted mass is
// <exp(-40) for Laplace and much smaller for Gaussian. The expectation itself is NOT truncated.
std::vector<double> LatitudeBreaks() {
  std::vector<double> result{ 0.0 };
  for (int i = -164; i <= 164; ++i) {
    result.push_back(i * kHalfPi);
  }
  return result;
}

AxisDensityValue RawLog(const Distribution& d, double x) {
  const double c = d.center * kDeg;
  const double s = d.spread * kDeg;
  switch (d.type) {
    case DistributionType::kUniform: {
      const double lower = (static_cast<double>(d.center) - static_cast<double>(d.spread) / 2) * kDeg;
      const double upper = (static_cast<double>(d.center) + static_cast<double>(d.spread) / 2) * kDeg;
      return { x >= lower && x < upper ? -std::log(s) : kNegInf, 0 };
    }
    case DistributionType::kGaussian:
    case DistributionType::kGaussianLegacy: {
      const double z = (x - c) / s;
      return { -0.5 * z * z - std::log(s) - 0.5 * std::log(kTurn), 0 };
    }
    case DistributionType::kLaplacian:
      return { -std::abs(x - c) / s - std::log(2 * s), 0 };
    case DistributionType::kZigzag: {
      if (x < 0) {
        return { kNegInf, 0 };
      }
      AxisDensityValue result{ kNegInf, 0 };
      const double lower = (static_cast<double>(d.center) - static_cast<double>(d.spread)) * kDeg;
      const double upper = (static_cast<double>(d.center) + static_cast<double>(d.spread)) * kDeg;
      for (double raw : { x, -x }) {
        const double from_lower = raw - lower;
        const double from_upper = upper - raw;
        if (std::min(std::abs(from_lower), std::abs(from_upper)) <=
            std::sqrt(kEps) * std::max({ 1.0, std::abs(c), s })) {
          result.flags |= Flag(AxisDensityFlag::kCusp);
        }
        if (raw == lower || raw == upper) {
          // Recognize the represented degree-space endpoint without an epsilon clamp.
          result.value = std::numeric_limits<double>::infinity();
        } else {
          // Preserve the c/s proposal parameterization in the interior; converting endpoints
          // separately would perturb the measure in ill-conditioned near-cusp importance ratios.
          const long double delta = static_cast<long double>(raw) - c;
          const long double distance = static_cast<long double>(s) - std::abs(delta);
          if (distance >= 0) {
            const double value = static_cast<double>(
                -std::log(static_cast<long double>(kPi)) -
                0.5L * (std::log(distance) + std::log(static_cast<long double>(s) + std::abs(delta))));
            result.value = LogAdd(result.value, value);
          }
        }
      }
      return result;
    }
    default:
      return { kNegInf, 0 };
  }
}

AxisDensityValue Images(const Distribution& d, double x) {
  if (Uniform(d)) {
    return { -std::log(kTurn), 0 };
  }
  const double c = d.center * kDeg;
  const double s = d.spread * kDeg;
  if (d.type == DistributionType::kLaplacian) {
    const double distance = std::abs(Wrap(x - c));
    return { LogAdd(-distance / s, -(kTurn - distance) / s) - std::log(2 * s) - std::log(-std::expm1(-kTurn / s)), 0 };
  }
  const int nearest = static_cast<int>(std::round((c - x) / kTurn));
  int radius = 2;
  if (d.type == DistributionType::kGaussian || d.type == DistributionType::kGaussianLegacy) {
    const double delta = x + nearest * kTurn - c;
    radius = static_cast<int>(std::ceil((std::abs(delta) + std::sqrt(delta * delta + 80 * s * s)) / kTurn)) + 1;
  } else if (d.type == DistributionType::kZigzag) {
    radius = 4;  // |center|, spread <=2*pi, and rectification stays in [0,4*pi].
  }
  AxisDensityValue result{ kNegInf, 0 };
  for (int i = nearest - radius; i <= nearest + radius; ++i) {
    const auto term = RawLog(d, x + i * kTurn);
    result.value = LogAdd(result.value, term.value);
    result.flags |= term.flags;
  }
  return result;
}

bool Match(double a, double b) {
  return std::abs(Wrap(a - b)) <= kRoundoff;
}

// Centered native moments, not E[X^2]-E[X]^2. Scaling removes narrow-law conditioning from
// adaptive error estimates; the final radian scaling happens only after integration.
double AngularVariance(const Distribution& d, double unflipped_probability, double flip_probability, uint32_t& flags) {
  if (Uniform(d)) {
    return kPi * kPi / 3;
  }
  std::vector<double> breaks{ 0.0 };
  const double re = NativeExpectation(
                        d, breaks, [](double x) { return std::cos(x); }, flags) *
                    (unflipped_probability - flip_probability);
  const double im = NativeExpectation(
                        d, breaks, [](double x) { return std::sin(x); }, flags) *
                    (unflipped_probability - flip_probability);
  if (std::hypot(re, im) <= kRoundoff) {
    flags |= Flag(AxisDensityFlag::kCircularMeanUndefined);
    return kNaN;
  }
  const double mean = std::atan2(im, re);
  for (int i = -164; i <= 164; ++i) {
    breaks.push_back(mean + i * kPi);
  }
  const double scale = d.type == DistributionType::kNoRandom ? 1.0 : d.spread * kDeg;
  return NativeExpectation(
             d, breaks,
             [&](double x) {
               const double a = Wrap(x - mean) / scale;
               const double b = Wrap(x + kPi - mean) / scale;
               return unflipped_probability * a * a + flip_probability * b * b;
             },
             flags) *
         scale * scale;
}

}  // namespace

const char* AxisPoseDensityError(const AxisDistribution& d) {
  const Distribution axes[] = { d.azimuth_dist, d.latitude_dist, d.roll_dist };
  const char* type_errors[] = { "azimuth_dist.type is unknown", "latitude_dist.type is unknown",
                                "roll_dist.type is unknown" };
  const char* center_errors[] = { "azimuth_dist.center must be finite with absolute value <=360",
                                  "latitude_dist.center must be finite with absolute value <=360",
                                  "roll_dist.center must be finite with absolute value <=360" };
  const char* spread_errors[] = { "azimuth_dist.spread must be finite and, unless pinned, in [0.001,360]",
                                  "latitude_dist.spread must be finite and, unless pinned, in [0.001,360]",
                                  "roll_dist.spread must be finite and, unless pinned, in [0.001,360]" };
  for (int i = 0; i < 3; ++i) {
    switch (axes[i].type) {
      case DistributionType::kNoRandom:
      case DistributionType::kUniform:
      case DistributionType::kGaussian:
      case DistributionType::kGaussianLegacy:
      case DistributionType::kZigzag:
      case DistributionType::kLaplacian:
        break;
      default:
        return type_errors[i];
    }
    if (!std::isfinite(axes[i].center) || std::abs(axes[i].center) > 360) {
      return center_errors[i];
    }
    if (!std::isfinite(axes[i].spread) ||
        (axes[i].type != DistributionType::kNoRandom && (axes[i].spread < 0.001f || axes[i].spread > 360))) {
      return spread_errors[i];
    }
  }
  if (d.latitude_dist.type == DistributionType::kNoRandom && std::abs(d.latitude_dist.center) > 90) {
    return "latitude_dist.center must be in [-90,90] when pinned";
  }
  return "";
}

AxisPoseDensity::AxisPoseDensity(const AxisDistribution& d) : distribution_(d) {
  const char* error = AxisPoseDensityError(d);
  if (*error) {
    throw std::invalid_argument(error);
  }
  const auto& lat = d.latitude_dist;
  if (lat.type == DistributionType::kNoRandom) {
    if (std::abs(lat.center) == 90) {
      moment_flags_ |= Flag(AxisDensityFlag::kUndefinedPole);
    }
  } else {
    const bool area = lat.type != DistributionType::kGaussianLegacy;
    const double scale = lat.spread * kDeg;
    const double raw_anchor = lat.center * kDeg;
    const double anchor = Fold(lat.type == DistributionType::kZigzag ? std::abs(raw_anchor) : raw_anchor).latitude;
    const double weight_scale = area ? std::max(std::abs(std::cos(anchor)), std::min(1.0, scale)) : 1.0;
    const auto breaks = LatitudeBreaks();
    auto weight = [&](double raw) { return area ? std::abs(std::cos(raw)) / weight_scale : 1.0; };
    const double unflipped =
        NativeExpectation(lat, breaks, [&](double raw) { return Fold(raw).flip ? 0.0 : weight(raw); }, moment_flags_);
    const double flipped =
        NativeExpectation(lat, breaks, [&](double raw) { return Fold(raw).flip ? weight(raw) : 0.0; }, moment_flags_);
    const double total = unflipped + flipped;
    normalization_ = area ? total * weight_scale : 1.0;
    flip_probability_ = flipped / total;
    unflipped_probability_ = unflipped / total;
    const double first =
        NativeExpectation(
            lat, breaks, [&](double raw) { return weight(raw) * ((Fold(raw).latitude - anchor) / scale); },
            moment_flags_) /
        total;
    const double second = NativeExpectation(
                              lat, breaks,
                              [&](double raw) {
                                const double delta = (Fold(raw).latitude - anchor) / scale;
                                return weight(raw) * delta * delta;
                              },
                              moment_flags_) /
                          total;
    variances_.sigma2_lat = (second - first * first) * scale * scale;
  }
  variances_.sigma2_az = AngularVariance(d.azimuth_dist, unflipped_probability_, flip_probability_, moment_flags_);
  variances_.sigma2_roll = AngularVariance(d.roll_dist, unflipped_probability_, flip_probability_, moment_flags_);
}

bool AxisPoseDensity::ReadsAzimuth() const {
  return !Uniform(distribution_.azimuth_dist);
}

bool AxisPoseDensity::ReadsRoll() const {
  return !Uniform(distribution_.roll_dist);
}

AxisDensityValue AxisPoseDensity::AngularLogFactor(const Distribution& d, double angle, bool flipped) const {
  if (Uniform(d)) {
    return { 0.0, 0 };
  }
  if (!std::isfinite(angle)) {
    return Invalid(AxisDensityFlag::kInvalidPose);
  }
  angle = Wrap(angle - (flipped ? kPi : 0));
  if (d.type == DistributionType::kNoRandom) {
    return { Match(angle, d.center * kDeg) ? 0.0 : kNegInf, 0 };
  }
  auto result = Images(d, angle);
  result.value += std::log(kTurn);
  return result;
}

AxisDensityValue AxisPoseDensity::LatitudeLogFactor(double phi, bool flipped) const {
  if (!std::isfinite(phi) || std::abs(phi) > kHalfPi) {
    return Invalid(AxisDensityFlag::kInvalidPose);
  }
  const double cosine = std::cos(phi);
  if (cosine <= kRoundoff) {
    return Invalid(AxisDensityFlag::kUndefinedPole);
  }
  const auto& d = distribution_.latitude_dist;
  if (d.type == DistributionType::kNoRandom) {
    return { !flipped && std::abs(phi - d.center * kDeg) <= kRoundoff ? 0.0 : kNegInf, 0 };
  }
  auto result = Images(d, flipped ? kPi - phi : phi);
  result.flags |= moment_flags_ & Flag(AxisDensityFlag::kQuadratureAccuracy);
  if (d.type == DistributionType::kGaussianLegacy && cosine < std::sqrt(kEps)) {
    result.flags |= Flag(AxisDensityFlag::kNearPole);
  }
  result.value +=
      std::log(2.0) - (d.type == DistributionType::kGaussianLegacy ? std::log(cosine) : std::log(normalization_));
  return result;
}

AxisDensityValue AxisPoseDensity::AzimuthLogFactor(double az, bool flipped) const {
  return AngularLogFactor(distribution_.azimuth_dist, az, flipped);
}

AxisDensityValue AxisPoseDensity::RollLogFactor(double roll, bool flipped) const {
  return AngularLogFactor(distribution_.roll_dist, roll, flipped);
}

AxisDensityValue AxisPoseDensity::LatitudeFactor(double phi, bool flipped) const {
  return Exp(LatitudeLogFactor(phi, flipped));
}

AxisDensityValue AxisPoseDensity::AzimuthFactor(double az, bool flipped) const {
  return Exp(AzimuthLogFactor(az, flipped));
}

AxisDensityValue AxisPoseDensity::RollFactor(double roll, bool flipped) const {
  return Exp(RollLogFactor(roll, flipped));
}

AxisDensityValue AxisPoseDensity::LogDensityAngles(double az, double phi, double roll) const {
  if (!std::isfinite(phi) || std::abs(phi) > kHalfPi || (ReadsAzimuth() && !std::isfinite(az)) ||
      (ReadsRoll() && !std::isfinite(roll))) {
    return Invalid(AxisDensityFlag::kInvalidPose);
  }
  if (std::cos(phi) <= kRoundoff) {
    return Invalid(AxisDensityFlag::kUndefinedPole);
  }
  AxisDensityValue result{ kNegInf, moment_flags_ & Flag(AxisDensityFlag::kQuadratureAccuracy) };
  for (bool flip : { false, true }) {
    const auto lat = LatitudeLogFactor(phi, flip);
    const auto a = AzimuthLogFactor(az, flip);
    const auto b = RollLogFactor(roll, flip);
    result.flags |= lat.flags | a.flags | b.flags;
    // A support constraint takes priority over a singular cusp on an absent leaf (0*infinity).
    const double term =
        lat.value == kNegInf || a.value == kNegInf || b.value == kNegInf ? kNegInf : lat.value + a.value + b.value;
    result.value = LogAdd(result.value, term);
  }
  if (std::isfinite(phi) && std::cos(phi) < std::sqrt(kEps) && (ReadsAzimuth() || ReadsRoll())) {
    result.flags |= Flag(AxisDensityFlag::kNearPole);
  }
  return result;
}

AxisDensityValue AxisPoseDensity::EvaluateAngles(double az, double phi, double roll) const {
  return Exp(LogDensityAngles(az, phi, roll));
}

AxisDensityValue AxisPoseDensity::LogDensity(const std::array<double, 9>& r) const {
  for (double x : r) {
    if (!std::isfinite(x)) {
      return Invalid(AxisDensityFlag::kInvalidPose);
    }
  }
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j <= i; ++j) {
      double dot = 0;
      for (int k = 0; k < 3; ++k) {
        dot += r[3 * i + k] * r[3 * j + k];
      }
      if (std::abs(dot - (i == j ? 1.0 : 0.0)) > kRoundoff) {
        return Invalid(AxisDensityFlag::kInvalidPose);
      }
    }
  }
  const double det =
      r[0] * (r[4] * r[8] - r[5] * r[7]) - r[1] * (r[3] * r[8] - r[5] * r[6]) + r[2] * (r[3] * r[7] - r[4] * r[6]);
  if (std::abs(det - 1) > kRoundoff) {
    return Invalid(AxisDensityFlag::kInvalidPose);
  }
  const double sine = std::hypot(r[6], r[7]);
  if (sine <= kRoundoff) {
    return Invalid(AxisDensityFlag::kUndefinedPole);
  }
  // R02=sin(theta)*cos(az), R12=sin(theta)*sin(az).
  return LogDensityAngles(ReadsAzimuth() ? std::atan2(r[5], r[2]) : 0, std::atan2(r[8], sine),
                          ReadsRoll() ? std::atan2(-r[7], r[6]) : 0);
}

AxisDensityValue AxisPoseDensity::Evaluate(const std::array<double, 9>& r) const {
  return Exp(LogDensity(r));
}

AxisDensityValue AxisPoseDensity::LogDensityThirdRow(double e1, double e2, double e3) const {
  if (ReadsAzimuth()) {
    return Invalid(AxisDensityFlag::kAzimuthRequired);
  }
  if (!std::isfinite(e1) || !std::isfinite(e2) || !std::isfinite(e3) ||
      std::abs(e1 * e1 + e2 * e2 + e3 * e3 - 1) > kRoundoff) {
    return Invalid(AxisDensityFlag::kInvalidPose);
  }
  const double sine = std::hypot(e1, e2);
  if (sine <= kRoundoff) {
    return Invalid(AxisDensityFlag::kUndefinedPole);
  }
  return LogDensityAngles(0, std::atan2(e3, sine), ReadsRoll() ? std::atan2(-e2, e1) : 0);
}

AxisDensityValue AxisPoseDensity::EvaluateThirdRow(double e1, double e2, double e3) const {
  return Exp(LogDensityThirdRow(e1, e2, e3));
}

AxisDensityWindow AxisPoseDensity::Window(int axis) const {
  const Distribution& d = axis == 0 ? distribution_.latitude_dist :
                          axis == 1 ? distribution_.azimuth_dist :
                                      distribution_.roll_dist;
  AxisDensityWindow result;
  result.flags = moment_flags_ & Flag(AxisDensityFlag::kQuadratureAccuracy);
  if (d.type == DistributionType::kZigzag || (axis == 0 && d.type == DistributionType::kGaussianLegacy)) {
    result.flags |= Flag(AxisDensityFlag::kUnboundedPeak);
    return result;
  }
  const double lo = axis == 0 ? -kHalfPi : -kPi;
  const double hi = -lo;
  if (d.type == DistributionType::kNoRandom) {
    if (axis == 0 || unflipped_probability_ > 0) {
      const double point = axis == 0 ? d.center * kDeg : Wrap(d.center * kDeg);
      result.intervals.push_back({ point, point });
    }
    if (axis != 0 && flip_probability_ > 0) {
      const double shifted = Wrap(d.center * kDeg + kPi);
      result.intervals.push_back({ shifted, shifted });
    }
    if (axis == 0 && std::abs(d.center) == 90) {
      result.flags |= Flag(AxisDensityFlag::kUndefinedPole);
    }
    return result;
  }
  auto log_marginal = [&](double x) {
    if (axis == 0) {
      // Use the continuous pole limit for finite-peak area laws, not the chart evaluator.
      return LogAdd(Images(d, x).value, Images(d, kPi - x).value) + std::log(2 / normalization_);
    }
    const auto a = AngularLogFactor(d, x, false);
    const auto b = AngularLogFactor(d, x, true);
    return LogAdd(unflipped_probability_ > 0 ? a.value + std::log(unflipped_probability_) : kNegInf,
                  flip_probability_ > 0 ? b.value + std::log(flip_probability_) : kNegInf);
  };
  if (d.type == DistributionType::kUniform) {
    // Piecewise-constant laws need exact support partitions: an arbitrarily short gap can
    // have no mesh node and no sampled extremum. No iterative root search is needed here.
    std::vector<double> edges{ lo, hi };
    for (double sign : { -1.0, 1.0 }) {
      const double raw = (static_cast<double>(d.center) + sign * static_cast<double>(d.spread) / 2) * kDeg;
      if (axis == 0) {
        edges.push_back(Fold(raw).latitude);
      } else {
        edges.push_back(Wrap(raw));
        edges.push_back(Wrap(raw + kPi));
      }
    }
    std::sort(edges.begin(), edges.end());
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
    std::vector<double> levels;
    double peak = kNegInf;
    for (size_t i = 1; i < edges.size(); ++i) {
      levels.push_back(log_marginal(edges[i - 1] + (edges[i] - edges[i - 1]) / 2));
      peak = std::max(peak, levels.back());
    }
    for (size_t i = 1; i < edges.size(); ++i) {
      if (levels[i - 1] > peak - 72) {
        if (!result.intervals.empty() && result.intervals.back()[1] == edges[i - 1]) {
          result.intervals.back()[1] = edges[i];
        } else {
          result.intervals.push_back({ edges[i - 1], edges[i] });
        }
      }
    }
    return result;
  }
  const int n = static_cast<int>(std::ceil((hi - lo) / std::min(d.spread * kDeg / 8, (hi - lo) / 256)));
  std::vector<double> mesh;
  mesh.reserve(n + 1);
  double peak = kNegInf;
  for (int i = 0; i <= n; ++i) {
    mesh.push_back(lo + (hi - lo) * i / n);
    peak = std::max(peak, log_marginal(mesh.back()));
  }
  std::vector<double> values;
  values.reserve(n + 1);
  for (double x : mesh) {
    values.push_back(log_marginal(x));
  }
  // Retain extrema as crossing nodes: a component can be arbitrarily shorter than the mesh
  // when its local peak barely exceeds the global threshold. Minima likewise retain tiny gaps.
  std::vector<double> extrema;
  auto refine = [&](double a, double b, bool maximum) {
    for (int j = 0; j < 96 && b - a > kRoundoff; ++j) {
      const double x = a + (b - a) / 3;
      const double y = b - (b - a) / 3;
      if ((log_marginal(x) < log_marginal(y)) == maximum) {
        a = x;
      } else {
        b = y;
      }
    }
    const double x = (a + b) / 2;
    peak = std::max(peak, log_marginal(x));
    extrema.push_back(axis == 0 ? x : Wrap(x));
  };
  for (int i = 1; i < n; ++i) {
    if (values[i] > values[i - 1] && values[i] >= values[i + 1]) {
      refine(mesh[i - 1], mesh[i + 1], true);
    } else if (values[i] < values[i - 1] && values[i] <= values[i + 1]) {
      refine(mesh[i - 1], mesh[i + 1], false);
    }
  }
  // Boundary brackets are necessary even if the endpoint itself beats its nearest node.
  // Angular brackets cross the periodic seam; latitude uses one-sided domain brackets.
  if (axis == 0) {
    refine(mesh[0], mesh[1], true);
    refine(mesh[n - 1], mesh[n], true);
  } else {
    refine(mesh[n - 1], mesh[1] + kTurn, true);
    refine(mesh[n - 1], mesh[1] + kTurn, false);
  }
  mesh.insert(mesh.end(), extrema.begin(), extrema.end());
  std::sort(mesh.begin(), mesh.end());
  mesh.erase(std::unique(mesh.begin(), mesh.end()), mesh.end());
  const double threshold = peak - 72;
  bool inside = log_marginal(lo) > threshold;
  double start = lo;
  for (size_t i = 1; i < mesh.size(); ++i) {
    const bool next_inside = log_marginal(mesh[i]) > threshold;
    if (inside != next_inside) {
      double a = mesh[i - 1];
      double b = mesh[i];
      for (int j = 0; j < 64 && b - a > kRoundoff; ++j) {
        const double mid = (a + b) / 2;
        if ((log_marginal(mid) > threshold) == inside) {
          a = mid;
        } else {
          b = mid;
        }
      }
      const double boundary = (a + b) / 2;
      if (inside) {
        result.intervals.push_back({ start, boundary });
      } else {
        start = boundary;
      }
    }
    inside = next_inside;
  }
  if (inside) {
    result.intervals.push_back({ start, hi });
  }
  return result;
}

AxisDensityWindow AxisPoseDensity::LatitudeWindow() const {
  return Window(0);
}

AxisDensityWindow AxisPoseDensity::AzimuthWindow() const {
  return Window(1);
}

AxisDensityWindow AxisPoseDensity::RollWindow() const {
  return Window(2);
}

}  // namespace lumice::analytic
