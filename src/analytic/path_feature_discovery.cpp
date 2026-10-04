#include "analytic/path_feature_discovery.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "analytic/path_evaluation.hpp"
#include "analytic/so3.hpp"

namespace lumice::analytic {
namespace {

constexpr double kPi = 3.14159265358979323846;

SphericalJet DivideJet(const SphericalJet& a, const SphericalJet& b) {
  SphericalJet r;
  r.value = a.value / b.value;
  for (int i = 0; i < 2; ++i) {
    r.gradient[i] = (a.gradient[i] - r.value * b.gradient[i]) / b.value;
  }
  constexpr int kRow[] = { 0, 0, 1 };
  constexpr int kCol[] = { 0, 1, 1 };
  for (int i = 0; i < 3; ++i) {
    const int j = kRow[i];
    const int k = kCol[i];
    r.hessian[i] =
        (a.hessian[i] - r.value * b.hessian[i] - r.gradient[j] * b.gradient[k] - r.gradient[k] * b.gradient[j]) /
        b.value;
  }
  return r;
}

bool Finite(const SphericalJet& jet) {
  return std::isfinite(jet.value) &&
         std::all_of(jet.gradient.begin(), jet.gradient.end(), [](double x) { return std::isfinite(x); }) &&
         std::all_of(jet.hessian.begin(), jet.hessian.end(), [](double x) { return std::isfinite(x); });
}

// exp(-x)*I_n(x), n=0,1,2. The positive power series is well conditioned
// below 50; above it the decreasing asymptotic terms reach double precision
// before their least term. Scaling prevents overflow at narrow observation widths.
std::array<double, 3> ScaledBessel(double x) {
  std::array<double, 3> values{};
  for (int n = 0; n < 3; ++n) {
    double term = 1;
    double sum = 1;
    if (x < 50) {
      for (int j = 1; j <= n; ++j) {
        term *= x / (2 * j);
      }
      sum = term;
      for (int j = 1; j < 256; ++j) {
        term *= (x * x / 4) / (j * (j + n));
        sum += term;
        if (term <= sum * std::numeric_limits<double>::epsilon()) {
          break;
        }
      }
      values[n] = std::exp(-x) * sum;
    } else {
      for (int j = 1; j <= 32; ++j) {
        term *= ((2.0 * j - 1) * (2.0 * j - 1) - 4.0 * n * n) / (8 * x * j);
        sum += term;
        if (std::abs(term) <= std::abs(sum) * std::numeric_limits<double>::epsilon()) {
          break;
        }
      }
      values[n] = sum / std::sqrt(2 * kPi * x);
    }
  }
  return values;
}

SphericalJet KernelJet(const WeightedSkySample& sample, const SphericalFieldQuery& query, double kappa,
                       double normalization) {
  const double cosine = std::clamp(so3::Dot3(sample.direction.data(), query.direction.data()), -1.0, 1.0);
  const double a = so3::Dot3(sample.direction.data(), query.basis[0].data());
  const double b = so3::Dot3(sample.direction.data(), query.basis[1].data());
  SphericalJet jet;
  if (!sample.uniform_orbit_axis) {
    jet.value = normalization * std::exp(kappa * (cosine - 1));
    jet.gradient = { jet.value * kappa * a, jet.value * kappa * b };
    jet.hessian = { jet.value * (kappa * kappa * a * a - kappa * cosine), jet.value * kappa * kappa * a * b,
                    jet.value * (kappa * kappa * b * b - kappa * cosine) };
    return jet;
  }
  const auto& axis = *sample.uniform_orbit_axis;
  const double c = std::clamp(so3::Dot3(axis.data(), sample.direction.data()), -1.0, 1.0);
  const double z = std::clamp(so3::Dot3(axis.data(), query.direction.data()), -1.0, 1.0);
  const double radius = std::sqrt(std::max(0.0, (1 - c) * (1 + c)));
  const double radial_query = std::sqrt(std::max(0.0, (1 - z) * (1 + z)));
  const auto bessel = ScaledBessel(kappa * radius * radial_query);
  jet.value = normalization * std::exp(kappa * std::min(0.0, c * z + radius * radial_query - 1)) * bessel[0];
  const double first = bessel[1] / bessel[0];
  const double second = bessel[2] / bessel[0];
  std::array<double, 2> axial{};
  std::array<double, 2> radial{};
  for (int i = 0; i < 2; ++i) {
    axial[i] = so3::Dot3(axis.data(), query.basis[i].data());
    // At a pole I1=I2=0, so no choice of radial frame is necessary.
    if (radial_query > 0) {
      for (int j = 0; j < 3; ++j) {
        radial[i] += (query.direction[j] - z * axis[j]) * query.basis[i][j] / radial_query;
      }
    }
    jet.gradient[i] = jet.value * kappa * (c * axial[i] + radius * first * radial[i]);
  }
  constexpr int kRow[] = { 0, 0, 1 };
  constexpr int kCol[] = { 0, 1, 1 };
  const double mean_cosine = c * z + radius * first * radial_query;
  for (int k = 0; k < 3; ++k) {
    const int i = kRow[k];
    const int j = kCol[k];
    const double metric = i == j ? 1 : 0;
    // E[d_i*d_j] under the tilted circular measure. I2 controls its anisotropy;
    // -k*E[d.q]*g_ij is the sphere's covariant (not ambient) correction.
    const double moment =
        c * c * axial[i] * axial[j] + c * radius * first * (axial[i] * radial[j] + radial[i] * axial[j]) +
        radius * radius * (.5 * (1 - second) * (metric - axial[i] * axial[j]) + second * radial[i] * radial[j]);
    jet.hessian[k] = jet.value * (kappa * kappa * moment - kappa * mean_cosine * metric);
  }
  return jet;
}

// Scale before squaring so tiny physical weights still have an effective count.
struct EffectiveCount {
  double scale = 0;
  double sum = 0;
  double sum_squared = 0;
  void Add(double weight) {
    if (weight <= 0) {
      return;
    }
    if (weight > scale) {
      const double ratio = scale / weight;
      sum *= ratio;
      sum_squared *= ratio * ratio;
      scale = weight;
    }
    const double relative = weight / scale;
    sum += relative;
    sum_squared += relative * relative;
  }
  double Value() const { return sum_squared > 0 ? sum * sum / sum_squared : 0; }
};

}  // namespace

bool EvaluateSphericalField(const std::vector<WeightedSkySample>& samples, const SphericalFieldQuery& query,
                            SphericalFieldValue* out) {
  if (!out) {
    return false;
  }
  *out = {};
  const double kappa = 1 / (query.bandwidth_rad * query.bandwidth_rad);
  if (!(query.bandwidth_rad > 0) || !std::isfinite(query.bandwidth_rad) || !(kappa > 0) ||
      !std::isfinite(kappa * kappa) || !ValidateUnitVector(query.direction.data())) {
    return false;
  }
  for (const auto& axis : query.basis) {
    if (!ValidateUnitVector(axis.data()) || std::abs(so3::Dot3(axis.data(), query.direction.data())) > kUnitTolerance) {
      return false;
    }
  }
  if (std::abs(so3::Dot3(query.basis[0].data(), query.basis[1].data())) > kUnitTolerance) {
    return false;
  }
  const double normalization = kappa / (2 * kPi * -std::expm1(-2 * kappa));
  SphericalFieldValue result;
  EffectiveCount effective;
  uint64_t current_index = 0;
  double group_y = 0;
  for (const auto& sample : samples) {
    if (sample.sample_index < current_index || !ValidateUnitVector(sample.direction.data()) ||
        (sample.uniform_orbit_axis && !ValidateUnitVector(sample.uniform_orbit_axis->data())) ||
        !std::all_of(sample.xyz_weight.begin(), sample.xyz_weight.end(),
                     [](double x) { return std::isfinite(x) && x >= 0; })) {
      return false;
    }
    if (result.outer_sample_count == 0 || sample.sample_index != current_index) {
      ++result.outer_sample_count;
      effective.Add(group_y);
      group_y = 0;
      current_index = sample.sample_index;
    }
    const auto kernel = KernelJet(sample, query, kappa, normalization);
    for (int c = 0; c < 3; ++c) {
      const double w = sample.xyz_weight[c];
      auto& jet = result.xyz[c];
      jet.value += w * kernel.value;
      for (int i = 0; i < 2; ++i) {
        jet.gradient[i] += w * kernel.gradient[i];
      }
      for (int i = 0; i < 3; ++i) {
        jet.hessian[i] += w * kernel.hessian[i];
      }
      if (c == 1) {
        group_y += w * kernel.value;
      }
    }
  }
  effective.Add(group_y);
  result.effective_samples_y = effective.Value();
  SphericalJet total;
  for (const auto& jet : result.xyz) {
    total.value += jet.value;
    for (int i = 0; i < 2; ++i) {
      total.gradient[i] += jet.gradient[i];
    }
    for (int i = 0; i < 3; ++i) {
      total.hessian[i] += jet.hessian[i];
    }
  }
  result.chromaticity_available = total.value > 0;
  if (result.chromaticity_available) {
    result.xy[0] = DivideJet(result.xyz[0], total);
    result.xy[1] = DivideJet(result.xyz[1], total);
  }
  if (!Finite(total) || !std::isfinite(result.effective_samples_y) ||
      !std::all_of(result.xyz.begin(), result.xyz.end(), Finite) ||
      !std::all_of(result.xy.begin(), result.xy.end(), Finite)) {
    return false;
  }
  *out = result;
  return true;
}

}  // namespace lumice::analytic
