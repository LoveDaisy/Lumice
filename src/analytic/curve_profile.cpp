#include "analytic/curve_profile.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "analytic/jet2.hpp"
#include "analytic/path_chain.hpp"
#include "analytic/path_fiber.hpp"

namespace lumice::analytic {
namespace {

constexpr double kEps = std::numeric_limits<double>::epsilon();
constexpr double kRelativeRoundoff = 64.0 * kEps;

bool ValidOptions(const CurveProfileOptions& o) {
  return std::isfinite(o.context_tolerance) && o.context_tolerance >= kRelativeRoundoff &&
         o.context_tolerance <= 1e-3 && std::isfinite(o.grazing_discriminant) && o.grazing_discriminant > 0.0 &&
         o.grazing_discriminant <= 1.0 && std::isfinite(o.rank_relative_tolerance) &&
         o.rank_relative_tolerance >= kRelativeRoundoff && o.rank_relative_tolerance < 1.0;
}

bool CorrectDimension(PoseCurveKind kind, const PoseManifold& manifold) {
  if (kind == PoseCurveKind::kSkyFiber) {
    return true;
  }
  return manifold.IsValid() && manifold.Dimension() == (kind == PoseCurveKind::kFamily1D ? 1 : 2);
}

bool InManifold(const PoseManifold& manifold, const double param[3], double tolerance) {
  for (int i = 0; i < 3; ++i) {
    const auto& axis = manifold.axes[i];
    if (!std::isfinite(param[i])) {
      return false;
    }
    if (!axis.free && std::abs(param[i] - axis.value) > tolerance) {
      return false;
    }
    if (axis.free && !axis.periodic && (param[i] < axis.min_val || param[i] > axis.max_val)) {
      return false;
    }
  }
  return true;
}

bool ValidNode(const PoseCurveNode& node, PoseCurveKind kind, const PoseManifold& manifold, const double sun[3],
               double tolerance) {
  if (!ValidateRotation(node.pose) || !ValidateUnitVector(node.u) || !ValidateUnitVector(node.sky)) {
    return false;
  }
  double u[3];
  WorldToBody(node.pose, sun, u);
  for (int i = 0; i < 3; ++i) {
    if (std::abs(u[i] - node.u[i]) > tolerance) {
      return false;
    }
  }
  if (kind != PoseCurveKind::kSkyFiber) {
    if (!InManifold(manifold, node.param, tolerance)) {
      return false;
    }
    double pose[9];
    EulerPose(node.param[0], node.param[1], node.param[2], pose);
    for (int i = 0; i < 9; ++i) {
      if (std::abs(pose[i] - node.pose[i]) > tolerance) {
        return false;
      }
    }
  }
  return true;
}

ProfileOptics OpticalAt(const PathIntrinsic& path, const double u[3], const CurveProfileOptions& options,
                        double* outgoing_body = nullptr) {
  ProfileOptics out;
  const auto& field = path.field();
  const int count = field.SlotCount();
  out.refractive_index = field.RefractiveIndex();
  const FieldSample sample = path.EvaluateU(u);
  out.valid = sample.v_p;
  out.transmission = sample.t_p;
  out.area = path.EvaluateAreaU(u);
  out.interfaces.resize(static_cast<size_t>(count));
  const double incident[3] = { -u[0], -u[1], -u[2] };
  const double identity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
  double directions[3 * (kMaxFaceCount + 1)]{};
  double factors[kMaxFaceCount]{};
  PathOutputs detail{};
  detail.segment_directions = directions;
  detail.interface_transmittances = factors;
  double outgoing[3];
  ChainInterfaceDiagnostics<double> diag;
  const bool valid = TracePathChain(field.table(), field.slots(), count, out.refractive_index, incident, identity,
                                    outgoing, &detail, nullptr, &diag);
  if (valid && outgoing_body) {
    std::copy_n(outgoing, 3, outgoing_body);
  }
  // Partial prefix diagnostics survive a later optical refusal; its coefficients are only available
  // if that interface's factor was actually evaluated (a failed gate precedes the factor).
  for (int i = 0; i < count; ++i) {
    auto& face = out.interfaces[static_cast<size_t>(i)];
    if (i >= diag.reached) {
      continue;
    }
    face.reached = true;
    face.incidence = diag.incidence[i];
    face.discriminant = diag.discriminant[i];
    face.factor_available = valid || i < diag.reached - 1;
    if (face.factor_available) {
      face.factor = factors[i];
    }
    if (i == 0 || i == count - 1) {
      face.tir = ProfileTirState::kNotInternal;
    } else if (std::isfinite(face.discriminant) && face.incidence > 0.0) {
      face.tir = face.discriminant < 0.0 ? ProfileTirState::kTotal :
                 face.discriminant > 0.0 ? ProfileTirState::kPartial :
                                           ProfileTirState::kCritical;
    }
  }
  if (count >= 2) {
    for (int i : { 0, count - 1 }) {
      if (i < diag.reached && std::isfinite(diag.discriminant[i]) &&
          diag.discriminant[i] < options.grazing_discriminant) {
        out.grazing_accuracy_note = true;
      }
    }
  }
  return out;
}

// Free-coordinate sky tensor. The same evaluator supplies both the fold coefficient and the
// chromatic corrector's desingularized defining equation; no finite-difference path exists.
struct SkyTensor {
  bool valid = false;
  Jet2<2> sky[3];
  Jet2<2> interfaces[kMaxFaceCount];
  double j_reg = 0.0;
  double grad_reg[2]{};
};

SkyTensor TensorAt(const PathIntrinsic& path, const PoseManifold& manifold, const double sun[3],
                   const double param[3]) {
  SkyTensor out;
  int axes[3];
  const int dim = manifold.FreeAxes(axes);
  Jet2<2> angle[3] = { param[0], param[1], param[2] };
  for (int i = 0; i < dim && i < 2; ++i) {
    angle[axes[i]] = Jet2<2>::Variable(param[axes[i]], i);
  }
  Jet2<2> pose[9];
  EulerPose(angle[0], angle[1], angle[2], pose);
  const Jet2<2> incident[3] = { -sun[0], -sun[1], -sun[2] };
  Jet2<2> outgoing[3];
  ChainInterfaceDiagnostics<Jet2<2>> diag;
  const auto& field = path.field();
  if (!TracePathChain(field.table(), field.slots(), field.SlotCount(), Jet2<2>(field.RefractiveIndex()), incident, pose,
                      outgoing, nullptr, nullptr, &diag)) {
    return out;
  }
  double x[3], a[3], b[3];
  for (int i = 0; i < 3; ++i) {
    out.sky[i] = -outgoing[i];
    x[i] = out.sky[i].a;
    a[i] = out.sky[i].v[0];
    b[i] = out.sky[i].v[1];
    if (!std::isfinite(x[i]) || !std::isfinite(a[i]) || !std::isfinite(b[i])) {
      return out;
    }
    for (int j = 0; j < dim; ++j) {
      for (int k = 0; k < dim; ++k) {
        if (!std::isfinite(out.sky[i].h[j][k])) {
          return out;
        }
      }
    }
  }
  for (int i = 0; i < field.SlotCount(); ++i) {
    out.interfaces[i] = diag.discriminant[i];
  }
  double cross[3];
  so3::Cross3(a, b, cross);
  const double j = so3::Dot3(x, cross);
  const Jet2<2> w =
      field.SlotCount() >= 2 ? diag.discriminant[0] * diag.discriminant[field.SlotCount() - 1] : Jet2<2>(1.0);
  out.j_reg = w.a * j;
  for (int k = 0; k < dim; ++k) {
    double da[3], db[3], dcross1[3], dcross2[3];
    for (int i = 0; i < 3; ++i) {
      da[i] = out.sky[i].h[0][k];
      db[i] = out.sky[i].h[1][k];
    }
    so3::Cross3(da, b, dcross1);
    so3::Cross3(a, db, dcross2);
    // sky_q . (sky_q0 x sky_q1) vanishes algebraically for k=0,1.
    out.grad_reg[k] = w.v[k] * j + w.a * (so3::Dot3(x, dcross1) + so3::Dot3(x, dcross2));
  }
  out.valid = std::isfinite(out.j_reg) && std::isfinite(out.grad_reg[0]) && std::isfinite(out.grad_reg[1]);
  return out;
}

ProfileGeometry GeometryAt(const PathIntrinsic& path, const PoseCurveNode& node, PoseCurveKind kind,
                           const PoseManifold& manifold, const double sun[3], const CurveProfileOptions& options) {
  ProfileGeometry out;
  if (kind == PoseCurveKind::kSkyFiber) {
    const auto& field = path.field();
    const double incident[3] = { -sun[0], -sun[1], -sun[2] };
    const auto diag = EvaluatePathDiagnostics(field.table(), field.slots(), field.SlotCount(), field.RefractiveIndex(),
                                              incident, node.pose);
    out.status = ProfileGeometryStatus::kUnavailable;
    if (diag.jacobian.available) {
      out.sigma1 = diag.jacobian.singular_values[0];
      out.sigma2 = diag.jacobian.singular_values[1];
      out.inverse_normal_jacobian = 1.0 / diag.jacobian.value;
      out.status = diag.jacobian.value > 0.0 ? ProfileGeometryStatus::kAvailable : ProfileGeometryStatus::kDegenerate;
    }
    return out;
  }
  if (kind != PoseCurveKind::kCaustic2D) {
    return out;
  }
  out.status = ProfileGeometryStatus::kUnavailable;
  const auto jet = TensorAt(path, manifold, sun, node.param);
  if (!jet.valid) {
    return out;
  }
  double a[3], b[3], x[3];
  for (int i = 0; i < 3; ++i) {
    a[i] = jet.sky[i].v[0];
    b[i] = jet.sky[i].v[1];
    x[i] = jet.sky[i].a;
  }
  const double g00 = so3::Dot3(a, a), g01 = so3::Dot3(a, b), g11 = so3::Dot3(b, b);
  const double theta = 0.5 * std::atan2(2.0 * g01, g00 - g11);
  const double c = std::cos(theta), s = std::sin(theta);
  const double kernel[2] = { -s, c };
  double image[3], residual[3];
  for (int i = 0; i < 3; ++i) {
    image[i] = c * a[i] + s * b[i];
    residual[i] = -s * a[i] + c * b[i];
  }
  out.sigma1 = so3::Norm3(image);
  out.sigma2 = so3::Norm3(residual);
  if (!(out.sigma1 > 0.0)) {
    out.status = ProfileGeometryStatus::kDegenerate;
    return out;
  }
  if (out.sigma2 / out.sigma1 > options.rank_relative_tolerance) {
    out.status = ProfileGeometryStatus::kNotCaustic;
    return out;
  }
  for (double& v : image) {
    v /= out.sigma1;
  }
  so3::Cross3(x, image, out.sky_normal.data());
  const double normal_length = so3::Norm3(out.sky_normal.data());
  if (!(normal_length > 0.0)) {
    out.status = ProfileGeometryStatus::kDegenerate;
    return out;
  }
  for (double& v : out.sky_normal) {
    v /= normal_length;
  }
  out.kernel = { kernel[0], kernel[1] };
  double beta = 0.0;
  for (int i = 0; i < 3; ++i) {
    double contraction = 0.0;
    for (int j = 0; j < 2; ++j) {
      for (int k = 0; k < 2; ++k) {
        contraction += kernel[j] * jet.sky[i].h[j][k] * kernel[k];
      }
    }
    beta += 0.5 * out.sky_normal[static_cast<size_t>(i)] * contraction;
  }
  out.lateral_beta = beta;
  out.fold_concentration = 1.0 / (out.sigma1 * std::sqrt(std::abs(beta)));
  out.status = beta == 0.0 ? ProfileGeometryStatus::kDegenerate : ProfileGeometryStatus::kAvailable;
  return out;
}

bool ValidChromaticOptions(const ProfileChromaticOptions& o) {
  return std::isfinite(o.parameter_tolerance) && o.parameter_tolerance >= kRelativeRoundoff &&
         o.parameter_tolerance <= 1e-3 && std::isfinite(o.max_travel) && o.max_travel > 0.0 &&
         o.max_travel <= math::kPiD && o.maximum_iterations > 0 && o.maximum_iterations <= 128;
}

void UpdateNode(PoseCurveNode* node, const double sun[3], const SkyTensor& jet) {
  EulerPose(node->param[0], node->param[1], node->param[2], node->pose);
  WorldToBody(node->pose, sun, node->u);
  for (int i = 0; i < 3; ++i) {
    node->sky[i] = jet.sky[i].a;
  }
}

}  // namespace

CurveProfile EvaluateCurveProfile(const PoseCurve& curve, const PathIntrinsic& main_path,
                                  const AxisPoseDensity& density, const PoseManifold& manifold, const double sun[3],
                                  const std::vector<const PathIntrinsic*>& wavelengths,
                                  const CurveProfileOptions& options) {
  if (!ValidOptions(options) || !ValidateUnitVector(sun) || !CorrectDimension(curve.kind, manifold)) {
    throw std::invalid_argument("CurveProfile requires valid options, unit sun and a compatible manifold");
  }
  for (const auto* path : wavelengths) {
    if (!path || path->faces() != main_path.faces()) {
      throw std::invalid_argument("CurveProfile wavelengths require the same literal path");
    }
  }
  CurveProfile out;
  out.kind = curve.kind;
  out.coverage = curve.coverage;
  out.nodes.reserve(curve.nodes.size());
  for (const auto& node : curve.nodes) {
    CurveProfileNode value;
    std::array<double, 9> pose;
    std::copy_n(node.pose, 9, pose.begin());
    value.rho = density.Evaluate(pose);
    value.log_rho = density.LogDensity(pose);
    value.context_valid = ValidNode(node, curve.kind, manifold, sun, options.context_tolerance);
    if (value.context_valid) {
      double outgoing_body[3];
      value.main = OpticalAt(main_path, node.u, options, outgoing_body);
      if (value.main.valid) {
        double outgoing[3];
        BodyToWorld(node.pose, outgoing_body, outgoing);
        for (int i = 0; i < 3; ++i) {
          if (std::abs(node.sky[i] + outgoing[i]) > options.context_tolerance) {
            value.context_valid = false;
          }
        }
      }
      for (const auto* path : wavelengths) {
        value.wavelengths.push_back(OpticalAt(*path, node.u, options));
      }
      if (wavelengths.empty()) {
        value.wavelengths.push_back(value.main);
      }
      if (value.context_valid) {
        value.geometry = GeometryAt(main_path, node, curve.kind, manifold, sun, options);
      } else {
        value.geometry.status = ProfileGeometryStatus::kInvalidNode;
      }
    } else {
      value.geometry.status = ProfileGeometryStatus::kInvalidNode;
    }
    out.nodes.push_back(std::move(value));
  }
  return out;
}

ProfileExpansion ExpandProfileNode(const PathIntrinsic& path, const PoseCurveNode& node) {
  ProfileExpansion out;
  if (!ValidateUnitVector(node.u)) {
    return out;
  }
  CorridorDiagnostics diag;
  out.area = path.EvaluateAreaU(node.u, &diag);
  if (out.area.status != EntryMeasureStatus::kOk) {
    return out;
  }
  const auto& field = path.field();
  const int count = field.SlotCount();
  const double incident[3] = { -node.u[0], -node.u[1], -node.u[2] };
  const double identity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
  double directions[3 * (kMaxFaceCount + 1)];
  double factors[kMaxFaceCount];
  PathOutputs detail{};
  detail.segment_directions = directions;
  detail.interface_transmittances = factors;
  if (!EvaluatePath(field.table(), field.slots(), count, field.RefractiveIndex(), incident, identity, &detail)) {
    return out;
  }
  out.status = path.RecoverRepresentativePath(diag, &out.path);
  if (out.status == RecoveryStatus::kOk) {
    out.interface_factors.assign(factors, factors + count);
    for (int i = 0; i <= count; ++i) {
      out.directions.push_back({ directions[3 * i], directions[3 * i + 1], directions[3 * i + 2] });
    }
  }
  return out;
}

ProfileChromaticPoint RetraceProfileSeed(const PathIntrinsic& path, PoseCurveKind kind, const PoseManifold& manifold,
                                         const double sun[3], const PoseCurveNode& seed, int internal_index,
                                         const ProfileChromaticOptions& options) {
  ProfileChromaticPoint out;
  out.node = seed;
  if (kind != PoseCurveKind::kFamily1D && kind != PoseCurveKind::kCaustic2D) {
    out.status = ProfileSolveStatus::kUnsupported;
    return out;
  }
  const bool valid_index =
      internal_index == -1 || (internal_index >= 1 && internal_index < path.field().SlotCount() - 1);
  if (!ValidChromaticOptions(options) || !ValidateUnitVector(sun) || !CorrectDimension(kind, manifold) ||
      !ValidNode(seed, kind, manifold, sun, CurveProfileOptions{}.context_tolerance) || !valid_index) {
    return out;
  }
  int axes[3];
  const int dim = manifold.FreeAxes(axes);
  double section[2]{};
  if (dim == 2 && internal_index == -1) {
    section[0] = seed.tangent[axes[0]];
    section[1] = seed.tangent[axes[1]];
    const double norm = std::hypot(section[0], section[1]);
    if (!(norm > 0.0) || !std::isfinite(norm)) {
      return out;
    }
    section[0] /= norm;
    section[1] /= norm;
  }
  out.status = ProfileSolveStatus::kIterationLimit;
  // A corrected node is a new point, not an accepted continuation sample: inherited differential
  // diagnostics must not masquerade as recomputed ones.
  out.node.sky_speed = out.node.sigma1 = out.node.sigma2 = std::numeric_limits<double>::quiet_NaN();
  out.node.arclength = std::numeric_limits<double>::quiet_NaN();
  for (int iteration = 0; iteration < options.maximum_iterations; ++iteration) {
    out.iterations = iteration + 1;
    EulerPose(out.node.param[0], out.node.param[1], out.node.param[2], out.node.pose);
    WorldToBody(out.node.pose, sun, out.node.u);
    std::fill_n(out.node.sky, 3, std::numeric_limits<double>::quiet_NaN());
    if (!InManifold(manifold, out.node.param, CurveProfileOptions{}.context_tolerance)) {
      out.status = ProfileSolveStatus::kOutsideDomain;
      return out;
    }
    const auto jet = TensorAt(path, manifold, sun, out.node.param);
    if (!jet.valid) {
      out.status = ProfileSolveStatus::kOpticallyInvalid;
      return out;
    }
    UpdateNode(&out.node, sun, jet);
    double delta[2]{};
    if (dim == 1) {
      if (internal_index >= 0) {
        const auto& d = jet.interfaces[internal_index];
        if (!std::isfinite(d.v[0]) || d.v[0] == 0.0) {
          out.status = ProfileSolveStatus::kDegenerate;
          return out;
        }
        delta[0] = -d.a / d.v[0];
      }
    } else {
      const double a = jet.grad_reg[0], b = jet.grad_reg[1];
      double c, d, residual;
      if (internal_index >= 0) {
        const auto& disc = jet.interfaces[internal_index];
        c = disc.v[0];
        d = disc.v[1];
        residual = disc.a;
      } else {
        c = section[0];
        d = section[1];
        residual =
            c * (out.node.param[axes[0]] - seed.param[axes[0]]) + d * (out.node.param[axes[1]] - seed.param[axes[1]]);
      }
      const double scale = std::hypot(a, b) * std::hypot(c, d);
      const double det = a * d - b * c;
      if (!std::isfinite(scale) || !std::isfinite(det) || !(scale > 0.0) ||
          std::abs(det) <= kRelativeRoundoff * scale) {
        out.status = ProfileSolveStatus::kDegenerate;
        return out;
      }
      delta[0] = (-jet.j_reg * d + b * residual) / det;
      delta[1] = (-a * residual + jet.j_reg * c) / det;
    }
    out.correction_rad = std::hypot(delta[0], delta[1]);
    if (!std::isfinite(out.correction_rad)) {
      out.status = ProfileSolveStatus::kDegenerate;
      return out;
    }
    if (out.correction_rad <= options.parameter_tolerance) {
      out.status = ProfileSolveStatus::kConverged;
      return out;
    }
    if (out.travelled_rad + out.correction_rad > options.max_travel) {
      out.status = ProfileSolveStatus::kTravelLimit;
      return out;
    }
    if (iteration + 1 == options.maximum_iterations) {
      return out;
    }
    for (int i = 0; i < dim; ++i) {
      out.node.param[axes[i]] += delta[i];
    }
    out.travelled_rad += out.correction_rad;
  }
  return out;
}

ProfileChromaticDisplacement ProfileChromaticShift(const PathIntrinsic& red, const PathIntrinsic& blue,
                                                   PoseCurveKind kind, const PoseManifold& manifold,
                                                   const double sun[3], const PoseCurveNode& seed, int internal_index,
                                                   const ProfileChromaticOptions& options) {
  ProfileChromaticDisplacement out;
  if (red.faces() != blue.faces()) {
    return out;
  }
  out.red = RetraceProfileSeed(red, kind, manifold, sun, seed, internal_index, options);
  out.blue = RetraceProfileSeed(blue, kind, manifold, sun, seed, internal_index, options);
  out.available = out.red.status == ProfileSolveStatus::kConverged && out.blue.status == ProfileSolveStatus::kConverged;
  if (out.available) {
    for (int i = 0; i < 3; ++i) {
      out.dx[static_cast<size_t>(i)] = out.blue.node.sky[i] - out.red.node.sky[i];
    }
    out.angle_rad = so3::AngleBetween(out.red.node.sky, out.blue.node.sky);
  }
  return out;
}

ProfileFiberLink LinkProfileFiber(IceDiscovery& discovery, const PoseCurveNode& node,
                                  const ProfileFiberOptions& options, const DiscoveryStop* stop) {
  ProfileFiberLink out;
  if (!ValidateRotation(node.pose) || !ValidateUnitVector(node.sky) || !ValidateParams(options.continuation) ||
      options.sample_count < 2 || !std::isfinite(options.band_half_width) || options.band_half_width <= 0.0 ||
      options.band_half_width > math::kPiD || !std::isfinite(options.cluster_radius) || options.cluster_radius <= 0.0 ||
      options.cluster_radius > math::kPiD || !std::isfinite(options.dedup_distance) || options.dedup_distance <= 0.0 ||
      options.dedup_distance > math::kPiD) {
    return out;
  }
  if (stop && stop->Stopped()) {
    out.stop_requested = true;
    out.status = ProfileFiberStatus::kStopped;
    return out;
  }
  const double target[3] = { -node.sky[0], -node.sky[1], -node.sky[2] };
  double corrected[9];
  out.hot_seed_admitted = discovery.Admit(MakeTargetChart(target), options.continuation, node.pose, corrected);
  out.output = discovery.Discover(target, options.sample_count, options.band_half_width, node.pose, 1,
                                  options.cluster_radius, options.dedup_distance, options.continuation, stop);
  out.stop_requested = stop && stop->Stopped();
  if (out.hot_seed_admitted) {
    for (size_t i = 0; i < out.output.components.size(); ++i) {
      const auto& component = out.output.components[i];
      if (so3::Distance(corrected, component.seed) < options.dedup_distance ||
          DistanceToCurve(corrected, component.forward) < options.dedup_distance ||
          DistanceToCurve(corrected, component.backward) < options.dedup_distance) {
        out.hot_component = static_cast<int>(i);
        out.status = ProfileFiberStatus::kConverged;
        return out;
      }
    }
  }
  out.status = stop && stop->Stopped() ? ProfileFiberStatus::kStopped :
               out.hot_seed_admitted   ? ProfileFiberStatus::kNotConverged :
                                         ProfileFiberStatus::kSeedRejected;
  return out;
}

}  // namespace lumice::analytic
