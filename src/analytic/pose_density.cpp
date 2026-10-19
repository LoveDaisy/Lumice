#include "analytic/pose_density.hpp"

#include <cmath>
#include <vector>

#include "core/math.hpp"

namespace lumice::analytic {

namespace {

bool PositiveWidth(double v) {
  return std::isfinite(v) && v > 0.0;
}

// The shared 400-point rule, computed once.
struct Rule {
  std::vector<double> x;
  std::vector<double> w;
  Rule() : x(kPoseDensityQuadratureNodes), w(kPoseDensityQuadratureNodes) {
    GaussLegendre(kPoseDensityQuadratureNodes, x.data(), w.data());
  }
};

const Rule& SharedRule() {
  static const Rule rule;
  return rule;
}

template <class F>
double Integrate(double lower, double upper, const F& f) {
  const Rule& rule = SharedRule();
  const double half = 0.5 * (upper - lower);
  const double mid = 0.5 * (upper + lower);
  double sum = 0.0;
  for (int i = 0; i < kPoseDensityQuadratureNodes; i++) {
    sum += rule.w[i] * f(half * rule.x[i] + mid);
  }
  return half * sum;
}

}  // namespace

const char* PoseDensityError(const PoseDensitySpec& spec) {
  switch (spec.family) {
    case PoseFamily::kRandom:
      if (spec.zenith_mean_deg != 0.0 || spec.zenith_std_deg != 0.0 || spec.roll_mean_deg != 0.0 ||
          spec.roll_std_deg != 0.0) {
        return "the random pose density takes no parameter: every field must be 0";
      }
      return "";
    case PoseFamily::kColumn:
    case PoseFamily::kPlate:
      if (spec.roll_mean_deg != 0.0 || spec.roll_std_deg != 0.0) {
        return "a column or plate pose density takes no roll: roll_mean_deg and roll_std_deg must be 0";
      }
      break;
    case PoseFamily::kParry:
    case PoseFamily::kLowitz:
      if (!std::isfinite(spec.roll_mean_deg)) {
        return "roll_mean_deg must be finite";
      }
      if (!PositiveWidth(spec.roll_std_deg)) {
        return "roll_std_deg must be finite and > 0";
      }
      break;
    default:
      return "unknown pose density family";
  }
  if (!(std::isfinite(spec.zenith_mean_deg) && spec.zenith_mean_deg >= 0.0 && spec.zenith_mean_deg <= 180.0)) {
    return "zenith_mean_deg must be finite and in [0, 180]";
  }
  if (!PositiveWidth(spec.zenith_std_deg)) {
    return "zenith_std_deg must be finite and > 0";
  }
  return "";
}

void GaussLegendre(int nodes, double* x, double* w) {
  const int n = nodes;
  for (int i = 0; i < (n + 1) / 2; i++) {
    // Tricomi's initial guess, then Newton on P_n.
    double z = std::cos(math::kPiD * (i + 0.75) / (n + 0.5));
    double dp = 0.0;
    for (int iteration = 0; iteration < 100; iteration++) {
      double p0 = 1.0;
      double p1 = z;
      for (int k = 2; k <= n; k++) {
        const double p2 = ((2.0 * k - 1.0) * z * p1 - (k - 1.0) * p0) / k;
        p0 = p1;
        p1 = p2;
      }
      const double pn = n == 1 ? z : p1;
      const double pn1 = n == 1 ? 1.0 : p0;
      dp = n * (z * pn - pn1) / (z * z - 1.0);
      const double step = pn / dp;
      z -= step;
      if (std::fabs(step) <= 1e-16) {
        break;
      }
    }
    // One more derivative at the converged node for the weight.
    double p0 = 1.0;
    double p1 = z;
    for (int k = 2; k <= n; k++) {
      const double p2 = ((2.0 * k - 1.0) * z * p1 - (k - 1.0) * p0) / k;
      p0 = p1;
      p1 = p2;
    }
    dp = n * (z * p1 - p0) / (z * z - 1.0);
    // Ascending order, as numpy's leggauss returns them.
    x[i] = -z;
    x[n - 1 - i] = z;
    w[i] = w[n - 1 - i] = 2.0 / ((1.0 - z * z) * dp * dp);
  }
}

PoseDensity::PoseDensity(const PoseDensitySpec& spec) : family_(spec.family) {
  if (family_ == PoseFamily::kRandom) {
    return;
  }
  zenith_mean_ = spec.zenith_mean_deg * math::kDegreeToRadD;
  zenith_std_ = spec.zenith_std_deg * math::kDegreeToRadD;
  double lower = 0.0;
  double upper = 0.0;
  ZenithWindow(&lower, &upper);
  zenith_integral_ = Integrate(lower, upper, [this](double theta) {
    const double d = theta - zenith_mean_;
    return std::exp(-(d * d) / (2.0 * zenith_std_ * zenith_std_)) * std::sin(theta);
  });
  if (!ReadsRoll()) {
    return;
  }
  roll_mean_ = spec.roll_mean_deg * math::kDegreeToRadD;
  roll_std_ = spec.roll_std_deg * math::kDegreeToRadD;
  const double r_lower = std::fmax(roll_mean_ - math::kPiD, roll_mean_ - kPoseDensityWindowSigmas * roll_std_);
  const double r_upper = std::fmin(roll_mean_ + math::kPiD, roll_mean_ + kPoseDensityWindowSigmas * roll_std_);
  // h over the window; every node lies in the one period about the mean, so the fold is the identity
  // there, and RollFactor's fold is applied anyway to keep one expression for h.
  roll_integral_ = Integrate(r_lower, r_upper, [this](double psi) {
    double offset = std::fmod(psi - roll_mean_ + math::kPiD, 2.0 * math::kPiD);
    if (offset < 0.0) {
      offset += 2.0 * math::kPiD;
    }
    offset -= math::kPiD;
    return std::exp(-(offset * offset) / (2.0 * roll_std_ * roll_std_));
  });
}

void PoseDensity::ZenithWindow(double* lower, double* upper) const {
  if (family_ == PoseFamily::kRandom) {
    *lower = 0.0;
    *upper = math::kPiD;
    return;
  }
  *lower = std::fmax(0.0, zenith_mean_ - kPoseDensityWindowSigmas * zenith_std_);
  *upper = std::fmin(math::kPiD, zenith_mean_ + kPoseDensityWindowSigmas * zenith_std_);
}

double PoseDensity::ZenithFactor(double theta) const {
  const double d = theta - zenith_mean_;
  return 2.0 * std::exp(-(d * d) / (2.0 * zenith_std_ * zenith_std_)) / zenith_integral_;
}

double PoseDensity::RollFactor(double psi) const {
  // Floored modulo into [0, 2 pi), as numpy's %.
  double offset = std::fmod(psi - roll_mean_ + math::kPiD, 2.0 * math::kPiD);
  if (offset < 0.0) {
    offset += 2.0 * math::kPiD;
  }
  offset -= math::kPiD;
  return 2.0 * math::kPiD * std::exp(-(offset * offset) / (2.0 * roll_std_ * roll_std_)) / roll_integral_;
}

double PoseDensity::Evaluate(double e1, double e2, double e3) const {
  if (family_ == PoseFamily::kRandom) {
    return 1.0;
  }
  const double zenith = ZenithFactor(std::acos(math::Clamp1(e3)));
  if (!ReadsRoll()) {
    return zenith;
  }
  return zenith * RollFactor(std::atan2(-e2, e1));
}

}  // namespace lumice::analytic
