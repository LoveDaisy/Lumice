#include "analytic/path_feature_discovery.hpp"

#include <algorithm>
#include <cmath>

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
        !std::all_of(sample.xyz_weight.begin(), sample.xyz_weight.end(),
                     [](double x) { return std::isfinite(x) && x >= 0; })) {
      return false;
    }
    if (sample.sample_index != current_index) {
      effective.Add(group_y);
      group_y = 0;
      current_index = sample.sample_index;
    }
    const double cosine = std::clamp(so3::Dot3(sample.direction.data(), query.direction.data()), -1.0, 1.0);
    const double a = so3::Dot3(sample.direction.data(), query.basis[0].data());
    const double b = so3::Dot3(sample.direction.data(), query.basis[1].data());
    const double kernel = normalization * std::exp(kappa * (cosine - 1));
    for (int c = 0; c < 3; ++c) {
      const double w = sample.xyz_weight[c] * kernel;
      auto& jet = result.xyz[c];
      jet.value += w;
      jet.gradient[0] += w * kappa * a;
      jet.gradient[1] += w * kappa * b;
      jet.hessian[0] += w * (kappa * kappa * a * a - kappa * cosine);
      jet.hessian[1] += w * kappa * kappa * a * b;
      jet.hessian[2] += w * (kappa * kappa * b * b - kappa * cosine);
      if (c == 1) {
        group_y += w;
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
