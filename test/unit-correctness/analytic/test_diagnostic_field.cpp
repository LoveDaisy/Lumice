// General diagnostic field: complete per-interface evidence and branch-aware derivatives.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "analytic/diagnostic_field.hpp"
#include "analytic/path_evaluation.hpp"

namespace lumice::analytic {
namespace {

LUMICE_ANALYTIC_Crystal AsymmetricPrism() {
  LUMICE_ANALYTIC_Crystal crystal{};
  crystal.kind = LUMICE_ANALYTIC_CRYSTAL_PRISM;
  crystal.height = 0.73;
  const double distances[6] = { 1.37, 0.91, 1.12, 1.46, 0.83, 1.05 };
  for (int i = 0; i < 6; i++) {
    crystal.face_distance[i] = distances[i];
  }
  return crystal;
}

struct Fixture {
  FaceNormalTable normals;
  FacePolygonTable polygons;
  std::vector<int> slots;
};

Fixture Build(const std::vector<int>& faces) {
  Fixture fixture;
  EXPECT_EQ(BuildFaceNormals(AsymmetricPrism(), &fixture.normals, &fixture.polygons), Status::kOk);
  fixture.slots.resize(faces.size());
  EXPECT_EQ(ResolveFaceSequence(fixture.normals, faces.data(), static_cast<int>(faces.size()), fixture.slots.data()),
            Status::kOk);
  return fixture;
}

DiagnosticRowInput Row(const double pose[9]) {
  DiagnosticRowInput row;
  row.refractive_index = 1.31;
  const double incident[3] = { -0.9659258262890683, 0.0, -0.25881904510252074 };
  for (int i = 0; i < 3; i++) {
    row.incident_direction[i] = incident[i];
  }
  for (int i = 0; i < 9; i++) {
    row.pose[i] = pose[i];
  }
  return row;
}

TEST(DiagnosticField, FourFaceRowReturnsEveryInterfaceAndMargin) {
  const std::vector<int> faces = { 3, 5, 6, 7 };
  Fixture fixture = Build(faces);
  DiagnosticField field(fixture.normals, fixture.polygons, faces.data(), fixture.slots.data(),
                        static_cast<int>(faces.size()));
  const double pose[9] = { 0.5930675096922245, -0.2054499789431593, -0.7784993481691032,
                           0.7535048750192704, 0.4823493488687678,  0.4467320326191859,
                           0.2837275669892805, -0.8515453081299498, 0.4408732878415432 };
  const DiagnosticFieldResult result = field.Evaluate(Row(pose));

  ASSERT_EQ(result.path_status, DiagnosticPathStatus::kOk);
  EXPECT_EQ(result.interfaces.size(), 4u);
  EXPECT_EQ(result.domain_margins.size(), 6u);
  EXPECT_EQ(result.tir_margins.size(), 2u);
  EXPECT_EQ(result.interfaces[0].kind, DiagnosticInterfaceKind::kEntryTransmission);
  EXPECT_EQ(result.interfaces[1].kind, DiagnosticInterfaceKind::kInternalReflection);
  EXPECT_EQ(result.interfaces[3].kind, DiagnosticInterfaceKind::kExitTransmission);
  EXPECT_NEAR(result.outgoing_direction[0], -0.09191560675075156, 2e-12);
  EXPECT_NEAR(result.outgoing_direction[1], 0.34631032018959496, 2e-12);
  EXPECT_NEAR(result.outgoing_direction[2], 0.9336062785595545, 2e-12);
  EXPECT_NEAR(result.interfaces[0].coefficient, 0.9698817572266369, 2e-12);
  EXPECT_NEAR(result.interfaces[1].coefficient, 0.16908062102143795, 2e-12);
  EXPECT_DOUBLE_EQ(result.interfaces[2].coefficient, 1.0);
  EXPECT_NEAR(result.tir_margins[0].value, 0.08515768351850417, 2e-12);
  EXPECT_NEAR(result.tir_margins[1].value, -0.6873127192073827, 2e-12);
  EXPECT_EQ(result.domain_margins.front().name, "entry_incidence_cosine");
  EXPECT_EQ(result.domain_margins.back().name, "exit_snell_discriminant");
  EXPECT_EQ(result.tir_margins[1].name, "internal_2_tir_discriminant");
  EXPECT_TRUE(result.direction_pose_jacobian_available);
  EXPECT_TRUE(result.direction_pose_hessian_available);
  EXPECT_TRUE(result.direction_index_derivative_available);
  for (int component = 0; component < 3; component++) {
    for (int a = 0; a < 3; a++) {
      for (int b = 0; b < 3; b++) {
        EXPECT_NEAR(result.direction_pose_hessian[9 * component + 3 * a + b],
                    result.direction_pose_hessian[9 * component + 3 * b + a], 1e-12);
      }
    }
  }
}

TEST(DiagnosticField, NonFirstTirKinkSuppressesOnlyTheCoefficientDerivative) {
  const std::vector<int> faces = { 1, 5, 2, 3 };
  Fixture fixture = Build(faces);
  DiagnosticField field(fixture.normals, fixture.polygons, faces.data(), fixture.slots.data(),
                        static_cast<int>(faces.size()));
  const double pose[9] = { -0.49760634739842935, -0.8471133033132857,  0.1865126654638934,
                           -0.6613497486591641,  0.23139308033739314,  -0.713494045049034,
                           0.5612525572122176,   -0.47838927007367726, -0.6753808357520372 };
  const DiagnosticFieldResult result = field.Evaluate(Row(pose));

  ASSERT_EQ(result.path_status, DiagnosticPathStatus::kOk);
  ASSERT_EQ(result.tir_margins.size(), 2u);
  EXPECT_LT(result.tir_margins[0].value, -0.2);
  EXPECT_NEAR(result.tir_margins[1].value, 2.8686495105012533e-05, 2e-12);
  EXPECT_TRUE(result.tir_margins[1].pose_derivative_available);
  EXPECT_TRUE(result.direction_pose_jacobian_available);
  EXPECT_TRUE(result.direction_pose_hessian_available);
  EXPECT_FALSE(result.interfaces[2].pose_derivative_available);
  EXPECT_TRUE(result.interfaces[1].pose_derivative_available);
}

TEST(DiagnosticField, InvalidPathKeepsPartialNamedMarginsWithoutDerivatives) {
  const std::vector<int> faces = { 3, 5 };
  Fixture fixture = Build(faces);
  DiagnosticField field(fixture.normals, fixture.polygons, faces.data(), fixture.slots.data(),
                        static_cast<int>(faces.size()));
  const double identity[9] = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };
  DiagnosticRowInput input = Row(identity);
  for (double& value : input.incident_direction) {
    value = -value;
  }
  const DiagnosticFieldResult result = field.Evaluate(input);

  EXPECT_EQ(result.path_status, DiagnosticPathStatus::kPathInfeasible);
  EXPECT_EQ(result.entry_status, DiagnosticEntryStatus::kEntryBackface);
  ASSERT_EQ(result.domain_margins.size(), 4u);
  EXPECT_LT(result.domain_margins[0].value, 0.0);
  EXPECT_TRUE(std::isfinite(result.domain_margins[1].value));
  EXPECT_TRUE(std::isnan(result.domain_margins[2].value));
  EXPECT_FALSE(result.direction_pose_jacobian_available);
  EXPECT_FALSE(result.direction_pose_hessian_available);
  EXPECT_FALSE(result.direction_index_derivative_available);
}

TEST(DiagnosticFieldCapi, MixedRowsOwnIndependentVariableResults) {
  const LUMICE_ANALYTIC_Crystal crystal = AsymmetricPrism();
  const int faces[4] = { 3, 5, 6, 7 };
  const double pose[9] = { 0.5930675096922245, -0.2054499789431593, -0.7784993481691032,
                           0.7535048750192704, 0.4823493488687678,  0.4467320326191859,
                           0.2837275669892805, -0.8515453081299498, 0.4408732878415432 };
  const DiagnosticRowInput source = Row(pose);
  LUMICE_ANALYTIC_DiagnosticFieldRow rows[3]{};
  for (auto& row : rows) {
    row.refractive_index = source.refractive_index;
    std::copy(source.incident_direction, source.incident_direction + 3, row.incident_direction);
    std::copy(source.pose, source.pose + 9, row.pose);
  }
  rows[1].refractive_index = 0.0;
  rows[2].refractive_index += 0.01;
  LUMICE_ANALYTIC_DiagnosticFieldResult results[3]{};
  for (auto& result : results) {
    result.struct_size = sizeof(result);
  }

  EXPECT_EQ(LUMICE_ANALYTIC_EvaluateDiagnosticFieldBatch(&crystal, faces, 4, rows, 3, results), LUMICE_ANALYTIC_OK);
  EXPECT_EQ(results[0].row_error, LUMICE_ANALYTIC_OK);
  EXPECT_EQ(results[0].path_status, LUMICE_ANALYTIC_DIAGNOSTIC_PATH_OK);
  ASSERT_EQ(results[0].interface_count, 4);
  EXPECT_EQ(results[0].interfaces[2].interface_index, 2);
  EXPECT_EQ(results[0].interfaces[2].face_number, 6);
  EXPECT_EQ(results[0].interfaces[2].kind, LUMICE_ANALYTIC_DIAGNOSTIC_INTERNAL_REFLECTION);
  ASSERT_EQ(results[0].domain_margin_count, 6);
  EXPECT_STREQ(results[0].domain_margins[0].name, "entry_incidence_cosine");
  ASSERT_EQ(results[0].tir_margin_count, 2);
  EXPECT_STREQ(results[0].tir_margins[1].name, "internal_2_tir_discriminant");
  EXPECT_NE(results[0].storage, nullptr);
  EXPECT_EQ(results[1].row_error, LUMICE_ANALYTIC_ERR_INVALID_VALUE);
  EXPECT_EQ(results[1].interface_count, 0);
  EXPECT_EQ(results[1].storage, nullptr);
  EXPECT_EQ(results[2].row_error, LUMICE_ANALYTIC_OK);
  EXPECT_NE(results[2].storage, nullptr);

  for (auto& result : results) {
    LUMICE_ANALYTIC_ReleaseDiagnosticFieldResult(&result);
    EXPECT_EQ(result.storage, nullptr);
    EXPECT_EQ(result.interface_count, 0);
    LUMICE_ANALYTIC_ReleaseDiagnosticFieldResult(&result);
  }
}

TEST(DiagnosticFieldCapi, BatchValidationPreservesWalkableReleaseState) {
  EXPECT_EQ(LUMICE_ANALYTIC_EvaluateDiagnosticFieldBatch(nullptr, nullptr, 0, nullptr, 0, nullptr), LUMICE_ANALYTIC_OK);

  const int faces[2] = { 3, 5 };
  const LUMICE_ANALYTIC_Crystal crystal = AsymmetricPrism();
  const double identity[9] = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };
  const DiagnosticRowInput source = Row(identity);
  LUMICE_ANALYTIC_DiagnosticFieldRow rows[2]{};
  for (auto& row : rows) {
    row.refractive_index = source.refractive_index;
    std::copy(source.incident_direction, source.incident_direction + 3, row.incident_direction);
    std::copy(source.pose, source.pose + 9, row.pose);
  }
  LUMICE_ANALYTIC_DiagnosticFieldResult results[2]{};
  results[0].struct_size = sizeof(results[0]);
  results[1].struct_size = sizeof(results[1]) - sizeof(void*);
  EXPECT_EQ(LUMICE_ANALYTIC_EvaluateDiagnosticFieldBatch(&crystal, faces, 2, rows, 2, results),
            LUMICE_ANALYTIC_ERR_INVALID_VALUE);
  EXPECT_EQ(results[0].storage, nullptr);
  EXPECT_EQ(results[0].row_error, LUMICE_ANALYTIC_OK);

  results[1].struct_size = sizeof(results[1]);
  const int bad_faces[2] = { 3, 99 };
  EXPECT_EQ(LUMICE_ANALYTIC_EvaluateDiagnosticFieldBatch(&crystal, bad_faces, 2, rows, 2, results),
            LUMICE_ANALYTIC_ERR_INVALID_VALUE);
  for (auto& result : results) {
    EXPECT_EQ(result.storage, nullptr);
    EXPECT_EQ(result.row_error, LUMICE_ANALYTIC_OK);
    LUMICE_ANALYTIC_ReleaseDiagnosticFieldResult(&result);
  }
}

}  // namespace
}  // namespace lumice::analytic
