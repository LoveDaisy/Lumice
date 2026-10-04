#include <gtest/gtest.h>

#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>

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

TEST(DiagnosticBatch, DeviationMinimumUsesTheDirectionMapNotTheObservedPeak) {
  auto row = Input();
  const auto deadline = std::chrono::steady_clock::time_point::max();
  const auto minimum = CorrectDeviationMinimum({ 3, 5 }, row, {}, 1024, deadline);
  ASSERT_EQ(minimum.status, InterfaceSolveStatus::kConverged);
  const double expected = 2 * std::asin(row.refractive_index * .5) - 3.14159265358979323846 / 3;
  EXPECT_NEAR(minimum.deviation_rad, expected, 1e-10);
  EXPECT_GT(minimum.value.entry.value, 0);
  EXPECT_GT(minimum.objective_curvatures[0], 2 * minimum.hessian_error);
  EXPECT_LT(minimum.correction_rad, 1e-8);
  EXPECT_LE(minimum.path_evaluations, 1024u);
  EXPECT_EQ(minimum.source.source_token, row.source_token);
  std::vector<DiagnosticOutputRow> replay;
  ASSERT_TRUE(EvaluateDiagnosticBatch({ 3, 5 }, { minimum.source }, {}, &replay));
  EXPECT_EQ(replay[0].outgoing, minimum.value.outgoing);
  const auto limited = CorrectDeviationMinimum({ 3, 5 }, row, { 1e-8, .1, 1 }, 1024, deadline);
  EXPECT_EQ(limited.status, InterfaceSolveStatus::kIterationLimit);
  EXPECT_EQ(limited.source.pose, row.pose);
  const auto expired = CorrectDeviationMinimum({ 3, 5 }, row, {}, 1024, std::chrono::steady_clock::now());
  EXPECT_EQ(expired.status, InterfaceSolveStatus::kBudgetExceeded);
  EXPECT_EQ(expired.path_evaluations, 0u);
  const auto low = CorrectDeviationMinimum({ 3, 5 }, row, {}, 5, deadline);
  EXPECT_EQ(low.status, InterfaceSolveStatus::kBudgetExceeded);
  EXPECT_EQ(low.path_evaluations, 0u);
}

TEST(DiagnosticBatch, NonreferenceNormalsSetTheirOwnDeviationEdge) {
  auto row = Input();
  row.crystal.kind = CrystalShapeKind::kPyramid;
  row.crystal.height = .73f;
  row.crystal.upper_h = row.crystal.lower_h = .7f;
  row.crystal.upper_wedge_deg = 25;
  row.crystal.lower_wedge_deg = 34;
  row.incident = { -std::cos(3.14159265358979323846 / 9), 0, -std::sin(3.14159265358979323846 / 9) };
  row.pose = { 0.91448860974778201,  -0.40161343762916757, -0.04916532677865465,
               -0.13052025871311235, -0.40782775363497287, 0.90368190500336698,
               -0.38298178116854503, -0.81998973778989126, -0.42537252522022839 };
  const double delta[]{ .02, -.01, .03 };
  double rotation[9];
  so3::Exp(delta, rotation);
  const auto initial = row.pose;
  so3::MatMul(initial.data(), rotation, row.pose.data());
  const auto minimum = CorrectDeviationMinimum({ 13, 15 }, row, {}, 1024, std::chrono::steady_clock::time_point::max());
  ASSERT_EQ(minimum.status, InterfaceSolveStatus::kConverged);
  // Independent plane geometry: adjacent upper normals have a 120-degree
  // azimuth gap, and their common axial component is sin(wedge).
  const double axial = std::sin(25 * 3.14159265358979323846 / 180);
  const double prism_angle = std::acos(.5 - 1.5 * axial * axial);
  EXPECT_NEAR(minimum.deviation_rad, 2 * std::asin(row.refractive_index * std::sin(prism_angle / 2)) - prism_angle,
              1e-10);
  EXPECT_GT(minimum.value.entry.value, 0);
  EXPECT_GT(minimum.deviation_rad, .5);
}

TEST(DiagnosticBatch, DeepInterfaceCorrectionKeepsPositiveSourcesAndBudget) {
  std::ifstream in(std::string(LUMICE_DIAGNOSTIC_FIXTURE_DIR) + "/internal-event.json");
  ASSERT_TRUE(in.good());
  const auto fixture = nlohmann::json::parse(in);
  for (const auto& item : fixture.at("cases")) {
    const auto& shape = item.at("shape");
    DiagnosticInputRow row;
    row.crystal.kind = CrystalShapeKind::kPyramid;
    row.crystal.height = shape.at("height");
    row.crystal.upper_h = shape.at("upper_h");
    row.crystal.lower_h = shape.at("lower_h");
    row.crystal.upper_wedge_deg = shape.at("upper_wedge_deg");
    row.crystal.lower_wedge_deg = shape.at("lower_wedge_deg");
    for (int j = 0; j < 6; ++j) {
      row.crystal.face_distance[j] = shape.at("face_distance").at(j);
    }
    row.refractive_index = item.at("refractive_index");
    row.pose = item.at("seed_pose").get<std::array<double, 9>>();
    row.incident = item.at("incident").get<std::array<double, 3>>();
    row.source_token = 76;
    const auto faces = item.at("faces").get<std::vector<int>>();
    const int slot = item.at("internal_slot");
    const auto deadline = std::chrono::steady_clock::time_point::max();
    const auto event = CorrectInterfaceEvent(faces, row, { slot }, 192, deadline);
    if (event.status != InterfaceSolveStatus::kConverged) {
      ADD_FAILURE() << static_cast<int>(event.status);
      return;
    }
    std::ifstream contacts_in(std::string(LUMICE_DIAGNOSTIC_FIXTURE_DIR) + "/interface-contact.json");
    if (!contacts_in.good()) {
      ADD_FAILURE();
      return;
    }
    const auto contacts = nlohmann::json::parse(contacts_in);
    for (const bool reverse : { false, true }) {
      const auto curve = TraceInterfaceCurve(faces, event.source, slot, .01, 1e-7, 256, reverse, 40000, deadline);
      if (curve.stop != InterfaceWalkStop::kGeometricContact || curve.events.size() != 2) {
        ADD_FAILURE() << "stop=" << static_cast<int>(curve.stop) << " events=" << curve.events.size()
                      << " points=" << curve.points.size();
        return;
      }
      EXPECT_EQ(curve.events[0].kind, InterfaceWalkStop::kAreaThreshold);
      EXPECT_EQ(curve.events[1].kind, InterfaceWalkStop::kGeometricContact);
      EXPECT_GT(curve.events[0].positive.value.entry.value, 0);
      EXPECT_EQ(curve.events[0].nonpositive.value.entry.value, 0);
      EXPECT_GT(curve.events[0].nonpositive.value.corridor.raw_area, 0);
      EXPECT_GT(curve.events[1].positive.value.corridor.raw_area, 0);
      EXPECT_EQ(curve.events[1].positive.value.entry.value, 0);
      EXPECT_EQ(curve.events[1].nonpositive.value.corridor.raw_area, 0);
      for (const auto& bracket : curve.events) {
        EXPECT_LE(bracket.source_width_rad, 1e-7);
        EXPECT_NEAR(bracket.positive.value.interfaces[slot].discriminant, 0, 1e-10);
        double u[3];
        chain_detail::WorldToBody(bracket.positive.source.pose.data(), row.incident.data(), u);
        for (auto& component : u) {
          component = -component;
        }
        double nearest = 10;
        for (const auto& reference : contacts.at("events")) {
          if (std::abs(reference.at("scope").at("refractive_index").get<double>() - row.refractive_index) > 1e-12) {
            continue;
          }
          const auto q = reference.at(bracket.kind == InterfaceWalkStop::kGeometricContact ? "exact_u" : "product_u")
                             .get<std::array<double, 3>>();
          double cross[3];
          so3::Cross3(u, q.data(), cross);
          nearest = std::min(nearest, std::atan2(so3::Norm3(cross), so3::Dot3(u, q.data())));
        }
        EXPECT_LT(nearest, 2e-6);
      }
    }
    EXPECT_GT(event.value.entry.value, 0);
    EXPECT_NEAR(event.value.interfaces[slot].discriminant, 0, 1e-10);
    EXPECT_GT(std::abs(event.value.interfaces[1].discriminant), .001);
    EXPECT_EQ(event.source.source_token, row.source_token);
    EXPECT_LE(event.path_evaluations, 192u);
    EXPECT_EQ(event.accepted_poses.back(), event.source.pose);
    const auto low = CorrectInterfaceEvent(faces, row, { slot }, 5, deadline);
    EXPECT_EQ(low.status, InterfaceSolveStatus::kBudgetExceeded);
    EXPECT_EQ(low.path_evaluations, 0u);
    const auto expired = CorrectInterfaceEvent(faces, row, { slot }, 192, std::chrono::steady_clock::now());
    EXPECT_EQ(expired.status, InterfaceSolveStatus::kBudgetExceeded);
    EXPECT_EQ(expired.path_evaluations, 0u);
    const auto limited = CorrectInterfaceEvent(faces, row, { slot, 1e-10, .05, 1 }, 192, deadline);
    EXPECT_EQ(limited.status, InterfaceSolveStatus::kIterationLimit);
    EXPECT_EQ(limited.source.pose, row.pose);
    // The independently verified point need not be the nearest correction from
    // this seed: one equation on SO(3) has a two-dimensional family of roots.
    row.pose = item.at("event_pose").get<std::array<double, 9>>();
    std::vector<DiagnosticOutputRow> value;
    if (!EvaluateDiagnosticBatch(faces, { row }, {}, &value)) {
      ADD_FAILURE();
      return;
    }
    EXPECT_NEAR(value[0].interfaces[slot].discriminant, 0, 1e-10);
    EXPECT_NEAR(value[0].entry.value, item.at("independent_event_area").get<double>(), 1e-9);
  }
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
