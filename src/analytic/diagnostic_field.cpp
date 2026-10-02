#include "analytic/diagnostic_field.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "analytic/path_chain.hpp"
#include "analytic/so3.hpp"

namespace lumice::analytic {

namespace {

constexpr double kPoseStep = 2.0e-4;
constexpr double kIndexRelativeStep = 1.0e-6;
constexpr double kTirCoefficientDerivativeGuard = 1.0e-4;

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

}  // namespace

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
  if (!valid) {
    return values;
  }
  for (int i = 0; i < 3; i++) {
    values.outgoing[i] = detail.outgoing_direction[i];
  }
  values.fresnel_weight = detail.fresnel_transmission;
  for (int k = 1; k < face_count - 1; k++) {
    const double* normal = normals_.normal[slots_[k]];
    const double* incoming = segments.data() + 3 * static_cast<size_t>(k);
    const double cosine = normal[0] * incoming[0] + normal[1] * incoming[1] + normal[2] * incoming[2];
    values.tir_margins[static_cast<size_t>(k - 1)] =
        1.0 - input.refractive_index * input.refractive_index * (1.0 - cosine * cosine);
  }

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
  Values minus[3];
  Values plus[3];
  bool direction_pose = true;
  for (int axis = 0; axis < 3; axis++) {
    DiagnosticRowInput lo = input;
    DiagnosticRowInput hi = input;
    PerturbPose(input.pose, axis, -kPoseStep, -1, 0.0, lo.pose);
    PerturbPose(input.pose, axis, kPoseStep, -1, 0.0, hi.pose);
    minus[axis] = EvaluateValues(lo);
    plus[axis] = EvaluateValues(hi);
    direction_pose = direction_pose && minus[axis].path_status == DiagnosticPathStatus::kOk &&
                     plus[axis].path_status == DiagnosticPathStatus::kOk && Finite(minus[axis].outgoing, 3) &&
                     Finite(plus[axis].outgoing, 3);
  }
  if (direction_pose) {
    out->direction_pose_jacobian_available = 1;
    for (int component = 0; component < 3; component++) {
      for (int axis = 0; axis < 3; axis++) {
        out->direction_pose_jacobian[3 * component + axis] =
            (plus[axis].outgoing[component] - minus[axis].outgoing[component]) / (2.0 * kPoseStep);
      }
    }
  }

  bool direction_hessian = direction_pose;
  for (int component = 0; component < 3; component++) {
    for (int axis = 0; axis < 3; axis++) {
      out->direction_pose_hessian[9 * component + 3 * axis + axis] =
          (plus[axis].outgoing[component] - 2.0 * base.outgoing[component] + minus[axis].outgoing[component]) /
          (kPoseStep * kPoseStep);
    }
  }
  for (int a = 0; a < 3; a++) {
    for (int b = a + 1; b < 3; b++) {
      Values corners[4];
      const double signs[4][2] = { { 1.0, 1.0 }, { 1.0, -1.0 }, { -1.0, 1.0 }, { -1.0, -1.0 } };
      for (int k = 0; k < 4; k++) {
        DiagnosticRowInput sample = input;
        PerturbPose(input.pose, a, signs[k][0] * kPoseStep, b, signs[k][1] * kPoseStep, sample.pose);
        corners[k] = EvaluateValues(sample);
        direction_hessian =
            direction_hessian && corners[k].path_status == DiagnosticPathStatus::kOk && Finite(corners[k].outgoing, 3);
      }
      for (int component = 0; component < 3; component++) {
        const double mixed = (corners[0].outgoing[component] - corners[1].outgoing[component] -
                              corners[2].outgoing[component] + corners[3].outgoing[component]) /
                             (4.0 * kPoseStep * kPoseStep);
        out->direction_pose_hessian[9 * component + 3 * a + b] = mixed;
        out->direction_pose_hessian[9 * component + 3 * b + a] = mixed;
      }
    }
  }
  if (direction_hessian && Finite(out->direction_pose_hessian, 27)) {
    out->direction_pose_hessian_available = 1;
  } else {
    std::fill(out->direction_pose_hessian, out->direction_pose_hessian + 27, 0.0);
  }

  auto fill_pose_scalar = [&](double centre, auto getter, int* available, double gradient[3], auto compatible) {
    bool ok = std::isfinite(centre);
    for (int axis = 0; axis < 3; axis++) {
      const double lo = getter(minus[axis]);
      const double hi = getter(plus[axis]);
      ok = ok && compatible(minus[axis], plus[axis]) && std::isfinite(lo) && std::isfinite(hi);
      gradient[axis] = (hi - lo) / (2.0 * kPoseStep);
    }
    if (ok && Finite(gradient, 3)) {
      *available = 1;
    } else {
      std::fill(gradient, gradient + 3, 0.0);
    }
  };
  auto path_compatible = [](const Values& lo, const Values& hi) {
    return lo.path_status == DiagnosticPathStatus::kOk && hi.path_status == DiagnosticPathStatus::kOk;
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
    auto coefficient_compatible = [i, &base](const Values& lo, const Values& hi) {
      if (lo.path_status != DiagnosticPathStatus::kOk || hi.path_status != DiagnosticPathStatus::kOk) {
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
    return lo.entry_status == DiagnosticEntryStatus::kOk && hi.entry_status == DiagnosticEntryStatus::kOk &&
           lo.entry_topology_signature == base.entry_topology_signature &&
           hi.entry_topology_signature == base.entry_topology_signature;
  };
  fill_pose_scalar(
      base.entry_measure, [](const Values& v) { return v.entry_measure; }, &out->entry_pose_gradient_available,
      out->entry_pose_gradient, entry_compatible);

  const double index_step = kIndexRelativeStep * std::max(1.0, std::fabs(input.refractive_index));
  if (!(input.refractive_index > index_step)) {
    return;
  }
  DiagnosticRowInput lo_input = input;
  DiagnosticRowInput hi_input = input;
  lo_input.refractive_index -= index_step;
  hi_input.refractive_index += index_step;
  const Values lo = EvaluateValues(lo_input);
  const Values hi = EvaluateValues(hi_input);
  const bool index_path = lo.path_status == DiagnosticPathStatus::kOk && hi.path_status == DiagnosticPathStatus::kOk;
  if (index_path && Finite(lo.outgoing, 3) && Finite(hi.outgoing, 3)) {
    out->direction_index_derivative_available = 1;
    for (int component = 0; component < 3; component++) {
      out->direction_index_derivative[component] =
          (hi.outgoing[component] - lo.outgoing[component]) / (2.0 * index_step);
    }
  }
  auto fill_index_scalar = [&](double low, double high, int* available, double* derivative, bool compatible) {
    if (compatible && std::isfinite(low) && std::isfinite(high)) {
      *derivative = (high - low) / (2.0 * index_step);
      *available = std::isfinite(*derivative) ? 1 : 0;
    }
  };
  for (size_t i = 0; i < out->domain_margins.size(); i++) {
    fill_index_scalar(lo.domain_margins[i], hi.domain_margins[i], &out->domain_margins[i].index_derivative_available,
                      &out->domain_margins[i].index_derivative, index_path);
  }
  for (size_t i = 0; i < out->tir_margins.size(); i++) {
    fill_index_scalar(lo.tir_margins[i], hi.tir_margins[i], &out->tir_margins[i].index_derivative_available,
                      &out->tir_margins[i].index_derivative, index_path);
  }
  for (size_t i = 0; i < out->interfaces.size(); i++) {
    const bool same_side =
        i == 0 || i + 1 == out->interfaces.size() ||
        IsSmoothTirCoefficientSample(base.tir_margins[i - 1], lo.tir_margins[i - 1], hi.tir_margins[i - 1]);
    fill_index_scalar(lo.coefficients[i], hi.coefficients[i], &out->interfaces[i].index_derivative_available,
                      &out->interfaces[i].index_derivative, index_path && same_side);
  }
  const bool entry_index = lo.entry_status == DiagnosticEntryStatus::kOk &&
                           hi.entry_status == DiagnosticEntryStatus::kOk &&
                           lo.entry_topology_signature == base.entry_topology_signature &&
                           hi.entry_topology_signature == base.entry_topology_signature;
  fill_index_scalar(lo.entry_measure, hi.entry_measure, &out->entry_index_derivative_available,
                    &out->entry_index_derivative, entry_index);
}

}  // namespace lumice::analytic
