#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "analytic/family_rank.hpp"
#include "analytic/path_chain.hpp"
#include "analytic/sky_blur_field.hpp"
#include "core/random.hpp"

namespace lumice::analytic {
namespace {
constexpr double kPi = math::kPiD;
constexpr double kEps = std::numeric_limits<double>::epsilon();
constexpr double kTol = 256 * kEps;

AxisDistribution PoleLaw(float spread = 1) {
  AxisDistribution law;
  law.azimuth_dist = { DistributionType::kUniform, 0, 360 };
  law.latitude_dist = { DistributionType::kGaussianLegacy, 90, spread };
  law.roll_dist = { DistributionType::kUniform, 0, 360 };
  return law;
}

FaceNormalTable Prism() {
  CrystalShape crystal;
  crystal.kind = CrystalShapeKind::kPrism;
  crystal.height = 1;
  for (double& distance : crystal.face_distance) {
    distance = 1;
  }
  FaceNormalTable table;
  EXPECT_EQ(BuildFaceNormals(crystal, &table), Status::kOk);
  return table;
}

std::array<double, 9> WorldCovariance(const double pose[9], const std::array<double, 9>& body) {
  double temp[9];
  std::array<double, 9> result{};
  so3::MatMul(pose, body.data(), temp);
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      for (int k = 0; k < 3; ++k) {
        result[3 * i + j] += temp[3 * i + k] * pose[3 * j + k];
      }
    }
  }
  return result;
}

double Difference(const std::array<double, 9>& a, const std::array<double, 9>& b) {
  double sum = 0;
  for (int i = 0; i < 9; ++i) {
    sum += (a[i] - b[i]) * (a[i] - b[i]);
  }
  return std::sqrt(sum);
}

TEST(SkyBlurField, PoleDecompositionIdentitiesAndRightChart) {
  const auto law = PoleLaw(3);
  const auto variances = AxisPoseDensity(law).Variances();
  const double v = variances.sigma2_lat;
  const double mu2 = AxisZenithPoleRawMoment(law, true);
  const double s = law.latitude_dist.spread * math::kDegreeToRadD;
  const double mu1 = s * std::sqrt(2 / kPi);
  EXPECT_NEAR(mu2, s * s, kTol);
  EXPECT_NEAR(v, s * s * (1 - 2 / kPi), kTol);
  EXPECT_NEAR(mu2, v + mu1 * mu1, kTol);
  EXPECT_NEAR(mu2 / v, 2.751938393884109, 1e-9);
  const std::array<double, 9> centered{ v / 2, 0, 0, 0, v / 2, 0, 0, 0, 0 };
  const std::array<double, 9> assigned{ mu2 / 2, 0, 0, 0, mu2 / 2, 0, 0, 0, 0 };
  EXPECT_NEAR(Difference(centered, assigned), mu1 * mu1 / std::sqrt(2.0), kTol);
  for (double zenith : { 1e-3, 1e-6, 1e-9, 0.0 }) {
    std::array<double, 9> average{};
    for (int ai = 0; ai < 8; ++ai) {
      const double az = 2 * kPi * ai / 8;
      double pose[9];
      EulerPose(az, zenith, 0.2, pose);
      const auto covariance = AxisPoseCovariance(law, pose);
      if (covariance.status != SkyBlurStatus::kOk) {
        ADD_FAILURE() << covariance.error;
        continue;
      }
      EXPECT_EQ(covariance.pole_marginal, zenith == 0);
      const auto world = WorldCovariance(pose, covariance.sigma_pose);
      auto latitude = world;
      for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
          const double roll_block = variances.sigma2_roll * pose[3 * i + 2] * pose[3 * j + 2];
          latitude[3 * i + j] -= roll_block;
          average[3 * i + j] += world[3 * i + j] / 8;
        }
      }
      latitude[8] -= variances.sigma2_az;
      if (zenith == 0) {
        EXPECT_LT(Difference(latitude, assigned), kTol);
      } else {
        EXPECT_NEAR(Difference(latitude, centered), v / std::sqrt(2.0), kTol);
        EXPECT_NEAR(Difference(latitude, assigned), std::sqrt(v * v - v * mu2 + mu2 * mu2 / 2), kTol);
        const double node_line[3]{ -std::sin(az), std::cos(az), 0 };
        for (int i = 0; i < 3; ++i) {
          for (int j = 0; j < 3; ++j) {
            EXPECT_NEAR(latitude[3 * i + j], v * node_line[i] * node_line[j], kTol);
          }
        }
      }
    }
    auto closure = zenith == 0 ? assigned : centered;
    closure[8] = variances.sigma2_az + variances.sigma2_roll;
    const double roll_gap = zenith == 0 ? 0 : std::sqrt(1.5) * variances.sigma2_roll * std::pow(std::sin(zenith), 2);
    EXPECT_NEAR(Difference(average, closure), roll_gap, kTol);
  }
  // Independently differentiate the production Euler chart, then read vee(R^T dR).
  double pose[9];
  EulerPose(0.7, 0.8, 0.2, pose);
  const auto actual = AxisPoseCovariance(law, pose);
  ASSERT_EQ(actual.status, SkyBlurStatus::kOk);
  using J = Jet<3>;
  J r[9];
  EulerPose(J::Variable(0.7, 0), J::Variable(0.8, 1), J::Variable(0.2, 2), r);
  std::array<double, 9> expected{};
  const double weights[3]{ variances.sigma2_az, v, variances.sigma2_roll };
  for (int axis = 0; axis < 3; ++axis) {
    double derivative[9], local[9], omega[3];
    for (int i = 0; i < 9; ++i) {
      derivative[i] = r[i].v[axis];
    }
    so3::MatTMul(pose, derivative, local);
    so3::SkewVector(local, omega);
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        expected[3 * i + j] += weights[axis] * omega[i] * omega[j];
      }
    }
  }
  EXPECT_LT(Difference(expected, actual.sigma_pose), kTol);
}

TEST(SkyBlurField, RawPoleMomentAndRefusalDomain) {
  auto law = PoleLaw();
  law.latitude_dist = { DistributionType::kUniform, 0, 360 };
  EXPECT_NEAR(AxisZenithPoleRawMoment(law, true), kPi * kPi / 2 - 2, 1e-11);
  EXPECT_NEAR(AxisZenithPoleRawMoment(law, false), kPi * kPi / 2 - 2, 1e-11);
  law.latitude_dist = { DistributionType::kNoRandom, -90, 0 };
  EXPECT_NEAR(AxisZenithPoleRawMoment(law, false), 0, kTol);
  EXPECT_NEAR(AxisZenithPoleRawMoment(law, true), kPi * kPi, kTol);
  // All nonlegacy laws use the module's area-normalized measure, not raw proposal moments.
  for (auto type : { DistributionType::kGaussian, DistributionType::kLaplacian, DistributionType::kZigzag }) {
    law.latitude_dist = { type, 90, 3 };
    const double moment = AxisZenithPoleRawMoment(law, true);
    EXPECT_GT(moment, 0);
    EXPECT_LT(moment, 0.02);
  }
  double pose[9];
  EulerPose(0.7, kPi, 0.2, pose);
  law = PoleLaw();
  law.latitude_dist.center = -90;
  EXPECT_TRUE(AxisPoseCovariance(law, pose).pole_marginal);
  for (double zenith : { 0.0, 1e-9, 1e-6 }) {
    EulerPose(0.7, zenith, 0.2, pose);
    law.azimuth_dist = { DistributionType::kGaussian, 40, 3 };
    // With nonpolar latitude, moments have a defined circular mean; test only the chart refusal.
    law.latitude_dist = { DistributionType::kNoRandom, 30, 0 };
    const auto result = AxisPoseCovariance(law, pose);
    EXPECT_EQ(result.status, zenith < std::sqrt(kEps) ? SkyBlurStatus::kUnsupportedAzimuth : SkyBlurStatus::kOk);
  }
  law.latitude_dist.spread = -1;
  law.latitude_dist.type = DistributionType::kGaussian;
  EXPECT_THROW(AxisZenithPoleRawMoment(law, true), std::invalid_argument);
  EXPECT_NE(std::string(AxisPoseCovariance(law, pose).error).find("latitude_dist.spread"), std::string::npos);
}

TEST(SkyBlurField, JetVersusCentralSkyDisplacementsSceneMatrix) {
  const auto table = Prism();
  const double incident[3]{ -std::cos(20 * math::kDegreeToRadD), 0, -std::sin(20 * math::kDegreeToRadD) };
  const std::array<double, 9> covariance{ 0.01, 0, 0, 0, 0.02, 0, 0, 0, 0.03 };
  const double step = std::cbrt(kEps);
  const int face_sets[3][3]{ { 3, 5, 0 }, { 3, 5, 0 }, { 3, 1, 5 } };
  for (int scene = 0; scene < 3; ++scene) {
    const int count = scene == 2 ? 3 : 2;
    int slots[3];
    if (ResolveFaceSequence(table, face_sets[scene], count, slots) != Status::kOk) {
      ADD_FAILURE();
      continue;
    }
    double pose[9];
    // C16 column / plate parhelion; C02 internal-basal reflection near its TIR diagnostic.
    EulerPoseDeg(237.7, scene == 0 ? 90 : 0, 0, pose);
    if (scene == 2) {
      double closest = 1;
      double candidate[9];
      for (int az = 0; az < 360; ++az) {
        EulerPoseDeg(az + 0.3, 90, 0, candidate);
        double out[3];
        ChainInterfaceDiagnostics<double> diagnostics;
        if (TracePathChain(table, slots, count, 1.31, incident, candidate, out, nullptr, nullptr, &diagnostics) &&
            std::abs(diagnostics.discriminant[1]) < closest) {
          closest = std::abs(diagnostics.discriminant[1]);
          std::copy(candidate, candidate + 9, pose);
        }
      }
      EXPECT_LT(closest, 0.05);
    }
    const auto field = PushforwardSkyBlur(table, slots, count, 1.31, incident, pose, covariance);
    if (field.status != SkyBlurStatus::kOk) {
      ADD_FAILURE() << "scene " << scene << ": " << field.error;
      continue;
    }
    for (int axis = 0; axis < 3; ++axis) {
      double sky[2][2][3];
      bool valid = true;
      for (int level = 0; level < 2; ++level) {
        for (int side = 0; side < 2; ++side) {
          double omega[3]{}, perturb[9], rotation[9], outgoing[3]{};
          omega[axis] = (side == 0 ? -step : step) / (level == 0 ? 1 : 2);
          so3::Exp(omega, perturb);
          so3::MatMul(pose, perturb, rotation);
          valid = TracePathChain(table, slots, count, 1.31, incident, rotation, outgoing, nullptr, nullptr) && valid;
          for (int i = 0; i < 3; ++i) {
            sky[level][side][i] = -outgoing[i];
          }
        }
      }
      if (!valid) {
        ADD_FAILURE() << "finite difference left optical domain";
        continue;
      }
      for (int a = 0; a < 2; ++a) {
        double central[2]{};
        for (int level = 0; level < 2; ++level) {
          for (int i = 0; i < 3; ++i) {
            central[level] += field.basis[a][i] * (sky[level][1][i] - sky[level][0][i]) / 2;
          }
        }
        // Richardson-combined CENTRAL differences cancel the O(step^3) sky displacement
        // term. Remaining truncation is O(step^5), leaving O(eps*kappa_eval) evaluation
        // roundoff on sky displacements; no reciprocal singular values or relaxed ruler.
        const double displacement = (8 * central[1] - central[0]) / 3;
        EXPECT_NEAR(displacement, step * field.j_sky[a][axis], kTol * (1 + std::abs(field.j_sky[a][axis])));
      }
    }
  }
}

TEST(SkyBlurField, CovariancePrincipalAxesProjectionAndRankLoss) {
  const auto table = Prism();
  const int slot = table.SlotOf(1);
  const double incident[3]{ 0, 0, -1 };
  PoseCurveNode node;
  node.param[0] = 0.7;
  node.param[1] = 0.4;
  node.param[2] = 0.2;
  node.tangent[0] = 1;
  EulerPose(node.param[0], node.param[1], node.param[2], node.pose);
  const std::array<double, 9> covariance{ 0.04, 0.012, -0.003, 0.012, 0.02, 0.002, -0.003, 0.002, 0.01 };
  const auto field = PushforwardSkyBlur(table, &slot, 1, 1.31, incident, node.pose, covariance);
  ASSERT_EQ(field.status, SkyBlurStatus::kOk);
  EXPECT_GE(field.sigma_major, field.sigma_minor);
  EXPECT_TRUE(field.azimuth_available);
  for (int a = 0; a < 2; ++a) {
    for (int b = 0; b < 2; ++b) {
      double quadratic = 0;
      for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
          quadratic += field.j_sky[a][i] * covariance[3 * i + j] * field.j_sky[b][j];
        }
      }
      EXPECT_NEAR(field.sigma_sky[a][b], quadratic, kTol);
      const double major[2]{ std::cos(field.major_azimuth), std::sin(field.major_azimuth) };
      const double minor[2]{ -major[1], major[0] };
      EXPECT_NEAR(
          quadratic,
          std::pow(field.sigma_major, 2) * major[a] * major[b] + std::pow(field.sigma_minor, 2) * minor[a] * minor[b],
          kTol);
    }
  }
  const double tangent[3]{ 0.3 * field.basis[0][0] + 0.8 * field.basis[1][0],
                           0.3 * field.basis[0][1] + 0.8 * field.basis[1][1],
                           0.3 * field.basis[0][2] + 0.8 * field.basis[1][2] };
  const auto projection = ProjectSkyBlur(field, tangent);
  ASSERT_TRUE(projection.available);
  const auto& c = field.sigma_sky;
  EXPECT_NEAR(std::pow(projection.sigma_parallel, 2), (0.09 * c[0][0] + 0.48 * c[0][1] + 0.64 * c[1][1]) / 0.73, kTol);
  EXPECT_NEAR(std::pow(projection.sigma_perp, 2), (0.64 * c[0][0] - 0.48 * c[0][1] + 0.09 * c[1][1]) / 0.73, kTol);
  const auto curve_projection = ProjectSkyBlurOnCurve(field, node, PoseCurveKind::kFamily1D);
  ASSERT_TRUE(curve_projection.available);
  // For vertical incident light, azimuth sweeps a sky latitude circle: tangent is z x sky.
  const double curve_tangent[3]{ -field.sky[1], field.sky[0], 0 };
  const auto direct = ProjectSkyBlur(field, curve_tangent);
  EXPECT_NEAR(curve_projection.sigma_parallel, direct.sigma_parallel, kTol);
  EXPECT_NEAR(curve_projection.sigma_perp, direct.sigma_perp, kTol);
  node.tangent[0] = 0;
  node.tangent[2] = 1;  // Body-e3 spin cannot move a basal reflection image.
  EXPECT_FALSE(ProjectSkyBlurOnCurve(field, node, PoseCurveKind::kSkyFiber).available);
  const std::array<double, 9> rank_one{ 0.01, 0, 0, 0, 0, 0, 0, 0, 0 };
  const auto rank = PushforwardSkyBlur(table, &slot, 1, 1.31, incident, node.pose, rank_one);
  ASSERT_EQ(rank.status, SkyBlurStatus::kOk);
  EXPECT_GT(rank.sigma_major, 0.01);
  EXPECT_LT(rank.sigma_minor, kTol);
  const auto zero = PushforwardSkyBlur(table, &slot, 1, 1.31, incident, node.pose, {});
  EXPECT_EQ(zero.status, SkyBlurStatus::kOk);
  EXPECT_FALSE(zero.azimuth_available);
  EXPECT_NEAR(zero.sigma_major, 0, kTol);
  EXPECT_FALSE(ProjectSkyBlur(field, field.sky).available);
  const double null_tangent[3]{};
  EXPECT_FALSE(ProjectSkyBlur(field, null_tangent).available);
}

TEST(SkyBlurField, EquivalentPoleChartsAndInputValidation) {
  const auto table = Prism();
  const int slot = table.SlotOf(1);
  const double incident[3]{ 0, 0, -1 };
  double pose[9], equivalent[9];
  EulerPose(0.7, 0.0, 0.2, pose);
  EulerPose(1.1, 0.0, -0.2, equivalent);
  const auto a = AxisPoseCovariance(PoleLaw(), pose);
  const auto b = AxisPoseCovariance(PoleLaw(), equivalent);
  ASSERT_EQ(a.status, SkyBlurStatus::kOk);
  ASSERT_EQ(b.status, SkyBlurStatus::kOk);
  EXPECT_LT(Difference(a.sigma_pose, b.sigma_pose), kTol);
  const auto fa = PushforwardSkyBlur(table, &slot, 1, 1.31, incident, pose, a.sigma_pose);
  const auto fb = PushforwardSkyBlur(table, &slot, 1, 1.31, incident, equivalent, b.sigma_pose);
  ASSERT_EQ(fa.status, SkyBlurStatus::kOk);
  ASSERT_EQ(fb.status, SkyBlurStatus::kOk);
  EXPECT_NEAR(fa.sigma_major, fb.sigma_major, kTol);
  EXPECT_NEAR(fa.sigma_minor, fb.sigma_minor, kTol);
  EXPECT_FALSE(fa.azimuth_available);
  // Change the BODY coordinate basis, not the physical scene: R'=R Q, n'=Q^T n,
  // Sigma'=Q^T Sigma Q. This moves e3 off the chart pole but must preserve the sky tensor.
  const double omega[3]{ 0.3, -0.2, 0.1 };
  double q[9], transformed_pose[9], temp[9];
  so3::Exp(omega, q);
  so3::MatMul(pose, q, transformed_pose);
  auto transformed_table = table;
  for (int i = 0; i < table.slot_cnt; ++i) {
    WorldToBody(q, table.normal[i], transformed_table.normal[i]);
  }
  so3::MatTMul(q, a.sigma_pose.data(), temp);
  std::array<double, 9> transformed_covariance{};
  so3::MatMul(temp, q, transformed_covariance.data());
  const auto transformed =
      PushforwardSkyBlur(transformed_table, &slot, 1, 1.31, incident, transformed_pose, transformed_covariance);
  ASSERT_EQ(transformed.status, SkyBlurStatus::kOk);
  EXPECT_NEAR(transformed.sigma_major, fa.sigma_major, kTol);
  EXPECT_NEAR(transformed.sigma_minor, fa.sigma_minor, kTol);
  for (int i = 0; i < 3; ++i) {
    EXPECT_NEAR(transformed.sky[i], fa.sky[i], kTol);
  }
  auto bad = a.sigma_pose;
  bad[0] = -1;
  EXPECT_NE(std::string(ValidateSkyBlurParams(table, &slot, 1, 1.31, incident, pose, bad)).find("sigma_pose"),
            std::string::npos);
  bad = a.sigma_pose;
  bad[1] = 1;
  EXPECT_EQ(PushforwardSkyBlur(table, &slot, 1, 1.31, incident, pose, bad).status, SkyBlurStatus::kInvalidInput);
  EXPECT_NE(std::string(ValidateSkyBlurParams(table, nullptr, 0, 1.31, incident, pose, {})).find("slots"),
            std::string::npos);
  EXPECT_NE(std::string(ValidateSkyBlurParams(table, &slot, 1, -1, incident, pose, {})).find("refractive_index"),
            std::string::npos);
  pose[0] = std::numeric_limits<double>::quiet_NaN();
  EXPECT_EQ(AxisPoseCovariance(PoleLaw(), pose).status, SkyBlurStatus::kInvalidInput);
  EXPECT_EQ(PushforwardSkyBlur(table, &slot, 1, 1.31, incident, equivalent, {}).status, SkyBlurStatus::kOk);
  const double backlit[3]{ 0, 0, 1 };
  EXPECT_EQ(PushforwardSkyBlur(table, &slot, 1, 1.31, backlit, equivalent, {}).status, SkyBlurStatus::kPathInvalid);
}

TEST(SkyBlurField, IndependentEngineSamplerMarginalSkyCovariance) {
  constexpr size_t kSamples = 200000;
  const auto law = PoleLaw(1);
  const auto table = Prism();
  const int slot = table.SlotOf(1);
  const double incident[3]{ 0, 0, -1 };
  double pole[9];
  EulerPose(0.7, 0.0, 0.2, pole);
  const auto covariance = AxisPoseCovariance(law, pole);
  ASSERT_EQ(covariance.status, SkyBlurStatus::kOk);
  const auto field = PushforwardSkyBlur(table, &slot, 1, 1.31, incident, pole, covariance.sigma_pose);
  ASSERT_EQ(field.status, SkyBlurStatus::kOk);
  std::vector<float> samples(3 * kSamples);
  RandomNumberGenerator::GetInstance().SetSeed(0x187ad23u);
  RandomSampler::SampleSphericalPointsSph(law, samples.data(), kSamples);
  double sum[2]{}, outer[2][2]{}, tilt2 = 0;
  size_t valid = 0;
  for (size_t i = 0; i < kSamples; ++i) {
    const double tilt = kPi / 2 - samples[3 * i + 1];
    tilt2 += tilt * tilt;
    double pose[9], outgoing[3];
    EulerPose(static_cast<double>(samples[3 * i]), tilt, static_cast<double>(samples[3 * i + 2]), pose);
    if (!TracePathChain(table, &slot, 1, 1.31, incident, pose, outgoing, nullptr, nullptr)) {
      continue;
    }
    ++valid;
    const double point[2]{ -so3::Dot3(field.basis[0], outgoing), -so3::Dot3(field.basis[1], outgoing) };
    for (int a = 0; a < 2; ++a) {
      sum[a] += point[a];
      for (int b = 0; b < 2; ++b) {
        outer[a][b] += point[a] * point[b];
      }
    }
  }
  ASSERT_EQ(valid, kSamples);
  const double predicted_trace = field.sigma_sky[0][0] + field.sigma_sky[1][1];
  double measured_trace = 0;
  for (int a = 0; a < 2; ++a) {
    for (int b = 0; b < 2; ++b) {
      const double measured = outer[a][b] / kSamples - (sum[a] / kSamples) * (sum[b] / kSamples);
      // Law-based band: >6 standard errors for component variances plus 0.13% nonlinear
      // bias at 1 degree. stdlib seed streams may differ; no assertion pins sampled values.
      EXPECT_NEAR(measured / predicted_trace, field.sigma_sky[a][b] / predicted_trace, 0.015);
      if (a == b) {
        measured_trace += measured;
      }
    }
  }
  EXPECT_NEAR(measured_trace / predicted_trace, 1, 0.025);
  // Remove sampling error with the CAPTURED sample's own raw tilt moment: exact reflection
  // has sin(2T), so its trace / (4 E_sample[T^2]) = 1-4s^2+O(s^4), not the old 2.75 ratio.
  const double s = law.latitude_dist.spread * math::kDegreeToRadD;
  const double observed_ratio = measured_trace / (4 * tilt2 / kSamples);
  // The band includes >7 sigma of the captured fourth/second-moment ratio and the
  // O(1/N) empirical vector-mean subtraction (stdlib-dependent engine samples).
  EXPECT_NEAR(observed_ratio, 1 - 4 * s * s, 0.3 * s * s);
  RecordProperty("samples", static_cast<int>(kSamples));
  RecordProperty("raw_prediction_trace_ratio", measured_trace / predicted_trace);
  RecordProperty("captured_raw_nonlinear_ratio", observed_ratio);
}

}  // namespace
}  // namespace lumice::analytic
