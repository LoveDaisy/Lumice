#include "analytic/sky_blur_field.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "analytic/path_chain.hpp"

namespace lumice::analytic {
namespace {
constexpr double kEps = std::numeric_limits<double>::epsilon();
constexpr double kRoundoff = 64 * kEps;

// Symmetric PSD square root by normalized Jacobi, preserving rank-deficient covariances.
// Columns of factor are eigenvectors times sqrt(eigenvalue): covariance = factor factor^T.
bool CovarianceFactor(const std::array<double, 9>& covariance, double factor[9]) {
  double scale = 0;
  for (double x : covariance) {
    if (!std::isfinite(x)) {
      return false;
    }
    scale = std::max(scale, std::abs(x));
  }
  std::fill(factor, factor + 9, 0.0);
  if (scale == 0) {
    return true;
  }
  double a[9];
  double vectors[9]{ 1, 0, 0, 0, 1, 0, 0, 0, 1 };
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      const double x = covariance[3 * i + j] / scale;
      const double y = covariance[3 * j + i] / scale;
      if (std::abs(x - y) > kRoundoff) {
        return false;
      }
      a[3 * i + j] = 0.5 * x + 0.5 * y;
    }
  }
  bool converged = false;
  for (int sweep = 0; sweep < 32; ++sweep) {
    if (std::max({ std::abs(a[1]), std::abs(a[2]), std::abs(a[5]) }) <= 8 * kEps) {
      converged = true;
      break;
    }
    for (int p = 0; p < 2; ++p) {
      for (int q = p + 1; q < 3; ++q) {
        const double apq = a[3 * p + q];
        if (apq == 0) {
          continue;
        }
        const double angle = 0.5 * std::atan2(2 * apq, a[3 * q + q] - a[3 * p + p]);
        const double c = std::cos(angle);
        const double s = std::sin(angle);
        const double app = a[3 * p + p];
        const double aqq = a[3 * q + q];
        a[3 * p + p] = c * c * app - 2 * s * c * apq + s * s * aqq;
        a[3 * q + q] = s * s * app + 2 * s * c * apq + c * c * aqq;
        a[3 * p + q] = a[3 * q + p] = 0;
        for (int k = 0; k < 3; ++k) {
          if (k != p && k != q) {
            const double akp = a[3 * k + p];
            const double akq = a[3 * k + q];
            a[3 * k + p] = a[3 * p + k] = c * akp - s * akq;
            a[3 * k + q] = a[3 * q + k] = s * akp + c * akq;
          }
          const double vkp = vectors[3 * k + p];
          const double vkq = vectors[3 * k + q];
          vectors[3 * k + p] = c * vkp - s * vkq;
          vectors[3 * k + q] = s * vkp + c * vkq;
        }
      }
    }
  }
  if (!converged) {
    return false;
  }
  for (int j = 0; j < 3; ++j) {
    if (a[3 * j + j] < -kRoundoff) {
      return false;
    }
    const double width = std::sqrt(std::max(0.0, a[3 * j + j])) * std::sqrt(scale);
    for (int i = 0; i < 3; ++i) {
      factor[3 * i + j] = vectors[3 * i + j] * width;
    }
  }
  return true;
}
}  // namespace

PoseCovariance AxisPoseCovariance(const AxisDistribution& distribution, const double pose[9]) {
  PoseCovariance result;
  result.error = AxisPoseDensityError(distribution);
  if (*result.error) {
    return result;
  }
  if (!pose || !ValidateRotation(pose)) {
    result.error = "pose must be a finite SO(3) rotation";
    return result;
  }
  const double sine = std::hypot(pose[2], pose[5]);
  const double zenith = std::atan2(sine, pose[8]);
  const bool pole = zenith == 0 || zenith == math::kPiD;
  const bool uniform =
      distribution.azimuth_dist.type == DistributionType::kUniform && distribution.azimuth_dist.spread == 360.0f;
  if (!uniform && sine < std::sqrt(kEps)) {
    result.status = SkyBlurStatus::kUnsupportedAzimuth;
    result.error = "azimuth_dist must be full-turn uniform at pole-adjacent poses";
    return result;
  }
  const AxisPoseDensity density(distribution);
  const auto v = density.Variances();
  if (!std::isfinite(v.sigma2_az) || !std::isfinite(v.sigma2_lat) || !std::isfinite(v.sigma2_roll) ||
      (density.MomentFlags() & static_cast<uint32_t>(AxisDensityFlag::kQuadratureAccuracy)) != 0) {
    result.status = SkyBlurStatus::kMomentUnavailable;
    result.error = "distribution centered moments unavailable";
    return result;
  }
  double world[9]{};
  if (pole) {
    try {
      const double mu2 = AxisZenithPoleRawMoment(distribution, pose[8] > 0);
      world[0] = world[4] = mu2 / 2;
      world[8] = v.sigma2_az + v.sigma2_roll;
    } catch (const std::runtime_error&) {
      result.status = SkyBlurStatus::kMomentUnavailable;
      result.error = "latitude_dist pole raw moment unavailable";
      return result;
    }
  } else {
    const double node_line[3]{ -pose[5] / sine, pose[2] / sine, 0 };
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        world[3 * i + j] =
            v.sigma2_lat * node_line[i] * node_line[j] + v.sigma2_roll * pose[3 * i + 2] * pose[3 * j + 2];
      }
    }
    world[8] += v.sigma2_az;
  }
  double temp[9];
  so3::MatTMul(pose, world, temp);
  so3::MatMul(temp, pose, result.sigma_pose.data());
  result.pole_marginal = pole;
  result.status = SkyBlurStatus::kOk;
  result.error = "";
  return result;
}

const char* ValidateSkyBlurParams(const FaceNormalTable& table, const int* slots, int slot_count,
                                  double refractive_index, const double incident[3], const double pose[9],
                                  const std::array<double, 9>& sigma_pose) {
  if (!pose || !ValidateRotation(pose)) {
    return "pose must be a finite SO(3) rotation";
  }
  if (!incident || !ValidateUnitVector(incident)) {
    return "incident must be a finite unit vector";
  }
  if (!std::isfinite(refractive_index) || refractive_index <= 0) {
    return "refractive_index must be finite and positive";
  }
  if (!slots || slot_count < 1 || slot_count > kMaxFaceCount) {
    return "slots/slot_count must name 1..kMaxFaceCount faces";
  }
  if (table.slot_cnt < 1 || table.slot_cnt > kMaxFaceSlots) {
    return "table.slot_cnt must be in 1..kMaxFaceSlots";
  }
  for (int i = 0; i < slot_count; ++i) {
    const int slot = slots[i];
    if (slot < 0 || slot >= table.slot_cnt || !table.present[slot]) {
      return "slots must name present table faces";
    }
    if (!ValidateUnitVector(table.normal[slot])) {
      return "table.normal must be finite unit vectors";
    }
  }
  double factor[9];
  if (!CovarianceFactor(sigma_pose, factor)) {
    return "sigma_pose must be finite symmetric positive semidefinite";
  }
  return "";
}

SkyBlurField PushforwardSkyBlur(const FaceNormalTable& table, const int* slots, int slot_count, double refractive_index,
                                const double incident[3], const double pose[9],
                                const std::array<double, 9>& sigma_pose) {
  SkyBlurField result;
  result.error = ValidateSkyBlurParams(table, slots, slot_count, refractive_index, incident, pose, sigma_pose);
  if (*result.error) {
    return result;
  }
  using J = Jet<3>;
  const J delta[3]{ J::Variable(0, 0), J::Variable(0, 1), J::Variable(0, 2) };
  J perturb[9], rotation[9], outgoing[3];
  so3::Exp(delta, perturb);
  so3::MatMul(pose, perturb, rotation);
  const J inc[3]{ J(incident[0]), J(incident[1]), J(incident[2]) };
  if (!TracePathChain(table, slots, slot_count, J(refractive_index), inc, rotation, outgoing, nullptr, nullptr)) {
    result.status = SkyBlurStatus::kPathInvalid;
    result.error = "pose/incident path is optically invalid";
    return result;
  }
  for (int i = 0; i < 3; ++i) {
    result.sky[i] = -outgoing[i].a;
  }
  so3::TangentBasis(result.sky, result.basis);
  double factor[9];
  CovarianceFactor(sigma_pose, factor);
  double b[2][3]{};
  for (int a = 0; a < 2; ++a) {
    for (int j = 0; j < 3; ++j) {
      for (int i = 0; i < 3; ++i) {
        result.j_sky[a][j] -= result.basis[a][i] * outgoing[i].v[j];
      }
    }
    for (int j = 0; j < 3; ++j) {
      for (int k = 0; k < 3; ++k) {
        b[a][j] += result.j_sky[a][k] * factor[3 * k + j];
      }
      if (!std::isfinite(result.j_sky[a][j]) || !std::isfinite(b[a][j])) {
        result.status = SkyBlurStatus::kNonFinite;
        result.error = "sky derivative or covariance is not representable";
        return result;
      }
    }
  }
  // Normalize before the Gram/SVD to avoid overflow of squared finite widths.
  double scale = 0;
  for (const auto& row : b) {
    for (double x : row) {
      scale = std::max(scale, std::abs(x));
    }
  }
  if (scale > 0) {
    for (auto& row : b) {
      for (double& x : row) {
        x /= scale;
      }
    }
  }
  const auto svd = so3::SvdTwoByThree(b[0], b[1]);
  result.sigma_major = scale * svd.sigma1;
  result.sigma_minor = scale * svd.sigma2;
  const double g00 = so3::Dot3(b[0], b[0]);
  const double g01 = so3::Dot3(b[0], b[1]);
  const double g11 = so3::Dot3(b[1], b[1]);
  result.sigma_sky[0][0] = (g00 * scale) * scale;
  result.sigma_sky[0][1] = result.sigma_sky[1][0] = (g01 * scale) * scale;
  result.sigma_sky[1][1] = (g11 * scale) * scale;
  result.azimuth_available = 2 * std::hypot((g00 - g11) / 2, g01) > kRoundoff * svd.sigma1 * svd.sigma1;
  if (result.azimuth_available) {
    result.major_azimuth = 0.5 * std::atan2(2 * g01, g00 - g11);
    if (result.major_azimuth < 0) {
      result.major_azimuth += math::kPiD;
    }
  }
  if (!std::isfinite(result.sigma_major) || !std::isfinite(result.sigma_sky[0][0]) ||
      !std::isfinite(result.sigma_sky[0][1]) || !std::isfinite(result.sigma_sky[1][1])) {
    result.status = SkyBlurStatus::kNonFinite;
    result.error = "sigma_sky is not representable";
    return result;
  }
  result.status = SkyBlurStatus::kOk;
  result.error = "";
  return result;
}

SkyBlurProjection ProjectSkyBlur(const SkyBlurField& field, const double sky_tangent[3]) {
  SkyBlurProjection result;
  if (field.status != SkyBlurStatus::kOk) {
    result.error = "field must be successfully evaluated";
    return result;
  }
  double scale = 0;
  for (int i = 0; i < 3; ++i) {
    if (!sky_tangent || !std::isfinite(sky_tangent[i])) {
      result.error = "sky_tangent must be finite";
      return result;
    }
    scale = std::max(scale, std::abs(sky_tangent[i]));
  }
  if (scale == 0) {
    result.error = "sky_tangent has no direction";
    return result;
  }
  const double t[3]{ sky_tangent[0] / scale, sky_tangent[1] / scale, sky_tangent[2] / scale };
  const double x = so3::Dot3(field.basis[0], t);
  const double y = so3::Dot3(field.basis[1], t);
  const double length = std::hypot(x, y);
  if (length <= kRoundoff * so3::Norm3(t)) {
    result.error = "sky_tangent has no resolved tangent-plane direction";
    return result;
  }
  const double unit[2]{ x / length, y / length };
  auto width = [&](double a, double b) {
    const double angle = field.major_azimuth;
    const double major = a * std::cos(angle) + b * std::sin(angle);
    const double minor = -a * std::sin(angle) + b * std::cos(angle);
    return std::hypot(field.sigma_major * major, field.sigma_minor * minor);
  };
  result.sigma_parallel = width(unit[0], unit[1]);
  result.sigma_perp = width(-unit[1], unit[0]);
  result.available = true;
  result.error = "";
  return result;
}

SkyBlurProjection ProjectSkyBlurOnCurve(const SkyBlurField& field, const PoseCurveNode& node, PoseCurveKind kind) {
  double right[3]{};
  if (kind == PoseCurveKind::kSkyFiber) {
    so3::Copy3(node.tangent, right);
  } else {
    const Jet<1> az = Jet<1>::Variable(node.param[0], 0);
    const Jet<1> zenith = Jet<1>::Variable(node.param[1], 0);
    const Jet<1> roll = Jet<1>::Variable(node.param[2], 0);
    Jet<1> angles[3]{ az, zenith, roll };
    for (int i = 0; i < 3; ++i) {
      angles[i].v[0] = node.tangent[i];
    }
    Jet<1> r[9];
    EulerPose(angles[0], angles[1], angles[2], r);
    double derivative[9], local[9];
    for (int i = 0; i < 9; ++i) {
      derivative[i] = r[i].v[0];
    }
    so3::MatTMul(node.pose, derivative, local);
    so3::SkewVector(local, right);
  }
  double right_scale = 0;
  double j_scale = 0;
  for (int i = 0; i < 3; ++i) {
    if (!std::isfinite(right[i])) {
      return { false, "node.tangent must be finite", 0, 0 };
    }
    right_scale = std::max(right_scale, std::abs(right[i]));
    j_scale = std::max({ j_scale, std::abs(field.j_sky[0][i]), std::abs(field.j_sky[1][i]) });
  }
  if (right_scale == 0 || j_scale == 0) {
    return { false, "curve has no resolved sky tangent", 0, 0 };
  }
  const double unit_right[3]{ right[0] / right_scale, right[1] / right_scale, right[2] / right_scale };
  double tangent[3]{};
  double speed_squared = 0;
  double j_squared = 0;
  for (int a = 0; a < 2; ++a) {
    double speed = 0;
    for (int i = 0; i < 3; ++i) {
      const double j = field.j_sky[a][i] / j_scale;
      speed += j * unit_right[i];
      j_squared += j * j;
    }
    speed_squared += speed * speed;
    for (int i = 0; i < 3; ++i) {
      tangent[i] += speed * field.basis[a][i];
    }
  }
  if (std::sqrt(speed_squared) <= kRoundoff * std::sqrt(j_squared) * so3::Norm3(unit_right)) {
    return { false, "curve has no resolved sky tangent", 0, 0 };
  }
  return ProjectSkyBlur(field, tangent);
}

}  // namespace lumice::analytic
