#include <gtest/gtest.h>

#include <cmath>

#include "analytic/diagnostic_batch.hpp"
#include "analytic/so3.hpp"

namespace lumice::analytic {
namespace {

DiagnosticInputRow Input() {
  DiagnosticInputRow row;
  row.crystal.height = 1;
  for (auto& d : row.crystal.face_distance) {
    d = 1;
  }
  const double w[] = { 0, 0, 1 };
  so3::Exp(w, row.pose.data());
  row.incident = { -std::cos(.35), 0, -std::sin(.35) };
  row.refractive_index = 1.31;
  row.source_token = 73;
  return row;
}

TEST(DiagnosticBatch, ErrorsArePerRowAndDerivativesUseTheActualPose) {
  const auto row = Input();
  auto bad = row;
  bad.pose[0] = 5;
  std::vector<DiagnosticOutputRow> result;
  ASSERT_TRUE(EvaluateDiagnosticBatch({ 3, 5 }, { row, bad, row }, { true, true }, &result));
  ASSERT_EQ(result.size(), 3u);
  ASSERT_TRUE(result[0].path_valid);
  EXPECT_EQ(result[0].source_token, 73u);
  EXPECT_EQ(result[1].input_status, Status::kInvalidValue);
  EXPECT_TRUE(result[2].path_valid);
  EXPECT_EQ(result[0].outgoing, result[2].outgoing);
  EXPECT_TRUE(result[0].direction_jacobian_available);
  EXPECT_TRUE(result[0].direction_index_available);
  EXPECT_GT(result[0].entry.value, 0);
  EXPECT_TRUE(result[0].corridor.geometry_evaluated);
  const auto base = result[0];
  constexpr double kStep = 1e-5;
  for (int axis = 0; axis < 3; ++axis) {
    auto lo = row;
    auto hi = row;
    double w[3]{};
    double rotation[9];
    w[axis] = -kStep;
    so3::Exp(w, rotation);
    so3::MatMul(row.pose.data(), rotation, lo.pose.data());
    w[axis] = kStep;
    so3::Exp(w, rotation);
    so3::MatMul(row.pose.data(), rotation, hi.pose.data());
    if (!EvaluateDiagnosticBatch({ 3, 5 }, { lo, hi }, {}, &result) || !result[0].path_valid || !result[1].path_valid) {
      ADD_FAILURE();
      return;
    }
    for (int j = 0; j < 3; ++j) {
      EXPECT_NEAR(base.direction_pose_jacobian[j][axis], (result[1].outgoing[j] - result[0].outgoing[j]) / (2 * kStep),
                  1e-8);
    }
    for (int slot = 0; slot < 2; ++slot) {
      EXPECT_TRUE(base.interfaces[slot].pose_gradient_available);
      EXPECT_NEAR(base.interfaces[slot].discriminant_pose_gradient[axis],
                  (result[1].interfaces[slot].discriminant - result[0].interfaces[slot].discriminant) / (2 * kStep),
                  1e-8);
    }
  }
  EXPECT_FALSE(EvaluateDiagnosticBatch({ 3 }, { row }, {}, &result));
  EXPECT_TRUE(result.empty());
}

TEST(DiagnosticBatch, LaterFailureDoesNotEraseReachedInterfaceFields) {
  auto row = Input();
  row.pose = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
  FaceNormalTable normals;
  ASSERT_EQ(BuildFaceNormals(row.crystal, &normals), Status::kOk);
  const auto* n = normals.normal[normals.SlotOf(3)];
  row.incident = { -.98 * n[0], -.98 * n[1], -std::sqrt(1 - .98 * .98) };
  std::vector<DiagnosticOutputRow> result;
  ASSERT_TRUE(EvaluateDiagnosticBatch({ 3, 6, 6, 6 }, { row }, { true, false }, &result));
  const auto& value = result[0];
  ASSERT_FALSE(value.path_valid);
  ASSERT_EQ(value.interfaces.size(), 4u);
  EXPECT_TRUE(value.interfaces[0].reached);
  EXPECT_TRUE(value.interfaces[1].reached);
  EXPECT_TRUE(value.interfaces[2].reached);
  EXPECT_FALSE(value.interfaces[3].reached);
  EXPECT_TRUE(value.interfaces[1].pose_gradient_available);
  EXPECT_TRUE(value.interfaces[1].index_derivative_available);
  EXPECT_TRUE(value.interfaces[1].factor_available);
  EXPECT_FALSE(value.interfaces[2].factor_available);
  EXPECT_FALSE(value.interfaces[3].pose_gradient_available);
  EXPECT_FALSE(value.direction_jacobian_available);
  EXPECT_FALSE(value.direction_index_available);
  EXPECT_EQ(value.optical_failure, ChainFailure::kPathInfeasible);
  // A complete rejected row is not a reason to discard an earlier event, nor
  // does an earlier event make the missing outgoing direction available.
  EXPECT_NE(value.interfaces[1].discriminant, 0);
}

}  // namespace
}  // namespace lumice::analytic
