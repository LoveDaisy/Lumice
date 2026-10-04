#include "analytic/diagnostic_batch.hpp"

#include <algorithm>
#include <cmath>

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

}  // namespace lumice::analytic
