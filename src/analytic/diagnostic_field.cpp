#include "analytic/diagnostic_field.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

#include "analytic/path_chain.hpp"
#include "analytic/so3.hpp"

namespace lumice::analytic {

namespace {

constexpr double kPoseFineStep = 2.0e-4;
constexpr double kIndexFineRelativeStep = 1.0e-6;
constexpr double kTirCoefficientDerivativeGuard = 1.0e-4;
constexpr double kDerivativeGateGuard = 1.0e-4;
constexpr double kPoseFirstAbsoluteError = 2.0e-6;
constexpr double kPoseFirstRelativeError = 2.0e-5;
constexpr double kPoseSecondAbsoluteError = 5.0e-5;
constexpr double kPoseSecondRelativeError = 2.0e-4;
constexpr double kIndexAbsoluteError = 2.0e-8;
constexpr double kIndexRelativeError = 2.0e-6;

double QuietNan() {
  return std::numeric_limits<double>::quiet_NaN();
}

bool Finite(const double* values, int count) {
  for (int i = 0; i < count; i++) {
    if (!std::isfinite(values[i])) {
      return false;
    }
  }
  return true;
}

DiagnosticPathStatus ToPathStatus(ChainFailure failure) {
  switch (failure) {
    case ChainFailure::kNone:
      return DiagnosticPathStatus::kOk;
    case ChainFailure::kPathInfeasible:
      return DiagnosticPathStatus::kPathInfeasible;
    case ChainFailure::kTirBoundary:
      return DiagnosticPathStatus::kRefractionCritical;
    case ChainFailure::kNonFinite:
      return DiagnosticPathStatus::kNonFinite;
  }
  return DiagnosticPathStatus::kNonFinite;
}

DiagnosticEntryStatus ToEntryStatus(EntryMeasureStatus status) {
  switch (status) {
    case EntryMeasureStatus::kOk:
      return DiagnosticEntryStatus::kOk;
    case EntryMeasureStatus::kEntryBackface:
      return DiagnosticEntryStatus::kEntryBackface;
    case EntryMeasureStatus::kExitCriticalAngle:
      return DiagnosticEntryStatus::kExitCriticalAngle;
    case EntryMeasureStatus::kCorridorEmpty:
      return DiagnosticEntryStatus::kCorridorEmpty;
  }
  return DiagnosticEntryStatus::kNotEvaluated;
}

void PerturbPose(const double pose[9], int axis0, double amount0, int axis1, double amount1, double out[9]) {
  double delta[3]{};
  delta[axis0] += amount0;
  if (axis1 >= 0) {
    delta[axis1] += amount1;
  }
  double increment[9];
  so3::Exp(delta, increment);
  so3::MatMul(pose, increment, out);
}

bool IsSmoothTirCoefficientSample(double centre, double low, double high) {
  if (!std::isfinite(centre) || !std::isfinite(low) || !std::isfinite(high)) {
    return false;
  }
  const bool same_side = (centre < 0.0) == (low < 0.0) && (centre < 0.0) == (high < 0.0);
  return same_side && std::fabs(centre) > kTirCoefficientDerivativeGuard &&
         std::fabs(low) > kTirCoefficientDerivativeGuard && std::fabs(high) > kTirCoefficientDerivativeGuard;
}

bool HasStablePathMargins(const std::vector<double>& margins) {
  return std::all_of(margins.begin(), margins.end(),
                     [](double value) { return std::isfinite(value) && value > kDerivativeGateGuard; });
}

}  // namespace

diagnostic_field_detail::CentralDifferenceEstimate diagnostic_field_detail::RichardsonEstimate(
    double coarse, double fine, double absolute_tolerance, double relative_tolerance) {
  CentralDifferenceEstimate out;
  if (!std::isfinite(coarse) || !std::isfinite(fine) || !(absolute_tolerance >= 0.0) || !(relative_tolerance >= 0.0)) {
    return out;
  }
  const double correction = (fine - coarse) / 3.0;
  out.value = fine + correction;
  out.error = std::fabs(correction);
  const double scale = std::max({ std::fabs(coarse), std::fabs(fine), std::fabs(out.value) });
  out.converged = std::isfinite(out.value) && std::isfinite(out.error) &&
                  out.error <= absolute_tolerance + relative_tolerance * scale;
  return out;
}

struct DiagnosticField::Values {
  DiagnosticPathStatus path_status = DiagnosticPathStatus::kNonFinite;
  DiagnosticEntryStatus entry_status = DiagnosticEntryStatus::kNotEvaluated;
  uint64_t entry_topology_signature = 0;
  double outgoing[3] = { QuietNan(), QuietNan(), QuietNan() };
  double entry_measure = QuietNan();
  double fresnel_weight = QuietNan();
  std::vector<double> coefficients;
  std::vector<double> domain_margins;
  std::vector<double> tir_margins;
};

DiagnosticField::DiagnosticField(const FaceNormalTable& normals, const FacePolygonTable& polygons, const int* faces,
                                 const int* slots, int face_count)
    : normals_(normals), faces_(faces, faces + face_count), slots_(slots, slots + face_count),
      corridor_(normals, polygons, slots, face_count) {}

DiagnosticField::Values DiagnosticField::EvaluateValues(const DiagnosticRowInput& input) {
  Values values;
  const int face_count = static_cast<int>(faces_.size());
  values.coefficients.assign(face_count, QuietNan());
  values.domain_margins.assign(BranchMarginCount(face_count), QuietNan());
  values.tir_margins.assign(std::max(0, face_count - 2), QuietNan());

  // Finite support is a distinct diagnostic: a direction-domain failure must not erase its
  // independently decidable entry state.
  double incident_body[3];
  for (int i = 0; i < 3; i++) {
    incident_body[i] = input.pose[0 * 3 + i] * input.incident_direction[0] +
                       input.pose[1 * 3 + i] * input.incident_direction[1] +
                       input.pose[2 * 3 + i] * input.incident_direction[2];
  }
  const EntryMeasure entry = corridor_.Evaluate(incident_body, input.refractive_index);
  values.entry_status = ToEntryStatus(entry.status);
  values.entry_topology_signature = entry.topology_signature;
  values.entry_measure = entry.status == EntryMeasureStatus::kOk ? entry.value : 0.0;

  std::vector<double> segments(3 * static_cast<size_t>(face_count + 1), QuietNan());
  PathOutputs detail{};
  detail.fresnel_transmission = QuietNan();
  detail.segment_directions = segments.data();
  detail.interface_transmittances = values.coefficients.data();
  ChainDomain domain;
  domain.margins = values.domain_margins.data();
  const bool valid =
      TracePathChain<double>(normals_, slots_.data(), face_count, input.refractive_index, input.incident_direction,
                             input.pose, detail.outgoing_direction, &detail, &domain);
  values.path_status = ToPathStatus(domain.failure);
  for (int k = 1; k < face_count - 1; k++) {
    const double* normal = normals_.normal[slots_[k]];
    const double* incoming = segments.data() + 3 * static_cast<size_t>(k);
    if (!Finite(incoming, 3)) {
      continue;
    }
    const double cosine = normal[0] * incoming[0] + normal[1] * incoming[1] + normal[2] * incoming[2];
    values.tir_margins[static_cast<size_t>(k - 1)] =
        1.0 - input.refractive_index * input.refractive_index * (1.0 - cosine * cosine);
  }
  if (!valid) {
    return values;
  }
  std::copy(detail.outgoing_direction, detail.outgoing_direction + 3, values.outgoing);
  values.fresnel_weight = detail.fresnel_transmission;
  return values;
}

DiagnosticFieldResult DiagnosticField::Evaluate(const DiagnosticRowInput& input) {
  const Values base = EvaluateValues(input);
  DiagnosticFieldResult out;
  out.path_status = base.path_status;
  out.entry_status = base.entry_status;
  out.entry_measure = std::isfinite(base.entry_measure) ? base.entry_measure : 0.0;
  out.fresnel_weight = std::isfinite(base.fresnel_weight) ? base.fresnel_weight : 0.0;
  if (base.path_status == DiagnosticPathStatus::kOk) {
    std::copy(base.outgoing, base.outgoing + 3, out.outgoing_direction);
  }

  const int face_count = static_cast<int>(faces_.size());
  out.interfaces.resize(face_count);
  for (int i = 0; i < face_count; i++) {
    DiagnosticInterface& interface = out.interfaces[static_cast<size_t>(i)];
    interface.face_number = faces_[static_cast<size_t>(i)];
    interface.kind = i == 0 ? DiagnosticInterfaceKind::kEntryTransmission :
                              (i + 1 == face_count ? DiagnosticInterfaceKind::kExitTransmission :
                                                     DiagnosticInterfaceKind::kInternalReflection);
    interface.coefficient = base.coefficients[static_cast<size_t>(i)];
  }
  out.domain_margins.resize(base.domain_margins.size());
  for (size_t i = 0; i < base.domain_margins.size(); i++) {
    DiagnosticMargin& margin = out.domain_margins[i];
    margin.name = BranchMarginName(static_cast<int>(i), face_count);
    margin.interface_index =
        i < 2 ? 0 : (i + 2 >= base.domain_margins.size() ? face_count - 1 : static_cast<int>(i - 1));
    margin.value = base.domain_margins[i];
  }
  out.tir_margins.resize(base.tir_margins.size());
  for (size_t i = 0; i < base.tir_margins.size(); i++) {
    DiagnosticMargin& margin = out.tir_margins[i];
    margin.name = "internal_" + std::to_string(i + 1) + "_tir_discriminant";
    margin.interface_index = static_cast<int>(i + 1);
    margin.value = base.tir_margins[i];
  }
  if (base.path_status == DiagnosticPathStatus::kOk) {
    FillDerivatives(input, base, &out);
  }
  return out;
}

void DiagnosticField::FillDerivatives(const DiagnosticRowInput& input, const Values& base, DiagnosticFieldResult* out) {
  constexpr int kLevelCount = 2;
  constexpr int kMixedPairCount = 3;
  constexpr int kMixedAxes[kMixedPairCount][2] = { { 0, 1 }, { 0, 2 }, { 1, 2 } };
  constexpr double kCornerSigns[4][2] = { { 1.0, 1.0 }, { 1.0, -1.0 }, { -1.0, 1.0 }, { -1.0, -1.0 } };
  struct PoseSamples {
    double step = 0.0;
    std::array<Values, 3> minus;
    std::array<Values, 3> plus;
    std::array<std::array<Values, 4>, kMixedPairCount> corners;
  };
  std::array<PoseSamples, kLevelCount> pose_samples;
  pose_samples[0].step = 2.0 * kPoseFineStep;
  pose_samples[1].step = kPoseFineStep;
  for (PoseSamples& level : pose_samples) {
    for (int axis = 0; axis < 3; axis++) {
      DiagnosticRowInput lo = input;
      DiagnosticRowInput hi = input;
      PerturbPose(input.pose, axis, -level.step, -1, 0.0, lo.pose);
      PerturbPose(input.pose, axis, level.step, -1, 0.0, hi.pose);
      level.minus[axis] = EvaluateValues(lo);
      level.plus[axis] = EvaluateValues(hi);
    }
    for (int pair = 0; pair < kMixedPairCount; pair++) {
      const int a = kMixedAxes[pair][0];
      const int b = kMixedAxes[pair][1];
      for (int corner = 0; corner < 4; corner++) {
        DiagnosticRowInput sample = input;
        PerturbPose(input.pose, a, kCornerSigns[corner][0] * level.step, b, kCornerSigns[corner][1] * level.step,
                    sample.pose);
        level.corners[pair][corner] = EvaluateValues(sample);
      }
    }
  }

  auto path_compatible = [&base](const Values& lo, const Values& hi) {
    return HasStablePathMargins(base.domain_margins) && HasStablePathMargins(lo.domain_margins) &&
           HasStablePathMargins(hi.domain_margins) && lo.path_status == DiagnosticPathStatus::kOk &&
           hi.path_status == DiagnosticPathStatus::kOk;
  };

  bool direction_pose = true;
  double jacobian[kLevelCount][9]{};
  for (int level = 0; level < kLevelCount; level++) {
    const PoseSamples& samples = pose_samples[level];
    for (int axis = 0; axis < 3; axis++) {
      direction_pose = direction_pose && path_compatible(samples.minus[axis], samples.plus[axis]) &&
                       Finite(samples.minus[axis].outgoing, 3) && Finite(samples.plus[axis].outgoing, 3);
      for (int component = 0; component < 3; component++) {
        jacobian[level][3 * component + axis] =
            (samples.plus[axis].outgoing[component] - samples.minus[axis].outgoing[component]) / (2.0 * samples.step);
      }
    }
  }
  for (int i = 0; i < 9; i++) {
    const auto estimate = diagnostic_field_detail::RichardsonEstimate(jacobian[0][i], jacobian[1][i],
                                                                      kPoseFirstAbsoluteError, kPoseFirstRelativeError);
    direction_pose = direction_pose && estimate.converged;
    out->direction_pose_jacobian[i] = estimate.value;
  }
  if (direction_pose) {
    out->direction_pose_jacobian_available = 1;
  } else {
    std::fill(out->direction_pose_jacobian, out->direction_pose_jacobian + 9, 0.0);
  }

  bool direction_hessian = true;
  double hessian[kLevelCount][27]{};
  for (int level = 0; level < kLevelCount; level++) {
    const PoseSamples& samples = pose_samples[level];
    for (int axis = 0; axis < 3; axis++) {
      direction_hessian = direction_hessian && path_compatible(samples.minus[axis], samples.plus[axis]) &&
                          Finite(samples.minus[axis].outgoing, 3) && Finite(samples.plus[axis].outgoing, 3);
      for (int component = 0; component < 3; component++) {
        hessian[level][9 * component + 3 * axis + axis] =
            (samples.plus[axis].outgoing[component] - 2.0 * base.outgoing[component] +
             samples.minus[axis].outgoing[component]) /
            (samples.step * samples.step);
      }
    }
    for (int pair = 0; pair < kMixedPairCount; pair++) {
      const int a = kMixedAxes[pair][0];
      const int b = kMixedAxes[pair][1];
      const auto& corners = samples.corners[pair];
      for (const Values& corner : corners) {
        direction_hessian = direction_hessian && HasStablePathMargins(base.domain_margins) &&
                            HasStablePathMargins(corner.domain_margins) &&
                            corner.path_status == DiagnosticPathStatus::kOk && Finite(corner.outgoing, 3);
      }
      for (int component = 0; component < 3; component++) {
        const double mixed = (corners[0].outgoing[component] - corners[1].outgoing[component] -
                              corners[2].outgoing[component] + corners[3].outgoing[component]) /
                             (4.0 * samples.step * samples.step);
        hessian[level][9 * component + 3 * a + b] = mixed;
        hessian[level][9 * component + 3 * b + a] = mixed;
      }
    }
  }
  for (int i = 0; i < 27; i++) {
    const auto estimate = diagnostic_field_detail::RichardsonEstimate(
        hessian[0][i], hessian[1][i], kPoseSecondAbsoluteError, kPoseSecondRelativeError);
    direction_hessian = direction_hessian && estimate.converged;
    out->direction_pose_hessian[i] = estimate.value;
  }
  if (direction_hessian) {
    out->direction_pose_hessian_available = 1;
  } else {
    std::fill(out->direction_pose_hessian, out->direction_pose_hessian + 27, 0.0);
  }

  auto fill_pose_scalar = [&](double centre, auto getter, int* available, double gradient[3], auto compatible) {
    bool ok = std::isfinite(centre);
    double derivatives[kLevelCount][3]{};
    for (int level = 0; level < kLevelCount; level++) {
      const PoseSamples& samples = pose_samples[level];
      for (int axis = 0; axis < 3; axis++) {
        const double lo = getter(samples.minus[axis]);
        const double hi = getter(samples.plus[axis]);
        ok = ok && compatible(samples.minus[axis], samples.plus[axis]) && std::isfinite(lo) && std::isfinite(hi);
        derivatives[level][axis] = (hi - lo) / (2.0 * samples.step);
      }
    }
    for (int axis = 0; axis < 3; axis++) {
      const auto estimate = diagnostic_field_detail::RichardsonEstimate(
          derivatives[0][axis], derivatives[1][axis], kPoseFirstAbsoluteError, kPoseFirstRelativeError);
      ok = ok && estimate.converged;
      gradient[axis] = estimate.value;
    }
    if (ok && Finite(gradient, 3)) {
      *available = 1;
    } else {
      std::fill(gradient, gradient + 3, 0.0);
    }
  };
  for (size_t i = 0; i < out->domain_margins.size(); i++) {
    fill_pose_scalar(
        base.domain_margins[i], [i](const Values& v) { return v.domain_margins[i]; },
        &out->domain_margins[i].pose_derivative_available, out->domain_margins[i].pose_gradient, path_compatible);
  }
  for (size_t i = 0; i < out->tir_margins.size(); i++) {
    fill_pose_scalar(
        base.tir_margins[i], [i](const Values& v) { return v.tir_margins[i]; },
        &out->tir_margins[i].pose_derivative_available, out->tir_margins[i].pose_gradient, path_compatible);
  }
  for (size_t i = 0; i < out->interfaces.size(); i++) {
    auto coefficient_compatible = [i, &base, &path_compatible](const Values& lo, const Values& hi) {
      if (!path_compatible(lo, hi)) {
        return false;
      }
      if (i == 0 || i + 1 == lo.coefficients.size()) {
        return true;
      }
      return IsSmoothTirCoefficientSample(base.tir_margins[i - 1], lo.tir_margins[i - 1], hi.tir_margins[i - 1]);
    };
    fill_pose_scalar(
        base.coefficients[i], [i](const Values& v) { return v.coefficients[i]; },
        &out->interfaces[i].pose_derivative_available, out->interfaces[i].pose_gradient, coefficient_compatible);
  }
  auto entry_compatible = [&](const Values& lo, const Values& hi) {
    return base.entry_status == DiagnosticEntryStatus::kOk && lo.entry_status == DiagnosticEntryStatus::kOk &&
           hi.entry_status == DiagnosticEntryStatus::kOk &&
           lo.entry_topology_signature == base.entry_topology_signature &&
           hi.entry_topology_signature == base.entry_topology_signature;
  };
  fill_pose_scalar(
      base.entry_measure, [](const Values& v) { return v.entry_measure; }, &out->entry_pose_gradient_available,
      out->entry_pose_gradient, entry_compatible);

  struct IndexSamples {
    double step = 0.0;
    Values minus;
    Values plus;
  };
  std::array<IndexSamples, kLevelCount> index_samples;
  index_samples[0].step = 2.0 * kIndexFineRelativeStep * std::max(1.0, std::fabs(input.refractive_index));
  index_samples[1].step = kIndexFineRelativeStep * std::max(1.0, std::fabs(input.refractive_index));
  if (!(input.refractive_index > index_samples[0].step)) {
    return;
  }
  for (IndexSamples& samples : index_samples) {
    DiagnosticRowInput lo_input = input;
    DiagnosticRowInput hi_input = input;
    lo_input.refractive_index -= samples.step;
    hi_input.refractive_index += samples.step;
    samples.minus = EvaluateValues(lo_input);
    samples.plus = EvaluateValues(hi_input);
  }
  bool index_path[kLevelCount]{};
  for (int level = 0; level < kLevelCount; level++) {
    index_path[level] = path_compatible(index_samples[level].minus, index_samples[level].plus);
  }
  bool direction_index = true;
  for (int component = 0; component < 3; component++) {
    double derivatives[kLevelCount]{};
    for (int level = 0; level < kLevelCount; level++) {
      const IndexSamples& samples = index_samples[level];
      direction_index =
          direction_index && index_path[level] && Finite(samples.minus.outgoing, 3) && Finite(samples.plus.outgoing, 3);
      derivatives[level] =
          (samples.plus.outgoing[component] - samples.minus.outgoing[component]) / (2.0 * samples.step);
    }
    const auto estimate = diagnostic_field_detail::RichardsonEstimate(derivatives[0], derivatives[1],
                                                                      kIndexAbsoluteError, kIndexRelativeError);
    direction_index = direction_index && estimate.converged;
    out->direction_index_derivative[component] = estimate.value;
  }
  if (direction_index) {
    out->direction_index_derivative_available = 1;
  } else {
    std::fill(out->direction_index_derivative, out->direction_index_derivative + 3, 0.0);
  }
  auto fill_index_scalar = [&](auto getter, int* available, double* derivative, auto compatible) {
    bool ok = true;
    double derivatives[kLevelCount]{};
    for (int level = 0; level < kLevelCount; level++) {
      const IndexSamples& samples = index_samples[level];
      const double low = getter(samples.minus);
      const double high = getter(samples.plus);
      ok = ok && compatible(samples.minus, samples.plus) && std::isfinite(low) && std::isfinite(high);
      derivatives[level] = (high - low) / (2.0 * samples.step);
    }
    const auto estimate = diagnostic_field_detail::RichardsonEstimate(derivatives[0], derivatives[1],
                                                                      kIndexAbsoluteError, kIndexRelativeError);
    ok = ok && estimate.converged;
    if (ok) {
      *derivative = estimate.value;
      *available = 1;
    } else {
      *derivative = 0.0;
    }
  };
  for (size_t i = 0; i < out->domain_margins.size(); i++) {
    fill_index_scalar([i](const Values& v) { return v.domain_margins[i]; },
                      &out->domain_margins[i].index_derivative_available, &out->domain_margins[i].index_derivative,
                      path_compatible);
  }
  for (size_t i = 0; i < out->tir_margins.size(); i++) {
    fill_index_scalar([i](const Values& v) { return v.tir_margins[i]; },
                      &out->tir_margins[i].index_derivative_available, &out->tir_margins[i].index_derivative,
                      path_compatible);
  }
  for (size_t i = 0; i < out->interfaces.size(); i++) {
    auto coefficient_compatible = [i, &base, &path_compatible](const Values& lo, const Values& hi) {
      if (!path_compatible(lo, hi)) {
        return false;
      }
      return i == 0 || i + 1 == lo.coefficients.size() ||
             IsSmoothTirCoefficientSample(base.tir_margins[i - 1], lo.tir_margins[i - 1], hi.tir_margins[i - 1]);
    };
    fill_index_scalar([i](const Values& v) { return v.coefficients[i]; },
                      &out->interfaces[i].index_derivative_available, &out->interfaces[i].index_derivative,
                      coefficient_compatible);
  }
  auto entry_index_compatible = [&base](const Values& lo, const Values& hi) {
    return base.entry_status == DiagnosticEntryStatus::kOk && lo.entry_status == DiagnosticEntryStatus::kOk &&
           hi.entry_status == DiagnosticEntryStatus::kOk &&
           lo.entry_topology_signature == base.entry_topology_signature &&
           hi.entry_topology_signature == base.entry_topology_signature;
  };
  fill_index_scalar([](const Values& v) { return v.entry_measure; }, &out->entry_index_derivative_available,
                    &out->entry_index_derivative, entry_index_compatible);
}

}  // namespace lumice::analytic
