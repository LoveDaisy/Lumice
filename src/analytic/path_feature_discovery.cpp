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

bool FieldWorkBudget::Consume(uint64_t count) {
  if (exhausted || count > max_component_evaluations - std::min(component_evaluations, max_component_evaluations) ||
      std::chrono::steady_clock::now() >= deadline) {
    exhausted = true;
    return false;
  }
  component_evaluations += count;
  return true;
}

bool EvaluateSphericalField(const std::vector<WeightedSkySample>& samples, const SphericalFieldQuery& query,
                            SphericalFieldValue* out, FieldWorkBudget* budget) {
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
  size_t processed = 0;
  for (const auto& sample : samples) {
    if (budget && processed % 256 == 0 && !budget->Consume(std::min<size_t>(256, samples.size() - processed))) {
      return false;
    }
    ++processed;
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

FieldStationaryPoint CorrectSphericalField(const std::vector<WeightedSkySample>& samples,
                                           const std::array<double, 3>& seed, const FieldSolveOptions& options,
                                           FieldWorkBudget* budget) {
  FieldStationaryPoint result;
  if (!ValidateUnitVector(seed.data()) || !(options.bandwidth_rad > 0) || !std::isfinite(options.bandwidth_rad) ||
      !(options.tolerance_rad > 0) || !std::isfinite(options.tolerance_rad) || !(options.max_step_rad > 0) ||
      !std::isfinite(options.max_step_rad) || options.max_step_rad >= kPi || options.max_iterations <= 0 ||
      !std::isfinite(options.level)) {
    return result;
  }
  auto q = seed;
  for (int it = 0; it < options.max_iterations; ++it) {
    result.query.direction = q;
    result.query.bandwidth_rad = options.bandwidth_rad;
    double basis[2][3];
    so3::TangentBasis(q.data(), basis);
    for (int i = 0; i < 2; ++i) {
      std::copy_n(basis[i], 3, result.query.basis[i].begin());
    }
    result.iterations = it + 1;
    if (!EvaluateSphericalField(samples, result.query, &result.field, budget)) {
      result.status = budget && budget->exhausted ? FieldSolveStatus::kBudgetExceeded : FieldSolveStatus::kInvalidInput;
      return result;
    }
    const auto& y = result.field.xyz[1];
    if (!(y.value > 0)) {
      result.status = FieldSolveStatus::kNoSignal;
      return result;
    }
    const double g0 = y.gradient[0] / y.value;
    const double g1 = y.gradient[1] / y.value;
    const double h00 = y.hessian[0] / y.value - g0 * g0;
    const double h01 = y.hessian[1] / y.value - g0 * g1;
    const double h11 = y.hessian[2] / y.value - g1 * g1;
    const double angle = .5 * std::atan2(2 * h01, h00 - h11);
    result.normal = { -std::sin(angle), std::cos(angle) };
    const double mid = .5 * (h00 + h11);
    const double radius = std::hypot(.5 * (h00 - h11), h01);
    result.log_y_curvatures = { mid - radius, mid + radius };
    std::array<double, 2> delta{};
    switch (options.equation) {
      case FieldEquation::kLogYPeak: {
        const double det = h00 * h11 - h01 * h01;
        if (!(result.log_y_curvatures[1] < 0) || !(det > 0)) {
          result.status = FieldSolveStatus::kDegenerate;
          return result;
        }
        delta = { (-h11 * g0 + h01 * g1) / det, (h01 * g0 - h00 * g1) / det };
        break;
      }
      case FieldEquation::kLogYRidge: {
        if (!(result.log_y_curvatures[0] < 0) || !(radius > 0)) {
          result.status = FieldSolveStatus::kDegenerate;
          return result;
        }
        const double step = -(g0 * result.normal[0] + g1 * result.normal[1]) / result.log_y_curvatures[0];
        delta = { step * result.normal[0], step * result.normal[1] };
        break;
      }
      case FieldEquation::kChromaticityX:
      case FieldEquation::kChromaticityY: {
        const auto& jet = result.field.xy[options.equation == FieldEquation::kChromaticityX ? 0 : 1];
        const double norm = std::hypot(jet.gradient[0], jet.gradient[1]);
        if (!(norm > 0) || !result.field.chromaticity_available) {
          result.status = FieldSolveStatus::kDegenerate;
          return result;
        }
        result.normal = { jet.gradient[0] / norm, jet.gradient[1] / norm };
        const double step = -(jet.value - options.level) / norm;
        delta = { step * result.normal[0], step * result.normal[1] };
        break;
      }
      default:
        return result;
    }
    const double step = std::hypot(delta[0], delta[1]);
    result.correction_rad = step;
    if (!std::isfinite(step)) {
      result.status = FieldSolveStatus::kDegenerate;
      return result;
    }
    if (step <= options.tolerance_rad) {
      result.status = FieldSolveStatus::kConverged;
      return result;
    }
    const double advance = std::min(step, options.max_step_rad);
    for (int j = 0; j < 3; ++j) {
      const double tangent = (delta[0] * basis[0][j] + delta[1] * basis[1][j]) / step;
      q[j] = std::cos(advance) * result.query.direction[j] + std::sin(advance) * tangent;
    }
    const double length = so3::Norm3(q.data());
    for (auto& x : q) {
      x /= length;
    }
    result.travelled_rad += advance;
  }
  // The published query/jet is the last EVALUATED point, not an unevaluated
  // Newton proposal; an iteration-limited point is never marked converged.
  result.status = FieldSolveStatus::kIterationLimit;
  return result;
}

FieldCurve TraceSphericalField(const std::vector<WeightedSkySample>& samples, const std::array<double, 3>& seed,
                               const FieldWalkOptions& options, bool reverse, FieldWorkBudget* budget) {
  FieldCurve result;
  if (!(options.step_rad > 0) || !std::isfinite(options.step_rad) || options.step_rad >= kPi ||
      !(options.minimum_y >= 0) || !std::isfinite(options.minimum_y) || options.max_points <= 0 ||
      options.corrector.equation == FieldEquation::kLogYPeak) {
    return result;
  }
  auto point = CorrectSphericalField(samples, seed, options.corrector, budget);
  std::array<double, 3> previous_tangent{};
  std::array<double, 3> first_tangent{};
  for (int i = 0; i < options.max_points; ++i) {
    if (point.status != FieldSolveStatus::kConverged) {
      result.terminal = point;
      result.stop = FieldWalkStop::kCorrectorFailed;
      return result;
    }
    if (point.field.xyz[1].value < options.minimum_y) {
      result.terminal = point;
      result.stop = FieldWalkStop::kObservationCensored;
      return result;
    }
    std::array<double, 3> tangent{};
    for (int j = 0; j < 3; ++j) {
      tangent[j] = -point.normal[1] * point.query.basis[0][j] + point.normal[0] * point.query.basis[1][j];
    }
    if ((i == 0 && reverse) || (i > 0 && so3::Dot3(tangent.data(), previous_tangent.data()) < 0)) {
      for (auto& x : tangent) {
        x = -x;
      }
    }
    if (i == 0) {
      first_tangent = tangent;
    } else if (i > 4) {
      const auto& first = result.points.front().query.direction;
      double cross[3];
      so3::Cross3(first.data(), point.query.direction.data(), cross);
      const double distance = std::atan2(so3::Norm3(cross), so3::Dot3(first.data(), point.query.direction.data()));
      if (distance < .75 * options.step_rad && so3::Dot3(tangent.data(), first_tangent.data()) > 0) {
        result.points.push_back(point);
        result.stop = FieldWalkStop::kClosed;
        return result;
      }
    }
    result.points.push_back(point);
    if (i + 1 == options.max_points) {
      break;
    }
    std::array<double, 3> next{};
    for (int j = 0; j < 3; ++j) {
      next[j] = std::cos(options.step_rad) * point.query.direction[j] + std::sin(options.step_rad) * tangent[j];
    }
    previous_tangent = tangent;
    point = CorrectSphericalField(samples, next, options.corrector, budget);
    // A large correction may land on another branch. Keep the trial, but do not
    // connect it to this curve just because both trials solved their equations.
    if (point.travelled_rad > options.step_rad) {
      result.terminal = point;
      result.stop = FieldWalkStop::kCorrectorFailed;
      return result;
    }
  }
  result.stop = FieldWalkStop::kPointLimit;
  return result;
}

}  // namespace lumice::analytic
