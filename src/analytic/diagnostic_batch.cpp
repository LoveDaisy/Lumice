#include "analytic/diagnostic_batch.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "analytic/so3.hpp"

namespace lumice::analytic {
namespace {

bool FiniteGradient(const std::array<double, 3>& value) {
  return std::all_of(value.begin(), value.end(), [](double x) { return std::isfinite(x); });
}

DiagnosticOutputRow EvaluateRow(const std::vector<int>& faces, const DiagnosticInputRow& row,
                                const DiagnosticBatchOptions& options) {
  DiagnosticOutputRow out;
  out.source_token = row.source_token;
  out.interfaces.resize(faces.size());
  if (!(row.refractive_index > 1) || !std::isfinite(row.refractive_index) || !ValidateRotation(row.pose.data()) ||
      !ValidateUnitVector(row.incident.data())) {
    return out;
  }
  FaceNormalTable normals;
  FacePolygonTable polygons;
  out.input_status = BuildFaceNormals(row.crystal, &normals, &polygons);
  if (out.input_status != Status::kOk) {
    return out;
  }
  std::vector<int> slots(faces.size());
  out.input_status = ResolveFaceSequence(normals, faces.data(), static_cast<int>(faces.size()), slots.data());
  if (out.input_status != Status::kOk) {
    return out;
  }
  const int count = static_cast<int>(slots.size());
  std::vector<double> segments(3 * (count + 1));
  std::vector<double> factors(count);
  double margins[kMaxFaceCount + 2]{};
  ChainDomain domain{ margins };
  ChainInterfaceDiagnostics<double> interfaces;
  PathOutputs detail{ {}, 0, segments.data(), factors.data() };
  ++out.path_evaluations;
  out.path_valid = TracePathChain(normals, slots.data(), count, row.refractive_index, row.incident.data(),
                                  row.pose.data(), out.outgoing.data(), &detail, &domain, &interfaces);
  out.optical_failure = domain.failure;
  for (int j = 0; j < interfaces.reached; ++j) {
    auto& face = out.interfaces[j];
    face.reached = true;
    face.incidence = interfaces.incidence[j];
    face.discriminant = interfaces.discriminant[j];
    // Only interfaces before the failed gate have had their factor computed.
    face.factor_available = out.path_valid || j + 1 < interfaces.reached;
    if (face.factor_available) {
      face.factor = factors[j];
    }
  }
  if (out.path_valid) {
    out.interface_product = detail.fresnel_transmission;
  }
  if (out.path_valid) {
    double incident_body[3];
    chain_detail::WorldToBody(row.pose.data(), row.incident.data(), incident_body);
    Corridor corridor(normals, polygons, slots.data(), count);
    out.entry =
        corridor.Evaluate(incident_body, row.refractive_index, options.corridor_lineage ? &out.corridor : nullptr);
    out.entry_available = true;
  }
  if (!options.derivatives) {
    return out;
  }
  using J = Jet<3>;
  const J delta[3] = { J::Variable(0, 0), J::Variable(0, 1), J::Variable(0, 2) };
  J rotation[9];
  J pose[9];
  J outgoing[3];
  so3::Exp(delta, rotation);
  so3::MatMul(row.pose.data(), rotation, pose);
  ChainInterfaceDiagnostics<J> derivatives;
  ++out.path_evaluations;
  const bool valid = TracePathChain(normals, slots.data(), count, row.refractive_index, row.incident.data(), pose,
                                    outgoing, nullptr, nullptr, &derivatives);
  for (int j = 0; j < std::min(interfaces.reached, derivatives.reached); ++j) {
    auto& face = out.interfaces[j];
    std::copy_n(derivatives.discriminant[j].v, 3, face.discriminant_pose_gradient.begin());
    face.pose_gradient_available = FiniteGradient(face.discriminant_pose_gradient);
  }
  if (valid && out.path_valid) {
    out.direction_jacobian_available = true;
    for (int j = 0; j < 3; ++j) {
      std::copy_n(outgoing[j].v, 3, out.direction_pose_jacobian[j].begin());
      out.direction_jacobian_available &= FiniteGradient(out.direction_pose_jacobian[j]);
    }
  }
  if (!(row.refractive_index - options.index_step > 1) ||
      row.refractive_index + options.index_step == row.refractive_index ||
      row.refractive_index + .5 * options.index_step == row.refractive_index) {
    return out;
  }
  ChainInterfaceDiagnostics<double> sides[2][2];
  double directions[2][2][3]{};
  bool valid_side[2][2]{};
  for (int level = 0; level < 2; ++level) {
    const double step = options.index_step / (level + 1);
    for (int side = 0; side < 2; ++side) {
      ++out.path_evaluations;
      valid_side[level][side] = TracePathChain(
          normals, slots.data(), count, row.refractive_index + (2 * side - 1) * step, row.incident.data(),
          row.pose.data(), directions[level][side], nullptr, nullptr, &sides[level][side]);
    }
  }
  out.direction_index_available =
      out.path_valid && valid_side[0][0] && valid_side[0][1] && valid_side[1][0] && valid_side[1][1];
  if (out.direction_index_available) {
    for (int j = 0; j < 3; ++j) {
      const double coarse = (directions[0][1][j] - directions[0][0][j]) / (2 * options.index_step);
      const double fine = (directions[1][1][j] - directions[1][0][j]) / options.index_step;
      out.direction_index_derivative[j] = (4 * fine - coarse) / 3;
      out.direction_index_error = std::max(out.direction_index_error, std::abs(fine - coarse) / 3);
    }
    out.direction_index_available =
        FiniteGradient(out.direction_index_derivative) && std::isfinite(out.direction_index_error);
  }
  for (int j = 0; j < interfaces.reached; ++j) {
    auto& face = out.interfaces[j];
    if (sides[0][0].reached <= j || sides[0][1].reached <= j || sides[1][0].reached <= j || sides[1][1].reached <= j) {
      continue;
    }
    const double coarse = (sides[0][1].discriminant[j] - sides[0][0].discriminant[j]) / (2 * options.index_step);
    const double fine = (sides[1][1].discriminant[j] - sides[1][0].discriminant[j]) / options.index_step;
    face.discriminant_index_derivative = (4 * fine - coarse) / 3;
    face.index_derivative_error = std::abs(fine - coarse) / 3;
    face.index_derivative_available =
        std::isfinite(face.discriminant_index_derivative) && std::isfinite(face.index_derivative_error);
  }
  return out;
}

}  // namespace

bool EvaluateDiagnosticBatch(const std::vector<int>& faces, const std::vector<DiagnosticInputRow>& rows,
                             const DiagnosticBatchOptions& options, std::vector<DiagnosticOutputRow>* out) {
  if (!out) {
    return false;
  }
  out->clear();
  if (faces.size() < 2 || faces.size() > kMaxFaceCount ||
      !std::all_of(faces.begin(), faces.end(), [](int face) { return face > 0; }) ||
      (options.derivatives && (!(options.index_step > 0) || !std::isfinite(options.index_step)))) {
    return false;
  }
  out->reserve(rows.size());
  for (const auto& row : rows) {
    out->push_back(EvaluateRow(faces, row, options));
  }
  return true;
}

InterfaceStationaryPoint CorrectInterfaceEvent(const std::vector<int>& faces, const DiagnosticInputRow& source,
                                               const InterfaceSolveOptions& options, uint64_t max_path_evaluations,
                                               std::chrono::steady_clock::time_point deadline) {
  InterfaceStationaryPoint result;
  result.source = source;
  if (options.internal_slot <= 0 || options.internal_slot + 1 >= static_cast<int>(faces.size()) ||
      !(options.residual_tolerance > 0) || !std::isfinite(options.residual_tolerance) || !(options.max_step_rad > 0) ||
      options.max_step_rad >= 1 || options.max_iterations <= 0) {
    return result;
  }
  for (int i = 0; i < options.max_iterations; ++i) {
    // One ordinary trace, one pose jet and four index-difference traces in the
    // existing batch evaluator. Count all six, including a rejected iterate.
    if (max_path_evaluations - std::min(max_path_evaluations, result.path_evaluations) < 6 ||
        std::chrono::steady_clock::now() >= deadline) {
      result.status = InterfaceSolveStatus::kBudgetExceeded;
      return result;
    }
    std::vector<DiagnosticOutputRow> values;
    if (!EvaluateDiagnosticBatch(faces, { result.source }, { true, true }, &values)) {
      return result;
    }
    result.value = std::move(values[0]);
    result.path_evaluations += result.value.path_evaluations;
    const auto& face = result.value.interfaces[options.internal_slot];
    if (!face.reached || !face.pose_gradient_available) {
      result.status = InterfaceSolveStatus::kUnavailable;
      return result;
    }
    if (!result.value.path_valid || !result.value.entry_available ||
        (options.require_positive_entry &&
         (!(result.value.entry.value > 0) || !(result.value.interface_product > 0)))) {
      result.status = InterfaceSolveStatus::kNoSupport;
      return result;
    }
    result.accepted_poses.push_back(result.source.pose);
    if (std::abs(face.discriminant) <= options.residual_tolerance) {
      result.status = InterfaceSolveStatus::kConverged;
      return result;
    }
    if (i + 1 == options.max_iterations) {
      break;
    }
    const double norm = so3::Norm3(face.discriminant_pose_gradient.data());
    if (!(norm > 0) || !std::isfinite(norm)) {
      result.status = InterfaceSolveStatus::kDegenerate;
      return result;
    }
    const double step = std::clamp(-face.discriminant / norm, -options.max_step_rad, options.max_step_rad);
    double delta[3];
    for (int j = 0; j < 3; ++j) {
      delta[j] = step * face.discriminant_pose_gradient[j] / norm;
    }
    double rotation[9];
    std::array<double, 9> next;
    so3::Exp(delta, rotation);
    so3::MatMul(result.source.pose.data(), rotation, next.data());
    result.source.pose = next;
    result.travelled_rad += std::abs(step);
  }
  result.status = InterfaceSolveStatus::kIterationLimit;
  return result;
}

InterfaceCurve TraceInterfaceCurve(const std::vector<int>& faces, const DiagnosticInputRow& seed, int slot,
                                   double step_rad, double event_resolution_rad, int max_points, bool reverse,
                                   uint64_t max_path_evaluations, std::chrono::steady_clock::time_point deadline) {
  InterfaceCurve curve;
  if (!(step_rad > 0) || step_rad >= .5 || !(event_resolution_rad > 0) || !std::isfinite(event_resolution_rad) ||
      event_resolution_rad >= step_rad || max_points < 2 || max_points > 4096) {
    return curve;
  }
  auto correct = [&](const DiagnosticInputRow& source) {
    const uint64_t remaining = max_path_evaluations - std::min(max_path_evaluations, curve.path_evaluations);
    auto result = CorrectInterfaceEvent(faces, source, { slot, 1e-10, .05, 32, false }, remaining, deadline);
    curve.path_evaluations += result.path_evaluations;
    return result;
  };
  auto body = [](const DiagnosticInputRow& row) {
    std::array<double, 3> incident;
    chain_detail::WorldToBody(row.pose.data(), row.incident.data(), incident.data());
    return incident;
  };
  auto distance = [&](const DiagnosticInputRow& x, const DiagnosticInputRow& y) {
    const auto a = body(x);
    const auto b = body(y);
    double cross[3];
    so3::Cross3(a.data(), b.data(), cross);
    return std::atan2(so3::Norm3(cross), so3::Dot3(a.data(), b.data()));
  };
  auto predicate = [](const InterfaceStationaryPoint& point, bool geometric) {
    return point.value.corridor.raw_area - (geometric ? 0 : point.value.corridor.area_threshold);
  };
  auto current = correct(seed);
  if (current.status != InterfaceSolveStatus::kConverged || !(current.value.entry.value > 0)) {
    curve.stop = current.status == InterfaceSolveStatus::kBudgetExceeded ? InterfaceWalkStop::kBudgetExceeded :
                                                                           InterfaceWalkStop::kCorrectorFailed;
    return curve;
  }
  curve.points.push_back(current);
  std::array<double, 3> previous_tangent{};
  for (int point = 1; point < max_points; ++point) {
    const auto incident = body(current.source);
    const auto& gradient = current.value.interfaces[slot].discriminant_pose_gradient;
    double tangent[3];
    so3::Cross3(incident.data(), gradient.data(), tangent);
    const double norm = so3::Norm3(tangent);
    if (!(norm > 0) || !std::isfinite(norm)) {
      curve.stop = InterfaceWalkStop::kCorrectorFailed;
      return curve;
    }
    double sign = point == 1 ? (reverse ? -1 : 1) : (so3::Dot3(tangent, previous_tangent.data()) < 0 ? -1 : 1);
    double delta[3];
    for (int j = 0; j < 3; ++j) {
      previous_tangent[j] = sign * tangent[j] / norm;
      delta[j] = step_rad * previous_tangent[j];
    }
    DiagnosticInputRow trial = current.source;
    double rotation[9];
    so3::Exp(delta, rotation);
    so3::MatMul(current.source.pose.data(), rotation, trial.pose.data());
    auto next = correct(trial);
    if (next.status != InterfaceSolveStatus::kConverged) {
      curve.stop = next.status == InterfaceSolveStatus::kBudgetExceeded ? InterfaceWalkStop::kBudgetExceeded :
                   next.status == InterfaceSolveStatus::kNoSupport      ? InterfaceWalkStop::kOpticalGate :
                                                                          InterfaceWalkStop::kCorrectorFailed;
      return curve;
    }
    for (const bool geometric : { false, true }) {
      if (!(predicate(current, geometric) > 0 && predicate(next, geometric) <= 0)) {
        continue;
      }
      auto positive = current;
      auto nonpositive = next;
      for (int bisect = 0; bisect < 48 && distance(positive.source, nonpositive.source) > event_resolution_rad;
           ++bisect) {
        // Interpolate the existing SO(3) lift, then correct the SAME interface.
        double rel[9];
        double w[3];
        so3::MatTMul(positive.source.pose.data(), nonpositive.source.pose.data(), rel);
        so3::Log(rel, w);
        for (auto& component : w) {
          component *= .5;
        }
        double half[9];
        so3::Exp(w, half);
        auto middle_source = positive.source;
        so3::MatMul(positive.source.pose.data(), half, middle_source.pose.data());
        auto middle = correct(middle_source);
        if (middle.status != InterfaceSolveStatus::kConverged) {
          curve.stop = middle.status == InterfaceSolveStatus::kBudgetExceeded ? InterfaceWalkStop::kBudgetExceeded :
                                                                                InterfaceWalkStop::kCorrectorFailed;
          return curve;
        }
        if (predicate(middle, geometric) > 0) {
          positive = std::move(middle);
        } else {
          nonpositive = std::move(middle);
        }
      }
      const double width = distance(positive.source, nonpositive.source);
      if (width > event_resolution_rad) {
        curve.stop = InterfaceWalkStop::kCorrectorFailed;
        return curve;
      }
      curve.events.push_back({ geometric ? InterfaceWalkStop::kGeometricContact : InterfaceWalkStop::kAreaThreshold,
                               std::move(positive), std::move(nonpositive), width });
    }
    if (next.value.corridor.raw_area <= 0) {
      curve.stop = InterfaceWalkStop::kGeometricContact;
      return curve;
    }
    curve.points.push_back(next);
    if (point > 8 && distance(seed, next.source) < step_rad * .5) {
      curve.stop = InterfaceWalkStop::kClosed;
      return curve;
    }
    current = std::move(next);
  }
  curve.stop = InterfaceWalkStop::kPointLimit;
  return curve;
}

DeviationStationaryPoint CorrectDeviationMinimum(const std::vector<int>& faces, const DiagnosticInputRow& source,
                                                 const DeviationSolveOptions& options, uint64_t max_path_evaluations,
                                                 std::chrono::steady_clock::time_point deadline) {
  DeviationStationaryPoint result;
  result.source = source;
  if (!(options.tolerance_rad > 0) || !std::isfinite(options.tolerance_rad) || !(options.max_step_rad > 0) ||
      options.max_step_rad >= 1 || options.max_iterations <= 0) {
    return result;
  }
  auto evaluate = [&](const DiagnosticInputRow& row, bool derivatives, DiagnosticOutputRow* value) {
    const uint64_t cost = derivatives ? 6 : 1;
    if (max_path_evaluations - std::min(max_path_evaluations, result.path_evaluations) < cost ||
        std::chrono::steady_clock::now() >= deadline) {
      result.status = InterfaceSolveStatus::kBudgetExceeded;
      return false;
    }
    std::vector<DiagnosticOutputRow> output;
    if (!EvaluateDiagnosticBatch(faces, { row }, { derivatives, true }, &output)) {
      result.status = InterfaceSolveStatus::kInvalidInput;
      return false;
    }
    *value = std::move(output[0]);
    result.path_evaluations += value->path_evaluations;
    if (!value->path_valid || !value->entry_available || !(value->entry.value > 0) || !(value->interface_product > 0)) {
      result.status = InterfaceSolveStatus::kNoSupport;
      return false;
    }
    return true;
  };
  auto objective = [&](const DiagnosticOutputRow& value) {
    return 1 - so3::Dot3(source.incident.data(), value.outgoing.data());
  };
  for (int iteration = 0; iteration < options.max_iterations; ++iteration) {
    if (!evaluate(result.source, true, &result.value)) {
      return result;
    }
    if (!result.value.direction_jacobian_available) {
      result.status = InterfaceSolveStatus::kUnavailable;
      return result;
    }
    double cross[3];
    so3::Cross3(source.incident.data(), result.value.outgoing.data(), cross);
    result.deviation_rad =
        std::atan2(so3::Norm3(cross), so3::Dot3(source.incident.data(), result.value.outgoing.data()));
    double incident_body[3];
    double basis[2][3];
    chain_detail::WorldToBody(result.source.pose.data(), source.incident.data(), incident_body);
    so3::TangentBasis(incident_body, basis);
    double gradient[2]{};
    for (int j = 0; j < 2; ++j) {
      for (int component = 0; component < 3; ++component) {
        for (int axis = 0; axis < 3; ++axis) {
          gradient[j] -=
              source.incident[component] * result.value.direction_pose_jacobian[component][axis] * basis[j][axis];
        }
      }
    }
    auto perturb = [&](double x, double y) {
      DiagnosticInputRow row = result.source;
      double delta[3];
      for (int j = 0; j < 3; ++j) {
        delta[j] = x * basis[0][j] + y * basis[1][j];
      }
      double rotation[9];
      so3::Exp(delta, rotation);
      so3::MatMul(result.source.pose.data(), rotation, row.pose.data());
      return row;
    };
    const double f = objective(result.value);
    double hessians[2][3]{};
    constexpr double kStep = 2e-4;
    for (int refinement = 0; refinement < 2; ++refinement) {
      const double h = kStep / (refinement + 1);
      double values[3][3]{};
      values[1][1] = f;
      for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
          if (x == 0 && y == 0) {
            continue;
          }
          DiagnosticOutputRow trial;
          if (!evaluate(perturb(x * h, y * h), false, &trial)) {
            return result;
          }
          values[x + 1][y + 1] = objective(trial);
        }
      }
      hessians[refinement][0] = (values[0][1] - 2 * f + values[2][1]) / (h * h);
      hessians[refinement][1] = (values[2][2] - values[2][0] - values[0][2] + values[0][0]) / (4 * h * h);
      hessians[refinement][2] = (values[1][0] - 2 * f + values[1][2]) / (h * h);
    }
    // Bound the observed stencil discrepancy separately from objective rounding.
    // This is a local numerical diagnostic, not a global minimum certificate.
    double hessian[3];
    result.hessian_error = 0;
    for (int j = 0; j < 3; ++j) {
      hessian[j] = (4 * hessians[1][j] - hessians[0][j]) / 3;
      result.hessian_error = std::max(result.hessian_error, std::abs(hessians[1][j] - hessians[0][j]));
    }
    result.hessian_error += 64 * std::numeric_limits<double>::epsilon() / (kStep * kStep);
    const double mean = .5 * (hessian[0] + hessian[2]);
    const double gap = std::hypot(.5 * (hessian[0] - hessian[2]), hessian[1]);
    result.objective_curvatures = { mean - gap, mean + gap };
    if (!(result.objective_curvatures[0] > 2 * result.hessian_error)) {
      result.status = InterfaceSolveStatus::kDegenerate;
      return result;
    }
    const double determinant = hessian[0] * hessian[2] - hessian[1] * hessian[1];
    double dx = (-hessian[2] * gradient[0] + hessian[1] * gradient[1]) / determinant;
    double dy = (hessian[1] * gradient[0] - hessian[0] * gradient[1]) / determinant;
    result.correction_rad = std::hypot(dx, dy);
    if (result.correction_rad <= options.tolerance_rad) {
      result.status = InterfaceSolveStatus::kConverged;
      return result;
    }
    if (iteration + 1 == options.max_iterations) {
      break;
    }
    const double scale = std::min(1.0, options.max_step_rad / result.correction_rad);
    dx *= scale;
    dy *= scale;
    bool accepted = false;
    for (int line = 0; line < 8; ++line) {
      const auto row = perturb(dx, dy);
      DiagnosticOutputRow value;
      if (!evaluate(row, false, &value)) {
        if (result.status == InterfaceSolveStatus::kBudgetExceeded ||
            result.status == InterfaceSolveStatus::kInvalidInput) {
          return result;
        }
      } else if (objective(value) <= f) {
        result.source = row;
        result.value = std::move(value);
        accepted = true;
        break;
      }
      dx *= .5;
      dy *= .5;
    }
    if (!accepted) {
      result.status = InterfaceSolveStatus::kUnavailable;
      return result;
    }
  }
  result.status = InterfaceSolveStatus::kIterationLimit;
  return result;
}

}  // namespace lumice::analytic
