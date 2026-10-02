// General diagnostic field: complete per-interface evidence and branch-aware derivatives.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "analytic/diagnostic_field.hpp"
#include "analytic/path_evaluation.hpp"
#include "analytic/so3.hpp"

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

LUMICE_ANALYTIC_Crystal ScaledAsymmetricPrism(double scale) {
  LUMICE_ANALYTIC_Crystal crystal = AsymmetricPrism();
  crystal.height *= scale;
  for (double& distance : crystal.face_distance) {
    distance *= scale;
  }
  return crystal;
}

struct Fixture {
  FaceNormalTable normals;
  FacePolygonTable polygons;
  std::vector<int> slots;
};

Fixture Build(const LUMICE_ANALYTIC_Crystal& crystal, const std::vector<int>& faces) {
  Fixture fixture;
  EXPECT_EQ(BuildFaceNormals(crystal, &fixture.normals, &fixture.polygons), Status::kOk);
  fixture.slots.resize(faces.size());
  EXPECT_EQ(ResolveFaceSequence(fixture.normals, faces.data(), static_cast<int>(faces.size()), fixture.slots.data()),
            Status::kOk);
  return fixture;
}

Fixture Build(const std::vector<int>& faces) {
  return Build(AsymmetricPrism(), faces);
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

void Perturb(const double pose[9], int axis, double amount, double out[9]) {
  double delta[3]{};
  delta[axis] = amount;
  double increment[9];
  so3::Exp(delta, increment);
  so3::MatMul(pose, increment, out);
}

void ExpectStablePoseSamples(DiagnosticField* field, const DiagnosticRowInput& input, double minimum_margin) {
  for (double step : { 4.0e-4, 2.0e-4 }) {
    for (int axis = 0; axis < 3; axis++) {
      for (double sign : { -1.0, 1.0 }) {
        DiagnosticRowInput sample = input;
        Perturb(input.pose, axis, sign * step, sample.pose);
        const DiagnosticFieldResult result = field->Evaluate(sample);
        EXPECT_EQ(result.path_status, DiagnosticPathStatus::kOk);
        for (const DiagnosticMargin& margin : result.domain_margins) {
          EXPECT_GT(margin.value, minimum_margin);
        }
      }
    }
  }
}

void ExpectStableIndexSamples(DiagnosticField* field, const DiagnosticRowInput& input, double minimum_margin) {
  const double fine_step = 1.0e-6 * std::max(1.0, std::fabs(input.refractive_index));
  for (double step : { 2.0 * fine_step, fine_step }) {
    for (double sign : { -1.0, 1.0 }) {
      DiagnosticRowInput sample = input;
      sample.refractive_index += sign * step;
      const DiagnosticFieldResult result = field->Evaluate(sample);
      EXPECT_EQ(result.path_status, DiagnosticPathStatus::kOk);
      for (const DiagnosticMargin& margin : result.domain_margins) {
        EXPECT_GT(margin.value, minimum_margin);
      }
    }
  }
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

TEST(DiagnosticField, DomainGuardSuppressesEveryInterfacePoseDerivativeBeforeTheGateCrosses) {
  const std::vector<int> faces = { 3, 5, 6, 7 };
  Fixture fixture = Build(faces);
  DiagnosticField field(fixture.normals, fixture.polygons, faces.data(), fixture.slots.data(),
                        static_cast<int>(faces.size()));
  const double pose[9] = { 0.1607738167827848, -0.5169504537544563, 0.8407817839369246,
                           0.18942835438805,   -0.8198654102527329, -0.5403125092249418,
                           0.9686426990335141, 0.24613601409419814, -0.03388781749965317 };
  const DiagnosticFieldResult result = field.Evaluate(Row(pose));

  ASSERT_EQ(result.path_status, DiagnosticPathStatus::kOk);
  ASSERT_EQ(result.domain_margins.size(), 6u);
  const auto closest =
      std::min_element(result.domain_margins.begin(), result.domain_margins.end(),
                       [](const DiagnosticMargin& a, const DiagnosticMargin& b) { return a.value < b.value; });
  EXPECT_GT(closest->value, 0.0);
  EXPECT_FALSE(result.direction_pose_jacobian_available);
  EXPECT_FALSE(result.direction_pose_hessian_available);
  for (const DiagnosticInterface& interface : result.interfaces) {
    EXPECT_FALSE(interface.pose_derivative_available);
  }
}

TEST(DiagnosticFieldNumerics, RichardsonEstimateReportsErrorAndRejectsNonConvergence) {
  const diagnostic_field_detail::CentralDifferenceEstimate stable =
      diagnostic_field_detail::RichardsonEstimate(1.0003, 1.000075, 1.0e-4, 0.0);
  EXPECT_TRUE(stable.converged);
  EXPECT_NEAR(stable.value, 1.0, 1.0e-15);
  EXPECT_NEAR(stable.error, 7.5e-5, 1.0e-15);

  const diagnostic_field_detail::CentralDifferenceEstimate unstable =
      diagnostic_field_detail::RichardsonEstimate(1.0, 2.0, 1.0e-6, 1.0e-6);
  EXPECT_FALSE(unstable.converged);
  EXPECT_GT(unstable.error, 0.3);
}

TEST(DiagnosticFieldNumerics, PublicDirectionDerivativeRejectsNonConvergenceOnAStableBranch) {
  const std::vector<int> faces = { 3, 5 };
  Fixture fixture = Build(faces);
  DiagnosticField field(fixture.normals, fixture.polygons, faces.data(), fixture.slots.data(),
                        static_cast<int>(faces.size()));
  const double pose[9] = { 0.965299175273876,    -0.24238479774432437, -0.09719625526746277,
                           0.21881440907585153,  0.547563436712245,    0.8076475327496896,
                           -0.14254036830442351, -0.8008894384359916,  0.5815998201558535 };
  DiagnosticRowInput input = Row(pose);
  input.refractive_index = 1.4650258857410854;
  const DiagnosticFieldResult result = field.Evaluate(input);

  ASSERT_EQ(result.path_status, DiagnosticPathStatus::kOk);
  ExpectStablePoseSamples(&field, input, 1.0e-2);
  EXPECT_FALSE(result.direction_pose_jacobian_available);
  EXPECT_TRUE(std::all_of(std::begin(result.direction_pose_jacobian), std::end(result.direction_pose_jacobian),
                          [](double value) { return value == 0.0; }));
  EXPECT_TRUE(result.direction_pose_hessian_available);
}

TEST(DiagnosticFieldNumerics, PublicScalarPoseDerivativeRejectsNonConvergenceOnAStableBranch) {
  const std::vector<int> faces = { 3, 5 };
  Fixture fixture = Build(faces);
  DiagnosticField field(fixture.normals, fixture.polygons, faces.data(), fixture.slots.data(),
                        static_cast<int>(faces.size()));
  const double pose[9] = { 0.8892503164034372,  -0.44956594229729063, 0.08440579543334724,
                           0.45083377646243805, 0.830191399730927,    -0.3279194196954804,
                           0.07734843745676649, 0.32965543122956936,  0.9409274764209212 };
  DiagnosticRowInput input = Row(pose);
  input.refractive_index = 1.3616948255014758;
  const DiagnosticFieldResult result = field.Evaluate(input);

  ASSERT_EQ(result.path_status, DiagnosticPathStatus::kOk);
  ASSERT_EQ(result.interfaces.size(), 2u);
  ExpectStablePoseSamples(&field, input, 1.0e-2);
  EXPECT_TRUE(result.direction_pose_jacobian_available);
  EXPECT_FALSE(result.interfaces[1].pose_derivative_available);
  EXPECT_TRUE(std::all_of(std::begin(result.interfaces[1].pose_gradient), std::end(result.interfaces[1].pose_gradient),
                          [](double value) { return value == 0.0; }));
}

TEST(DiagnosticFieldNumerics, PublicScalarIndexDerivativeRejectsNonConvergenceOnAStableBranch) {
  const std::vector<int> faces = { 3, 5 };
  Fixture fixture = Build(faces);
  DiagnosticField field(fixture.normals, fixture.polygons, faces.data(), fixture.slots.data(),
                        static_cast<int>(faces.size()));
  const double pose[9] = { 0.8642011404007943,   0.1200439016309499, 0.4886162610998384,
                           0.1922639758599871,   0.8186355047860818, -0.5411750861691135,
                           -0.46496338836136003, 0.5616274316527424, 0.6843856190034001 };
  DiagnosticRowInput input = Row(pose);
  input.refractive_index = 1.0000026742626695;
  const DiagnosticFieldResult result = field.Evaluate(input);

  ASSERT_EQ(result.path_status, DiagnosticPathStatus::kOk);
  ASSERT_EQ(result.interfaces.size(), 2u);
  ExpectStableIndexSamples(&field, input, 1.0e-2);
  EXPECT_TRUE(result.direction_index_derivative_available);
  EXPECT_FALSE(result.interfaces[1].index_derivative_available);
  EXPECT_DOUBLE_EQ(result.interfaces[1].index_derivative, 0.0);
}

TEST(DiagnosticFieldNumerics, EntryDerivativeGateIsInvariantUnderCommonLengthUnitChanges) {
  const std::vector<int> faces = { 3, 5 };
  const double pose[9] = { 0.7204904865724651,  -0.6914459954202334,  0.05287621559732775,
                           0.6340874179135871,  0.6877519257467668,   0.35345499724193113,
                           -0.2807607615074663, -0.22113281992715655, 0.9339559254851437 };
  DiagnosticRowInput input = Row(pose);
  input.refractive_index = 1.2427044850879065;
  auto evaluate = [&](double scale) {
    Fixture fixture = Build(ScaledAsymmetricPrism(scale), faces);
    DiagnosticField field(fixture.normals, fixture.polygons, faces.data(), fixture.slots.data(),
                          static_cast<int>(faces.size()));
    return field.Evaluate(input);
  };

  const DiagnosticFieldResult reference = evaluate(1.0);
  ASSERT_EQ(reference.entry_status, DiagnosticEntryStatus::kOk);
  ASSERT_TRUE(reference.entry_pose_gradient_available);
  ASSERT_TRUE(reference.entry_index_derivative_available);
  for (double scale : { 1.0e-3, 1.0e3 }) {
    const DiagnosticFieldResult scaled = evaluate(scale);
    const double area_scale = scale * scale;
    EXPECT_EQ(scaled.entry_status, DiagnosticEntryStatus::kOk);
    EXPECT_EQ(scaled.entry_pose_gradient_available, reference.entry_pose_gradient_available);
    EXPECT_EQ(scaled.entry_index_derivative_available, reference.entry_index_derivative_available);
    EXPECT_NEAR(scaled.entry_measure / area_scale, reference.entry_measure, 2.0e-10);
    for (int axis = 0; axis < 3; axis++) {
      EXPECT_NEAR(scaled.entry_pose_gradient[axis] / area_scale, reference.entry_pose_gradient[axis], 2.0e-8);
    }
    EXPECT_NEAR(scaled.entry_index_derivative / area_scale, reference.entry_index_derivative, 2.0e-8);
  }
}

TEST(DiagnosticField, CorridorTopologyChangeSuppressesEntryDerivatives) {
  const std::vector<int> faces = { 3, 5, 6, 7 };
  Fixture fixture = Build(faces);
  DiagnosticField field(fixture.normals, fixture.polygons, faces.data(), fixture.slots.data(),
                        static_cast<int>(faces.size()));
  const double pose[9] = { 0.9290709982052409,  -0.36839005431024047, 0.033404313781536035,
                           0.35559103306313394, 0.91435424672714,     0.19367841567179747,
                           -0.1018925782332082, -0.168062724532665,   0.9804962127023475 };
  const DiagnosticRowInput input = Row(pose);
  const DiagnosticFieldResult result = field.Evaluate(input);

  Corridor corridor(fixture.normals, fixture.polygons, fixture.slots.data(), static_cast<int>(faces.size()));
  auto entry_at = [&](const DiagnosticRowInput& row) {
    double incident_body[3];
    for (int i = 0; i < 3; i++) {
      incident_body[i] = row.pose[i] * row.incident_direction[0] + row.pose[3 + i] * row.incident_direction[1] +
                         row.pose[6 + i] * row.incident_direction[2];
    }
    return corridor.Evaluate(incident_body, row.refractive_index);
  };
  const EntryMeasure base_entry = entry_at(input);
  bool topology_changed = false;
  for (double step : { 4.0e-4, 2.0e-4 }) {
    for (int axis = 0; axis < 3; axis++) {
      for (double sign : { -1.0, 1.0 }) {
        DiagnosticRowInput sample = input;
        Perturb(input.pose, axis, sign * step, sample.pose);
        const EntryMeasure entry = entry_at(sample);
        topology_changed = topology_changed || entry.topology_signature != base_entry.topology_signature;
      }
    }
  }

  ASSERT_EQ(result.path_status, DiagnosticPathStatus::kOk);
  ASSERT_EQ(result.entry_status, DiagnosticEntryStatus::kOk);
  EXPECT_TRUE(result.direction_pose_jacobian_available);
  EXPECT_TRUE(topology_changed);
  EXPECT_FALSE(result.entry_pose_gradient_available);
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

TEST(DiagnosticField, InvalidRepeatedFacePathKeepsIndependentEntryDerivatives) {
  // Repeated faces are legal concrete input.  The optical chain cannot reach the same face again,
  // while the independently projected finite corridor remains non-empty and smooth.
  const std::vector<int> faces = { 3, 7, 7, 7 };
  Fixture fixture = Build(faces);
  DiagnosticField field(fixture.normals, fixture.polygons, faces.data(), fixture.slots.data(),
                        static_cast<int>(faces.size()));
  const double pose[9] = { 0.9528507753336312,   -0.028754374054210774, 0.3020738087270204,
                           0.2441192873885814,   0.663911192480992,     -0.7068434777398448,
                           -0.18022534081252045, 0.7472583987291262,    0.6396277918116074 };
  DiagnosticRowInput input = Row(pose);
  input.refractive_index = 1.0966873551331053;
  const DiagnosticFieldResult result = field.Evaluate(input);

  EXPECT_EQ(result.path_status, DiagnosticPathStatus::kPathInfeasible);
  EXPECT_EQ(result.entry_status, DiagnosticEntryStatus::kOk);
  EXPECT_GT(result.entry_measure, 0.07);
  EXPECT_TRUE(result.entry_pose_gradient_available);
  EXPECT_TRUE(result.entry_index_derivative_available);
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
